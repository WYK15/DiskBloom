# Native Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a runnable x64 native Windows application that enumerates local volumes, renders the DaisyDisk-inspired disk overview, and switches light/dark theme and Simplified Chinese/English at runtime.

**Architecture:** A small Win32 application owns one D3D11/DXGI-backed Direct2D render surface. Core resource catalogs and semantic theme tokens are independent of the window, while Windows volume and theme detection live behind platform functions. The disk overview consumes immutable volume snapshots and emits command identifiers for the scanning subsystem implemented in the next plan.

**Tech Stack:** C++20, Win32, CMake, MSVC, Direct3D 11, DXGI, Direct2D 1.1, DirectWrite, CTest.

## Global Constraints

- Target Windows 10 22H2 and Windows 11 on x64.
- Every user-visible state must support light and dark themes.
- Every user-visible string must exist in Simplified Chinese and English.
- Performance-sensitive paths use native C++ and avoid per-frame or per-volume avoidable allocations.
- The application uses an original working title, `DiskBloom`; DaisyDisk names and assets are not shipped.
- Release performance claims require measured evidence.

---

## File Map

```text
CMakeLists.txt                         Root build, test registration, Windows libraries
CMakePresets.json                     Reproducible x64 Debug and Release builds
.gitignore                            Build and editor outputs
src/CMakeLists.txt                     Core, platform, render, and executable targets
src/core/language.h                    Language and string identifiers
src/core/string_catalog.h/.cpp         Runtime bilingual lookup
src/core/theme.h/.cpp                  Semantic color and metric tokens
src/platform/windows/system_theme.h/.cpp  Windows theme detection
src/platform/windows/volume_service.h/.cpp Mounted-volume snapshots
src/render/graphics_device.h/.cpp      D3D11, DXGI, D2D, and DirectWrite lifecycle
src/app/disk_overview.h/.cpp           Disk-row layout, drawing, and hit targets
src/app/main_window.h/.cpp             Win32 lifecycle, input, language/theme commands
src/app/win_main.cpp                   Process entry point
tests/test_main.cpp                    Minimal deterministic test runner
tests/test_support.h/.cpp              Test registration and assertions
tests/string_catalog_tests.cpp         Catalog coverage and switching
tests/theme_tests.cpp                  Theme token invariants
tests/volume_service_tests.cpp         Pure formatting and capacity invariants
tests/render_smoke_main.cpp             DirectX initialization smoke executable
tests/disk_overview_layout_tests.cpp    DPI-aware deterministic layout tests
```

### Task 1: Build and Test Bootstrap

**Files:**
- Create: `.gitignore`
- Create: `CMakeLists.txt`
- Create: `CMakePresets.json`
- Create: `src/CMakeLists.txt`
- Create: `tests/CMakeLists.txt`
- Create: `tests/test_main.cpp`
- Create: `tests/test_support.h`
- Create: `tests/test_support.cpp`

**Interfaces:**
- Produces: `diskbloom_tests` CTest target. The application target is introduced after its entry point exists.

- [ ] **Step 1: Add a deliberately failing test harness**

```cpp
// tests/test_main.cpp
#include <iostream>

int main() {
    std::cerr << "bootstrap test has no implementation\n";
    return 1;
}
```

- [ ] **Step 2: Configure and run the test to verify failure**

Run from a Visual Studio developer shell:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug --output-on-failure
```

Expected: `diskbloom_tests` fails with `bootstrap test has no implementation`.

- [ ] **Step 3: Replace the harness with a reusable registration API**

```cpp
// tests/test_main.cpp
#include "test_support.h"

