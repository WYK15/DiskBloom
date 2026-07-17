# Smaller Sunburst Design

## Goal

Reduce the analyzer sunburst radius to 80 percent of its original responsive
radius. This replaces the current 91-percent scale and makes the chart visibly
smaller without changing the surrounding analyzer composition.

## Behavior

- Keep the current responsive radius calculation as the unscaled baseline.
- Multiply the final baseline radius by `0.80F`.
- Preserve the current chart center, inner-radius ratio, ring distribution,
  animation behavior, and mathematical hit testing.
- Do not move or resize the header, breadcrumb, details list, or action bar.
- Apply identically in light and dark themes and in Chinese and English.

## Verification

- Update the analyzer layout test to assert an exact 80-percent radius at the
  representative 1200 x 720 DIP viewport.
- Keep the existing center, ring-width, containment, overlap, and hit-test
  assertions passing.
- Run the analyzer unit suite and deterministic render smoke test.

## Scope

This change only adjusts analyzer chart geometry. It does not change scan data,
review exclusions, navigation, localization strings, or transition durations.
