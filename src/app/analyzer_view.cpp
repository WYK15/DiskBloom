#include "app/analyzer_view.h"

#include "core/child_ranking.h"
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
    constexpr float actionBarHeight = 56.0F;
    const auto width = std::max(widthDip, 0.0F);
    const auto height = std::max(heightDip, headerHeight + 120.0F);
    const auto detailsLeft = std::clamp(width * 0.68F, 544.0F, width - 180.0F);
    const auto chartAreaRight = std::max(120.0F, detailsLeft - 24.0F);
    const auto centerX = chartAreaRight * 0.5F;
    const auto actionTop = height - actionBarHeight;
    const auto centerY = headerHeight + (actionTop - headerHeight) * 0.5F;
    const auto radius = std::max(
        20.0F,
        std::min((chartAreaRight - 48.0F) * 0.5F, (actionTop - headerHeight - 24.0F) * 0.5F));
    const auto bandCount = static_cast<float>(std::max<std::size_t>(depthCount, 1U));
    const auto innerRadius = radius * 0.22F;

    return {
        .header = {0.0F, 0.0F, width, headerHeight},
        .backButton = {16.0F, 10.0F, 60.0F, 54.0F},
        .minimizeButton = {width - 138.0F, 0.0F, width - 92.0F, headerHeight},
        .maximizeButton = {width - 92.0F, 0.0F, width - 46.0F, headerHeight},
        .closeButton = {width - 46.0F, 0.0F, width, headerHeight},
        .titleBounds = {76.0F, 0.0F, std::max(76.0F, width - 150.0F), headerHeight},
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
            actionTop - 16.0F,
        },
        .actionBar = {0.0F, actionTop, width, height},
        .reviewButton = {width - 624.0F, actionTop + 10.0F, width - 512.0F, height - 10.0F},
        .addReviewButton = {width - 500.0F, actionTop + 10.0F, width - 344.0F, height - 10.0F},
        .previewButton = {width - 332.0F, actionTop + 10.0F, width - 220.0F, height - 10.0F},
        .revealButton = {width - 208.0F, actionTop + 10.0F, width - 28.0F, height - 10.0F},
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
    if (contains(layout.minimizeButton, xDip, yDip)) {
        return AnalyzerHitTarget::MinimizeWindow;
    }
    if (contains(layout.maximizeButton, xDip, yDip)) {
        return AnalyzerHitTarget::MaximizeWindow;
    }
    if (contains(layout.closeButton, xDip, yDip)) {
        return AnalyzerHitTarget::CloseWindow;
    }
    if (contains(layout.previewButton, xDip, yDip)) {
        return AnalyzerHitTarget::Preview;
    }
    if (contains(layout.revealButton, xDip, yDip)) {
        return AnalyzerHitTarget::Reveal;
    }
    if (contains(layout.addReviewButton, xDip, yDip)) {
        return AnalyzerHitTarget::AddToReview;
    }
    if (contains(layout.reviewButton, xDip, yDip)) {
        return AnalyzerHitTarget::ReviewDelete;
    }
    const auto dx = xDip - layout.chartGeometry.centerX;
    const auto dy = yDip - layout.chartGeometry.centerY;
    const auto radius = std::hypot(dx, dy);
    const auto outerRadius = (layout.chartBounds.right - layout.chartBounds.left) * 0.5F;
    return radius >= layout.chartGeometry.innerRadius && radius < outerRadius
        ? AnalyzerHitTarget::Chart
        : AnalyzerHitTarget::None;
}

