#include "Database.h"
#include "rsyn/io/parser/lef_def/DEFControlParser.h"

db::Database database;

namespace db {

void Database::init() {
    log() << std::endl;
    log() << "################################################################" << std::endl;
    log() << "Start initializing database" << std::endl;
    log() << std::endl;

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

    sliceRouteGuides();

    log() << "Finish initializing database" << std::endl;
    log() << "MEM: cur=" << utils::mem_use::get_current() << "MB, peak=" << utils::mem_use::get_peak() << "MB"
          << std::endl;
    log() << std::endl;
}

void Database::writeDEF(const std::string& filename) {
    DEFControlParser defParser;
    DefDscp def;
    def.clsDieBounds = rsynService.physicalDesign.getPhysicalDie().getBounds();
    def.clsDatabaseUnits = rsynService.physicalDesign.getDatabaseUnits(Rsyn::DESIGN_DBU);
    def.clsDesignName = rsynService.design.getName();

    int numCells = rsynService.design.getNumInstances(Rsyn::CELL);
    def.clsComps.reserve(numCells);
    for (Rsyn::Instance instance : rsynService.module.allInstances()) {
        if (instance.getType() != Rsyn::CELL) continue;

        Rsyn::Cell cell = instance.asCell();  // TODO: hack, assuming that the instance is a cell
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

    int numNets = rsynService.design.getNumNets();
    int i = 0;
    def.clsNets.reserve(numNets);
    for (Rsyn::Net net : rsynService.module.allNets()) {
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
        defNet.clsWires = move(dbNet.topo);
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

    int numRows = rsynService.physicalDesign.getNumRows();
    def.clsRows.reserve(numRows);
    for (Rsyn::PhysicalRow phRow : rsynService.physicalDesign.allPhysicalRows()) {
        def.clsRows.push_back(DefRowDscp());
        DefRowDscp& defRow = def.clsRows.back();
        defRow.clsName = phRow.getName();
        defRow.clsSite = phRow.getSiteName();
        defRow.clsOrigin = phRow.getOrigin();
        defRow.clsStepX = phRow.getStep(X);
        defRow.clsStepY = phRow.getStep(Y);
        defRow.clsNumX = phRow.getNumSites(X);
        defRow.clsNumY = phRow.getNumSites(Y);
        defRow.clsOrientation = Rsyn::getPhysicalOrientation(phRow.getSiteOrientation());
    }  // end for

    // write def tracks
    int numTracks = rsynService.physicalDesign.getNumPhysicalTracks();
    def.clsTracks.reserve(numTracks);
    for (Rsyn::PhysicalTracks phTrack : rsynService.physicalDesign.allPhysicalTracks()) {
        def.clsTracks.push_back(DefTrackDscp());
        DefTrackDscp& defTrack = def.clsTracks.back();
        defTrack.clsDirection = Rsyn::getPhysicalTrackDirectionDEF(phTrack.getDirection());
        defTrack.clsLocation = phTrack.getLocation();
        defTrack.clsSpace = phTrack.getSpace();
        int numLayers = phTrack.getNumberOfLayers();
        defTrack.clsLayers.reserve(numLayers);
        for (Rsyn::PhysicalLayer phLayer : phTrack.allLayers()) defTrack.clsLayers.push_back(phLayer.getName());
        defTrack.clsNumTracks = phTrack.getNumberOfTracks();
    }  // end for

    defParser.writeFullDEF(filename, def);
}

void Database::printAllUsageAndVio() {
    int minAreaVio = 0;
    int minAreaShadowedVio = 0;
    int viaPinVio = 0;
    for (const auto& net : nets) {
        minAreaVio += net.minAreaVio;
        minAreaShadowedVio += net.minAreaShadowedVio;
        viaPinVio += net.viaPinVio;
    }
    log() << "# min-area vio: " << minAreaVio << std::endl;
    log() << "# min-area shadowed vio: " << minAreaShadowedVio << std::endl;
    log() << "# via-pin vio: " << viaPinVio << std::endl;
    RouteGrid::printAllUsageAndVio();
}

void Database::markPinAndObsOccupancy() {
    if (db::setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << "Mark pin & obs occupancy on RouteGrid ..." << std::endl;
    }

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
    const Rsyn::PhysicalDesign& physicalDesign = static_cast<Rsyn::PhysicalService*>(session.getService("rsyn.physical"))->getPhysicalDesign();
    const DBU libDBU = physicalDesign.getDatabaseUnits(Rsyn::LIBRARY_DBU);
    int numUnusedPins, numObs = 0;
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
                    fixedMetalVec.emplace_back(box, -1);
                }
                ++numUnusedPins;
            }
        }
        // libObs
        DBUxy displacement = phCell.getPosition() + origin;
        auto transform = phCell.getTransform();
        for (const Rsyn::PhysicalObstacle& phObs : phLibCell.allObstacles()) {
            int layerIdx = phObs.getLayer().getRelativeIndex();  // always metal layer?
            for (auto bounds : phObs.allBounds()) {
                bounds.translate(displacement);
                bounds = transform.apply(bounds);
                BoxOnLayer box(layerIdx, getBoxFromRsynBounds(bounds));
                fixedMetalVec.emplace_back(box, -1);
                ++numObs;
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
    markFixedMetalBatch();
    if (setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("mark poor wire map...");
    }
    for (const auto& fixedMetal : fixedMetalVec) {
        const auto& fixedBox = fixedMetal.first;
        bool aggr = true;
        if (min(fixedBox.width(), fixedBox.height()) == getLayer(fixedBox.layerIdx).width) {
            if (getOvlpFixedMetals(fixedBox).size() == 1) {
                aggr = false;
            }
        }
        auto fixedForbidRegion = getMetalRectForbidRegion(fixedBox, aggr);
        auto gridBox = rangeSearch(fixedForbidRegion, aggr);
        for (int trackIdx = gridBox.trackRange.low; trackIdx <= gridBox.trackRange.high; ++trackIdx) {
            usePoorWireSegment({gridBox.layerIdx, trackIdx, gridBox.crossPointRange}, fixedMetal.second);
        }
    }
    if (setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("mark poor via map...");
    }
    usePoorViaMap.resize(getLayerNum() - 1, false);
    for (int i = 0; i < getLayerNum() - 1; ++i) {
        usePoorViaMap[i] = (layerNumFixedObjects[i] >= setting.dbUsePoorViaMapThres ||
                            layerNumFixedObjects[i + 1] >= setting.dbUsePoorViaMapThres);
    }
    if (setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("usePoorViaMap", usePoorViaMap);
    }
    RouteGrid::initPoorViaMap();
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

void Database::writeDEFWireSegment(Net& dbNet, const utils::PointT<DBU>& u, const utils::PointT<DBU>& v, int layerIdx) {
    dbNet.topo.emplace_back();
    DefWireDscp& wire = dbNet.topo.back();
    wire.clsWireSegments.emplace_back();
    DefWireSegmentDscp& segment = wire.clsWireSegments.back();

    segment.clsRoutingPoints.resize(2);
    segment.clsRoutingPoints[0].clsPos.set(u.x, u.y);
    segment.clsRoutingPoints[1].clsPos.set(v.x, v.y);
    segment.clsLayerName = getLayer(layerIdx).name;
}

void Database::writeDEFVia(Net& dbNet, const utils::PointT<DBU>& point, const ViaType& viaType, int layerIdx) {
    dbNet.topo.emplace_back();
    DefWireDscp& wire = dbNet.topo.back();
    wire.clsWireSegments.emplace_back();
    DefWireSegmentDscp& segment = wire.clsWireSegments.back();

    segment.clsRoutingPoints.resize(1);
    segment.clsRoutingPoints[0].clsPos.set(point.x, point.y);
    segment.clsRoutingPoints[0].clsHasVia = true;
    segment.clsLayerName = getLayer(layerIdx).name;
    segment.clsRoutingPoints[0].clsViaName = viaType.name;
}

void Database::writeDEFFillRect(Net& dbNet, utils::BoxT<DBU>& rect, int layerIdx) {
    dbNet.topo.emplace_back();
    DefWireDscp& wire = dbNet.topo.back();
    wire.clsWireSegments.emplace_back();
    DefWireSegmentDscp& segment = wire.clsWireSegments.back();

    segment.clsRoutingPoints.resize(1);
    segment.clsRoutingPoints[0].clsPos.set(rect.lx(), rect.ly());
    segment.clsRoutingPoints[0].clsHasRectangle = true;
    segment.clsRoutingPoints[0].clsRect = {0, 0, rect.hx() - rect.lx(), rect.hy() - rect.ly()};
    segment.clsLayerName = getLayer(layerIdx).name;
}

}  // namespace db
