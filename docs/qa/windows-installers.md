# Windows Installer QA

Date: 2026-07-18, Asia/Shanghai.

## Release Contract

- Automatic trigger: GitHub `release.published`.
- Automatic tag: strict `vX.Y.Z`.
- Prerelease behavior: skip the packaging job.
- Manual input: strict `X.Y.Z`.
- Runner: `windows-2022`.
- Upload permission: repository-scoped `contents: write`.
- Manual runs retain an Actions artifact and do not call the Release upload
  step.
- Published Release runs upload only the three validated package paths with
  `gh release upload --clobber`.

The workflow passed `actionlint` 1.7.12 and the repository workflow contract
test. A remote manual run can be performed after the workflow reaches the
default branch; a published Release is required to exercise the final GitHub
asset API.

## Toolchain

- Visual Studio 2026 Developer Command Prompt 18.7.2.
- MSVC 19.51.36248.0, x64.
- CMake 4.3.1-msvc1.
- WiX Toolset 4.0.6 with WixToolset.UI.wixext 4.0.6.
- Inno Setup 6.4.3.
- Inno Setup installer Authenticode: valid, publisher `Pyrsys B.V.`.
- Inno Simplified Chinese resource: official `is-6_4_3` source,
  SHA-256 `DCECA9FEA16EA057A64012AC8744A1ECA7DC6CE3187D0C8991ACCBB539F2D4A0`.
  The vendored file is preserved byte-for-byte, including its upstream line
  ending whitespace.

## Build And Test

`packaging/windows/build-installers.ps1 -Version 0.1.0` completed the following
pipeline from clean build and output directories:

1. Configure and build x64 Release once.
2. Run the complete Release CTest suite: 12/12 passed.
3. Stage `DiskBloom.exe` and eight required MSVC runtime DLLs.
4. Build localized en-US and zh-CN MSIs from the staged payload.
5. Build one English/Simplified Chinese EXE from the same staged payload.
6. Reject missing or empty packages and calculate SHA-256 hashes.

Artifact evidence from the verified run:

| Package | Bytes | SHA-256 |
| --- | ---: | --- |
| `DiskBloom-0.1.0-windows-x64-en-US.msi` | 942080 | `83EC5585632F69CA8FBE89A00BE363B65781C0DDDDBA3E14C0C78BAE1938E767` |
| `DiskBloom-0.1.0-windows-x64-zh-CN.msi` | 933888 | `BBF1800E59542269B820E7EE6646B8140DFDD850912534CC187B83F60CAB4E4A` |
| `DiskBloom-0.1.0-windows-x64-setup.exe` | 2342894 | `EEA35F05872B09FE8F933C092ADD66E62B6CFBE84314C6B2581C13625491E882` |

MSI ProductCodes are stable for a given version and culture:

- en-US: `{A195C4BE-58F0-B5FC-3431-57E5907D9B5A}`
- zh-CN: `{1FFDD38B-50FD-33BA-14A7-CCD35B1A3E06}`

## Installation Scenarios

`packaging/windows/verify-installers.ps1` queried both MSI Feature tables and
verified the localized desktop option titles before installation. It then ran
four elevated install/uninstall scenarios:

1. en-US MSI default: application, Start menu link, and desktop link created;
   installed application smoke test passed; uninstall removed all three.
2. zh-CN MSI opt-out: application and Start menu link created; desktop link
   absent; installed application smoke test passed; uninstall removed the
   installation.
3. EXE Simplified Chinese default: application, Start menu link, and desktop
   link created; installed application smoke test passed; uninstall removed all
   three.
4. EXE English opt-out: application and Start menu link created; desktop link
   absent; installed application smoke test passed; uninstall removed the
   installation.

The verifier refuses to start if the target installation directory or shortcut
paths already exist. The completed run left no DiskBloom installation directory
or public desktop shortcut behind.

## Signing Status

The application, both MSIs, and EXE installer are unsigned. Code signing is a
separate future stage before Release upload; the current packages can trigger
Windows SmartScreen warnings.
