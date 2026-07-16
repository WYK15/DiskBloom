#include "test_support.h"

#include "core/scan_tree_exclusion.h"

#include <array>

using diskbloom::core::NodeIndex;
using diskbloom::core::ScanNodeFlags;
using diskbloom::core::ScanTree;
using diskbloom::core::ScanTreeExclusion;
using diskbloom::core::invalid_node;

namespace {

struct ExclusionFixture {
    ScanTree tree;
    NodeIndex root;
    NodeIndex users;
    NodeIndex alice;
    NodeIndex cache;
    NodeIndex document;
    NodeIndex temp;
    NodeIndex other;

    ExclusionFixture()
        : root(tree.add_root(L"root", ScanNodeFlags::Directory)),
          users(tree.add_child(root, L"users", 0U, ScanNodeFlags::Directory)),
          alice(tree.add_child(users, L"alice", 0U, ScanNodeFlags::Directory)),
          cache(tree.add_child(alice, L"cache.bin", 40U, ScanNodeFlags::None)),
          document(tree.add_child(alice, L"document.bin", 10U, ScanNodeFlags::None)),
          temp(tree.add_child(root, L"temp.bin", 20U, ScanNodeFlags::None)),
          other(tree.add_child(root, L"other.bin", 30U, ScanNodeFlags::None)) {
        tree.aggregate();
    }
};

} // namespace

TEST_CASE(scan_tree_exclusion_normalizes_roots_and_ancestor_descendants) {
    ExclusionFixture fixture;
    ScanTreeExclusion exclusion;
    const std::array requested{
        fixture.cache,
        fixture.alice,
        fixture.cache,
        fixture.root,
        invalid_node,
        static_cast<NodeIndex>(999U),
    };

    exclusion.rebuild(fixture.tree, requested);

    CHECK(exclusion.roots().size() == 1U);
    CHECK(exclusion.roots()[0] == fixture.alice);
    CHECK(exclusion.is_excluded_root(fixture.alice));
    CHECK(!exclusion.is_excluded_root(fixture.cache));
    CHECK(!exclusion.is_visible(fixture.tree, fixture.cache));
    CHECK(exclusion.effective_size(fixture.tree, fixture.cache) == 0U);
}

TEST_CASE(scan_tree_exclusion_reports_visibility_and_effective_sizes) {
    ExclusionFixture fixture;
    ScanTreeExclusion exclusion;
    const std::array requested{fixture.cache, fixture.temp};
    exclusion.rebuild(fixture.tree, requested);

    CHECK(exclusion.effective_size(fixture.tree, fixture.root) == 40U);
    CHECK(exclusion.effective_size(fixture.tree, fixture.users) == 10U);
    CHECK(exclusion.effective_size(fixture.tree, fixture.alice) == 10U);
    CHECK(exclusion.effective_size(fixture.tree, fixture.cache) == 0U);
    CHECK(exclusion.effective_size(fixture.tree, fixture.document) == 10U);
    CHECK(!exclusion.is_visible(fixture.tree, fixture.cache));
    CHECK(!exclusion.is_visible(fixture.tree, fixture.temp));
    CHECK(exclusion.is_visible(fixture.tree, fixture.document));
    CHECK(exclusion.is_visible(fixture.tree, fixture.other));
    CHECK(!exclusion.is_visible(fixture.tree, invalid_node));
}

TEST_CASE(scan_tree_exclusion_keeps_sparse_sorted_reductions) {
    ExclusionFixture fixture;
    ScanTreeExclusion exclusion;
    const std::array requested{fixture.cache, fixture.temp};
    exclusion.rebuild(fixture.tree, requested);

    const auto reductions = exclusion.reductions();
    CHECK(reductions.size() == 3U);
    CHECK(reductions[0].node == fixture.root);
    CHECK(reductions[0].bytes == 60U);
    CHECK(reductions[1].node == fixture.users);
    CHECK(reductions[1].bytes == 40U);
    CHECK(reductions[2].node == fixture.alice);
    CHECK(reductions[2].bytes == 40U);
}

TEST_CASE(scan_tree_exclusion_clear_restores_full_tree) {
    ExclusionFixture fixture;
    ScanTreeExclusion exclusion;
    const std::array requested{fixture.alice};
    exclusion.rebuild(fixture.tree, requested);
    CHECK(!exclusion.empty());

    exclusion.clear();

    CHECK(exclusion.empty());
    CHECK(exclusion.roots().empty());
    CHECK(exclusion.reductions().empty());
    CHECK(exclusion.effective_size(fixture.tree, fixture.root) == 100U);
    CHECK(exclusion.is_visible(fixture.tree, fixture.cache));
}

TEST_CASE(scan_tree_exclusion_saturates_inconsistent_unaggregated_sizes) {
    ScanTree tree;
    const auto root = tree.add_root(L"root", ScanNodeFlags::Directory);
    const auto file = tree.add_child(root, L"file.bin", 25U, ScanNodeFlags::None);
    ScanTreeExclusion exclusion;
    const std::array requested{file};

    exclusion.rebuild(tree, requested);

    CHECK(exclusion.effective_size(tree, root) == 0U);
}
