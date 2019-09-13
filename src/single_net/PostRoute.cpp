#include "PostRoute.h"
#include "PinTapConnector.h"

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
        if (pinTapConnector.bestLinkVia.first != -1) {
            linkViaToPins[tap] = pinTapConnector.bestLinkVia;
        }
    }

    if (!db::isSucc(netStatus)) {
        printWarnMsg(netStatus, dbNet);
    }

    return netStatus;
}

void PostRoute::getViaTypes() {
    db::RouteStatus status = db::RouteStatus::SUCC_NORMAL;
    getPinTapPoints();
    status = connectPins();
    if (!db::isSucc(status)) {
        return;
    }
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
                node->viaType = getViaType(steinerU, steinerV, pinIdx);
            }
        }
    });
}

void PostRoute::getTopo() {
    // 1. from gridTopo
    dbNet.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        if (node->parent) {
            db::GridEdge edge(*node, *(node->parent));
            if (edge.isVia()) {
                const db::GridPoint &via = edge.lowerGridPoint();
                database.writeDEFVia(dbNet, database.getLoc(edge.u), *(node->viaType), edge.u.layerIdx);
            } else if (edge.isTrackSegment() || edge.isWrongWaySegment()) {
                database.writeDEFWireSegment(dbNet, database.getLoc(edge.u), database.getLoc(edge.v), edge.u.layerIdx);
            } else {
                log() << "Warning in " << __func__ << ": invalid edge type. skip." << std::endl;
            }
        }
        if (node->extWireSeg) {
            const auto seg = database.getLoc(*(node->extWireSeg));
            utils::BoxT<DBU> box(seg.first, seg.second);
            int layerIdx = node->extWireSeg->u.layerIdx;
            DBU halfWidth = database.getLayer(layerIdx).width / 2;
            for (int i = 0; i < 2; ++i) {
                box[i].low -= halfWidth;
                box[i].high += halfWidth;
            }
            database.writeDEFFillRect(dbNet, box, layerIdx);
        }
    });

    //  2. fill metal to resolve same-net spacing violations
    vector<std::tuple<int, std::unordered_set<unsigned>, vector<utils::BoxT<DBU>>>> subTreeMetals;
    vector<vector<std::unordered_set<unsigned>>> pinSubTrees(
        dbNet.numOfPins(), vector<std::unordered_set<unsigned>>(database.getLayerNum()));
    // initialize subTreeMetals and pinSubTrees
    std::function<void(std::shared_ptr<db::GridSteiner>, const unsigned)> updateSubTreeMetals = [&](std::shared_ptr<
                                                                                                        db::GridSteiner>
                                                                                                        node,
                                                                                                    const unsigned
                                                                                                        subTreeIdx) {
        if (node->extWireSeg)
            std::get<2>(subTreeMetals[subTreeIdx]).push_back(getWireSegmentMetal(*(node->extWireSeg)));
        const int layerIdx = node->layerIdx;
        if (node->pinIdx >= 0) {
            const unsigned pinIdx = static_cast<unsigned>(node->pinIdx);
            std::get<1>(subTreeMetals[subTreeIdx]).insert(pinIdx);
            pinSubTrees[pinIdx][layerIdx].insert(subTreeIdx);
            const std::unordered_map<std::shared_ptr<db::GridSteiner>, vector<utils::SegmentT<DBU>>>::const_iterator
                &linkIt = linkToPins.find(node);
            if (linkIt != linkToPins.end()) {
                for (const utils::SegmentT<DBU> &link : linkIt->second) {
                    database.writeDEFWireSegment(dbNet, {link.lx(), link.ly()}, {link.hx(), link.hy()}, layerIdx);
                    std::get<2>(subTreeMetals[subTreeIdx]).push_back(PinTapConnector::getLinkMetal(link, layerIdx));
                }
            }
            const std::unordered_map<std::shared_ptr<db::GridSteiner>,
                                     std::pair<int, utils::PointT<DBU>>>::const_iterator &linkViaIt =
                linkViaToPins.find(node);
            if (linkViaIt != linkViaToPins.end()) {
                const int viaLayerIdx = linkViaIt->second.first;
                const utils::PointT<DBU> &viaLoc = linkViaIt->second.second;
                const db::ViaType &bestViaType = database.getBestViaTypeForFixed(viaLoc, viaLayerIdx, dbNet.idx);
                database.writeDEFVia(dbNet, viaLoc, bestViaType, viaLayerIdx);
                const utils::BoxT<DBU> &box = viaLayerIdx == layerIdx ? bestViaType.getShiftedBotMetal(viaLoc)
                                                                      : bestViaType.getShiftedTopMetal(viaLoc);
                std::get<2>(subTreeMetals[subTreeIdx]).push_back(box);
                const int pinLayerIdx = viaLayerIdx == layerIdx ? viaLayerIdx + 1 : viaLayerIdx;
                const utils::BoxT<DBU> &pinBox = viaLayerIdx == layerIdx ? bestViaType.getShiftedTopMetal(viaLoc)
                                                                         : bestViaType.getShiftedBotMetal(viaLoc);
                if (pinSubTrees[pinIdx][pinLayerIdx].empty()) {
                    subTreeMetals.emplace_back(
                        pinLayerIdx, std::unordered_set<unsigned>({pinIdx}), vector<utils::BoxT<DBU>>(1, pinBox));
                    pinSubTrees[pinIdx][pinLayerIdx].insert(subTreeMetals.size() - 1);
                } else {
                    std::get<1>(subTreeMetals[*pinSubTrees[pinIdx][pinLayerIdx].begin()]).insert(pinIdx);
                    std::get<2>(subTreeMetals[*pinSubTrees[pinIdx][pinLayerIdx].begin()]).push_back(pinBox);
                }
            }
        }
        for (auto child : node->children) {
            std::get<2>(subTreeMetals[subTreeIdx]).push_back(getEdgeLayerMetal({*node, *child}, child->viaType));
            if (layerIdx == child->layerIdx)
                updateSubTreeMetals(child, subTreeIdx);
            else {
                subTreeMetals.emplace_back(
                    child->layerIdx,
                    std::unordered_set<unsigned>(),
                    vector<utils::BoxT<DBU>>(1, getEdgeLayerMetal({*child, *node}, child->viaType)));
                updateSubTreeMetals(child, subTreeMetals.size() - 1);
            }
        }
    };
    for (const std::shared_ptr<db::GridSteiner> &tree : dbNet.gridTopo) {
        subTreeMetals.emplace_back(tree->layerIdx, std::unordered_set<unsigned>(), vector<utils::BoxT<DBU>>());
        updateSubTreeMetals(tree, subTreeMetals.size() - 1);
    }
    // merge subTreeMetals according to pinSubTrees
    for (unsigned pinIdx = 0; pinIdx != dbNet.numOfPins(); ++pinIdx) {
        for (unsigned layerIdx = 0; layerIdx != database.getLayerNum(); ++layerIdx) {
            std::unordered_set<unsigned> &pinSubTree = pinSubTrees[pinIdx][layerIdx];
            if (pinSubTree.size() <= 1) continue;
            const unsigned dstSubTreeIdx = *pinSubTree.begin();
            std::for_each(++pinSubTree.begin(), pinSubTree.end(), [&](const unsigned srcSubTreeIdx) {
                for (const unsigned srcPinIdx : std::get<1>(subTreeMetals[srcSubTreeIdx])) {
                    if (srcPinIdx == pinIdx) continue;
                    pinSubTrees[srcPinIdx][layerIdx].erase(srcSubTreeIdx);
                    pinSubTrees[srcPinIdx][layerIdx].insert(dstSubTreeIdx);
                }
                std::get<1>(subTreeMetals[dstSubTreeIdx])
                    .insert(std::get<1>(subTreeMetals[srcSubTreeIdx]).begin(),
                            std::get<1>(subTreeMetals[srcSubTreeIdx]).end());
                std::get<1>(subTreeMetals[srcSubTreeIdx]).clear();
                std::move(std::get<2>(subTreeMetals[srcSubTreeIdx]).begin(),
                          std::get<2>(subTreeMetals[srcSubTreeIdx]).end(),
                          std::back_inserter(std::get<2>(subTreeMetals[dstSubTreeIdx])));
                std::get<2>(subTreeMetals[srcSubTreeIdx]).clear();
            });
            pinSubTree = {dstSubTreeIdx};
        }
    }
    // add pinAccessBoxes to subTreeMetals
    for (unsigned pinIdx = 0; pinIdx != dbNet.numOfPins(); ++pinIdx) {
        for (const db::BoxOnLayer &ab : dbNet.pinAccessBoxes[pinIdx]) {
            for (const unsigned subTreeIdx : pinSubTrees[pinIdx][ab.layerIdx])
                std::get<2>(subTreeMetals[subTreeIdx]).push_back(ab);
        }
    }
    // fill
    for (const std::tuple<int, std::unordered_set<unsigned>, vector<utils::BoxT<DBU>>> &subTreeMetal : subTreeMetals) {
        if (std::get<2>(subTreeMetal).empty()) continue;
        const db::AggrParaRunSpace aggressiveSpacing =
            std::get<1>(subTreeMetal).empty() ? db::AggrParaRunSpace::DEFAULT : db::AggrParaRunSpace::LARGER_WIDTH;
        MetalFiller metalFiller(std::get<2>(subTreeMetal), std::get<0>(subTreeMetal), aggressiveSpacing);
        metalFiller.run();
        for (const utils::BoxT<DBU> &rect : metalFiller.fillMetals)
            database.writeDEFFillRect(dbNet, rect, std::get<0>(subTreeMetal));
        if (!metalFiller.fillMetals.empty()) {
            db::routeStat.increment(db::RouteStage::POST, db::MiscRouteEvent::FILL_SAME_NET_SPACE, 1);
        }
    }

    // 3. corner spacing hack
    dbNet.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        const db::MetalLayer &layer = database.getLayer(node->layerIdx);
        if (!layer.hasCornerSpace() || layer.isWireViaMultiTrack) return;
        for (std::shared_ptr<db::GridSteiner> c : node->children) {
            if (c->pinIdx >= 0 || node->layerIdx != c->layerIdx || c->children.size() != 1) continue;
            std::shared_ptr<db::GridSteiner> cc = c->children[0];
            if (c->layerIdx != cc->layerIdx || node->trackIdx == c->trackIdx && c->trackIdx == cc->trackIdx) continue;
            utils::BoxT<DBU> rect(database.getLoc(*c));
            DBU halfWidth = layer.width / 2;
            if (node->crossPointIdx < c->crossPointIdx || c->crossPointIdx > cc->crossPointIdx) {
                rect[1 - layer.direction].low += halfWidth;
                rect[1 - layer.direction].high += halfWidth + 1;
            } else if (node->crossPointIdx > c->crossPointIdx || c->crossPointIdx < cc->crossPointIdx) {
                rect[1 - layer.direction].low -= halfWidth + 1;
                rect[1 - layer.direction].high -= halfWidth;
            } else {
                continue;
            }
            rect[layer.direction].low -= halfWidth;
            rect[layer.direction].high += halfWidth;
            db::routeStat.increment(db::RouteStage::POST, db::MiscRouteEvent::REMOVE_CORNER, 1);
            database.writeDEFFillRect(dbNet, rect, c->layerIdx);
        }
    });

    // 4. compress memory
    dbNet.defWireSegments.shrink_to_fit();
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

