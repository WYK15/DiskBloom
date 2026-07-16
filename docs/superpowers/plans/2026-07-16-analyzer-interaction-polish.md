# Analyzer Interaction Polish Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a DaisyDisk-style right-list-to-sunburst hover pulse, replace the empty collector directory name with a localized drag hint, and make unused analyzer header space drag the native window.

**Architecture:** A render-independent hover module owns branch matching and the 900 ms pulse phase. `AnalyzerView` caches one Direct2D overlay geometry per hovered row and exposes timer/header-hit state; `MainWindow` owns one bounded 16 ms hover timer and native `WM_NCHITTEST` routing. Collector copy stays in the existing string catalog and draw path.

**Tech Stack:** C++20, immutable bounded `SunburstLayout`, Win32 timers and non-client hit testing, Direct2D/DirectWrite, CMake/CTest, D3D11 BGRA capture.

## Global Constraints

- Every new visual works in dark/light themes and en-US/zh-CN.
- The hover branch scans only the cached layout and never traverses the filesystem.
- Geometry construction is bounded to 2,048 segments and occurs only when the hovered row changes.
- Animated frames perform no heap allocation and submit one additional `FillGeometry` call.
- The pulse period is 900 ms; disabled effective animation uses a static highlight and no timer.
- Directory transitions suspend and clear list hover pulse state.
- Back, forward, visible breadcrumb pills, ellipsis, and window controls remain clickable.
- Populated collector behavior and recycle-only deletion semantics remain unchanged.
- Every behavioral change is developed test-first with an observed relevant RED failure.

---

### Task 1: Pure Hover Branch And Pulse Model

**Files:**
- Create: `src/app/analyzer_hover_pulse.h`
- Create: `src/app/analyzer_hover_pulse.cpp`
- Modify: `src/CMakeLists.txt`
- Create: `tests/analyzer_hover_pulse_tests.cpp`
- Modify: `tests/CMakeLists.txt`
- Create: `benchmarks/analyzer_hover_pulse_benchmark.cpp`
- Modify: `benchmarks/CMakeLists.txt`

**Interfaces:**
- Consumes: bounded `core::SunburstLayout`, hovered immediate-child `NodeIndex`, monotonic time, and effective animation state.
- Produces: `find_hover_branch`, `segment_is_in_hover_branch`, and `AnalyzerHoverPulse` lifecycle/opacity APIs.

- [ ] **Step 1: Write failing branch-mapping tests**

Build a deterministic layout with two root branches and descendants, then assert:

```cpp
const auto branch = find_hover_branch(layout, firstChild);
CHECK(branch.has_value());
CHECK(segment_is_in_hover_branch(layout.segments[firstIndex], *branch));
CHECK(segment_is_in_hover_branch(layout.segments[firstGrandchildIndex], *branch));
CHECK(!segment_is_in_hover_branch(layout.segments[secondIndex], *branch));
CHECK(!find_hover_branch(layout, invalid_node).has_value());
CHECK(!find_hover_branch(layout, aggregatedOutNode).has_value());
```

Add a file-child case and exact half-open angular boundary assertions.

- [ ] **Step 2: Write failing pulse lifecycle tests**

Use fixed `steady_clock::time_point` values:

```cpp
AnalyzerHoverPulse pulse;
pulse.set_target(firstChild, start);
CHECK(pulse.has_target());
CHECK(pulse.timer_required(true));
CHECK(!pulse.timer_required(false));
CHECK(pulse.opacity(start, true) == pulse.opacity(start + 900ms, true));
CHECK(pulse.opacity(start + 225ms, true) > pulse.opacity(start, true));
CHECK(pulse.opacity(start, false) == pulse.opacity(start + 225ms, false));
pulse.clear();
CHECK(!pulse.has_target());
```

Assert all opacity samples remain within declared `minimum_opacity` and `maximum_opacity`, and changing targets resets the phase.

- [ ] **Step 3: Build and verify RED**

Run in an x64 Visual Studio developer environment:

```powershell
cmake --build build/windows-debug --config Debug --target diskbloom_tests --parallel
```

Expected: compilation fails because `app/analyzer_hover_pulse.h` does not exist.

- [ ] **Step 4: Implement the bounded pure model**

Declare:

