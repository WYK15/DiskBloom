# Typography And Chart Settings Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add immediately applied and persisted presets for global text size, font family, and analyzer chart size to the existing Settings menu.

**Architecture:** Extend the existing `AppearanceSettings` snapshot with compact preset enums and constant-time conversion helpers. Persist the complete snapshot in an atomically replaced v2 file with v1 transition migration, pass typography through the two DirectWrite views as a resource-cache key, and pass chart scale through analyzer layout geometry so drawing, animation, and hit testing remain aligned.

**Tech Stack:** C++20, Win32 popup menus and filesystem APIs, Direct2D, DirectWrite, CMake/Ninja, CTest, MSVC Debug/Release.

## Global Constraints

- Every new UI state must work in light and dark modes.
- Every visible label, including identical percentage and font-family labels, must come from both English and Simplified Chinese string catalogs.
- Preserve the existing DaisyDisk-like menu, analyzer layout, animation, and mathematical hit-testing behavior.
- Default text scale is `100%`, default font is `Segoe UI Variable`, and default chart scale is `80%`.
- Available text scales are exactly `80%`, `90%`, `100%`, `110%`, and `120%`.
- Available font presets are exactly `Segoe UI Variable`, `Microsoft YaHei UI`, `Arial`, and `Consolas`.
- Available chart scales are exactly `60%`, `70%`, `80%`, `90%`, and `100%`.
- Do not enumerate installed fonts or add per-frame allocation, parsing, filesystem work, or native controls.
- Settings changes are persisted before activation; a failed write keeps the prior active snapshot.
- The legacy `settings-v1.ini` file is read only when `settings-v2.ini` does not exist.

---

### Task 1: Define Typed Presets And Settings Commands

**Files:**
- Modify: `tests/appearance_settings_tests.cpp`
- Modify: `src/app/appearance_settings.h`
- Modify: `src/app/appearance_settings.cpp`

**Interfaces:**
- Consumes: existing `AppearanceSettings`, `SettingsCommand`, and `apply_settings_command`.
- Produces: `TextScalePreset`, `FontFamilyPreset`, `ChartScalePreset`, `TypographySettings`, `text_scale_factor`, `chart_scale_factor`, `body_font_family`, and `display_font_family`.

- [x] **Step 1: Add failing default and conversion tests**

Add the following tests before changing production code:

```cpp
TEST_CASE(appearance_settings_typography_and_chart_defaults_match_current_ui) {
    const AppearanceSettings settings{ThemeMode::System, Language::English};
    CHECK(settings.typography.textScale == TextScalePreset::Percent100);
    CHECK(settings.typography.fontFamily == FontFamilyPreset::SegoeUiVariable);
    CHECK(settings.chartScale == ChartScalePreset::Percent80);
    CHECK(std::abs(text_scale_factor(settings.typography.textScale) - 1.0F) < 0.001F);
    CHECK(std::abs(chart_scale_factor(settings.chartScale) - 0.80F) < 0.001F);
}

TEST_CASE(appearance_settings_maps_all_preset_values_without_string_parsing) {
    CHECK(std::abs(text_scale_factor(TextScalePreset::Percent80) - 0.80F) < 0.001F);
    CHECK(std::abs(text_scale_factor(TextScalePreset::Percent90) - 0.90F) < 0.001F);
    CHECK(std::abs(text_scale_factor(TextScalePreset::Percent100) - 1.00F) < 0.001F);
    CHECK(std::abs(text_scale_factor(TextScalePreset::Percent110) - 1.10F) < 0.001F);
    CHECK(std::abs(text_scale_factor(TextScalePreset::Percent120) - 1.20F) < 0.001F);
    CHECK(std::abs(chart_scale_factor(ChartScalePreset::Percent60) - 0.60F) < 0.001F);
    CHECK(std::abs(chart_scale_factor(ChartScalePreset::Percent70) - 0.70F) < 0.001F);
    CHECK(std::abs(chart_scale_factor(ChartScalePreset::Percent80) - 0.80F) < 0.001F);
    CHECK(std::abs(chart_scale_factor(ChartScalePreset::Percent90) - 0.90F) < 0.001F);
    CHECK(std::abs(chart_scale_factor(ChartScalePreset::Percent100) - 1.00F) < 0.001F);
    CHECK(body_font_family(FontFamilyPreset::SegoeUiVariable)
        == L"Segoe UI Variable Text");
    CHECK(display_font_family(FontFamilyPreset::SegoeUiVariable)
        == L"Segoe UI Variable Display");
    CHECK(body_font_family(FontFamilyPreset::MicrosoftYaHeiUi)
        == L"Microsoft YaHei UI");
    CHECK(body_font_family(FontFamilyPreset::Arial) == L"Arial");
    CHECK(display_font_family(FontFamilyPreset::Arial) == L"Arial");
    CHECK(body_font_family(FontFamilyPreset::Consolas) == L"Consolas");
    CHECK(display_font_family(FontFamilyPreset::Consolas) == L"Consolas");
}
```

