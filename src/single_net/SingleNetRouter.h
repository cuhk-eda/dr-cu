#pragma once

#include "LocalNet.h"
#include "PostMazeRoute.h"
#include "UpdateDB.h"
#include "PostRoute.h"

class SingleNetRouter {
public:
    LocalNet localNet;
    db::Net& dbNet;

    db::RouteStatus status;

    SingleNetRouter(db::Net& dbNet);

    void preRoute();
    void mazeRoute();
    void commitNetToDB();
};