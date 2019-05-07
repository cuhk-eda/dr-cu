#pragma once

#include "db/Database.h"

class PinTapConnector {
public:
    vector<utils::SegmentT<DBU>> bestLink;
    DBU bestVio = 0;

    PinTapConnector(const db::GridPoint& pinTap, const db::Net& databaseNet, int pinIndex)
        : tap(pinTap), dbNet(databaseNet), pinIdx(pinIndex) {}
    db::RouteStatus run();
    static utils::BoxT<DBU> getLinkMetal(const utils::SegmentT<DBU> &link, int layerIdx);

private:
    const db::GridPoint& tap;
    const db::Net& dbNet;
    int pinIdx;

    void shrinkInterval(utils::IntervalT<DBU>& interval, DBU margin);
    void shrinkBox(db::BoxOnLayer& box, DBU margin);
    vector<utils::SegmentT<DBU>> getLinkFromPts(const vector<utils::PointT<DBU>>& linkPts);
    DBU getLinkPinSpaceVio(const vector<utils::SegmentT<DBU>>& link, int layerIdx);
};