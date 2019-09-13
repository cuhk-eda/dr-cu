#include "PoorViaMap.h"

namespace db {

ViaData* PoorViaMapBuilder::createViaData(const std::unordered_set<int>& allowNetIdxs,
                                          bool allAllow,
                                          bool nonDefaultOnly) {
    ViaData* ret;
    if (allAllow) {
        ret = new GoodVia();
    } else if (allowNetIdxs.empty()) {
        ret = new PoorVia();
    } else if (allowNetIdxs.size() == 1) {
        ret = new FixVia();
        ret->setAllowNet(*allowNetIdxs.begin());
    } else {
        ret = new FlexVia();
        ret->setAllowNets(allowNetIdxs);
    }
    ret->nonDefaultOnly = nonDefaultOnly;
    return ret;
}

void PoorViaMapBuilder::addViaData(vector<std::pair<int, ViaData*>>& rowData,
                                   int topTrackIdx,
                                   const std::unordered_set<int>& allowNetIdxs,
                                   bool allAllow,
                                   bool nonDefaultOnly) {
    ViaData* prevViaData = rowData.empty() ? NULL : rowData.back().second;

    if (rowData.empty()) {
        rowData.emplace_back(topTrackIdx, createViaData(allowNetIdxs, allAllow, nonDefaultOnly));
    } else if (allAllow) {
        if (typeid(*prevViaData) != typeid(GoodVia) || prevViaData->isNonDefaultOnly() != nonDefaultOnly)
            rowData.emplace_back(topTrackIdx, createViaData(allowNetIdxs, allAllow, nonDefaultOnly));
    } else if (allowNetIdxs.empty()) {
        if (typeid(*prevViaData) != typeid(PoorVia))
            rowData.emplace_back(topTrackIdx, createViaData(allowNetIdxs, allAllow, nonDefaultOnly));
    } else if (allowNetIdxs.size() == 1) {
        if (typeid(*prevViaData) != typeid(FixVia) || prevViaData->getAllowNet() != *allowNetIdxs.begin() ||
            prevViaData->isNonDefaultOnly() != nonDefaultOnly)
            rowData.emplace_back(topTrackIdx, createViaData(allowNetIdxs, allAllow, nonDefaultOnly));
    } else {
        rowData.emplace_back(topTrackIdx, createViaData(allowNetIdxs, allAllow, nonDefaultOnly));
    }
}

PoorViaMapRegionBuilder::PoorViaMapRegionBuilder(vector<vector<std::pair<int, ViaData*>>>& _poorViaMap,
                                                 const RouteGrid& _routeGrid,
                                                 const vector<std::pair<BoxOnLayer, int>>& _fixObjects,
                                                 int cutLayerIdx)
    : poorViaMap(_poorViaMap),
      routeGrid(_routeGrid),
      fixObjects(_fixObjects),
      botLayer(routeGrid.getLayer(cutLayerIdx)),
      topLayer(routeGrid.getLayer(cutLayerIdx + 1)),
      cutLayer(routeGrid.getCutLayer(cutLayerIdx)) {}

void PoorViaMapRegionBuilder::run(const utils::IntervalT<int>& regionTrackRange,
                                  const vector<vector<int>>& regionObjects) {
    int regionTracks = regionTrackRange.range() + 1;

    goodMap.assign(regionTracks, vector<bool>(topLayer.numTracks(), false));
    nonDefaultMap.assign(regionTracks, vector<bool>(topLayer.numTracks(), true));
    exceptionMap.assign(regionTracks, vector<vector<int>>(topLayer.numTracks()));

    markViaType(cutLayer.defaultViaType(),
                utils::IntervalT<int>{regionTrackRange.low, regionTrackRange.high},
                regionObjects[0],
                regionObjects[1]);
    for (const auto& viaType : cutLayer.allViaTypes) {
        if (cutLayer.isDefaultViaType(viaType) || viaType.hasMultiCut) continue;
        markViaType(viaType,
                    utils::IntervalT<int>{regionTrackRange.low, regionTrackRange.high},
                    regionObjects[0],
                    regionObjects[1]);
    }
    genViaData(regionTrackRange);
}  // namespace db

void PoorViaMapRegionBuilder::getSlideRegion(const utils::BoxT<DBU>& moveRect,
                                             const utils::BoxT<DBU>& fixRect,
                                             utils::IntervalT<int>& botTrackRange,
                                             utils::IntervalT<int>& topTrackRange) const {
    auto dir = botLayer.direction;
    int botLo = fixRect[dir].low - abs(moveRect[dir].high);
    int botHi = fixRect[dir].high + abs(moveRect[dir].low);
    int topLo = fixRect[1 - dir].low - abs(moveRect[1 - dir].high);
    int topHi = fixRect[1 - dir].high + abs(moveRect[1 - dir].low);

    botTrackRange = botLayer.rangeSearchTrack({botLo, botHi}, false);
    topTrackRange = topLayer.rangeSearchTrack({topLo, topHi}, false);
}

void PoorViaMapRegionBuilder::markSpace(const utils::BoxT<DBU>& moveRect,
                                        const utils::BoxT<DBU>& fixRect,
                                        int netIdx,
                                        const utils::IntervalT<int>& regionTrackRange) {
    utils::IntervalT<int> botTrackRange, topTrackRange;
    getSlideRegion(moveRect, fixRect, botTrackRange, topTrackRange);

    auto botRegionTrackRange = botTrackRange.IntersectWith(regionTrackRange);
    if (botRegionTrackRange.IsValid()) {
        botRegionTrackRange.ShiftBy(-regionTrackRange.low);
        for (int a = botRegionTrackRange.low; a <= botRegionTrackRange.high; a++) {
            for (int b = topTrackRange.low; b <= topTrackRange.high; b++) {
                markLocMap(a, b, netIdx);
            }
        }
    }
}

void PoorViaMapRegionBuilder::markC2C(const utils::BoxT<DBU>& viaRect,
                                      const utils::BoxT<DBU>& metalRect,
                                      int netIdx,
                                      const utils::IntervalT<int>& regionTrackRange,
                                      bool isBot) {
    const auto& curLayer = isBot ? botLayer : topLayer;

    if (curLayer.isEolDominated(metalRect)) return;

    DBU space = curLayer.getParaRunSpace(metalRect);
    auto dir = botLayer.direction;

    vector<DBU> botCornerLoc = {metalRect[dir].low, metalRect[dir].high};
    vector<DBU> topCornerLoc = {metalRect[1 - dir].low, metalRect[1 - dir].high};
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            int botLo = botCornerLoc[i] - space - abs(viaRect[dir].high);
            int botHi = botCornerLoc[i] + space + abs(viaRect[dir].low);
            int topLo = topCornerLoc[j] - space - abs(viaRect[1 - dir].high);
            int topHi = topCornerLoc[j] + space + abs(viaRect[1 - dir].low);

            utils::IntervalT<int> botTrackRange = botLayer.rangeSearchTrack({botLo, botHi}, false);
            utils::IntervalT<int> topTrackRange = topLayer.rangeSearchTrack({topLo, topHi}, false);

            auto botRegionTrackRange = botTrackRange.IntersectWith(regionTrackRange);
            if (botRegionTrackRange.IsValid()) {
                botRegionTrackRange.ShiftBy(-regionTrackRange.low);
                for (int a = botRegionTrackRange.low; a <= botRegionTrackRange.high; a++) {
                    for (int b = topTrackRange.low; b <= topTrackRange.high; b++) {
                        auto shiftedViaRect = viaRect;
                        if (dir == Dimension::X) {
                            shiftedViaRect.ShiftBy(
                                {botLayer.tracks[a + regionTrackRange.low].location, topLayer.tracks[b].location});
                        } else {
                            shiftedViaRect.ShiftBy(
                                {topLayer.tracks[b].location, botLayer.tracks[a + regionTrackRange.low].location});
                        }

                        if (utils::L2Dist(metalRect, shiftedViaRect) >= space) continue;

                        markLocMap(a, b, netIdx);
                    }
                }
            }
        }
    }
}

