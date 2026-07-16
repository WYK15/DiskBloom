# Breadcrumb Navigation And Sunburst Transitions Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a complete DaisyDisk-style clickable path bar with bounded history and animate directory navigation through 700 ms per-segment polar transitions.

**Architecture:** Pure breadcrumb/path layout and navigation history remain in focused app modules; a render-independent core transition module matches and interpolates bounded sunburst segments. `AnalyzerView` renders breadcrumbs and at most 24 transition geometry batches (12 source plus 12 destination), while `MainWindow` owns Win32 timers, history commands, tooltip/menu coordination, and system animation policy.

**Tech Stack:** C++20, immutable contiguous `ScanTree`, Win32, Direct2D/DirectWrite, D3D11 staging readback for QA, CMake/CTest, MSVC Release benchmarks.

## Global Constraints

- Start implementation from the completed review-collector feature branch; preserve all collector hover, drag, confirmation, and recycle-only behavior.
- Every visual state supports light/dark themes and runtime en-US/zh-CN switching.
- The breadcrumb and transition should reproduce the supplied DaisyDisk recording as closely as practical while retaining DiskBloom branding.
- Breadcrumb construction occurs only when the scan/current root changes; pointer and draw hot paths perform no filesystem calls or path reconstruction.
- Transition work is bounded to source/destination layouts of at most 2,048 segments and at most 24 color batches; static rendering remains at 12 batches.
- Per-frame transition code reuses reserved contiguous buffers and performs no per-segment heap allocation.
- Navigation never targets files, stale nodes, or unscanned folder-prefix components.
- Windows `SPI_GETCLIENTAREAANIMATION == FALSE` disables the transition.
- The existing confirmation and recycle-only deletion policy must remain unchanged.
- Every behavior is implemented test-first with an observed, relevant RED failure.

---

### Task 1: Breadcrumb Path Model And Responsive Layout

**Files:**
- Create: `src/app/analyzer_breadcrumb.h`
- Create: `src/app/analyzer_breadcrumb.cpp`
- Modify: `src/CMakeLists.txt`
- Create: `tests/analyzer_breadcrumb_tests.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: immutable `core::ScanTree`, normalized `ScanResult::rootPath`, scan-root node, current-root node, measured label widths, and available header bounds.
- Produces: `build_analyzer_breadcrumb`, `layout_analyzer_breadcrumb`, `hit_test_analyzer_breadcrumb`, and hidden-item menu data without filesystem access.

- [ ] **Step 1: Write failing path-construction tests**

Create `tests/analyzer_breadcrumb_tests.cpp` with real tree fixtures:

```cpp
TEST_CASE(breadcrumb_drive_scan_maps_every_tree_ancestor_to_clickable_nodes) {
    ScanTree tree;
    const auto drive = tree.add_root(L"C:\\", ScanNodeFlags::Directory);
    const auto users = tree.add_child(drive, L"Users", 0U, ScanNodeFlags::Directory);
    const auto leo = tree.add_child(users, L"leo", 0U, ScanNodeFlags::Directory);
    tree.aggregate();

    const auto model = build_analyzer_breadcrumb(tree, drive, leo, L"C:\\");
    CHECK(model.items.size() == 4U);
    CHECK(model.items[0].kind == BreadcrumbItemKind::Overview);
    CHECK(model.items[1].node == drive);
    CHECK(model.items[2].node == users);
    CHECK(model.items[3].node == leo);
    CHECK(model.items[1].enabled && model.items[2].enabled && model.items[3].enabled);
}

TEST_CASE(breadcrumb_folder_scan_disables_unscanned_absolute_prefix) {
    ScanTree tree;
    const auto root = tree.add_root(L"project", ScanNodeFlags::Directory);
    const auto src = tree.add_child(root, L"src", 0U, ScanNodeFlags::Directory);
    tree.aggregate();

    const auto model = build_analyzer_breadcrumb(
        tree, root, src, L"D:\\work\\project");
    CHECK(model.items[1].label == L"D:\\");
    CHECK(!model.items[1].enabled);
    CHECK(model.items[2].label == L"work");
    CHECK(!model.items[2].enabled);
    CHECK(model.items[3].node == root && model.items[3].enabled);
    CHECK(model.items[4].node == src && model.items[4].enabled);
}
```

Add invalid-node, Unicode component, drive-root, trailing-separator, and duplicate scan-root-name cases. Register the test source.

- [ ] **Step 2: Build and verify RED**

Run from a Visual Studio x64 developer environment:

```powershell
cmake --build --preset windows-debug --target diskbloom_tests
```

Expected: compilation fails because `app/analyzer_breadcrumb.h` does not exist.

- [ ] **Step 3: Implement the minimal path model**

Define:

```cpp
enum class BreadcrumbItemKind : std::uint8_t {
    Overview,
    UnscannedPrefix,
    ScanNode,
    Ellipsis,
};

