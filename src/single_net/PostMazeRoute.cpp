#include "PostMazeRoute.h"

void PostMazeRoute::removeTrackSwitchWithVio() {
    // 1. Track switch around pin
    // handle root/leaf pin taps only for simplicity (TODO: generalize)
    // TODO: consider via connecting to lower metal layers
    std::vector<std::shared_ptr<db::GridSteiner>> pinTaps[localNet.numOfPins()];
    auto isViaTap = [](std::shared_ptr<db::GridSteiner> node) {
        return node->pinIdx >= 0 &&
               ((node->parent && node->children.empty() && node->parent->layerIdx > node->layerIdx) ||
                (!(node->parent) && node->children.size() == 1 && node->children[0]->layerIdx > node->layerIdx));
    };
    localNet.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
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
                const auto &viaCut = database.getCutLayer(tapI->layerIdx).viaCut;
                int lowerDiff = abs(tapI->trackIdx - tapJ->trackIdx);
                int upperDiff = abs(neighI->trackIdx - neighJ->trackIdx);
                if (lowerDiff < viaCut.size() && upperDiff < viaCut[0].size() && viaCut[lowerDiff][upperDiff]) {
                    db::routeStat.increment(db::RouteStage::MAZE, db::MiscRouteEvent::REMOVE_TRACK_SWITCH_PIN, 1);
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
    localNet.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
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
                        int size = (node->layerIdx < c->layerIdx)
                                       ? database.getCutLayer(node->layerIdx).viaMetal.size()
                                       : database.getCutLayer(c->layerIdx).viaMetal[0].size();
                        if (abs(node->trackIdx - ccc->trackIdx) < size) {
                            db::routeStat.increment(
                                db::RouteStage::MAZE, db::MiscRouteEvent::REMOVE_TRACK_SWITCH_NORMAL, 1);
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
    localNet.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        auto tmpC = node->children;
        for (auto c1 : tmpC) {
            if (node->layerIdx != c1->layerIdx) continue;
            auto tmpCC = c1->children;
            for (auto c2 : tmpC) {
                if (node->layerIdx == c2->layerIdx) continue;  // c1 and c2 is guaranteed to be different by layerIdx
                for (auto cc1 : tmpCC) {
                    if (c2->layerIdx == cc1->layerIdx &&
                        c2->crossPointIdx == cc1->crossPointIdx &&  // redundant constraint for safety
                        node->trackIdx == c1->trackIdx              // redundant constraint for safety
                    ) {
                        // check size first (more efficient)
                        int size = (c2->layerIdx < node->layerIdx)
                                       ? database.getCutLayer(c2->layerIdx).viaMetal.size()
                                       : database.getCutLayer(node->layerIdx).viaMetal[0].size();
                        if (abs(c2->trackIdx - cc1->trackIdx) < size) {
                            // has Steiner or pin: add edge c2-cc1, remove edge c1-cc1
                            db::GridSteiner::resetParent(cc1);
                            db::GridSteiner::setParent(cc1, c2);
                        }
                    }
                }
            }
        }
    });

    // 3. Clean up
    // remove redundant nodes
    localNet.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        auto tmpC = node->children;
        for (auto c : tmpC) {
            if (c->pinIdx < 0 && c->children.size() == 0) {
                db::GridSteiner::resetParent(c);
            }
        }
    });
    // merge wrong-way edges (rarely triggered, but essential for Innovus' connectivity check)
    // node - c - cc
    localNet.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        if (node->pinIdx < 0 && node->children.size() == 1) {
            auto c = node->children[0];
            if (node->layerIdx == c->layerIdx && node->crossPointIdx == c->crossPointIdx && c->pinIdx < 0 &&
                c->children.size() == 1) {
                auto cc = c->children[0];
                if (c->layerIdx == cc->layerIdx && c->crossPointIdx == cc->crossPointIdx) {
                    db::GridSteiner::resetParent(c);
                    db::GridSteiner::resetParent(cc);
                    db::GridSteiner::setParent(cc, node);
                }
            }
        }
    });
}

