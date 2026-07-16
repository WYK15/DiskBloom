# Review Collector Interactions Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a DaisyDisk-style hover review panel and drag-to-collector interaction from both analyzer chart segments and ranked child rows.

**Architecture:** Pure collector geometry and drag-state code lives in a focused app module and is tested without Win32 or Direct2D. `AnalyzerView` maps existing chart/list hits to node indices, owns transient hover/drag state, and draws the panel and drag preview in its existing Direct2D pass; `MainWindow` remains the only owner of `DeletionReview`, mouse capture, confirmation, and recycle execution.

**Tech Stack:** C++20, Win32 mouse input/capture, immutable contiguous `ScanTree`, Direct2D/DirectWrite, CMake/CTest, MSVC Release benchmarks.

## Global Constraints

- Every visual and interaction state must support light and dark themes.
- Every user-facing string must support runtime English and Simplified Chinese switching.
- The panel and drag behavior should reproduce `feedback/PixPin_2026-07-16_09-30-14.png` as closely as practical while retaining DiskBloom branding.
- Pointer movement must not perform filesystem calls, path construction, review-vector rebuilding, or per-node native-window work.
- Dropping adds an item to `DeletionReview`; it never deletes immediately.
- The existing explicit confirmation and recycle-only deletion policy must remain unchanged.
- Dragging is internal to DiskBloom; Windows Explorer/OLE drag-and-drop is out of scope.
- Implement every behavior test-first and observe the expected failure before production changes.

---

### Task 1: Pure Collector Layout And Drag State

**Files:**
- Create: `src/app/analyzer_geometry.h`
- Create: `src/app/review_collector_interaction.h`
- Create: `src/app/review_collector_interaction.cpp`
- Modify: `src/app/analyzer_view.h`
- Modify: `src/CMakeLists.txt`
- Create: `tests/review_collector_interaction_tests.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `AnalyzerRectF` moved without behavior changes from `app/analyzer_view.h` to `app/analyzer_geometry.h`, plus `core::NodeIndex`.
- Produces: `ReviewCollectorLayout compute_review_collector_layout(...)`, `std::size_t next_review_scroll_offset(...)`, `bool contains_point(...)`, and `ReviewDragState` with `begin`, `move`, `release`, and `cancel`.

- [ ] **Step 1: Write failing collector geometry and scrolling tests**

Move the existing `AnalyzerRectF` declaration into `src/app/analyzer_geometry.h`, include that header from `analyzer_view.h`, then add `tests/review_collector_interaction_tests.cpp` with these cases:

```cpp
TEST_CASE(review_collector_panel_is_anchored_above_summary_and_clipped_to_viewport) {
    const AnalyzerRectF actionBar{0.0F, 640.0F, 1200.0F, 720.0F};
    const auto layout = compute_review_collector_layout(
        actionBar, 1200.0F, 720.0F, 20U, 0U);
    CHECK(layout.summary.left == 20.0F);
    CHECK(layout.summary.bottom == actionBar.bottom);
    CHECK(layout.panel.bottom == actionBar.top + 1.0F);
    CHECK(layout.panel.top >= 64.0F);
    CHECK(layout.visibleCapacity < 20U);
    CHECK(layout.rows.size() == layout.visibleCapacity);
}

TEST_CASE(review_collector_scroll_offset_is_clamped) {
    CHECK(next_review_scroll_offset(0U, 20U, 6U, 3) == 3U);
    CHECK(next_review_scroll_offset(13U, 20U, 6U, 4) == 14U);
    CHECK(next_review_scroll_offset(0U, 2U, 6U, -1) == 0U);
}
```

Register the new test source in `tests/CMakeLists.txt`.

- [ ] **Step 2: Run the unit target and verify RED**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset windows-debug --target diskbloom_tests
```

Expected: compilation fails because `review_collector_interaction.h` and its functions do not exist.

- [ ] **Step 3: Add the minimal layout model**

Define the production API with stable, allocation-bounded row geometry:

