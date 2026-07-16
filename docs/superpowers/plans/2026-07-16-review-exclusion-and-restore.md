# Review Exclusion And Restore Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Shrink the analyzer sunburst, exclude reviewed files and directories from its visible layout, and restore individual reviewed roots through a DaisyDisk-style cross interaction with interruptible reflow animation.

**Architecture:** Keep the completed `ScanTree` immutable and add a sparse core projection containing excluded roots and ancestor byte reductions. Projection-aware sunburst, ranking, navigation, and analyzer reflow consume that value; `MainWindow` remains the owner of `DeletionReview` and the current projection. Collector changes reuse the bounded transition renderer with a 500 ms duration while directory navigation remains 700 ms.

**Tech Stack:** C++20, contiguous/sorted native data, Direct2D/DirectWrite, Win32 timers and pointer input, immutable `ScanTree`, CMake/CTest, D3D11 BGRA capture.

## Global Constraints

- Every new visual and interaction supports dark/light themes and en-US/zh-CN.
- The completed `ScanTree` remains immutable; reviewed roots are visual exclusions only until confirmed recycling.
- The analyzer radius is reduced by exactly 9 percent after responsive constraints are applied.
- Collector reflow lasts 500 ms; directory navigation remains 700 ms.
- `Always on`, `Follow Windows`, and `Off` control collector animation exactly as they control directory transitions.
- Projection rebuilds perform no filesystem access and ordinary pointer movement never rebuilds a projection.
- Animation remains bounded to 2,048 sunburst segments and reuses reserved buffers during interruption.
- Scan roots cannot be collected. Collecting the current child directory targets its nearest visible parent.
- Recycling disables additions, restores, drags, collector scrolling, and destructive commands.
- Real deletion still requires localized confirmation and uses the Windows Recycle Bin only.
- Every behavioral change follows an observed RED test before implementation.

---

### Task 1: Sparse Scan-Tree Exclusion Projection

**Files:**
- Create: `src/core/scan_tree_exclusion.h`
- Create: `src/core/scan_tree_exclusion.cpp`
- Modify: `src/CMakeLists.txt`
- Create: `tests/scan_tree_exclusion_tests.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: immutable `core::ScanTree` and normalized reviewed root indices.
- Produces: `core::ScanTreeExclusion::rebuild`, `is_excluded_root`, `is_visible`, `effective_size`, `roots`, and `reductions`.

- [x] **Step 1: Write failing projection tests**

Create a tree with two directory branches, nested files, and non-overlapping reviewed roots. Add tests equivalent to:

```cpp
ScanTreeExclusion exclusion;
exclusion.rebuild(tree, std::array{firstFile, secondFolder});

CHECK(exclusion.is_excluded_root(firstFile));
CHECK(exclusion.is_excluded_root(secondFolder));
CHECK(!exclusion.is_visible(tree, firstFile));
CHECK(!exclusion.is_visible(tree, secondFolderChild));
CHECK(exclusion.is_visible(tree, firstFolder));
CHECK(exclusion.effective_size(tree, firstFolder)
    == tree.node(firstFolder).logicalSize - tree.node(firstFile).logicalSize);
CHECK(exclusion.effective_size(tree, root)
    == tree.node(root).logicalSize
        - tree.node(firstFile).logicalSize
        - tree.node(secondFolder).logicalSize);
```

Also assert empty projection identity, invalid roots being ignored, deterministic sorted roots/reductions, duplicate roots being counted once, an ancestor suppressing a supplied descendant, and saturating subtraction for inconsistent synthetic nodes.

- [x] **Step 2: Build and verify RED**

Run in the Visual Studio x64 developer environment:

```powershell
cmake --build --preset windows-debug --target diskbloom_tests
```

Expected: compilation fails because `core/scan_tree_exclusion.h` does not exist.

- [x] **Step 3: Implement the sparse value type**

Declare the stable public surface:

```cpp
struct ExcludedByteReduction {
    NodeIndex node = invalid_node;
    std::uint64_t bytes = 0U;
};

