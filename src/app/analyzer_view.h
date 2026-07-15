#pragma once

#include "core/language.h"
#include "core/child_ranking.h"
#include "core/scan_tree.h"
#include "core/sunburst_layout.h"
#include "core/theme.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace diskbloom::render {
class GraphicsDevice;
}

namespace diskbloom::app {

struct AnalyzerRectF {
    float left = 0.0F;
    float top = 0.0F;
    float right = 0.0F;
    float bottom = 0.0F;
};

struct AnalyzerLayout {
    AnalyzerRectF header;
    AnalyzerRectF backButton;
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
    void set_selected_node(core::NodeIndex node) noexcept;
    void set_review_summary(std::size_t itemCount, std::uint64_t totalBytes) noexcept;
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
    void pointer_pressed(float xDip, float yDip);
    [[nodiscard]] std::optional<AnalyzerCommand> take_command() noexcept;

private:
    struct Resources;

    [[nodiscard]] bool ensure_resources(
        render::GraphicsDevice& graphics,
        const core::ThemeTokens& theme,
        core::Language language);
    [[nodiscard]] bool ensure_geometry();
    void rebuild_layout();

    const core::ScanTree* tree_ = nullptr;
    core::NodeIndex root_ = core::invalid_node;
    core::NodeIndex selectedNode_ = core::invalid_node;
    core::SunburstLayout sunburst_;
    std::vector<core::RankedChild> rankedChildren_;
    std::vector<std::uint8_t> childPaletteIndices_;
    AnalyzerLayout layout_{};
    AnalyzerChildListLayout childListLayout_{};
    std::optional<core::SunburstHit> hoveredSegment_;
    std::optional<std::size_t> hoveredChild_;
    std::size_t childScrollOffset_ = 0U;
    std::size_t reviewItemCount_ = 0U;
    std::uint64_t reviewTotalBytes_ = 0U;
    bool recycleInProgress_ = false;
    AnalyzerHitTarget hoveredChrome_ = AnalyzerHitTarget::None;
    std::optional<AnalyzerCommand> pendingCommand_;
    std::size_t layoutRevision_ = 0U;
    std::unique_ptr<Resources> resources_;
};

} // namespace diskbloom::app
