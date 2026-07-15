# DiskBloom

DiskBloom is an early native Windows disk-space visualizer inspired by the core workflow of DaisyDisk. It is built with C++20, Win32, Direct3D 11, DXGI, Direct2D, and DirectWrite.

The current vertical slice provides:

- Native fixed/removable volume discovery.
- A GPU-rendered disk overview.
- Runtime light, dark, English, and Simplified Chinese UI.
- Per-monitor DPI-aware custom window rendering.
- Debug and Release test presets.

Filesystem scanning, the interactive sunburst, deletion review, and packaging are under active development.

## Requirements

- Windows 10 22H2 or Windows 11, x64.
- Visual Studio 2026 with Desktop development with C++.
- CMake and Ninja from the Visual Studio installation.

## Build

Run from an x64 Visual Studio developer shell:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug --output-on-failure
```

For an optimized build:

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release --output-on-failure
```

The executable is written to `build/windows-release/src/DiskBloom.exe`.

## QA Launch Options

The Settings menu changes theme and language at runtime. Repeatable visual QA can also select an initial combination:

```powershell
build/windows-release/src/DiskBloom.exe --theme=light --language=en-US
build/windows-release/src/DiskBloom.exe --theme=dark --language=zh-CN
```

Supported theme values are `system`, `light`, and `dark`. Supported language values are `en-US` and `zh-CN`.

## Project Documents

- [V1 design](docs/superpowers/specs/2026-07-15-windows-disk-visualizer-design.md)
- [Native foundation plan](docs/superpowers/plans/2026-07-15-native-foundation.md)
- [Native foundation QA](docs/qa/native-foundation.md)

