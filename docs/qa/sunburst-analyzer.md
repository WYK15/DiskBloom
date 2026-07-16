# Sunburst Analyzer QA And Performance Baseline

Date: 2026-07-16

## Scope

The analyzer consumes an immutable completed `ScanTree`, builds at most 2,048
cached polar segments, batches static Direct2D path geometry into at most 12
palette draws and transition geometry into at most 24 palette draws, and
performs pointer hit testing without traversing the filesystem tree.
Cancelled and failed scans do not enter this view.

## Build And Host

- Build: `windows-release`
- Compiler: Microsoft C/C++ Optimizing Compiler 19.51.36248 for x64
- CPU: AMD Ryzen 7 PRO 4750G, 8 cores / 16 logical processors
- RAM: 16,510,099,456 bytes
- OS: Windows 11 Pro for Workstations, build 26100
- Display scale for visual checks: 150%

Command:

```powershell
build/windows-release/benchmarks/diskbloom_sunburst_layout_benchmark.exe 1000000 1000
```

The deterministic fixture contains one million nodes arranged under roughly
977 top-level directories. Every run performs 1,000 complete layout rebuilds
and 1,000,000 cached polar hit tests.

## Raw Results

| Run | Layout total (ms) | Layout average (ms) | Max segments | Hit total (ms) | Hits/second |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 6,388.02 | 6.39 | 1,465 | 107.84 | 9,273,229.21 |
| 2 | 6,437.52 | 6.44 | 1,465 | 105.88 | 9,444,877.33 |
| 3 | 6,390.06 | 6.39 | 1,465 | 106.40 | 9,398,540.41 |

Median layout total: **6,390.06 ms**, or **6.39 ms per rebuild**. Median
hit-test total: **106.40 ms per million points**, or **9,398,540.41
hits/second**.

## Same-Host Regression Gates

- Validation must pass and segment count must remain at or below the hard
  2,048-segment bound.
- On the recorded host and toolchain, the median of three layout runs must not
  exceed the measured 6.39 ms per rebuild.
- On the recorded host and toolchain, the median of three hit-test runs must not
  exceed the measured 106.40 ms per million points.
- A new host or compiler requires a separately recorded baseline before timing
  gates are enforced.

The layout result is below a 16.7 ms CPU frame budget on this fixture. This is
not a claim that full composition, input handling, and presentation meet 60 Hz;
GPU frame instrumentation is still required before that release claim.

The 0.1.0 release verification precomputed each parent's byte-to-angle scale in
the two child passes. Three final samples measured 5.97, 6.18, and 6.10 ms per
layout rebuild (6.10 ms median), with the same 1,465-segment maximum. Cached
hit tests measured 105.83, 114.16, and 106.25 ms per million (106.25 ms
median). Both medians remain inside the original same-host gates.

## Correctness And Visual Checks

- Debug and Release: 4/4 CTest targets passed, including a real hidden-window
  D3D11/Direct2D analyzer render and Present.
- A real C: scan transitioned to the analyzer in about 3 seconds in Release.
- Chart activation, directory/root back navigation, and return to the overview
  were exercised through real Win32 pointer messages.
- 1200x720 captures were inspected in dark/light and zh-CN/en-US at 150% scale.
- Current-root name, center size, right-side size/item count, back control, and
  Windows window controls remained visible without overlap in all combinations.
- Descendants inherit their top-level branch palette index in both themes.

## DaisyDisk Reference Comparison

The supplied `feedback/iShot_2026-07-15_22.10.59.mp4` was decoded locally with
Chrome at eight ten-second intervals. It is 77.16 seconds at 1600x1200.

Implemented parity:

- Large unframed radial analyzer on the left with direct directory drill-down.
- Current size in the center, current item summary on the right, and back
  navigation in the top bar.
- Stable color identity across descendant rings and visible small-item
  aggregation.

Added parity for the supplied navigation reference:

- Full clickable absolute-path breadcrumbs with back/forward history.
- Responsive middle-path collapse into a themed, clickable overflow menu.
- A 700 ms cubic ease-out polar segment morph with immediate path updates,
  detail/list cross-fade, rapid-navigation snapshots, and reduced-motion support.

Added after the original baseline:

- A bounded, ranked, scrollable immediate-child list with stable name and size
  columns.
- Open/preview and Explorer reveal commands for the selected node.
- A bottom deletion collection with size summary, absolute-path confirmation,
  asynchronous recycle-only execution, and automatic rescan after success.
- Selected-folder scanning through the native Windows folder picker.

## Review Collector Interaction Baseline

Recorded on 2026-07-16 using the same host described above, at 100% display
scale (`GetDpiForWindow` returned 96), with:

- Microsoft C/C++ Optimizing Compiler 19.51.36248 for x64
- CMake 4.3.1-msvc1
- Ninja 1.13.2
- Windows Release preset with compiler optimizations enabled

Command:

```powershell
build/windows-release/benchmarks/diskbloom_review_collector_benchmark.exe 1000000
```

The benchmark precomputes a fixed 1200x720 collector layout, begins one drag,
then runs one million deterministic iterations. Each iteration performs drag
movement, collector and panel point containment, and bounded scroll-offset
movement. The timed loop performs no filesystem queries, traversal, or other
filesystem work. Its checksum consumes the interaction results so the measured
work cannot be discarded.

