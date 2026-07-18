Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function ConvertFrom-DiskBloomReleaseTag {
    param([Parameter(Mandatory = $true)][string]$Tag)

    if ($Tag -notmatch '^v(?<Version>\d+\.\d+\.\d+)$') {
        throw "Release tag '$Tag' must match vX.Y.Z."
    }
    return $Matches.Version
}

function Assert-DiskBloomVersion {
    param([Parameter(Mandatory = $true)][string]$Version)

    if ($Version -notmatch '^\d+\.\d+\.\d+$') {
        throw "Version '$Version' must match X.Y.Z."
    }
}

function Get-DiskBloomPackagePaths {
    param(
        [Parameter(Mandatory = $true)][string]$Version,
        [Parameter(Mandatory = $true)][string]$OutputDirectory
    )

    Assert-DiskBloomVersion $Version
    $root = [IO.Path]::GetFullPath($OutputDirectory)
    return [pscustomobject][ordered]@{
        EnglishMsi = Join-Path $root "DiskBloom-$Version-windows-x64-en-US.msi"
        ChineseMsi = Join-Path $root "DiskBloom-$Version-windows-x64-zh-CN.msi"
        SetupExe = Join-Path $root "DiskBloom-$Version-windows-x64-setup.exe"
    }
}

function Assert-DiskBloomPackageSet {
    param([Parameter(Mandatory = $true)]$Paths)

    foreach ($path in @($Paths.EnglishMsi, $Paths.ChineseMsi, $Paths.SetupExe)) {
        $item = Get-Item -LiteralPath $path -ErrorAction Stop
        if ($item.Length -le 0) {
            throw "Package '$path' is empty."
        }
        Get-FileHash -Algorithm SHA256 -LiteralPath $path
    }
}

Export-ModuleMember -Function @(
    'ConvertFrom-DiskBloomReleaseTag'
    'Assert-DiskBloomVersion'
    'Get-DiskBloomPackagePaths'
    'Assert-DiskBloomPackageSet'
)
