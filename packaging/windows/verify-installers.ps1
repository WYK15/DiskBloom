param(
    [Parameter(Mandatory = $true)]
    [string]$Version,

    [string]$OutputDirectory = 'build/release-packages'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot 'Packaging.psm1') -Force
Assert-DiskBloomVersion $Version

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if ([IO.Path]::IsPathRooted($OutputDirectory)) {
    $output = [IO.Path]::GetFullPath($OutputDirectory)
} else {
    $output = [IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDirectory))
}
$paths = Get-DiskBloomPackagePaths -Version $Version -OutputDirectory $output
Assert-DiskBloomPackageSet $paths | Out-Null

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Installer verification requires an elevated PowerShell process.'
}

$installDirectory = Join-Path $env:ProgramFiles 'DiskBloom'
$installedExecutable = Join-Path $installDirectory 'DiskBloom.exe'
$desktopShortcut = Join-Path ([Environment]::GetFolderPath('CommonDesktopDirectory')) 'DiskBloom.lnk'
$commonPrograms = [Environment]::GetFolderPath('CommonPrograms')
$msiStartShortcut = Join-Path $commonPrograms 'DiskBloom\DiskBloom.lnk'
$innoStartShortcut = Join-Path $commonPrograms 'DiskBloom.lnk'
$msiexec = Join-Path $env:SystemRoot 'System32\msiexec.exe'

foreach ($path in @(
    $installDirectory,
    $desktopShortcut,
    $msiStartShortcut,
    $innoStartShortcut
)) {
    if (Test-Path -LiteralPath $path) {
        throw "Refusing to overwrite existing installation artifact '$path'."
    }
}

function Invoke-InstallerProcess {
    param(
        [string]$FilePath,
        [string[]]$Arguments,
        [string]$Description,
        [int[]]$AcceptedExitCodes = @(0)
    )

    Write-Output "==> $Description"
    $process = Start-Process `
        -FilePath $FilePath `
        -ArgumentList $Arguments `
        -Wait `
        -PassThru `
        -WindowStyle Hidden
    if ($process.ExitCode -notin $AcceptedExitCodes) {
        throw "$Description failed with exit code $($process.ExitCode)."
    }
}

function Assert-PathPresent {
    param([string]$Path, [string]$Description)

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Description was not created at '$Path'."
    }
}

function Assert-PathAbsent {
    param([string]$Path, [string]$Description)

    if (Test-Path -LiteralPath $Path) {
        throw "$Description unexpectedly exists at '$Path'."
    }
}

function Wait-PathAbsent {
    param([string]$Path, [string]$Description)

    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    while ((Test-Path -LiteralPath $Path) -and [DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 200
    }
    Assert-PathAbsent $Path $Description
}

function Invoke-InstalledSmokeTest {
    param([string]$Language)

    Invoke-InstallerProcess `
        -FilePath $installedExecutable `
        -Arguments @('--smoke-test', '--theme=dark', "--language=$Language") `
        -Description "Run installed application smoke test ($Language)"
}

