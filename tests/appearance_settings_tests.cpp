#include "test_support.h"

#include "app/appearance_settings.h"

#include <array>
#include <cmath>
#include <utility>

using diskbloom::app::AppearanceSettings;
using diskbloom::app::ChartScalePreset;
using diskbloom::app::DirectoryTransitionMode;
using diskbloom::app::FontFamilyPreset;
using diskbloom::app::SettingsCommand;
using diskbloom::app::TextScalePreset;
using diskbloom::app::apply_launch_argument;
using diskbloom::app::apply_settings_command;
using diskbloom::app::body_font_family;
using diskbloom::app::chart_scale_factor;
using diskbloom::app::display_font_family;
using diskbloom::app::is_directory_transition_command;
using diskbloom::app::make_settings_candidate;
using diskbloom::app::resolve_directory_transitions;
using diskbloom::app::text_scale_factor;
using diskbloom::core::Language;
using diskbloom::core::ThemeMode;

TEST_CASE(appearance_settings_typography_and_chart_defaults_match_current_ui) {
    const AppearanceSettings settings{ThemeMode::System, Language::English};
    CHECK(settings.typography.textScale == TextScalePreset::Percent100);
    CHECK(settings.typography.fontFamily == FontFamilyPreset::SegoeUiVariable);
    CHECK(settings.chartScale == ChartScalePreset::Percent80);
    CHECK(std::abs(text_scale_factor(settings.typography.textScale) - 1.0F) < 0.001F);
    CHECK(std::abs(chart_scale_factor(settings.chartScale) - 0.80F) < 0.001F);
}

TEST_CASE(appearance_settings_maps_all_preset_values_without_string_parsing) {
    CHECK(std::abs(text_scale_factor(TextScalePreset::Percent80) - 0.80F) < 0.001F);
    CHECK(std::abs(text_scale_factor(TextScalePreset::Percent90) - 0.90F) < 0.001F);
    CHECK(std::abs(text_scale_factor(TextScalePreset::Percent100) - 1.00F) < 0.001F);
    CHECK(std::abs(text_scale_factor(TextScalePreset::Percent110) - 1.10F) < 0.001F);
    CHECK(std::abs(text_scale_factor(TextScalePreset::Percent120) - 1.20F) < 0.001F);
    CHECK(std::abs(chart_scale_factor(ChartScalePreset::Percent60) - 0.60F) < 0.001F);
    CHECK(std::abs(chart_scale_factor(ChartScalePreset::Percent70) - 0.70F) < 0.001F);
    CHECK(std::abs(chart_scale_factor(ChartScalePreset::Percent80) - 0.80F) < 0.001F);
    CHECK(std::abs(chart_scale_factor(ChartScalePreset::Percent90) - 0.90F) < 0.001F);
    CHECK(std::abs(chart_scale_factor(ChartScalePreset::Percent100) - 1.00F) < 0.001F);
    CHECK(body_font_family(FontFamilyPreset::SegoeUiVariable)
        == L"Segoe UI Variable Text");
    CHECK(display_font_family(FontFamilyPreset::SegoeUiVariable)
        == L"Segoe UI Variable Display");
    CHECK(body_font_family(FontFamilyPreset::MicrosoftYaHeiUi)
        == L"Microsoft YaHei UI");
    CHECK(body_font_family(FontFamilyPreset::Arial) == L"Arial");
    CHECK(display_font_family(FontFamilyPreset::Arial) == L"Arial");
    CHECK(body_font_family(FontFamilyPreset::Consolas) == L"Consolas");
    CHECK(display_font_family(FontFamilyPreset::Consolas) == L"Consolas");
}

TEST_CASE(appearance_settings_switch_theme_without_changing_language) {
    AppearanceSettings settings{ThemeMode::System, Language::SimplifiedChinese};
    CHECK(apply_settings_command(settings, SettingsCommand::ThemeLight));
    CHECK(settings.themeMode == ThemeMode::Light);
    CHECK(settings.language == Language::SimplifiedChinese);
}

