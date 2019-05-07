#include "SingleNetRouter.h"
#include "PreRoute.h"
#include "MazeRoute.h"
#include "PostMazeRoute.h"
#include "UpdateDB.h"
#include "PostRoute.h"

SingleNetRouter::SingleNetRouter(db::Net& databaseNet)
    : localNet(databaseNet), dbNet(databaseNet), status(db::RouteStatus::SUCC_NORMAL) {}

void SingleNetRouter::preRoute() {
    // Pre-route (obtain proper grid boxes)
    status &= PreRoute(localNet).runIterative();
}

void SingleNetRouter::mazeRoute() {
    // Maze route (working on grid only)
    status &= MazeRoute(localNet).run();
    PostMazeRoute(localNet).run();
}

void SingleNetRouter::commitNetToDB() {
    // Commit net to DB (commit result to DB)
    UpdateDB::commitRouteResult(localNet, dbNet);
}