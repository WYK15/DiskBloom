#include "test_support.h"

#include "app/analyzer_navigation.h"
#include "core/scan_tree_exclusion.h"

#include <array>

using diskbloom::app::AnalyzerCommand;
using diskbloom::app::AnalyzerCommandKind;
using diskbloom::app::AnalyzerNavigationState;
using diskbloom::app::MainContentView;
using diskbloom::app::apply_analyzer_command;
using diskbloom::app::nearest_visible_directory;
using diskbloom::app::navigation_can_back;
using diskbloom::app::navigation_can_forward;
using diskbloom::app::open_analyzer;
using diskbloom::app::reconcile_analyzer_visibility;
using diskbloom::core::ScanNodeFlags;
using diskbloom::core::ScanTree;
using diskbloom::core::ScanTreeExclusion;

namespace {

struct NavigationFixture {
    ScanTree tree;
    diskbloom::core::NodeIndex root;
    diskbloom::core::NodeIndex folder;
    diskbloom::core::NodeIndex file;

    NavigationFixture()
        : root(tree.add_root(L"root", ScanNodeFlags::Directory)),
          folder(tree.add_child(root, L"folder", 0U, ScanNodeFlags::Directory)),
          file(tree.add_child(folder, L"file", 10U, ScanNodeFlags::None)) {
        tree.aggregate();
    }
};

} // namespace

TEST_CASE(analyzer_navigation_opens_completed_tree_at_root) {
    NavigationFixture fixture;
    AnalyzerNavigationState state;

    CHECK(open_analyzer(state, fixture.tree, fixture.root));
    CHECK(state.view == MainContentView::Analyzer);
    CHECK(state.root == fixture.root);
    CHECK(state.selected == fixture.root);
    CHECK(state.history.can_back());
}

TEST_CASE(analyzer_navigation_drills_into_directory) {
    NavigationFixture fixture;
    AnalyzerNavigationState state;
    (void)open_analyzer(state, fixture.tree, fixture.root);

    CHECK(apply_analyzer_command(
        state,
        fixture.tree,
        {AnalyzerCommandKind::NavigateToNode, fixture.folder}));
    CHECK(state.root == fixture.folder);
    CHECK(state.selected == fixture.folder);
}

TEST_CASE(analyzer_navigation_selects_file_without_changing_root) {
    NavigationFixture fixture;
    AnalyzerNavigationState state;
    (void)open_analyzer(state, fixture.tree, fixture.folder);

    CHECK(apply_analyzer_command(
        state,
        fixture.tree,
        {AnalyzerCommandKind::SelectNode, fixture.file}));
    CHECK(state.root == fixture.folder);
    CHECK(state.selected == fixture.file);
}

TEST_CASE(analyzer_navigation_returns_to_parent_then_overview) {
    NavigationFixture fixture;
    AnalyzerNavigationState state;
    (void)open_analyzer(state, fixture.tree, fixture.folder);

    CHECK(apply_analyzer_command(
        state,
        fixture.tree,
        {AnalyzerCommandKind::NavigateToParent, fixture.root}));
    CHECK(state.view == MainContentView::Analyzer);
    CHECK(state.root == fixture.root);

    CHECK(apply_analyzer_command(
        state,
        fixture.tree,
        {AnalyzerCommandKind::ReturnToOverview, diskbloom::core::invalid_node}));
    CHECK(state.view == MainContentView::Overview);
}

TEST_CASE(analyzer_navigation_rejects_file_as_chart_root) {
    NavigationFixture fixture;
    AnalyzerNavigationState state;
    (void)open_analyzer(state, fixture.tree, fixture.root);

    CHECK(!apply_analyzer_command(
        state,
        fixture.tree,
        AnalyzerCommand{AnalyzerCommandKind::NavigateToNode, fixture.file}));
    CHECK(state.root == fixture.root);
}

