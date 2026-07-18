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

Assert-Equal '1.2.3' (ConvertFrom-DiskBloomReleaseTag 'v1.2.3') 'Tag conversion failed.'
Assert-Throws { ConvertFrom-DiskBloomReleaseTag '1.2.3' } 'Missing v prefix was accepted.'
Assert-Throws { ConvertFrom-DiskBloomReleaseTag 'v1.2.3-beta.1' } 'Prerelease tag was accepted.'
Assert-Throws { Assert-DiskBloomVersion 'v1.2.3' } 'Manual version accepted a v prefix.'

$paths = Get-DiskBloomPackagePaths -Version '1.2.3' -OutputDirectory 'C:\packages'
Assert-Equal 'DiskBloom-1.2.3-windows-x64-en-US.msi' (Split-Path $paths.EnglishMsi -Leaf) 'English MSI name drifted.'
Assert-Equal 'DiskBloom-1.2.3-windows-x64-zh-CN.msi' (Split-Path $paths.ChineseMsi -Leaf) 'Chinese MSI name drifted.'
Assert-Equal 'DiskBloom-1.2.3-windows-x64-setup.exe' (Split-Path $paths.SetupExe -Leaf) 'EXE name drifted.'

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
