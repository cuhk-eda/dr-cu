#include "Net.h"

#include <fstream>

#include "Setting.h"

namespace db {
    
NetBase::~NetBase() {
    postOrderVisitGridTopo([](std::shared_ptr<GridSteiner> node){
        node->children.clear(); // free the memory
    });
}

BoxOnLayer NetBase::getMaxAccessBox(int pinIdx) const {
    DBU maxArea = std::numeric_limits<DBU>::min();
    db::BoxOnLayer bestBox;
    for (const auto &box : pinAccessBoxes[pinIdx]) {
        if (maxArea < box.area()) {
            maxArea = box.area();
            bestBox = box;
        }
    }
    return bestBox;
}

void NetBase::postOrderVisitGridTopo(const std::function<void(std::shared_ptr<GridSteiner>)>& visit) const {
    for (const std::shared_ptr<GridSteiner>& tree : gridTopo) {
        GridSteiner::postOrder(tree, visit);
    }
}

void NetBase::printBasics(ostream& os) const {
    os << "Net " << getName() << " (idx = " << idx << ") with " << numOfPins() << " pins " << std::endl;
    for (int i = 0; i < numOfPins(); ++i) {
        os << "pin " << i << " " << rsynPins[i].getInstanceName() << std::endl;
        for (auto& accessBox : pinAccessBoxes[i]) {
            os << accessBox << std::endl;
        }
    }

    os << routeGuides.size() << " route guides" << std::endl;
    if (routeGuides.size() == gridRouteGuides.size()) {
        for (int i = 0; i < routeGuides.size(); ++i) {
            os << routeGuides[i] << " " << gridRouteGuides[i] << std::endl;
        }
    }
    else {
        for (auto& routeGuide : routeGuides) {
            os << routeGuide << std::endl;
        }
    }

    os << std::endl;
}

void NetBase::printResult(ostream& os) const {
    os << "grid topo: " << std::endl;
    for (const auto& tree : gridTopo) {
        tree->printTree(os);
        os << std::endl;
    }
    os << "extend seg: " << std::endl;
    postOrderVisitGridTopo([&](std::shared_ptr<GridSteiner> node) {
        if (node->extWireSeg) {
            os << *(node->extWireSeg) << " ";
        }
    });
    os << std::endl;
}

Net::Net(int i, Rsyn::Net net, RsynService& rsynService) {
    idx = i;
    rsynNet = net;

    // pins
    pinAccessBoxes.reserve(net.getNumPins());
    const Rsyn::Session session;
    const Rsyn::PhysicalDesign& physicalDesign = static_cast<Rsyn::PhysicalService*>(session.getService("rsyn.physical"))->getPhysicalDesign();
    const DBU libDBU = physicalDesign.getDatabaseUnits(Rsyn::LIBRARY_DBU);
    for (auto RsynPin : net.allPins()) {
        rsynPins.push_back(RsynPin);
        pinAccessBoxes.emplace_back();
        initPinAccessBoxes(RsynPin, rsynService, pinAccessBoxes.back(), libDBU);
    }

    // route guides
    const Rsyn::NetGuide& netGuide = rsynService.routeGuideService->getGuide(net);
    for (const Rsyn::LayerGuide& layerGuide : netGuide.allLayerGuides()) {
        auto bounds = layerGuide.getBounds();
        routeGuides.emplace_back(layerGuide.getLayer().getRelativeIndex(), getBoxFromRsynBounds(bounds));
    }
    routeGuideVios.resize(routeGuides.size(), 0);
}

void Net::clearPostRouteResult() {
    defWireSegments.clear();
}

void Net::clearResult() {
    gridTopo.clear();
    clearPostRouteResult();
}

void Net::stash() {
    routeGuideVios_copy = routeGuideVios;
    routeGuideRTrees_copy = routeGuideRTrees;
    gridTopo_copy = gridTopo;
}

void Net::reset() {
    routeGuideVios = (routeGuideVios_copy);
    routeGuideRTrees = (routeGuideRTrees_copy);
    gridTopo = gridTopo_copy;
}

void Net::initPinAccessBoxes(Rsyn::Pin rsynPin, RsynService& rsynService, vector<BoxOnLayer>& accessBoxes, const DBU libDBU) {
    // PhysicalPort
    if (rsynPin.isPort()) {
        Rsyn::PhysicalPort phPort = rsynService.physicalDesign.getPhysicalPort(rsynPin.getPort());
        getPinAccessBoxes(phPort, accessBoxes);
        return;
    }

    // PhysicalLibraryPin
    Rsyn::PhysicalLibraryPin phLibPin = rsynService.physicalDesign.getPhysicalLibraryPin(rsynPin);

    // PhysicalCell
    Rsyn::Instance instance = rsynPin.getInstance();
    if (instance.getType() != Rsyn::CELL) {
        log() << "Warning: pin is not on a cell " << rsynPin.getNetName() << " " << rsynPin.getInstanceName() << std::endl;
        return;
    }
    Rsyn::Cell cell = instance.asCell();
    Rsyn::PhysicalCell phCell = rsynService.physicalDesign.getPhysicalCell(cell);
    Rsyn::PhysicalLibraryCell phLibCell = rsynService.physicalDesign.getPhysicalLibraryCell(cell);
    const DBUxy origin(static_cast<DBU>(std::round(phLibCell.getMacro()->originX() * libDBU)),
                       static_cast<DBU>(std::round(phLibCell.getMacro()->originY() * libDBU)));

    // fill accessBoxes
    getPinAccessBoxes(phLibPin, phCell, accessBoxes, origin);
};

void Net::getPinAccessBoxes(Rsyn::PhysicalPort phPort, vector<BoxOnLayer>& accessBoxes) {
    auto displacement = phPort.getPosition();
    auto bounds = phPort.getBounds();
    Bounds dummyCellBounds(displacement, displacement);
    Rsyn::PhysicalTransform transform(dummyCellBounds, phPort.getOrientation());
    bounds.translate(displacement);
    bounds = transform.apply(bounds);
    accessBoxes.emplace_back(phPort.getLayer().getRelativeIndex(), getBoxFromRsynBounds(bounds));
}

void Net::getPinAccessBoxes(Rsyn::PhysicalLibraryPin phLibPin,
                            Rsyn::PhysicalCell phCell,
                            vector<BoxOnLayer>& accessBoxes,
                            const DBUxy& origin) {
    if (!phLibPin.hasPinGeometries()) {
        log() << "Warning: pin of " << phCell.getName() << " has no pinGeometries" << std::endl;
        return;
    }

    const DBUxy displacement = phCell.getPosition() + origin;
    auto transform = phCell.getTransform();
    // for (Rsyn::PhysicalPinGeometry phPinGeo : phLibPin.allPinGeometries()) {
    // TODO: check why multiple PinGeometry on 8t4 inst60849
    auto phPinGeo = phLibPin.allPinGeometries()[0];
    for (Rsyn::PhysicalPinLayer phPinLayer : phPinGeo.allPinLayers()) {
        if (!phPinLayer.hasRectangleBounds()) {
            log() << "Warning: pin has no RectangleBounds" << std::endl;
            continue;
        }
        int layerIdx = phPinLayer.getLayer().getRelativeIndex();
        for (auto bounds : phPinLayer.allBounds()) {
            bounds.translate(displacement);
            bounds = transform.apply(bounds);
            accessBoxes.emplace_back(layerIdx, getBoxFromRsynBounds(bounds));
        }
    }
}

void NetList::init(RsynService& rsynService) {
    if (db::setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << "Init NetList ..." << std::endl;
    }
    nets.clear();
    nets.reserve(rsynService.design.getNumNets());
    int numPins = 0;
    for (Rsyn::Net net : rsynService.module.allNets()) {
        switch(net.getUse()) {
            case Rsyn::POWER:
                continue;
            case Rsyn::GROUND:
                continue;
            default:
                break;
        }
        nets.emplace_back(nets.size(), net, rsynService);
        numPins += nets.back().pinAccessBoxes.size();
    }
    if (setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << "The number of nets is " << nets.size() << std::endl;
        log() << "The number of pins is " << numPins << std::endl;
        log() << std::endl;
    }
}

void NetList::writeNetTopo(const std::string& filename) {
    if (!setting.dbWriteDebugFile) {
        return;
    }
    log() << "Write net topologies to " << filename << " ..." << std::endl;
    log() << std::endl;
    std::ofstream ofs(filename);

    for (const auto& net : nets) {
        ofs << net.getName() << " (idx = " << net.idx << ")" << std::endl;
        net.printResult(ofs);
    }
}

}  // namespace db
