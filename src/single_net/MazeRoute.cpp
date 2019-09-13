#include "MazeRoute.h"
#include "UpdateDB.h"

ostream &operator<<(ostream &os, const Solution &sol) {
    os << "cost=" << sol.cost << ", len=" << sol.len << ", vertex=" << sol.vertex
       << ", prev=" << (sol.prev ? sol.prev->vertex : -1);
    return os;
}

db::RouteStatus MazeRoute::run() {
    GridGraphBuilder graphBuilder(localNet, graph);
    graphBuilder.run();

    vertexCostUBs.assign(graph.getNodeNum(), std::numeric_limits<db::CostT>::max());
    // vertexCostLBs.assign(graph.getNodeNum(), 0);
    pinSols.assign(localNet.numOfPins(), nullptr);
    const int startPin = 0;

    auto status = route(startPin);
    if (!db::isSucc(status)) {
        return status;
    }

    getResult();

    db::routeStat.increment(db::RouteStage::MAZE, status);
    return status;
}

db::RouteStatus MazeRoute::route(int startPin) {
    // define std::priority_queue
    auto solComp = [](const std::shared_ptr<Solution> &lhs, const std::shared_ptr<Solution> &rhs) {
        return rhs->cost < lhs->cost || (rhs->cost == lhs->cost && rhs->costUB < lhs->costUB);
    };
    std::priority_queue<std::shared_ptr<Solution>, vector<std::shared_ptr<Solution>>, decltype(solComp)> solQueue(
        solComp);

    auto updateSol = [&](const std::shared_ptr<Solution> &sol) {
        solQueue.push(sol);
        if (sol->costUB < vertexCostUBs[sol->vertex]) {
            vertexCostUBs[sol->vertex] = sol->costUB;
        }
    };

    // init from startPin
    for (auto vertex : graph.getVertices(startPin)) {
        DBU minLen = graph.isFakePin(vertex) ? 0 : database.getLayer(graph.getGridPoint(vertex).layerIdx).getMinLen();
        updateSol(std::make_shared<Solution>(
            graph.getVertexCost(vertex), minLen, graph.getVertexCost(vertex), vertex, nullptr));
    }
    std::unordered_set<int> visitedPin = {startPin};
    int nPinToConnect = localNet.numOfPins() - 1;

    while (nPinToConnect != 0) {
        std::shared_ptr<Solution> dstVertex;
        int dstPinIdx = -1;

        // Dijkstra
        while (!solQueue.empty()) {
            auto newSol = solQueue.top();
            int u = newSol->vertex;
            solQueue.pop();

            // reach a pin?
            dstPinIdx = graph.getPinIdx(u);
            if (dstPinIdx != -1 && visitedPin.find(dstPinIdx) == visitedPin.end()) {
                dstVertex = newSol;
                break;
            }

            // pruning by upper bound
            if (vertexCostUBs[u] < newSol->cost) continue;

            const db::MetalLayer &uLayer = database.getLayer(graph.getGridPoint(u).layerIdx);

            for (auto direction : directions) {
                if (!graph.hasEdge(u, direction) ||
                    (newSol->prev && graph.getEdgeEndPoint(u, direction) == newSol->prev->vertex)) {
                    continue;
                }

                // from u to v
                int v = graph.getEdgeEndPoint(u, direction);
                const db::MetalLayer &vLayer = database.getLayer(graph.getGridPoint(v).layerIdx);
                bool areOverlappedVertexes = (switchLayer(direction) && graph.getEdgeCost(u, direction) == 0);

                // edge cost
                db::CostT w = areOverlappedVertexes ? 0 : graph.getEdgeCost(u, direction) + graph.getVertexCost(v);

                // minArea penalty
                db::CostT penalty = 0;
                if (!areOverlappedVertexes && switchLayer(direction)) {
                    if (uLayer.hasMinLenVioAcc(newSol->len)) {
                        if (graph.isMinAreaFixable(u) || dstPinIdx != -1) {
                            penalty = uLayer.getMinLen() - newSol->len;
                        } else {
                            penalty = database.getUnitMinAreaVioCost();
                        }
                    }
                }

                db::CostT newCost = w + newSol->cost + penalty;
                DBU newLen;
                const db::GridPoint &vPoint = graph.getGridPoint(v);
                if (graph.getGridPoint(u).layerIdx == graph.getGridPoint(v).layerIdx) {
                    const db::GridPoint &uPoint = graph.getGridPoint(u);
                    newLen = newSol->len;
                    utils::IntervalT<int> cpRange =
                        uPoint.crossPointIdx < vPoint.crossPointIdx
                            ? utils::IntervalT<int>(uPoint.crossPointIdx, vPoint.crossPointIdx)
                            : utils::IntervalT<int>(vPoint.crossPointIdx, uPoint.crossPointIdx);
                    utils::IntervalT<int> trackRange = uPoint.trackIdx < vPoint.trackIdx
                                                           ? utils::IntervalT<int>(uPoint.trackIdx, vPoint.trackIdx)
                                                           : utils::IntervalT<int>(vPoint.trackIdx, uPoint.trackIdx);
                    newLen += uLayer.getCrossPointRangeDist(cpRange);
                    newLen += uLayer.pitch * trackRange.range();
                } else {
                    newLen = 0;
                }
                newLen = min(newLen, database.getLayer(graph.getGridPoint(v).layerIdx).getMinLen());

                // potential minArea penalty
                db::CostT potentialPenalty = 0;
                if (vLayer.hasMinLenVioAcc(newLen)) {
                    if (graph.isMinAreaFixable(v) || graph.getPinIdx(v) != -1) {
                        potentialPenalty = vLayer.getMinLen() - newLen;
                    } else {
                        potentialPenalty = database.getUnitMinAreaVioCost();
                    }
                }
                // if (newCost < vertexCostUBs[v] && !(newCost == vertexCostLBs[v] && (newCost + potentialPenalty) ==
                // vertexCostUBs[v])) {
                if (newCost < vertexCostUBs[v]) {
                    updateSol(std::make_shared<Solution>(newCost, newLen, newCost + potentialPenalty, v, newSol));
                }
            }
        }

        if (!dstVertex) {
            printWarnMsg(db::RouteStatus::FAIL_DISCONNECTED_GRID_GRAPH, localNet.dbNet);
            return db::RouteStatus::FAIL_DISCONNECTED_GRID_GRAPH;
        }

        // update pinSols
        pinSols[dstPinIdx] = dstVertex;

        // mark the path to be zero
        auto tmp = dstVertex;
        while (tmp && tmp->cost != 0) {
            DBU minLen = database.getLayer(graph.getGridPoint(tmp->vertex).layerIdx).getMinLen();
            updateSol(std::make_shared<Solution>(0, minLen, 0, tmp->vertex, tmp->prev));
            tmp = tmp->prev;
        }

        // mark all the accessbox of the pin to be almost zero
        for (auto vertex : graph.getVertices(dstPinIdx)) {
            if (vertex == dstVertex->vertex) continue;
            DBU minLen =
                graph.isFakePin(vertex) ? 0 : database.getLayer(graph.getGridPoint(vertex).layerIdx).getMinLen();
            updateSol(std::make_shared<Solution>(
                graph.getVertexCost(vertex), minLen, graph.getVertexCost(vertex), vertex, nullptr));
        }

        visitedPin.insert(dstPinIdx);
        nPinToConnect--;
    }

    return db::RouteStatus::SUCC_NORMAL;
}

