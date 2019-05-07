#pragma once

#include "RsynService.h"
#include "GridTopo.h"

namespace db {

class NetRouteResult {
public:
    vector<std::shared_ptr<GridSteiner>> gridTopo;  // major topology on grid
    vector<GridEdge> extendedWireSegment;  // wire segment extended for safisfying min area
    vector<DefWireDscp> topo;
    int minAreaVio = 0;
    int minAreaShadowedVio = 0;
    int viaPinVio = 0;

    void postOrderVisitGridTopo(const std::function<void(std::shared_ptr<GridSteiner>)>& visit) const;
    void printResult(ostream& os) const;
    void clearPostRouteResult();
    void clearResult();
};

class Net : public NetRouteResult {
public:
    int idx;
    Rsyn::Net rsynNet;
    const std::string& getName() const { return rsynNet.getName(); }

    vector<Rsyn::Pin> rsynPins;
    vector<vector<BoxOnLayer>> pinAccessBoxes;  // (pinIdx, accessBoxIdx) -> BoxOnLayer
    int numOfPins() const { return pinAccessBoxes.size(); }

    vector<BoxOnLayer> routeGuides;
    vector<GridBoxOnLayer> gridRouteGuides;

    Net(int i, Rsyn::Net net, RsynService& rsynService);
    void initPinAccessBoxes(Rsyn::Pin rsynPin, RsynService& rsynService, vector<BoxOnLayer>& accessBoxes, const DBU libDBU);
    void print(ostream& os = std::cout) const {
        printBasics(os);
        printResult(os);
    }
    void printBasics(ostream& os) const;

    static void getPinAccessBoxes(Rsyn::PhysicalPort phPort, vector<BoxOnLayer>& accessBoxes);
    static void getPinAccessBoxes(Rsyn::PhysicalLibraryPin phLibPin,
                                  Rsyn::PhysicalCell phCell,
                                  vector<BoxOnLayer>& accessBoxes,
                                  const DBUxy& origin);
};

class NetList {
public:
    vector<Net> nets;

    void init(RsynService& rsynService);
    void writeNetTopo(const std::string& filename);
};

}  // namespace db
