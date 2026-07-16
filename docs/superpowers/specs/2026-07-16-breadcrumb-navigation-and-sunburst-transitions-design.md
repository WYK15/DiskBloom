# Breadcrumb Navigation And Sunburst Transitions Design

Date: 2026-07-16
Status: Approved for planning
References:

- `feedback/iShot_2026-07-15_22.10.59.mp4`
- Extracted reference frames around 43.9, 44.5, 44.8, and 45.1 seconds

## Goal

Replace the analyzer's single current-directory title with a DaisyDisk-style full-path breadcrumb, and replace abrupt directory changes with a polar segment transition that visually preserves the selected branch.

The work must retain DiskBloom branding, recycle-only deletion safety, runtime English/Simplified Chinese switching, light/dark themes, and the performance-first native C++ architecture.

## Reference Findings

The supplied recording shows behavior closer to per-segment polar interpolation than a whole-chart bitmap transform:

- The breadcrumb updates at the start of navigation.
- The selected branch remains spatially anchored.
- Unselected branches contract and fade.
- New child segments expand from the selected branch's previous angle.
- Segment start/end angles and inner/outer radii change during the transition.
- The right title and list cross-fade while the chart expands.
- The observed transition completes in approximately 0.6 to 0.8 seconds without spring overshoot.

DiskBloom will use a 700 ms cubic ease-out transition and interpolate segment polar parameters directly.

## Breadcrumb Navigation

### Content

The header contains, from left to right:

1. Back and forward icon buttons.
2. A localized `Disks and Folders` breadcrumb that returns to the overview.
3. The absolute scan-root path components.
4. Every ancestor from the scan root to the current analyzer root.

For a full-drive scan, every filesystem breadcrumb represented in the immutable `ScanTree` is clickable. For a selected-folder scan, absolute path components above the scanned root are displayed as disabled context because their contents were not scanned. The scanned root and its descendants are clickable. The overview breadcrumb is always clickable.

Path components are derived once when the completed scan or analyzer root changes. Pointer movement and drawing never query the filesystem or rebuild paths.

### Overflow

When all segments fit, the entire path is visible. At constrained widths, retain:

- The overview/root entry.
- The current directory.
- The nearest parent that fits.
- An ellipsis segment representing hidden middle components.

Clicking the ellipsis opens a themed menu listing the hidden components in path order. Enabled scanned-tree components navigate directly; unscanned prefix components remain disabled. Hovering a segment or the ellipsis exposes the complete absolute path through a native tooltip/accessibility name.

Breadcrumb labels use measured text widths, fixed horizontal padding, and a chevron end cap. They never resize the window controls or overlap them. Long individual components use ellipsis trimming.

### History

Back and forward use a bounded contiguous history of at most 128 navigation entries. Entries identify either the overview or a valid node in the current completed scan.

- Drilling into a directory or clicking a breadcrumb appends an entry and clears forward history.
- Back and forward move through history without appending duplicates.
- Starting, cancelling, failing, replacing, or completing a scan clears history tied to the previous tree.
- Recycling and automatic rescan clear stale node history.
- Invalid history nodes are discarded instead of dereferenced.

Back, forward, and breadcrumb jumps between analyzer nodes use the same chart transition. Returning to the overview ends the analyzer immediately and cancels any active chart transition. The current standalone back control is replaced by the history-aware back button; forward is disabled when no later entry exists.

## Polar Segment Transition

### Transition Model

`AnalyzerView` retains immutable bounded snapshots for the source visual state and destination layout. Each transition segment contains:

- Stable identity: node index, or aggregate identity based on parent/depth/aggregate role.
- Source and destination start/end angles.
- Source and destination inner/outer radii.
- Source and destination opacity.
- Palette index.

Entering a directory anchors the transition to that directory's source segment. Returning or jumping to an ancestor uses the inverse relationship. A multi-level breadcrumb jump matches every node shared by the source and destination layouts and collapses unmatched segments toward the closest shared ancestor.

Matched segments interpolate their polar bounds. Newly visible segments begin collapsed at the anchor angle/radius with zero opacity. Segments absent from the destination collapse toward the anchor and fade out. Aggregate segments use their parent/depth identity when possible and otherwise use the unmatched behavior.

At `t = 0`, the rendered chart equals the source visual. At `t = 1`, it equals the normal destination `SunburstLayout` exactly; transition buffers are then released.

### Timing And Interruption

