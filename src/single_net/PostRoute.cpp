#include "PostRoute.h"
#include "PinTapConnector.h"
#include <boost/functional/hash.hpp>

void PostRoute::getPinTapPoints() {
    dbNet.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        if (node->pinIdx >= 0) {
            pinTaps.push_back(node);
        }
    });
}

db::RouteStatus PostRoute::run() {
    db::RouteStatus status = db::RouteStatus::SUCC_NORMAL;

    getPinTapPoints();

    status = connectPins();
    if (!db::isSucc(status)) {
        return status;
    }

    getTopo();

    db::routeStat.increment(db::RouteStage::POST, status);
    return status;
}

db::RouteStatus PostRoute::connectPins() {
    db::RouteStatus netStatus = db::RouteStatus::SUCC_NORMAL;

    vector<vector<std::shared_ptr<db::GridSteiner>>> samePinTaps(dbNet.numOfPins());
    for (auto tap : pinTaps) {
        PinTapConnector pinTapConnector(*tap, dbNet, tap->pinIdx);
        netStatus &= pinTapConnector.run();
        if (!pinTapConnector.bestLink.empty()) {
            linkToPins[tap] = move(pinTapConnector.bestLink);
            samePinTaps[tap->pinIdx].push_back(tap);  // only consider those with links
            if (pinTapConnector.bestVio > 0) {
                db::routeStat.increment(db::RouteStage::POST, db::MiscRouteEvent::LINK_PIN_VIO, 1);
            }
        }
    }

    if (!isSucc(netStatus)) {
        printWarnMsg(netStatus, dbNet);
    }

    // merge overlaped links
    for (const auto &taps : samePinTaps) {
        for (auto tap1 : taps) {
            for (auto tap2 : taps) {
                if (tap1 != tap2 && tap1->layerIdx == tap2->layerIdx) {
                    mergeLinks(linkToPins[tap1], linkToPins[tap2]);
                }
            }
        }
    }
    return netStatus;
}

void PostRoute::mergeLinks(vector<utils::SegmentT<DBU>> &lhs, vector<utils::SegmentT<DBU>> &rhs) {
    for (auto &seg1 : lhs) {
        for (int i2 = 0; i2 < rhs.size(); ++i2) {
            auto &seg2 = rhs[i2];  // make rhs.pop_back() safe
            for (int dir = 0; dir < 2; ++dir) {
                if (seg1[dir].range() != 0 || seg2[dir].range() != 0) continue;
                // both run along 1 - dir
                if (seg1[dir].low != seg2[dir].low) continue;
                // overlap in dir
                auto &segProj1 = seg1[1 - dir];  // projected
                if (segProj1.low > segProj1.high) {
                    std::swap(segProj1.low, segProj1.high);
                }
                auto &segProj2 = seg2[1 - dir];  // projected
                if (segProj2.low > segProj2.high) {
                    std::swap(segProj2.low, segProj2.high);
                }
                // merge
                if (segProj1.low == segProj2.low) {
                    db::routeStat.increment(db::RouteStage::POST, +db::MiscRouteEvent::LINK_MERGE, 1);
                    if (segProj1.high == segProj2.high) {
                        seg2 = rhs.back();
                        rhs.pop_back();
                    } else {
                        if (segProj1.high > segProj2.high) {
                            // make sure segProj1.high < segProj2.high
                            std::swap(segProj1, segProj2);
                        }
                        segProj2.low = segProj1.high;  // remove the overlap
                    }
                } else if (segProj1.high == segProj2.high) {
                    db::routeStat.increment(db::RouteStage::POST, +db::MiscRouteEvent::LINK_MERGE, 1);
                    if (segProj1.low == segProj2.low) {
                        seg2 = rhs.back();
                        rhs.pop_back();
                    } else {
                        if (segProj1.low < segProj2.low) {
                            // make sure segProj1.low > segProj2.low
                            std::swap(segProj1, segProj2);
                        }
                        segProj2.high = segProj1.low;  // remove the overlap
                    }
                }
            }
        }
    }
}