Add `#include <cmath>` and `using` declarations for each new type/helper.

- [x] **Step 2: Run the unit executable and verify RED**

```powershell
cmake --build --preset windows-debug --target diskbloom_tests
build/windows-debug/tests/diskbloom_tests.exe
```

Expected: compilation fails because the preset types and helpers do not exist.

- [x] **Step 3: Add the preset model and constant-time helpers**

Add these declarations to `appearance_settings.h`:

```cpp
enum class TextScalePreset : std::uint8_t {
    Percent80,
    Percent90,
    Percent100,
    Percent110,
    Percent120,
};

enum class FontFamilyPreset : std::uint8_t {
    SegoeUiVariable,
    MicrosoftYaHeiUi,
    Arial,
    Consolas,
};

enum class ChartScalePreset : std::uint8_t {
    Percent60,
    Percent70,
    Percent80,
    Percent90,
    Percent100,
};

struct TypographySettings {
    TextScalePreset textScale = TextScalePreset::Percent100;
    FontFamilyPreset fontFamily = FontFamilyPreset::SegoeUiVariable;
    bool operator==(const TypographySettings&) const noexcept = default;
};

[[nodiscard]] float text_scale_factor(TextScalePreset preset) noexcept;
[[nodiscard]] float chart_scale_factor(ChartScalePreset preset) noexcept;
[[nodiscard]] std::wstring_view body_font_family(FontFamilyPreset preset) noexcept;
[[nodiscard]] std::wstring_view display_font_family(FontFamilyPreset preset) noexcept;
```

Extend `AppearanceSettings`:

```cpp
struct AppearanceSettings {
    core::ThemeMode themeMode;
    core::Language language;
    DirectoryTransitionMode directoryTransitions = DirectoryTransitionMode::AlwaysOn;
    TypographySettings typography{};
    ChartScalePreset chartScale = ChartScalePreset::Percent80;
    bool operator==(const AppearanceSettings&) const noexcept = default;
};
```

Implement each helper with a complete `switch`. Invalid enum values return the documented defaults (`1.0F`, `0.80F`, and the Segoe Variable families). Do not use maps, allocations, or runtime string parsing.

- [x] **Step 4: Add failing command-isolation tests**

Extend `SettingsCommand` with these exact values:

```cpp
TextScale80 = 1031,
TextScale90 = 1032,
TextScale100 = 1033,
TextScale110 = 1034,
TextScale120 = 1035,
FontSegoeUiVariable = 1041,
FontMicrosoftYaHeiUi = 1042,
FontArial = 1043,
FontConsolas = 1044,
ChartScale60 = 1051,
ChartScale70 = 1052,
ChartScale80 = 1053,
ChartScale90 = 1054,
ChartScale100 = 1055,
```

Then add this test before implementing the new switch arms:

