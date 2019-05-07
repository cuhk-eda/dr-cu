#include "LocalNet.h"

void LocalNet::initGridBoxes() {
    // rangeSearch & slice routeGuides
    vector<vector<db::GridBoxOnLayer>> guides(database.getLayerNum());
    for (auto& guide : routeGuides) {
        guides[guide.layerIdx].push_back(database.rangeSearch(guide));
    }
    routeGuides.clear();
    gridRouteGuides.clear();
    for (int layerIdx = 0; layerIdx < database.getLayerNum(); ++layerIdx) {
        db::GridBoxOnLayer::sliceGridPolygons(guides[layerIdx]);
        for (const auto& guide : guides[layerIdx]) {
            gridRouteGuides.push_back(guide);
            routeGuides.push_back(database.getLoc(guide));
        }
    }

    // init gridPinAccessBoxes
    gridPinAccessBoxes.resize(numOfPins());
    for (unsigned p = 0; p < numOfPins(); p++) {
        vector<vector<db::GridBoxOnLayer>> pins(database.getLayerNum());
        for (auto& pin : pinAccessBoxes[p]) {
            int dir = database.getLayerDir(pin.layerIdx);
            DBU pitch = database.getLayer(pin.layerIdx).pitch;
            // pinForbidRegion
            auto pinForbidRegion = database.getMetalRectForbidRegion(pin, false);
            auto gridPinForbidRegion = database.rangeSearch(pinForbidRegion);
            pins[pin.layerIdx].push_back(gridPinForbidRegion);
            // One-pitch extension
            auto pinExtension = pin;
            for (int d = 0; d < 2; ++d) {
                pinExtension[d].low -= pitch;
                pinExtension[d].high += pitch;
            }
            auto gridPinExtension = database.rangeSearch(pinExtension);
            for (int trackIdx = gridPinExtension.trackRange.low; trackIdx <= gridPinExtension.trackRange.high;
                 ++trackIdx) {
                for (int cpIdx = gridPinExtension.crossPointRange.low; cpIdx <= gridPinExtension.crossPointRange.high;
                     ++cpIdx) {
                    db::GridPoint pt(pin.layerIdx, trackIdx, cpIdx);
                    if (!gridPinForbidRegion.includePoint(pt) && Dist(pin, database.getLoc(pt)) <= pitch) {
                        pins[pin.layerIdx].emplace_back(pin.layerIdx,
                                                        utils::IntervalT<int>{trackIdx, trackIdx},
                                                        utils::IntervalT<int>{cpIdx, cpIdx});
                    }
                }
            }
        }
        pinAccessBoxes[p].clear();
        gridPinAccessBoxes[p].clear();
        for (int layerIdx = 0; layerIdx < database.getLayerNum(); ++layerIdx) {
            if (!pins[layerIdx].empty()) {
                db::GridBoxOnLayer::sliceGridPolygons(pins[layerIdx]);
                for (const auto& pin : pins[layerIdx]) {
                    if (database.isValid(pin)) {
                        gridPinAccessBoxes[p].push_back(pin);
                        pinAccessBoxes[p].push_back(database.getLoc(pin));
                    }
                }
            }
        }
    }

    getRouteGuideMapping();
}

void LocalNet::getRouteGuideMapping() {
    using point = bg::model::point<DBU, 2, bg::cs::cartesian>;
    using box = bg::model::box<point>;
    vector<bgi::rtree<std::pair<box, int>, bgi::quadratic<16>>> rtrees(database.getLayerNum());

    for (unsigned i = 0; i < dbNet.routeGuides.size(); ++i) {
        const db::BoxOnLayer& routeGuide = dbNet.routeGuides[i];
        box b(point(routeGuide.x.low, routeGuide.y.low), point(routeGuide.x.high, routeGuide.y.high));
        rtrees[routeGuide.layerIdx].insert(std::make_pair(b, i));
    }

    dbRouteGuideIdxes.resize(gridRouteGuides.size());
    for (unsigned i = 0; i < dbRouteGuideIdxes.size(); i++) {
        const db::BoxOnLayer& routeGuide = routeGuides[i];
        box query_box(point(routeGuide.x.low, routeGuide.y.low), point(routeGuide.x.high, routeGuide.y.high));
        std::vector<std::pair<box, int>> results;
        rtrees[routeGuide.layerIdx].query(bgi::intersects(query_box), std::back_inserter(results));

        dbRouteGuideIdxes.reserve(results.size());

        for (const auto& v : results) {
            dbRouteGuideIdxes[i].push_back(v.second);
        }
    }
}