void PostRoute::getTopo() {
    vector<vector<vector<utils::BoxT<DBU>>>> pinMetals(dbNet.numOfPins(),
                                                       vector<vector<utils::BoxT<DBU>>>(database.getLayerNum()));
    dbNet.viaPinVio = 0;
    // 1. from gridTopo
    std::unordered_map<std::shared_ptr<db::GridSteiner>, const db::ViaType *> bestViaTypes;
    dbNet.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        if (node->parent) {
            db::GridEdge edge(*node, *(node->parent));
            if (edge.isVia()) {
                // get pinIdx (consider a neighbor vertex of node-parent edge)
                auto steinerU = node, steinerV = node->parent;
                int pinIdx = node->pinIdx;
                if (node->parent && hasNeighPin(node->parent, node->parent)) {
                    pinIdx = node->parent->pinIdx;
                    std::swap(steinerU, steinerV);  // make steinerU the pin tap
                } else if (node->parent && node->parent->parent && hasNeighPin(node->parent, node->parent->parent)) {
                    pinIdx = node->parent->parent->pinIdx;
                    std::swap(steinerU, steinerV);  // make steinerU the pin tap
                } else {
                    for (auto child : node->children) {
                        if (child->pinIdx >= 0 && hasNeighPin(node, child)) {
                            pinIdx = child->pinIdx;
                            break;
                        }
                    }
                }
                auto bestViaType = getViaType(steinerU, steinerV, pinIdx);
                bestViaTypes[node] = bestViaType;
                database.writeDEFVia(dbNet, database.getLoc(edge.u), *bestViaType, edge.u.layerIdx);
            } else if (edge.isTrackSegment() || edge.isWrongWaySegment()) {
                database.writeDEFWireSegment(dbNet, database.getLoc(edge.u), database.getLoc(edge.v), edge.u.layerIdx);
            } else {
                log() << "Warning in " << __func__ << ": invalid edge type. skip." << std::endl;
            }
        }
    });

    // update pinMetals according to bestViaTypes
    std::function<void(std::shared_ptr<db::GridSteiner>, int, int)> updateToChildren =
        [&](std::shared_ptr<db::GridSteiner> node, int layerIdx, int pinIdx) {
            for (auto child : node->children) {
                // edge node-child
                if (node->layerIdx == layerIdx) {
                    pinMetals[pinIdx][layerIdx].push_back(getEdgeLayerMetal({*node, *child}, bestViaTypes[child]));
                    updateToChildren(child, layerIdx, pinIdx);
                }
            }
        };
    std::function<void(std::shared_ptr<db::GridSteiner>, int, int)> updateToParent =
        [&](std::shared_ptr<db::GridSteiner> node, int layerIdx, int pinIdx) {
            if (node->parent && node->layerIdx == layerIdx) {
                // edge node-parent
                pinMetals[pinIdx][layerIdx].push_back(getEdgeLayerMetal({*node, *(node->parent)}, bestViaTypes[node]));
                updateToParent(node->parent, layerIdx, pinIdx);
            }
        };
    dbNet.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        if (node->pinIdx >= 0) {
            updateToChildren(node, node->layerIdx, node->pinIdx);
            updateToParent(node, node->layerIdx, node->pinIdx);
        }
    });

    // 2. from PinTapPoint::linkToPin
    for (const auto &tap : pinTaps) {
        for (const auto &link : linkToPins[tap]) {
            database.writeDEFWireSegment(dbNet, {link.lx(), link.ly()}, {link.hx(), link.hy()}, tap->layerIdx);
            // for fill rects
            pinMetals[tap->pinIdx][tap->layerIdx].push_back(PinTapConnector::getLinkMetal(link, tap->layerIdx));
        }
    }

    // 3. from extendedWireSegment
    for (const auto &edge : dbNet.extendedWireSegment) {
        utils::BoxT<DBU> box(database.getLoc(edge).first, database.getLoc(edge).second);
        int layerIdx = edge.u.layerIdx;
        DBU halfWidth = database.getLayer(layerIdx).width / 2;
        for (int i = 0; i < 2; ++i) {
            box[i].low -= halfWidth;
            box[i].high += halfWidth;
        }
        database.writeDEFFillRect(dbNet, box, layerIdx);
    }

    // 3. fill metal to resolve same-net violations
    // 3.1 around pins (pins, vias, wires, links)
    for (int pinIdx = 0; pinIdx < dbNet.numOfPins(); ++pinIdx) {
        for (int layerIdx = 0; layerIdx < database.getLayerNum(); ++layerIdx) {
            if (!pinMetals[pinIdx][layerIdx].empty()) {
                for (const auto &ab : dbNet.pinAccessBoxes[pinIdx]) {
                    if (ab.layerIdx == layerIdx) {
                        pinMetals[pinIdx][layerIdx].push_back(ab);
                    }
                }
                MetalFiller metalFiller(pinMetals[pinIdx][layerIdx], layerIdx, true);
                metalFiller.run();
                for (auto &rect : metalFiller.fillMetals) {
                    database.writeDEFFillRect(dbNet, rect, layerIdx);
                }
            }
        }
    }
    // 3.2 bot-top vias
    dbNet.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        for (auto c : node->children) {
            for (auto cc : c->children) {
                for (auto ccc : cc->children) {
                    if (node->layerIdx != c->layerIdx && c->layerIdx == cc->layerIdx && cc->layerIdx != ccc->layerIdx &&
                        node->layerIdx != ccc->layerIdx) {
                        vector<utils::BoxT<DBU>> twoViaMetals;
                        twoViaMetals.push_back(getEdgeLayerMetal({*c, *node}, bestViaTypes[c]));
                        twoViaMetals.push_back(getWireSegmentMetal({*c, *cc}));
                        twoViaMetals.push_back(getEdgeLayerMetal({*cc, *ccc}, bestViaTypes[ccc]));
                        MetalFiller metalFiller(twoViaMetals, c->layerIdx);
                        metalFiller.run();
                        if (!metalFiller.fillMetals.empty()) {
                            db::routeStat.increment(
                                db::RouteStage::POST, db::MiscRouteEvent::FILL_SAME_NET_BOT_TOP_VIAS, 1);
                        }
                        for (auto &rect : metalFiller.fillMetals) {
                            database.writeDEFFillRect(dbNet, rect, c->layerIdx);
                        }
                    }
                }
            }
        }
    });
    // 3.3 via-wire
    // via1 - via2 - wire1 - wire2
    dbNet.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        for (auto c : node->children) {
            for (auto cc : c->children) {
                for (auto ccc : cc->children) {
                    vector<utils::BoxT<DBU>> viaWireMetals;
                    if (node->layerIdx != c->layerIdx && c->layerIdx == cc->layerIdx && cc->layerIdx == ccc->layerIdx && c->trackIdx != ccc->trackIdx) {
                        viaWireMetals.push_back(getEdgeLayerMetal({*c, *node}, bestViaTypes[c]));
                        viaWireMetals.push_back(getWireSegmentMetal({*c, *cc}));
                        viaWireMetals.push_back(getWireSegmentMetal({*cc, *ccc}));
                    }
                    else if (ccc->layerIdx != cc->layerIdx && cc->layerIdx == c->layerIdx && c->layerIdx == node->layerIdx && cc->trackIdx != node->trackIdx) {
                        viaWireMetals.push_back(getEdgeLayerMetal({*cc, *ccc}, bestViaTypes[ccc]));
                        viaWireMetals.push_back(getWireSegmentMetal({*cc, *c}));
                        viaWireMetals.push_back(getWireSegmentMetal({*c, *node}));
                    } else {
                        continue;
                    }
                    MetalFiller metalFiller(viaWireMetals, c->layerIdx);
                    metalFiller.run();
                    if (!metalFiller.fillMetals.empty()) {
                        db::routeStat.increment(
                            db::RouteStage::POST, db::MiscRouteEvent::FILL_SAME_NET_VIA_WIRE, 1);
                    }
                    for (auto &rect : metalFiller.fillMetals) {
                        database.writeDEFFillRect(dbNet, rect, c->layerIdx);
                    }
                }
            }
        }
    });
}

