#include "MetalLayer.h"

namespace db {

ostream& operator<<(ostream& os, const Track& track) {
    os << "track(lo=" << track.lowerCPIdx << ", up=" << track.upperCPIdx << ", loc=" << track.location << ")";
    return os;
}

ostream& operator<<(ostream& os, const CrossPoint& cp) {
    os << "crossPt(lo=" << cp.lowerTrackIdx << ", up=" << cp.upperTrackIdx << ", loc=" << cp.location << ")";
    return os;
}

MetalLayer::MetalLayer(Rsyn::PhysicalLayer rsynLayer, Rsyn::PhysicalTracks rsynTrack, const DBU libDBU) {
    // Rsyn::PhysicalLayer (LEF)
    lefiLayer* layer = rsynLayer.getLayer();
    name = layer->name();
    direction = !strcmp(layer->direction(), "HORIZONTAL") ? Y : X;
    idx = rsynLayer.getRelativeIndex();
    width = static_cast<DBU>(std::round(layer->width() * libDBU));
    minWidth = static_cast<DBU>(std::round(layer->minwidth() * libDBU));
    minArea = static_cast<DBU>(std::round(layer->area() * libDBU * libDBU));
    minLenRaw = minArea / width - width;
    // default spacing
    const int numSpaceTable = layer->numSpacingTable();
    if (!numSpaceTable) {
        log() << "Warning in " << __func__ << ": For " << name << ", no run spacing table..."
              << std::endl;
    } else {
        for (int iSpaceTable = 0; iSpaceTable < numSpaceTable; ++iSpaceTable) {
            if (!layer->spacingTable(iSpaceTable)->isParallel()) {
                continue;
            }

            const lefiParallel* parallel = layer->spacingTable(iSpaceTable)->parallel();
            const int numLength = parallel->numLength();
            if (numLength > 0) {
                parallelLength.resize(numLength);
                for (unsigned iLength = 0; iLength != (unsigned)numLength; ++iLength) {
                    parallelLength[iLength] = static_cast<DBU>(std::round(parallel->length(iLength) * libDBU));
                }
            }
            const int numWidth = parallel->numWidth();
            if (numWidth > 0) {
                parallelWidth.resize(numWidth);
                parallelWidthSpace.resize(numWidth);
                for (unsigned iWidth = 0; iWidth != (unsigned)numWidth; ++iWidth) {
                    parallelWidth[iWidth] = static_cast<DBU>(std::round(parallel->width(iWidth) * libDBU));
                    parallelWidthSpace[iWidth].resize(std::max(1, numLength), 0);
                    for (int iLength = 0; iLength < numLength; ++iLength) {
                        parallelWidthSpace[iWidth][iLength] = static_cast<DBU>(std::round(parallel->widthSpacing(iWidth, iLength) * libDBU));
                    }
                }
                defaultSpace = getSpace(width);
                aggressiveSpace = (parallelWidthSpace.size() > 1) ? parallelWidthSpace[1][0] : defaultSpace;
            }
        }
    }
    // eol spacing
    if (!layer->hasSpacingNumber()) {
        log() << "Warning in " << __func__ << ": For " << name << ", no spacing rules..." << std::endl;
    } else {
        const int numSpace = layer->numSpacing();
        spaceRules.reserve(numSpace);
        for (int iSpace = 0; iSpace < numSpace; ++iSpace) {
            const DBU space = static_cast<DBU>(std::round(layer->spacing(iSpace) * libDBU));
            const DBU eolWidth = static_cast<DBU>(std::round(layer->spacingEolWidth(iSpace) * libDBU));
            const DBU eolWithin = static_cast<DBU>(std::round(layer->spacingEolWithin(iSpace) * libDBU));
            const DBU parSpace = static_cast<DBU>(std::round(layer->spacingParSpace(iSpace) * libDBU));
            const DBU parWithin = static_cast<DBU>(std::round(layer->spacingParWithin(iSpace) * libDBU));
            if (layer->hasSpacingParellelEdge(iSpace)) {
                spaceRules.emplace_back(space, eolWidth, eolWithin, parSpace, parWithin);
                maxEolSpace = std::max(maxEolSpace, space);
                maxEolWidth = std::max(maxEolWidth, eolWidth);
                maxEolWithin = std::max(maxEolWithin, eolWithin);
            } else if (layer->hasSpacingEndOfLine(iSpace)) {
                spaceRules.emplace_back(space, eolWidth, eolWithin);
                maxEolSpace = std::max(maxEolSpace, space);
                maxEolWidth = std::max(maxEolWidth, eolWidth);
                maxEolWithin = std::max(maxEolWithin, eolWithin);
            } else if (space != defaultSpace) {
                log() << "Warning in " << __func__ << ": For " << rsynLayer.getName()
                      << ", mismatched defaultSpace & spacingTable... " << std::endl;
            }
        }
        if (spaceRules.empty()) {
            log() << "Warning in " << __func__ << ": For " << name << ", no eol spacing rules..."
                  << std::endl;
        }
    }
    delete rsynLayer.getLayer();

    // Rsyn::PhysicalTracks (DEF)
    // note: crossPoints will be initialized in LayerList
    pitch = rsynTrack.getSpace();
    DBU location = rsynTrack.getLocation();
    for (int i = 0; i < rsynTrack.getNumberOfTracks(); ++i) {
        tracks.emplace_back(location);
        location += pitch;
    }

    // safe margin
    // TODO: add via margin
    safeMargin = ceil(((minArea / width) + width) * 1.0 / pitch) * pitch * 2;

    // Check consistency between LEF and DEF
    check();
}

bool MetalLayer::isTrackRangeValid(const utils::IntervalT<int>& trackRange) const {
    return trackRange.low >= 0 && trackRange.high < tracks.size() && trackRange.IsValid();
}

utils::IntervalT<int> MetalLayer::getUpperCrossPointRange(const utils::IntervalT<int>& trackRange) const {
    return {tracks[trackRange.low].upperCPIdx, tracks[trackRange.high].upperCPIdx};
}

utils::IntervalT<int> MetalLayer::getLowerCrossPointRange(const utils::IntervalT<int>& trackRange) const {
    return {tracks[trackRange.low].lowerCPIdx, tracks[trackRange.high].lowerCPIdx};
}

utils::IntervalT<int> MetalLayer::rangeSearchTrack(const utils::IntervalT<DBU>& locRange, bool includeBound) const {
    auto locRangeCopy = locRange;
    // invalid range (low >= high) will still be invalid
    if (locRangeCopy.low < firstTrackLoc()) {
        locRangeCopy.low = firstTrackLoc();
    }
    if (locRangeCopy.high > lastTrackLoc()) {
        locRangeCopy.high = lastTrackLoc();
    }

    utils::IntervalT<int> res{ceil(double(locRangeCopy.low - firstTrackLoc()) / double(pitch)),
                              floor(double(locRangeCopy.high - firstTrackLoc()) / double(pitch))};

    if (!includeBound) {
        if (tracks[res.high].location == locRange.high) --res.high;
        if (tracks[res.low].location == locRange.low) ++res.low;
    }

    return res;
}

bool MetalLayer::isCrossPointRangeValid(const utils::IntervalT<int>& crossPointRange) const {
    return crossPointRange.low >= 0 && crossPointRange.high < crossPoints.size() && crossPointRange.IsValid();
}

void MetalLayer::initAccCrossPointDistCost() {
    accCrossPointDistCost.resize(numCrossPoints() + 1);
    accCrossPointDistCost[0] = 0;
    // For four uniform-distributed crossPoints with dist 1:
    // accCrossPointCost = {0, 0.5, 1.5, 2.5, 3}
    for (int cpIdx = 0; cpIdx < numCrossPoints(); ++cpIdx) {
        DBU delta = 0;
        if ((cpIdx + 1) < numCrossPoints()) {
            delta = (crossPoints[cpIdx + 1].location - crossPoints[cpIdx].location) / 2;
        }
        accCrossPointDistCost[cpIdx + 1] = crossPoints[cpIdx].location - crossPoints[0].location + delta;
    }
}

DBU MetalLayer::getCrossPointRangeDistCost(const utils::IntervalT<int>& crossPointRange) const {
    assert(crossPointRange.IsValid());
    return accCrossPointDistCost[crossPointRange.high + 1] - accCrossPointDistCost[crossPointRange.low];
}

DBU MetalLayer::getTrackSegmentLen(const db::GridEdge& edge) const {
    assert(edge.isTrackSegment());

    utils::IntervalT<int> crossPointRange;
    if (edge.u.crossPointIdx < edge.v.crossPointIdx) {
        crossPointRange.Set(edge.u.crossPointIdx, edge.v.crossPointIdx);
    } else {
        crossPointRange.Set(edge.v.crossPointIdx, edge.u.crossPointIdx);
    }

    return getCrossPointRangeDist(crossPointRange);
}

DBU MetalLayer::getCrossPointRangeDist(const utils::IntervalT<int>& crossPointRange) const {
    return crossPoints[crossPointRange.high].location - crossPoints[crossPointRange.low].location;
}

utils::PointT<DBU> MetalLayer::getLoc(const GridPoint& grid) const {
    utils::PointT<DBU> loc;
    loc[direction] = tracks[grid.trackIdx].location;
    loc[1 - direction] = crossPoints[grid.crossPointIdx].location;
    return loc;
}

std::pair<utils::PointT<DBU>, utils::PointT<DBU>> MetalLayer::getLoc(const GridEdge& edge) const {
    utils::PointT<DBU> loc1 = getLoc(edge.u);
    utils::PointT<DBU> loc2 = getLoc(edge.v);

    if (loc1.x == loc2.x) {
        if (loc1.y < loc2.y)
            return {loc1, loc2};
        else
            return {loc2, loc1};
    } else {
        if (loc1.x < loc2.x)
            return {loc1, loc2};
        else
            return {loc2, loc1};
    }
}

BoxOnLayer MetalLayer::getLoc(const GridBoxOnLayer& gridBox) const {
    BoxOnLayer box;
    box.layerIdx = gridBox.layerIdx;
    box[direction].Set(tracks[gridBox.trackRange.low].location, tracks[gridBox.trackRange.high].location);
    box[1 - direction].Set(crossPoints[gridBox.crossPointRange.low].location,
                           crossPoints[gridBox.crossPointRange.high].location);
    return box;
}

GridPoint MetalLayer::getUpper(const GridPoint& cur) const {
    return {cur.layerIdx + 1, crossPoints[cur.crossPointIdx].upperTrackIdx, tracks[cur.trackIdx].upperCPIdx};
}

GridPoint MetalLayer::getLower(const GridPoint& cur) const {
    return {cur.layerIdx - 1, crossPoints[cur.crossPointIdx].lowerTrackIdx, tracks[cur.trackIdx].lowerCPIdx};
}

bool MetalLayer::isValid(const GridPoint& gridPt) const {
    return gridPt.trackIdx >= 0 && gridPt.trackIdx < tracks.size() &&               // track
           gridPt.crossPointIdx >= 0 && gridPt.crossPointIdx < crossPoints.size();  // cross point
}

bool MetalLayer::isValid(const GridBoxOnLayer& gridBox) const {
    return isTrackRangeValid(gridBox.trackRange) && isCrossPointRangeValid(gridBox.crossPointRange);
}

DBU MetalLayer::getSpace(DBU width) const {
    DBU space = 0;
    int iWidth = parallelWidth.size() - 1;  // first smaller than or equal to
    while (iWidth >= 0 && parallelWidth[iWidth] > width) {
        --iWidth;
    }
    if (iWidth < 0) {
        return 0;  // shouldn't happen
    }
    return parallelWidthSpace[iWidth][0];
}

DBU MetalLayer::getSpace(const utils::BoxT<DBU>& targetMetal) const {
    return getSpace(min(targetMetal.x.range(), targetMetal.y.range()));
}

DBU MetalLayer::getSpace(const utils::BoxT<DBU>& targetMetal, int dir, bool aggressive) const {
    const DBU range = targetMetal[1 - dir].range();
    DBU space = getEolSpace(range);
    if (!space) {
        if (range == maxEolWidth) {
            //  TODO: Should not be EOL violation
            space = maxEolSpace;
        } else {
            space = getSpace(min(targetMetal.width(), targetMetal.height()));
        }
    }
    // do not know the width of neighbor, so aggressiveSpace
    if (aggressive && aggressiveSpace > space) {
        space = aggressiveSpace;
    }
    return space;
}

DBU MetalLayer::getEolSpace(const DBU width) const {
    if (width >= maxEolWidth) {
        return 0;
    }

    DBU space = 0;
    for (const SpaceRule& spaceRule : spaceRules) {
        if (spaceRule.hasEol /* && !spaceRule.hasPar */ && width < spaceRule.eolWidth) {
            space = max(space, spaceRule.space);
        }
    }
    return space;
}

bool MetalLayer::isEolViolation(const DBU space, const DBU width, const DBU within) const {
    for (const SpaceRule& spaceRule : spaceRules) {
        if (spaceRule.hasEol /* && !spaceRule.hasPar */ && space < spaceRule.space && width < spaceRule.eolWidth && within < spaceRule.eolWithin) {
            return true;
        }
    }
    return false;
}

void MetalLayer::initWireViaRange(const vector<vector<vector<bool>>>& wireVia,
                                  vector<vector<utils::IntervalT<int>>>& wireInr) {
    wireInr.clear();
    const unsigned nCPs = wireVia.size();
    wireInr.resize(nCPs);
    for (unsigned i = 0; i != nCPs; ++i) {
        const unsigned xSize = wireVia[i].size();
        wireInr[i].resize(xSize);
        for (unsigned j = 0; j != xSize; ++j) {
            const int ySize = ((int)wireVia[i][j].size() - 1) / 2;
            wireInr[i][j].Set(ySize + 1, -ySize - 1);  // for ease of later use
            for (unsigned k = 0; k != ySize * 2 + 1; ++k) {
                if (wireVia[i][j][k]) {
                    wireInr[i][j].Update((int)k - (int)ySize);
                    break;
                }
            }
            for (int k = ySize * 2; k != -1; --k) {
                if (wireVia[i][j][k]) {
                    wireInr[i][j].Update(k - ySize);
                    break;
                }
            }
        }
    }
}

void MetalLayer::initWireRange() {
    // 1. wireBotViaRange / wireTopViaRange
    if (wireBotVia.size()) {
        MetalLayer::initWireViaRange(wireBotVia, wireBotViaRange);
    }
    if (wireTopVia.size()) {
        MetalLayer::initWireViaRange(wireTopVia, wireTopViaRange);
    }
    // 2. wireRange
    const DBU eolSpace = getEolSpace(width);
    // consider two half width
    const DBU wireEndPointSpace = eolSpace ? eolSpace + width : defaultSpace + width;
    wireRange.resize(numCrossPoints(), {0, 0});
    int i, j;
    for (int cpIdx = 0; cpIdx < numCrossPoints(); ++cpIdx) {
        i = 0;
        while (cpIdx + i >= 0 && crossPoints[cpIdx].location - crossPoints[cpIdx + i].location < wireEndPointSpace) {
            --i;
        }
        j = 0;
        while (cpIdx + j < numCrossPoints() &&
               crossPoints[cpIdx + j].location - crossPoints[cpIdx].location < wireEndPointSpace) {
            ++j;
        }
        wireRange[cpIdx] = {i + 1, j - 1};
    }
}

ostream& MetalLayer::printBasics(ostream& os) const {
    os << name << ": dir=" << getDimension(direction) << ", idx=" << idx;
    os << ", tracks=(locs=" << firstTrackLoc() << "-" << lastTrackLoc() << ",pitch=" << pitch << ",#=" << numTracks()
       << ")";
    os << ", crossPts=(locs=" << firstCrossPointLoc() << "-" << lastCrossPointLoc() << ",#=" << numCrossPoints() << ")";
    os << ", #grids=" << numGridPoints() << ")";
    return os;
}

ostream& MetalLayer::printDesignRules(ostream& os) const {
    os << name << ": width=" << width << ", paraSpace=(default=" << defaultSpace;
    for (int i = 0; i < parallelWidth.size(); ++i) {
        os << ", " << parallelWidth[i] << ":" << parallelWidthSpace[i];
    }
    os << "), eolSpace=(";
    for (const SpaceRule& spaceRule : spaceRules) {
        if (spaceRule.hasEol /* && !spaceRule.hasPar */) {
            os << spaceRule.eolWidth << ':' << spaceRule.space << ", ";
        }
    }
    os << "), minArea=" << minArea;
    return os;
}

ostream& MetalLayer::printViaOccupancyLUT(ostream& os) const {
    auto getMaxSize2d =
        [](const vector<vector<utils::IntervalT<int>>>& ranges, size_t& xSize, utils::IntervalT<int>& yRange) {
            xSize = 0;
            yRange = {0, 0};
            for (const auto& rangeCP : ranges) {
                xSize = max(xSize, rangeCP.size());
                for (const auto& rangeTrack : rangeCP) {
                    yRange = yRange.UnionWith(rangeTrack);
                }
            }
        };
    auto getMaxSize1d = [](const vector<utils::IntervalT<int>>& ranges, utils::IntervalT<int>& yRange) {
        yRange = {0, 0};
        for (const auto& rangeCP : ranges) {
            yRange = yRange.UnionWith(rangeCP);
        }
    };
    size_t xSize;
    utils::IntervalT<int> yRange;
    getMaxSize1d(wireRange, yRange);
    os << name << ": wire(" << wireRange.size() << ',' << yRange << ')';
    getMaxSize2d(wireBotViaRange, xSize, yRange);
    os << ", wireBotVia(" << wireBotViaRange.size() << ',' << xSize << ',' << yRange << ')';
    getMaxSize2d(wireTopViaRange, xSize, yRange);
    os << ", wireTopVia(" << wireTopViaRange.size() << ',' << xSize << ',' << yRange << ')';
    return os;
}

ostream& operator<<(ostream& os, const MetalLayer& layer) { return layer.printBasics(os); }

void MetalLayer::check() const {
    if (width < minWidth) {
        log() << "Warning: In layer " << name << ", width = " << width << " < minWidth = " << minWidth << std::endl;
    }
    if (width > maxEolWidth) {
        log() << "Warning: In layer " << name << ", width = " << width << " > maxEolWidth = " << maxEolWidth << std::endl;
    }
    if (width + defaultSpace > pitch) {
        log() << "Warning: In layer " << name << ", width + defaultSpace =" << width + defaultSpace
              << " > picth = " << pitch << std::endl;
    }
}

}  // namespace db
