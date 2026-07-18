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
