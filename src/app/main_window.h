#pragma once

#include "app/appearance_settings.h"
#include "app/analyzer_navigation.h"
#include "app/analyzer_view.h"
#include "app/disk_overview.h"
#include "app/deletion_review.h"
#include "app/scan_ui_state.h"
#include "core/language.h"
#include "core/theme.h"
#include "render/graphics_device.h"
#include "scan/scan_session.h"

#include <Windows.h>

#include <memory>
#include <optional>
#include <vector>

namespace diskbloom::app {

class MainWindow final {
public:
    [[nodiscard]] static int run(
        HINSTANCE instance,
        int showCommand,
        bool smokeTest,
        AppearanceSettings appearance);

private:
    [[nodiscard]] bool create(HINSTANCE instance, int showCommand, bool showWindow);
    [[nodiscard]] bool render_frame();
    void handle_overview_command(const OverviewCommand& command);
    void handle_analyzer_command(const AnalyzerCommand& command);
    void confirm_review_deletion();
    void handle_volume_scan(std::size_t volumeIndex);
    void poll_scan_session();
    void show_settings_menu();
    void apply_appearance();
    [[nodiscard]] bool dark_theme_enabled() const noexcept;
    [[nodiscard]] float pixels_to_dip(int pixels) const noexcept;

    [[nodiscard]] LRESULT handle_message(UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK window_proc(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);

    HWND window_ = nullptr;
    render::GraphicsDevice graphics_;
    DiskOverview overview_;
    AnalyzerView analyzer_;
    AnalyzerNavigationState navigation_;
    DeletionReview deletionReview_;
    std::vector<platform::windows::VolumeSnapshot> volumes_;
    ScanUiModel scanUi_;
    std::unique_ptr<scan::ScanSession> scanSession_;
    std::optional<platform::windows::ScanResult> completedScan_;
    std::optional<std::size_t> completedVolumeIndex_;
    AppearanceSettings appearance_{core::ThemeMode::System, core::Language::English};
    bool trackingMouse_ = false;
};

} // namespace diskbloom::app
