#include "app/deletion_review.h"

#include <algorithm>
#include <limits>

namespace diskbloom::app {
namespace {

bool is_ancestor_or_same(
    const core::ScanTree& tree,
    const core::NodeIndex ancestor,
    core::NodeIndex node) noexcept {
    while (node != core::invalid_node && node < tree.nodes().size()) {
        if (node == ancestor) {
            return true;
        }
        node = tree.node(node).parent;
    }
    return false;
}

} // namespace

bool DeletionReview::add(const core::ScanTree& tree, const core::NodeIndex node) {
    if (node == 0U || node >= tree.nodes().size()) {
        return false;
    }
    for (const auto existing : nodes_) {
        if (is_ancestor_or_same(tree, existing, node)) {
            return false;
        }
    }

    for (std::size_t index = nodes_.size(); index > 0U; --index) {
        const auto candidate = index - 1U;
        if (is_ancestor_or_same(tree, node, nodes_[candidate])) {
            nodes_.erase(nodes_.begin() + static_cast<std::ptrdiff_t>(candidate));
            sizes_.erase(sizes_.begin() + static_cast<std::ptrdiff_t>(candidate));
        }
    }
    nodes_.push_back(node);
    sizes_.push_back(tree.node(node).logicalSize);
    recompute_total();
    return true;
}

bool DeletionReview::remove(const core::NodeIndex node) noexcept {
    const auto iterator = std::find(nodes_.begin(), nodes_.end(), node);
    if (iterator == nodes_.end()) {
        return false;
    }
    const auto index = static_cast<std::size_t>(std::distance(nodes_.begin(), iterator));
    nodes_.erase(iterator);
    sizes_.erase(sizes_.begin() + static_cast<std::ptrdiff_t>(index));
    recompute_total();
    return true;
}

void DeletionReview::clear() noexcept {
    nodes_.clear();
    sizes_.clear();
    totalBytes_ = 0U;
}

std::span<const core::NodeIndex> DeletionReview::nodes() const noexcept {
    return nodes_;
}

std::uint64_t DeletionReview::total_bytes() const noexcept {
    return totalBytes_;
}

void DeletionReview::recompute_total() noexcept {
    totalBytes_ = 0U;
    for (const auto bytes : sizes_) {
        if (bytes > std::numeric_limits<std::uint64_t>::max() - totalBytes_) {
            totalBytes_ = std::numeric_limits<std::uint64_t>::max();
            return;
        }
        totalBytes_ += bytes;
    }
}

} // namespace diskbloom::app
