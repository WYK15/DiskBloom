#include "core/sunburst_layout.h"

#include <algorithm>
#include <cmath>

namespace diskbloom::core {
namespace {

constexpr float full_circle = 6.28318530717958647692F;
constexpr std::uint8_t palette_size = 12U;

struct LayoutWork {
    NodeIndex node = invalid_node;
    float startAngle = 0.0F;
    float endAngle = 0.0F;
    std::uint16_t childDepth = 0U;
};

void build_depth_ranges(SunburstLayout& layout) {
    std::size_t cursor = 0U;
    while (cursor < layout.segments.size()) {
        const auto depth = layout.segments[cursor].depth;
        while (layout.depthRanges.size() <= depth) {
            layout.depthRanges.push_back({cursor, cursor});
        }
        const auto begin = cursor;
        while (cursor < layout.segments.size()
            && layout.segments[cursor].depth == depth) {
            ++cursor;
        }
        layout.depthRanges[depth] = {begin, cursor};
    }
}

} // namespace

SunburstLayout build_sunburst_layout(
    const ScanTree& tree,
    const NodeIndex root,
    const SunburstLayoutOptions& options) {
    SunburstLayout layout;
    layout.root = root;
    if (root >= tree.nodes().size()
        || options.maxSegments == 0U
        || options.maxDepth == 0U) {
        return layout;
    }

    layout.segments.reserve(options.maxSegments);
    std::vector<LayoutWork> work;
    work.reserve(std::min(options.maxSegments, tree.nodes().size()));
    work.push_back({root, layout.startAngle, layout.startAngle + full_circle, 0U});
    const auto minSweep = std::max(options.minSweepRadians, 0.0F);

    for (std::size_t workIndex = 0U;
         workIndex < work.size() && layout.segments.size() < options.maxSegments;
         ++workIndex) {
        const auto current = work[workIndex];
        if (current.childDepth >= options.maxDepth) {
            continue;
        }

        const auto& parent = tree.node(current.node);
        if (parent.logicalSize == 0U || parent.firstChild == invalid_node) {
            continue;
        }

        const auto parentSweep = current.endAngle - current.startAngle;
        std::size_t visibleCount = 0U;
        std::uint64_t belowThresholdBytes = 0U;
        for (auto child = parent.firstChild;
             child != invalid_node;
             child = tree.node(child).nextSibling) {
            const auto childBytes = tree.node(child).logicalSize;
            if (childBytes == 0U) {
                continue;
            }
            const auto sweep = parentSweep
                * static_cast<float>(static_cast<double>(childBytes)
                    / static_cast<double>(parent.logicalSize));
            if (sweep >= minSweep) {
                ++visibleCount;
            } else {
                belowThresholdBytes += childBytes;
            }
        }

        const auto remaining = options.maxSegments - layout.segments.size();
        const auto needsAggregate = belowThresholdBytes > 0U || visibleCount > remaining;
        const auto individualLimit = needsAggregate
            ? (remaining > 0U ? remaining - 1U : 0U)
            : std::min(visibleCount, remaining);

        const auto parentSegmentBegin = layout.segments.size();
        std::size_t individualCount = 0U;
        std::uint64_t aggregateBytes = 0U;
        float angle = current.startAngle;
        std::uint8_t paletteOrdinal = 0U;
        for (auto child = parent.firstChild;
             child != invalid_node;
             child = tree.node(child).nextSibling) {
            const auto& childNode = tree.node(child);
            if (childNode.logicalSize == 0U) {
                continue;
            }
            const auto sweep = parentSweep
                * static_cast<float>(static_cast<double>(childNode.logicalSize)
                    / static_cast<double>(parent.logicalSize));
            if (sweep < minSweep || individualCount >= individualLimit) {
                aggregateBytes += childNode.logicalSize;
                continue;
            }

            const auto endAngle = angle + sweep;
            layout.segments.push_back({
                .node = child,
                .startAngle = angle,
                .endAngle = endAngle,
                .logicalSize = childNode.logicalSize,
                .depth = current.childDepth,
                .paletteIndex = static_cast<std::uint8_t>(
                    (paletteOrdinal + current.childDepth * 3U) % palette_size),
                .flags = SunburstSegmentFlags::None,
            });
            if (has_flag(childNode.flags, ScanNodeFlags::Directory)
                && current.childDepth + 1U < options.maxDepth) {
                work.push_back({
                    child,
                    angle,
                    endAngle,
                    static_cast<std::uint16_t>(current.childDepth + 1U),
                });
            }
            angle = endAngle;
            ++individualCount;
            ++paletteOrdinal;
        }

        if (aggregateBytes > 0U && layout.segments.size() < options.maxSegments) {
            layout.segments.push_back({
                .node = current.node,
                .startAngle = angle,
                .endAngle = current.endAngle,
                .logicalSize = aggregateBytes,
                .depth = current.childDepth,
                .paletteIndex = static_cast<std::uint8_t>(
                    (paletteOrdinal + current.childDepth * 3U) % palette_size),
                .flags = SunburstSegmentFlags::Aggregate,
            });
        } else if (layout.segments.size() > parentSegmentBegin) {
            layout.segments.back().endAngle = current.endAngle;
        }
    }

    build_depth_ranges(layout);
    return layout;
}

std::optional<SunburstHit> hit_test_sunburst(
    const SunburstLayout& layout,
    const SunburstGeometry& geometry,
    const float x,
    const float y) noexcept {
    if (geometry.innerRadius < 0.0F || geometry.ringWidth <= 0.0F) {
        return std::nullopt;
    }

    const auto offsetX = x - geometry.centerX;
    const auto offsetY = y - geometry.centerY;
    const auto radius = std::hypot(offsetX, offsetY);
    const auto relativeRadius = radius - geometry.innerRadius;
    if (relativeRadius < 0.0F) {
        return std::nullopt;
    }

    const auto depth = static_cast<std::size_t>(relativeRadius / geometry.ringWidth);
    if (depth >= layout.depthRanges.size()) {
        return std::nullopt;
    }

    auto angle = std::atan2(offsetY, offsetX);
    while (angle < layout.startAngle) {
        angle += full_circle;
    }
    while (angle >= layout.startAngle + full_circle) {
        angle -= full_circle;
    }

    const auto range = layout.depthRanges[depth];
    for (auto index = range.begin; index < range.end; ++index) {
        const auto& segment = layout.segments[index];
        if (angle >= segment.startAngle && angle < segment.endAngle) {
            return SunburstHit{
                .segmentIndex = index,
                .node = segment.node,
                .aggregate = has_flag(segment.flags, SunburstSegmentFlags::Aggregate),
            };
        }
    }
    return std::nullopt;
}

} // namespace diskbloom::core
