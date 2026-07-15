# Sunburst Analyzer QA And Performance Baseline

Date: 2026-07-16

## Scope

The analyzer consumes an immutable completed `ScanTree`, builds at most 2,048
cached polar segments, batches Direct2D path geometry into at most 12 palette
draws, and performs pointer hit testing without traversing the filesystem tree.
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

Remaining differences, not accepted as complete parity yet:

- The reference has a full clickable breadcrumb chain; DiskBloom currently
  shows the current root plus a back control.
- The reference has a ranked, scrollable immediate-child list; DiskBloom
  currently shows only the active item summary.
- The reference has a bottom deletion collection and review action; DiskBloom
  has not implemented collection or deletion yet.
- The reference uses animated fan-shaped transitions. DiskBloom currently
  switches cached layouts without the matching transition animation.