AnalyzerChildListLayout compute_analyzer_child_list_layout(
    const AnalyzerRectF& detailsBounds,
    const std::size_t itemCount,
    const std::size_t scrollOffset) {
    constexpr float listTopOffset = 128.0F;
    constexpr float rowHeight = 32.0F;
    constexpr float sizeWidth = 82.0F;
    AnalyzerChildListLayout layout;
    const auto listTop = detailsBounds.top + listTopOffset;
    const auto availableHeight = std::max(detailsBounds.bottom - listTop, 0.0F);
    layout.visibleCapacity = static_cast<std::size_t>(availableHeight / rowHeight);
    layout.scrollOffset = std::min(
        scrollOffset,
        itemCount > layout.visibleCapacity ? itemCount - layout.visibleCapacity : 0U);
    const auto visibleCount = std::min(
        layout.visibleCapacity,
        itemCount - std::min(itemCount, layout.scrollOffset));
    layout.rows.reserve(visibleCount);
    const auto sizeLeft = std::max(detailsBounds.left + 80.0F, detailsBounds.right - sizeWidth);
    for (std::size_t index = 0U; index < visibleCount; ++index) {
        const auto top = listTop + static_cast<float>(index) * rowHeight;
        layout.rows.push_back({
            .bounds = {detailsBounds.left, top, detailsBounds.right, top + rowHeight},
            .nameBounds = {detailsBounds.left + 22.0F, top, sizeLeft - 8.0F, top + rowHeight},
            .sizeBounds = {sizeLeft, top, detailsBounds.right, top + rowHeight},
            .itemIndex = layout.scrollOffset + index,
        });
    }
    return layout;
}

std::optional<std::size_t> hit_test_analyzer_child_rows(
    const AnalyzerChildListLayout& layout,
    const float xDip,
    const float yDip) noexcept {
    for (const auto& row : layout.rows) {
        if (contains(row.bounds, xDip, yDip)) {
            return row.itemIndex;
        }
    }
    return std::nullopt;
}

std::size_t next_child_scroll_offset(
    const std::size_t current,
    const std::size_t itemCount,
    const std::size_t visibleCount,
    const int deltaRows) noexcept {
    const auto maximum = itemCount > visibleCount ? itemCount - visibleCount : 0U;
    const auto next = static_cast<long long>(std::min(current, maximum))
        + static_cast<long long>(deltaRows);
    return static_cast<std::size_t>(std::clamp<long long>(
        next,
        0LL,
        static_cast<long long>(maximum)));
}

struct AnalyzerView::Resources {
    ID2D1DeviceContext* context = nullptr;
    core::Rgba themeKey{};
    core::Language language = core::Language::English;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> header;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> surface;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> panel;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> primary;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> secondary;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> hover;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> accent;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> border;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> danger;
    std::array<Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>, palette_size> palette;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> titleFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> detailHeadingFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> detailFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> centerFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> rowNameFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> rowSizeFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> buttonFormat;
    Microsoft::WRL::ComPtr<IDWriteInlineObject> rowEllipsis;
    std::array<Microsoft::WRL::ComPtr<ID2D1PathGeometry>, palette_size> batches;
    std::size_t geometryRevision = 0U;
    core::SunburstGeometry geometry{};
    bool geometryValid = false;
};

AnalyzerView::AnalyzerView() = default;
AnalyzerView::~AnalyzerView() = default;

void AnalyzerView::set_tree(const core::ScanTree* tree, const core::NodeIndex root) {
    (void)cancel_drag();
    tree_ = tree;
    root_ = root;
    selectedNode_ = root;
    hoveredSegment_.reset();
    hoveredChild_.reset();
    pendingCommand_.reset();
    rebuild_layout();
}

bool AnalyzerView::set_root(const core::NodeIndex root) {
    if (tree_ == nullptr || root >= tree_->nodes().size()
        || !core::has_flag(tree_->node(root).flags, core::ScanNodeFlags::Directory)) {
        return false;
    }
    (void)cancel_drag();
    root_ = root;
    selectedNode_ = root;
    hoveredSegment_.reset();
    hoveredChild_.reset();
    rebuild_layout();
    return true;
}

void AnalyzerView::set_selected_node(const core::NodeIndex node) noexcept {
    if (tree_ != nullptr && node < tree_->nodes().size()) {
        selectedNode_ = node;
    }
}

