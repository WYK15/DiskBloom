#pragma once

#include "core/scan_tree.h"

#include <cstdint>
#include <functional>
#include <stop_token>
#include <string_view>

namespace diskbloom::platform::windows {

struct ScanProgress {
    std::uint64_t entries = 0U;
    std::uint64_t files = 0U;
    std::uint64_t directories = 0U;
    std::uint64_t logicalBytes = 0U;
    std::uint64_t errors = 0U;
};

enum class ScanCompletion {
    Completed,
    Cancelled,
    RootUnavailable,
};

struct ScanResult {
    ScanCompletion completion = ScanCompletion::RootUnavailable;
    ScanProgress progress;
    core::ScanTree tree;
};

using ProgressCallback = std::function<void(const ScanProgress&)>;

[[nodiscard]] ScanResult scan_directory(
    std::wstring_view rootPath,
    std::stop_token stopToken,
    ProgressCallback progressCallback);

} // namespace diskbloom::platform::windows
