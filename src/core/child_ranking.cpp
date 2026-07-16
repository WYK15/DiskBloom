#include "core/child_ranking.h"

#include "core/scan_tree_exclusion.h"

#include <algorithm>
#include <queue>

namespace diskbloom::core {
namespace {

bool better(const RankedChild& left, const RankedChild& right) noexcept {
    return left.logicalSize > right.logicalSize
        || (left.logicalSize == right.logicalSize && left.node < right.node);
}

struct BetterComparator {
    bool operator()(const RankedChild& left, const RankedChild& right) const noexcept {
        return better(left, right);
    }
};

} // namespace

std::vector<RankedChild> rank_children(
    const ScanTree& tree,
    const NodeIndex parent,
    const std::size_t limit,
    const ScanTreeExclusion* const exclusion) {
    if (parent >= tree.nodes().size() || limit == 0U) {
        return {};
    }

    std::priority_queue<RankedChild, std::vector<RankedChild>, BetterComparator> top;
    for (auto child = tree.node(parent).firstChild;
         child != invalid_node;
         child = tree.node(child).nextSibling) {
        const auto logicalSize = exclusion != nullptr
            ? exclusion->effective_size(tree, child)
            : tree.node(child).logicalSize;
        if (exclusion != nullptr && logicalSize == 0U) {
            continue;
        }
        const RankedChild candidate{child, logicalSize};
        if (top.size() < limit) {
            top.push(candidate);
        } else if (better(candidate, top.top())) {
            top.pop();
            top.push(candidate);
        }
    }

    std::vector<RankedChild> result;
    result.reserve(top.size());
    while (!top.empty()) {
        result.push_back(top.top());
        top.pop();
    }
    std::sort(result.begin(), result.end(), better);
    return result;
}

} // namespace diskbloom::core
