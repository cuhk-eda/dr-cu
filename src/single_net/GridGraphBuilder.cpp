#include "GridGraphBuilder.h"
#include "db/Database.h"

void GridGraphBuilder::run() {
    // 1. Give each grid point an index
    for (auto &gridBox : localNet.gridRouteGuides) {
        int begin = intervals.empty() ? 0 : intervals.back().second;
        int end = begin + (gridBox.trackRange.range() + 1) * (gridBox.crossPointRange.range() + 1);

        intervals.emplace_back(begin, end);
    }
    vertexToGridPoint.reserve(intervals.back().second);
    for (auto &gridBox : localNet.gridRouteGuides) {
        for (int t = gridBox.trackRange.low; t <= gridBox.trackRange.high; t++) {
            for (int c = gridBox.crossPointRange.low; c <= gridBox.crossPointRange.high; c++) {
                vertexToGridPoint.emplace_back(gridBox.layerIdx, t, c);
            }
        }
    }

    // 2. Add guide-pin connection
    pinToOriVertex.resize(localNet.numOfPins());
    for (unsigned p = 0; p < localNet.numOfPins(); p++) {
        for (auto ga : localNet.pinGuideConn[p]) {
            const auto &accessBoxes = localNet.gridPinAccessBoxes[p];
            int g = ga.second;  // guideIdx
            const auto &accessBox = accessBoxes[g];
            const auto &guideBox = localNet.gridRouteGuides[ga.first];

            if (accessBox.layerIdx != guideBox.layerIdx) continue;

            utils::IntervalT<int> cpInterval = accessBox.crossPointRange.IntersectWith(guideBox.crossPointRange);
            utils::IntervalT<int> trackInterval = accessBox.trackRange.IntersectWith(guideBox.trackRange);
            bool fakePin = (accessBoxes.size() > 1 && (g + 1) == accessBoxes.size() && accessBox.layerIdx != accessBoxes[g - 1].layerIdx);

            for (int t = trackInterval.low; t <= trackInterval.high; t++) {
                for (int c = cpInterval.low; c <= cpInterval.high; c++) {
                    int u = guideToVertex(ga.first, t, c);
                    updatePinVertex(p, u, fakePin);
                }
            }
        }
    }

    graph.init(intervals.back().second);

    // 3. Add inter-guide connection
    for (unsigned b1 = 0; b1 < localNet.gridRouteGuides.size(); b1++) {
        for (unsigned b2 : localNet.guideConn[b1])
            if (b1 < b2) connectTwoGuides(b1, b2);
    }

    // 4. Add wrong way connection
    addWrongWayConn();

    // 5. Add intra-guide connection
    for (unsigned b = 0; b < localNet.gridRouteGuides.size(); b++) connectGuide(b);

    setMinAreaFlags();
    addOutofPinPenalty();
    fixDisconnectedPin();
}

void GridGraphBuilder::addWrongWayConn() {
    // Note: assume all wrong way edge has same weight
    for (unsigned b = 0; b < localNet.gridRouteGuides.size(); b++) addRegWrongWayConn(b);
    addPinWrongWayConn();
    addAdjGuideWrongWayConn();
}

void GridGraphBuilder::addAdjGuideWrongWayConn() {
    for (unsigned b1 = 0; b1 < localNet.gridRouteGuides.size(); b1++) {
        for (unsigned b2 : localNet.guideAdj[b1]) {
            if (b1 < b2) {
                const db::GridBoxOnLayer &box1 = localNet.gridRouteGuides[b1];
                const db::GridBoxOnLayer &box2 = localNet.gridRouteGuides[b2];

                if (!database.isValid(box1) || !database.isValid(box2)) continue;

                int loTrackIdx, loGuideIdx, hiGuideIdx;
                if (box1.trackRange.low > box2.trackRange.high) {
                    loTrackIdx = box2.trackRange.high;
                    loGuideIdx = b2;
                    hiGuideIdx = b1;
                } else {
                    loTrackIdx = box1.trackRange.high;
                    loGuideIdx = b1;
                    hiGuideIdx = b2;
                }

                auto cpRange = box1.crossPointRange.IntersectWith(box2.crossPointRange);

                int numWrongWayPoint = (cpRange.range() + 1) * db::rrrIterSetting.wrongWayPointDensity;

                DBU pitch = database.getLayer(box1.layerIdx).pitch;
                db::CostT wrongWayCost = db::setting.wrongWayPenaltyCoeff * pitch;

                auto setEdgeCost = [&](int crossPointIdx) {
                    int u = guideToVertex(loGuideIdx, loTrackIdx, crossPointIdx);
                    int v = guideToVertex(hiGuideIdx, loTrackIdx + 1, crossPointIdx);
                    graph.addEdge(u, v, RIGHT, wrongWayCost);
                };

                if (numWrongWayPoint == 1) {
                    int crossPointIdx = (cpRange.high + cpRange.low) / 2;
                    setEdgeCost(crossPointIdx);
                } else if (numWrongWayPoint > 1) {
                    int wrongWayPointDist = (cpRange.range() + 1 - numWrongWayPoint) / (numWrongWayPoint - 1);
                    for (int i = 0; i < numWrongWayPoint; i++) {
                        int crossPointIdx = i * (1 + wrongWayPointDist) + cpRange.low;
                        setEdgeCost(crossPointIdx);
                    }
                }
            }
        }
    }
}

