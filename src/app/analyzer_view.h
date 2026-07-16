#pragma once

#include "app/analyzer_breadcrumb.h"
#include "app/analyzer_geometry.h"
#include "app/analyzer_hover_pulse.h"
#include "app/analyzer_transition_controller.h"
#include "app/review_collector_interaction.h"
#include "core/language.h"
#include "core/child_ranking.h"
#include "core/scan_tree.h"
#include "core/sunburst_layout.h"
#include "core/sunburst_transition.h"
#include "core/theme.h"

#include <cstddef>
#include <chrono>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace diskbloom::render {
class GraphicsDevice;
}

namespace diskbloom::app {

struct AnalyzerLayout {
    AnalyzerRectF header;
    AnalyzerRectF backButton;
    AnalyzerRectF forwardButton;
    AnalyzerRectF breadcrumbBounds;
    AnalyzerRectF minimizeButton;
    AnalyzerRectF maximizeButton;
    AnalyzerRectF closeButton;
    AnalyzerRectF titleBounds;
    AnalyzerRectF chartBounds;
    AnalyzerRectF detailsBounds;
    AnalyzerRectF actionBar;
    AnalyzerRectF reviewButton;
    AnalyzerRectF addReviewButton;
    AnalyzerRectF previewButton;
    AnalyzerRectF revealButton;
    core::SunburstGeometry chartGeometry;
};

struct AnalyzerChildRowLayout {
    AnalyzerRectF bounds;
    AnalyzerRectF nameBounds;
    AnalyzerRectF sizeBounds;
    std::size_t itemIndex = 0U;
};

struct AnalyzerChildListLayout {
    std::vector<AnalyzerChildRowLayout> rows;
    std::size_t visibleCapacity = 0U;
    std::size_t scrollOffset = 0U;
};

[[nodiscard]] AnalyzerChildListLayout compute_analyzer_child_list_layout(
    const AnalyzerRectF& detailsBounds,
    std::size_t itemCount,
    std::size_t scrollOffset);

[[nodiscard]] std::optional<std::size_t> hit_test_analyzer_child_rows(
    const AnalyzerChildListLayout& layout,
    float xDip,
    float yDip) noexcept;

[[nodiscard]] std::size_t next_child_scroll_offset(
    std::size_t current,
    std::size_t itemCount,
    std::size_t visibleCount,
    int deltaRows) noexcept;

enum class AnalyzerHitTarget {
    None,
    Back,
    Forward,
    Chart,
    MinimizeWindow,
    MaximizeWindow,
    CloseWindow,
    Preview,
    Reveal,
    AddToReview,
    ReviewDelete,
};

[[nodiscard]] AnalyzerLayout compute_analyzer_layout(
    float widthDip,
    float heightDip,
    std::size_t depthCount) noexcept;

[[nodiscard]] AnalyzerHitTarget hit_test_analyzer_layout(
    const AnalyzerLayout& layout,
    float xDip,
    float yDip) noexcept;

enum class AnalyzerCommandKind {
    ReturnToOverview,
    NavigateBack,
    NavigateForward,
    NavigateBreadcrumb,
    OpenBreadcrumbOverflow,
    NavigateToNode,
    NavigateToParent,
    SelectNode,
    MinimizeWindow,
    ToggleMaximizeWindow,
    CloseWindow,
    PreviewNode,
    RevealNode,
    AddToReview,
    ConfirmReview,
};

struct AnalyzerCommand {
    AnalyzerCommandKind kind = AnalyzerCommandKind::ReturnToOverview;
    core::NodeIndex node = core::invalid_node;
};

class AnalyzerView final {
public:
    AnalyzerView();
    ~AnalyzerView();

    AnalyzerView(const AnalyzerView&) = delete;
    AnalyzerView& operator=(const AnalyzerView&) = delete;

    void set_tree(const core::ScanTree* tree, core::NodeIndex root);
    [[nodiscard]] bool set_root(core::NodeIndex root);
    [[nodiscard]] bool navigate_to_root(
        core::NodeIndex root,
        std::chrono::steady_clock::time_point now,
        bool animationsEnabled);
    [[nodiscard]] bool advance_transition(
        std::chrono::steady_clock::time_point now) noexcept;
    [[nodiscard]] bool transition_active() const noexcept;
    void cancel_transition() noexcept;
    void set_hover_animations_enabled(bool enabled) noexcept;
    [[nodiscard]] bool hover_pulse_active() const noexcept;
    [[nodiscard]] bool hover_pulse_timer_required() const noexcept;
    void set_breadcrumb(AnalyzerBreadcrumbModel model);
    void set_history_availability(bool canBack, bool canForward) noexcept;
    [[nodiscard]] std::wstring_view hovered_breadcrumb_path() const noexcept;
    [[nodiscard]] bool dismiss_breadcrumb_overflow() noexcept;
    void set_selected_node(core::NodeIndex node) noexcept;
    void set_review_summary(std::size_t itemCount, std::uint64_t totalBytes) noexcept;
    void set_review_nodes(std::span<const core::NodeIndex> nodes);
    void set_recycle_in_progress(bool inProgress) noexcept;
    [[nodiscard]] core::NodeIndex current_root() const noexcept;

