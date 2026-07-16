#include "core/scan_tree.h"
#include "core/scan_tree_exclusion.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <vector>

namespace {

constexpr std::size_t directory_count = 256U;
constexpr std::size_t files_per_directory = 1'023U;
constexpr std::size_t iteration_count = 200U;
constexpr std::size_t query_count = 4'096U;
constexpr double maximum_p95_ms = 5.0;
constexpr std::uint64_t expected_checksum = 120'265'807'856ULL;

[[nodiscard]] double milliseconds(
    const std::chrono::steady_clock::time_point start,
    const std::chrono::steady_clock::time_point end) noexcept {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

} // namespace

int main() {
    using namespace diskbloom::core;
    ScanTree tree;
    constexpr auto nodeCount = 1U + directory_count * (files_per_directory + 1U);
    tree.reserve(nodeCount, nodeCount * 8U);
    const auto root = tree.add_root(L"root", ScanNodeFlags::Directory);
    std::vector<NodeIndex> directories;
    directories.reserve(directory_count);
    for (std::size_t directory = 0U; directory < directory_count; ++directory) {
        const auto parent = tree.add_child(root, L"folder", 0U, ScanNodeFlags::Directory);
        directories.push_back(parent);
        for (std::size_t file = 0U; file < files_per_directory; ++file) {
            (void)tree.add_child(
                parent,
                L"file.bin",
                static_cast<std::uint64_t>((directory * 31U + file) % 65'521U + 1U),
                ScanNodeFlags::None);
        }
    }
    tree.aggregate();

    std::vector<NodeIndex> excluded;
    excluded.reserve(directory_count / 2U);
    for (std::size_t index = 0U; index < directories.size(); index += 2U) {
        excluded.push_back(directories[index]);
    }
    std::vector<double> samples;
    samples.reserve(iteration_count);
    ScanTreeExclusion exclusion;
    std::uint64_t checksum = 0U;
    for (std::size_t iteration = 0U; iteration < iteration_count; ++iteration) {
        const auto start = std::chrono::steady_clock::now();
        exclusion.rebuild(tree, excluded);
        checksum += exclusion.effective_size(tree, root);
        for (std::size_t query = 0U; query < query_count; ++query) {
            const auto node = static_cast<NodeIndex>(
                1U + (query * 65'537U + iteration * 17U) % (nodeCount - 1U));
            checksum += exclusion.effective_size(tree, node);
            checksum += exclusion.is_visible(tree, node) ? 1U : 0U;
        }
        samples.push_back(milliseconds(start, std::chrono::steady_clock::now()));
    }

    std::ranges::sort(samples);
    const auto total = std::accumulate(samples.begin(), samples.end(), 0.0);
    const auto p95Index = static_cast<std::size_t>(
        static_cast<double>(samples.size() - 1U) * 0.95);
    const auto average = total / static_cast<double>(samples.size());
    const auto p95 = samples[p95Index];
    std::cout << std::fixed << std::setprecision(3)
              << "nodes=" << tree.nodes().size() << '\n'
              << "excluded_roots=" << exclusion.roots().size() << '\n'
              << "iterations=" << iteration_count << '\n'
              << "average_ms=" << average << '\n'
              << "p95_ms=" << p95 << '\n'
              << "checksum=" << checksum << '\n';
    return tree.nodes().size() == nodeCount
            && exclusion.roots().size() == excluded.size()
            && checksum == expected_checksum
            && p95 <= maximum_p95_ms
        ? 0
        : 2;
}
