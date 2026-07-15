#pragma once

#include <Windows.h>

#include <span>
#include <string>

namespace diskbloom::platform::windows {

enum class RecycleResult {
    Succeeded,
    Cancelled,
    InvalidPath,
    NotFound,
    Failed,
};

[[nodiscard]] RecycleResult recycle_paths(
    std::span<const std::wstring> paths,
    HWND ownerWindow) noexcept;

} // namespace diskbloom::platform::windows
