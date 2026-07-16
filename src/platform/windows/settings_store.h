#pragma once

#include "app/appearance_settings.h"

#include <filesystem>
#include <optional>

namespace diskbloom::platform::windows {

[[nodiscard]] std::filesystem::path default_settings_path();

[[nodiscard]] std::optional<app::DirectoryTransitionMode>
load_directory_transition_mode(const std::filesystem::path& path) noexcept;

[[nodiscard]] bool save_directory_transition_mode_atomic(
    const std::filesystem::path& path,
    app::DirectoryTransitionMode mode) noexcept;

} // namespace diskbloom::platform::windows