struct BreadcrumbItem {
    BreadcrumbItemKind kind = BreadcrumbItemKind::Overview;
    core::NodeIndex node = core::invalid_node;
    std::wstring label;
    std::wstring absolutePath;
    bool enabled = false;
};

struct AnalyzerBreadcrumbModel {
    std::vector<BreadcrumbItem> items;
};

[[nodiscard]] AnalyzerBreadcrumbModel build_analyzer_breadcrumb(
    const core::ScanTree& tree,
    core::NodeIndex scanRoot,
    core::NodeIndex currentRoot,
    std::wstring_view absoluteScanRoot);
```

Use `std::filesystem::path` lexical component iteration for the normalized scan root and the existing parent chain for scanned descendants. Build absolute display paths once per model construction. Do not call `exists`, `status`, canonicalization, or another filesystem query.

- [ ] **Step 4: Run unit tests and verify GREEN**

```powershell
ctest --preset windows-debug -R diskbloom_tests --output-on-failure
```

Expected: PASS.

- [ ] **Step 5: Write failing responsive-layout and hit tests**

Add measured-width tests for wide and compact headers:

```cpp
TEST_CASE(breadcrumb_compact_layout_keeps_overview_parent_current_and_ellipsis) {
    const auto layout = layout_analyzer_breadcrumb(
        model,
        std::array{120.0F, 72.0F, 96.0F, 110.0F, 84.0F, 92.0F},
        AnalyzerRectF{112.0F, 8.0F, 650.0F, 56.0F});
    CHECK(layout.visible.front().itemIndex == 0U);
    CHECK(layout.visible.back().itemIndex == model.items.size() - 1U);
    CHECK(layout.visible[layout.visible.size() - 2U].itemIndex == model.items.size() - 2U);
    CHECK(layout.ellipsis.has_value());
    CHECK(!layout.hiddenItemIndices.empty());
}
```

Assert all bounds are inside the available header, do not overlap, preserve path order, and hit-test half-open edges. Include 800x600 and 1200x720 width budgets using both English and Chinese overview-label measurements.

- [ ] **Step 6: Implement layout, overflow, and hit testing**

Define value types:

```cpp
struct BreadcrumbSegmentLayout {
    AnalyzerRectF bounds;
    std::size_t itemIndex = 0U;
};

struct AnalyzerBreadcrumbLayout {
    std::vector<BreadcrumbSegmentLayout> visible;
    std::optional<BreadcrumbSegmentLayout> ellipsis;
    std::vector<std::size_t> hiddenItemIndices;
};
```

Use 14 DIP left/right text padding plus a 10 DIP chevron cap. Collapse middle items only after measured full layout fails. If even the retained set is too wide, trim the nearest parent first and finally trim individual labels through DirectWrite; never overlap the window controls.

- [ ] **Step 7: Verify and commit Task 1**

Run focused unit tests and the full Debug CTest suite.

```powershell
git add src/app/analyzer_breadcrumb.* src/CMakeLists.txt tests/analyzer_breadcrumb_tests.cpp tests/CMakeLists.txt
git commit -m "feat: add responsive analyzer breadcrumb model"
```

---

### Task 2: Bounded Back And Forward History

**Files:**
- Create: `src/app/analyzer_history.h`
- Create: `src/app/analyzer_history.cpp`
- Modify: `src/app/analyzer_navigation.h`
- Modify: `src/app/analyzer_navigation.cpp`
- Modify: `src/app/analyzer_view.h`
- Modify: `src/CMakeLists.txt`
- Create: `tests/analyzer_history_tests.cpp`
- Modify: `tests/analyzer_navigation_tests.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: validated directory nodes and overview/analyzer navigation entries.
- Produces: a bounded 128-entry `AnalyzerHistory` with record/back/forward/reset and command kinds `NavigateBack`, `NavigateForward`, and `NavigateBreadcrumb`.

