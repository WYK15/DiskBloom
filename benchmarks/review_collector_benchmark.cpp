#include "app/review_collector_interaction.h"

#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>

namespace {

constexpr std::size_t default_iteration_count = 1'000'000U;
constexpr std::uint64_t expected_default_checksum = 11'879'453U;

[[nodiscard]] std::size_t parse_iteration_count(const int argc, char** argv) {
    if (argc < 2) {
        return default_iteration_count;
    }

    std::size_t value = 0U;
    const std::string_view argument(argv[1]);
    const auto result = std::from_chars(
        argument.data(),
        argument.data() + argument.size(),
        value);
    return result.ec == std::errc{}
            && result.ptr == argument.data() + argument.size()
            && value > 0U
        ? value
        : 0U;
}

[[nodiscard]] double elapsed_milliseconds(
    const std::chrono::steady_clock::time_point start,
    const std::chrono::steady_clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

} // namespace

int main(const int argc, char** argv) {
    const auto iteration_count = parse_iteration_count(argc, argv);
    if (iteration_count == 0U) {
        std::cerr << "usage: diskbloom_review_collector_benchmark [iterations]\n";
        return 2;
    }

    constexpr diskbloom::app::AnalyzerRectF action_bar{
        .left = 0.0F,
        .top = 640.0F,
        .right = 1'200.0F,
        .bottom = 720.0F,
    };
    constexpr std::size_t item_count = 128U;
    const auto layout = diskbloom::app::compute_review_collector_layout(
        action_bar,
        1'200.0F,
        720.0F,
        item_count,
        7U);

    diskbloom::app::ReviewDragState drag;
    if (!drag.begin(42U, 0.0F, 0.0F)) {
        std::cerr << "drag setup failed\n";
        return 3;
    }

    std::uint64_t checksum = 0U;
    auto scroll_offset = layout.scrollOffset;
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t iteration = 0U; iteration < iteration_count; ++iteration) {
        const auto x_dip = static_cast<float>((iteration * 37U + 11U) % 1'200U);
        const auto y_dip = static_cast<float>((iteration * 53U + 7U) % 720U);
        const auto over_collector = diskbloom::app::contains_point(
            layout.summary,
            x_dip,
            y_dip);
        const auto over_panel = diskbloom::app::contains_point(
            layout.panel,
            x_dip,
            y_dip);
        const auto drag_changed = drag.move(x_dip, y_dip, over_collector);
        scroll_offset = diskbloom::app::next_review_scroll_offset(
            scroll_offset,
            item_count,
            layout.visibleCapacity,
            (iteration & 1U) == 0U ? 1 : -1);

        checksum += static_cast<std::uint64_t>(scroll_offset + 1U);
        checksum += over_collector ? 3U : 0U;
        checksum += over_panel ? 5U : 0U;
        checksum += drag_changed ? 7U : 0U;
        checksum += drag.valid_drop() ? 11U : 0U;
    }
    const auto end = std::chrono::steady_clock::now();

    if (!drag.active()
        || checksum == 0U
        || (iteration_count == default_iteration_count
            && checksum != expected_default_checksum)) {
        std::cerr << "benchmark validation failed: checksum=" << checksum;
        if (iteration_count == default_iteration_count) {
            std::cerr << " expected=" << expected_default_checksum;
        }
        std::cerr << '\n';
        return 4;
    }

    const auto total_ms = elapsed_milliseconds(start, end);
    const auto operations_per_second = static_cast<double>(iteration_count)
        / (total_ms / 1'000.0);
    std::cout << std::fixed << std::setprecision(2)
              << "iterations=" << iteration_count << '\n'
              << "total_ms=" << total_ms << '\n'
              << "operations_per_second=" << operations_per_second << '\n'
              << "checksum=" << checksum << '\n';
    return 0;
}
