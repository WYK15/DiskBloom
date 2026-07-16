#include "app/analyzer_breadcrumb.h"

#include <algorithm>
#include <filesystem>
#include <limits>
#include <numeric>

namespace diskbloom::app {
namespace {

[[nodiscard]] bool is_directory(
    const core::ScanTree& tree,
    const core::NodeIndex node) noexcept {
    return node < tree.nodes().size()
        && core::has_flag(tree.node(node).flags, core::ScanNodeFlags::Directory);
}

[[nodiscard]] std::wstring display_path(const std::filesystem::path& path) {
    auto value = path.native();
    const auto root = path.root_path().native();
    while (value.size() > root.size() && (value.back() == L'\\' || value.back() == L'/')) {
        value.pop_back();
    }
    return value;
}

[[nodiscard]] bool contains(
    const AnalyzerRectF bounds,
    const float xDip,
    const float yDip) noexcept {
    return xDip >= bounds.left && xDip < bounds.right
        && yDip >= bounds.top && yDip < bounds.bottom;
}

} // namespace

AnalyzerBreadcrumbModel build_analyzer_breadcrumb(
    const core::ScanTree& tree,
    const core::NodeIndex scanRoot,
    const core::NodeIndex currentRoot,
    const std::wstring_view absoluteScanRoot) {
    AnalyzerBreadcrumbModel model;
    if (!is_directory(tree, scanRoot)
        || !is_directory(tree, currentRoot)
        || absoluteScanRoot.empty()) {
        return model;
    }

    std::vector<core::NodeIndex> scannedAncestors;
    for (auto node = currentRoot; node != core::invalid_node; node = tree.node(node).parent) {
        scannedAncestors.push_back(node);
        if (node == scanRoot) {
            break;
        }
        if (tree.node(node).parent >= tree.nodes().size()) {
            return {};
        }
    }
    if (scannedAncestors.empty() || scannedAncestors.back() != scanRoot) {
        return {};
    }

    const auto scanPath = std::filesystem::path(absoluteScanRoot).lexically_normal();
    if (scanPath.empty()) {
        return {};
    }

    model.items.push_back({
        .kind = BreadcrumbItemKind::Overview,
        .enabled = true,
    });

    std::vector<std::filesystem::path> relativeComponents;
    for (const auto& component : scanPath.relative_path()) {
        if (!component.empty() && component != L".") {
            relativeComponents.push_back(component);
        }
    }

    auto prefixPath = scanPath.root_path();
    if (!relativeComponents.empty() && !prefixPath.empty()) {
        model.items.push_back({
            .kind = BreadcrumbItemKind::UnscannedPrefix,
            .label = prefixPath.native(),
            .absolutePath = display_path(prefixPath),
            .enabled = false,
        });
    }
    for (std::size_t index = 0U; index + 1U < relativeComponents.size(); ++index) {
        prefixPath /= relativeComponents[index];
        model.items.push_back({
            .kind = BreadcrumbItemKind::UnscannedPrefix,
            .label = relativeComponents[index].native(),
            .absolutePath = display_path(prefixPath),
            .enabled = false,
        });
    }

    const auto normalizedScanPath = display_path(scanPath);
    auto scannedPath = std::filesystem::path(normalizedScanPath);
    for (auto iterator = scannedAncestors.rbegin(); iterator != scannedAncestors.rend(); ++iterator) {
        const auto node = *iterator;
        const auto isScanRoot = node == scanRoot;
        if (!isScanRoot) {
            scannedPath /= tree.name(node);
        }

        auto label = std::wstring(tree.name(node));
        if (isScanRoot && relativeComponents.empty() && !scanPath.root_path().empty()) {
            label = scanPath.root_path().native();
        }
        model.items.push_back({
            .kind = BreadcrumbItemKind::ScanNode,
            .node = node,
            .label = std::move(label),
            .absolutePath = display_path(scannedPath),
            .enabled = true,
        });
    }
    return model;
}

AnalyzerBreadcrumbLayout layout_analyzer_breadcrumb(
    const AnalyzerBreadcrumbModel& model,
    const std::span<const float> measuredLabelWidths,
    const AnalyzerRectF bounds) {
    constexpr float segmentChromeWidth = 38.0F;
    constexpr float ellipsisWidth = 48.0F;
    constexpr float minimumSegmentWidth = 44.0F;

    AnalyzerBreadcrumbLayout layout;
    const auto itemCount = model.items.size();
    const auto availableWidth = std::max(0.0F, bounds.right - bounds.left);
    if (itemCount == 0U
        || measuredLabelWidths.size() < itemCount
        || availableWidth <= 0.0F
        || bounds.bottom <= bounds.top) {
        return layout;
    }

    std::vector<float> preferredWidths;
    preferredWidths.reserve(itemCount);
    for (std::size_t index = 0U; index < itemCount; ++index) {
        preferredWidths.push_back(
            std::max(0.0F, measuredLabelWidths[index]) + segmentChromeWidth);
    }
    const auto fullWidth = std::accumulate(
        preferredWidths.begin(),
        preferredWidths.end(),
        0.0F);
    if (fullWidth <= availableWidth) {
        layout.visible.reserve(itemCount);
        auto x = bounds.left;
        for (std::size_t index = 0U; index < itemCount; ++index) {
            const auto right = std::min(bounds.right, x + preferredWidths[index]);
            layout.visible.push_back({{x, bounds.top, right, bounds.bottom}, index});
            x = right;
        }
        return layout;
    }

    std::vector<std::size_t> retained;
    retained.reserve(4U);
    retained.push_back(0U);
    const auto scanRoot = std::find_if(
        model.items.begin(),
        model.items.end(),
        [](const BreadcrumbItem& item) {
            return item.kind == BreadcrumbItemKind::ScanNode;
        });
    if (scanRoot != model.items.end()) {
        retained.push_back(static_cast<std::size_t>(scanRoot - model.items.begin()));
    }
    if (itemCount >= 2U) {
        retained.push_back(itemCount - 2U);
    }
    retained.push_back(itemCount - 1U);
    std::ranges::sort(retained);
    const auto duplicate = std::ranges::unique(retained);
    retained.erase(duplicate.begin(), duplicate.end());

    layout.hiddenItemIndices.reserve(itemCount - retained.size());
    for (std::size_t index = 0U; index < itemCount; ++index) {
        if (!std::ranges::binary_search(retained, index)) {
            layout.hiddenItemIndices.push_back(index);
        }
    }

    std::vector<float> retainedWidths;
    retainedWidths.reserve(retained.size());
    for (const auto index : retained) {
        retainedWidths.push_back(preferredWidths[index]);
    }

    const auto hasEllipsis = !layout.hiddenItemIndices.empty();
    auto compactWidth = std::accumulate(retainedWidths.begin(), retainedWidths.end(), 0.0F)
        + (hasEllipsis ? ellipsisWidth : 0.0F);
    auto reduce = [&](const std::size_t itemIndex) {
        const auto found = std::ranges::find(retained, itemIndex);
        if (found == retained.end() || compactWidth <= availableWidth) {
            return;
        }
        const auto position = static_cast<std::size_t>(found - retained.begin());
        const auto reduction = std::min(
            retainedWidths[position] - minimumSegmentWidth,
            compactWidth - availableWidth);
        if (reduction > 0.0F) {
            retainedWidths[position] -= reduction;
            compactWidth -= reduction;
        }
    };
    if (itemCount >= 2U) {
        reduce(itemCount - 2U);
    }
    for (const auto index : retained) {
        reduce(index);
    }
    if (compactWidth > availableWidth && !retainedWidths.empty()) {
        const auto segmentBudget = std::max(
            0.0F,
            availableWidth - (hasEllipsis ? ellipsisWidth : 0.0F));
        const auto equalWidth = segmentBudget / static_cast<float>(retainedWidths.size());
        std::ranges::fill(retainedWidths, equalWidth);
    }

    std::size_t ellipsisPosition = retained.size();
    if (hasEllipsis) {
        if (scanRoot != model.items.end()) {
            const auto scanRootIndex = static_cast<std::size_t>(scanRoot - model.items.begin());
            const auto rootPosition = static_cast<std::size_t>(
                std::ranges::find(retained, scanRootIndex) - retained.begin());
            ellipsisPosition = rootPosition + (rootPosition + 1U < retained.size() ? 1U : 0U);
        } else {
            ellipsisPosition = std::min<std::size_t>(1U, retained.size());
        }
    }

    layout.visible.reserve(retained.size());
    auto x = bounds.left;
    for (std::size_t position = 0U; position <= retained.size(); ++position) {
        if (hasEllipsis && position == ellipsisPosition) {
            const auto right = std::min(bounds.right, x + ellipsisWidth);
            layout.ellipsis = BreadcrumbSegmentLayout{
                {x, bounds.top, right, bounds.bottom},
                std::numeric_limits<std::size_t>::max(),
            };
            x = right;
        }
        if (position == retained.size()) {
            break;
        }
        const auto right = std::min(bounds.right, x + retainedWidths[position]);
        layout.visible.push_back({
            {x, bounds.top, right, bounds.bottom},
            retained[position],
        });
        x = right;
    }
    return layout;
}

std::optional<BreadcrumbHit> hit_test_analyzer_breadcrumb(
    const AnalyzerBreadcrumbLayout& layout,
    const float xDip,
    const float yDip) noexcept {
    for (const auto& segment : layout.visible) {
        if (contains(segment.bounds, xDip, yDip)) {
            return BreadcrumbHit{BreadcrumbHitKind::Item, segment.itemIndex};
        }
    }
    if (layout.ellipsis.has_value() && contains(layout.ellipsis->bounds, xDip, yDip)) {
        return BreadcrumbHit{BreadcrumbHitKind::Ellipsis, 0U};
    }
    return std::nullopt;
}

} // namespace diskbloom::app