```cpp
struct AnalyzerHoverBranch {
    float startAngle = 0.0F;
    float endAngle = 0.0F;
    core::NodeIndex node = core::invalid_node;
};

[[nodiscard]] std::optional<AnalyzerHoverBranch> find_hover_branch(
    const core::SunburstLayout& layout,
    core::NodeIndex node) noexcept;
[[nodiscard]] bool segment_is_in_hover_branch(
    const core::SunburstSegment& segment,
    const AnalyzerHoverBranch& branch) noexcept;

class AnalyzerHoverPulse final {
public:
    static constexpr auto period = std::chrono::milliseconds(900);
    static constexpr float minimum_opacity = 0.10F;
    static constexpr float maximum_opacity = 0.32F;
    void set_target(core::NodeIndex node, std::chrono::steady_clock::time_point now) noexcept;
    void clear() noexcept;
    [[nodiscard]] bool has_target() const noexcept;
    [[nodiscard]] core::NodeIndex target() const noexcept;
    [[nodiscard]] bool timer_required(bool animationsEnabled) const noexcept;
    [[nodiscard]] float opacity(
        std::chrono::steady_clock::time_point now,
        bool animationsEnabled) const noexcept;
};
```

Match only nonaggregate depth-zero segments by node. Use a sine-derived `0..1..0` phase over 900 ms and return the midpoint opacity when animation is disabled.

- [ ] **Step 5: Add a deterministic bounded benchmark**

Create a Release benchmark that builds one 2,048-segment layout, selects a depth-zero branch, and measures 1,000 iterations of `find_hover_branch` plus containment checks over every segment. Preallocate timing storage, consume matches in a stable checksum, print `segments`, `iterations`, `average_ms`, `p95_ms`, and `checksum`, and exit nonzero when p95 exceeds 1 ms or the checksum changes. Register a CTest gate that requires `segments=2048`.

- [ ] **Step 6: Verify GREEN and commit**

Run the focused unit target and test, then commit:

```powershell
git add src/app/analyzer_hover_pulse.h src/app/analyzer_hover_pulse.cpp src/CMakeLists.txt tests/analyzer_hover_pulse_tests.cpp tests/CMakeLists.txt benchmarks/analyzer_hover_pulse_benchmark.cpp benchmarks/CMakeLists.txt
git commit -m "feat: model analyzer hover pulse"
```

---

### Task 2: Cached Hover Geometry And Win32 Timer

**Files:**
- Modify: `src/app/analyzer_view.h`
- Modify: `src/app/analyzer_view.cpp`
- Modify: `src/app/main_window.h`
- Modify: `src/app/main_window.cpp`
- Modify: `tests/analyzer_render_smoke_main.cpp`

**Interfaces:**
- Consumes: Task 1 pulse target, branch interval, current layout geometry, theme `primaryText`, and effective animation policy.
- Produces: cached overlay geometry, `hover_pulse_timer_required`, and one synchronized Win32 timer.

- [ ] **Step 1: Add failing lifecycle assertions to analyzer render smoke**

After laying out the analyzer, move the pointer to a visible ranked row and assert:

```cpp
CHECK(analyzer.pointer_moved(rowCenterX, rowCenterY));
CHECK(analyzer.hover_pulse_active());
CHECK(analyzer.hover_pulse_timer_required());
analyzer.set_hover_animations_enabled(false);
CHECK(analyzer.hover_pulse_active());
CHECK(!analyzer.hover_pulse_timer_required());
CHECK(analyzer.pointer_left());
CHECK(!analyzer.hover_pulse_active());
```

Capture two enabled pulse phases and one disabled static-highlight frame in dark/en-US and light/zh-CN.

- [ ] **Step 2: Build and verify RED**

Expected: analyzer smoke compilation fails because the hover-pulse view APIs are missing.

- [ ] **Step 3: Integrate state and cached Direct2D geometry**

Add to `AnalyzerView`:

```cpp
void set_hover_animations_enabled(bool enabled) noexcept;
[[nodiscard]] bool hover_pulse_active() const noexcept;
[[nodiscard]] bool hover_pulse_timer_required() const noexcept;
```

