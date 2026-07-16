#include "core/scan_tree_exclusion.h"

#include <algorithm>
#include <limits>

namespace diskbloom::core {
namespace {

[[nodiscard]] std::uint64_t saturating_add(
    const std::uint64_t left,
    const std::uint64_t right) noexcept {
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    return left > maximum - right ? maximum : left + right;
}

} // namespace

void ScanTreeExclusion::rebuild(
    const ScanTree& tree,
    const std::span<const NodeIndex> requestedRoots) {
    clear();
    const auto nodes = tree.nodes();
    if (nodes.empty() || requestedRoots.empty()) {
        return;
    }

    roots_.reserve(requestedRoots.size());
    for (const auto candidate : requestedRoots) {
        if (candidate < nodes.size() && nodes[candidate].parent != invalid_node) {
            roots_.push_back(candidate);
        }
    }
    std::ranges::sort(roots_);
    roots_.erase(std::unique(roots_.begin(), roots_.end()), roots_.end());

    std::vector<NodeIndex> normalized;
    normalized.reserve(roots_.size());
    for (const auto candidate : roots_) {
        auto ancestor = nodes[candidate].parent;
        bool covered = false;
        while (ancestor != invalid_node && ancestor < nodes.size()) {
            if (std::ranges::binary_search(normalized, ancestor)) {
                covered = true;
                break;
            }
            ancestor = nodes[ancestor].parent;
        }
        if (!covered) {
            normalized.push_back(candidate);
        }
    }
    roots_ = std::move(normalized);

    std::vector<ExcludedByteReduction> pending;
    for (const auto root : roots_) {
        const auto bytes = nodes[root].logicalSize;
        auto ancestor = nodes[root].parent;
        while (ancestor != invalid_node && ancestor < nodes.size()) {
            pending.push_back({ancestor, bytes});
            ancestor = nodes[ancestor].parent;
        }
    }
    std::ranges::sort(pending, {}, &ExcludedByteReduction::node);

    reductions_.reserve(pending.size());
    for (const auto& reduction : pending) {
        if (!reductions_.empty() && reductions_.back().node == reduction.node) {
            reductions_.back().bytes = saturating_add(
                reductions_.back().bytes,
                reduction.bytes);
        } else {
            reductions_.push_back(reduction);
        }
    }
}

void ScanTreeExclusion::clear() noexcept {
    roots_.clear();
    reductions_.clear();
}

bool ScanTreeExclusion::empty() const noexcept {
    return roots_.empty();
}

bool ScanTreeExclusion::is_excluded_root(const NodeIndex node) const noexcept {
    return std::ranges::binary_search(roots_, node);
}

bool ScanTreeExclusion::is_visible(const ScanTree& tree, const NodeIndex node) const noexcept {
    const auto nodes = tree.nodes();
    if (node >= nodes.size()) {
        return false;
    }

    auto candidate = node;
    while (candidate != invalid_node && candidate < nodes.size()) {
        if (is_excluded_root(candidate)) {
            return false;
        }
        candidate = nodes[candidate].parent;
    }
    return true;
}

std::uint64_t ScanTreeExclusion::effective_size(
    const ScanTree& tree,
    const NodeIndex node) const noexcept {
    const auto nodes = tree.nodes();
    if (node >= nodes.size() || !is_visible(tree, node)) {
        return 0U;
    }

    const auto match = std::ranges::lower_bound(
        reductions_,
        node,
        {},
        &ExcludedByteReduction::node);
    const auto reduction = match != reductions_.end() && match->node == node
        ? match->bytes
        : 0U;
    const auto size = nodes[node].logicalSize;
    return reduction >= size ? 0U : size - reduction;
}

std::span<const NodeIndex> ScanTreeExclusion::roots() const noexcept {
    return roots_;
}

std::span<const ExcludedByteReduction> ScanTreeExclusion::reductions() const noexcept {
    return reductions_;
}

} // namespace diskbloom::core
