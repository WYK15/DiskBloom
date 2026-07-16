# Review Exclusion And Restore Design

Date: 2026-07-16
Status: Pending written review
References:

- `feedback/PixPin_2026-07-16_09-30-14.png`
- `feedback/iShot_2026-07-15_22.10.59.mp4`

## Goal

Bring DiskBloom's deletion collector closer to DaisyDisk in three related ways:

- Reduce the analyzer sunburst radius slightly so the chart has more space
  around its outer edge and above the collector.
- Remove collected files and directories from the active sunburst and ranked
  child list without mutating the completed scan tree.
- Let users restore a collected item by hovering its colored marker, clicking
  the replacement cross icon, and watching the item return to the analyzer.

Adding and restoring items use smooth, interruptible reflow animations. If the
currently browsed directory is collected, the analyzer first targets its
nearest visible parent and performs one combined transition. The scan root or
whole scanned volume remains impossible to collect.

## Chosen Approach

Keep `ScanTree` immutable and derive a sparse review-exclusion projection from
the current `DeletionReview`. The projection stores collected roots plus the
byte reductions applied to their ancestors. Sunburst layout and child ranking
consume the projection through explicit size and visibility queries.

This approach was selected over cloning a pruned tree or mutating the original
tree. It preserves stable node indices, paths, names, deletion targets, and
navigation history while avoiding a second million-node tree. Projection work
occurs only when the review collection changes; drawing, pointer movement, and
animation frames never traverse the filesystem.

## Analyzer Geometry

The analyzer's maximum chart radius is reduced by 9 percent after the existing
responsive width and height constraints are applied. The chart center, inner
radius ratio, band count, detail panel, header, and action bar anchors remain
unchanged. The result must preserve stable bounds at compact and default window
sizes and must not cause the chart, collector, or details list to overlap.

All hit testing continues to use the geometry returned by
`compute_analyzer_layout`; no separate visual-only scale is introduced.

## Review Exclusion Projection

Introduce a render-independent value type that consumes an immutable
`ScanTree` and the non-overlapping roots owned by `DeletionReview`. It provides:

- `is_excluded_root(node)`: true only for a directly collected root.
- `is_visible(node)`: false when the node or an ancestor is collected.
- `effective_size(node)`: original logical size minus collected descendants.
- An ordered view of ancestor byte reductions for deterministic queries and
  tests.

Building the projection walks the parent chain of each collected root and
accumulates its logical size at every visible ancestor. Collected roots are
sorted for bounded binary-search membership. Ancestor reductions are merged
into a sorted contiguous vector. `DeletionReview` already guarantees that
collected roots do not overlap, so subtraction does not double count bytes.
All subtraction is saturating to protect against malformed or inconsistent
input.

The projection does not mark every descendant individually. Layout traversal
stops at a collected root, and the less frequent navigation visibility query
walks ancestors. This keeps memory proportional to collected items and their
depth rather than total scan-tree size.

## Filtered Layout And Ranking

Sunburst layout gains a projection-aware entry point. For each child it:

- Skips directly collected roots and never traverses their descendants.
- Uses `effective_size` when assigning angular spans and aggregate sizes.
- Preserves the original node index and palette assignment for visible nodes.
- Returns an empty segment set when the current root has zero effective bytes.

Ranked child generation follows the same rules: collected roots are omitted,
remaining children are ordered by effective size, and the existing item limit
and deterministic tie breaking remain unchanged.

The original unfiltered APIs remain usable by tests and other callers. The
analyzer rebuild path always supplies the current projection, so its sunburst,
right-side list, center byte total, selection, hover pulse, drag hit testing,
and shell actions describe the same visible state.

## Add-To-Review Flow

When a chart segment, ranked row, or selected item emits `AddToReview`:

1. `MainWindow` validates and updates `DeletionReview`.
2. If nothing changed, it leaves the analyzer and timers untouched.
3. It builds the new exclusion projection.
4. If the current root is no longer visible, it chooses the nearest visible
   parent. The scan root is never eligible for collection.
5. `AnalyzerView` snapshots its current rendered transition frame or static
   layout, then builds the filtered destination layout and ranked list.
6. Breadcrumb and navigation state target the visible parent immediately while
   the chart and detail content animate to that destination.

If collecting a parent replaces reviewed descendants, the collector displays
only the parent and the projection is rebuilt once from the final normalized
review set.

## Restore Interaction

The collector panel retains one row per reviewed node. Each row layout adds a
minimum 28 by 28 DIP leading action bound while preserving non-overlapping name
and right-aligned size bounds.

- Normal: show the existing palette-colored circular marker.
- Row hover: show the semantic hover background and replace the marker with a
  familiar cross icon.
- Cross hover/press: use theme resources for visible feedback in dark and light
  modes; restoring is not styled as a destructive red action.
- Activation: emit `RestoreReviewItem` for that row's stable node index.

The cross receives localized accessibility/help text:

- English: `Restore item`
- Simplified Chinese: `恢复项目`

`MainWindow` calls the existing `DeletionReview::remove`, rebuilds the
projection, refreshes collector totals and rows, and starts the reverse layout
transition. Only the chosen collected root is restored. Restoring a collected
directory restores its complete subtree.

