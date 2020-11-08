#include "PostMazeRoute.h"

void PostMazeRoute::run() {
    removeTrackSwitchWithVio();
    extendMinAreaWires();
}

void PostMazeRoute::run2() {
    net.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        if (node->extWireSeg && database.getEdgeVioCost(*(node->extWireSeg), net.idx, false)) {
            node->extWireSeg.reset();
        }
    });
    extendMinAreaWires();
}

void PostMazeRoute::removeTrackSwitchWithVio() {
    // 1. Track switch around pin
    // handle root/leaf pin taps only for simplicity (TODO: generalize)
    // TODO: consider via connecting to lower metal layers
    std::vector<std::shared_ptr<db::GridSteiner>> pinTaps[net.numOfPins()];
    auto isViaTap = [](std::shared_ptr<db::GridSteiner> node) {
        return node->pinIdx >= 0 &&
               ((node->parent && node->children.empty() && node->parent->layerIdx > node->layerIdx) ||
                (!(node->parent) && node->children.size() == 1 && node->children[0]->layerIdx > node->layerIdx));
    };
    net.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        if (node->pinIdx >= 0) {
            pinTaps[node->pinIdx].push_back(node);
        }
    });
    for (auto &taps : pinTaps) {
        if (taps.size() == 1) continue;
        for (int i = 0; i < taps.size(); ++i) {
            auto tapI = taps[i];
            if (!(tapI && isViaTap(tapI))) continue;  // double check in case in-loop change
            for (int j = i + 1; j < taps.size(); ++j) {
                auto tapJ = taps[j];
                if (!(tapJ && isViaTap(tapJ))) continue;
                if (tapI->layerIdx != tapJ->layerIdx) continue;
                if (tapI->parent && tapJ->parent) continue;  // should be topologically impossible
                int iBackup = i;
                if (tapI->parent) {
                    swap(tapI, tapJ);  // make sure that tapI is a root node
                    iBackup = j;
                }
                auto neighI = tapI->parent ? tapI->parent : tapI->children[0];
                auto neighJ = tapJ->parent ? tapJ->parent : tapJ->children[0];
                const auto &viaCut = database.getCutLayer(tapI->layerIdx).viaCut();
                const int lowerDiff = abs(tapI->trackIdx - tapJ->trackIdx);
                const int upperDiff = abs(neighI->trackIdx - neighJ->trackIdx);
                const unsigned xSize = viaCut.size() / 2;
                const unsigned ySize = viaCut[0].size() / 2;
                if (lowerDiff <= xSize && upperDiff <= ySize && viaCut[lowerDiff + xSize][upperDiff + ySize]) {
                    db::routeStat.increment(db::RouteStage::POST_MAZE, db::MiscRouteEvent::REMOVE_TRACK_SWITCH_PIN, 1);
                    // remove tapI, graft neighI to neighJ
                    db::GridSteiner::resetParent(neighI);
                    if (tapI->trackIdx == tapJ->trackIdx || tapI->crossPointIdx == tapJ->crossPointIdx) {
                        db::GridSteiner::setParent(neighI, neighJ);
                    } else {
                        auto turn = std::make_shared<db::GridSteiner>(
                            db::GridPoint(neighI->layerIdx, neighI->trackIdx, neighJ->crossPointIdx));
                        db::GridSteiner::setParent(neighI, turn);
                        db::GridSteiner::setParent(turn, neighJ);
                    }
                    taps[iBackup].reset();
                }
            }
        }
    }

    // 2. Normal track switch
    // node - c - cc - ccc
    net.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        auto tmpC = node->children;
        for (auto c : tmpC) {
            if (node->layerIdx == c->layerIdx) continue;
            auto tmpCC = c->children;
            for (auto cc : tmpCC) {
                if (c->layerIdx != cc->layerIdx) continue;
                auto tmpCCC = cc->children;
                for (auto ccc : tmpCCC) {
                    if (node->layerIdx == ccc->layerIdx &&
                        node->crossPointIdx == ccc->crossPointIdx &&  // redundant constraint for safety
                        c->trackIdx == cc->trackIdx                   // redundant constraint for safety
                    ) {
                        // check size first (more efficient)
                        const int size = (node->layerIdx < c->layerIdx)
                                       ? database.getCutLayer(node->layerIdx).viaMetal().size() / 2
                                       : database.getCutLayer(c->layerIdx).viaMetal()[0].size() / 2;
                        if (abs(node->trackIdx - ccc->trackIdx) <= size) {
                            db::routeStat.increment(
                                db::RouteStage::POST_MAZE, db::MiscRouteEvent::REMOVE_TRACK_SWITCH_NORMAL, 1);
                            if (c->children.size() > 1 || c->pinIdx >= 0 || cc->children.size() > 1 ||
                                cc->pinIdx >= 0) {
                                // has Steiner or pin: add edge node-ccc, remove edge cc-ccc
                                db::GridSteiner::resetParent(ccc);
                                db::GridSteiner::setParent(ccc, node);
                            } else {
                                // directly remove: add edge node-ccc, remove edges node-c-cc-ccc
                                db::GridSteiner::resetParent(c);
                                db::GridSteiner::resetParent(cc);
                                db::GridSteiner::resetParent(ccc);
                                db::GridSteiner::setParent(ccc, node);
                            }
                        }
                    }
                }
            }
        }
    });
    // c2 - node - c1 - cc1
    net.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        vector<std::shared_ptr<db::GridSteiner>> tmpC = node->children;
        for (std::shared_ptr<db::GridSteiner> c1 : tmpC) {
            if (node->layerIdx != c1->layerIdx) continue;
            vector<std::shared_ptr<db::GridSteiner>> tmpCC = c1->children;
            for (std::shared_ptr<db::GridSteiner> c2 : tmpC) {
                if (node->layerIdx == c2->layerIdx) continue;  // c1 and c2 is guaranteed to be different by layerIdx
                for (std::shared_ptr<db::GridSteiner> cc1 : tmpCC) {
                    if (c2->layerIdx != cc1->layerIdx ||
                        c2->crossPointIdx != cc1->crossPointIdx ||  // redundant constraint for safety
                        node->trackIdx != c1->trackIdx              // redundant constraint for safety
                    ) {
                        continue;
                    }
                    // check size first (more efficient)
                    const unsigned size = (c2->layerIdx < node->layerIdx)
                                              ? database.getCutLayer(c2->layerIdx).viaMetal().size() / 2
                                              : database.getCutLayer(node->layerIdx).viaMetal()[0].size() / 2;
                    if (static_cast<unsigned>(abs(c2->trackIdx - cc1->trackIdx)) <= size) {
                        // has Steiner or pin: add edge c2-cc1, remove edge c1-cc1
                        db::GridSteiner::resetParent(cc1);
                        db::GridSteiner::setParent(cc1, c2);
                    }
                }
            }
        }
    });

    // 3. Horseshoe track switch
    // node - c - s - cc - ccc
    net.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        vector<std::shared_ptr<db::GridSteiner>> tmpC = node->children;
        for (std::shared_ptr<db::GridSteiner> c : tmpC) {
            const vector<std::shared_ptr<db::GridSteiner>> tmpCC = c->children;
            if (node->layerIdx != c->layerIdx || node->trackIdx != c->trackIdx || tmpCC.empty()) {
                //  FIXME: should be `continue`
                return;
            }
            std::shared_ptr<db::GridSteiner> s = nullptr;
            if (c->pinIdx >= 0 || tmpCC.size() > 1) s = c;
            for (std::shared_ptr<db::GridSteiner> cc : tmpCC) {
                if (c->layerIdx != cc->layerIdx || c->crossPointIdx != cc->crossPointIdx) continue;
                const int nodeDCP = node->crossPointIdx - c->crossPointIdx;
                int ccDCP = 0;
                std::shared_ptr<db::GridSteiner> ccc = nullptr;
                while (true) {
                    const vector<std::shared_ptr<db::GridSteiner>> &tmpCCC = cc->children;
                    if (tmpCCC.empty()) break;
                    if (cc->pinIdx >= 0 || tmpCCC.size() > 1) {
                        if (s) break;
                        s = cc;
                    }
                    for (std::shared_ptr<db::GridSteiner> tccc : tmpCCC) {
                        if (cc->layerIdx != tccc->layerIdx) continue;
                        if (cc->crossPointIdx == tccc->crossPointIdx) {
                            ccc = tccc;
                            continue;
                        }
                        ccDCP = cc->crossPointIdx - tccc->crossPointIdx;
                        if (cc->trackIdx != tccc->trackIdx || nodeDCP * ccDCP > 0) continue;
                        ccc = tccc;
                        break;
                    }
                    if (!ccc || cc->crossPointIdx != ccc->crossPointIdx) break;
                    if (s != cc) {
                        db::GridSteiner::resetParent(cc);
                        db::GridSteiner::resetParent(ccc);
                        db::GridSteiner::setParent(ccc, c);
                    }
                    cc = ccc;
                    ccc = nullptr;
                }
                if (!ccc) continue;
                const int crossPointIdx = abs(nodeDCP) <= abs(ccDCP) ? node->crossPointIdx : ccc->crossPointIdx;
                if (database.getWrongWayWireSegmentVioCost(
                        {node->layerIdx,
                         {std::min(node->trackIdx, ccc->trackIdx) + 1, std::max(node->trackIdx, ccc->trackIdx) - 1},
                         crossPointIdx},
                        net.idx,
                        false) ||
                    s && s != c && s != cc &&
                        database.getWireSegmentVioCost(
                            {s->layerIdx,
                             s->trackIdx,
                             {std::min(crossPointIdx, s->crossPointIdx), std::max(crossPointIdx, s->crossPointIdx)}},
                            net.idx,
                            false)) {
                    //  FIXME: should be `continue`
                    return;
                }
                db::routeStat.increment(
                    db::RouteStage::POST_MAZE, db::MiscRouteEvent::REMOVE_TRACK_SWITCH_HORSESHOE, 1);
                db::GridSteiner::resetParent(c);
                db::GridSteiner::resetParent(cc);
                db::GridSteiner::resetParent(ccc);
                std::shared_ptr<db::GridSteiner> nn = nullptr;
                std::shared_ptr<db::GridSteiner> nc = nullptr;
                if (abs(nodeDCP) > abs(ccDCP)) {
                    nn =
                        std::make_shared<db::GridSteiner>(db::GridPoint(node->layerIdx, node->trackIdx, crossPointIdx));
                    db::GridSteiner::setParent(nn, node);
                } else {
                    nn = node;
                }
                if (abs(nodeDCP) < abs(ccDCP)) {
                    nc = std::make_shared<db::GridSteiner>(db::GridPoint(ccc->layerIdx, ccc->trackIdx, crossPointIdx));
                    db::GridSteiner::setParent(ccc, nc);
                } else {
                    nc = ccc;
                }
                if (s == c) {
                    db::GridSteiner::setParent(nc, nn);
                    db::GridSteiner::setParent(s, nn);
                } else if (s == cc) {
                    db::GridSteiner::setParent(nc, nn);
                    db::GridSteiner::setParent(s, nc);
                } else if (s) {
                    db::GridSteiner::resetParent(s);
                    std::shared_ptr<db::GridSteiner> ns =
                        std::make_shared<db::GridSteiner>(db::GridPoint(s->layerIdx, s->trackIdx, crossPointIdx));
                    db::GridSteiner::setParent(ns, nn);
                    db::GridSteiner::setParent(nc, ns);
                    db::GridSteiner::setParent(s, ns);
                } else {
                    db::GridSteiner::setParent(nc, nn);
                }
            }
        }
    });
    // cc1 - c1 - node - c2 - cc2
    net.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        vector<std::shared_ptr<db::GridSteiner>> tmpC = node->children;
        //  { children connected to children of children with higher cross point indices,
        //    children connected to children of children with lower cross point indices }
        vector<vector<std::shared_ptr<db::GridSteiner>>> cs(2);
        //  { children of children with higher cross point indices,
        //    children of children with lower cross point indices }
        vector<vector<std::shared_ptr<db::GridSteiner>>> ccs(2);
        // construct cs and ccs
        for (std::shared_ptr<db::GridSteiner> c : tmpC) {
            if (node->layerIdx != c->layerIdx) continue;
            std::shared_ptr<db::GridSteiner> cc = nullptr;
            if (node->crossPointIdx == c->crossPointIdx) {
                while (c->pinIdx < 0 && c->children.size() == 1) {
                    std::shared_ptr<db::GridSteiner> tcc = c->children[0];
                    if (c->layerIdx != tcc->layerIdx) break;
                    if (c->crossPointIdx == tcc->crossPointIdx) {
                        db::GridSteiner::resetParent(c);
                        db::GridSteiner::resetParent(tcc);
                        db::GridSteiner::setParent(tcc, node);
                        c = tcc;
                    } else {
                        cc = tcc;
                        break;
                    }
                }
                if (!cc || c->trackIdx != cc->trackIdx) continue;
            } else if (node->trackIdx == c->trackIdx) {
                cc = c;
                c = node;
            } else {
                continue;
            }
            if (c->crossPointIdx < cc->crossPointIdx) {
                cs[0].push_back(c);
                ccs[0].push_back(cc);
            } else {
                cs[1].push_back(c);
                ccs[1].push_back(cc);
            }
        }
        //  remove extra cs
        for (unsigned i = 0; i != 2; ++i) {
            if (cs[i].size() < 2) continue;
            int minTrackIdx = INT_MAX;
            int maxTrackIdx = INT_MIN;
            int steinerCrossPointIdx = i ? INT_MIN : INT_MAX;
            int trunkCrossPointIdx = i ? INT_MIN : INT_MAX;
            const unsigned ncs = cs[i].size();
            std::shared_ptr<db::GridSteiner> nn = nullptr;
            for (unsigned j = 0; j != ncs; ++j) {
                minTrackIdx = std::min(minTrackIdx, ccs[i][j]->trackIdx);
                maxTrackIdx = std::max(maxTrackIdx, ccs[i][j]->trackIdx);
                if (i) {
                    steinerCrossPointIdx = std::max(steinerCrossPointIdx, ccs[i][j]->crossPointIdx);
                    if (cs[i][j] == node && ccs[i][j]->crossPointIdx > trunkCrossPointIdx) {
                        nn = ccs[i][j];
                        trunkCrossPointIdx = ccs[i][j]->crossPointIdx;
                    }
                } else {
                    steinerCrossPointIdx = std::min(steinerCrossPointIdx, ccs[i][j]->crossPointIdx);
                    if (cs[i][j] == node && ccs[i][j]->crossPointIdx < trunkCrossPointIdx) {
                        nn = ccs[i][j];
                        trunkCrossPointIdx = ccs[i][j]->crossPointIdx;
                    }
                }
            }
            if (database.getWrongWayWireSegmentVioCost(
                    {node->layerIdx, {minTrackIdx + 1, maxTrackIdx - 1}, steinerCrossPointIdx}, net.idx, false) ||
                trunkCrossPointIdx != INT_MIN && trunkCrossPointIdx != INT_MAX &&
                    database.getWireSegmentVioCost({node->layerIdx,
                                                    node->trackIdx,
                                                    {std::min(steinerCrossPointIdx, node->crossPointIdx),
                                                     std::max(steinerCrossPointIdx, node->crossPointIdx)}},
                                                   net.idx,
                                                   false)) {
                return;
            }
            db::routeStat.increment(db::RouteStage::POST_MAZE, db::MiscRouteEvent::REMOVE_TRACK_SWITCH_HORSESHOE, 1);
            if (steinerCrossPointIdx != trunkCrossPointIdx) {
                nn = std::make_shared<db::GridSteiner>(
                    db::GridPoint(node->layerIdx, node->trackIdx, steinerCrossPointIdx));
                db::GridSteiner::setParent(nn, node);
            }
            for (unsigned j = 0; j != ncs; ++j) {
                if (cs[i][j] != node) {
                    db::GridSteiner::resetParent(cs[i][j]);
                }
                if (ccs[i][j] == nn) continue;
                db::GridSteiner::resetParent(ccs[i][j]);
                if (cs[i][j] == node || ccs[i][j]->crossPointIdx == steinerCrossPointIdx)
                    db::GridSteiner::setParent(ccs[i][j], nn);
                else {
                    std::shared_ptr<db::GridSteiner> nc = std::make_shared<db::GridSteiner>(
                        db::GridPoint(nn->layerIdx, ccs[i][j]->trackIdx, steinerCrossPointIdx));
                    db::GridSteiner::setParent(ccs[i][j], nc);
                    db::GridSteiner::setParent(nc, nn);
                }
            }
        }
    });

    // 4. Clean up
    // remove redundant nodes
    net.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        auto tmpC = node->children;
        for (auto c : tmpC) {
            if (c->pinIdx < 0 && c->children.size() == 0) {
                db::GridSteiner::resetParent(c);
            }
        }
    });
    // merge wrong-way edges (rarely triggered, but essential for Innovus' connectivity check)
    // node - c - cc
    net.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        if (node->pinIdx >= 0 || node->children.size() != 1) return;
        auto c = node->children[0];
        if (node->layerIdx != c->layerIdx || node->crossPointIdx != c->crossPointIdx || c->pinIdx >= 0 ||
            c->children.size() != 1) {
            return;
        }
        auto cc = c->children[0];
        if (c->layerIdx == cc->layerIdx && c->crossPointIdx == cc->crossPointIdx) {
            db::GridSteiner::resetParent(c);
            db::GridSteiner::resetParent(cc);
            db::GridSteiner::setParent(cc, node);
        }
    });
}

