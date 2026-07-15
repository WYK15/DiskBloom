#pragma once

#include <Windows.h>

#include <string>

namespace diskbloom::platform::windows {

enum class FolderPickStatus {
    Selected,
    Cancelled,
    Failed,
};

struct FolderPickResult {
    FolderPickStatus status = FolderPickStatus::Failed;
    std::wstring path;
};

[[nodiscard]] FolderPickResult pick_folder(HWND ownerWindow) noexcept;

} // namespace diskbloom::platform::windows
