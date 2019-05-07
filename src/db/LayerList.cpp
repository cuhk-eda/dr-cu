#include "LayerList.h"

namespace db {

void LayerList::init() {
    const Rsyn::Session session;
    Rsyn::PhysicalDesign physicalDesign = static_cast<Rsyn::PhysicalService*>(session.getService("rsyn.physical"))->getPhysicalDesign();
    const DBU libDBU = physicalDesign.getDatabaseUnits(Rsyn::LIBRARY_DBU);

    //  Rsyn::PhysicalLayer (LEF)
    vector<Rsyn::PhysicalLayer> rsynLayers;
    vector<Rsyn::PhysicalLayer> rsynCutLayers;
    for (const Rsyn::PhysicalLayer& rsynLayer : physicalDesign.allPhysicalLayers()) {
        switch (rsynLayer.getType()) {
            case Rsyn::ROUTING:
                rsynLayers.push_back(rsynLayer);
                break;
            case Rsyn::CUT:
                rsynCutLayers.push_back(rsynLayer);
                break;
            default:
                break;
        }
    }
    if (rsynCutLayers.size() + 1 != rsynLayers.size()) {
        log() << "Error in " << __func__ << ": rsynCutLayers.size() is " << rsynCutLayers.size()
              << " , rsynLayers.size() is " << rsynLayers.size() << " , not matched... " << std::endl;
    }

    //  Rsyn::PhysicalVia (LEF)
    vector<vector<Rsyn::PhysicalVia>> rsynVias(rsynCutLayers.size());
    for (const Rsyn::PhysicalVia& rsynVia : physicalDesign.allPhysicalVias()) {
        rsynVias[rsynVia.getCutLayer().getRelativeIndex()].push_back(rsynVia);
    }

    //  Rsyn::PhysicalTracks (DEF)
    vector<Rsyn::PhysicalTracks> rsynTracks(rsynLayers.size());
    for (const Rsyn::PhysicalTracks& rsynTrack : physicalDesign.allPhysicalTracks()) {
        int idx = rsynTrack.allLayers().front().getRelativeIndex();
        if ((rsynTrack.getDirection() == Rsyn::TRACK_HORIZONTAL) ==
            !strcmp(rsynLayers[idx].getLayer()->direction(), "HORIZONTAL")) {
            assert(rsynLayers[idx].getRelativeIndex() == idx);
            rsynTracks[idx] = (rsynTrack);
        }
    }

    // init each MetalLayer
    layers.clear();
    for (unsigned i = 0; i != rsynLayers.size(); ++i) {
        layers.emplace_back(rsynLayers[i], rsynTracks[i], libDBU);
    }

    //  init MetalLayer::CrossPointSet
    initCrossPoints();

    cutLayers.clear();
    for (unsigned i = 0; i != rsynCutLayers.size(); ++i) {
        cutLayers.emplace_back(rsynCutLayers[i], rsynVias[i], layers[i].direction, layers[i + 1].direction, libDBU);
    }

    // via area equivalent length (conservative)
    for (int i = 0, sz = rsynLayers.size(); i < sz; i++) {
        MetalLayer& layer = layers[i];

        auto dir = layer.direction;
        auto width = layer.width;

        auto getEqLen = [&](const utils::BoxT<DBU>& rect, DBU& viaLenEqLen, DBU& viaWidthEqLen) {
            DBU minViaLen = min(abs(rect[1 - dir].low), abs(rect[1 - dir].high));  // conservative
            DBU viaWidth = rect[dir].range();
            DBU viaLen = rect[1 - dir].range();
            viaWidthEqLen = viaLen * max((DBU)0, viaWidth - width) / width;
            viaLenEqLen = max((DBU)0, minViaLen - width / 2);
        };

        DBU topViaLenEqLen = INT_MAX;
        DBU botViaLenEqLen = INT_MAX;
        DBU topViaWidthEqLen = INT_MAX;
        DBU botViaWidthEqLen = INT_MAX;

        if (i != 0) getEqLen(cutLayers[i - 1].defaultViaType().top, botViaLenEqLen, botViaWidthEqLen);
        if (i != sz - 1) getEqLen(cutLayers[i].defaultViaType().bot, topViaLenEqLen, topViaWidthEqLen);

        layer.viaLenEqLen = min(topViaLenEqLen, botViaLenEqLen);
        layer.viaWidthEqLen = min(topViaWidthEqLen, botViaWidthEqLen);

        layer.minLenRaw = layer.minLenRaw - layer.viaWidthEqLen;
        layer.minLenOneVia = layer.minLenRaw - 2 * layer.viaLenEqLen;
        layer.minLenTwoVia = layer.minLenOneVia - layer.viaWidthEqLen;

        if (i == 0) {
            layer.viaOvlpDist = 0;
        } else if (i == sz - 1) {
            layer.viaOvlpDist = 0;
        } else {
            auto intvl1 = cutLayers[i - 1].defaultViaType().top[1 - dir];
            auto intvl2 = cutLayers[i].defaultViaType().bot[1 - dir];
            layer.viaOvlpDist = max(abs(intvl1.high) + abs(intvl2.low), abs(intvl1.low) + abs(intvl2.high));
            layer.viaOvlpDist = min(layer.viaOvlpDist, intvl1.range());
            layer.viaOvlpDist = min(layer.viaOvlpDist, intvl2.range());
        }

        layer.minLenOneVia = max((DBU)0, layer.minLenOneVia);
        layer.minLenTwoVia = max((DBU)0, layer.minLenTwoVia);
        layer.minLenRaw = max((DBU)0, layer.minLenRaw);
    }

    //  init CutLayer::viaAccess
    initViaAccess();
    initViaForbidRegions();
}

bool LayerList::isValid(const GridPoint& gridPt) const {
    return gridPt.layerIdx >= 0 && gridPt.layerIdx < layers.size() && layers[gridPt.layerIdx].isValid(gridPt);
}

bool LayerList::isValid(const GridBoxOnLayer& gridBox) const {
    return gridBox.layerIdx >= 0 && gridBox.layerIdx < layers.size() && layers[gridBox.layerIdx].isValid(gridBox);
}

bool LayerList::isValid(const ViaBox& viaBox) const {
    return isValid(viaBox.lower) && isValid(viaBox.upper) &&                                  // seperately
           getUpper(viaBox.lower) == viaBox.upper && getLower(viaBox.upper) == viaBox.lower;  // consistent
}

BoxOnLayer LayerList::getMetalRectForbidRegion(const BoxOnLayer& metalRect, bool aggressive) const {
    const MetalLayer& layer = layers[metalRect.layerIdx];
    DBU margin[2];  // x, y
    for (int dir = 0; dir < 2; ++dir) {
        margin[dir] = layer.getSpace(metalRect, dir, aggressive);
        margin[dir] += layer.width / 2;
    }
    return {metalRect.layerIdx,
            metalRect.lx() - margin[0],
            metalRect.ly() - margin[1],
            metalRect.hx() + margin[0],
            metalRect.hy() + margin[1]};
}

vector<utils::BoxT<DBU>> LayerList::getAccurateMetalRectForbidRegions(const BoxOnLayer& metalRect) const {
    const MetalLayer& layer = layers[metalRect.layerIdx];
    vector<utils::BoxT<DBU>> results;
    for (int dir = 0; dir < 2; ++dir) {
        const DBU range = metalRect[1 - dir].range();
        if (range < layer.maxEolWidth) {
            for (const SpaceRule& spaceRule : layer.spaceRules) {
                if (spaceRule.hasEol /* && !spaceRule.hasPar */) {
                    utils::BoxT<DBU> region = metalRect;
                    region[1 - dir].low -= spaceRule.eolWithin;
                    region[1 - dir].high += spaceRule.eolWithin;
                    region[dir].low -= spaceRule.space;
                    region[dir].high += spaceRule.space;
                    results.push_back(region);
                } else if (spaceRule.hasPar) {
                    log() << "Warning in " << __func__ << ": Metal Layer " << layer.name
                          << " has ENDOFLINE with PARALLELEDGE... " << std::endl;
                }
            }
        } else if (range == layer.maxEolWidth) {
            //  TODO: Should not be EOL violation
            utils::BoxT<DBU> region = metalRect;
            region[1 - dir].low -= layer.maxEolWithin;
            region[1 - dir].high += layer.maxEolWithin;
            region[dir].low -= layer.maxEolSpace;
            region[dir].high += layer.maxEolSpace;
            results.push_back(region);
        } else {
            DBU space = layer.getSpace(min(metalRect.width(), metalRect.height()));
            utils::BoxT<DBU> region = metalRect;
            region[dir].low -= space;
            region[dir].high += space;
            results.push_back(region);
        }
    }
    return results;
}

void LayerList::expandBox(BoxOnLayer& box, int numPitchToExtend) const {
    DBU margin = layers[box.layerIdx].pitch * numPitchToExtend;
    box.lx() -= margin;
    box.ly() -= margin;
    box.hx() += margin;
    box.hy() += margin;
}

void LayerList::expandBox(BoxOnLayer& box, int numPitchToExtend, int dir) const {
    DBU margin = layers[box.layerIdx].pitch * numPitchToExtend;
    box[dir].low -= margin;
    box[dir].high += margin;
}

utils::IntervalT<int> LayerList::rangeSearchTrack(int layerIdx,
                                                  const utils::IntervalT<DBU>& locRange,
                                                  bool includeBound) const {
    assert(layerIdx >= 0 && layerIdx < layers.size());
    return layers[layerIdx].rangeSearchTrack(locRange, includeBound);
}

utils::IntervalT<int> LayerList::rangeSearchCrossPoint(int layerIdx,
                                                       const utils::IntervalT<DBU>& locRange,
                                                       bool includeBound) const {
    assert(layerIdx >= 0 && layerIdx < layers.size());
    if (layerIdx == 0) {
        return layers[1].rangeSearchTrack(locRange, includeBound);
    } else if (layerIdx == layers.size() - 1) {
        return layers[layerIdx - 1].rangeSearchTrack(locRange, includeBound);
    } else {
        const utils::IntervalT<int>& upperTrack = layers[layerIdx + 1].rangeSearchTrack(locRange, includeBound);
        const utils::IntervalT<int>& lowerTrack = layers[layerIdx - 1].rangeSearchTrack(locRange, includeBound);
        bool upperValid = layers[layerIdx + 1].isTrackRangeValid(upperTrack);
        bool lowerValid = layers[layerIdx - 1].isTrackRangeValid(lowerTrack);
        if (upperValid && lowerValid) {
            // the most typical case
            const utils::IntervalT<int>& fromUpperTrack = layers[layerIdx + 1].getLowerCrossPointRange(upperTrack);
            const utils::IntervalT<int>& fromLowerTrack = layers[layerIdx - 1].getUpperCrossPointRange(lowerTrack);
            return fromUpperTrack.UnionWith(fromLowerTrack);
        } else if (upperValid) {
            return layers[layerIdx + 1].getLowerCrossPointRange(upperTrack);
        } else if (lowerValid) {
            return layers[layerIdx - 1].getUpperCrossPointRange(lowerTrack);
        } else {
            return {0, -1};  // a little bit safer than default {inf, -inf}
        }
    }
}

GridBoxOnLayer LayerList::rangeSearch(const BoxOnLayer& box, bool includeBound) const {
    auto dir = layers[box.layerIdx].direction;
    return GridBoxOnLayer(box.layerIdx,
                          rangeSearchTrack(box.layerIdx, box[dir], includeBound),
                          rangeSearchCrossPoint(box.layerIdx, box[1 - dir], includeBound));
}

utils::PointT<DBU> LayerList::getLoc(const GridPoint& gridPt) const {
    assert(isValid(gridPt));
    return layers[gridPt.layerIdx].getLoc(gridPt);
}

BoxOnLayer LayerList::getLoc(const GridBoxOnLayer& gridBox) const {
    assert(isValid(gridBox));
    return layers[gridBox.layerIdx].getLoc(gridBox);
}

std::pair<utils::PointT<DBU>, utils::PointT<DBU>> LayerList::getLoc(const GridEdge& edge) const {
    assert(edge.isTrackSegment());
    return layers[edge.u.layerIdx].getLoc(edge);
}

GridPoint LayerList::getUpper(const GridPoint& cur) const {
    assert(isValid(cur) && cur.layerIdx < (int)layers.size() - 1);
    return layers[cur.layerIdx].getUpper(cur);
}

GridPoint LayerList::getLower(const GridPoint& cur) const {
    assert(isValid(cur) && cur.layerIdx > 0);
    return layers[cur.layerIdx].getLower(cur);
}

GridBoxOnLayer LayerList::getUpper(const GridBoxOnLayer& cur) const {
    assert(isValid(cur) && cur.layerIdx < (int)layers.size() - 1);
    return GridBoxOnLayer(
        // layer
        cur.layerIdx + 1,
        // track
        layers[cur.layerIdx + 1].rangeSearchTrack(
            {layers[cur.layerIdx].crossPoints[cur.crossPointRange.low].location,
             layers[cur.layerIdx].crossPoints[cur.crossPointRange.high].location}),
        // cross point
        layers[cur.layerIdx].getUpperCrossPointRange(cur.trackRange));
}

GridBoxOnLayer LayerList::getLower(const GridBoxOnLayer& cur) const {
    assert(isValid(cur) && cur.layerIdx > 0);
    return GridBoxOnLayer(
        // layer
        cur.layerIdx - 1,
        // track
        layers[cur.layerIdx - 1].rangeSearchTrack(
            {layers[cur.layerIdx].crossPoints[cur.crossPointRange.low].location,
             layers[cur.layerIdx].crossPoints[cur.crossPointRange.high].location}),
        // cross point
        layers[cur.layerIdx].getLowerCrossPointRange(cur.trackRange));
}

ViaBox LayerList::getViaBoxBetween(const BoxOnLayer& lower, const BoxOnLayer& upper) {
    assert((lower.layerIdx + 1) == upper.layerIdx);
    auto box2d = lower.IntersectWith(upper);
    auto lowerGridBoxTmp = rangeSearch({lower.layerIdx, box2d});
    if (!isValid(lowerGridBoxTmp)) return ViaBox();  // invalid
    auto upperGridBox = getUpper(lowerGridBoxTmp);
    if (!isValid(upperGridBox)) return ViaBox();  // invalid
    auto lowerGridBox = getLower(upperGridBox);
    if (!isValid(lowerGridBox)) return ViaBox();  // invalid
    return ViaBox(lowerGridBox, upperGridBox);
}

bool LayerList::isConnected(const GridBoxOnLayer& lhs, const GridBoxOnLayer& rhs) {
    if (!isValid(lhs) || !isValid(rhs)) {
        return false;
    } else if ((lhs.layerIdx + 1) == rhs.layerIdx) {
        return isValid(getViaBoxBetween(lhs, rhs));
    } else if (lhs.layerIdx == (rhs.layerIdx + 1)) {
        return isValid(getViaBoxBetween(rhs, lhs));
    } else if (lhs.layerIdx == rhs.layerIdx) {
        return lhs.trackRange.HasIntersectWith(rhs.trackRange) &&
               lhs.crossPointRange.HasIntersectWith(rhs.crossPointRange);
    } else {
        return false;
    }
}

bool LayerList::isAdjacent(const GridBoxOnLayer& lhs, const GridBoxOnLayer& rhs) {
    if (!isValid(lhs) || !isValid(rhs)) {
        return false;
    } else if (lhs.layerIdx == rhs.layerIdx) {
        return (abs(lhs.trackRange.low - rhs.trackRange.high) == 1 ||
                abs(rhs.trackRange.low - lhs.trackRange.high) == 1) &&
               lhs.crossPointRange.HasIntersectWith(rhs.crossPointRange);
    } else {
        return false;
    }
}

void LayerList::initCrossPoints() {
    for (unsigned i = 0; i != layers.size(); ++i) {
        vector<CrossPoint>& crossPoints = layers[i].crossPoints;
        vector<Track> emptyTrackSet;
        vector<Track>& lowerTrackSet = (i > 0) ? layers[i - 1].tracks : emptyTrackSet;
        vector<Track>& upperTrackSet = (i < (layers.size() - 1)) ? layers[i + 1].tracks : emptyTrackSet;

        // merge cross points to lower and upper layers
        int iLo = 0, iUp = 0;  // track indexes
        DBU lastBoth = 0;
        while (iLo < lowerTrackSet.size() || iUp < upperTrackSet.size()) {
            if (iUp >= upperTrackSet.size()) {
                crossPoints.emplace_back(lowerTrackSet[iLo].location, iLo, -1);
                lowerTrackSet[iLo].upperCPIdx = crossPoints.size() - 1;
                ++iLo;
            } else if (iLo >= lowerTrackSet.size()) {
                crossPoints.emplace_back(upperTrackSet[iUp].location, -1, iUp);
                upperTrackSet[iUp].lowerCPIdx = crossPoints.size() - 1;
                ++iUp;
            }  // boundaries should be checked first
            else if (lowerTrackSet[iLo].location < upperTrackSet[iUp].location) {
                crossPoints.emplace_back(lowerTrackSet[iLo].location, iLo, -1);
                lowerTrackSet[iLo].upperCPIdx = crossPoints.size() - 1;
                ++iLo;
            } else if (lowerTrackSet[iLo].location > upperTrackSet[iUp].location) {
                crossPoints.emplace_back(upperTrackSet[iUp].location, -1, iUp);
                upperTrackSet[iUp].lowerCPIdx = crossPoints.size() - 1;
                ++iUp;
            } else {  // iLo < lowerTrackSet.size() && iUp < lowerTrackSet.size() && lowerTrackSet[iLo].location ==
                      // lowerTrackSet[iUp].location
                crossPoints.emplace_back(lowerTrackSet[iLo].location, iLo, iUp);
                lowerTrackSet[iLo].upperCPIdx = crossPoints.size() - 1;
                upperTrackSet[iUp].lowerCPIdx = crossPoints.size() - 1;
                ++iLo;
                ++iUp;
            }
        }

        layers[i].initAccCrossPointDistCost();
    }
}

void LayerList::initOppLUT(const vector<vector<vector<bool>>>& ori, vector<vector<vector<bool>>>& opp) {
    const size_t nCPs = ori.size();
    size_t xSize = 0;
    for (const vector<vector<bool>>& orig : ori) {
        xSize = orig.size();
        if (xSize) {
            break;
        }
    }

    auto travelCPs = [&](std::function<void(const unsigned, const unsigned, const unsigned, const unsigned)> handle) {
        for (unsigned i = 0; i != nCPs; ++i) {
            if (ori[i].empty()) {
                continue;
            }
            for (unsigned j = 0; j != xSize; ++j) {
                const int ySize = ((int)ori[i][0].size() - 1) / 2;
                for (unsigned k = max(0, ySize - (int)i); k < min(ySize * 2 + 1, int(nCPs + ySize - i)); ++k) {
                    if (ori[i][j][k]) {
                        handle(i, j, ySize, k);
                    }
                }
            }
        }
    };

    vector<int> ySizes(nCPs, -1);
    auto updateSize = [&](const unsigned i, const unsigned j, const unsigned ySize, const unsigned k) {
        ySizes[i + k - ySize] = max(ySizes[i + k - ySize], abs((int)ySize - (int)k));
    };
    auto fillTable = [&](const unsigned i, const unsigned j, const unsigned ySize, const unsigned k) {
        vector<bool>& tmpOpp = opp[i + k - ySize][xSize - j - 1];
        const int tmpYSize = ((int)tmpOpp.size() - 1) / 2;
        tmpOpp[tmpYSize + ySize - k] = true;
    };

    travelCPs(updateSize);
    opp.clear();
    opp.resize(nCPs);
    for (unsigned i = 0; i != nCPs; ++i) {
        if (ySizes[i] >= 0) {
            opp[i].resize(xSize, vector<bool>(ySizes[i] * 2 + 1, false));
        } else if (ori[i].empty()) {
            opp[i].resize(xSize, vector<bool>(1, false));
        }
        assert(ori[i].size() + opp[i].size());
    }
    travelCPs(fillTable);
}

void LayerList::initViaWire(const int layerIdx,
                            const utils::BoxT<DBU>& viaMetal,
                            vector<vector<vector<bool>>>& viaWireLUT) {
    const MetalLayer& layer = layers[layerIdx];
    const DBU halfWidth = ceil(layer.width / 2.0);
    const DBU viaMetalWidth = viaMetal[layer.direction].range();
    const DBU viaMetalHeight = viaMetal[1 - layer.direction].range();
    const DBU pSpace = layer.getSpace(min(viaMetalWidth, viaMetalHeight));
    const DBU space = max(pSpace, max(layer.maxEolSpace, layer.maxEolWithin));
    const size_t xSize = max(ceil((space + halfWidth + viaMetal[layer.direction].high) / (double)layer.pitch),
                             ceil((space + halfWidth - viaMetal[layer.direction].low) / (double)layer.pitch)) -
                         1;
    const utils::IntervalT<DBU> locRange(-space - halfWidth + viaMetal[1 - layer.direction].low + 1,
                                         +space + halfWidth + viaMetal[1 - layer.direction].high - 1);
    viaWireLUT.resize(layer.numCrossPoints());
    vector<bool> viaTrack(xSize + 1, false);
    for (unsigned i = 0; i != layer.numCrossPoints(); ++i) {
        const CrossPoint& cp = layer.crossPoints[i];
        utils::IntervalT<DBU> tmpLocRange(locRange);
        tmpLocRange.ShiftBy(layer.crossPoints[i].location);
        const utils::IntervalT<int>& cpRange = rangeSearchCrossPoint(layerIdx, tmpLocRange);
        const size_t ySize = max((int)i - cpRange.low, cpRange.high - (int)i);
        viaWireLUT[i].resize(xSize * 2 + 1, vector<bool>(ySize * 2 + 1, false));
        for (unsigned j = 0; j != xSize * 2 + 1; ++j) {
            DBU xDist = 0;
            if (j < xSize) {
                xDist = max(0L, int(xSize - j) * layer.pitch - halfWidth + viaMetal[layer.direction].low);
            } else if (j > xSize) {
                xDist = max(0L, int(j - xSize) * layer.pitch - halfWidth - viaMetal[layer.direction].high);
            }
            for (unsigned k = max(0, (int)ySize - (int)i); k < min(ySize * 2 + 1, layer.numCrossPoints() + ySize - i);
                 ++k) {
                const CrossPoint& tmpCP = layer.crossPoints[i + k - ySize];
                DBU yDist = 0;
                if (k < ySize) {
                    yDist = max(0L, cp.location - tmpCP.location - halfWidth + viaMetal[1 - layer.direction].low);
                } else if (k > ySize) {
                    yDist = max(0L, tmpCP.location - cp.location - halfWidth - viaMetal[1 - layer.direction].high);
                }
                if (xDist < pSpace && !yDist && viaMetalHeight >= layer.maxEolWidth ||
                    yDist < pSpace && !xDist && viaMetalWidth >= layer.maxEolWidth ||
                    layer.isEolViolation(xDist, viaMetalHeight, yDist) ||
                    layer.isEolViolation(yDist, viaMetalWidth, xDist)) {
                    viaTrack[abs((int)j - (int)xSize)] = true;
                    viaWireLUT[i][j][k] = true;
                }
            }
        }
    }
    size_t minXSize = xSize;
    for (; minXSize && !viaTrack[minXSize]; --minXSize) {
    }
    unsigned d = xSize - minXSize;
    if (!d) {
        return;
    }
    for (vector<vector<bool>>& vvb : viaWireLUT) {
        for (unsigned i = 0; i != minXSize * 2 + 1; ++i) {
            vvb[i] = vvb[i + d];
        }
        vvb.resize(minXSize * 2 + 1);
    }
}

void LayerList::initViaAccess() {
    for (unsigned i = 0; i != cutLayers.size(); ++i) {
        CutLayer& cutLayer = cutLayers[i];
        const MetalLayer& botLayer = layers[i];
        const MetalLayer& topLayer = layers[i + 1];
        const Dimension botDim = botLayer.direction;
        const Dimension topDim = topLayer.direction;
        if (botDim == topDim) {
            log() << "Error in " << __func__ << ": botLayer.direction is " << botDim << " , topLayer.direction is "
                  << topDim << " , the same... " << std::endl;
        }
        const utils::BoxT<DBU>& bot = cutLayer.defaultViaType().bot;
        const utils::BoxT<DBU>& cut = cutLayer.defaultViaType().cut;
        const utils::BoxT<DBU>& top = cutLayer.defaultViaType().top;
        const DBU botWidth = bot[botDim].range();
        const DBU cutWidth = cut[botDim].range();
        const DBU topWidth = top[botDim].range();
        const DBU botHeight = bot[topDim].range();
        const DBU cutHeight = cut[topDim].range();
        const DBU topHeight = top[topDim].range();
        //  FIXED: consider non-default spacing
        //  const DBU botDftSpacing = botLayer.defaultSpace;
        const DBU cutSpacing = cutLayer.spacing;
        //  const DBU topDftSpacing = topLayer.defaultSpace;
        const DBU botPSpace = botLayer.getSpace(min(botWidth, botHeight));
        const DBU topPSpace = topLayer.getSpace(min(topWidth, topHeight));
        const DBU botSpace = max(botPSpace, max(botLayer.maxEolSpace, botLayer.maxEolWithin));
        const DBU topSpace = max(topPSpace, max(topLayer.maxEolSpace, topLayer.maxEolWithin));
        const DBU botPitch = botLayer.pitch;
        const DBU topPitch = topLayer.pitch;

        //  1. init viaCut & viaMetal
        const size_t cutXSize = ceil((cutSpacing + cutWidth) / (double)botPitch) - 1;
        //  cout << cutSpacing << '\t' << cutWidth << '\t' << botPitch << std::endl;
        const size_t cutYSize = ceil((cutSpacing + cutHeight) / (double)topPitch) - 1;
        const size_t metalXSize =
            max(cutXSize, (size_t)ceil((max(botSpace + botWidth, topSpace + topWidth)) / (double)botPitch) - 1);
        const size_t metalYSize =
            max(cutYSize, (size_t)ceil((max(botSpace + botHeight, topSpace + topHeight)) / (double)topPitch) - 1);
        cutLayer.viaCut.resize(cutXSize + 1, vector<bool>(cutYSize + 1, false));
        cutLayer.viaMetal.resize(metalXSize + 1, vector<bool>(metalYSize + 1, false));
        vector<bool> viaMetalTrack(metalXSize + 1, false);
        for (unsigned j = 0; j != cutXSize + 1; ++j) {
            const DBU cutXDist = max(0L, botPitch * j - cutWidth);
            for (unsigned k = 0; k != cutYSize + 1; ++k) {
                const DBU cutYDist = max(0L, topPitch * k - cutHeight);
                if (pow(cutXDist, 2) + pow(cutYDist, 2) < pow(cutSpacing, 2)) {
                    cutLayer.viaCut[j][k] = true;
                }
            }
        }

        for (unsigned j = 0; j != metalXSize + 1; ++j) {
            const DBU botXDist = max(0L, botPitch * j - botWidth);
            const DBU topXDist = max(0L, botPitch * j - topWidth);
            for (unsigned k = 0; k != metalYSize + 1; ++k) {
                const DBU botYDist = max(0L, topPitch * k - botHeight);
                const DBU topYDist = max(0L, topPitch * k - topHeight);
                if (j <= cutXSize && k <= cutYSize && cutLayer.viaCut[j][k] ||
                    botXDist < botPSpace && !botYDist && botHeight >= botLayer.maxEolWidth ||
                    botYDist < botPSpace && !botXDist && botWidth >= botLayer.maxEolWidth ||
                    topXDist < topPSpace && !topYDist && topHeight >= topLayer.maxEolWidth ||
                    topYDist < topPSpace && !topXDist && topWidth >= topLayer.maxEolWidth ||
                    botLayer.isEolViolation(botXDist, botHeight, botYDist) ||
                    botLayer.isEolViolation(botYDist, botWidth, botXDist) ||
                    topLayer.isEolViolation(topXDist, topHeight, topYDist) ||
                    topLayer.isEolViolation(topYDist, topWidth, topXDist)) {
                    viaMetalTrack[j] = true;
                    cutLayer.viaMetal[j][k] = true;
                }
            }
        }
        size_t minMetalXSize = metalXSize;
        for (; minMetalXSize && !viaMetalTrack[minMetalXSize]; --minMetalXSize) {
        }
        if (minMetalXSize < metalXSize) {
            cutLayer.viaMetal.resize(minMetalXSize + 1);
        }

        //  2. init viaTopVia & viaBotVia
        const unsigned nBotCPs = botLayer.numCrossPoints();
        if (i > 0) {
            const utils::BoxT<DBU>& tmp = cutLayers[i - 1].defaultViaType().top;
            const size_t xSize = max(ceil((botSpace + bot[botDim].high - tmp[botDim].low) / (double)botPitch),
                                     ceil((botSpace + tmp[botDim].high - bot[botDim].low) / (double)botPitch)) -
                                 1;
            const utils::IntervalT<DBU> botLocRange(-tmp[1 - botDim].high + bot[1 - botDim].low - botSpace + 1,
                                                    +bot[1 - botDim].high - tmp[1 - botDim].low + botSpace - 1);
            vector<vector<vector<bool>>>& viaBotVia = cutLayer.viaBotVia;
            viaBotVia.resize(nBotCPs);
            for (unsigned j = 0; j != nBotCPs; ++j) {
                utils::IntervalT<DBU> tmpLocRange(botLocRange);
                tmpLocRange.ShiftBy(botLayer.crossPoints[j].location);
                const utils::IntervalT<int>& cpRange = rangeSearchCrossPoint(i, tmpLocRange);
                const size_t ySize = max((int)j - cpRange.low, cpRange.high - (int)j);
                if (botLayer.crossPoints[j].upperTrackIdx >= 0) {
                    viaBotVia[j].resize(xSize * 2 + 1, vector<bool>(ySize * 2 + 1, false));
                } else {
                    viaBotVia[j].resize(xSize * 2 + 1, vector<bool>(ySize * 2 + 1, true));
                }
            }
            for (unsigned j = 0; j != nBotCPs; ++j) {
                const CrossPoint& cp = botLayer.crossPoints[j];
                if (cp.upperTrackIdx == -1) {
                    continue;
                }
                const unsigned ySize = (viaBotVia[j][0].size() - 1) / 2;
                for (unsigned k = 0; k != xSize * 2 + 1; ++k) {
                    DBU xDist = 0;
                    if (k < xSize) {
                        xDist = max(0L, int(xSize - k) * botPitch - tmp[botDim].high + bot[botDim].low);
                    } else if (k > xSize) {
                        xDist = max(0L, int(k - xSize) * botPitch - bot[botDim].high + tmp[botDim].low);
                    }
                    for (unsigned l = max(0, (int)ySize - (int)j); l < min(ySize * 2 + 1, nBotCPs + ySize - j); ++l) {
                        const CrossPoint& tmpCP = botLayer.crossPoints[j + l - ySize];
                        if (tmpCP.lowerTrackIdx == -1) {
                            viaBotVia[j][k][l] = true;
                            continue;
                        }
                        DBU yDist = 0;
                        if (l < ySize) {
                            yDist = max(0L, cp.location - tmpCP.location - tmp[1 - botDim].high + bot[1 - botDim].low);
                        } else if (l > ySize) {
                            yDist = max(0L, tmpCP.location - cp.location - bot[1 - botDim].high + tmp[1 - botDim].low);
                        }
                        if (xDist < botPSpace && !yDist && botHeight >= botLayer.maxEolWidth ||
                            yDist < botPSpace && !xDist && botWidth >= botLayer.maxEolWidth ||
                            botLayer.isEolViolation(xDist, botHeight, yDist) ||
                            botLayer.isEolViolation(yDist, botWidth, xDist)) {
                            viaBotVia[j][k][l] = true;
                        }
                    }
                }
            }
            LayerList::initOppLUT(viaBotVia, cutLayers[i - 1].viaTopVia);
        }

        //  3. init viaBotWire & viaTopWire
        initViaWire(i, bot, cutLayer.viaBotWire);
        initViaWire(i + 1, top, cutLayer.viaTopWire);
        for (auto& viaType : cutLayer.allViaTypes) {
            initViaWire(i, viaType.bot, viaType.viaBotWire);
            initViaWire(i + 1, viaType.top, viaType.viaTopWire);
        }

        LayerList::initOppLUT(cutLayer.viaBotWire, layers[i].wireTopVia);
        LayerList::initOppLUT(cutLayer.viaTopWire, layers[i + 1].wireBotVia);
    }
    for (MetalLayer& layer : layers) {
        layer.initWireRange();
    }
}

void LayerList::initViaForbidRegions() {
    for (int i = 0; i < cutLayers.size(); ++i) {
        auto& cutLayer = cutLayers[i];
        cutLayer.botMaxForbidRegion = cutLayer.defaultViaType().bot;
        cutLayer.topMaxForbidRegion = cutLayer.defaultViaType().top;
        for (auto& viaType : cutLayer.allViaTypes) {
            viaType.botForbidRegions = getAccurateMetalRectForbidRegions({i, viaType.bot});
            for (const auto& region : viaType.botForbidRegions) {
                cutLayer.botMaxForbidRegion = cutLayer.botMaxForbidRegion.UnionWith(region);
            }
            viaType.topForbidRegions = getAccurateMetalRectForbidRegions({i + 1, viaType.top});
            for (const auto& region : viaType.topForbidRegions) {
                cutLayer.topMaxForbidRegion = cutLayer.topMaxForbidRegion.UnionWith(region);
            }
        }
    }
}

void LayerList::print() {
    log() << "METAL LAYERS" << std::endl;
    numGridPoints = 0;
    totalTrackLength = 0;
    for (const MetalLayer& layer : layers) {
        log() << layer << std::endl;
        numGridPoints += layer.numGridPoints();
        totalTrackLength += layer.numTracks() * layer.getCrossPointRangeDistCost({0, layer.numCrossPoints() - 1});
    }
    log() << "The total number of grid points is " << numGridPoints << std::endl;
    log() << "The total length of tracks is " << totalTrackLength << " DBU " << std::endl;
    log() << "(Note: dir=Y means that each track is horizontal and has differnt Y)" << std::endl;
    log() << "Metal layer design rules: " << std::endl;
    for (const MetalLayer& layer : layers) {
        layer.printDesignRules(log()) << std::endl;
    }
    log() << "Wire conflict LUT: " << std::endl;
    for (const MetalLayer& layer : layers) {
        layer.printViaOccupancyLUT(log()) << std::endl;
    }

    log() << "CUT LAYERS" << std::endl;
    numVias = 0;
    for (int i = 0; i < getLayerNum() - 1; ++i) {
        int num = layers[i].numTracks() * layers[i + 1].numTracks();
        const auto& cutLayer = cutLayers[i];
        log() << cutLayer << ", #vias=" << num << std::endl;
        numVias += num;
    }
    log() << "Cut layer design rules: " << std::endl;
    for (const CutLayer& layer : cutLayers) {
        layer.printDesignRules(log()) << std::endl;
    }
    log() << "Via conflict LUT: " << std::endl;
    for (const CutLayer& layer : cutLayers) {
        layer.printViaOccupancyLUT(log()) << std::endl;
    }
    //  cout << cutLayers[3].viaCut << std::endl;
    log() << "The total number of via candidates is " << numVias << std::endl;
}

}  // namespace db