TEST_CASE(appearance_settings_switch_language_without_changing_theme) {
    AppearanceSettings settings{ThemeMode::Dark, Language::SimplifiedChinese};
    CHECK(apply_settings_command(settings, SettingsCommand::LanguageEnglish));
    CHECK(settings.themeMode == ThemeMode::Dark);
    CHECK(settings.language == Language::English);
}

TEST_CASE(directory_transitions_default_to_always_on) {
    const AppearanceSettings settings{ThemeMode::System, Language::English};
    CHECK(settings.directoryTransitions == DirectoryTransitionMode::AlwaysOn);
}

TEST_CASE(directory_transition_policy_resolves_all_modes) {
    CHECK(resolve_directory_transitions(DirectoryTransitionMode::AlwaysOn, false));
    CHECK(resolve_directory_transitions(DirectoryTransitionMode::AlwaysOn, true));
    CHECK(!resolve_directory_transitions(DirectoryTransitionMode::FollowSystem, false));
    CHECK(resolve_directory_transitions(DirectoryTransitionMode::FollowSystem, true));
    CHECK(!resolve_directory_transitions(DirectoryTransitionMode::Off, false));
    CHECK(!resolve_directory_transitions(DirectoryTransitionMode::Off, true));
}

TEST_CASE(appearance_settings_switch_directory_transitions_without_other_changes) {
    AppearanceSettings settings{ThemeMode::Dark, Language::SimplifiedChinese};

    CHECK(apply_settings_command(
        settings,
        SettingsCommand::DirectoryTransitionsFollowSystem));
    CHECK(settings.directoryTransitions == DirectoryTransitionMode::FollowSystem);
    CHECK(settings.themeMode == ThemeMode::Dark);
    CHECK(settings.language == Language::SimplifiedChinese);

    CHECK(apply_settings_command(settings, SettingsCommand::DirectoryTransitionsOff));
    CHECK(settings.directoryTransitions == DirectoryTransitionMode::Off);

    CHECK(apply_settings_command(
        settings,
        SettingsCommand::DirectoryTransitionsAlwaysOn));
    CHECK(settings.directoryTransitions == DirectoryTransitionMode::AlwaysOn);
}

TEST_CASE(appearance_settings_applies_each_new_preset_without_touching_other_fields) {
    AppearanceSettings settings{ThemeMode::Dark, Language::SimplifiedChinese};
    settings.directoryTransitions = DirectoryTransitionMode::FollowSystem;

    constexpr std::array textCases{
        std::pair{SettingsCommand::TextScale80, TextScalePreset::Percent80},
        std::pair{SettingsCommand::TextScale90, TextScalePreset::Percent90},
        std::pair{SettingsCommand::TextScale100, TextScalePreset::Percent100},
        std::pair{SettingsCommand::TextScale110, TextScalePreset::Percent110},
        std::pair{SettingsCommand::TextScale120, TextScalePreset::Percent120},
    };
    for (const auto [command, expected] : textCases) {
        CHECK(apply_settings_command(settings, command));
        CHECK(settings.typography.textScale == expected);
        CHECK(settings.themeMode == ThemeMode::Dark);
        CHECK(settings.language == Language::SimplifiedChinese);
        CHECK(settings.directoryTransitions == DirectoryTransitionMode::FollowSystem);
    }

    constexpr std::array fontCases{
        std::pair{SettingsCommand::FontSegoeUiVariable, FontFamilyPreset::SegoeUiVariable},
        std::pair{SettingsCommand::FontMicrosoftYaHeiUi, FontFamilyPreset::MicrosoftYaHeiUi},
        std::pair{SettingsCommand::FontArial, FontFamilyPreset::Arial},
        std::pair{SettingsCommand::FontConsolas, FontFamilyPreset::Consolas},
    };
    for (const auto [command, expected] : fontCases) {
        CHECK(apply_settings_command(settings, command));
        CHECK(settings.typography.fontFamily == expected);
        CHECK(settings.chartScale == ChartScalePreset::Percent80);
    }

    constexpr std::array chartCases{
        std::pair{SettingsCommand::ChartScale60, ChartScalePreset::Percent60},
        std::pair{SettingsCommand::ChartScale70, ChartScalePreset::Percent70},
        std::pair{SettingsCommand::ChartScale80, ChartScalePreset::Percent80},
        std::pair{SettingsCommand::ChartScale90, ChartScalePreset::Percent90},
        std::pair{SettingsCommand::ChartScale100, ChartScalePreset::Percent100},
    };
    for (const auto [command, expected] : chartCases) {
        CHECK(apply_settings_command(settings, command));
        CHECK(settings.chartScale == expected);
        CHECK(settings.typography.fontFamily == FontFamilyPreset::Consolas);
    }
}

