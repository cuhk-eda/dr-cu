#include "CutLayer.h"

namespace db {

ViaType::ViaType(Rsyn::PhysicalVia rsynVia) {
    bot = getBoxFromRsynGeometries(rsynVia.allBottomGeometries());
    cut = getBoxFromRsynGeometries(rsynVia.allCutGeometries());
    top = getBoxFromRsynGeometries(rsynVia.allTopGeometries());
    name = rsynVia.getName();
    /*
    if (rsynVia.hasRowCol()) {
        const DBU xBotEnc = rsynVia.getEnclosure(Rsyn::BOTTOM_VIA_LEVEL, X);
        const DBU yBotEnc = rsynVia.getEnclosure(Rsyn::BOTTOM_VIA_LEVEL, Y);
        const DBU xTopEnc = rsynVia.getEnclosure(Rsyn::TOP_VIA_LEVEL, X);
        const DBU yTopEnc = rsynVia.getEnclosure(Rsyn::TOP_VIA_LEVEL, Y);
        const int numRows = rsynVia.getNumRows();
        const int numCols = rsynVia.getNumCols();
        const DBU xCut = (rsynVia.getCutSize(X) * numCols + rsynVia.getSpacing(X) * (numCols - 1) + 1) / 2;
        const DBU yCut = (rsynVia.getCutSize(Y) * numRows + rsynVia.getSpacing(Y) * (numRows - 1) + 1) / 2;
        bot = {-xCut - xBotEnc, -yCut - yBotEnc, xCut + xBotEnc, yCut + yBotEnc};
        cut = {-xCut, -yCut, xCut, yCut};
        top = {-xCut - xTopEnc, -yCut - yTopEnc, xCut + xTopEnc, yCut + yTopEnc};
    }
    */
    if (!bot.IsStrictValid() || !cut.IsStrictValid() || !top.IsStrictValid()) {
        log() << "Warning in " << __func__ << ": For " << rsynVia.getName()
              << " , has non strict valid via layer bound... " << std::endl;
    }
}

std::tuple<DBU, DBU, DBU, DBU> ViaType::getDefaultScore(const Dimension botDim, const Dimension topDim) const {
    return std::tuple<DBU, DBU, DBU, DBU>(bot[botDim].range(),       // belowWidth
                                     top[topDim].range(),       // aboveWidth
                                     bot[1 - botDim].range(),   // belowWidth
                                     top[1 - topDim].range());  // aboveLength
}

CutLayer::CutLayer(const Rsyn::PhysicalLayer& rsynLayer,
                   const vector<Rsyn::PhysicalVia>& rsynVias,
                   const Dimension botDim,
                   const Dimension topDim,
                   const DBU libDBU)
    : name(rsynLayer.getName()), idx(rsynLayer.getRelativeIndex()), width(rsynLayer.getWidth()) {
    //  Rsyn::PhysicalLayer (LEF)
    const lefiLayer* layer = rsynLayer.getLayer();
    if (!layer->hasSpacingNumber() || !layer->numSpacing()) {
        log() << "Error in " << __func__ << ": For " << rsynLayer.getName()
              << " CutLayer::init, rsynSpacingRule is empty, init all rules with default 0... " << std::endl;
    } else {
        spacing = static_cast<DBU>(std::round(layer->spacing(0) * libDBU));
    }
    delete rsynLayer.getLayer();

    //  Rsyn::PhysicalVia (LEF)
    if (rsynVias.empty()) {
        log() << "Error in " << __func__ << ": For " << name << " rsynVias is empty... " << std::endl;
    }

    defaultViaTypeIdx = -1;
    constexpr DBU dbuMax = std::numeric_limits<DBU>::has_infinity ? std::numeric_limits<DBU>::infinity() : std::numeric_limits<DBU>::max();
    std::tuple<DBU, DBU, DBU, DBU> bestScore(dbuMax, dbuMax, dbuMax, dbuMax);
    for (const Rsyn::PhysicalVia& rsynVia : rsynVias) {
        if (rsynVia.allBottomGeometries().size() != 1 || rsynVia.allCutGeometries().size() != 1 ||
            rsynVia.allTopGeometries().size() != 1) {
            log() << "Warning in " << __func__ << ": For " << rsynVia.getName()
                  << " , has not exactly one via layer bound... " << std::endl;
            continue;
        }

        if (rsynVia.isViaDesign()) {
            continue;
        }

        allViaTypes.emplace_back(rsynVia);
        const std::tuple<DBU, DBU, DBU, DBU>& score = allViaTypes.back().getDefaultScore(botDim, topDim);
        if (score < bestScore) {
            bestScore = score;
            defaultViaTypeIdx = allViaTypes.size() - 1;
        }
    }

    if (defaultViaTypeIdx == -1) {
        log() << "Error in " << __func__ << ": For " << name << " all rsyn vias have not exactly one via bound... " << std::endl;
    }
}

ostream& CutLayer::printBasics(ostream& os) const {
    os << name << ": idx=" << idx << ", viaTypes=" << defaultViaType().name << " (";
    for (auto via : allViaTypes) {
        if (via.name != defaultViaType().name) {
            os << via.name << " ";
        }
    }
    os << ")";
    return os;
}

ostream& CutLayer::printDesignRules(ostream& os) const {
    os << name << ": width=" << width << ", space=" << spacing;
    return os;
}

ostream& CutLayer::printViaOccupancyLUT(ostream& os) const {
    os << name << ": viaCut(" << viaCut.size() << ',' << viaCut[0].size() << ")";
    os << ", viaMetal(" << viaMetal.size() << ',' << viaMetal[0].size() << ")";
    // TODO: make xSize member variables, since they will be the same in a LUT over all cps
    auto getMaxSize = [](const vector<vector<vector<bool>>>& LUT, size_t& xSize, size_t& ySize) {
        xSize = 0;
        ySize = 0;
        for (const vector<vector<bool>>& cpLUT : LUT) {
            if (cpLUT.size()) {
                xSize = max(xSize, cpLUT.size());
                ySize = max(ySize, cpLUT[0].size());
            }
        }
    };
    size_t xSize, ySize;
    getMaxSize(viaBotVia, xSize, ySize);
    os << ", viaBotVia(" << viaBotVia.size() << ',' << xSize << ',' << ySize << ")";
    getMaxSize(viaTopVia, xSize, ySize);
    os << ", viaTopVia(" << viaTopVia.size() << ',' << xSize << ',' << ySize << ")";
    getMaxSize(viaBotWire, xSize, ySize);
    os << ", viaBotWire(" << viaBotWire.size() << ',' << xSize << ',' << ySize << ")";
    getMaxSize(viaTopWire, xSize, ySize);
    os << ", viaTopWire(" << viaTopWire.size() << ',' << xSize << ',' << ySize << ")";
    return os;
}

ostream& operator<<(ostream& os, const CutLayer& layer) { return layer.printBasics(os); }

}  //   namespace db
