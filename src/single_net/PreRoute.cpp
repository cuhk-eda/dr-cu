#include "PreRoute.h"

db::RouteStatus PreRoute::run(int numPitchForGuideExpand) {
    // expand
    for (auto& guide : localNet.routeGuides) {
        database.expandBox(guide, numPitchForGuideExpand);
    }

    // expand box by cross layer connection
    // expandBoxToMargin(localNet.routeGuides);

    db::RouteStatus status = db::RouteStatus::SUCC_NORMAL;
    if (localNet.numOfPins() < 2) {
        status = db::RouteStatus::SUCC_ONE_PIN;
    } else {
        status = expandGuidesToCoverPins();
        if (isSucc(status)) {
            localNet.initGridBoxes();
            localNet.initConn(localNet.gridPinAccessBoxes, localNet.gridRouteGuides);
            localNet.initNumOfVertices();

            if (!localNet.checkPin()) {
                status = db::RouteStatus::FAIL_PIN_OUT_OF_GRID;
            } else if (!localNet.checkPinGuideConn()) {
                status = db::RouteStatus::FAIL_DETACHED_PIN;
            } else if (!checkGuideConnTrack()) {
                status = db::RouteStatus::FAIL_DETACHED_GUIDE;
            }
        }
    }

    printWarnMsg(status, localNet.dbNet);
    return status;
}

db::RouteStatus PreRoute::runIterative() {
    db::RouteStatus status = run(db::setting.defaultGuideExpand);

    int iter = 0;
    int numPitchForGuideExpand = db::setting.defaultGuideExpand;
    utils::timer singleNetTimer;
    while (status == +db::RouteStatus::FAIL_DETACHED_GUIDE && iter < db::setting.guideExpandIterLimit) {
        iter++;
        numPitchForGuideExpand += iter;
        // numPitchForGuideExpand += 2;

        status = run(numPitchForGuideExpand);
    }

    if (iter >= 1) {
        log() << "Warning: Net " << localNet.getName() << " expands " << iter << " iterations"
              << ", which takes " << singleNetTimer.elapsed() << " s in total." << std::endl;
        if (status == +db::RouteStatus::FAIL_DETACHED_GUIDE) {
            log() << "Error: Exceed the guideExpandIterLimit, but still FAIL_DETACHED_GUIDE" << std::endl;
        }
    }

    db::routeStat.increment(db::RouteStage::PRE, status);
    return status;
}

void PreRoute::expandBoxToMargin(vector<db::BoxOnLayer>& routeGuides) {
    vector<vector<int>> crossLayerConn(routeGuides.size());
    for (unsigned g1 = 0; g1 < routeGuides.size(); g1++) {
        db::BoxOnLayer& box1 = routeGuides[g1];
        for (unsigned g2 = g1 + 1; g2 < routeGuides.size(); g2++) {
            db::BoxOnLayer& box2 = routeGuides[g2];
            if (abs(box1.layerIdx - box2.layerIdx) == 1 && box1.HasIntersectWith(box2)) {
                crossLayerConn[g2].push_back(g1);
                crossLayerConn[g1].push_back(g2);
            }
        }
    }

    for (unsigned g1 = 0; g1 < routeGuides.size(); g1++) {
        for (auto g2 : crossLayerConn[g1]) {
            if (g2 < g1) continue;

            Dimension dir1 = database.getLayerDir(routeGuides[g1].layerIdx);
            Dimension dir2 = database.getLayerDir(routeGuides[g2].layerIdx);

            routeGuides[g1][dir2].Update(routeGuides[g2][dir2].low);
            routeGuides[g1][dir2].Update(routeGuides[g2][dir2].high);
            routeGuides[g2][dir1].Update(routeGuides[g1][dir1].low);
            routeGuides[g2][dir1].Update(routeGuides[g1][dir1].high);
        }
    }
}

db::RouteStatus PreRoute::expandGuidesToCoverPins() {
    db::RouteStatus status = db::RouteStatus::SUCC_NORMAL;
    for (int i = 0; i < localNet.numOfPins(); ++i) {
        int bestAB = -1;
        int bestGuide = -1;
        DBU bestDist = std::numeric_limits<DBU>::max();
        for (int j = 0; j < localNet.pinAccessBoxes[i].size(); ++j) {
            const auto& accessBox = localNet.pinAccessBoxes[i][j];
            for (int k = 0; k < localNet.routeGuides.size(); ++k) {
                const auto& guide = localNet.routeGuides[k];
                if (abs(guide.layerIdx - accessBox.layerIdx) > 1 || !guide.IsValid()) continue;
                DBU dist = Dist(accessBox, guide);
                if (dist < bestDist) {
                    bestDist = dist;
                    bestAB = j;
                    bestGuide = k;
                    if (bestDist == 0) {
                        break;
                    }
                }
            }
        }
        if (bestDist > 0) {
            if (bestAB == -1) {
                return db::RouteStatus::FAIL_DETACHED_PIN;
            } else {
                localNet.routeGuides[bestGuide] = {
                    localNet.routeGuides[bestGuide].layerIdx,
                    localNet.routeGuides[bestGuide].UnionWith(localNet.pinAccessBoxes[i][bestAB])};
                status = db::RouteStatus::SUCC_DETACHED_PIN_FIXED;
            }
        }
    }
    return status;
}

