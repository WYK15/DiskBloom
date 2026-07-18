# Windows Release Installers Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build localized Windows x64 MSI installers and one multilingual EXE installer, verify them, and attach all three to every published GitHub Release.

**Architecture:** CMake owns the application version and staged native payload. WiX Toolset v4 consumes that payload twice to produce en-US and zh-CN MSIs; Inno Setup consumes it once to produce a multilingual EXE. A PowerShell packaging module enforces the release contract, a driver performs the deterministic build, and one GitHub Actions workflow either uploads Release assets or retains manual-run artifacts.

**Tech Stack:** C++20, CMake, MSVC x64 Release, PowerShell 7, WiX Toolset 4.0.6 with WixToolset.UI.wixext 4.0.6, Inno Setup 6.4.3, GitHub Actions, GitHub CLI.

## Global Constraints

- Automatic release tags must match `vX.Y.Z`; manual versions must match `X.Y.Z`.
- Publish exactly `DiskBloom-X.Y.Z-windows-x64-en-US.msi`, `DiskBloom-X.Y.Z-windows-x64-zh-CN.msi`, and `DiskBloom-X.Y.Z-windows-x64-setup.exe`.
- All installers are x64, per-machine, elevated, and install under `Program Files\DiskBloom`.
- All installers add a Start menu shortcut and offer a default-enabled desktop shortcut.
- Installer text must exist in English and Simplified Chinese; the EXE selects the Windows UI language at startup.
- Both application themes remain supported; packaging must not change runtime theme behavior.
- The application and installers remain unsigned in this iteration.
- The application is compiled once in Release mode and all three packages consume the same staged payload.
- A package is never uploaded unless the complete Release CTest suite and installer smoke checks pass.
- Keep scan, rendering, startup, and interaction hot paths unchanged; packaging work must add no runtime abstraction or dependency.

---

### Task 1: Make The CMake Version The Single Source Of Truth

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `src/CMakeLists.txt`
- Rename: `src/resources/diskbloom.rc` to `src/resources/diskbloom.rc.in`
- Create: `tests/release_version_tests.ps1`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `-DDISKBLOOM_VERSION=X.Y.Z` at configure time.
- Produces: `PROJECT_VERSION`, CPack names, and `DiskBloom.exe` file/product metadata with the same `X.Y.Z` value.

- [x] **Step 1: Add a metadata assertion script and CTest entry**

Create `tests/release_version_tests.ps1`:

```powershell
param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,

    [Parameter(Mandatory = $true)]
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string]$ExpectedVersion
)

$ErrorActionPreference = 'Stop'
$versionInfo = (Get-Item -LiteralPath $Executable).VersionInfo

if ($versionInfo.FileVersion -ne $ExpectedVersion) {
    throw "FileVersion '$($versionInfo.FileVersion)' did not equal '$ExpectedVersion'."
}
if ($versionInfo.ProductVersion -ne $ExpectedVersion) {
    throw "ProductVersion '$($versionInfo.ProductVersion)' did not equal '$ExpectedVersion'."
}
```

Add to `tests/CMakeLists.txt`:

```cmake
add_test(
    NAME diskbloom_release_version_metadata
    COMMAND
        powershell
        -NoProfile
        -ExecutionPolicy Bypass
        -File ${CMAKE_CURRENT_SOURCE_DIR}/release_version_tests.ps1
        -Executable $<TARGET_FILE:DiskBloom>
        -ExpectedVersion ${PROJECT_VERSION}
)
```

- [x] **Step 2: Run an override build to prove the metadata test fails**

Run from a Visual Studio developer shell:

```powershell
cmake -S . -B build/version-red -G Ninja -DCMAKE_BUILD_TYPE=Release -DDISKBLOOM_VERSION=9.8.7
cmake --build build/version-red --target DiskBloom
ctest --test-dir build/version-red -C Release -R diskbloom_release_version_metadata --output-on-failure
```

Expected: FAIL because the current resource still reports `0.1.0`.

- [x] **Step 3: Validate the cache version before `project()`**

Replace the fixed project declaration in `CMakeLists.txt` with:

```cmake
set(DISKBLOOM_VERSION "0.1.0" CACHE STRING "DiskBloom semantic version")
if(NOT DISKBLOOM_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "DISKBLOOM_VERSION must match X.Y.Z")
endif()

project(DiskBloom VERSION ${DISKBLOOM_VERSION} LANGUAGES CXX)
```

- [x] **Step 4: Generate the Windows resource from CMake version values**

Rename the resource file to `diskbloom.rc.in`. Replace every numeric and string version literal with:

```rc
 FILEVERSION @PROJECT_VERSION_MAJOR@,@PROJECT_VERSION_MINOR@,@PROJECT_VERSION_PATCH@,0
 PRODUCTVERSION @PROJECT_VERSION_MAJOR@,@PROJECT_VERSION_MINOR@,@PROJECT_VERSION_PATCH@,0
```

