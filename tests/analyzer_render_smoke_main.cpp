#include "app/analyzer_view.h"
#include "app/review_collector_interaction.h"
#include "core/scan_tree.h"
#include "core/scan_tree_exclusion.h"
#include "core/theme.h"
#include "render/graphics_device.h"

#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

[[nodiscard]] diskbloom::render::CapturedFrame crop_bottom(
    const diskbloom::render::CapturedFrame& source,
    const std::uint32_t height) {
    diskbloom::render::CapturedFrame cropped;
    cropped.width = source.width;
    cropped.height = std::min(height, source.height);
    cropped.rowPitch = source.rowPitch;
    const auto firstByte = static_cast<std::size_t>(
        source.height - cropped.height) * source.rowPitch;
    cropped.pixels.assign(source.pixels.begin() + firstByte, source.pixels.end());
    return cropped;
}

[[nodiscard]] bool save_png(
    const diskbloom::render::CapturedFrame& capture,
    const std::filesystem::path& path) {
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&factory)))) {
        return false;
    }
    Microsoft::WRL::ComPtr<IWICStream> stream;
    Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
    Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
    if (FAILED(factory->CreateStream(&stream))
        || FAILED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE))
        || FAILED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder))
        || FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache))
        || FAILED(encoder->CreateNewFrame(&frame, nullptr))
        || FAILED(frame->Initialize(nullptr))
        || FAILED(frame->SetSize(capture.width, capture.height))) {
        return false;
    }
    auto format = GUID_WICPixelFormat32bppBGRA;
    if (FAILED(frame->SetPixelFormat(&format))
        || format != GUID_WICPixelFormat32bppBGRA
        || FAILED(frame->WritePixels(
            capture.height,
            capture.rowPitch,
            static_cast<UINT>(capture.pixels.size()),
            reinterpret_cast<BYTE*>(const_cast<std::byte*>(capture.pixels.data()))))
        || FAILED(frame->Commit())
        || FAILED(encoder->Commit())) {
        return false;
    }
    return true;
}

} // namespace

