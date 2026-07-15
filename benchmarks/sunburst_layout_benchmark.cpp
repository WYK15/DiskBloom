#include "core/scan_tree.h"
#include "core/sunburst_layout.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

constexpr std::size_t default_node_count = 1'000'000U;
constexpr std::size_t default_layout_iterations = 1'000U;
constexpr std::size_t hit_count = 1'000'000U;
constexpr std::size_t entries_per_directory = 1'024U;
constexpr std::size_t point_count = 4'096U;
constexpr float pi = 3.14159265358979323846F;

[[nodiscard]] std::size_t parse_count(
    const int argc,
    char** argv,
    const int index,
    const std::size_t fallback) {
    if (argc <= index) {
        return fallback;
    }
    std::size_t value = 0U;
    const std::string_view argument(argv[index]);
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

[[nodiscard]] double milliseconds(
    const std::chrono::steady_clock::time_point start,
    const std::chrono::steady_clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

} // namespace

int main(const int argc, char** argv) {
    const auto nodeCount = parse_count(argc, argv, 1, default_node_count);
    const auto layoutIterations = parse_count(argc, argv, 2, default_layout_iterations);
    if (nodeCount < 2U || layoutIterations == 0U) {
        std::cerr << "usage: diskbloom_sunburst_layout_benchmark [nodes>=2] [layout-iterations]\n";
        return 2;
    }

    diskbloom::core::ScanTree tree;
    tree.reserve(nodeCount, nodeCount * 8U);
    const auto root = tree.add_root(L"root", diskbloom::core::ScanNodeFlags::Directory);
    auto parent = root;
    for (std::size_t index = 1U; index < nodeCount; ++index) {
        if ((index - 1U) % entries_per_directory == 0U) {
            parent = tree.add_child(
                root,
                L"folder",
                0U,
                diskbloom::core::ScanNodeFlags::Directory);
        } else {
            const auto child = tree.add_child(
                parent,
                L"file.bin",
                static_cast<std::uint64_t>((index % 4'096U) + 1U),
                diskbloom::core::ScanNodeFlags::None);
            if (child == diskbloom::core::invalid_node) {
                std::cerr << "failed to append fixture node\n";
                return 3;
            }
        }
    }
    tree.aggregate();

    diskbloom::core::SunburstLayout layout;
    std::size_t maximumSegments = 0U;
    const auto layoutStart = std::chrono::steady_clock::now();
    for (std::size_t iteration = 0U; iteration < layoutIterations; ++iteration) {
        layout = diskbloom::core::build_sunburst_layout(tree, root, {});
        maximumSegments = std::max(maximumSegments, layout.segments.size());
    }
    const auto layoutEnd = std::chrono::steady_clock::now();
    if (layout.segments.empty() || maximumSegments > 2'048U) {
        std::cerr << "layout validation failed\n";
        return 4;
    }

    const diskbloom::core::SunburstGeometry geometry{
        .centerX = 400.0F,
        .centerY = 360.0F,
        .innerRadius = 55.0F,
        .ringWidth = 42.0F,
    };
    std::vector<std::array<float, 2U>> points;
    points.reserve(point_count);
    for (std::size_t index = 0U; index < point_count; ++index) {
        const auto angle = -pi * 0.5F
            + 2.0F * pi * static_cast<float>(index) / static_cast<float>(point_count);
        const auto depth = static_cast<float>(index % 6U);
        const auto radius = geometry.innerRadius + geometry.ringWidth * (depth + 0.5F);
        points.push_back({
            geometry.centerX + std::cos(angle) * radius,
            geometry.centerY + std::sin(angle) * radius,
        });
    }

    std::uint64_t hitChecksum = 0U;
    const auto hitStart = std::chrono::steady_clock::now();
    for (std::size_t index = 0U; index < hit_count; ++index) {
        const auto& point = points[index % points.size()];
        const auto hit = diskbloom::core::hit_test_sunburst(
            layout,
            geometry,
            point[0],
            point[1]);
        if (hit.has_value()) {
            hitChecksum += static_cast<std::uint64_t>(hit->segmentIndex + 1U);
        }
    }
    const auto hitEnd = std::chrono::steady_clock::now();

    const auto layoutTotalMs = milliseconds(layoutStart, layoutEnd);
    const auto hitTotalMs = milliseconds(hitStart, hitEnd);
    std::cout << std::fixed << std::setprecision(2)
              << "nodes=" << nodeCount << '\n'
              << "layout_iterations=" << layoutIterations << '\n'
              << "layout_total_ms=" << layoutTotalMs << '\n'
              << "layout_average_ms=" << layoutTotalMs / static_cast<double>(layoutIterations) << '\n'
              << "maximum_segments=" << maximumSegments << '\n'
              << "hit_tests=" << hit_count << '\n'
              << "hit_total_ms=" << hitTotalMs << '\n'
              << "hits_per_second=" << static_cast<double>(hit_count) / (hitTotalMs / 1'000.0) << '\n'
              << "hit_checksum=" << hitChecksum << '\n';
    return hitChecksum == 0U ? 5 : 0;
}
