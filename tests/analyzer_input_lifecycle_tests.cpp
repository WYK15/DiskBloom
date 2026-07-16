#include "test_support.h"

#include "app/analyzer_input_lifecycle.h"

using diskbloom::app::AnalyzerInputTransition;
using diskbloom::app::compute_analyzer_input_actions;

TEST_CASE(analyzer_input_pointer_down_requests_capture_only_for_pending_drag) {
    const auto pending = compute_analyzer_input_actions(
        AnalyzerInputTransition::PointerDown, true, false);
    CHECK(pending.beginCapture);
    CHECK(!pending.releaseCapture);
    CHECK(!pending.cancelDrag);
    CHECK(!pending.clearHover);
    CHECK(!pending.dispatchCommand);

    const auto idle = compute_analyzer_input_actions(
        AnalyzerInputTransition::PointerDown, false, false);
    CHECK(!idle.beginCapture);
}

TEST_CASE(analyzer_input_release_releases_capture_and_dispatches) {
    const auto captured = compute_analyzer_input_actions(
        AnalyzerInputTransition::PointerReleased, false, true);
    CHECK(!captured.beginCapture);
    CHECK(captured.releaseCapture);
    CHECK(!captured.cancelDrag);
    CHECK(!captured.clearHover);
    CHECK(captured.dispatchCommand);
}

TEST_CASE(analyzer_input_leave_and_capture_loss_cancel_drag) {
    const auto leave = compute_analyzer_input_actions(
        AnalyzerInputTransition::PointerLeft, false, true);
    CHECK(leave.releaseCapture);
    CHECK(leave.cancelDrag);
    CHECK(leave.clearHover);
    CHECK(!leave.dispatchCommand);

    const auto captureLoss = compute_analyzer_input_actions(
        AnalyzerInputTransition::CaptureLost, false, false);
    CHECK(!captureLoss.releaseCapture);
    CHECK(captureLoss.cancelDrag);
    CHECK(!captureLoss.clearHover);
}

TEST_CASE(analyzer_input_content_change_cancels_before_releasing_capture) {
    const auto transition = compute_analyzer_input_actions(
        AnalyzerInputTransition::ContentChanging, false, true);
    CHECK(!transition.beginCapture);
    CHECK(transition.releaseCapture);
    CHECK(transition.cancelDrag);
    CHECK(!transition.clearHover);
    CHECK(!transition.dispatchCommand);
}
