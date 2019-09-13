#include "PinTapConnector.h"

db::RouteStatus PinTapConnector::run() {
    // 1 Get bestBox
    auto tapXY = database.getLoc(tap);
    db::BoxOnLayer bestBox;
    db::RouteStatus status = getBestPinAccessBox(tapXY, tap.layerIdx, dbNet.pinAccessBoxes[pinIdx], bestBox);
    if (status != +db::RouteStatus::SUCC_CONN_EXT_PIN) {
        return status;
    }

    // 2 Get tapsOnPin
    const auto &layer = database.getLayer(tap.layerIdx);
    std::vector<utils::PointT<DBU>> tapsOnPin;
    if (bestBox.layerIdx != tap.layerIdx) {
        tapsOnPin.emplace_back(bestBox.cx(), bestBox.cy());
        // also need a via
        bestLinkVia.first = min(bestBox.layerIdx, tap.layerIdx);
        bestLinkVia.second = tapsOnPin[0];
    } else {
        // a tap within a pin access box
        if (min(bestBox.width(), bestBox.height()) > layer.widthForSuffOvlp) {
            auto bestBoxTmp = bestBox;
            shrinkBox(bestBoxTmp, layer.shrinkForSuffOvlp);
            tapsOnPin.push_back(bestBoxTmp.GetNearestPointTo(tapXY));
        }
        // route "into" a pin access box
        shrinkBox(bestBox, std::max<DBU>(layer.shrinkForSuffOvlp, layer.width * 0.5));
        tapsOnPin.push_back(bestBox.GetNearestPointTo(tapXY));
    }

    // 3 Get candidateLinks
    vector<vector<utils::SegmentT<DBU>>> candidateLinks;
    for (auto tapOnPin : tapsOnPin) {
        utils::PointT<DBU> turn1(tapXY.x, tapOnPin.y), turn2(tapOnPin.x, tapXY.y);
        if (layer.direction == Y) {
            // prefer routing from tap on track first (to avoid uncontrolled violations with vias)
            std::swap(turn1, turn2);
        }
        candidateLinks.push_back(getLinkFromPts({tapXY, turn1, tapOnPin}));
        candidateLinks.push_back(getLinkFromPts({tapXY, turn2, tapOnPin}));
    }

    // 4 Get bestLink
    bestVio = std::numeric_limits<int>::max();
    for (auto &candidateLink : candidateLinks) {
        int vio = getLinkPinSpaceVio(candidateLink, tap.layerIdx);
        if (vio < bestVio) {
            bestLink = move(candidateLink);
            bestVio = vio;
            if (bestVio == 0) break;
        }
    }

    return db::RouteStatus::SUCC_CONN_EXT_PIN;
}

void PinTapConnector::shrinkInterval(utils::IntervalT<DBU> &interval, DBU margin) {
    if (interval.range() < margin * 2) {
        DBU center = interval.center();
        interval.Set(center);
    } else {
        interval.low += margin;
        interval.high -= margin;
    }
}

void PinTapConnector::shrinkBox(db::BoxOnLayer &box, DBU margin) {
    shrinkInterval(box.x, margin);
    shrinkInterval(box.y, margin);
}

vector<utils::SegmentT<DBU>> PinTapConnector::getLinkFromPts(const vector<utils::PointT<DBU>> &linkPts) {
    vector<utils::SegmentT<DBU>> link;
    for (int i = 0; (i + 1) < linkPts.size(); ++i) {
        if (linkPts[i] != linkPts[i + 1]) {
            link.emplace_back(linkPts[i], linkPts[i + 1]);
        }
    }
    return link;
}

int PinTapConnector::getLinkPinSpaceVio(const vector<utils::SegmentT<DBU>> &link, int layerIdx) {
    int numVio = 0;
    for (const auto &linkSeg : link) {
        auto linkMetal = getLinkMetal(linkSeg, layerIdx);
        numVio += database.getFixedMetalVio({layerIdx, linkMetal}, dbNet.idx);
    }
    return numVio;
}

db::RouteStatus PinTapConnector::getBestPinAccessBox(const utils::PointT<DBU> &tapXY,
                                                     int layerIdx,
                                                     const std::vector<db::BoxOnLayer> &pinAccessBoxes,
                                                     db::BoxOnLayer &bestBox) {
    DBU minDist = std::numeric_limits<DBU>::max();

    // 1 Get bestBox
    // 1.1 try same-layer boxes
    for (auto &box : pinAccessBoxes) {
        if (box.layerIdx != layerIdx) {
            continue;
        }
        if (box.Contain(tapXY)) {
            return db::RouteStatus::SUCC_NORMAL;
        }
        DBU dist = utils::Dist(box, tapXY);
        if (dist < minDist) {
            minDist = dist;
            bestBox = box;
        }
    }
    // 1.2 try diff-layer boxes
    if (minDist == std::numeric_limits<DBU>::max()) {
        DBU maxArea = std::numeric_limits<DBU>::min();
        for (auto &box : pinAccessBoxes) {
            if (abs(box.layerIdx - layerIdx) == 1 && maxArea < box.area()) {
                maxArea = box.area();
                bestBox = box;
            }
        }
        if (maxArea == std::numeric_limits<DBU>::min()) {
            return db::RouteStatus::FAIL_CONN_EXT_PIN;
        }
    }

    return db::RouteStatus::SUCC_CONN_EXT_PIN;
}

utils::BoxT<DBU> PinTapConnector::getLinkMetal(const utils::SegmentT<DBU> &link, int layerIdx) {
    utils::BoxT<DBU> box(link.x, link.y);  // copy
    int dir = (box[0].range() == 0) ? 0 : 1;
    if (box[1 - dir].low > box[1 - dir].high) {
        std::swap(box[1 - dir].low, box[1 - dir].high);
    }
    DBU halfWidth = database.getLayer(layerIdx).width / 2;
    for (int d = 0; d < 2; ++d) {
        box[d].low -= halfWidth;
        box[d].high += halfWidth;
    }
    return box;
}
