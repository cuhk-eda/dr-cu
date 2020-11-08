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

void RouteGrid::clear() {
    // Fixed metal
    fixedMetals.clear();
    // Wire
    routedWireMap.clear();
    poorWireMap.clear();
    histWireMap.clear();
    wireLocks.clear();
    // Via
    routedViaMap.clear();       // the last layer will not be used
    routedViaMapUpper.clear();  // the first layer will not be used
    viaLocks.clear();           // the last layer will not be used
    viaLocksUpper.clear();      // the first layer will not be used
    histViaMap.clear();         // the last layer will not be used
    for (auto& layer : poorViaMap) {
        for (auto& track : layer) {
            for (auto& intvl : track) {
                delete intvl.second;
            }
        }
    }
    poorViaMap.clear();
    usePoorViaMap.clear();
}

void RouteGrid::stash() {
    histWireMap_copy = histWireMap;
    histViaMap_copy = histViaMap;
}

void RouteGrid::reset() {
    histWireMap = histWireMap_copy;
    histViaMap = histViaMap_copy;
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
        auto viaType = getViaType(edge.lowerGridPoint());
        return getViaVioCost(edge.lowerGridPoint(), netIdx, histCost, viaType);
    } else if (edge.isTrackSegment()) {
        return getWireSegmentVioCost({edge}, netIdx, histCost);
    } else if (edge.isWrongWaySegment()) {
        return getWrongWayWireSegmentVioCost({edge}, netIdx, histCost);
    } else {
        log() << "Warning in RouteGrid::getEdgeVioCost: invalid edge type" << std::endl;
        return 0;
    }
}

CostT RouteGrid::getViaVioCost(const GridPoint& via, const int netIdx, bool histCost, const ViaType* viaType) const {
    unsigned vioWithViaWire = getViaUsageOnVias(via, netIdx) + getViaUsageOnWires(via, netIdx);
    auto viaPoorness = getViaPoorness(via, netIdx);
    double vioWithObs = (viaPoorness == ViaPoorness::Poor) ? setting.dbPoorViaPenaltyCoeff : 0;
    double nondefault = (viaPoorness == ViaPoorness::Nondefault && histCost) ? setting.dbNondefaultViaPenaltyCoeff : 0;
    HistUsageT histUsage = histCost ? getViaHistUsage(via) : 0;
    return (vioWithViaWire + histUsage) * unitSpaceVioCostDiscounted + (vioWithObs + nondefault) * unitSpaceVioCost;
}

unsigned RouteGrid::getViaUsageOnWires(const GridPoint& via, const int netIdx, const ViaType* viaType) const {
    unsigned usageOnWires = getViaUsageOnBotWires(via, netIdx, viaType);
    return usageOnWires ? usageOnWires : getViaUsageOnTopWires(via, netIdx, viaType);
}

unsigned RouteGrid::getViaUsageOnWiresPost(const GridPoint& via, const int netIdx, const ViaType* viaType) const {
    return getViaUsageOnBotWires(via, netIdx, viaType) + getViaUsageOnTopWires(via, netIdx, viaType);
}

unsigned RouteGrid::getViaUsageOnBotWires(const GridPoint& via, const int netIdx, const ViaType* viaType) const {
    if (!viaType) viaType = &cutLayers[via.layerIdx].defaultViaType();
    return getViaUsageOnWiresHelper(via, netIdx, viaType->viaBotWire[via.crossPointIdx]);
}

unsigned RouteGrid::getViaUsageOnTopWires(const GridPoint& via, const int netIdx, const ViaType* viaType) const {
    if (!viaType) viaType = &cutLayers[via.layerIdx].defaultViaType();
    auto upper = getUpper(via);
    return getViaUsageOnWiresHelper(upper, netIdx, viaType->viaTopWire[upper.crossPointIdx]);
}

