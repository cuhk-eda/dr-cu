#pragma once

#include "RsynService.h"
#include "RouteGrid.h"
#include "Net.h"
#include "Setting.h"
#include "Stat.h"

class MTStat {
public:
    vector<double> durations;
    MTStat(int numOfThreads = 0) : durations(numOfThreads, 0.0) {}
    const MTStat& operator+=(const MTStat& rhs);
    friend ostream& operator<<(ostream& os, const MTStat mtStat);
};

namespace db {

class Database : public RouteGrid, public NetList {
public:
    utils::BoxT<DBU> dieRegion;

    void init();
    void clear() { RouteGrid::clear(); }
    void reset() { RouteGrid::reset(); }
    void stash() { RouteGrid::stash(); }

    void writeDEFWireSegment(Net& dbNet, const utils::PointT<DBU>& u, const utils::PointT<DBU>& v, int layerIdx);
    void writeDEFVia(Net& dbNet, const utils::PointT<DBU>& point, const ViaType& viaType, int layerIdx);
    void writeDEFFillRect(Net& dbNet, const utils::BoxT<DBU>& rect, const int layerIdx);
    void writeDEF(const std::string& filename);

    // get girdPinAccessBoxes
    // TODO: better way to differetiate same-layer and diff-layer girdPinAccessBoxes
    void getGridPinAccessBoxes(const Net& net, vector<vector<db::GridBoxOnLayer>>& gridPinAccessBoxes) const;

private:
    RsynService rsynService;

    // mark pin and obstacle occupancy on RouteGrid
    void markPinAndObsOccupancy();
    // mark off-grid vias as obstacles
    void addPinViaMetal(vector<std::pair<BoxOnLayer, int>>& fixedMetalVec);

    // init safe margin for multi-thread
    void initMTSafeMargin();

    // slice route guide polygons along track direction
    void sliceRouteGuides();

    // construct RTrees for route guides of each net
    void constructRouteGuideRTrees();
};

}  //   namespace db

extern db::Database database;

namespace std {

//  hash function for Dimension
template <>
struct hash<Dimension> {
    std::size_t operator()(const Dimension d) const { return (hash<unsigned>()(d)); }
};

//  hash function for std::tuple<typename t0, typename t1, typename t2>
template <typename t0, typename t1, typename t2>
struct hash<std::tuple<t0, t1, t2>> {
    std::size_t operator()(const std::tuple<t0, t1, t2>& t) const {
        return (hash<t0>()(std::get<0>(t)) ^ hash<t1>()(std::get<1>(t)) ^ hash<t2>()(std::get<2>(t)));
    }
};

}  // namespace std

MTStat runJobsMT(int numJobs, const std::function<void(int)>& handle);
