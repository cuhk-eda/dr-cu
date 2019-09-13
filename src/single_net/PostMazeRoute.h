#pragma once

#include "LocalNet.h"

class PostMazeRoute {
public:
    PostMazeRoute(db::NetBase& netBase) : net(netBase) {}
    void run();
    void run2();  // rerun extendMinAreaWires

private:
    db::NetBase& net;

    // Remove track switch which goes to another layer and back with via spacing violation
    void removeTrackSwitchWithVio();

    // Extend wires to resolve min area violations
    void extendMinAreaWires();
    void getExtendWireRects(const std::vector<std::shared_ptr<db::GridSteiner>>& candEdge) const;
};