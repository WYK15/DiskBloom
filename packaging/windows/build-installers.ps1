param(
    [Parameter(Mandatory = $true)]
    [string]$Version,

    [Parameter(Mandatory = $true)]
    [string]$WixPath,

    [Parameter(Mandatory = $true)]
    [string]$InnoCompilerPath,

    [string]$CMakePath = 'cmake',
    [string]$CTestPath = 'ctest',
    [string]$Generator = 'Visual Studio 17 2022',
    [string]$BuildDirectory = 'build/release-installers',
    [string]$OutputDirectory = 'build/release-packages'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot 'Packaging.psm1') -Force
Assert-DiskBloomVersion $Version

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$allowedBuildRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'build')).TrimEnd('\')

function Resolve-RepositoryPath {
    param([string]$Path)

    if ([IO.Path]::IsPathRooted($Path)) {
        return [IO.Path]::GetFullPath($Path)
    }
    return [IO.Path]::GetFullPath((Join-Path $repoRoot $Path))
}

function Assert-BuildPath {
    param([string]$Path)

    $fullPath = [IO.Path]::GetFullPath($Path).TrimEnd('\')
    if (-not $fullPath.StartsWith(
        "$allowedBuildRoot\",
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "Packaging path '$fullPath' must be inside '$allowedBuildRoot'."
    }
    return $fullPath
}

function Resolve-Executable {
    param([string]$PathOrCommand)

    if (Test-Path -LiteralPath $PathOrCommand -PathType Leaf) {
        return [IO.Path]::GetFullPath($PathOrCommand)
    }
    $command = Get-Command $PathOrCommand -ErrorAction SilentlyContinue
    if ($null -eq $command) {
        throw "Required executable '$PathOrCommand' was not found."
    }
    return $command.Source
}

function Invoke-Checked {
    param(
        [string]$FilePath,
        [string[]]$Arguments,
        [string]$Description
    )

    Write-Output "==> $Description"
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed with exit code $LASTEXITCODE."
    }
}

$cmake = Resolve-Executable $CMakePath
$ctest = Resolve-Executable $CTestPath
$wix = Resolve-Executable $WixPath
$iscc = Resolve-Executable $InnoCompilerPath
$build = Assert-BuildPath (Resolve-RepositoryPath $BuildDirectory)
$output = Assert-BuildPath (Resolve-RepositoryPath $OutputDirectory)
$stage = Assert-BuildPath (Join-Path $build 'installer-stage')

foreach ($path in @($build, $output)) {
    if (Test-Path -LiteralPath $path) {
        Remove-Item -LiteralPath $path -Recurse -Force
    }
    New-Item -ItemType Directory -Path $path | Out-Null
}

$configureArguments = @(
    '-S', $repoRoot,
    '-B', $build,
    '-G', $Generator,
    "-DDISKBLOOM_VERSION=$Version",
    '-DCMAKE_BUILD_TYPE=Release'
)
if ($Generator.StartsWith('Visual Studio', [StringComparison]::OrdinalIgnoreCase)) {
    $configureArguments += @('-A', 'x64')
}

Invoke-Checked $cmake $configureArguments 'Configure Release build'
Invoke-Checked $cmake @('--build', $build, '--config', 'Release', '--parallel') 'Build Release application'
Invoke-Checked $ctest @('--test-dir', $build, '-C', 'Release', '--output-on-failure') 'Run Release test suite'
Invoke-Checked $cmake @('--install', $build, '--prefix', $stage, '--config', 'Release') 'Stage application payload'

$requiredPayload = @(
    'DiskBloom.exe',
    'concrt140.dll',
    'msvcp140.dll',
    'msvcp140_1.dll',
    'msvcp140_2.dll',
    'msvcp140_atomic_wait.dll',
    'msvcp140_codecvt_ids.dll',
    'vcruntime140.dll',
    'vcruntime140_1.dll'
)
foreach ($name in $requiredPayload) {
    $payloadPath = Join-Path $stage $name
    if (-not (Test-Path -LiteralPath $payloadPath -PathType Leaf)) {
        throw "Staged payload is missing '$name'."
    }
}

$paths = Get-DiskBloomPackagePaths -Version $Version -OutputDirectory $output
$englishProductCode = Get-DiskBloomMsiProductCode -Version $Version -Culture 'en-US'
$chineseProductCode = Get-DiskBloomMsiProductCode -Version $Version -Culture 'zh-CN'
$wixSource = Join-Path $PSScriptRoot 'wix\Product.wxs'
$icon = Join-Path $repoRoot 'src\resources\diskbloom_icon.ico'
$wixCommon = @(
    'build', $wixSource,
    '-arch', 'x64',
    '-ext', 'WixToolset.UI.wixext',
    '-pdbtype', 'none',
    '-d', "ProductVersion=$Version",
    '-d', "StageDir=$stage",
    '-d', "IconPath=$icon"
)

Invoke-Checked $wix ($wixCommon + @(
    '-d', "ProductCode=$englishProductCode",
    '-culture', 'en-US',
    '-loc', (Join-Path $PSScriptRoot 'wix\Product.en-US.wxl'),
    '-intermediateFolder', (Join-Path $build 'wix-en-US'),
    '-out', $paths.EnglishMsi
)) 'Build en-US MSI'

Invoke-Checked $wix ($wixCommon + @(
    '-d', "ProductCode=$chineseProductCode",
    '-culture', 'zh-CN',
    '-loc', (Join-Path $PSScriptRoot 'wix\Product.zh-CN.wxl'),
    '-intermediateFolder', (Join-Path $build 'wix-zh-CN'),
    '-out', $paths.ChineseMsi
)) 'Build zh-CN MSI'

Invoke-Checked $iscc @(
    "/DAppVersion=$Version",
    "/DStageDir=$stage",
    "/DOutputDir=$output",
    "/DIconPath=$icon",
    (Join-Path $PSScriptRoot 'inno\DiskBloom.iss')
) 'Build multilingual EXE installer'

$hashes = @(Assert-DiskBloomPackageSet $paths)
foreach ($hash in $hashes) {
    $item = Get-Item -LiteralPath $hash.Path
    Write-Output ("Package: {0} ({1} bytes) SHA256={2}" -f $item.FullName, $item.Length, $hash.Hash)
}
