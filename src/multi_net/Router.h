#pragma once

#include "db/Database.h"
#include "single_net/SingleNetRouter.h"

class MTStat {
public:
    vector<double> durations;
    MTStat(int numOfThreads = 0) : durations(numOfThreads, 0.0) {}
    const MTStat& operator+=(const MTStat& rhs);
    friend ostream& operator<<(ostream& os, const MTStat mtStat);
};

class Router {
public:
    void run();

private:
    int iter;
    vector<db::RouteStatus> allNetStatus;  // minor TODO: integrate with db::RouteStat

    vector<int> getNetsToRoute();
    void ripup(const vector<int>& netsToRoute);
    void updateCost();
    void route(const vector<int>& netsToRoute);  // minor TODO: seperate into a class
    void finish();
    void unfinish();

    void printStat(bool major = false);
    MTStat runJobsMT(int numJobs, const std::function<void(int)>& handle);
};
