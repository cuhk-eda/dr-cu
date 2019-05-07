#pragma once

#include "db/Database.h"
#include "single_net/SingleNetRouter.h"

class ConflictDetectionBase {
public:
    ConflictDetectionBase(const vector<SingleNetRouter>& routersToExec) : routers(routersToExec){};
    virtual void initSet(vector<int> jobIdxes) = 0;
    virtual void updateSet(int jobIdx) = 0;
    virtual bool hasConflict(int jobIdx) = 0;

protected:
    using point = bg::model::point<DBU, 2, bg::cs::cartesian>;
    using box = bg::model::box<point>;
    const vector<SingleNetRouter>& routers;
};

class ConflictDetectionFastConstruct : public ConflictDetectionBase {
public:
    using ConflictDetectionBase::ConflictDetectionBase;
    void initSet(vector<int> jobIdxes);
    void updateSet(int jobIdx);
    bool hasConflict(int jobIdx);

private:
    vector<bgi::rtree<std::pair<box, int>, bgi::quadratic<16>>> rtrees;
};

class ConflictDetectionFastQuery : public ConflictDetectionBase {
public:
    ConflictDetectionFastQuery(const vector<SingleNetRouter>& routersToExec);
    void initSet(vector<int> jobIdxes);
    void updateSet(int jobIdx);
    bool hasConflict(int jobIdx);

private:
    vector<vector<int>> conflicts;
    std::unordered_set<int> curConflicts;
};

class Scheduler {
public:
    Scheduler(const vector<SingleNetRouter>& routersToExec) : routers(routersToExec), conflictDetect(routers){};
    vector<vector<int>>& schedule();

private:
    const int defaultMinBatchSize = 100;

    const vector<SingleNetRouter>& routers;
    vector<vector<int>> batches;
    ConflictDetectionFastConstruct conflictDetect;
    // ConflictDetectionFastQuery conflictDetect;

    void assignBackward();
};
