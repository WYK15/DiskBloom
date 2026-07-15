#pragma once

#include "core/language.h"
#include "core/scan_tree.h"
#include "core/sunburst_layout.h"
#include "core/theme.h"

#include <cstddef>
#include <memory>
#include <optional>

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
    core::SunburstGeometry chartGeometry;
};

enum class AnalyzerHitTarget {
    None,
    Back,
    Chart,
    MinimizeWindow,
    MaximizeWindow,
    CloseWindow,
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
    [[nodiscard]] core::NodeIndex current_root() const noexcept;

    [[nodiscard]] bool draw(
        render::GraphicsDevice& graphics,
        const core::ThemeTokens& theme,
        core::Language language,
        float widthDip,
        float heightDip);

    [[nodiscard]] bool pointer_moved(float xDip, float yDip);
    [[nodiscard]] bool pointer_left();
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
    AnalyzerLayout layout_{};
    std::optional<core::SunburstHit> hoveredSegment_;
    AnalyzerHitTarget hoveredChrome_ = AnalyzerHitTarget::None;
    std::optional<AnalyzerCommand> pendingCommand_;
    std::size_t layoutRevision_ = 0U;
    std::unique_ptr<Resources> resources_;
};

} // namespace diskbloom::app
