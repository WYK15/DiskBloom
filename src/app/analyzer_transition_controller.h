#pragma once

#include <chrono>

namespace diskbloom::app {

struct AnalyzerTransitionAdvance {
    float linearProgress = 1.0F;
    float easedProgress = 1.0F;
    bool active = false;
};

class AnalyzerTransitionController final {
public:
    static constexpr auto navigation_duration = std::chrono::milliseconds{700};
    static constexpr auto collector_duration = std::chrono::milliseconds{500};
    static constexpr auto duration = navigation_duration;

    void start(
        std::chrono::steady_clock::time_point now,
        bool animationsEnabled,
        std::chrono::milliseconds requestedDuration = navigation_duration) noexcept;
    [[nodiscard]] AnalyzerTransitionAdvance advance(
        std::chrono::steady_clock::time_point now) noexcept;
    [[nodiscard]] bool cancel() noexcept;
    [[nodiscard]] bool active() const noexcept;

private:
    std::chrono::steady_clock::time_point start_{};
    std::chrono::milliseconds duration_ = navigation_duration;
    bool active_ = false;
};

} // namespace diskbloom::app