void PostMazeRoute::extendMinAreaWires() {
    // edges that may violate min-area constraint
    std::vector<std::vector<std::shared_ptr<db::GridSteiner>>> candidateEdges;

    // 1. Mark wrong-way segments
    // assume wrong-way segments make min area rule satisfied...
    std::unordered_set<std::shared_ptr<db::GridSteiner>> hasWrongWaySeg;
    net.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        if (node->parent && db::GridEdge(*node, *(node->parent)).isWrongWaySegment()) {
            hasWrongWaySeg.insert(node);
            hasWrongWaySeg.insert(node->parent);
        }
    });

    // 2. Process track segments (may be pieced)
    net.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        if (node->isRealPin() || hasWrongWaySeg.find(node) != hasWrongWaySeg.end()) return;
        if (node->parent && db::GridEdge(*node, *(node->parent)).isTrackSegment()) return;
        vector<std::shared_ptr<db::GridSteiner>> descendants;
        for (auto c : node->children) {
            if (db::GridEdge(*node, *c).isTrackSegment()) {
                if (c->isRealPin() || hasWrongWaySeg.find(c) != hasWrongWaySeg.end()) return;
                // extend track segment along node -> c -> cc
                bool extended = true;
                while (extended) {
                    extended = false;
                    for (auto cc : c->children) {
                        if (db::GridEdge(*cc, *c).isTrackSegment()) {
                            if (cc->isRealPin() || hasWrongWaySeg.find(cc) != hasWrongWaySeg.end()) return;
                            extended = true;
                            c = cc;
                            break;
                        }
                    }
                }
                descendants.push_back(c);
                if (descendants.size() > 2) break;  // at most two
            }
        }
        if (descendants.size() == 1) {
            // track segment node - neigh[0]
            candidateEdges.push_back({node, descendants[0]});
        } else if (descendants.size() == 2) {
            // track segment neigh[0] - node - neigh[1]
            candidateEdges.push_back({descendants[0], descendants[1]});
        }
    });

    // 3. Process vias (1. two vias only, 2. single via with fake pin)
    net.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        if (!(node->parent)) return;
        db::GridEdge edge(*node, *(node->parent));
        if (edge.isVia()) {
            if (!node->isRealPin()) {
                if (node->children.size() == 1 &&
                    db::GridEdge(*node, *(node->children[0])).isVia()) {  // case 1: both are vias
                    candidateEdges.push_back({node});
                } else if (node->children.empty()) {  // case 2.1: node end by a via (should be a fakePin)
                    candidateEdges.push_back({node});
                }
            }
            if (!node->parent->isRealPin() && !(node->parent->parent) &&
                node->parent->children.size() == 1) {  // case 2.2: parent end by a via (should be a fakePin)
                candidateEdges.push_back({node->parent});
            }
        }
    });

    // 4. Check
    for (const auto &candEdge : candidateEdges) {
        bool handled = false;
        for (const auto &gridSteiner : candEdge) {
            if (gridSteiner->extWireSeg) {
                handled = true;
                break;
            }
        }
        if (!handled) getExtendWireRects(candEdge);
    }
}

