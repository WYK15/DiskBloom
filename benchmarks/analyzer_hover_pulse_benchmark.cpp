#include "app/analyzer_hover_pulse.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>

namespace {

constexpr std::size_t segment_count = 2'048U;
constexpr std::size_t iteration_count = 1'000U;
constexpr std::uint64_t expected_checksum = 1'024'000U;

} // namespace

int main() {
    diskbloom::core::SunburstLayout layout;
    layout.segments.reserve(segment_count);
    layout.segments.push_back({
        .node = 1U, .startAngle = 0.0F, .endAngle = 3.0F, .depth = 0U});
    layout.segments.push_back({
        .node = 2U, .startAngle = 3.0F, .endAngle = 6.0F, .depth = 0U});
    for (std::size_t index = 0U; index < segment_count - 2U; ++index) {
        const auto firstBranch = index < (segment_count - 2U) / 2U;
        const auto ordinal = firstBranch
            ? index
            : index - (segment_count - 2U) / 2U;
        const auto start = (firstBranch ? 0.0F : 3.0F)
            + static_cast<float>(ordinal) * 3.0F / 1'023.0F;
        const auto end = (firstBranch ? 0.0F : 3.0F)
            + static_cast<float>(ordinal + 1U) * 3.0F / 1'023.0F;
        layout.segments.push_back({
            .node = static_cast<diskbloom::core::NodeIndex>(index + 3U),
            .startAngle = start,
            .endAngle = end,
            .depth = 1U,
        });
    }

    std::array<double, iteration_count> samples{};
    std::uint64_t checksum = 0U;
    for (std::size_t iteration = 0U; iteration < iteration_count; ++iteration) {
        const auto started = std::chrono::steady_clock::now();
        const auto branch = diskbloom::app::find_hover_branch(layout, 1U);
        if (!branch.has_value()) {
            return 2;
        }
        std::size_t matches = 0U;
        for (const auto& segment : layout.segments) {
            matches += diskbloom::app::segment_is_in_hover_branch(segment, *branch) ? 1U : 0U;
        }
        const auto finished = std::chrono::steady_clock::now();
        samples[iteration] = std::chrono::duration<double, std::milli>(
            finished - started).count();
        checksum += matches;
    }

    std::ranges::sort(samples);
    const auto p95Index = static_cast<std::size_t>(
        std::ceil(static_cast<double>(iteration_count) * 0.95)) - 1U;
    const auto p95 = samples[p95Index];
    const auto average = std::accumulate(samples.begin(), samples.end(), 0.0)
        / static_cast<double>(samples.size());
    std::cout << std::fixed << std::setprecision(3)
              << "segments=" << layout.segments.size() << '\n'
              << "iterations=" << iteration_count << '\n'
              << "average_ms=" << average << '\n'
              << "p95_ms=" << p95 << '\n'
              << "checksum=" << checksum << '\n';
    return checksum != expected_checksum || p95 > 1.0 ? 3 : 0;
}
