#pragma once

#include "core/sunburst_layout.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace diskbloom::core {

constexpr std::size_t max_sunburst_transition_segments = 2'048U;

struct PolarSegmentState {
    NodeIndex node = invalid_node;
    float startAngle = 0.0F;
    float endAngle = 0.0F;
    float innerRadius = 0.0F;
    float outerRadius = 0.0F;
    float opacity = 0.0F;
    std::uint16_t depth = 0U;
    std::uint8_t paletteIndex = 0U;
    bool aggregate = false;
};

struct SegmentMorph {
    std::optional<PolarSegmentState> source;
    std::optional<PolarSegmentState> destination;
};

struct SunburstTransitionPlan {
    std::vector<SegmentMorph> morphs;
    float anchorAngle = 0.0F;
    float anchorRadius = 0.0F;
};

struct TransitionDrawSegment {
    NodeIndex node = invalid_node;
    float startAngle = 0.0F;
    float endAngle = 0.0F;
    float innerRadius = 0.0F;
    float outerRadius = 0.0F;
    float sourceOpacity = 0.0F;
    float destinationOpacity = 0.0F;
    std::uint16_t depth = 0U;
    std::uint8_t sourcePaletteIndex = 0U;
    std::uint8_t destinationPaletteIndex = 0U;
    bool aggregate = false;
};

class SunburstTransitionFrame final {
public:
    void reserve(std::size_t count);
    [[nodiscard]] std::span<const TransitionDrawSegment> segments() const noexcept;

private:
    friend void interpolate_sunburst_transition(
        const SunburstTransitionPlan&,
        float,
        SunburstTransitionFrame&);

    std::vector<TransitionDrawSegment> segments_;
};

struct SunburstTransitionSnapshot {
    std::vector<PolarSegmentState> segments;
};

[[nodiscard]] float sunburst_transition_easing(float progress) noexcept;

[[nodiscard]] SunburstTransitionPlan build_sunburst_transition(
    const SunburstLayout& source,
    const SunburstGeometry& sourceGeometry,
    const SunburstLayout& destination,
    const SunburstGeometry& destinationGeometry,
    NodeIndex anchorNode);

[[nodiscard]] SunburstTransitionPlan build_sunburst_transition(
    const SunburstTransitionSnapshot& source,
    const SunburstLayout& destination,
    const SunburstGeometry& destinationGeometry,
    NodeIndex anchorNode);

void interpolate_sunburst_transition(
    const SunburstTransitionPlan& plan,
    float progress,
    SunburstTransitionFrame& frame);

[[nodiscard]] SunburstTransitionSnapshot snapshot_transition_frame(
    const SunburstTransitionFrame& frame);

} // namespace diskbloom::core
