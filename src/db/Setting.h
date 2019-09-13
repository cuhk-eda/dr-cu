#pragma once

#include "global.h"

namespace db {

BETTER_ENUM(VerboseLevelT, int, LOW = 0, MIDDLE = 1, HIGH = 2);

// global setting
class Setting {
public:
    // basic
    std::string outputFile;
    int numThreads = 1;  // 0 for simple scheduling
    int tat = std::numeric_limits<int>::max();

    // multi_net
    VerboseLevelT multiNetVerbose = VerboseLevelT::MIDDLE;
    bool multiNetScheduleSortAll = true;
    bool multiNetScheduleSort = true;
    bool multiNetScheduleReverse = true;
    int multiNetSelectViaTypesIter = 3;
    int rrrIterLimit = 4;
    bool rrrWriteEachIter = false;
    double rrrInitVioCostDiscount = 0.1;
    double rrrFadeCoeff = 0.01;  // should be <= 0.5 to make sure fade/(1-fade) <= 1

    // single_net
    VerboseLevelT singleNetVerbose = VerboseLevelT::MIDDLE;
    int defaultGuideExpand = 2;  // route guide expansion in pitch for all nets
    int guideExpandIterLimit = 9;
    int diffLayerGuideVioThres = 4;
    double wrongWayPointDensity = 0.1;
    double wrongWayPenaltyCoeff = 4;  // at least weightWrongWayWirelength / weightWirelength + 1 = 3
    bool fixOpenBySST = true;

    // db
    VerboseLevelT dbVerbose = VerboseLevelT::MIDDLE;
    int maxNumWarnForEachRouteStatus = 5;
    bool dbWriteDebugFile = false;
    int dbUsePoorViaMapThres = 100000;
    double dbPoorWirePenaltyCoeff = 8;
    double dbPoorViaPenaltyCoeff = 8;
    double dbInitHistUsageForPinAccess = 0.1;
    double dbNondefaultViaPenaltyCoeff = 0.005;

    //  Metric weights of ISPD 2018 Contest
    //  Wirelength unit is M2 pitch
    //  1. basic objective
    static constexpr double weightWirelength = 0.5;
    static constexpr int weightViaNum = 2;
    //  2. routing preference
    static constexpr int weightOutOfGuideWirelength = 1;
    static constexpr int weightOutOfGuideViaNum = 1;
    static constexpr double weightOffTrackWirelength = 0.5;
    static constexpr int weightOffTrackViaNum = 1;
    static constexpr int weightWrongWayWirelength = 1;
    //  3. violation
    //  normalized by square of M2 pitch
    static constexpr int weightShortArea = 500;
    //  including wire spacing, eol spacing, cut spacing
    static constexpr int weightSpaceVioNum = 500;
    static constexpr int weightMinAreaVioNum = 500;

    void makeItSilent();
    void adapt();
};

extern Setting setting;

// setting that changes in each rrr iteration
class RrrIterSetting {
public:
    int defaultGuideExpand;
    double wrongWayPointDensity;
    bool addDiffLayerGuides;
    bool converMinAreaToOtherVio;

    void update(int iter);
    void print() const;
};

extern RrrIterSetting rrrIterSetting;

}  //   namespace db
