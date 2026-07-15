# DiskBloom 0.1.0 Release QA

Date: 2026-07-16, Asia/Shanghai.

## Release Scope

This build is a public preview of the local-drive workflow. It includes volume
and folder scanning, cancellation, sunburst navigation, ranked child rows,
Shell open/reveal actions, deletion review, and recycle-only deletion.

## Build Matrix

Commands:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug --output-on-failure

cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release --output-on-failure
cmake --build --preset windows-release --target package
```

The automated matrix covers:

- Unit and filesystem integration checks.
- D3D11/Direct2D device creation and presentation.
- Analyzer rendering in light/dark and en-US/zh-CN combinations.
- Real application smoke launches in all four theme/language combinations.
- Bundled MSVC runtime libraries with no separate redistributable install.

## Manual Integration Evidence

- A real `C:\` Release scan completed and entered the analyzer in about three
  seconds on the recorded host.
- The folder picker selected `feedback`, scanned its two files, and opened an
  analyzer whose root total matched 17.4 MB.
- A `D:\` folder scan was cancelled after publishing 47.7 GB of progress; the
  app remained on the overview and displayed the localized cancelled state.
- Explorer reveal opened the correct folder and retained application
  responsiveness.
- A uniquely named temporary file was moved to the Recycle Bin through the
  production `IFileOperation` adapter. A second fixture verified the same call
  through the asynchronous `RecycleSession`; both returned success and removed
  the original path.
- The folder picker cancel action returned to the responsive overview without
  starting a scan.

Generated screenshots and disposable fixtures live under ignored `build/qa/`.

## Package Verification

- CPack generator: ZIP.
- Package name: `DiskBloom-0.1.0-windows-x64.zip`.
- ZIP contents: one self-contained `DiskBloom.exe` under the package directory.
- The executable extracted to a clean QA directory and exited 0 with
  `--smoke-test`.
- The ZIP bundles the required `VCRUNTIME` and `MSVCP` libraries next to the
  executable, so no separate Visual C++ Redistributable install is required.
- Embedded file/product version: 0.1.0.
- Embedded manifest: PerMonitorV2, long-path aware, asInvoker.
- The original DiskBloom radial icon is generated deterministically by
  `tools/generate_icon.ps1` and embedded as the application icon.

## Final Performance Sample

Host and fixture match the baselines in `scan-engine.md` and
`sunburst-analyzer.md`.

- One-million-node scan-tree median: 36.62 ms; estimated memory 77,996,098
  bytes. This passes the recorded 36.75 ms same-host gate.
- One-million-node sunburst, 1,000 rebuilds: 6.10 ms median per rebuild after
  precomputing the parent sweep scale; maximum 1,465 segments.
- One million cached polar hit tests: 106.25 ms median.

Both sunburst medians pass the recorded 6.39 ms and 106.40 ms same-host gates.

## Known Release Gaps

- The package is unsigned and is not an MSIX.
- Theme/language settings are runtime-only.
- Full breadcrumb navigation and DaisyDisk-style animated fan transitions are
  not implemented.
- The custom-rendered interface does not yet expose a complete UI Automation
  accessibility tree.
