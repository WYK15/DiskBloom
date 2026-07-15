# Sunburst Analyzer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn a completed `ScanTree` into a bounded, GPU-rendered interactive sunburst with hover hit testing, directory drill-down, parent navigation, and an overview return path.

**Architecture:** A platform-independent core layout converts the selected subtree into at most 2,048 immutable segments without allocating per filesystem node. An analyzer view caches Direct2D path batches by palette color and rebuilds them only when the selected root or viewport changes. `MainWindow` switches to the analyzer only after a fully completed scan result is transferred from the worker.

**Tech Stack:** C++20, contiguous `ScanTree`, Direct2D/DirectWrite on the existing D3D11/DXGI device, Win32 pointer messages, CTest, CMake.

## Global Constraints

- Support dark and light themes through `ThemeTokens::chartPalette`; no literal feature colors.
- All visible text comes from the English and Simplified Chinese catalogs.
- Match DaisyDisk's large central radial visualization and direct drill-down workflow while retaining original branding.
- Chart layout is bounded to 2,048 segments and six radial levels per selected root.
- Do not allocate a Win32 control or COM geometry per segment.
- Hover hit testing must use cached segment math and must not traverse the filesystem tree.
- Completed trees are immutable while displayed; cancelled or failed trees never enter the analyzer.

---

### Task 1: Bounded Sunburst Layout And Polar Hit Testing

**Files:**
- Create: `src/core/sunburst_layout.h`
- Create: `src/core/sunburst_layout.cpp`
- Create: `tests/sunburst_layout_tests.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `const ScanTree&`, a valid `NodeIndex` root, and `SunburstLayoutOptions`.
- Produces: `SunburstLayout build_sunburst_layout(const ScanTree&, NodeIndex, const SunburstLayoutOptions&)`.
- Produces: `std::optional<SunburstHit> hit_test_sunburst(const SunburstLayout&, const SunburstGeometry&, float x, float y)`.
- `SunburstSegment` stores node index, start/end radians, depth, palette index, and `Aggregate` flag.

- [ ] **Step 1: Write failing geometry tests**

Create a root with 60-byte and 40-byte children and assert their sweeps are 60% and 40% of `2*pi`. Add tests for a tiny-child aggregate, a 32-segment cap, invalid roots, radial-band hit testing, and misses inside the center hole/outside the chart.

- [ ] **Step 2: Build and verify RED**

Run `cmake --build --preset windows-debug`. Expected: compilation fails because `core/sunburst_layout.h` does not exist.

- [ ] **Step 3: Implement a bounded two-pass child layout**

For each expanded directory, pass over its direct children once to classify individually visible sweeps and once to append visible segments in stable sibling order. Sum all below-threshold or over-budget children into one final aggregate segment. Expand only individual directory segments using a bounded breadth-first work vector.

- [ ] **Step 4: Implement polar hit testing**

Normalize `atan2` into the layout's `[-pi/2, 3pi/2)` interval, compute the radial depth once, and linearly search only the cached segment array for that depth. Do not touch `ScanTree`.

- [ ] **Step 5: Run all Debug tests and commit**

Run `ctest --preset windows-debug --output-on-failure` and commit as `feat: add bounded sunburst layout`.

### Task 2: Batched Direct2D Analyzer View

**Files:**
- Create: `src/app/analyzer_view.h`
- Create: `src/app/analyzer_view.cpp`
- Create: `tests/analyzer_layout_tests.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Modify: `src/core/language.h`
- Modify: `src/core/string_catalog.cpp`

**Interfaces:**
- Consumes: immutable `ScanTree`, `ThemeTokens`, language, viewport size, and pointer coordinates.
- Produces: `AnalyzerView::set_tree`, `draw`, `pointer_moved`, `pointer_left`, `pointer_pressed`, `take_command`.
- Produces: `AnalyzerCommandKind::{ReturnToOverview,NavigateToNode,NavigateToParent}`.

- [ ] **Step 1: Write failing analyzer layout and command hit tests**

Assert the header, back command, chart center, inner radius, ring width, and details panel remain inside 800x600 and 1200x720 viewports. Assert the back hit target and chart segment command are distinct.

- [ ] **Step 2: Build and verify RED**

Expected: missing `app/analyzer_view.h`.

- [ ] **Step 3: Implement cached color batches**

Build at most 12 `ID2D1PathGeometry` objects, one per chart palette role. Append every annular segment figure to its color sink, close the sinks, then fill the 12 batches. Rebuild only when context, theme, selected root, layout revision, or viewport geometry changes.

- [ ] **Step 4: Render localized analyzer chrome**

Add localized Back, Overview, Items, and aggregate labels. Draw an unframed central chart, compact header, breadcrumb-like current name, and selected/hovered item details using semantic tokens in both themes.

- [ ] **Step 5: Run tests and commit**

Run Debug tests and commit as `feat: render batched sunburst analyzer`.

### Task 3: Main Window Navigation And Drill-Down

**Files:**
- Modify: `src/app/main_window.h`
- Modify: `src/app/main_window.cpp`
- Modify: `src/app/analyzer_view.h`
- Modify: `src/app/analyzer_view.cpp`
- Create: `tests/analyzer_navigation_tests.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- A completed volume scan switches `MainWindow` from overview to analyzer and calls `AnalyzerView::set_tree` with the immutable completed tree.
- Directory activation selects a new chart root; parent activation returns one level; root back returns to the disk overview.

- [ ] **Step 1: Write failing navigation reducer tests**

Assert `Overview -> Analyzer(root)`, directory drill-down, file selection without drill-down, parent navigation, and root return-to-overview.

- [ ] **Step 2: Build and verify RED**

Expected: missing analyzer navigation reducer API.

- [ ] **Step 3: Route rendering and pointer events by active view**

On terminal `Completed`, move only the completed result into stable storage, set the analyzer tree, stop the timer, and invalidate once. Route paint/move/leave/click to the active view; settings and window commands remain available in both views.

- [ ] **Step 4: Run Debug/Release tests and manual scan QA**

Verify cancellation remains on the overview, completion opens the analyzer, drill-down does not rescan, and returning to overview preserves the completed row.

- [ ] **Step 5: Commit**

Commit as `feat: navigate completed scans in sunburst`.

### Task 4: Layout And Interaction Performance Baseline

**Files:**
- Create: `benchmarks/sunburst_layout_benchmark.cpp`
- Modify: `benchmarks/CMakeLists.txt`
- Create: `docs/qa/sunburst-analyzer.md`

**Interfaces:**
- Produces `diskbloom_sunburst_layout_benchmark` accepting a synthetic node count and iteration count.

- [ ] **Step 1: Build a deterministic one-million-node Release fixture**

Measure layout rebuild time for 1,000 iterations and cached polar hit testing for 1,000,000 points. Validate segment count never exceeds 2,048.

- [ ] **Step 2: Run three Release samples**

Record raw layout and hit-test timings, compiler, CPU, and maximum produced segment count.

- [ ] **Step 3: Capture the visual matrix**

Capture analyzer screenshots at 1200x720 for dark/light and zh-CN/en-US, inspect text bounds and chart framing, and compare the interaction hierarchy to the supplied DaisyDisk recording.

- [ ] **Step 4: Record measured gates and commit**

Write same-host gates from the measured medians without extrapolating to other hardware. Commit as `perf: establish sunburst interaction baseline`.
