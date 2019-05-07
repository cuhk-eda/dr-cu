#pragma once

#include "GeoPrimitive.h"

namespace db {

class ViaType {
public:
    utils::BoxT<DBU> bot;  // box on bottom metal layer
    utils::BoxT<DBU> top;  // box on top metal layer
    utils::BoxT<DBU> cut;  // box on cut layer
    std::string name;

    vector<utils::BoxT<DBU>> botForbidRegions;
    vector<utils::BoxT<DBU>> topForbidRegions;

    // via-wire conflict (crossPointIdx, trackIdx, crossPointIdx)
    vector<vector<vector<bool>>> viaBotWire;
    vector<vector<vector<bool>>> viaTopWire;

    ViaType() {}
    ViaType(Rsyn::PhysicalVia rsynVia);

    // alphabetical score tuple (belowWidth, aboveWidth, belowLength, aboveLength)
    std::tuple<DBU, DBU, DBU, DBU> getDefaultScore(const Dimension botDim, const Dimension topDim) const;
};

class CutLayer {
public:
    CutLayer(const Rsyn::PhysicalLayer& rsynLayer,
             const vector<Rsyn::PhysicalVia>& rsynVias,
             const Dimension botDim,
             const Dimension topDim,
             const DBU libDBU);

    // Basic infomation
    std::string name;
    int idx;  // layerIdx (consistent with Rsyn::xxx::getRelativeIndex())

    // Design rules
    DBU width = 0;
    DBU spacing = 0;

    // Via types
    vector<ViaType> allViaTypes;
    int defaultViaTypeIdx;  //  along preferred direction in both metal layers
    const ViaType& defaultViaType() const { return allViaTypes[defaultViaTypeIdx]; }
    bool isDefaultViaType(const ViaType& viaType) const { return viaType.name == defaultViaType().name; }
    utils::BoxT<DBU> topMaxForbidRegion;
    utils::BoxT<DBU> botMaxForbidRegion;

    // Via conflict lookup table (true means "not available" / with conflict)
    // 1. same-layer via-via conflict (lowerTrackIdx, upperTrackIdx)
    vector<vector<bool>> viaCut;    // due to cut spacing
    vector<vector<bool>> viaMetal;  // due to metal spacing
    // 2. cross-layer via-via conflict (crossPointIdx, trackIdx, crossPointIdx)
    vector<vector<vector<bool>>> viaBotVia;
    vector<vector<vector<bool>>> viaTopVia;
    // 3. via-wire conflict (crossPointIdx, trackIdx, crossPointIdx)
    // TODO: move into ViaType
    vector<vector<vector<bool>>> viaBotWire;
    vector<vector<vector<bool>>> viaTopWire;

    ostream& printBasics(ostream& os) const;
    ostream& printDesignRules(ostream& os) const;
    ostream& printViaOccupancyLUT(ostream& os) const;
    friend ostream& operator<<(ostream& os, const CutLayer& layer);
};

}  // namespace db
