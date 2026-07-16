#include "test_support.h"

#include "app/review_collector_interaction.h"
#include "core/scan_tree.h"

#include <optional>

using diskbloom::app::AnalyzerRectF;
using diskbloom::app::ReviewCollectorVisualToken;
using diskbloom::app::ReviewDragState;
using diskbloom::app::ReviewScrollTarget;
using diskbloom::app::compute_review_collector_layout;
using diskbloom::app::compute_review_drag_visual;
using diskbloom::app::contains_point;
using diskbloom::app::hit_test_review_restore;
using diskbloom::app::next_review_scroll_offset;
using diskbloom::app::review_scroll_target_at;

namespace core = diskbloom::core;

TEST_CASE(review_collector_panel_is_anchored_above_summary_and_clipped_to_viewport) {
    const AnalyzerRectF actionBar{0.0F, 640.0F, 1200.0F, 720.0F};
    const auto layout = compute_review_collector_layout(
        actionBar, 1200.0F, 720.0F, 20U, 0U);
    CHECK(layout.summary.left == 20.0F);
    CHECK(layout.summary.bottom == actionBar.bottom);
    CHECK(layout.panel.bottom == actionBar.top + 1.0F);
    CHECK(layout.panel.top >= 64.0F);
    CHECK(layout.visibleCapacity < 20U);
    CHECK(layout.rows.size() == layout.visibleCapacity);
}

TEST_CASE(review_collector_scroll_offset_is_clamped) {
    CHECK(next_review_scroll_offset(0U, 20U, 6U, 3) == 3U);
    CHECK(next_review_scroll_offset(13U, 20U, 6U, 4) == 14U);
    CHECK(next_review_scroll_offset(0U, 2U, 6U, -1) == 0U);
}

TEST_CASE(review_collector_rows_reserve_non_overlapping_restore_targets) {
    const AnalyzerRectF actionBar{0.0F, 640.0F, 1200.0F, 720.0F};
    const auto layout = compute_review_collector_layout(
        actionBar, 1200.0F, 720.0F, 4U, 0U);
    CHECK(layout.rows.size() == 4U);

    for (const auto& row : layout.rows) {
        CHECK(row.restoreBounds.right - row.restoreBounds.left >= 28.0F);
        CHECK(row.restoreBounds.bottom - row.restoreBounds.top >= 28.0F);
        CHECK(row.restoreBounds.left >= row.bounds.left);
        CHECK(row.restoreBounds.top >= row.bounds.top);
        CHECK(row.restoreBounds.right <= row.bounds.right);
        CHECK(row.restoreBounds.bottom <= row.bounds.bottom);
        CHECK(row.restoreBounds.right <= row.nameBounds.left);
        CHECK(row.nameBounds.right <= row.sizeBounds.left);
    }
}

TEST_CASE(review_collector_restore_hit_test_uses_half_open_edges_and_item_index) {
    const AnalyzerRectF actionBar{0.0F, 244.0F, 400.0F, 300.0F};
    const auto layout = compute_review_collector_layout(
        actionBar, 400.0F, 300.0F, 10U, 3U);
    CHECK(!layout.rows.empty());
    const auto& target = layout.rows[0];

    CHECK(hit_test_review_restore(
              layout,
              target.restoreBounds.left,
              target.restoreBounds.top)
        == std::optional<std::size_t>{3U});
    CHECK(hit_test_review_restore(
              layout,
              target.restoreBounds.right - 0.01F,
              target.restoreBounds.bottom - 0.01F)
        == std::optional<std::size_t>{3U});
    CHECK(!hit_test_review_restore(
               layout,
               target.restoreBounds.right,
               target.restoreBounds.top)
               .has_value());
    CHECK(!hit_test_review_restore(
               layout,
               target.restoreBounds.left,
               target.restoreBounds.bottom)
               .has_value());
}

TEST_CASE(review_collector_contains_point_uses_half_open_boundaries) {
    const AnalyzerRectF bounds{10.0F, 20.0F, 30.0F, 40.0F};
    CHECK(contains_point(bounds, 20.0F, 30.0F));
    CHECK(contains_point(bounds, bounds.left, 30.0F));
    CHECK(contains_point(bounds, 20.0F, bounds.top));
    CHECK(!contains_point(bounds, bounds.right, 30.0F));
    CHECK(!contains_point(bounds, 20.0F, bounds.bottom));
}

TEST_CASE(review_drag_activates_after_six_dip_and_drops_only_on_collector) {
    ReviewDragState drag;
    CHECK(drag.begin(7U, 100.0F, 100.0F));
    CHECK(!drag.move(105.0F, 100.0F, false));
    CHECK(!drag.active());
    CHECK(drag.move(106.0F, 100.0F, true));
    CHECK(drag.active());
    CHECK(drag.valid_drop());
    CHECK(drag.release(true) == std::optional<core::NodeIndex>{7U});
    CHECK(!drag.pending());
    CHECK(!drag.active());
    CHECK(!drag.valid_drop());
    CHECK(drag.node() == core::invalid_node);
}

