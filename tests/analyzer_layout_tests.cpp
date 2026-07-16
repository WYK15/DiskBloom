#include "test_support.h"

#include "app/analyzer_view.h"
#include "app/review_collector_interaction.h"

#include <algorithm>
#include <array>

using diskbloom::app::AnalyzerHitTarget;
using diskbloom::app::AnalyzerCommandKind;
using diskbloom::app::AnalyzerBreadcrumbLayout;
using diskbloom::app::BreadcrumbSegmentLayout;
using diskbloom::app::AnalyzerRectF;
using diskbloom::app::compute_analyzer_layout;
using diskbloom::app::compute_review_collector_layout;
using diskbloom::app::hit_test_analyzer_layout;
using diskbloom::app::is_analyzer_header_interactive_point;

namespace {

bool inside(const AnalyzerRectF& inner, const AnalyzerRectF& outer) {
    return inner.left >= outer.left
        && inner.top >= outer.top
        && inner.right <= outer.right
        && inner.bottom <= outer.bottom;
}

bool overlaps(const AnalyzerRectF& left, const AnalyzerRectF& right) {
    return left.left < right.right && left.right > right.left
        && left.top < right.bottom && left.bottom > right.top;
}

} // namespace

TEST_CASE(analyzer_layout_keeps_chart_and_details_inside_supported_viewports) {
    constexpr std::array viewports{
        std::array{800.0F, 600.0F},
        std::array{1200.0F, 720.0F},
    };
    for (const auto& viewport : viewports) {
        const auto layout = compute_analyzer_layout(viewport[0], viewport[1], 6U);
        const AnalyzerRectF window{0.0F, 0.0F, viewport[0], viewport[1]};
        CHECK(inside(layout.header, window));
        CHECK(inside(layout.backButton, layout.header));
        CHECK(inside(layout.forwardButton, layout.header));
        CHECK(inside(layout.breadcrumbBounds, layout.header));
        CHECK(inside(layout.minimizeButton, layout.header));
        CHECK(inside(layout.maximizeButton, layout.header));
        CHECK(inside(layout.closeButton, layout.header));
        CHECK(inside(layout.chartBounds, window));
        CHECK(inside(layout.detailsBounds, window));
        CHECK(inside(layout.actionBar, window));
        CHECK(inside(layout.reviewButton, layout.actionBar));
        CHECK(inside(layout.addReviewButton, layout.actionBar));
        CHECK(inside(layout.previewButton, layout.actionBar));
        CHECK(inside(layout.revealButton, layout.actionBar));
        CHECK(!overlaps(layout.chartBounds, layout.detailsBounds));
        CHECK(!overlaps(layout.reviewButton, layout.addReviewButton));
        CHECK(!overlaps(layout.addReviewButton, layout.previewButton));
        CHECK(!overlaps(layout.previewButton, layout.revealButton));
        CHECK(!overlaps(layout.backButton, layout.forwardButton));
        CHECK(!overlaps(layout.forwardButton, layout.breadcrumbBounds));
        CHECK(!overlaps(layout.breadcrumbBounds, layout.minimizeButton));
        auto collector = compute_review_collector_layout(
            layout.actionBar,
            viewport[0],
            viewport[1],
            4U,
            0U);
        collector.summary.right = std::max(
            collector.summary.left,
            std::min(collector.summary.right, layout.reviewButton.left - 20.0F));
        CHECK(inside(collector.summary, layout.actionBar));
        CHECK(!overlaps(collector.summary, layout.reviewButton));
        CHECK(layout.chartGeometry.innerRadius > 0.0F);
        CHECK(layout.chartGeometry.ringWidth > 0.0F);
    }
}

TEST_CASE(analyzer_layout_chart_geometry_matches_chart_bounds) {
    const auto layout = compute_analyzer_layout(1200.0F, 720.0F, 4U);
    const auto radius = (layout.chartBounds.right - layout.chartBounds.left) * 0.5F;
    CHECK(layout.chartGeometry.centerX == (layout.chartBounds.left + layout.chartBounds.right) * 0.5F);
    CHECK(layout.chartGeometry.centerY == (layout.chartBounds.top + layout.chartBounds.bottom) * 0.5F);
    CHECK(std::abs(
              layout.chartGeometry.innerRadius
              + layout.chartGeometry.ringWidth * 4.0F
              - radius)
        < 0.001F);
}

