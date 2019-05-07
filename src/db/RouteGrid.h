#pragma once

#include "LayerList.h"
#include "Net.h"
#include "Setting.h"

namespace db {

class ViaData;

using CostT = double;
using HistUsageT = double;

struct mutex_wrapper : std::mutex {
    mutex_wrapper() = default;
    mutex_wrapper(mutex_wrapper const&) noexcept : std::mutex() {}
    bool operator==(mutex_wrapper const& other) noexcept { return this == &other; }
};

class PoorWire {
public:
    int netIdx;  // -1 stands for "poor to all" (-2 is invalid)

    PoorWire() : netIdx(-2) {}
    PoorWire(int netIndex) : netIdx(netIndex) {}
    PoorWire& operator+=(const PoorWire& rhs) {
        if (netIdx != rhs.netIdx) {
            netIdx = -1;
        }
        return *this;
    }
};

inline bool operator==(const PoorWire& lhs, const PoorWire& rhs) { return lhs.netIdx == rhs.netIdx; }

class RouteGrid : public LayerList {
public:
    using ViaMapT = vector<vector<std::multimap<int, int>>>;
    using boostPoint = bg::model::point<DBU, 2, bg::cs::cartesian>;
    using boostBox = bg::model::box<boostPoint>;
    using RTrees = vector<bgi::rtree<std::pair<boostBox, int>, bgi::quadratic<16>>>;

    void init();
    void setUnitVioCost(double discount = 1.0);

    // Query fixed metals
    vector<std::pair<utils::BoxT<DBU>, int>> getOvlpBoxes(const BoxOnLayer& box, int netIdx, const RTrees& rtree) const;
    vector<std::pair<utils::BoxT<DBU>, int>> getOvlpBoxes(const BoxOnLayer& box, const RTrees& rtree) const;
    vector<std::pair<utils::BoxT<DBU>, int>> getOvlpFixedMetals(const BoxOnLayer& box, int netIdx) const;
    vector<std::pair<utils::BoxT<DBU>, int>> getOvlpFixedMetals(const BoxOnLayer& box) const;
    vector<std::pair<utils::BoxT<DBU>, int>> getOvlpFixedMetalForbidRegions(const BoxOnLayer& box, int netIdx) const;
    vector<std::pair<utils::BoxT<DBU>, int>> getOvlpFixedMetalForbidRegions(const BoxOnLayer& box) const;
    vector<std::pair<utils::BoxT<DBU>, int>> getOvlpC2CMetals(const BoxOnLayer& box, int netIdx) const;
    vector<std::pair<utils::BoxT<DBU>, int>> getOvlpC2CMetals(const BoxOnLayer& box) const;
    DBU getOvlpArea(const BoxOnLayer& box, int netIdx, const RTrees& rtree) const;
    DBU getOvlpFixedMetalArea(const BoxOnLayer& box, int netIdx) const;
    DBU getOvlpFixedMetalForbidRegionArea(const BoxOnLayer& box, int netIdx) const;

    // Get unit cost
    inline CostT getUnitMinAreaVioCost() const { return unitMinAreaVioCost; }
    inline CostT getUnitSpaceVioCost() const { return unitSpaceVioCost; }

    // Get edge cost
    CostT getEdgeCost(const GridEdge& edge, const int netIdx) const;
    CostT getEdgeVioCost(const GridEdge& edge, const int netIdx, bool histCost = true) const;
    // 1. via
    CostT getViaCost(const GridPoint& via, const int netIdx) const { return unitViaCost + getViaVioCost(via, netIdx); }
    CostT getViaVioCost(const GridPoint& via, const int netIdx, bool histCost = true) const;
    std::pair<unsigned, unsigned> getViaUsage(const GridPoint& via, const int netIdx) const;
    // 1.1 via on via(s)
    unsigned getViaUsageOnSameLayerVias(const GridPoint& via, const int netIdx) const;
    unsigned getViaUsageOnBotLayerVias(const GridPoint& via, const int netIdx) const;
    unsigned getViaUsageOnTopLayerVias(const GridPoint& via, const int netIdx) const;
    unsigned getViaUsageOnVia(const GridPoint& via, const int netIdx) const;
    HistUsageT getViaUsageOnHistVia(const GridPoint& via) const;
    // 1.2 via on wire
    unsigned getViaUsageOnWires(const vector<vector<vector<bool>>>& viaWire,
                                const GridPoint& gp,
                                const int netIdx) const;
    unsigned getViaUsageOnWires(const vector<GridPoint>& gps, const int netIdx) const;
    // 1.3 poor via
    enum class ViaPoorness { Poor, Nondefault, Good };
    ViaPoorness getViaPoorness(const GridPoint& via, int netIdx) const;
    // DBU getViaSpaceVio(const GridPoint& via, const ViaType& viaType, int netIdx) const;
    int getViaSpaceVio(const GridPoint& via, const ViaType& viaType, int netIdx) const;
    void initPoorViaMap();
    ViaData* getViaData(const GridPoint& via) const;