void PostMazeRoute::getExtendWireRects(const std::vector<std::shared_ptr<db::GridSteiner>> &candEdge) const {
    if (candEdge.size() < 1 || candEdge.size() > 2) {
        printlog("Error: invalid candEdge size in PostMazeRoute::getExtendWireRects()");
        return;
    }
    int leftCP, rightCP;
    if (candEdge.size() == 1) {
        leftCP = rightCP = candEdge[0]->crossPointIdx;
    } else {
        leftCP = std::min(candEdge[0]->crossPointIdx, candEdge[1]->crossPointIdx);
        rightCP = std::max(candEdge[0]->crossPointIdx, candEdge[1]->crossPointIdx);
    }
    const int trackIdx = candEdge[0]->trackIdx;
    const db::MetalLayer &layer = database.getLayer(candEdge[0]->layerIdx);
    if (!layer.hasMinLenVioAcc(layer.getCrossPointRangeDist({leftCP, rightCP}))) return;

    // get the lower and upper bound cp index to fix minArea violation and get the interval cp usage
    DBU minLen = layer.getMinLen();
    int low = leftCP - 1, high = rightCP + 1;
    while (low >= 0 && minLen > layer.getCrossPointRangeDist({low, rightCP})) low--;
    low = max(0, low);
    while (high < layer.numCrossPoints() && minLen > layer.getCrossPointRangeDist({leftCP, high})) high++;
    high = min(high, layer.numCrossPoints() - 1);
    vector<db::CostT> crossPointVioCostWOHist =
        database.getShortWireSegmentVioCost({layer.idx, trackIdx, {low, high}}, net.idx, false);

    // with other violations in the candEdge itself?
    bool withOtherViolations = false;
    for (int i = leftCP; i <= rightCP; i++) {
        if (crossPointVioCostWOHist[i - low] > 0) {
            db::routeStat.increment(db::RouteStage::POST_MAZE, +db::MiscRouteEvent::MIN_AREA_SHADOWED_VIO, 1);
            withOtherViolations = true;
            break;
        }
    }

    // get the most balancing patches
    int begin = leftCP, end = rightCP;
    bool succ = false;
    if (!withOtherViolations) {
        auto extendRange = [&](const vector<db::CostT> &crossPointVioCost) {
            begin = leftCP;
            end = rightCP;
            for (bool moveLeft = false, leftStop = false, rightStop = false;;) {
                if (begin < low || crossPointVioCost[begin - low] > 0) {
                    leftStop = true;
                    if (begin < leftCP) {
                        ++begin;
                    }
                }
                if (end > high || crossPointVioCost[end - low] > 0) {
                    rightStop = true;
                    if (end > rightCP) {
                        --end;
                    }
                }
                if ((leftStop && rightStop) || layer.getCrossPointRangeDist({begin, end}) >= minLen) {
                    break;
                }

                moveLeft = (!moveLeft || rightStop) && !leftStop;

                if (moveLeft) {
                    begin--;
                } else {
                    end++;
                }
            }
            succ = !layer.hasMinLenVio(layer.getCrossPointRangeDist({begin, end}));
        };
        vector<db::CostT> crossPointVioCostWHist =
            database.getShortWireSegmentVioCost({layer.idx, trackIdx, {low, high}}, net.idx, true);
        extendRange(crossPointVioCostWHist);
        if (!succ) {
            extendRange(crossPointVioCostWOHist);
        }
    }

    // convert min-area violations to space/short violations for generating history cost
    if (db::rrrIterSetting.converMinAreaToOtherVio && !succ) {
        for (bool moveLeft = false, leftStop = false, rightStop = false;;) {
            if (begin < low) {
                leftStop = true;
                begin++;
            }
            if (end > high) {
                rightStop = true;
                end--;
            }
            if ((leftStop && rightStop) || layer.getCrossPointRangeDist({begin, end}) >= minLen) break;

            moveLeft = (!moveLeft || rightStop) && !leftStop;

            if (moveLeft) {
                begin--;
            } else {
                end++;
            }
        }
        succ = !layer.hasMinLenVio(layer.getCrossPointRangeDist({begin, end}));
    }

    if (!succ && !withOtherViolations) {
        db::routeStat.increment(db::RouteStage::POST_MAZE, +db::MiscRouteEvent::MIN_AREA_VIO, 1);
    }

    // return
    vector<db::GridEdge> wirePatches;
    if (succ) {
        if (candEdge.size() == 1) {
            // a long wire ext
            candEdge[0]->extWireSeg = std::make_unique<db::GridEdge>(db::GridPoint(layer.idx, trackIdx, begin),
                                                                     db::GridPoint(layer.idx, trackIdx, end));
        } else {
            // one or two short wire ext
            for (const auto &gridSteiner : candEdge) {
                if (gridSteiner->crossPointIdx == leftCP && begin != leftCP) {
                    gridSteiner->extWireSeg = std::make_unique<db::GridEdge>(
                        db::GridPoint(layer.idx, trackIdx, begin), db::GridPoint(layer.idx, trackIdx, leftCP));
                } else if (gridSteiner->crossPointIdx == rightCP && end != rightCP) {
                    gridSteiner->extWireSeg = std::make_unique<db::GridEdge>(
                        db::GridPoint(layer.idx, trackIdx, rightCP), db::GridPoint(layer.idx, trackIdx, end));
                }
            }
        }
    }
}
