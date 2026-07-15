param(
    [string]$OutputDirectory = "$PSScriptRoot\..\src\resources"
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$output = [IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($output) | Out-Null
$pngPath = Join-Path $output 'diskbloom_icon.png'
$icoPath = Join-Path $output 'diskbloom_icon.ico'

$bitmap = New-Object Drawing.Bitmap 256, 256, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [Drawing.Graphics]::FromImage($bitmap)
$graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.Clear([Drawing.Color]::Transparent)

$backgroundPath = New-Object Drawing.Drawing2D.GraphicsPath
$radius = 42.0
$backgroundPath.AddArc(12, 12, $radius, $radius, 180, 90)
$backgroundPath.AddArc(202, 12, $radius, $radius, 270, 90)
$backgroundPath.AddArc(202, 202, $radius, $radius, 0, 90)
$backgroundPath.AddArc(12, 202, $radius, $radius, 90, 90)
$backgroundPath.CloseFigure()
$backgroundBrush = New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(255, 35, 40, 48))
$graphics.FillPath($backgroundBrush, $backgroundPath)

$pieBounds = New-Object Drawing.Rectangle 34, 34, 188, 188
$segments = @(
    @{ Start = -90.0; Sweep = 130.0; Color = [Drawing.Color]::FromArgb(255, 95, 157, 247) },
    @{ Start = 40.0; Sweep = 78.0; Color = [Drawing.Color]::FromArgb(255, 71, 215, 172) },
    @{ Start = 118.0; Sweep = 64.0; Color = [Drawing.Color]::FromArgb(255, 243, 200, 75) },
    @{ Start = 182.0; Sweep = 70.0; Color = [Drawing.Color]::FromArgb(255, 238, 117, 94) },
    @{ Start = 252.0; Sweep = 18.0; Color = [Drawing.Color]::FromArgb(255, 172, 124, 232) }
)

$separator = New-Object Drawing.Pen ([Drawing.Color]::FromArgb(255, 35, 40, 48)), 3.0
foreach ($segment in $segments) {
    $brush = New-Object Drawing.SolidBrush $segment.Color
    $graphics.FillPie(
        $brush,
        $pieBounds,
        [single]$segment.Start,
        [single]$segment.Sweep)
    $graphics.DrawPie(
        $separator,
        $pieBounds,
        [single]$segment.Start,
        [single]$segment.Sweep)
    $brush.Dispose()
}

$centerBrush = New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(255, 35, 40, 48))
$centerPen = New-Object Drawing.Pen ([Drawing.Color]::FromArgb(255, 213, 220, 229)), 4.0
$graphics.FillEllipse($centerBrush, 91, 91, 74, 74)
$graphics.DrawEllipse($centerPen, 91, 91, 74, 74)

$bitmap.Save($pngPath, [Drawing.Imaging.ImageFormat]::Png)
$graphics.Dispose()
$bitmap.Dispose()
$backgroundPath.Dispose()
$backgroundBrush.Dispose()
$separator.Dispose()
$centerBrush.Dispose()
$centerPen.Dispose()

$pngBytes = [IO.File]::ReadAllBytes($pngPath)
$stream = [IO.File]::Create($icoPath)
$writer = New-Object IO.BinaryWriter $stream
$writer.Write([UInt16]0)
$writer.Write([UInt16]1)
$writer.Write([UInt16]1)
$writer.Write([Byte]0)
$writer.Write([Byte]0)
$writer.Write([Byte]0)
$writer.Write([Byte]0)
$writer.Write([UInt16]1)
$writer.Write([UInt16]32)
$writer.Write([UInt32]$pngBytes.Length)
$writer.Write([UInt32]22)
$writer.Write($pngBytes)
$writer.Dispose()

Write-Output $icoPath
