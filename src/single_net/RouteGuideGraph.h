#pragma once

#include "db/Database.h"

class RouteGuideGraph {
public:
    void print() const;

    // Two priciples:
    // 1. fast std::pair query (whether A & B are connected)
    // 2. fast relation query (who overlap/adjacent with A)
    vector<vector<int>> guideConn;                // routeGuideIdx -> ovlp routeGuideIdx
    vector<vector<int>> guideAdj;                 // routeGuideIdx -> adj routeGuideIdx
    vector<vector<std::pair<int, int>>> pinGuideConn;  // pinIdx -> ovlp (routeGuideIdx, accessBoxIdx)
    vector<vector<std::pair<int, int>>> guidePinConn;  // routeGuideIdx -> ovlp (pinIdx, accessBoxIdx)

    void initConn(const vector<vector<db::GridBoxOnLayer>>& pinAccessBoxes,
                  const vector<db::GridBoxOnLayer>& routeGuides);
    bool checkGuideConn() const;
    bool checkPinGuideConn() const;
};