void GridGraphBuilder::addPinWrongWayConn() {
    const int extraRange = 2;
    for (unsigned p = 0; p < localNet.numOfPins(); p++) {
        for (auto ga : localNet.pinGuideConn[p]) {
            db::GridBoxOnLayer accessBox = localNet.gridPinAccessBoxes[p][ga.second];
            const db::GridBoxOnLayer &guideBox = localNet.gridRouteGuides[ga.first];

            if ((accessBox.layerIdx + 1) == guideBox.layerIdx) {
                accessBox = database.getUpper(accessBox);
            }
            else if ((accessBox.layerIdx - 1) == guideBox.layerIdx) {
                accessBox = database.getLower(accessBox);
            }
            else if (accessBox.layerIdx != guideBox.layerIdx) {
                continue;
            }

            accessBox.trackRange.low -= extraRange;
            accessBox.trackRange.high += extraRange;
            accessBox.crossPointRange.low -= extraRange;
            accessBox.crossPointRange.high += extraRange;

            utils::IntervalT<int> cpInterval = accessBox.crossPointRange.IntersectWith(guideBox.crossPointRange);
            utils::IntervalT<int> trackInterval = accessBox.trackRange.IntersectWith(guideBox.trackRange);

            DBU pitch = database.getLayer(guideBox.layerIdx).pitch;
            db::CostT wrongWayCost = db::setting.wrongWayPenaltyCoeff * pitch;

            for (int c = cpInterval.low; c <= cpInterval.high; c++) {
                for (int t = trackInterval.low; t < trackInterval.high; t++) {
                    int u = guideToVertex(ga.first, t, c);
                    int v = guideToVertex(ga.first, t + 1, c);
                    graph.addEdge(u, v, RIGHT, wrongWayCost);
                }
            }
        }
    }
}

void GridGraphBuilder::addRegWrongWayConn(int guideIdx) {
    const db::GridBoxOnLayer &box = localNet.gridRouteGuides[guideIdx];

    if (!database.isValid(box)) return;

    const auto &cpRange = box.crossPointRange;
    const auto &trackRange = box.trackRange;

    int numWrongWayPoint = (cpRange.range() + 1) * db::rrrIterSetting.wrongWayPointDensity;

    int pitch = database.getLayer(box.layerIdx).pitch;
    db::CostT wrongWayCost = db::setting.wrongWayPenaltyCoeff * pitch;

    auto setEdgeCost = [&](int crossPointIdx, int beginTrack, int endTrack) {
        int u = guideToVertex(guideIdx, beginTrack, crossPointIdx);
        int v = guideToVertex(guideIdx, endTrack, crossPointIdx);
        graph.addEdge(u, v, RIGHT, wrongWayCost);
    };

    if (numWrongWayPoint == 1) {
        int crossPointIdx = (cpRange.high + cpRange.low) / 2;
        for (int t = trackRange.low; t < trackRange.high; t++) setEdgeCost(crossPointIdx, t, t + 1);
    } else if (numWrongWayPoint > 1) {
        int wrongWayPointDist = (cpRange.range() + 1 - numWrongWayPoint) / (numWrongWayPoint - 1);
        for (int i = 0; i < numWrongWayPoint; i++) {
            int crossPointIdx = i * (1 + wrongWayPointDist) + cpRange.low;
            for (int t = trackRange.low; t < trackRange.high; t++) setEdgeCost(crossPointIdx, t, t + 1);
        }
    }
}

