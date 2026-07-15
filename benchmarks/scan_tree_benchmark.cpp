#include "core/scan_tree.h"

#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string_view>

namespace {

constexpr std::size_t default_node_count = 1'000'000U;
constexpr std::size_t entries_per_directory = 1'024U;
constexpr std::wstring_view root_name = L"benchmark-root";
constexpr std::wstring_view directory_name = L"directory";
constexpr std::wstring_view file_name = L"payload.bin";

[[nodiscard]] std::size_t parse_node_count(const int argc, char** argv) {
    if (argc < 2) {
        return default_node_count;
    }

    std::size_t value = 0U;
    const std::string_view argument(argv[1]);
    const auto parse_result = std::from_chars(
        argument.data(),
        argument.data() + argument.size(),
        value);
    if (parse_result.ec != std::errc{}
        || parse_result.ptr != argument.data() + argument.size()
        || value == 0U
        || value >= static_cast<std::size_t>(diskbloom::core::invalid_node)) {
        return 0U;
    }
    return value;
}

[[nodiscard]] double elapsed_milliseconds(
    const std::chrono::steady_clock::time_point start,
    const std::chrono::steady_clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

} // namespace

int main(const int argc, char** argv) {
    const auto node_count = parse_node_count(argc, argv);
    if (node_count == 0U) {
        std::cerr << "usage: diskbloom_scan_tree_benchmark [node-count]\n";
        return 2;
    }

    const auto directory_count = node_count > 1U
        ? (node_count - 2U) / entries_per_directory + 1U
        : 0U;
    const auto file_count = node_count - 1U - directory_count;
    const auto name_character_count = root_name.size()
        + directory_count * directory_name.size()
        + file_count * file_name.size();

    diskbloom::core::ScanTree tree;
    tree.reserve(node_count, name_character_count);

    const auto build_start = std::chrono::steady_clock::now();
    const auto root = tree.add_root(root_name, diskbloom::core::ScanNodeFlags::Directory);
    auto parent = root;
    std::uint64_t expected_bytes = 0U;
    for (std::size_t index = 1U; index < node_count; ++index) {
        if ((index - 1U) % entries_per_directory == 0U) {
            parent = tree.add_child(
                root,
                directory_name,
                0U,
                diskbloom::core::ScanNodeFlags::Directory);
        } else {
            const auto logical_size = static_cast<std::uint64_t>((index % 4'096U) + 1U);
            expected_bytes += logical_size;
            const auto child = tree.add_child(
                parent,
                file_name,
                logical_size,
                diskbloom::core::ScanNodeFlags::None);
            if (child == diskbloom::core::invalid_node) {
                std::cerr << "failed to append benchmark node\n";
                return 3;
            }
        }
    }
    const auto build_end = std::chrono::steady_clock::now();

    tree.aggregate();
    const auto aggregate_end = std::chrono::steady_clock::now();

    if (tree.nodes().size() != node_count || tree.node(root).logicalSize != expected_bytes) {
        std::cerr << "benchmark validation failed\n";
        return 4;
    }

    const auto build_ms = elapsed_milliseconds(build_start, build_end);
    const auto aggregate_ms = elapsed_milliseconds(build_end, aggregate_end);
    const auto total_ms = build_ms + aggregate_ms;
    const auto nodes_per_second = static_cast<double>(node_count) / (total_ms / 1'000.0);
    const auto memory_bytes = tree.estimated_memory_bytes();
    const auto bytes_per_node = static_cast<double>(memory_bytes)
        / static_cast<double>(node_count);

    std::cout << std::fixed << std::setprecision(2)
              << "nodes=" << node_count << '\n'
              << "build_ms=" << build_ms << '\n'
              << "aggregate_ms=" << aggregate_ms << '\n'
              << "total_ms=" << total_ms << '\n'
              << "nodes_per_second=" << nodes_per_second << '\n'
              << "estimated_memory_bytes=" << memory_bytes << '\n'
              << "estimated_bytes_per_node=" << bytes_per_node << '\n';
    return 0;
}
