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
    model.activeVolume.reset();
}

ScanUiAction activate_volume(
    ScanUiModel& model,
    const std::size_t volumeIndex) noexcept {
    if (volumeIndex >= model.volumes.size()) {
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

bool apply_scan_snapshot(
    ScanUiModel& model,
    const scan::ScanSessionState sessionState,
    const platform::windows::ScanProgress& progress) noexcept {
    if (!model.activeVolume.has_value()
        || *model.activeVolume >= model.volumes.size()) {
        return false;
    }

    auto& status = model.volumes[*model.activeVolume];
    const auto nextState = to_volume_state(sessionState, status.state);
    const auto changed = status.state != nextState
        || !same_progress(status.progress, progress);
    status.state = nextState;
    status.progress = progress;
    return changed;
}

std::optional<std::size_t> release_terminal_scan(ScanUiModel& model) noexcept {
    if (!model.activeVolume.has_value()
        || *model.activeVolume >= model.volumes.size()
        || !is_terminal(model.volumes[*model.activeVolume].state)) {
        return std::nullopt;
    }

    const auto volumeIndex = model.activeVolume;
    model.activeVolume.reset();
    return volumeIndex;
}

} // namespace diskbloom::app
