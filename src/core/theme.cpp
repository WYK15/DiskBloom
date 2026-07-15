#include "core/theme.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace diskbloom::core {
namespace {

constexpr Rgba rgb(const std::uint32_t value) noexcept {
    constexpr float channel_scale = 1.0F / 255.0F;
    return {
        static_cast<float>((value >> 16U) & 0xFFU) * channel_scale,
        static_cast<float>((value >> 8U) & 0xFFU) * channel_scale,
        static_cast<float>(value & 0xFFU) * channel_scale,
        1.0F,
    };
}

float linear_channel(const float value) noexcept {
    return value <= 0.04045F
        ? value / 12.92F
        : std::pow((value + 0.055F) / 1.055F, 2.4F);
}

float luminance(const Rgba color) noexcept {
    return 0.2126F * linear_channel(color.r)
        + 0.7152F * linear_channel(color.g)
        + 0.0722F * linear_channel(color.b);
}

constexpr std::array<Rgba, 12> light_chart_palette{
    rgb(0x3D7FD6), rgb(0x17A673), rgb(0xD99A28), rgb(0xD65745),
    rgb(0x7A62C7), rgb(0x1B9BB3), rgb(0xBD5B8A), rgb(0x78962E),
    rgb(0x4E6EB1), rgb(0xC2742F), rgb(0x5B8F72), rgb(0x9A5FB0),
};

constexpr std::array<Rgba, 12> dark_chart_palette{
    rgb(0x61A0FF), rgb(0x3DD69A), rgb(0xF2BC4D), rgb(0xFF7462),
    rgb(0xA88AF2), rgb(0x45C4DC), rgb(0xE47BAE), rgb(0xA7C857),
    rgb(0x7899E8), rgb(0xE99A50), rgb(0x78B797), rgb(0xC081D3),
};

} // namespace

ThemeTokens make_theme(const bool dark) noexcept {
    if (dark) {
        return {
            rgb(0x1E2126),
            rgb(0x292D33),
            rgb(0x24282E),
            rgb(0x2A2F36),
            rgb(0xF5F7FA),
            rgb(0xAAB2BE),
            rgb(0x16191D),
            rgb(0x3A414B),
            rgb(0x48515D),
            rgb(0x4C5561),
            rgb(0x61A0FF),
            rgb(0xFF7462),
            rgb(0x79B8FF),
            dark_chart_palette,
        };
    }

    return {
        rgb(0xF1F3F6),
        rgb(0xFFFFFF),
        rgb(0xFFFFFF),
        rgb(0xF7F8FA),
        rgb(0x1A1D22),
        rgb(0x5D6570),
        rgb(0xDCE0E6),
        rgb(0xE1E6EC),
        rgb(0xD3DAE3),
        rgb(0xC8CDD5),
        rgb(0x276CC5),
        rgb(0xC53D3D),
        rgb(0x0F6CBD),
        light_chart_palette,
    };
}

float contrast_ratio(const Rgba first, const Rgba second) noexcept {
    const auto first_luminance = luminance(first);
    const auto second_luminance = luminance(second);
    const auto lighter = std::max(first_luminance, second_luminance);
    const auto darker = std::min(first_luminance, second_luminance);
    return (lighter + 0.05F) / (darker + 0.05F);
}

} // namespace diskbloom::core
