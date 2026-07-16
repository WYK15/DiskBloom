#include "test_support.h"

#include "app/analyzer_hover_pulse.h"

#include <chrono>
#include <cmath>

using diskbloom::app::AnalyzerHoverPulse;
using diskbloom::app::find_hover_branch;
using diskbloom::app::segment_is_in_hover_branch;
using diskbloom::core::SunburstLayout;
using diskbloom::core::SunburstSegment;
using diskbloom::core::SunburstSegmentFlags;
using diskbloom::core::invalid_node;

namespace {

SunburstLayout make_layout() {
    SunburstLayout layout;
    layout.segments = {
        {.node = 1U, .startAngle = 0.0F, .endAngle = 2.0F, .depth = 0U},
        {.node = 2U, .startAngle = 2.0F, .endAngle = 4.0F, .depth = 0U},
        {.node = 3U, .startAngle = 0.0F, .endAngle = 1.0F, .depth = 1U},
        {.node = 4U, .startAngle = 1.0F, .endAngle = 2.0F, .depth = 1U},
        {.node = 5U, .startAngle = 2.0F, .endAngle = 4.0F, .depth = 1U},
        {.node = 6U,
         .startAngle = 4.0F,
         .endAngle = 5.0F,
         .depth = 0U,
         .flags = SunburstSegmentFlags::Aggregate},
    };
    return layout;
}

bool near(const float left, const float right) {
    return std::abs(left - right) < 0.0001F;
}

} // namespace

TEST_CASE(analyzer_hover_branch_contains_only_target_root_branch) {
    const auto layout = make_layout();
    const auto branch = find_hover_branch(layout, 1U);
    CHECK(branch.has_value());
    if (!branch.has_value()) {
        return;
    }
    CHECK(segment_is_in_hover_branch(layout.segments[0], *branch));
    CHECK(segment_is_in_hover_branch(layout.segments[2], *branch));
    CHECK(segment_is_in_hover_branch(layout.segments[3], *branch));
    CHECK(!segment_is_in_hover_branch(layout.segments[1], *branch));
    CHECK(!segment_is_in_hover_branch(layout.segments[4], *branch));
}

TEST_CASE(analyzer_hover_branch_rejects_invalid_nested_and_aggregate_nodes) {
    const auto layout = make_layout();
    CHECK(!find_hover_branch(layout, invalid_node).has_value());
    CHECK(!find_hover_branch(layout, 3U).has_value());
    CHECK(!find_hover_branch(layout, 6U).has_value());
    CHECK(!find_hover_branch(layout, 99U).has_value());
}

TEST_CASE(analyzer_hover_branch_uses_half_open_angular_boundaries) {
    const auto layout = make_layout();
    const auto branch = find_hover_branch(layout, 1U);
    CHECK(branch.has_value());
    if (!branch.has_value()) {
        return;
    }
    const SunburstSegment touchingEnd{
        .node = 8U, .startAngle = 2.0F, .endAngle = 2.5F, .depth = 1U};
    const SunburstSegment crossingEnd{
        .node = 9U, .startAngle = 1.5F, .endAngle = 2.5F, .depth = 1U};
    CHECK(!segment_is_in_hover_branch(touchingEnd, *branch));
    CHECK(!segment_is_in_hover_branch(crossingEnd, *branch));
}

TEST_CASE(analyzer_hover_pulse_is_periodic_bounded_and_static_when_disabled) {
    using namespace std::chrono_literals;
    const auto start = std::chrono::steady_clock::time_point{} + 10s;
    AnalyzerHoverPulse pulse;
    pulse.set_target(1U, start);

    CHECK(pulse.has_target());
    CHECK(pulse.target() == 1U);
    CHECK(pulse.timer_required(true));
    CHECK(!pulse.timer_required(false));
    CHECK(near(pulse.opacity(start, true), pulse.opacity(start + 900ms, true)));
    CHECK(pulse.opacity(start + 225ms, true) > pulse.opacity(start, true));
    CHECK(near(pulse.opacity(start, false), pulse.opacity(start + 225ms, false)));
    for (const auto offset : {0ms, 100ms, 225ms, 450ms, 675ms, 900ms}) {
        const auto opacity = pulse.opacity(start + offset, true);
        CHECK(opacity >= AnalyzerHoverPulse::minimum_opacity);
        CHECK(opacity <= AnalyzerHoverPulse::maximum_opacity);
    }
}

TEST_CASE(analyzer_hover_pulse_target_change_restarts_phase_and_clear_stops_timer) {
    using namespace std::chrono_literals;
    const auto start = std::chrono::steady_clock::time_point{};
    AnalyzerHoverPulse pulse;
    pulse.set_target(1U, start);
    const auto advanced = pulse.opacity(start + 225ms, true);
    pulse.set_target(2U, start + 225ms);
    CHECK(pulse.opacity(start + 225ms, true) < advanced);
    pulse.clear();
    CHECK(!pulse.has_target());
    CHECK(pulse.target() == invalid_node);
    CHECK(!pulse.timer_required(true));
}