TEST_CASE(appearance_settings_reject_unknown_command) {
    AppearanceSettings settings{ThemeMode::Dark, Language::English};
    CHECK(!apply_settings_command(settings, static_cast<SettingsCommand>(9999)));
    CHECK(settings.themeMode == ThemeMode::Dark);
    CHECK(settings.language == Language::English);
}

TEST_CASE(appearance_settings_builds_only_changed_valid_candidates) {
    const AppearanceSettings active{ThemeMode::System, Language::English};
    const auto candidate = make_settings_candidate(
        active,
        SettingsCommand::ChartScale60);
    CHECK(candidate.has_value());
    if (candidate.has_value()) {
        CHECK(candidate->chartScale == ChartScalePreset::Percent60);
        CHECK(*candidate != active);
    }
    CHECK(active.chartScale == ChartScalePreset::Percent80);
    CHECK(!make_settings_candidate(active, SettingsCommand::TextScale100).has_value());
    CHECK(!make_settings_candidate(
        active,
        static_cast<SettingsCommand>(9999)).has_value());
}

TEST_CASE(appearance_settings_accept_qa_launch_arguments) {
    AppearanceSettings settings{ThemeMode::System, Language::SimplifiedChinese};
    CHECK(apply_launch_argument(settings, L"--theme=light"));
    CHECK(apply_launch_argument(settings, L"--language=en-US"));
    CHECK(settings.themeMode == ThemeMode::Light);
    CHECK(settings.language == Language::English);

    CHECK(apply_launch_argument(settings, L"--theme=dark"));
    CHECK(apply_launch_argument(settings, L"--language=zh-CN"));
    CHECK(settings.themeMode == ThemeMode::Dark);
    CHECK(settings.language == Language::SimplifiedChinese);

    CHECK(apply_launch_argument(settings, L"--directory-transitions=system"));
    CHECK(settings.directoryTransitions == DirectoryTransitionMode::FollowSystem);
    CHECK(apply_launch_argument(settings, L"--directory-transitions=off"));
    CHECK(settings.directoryTransitions == DirectoryTransitionMode::Off);
    CHECK(apply_launch_argument(settings, L"--directory-transitions=always"));
    CHECK(settings.directoryTransitions == DirectoryTransitionMode::AlwaysOn);
}

TEST_CASE(appearance_settings_identify_only_directory_transition_commands) {
    CHECK(is_directory_transition_command(
        SettingsCommand::DirectoryTransitionsAlwaysOn));
    CHECK(is_directory_transition_command(
        SettingsCommand::DirectoryTransitionsFollowSystem));
    CHECK(is_directory_transition_command(SettingsCommand::DirectoryTransitionsOff));
    CHECK(!is_directory_transition_command(SettingsCommand::ThemeDark));
    CHECK(!is_directory_transition_command(SettingsCommand::LanguageEnglish));
    CHECK(!is_directory_transition_command(static_cast<SettingsCommand>(9999)));
}

TEST_CASE(appearance_settings_ignore_unrecognized_launch_argument) {
    AppearanceSettings settings{ThemeMode::System, Language::English};
    CHECK(!apply_launch_argument(settings, L"--theme=purple"));
    CHECK(!apply_launch_argument(settings, L"--directory-transitions=slow"));
    CHECK(settings.themeMode == ThemeMode::System);
    CHECK(settings.language == Language::English);
    CHECK(settings.directoryTransitions == DirectoryTransitionMode::AlwaysOn);
}
