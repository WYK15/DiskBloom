#include "core/sunburst_transition.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace diskbloom::core {
namespace {

[[nodiscard]] float lerp(const float source, const float destination, const float progress) noexcept {
    return source + (destination - source) * progress;
}

[[nodiscard]] PolarSegmentState to_polar_state(
    const SunburstSegment& segment,
    const SunburstGeometry& geometry) noexcept {
    return {
        .node = segment.node,
        .startAngle = segment.startAngle,
        .endAngle = segment.endAngle,
        .innerRadius = geometry.innerRadius
            + geometry.ringWidth * static_cast<float>(segment.depth),
        .outerRadius = geometry.innerRadius
            + geometry.ringWidth * static_cast<float>(segment.depth + 1U),
        .opacity = 1.0F,
        .depth = segment.depth,
        .paletteIndex = segment.paletteIndex,
        .aggregate = has_flag(segment.flags, SunburstSegmentFlags::Aggregate),
    };
}

[[nodiscard]] std::vector<PolarSegmentState> polar_states(
    const SunburstLayout& layout,
    const SunburstGeometry& geometry) {
    const auto count = std::min(layout.segments.size(), max_sunburst_transition_segments);
    std::vector<PolarSegmentState> states;
    states.reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        states.push_back(to_polar_state(layout.segments[index], geometry));
    }
    return states;
}

[[nodiscard]] std::uint64_t identity(const PolarSegmentState& state) noexcept {
    auto key = static_cast<std::uint64_t>(state.node);
    if (state.aggregate) {
        key |= 1ULL << 63U;
        key |= static_cast<std::uint64_t>(state.depth) << 32U;
    }
    return key;
}

[[nodiscard]] std::pair<float, float> find_anchor(
    const std::span<const PolarSegmentState> source,
    const std::span<const PolarSegmentState> destination,
    const NodeIndex anchorNode) noexcept {
    const auto find = [anchorNode](const std::span<const PolarSegmentState> states)
        -> const PolarSegmentState* {
        const auto result = std::ranges::find_if(states, [anchorNode](const auto& state) {
            return state.node == anchorNode && !state.aggregate;
        });
        return result == states.end() ? nullptr : &*result;
    };
    const auto* anchor = find(source);
    if (anchor == nullptr) {
        anchor = find(destination);
    }
    if (anchor == nullptr) {
        if (!source.empty()) {
            anchor = &source.front();
        } else if (!destination.empty()) {
            anchor = &destination.front();
        }
    }
    return anchor == nullptr
        ? std::pair{0.0F, 0.0F}
        : std::pair{
            (anchor->startAngle + anchor->endAngle) * 0.5F,
            (anchor->innerRadius + anchor->outerRadius) * 0.5F,
        };
}

[[nodiscard]] SunburstTransitionPlan build_plan(
    const std::span<const PolarSegmentState> source,
    const std::span<const PolarSegmentState> destination,
    const NodeIndex anchorNode) {
    SunburstTransitionPlan plan;
    const auto [anchorAngle, anchorRadius] = find_anchor(source, destination, anchorNode);
    plan.anchorAngle = anchorAngle;
    plan.anchorRadius = anchorRadius;
    plan.morphs.reserve(std::max(source.size(), destination.size()));

    std::unordered_map<std::uint64_t, std::size_t> destinationByIdentity;
    destinationByIdentity.reserve(destination.size());
    for (std::size_t index = 0U; index < destination.size(); ++index) {
        destinationByIdentity.try_emplace(identity(destination[index]), index);
    }
    std::vector<bool> sourceMatched(source.size(), false);
    std::vector<bool> destinationMatched(destination.size(), false);
    for (std::size_t sourceIndex = 0U; sourceIndex < source.size(); ++sourceIndex) {
        const auto found = destinationByIdentity.find(identity(source[sourceIndex]));
        if (found == destinationByIdentity.end() || destinationMatched[found->second]) {
            continue;
        }
        sourceMatched[sourceIndex] = true;
        destinationMatched[found->second] = true;
        plan.morphs.push_back({source[sourceIndex], destination[found->second]});
    }

    std::vector<std::size_t> remainingSource;
    std::vector<std::size_t> remainingDestination;
    remainingSource.reserve(source.size());
    remainingDestination.reserve(destination.size());
    for (std::size_t index = 0U; index < source.size(); ++index) {
        if (!sourceMatched[index]) {
            remainingSource.push_back(index);
        }
    }
    for (std::size_t index = 0U; index < destination.size(); ++index) {
        if (!destinationMatched[index]) {
            remainingDestination.push_back(index);
        }
    }
    const auto remainingCount = std::max(remainingSource.size(), remainingDestination.size());
    for (std::size_t index = 0U; index < remainingCount; ++index) {
        SegmentMorph morph;
        if (index < remainingSource.size()) {
            morph.source = source[remainingSource[index]];
        }
        if (index < remainingDestination.size()) {
            morph.destination = destination[remainingDestination[index]];
        }
        plan.morphs.push_back(std::move(morph));
    }
    return plan;
}

