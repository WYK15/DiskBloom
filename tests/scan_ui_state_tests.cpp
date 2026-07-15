#include "test_support.h"

#include "app/scan_ui_state.h"

using diskbloom::app::ScanUiActionKind;
using diskbloom::app::ScanUiModel;
using diskbloom::app::ScanUiTargetKind;
using diskbloom::app::VolumeScanState;
using diskbloom::app::activate_folder;
using diskbloom::app::activate_volume;
using diskbloom::app::apply_scan_snapshot;
using diskbloom::app::release_terminal_scan;
using diskbloom::app::reset_scan_ui;
using diskbloom::scan::ScanSessionState;

TEST_CASE(scan_ui_click_starts_selected_volume) {
    ScanUiModel model;
    reset_scan_ui(model, 3U);

    const auto action = activate_volume(model, 1U);

    CHECK(action.kind == ScanUiActionKind::Start);
    CHECK(action.volumeIndex == 1U);
    CHECK(model.activeVolume.has_value());
    CHECK(*model.activeVolume == 1U);
    CHECK(model.volumes[1].state == VolumeScanState::Running);
}

TEST_CASE(scan_ui_second_active_click_requests_cancellation_once) {
    ScanUiModel model;
    reset_scan_ui(model, 2U);
    (void)activate_volume(model, 0U);

    const auto cancel = activate_volume(model, 0U);
    const auto duplicate = activate_volume(model, 0U);
    const auto other_volume = activate_volume(model, 1U);

    CHECK(cancel.kind == ScanUiActionKind::Cancel);
    CHECK(model.volumes[0].state == VolumeScanState::Cancelling);
    CHECK(duplicate.kind == ScanUiActionKind::None);
    CHECK(other_volume.kind == ScanUiActionKind::None);
}

TEST_CASE(scan_ui_applies_progress_without_regressing_cancellation) {
    ScanUiModel model;
    reset_scan_ui(model, 1U);
    (void)activate_volume(model, 0U);
    (void)activate_volume(model, 0U);

    diskbloom::platform::windows::ScanProgress progress{};
    progress.entries = 42U;
    progress.logicalBytes = 4'096U;
    CHECK(apply_scan_snapshot(model, ScanSessionState::Running, progress));
    CHECK(model.volumes[0].state == VolumeScanState::Cancelling);
    CHECK(model.volumes[0].progress.entries == 42U);
}

TEST_CASE(scan_ui_terminal_state_exposes_volume_once) {
    ScanUiModel model;
    reset_scan_ui(model, 2U);
    (void)activate_volume(model, 1U);

    diskbloom::platform::windows::ScanProgress progress{};
    progress.files = 7U;
    CHECK(apply_scan_snapshot(model, ScanSessionState::Completed, progress));
    CHECK(model.volumes[1].state == VolumeScanState::Completed);

    const auto completed_target = release_terminal_scan(model);
    CHECK(completed_target.has_value());
    CHECK(completed_target->kind == ScanUiTargetKind::Volume);
    CHECK(completed_target->volumeIndex == 1U);
    CHECK(!model.activeVolume.has_value());
    CHECK(!release_terminal_scan(model).has_value());
}

TEST_CASE(scan_ui_rejects_invalid_volume_index) {
    ScanUiModel model;
    reset_scan_ui(model, 1U);
    CHECK(activate_volume(model, 4U).kind == ScanUiActionKind::None);
}

TEST_CASE(scan_ui_click_starts_folder_scan) {
    ScanUiModel model;
    reset_scan_ui(model, 2U);

    const auto action = activate_folder(model);

    CHECK(action.kind == ScanUiActionKind::Start);
    CHECK(model.folderActive);
    CHECK(!model.activeVolume.has_value());
    CHECK(model.folder.state == VolumeScanState::Running);
}

TEST_CASE(scan_ui_folder_and_volume_scans_are_mutually_exclusive) {
    ScanUiModel model;
    reset_scan_ui(model, 2U);

    (void)activate_folder(model);
    CHECK(activate_volume(model, 0U).kind == ScanUiActionKind::None);

    reset_scan_ui(model, 2U);
    (void)activate_volume(model, 0U);
    CHECK(activate_folder(model).kind == ScanUiActionKind::None);
}

TEST_CASE(scan_ui_second_folder_click_requests_cancellation_once) {
    ScanUiModel model;
    reset_scan_ui(model, 1U);
    (void)activate_folder(model);

    const auto cancel = activate_folder(model);
    const auto duplicate = activate_folder(model);

    CHECK(cancel.kind == ScanUiActionKind::Cancel);
    CHECK(model.folder.state == VolumeScanState::Cancelling);
    CHECK(duplicate.kind == ScanUiActionKind::None);
}

TEST_CASE(scan_ui_folder_terminal_state_releases_folder_target_once) {
    ScanUiModel model;
    reset_scan_ui(model, 1U);
    (void)activate_folder(model);

    diskbloom::platform::windows::ScanProgress progress{};
    progress.directories = 9U;
    CHECK(apply_scan_snapshot(model, ScanSessionState::Completed, progress));
    CHECK(model.folder.state == VolumeScanState::Completed);

    const auto completed_target = release_terminal_scan(model);
    CHECK(completed_target.has_value());
    CHECK(completed_target->kind == ScanUiTargetKind::Folder);
    CHECK(!model.folderActive);
    CHECK(!release_terminal_scan(model).has_value());
}
