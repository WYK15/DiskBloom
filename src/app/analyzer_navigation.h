#pragma once

#include "app/analyzer_view.h"
#include "core/scan_tree.h"

namespace diskbloom::app {

enum class MainContentView {
    Overview,
    Analyzer,
};

struct AnalyzerNavigationState {
    MainContentView view = MainContentView::Overview;
    core::NodeIndex root = core::invalid_node;
    core::NodeIndex selected = core::invalid_node;
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
