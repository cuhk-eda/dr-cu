#pragma once

#include "LocalNet.h"

class PreRoute {
public:
    PreRoute(LocalNet& localNetData) : localNet(localNetData) {}

    db::RouteStatus runIterative();

private:
    LocalNet& localNet;

    db::RouteStatus run(int numPitchForGuideExpand);
    void expandGuidesToMargin();
    db::RouteStatus expandGuidesToCoverPins();

    bool checkGuideConnTrack() const;
};