void AnalyzerView::set_review_summary(
    const std::size_t itemCount,
    const std::uint64_t totalBytes) noexcept {
    reviewItemCount_ = itemCount;
    reviewTotalBytes_ = totalBytes;
    if (itemCount == 0U) {
        reviewPanelOpen_ = false;
    }
}

void AnalyzerView::set_review_nodes(const std::span<const core::NodeIndex> nodes) {
    if (reviewNodes_.size() == nodes.size()
        && std::equal(reviewNodes_.begin(), reviewNodes_.end(), nodes.begin())) {
        return;
    }
    reviewNodes_.assign(nodes.begin(), nodes.end());
    reviewScrollOffset_ = next_review_scroll_offset(
        reviewScrollOffset_,
        reviewNodes_.size(),
        reviewLayout_.visibleCapacity,
        0);
    if (reviewNodes_.empty()) {
        reviewPanelOpen_ = false;
        reviewLayout_ = {};
    }
}

void AnalyzerView::set_recycle_in_progress(const bool inProgress) noexcept {
    recycleInProgress_ = inProgress;
    if (inProgress) {
        (void)cancel_drag();
        hoveredChrome_ = AnalyzerHitTarget::None;
        reviewPanelOpen_ = false;
        pendingCommand_.reset();
    }
}

core::NodeIndex AnalyzerView::current_root() const noexcept {
    return root_;
}

