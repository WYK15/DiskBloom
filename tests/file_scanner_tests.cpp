#include "test_support.h"

#include "platform/windows/file_scanner.h"

#include <Windows.h>

#include <array>
#include <stop_token>
#include <string>

using diskbloom::platform::windows::ScanCompletion;
using diskbloom::platform::windows::ScanProgress;
using diskbloom::platform::windows::scan_directory;

namespace {

void create_sized_file(const std::wstring& path, const std::uint64_t size) {
    const auto file = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        0U,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    CHECK(file != INVALID_HANDLE_VALUE);
    LARGE_INTEGER position{};
    position.QuadPart = static_cast<LONGLONG>(size);
    CHECK(SetFilePointerEx(file, position, nullptr, FILE_BEGIN));
    CHECK(SetEndOfFile(file));
    CloseHandle(file);
}

class TemporaryScanTree final {
public:
    TemporaryScanTree() {
        std::array<wchar_t, MAX_PATH + 1U> temporary_path{};
        std::array<wchar_t, MAX_PATH + 1U> unique_path{};
        CHECK(GetTempPathW(static_cast<DWORD>(temporary_path.size()), temporary_path.data()) != 0U);
        CHECK(GetTempFileNameW(temporary_path.data(), L"DBT", 0U, unique_path.data()) != 0U);
        CHECK(DeleteFileW(unique_path.data()));
        root = unique_path.data();
        nested = root + L"\\nested";
        empty = root + L"\\empty";
        root_file = root + L"\\root.bin";
        nested_file = nested + L"\\nested.bin";
        CHECK(CreateDirectoryW(root.c_str(), nullptr));
        CHECK(CreateDirectoryW(nested.c_str(), nullptr));
        CHECK(CreateDirectoryW(empty.c_str(), nullptr));
        create_sized_file(root_file, 10U);
        create_sized_file(nested_file, 20U);
    }

    ~TemporaryScanTree() {
        DeleteFileW(nested_file.c_str());
        DeleteFileW(root_file.c_str());
        RemoveDirectoryW(empty.c_str());
        RemoveDirectoryW(nested.c_str());
        RemoveDirectoryW(root.c_str());
    }

    std::wstring root;
    std::wstring nested;
    std::wstring empty;
    std::wstring root_file;
    std::wstring nested_file;
};

} // namespace

TEST_CASE(file_scanner_builds_exact_tree_and_totals) {
    TemporaryScanTree fixture;
    int callbacks = 0;
    ScanProgress final_progress{};
    const auto result = scan_directory(
        fixture.root,
        {},
        [&](const ScanProgress& progress) {
            ++callbacks;
            final_progress = progress;
        });

    CHECK(result.completion == ScanCompletion::Completed);
    CHECK(result.progress.files == 2U);
    CHECK(result.progress.directories == 2U);
    CHECK(result.progress.entries == 4U);
    CHECK(result.progress.logicalBytes == 30U);
    CHECK(result.progress.errors == 0U);
    CHECK(result.tree.nodes().size() == 5U);
    CHECK(result.tree.node(0U).logicalSize == 30U);
    CHECK(result.tree.node(0U).fileCount == 2U);
    CHECK(result.tree.node(0U).directoryCount == 2U);
    CHECK(callbacks >= 1);
    CHECK(final_progress.entries == result.progress.entries);
}

TEST_CASE(file_scanner_honors_pre_cancelled_token) {
    TemporaryScanTree fixture;
    std::stop_source stop;
    stop.request_stop();
    const auto result = scan_directory(fixture.root, stop.get_token(), {});
    CHECK(result.completion == ScanCompletion::Cancelled);
    CHECK(result.progress.entries == 0U);
}

TEST_CASE(file_scanner_reports_unavailable_root) {
    const auto result = scan_directory(L"Z:\\diskbloom-path-that-does-not-exist", {}, {});
    CHECK(result.completion == ScanCompletion::RootUnavailable);
    CHECK(result.progress.errors == 1U);
    CHECK(result.tree.nodes().empty());
}
