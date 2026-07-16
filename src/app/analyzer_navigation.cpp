#include "app/analyzer_navigation.h"

namespace diskbloom::app {
namespace {

bool is_directory(const core::ScanTree& tree, const core::NodeIndex node) noexcept {
    return node < tree.nodes().size()
        && core::has_flag(tree.node(node).flags, core::ScanNodeFlags::Directory);
}

bool is_visible_destination(
    const core::ScanTree& tree,
    const core::NodeIndex node,
    const core::ScanTreeExclusion* const exclusion) noexcept {
    return is_directory(tree, node)
        && (exclusion == nullptr || exclusion->is_visible(tree, node));
}

bool is_history_entry_visible(
    const core::ScanTree& tree,
    const NavigationEntry entry,
    const core::ScanTreeExclusion* const exclusion) noexcept {
    return entry.view == MainContentView::Overview
        || is_visible_destination(tree, entry.node, exclusion);
}

bool is_descendant_or_same(
    const core::ScanTree& tree,
    core::NodeIndex node,
    const core::NodeIndex ancestor) noexcept {
    while (node != core::invalid_node && node < tree.nodes().size()) {
        if (node == ancestor) {
            return true;
        }
        node = tree.node(node).parent;
    }
    return false;
}

bool apply_history_entry(
    AnalyzerNavigationState& state,
    const core::ScanTree& tree,
    const NavigationEntry entry,
    const core::ScanTreeExclusion* const exclusion) noexcept {
    if (entry.view == MainContentView::Overview) {
        state.view = MainContentView::Overview;
        state.root = core::invalid_node;
        state.selected = core::invalid_node;
        return true;
    }
    if (!is_visible_destination(tree, entry.node, exclusion)) {
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
    const core::ScanTreeExclusion* const exclusion,
    Move&& move) noexcept {
    while (const auto entry = move()) {
        if (apply_history_entry(state, tree, *entry, exclusion)) {
            return true;
        }
    }
    return false;
}

bool has_visible_history_entry(
    const AnalyzerNavigationState& state,
    const core::ScanTree& tree,
    const core::ScanTreeExclusion* const exclusion,
    const bool forward) noexcept {
    const auto entries = state.history.entries();
    if (entries.empty()) {
        return false;
    }

    if (forward) {
        for (auto index = state.history.cursor() + 1U; index < entries.size(); ++index) {
            if (is_history_entry_visible(tree, entries[index], exclusion)) {
                return true;
            }
        }
        return false;
    }

    auto index = state.history.cursor();
    while (index > 0U) {
        --index;
        if (is_history_entry_visible(tree, entries[index], exclusion)) {
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

core::NodeIndex nearest_visible_directory(
    const core::ScanTree& tree,
    const core::ScanTreeExclusion& exclusion,
    core::NodeIndex node) noexcept {
    while (node != core::invalid_node && node < tree.nodes().size()) {
        if (is_directory(tree, node) && exclusion.is_visible(tree, node)) {
            return node;
        }
        node = tree.node(node).parent;
    }
    return core::invalid_node;
}

bool reconcile_analyzer_visibility(
    AnalyzerNavigationState& state,
    const core::ScanTree& tree,
    const core::ScanTreeExclusion& exclusion) noexcept {
    if (state.view != MainContentView::Analyzer) {
        return false;
    }

    const auto visibleRoot = nearest_visible_directory(tree, exclusion, state.root);
    if (visibleRoot == core::invalid_node) {
        return false;
    }
    if (visibleRoot != state.root) {
        state.root = visibleRoot;
        state.selected = visibleRoot;
        return true;
    }

    if (state.selected < tree.nodes().size()
        && exclusion.is_visible(tree, state.selected)
        && is_descendant_or_same(tree, state.selected, state.root)) {
        return false;
    }

    auto visibleSelection = nearest_visible_directory(tree, exclusion, state.selected);
    if (visibleSelection == core::invalid_node
        || !is_descendant_or_same(tree, visibleSelection, state.root)) {
        visibleSelection = state.root;
    }
    if (visibleSelection == state.selected) {
        return false;
    }
    state.selected = visibleSelection;
    return true;
}

bool navigation_can_back(
    const AnalyzerNavigationState& state,
    const core::ScanTree& tree,
    const core::ScanTreeExclusion* const exclusion) noexcept {
    return has_visible_history_entry(state, tree, exclusion, false);
}

bool navigation_can_forward(
    const AnalyzerNavigationState& state,
    const core::ScanTree& tree,
    const core::ScanTreeExclusion* const exclusion) noexcept {
    return has_visible_history_entry(state, tree, exclusion, true);
}

bool apply_analyzer_command(
    AnalyzerNavigationState& state,
    const core::ScanTree& tree,
    const AnalyzerCommand& command,
    const core::ScanTreeExclusion* const exclusion) noexcept {
    if (command.kind == AnalyzerCommandKind::NavigateBack) {
        return move_through_history(state, tree, exclusion, [&state]() noexcept {
            return state.history.back();
        });
    }
    if (command.kind == AnalyzerCommandKind::NavigateForward) {
        return move_through_history(state, tree, exclusion, [&state]() noexcept {
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
        if (!is_visible_destination(tree, command.node, exclusion)) {
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
        if (command.node >= tree.nodes().size()
            || (exclusion != nullptr && !exclusion->is_visible(tree, command.node))) {
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
    case AnalyzerCommandKind::RestoreReviewItem:
    case AnalyzerCommandKind::ConfirmReview:
        return false;
    }
    return false;
}

} // namespace diskbloom::app
