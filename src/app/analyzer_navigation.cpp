#include "app/analyzer_navigation.h"

namespace diskbloom::app {
namespace {

bool is_directory(const core::ScanTree& tree, const core::NodeIndex node) noexcept {
    return node < tree.nodes().size()
        && core::has_flag(tree.node(node).flags, core::ScanNodeFlags::Directory);
}

} // namespace

bool open_analyzer(
    AnalyzerNavigationState& state,
    const core::ScanTree& tree,
    const core::NodeIndex root) noexcept {
    if (!is_directory(tree, root)) {
        return false;
    }
    state.view = MainContentView::Analyzer;
    state.root = root;
    state.selected = root;
    return true;
}

bool apply_analyzer_command(
    AnalyzerNavigationState& state,
    const core::ScanTree& tree,
    const AnalyzerCommand& command) noexcept {
    if (state.view != MainContentView::Analyzer) {
        return false;
    }

    switch (command.kind) {
    case AnalyzerCommandKind::ReturnToOverview:
        state.view = MainContentView::Overview;
        state.root = core::invalid_node;
        state.selected = core::invalid_node;
        return true;
    case AnalyzerCommandKind::NavigateToNode:
    case AnalyzerCommandKind::NavigateToParent:
        if (!is_directory(tree, command.node)) {
            return false;
        }
        state.root = command.node;
        state.selected = command.node;
        return true;
    case AnalyzerCommandKind::SelectNode:
        if (command.node >= tree.nodes().size()) {
            return false;
        }
        state.selected = command.node;
        return true;
    case AnalyzerCommandKind::MinimizeWindow:
    case AnalyzerCommandKind::ToggleMaximizeWindow:
    case AnalyzerCommandKind::CloseWindow:
        return false;
    }
    return false;
}

} // namespace diskbloom::app