    // 2. wire
    CostT getWireSegmentCost(const TrackSegment& ts, const int netIdx) const;
    CostT getWireSegmentVioCost(const TrackSegment& ts, const int netIdx, bool histCost = true) const;
    CostT getWrongWayWireSegmentVioCost(const WrongWaySegment& wws, const int netIdx, bool histCost) const;
    vector<CostT> getShortWireSegmentCost(const TrackSegment& ts, int netIdx) const;
    vector<CostT> getShortWireSegmentVioCost(const TrackSegment& ts, int netIdx, bool histCost = true) const;
    // TODO: consider history cost in getEmptyIntvl?
    vector<utils::IntervalT<int>> getEmptyIntvl(const TrackSegment& ts, int netIdx) const;
    // 2.1 wire on wires
    vector<int> getShortWireSegmentUsageOnOvlpWire(const TrackSegment& ts, int netIdx) const;
    vector<int> getShortWireSegmentUsageOnOvlpPoorWire(const TrackSegment& ts, int netIdx) const;
    vector<HistUsageT> getShortWireSegmentUsageOnOvlpHistWire(const TrackSegment& ts) const;
    // handle: (interval, usage)
    void iterateWireSegments(const TrackSegment& ts,
                             int netIdx,
                             const std::function<void(const utils::IntervalT<int>&, int)>& handle) const;
    void iteratePoorWireSegments(const TrackSegment& ts,
                                 int netIdx,
                                 const std::function<void(const utils::IntervalT<int>&)>& handle) const;
    void iterateHistWireSegments(const TrackSegment& ts,
                                 const std::function<void(const utils::IntervalT<int>&, HistUsageT)>& handle) const;
    // return crossPoint indexes
    vector<int> getWireSegmentSpaceVioOnWires(const TrackSegment& ts, int netIdx) const;
    vector<int> getShortWireSegmentSpaceVioOnWires(const TrackSegment& ts, int netIdx) const;
    // 2.2 wire on vias
    // return crossPoint indexes (may be out of the range of TrackSegment)
    vector<int> getWireSegmentUsageOnVias(const TrackSegment& ts, int netIdx) const;
    void getWireSegmentUsageOnVias(const TrackSegment& ts,
                                   int netIdx,
                                   const vector<vector<vector<bool>>>& wireVia,
                                   const vector<vector<vector<bool>>>& viaWire,
                                   const ViaMapT& routedViaMap,
                                   vector<int>& viaCPs) const;

    // Use edge
    void useEdge(const GridEdge& edge, int netIdx);
    void useVia(const GridPoint& via, int netIdx, ViaMapT& routedViaMap);
    void useVia(const GridPoint& via, int netIdx);
    void useWireSegment(const TrackSegment& ts, int netIdx);
    void useWrongWayWireSegment(const WrongWaySegment& wws, int netIdx);
    void usePoorWireSegment(const TrackSegment& ts, int netIdx);
    void useHistWireSegment(const TrackSegment& ts, HistUsageT usage);
    void markFixedMetal(int fixedObjectIdx);
    void markFixedMetalBatch();