int main() {
    return diskbloom::tests::run_all();
}
```

`test_support.h` defines `TEST_CASE(name)`, `CHECK(condition)`, a registry of `void()` test functions, and `run_all()` returning nonzero when any check fails.

- [ ] **Step 4: Run the empty harness successfully**

Run: `ctest --preset windows-debug --output-on-failure`

Expected: one test target passes and reports zero failed checks.

- [ ] **Step 5: Commit the build foundation**

```powershell
git add .gitignore CMakeLists.txt CMakePresets.json src/CMakeLists.txt tests
git commit -m "build: bootstrap native C++ application"
```

### Task 2: Bilingual String Catalog

**Files:**
- Create: `src/core/language.h`
- Create: `src/core/string_catalog.h`
- Create: `src/core/string_catalog.cpp`
- Create: `tests/string_catalog_tests.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `enum class Language { English, SimplifiedChinese }`.
- Produces: `enum class StringId { AppTitle, Scan, Cancel, ScanFolder, Settings, UsedOfTotal, FixedDrive, RemovableDrive, ScanFailed, Count }`.
- Produces: `std::wstring_view get_string(Language, StringId) noexcept`.

- [ ] **Step 1: Write failing catalog tests**

```cpp
TEST_CASE(string_catalog_has_every_translation) {
    for (auto language : {Language::English, Language::SimplifiedChinese}) {
        for (std::size_t value = 0; value < static_cast<std::size_t>(StringId::Count); ++value) {
            CHECK(!get_string(language, static_cast<StringId>(value)).empty());
        }
    }
}

TEST_CASE(string_catalog_switches_language) {
    CHECK(get_string(Language::English, StringId::Scan) == L"Scan");
    CHECK(get_string(Language::SimplifiedChinese, StringId::Scan) == L"\u626b\u63cf");
}
```

- [ ] **Step 2: Build to verify the missing API fails compilation**

Run: `cmake --build --preset windows-debug`

Expected: compilation fails because `Language`, `StringId`, and `get_string` are undefined.

- [ ] **Step 3: Implement fixed contiguous catalogs**

Use two `constexpr std::array<std::wstring_view, static_cast<size_t>(StringId::Count)>` values indexed by `StringId`. `get_string` performs a bounds check and returns an empty view only for an invalid identifier.

- [ ] **Step 4: Run catalog tests**

Run: `ctest --preset windows-debug --output-on-failure`

Expected: all catalog checks pass for both languages.

- [ ] **Step 5: Commit localization**

```powershell
git add src/core tests CMakeLists.txt src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add runtime bilingual string catalog"
```

### Task 3: Semantic Themes

**Files:**
- Create: `src/core/theme.h`
- Create: `src/core/theme.cpp`
- Create: `src/platform/windows/system_theme.h`
- Create: `src/platform/windows/system_theme.cpp`
- Create: `tests/theme_tests.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `enum class ThemeMode { System, Light, Dark }`.
- Produces: `struct Rgba { float r, g, b, a; }`.
- Produces: `struct ThemeTokens` containing window, titleBar, row, alternateRow, primaryText, secondaryText, track, button, buttonHover, border, accent, danger, focus, and a 12-color chart palette.
- Produces: `ThemeTokens make_theme(bool dark) noexcept`.
- Produces: `float contrast_ratio(Rgba, Rgba) noexcept`.
- Produces: `bool is_system_dark_theme() noexcept`.

- [ ] **Step 1: Write failing theme invariants**

```cpp
TEST_CASE(theme_tokens_are_visible) {
    const auto light = make_theme(false);
    const auto dark = make_theme(true);
    CHECK(contrast_ratio(light.primaryText, light.window) >= 4.5f);
    CHECK(contrast_ratio(dark.primaryText, dark.window) >= 4.5f);
    CHECK(light.window.r != dark.window.r);
    CHECK(light.chartPalette.size() == 12);
    CHECK(dark.chartPalette.size() == 12);
}
```

- [ ] **Step 2: Build to verify theme APIs are missing**

Run: `cmake --build --preset windows-debug`

Expected: compilation fails on `make_theme` and `contrast_ratio`.

- [ ] **Step 3: Implement theme creation and system detection**

`make_theme` returns fully populated semantic tokens. `is_system_dark_theme` reads `AppsUseLightTheme` from `HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize` and defaults to light when unavailable.

- [ ] **Step 4: Run theme tests**

Run: `ctest --preset windows-debug --output-on-failure`

Expected: light and dark contrast and palette checks pass.

- [ ] **Step 5: Commit themes**

```powershell
git add src/core src/platform tests src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add semantic light and dark themes"
```

### Task 4: Volume Enumeration

**Files:**
- Create: `src/platform/windows/volume_service.h`
- Create: `src/platform/windows/volume_service.cpp`
- Create: `tests/volume_service_tests.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `enum class VolumeKind { Fixed, Removable }`.
- Produces: `struct VolumeSnapshot { std::wstring rootPath; std::wstring displayName; uint64_t totalBytes; uint64_t freeBytes; VolumeKind kind; }`.
- Produces: `std::vector<VolumeSnapshot> enumerate_volumes()`.
- Produces: `double used_fraction(const VolumeSnapshot&) noexcept`.

