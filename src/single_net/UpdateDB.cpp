#include "UpdateDB.h"

void UpdateDB::commitRouteResult(LocalNet &localNet, db::Net &dbNet) {
    // update db::Net
    dbNet.gridTopo = move(localNet.gridTopo);
    dbNet.extendedWireSegment = move(localNet.extendedWireSegment);
    dbNet.minAreaVio = localNet.minAreaVio;
    dbNet.minAreaShadowedVio = localNet.minAreaShadowedVio;
    // update RouteGrid
    dbNet.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        if (node->parent) {
            database.useEdge({*node, *(node->parent)}, dbNet.idx);
        }
    });
    for (const auto &edge : dbNet.extendedWireSegment) {
        database.useEdge(edge, dbNet.idx);
    }
}

void UpdateDB::clearRouteResult(db::Net &dbNet) {
    // update RouteGrid
    dbNet.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        if (node->parent) {
            database.removeEdge({*node, *(node->parent)}, dbNet.idx);
        }
    });
    for (const auto &edge : dbNet.extendedWireSegment) {
        database.removeEdge(edge, dbNet.idx);
    }
    // update db::Net
    dbNet.clearResult();
}