function Get-MsiDesktopFeatureTitle {
    param([string]$MsiPath)

    $installer = $null
    $database = $null
    $view = $null
    $record = $null
    try {
        $flags = [Reflection.BindingFlags]::InvokeMethod
        $installer = New-Object -ComObject WindowsInstaller.Installer
        $database = $installer.GetType().InvokeMember(
            'OpenDatabase', $flags, $null, $installer, @($MsiPath, 0))
        $query = "SELECT ``Title`` FROM ``Feature`` WHERE ``Feature``='DesktopShortcutFeature'"
        $view = $database.GetType().InvokeMember(
            'OpenView', $flags, $null, $database, @($query))
        $view.GetType().InvokeMember('Execute', $flags, $null, $view, $null) | Out-Null
        $record = $view.GetType().InvokeMember('Fetch', $flags, $null, $view, $null)
        if ($null -eq $record) {
            throw "DesktopShortcutFeature was not found in '$MsiPath'."
        }
        return $record.GetType().InvokeMember(
            'StringData',
            [Reflection.BindingFlags]::GetProperty,
            $null,
            $record,
            @([int]1))
    } finally {
        foreach ($comObject in @($record, $view, $database, $installer)) {
            if ($null -ne $comObject -and [Runtime.InteropServices.Marshal]::IsComObject($comObject)) {
                [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($comObject)
            }
        }
    }
}

$chineseShortcutPrompt = -join ([char[]]@(
    0x662F, 0x5426, 0x5728, 0x684C, 0x9762, 0x5EFA,
    0x7ACB, 0x5FEB, 0x6377, 0x65B9, 0x5F0F
))
$englishTitle = Get-MsiDesktopFeatureTitle $paths.EnglishMsi
$chineseTitle = Get-MsiDesktopFeatureTitle $paths.ChineseMsi
if ($englishTitle -ne 'Create a desktop shortcut') {
    throw "Unexpected en-US desktop feature title '$englishTitle'."
}
if ($chineseTitle -ne $chineseShortcutPrompt) {
    throw "Unexpected zh-CN desktop feature title '$chineseTitle'."
}

function Test-MsiScenario {
    param(
        [string]$MsiPath,
        [string]$Language,
        [bool]$CreateDesktopShortcut
    )

    $installed = $false
    $arguments = @('/i', "`"$MsiPath`"", '/qn', '/norestart')
    if (-not $CreateDesktopShortcut) {
        $arguments += @('ADDLOCAL=MainFeature', 'REMOVE=DesktopShortcutFeature')
    }
    try {
        Invoke-InstallerProcess $msiexec $arguments "Install $Language MSI" @(0, 3010)
        $installed = $true
        Assert-PathPresent $installedExecutable 'Installed application'
        Assert-PathPresent $msiStartShortcut 'MSI Start menu shortcut'
        if ($CreateDesktopShortcut) {
            Assert-PathPresent $desktopShortcut 'MSI desktop shortcut'
        } else {
            Assert-PathAbsent $desktopShortcut 'MSI desktop shortcut'
        }
        Invoke-InstalledSmokeTest $Language
    } finally {
        if ($installed) {
            Invoke-InstallerProcess `
                $msiexec `
                @('/x', "`"$MsiPath`"", '/qn', '/norestart') `
                "Uninstall $Language MSI" `
                @(0, 3010)
        }
    }
    Wait-PathAbsent $installDirectory 'MSI installation directory'
    Wait-PathAbsent $desktopShortcut 'MSI desktop shortcut'
    Wait-PathAbsent $msiStartShortcut 'MSI Start menu shortcut'
}

function Test-InnoScenario {
    param(
        [string]$Language,
        [bool]$CreateDesktopShortcut
    )

    $installed = $false
    $arguments = @(
        '/VERYSILENT',
        '/SUPPRESSMSGBOXES',
        '/NORESTART',
        "/LANG=$Language"
    )
    if (-not $CreateDesktopShortcut) {
        $arguments += '/MERGETASKS=!desktopicon'
    }
    try {
        Invoke-InstallerProcess $paths.SetupExe $arguments "Install $Language EXE"
        $installed = $true
        Assert-PathPresent $installedExecutable 'Installed application'
        Assert-PathPresent $innoStartShortcut 'EXE Start menu shortcut'
        if ($CreateDesktopShortcut) {
            Assert-PathPresent $desktopShortcut 'EXE desktop shortcut'
        } else {
            Assert-PathAbsent $desktopShortcut 'EXE desktop shortcut'
        }
        $smokeLanguage = if ($Language -eq 'chinesesimplified') { 'zh-CN' } else { 'en-US' }
        Invoke-InstalledSmokeTest $smokeLanguage
    } finally {
        $uninstaller = Join-Path $installDirectory 'unins000.exe'
        if ($installed -and (Test-Path -LiteralPath $uninstaller)) {
            Invoke-InstallerProcess `
                $uninstaller `
                @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART') `
                "Uninstall $Language EXE"
        }
    }
    Wait-PathAbsent $installDirectory 'EXE installation directory'
    Wait-PathAbsent $desktopShortcut 'EXE desktop shortcut'
    Wait-PathAbsent $innoStartShortcut 'EXE Start menu shortcut'
}

Test-MsiScenario $paths.EnglishMsi 'en-US' $true
Test-MsiScenario $paths.ChineseMsi 'zh-CN' $false
Test-InnoScenario 'chinesesimplified' $true
Test-InnoScenario 'english' $false

Write-Output 'Installer verification passed for all four scenarios.'
