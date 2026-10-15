#Requires -Version 7
<#
.SYNOPSIS
  Derives the Inno Setup wizard images from the Bowling Box splash.

.DESCRIPTION
  Large image (Welcome/Finished pages): the splash cropped to Inno's 164:314
  aspect around its horizontal centre, at 100/150/200 % DPI.
  Small image (top right of inner pages): the cube emblem above the wordmark,
  letterboxed into a square on the splash's own background colour, at the same
  three scales. (The wordmark is about 850 px wide; no square holds it, and it
  would be unreadable at 55 px.) Inno Setup picks the size matching the screen.
  24-bit BMP, which every Inno Setup version reads.
#>
param(
  [Parameter(Mandatory = $true)][string]$Source,
  [Parameter(Mandatory = $true)][string]$OutDir
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

# The cube emblem in the 941x1672 splash, stopping just above the wordmark and
# inside the bowling pins on either side of it.
$EmblemCrop = [System.Drawing.Rectangle]::new(356, 222, 238, 168)
$LargeSizes = @(@(164, 314), @(246, 471), @(328, 628))
$SmallSizes = @(55, 83, 110)

# Draws Crop scaled into Target (a rectangle of a Width x Height bitmap filled
# with Fill) and saves it as a 24-bit BMP.
function Save-Scaled {
  param($Image, [System.Drawing.Rectangle]$Crop, [int]$Width, [int]$Height,
        [System.Drawing.Rectangle]$Target, [System.Drawing.Color]$Fill, [string]$Path)
  $bitmap = [System.Drawing.Bitmap]::new($Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
  $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
  $attributes = [System.Drawing.Imaging.ImageAttributes]::new()
  try {
    $graphics.Clear($Fill)
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    # Without this, bicubic sampling blends a dark fringe in from outside the crop.
    $attributes.SetWrapMode([System.Drawing.Drawing2D.WrapMode]::TileFlipXY)
    $graphics.DrawImage($Image, $Target,
      $Crop.X, $Crop.Y, $Crop.Width, $Crop.Height, [System.Drawing.GraphicsUnit]::Pixel, $attributes)
  } finally {
    $attributes.Dispose()
    $graphics.Dispose()
  }
  $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Bmp)
  $bitmap.Dispose()
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$image = [System.Drawing.Image]::FromFile((Resolve-Path $Source).Path)
try {
  if ($image.Height -lt $EmblemCrop.Bottom -or $image.Width -lt $EmblemCrop.Right) {
    throw "$Source is $($image.Width)x$($image.Height); the emblem crop needs at least $($EmblemCrop.Right)x$($EmblemCrop.Bottom)."
  }
  $background = ([System.Drawing.Bitmap]$image).GetPixel($EmblemCrop.X, $EmblemCrop.Y)
  $cropWidth = [int][Math]::Round($image.Height * 164 / 314)
  if ($cropWidth -gt $image.Width) { throw "$Source is too wide-short for the 164:314 wizard image." }
  $largeCrop = [System.Drawing.Rectangle]::new([int](($image.Width - $cropWidth) / 2), 0, $cropWidth, $image.Height)

  $expected = @{}
  foreach ($size in $LargeSizes) {
    $path = Join-Path $OutDir "wizard-$($size[0]).bmp"
    Save-Scaled $image $largeCrop $size[0] $size[1] `
      ([System.Drawing.Rectangle]::new(0, 0, $size[0], $size[1])) $background $path
    $expected[$path] = $size
  }
  foreach ($side in $SmallSizes) {
    $path = Join-Path $OutDir "small-$side.bmp"
    $height = [int][Math]::Round($side * $EmblemCrop.Height / $EmblemCrop.Width)
    $target = [System.Drawing.Rectangle]::new(0, [int](($side - $height) / 2), $side, $height)
    Save-Scaled $image $EmblemCrop $side $side $target $background $path
    $expected[$path] = @($side, $side)
  }
} finally {
  $image.Dispose()
}

foreach ($path in $expected.Keys) {
  $check = [System.Drawing.Image]::FromFile($path)
  try {
    if ($check.Width -ne $expected[$path][0] -or $check.Height -ne $expected[$path][1]) {
      throw "$path is $($check.Width)x$($check.Height), expected $($expected[$path] -join 'x')"
    }
  } finally { $check.Dispose() }
  Write-Host "  $([System.IO.Path]::GetFileName($path))  $($expected[$path] -join 'x')"
}
