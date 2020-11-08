#pragma once

#include "LocalNet.h"

class UpdateDB {
public:
    // Note: after commitRouteResult, localNet should not be used (as move())
    static void commitRouteResult(LocalNet& localNet, db::Net& dbNet);
    static void clearRouteResult(db::Net& dbNet);
    static void commitMinAreaRouteResult(db::Net& dbNet);
    static void clearMinAreaRouteResult(db::Net& dbNet);
    static void commitViaTypes(db::Net& dbNet);
    static bool checkViolation(db::Net& dbNet);
    static double getNetVioCost(const db::Net &dbNet);
};
