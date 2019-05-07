#pragma once

#include "GridGraphBuilderBase.h"

class GridGraphBuilder : public GridGraphBuilderBase {
public:
    using GridGraphBuilderBase::GridGraphBuilderBase;

    void run();

private:
    void connectGuide(int guideIdx);
    void addRegWrongWayConn(int guideIdx);
    void addPinWrongWayConn();
    void addAdjGuideWrongWayConn();
    void addWrongWayConn();
    void connectTwoGuides(int guideIdx1, int guideIdx2);

    void setMinAreaFlags();

    int guideToVertex(int gIdx, int trackIdx, int cpIdx) const;
    int boxToVertex(const db::GridBoxOnLayer& box, int pointBias, int trackIdx, int cpIdx) const;
};