#pragma once

#include "core/scan_tree.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace diskbloom::core {

enum class SunburstSegmentFlags : std::uint8_t {
    None = 0U,
    Aggregate = 1U << 0U,
};

[[nodiscard]] constexpr bool has_flag(
    const SunburstSegmentFlags value,
    const SunburstSegmentFlags flag) noexcept {
    return (static_cast<std::uint8_t>(value) & static_cast<std::uint8_t>(flag)) != 0U;
}

struct SunburstSegment {
    NodeIndex node = invalid_node;
    float startAngle = 0.0F;
    float endAngle = 0.0F;
    std::uint64_t logicalSize = 0U;
    std::uint16_t depth = 0U;
    std::uint8_t paletteIndex = 0U;
    SunburstSegmentFlags flags = SunburstSegmentFlags::None;
};

struct SunburstDepthRange {
    std::size_t begin = 0U;
    std::size_t end = 0U;
};

struct SunburstLayout {
    NodeIndex root = invalid_node;
    float startAngle = -1.57079632679489661923F;
    std::vector<SunburstSegment> segments;
    std::vector<SunburstDepthRange> depthRanges;
};

struct SunburstLayoutOptions {
    std::size_t maxSegments = 2'048U;
    std::uint16_t maxDepth = 6U;
    float minSweepRadians = 0.004F;
};

struct SunburstGeometry {
    float centerX = 0.0F;
    float centerY = 0.0F;
    float innerRadius = 0.0F;
    float ringWidth = 0.0F;
};

struct SunburstHit {
    std::size_t segmentIndex = 0U;
    NodeIndex node = invalid_node;
    bool aggregate = false;
};

[[nodiscard]] SunburstLayout build_sunburst_layout(
    const ScanTree& tree,
    NodeIndex root,
    const SunburstLayoutOptions& options);

[[nodiscard]] std::optional<SunburstHit> hit_test_sunburst(
    const SunburstLayout& layout,
    const SunburstGeometry& geometry,
    float x,
    float y) noexcept;

} // namespace diskbloom::core