class ScanTreeExclusion final {
public:
    void rebuild(const ScanTree& tree, std::span<const NodeIndex> roots);
    void clear() noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] bool is_excluded_root(NodeIndex node) const noexcept;
    [[nodiscard]] bool is_visible(const ScanTree& tree, NodeIndex node) const noexcept;
    [[nodiscard]] std::uint64_t effective_size(
        const ScanTree& tree,
        NodeIndex node) const noexcept;
    [[nodiscard]] std::span<const NodeIndex> roots() const noexcept;
    [[nodiscard]] std::span<const ExcludedByteReduction> reductions() const noexcept;
private:
    std::vector<NodeIndex> roots_;
    std::vector<ExcludedByteReduction> reductions_;
};
```

Normalize roots by sorting, uniquing, rejecting invalid/root node `0`, and removing any root whose ancestor is already present. Build reductions by walking each remaining root's parent chain, sorting by node, and merging with saturating addition. Use binary search for root and reduction queries; `is_visible` walks parents until the scan root.

- [x] **Step 4: Run focused and full unit tests**

```powershell
cmake --build --preset windows-debug --target diskbloom_tests
build/windows-debug/tests/diskbloom_tests.exe
```

Expected: all unit cases pass with no new warnings.

- [x] **Step 5: Commit the projection**

```powershell
git add src/core/scan_tree_exclusion.h src/core/scan_tree_exclusion.cpp src/CMakeLists.txt tests/scan_tree_exclusion_tests.cpp tests/CMakeLists.txt
git commit -m "feat: model reviewed tree exclusions"
```

---

### Task 2: Projection-Aware Sunburst And Child Ranking

**Files:**
- Modify: `src/core/sunburst_layout.h`
- Modify: `src/core/sunburst_layout.cpp`
- Modify: `src/core/child_ranking.h`
- Modify: `src/core/child_ranking.cpp`
- Modify: `tests/sunburst_layout_tests.cpp`
- Modify: `tests/child_ranking_tests.cpp`

**Interfaces:**
- Consumes: Task 1 `core::ScanTreeExclusion`.
- Produces: optional exclusion parameters on `build_sunburst_layout` and `rank_children` while preserving existing unfiltered calls.

- [x] **Step 1: Write failing filtered-layout tests**

Add cases using the Task 1 fixture:

```cpp
const auto layout = build_sunburst_layout(tree, root, {}, &exclusion);
CHECK(std::ranges::none_of(layout.segments, [excludedFolder](const auto& segment) {
    return segment.node == excludedFolder;
}));
CHECK(std::ranges::none_of(layout.segments, [excludedChild](const auto& segment) {
    return segment.node == excludedChild;
}));
CHECK(layout.segments.front().logicalSize
    == exclusion.effective_size(tree, layout.segments.front().node));

const auto ranked = rank_children(tree, root, 24U, &exclusion);
CHECK(std::ranges::none_of(ranked, [excludedFolder](const auto& item) {
    return item.node == excludedFolder;
}));
CHECK(ranked.front().logicalSize
    == exclusion.effective_size(tree, ranked.front().node));
```

Cover a visible directory with one excluded descendant, all children excluded, aggregate segments, deterministic ties after effective-size changes, and `nullptr` preserving current output exactly.

- [x] **Step 2: Build and verify RED**

```powershell
cmake --build --preset windows-debug --target diskbloom_tests
```

Expected: compilation fails because the overloads do not accept an exclusion.

- [x] **Step 3: Add optional projection parameters**

Use these signatures:

```cpp
[[nodiscard]] SunburstLayout build_sunburst_layout(
    const ScanTree& tree,
    NodeIndex root,
    const SunburstLayoutOptions& options,
    const ScanTreeExclusion* exclusion = nullptr);

[[nodiscard]] std::vector<RankedChild> rank_children(
    const ScanTree& tree,
    NodeIndex parent,
    std::size_t limit,
    const ScanTreeExclusion* exclusion = nullptr);
