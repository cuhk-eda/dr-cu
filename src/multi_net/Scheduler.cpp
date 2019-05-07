#include "Scheduler.h"

vector<vector<int>> &Scheduler::schedule() {
    // init assigned table
    vector<bool> assigned(routers.size(), false);
    for (int i = 0; i < routers.size(); i++) {
        if (!db::isSucc(routers[i].status) || routers[i].status == +db::RouteStatus::SUCC_ONE_PIN) {
            assigned[i] = true;
        }
    }

    // sort by sizes
    vector<int> routerIds;
    for (int id = 0; id < routers.size(); ++id) {
        routerIds.push_back(id);
    }
    if (db::setting.multiNetScheduleSortAll) {
        std::sort(routerIds.begin(), routerIds.end(), [&](int lhs, int rhs) {
            return routers[lhs].localNet.estimatedNumOfVertices > routers[rhs].localNet.estimatedNumOfVertices;
        });
    }

    if (db::setting.numThreads == 0) {
        // simple case
        for (int routerId : routerIds) {
            if (!assigned[routerId]) {
                batches.push_back({routerId});
            }
        }
    } else {
        // normal case
        int lastUnroute = 0;
        while (lastUnroute < routerIds.size()) {
            // create a new batch from a seed
            batches.emplace_back();
            conflictDetect.initSet({});
            vector<int> &batch = batches.back();
            for (int i = lastUnroute; i < routerIds.size(); ++i) {
                int routerId = routerIds[i];
                if (!assigned[routerId] && !conflictDetect.hasConflict(routerId)) {
                    batch.push_back(routerId);
                    assigned[routerId] = true;
                    conflictDetect.updateSet(routerId);
                }
            }
            // find the next seed
            while (lastUnroute < routerIds.size() && assigned[routerIds[lastUnroute]]) {
                ++lastUnroute;
            }
        }

        // fill later batches by jobs in the previous batches
        if (db::setting.multiNetScheduleAssignBackRatio) {
            assignBackward();
        }

        // sort within batches by NumOfVertices
        if (db::setting.multiNetScheduleSort) {
            for (auto &batch : batches) {
                std::sort(batch.begin(), batch.end(), [&](int lhs, int rhs) {
                    return routers[lhs].localNet.estimatedNumOfVertices > routers[rhs].localNet.estimatedNumOfVertices;
                });
            }
        }
    }

    if (db::setting.multiNetScheduleReverse) {
        reverse(batches.begin(), batches.end());
    }

    return batches;
}

void Scheduler::assignBackward() {
    const unsigned numRouters = routers.size();
    const int minBatchSize = min(int(numRouters / batches.size()), defaultMinBatchSize);
    if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << "Start assigning back..." << std::endl;
        log() << "minBatchSize = " << minBatchSize << std::endl;
    }

    const unsigned numIdx = (numRouters + 1) * db::setting.multiNetScheduleAssignBackRatio;
    if (!numIdx) {
        return;
    }

    vector<int> estimatedNumsOfVertices;
    int maxNumOfVertices = INT_MAX;
    if (numIdx < numRouters) {
        estimatedNumsOfVertices.reserve(numRouters);
        for (const SingleNetRouter &router : routers) {
            estimatedNumsOfVertices.push_back(router.localNet.estimatedNumOfVertices);
        }
        sort(estimatedNumsOfVertices.begin(), estimatedNumsOfVertices.end());
        maxNumOfVertices = estimatedNumsOfVertices[numIdx - 1];
    }

    for (int iDst = (int)batches.size() - 1; iDst >= 0; --iDst) {
        auto &dstBatch = batches[iDst];
        conflictDetect.initSet(dstBatch);
        for (int iSrc = 0; iSrc < iDst && dstBatch.size() < minBatchSize; ++iSrc) {
            auto &srcBatch = batches[iSrc];
            for (int i = 0; i < srcBatch.size() && srcBatch.size() > minBatchSize && dstBatch.size() < minBatchSize;) {
                if (routers[srcBatch[i]].localNet.estimatedNumOfVertices <= maxNumOfVertices &&
                    !conflictDetect.hasConflict(srcBatch[i])) {
                    dstBatch.push_back(srcBatch[i]);
                    conflictDetect.updateSet(srcBatch[i]);
                    srcBatch[i] = srcBatch.back();
                    srcBatch.pop_back();
                } else {
                    ++i;
                }
            }
        }
    }
}

