# DiskBloom

DiskBloom is a native Windows disk-space visualizer inspired by DaisyDisk's
core workflow. It uses an original name, icon, and implementation built with
C++20, Win32, Direct3D 11, DXGI, Direct2D, and DirectWrite.

Version 0.1.0 is a public preview with:

- Fixed and removable drive discovery.
- Cancellable background scans for drives and selected folders.
- A bounded GPU-rendered sunburst with drill-down, back navigation, and polar
  hit testing.
- A ranked, scrollable immediate-child list.
- System-associated open/preview and File Explorer reveal actions.
- Explicit deletion review, path confirmation, and asynchronous Recycle Bin
  execution with no permanent-delete fallback.
- Runtime light/dark/system themes and English/Simplified Chinese switching.
- Per-monitor V2 DPI awareness and long-path-aware filesystem access.
- A self-contained x64 ZIP package that bundles its required Visual C++
  runtime DLLs.

## Requirements

- Windows 10 22H2 or Windows 11, x64.
- Direct3D 11-capable graphics hardware.
- Visual Studio 2026 with Desktop development with C++ to build from source.

## Build And Test

Run from an x64 Visual Studio developer shell:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug --output-on-failure
```

For an optimized build and ZIP package:

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release --output-on-failure
cmake --build --preset windows-release --target package
```

Outputs:

- Executable: `build/windows-release/src/DiskBloom.exe`
- Package: `build/windows-release/DiskBloom-0.1.0-windows-x64.zip`

## QA Launch Options

The Settings menu changes theme and language at runtime. Visual QA can select
an initial combination:

```powershell
build/windows-release/src/DiskBloom.exe --theme=light --language=en-US
build/windows-release/src/DiskBloom.exe --theme=dark --language=zh-CN
```

Supported theme values are `system`, `light`, and `dark`. Supported language
values are `en-US` and `zh-CN`.

## Deletion Safety

Chart and list clicks never delete immediately. Add an item to the review
collection, open the delete action, verify the displayed absolute paths and
total size, and confirm the Recycle Bin operation. DiskBloom does not silently
fall back to permanent deletion.

## Preview Limits

- Navigation uses a current-root title and back command rather than a full
  clickable breadcrumb chain.
- Analyzer transitions do not yet reproduce DaisyDisk's animated fan motion.
- Theme and language choices are not yet persisted across restarts.
- The ZIP is unsigned; public production distribution still needs code signing
  and MSIX packaging to avoid SmartScreen friction.
- Cloud storage, network shares, duplicate detection, and direct NTFS MFT
  parsing are outside the current scope.

## Project Documents

- [V1 design](docs/superpowers/specs/2026-07-15-windows-disk-visualizer-design.md)
- [Scan engine QA](docs/qa/scan-engine.md)
- [Sunburst analyzer QA](docs/qa/sunburst-analyzer.md)
- [0.1.0 release QA](docs/qa/release-0.1.0.md)