```

Skip excluded roots before traversal or heap insertion. Route every size comparison, sweep calculation, aggregate byte count, and emitted `logicalSize` through a local effective-size helper. A root with zero effective bytes returns no segments without fabricating an aggregate node.

- [x] **Step 4: Verify filtered and legacy behavior**

```powershell
cmake --build --preset windows-debug --target diskbloom_tests
build/windows-debug/tests/diskbloom_tests.exe
```

Expected: all projection, layout, ranking, hit-test, and existing deterministic layout cases pass.

- [x] **Step 5: Commit filtered layout support**

```powershell
git add src/core/sunburst_layout.h src/core/sunburst_layout.cpp src/core/child_ranking.h src/core/child_ranking.cpp tests/sunburst_layout_tests.cpp tests/child_ranking_tests.cpp
git commit -m "feat: filter reviewed nodes from analyzer layouts"
```

---

### Task 3: Visibility-Aware Navigation State

**Files:**
- Modify: `src/app/analyzer_history.h`
- Modify: `src/app/analyzer_history.cpp`
- Modify: `src/app/analyzer_navigation.h`
- Modify: `src/app/analyzer_navigation.cpp`
- Modify: `tests/analyzer_history_tests.cpp`
- Modify: `tests/analyzer_navigation_tests.cpp`

**Interfaces:**
- Consumes: `core::ScanTreeExclusion` plus the existing navigation state/history.
- Produces: `nearest_visible_directory`, `reconcile_analyzer_visibility`, visibility-aware `apply_analyzer_command`, and accurate visible back/forward availability.

- [x] **Step 1: Write failing navigation tests**

Add tests proving:

```cpp
CHECK(nearest_visible_directory(tree, exclusion, hiddenFolder) == visibleParent);
CHECK(reconcile_analyzer_visibility(state, tree, exclusion));
CHECK(state.root == visibleParent);
CHECK(state.selected == visibleParent);

CHECK(apply_analyzer_command(
    state, tree, {AnalyzerCommandKind::NavigateBack}, &exclusion));
CHECK(state.root != hiddenFolder);
CHECK(!apply_analyzer_command(
    state, tree, {AnalyzerCommandKind::NavigateBreadcrumb, hiddenFolder}, &exclusion));
```

Keep the hidden history entry in the history vector, clear the exclusion, then assert forward/back can reach it again. Add a selected-descendant fallback case and visible `can_back`/`can_forward` checks that ignore only currently hidden analyzer destinations.

- [x] **Step 2: Build and verify RED**

Expected: compilation fails on the missing visibility-aware functions and parameters.

- [x] **Step 3: Implement dynamic history skipping**

Add non-mutating indexed access to history:

```cpp
[[nodiscard]] std::span<const NavigationEntry> entries() const noexcept;
[[nodiscard]] std::size_t cursor() const noexcept;
```

Extend navigation APIs:

```cpp
[[nodiscard]] NodeIndex nearest_visible_directory(
    const ScanTree& tree,
    const ScanTreeExclusion& exclusion,
    NodeIndex node) noexcept;
[[nodiscard]] bool reconcile_analyzer_visibility(
    AnalyzerNavigationState& state,
    const ScanTree& tree,
    const ScanTreeExclusion& exclusion) noexcept;
[[nodiscard]] bool navigation_can_back(
    const AnalyzerNavigationState& state,
    const ScanTree& tree,
    const ScanTreeExclusion* exclusion) noexcept;
[[nodiscard]] bool navigation_can_forward(
    const AnalyzerNavigationState& state,
    const ScanTree& tree,
    const ScanTreeExclusion* exclusion) noexcept;
```

Add `const ScanTreeExclusion* exclusion = nullptr` to `apply_analyzer_command`. Reject hidden direct destinations and loop past hidden history entries without erasing them. Reconciliation changes root/selection only; it does not append duplicate history entries.

- [x] **Step 4: Verify navigation behavior**

```powershell
cmake --build --preset windows-debug --target diskbloom_tests
build/windows-debug/tests/diskbloom_tests.exe
```

Expected: all history/navigation cases pass, including unfiltered legacy behavior.

- [x] **Step 5: Commit visibility-aware navigation**

```powershell
git add src/app/analyzer_history.h src/app/analyzer_history.cpp src/app/analyzer_navigation.h src/app/analyzer_navigation.cpp tests/analyzer_history_tests.cpp tests/analyzer_navigation_tests.cpp
git commit -m "feat: skip reviewed navigation destinations"
```

---

### Task 4: Smaller Chart And Collector Restore Hit Targets

**Files:**
- Modify: `src/app/analyzer_view.h`
- Modify: `src/app/analyzer_view.cpp`
- Modify: `src/app/review_collector_interaction.h`
- Modify: `src/app/review_collector_interaction.cpp`
- Modify: `src/core/language.h`
- Modify: `src/core/string_catalog.cpp`
- Modify: `tests/analyzer_layout_tests.cpp`
- Modify: `tests/review_collector_interaction_tests.cpp`
- Modify: `tests/string_catalog_tests.cpp`

**Interfaces:**
- Consumes: current analyzer/collector geometry and localization catalog.
- Produces: 9-percent smaller `chartBounds`, `ReviewRowLayout::restoreBounds`, `hit_test_review_restore`, `StringId::RestoreItem`, and `AnalyzerCommandKind::RestoreReviewItem`.

- [x] **Step 1: Write failing geometry and localization tests**

Assert the new radius against the pre-scale responsive radius:

```cpp
const auto layout = compute_analyzer_layout(1200.0F, 720.0F, 6U);
const auto radius = (layout.chartBounds.right - layout.chartBounds.left) * 0.5F;
constexpr float unscaledResponsiveRadius = 288.0F;
CHECK(std::abs(radius - unscaledResponsiveRadius * 0.91F) < 0.001F);
CHECK(layout.chartGeometry.centerX
    == (layout.chartBounds.left + layout.chartBounds.right) * 0.5F);
