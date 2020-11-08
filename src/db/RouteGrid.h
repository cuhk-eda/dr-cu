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

// net index
// a valid net idx >= 0
const int OBS_NET_IDX = -1;   // for obstacles
const int NULL_NET_IDX = -2;  // for neither net nor obstacle

class PoorWire {
public:
    int netIdx;

    PoorWire() : netIdx(NULL_NET_IDX) {}
    PoorWire(int netIndex) : netIdx(netIndex) {}
    PoorWire& operator+=(const PoorWire& rhs) {
        if (netIdx != rhs.netIdx) {
            netIdx = OBS_NET_IDX;
        }
        return *this;
    }
};

inline bool operator==(const PoorWire& lhs, const PoorWire& rhs) { return lhs.netIdx == rhs.netIdx; }

class HistWire {
public:
    int netIdx;
    HistUsageT usage;

    HistWire() : netIdx(NULL_NET_IDX), usage(0.0) {}
    HistWire(int netIndex, HistUsageT histUsage) : netIdx(netIndex), usage(histUsage) {}
    HistWire& operator+=(const HistWire& rhs) {
        if (rhs.netIdx == OBS_NET_IDX) {
            // for "normal" hist cost
            netIdx = OBS_NET_IDX;
            usage += rhs.usage;
        } else {
            // for initial pin access
            if (netIdx != rhs.netIdx) {
                netIdx = OBS_NET_IDX;
            }
            usage = std::max(usage, rhs.usage);
        }
        return *this;
    }
};

inline bool operator==(const HistWire& lhs, const HistWire& rhs) {
    return lhs.netIdx == rhs.netIdx && lhs.usage == rhs.usage;
}

class RouteGrid : public LayerList {
public:
    using ViaMapT = vector<vector<std::multimap<int, int>>>;
    using NDViaMapT = std::unordered_map<GridPoint, const ViaType*>;

    void init();
    void clear();
    void stash();
    void reset();
    void setUnitVioCost(double discount = 1.0);

    // Get unit cost
    inline CostT getUnitViaCost() const { return unitViaCost; }
    inline CostT getUnitMinAreaVioCost() const { return unitMinAreaVioCost; }
    inline CostT getUnitSpaceVioCost() const { return unitSpaceVioCost; }

    // Query fixed metals
    vector<std::pair<utils::BoxT<DBU>, int>> getOvlpBoxes(const BoxOnLayer& box, int idx, const RTrees& rtree) const;
    vector<std::pair<utils::BoxT<DBU>, int>> getOvlpFixedMetals(const BoxOnLayer& box, int netIdx) const;

    // Get edge cost
    CostT getEdgeCost(const GridEdge& edge, const int netIdx) const;
    CostT getEdgeVioCost(const GridEdge& edge, const int netIdx, bool histCost = true) const;
    // 1. via
    CostT getViaCost(const GridPoint& via, const int netIdx) const { return unitViaCost + getViaVioCost(via, netIdx); }
    CostT getViaVioCost(const GridPoint& via,
                        const int netIdx,
                        bool histCost = true,
                        const ViaType* viaType = nullptr) const;
    HistUsageT getViaHistUsage(const GridPoint& via) const;
    // 1.1 via on via(s)
    unsigned getViaUsageOnVias(const GridPoint& via, const int netIdx, const ViaType* viaType = nullptr) const;
    unsigned getViaUsageOnSameLayerVias(const GridPoint& via, const int netIdx, const ViaType* viaType = nullptr) const;
    unsigned getViaUsageOnBotLayerVias(const GridPoint& via, const int netIdx, const ViaType* viaType = nullptr) const;
    unsigned getViaUsageOnTopLayerVias(const GridPoint& via, const int netIdx, const ViaType* viaType = nullptr) const;
    unsigned getViaUsageOnDiffLayerViasHelper(const GridPoint& gp,
                                              const int netIdx,
                                              const vector<vector<bool>>& mergedAllViaVia,
                                              const vector<vector<vector<vector<bool>>>>& allViaVia,
                                              const ViaMapT& viaMap,
                                              bool viaBotVia) const;
    const ViaType* getViaType(const GridPoint& via) const;
    // 1.2 via on wire
    unsigned getViaUsageOnWires(const GridPoint& via, const int netIdx, const ViaType* viaType = nullptr) const;
    unsigned getViaUsageOnWiresPost(const GridPoint& via, const int netIdx, const ViaType* viaType = nullptr) const;
    unsigned getViaUsageOnBotWires(const GridPoint& via, const int netIdx, const ViaType* viaType = nullptr) const;
    unsigned getViaUsageOnTopWires(const GridPoint& via, const int netIdx, const ViaType* viaType = nullptr) const;
    unsigned getViaUsageOnWiresHelper(const GridPoint& gp, const int netIdx, const vector<vector<bool>>& viaWire) const;
    // 1.3 poor via
    enum class ViaPoorness { Poor, Nondefault, Good };
    ViaPoorness getViaPoorness(const GridPoint& via, int netIdx) const;
    int getViaFixedVio(const GridPoint& via, const ViaType& viaType, int netIdx) const;
    int getViaFixedVio(const utils::PointT<DBU>& viaLoc, int viaLayerIdx, const db::ViaType& viaType, int netIdx) const;
    int getFixedMetalVio(const BoxOnLayer& box, int netIdx) const;

