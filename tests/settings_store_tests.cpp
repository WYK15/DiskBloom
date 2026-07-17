#include "test_support.h"

#include "platform/windows/settings_store.h"

#include <Windows.h>

#include <atomic>
#include <array>
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

using diskbloom::app::AppearanceSettings;
using diskbloom::app::ChartScalePreset;
using diskbloom::app::DirectoryTransitionMode;
using diskbloom::app::FontFamilyPreset;
using diskbloom::app::TextScalePreset;
using diskbloom::core::Language;
using diskbloom::core::ThemeMode;
using diskbloom::platform::windows::load_legacy_directory_transition_mode;
using diskbloom::platform::windows::load_settings;
using diskbloom::platform::windows::save_settings_atomic;

TEST_CASE(settings_store_round_trips_complete_v2_snapshot) {
    const auto path = unique_settings_test_path();
    AppearanceSettings expected{ThemeMode::Dark, Language::SimplifiedChinese};
    expected.directoryTransitions = DirectoryTransitionMode::FollowSystem;
    expected.typography.textScale = TextScalePreset::Percent120;
    expected.typography.fontFamily = FontFamilyPreset::MicrosoftYaHeiUi;
    expected.chartScale = ChartScalePreset::Percent60;

    CHECK(save_settings_atomic(path, expected));
    const auto loaded = load_settings(
        path,
        AppearanceSettings{ThemeMode::System, Language::English});
    CHECK(loaded.has_value());
    if (loaded.has_value()) {
        CHECK(*loaded == expected);
    }
    cleanup_settings_test_path(path);
}

TEST_CASE(settings_store_v2_invalid_fields_fall_back_independently) {
    const auto path = unique_settings_test_path();
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output
            << "DiskBloomSettings/2\n"
            << "theme=purple\n"
            << "language=zh-CN\n"
            << "directoryTransitions=slow\n"
            << "textScale=135\n"
            << "fontFamily=unknown\n"
            << "chartScale=75\n"
            << "futureKey=future-value\n";
    }

    AppearanceSettings defaults{ThemeMode::Light, Language::English};
    defaults.directoryTransitions = DirectoryTransitionMode::Off;
    defaults.typography.textScale = TextScalePreset::Percent90;
    defaults.typography.fontFamily = FontFamilyPreset::Arial;
    defaults.chartScale = ChartScalePreset::Percent90;
    const auto loaded = load_settings(path, defaults);
    CHECK(loaded.has_value());
    if (loaded.has_value()) {
        CHECK(loaded->themeMode == ThemeMode::Light);
        CHECK(loaded->language == Language::SimplifiedChinese);
        CHECK(loaded->directoryTransitions == DirectoryTransitionMode::Off);
        CHECK(loaded->typography.textScale == TextScalePreset::Percent90);
        CHECK(loaded->typography.fontFamily == FontFamilyPreset::Arial);
        CHECK(loaded->chartScale == ChartScalePreset::Percent90);
    }

    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << "OtherSettings/2\nlanguage=zh-CN\n";
    }
    CHECK(!load_settings(path, defaults).has_value());
    cleanup_settings_test_path(path);
}

TEST_CASE(settings_store_round_trips_legacy_directory_transition_modes) {
    const auto path = unique_settings_test_path();
    struct LegacyCase {
        DirectoryTransitionMode mode;
        const char* contents;
    };
    constexpr std::array cases{
        LegacyCase{
            DirectoryTransitionMode::AlwaysOn,
            "DiskBloomSettings/1\ndirectoryTransitions=always\n"},
        LegacyCase{
            DirectoryTransitionMode::FollowSystem,
            "DiskBloomSettings/1\ndirectoryTransitions=system\n"},
        LegacyCase{
            DirectoryTransitionMode::Off,
            "DiskBloomSettings/1\ndirectoryTransitions=off\n"},
    };

    for (const auto& legacyCase : cases) {
        {
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            output << legacyCase.contents;
        }
        const auto loaded = load_legacy_directory_transition_mode(path);
        CHECK(loaded.has_value());
        if (!loaded.has_value()) {
            cleanup_settings_test_path(path);
            return;
        }
        CHECK(*loaded == legacyCase.mode);
    }

    cleanup_settings_test_path(path);
}

TEST_CASE(settings_store_legacy_missing_or_malformed_value_has_no_preference) {
    const auto path = unique_settings_test_path();
    CHECK(!load_legacy_directory_transition_mode(path).has_value());

    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << "DiskBloomSettings/1\ndirectoryTransitions=slow\n";
    }
    CHECK(!load_legacy_directory_transition_mode(path).has_value());

    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << "OtherSettings/1\ndirectoryTransitions=always\n";
    }
    CHECK(!load_legacy_directory_transition_mode(path).has_value());

    cleanup_settings_test_path(path);
}

TEST_CASE(settings_store_failed_temp_write_preserves_prior_value) {
    const auto path = unique_settings_test_path();
    AppearanceSettings initial{ThemeMode::Dark, Language::English};
    initial.typography.textScale = TextScalePreset::Percent110;
    initial.typography.fontFamily = FontFamilyPreset::Consolas;
    initial.chartScale = ChartScalePreset::Percent70;
    const auto initiallySaved = save_settings_atomic(path, initial);
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

    auto replacement = initial;
    replacement.themeMode = ThemeMode::Light;
    CHECK(!save_settings_atomic(path, replacement));
    const auto loaded = load_settings(
        path,
        AppearanceSettings{ThemeMode::System, Language::SimplifiedChinese});
    CHECK(loaded.has_value());
    if (!loaded.has_value()) {
        cleanup_settings_test_path(path);
        return;
    }
    CHECK(*loaded == initial);

    cleanup_settings_test_path(path);
}