void PoorViaMapRegionBuilder::markLocMap(int a, int b, int netIdx) {
    if (goodMap[a][b] || curPoorMap[a][b]) return;
    if (netIdx == OBS_NET_IDX) {
        if (!curGoodMap[a][b]) exceptionMap[a][b].pop_back();
        curPoorMap[a][b] = true;
        curGoodMap[a][b] = false;
        return;
    }

    if (curGoodMap[a][b]) {
        curGoodMap[a][b] = false;
        exceptionMap[a][b].push_back(netIdx);
    } else {
        if (exceptionMap[a][b].back() != netIdx) {
            exceptionMap[a][b].pop_back();
            curPoorMap[a][b] = true;
        }
    }
}

void PoorViaMapRegionBuilder::markViaType(const ViaType& viaType,
                                          utils::IntervalT<int> regionTrackRange,
                                          const vector<int>& botFixObjectIdxs,
                                          const vector<int>& topFixObjectIdxs) {
    bool isDefaultViaType = cutLayer.isDefaultViaType(viaType);

    int regionTracks = regionTrackRange.range() + 1;
    curGoodMap.assign(regionTracks, vector<bool>(topLayer.numTracks(), true));
    curPoorMap.assign(regionTracks, vector<bool>(topLayer.numTracks(), false));
    const auto& viaTop = viaType.top;
    const auto& viaBot = viaType.bot;
    const auto& topForbidRegions = viaType.topForbidRegions;
    const auto& botForbidRegions = viaType.botForbidRegions;
    // 1.via to fixObject forbid polygons 2.fixObject to via forbid polygons 3. via to fixObject corners
    for (auto idx : botFixObjectIdxs) {
        const BoxOnLayer& fixObject = fixObjects[idx].first;
        int netIdx = fixObjects[idx].second;
        auto fixObjRegions = routeGrid.getAccurateMetalRectForbidRegions(fixObject);
        for (const auto& rect : fixObjRegions) markSpace(viaBot, rect, netIdx, regionTrackRange);
        for (const auto& rect : botForbidRegions) markSpace(rect, fixObject, netIdx, regionTrackRange);
        markC2C(viaBot, fixObject, netIdx, regionTrackRange, true);
    }
    for (auto idx : topFixObjectIdxs) {
        const BoxOnLayer& fixObject = fixObjects[idx].first;
        int netIdx = fixObjects[idx].second;
        auto fixObjRegions = routeGrid.getAccurateMetalRectForbidRegions(fixObject);
        for (const auto& rect : fixObjRegions) markSpace(viaTop, rect, netIdx, regionTrackRange);
        for (const auto& rect : topForbidRegions) markSpace(rect, fixObject, netIdx, regionTrackRange);
        markC2C(viaTop, fixObject, netIdx, regionTrackRange, false);
    }

    regionTrackRange.ShiftBy(-regionTrackRange.low);
    for (int a = regionTrackRange.low; a <= regionTrackRange.high; a++) {
        for (int b = 0; b < topLayer.numTracks(); b++) {
            goodMap[a][b] = goodMap[a][b] || curGoodMap[a][b];
            // Note: under the assumption that default viaType is always first marked
            if (!curPoorMap[a][b] && isDefaultViaType) nonDefaultMap[a][b] = false;
        }
    }
};

