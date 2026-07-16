#include "app/analyzer_view.h"
#include "app/review_collector_interaction.h"
#include "core/scan_tree.h"
#include "core/theme.h"
#include "render/graphics_device.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>
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
    const auto folder = tree.add_child(
        root,
        L"folder",
        0U,
        diskbloom::core::ScanNodeFlags::Directory);
    (void)tree.add_child(
        root,
        L"tiny.bin",
        1U,
        diskbloom::core::ScanNodeFlags::None);
    std::vector<diskbloom::core::NodeIndex> reviewNodes;
    reviewNodes.reserve(20U);
    for (std::size_t index = 0U; index < 20U; ++index) {
        reviewNodes.push_back(tree.add_child(
            folder,
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
    constexpr bool theme_modes[]{false, true};
    constexpr diskbloom::core::Language languages[]{
        diskbloom::core::Language::English,
        diskbloom::core::Language::SimplifiedChinese,
    };
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

    const auto analyzerLayout = diskbloom::app::compute_analyzer_layout(800.0F, 600.0F, 2U);
    const auto childLayout = diskbloom::app::compute_analyzer_child_list_layout(
        analyzerLayout.detailsBounds, 2U, 0U);
    const auto& childRow = childLayout.rows.front().bounds;
    const auto childRowX = (childRow.left + childRow.right) * 0.5F;
    const auto childRowY = (childRow.top + childRow.bottom) * 0.5F;
    auto dropCollector = diskbloom::app::compute_review_collector_layout(
        analyzerLayout.actionBar, 800.0F, 600.0F, 1U, 0U).summary;
    dropCollector.right = std::max(
        dropCollector.left,
        std::min(dropCollector.right, analyzerLayout.reviewButton.left - 20.0F));
    const auto collectorX = (dropCollector.left + dropCollector.right) * 0.5F;
    const auto collectorY = (dropCollector.top + dropCollector.bottom) * 0.5F;

    analyzer.pointer_down(childRowX, childRowY);
    if (!analyzer.drag_pending() || analyzer.drag_active()
        || analyzer.take_command().has_value()) {
        DestroyWindow(window);
        return 4;
    }
    (void)analyzer.pointer_moved(collectorX, collectorY);
    if (analyzer.drag_pending() || !analyzer.drag_active()) {
        DestroyWindow(window);
        return 12;
    }
    for (const auto dark : theme_modes) {
        const auto theme = diskbloom::core::make_theme(dark);
        for (const auto language : languages) {
            if (!drawAnalyzer(theme, language)) {
                DestroyWindow(window);
                return 16;
            }
        }
    }
    analyzer.pointer_released(collectorX, collectorY);
    const auto rowDrop = analyzer.take_command();
    if (!rowDrop.has_value()
        || rowDrop->kind != diskbloom::app::AnalyzerCommandKind::AddToReview
        || rowDrop->node != folder) {
        DestroyWindow(window);
        return 5;
    }
    if (analyzer.drag_pending() || analyzer.drag_active()) {
        DestroyWindow(window);
        return 13;
    }

    const auto sunburst = diskbloom::core::build_sunburst_layout(tree, root, {});
    const auto segment = std::find_if(
        sunburst.segments.begin(),
        sunburst.segments.end(),
        [](const diskbloom::core::SunburstSegment& value) {
            return !diskbloom::core::has_flag(
                value.flags,
                diskbloom::core::SunburstSegmentFlags::Aggregate);
        });
    if (segment == sunburst.segments.end()) {
        DestroyWindow(window);
        return 6;
    }
    const auto segmentAngle = (segment->startAngle + segment->endAngle) * 0.5F;
    const auto segmentRadius = analyzerLayout.chartGeometry.innerRadius
        + analyzerLayout.chartGeometry.ringWidth
            * (static_cast<float>(segment->depth) + 0.5F);
    const auto segmentX = analyzerLayout.chartGeometry.centerX
        + std::cos(segmentAngle) * segmentRadius;
    const auto segmentY = analyzerLayout.chartGeometry.centerY
        + std::sin(segmentAngle) * segmentRadius;

    (void)analyzer.pointer_left();
    analyzer.pointer_down(segmentX, segmentY);
    (void)analyzer.pointer_moved(collectorX, collectorY);
    analyzer.pointer_released(collectorX, collectorY);
    const auto chartDrop = analyzer.take_command();
    if (!chartDrop.has_value()
        || chartDrop->kind != diskbloom::app::AnalyzerCommandKind::AddToReview
        || chartDrop->node != segment->node) {
        DestroyWindow(window);
        return 7;
    }

    (void)analyzer.pointer_left();
    analyzer.pointer_down(segmentX, segmentY);
    analyzer.pointer_released(segmentX + 1.0F, segmentY + 1.0F);
    const auto belowThresholdClick = analyzer.take_command();
    if (!belowThresholdClick.has_value()
        || belowThresholdClick->kind != diskbloom::app::AnalyzerCommandKind::NavigateToNode
        || belowThresholdClick->node != folder) {
        DestroyWindow(window);
        return 8;
    }

    analyzer.pointer_down(childRowX, childRowY);
    (void)analyzer.pointer_moved(790.0F, 100.0F);
    if (!analyzer.drag_active()
        || !drawAnalyzer(lightTheme, diskbloom::core::Language::English)) {
        DestroyWindow(window);
        return 14;
    }
    analyzer.pointer_released(790.0F, 100.0F);
    if (analyzer.take_command().has_value()) {
        DestroyWindow(window);
        return 9;
    }

    analyzer.pointer_down(
        analyzerLayout.chartGeometry.centerX,
        analyzerLayout.chartGeometry.centerY);
    if (analyzer.drag_pending() || analyzer.drag_active()) {
        DestroyWindow(window);
        return 10;
    }
    analyzer.pointer_released(
        analyzerLayout.chartGeometry.centerX,
        analyzerLayout.chartGeometry.centerY);
    (void)analyzer.take_command();

    analyzer.set_recycle_in_progress(true);
    analyzer.pointer_down(childRowX, childRowY);
    if (analyzer.drag_pending() || analyzer.drag_active()) {
        DestroyWindow(window);
        return 15;
    }
    (void)analyzer.pointer_moved(collectorX, collectorY);
    analyzer.pointer_released(collectorX, collectorY);
    if (analyzer.take_command().has_value()) {
        DestroyWindow(window);
        return 11;
    }
    analyzer.set_recycle_in_progress(false);

    (void)analyzer.pointer_left();
    analyzer.pointer_down(childRowX, childRowY);
    analyzer.pointer_released(collectorX, collectorY);
    const auto releaseActivatedDrop = analyzer.take_command();
    if (!releaseActivatedDrop.has_value()
        || releaseActivatedDrop->kind
            != diskbloom::app::AnalyzerCommandKind::AddToReview
        || releaseActivatedDrop->node != folder) {
        DestroyWindow(window);
        return 44;
    }

    const auto aggregate = std::find_if(
        sunburst.segments.begin(),
        sunburst.segments.end(),
        [](const diskbloom::core::SunburstSegment& value) {
            return diskbloom::core::has_flag(
                value.flags,
                diskbloom::core::SunburstSegmentFlags::Aggregate);
        });
    if (aggregate == sunburst.segments.end()) {
        DestroyWindow(window);
        return 45;
    }
    const auto aggregateAngle = (aggregate->startAngle + aggregate->endAngle) * 0.5F;
    const auto aggregateRadius = analyzerLayout.chartGeometry.innerRadius
        + analyzerLayout.chartGeometry.ringWidth
            * (static_cast<float>(aggregate->depth) + 0.5F);
    const auto aggregateX = analyzerLayout.chartGeometry.centerX
        + std::cos(aggregateAngle) * aggregateRadius;
    const auto aggregateY = analyzerLayout.chartGeometry.centerY
        + std::sin(aggregateAngle) * aggregateRadius;
    analyzer.pointer_down(aggregateX, aggregateY);
    if (analyzer.drag_pending() || analyzer.drag_active()) {
        DestroyWindow(window);
        return 46;
    }
    analyzer.pointer_released(aggregateX, aggregateY);
    if (analyzer.take_command().has_value()) {
        DestroyWindow(window);
        return 47;
    }

    (void)analyzer.pointer_moved(collectorX, collectorY);
    analyzer.pointer_down(600.0F, childRowY);
    if (analyzer.drag_pending() || analyzer.drag_active()) {
        DestroyWindow(window);
        return 48;
    }

    (void)analyzer.pointer_left();
    analyzer.pointer_down(childRowX, childRowY);
    if (!analyzer.drag_pending() || !analyzer.cancel_drag()
        || analyzer.drag_pending() || analyzer.drag_active()) {
        DestroyWindow(window);
        return 49;
    }

    analyzer.pointer_down(childRowX, childRowY);
    (void)analyzer.pointer_moved(790.0F, 100.0F);
    if (!analyzer.drag_active() || !analyzer.pointer_left()
        || analyzer.drag_pending() || analyzer.drag_active()) {
        DestroyWindow(window);
        return 50;
    }

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
        return 24;
    }
    const auto summaryX = (collector.summary.left + collector.summary.right) * 0.5F;
    const auto summaryY = (collector.summary.top + collector.summary.bottom) * 0.5F;
    const auto rowX = (collector.rows.front().bounds.left
        + collector.rows.front().bounds.right) * 0.5F;
    const auto rowY = (collector.rows.front().bounds.top
        + collector.rows.front().bounds.bottom) * 0.5F;

    if (!analyzer.pointer_moved(summaryX, summaryY)) {
        DestroyWindow(window);
        return 25;
    }
    if (!drawAnalyzer(lightTheme, diskbloom::core::Language::English)) {
        DestroyWindow(window);
        return 26;
    }
    (void)analyzer.pointer_moved(rowX, rowY);
    if (!analyzer.scroll_at(rowX, rowY, 1)) {
        DestroyWindow(window);
        return 27;
    }

    for (const auto dark : theme_modes) {
        const auto theme = diskbloom::core::make_theme(dark);
        for (const auto language : languages) {
            if (!drawAnalyzer(theme, language)) {
                DestroyWindow(window);
                return 28;
            }
        }
    }

    analyzer.pointer_pressed(rowX, rowY);
    if (analyzer.take_command().has_value()) {
        DestroyWindow(window);
        return 29;
    }

    (void)analyzer.pointer_moved(790.0F, 100.0F);
    if (analyzer.scroll_review(1)) {
        DestroyWindow(window);
        return 30;
    }

    (void)analyzer.pointer_moved(summaryX, summaryY);
    if (!analyzer.pointer_left()) {
        DestroyWindow(window);
        return 31;
    }
    if (analyzer.scroll_review(1)) {
        DestroyWindow(window);
        return 32;
    }

    (void)analyzer.pointer_moved(summaryX, summaryY);
    if (!analyzer.scroll_review(1)) {
        DestroyWindow(window);
        return 41;
    }
    analyzer.set_review_nodes({});
    if (analyzer.scroll_review(1)) {
        DestroyWindow(window);
        return 33;
    }

    analyzer.set_review_nodes(reviewNodes);
    if (!drawAnalyzer(lightTheme, diskbloom::core::Language::English)) {
        DestroyWindow(window);
        return 34;
    }
    (void)analyzer.pointer_moved(summaryX, summaryY);
    if (!analyzer.scroll_review(1)) {
        DestroyWindow(window);
        return 42;
    }
    analyzer.set_recycle_in_progress(true);
    if (analyzer.scroll_review(1)) {
        DestroyWindow(window);
        return 35;
    }
    (void)analyzer.pointer_moved(summaryX, summaryY);
    if (analyzer.scroll_review(1)) {
        DestroyWindow(window);
        return 36;
    }
    analyzer.set_recycle_in_progress(false);

    (void)analyzer.pointer_moved(summaryX, summaryY);
    if (!analyzer.scroll_review(1)) {
        DestroyWindow(window);
        return 37;
    }
    analyzer.set_review_summary(1U, tree.node(reviewNodes.front()).logicalSize);
    analyzer.set_review_nodes(std::span<const diskbloom::core::NodeIndex>(
        reviewNodes.data(),
        1U));
    if (!drawAnalyzer(lightTheme, diskbloom::core::Language::English)) {
        DestroyWindow(window);
        return 38;
    }
    if (analyzer.scroll_review(1)) {
        DestroyWindow(window);
        return 39;
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
        return 43;
    }
    for (const auto dark : theme_modes) {
        const auto theme = diskbloom::core::make_theme(dark);
        for (const auto language : languages) {
            if (!drawAnalyzer(theme, language)) {
                DestroyWindow(window);
                return 40;
            }
        }
    }

    if (!analyzer.set_root(folder)
        || !drawAnalyzer(lightTheme, diskbloom::core::Language::English)
        || !analyzer.scroll_at(790.0F, 100.0F, 1)) {
        DestroyWindow(window);
        return 51;
    }

    DestroyWindow(window);
    return 0;
}
