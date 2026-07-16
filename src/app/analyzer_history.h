#pragma once

#include "core/scan_tree.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace diskbloom::app {

enum class MainContentView {
    Overview,
    Analyzer,
};

struct NavigationEntry {
    MainContentView view = MainContentView::Overview;
    core::NodeIndex node = core::invalid_node;

    friend bool operator==(const NavigationEntry&, const NavigationEntry&) = default;
};

class AnalyzerHistory final {
public:
    static constexpr std::size_t capacity = 128U;

    void reset(NavigationEntry entry);
    [[nodiscard]] bool record(NavigationEntry entry);
    [[nodiscard]] std::optional<NavigationEntry> back() noexcept;
    [[nodiscard]] std::optional<NavigationEntry> forward() noexcept;
    [[nodiscard]] std::optional<NavigationEntry> current() const noexcept;
    [[nodiscard]] bool can_back() const noexcept;
    [[nodiscard]] bool can_forward() const noexcept;

private:
    std::vector<NavigationEntry> entries_;
    std::size_t cursor_ = 0U;
};

} // namespace diskbloom::app