unsigned RouteGrid::getViaUsageOnWiresHelper(const GridPoint& gp,
                                             const int netIdx,
                                             const vector<vector<bool>>& viaWire) const {
    int xSize = viaWire.size() / 2;
    int ySize = viaWire[0].size() / 2;
    unsigned usageOnWires = 0;
    for (unsigned i = max(0, gp.trackIdx - xSize); i < min(layers[gp.layerIdx].numTracks(), gp.trackIdx + xSize + 1);
         ++i) {
        vector<GridPoint> gps;
        for (unsigned j = max(0, gp.crossPointIdx - ySize);
             j < min(layers[gp.layerIdx].numCrossPoints(), gp.crossPointIdx + ySize + 1);
             ++j) {
            if (viaWire[i + xSize - gp.trackIdx][j + ySize - gp.crossPointIdx]) {
                gps.emplace_back(gp.layerIdx, i, j);
            }
        }
        if (gps.empty()) {
            continue;
        }
        const int low = gps.front().crossPointIdx;
        const int high = gps.back().crossPointIdx;
        const vector<int>& crossPointUsage =
            getShortWireSegmentUsageOnOvlpWire({gps[0].layerIdx, gps[0].trackIdx, {low, high}}, netIdx);
        for (int usage : crossPointUsage) {
            if (usage) {
                ++usageOnWires;  // count # tracks violated
                break;
            }
        }
    }
    return usageOnWires;
}

HistUsageT RouteGrid::getViaHistUsage(const GridPoint& via) const {
    auto it = histViaMap[via.layerIdx][via.trackIdx].find(via.crossPointIdx);
    if (it == histViaMap[via.layerIdx][via.trackIdx].end()) {
        return 0.0;
    } else {
        return it->second;
    }
}

const ViaType* RouteGrid::getViaType(const GridPoint& via) const {
    auto it = routedNonDefViaMap.find(via);
    return (it != routedNonDefViaMap.end()) ? it->second : &cutLayers[via.layerIdx].defaultViaType();
}

unsigned RouteGrid::getViaUsageOnVias(const GridPoint& via, const int netIdx, const ViaType* viaType) const {
    return getViaUsageOnSameLayerVias(via, netIdx, viaType) + getViaUsageOnBotLayerVias(via, netIdx, viaType) +
           getViaUsageOnTopLayerVias(via, netIdx, viaType);
}

unsigned RouteGrid::getViaUsageOnSameLayerVias(const GridPoint& via, const int netIdx, const ViaType* viaType) const {
    if (!viaType) viaType = &cutLayers[via.layerIdx].defaultViaType();
    unsigned usageOnVias = 0;
    const GridPoint& upper = getUpper(via);
    vector<GridPoint> viaVios;
    int xSize = viaType->mergedAllViaMetal.size() / 2;
    int ySize = viaType->mergedAllViaMetal[0].size() / 2;
    for (int i = max(0, via.trackIdx - xSize); i < min(layers[via.layerIdx].numTracks(), via.trackIdx + xSize + 1);
         ++i) {
        int firstCP = layers[upper.layerIdx].tracks[max(0, upper.trackIdx - ySize)].lowerCPIdx;
        int lastCP = layers[upper.layerIdx]
                         .tracks[min(layers[upper.layerIdx].numTracks() - 1, upper.trackIdx + ySize)]
                         .lowerCPIdx;
        auto itBegin = routedViaMap[via.layerIdx][i].lower_bound(firstCP);
        auto itEnd = routedViaMap[via.layerIdx][i].upper_bound(lastCP);
        for (auto it = itBegin; it != itEnd; ++it) {
            if (it->second == netIdx) continue;
            const GridPoint viaVio(via.layerIdx, i, it->first);
            const GridPoint upperVio = getUpper(viaVio);
            const auto& LUT = viaType->allViaMetalNum[getViaType(viaVio)->idx];
            int xSizeVio = LUT.size() / 2;
            int ySizeVio = LUT[0].size() / 2;
            int offsetX = viaVio.trackIdx - via.trackIdx;
            int offsetY = upperVio.trackIdx - upper.trackIdx;
            if (abs(offsetX) <= xSizeVio && abs(offsetY) <= ySizeVio && LUT[offsetX + xSizeVio][offsetY + ySizeVio]) {
                usageOnVias += LUT[offsetX + xSizeVio][offsetY + ySizeVio];
            }
        }
    }
    return usageOnVias;
}