and:

```rc
            VALUE "FileVersion", "@PROJECT_VERSION@\0"
            VALUE "ProductVersion", "@PROJECT_VERSION@\0"
```

In `src/CMakeLists.txt`, generate the resource before `add_executable` and use the generated path:

```cmake
set(DISKBLOOM_GENERATED_RESOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/diskbloom.rc")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(
    resources/diskbloom.rc.in
    "${DISKBLOOM_GENERATED_RESOURCE}"
    @ONLY
)
```

Keep the icon reference valid from the generated directory by writing its absolute CMake-substituted source path in the template:

```rc
IDI_DISKBLOOM ICON "@CMAKE_CURRENT_SOURCE_DIR@/resources/diskbloom_icon.ico"
```

Replace `resources/diskbloom.rc` in `add_executable(DiskBloom ...)` with `${DISKBLOOM_GENERATED_RESOURCE}`.

- [x] **Step 5: Run version validation in both positive and negative modes**

Run:

```powershell
cmake -S . -B build/version-green -G Ninja -DCMAKE_BUILD_TYPE=Release -DDISKBLOOM_VERSION=9.8.7
cmake --build build/version-green --target DiskBloom
ctest --test-dir build/version-green -C Release -R diskbloom_release_version_metadata --output-on-failure
cmake -S . -B build/version-invalid -G Ninja -DDISKBLOOM_VERSION=9.8
```

Expected: metadata test PASS; invalid configure exits nonzero with `DISKBLOOM_VERSION must match X.Y.Z`.

- [x] **Step 6: Commit**

```powershell
git add CMakeLists.txt src/CMakeLists.txt src/resources/diskbloom.rc.in tests/CMakeLists.txt tests/release_version_tests.ps1
git commit -m "build: make release version configurable"
```

---

### Task 2: Encode And Test The Release Artifact Contract

**Files:**
- Create: `packaging/windows/Packaging.psm1`
- Create: `packaging/windows/tests/packaging_tests.ps1`

**Interfaces:**
- Produces: `ConvertFrom-DiskBloomReleaseTag(string) -> string`, `Assert-DiskBloomVersion(string)`, `Get-DiskBloomPackagePaths(string, string) -> PSCustomObject`, and `Assert-DiskBloomPackageSet(PSCustomObject) -> object[]`.
- Consumes: no build tools; these functions are pure except for final file validation.

- [x] **Step 1: Write contract tests first**

Create `packaging/windows/tests/packaging_tests.ps1` that imports the module and performs these assertions:

```powershell
$ErrorActionPreference = 'Stop'
$module = Join-Path $PSScriptRoot '..\Packaging.psm1'
Import-Module $module -Force

function Assert-Equal($Expected, $Actual, [string]$Message) {
    if ($Expected -ne $Actual) { throw "$Message Expected '$Expected', got '$Actual'." }
}

function Assert-Throws([scriptblock]$Action, [string]$Message) {
    try { & $Action } catch { return }
    throw $Message
}

Assert-Equal '1.2.3' (ConvertFrom-DiskBloomReleaseTag 'v1.2.3') 'Tag conversion failed.'
Assert-Throws { ConvertFrom-DiskBloomReleaseTag '1.2.3' } 'Missing v prefix was accepted.'
Assert-Throws { ConvertFrom-DiskBloomReleaseTag 'v1.2.3-beta.1' } 'Prerelease tag was accepted.'
Assert-Throws { Assert-DiskBloomVersion 'v1.2.3' } 'Manual version accepted a v prefix.'
Assert-Equal `
    (Get-DiskBloomMsiProductCode -Version '1.2.3' -Culture 'en-US') `
    (Get-DiskBloomMsiProductCode -Version '1.2.3' -Culture 'en-US') `
    'MSI ProductCode is not repeatable.'

$paths = Get-DiskBloomPackagePaths -Version '1.2.3' -OutputDirectory 'C:\packages'
Assert-Equal 'DiskBloom-1.2.3-windows-x64-en-US.msi' (Split-Path $paths.EnglishMsi -Leaf) 'English MSI name drifted.'
Assert-Equal 'DiskBloom-1.2.3-windows-x64-zh-CN.msi' (Split-Path $paths.ChineseMsi -Leaf) 'Chinese MSI name drifted.'
Assert-Equal 'DiskBloom-1.2.3-windows-x64-setup.exe' (Split-Path $paths.SetupExe -Leaf) 'EXE name drifted.'

