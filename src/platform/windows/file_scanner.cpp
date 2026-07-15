#include "platform/windows/file_scanner.h"

#include <Windows.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace diskbloom::platform::windows {
namespace {

class FindHandle final {
public:
    explicit FindHandle(const HANDLE value) noexcept : value_(value) {}
    ~FindHandle() {
        if (value_ != INVALID_HANDLE_VALUE) {
            FindClose(value_);
        }
    }

    FindHandle(const FindHandle&) = delete;
    FindHandle& operator=(const FindHandle&) = delete;

    [[nodiscard]] HANDLE get() const noexcept { return value_; }
    [[nodiscard]] bool valid() const noexcept { return value_ != INVALID_HANDLE_VALUE; }

private:
    HANDLE value_;
};

struct DirectoryWork {
    core::NodeIndex node;
    std::wstring apiPath;
};

std::wstring absolute_path(const std::wstring_view path) {
    if (path.empty()) {
        return {};
    }

    const std::wstring input(path);
    const auto required = GetFullPathNameW(input.c_str(), 0U, nullptr, nullptr);
    if (required == 0U) {
        return {};
    }

    std::wstring result(required, L'\0');
    const auto length = GetFullPathNameW(
        input.c_str(),
        required,
        result.data(),
        nullptr);
    if (length == 0U || length >= required) {
        return {};
    }
    result.resize(length);
    return result;
}

std::wstring long_path(const std::wstring_view absolute) {
    if (absolute.starts_with(L"\\\\?\\")) {
        return std::wstring(absolute);
    }
    if (absolute.starts_with(L"\\\\")) {
        std::wstring result = L"\\\\?\\UNC\\";
        result.append(absolute.substr(2U));
        return result;
    }
    std::wstring result = L"\\\\?\\";
    result.append(absolute);
    return result;
}

std::wstring display_root_name(std::wstring_view path) {
    while (path.size() > 3U && (path.back() == L'\\' || path.back() == L'/')) {
        path.remove_suffix(1U);
    }
    const auto separator = path.find_last_of(L"\\/");
    if (separator == std::wstring_view::npos || separator + 1U >= path.size()) {
        return std::wstring(path);
    }
    return std::wstring(path.substr(separator + 1U));
}

std::wstring child_path(const std::wstring_view parent, const std::wstring_view name) {
    std::wstring result;
    result.reserve(parent.size() + name.size() + 1U);
    result.append(parent);
    if (!result.empty() && result.back() != L'\\') {
        result.push_back(L'\\');
    }
    result.append(name);
    return result;
}

std::wstring search_pattern(const std::wstring_view directory) {
    return child_path(directory, L"*");
}

bool is_dot_entry(const wchar_t* name) noexcept {
    return name[0] == L'.'
        && (name[1] == L'\0' || (name[1] == L'.' && name[2] == L'\0'));
}

std::uint64_t file_size(const WIN32_FIND_DATAW& data) noexcept {
    return (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32U)
        | static_cast<std::uint64_t>(data.nFileSizeLow);
}

class ProgressPublisher final {
public:
    explicit ProgressPublisher(ProgressCallback callback)
        : callback_(std::move(callback)), last_time_(std::chrono::steady_clock::now()) {}

