#include "UpdateDB.h"

void UpdateDB::commitRouteResult(LocalNet &localNet, db::Net &dbNet) {
    // update db::Net
    dbNet.gridTopo = move(localNet.gridTopo);
    // update RouteGrid
    dbNet.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        if (node->parent) {
            database.useEdge({*node, *(node->parent)}, dbNet.idx);
        }
        if (node->extWireSeg) {
            database.useEdge(*(node->extWireSeg), dbNet.idx);
        }
    });
}

void UpdateDB::clearRouteResult(db::Net &dbNet) {
    // update RouteGrid
    dbNet.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        if (node->parent) {
            database.removeEdge({*node, *(node->parent)}, dbNet.idx);
        }
        if (node->extWireSeg) {
            database.removeEdge(*(node->extWireSeg), dbNet.idx);
        }
    });
    // update db::Net
    dbNet.clearResult();
}

void UpdateDB::commitMinAreaRouteResult(db::Net& dbNet) {
    dbNet.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        if (node->extWireSeg) {
            database.useEdge(*(node->extWireSeg), dbNet.idx);
        }
    });
};

void UpdateDB::clearMinAreaRouteResult(db::Net& dbNet) {
    dbNet.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        if (node->extWireSeg) {
            database.removeEdge(*(node->extWireSeg), dbNet.idx);
        }
    });
};

void UpdateDB::commitViaTypes(db::Net& dbNet) {
    dbNet.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        if (!(node->parent)) return;
        db::GridEdge edge(*node, *(node->parent));
        if (!edge.isVia())  return;
        database.markViaType(edge.lowerGridPoint(), node->viaType);
    });
};

bool UpdateDB::checkViolation(db::Net &dbNet) {
    bool hasVio = false;
    auto checkEdge = [&](const db::GridEdge& edge) {
        if (database.getEdgeVioCost(edge, dbNet.idx, false)) {
            hasVio = true;
            auto uLoc = database.getLoc(edge.u);
            auto vLoc = database.getLoc(edge.v);
            std::vector<std::pair<boostBox, int>> relatedGuides;
            auto checkBox = [&](const db::BoxOnLayer& boxOnLayer) {
                auto box = boxOnLayer;
                database.expandBox(box, db::rrrIterSetting.defaultGuideExpand);
                boostBox query_box(boostPoint(box.x.low, box.y.low), boostPoint(box.x.high, box.y.high));
                dbNet.routeGuideRTrees[box.layerIdx].query(bgi::intersects(query_box), std::back_inserter(relatedGuides));
            };
            if (edge.u.layerIdx != edge.v.layerIdx) {
                checkBox({edge.u.layerIdx, uLoc, uLoc});
                checkBox({edge.v.layerIdx, vLoc, vLoc});
            }
            else {
                checkBox({edge.u.layerIdx, uLoc, vLoc});
            }
            for (const auto& guide : relatedGuides) {
                ++dbNet.routeGuideVios[guide.second];
            }
        }
    };
    dbNet.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        if (node->parent) checkEdge({*node, *(node->parent)});
        if (node->extWireSeg) checkEdge(*(node->extWireSeg));
    });
    return hasVio;
}

double UpdateDB::getNetVioCost(const db::Net &dbNet) {
    double net_cost{0};
    auto checkEdge = [&](const db::GridEdge& edge) {
        double edge_cost = database.getEdgeVioCost(edge, dbNet.idx, false);
        net_cost += edge_cost;
    };
    dbNet.postOrderVisitGridTopo([&](std::shared_ptr<db::GridSteiner> node) {
        if (node->parent) checkEdge({*node, *(node->parent)});
        if (node->extWireSeg) checkEdge(*(node->extWireSeg));
    });
    return net_cost;
}

