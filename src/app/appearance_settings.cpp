#include "app/appearance_settings.h"

namespace diskbloom::app {

float text_scale_factor(const TextScalePreset preset) noexcept {
    switch (preset) {
    case TextScalePreset::Percent80:
        return 0.80F;
    case TextScalePreset::Percent90:
        return 0.90F;
    case TextScalePreset::Percent100:
        return 1.00F;
    case TextScalePreset::Percent110:
        return 1.10F;
    case TextScalePreset::Percent120:
        return 1.20F;
    }
    return 1.00F;
}

float chart_scale_factor(const ChartScalePreset preset) noexcept {
    switch (preset) {
    case ChartScalePreset::Percent60:
        return 0.60F;
    case ChartScalePreset::Percent70:
        return 0.70F;
    case ChartScalePreset::Percent80:
        return 0.80F;
    case ChartScalePreset::Percent90:
        return 0.90F;
    case ChartScalePreset::Percent100:
        return 1.00F;
    }
    return 0.80F;
}

std::wstring_view body_font_family(const FontFamilyPreset preset) noexcept {
    switch (preset) {
    case FontFamilyPreset::SegoeUiVariable:
        return L"Segoe UI Variable Text";
    case FontFamilyPreset::MicrosoftYaHeiUi:
        return L"Microsoft YaHei UI";
    case FontFamilyPreset::Arial:
        return L"Arial";
    case FontFamilyPreset::Consolas:
        return L"Consolas";
    }
    return L"Segoe UI Variable Text";
}

std::wstring_view display_font_family(const FontFamilyPreset preset) noexcept {
    if (preset == FontFamilyPreset::SegoeUiVariable) {
        return L"Segoe UI Variable Display";
    }
    return body_font_family(preset);
}

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
    case SettingsCommand::TextScale80:
        settings.typography.textScale = TextScalePreset::Percent80;
        return true;
    case SettingsCommand::TextScale90:
        settings.typography.textScale = TextScalePreset::Percent90;
        return true;
    case SettingsCommand::TextScale100:
        settings.typography.textScale = TextScalePreset::Percent100;
        return true;
    case SettingsCommand::TextScale110:
        settings.typography.textScale = TextScalePreset::Percent110;
        return true;
    case SettingsCommand::TextScale120:
        settings.typography.textScale = TextScalePreset::Percent120;
        return true;
    case SettingsCommand::FontSegoeUiVariable:
        settings.typography.fontFamily = FontFamilyPreset::SegoeUiVariable;
        return true;
    case SettingsCommand::FontMicrosoftYaHeiUi:
        settings.typography.fontFamily = FontFamilyPreset::MicrosoftYaHeiUi;
        return true;
    case SettingsCommand::FontArial:
        settings.typography.fontFamily = FontFamilyPreset::Arial;
        return true;
    case SettingsCommand::FontConsolas:
        settings.typography.fontFamily = FontFamilyPreset::Consolas;
        return true;
    case SettingsCommand::ChartScale60:
        settings.chartScale = ChartScalePreset::Percent60;
        return true;
    case SettingsCommand::ChartScale70:
        settings.chartScale = ChartScalePreset::Percent70;
        return true;
    case SettingsCommand::ChartScale80:
        settings.chartScale = ChartScalePreset::Percent80;
        return true;
    case SettingsCommand::ChartScale90:
        settings.chartScale = ChartScalePreset::Percent90;
        return true;
    case SettingsCommand::ChartScale100:
        settings.chartScale = ChartScalePreset::Percent100;
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
