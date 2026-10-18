#Requires -Version 7
<#
.SYNOPSIS
  Checks the staged Bowling Box folder before Inno Setup packages it.

.DESCRIPTION
  1. The folder holds exactly the shipped files, nothing more and nothing less.
  2. Every DLL an .exe or .dll imports is either shipped beside it or a
     Windows system DLL from the allowlist below.
  3. Nothing imports a MinGW runtime DLL; BOWLINGBOX_STATIC_RUNTIME links the
     runtime into every module.

  The allowlist is explicit on purpose. Accepting "whatever is in System32"
  would accept DLLs that exist on the build runner (the Visual C++ runtime, for
  one) but not on a user's PC.
#>
param(
  [Parameter(Mandatory = $true)][string]$AppDir,
  [string]$Objdump = "objdump"
)

$ErrorActionPreference = "Stop"

$ShippedDlls = @(
  "libBowlingboxScore.dll",
  "libBowlingboxCore.dll",
  "libBowlingboxAlley.dll",
  "libBowlingboxRender.dll",
  "libBowlingboxAudio.dll"
)
$Shipped = @("BowlingGL.exe") + $ShippedDlls + @("LICENSE", "THIRD_PARTY_NOTICES.txt")

$SystemDlls = @(
  "kernel32.dll", "user32.dll", "gdi32.dll", "opengl32.dll", "comctl32.dll",
  "comdlg32.dll", "shell32.dll", "shlwapi.dll", "ole32.dll", "oleaut32.dll",
  "advapi32.dll", "winspool.drv", "winmm.dll", "version.dll", "uxtheme.dll",
  "oleacc.dll", "ws2_32.dll", "rpcrt4.dll", "imm32.dll", "msimg32.dll",
  "gdiplus.dll", "ucrtbase.dll"
)
$SystemPatterns = @("api-ms-win-*.dll")
$RuntimePatterns = @("libstdc++*", "libgcc*", "libwinpthread*", "libssp*")

$errors = [System.Collections.Generic.List[string]]::new()

if (-not (Test-Path $AppDir -PathType Container)) {
  throw "Stage folder not found: $AppDir"
}
$AppDir = (Resolve-Path $AppDir).Path

foreach ($name in $Shipped) {
  if (-not (Test-Path (Join-Path $AppDir $name) -PathType Leaf)) {
    $errors.Add("missing: $name")
  }
}

$files = @(Get-ChildItem $AppDir -Recurse -File)
foreach ($file in $files) {
  $relative = [System.IO.Path]::GetRelativePath($AppDir, $file.FullName)
  if ($Shipped -notcontains $relative) {
    $errors.Add("unexpected file: $relative (only the shipped set may be staged)")
  }
}

function Test-Like([string]$Name, [string[]]$Patterns) {
  foreach ($pattern in $Patterns) {
    if ($Name -like $pattern) { return $true }
  }
  return $false
}

$binaries = @($files | Where-Object { $_.Extension -in ".exe", ".dll" })
foreach ($binary in $binaries) {
  $dump = & $Objdump -p $binary.FullName
  if ($LASTEXITCODE -ne 0) { throw "$Objdump -p failed on $($binary.Name)" }
  $imports = @($dump | Select-String '^\s*DLL Name:\s*(\S+)' |
    ForEach-Object { $_.Matches[0].Groups[1].Value })
  # Every Windows .exe and .dll imports at least KERNEL32. None found means the
  # objdump output was not understood, not that the file is clean.
  if ($imports.Count -eq 0) {
    $errors.Add("$($binary.Name): no imports parsed from '$Objdump -p' output; the import check cannot vouch for it")
  }
  foreach ($import in $imports) {
    if (Test-Like $import $RuntimePatterns) {
      $errors.Add("$($binary.Name) imports $import; BOWLINGBOX_STATIC_RUNTIME should have linked it statically")
    } elseif ($ShippedDlls -contains $import) {
      continue
    } elseif ($SystemDlls -contains $import.ToLowerInvariant() -or (Test-Like $import $SystemPatterns)) {
      continue
    } else {
      $errors.Add("$($binary.Name) imports $import, which is neither shipped nor an allowed Windows system DLL")
    }
  }
  Write-Host ("  {0,-26} imports {1}" -f $binary.Name, ($imports -join ", "))
}

if ($errors.Count -gt 0) {
  Write-Host "`nStage verification FAILED:"
  $errors | ForEach-Object { Write-Host "  $_" }
  exit 1
}
Write-Host "`nStage verification passed: $($files.Count) files in $AppDir"
exit 0
