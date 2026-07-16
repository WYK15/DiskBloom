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
    static constexpr auto duration = std::chrono::milliseconds{700};

    void start(
        std::chrono::steady_clock::time_point now,
        bool animationsEnabled) noexcept;
    [[nodiscard]] AnalyzerTransitionAdvance advance(
        std::chrono::steady_clock::time_point now) noexcept;
    [[nodiscard]] bool cancel() noexcept;
    [[nodiscard]] bool active() const noexcept;

private:
    std::chrono::steady_clock::time_point start_{};
    bool active_ = false;
};

} // namespace diskbloom::app