void LocalNet::initNumOfVertices() {
    estimatedNumOfVertices = 0;
    for (unsigned b1 = 0; b1 < gridRouteGuides.size(); b1++) {
        for (unsigned b2 : guideConn[b1]) {
            if (b2 <= b1) {
                continue;
            }

            const db::GridBoxOnLayer& box1 = gridRouteGuides[b1];
            const db::GridBoxOnLayer& box2 = gridRouteGuides[b2];

            if (!database.isValid(box1) || !database.isValid(box2)) {
                continue;
            }

            const int upperIdx = box1.layerIdx > box2.layerIdx ? b1 : b2;
            const int lowerIdx = box1.layerIdx < box2.layerIdx ? b1 : b2;

            const db::ViaBox viaBox = database.getViaBoxBetween(gridRouteGuides[lowerIdx], gridRouteGuides[upperIdx]);
            if (!database.isValid(viaBox)) {
                continue;
            }
            estimatedNumOfVertices += ((viaBox.lower.trackRange.high + 1 - viaBox.lower.trackRange.low) *
                                       (viaBox.upper.trackRange.high + 1 - viaBox.upper.trackRange.low));
        }
    }
}

bool LocalNet::checkPin() const {
    for (const auto& gridAccessBoxes : gridPinAccessBoxes) {
        int isPinValid = false;
        for (const auto& gridBox : gridAccessBoxes) {
            if (database.isValid(gridBox)) {
                isPinValid = true;
                break;
            }
        }
        if (!isPinValid) {
            return false;
        }
    }
    return true;
}

void LocalNet::print() const {
    db::Net::print();

    // for (int i = 0; i < numOfPins(); ++i) {
    //     log() << gridPinAccessBoxes[i] << std::endl;
    // }

    log() << "dbRouteGuideIdxes" << std::endl;
    for (unsigned i = 0; i < dbRouteGuideIdxes.size(); i++) {
        log() << "guide " << i << ": [";
        for (auto idx : dbRouteGuideIdxes[i]) std::cout << idx << ", ";
        std::cout << "]" << std::endl;
    }

    RouteGuideGraph::print();
}

int LocalNet::getViaPenalty(int guideIdx1, int trackIdx1, int cpIdx1, int guideIdx2, int trackIdx2, int cpIdx2) const {
    bool isContain = false;
    for (auto idx1 : dbRouteGuideIdxes[guideIdx1]) {
        const db::GridBoxOnLayer& box1 = dbNet.gridRouteGuides[idx1];
        if (box1.trackRange.Contain(trackIdx1) && box1.crossPointRange.Contain(cpIdx1)) {
            isContain = true;
            break;
        }
    }
    if (!isContain) return 1;

    for (auto idx2 : dbRouteGuideIdxes[guideIdx2]) {
        const db::GridBoxOnLayer& box2 = dbNet.gridRouteGuides[idx2];
        if (box2.trackRange.Contain(trackIdx2) && box2.crossPointRange.Contain(cpIdx2)) return 0;
    }
    return 1;
}

double LocalNet::getWireSegmentPenalty(int guideIdx, int trackIdx, int cpIdx1, int cpIdx2) const {
    vector<utils::IntervalT<DBU>> intersections;

    db::GridBoxOnLayer segmentGridBox(gridRouteGuides[guideIdx].layerIdx, {trackIdx, trackIdx}, {cpIdx1, cpIdx2});
    db::BoxOnLayer segmentBox = database.getLoc(segmentGridBox);

    utils::IntervalT<DBU> segment = segmentBox.x.range() == 0 ? segmentBox.y : segmentBox.x;

    for (auto idx : dbRouteGuideIdxes[guideIdx]) {
        const db::GridBoxOnLayer& box = dbNet.gridRouteGuides[idx];

        if (box.trackRange.Contain(trackIdx)) {
            if (box.crossPointRange.Contain(cpIdx1) && box.crossPointRange.Contain(cpIdx2)) {
                return 0;
            } else {
                const utils::IntervalT<DBU>& boxSegment =
                    segmentBox.y.range() == 0 ? dbNet.routeGuides[idx].x : dbNet.routeGuides[idx].y;
                utils::IntervalT<DBU> intersection = boxSegment.IntersectWith(segment);
                if (intersection.IsValid()) {
                    intersections.push_back(intersection);
                }
            }
        }
    }

    if (intersections.empty()) {
        return 1;
    } else {
        sort(intersections.begin(),
             intersections.end(),
             [](const utils::IntervalT<DBU>& a, const utils::IntervalT<DBU>& b) {
                 return a.low < b.low || (a.low == b.low && a.high < b.high);
             });

        DBU length = 0, low = 0, high = 0;
        int sz = intersections.size();
        for (int i = 0; i < sz; ++i) {
            low = intersections[i].low;
            high = intersections[i].high;
            while (i + 1 < sz && high >= intersections[i + 1].low) {
                ++i;
                high = intersections[i].high > high ? intersections[i].high : high;
            }
            length += (high - low);
        }

        return 1 - length * 1.0 / segment.range();
    }
}

int LocalNet::getCrossPointPenalty(int guideIdx, int trackIdx, int cpIdx) const {
    for (auto idx : dbRouteGuideIdxes[guideIdx]) {
        const db::GridBoxOnLayer& box = dbNet.gridRouteGuides[idx];
        if (box.trackRange.Contain(trackIdx) && box.crossPointRange.Contain(cpIdx)) return 0;
    }
    return 1;
}
