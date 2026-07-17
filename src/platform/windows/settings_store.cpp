#include "platform/windows/settings_store.h"

#include <Windows.h>
#include <ShlObj.h>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <system_error>

namespace diskbloom::platform::windows {
namespace {

constexpr std::string_view settings_header = "DiskBloomSettings/2";
constexpr std::string_view always_on_settings =
    "DiskBloomSettings/1\ndirectoryTransitions=always\n";
constexpr std::string_view follow_system_settings =
    "DiskBloomSettings/1\ndirectoryTransitions=system\n";
constexpr std::string_view off_settings =
    "DiskBloomSettings/1\ndirectoryTransitions=off\n";

std::filesystem::path settings_path(const wchar_t* const filename) {
    PWSTR localAppData = nullptr;
    if (FAILED(SHGetKnownFolderPath(
            FOLDERID_LocalAppData,
            KF_FLAG_DEFAULT,
            nullptr,
            &localAppData))) {
        return {};
    }
    std::filesystem::path path(localAppData);
    CoTaskMemFree(localAppData);
    return path / L"DiskBloom" / filename;
}

std::optional<std::string> read_bounded_file(
    const std::filesystem::path& path,
    const LONGLONG maximumSize) {
    const auto file = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return std::nullopt;
    }

    LARGE_INTEGER size{};
    if (GetFileSizeEx(file, &size) == FALSE
        || size.QuadPart <= 0
        || size.QuadPart > maximumSize) {
        CloseHandle(file);
        return std::nullopt;
    }