void AnalyzerView::rebuild_layout() {
    sunburst_ = tree_ == nullptr
        ? core::SunburstLayout{}
        : core::build_sunburst_layout(*tree_, root_, {});
    rankedChildren_ = tree_ == nullptr
        ? std::vector<core::RankedChild>{}
        : core::rank_children(*tree_, root_, 24U);
    childPaletteIndices_.assign(rankedChildren_.size(), 0U);
    for (std::size_t childIndex = 0U; childIndex < rankedChildren_.size(); ++childIndex) {
        const auto segment = std::find_if(
            sunburst_.segments.begin(),
            sunburst_.segments.end(),
            [&](const core::SunburstSegment& value) {
                return value.depth == 0U && value.node == rankedChildren_[childIndex].node;
            });
        childPaletteIndices_[childIndex] = segment == sunburst_.segments.end()
            ? static_cast<std::uint8_t>(childIndex % palette_size)
            : segment->paletteIndex;
    }
    childScrollOffset_ = 0U;
    childListLayout_ = {};
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
        || !makeBrush(theme.alternateRow, resources->panel)
        || !makeBrush(theme.primaryText, resources->primary)
        || !makeBrush(theme.secondaryText, resources->secondary)
        || !makeBrush(theme.buttonHover, resources->hover)
        || !makeBrush(theme.accent, resources->accent)
        || !makeBrush(theme.border, resources->border)
        || !makeBrush(theme.danger, resources->danger)) {
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
        || !createFormat(18.0F, resources->detailFormat)
        || !createFormat(18.0F, resources->centerFormat)
        || !createFormat(16.0F, resources->rowNameFormat)
        || !createFormat(16.0F, resources->rowSizeFormat)
        || !createFormat(16.0F, resources->buttonFormat)) {
        return false;
    }
    resources->titleFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    resources->titleFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    resources->detailHeadingFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    resources->detailFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    resources->centerFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    resources->centerFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    resources->centerFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    resources->rowNameFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    resources->rowNameFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    const DWRITE_TRIMMING rowTrimming{
        .granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER,
        .delimiter = 0U,
        .delimiterCount = 0U,
    };
    if (FAILED(writeFactory->CreateEllipsisTrimmingSign(
            resources->rowNameFormat.Get(),
            &resources->rowEllipsis))
        || FAILED(resources->rowNameFormat->SetTrimming(
            &rowTrimming,
            resources->rowEllipsis.Get()))) {
        return false;
    }
    resources->rowSizeFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    resources->rowSizeFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    resources->rowSizeFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    resources->buttonFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    resources->buttonFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    resources->buttonFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
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
    childListLayout_ = compute_analyzer_child_list_layout(
        layout_.detailsBounds,
        rankedChildren_.size(),
        childScrollOffset_);
    childScrollOffset_ = childListLayout_.scrollOffset;
    reviewLayout_ = compute_review_collector_layout(
        layout_.actionBar,
        widthDip,
        heightDip,
        reviewNodes_.size(),
        reviewScrollOffset_);
    reviewLayout_.summary.right = std::max(
        reviewLayout_.summary.left,
        std::min(reviewLayout_.summary.right, layout_.reviewButton.left - 20.0F));
    reviewScrollOffset_ = reviewLayout_.scrollOffset;
    if (!ensure_geometry()) {
        return false;
    }

    auto* context = resources_->context;
    const auto dragVisual = compute_review_drag_visual(
        reviewDrag_.active(),
        reviewDrag_.valid_drop(),
        dragPointerX_,
        dragPointerY_,
        widthDip,
        heightDip);
    context->FillRectangle(to_d2d_rect(layout_.header), resources_->header.Get());
    context->FillRectangle(to_d2d_rect(layout_.actionBar), resources_->header.Get());
    if (hoveredChrome_ == AnalyzerHitTarget::Back) {
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

    const auto drawWindowButtonBackground = [&](const AnalyzerRectF& bounds, const AnalyzerHitTarget target) {
        if (hoveredChrome_ == target) {
            context->FillRectangle(
                to_d2d_rect(bounds),
                target == AnalyzerHitTarget::CloseWindow
                    ? resources_->danger.Get()
                    : resources_->hover.Get());
        }
    };
    drawWindowButtonBackground(layout_.minimizeButton, AnalyzerHitTarget::MinimizeWindow);
    drawWindowButtonBackground(layout_.maximizeButton, AnalyzerHitTarget::MaximizeWindow);
    drawWindowButtonBackground(layout_.closeButton, AnalyzerHitTarget::CloseWindow);

    const auto buttonCenter = [](const AnalyzerRectF& bounds) {
        return D2D1::Point2F(
            (bounds.left + bounds.right) * 0.5F,
            (bounds.top + bounds.bottom) * 0.5F);
    };
    const auto minimizeCenter = buttonCenter(layout_.minimizeButton);
    context->DrawLine(
        {minimizeCenter.x - 6.0F, minimizeCenter.y},
        {minimizeCenter.x + 6.0F, minimizeCenter.y},
        resources_->primary.Get(),
        1.5F);
    const auto maximizeCenter = buttonCenter(layout_.maximizeButton);
    context->DrawRectangle(
        {maximizeCenter.x - 5.0F, maximizeCenter.y - 5.0F,
         maximizeCenter.x + 5.0F, maximizeCenter.y + 5.0F},
        resources_->primary.Get(),
        1.25F);
    const auto closeCenter = buttonCenter(layout_.closeButton);
    context->DrawLine(
        {closeCenter.x - 5.0F, closeCenter.y - 5.0F},
        {closeCenter.x + 5.0F, closeCenter.y + 5.0F},
        resources_->primary.Get(),
        1.5F);
    context->DrawLine(
        {closeCenter.x + 5.0F, closeCenter.y - 5.0F},
        {closeCenter.x - 5.0F, closeCenter.y + 5.0F},
        resources_->primary.Get(),
        1.5F);
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

    const auto drawActionButton = [&](
                                      const AnalyzerRectF& bounds,
                                      const AnalyzerHitTarget target,
                                      const core::StringId label,
                                      ID2D1Brush* textBrush) {
        context->FillRoundedRectangle(
            D2D1::RoundedRect(to_d2d_rect(bounds), 5.0F, 5.0F),
            hoveredChrome_ == target ? resources_->hover.Get() : resources_->surface.Get());
        context->DrawRoundedRectangle(
            D2D1::RoundedRect(to_d2d_rect(bounds), 5.0F, 5.0F),
            resources_->border.Get(),
            1.0F);
        drawText(
            core::get_string(language, label),
            resources_->buttonFormat.Get(),
            bounds,
            textBrush);
    };
    drawActionButton(
        layout_.reviewButton,
        AnalyzerHitTarget::ReviewDelete,
        recycleInProgress_ ? core::StringId::Recycling : core::StringId::DeleteReviewed,
        reviewItemCount_ > 0U ? resources_->danger.Get() : resources_->secondary.Get());
    drawActionButton(
        layout_.addReviewButton,
        AnalyzerHitTarget::AddToReview,
        core::StringId::AddToReview,
        resources_->primary.Get());
    drawActionButton(
        layout_.previewButton,
        AnalyzerHitTarget::Preview,
        core::StringId::Open,
        resources_->primary.Get());
    drawActionButton(
        layout_.revealButton,
        AnalyzerHitTarget::Reveal,
        core::StringId::ShowInExplorer,
        resources_->primary.Get());
    ID2D1Brush* collectorDragBrush = nullptr;
    switch (dragVisual.collectorToken) {
    case ReviewCollectorVisualToken::None:
        break;
    case ReviewCollectorVisualToken::Hover:
        collectorDragBrush = resources_->hover.Get();
        break;
    case ReviewCollectorVisualToken::Accent:
        collectorDragBrush = resources_->accent.Get();
        break;
    }
    if (collectorDragBrush != nullptr) {
        context->FillRectangle(
            to_d2d_rect(reviewLayout_.summary),
            collectorDragBrush);
    }
    if (reviewItemCount_ > 0U) {
        auto reviewText = std::to_wstring(reviewItemCount_);
        reviewText.append(L" ");
        reviewText.append(core::get_string(language, core::StringId::Items));
        reviewText.append(L"\n");
        reviewText.append(core::get_string(language, core::StringId::Collected));
        reviewText.append(L" ");
        reviewText.append(format_bytes(reviewTotalBytes_));
        drawText(
            reviewText,
            resources_->buttonFormat.Get(),
            reviewLayout_.summary,
            resources_->secondary.Get());
    } else if (selectedNode_ < tree_->nodes().size()) {
        const AnalyzerRectF selectedBounds{
            28.0F,
            layout_.actionBar.top,
            std::max(28.0F, layout_.reviewButton.left - 12.0F),
            layout_.actionBar.bottom,
        };
        drawText(
            tree_->name(selectedNode_),
            resources_->detailFormat.Get(),
            selectedBounds,
            resources_->secondary.Get());
    }

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
    const AnalyzerRectF centerBounds{
        layout_.chartGeometry.centerX - centerRadius,
        layout_.chartGeometry.centerY - centerRadius,
        layout_.chartGeometry.centerX + centerRadius,
        layout_.chartGeometry.centerY + centerRadius,
    };
    const auto rootSizeText = format_bytes(tree_->node(root_).logicalSize);
    drawText(
        rootSizeText,
        resources_->centerFormat.Get(),
        centerBounds,
        resources_->primary.Get());

    const auto detailNode = root_;
    const auto detailBytes = tree_->node(detailNode).logicalSize;
    const std::wstring detailName(tree_->name(detailNode));
    const auto itemCount = tree_->node(detailNode).fileCount
        + tree_->node(detailNode).directoryCount;

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

    for (const auto& row : childListLayout_.rows) {
        if (row.itemIndex >= rankedChildren_.size()) {
            continue;
        }
        const auto node = rankedChildren_[row.itemIndex].node;
        if ((hoveredChild_.has_value() && *hoveredChild_ == row.itemIndex)
            || selectedNode_ == node) {
            context->FillRectangle(to_d2d_rect(row.bounds), resources_->hover.Get());
        }
        const auto dotCenter = D2D1::Point2F(
            row.bounds.left + 9.0F,
            (row.bounds.top + row.bounds.bottom) * 0.5F);
        context->FillEllipse(
            D2D1::Ellipse(dotCenter, 4.5F, 4.5F),
            resources_->palette[childPaletteIndices_[row.itemIndex]].Get());
        drawText(
            tree_->name(node),
            resources_->rowNameFormat.Get(),
            row.nameBounds,
            resources_->primary.Get());
        const auto childSize = format_bytes(rankedChildren_[row.itemIndex].logicalSize);
        drawText(
            childSize,
            resources_->rowSizeFormat.Get(),
            row.sizeBounds,
            resources_->secondary.Get());
    }
    if (reviewPanelOpen_ && !reviewNodes_.empty()) {
        context->FillRoundedRectangle(
            D2D1::RoundedRect(to_d2d_rect(reviewLayout_.panel), 5.0F, 5.0F),
            resources_->panel.Get());
        context->DrawRoundedRectangle(
            D2D1::RoundedRect(to_d2d_rect(reviewLayout_.panel), 5.0F, 5.0F),
            resources_->border.Get(),
            1.0F);
        for (const auto& row : reviewLayout_.rows) {
            if (row.itemIndex >= reviewNodes_.size()) {
                continue;
            }
            const auto node = reviewNodes_[row.itemIndex];
            if (node >= tree_->nodes().size()) {
                continue;
            }
            drawText(
                tree_->name(node),
                resources_->rowNameFormat.Get(),
                row.nameBounds,
                resources_->primary.Get());
            const auto size = format_bytes(tree_->node(node).logicalSize);
            drawText(
                size,
                resources_->rowSizeFormat.Get(),
                row.sizeBounds,
                resources_->secondary.Get());
        }
    }
    const auto dragNode = reviewDrag_.node();
    if (dragVisual.previewVisible && dragNode < tree_->nodes().size()) {
        context->FillRoundedRectangle(
            D2D1::RoundedRect(to_d2d_rect(dragVisual.previewBounds), 5.0F, 5.0F),
            resources_->panel.Get());
        context->DrawRoundedRectangle(
            D2D1::RoundedRect(to_d2d_rect(dragVisual.previewBounds), 5.0F, 5.0F),
            dragVisual.collectorToken == ReviewCollectorVisualToken::Accent
                ? resources_->accent.Get()
                : resources_->border.Get(),
            1.0F);
        drawText(
            tree_->name(dragNode),
            resources_->rowNameFormat.Get(),
            dragVisual.nameBounds,
            resources_->primary.Get());
        const auto dragSize = format_bytes(tree_->node(dragNode).logicalSize);
        drawText(
            dragSize,
            resources_->rowNameFormat.Get(),
            dragVisual.sizeBounds,
            resources_->secondary.Get());
    }
    return true;
}