    // Remove edge
    void removeEdge(const GridEdge& edge, int netIdx);
    void removeVia(const GridPoint& via, int netIdx, ViaMapT& routedViaMap);
    void removeVia(const GridPoint& via, int netIdx);
    void removeWireSegment(const TrackSegment& ts, int netIdx);
    void removeWrongWayWireSegment(const WrongWaySegment& wws, int netIdx);

    // Print stat
    // TODO: seperate print stat into a class
    void printAllUsageAndVio() const;
    // usage
    void getAllWireUsage(const vector<int>& buckets, vector<int>& wireUsageGrid, vector<DBU>& wireUsageLength) const;
    void getAllViaUsage(const vector<int>& buckets, const ViaMapT& routedViaMap, vector<int>& viaUsage) const;
    std::pair<double, double> printAllUsage() const;
    std::string getRangeStr(const vector<int>& buckets, int i) const;
    // violations
    void getAllWireShortVio(vector<int>& shortNum, vector<DBU>& shortLen) const;
    void getAllPoorWire(vector<int>& num, vector<DBU>& len) const;
    void getAllWireSpaceVio(vector<int>& spaceVioNum) const;
    void getAllViaVio(vector<int>& sameLayerViaVios,
                      vector<int>& viaTopViaVios,
                      vector<int>& viaBotWireVios,
                      vector<int>& viaTopWireVios,
                      vector<int>& poorVia) const;
    std::pair<double, double> printAllVio() const;

    // for ripup and reroute
    void addHistCost();
    void addWireHistCost();
    void addViaHistCost();
    void fadeHistCost();
    CostT getNetVioCost(const Net& net) const;

protected:
    // Fixed metals (e.g., cell pins, blockage)
    vector<std::pair<BoxOnLayer, int>> fixedMetalVec;
    RTrees fixedMetals;
    RTrees fixedMetalForbidRegions;
    RTrees fixedMetalC2CForbidRegions;

    // Wire segments
    // 1. routed wires
    // (layerIdx, trackIdx) -> all (crossPointRange, netIdxs)
    // TODO: change set to unordered_set or vector or list
    vector<vector<boost::icl::interval_map<int, std::set<int>>>> routedWireMap;
    vector<vector<mutex_wrapper>> wireLocks;
    // 2. poor wires due to violations with pin/obs
    // (layerIdx, trackIdx) -> all (crossPointRange, netIdx)
    vector<vector<boost::icl::interval_map<int, PoorWire>>> poorWireMap;
    // 3. wires with history violations
    // (layerIdx, trackIdx) -> all (crossPointRange, discountedUsage)
    vector<vector<boost::icl::interval_map<int, HistUsageT>>> histWireMap;

    // Vias
    // (layerIdx, trackIdx) -> all (crossPointIdx, netIdx)
    // (layerIdx, trackIdx, crossPointIdx) -> all netIdx
    ViaMapT routedViaMap;       // major version, recorded by lower GridPoint
    ViaMapT routedViaMapUpper;  // recorded by upper GridPoint
    vector<vector<mutex_wrapper>> viaLocks;
    vector<vector<mutex_wrapper>> viaLocksUpper;
    vector<vector<vector<std::pair<int, ViaData*>>>> poorViaMap;
    vector<bool> usePoorViaMap;
    vector<vector<std::unordered_map<int, HistUsageT>>> histViaMap;

    // Unit cost
    // in contest metric
    CostT unitWireCostRaw;                                       // for each DBU
    CostT unitViaCostRaw;                                        // for each occurrence
    CostT unitShortVioCostRaw;                                   // for each area of m2Pitch * m2Pitch
    CostT unitSpaceVioCostRaw;                                   // for each occurrence
    CostT unitMinAreaVioCostRaw;                                 // for each occurrence
    vector<CostT> unitShortVioCost, unitShortVioCostDiscounted;  // ceoff (no unit), a M2 short will charged by
                                                                 // (shortLength * unitWireCostRaw) * unitShortVioCost
    // in DBU
    CostT unitViaCost;                                   // for each occurrence
    CostT unitMinAreaVioCost;                            // for each occurrence
    CostT unitSpaceVioCost, unitSpaceVioCostDiscounted;  // for each occurrence
};

}  //   namespace db
