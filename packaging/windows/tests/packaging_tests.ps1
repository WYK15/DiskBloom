$ErrorActionPreference = 'Stop'
$module = Join-Path $PSScriptRoot '..\Packaging.psm1'
Import-Module $module -Force

function Assert-Equal {
    param($Expected, $Actual, [string]$Message)

    if ($Expected -ne $Actual) {
        throw "$Message Expected '$Expected', got '$Actual'."
    }
}

function Assert-Throws {
    param([scriptblock]$Action, [string]$Message)

    try {
        & $Action
    } catch {
        return
    }
    throw $Message
}

function Assert-FileContains {
    param([string]$Path, [string]$Expected, [string]$Message)

    $content = [IO.File]::ReadAllText($Path, [Text.Encoding]::UTF8)
    if (-not $content.Contains($Expected)) {
        throw "$Message Missing '$Expected' in '$Path'."
    }
}

Assert-Equal '1.2.3' (ConvertFrom-DiskBloomReleaseTag 'v1.2.3') 'Tag conversion failed.'
Assert-Throws { ConvertFrom-DiskBloomReleaseTag '1.2.3' } 'Missing v prefix was accepted.'
Assert-Throws { ConvertFrom-DiskBloomReleaseTag 'v1.2.3-beta.1' } 'Prerelease tag was accepted.'
Assert-Throws { Assert-DiskBloomVersion 'v1.2.3' } 'Manual version accepted a v prefix.'

$englishProductCode = Get-DiskBloomMsiProductCode -Version '1.2.3' -Culture 'en-US'
$englishProductCodeAgain = Get-DiskBloomMsiProductCode -Version '1.2.3' -Culture 'en-US'
$chineseProductCode = Get-DiskBloomMsiProductCode -Version '1.2.3' -Culture 'zh-CN'
$nextVersionProductCode = Get-DiskBloomMsiProductCode -Version '1.2.4' -Culture 'en-US'
Assert-Equal $englishProductCode $englishProductCodeAgain 'MSI ProductCode is not repeatable.'
if ($englishProductCode -eq $chineseProductCode) { throw 'MSI cultures share a ProductCode.' }
if ($englishProductCode -eq $nextVersionProductCode) { throw 'MSI versions share a ProductCode.' }
[void][guid]::Parse($englishProductCode)
Assert-Throws { Get-DiskBloomMsiProductCode -Version '1.2.3' -Culture 'fr-FR' } 'Unsupported MSI culture was accepted.'

$paths = Get-DiskBloomPackagePaths -Version '1.2.3' -OutputDirectory 'C:\packages'
Assert-Equal 'DiskBloom-1.2.3-windows-x64-en-US.msi' (Split-Path $paths.EnglishMsi -Leaf) 'English MSI name drifted.'
Assert-Equal 'DiskBloom-1.2.3-windows-x64-zh-CN.msi' (Split-Path $paths.ChineseMsi -Leaf) 'Chinese MSI name drifted.'
Assert-Equal 'DiskBloom-1.2.3-windows-x64-setup.exe' (Split-Path $paths.SetupExe -Leaf) 'EXE name drifted.'

$windowsDirectory = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$wixProduct = Join-Path $windowsDirectory 'wix\Product.wxs'
$wixEnglish = Join-Path $windowsDirectory 'wix\Product.en-US.wxl'
$wixChinese = Join-Path $windowsDirectory 'wix\Product.zh-CN.wxl'
$innoProduct = Join-Path $windowsDirectory 'inno\DiskBloom.iss'
$innoChinese = Join-Path $windowsDirectory 'inno\languages\ChineseSimplified.isl'
$chineseShortcutPrompt = -join ([char[]]@(
    0x662F, 0x5426, 0x5728, 0x684C, 0x9762, 0x5EFA,
    0x7ACB, 0x5FEB, 0x6377, 0x65B9, 0x5F0F
))

Assert-FileContains $wixProduct 'Scope="perMachine"' 'MSI installation scope drifted.'
Assert-FileContains $wixProduct '{3FE244EB-3C40-4A15-816C-7E4F0994D9D5}' 'MSI upgrade identity drifted.'
Assert-FileContains $wixProduct 'Id="DesktopShortcutFeature"' 'MSI desktop feature is missing.'
Assert-FileContains $wixProduct 'Level="1"' 'MSI desktop feature is not enabled by default.'
Assert-FileContains $wixEnglish 'Culture="en-US"' 'English MSI culture is missing.'
Assert-FileContains $wixEnglish 'Create a desktop shortcut' 'English MSI shortcut prompt is missing.'
Assert-FileContains $wixChinese 'Culture="zh-CN"' 'Chinese MSI culture is missing.'
Assert-FileContains $wixChinese $chineseShortcutPrompt 'Chinese MSI shortcut prompt is missing.'
Assert-FileContains $innoProduct 'Name: "english"' 'English EXE installer language is missing.'
Assert-FileContains $innoProduct 'Name: "chinesesimplified"' 'Chinese EXE installer language is missing.'
Assert-FileContains $innoProduct 'MessagesFile: "languages\ChineseSimplified.isl"' 'Chinese EXE installer language file is not repository-owned.'
Assert-FileContains $innoProduct "chinesesimplified.CreateDesktopShortcut=$chineseShortcutPrompt" 'Chinese EXE shortcut prompt is missing.'
Assert-FileContains $innoProduct 'Name: "desktopicon"' 'EXE desktop task is missing.'
Assert-FileContains $innoChinese 'LanguageID=$0804' 'Chinese language resource has the wrong LCID.'

$temp = Join-Path ([IO.Path]::GetTempPath()) "diskbloom-package-tests-$([guid]::NewGuid())"
New-Item -ItemType Directory -Path $temp | Out-Null
try {
    $fixture = Get-DiskBloomPackagePaths -Version '1.2.3' -OutputDirectory $temp
    Assert-Throws { Assert-DiskBloomPackageSet $fixture } 'Missing files were accepted.'

    [IO.File]::WriteAllBytes($fixture.EnglishMsi, [byte[]]@())
    'msi-zh' | Set-Content -LiteralPath $fixture.ChineseMsi -NoNewline
    'setup' | Set-Content -LiteralPath $fixture.SetupExe -NoNewline
    Assert-Throws { Assert-DiskBloomPackageSet $fixture } 'An empty package was accepted.'

    'msi-en' | Set-Content -LiteralPath $fixture.EnglishMsi -NoNewline
    $result = @(Assert-DiskBloomPackageSet $fixture)
    Assert-Equal 3 $result.Count 'Package validation did not return three hashes.'
    foreach ($hash in $result) {
        Assert-Equal 'SHA256' $hash.Algorithm 'Unexpected package hash algorithm.'
    }
} finally {
    Remove-Item -LiteralPath $temp -Recurse -Force
}

Write-Output 'Packaging contract tests passed.'
