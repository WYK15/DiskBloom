#include "test_support.h"

#include "core/sunburst_transition.h"

#include <algorithm>
#include <cmath>

using diskbloom::core::NodeIndex;
using diskbloom::core::SunburstGeometry;
using diskbloom::core::SunburstLayout;
using diskbloom::core::SunburstSegment;
using diskbloom::core::SunburstSegmentFlags;
using diskbloom::core::SunburstTransitionFrame;
using diskbloom::core::build_sunburst_transition;
using diskbloom::core::interpolate_sunburst_transition;
using diskbloom::core::snapshot_transition_frame;
using diskbloom::core::sunburst_transition_easing;

namespace {

[[nodiscard]] bool near(
    const float left,
    const float right,
    const float tolerance = 0.0001F) {
    return std::abs(left - right) <= tolerance;
}

[[nodiscard]] SunburstLayout make_layout(
    const std::initializer_list<SunburstSegment> segments) {
    SunburstLayout layout;
    layout.root = 0U;
    layout.segments.assign(segments);
    return layout;
}

[[nodiscard]] const diskbloom::core::TransitionDrawSegment* find_visible(
    const SunburstTransitionFrame& frame,
    const NodeIndex node) {
    const auto found = std::ranges::find_if(
        frame.segments(),
        [node](const auto& segment) {
            return segment.node == node
                && segment.sourceOpacity + segment.destinationOpacity > 0.999F;
        });
    return found == frame.segments().end() ? nullptr : &*found;
}

} // namespace

TEST_CASE(sunburst_transition_matches_source_and_destination_endpoints) {
    const auto source = make_layout({
        {.node = 1U, .startAngle = 0.0F, .endAngle = 1.0F, .depth = 0U, .paletteIndex = 1U},
        {.node = 2U, .startAngle = 1.0F, .endAngle = 2.0F, .depth = 1U, .paletteIndex = 2U},
    });
    const auto destination = make_layout({
        {.node = 2U, .startAngle = 0.2F, .endAngle = 1.4F, .depth = 0U, .paletteIndex = 4U},
        {.node = 3U, .startAngle = 1.4F, .endAngle = 2.8F, .depth = 1U, .paletteIndex = 5U},
    });
    constexpr SunburstGeometry sourceGeometry{0.0F, 0.0F, 20.0F, 10.0F};
    constexpr SunburstGeometry destinationGeometry{0.0F, 0.0F, 30.0F, 12.0F};
    const auto plan = build_sunburst_transition(
        source,
        sourceGeometry,
        destination,
        destinationGeometry,
        2U);
    SunburstTransitionFrame frame;

    interpolate_sunburst_transition(plan, 0.0F, frame);
    const auto* sourceOne = find_visible(frame, 1U);
    const auto* sourceTwo = find_visible(frame, 2U);
    CHECK(sourceOne != nullptr && sourceTwo != nullptr);
    CHECK(near(sourceOne->startAngle, 0.0F));
    CHECK(near(sourceOne->innerRadius, 20.0F));
    CHECK(near(sourceTwo->innerRadius, 30.0F));

    interpolate_sunburst_transition(plan, 1.0F, frame);
    const auto* destinationTwo = find_visible(frame, 2U);
    const auto* destinationThree = find_visible(frame, 3U);
    CHECK(destinationTwo != nullptr && destinationThree != nullptr);
    CHECK(near(destinationTwo->startAngle, 0.2F));
    CHECK(near(destinationTwo->innerRadius, 30.0F));
    CHECK(near(destinationThree->innerRadius, 42.0F));
}

TEST_CASE(sunburst_transition_uses_monotonic_cubic_ease_out) {
    CHECK(sunburst_transition_easing(0.0F) == 0.0F);
    CHECK(sunburst_transition_easing(1.0F) == 1.0F);
    CHECK(near(sunburst_transition_easing(0.5F), 0.875F));
    CHECK(sunburst_transition_easing(0.25F) < sunburst_transition_easing(0.5F));
    CHECK(sunburst_transition_easing(0.5F) < sunburst_transition_easing(0.75F));
}

TEST_CASE(sunburst_transition_matches_nodes_and_crossfades_palette_changes) {
    const auto source = make_layout({
        {.node = 7U, .startAngle = 0.0F, .endAngle = 1.0F, .depth = 0U, .paletteIndex = 1U},
    });
    const auto destination = make_layout({
        {.node = 7U, .startAngle = 2.0F, .endAngle = 4.0F, .depth = 1U, .paletteIndex = 9U},
    });
    constexpr SunburstGeometry geometry{0.0F, 0.0F, 10.0F, 10.0F};
    const auto plan = build_sunburst_transition(source, geometry, destination, geometry, 7U);
    SunburstTransitionFrame frame;
    interpolate_sunburst_transition(plan, 0.5F, frame);

    CHECK(frame.segments().size() == 1U);
    const auto& segment = frame.segments().front();
    CHECK(segment.node == 7U);
    CHECK(near(segment.startAngle, 1.75F));
    CHECK(near(segment.sourceOpacity, 0.125F));
    CHECK(near(segment.destinationOpacity, 0.875F));
    CHECK(segment.sourcePaletteIndex == 1U);
    CHECK(segment.destinationPaletteIndex == 9U);
}