void GridGraphBuilder::connectGuide(int guideIdx) {
    const db::GridBoxOnLayer &box = localNet.gridRouteGuides[guideIdx];

    if (!database.isValid(box)) return;

    const auto &cpRange = box.crossPointRange;
    const auto &trackRange = box.trackRange;
    int layerIdx = box.layerIdx;
    int netIdx = localNet.idx;

    auto setEdgeCost = [&](int trackIdx, int beginCP, int endCP) {
        if (beginCP == endCP) return;

        int u = guideToVertex(guideIdx, trackIdx, beginCP);
        int v = guideToVertex(guideIdx, trackIdx, endCP);

        if (endCP - beginCP == 1) {
            graph.addEdge(u, v, FORWARD, 0);
        } else {
            db::CostT cost = database.getWireSegmentCost({layerIdx, trackIdx, {beginCP + 1, endCP - 1}}, netIdx);
            int penalty = localNet.getWireSegmentPenalty(guideIdx, trackIdx, beginCP, endCP);
            graph.addEdge(u, v, FORWARD, cost * (1 + penalty));
        }
    };

    auto canRemove = [&](int trackIdx, int crossPointIdx) {
        int vertexIdx = guideToVertex(guideIdx, trackIdx, crossPointIdx);
        return !graph.hasEdge(vertexIdx, UP) && !graph.hasEdge(vertexIdx, DOWN) && !graph.hasEdge(vertexIdx, LEFT) &&
               !graph.hasEdge(vertexIdx, RIGHT) && graph.getPinIdx(vertexIdx) == -1 && crossPointIdx != cpRange.low &&
               crossPointIdx != cpRange.high;
    };

    if (cpRange.range() == 0) {
        for (int t = trackRange.low; t <= trackRange.high; t++) {
            int vertex = guideToVertex(guideIdx, t, cpRange.low);
            db::CostT cost = database.getWireSegmentCost({layerIdx, t, cpRange}, netIdx);
            int penalty = localNet.getCrossPointPenalty(guideIdx, t, cpRange.low);
            graph.setVertexCost(vertex, cost * (1 + penalty));
        }
        return;
    }

    for (int t = trackRange.low; t <= trackRange.high; t++) {
        // get the directIntervals
        vector<utils::IntervalT<int>> directIntervals;
        int begin = -1, end = -1;
        for (int c = cpRange.low; c <= cpRange.high; c++) {
            if (begin == -1) {
                if (canRemove(t, c)) begin = c - 1;
            } else {
                if (!canRemove(t, c)) {
                    end = c;
                    directIntervals.emplace_back(begin, end);
                    begin = -1;
                    end = -1;
                }
            }
        }

        // get the indirectIntervals
        vector<utils::IntervalT<int>> indirectIntervals;
        if (directIntervals.empty()) {
            indirectIntervals.push_back(cpRange);
        } else {
            indirectIntervals.emplace_back(cpRange.low, directIntervals.begin()->low);
            for (int i = 0; i + 1 < directIntervals.size(); i++) {
                indirectIntervals.emplace_back(directIntervals[i].high, directIntervals[i + 1].low);
            }
            indirectIntervals.emplace_back(directIntervals.rbegin()->high, cpRange.high);
        }

        // connect the intervals
        for (auto &interval : directIntervals) setEdgeCost(t, interval.low, interval.high);

        for (auto &interval : indirectIntervals) {
            for (int c = interval.low; c + 1 <= interval.high; c++) setEdgeCost(t, c, c + 1);

            const vector<db::CostT> &crossPointCost = database.getShortWireSegmentCost({layerIdx, t, interval}, netIdx);
            for (int c = interval.low; c <= interval.high; c++) {
                int vertex = guideToVertex(guideIdx, t, c);
                db::CostT cost = crossPointCost[c - interval.low];
                int penalty = localNet.getCrossPointPenalty(guideIdx, t, c);
                graph.setVertexCost(vertex, cost * (1 + penalty));
            }
        }
    }
}

