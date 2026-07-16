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

void append_transition_segment(
    ID2D1GeometrySink* sink,
    const float centerX,
    const float centerY,
    const core::TransitionDrawSegment& segment) {
    const auto sweep = segment.endAngle - segment.startAngle;
    const auto angularGap = std::min(0.003F, sweep * 0.15F);
    const auto start = segment.startAngle + angularGap * 0.5F;
    const auto end = segment.endAngle - angularGap * 0.5F;
    const auto inner = segment.innerRadius + radial_gap * 0.5F;
    const auto outer = segment.outerRadius - radial_gap * 0.5F;
    if (end <= start || outer <= inner) {
        return;
    }
    const auto point = [centerX, centerY](const float radius, const float angle) {
        return D2D1::Point2F(
            centerX + std::cos(angle) * radius,
            centerY + std::sin(angle) * radius);
    };
    const auto middle = (start + end) * 0.5F;
    const auto outerStart = point(outer, start);
    const auto outerMiddle = point(outer, middle);
    const auto outerEnd = point(outer, end);
    const auto innerEnd = point(inner, end);
    const auto innerMiddle = point(inner, middle);
    const auto innerStart = point(inner, start);
    sink->BeginFigure(outerStart, D2D1_FIGURE_BEGIN_FILLED);
    sink->AddArc(D2D1::ArcSegment(
        outerMiddle, D2D1::SizeF(outer, outer), 0.0F,
        D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
    sink->AddArc(D2D1::ArcSegment(
        outerEnd, D2D1::SizeF(outer, outer), 0.0F,
        D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
    sink->AddLine(innerEnd);
    sink->AddArc(D2D1::ArcSegment(
        innerMiddle, D2D1::SizeF(inner, inner), 0.0F,
        D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
    sink->AddArc(D2D1::ArcSegment(
        innerStart, D2D1::SizeF(inner, inner), 0.0F,
        D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
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
        .backButton = {12.0F, 10.0F, 52.0F, 54.0F},
        .forwardButton = {56.0F, 10.0F, 96.0F, 54.0F},
        .breadcrumbBounds = {108.0F, 8.0F, std::max(108.0F, width - 150.0F), 56.0F},
        .minimizeButton = {width - 138.0F, 0.0F, width - 92.0F, headerHeight},
        .maximizeButton = {width - 92.0F, 0.0F, width - 46.0F, headerHeight},
        .closeButton = {width - 46.0F, 0.0F, width, headerHeight},
        .titleBounds = {108.0F, 0.0F, std::max(108.0F, width - 150.0F), headerHeight},
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
    if (contains(layout.forwardButton, xDip, yDip)) {
        return AnalyzerHitTarget::Forward;
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
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> hoverPulse;
    std::array<Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>, palette_size> palette;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> titleFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> breadcrumbFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> detailHeadingFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> detailFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> centerFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> rowNameFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> rowSizeFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> buttonFormat;
    Microsoft::WRL::ComPtr<IDWriteInlineObject> rowEllipsis;
    Microsoft::WRL::ComPtr<IDWriteInlineObject> breadcrumbEllipsis;
    std::array<Microsoft::WRL::ComPtr<ID2D1PathGeometry>, palette_size> batches;
    std::array<Microsoft::WRL::ComPtr<ID2D1PathGeometry>, palette_size> transitionSourceBatches;
    std::array<Microsoft::WRL::ComPtr<ID2D1PathGeometry>, palette_size> transitionDestinationBatches;
    Microsoft::WRL::ComPtr<ID2D1PathGeometry> hoverPulseGeometry;
    std::size_t geometryRevision = 0U;
    core::SunburstGeometry geometry{};
    bool geometryValid = false;
    bool transitionGeometryValid = false;
    core::NodeIndex hoverPulseNode = core::invalid_node;
    core::SunburstGeometry hoverPulseChartGeometry{};
};

AnalyzerView::AnalyzerView() = default;
AnalyzerView::~AnalyzerView() = default;

void AnalyzerView::set_tree(const core::ScanTree* tree, const core::NodeIndex root) {
    (void)cancel_drag();
    cancel_transition();
    tree_ = tree;
    root_ = root;
    selectedNode_ = root;
    hoveredSegment_.reset();
    hoveredChild_.reset();
    hoverPulse_.clear();
    pendingCommand_.reset();
    hoveredBreadcrumbItem_.reset();
    pressedBreadcrumbItem_.reset();
    breadcrumbOverflowOpen_ = false;
    if (tree == nullptr) {
        set_breadcrumb({});
        set_history_availability(false, false);
    }
    rebuild_layout();
}

bool AnalyzerView::set_root(const core::NodeIndex root) {
    if (tree_ == nullptr || root >= tree_->nodes().size()
        || !core::has_flag(tree_->node(root).flags, core::ScanNodeFlags::Directory)) {
        return false;
    }
    (void)cancel_drag();
    cancel_transition();
    root_ = root;
    selectedNode_ = root;
    hoveredSegment_.reset();
    hoveredChild_.reset();
    hoverPulse_.clear();
    hoveredBreadcrumbItem_.reset();
    pressedBreadcrumbItem_.reset();
    breadcrumbOverflowOpen_ = false;
    rebuild_layout();
    return true;
}

bool AnalyzerView::navigate_to_root(
    const core::NodeIndex root,
    const std::chrono::steady_clock::time_point now,
    const bool animationsEnabled) {
    if (tree_ == nullptr || root >= tree_->nodes().size()
        || !core::has_flag(tree_->node(root).flags, core::ScanNodeFlags::Directory)) {
        return false;
    }
    if (root == root_) {
        selectedNode_ = root;
        return true;
    }
    if (!animationsEnabled
        || layout_.header.right <= 0.0F
        || layout_.actionBar.bottom <= 0.0F) {
        return set_root(root);
    }

    (void)cancel_drag();
    (void)dismiss_breadcrumb_overflow();
    const auto destination = core::build_sunburst_layout(*tree_, root, {});
    const auto destinationLayout = compute_analyzer_layout(
        layout_.header.right,
        layout_.actionBar.bottom,
        destination.depthRanges.size());
    if (transitionController_.active()) {
        transitionPlan_ = core::build_sunburst_transition(
            core::snapshot_transition_frame(transitionFrame_),
            destination,
            destinationLayout.chartGeometry,
            root);
    } else {
        transitionPlan_ = core::build_sunburst_transition(
            sunburst_,
            layout_.chartGeometry,
            destination,
            destinationLayout.chartGeometry,
            root);
    }

    transitionSourceRoot_ = root_;
    transitionSourceRankedChildren_ = rankedChildren_;
    transitionSourcePaletteIndices_ = childPaletteIndices_;
    transitionSourceChildListLayout_ = compute_analyzer_child_list_layout(
        layout_.detailsBounds,
        transitionSourceRankedChildren_.size(),
        childScrollOffset_);
    root_ = root;
    selectedNode_ = root;
    hoveredSegment_.reset();
    hoveredChild_.reset();
    hoverPulse_.clear();
    reviewPanelOpen_ = false;
    rebuild_layout();
    transitionFrame_.reserve(transitionPlan_.morphs.size());
    core::interpolate_sunburst_transition(transitionPlan_, 0.0F, transitionFrame_);
    transitionCenterX_ = destinationLayout.chartGeometry.centerX;
    transitionCenterY_ = destinationLayout.chartGeometry.centerY;
    transitionTargetGeometry_ = destinationLayout.chartGeometry;
    transitionLinearProgress_ = 0.0F;
    transitionController_.start(now, !transitionPlan_.morphs.empty());
    if (resources_ != nullptr) {
        resources_->transitionGeometryValid = false;
    }
    return true;
}

bool AnalyzerView::advance_transition(
    const std::chrono::steady_clock::time_point now) noexcept {
    if (!transitionController_.active()) {
        return false;
    }
    const auto state = transitionController_.advance(now);
    transitionLinearProgress_ = state.linearProgress;
    if (state.active) {
        core::interpolate_sunburst_transition(
            transitionPlan_,
            state.linearProgress,
            transitionFrame_);
    } else {
        transitionPlan_.morphs.clear();
        transitionSourceRankedChildren_.clear();
        transitionSourcePaletteIndices_.clear();
        transitionSourceChildListLayout_ = {};
        transitionSourceRoot_ = core::invalid_node;
    }
    if (resources_ != nullptr) {
        resources_->transitionGeometryValid = false;
    }
    return true;
}

bool AnalyzerView::transition_active() const noexcept {
    return transitionController_.active();
}

void AnalyzerView::set_hover_animations_enabled(const bool enabled) noexcept {
    hoverAnimationsEnabled_ = enabled;
}

bool AnalyzerView::hover_pulse_active() const noexcept {
    return hoverPulse_.has_target();
}

bool AnalyzerView::hover_pulse_timer_required() const noexcept {
    return hoverPulse_.timer_required(hoverAnimationsEnabled_)
        && !transitionController_.active();
}

void AnalyzerView::cancel_transition() noexcept {
    (void)transitionController_.cancel();
    transitionPlan_.morphs.clear();
    transitionSourceRankedChildren_.clear();
    transitionSourcePaletteIndices_.clear();
    transitionSourceChildListLayout_ = {};
    transitionSourceRoot_ = core::invalid_node;
    transitionLinearProgress_ = 1.0F;
    if (resources_ != nullptr) {
        resources_->transitionGeometryValid = false;
    }
}

void AnalyzerView::set_breadcrumb(AnalyzerBreadcrumbModel model) {
    breadcrumb_ = std::move(model);
    breadcrumbLayout_ = {};
    breadcrumbLabelWidths_.clear();
    breadcrumbFlyoutRows_.clear();
    breadcrumbFlyoutBounds_ = {};
    hoveredBreadcrumbItem_.reset();
    pressedBreadcrumbItem_.reset();
    breadcrumbEllipsisHovered_ = false;
    breadcrumbOverflowOpen_ = false;
    breadcrumbLayoutWidth_ = -1.0F;
}

void AnalyzerView::set_history_availability(
    const bool canBack,
    const bool canForward) noexcept {
    canNavigateBack_ = canBack;
    canNavigateForward_ = canForward;
}

std::wstring_view AnalyzerView::hovered_breadcrumb_path() const noexcept {
    if (!hoveredBreadcrumbItem_.has_value()
        || *hoveredBreadcrumbItem_ >= breadcrumb_.items.size()) {
        return {};
    }
    return breadcrumb_.items[*hoveredBreadcrumbItem_].absolutePath;
}

bool AnalyzerView::dismiss_breadcrumb_overflow() noexcept {
    if (!breadcrumbOverflowOpen_) {
        return false;
    }
    breadcrumbOverflowOpen_ = false;
    breadcrumbFlyoutRows_.clear();
    breadcrumbFlyoutBounds_ = {};
    breadcrumbEllipsisHovered_ = false;
    hoveredBreadcrumbItem_.reset();
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
        cancel_transition();
        (void)dismiss_breadcrumb_overflow();
        hoveredChrome_ = AnalyzerHitTarget::None;
        hoveredChild_.reset();
        hoverPulse_.clear();
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
        || !makeBrush(theme.danger, resources->danger)
        || !makeBrush(theme.primaryText, resources->hoverPulse)) {
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
        || !createFormat(15.0F, resources->breadcrumbFormat)
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
    resources->breadcrumbFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    resources->breadcrumbFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
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
            resources->rowEllipsis.Get()))
        || FAILED(writeFactory->CreateEllipsisTrimmingSign(
            resources->breadcrumbFormat.Get(),
            &resources->breadcrumbEllipsis))
        || FAILED(resources->breadcrumbFormat->SetTrimming(
            &rowTrimming,
            resources->breadcrumbEllipsis.Get()))) {
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

bool AnalyzerView::ensure_breadcrumb_layout(
    render::GraphicsDevice& graphics,
    const core::Language language,
    const float clientHeightDip) {
    if (resources_ == nullptr) {
        return false;
    }
    const auto width = std::max(0.0F, layout_.breadcrumbBounds.right - layout_.breadcrumbBounds.left);
    if (breadcrumbLabelWidths_.size() != breadcrumb_.items.size()
        || breadcrumbLayoutWidth_ != width
        || breadcrumbLayoutLanguage_ != language) {
        auto* writeFactory = graphics.dwrite_factory();
        if (writeFactory == nullptr) {
            return false;
        }
        breadcrumbLabelWidths_.clear();
        breadcrumbLabelWidths_.reserve(breadcrumb_.items.size());
        for (std::size_t index = 0U; index < breadcrumb_.items.size(); ++index) {
            const auto label = breadcrumb_.items[index].kind == BreadcrumbItemKind::Overview
                ? core::get_string(language, core::StringId::DisksAndFolders)
                : std::wstring_view(breadcrumb_.items[index].label);
            Microsoft::WRL::ComPtr<IDWriteTextLayout> textLayout;
            if (FAILED(writeFactory->CreateTextLayout(
                    label.data(),
                    static_cast<UINT32>(label.size()),
                    resources_->breadcrumbFormat.Get(),
                    4'096.0F,
                    64.0F,
                    &textLayout))) {
                return false;
            }
            DWRITE_TEXT_METRICS metrics{};
            if (FAILED(textLayout->GetMetrics(&metrics))) {
                return false;
            }
            breadcrumbLabelWidths_.push_back(metrics.widthIncludingTrailingWhitespace);
        }
        breadcrumbLayout_ = layout_analyzer_breadcrumb(
            breadcrumb_,
            breadcrumbLabelWidths_,
            layout_.breadcrumbBounds);
        breadcrumbLayoutWidth_ = width;
        breadcrumbLayoutLanguage_ = language;
    }

    breadcrumbFlyoutRows_.clear();
    breadcrumbFlyoutBounds_ = {};
    if (!breadcrumbOverflowOpen_
        || !breadcrumbLayout_.ellipsis.has_value()
        || breadcrumbLayout_.hiddenItemIndices.empty()) {
        return true;
    }

    auto flyoutWidth = 220.0F;
    for (const auto index : breadcrumbLayout_.hiddenItemIndices) {
        if (index < breadcrumbLabelWidths_.size()) {
            flyoutWidth = std::max(flyoutWidth, breadcrumbLabelWidths_[index] + 32.0F);
        }
    }
    flyoutWidth = std::min(flyoutWidth, std::max(0.0F, layout_.header.right - 32.0F));
    const auto left = std::clamp(
        breadcrumbLayout_.ellipsis->bounds.left,
        16.0F,
        std::max(16.0F, layout_.header.right - 16.0F - flyoutWidth));
    const auto top = layout_.breadcrumbBounds.bottom;
    const auto availableHeight = std::max(0.0F, clientHeightDip - top - 16.0F);
    const auto preferredHeight = 34.0F
        * static_cast<float>(breadcrumbLayout_.hiddenItemIndices.size());
    const auto panelHeight = std::min(preferredHeight, availableHeight);
    const auto rowHeight = breadcrumbLayout_.hiddenItemIndices.empty()
        ? 0.0F
        : panelHeight / static_cast<float>(breadcrumbLayout_.hiddenItemIndices.size());
    breadcrumbFlyoutBounds_ = {left, top, left + flyoutWidth, top + panelHeight};
    breadcrumbFlyoutRows_.reserve(breadcrumbLayout_.hiddenItemIndices.size());
    for (std::size_t row = 0U; row < breadcrumbLayout_.hiddenItemIndices.size(); ++row) {
        const auto rowTop = top + rowHeight * static_cast<float>(row);
        breadcrumbFlyoutRows_.push_back({
            {left, rowTop, left + flyoutWidth, rowTop + rowHeight},
            breadcrumbLayout_.hiddenItemIndices[row],
        });
    }
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

bool AnalyzerView::ensure_transition_geometry() {
    if (resources_ == nullptr || !transitionController_.active()) {
        return false;
    }
    auto& resources = *resources_;
    if (resources.transitionGeometryValid) {
        return true;
    }
    Microsoft::WRL::ComPtr<ID2D1Factory> factory;
    resources.context->GetFactory(&factory);
    if (factory == nullptr) {
        return false;
    }
    const auto buildBatch = [&](auto& batch, const std::size_t paletteIndex, const bool source) {
        batch.Reset();
        if (FAILED(factory->CreatePathGeometry(&batch))) {
            return false;
        }
        Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
        if (FAILED(batch->Open(&sink))) {
            return false;
        }
        sink->SetFillMode(D2D1_FILL_MODE_WINDING);
        for (const auto& segment : transitionFrame_.segments()) {
            const auto opacity = source ? segment.sourceOpacity : segment.destinationOpacity;
            const auto segmentPalette = source
                ? segment.sourcePaletteIndex
                : segment.destinationPaletteIndex;
            if (opacity > 0.0001F && segmentPalette == paletteIndex) {
                append_transition_segment(
                    sink.Get(),
                    transitionCenterX_,
                    transitionCenterY_,
                    segment);
            }
        }
        return SUCCEEDED(sink->Close());
    };
    for (std::size_t paletteIndex = 0U; paletteIndex < palette_size; ++paletteIndex) {
        if (!buildBatch(resources.transitionSourceBatches[paletteIndex], paletteIndex, true)
            || !buildBatch(
                resources.transitionDestinationBatches[paletteIndex],
                paletteIndex,
                false)) {
            return false;
        }
    }
    resources.transitionGeometryValid = true;
    return true;
}

bool AnalyzerView::ensure_hover_pulse_geometry() {
    if (resources_ == nullptr || !hoverPulse_.has_target()) {
        return false;
    }
    auto& resources = *resources_;
    const auto& geometry = layout_.chartGeometry;
    if (resources.hoverPulseGeometry != nullptr
        && resources.hoverPulseNode == hoverPulse_.target()
        && resources.hoverPulseChartGeometry.centerX == geometry.centerX
        && resources.hoverPulseChartGeometry.centerY == geometry.centerY
        && resources.hoverPulseChartGeometry.innerRadius == geometry.innerRadius
        && resources.hoverPulseChartGeometry.ringWidth == geometry.ringWidth) {
        return true;
    }
    const auto branch = find_hover_branch(sunburst_, hoverPulse_.target());
    if (!branch.has_value()) {
        return false;
    }
    Microsoft::WRL::ComPtr<ID2D1Factory> factory;
    resources.context->GetFactory(&factory);
    if (factory == nullptr
        || FAILED(factory->CreatePathGeometry(&resources.hoverPulseGeometry))) {
        return false;
    }
    Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(resources.hoverPulseGeometry->Open(&sink))) {
        resources.hoverPulseGeometry.Reset();
        return false;
    }
    sink->SetFillMode(D2D1_FILL_MODE_WINDING);
    for (const auto& segment : sunburst_.segments) {
        if (segment_is_in_hover_branch(segment, *branch)) {
            append_annular_segment(sink.Get(), geometry, segment);
        }
    }
    if (FAILED(sink->Close())) {
        resources.hoverPulseGeometry.Reset();
        return false;
    }
    resources.hoverPulseNode = hoverPulse_.target();
    resources.hoverPulseChartGeometry = geometry;
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
    if (!ensure_breadcrumb_layout(graphics, language, heightDip)) {
        return false;
    }
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
    auto drawingTransition = transitionController_.active();
    if (drawingTransition) {
        const auto& geometry = layout_.chartGeometry;
        if (geometry.centerX != transitionTargetGeometry_.centerX
            || geometry.centerY != transitionTargetGeometry_.centerY
            || geometry.innerRadius != transitionTargetGeometry_.innerRadius
            || geometry.ringWidth != transitionTargetGeometry_.ringWidth) {
            cancel_transition();
            drawingTransition = false;
        }
    }
    if (drawingTransition) {
        if (!ensure_transition_geometry()) {
            cancel_transition();
            drawingTransition = false;
        }
    }
    if (!drawingTransition && !ensure_geometry()) {
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
    if (hoveredChrome_ == AnalyzerHitTarget::Back && canNavigateBack_) {
        context->FillRoundedRectangle(
            D2D1::RoundedRect(to_d2d_rect(layout_.backButton), 5.0F, 5.0F),
            resources_->hover.Get());
    }
    if (hoveredChrome_ == AnalyzerHitTarget::Forward && canNavigateForward_) {
        context->FillRoundedRectangle(
            D2D1::RoundedRect(to_d2d_rect(layout_.forwardButton), 5.0F, 5.0F),
            resources_->hover.Get());
    }
    auto* backBrush = canNavigateBack_ ? resources_->primary.Get() : resources_->secondary.Get();
    const auto backCenterY = (layout_.backButton.top + layout_.backButton.bottom) * 0.5F;
    context->DrawLine(
        {layout_.backButton.left + 27.0F, backCenterY - 9.0F},
        {layout_.backButton.left + 18.0F, backCenterY},
        backBrush,
        2.0F);
    context->DrawLine(
        {layout_.backButton.left + 18.0F, backCenterY},
        {layout_.backButton.left + 27.0F, backCenterY + 9.0F},
        backBrush,
        2.0F);
    auto* forwardBrush = canNavigateForward_
        ? resources_->primary.Get()
        : resources_->secondary.Get();
    const auto forwardCenterY = (layout_.forwardButton.top + layout_.forwardButton.bottom) * 0.5F;
    context->DrawLine(
        {layout_.forwardButton.left + 15.0F, forwardCenterY - 9.0F},
        {layout_.forwardButton.left + 24.0F, forwardCenterY},
        forwardBrush,
        2.0F);
    context->DrawLine(
        {layout_.forwardButton.left + 24.0F, forwardCenterY},
        {layout_.forwardButton.left + 15.0F, forwardCenterY + 9.0F},
        forwardBrush,
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
    const auto drawBreadcrumbSegment = [&](const BreadcrumbSegmentLayout& segment) {
        if (segment.itemIndex >= breadcrumb_.items.size()) {
            return;
        }
        const auto& item = breadcrumb_.items[segment.itemIndex];
        const auto current = segment.itemIndex + 1U == breadcrumb_.items.size();
        const auto hovered = hoveredBreadcrumbItem_ == segment.itemIndex;
        auto* fill = hovered || pressedBreadcrumbItem_ == segment.itemIndex
            ? resources_->hover.Get()
            : resources_->surface.Get();
        context->FillRoundedRectangle(
            D2D1::RoundedRect(to_d2d_rect(segment.bounds), 4.0F, 4.0F),
            fill);
        context->DrawRoundedRectangle(
            D2D1::RoundedRect(to_d2d_rect(segment.bounds), 4.0F, 4.0F),
            current ? resources_->accent.Get() : resources_->border.Get(),
            1.0F);
        const auto label = item.kind == BreadcrumbItemKind::Overview
            ? core::get_string(language, core::StringId::DisksAndFolders)
            : std::wstring_view(item.label);
        auto textBounds = segment.bounds;
        textBounds.left += 12.0F;
        textBounds.right = std::max(textBounds.left, textBounds.right - 16.0F);
        drawText(
            label,
            resources_->breadcrumbFormat.Get(),
            textBounds,
            item.enabled ? resources_->primary.Get() : resources_->secondary.Get());
        const auto chevronX = segment.bounds.right - 8.0F;
        const auto centerY = (segment.bounds.top + segment.bounds.bottom) * 0.5F;
        context->DrawLine(
            {chevronX - 3.0F, centerY - 5.0F},
            {chevronX + 1.0F, centerY},
            resources_->border.Get(),
            1.0F);
        context->DrawLine(
            {chevronX + 1.0F, centerY},
            {chevronX - 3.0F, centerY + 5.0F},
            resources_->border.Get(),
            1.0F);
    };
    for (const auto& segment : breadcrumbLayout_.visible) {
        drawBreadcrumbSegment(segment);
    }
    if (breadcrumbLayout_.ellipsis.has_value()) {
        const auto& segment = *breadcrumbLayout_.ellipsis;
        context->FillRoundedRectangle(
            D2D1::RoundedRect(to_d2d_rect(segment.bounds), 4.0F, 4.0F),
            breadcrumbEllipsisHovered_ || breadcrumbOverflowOpen_
                ? resources_->hover.Get()
                : resources_->surface.Get());
        context->DrawRoundedRectangle(
            D2D1::RoundedRect(to_d2d_rect(segment.bounds), 4.0F, 4.0F),
            resources_->border.Get(),
            1.0F);
        drawText(
            L"...",
            resources_->breadcrumbFormat.Get(),
            segment.bounds,
            resources_->primary.Get());
    }

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
    } else {
        auto hintBounds = reviewLayout_.summary;
        hintBounds.left += 12.0F;
        hintBounds.right = std::max(hintBounds.left, hintBounds.right - 12.0F);
        drawText(
            core::get_string(language, core::StringId::CollectorDragHint),
            resources_->rowNameFormat.Get(),
            hintBounds,
            resources_->secondary.Get());
    }

    if (drawingTransition) {
        const auto eased = core::sunburst_transition_easing(transitionLinearProgress_);
        for (std::size_t index = 0U; index < palette_size; ++index) {
            resources_->palette[index]->SetOpacity(1.0F - eased);
            context->FillGeometry(
                resources_->transitionSourceBatches[index].Get(),
                resources_->palette[index].Get());
            resources_->palette[index]->SetOpacity(eased);
            context->FillGeometry(
                resources_->transitionDestinationBatches[index].Get(),
                resources_->palette[index].Get());
            resources_->palette[index]->SetOpacity(1.0F);
        }
    } else {
        for (std::size_t index = 0U; index < palette_size; ++index) {
            context->FillGeometry(resources_->batches[index].Get(), resources_->palette[index].Get());
        }
        if (hoverPulse_.has_target() && ensure_hover_pulse_geometry()) {
            resources_->hoverPulse->SetOpacity(hoverPulse_.opacity(
                std::chrono::steady_clock::now(),
                hoverAnimationsEnabled_));
            context->FillGeometry(
                resources_->hoverPulseGeometry.Get(),
                resources_->hoverPulse.Get());
            resources_->hoverPulse->SetOpacity(1.0F);
        }
    }
    auto centerRadius = layout_.chartGeometry.innerRadius - radial_gap;
    if (drawingTransition && !transitionFrame_.segments().empty()) {
        centerRadius = std::ranges::min(
            transitionFrame_.segments(),
            {},
            &core::TransitionDrawSegment::innerRadius).innerRadius - radial_gap;
    }
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
    const auto setDetailOpacity = [&](const float opacity) {
        resources_->primary->SetOpacity(opacity);
        resources_->secondary->SetOpacity(opacity);
        resources_->hover->SetOpacity(opacity);
        for (auto& brush : resources_->palette) {
            brush->SetOpacity(opacity);
        }
    };
    const auto drawDetails = [&](
        const core::NodeIndex detailRoot,
        const std::vector<core::RankedChild>& ranked,
        const std::vector<std::uint8_t>& palettes,
        const AnalyzerChildListLayout& rows,
        const float opacity,
        const bool destination) {
        setDetailOpacity(opacity);
        const auto rootSizeText = format_bytes(tree_->node(detailRoot).logicalSize);
        drawText(
            rootSizeText,
            resources_->centerFormat.Get(),
            centerBounds,
            resources_->primary.Get());
        drawText(
            tree_->name(detailRoot),
            resources_->detailHeadingFormat.Get(),
            nameBounds,
            resources_->primary.Get());
        const auto sizeText = format_bytes(tree_->node(detailRoot).logicalSize);
        drawText(sizeText, resources_->detailFormat.Get(), sizeBounds, resources_->secondary.Get());
        const auto itemCount = tree_->node(detailRoot).fileCount
            + tree_->node(detailRoot).directoryCount;
        if (itemCount > 0U) {
            auto countText = std::to_wstring(itemCount);
            countText.append(L" ");
            countText.append(core::get_string(language, core::StringId::Items));
            drawText(
                countText,
                resources_->detailFormat.Get(),
                countBounds,
                resources_->secondary.Get());
        }
        for (const auto& row : rows.rows) {
            if (row.itemIndex >= ranked.size() || row.itemIndex >= palettes.size()) {
                continue;
            }
            const auto node = ranked[row.itemIndex].node;
            if (destination
                && ((hoveredChild_.has_value() && *hoveredChild_ == row.itemIndex)
                    || selectedNode_ == node)) {
                context->FillRectangle(to_d2d_rect(row.bounds), resources_->hover.Get());
            }
            const auto dotCenter = D2D1::Point2F(
                row.bounds.left + 9.0F,
                (row.bounds.top + row.bounds.bottom) * 0.5F);
            context->FillEllipse(
                D2D1::Ellipse(dotCenter, 4.5F, 4.5F),
                resources_->palette[palettes[row.itemIndex]].Get());
            drawText(
                tree_->name(node),
                resources_->rowNameFormat.Get(),
                row.nameBounds,
                resources_->primary.Get());
            const auto childSize = format_bytes(ranked[row.itemIndex].logicalSize);
            drawText(
                childSize,
                resources_->rowSizeFormat.Get(),
                row.sizeBounds,
                resources_->secondary.Get());
        }
        setDetailOpacity(1.0F);
    };
    if (drawingTransition
        && transitionSourceRoot_ < tree_->nodes().size()) {
        const auto sourceOpacity = 1.0F
            - std::clamp(transitionLinearProgress_ / 0.45F, 0.0F, 1.0F);
        const auto destinationOpacity = std::clamp(
            (transitionLinearProgress_ - 0.30F) / 0.70F,
            0.0F,
            1.0F);
        if (sourceOpacity > 0.0F) {
            drawDetails(
                transitionSourceRoot_,
                transitionSourceRankedChildren_,
                transitionSourcePaletteIndices_,
                transitionSourceChildListLayout_,
                sourceOpacity,
                false);
        }
        if (destinationOpacity > 0.0F) {
            drawDetails(
                root_,
                rankedChildren_,
                childPaletteIndices_,
                childListLayout_,
                destinationOpacity,
                true);
        }
    } else {
        drawDetails(
            root_,
            rankedChildren_,
            childPaletteIndices_,
            childListLayout_,
            1.0F,
            true);
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
    if (breadcrumbOverflowOpen_ && !breadcrumbFlyoutRows_.empty()) {
        context->FillRoundedRectangle(
            D2D1::RoundedRect(to_d2d_rect(breadcrumbFlyoutBounds_), 5.0F, 5.0F),
            resources_->panel.Get());
        context->DrawRoundedRectangle(
            D2D1::RoundedRect(to_d2d_rect(breadcrumbFlyoutBounds_), 5.0F, 5.0F),
            resources_->border.Get(),
            1.0F);
        for (const auto& row : breadcrumbFlyoutRows_) {
            if (row.itemIndex >= breadcrumb_.items.size()) {
                continue;
            }
            const auto& item = breadcrumb_.items[row.itemIndex];
            if (hoveredBreadcrumbItem_ == row.itemIndex) {
                context->FillRectangle(to_d2d_rect(row.bounds), resources_->hover.Get());
            }
            auto textBounds = row.bounds;
            textBounds.left += 14.0F;
            textBounds.right -= 14.0F;
            drawText(
                item.label,
                resources_->breadcrumbFormat.Get(),
                textBounds,
                item.enabled ? resources_->primary.Get() : resources_->secondary.Get());
        }
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
    auto overflowChanged = false;
    std::optional<std::size_t> nextBreadcrumbItem;
    auto nextEllipsisHovered = false;
    auto pointerOverFlyout = false;
    if (breadcrumbOverflowOpen_) {
        for (const auto& row : breadcrumbFlyoutRows_) {
            if (contains(row.bounds, xDip, yDip)) {
                pointerOverFlyout = true;
                nextBreadcrumbItem = row.itemIndex;
                break;
            }
        }
    }
    if (!pointerOverFlyout) {
        const auto breadcrumbHit = hit_test_analyzer_breadcrumb(
            breadcrumbLayout_,
            xDip,
            yDip);
        if (breadcrumbHit.has_value()) {
            if (breadcrumbHit->kind == BreadcrumbHitKind::Item) {
                nextBreadcrumbItem = breadcrumbHit->itemIndex;
            } else {
                nextEllipsisHovered = true;
            }
        }
    }
    if (breadcrumbOverflowOpen_
        && !breadcrumbFlyoutRows_.empty()
        && !contains(layout_.breadcrumbBounds, xDip, yDip)
        && !contains(breadcrumbFlyoutBounds_, xDip, yDip)) {
        overflowChanged = dismiss_breadcrumb_overflow();
        pointerOverFlyout = false;
        nextBreadcrumbItem.reset();
        nextEllipsisHovered = false;
    }
    const auto pointerOverBreadcrumb = contains(layout_.breadcrumbBounds, xDip, yDip)
        || pointerOverFlyout;
    const auto pointerOverReviewPanel = reviewPanelOpen_
        && contains_point(reviewLayout_.panel, xDip, yDip);
    const auto nextReviewPanelOpen = !recycleInProgress_ && !reviewNodes_.empty()
        && !transitionController_.active()
        && !pointerOverBreadcrumb
        && (contains_point(reviewLayout_.summary, xDip, yDip)
            || pointerOverReviewPanel);
    const auto target = hit_test_analyzer_layout(layout_, xDip, yDip);
    const auto nextChild = pointerOverReviewPanel || pointerOverBreadcrumb
            || transitionController_.active()
        ? std::optional<std::size_t>{}
        : hit_test_analyzer_child_rows(childListLayout_, xDip, yDip);
    const auto navigationChrome = (target == AnalyzerHitTarget::Back && canNavigateBack_)
            || (target == AnalyzerHitTarget::Forward && canNavigateForward_)
            || target == AnalyzerHitTarget::MinimizeWindow
            || target == AnalyzerHitTarget::MaximizeWindow
            || target == AnalyzerHitTarget::CloseWindow;
    const auto actionChrome = target == AnalyzerHitTarget::Preview
            || target == AnalyzerHitTarget::Reveal
            || target == AnalyzerHitTarget::AddToReview
            || target == AnalyzerHitTarget::ReviewDelete;
    const auto nextChrome = navigationChrome
            || (!transitionController_.active() && actionChrome)
        ? target
        : AnalyzerHitTarget::None;
    std::optional<core::SunburstHit> nextSegment;
    if (!transitionController_.active()
        && !pointerOverReviewPanel && !pointerOverBreadcrumb && !nextChild.has_value()
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
    auto nextPulseNode = core::invalid_node;
    if (nextChild.has_value() && *nextChild < rankedChildren_.size()) {
        const auto node = rankedChildren_[*nextChild].node;
        if (find_hover_branch(sunburst_, node).has_value()) {
            nextPulseNode = node;
        }
    }
    if (hoveredChrome_ == nextChrome && sameSegment && hoveredChild_ == nextChild
        && hoverPulse_.target() == nextPulseNode
        && hoveredBreadcrumbItem_ == nextBreadcrumbItem
        && breadcrumbEllipsisHovered_ == nextEllipsisHovered
        && reviewPanelOpen_ == nextReviewPanelOpen) {
        return dragChanged || overflowChanged;
    }
    hoveredChrome_ = nextChrome;
    hoveredSegment_ = nextSegment;
    hoveredChild_ = nextChild;
    if (nextPulseNode == core::invalid_node) {
        hoverPulse_.clear();
    } else {
        hoverPulse_.set_target(nextPulseNode, std::chrono::steady_clock::now());
    }
    hoveredBreadcrumbItem_ = nextBreadcrumbItem;
    breadcrumbEllipsisHovered_ = nextEllipsisHovered;
    reviewPanelOpen_ = nextReviewPanelOpen;
    return true;
}

bool AnalyzerView::pointer_left() {
    const auto dragChanged = cancel_drag();
    if (hoveredChrome_ == AnalyzerHitTarget::None
        && !hoveredSegment_.has_value()
        && !hoveredChild_.has_value()
        && !hoveredBreadcrumbItem_.has_value()
        && !breadcrumbEllipsisHovered_
        && !breadcrumbOverflowOpen_
        && !reviewPanelOpen_) {
        return dragChanged;
    }
    hoveredChrome_ = AnalyzerHitTarget::None;
    hoveredSegment_.reset();
    hoveredChild_.reset();
    hoverPulse_.clear();
    hoveredBreadcrumbItem_.reset();
    pressedBreadcrumbItem_.reset();
    breadcrumbEllipsisHovered_ = false;
    breadcrumbOverflowOpen_ = false;
    breadcrumbFlyoutRows_.clear();
    breadcrumbFlyoutBounds_ = {};
    reviewPanelOpen_ = false;
    return true;
}

bool AnalyzerView::scroll_children(const int deltaRows) noexcept {
    if (transitionController_.active()) {
        return false;
    }
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
    hoverPulse_.clear();
    return true;
}

bool AnalyzerView::scroll_review(const int deltaRows) noexcept {
    if (transitionController_.active() || !reviewPanelOpen_) {
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
    const auto breadcrumbHit = hit_test_analyzer_breadcrumb(
        breadcrumbLayout_,
        xDip,
        yDip);
    if (breadcrumbHit.has_value()) {
        pressedBreadcrumbItem_ = breadcrumbHit->kind == BreadcrumbHitKind::Item
            ? std::optional<std::size_t>{breadcrumbHit->itemIndex}
            : std::nullopt;
        return;
    }
    for (const auto& row : breadcrumbFlyoutRows_) {
        if (contains(row.bounds, xDip, yDip)) {
            pressedBreadcrumbItem_ = row.itemIndex;
            return;
        }
    }
    const auto chrome = hit_test_analyzer_layout(layout_, xDip, yDip);
    if (chrome == AnalyzerHitTarget::Back || chrome == AnalyzerHitTarget::Forward) {
        return;
    }
    if (transitionController_.active()) {
        return;
    }
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
        pressedBreadcrumbItem_.reset();
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
    const auto target = hit_test_analyzer_layout(layout_, xDip, yDip);
    if (target == AnalyzerHitTarget::Back) {
        pendingCommand_ = canNavigateBack_
            ? std::optional<AnalyzerCommand>(
                AnalyzerCommand{AnalyzerCommandKind::NavigateBack, core::invalid_node})
            : std::nullopt;
        return;
    }
    if (target == AnalyzerHitTarget::Forward) {
        pendingCommand_ = canNavigateForward_
            ? std::optional<AnalyzerCommand>(
                AnalyzerCommand{AnalyzerCommandKind::NavigateForward, core::invalid_node})
            : std::nullopt;
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
    auto activateBreadcrumb = [&](const std::size_t itemIndex) {
        if (itemIndex >= breadcrumb_.items.size()) {
            pendingCommand_.reset();
            return;
        }
        const auto& item = breadcrumb_.items[itemIndex];
        if (!item.enabled) {
            pendingCommand_.reset();
        } else if (item.kind == BreadcrumbItemKind::Overview) {
            pendingCommand_ = AnalyzerCommand{
                AnalyzerCommandKind::ReturnToOverview,
                core::invalid_node,
            };
            breadcrumbOverflowOpen_ = false;
        } else if (item.kind == BreadcrumbItemKind::ScanNode) {
            pendingCommand_ = AnalyzerCommand{
                AnalyzerCommandKind::NavigateBreadcrumb,
                item.node,
            };
            breadcrumbOverflowOpen_ = false;
        } else {
            pendingCommand_.reset();
        }
    };
    for (const auto& row : breadcrumbFlyoutRows_) {
        if (contains(row.bounds, xDip, yDip)) {
            activateBreadcrumb(row.itemIndex);
            return;
        }
    }
    if (const auto breadcrumbHit = hit_test_analyzer_breadcrumb(
            breadcrumbLayout_,
            xDip,
            yDip);
        breadcrumbHit.has_value()) {
        if (breadcrumbHit->kind == BreadcrumbHitKind::Ellipsis) {
            breadcrumbOverflowOpen_ = !breadcrumbOverflowOpen_;
            hoveredBreadcrumbItem_.reset();
            pendingCommand_ = AnalyzerCommand{
                AnalyzerCommandKind::OpenBreadcrumbOverflow,
                core::invalid_node,
            };
        } else {
            activateBreadcrumb(breadcrumbHit->itemIndex);
        }
        return;
    }
    if (breadcrumbOverflowOpen_) {
        (void)dismiss_breadcrumb_overflow();
        pendingCommand_.reset();
        return;
    }
    if (transitionController_.active()) {
        pendingCommand_.reset();
        return;
    }
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
