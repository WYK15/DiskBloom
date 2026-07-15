#pragma once

#include "platform/windows/file_scanner.h"
#include "scan/scan_session.h"

#include <cstddef>
#include <limits>
#include <optional>
#include <vector>

namespace diskbloom::app {

enum class VolumeScanState {
    Idle,
    Running,
    Cancelling,
    Completed,
    Cancelled,
    Failed,
};

struct VolumeScanStatus {
    VolumeScanState state = VolumeScanState::Idle;
    platform::windows::ScanProgress progress;
};

struct ScanUiModel {
    std::vector<VolumeScanStatus> volumes;
    VolumeScanStatus folder;
    std::optional<std::size_t> activeVolume;
    bool folderActive = false;
};

enum class ScanUiTargetKind {
    Volume,
    Folder,
};

struct ScanUiTarget {
    ScanUiTargetKind kind = ScanUiTargetKind::Folder;
    std::size_t volumeIndex = std::numeric_limits<std::size_t>::max();
};

enum class ScanUiActionKind {
    None,
    Start,
    Cancel,
};

struct ScanUiAction {
    ScanUiActionKind kind = ScanUiActionKind::None;
    std::size_t volumeIndex = std::numeric_limits<std::size_t>::max();
};

void reset_scan_ui(ScanUiModel& model, std::size_t volumeCount);

[[nodiscard]] ScanUiAction activate_volume(
    ScanUiModel& model,
    std::size_t volumeIndex) noexcept;

[[nodiscard]] ScanUiAction activate_folder(ScanUiModel& model) noexcept;

[[nodiscard]] bool apply_scan_snapshot(
    ScanUiModel& model,
    scan::ScanSessionState sessionState,
    const platform::windows::ScanProgress& progress) noexcept;

[[nodiscard]] std::optional<ScanUiTarget> release_terminal_scan(
    ScanUiModel& model) noexcept;

} // namespace diskbloom::app
