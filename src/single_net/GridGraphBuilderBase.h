#pragma once

#include "GridGraph.h"
#include "LocalNet.h"

class GridGraphBuilderBase {
public:
    GridGraphBuilderBase(LocalNet &localNetData, GridGraph &gridGraph)
        : localNet(localNetData),
          graph(gridGraph),
          vertexToGridPoint(graph.vertexToGridPoint),
          minAreaFixable(graph.minAreaFixable) {
        graph.pinToVertex.resize(localNetData.numOfPins());
    }

    virtual void run() = 0;

protected:
    LocalNet &localNet;
    GridGraph &graph;

    // reference to GridGraph
    vector<db::GridPoint> &vertexToGridPoint;
    vector<bool> &minAreaFixable;

    vector<std::pair<int, int>> intervals;
    vector<vector<int>> pinToOriVertex;

    // Besides wrong-way wire cost itself, discourage out-of-pin taps slightly more
    // Because violations between link and via/wire are out of control now
    double outOfPinWireLengthPenalty = db::setting.weightWrongWayWirelength / db::setting.weightWirelength + 1;

    // TODO: replace getPinPointCost() by PinTapConnector
    double getPinPointCost(const vector<db::BoxOnLayer> &accessBoxes, const db::GridPoint &grid);
    void updatePinVertex(int pinIdx, int vertexIdx, bool fakePin = false);
    void addOutofPinPenalty();
    virtual void setMinAreaFlags() = 0;
    void fixDisconnectedPin();
};
