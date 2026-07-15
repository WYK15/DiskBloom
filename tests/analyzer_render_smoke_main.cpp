#include "app/analyzer_view.h"
#include "core/scan_tree.h"
#include "core/theme.h"
#include "render/graphics_device.h"

#include <Windows.h>

int main() {
    const auto window = CreateWindowExW(
        0U,
        L"STATIC",
        L"DiskBloom analyzer render smoke test",
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

    diskbloom::core::ScanTree tree;
    const auto root = tree.add_root(L"root", diskbloom::core::ScanNodeFlags::Directory);
    const auto folder = tree.add_child(
        root,
        L"folder",
        0U,
        diskbloom::core::ScanNodeFlags::Directory);
    (void)tree.add_child(folder, L"one.bin", 60U, diskbloom::core::ScanNodeFlags::None);
    (void)tree.add_child(folder, L"two.bin", 40U, diskbloom::core::ScanNodeFlags::None);
    tree.aggregate();

    diskbloom::app::AnalyzerView analyzer;
    analyzer.set_tree(&tree, root);
    const auto theme = diskbloom::core::make_theme(true);
    if (!graphics.begin_draw(theme.window)
        || !analyzer.draw(
            graphics,
            theme,
            diskbloom::core::Language::English,
            800.0F,
            600.0F)) {
        DestroyWindow(window);
        return 3;
    }

    const auto result = graphics.end_draw();
    DestroyWindow(window);
    return SUCCEEDED(result) ? 0 : 4;
}