```cpp
TEST_CASE(appearance_settings_applies_each_new_preset_without_touching_other_fields) {
    AppearanceSettings settings{ThemeMode::Dark, Language::SimplifiedChinese};
    settings.directoryTransitions = DirectoryTransitionMode::FollowSystem;

    CHECK(apply_settings_command(settings, SettingsCommand::TextScale80));
    CHECK(settings.typography.textScale == TextScalePreset::Percent80);
    CHECK(settings.themeMode == ThemeMode::Dark);
    CHECK(settings.language == Language::SimplifiedChinese);
    CHECK(settings.directoryTransitions == DirectoryTransitionMode::FollowSystem);

    CHECK(apply_settings_command(settings, SettingsCommand::FontConsolas));
    CHECK(settings.typography.fontFamily == FontFamilyPreset::Consolas);
    CHECK(settings.chartScale == ChartScalePreset::Percent80);

    CHECK(apply_settings_command(settings, SettingsCommand::ChartScale100));
    CHECK(settings.chartScale == ChartScalePreset::Percent100);
    CHECK(settings.typography.textScale == TextScalePreset::Percent80);
    CHECK(settings.typography.fontFamily == FontFamilyPreset::Consolas);

    constexpr std::array textCases{
        std::pair{SettingsCommand::TextScale80, TextScalePreset::Percent80},
        std::pair{SettingsCommand::TextScale90, TextScalePreset::Percent90},
        std::pair{SettingsCommand::TextScale100, TextScalePreset::Percent100},
        std::pair{SettingsCommand::TextScale110, TextScalePreset::Percent110},
        std::pair{SettingsCommand::TextScale120, TextScalePreset::Percent120},
    };
    for (const auto [command, expected] : textCases) {
        CHECK(apply_settings_command(settings, command));
        CHECK(settings.typography.textScale == expected);
    }

    constexpr std::array fontCases{
        std::pair{SettingsCommand::FontSegoeUiVariable, FontFamilyPreset::SegoeUiVariable},
        std::pair{SettingsCommand::FontMicrosoftYaHeiUi, FontFamilyPreset::MicrosoftYaHeiUi},
        std::pair{SettingsCommand::FontArial, FontFamilyPreset::Arial},
        std::pair{SettingsCommand::FontConsolas, FontFamilyPreset::Consolas},
    };
    for (const auto [command, expected] : fontCases) {
        CHECK(apply_settings_command(settings, command));
        CHECK(settings.typography.fontFamily == expected);
    }

    constexpr std::array chartCases{
        std::pair{SettingsCommand::ChartScale60, ChartScalePreset::Percent60},
        std::pair{SettingsCommand::ChartScale70, ChartScalePreset::Percent70},
        std::pair{SettingsCommand::ChartScale80, ChartScalePreset::Percent80},
        std::pair{SettingsCommand::ChartScale90, ChartScalePreset::Percent90},
        std::pair{SettingsCommand::ChartScale100, ChartScalePreset::Percent100},
    };
    for (const auto [command, expected] : chartCases) {
        CHECK(apply_settings_command(settings, command));
        CHECK(settings.chartScale == expected);
    }
}
```

Add `<array>` and `<utility>` to the test includes.

- [x] **Step 5: Verify RED, implement every command, and verify GREEN**

Run `build/windows-debug/tests/diskbloom_tests.exe`; expect the first new command assertion to fail. Add one explicit `switch` arm per preset to `apply_settings_command`, then run:

```powershell
cmake --build --preset windows-debug --target diskbloom_tests
ctest --preset windows-debug -R '^diskbloom_tests$' --output-on-failure
```

Expected: `diskbloom_tests` passes, including unknown-command rejection and all prior theme/language/animation behavior.

- [x] **Step 6: Commit the typed settings domain**

```powershell
git add src/app/appearance_settings.h src/app/appearance_settings.cpp tests/appearance_settings_tests.cpp
git commit -m "feat: add typography and chart presets"
```

---

### Task 2: Localize Every New Menu Label

**Files:**
- Modify: `src/core/language.h`
- Modify: `src/core/string_catalog.cpp`
- Modify: `tests/string_catalog_tests.cpp`

**Interfaces:**
- Consumes: `core::StringId` and `core::get_string`.
- Produces: localized IDs for submenu headings, percentages, and font names.

- [x] **Step 1: Add failing catalog assertions**

Add IDs named `TextSize`, `Font`, `ChartSize`, `Percent60`, `Percent70`, `Percent80`, `Percent90`, `Percent100`, `Percent110`, `Percent120`, `FontSegoeUiVariable`, `FontMicrosoftYaHeiUi`, `FontArial`, and `FontConsolas`. Before adding catalog entries, assert representative values:

```cpp
CHECK(get_string(Language::English, StringId::TextSize) == L"Text size");
CHECK(get_string(Language::SimplifiedChinese, StringId::TextSize)
    == L"\u6587\u5b57\u5927\u5c0f");
CHECK(get_string(Language::English, StringId::ChartSize) == L"Chart size");
CHECK(get_string(Language::SimplifiedChinese, StringId::ChartSize)
    == L"\u5706\u56fe\u5927\u5c0f");
CHECK(get_string(Language::English, StringId::Percent80) == L"80%");
CHECK(get_string(Language::SimplifiedChinese, StringId::Percent80) == L"80%");
CHECK(get_string(Language::English, StringId::FontSegoeUiVariable)
    == L"Segoe UI Variable");
CHECK(get_string(Language::SimplifiedChinese, StringId::FontConsolas)
    == L"Consolas");
```

- [x] **Step 2: Run and verify RED**

```powershell
cmake --build --preset windows-debug --target diskbloom_tests
```

Expected: build or catalog-completeness failure because the new IDs have no entries.

- [x] **Step 3: Add both complete catalog sequences**

Append entries in the exact enum order. English headings are `Text size`, `Font`, `Chart size`; Chinese headings are `\u6587\u5b57\u5927\u5c0f`, `\u5b57\u4f53`, `\u5706\u56fe\u5927\u5c0f`. Percentage and registered family-name values are identical in both arrays.

