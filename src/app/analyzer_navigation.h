#pragma once

#include "app/analyzer_history.h"
#include "app/analyzer_view.h"
#include "core/scan_tree.h"
#include "core/scan_tree_exclusion.h"

namespace diskbloom::app {

struct AnalyzerNavigationState {
    MainContentView view = MainContentView::Overview;
    core::NodeIndex root = core::invalid_node;
    core::NodeIndex selected = core::invalid_node;
    AnalyzerHistory history;
};

[[nodiscard]] bool open_analyzer(
    AnalyzerNavigationState& state,
    const core::ScanTree& tree,
    core::NodeIndex root) noexcept;

[[nodiscard]] core::NodeIndex nearest_visible_directory(
    const core::ScanTree& tree,
    const core::ScanTreeExclusion& exclusion,
    core::NodeIndex node) noexcept;

[[nodiscard]] bool reconcile_analyzer_visibility(
    AnalyzerNavigationState& state,
    const core::ScanTree& tree,
    const core::ScanTreeExclusion& exclusion) noexcept;

[[nodiscard]] bool navigation_can_back(
    const AnalyzerNavigationState& state,
    const core::ScanTree& tree,
    const core::ScanTreeExclusion* exclusion = nullptr) noexcept;

[[nodiscard]] bool navigation_can_forward(
    const AnalyzerNavigationState& state,
    const core::ScanTree& tree,
    const core::ScanTreeExclusion* exclusion = nullptr) noexcept;

[[nodiscard]] bool apply_analyzer_command(
    AnalyzerNavigationState& state,
    const core::ScanTree& tree,
    const AnalyzerCommand& command,
    const core::ScanTreeExclusion* exclusion = nullptr) noexcept;

} // namespace diskbloom::app
