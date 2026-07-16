#include "app/appearance_settings.h"

namespace diskbloom::app {

bool resolve_directory_transitions(
    const DirectoryTransitionMode mode,
    const bool windowsAnimationsEnabled) noexcept {
    switch (mode) {
    case DirectoryTransitionMode::AlwaysOn:
        return true;
    case DirectoryTransitionMode::FollowSystem:
        return windowsAnimationsEnabled;
    case DirectoryTransitionMode::Off:
        return false;
    }
    return false;
}

bool is_directory_transition_command(const SettingsCommand command) noexcept {
    switch (command) {
    case SettingsCommand::DirectoryTransitionsAlwaysOn:
    case SettingsCommand::DirectoryTransitionsFollowSystem:
    case SettingsCommand::DirectoryTransitionsOff:
        return true;
    default:
        return false;
    }
}

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
    case SettingsCommand::DirectoryTransitionsAlwaysOn:
        settings.directoryTransitions = DirectoryTransitionMode::AlwaysOn;
        return true;
    case SettingsCommand::DirectoryTransitionsFollowSystem:
        settings.directoryTransitions = DirectoryTransitionMode::FollowSystem;
        return true;
    case SettingsCommand::DirectoryTransitionsOff:
        settings.directoryTransitions = DirectoryTransitionMode::Off;
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
    if (argument == L"--directory-transitions=always") {
        settings.directoryTransitions = DirectoryTransitionMode::AlwaysOn;
        return true;
    }
    if (argument == L"--directory-transitions=system") {
        settings.directoryTransitions = DirectoryTransitionMode::FollowSystem;
        return true;
    }
    if (argument == L"--directory-transitions=off") {
        settings.directoryTransitions = DirectoryTransitionMode::Off;
        return true;
    }
    return false;
}

} // namespace diskbloom::app
