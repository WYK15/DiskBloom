#pragma once

#include "core/sunburst_layout.h"

#include <chrono>
#include <optional>

namespace diskbloom::app {

struct AnalyzerHoverBranch {
    float startAngle = 0.0F;
    float endAngle = 0.0F;
    core::NodeIndex node = core::invalid_node;
};

[[nodiscard]] std::optional<AnalyzerHoverBranch> find_hover_branch(
    const core::SunburstLayout& layout,
    core::NodeIndex node) noexcept;

[[nodiscard]] bool segment_is_in_hover_branch(
    const core::SunburstSegment& segment,
    const AnalyzerHoverBranch& branch) noexcept;

class AnalyzerHoverPulse final {
public:
    static constexpr auto period = std::chrono::milliseconds(900);
    static constexpr float minimum_opacity = 0.10F;
    static constexpr float maximum_opacity = 0.32F;

    void set_target(
        core::NodeIndex node,
        std::chrono::steady_clock::time_point now) noexcept;
    void clear() noexcept;
    [[nodiscard]] bool has_target() const noexcept;
    [[nodiscard]] core::NodeIndex target() const noexcept;
    [[nodiscard]] bool timer_required(bool animationsEnabled) const noexcept;
    [[nodiscard]] float opacity(
        std::chrono::steady_clock::time_point now,
        bool animationsEnabled) const noexcept;

private:
    core::NodeIndex target_ = core::invalid_node;
    std::chrono::steady_clock::time_point started_{};
};

} // namespace diskbloom::app
