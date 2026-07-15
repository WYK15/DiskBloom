# Native Foundation QA

## Verification Date

2026-07-16, Asia/Shanghai.

## Environment

- CPU: AMD Ryzen 7 PRO 4750G with Radeon Graphics.
- Memory: 15.4 GB.
- Toolchain: MSVC 19.51.36248.0, Visual Studio Community 2026 18.7.2.
- Build: x64 Release, CMake 4.3.1-msvc1, Ninja 1.13.2.
- Display scale used for visual checks: 150%.

## Correctness

Commands:

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release --output-on-failure
```

Result: 3/3 CTest targets passed.

- `diskbloom_tests`: 20 unit checks across layout, command hit testing, theme contrast, bilingual resources, appearance switching, and volume capacity math.
- `diskbloom_render_smoke`: created D3D11, DXGI, Direct2D, and DirectWrite resources and presented one frame.
- `diskbloom_app_smoke`: created the real application window, enumerated volumes, rendered one frame, and exited successfully.

## Performance Baseline

These are baseline measurements, not release claims.

| Metric | Result |
|---|---:|
| Release executable size | 62,464 bytes |
| First process launch plus first frame | 1,037.02 ms |
| Median of 10 process launch plus first frame runs | 1,014.64 ms |
| Minimum of 10 runs | 1,005.58 ms |
| Idle working set after 5 seconds | 39.91 MB |
| Idle private bytes after 5 seconds | 37.12 MB |
| Synchronized resize average, 120 samples | 27.93 ms |
| Synchronized resize P95 | 50.75 ms |
| Synchronized resize maximum | 67.75 ms |

Startup was measured by launching `DiskBloom.exe --smoke-test`, waiting for exit, and timing the complete process. Resize measurements alternated window dimensions through `SetWindowPos` and called `DwmFlush` after every sample, so the result includes operating-system window management and desktop composition rather than only application draw time.

The resize baseline does not meet a 16.7 ms 60 Hz frame budget. It must not be described as meeting the rendering target. In-application CPU/GPU frame instrumentation is required before the animated sunburst is accepted.

## Visual Matrix

The disk overview was captured at 1200x720 physical pixels in all four combinations:

- Dark, English.
- Dark, Simplified Chinese.
- Light, English.
- Light, Simplified Chinese.

All combinations showed complete text, stable controls, non-overlapping progress tracks, and consistent row geometry. Captures are generated under `build/qa/` and are intentionally not committed.

Comparison with `feedback/ds_launch.png`:

- Preserved: centered product title, dense alternating disk rows, left drive identity, right usage track, right scan command, and bottom command bar.
- Windows adaptation: original Windows minimize, maximize, and close controls replace macOS traffic lights.
- Original assets: the current drive glyph is a generated geometric placeholder and does not copy DaisyDisk artwork.
- Incomplete: scan progress/cancel states, mounted image rows, cloud storage, and the analyzer view are not part of this foundation slice.

