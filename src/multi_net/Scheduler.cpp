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
            initSet({});
            vector<int> &batch = batches.back();
            for (int i = lastUnroute; i < routerIds.size(); ++i) {
                int routerId = routerIds[i];
                if (!assigned[routerId] && !hasConflict(routerId)) {
                    batch.push_back(routerId);
                    assigned[routerId] = true;
                    updateSet(routerId);
                }
            }
            // find the next seed
            while (lastUnroute < routerIds.size() && assigned[routerIds[lastUnroute]]) {
                ++lastUnroute;
            }
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

void Scheduler::initSet(vector<int> jobIdxes) {
    rtrees = RTrees(database.getLayerNum());
    for (int jobIdx : jobIdxes) {
        updateSet(jobIdx);
    }
}

void Scheduler::updateSet(int jobIdx) {
    for (const auto &routeGuide : routers[jobIdx].localNet.routeGuides) {
        DBU safeMargin = database.getLayer(routeGuide.layerIdx).mtSafeMargin / 2;
        boostBox box(boostPoint(routeGuide.x.low - safeMargin, routeGuide.y.low - safeMargin),
                     boostPoint(routeGuide.x.high + safeMargin, routeGuide.y.high + safeMargin));
        rtrees[routeGuide.layerIdx].insert({box, jobIdx});
    }
}

bool Scheduler::hasConflict(int jobIdx) {
    for (const auto &routeGuide : routers[jobIdx].localNet.routeGuides) {
        DBU safeMargin = database.getLayer(routeGuide.layerIdx).mtSafeMargin / 2;
        boostBox box(boostPoint(routeGuide.x.low - safeMargin, routeGuide.y.low - safeMargin),
                     boostPoint(routeGuide.x.high + safeMargin, routeGuide.y.high + safeMargin));

        std::vector<std::pair<boostBox, int>> results;
        rtrees[routeGuide.layerIdx].query(bgi::intersects(box), std::back_inserter(results));

        for (const auto &result : results) {
            if (result.second != jobIdx) {
                return true;
            }
        }
    }
    return false;
}

vector<vector<int>> &PostScheduler::schedule() {
    // init assigned table
    vector<bool> assigned(dbNets.size(), false);
    if (db::setting.numThreads == 0) {
        // simple case
        for (int i = 0; i < dbNets.size(); ++i) {
            batches.push_back({i});
        }
    } else {
        // normal case
        int lastUnroute = 0;
        while (lastUnroute < dbNets.size()) {
            // create a new batch from a seed
            batches.emplace_back();
            initSet({});
            vector<int> &batch = batches.back();
            for (int i = lastUnroute; i < dbNets.size(); ++i) {
                if (!assigned[i] && !hasConflict(i)) {
                    batch.push_back(i);
                    assigned[i] = true;
                    updateSet(i);
                }
            }
            // find the next seed
            while (lastUnroute < dbNets.size() && assigned[lastUnroute]) {
                ++lastUnroute;
            }
        }
    }
    return batches;
}

void PostScheduler::initSet(vector<int> jobIdxes) {
    rtrees = RTrees(database.getLayerNum());
    for (int jobIdx : jobIdxes) {
        updateSet(jobIdx);
    }
}

void PostScheduler::updateSet(int jobIdx) {
    auto &dbNet = dbNets[jobIdx];
    vector<std::pair<boostBox, int>> boostBoxes = getNetBoxes(dbNet);
    for (auto &box : boostBoxes) {
        rtrees[box.second].insert({box.first, jobIdx});
    }
}

// TODO: terminate the postOrderVisitGridTopo when hasCon == true
bool PostScheduler::hasConflict(int jobIdx) {
    auto &dbNet = dbNets[jobIdx];
    bool hasCon = false;
    std::vector<std::pair<boostBox, int>> results;
    vector<std::pair<boostBox, int>> boostBoxes = getNetBoxes(dbNet);
    for (auto &box : boostBoxes) {
        rtrees[box.second].query(bgi::intersects(box.first), std::back_inserter(results));
    }
    for (const auto &result : results) {
        if (result.second != jobIdx) {
            hasCon = true;
            break;
        }
    }
    return hasCon;
}

boostBox PostScheduler::getBoostBox(const db::GridPoint &gp) {
    DBU safeMargin = database.getLayer(gp.layerIdx).mtSafeMargin / 2;
    const auto gpLoc = database.getLoc(gp);
    return {boostPoint(gpLoc.x - safeMargin, gpLoc.y - safeMargin),
            boostPoint(gpLoc.x + safeMargin, gpLoc.y + safeMargin)};
}

boostBox PostScheduler::getBoostBox(const db::GridEdge &edge) {
    DBU safeMargin = database.getLayer(edge.u.layerIdx).mtSafeMargin / 2;
    const auto edgeLoc = database.getLoc(edge);
    return {boostPoint(edgeLoc.first.x - safeMargin, edgeLoc.first.y - safeMargin),
            boostPoint(edgeLoc.second.x + safeMargin, edgeLoc.second.y + safeMargin)};
}

vector<std::pair<boostBox, int>> PostScheduler::getNetBoxes(const db::Net &dbNet) {
    vector<std::pair<boostBox, int>> boostBoxes;
    dbNet.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        if (node->parent) {
            db::GridEdge edge(*node, *(node->parent));
            if (edge.isVia()) {
                boostBoxes.emplace_back(getBoostBox(edge.u), edge.u.layerIdx);
                boostBoxes.emplace_back(getBoostBox(edge.v), edge.v.layerIdx);
            } else if (edge.isTrackSegment() || edge.isWrongWaySegment()) {
                boostBoxes.emplace_back(getBoostBox(edge), node->layerIdx);
            } else {
                log() << "Warning in " << __func__ << ": invalid edge type. skip." << std::endl;
            }
        }
        if (node->extWireSeg) {
            boostBoxes.emplace_back(getBoostBox(*(node->extWireSeg)), node->layerIdx);
        }
    });
    return boostBoxes;
}