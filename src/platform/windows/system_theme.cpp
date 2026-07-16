#include "platform/windows/system_theme.h"

#include <Windows.h>

#include <cstdint>
#include <optional>

namespace diskbloom::platform::windows {

bool is_dark_theme_registry_value(
    const std::optional<std::uint32_t> apps_use_light_theme) noexcept {
    return apps_use_light_theme.has_value() && *apps_use_light_theme == 0U;
}

bool is_system_dark_theme() noexcept {
    constexpr auto key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
    constexpr auto value_name = L"AppsUseLightTheme";

    DWORD value = 1U;
    DWORD value_size = sizeof(value);
    const auto result = RegGetValueW(
        HKEY_CURRENT_USER,
        key,
        value_name,
        RRF_RT_REG_DWORD,
        nullptr,
        &value,
        &value_size);

    if (result != ERROR_SUCCESS) {
        return is_dark_theme_registry_value(std::nullopt);
    }

    return is_dark_theme_registry_value(static_cast<std::uint32_t>(value));
}

bool resolve_client_area_animations(
    const bool querySucceeded,
    const bool enabled) noexcept {
    return querySucceeded ? enabled : true;
}

bool client_area_animations_enabled() noexcept {
    BOOL enabled = TRUE;
    const auto succeeded = SystemParametersInfoW(
        SPI_GETCLIENTAREAANIMATION,
        0U,
        &enabled,
        0U) != FALSE;
    return resolve_client_area_animations(succeeded, enabled != FALSE);
}

} // namespace diskbloom::platform::windows
