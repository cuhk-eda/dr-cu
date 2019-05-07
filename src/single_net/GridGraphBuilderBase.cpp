#include "GridGraphBuilderBase.h"
#include "PinTapConnector.h"

DBU GridGraphBuilderBase::getPinPointDist(const vector<db::BoxOnLayer> &accessBoxes, const db::GridPoint &grid) {
    auto point = database.getLoc(grid);
    DBU minDist = std::numeric_limits<DBU>::max() / 10;  // make it safe for adding in case no same-layer box
    for (const auto &box : accessBoxes) {
        if (box.layerIdx != grid.layerIdx) continue;

        DBU dist = Dist(box, point);
        if (dist < minDist) {
            minDist = dist;
        }
    }
    return minDist;
}

void GridGraphBuilderBase::updatePinVertex(int pinIdx, int vertexIdx) {
    auto it = graph.vertexToPin.find(vertexIdx);
    if (it != graph.vertexToPin.end()) {
        int oriPinIdx = it->second;
        if (pinIdx != oriPinIdx) {
            auto point = vertexToGridPoint[vertexIdx];
            DBU oriDist = getPinPointDist(localNet.pinAccessBoxes[oriPinIdx], point);
            DBU newDist = getPinPointDist(localNet.pinAccessBoxes[pinIdx], point);
            if (newDist < oriDist) {
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
            graph.vertexCost[vertex] += getPinPointDist(localNet.dbNet.pinAccessBoxes[p], vertexToGridPoint[vertex]) *
                                        outOfPinWireLengthPenalty;
            PinTapConnector pinTapConnector(vertexToGridPoint[vertex], localNet.dbNet, p);
            if (pinTapConnector.bestVio > 0) {
                graph.vertexCost[vertex] += database.getUnitSpaceVioCost();
            }
            // Note: before fixing (removing redundant pinToVertex), charged multiple times
        }
    }
}