$temp = Join-Path ([IO.Path]::GetTempPath()) "diskbloom-package-tests-$([guid]::NewGuid())"
New-Item -ItemType Directory -Path $temp | Out-Null
try {
    $fixture = Get-DiskBloomPackagePaths -Version '1.2.3' -OutputDirectory $temp
    Assert-Throws { Assert-DiskBloomPackageSet $fixture } 'Missing files were accepted.'
    'msi-en' | Set-Content -LiteralPath $fixture.EnglishMsi -NoNewline
    'msi-zh' | Set-Content -LiteralPath $fixture.ChineseMsi -NoNewline
    'setup' | Set-Content -LiteralPath $fixture.SetupExe -NoNewline
    $result = @(Assert-DiskBloomPackageSet $fixture)
    Assert-Equal 3 $result.Count 'Package validation did not return three hashes.'
} finally {
    Remove-Item -LiteralPath $temp -Recurse -Force
}
```

- [x] **Step 2: Run the tests to prove the module is missing**

Run:

```powershell
pwsh -NoProfile -File packaging/windows/tests/packaging_tests.ps1
```

Expected: FAIL because `Packaging.psm1` does not exist.

- [x] **Step 3: Implement the release contract module**

Create `Packaging.psm1` with strict mode, anchored regular expressions, absolute output paths, ordered package properties, non-empty file checks, and SHA-256 results:

```powershell
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function ConvertFrom-DiskBloomReleaseTag {
    param([Parameter(Mandatory)][string]$Tag)
    if ($Tag -notmatch '^v(?<Version>\d+\.\d+\.\d+)$') {
        throw "Release tag '$Tag' must match vX.Y.Z."
    }
    return $Matches.Version
}

function Assert-DiskBloomVersion {
    param([Parameter(Mandatory)][string]$Version)
    if ($Version -notmatch '^\d+\.\d+\.\d+$') {
        throw "Version '$Version' must match X.Y.Z."
    }
}

function Get-DiskBloomPackagePaths {
    param(
        [Parameter(Mandatory)][string]$Version,
        [Parameter(Mandatory)][string]$OutputDirectory
    )
    Assert-DiskBloomVersion $Version
    $root = [IO.Path]::GetFullPath($OutputDirectory)
    return [pscustomobject][ordered]@{
        EnglishMsi = Join-Path $root "DiskBloom-$Version-windows-x64-en-US.msi"
        ChineseMsi = Join-Path $root "DiskBloom-$Version-windows-x64-zh-CN.msi"
        SetupExe = Join-Path $root "DiskBloom-$Version-windows-x64-setup.exe"
    }
}

function Get-DiskBloomMsiProductCode {
    param(
        [Parameter(Mandatory)][string]$Version,
        [Parameter(Mandatory)][string]$Culture
    )
    Assert-DiskBloomVersion $Version
    if ($Culture -notin @('en-US', 'zh-CN')) {
        throw "Unsupported MSI culture '$Culture'."
    }
    $seed = "3FE244EB-3C40-4A15-816C-7E4F0994D9D5|$Version|$Culture"
    $sha256 = [Security.Cryptography.SHA256]::Create()
    try {
        $hash = $sha256.ComputeHash([Text.Encoding]::UTF8.GetBytes($seed))
    } finally {
        $sha256.Dispose()
    }
    $hex = ([BitConverter]::ToString($hash, 0, 16)).Replace('-', '')
    return ([guid]::ParseExact($hex, 'N')).ToString('B').ToUpperInvariant()
}

function Assert-DiskBloomPackageSet {
    param([Parameter(Mandatory)]$Paths)
    foreach ($path in @($Paths.EnglishMsi, $Paths.ChineseMsi, $Paths.SetupExe)) {
        $item = Get-Item -LiteralPath $path -ErrorAction Stop
        if ($item.Length -le 0) { throw "Package '$path' is empty." }
        Get-FileHash -Algorithm SHA256 -LiteralPath $path
    }
}

