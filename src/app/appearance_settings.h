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

enum class TextScalePreset : std::uint8_t {
    Percent80,
    Percent90,
    Percent100,
    Percent110,
    Percent120,
};

enum class FontFamilyPreset : std::uint8_t {
    SegoeUiVariable,
    MicrosoftYaHeiUi,
    Arial,
    Consolas,
};

enum class ChartScalePreset : std::uint8_t {
    Percent60,
    Percent70,
    Percent80,
    Percent90,
    Percent100,
};

struct TypographySettings {
    TextScalePreset textScale = TextScalePreset::Percent100;
    FontFamilyPreset fontFamily = FontFamilyPreset::SegoeUiVariable;

    bool operator==(const TypographySettings&) const noexcept = default;
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
    TextScale80 = 1031,
    TextScale90 = 1032,
    TextScale100 = 1033,
    TextScale110 = 1034,
    TextScale120 = 1035,
    FontSegoeUiVariable = 1041,
    FontMicrosoftYaHeiUi = 1042,
    FontArial = 1043,
    FontConsolas = 1044,
    ChartScale60 = 1051,
    ChartScale70 = 1052,
    ChartScale80 = 1053,
    ChartScale90 = 1054,
    ChartScale100 = 1055,
};

struct AppearanceSettings {
    core::ThemeMode themeMode;
    core::Language language;
    DirectoryTransitionMode directoryTransitions = DirectoryTransitionMode::AlwaysOn;
    TypographySettings typography{};
    ChartScalePreset chartScale = ChartScalePreset::Percent80;

    bool operator==(const AppearanceSettings&) const noexcept = default;
};

[[nodiscard]] float text_scale_factor(TextScalePreset preset) noexcept;
[[nodiscard]] float chart_scale_factor(ChartScalePreset preset) noexcept;
[[nodiscard]] std::wstring_view body_font_family(FontFamilyPreset preset) noexcept;
[[nodiscard]] std::wstring_view display_font_family(FontFamilyPreset preset) noexcept;

[[nodiscard]] bool resolve_directory_transitions(
    DirectoryTransitionMode mode,
    bool windowsAnimationsEnabled) noexcept;

[[nodiscard]] bool is_directory_transition_command(SettingsCommand command) noexcept;

[[nodiscard]] bool apply_settings_command(
    AppearanceSettings& settings,
    SettingsCommand command) noexcept;
[[nodiscard]] bool apply_launch_argument(
    AppearanceSettings& settings,
    std::wstring_view argument) noexcept;

} // namespace diskbloom::app
