#pragma once

#include "app/appearance_settings.h"

#include <filesystem>
#include <optional>

namespace diskbloom::platform::windows {

[[nodiscard]] std::filesystem::path default_settings_path();
[[nodiscard]] std::filesystem::path legacy_settings_path();

[[nodiscard]] std::optional<app::AppearanceSettings> load_settings(
    const std::filesystem::path& path,
    const app::AppearanceSettings& defaults) noexcept;

[[nodiscard]] std::optional<app::DirectoryTransitionMode>
load_legacy_directory_transition_mode(const std::filesystem::path& path) noexcept;

[[nodiscard]] bool save_settings_atomic(
    const std::filesystem::path& path,
    const app::AppearanceSettings& settings) noexcept;

[[nodiscard]] std::optional<app::DirectoryTransitionMode>
load_directory_transition_mode(const std::filesystem::path& path) noexcept;

[[nodiscard]] bool save_directory_transition_mode_atomic(
    const std::filesystem::path& path,
    app::DirectoryTransitionMode mode) noexcept;

} // namespace diskbloom::platform::windows
