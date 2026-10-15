#Requires -Version 7
<#
.SYNOPSIS
  Builds the Bowling Box Windows installer (BowlingBox_Setup_<version>.exe).

.DESCRIPTION
  1. Configure (cmake/windows-installer.cmake) and build BowlingGL plus the
     window-free unit tests with MSYS2 UCRT64 GCC.
  2. Run the unit tests against that build (static runtime, shared libraries).
  3. Stage the BowlingBox install component and verify it.
  4. Generate the wizard art and compile the installer with Inno Setup 6.3+.

  Needs MSYS2 with mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,binutils} and
  Inno Setup 6.3 or newer. Everything is written under build\windows.

.EXAMPLE
  pwsh -File scripts\build_windows_installer.ps1
  pwsh -File scripts\build_windows_installer.ps1 -SkipInstaller
#>
param(
  [string]$BuildDir = "",
  [string]$Msys2Root = "",
  [string]$InnoSetupDir = "",
  [string]$FetchContentDir = "",
  [switch]$SkipBuild,
  [switch]$SkipTests,
  [switch]$SkipInstaller
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot

if (-not $BuildDir) { $BuildDir = Join-Path $RepoRoot "build\windows" }
if (-not $Msys2Root) {
  $Msys2Root = if ($env:MSYS2_LOCATION) { $env:MSYS2_LOCATION } else { "C:\msys64" }
}
$Ucrt64Bin = Join-Path $Msys2Root "ucrt64\bin"
if (-not (Test-Path (Join-Path $Ucrt64Bin "g++.exe"))) {
  throw "UCRT64 g++ not found in $Ucrt64Bin. Install mingw-w64-ucrt-x86_64-gcc or pass -Msys2Root."
}
$env:PATH = "$Ucrt64Bin;$env:PATH"

$StageRoot = Join-Path $BuildDir "installer\stage"
$AppDir = Join-Path $StageRoot "bin"

# The unit-test executables this build runs. BowlingConsole's and
# BowlingDisplay's tests are not built: those apps are not part of the release.
$TestTargets = @(
  "test_bowlingbox_score",
  "test_bowlingbox_core",
  "test_bowlingbox_alley",
  "test_bowlingbox_render",
  "test_bowlinggl"
)

function Invoke-Native {
  param([string]$Exe, [string[]]$Arguments)
  Write-Host "> $Exe $($Arguments -join ' ')"
  & $Exe @Arguments
  if ($LASTEXITCODE -ne 0) { throw "$Exe failed with exit code $LASTEXITCODE" }
}

Write-Host "=== Bowling Box Windows installer ==="
Write-Host "Repo:   $RepoRoot"
Write-Host "Build:  $BuildDir"
Write-Host "MSYS2:  $Msys2Root"

if (-not $SkipBuild) {
  Write-Host "`n--- Configure ---"
  $configure = @(
    "-S", $RepoRoot, "-B", $BuildDir, "-G", "Ninja",
    "-C", (Join-Path $RepoRoot "cmake\windows-installer.cmake")
  )
  # Forward slashes: FetchContent writes this path into generated CMake code,
  # where backslashes would read as escapes.
  if ($FetchContentDir) { $configure += "-DFETCHCONTENT_BASE_DIR=$($FetchContentDir -replace '\\', '/')" }
  Invoke-Native cmake $configure

  Write-Host "`n--- Build ---"
  $jobs = [Math]::Max(1, [int]$env:NUMBER_OF_PROCESSORS)
  Invoke-Native cmake (@("--build", $BuildDir, "--parallel", "$jobs", "--target", "BowlingGL") + $TestTargets)
}

if (-not $SkipTests) {
  Write-Host "`n--- Unit tests ---"
  # gtest_discover_tests registers a <target>_NOT_BUILT placeholder for test
  # executables that were not built. Those carry no label; make sure of it, or
  # the label filter below would run (and fail on) them.
  $listed = & ctest --test-dir $BuildDir -N --label-regex unit
  if ($LASTEXITCODE -ne 0) { throw "ctest -N failed" }
  $notBuilt = @($listed | Select-String '_NOT_BUILT')
  if ($notBuilt.Count -gt 0) {
    throw "Label filter selects unbuilt tests:`n$($notBuilt -join "`n")"
  }
  Invoke-Native ctest @("--test-dir", $BuildDir, "--label-regex", "unit",
                        "--output-on-failure", "--no-tests=error")
}

Write-Host "`n--- Stage ---"
if (Test-Path $StageRoot) { Remove-Item $StageRoot -Recurse -Force }
Invoke-Native cmake @("--install", $BuildDir, "--component", "BowlingBox", "--prefix", $StageRoot)

Write-Host "`n--- Verify stage ---"
& (Join-Path $PSScriptRoot "verify_installer_stage.ps1") -AppDir $AppDir -Objdump (Join-Path $Ucrt64Bin "objdump.exe")
if ($LASTEXITCODE -ne 0) { throw "Stage verification failed." }

if ($SkipInstaller) {
  Write-Host "`nSkipInstaller set; staged folder: $AppDir"
  exit 0
}

function Get-ProjectVersion {
  # The version CMake configured: cmake/GitVersion.cmake resolves it from the
  # nearest vX.Y.Z tag (fallback in that file), and project() records it here.
  $cache = Join-Path $BuildDir "CMakeCache.txt"
  if (-not (Test-Path $cache)) { throw "No $cache; configure the build first." }
  $match = Select-String -Path $cache -Pattern '^CMAKE_PROJECT_VERSION:[A-Z]+=(.*)$' | Select-Object -First 1
  if (-not $match) { throw "CMAKE_PROJECT_VERSION not found in $cache." }
  $version = $match.Matches[0].Groups[1].Value.Trim()
  if ($version -notmatch '^\d+\.\d+\.\d+$') { throw "Unexpected project version '$version' in $cache." }
  return $version
}

function Resolve-Iscc {
  param([string]$Hint)
  $candidates = @()
  if ($Hint) { $candidates += (Join-Path $Hint "ISCC.exe") }
  $onPath = Get-Command ISCC.exe -ErrorAction SilentlyContinue
  if ($onPath) { $candidates += $onPath.Source }
  $candidates += "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
  $iscc = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
  if (-not $iscc) { throw "Inno Setup 6.3+ not found. Install it (choco install innosetup) or pass -InnoSetupDir." }
  # ISCC.exe carries no version resource; installer\bowlingbox.iss checks the
  # compiler version itself (#if Ver < EncodeVer(6,3,0)).
  return $iscc
}

Write-Host "`n--- Wizard art ---"
$ArtDir = Join-Path $BuildDir "installer\art"
& (Join-Path $PSScriptRoot "make_installer_art.ps1") `
  -Source (Join-Path $RepoRoot "docs\Neon-Bowling-phone.png") -OutDir $ArtDir

$Version = Get-ProjectVersion
$DistDir = Join-Path $BuildDir "dist"
New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
Write-Host "`n--- Inno Setup (Bowling Box $Version) ---"
$Iscc = Resolve-Iscc -Hint $InnoSetupDir
Invoke-Native $Iscc @(
  "/DStageDir=$((Resolve-Path $AppDir).Path)",
  "/DArtDir=$((Resolve-Path $ArtDir).Path)",
  "/DOutputDir=$((Resolve-Path $DistDir).Path)",
  "/DMyAppVersion=$Version",
  (Join-Path $RepoRoot "installer\bowlingbox.iss")
)

$Installer = Join-Path $DistDir "BowlingBox_Setup_$Version.exe"
if (-not (Test-Path $Installer)) { throw "Installer not found: $Installer" }
Write-Host "`n=== Done ==="
Write-Host "Installer: $Installer"
Write-Host "Size:      $([math]::Round((Get-Item $Installer).Length / 1MB, 1)) MB"