Keep both `static_assert` checks and the all-IDs completeness test intact.

- [x] **Step 4: Verify and commit localization**

```powershell
cmake --build --preset windows-debug --target diskbloom_tests
ctest --preset windows-debug -R '^diskbloom_tests$' --output-on-failure
git add src/core/language.h src/core/string_catalog.cpp tests/string_catalog_tests.cpp
git commit -m "feat: localize appearance presets"
```

Expected: unit suite passes with no empty English or Chinese string.

---

### Task 3: Persist A Versioned Complete Settings Snapshot

**Files:**
- Modify: `src/platform/windows/settings_store.h`
- Modify: `src/platform/windows/settings_store.cpp`
- Modify: `tests/settings_store_tests.cpp`

**Interfaces:**
- Consumes: `AppearanceSettings` and all preset enums from Task 1.
- Produces: `default_settings_path`, `legacy_settings_path`, `load_settings`, `load_legacy_directory_transition_mode`, and `save_settings_atomic`.

- [x] **Step 1: Replace the old round-trip test with a failing v2 snapshot test**

Use a unique `settings-v2.ini` test path and add:

```cpp
TEST_CASE(settings_store_round_trips_complete_v2_snapshot) {
    const auto path = unique_settings_test_path();
    AppearanceSettings expected{ThemeMode::Dark, Language::SimplifiedChinese};
    expected.directoryTransitions = DirectoryTransitionMode::FollowSystem;
    expected.typography.textScale = TextScalePreset::Percent120;
    expected.typography.fontFamily = FontFamilyPreset::MicrosoftYaHeiUi;
    expected.chartScale = ChartScalePreset::Percent60;

    CHECK(save_settings_atomic(path, expected));
    const auto loaded = load_settings(path, AppearanceSettings{
        ThemeMode::System, Language::English});
    CHECK(loaded.has_value());
    CHECK(loaded == expected);
    cleanup_settings_test_path(path);
}
```

- [x] **Step 2: Run and verify RED**

```powershell
cmake --build --preset windows-debug --target diskbloom_tests
```

Expected: compilation fails because `load_settings` and `save_settings_atomic` are absent.

- [x] **Step 3: Introduce the v2 API and deterministic serializer**

Replace the public API with:

```cpp
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
```

Make the path helpers return `settings-v2.ini` and `settings-v1.ini` under the same `%LOCALAPPDATA%\DiskBloom` directory. Serialize exactly:

```text
DiskBloomSettings/2
theme=dark
language=zh-CN
directoryTransitions=system
textScale=120
fontFamily=microsoft-yahei-ui
chartScale=60
```

Use fixed token conversion switches and one bounded `std::string` reserved to 256 bytes. Retain `CreateFileW`, `FILE_FLAG_WRITE_THROUGH`, `FlushFileBuffers`, and `MoveFileExW(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`.

Recognize only these field tokens: `system/light/dark`, `en-US/zh-CN`,
`always/system/off`, `80/90/100/110/120`,
`segoe-variable/microsoft-yahei-ui/arial/consolas`, and
`60/70/80/90/100`, respectively.

- [x] **Step 4: Add failing validation, migration, and atomicity tests**

Add tests that write these exact fixtures:

```text
DiskBloomSettings/2
theme=purple
language=zh-CN
directoryTransitions=slow
textScale=135
fontFamily=unknown
chartScale=75
futureKey=preserved-by-future-version
```

Assert the valid language loads while every invalid known field uses the caller-provided default. Add an invalid-header fixture and assert `load_settings` returns `std::nullopt`. Rename the existing v1 tests to call `load_legacy_directory_transition_mode`, and retain the `.tmp` directory collision test with `save_settings_atomic`, asserting the previous complete snapshot remains readable.

- [x] **Step 5: Implement bounded line parsing and verify GREEN**

Read at most 2048 bytes. Require the first line to equal `DiskBloomSettings/2`. Scan the fixed input buffer with `std::string_view`, split each line at the first `=`, and do not allocate per line. For each known key, assign only an exact recognized token. Start from the supplied `defaults`, ignore unknown keys, and return the resulting snapshot. Keep the legacy loader's exact three-file recognition unchanged.

Run:

```powershell
cmake --build --preset windows-debug --target diskbloom_tests
ctest --preset windows-debug -R '^diskbloom_tests$' --output-on-failure
```

Expected: all v1, v2, malformed-input, and atomic-failure tests pass.

- [x] **Step 6: Commit the store upgrade**

