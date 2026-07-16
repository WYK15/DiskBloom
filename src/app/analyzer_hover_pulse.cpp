#include "app/analyzer_hover_pulse.h"

#include <algorithm>
#include <cmath>

namespace diskbloom::app {

std::optional<AnalyzerHoverBranch> find_hover_branch(
    const core::SunburstLayout& layout,
    const core::NodeIndex node) noexcept {
    if (node == core::invalid_node) {
        return std::nullopt;
    }
    for (const auto& segment : layout.segments) {
        if (segment.depth == 0U
            && segment.node == node
            && !core::has_flag(segment.flags, core::SunburstSegmentFlags::Aggregate)) {
            return AnalyzerHoverBranch{
                .startAngle = segment.startAngle,
                .endAngle = segment.endAngle,
                .node = node,
            };
        }
    }
    return std::nullopt;
}

bool segment_is_in_hover_branch(
    const core::SunburstSegment& segment,
    const AnalyzerHoverBranch& branch) noexcept {
    return segment.startAngle >= branch.startAngle
        && segment.startAngle < branch.endAngle
        && segment.endAngle <= branch.endAngle;
}

void AnalyzerHoverPulse::set_target(
    const core::NodeIndex node,
    const std::chrono::steady_clock::time_point now) noexcept {
    if (node == target_) {
        return;
    }
    target_ = node;
    started_ = now;
}

void AnalyzerHoverPulse::clear() noexcept {
    target_ = core::invalid_node;
    started_ = {};
}

bool AnalyzerHoverPulse::has_target() const noexcept {
    return target_ != core::invalid_node;
}

core::NodeIndex AnalyzerHoverPulse::target() const noexcept {
    return target_;
}

bool AnalyzerHoverPulse::timer_required(const bool animationsEnabled) const noexcept {
    return has_target() && animationsEnabled;
}

float AnalyzerHoverPulse::opacity(
    const std::chrono::steady_clock::time_point now,
    const bool animationsEnabled) const noexcept {
    constexpr float midpoint = (minimum_opacity + maximum_opacity) * 0.5F;
    if (!has_target() || !animationsEnabled) {
        return midpoint;
    }
    constexpr double fullCircle = 6.28318530717958647692;
    const auto elapsed = std::max(now - started_, std::chrono::steady_clock::duration::zero());
    const auto periodSeconds = std::chrono::duration<double>(period).count();
    const auto phase = std::fmod(std::chrono::duration<double>(elapsed).count(), periodSeconds)
        / periodSeconds;
    const auto wave = static_cast<float>((1.0 - std::cos(fullCircle * phase)) * 0.5);
    return minimum_opacity + (maximum_opacity - minimum_opacity) * wave;
}

} // namespace diskbloom::app
