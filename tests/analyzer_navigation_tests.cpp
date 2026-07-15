#include "test_support.h"

#include "app/analyzer_navigation.h"

using diskbloom::app::AnalyzerCommand;
using diskbloom::app::AnalyzerCommandKind;
using diskbloom::app::AnalyzerNavigationState;
using diskbloom::app::MainContentView;
using diskbloom::app::apply_analyzer_command;
using diskbloom::app::open_analyzer;
using diskbloom::core::ScanNodeFlags;
using diskbloom::core::ScanTree;

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