```powershell
git add src/platform/windows/settings_store.h src/platform/windows/settings_store.cpp tests/settings_store_tests.cpp
git commit -m "feat: persist complete appearance settings"
```

---

### Task 4: Make Chart Scale Part Of Analyzer Geometry

**Files:**
- Modify: `tests/analyzer_layout_tests.cpp`
- Modify: `tests/analyzer_render_smoke_main.cpp`
- Modify: `src/app/analyzer_view.h`
- Modify: `src/app/analyzer_view.cpp`

**Interfaces:**
- Consumes: `chart_scale_factor(ChartScalePreset)`.
- Produces: `compute_analyzer_layout(widthDip, heightDip, depthCount, chartScale)` and analyzer drawing with a chart-scale input.

- [x] **Step 1: Add a failing all-presets geometry test**

Replace the fixed 80-percent assertion with:

```cpp
TEST_CASE(analyzer_layout_scales_chart_radius_for_every_supported_preset) {
    constexpr std::array scales{0.60F, 0.70F, 0.80F, 0.90F, 1.00F};
    constexpr float unscaledResponsiveRadius = 288.0F;
    for (const auto scale : scales) {
        const auto layout = compute_analyzer_layout(1200.0F, 720.0F, 6U, scale);
        const auto radius = (layout.chartBounds.right - layout.chartBounds.left) * 0.5F;
        CHECK(std::abs(radius - unscaledResponsiveRadius * scale) < 0.001F);
        CHECK(layout.chartGeometry.centerX
            == (layout.chartBounds.left + layout.chartBounds.right) * 0.5F);
    }
}
```

Update the existing containment and hit-test test inputs to pass `0.80F` explicitly.

- [x] **Step 2: Run and verify RED**

```powershell
cmake --build --preset windows-debug --target diskbloom_tests
```

Expected: compilation fails because the layout function has no scale parameter.

- [x] **Step 3: Thread scale through layout, transitions, and drawing**

Change the layout signature to:

```cpp
[[nodiscard]] AnalyzerLayout compute_analyzer_layout(
    float widthDip,
    float heightDip,
    std::size_t depthCount,
    float chartScale) noexcept;
```

Compute:

```cpp
const auto radius = responsiveRadius * std::clamp(chartScale, 0.60F, 1.00F);
```

Add `float chartScale_ = 0.80F;` to `AnalyzerView`. Extend `draw` with a `float chartScale` argument, assign the clamped value before computing layout, and pass `chartScale_` in both destination-layout calls used by navigation and review transitions. Pass `0.80F` from existing smoke-test calls until Task 5 supplies typography alongside it.

- [x] **Step 4: Verify geometry and render smoke**

```powershell
cmake --build --preset windows-debug --target diskbloom_tests diskbloom_analyzer_render_smoke
ctest --preset windows-debug -R 'diskbloom_tests|diskbloom_analyzer_render_smoke' --output-on-failure
```

Expected: both targets pass; existing animation target geometry and chart hit testing remain aligned.

- [x] **Step 5: Commit chart configurability**

```powershell
git add src/app/analyzer_view.h src/app/analyzer_view.cpp tests/analyzer_layout_tests.cpp tests/analyzer_render_smoke_main.cpp
git commit -m "feat: make analyzer chart scale configurable"
```

---

### Task 5: Apply Typography To Both DirectWrite Views

**Files:**
- Modify: `src/app/disk_overview.h`
- Modify: `src/app/disk_overview.cpp`
- Modify: `src/app/analyzer_view.h`
- Modify: `src/app/analyzer_view.cpp`
- Modify: `src/app/main_window.cpp`
- Modify: `tests/render_smoke_main.cpp`
- Modify: `tests/analyzer_render_smoke_main.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `TypographySettings`, `text_scale_factor`, `body_font_family`, and `display_font_family`.
- Produces: typography-aware, cached DirectWrite formats in both views.

- [x] **Step 1: Add failing view call sites for minimum and maximum typography**

In both smoke executables, define:

```cpp
const diskbloom::app::TypographySettings smallSegoe{
    .textScale = diskbloom::app::TextScalePreset::Percent80,
    .fontFamily = diskbloom::app::FontFamilyPreset::SegoeUiVariable,
};
const diskbloom::app::TypographySettings largeConsolas{
    .textScale = diskbloom::app::TextScalePreset::Percent120,
    .fontFamily = diskbloom::app::FontFamilyPreset::Consolas,
};
```

Call `DiskOverview::draw` and `AnalyzerView::draw` twice on the same view instance with these settings, in both theme/language loops. For analyzer calls also pass `0.60F` and `1.00F`. Update `diskbloom_render_smoke` to link `diskbloom_app` and `diskbloom_platform_windows` so it can instantiate an empty `DiskOverview`.

- [x] **Step 2: Build and verify RED**

```powershell
cmake --build --preset windows-debug --target diskbloom_render_smoke diskbloom_analyzer_render_smoke
```

Expected: compilation fails because view draw methods do not accept typography.

- [x] **Step 3: Add typography to resource cache keys**

Extend both view APIs:

```cpp
bool draw(
    render::GraphicsDevice& graphics,
    const core::ThemeTokens& theme,
    core::Language language,
    const TypographySettings& typography,
    float widthDip,
    float heightDip);
