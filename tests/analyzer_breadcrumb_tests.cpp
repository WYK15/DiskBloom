#include "test_support.h"

#include "app/analyzer_breadcrumb.h"

#include <algorithm>
#include <array>
#include <vector>

using diskbloom::app::AnalyzerBreadcrumbModel;
using diskbloom::app::AnalyzerRectF;
using diskbloom::app::BreadcrumbHitKind;
using diskbloom::app::BreadcrumbItem;
using diskbloom::app::BreadcrumbItemKind;
using diskbloom::app::build_analyzer_breadcrumb;
using diskbloom::app::hit_test_analyzer_breadcrumb;
using diskbloom::app::layout_analyzer_breadcrumb;
using diskbloom::core::ScanNodeFlags;
using diskbloom::core::ScanTree;
using diskbloom::core::invalid_node;

TEST_CASE(breadcrumb_drive_scan_maps_every_tree_ancestor_to_clickable_nodes) {
    ScanTree tree;
    const auto drive = tree.add_root(L"C:\\", ScanNodeFlags::Directory);
    const auto users = tree.add_child(drive, L"Users", 0U, ScanNodeFlags::Directory);
    const auto leo = tree.add_child(users, L"leo", 0U, ScanNodeFlags::Directory);
    tree.aggregate();

    const auto model = build_analyzer_breadcrumb(tree, drive, leo, L"C:\\");
    CHECK(model.items.size() == 4U);
    CHECK(model.items[0].kind == BreadcrumbItemKind::Overview);
    CHECK(model.items[1].node == drive);
    CHECK(model.items[2].node == users);
    CHECK(model.items[3].node == leo);
    CHECK(model.items[1].enabled && model.items[2].enabled && model.items[3].enabled);
    CHECK(model.items[3].absolutePath == L"C:\\Users\\leo");
}

TEST_CASE(breadcrumb_folder_scan_disables_unscanned_absolute_prefix) {
    ScanTree tree;
    const auto root = tree.add_root(L"project", ScanNodeFlags::Directory);
    const auto src = tree.add_child(root, L"src", 0U, ScanNodeFlags::Directory);
    tree.aggregate();

    const auto model = build_analyzer_breadcrumb(tree, root, src, L"D:\\work\\project");
    CHECK(model.items.size() == 5U);
    CHECK(model.items[1].label == L"D:\\");
    CHECK(!model.items[1].enabled);
    CHECK(model.items[2].label == L"work");
    CHECK(!model.items[2].enabled);
    CHECK(model.items[3].node == root && model.items[3].enabled);
    CHECK(model.items[4].node == src && model.items[4].enabled);
    CHECK(model.items[4].absolutePath == L"D:\\work\\project\\src");
}

TEST_CASE(breadcrumb_rejects_invalid_or_unrelated_current_nodes) {
    ScanTree tree;
    const auto root = tree.add_root(L"C:\\", ScanNodeFlags::Directory);
    const auto child = tree.add_child(root, L"child", 0U, ScanNodeFlags::Directory);

    CHECK(build_analyzer_breadcrumb(tree, invalid_node, child, L"C:\\").items.empty());
    CHECK(build_analyzer_breadcrumb(tree, root, 99U, L"C:\\").items.empty());
}

TEST_CASE(breadcrumb_preserves_unicode_components) {
    ScanTree tree;
    const auto root = tree.add_root(L"project", ScanNodeFlags::Directory);
    const auto child = tree.add_child(root, L"\u8d44\u6e90", 0U, ScanNodeFlags::Directory);

    const auto model = build_analyzer_breadcrumb(
        tree,
        root,
        child,
        L"D:\\\u5de5\u4f5c\\project");
    CHECK(model.items[2].label == L"\u5de5\u4f5c");
    CHECK(model.items.back().label == L"\u8d44\u6e90");
    CHECK(model.items.back().absolutePath == L"D:\\\u5de5\u4f5c\\project\\\u8d44\u6e90");
}

TEST_CASE(breadcrumb_normalizes_trailing_separators_and_does_not_duplicate_scan_root) {
    ScanTree tree;
    const auto root = tree.add_root(L"project", ScanNodeFlags::Directory);

    const auto model = build_analyzer_breadcrumb(
        tree,
        root,
        root,
        L"D:\\work\\project\\");
    CHECK(model.items.size() == 4U);
    CHECK(model.items[3].label == L"project");
    CHECK(model.items[3].absolutePath == L"D:\\work\\project");
}

namespace {

[[nodiscard]] AnalyzerBreadcrumbModel make_layout_model() {
    AnalyzerBreadcrumbModel model;
    model.items = {
        BreadcrumbItem{.kind = BreadcrumbItemKind::Overview, .enabled = true},
        BreadcrumbItem{.kind = BreadcrumbItemKind::UnscannedPrefix, .label = L"D:\\"},
        BreadcrumbItem{.kind = BreadcrumbItemKind::ScanNode, .node = 0U, .label = L"project", .enabled = true},
        BreadcrumbItem{.kind = BreadcrumbItemKind::ScanNode, .node = 1U, .label = L"source", .enabled = true},
        BreadcrumbItem{.kind = BreadcrumbItemKind::ScanNode, .node = 2U, .label = L"app", .enabled = true},
        BreadcrumbItem{.kind = BreadcrumbItemKind::ScanNode, .node = 3U, .label = L"assets", .enabled = true},
    };
    return model;
}

} // namespace