```

For collector rows assert a 28 DIP minimum restore bound, no overlap with name/size bounds, half-open hit edges, and stable compact-window bounds. Add exact catalog assertions:

```cpp
CHECK(get_string(Language::English, StringId::RestoreItem) == L"Restore item");
CHECK(get_string(Language::SimplifiedChinese, StringId::RestoreItem)
    == L"\u6062\u590d\u9879\u76ee");
```

- [x] **Step 2: Build and verify RED**

Expected: layout radius assertion fails and restore fields/functions/string IDs are missing.

- [x] **Step 3: Implement pure layout changes**

Multiply the final responsive radius by `0.91F` before producing chart bounds and geometry. Extend rows:

```cpp
struct ReviewRowLayout {
    AnalyzerRectF bounds;
    AnalyzerRectF restoreBounds;
    AnalyzerRectF nameBounds;
    AnalyzerRectF sizeBounds;
    std::size_t itemIndex = 0U;
};

[[nodiscard]] std::optional<std::size_t> hit_test_review_restore(
    const ReviewCollectorLayout& layout,
    float xDip,
    float yDip) noexcept;
```

Reserve 28 DIP at the leading edge plus 8 DIP spacing, clamp all rectangles to the row, and return the stable review item index. Add `RestoreReviewItem` to `AnalyzerCommandKind` and both localized strings without drawing the icon yet.

- [x] **Step 4: Verify layout and catalog tests**

```powershell
cmake --build --preset windows-debug --target diskbloom_tests
build/windows-debug/tests/diskbloom_tests.exe
```

Expected: all compact/default geometry and localization completeness tests pass.

- [x] **Step 5: Commit pure UI contracts**

```powershell
git add src/app/analyzer_view.h src/app/analyzer_view.cpp src/app/review_collector_interaction.h src/app/review_collector_interaction.cpp src/core/language.h src/core/string_catalog.cpp tests/analyzer_layout_tests.cpp tests/review_collector_interaction_tests.cpp tests/string_catalog_tests.cpp
git commit -m "feat: add collector restore targets"
```

---

### Task 5: Projection-Aware Analyzer Reflow

**Files:**
- Modify: `src/app/analyzer_transition_controller.h`
- Modify: `src/app/analyzer_transition_controller.cpp`
- Modify: `src/app/analyzer_view.h`
- Modify: `src/app/analyzer_view.cpp`
- Modify: `tests/analyzer_transition_controller_tests.cpp`
- Modify: `tests/analyzer_render_smoke_main.cpp`

**Interfaces:**
- Consumes: Tasks 1-4 projection, filtered layouts, restore command, and chart geometry.
- Produces: `AnalyzerView::set_exclusion`, `reflow_review_change`, `hovered_review_restore_node`, a 500 ms collector duration, projected center totals, restore hover/press rendering, and interruptible same-root transitions.

- [x] **Step 1: Add failing duration and analyzer lifecycle tests**

Extend controller tests:

```cpp
controller.start(start, true, AnalyzerTransitionController::collector_duration);
CHECK(controller.advance(start + 499ms).active);
CHECK(!controller.advance(start + 500ms).active);
```

Extend analyzer smoke to install an exclusion, reflow the same root, and assert:

```cpp
const std::array reviewedNodes{folder};
CHECK(analyzer.reflow_review_change(
    &exclusion, reviewedNodes, root, start, true));