TEST_CASE(analyzer_layout_scales_chart_radius_down_by_nine_percent) {
    const auto layout = compute_analyzer_layout(1200.0F, 720.0F, 6U);
    const auto radius = (layout.chartBounds.right - layout.chartBounds.left) * 0.5F;
    constexpr float unscaledResponsiveRadius = 288.0F;

    CHECK(std::abs(radius - unscaledResponsiveRadius * 0.91F) < 0.001F);
    CHECK(layout.chartGeometry.centerX
        == (layout.chartBounds.left + layout.chartBounds.right) * 0.5F);
    CHECK(AnalyzerCommandKind::RestoreReviewItem != AnalyzerCommandKind::ConfirmReview);
}

TEST_CASE(analyzer_hit_test_distinguishes_back_chart_and_empty_space) {
    const auto layout = compute_analyzer_layout(1200.0F, 720.0F, 6U);

    CHECK(hit_test_analyzer_layout(
              layout,
              (layout.backButton.left + layout.backButton.right) * 0.5F,
              (layout.backButton.top + layout.backButton.bottom) * 0.5F)
        == AnalyzerHitTarget::Back);
    CHECK(hit_test_analyzer_layout(
              layout,
              layout.chartGeometry.centerX + layout.chartGeometry.innerRadius + 4.0F,
              layout.chartGeometry.centerY)
        == AnalyzerHitTarget::Chart);
    CHECK(hit_test_analyzer_layout(
              layout,
              layout.detailsBounds.left + 4.0F,
              layout.detailsBounds.top + 4.0F)
        == AnalyzerHitTarget::None);
    CHECK(hit_test_analyzer_layout(
              layout,
              (layout.closeButton.left + layout.closeButton.right) * 0.5F,
              (layout.closeButton.top + layout.closeButton.bottom) * 0.5F)
        == AnalyzerHitTarget::CloseWindow);
    CHECK(hit_test_analyzer_layout(
              layout,
              layout.previewButton.left + 4.0F,
              layout.previewButton.top + 4.0F)
        == AnalyzerHitTarget::Preview);
    CHECK(hit_test_analyzer_layout(
              layout,
              layout.revealButton.left + 4.0F,
              layout.revealButton.top + 4.0F)
        == AnalyzerHitTarget::Reveal);
    CHECK(hit_test_analyzer_layout(
              layout,
              layout.addReviewButton.left + 4.0F,
              layout.addReviewButton.top + 4.0F)
        == AnalyzerHitTarget::AddToReview);
    CHECK(hit_test_analyzer_layout(
              layout,
              layout.reviewButton.left + 4.0F,
              layout.reviewButton.top + 4.0F)
        == AnalyzerHitTarget::ReviewDelete);
}

TEST_CASE(analyzer_header_hit_test_keeps_controls_client_and_exposes_gaps) {
    const auto layout = compute_analyzer_layout(1200.0F, 720.0F, 6U);
    AnalyzerBreadcrumbLayout breadcrumb;
    breadcrumb.visible = {
        BreadcrumbSegmentLayout{{120.0F, 8.0F, 210.0F, 56.0F}, 0U},
        BreadcrumbSegmentLayout{{260.0F, 8.0F, 350.0F, 56.0F}, 2U},
    };
    breadcrumb.ellipsis = BreadcrumbSegmentLayout{{220.0F, 8.0F, 250.0F, 56.0F}, 1U};

    const auto center = [](const AnalyzerRectF& bounds) {
        return std::array{
            (bounds.left + bounds.right) * 0.5F,
            (bounds.top + bounds.bottom) * 0.5F};
    };
    for (const auto& bounds : {
             layout.backButton,
             layout.forwardButton,
             layout.minimizeButton,
             layout.maximizeButton,
             layout.closeButton,
             breadcrumb.visible[0].bounds,
             breadcrumb.visible[1].bounds,
             breadcrumb.ellipsis->bounds}) {
        const auto point = center(bounds);
        CHECK(is_analyzer_header_interactive_point(
            layout, breadcrumb, point[0], point[1]));
    }

    CHECK(!is_analyzer_header_interactive_point(layout, breadcrumb, 215.0F, 32.0F));
    CHECK(!is_analyzer_header_interactive_point(layout, breadcrumb, 255.0F, 32.0F));
    CHECK(!is_analyzer_header_interactive_point(layout, breadcrumb, 500.0F, 32.0F));
    CHECK(!is_analyzer_header_interactive_point(layout, breadcrumb, 120.0F, 56.0F));
}
