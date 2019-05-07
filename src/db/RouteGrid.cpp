#include "RouteGrid.h"
#include "PoorViaMap.h"

namespace db {

void RouteGrid::init() {
    if (db::setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << "Init RouteGrid ..." << std::endl;
    }
    LayerList::init();
    // Fixed metal
    fixedMetals.resize(layers.size());
    fixedMetalForbidRegions.resize(layers.size());
    fixedMetalC2CForbidRegions.resize(layers.size());
    // Wire
    routedWireMap.resize(layers.size());
    poorWireMap.resize(layers.size());
    histWireMap.resize(layers.size());
    wireLocks.resize(layers.size());
    // Via
    routedViaMap.resize(layers.size());       // the last layer will not be used
    routedViaMapUpper.resize(layers.size());  // the first layer will not be used
    viaLocks.resize(layers.size());           // the last layer will not be used
    viaLocksUpper.resize(layers.size());      // the first layer will not be used
    histViaMap.resize(layers.size());         // the last layer will not be used
    for (int i = 0; i < layers.size(); ++i) {
        // Wire
        routedWireMap[i].resize(layers[i].numTracks());
        poorWireMap[i].resize(layers[i].numTracks());
        histWireMap[i].resize(layers[i].numTracks());
        wireLocks[i].resize(layers[i].numTracks());
        // Via
        routedViaMap[i].resize(layers[i].numTracks());
        routedViaMapUpper[i].resize(layers[i].numTracks());
        viaLocks[i].resize(layers[i].numTracks());
        viaLocksUpper[i].resize(layers[i].numTracks());
        histViaMap[i].resize(layers[i].numTracks());
    }

    DBU m2Pitch = layers[1].pitch;
    unitWireCostRaw = db::Setting::weightWirelength / m2Pitch;
    unitViaCostRaw = db::Setting::weightViaNum;
    unitViaCost = unitViaCostRaw / unitWireCostRaw;

    unitShortVioCostRaw = db::Setting::weightShortArea;
    unitSpaceVioCostRaw = db::Setting::weightSpaceVioNum;
    unitMinAreaVioCostRaw = db::Setting::weightMinAreaVioNum;

    unitShortVioCost.resize(layers.size());
    for (int i = 0; i < layers.size(); ++i) {
        unitShortVioCost[i] = unitShortVioCostRaw * layers[i].width / m2Pitch / m2Pitch / unitWireCostRaw;
    }
    // note: a short of M2 track segments will be charged by
    // (shortLength * unitWireCostRaw) * unitShortVioCost
    // which should be unitShortVioCostRaw * (shortLength / m2Pitch) * (layers[i].width / m2Pitch)
    // Therefore, unitShortVioCost = unitShortVioCostRaw * layers[i].width / m2Pitch / m2Pitch / unitWireCostRaw

    unitSpaceVioCost = unitSpaceVioCostRaw / unitWireCostRaw;
    unitMinAreaVioCost = unitMinAreaVioCostRaw / unitWireCostRaw;

    setUnitVioCost();

    if (setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        LayerList::print();
        log() << "ROUTE GRID COST" << std::endl;
        log() << "unitWireCostRaw (score / DBU) = " << unitWireCostRaw << std::endl;
        log() << "unitViaCostRaw (score for each via) = " << unitViaCostRaw << std::endl;
        log() << "unitShortVioCostRaw (score for each area of m2Pitch * m2Pitch) = " << unitShortVioCostRaw
              << std::endl;
        log() << "unitSpaceVioCostRaw (score for each violation) = " << unitSpaceVioCostRaw << std::endl;
        log() << "unitMinAreaVioCostRaw (score for each violation) = " << unitMinAreaVioCostRaw << std::endl;
        log() << "After normalization by unitWireCostRaw: " << std::endl;
        log() << "unitWireCost (in DBU) = 1" << std::endl;
        log() << "unitViaCost (in DBU) = " << unitViaCost << std::endl;
        log() << "unitShortVioCost (coeff for \"normal\" short) = " << unitShortVioCost << std::endl;
        log() << "unitSpaceVioCost (in DBU) = " << unitSpaceVioCost << std::endl;
        log() << "unitMinAreaVioCost (in DBU) = " << unitMinAreaVioCost << std::endl;
        log() << std::endl;
    }
}

void RouteGrid::setUnitVioCost(double discount) {
    if (setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("Set unit vio cost with discount of", discount);
    }
    unitShortVioCostDiscounted.resize(unitShortVioCost.size());
    for (int i = 0; i < unitShortVioCost.size(); ++i) {
        unitShortVioCostDiscounted[i] = unitShortVioCost[i] * discount;
    }
    unitSpaceVioCostDiscounted = unitSpaceVioCost * discount;
}

CostT RouteGrid::getEdgeCost(const GridEdge& edge, const int netIdx) const {
    if (edge.isVia()) {
        return getViaCost(edge.lowerGridPoint(), netIdx);
    } else if (edge.isTrackSegment()) {
        return getWireSegmentCost({edge}, netIdx);
    } else {
        log() << "Warning in RouteGrid::getEdgeCost: invalid edge type" << std::endl;
        return 0;
    }
}

CostT RouteGrid::getEdgeVioCost(const GridEdge& edge, const int netIdx, bool histCost) const {
    if (edge.isVia()) {
        return getViaVioCost(edge.lowerGridPoint(), netIdx, histCost);
    } else if (edge.isTrackSegment()) {
        return getWireSegmentVioCost({edge}, netIdx, histCost);
    } else if (edge.isWrongWaySegment()) {
        return getWrongWayWireSegmentVioCost({edge}, netIdx, histCost);
    } else {
        log() << "Warning in RouteGrid::getEdgeVioCost: invalid edge type" << std::endl;
        return 0;
    }
}

CostT RouteGrid::getViaVioCost(const GridPoint& via, const int netIdx, bool histCost) const {
    auto usage = getViaUsage(via, netIdx);
    auto viaPoorness = getViaPoorness(via, netIdx);
    double pinSpace = (viaPoorness == ViaPoorness::Poor) ? setting.dbPoorViaPenaltyCoeff : 0;
    double nondefault = (viaPoorness == ViaPoorness::Nondefault && histCost) ? setting.dbNondefaultViaPenaltyCoeff : 0;
    HistUsageT histUsage = histCost ? getViaUsageOnHistVia(via) : 0;
    return (usage.first + usage.second + histUsage) * unitSpaceVioCostDiscounted +
           (pinSpace + nondefault) * unitSpaceVioCost;
}

std::pair<unsigned, unsigned> RouteGrid::getViaUsage(const GridPoint& via, const int netIdx) const {
    unsigned usageOnVias = 0;
    usageOnVias += getViaUsageOnSameLayerVias(via, netIdx);
    usageOnVias += getViaUsageOnBotLayerVias(via, netIdx);
    usageOnVias += getViaUsageOnTopLayerVias(via, netIdx);

    unsigned usageOnWires = getViaUsageOnWires(cutLayers[via.layerIdx].viaBotWire, via, netIdx);
    if (usageOnWires) {
        return {usageOnVias, usageOnWires};
    }

    return {usageOnVias, getViaUsageOnWires(cutLayers[via.layerIdx].viaTopWire, getUpper(via), netIdx)};
}

unsigned RouteGrid::getViaUsageOnWires(const vector<GridPoint>& gps, const int netIdx) const {
    const int low = gps.front().crossPointIdx;
    const int high = gps.back().crossPointIdx;
    const vector<int>& crossPointUsage =
        getShortWireSegmentUsageOnOvlpWire({gps[0].layerIdx, gps[0].trackIdx, {low, high}}, netIdx);
    for (int usage : crossPointUsage) {
        if (usage) return usage;
    }
    return 0;
}

unsigned RouteGrid::getViaUsageOnWires(const vector<vector<vector<bool>>>& viaWire,
                                       const GridPoint& gp,
                                       const int netIdx) const {
    int xSize = ((int)viaWire[gp.crossPointIdx].size() - 1) / 2;
    int ySize = ((int)viaWire[gp.crossPointIdx][0].size() - 1) / 2;
    for (unsigned i = max(0, gp.trackIdx - xSize); i < min(layers[gp.layerIdx].numTracks(), gp.trackIdx + xSize + 1);
         ++i) {
        vector<GridPoint> gps;
        for (unsigned j = max(0, gp.crossPointIdx - ySize);
             j < min(layers[gp.layerIdx].numCrossPoints(), gp.crossPointIdx + ySize + 1);
             ++j) {
            if (viaWire[gp.crossPointIdx][i + xSize - gp.trackIdx][j + ySize - gp.crossPointIdx]) {
                gps.emplace_back(gp.layerIdx, i, j);
            }
        }
        if (gps.empty()) {
            continue;
        }
        unsigned usageOnWires = getViaUsageOnWires(gps, netIdx);
        if (usageOnWires) {
            return usageOnWires;
        }
    }
    return 0;
}

unsigned RouteGrid::getViaUsageOnVia(const GridPoint& via, const int netIdx) const {
    unsigned usage = 0;
    auto itRange = routedViaMap[via.layerIdx][via.trackIdx].equal_range(via.crossPointIdx);
    for (auto it = itRange.first; it != itRange.second; ++it) {
        if (it->second != netIdx) {
            ++usage;
        }
    }
    return usage;
}

HistUsageT RouteGrid::getViaUsageOnHistVia(const GridPoint& via) const {
    auto it = histViaMap[via.layerIdx][via.trackIdx].find(via.crossPointIdx);
    if (it == histViaMap[via.layerIdx][via.trackIdx].end()) {
        return 0.0;
    } else {
        return it->second;
    }
}

unsigned RouteGrid::getViaUsageOnSameLayerVias(const GridPoint& via, const int netIdx) const {
    unsigned usageOnVias = 0;
    const GridPoint& upper = getUpper(via);
    int xSize = (int)cutLayers[via.layerIdx].viaMetal.size() - 1;
    int ySize = (int)cutLayers[via.layerIdx].viaMetal[0].size() - 1;
    for (int i = max(0, via.trackIdx - xSize); i < min(layers[via.layerIdx].numTracks(), via.trackIdx + xSize + 1);
         ++i) {
        for (int j = max(0, upper.trackIdx - ySize);
             j < min(layers[upper.layerIdx].numTracks(), upper.trackIdx + ySize + 1);
             ++j) {
            if (cutLayers[via.layerIdx].viaMetal[abs(i - via.trackIdx)][abs(j - upper.trackIdx)]) {
                usageOnVias += getViaUsageOnVia({via.layerIdx, i, layers[upper.layerIdx].tracks[j].lowerCPIdx}, netIdx);
            }
        }
    }
    return usageOnVias;
}

unsigned RouteGrid::getViaUsageOnBotLayerVias(const GridPoint& via, const int netIdx) const {
    unsigned usageOnVias = 0;
    const int viaCrossPointIdx = via.crossPointIdx;
    if (via.layerIdx) {
        int xSize = ((int)cutLayers[via.layerIdx].viaBotVia[viaCrossPointIdx].size() - 1) / 2;
        int ySize = ((int)cutLayers[via.layerIdx].viaBotVia[viaCrossPointIdx][0].size() - 1) / 2;
        for (unsigned i = max(0, via.trackIdx - xSize);
             i < min(layers[via.layerIdx].numTracks(), via.trackIdx + xSize + 1);
             ++i) {
            for (unsigned j = max(0, viaCrossPointIdx - ySize);
                 j < min(layers[via.layerIdx].numCrossPoints(), viaCrossPointIdx + ySize + 1);
                 ++j) {
                int lowerTrackIdx = layers[via.layerIdx].crossPoints[j].lowerTrackIdx;
                if (lowerTrackIdx >= 0 && cutLayers[via.layerIdx].viaBotVia[viaCrossPointIdx][i + xSize - via.trackIdx]
                                                                           [j + ySize - viaCrossPointIdx]) {
                    usageOnVias += getViaUsageOnVia(
                        {via.layerIdx - 1, lowerTrackIdx, layers[via.layerIdx].tracks[i].lowerCPIdx}, netIdx);
                }
            }
        }
    }
    return usageOnVias;
}

unsigned RouteGrid::getViaUsageOnTopLayerVias(const GridPoint& via, const int netIdx) const {
    unsigned usageOnVias = 0;
    const GridPoint& upper = getUpper(via);
    if (upper.layerIdx + 1 < layers.size()) {
        int xSize = ((int)cutLayers[via.layerIdx].viaTopVia[upper.crossPointIdx].size() - 1) / 2;
        int ySize = ((int)cutLayers[via.layerIdx].viaTopVia[upper.crossPointIdx][0].size() - 1) / 2;
        for (unsigned i = max(0, upper.trackIdx - xSize);
             i < min(layers[upper.layerIdx].numTracks(), upper.trackIdx + xSize + 1);
             ++i) {
            for (unsigned j = max(0, upper.crossPointIdx - ySize);
                 j < min(layers[upper.layerIdx].numCrossPoints(), upper.crossPointIdx + ySize + 1);
                 ++j) {
                if (layers[upper.layerIdx].crossPoints[j].upperTrackIdx >= 0 &&
                    cutLayers[via.layerIdx]
                        .viaTopVia[upper.crossPointIdx][i + xSize - upper.trackIdx][j + ySize - upper.crossPointIdx]) {
                    usageOnVias += getViaUsageOnVia({upper.layerIdx, (int)i, (int)j}, netIdx);
                }
            }
        }
    }
    return usageOnVias;
}

RouteGrid::ViaPoorness RouteGrid::getViaPoorness(const GridPoint& via, int netIdx) const {
    if (usePoorViaMap[via.layerIdx]) {
        auto viaData = getViaData(via);
        if (viaData->allow(netIdx)) {
            return viaData->isNonDefaultOnly() ? ViaPoorness::Nondefault : ViaPoorness::Good;
        } else {
            return ViaPoorness::Poor;
        }
    } else {
        const auto& cutLayer = getCutLayer(via.layerIdx);
        if (getViaSpaceVio(via, cutLayer.defaultViaType(), netIdx) == 0) {
            return ViaPoorness::Good;
        }
        for (const auto& viaType : cutLayer.allViaTypes) {
            if (getViaSpaceVio(via, viaType, netIdx) == 0) {
                return ViaPoorness::Nondefault;
            }
        }
        return ViaPoorness::Poor;
    }
}

void RouteGrid::initPoorViaMap() {
    PoorViaMapBuilder builder(poorViaMap, usePoorViaMap, *this);
    builder.run(fixedMetalVec);
}

ViaData* RouteGrid::getViaData(const GridPoint& via) const {
    int layerIdx = via.layerIdx;
    int loTrackIdx = via.trackIdx;
    int hiTrackIdx = layers[layerIdx].getUpper(via).trackIdx;
    auto& intvls = poorViaMap[layerIdx][loTrackIdx];
    int idx = upper_bound(intvls.begin(), intvls.end(), std::pair<int, ViaData*>(hiTrackIdx, NULL)) - intvls.begin();
    if (hiTrackIdx != intvls[idx].first) idx--;

    return intvls[idx].second;
}

int RouteGrid::getViaSpaceVio(const db::GridPoint& via, const db::ViaType& viaType, int netIdx) const {
    int numOvlp = 0;
    const auto& cutLayer = getCutLayer(via.layerIdx);
    auto viaLoc = getLoc(via);
    auto updateSpaceVio = [&](const vector<utils::BoxT<DBU>>& forbidRegions,
                              const vector<std::pair<utils::BoxT<DBU>, int>>& neighMetals) {
        for (auto forbidRegion : forbidRegions) {
            forbidRegion.ShiftBy(viaLoc);
            for (const auto& neighMetal : neighMetals) {
                auto ovlp = forbidRegion.IntersectWith(neighMetal.first);
                if (ovlp.IsValid()) {
                    numOvlp += (ovlp.area() > 0);
                }
            }
        }
    };
    auto updateC2CSpaceVio = [&](const vector<utils::BoxT<DBU>>& forbidRegions,
                                 const vector<std::pair<utils::BoxT<DBU>, int>>& neighMetals,
                                 int layerIdx) {
        for (auto forbidRegion : forbidRegions) {
            forbidRegion.ShiftBy(viaLoc);
            for (const auto& neighMetal : neighMetals) {
                DBU space = layers[layerIdx].getSpace(neighMetal.first);
                numOvlp += (utils::L2Dist(forbidRegion, neighMetal.first) < space);
            }
        }
    };

    // to neighbors
    auto botMaxForbidRegion = cutLayer.botMaxForbidRegion;
    botMaxForbidRegion.ShiftBy(viaLoc);
    auto topMaxForbidRegion = cutLayer.topMaxForbidRegion;
    topMaxForbidRegion.ShiftBy(viaLoc);
    auto botNeighMetals = getOvlpFixedMetals({via.layerIdx, botMaxForbidRegion}, netIdx);
    auto topNeighMetals = getOvlpFixedMetals({via.layerIdx + 1, topMaxForbidRegion}, netIdx);
    updateSpaceVio(viaType.botForbidRegions, botNeighMetals);
    updateSpaceVio(viaType.topForbidRegions, topNeighMetals);
    // from neighbors
    auto viaBot = viaType.bot, viaTop = viaType.top;
    viaBot.ShiftBy(viaLoc);
    viaTop.ShiftBy(viaLoc);
    auto botNeighForbidRegions = getOvlpFixedMetalForbidRegions({via.layerIdx, viaBot}, netIdx);
    auto topNeighForbidRegions = getOvlpFixedMetalForbidRegions({via.layerIdx + 1, viaTop}, netIdx);
    updateSpaceVio({viaType.bot}, botNeighForbidRegions);
    updateSpaceVio({viaType.top}, topNeighForbidRegions);
    // corner to corner spacing
    auto botNeighC2CMetals = getOvlpC2CMetals({via.layerIdx, viaBot}, netIdx);
    auto topNeighC2CMetals = getOvlpC2CMetals({via.layerIdx + 1, viaTop}, netIdx);
    updateC2CSpaceVio({viaType.bot}, botNeighC2CMetals, via.layerIdx);
    updateC2CSpaceVio({viaType.top}, topNeighC2CMetals, via.layerIdx + 1);

    return numOvlp;
}  // namespace db

CostT RouteGrid::getWireSegmentCost(const TrackSegment& ts, const int netIdx) const {
    DBU dist = layers[ts.layerIdx].getCrossPointRangeDistCost(ts.crossPointRange);
    return getWireSegmentVioCost(ts, netIdx) + dist;
}

CostT RouteGrid::getWireSegmentVioCost(const TrackSegment& ts, const int netIdx, bool histCost) const {
    CostT cost = 0;
    // 1. With other wires
    // 1.1 Short
    iterateWireSegments(ts, netIdx, [&](const utils::IntervalT<int>& intvl, int usage) {
        DBU dist = layers[ts.layerIdx].getCrossPointRangeDistCost(intvl);
        cost += unitShortVioCostDiscounted[ts.layerIdx] * usage * dist;
    });
    // 1.2 Space
    cost += (unitSpaceVioCostDiscounted * getWireSegmentSpaceVioOnWires(ts, netIdx).size());
    // 2. Short or space vio with pins/obs
    iteratePoorWireSegments(ts, netIdx, [&](const utils::IntervalT<int>& intvl) {
        DBU dist = layers[ts.layerIdx].getCrossPointRangeDistCost(intvl);
        cost += unitShortVioCost[ts.layerIdx] * dist * setting.dbPoorWirePenaltyCoeff;
    });
    // 3. With vias
    cost += (unitSpaceVioCostDiscounted * getWireSegmentUsageOnVias(ts, netIdx).size());
    // 4. Hist cost
    if (histCost) {
        iterateHistWireSegments(ts, [&](const utils::IntervalT<int>& intvl, HistUsageT discountedUsage) {
            DBU dist = layers[ts.layerIdx].getCrossPointRangeDistCost(intvl);
            cost += unitShortVioCostDiscounted[ts.layerIdx] * discountedUsage * dist;
        });
    }
    return cost;
}

CostT RouteGrid::getWrongWayWireSegmentVioCost(const WrongWaySegment& wws, const int netIdx, bool histCost) const {
    CostT cost = 0;
    for (int trackIdx = wws.trackRange.low; trackIdx <= wws.trackRange.high; ++trackIdx) {
        cost += getWireSegmentVioCost({wws.layerIdx, trackIdx, wws.crossPointIdx}, netIdx, histCost);
    }
    return cost;
}

vector<CostT> RouteGrid::getShortWireSegmentCost(const TrackSegment& ts, int netIdx) const {
    const auto& cps = ts.crossPointRange;
    vector<CostT> crossPointCost = getShortWireSegmentVioCost(ts, netIdx);
    for (int cpIdx = cps.low; cpIdx <= cps.high; cpIdx++) {
        DBU dist = layers[ts.layerIdx].getCrossPointRangeDistCost({cpIdx, cpIdx});
        crossPointCost[cpIdx - cps.low] += dist;
    }
    return crossPointCost;
}

vector<CostT> RouteGrid::getShortWireSegmentVioCost(const TrackSegment& ts, int netIdx, bool histCost) const {
    const auto& cps = ts.crossPointRange;
    vector<CostT> crossPointCost(cps.range() + 1, 0);
    // 1. With other wires and pins/obs
    const vector<int>& routedWire = getShortWireSegmentUsageOnOvlpWire(ts, netIdx);
    const vector<int>& poorWire = getShortWireSegmentUsageOnOvlpPoorWire(ts, netIdx);
    for (int cpIdx = cps.low; cpIdx <= cps.high; cpIdx++) {
        DBU dist = layers[ts.layerIdx].getCrossPointRangeDistCost({cpIdx, cpIdx});
        int i = cpIdx - cps.low;
        crossPointCost[i] += (unitShortVioCostDiscounted[ts.layerIdx] * routedWire[i] +
                              unitShortVioCost[ts.layerIdx] * poorWire[i] * setting.dbPoorWirePenaltyCoeff) *
                             dist;
    }
    vector<int> cpLocs = getShortWireSegmentSpaceVioOnWires(ts, netIdx);
    for (int cpIdx : cpLocs) {
        cpIdx = min(max(cps.low, cpIdx), cps.high);
        crossPointCost[cpIdx - cps.low] += unitSpaceVioCostDiscounted;
    }
    // 2. With vias
    vector<int> viaLocs = getWireSegmentUsageOnVias(ts, netIdx);
    for (int cpIdx : viaLocs) {
        cpIdx = min(max(cps.low, cpIdx), cps.high);
        crossPointCost[cpIdx - cps.low] += unitSpaceVioCostDiscounted;
    }
    // 3. Hist cost
    if (histCost) {
        const vector<HistUsageT>& histWire = getShortWireSegmentUsageOnOvlpHistWire(ts);
        for (int cpIdx = cps.low; cpIdx <= cps.high; cpIdx++) {
            DBU dist = layers[ts.layerIdx].getCrossPointRangeDistCost({cpIdx, cpIdx});
            crossPointCost[cpIdx - cps.low] +=
                unitShortVioCostDiscounted[ts.layerIdx] * histWire[cpIdx - cps.low] * dist;
        }
    }
    return crossPointCost;
}

vector<utils::IntervalT<int>> RouteGrid::getEmptyIntvl(const TrackSegment& ts, int netIdx) const {
    int loCPIdx = ts.crossPointRange.low;
    int hiCPIdx = ts.crossPointRange.high;

    vector<CostT> cpVioCost = getShortWireSegmentVioCost(ts, netIdx, false);
    vector<utils::IntervalT<int>> emptyIntervals;

    int begin = -1, end = -1;
    for (int i = 0; i < cpVioCost.size(); i++) {
        if (begin == -1) {
            if (cpVioCost[i] == 0) {
                begin = i;
            }
        } else {
            if (cpVioCost[i] > 0) {
                end = i - 1;
                emptyIntervals.emplace_back(begin + loCPIdx, end + loCPIdx);
                begin = -1;

            } else if (cpVioCost[i] == 0 && i == cpVioCost.size() - 1) {
                end = i;
                emptyIntervals.emplace_back(begin + loCPIdx, end + loCPIdx);
                begin = -1;
            }
        }
    }

    return emptyIntervals;
}

vector<int> RouteGrid::getShortWireSegmentUsageOnOvlpWire(const TrackSegment& ts, int netIdx) const {
    vector<int> crossPointUsage(ts.crossPointRange.range() + 1, 0);
    iterateWireSegments(ts, netIdx, [&](const utils::IntervalT<int>& intvl, int usage) {
        for (int cpIdx = intvl.low; cpIdx <= intvl.high; cpIdx++) {
            crossPointUsage[cpIdx - ts.crossPointRange.low] += usage;
        }
    });
    return crossPointUsage;
}

vector<int> RouteGrid::getShortWireSegmentUsageOnOvlpPoorWire(const TrackSegment& ts, int netIdx) const {
    vector<int> crossPointUsage(ts.crossPointRange.range() + 1, 0);
    iteratePoorWireSegments(ts, netIdx, [&](const utils::IntervalT<int>& intvl) {
        for (int cpIdx = intvl.low; cpIdx <= intvl.high; cpIdx++) {
            ++crossPointUsage[cpIdx - ts.crossPointRange.low];
        }
    });
    return crossPointUsage;
}

vector<HistUsageT> RouteGrid::getShortWireSegmentUsageOnOvlpHistWire(const TrackSegment& ts) const {
    vector<HistUsageT> crossPointUsage(ts.crossPointRange.range() + 1, 0);
    iterateHistWireSegments(ts, [&](const utils::IntervalT<int>& intvl, HistUsageT discountedUsage) {
        for (int cpIdx = intvl.low; cpIdx <= intvl.high; cpIdx++) {
            crossPointUsage[cpIdx - ts.crossPointRange.low] += discountedUsage;
        }
    });
    return crossPointUsage;
}

void RouteGrid::iterateWireSegments(const TrackSegment& ts,
                                    int netIdx,
                                    const std::function<void(const utils::IntervalT<int>&, int)>& handle) const {
    auto queryInterval = boost::icl::interval<int>::closed(ts.crossPointRange.low, ts.crossPointRange.high);
    auto intervals = routedWireMap[ts.layerIdx][ts.trackIdx].equal_range(queryInterval);
    for (auto it = intervals.first; it != intervals.second; ++it) {
        auto mergedInterval = it->first & queryInterval;
        int usage = it->second.size();
        if (it->second.find(netIdx) != it->second.end()) {
            --usage;
        }
        if (usage > 0) {
            handle({first(mergedInterval), last(mergedInterval)}, usage);
        }
    }
}

void RouteGrid::iteratePoorWireSegments(const TrackSegment& ts,
                                        int netIdx,
                                        const std::function<void(const utils::IntervalT<int>&)>& handle) const {
    auto queryInterval = boost::icl::interval<int>::closed(ts.crossPointRange.low, ts.crossPointRange.high);
    auto intervals = poorWireMap[ts.layerIdx][ts.trackIdx].equal_range(queryInterval);
    for (auto it = intervals.first; it != intervals.second; ++it) {
        auto mergedInterval = it->first & queryInterval;
        if (netIdx != it->second.netIdx) {
            handle({first(mergedInterval), last(mergedInterval)});
        }
    }
}

void RouteGrid::iterateHistWireSegments(
    const TrackSegment& ts, const std::function<void(const utils::IntervalT<int>&, HistUsageT)>& handle) const {
    auto queryInterval = boost::icl::interval<int>::closed(ts.crossPointRange.low, ts.crossPointRange.high);
    auto intervals = histWireMap[ts.layerIdx][ts.trackIdx].equal_range(queryInterval);
    for (auto it = intervals.first; it != intervals.second; ++it) {
        auto mergedInterval = it->first & queryInterval;
        handle({first(mergedInterval), last(mergedInterval)}, it->second);
    }
}

vector<int> RouteGrid::getWireSegmentSpaceVioOnWires(const TrackSegment& ts, int netIdx) const {
    vector<int> vioCPs;
    const auto& cps = ts.crossPointRange;
    const auto& wireRange = layers[ts.layerIdx].wireRange;
    if (wireRange[cps.low].low < 0) {
        auto queryInterval = boost::icl::interval<int>::closed(cps.low + wireRange[cps.low].low, cps.low - 1);
        auto intervals = routedWireMap[ts.layerIdx][ts.trackIdx].equal_range(queryInterval);
        if (intervals.first != intervals.second) {
            auto it = intervals.second;
            --it;
            int prevEnd = last(it->first);
            if (it->second.find(netIdx) == it->second.end() && prevEnd < cps.low) {  // no ovlp/short
                for (int cpIdx = cps.low; cpIdx <= prevEnd + wireRange[prevEnd].high; ++cpIdx) {
                    vioCPs.push_back(cpIdx);
                }
            }
        }
    }
    if (wireRange[cps.high].high > 0) {
        auto queryInterval = boost::icl::interval<int>::closed(cps.high + 1, cps.high + wireRange[cps.high].high);
        auto intervals = routedWireMap[ts.layerIdx][ts.trackIdx].equal_range(queryInterval);
        if (intervals.first != intervals.second) {
            auto it = intervals.first;
            int nextEnd = first(it->first);
            if (it->second.find(netIdx) == it->second.end() && nextEnd > cps.high) {  // no ovlp/short
                for (int cpIdx = nextEnd + wireRange[nextEnd].low; cpIdx <= cps.high; ++cpIdx) {
                    vioCPs.push_back(cpIdx);
                }
            }
        }
    }
    return vioCPs;
}

vector<int> RouteGrid::getShortWireSegmentSpaceVioOnWires(const TrackSegment& ts, int netIdx) const {
    vector<int> vioCPs;
    const auto& cps = ts.crossPointRange;
    const auto& wireRange = layers[ts.layerIdx].wireRange;
    auto queryInterval =
        boost::icl::interval<int>::closed(cps.low + wireRange[cps.low].low, cps.high + wireRange[cps.high].high);
    auto intervals = routedWireMap[ts.layerIdx][ts.trackIdx].equal_range(queryInterval);
    for (auto it = intervals.first; it != intervals.second; ++it) {
        int low = first(it->first), high = last(it->first);
        for (int cpIdx = low + wireRange[low].low; cpIdx < low; ++cpIdx) {
            if (cpIdx >= cps.low && cpIdx <= cps.high) vioCPs.push_back(cpIdx);
        }
        for (int cpIdx = high + wireRange[high].high; cpIdx > high; --cpIdx) {
            if (cpIdx >= cps.low && cpIdx <= cps.high) vioCPs.push_back(cpIdx);
        }
    }
    return vioCPs;
}

vector<int> RouteGrid::getWireSegmentUsageOnVias(const TrackSegment& ts, int netIdx) const {
    vector<int> viaCPs;
    if ((ts.layerIdx + 1) != layers.size()) {
        // top vias
        getWireSegmentUsageOnVias(
            ts, netIdx, layers[ts.layerIdx].wireTopVia, cutLayers[ts.layerIdx].viaBotWire, routedViaMap, viaCPs);
    }
    if (ts.layerIdx != 0) {
        // bot vias
        getWireSegmentUsageOnVias(ts,
                                  netIdx,
                                  layers[ts.layerIdx].wireBotVia,
                                  cutLayers[ts.layerIdx - 1].viaTopWire,
                                  routedViaMapUpper,
                                  viaCPs);
    }
    return viaCPs;
}

void RouteGrid::getWireSegmentUsageOnVias(const TrackSegment& ts,
                                          int netIdx,
                                          const vector<vector<vector<bool>>>& wireVia,
                                          const vector<vector<vector<bool>>>& viaWire,
                                          const ViaMapT& netsOnVias,
                                          vector<int>& viaCPs) const {
    int layerIdx = ts.layerIdx;
    const MetalLayer& layer = layers[layerIdx];
    const int xSize =
        ((int)max(wireVia[ts.crossPointRange.low].size(), wireVia[ts.crossPointRange.high].size()) - 1) / 2;
    const int ySize =
        ((int)max(wireVia[ts.crossPointRange.low][0].size(), wireVia[ts.crossPointRange.high][0].size()) - 1) / 2;

    std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> itRange;

    for (unsigned i = max(0, ts.trackIdx - xSize); i < min(layer.numTracks(), ts.trackIdx + xSize + 1); ++i) {
        itRange.first = netsOnVias[layerIdx][i].lower_bound(max(0, ts.crossPointRange.low - ySize));
        itRange.second =
            netsOnVias[layerIdx][i].upper_bound(min(layer.numCrossPoints() - 1, ts.crossPointRange.high + ySize));

        for (std::multimap<int, int>::const_iterator it = itRange.first; it != itRange.second; ++it) {
            if (it->second != netIdx) {
                int viaCP = it->first;
                int viaWireXSize = (viaWire[viaCP].size() - 1) / 2;
                int viaWireYSize = (viaWire[viaCP][0].size() - 1) / 2;
                for (unsigned j = max(ts.crossPointRange.low, viaCP - viaWireYSize);
                     j < min(ts.crossPointRange.high, viaCP + viaWireYSize + 1);
                     ++j) {
                    if (viaWire[viaCP][ts.trackIdx + viaWireXSize - i][j + viaWireYSize - viaCP]) {
                        viaCPs.push_back(j);
                    }
                }
            }
        }
    }
}

vector<std::pair<utils::BoxT<DBU>, int>> RouteGrid::getOvlpBoxes(const BoxOnLayer& box,
                                                                 int netIdx,
                                                                 const RTrees& rtrees) const {
    boostBox queryBox(boostPoint(box.x.low, box.y.low), boostPoint(box.x.high, box.y.high));
    vector<std::pair<boostBox, int>> queryResults;
    rtrees[box.layerIdx].query(bgi::intersects(queryBox), std::back_inserter(queryResults));
    vector<std::pair<utils::BoxT<DBU>, int>> results;
    for (const auto& queryResult : queryResults) {
        if (queryResult.second != netIdx) {
            const auto& b = queryResult.first;
            results.emplace_back(utils::BoxT<DBU>(bg::get<bg::min_corner, 0>(b),
                                                  bg::get<bg::min_corner, 1>(b),
                                                  bg::get<bg::max_corner, 0>(b),
                                                  bg::get<bg::max_corner, 1>(b)),
                                 queryResult.second);
        }
    }
    return results;
}

vector<std::pair<utils::BoxT<DBU>, int>> RouteGrid::getOvlpBoxes(const BoxOnLayer& box, const RTrees& rtrees) const {
    boostBox queryBox(boostPoint(box.x.low, box.y.low), boostPoint(box.x.high, box.y.high));
    vector<std::pair<boostBox, int>> queryResults;
    rtrees[box.layerIdx].query(bgi::intersects(queryBox), std::back_inserter(queryResults));
    vector<std::pair<utils::BoxT<DBU>, int>> results;
    for (const auto& queryResult : queryResults) {
        const auto& b = queryResult.first;
        results.emplace_back(utils::BoxT<DBU>(bg::get<bg::min_corner, 0>(b),
                                              bg::get<bg::min_corner, 1>(b),
                                              bg::get<bg::max_corner, 0>(b),
                                              bg::get<bg::max_corner, 1>(b)),
                             queryResult.second);
    }
    return results;
}

vector<std::pair<utils::BoxT<DBU>, int>> RouteGrid::getOvlpFixedMetals(const BoxOnLayer& box, int netIdx) const {
    return getOvlpBoxes(box, netIdx, fixedMetals);
}

vector<std::pair<utils::BoxT<DBU>, int>> RouteGrid::getOvlpFixedMetals(const BoxOnLayer& box) const {
    return getOvlpBoxes(box, fixedMetals);
}

vector<std::pair<utils::BoxT<DBU>, int>> RouteGrid::getOvlpFixedMetalForbidRegions(const BoxOnLayer& box,
                                                                                   int netIdx) const {
    return getOvlpBoxes(box, netIdx, fixedMetalForbidRegions);
}

vector<std::pair<utils::BoxT<DBU>, int>> RouteGrid::getOvlpFixedMetalForbidRegions(const BoxOnLayer& box) const {
    return getOvlpBoxes(box, fixedMetalForbidRegions);
}

vector<std::pair<utils::BoxT<DBU>, int>> RouteGrid::getOvlpC2CMetals(const BoxOnLayer& box, int netIdx) const {
    auto indices = getOvlpBoxes(box, fixedMetalC2CForbidRegions);
    vector<std::pair<utils::BoxT<DBU>, int>> results;
    for (const auto& pair : indices) {
        int index = pair.second;
        if (fixedMetalVec[index].second != netIdx)
            results.emplace_back(fixedMetalVec[index].first, fixedMetalVec[index].second);
    }
    return results;
}

vector<std::pair<utils::BoxT<DBU>, int>> RouteGrid::getOvlpC2CMetals(const BoxOnLayer& box) const {
    auto indices = getOvlpBoxes(box, fixedMetalC2CForbidRegions);
    vector<std::pair<utils::BoxT<DBU>, int>> results;
    for (const auto& pair : indices) {
        int index = pair.second;
        results.emplace_back(fixedMetalVec[index].first, fixedMetalVec[index].second);
    }
    return results;
}

DBU RouteGrid::getOvlpArea(const BoxOnLayer& box, int netIdx, const RTrees& rtrees) const {
    DBU area = 0;
    auto regions = getOvlpBoxes(box, netIdx, rtrees);
    for (const auto& region : regions) {
        auto ovlp = box.IntersectWith(region.first);
        if (ovlp.IsValid()) {
            area += ovlp.area();
        }
    }
    return area;
}

DBU RouteGrid::getOvlpFixedMetalArea(const BoxOnLayer& box, int netIdx) const {
    return getOvlpArea(box, netIdx, fixedMetals);
}

DBU RouteGrid::getOvlpFixedMetalForbidRegionArea(const BoxOnLayer& box, int netIdx) const {
    return getOvlpArea(box, netIdx, fixedMetalForbidRegions);
}

void RouteGrid::useEdge(const GridEdge& edge, int netIdx) {
    if (edge.isVia()) {
        return useVia(edge.lowerGridPoint(), netIdx);
    } else if (edge.isTrackSegment()) {
        return useWireSegment({edge}, netIdx);
    } else if (edge.isWrongWaySegment()) {
        return useWrongWayWireSegment({edge}, netIdx);
    } else {
        log() << "Warning in RouteGrid::useEdge: invalid edge type" << std::endl;
    }
}

void RouteGrid::useVia(const GridPoint& via, int netIdx, ViaMapT& netsOnVias) {
    netsOnVias[via.layerIdx][via.trackIdx].emplace(via.crossPointIdx, netIdx);
}

void RouteGrid::useVia(const GridPoint& via, int netIdx) {
    viaLocks[via.layerIdx][via.trackIdx].lock();
    useVia(via, netIdx, routedViaMap);
    viaLocks[via.layerIdx][via.trackIdx].unlock();
    auto upper = getUpper(via);
    viaLocksUpper[upper.layerIdx][upper.trackIdx].lock();
    useVia(upper, netIdx, routedViaMapUpper);
    viaLocksUpper[upper.layerIdx][upper.trackIdx].unlock();
}

void RouteGrid::useWireSegment(const TrackSegment& ts, int netIdx) {
    wireLocks[ts.layerIdx][ts.trackIdx].lock();
    auto iclRange = boost::icl::interval<int>::closed(ts.crossPointRange.low, ts.crossPointRange.high);
    routedWireMap[ts.layerIdx][ts.trackIdx].add({iclRange, {netIdx}});
    wireLocks[ts.layerIdx][ts.trackIdx].unlock();
}

void RouteGrid::useWrongWayWireSegment(const WrongWaySegment& wws, int netIdx) {
    for (int trackIdx = wws.trackRange.low; trackIdx <= wws.trackRange.high; ++trackIdx) {
        useWireSegment({wws.layerIdx, trackIdx, wws.crossPointIdx}, netIdx);
    }
}

void RouteGrid::usePoorWireSegment(const TrackSegment& ts, int netIdx) {
    auto iclRange = boost::icl::interval<int>::closed(ts.crossPointRange.low, ts.crossPointRange.high);
    poorWireMap[ts.layerIdx][ts.trackIdx].add({iclRange, netIdx});
}

void RouteGrid::useHistWireSegment(const TrackSegment& ts, HistUsageT usage) {
    auto iclRange = boost::icl::interval<int>::closed(ts.crossPointRange.low, ts.crossPointRange.high);
    histWireMap[ts.layerIdx][ts.trackIdx].add({iclRange, usage});
}

void RouteGrid::markFixedMetal(int fixedObjectIdx) {
    // fixedMetals
    const BoxOnLayer& box = fixedMetalVec[fixedObjectIdx].first;
    int netIdx = fixedMetalVec[fixedObjectIdx].second;

    boostBox markBox(boostPoint(box.x.low, box.y.low), boostPoint(box.x.high, box.y.high));
    fixedMetals[box.layerIdx].insert({markBox, netIdx});
    // fixedMetalForbigRegions
    auto regions = getAccurateMetalRectForbidRegions(box);
    for (const auto& region : regions) {
        boostBox markRegion(boostPoint(region.x.low, region.y.low), boostPoint(region.x.high, region.y.high));
        fixedMetalForbidRegions[box.layerIdx].insert({markRegion, netIdx});
    }

    // fixedMetalC2CForbidRegions
    // assume the forbid regions are two rect
    if (!layers[box.layerIdx].isEolActive(box)) {
        DBU space = layers[box.layerIdx].getSpace(box);
        boostBox spaceBox(boostPoint(box.x.low - space, box.y.low - space),
                          boostPoint(box.x.high + space, box.y.high + space));
        fixedMetalC2CForbidRegions[box.layerIdx].insert({spaceBox, fixedObjectIdx});
    }
}

void RouteGrid::markFixedMetalBatch() {
    vector<vector<int>> layerToObjIdx(getLayerNum());
    for (unsigned i = 0; i < fixedMetalVec.size(); i++) layerToObjIdx[fixedMetalVec[i].first.layerIdx].push_back(i);

    int curLayer = 0;
    std::mutex layer_mutex;

    auto thread_func = [&]() {
        while (true) {
            layer_mutex.lock();
            int l = curLayer++;
            layer_mutex.unlock();

            if (l >= getLayerNum()) return;

            for (auto idx : layerToObjIdx[l]) markFixedMetal(idx);
        }
    };

    const int numThreads = max(1, db::setting.numThreads);
    std::thread threads[numThreads];
    for (int i = 0; i < numThreads; i++) threads[i] = std::thread(thread_func);
    for (int i = 0; i < numThreads; i++) threads[i].join();
}

void RouteGrid::removeEdge(const GridEdge& edge, int netIdx) {
    if (edge.isVia()) {
        return removeVia(edge.lowerGridPoint(), netIdx);
    } else if (edge.isTrackSegment()) {
        return removeWireSegment(TrackSegment(edge), netIdx);
    } else if (edge.isWrongWaySegment()) {
        return removeWrongWayWireSegment({edge}, netIdx);
    } else {
        log() << "Warning in RouteGrid::removeEdge: invalid edge type" << std::endl;
    }
}

void RouteGrid::removeVia(const GridPoint& via, int netIdx, ViaMapT& netsOnVias) {
    unsigned usage = 0;
    std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> itRange =
        netsOnVias[via.layerIdx][via.trackIdx].equal_range(via.crossPointIdx);
    for (std::multimap<int, int>::const_iterator it = itRange.first; it != itRange.second; ++it) {
        if (it->second == netIdx) {
            netsOnVias[via.layerIdx][via.trackIdx].erase(it);
            break;
        }
    }
}

void RouteGrid::removeVia(const GridPoint& via, int netIdx) {
    viaLocks[via.layerIdx][via.trackIdx].lock();
    removeVia(via, netIdx, routedViaMap);
    viaLocks[via.layerIdx][via.trackIdx].unlock();
    viaLocksUpper[via.layerIdx][via.trackIdx].lock();
    removeVia(getUpper(via), netIdx, routedViaMapUpper);
    viaLocksUpper[via.layerIdx][via.trackIdx].unlock();
}

void RouteGrid::removeWireSegment(const TrackSegment& ts, int netIdx) {
    auto queryInterval = boost::icl::interval<int>::closed(ts.crossPointRange.low, ts.crossPointRange.high);
    routedWireMap[ts.layerIdx][ts.trackIdx].subtract({queryInterval, {netIdx}});
}

void RouteGrid::removeWrongWayWireSegment(const WrongWaySegment& wws, int netIdx) {
    for (int trackIdx = wws.trackRange.low; trackIdx <= wws.trackRange.high; ++trackIdx) {
        removeWireSegment({wws.layerIdx, trackIdx, wws.crossPointIdx}, netIdx);
    }
}

void RouteGrid::printAllUsageAndVio() const {
    constexpr int width = 10;
    auto wlVia = printAllUsage();
    auto shortSpace = printAllVio();
    log() << "--- Estimated Scores ---" << std::endl;
    vector<std::string> items = {"wirelength", "# vias", "short", "space"};
    vector<double> metrics = {wlVia.first, wlVia.second, shortSpace.first, shortSpace.second};
    vector<double> weights = {
        setting.weightWirelength, setting.weightViaNum, setting.weightShortArea, setting.weightSpaceVioNum};
    double totalScore = 0;
    for (int i = 0; i < items.size(); ++i) {
        totalScore += metrics[i] * weights[i];
    }
    log() << std::setw(width) << "item"
          << " | " << std::setw(width) << "metric"
          << " | " << std::setw(width) << "weight"
          << " | " << std::setw(width) << "score"
          << " | " << std::setw(width) << "\%" << std::endl;
    for (int i = 0; i < items.size(); ++i) {
        double score = metrics[i] * weights[i];
        log() << std::setw(width) << items[i] << " | " << std::setw(width) << metrics[i] << " | " << std::setw(width)
              << weights[i] << " | " << std::setw(width) << score << " | " << std::setw(width) << score / totalScore
              << std::endl;
    }
    log() << "total score = " << totalScore << std::endl;
}

void RouteGrid::getAllWireUsage(const vector<int>& buckets,
                                vector<int>& wireUsageGrid,
                                vector<DBU>& wireUsageLength) const {
    wireUsageGrid.assign(buckets.size(), 0);
    wireUsageLength.assign(buckets.size(), 0);
    for (int layerIdx = 0; layerIdx < getLayerNum(); ++layerIdx) {
        for (const auto& track : routedWireMap[layerIdx]) {
            for (const auto& intvlUsage : track) {
                const auto& intvl = intvlUsage.first;
                int usage = intvlUsage.second.size();
                int numGrids = (last(intvl) - first(intvl) + 1);
                DBU dist = layers[layerIdx].getCrossPointRangeDistCost({first(intvl), last(intvl)});
                int bucketIdx = buckets.size() - 1;
                while (buckets[bucketIdx] > usage) --bucketIdx;
                wireUsageGrid[bucketIdx] += numGrids;
                wireUsageLength[bucketIdx] += dist;
            }
        }
    }
}

void RouteGrid::getAllViaUsage(const vector<int>& buckets, const ViaMapT& netsOnVias, vector<int>& viaUsage) const {
    viaUsage.assign(buckets.size(), 0);
    for (unsigned layerIdx = 0; (layerIdx + 1) < getLayerNum(); ++layerIdx) {
        for (const auto& track : netsOnVias[layerIdx]) {
            std::unordered_map<int, int> posUsages;
            for (const auto& via : track) {
                ++posUsages[via.first];
            }
            for (const auto& usage : posUsages) {
                ++viaUsage[usage.second];
            }
        }
    }
}

std::pair<double, double> RouteGrid::printAllUsage() const {
    constexpr int width = 10;
    vector<int> buckets = {0, 1, 2, 3, 5, 10};  // the i-th bucket: buckets[i] <= x < buckets[i+1]

    // Wire
    vector<int> routedWireUsageGrid;
    vector<DBU> routedWireUsageLength;
    getAllWireUsage(buckets, routedWireUsageGrid, routedWireUsageLength);
    log() << "--- Wire Usage ---" << std::endl;
    log() << "Among " << numGridPoints << " grid points and " << totalTrackLength / double(layers[1].pitch)
          << "-long tracks (length is normalized by M2 pitch): " << std::endl;
    log() << std::setw(width) << "usage"
          << " | " << std::setw(width) << "   grid   "
          << " | " << std::setw(width) << "  length  " << std::endl;
    double wireLength = 0;
    for (int i = 1; i < buckets.size(); ++i) {
        if (routedWireUsageGrid[i] == 0 && routedWireUsageLength[i] == 0) {
            continue;
        }
        log() << std::setw(width) << getRangeStr(buckets, i) << " | " << std::setw(width) << routedWireUsageGrid[i]
              << " | " << std::setw(width) << routedWireUsageLength[i] / double(layers[1].pitch) << std::endl;
        wireLength += i * routedWireUsageLength[i] / double(layers[1].pitch);
    }

    // Via
    vector<int> routedViaUsage;
    getAllViaUsage(buckets, routedViaMap, routedViaUsage);
    log() << "--- Via Usage ---" << std::endl;
    log() << "Among " << numVias << " via candidates --- " << std::endl;
    log() << std::setw(width) << "usage"
          << " | " << std::setw(width) << "routed" << std::endl;
    double viaNum = 0;
    for (int i = 1; i < buckets.size(); ++i) {
        if (routedViaUsage[i] == 0) {
            continue;
        }
        log() << std::setw(width) << getRangeStr(buckets, i) << " | " << std::setw(width) << routedViaUsage[i]
              << std::endl;
        viaNum += i * routedViaUsage[i];
    }

    return {wireLength, viaNum};
}

std::string RouteGrid::getRangeStr(const vector<int>& buckets, int i) const {
    std::string range;
    if ((i + 1) < buckets.size()) {
        if ((buckets[i] + 1) == buckets[i + 1]) {
            range = std::to_string(buckets[i]);
        } else {
            range = std::to_string(buckets[i]) + "~" + std::to_string(buckets[i + 1] - 1);
        }
    } else {
        range = std::to_string(buckets[i]) + "~inf";
    }
    return range;
}

void RouteGrid::getAllViaVio(vector<int>& sameLayerViaVios,
                             vector<int>& viaTopViaVios,
                             vector<int>& viaBotWireVios,
                             vector<int>& viaTopWireVios,
                             vector<int>& poorVia) const {
    sameLayerViaVios.assign(getLayerNum() - 1, 0);
    viaTopViaVios.assign(getLayerNum() - 1, 0);
    viaBotWireVios.assign(getLayerNum() - 1, 0);
    viaTopWireVios.assign(getLayerNum() - 1, 0);
    poorVia.assign(getLayerNum() - 1, 0);
    for (unsigned layerIdx = 0; (layerIdx + 1) != getLayerNum(); ++layerIdx) {
        for (unsigned trackIdx = 0; trackIdx != layers[layerIdx].numTracks(); ++trackIdx) {
            for (const std::pair<int, int>& p : routedViaMap[layerIdx][trackIdx]) {
                GridPoint via(layerIdx, trackIdx, p.first);
                sameLayerViaVios[layerIdx] += getViaUsageOnSameLayerVias(via, p.second);
                viaTopViaVios[layerIdx] += getViaUsageOnTopLayerVias(via, p.second);
                viaBotWireVios[layerIdx] += getViaUsageOnWires(cutLayers[layerIdx].viaBotWire, via, p.second);
                viaTopWireVios[layerIdx] += getViaUsageOnWires(cutLayers[layerIdx].viaTopWire, getUpper(via), p.second);
                if (getViaPoorness(via, p.second) == ViaPoorness::Poor) {
                    ++poorVia[layerIdx];
                }
            }
        }
        sameLayerViaVios[layerIdx] /= 2;
    }
}

void RouteGrid::getAllWireShortVio(vector<int>& shortNum, vector<DBU>& shortLen) const {
    shortNum.assign(getLayerNum(), 0);
    shortLen.assign(getLayerNum(), 0.0);
    for (int layerIdx = 0; layerIdx < getLayerNum(); ++layerIdx) {
        for (const auto& track : routedWireMap[layerIdx]) {
            for (const auto& intvlUsage : track) {
                const auto& intvl = intvlUsage.first;
                int usage = intvlUsage.second.size();
                if (usage > 1) {
                    shortNum[layerIdx] += (usage - 1);
                    shortLen[layerIdx] +=
                        (usage - 1) * layers[layerIdx].getCrossPointRangeDistCost({first(intvl), last(intvl)});
                }
            }
        }
    }
}

void RouteGrid::getAllPoorWire(vector<int>& num, vector<DBU>& len) const {
    num.assign(getLayerNum(), 0);
    len.assign(getLayerNum(), 0.0);
    for (int layerIdx = 0; layerIdx < getLayerNum(); ++layerIdx) {
        for (int trackIdx = 0; trackIdx < routedWireMap[layerIdx].size(); ++trackIdx) {
            for (const auto& intvlUsage : routedWireMap[layerIdx][trackIdx]) {
                const auto& intvl = intvlUsage.first;
                int usage = intvlUsage.second.size();
                iteratePoorWireSegments({layerIdx, trackIdx, {first(intvl), last(intvl)}},
                                        *intvlUsage.second.begin(),
                                        [&](const utils::IntervalT<int>& poorIntvl) {
                                            ++num[layerIdx];
                                            len[layerIdx] += layers[layerIdx].getCrossPointRangeDistCost(poorIntvl);
                                        });
            }
        }
    }
}

void RouteGrid::getAllWireSpaceVio(vector<int>& spaceVioNum) const {
    spaceVioNum.assign(getLayerNum(), 0);
    for (int layerIdx = 0; layerIdx < getLayerNum(); ++layerIdx) {
        for (int trackIdx = 0; trackIdx != layers[layerIdx].numTracks(); ++trackIdx) {
            for (const auto& intvlUsage : routedWireMap[layerIdx][trackIdx]) {
                for (int netIdx : intvlUsage.second) {
                    const auto& intvl = intvlUsage.first;
                    auto vioCPs =
                        getWireSegmentSpaceVioOnWires({layerIdx, trackIdx, {first(intvl), last(intvl)}}, netIdx);
                    spaceVioNum[layerIdx] += vioCPs.size();
                }
            }
        }
        spaceVioNum[layerIdx] /= 2;
    }
}

std::pair<double, double> RouteGrid::printAllVio() const {
    constexpr int width = 10;
    auto sumVec = [](const vector<int>& vec) {
        int sum = 0;
        for (int val : vec) {
            sum += val;
        }
        return sum;
    };

    // Wire violations
    vector<int> routedShortNum, poorNum;
    vector<DBU> routedShortLen, poorLen;
    getAllWireShortVio(routedShortNum, routedShortLen);
    getAllPoorWire(poorNum, poorLen);
    vector<int> wireSpaceNum;
    getAllWireSpaceVio(wireSpaceNum);
    log() << "--- Wire-Wire Short/Spacing Vios Vios ---" << std::endl;
    log() << std::setw(width) << "usage"
          << " | " << std::setw(width) << "# space"
          << " | " << std::setw(width * 2 + 3) << "       # short       "
          << " | " << std::setw(width * 2 + 3) << "      short area     " << std::endl;
    log() << std::setw(width) << "layer"
          << " | " << std::setw(width) << "wire-wire"
          << " | " << std::setw(width) << "wire-wire"
          << " | " << std::setw(width) << "wire-fixed"
          << " | " << std::setw(width) << "wire-wire"
          << " | " << std::setw(width) << "wire-fixed" << std::endl;
    double routedShortArea = 0;
    double poorVioArea = 0;
    for (int i = 0; i < getLayerNum(); ++i) {
        if (wireSpaceNum[i] == 0 && routedShortNum[i] == 0 && poorNum[i] == 0) continue;
        double routedArea = double(routedShortLen[i]) * layers[i].width / layers[1].pitch / layers[1].pitch;
        double poorArea = double(poorLen[i]) * layers[i].width / layers[1].pitch / layers[1].pitch;
        log() << std::setw(width) << getLayer(i).name << " | " << std::setw(width) << wireSpaceNum[i] << " | "
              << std::setw(width) << routedShortNum[i] << " | " << std::setw(width) << poorNum[i] << " | "
              << std::setw(width) << routedArea << " | " << std::setw(width) << poorArea << std::endl;
        routedShortArea += routedArea;
        poorVioArea += poorArea;
    }
    int numSpaceVio = sumVec(wireSpaceNum);
    log() << std::setw(width) << "SumW"
          << " | " << std::setw(width) << numSpaceVio << " | " << std::setw(width) << sumVec(routedShortNum) << " | "
          << std::setw(width) << sumVec(poorNum) << " | " << std::setw(width) << routedShortArea << " | "
          << std::setw(width) << poorVioArea << std::endl;
    log() << std::setw(width) << "BigSumW"
          << " | " << std::setw(width) << numSpaceVio << " | " << std::setw(width * 2 + 3)
          << sumVec(poorNum) + sumVec(routedShortNum) << " | " << std::setw(width * 2 + 3)
          << poorVioArea + routedShortArea << std::endl;

    // Via violations
    vector<int> sameLayerViaVios, viaTopViaVios, viaBotWireVios, viaTopWireVios, poorVia;
    getAllViaVio(sameLayerViaVios, viaTopViaVios, viaBotWireVios, viaTopWireVios, poorVia);
    log() << "--- Via-Via/Wire Short/Spacing Vios ---" << std::endl;
    log() << std::setw(width) << "layer"
          << " | " << std::setw(width * 2 + 3) << "     via-via     "
          << " | " << std::setw(width * 2 + 3) << "     via-wire    "
          << " | " << std::setw(width) << "via-fixed" << std::endl;
    log() << std::setw(width) << ""
          << " | " << std::setw(width) << "same"
          << " | " << std::setw(width) << "top"
          << " | " << std::setw(width) << "bot"
          << " | " << std::setw(width) << "top"
          << " | " << std::endl;
    for (int i = 0; (i + 1) < getLayerNum(); ++i) {
        if (sameLayerViaVios[i] == 0 && viaTopViaVios[i] == 0 && viaBotWireVios[i] == 0 && viaTopWireVios[i] == 0 &&
            poorVia[i] == 0) {
            continue;
        }
        log() << std::setw(width) << getCutLayer(i).name << " | " << std::setw(width) << sameLayerViaVios[i] << " | "
              << std::setw(width) << viaTopViaVios[i] << " | " << std::setw(width) << viaBotWireVios[i] << " | "
              << std::setw(width) << viaTopWireVios[i] << " | " << std::setw(width) << poorVia[i] << std::endl;
    }
    log() << std::setw(width) << "SumV"
          << " | " << std::setw(width) << sumVec(sameLayerViaVios) << " | " << std::setw(width) << sumVec(viaTopViaVios)
          << " | " << std::setw(width) << sumVec(viaBotWireVios) << " | " << std::setw(width) << sumVec(viaTopWireVios)
          << " | " << std::setw(width) << sumVec(poorVia) << std::endl;
    int numViaViaVio = sumVec(sameLayerViaVios) + sumVec(viaTopViaVios);
    int numViaWireVio = sumVec(viaBotWireVios) + sumVec(viaTopWireVios);
    log() << std::setw(width) << "BigSumV"
          << " | " << std::setw(width * 2 + 3) << numViaViaVio << " | " << std::setw(width * 2 + 3) << numViaWireVio
          << " | " << std::setw(width) << sumVec(poorVia) << std::endl;
    double spaceVioNum = numSpaceVio + numViaViaVio + numViaWireVio + sumVec(poorVia);

    return {poorVioArea + routedShortArea, spaceVioNum};
}

CostT RouteGrid::getNetVioCost(const Net& net) const {
    CostT cost = 0.0;
    net.postOrderVisitGridTopo([&](std::shared_ptr<GridSteiner> node) {
        if (node->parent) {
            cost += getEdgeVioCost({*node, *(node->parent)}, net.idx, false);
        }
    });
    cost += unitMinAreaVioCost * (net.minAreaVio + net.minAreaShadowedVio);
    cost += unitSpaceVioCost * net.viaPinVio;  // TODO: remove
    return cost;
}

void RouteGrid::addHistCost() {
    if (setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("Add hist cost");
    }
    addWireHistCost();
    addViaHistCost();
}

void RouteGrid::addWireHistCost() {
    for (int layerIdx = 0; layerIdx < getLayerNum(); ++layerIdx) {
        for (int trackIdx = 0; trackIdx < layers[layerIdx].numTracks(); ++trackIdx) {
            for (const auto& intvlUsage : routedWireMap[layerIdx][trackIdx]) {
                const auto& intvl = intvlUsage.first;
                TrackSegment ts{layerIdx, trackIdx, {first(intvl), last(intvl)}};
                int usage = intvlUsage.second.size();
                if (usage > 1) {
                    useHistWireSegment(ts, 1.0);
                }
                for (int netIdx : intvlUsage.second) {
                    vector<int> viaLocs = getWireSegmentUsageOnVias(ts, netIdx);
                    for (int cpIdx : viaLocs) {
                        cpIdx = min(max(first(intvl), cpIdx), last(intvl));
                        useHistWireSegment({layerIdx, trackIdx, {cpIdx, cpIdx}}, 1.0);
                    }
                }
                // TODO: wire-wire spacing violations
            }
        }
    }
}

void RouteGrid::addViaHistCost() {
    for (unsigned layerIdx = 0; (layerIdx + 1) != getLayerNum(); ++layerIdx) {
        for (unsigned trackIdx = 0; trackIdx != layers[layerIdx].numTracks(); ++trackIdx) {
            for (const std::pair<int, int>& p : routedViaMap[layerIdx][trackIdx]) {
                GridPoint via(layerIdx, trackIdx, p.first);
                if (getViaUsageOnSameLayerVias(via, p.second) > 0 ||
                    getViaUsageOnTopLayerVias(via, p.second) > 0) {  // ||
                    // getViaUsageOnWires(cutLayers[layerIdx].viaBotWire, via, p.second) > 0 ||
                    // getViaUsageOnWires(cutLayers[layerIdx].viaTopWire, getUpper(via), p.second)) {
                    histViaMap[layerIdx][trackIdx][p.first] += 1.0;
                }
            }
        }
    }
}

void RouteGrid::fadeHistCost() {
    if (setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("Fade hist cost by", setting.rrrFadeCoeff, "...");
    }
    std::map<CostT, int> histWireUsage, histViaUsage;
    for (int layerIdx = 0; layerIdx < getLayerNum(); ++layerIdx) {
        for (int trackIdx = 0; trackIdx < layers[layerIdx].numTracks(); ++trackIdx) {
            // wire
            for (auto& intvl : histWireMap[layerIdx][trackIdx]) {
                intvl.second *= setting.rrrFadeCoeff;
                ++histWireUsage[intvl.second];
            }
            // via
            for (auto& p : histViaMap[layerIdx][trackIdx]) {
                p.second *= setting.rrrFadeCoeff;
                ++histViaUsage[p.second];
            }
        }
    }
    if (setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("Hist wire usage after fading is", histWireUsage);
        printlog("Hist via usage after fading is", histViaUsage);
    }
}

}  // namespace db
