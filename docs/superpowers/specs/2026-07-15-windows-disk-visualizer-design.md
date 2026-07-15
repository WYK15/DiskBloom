# Windows Disk Visualizer V1 Design

## 1. Product Goal

Build a publicly distributable Windows disk-space visualizer whose core workflow and interaction quality closely match DaisyDisk while using an original product name and original brand assets.

V1 covers:

- Local fixed drives and removable drives.
- Scanning an explicitly selected folder.
- Live scan progress and cancellation.
- An interactive sunburst chart for navigating directory size.
- File preview, reveal in File Explorer, and deletion through the Recycle Bin.
- Simplified Chinese and English switching at runtime.
- Complete light and dark themes.

Cloud providers, network shares, duplicate detection, automatic cleanup, and direct NTFS MFT parsing are outside V1.

## 2. Product Constraints

1. Performance is the primary engineering constraint. Release builds must be measured before performance claims are made.
2. The UI must remain responsive during enumeration, aggregation, cancellation, and deletion.
3. Windows filesystem and safety semantics take precedence when they conflict with macOS behavior.
4. Every user-visible feature must work in light and dark modes and in both supported languages.
5. DaisyDisk references define workflow and interaction targets, but names, icons, and proprietary assets must be original.

## 3. Technology

- Language: C++20.
- Build: CMake with the Visual Studio 2026 MSVC toolchain and Ninja for local iteration.
- Windowing: Win32.
- Graphics: Direct3D 11 device, DXGI swap chain, Direct2D device context, and DirectWrite.
- Animation composition: DirectComposition where it reduces redraw or presentation overhead.
- Tests: CTest with a small in-repository test runner. External test frameworks are not required for the first bootstrap.
- Packaging target: self-contained x64 application, followed by MSIX packaging for public builds.

WinUI, WPF, browser runtimes, and managed runtime dependencies are deliberately excluded from the application hot path.

## 4. Solution Boundaries

```text
src/app/                 Win32 entry point, windows, navigation, commands
src/core/                scan model, aggregation, chart layout, application state
src/platform/windows/    filesystem, volumes, shell, recycle bin, theme detection
src/render/              D3D/D2D resources, sunburst rendering, text, animation
src/resources/           theme tokens and zh-CN/en-US string catalogs
tests/                   unit, integration, and deterministic filesystem fixtures
benchmarks/              scan, aggregation, layout, and rendering benchmarks
docs/                    architecture, QA, packaging, and performance records
```

Platform code communicates with core code through narrow interfaces. Core algorithms must be testable without creating a window or accessing a physical disk.

## 5. Scan Engine

### 5.1 Enumeration

V1 uses `FindFirstFileExW` and `FindNextFileW` with `FIND_FIRST_EX_LARGE_FETCH` where supported. Enumeration is iterative rather than recursively consuming the call stack.

The scanner:

- Does not follow directory reparse points by default.
- Records reparse points as leaf entries so their existence remains visible.
- Continues after access-denied, path, and transient I/O errors and attaches compact error state to the affected directory.
- Uses 64-bit logical sizes. Allocation-size accounting can be added behind the same interface after measurement.
- Supports cooperative cancellation through `std::stop_token`.
- Selects conservative per-volume concurrency to avoid destructive random I/O on rotational media.

### 5.2 Data Layout

The filesystem tree is stored in contiguous node blocks. Nodes use integer indices instead of owning pointers.

Each node contains only hot structural and numeric fields:

- Parent index.
- First-child index and next-sibling index.
- Offset and length in a shared UTF-16 string arena.
- Logical byte size.
- File count and directory count.
- Flags for directory, reparse point, inaccessible, and incomplete state.

Large names live in an append-only string arena. The hot path avoids per-file heap allocation, `std::filesystem` object construction, and repeated full-path copies.

### 5.3 Aggregation and Progress

Directory totals are calculated in post-order after child enumeration completes. Worker threads publish bounded progress snapshots rather than one notification per entry.

The UI receives at most one visible update per presentation interval. Progress messages contain counters and stable node indices, not copies of the full tree.

## 6. Rendering and Interaction

The sunburst is a single render surface, not a hierarchy of Win32 controls.

The layout stage converts a selected subtree into a flat array of immutable segments containing:

