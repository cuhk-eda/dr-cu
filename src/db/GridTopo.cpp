#include "GridTopo.h"

namespace db {

void GridSteiner::setParent(std::shared_ptr<GridSteiner> childNode, std::shared_ptr<GridSteiner> parentNode) {
    parentNode->children.push_back(childNode);
    childNode->parent = parentNode;
}

void GridSteiner::resetParent(std::shared_ptr<GridSteiner> node) {
    assert(node->parent);

    auto& n = node->parent->children;
    auto it = find(n.begin(), n.end(), node);
    assert(it != n.end());
    *it = n.back();
    n.pop_back();

    node->parent.reset();
}

void GridSteiner::preOrder(std::shared_ptr<GridSteiner> node, const std::function<void(std::shared_ptr<GridSteiner>)>& visit) {
    visit(node);
    for (auto c : node->children) preOrder(c, visit);
}

void GridSteiner::postOrder(std::shared_ptr<GridSteiner> node, const std::function<void(std::shared_ptr<GridSteiner>)>& visit) {
    for (auto c : node->children) postOrder(c, visit);
    visit(node);
}

void GridSteiner::postOrderCopy(std::shared_ptr<GridSteiner> node, const std::function<void(std::shared_ptr<GridSteiner>)>& visit) {
    auto tmp = node->children;
    for (auto c : tmp) postOrderCopy(c, visit);
    visit(node);
}

void GridSteiner::mergeNodes(std::shared_ptr<GridSteiner> root) {
    postOrderCopy(root, [](std::shared_ptr<GridSteiner> node) {
        // parent - node - child
        if (node->parent && node->parent->layerIdx == node->layerIdx && node->children.size() == 1 &&
            node->pinIdx < 0) {
            auto oldChild = node->children[0];
            if (node->layerIdx == oldChild->layerIdx && node->trackIdx == oldChild->trackIdx &&
                node->parent->trackIdx == node->trackIdx) {
                auto oldParent = node->parent;
                resetParent(node);
                resetParent(oldChild);
                setParent(oldChild, oldParent);
            }
        }
    });
}

ostream& operator<<(ostream& os, const GridSteiner& node) {
    os << GridPoint(node);
    if (node.pinIdx >= 0) os << " pin" << node.pinIdx;
    return os;
}

void GridSteiner::printTree(ostream& os, int depth) {
    os << *this << ", ";
    bool first = true;
    for (auto child : children) {
        if (!first) {
            for (int i = 0; i < depth; ++i) os << '.';
            os << *this << ", ";
        }
        child->printTree(os, depth + 1);
        if (children.size() > 1) {
            os << std::endl;
        }
        first = false;
    }
}

}  // namespace db