- [ ] **Step 1: Write failing history tests**

```cpp
TEST_CASE(analyzer_history_supports_back_forward_and_clears_forward_on_branch) {
    AnalyzerHistory history;
    history.reset({MainContentView::Overview, core::invalid_node});
    CHECK(history.record({MainContentView::Analyzer, 0U}));
    CHECK(history.record({MainContentView::Analyzer, 1U}));
    CHECK(history.back() == (NavigationEntry{MainContentView::Analyzer, 0U}));
    CHECK(history.forward() == (NavigationEntry{MainContentView::Analyzer, 1U}));
    (void)history.back();
    CHECK(history.record({MainContentView::Analyzer, 2U}));
    CHECK(!history.can_forward());
}
```

Add duplicate suppression, 128-entry eviction, reset, overview entry, stale-node validation, and scan replacement tests.

- [ ] **Step 2: Verify RED**

Build `diskbloom_tests`; expect missing `AnalyzerHistory` compilation errors.

- [ ] **Step 3: Implement bounded contiguous history**

Move the existing `MainContentView` enum from `analyzer_navigation.h` into `analyzer_history.h` so history and navigation share it without a circular include. Define:

```cpp
struct NavigationEntry {
    MainContentView view = MainContentView::Overview;
    core::NodeIndex node = core::invalid_node;
    friend bool operator==(const NavigationEntry&, const NavigationEntry&) = default;
};

class AnalyzerHistory final {
public:
    static constexpr std::size_t capacity = 128U;
    void reset(NavigationEntry entry);
    [[nodiscard]] bool record(NavigationEntry entry);
    [[nodiscard]] std::optional<NavigationEntry> back() noexcept;
    [[nodiscard]] std::optional<NavigationEntry> forward() noexcept;
    [[nodiscard]] bool can_back() const noexcept;
    [[nodiscard]] bool can_forward() const noexcept;
};
```

Use one reserved vector and a cursor. Erase the oldest entry only at capacity. Do not allocate in back/forward.

- [ ] **Step 4: Integrate command semantics test-first**

Extend `AnalyzerCommandKind` with `NavigateBack`, `NavigateForward`, `NavigateBreadcrumb`, and `OpenBreadcrumbOverflow`. Add tests proving files, invalid nodes, disabled prefix items, and stale history entries do not change `AnalyzerNavigationState`.

Add `AnalyzerHistory history;` to `AnalyzerNavigationState`. `MainWindow`, not `AnalyzerView`, records successful root changes through that state. Back/forward retrieve an entry, validate it against the active tree, and discard invalid entries until a valid entry or history end is reached.

- [ ] **Step 5: Verify and commit Task 2**

Run unit and full Debug tests.

```powershell
git add src/app/analyzer_history.* src/app/analyzer_navigation.* src/app/analyzer_view.h src/CMakeLists.txt tests/analyzer_history_tests.cpp tests/analyzer_navigation_tests.cpp tests/CMakeLists.txt
git commit -m "feat: add bounded analyzer navigation history"
```

---

### Task 3: Breadcrumb Header Rendering And Interaction

**Files:**
- Modify: `src/app/analyzer_view.h`
- Modify: `src/app/analyzer_view.cpp`
- Modify: `src/app/main_window.h`
- Modify: `src/app/main_window.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `src/core/language.h`
- Modify: `src/core/string_catalog.cpp`
- Modify: `tests/string_catalog_tests.cpp`
- Modify: `tests/analyzer_layout_tests.cpp`
- Modify: `tests/analyzer_render_smoke_main.cpp`

**Interfaces:**
- Consumes: Tasks 1-2 breadcrumb model/history state, DirectWrite measurements, existing theme resources, and current scan root path.
- Produces: rendered chevron segments, hover/pressed/current/disabled/focus states, overflow flyout, tooltip text, and analyzer commands for breadcrumbs/history.

- [ ] **Step 1: Write failing localization and header-layout tests**

Add `StringId::DisksAndFolders` with:

```cpp
CHECK(get_string(Language::English, StringId::DisksAndFolders)
    == L"Disks and Folders");
CHECK(get_string(Language::SimplifiedChinese, StringId::DisksAndFolders)
    == L"\u78c1\u76d8\u548c\u6587\u4ef6\u5939");