const db::ViaType *PostRoute::getViaType(std::shared_ptr<db::GridSteiner> u,
                                         std::shared_ptr<db::GridSteiner> v,
                                         int pinIdx) {
    const auto &cutLayer = database.getCutLayer(min(u->layerIdx, v->layerIdx));
    db::GridPoint via = (u->layerIdx <= v->layerIdx) ? *u : *v;
    auto botDir = database.getLayerDir(via.layerIdx);
    auto topDir = database.getLayerDir(via.layerIdx + 1);
    auto viaLoc = database.getLoc(via);
    vector<utils::SegmentT<DBU>> emptylinkToPin;
    auto it = linkToPins.find(u);
    const vector<utils::SegmentT<DBU>> &linkToPin = (it != linkToPins.end()) ? it->second : emptylinkToPin;

    // define getViaTypeScore
    auto getViaTypeScore = [&](const db::ViaType &viaType) {
        int estNumVios = database.getViaFixedVio(via, viaType, dbNet.idx);
        DBU outSideArea = 0;
        estNumVios += database.getViaUsageOnWiresPost(via, dbNet.idx, &viaType);
        if (considerViaViaVio) {
            estNumVios += database.getViaUsageOnVias(via, dbNet.idx, &viaType);
        }
        if (pinIdx >= 0) {
            utils::BoxT<DBU> viaRegion = viaType.bot;
            viaRegion.ShiftBy(viaLoc);
            DBU ovlpArea = 0;
            processAllSameLayerSameNetBoxes(
                dbNet.pinAccessBoxes[pinIdx], u->layerIdx, linkToPin, [&](const utils::BoxT<DBU> &box) {
                    auto ovlp = viaRegion.IntersectWith(box);
                    if (ovlp.IsValid()) {
                        ovlpArea += ovlp.area();
                    }
                });
            outSideArea = viaRegion.area() - ovlpArea;
        }
        return make_tuple(estNumVios, !viaType.hasMultiCut, outSideArea, viaType.getDefaultScore(botDir, topDir));
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
        DBU space = database.getLayer(layerIdx).getSpace(targetMetal, dir, aggrSpace);
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