CHECK(analyzer.transition_active());
CHECK(analyzer.current_root() == root);
CHECK(analyzer.advance_transition(start + 250ms));
CHECK(!analyzer.advance_transition(start + 500ms));
```

Add an interrupted reflow using the midpoint snapshot, an animation-disabled immediate result, an all-children-excluded `0 B` draw, and pointer navigation suppression during reflow while restore-hit commands remain allowed.

- [x] **Step 2: Build and verify RED**

Expected: compilation fails on duration parameters and analyzer exclusion/reflow APIs.

- [x] **Step 3: Parameterize transition duration**

Use:

```cpp
static constexpr auto navigation_duration = std::chrono::milliseconds{700};
static constexpr auto collector_duration = std::chrono::milliseconds{500};
void start(
    std::chrono::steady_clock::time_point now,
    bool animationsEnabled,
    std::chrono::milliseconds duration = navigation_duration) noexcept;
```

Store a clamped positive duration per start. Preserve every existing directory transition test at 700 ms.

- [x] **Step 4: Integrate projected rebuild and reflow**

Add to `AnalyzerView`:

```cpp
void set_exclusion(const core::ScanTreeExclusion* exclusion) noexcept;
[[nodiscard]] bool reflow_review_change(
    const core::ScanTreeExclusion* exclusion,
    std::span<const core::NodeIndex> reviewedNodes,
    core::NodeIndex root,
    std::chrono::steady_clock::time_point now,
    bool animationsEnabled);
[[nodiscard]] std::optional<core::NodeIndex>
hovered_review_restore_node() const noexcept;
```

Store a non-owning projection pointer whose lifetime is owned by `MainWindow`. Refactor `navigate_to_root` and review reflow through one private transition builder that accepts duration. Capture the current interpolated frame and reviewed-node presentation when interrupted; install the supplied destination reviewed-node span; build destination sunburst/ranking with the new projection; clear chart hover/drag state; update effective center bytes; reserve bounded transition vectors; and start at 500 ms. Review reflow does not change breadcrumb itself.

Track source and destination reviewed-node snapshots during reflow so the open panel can cross-fade removed/restored rows. Draw a palette marker normally. For the destination row under `hoveredReviewRestore_`, draw the semantic row-hover background and two theme-token lines forming a cross; track `pressedReviewRestore_` for pressed feedback. On a matching release, emit `AnalyzerCommand{AnalyzerCommandKind::RestoreReviewItem, node}`. During chart transition, ignore chart/ranked navigation and drag starts but evaluate review restore hit testing before the transition early return.

Keep a palette-index vector aligned with reviewed nodes. Preserve indices for
existing rows, derive a new row's index from its source sunburst branch, and
use `node % palette_size` only when the node was not visible in the source
layout. Match ranked source/destination rows by node and linearly interpolate
their vertical bounds while cross-fading unmatched rows; do not limit the
animation to an opacity-only list swap.

- [x] **Step 5: Verify deterministic render behavior**

```powershell
cmake --build --preset windows-debug --target diskbloom_analyzer_render_smoke diskbloom_tests
build/windows-debug/tests/diskbloom_analyzer_render_smoke.exe
build/windows-debug/tests/diskbloom_tests.exe
```

Expected: start/midpoint/endpoint/interruption/static/empty states pass in all four theme-language combinations.

- [x] **Step 6: Commit analyzer reflow**

```powershell
git add src/app/analyzer_transition_controller.h src/app/analyzer_transition_controller.cpp src/app/analyzer_view.h src/app/analyzer_view.cpp tests/analyzer_transition_controller_tests.cpp tests/analyzer_render_smoke_main.cpp
git commit -m "feat: animate reviewed item reflow"
```

---

### Task 6: Main-Window Add And Restore Integration

**Files:**
- Create: `src/app/review_change.h`
- Create: `src/app/review_change.cpp`
- Modify: `src/app/main_window.h`
- Modify: `src/app/main_window.cpp`
- Modify: `src/app/analyzer_navigation.h`
- Modify: `src/app/analyzer_navigation.cpp`
- Modify: `src/CMakeLists.txt`
- Create: `tests/review_change_tests.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/analyzer_navigation_tests.cpp`
- Modify: `tests/deletion_review_tests.cpp`
- Modify: `tests/analyzer_render_smoke_main.cpp`

**Interfaces:**
- Consumes: normalized `DeletionReview`, `ScanTreeExclusion`, visibility reconciliation, `RestoreReviewItem`, and analyzer reflow.
- Produces: testable `apply_review_mutation`, one authoritative projection lifecycle, synchronized add/restore/navigation/timer behavior, and localized restore help.

- [x] **Step 1: Write failing command-flow regression tests**

Add pure coordinator coverage for add and restore:

```cpp
const auto result = apply_review_mutation(
    review,
    tree,
    {ReviewMutationKind::Add, currentRoot},
    currentRoot,
    selectedNode);
