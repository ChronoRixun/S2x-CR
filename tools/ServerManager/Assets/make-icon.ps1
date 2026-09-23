# Draws the title bar's amber mark on the panel colour at 16, 32, 48 and 256px and packs them
# into S2xServerManager.ico, next to this script. Re-run after the mark or the palette changes;
# the .ico is committed, this script is only how it was made.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$panel = [System.Drawing.Color]::FromArgb(0xFF, 0x0E, 0x10, 0x12)
$amber = [System.Drawing.Color]::FromArgb(0xFF, 0xE8, 0xA3, 0x3D)
$sizes = 16, 32, 48, 256

function New-MarkPng([int]$size) {
    $bmp = New-Object System.Drawing.Bitmap $size, $size
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear($panel)
    $pad = [Math]::Round($size * 0.16)
    $points = @(
        (New-Object System.Drawing.Point $pad, $pad),
        (New-Object System.Drawing.Point ($size - $pad), $pad),
        (New-Object System.Drawing.Point ($size - $pad), ($size - $pad))
    )
    $brush = New-Object System.Drawing.SolidBrush $amber
    $g.FillPolygon($brush, $points)
    $brush.Dispose(); $g.Dispose()
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    return ,$ms.ToArray()
}

$pngs = foreach ($size in $sizes) { New-MarkPng $size }

$out = Join-Path $PSScriptRoot 'S2xServerManager.ico'
$stream = [System.IO.File]::Open($out, [System.IO.FileMode]::Create)
$writer = New-Object System.IO.BinaryWriter $stream
$writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $side = if ($sizes[$i] -ge 256) { 0 } else { $sizes[$i] } # 0 means 256 in the ICO format
    $writer.Write([byte]$side); $writer.Write([byte]$side)
    $writer.Write([byte]0); $writer.Write([byte]0)
    $writer.Write([uint16]0); $writer.Write([uint16]32)
    $writer.Write([uint32]$pngs[$i].Length); $writer.Write([uint32]$offset)
    $offset += $pngs[$i].Length
}
foreach ($png in $pngs) { $writer.Write($png) }
$writer.Flush(); $writer.Close()
"Wrote $out ($($sizes -join ', ')px)"