- [ ] **Step 1: Write failing capacity tests**

```cpp
TEST_CASE(used_fraction_is_bounded) {
    CHECK(used_fraction({L"C:\\", L"System", 100, 25, VolumeKind::Fixed}) == 0.75);
    CHECK(used_fraction({L"X:\\", L"Empty", 0, 0, VolumeKind::Removable}) == 0.0);
    CHECK(used_fraction({L"X:\\", L"Odd", 100, 120, VolumeKind::Removable}) == 0.0);
}
```

- [ ] **Step 2: Build to verify the service is missing**

Run: `cmake --build --preset windows-debug`

Expected: compilation fails because `VolumeSnapshot` and `used_fraction` do not exist.

- [ ] **Step 3: Implement enumeration**

Use `GetLogicalDriveStringsW`, `GetDriveTypeW`, `GetVolumeInformationW`, and `GetDiskFreeSpaceExW`. Include only ready fixed and removable volumes, preserve root paths, and sort fixed volumes before removable volumes without locale-dependent comparison.

- [ ] **Step 4: Run tests and a diagnostic enumeration**

Run: `ctest --preset windows-debug --output-on-failure`

Expected: capacity tests pass. A Debug log from the application lists each included root exactly once.

- [ ] **Step 5: Commit volume discovery**

```powershell
git add src/platform tests src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: enumerate local Windows volumes"
```

### Task 5: DirectX Graphics Device

**Files:**
- Create: `src/render/graphics_device.h`
- Create: `src/render/graphics_device.cpp`
- Create: `tests/render_smoke_main.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `class GraphicsDevice` with `initialize(HWND)`, `resize(uint32_t,uint32_t)`, `begin_draw(Rgba)`, `end_draw()`, `d2d_context()`, `dwrite_factory()`, and `discard_device_resources()`.
- Consumes: `Rgba` from `src/core/theme.h`.

- [ ] **Step 1: Add a renderer smoke-test mode to the executable**

The `diskbloom_render_smoke` test executable creates a hidden Win32 window, initializes `GraphicsDevice`, renders one cleared frame, presents it, and exits zero. Before implementation, the link must fail because `GraphicsDevice` is missing.

- [ ] **Step 2: Build to verify the renderer is missing**

Run: `cmake --build --preset windows-debug`

Expected: linking fails on `GraphicsDevice` methods.

- [ ] **Step 3: Implement device creation and recovery**

Create a BGRA-capable D3D11 device, flip-sequential DXGI swap chain, Direct2D device/context, target bitmap, and shared DirectWrite factory. `end_draw` handles `D2DERR_RECREATE_TARGET`, `DXGI_ERROR_DEVICE_REMOVED`, and `DXGI_ERROR_DEVICE_RESET` by discarding device-dependent resources.

- [ ] **Step 4: Run the renderer smoke test**

Run: `build/windows-debug/tests/diskbloom_render_smoke.exe`

Expected: exit code 0 with no DirectX debug-layer error.

- [ ] **Step 5: Commit the renderer**

```powershell
git add src/render tests/render_smoke_main.cpp src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add native Direct2D render surface"
```

### Task 6: Disk Overview Window

**Files:**
- Create: `src/app/disk_overview.h`
- Create: `src/app/disk_overview.cpp`
- Create: `src/app/main_window.h`
- Create: `src/app/main_window.cpp`
- Create: `src/app/win_main.cpp`
- Create: `tests/disk_overview_layout_tests.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `enum class OverviewCommandKind { ScanVolume, CancelScan, ScanFolder, OpenSettings, ToggleTheme, ToggleLanguage }`.
- Produces: `struct OverviewCommand { OverviewCommandKind kind; std::size_t volumeIndex; }`.
- Produces: `OverviewLayout compute_overview_layout(float widthDip, float heightDip, std::size_t volumeCount) noexcept`.
- Produces: `class DiskOverview` with `set_volumes`, `draw`, `pointer_moved`, `pointer_pressed`, and `take_command`.
- Consumes: `ThemeTokens`, `Language`, `get_string`, `VolumeSnapshot`, `GraphicsDevice`.