TEST_CASE(analyzer_navigation_moves_back_and_forward_across_overview_and_directories) {
    NavigationFixture fixture;
    AnalyzerNavigationState state;
    (void)open_analyzer(state, fixture.tree, fixture.root);
    CHECK(apply_analyzer_command(
        state,
        fixture.tree,
        {AnalyzerCommandKind::NavigateBreadcrumb, fixture.folder}));

    CHECK(apply_analyzer_command(
        state,
        fixture.tree,
        {AnalyzerCommandKind::NavigateBack, diskbloom::core::invalid_node}));
    CHECK(state.view == MainContentView::Analyzer);
    CHECK(state.root == fixture.root);

    CHECK(apply_analyzer_command(
        state,
        fixture.tree,
        {AnalyzerCommandKind::NavigateBack, diskbloom::core::invalid_node}));
    CHECK(state.view == MainContentView::Overview);

    CHECK(apply_analyzer_command(
        state,
        fixture.tree,
        {AnalyzerCommandKind::NavigateForward, diskbloom::core::invalid_node}));
    CHECK(state.view == MainContentView::Analyzer);
    CHECK(state.root == fixture.root);
}

TEST_CASE(analyzer_navigation_branch_clears_forward_history) {
    NavigationFixture fixture;
    const auto sibling = fixture.tree.add_child(
        fixture.root,
        L"sibling",
        0U,
        ScanNodeFlags::Directory);
    AnalyzerNavigationState state;
    (void)open_analyzer(state, fixture.tree, fixture.root);
    (void)apply_analyzer_command(
        state,
        fixture.tree,
        {AnalyzerCommandKind::NavigateToNode, fixture.folder});
    (void)apply_analyzer_command(
        state,
        fixture.tree,
        {AnalyzerCommandKind::NavigateBack, diskbloom::core::invalid_node});

    CHECK(apply_analyzer_command(
        state,
        fixture.tree,
        {AnalyzerCommandKind::NavigateBreadcrumb, sibling}));
    CHECK(!state.history.can_forward());
}

TEST_CASE(analyzer_navigation_skips_stale_history_nodes) {
    NavigationFixture fixture;
    AnalyzerNavigationState state;
    (void)open_analyzer(state, fixture.tree, fixture.root);
    state.history.reset({MainContentView::Analyzer, fixture.root});
    (void)state.history.record({MainContentView::Analyzer, 99U});
    (void)state.history.record({MainContentView::Analyzer, fixture.folder});
    state.root = fixture.folder;
    state.selected = fixture.folder;

    CHECK(apply_analyzer_command(
        state,
        fixture.tree,
        {AnalyzerCommandKind::NavigateBack, diskbloom::core::invalid_node}));
    CHECK(state.root == fixture.root);
    CHECK(apply_analyzer_command(
        state,
        fixture.tree,
        {AnalyzerCommandKind::NavigateForward, diskbloom::core::invalid_node}));
    CHECK(state.root == fixture.folder);
}

TEST_CASE(analyzer_navigation_rejects_invalid_breadcrumb_and_overflow_commands) {
    NavigationFixture fixture;
    AnalyzerNavigationState state;
    (void)open_analyzer(state, fixture.tree, fixture.root);
    const auto historyCanBack = state.history.can_back();

    CHECK(!apply_analyzer_command(
        state,
        fixture.tree,
        {AnalyzerCommandKind::NavigateBreadcrumb, fixture.file}));
    CHECK(!apply_analyzer_command(
        state,
        fixture.tree,
        {AnalyzerCommandKind::NavigateBreadcrumb, diskbloom::core::invalid_node}));
    CHECK(!apply_analyzer_command(
        state,
        fixture.tree,
        {AnalyzerCommandKind::OpenBreadcrumbOverflow, diskbloom::core::invalid_node}));
    CHECK(state.root == fixture.root);
    CHECK(state.history.can_back() == historyCanBack);
}

TEST_CASE(analyzer_navigation_new_scan_replaces_old_history) {
    NavigationFixture first;
    AnalyzerNavigationState state;
    (void)open_analyzer(state, first.tree, first.root);
    (void)apply_analyzer_command(
        state,
        first.tree,
        {AnalyzerCommandKind::NavigateToNode, first.folder});

    ScanTree replacement;
    const auto replacementRoot = replacement.add_root(L"replacement", ScanNodeFlags::Directory);
    CHECK(open_analyzer(state, replacement, replacementRoot));
    CHECK(apply_analyzer_command(
        state,
        replacement,
        {AnalyzerCommandKind::NavigateBack, diskbloom::core::invalid_node}));
    CHECK(state.view == MainContentView::Overview);
    CHECK(!state.history.can_back());
}

