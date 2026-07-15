#include "test_support.h"

#include "core/node_path.h"

using diskbloom::core::ScanNodeFlags;
using diskbloom::core::ScanTree;
using diskbloom::core::build_node_path;

TEST_CASE(node_path_returns_scan_root_without_duplication) {
    ScanTree tree;
    const auto root = tree.add_root(L"C:\\", ScanNodeFlags::Directory);
    const auto path = build_node_path(tree, root, L"C:\\");
    CHECK(path.has_value());
    CHECK(*path == L"C:\\");
}

TEST_CASE(node_path_appends_nested_names_to_volume_root) {
    ScanTree tree;
    const auto root = tree.add_root(L"C:\\", ScanNodeFlags::Directory);
    const auto folder = tree.add_child(root, L"folder", 0U, ScanNodeFlags::Directory);
    const auto file = tree.add_child(folder, L"data.bin", 10U, ScanNodeFlags::None);

    const auto path = build_node_path(tree, file, L"C:\\");
    CHECK(path.has_value());
    CHECK(*path == L"C:\\folder\\data.bin");
}

TEST_CASE(node_path_supports_explicit_folder_scan_roots) {
    ScanTree tree;
    const auto root = tree.add_root(L"picked", ScanNodeFlags::Directory);
    const auto child = tree.add_child(root, L"child", 0U, ScanNodeFlags::Directory);

    const auto path = build_node_path(tree, child, L"D:\\work\\picked");
    CHECK(path.has_value());
    CHECK(*path == L"D:\\work\\picked\\child");
}

TEST_CASE(node_path_rejects_invalid_node_or_empty_root) {
    ScanTree tree;
    const auto root = tree.add_root(L"root", ScanNodeFlags::Directory);
    CHECK(!build_node_path(tree, 99U, L"C:\\").has_value());
    CHECK(!build_node_path(tree, root, {}).has_value());
}