void MazeRoute::getResult() {
    std::unordered_map<int, std::shared_ptr<db::GridSteiner>> visited;

    // back track from pin to source
    for (unsigned p = 0; p < localNet.numOfPins(); p++) {
        std::unordered_map<int, std::shared_ptr<db::GridSteiner>> curVisited;
        auto cur = pinSols[p];
        std::shared_ptr<db::GridSteiner> prevS;
        while (cur) {
            auto it = visited.find(cur->vertex);
            if (it != visited.end()) {
                // graft to an existing node
                if (prevS) {
                    db::GridSteiner::setParent(prevS, it->second);
                }
                break;
            } else {
                // get curS
                auto curS = std::make_shared<db::GridSteiner>(
                    graph.getGridPoint(cur->vertex), graph.getPinIdx(cur->vertex), graph.isFakePin(cur->vertex));
                if (prevS) {
                    db::GridSteiner::setParent(prevS, curS);
                }
                if (curVisited.find(cur->vertex) != curVisited.end() && db::setting.singleNetVerbose >= +db::VerboseLevelT::MIDDLE) {
                    printlog("Warning: self loop found in a path for net", localNet.getName(), "for pin", p);
                }
                curVisited.emplace(cur->vertex, curS);
                // store tree root
                if (!(cur->prev)) {
                    localNet.gridTopo.push_back(curS);
                    break;
                }
                // prep for the next loop
                prevS = curS;
                cur = cur->prev;
            }
        }
        for (const auto &v : curVisited) visited.insert(v);
    }

    // remove redundant Steiner nodes
    for (auto &tree : localNet.gridTopo) {
        db::GridSteiner::mergeNodes(tree);
    }
}
