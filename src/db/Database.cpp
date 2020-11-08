#include "Database.h"
#include "rsyn/io/parser/lef_def/DEFControlParser.h"
#include "single_net/PinTapConnector.h"

db::Database database;

namespace db {

void Database::init() {
    if (setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << std::endl;
        log() << "################################################################" << std::endl;
        log() << "Start initializing database" << std::endl;
        log() << std::endl;
    }
    rsynService.init();

    auto dieBound = rsynService.physicalDesign.getPhysicalDie().getBounds();
    dieRegion = getBoxFromRsynBounds(dieBound);
    if (setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << "Die region (in DBU): " << dieRegion << std::endl;
        log() << std::endl;
    }

    RouteGrid::init();

    NetList::init(rsynService);

    markPinAndObsOccupancy();

    initMTSafeMargin();

    sliceRouteGuides();

    constructRouteGuideRTrees();

    log() << "Finish initializing database" << std::endl;
    log() << "MEM: cur=" << utils::mem_use::get_current() << "MB, peak=" << utils::mem_use::get_peak() << "MB"
          << std::endl;
    log() << std::endl;
}

void Database::writeDEF(const std::string& filename) {
    DEFControlParser defParser;
    DefDscp def;
    def.clsDesignName = rsynService.design.getName();
    def.clsDatabaseUnits = rsynService.physicalDesign.getDatabaseUnits(Rsyn::DESIGN_DBU);
    def.clsHasDatabaseUnits = true;
    def.clsDieBounds = rsynService.physicalDesign.getPhysicalDie().getBounds();
    def.clsHasDieBounds = true;

    def.clsRows.reserve(rsynService.physicalDesign.getNumRows());
    for (Rsyn::PhysicalRow phRow : rsynService.physicalDesign.allPhysicalRows()) {
        def.clsRows.push_back(DefRowDscp());
        DefRowDscp& defRow = def.clsRows.back();
        defRow.clsName = phRow.getName();
        defRow.clsSite = phRow.getSiteName();
        defRow.clsOrigin = phRow.getOrigin();
        defRow.clsStepX = phRow.getStep(X);
        defRow.clsStepY = 0;  // phRow.getStep(Y);
        defRow.clsNumX = phRow.getNumSites(X);
        defRow.clsNumY = phRow.getNumSites(Y);
        defRow.clsOrientation = Rsyn::getPhysicalOrientation(phRow.getSiteOrientation());
    }  // end for

    // write def tracks
    def.clsTracks.reserve(rsynService.physicalDesign.getNumPhysicalTracks());
    for (const Rsyn::PhysicalTracks& phTrack : rsynService.physicalDesign.allPhysicalTracks()) {
        def.clsTracks.emplace_back();
        DefTrackDscp& defTrack = def.clsTracks.back();
        defTrack.clsDirection = Rsyn::getPhysicalTrackDirectionDEF(phTrack.getDirection());
        defTrack.clsLocation = phTrack.getLocation();
        defTrack.clsSpace = phTrack.getSpace();
        int numLayers = phTrack.getNumberOfLayers();
        defTrack.clsLayers.reserve(numLayers);
        for (Rsyn::PhysicalLayer phLayer : phTrack.allLayers()) defTrack.clsLayers.push_back(phLayer.getName());
        defTrack.clsNumTracks = phTrack.getNumberOfTracks();
    }  // end for

    // write def vias
    unsigned numVias = 0;
    for (const Rsyn::PhysicalVia& phVia : rsynService.physicalDesign.allPhysicalVias()) {
        if (phVia.isViaDesign()) ++numVias;
    }  // end for
    def.clsVias.reserve(numVias);
    for (const Rsyn::PhysicalVia& phVia : rsynService.physicalDesign.allPhysicalVias()) {
        if (!phVia.isViaDesign()) continue;
        def.clsVias.emplace_back();
        DefViaDscp& defVia = def.clsVias.back();
        defVia.clsName = phVia.getName();
        defVia.clsBottomLayer = phVia.getBottomLayer().getName();
        defVia.clsCutLayer = phVia.getCutLayer().getName();
        defVia.clsTopLayer = phVia.getTopLayer().getName();
        defVia.clsGeometries.emplace(defVia.clsBottomLayer, std::deque<DefViaGeometryDscp>());
        for (const Rsyn::PhysicalViaGeometry& phGeo : phVia.allBottomGeometries()) {
            DefViaGeometryDscp defGeo;
            defGeo.clsBounds = phGeo.getBounds();
            defVia.clsGeometries[defVia.clsBottomLayer].push_back(defGeo);
        }  // end for
        defVia.clsGeometries.emplace(defVia.clsCutLayer, std::deque<DefViaGeometryDscp>());
        for (const Rsyn::PhysicalViaGeometry& phGeo : phVia.allCutGeometries()) {
            DefViaGeometryDscp defGeo;
            defGeo.clsBounds = phGeo.getBounds();
            defVia.clsGeometries[defVia.clsCutLayer].push_back(defGeo);
        }  // end for
        defVia.clsGeometries.emplace(defVia.clsTopLayer, std::deque<DefViaGeometryDscp>());
        for (const Rsyn::PhysicalViaGeometry& phGeo : phVia.allTopGeometries()) {
            DefViaGeometryDscp defGeo;
            defGeo.clsBounds = phGeo.getBounds();
            defVia.clsGeometries[defVia.clsTopLayer].push_back(defGeo);
        }  // end for
        if (phVia.hasViaRule()) {
            defVia.clsHasViaRule = true;
            defVia.clsViaRuleName = phVia.getViaRule().getName();
            defVia.clsXCutSize = phVia.getCutSize(X);
            defVia.clsYCutSize = phVia.getCutSize(Y);
            defVia.clsXCutSpacing = phVia.getSpacing(X);
            defVia.clsYCutSpacing = phVia.getSpacing(Y);
            defVia.clsXBottomEnclosure = phVia.getEnclosure(Rsyn::BOTTOM_VIA_LEVEL, X);
            defVia.clsYBottomEnclosure = phVia.getEnclosure(Rsyn::BOTTOM_VIA_LEVEL, Y);
            defVia.clsXTopEnclosure = phVia.getEnclosure(Rsyn::TOP_VIA_LEVEL, X);
            defVia.clsYTopEnclosure = phVia.getEnclosure(Rsyn::TOP_VIA_LEVEL, Y);
        }
        if (phVia.hasRowCol()) {
            defVia.clsHasRowCol = true;
            defVia.clsNumCutRows = phVia.getNumRows();
            defVia.clsNumCutCols = phVia.getNumCols();
        }
        if (phVia.hasOrigin()) {
            defVia.clsHasOrigin = true;
            defVia.clsXOffsetOrigin = phVia.getOrigin(X);
            defVia.clsYOffsetOrigin = phVia.getOrigin(Y);
        }
        if (phVia.hasOffset()) {
            defVia.clsHasOffset = true;
            defVia.clsXBottomOffset = phVia.getOffset(Rsyn::BOTTOM_VIA_LEVEL, X);
            defVia.clsYBottomOffset = phVia.getOffset(Rsyn::BOTTOM_VIA_LEVEL, Y);
            defVia.clsXTopOffset = phVia.getOffset(Rsyn::TOP_VIA_LEVEL, X);
            defVia.clsYTopOffset = phVia.getOffset(Rsyn::TOP_VIA_LEVEL, Y);
        }
        if (phVia.hasPattern()) {
            defVia.clsHasPattern = true;
            defVia.clsPattern = phVia.getPattern();
        }
    }  // end for

    def.clsComps.reserve(rsynService.design.getNumInstances(Rsyn::CELL));
    for (Rsyn::Instance instance : rsynService.module.allInstances()) {
        if (instance.getType() != Rsyn::CELL) continue;

        Rsyn::Cell cell = instance.asCell();  // minor TODO: hack, assuming that the instance is a cell
        Rsyn::PhysicalCell ph = rsynService.physicalDesign.getPhysicalCell(cell);
        def.clsComps.push_back(DefComponentDscp());
        DefComponentDscp& defComp = def.clsComps.back();
        defComp.clsName = cell.getName();
        defComp.clsMacroName = cell.getLibraryCellName();
        defComp.clsPos = ph.getPosition();
        defComp.clsIsFixed = instance.isFixed();
        defComp.clsOrientation = Rsyn::getPhysicalOrientation(ph.getOrientation());
        defComp.clsIsPlaced = ph.isPlaced();
    }  // end for

    // write def special nets
    def.clsSpecialNets.reserve(rsynService.physicalDesign.getNumPhysicalSpecialNets());
    for (Rsyn::PhysicalSpecialNet phSpecialNet : rsynService.physicalDesign.allPhysicalSpecialNets()) {
        def.clsSpecialNets.push_back(phSpecialNet.getNet());
    }

    int numNets = rsynService.design.getNumNets();
    int i = 0;
    def.clsNets.reserve(numNets);
    for (Rsyn::Net net : rsynService.module.allNets()) {
        switch(net.getUse()) {
            case Rsyn::POWER:
                continue;
            case Rsyn::GROUND:
                continue;
            default:
                break;
        }
        def.clsNets.push_back(DefNetDscp());
        DefNetDscp& defNet = def.clsNets.back();
        defNet.clsName = net.getName();
        defNet.clsConnections.reserve(net.getNumPins());
        for (Rsyn::Pin pin : net.allPins()) {
            if (!pin.isPort()) continue;
            defNet.clsConnections.push_back(DefNetConnection());
            DefNetConnection& netConnection = defNet.clsConnections.back();
            netConnection.clsComponentName = "PIN";
            netConnection.clsPinName = pin.getInstanceName();
        }  // end for
        for (Rsyn::Pin pin : net.allPins()) {
            if (pin.isPort()) continue;
            defNet.clsConnections.push_back(DefNetConnection());
            DefNetConnection& netConnection = defNet.clsConnections.back();
            netConnection.clsComponentName = pin.getInstanceName();
            netConnection.clsPinName = pin.getName();
        }  // end for

        const db::Net& dbNet = nets[i++];
        defNet.clsWires.clear();
        if (!dbNet.defWireSegments.empty()) {
            defNet.clsWires.emplace_back();
            // defNet.clsWires.back().clsWireSegments = move(dbNet.defWireSegments);
            vector<DefWireSegmentDscp>& defWireSegments = defNet.clsWires.back().clsWireSegments;
            std::unordered_map<std::tuple<string, Dimension, DBU>, vector<std::pair<DBU, bool>>> tracks;
            for (const DefWireSegmentDscp& seg : dbNet.defWireSegments) {
                if (seg.clsRoutingPoints.size() == 1) {
                    defWireSegments.push_back(seg);
                    continue;
                }
                const string& layerName = seg.clsLayerName;
                const DBUxy& xy0 = seg.clsRoutingPoints[0].clsPos;
                const DBUxy& xy1 = seg.clsRoutingPoints[1].clsPos;
                for (unsigned dim = 0; dim != 2; ++dim) {
                    if (xy0[dim] == xy1[dim]) {
                        const std::tuple<string, Dimension, DBU> key =
                            std::make_tuple(layerName, static_cast<Dimension>(dim), xy0[dim]);
                        std::unordered_map<std::tuple<string, Dimension, DBU>, vector<std::pair<DBU, bool>>>::iterator it =
                            tracks.find(key);
                        if (it == tracks.end()) {
                            it = tracks.emplace(key, vector<std::pair<DBU, bool>>()).first;
                        }
                        it->second.emplace_back(std::min(xy0[1 - dim], xy1[1 - dim]), true);
                        it->second.emplace_back(std::max(xy0[1 - dim], xy1[1 - dim]), false);
                    }
                }
            }
            for (const std::pair<std::tuple<string, Dimension, DBU>, vector<std::pair<DBU, bool>>>& p : tracks) {
                vector<std::pair<DBU, bool>> pts = p.second;
                std::sort(pts.begin(), pts.end());
                unsigned isWire = 0;
                DBU start = std::numeric_limits<DBU>::has_infinity ? -std::numeric_limits<DBU>::infinity()
                                                                : std::numeric_limits<DBU>::lowest();
                const Dimension dim = std::get<1>(p.first);
                for (const std::pair<DBU, bool>& pt : pts) {
                    if (isWire && pt.first != start) {
                        defWireSegments.emplace_back();
                        DefWireSegmentDscp& segment = defWireSegments.back();

                        segment.clsRoutingPoints.resize(2);
                        segment.clsRoutingPoints[0].clsPos[dim] = std::get<2>(p.first);
                        segment.clsRoutingPoints[1].clsPos[dim] = std::get<2>(p.first);
                        segment.clsRoutingPoints[0].clsPos[1 - dim] = start;
                        segment.clsRoutingPoints[1].clsPos[1 - dim] = pt.first;
                        segment.clsLayerName = std::get<0>(p.first);
                    }
                    if (pt.second) {
                        --isWire;
                    } else {
                        ++isWire;
                    }
                    start = pt.first;
                }
            }
        }
    }  // end for

    int numPorts = rsynService.module.getNumPorts(Rsyn::IN) + rsynService.module.getNumPorts(Rsyn::OUT);
    def.clsPorts.reserve(numPorts);
    for (Rsyn::Port port : rsynService.module.allPorts()) {
        Rsyn::PhysicalPort phPort = rsynService.physicalDesign.getPhysicalPort(port);
        def.clsPorts.push_back(DefPortDscp());
        DefPortDscp& defPort = def.clsPorts.back();
        defPort.clsName = port.getName();
        defPort.clsNetName = port.getInnerPin().getNetName();
        if (port.getDirection() == Rsyn::IN)
            defPort.clsDirection = "INPUT";
        else if (port.getDirection() == Rsyn::OUT)
            defPort.clsDirection = "OUTPUT";

        defPort.clsLocationType = "FIXED";
        defPort.clsOrientation = Rsyn::getPhysicalOrientation(phPort.getOrientation());
        defPort.clsLayerName = phPort.getLayer().getName();
        defPort.clsLayerBounds = phPort.getBounds();
        defPort.clsPos = phPort.getPosition();

    }  // end for

    defParser.writeFullDEF(filename, def);
}

void Database::markPinAndObsOccupancy() {
    if (db::setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << "Mark pin & obs occupancy on RouteGrid ..." << std::endl;
    }
    vector<std::pair<BoxOnLayer, int>> fixedMetalVec;

    // STEP 1: get fixed objects
    // Mark pins associated with nets
    for (const auto& net : nets) {
        for (const auto& accessBoxes : net.pinAccessBoxes) {
            for (const auto& box : accessBoxes) {
                fixedMetalVec.emplace_back(box, net.idx);
            }
        }
    }
    // Mark dangling pins
    // minor TODO: port?
    const Rsyn::Session session;
    const Rsyn::PhysicalDesign& physicalDesign =
        static_cast<Rsyn::PhysicalService*>(session.getService("rsyn.physical"))->getPhysicalDesign();
    const DBU libDBU = physicalDesign.getDatabaseUnits(Rsyn::LIBRARY_DBU);
    unsigned numUnusedPins = 0;
    unsigned numObs = 0;
    unsigned numSNetObs = 0;
    for (Rsyn::Instance instance : rsynService.module.allInstances()) {
        if (instance.getType() != Rsyn::CELL) continue;
        // phCell
        Rsyn::Cell cell = instance.asCell();
        Rsyn::PhysicalCell phCell = rsynService.physicalDesign.getPhysicalCell(cell);
        Rsyn::PhysicalLibraryCell phLibCell = rsynService.physicalDesign.getPhysicalLibraryCell(cell);
        const DBUxy origin(static_cast<DBU>(std::round(phLibCell.getMacro()->originX() * libDBU)),
                           static_cast<DBU>(std::round(phLibCell.getMacro()->originY() * libDBU)));
        // libPin
        for (Rsyn::Pin pin : instance.allPins(false)) {
            if (!pin.getNet()) {  // no associated net
                Rsyn::PhysicalLibraryPin phLibPin = rsynService.physicalDesign.getPhysicalLibraryPin(pin);
                vector<BoxOnLayer> accessBoxes;
                Net::getPinAccessBoxes(phLibPin, phCell, accessBoxes, origin);
                for (const auto& box : accessBoxes) {
                    fixedMetalVec.emplace_back(box, OBS_NET_IDX);
                }
                ++numUnusedPins;
            }
        }
        // libObs
        DBUxy displacement = phCell.getPosition() + origin;
        auto transform = phCell.getTransform();
        for (const Rsyn::PhysicalObstacle& phObs : phLibCell.allObstacles()) {
            if (phObs.getLayer().getType() != Rsyn::PhysicalLayerType::ROUTING) continue;
            const int layerIdx = phObs.getLayer().getRelativeIndex();
            for (auto bounds : phObs.allBounds()) {
                bounds.translate(displacement);
                bounds = transform.apply(bounds);
                const BoxOnLayer box(layerIdx, getBoxFromRsynBounds(bounds));
                fixedMetalVec.emplace_back(box, OBS_NET_IDX);
                ++numObs;
            }
        }
    }
    // Mark special nets
    for (Rsyn::PhysicalSpecialNet specialNet : rsynService.physicalDesign.allPhysicalSpecialNets()) {
        for (const DefWireDscp& wire : specialNet.getNet().clsWires) {
            for (const DefWireSegmentDscp& segment : wire.clsWireSegments) {
                int layerIdx =
                    rsynService.physicalDesign.getPhysicalLayerByName(segment.clsLayerName).getRelativeIndex();
                const DBU width = segment.clsRoutedWidth;
                DBUxy pos;
                DBU ext = 0;
                for (unsigned i = 0; i != segment.clsRoutingPoints.size(); ++i) {
                    const DefRoutingPointDscp& pt = segment.clsRoutingPoints[i];
                    const DBUxy& nextPos = pt.clsPos;
                    const DBU nextExt = pt.clsHasExtension ? pt.clsExtension : 0;
                    if (i >= 1) {
                        for (unsigned dim = 0; dim != 2; ++dim) {
                            if (pos[dim] == nextPos[dim]) continue;
                            const DBU l = pos[dim] < nextPos[dim] ? pos[dim] - ext : nextPos[dim] - nextExt;
                            const DBU h = pos[dim] < nextPos[dim] ? nextPos[dim] + nextExt : pos[dim] + ext;
                            BoxOnLayer box(layerIdx);
                            box[dim].Set(l, h);
                            box[1 - dim].Set(pos[1 - dim] - width / 2, pos[1 - dim] + width / 2);
                            fixedMetalVec.emplace_back(box, OBS_NET_IDX);
                            ++numSNetObs;
                            break;
                        }
                    }
                    pos = nextPos;
                    ext = nextExt;
                    if (!pt.clsHasVia) continue;
                    const Rsyn::PhysicalVia& via = rsynService.physicalDesign.getPhysicalViaByName(pt.clsViaName);
                    const int botLayerIdx = via.getBottomLayer().getRelativeIndex();
                    for (const Rsyn::PhysicalViaGeometry& geo : via.allBottomGeometries()) {
                        Bounds bounds = geo.getBounds();
                        bounds.translate(pos);
                        const BoxOnLayer box(botLayerIdx, getBoxFromRsynBounds(bounds));
                        fixedMetalVec.emplace_back(box, OBS_NET_IDX);
                        ++numSNetObs;
                    }
                    const int topLayerIdx = via.getTopLayer().getRelativeIndex();
                    for (const Rsyn::PhysicalViaGeometry& geo : via.allTopGeometries()) {
                        Bounds bounds = geo.getBounds();
                        bounds.translate(pos);
                        const BoxOnLayer box(topLayerIdx, getBoxFromRsynBounds(bounds));
                        fixedMetalVec.emplace_back(box, OBS_NET_IDX);
                        ++numSNetObs;
                    }
                    if (via.hasViaRule()) {
                        const utils::PointT<int> numRowCol =
                            via.hasRowCol() ? utils::PointT<int>(via.getNumCols(), via.getNumRows())
                                            : utils::PointT<int>(1, 1);
                        BoxOnLayer botBox(botLayerIdx);
                        BoxOnLayer topBox(topLayerIdx);
                        for (unsigned dimIdx = 0; dimIdx != 2; ++dimIdx) {
                            const Dimension dim = static_cast<Dimension>(dimIdx);
                            const DBU origin = via.hasOrigin() ? pos[dim] + via.getOrigin(dim) : pos[dim];
                            const DBU botOff =
                                via.hasOffset() ? origin + via.getOffset(Rsyn::BOTTOM_VIA_LEVEL, dim) : origin;
                            const DBU topOff =
                                via.hasOffset() ? origin + via.getOffset(Rsyn::TOP_VIA_LEVEL, dim) : origin;
                            const DBU length =
                                (via.getCutSize(dim) * numRowCol[dim] + via.getSpacing(dim) * (numRowCol[dim] - 1)) / 2;
                            const DBU botEnc = length + via.getEnclosure(Rsyn::BOTTOM_VIA_LEVEL, dim);
                            const DBU topEnc = length + via.getEnclosure(Rsyn::TOP_VIA_LEVEL, dim);
                            botBox[dim].Set(botOff - botEnc, botOff + botEnc);
                            topBox[dim].Set(topOff - topEnc, topOff + topEnc);
                        }
                        fixedMetalVec.emplace_back(botBox, OBS_NET_IDX);
                        fixedMetalVec.emplace_back(topBox, OBS_NET_IDX);
                        numSNetObs += 2;
                    }
                    if (layerIdx == botLayerIdx)
                        layerIdx = topLayerIdx;
                    else if (layerIdx == topLayerIdx)
                        layerIdx = botLayerIdx;
                    else {
                        log() << "Error: Special net " << specialNet.getNet().clsName << " via " << pt.clsViaName
                              << " on wrong layer " << layerIdx << std::endl;
                        break;
                    }
                }
            }
        }
    }
    // Stat
    vector<int> layerNumFixedObjects(getLayerNum(), 0);
    for (const auto& fixedMetal : fixedMetalVec) {
        layerNumFixedObjects[fixedMetal.first.layerIdx]++;
    }
    // Print
    if (setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << "The number of unused pins is " << numUnusedPins << std::endl;
        log() << "The number of OBS is " << numObs << std::endl;
        log() << "The number of special net OBS is " << numSNetObs << std::endl;
        log() << "The number of fixed objects on each layers:" << std::endl;
        for (unsigned i = 0; i < getLayerNum(); i++) {
            if (layerNumFixedObjects[i] > 0) log() << getLayer(i).name << ": " << layerNumFixedObjects[i] << std::endl;
        }
    }
    log() << std::endl;

    // STEP 2: mark
    if (setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("mark fixed metal rtrees...");
    }

    markFixedMetalBatch(fixedMetalVec, 0, fixedMetalVec.size());

    addPinViaMetal(fixedMetalVec);

    // Mark poor wire
    if (setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("mark poor wire map...");
    }
    for (const auto& fixedMetal : fixedMetalVec) {
        const auto& fixedBox = fixedMetal.first;
        AggrParaRunSpace aggr = AggrParaRunSpace::DEFAULT;
        if (getLayer(0).parallelLength.size() <= 1) {
            // hack for ISPD'18 test cases
            aggr = AggrParaRunSpace::LARGER_WIDTH;
            if (min(fixedBox.width(), fixedBox.height()) == getLayer(fixedBox.layerIdx).width &&
                getOvlpFixedMetals(fixedBox, NULL_NET_IDX).size() == 1) {
                aggr = AggrParaRunSpace::DEFAULT;
            }
        } else {
            // hack for ISPD'19 test cases
            aggr = AggrParaRunSpace::LARGER_LENGTH;
        }
        auto fixedForbidRegion = getMetalRectForbidRegion(fixedBox, aggr);
        auto gridBox = rangeSearch(fixedForbidRegion, aggr == AggrParaRunSpace::LARGER_WIDTH);  // TODO: change to false
        if (!isValid(gridBox)) continue;
        for (int trackIdx = gridBox.trackRange.low; trackIdx <= gridBox.trackRange.high; ++trackIdx) {
            usePoorWireSegment({gridBox.layerIdx, trackIdx, gridBox.crossPointRange}, fixedMetal.second);
        }
        if (fixedMetal.second >=0) {
            // add initial hist cost to help pin access
            if (gridBox.layerIdx != 0) {
                useHistWireSegments(getLower(gridBox), fixedMetal.second, db::setting.dbInitHistUsageForPinAccess);
            }
            if (gridBox.layerIdx != getLayerNum() - 1) {
                useHistWireSegments(getUpper(gridBox), fixedMetal.second, db::setting.dbInitHistUsageForPinAccess);
            }
        }
    }
    // Mark poor via
    for (int i = 0; i < getLayerNum() - 1; ++i) {
        usePoorViaMap[i] = (layerNumFixedObjects[i] >= setting.dbUsePoorViaMapThres ||
                            layerNumFixedObjects[i + 1] >= setting.dbUsePoorViaMapThres);
    }
    if (setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("mark poor via map...");
        printlog("usePoorViaMap", usePoorViaMap);
    }
    initPoorViaMap(fixedMetalVec);
}

void Database::addPinViaMetal(vector<std::pair<BoxOnLayer, int>>& fixedMetalVec) {
    usePoorViaMap.resize(getLayerNum() - 1, false);
    int beginIdx = fixedMetalVec.size();

    std::mutex metalMutex;
    auto pinViaMT = runJobsMT(database.nets.size(), [&](int netIdx) { 
        const auto& net = database.nets[netIdx];
        vector<vector<db::GridBoxOnLayer>> gridPinAccessBoxes;
        getGridPinAccessBoxes(net, gridPinAccessBoxes);
        for (int pinIdx = 0; pinIdx < net.numOfPins(); pinIdx++) {
            const auto& accessBoxes = gridPinAccessBoxes[pinIdx];
            if (accessBoxes.size() <= 1) continue;
            int lastIdx = accessBoxes.size() - 1;
            if (accessBoxes[lastIdx].layerIdx == accessBoxes[lastIdx - 1].layerIdx) continue;
            // handle diff-layer gridBox
            const auto& gridBox = accessBoxes[lastIdx];
            db::GridPoint tap(gridBox.layerIdx, gridBox.trackRange.low, gridBox.crossPointRange.low);
            db::BoxOnLayer bestBox;
            db::RouteStatus status =
                PinTapConnector::getBestPinAccessBox(getLoc(tap), tap.layerIdx, net.pinAccessBoxes[pinIdx], bestBox);
            if (status != +db::RouteStatus::SUCC_CONN_EXT_PIN || bestBox.layerIdx == tap.layerIdx) continue;
            utils::PointT<DBU> viaLoc(bestBox.cx(), bestBox.cy());
            int layerIdx = min(bestBox.layerIdx, tap.layerIdx);
            auto viaType = database.getBestViaTypeForFixed(viaLoc, layerIdx, net.idx);
            metalMutex.lock();
            fixedMetalVec.emplace_back(db::BoxOnLayer(layerIdx, viaType.getShiftedBotMetal(viaLoc)), net.idx);
            fixedMetalVec.emplace_back(db::BoxOnLayer(layerIdx + 1, viaType.getShiftedTopMetal(viaLoc)), net.idx);
            metalMutex.unlock();
        }
    });
    if (db::setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("pinViaMT", pinViaMT);
    }

    markFixedMetalBatch(fixedMetalVec, beginIdx, fixedMetalVec.size());  // TODO: may not be needed
}

void Database::initMTSafeMargin() {
    for (auto& layer : layers) {
        layer.mtSafeMargin = max({layer.minAreaMargin, layer.confLutMargin, layer.fixedMetalQueryMargin});
        if (db::setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
            printlog(layer.name, "mtSafeMargin = max {", layer.minAreaMargin, layer.confLutMargin, layer.fixedMetalQueryMargin, "} =", layer.mtSafeMargin);
        }
    }
}

void Database::sliceRouteGuides() {
    if (db::setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << "Slice RouteGuides ..." << std::endl;
        log() << std::endl;
    }
    for (auto& net : nets) {
        vector<vector<utils::BoxT<DBU>>> guides(getLayerNum());  // route guides on different layers
        for (auto& guide : net.routeGuides) {
            guides[guide.layerIdx].push_back(guide);
        }
        net.routeGuides.clear();
        for (int layerIdx = 0; layerIdx < getLayerNum(); ++layerIdx) {
            utils::SlicePolygons<DBU>(guides[layerIdx], 1 - getLayerDir(layerIdx));
            for (const auto& guide : guides[layerIdx]) {
                net.routeGuides.emplace_back(layerIdx, guide);
                net.gridRouteGuides.push_back(rangeSearch(net.routeGuides.back()));
            }
        }
    }
}

void Database::constructRouteGuideRTrees() {
    if (db::setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << "Construct r-trees for route guides of each net ..." << std::endl;
        log() << std::endl;
    }
    for (auto& net : nets) {
        net.routeGuideRTrees.resize(getLayerNum());
        vector<vector<std::pair<boostBox, int>>> rtreeItems;
        rtreeItems.resize(getLayerNum());
        for (unsigned i = 0; i < net.routeGuides.size(); ++i) {
            const auto& guide = net.routeGuides[i];
            boostBox b(boostPoint(guide.x.low, guide.y.low), boostPoint(guide.x.high, guide.y.high));
            rtreeItems[guide.layerIdx].push_back(std::make_pair(b, i));
            // net.routeGuideRTrees[guide.layerIdx].insert(std::make_pair(b, i));
        }
        for (int layerIdx = 0; layerIdx < getLayerNum(); layerIdx++) {
            RTree tRtree(rtreeItems[layerIdx]);
            net.routeGuideRTrees[layerIdx] = boost::move(tRtree);
        }
    }
}

void Database::writeDEFWireSegment(Net& dbNet, const utils::PointT<DBU>& u, const utils::PointT<DBU>& v, int layerIdx) {
    dbNet.defWireSegments.emplace_back();
    DefWireSegmentDscp& segment = dbNet.defWireSegments.back();

    segment.clsRoutingPoints.resize(2);
    segment.clsRoutingPoints[0].clsPos.set(u.x, u.y);
    segment.clsRoutingPoints[1].clsPos.set(v.x, v.y);
    segment.clsLayerName = getLayer(layerIdx).name;
}

void Database::writeDEFVia(Net& dbNet, const utils::PointT<DBU>& point, const ViaType& viaType, int layerIdx) {
    dbNet.defWireSegments.emplace_back();
    DefWireSegmentDscp& segment = dbNet.defWireSegments.back();

    segment.clsRoutingPoints.resize(1);
    segment.clsRoutingPoints[0].clsPos.set(point.x, point.y);
    segment.clsRoutingPoints[0].clsHasVia = true;
    segment.clsLayerName = getLayer(layerIdx).name;
    segment.clsRoutingPoints[0].clsViaName = viaType.name;
}

void Database::writeDEFFillRect(Net& dbNet, const utils::BoxT<DBU>& rect, const int layerIdx) {
    dbNet.defWireSegments.emplace_back();
    DefWireSegmentDscp& segment = dbNet.defWireSegments.back();

    segment.clsRoutingPoints.resize(1);
    segment.clsRoutingPoints[0].clsPos.set(rect.lx(), rect.ly());
    segment.clsRoutingPoints[0].clsHasRectangle = true;
    segment.clsRoutingPoints[0].clsRect = {0, 0, rect.hx() - rect.lx(), rect.hy() - rect.ly()};
    segment.clsLayerName = getLayer(layerIdx).name;
}

void Database::getGridPinAccessBoxes(const Net& net, vector<vector<db::GridBoxOnLayer>>& gridPinAccessBoxes) const {
    gridPinAccessBoxes.resize(net.numOfPins());
    for (unsigned pinIdx = 0; pinIdx != net.numOfPins(); ++pinIdx) {
        vector<vector<db::GridBoxOnLayer>> pins(getLayerNum());
        for (const db::BoxOnLayer& pinAccessBox : net.pinAccessBoxes[pinIdx]) {
            int dir = getLayerDir(pinAccessBox.layerIdx);
            DBU pitch = getLayer(pinAccessBox.layerIdx).pitch;
            // pinForbidRegion
            auto pinForbidRegion = getMetalRectForbidRegion(pinAccessBox, AggrParaRunSpace::DEFAULT);
            const db::GridBoxOnLayer& gridPinForbidRegion = rangeSearch(pinForbidRegion);
            if (isValid(gridPinForbidRegion)) {
                pins[pinAccessBox.layerIdx].push_back(gridPinForbidRegion);
            }
            // One-pitch extension
            auto pinExtension = pinAccessBox;
            for (int d = 0; d < 2; ++d) {
                pinExtension[d].low -= pitch;
                pinExtension[d].high += pitch;
            }
            const db::GridBoxOnLayer& gridPinExtension = rangeSearch(pinExtension);
            for (int trackIdx = gridPinExtension.trackRange.low; trackIdx <= gridPinExtension.trackRange.high;
                 ++trackIdx) {
                for (int cpIdx = gridPinExtension.crossPointRange.low; cpIdx <= gridPinExtension.crossPointRange.high;
                     ++cpIdx) {
                    db::GridPoint pt(pinAccessBox.layerIdx, trackIdx, cpIdx);
                    if (!gridPinForbidRegion.includePoint(pt) && Dist(pinAccessBox, getLoc(pt)) <= pitch) {
                        pins[pinAccessBox.layerIdx].emplace_back(pinAccessBox.layerIdx,
                                                                 utils::IntervalT<int>{trackIdx, trackIdx},
                                                                 utils::IntervalT<int>{cpIdx, cpIdx});
                    }
                }
            }
        }

        // assign a relatively far grid access box if none (rarely happen)
        unsigned numBoxes = 0;
        for (const vector<db::GridBoxOnLayer>& pin : pins) {
            numBoxes += pin.size();
        }
        if (!numBoxes) {
            for (const db::BoxOnLayer& pinAccessBox : net.pinAccessBoxes[pinIdx]) {
                db::GridBoxOnLayer gridBox = rangeSearch(pinAccessBox);
                if (gridBox.trackRange.low > gridBox.trackRange.high) {
                    if (gridBox.trackRange.low == 0) {
                        gridBox.trackRange.high = 0;
                    } else {
                        gridBox.trackRange.low = gridBox.trackRange.high;
                    }
                }
                if (gridBox.crossPointRange.low > gridBox.crossPointRange.high) {
                    if (gridBox.crossPointRange.low == 0) {
                        gridBox.crossPointRange.high = 0;
                    } else {
                        gridBox.crossPointRange.low = gridBox.crossPointRange.high;
                    }
                }
                pins[pinAccessBox.layerIdx].push_back(gridBox);
            }
        }

        // slice
        gridPinAccessBoxes[pinIdx].clear();
        for (vector<db::GridBoxOnLayer>& pin : pins) {
            if (!pin.empty()) {
                db::GridBoxOnLayer::sliceGridPolygons(pin);
                for (const db::GridBoxOnLayer& box : pin) {
                    if (isValid(box)) {
                        gridPinAccessBoxes[pinIdx].push_back(box);
                    }
                }
            }
        }
        if (gridPinAccessBoxes[pinIdx].empty()) {
            log() << "Error: Net " << net.getName() << " Pin " << pinIdx << " has empty grid pin access boxes\n";
        }

        // assign diff-layer access point if all poor
        bool allPinTapPoor = true;
        for (auto& gridBox : gridPinAccessBoxes[pinIdx]) {
            for (int trackIdx = gridBox.trackRange.low; trackIdx <= gridBox.trackRange.high; ++trackIdx) {
                for (int cpIdx = gridBox.crossPointRange.low; cpIdx <= gridBox.crossPointRange.high; ++cpIdx) {
                    db::GridPoint upper_pt(gridBox.layerIdx, trackIdx, cpIdx);
                    if ((gridBox.layerIdx == getLayerNum() - 1 || !isValid(getUpper(upper_pt)) ||
                         getViaPoorness(upper_pt, net.idx) != db::RouteGrid::ViaPoorness::Poor) &&
                        (gridBox.layerIdx == 0 || !isValid(getLower(upper_pt)) ||
                         getViaPoorness(getLower(upper_pt), net.idx) != db::RouteGrid::ViaPoorness::Poor)) {
                        allPinTapPoor = false;
                        break;
                    }
                }
                if (!allPinTapPoor) break;
            }
            if (!allPinTapPoor) break;
        }
        if (allPinTapPoor) {
            auto bestBox = net.getMaxAccessBox(pinIdx);
            auto addDiffLayerGridPinAccessBox = [&](const BoxOnLayer& pinBox) {
                auto gridBox =
                    getSurroundingGrid(pinBox.layerIdx, utils::PointT<DBU>(pinBox.x.center(), pinBox.y.center()));
                gridPinAccessBoxes[pinIdx].push_back(gridBox);
            };
            if (bestBox.layerIdx > 0) {
                auto pinBox = bestBox;
                pinBox.layerIdx--;
                addDiffLayerGridPinAccessBox(pinBox);
            }
            if (bestBox.layerIdx < getLayerNum() - 1) {
                auto pinBox = bestBox;
                pinBox.layerIdx++;
                addDiffLayerGridPinAccessBox(pinBox);
            }
        }
    }
}

}  // namespace db

MTStat runJobsMT(int numJobs, const std::function<void(int)>& handle) {
    int numThreads = min(numJobs, db::setting.numThreads);
    MTStat mtStat(max(1, db::setting.numThreads));
    if (numThreads <= 1) {
        utils::timer threadTimer;
        for (int i = 0; i < numJobs; ++i) {
            handle(i);
        }
        mtStat.durations[0] = threadTimer.elapsed();
    } else {
        int globalJobIdx = 0;
        std::mutex mtx;
        utils::timer threadTimer;
        auto thread_func = [&](int threadIdx) {
            int jobIdx;
            while (true) {
                mtx.lock();
                jobIdx = globalJobIdx++;
                mtx.unlock();
                if (jobIdx >= numJobs) {
                    mtStat.durations[threadIdx] = threadTimer.elapsed();
                    break;
                }
                handle(jobIdx);
            }
        };

        std::thread threads[numThreads];
        for (int i = 0; i < numThreads; i++) {
            threads[i] = std::thread(thread_func, i);
        }
        for (int i = 0; i < numThreads; i++) {
            threads[i].join();
        }
    }
    return mtStat;
}
