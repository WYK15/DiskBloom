#include "test_support.h"

#include "app/analyzer_view.h"

using diskbloom::app::AnalyzerRectF;
using diskbloom::app::compute_analyzer_child_list_layout;
using diskbloom::app::hit_test_analyzer_child_rows;
using diskbloom::app::next_child_scroll_offset;

namespace {

bool overlaps(const AnalyzerRectF& left, const AnalyzerRectF& right) {
    return left.left < right.right && left.right > right.left
        && left.top < right.bottom && left.bottom > right.top;
}

} // namespace

TEST_CASE(analyzer_child_list_rows_stay_inside_details_panel) {
    const AnalyzerRectF details{544.0F, 96.0F, 772.0F, 448.0F};
    const auto layout = compute_analyzer_child_list_layout(details, 24U, 0U);

    CHECK(!layout.rows.empty());
    CHECK(layout.rows.size() < 24U);
    for (std::size_t index = 0U; index < layout.rows.size(); ++index) {
        const auto& row = layout.rows[index];
        CHECK(row.bounds.left >= details.left);
        CHECK(row.bounds.right <= details.right);
        CHECK(row.bounds.top >= details.top);
        CHECK(row.bounds.bottom <= details.bottom);
        CHECK(row.nameBounds.right <= row.sizeBounds.left);
        CHECK(row.itemIndex == index);
        if (index > 0U) {
            CHECK(!overlaps(layout.rows[index - 1U].bounds, row.bounds));
        }
    }
}

TEST_CASE(analyzer_child_list_hit_test_returns_underlying_item_index) {
    const AnalyzerRectF details{544.0F, 96.0F, 772.0F, 448.0F};
    const auto layout = compute_analyzer_child_list_layout(details, 24U, 5U);
    CHECK(!layout.rows.empty());
    const auto& row = layout.rows[1U];
    const auto hit = hit_test_analyzer_child_rows(
        layout,
        row.bounds.left + 4.0F,
        row.bounds.top + 4.0F);
    CHECK(hit.has_value());
    CHECK(*hit == 6U);
    CHECK(!hit_test_analyzer_child_rows(layout, details.left, details.top).has_value());
}

TEST_CASE(analyzer_child_list_scroll_offset_is_bounded) {
    CHECK(next_child_scroll_offset(0U, 24U, 7U, 3) == 3U);
    CHECK(next_child_scroll_offset(3U, 24U, 7U, -5) == 0U);
    CHECK(next_child_scroll_offset(3U, 24U, 7U, 100) == 17U);
    CHECK(next_child_scroll_offset(4U, 4U, 7U, 1) == 0U);
}