utils::BoxT<DBU> PostRoute::getEdgeLayerMetal(const db::GridEdge &edge, const db::ViaType *viaType) {
    utils::BoxT<DBU> targetMetal;
    if (edge.isVia()) {
        targetMetal = (edge.u.layerIdx < edge.v.layerIdx) ? viaType->bot : viaType->top;
        targetMetal.ShiftBy(database.getLoc(edge.u));
    } else if (edge.isTrackSegment() || edge.isWrongWaySegment()) {
        targetMetal = getWireSegmentMetal(edge);
    } else {
        log() << "Warning in " << __func__ << ": invalid edge type. skip." << std::endl;
    }
    return targetMetal;
}

void PostRoute::processAllSameLayerSameNetBoxes(const vector<db::BoxOnLayer> &pinAccessBoxes,
                                                int layerIdx,
                                                const vector<utils::SegmentT<DBU>> &linkToPin,
                                                const std::function<void(const utils::BoxT<DBU> &)> &handle) {
    // pin access box
    for (const auto &ab : pinAccessBoxes) {
        if (ab.layerIdx == layerIdx) {
            handle(ab);
        }
    }
    // link to pins
    for (const auto &link : linkToPin) {
        // assme same layer
        handle(PinTapConnector::getLinkMetal(link, layerIdx));
    }
}

