# Directory Transition Preference Design

## Goal

Make DaisyDisk-style directory transitions visible by default while retaining an explicit reduced-motion choice. The current implementation suppresses all transitions when Windows reports client-area animations as disabled; on the target machine that system flag is disabled, so navigation appears instantaneous.

## User Experience

The existing Settings menu gains a localized `Directory transitions` / `目录切换动画` submenu with three mutually exclusive choices:

- `Always on` / `始终开启`
- `Follow Windows` / `跟随 Windows`
- `Off` / `关闭`

`Always on` is the default for new installations and for installations without a saved animation preference. The active item is marked with the same native menu check treatment already used for theme and language. The menu must remain usable in English and Simplified Chinese and in both light and dark application themes.

Changing the setting applies immediately without restarting:

- `Always on` plays the existing 700 ms polar sunburst transition.
- `Follow Windows` plays the transition only when `SPI_GETCLIENTAREAANIMATION` is enabled.
- `Off` completes directory navigation immediately.

Changing from an enabled mode to `Off` while a transition is running completes the current transition immediately and stops the animation timer. Changing between enabled modes does not restart an active transition.

## State And Resolution

Add a strongly typed `DirectoryTransitionMode` with `AlwaysOn`, `FollowSystem`, and `Off` values to application settings. A pure resolver converts the selected mode and the current Windows animation flag into one boolean used by every navigation entry point. The UI must not query system policy independently.

The startup default is `AlwaysOn`. A valid persisted value replaces the default before the main window is shown. Command-line QA overrides may select all three modes without changing the saved value.

## Persistence

Persist the animation preference under the current user's local application data in a small versioned DiskBloom settings file. Write through a sibling temporary file and atomically replace the destination so an interrupted write cannot leave a partially written preference. Missing, unknown, or malformed values fall back to `AlwaysOn` and do not prevent startup.

Only the directory-transition preference is added to persistence in this change. Existing runtime behavior for theme and language remains unchanged; broad settings-storage migration is outside scope.

## Animation Behavior

This change does not replace the existing transition renderer. It makes the existing bounded 700 ms polar interpolation reachable according to the selected preference. Directory navigation through chart segments, breadcrumbs, parent navigation, and back/forward history must all use the same resolved policy.

The renderer retains its current 2,048-segment cap, 16 ms timer cadence, interruption snapshots, input suppression, palette crossfade, and destination-frame completion. No filesystem work is added to the UI thread.

## Localization And Theme Requirements

All menu labels come from `StringCatalog` entries in both languages. No feature colors are introduced; the native menu and existing semantic theme resources provide light/dark rendering. English and Chinese labels must not clip in the Settings submenu.

## Testing And Verification

Automated coverage must verify:

- The default mode is `AlwaysOn`.
- Mode resolution for all combinations of preference and Windows animation flag.
- Settings commands change only the transition preference.
- QA launch arguments select all three modes.
- Missing, malformed, and valid persisted values resolve correctly.
- Atomic-save failure preserves the prior valid file.
- Switching to `Off` stops an active transition and timer.
- All navigation entry points use the resolved preference.
- String catalogs are complete for English and Simplified Chinese.
- Existing dark/light and English/Chinese render smoke tests continue to pass.

Manual verification must run the Release build with Windows client-area animations disabled, confirm that the default still animates, then validate `Follow Windows` and `Off`. The existing transition benchmark gate remains at 8 ms P95 for 2,048 segments.

## Out Of Scope

- New transition styles or duration controls.
- Persisting or migrating theme and language settings.
- Changing the sunburst interpolation algorithm.
- Ignoring the explicit `Off` preference.
