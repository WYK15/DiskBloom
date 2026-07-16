# Analyzer Hover Pulse, Collector Hint, And Title-Bar Drag Design

## Goal

Close three interaction gaps against the supplied DaisyDisk reference: hovering a ranked child must visibly identify its matching sunburst branch, an empty deletion collector must explain its drag target instead of repeating the current directory name, and unused analyzer header space must drag the native window.

## Ranked-Child Hover Pulse

Hovering a visible row in the right-side immediate-child list identifies that row's `NodeIndex`. The analyzer locates the matching depth-zero sunburst segment and treats its angular interval as one branch. The highlight geometry contains that segment and every deeper segment fully inside the same interval, so a directory and its visible descendants pulse together. A file highlights only its own segment. If a ranked item was aggregated out of the bounded sunburst layout, the row still receives its normal hover state but no unrelated chart segment is highlighted.

The branch uses a soft continuous pulse with a 900 ms period. Opacity follows a smooth sine-based curve bounded between a subtle floor and a clear peak; it never changes segment geometry, layout, palette identity, selection, or hit testing. Entering a different row restarts the pulse at its low point. Leaving the right-side list, starting a directory transition, opening the deletion review panel over the list, or leaving the window stops the pulse immediately.

Pulse animation uses the existing effective animation preference. `Always on` animates even when Windows client-area animation is disabled; `Follow Windows` animates only when Windows enables it; `Off` uses a stable midpoint highlight. Reduced animation therefore preserves the row-to-chart association without movement.

## Rendering And Performance

The analyzer builds one cached Direct2D path geometry when the hovered row changes. Construction scans only the already bounded in-memory `SunburstLayout::segments` array, never the filesystem tree, and visits at most 2,048 segments. Per-frame work changes only one semantic overlay brush opacity and submits one additional `FillGeometry` call.

A dedicated 16 ms Win32 timer invalidates while a pulse is active. It does not rebuild geometry, allocate per frame, or run when no row is hovered. Directory-transition rendering takes priority and suspends the hover timer. The pulse resumes only after a later pointer move establishes a valid row hover.

The overlay color derives from existing semantic theme tokens and is rebuilt when light/dark theme resources change. No literal color is introduced. The highlight remains distinguishable in both themes without obscuring segment boundaries.

## Empty Collector Hint

When the deletion collector contains no items, the bottom-left summary region no longer displays `selectedNode_` or the current directory name. It displays one localized secondary-color line:

- Simplified Chinese: `将文件拖放至此，以收集需要删除的文件`
- English: `Drag files here to collect them for deletion`

The existing DirectWrite trimming behavior keeps the copy on one line and adds an ellipsis when space is constrained. The message disappears as soon as the collector contains an item, at which point the existing item count and collected byte total remain unchanged. Drag hover and valid-drop visual treatments continue to render behind the text.

## Native Header Dragging

`WM_NCHITTEST` retains resize-border priority, followed by real analyzer controls. The following header regions remain `HTCLIENT`:

- Back and forward buttons.
- Each currently visible breadcrumb pill.
- The breadcrumb ellipsis control.
- Minimize, maximize, and close buttons.

The breadcrumb rectangle itself is no longer treated as one large client control. Header background, gaps between breadcrumb pills, unused space after the last visible pill, and other noninteractive header pixels return `HTCAPTION`. This enables native window dragging and native title-bar double-click maximize behavior without manual capture logic. Breadcrumb flyout rows remain client content.

The hit-test decision is implemented as a pure geometry function that consumes current analyzer layout and breadcrumb layout. It performs no text measurement, allocation, rendering, or filesystem work.

## Localization And Theme Requirements

The collector hint is added to the English and Simplified Chinese string catalogs. The hover overlay uses semantic resources and is inspected in both light and dark themes. Layout verification covers 800x600 and 1200x720 viewports so both localized hint strings remain inside the summary region without overlap.

## Testing And Verification

Automated coverage must verify:

- A hovered immediate-child node maps only to its own depth-zero branch and descendants.
- Files, directories, aggregate-only items, and invalid nodes behave deterministically.
- The 900 ms pulse is periodic, bounded, and stable at a fixed time.
- Disabled animation returns a static highlight and requests no timer.
- Row changes restart the phase; pointer leave and transitions clear it.
- Header controls remain client hit targets while breadcrumb gaps and unused header pixels become caption targets.
- The collector hint is present in both string catalogs and current-directory text is absent from the empty summary path.
- Dark/light and en-US/zh-CN analyzer render smoke tests pass.
- The existing transition and collector performance gates remain unchanged.

Manual Release verification must hover files and folders in the right list, inspect the matching branch pulse in both themes, drag the window from multiple header gaps, click every breadcrumb control, and confirm the localized empty collector hint does not clip.

## Out Of Scope

- Pulsing chart segments when the pointer hovers the chart itself.
- Changing row sorting, sunburst aggregation, or palette assignment.
- Adding a separate pulse-speed or hover-animation preference.
- Allowing a drag gesture to begin directly on an interactive breadcrumb pill.
- Changing populated collector text or deletion behavior.
