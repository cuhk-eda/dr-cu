#include "RouteGuideGraph.h"

void RouteGuideGraph::initConn(const vector<vector<db::GridBoxOnLayer>>& pinAccessBoxes,
                               const vector<db::GridBoxOnLayer>& routeGuides) {
    pinGuideConn.clear();
    pinGuideConn.resize(pinAccessBoxes.size());
    guidePinConn.clear();
    guidePinConn.resize(routeGuides.size());
    for (unsigned p = 0; p < pinAccessBoxes.size(); p++) {
        const auto& accessBoxes = pinAccessBoxes[p];
        for (unsigned a = 0; a < accessBoxes.size(); a++) {
            for (unsigned g = 0; g < routeGuides.size(); g++) {
                if (database.isConnected(routeGuides[g], accessBoxes[a])) {
                    pinGuideConn[p].emplace_back(g, a);
                    guidePinConn[g].emplace_back(p, a);
                }
            }
        }
    }

    guideConn.clear();
    guideConn.resize(routeGuides.size());
    for (unsigned g1 = 0; g1 < routeGuides.size(); g1++) {
        for (unsigned g2 = g1 + 1; g2 < routeGuides.size(); g2++) {
            if (database.isConnected(routeGuides[g1], routeGuides[g2])) {
                guideConn[g2].push_back(g1);
                guideConn[g1].push_back(g2);
            }
        }
    }

    guideAdj.clear();
    guideAdj.resize(routeGuides.size());
    for (unsigned g1 = 0; g1 < routeGuides.size(); g1++) {
        for (unsigned g2 = g1 + 1; g2 < routeGuides.size(); g2++) {
            if (database.isAdjacent(routeGuides[g1], routeGuides[g2])) {
                guideAdj[g2].push_back(g1);
                guideAdj[g1].push_back(g2);
            }
        }
    }
}

bool RouteGuideGraph::checkGuideConn() const {
    // init to all false
    vector<char> pinVisited(pinGuideConn.size(), false);
    vector<char> guideVisited(guideConn.size(), false);

    // define recursive std::functions VisitPin & VisitGuide
    std::function<void(int)> VisitPin, VisitGuide;
    VisitPin = [&](int pinIdx) {
        pinVisited[pinIdx] = true;
        for (const auto& ga : pinGuideConn[pinIdx]) {
            int guideIdx = ga.first;
            if (!guideVisited[guideIdx]) {
                VisitGuide(guideIdx);
            }
        }
    };
    VisitGuide = [&](int guideIdx) {
        guideVisited[guideIdx] = true;
        for (int adjGuideIdx : guideConn[guideIdx]) {
            if (!guideVisited[adjGuideIdx]) {
                VisitGuide(adjGuideIdx);
            }
        }
        for (const auto& pa : guidePinConn[guideIdx]) {
            int pinIdx = pa.first;
            if (!pinVisited[pinIdx]) {
                VisitPin(pinIdx);
            }
        }
    };

    // DFS from pin 0
    VisitPin(0);

    return all_of(pinVisited.begin(), pinVisited.end(), [](bool visited) { return visited; });
}

bool RouteGuideGraph::checkPinGuideConn() const {
    for (const auto& pinAdj : pinGuideConn) {
        if (pinAdj.empty()) {
            return false;
        }
    }
    return true;
}

void RouteGuideGraph::print() const {
    log() << "guide-guide conn" << std::endl;
    for (int i = 0; i < guideConn.size(); ++i) {
        log() << "guide " << i << ": " << guideConn[i] << std::endl;
    }
    log() << "guide-pin conn" << std::endl;
    for (int i = 0; i < guidePinConn.size(); ++i) {
        log() << "guide " << i << ": " << guidePinConn[i] << std::endl;
    }
    log() << "pin-guide conn" << std::endl;
    for (int i = 0; i < pinGuideConn.size(); ++i) {
        log() << "pin " << i << ": " << pinGuideConn[i] << std::endl;
    }
}
