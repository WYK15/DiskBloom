#include "app/main_window.h"
#include "platform/windows/settings_store.h"

#include <Windows.h>
#include <shellapi.h>

#include <filesystem>
#include <system_error>

int WINAPI wWinMain(
    const HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    const int showCommand) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    diskbloom::app::AppearanceSettings appearance{
        diskbloom::core::ThemeMode::System,
        PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_CHINESE
            ? diskbloom::core::Language::SimplifiedChinese
            : diskbloom::core::Language::English,
    };
    const auto settingsPath = diskbloom::platform::windows::default_settings_path();
    if (const auto saved = diskbloom::platform::windows::load_settings(
            settingsPath,
            appearance)) {
        appearance = *saved;
    } else {
        std::error_code existsError;
        const auto v2Exists = std::filesystem::exists(settingsPath, existsError);
        if (!v2Exists && !existsError) {
            if (const auto legacy =
                    diskbloom::platform::windows::load_legacy_directory_transition_mode(
                        diskbloom::platform::windows::legacy_settings_path())) {
                appearance.directoryTransitions = *legacy;
            }
        }
    }
    bool smoke_test = false;
    int argument_count = 0;
    auto** arguments = CommandLineToArgvW(GetCommandLineW(), &argument_count);
    if (arguments != nullptr) {
        for (int index = 1; index < argument_count; ++index) {
            const std::wstring_view argument(arguments[index]);
            smoke_test = smoke_test || argument == L"--smoke-test";
            const auto recognized = diskbloom::app::apply_launch_argument(appearance, argument);
            (void)recognized;
        }
        LocalFree(arguments);
    }

    return diskbloom::app::MainWindow::run(
        instance,
        showCommand,
        smoke_test,
        appearance);
}