const db::ViaType *PostRoute::getViaType(std::shared_ptr<db::GridSteiner> u, std::shared_ptr<db::GridSteiner> v, int pinIdx) {
    const auto &cutLayer = database.getCutLayer(min(u->layerIdx, v->layerIdx));
    db::GridPoint via = (u->layerIdx <= v->layerIdx) ? *u : *v;
    if (pinIdx == -1) {
        return getViaType(via);  // prune by layer?
    }
    auto botDir = database.getLayerDir(via.layerIdx);
    auto topDir = database.getLayerDir(via.layerIdx + 1);
    auto viaLoc = database.getLoc(via);
    static vector<utils::SegmentT<DBU>> emptylinkToPin;
    auto it = linkToPins.find(u);
    const vector<utils::SegmentT<DBU>> &linkToPin = (it != linkToPins.end()) ? it->second : emptylinkToPin;

    // define getViaTypeScore
    auto getViaTypeScore = [&](const db::ViaType &viaType) {
        int viaWireVio = database.getViaUsageOnWires(viaType.viaBotWire, via, dbNet.idx) +
                         database.getViaUsageOnWires(viaType.viaTopWire, database.getUpper(via), dbNet.idx);
        utils::BoxT<DBU> viaRegion = viaType.bot;
        viaRegion.ShiftBy(viaLoc);
        DBU sameNetOvlpArea = 0;
        processAllSameLayerSameNetBoxes(
            dbNet.pinAccessBoxes[pinIdx], u->layerIdx, linkToPin, [&](const utils::BoxT<DBU> &box) {
                auto ovlp = viaRegion.IntersectWith(box);
                if (ovlp.IsValid()) {
                    sameNetOvlpArea += ovlp.area();
                }
            });
        return make_tuple(viaWireVio,
                          database.getViaSpaceVio(via, viaType, dbNet.idx),
                          viaRegion.area() - sameNetOvlpArea,
                          viaType.getDefaultScore(botDir, topDir));
    };

    // select the best via type
    const db::ViaType *bestViaType = &cutLayer.defaultViaType();  // last tie break by preferring defaultViaType
    auto bestScore = getViaTypeScore(cutLayer.defaultViaType());
    for (const auto &viaType : cutLayer.allViaTypes) {
        auto score = getViaTypeScore(viaType);
        if (score < bestScore) {
            bestViaType = &viaType;
            bestScore = score;
        }
    }
    if (std::get<0>(bestScore) > 0) {
        db::routeStat.increment(db::RouteStage::POST, db::MiscRouteEvent::VIA_WIRE_VIO_TAP, 1);
        ++dbNet.viaPinVio;
    }
    if (std::get<1>(bestScore) > 0) {
        db::routeStat.increment(db::RouteStage::POST, db::MiscRouteEvent::VIA_PIN_VIO_TAP, 1);
        ++dbNet.viaPinVio;
    }
    return bestViaType;
}

