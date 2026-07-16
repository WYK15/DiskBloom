#pragma once

#include "app/analyzer_history.h"
#include "app/analyzer_view.h"
#include "core/scan_tree.h"

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

[[nodiscard]] bool apply_analyzer_command(
    AnalyzerNavigationState& state,
    const core::ScanTree& tree,
    const AnalyzerCommand& command) noexcept;

} // namespace diskbloom::app