bool AnalyzerView::pointer_moved(const float xDip, const float yDip) {
    auto dragChanged = false;
    if (reviewDrag_.pending() || reviewDrag_.active()) {
        const auto pointerChanged = dragPointerX_ != xDip || dragPointerY_ != yDip;
        dragPointerX_ = xDip;
        dragPointerY_ = yDip;
        dragChanged = reviewDrag_.move(
            xDip,
            yDip,
            contains_point(reviewLayout_.summary, xDip, yDip));
        dragChanged = dragChanged || (reviewDrag_.active() && pointerChanged);
    }
    const auto pointerOverReviewPanel = reviewPanelOpen_
        && contains_point(reviewLayout_.panel, xDip, yDip);
    const auto nextReviewPanelOpen = !recycleInProgress_ && !reviewNodes_.empty()
        && (contains_point(reviewLayout_.summary, xDip, yDip)
            || pointerOverReviewPanel);
    const auto target = hit_test_analyzer_layout(layout_, xDip, yDip);
    const auto nextChild = pointerOverReviewPanel
        ? std::optional<std::size_t>{}
        : hit_test_analyzer_child_rows(childListLayout_, xDip, yDip);
    const auto nextChrome = target == AnalyzerHitTarget::Back
            || target == AnalyzerHitTarget::MinimizeWindow
            || target == AnalyzerHitTarget::MaximizeWindow
            || target == AnalyzerHitTarget::CloseWindow
            || target == AnalyzerHitTarget::Preview
            || target == AnalyzerHitTarget::Reveal
            || target == AnalyzerHitTarget::AddToReview
            || target == AnalyzerHitTarget::ReviewDelete
        ? target
        : AnalyzerHitTarget::None;
    std::optional<core::SunburstHit> nextSegment;
    if (!pointerOverReviewPanel && !nextChild.has_value()
        && target == AnalyzerHitTarget::Chart) {
        nextSegment = core::hit_test_sunburst(
            sunburst_,
            layout_.chartGeometry,
            xDip,
            yDip);
    }
    const auto sameSegment = hoveredSegment_.has_value() == nextSegment.has_value()
        && (!hoveredSegment_.has_value()
            || hoveredSegment_->segmentIndex == nextSegment->segmentIndex);
    if (hoveredChrome_ == nextChrome && sameSegment && hoveredChild_ == nextChild
        && reviewPanelOpen_ == nextReviewPanelOpen) {
        return dragChanged;
    }
    hoveredChrome_ = nextChrome;
    hoveredSegment_ = nextSegment;
    hoveredChild_ = nextChild;
    reviewPanelOpen_ = nextReviewPanelOpen;
    return true;
}

