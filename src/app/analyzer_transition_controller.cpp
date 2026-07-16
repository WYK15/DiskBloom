#include "app/analyzer_transition_controller.h"

#include "core/sunburst_transition.h"

#include <algorithm>

namespace diskbloom::app {

void AnalyzerTransitionController::start(
    const std::chrono::steady_clock::time_point now,
    const bool animationsEnabled) noexcept {
    start_ = now;
    active_ = animationsEnabled;
}

AnalyzerTransitionAdvance AnalyzerTransitionController::advance(
    const std::chrono::steady_clock::time_point now) noexcept {
    if (!active_) {
        return {};
    }
    const auto elapsed = std::max(now - start_, std::chrono::steady_clock::duration::zero());
    const auto linear = std::clamp(
        std::chrono::duration<float>(elapsed).count()
            / std::chrono::duration<float>(duration).count(),
        0.0F,
        1.0F);
    if (linear >= 1.0F) {
        active_ = false;
    }
    return {
        .linearProgress = linear,
        .easedProgress = core::sunburst_transition_easing(linear),
        .active = active_,
    };
}

bool AnalyzerTransitionController::cancel() noexcept {
    return std::exchange(active_, false);
}

bool AnalyzerTransitionController::active() const noexcept {
    return active_;
}

} // namespace diskbloom::app