void PoorViaMapRegionBuilder::genViaData(utils::IntervalT<int> regionTrackRange) {
    int offset = regionTrackRange.low;
    regionTrackRange.ShiftBy(-regionTrackRange.low);
    for (int a = regionTrackRange.low; a <= regionTrackRange.high; a++) {
        auto& intvls = poorViaMap[a + offset];
        for (int b = 0; b < topLayer.numTracks(); b++) {
            ViaData* prevViaData = intvls.empty() ? NULL : intvls.back().second;

            std::unordered_set<int> allowNetIdxs(exceptionMap[a][b].begin(), exceptionMap[a][b].end());
            PoorViaMapBuilder::addViaData(intvls, b, allowNetIdxs, goodMap[a][b], nonDefaultMap[a][b]);
        }
        intvls.emplace_back(topLayer.numTracks(), (ViaData*)NULL);
    }
}

void PoorViaMapBuilder::initPoorViaMapFast(const vector<std::pair<BoxOnLayer, int>>& fixObjects) {
    if (setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << "Init poorViaMapFast ..." << std::endl;
    }
    const int initMem = utils::mem_use::get_current();

    int numCutLayer = routeGrid.getLayerNum() - 1;
    vector<vector<int>> layerToObjIdx(routeGrid.getLayerNum());
    for (unsigned i = 0; i < fixObjects.size(); i++) layerToObjIdx[fixObjects[i].first.layerIdx].push_back(i);

    poorViaMap.resize(numCutLayer);
    for (int i = 0; i < numCutLayer; i++) {
        if (!usePoorViaMap[i]) continue;
        const MetalLayer& botLayer = routeGrid.getLayer(i);
        const MetalLayer& topLayer = routeGrid.getLayer(i + 1);
        const auto& cutLayer = routeGrid.getCutLayer(i);

        poorViaMap[i].resize(botLayer.tracks.size());

        const int numThreads = max(1, db::setting.numThreads);
        int numRegions = 40;

        int numTrackPerRegion = ceil(botLayer.tracks.size() * 1.0 / numRegions);
        auto dir = routeGrid.getLayerDir(i);
        int firstTrackLoc = botLayer.tracks.front().location;
        int lastTrackLoc = botLayer.tracks.back().location;

        vector<utils::IntervalT<int>> regionTrackIntvls(numRegions);
        for (int r = 0; r < numRegions; r++) {
            regionTrackIntvls[r].Set(r * numTrackPerRegion,
                                     min((int)botLayer.tracks.size(), (r + 1) * numTrackPerRegion) - 1);
            if (!regionTrackIntvls[r].IsValid()) {
                regionTrackIntvls.resize(r);
                numRegions = r;
                break;
            }
        }

        vector<vector<vector<int>>> regionToFixObjects(numRegions, vector<vector<int>>(2));
        for (int l = i; l <= i + 1; l++) {
            const auto& viaMaxForbidRegion = (i == l) ? cutLayer.botMaxForbidRegion : cutLayer.topMaxForbidRegion;
            for (auto idx : layerToObjIdx[l]) {
                const auto& fixObject = fixObjects[idx].first;
                auto fixObjRegions = routeGrid.getAccurateMetalRectForbidRegions(fixObject);
                utils::BoxT<DBU> maxForbidRegion(fixObject.x, fixObject.y);
                for (const auto& rect : fixObjRegions) maxForbidRegion = maxForbidRegion.UnionWith(rect);

                int low = maxForbidRegion[dir].low - abs(viaMaxForbidRegion[dir].high);
                int high = maxForbidRegion[dir].high + abs(viaMaxForbidRegion[dir].low);
                if (high <= firstTrackLoc || low >= lastTrackLoc) continue;
                auto trackRange = botLayer.rangeSearchTrack({low, high}, false);

                int loRegionIdx = trackRange.low / numTrackPerRegion;
                int hiRegionIdx = trackRange.high / numTrackPerRegion;
                for (int r = loRegionIdx; r <= hiRegionIdx; r++) regionToFixObjects[r][l - i].push_back(idx);
            }
        }

        int curRegion = 0;
        std::mutex region_mutex;
        auto thread_func = [&]() {
            while (true) {
                region_mutex.lock();
                int r = curRegion++;
                region_mutex.unlock();

                if (r >= numRegions) return;

                PoorViaMapRegionBuilder regionBuilder(poorViaMap[i], routeGrid, fixObjects, i);
                regionBuilder.run(regionTrackIntvls[r], regionToFixObjects[r]);
            }
        };

        std::thread threads[numThreads];
        for (int i = 0; i < numThreads; i++) threads[i] = std::thread(thread_func);
        for (int i = 0; i < numThreads; i++) threads[i].join();
    }

    printPoorViaMapInfo();
    const int curMem = utils::mem_use::get_current();
    if (setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        printflog("MEM(MB): init/cur=%d/%d, incr=%d\n", initMem, curMem, curMem - initMem);
        log() << std::endl;
    }
}