```

Extend analyzer layout tests so back, forward, breadcrumb region, and window controls fit without overlap at 800x600 and 1200x720.

- [ ] **Step 2: Verify RED**

Build unit/render targets; expect missing string/header fields.

- [ ] **Step 3: Replace the centered title with breadcrumb state**

Add to `AnalyzerLayout`:

```cpp
AnalyzerRectF backButton;
AnalyzerRectF forwardButton;
AnalyzerRectF breadcrumbBounds;
```

Add to `AnalyzerView`:

```cpp
void set_breadcrumb(AnalyzerBreadcrumbModel model);
void set_history_availability(bool canBack, bool canForward) noexcept;
[[nodiscard]] std::wstring_view hovered_breadcrumb_path() const noexcept;
```

Measure labels only when the model, language, DPI, or available header width changes. Cache measured widths and the responsive layout. Draw chevron polygons with existing surface/hover/border/text theme brushes. Current uses the active palette accent; disabled uses secondary text.

- [ ] **Step 4: Implement hit testing and overflow flyout**

Breadcrumb pointer hits take precedence over chart/list hits. Clicking:

- Back/forward emits the corresponding command only when enabled.
- Overview emits `ReturnToOverview`.
- Enabled scan nodes emit `NavigateBreadcrumb` with the node.
- Disabled prefix items emit nothing.
- Ellipsis toggles an in-app Direct2D flyout containing hidden items in path order.

Use one flyout layout, mathematical hit testing, and no child window per item. Close it on outside click, Escape, navigation, scan replacement, recycling, or pointer leave from both breadcrumb/flyout bounds.

- [ ] **Step 5: Add one shared native tooltip**

`MainWindow` creates one `TOOLTIPS_CLASSW` window with a single trackable tool. On breadcrumb hover changes, update its rectangle and text to `hovered_breadcrumb_path()`. It must be disabled for empty text and destroyed with the main window. Do not create a tooltip/control per segment.

Initialize common controls once and link `diskbloom_app` to `comctl32` in `src/CMakeLists.txt`.

- [ ] **Step 6: Synchronize model and history**

After a completed scan enters the analyzer and after every successful root/history change, rebuild the breadcrumb from `completedScan_->tree`, tree root, current root, and `completedScan_->rootPath`. Update history button availability. Clear model/history on scan replacement, recycle rescan, or overview return.

- [ ] **Step 7: Render-smoke all visual states**

In `tests/analyzer_render_smoke_main.cpp`, render wide/full and compact/collapsed breadcrumbs, flyout open, hover/current/disabled states, long Unicode names, and enabled/disabled history in all four theme/language combinations. Assert commands from each hit target and no command from disabled prefix items.

- [ ] **Step 8: Verify and commit Task 3**

Run focused unit/render targets and full Debug CTest.

```powershell
git add src/app/analyzer_view.* src/app/main_window.* src/CMakeLists.txt src/core/language.h src/core/string_catalog.cpp tests/string_catalog_tests.cpp tests/analyzer_layout_tests.cpp tests/analyzer_render_smoke_main.cpp
git commit -m "feat: render clickable full-path breadcrumbs"
```

---

### Task 4: Render-Independent Polar Transition Core

**Files:**
- Create: `src/core/sunburst_transition.h`
- Create: `src/core/sunburst_transition.cpp`
- Modify: `src/CMakeLists.txt`
- Create: `tests/sunburst_transition_tests.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: source/destination `SunburstLayout`, source/destination `SunburstGeometry`, anchor node, direction, and normalized progress.
- Produces: a bounded `SunburstTransitionPlan` and reusable `std::span<const TransitionDrawSegment>` with exact endpoints.

- [ ] **Step 1: Write failing endpoint and easing tests**

```cpp
TEST_CASE(sunburst_transition_matches_source_and_destination_endpoints) {
    const auto plan = build_sunburst_transition(source, sourceGeometry,
                                                destination, destinationGeometry,
                                                anchorNode);
    SunburstTransitionFrame frame;
    interpolate_sunburst_transition(plan, 0.0F, frame);
    CHECK(frame_matches_layout(frame, source, sourceGeometry));
    interpolate_sunburst_transition(plan, 1.0F, frame);
    CHECK(frame_matches_layout(frame, destination, destinationGeometry));
}

TEST_CASE(sunburst_transition_uses_monotonic_cubic_ease_out) {
    CHECK(sunburst_transition_easing(0.0F) == 0.0F);
    CHECK(sunburst_transition_easing(1.0F) == 1.0F);
    CHECK(sunburst_transition_easing(0.5F) == doctest::Approx(0.875F));
}
```