    const ViaType& getBestViaTypeForFixed(const utils::PointT<DBU>& viaLoc,
                                          int viaLayerIdx,
                                          int netIdx) const;  // TODO: consider more types of violations
    void initPoorViaMap(vector<std::pair<BoxOnLayer, int>>& fixedMetalVec);
    ViaData* getViaData(const GridPoint& via) const;

    // 2. wire
    CostT getWireSegmentCost(const TrackSegment& ts, const int netIdx) const;
    CostT getWireSegmentVioCost(const TrackSegment& ts, const int netIdx, bool histCost = true) const;
    CostT getWrongWayWireSegmentVioCost(const WrongWaySegment& wws, const int netIdx, bool histCost) const;
    vector<CostT> getShortWireSegmentCost(const TrackSegment& ts, int netIdx) const;
    vector<CostT> getShortWireSegmentVioCost(const TrackSegment& ts, int netIdx, bool histCost = true) const;
    vector<utils::IntervalT<int>> getEmptyIntvl(const TrackSegment& ts, int netIdx) const;
    // 2.1 wire on wires
    vector<int> getShortWireSegmentUsageOnOvlpWire(const TrackSegment& ts, int netIdx) const;
    vector<int> getShortWireSegmentUsageOnOvlpPoorWire(const TrackSegment& ts, int netIdx) const;
    vector<HistUsageT> getShortWireSegmentUsageOnOvlpHistWire(const TrackSegment& ts, int netIdx) const;
    // handle: (interval, usage)
    void iterateWireSegments(const TrackSegment& ts,
                             int netIdx,
                             const std::function<void(const utils::IntervalT<int>&, int)>& handle) const;
    void iteratePoorWireSegments(const TrackSegment& ts,
                                 int netIdx,
                                 const std::function<void(const utils::IntervalT<int>&)>& handle) const;
    void iterateHistWireSegments(const TrackSegment& ts,
                                 int netIdx,
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
                                   const ViaMapT& routedViaMap,
                                   bool wireBotVia,
                                   vector<int>& viaCPs) const;

    // Use edge
    void useEdge(const GridEdge& edge, int netIdx);
    void useVia(const GridPoint& via, int netIdx, ViaMapT& routedViaMap);
    void useVia(const GridPoint& via, int netIdx);
    void markViaType(const GridPoint& via, const ViaType* viaType);
    void useWireSegment(const TrackSegment& ts, int netIdx);
    void useWrongWayWireSegment(const WrongWaySegment& wws, int netIdx);
    void usePoorWireSegment(const TrackSegment& ts, int netIdx);
    void useHistWireSegment(const TrackSegment& ts, int netIdx, HistUsageT usage);
    void useHistWireSegments(const GridBoxOnLayer& gb, int netIdx, HistUsageT usage);
    void markFixedMetalBatch(vector<std::pair<BoxOnLayer, int>>& fixedMetalVec, int beginIdx, int endIdx);

    // Remove edge
    void removeEdge(const GridEdge& edge, int netIdx);
    void removeVia(const GridPoint& via, int netIdx, ViaMapT& routedViaMap);
    void removeVia(const GridPoint& via, int netIdx);
    void removeWireSegment(const TrackSegment& ts, int netIdx);
    void removeWrongWayWireSegment(const WrongWaySegment& wws, int netIdx);

    // Print stat
    double printAllUsageAndVio() const;
    double getScore();
    std::array<double, 4> getAllVio() const;
    // usage
    void getAllWireUsage(const vector<int>& buckets, vector<int>& wireUsageGrid, vector<DBU>& wireUsageLength) const;
    void getAllViaUsage(const vector<int>& buckets, const ViaMapT& routedViaMap, vector<int>& viaUsage) const;
    std::pair<double, double> printAllUsage() const;
    std::string getRangeStr(const vector<int>& buckets, int i) const;
    void getNetWireVioUsage(std::unordered_map<int, int>& via_usage,
                            std::unordered_map<int, float>& wire_usage_length,
                            std::unordered_map<int, std::set<int>>& layer_usage);
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
    void fadeHistCost(const vector<int>& exceptedNets);  // excepted because still not routed...
    void statHistCost() const;

protected:
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

    // Fixed metals (e.g., cell pins, blockage)
    RTrees fixedMetals;  // box -> netIdx

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
    vector<vector<boost::icl::interval_map<int, HistWire>>> histWireMap;
    vector<vector<boost::icl::interval_map<int, HistWire>>> histWireMap_copy;

    // Vias
    // (layerIdx, trackIdx) -> all (crossPointIdx, netIdx)
    // (layerIdx, trackIdx, crossPointIdx) -> all netIdx
    ViaMapT routedViaMap;          // major version, recorded by lower GridPoint
    ViaMapT routedViaMapUpper;     // recorded by upper GridPoint
    NDViaMapT routedNonDefViaMap;  // TODO: merge into routedViaMap
    vector<vector<mutex_wrapper>> viaLocks;
    vector<vector<mutex_wrapper>> viaLocksUpper;
    std::mutex viaTypeLock;
    vector<vector<vector<std::pair<int, ViaData*>>>> poorViaMap;
    vector<bool> usePoorViaMap;
    vector<vector<std::unordered_map<int, HistUsageT>>> histViaMap;
    vector<vector<std::unordered_map<int, HistUsageT>>> histViaMap_copy;
    std::array<double, 4> _vio_usage;
};

}  //   namespace db
