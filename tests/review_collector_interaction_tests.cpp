#include "test_support.h"

#include "app/review_collector_interaction.h"
#include "core/scan_tree.h"

#include <optional>

using diskbloom::app::AnalyzerRectF;
using diskbloom::app::ReviewDragState;
using diskbloom::app::compute_review_collector_layout;
using diskbloom::app::contains_point;
using diskbloom::app::next_review_scroll_offset;

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
