#include "test_support.h"

#include "app/disk_overview.h"

#include <array>
#include <cstddef>

using diskbloom::app::RectF;
using diskbloom::app::OverviewCommandKind;
using diskbloom::app::compute_overview_layout;
using diskbloom::app::hit_test_overview;

namespace {

bool overlaps(const RectF& first, const RectF& second) {
    return first.left < second.right && first.right > second.left
        && first.top < second.bottom && first.bottom > second.top;
}

} // namespace

TEST_CASE(overview_layout_has_stable_title_and_rows) {
    const auto layout = compute_overview_layout(1600.0F, 900.0F, 5U);
    CHECK(layout.titleBar.bottom - layout.titleBar.top == 76.0F);
    CHECK(layout.rows.size() == 5U);
    CHECK(layout.rows[1].bounds.top - layout.rows[0].bounds.top == 100.0F);
    CHECK(layout.bottomBar.bottom == 900.0F);
}

TEST_CASE(overview_layout_keeps_commands_inside_rows_at_supported_dpi_widths) {
    constexpr std::array widths{1600.0F, 1066.0F, 800.0F};
    for (const auto width : widths) {
        const auto layout = compute_overview_layout(width, 720.0F, 4U);
        for (const auto& row : layout.rows) {
            CHECK(row.scanButton.left >= row.bounds.left);
            CHECK(row.scanButton.right <= row.bounds.right);
            CHECK(row.progressTrack.right <= row.scanButton.left);
            CHECK(!overlaps(row.textBounds, row.progressTrack));
            CHECK(!overlaps(row.progressTrack, row.scanButton));
        }
    }
}

TEST_CASE(overview_layout_bottom_commands_do_not_overlap) {
    const auto layout = compute_overview_layout(800.0F, 720.0F, 4U);
    CHECK(!overlaps(layout.scanFolderButton, layout.settingsButton));
    CHECK(layout.scanFolderButton.bottom <= layout.bottomBar.bottom);
    CHECK(layout.settingsButton.bottom <= layout.bottomBar.bottom);
}

TEST_CASE(overview_hit_test_maps_primary_commands) {
    const auto layout = compute_overview_layout(1000.0F, 720.0F, 2U);

    const auto scan = hit_test_overview(
        layout,
        layout.rows[1].scanButton.left + 2.0F,
        layout.rows[1].scanButton.top + 2.0F);
    CHECK(scan.has_value());
    CHECK(scan->kind == OverviewCommandKind::ScanVolume);
    CHECK(scan->volumeIndex == 1U);

    const auto folder = hit_test_overview(
        layout,
        layout.scanFolderButton.left + 2.0F,
        layout.scanFolderButton.top + 2.0F);
    CHECK(folder.has_value());
    CHECK(folder->kind == OverviewCommandKind::ScanFolder);

    const auto settings = hit_test_overview(
        layout,
        layout.settingsButton.left + 2.0F,
        layout.settingsButton.top + 2.0F);
    CHECK(settings.has_value());
    CHECK(settings->kind == OverviewCommandKind::OpenSettings);
}

TEST_CASE(overview_hit_test_ignores_empty_space) {
    const auto layout = compute_overview_layout(1000.0F, 720.0F, 2U);
    CHECK(!hit_test_overview(layout, 400.0F, 400.0F).has_value());
}
