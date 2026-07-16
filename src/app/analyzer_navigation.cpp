#include "app/analyzer_navigation.h"

namespace diskbloom::app {
namespace {

bool is_directory(const core::ScanTree& tree, const core::NodeIndex node) noexcept {
    return node < tree.nodes().size()
        && core::has_flag(tree.node(node).flags, core::ScanNodeFlags::Directory);
}

bool apply_history_entry(
    AnalyzerNavigationState& state,
    const core::ScanTree& tree,
    const NavigationEntry entry) noexcept {
    if (entry.view == MainContentView::Overview) {
        state.view = MainContentView::Overview;
        state.root = core::invalid_node;
        state.selected = core::invalid_node;
        return true;
    }
    if (!is_directory(tree, entry.node)) {
        return false;
    }
    state.view = MainContentView::Analyzer;
    state.root = entry.node;
    state.selected = entry.node;
    return true;
}

template <typename Move>
bool move_through_history(
    AnalyzerNavigationState& state,
    const core::ScanTree& tree,
    Move&& move) noexcept {
    while (const auto entry = move()) {
        if (apply_history_entry(state, tree, *entry)) {
            return true;
        }
    }
    return false;
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
    state.history.reset({MainContentView::Overview, core::invalid_node});
    (void)state.history.record({MainContentView::Analyzer, root});
    return true;
}

bool apply_analyzer_command(
    AnalyzerNavigationState& state,
    const core::ScanTree& tree,
    const AnalyzerCommand& command) noexcept {
    if (command.kind == AnalyzerCommandKind::NavigateBack) {
        return move_through_history(state, tree, [&state]() noexcept {
            return state.history.back();
        });
    }
    if (command.kind == AnalyzerCommandKind::NavigateForward) {
        return move_through_history(state, tree, [&state]() noexcept {
            return state.history.forward();
        });
    }

    switch (command.kind) {
    case AnalyzerCommandKind::ReturnToOverview:
        if (state.view != MainContentView::Analyzer) {
            return false;
        }
        state.view = MainContentView::Overview;
        state.root = core::invalid_node;
        state.selected = core::invalid_node;
        (void)state.history.record({MainContentView::Overview, core::invalid_node});
        return true;
    case AnalyzerCommandKind::NavigateBreadcrumb:
    case AnalyzerCommandKind::NavigateToNode:
    case AnalyzerCommandKind::NavigateToParent:
        if (state.view != MainContentView::Analyzer) {
            return false;
        }
        if (!is_directory(tree, command.node)) {
            return false;
        }
        state.root = command.node;
        state.selected = command.node;
        (void)state.history.record({MainContentView::Analyzer, command.node});
        return true;
    case AnalyzerCommandKind::SelectNode:
        if (state.view != MainContentView::Analyzer) {
            return false;
        }
        if (command.node >= tree.nodes().size()) {
            return false;
        }
        state.selected = command.node;
        return true;
    case AnalyzerCommandKind::NavigateBack:
    case AnalyzerCommandKind::NavigateForward:
    case AnalyzerCommandKind::OpenBreadcrumbOverflow:
    case AnalyzerCommandKind::MinimizeWindow:
    case AnalyzerCommandKind::ToggleMaximizeWindow:
    case AnalyzerCommandKind::CloseWindow:
    case AnalyzerCommandKind::PreviewNode:
    case AnalyzerCommandKind::RevealNode:
    case AnalyzerCommandKind::AddToReview:
    case AnalyzerCommandKind::ConfirmReview:
        return false;
    }
    return false;
}

} // namespace diskbloom::app
