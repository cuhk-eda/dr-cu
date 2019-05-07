#pragma once

#include "GridGraphBuilder.h"

class Solution {
public:
    db::CostT cost;
    DBU len;           // length on current track
    db::CostT costUB;  // cost upper bound
    int vertex;
    std::shared_ptr<Solution> prev;

    Solution(db::CostT c, DBU l, db::CostT ub, int v, const std::shared_ptr<Solution> &p)
        : cost(c), len(l), costUB(ub), vertex(v), prev(p) {}

    friend ostream &operator<<(ostream &os, const Solution &sol);
};

class MazeRoute {
public:
    MazeRoute(LocalNet &localNetData) : localNet(localNetData) {}

    db::RouteStatus run();

private:
    LocalNet &localNet;
    GridGraph graph;

    vector<db::CostT> vertexCostUBs;       // min cost upper bound for each vertex
    // vector<db::CostT> vertexCostLBs;       // cost lower bound corresponding to the min-upper-bound solution for each vertex
    vector<std::shared_ptr<Solution>> pinSols;  // best solution for each pin

    db::RouteStatus route(int startPin);
    void getResult();
};
