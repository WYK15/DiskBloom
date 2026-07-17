#include "app/appearance_settings.h"
#include "app/disk_overview.h"
#include "core/theme.h"
#include "render/graphics_device.h"

#include <Windows.h>

int main() {
    const auto window = CreateWindowExW(
        0U,
        L"STATIC",
        L"DiskBloom render smoke test",
        WS_OVERLAPPED,
        0,
        0,
        800,
        600,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);
    if (window == nullptr) {
        return 1;
    }

    diskbloom::render::GraphicsDevice graphics;
    if (!graphics.initialize(window)) {
        DestroyWindow(window);
        return 2;
    }

    diskbloom::app::DiskOverview overview;
    const diskbloom::app::TypographySettings smallSegoe{
        .textScale = diskbloom::app::TextScalePreset::Percent80,
        .fontFamily = diskbloom::app::FontFamilyPreset::SegoeUiVariable,
    };
    const diskbloom::app::TypographySettings largeConsolas{
        .textScale = diskbloom::app::TextScalePreset::Percent120,
        .fontFamily = diskbloom::app::FontFamilyPreset::Consolas,
    };
    constexpr bool darkModes[]{false, true};
    constexpr diskbloom::core::Language languages[]{
        diskbloom::core::Language::English,
        diskbloom::core::Language::SimplifiedChinese,
    };
    const diskbloom::app::TypographySettings typographyCases[]{smallSegoe, largeConsolas};
    for (const auto dark : darkModes) {
        const auto theme = diskbloom::core::make_theme(dark);
        for (const auto language : languages) {
            for (const auto& typography : typographyCases) {
                if (!graphics.begin_draw(theme.window)
                    || !overview.draw(
                        graphics,
                        theme,
                        language,
                        typography,
                        800.0F,
                        600.0F)
                    || FAILED(graphics.end_draw())) {
                    DestroyWindow(window);
                    return 3;
                }
            }
        }
    }
    DestroyWindow(window);
    return 0;
}
