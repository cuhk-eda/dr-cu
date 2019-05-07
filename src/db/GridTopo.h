#pragma once

#include "GeoPrimitive.h"

namespace db {

class GridSteiner : public GridPoint {
public:
    int pinIdx;  // -1 stands for "not pin"
    std::shared_ptr<GridSteiner> parent;
    vector<std::shared_ptr<GridSteiner>> children;

    GridSteiner(const GridPoint& gridPoint, int pinIndex = -1) : GridPoint(gridPoint), pinIdx(pinIndex) {}

    // Set/reset parent
    static void setParent(std::shared_ptr<GridSteiner> childNode, std::shared_ptr<GridSteiner> parentNode);
    static void resetParent(std::shared_ptr<GridSteiner> node);

    // Traverse
    static void preOrder(std::shared_ptr<GridSteiner> node, const std::function<void(std::shared_ptr<GridSteiner>)>& visit);
    static void postOrder(std::shared_ptr<GridSteiner> node, const std::function<void(std::shared_ptr<GridSteiner>)>& visit);
    static void postOrderCopy(std::shared_ptr<GridSteiner> node, const std::function<void(std::shared_ptr<GridSteiner>)>& visit);

    // Merge two same-layer edges (assume they are on the same track)
    static void mergeNodes(std::shared_ptr<GridSteiner> root);

    friend ostream& operator<<(ostream& os, const GridSteiner& node);
    void printTree(ostream& os = std::cout, int depth = 0);
};

}  // namespace db