Clicking elsewhere in a collector row does not open, navigate, restore, or
delete the item. Scrolling and panel hover behavior remain unchanged.

## Animation

Collector reflow uses a 500 ms ease-in-out transition and extends the existing
bounded sunburst transition machinery:

- Matched visible segments interpolate angle and radial geometry.
- Removed segments contract toward their retained ancestor boundary and fade.
- Restored segments expand from their parent boundary and fade in.
- Ranked rows cross-fade and interpolate vertical position into the new order.
- The collector row appears or disappears in sync with the analyzer reflow.

The chart and ranked list do not accept navigation or new drag starts while
their geometry is moving. Collector cross actions remain available. A second
add or restore snapshots the current interpolated frame and builds a new target
from it, preventing a snap before the next transition.

The effective directory-animation preference controls collector reflow too:

- `Always on`: animate regardless of the Windows client-area animation flag.
- `Follow Windows`: follow the platform flag.
- `Off`: apply the destination immediately and do not start an animation timer.

The existing 16 ms animation timer remains the only frame timer. No filesystem
queries, tree mutation, path construction, or per-frame unbounded allocation is
allowed.

## Navigation And Selection

Navigation history keeps its original entries. Back, forward, and breadcrumb
activation dynamically skip destinations that are not visible in the current
projection. Restoring an item makes its retained history entries eligible
again.

When the selected node becomes hidden but the root remains visible, selection
falls back to the current root. When the current root becomes hidden, both root
and selection move to the nearest visible parent before the transition target
is published. Shell open/reveal and add-to-review commands are never issued for
a hidden selection.

If every child of a visible root is collected, the chart has no segments, the
ranked list is empty, and the center displays `0 B`. The collector remains
visible and interactive so items can be restored.

## Error And Safety Behavior

- Duplicate adds, invalid nodes, scan-root adds, and restores of absent nodes
  are no-ops and do not start animations.
- Recycling disables new drags, additions, restore crosses, scrolling changes,
  and deletion commands until the recycle session reaches a terminal state.
- Scan replacement clears the review, projection, active reflow, hover pulse,
  drag state, and collector hover state before installing the new tree.
- Collected items remain present in the immutable tree so confirmation can
  build their original paths and recycle exactly the reviewed roots.
- Actual deletion still requires the localized confirmation dialog and uses
  the Windows Recycle Bin only.

## Theme And Localization

All new row backgrounds, markers, cross states, disabled states, transition
opacity, empty-chart text, and focus visuals use existing semantic theme tokens
or new shared tokens defined for both light and dark themes. No view contains a
theme-specific literal color.

The only new user-facing text is the localized restore help/accessibility
label. Dynamic names retain DirectWrite ellipsis, and the leading action plus
size column remain stable for both English and Simplified Chinese layouts.

## Performance Requirements

- Projection storage is sparse, contiguous, deterministic, and proportional to
  collected roots plus touched ancestors.
- Projection rebuild performs no filesystem calls and no work on the UI thread
  beyond bounded in-memory aggregation, layout construction, and transition
  preparation.
- Pointer movement performs only current visible-row hit testing and state
  comparison; it never rebuilds the projection.
- Animation buffers are reserved from the bounded 2,048-segment target and
  reused across interrupted transitions.
- The Release projection benchmark uses a representative deep tree and
  multiple reviewed roots, reports average and p95 rebuild/query time plus a
  stable checksum, and enforces a documented regression gate.
- Existing sunburst layout, transition, collector, scan throughput, and peak
  memory baselines must not materially regress.

## Test Strategy

Follow test-driven development with observed RED failures for each behavior:

- Projection unit tests for file and directory roots, multiple branches,
  parent/descendant normalization, saturation, visibility, and effective size.
- Filtered sunburst and ranking tests proving collected nodes and descendants
  disappear while visible siblings receive correct angles and sizes.
- Deletion review and command tests for individual restore, absent restore,
  parent replacement, totals, and recycling-disabled behavior.
- Navigation tests for current-root fallback, selected-node fallback, dynamic
  history skipping, breadcrumb rejection, and history becoming valid after
  restore.
- Pure layout and hit-test tests for the 9 percent chart reduction, 28 DIP cross
  target, stable name/size bounds, compact windows, and half-open edges.
- Deterministic transition tests at start, midpoint, endpoint, interruption,
  animation-disabled, empty-chart, and add-current-directory cases.
- Render smoke coverage in dark/light and en-US/zh-CN for normal markers, hover
  crosses, collected exclusion, restore, empty visible root, and action states.
- Release benchmarks for projection rebuild/query and the existing layout,
  transition, collector, and hover-pulse gates.
- Visible Release QA against the supplied DaisyDisk recording and screenshot,
  with captured before, collected, restore-hover, midpoint, and restored frames.

## Out Of Scope

- Reordering collected items.
- Dragging collected items back onto the chart as an alternative restore input.
- Explorer/OLE drag-and-drop into or out of DiskBloom.
- Permanent deletion or bypassing confirmation.
- Persisting a collector across scans or application restarts.