Export-ModuleMember -Function ConvertFrom-DiskBloomReleaseTag, Assert-DiskBloomVersion, Get-DiskBloomPackagePaths, Get-DiskBloomMsiProductCode, Assert-DiskBloomPackageSet
```

- [x] **Step 4: Run the contract tests**

Run: `pwsh -NoProfile -File packaging/windows/tests/packaging_tests.ps1`

Expected: exit 0 with no assertion failure.

- [x] **Step 5: Commit**

```powershell
git add packaging/windows/Packaging.psm1 packaging/windows/tests/packaging_tests.ps1
git commit -m "build: define Windows release artifact contract"
```

---

### Task 3: Build Localized Per-Machine MSI Packages

**Files:**
- Create: `packaging/windows/wix/Product.wxs`
- Create: `packaging/windows/wix/Product.en-US.wxl`
- Create: `packaging/windows/wix/Product.zh-CN.wxl`

**Interfaces:**
- Consumes WiX defines: `ProductVersion`, `StageDir`, and `IconPath`.
- Produces one en-US MSI and one zh-CN MSI from identical staged files.
- Stable identifiers: upgrade `{3FE244EB-3C40-4A15-816C-7E4F0994D9D5}`, Start menu component `{0C5F7299-9120-483A-B7A2-B7864FDA9BBF}`, desktop component `{279A285A-A1D9-4068-9E08-EFFF4FDE91D8}`.

- [x] **Step 1: Create localization files before the product source**

Create matching WiX v4 `.wxl` files. Use culture `en-US`, codepage `1252`, LCID `1033` for English; use culture `zh-CN`, codepage `936`, LCID `2052` for Chinese. Each file defines these exact IDs:

```xml
<String Id="ProductLanguage" Value="1033" />
<String Id="DesktopShortcutTitle" Value="Create a desktop shortcut" />
<String Id="DesktopShortcutDescription" Value="Adds a DiskBloom shortcut to the desktop." />
<String Id="DowngradeError" Value="A newer version of DiskBloom is already installed." />
```

and:

```xml
<String Id="ProductLanguage" Value="2052" />
<String Id="DesktopShortcutTitle" Value="是否在桌面建立快捷方式" />
<String Id="DesktopShortcutDescription" Value="在桌面添加 DiskBloom 快捷方式。" />
<String Id="DowngradeError" Value="已安装更高版本的 DiskBloom。" />
```

- [x] **Step 2: Prove the WiX build has no product source yet**

Run:

```powershell
wix build packaging/windows/wix/Product.wxs -arch x64 -ext WixToolset.UI.wixext -culture en-US -loc packaging/windows/wix/Product.en-US.wxl -out build/packages/red.msi
```

Expected: FAIL because `Product.wxs` does not exist.

- [x] **Step 3: Author the per-machine product and shortcut features**

Create `Product.wxs` with:

```xml
<?xml version="1.0" encoding="utf-8"?>
<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs"
     xmlns:ui="http://wixtoolset.org/schemas/v4/wxs/ui">
  <Package Name="DiskBloom"
           Manufacturer="DiskBloom Contributors"
           Version="$(ProductVersion)"
           ProductCode="$(ProductCode)"
           Language="!(loc.ProductLanguage)"
           UpgradeCode="{3FE244EB-3C40-4A15-816C-7E4F0994D9D5}"
           Scope="perMachine"
           Compressed="yes">
    <MajorUpgrade DowngradeErrorMessage="!(loc.DowngradeError)" />
    <MediaTemplate EmbedCab="yes" CompressionLevel="high" />

    <Icon Id="DiskBloomIcon" SourceFile="$(IconPath)" />
    <Property Id="ARPPRODUCTICON" Value="DiskBloomIcon" />

    <StandardDirectory Id="ProgramFiles64Folder">
      <Directory Id="INSTALLFOLDER" Name="DiskBloom" />
    </StandardDirectory>
    <StandardDirectory Id="ProgramMenuFolder">
      <Directory Id="PROGRAMMENUFOLDER" Name="DiskBloom" />
    </StandardDirectory>
    <StandardDirectory Id="DesktopFolder" />

    <Feature Id="MainFeature" Title="DiskBloom" Level="1" Display="expand" AllowAbsent="no">
      <ComponentGroupRef Id="AppPayload" />
      <ComponentRef Id="StartMenuShortcutComponent" />
      <Feature Id="DesktopShortcutFeature"
               Title="!(loc.DesktopShortcutTitle)"
               Description="!(loc.DesktopShortcutDescription)"
               Level="1"
               AllowAbsent="yes">
        <ComponentRef Id="DesktopShortcutComponent" />
      </Feature>
    </Feature>

    <ComponentGroup Id="AppPayload" Directory="INSTALLFOLDER">
      <Component Id="DiskBloomExecutableComponent" Guid="*">
        <File Id="DiskBloomExecutable" Source="$(StageDir)\DiskBloom.exe" KeyPath="yes" />
      </Component>
      <Component Id="ConcrtRuntimeComponent" Guid="*">
        <File Id="ConcrtRuntime" Source="$(StageDir)\concrt140.dll" KeyPath="yes" />
      </Component>
      <Component Id="MsvcRuntimeComponent" Guid="*">
        <File Id="MsvcRuntime" Source="$(StageDir)\msvcp140.dll" KeyPath="yes" />
      </Component>
      <Component Id="MsvcRuntime1Component" Guid="*">
        <File Id="MsvcRuntime1" Source="$(StageDir)\msvcp140_1.dll" KeyPath="yes" />
      </Component>
      <Component Id="MsvcRuntime2Component" Guid="*">
        <File Id="MsvcRuntime2" Source="$(StageDir)\msvcp140_2.dll" KeyPath="yes" />
      </Component>
      <Component Id="MsvcAtomicWaitComponent" Guid="*">
        <File Id="MsvcAtomicWait" Source="$(StageDir)\msvcp140_atomic_wait.dll" KeyPath="yes" />
      </Component>
      <Component Id="MsvcCodecvtIdsComponent" Guid="*">
        <File Id="MsvcCodecvtIds" Source="$(StageDir)\msvcp140_codecvt_ids.dll" KeyPath="yes" />
      </Component>
      <Component Id="VcRuntimeComponent" Guid="*">
        <File Id="VcRuntime" Source="$(StageDir)\vcruntime140.dll" KeyPath="yes" />
      </Component>
      <Component Id="VcRuntime1Component" Guid="*">
        <File Id="VcRuntime1" Source="$(StageDir)\vcruntime140_1.dll" KeyPath="yes" />
      </Component>
    </ComponentGroup>

    <Component Id="StartMenuShortcutComponent"
               Directory="PROGRAMMENUFOLDER"
               Guid="{0C5F7299-9120-483A-B7A2-B7864FDA9BBF}">
      <Shortcut Id="StartMenuShortcut"
                Name="DiskBloom"
                Target="[INSTALLFOLDER]DiskBloom.exe"
                WorkingDirectory="INSTALLFOLDER" />
      <RemoveFolder Id="RemoveProgramMenuFolder" On="uninstall" />
      <RegistryValue Root="HKLM"
                     Key="Software\DiskBloom"
                     Name="StartMenuShortcut"
                     Type="integer"
                     Value="1"
                     KeyPath="yes" />
    </Component>

    <Component Id="DesktopShortcutComponent"
               Directory="DesktopFolder"
               Guid="{279A285A-A1D9-4068-9E08-EFFF4FDE91D8}">
      <Shortcut Id="DesktopShortcut"
                Name="DiskBloom"
                Target="[INSTALLFOLDER]DiskBloom.exe"
                WorkingDirectory="INSTALLFOLDER" />
      <RegistryValue Root="HKLM"
                     Key="Software\DiskBloom"
                     Name="DesktopShortcut"
                     Type="integer"
                     Value="1"
                     KeyPath="yes" />
    </Component>

    <ui:WixUI Id="WixUI_FeatureTree" />
    <UIRef Id="WixUI_ErrorProgressText" />
  </Package>
