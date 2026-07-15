#include "test_support.h"

#include "app/appearance_settings.h"

using diskbloom::app::AppearanceSettings;
using diskbloom::app::SettingsCommand;
using diskbloom::app::apply_launch_argument;
using diskbloom::app::apply_settings_command;
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
}

TEST_CASE(appearance_settings_ignore_unrecognized_launch_argument) {
    AppearanceSettings settings{ThemeMode::System, Language::English};
    CHECK(!apply_launch_argument(settings, L"--theme=purple"));
    CHECK(settings.themeMode == ThemeMode::System);
    CHECK(settings.language == Language::English);
}