TEST_CASE(sunburst_transition_collapses_missing_endpoints_to_anchor) {
    const auto source = make_layout({
        {.node = 4U, .startAngle = 1.0F, .endAngle = 2.0F, .depth = 0U, .paletteIndex = 3U},
        {.node = 5U, .startAngle = 2.0F, .endAngle = 3.0F, .depth = 1U, .paletteIndex = 4U},
    });
    const SunburstLayout destination;
    constexpr SunburstGeometry geometry{0.0F, 0.0F, 20.0F, 10.0F};
    const auto plan = build_sunburst_transition(source, geometry, destination, geometry, 4U);
    SunburstTransitionFrame frame;
    interpolate_sunburst_transition(plan, 1.0F, frame);

    CHECK(frame.segments().size() == 2U);
    for (const auto& segment : frame.segments()) {
        CHECK(near(segment.startAngle, segment.endAngle));
        CHECK(near(segment.innerRadius, segment.outerRadius));
        CHECK(near(segment.sourceOpacity + segment.destinationOpacity, 0.0F));
    }
}

TEST_CASE(sunburst_transition_matches_aggregate_identity_by_node_and_depth) {
    const auto source = make_layout({
        {.node = 4U, .startAngle = 1.0F, .endAngle = 2.0F, .depth = 1U, .paletteIndex = 3U,
         .flags = SunburstSegmentFlags::Aggregate},
    });
    const auto destination = make_layout({
        {.node = 4U, .startAngle = 2.0F, .endAngle = 3.0F, .depth = 1U, .paletteIndex = 6U,
         .flags = SunburstSegmentFlags::Aggregate},
    });
    constexpr SunburstGeometry geometry{0.0F, 0.0F, 20.0F, 10.0F};
    const auto plan = build_sunburst_transition(source, geometry, destination, geometry, 4U);
    CHECK(plan.morphs.size() == 1U);
    CHECK(plan.morphs.front().source.has_value());
    CHECK(plan.morphs.front().destination.has_value());
}

TEST_CASE(sunburst_transition_never_exceeds_2048_morphs) {
    SunburstLayout source;
    SunburstLayout destination;
    for (std::size_t index = 0U; index < 2'100U; ++index) {
        source.segments.push_back({.node = static_cast<NodeIndex>(index), .endAngle = 1.0F});
        destination.segments.push_back({
            .node = static_cast<NodeIndex>(index + 3'000U),
            .endAngle = 1.0F,
        });
    }
    constexpr SunburstGeometry geometry{0.0F, 0.0F, 20.0F, 10.0F};
    const auto plan = build_sunburst_transition(source, geometry, destination, geometry, 0U);
    CHECK(plan.morphs.size() == 2'048U);
}

TEST_CASE(sunburst_transition_snapshot_starts_interrupted_plan_at_same_geometry) {
    const auto source = make_layout({
        {.node = 1U, .startAngle = 0.0F, .endAngle = 1.0F, .depth = 0U, .paletteIndex = 1U},
    });
    const auto destination = make_layout({
        {.node = 1U, .startAngle = 2.0F, .endAngle = 4.0F, .depth = 1U, .paletteIndex = 2U},
    });
    const auto finalDestination = make_layout({
        {.node = 1U, .startAngle = 4.0F, .endAngle = 5.0F, .depth = 0U, .paletteIndex = 3U},
    });
    constexpr SunburstGeometry geometry{0.0F, 0.0F, 10.0F, 10.0F};
    const auto first = build_sunburst_transition(source, geometry, destination, geometry, 1U);
    SunburstTransitionFrame before;
    interpolate_sunburst_transition(first, 0.4F, before);
    const auto snapshot = snapshot_transition_frame(before);
    const auto second = build_sunburst_transition(snapshot, finalDestination, geometry, 1U);
    SunburstTransitionFrame after;
    interpolate_sunburst_transition(second, 0.0F, after);

    CHECK(before.segments().size() == after.segments().size());
    CHECK(near(before.segments().front().startAngle, after.segments().front().startAngle));
    CHECK(near(before.segments().front().endAngle, after.segments().front().endAngle));
    CHECK(near(before.segments().front().innerRadius, after.segments().front().innerRadius));
    CHECK(near(before.segments().front().outerRadius, after.segments().front().outerRadius));
}