</Wix>
```

- [x] **Step 4: Stage once and build both cultures**

Run after installing WiX 4.0.6 and its UI extension:

```powershell
cmake --install build/windows-release --prefix build/installer-stage --config Release
$stage = (Resolve-Path build/installer-stage).Path
$icon = (Resolve-Path src/resources/diskbloom_icon.ico).Path
New-Item -ItemType Directory build/packages -Force | Out-Null
wix build packaging/windows/wix/Product.wxs -arch x64 -ext WixToolset.UI.wixext -culture en-US -loc packaging/windows/wix/Product.en-US.wxl -pdbtype none -d ProductVersion=0.1.0 -d ProductCode={A195C4BE-58F0-B5FC-3431-57E5907D9B5A} -d StageDir=$stage -d IconPath=$icon -out build/packages/DiskBloom-0.1.0-windows-x64-en-US.msi
wix build packaging/windows/wix/Product.wxs -arch x64 -ext WixToolset.UI.wixext -culture zh-CN -loc packaging/windows/wix/Product.zh-CN.wxl -pdbtype none -d ProductVersion=0.1.0 -d ProductCode={1FFDD38B-50FD-33BA-14A7-CCD35B1A3E06} -d StageDir=$stage -d IconPath=$icon -out build/packages/DiskBloom-0.1.0-windows-x64-zh-CN.msi
```

Expected: both commands exit 0 and produce non-empty MSI files.

- [x] **Step 5: Commit**

```powershell
git add packaging/windows/wix
git commit -m "build: add localized MSI packages"
```

---

### Task 4: Build The Multilingual EXE Installer

**Files:**
- Create: `packaging/windows/inno/DiskBloom.iss`
- Create: `packaging/windows/inno/languages/ChineseSimplified.isl`

**Interfaces:**
- Consumes Inno preprocessor defines: `AppVersion`, `StageDir`, `OutputDir`, and `IconPath`.
- Produces `DiskBloom-X.Y.Z-windows-x64-setup.exe` with English and Simplified Chinese messages.

- [x] **Step 1: Prove the Inno source is absent**

Run:

```powershell
& 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe' packaging/windows/inno/DiskBloom.iss
```

Expected: FAIL because `DiskBloom.iss` does not exist.

- [x] **Step 2: Author the x64 per-machine installer**

Create `DiskBloom.iss` with:

```ini
#ifndef AppVersion
  #error AppVersion is required
#endif
#ifndef StageDir
  #error StageDir is required
