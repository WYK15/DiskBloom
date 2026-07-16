#include "app/main_window.h"

#include "core/string_catalog.h"
#include "core/node_path.h"
#include "platform/windows/folder_picker.h"
#include "platform/windows/shell_actions.h"
#include "platform/windows/recycle_bin.h"
#include "platform/windows/system_theme.h"
#include "platform/windows/volume_service.h"
#include "resources/resource.h"

#include <Windowsx.h>
#include <dwmapi.h>

#include <algorithm>
#include <array>
#include <string>

namespace diskbloom::app {
namespace {

constexpr wchar_t window_class_name[] = L"DiskBloom.MainWindow";
constexpr UINT_PTR scan_timer_id = 1U;
constexpr UINT_PTR recycle_timer_id = 2U;
constexpr UINT scan_timer_interval_ms = 33U;

std::wstring menu_text(const core::Language language, const core::StringId id) {
    return std::wstring(core::get_string(language, id));
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

int MainWindow::run(
    const HINSTANCE instance,
    const int showCommand,
    const bool smokeTest,
    const AppearanceSettings appearance) {
    MainWindow main_window;
    main_window.appearance_ = appearance;
    if (!main_window.create(instance, showCommand, !smokeTest)) {
        return 1;
    }

    if (smokeTest) {
        const auto rendered = main_window.render_frame();
        DestroyWindow(main_window.window_);
        return rendered ? 0 : 2;
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0U, 0U) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

bool MainWindow::create(
    const HINSTANCE instance,
    const int showCommand,
    const bool showWindow) {
    WNDCLASSEXW window_class{
        .cbSize = sizeof(WNDCLASSEXW),
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = &MainWindow::window_proc,
        .cbClsExtra = 0,
        .cbWndExtra = 0,
        .hInstance = instance,
        .hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_DISKBLOOM)),
        .hCursor = LoadCursorW(nullptr, IDC_ARROW),
        .hbrBackground = nullptr,
        .lpszMenuName = nullptr,
        .lpszClassName = window_class_name,
        .hIconSm = LoadIconW(instance, MAKEINTRESOURCEW(IDI_DISKBLOOM)),
    };
    if (RegisterClassExW(&window_class) == 0U
        && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    RECT work_area{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0U, &work_area, 0U);
    constexpr int initial_width = 1200;
    constexpr int initial_height = 720;
    const auto left = work_area.left + ((work_area.right - work_area.left) - initial_width) / 2;
    const auto top = work_area.top + ((work_area.bottom - work_area.top) - initial_height) / 2;

    const auto title = core::get_string(appearance_.language, core::StringId::AppTitle);
    window_ = CreateWindowExW(
        WS_EX_APPWINDOW,
        window_class_name,
        title.data(),
        WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU,
        left,
        top,
        initial_width,
        initial_height,
        nullptr,
        nullptr,
        instance,
        this);
    if (window_ == nullptr) {
        return false;
    }

    if (showWindow) {
        ShowWindow(window_, showCommand);
        UpdateWindow(window_);
    }
    return true;
}

bool MainWindow::render_frame() {
    RECT client{};
    if (!GetClientRect(window_, &client)) {
        return false;
    }

    const auto theme = core::make_theme(dark_theme_enabled());
    if (!graphics_.begin_draw(theme.window)) {
        return false;
    }

    const auto width = pixels_to_dip(client.right - client.left);
    const auto height = pixels_to_dip(client.bottom - client.top);
    const auto rendered = navigation_.view == MainContentView::Analyzer
        ? analyzer_.draw(graphics_, theme, appearance_.language, width, height)
        : overview_.draw(graphics_, theme, appearance_.language, width, height);
    if (!rendered) {
        return false;
    }
    return SUCCEEDED(graphics_.end_draw());
}

void MainWindow::dispatch_analyzer_command() {
    if (const auto command = analyzer_.take_command(); command.has_value()) {
        handle_analyzer_command(*command);
    }
}

bool MainWindow::apply_analyzer_input_actions(const AnalyzerInputActions& actions) {
    auto changed = false;
    if (actions.cancelDrag) {
        changed = analyzer_.cancel_drag();
    }
    if (actions.clearHover) {
        changed = analyzer_.pointer_left() || changed;
    }
    if (actions.beginCapture) {
        SetCapture(window_);
    }
    if (actions.releaseCapture && GetCapture() == window_) {
        ReleaseCapture();
    }
    if (actions.dispatchCommand) {
        dispatch_analyzer_command();
    }
    return changed;
}

void MainWindow::handle_analyzer_command(const AnalyzerCommand& command) {
    const auto recycleBusy = recycleSession_ != nullptr
        && recycleSession_->state() == RecycleSessionState::Running;
    if (recycleBusy
        && command.kind != AnalyzerCommandKind::MinimizeWindow
        && command.kind != AnalyzerCommandKind::ToggleMaximizeWindow
        && command.kind != AnalyzerCommandKind::CloseWindow) {
        return;
    }
    switch (command.kind) {
    case AnalyzerCommandKind::MinimizeWindow:
        ShowWindow(window_, SW_MINIMIZE);
        return;
    case AnalyzerCommandKind::ToggleMaximizeWindow:
        ShowWindow(window_, IsZoomed(window_) ? SW_RESTORE : SW_MAXIMIZE);
        return;
    case AnalyzerCommandKind::CloseWindow:
        DestroyWindow(window_);
        return;
    case AnalyzerCommandKind::PreviewNode:
    case AnalyzerCommandKind::RevealNode: {
        if (!completedScan_.has_value()) {
            return;
        }
        const auto path = core::build_node_path(
            completedScan_->tree,
            command.node,
            completedScan_->rootPath);
        const auto result = !path.has_value()
            ? platform::windows::ShellActionResult::InvalidPath
            : command.kind == AnalyzerCommandKind::PreviewNode
                ? platform::windows::open_with_shell(*path)
                : platform::windows::reveal_in_explorer(*path);
        if (result != platform::windows::ShellActionResult::Succeeded) {
            MessageBoxW(
                window_,
                core::get_string(appearance_.language, core::StringId::ActionFailed).data(),
                core::get_string(appearance_.language, core::StringId::AppTitle).data(),
                MB_OK | MB_ICONERROR);
        }
        return;
    }
    case AnalyzerCommandKind::AddToReview:
        if (completedScan_.has_value()
            && deletionReview_.add(completedScan_->tree, command.node)) {
            analyzer_.set_review_summary(
                deletionReview_.nodes().size(),
                deletionReview_.total_bytes());
            analyzer_.set_review_nodes(deletionReview_.nodes());
            InvalidateRect(window_, nullptr, FALSE);
        }
        return;
    case AnalyzerCommandKind::ConfirmReview:
        confirm_review_deletion();
        return;
    case AnalyzerCommandKind::ReturnToOverview:
    case AnalyzerCommandKind::NavigateBack:
    case AnalyzerCommandKind::NavigateForward:
    case AnalyzerCommandKind::NavigateBreadcrumb:
    case AnalyzerCommandKind::NavigateToNode:
    case AnalyzerCommandKind::NavigateToParent:
    case AnalyzerCommandKind::SelectNode:
        break;
    case AnalyzerCommandKind::OpenBreadcrumbOverflow:
        return;
    }
    if (!completedScan_.has_value()) {
        return;
    }
    const auto previousRoot = navigation_.root;
    const auto previousSelected = navigation_.selected;
    if (!apply_analyzer_command(navigation_, completedScan_->tree, command)) {
        return;
    }
    if (navigation_.view == MainContentView::Analyzer) {
        if (navigation_.root != previousRoot) {
            (void)analyzer_.set_root(navigation_.root);
        }
        if (navigation_.selected != previousSelected) {
            analyzer_.set_selected_node(navigation_.selected);
        }
    } else {
        (void)apply_analyzer_input_actions(compute_analyzer_input_actions(
            AnalyzerInputTransition::PointerLeft,
            analyzer_.drag_pending(),
            GetCapture() == window_));
    }
    InvalidateRect(window_, nullptr, FALSE);
}

void MainWindow::confirm_review_deletion() {
    if (!completedScan_.has_value() || deletionReview_.nodes().empty()) {
        return;
    }

    std::vector<std::wstring> paths;
    paths.reserve(deletionReview_.nodes().size());
    for (const auto node : deletionReview_.nodes()) {
        auto path = core::build_node_path(
            completedScan_->tree,
            node,
            completedScan_->rootPath);
        if (!path.has_value()) {
            MessageBoxW(
                window_,
                core::get_string(appearance_.language, core::StringId::ActionFailed).data(),
                core::get_string(appearance_.language, core::StringId::AppTitle).data(),
                MB_OK | MB_ICONERROR);
            return;
        }
        paths.push_back(std::move(*path));
    }

    auto confirmation = std::wstring(
        core::get_string(appearance_.language, core::StringId::ConfirmRecycle));
    confirmation.append(L"\n\n");
    confirmation.append(std::to_wstring(deletionReview_.nodes().size()));
    confirmation.append(L" ");
    confirmation.append(core::get_string(appearance_.language, core::StringId::Items));
    confirmation.append(L"  ");
    confirmation.append(format_bytes(deletionReview_.total_bytes()));
    constexpr std::size_t visible_path_limit = 8U;
    for (std::size_t index = 0U;
         index < std::min(paths.size(), visible_path_limit);
         ++index) {
        confirmation.append(L"\n");
        confirmation.append(paths[index]);
    }
    if (paths.size() > visible_path_limit) {
        confirmation.append(L"\n...");
    }
    if (MessageBoxW(
            window_,
            confirmation.c_str(),
            core::get_string(appearance_.language, core::StringId::AppTitle).data(),
            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2)
        != IDYES) {
        return;
    }

    recycleSession_ = std::make_unique<RecycleSession>();
    try {
        if (!recycleSession_->start(std::move(paths), window_)) {
            recycleSession_.reset();
        }
    } catch (...) {
        recycleSession_.reset();
    }
    if (recycleSession_ == nullptr) {
        MessageBoxW(
            window_,
            core::get_string(appearance_.language, core::StringId::ActionFailed).data(),
            core::get_string(appearance_.language, core::StringId::AppTitle).data(),
            MB_OK | MB_ICONERROR);
        return;
    }

    analyzer_.set_recycle_in_progress(true);
    SetTimer(window_, recycle_timer_id, scan_timer_interval_ms, nullptr);
    InvalidateRect(window_, nullptr, FALSE);
}

void MainWindow::finish_recycle_success() {
    if (!completedScan_.has_value()) {
        return;
    }

    const auto scanTarget = completedScanTarget_;
    const auto rootPath = completedScan_->rootPath;
    (void)apply_analyzer_input_actions(compute_analyzer_input_actions(
        AnalyzerInputTransition::ContentChanging,
        analyzer_.drag_pending(),
        GetCapture() == window_));
    deletionReview_.clear();
    analyzer_.set_review_summary(0U, 0U);
    analyzer_.set_review_nodes(deletionReview_.nodes());
    analyzer_.set_tree(nullptr, core::invalid_node);
    navigation_ = {};
    completedScan_.reset();
    completedScanTarget_.reset();
    InvalidateRect(window_, nullptr, FALSE);
    if (!scanTarget.has_value()) {
        return;
    }
    if (scanTarget->kind == ScanUiTargetKind::Volume) {
        handle_volume_scan(scanTarget->volumeIndex);
    } else if (activate_folder(scanUi_).kind == ScanUiActionKind::Start) {
        start_scan(rootPath);
        overview_.set_scan_statuses(scanUi_.volumes, scanUi_.folder);
        InvalidateRect(window_, nullptr, FALSE);
    }
}

void MainWindow::poll_recycle_session() {
    if (recycleSession_ == nullptr
        || recycleSession_->state() == RecycleSessionState::Running) {
        return;
    }

    const auto result = recycleSession_->take_result();
    KillTimer(window_, recycle_timer_id);
    recycleSession_.reset();
    analyzer_.set_recycle_in_progress(false);

    if (result == platform::windows::RecycleResult::Succeeded) {
        finish_recycle_success();
        return;
    }
    if (result.has_value()
        && result != platform::windows::RecycleResult::Cancelled) {
        MessageBoxW(
            window_,
            core::get_string(appearance_.language, core::StringId::ActionFailed).data(),
            core::get_string(appearance_.language, core::StringId::AppTitle).data(),
            MB_OK | MB_ICONERROR);
    }
    InvalidateRect(window_, nullptr, FALSE);
}

void MainWindow::handle_overview_command(const OverviewCommand& command) {
    switch (command.kind) {
    case OverviewCommandKind::ScanVolume:
        handle_volume_scan(command.volumeIndex);
        break;
    case OverviewCommandKind::ScanFolder:
        handle_folder_scan();
        break;
    case OverviewCommandKind::OpenSettings:
        show_settings_menu();
        break;
    case OverviewCommandKind::MinimizeWindow:
        ShowWindow(window_, SW_MINIMIZE);
        break;
    case OverviewCommandKind::ToggleMaximizeWindow:
        ShowWindow(window_, IsZoomed(window_) ? SW_RESTORE : SW_MAXIMIZE);
        break;
    case OverviewCommandKind::CloseWindow:
        DestroyWindow(window_);
        break;
    }
}

void MainWindow::handle_volume_scan(const std::size_t volumeIndex) {
    const auto action = activate_volume(scanUi_, volumeIndex);
    switch (action.kind) {
    case ScanUiActionKind::None:
        return;
    case ScanUiActionKind::Cancel:
        if (scanSession_ != nullptr) {
            scanSession_->cancel();
        }
        break;
    case ScanUiActionKind::Start:
        if (action.volumeIndex >= volumes_.size()) {
            return;
        }
        start_scan(volumes_[action.volumeIndex].rootPath);
        break;
    }

    overview_.set_scan_statuses(scanUi_.volumes, scanUi_.folder);
    InvalidateRect(window_, nullptr, FALSE);
}

void MainWindow::handle_folder_scan() {
    if (scanUi_.folderActive) {
        if (activate_folder(scanUi_).kind == ScanUiActionKind::Cancel
            && scanSession_ != nullptr) {
            scanSession_->cancel();
        }
        overview_.set_scan_statuses(scanUi_.volumes, scanUi_.folder);
        InvalidateRect(window_, nullptr, FALSE);
        return;
    }
    if (scanUi_.activeVolume.has_value()) {
        return;
    }

    auto selection = platform::windows::pick_folder(window_);
    if (selection.status == platform::windows::FolderPickStatus::Cancelled) {
        return;
    }
    if (selection.status != platform::windows::FolderPickStatus::Selected) {
        MessageBoxW(
            window_,
            core::get_string(appearance_.language, core::StringId::ActionFailed).data(),
            core::get_string(appearance_.language, core::StringId::AppTitle).data(),
            MB_OK | MB_ICONERROR);
        return;
    }

    if (activate_folder(scanUi_).kind == ScanUiActionKind::Start) {
        start_scan(std::move(selection.path));
    }
    overview_.set_scan_statuses(scanUi_.volumes, scanUi_.folder);
    InvalidateRect(window_, nullptr, FALSE);
}

void MainWindow::start_scan(std::wstring rootPath) {
    scanSession_ = std::make_unique<scan::ScanSession>();
    deletionReview_.clear();
    analyzer_.set_review_summary(0U, 0U);
    analyzer_.set_review_nodes(deletionReview_.nodes());
    try {
        if (!scanSession_->start(std::move(rootPath))) {
            (void)apply_scan_snapshot(scanUi_, scan::ScanSessionState::Failed, {});
            (void)release_terminal_scan(scanUi_);
            scanSession_.reset();
            return;
        }
    } catch (...) {
        (void)apply_scan_snapshot(scanUi_, scan::ScanSessionState::Failed, {});
        (void)release_terminal_scan(scanUi_);
        scanSession_.reset();
        return;
    }
    SetTimer(window_, scan_timer_id, scan_timer_interval_ms, nullptr);
}

void MainWindow::poll_scan_session() {
    if (scanSession_ == nullptr) {
        return;
    }

    const auto sessionState = scanSession_->state();
    const auto progress = scanSession_->progress();
    auto changed = apply_scan_snapshot(scanUi_, sessionState, progress);
    if (sessionState == scan::ScanSessionState::Completed
        || sessionState == scan::ScanSessionState::Cancelled
        || sessionState == scan::ScanSessionState::Failed) {
        auto result = scanSession_->take_result();
        const auto terminalTarget = release_terminal_scan(scanUi_);
        if (result.has_value()
            && result->completion == platform::windows::ScanCompletion::Completed) {
            (void)apply_analyzer_input_actions(compute_analyzer_input_actions(
                AnalyzerInputTransition::ContentChanging,
                analyzer_.drag_pending(),
                GetCapture() == window_));
            completedScan_ = std::move(*result);
            completedScanTarget_ = terminalTarget;
            deletionReview_.clear();
            analyzer_.set_review_summary(0U, 0U);
            analyzer_.set_review_nodes(deletionReview_.nodes());
            if (!completedScan_->tree.nodes().empty()
                && open_analyzer(navigation_, completedScan_->tree, 0U)) {
                analyzer_.set_tree(&completedScan_->tree, 0U);
            }
        }
        KillTimer(window_, scan_timer_id);
        scanSession_.reset();
        changed = true;
    }

    if (changed) {
        overview_.set_scan_statuses(scanUi_.volumes, scanUi_.folder);
        InvalidateRect(window_, nullptr, FALSE);
    }
}

void MainWindow::show_settings_menu() {
    const auto root = CreatePopupMenu();
    const auto theme_menu = CreatePopupMenu();
    const auto language_menu = CreatePopupMenu();
    if (root == nullptr || theme_menu == nullptr || language_menu == nullptr) {
        if (root != nullptr) {
            DestroyMenu(root);
        }
        if (theme_menu != nullptr) {
            DestroyMenu(theme_menu);
        }
        if (language_menu != nullptr) {
            DestroyMenu(language_menu);
        }
        return;
    }

    const auto append_checked = [](
                                    HMENU menu,
                                    const SettingsCommand command,
                                    const std::wstring& text,
                                    const bool checked) {
        AppendMenuW(
            menu,
            MF_STRING | (checked ? MF_CHECKED : MF_UNCHECKED),
            static_cast<UINT_PTR>(command),
            text.c_str());
    };

    append_checked(
        theme_menu,
        SettingsCommand::ThemeSystem,
        menu_text(appearance_.language, core::StringId::ThemeSystem),
        appearance_.themeMode == core::ThemeMode::System);
    append_checked(
        theme_menu,
        SettingsCommand::ThemeLight,
        menu_text(appearance_.language, core::StringId::ThemeLight),
        appearance_.themeMode == core::ThemeMode::Light);
    append_checked(
        theme_menu,
        SettingsCommand::ThemeDark,
        menu_text(appearance_.language, core::StringId::ThemeDark),
        appearance_.themeMode == core::ThemeMode::Dark);

    append_checked(
        language_menu,
        SettingsCommand::LanguageEnglish,
        menu_text(appearance_.language, core::StringId::English),
        appearance_.language == core::Language::English);
    append_checked(
        language_menu,
        SettingsCommand::LanguageChinese,
        menu_text(appearance_.language, core::StringId::SimplifiedChinese),
        appearance_.language == core::Language::SimplifiedChinese);

    const auto theme_label = menu_text(appearance_.language, core::StringId::Theme);
    const auto language_label = menu_text(appearance_.language, core::StringId::Language);
    AppendMenuW(root, MF_POPUP, reinterpret_cast<UINT_PTR>(theme_menu), theme_label.c_str());
    AppendMenuW(root, MF_POPUP, reinterpret_cast<UINT_PTR>(language_menu), language_label.c_str());

    POINT cursor{};
    GetCursorPos(&cursor);
    const auto selected = TrackPopupMenuEx(
        root,
        TPM_RETURNCMD | TPM_RIGHTBUTTON,
        cursor.x,
        cursor.y,
        window_,
        nullptr);
    DestroyMenu(root);

    if (!apply_settings_command(
            appearance_,
            static_cast<SettingsCommand>(selected))) {
        return;
    }

    SetWindowTextW(
        window_,
        core::get_string(appearance_.language, core::StringId::AppTitle).data());
    apply_appearance();
    InvalidateRect(window_, nullptr, FALSE);
}

void MainWindow::apply_appearance() {
    const BOOL dark = dark_theme_enabled() ? TRUE : FALSE;
    DwmSetWindowAttribute(
        window_,
        DWMWA_USE_IMMERSIVE_DARK_MODE,
        &dark,
        sizeof(dark));

    constexpr DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
    DwmSetWindowAttribute(
        window_,
        DWMWA_WINDOW_CORNER_PREFERENCE,
        &corners,
        sizeof(corners));
}

bool MainWindow::dark_theme_enabled() const noexcept {
    if (appearance_.themeMode == core::ThemeMode::Dark) {
        return true;
    }
    if (appearance_.themeMode == core::ThemeMode::Light) {
        return false;
    }
    return platform::windows::is_system_dark_theme();
}

float MainWindow::pixels_to_dip(const int pixels) const noexcept {
    const auto dpi = std::max(GetDpiForWindow(window_), 96U);
    return static_cast<float>(pixels) * 96.0F / static_cast<float>(dpi);
}

LRESULT MainWindow::handle_message(
    const UINT message,
    const WPARAM wParam,
    const LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        if (!graphics_.initialize(window_)) {
            return -1;
        }
        volumes_ = platform::windows::enumerate_volumes();
        overview_.set_volumes(volumes_);
        reset_scan_ui(scanUi_, volumes_.size());
        overview_.set_scan_statuses(scanUi_.volumes, scanUi_.folder);
        apply_appearance();
        return 0;

    case WM_TIMER:
        if (wParam == scan_timer_id) {
            poll_scan_session();
            return 0;
        }
        if (wParam == recycle_timer_id) {
            poll_recycle_session();
            return 0;
        }
        break;

    case WM_NCCALCSIZE:
        if (wParam != 0U) {
            return 0;
        }
        break;

    case WM_NCHITTEST: {
        const POINT screen_point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        RECT window_rect{};
        GetWindowRect(window_, &window_rect);
        const auto dpi = std::max(GetDpiForWindow(window_), 96U);
        const auto border = std::max<LONG>(
            6L,
            static_cast<LONG>(MulDiv(6, static_cast<int>(dpi), 96)));
        const bool left = screen_point.x < window_rect.left + border;
        const bool right = screen_point.x >= window_rect.right - border;
        const bool top = screen_point.y < window_rect.top + border;
        const bool bottom = screen_point.y >= window_rect.bottom - border;
        if (top && left) return HTTOPLEFT;
        if (top && right) return HTTOPRIGHT;
        if (bottom && left) return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;
        if (left) return HTLEFT;
        if (right) return HTRIGHT;
        if (top) return HTTOP;
        if (bottom) return HTBOTTOM;

        POINT client_point = screen_point;
        ScreenToClient(window_, &client_point);
        RECT client{};
        GetClientRect(window_, &client);
        const auto x = pixels_to_dip(client_point.x);
        const auto y = pixels_to_dip(client_point.y);
        const auto width = pixels_to_dip(client.right - client.left);
        const auto analyzer_back = navigation_.view == MainContentView::Analyzer
            && x < 76.0F;
        if (y < 76.0F && x < width - 138.0F && !analyzer_back) {
            return HTCAPTION;
        }
        return HTCLIENT;
    }

    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            const auto resized = graphics_.resize(
                static_cast<std::uint32_t>(LOWORD(lParam)),
                static_cast<std::uint32_t>(HIWORD(lParam)));
            if (!resized) {
                return 0;
            }
            InvalidateRect(window_, nullptr, FALSE);
        }
        return 0;

    case WM_DPICHANGED: {
        const auto* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(
            window_,
            nullptr,
            suggested->left,
            suggested->top,
            suggested->right - suggested->left,
            suggested->bottom - suggested->top,
            SWP_NOACTIVATE | SWP_NOZORDER);
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (!trackingMouse_) {
            TRACKMOUSEEVENT tracking{
                .cbSize = sizeof(TRACKMOUSEEVENT),
                .dwFlags = TME_LEAVE,
                .hwndTrack = window_,
                .dwHoverTime = 0U,
            };
            TrackMouseEvent(&tracking);
            trackingMouse_ = true;
        }
        const auto x = pixels_to_dip(GET_X_LPARAM(lParam));
        const auto y = pixels_to_dip(GET_Y_LPARAM(lParam));
        const auto changed = navigation_.view == MainContentView::Analyzer
            ? analyzer_.pointer_moved(x, y)
            : overview_.pointer_moved(x, y);
        if (changed) {
            InvalidateRect(window_, nullptr, FALSE);
        }
        return 0;
    }

    case WM_MOUSELEAVE: {
        trackingMouse_ = false;
        auto changed = false;
        if (navigation_.view == MainContentView::Analyzer) {
            changed = apply_analyzer_input_actions(compute_analyzer_input_actions(
                AnalyzerInputTransition::PointerLeft,
                analyzer_.drag_pending(),
                GetCapture() == window_));
        } else {
            changed = overview_.pointer_left();
        }
        if (changed) {
            InvalidateRect(window_, nullptr, FALSE);
        }
        return 0;
    }

    case WM_MOUSEWHEEL:
        if (navigation_.view == MainContentView::Analyzer) {
            const auto notches = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(window_, &point);
            if (notches != 0 && analyzer_.scroll_at(
                    pixels_to_dip(point.x),
                    pixels_to_dip(point.y),
                    -notches)) {
                InvalidateRect(window_, nullptr, FALSE);
            }
            return 0;
        }
        break;

    case WM_LBUTTONDOWN:
        if (navigation_.view == MainContentView::Analyzer) {
            analyzer_.pointer_down(
                pixels_to_dip(GET_X_LPARAM(lParam)),
                pixels_to_dip(GET_Y_LPARAM(lParam)));
            (void)apply_analyzer_input_actions(compute_analyzer_input_actions(
                AnalyzerInputTransition::PointerDown,
                analyzer_.drag_pending(),
                GetCapture() == window_));
        }
        return 0;

    case WM_LBUTTONUP:
        if (navigation_.view == MainContentView::Analyzer) {
            analyzer_.pointer_released(
                pixels_to_dip(GET_X_LPARAM(lParam)),
                pixels_to_dip(GET_Y_LPARAM(lParam)));
            (void)apply_analyzer_input_actions(compute_analyzer_input_actions(
                AnalyzerInputTransition::PointerReleased,
                analyzer_.drag_pending(),
                GetCapture() == window_));
        } else {
            overview_.pointer_pressed(
                pixels_to_dip(GET_X_LPARAM(lParam)),
                pixels_to_dip(GET_Y_LPARAM(lParam)));
            if (const auto command = overview_.take_command(); command.has_value()) {
                handle_overview_command(*command);
            }
        }
        return 0;

    case WM_CAPTURECHANGED:
        if (reinterpret_cast<HWND>(lParam) != window_
            && apply_analyzer_input_actions(compute_analyzer_input_actions(
                AnalyzerInputTransition::CaptureLost,
                analyzer_.drag_pending(),
                false))) {
            InvalidateRect(window_, nullptr, FALSE);
        }
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_F10) {
            show_settings_menu();
            return 0;
        }
        break;

    case WM_SETTINGCHANGE:
        if (appearance_.themeMode == core::ThemeMode::System) {
            apply_appearance();
            InvalidateRect(window_, nullptr, FALSE);
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT paint{};
        BeginPaint(window_, &paint);
        const auto rendered = render_frame();
        EndPaint(window_, &paint);
        return rendered ? 0 : DefWindowProcW(window_, message, wParam, lParam);
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_CLOSE:
        DestroyWindow(window_);
        return 0;

    case WM_DESTROY:
        KillTimer(window_, scan_timer_id);
        KillTimer(window_, recycle_timer_id);
        scanSession_.reset();
        recycleSession_.reset();
        graphics_.discard_device_resources();
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(window_, message, wParam, lParam);
}

LRESULT CALLBACK MainWindow::window_proc(
    const HWND window,
    const UINT message,
    const WPARAM wParam,
    const LPARAM lParam) {
    MainWindow* self = nullptr;
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        self = static_cast<MainWindow*>(create->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    }

    return self != nullptr
        ? self->handle_message(message, wParam, lParam)
        : DefWindowProcW(window, message, wParam, lParam);
}

} // namespace diskbloom::app