```

The analyzer signature additionally ends with `float chartScale`. Extend both `ensure_resources` methods identically. Add `TypographySettings typography{}` to each `Resources` struct and require `resources_->typography == typography` in the reuse check.

Create formats with:

```cpp
const auto scale = text_scale_factor(typography.textScale);
const auto bodyFamily = body_font_family(typography.fontFamily);
const auto displayFamily = display_font_family(typography.fontFamily);
```

Multiply every existing format size by `scale`. Use `displayFamily.data()` only for the overview title format and `bodyFamily.data()` for all other formats. Preserve all alignment, wrapping, trimming, and ellipsis configuration.

- [x] **Step 4: Pass active settings from MainWindow**

Change `render_frame` calls to:

```cpp
const auto rendered = navigation_.view == MainContentView::Analyzer
    ? analyzer_.draw(
          graphics_, theme, appearance_.language, appearance_.typography,
          width, height, chart_scale_factor(appearance_.chartScale))
    : overview_.draw(
          graphics_, theme, appearance_.language, appearance_.typography,
          width, height);
```

Update every smoke helper and direct draw call to the new exact parameter order.

- [x] **Step 5: Verify cache invalidation visually and automatically**

In analyzer smoke, render the same populated analyzer sequentially with all four font presets and both text extremes. Capture the BGRA frames for the four representative combinations when `--capture-dir` is present:

```text
typography-segoe-80-chart-60-light-en.png
typography-yahei-120-chart-100-dark-zh.png
typography-arial-80-chart-100-dark-en.png
typography-consolas-120-chart-60-light-zh.png
```

Add this deterministic contrast helper:

```cpp
[[nodiscard]] bool frame_has_contrast(
    const diskbloom::render::CapturedFrame& frame) noexcept {
    if (frame.pixels.size() < 8U) {
        return false;
    }
    for (std::size_t offset = 4U; offset + 3U < frame.pixels.size(); offset += 4U) {
        if (!std::equal(
                frame.pixels.begin(),
                frame.pixels.begin() + 4,
                frame.pixels.begin() + static_cast<std::ptrdiff_t>(offset))) {
            return true;
        }
    }
    return false;
}
```

Assert `frame_has_contrast` for each capture and assert the small-Segoe and
large-Consolas `pixels` vectors are not equal. Disable hover animations and
finish any directory transition before the comparison. Re-render one unchanged
combination and assert its `pixels` vector is identical, guarding deterministic
resource reuse.

Run:

```powershell
cmake --build --preset windows-debug --target diskbloom_render_smoke diskbloom_analyzer_render_smoke DiskBloom
ctest --preset windows-debug -R 'diskbloom_render_smoke|diskbloom_analyzer_render_smoke|diskbloom_app_smoke_' --output-on-failure
```

Expected: overview and analyzer render in light/dark and English/Chinese at both text extremes; all six smoke tests pass.

- [x] **Step 6: Commit typography rendering**

```powershell
git add src/app/disk_overview.h src/app/disk_overview.cpp src/app/analyzer_view.h src/app/analyzer_view.cpp src/app/main_window.cpp tests/render_smoke_main.cpp tests/analyzer_render_smoke_main.cpp tests/CMakeLists.txt
git commit -m "feat: apply typography presets to app views"
```

---

### Task 6: Integrate Startup Migration And Transactional Settings Menu

**Files:**
- Modify: `src/app/appearance_settings.h`
- Modify: `src/app/appearance_settings.cpp`
- Modify: `src/app/win_main.cpp`
- Modify: `src/app/main_window.cpp`
- Modify: `tests/appearance_settings_tests.cpp`
- Modify: `tests/settings_store_tests.cpp`

**Interfaces:**
- Consumes: all setting commands, v2 path/load/save APIs, legacy path/loader, and localized `StringId` labels.
- Produces: `make_settings_candidate`, checked preset submenus, save-before-activate behavior, and startup v1 migration.

- [x] **Step 1: Add a failing candidate-snapshot isolation test**

Document the transactional UI behavior through the pure settings model:

```cpp
TEST_CASE(appearance_settings_builds_only_changed_valid_candidates) {
    const AppearanceSettings active{ThemeMode::System, Language::English};
    const auto candidate = make_settings_candidate(
        active, SettingsCommand::ChartScale60);
    CHECK(candidate.has_value());
    CHECK(candidate->chartScale == ChartScalePreset::Percent60);
    CHECK(active.chartScale == ChartScalePreset::Percent80);
    CHECK(*candidate != active);
    CHECK(!make_settings_candidate(active, SettingsCommand::TextScale100).has_value());
    CHECK(!make_settings_candidate(
        active, static_cast<SettingsCommand>(9999)).has_value());
}
```

Declare and implement:

```cpp
[[nodiscard]] std::optional<AppearanceSettings> make_settings_candidate(
    const AppearanceSettings& active,
    SettingsCommand command) noexcept;
