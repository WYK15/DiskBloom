#pragma once

#include "core/language.h"
#include "core/theme.h"

#include <string_view>

namespace diskbloom::app {

enum class SettingsCommand : int {
    ThemeSystem = 1001,
    ThemeLight = 1002,
    ThemeDark = 1003,
    LanguageEnglish = 1011,
    LanguageChinese = 1012,
};

struct AppearanceSettings {
    core::ThemeMode themeMode;
    core::Language language;
};

[[nodiscard]] bool apply_settings_command(
    AppearanceSettings& settings,
    SettingsCommand command) noexcept;
[[nodiscard]] bool apply_launch_argument(
    AppearanceSettings& settings,
    std::wstring_view argument) noexcept;

} // namespace diskbloom::app