CHECK(result.changed);
CHECK(result.root == visibleParent);
CHECK(result.selected == visibleParent);
CHECK(result.exclusion.is_excluded_root(currentRoot));

const auto restored = apply_review_mutation(
    review,
    tree,
    {ReviewMutationKind::Restore, currentRoot},
    result.root,
    result.selected);
CHECK(restored.changed);
CHECK(restored.exclusion.empty());
```

Also cover duplicate add, absent restore, scan-root rejection, parent replacing reviewed descendants, totals, and selected-descendant fallback. Add render/integration assertions that a successful result removes the node from the analyzer destination, restore emits the stable node, and recycling blocks restore dispatch.

- [x] **Step 2: Build and verify RED**

Expected: compilation fails because `app/review_change.h` and its mutation types do not exist.

- [x] **Step 3: Own and synchronize the projection**

Implement the pure coordinator first:

```cpp
enum class ReviewMutationKind { Add, Restore };

struct ReviewMutation {
    ReviewMutationKind kind = ReviewMutationKind::Add;
    core::NodeIndex node = core::invalid_node;
};

struct ReviewMutationResult {
    bool changed = false;
    core::ScanTreeExclusion exclusion;
    core::NodeIndex root = core::invalid_node;
    core::NodeIndex selected = core::invalid_node;
};

[[nodiscard]] ReviewMutationResult apply_review_mutation(
    DeletionReview& review,
    const core::ScanTree& tree,
    ReviewMutation mutation,
    core::NodeIndex currentRoot,
    core::NodeIndex selectedNode);
```

On success, rebuild the returned projection from the normalized review nodes
and use Task 3 visibility reconciliation rules for root/selection. On failure,
return `changed == false` and leave `DeletionReview` untouched.

Then add to `MainWindow`:

```cpp
core::ScanTreeExclusion reviewExclusion_;

void apply_review_change(
    core::NodeIndex changedNode,
    std::chrono::steady_clock::time_point now);