    void publish(const ScanProgress& progress, const bool force) {
        if (!callback_) {
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        if (!force
            && progress.entries - last_entries_ < 4096U
            && now - last_time_ < std::chrono::milliseconds(50)) {
            return;
        }

        callback_(progress);
        last_entries_ = progress.entries;
        last_time_ = now;
    }

private:
    ProgressCallback callback_;
    std::uint64_t last_entries_ = 0U;
    std::chrono::steady_clock::time_point last_time_;
};

ScanResult cancelled_result(
    ScanProgress progress,
    core::ScanTree tree,
    ProgressPublisher& publisher) {
    tree.aggregate();
    publisher.publish(progress, true);
    return {ScanCompletion::Cancelled, progress, std::move(tree)};
}

} // namespace

ScanResult scan_directory(
    const std::wstring_view rootPath,
    const std::stop_token stopToken,
    ProgressCallback progressCallback) {
    ScanProgress progress{};
    core::ScanTree tree;
    ProgressPublisher publisher(std::move(progressCallback));

    if (stopToken.stop_requested()) {
        return cancelled_result(progress, std::move(tree), publisher);
    }

    const auto absolute = absolute_path(rootPath);
    const auto api_root = long_path(absolute);
    const auto attributes = api_root.empty()
        ? INVALID_FILE_ATTRIBUTES
        : GetFileAttributesW(api_root.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0U) {
        progress.errors = 1U;
        publisher.publish(progress, true);
        return {ScanCompletion::RootUnavailable, progress, std::move(tree)};
    }

    tree.reserve(4096U, 65536U);
    const auto root = tree.add_root(display_root_name(absolute), core::ScanNodeFlags::Directory);
    if (root == core::invalid_node) {
        progress.errors = 1U;
        return {ScanCompletion::RootUnavailable, progress, std::move(tree)};
    }

    std::vector<DirectoryWork> pending;
    pending.reserve(256U);
    pending.push_back({root, api_root});

    while (!pending.empty()) {
        if (stopToken.stop_requested()) {
            return cancelled_result(progress, std::move(tree), publisher);
        }

        auto work = std::move(pending.back());
        pending.pop_back();
        const auto pattern = search_pattern(work.apiPath);
        WIN32_FIND_DATAW data{};
        auto raw_handle = FindFirstFileExW(
            pattern.c_str(),
            FindExInfoBasic,
            &data,
            FindExSearchNameMatch,
            nullptr,
            FIND_FIRST_EX_LARGE_FETCH);
        if (raw_handle == INVALID_HANDLE_VALUE && GetLastError() == ERROR_INVALID_PARAMETER) {
            raw_handle = FindFirstFileExW(
                pattern.c_str(),
                FindExInfoBasic,
                &data,
                FindExSearchNameMatch,
                nullptr,
                0U);
        }

        FindHandle find(raw_handle);
        if (!find.valid()) {
            ++progress.errors;
            tree.add_flags(
                work.node,
                core::ScanNodeFlags::Inaccessible | core::ScanNodeFlags::Incomplete);
            publisher.publish(progress, false);
            continue;
        }

        bool more = true;
        while (more) {
            if (stopToken.stop_requested()) {
                return cancelled_result(progress, std::move(tree), publisher);
            }

            if (!is_dot_entry(data.cFileName)) {
                const bool directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U;
                const bool reparse = (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U;
                auto flags = directory ? core::ScanNodeFlags::Directory : core::ScanNodeFlags::None;
                if (reparse) {
                    flags |= core::ScanNodeFlags::ReparsePoint;
                }

                const auto size = directory ? 0U : file_size(data);
                const auto child = tree.add_child(work.node, data.cFileName, size, flags);
                if (child == core::invalid_node) {
                    ++progress.errors;
                    tree.add_flags(work.node, core::ScanNodeFlags::Incomplete);
                } else {
                    ++progress.entries;
                    if (directory) {
                        ++progress.directories;
                        if (!reparse) {
                            pending.push_back({child, child_path(work.apiPath, data.cFileName)});
                        }
                    } else {
                        ++progress.files;
                        progress.logicalBytes += size;
                    }
                    publisher.publish(progress, false);
                }
            }

            more = FindNextFileW(find.get(), &data) != FALSE;
        }

        if (GetLastError() != ERROR_NO_MORE_FILES) {
            ++progress.errors;
            tree.add_flags(work.node, core::ScanNodeFlags::Incomplete);
        }
    }

    tree.aggregate();
    publisher.publish(progress, true);
    return {ScanCompletion::Completed, progress, std::move(tree)};
}

} // namespace diskbloom::platform::windows
