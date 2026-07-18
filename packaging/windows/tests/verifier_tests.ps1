$ErrorActionPreference = 'Stop'

$verifier = Join-Path $PSScriptRoot '..\verify-installers.ps1'
$ErrorActionPreference = 'Continue'
$result = & powershell -NoProfile -ExecutionPolicy Bypass -File $verifier `
    -Version 'v1.2.3' `
    -OutputDirectory 'missing-output' 2>&1
$verifierExitCode = $LASTEXITCODE
$ErrorActionPreference = 'Stop'
$resultText = $result | Out-String

if ($verifierExitCode -eq 0) {
    throw 'Invalid version unexpectedly succeeded.'
}
if ($resultText -notmatch "Version 'v1\.2\.3' must match X\.Y\.Z") {
    throw "Verifier failed for the wrong reason: $resultText"
}

Write-Output 'Installer verifier tests passed.'
