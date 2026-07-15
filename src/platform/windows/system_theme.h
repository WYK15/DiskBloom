#pragma once

#include <cstdint>
#include <optional>

namespace diskbloom::platform::windows {

[[nodiscard]] bool is_dark_theme_registry_value(
    std::optional<std::uint32_t> apps_use_light_theme) noexcept;
[[nodiscard]] bool is_system_dark_theme() noexcept;

} // namespace diskbloom::platform::windows
