#include "test_support.h"

#include "core/theme.h"
#include "platform/windows/system_theme.h"

#include <optional>

using diskbloom::core::Rgba;
using diskbloom::core::contrast_ratio;
using diskbloom::core::make_theme;
using diskbloom::platform::windows::is_dark_theme_registry_value;

namespace {

bool valid_color(const Rgba color) {
    return color.r >= 0.0F && color.r <= 1.0F
        && color.g >= 0.0F && color.g <= 1.0F
        && color.b >= 0.0F && color.b <= 1.0F
        && color.a >= 0.0F && color.a <= 1.0F;
}

} // namespace

TEST_CASE(theme_tokens_keep_primary_text_readable) {
    const auto light = make_theme(false);
    const auto dark = make_theme(true);

    CHECK(contrast_ratio(light.primaryText, light.window) >= 4.5F);
    CHECK(contrast_ratio(dark.primaryText, dark.window) >= 4.5F);
    CHECK(contrast_ratio(light.secondaryText, light.window) >= 3.0F);
    CHECK(contrast_ratio(dark.secondaryText, dark.window) >= 3.0F);
}

TEST_CASE(theme_tokens_are_complete_and_distinct) {
    const auto light = make_theme(false);
    const auto dark = make_theme(true);

    CHECK(light.window.r != dark.window.r);
    CHECK(light.chartPalette.size() == 12U);
    CHECK(dark.chartPalette.size() == 12U);
    CHECK(valid_color(light.accent));
    CHECK(valid_color(dark.accent));

    for (const auto color : light.chartPalette) {
        CHECK(valid_color(color));
    }
    for (const auto color : dark.chartPalette) {
        CHECK(valid_color(color));
    }
}

TEST_CASE(contrast_ratio_matches_black_and_white_reference) {
    constexpr Rgba black{0.0F, 0.0F, 0.0F, 1.0F};
    constexpr Rgba white{1.0F, 1.0F, 1.0F, 1.0F};
    const auto ratio = contrast_ratio(black, white);
    CHECK(ratio > 20.9F);
    CHECK(ratio < 21.1F);
}

TEST_CASE(system_theme_registry_value_has_safe_default) {
    CHECK(is_dark_theme_registry_value(0U));
    CHECK(!is_dark_theme_registry_value(1U));
    CHECK(!is_dark_theme_registry_value(2U));
    CHECK(!is_dark_theme_registry_value(std::nullopt));
}
