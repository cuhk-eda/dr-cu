#include "GridGraph.h"

#include <fstream>

void GridGraph::init(int nNodes) {
    conn.resize(nNodes, {-1, -1, -1, -1, -1, -1});
    edgeCost.resize(nNodes, {-1, -1, -1, -1, -1, -1});
    vertexCost.resize(nNodes, 0);

    edgeCount = 0;
}

void GridGraph::addEdge(int u, int v, EdgeDirection dir, db::CostT w) {
    if (hasEdge(u, dir)) return;

    edgeCost[u][dir] = edgeCost[v][getOppDir(dir)] = w;
    conn[u][dir] = v;
    conn[v][oppDirections[dir]] = u;

    edgeCount++;
}

int GridGraph::getPinIdx(int u) const {
    auto it = vertexToPin.find(u);
    return (it != vertexToPin.end()) ? it->second : -1;
}

void GridGraph::writeDebugFile(const std::string& fn) const {
    std::ofstream debugFile(fn);
    for (int i = 0; i < conn.size(); ++i) {
        debugFile << vertexToGridPoint[i] << " vertexC=" << getVertexCost(i) << " edgeC=" << edgeCost[i] << std::endl;
    }
}

bool switchLayer(EdgeDirection direction) { return direction == UP || direction == DOWN; }
bool wrongWay(EdgeDirection direction) { return direction == LEFT || direction == RIGHT; }
EdgeDirection getOppDir(EdgeDirection direction) { return oppDirections[direction]; }