void ConflictDetectionFastConstruct::initSet(vector<int> jobIdxes) {
    rtrees = vector<bgi::rtree<std::pair<box, int>, bgi::quadratic<16>>>(database.getLayerNum());
    for (int jobIdx : jobIdxes) {
        updateSet(jobIdx);
    }
}

void ConflictDetectionFastConstruct::updateSet(int jobIdx) {
    for (const auto &routeGuide : routers[jobIdx].localNet.routeGuides) {
        DBU safeMargin = database.getLayer(routeGuide.layerIdx).safeMargin / 2;
        box query_box(point(routeGuide.x.low - safeMargin, routeGuide.y.low - safeMargin),
                      point(routeGuide.x.high + safeMargin, routeGuide.y.high + safeMargin));
        rtrees[routeGuide.layerIdx].insert({query_box, jobIdx});
    }
}

bool ConflictDetectionFastConstruct::hasConflict(int jobIdx) {
    for (const auto &routeGuide : routers[jobIdx].localNet.routeGuides) {
        DBU safeMargin = database.getLayer(routeGuide.layerIdx).safeMargin / 2;
        box query_box(point(routeGuide.x.low - safeMargin, routeGuide.y.low - safeMargin),
                      point(routeGuide.x.high + safeMargin, routeGuide.y.high + safeMargin));

        std::vector<std::pair<box, int>> results;
        rtrees[routeGuide.layerIdx].query(bgi::intersects(query_box), std::back_inserter(results));

        for (const auto &result : results) {
            if (result.second != jobIdx) {
                return true;
            }
        }
    }
    return false;
}

ConflictDetectionFastQuery::ConflictDetectionFastQuery(const vector<SingleNetRouter> &routersToExec)
    : ConflictDetectionBase(routersToExec) {
    int numEdges = 0;
    conflicts.resize(routers.size());
    vector<bgi::rtree<std::pair<box, int>, bgi::quadratic<16>>> rtrees(database.getLayerNum());
    for (int i = 0; i < routers.size(); ++i) {
        for (const auto &routeGuide : routers[i].localNet.routeGuides) {
            DBU safeMargin = database.getLayer(routeGuide.layerIdx).safeMargin / 2;
            box query_box(point(routeGuide.x.low - safeMargin, routeGuide.y.low - safeMargin),
                          point(routeGuide.x.high + safeMargin, routeGuide.y.high + safeMargin));

            std::vector<std::pair<box, int>> results;
            rtrees[routeGuide.layerIdx].query(bgi::intersects(query_box), std::back_inserter(results));

            for (const auto &result : results) {
                int j = result.second;
                if (j != i) {
                    conflicts[i].push_back(j);
                    conflicts[j].push_back(i);
                }
            }

            rtrees[routeGuide.layerIdx].insert({query_box, i});
        }
    }

    // unique
    for (auto &conflict : conflicts) {
        conflict.erase(unique(conflict.begin(), conflict.end()), conflict.end());
        numEdges += conflict.size();
    }
    numEdges /= 2;

    log() << "numEdges=" << numEdges << std::endl;
}

void ConflictDetectionFastQuery::initSet(vector<int> jobIdxes) {
    curConflicts.clear();
    for (int jobIdx : jobIdxes) {
        updateSet(jobIdx);
    }
}

void ConflictDetectionFastQuery::updateSet(int jobIdx) {
    for (int conflict : conflicts[jobIdx]) {
        curConflicts.insert(conflict);
    }
}

bool ConflictDetectionFastQuery::hasConflict(int jobIdx) { return curConflicts.find(jobIdx) != curConflicts.end(); }