Add matched node, unmatched fade/collapse, aggregate identity, enter, return, multi-level jump, and 2,048-segment bound cases.

- [ ] **Step 2: Verify RED**

Build unit tests; expect missing transition API.

- [ ] **Step 3: Implement stable matching and bounded morph records**

Define:

```cpp
struct PolarSegmentState {
    NodeIndex node = invalid_node;
    float startAngle = 0.0F;
    float endAngle = 0.0F;
    float innerRadius = 0.0F;
    float outerRadius = 0.0F;
    float opacity = 0.0F;
    std::uint8_t paletteIndex = 0U;
    bool aggregate = false;
};

struct SegmentMorph {
    std::optional<PolarSegmentState> source;
    std::optional<PolarSegmentState> destination;
};

struct SunburstTransitionPlan {
    std::vector<SegmentMorph> morphs;
};

struct TransitionDrawSegment {
    NodeIndex node = invalid_node;
    float startAngle = 0.0F;
    float endAngle = 0.0F;
    float innerRadius = 0.0F;
    float outerRadius = 0.0F;
    float sourceOpacity = 0.0F;
    float destinationOpacity = 0.0F;
    std::uint8_t sourcePaletteIndex = 0U;
    std::uint8_t destinationPaletteIndex = 0U;
};

class SunburstTransitionFrame final {
public:
    void reserve(std::size_t count);
    [[nodiscard]] std::span<const TransitionDrawSegment> segments() const noexcept;
};
```

Match non-aggregate records by node. Match aggregate records by aggregate flag, node/parent identity exposed by the layout, and depth. Pair remaining source/destination records so `morphs.size() == max(source.size(), destination.size()) <= 2'048`. Missing endpoints collapse to the anchor angle/radius with zero opacity. Every morph contributes its interpolated geometry to one of 12 source-palette batches at source opacity and one of 12 destination-palette batches at destination opacity. This preserves the 2,048 morph-record and 24-batch bounds while cross-fading palette changes.

- [ ] **Step 4: Add interruption snapshots test-first**

Add `snapshot_transition_frame(frame)` and prove a second plan begins exactly at the current interpolated geometry with no discontinuity. Add cancellation and invalid-anchor fallback tests.

- [ ] **Step 5: Verify and commit Task 4**

Run unit/full Debug suites.

```powershell
git add src/core/sunburst_transition.* src/CMakeLists.txt tests/sunburst_transition_tests.cpp tests/CMakeLists.txt
git commit -m "feat: interpolate bounded sunburst transitions"
```

---

### Task 5: Animated Rendering, Input Suppression, And Win32 Scheduling

**Files:**
- Create: `src/app/analyzer_transition_controller.h`
- Create: `src/app/analyzer_transition_controller.cpp`
- Modify: `src/app/analyzer_view.h`
- Modify: `src/app/analyzer_view.cpp`
- Modify: `src/app/analyzer_input_lifecycle.h`
- Modify: `src/app/analyzer_input_lifecycle.cpp`
- Modify: `src/app/main_window.h`
- Modify: `src/app/main_window.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `src/platform/windows/system_theme.h`
- Modify: `src/platform/windows/system_theme.cpp`
- Modify: `tests/analyzer_input_lifecycle_tests.cpp`
- Create: `tests/analyzer_transition_controller_tests.cpp`
- Create: `tests/system_animation_tests.cpp`
- Modify: `tests/analyzer_render_smoke_main.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 4 transition plans/frames, 700 ms duration, system client-animation flag, navigation commands, and existing D2D batch resources.
- Produces: `AnalyzerTransitionController`, `AnalyzerView::navigate_to_root`, `advance_transition`, `transition_active`, and exact destination rendering after completion.

- [ ] **Step 1: Write failing transition-lifecycle tests**

Use a fake monotonic timestamp passed explicitly to the view/controller:

