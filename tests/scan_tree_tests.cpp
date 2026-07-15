#include "test_support.h"

#include "core/scan_tree.h"

using diskbloom::core::ScanNodeFlags;
using diskbloom::core::ScanTree;
using diskbloom::core::has_flag;
using diskbloom::core::invalid_node;

TEST_CASE(scan_tree_aggregates_nested_files) {
    ScanTree tree;
    const auto root = tree.add_root(L"C:", ScanNodeFlags::Directory);
    const auto folder = tree.add_child(root, L"folder", 0U, ScanNodeFlags::Directory);
    const auto first_file = tree.add_child(folder, L"a.bin", 10U, ScanNodeFlags::None);
    const auto second_file = tree.add_child(folder, L"b.bin", 20U, ScanNodeFlags::None);
    CHECK(first_file != invalid_node);
    CHECK(second_file != invalid_node);

    tree.aggregate();

    CHECK(tree.node(root).logicalSize == 30U);
    CHECK(tree.node(root).fileCount == 2U);
    CHECK(tree.node(root).directoryCount == 1U);
    CHECK(tree.node(folder).logicalSize == 30U);
}

TEST_CASE(scan_tree_keeps_names_and_links_in_contiguous_storage) {
    ScanTree tree;
    tree.reserve(4U, 64U);
    const auto root = tree.add_root(L"root", ScanNodeFlags::Directory);
    const auto first = tree.add_child(root, L"\u6587\u4ef6.txt", 4U, ScanNodeFlags::None);
    const auto second = tree.add_child(root, L"second.bin", 8U, ScanNodeFlags::None);

    CHECK(tree.name(first) == L"\u6587\u4ef6.txt");
    CHECK(tree.name(second) == L"second.bin");
    CHECK(tree.node(root).firstChild == second);
    CHECK(tree.node(second).nextSibling == first);
    CHECK(tree.nodes().size() == 3U);
}

TEST_CASE(scan_tree_aggregation_is_idempotent) {
    ScanTree tree;
    const auto root = tree.add_root(L"root", ScanNodeFlags::Directory);
    const auto file = tree.add_child(root, L"file", 99U, ScanNodeFlags::None);
    CHECK(file != invalid_node);
    tree.aggregate();
    tree.aggregate();
    CHECK(tree.node(root).logicalSize == 99U);
    CHECK(tree.node(root).fileCount == 1U);
}

TEST_CASE(scan_tree_rejects_invalid_parent) {
    ScanTree tree;
    const auto root = tree.add_root(L"root", ScanNodeFlags::Directory);
    const auto result = tree.add_child(invalid_node, L"bad", 1U, ScanNodeFlags::None);
    CHECK(result == invalid_node);
    CHECK(tree.nodes().size() == 1U);
    CHECK(tree.node(root).firstChild == invalid_node);
}

TEST_CASE(scan_tree_can_mark_existing_node_incomplete) {
    ScanTree tree;
    const auto root = tree.add_root(L"root", ScanNodeFlags::Directory);
    tree.add_flags(root, ScanNodeFlags::Inaccessible | ScanNodeFlags::Incomplete);
    CHECK(has_flag(tree.node(root).flags, ScanNodeFlags::Inaccessible));
    CHECK(has_flag(tree.node(root).flags, ScanNodeFlags::Incomplete));
}
