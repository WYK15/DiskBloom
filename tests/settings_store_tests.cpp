#include "test_support.h"

#include "platform/windows/settings_store.h"

#include <Windows.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::filesystem::path unique_settings_test_path() {
    static std::atomic_uint32_t sequence{};
    std::wstring tempPath(MAX_PATH, L'\0');
    const auto length = GetTempPathW(
        static_cast<DWORD>(tempPath.size()),
        tempPath.data());
    tempPath.resize(length);
    auto directory = std::filesystem::path(tempPath)
        / (L"DiskBloomSettingsTests-"
            + std::to_wstring(GetCurrentProcessId())
            + L"-"
            + std::to_wstring(sequence.fetch_add(1U)));
    std::filesystem::create_directories(directory);
    return directory / L"settings-v1.ini";
}

void cleanup_settings_test_path(const std::filesystem::path& path) {
    std::error_code ignored;
    std::filesystem::remove_all(path.parent_path(), ignored);
}

} // namespace

using diskbloom::app::DirectoryTransitionMode;
using diskbloom::platform::windows::load_directory_transition_mode;
using diskbloom::platform::windows::save_directory_transition_mode_atomic;

TEST_CASE(settings_store_round_trips_directory_transition_modes) {
    const auto path = unique_settings_test_path();

    for (const auto mode : {
             DirectoryTransitionMode::AlwaysOn,
             DirectoryTransitionMode::FollowSystem,
             DirectoryTransitionMode::Off}) {
        const auto saved = save_directory_transition_mode_atomic(path, mode);
        CHECK(saved);
        if (!saved) {
            cleanup_settings_test_path(path);
            return;
        }
        const auto loaded = load_directory_transition_mode(path);
        CHECK(loaded.has_value());
        if (!loaded.has_value()) {
            cleanup_settings_test_path(path);
            return;
        }
        CHECK(*loaded == mode);
    }

    cleanup_settings_test_path(path);
}

TEST_CASE(settings_store_missing_or_malformed_value_has_no_preference) {
    const auto path = unique_settings_test_path();
    CHECK(!load_directory_transition_mode(path).has_value());

    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << "DiskBloomSettings/1\ndirectoryTransitions=slow\n";
    }
    CHECK(!load_directory_transition_mode(path).has_value());

    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << "OtherSettings/1\ndirectoryTransitions=always\n";
    }
    CHECK(!load_directory_transition_mode(path).has_value());

    cleanup_settings_test_path(path);
}

TEST_CASE(settings_store_failed_temp_write_preserves_prior_value) {
    const auto path = unique_settings_test_path();
    const auto initiallySaved = save_directory_transition_mode_atomic(
        path,
        DirectoryTransitionMode::AlwaysOn);
    CHECK(initiallySaved);
    if (!initiallySaved) {
        cleanup_settings_test_path(path);
        return;
    }
    const auto collisionCreated =
        CreateDirectoryW((path.wstring() + L".tmp").c_str(), nullptr) != FALSE;
    CHECK(collisionCreated);
    if (!collisionCreated) {
        cleanup_settings_test_path(path);
        return;
    }

    CHECK(!save_directory_transition_mode_atomic(path, DirectoryTransitionMode::Off));
    const auto loaded = load_directory_transition_mode(path);
    CHECK(loaded.has_value());
    if (!loaded.has_value()) {
        cleanup_settings_test_path(path);
        return;
    }
    CHECK(*loaded == DirectoryTransitionMode::AlwaysOn);

    cleanup_settings_test_path(path);
}