```cpp
TEST_CASE(analyzer_transition_completes_at_700_milliseconds) {
    const auto start = std::chrono::steady_clock::time_point{};
    controller.start(start, true);
    CHECK(controller.active());
    CHECK(controller.advance(start + std::chrono::milliseconds{350}).linearProgress
        == doctest::Approx(0.5F));
    CHECK(!controller.advance(start + std::chrono::milliseconds{700}).active);
}
```

Add zero-duration reduced-motion, interruption, tree replacement, overview, recycling, and geometry-allocation failure fallback tests.

- [ ] **Step 2: Verify RED**

Build focused tests; expect missing controller/view APIs.

- [ ] **Step 3: Add system animation policy**

Implement:

```cpp
[[nodiscard]] bool client_area_animations_enabled() noexcept;
```

Call `SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0)`. On API failure, return true. Make the policy injectable into the controller test rather than mocking Win32.

- [ ] **Step 4: Implement the deterministic transition controller**

Define the API used by the Step 1 tests:

```cpp
struct AnalyzerTransitionAdvance {
    float linearProgress = 1.0F;
    float easedProgress = 1.0F;
    bool active = false;
};

class AnalyzerTransitionController final {
public:
    static constexpr auto duration = std::chrono::milliseconds{700};
    void start(std::chrono::steady_clock::time_point now, bool animationsEnabled) noexcept;
    [[nodiscard]] AnalyzerTransitionAdvance advance(
        std::chrono::steady_clock::time_point now) noexcept;
    [[nodiscard]] bool cancel() noexcept;
    [[nodiscard]] bool active() const noexcept;
};
```

Store only the start time and active flag. Clamp elapsed time to `[0, 700 ms]`, derive linear progress from elapsed duration, and apply `sunburst_transition_easing` for eased progress. Reduced motion returns the completed state immediately.

- [ ] **Step 5: Integrate transition state into `AnalyzerView`**

Replace abrupt `set_root` use for node-to-node navigation with:

```cpp
[[nodiscard]] bool navigate_to_root(
    core::NodeIndex root,
    std::chrono::steady_clock::time_point now,
    bool animationsEnabled);
[[nodiscard]] bool advance_transition(
    std::chrono::steady_clock::time_point now) noexcept;
[[nodiscard]] bool transition_active() const noexcept;
void cancel_transition() noexcept;
```

At navigation start, retain the current interpolated frame if interrupted, build destination layout/ranking once, and reserve transition/batch buffers. Update breadcrumb/root immediately. Cross-fade source/destination title and ranked rows using the specified 45/30-100 percent intervals.

- [ ] **Step 6: Draw transition geometry in at most 24 batches**

Reuse 12 source and 12 destination path geometries and geometry sinks per frame. Convert each interpolated polar segment into annular-sector arcs and append the same geometry to its source/destination palette batches. Apply the two global contribution opacities through per-palette brushes/layers without allocating per segment. If geometry creation fails, cancel transition and draw cached destination geometry.

- [ ] **Step 7: Suppress unsafe input while retaining navigation**

During transition, chart/list selection, drag start/drop, preview/reveal/add-review/delete-review, collector panel mutation, and child scrolling emit no command. Back, forward, breadcrumb, window buttons, and Escape remain active. Extend the existing input lifecycle decision boundary so scan replacement, overview, recycling, capture loss, and close cancel drag and transition together.

- [ ] **Step 8: Add the 16 ms animation timer**

Add `animation_timer_id` in `MainWindow`. Start it only while `transition_active()`. On each tick call `advance_transition(steady_clock::now())`, invalidate, and kill the timer immediately on completion/cancellation. Timer ticks never determine progress.

- [ ] **Step 9: Render-smoke start/mid/end and interruption**

Render deterministic frames at 0, 350, and 700 ms plus an interrupted transition in every theme/language combination. Assert start/destination endpoint checksums differ, midpoint is nonblank, final equals the static destination, breadcrumb changes at start, and disallowed input remains suppressed.

- [ ] **Step 10: Verify and commit Task 5**

Run focused tests, full Debug CTest, and a real visible navigation smoke.

```powershell
git add src/app/analyzer_transition_controller.* src/app/analyzer_view.* src/app/analyzer_input_lifecycle.* src/app/main_window.* src/platform/windows/system_theme.* src/CMakeLists.txt tests/analyzer_input_lifecycle_tests.cpp tests/analyzer_transition_controller_tests.cpp tests/system_animation_tests.cpp tests/analyzer_render_smoke_main.cpp tests/CMakeLists.txt
git commit -m "feat: animate analyzer directory transitions"
```