void PoorViaMapBuilder::printPoorViaMapInfo() {
    for (int i = 0, sz = routeGrid.getLayerNum(); i < sz - 1; i++) {
        if (!usePoorViaMap[i]) continue;

        const MetalLayer& botLayer = routeGrid.getLayer(i);
        const MetalLayer& topLayer = routeGrid.getLayer(i + 1);

        int one = 0, poor = 0, good = 0, multi = 0;
        int nInterv = 0;

        for (unsigned a = 0; a < botLayer.tracks.size(); a++) {
            nInterv += poorViaMap[i][a].size() - 1;
            for (unsigned b = 0; b < poorViaMap[i][a].size() - 1; b++) {
                int sz = poorViaMap[i][a][b + 1].first - poorViaMap[i][a][b].first;
                if (typeid(*poorViaMap[i][a][b].second) == typeid(GoodVia)) {
                    good += sz;
                } else if (typeid(*poorViaMap[i][a][b].second) == typeid(PoorVia)) {
                    poor += sz;
                } else if (typeid(*poorViaMap[i][a][b].second) == typeid(FlexVia)) {
                    multi += sz;
                } else {
                    one += sz;
                }
            }
        }

        int totVia = botLayer.tracks.size() * topLayer.tracks.size();
        if (setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
            printflog("cutlayer%d: poor=%d, one=%d, multi=%d, good=%d, nInterv=%d, totVia=%d, ratio=%.3f\n",
                      i,
                      poor,
                      one,
                      multi,
                      good,
                      nInterv,
                      totVia,
                      nInterv * 1.0 / totVia);
        }
    }
}

PoorViaMapBuilder::PoorViaMapBuilder(vector<vector<vector<std::pair<int, ViaData*>>>>& _poorViaMap,
                                     const vector<bool>& _usePoorViaMap,
                                     const RouteGrid& _routeGrid)
    : poorViaMap(_poorViaMap), usePoorViaMap(_usePoorViaMap), routeGrid(_routeGrid) {}

void PoorViaMapBuilder::run(const vector<std::pair<BoxOnLayer, int>>& fixObjects) { initPoorViaMapFast(fixObjects); }
}  // namespace db