bool AnalyzerView::pointer_left() {
    const auto dragChanged = cancel_drag();
    if (hoveredChrome_ == AnalyzerHitTarget::None
        && !hoveredSegment_.has_value()
        && !hoveredChild_.has_value()
        && !reviewPanelOpen_) {
        return dragChanged;
    }
    hoveredChrome_ = AnalyzerHitTarget::None;
    hoveredSegment_.reset();
    hoveredChild_.reset();
    reviewPanelOpen_ = false;
    return true;
}

bool AnalyzerView::scroll_children(const int deltaRows) noexcept {
    const auto next = next_child_scroll_offset(
        childScrollOffset_,
        rankedChildren_.size(),
        childListLayout_.visibleCapacity,
        deltaRows);
    if (next == childScrollOffset_) {
        return false;
    }
    childScrollOffset_ = next;
    hoveredChild_.reset();
    return true;
}

bool AnalyzerView::scroll_review(const int deltaRows) noexcept {
    if (!reviewPanelOpen_) {
        return false;
    }
    const auto next = next_review_scroll_offset(
        reviewScrollOffset_,
        reviewNodes_.size(),
        reviewLayout_.visibleCapacity,
        deltaRows);
    if (next == reviewScrollOffset_) {
        return false;
    }
    reviewScrollOffset_ = next;
    return true;
}