```cpp
struct ReviewRowLayout {
    AnalyzerRectF bounds;
    AnalyzerRectF nameBounds;
    AnalyzerRectF sizeBounds;
    std::size_t itemIndex = 0U;
};

struct ReviewCollectorLayout {
    AnalyzerRectF summary;
    AnalyzerRectF panel;
    std::vector<ReviewRowLayout> rows;
    std::size_t visibleCapacity = 0U;
    std::size_t scrollOffset = 0U;
};

[[nodiscard]] ReviewCollectorLayout compute_review_collector_layout(
    const AnalyzerRectF& actionBar,
    float widthDip,
    float heightDip,
    std::size_t itemCount,
    std::size_t scrollOffset);

[[nodiscard]] std::size_t next_review_scroll_offset(
    std::size_t current,
    std::size_t itemCount,
    std::size_t visibleCount,
    int deltaRows) noexcept;
```

Use a 20 DIP left inset, a panel width clamped between 360 and 620 DIP, 36 DIP rows, 12 DIP panel padding, and a panel top no higher than 64 DIP. Reserve only `visibleCapacity` rows.

- [ ] **Step 4: Run the unit target and verify GREEN**

Run the build command from Step 2, then:

```powershell
ctest --preset windows-debug -R diskbloom_tests --output-on-failure
```

Expected: `diskbloom_tests` passes.

- [ ] **Step 5: Write failing drag-state tests**

Append tests that specify the threshold and drop semantics:

```cpp
TEST_CASE(review_drag_activates_after_six_dip_and_drops_only_on_collector) {
    ReviewDragState drag;
    CHECK(drag.begin(7U, 100.0F, 100.0F));
    CHECK(!drag.move(105.0F, 100.0F, false));
    CHECK(!drag.active());
    CHECK(drag.move(106.0F, 100.0F, true));
    CHECK(drag.active());
    CHECK(drag.valid_drop());
    CHECK(drag.release(true) == std::optional<core::NodeIndex>{7U});
    CHECK(!drag.active());
}

TEST_CASE(review_drag_cancel_and_invalid_release_emit_no_node) {
    ReviewDragState drag;
    CHECK(!drag.begin(core::invalid_node, 0.0F, 0.0F));
    CHECK(drag.begin(9U, 0.0F, 0.0F));
    (void)drag.move(10.0F, 0.0F, false);
    CHECK(!drag.release(false).has_value());
    CHECK(!drag.active());
}
```

- [ ] **Step 6: Run tests and verify RED**

Run the Debug build from Step 2.

Expected: compilation fails because `ReviewDragState` does not exist.

- [ ] **Step 7: Implement the minimal allocation-free state machine**

Add this public shape and keep all movement arithmetic in `noexcept` methods:

```cpp
class ReviewDragState final {
public:
    [[nodiscard]] bool begin(core::NodeIndex node, float xDip, float yDip) noexcept;
    [[nodiscard]] bool move(float xDip, float yDip, bool overCollector) noexcept;
    [[nodiscard]] std::optional<core::NodeIndex> release(bool overCollector) noexcept;
    [[nodiscard]] bool cancel() noexcept;
    [[nodiscard]] bool pending() const noexcept;
    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] bool valid_drop() const noexcept;
    [[nodiscard]] core::NodeIndex node() const noexcept;
};
```

Activate at squared distance `>= 36.0F`. `release` returns a node only when active and currently over the collector, and always resets the state.

- [ ] **Step 8: Verify GREEN and commit**

Run `ctest --preset windows-debug -R diskbloom_tests --output-on-failure`.

Expected: PASS.

```powershell
git add src/app/analyzer_geometry.h src/app/analyzer_view.h src/app/review_collector_interaction.* src/CMakeLists.txt tests/review_collector_interaction_tests.cpp tests/CMakeLists.txt
git commit -m "feat: add collector layout and drag state"
```

---

### Task 2: Review Snapshot, Hover Panel, And Localization

