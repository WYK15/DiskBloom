#pragma once

#include <string_view>

namespace diskbloom::platform::windows {

enum class ShellActionResult {
    Succeeded,
    InvalidPath,
    NotFound,
    Failed,
};

[[nodiscard]] ShellActionResult open_with_shell(std::wstring_view path) noexcept;
[[nodiscard]] ShellActionResult reveal_in_explorer(std::wstring_view path) noexcept;

} // namespace diskbloom::platform::windows
