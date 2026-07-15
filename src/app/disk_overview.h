#pragma once

#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/language.h"
#include "core/theme.h"
#include "app/scan_ui_state.h"
#include "platform/windows/volume_service.h"

namespace diskbloom::render {
class GraphicsDevice;
}

namespace diskbloom::app {

struct RectF {
    float left;
    float top;
    float right;
    float bottom;
};

struct VolumeRowLayout {
    RectF bounds;
    RectF iconBounds;
    RectF textBounds;
    RectF progressTrack;
    RectF scanButton;
};

struct OverviewLayout {
    RectF titleBar;
    RectF minimizeButton;
    RectF maximizeButton;
    RectF closeButton;
    RectF bottomBar;
    RectF scanFolderButton;
    RectF settingsButton;
    std::vector<VolumeRowLayout> rows;
};

enum class OverviewCommandKind {
    ScanVolume,
    ScanFolder,
    OpenSettings,
    MinimizeWindow,
    ToggleMaximizeWindow,
    CloseWindow,
};

struct OverviewCommand {
    OverviewCommandKind kind;
    std::size_t volumeIndex = std::numeric_limits<std::size_t>::max();
};

[[nodiscard]] OverviewLayout compute_overview_layout(
    float widthDip,
    float heightDip,
    std::size_t volumeCount);

[[nodiscard]] std::optional<OverviewCommand> hit_test_overview(
    const OverviewLayout& layout,
    float xDip,
    float yDip) noexcept;

class DiskOverview final {
public:
    DiskOverview();
    ~DiskOverview();

    DiskOverview(const DiskOverview&) = delete;
    DiskOverview& operator=(const DiskOverview&) = delete;

    void set_volumes(std::vector<platform::windows::VolumeSnapshot> volumes);
    void set_scan_statuses(const std::vector<VolumeScanStatus>& statuses);
    [[nodiscard]] bool draw(
        render::GraphicsDevice& graphics,
        const core::ThemeTokens& theme,
        core::Language language,
        float widthDip,
        float heightDip);

    [[nodiscard]] bool pointer_moved(float xDip, float yDip);
    [[nodiscard]] bool pointer_left();
    void pointer_pressed(float xDip, float yDip);
    [[nodiscard]] std::optional<OverviewCommand> take_command() noexcept;

private:
    struct Resources;

    [[nodiscard]] bool ensure_resources(
        render::GraphicsDevice& graphics,
        const core::ThemeTokens& theme,
        core::Language language);

    std::vector<platform::windows::VolumeSnapshot> volumes_;
    std::vector<VolumeScanStatus> scan_statuses_;
    std::vector<std::wstring> detail_text_;
    core::Language detail_language_ = core::Language::English;
    bool detail_text_valid_ = false;
    OverviewLayout layout_{};
    std::optional<OverviewCommand> hovered_command_;
    std::optional<OverviewCommand> pending_command_;
    std::unique_ptr<Resources> resources_;
};

} // namespace diskbloom::app
