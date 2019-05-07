#include "PinTapConnector.h"

db::RouteStatus PinTapConnector::run() {
    auto tapXY = database.getLoc(tap);
    db::BoxOnLayer bestBox;
    DBU minDist = std::numeric_limits<DBU>::max();

    for (auto &box : dbNet.pinAccessBoxes[pinIdx]) {
        if (box.layerIdx != tap.layerIdx) {
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
    if (minDist == std::numeric_limits<DBU>::max()) {
        return db::RouteStatus::FAIL_CONN_EXT_PIN;
    }

    const auto &layer = database.getLayer(bestBox.layerIdx);
    auto tapOnPin = bestBox.GetNearestPointTo(tapXY);   // a tap within a pin access box
    shrinkBox(bestBox, layer.width / 2);                // route "into" a pin access box
    auto tapOnPin2 = bestBox.GetNearestPointTo(tapXY);  // another tap within a pin access box
    vector<vector<utils::SegmentT<DBU>>> candidateLinks = {getLinkFromPts({tapXY, {tapXY.x, tapOnPin.y}, tapOnPin}),
                                                           getLinkFromPts({tapXY, {tapOnPin.x, tapXY.y}, tapOnPin}),
                                                           getLinkFromPts({tapXY, {tapXY.x, tapOnPin2.y}, tapOnPin2}),
                                                           getLinkFromPts({tapXY, {tapOnPin2.x, tapXY.y}, tapOnPin2})};
    if (layer.direction == Y) {
        // prefer routing from tap on track first (to avoid uncontrolled violations with vias)
        std::swap(candidateLinks[0], candidateLinks[1]);
        std::swap(candidateLinks[2], candidateLinks[3]);
    }
    bestVio = std::numeric_limits<DBU>::max();
    for (auto &candidateLink : candidateLinks) {
        DBU vio = getLinkPinSpaceVio(candidateLink, tap.layerIdx);
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

DBU PinTapConnector::getLinkPinSpaceVio(const vector<utils::SegmentT<DBU>> &link, int layerIdx) {
    DBU area = 0;
    for (const auto &linkSeg : link) {
        auto linkMetal = getLinkMetal(linkSeg, layerIdx);
        // to neighbors
        auto forbidRegions = database.getAccurateMetalRectForbidRegions({layerIdx, linkMetal});
        for (const auto &region : forbidRegions) {
            area += database.getOvlpFixedMetalArea({layerIdx, region}, dbNet.idx);
        }
        // from neighbors
        area += database.getOvlpFixedMetalForbidRegionArea({layerIdx, linkMetal}, dbNet.idx);
    }
    return area;
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