- [ ] **Step 1: Write deterministic layout tests**

Add pure `compute_overview_layout(width, height, volumeCount)` tests that assert a 76 DIP title bar, 100 DIP volume rows, stable 14 DIP row gaps, and non-overlapping scan-button rectangles at 100%, 150%, and 200% DPI.

- [ ] **Step 2: Build to verify layout and overview types are missing**

Run: `cmake --build --preset windows-debug`

Expected: compilation fails on `compute_overview_layout` and `DiskOverview`.

- [ ] **Step 3: Implement the window and drawing**

Create a borderless resizable Win32 window with DWM-backed rounded corners where supported. Draw the title bar, application title, alternating volume rows, drive label/capacity, usage track, and scan button through Direct2D/DirectWrite. Store text formats and brushes across frames and update them only when DPI, theme, language, or device changes.

- [ ] **Step 4: Implement runtime theme and language commands**

Keyboard-accessible Settings commands switch `ThemeMode` and `Language`, invalidate cached text layout, and repaint without process restart. The initial implementation exposes these commands through a compact settings menu opened from the title bar.

- [ ] **Step 5: Run automated and manual checks**

Run:

```powershell
ctest --preset windows-debug --output-on-failure
build/windows-debug/src/DiskBloom.exe
```

Expected: all tests pass; the window lists real local volumes; theme and language changes update all visible colors and strings immediately; resize and DPI changes do not overlap rows or buttons.

- [ ] **Step 6: Commit the first runnable vertical slice**

```powershell
git add src tests src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: render localized disk overview"
```

### Task 7: Release Build and Foundation Evidence

**Files:**
- Create: `docs/qa/native-foundation.md`
- Create: `README.md`

**Interfaces:**
- Consumes: complete native foundation executable and test suite.
- Produces: reproducible build/verification commands and visual QA record.

- [ ] **Step 1: Configure and build Release**

Run:

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release --output-on-failure
```

Expected: clean x64 Release build and all tests pass.

- [ ] **Step 2: Record foundation measurements**

Measure executable size, cold process startup time to first presented frame, idle working set after five seconds, and frame time while resizing. Record the machine, commands, and raw values in `docs/qa/native-foundation.md`; do not label unmeasured values.

- [ ] **Step 3: Capture the required visual matrix**

Capture the disk overview at 1600x1200 for light/en-US, light/zh-CN, dark/en-US, and dark/zh-CN. Compare spacing, density, hierarchy, progress-track proportions, and command placement with `feedback/ds_launch.png`. Record discrepancies in `docs/qa/native-foundation.md` before adjusting the implementation.

- [ ] **Step 4: Re-run Release verification after visual adjustments**

Run: `cmake --build --preset windows-release; ctest --preset windows-release --output-on-failure`

Expected: build succeeds and all tests pass after final visual changes.

- [ ] **Step 5: Commit foundation evidence**

```powershell
git add README.md docs/qa src tests
git commit -m "docs: verify native application foundation"
```
