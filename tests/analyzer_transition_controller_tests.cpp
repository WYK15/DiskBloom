#include "test_support.h"

#include "app/analyzer_transition_controller.h"

#include <chrono>

using diskbloom::app::AnalyzerTransitionController;

namespace {

[[nodiscard]] bool near(const float left, const float right) {
    const auto difference = left > right ? left - right : right - left;
    return difference < 0.0001F;
}

} // namespace

TEST_CASE(analyzer_transition_completes_at_700_milliseconds) {
    AnalyzerTransitionController controller;
    const auto start = std::chrono::steady_clock::time_point{};
    controller.start(start, true);
    CHECK(controller.active());
    const auto middle = controller.advance(start + std::chrono::milliseconds{350});
    CHECK(middle.active);
    CHECK(near(middle.linearProgress, 0.5F));
    CHECK(near(middle.easedProgress, 0.875F));
    const auto end = controller.advance(start + std::chrono::milliseconds{700});
    CHECK(!end.active);
    CHECK(end.linearProgress == 1.0F);
    CHECK(!controller.active());
}

TEST_CASE(analyzer_transition_reduced_motion_completes_immediately) {
    AnalyzerTransitionController controller;
    const auto start = std::chrono::steady_clock::time_point{};
    controller.start(start, false);
    CHECK(!controller.active());
    const auto state = controller.advance(start);
    CHECK(!state.active);
    CHECK(state.linearProgress == 1.0F);
    CHECK(state.easedProgress == 1.0F);
}

TEST_CASE(analyzer_transition_restart_uses_new_monotonic_origin) {
    AnalyzerTransitionController controller;
    const auto start = std::chrono::steady_clock::time_point{};
    controller.start(start, true);
    (void)controller.advance(start + std::chrono::milliseconds{500});
    const auto restarted = start + std::chrono::seconds{2};
    controller.start(restarted, true);
    const auto state = controller.advance(restarted + std::chrono::milliseconds{70});
    CHECK(near(state.linearProgress, 0.1F));
}

TEST_CASE(analyzer_transition_cancel_is_idempotent) {
    AnalyzerTransitionController controller;
    controller.start(std::chrono::steady_clock::time_point{}, true);
    CHECK(controller.cancel());
    CHECK(!controller.cancel());
    CHECK(!controller.active());
}

TEST_CASE(analyzer_transition_uses_collector_duration_when_requested) {
    AnalyzerTransitionController controller;
    const auto start = std::chrono::steady_clock::time_point{};
    controller.start(start, true, AnalyzerTransitionController::collector_duration);
    CHECK(controller.advance(start + std::chrono::milliseconds{499}).active);
    CHECK(!controller.advance(start + std::chrono::milliseconds{500}).active);
}

