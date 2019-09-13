#pragma once

#include "db/Database.h"
#include "single_net/SingleNetRouter.h"

class Scheduler {
public:
    Scheduler(const vector<SingleNetRouter>& routersToExec) : routers(routersToExec){};
    vector<vector<int>>& schedule();

private:
    const vector<SingleNetRouter>& routers;
    vector<vector<int>> batches;

    // for conflict detect
    RTrees rtrees;
    virtual void initSet(vector<int> jobIdxes);
    virtual void updateSet(int jobIdx);
    virtual bool hasConflict(int jobIdx);
};

class PostScheduler {
public:
    PostScheduler(const vector<db::Net>& netsToExec) : dbNets(netsToExec){};
    vector<vector<int>>& schedule();

private:
    const vector<db::Net>& dbNets;
    vector<vector<int>> batches;

    // for conflict detect
    RTrees rtrees;
    boostBox getBoostBox(const db::GridPoint &gp);
    boostBox getBoostBox(const db::GridEdge &edge);
    vector<std::pair<boostBox, int>> getNetBoxes(const db::Net& dbNet);
    virtual void initSet(vector<int> jobIdxes);
    virtual void updateSet(int jobIdx);
    virtual bool hasConflict(int jobIdx);   
};