```

For add/restore commands, call `apply_review_mutation`; return immediately when `changed` is false. Move the result projection into `reviewExclusion_`, apply its root/selection, update the review summary, and call `reflow_review_change(&reviewExclusion_, deletionReview_.nodes(), result.root, now, animationsEnabled)`. Then synchronize breadcrumb/history chrome, start or kill the existing animation timer, synchronize the hover timer, and invalidate once. Do not call `set_review_nodes` first because reflow must snapshot the previous collector rows.

Handle `RestoreReviewItem` by passing a restore mutation into the same coordinator path used by additions. Duplicate/absent operations return before projection replacement. Pass `&reviewExclusion_` into all analyzer navigation commands and visible back/forward queries.

Create a native restore tooltip owned by `MainWindow`, parallel to the existing
breadcrumb tooltip. Show `StringId::RestoreItem` while a restore cross is
hovered, update it after a language change, and hide it on pointer leave,
transition completion, recycling, scan replacement, or collector closure.

- [x] **Step 4: Reset state at lifecycle boundaries**

On scan start/completion replacement and recycle completion, clear review and projection before setting the analyzer tree. Call `analyzer_.set_exclusion(&reviewExclusion_)` after installing a completed tree. When recycling begins, preserve the projection and rows but disable restore interactions until terminal state; successful recycling clears both before rescan.

- [x] **Step 5: Run Debug integration suite**

```powershell
cmake --build --preset windows-debug
ctest --preset windows-debug --output-on-failure
```

Expected: all existing and new CTest targets pass, including dark/light en-US/zh-CN app smoke.

- [x] **Step 6: Commit integrated review behavior**

```powershell
git add src/app/review_change.h src/app/review_change.cpp src/app/main_window.h src/app/main_window.cpp src/app/analyzer_navigation.h src/app/analyzer_navigation.cpp src/CMakeLists.txt tests/review_change_tests.cpp tests/CMakeLists.txt tests/analyzer_navigation_tests.cpp tests/deletion_review_tests.cpp tests/analyzer_render_smoke_main.cpp
git commit -m "feat: hide and restore reviewed items"
```

---

### Task 7: Release Performance And Visual Verification

**Files:**
- Create: `benchmarks/scan_tree_exclusion_benchmark.cpp`
- Modify: `benchmarks/CMakeLists.txt`
- Modify: `tests/analyzer_render_smoke_main.cpp`
- Modify: `docs/qa/sunburst-analyzer.md`
- Create: `docs/qa/evidence/review-exclusion/before-dark-en.png`
- Create: `docs/qa/evidence/review-exclusion/collected-midpoint-dark-en.png`
- Create: `docs/qa/evidence/review-exclusion/restore-hover-light-zh.png`
- Create: `docs/qa/evidence/review-exclusion/restored-light-zh.png`
- Modify: `docs/superpowers/plans/2026-07-16-review-exclusion-and-restore.md`

**Interfaces:**
- Consumes: completed Tasks 1-6.
- Produces: a repeatable projection gate, inspected pixel evidence, checked plan, and final Release executable.

- [x] **Step 1: Add the deterministic Release projection benchmark**

Build a representative deep synthetic tree, choose multiple non-overlapping reviewed roots, reserve sample storage, and run projection rebuild plus effective-size/visibility queries. Print:

```text
nodes=<count>
excluded_roots=<count>
iterations=<count>
average_ms=<value>
p95_ms=<value>
checksum=<stable value>
```

Register `diskbloom_scan_tree_exclusion_benchmark_gate` in CTest with exact node/root/checksum regexes and a documented p95 threshold established from three Release runs on the QA host. The executable returns nonzero when count, checksum, or threshold changes.

- [x] **Step 2: Build and test Release**

```powershell
cmake --build --preset windows-release
ctest --preset windows-release --output-on-failure
```

Expected: full Release suite passes, including projection, layout, transition, collector, hover, render, and four app smoke combinations.

- [x] **Step 3: Run performance gates three times**

Run the new exclusion benchmark plus existing layout, transition, collector, and hover-pulse benchmarks three times. Record every average, p95, throughput, segment count, and checksum. Reject material regressions or document an explicit measured trade-off before continuing.

- [x] **Step 4: Capture and inspect deterministic frames**

Extend `--capture-dir` analyzer smoke output with the four named frames. Inspect them directly for:

- 9-percent smaller chart with stable center and no overlap.
- Collected branch absent from the destination chart and ranked list.
- Nonblank, coherent 250 ms add midpoint.
- Normal palette marker becoming a cross on hover in light/zh-CN.
- Restored branch returning to its original palette and relative location.
- Readable labels and controls in both themes/languages.

- [x] **Step 5: Perform visible native QA**

In the Release app, scan a folder fixture and verify file add/restore, directory add/restore, current-directory automatic parent fallback, repeated cross clicks during an active reflow, pointer leave, scrolling, back/forward skipping, animation `Always on`/`Follow Windows`/`Off`, and recycling-disabled controls. Compare timing, hierarchy, marker-to-cross behavior, and chart reflow against the supplied DaisyDisk screenshot and recording.

- [x] **Step 6: Record QA, check the plan, and commit**

Append commands, raw benchmark runs, test totals, limitations, and evidence paths to `docs/qa/sunburst-analyzer.md`. Mark all plan checkboxes, then run:

```powershell
git diff --check
git add benchmarks/scan_tree_exclusion_benchmark.cpp benchmarks/CMakeLists.txt tests/analyzer_render_smoke_main.cpp docs/qa/sunburst-analyzer.md docs/qa/evidence/review-exclusion docs/superpowers/plans/2026-07-16-review-exclusion-and-restore.md
git commit -m "docs: verify review exclusion and restore"
```

- [x] **Step 7: Final verification**

Run fresh Debug and Release builds plus both full CTest presets. Confirm the worktree is clean and report the exact Release executable path:

```text
D:\daisydiskX\build\windows-release\src\DiskBloom.exe
```
