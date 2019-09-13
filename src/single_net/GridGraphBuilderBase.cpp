#include "GridGraphBuilderBase.h"
#include "PinTapConnector.h"

double GridGraphBuilderBase::getPinPointCost(const vector<db::BoxOnLayer> &accessBoxes, const db::GridPoint &grid) {
    auto point = database.getLoc(grid);
    DBU minDist = std::numeric_limits<DBU>::max() / 10;  // make it safe for adding in case no same-layer box
    double cost = -1;
    for (const auto &box : accessBoxes) {
        if (box.layerIdx != grid.layerIdx) continue;

        DBU dist = Dist(box, point);
        if (dist < minDist) {
            minDist = dist;
            cost = minDist * outOfPinWireLengthPenalty;
        }
    }

    if (cost == -1) {
        for (const auto &box : accessBoxes) {
            DBU dist = Dist(box, point);
            if (dist < minDist) {
                minDist = dist;
                cost = minDist * outOfPinWireLengthPenalty;
            }
        }
        cost += database.getUnitViaCost();
    }
    
    return cost;
}

void GridGraphBuilderBase::updatePinVertex(int pinIdx, int vertexIdx, bool fakePin) {
    if (fakePin) {
        graph.fakePins.insert(vertexIdx);
    }
    pinToOriVertex[pinIdx].push_back(vertexIdx);
    auto it = graph.vertexToPin.find(vertexIdx);
    if (it != graph.vertexToPin.end()) {
        int oriPinIdx = it->second;
        if (pinIdx != oriPinIdx) {
            const db::GridPoint& point = vertexToGridPoint[vertexIdx];
            const double oriCost = getPinPointCost(localNet.dbNet.pinAccessBoxes[oriPinIdx], point);
            const double newCost = getPinPointCost(localNet.dbNet.pinAccessBoxes[pinIdx], point);
            if (oriCost < newCost) {
                graph.vertexToPin[vertexIdx] = pinIdx;
                auto &oriPinToVertex = graph.pinToVertex[oriPinIdx];
                auto oriIt = find(oriPinToVertex.begin(), oriPinToVertex.end(), vertexIdx);
                assert(oriIt != oriPinToVertex.end());
                *oriIt = oriPinToVertex.back();
                oriPinToVertex.pop_back();
                graph.pinToVertex[pinIdx].push_back(vertexIdx);
            }
        }
    } else {
        graph.vertexToPin[vertexIdx] = pinIdx;
        graph.pinToVertex[pinIdx].push_back(vertexIdx);
    }
}

void GridGraphBuilderBase::addOutofPinPenalty() {
    for (unsigned p = 0; p < localNet.numOfPins(); p++) {
        for (auto vertex : graph.pinToVertex[p]) {
            graph.vertexCost[vertex] += getPinPointCost(localNet.dbNet.pinAccessBoxes[p], vertexToGridPoint[vertex]);
            PinTapConnector pinTapConnector(vertexToGridPoint[vertex], localNet.dbNet, p);
            pinTapConnector.run();
            if (pinTapConnector.bestVio > 0) {
                graph.vertexCost[vertex] += database.getUnitSpaceVioCost();
            }
        }
    }
}

void GridGraphBuilderBase::fixDisconnectedPin() {
    for (unsigned pinIdx = 0; pinIdx < localNet.numOfPins(); pinIdx++) {
        if (graph.pinToVertex[pinIdx].size() == 0) {
            double minCost = std::numeric_limits<DBU>::max();
            int givenVertexIdx, friendPinIdx; 
            for (int vertexIdx : pinToOriVertex[pinIdx]) {
                if (graph.pinToVertex[graph.vertexToPin[vertexIdx]].size() > 1) {
                    if (minCost > graph.vertexCost[vertexIdx]) {
                        givenVertexIdx = vertexIdx;
                        friendPinIdx = graph.vertexToPin[vertexIdx];
                        minCost = graph.vertexCost[vertexIdx];
                    }
                }
            }
            if (minCost != std::numeric_limits<DBU>::max()) {
                graph.vertexToPin[givenVertexIdx] = pinIdx;
                for (int eraseIdx = 0; eraseIdx < graph.pinToVertex[friendPinIdx].size(); eraseIdx++) {
                    if (graph.pinToVertex[friendPinIdx][eraseIdx] == givenVertexIdx){
                        graph.pinToVertex[friendPinIdx].erase(graph.pinToVertex[friendPinIdx].begin() + eraseIdx);
                        break;
                    } 
                }   
            }
        }
    }
}