When `hoveredChild_` changes, map its ranked child to `NodeIndex`, restart the pulse with `steady_clock::now`, and invalidate one cached `ID2D1PathGeometry`. `ensure_hover_pulse_geometry` finds the depth-zero branch and appends only contained segments with existing `append_annular_segment`. Theme resource construction creates one pulse brush from `theme.primaryText`. During static chart rendering, draw the cached overlay after palette batches and before the center disk, using the controller opacity. Clear pulse state on root/tree changes, transitions, pointer leave, review-panel takeover, and recycling.

- [ ] **Step 4: Own timer lifecycle in MainWindow**

Add `hover_pulse_timer_id = 4`, reuse the 16 ms interval, and implement `sync_analyzer_hover_timer()`:

```cpp
if (navigation_.view == MainContentView::Analyzer
    && analyzer_.hover_pulse_timer_required()) {
    SetTimer(window_, hover_pulse_timer_id, animation_timer_interval_ms, nullptr);
} else {
    KillTimer(window_, hover_pulse_timer_id);
}
```

Call it after analyzer pointer moves/leaves, navigation changes, settings policy changes, resize/transition cancellation, and content changes. The timer only invalidates while required. Kill it on `WM_DESTROY`.

- [ ] **Step 5: Run analyzer smoke and full Debug suite**

Run:

```powershell
cmake --build build/windows-debug --config Debug --parallel
ctest --test-dir build/windows-debug -C Debug --output-on-failure
```

Expected: all existing tests plus the new hover lifecycle/capture assertions pass.

- [ ] **Step 6: Commit rendering and timer integration**

```powershell
git add src/app/analyzer_view.h src/app/analyzer_view.cpp src/app/main_window.h src/app/main_window.cpp tests/analyzer_render_smoke_main.cpp
git commit -m "feat: pulse hovered analyzer branches"
```

---

### Task 3: Localized Empty Collector Hint

**Files:**
- Modify: `src/core/language.h`
- Modify: `src/core/string_catalog.cpp`
- Modify: `src/app/analyzer_view.cpp`
- Modify: `tests/string_catalog_tests.cpp`
- Modify: `tests/analyzer_render_smoke_main.cpp`

**Interfaces:**
- Consumes: existing collector summary bounds, `buttonFormat`, and semantic secondary-text brush.
- Produces: `StringId::CollectorDragHint` in both languages and one-line empty collector rendering.

- [ ] **Step 1: Write failing localization tests**

Add:

```cpp
CHECK(get_string(Language::English, StringId::CollectorDragHint)
    == L"Drag files here to collect them for deletion");
CHECK(get_string(Language::SimplifiedChinese, StringId::CollectorDragHint)
    == L"\u5c06\u6587\u4ef6\u62d6\u653e\u81f3\u6b64\uff0c\u4ee5\u6536\u96c6\u9700\u8981\u5220\u9664\u7684\u6587\u4ef6");
```

The existing completeness loop must include the new ID.

- [ ] **Step 2: Build and verify RED**

Expected: compilation fails because `CollectorDragHint` is missing.

- [ ] **Step 3: Replace current-directory empty rendering**

Append the string ID and both translations. In the `reviewItemCount_ == 0` draw branch, remove `tree_->name(selectedNode_)`; draw `CollectorDragHint` once inside `reviewLayout_.summary` with `buttonFormat` and the secondary brush. Retain the populated count/bytes branch and drag background unchanged.

- [ ] **Step 4: Verify all theme/language render combinations and commit**

Run `diskbloom_tests`, `diskbloom_analyzer_render_smoke`, and all four app smoke cases, then commit:

```powershell
git add src/core/language.h src/core/string_catalog.cpp src/app/analyzer_view.cpp tests/string_catalog_tests.cpp tests/analyzer_render_smoke_main.cpp
git commit -m "feat: add empty collector drag hint"
```

---

### Task 4: Precise Native Header Hit Testing

**Files:**
- Modify: `src/app/analyzer_view.h`
- Modify: `src/app/analyzer_view.cpp`
- Modify: `src/app/main_window.cpp`
- Modify: `tests/analyzer_breadcrumb_tests.cpp`
- Modify: `tests/analyzer_layout_tests.cpp`

**Interfaces:**
- Consumes: current `AnalyzerLayout`, actual visible `AnalyzerBreadcrumbLayout`, and pointer DIPs.
- Produces: `is_analyzer_header_interactive_point` plus an `AnalyzerView` forwarding query used by `WM_NCHITTEST`.

