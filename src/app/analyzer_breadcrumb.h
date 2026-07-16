#pragma once

#include "app/analyzer_geometry.h"
#include "core/scan_tree.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace diskbloom::app {

enum class BreadcrumbItemKind : std::uint8_t {
    Overview,
    UnscannedPrefix,
    ScanNode,
    Ellipsis,
};

struct BreadcrumbItem {
    BreadcrumbItemKind kind = BreadcrumbItemKind::Overview;
    core::NodeIndex node = core::invalid_node;
    std::wstring label;
    std::wstring absolutePath;
    bool enabled = false;
};

struct AnalyzerBreadcrumbModel {
    std::vector<BreadcrumbItem> items;
};

struct BreadcrumbSegmentLayout {
    AnalyzerRectF bounds;
    std::size_t itemIndex = 0U;
};

struct AnalyzerBreadcrumbLayout {
    std::vector<BreadcrumbSegmentLayout> visible;
    std::optional<BreadcrumbSegmentLayout> ellipsis;
    std::vector<std::size_t> hiddenItemIndices;
};

enum class BreadcrumbHitKind : std::uint8_t {
    Item,
    Ellipsis,
};

struct BreadcrumbHit {
    BreadcrumbHitKind kind = BreadcrumbHitKind::Item;
    std::size_t itemIndex = 0U;
};

[[nodiscard]] AnalyzerBreadcrumbModel build_analyzer_breadcrumb(
    const core::ScanTree& tree,
    core::NodeIndex scanRoot,
    core::NodeIndex currentRoot,
    std::wstring_view absoluteScanRoot);

[[nodiscard]] AnalyzerBreadcrumbLayout layout_analyzer_breadcrumb(
    const AnalyzerBreadcrumbModel& model,
    std::span<const float> measuredLabelWidths,
    AnalyzerRectF bounds);

[[nodiscard]] std::optional<BreadcrumbHit> hit_test_analyzer_breadcrumb(
    const AnalyzerBreadcrumbLayout& layout,
    float xDip,
    float yDip) noexcept;

} // namespace diskbloom::app