bool PreRoute::checkGuideConnTrack() const {
    // init to all false
    vector<char> pinVisited(localNet.pinGuideConn.size(), false);
    vector<vector<char>> guideVisited(localNet.guideConn.size());
    for (int i = 0; i < localNet.guideConn.size(); i++) {
        guideVisited[i].resize(localNet.gridRouteGuides[i].trackRange.range() + 1, false);
    }

    // define recursive std::functions VisitPin & VisitGuide
    std::function<void(int)> VisitPin;
    std::function<void(int, utils::IntervalT<int>)> VisitGuide;
    VisitPin = [&](int pinIdx) {
        pinVisited[pinIdx] = true;
        for (const auto& ga : localNet.pinGuideConn[pinIdx]) {
            int guideIdx = ga.first;
            utils::IntervalT<int> trackInterval = localNet.gridRouteGuides[ga.first].trackRange.IntersectWith(
                localNet.gridPinAccessBoxes[pinIdx][ga.second].trackRange);
            bool isVisited = true;
            int trackLowIdx = localNet.gridRouteGuides[ga.first].trackRange.low;
            for (int t = trackInterval.low - trackLowIdx; t <= trackInterval.high - trackLowIdx; t++) {
                if (guideVisited[ga.first][t] == false) {
                    isVisited = false;
                    guideVisited[ga.first][t] = true;
                }
            }
            if (!isVisited) {
                VisitGuide(guideIdx, trackInterval);
            }
        }
    };
    VisitGuide = [&](int guideIdx, utils::IntervalT<int> trackRange) {
        for (int adjGuideIdx : localNet.guideConn[guideIdx]) {
            // to upper layer
            if (localNet.gridRouteGuides[guideIdx].layerIdx < localNet.gridRouteGuides[adjGuideIdx].layerIdx) {
                utils::IntervalT<int> upperCpInterval =
                    database.getLayer(localNet.gridRouteGuides[guideIdx].layerIdx).getUpperCrossPointRange(trackRange);
                if (localNet.gridRouteGuides[adjGuideIdx].crossPointRange.IntersectWith(upperCpInterval).IsValid()) {
                    db::ViaBox viaBox = database.getViaBoxBetween(localNet.gridRouteGuides[guideIdx],
                                                                  localNet.gridRouteGuides[adjGuideIdx]);
                    bool isVisited = true;
                    int trackLowIdx = localNet.gridRouteGuides[adjGuideIdx].trackRange.low;
                    for (int t = viaBox.upper.trackRange.low - trackLowIdx;
                         t <= viaBox.upper.trackRange.high - trackLowIdx;
                         t++) {
                        if (guideVisited[adjGuideIdx][t] == false) {
                            isVisited = false;
                            guideVisited[adjGuideIdx][t] = true;
                        }
                    }
                    if (!isVisited) {
                        VisitGuide(adjGuideIdx, viaBox.upper.trackRange);
                    }
                }
            }
            if (localNet.gridRouteGuides[guideIdx].layerIdx > localNet.gridRouteGuides[adjGuideIdx].layerIdx) {
                utils::IntervalT<int> lowerCpInterval =
                    database.getLayer(localNet.gridRouteGuides[guideIdx].layerIdx).getLowerCrossPointRange(trackRange);
                if (localNet.gridRouteGuides[adjGuideIdx].crossPointRange.IntersectWith(lowerCpInterval).IsValid()) {
                    db::ViaBox viaBox = database.getViaBoxBetween(localNet.gridRouteGuides[adjGuideIdx],
                                                                  localNet.gridRouteGuides[guideIdx]);
                    bool isVisited = true;
                    int trackLowIdx = localNet.gridRouteGuides[adjGuideIdx].trackRange.low;
                    for (int t = viaBox.lower.trackRange.low - trackLowIdx;
                         t <= viaBox.lower.trackRange.high - trackLowIdx;
                         t++) {
                        if (guideVisited[adjGuideIdx][t] == false) {
                            isVisited = false;
                            guideVisited[adjGuideIdx][t] = true;
                        }
                    }
                    if (!isVisited) {
                        VisitGuide(adjGuideIdx, viaBox.lower.trackRange);
                    }
                }
            }
        }
        for (const auto& pa : localNet.guidePinConn[guideIdx]) {
            utils::IntervalT<int> trackInterval =
                trackRange.IntersectWith(localNet.gridPinAccessBoxes[pa.first][pa.second].trackRange);
            if (trackInterval.IsValid() && !pinVisited[pa.first]) {
                VisitPin(pa.first);
            }
        }
    };

    // DFS from pin 0
    VisitPin(0);

    return all_of(pinVisited.begin(), pinVisited.end(), [](bool visited) { return visited; });
}
