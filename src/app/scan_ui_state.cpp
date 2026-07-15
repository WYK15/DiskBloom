#include "app/scan_ui_state.h"

namespace diskbloom::app {
namespace {

bool same_progress(
    const platform::windows::ScanProgress& left,
    const platform::windows::ScanProgress& right) noexcept {
    return left.entries == right.entries
        && left.files == right.files
        && left.directories == right.directories
        && left.logicalBytes == right.logicalBytes
        && left.errors == right.errors;
}

bool is_terminal(const VolumeScanState state) noexcept {
    return state == VolumeScanState::Completed
        || state == VolumeScanState::Cancelled
        || state == VolumeScanState::Failed;
}

VolumeScanState to_volume_state(
    const scan::ScanSessionState sessionState,
    const VolumeScanState currentState) noexcept {
    switch (sessionState) {
    case scan::ScanSessionState::Idle:
        return VolumeScanState::Idle;
    case scan::ScanSessionState::Running:
        return currentState == VolumeScanState::Cancelling
            ? VolumeScanState::Cancelling
            : VolumeScanState::Running;
    case scan::ScanSessionState::Cancelling:
        return VolumeScanState::Cancelling;
    case scan::ScanSessionState::Completed:
        return VolumeScanState::Completed;
    case scan::ScanSessionState::Cancelled:
        return VolumeScanState::Cancelled;
    case scan::ScanSessionState::Failed:
        return VolumeScanState::Failed;
    }
    return VolumeScanState::Failed;
}

} // namespace

void reset_scan_ui(ScanUiModel& model, const std::size_t volumeCount) {
    model.volumes.assign(volumeCount, VolumeScanStatus{});
    model.folder = {};
    model.activeVolume.reset();
    model.folderActive = false;
}

ScanUiAction activate_volume(
    ScanUiModel& model,
    const std::size_t volumeIndex) noexcept {
    if (volumeIndex >= model.volumes.size() || model.folderActive) {
        return {};
    }

    if (!model.activeVolume.has_value()) {
        model.activeVolume = volumeIndex;
        model.volumes[volumeIndex] = {
            .state = VolumeScanState::Running,
            .progress = {},
        };
        return {ScanUiActionKind::Start, volumeIndex};
    }

    if (*model.activeVolume == volumeIndex
        && model.volumes[volumeIndex].state == VolumeScanState::Running) {
        model.volumes[volumeIndex].state = VolumeScanState::Cancelling;
        return {ScanUiActionKind::Cancel, volumeIndex};
    }
    return {};
}

ScanUiAction activate_folder(ScanUiModel& model) noexcept {
    if (model.activeVolume.has_value()) {
        return {};
    }
    if (!model.folderActive) {
        model.folderActive = true;
        model.folder = {
            .state = VolumeScanState::Running,
            .progress = {},
        };
        return {ScanUiActionKind::Start};
    }
    if (model.folder.state == VolumeScanState::Running) {
        model.folder.state = VolumeScanState::Cancelling;
        return {ScanUiActionKind::Cancel};
    }
    return {};
}

bool apply_scan_snapshot(
    ScanUiModel& model,
    const scan::ScanSessionState sessionState,
    const platform::windows::ScanProgress& progress) noexcept {
    VolumeScanStatus* status = nullptr;
    if (model.activeVolume.has_value()
        && *model.activeVolume < model.volumes.size()) {
        status = &model.volumes[*model.activeVolume];
    } else if (model.folderActive) {
        status = &model.folder;
    }
    if (status == nullptr) {
        return false;
    }

    const auto nextState = to_volume_state(sessionState, status->state);
    const auto changed = status->state != nextState
        || !same_progress(status->progress, progress);
    status->state = nextState;
    status->progress = progress;
    return changed;
}

std::optional<ScanUiTarget> release_terminal_scan(ScanUiModel& model) noexcept {
    if (model.activeVolume.has_value()
        && *model.activeVolume < model.volumes.size()
        && is_terminal(model.volumes[*model.activeVolume].state)) {
        const auto target = ScanUiTarget{ScanUiTargetKind::Volume, *model.activeVolume};
        model.activeVolume.reset();
        return target;
    }
    if (model.folderActive && is_terminal(model.folder.state)) {
        model.folderActive = false;
        return ScanUiTarget{ScanUiTargetKind::Folder};
    }
    return std::nullopt;
}

} // namespace diskbloom::app
