# Smaller Sunburst Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reduce the analyzer sunburst radius from 91 percent to 80 percent of its original responsive radius.

**Architecture:** Keep the existing responsive radius calculation and change only its final scale factor. The chart center, inner-radius ratio, ring width, hit testing, transitions, and surrounding layout continue to derive from the resulting radius.

**Tech Stack:** C++20, Direct2D analyzer geometry, custom C++ test harness, CMake/CTest.

## Global Constraints

- Every feature and UI change must support light and dark mode.
- User-facing behavior must remain valid in Simplified Chinese and English.
- Preserve DaisyDisk-like layout and interaction behavior.
- Performance remains a primary requirement; this change must not add per-frame work or allocation.

---

### Task 1: Scale The Analyzer Chart To 80 Percent

**Files:**
- Modify: `tests/analyzer_layout_tests.cpp`
- Modify: `src/app/analyzer_view.cpp`

**Interfaces:**
- Consumes: `compute_analyzer_layout(float widthDip, float heightDip, std::size_t depthCount)`.
- Produces: the same `AnalyzerLayout` contract with chart radius scaled by `0.80F`.

- [x] **Step 1: Write the failing radius test**

Rename the existing scale test and change its expected factor:

```cpp
TEST_CASE(analyzer_layout_scales_chart_radius_to_eighty_percent) {
    const auto layout = compute_analyzer_layout(1200.0F, 720.0F, 6U);
    const auto radius = (layout.chartBounds.right - layout.chartBounds.left) * 0.5F;
    constexpr float unscaledResponsiveRadius = 288.0F;

    CHECK(std::abs(radius - unscaledResponsiveRadius * 0.80F) < 0.001F);
    CHECK(layout.chartGeometry.centerX
        == (layout.chartBounds.left + layout.chartBounds.right) * 0.5F);
    CHECK(AnalyzerCommandKind::RestoreReviewItem != AnalyzerCommandKind::ConfirmReview);
}
```

- [x] **Step 2: Run the test and verify RED**

```powershell
cmake --build --preset windows-debug --target diskbloom_tests
build/windows-debug/tests/diskbloom_tests.exe
```

Expected: `analyzer_layout_scales_chart_radius_to_eighty_percent` fails because the implementation still multiplies by `0.91F`.

- [x] **Step 3: Implement the 80-percent scale**

In `compute_analyzer_layout`, change only the final scale factor:

```cpp
const auto radius = responsiveRadius * 0.80F;
```

- [x] **Step 4: Verify unit and render behavior**

```powershell
cmake --build --preset windows-debug --target diskbloom_tests diskbloom_analyzer_render_smoke
ctest --preset windows-debug -R "diskbloom_tests|diskbloom_analyzer_render_smoke" --output-on-failure
```

Expected: both tests pass; existing containment, center, ring-width, hit-test, theme, language, and transition assertions remain green.

- [x] **Step 5: Verify Release and commit**

```powershell
cmake --build --preset windows-release --target DiskBloom diskbloom_tests diskbloom_analyzer_render_smoke
ctest --preset windows-release -R "diskbloom_tests|diskbloom_analyzer_render_smoke" --output-on-failure
git add src/app/analyzer_view.cpp tests/analyzer_layout_tests.cpp docs/superpowers/plans/2026-07-17-smaller-sunburst.md
git commit -m "fix: reduce analyzer chart size"
```

Expected: Release verification passes and the executable is available at `D:\daisydiskX\build\windows-release\src\DiskBloom.exe`.