---

### Task 6: Transition Benchmark, Reliable Frame Capture, And Final QA

**Files:**
- Create: `benchmarks/sunburst_transition_benchmark.cpp`
- Modify: `benchmarks/CMakeLists.txt`
- Modify: `src/render/graphics_device.h`
- Modify: `src/render/graphics_device.cpp`
- Create: `tests/graphics_capture_tests.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/analyzer_render_smoke_main.cpp`
- Modify: `docs/qa/sunburst-analyzer.md`
- Create: selected PNG evidence under `docs/qa/evidence/breadcrumb-transition/`

**Interfaces:**
- Consumes: the 2,048-segment transition core and rendered D3D11 back buffer.
- Produces: a 60-frame Release benchmark with checksum/p95, reliable BGRA readback, and trustworthy visual evidence without desktop-capture black regions.

- [ ] **Step 1: Write failing GPU-readback tests**

Add a test that clears the render target to a known nonblack theme color, ends drawing, captures BGRA pixels, and verifies dimensions plus representative pixels. Use the wished-for API:

```cpp
struct CapturedFrame {
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::vector<std::byte> bgra;
};

[[nodiscard]] HRESULT GraphicsDevice::end_draw(CapturedFrame* capture) noexcept;
```

- [ ] **Step 2: Verify RED, then implement pre-present staging-texture readback**

After observing the missing overload failure, make the existing no-argument `end_draw()` delegate to `end_draw(nullptr)`. When a capture pointer is supplied, finish Direct2D drawing, copy the current swap-chain buffer to a reusable `D3D11_USAGE_STAGING` texture with `D3D11_CPU_ACCESS_READ`, map it, and copy row pitches into tightly packed BGRA storage before calling `Present`. A failed capture returns an error without presenting stale evidence. Readback is used only by QA call sites and never occurs in the normal render loop.

- [ ] **Step 3: Add the deterministic Release benchmark**

Build source/destination fixtures with exactly 2,048 segments, create one transition plan, reserve one frame, and time 60 interpolations plus batched-geometry preparation. Print per-frame samples, median, p95, memory bytes, segment count, and checksum.

Register a CTest invocation with an exact checksum regex. Reject wrong segment/frame counts, endpoint mismatch, nonfinite geometry, and checksum mismatch.

- [ ] **Step 4: Establish the performance gate**

Run three Release samples:

```powershell
build\windows-release\benchmarks\diskbloom_sunburst_transition_benchmark.exe 2048 60
```

Record raw samples and median p95 in `docs/qa/sunburst-analyzer.md`. On the recorded host/compiler, p95 interpolation plus geometry preparation must be `<= 8.0 ms`. Re-run existing scan-tree, static sunburst, and collector baselines to confirm their current gates.

- [ ] **Step 5: Capture trustworthy visual evidence**

Use `end_draw(&capturedFrame)` and a WIC PNG encoder in the analyzer render QA executable. Save only the app client area for:

- Full and collapsed breadcrumbs.
- Overflow flyout.
- Enter transition at 0, 350, and 700 ms.
- Return transition midpoint.
- Interrupted transition midpoint.
- Reduced-motion destination.
- Light/dark x en-US/zh-CN.

Inspect every PNG for nonblack/nonblank content, clipping, overlap, correct path order, disabled prefix contrast, current segment, and chart framing. Compare transition stages to reference frames near 43.9, 44.5, 44.8, and 45.1 seconds.

- [ ] **Step 6: Run complete regression and commit QA**

Run Debug and Release builds plus all CTest targets. Exercise real drive and selected-folder navigation, history, breadcrumb jumps, collector hover/drag, confirmation against a disposable directory, recycle/rescan, theme/language switching, window resize, DPI change, reduced animation, and rapid interrupted navigation.

```powershell
git diff --check
git add benchmarks/sunburst_transition_benchmark.cpp benchmarks/CMakeLists.txt src/render/graphics_device.* tests/graphics_capture_tests.cpp tests/CMakeLists.txt tests/analyzer_render_smoke_main.cpp docs/qa/sunburst-analyzer.md docs/qa/evidence/breadcrumb-transition
git commit -m "perf: verify breadcrumb transition rendering"
```