void GridGraphBuilder::connectTwoGuides(int guideIdx1, int guideIdx2) {
    const db::GridBoxOnLayer &box1 = localNet.gridRouteGuides[guideIdx1];
    const db::GridBoxOnLayer &box2 = localNet.gridRouteGuides[guideIdx2];

    if (!database.isValid(box1) || !database.isValid(box2)) {
        return;
    }

    int upperIdx = box1.layerIdx > box2.layerIdx ? guideIdx1 : guideIdx2;
    int lowerIdx = box1.layerIdx < box2.layerIdx ? guideIdx1 : guideIdx2;

    db::ViaBox viaBox =
        database.getViaBoxBetween(localNet.gridRouteGuides[lowerIdx], localNet.gridRouteGuides[upperIdx]);
    if (!database.isValid(viaBox)) return;

    for (int lowerTrackIdx = viaBox.lower.trackRange.low; lowerTrackIdx <= viaBox.lower.trackRange.high;
         lowerTrackIdx++) {
        int upperCPIdx = database.getLayer(viaBox.lower.layerIdx).tracks[lowerTrackIdx].upperCPIdx;

        for (int upperTrackIdx = viaBox.upper.trackRange.low; upperTrackIdx <= viaBox.upper.trackRange.high;
             upperTrackIdx++) {
            int lowerCPIdx = database.getLayer(viaBox.upper.layerIdx).tracks[upperTrackIdx].lowerCPIdx;

            int u = guideToVertex(upperIdx, upperTrackIdx, upperCPIdx);
            int v = guideToVertex(lowerIdx, lowerTrackIdx, lowerCPIdx);

            db::CostT edgeCost = database.getViaCost({viaBox.lower.layerIdx, lowerTrackIdx, lowerCPIdx}, localNet.idx);
            int penalty =
                localNet.getViaPenalty(upperIdx, upperTrackIdx, upperCPIdx, lowerIdx, lowerTrackIdx, lowerCPIdx);

            graph.addEdge(u, v, DOWN, edgeCost * (1 + penalty));
        }
    }
}

void GridGraphBuilder::setMinAreaFlags() {
    minAreaFixable.resize(intervals.back().second, false);
    for (unsigned guideIdx = 0; guideIdx < localNet.gridRouteGuides.size(); guideIdx++) {
        const db::GridBoxOnLayer &box = localNet.gridRouteGuides[guideIdx];

        if (!database.isValid(box)) continue;

        const db::MetalLayer &layer = database.getLayer(box.layerIdx);
        const DBU minLen = layer.getMinLen();
        int loCPIdx = box.crossPointRange.low;
        int hiCPIdx = box.crossPointRange.high;

        int low = loCPIdx - 1;
        int high = hiCPIdx + 1;
        while (low >= 0 && minLen > layer.getCrossPointRangeDist({low, loCPIdx})) low--;
        low = max(0, low);
        while (high < layer.numCrossPoints() && minLen > layer.getCrossPointRangeDist({hiCPIdx, high})) high++;
        high = min(high, layer.numCrossPoints() - 1);
        utils::IntervalT<int> queryInterval(low, high);

        for (int t = box.trackRange.low; t <= box.trackRange.high; t++) {
            vector<utils::IntervalT<int>> emptyIntervals =
                database.getEmptyIntvl({layer.idx, t, queryInterval}, localNet.idx);

            const int vertexIdxBias = guideToVertex(guideIdx, t, loCPIdx);

            for (const auto &interval : emptyIntervals) {
                if (layer.getCrossPointRangeDist(interval) >= minLen) {
                    for (int c = interval.low; c <= interval.high; c++) {
                        if (c >= loCPIdx && c <= hiCPIdx) minAreaFixable[vertexIdxBias + c - loCPIdx] = true;
                    }
                }
            }
        }
    }
}

int GridGraphBuilder::guideToVertex(int gIdx, int trackIdx, int cpIdx) const {
    const db::GridBoxOnLayer &box = localNet.gridRouteGuides[gIdx];
    int pointBias = intervals[gIdx].first;

    return boxToVertex(box, pointBias, trackIdx, cpIdx);
}

int GridGraphBuilder::boxToVertex(const db::GridBoxOnLayer &box, int pointBias, int trackIdx, int cpIdx) const {
    return pointBias + (trackIdx - box.trackRange.low) * (box.crossPointRange.range() + 1) +
           (cpIdx - box.crossPointRange.low);
}