    [[nodiscard]] bool draw(
        render::GraphicsDevice& graphics,
        const core::ThemeTokens& theme,
        core::Language language,
        float widthDip,
        float heightDip);

    [[nodiscard]] bool pointer_moved(float xDip, float yDip);
    [[nodiscard]] bool pointer_left();
    [[nodiscard]] bool scroll_children(int deltaRows) noexcept;
    [[nodiscard]] bool scroll_review(int deltaRows) noexcept;
    [[nodiscard]] bool scroll_at(float xDip, float yDip, int deltaRows) noexcept;
    void pointer_down(float xDip, float yDip);
    void pointer_released(float xDip, float yDip);
    [[nodiscard]] bool cancel_drag() noexcept;
    [[nodiscard]] bool drag_pending() const noexcept;
    [[nodiscard]] bool drag_active() const noexcept;
    void pointer_pressed(float xDip, float yDip);
    [[nodiscard]] std::optional<AnalyzerCommand> take_command() noexcept;

private:
    struct Resources;

    [[nodiscard]] bool ensure_resources(
        render::GraphicsDevice& graphics,
        const core::ThemeTokens& theme,
        core::Language language);
    [[nodiscard]] bool ensure_geometry();
    [[nodiscard]] bool ensure_hover_pulse_geometry();
    [[nodiscard]] bool ensure_transition_geometry();
    [[nodiscard]] bool ensure_breadcrumb_layout(
        render::GraphicsDevice& graphics,
        core::Language language,
        float clientHeightDip);
    void rebuild_layout();

    const core::ScanTree* tree_ = nullptr;
    core::NodeIndex root_ = core::invalid_node;
    core::NodeIndex selectedNode_ = core::invalid_node;
    core::SunburstLayout sunburst_;
    core::SunburstTransitionPlan transitionPlan_;
    core::SunburstTransitionFrame transitionFrame_;
    AnalyzerTransitionController transitionController_;
    AnalyzerHoverPulse hoverPulse_;
    bool hoverAnimationsEnabled_ = true;
    float transitionCenterX_ = 0.0F;
    float transitionCenterY_ = 0.0F;
    core::SunburstGeometry transitionTargetGeometry_{};
    float transitionLinearProgress_ = 1.0F;
    core::NodeIndex transitionSourceRoot_ = core::invalid_node;
    std::vector<core::RankedChild> transitionSourceRankedChildren_;
    std::vector<std::uint8_t> transitionSourcePaletteIndices_;
    AnalyzerChildListLayout transitionSourceChildListLayout_{};
    std::vector<core::RankedChild> rankedChildren_;
    std::vector<std::uint8_t> childPaletteIndices_;
    AnalyzerLayout layout_{};
    AnalyzerBreadcrumbModel breadcrumb_;
    AnalyzerBreadcrumbLayout breadcrumbLayout_;
    std::vector<float> breadcrumbLabelWidths_;
    std::vector<BreadcrumbSegmentLayout> breadcrumbFlyoutRows_;
    AnalyzerRectF breadcrumbFlyoutBounds_{};
    std::optional<std::size_t> hoveredBreadcrumbItem_;
    std::optional<std::size_t> pressedBreadcrumbItem_;
    float breadcrumbLayoutWidth_ = -1.0F;
    core::Language breadcrumbLayoutLanguage_ = core::Language::English;
    bool breadcrumbEllipsisHovered_ = false;
    bool breadcrumbOverflowOpen_ = false;
    bool canNavigateBack_ = false;
    bool canNavigateForward_ = false;
    AnalyzerChildListLayout childListLayout_{};
    std::optional<core::SunburstHit> hoveredSegment_;
    std::optional<std::size_t> hoveredChild_;
    std::size_t childScrollOffset_ = 0U;
    std::size_t reviewItemCount_ = 0U;
    std::uint64_t reviewTotalBytes_ = 0U;
    std::vector<core::NodeIndex> reviewNodes_;
    ReviewCollectorLayout reviewLayout_{};
    std::size_t reviewScrollOffset_ = 0U;
    bool reviewPanelOpen_ = false;
    bool recycleInProgress_ = false;
    ReviewDragState reviewDrag_;
    float dragPointerX_ = 0.0F;
    float dragPointerY_ = 0.0F;
    AnalyzerHitTarget hoveredChrome_ = AnalyzerHitTarget::None;
    std::optional<AnalyzerCommand> pendingCommand_;
    std::size_t layoutRevision_ = 0U;
    std::unique_ptr<Resources> resources_;
};

} // namespace diskbloom::app
