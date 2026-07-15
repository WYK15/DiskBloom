#include "test_support.h"

#include "platform/windows/recycle_bin.h"

#include <array>
#include <string>

using diskbloom::platform::windows::RecycleResult;
using diskbloom::platform::windows::recycle_paths;

TEST_CASE(recycle_bin_rejects_empty_collection) {
    CHECK(recycle_paths({}, nullptr) == RecycleResult::InvalidPath);
}

TEST_CASE(recycle_bin_rejects_drive_roots) {
    const std::array paths{std::wstring(L"C:\\")};
    CHECK(recycle_paths(paths, nullptr) == RecycleResult::InvalidPath);
}

TEST_CASE(recycle_bin_rejects_missing_paths_before_file_operation) {
    const std::array paths{
        std::wstring(L"Z:\\diskbloom-recycle-path-that-does-not-exist"),
    };
    CHECK(recycle_paths(paths, nullptr) == RecycleResult::NotFound);
}