const db::ViaType *PostRoute::getViaType(const db::GridPoint &via) {
    const auto &cutLayer = database.getCutLayer(via.layerIdx);
    auto botDir = database.getLayerDir(via.layerIdx);
    auto topDir = database.getLayerDir(via.layerIdx + 1);
    auto viaLoc = database.getLoc(via);

    // define getViaTypeScore
    auto getViaTypeScore = [&](const db::ViaType &viaType) {
        return make_tuple(database.getViaSpaceVio(via, viaType, dbNet.idx), viaType.getDefaultScore(botDir, topDir));
    };

    // select the best via type
    const db::ViaType *bestViaType = &cutLayer.defaultViaType();  // last tie break by preferring defaultViaType
    auto bestScore = getViaTypeScore(cutLayer.defaultViaType());
    for (const auto &viaType : cutLayer.allViaTypes) {
        if (std::get<0>(bestScore) == 0) {  // prune
            return bestViaType;
        }
        auto score = getViaTypeScore(viaType);
        if (score < bestScore) {
            bestViaType = &viaType;
            bestScore = score;
        }
    }
    if (std::get<0>(bestScore) != 0) {
        db::routeStat.increment(db::RouteStage::POST, db::MiscRouteEvent::VIA_PIN_VIO_OTHERS, 1);
        ++dbNet.viaPinVio;
    }
    return bestViaType;
}

utils::BoxT<DBU> PostRoute::getWireSegmentMetal(const db::GridEdge &edge) {
    auto tmpEdge = edge;
    if (tmpEdge.u.trackIdx > tmpEdge.v.trackIdx || tmpEdge.u.crossPointIdx > tmpEdge.v.crossPointIdx)
        std::swap(tmpEdge.u, tmpEdge.v);
    utils::BoxT<DBU> box(database.getLoc(tmpEdge.u), database.getLoc(tmpEdge.v));
    DBU halfWidth = database.getLayer(edge.u.layerIdx).width / 2;
    for (int d = 0; d < 2; ++d) {
        box[d].low -= halfWidth;
        box[d].high += halfWidth;
    }
    return box;
}

void MetalFiller::run() {
    for (int i = 0; i < targetMetals.size(); ++i) {
        getFillRect(targetMetals[i]);
    }
    for (int i = 0; i < fillMetals.size(); ++i) {
        getFillRect(fillMetals[i]);
    }
}

void MetalFiller::getFillRect(utils::BoxT<DBU> targetMetal) {
    for (int dir = 0; dir < 2; ++dir) {
        DBU space = database.getLayer(layerIdx).getSpace(targetMetal, dir, aggressiveSpacing);
        auto extendLow = targetMetal;  // left: lower end of dir
        auto extendHigh = targetMetal;
        extendLow[dir] = {targetMetal[dir].low - space, targetMetal[dir].low};
        extendHigh[dir] = {targetMetal[dir].high, targetMetal[dir].high + space};
        vector<utils::BoxT<DBU>> ovlpsLow;
        vector<utils::BoxT<DBU>> ovlpsHigh;
        iterateAllMetals([&](const utils::BoxT<DBU> &metal) {
            auto ovlpLow = extendLow.IntersectWith(metal);
            if (ovlpLow.IsStrictValid()) {
                ovlpsLow.push_back(ovlpLow);
            }
            auto ovlpHigh = extendHigh.IntersectWith(metal);
            if (ovlpHigh.IsStrictValid()) {
                ovlpsHigh.push_back(ovlpHigh);
            }
        });
        if (!ovlpsLow.empty()) {
            utils::SlicePolygons<DBU>(ovlpsLow, dir);
            for (auto &slice : ovlpsLow) {
                if (slice[dir].high < targetMetal[dir].low) {
                    slice[dir] = {slice[dir].high, targetMetal[dir].low};
                    fillMetals.push_back(slice);
                }
            }
        }
        if (!ovlpsHigh.empty()) {
            utils::SlicePolygons<DBU>(ovlpsHigh, dir);
            for (auto &slice : ovlpsHigh) {
                if (slice[dir].low > targetMetal[dir].high) {
                    slice[dir] = {targetMetal[dir].high, slice[dir].low};
                    fillMetals.push_back(slice);
                }
            }
        }
    }
}

