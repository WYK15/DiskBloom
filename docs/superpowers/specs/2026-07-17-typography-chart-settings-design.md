# Typography And Chart Settings Design

## Goal

Add preset controls for application text size, font family, and analyzer chart
size to the existing Settings menu. Changes apply immediately, persist across
launches, and preserve the current light/dark and English/Simplified Chinese
behavior.

## Settings Menu

Add three checked submenus to the existing native popup menu:

- **Text size / 文字大小:** `80%`, `90%`, `100%`, `110%`, `120%`.
- **Font / 字体:** `Segoe UI Variable`, `Microsoft YaHei UI`, `Arial`,
  `Consolas`.
- **Chart size / 圆图大小:** `60%`, `70%`, `80%`, `90%`, `100%`.

The defaults are text size `100%`, font `Segoe UI Variable`, and chart size
`80%`. The chart default therefore preserves the current analyzer geometry.
Each submenu shows exactly one checked item. Every visible submenu and preset
label comes from the string catalog, including percentages and font family
names whose English and Chinese resource values are intentionally identical.

Selecting an item creates a candidate settings snapshot. The candidate is
saved atomically before becoming the active snapshot. A successful save
updates the relevant render resources and invalidates the window once. A
failed save leaves the prior setting active and displays the existing localized
action-failed message.

## Application Model

Extend `AppearanceSettings` with compact preset enums:

- `TextScalePreset` represents the five supported percentages.
- `FontFamilyPreset` represents the four supported font choices.
- `ChartScalePreset` represents the five supported percentages.

Conversion helpers expose validated scale factors and DirectWrite family names
without parsing strings during rendering. Settings commands modify only their
target field. Unknown commands and invalid persisted values do not mutate the
active settings.

The selected text scale applies to all DirectWrite text in the disk overview
and analyzer, including headings, details, breadcrumb navigation, rows, action
buttons, center labels, and collector hints. Layout boxes keep their existing
stable dimensions; trimming and ellipsis behavior prevent long English or
Chinese strings from resizing controls.

For the default font preset, overview display titles continue to use
`Segoe UI Variable Display` and other text uses `Segoe UI Variable Text`.
The other presets use their selected family for all application text.
DirectWrite font fallback remains enabled so Chinese glyphs remain renderable
when a selected Latin-oriented family lacks a glyph.

The chart scale is an input to `compute_analyzer_layout`. It multiplies the
existing responsive-radius baseline. The chart center, inner-radius ratio,
ring widths, hit testing, hover pulse, and directory transitions continue to
derive from the resulting geometry.

## Rendering And Performance

Typography settings become part of each view's DirectWrite resource cache key.
Changing font family, text scale, language, theme, or Direct2D context rebuilds
the cached resources. Drawing repeatedly with an unchanged settings snapshot
reuses them.

Chart scaling changes only layout arithmetic. It adds no allocation, filesystem
work, or control creation to the render loop. Presets are enums converted with
constant-time switches, and the menu does not enumerate installed fonts.

## Persistence And Migration

Store the complete settings snapshot in
`%LOCALAPPDATA%\DiskBloom\settings-v2.ini` using a deterministic text format:

```text
DiskBloomSettings/2
theme=system
language=en-US
directoryTransitions=always
textScale=100
fontFamily=segoe-variable
chartScale=80
```

The v2 loader validates each known field independently. Missing or invalid
fields use their documented defaults, and unknown fields are ignored for
forward compatibility. An invalid header rejects the file as a whole.

When v2 does not exist, startup reads the existing `settings-v1.ini` directory
transition preference and merges it into a default v2 snapshot. The new file
is created on the first subsequent settings change. Command-line QA arguments
continue to override loaded settings for that process without rewriting the
saved file.

Writes retain the existing temporary-file, flush, and atomic-replace behavior.
Failed writes preserve the last valid file.

## Localization And Themes

Add English and Simplified Chinese strings for Text size, Font, Chart size,
every percentage preset, and every font family preset. Percentage labels and
registered Windows font family names have identical values in both catalogs
but are still retrieved through the localization API. Settings controls use
the native checked-menu states in both themes, while rendered application text
continues to use shared theme tokens.

The layouts must remain usable at both text-scale extremes in both languages.
No view may hard-code a theme-specific color as part of this feature.

## Verification

Automated coverage includes:

- Defaults and every preset command, including isolation from unrelated fields.
- Rejection of unknown commands and invalid preset conversions.
- Complete v2 round trips, independent invalid-field fallback, invalid-header
  rejection, v1 migration, and atomic-write failure preservation.
- Analyzer layout geometry for every chart scale from `60%` through `100%`,
  including center, bounds, ring width, and mathematical hit testing.
- DirectWrite resource cache invalidation for changed typography and reuse for
  unchanged typography.
- String-catalog completeness for English and Simplified Chinese.
- Disk overview and analyzer render smoke tests in light/dark themes, both
  languages, and the minimum/maximum typography and chart scales.

Release QA captures representative combinations of `80%` and `120%` text,
all four font presets, and `60%` and `100%` chart sizes. Captures are checked
for clipping and overlap in breadcrumb navigation, action buttons, the child
list, and the deletion collector. Existing render-smoke timing provides a
repeatable check that unchanged settings add no recurring resource rebuild.

## Scope

This feature uses fixed presets in the existing Settings popup. It does not add
a separate settings window, arbitrary numeric inputs, installed-font
enumeration, per-view typography, or changes to scan and deletion behavior.
