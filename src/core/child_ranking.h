#pragma once

#include "core/scan_tree.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace diskbloom::core {

class ScanTreeExclusion;

struct RankedChild {
    NodeIndex node = invalid_node;
    std::uint64_t logicalSize = 0U;
};

[[nodiscard]] std::vector<RankedChild> rank_children(
    const ScanTree& tree,
    NodeIndex parent,
    std::size_t limit,
    const ScanTreeExclusion* exclusion = nullptr);

} // namespace diskbloom::core
