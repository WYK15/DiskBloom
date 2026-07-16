#include "core/sunburst_transition.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <vector>

namespace {

constexpr std::size_t segment_count = 2'048U;
constexpr std::size_t frame_count = 60U;
constexpr float full_circle = 6.28318530717958647692F;

struct PreparedSegment {
    std::array<float, 12U> points{};
};

} // namespace

int main() {
    diskbloom::core::SunburstLayout source;
    diskbloom::core::SunburstLayout destination;
    source.segments.reserve(segment_count);
    destination.segments.reserve(segment_count);
    for (std::size_t index = 0U; index < segment_count; ++index) {
        const auto ordinal = static_cast<float>(index % 512U);
        const auto start = -full_circle * 0.25F + full_circle * ordinal / 512.0F;
        const auto end = start + full_circle / 512.0F;
        source.segments.push_back({
            .node = static_cast<diskbloom::core::NodeIndex>(index + 1U),
            .startAngle = start,
            .endAngle = end,
            .depth = static_cast<std::uint16_t>(index % 4U),
            .paletteIndex = static_cast<std::uint8_t>(index % 12U),
        });
        destination.segments.push_back({
            .node = static_cast<diskbloom::core::NodeIndex>(index + 1U),
            .startAngle = start + 0.35F,
            .endAngle = end + 0.5F,
            .depth = static_cast<std::uint16_t>((index + 1U) % 5U),
            .paletteIndex = static_cast<std::uint8_t>((index + 3U) % 12U),
        });
    }
    constexpr diskbloom::core::SunburstGeometry sourceGeometry{
        400.0F, 360.0F, 50.0F, 42.0F};
    constexpr diskbloom::core::SunburstGeometry destinationGeometry{
        400.0F, 360.0F, 58.0F, 36.0F};
    const auto plan = diskbloom::core::build_sunburst_transition(
        source,
        sourceGeometry,
        destination,
        destinationGeometry,
        1U);
    if (plan.morphs.size() != segment_count) {
        std::cerr << "transition plan validation failed\n";
        return 2;
    }

    diskbloom::core::SunburstTransitionFrame frame;
    frame.reserve(segment_count);
    std::vector<PreparedSegment> prepared;
    prepared.reserve(segment_count);
    std::array<double, frame_count> samples{};
    std::uint64_t checksum = 0U;
    for (std::size_t frameIndex = 0U; frameIndex < frame_count; ++frameIndex) {
        const auto startTime = std::chrono::steady_clock::now();
        const auto progress = static_cast<float>(frameIndex)
            / static_cast<float>(frame_count - 1U);
        diskbloom::core::interpolate_sunburst_transition(plan, progress, frame);
        prepared.clear();
        for (const auto& segment : frame.segments()) {
            const auto middle = (segment.startAngle + segment.endAngle) * 0.5F;
            PreparedSegment geometry;
            const std::array angles{segment.startAngle, middle, segment.endAngle};
            for (std::size_t point = 0U; point < angles.size(); ++point) {
                geometry.points[point * 2U] = std::cos(angles[point]) * segment.outerRadius;
                geometry.points[point * 2U + 1U] = std::sin(angles[point]) * segment.outerRadius;
                geometry.points[6U + point * 2U]
                    = std::cos(angles[point]) * segment.innerRadius;
                geometry.points[6U + point * 2U + 1U]
                    = std::sin(angles[point]) * segment.innerRadius;
            }
            prepared.push_back(geometry);
        }
        const auto endTime = std::chrono::steady_clock::now();
        samples[frameIndex] = std::chrono::duration<double, std::milli>(
            endTime - startTime).count();
        checksum += static_cast<std::uint64_t>(
            std::abs(prepared[frameIndex % prepared.size()].points[frameIndex % 12U])
            * 10'000.0F);
    }
    std::ranges::sort(samples);
    const auto p95Index = static_cast<std::size_t>(
        std::ceil(static_cast<double>(frame_count) * 0.95)) - 1U;
    const auto p95 = samples[p95Index];
    const auto average = std::accumulate(samples.begin(), samples.end(), 0.0)
        / static_cast<double>(samples.size());
    std::cout << std::fixed << std::setprecision(3)
              << "segments=" << plan.morphs.size() << '\n'
              << "frames=" << frame_count << '\n'
              << "average_ms=" << average << '\n'
              << "p95_ms=" << p95 << '\n'
              << "checksum=" << checksum << '\n';
    return checksum == 0U || p95 > 8.0 ? 3 : 0;
}
