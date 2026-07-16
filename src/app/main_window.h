#pragma once

#include "app/appearance_settings.h"
#include "app/analyzer_input_lifecycle.h"
#include "app/analyzer_navigation.h"
#include "app/analyzer_view.h"
#include "app/disk_overview.h"
#include "app/deletion_review.h"
#include "app/recycle_session.h"
#include "app/review_change.h"
#include "app/scan_ui_state.h"
#include "core/language.h"
#include "core/theme.h"
#include "render/graphics_device.h"
#include "scan/scan_session.h"

#include <Windows.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
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
    void apply_review_change(
        ReviewMutation mutation,
        std::chrono::steady_clock::time_point now);
    void dispatch_analyzer_command();
    [[nodiscard]] bool apply_analyzer_input_actions(const AnalyzerInputActions& actions);
    void confirm_review_deletion();
    void handle_volume_scan(std::size_t volumeIndex);
    void handle_folder_scan();
    void start_scan(std::wstring rootPath);
    void poll_scan_session();
    void poll_recycle_session();
    void finish_recycle_success();
    void sync_analyzer_navigation_chrome();
    [[nodiscard]] bool create_breadcrumb_tooltip();
    void update_breadcrumb_tooltip();
    void hide_breadcrumb_tooltip() noexcept;
    void show_settings_menu();
    void apply_appearance();
    void apply_directory_transition_policy_change();
    void sync_analyzer_hover_timer();
    [[nodiscard]] bool dark_theme_enabled() const noexcept;
    [[nodiscard]] bool directory_transitions_enabled() const noexcept;
    [[nodiscard]] float pixels_to_dip(int pixels) const noexcept;

    [[nodiscard]] LRESULT handle_message(UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK window_proc(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);

    HWND window_ = nullptr;
    HWND breadcrumbTooltip_ = nullptr;
    render::GraphicsDevice graphics_;
    DiskOverview overview_;
    AnalyzerView analyzer_;
    AnalyzerNavigationState navigation_;
    DeletionReview deletionReview_;
    core::ScanTreeExclusion reviewExclusion_;
    std::vector<platform::windows::VolumeSnapshot> volumes_;
    ScanUiModel scanUi_;
    std::unique_ptr<scan::ScanSession> scanSession_;
    std::unique_ptr<RecycleSession> recycleSession_;
    std::optional<platform::windows::ScanResult> completedScan_;
    std::optional<ScanUiTarget> completedScanTarget_;
    AppearanceSettings appearance_{core::ThemeMode::System, core::Language::English};
    std::wstring breadcrumbTooltipText_;
    bool trackingMouse_ = false;
};

} // namespace diskbloom::app
