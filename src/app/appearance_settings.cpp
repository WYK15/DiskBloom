#include "app/appearance_settings.h"

namespace diskbloom::app {

bool apply_settings_command(
    AppearanceSettings& settings,
    const SettingsCommand command) noexcept {
    switch (command) {
    case SettingsCommand::ThemeSystem:
        settings.themeMode = core::ThemeMode::System;
        return true;
    case SettingsCommand::ThemeLight:
        settings.themeMode = core::ThemeMode::Light;
        return true;
    case SettingsCommand::ThemeDark:
        settings.themeMode = core::ThemeMode::Dark;
        return true;
    case SettingsCommand::LanguageEnglish:
        settings.language = core::Language::English;
        return true;
    case SettingsCommand::LanguageChinese:
        settings.language = core::Language::SimplifiedChinese;
        return true;
    default:
        return false;
    }
}

bool apply_launch_argument(
    AppearanceSettings& settings,
    const std::wstring_view argument) noexcept {
    if (argument == L"--theme=light") {
        settings.themeMode = core::ThemeMode::Light;
        return true;
    }
    if (argument == L"--theme=dark") {
        settings.themeMode = core::ThemeMode::Dark;
        return true;
    }
    if (argument == L"--theme=system") {
        settings.themeMode = core::ThemeMode::System;
        return true;
    }
    if (argument == L"--language=en-US") {
        settings.language = core::Language::English;
        return true;
    }
    if (argument == L"--language=zh-CN") {
        settings.language = core::Language::SimplifiedChinese;
        return true;
    }
    return false;
}

} // namespace diskbloom::app
