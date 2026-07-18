$ErrorActionPreference = 'Stop'

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$workflowPath = Join-Path $repoRoot '.github\workflows\release-windows.yml'
$workflow = [IO.File]::ReadAllText($workflowPath, [Text.Encoding]::UTF8)

function Assert-Contains {
    param([string]$Expected, [string]$Message)

    if (-not $workflow.Contains($Expected)) {
        throw "$Message Missing '$Expected'."
    }
}

Assert-Contains 'types: [published]' 'Published Release trigger is missing.'
Assert-Contains 'workflow_dispatch:' 'Manual validation trigger is missing.'
Assert-Contains 'contents: write' 'Release upload permission is missing.'
Assert-Contains 'runs-on: windows-2022' 'Pinned Windows runner is missing.'
Assert-Contains 'actions/checkout@v4' 'Checkout action version drifted.'
Assert-Contains 'wix --version 4.0.6' 'WiX version is not pinned.'
Assert-Contains 'is-6_4_3/innosetup-6.4.3.exe' 'Inno Setup download is not immutable.'
Assert-Contains 'extension add --global WixToolset.UI.wixext/4.0.6' 'WiX UI extension is not pinned globally.'
Assert-Contains 'actions/upload-artifact@v4' 'Manual artifact upload is missing.'
Assert-Contains "if: github.event_name == 'workflow_dispatch'" 'Manual upload condition is missing.'
Assert-Contains "if: github.event_name == 'release'" 'Release upload condition is missing.'
Assert-Contains 'gh release upload' 'Release asset upload is missing.'
Assert-Contains '--clobber' 'Idempotent Release upload is missing.'

Write-Output 'Release workflow contract tests passed.'
