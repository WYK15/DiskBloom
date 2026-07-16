#pragma once

#include "app/analyzer_geometry.h"
#include "core/scan_tree.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace diskbloom::app {

struct ReviewRowLayout {
    AnalyzerRectF bounds;
    AnalyzerRectF nameBounds;
    AnalyzerRectF sizeBounds;
    std::size_t itemIndex = 0U;
};

struct ReviewCollectorLayout {
    AnalyzerRectF summary;
    AnalyzerRectF panel;
    std::vector<ReviewRowLayout> rows;
    std::size_t visibleCapacity = 0U;
    std::size_t scrollOffset = 0U;
};

[[nodiscard]] ReviewCollectorLayout compute_review_collector_layout(
    const AnalyzerRectF& actionBar,
    float widthDip,
    float heightDip,
    std::size_t itemCount,
    std::size_t scrollOffset);

[[nodiscard]] std::size_t next_review_scroll_offset(
    std::size_t current,
    std::size_t itemCount,
    std::size_t visibleCount,
    int deltaRows) noexcept;

[[nodiscard]] bool contains_point(
    const AnalyzerRectF& bounds,
    float xDip,
    float yDip) noexcept;

class ReviewDragState final {
public:
    [[nodiscard]] bool begin(core::NodeIndex node, float xDip, float yDip) noexcept;
    [[nodiscard]] bool move(float xDip, float yDip, bool overCollector) noexcept;
    [[nodiscard]] std::optional<core::NodeIndex> release(bool overCollector) noexcept;
    [[nodiscard]] bool cancel() noexcept;
    [[nodiscard]] bool pending() const noexcept;
    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] bool valid_drop() const noexcept;
    [[nodiscard]] core::NodeIndex node() const noexcept;

private:
    core::NodeIndex node_ = core::invalid_node;
    float startX_ = 0.0F;
    float startY_ = 0.0F;
    bool pending_ = false;
    bool active_ = false;
    bool validDrop_ = false;
};

} // namespace diskbloom::app