| Run | Iterations | Total (ms) | Operations/second | Checksum |
| ---: | ---: | ---: | ---: | ---: |
| 1 | 1,000,000 | 10.26 | 97,508,653.89 | 11,879,453 |
| 2 | 1,000,000 | 10.93 | 91,503,866.04 | 11,879,453 |
| 3 | 1,000,000 | 10.25 | 97,564,783.02 | 11,879,453 |

Median: **10.26 ms** and **97,508,653.89 operations/second**. The checksum was
stable at **11,879,453**. This median is the baseline for future comparisons on
this host and toolchain; a different host or compiler requires a new three-run
baseline before drawing a regression conclusion.

Correctness gate: a run of exactly **1,000,000 iterations must produce checksum
11,879,453**. The benchmark exits nonzero when that deterministic checksum
changes, even if the changed checksum is itself nonzero, and CTest verifies the
canonical checksum in the benchmark output. Any intentional workload change
must update this gate and record a new same-host baseline in the same change.

## Review Collector Regression And Visual QA

The full Debug and Release builds completed successfully on 2026-07-16. CTest
passed **8/8 targets in Debug** and **8/8 targets in Release**, including the
collector checksum regression gate, hidden-window Direct3D/Direct2D analyzer
render smoke test, and the four app smoke targets for dark/light and
en-US/zh-CN.

Checks performed in the visible Release application:

- Launched dark/en-US, dark/zh-CN, light/en-US, and light/zh-CN. The overview
  window rendered in all four combinations. The inspected English and Chinese
  overview labels were localized and did not clip or overlap at 1200x720.
- Scanned C: in dark/en-US and inspected the real analyzer with an empty
  collector, populated summary, and hover panel.
- Exercised a ranked-row drag and a chart-segment drag with physical pointer
  input. The collector highlight and drag preview were visible on a valid drop.
- Repeated the ranked-row drop and performed a release outside the collector.
  The item count did not change for the duplicate or invalid drop; the chart
  drop then increased it from one to two.
- Inspected `feedback/PixPin_2026-07-16_09-30-14.png`. The implemented hover
  panel follows the reference hierarchy: summary at the bottom left, list panel
  anchored directly above it, names on the left, sizes on the right, and the
  panel above the analyzer content. DiskBloom uses its own colors, labels, and
  controls.

Checks not claimed as manual visual verification:

- Long-list scrolling is exercised by the analyzer render smoke fixture with
  20 reviewed nodes, but its hidden window was not captured for visual review.
- Recycling-disabled drag and scroll behavior is exercised by automated tests,
  but the visible disabled state was not manually inspected.
- The confirmation dialog and recycle/rescan flow were not run against the
  scanned system drive, to avoid destructive filesystem changes.
- Collector states were manually inspected only in dark/en-US. The automated
  analyzer fixture renders populated/drag/scroll states in all four
  theme/language combinations, but those frames were not treated as manual
  pixel verification.

## Breadcrumb And Transition Baseline

Recorded on 2026-07-16 with the same Release host and toolchain. The benchmark
interpolates exactly 2,048 matched segments for 60 frames and prepares six
polar geometry points per segment in preallocated contiguous buffers. It does
not include GPU submission or `Present`.

Command:

```powershell
build/windows-release/benchmarks/diskbloom_sunburst_transition_benchmark.exe
```

| Run | Segments | Frames | Average (ms) | p95 (ms) | Checksum |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 2,048 | 60 | 0.090 | 0.098 | 58,084,095 |
| 2 | 2,048 | 60 | 0.091 | 0.095 | 58,084,095 |
| 3 | 2,048 | 60 | 0.089 | 0.090 | 58,084,095 |

Median p95: **0.095 ms**. The executable fails if the p95 exceeds **8 ms**, if
the checksum is zero, or if the plan does not contain exactly 2,048 morphs.
CTest also requires `segments=2048` in the output. The checksum remained stable
at **58,084,095** in all three Release runs.

Debug and Release both passed **9/9 CTest targets**, including the transition
performance gate, BGRA capture test, deterministic transition render smoke,
and all dark/light plus en-US/zh-CN application smoke combinations.

### Reliable Frame Evidence

`GraphicsDevice::end_draw(CapturedFrame*)` copies the D3D11 BGRA back buffer to
a staging texture after Direct2D `EndDraw` and before swap-chain `Present`.
The analyzer smoke executable encodes that buffer through WIC, avoiding the
black or unrelated regions seen in desktop-level capture attempts.

Inspected evidence under `docs/qa/evidence/breadcrumb-transition/`:

- `breadcrumb-compact-light-en.png`: compact root/ellipsis/parent/current path.
- `breadcrumb-overflow-dark-zh.png`: dark themed hidden-path menu and Chinese UI.
- `transition-000ms-light-en.png`: destination breadcrumb updated immediately,
  with the source chart/detail still visible.
- `transition-350ms-light-en.png`: nonblank midpoint with contracted/morphed
  polar segments and fading details.
- `transition-700ms-light-en.png`: exact static endpoint after interrupted
  navigation, with matching breadcrumb, chart, and ranked list.