TEST_CASE(analyzer_navigation_reconciles_hidden_root_to_visible_parent) {
    NavigationFixture fixture;
    AnalyzerNavigationState state;
    (void)open_analyzer(state, fixture.tree, fixture.root);
    (void)apply_analyzer_command(
        state,
        fixture.tree,
        {AnalyzerCommandKind::NavigateToNode, fixture.folder});
    ScanTreeExclusion exclusion;
    const std::array requested{fixture.folder};
    exclusion.rebuild(fixture.tree, requested);

    CHECK(nearest_visible_directory(fixture.tree, exclusion, fixture.folder) == fixture.root);
    CHECK(reconcile_analyzer_visibility(state, fixture.tree, exclusion));
    CHECK(state.root == fixture.root);
    CHECK(state.selected == fixture.root);
    CHECK(state.history.current()->node == fixture.folder);
}

TEST_CASE(analyzer_navigation_reconciles_hidden_selection_to_visible_directory) {
    NavigationFixture fixture;
    AnalyzerNavigationState state;
    (void)open_analyzer(state, fixture.tree, fixture.folder);
    (void)apply_analyzer_command(
        state,
        fixture.tree,
        {AnalyzerCommandKind::SelectNode, fixture.file});
    ScanTreeExclusion exclusion;
    const std::array requested{fixture.file};
    exclusion.rebuild(fixture.tree, requested);

    CHECK(reconcile_analyzer_visibility(state, fixture.tree, exclusion));
    CHECK(state.root == fixture.folder);
    CHECK(state.selected == fixture.folder);
}

TEST_CASE(analyzer_navigation_skips_hidden_history_and_reaches_it_after_restore) {
    NavigationFixture fixture;
    AnalyzerNavigationState state;
    (void)open_analyzer(state, fixture.tree, fixture.root);
    (void)apply_analyzer_command(
        state,
        fixture.tree,
        {AnalyzerCommandKind::NavigateToNode, fixture.folder});
    ScanTreeExclusion exclusion;
    const std::array requested{fixture.folder};
    exclusion.rebuild(fixture.tree, requested);
    (void)reconcile_analyzer_visibility(state, fixture.tree, exclusion);

    CHECK(navigation_can_back(state, fixture.tree, &exclusion));
    CHECK(!navigation_can_forward(state, fixture.tree, &exclusion));
    CHECK(apply_analyzer_command(
        state,
        fixture.tree,
        {AnalyzerCommandKind::NavigateBack},
        &exclusion));
    CHECK(state.root == fixture.root);
    CHECK(!navigation_can_forward(state, fixture.tree, &exclusion));

    exclusion.clear();
    CHECK(navigation_can_forward(state, fixture.tree, &exclusion));
    CHECK(apply_analyzer_command(
        state,
        fixture.tree,
        {AnalyzerCommandKind::NavigateForward},
        &exclusion));
    CHECK(state.root == fixture.folder);
}

TEST_CASE(analyzer_navigation_rejects_hidden_direct_destinations) {
    NavigationFixture fixture;
    AnalyzerNavigationState state;
    (void)open_analyzer(state, fixture.tree, fixture.root);
    ScanTreeExclusion exclusion;
    const std::array requested{fixture.folder};
    exclusion.rebuild(fixture.tree, requested);

    CHECK(!apply_analyzer_command(
        state,
        fixture.tree,
        {AnalyzerCommandKind::NavigateBreadcrumb, fixture.folder},
        &exclusion));
    CHECK(!apply_analyzer_command(
        state,
        fixture.tree,
        {AnalyzerCommandKind::SelectNode, fixture.file},
        &exclusion));
    CHECK(state.root == fixture.root);
    CHECK(state.selected == fixture.root);
}