- Start and end angle.
- Inner and outer radius.
- Node index.
- Color role and visual state.

Tiny segments below a pixel/angle threshold are grouped visually while remaining discoverable through parent navigation and list details.

Hit testing converts the pointer position to polar coordinates and searches the relevant radial band. Geometry batches are cached until the selected node, size, viewport, theme, or layout threshold changes.

Navigation animation interpolates a compact view transform and opacity values. It does not rebuild filesystem nodes or allocate controls per frame. The target is smooth interaction at 60 Hz on representative integrated graphics.

## 7. Application Views

### 7.1 Disk Overview

- Custom title bar and restrained dark/light visual treatment modeled on the supplied launch screenshot.
- One row per mounted fixed or removable volume.
- Capacity, usage gauge, scan state, progress, and scan/cancel command.
- Commands for scanning a folder and opening Settings.

### 7.2 Analyzer

- Large central sunburst visualization.
- Current path and back navigation.
- Selected item name, size, item count, and type.
- Preview and reveal commands.
- A collection area for reviewed deletion candidates.

### 7.3 Deletion Review

Deletion is never triggered directly by clicking a chart segment. Users explicitly add items to a review collection, review the resulting paths and total size, and confirm deletion to the Recycle Bin.

Protected, missing, in-use, or no-longer-equivalent paths are reported individually. The application does not silently fall back to permanent deletion.

### 7.4 Settings

- Theme: system, light, or dark.
- Language: Simplified Chinese or English.
- Reduced animation option for accessibility and low-power devices.

Theme and language changes apply without restarting the application.

## 8. Themes and Localization

Rendering code consumes semantic theme tokens such as background, surface, primary text, secondary text, accent, danger, focus, and chart palette roles. Literal UI colors do not appear in feature code.

English and Simplified Chinese catalogs use stable string identifiers. Feature code requests an identifier and receives a UTF-16 string from the active catalog. Missing keys are test failures.

Layout measurements use DirectWrite and must be verified with the longest string from either catalog. Fixed command surfaces have stable dimensions and ellipsize only paths or explicitly variable content.

## 9. Error Handling and Recovery

- Device loss recreates the DirectX device and all device-dependent resources without losing scan state.
- A removed volume transitions the active scan to an interrupted state and preserves partial results until dismissed.
- Scan errors are accumulated in bounded summaries to avoid unbounded memory growth.
- Unhandled exceptions at thread boundaries are converted into structured operation failures.
- Settings are written atomically under the user's local application data directory.

## 10. Performance Gates

Benchmarks run in x64 Release builds and record CPU, storage type, dataset, elapsed time, throughput, peak working set, and result counts.

Initial V1 gates on a generated one-million-entry fixture are:

- No per-entry UI messages.
- Scanner memory growth remains bounded by the compact node and name stores plus fixed worker buffers.
- Cancellation is observed within 250 ms when workers are not blocked inside an operating-system call.
- Progress publishing is capped at 30 updates per second.
- Cached chart hover interaction remains within a 16.7 ms frame budget at 60 Hz.

Absolute scan throughput and peak-memory release thresholds will be set only after the first benchmark is executed on representative SSD and HDD hardware. This avoids inventing targets without evidence.

## 11. Testing

- Unit tests cover size aggregation, node linking, overflow boundaries, localization completeness, theme token completeness, segment layout, and polar hit testing.
- Filesystem integration tests create temporary trees containing inaccessible entries where permissions allow, long paths, empty directories, Unicode names, reparse points, and files changed during scanning.
- Rendering tests validate deterministic segment geometry and use captured screenshots for light/dark and zh-CN/en-US visual checks.
- Manual QA covers device removal, cancellation, locked files, Recycle Bin behavior, multiple DPI scales, keyboard navigation, and reduced motion.
- Performance tests are separate from correctness tests and always use Release builds.

## 12. Delivery Sequence

1. Bootstrap the native window, renderer, resource catalogs, settings, and test harness.
2. Build and benchmark the compact scan model and cancellable Win32 enumerator.
3. Implement the disk overview and live scan state.
4. Implement sunburst layout, rendering, hit testing, and navigation.
5. Implement preview, reveal, deletion review, and Recycle Bin integration.
6. Complete accessibility, error recovery, visual comparison, performance records, and MSIX packaging.

