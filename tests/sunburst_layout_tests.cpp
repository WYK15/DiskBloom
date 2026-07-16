#include "test_support.h"

#include "core/sunburst_layout.h"
#include "core/scan_tree_exclusion.h"

#include <algorithm>
#include <array>
#include <cmath>

using diskbloom::core::ScanNodeFlags;
using diskbloom::core::ScanTree;
using diskbloom::core::ScanTreeExclusion;
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

TEST_CASE(sunburst_layout_omits_excluded_branches_and_uses_effective_sizes) {
    ScanTree tree;
    const auto root = tree.add_root(L"root", ScanNodeFlags::Directory);
    const auto folder = tree.add_child(root, L"folder", 0U, ScanNodeFlags::Directory);
    const auto kept = tree.add_child(folder, L"kept.bin", 30U, ScanNodeFlags::None);
    const auto excluded = tree.add_child(folder, L"excluded.bin", 70U, ScanNodeFlags::None);
    const auto sibling = tree.add_child(root, L"sibling.bin", 100U, ScanNodeFlags::None);
    tree.aggregate();
    ScanTreeExclusion exclusion;
    const std::array requested{excluded};
    exclusion.rebuild(tree, requested);

    const auto layout = build_sunburst_layout(tree, root, {}, &exclusion);

    const auto folderSegment = std::ranges::find(layout.segments, folder, &diskbloom::core::SunburstSegment::node);
    const auto keptSegment = std::ranges::find(layout.segments, kept, &diskbloom::core::SunburstSegment::node);
    CHECK(folderSegment != layout.segments.end());
    CHECK(folderSegment->logicalSize == 30U);
    CHECK(keptSegment != layout.segments.end());
    CHECK(std::ranges::none_of(layout.segments, [excluded](const auto& segment) {
        return segment.node == excluded;
    }));
    CHECK(near(folderSegment->endAngle - folderSegment->startAngle, 2.0F * pi * 30.0F / 130.0F));
    CHECK(std::ranges::find(layout.segments, sibling, &diskbloom::core::SunburstSegment::node)
        != layout.segments.end());
}

TEST_CASE(sunburst_layout_returns_no_segments_when_all_visible_bytes_are_excluded) {
    ScanTree tree;
    const auto root = tree.add_root(L"root", ScanNodeFlags::Directory);
    const auto folder = tree.add_child(root, L"folder", 0U, ScanNodeFlags::Directory);
    const auto child = tree.add_child(folder, L"child.bin", 10U, ScanNodeFlags::None);
    tree.aggregate();
    ScanTreeExclusion exclusion;
    const std::array requested{folder};
    exclusion.rebuild(tree, requested);

    const auto layout = build_sunburst_layout(tree, root, {}, &exclusion);

    CHECK(layout.segments.empty());
    CHECK(std::ranges::none_of(layout.segments, [child](const auto& segment) {
        return segment.node == child;
    }));
}

TEST_CASE(sunburst_layout_aggregate_uses_projected_child_bytes) {
    ScanTree tree;
    const auto root = tree.add_root(L"root", ScanNodeFlags::Directory);
    const auto large = tree.add_child(root, L"large", 0U, ScanNodeFlags::Directory);
    (void)tree.add_child(large, L"kept.bin", 900U, ScanNodeFlags::None);
    const auto removed = tree.add_child(large, L"removed.bin", 99U, ScanNodeFlags::None);
    (void)tree.add_child(root, L"tiny.bin", 1U, ScanNodeFlags::None);
    tree.aggregate();
    ScanTreeExclusion exclusion;
    const std::array requested{removed};
    exclusion.rebuild(tree, requested);
    SunburstLayoutOptions options{};
    options.maxSegments = 1U;
    options.minSweepRadians = 0.0F;

    const auto layout = build_sunburst_layout(tree, root, options, &exclusion);

    CHECK(layout.segments.size() == 1U);
    CHECK(has_flag(layout.segments[0].flags, SunburstSegmentFlags::Aggregate));
    CHECK(layout.segments[0].logicalSize == 901U);
}

TEST_CASE(sunburst_layout_null_projection_preserves_legacy_output) {
    ScanTree tree;
    const auto root = tree.add_root(L"root", ScanNodeFlags::Directory);
    (void)tree.add_child(root, L"first", 10U, ScanNodeFlags::None);
    (void)tree.add_child(root, L"second", 20U, ScanNodeFlags::None);
    tree.aggregate();

    const auto legacy = build_sunburst_layout(tree, root, {});
    const auto explicitNull = build_sunburst_layout(tree, root, {}, nullptr);

    CHECK(legacy.segments.size() == explicitNull.segments.size());
    for (std::size_t index = 0U; index < legacy.segments.size(); ++index) {
        CHECK(legacy.segments[index].node == explicitNull.segments[index].node);
        CHECK(legacy.segments[index].logicalSize == explicitNull.segments[index].logicalSize);
        CHECK(near(legacy.segments[index].startAngle, explicitNull.segments[index].startAngle));
        CHECK(near(legacy.segments[index].endAngle, explicitNull.segments[index].endAngle));
    }
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
