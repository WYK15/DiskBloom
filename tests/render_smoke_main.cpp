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
        64,
        64,
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

    const auto theme = diskbloom::core::make_theme(true);
    if (!graphics.begin_draw(theme.window)) {
        DestroyWindow(window);
        return 3;
    }

    const auto result = graphics.end_draw();
    DestroyWindow(window);
    return SUCCEEDED(result) ? 0 : 4;
}