TEST_CASE(breadcrumb_wide_layout_shows_every_item_in_path_order) {
    const auto model = make_layout_model();
    constexpr std::array widths{120.0F, 72.0F, 96.0F, 110.0F, 84.0F, 92.0F};
    constexpr AnalyzerRectF bounds{112.0F, 8.0F, 1'100.0F, 56.0F};

    const auto layout = layout_analyzer_breadcrumb(model, widths, bounds);
    CHECK(layout.visible.size() == model.items.size());
    CHECK(!layout.ellipsis.has_value());
    CHECK(layout.hiddenItemIndices.empty());
    for (std::size_t index = 0U; index < layout.visible.size(); ++index) {
        CHECK(layout.visible[index].itemIndex == index);
        CHECK(layout.visible[index].bounds.left >= bounds.left);
        CHECK(layout.visible[index].bounds.right <= bounds.right);
        if (index > 0U) {
            CHECK(layout.visible[index - 1U].bounds.right <= layout.visible[index].bounds.left);
        }
    }
}

TEST_CASE(breadcrumb_compact_layout_keeps_overview_root_parent_current_and_ellipsis) {
    const auto model = make_layout_model();
    constexpr std::array widths{120.0F, 72.0F, 96.0F, 110.0F, 84.0F, 92.0F};
    constexpr AnalyzerRectF bounds{112.0F, 8.0F, 650.0F, 56.0F};

    const auto layout = layout_analyzer_breadcrumb(model, widths, bounds);
    CHECK(layout.visible.front().itemIndex == 0U);
    CHECK(layout.visible[1U].itemIndex == 2U);
    CHECK(layout.visible[layout.visible.size() - 2U].itemIndex == model.items.size() - 2U);
    CHECK(layout.visible.back().itemIndex == model.items.size() - 1U);
    CHECK(layout.ellipsis.has_value());
    CHECK(!layout.hiddenItemIndices.empty());

    std::vector<AnalyzerRectF> rectangles;
    for (const auto& segment : layout.visible) {
        rectangles.push_back(segment.bounds);
    }
    rectangles.push_back(layout.ellipsis->bounds);
    std::ranges::sort(rectangles, {}, &AnalyzerRectF::left);
    for (std::size_t index = 0U; index < rectangles.size(); ++index) {
        CHECK(rectangles[index].left >= bounds.left);
        CHECK(rectangles[index].right <= bounds.right);
        if (index > 0U) {
            CHECK(rectangles[index - 1U].right <= rectangles[index].left);
        }
    }
}

TEST_CASE(breadcrumb_layout_fits_english_and_chinese_width_budgets) {
    const auto model = make_layout_model();
    constexpr std::array english{122.0F, 48.0F, 80.0F, 72.0F, 52.0F, 68.0F};
    constexpr std::array chinese{104.0F, 48.0F, 80.0F, 72.0F, 52.0F, 68.0F};
    for (const auto width : {688.0F, 1'088.0F}) {
        const AnalyzerRectF bounds{112.0F, 8.0F, 112.0F + width, 56.0F};
        for (const auto& measurements : {english, chinese}) {
            const auto layout = layout_analyzer_breadcrumb(model, measurements, bounds);
            for (const auto& segment : layout.visible) {
                CHECK(segment.bounds.left >= bounds.left);
                CHECK(segment.bounds.right <= bounds.right);
            }
            if (layout.ellipsis.has_value()) {
                CHECK(layout.ellipsis->bounds.left >= bounds.left);
                CHECK(layout.ellipsis->bounds.right <= bounds.right);
            }
        }
    }
}

TEST_CASE(breadcrumb_hit_test_uses_half_open_edges) {
    const auto model = make_layout_model();
    constexpr std::array widths{120.0F, 72.0F, 96.0F, 110.0F, 84.0F, 92.0F};
    const auto layout = layout_analyzer_breadcrumb(
        model,
        widths,
        AnalyzerRectF{112.0F, 8.0F, 650.0F, 56.0F});
    const auto first = layout.visible.front();

    const auto inside = hit_test_analyzer_breadcrumb(
        layout,
        first.bounds.left,
        first.bounds.top);
    CHECK(inside.has_value());
    CHECK(inside->kind == BreadcrumbHitKind::Item);
    CHECK(inside->itemIndex == first.itemIndex);
    CHECK(!hit_test_analyzer_breadcrumb(
        layout,
        first.bounds.right,
        first.bounds.bottom).has_value());

    CHECK(layout.ellipsis.has_value());
    const auto ellipsis = hit_test_analyzer_breadcrumb(
        layout,
        layout.ellipsis->bounds.left,
        layout.ellipsis->bounds.top);
    CHECK(ellipsis.has_value());
    CHECK(ellipsis->kind == BreadcrumbHitKind::Ellipsis);
}
