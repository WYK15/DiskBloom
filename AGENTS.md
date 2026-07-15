# Project Development Rules

## 1. Theme Support

- Every feature and UI change must support both light mode and dark mode.
- Do not hard-code colors that only work in one theme. Use shared theme resources or design tokens.
- New UI must be checked in both themes, including normal, hover, pressed, selected, focused, disabled, loading, and error states.

## 2. Chinese and English Localization

- The application must support switching between Simplified Chinese and English in Settings.
- All user-facing text must come from localization resources. Do not hard-code display strings in views or business logic.
- Layouts must accommodate text-length differences between Chinese and English without clipping, overlap, or unintended resizing.
- New features are incomplete until both Chinese and English strings are provided and verified.

## 3. DaisyDisk Parity

- Features, workflows, interactions, animations, layout, spacing, and visual hierarchy should reproduce DaisyDisk as closely as practical, using the supplied screenshots and recordings as the primary references.
- When Windows behavior differs from macOS, preserve the DaisyDisk user experience while using correct Windows filesystem, permission, windowing, accessibility, and safety conventions.
- The public application must use its own product name, icon, brand assets, and other original identifiers. Do not copy DaisyDisk trademarks or proprietary assets.
- A feature is not complete until its behavior and UI have been compared against the relevant DaisyDisk reference material.

## 4. Performance First

- Performance is a primary product requirement, not a later optimization phase. Prefer algorithms, data layouts, APIs, and rendering techniques that minimize scan time, memory use, UI latency, and startup time.
- Performance-critical Windows functionality must be implemented in native C++. Use direct Win32, NTFS, Direct2D, DirectWrite, Direct3D, or DirectComposition APIs where they provide measurable advantages.
- Keep the UI thread free of filesystem traversal, aggregation, deletion, and other blocking work. Long-running operations must be asynchronous and cancellable.
- Avoid per-file heap allocations, heavyweight objects, unnecessary string copies, repeated filesystem queries, and unbounded queues in scan hot paths. Prefer contiguous storage, arenas, string interning, batching, and bounded message passing where appropriate.
- Do not create one native window or UI control per chart segment or filesystem node. Render large visualizations in batches and use mathematical hit testing.
- Every performance-sensitive feature must include a repeatable benchmark or profiling procedure. Optimization claims require measurements on representative datasets and Release builds.
- Changes that materially regress scan throughput, peak memory, startup time, rendering frame time, or interaction latency must not be accepted without an explicit documented trade-off.