- [ ] **Step 1: Write failing pure hit-test coverage**

Use a compact breadcrumb layout and assert:

```cpp
CHECK(is_analyzer_header_interactive_point(layout, breadcrumb, backX, backY));
CHECK(is_analyzer_header_interactive_point(layout, breadcrumb, pillX, pillY));
CHECK(is_analyzer_header_interactive_point(layout, breadcrumb, ellipsisX, ellipsisY));
CHECK(!is_analyzer_header_interactive_point(layout, breadcrumb, gapX, gapY));
CHECK(!is_analyzer_header_interactive_point(layout, breadcrumb, unusedX, unusedY));
```

Also cover forward and three window buttons, half-open edges, and wide/compact breadcrumb layouts.

- [ ] **Step 2: Build and verify RED**

Expected: compilation fails because the pure header hit-test function is missing.

- [ ] **Step 3: Implement client-control priority**

Add:

```cpp
[[nodiscard]] bool is_analyzer_header_interactive_point(
    const AnalyzerLayout& layout,
    const AnalyzerBreadcrumbLayout& breadcrumb,
    float xDip,
    float yDip) noexcept;
```

Return true only for back/forward, visible breadcrumb segments, ellipsis, and three window-button rectangles. Expose `AnalyzerView::header_point_is_interactive` to query the current layouts.

- [ ] **Step 4: Route WM_NCHITTEST through actual geometry**

Keep border/resize handling first. Within the caption band and left of window controls, return `HTCLIENT` only when the analyzer forwarding query is true; otherwise return `HTCAPTION`. Preserve existing overview behavior and native double-click maximize.

- [ ] **Step 5: Run focused and full tests, then commit**

```powershell
git add src/app/analyzer_view.h src/app/analyzer_view.cpp src/app/main_window.cpp tests/analyzer_breadcrumb_tests.cpp tests/analyzer_layout_tests.cpp
git commit -m "fix: restore analyzer header window dragging"
```

---

### Task 5: Release Visual And Performance Verification

**Files:**
- Modify: `docs/qa/sunburst-analyzer.md`
- Create: `docs/qa/evidence/analyzer-interaction-polish/hover-pulse-dark-en.png`
- Create: `docs/qa/evidence/analyzer-interaction-polish/hover-pulse-light-zh.png`
- Create: `docs/qa/evidence/analyzer-interaction-polish/collector-hint-dark-zh.png`
- Modify: `docs/superpowers/plans/2026-07-16-analyzer-interaction-polish.md`

**Interfaces:**
- Consumes: completed Tasks 1-4.
- Produces: inspected pixel evidence, repeated performance results, checked plan, and latest Release executable.

- [ ] **Step 1: Build and test Release**

```powershell
cmake --build build/windows-release --config Release --parallel
ctest --test-dir build/windows-release -C Release --output-on-failure
```

Expected: the full suite passes, including dark/light, en-US/zh-CN, transition, collector, and new hover tests.

- [ ] **Step 2: Capture and inspect visual states**

Extend the deterministic analyzer capture path to save dark/en-US animated highlight, light/zh-CN animated highlight, and dark/zh-CN empty collector hint frames. Inspect each image for correct branch isolation, visible but readable overlay, unclipped hint text, and no overlap.

- [ ] **Step 3: Re-run performance gates**

Run the Release sunburst transition and collector benchmarks three times. Checksums and existing gates must remain unchanged. Record the hover branch geometry construction timing over a deterministic 2,048-segment layout; its loop performs no filesystem access and must remain below 1 ms P95 on the recorded host.

- [ ] **Step 4: Manual native interaction verification**

In the visible Release app, hover one file and one directory, move between rows, leave the window, toggle animation modes, drag the window from multiple breadcrumb gaps, and click all breadcrumb/window controls. Repeat the empty hint check in both languages and themes.

- [ ] **Step 5: Record QA and commit**

Update QA with commands, results, limitations, and evidence paths; mark every plan checkbox, run `git diff --check`, and commit:

```powershell
git add docs/qa/sunburst-analyzer.md docs/qa/evidence/analyzer-interaction-polish docs/superpowers/plans/2026-07-16-analyzer-interaction-polish.md tests/analyzer_render_smoke_main.cpp
git commit -m "docs: verify analyzer interaction polish"
```
