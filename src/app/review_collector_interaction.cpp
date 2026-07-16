#include "app/review_collector_interaction.h"

#include <algorithm>

namespace diskbloom::app {

ReviewCollectorLayout compute_review_collector_layout(
    const AnalyzerRectF& actionBar,
    const float widthDip,
    const float heightDip,
    const std::size_t itemCount,
    const std::size_t scrollOffset) {
    constexpr float leftInset = 20.0F;
    constexpr float minimumPanelWidth = 360.0F;
    constexpr float maximumPanelWidth = 620.0F;
    constexpr float rowHeight = 36.0F;
    constexpr float panelPadding = 12.0F;
    constexpr float minimumPanelTop = 64.0F;
    constexpr float sizeWidth = 96.0F;

    ReviewCollectorLayout layout;
    const auto viewportWidth = std::max(widthDip, 0.0F);
    const auto viewportHeight = std::max(heightDip, 0.0F);
    const auto panelWidth = std::clamp(
        viewportWidth - leftInset * 2.0F,
        minimumPanelWidth,
        maximumPanelWidth);
    const auto panelRight = std::max(
        leftInset,
        std::min(leftInset + panelWidth, viewportWidth - leftInset));
    const auto panelBottom = std::min(actionBar.top + 1.0F, viewportHeight);
    const auto availableRowsHeight = std::max(
        panelBottom - minimumPanelTop - panelPadding * 2.0F,
        0.0F);
    layout.visibleCapacity = static_cast<std::size_t>(availableRowsHeight / rowHeight);
    layout.scrollOffset = std::min(
        scrollOffset,
        itemCount > layout.visibleCapacity ? itemCount - layout.visibleCapacity : 0U);

    const auto visibleCount = std::min(
        layout.visibleCapacity,
        itemCount - std::min(itemCount, layout.scrollOffset));
    const auto panelHeight = panelPadding * 2.0F
        + static_cast<float>(visibleCount) * rowHeight;
    const auto panelTop = std::max(minimumPanelTop, panelBottom - panelHeight);

    layout.summary = {
        leftInset,
        actionBar.top,
        panelRight,
        actionBar.bottom,
    };
    layout.panel = {
        leftInset,
        panelTop,
        panelRight,
        panelBottom,
    };
    layout.rows.reserve(layout.visibleCapacity);
    const auto rowLeft = layout.panel.left + panelPadding;
    const auto rowRight = std::max(rowLeft, layout.panel.right - panelPadding);
    const auto sizeLeft = std::max(rowLeft, rowRight - sizeWidth);
    for (std::size_t index = 0U; index < visibleCount; ++index) {
        const auto top = panelTop + panelPadding + static_cast<float>(index) * rowHeight;
        layout.rows.push_back({
            .bounds = {rowLeft, top, rowRight, top + rowHeight},
            .nameBounds = {rowLeft, top, std::max(rowLeft, sizeLeft - 8.0F), top + rowHeight},
            .sizeBounds = {sizeLeft, top, rowRight, top + rowHeight},
            .itemIndex = layout.scrollOffset + index,
        });
    }
    return layout;
}

std::size_t next_review_scroll_offset(
    const std::size_t current,
    const std::size_t itemCount,
    const std::size_t visibleCount,
    const int deltaRows) noexcept {
    const auto maximum = itemCount > visibleCount ? itemCount - visibleCount : 0U;
    const auto next = static_cast<long long>(std::min(current, maximum))
        + static_cast<long long>(deltaRows);
    return static_cast<std::size_t>(std::clamp<long long>(next, 0LL, maximum));
}

bool contains_point(
    const AnalyzerRectF& bounds,
    const float xDip,
    const float yDip) noexcept {
    return xDip >= bounds.left && xDip < bounds.right
        && yDip >= bounds.top && yDip < bounds.bottom;
}

bool ReviewDragState::begin(
    const core::NodeIndex node,
    const float xDip,
    const float yDip) noexcept {
    (void)cancel();
    if (node == core::invalid_node) {
        return false;
    }
    node_ = node;
    startX_ = xDip;
    startY_ = yDip;
    pending_ = true;
    return true;
}

bool ReviewDragState::move(
    const float xDip,
    const float yDip,
    const bool overCollector) noexcept {
    if (!pending_ && !active_) {
        return false;
    }

    auto changed = false;
    if (pending_) {
        const auto deltaX = xDip - startX_;
        const auto deltaY = yDip - startY_;
        if (deltaX * deltaX + deltaY * deltaY >= 36.0F) {
            pending_ = false;
            active_ = true;
            changed = true;
        }
    }

    const auto validDrop = active_ && overCollector;
    if (validDrop_ != validDrop) {
        validDrop_ = validDrop;
        changed = true;
    }
    return changed;
}

std::optional<core::NodeIndex> ReviewDragState::release(
    const bool overCollector) noexcept {
    const auto droppedNode = active_ && overCollector
        ? std::optional<core::NodeIndex>{node_}
        : std::nullopt;
    (void)cancel();
    return droppedNode;
}

bool ReviewDragState::cancel() noexcept {
    const auto changed = pending_ || active_;
    node_ = core::invalid_node;
    startX_ = 0.0F;
    startY_ = 0.0F;
    pending_ = false;
    active_ = false;
    validDrop_ = false;
    return changed;
}

bool ReviewDragState::pending() const noexcept {
    return pending_;
}

bool ReviewDragState::active() const noexcept {
    return active_;
}

bool ReviewDragState::valid_drop() const noexcept {
    return validDrop_;
}

core::NodeIndex ReviewDragState::node() const noexcept {
    return node_;
}

} // namespace diskbloom::app