- Duration: 700 ms.
- Easing: cubic ease-out, `1 - (1 - t)^3`.
- Frame scheduling: a 16 ms Win32 timer invalidates the analyzer while active; elapsed time comes from `std::chrono::steady_clock`, not timer tick counts.
- Right title, item count, and ranked list cross-fade: source fades during the first 45 percent; destination fades in from 30 to 100 percent.
- Breadcrumb state changes immediately when navigation is accepted.
- Chart, list, drag, and collector mutation input is suppressed during the transition. Window controls, history buttons, and breadcrumbs remain available.
- A navigation command during an active transition captures the current interpolated segment state as the next source, preventing a snap before the new transition.
- Replacing the scan, returning to overview, starting recycling, losing the tree, or closing the window cancels the transition and releases its buffers.

When Windows client-area animations are disabled through `SPI_GETCLIENTAREAANIMATION`, duration is zero and the destination renders immediately. Theme or language changes during animation rebuild only text/theme resources; geometric progress continues.

### Rendering

Each frame interpolates at most 2,048 bounded segment records and rebuilds at most 12 Direct2D color-batched path geometries. It does not rebuild the filesystem tree, rank children, allocate per segment, build paths, or access the filesystem.

Transition vectors reserve their maximum required size at navigation start. Per-frame work reuses buffers. Mathematical hit testing remains disabled until the destination becomes authoritative.

## Components And Ownership

### Breadcrumb Model

A focused app module owns pure breadcrumb construction, width collapse, hit geometry, hidden-item menu data, and localized accessibility text inputs. It consumes the immutable scan tree and the completed scan's normalized root path.

### Navigation History

`AnalyzerNavigationState` owns the bounded history cursor and validates entries against the active tree. `MainWindow` converts breadcrumb/history commands into navigation changes and remains responsible for overview/analyzer transitions.

### Transition Model

A focused render-independent transition module owns segment matching, interpolation, easing, interruption snapshots, and completion. `AnalyzerView` supplies layouts, draws interpolated batches, and exposes whether another frame is required. `MainWindow` owns the timer.

No component performs filesystem scanning or Shell work on the UI thread.

## Themes And Localization

Breadcrumb normal, hover, pressed, current, disabled, ellipsis, focused, and menu states use shared theme tokens in both light and dark modes. Chevron separators and disabled text must retain sufficient contrast without hard-coded theme-specific colors.

`Disks and Folders` and every new tooltip/menu label come from the localization catalog in English and Simplified Chinese. Filesystem names remain unchanged Unicode data. Layout uses measured strings and trimming so either language cannot overlap navigation or window controls.

Animation colors derive from existing palette indices and theme tokens. Switching theme mid-transition must not restart navigation.

## Safety And Error Handling

- Breadcrumbs never navigate to files, invalid nodes, stale nodes, or unscanned prefix paths.
- Menu and hit targets are recomputed after DPI, size, theme, language, or current-root changes.
- Transition matching tolerates missing nodes and aggregate changes by fading/collapsing unmatched segments.
- Allocation or Direct2D geometry creation failure cancels animation and renders the destination layout immediately.
- No transition or navigation path bypasses deletion review, confirmation, or Recycle Bin behavior.

## Performance Gates

Add a Release benchmark using a deterministic 2,048-segment source/destination pair across 60 transition frames.

- Validate exact endpoint layouts and a stable checksum.
- Record median and 95th-percentile CPU time per interpolated frame.
- On the recorded host/compiler, p95 interpolation plus batched-geometry preparation must remain at or below 8.0 ms.
- Existing static sunburst layout and hit-test baselines must remain within their current same-host gates.
- Record transition buffer memory; it must remain bounded by the two 2,048-segment snapshots and reusable batch storage.

The 8.0 ms CPU gate leaves headroom inside a 16.7 ms frame budget but is not, by itself, a full 60 FPS presentation claim.

## Test And QA Strategy

Follow TDD for every new behavior:

- Breadcrumb construction for drive scans, selected-folder scans, root paths, Unicode names, and invalid nodes.
- Width collapse at 800x600 and 1200x720 in English and Simplified Chinese.
- Ellipsis contents, enabled/disabled states, hit testing, tooltips, and navigation commands.
- Back/forward history, forward invalidation, duplicate suppression, bounded capacity, scan replacement, and stale nodes.
- Transition endpoint equality, monotonic easing, matched/unmatched segments, enter/return/multi-level jumps, interruption, cancellation, and reduced-motion behavior.
- Render smoke at start, midpoint, and completion in light/dark and English/Simplified Chinese.
- Interaction tests proving chart/list/drag input is suppressed while history and breadcrumbs remain active.
- Release benchmark for 2,048 segments and 60 frames.
- Visible screenshots compared with the supplied recording, especially the breadcrumb and the transition stages near 43.9, 44.5, 44.8, and 45.1 seconds.

## Out Of Scope

- Editing or typing arbitrary paths into the breadcrumb.
- Navigating above a selected-folder scan root without starting a new scan.
- Windows Explorer/OLE drag-and-drop.
- Spring, physics, or user-configurable animation duration.
- Permanent deletion or changes to recycle-only safety.
