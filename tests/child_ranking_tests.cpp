#include "test_support.h"

#include "core/child_ranking.h"

using diskbloom::core::ScanNodeFlags;
using diskbloom::core::ScanTree;
using diskbloom::core::rank_children;

TEST_CASE(child_ranking_returns_largest_direct_children) {
    ScanTree tree;
    const auto root = tree.add_root(L"root", ScanNodeFlags::Directory);
    const auto small = tree.add_child(root, L"small", 10U, ScanNodeFlags::None);
    const auto large = tree.add_child(root, L"large", 30U, ScanNodeFlags::None);
    const auto medium = tree.add_child(root, L"medium", 20U, ScanNodeFlags::None);
    tree.aggregate();

    const auto ranked = rank_children(tree, root, 2U);
    CHECK(ranked.size() == 2U);
    CHECK(ranked[0].node == large);
    CHECK(ranked[0].logicalSize == 30U);
    CHECK(ranked[1].node == medium);
    CHECK(ranked[1].logicalSize == 20U);
    CHECK(ranked[1].node != small);
}

TEST_CASE(child_ranking_uses_node_index_to_break_size_ties) {
    ScanTree tree;
    const auto root = tree.add_root(L"root", ScanNodeFlags::Directory);
    const auto first = tree.add_child(root, L"first", 10U, ScanNodeFlags::None);
    const auto second = tree.add_child(root, L"second", 10U, ScanNodeFlags::None);
    tree.aggregate();

    const auto ranked = rank_children(tree, root, 2U);
    CHECK(ranked.size() == 2U);
    CHECK(ranked[0].node == first);
    CHECK(ranked[1].node == second);
}

TEST_CASE(child_ranking_rejects_invalid_parent_or_zero_limit) {
    ScanTree tree;
    const auto root = tree.add_root(L"root", ScanNodeFlags::Directory);
    CHECK(rank_children(tree, root, 0U).empty());
    CHECK(rank_children(tree, 88U, 10U).empty());
}
