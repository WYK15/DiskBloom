#include "app/main_window.h"

#include <Windows.h>
#include <shellapi.h>

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