**Files:**
- Modify: `src/app/analyzer_view.h`
- Modify: `src/app/analyzer_view.cpp`
- Modify: `src/core/language.h`
- Modify: `src/core/string_catalog.cpp`
- Modify: `tests/string_catalog_tests.cpp`
- Modify: `tests/analyzer_layout_tests.cpp`
- Modify: `tests/analyzer_render_smoke_main.cpp`
- Modify: `src/app/main_window.cpp`

**Interfaces:**
- Consumes: Task 1 `ReviewCollectorLayout`, the immutable `ScanTree`, and `DeletionReview::nodes()`.
- Produces: `AnalyzerView::set_review_nodes(std::span<const core::NodeIndex>)`, `AnalyzerView::scroll_review(int)`, and a hover panel drawn from node names and logical sizes.

- [ ] **Step 1: Write failing localization and review-snapshot tests**

Add `StringId::Collected` with expected catalog values and extend analyzer tests:

```cpp
CHECK(get_string(Language::English, StringId::Collected) == L"collected");
CHECK(get_string(Language::SimplifiedChinese, StringId::Collected)
    == L"\u5df2\u6536\u96c6");
```

Add analyzer assertions that the computed collector summary is inside the action bar and does not overlap the delete button.

- [ ] **Step 2: Run tests and verify RED**

Run the Debug unit target.

Expected: compilation fails because `StringId::Collected` and the collector layout fields are missing from the analyzer.

- [ ] **Step 3: Add review nodes and hover state to `AnalyzerView`**

Add the API and state:

```cpp
void set_review_nodes(std::span<const core::NodeIndex> nodes);
[[nodiscard]] bool scroll_review(int deltaRows) noexcept;

std::vector<core::NodeIndex> reviewNodes_;
ReviewCollectorLayout reviewLayout_{};
std::size_t reviewScrollOffset_ = 0U;
bool reviewPanelOpen_ = false;
```

`set_review_nodes` copies only node indices when the collection changes, derives count and total from the existing caller-provided summary, clamps scroll, and closes the panel when empty. Update every `MainWindow` review mutation and clear site to call both `set_review_summary(...)` and `set_review_nodes(deletionReview_.nodes())`.

- [ ] **Step 4: Draw the summary and hover panel using theme resources**

Extend `AnalyzerView::Resources` with a panel/drag brush using existing `ThemeTokens` instead of literal theme-specific colors. Reuse `rowNameFormat`, `rowSizeFormat`, and ellipsis trimming. Draw:

```cpp
for (const auto& row : reviewLayout_.rows) {
    const auto node = reviewNodes_[row.itemIndex];
    drawText(tree_->name(node), resources_->rowNameFormat.Get(),
             row.nameBounds, resources_->primary.Get());
    const auto size = format_bytes(tree_->node(node).logicalSize);
    drawText(size, resources_->rowSizeFormat.Get(),
             row.sizeBounds, resources_->secondary.Get());
}
```

Open the panel only when the review is non-empty and the pointer is within the summary or already-open panel. Keep it open while moving between those bounds. Draw the panel after the sunburst and before the drag preview so it overlays the chart without changing layout.

- [ ] **Step 5: Run unit and analyzer render tests and verify GREEN**

Run:

```powershell
ctest --preset windows-debug -R "diskbloom_tests|diskbloom_analyzer_render_smoke" --output-on-failure
```

Expected: both targets pass.

- [ ] **Step 6: Commit the hover panel**

```powershell
git add src/app/analyzer_view.* src/app/main_window.cpp src/core/language.h src/core/string_catalog.cpp tests/string_catalog_tests.cpp tests/analyzer_layout_tests.cpp tests/analyzer_render_smoke_main.cpp
git commit -m "feat: show reviewed items on collector hover"
```

---

### Task 3: Chart And List Drag Integration

**Files:**
- Modify: `src/app/analyzer_view.h`
- Modify: `src/app/analyzer_view.cpp`
- Modify: `src/app/main_window.h`
- Modify: `src/app/main_window.cpp`
- Modify: `tests/analyzer_render_smoke_main.cpp`