void PostMazeRoute::extendMinAreaWires() {
    vector<db::GridEdge> candidateEdges;  // edges that may violate min-area constraint

    // 1. Mark wrong-way segments
    // assume wrong-way segments make min area rule satisfied...
    std::unordered_set<std::shared_ptr<db::GridSteiner>> hasWrongWaySeg;
    localNet.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        if (node->parent && db::GridEdge(*node, *(node->parent)).isWrongWaySegment()) {
            hasWrongWaySeg.insert(node);
            hasWrongWaySeg.insert(node->parent);
        }
    });

    // 2. Process Steiner with two track segments
    std::unordered_set<std::shared_ptr<db::GridSteiner>> hasTwoTrackSegs;
    localNet.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        if (hasWrongWaySeg.find(node) == hasWrongWaySeg.end()) {
            vector<std::shared_ptr<db::GridSteiner>> neighs;
            if (node->parent && db::GridEdge(*node, *(node->parent)).isTrackSegment()) {
                neighs.push_back(node->parent);
            }
            for (auto child : node->children) {
                if (db::GridEdge(*node, *child).isTrackSegment()) {
                    neighs.push_back(child);
                    if (neighs.size() >= 2) {
                        break;
                    }
                }
            }
            if (neighs.size() == 2 && hasWrongWaySeg.find(neighs[0]) == hasWrongWaySeg.end() &&
                hasWrongWaySeg.find(neighs[1]) == hasWrongWaySeg.end()) {
                hasTwoTrackSegs.insert(node);
                // skip pin
                if (node->pinIdx < 0 && neighs[0]->pinIdx < 0 && neighs[1]->pinIdx < 0) {
                    candidateEdges.emplace_back(*neighs[0], *neighs[1]);
                }
            }
        }
    });

    // 3. Process Steiner with 1. two vias only or 2. single track segment
    localNet.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        if (node->parent && node->pinIdx < 0 && hasTwoTrackSegs.find(node) == hasTwoTrackSegs.end()) {
            db::GridEdge edge(*node, *(node->parent));
            if (edge.isVia() && node->children.size() == 1 &&
                db::GridEdge(*node, *(node->children[0])).isVia()) {  // both are vias
                candidateEdges.emplace_back(*node, *node);
            } else if (edge.isTrackSegment() && hasWrongWaySeg.find(node) == hasWrongWaySeg.end() &&
                       hasWrongWaySeg.find(node->parent) == hasWrongWaySeg.end() &&
                       (node->parent->pinIdx < 0 && hasTwoTrackSegs.find(node->parent) == hasTwoTrackSegs.end())) {
                candidateEdges.push_back(edge);
            }
        }
    });

    // 4. Check
    localNet.minAreaVio = 0;
    localNet.minAreaShadowedVio = 0;
    for (const auto &edge : candidateEdges) {
        const db::MetalLayer &layer = database.getLayer(edge.u.layerIdx);
        if (layer.hasMinLenVioAcc(layer.getTrackSegmentLen(edge))) {
            vector<db::GridEdge> wirePatches = getExtendWireRects(edge);
            for (const auto &patch : wirePatches) localNet.extendedWireSegment.push_back(patch);
        }
    }
}

vector<db::GridEdge> PostMazeRoute::getExtendWireRects(const db::GridEdge &edge) const {
    // assume input edge has minArea violation
    const int leftCP = edge.u.crossPointIdx < edge.v.crossPointIdx ? edge.u.crossPointIdx : edge.v.crossPointIdx;
    const int rightCP = edge.u.crossPointIdx >= edge.v.crossPointIdx ? edge.u.crossPointIdx : edge.v.crossPointIdx;
    const db::MetalLayer &layer = database.getLayer(edge.u.layerIdx);
    const int trackIdx = edge.u.trackIdx;
    DBU minLen;

    vector<db::GridEdge> wirePatches;

    // get the lower and upper bound cp index to fix minArea violation and get the interval cp usage
    minLen = layer.getMinLen();
    int low = leftCP - 1, high = rightCP + 1;
    while (low >= 0 && minLen > layer.getCrossPointRangeDist({low, rightCP})) low--;
    low = max(0, low);
    while (high < layer.numCrossPoints() && minLen > layer.getCrossPointRangeDist({leftCP, high})) high++;
    high = min(high, layer.numCrossPoints() - 1);
    vector<db::CostT> crossPointVioCost =
        database.getShortWireSegmentVioCost({layer.idx, trackIdx, {low, high}}, localNet.idx, false);

    // no overlapping in the edge itself
    bool withOtherViolations = false;
    for (int i = leftCP; i <= rightCP; i++) {
        if (crossPointVioCost[i - low] > 0) {
            db::routeStat.increment(db::RouteStage::MAZE, +db::MiscRouteEvent::MIN_AREA_SHADOWED_VIO, 1);
            ++localNet.minAreaShadowedVio;
            withOtherViolations = true;
            break;
        }
    }

    bool succ = false;
    int begin = leftCP, end = rightCP;

    if (!succ) {
        minLen = layer.getMinLen();
        // get the most balancing patches
        begin = leftCP;
        end = rightCP;
        if (!withOtherViolations) {
            for (bool moveLeft = false, leftStop = false, rightStop = false;;) {
                if (begin < low || crossPointVioCost[begin - low] > 0) {
                    leftStop = true;
                    begin++;
                }
                if (end > high || crossPointVioCost[end - low] > 0) {
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
        }
        succ = !layer.hasMinLenVio(layer.getCrossPointRangeDist({begin, end}));
    }

    // convert min-area violations to space/short violations for generating history cost
    if ((db::rrrIter + 1) < db::setting.rrrIterLimit && !succ) {
        minLen = layer.getMinLen();
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
        db::routeStat.increment(db::RouteStage::MAZE, +db::MiscRouteEvent::MIN_AREA_VIO, 1);
        ++localNet.minAreaVio;
    }

    // return
    if (succ) {
        if (begin != leftCP)
            wirePatches.emplace_back(db::GridPoint(layer.idx, trackIdx, begin),
                                     db::GridPoint(layer.idx, trackIdx, leftCP));
        if (end != rightCP)
            wirePatches.emplace_back(db::GridPoint(layer.idx, trackIdx, rightCP),
                                     db::GridPoint(layer.idx, trackIdx, end));
    }
    return wirePatches;
}

void PostMazeRoute::run() {
    if (localNet.getName() != "net181936") {
        removeTrackSwitchWithVio();
    }
    extendMinAreaWires();
}
