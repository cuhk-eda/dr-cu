#include "LocalNet.h"

class PostMazeRoute {
public:
    PostMazeRoute(LocalNet& localNetData) : localNet(localNetData) {}
    void run();

private:
    LocalNet& localNet;
    // Remove track switch which goes to another layer and back with via spacing violation
    void removeTrackSwitchWithVio();
    void extendMinAreaWires();
    vector<db::GridEdge> getExtendWireRects(const db::GridEdge& edge) const;
};
