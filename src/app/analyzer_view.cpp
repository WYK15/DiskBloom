#include "app/analyzer_view.h"

#include "core/string_catalog.h"
#include "render/graphics_device.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>
#include <string>
#include <utility>

#include <d2d1_1helper.h>
#include <wrl/client.h>

namespace diskbloom::app {
namespace {

constexpr std::size_t palette_size = 12U;
constexpr float radial_gap = 1.5F;

bool contains(const AnalyzerRectF& bounds, const float x, const float y) noexcept {
    return x >= bounds.left && x < bounds.right && y >= bounds.top && y < bounds.bottom;
}

D2D1_RECT_F to_d2d_rect(const AnalyzerRectF& value) noexcept {
    return {value.left, value.top, value.right, value.bottom};
}

D2D1_COLOR_F to_d2d_color(const core::Rgba value) noexcept {
    return {value.r, value.g, value.b, value.a};
}

bool same_color(const core::Rgba left, const core::Rgba right) noexcept {
    return left.r == right.r && left.g == right.g && left.b == right.b && left.a == right.a;
}

D2D1_POINT_2F polar_point(
    const core::SunburstGeometry& geometry,
    const float radius,
    const float angle) noexcept {
    return {
        geometry.centerX + std::cos(angle) * radius,
        geometry.centerY + std::sin(angle) * radius,
    };
}

void append_annular_segment(
    ID2D1GeometrySink* sink,
    const core::SunburstGeometry& geometry,
    const core::SunburstSegment& segment) {
    const auto sweep = segment.endAngle - segment.startAngle;
    const auto angularGap = std::min(0.003F, sweep * 0.15F);
    const auto start = segment.startAngle + angularGap * 0.5F;
    const auto end = segment.endAngle - angularGap * 0.5F;
    if (end <= start) {
        return;
    }

    const auto inner = geometry.innerRadius
        + geometry.ringWidth * static_cast<float>(segment.depth)
        + radial_gap * 0.5F;
    const auto outer = geometry.innerRadius
        + geometry.ringWidth * static_cast<float>(segment.depth + 1U)
        - radial_gap * 0.5F;
    if (outer <= inner) {
        return;
    }

    const auto middle = (start + end) * 0.5F;
    const auto outerStart = polar_point(geometry, outer, start);
    const auto outerMiddle = polar_point(geometry, outer, middle);
    const auto outerEnd = polar_point(geometry, outer, end);
    const auto innerEnd = polar_point(geometry, inner, end);
    const auto innerMiddle = polar_point(geometry, inner, middle);
    const auto innerStart = polar_point(geometry, inner, start);

    sink->BeginFigure(outerStart, D2D1_FIGURE_BEGIN_FILLED);
    sink->AddArc(D2D1::ArcSegment(
        outerMiddle,
        D2D1::SizeF(outer, outer),
        0.0F,
        D2D1_SWEEP_DIRECTION_CLOCKWISE,
        D2D1_ARC_SIZE_SMALL));
    sink->AddArc(D2D1::ArcSegment(
        outerEnd,
        D2D1::SizeF(outer, outer),
        0.0F,
        D2D1_SWEEP_DIRECTION_CLOCKWISE,
        D2D1_ARC_SIZE_SMALL));
    sink->AddLine(innerEnd);
    sink->AddArc(D2D1::ArcSegment(
        innerMiddle,
        D2D1::SizeF(inner, inner),
        0.0F,
        D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
        D2D1_ARC_SIZE_SMALL));
    sink->AddArc(D2D1::ArcSegment(
        innerStart,
        D2D1::SizeF(inner, inner),
        0.0F,
        D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
        D2D1_ARC_SIZE_SMALL));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
}

std::wstring format_bytes(const std::uint64_t bytes) {
    constexpr std::array units{L"B", L"KB", L"MB", L"GB", L"TB", L"PB"};
    auto value = static_cast<double>(bytes);
    std::size_t unit = 0U;
    while (value >= 1024.0 && unit + 1U < units.size()) {
        value /= 1024.0;
        ++unit;
    }
    std::array<wchar_t, 64> buffer{};
    _snwprintf_s(
        buffer.data(),
        buffer.size(),
        _TRUNCATE,
        unit == 0U || value >= 100.0 ? L"%.0f %ls" : L"%.1f %ls",
        value,
        units[unit]);
    return std::wstring(buffer.data());
}

} // namespace

AnalyzerLayout compute_analyzer_layout(
    const float widthDip,
    const float heightDip,
    const std::size_t depthCount) noexcept {
    constexpr float headerHeight = 64.0F;
    const auto width = std::max(widthDip, 0.0F);
    const auto height = std::max(heightDip, headerHeight + 120.0F);
    const auto detailsLeft = std::clamp(width * 0.68F, 544.0F, width - 180.0F);
    const auto chartAreaRight = std::max(120.0F, detailsLeft - 24.0F);
    const auto centerX = chartAreaRight * 0.5F;
    const auto centerY = headerHeight + (height - headerHeight) * 0.5F;
    const auto radius = std::max(
        20.0F,
        std::min((chartAreaRight - 48.0F) * 0.5F, (height - headerHeight - 32.0F) * 0.5F));
    const auto bandCount = static_cast<float>(std::max<std::size_t>(depthCount, 1U));
    const auto innerRadius = radius * 0.22F;

    return {
        .header = {0.0F, 0.0F, width, headerHeight},
        .backButton = {16.0F, 10.0F, 60.0F, 54.0F},
        .titleBounds = {76.0F, 0.0F, std::max(76.0F, width - 32.0F), headerHeight},
        .chartBounds = {
            centerX - radius,
            centerY - radius,
            centerX + radius,
            centerY + radius,
        },
        .detailsBounds = {
            detailsLeft,
            headerHeight + 32.0F,
            std::max(detailsLeft, width - 28.0F),
            height - 32.0F,
        },
        .chartGeometry = {
            .centerX = centerX,
            .centerY = centerY,
            .innerRadius = innerRadius,
            .ringWidth = (radius - innerRadius) / bandCount,
        },
    };
}

AnalyzerHitTarget hit_test_analyzer_layout(
    const AnalyzerLayout& layout,
    const float xDip,
    const float yDip) noexcept {
    if (contains(layout.backButton, xDip, yDip)) {
        return AnalyzerHitTarget::Back;
    }
    const auto dx = xDip - layout.chartGeometry.centerX;
    const auto dy = yDip - layout.chartGeometry.centerY;
    const auto radius = std::hypot(dx, dy);
    const auto outerRadius = (layout.chartBounds.right - layout.chartBounds.left) * 0.5F;
    return radius >= layout.chartGeometry.innerRadius && radius < outerRadius
        ? AnalyzerHitTarget::Chart
        : AnalyzerHitTarget::None;
}

struct AnalyzerView::Resources {
    ID2D1DeviceContext* context = nullptr;
    core::Rgba themeKey{};
    core::Language language = core::Language::English;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> header;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> surface;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> primary;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> secondary;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> hover;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> border;
    std::array<Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>, palette_size> palette;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> titleFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> detailHeadingFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> detailFormat;
    std::array<Microsoft::WRL::ComPtr<ID2D1PathGeometry>, palette_size> batches;
    std::size_t geometryRevision = 0U;
    core::SunburstGeometry geometry{};
    bool geometryValid = false;
};

AnalyzerView::AnalyzerView() = default;
AnalyzerView::~AnalyzerView() = default;

void AnalyzerView::set_tree(const core::ScanTree* tree, const core::NodeIndex root) {
    tree_ = tree;
    root_ = root;
    selectedNode_ = root;
    hoveredSegment_.reset();
    pendingCommand_.reset();
    rebuild_layout();
}

bool AnalyzerView::set_root(const core::NodeIndex root) {
    if (tree_ == nullptr || root >= tree_->nodes().size()
        || !core::has_flag(tree_->node(root).flags, core::ScanNodeFlags::Directory)) {
        return false;
    }
    root_ = root;
    selectedNode_ = root;
    hoveredSegment_.reset();
    rebuild_layout();
    return true;
}

void AnalyzerView::set_selected_node(const core::NodeIndex node) noexcept {
    if (tree_ != nullptr && node < tree_->nodes().size()) {
        selectedNode_ = node;
    }
}

core::NodeIndex AnalyzerView::current_root() const noexcept {
    return root_;
}

void AnalyzerView::rebuild_layout() {
    sunburst_ = tree_ == nullptr
        ? core::SunburstLayout{}
        : core::build_sunburst_layout(*tree_, root_, {});
    ++layoutRevision_;
    if (resources_ != nullptr) {
        resources_->geometryValid = false;
    }
}

bool AnalyzerView::ensure_resources(
    render::GraphicsDevice& graphics,
    const core::ThemeTokens& theme,
    const core::Language language) {
    auto* context = graphics.d2d_context();
    auto* writeFactory = graphics.dwrite_factory();
    if (context == nullptr || writeFactory == nullptr) {
        return false;
    }
    if (resources_ != nullptr && resources_->context == context
        && same_color(resources_->themeKey, theme.window)
        && resources_->language == language) {
        return true;
    }

    auto resources = std::make_unique<Resources>();
    resources->context = context;
    resources->themeKey = theme.window;
    resources->language = language;
    const auto makeBrush = [&](const core::Rgba color, auto& brush) {
        return SUCCEEDED(context->CreateSolidColorBrush(to_d2d_color(color), &brush));
    };
    if (!makeBrush(theme.titleBar, resources->header)
        || !makeBrush(theme.row, resources->surface)
        || !makeBrush(theme.primaryText, resources->primary)
        || !makeBrush(theme.secondaryText, resources->secondary)
        || !makeBrush(theme.buttonHover, resources->hover)
        || !makeBrush(theme.border, resources->border)) {
        return false;
    }
    for (std::size_t index = 0U; index < palette_size; ++index) {
        if (!makeBrush(theme.chartPalette[index], resources->palette[index])) {
            return false;
        }
    }

    const auto locale = language == core::Language::SimplifiedChinese ? L"zh-CN" : L"en-US";
    const auto createFormat = [&](const float size, auto& format) {
        return SUCCEEDED(writeFactory->CreateTextFormat(
            L"Segoe UI Variable Text",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            size,
            locale,
            &format));
    };
    if (!createFormat(24.0F, resources->titleFormat)
        || !createFormat(27.0F, resources->detailHeadingFormat)
        || !createFormat(18.0F, resources->detailFormat)) {
        return false;
    }
    resources->titleFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    resources->titleFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    resources->detailHeadingFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    resources->detailFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    resources_ = std::move(resources);
    return true;
}

bool AnalyzerView::ensure_geometry() {
    if (resources_ == nullptr) {
        return false;
    }
    auto& resources = *resources_;
    const auto& geometry = layout_.chartGeometry;
    if (resources.geometryValid
        && resources.geometryRevision == layoutRevision_
        && resources.geometry.centerX == geometry.centerX
        && resources.geometry.centerY == geometry.centerY
        && resources.geometry.innerRadius == geometry.innerRadius
        && resources.geometry.ringWidth == geometry.ringWidth) {
        return true;
    }

    Microsoft::WRL::ComPtr<ID2D1Factory> factory;
    resources.context->GetFactory(&factory);
    if (factory == nullptr) {
        return false;
    }
    for (std::size_t paletteIndex = 0U; paletteIndex < palette_size; ++paletteIndex) {
        resources.batches[paletteIndex].Reset();
        if (FAILED(factory->CreatePathGeometry(&resources.batches[paletteIndex]))) {
            return false;
        }
        Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
        if (FAILED(resources.batches[paletteIndex]->Open(&sink))) {
            return false;
        }
        sink->SetFillMode(D2D1_FILL_MODE_WINDING);
        for (const auto& segment : sunburst_.segments) {
            if (segment.paletteIndex == paletteIndex) {
                append_annular_segment(sink.Get(), geometry, segment);
            }
        }
        if (FAILED(sink->Close())) {
            return false;
        }
    }
    resources.geometryRevision = layoutRevision_;
    resources.geometry = geometry;
    resources.geometryValid = true;
    return true;
}

bool AnalyzerView::draw(
    render::GraphicsDevice& graphics,
    const core::ThemeTokens& theme,
    const core::Language language,
    const float widthDip,
    const float heightDip) {
    if (tree_ == nullptr || root_ >= tree_->nodes().size()
        || !ensure_resources(graphics, theme, language)) {
        return false;
    }
    layout_ = compute_analyzer_layout(widthDip, heightDip, sunburst_.depthRanges.size());
    if (!ensure_geometry()) {
        return false;
    }

    auto* context = resources_->context;
    context->FillRectangle(to_d2d_rect(layout_.header), resources_->header.Get());
    if (backHovered_) {
        context->FillRoundedRectangle(
            D2D1::RoundedRect(to_d2d_rect(layout_.backButton), 5.0F, 5.0F),
            resources_->hover.Get());
    }
    const auto backCenterY = (layout_.backButton.top + layout_.backButton.bottom) * 0.5F;
    context->DrawLine(
        {layout_.backButton.left + 27.0F, backCenterY - 9.0F},
        {layout_.backButton.left + 18.0F, backCenterY},
        resources_->primary.Get(),
        2.0F);
    context->DrawLine(
        {layout_.backButton.left + 18.0F, backCenterY},
        {layout_.backButton.left + 27.0F, backCenterY + 9.0F},
        resources_->primary.Get(),
        2.0F);

    const auto drawText = [&](
                              const std::wstring_view text,
                              IDWriteTextFormat* format,
                              const AnalyzerRectF& bounds,
                              ID2D1Brush* brush) {
        context->DrawTextW(
            text.data(),
            static_cast<UINT32>(text.size()),
            format,
            to_d2d_rect(bounds),
            brush,
            D2D1_DRAW_TEXT_OPTIONS_CLIP);
    };
    drawText(
        tree_->name(root_),
        resources_->titleFormat.Get(),
        layout_.titleBounds,
        resources_->primary.Get());

    for (std::size_t index = 0U; index < palette_size; ++index) {
        context->FillGeometry(resources_->batches[index].Get(), resources_->palette[index].Get());
    }
    const auto centerRadius = layout_.chartGeometry.innerRadius - radial_gap;
    context->FillEllipse(
        D2D1::Ellipse(
            {layout_.chartGeometry.centerX, layout_.chartGeometry.centerY},
            centerRadius,
            centerRadius),
        resources_->surface.Get());
    context->DrawEllipse(
        D2D1::Ellipse(
            {layout_.chartGeometry.centerX, layout_.chartGeometry.centerY},
            centerRadius,
            centerRadius),
        resources_->border.Get(),
        1.0F);

    core::NodeIndex detailNode = selectedNode_ < tree_->nodes().size() ? selectedNode_ : root_;
    std::uint64_t detailBytes = tree_->node(detailNode).logicalSize;
    std::wstring detailName(tree_->name(detailNode));
    std::uint64_t itemCount = tree_->node(detailNode).fileCount
        + tree_->node(detailNode).directoryCount;
    if (hoveredSegment_.has_value()) {
        const auto& segment = sunburst_.segments[hoveredSegment_->segmentIndex];
        detailNode = segment.node;
        detailBytes = segment.logicalSize;
        if (segment.flags == core::SunburstSegmentFlags::Aggregate) {
            detailName = core::get_string(language, core::StringId::OtherItems);
            itemCount = 0U;
        } else {
            detailName = tree_->name(detailNode);
            itemCount = tree_->node(detailNode).fileCount
                + tree_->node(detailNode).directoryCount;
        }
    }

    const AnalyzerRectF nameBounds{
        layout_.detailsBounds.left,
        layout_.detailsBounds.top,
        layout_.detailsBounds.right,
        layout_.detailsBounds.top + 44.0F,
    };
    const AnalyzerRectF sizeBounds{
        layout_.detailsBounds.left,
        layout_.detailsBounds.top + 52.0F,
        layout_.detailsBounds.right,
        layout_.detailsBounds.top + 82.0F,
    };
    const AnalyzerRectF countBounds{
        layout_.detailsBounds.left,
        layout_.detailsBounds.top + 86.0F,
        layout_.detailsBounds.right,
        layout_.detailsBounds.top + 116.0F,
    };
    drawText(
        detailName,
        resources_->detailHeadingFormat.Get(),
        nameBounds,
        resources_->primary.Get());
    const auto sizeText = format_bytes(detailBytes);
    drawText(sizeText, resources_->detailFormat.Get(), sizeBounds, resources_->secondary.Get());
    if (itemCount > 0U) {
        auto countText = std::to_wstring(itemCount);
        countText.append(L" ");
        countText.append(core::get_string(language, core::StringId::Items));
        drawText(countText, resources_->detailFormat.Get(), countBounds, resources_->secondary.Get());
    }
    return true;
}

bool AnalyzerView::pointer_moved(const float xDip, const float yDip) {
    const auto target = hit_test_analyzer_layout(layout_, xDip, yDip);
    const auto nextBackHovered = target == AnalyzerHitTarget::Back;
    std::optional<core::SunburstHit> nextSegment;
    if (target == AnalyzerHitTarget::Chart) {
        nextSegment = core::hit_test_sunburst(
            sunburst_,
            layout_.chartGeometry,
            xDip,
            yDip);
    }
    const auto sameSegment = hoveredSegment_.has_value() == nextSegment.has_value()
        && (!hoveredSegment_.has_value()
            || hoveredSegment_->segmentIndex == nextSegment->segmentIndex);
    if (backHovered_ == nextBackHovered && sameSegment) {
        return false;
    }
    backHovered_ = nextBackHovered;
    hoveredSegment_ = nextSegment;
    return true;
}

bool AnalyzerView::pointer_left() {
    if (!backHovered_ && !hoveredSegment_.has_value()) {
        return false;
    }
    backHovered_ = false;
    hoveredSegment_.reset();
    return true;
}

void AnalyzerView::pointer_pressed(const float xDip, const float yDip) {
    const auto target = hit_test_analyzer_layout(layout_, xDip, yDip);
    if (target == AnalyzerHitTarget::Back) {
        const auto parent = tree_ != nullptr && root_ < tree_->nodes().size()
            ? tree_->node(root_).parent
            : core::invalid_node;
        pendingCommand_ = parent == core::invalid_node
            ? AnalyzerCommand{AnalyzerCommandKind::ReturnToOverview, core::invalid_node}
            : AnalyzerCommand{AnalyzerCommandKind::NavigateToParent, parent};
        return;
    }
    if (target != AnalyzerHitTarget::Chart || tree_ == nullptr) {
        pendingCommand_.reset();
        return;
    }
    const auto hit = core::hit_test_sunburst(
        sunburst_,
        layout_.chartGeometry,
        xDip,
        yDip);
    if (!hit.has_value() || hit->aggregate) {
        pendingCommand_.reset();
        return;
    }
    const auto& node = tree_->node(hit->node);
    pendingCommand_ = core::has_flag(node.flags, core::ScanNodeFlags::Directory)
        ? AnalyzerCommand{AnalyzerCommandKind::NavigateToNode, hit->node}
        : AnalyzerCommand{AnalyzerCommandKind::SelectNode, hit->node};
}

std::optional<AnalyzerCommand> AnalyzerView::take_command() noexcept {
    return std::exchange(pendingCommand_, std::nullopt);
}

} // namespace diskbloom::app
