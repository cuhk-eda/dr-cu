#pragma once

#include "GeoPrimitive.h"

namespace db {

class Track {
public:
    Track(DBU loc, int lowerIdx = -1, int upperIdx = -1) : location(loc), lowerCPIdx(lowerIdx), upperCPIdx(upperIdx) {}
    DBU location;
    int lowerCPIdx;
    int upperCPIdx;

    friend ostream& operator<<(ostream& os, const Track& track);
};

// Cross points are projection of tracks from upper and lower layers
class CrossPoint {
public:
    CrossPoint(DBU loc, int lowerIdx = -1, int upperIdx = -1)
        : location(loc), lowerTrackIdx(lowerIdx), upperTrackIdx(upperIdx) {}
    DBU location;
    int lowerTrackIdx;
    int upperTrackIdx;

    friend ostream& operator<<(ostream& os, const CrossPoint& cp);
};

class SpaceRule {
public:
    SpaceRule(const DBU space, const DBU eolWidth, const DBU eolWithin) : space(space), hasEol(true), eolWidth(eolWidth), eolWithin(eolWithin) {}
    SpaceRule(const DBU space, const DBU eolWidth, const DBU eolWithin, const DBU parSpace, const DBU parWithin) : space(space), hasEol(true), eolWidth(eolWidth), eolWithin(eolWithin), hasPar(true), parSpace(parSpace), parWithin(parWithin) {}

    DBU space = 0;
    bool hasEol = false;
    DBU eolWidth = 0;
    DBU eolWithin = 0;
    bool hasPar = false;
    DBU parSpace = 0;
    DBU parWithin = 0;
};

// note: for operations on GeoPrimitives, all checking is down in LayerList for low level efficiency
class MetalLayer {
public:
    MetalLayer(Rsyn::PhysicalLayer rsynLayer, Rsyn::PhysicalTracks rsynTrack, const DBU libDBU);

    // Basic infomation
    std::string name;
    Dimension direction;  // direction of track dimension
    int idx;              // layerIdx (consistent with Rsyn::xxx::getRelativeIndex())

    // Track (1D)
    DBU pitch = 0;
    vector<Track> tracks;
    int numTracks() const { return tracks.size(); }
    DBU firstTrackLoc() const { return tracks.front().location; }
    DBU lastTrackLoc() const { return tracks.back().location; }
    bool isTrackRangeValid(const utils::IntervalT<int>& trackRange) const;
    utils::IntervalT<int> getUpperCrossPointRange(const utils::IntervalT<int>& trackRange) const;
    utils::IntervalT<int> getLowerCrossPointRange(const utils::IntervalT<int>& trackRange) const;
    // search by location range (result may be invalid/empty)
    utils::IntervalT<int> rangeSearchTrack(const utils::IntervalT<DBU>& locRange, bool includeBound = true) const;

    // CrossPoint (1D)
    vector<CrossPoint> crossPoints;
    int numCrossPoints() const { return crossPoints.size(); }
    DBU firstCrossPointLoc() const { return crossPoints.front().location; }
    DBU lastCrossPointLoc() const { return crossPoints.back().location; }
    bool isCrossPointRangeValid(const utils::IntervalT<int>& crossPointRange) const;
    // base grid cost without congestion penalty
    // edge cost = accCrossPointCost[crossPointRange.high + 1] - accCrossPointCost[crossPointRange.low]
    // cost is directly posed on grids instead of edges (easier to cross layers & handle corners)
    vector<DBU> accCrossPointDistCost;
    void initAccCrossPointDistCost();
    DBU getCrossPointRangeDistCost(const utils::IntervalT<int>& crossPointRange) const;
    DBU getTrackSegmentLen(const db::GridEdge& edge) const;
    DBU getCrossPointRangeDist(const utils::IntervalT<int>& crossPointRange) const;

    // GridPoint (2D) = Track (1D) x CrossPoint (1D)
    int numGridPoints() const { return tracks.size() * crossPoints.size(); }
    bool isValid(const GridPoint& gridPt) const;
    bool isValid(const GridBoxOnLayer& gridBox) const;
    utils::PointT<DBU> getLoc(const GridPoint& grid) const;
    BoxOnLayer getLoc(const GridBoxOnLayer& gridBox) const;
    std::pair<utils::PointT<DBU>, utils::PointT<DBU>> getLoc(const GridEdge& edge) const;
    GridPoint getUpper(const GridPoint& cur) const;
    GridPoint getLower(const GridPoint& cur) const;

    // Design rules
    // width
    DBU width = 0;
    DBU minWidth = 0;
    // minArea
    DBU minArea = 0;
    DBU minLenRaw = 0;
    DBU minLenOneVia = 0;
    DBU minLenTwoVia = 0;
    DBU viaOvlpDist = 0;
    DBU viaLenEqLen = 0;
    DBU viaWidthEqLen = 0;
    bool hasMinLenVio(DBU len) const { return len < getMinLen(); }
    bool hasMinLenVioAcc(DBU len) const { return len < getMinLenAcc(len); }
    DBU getMinLen() const { return minLenRaw; }
    DBU getMinLenAcc(DBU len) const { return len < viaOvlpDist ? minLenOneVia : minLenTwoVia; }
    // parallel spacing
    vector<DBU> parallelWidth{0};
    vector<DBU> parallelLength{0};
    vector<vector<DBU>> parallelWidthSpace{{0}};
    DBU defaultSpace = 0;
    DBU aggressiveSpace = 0;
    DBU getSpace(DBU width) const;
    DBU getSpace(const utils::BoxT<DBU>& targetMetal) const;
    DBU getSpace(const utils::BoxT<DBU>& targetMetal, int dir, bool aggressive) const;
    // eol spacing
    DBU maxEolSpace = 0;
    DBU maxEolWidth = 0;
    DBU maxEolWithin = 0;
    vector<SpaceRule> spaceRules;
    bool isEolActive(const utils::BoxT<DBU>& targetMetal) const {
        return min(targetMetal.x.range(), targetMetal.y.range()) < maxEolWidth;
    }
    DBU getEolSpace(const DBU width) const;
    bool isEolViolation(const DBU space, const DBU width, const DBU within) const;
    //  margin for multi-thread safe
    DBU safeMargin = 0;

    // Via conflict lookup table (true means "available" / no conflict)
    // 1. wire-via conflict (crossPointIdx, trackIdx, crossPointIdx)
    vector<vector<vector<bool>>> wireBotVia;
    vector<vector<vector<bool>>> wireTopVia;
    vector<vector<utils::IntervalT<int>>> wireBotViaRange;
    vector<vector<utils::IntervalT<int>>> wireTopViaRange;
    // 2. wire-wire conflict (crossPointIdx, crossPointIdx)
    vector<utils::IntervalT<int>> wireRange;

    void initWireRange();

    ostream& printBasics(ostream& os) const;
    ostream& printDesignRules(ostream& os) const;
    ostream& printViaOccupancyLUT(ostream& os) const;
    friend ostream& operator<<(ostream& os, const MetalLayer& layer);

private:
    void check() const;
    static void initWireViaRange(const vector<vector<vector<bool>>>& wireVia,
                                 vector<vector<utils::IntervalT<int>>>& wireInr);
};

}  // namespace db
