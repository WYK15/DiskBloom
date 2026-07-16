#pragma once

#include "core/scan_tree.h"

#include <cstdint>
#include <span>
#include <vector>

namespace diskbloom::core {

struct ExcludedByteReduction {
    NodeIndex node = invalid_node;
    std::uint64_t bytes = 0U;
};

class ScanTreeExclusion final {
public:
    void rebuild(const ScanTree& tree, std::span<const NodeIndex> requestedRoots);
    void clear() noexcept;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] bool is_excluded_root(NodeIndex node) const noexcept;
    [[nodiscard]] bool is_visible(const ScanTree& tree, NodeIndex node) const noexcept;
    [[nodiscard]] std::uint64_t effective_size(
        const ScanTree& tree,
        NodeIndex node) const noexcept;
    [[nodiscard]] std::span<const NodeIndex> roots() const noexcept;
    [[nodiscard]] std::span<const ExcludedByteReduction> reductions() const noexcept;

private:
    std::vector<NodeIndex> roots_;
    std::vector<ExcludedByteReduction> reductions_;
};

} // namespace diskbloom::core