TEST_CASE(review_drag_invalid_release_emits_no_node_and_resets_state) {
    ReviewDragState drag;
    CHECK(!drag.begin(core::invalid_node, 0.0F, 0.0F));
    CHECK(drag.begin(9U, 0.0F, 0.0F));
    (void)drag.move(10.0F, 0.0F, false);
    CHECK(!drag.release(false).has_value());
    CHECK(!drag.pending());
    CHECK(!drag.active());
    CHECK(!drag.valid_drop());
    CHECK(drag.node() == core::invalid_node);
}

TEST_CASE(review_drag_cancel_resets_active_state_without_emitting_a_node) {
    ReviewDragState drag;
    CHECK(drag.begin(11U, 0.0F, 0.0F));
    CHECK(drag.move(6.0F, 0.0F, true));
    CHECK(drag.active());
    CHECK(drag.valid_drop());
    CHECK(drag.cancel());
    CHECK(!drag.pending());
    CHECK(!drag.active());
    CHECK(!drag.valid_drop());
    CHECK(drag.node() == core::invalid_node);
}

TEST_CASE(review_drag_visual_is_hidden_without_an_active_drag) {
    const auto visual = compute_review_drag_visual(
        false, false, 100.0F, 120.0F, 800.0F, 600.0F);
    CHECK(!visual.previewVisible);
    CHECK(visual.collectorToken == ReviewCollectorVisualToken::None);
}

TEST_CASE(review_drag_visual_selects_typed_collector_tokens) {
    const auto ordinary = compute_review_drag_visual(
        true, false, 100.0F, 120.0F, 800.0F, 600.0F);
    CHECK(ordinary.previewVisible);
    CHECK(ordinary.collectorToken == ReviewCollectorVisualToken::Hover);

    const auto validDrop = compute_review_drag_visual(
        true, true, 100.0F, 120.0F, 800.0F, 600.0F);
    CHECK(validDrop.previewVisible);
    CHECK(validDrop.collectorToken == ReviewCollectorVisualToken::Accent);
}

TEST_CASE(review_drag_visual_clamps_preview_and_text_bounds_to_client) {
    const auto visual = compute_review_drag_visual(
        true, true, 790.0F, 590.0F, 800.0F, 600.0F);
    CHECK(visual.previewBounds.left == 540.0F);
    CHECK(visual.previewBounds.top == 536.0F);
    CHECK(visual.previewBounds.right == 800.0F);
    CHECK(visual.previewBounds.bottom == 600.0F);
    CHECK(visual.nameBounds.left == 552.0F);
    CHECK(visual.nameBounds.top == 539.0F);
    CHECK(visual.nameBounds.right == 788.0F);
    CHECK(visual.nameBounds.bottom == 570.0F);
    CHECK(visual.sizeBounds.left == 552.0F);
    CHECK(visual.sizeBounds.top == 567.0F);
    CHECK(visual.sizeBounds.right == 788.0F);
    CHECK(visual.sizeBounds.bottom == 597.0F);

    const auto topLeft = compute_review_drag_visual(
        true, false, -40.0F, -40.0F, 800.0F, 600.0F);
    CHECK(topLeft.previewBounds.left == 0.0F);
    CHECK(topLeft.previewBounds.top == 0.0F);
}

TEST_CASE(review_drag_visual_keeps_text_bounds_valid_in_a_small_client) {
    const auto visual = compute_review_drag_visual(
        true, false, 18.0F, 8.0F, 20.0F, 10.0F);
    CHECK(visual.previewBounds.left == 0.0F);
    CHECK(visual.previewBounds.top == 0.0F);
    CHECK(visual.previewBounds.right == 20.0F);
    CHECK(visual.previewBounds.bottom == 10.0F);
    CHECK(visual.nameBounds.left <= visual.nameBounds.right);
    CHECK(visual.nameBounds.top <= visual.nameBounds.bottom);
    CHECK(visual.nameBounds.left >= visual.previewBounds.left);
    CHECK(visual.nameBounds.right <= visual.previewBounds.right);
    CHECK(visual.nameBounds.top >= visual.previewBounds.top);
    CHECK(visual.nameBounds.bottom <= visual.previewBounds.bottom);
    CHECK(visual.sizeBounds.left <= visual.sizeBounds.right);
    CHECK(visual.sizeBounds.top <= visual.sizeBounds.bottom);
    CHECK(visual.sizeBounds.left >= visual.previewBounds.left);
    CHECK(visual.sizeBounds.right <= visual.previewBounds.right);
    CHECK(visual.sizeBounds.top >= visual.previewBounds.top);
    CHECK(visual.sizeBounds.bottom <= visual.previewBounds.bottom);
}

TEST_CASE(review_scroll_target_routes_open_panel_coordinates_and_falls_back) {
    const AnalyzerRectF panel{20.0F, 100.0F, 620.0F, 540.0F};
    CHECK(review_scroll_target_at(true, panel, 100.0F, 200.0F)
        == ReviewScrollTarget::Review);
    CHECK(review_scroll_target_at(true, panel, 700.0F, 200.0F)
        == ReviewScrollTarget::Children);
    CHECK(review_scroll_target_at(false, panel, 100.0F, 200.0F)
        == ReviewScrollTarget::Children);
}