    std::string contents(static_cast<std::size_t>(size.QuadPart), '\0');
    DWORD bytesRead = 0U;
    const auto read = ReadFile(
        file,
        contents.data(),
        static_cast<DWORD>(contents.size()),
        &bytesRead,
        nullptr);
    CloseHandle(file);
    if (read == FALSE || bytesRead != contents.size()) {
        return std::nullopt;
    }
    return contents;
}

std::string_view theme_token(const core::ThemeMode value) noexcept {
    switch (value) {
    case core::ThemeMode::System:
        return "system";
    case core::ThemeMode::Light:
        return "light";
    case core::ThemeMode::Dark:
        return "dark";
    }
    return "system";
}

std::string_view language_token(const core::Language value) noexcept {
    return value == core::Language::SimplifiedChinese ? "zh-CN" : "en-US";
}

std::string_view transition_token(const app::DirectoryTransitionMode value) noexcept {
    switch (value) {
    case app::DirectoryTransitionMode::AlwaysOn:
        return "always";
    case app::DirectoryTransitionMode::FollowSystem:
        return "system";
    case app::DirectoryTransitionMode::Off:
        return "off";
    }
    return "always";
}

std::string_view text_scale_token(const app::TextScalePreset value) noexcept {
    switch (value) {
    case app::TextScalePreset::Percent80:
        return "80";
    case app::TextScalePreset::Percent90:
        return "90";
    case app::TextScalePreset::Percent100:
        return "100";
    case app::TextScalePreset::Percent110:
        return "110";
    case app::TextScalePreset::Percent120:
        return "120";
    }
    return "100";
}

std::string_view font_token(const app::FontFamilyPreset value) noexcept {
    switch (value) {
    case app::FontFamilyPreset::SegoeUiVariable:
        return "segoe-variable";
    case app::FontFamilyPreset::MicrosoftYaHeiUi:
        return "microsoft-yahei-ui";
    case app::FontFamilyPreset::Arial:
        return "arial";
    case app::FontFamilyPreset::Consolas:
        return "consolas";
    }
    return "segoe-variable";
}

std::string_view chart_scale_token(const app::ChartScalePreset value) noexcept {
    switch (value) {
    case app::ChartScalePreset::Percent60:
        return "60";
    case app::ChartScalePreset::Percent70:
        return "70";
    case app::ChartScalePreset::Percent80:
        return "80";
    case app::ChartScalePreset::Percent90:
        return "90";
    case app::ChartScalePreset::Percent100:
        return "100";
    }
    return "80";
}

std::string serialize_settings(const app::AppearanceSettings& settings) {
    std::string output;
    output.reserve(256U);
    const auto append = [&](const std::string_view key, const std::string_view value) {
        output.append(key);
        output.push_back('=');
        output.append(value);
        output.push_back('\n');
    };
    output.append(settings_header);
    output.push_back('\n');
    append("theme", theme_token(settings.themeMode));
    append("language", language_token(settings.language));
    append("directoryTransitions", transition_token(settings.directoryTransitions));
    append("textScale", text_scale_token(settings.typography.textScale));
    append("fontFamily", font_token(settings.typography.fontFamily));
    append("chartScale", chart_scale_token(settings.chartScale));
    return output;
}

bool write_atomic(
    const std::filesystem::path& path,
    const std::string_view contents) noexcept {
    try {
        if (path.empty() || contents.empty()) {
            return false;
        }
        std::error_code directoryError;
        std::filesystem::create_directories(path.parent_path(), directoryError);
        if (directoryError) {
            return false;
        }

        auto temporaryPath = path;
        temporaryPath += L".tmp";
        DeleteFileW(temporaryPath.c_str());
        const auto file = CreateFileW(
            temporaryPath.c_str(),
            GENERIC_WRITE,
            0U,
            nullptr,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
            nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            return false;
        }

        DWORD bytesWritten = 0U;
        const auto wrote = WriteFile(
            file,
            contents.data(),
            static_cast<DWORD>(contents.size()),
            &bytesWritten,
            nullptr);
        const auto flushed = wrote != FALSE
            && bytesWritten == contents.size()
            && FlushFileBuffers(file) != FALSE;
        CloseHandle(file);
        if (!flushed) {
            DeleteFileW(temporaryPath.c_str());
            return false;
        }
        if (MoveFileExW(
                temporaryPath.c_str(),
                path.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == FALSE) {
            DeleteFileW(temporaryPath.c_str());
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_theme(const std::string_view value, core::ThemeMode& result) noexcept {
    if (value == "system") result = core::ThemeMode::System;
    else if (value == "light") result = core::ThemeMode::Light;
    else if (value == "dark") result = core::ThemeMode::Dark;
    else return false;
    return true;
}

bool parse_language(const std::string_view value, core::Language& result) noexcept {
    if (value == "en-US") result = core::Language::English;
    else if (value == "zh-CN") result = core::Language::SimplifiedChinese;
    else return false;
    return true;
}

bool parse_transition(
    const std::string_view value,
    app::DirectoryTransitionMode& result) noexcept {
    if (value == "always") result = app::DirectoryTransitionMode::AlwaysOn;
    else if (value == "system") result = app::DirectoryTransitionMode::FollowSystem;
    else if (value == "off") result = app::DirectoryTransitionMode::Off;
    else return false;
    return true;
}

bool parse_text_scale(
    const std::string_view value,
    app::TextScalePreset& result) noexcept {
    if (value == "80") result = app::TextScalePreset::Percent80;
    else if (value == "90") result = app::TextScalePreset::Percent90;
    else if (value == "100") result = app::TextScalePreset::Percent100;
    else if (value == "110") result = app::TextScalePreset::Percent110;
    else if (value == "120") result = app::TextScalePreset::Percent120;
    else return false;
    return true;
}

bool parse_font(const std::string_view value, app::FontFamilyPreset& result) noexcept {
    if (value == "segoe-variable") result = app::FontFamilyPreset::SegoeUiVariable;
    else if (value == "microsoft-yahei-ui") result = app::FontFamilyPreset::MicrosoftYaHeiUi;
    else if (value == "arial") result = app::FontFamilyPreset::Arial;
    else if (value == "consolas") result = app::FontFamilyPreset::Consolas;
    else return false;
    return true;
}

bool parse_chart_scale(
    const std::string_view value,
    app::ChartScalePreset& result) noexcept {
    if (value == "60") result = app::ChartScalePreset::Percent60;
    else if (value == "70") result = app::ChartScalePreset::Percent70;
    else if (value == "80") result = app::ChartScalePreset::Percent80;
    else if (value == "90") result = app::ChartScalePreset::Percent90;
    else if (value == "100") result = app::ChartScalePreset::Percent100;
    else return false;
    return true;
}

} // namespace

std::filesystem::path default_settings_path() {
    return settings_path(L"settings-v2.ini");
}

std::filesystem::path legacy_settings_path() {
    return settings_path(L"settings-v1.ini");
}

std::optional<app::AppearanceSettings> load_settings(
    const std::filesystem::path& path,
    const app::AppearanceSettings& defaults) noexcept {
    try {
        const auto file = read_bounded_file(path, 2048);
        if (!file.has_value()) {
            return std::nullopt;
        }
        std::string_view remaining(*file);
        const auto headerEnd = remaining.find('\n');
        if (headerEnd == std::string_view::npos
            || remaining.substr(0U, headerEnd) != settings_header) {
            return std::nullopt;
        }
        remaining.remove_prefix(headerEnd + 1U);
        auto result = defaults;
        while (!remaining.empty()) {
            const auto lineEnd = remaining.find('\n');
            const auto line = remaining.substr(0U, lineEnd);
            remaining = lineEnd == std::string_view::npos
                ? std::string_view{}
                : remaining.substr(lineEnd + 1U);
            if (line.empty()) {
                continue;
            }
            const auto separator = line.find('=');
            if (separator == std::string_view::npos) {
                continue;
            }
            const auto key = line.substr(0U, separator);
            const auto value = line.substr(separator + 1U);
            if (key == "theme") {
                if (!parse_theme(value, result.themeMode)) {
                    result.themeMode = defaults.themeMode;
                }
            } else if (key == "language") {
                if (!parse_language(value, result.language)) {
                    result.language = defaults.language;
                }
            }
            else if (key == "directoryTransitions") {
                if (!parse_transition(value, result.directoryTransitions)) {
                    result.directoryTransitions = defaults.directoryTransitions;
                }
            } else if (key == "textScale") {
                if (!parse_text_scale(value, result.typography.textScale)) {
                    result.typography.textScale = defaults.typography.textScale;
                }
            } else if (key == "fontFamily") {
                if (!parse_font(value, result.typography.fontFamily)) {
                    result.typography.fontFamily = defaults.typography.fontFamily;
                }
            } else if (key == "chartScale") {
                if (!parse_chart_scale(value, result.chartScale)) {
                    result.chartScale = defaults.chartScale;
                }
            }
        }
        return result;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<app::DirectoryTransitionMode> load_legacy_directory_transition_mode(
    const std::filesystem::path& path) noexcept {
    try {
        const auto contents = read_bounded_file(path, 512);
        if (!contents.has_value()) {
            return std::nullopt;
        }
        if (*contents == always_on_settings) return app::DirectoryTransitionMode::AlwaysOn;
        if (*contents == follow_system_settings) return app::DirectoryTransitionMode::FollowSystem;
        if (*contents == off_settings) return app::DirectoryTransitionMode::Off;
    } catch (...) {
        return std::nullopt;
    }
    return std::nullopt;
}

bool save_settings_atomic(
    const std::filesystem::path& path,
    const app::AppearanceSettings& settings) noexcept {
    try {
        return write_atomic(path, serialize_settings(settings));
    } catch (...) {
        return false;
    }
}

std::optional<app::DirectoryTransitionMode> load_directory_transition_mode(
    const std::filesystem::path& path) noexcept {
    return load_legacy_directory_transition_mode(path);
}

bool save_directory_transition_mode_atomic(
    const std::filesystem::path& path,
    const app::DirectoryTransitionMode mode) noexcept {
    switch (mode) {
    case app::DirectoryTransitionMode::AlwaysOn:
        return write_atomic(path, always_on_settings);
    case app::DirectoryTransitionMode::FollowSystem:
        return write_atomic(path, follow_system_settings);
    case app::DirectoryTransitionMode::Off:
        return write_atomic(path, off_settings);
    }
    return false;
}

} // namespace diskbloom::platform::windows