**Interfaces:**
- Consumes: Task 1 `ReviewDragState`, existing chart/list hit tests, and `AnalyzerCommandKind::AddToReview`.
- Produces: `pointer_down`, `pointer_released`, `cancel_drag`, `drag_pending`, and `drag_active` on `AnalyzerView`; Win32 capture begins on a valid potential drag and ends on release/cancellation.

- [ ] **Step 1: Write failing analyzer drag-source checks in the real render fixture**

After the existing first successful 800x600 draw in `tests/analyzer_render_smoke_main.cpp`, use its real `ScanTree`, `GraphicsDevice`, and `AnalyzerView`. Compute the ranked row position with `compute_analyzer_layout` and `compute_analyzer_child_list_layout`, then add this check:

```cpp
const auto analyzerLayout = diskbloom::app::compute_analyzer_layout(800.0F, 600.0F, 2U);
const auto childLayout = diskbloom::app::compute_analyzer_child_list_layout(
    analyzerLayout.detailsBounds, 1U, 0U);
const auto& row = childLayout.rows.front().bounds;
const auto rowX = (row.left + row.right) * 0.5F;
const auto rowY = (row.top + row.bottom) * 0.5F;
const auto collector = diskbloom::app::compute_review_collector_layout(
    analyzerLayout.actionBar, 800.0F, 600.0F, 1U, 0U).summary;
const auto collectorX = (collector.left + collector.right) * 0.5F;
const auto collectorY = (collector.top + collector.bottom) * 0.5F;

analyzer.pointer_down(rowX, rowY);
if (analyzer.take_command().has_value()) {
    DestroyWindow(window);
    return 4;
}
(void)analyzer.pointer_moved(collectorX, collectorY);
analyzer.pointer_released(collectorX, collectorY);
const auto rowDrop = analyzer.take_command();
if (!rowDrop.has_value()
    || rowDrop->kind != diskbloom::app::AnalyzerCommandKind::AddToReview
    || rowDrop->node != folder) {
    DestroyWindow(window);
    return 5;
}
```

Add a chart-segment check by building the fixture's `SunburstLayout`, taking the midpoint angle of its first non-aggregate segment, and converting that polar point with `analyzerLayout.chartGeometry`. Add checks with distinct exit codes for a below-threshold click preserving navigation, an invalid drop emitting no command, the root not beginning a drag, and recycle-disabled state emitting no add command.

- [ ] **Step 2: Build and verify RED**

Run the Debug unit target.

Expected: compilation fails because the analyzer drag API does not exist.

- [ ] **Step 3: Add pointer lifecycle methods to `AnalyzerView`**

Add:

```cpp
void pointer_down(float xDip, float yDip);
void pointer_released(float xDip, float yDip);
[[nodiscard]] bool cancel_drag() noexcept;
[[nodiscard]] bool drag_pending() const noexcept;
[[nodiscard]] bool drag_active() const noexcept;
```

`pointer_down` first checks a ranked row, then a non-aggregate sunburst segment. It rejects `root_`, invalid nodes, and recycling state. `pointer_moved` advances `ReviewDragState` and sets valid-drop from `contains_point(reviewLayout_.summary, xDip, yDip)`. `pointer_released` emits `AddToReview` only for a successful drop; if no drag activated, it delegates to the existing click-command behavior.

- [ ] **Step 4: Draw the drag preview and valid-drop state**

When active, draw a fixed-size preview near the pointer using the current node name and logical size, clipped to client bounds. Fill the summary with the existing hover token on ordinary drag and a palette accent when it is a valid drop. Use no text allocation in `pointer_moved`; format the size only during draw.

- [ ] **Step 5: Wire Win32 down/up/capture/cancellation**

In `MainWindow::window_proc`:

```cpp
case WM_LBUTTONDOWN:
    if (navigation_.view == MainContentView::Analyzer) {
        analyzer_.pointer_down(xDip, yDip);
        if (analyzer_.drag_pending()) {
            SetCapture(window_);
        }
    }
    return 0;

case WM_LBUTTONUP:
    if (navigation_.view == MainContentView::Analyzer) {
        analyzer_.pointer_released(xDip, yDip);
        if (GetCapture() == window_) {
            ReleaseCapture();
        }
        dispatch_analyzer_command();
    }
    return 0;

case WM_CAPTURECHANGED:
    if (reinterpret_cast<HWND>(lParam) != window_ && analyzer_.cancel_drag()) {
        InvalidateRect(window_, nullptr, FALSE);
    }
    return 0;
```

