#pragma once

#include "core/scan_tree.h"

#include <cstdint>
#include <span>
#include <vector>

namespace diskbloom::app {

class DeletionReview final {
public:
    [[nodiscard]] bool add(const core::ScanTree& tree, core::NodeIndex node);
    [[nodiscard]] bool remove(core::NodeIndex node) noexcept;
    void clear() noexcept;

    [[nodiscard]] std::span<const core::NodeIndex> nodes() const noexcept;
    [[nodiscard]] std::uint64_t total_bytes() const noexcept;

private:
    void recompute_total() noexcept;

    std::vector<core::NodeIndex> nodes_;
    std::vector<std::uint64_t> sizes_;
    std::uint64_t totalBytes_ = 0U;
};

} // namespace diskbloom::app
