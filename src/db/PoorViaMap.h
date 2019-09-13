#pragma once

#include "RouteGrid.h"

namespace db {

class RouteGrid;

class ViaData {
public:
    virtual ~ViaData() = default;
    virtual bool allow(int netIdx) const = 0;
    virtual void setAllowNets(const std::unordered_set<int>& netIdxs){};
    virtual void setAllowNet(int netIdx){};
    virtual int getAllowNet() { return OBS_NET_IDX; };  // should never be called

    bool isNonDefaultOnly() const { return nonDefaultOnly; }
    bool nonDefaultOnly;  // none of the allow net can use default via
};

class GoodVia : public ViaData {
public:
    bool allow(int netIdx) const { return true; }
};

class PoorVia : public ViaData {
public:
    bool allow(int netIdx) const { return false; }
};

class FlexVia : public ViaData {
public:
    std::list<int> allowNetIdxs;
    void setAllowNets(const std::unordered_set<int>& netIdxs) {
        allowNetIdxs.insert(allowNetIdxs.end(), netIdxs.begin(), netIdxs.end());
    }
    bool allow(int netIdx) const {
        for (auto id : allowNetIdxs)
            if (id == netIdx) return true;
        return false;
    }
};

class FixVia : public ViaData {
public:
    int allowNetIdx;
    void setAllowNet(int netIdx) { allowNetIdx = netIdx; }
    bool allow(int netIdx) const { return netIdx == allowNetIdx; }
    int getAllowNet() { return allowNetIdx; }
};

class PoorViaMapBuilder {
public:
    PoorViaMapBuilder(vector<vector<vector<std::pair<int, ViaData*>>>>& poorViaMap,
                      const vector<bool>& _usePoorViaMap,
                      const RouteGrid& _routeGrid);
    void run(const vector<std::pair<BoxOnLayer, int>>& fixObjects);

    static ViaData* createViaData(const std::unordered_set<int>& allowNetIdxs, bool allAllow, bool nonDefaultOnly);
    static void addViaData(vector<std::pair<int, ViaData*>>& rowData,
                           int topTrackIdx,
                           const std::unordered_set<int>& allowNetIdxs,
                           bool allAllow,
                           bool nonDefaultOnly);

private:
    vector<vector<vector<std::pair<int, ViaData*>>>>& poorViaMap;
    const vector<bool>& usePoorViaMap;
    const RouteGrid& routeGrid;

    void printPoorViaMapInfo();

    void initPoorViaMapFast(const vector<std::pair<BoxOnLayer, int>>& fixObjects);
};

class PoorViaMapRegionBuilder {
public:
    PoorViaMapRegionBuilder(vector<vector<std::pair<int, ViaData*>>>& _poorViaMap,
                            const RouteGrid& _routeGrid,
                            const vector<std::pair<BoxOnLayer, int>>& _fixObjects,
                            int cutLayerIdx);
    void run(const utils::IntervalT<int>& regionTrackRange, const vector<vector<int>>& regionObjects);

private:
    vector<vector<std::pair<int, ViaData*>>>& poorViaMap;
    const RouteGrid& routeGrid;
    const vector<std::pair<BoxOnLayer, int>>& fixObjects;
    const MetalLayer& botLayer;
    const MetalLayer& topLayer;
    const CutLayer& cutLayer;

    vector<vector<bool>> goodMap;
    vector<vector<vector<int>>> exceptionMap;
    vector<vector<bool>> curGoodMap;
    vector<vector<bool>> curPoorMap;
    vector<vector<bool>> nonDefaultMap;

    void getSlideRegion(const utils::BoxT<DBU>& moveRect,
                        const utils::BoxT<DBU>& fixRect,
                        utils::IntervalT<int>& botTrackRange,
                        utils::IntervalT<int>& topTrackRange) const;
    void markSpace(const utils::BoxT<DBU>& moveRect,
                   const utils::BoxT<DBU>& fixRect,
                   int netIdx,
                   const utils::IntervalT<int>& regionTrackRange);
    void markC2C(const utils::BoxT<DBU>& viaRect,
                 const utils::BoxT<DBU>& metalRect,
                 int netIdx,
                 const utils::IntervalT<int>& regionTrackRange,
                 bool isBot);
    void markLocMap(int a, int b, int netIdx);

    void markViaType(const ViaType& viaType,
                     utils::IntervalT<int> regionTrackRange,
                     const vector<int>& botFixObjectIdxs,
                     const vector<int>& topFixObjectIdxs);
    void genViaData(utils::IntervalT<int> regionTrackRange);
};
}  // namespace db