Cancel drag state before replacing/clearing the completed scan and when leaving the analyzer. On `WM_MOUSELEAVE`, cancel the drag and release capture; `WM_CAPTURECHANGED` remains the fallback cleanup path when capture is lost for another reason.

- [ ] **Step 6: Route wheel input to the open review panel**

Add `AnalyzerView::scroll_at(xDip, yDip, deltaRows)`, returning true after routing to `scroll_review` when the pointer is over the open panel and otherwise to `scroll_children`. Convert `WM_MOUSEWHEEL` screen coordinates with `ScreenToClient` before DIP conversion.

- [ ] **Step 7: Run tests and verify GREEN**

Run:

```powershell
ctest --preset windows-debug -R "diskbloom_tests|diskbloom_analyzer_render_smoke" --output-on-failure
```

Expected: all selected targets pass with chart and list drag tests included.

- [ ] **Step 8: Commit drag integration**

```powershell
git add src/app/analyzer_view.* src/app/main_window.* tests/analyzer_render_smoke_main.cpp
git commit -m "feat: drag analyzer items into deletion review"
```

---

### Task 4: Performance, Visual QA, And Full Regression

**Files:**
- Create: `benchmarks/review_collector_benchmark.cpp`
- Modify: `benchmarks/CMakeLists.txt`
- Modify: `docs/qa/sunburst-analyzer.md`

**Interfaces:**
- Consumes: Task 1 pure layout, point containment, scrolling, and drag-state APIs.
- Produces: a repeatable one-million-iteration Release benchmark and recorded same-host result.

- [ ] **Step 1: Add the Release microbenchmark**

Create a deterministic benchmark that runs one million iterations of drag movement plus collector/panel point containment, accumulates results into a printed checksum, and prints total milliseconds and operations per second. Register `diskbloom_review_collector_benchmark` linked to `diskbloom_app`, `diskbloom_core`, and `diskbloom_options`.

- [ ] **Step 2: Build and run Debug and Release suites**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset windows-debug
ctest --preset windows-debug --output-on-failure
& 'D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset windows-release
ctest --preset windows-release --output-on-failure
```

Expected: all seven CTest targets pass in both configurations.

- [ ] **Step 3: Record three Release benchmark samples**

Run three times:

```powershell
build\windows-release\benchmarks\diskbloom_review_collector_benchmark.exe 1000000
```

Record raw samples, median, checksum, host/compiler details, and the no-filesystem-work claim in `docs/qa/sunburst-analyzer.md`. Treat the median as the baseline for future same-host regressions.

- [ ] **Step 4: Perform visual and interaction verification**

Launch the real app in each combination:

```powershell
build\windows-release\src\DiskBloom.exe --theme=dark --language=en-US
build\windows-release\src\DiskBloom.exe --theme=dark --language=zh-CN
build\windows-release\src\DiskBloom.exe --theme=light --language=en-US
build\windows-release\src\DiskBloom.exe --theme=light --language=zh-CN
```

For each combination verify: empty collector, populated summary, hover panel, long-list scrolling, chart drag, ranked-row drag, invalid drop, duplicate drop, valid-drop highlight, recycling-disabled state, text clipping, and unchanged confirmation/recycle flow. Compare panel position, hierarchy, and collected summary with the supplied reference image.

- [ ] **Step 5: Check diff quality and commit QA evidence**

Run:

```powershell
git diff --check
git status --short
```

Expected: no whitespace errors and only intended files changed.

```powershell
git add benchmarks/review_collector_benchmark.cpp benchmarks/CMakeLists.txt docs/qa/sunburst-analyzer.md
git commit -m "perf: benchmark collector interactions"
```
