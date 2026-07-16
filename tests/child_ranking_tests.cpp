#include "test_support.h"

#include "core/child_ranking.h"
#include "core/scan_tree_exclusion.h"

#include <array>

using diskbloom::core::ScanNodeFlags;
using diskbloom::core::ScanTree;
using diskbloom::core::ScanTreeExclusion;
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

TEST_CASE(child_ranking_omits_excluded_nodes_and_uses_effective_sizes) {
    ScanTree tree;
    const auto root = tree.add_root(L"root", ScanNodeFlags::Directory);
    const auto folder = tree.add_child(root, L"folder", 0U, ScanNodeFlags::Directory);
    (void)tree.add_child(folder, L"kept.bin", 10U, ScanNodeFlags::None);
    const auto removed = tree.add_child(folder, L"removed.bin", 20U, ScanNodeFlags::None);
    const auto sibling = tree.add_child(root, L"sibling.bin", 15U, ScanNodeFlags::None);
    tree.aggregate();
    ScanTreeExclusion exclusion;
    const std::array requested{removed};
    exclusion.rebuild(tree, requested);

    const auto ranked = rank_children(tree, root, 24U, &exclusion);

    CHECK(ranked.size() == 2U);
    CHECK(ranked[0].node == sibling);
    CHECK(ranked[0].logicalSize == 15U);
    CHECK(ranked[1].node == folder);
    CHECK(ranked[1].logicalSize == 10U);
}

TEST_CASE(child_ranking_drops_zero_byte_projected_directories) {
    ScanTree tree;
    const auto root = tree.add_root(L"root", ScanNodeFlags::Directory);
    const auto folder = tree.add_child(root, L"folder", 0U, ScanNodeFlags::Directory);
    const auto child = tree.add_child(folder, L"child.bin", 10U, ScanNodeFlags::None);
    tree.aggregate();
    ScanTreeExclusion exclusion;
    const std::array requested{child};
    exclusion.rebuild(tree, requested);

    CHECK(rank_children(tree, root, 24U, &exclusion).empty());
}

TEST_CASE(child_ranking_keeps_node_order_for_projected_size_ties) {
    ScanTree tree;
    const auto root = tree.add_root(L"root", ScanNodeFlags::Directory);
    const auto first = tree.add_child(root, L"first", 0U, ScanNodeFlags::Directory);
    (void)tree.add_child(first, L"kept.bin", 10U, ScanNodeFlags::None);
    const auto removed = tree.add_child(first, L"removed.bin", 10U, ScanNodeFlags::None);
    const auto second = tree.add_child(root, L"second.bin", 10U, ScanNodeFlags::None);
    tree.aggregate();
    ScanTreeExclusion exclusion;
    const std::array requested{removed};
    exclusion.rebuild(tree, requested);

    const auto ranked = rank_children(tree, root, 24U, &exclusion);

    CHECK(ranked.size() == 2U);
    CHECK(ranked[0].node == first);
    CHECK(ranked[1].node == second);
}

TEST_CASE(child_ranking_null_projection_preserves_zero_sized_children) {
    ScanTree tree;
    const auto root = tree.add_root(L"root", ScanNodeFlags::Directory);
    const auto empty = tree.add_child(root, L"empty", 0U, ScanNodeFlags::Directory);
    tree.aggregate();

    const auto legacy = rank_children(tree, root, 24U);
    const auto explicitNull = rank_children(tree, root, 24U, nullptr);

    CHECK(legacy.size() == 1U);
    CHECK(explicitNull.size() == legacy.size());
    CHECK(explicitNull[0].node == empty);
    CHECK(explicitNull[0].logicalSize == 0U);
}