void MetalFiller::iterateAllMetals(const std::function<void(const utils::BoxT<DBU> &)> handle) {
    for (const auto &metal : targetMetals) handle(metal);
    for (const auto &metal : fillMetals) handle(metal);
}

void connectBySTT(db::Net &net) {
    using PointOnLayer = std::pair<utils::PointT<DBU>, int>;

    int numPin = net.numOfPins();

    // construct the graph
    vector<PointOnLayer> points(numPin);
    vector<std::pair<DBU, DBU>> coors(numPin);
    vector<DBU> yCoors(numPin);

    for (int pinIdx = 0; pinIdx < numPin; pinIdx++) {
        const auto &boxes = net.pinAccessBoxes[pinIdx];

        utils::PointT<DBU> &point = points[pinIdx].first;
        int &layerIdx = points[pinIdx].second;

        bool found = false;
        for (const auto &box : boxes) {
            db::GridBoxOnLayer gridBox = database.rangeSearch(box);
            if (database.isValid(gridBox)) {
                point = database.getLoc(
                    db::GridPoint(gridBox.layerIdx, gridBox.trackRange.low, gridBox.crossPointRange.low));
                layerIdx = gridBox.layerIdx;
                found = true;
                break;
            }
        }
        if (!found) {
            point =
                utils::PointT<DBU>(boxes[0].x.low + boxes[0].x.range() / 2, boxes[0].y.low + boxes[0].y.range() / 2);
            layerIdx = boxes[0].layerIdx;
        }

        yCoors[pinIdx] = point.y;
        coors[pinIdx] = {point.x, point.y};
    }

    sort(coors.begin(), coors.end());
    sort(yCoors.begin(), yCoors.end());
    DBU mid = yCoors[yCoors.size() / 2];

    // dump to def
    int horLayerIdx = 4;
    int vertLayerIdx = 5;
    if (database.getLayerNum() < 2) {
        return;
    } else if (database.getLayerNum() < 6) {
        horLayerIdx = database.getLayerNum() - 1;
        vertLayerIdx = database.getLayerNum() - 2;
    }
    if (database.getLayerDir(horLayerIdx) != Y) std::swap(horLayerIdx, vertLayerIdx);

    // trunk
    database.writeDEFWireSegment(net, {coors.begin()->first, mid}, {coors.rbegin()->first, mid}, horLayerIdx);

    // branch
    int i, j;
    for (i = 0; i < coors.size(); i++) {
        for (j = i + 1; j < coors.size(); j++) {
            if (coors[j].first != coors[i].first) {
                break;
            }
        }

        vector<int> endPoints;
        if (coors[i].second <= mid) endPoints.push_back(i);
        if (coors[j - 1].second > mid) endPoints.push_back(j - 1);

        for (auto endPoint : endPoints) {
            utils::PointT<DBU> point1(coors[endPoint].first, coors[endPoint].second);
            utils::PointT<DBU> point2(coors[endPoint].first, mid);
            if (point1 != point2) database.writeDEFWireSegment(net, point1, point2, vertLayerIdx);
            database.writeDEFVia(
                net, point2, database.getCutLayer(min(horLayerIdx, vertLayerIdx)).defaultViaType(), vertLayerIdx);
        }

        i = j - 1;
    }

    // pop to branch layer
    for (int pinIdx = 0; pinIdx < numPin; pinIdx++) {
        int layerIdx = points[pinIdx].second;

        int loLayerIdx = min(layerIdx, vertLayerIdx);
        int hiLayerIdx = max(layerIdx, vertLayerIdx);

        for (int i = loLayerIdx; i < hiLayerIdx; i++) {
            database.writeDEFVia(net, points[pinIdx].first, database.getCutLayer(i).defaultViaType(), i);
        }
    }
}