bool AnalyzerView::scroll_at(
    const float xDip,
    const float yDip,
    const int deltaRows) noexcept {
    return review_scroll_target_at(reviewPanelOpen_, reviewLayout_.panel, xDip, yDip)
            == ReviewScrollTarget::Review
        ? scroll_review(deltaRows)
        : scroll_children(deltaRows);
}

void AnalyzerView::pointer_down(const float xDip, const float yDip) {
    (void)cancel_drag();
    pendingCommand_.reset();
    (void)pointer_moved(xDip, yDip);
    if (recycleInProgress_ || tree_ == nullptr
        || (reviewPanelOpen_ && contains_point(reviewLayout_.panel, xDip, yDip))) {
        return;
    }

    auto node = core::invalid_node;
    if (const auto childIndex = hit_test_analyzer_child_rows(
            childListLayout_,
            xDip,
            yDip);
        childIndex.has_value() && *childIndex < rankedChildren_.size()) {
        node = rankedChildren_[*childIndex].node;
    } else if (hit_test_analyzer_layout(layout_, xDip, yDip) == AnalyzerHitTarget::Chart) {
        const auto hit = core::hit_test_sunburst(
            sunburst_,
            layout_.chartGeometry,
            xDip,
            yDip);
        if (hit.has_value() && !hit->aggregate) {
            node = hit->node;
        }
    }

    if (node == root_ || node == core::invalid_node || node >= tree_->nodes().size()) {
        return;
    }
    dragPointerX_ = xDip;
    dragPointerY_ = yDip;
    (void)reviewDrag_.begin(node, xDip, yDip);
}

