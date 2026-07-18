$ErrorActionPreference = 'Stop'

$driver = Join-Path $PSScriptRoot '..\build-installers.ps1'
$temp = Join-Path ([IO.Path]::GetTempPath()) "diskbloom-driver-tests-$([guid]::NewGuid())"
$build = Join-Path $temp 'build'
$output = Join-Path $temp 'output'

try {
    $ErrorActionPreference = 'Continue'
    $result = & powershell -NoProfile -ExecutionPolicy Bypass -File $driver `
        -Version 'v1.2.3' `
        -WixPath 'missing-wix.exe' `
        -InnoCompilerPath 'missing-iscc.exe' `
        -BuildDirectory $build `
        -OutputDirectory $output 2>&1
    $driverExitCode = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    $resultText = $result | Out-String
    if ($driverExitCode -eq 0) {
        throw 'Invalid version unexpectedly succeeded.'
    }
    if ($resultText -notmatch "Version 'v1\.2\.3' must match X\.Y\.Z") {
        throw "Driver failed for the wrong reason: $resultText"
    }
    if ((Test-Path -LiteralPath $build) -or (Test-Path -LiteralPath $output)) {
        throw 'Invalid version created build output before validation.'
    }
} finally {
    if (Test-Path -LiteralPath $temp) {
        Remove-Item -LiteralPath $temp -Recurse -Force
    }
}

Write-Output 'Packaging driver tests passed.'