[[nodiscard]] PolarSegmentState collapsed_state(
    const PolarSegmentState* peer,
    const float angle,
    const float radius) noexcept {
    return {
        .node = peer == nullptr ? invalid_node : peer->node,
        .startAngle = angle,
        .endAngle = angle,
        .innerRadius = radius,
        .outerRadius = radius,
        .opacity = 0.0F,
        .depth = peer == nullptr ? 0U : peer->depth,
        .paletteIndex = peer == nullptr ? 0U : peer->paletteIndex,
        .aggregate = peer != nullptr && peer->aggregate,
    };
}

} // namespace

void SunburstTransitionFrame::reserve(const std::size_t count) {
    segments_.reserve(std::min(count, max_sunburst_transition_segments));
}

std::span<const TransitionDrawSegment> SunburstTransitionFrame::segments() const noexcept {
    return segments_;
}

float sunburst_transition_easing(const float progress) noexcept {
    const auto clamped = std::clamp(progress, 0.0F, 1.0F);
    const auto inverse = 1.0F - clamped;
    return 1.0F - inverse * inverse * inverse;
}

SunburstTransitionPlan build_sunburst_transition(
    const SunburstLayout& source,
    const SunburstGeometry& sourceGeometry,
    const SunburstLayout& destination,
    const SunburstGeometry& destinationGeometry,
    const NodeIndex anchorNode) {
    const auto sourceStates = polar_states(source, sourceGeometry);
    const auto destinationStates = polar_states(destination, destinationGeometry);
    return build_plan(sourceStates, destinationStates, anchorNode);
}

SunburstTransitionPlan build_sunburst_transition(
    const SunburstTransitionSnapshot& source,
    const SunburstLayout& destination,
    const SunburstGeometry& destinationGeometry,
    const NodeIndex anchorNode) {
    const auto destinationStates = polar_states(destination, destinationGeometry);
    const auto sourceCount = std::min(source.segments.size(), max_sunburst_transition_segments);
    return build_plan(
        std::span(source.segments.data(), sourceCount),
        destinationStates,
        anchorNode);
}

void interpolate_sunburst_transition(
    const SunburstTransitionPlan& plan,
    const float progress,
    SunburstTransitionFrame& frame) {
    const auto eased = sunburst_transition_easing(progress);
    frame.reserve(plan.morphs.size());
    frame.segments_.clear();
    for (const auto& morph : plan.morphs) {
        const auto sourceFallback = collapsed_state(
            morph.destination ? &*morph.destination : nullptr,
            plan.anchorAngle,
            plan.anchorRadius);
        const auto destinationFallback = collapsed_state(
            morph.source ? &*morph.source : nullptr,
            plan.anchorAngle,
            plan.anchorRadius);
        const auto& source = morph.source ? *morph.source : sourceFallback;
        const auto& destination = morph.destination ? *morph.destination : destinationFallback;
        const auto useDestinationIdentity = eased >= 0.5F && morph.destination.has_value();
        const auto& identityState = useDestinationIdentity ? destination : source;
        frame.segments_.push_back({
            .node = identityState.node,
            .startAngle = lerp(source.startAngle, destination.startAngle, eased),
            .endAngle = lerp(source.endAngle, destination.endAngle, eased),
            .innerRadius = lerp(source.innerRadius, destination.innerRadius, eased),
            .outerRadius = lerp(source.outerRadius, destination.outerRadius, eased),
            .sourceOpacity = source.opacity * (1.0F - eased),
            .destinationOpacity = destination.opacity * eased,
            .depth = identityState.depth,
            .sourcePaletteIndex = source.paletteIndex,
            .destinationPaletteIndex = destination.paletteIndex,
            .aggregate = identityState.aggregate,
        });
    }
}

SunburstTransitionSnapshot snapshot_transition_frame(const SunburstTransitionFrame& frame) {
    SunburstTransitionSnapshot snapshot;
    snapshot.segments.reserve(frame.segments().size());
    for (const auto& segment : frame.segments()) {
        const auto sourceDominates = segment.sourceOpacity >= segment.destinationOpacity;
        snapshot.segments.push_back({
            .node = segment.node,
            .startAngle = segment.startAngle,
            .endAngle = segment.endAngle,
            .innerRadius = segment.innerRadius,
            .outerRadius = segment.outerRadius,
            .opacity = std::clamp(
                segment.sourceOpacity + segment.destinationOpacity,
                0.0F,
                1.0F),
            .depth = segment.depth,
            .paletteIndex = sourceDominates
                ? segment.sourcePaletteIndex
                : segment.destinationPaletteIndex,
            .aggregate = segment.aggregate,
        });
    }
    return snapshot;
}

} // namespace diskbloom::core
