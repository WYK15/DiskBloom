# Review Collector Interactions Design

Date: 2026-07-16
Status: Approved for planning
Reference: `feedback/PixPin_2026-07-16_09-30-14.png`

## Goal

Bring DiskBloom's deletion collector closer to DaisyDisk by adding two analyzer interactions:

- Hovering the collected-items summary opens a read-only panel listing every reviewed item by name and size.
- Files and directories can be dragged from either the sunburst chart or the ranked child list onto the bottom collector.

Dropping an item only adds it to `DeletionReview`. Actual deletion continues to require the existing explicit confirmation and uses the Recycle Bin only.

## Chosen Approach

Implement a lightweight drag state machine inside `AnalyzerView`. Reuse the analyzer's mathematical chart and list hit testing, and render all new visuals in the existing Direct2D pass. Do not use OLE drag-and-drop or create another native window.

This approach keeps pointer handling synchronous and allocation-free during ordinary pointer movement. Review display data is refreshed only when the review collection changes; drawing and hovering never query the filesystem.

## Interaction Model

### Drag Sources

Pressing a draggable sunburst segment or ranked-list row creates a potential drag for that node. Pointer movement beyond a 6 DIP threshold changes it into an active drag. Movement below the threshold preserves the existing click behavior.

The scan root is not draggable because `DeletionReview` rejects it. Files and directories otherwise use the same interaction.

During an active drag:

- Draw a compact, translucent preview containing the node name and formatted size near the pointer.
- Highlight the collector when the pointer is inside its drop bounds.
- Suppress chart navigation and row selection commands until release.
- On release over the collector, emit `AddToReview` for the dragged node.
- On release elsewhere, cancel without changing the review collection.
- On pointer leave, capture loss, scan replacement, or navigation back to overview, cancel the drag.

The window captures the mouse while a drag is active so releasing just outside the client area cannot leave stale drag state.

### Collector Hover Panel

When the review is non-empty, hovering either the collected-items summary or its open panel displays an anchored panel above the action bar. Moving between the summary and panel keeps it open. Leaving their combined bounds closes it.

The panel contains:

- One row per reviewed node, showing a colored marker, elided node name, and right-aligned formatted size.
- A visible selection count and total size in the collector summary.
- The existing localized delete action, which continues to open the current confirmation flow.

The panel height is bounded by the analyzer's available client height. If not all rows fit, the mouse wheel scrolls the read-only list while the pointer is over the panel. Scrolling does not mutate or reorder the review.

The panel is not shown for an empty review. Recycling disables collector input and closes the panel.

## Components and Data Flow

### Pure Layout and Hit Testing

Add value types and pure functions in the analyzer UI module for:

- Collector summary bounds and panel bounds.
- Visible review-row layout for an item count and scroll offset.
- Collector/panel/row hit testing.
- Drag-threshold detection.

These functions are tested independently of Direct2D and Win32.

### Review Snapshot

`MainWindow` supplies `AnalyzerView` with the current reviewed node indices whenever `DeletionReview` changes or clears. `AnalyzerView` stores a bounded contiguous vector of node indices and obtains immutable names and logical sizes from the completed `ScanTree` during drawing.

No paths are constructed for hover rendering. Path construction remains deferred to the existing confirmation action.

### Pointer Commands

`AnalyzerView` owns potential-drag and active-drag state. It emits the existing `AddToReview` command only after a valid drop. `MainWindow` remains the sole owner of `DeletionReview`, overlap elimination, totals, confirmation, and recycling.

Win32 mouse capture is coordinated by `MainWindow` based on an analyzer drag-state query. Capture changes do not introduce filesystem or worker-thread work.

## Visual Design

The panel follows the supplied DaisyDisk reference in position and hierarchy but uses DiskBloom branding and existing theme tokens. It is anchored to the bottom collector, uses a maximum 5 DIP corner radius, and appears above the chart without changing layout dimensions.

Dark and light themes use shared tokens for panel surface, border, primary text, secondary text, hover rows, drag preview, collector highlight, and danger action. The full states are checked in both themes: normal, hover, active drag, valid drop, recycling-disabled, and empty.

All display strings come from the localization catalog. English and Simplified Chinese must fit without overlap. Dynamic file names are ellipsized; sizes remain right-aligned.

## Performance Constraints

- Pointer movement performs only arithmetic, bounded hit testing, and state comparison.
- No filesystem calls, path building, text allocation, or review-vector rebuilding occurs per pointer move.
- Review rows are contiguous and their layout is limited to visible capacity.
- The panel and drag preview are drawn in the existing Direct2D frame; no child windows or controls are created.
- A Release microbenchmark covers one million drag/collector hit-test iterations and records the result in the analyzer QA document.

## Error and Safety Behavior

- Invalid nodes and the scan root cannot start a drag.
- Dropping a node already covered by the review is a no-op through existing `DeletionReview::add` behavior.
- If a dragged node becomes invalid because the scan is replaced, the drag is cancelled before drawing or command dispatch.
- Recycle-in-progress state blocks new drags, drops, and delete commands.
- No interaction bypasses the localized confirmation dialog or changes the recycle-only policy.

## Test Strategy

Follow test-driven development for each behavior:

- Unit tests for collector/panel geometry at compact and large window sizes.
- Unit tests for review row capacity, clamped scrolling, and hit testing.
- Unit tests for the 6 DIP drag threshold and valid/invalid drop transitions.
- Analyzer interaction tests proving clicks retain existing behavior while drags suppress click commands.
- Tests proving chart and ranked rows both emit `AddToReview` only after release over the collector.
- Tests for empty, duplicate, root, recycling-disabled, pointer-leave, and capture-loss cases.
- Render smoke tests in dark/light and English/Simplified Chinese.
- Release benchmark for drag and collector hit testing.
- Manual comparison against the supplied reference image, including a long review list.

## Out of Scope

- Removing individual items from the hover panel.
- Dragging files into or out of DiskBloom through Windows Explorer/OLE.
- Reordering review items.
- Permanent deletion or deletion without confirmation.
