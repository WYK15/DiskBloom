#include "app/analyzer_view.h"
#include "app/review_collector_interaction.h"
#include "core/scan_tree.h"
#include "core/theme.h"
#include "render/graphics_device.h"

#include <Windows.h>

#include <algorithm>
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
    const auto lightTheme = diskbloom::core::make_theme(false);
    const auto drawAnalyzer = [&](const diskbloom::core::ThemeTokens& theme,
                                  const diskbloom::core::Language language) {
        return graphics.begin_draw(theme.window)
            && analyzer.draw(graphics, theme, language, 800.0F, 600.0F)
            && SUCCEEDED(graphics.end_draw());
    };
    if (!drawAnalyzer(lightTheme, diskbloom::core::Language::English)) {
        DestroyWindow(window);
        return 3;
    }

    const auto analyzerLayout = diskbloom::app::compute_analyzer_layout(800.0F, 600.0F, 1U);
    auto collector = diskbloom::app::compute_review_collector_layout(
        analyzerLayout.actionBar,
        800.0F,
        600.0F,
        reviewNodes.size(),
        0U);
    collector.summary.right = std::max(
        collector.summary.left,
        std::min(collector.summary.right, analyzerLayout.reviewButton.left - 20.0F));
    if (collector.rows.empty()) {
        DestroyWindow(window);
        return 4;
    }
    const auto summaryX = (collector.summary.left + collector.summary.right) * 0.5F;
    const auto summaryY = (collector.summary.top + collector.summary.bottom) * 0.5F;
    const auto rowX = (collector.rows.front().bounds.left
        + collector.rows.front().bounds.right) * 0.5F;
    const auto rowY = (collector.rows.front().bounds.top
        + collector.rows.front().bounds.bottom) * 0.5F;

    if (!analyzer.pointer_moved(summaryX, summaryY)) {
        DestroyWindow(window);
        return 5;
    }
    if (!drawAnalyzer(lightTheme, diskbloom::core::Language::English)) {
        DestroyWindow(window);
        return 6;
    }
    (void)analyzer.pointer_moved(rowX, rowY);
    if (!analyzer.scroll_review(1)) {
        DestroyWindow(window);
        return 7;
    }

    constexpr bool theme_modes[]{false, true};
    constexpr diskbloom::core::Language languages[]{
        diskbloom::core::Language::English,
        diskbloom::core::Language::SimplifiedChinese,
    };
    for (const auto dark : theme_modes) {
        const auto theme = diskbloom::core::make_theme(dark);
        for (const auto language : languages) {
            if (!drawAnalyzer(theme, language)) {
                DestroyWindow(window);
                return 8;
            }
        }
    }

    analyzer.pointer_pressed(rowX, rowY);
    if (analyzer.take_command().has_value()) {
        DestroyWindow(window);
        return 9;
    }

    (void)analyzer.pointer_moved(790.0F, 100.0F);
    if (analyzer.scroll_review(1)) {
        DestroyWindow(window);
        return 10;
    }

    (void)analyzer.pointer_moved(summaryX, summaryY);
    if (!analyzer.pointer_left()) {
        DestroyWindow(window);
        return 11;
    }
    if (analyzer.scroll_review(1)) {
        DestroyWindow(window);
        return 12;
    }

    (void)analyzer.pointer_moved(summaryX, summaryY);
    if (!analyzer.scroll_review(1)) {
        DestroyWindow(window);
        return 21;
    }
    analyzer.set_review_nodes({});
    if (analyzer.scroll_review(1)) {
        DestroyWindow(window);
        return 13;
    }

    analyzer.set_review_nodes(reviewNodes);
    if (!drawAnalyzer(lightTheme, diskbloom::core::Language::English)) {
        DestroyWindow(window);
        return 14;
    }
    (void)analyzer.pointer_moved(summaryX, summaryY);
    if (!analyzer.scroll_review(1)) {
        DestroyWindow(window);
        return 22;
    }
    analyzer.set_recycle_in_progress(true);
    if (analyzer.scroll_review(1)) {
        DestroyWindow(window);
        return 15;
    }
    (void)analyzer.pointer_moved(summaryX, summaryY);
    if (analyzer.scroll_review(1)) {
        DestroyWindow(window);
        return 16;
    }
    analyzer.set_recycle_in_progress(false);

    (void)analyzer.pointer_moved(summaryX, summaryY);
    if (!analyzer.scroll_review(1)) {
        DestroyWindow(window);
        return 17;
    }
    analyzer.set_review_summary(1U, tree.node(reviewNodes.front()).logicalSize);
    analyzer.set_review_nodes(std::span<const diskbloom::core::NodeIndex>(
        reviewNodes.data(),
        1U));
    if (!drawAnalyzer(lightTheme, diskbloom::core::Language::English)) {
        DestroyWindow(window);
        return 18;
    }
    if (analyzer.scroll_review(1)) {
        DestroyWindow(window);
        return 19;
    }

    auto invalidReviewNodes = reviewNodes;
    invalidReviewNodes[1U] = diskbloom::core::invalid_node;
    invalidReviewNodes[2U] = static_cast<diskbloom::core::NodeIndex>(
        tree.nodes().size() + 100U);
    analyzer.set_review_summary(invalidReviewNodes.size(), 0U);
    analyzer.set_review_nodes(invalidReviewNodes);
    (void)analyzer.pointer_moved(790.0F, 100.0F);
    (void)analyzer.pointer_moved(summaryX, summaryY);
    if (!analyzer.scroll_review(1)) {
        DestroyWindow(window);
        return 23;
    }
    for (const auto dark : theme_modes) {
        const auto theme = diskbloom::core::make_theme(dark);
        for (const auto language : languages) {
            if (!drawAnalyzer(theme, language)) {
                DestroyWindow(window);
                return 20;
            }
        }
    }

    DestroyWindow(window);
    return 0;
}
