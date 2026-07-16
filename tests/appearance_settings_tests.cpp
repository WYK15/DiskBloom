#include "test_support.h"

#include "app/appearance_settings.h"

using diskbloom::app::AppearanceSettings;
using diskbloom::app::DirectoryTransitionMode;
using diskbloom::app::SettingsCommand;
using diskbloom::app::apply_launch_argument;
using diskbloom::app::apply_settings_command;
using diskbloom::app::is_directory_transition_command;
using diskbloom::app::resolve_directory_transitions;
using diskbloom::core::Language;
using diskbloom::core::ThemeMode;

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

TEST_CASE(appearance_settings_reject_unknown_command) {
    AppearanceSettings settings{ThemeMode::Dark, Language::English};
    CHECK(!apply_settings_command(settings, static_cast<SettingsCommand>(9999)));
    CHECK(settings.themeMode == ThemeMode::Dark);
    CHECK(settings.language == Language::English);
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
