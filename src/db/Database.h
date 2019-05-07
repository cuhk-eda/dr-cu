#pragma once

#include "RsynService.h"
#include "RouteGrid.h"
#include "Net.h"
#include "Setting.h"
#include "Stat.h"

namespace db {

class Database : public RouteGrid, public NetList {
public:
    utils::BoxT<DBU> dieRegion;

    void init();
    // mark pin occupancy on RouteGrid
    void markPinAndObsOccupancy();

    void writeDEFWireSegment(Net& dbNet, const utils::PointT<DBU>& u, const utils::PointT<DBU>& v, int layerIdx);
    void writeDEFVia(Net& dbNet, const utils::PointT<DBU>& point, const ViaType& viaType, int layerIdx);
    void writeDEFFillRect(Net& dbNet, utils::BoxT<DBU>& rect, int layerIdx);
    void writeDEF(const std::string& filename);

    // TODO: seperate the batch reporting codes
    void printAllUsageAndVio();

private:
    RsynService rsynService;

    // slice route guide polygons along track direction
    void sliceRouteGuides();
};

}  //   namespace db

extern db::Database database;
