#pragma once

#include "LocalNet.h"

class UpdateDB {
public:
    // Note: after commitRouteResult, localNet should not be used (as move())
    static void commitRouteResult(LocalNet& localNet, db::Net& dbNet);
    static void clearRouteResult(db::Net& dbNet);
};