void AnalyzerView::pointer_released(const float xDip, const float yDip) {
    if (!reviewDrag_.pending() && !reviewDrag_.active()) {
        pointer_pressed(xDip, yDip);
        return;
    }

    const auto overCollector = contains_point(reviewLayout_.summary, xDip, yDip);
    (void)reviewDrag_.move(xDip, yDip, overCollector);
    const auto wasActive = reviewDrag_.active();
    const auto droppedNode = reviewDrag_.release(overCollector);
    if (droppedNode.has_value() && !recycleInProgress_) {
        pendingCommand_ = AnalyzerCommand{
            AnalyzerCommandKind::AddToReview,
            *droppedNode,
        };
    } else if (!wasActive) {
        pointer_pressed(xDip, yDip);
    } else {
        pendingCommand_.reset();
    }
}

bool AnalyzerView::cancel_drag() noexcept {
    return reviewDrag_.cancel();
}

bool AnalyzerView::drag_pending() const noexcept {
    return reviewDrag_.pending();
}

bool AnalyzerView::drag_active() const noexcept {
    return reviewDrag_.active();
}

void AnalyzerView::pointer_pressed(const float xDip, const float yDip) {
    if (reviewPanelOpen_ && contains_point(reviewLayout_.panel, xDip, yDip)) {
        pendingCommand_.reset();
        return;
    }
    if (const auto childIndex = hit_test_analyzer_child_rows(
            childListLayout_,
            xDip,
            yDip);
        childIndex.has_value() && *childIndex < rankedChildren_.size()) {
        const auto nodeIndex = rankedChildren_[*childIndex].node;
        pendingCommand_ = core::has_flag(
                              tree_->node(nodeIndex).flags,
                              core::ScanNodeFlags::Directory)
            ? AnalyzerCommand{AnalyzerCommandKind::NavigateToNode, nodeIndex}
            : AnalyzerCommand{AnalyzerCommandKind::SelectNode, nodeIndex};
        return;
    }
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
    if (target == AnalyzerHitTarget::MinimizeWindow) {
        pendingCommand_ = {AnalyzerCommandKind::MinimizeWindow, core::invalid_node};
        return;
    }
    if (target == AnalyzerHitTarget::MaximizeWindow) {
        pendingCommand_ = {AnalyzerCommandKind::ToggleMaximizeWindow, core::invalid_node};
        return;
    }
    if (target == AnalyzerHitTarget::CloseWindow) {
        pendingCommand_ = {AnalyzerCommandKind::CloseWindow, core::invalid_node};
        return;
    }
    if (target == AnalyzerHitTarget::Preview) {
        pendingCommand_ = {AnalyzerCommandKind::PreviewNode, selectedNode_};
        return;
    }
    if (target == AnalyzerHitTarget::Reveal) {
        pendingCommand_ = {AnalyzerCommandKind::RevealNode, selectedNode_};
        return;
    }
    if (target == AnalyzerHitTarget::AddToReview) {
        pendingCommand_ = recycleInProgress_
            ? std::nullopt
            : std::optional<AnalyzerCommand>(
                AnalyzerCommand{AnalyzerCommandKind::AddToReview, selectedNode_});
        return;
    }
    if (target == AnalyzerHitTarget::ReviewDelete) {
        pendingCommand_ = reviewItemCount_ > 0U && !recycleInProgress_
            ? std::optional<AnalyzerCommand>(
                AnalyzerCommand{AnalyzerCommandKind::ConfirmReview, core::invalid_node})
            : std::nullopt;
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