unsigned RouteGrid::getViaUsageOnBotLayerVias(const GridPoint& via, const int netIdx, const ViaType* viaType) const {
    if (!viaType) viaType = &cutLayers[via.layerIdx].defaultViaType();
    return via.layerIdx ? getViaUsageOnDiffLayerViasHelper(via,
                                                           netIdx,
                                                           viaType->mergedAllViaBotVia[via.crossPointIdx],
                                                           viaType->allViaBotVia,
                                                           routedViaMapUpper,
                                                           true)
                        : 0;
}

unsigned RouteGrid::getViaUsageOnTopLayerVias(const GridPoint& via, const int netIdx, const ViaType* viaType) const {
    if (!viaType) viaType = &cutLayers[via.layerIdx].defaultViaType();
    auto upper = getUpper(via);
    return (via.layerIdx + 2 < layers.size())
               ? getViaUsageOnDiffLayerViasHelper(upper,
                                                  netIdx,
                                                  viaType->mergedAllViaTopVia[upper.crossPointIdx],
                                                  viaType->allViaTopVia,
                                                  routedViaMap,
                                                  false)
               : 0;
}

unsigned RouteGrid::getViaUsageOnDiffLayerViasHelper(const GridPoint& via,
                                                     const int netIdx,
                                                     const vector<vector<bool>>& mergedAllViaVia,
                                                     const vector<vector<vector<vector<bool>>>>& allViaVia,
                                                     const ViaMapT& viaMap,
                                                     bool viaBotVia) const {
    // Note: via and viaVio are defined by their grid points in the middle layer in this function
    unsigned usageOnVias = 0;
    int xSize = mergedAllViaVia.size() / 2;
    int ySize = mergedAllViaVia[0].size() / 2;
    for (unsigned i = max(0, via.trackIdx - xSize); i < min(layers[via.layerIdx].numTracks(), via.trackIdx + xSize + 1);
         ++i) {
        auto itBegin = viaMap[via.layerIdx][i].lower_bound(max(0, via.crossPointIdx - ySize));
        auto itEnd = viaMap[via.layerIdx][i].upper_bound(
            min(layers[via.layerIdx].numCrossPoints() - 1, via.crossPointIdx + ySize));
        for (auto it = itBegin; it != itEnd; ++it) {
            if (it->second == netIdx) continue;
            int neighCP = it->first;
            if (mergedAllViaVia[i - via.trackIdx + xSize][neighCP - via.crossPointIdx + ySize]) {
                GridPoint viaVio(via.layerIdx, i, neighCP);
                GridPoint viaVioLower = viaBotVia ? getLower(viaVio) : viaVio;
                const auto& LUT = allViaVia[getViaType(viaVioLower)->idx][via.crossPointIdx];
                int xSizeVio = LUT.size() / 2;
                int ySizeVio = LUT[0].size() / 2;
                int offsetX = viaVio.trackIdx - via.trackIdx;
                int offsetY = viaVio.crossPointIdx - via.crossPointIdx;
                if (abs(offsetX) <= xSizeVio && abs(offsetY) <= ySizeVio &&
                    LUT[offsetX + xSizeVio][offsetY + ySizeVio]) {
                    usageOnVias += 1;
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
        if (getViaFixedVio(via, cutLayer.defaultViaType(), netIdx) == 0) {
            return ViaPoorness::Good;
        }
        for (const auto& viaType : cutLayer.allViaTypes) {
            if (getViaFixedVio(via, viaType, netIdx) == 0) {
                return ViaPoorness::Nondefault;
            }
        }
        return ViaPoorness::Poor;
    }
}

void RouteGrid::initPoorViaMap(vector<std::pair<BoxOnLayer, int>>& fixedMetalVec) {
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

int RouteGrid::getViaFixedVio(const db::GridPoint& via, const db::ViaType& viaType, int netIdx) const {
    auto viaLoc = getLoc(via);
    return getViaFixedVio(viaLoc, via.layerIdx, viaType, netIdx);
}

int RouteGrid::getViaFixedVio(const utils::PointT<DBU>& viaLoc,
                              int viaLayerIdx,
                              const db::ViaType& viaType,
                              int netIdx) const {
    int numOvlp = 0;
    auto viaBot = viaType.bot;
    viaBot.ShiftBy(viaLoc);
    numOvlp += getFixedMetalVio({viaLayerIdx, viaBot}, netIdx);
    auto viaTop = viaType.top;
    viaTop.ShiftBy(viaLoc);
    numOvlp += getFixedMetalVio({viaLayerIdx + 1, viaTop}, netIdx);

    return numOvlp;
}

int RouteGrid::getFixedMetalVio(const BoxOnLayer& box, int netIdx) const {
    auto regions = getAccurateMetalRectForbidRegions(box);
    utils::BoxT<DBU> queryBox = box;
    queryBox.x.low -= layers[box.layerIdx].fixedMetalQueryMargin;
    queryBox.x.high += layers[box.layerIdx].fixedMetalQueryMargin;
    queryBox.y.low -= layers[box.layerIdx].fixedMetalQueryMargin;
    queryBox.y.high += layers[box.layerIdx].fixedMetalQueryMargin;

    for (const auto& region : regions) {
        queryBox = queryBox.UnionWith(region);
    }

    boostBox rtreeQueryBox(boostPoint(queryBox.x.low, queryBox.y.low), boostPoint(queryBox.x.high, queryBox.y.high));
    vector<std::pair<boostBox, int>> queryResults;
    fixedMetals[box.layerIdx].query(bgi::intersects(rtreeQueryBox), std::back_inserter(queryResults));
    vector<std::pair<utils::BoxT<DBU>, int>> neighMetals;
    for (const auto& queryResult : queryResults) {
        if (queryResult.second != netIdx) {
            const auto& b = queryResult.first;
            neighMetals.emplace_back(utils::BoxT<DBU>(bg::get<bg::min_corner, 0>(b),
                                                      bg::get<bg::min_corner, 1>(b),
                                                      bg::get<bg::max_corner, 0>(b),
                                                      bg::get<bg::max_corner, 1>(b)),
                                     queryResult.second);
        }
    }

    int numOvlp = 0;
    for (const auto& neighMetal : neighMetals) {
        // getOvlpFixedMetals
        for (auto forbidRegion : regions) {
            auto ovlp = forbidRegion.IntersectWith(neighMetal.first);
            if (ovlp.IsValid()) {
                numOvlp += (ovlp.area() > 0);
            }
        }

        // getOvlpFixedMetalForbidRegions
        auto forbidRegions = getAccurateMetalRectForbidRegions({box.layerIdx, neighMetal.first});
        for (auto forbidRegion : forbidRegions) {
            auto ovlp = forbidRegion.IntersectWith(box);
            if (ovlp.IsValid()) {
                numOvlp += (ovlp.area() > 0);
            }
        }

        // getOvlpC2CMetals
        if (!layers[box.layerIdx].isEolDominated(neighMetal.first)) {
            DBU space = layers[box.layerIdx].getParaRunSpace(neighMetal.first);
            numOvlp += (utils::L2Dist(box, neighMetal.first) < space);
        }
    }
    return numOvlp;
}

const ViaType& RouteGrid::getBestViaTypeForFixed(const utils::PointT<DBU>& viaLoc, int viaLayerIdx, int netIdx) const {
    const auto& cutLayer = getCutLayer(viaLayerIdx);
    auto bestViaType = &cutLayer.defaultViaType();
    int minVio = getViaFixedVio(viaLoc, viaLayerIdx, cutLayer.defaultViaType(), netIdx);
    for (auto& viaType : cutLayer.allViaTypes) {
        int vio = getViaFixedVio(viaLoc, viaLayerIdx, viaType, netIdx);
        if (vio < minVio) {
            vio = minVio;
            bestViaType = &viaType;
        }
    }
    return *bestViaType;
}

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
        iterateHistWireSegments(ts, netIdx, [&](const utils::IntervalT<int>& intvl, HistUsageT discountedUsage) {
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
        const vector<HistUsageT>& histWire = getShortWireSegmentUsageOnOvlpHistWire(ts, netIdx);
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

vector<HistUsageT> RouteGrid::getShortWireSegmentUsageOnOvlpHistWire(const TrackSegment& ts, int netIdx) const {
    vector<HistUsageT> crossPointUsage(ts.crossPointRange.range() + 1, 0);
    iterateHistWireSegments(ts, netIdx, [&](const utils::IntervalT<int>& intvl, HistUsageT discountedUsage) {
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
    const TrackSegment& ts,
    int netIdx,
    const std::function<void(const utils::IntervalT<int>&, HistUsageT)>& handle) const {
    auto queryInterval = boost::icl::interval<int>::closed(ts.crossPointRange.low, ts.crossPointRange.high);
    auto intervals = histWireMap[ts.layerIdx][ts.trackIdx].equal_range(queryInterval);
    for (auto it = intervals.first; it != intervals.second; ++it) {
        auto mergedInterval = it->first & queryInterval;
        if (netIdx != it->second.netIdx) {
            handle({first(mergedInterval), last(mergedInterval)}, it->second.usage);
        }
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
        getWireSegmentUsageOnVias(ts, netIdx, layers[ts.layerIdx].mergedWireTopVia, routedViaMap, false, viaCPs);
    }
    if (ts.layerIdx != 0) {
        // bot vias
        getWireSegmentUsageOnVias(ts, netIdx, layers[ts.layerIdx].mergedWireBotVia, routedViaMapUpper, true, viaCPs);
    }
    return viaCPs;
}

void RouteGrid::getWireSegmentUsageOnVias(const TrackSegment& ts,
                                          int netIdx,
                                          const vector<vector<vector<bool>>>& wireVia,
                                          const ViaMapT& viaMap,
                                          bool wireBotVia,
                                          vector<int>& viaCPs) const {
    int layerIdx = ts.layerIdx;
    const MetalLayer& layer = layers[layerIdx];
    const int xSize = wireVia[0].size() / 2;
    const int ySize = max(wireVia[ts.crossPointRange.low][0].size(), wireVia[ts.crossPointRange.high][0].size()) / 2;

    for (unsigned i = max(0, ts.trackIdx - xSize); i < min(layer.numTracks(), ts.trackIdx + xSize + 1); ++i) {
        auto itBegin = viaMap[layerIdx][i].lower_bound(ts.crossPointRange.low - ySize);
        auto itEnd = viaMap[layerIdx][i].upper_bound(ts.crossPointRange.high + ySize);

        for (auto it = itBegin; it != itEnd; ++it) {
            if (it->second == netIdx) continue;
            const int viaCP = it->first;
            GridPoint via(ts.layerIdx, i, viaCP);
            const auto& viaWire =
                wireBotVia ? getViaType(getLower(via))->viaTopWire[viaCP] : getViaType(via)->viaBotWire[viaCP];
            const int viaWireXSize = viaWire.size() / 2;
            const int viaWireYSize = viaWire[0].size() / 2;
            const int offsetX = ts.trackIdx - via.trackIdx;
            if (abs(offsetX) > viaWireXSize) continue;
            for (unsigned j = max(ts.crossPointRange.low, viaCP - viaWireYSize);
                 j <= min(ts.crossPointRange.high, viaCP + viaWireYSize);
                 ++j) {
                const int offsetY = j - via.crossPointIdx;
                if (abs(offsetY) <= viaWireYSize && viaWire[offsetX + viaWireXSize][offsetY + viaWireYSize]) {
                    viaCPs.push_back(j);
                }
            }
        }
    }
}

vector<std::pair<utils::BoxT<DBU>, int>> RouteGrid::getOvlpBoxes(const BoxOnLayer& box,
                                                                 int idx,
                                                                 const RTrees& rtrees) const {
    boostBox queryBox(boostPoint(box.x.low, box.y.low), boostPoint(box.x.high, box.y.high));
    vector<std::pair<boostBox, int>> queryResults;
    rtrees[box.layerIdx].query(bgi::intersects(queryBox), std::back_inserter(queryResults));
    vector<std::pair<utils::BoxT<DBU>, int>> results;
    for (const auto& queryResult : queryResults) {
        if (queryResult.second != idx) {
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

vector<std::pair<utils::BoxT<DBU>, int>> RouteGrid::getOvlpFixedMetals(const BoxOnLayer& box, int netIdx) const {
    return getOvlpBoxes(box, netIdx, fixedMetals);
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

void RouteGrid::markViaType(const GridPoint& via, const ViaType* viaType) {
    viaTypeLock.lock();
    if (viaType->idx == cutLayers[via.layerIdx].defaultViaType().idx) {
        routedNonDefViaMap.erase(via);
    } else {
        routedNonDefViaMap[via] = viaType;
    }
    viaTypeLock.unlock();
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
    auto iclRange = boost::icl::interval<int>::closed(ts.crossPointRange.low, ts.crossPointRange.high);
    wireLocks[ts.layerIdx][ts.trackIdx].lock();
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

void RouteGrid::useHistWireSegment(const TrackSegment& ts, int netIdx, HistUsageT usage) {
    auto iclRange = boost::icl::interval<int>::closed(ts.crossPointRange.low, ts.crossPointRange.high);
    histWireMap[ts.layerIdx][ts.trackIdx].add({iclRange, {netIdx, usage}});
}

void RouteGrid::useHistWireSegments(const GridBoxOnLayer& gb, int netIdx, HistUsageT usage) {
    auto iclRange = boost::icl::interval<int>::closed(gb.crossPointRange.low, gb.crossPointRange.high);
    for (int trackIdx = gb.trackRange.low; trackIdx <= gb.trackRange.high; ++trackIdx) {
        histWireMap[gb.layerIdx][trackIdx].add({iclRange, {netIdx, usage}});
    }
}

void RouteGrid::markFixedMetalBatch(vector<std::pair<BoxOnLayer, int>>& fixedMetalVec, int beginIdx, int endIdx) {
    vector<vector<std::pair<boostBox, int>>> fixedMetalsRtreeItems;
    fixedMetalsRtreeItems.resize(layers.size());

    if (setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << "mark fixed metal batch ..." << std::endl;
    }
    const int initMem = utils::mem_use::get_current();

    vector<vector<int>> layerToObjIdx(getLayerNum());
    for (unsigned i = beginIdx; i < endIdx; i++) layerToObjIdx[fixedMetalVec[i].first.layerIdx].push_back(i);

    int curLayer = 0;
    std::mutex layer_mutex;

    auto thread_func = [&]() {
        while (true) {
            layer_mutex.lock();
            int l = curLayer++;
            layer_mutex.unlock();

            if (l >= getLayerNum()) return;

            for (auto idx : layerToObjIdx[l]) {
                // fixedMetals
                const BoxOnLayer& box = fixedMetalVec[idx].first;
                int netIdx = fixedMetalVec[idx].second;

                boostBox markBox(boostPoint(box.x.low, box.y.low), boostPoint(box.x.high, box.y.high));
                fixedMetalsRtreeItems[box.layerIdx].push_back({markBox, netIdx});

                DBU space = layers[box.layerIdx].getParaRunSpace(box);
                if (space > layers[box.layerIdx].fixedMetalQueryMargin) {
                    layers[box.layerIdx].fixedMetalQueryMargin = space;
                }
            }
        }
    };

    const int numThreads = max(1, db::setting.numThreads);
    std::thread threads[numThreads];
    for (int i = 0; i < numThreads; i++) threads[i] = std::thread(thread_func);
    for (int i = 0; i < numThreads; i++) threads[i].join();

    const int curMem = utils::mem_use::get_current();
    if (setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        printflog("MEM(MB): init/cur=%d/%d, incr=%d\n", initMem, curMem, curMem - initMem);
        log() << std::endl;
    }

    if (!beginIdx) {
        for (int layerIdx = 0; layerIdx < layers.size(); layerIdx++) {
            RTree tRtree(fixedMetalsRtreeItems[layerIdx]);
            fixedMetals[layerIdx] = boost::move(tRtree);
        }
    } else {
        for (int layerIdx = 0; layerIdx < layers.size(); layerIdx++) {
            for (auto& item : fixedMetalsRtreeItems[layerIdx]) fixedMetals[layerIdx].insert(item);
        }
    }
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

void RouteGrid::removeVia(const GridPoint& via, int netIdx, ViaMapT& viaMap) {
    unsigned usage = 0;
    std::pair<std::multimap<int, int>::const_iterator, std::multimap<int, int>::const_iterator> itRange =
        viaMap[via.layerIdx][via.trackIdx].equal_range(via.crossPointIdx);
    for (std::multimap<int, int>::const_iterator it = itRange.first; it != itRange.second; ++it) {
        if (it->second == netIdx) {
            viaMap[via.layerIdx][via.trackIdx].erase(it);
            break;
        }
    }
}

void RouteGrid::removeVia(const GridPoint& via, int netIdx) {
    viaLocks[via.layerIdx][via.trackIdx].lock();
    removeVia(via, netIdx, routedViaMap);
    viaLocks[via.layerIdx][via.trackIdx].unlock();
    viaTypeLock.lock();
    routedNonDefViaMap.erase(via);
    viaTypeLock.unlock();
    viaLocksUpper[via.layerIdx][via.trackIdx].lock();
    removeVia(getUpper(via), netIdx, routedViaMapUpper);
    viaLocksUpper[via.layerIdx][via.trackIdx].unlock();
}

void RouteGrid::removeWireSegment(const TrackSegment& ts, int netIdx) {
    auto queryInterval = boost::icl::interval<int>::closed(ts.crossPointRange.low, ts.crossPointRange.high);
    wireLocks[ts.layerIdx][ts.trackIdx].lock();
    routedWireMap[ts.layerIdx][ts.trackIdx].subtract({queryInterval, {netIdx}});
    wireLocks[ts.layerIdx][ts.trackIdx].unlock();
}

void RouteGrid::removeWrongWayWireSegment(const WrongWaySegment& wws, int netIdx) {
    for (int trackIdx = wws.trackRange.low; trackIdx <= wws.trackRange.high; ++trackIdx) {
        removeWireSegment({wws.layerIdx, trackIdx, wws.crossPointIdx}, netIdx);
    }
}

double RouteGrid::printAllUsageAndVio() const {
    const int width = 10;
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
    return totalScore;
}

double RouteGrid::getScore() {
    auto wlVia = printAllUsage();
    auto shortSpace = printAllVio();
    vector<std::string> items = {"wirelength", "# vias", "short", "space"};
    vector<double> metrics = {wlVia.first, wlVia.second, shortSpace.first, shortSpace.second};
    vector<double> weights = {
            setting.weightWirelength, setting.weightViaNum, setting.weightShortArea, setting.weightSpaceVioNum};
    double totalScore = 0;
    for (int i = 0; i < items.size(); ++i) {
        totalScore += metrics[i] * weights[i];
        _vio_usage.at(i) = metrics.at(i);
    }
    return totalScore;
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

void RouteGrid::getNetWireVioUsage(std::unordered_map<int, int>& via_usage,
                                   std::unordered_map<int, float>& wire_usage_length,
                                   std::unordered_map<int, std::set<int>>& layer_usage) {
    for (int layer_idx = 0; layer_idx < getLayerNum(); ++layer_idx) {
        for (const auto& track : routedWireMap[layer_idx]) {
            for (const auto& usage : track) {
                const auto& intval = usage.first;
                for (int net_idx : usage.second) {
                    DBU dist = layers.at(layer_idx).getCrossPointRangeDist({first(intval), last(intval)});
                    wire_usage_length[net_idx] = dist / float(layers[1].pitch);
                    layer_usage[net_idx].insert(layer_idx);
                }
            }
        }
    }

    for (unsigned layerIdx = 0; (layerIdx + 1) < getLayerNum(); ++layerIdx) {
        for (const auto& track : routedViaMap[layerIdx]) {
            for ( const auto& usage: track) {
                ++via_usage[usage.second];
            }
        }
    }
}

void RouteGrid::getAllViaUsage(const vector<int>& buckets, const ViaMapT& viaMap, vector<int>& viaUsage) const {
    viaUsage.assign(buckets.size(), 0);
    for (unsigned layerIdx = 0; (layerIdx + 1) < getLayerNum(); ++layerIdx) {
        for (const auto& track : viaMap[layerIdx]) {
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
    const int width = 10;
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
                const ViaType* viaType = getViaType(via);
                sameLayerViaVios[layerIdx] += getViaUsageOnSameLayerVias(via, p.second, viaType);
                viaTopViaVios[layerIdx] += getViaUsageOnTopLayerVias(via, p.second, viaType);
                viaBotWireVios[layerIdx] += getViaUsageOnBotWires(via, p.second, viaType);
                viaTopWireVios[layerIdx] += getViaUsageOnTopWires(via, p.second, viaType);
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
    const int width = 10;
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
                    useHistWireSegment(ts, OBS_NET_IDX, 1.0);
                }
                for (int netIdx : intvlUsage.second) {
                    vector<int> viaLocs = getWireSegmentUsageOnVias(ts, netIdx);
                    for (int cpIdx : viaLocs) {
                        cpIdx = min(max(first(intvl), cpIdx), last(intvl));
                        useHistWireSegment({layerIdx, trackIdx, {cpIdx, cpIdx}}, OBS_NET_IDX, 1.0);
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
                if (getViaUsageOnVias(via, p.second, getViaType(via))) {  // ||
                    // getViaUsageOnBotWires(via, p.second) > 0 ||
                    // getViaUsageOnTopWires(via, p.second)) {
                    histViaMap[layerIdx][trackIdx][p.first] += 1.0;
                }
            }
        }
    }
}

void RouteGrid::fadeHistCost(const vector<int>& exceptedNets) {
    if (setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("Fade hist cost by", setting.rrrFadeCoeff, "...");
    }
    std::unordered_set<int> exceptedNetSet;
    for (int netIdx : exceptedNets) exceptedNetSet.insert(netIdx);
    std::map<CostT, int> histWireUsage, histViaUsage;
    for (int layerIdx = 0; layerIdx < getLayerNum(); ++layerIdx) {
        for (int trackIdx = 0; trackIdx < layers[layerIdx].numTracks(); ++trackIdx) {
            // wire
            for (auto& intvl : histWireMap[layerIdx][trackIdx]) {
                auto& histWire = intvl.second;
                if (histWire.netIdx == OBS_NET_IDX || exceptedNetSet.find(histWire.netIdx) == exceptedNetSet.end()) {
                    histWire.usage *= setting.rrrFadeCoeff;
                }
            }
            // via
            for (auto& p : histViaMap[layerIdx][trackIdx]) {
                p.second *= setting.rrrFadeCoeff;
            }
        }
    }
}

void RouteGrid::statHistCost() const {
    if (setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        std::map<CostT, int> histWireUsage, histViaUsage;
        for (int layerIdx = 0; layerIdx < getLayerNum(); ++layerIdx) {
            for (int trackIdx = 0; trackIdx < layers[layerIdx].numTracks(); ++trackIdx) {
                // wire
                for (auto& intvl : histWireMap[layerIdx][trackIdx]) {
                    ++histWireUsage[intvl.second.usage];
                }
                // via
                for (auto& p : histViaMap[layerIdx][trackIdx]) {
                    ++histViaUsage[p.second];
                }
            }
        }
        printlog("Hist wire usage is", histWireUsage);
        printlog("Hist via usage is", histViaUsage);
    }
}

std::array<double, 4> RouteGrid::getAllVio() const {
     return std::array<double, 4>(_vio_usage);
}

}   // namespace db