```

The helper copies `active`, calls `apply_settings_command`, and returns
`std::nullopt` for an invalid command or an unchanged snapshot. Add `<optional>`
to `appearance_settings.h`. Run the test before implementation and confirm RED
because `make_settings_candidate` is undefined; then implement the minimal
helper and verify the unit suite passes.

- [x] **Step 2: Load v2 or v1 before applying QA arguments**

In `win_main.cpp`, preserve the system-derived initial language as the caller defaults, then implement this order:

```cpp
const auto settingsPath = diskbloom::platform::windows::default_settings_path();
if (const auto saved = diskbloom::platform::windows::load_settings(
        settingsPath, appearance)) {
    appearance = *saved;
} else {
    std::error_code existsError;
    const auto v2Exists = std::filesystem::exists(settingsPath, existsError);
    if (!v2Exists && !existsError) {
        if (const auto legacy =
                diskbloom::platform::windows::load_legacy_directory_transition_mode(
                    diskbloom::platform::windows::legacy_settings_path())) {
            appearance.directoryTransitions = *legacy;
        }
    }
}
```

Add `<filesystem>` and `<system_error>` includes. Keep command-line argument
processing after this block, so QA overrides remain process-local.

- [x] **Step 3: Build all checked submenus from localized entries**

Create native popup handles for theme, language, text size, font, chart size, and directory transitions. Append exactly one checked item in each group using the current candidate values. Use these localized ID mappings:

```cpp
TextScale80  -> StringId::Percent80
TextScale90  -> StringId::Percent90
TextScale100 -> StringId::Percent100
TextScale110 -> StringId::Percent110
TextScale120 -> StringId::Percent120
FontSegoeUiVariable  -> StringId::FontSegoeUiVariable
FontMicrosoftYaHeiUi -> StringId::FontMicrosoftYaHeiUi
FontArial            -> StringId::FontArial
FontConsolas         -> StringId::FontConsolas
ChartScale60  -> StringId::Percent60
ChartScale70  -> StringId::Percent70
ChartScale80  -> StringId::Percent80
ChartScale90  -> StringId::Percent90
ChartScale100 -> StringId::Percent100
```

Attach them to the root with `StringId::TextSize`, `StringId::Font`, and `StringId::ChartSize`. On allocation failure, destroy every handle that has not yet become owned by the root. `DestroyMenu(root)` remains the single cleanup after tracking.

- [x] **Step 4: Persist the candidate before activation**

Replace in-place mutation with:

```cpp
const auto command = static_cast<SettingsCommand>(selected);
const auto candidate = make_settings_candidate(appearance_, command);
if (!candidate.has_value()) {
    return;
}
if (!platform::windows::save_settings_atomic(
        platform::windows::default_settings_path(), *candidate)) {
    MessageBoxW(
        window_,
        core::get_string(appearance_.language, core::StringId::ActionFailed).data(),
        core::get_string(appearance_.language, core::StringId::AppTitle).data(),
        MB_OK | MB_ICONERROR);
    return;
}

const auto prior = appearance_;
appearance_ = *candidate;
if (prior.directoryTransitions != appearance_.directoryTransitions) {
    apply_directory_transition_policy_change();
}
SetWindowTextW(
    window_, core::get_string(appearance_.language, core::StringId::AppTitle).data());
