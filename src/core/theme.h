#pragma once

#include <array>

namespace diskbloom::core {

enum class ThemeMode {
    System,
    Light,
    Dark,
};

struct Rgba {
    float r;
    float g;
    float b;
    float a;
};

struct ThemeTokens {
    Rgba window;
    Rgba titleBar;
    Rgba row;
    Rgba alternateRow;
    Rgba primaryText;
    Rgba secondaryText;
    Rgba track;
    Rgba button;
    Rgba buttonHover;
    Rgba border;
    Rgba accent;
    Rgba danger;
    Rgba focus;
    std::array<Rgba, 12> chartPalette;
};

[[nodiscard]] ThemeTokens make_theme(bool dark) noexcept;
[[nodiscard]] float contrast_ratio(Rgba first, Rgba second) noexcept;

} // namespace diskbloom::core