int main(const int argc, char** argv) {
    std::filesystem::path captureDirectory;
    for (int index = 1; index < argc; ++index) {
        constexpr std::string_view prefix = "--capture-dir=";
        const std::string_view argument(argv[index]);
        if (argument.starts_with(prefix)) {
            captureDirectory = std::filesystem::path(
                std::string(argument.substr(prefix.size())));
        }
    }
    if (!captureDirectory.empty()) {
        std::error_code error;
        std::filesystem::create_directories(captureDirectory, error);
        if (error) {
            return 72;
        }
    }
    const auto comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const auto uninitializeCom = SUCCEEDED(comResult);
    const auto window = CreateWindowExW(
        0U,
        L"STATIC",
        L"DiskBloom analyzer render smoke test",
        WS_POPUP,
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
    (void)tree.add_child(
        root,
        L"other.bin",
        96U * 1024U,
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
    diskbloom::app::AnalyzerBreadcrumbModel breadcrumb;
    breadcrumb.items = {
        {.kind = diskbloom::app::BreadcrumbItemKind::Overview, .enabled = true},
        {
            .kind = diskbloom::app::BreadcrumbItemKind::UnscannedPrefix,
            .label = L"D:\\",
            .absolutePath = L"D:\\",
        },
        {
            .kind = diskbloom::app::BreadcrumbItemKind::UnscannedPrefix,
            .label = L"workspace-with-a-long-name",
            .absolutePath = L"D:\\workspace-with-a-long-name",
        },
        {
            .kind = diskbloom::app::BreadcrumbItemKind::ScanNode,
            .node = root,
            .label = L"project-with-a-long-name",
            .absolutePath = L"D:\\workspace-with-a-long-name\\project-with-a-long-name",
            .enabled = true,
        },
        {
            .kind = diskbloom::app::BreadcrumbItemKind::ScanNode,
            .node = folder,
            .label = L"source-with-a-long-name",
            .absolutePath = L"D:\\workspace-with-a-long-name\\project-with-a-long-name\\source",
            .enabled = true,
        },
        {
            .kind = diskbloom::app::BreadcrumbItemKind::ScanNode,
            .node = root,
            .label = L"app-with-a-long-name",
            .absolutePath = L"D:\\workspace-with-a-long-name\\project-with-a-long-name\\source\\app",
            .enabled = true,
        },
        {
            .kind = diskbloom::app::BreadcrumbItemKind::ScanNode,
            .node = folder,
            .label = L"assets-with-a-long-name",
            .absolutePath = L"D:\\workspace-with-a-long-name\\project-with-a-long-name\\source\\app\\assets",
            .enabled = true,
        },
    };
    analyzer.set_breadcrumb(std::move(breadcrumb));
    analyzer.set_history_availability(true, false);
    analyzer.set_review_summary(reviewNodes.size(), tree.node(root).logicalSize);
    analyzer.set_review_nodes(reviewNodes);
    const auto lightTheme = diskbloom::core::make_theme(false);
    constexpr bool theme_modes[]{false, true};
    constexpr diskbloom::core::Language languages[]{
        diskbloom::core::Language::English,
        diskbloom::core::Language::SimplifiedChinese,
    };
    const auto drawAnalyzer = [&](const diskbloom::core::ThemeTokens& theme,
                                  const diskbloom::core::Language language,
                                  diskbloom::render::CapturedFrame* capture = nullptr) {
        return graphics.begin_draw(theme.window)
            && analyzer.draw(graphics, theme, language, 800.0F, 600.0F)
            && SUCCEEDED(graphics.end_draw(capture));
    };
    if (!drawAnalyzer(lightTheme, diskbloom::core::Language::English)) {
        DestroyWindow(window);
        return 3;
    }
    if (!captureDirectory.empty()) {
        diskbloom::render::CapturedFrame capture;
        if (!drawAnalyzer(lightTheme, diskbloom::core::Language::English, &capture)
            || !save_png(capture, captureDirectory / L"breadcrumb-compact-light-en.png")) {
            DestroyWindow(window);
            return 73;
        }
    }

    analyzer.pointer_pressed(32.0F, 32.0F);
    auto navigationCommand = analyzer.take_command();
    if (!navigationCommand.has_value()
        || navigationCommand->kind != diskbloom::app::AnalyzerCommandKind::NavigateBack) {
        DestroyWindow(window);
        return 52;
    }
    analyzer.pointer_pressed(76.0F, 32.0F);
    if (analyzer.take_command().has_value()) {
        DestroyWindow(window);
        return 53;
    }
    analyzer.set_history_availability(true, true);
    analyzer.pointer_pressed(76.0F, 32.0F);
    navigationCommand = analyzer.take_command();
    if (!navigationCommand.has_value()
        || navigationCommand->kind != diskbloom::app::AnalyzerCommandKind::NavigateForward) {
        DestroyWindow(window);
        return 54;
    }
    analyzer.pointer_pressed(110.0F, 32.0F);
    navigationCommand = analyzer.take_command();
    if (!navigationCommand.has_value()
        || navigationCommand->kind != diskbloom::app::AnalyzerCommandKind::ReturnToOverview) {
        DestroyWindow(window);
        return 55;
    }

    auto ellipsisX = -1.0F;
    for (auto x = 108.0F; x < 650.0F; x += 1.0F) {
        analyzer.pointer_pressed(x, 32.0F);
        const auto command = analyzer.take_command();
        if (command.has_value()
            && command->kind == diskbloom::app::AnalyzerCommandKind::OpenBreadcrumbOverflow) {
            ellipsisX = x;
            break;
        }
    }
    if (ellipsisX < 0.0F
        || !drawAnalyzer(lightTheme, diskbloom::core::Language::English)) {
        DestroyWindow(window);
        return 56;
    }
    if (!captureDirectory.empty()) {
        diskbloom::render::CapturedFrame capture;
        if (!drawAnalyzer(
                diskbloom::core::make_theme(true),
                diskbloom::core::Language::SimplifiedChinese,
                &capture)
            || !save_png(capture, captureDirectory / L"breadcrumb-overflow-dark-zh.png")) {
            DestroyWindow(window);
            return 74;
        }
    }
    (void)analyzer.pointer_moved(ellipsisX + 4.0F, 79.0F);
    if (analyzer.hovered_breadcrumb_path() != L"D:\\") {
        DestroyWindow(window);
        return 57;
    }
    analyzer.pointer_pressed(ellipsisX + 4.0F, 79.0F);
    if (analyzer.take_command().has_value()) {
        DestroyWindow(window);
        return 58;
    }
    analyzer.pointer_pressed(ellipsisX + 4.0F, 147.0F);
    navigationCommand = analyzer.take_command();
    if (!navigationCommand.has_value()
        || navigationCommand->kind != diskbloom::app::AnalyzerCommandKind::NavigateBreadcrumb
        || navigationCommand->node != folder) {
        DestroyWindow(window);
        return 59;
    }

    const auto analyzerLayout = diskbloom::app::compute_analyzer_layout(800.0F, 600.0F, 2U);
    const auto childLayout = diskbloom::app::compute_analyzer_child_list_layout(
        analyzerLayout.detailsBounds, 3U, 0U);
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

    analyzer.set_hover_animations_enabled(true);
    if (!analyzer.pointer_moved(childRowX, childRowY)
        || !analyzer.hover_pulse_active()
        || !analyzer.hover_pulse_timer_required()) {
        DestroyWindow(window);
        return 80;
    }
    if (!captureDirectory.empty()) {
        Sleep(450U);
        diskbloom::render::CapturedFrame darkHover;
        diskbloom::render::CapturedFrame lightHover;
        if (!drawAnalyzer(
                diskbloom::core::make_theme(true),
                diskbloom::core::Language::English,
                &darkHover)
            || !save_png(
                darkHover,
                captureDirectory / L"hover-pulse-dark-en.png")
            || !drawAnalyzer(
                lightTheme,
                diskbloom::core::Language::SimplifiedChinese,
                &lightHover)
            || !save_png(
                lightHover,
                captureDirectory / L"hover-pulse-light-zh.png")) {
            DestroyWindow(window);
            return 83;
        }

        (void)analyzer.pointer_left();
        analyzer.set_review_summary(0U, 0U);
        analyzer.set_review_nodes(std::span<const diskbloom::core::NodeIndex>{});
        diskbloom::render::CapturedFrame collectorHint;
        if (!graphics.resize(1200U, 720U)
            || !graphics.begin_draw(diskbloom::core::make_theme(true).window)
            || !analyzer.draw(
                graphics,
                diskbloom::core::make_theme(true),
                diskbloom::core::Language::SimplifiedChinese,
                1200.0F,
                720.0F)
            || FAILED(graphics.end_draw(&collectorHint))
            || !save_png(
                crop_bottom(collectorHint, 88U),
                captureDirectory / L"collector-hint-dark-zh.png")
            || !graphics.resize(800U, 600U)
            || !drawAnalyzer(
                diskbloom::core::make_theme(true),
                diskbloom::core::Language::SimplifiedChinese)) {
            DestroyWindow(window);
            return 84;
        }
        analyzer.set_review_summary(reviewNodes.size(), tree.node(root).logicalSize);
        analyzer.set_review_nodes(reviewNodes);
        (void)analyzer.pointer_moved(childRowX, childRowY);
    }
    analyzer.set_hover_animations_enabled(false);
    if (!analyzer.hover_pulse_active()
        || analyzer.hover_pulse_timer_required()
        || !drawAnalyzer(lightTheme, diskbloom::core::Language::English)) {
        DestroyWindow(window);
        return 81;
    }
    if (!analyzer.pointer_left() || analyzer.hover_pulse_active()) {
        DestroyWindow(window);
        return 82;
    }
    analyzer.set_hover_animations_enabled(true);

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
        [folder](const diskbloom::core::SunburstSegment& value) {
            return value.node == folder
                && !diskbloom::core::has_flag(
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
    analyzer.set_breadcrumb(diskbloom::app::build_analyzer_breadcrumb(
        tree,
        root,
        folder,
        L"D:\\workspace\\root"));

    const auto transitionStart = std::chrono::steady_clock::time_point{};
    analyzer.set_history_availability(true, true);
    if (!analyzer.navigate_to_root(root, transitionStart, true)
        || !analyzer.transition_active()
        || analyzer.current_root() != root) {
        DestroyWindow(window);
        return 60;
    }
    analyzer.set_breadcrumb(diskbloom::app::build_analyzer_breadcrumb(
        tree,
        root,
        root,
        L"D:\\workspace\\root"));
    if (!captureDirectory.empty()) {
        diskbloom::render::CapturedFrame capture;
        if (!drawAnalyzer(lightTheme, diskbloom::core::Language::English, &capture)
            || !save_png(capture, captureDirectory / L"transition-000ms-light-en.png")) {
            DestroyWindow(window);
            return 75;
        }
    }
    analyzer.pointer_pressed(analyzerLayout.previewButton.left + 4.0F,
                             analyzerLayout.previewButton.top + 4.0F);
    if (analyzer.take_command().has_value()
        || analyzer.scroll_at(790.0F, 100.0F, 1)) {
        DestroyWindow(window);
        return 61;
    }
    analyzer.pointer_pressed(
        (analyzerLayout.minimizeButton.left + analyzerLayout.minimizeButton.right) * 0.5F,
        (analyzerLayout.minimizeButton.top + analyzerLayout.minimizeButton.bottom) * 0.5F);
    navigationCommand = analyzer.take_command();
    if (!navigationCommand.has_value()
        || navigationCommand->kind != diskbloom::app::AnalyzerCommandKind::MinimizeWindow) {
        DestroyWindow(window);
        return 71;
    }
    analyzer.pointer_pressed(32.0F, 32.0F);
    navigationCommand = analyzer.take_command();
    if (!navigationCommand.has_value()
        || navigationCommand->kind != diskbloom::app::AnalyzerCommandKind::NavigateBack) {
        DestroyWindow(window);
        return 62;
    }
    for (const auto dark : theme_modes) {
        const auto theme = diskbloom::core::make_theme(dark);
        for (const auto language : languages) {
            if (!drawAnalyzer(theme, language)) {
                DestroyWindow(window);
                return 63;
            }
        }
    }
    if (!analyzer.advance_transition(transitionStart + std::chrono::milliseconds{350})
        || !analyzer.transition_active()) {
        DestroyWindow(window);
        return 64;
    }
    for (const auto dark : theme_modes) {
        const auto theme = diskbloom::core::make_theme(dark);
        for (const auto language : languages) {
            if (!drawAnalyzer(theme, language)) {
                DestroyWindow(window);
                return 68;
            }
        }
    }
    if (!captureDirectory.empty()) {
        diskbloom::render::CapturedFrame capture;
        if (!drawAnalyzer(lightTheme, diskbloom::core::Language::English, &capture)
            || !save_png(capture, captureDirectory / L"transition-350ms-light-en.png")) {
            DestroyWindow(window);
            return 76;
        }
    }
    const auto interruption = transitionStart + std::chrono::milliseconds{400};
    if (!analyzer.navigate_to_root(folder, interruption, true)
        || !analyzer.transition_active()) {
        DestroyWindow(window);
        return 65;
    }
    analyzer.set_breadcrumb(diskbloom::app::build_analyzer_breadcrumb(
        tree,
        root,
        folder,
        L"D:\\workspace\\root"));
    for (const auto dark : theme_modes) {
        const auto theme = diskbloom::core::make_theme(dark);
        for (const auto language : languages) {
            if (!drawAnalyzer(theme, language)) {
                DestroyWindow(window);
                return 69;
            }
        }
    }
    if (!analyzer.advance_transition(interruption + std::chrono::milliseconds{700})
        || analyzer.transition_active()) {
        DestroyWindow(window);
        return 66;
    }
    for (const auto dark : theme_modes) {
        const auto theme = diskbloom::core::make_theme(dark);
        for (const auto language : languages) {
            if (!drawAnalyzer(theme, language)) {
                DestroyWindow(window);
                return 70;
            }
        }
    }
    if (!captureDirectory.empty()) {
        diskbloom::render::CapturedFrame capture;
        if (!drawAnalyzer(lightTheme, diskbloom::core::Language::English, &capture)
            || !save_png(capture, captureDirectory / L"transition-700ms-light-en.png")) {
            DestroyWindow(window);
            return 77;
        }
    }
    if (!analyzer.navigate_to_root(root, interruption, false)
        || analyzer.transition_active()
        || analyzer.current_root() != root) {
        DestroyWindow(window);
        return 67;
    }

    diskbloom::core::ScanTreeExclusion exclusion;
    const std::array excludedNodes{folder};
    exclusion.rebuild(tree, excludedNodes);
    const std::array reviewedFolder{folder};
    const auto reflowStart = interruption + std::chrono::seconds{2};
    if (!analyzer.reflow_review_change(
            &exclusion, reviewedFolder, root, reflowStart, true)
        || !analyzer.transition_active()
        || analyzer.current_root() != root) {
        DestroyWindow(window);
        return 84;
    }
    if (!analyzer.advance_transition(reflowStart + std::chrono::milliseconds{250})
        || !analyzer.transition_active()) {
        DestroyWindow(window);
        return 85;
    }
    auto reflowCollector = diskbloom::app::compute_review_collector_layout(
        analyzerLayout.actionBar, 800.0F, 600.0F, 1U, 0U);
    reflowCollector.summary.right = std::max(
        reflowCollector.summary.left,
        std::min(reflowCollector.summary.right, analyzerLayout.reviewButton.left - 20.0F));
    const auto reflowSummaryX = (reflowCollector.summary.left + reflowCollector.summary.right) * 0.5F;
    const auto reflowSummaryY = (reflowCollector.summary.top + reflowCollector.summary.bottom) * 0.5F;
    (void)analyzer.pointer_moved(reflowSummaryX, reflowSummaryY);
    const auto restore = reflowCollector.rows.front().restoreBounds;
    const auto restoreX = (restore.left + restore.right) * 0.5F;
    const auto restoreY = (restore.top + restore.bottom) * 0.5F;
    (void)analyzer.pointer_moved(restoreX, restoreY);
    if (analyzer.hovered_review_restore_node() != std::optional{folder}) {
        DestroyWindow(window);
        return 86;
    }
    analyzer.pointer_down(restoreX, restoreY);
    analyzer.pointer_released(restoreX, restoreY);
    const auto restoreCommand = analyzer.take_command();
    if (!restoreCommand.has_value()
        || restoreCommand->kind != diskbloom::app::AnalyzerCommandKind::RestoreReviewItem
        || restoreCommand->node != folder) {
        DestroyWindow(window);
        return 87;
    }
    exclusion.clear();
    if (!analyzer.reflow_review_change(
            &exclusion,
            std::span<const diskbloom::core::NodeIndex>{},
            root,
            reflowStart + std::chrono::milliseconds{250},
            true)
        || !analyzer.transition_active()
        || !analyzer.advance_transition(reflowStart + std::chrono::milliseconds{750})
        || analyzer.transition_active()) {
        DestroyWindow(window);
        return 88;
    }

    DestroyWindow(window);
    if (uninitializeCom) {
        CoUninitialize();
    }
    return 0;
}