apply_appearance();
InvalidateRect(window_, nullptr, FALSE);
```

Do not manually rebuild DirectWrite resources: their typed cache keys invalidate on the next render.

- [x] **Step 5: Verify startup, menu commands, and persistence**

```powershell
cmake --build --preset windows-debug --target DiskBloom diskbloom_tests diskbloom_render_smoke diskbloom_analyzer_render_smoke
ctest --preset windows-debug --output-on-failure
```

Expected: the complete Debug suite passes. Manually launch `build/windows-debug/src/DiskBloom.exe`, select each submenu once in light and dark modes, switch English/Chinese, restart, and confirm the last successful values remain checked.

- [x] **Step 6: Commit menu and startup integration**

```powershell
git add src/app/appearance_settings.h src/app/appearance_settings.cpp src/app/win_main.cpp src/app/main_window.cpp tests/appearance_settings_tests.cpp tests/settings_store_tests.cpp
git commit -m "feat: expose persisted display settings"
```

---

### Task 7: Release QA, Performance Check, And Documentation

**Files:**
- Modify: `docs/qa/sunburst-analyzer.md`
- Modify: `docs/superpowers/plans/2026-07-18-typography-chart-settings.md`
- Create: `docs/qa/evidence/typography-chart-settings/typography-segoe-80-chart-60-light-en.png`
- Create: `docs/qa/evidence/typography-chart-settings/typography-yahei-120-chart-100-dark-zh.png`
- Create: `docs/qa/evidence/typography-chart-settings/typography-arial-80-chart-100-dark-en.png`
- Create: `docs/qa/evidence/typography-chart-settings/typography-consolas-120-chart-60-light-zh.png`

**Interfaces:**
- Consumes: completed feature and analyzer `--capture-dir` support.
- Produces: Release evidence, repeatable render timing, completed checklist, and final executable.

- [x] **Step 1: Run the complete Debug and Release suites**

```powershell
cmake --build --preset windows-debug
ctest --preset windows-debug --output-on-failure
cmake --build --preset windows-release
ctest --preset windows-release --output-on-failure
```

Expected: every registered unit, render, app smoke, and benchmark gate passes in both configurations.

- [x] **Step 2: Generate and inspect Release captures**

```powershell
$evidence = 'docs/qa/evidence/typography-chart-settings'
$temporary = 'build/windows-release/typography-chart-captures'
New-Item -ItemType Directory -Force $evidence | Out-Null
New-Item -ItemType Directory -Force $temporary | Out-Null
build/windows-release/tests/diskbloom_analyzer_render_smoke.exe "--capture-dir=$temporary"
Copy-Item "$temporary/typography-segoe-80-chart-60-light-en.png" $evidence
Copy-Item "$temporary/typography-yahei-120-chart-100-dark-zh.png" $evidence
Copy-Item "$temporary/typography-arial-80-chart-100-dark-en.png" $evidence
Copy-Item "$temporary/typography-consolas-120-chart-60-light-zh.png" $evidence
```

Inspect the four typography/chart frames in Task 5. Verify the chart is centered, text does not overlap preceding or following content, breadcrumb labels trim correctly, action labels remain inside buttons, child rows remain aligned, collector hints remain visible, and Chinese glyph fallback works for Arial and Consolas. Check both light and dark evidence.

- [x] **Step 3: Measure unchanged-settings render smoke repeatedly**

```powershell
$samples = 1..20 | ForEach-Object {
    (Measure-Command {
        build/windows-release/tests/diskbloom_analyzer_render_smoke.exe
    }).TotalMilliseconds
}
$sorted = $samples | Sort-Object
[pscustomobject]@{
    Samples = $samples.Count
    MedianMs = $sorted[[int]($sorted.Count / 2)]
    MaxMs = ($samples | Measure-Object -Maximum).Maximum
}
```

Run the same command on the parent commit if the median materially increases. No regression is accepted without a documented measured reason. This is a repeatable whole-process check; code inspection must also confirm the unchanged typography key returns before any brush or text-format creation.

- [x] **Step 4: Record QA evidence and finish the plan**

Append the exact Debug/Release test totals, capture filenames, manual light/dark and English/Chinese observations, and 20-run median/max timing to `docs/qa/sunburst-analyzer.md`. Mark every completed checkbox in this plan, then run:

```powershell
git diff --check
git status --short
git add docs/qa/sunburst-analyzer.md docs/qa/evidence/typography-chart-settings docs/superpowers/plans/2026-07-18-typography-chart-settings.md
git commit -m "docs: verify typography and chart settings"
```

- [x] **Step 5: Final verification and handoff**

```powershell
cmake --build --preset windows-release --target DiskBloom diskbloom_tests diskbloom_render_smoke diskbloom_analyzer_render_smoke
ctest --preset windows-release --output-on-failure
git status --short
```

Expected: Release verification is green, the worktree is clean, and the executable is available at `D:\daisydiskX\build\windows-release\src\DiskBloom.exe` after integration.
