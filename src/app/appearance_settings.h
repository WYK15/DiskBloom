#pragma once

#include "core/language.h"
#include "core/theme.h"

#include <cstdint>
#include <string_view>

namespace diskbloom::app {

enum class DirectoryTransitionMode : std::uint8_t {
    AlwaysOn,
    FollowSystem,
    Off,
};

enum class SettingsCommand : int {
    ThemeSystem = 1001,
    ThemeLight = 1002,
    ThemeDark = 1003,
    LanguageEnglish = 1011,
    LanguageChinese = 1012,
    DirectoryTransitionsAlwaysOn = 1021,
    DirectoryTransitionsFollowSystem = 1022,
    DirectoryTransitionsOff = 1023,
};

struct AppearanceSettings {
    core::ThemeMode themeMode;
    core::Language language;
    DirectoryTransitionMode directoryTransitions = DirectoryTransitionMode::AlwaysOn;
};

[[nodiscard]] bool resolve_directory_transitions(
    DirectoryTransitionMode mode,
    bool windowsAnimationsEnabled) noexcept;

[[nodiscard]] bool apply_settings_command(
    AppearanceSettings& settings,
    SettingsCommand command) noexcept;
[[nodiscard]] bool apply_launch_argument(
    AppearanceSettings& settings,
    std::wstring_view argument) noexcept;

} // namespace diskbloom::app