#endif
#ifndef OutputDir
  #error OutputDir is required
#endif
#ifndef IconPath
  #error IconPath is required
#endif

[Setup]
AppId={{8DD36945-084D-4BDB-B871-AB457D25DFF9}
AppName=DiskBloom
AppVersion={#AppVersion}
AppPublisher=DiskBloom Contributors
DefaultDirName={autopf}\DiskBloom
DefaultGroupName=DiskBloom
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir={#OutputDir}
OutputBaseFilename=DiskBloom-{#AppVersion}-windows-x64-setup
SetupIconFile={#IconPath}
UninstallDisplayIcon={app}\DiskBloom.exe
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ChangesAssociations=no
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "chinesesimplified"; MessagesFile: "languages\ChineseSimplified.isl"

[CustomMessages]
english.CreateDesktopShortcut=Create a desktop shortcut
chinesesimplified.CreateDesktopShortcut=是否在桌面建立快捷方式

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopShortcut}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\DiskBloom"; Filename: "{app}\DiskBloom.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\DiskBloom"; Filename: "{app}\DiskBloom.exe"; WorkingDir: "{app}"; Tasks: desktopicon
```

- [x] **Step 3: Build the multilingual EXE**

Run:

```powershell
$stage = (Resolve-Path build/installer-stage).Path
$out = [IO.Path]::GetFullPath('build/packages')
$icon = (Resolve-Path src/resources/diskbloom_icon.ico).Path
& 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe' "/DAppVersion=0.1.0" "/DStageDir=$stage" "/DOutputDir=$out" "/DIconPath=$icon" packaging/windows/inno/DiskBloom.iss
```

Expected: exit 0 and a non-empty `DiskBloom-0.1.0-windows-x64-setup.exe`.

- [x] **Step 4: Commit**

```powershell
git add packaging/windows/inno/DiskBloom.iss
git commit -m "build: add multilingual EXE installer"
```

---

### Task 5: Orchestrate Packaging And Verify Real Install Behavior

**Files:**
- Create: `packaging/windows/build-installers.ps1`
- Create: `packaging/windows/verify-installers.ps1`

**Interfaces:**
- `build-installers.ps1 -Version X.Y.Z -WixPath path -InnoCompilerPath path [-BuildDirectory path] [-OutputDirectory path]` produces the contract paths and exits nonzero on any failed stage.
- `verify-installers.ps1 -Version X.Y.Z -OutputDirectory path` installs and removes all three packages, checking payload and shortcut behavior.

- [x] **Step 1: Add a driver test using an invalid version**

Run before creating the driver:

```powershell
pwsh -NoProfile -File packaging/windows/build-installers.ps1 -Version v1.2.3 -WixPath wix -InnoCompilerPath iscc
```

Expected: FAIL because the driver is absent.

- [x] **Step 2: Implement the packaging driver in fixed pipeline order**

The driver must:

```powershell
param(
    [Parameter(Mandatory)][string]$Version,
    [Parameter(Mandatory)][string]$WixPath,
    [Parameter(Mandatory)][string]$InnoCompilerPath,
    [string]$BuildDirectory = 'build/release-installers',
    [string]$OutputDirectory = 'build/release-packages'
)
```

Import `Packaging.psm1`, call `Assert-DiskBloomVersion` before creating any directory, resolve all paths, and execute this exact sequence with an `Invoke-Checked` helper that throws on nonzero exit codes:

```powershell
cmake -S $repoRoot -B $build -G 'Visual Studio 17 2022' -A x64 -DDISKBLOOM_VERSION=$Version -DCMAKE_BUILD_TYPE=Release
cmake --build $build --config Release --parallel
ctest --test-dir $build -C Release --output-on-failure
cmake --install $build --prefix $stage --config Release
```

Assert that `$stage\DiskBloom.exe` exists, then invoke WiX twice with `-arch x64`, `-ext WixToolset.UI.wixext`, `-pdbtype none`, the matching `-culture` and `-loc`, and the shared defines. Invoke Inno once with the four `/D` defines. End by returning `Assert-DiskBloomPackageSet` results so paths, sizes, and SHA-256 hashes appear in the log.

- [x] **Step 3: Run invalid and valid driver paths**

Run:

```powershell
pwsh -NoProfile -File packaging/windows/build-installers.ps1 -Version v1.2.3 -WixPath wix -InnoCompilerPath iscc
pwsh -NoProfile -File packaging/windows/build-installers.ps1 -Version 0.1.0 -WixPath $wix -InnoCompilerPath $iscc
```

Expected: first exits nonzero before build; second runs all Release tests and produces three validated packages.

- [x] **Step 4: Implement elevated installer smoke checks**

`verify-installers.ps1` must reject non-administrator execution, remove stale `C:\Program Files\DiskBloom` and `C:\Users\Public\Desktop\DiskBloom.lnk` only after verifying those exact resolved paths, and execute these scenarios with exit-code checks and `try/finally` cleanup:

1. Install en-US MSI with `msiexec /i <path> /qn /norestart`; verify installed EXE, Start menu link, and Public Desktop link; uninstall with `/x`.
2. Install zh-CN MSI with `ADDLOCAL=MainFeature REMOVE=DesktopShortcutFeature`; verify installed EXE and absence of Public Desktop link; uninstall with `/x`.
3. Install EXE with `/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /LANG=chinesesimplified`; verify installed EXE and Public Desktop link; run `unins000.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART`.
4. Install EXE with `/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /LANG=english /MERGETASKS="!desktopicon"`; verify installed EXE and absence of Public Desktop link; uninstall.

Each installed `DiskBloom.exe` must run with `--smoke-test` and exit 0 before uninstall.
Before installation, query each MSI's `Feature` table through the
`WindowsInstaller.Installer` COM API and assert that
`DesktopShortcutFeature` has the expected English or Chinese title. Also read
`DiskBloom.iss` as UTF-8 and assert that both language declarations and both
`CreateDesktopShortcut` messages are present.

- [x] **Step 5: Run the installer smoke suite**

Run from an elevated PowerShell:

```powershell
pwsh -NoProfile -File packaging/windows/verify-installers.ps1 -Version 0.1.0 -OutputDirectory build/release-packages
```

Expected: four install/uninstall scenarios pass and leave neither the application directory nor desktop shortcut behind.

- [x] **Step 6: Commit**

```powershell
git add packaging/windows/build-installers.ps1 packaging/windows/verify-installers.ps1
git commit -m "build: verify Windows installer behavior"
```

---

### Task 6: Publish Packages From GitHub Actions

**Files:**
- Create: `.github/workflows/release-windows.yml`

**Interfaces:**
- Consumes: `release.published` with tag `vX.Y.Z`, or manual `version` input `X.Y.Z`.
- Produces: three Release attachments for automatic runs, or one Actions artifact containing the three packages for manual runs.

- [x] **Step 1: Create the workflow triggers, permissions, and concurrency key**

Start the workflow with:

```yaml
name: Windows Release Installers

on:
  release:
    types: [published]
  workflow_dispatch:
    inputs:
      version:
        description: Version to build in X.Y.Z form
        required: true
        type: string

permissions:
  contents: write

concurrency:
  group: windows-release-${{ github.event.release.tag_name || inputs.version }}
  cancel-in-progress: false
```

- [x] **Step 2: Add deterministic tool setup and version resolution**

Use `windows-2022`, `actions/checkout@v4`, PowerShell, WiX 4.0.6, and Inno Setup 6.4.3:

```yaml
jobs:
  package:
    runs-on: windows-2022
    steps:
      - uses: actions/checkout@v4
        with:
          ref: ${{ github.event_name == 'release' && github.event.release.tag_name || github.sha }}

      - name: Resolve version
        id: version
        shell: pwsh
        env:
          RELEASE_TAG: ${{ github.event.release.tag_name }}
          MANUAL_VERSION: ${{ inputs.version }}
        run: |
          Import-Module ./packaging/windows/Packaging.psm1 -Force
          if ('${{ github.event_name }}' -eq 'release') {
            $version = ConvertFrom-DiskBloomReleaseTag $env:RELEASE_TAG
          } else {
            Assert-DiskBloomVersion $env:MANUAL_VERSION
            $version = $env:MANUAL_VERSION
          }
          "value=$version" >> $env:GITHUB_OUTPUT

      - name: Install WiX Toolset
        shell: pwsh
        run: |
          dotnet tool install --tool-path "$env:RUNNER_TEMP\wix" wix --version 4.0.6
          & "$env:RUNNER_TEMP\wix\wix.exe" extension add --global WixToolset.UI.wixext/4.0.6

      - name: Install Inno Setup
        shell: pwsh
        run: |
          $installer = Join-Path $env:RUNNER_TEMP 'innosetup-6.4.3.exe'
          Invoke-WebRequest -UseBasicParsing `
            -Uri 'https://github.com/jrsoftware/issrc/releases/download/is-6_4_3/innosetup-6.4.3.exe' `
            -OutFile $installer
          $signature = Get-AuthenticodeSignature -LiteralPath $installer
          if ($signature.Status -ne 'Valid' -or
              $signature.SignerCertificate.Subject -notlike '*Pyrsys B.V.*') {
            throw 'Inno Setup signature validation failed.'
          }
```

- [x] **Step 3: Add build, installer verification, and package validation**

```yaml
      - name: Build and test installers
        shell: pwsh
        run: |
          ./packaging/windows/build-installers.ps1 `
            -Version '${{ steps.version.outputs.value }}' `
            -WixPath "$env:RUNNER_TEMP\wix\wix.exe" `
            -InnoCompilerPath 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe'

      - name: Verify install and uninstall behavior
        shell: pwsh
        run: |
          ./packaging/windows/verify-installers.ps1 `
            -Version '${{ steps.version.outputs.value }}' `
            -OutputDirectory build/release-packages
```

- [x] **Step 4: Add mutually exclusive manual and Release uploads**

```yaml
      - name: Retain manual-build packages
        if: github.event_name == 'workflow_dispatch'
        uses: actions/upload-artifact@v4
        with:
          name: DiskBloom-${{ steps.version.outputs.value }}-windows-x64-installers
          path: |
            build/release-packages/DiskBloom-${{ steps.version.outputs.value }}-windows-x64-en-US.msi
            build/release-packages/DiskBloom-${{ steps.version.outputs.value }}-windows-x64-zh-CN.msi
            build/release-packages/DiskBloom-${{ steps.version.outputs.value }}-windows-x64-setup.exe
          if-no-files-found: error

      - name: Upload published Release assets
        if: github.event_name == 'release'
        shell: pwsh
        env:
          GH_TOKEN: ${{ github.token }}
          RELEASE_TAG: ${{ github.event.release.tag_name }}
        run: |
          Import-Module ./packaging/windows/Packaging.psm1 -Force
          $paths = Get-DiskBloomPackagePaths `
            -Version '${{ steps.version.outputs.value }}' `
            -OutputDirectory build/release-packages
          Assert-DiskBloomPackageSet $paths | Format-Table Path, Hash
          gh release upload $env:RELEASE_TAG `
            $paths.EnglishMsi $paths.ChineseMsi $paths.SetupExe `
            --clobber --repo $env:GITHUB_REPOSITORY
```

- [x] **Step 5: Perform static workflow checks**

Run:

```powershell
rg -n "release:|types: \[published\]|workflow_dispatch:|contents: write|gh release upload|actions/upload-artifact@v4" .github/workflows/release-windows.yml
pwsh -NoProfile -Command "Import-Module ./packaging/windows/Packaging.psm1; ConvertFrom-DiskBloomReleaseTag v1.2.3"
```

Expected: all required workflow clauses are found; tag conversion prints `1.2.3`.

- [x] **Step 6: Commit**

```powershell
git add .github/workflows/release-windows.yml
git commit -m "ci: publish Windows release installers"
```

---

### Task 7: Document And Run Final Release Verification

**Files:**
- Modify: `README.md`
- Create: `docs/qa/windows-installers.md`

**Interfaces:**
- Documents manual packaging, artifact names, unsigned-package warning, Release tag contract, and repeatable CI/local verification.

- [x] **Step 1: Update public build documentation**

Add a `Windows Installers` section to `README.md` with:

```powershell
pwsh -NoProfile -File packaging/windows/build-installers.ps1 `
  -Version 0.1.0 `
  -WixPath C:\tools\wix\wix.exe `
  -InnoCompilerPath 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe'
```

Document the three exact filenames, `vX.Y.Z` Release-tag rule, per-machine/UAC behavior, language split, default desktop option, and unsigned SmartScreen warning.

- [x] **Step 2: Record installer QA evidence**

Create `docs/qa/windows-installers.md` containing:

- build host and exact WiX/Inno/CMake versions;
- Release CTest result count;
- file sizes and SHA-256 hashes for all three packages;
- four installer smoke scenarios and their results;
- confirmation that both MSI prompt translations and both Inno custom messages were built;
- confirmation that the default path creates the shortcut and opt-out leaves none;
- the manual Actions artifact behavior and published Release attachment behavior.

- [x] **Step 3: Run all contract, build, test, and installer checks fresh**

Run:

```powershell
pwsh -NoProfile -File packaging/windows/tests/packaging_tests.ps1
pwsh -NoProfile -File packaging/windows/build-installers.ps1 -Version 0.1.0 -WixPath $wix -InnoCompilerPath $iscc
ctest --test-dir build/release-installers -C Release --output-on-failure
pwsh -NoProfile -File packaging/windows/verify-installers.ps1 -Version 0.1.0 -OutputDirectory build/release-packages
git diff --check
```

Expected: contract tests exit 0; full CTest reports zero failures; all four installer scenarios pass; diff check emits no errors.

- [x] **Step 4: Commit documentation**

```powershell
git add README.md docs/qa/windows-installers.md
git commit -m "docs: document Windows installer releases"
```

- [x] **Step 5: Review branch scope**

Run:

```powershell
git status --short
git log --oneline --decorate -8
git diff main...HEAD --stat
```

Expected: clean worktree; only versioning, packaging, workflow, tests, and related documentation changed.
