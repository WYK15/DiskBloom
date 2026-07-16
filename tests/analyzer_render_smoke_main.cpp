#include "app/analyzer_view.h"
#include "core/scan_tree.h"
#include "core/theme.h"
#include "render/graphics_device.h"

#include <Windows.h>

#include <vector>

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
    std::vector<diskbloom::core::NodeIndex> reviewNodes;
    reviewNodes.reserve(20U);
    for (std::size_t index = 0U; index < 20U; ++index) {
        reviewNodes.push_back(tree.add_child(
            root,
            L"reviewed-file-with-a-long-name.bin",
            1024U * (index + 1U),
            diskbloom::core::ScanNodeFlags::None));
    }
    tree.aggregate();

    diskbloom::app::AnalyzerView analyzer;
    analyzer.set_tree(&tree, root);
    analyzer.set_review_summary(reviewNodes.size(), tree.node(root).logicalSize);
    analyzer.set_review_nodes(reviewNodes);
    constexpr bool theme_modes[]{false, true};
    constexpr diskbloom::core::Language languages[]{
        diskbloom::core::Language::English,
        diskbloom::core::Language::SimplifiedChinese,
    };
    for (const auto dark : theme_modes) {
        const auto theme = diskbloom::core::make_theme(dark);
        for (const auto language : languages) {
            if (!graphics.begin_draw(theme.window)
                || !analyzer.draw(graphics, theme, language, 800.0F, 600.0F)
                || FAILED(graphics.end_draw())) {
                DestroyWindow(window);
                return 3;
            }
            (void)analyzer.pointer_moved(28.0F, 570.0F);
            if (!graphics.begin_draw(theme.window)
                || !analyzer.draw(graphics, theme, language, 800.0F, 600.0F)
                || FAILED(graphics.end_draw())) {
                DestroyWindow(window);
                return 4;
            }
            if (!analyzer.scroll_review(1)) {
                DestroyWindow(window);
                return 5;
            }
        }
    }

    DestroyWindow(window);
    return 0;
}
