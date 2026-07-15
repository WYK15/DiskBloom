#include "test_support.h"

#include "core/sunburst_layout.h"

#include <algorithm>
#include <cmath>

using diskbloom::core::ScanNodeFlags;
using diskbloom::core::ScanTree;
using diskbloom::core::SunburstGeometry;
using diskbloom::core::SunburstLayoutOptions;
using diskbloom::core::SunburstSegmentFlags;
using diskbloom::core::build_sunburst_layout;
using diskbloom::core::has_flag;
using diskbloom::core::hit_test_sunburst;

namespace {

constexpr float pi = 3.14159265358979323846F;

bool near(const float left, const float right, const float tolerance = 0.0001F) {
    return std::abs(left - right) <= tolerance;
}

} // namespace

TEST_CASE(sunburst_layout_sweep_matches_child_byte_fraction) {
    ScanTree tree;
    const auto root = tree.add_root(L"root", ScanNodeFlags::Directory);
    const auto large = tree.add_child(root, L"large", 60U, ScanNodeFlags::None);
    const auto small = tree.add_child(root, L"small", 40U, ScanNodeFlags::None);
    tree.aggregate();

    const auto layout = build_sunburst_layout(tree, root, {});
    CHECK(layout.segments.size() == 2U);

    const auto large_segment = std::find_if(
        layout.segments.begin(),
        layout.segments.end(),
        [large](const auto& segment) { return segment.node == large; });
    const auto small_segment = std::find_if(
        layout.segments.begin(),
        layout.segments.end(),
        [small](const auto& segment) { return segment.node == small; });
    CHECK(large_segment != layout.segments.end());
    CHECK(small_segment != layout.segments.end());
    CHECK(near(large_segment->endAngle - large_segment->startAngle, 2.0F * pi * 0.6F));
    CHECK(near(small_segment->endAngle - small_segment->startAngle, 2.0F * pi * 0.4F));
}

TEST_CASE(sunburst_layout_expands_directories_into_next_band) {
    ScanTree tree;
    const auto root = tree.add_root(L"root", ScanNodeFlags::Directory);
    const auto folder = tree.add_child(root, L"folder", 0U, ScanNodeFlags::Directory);
    const auto file = tree.add_child(folder, L"file", 30U, ScanNodeFlags::None);
    tree.aggregate();

    const auto layout = build_sunburst_layout(tree, root, {});
    CHECK(layout.segments.size() == 2U);
    CHECK(layout.segments[0].node == folder);
    CHECK(layout.segments[0].depth == 0U);
    CHECK(layout.segments[1].node == file);
    CHECK(layout.segments[1].depth == 1U);
    CHECK(layout.segments[1].paletteIndex == layout.segments[0].paletteIndex);
    CHECK(near(layout.segments[1].startAngle, layout.segments[0].startAngle));
    CHECK(near(layout.segments[1].endAngle, layout.segments[0].endAngle));
}

TEST_CASE(sunburst_layout_groups_below_threshold_children) {
    ScanTree tree;
    const auto root = tree.add_root(L"root", ScanNodeFlags::Directory);
    (void)tree.add_child(root, L"large", 999U, ScanNodeFlags::None);
    (void)tree.add_child(root, L"tiny", 1U, ScanNodeFlags::None);
    tree.aggregate();

    SunburstLayoutOptions options{};
    options.minSweepRadians = 0.1F;
    const auto layout = build_sunburst_layout(tree, root, options);
    CHECK(layout.segments.size() == 2U);
    const auto aggregate = std::find_if(
        layout.segments.begin(),
        layout.segments.end(),
        [](const auto& segment) {
            return has_flag(segment.flags, SunburstSegmentFlags::Aggregate);
        });
    CHECK(aggregate != layout.segments.end());
    CHECK(aggregate->node == root);
    CHECK(aggregate->logicalSize == 1U);
    CHECK(near(aggregate->endAngle - aggregate->startAngle, 2.0F * pi * 0.001F));
}

TEST_CASE(sunburst_layout_never_exceeds_segment_budget) {
    ScanTree tree;
    const auto root = tree.add_root(L"root", ScanNodeFlags::Directory);
    for (std::size_t index = 0U; index < 100U; ++index) {
        (void)tree.add_child(root, L"file", 1U, ScanNodeFlags::None);
    }
    tree.aggregate();

    SunburstLayoutOptions options{};
    options.maxSegments = 32U;
    options.minSweepRadians = 0.0F;
    const auto layout = build_sunburst_layout(tree, root, options);
    CHECK(layout.segments.size() == 32U);
    CHECK(has_flag(layout.segments.back().flags, SunburstSegmentFlags::Aggregate));
}

TEST_CASE(sunburst_layout_rejects_invalid_roots) {
    ScanTree tree;
    (void)tree.add_root(L"root", ScanNodeFlags::Directory);
    const auto layout = build_sunburst_layout(tree, 99U, {});
    CHECK(layout.segments.empty());
}

TEST_CASE(sunburst_hit_test_uses_radial_band_and_angle) {
    ScanTree tree;
    const auto root = tree.add_root(L"root", ScanNodeFlags::Directory);
    (void)tree.add_child(root, L"first", 1U, ScanNodeFlags::None);
    (void)tree.add_child(root, L"second", 1U, ScanNodeFlags::None);
    tree.aggregate();
    const auto layout = build_sunburst_layout(tree, root, {});
    CHECK(layout.segments.size() == 2U);

    const SunburstGeometry geometry{
        .centerX = 100.0F,
        .centerY = 100.0F,
        .innerRadius = 20.0F,
        .ringWidth = 30.0F,
    };
    const auto& target = layout.segments.front();
    const auto angle = (target.startAngle + target.endAngle) * 0.5F;
    constexpr float radius = 35.0F;
    const auto hit = hit_test_sunburst(
        layout,
        geometry,
        geometry.centerX + std::cos(angle) * radius,
        geometry.centerY + std::sin(angle) * radius);
    CHECK(hit.has_value());
    CHECK(hit->segmentIndex == 0U);
    CHECK(hit->node == target.node);

    CHECK(!hit_test_sunburst(layout, geometry, 100.0F, 100.0F).has_value());
    CHECK(!hit_test_sunburst(layout, geometry, 100.0F, 151.0F).has_value());
}
