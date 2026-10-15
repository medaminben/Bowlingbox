#Requires -Version 7
<#
.SYNOPSIS
  Proves verify_installer_stage.ps1 rejects what it exists to reject.

.DESCRIPTION
  Runs the verifier against copies of a good stage folder:
    - unchanged: must pass;
    - with a stale extra file: must fail naming it;
    - with an objdump whose output yields no "DLL Name:" lines (a changed
      binutils format, the wrong tool): must fail, not pass with no imports;
    - with an objdump that reports a foreign import: must fail naming it.
  Each failure must give its reason, so a check that fails for the wrong
  reason does not count. Runs on Windows (CI) and Linux (pwsh + binutils).

.EXAMPLE
  pwsh -File scripts/test_verify_installer_stage.ps1 -AppDir build/windows/installer/stage/bin -Objdump objdump
#>
param(
  [Parameter(Mandatory = $true)][string]$AppDir,
  [string]$Objdump = "objdump"
)

$ErrorActionPreference = "Stop"
$Verify = Join-Path $PSScriptRoot "verify_installer_stage.ps1"
$Work = Join-Path ([System.IO.Path]::GetTempPath()) "verify-stage-test-$PID"
$failures = [System.Collections.Generic.List[string]]::new()

function New-StageCopy([string]$Name) {
  $dir = Join-Path $Work $Name
  Copy-Item $AppDir $dir -Recurse
  return $dir
}

# Writes a stand-in for objdump: a PowerShell script the verifier calls with
# "-p <file>". $Body sees the real tool as $real and the arguments as $args.
function New-FakeObjdump([string]$Name, [string]$Body) {
  $path = Join-Path $Work "$Name.ps1"
  Set-Content $path "`$real = '$Objdump'`n$Body`nexit 0"
  return $path
}

function Invoke-Case([string]$Name, [string]$Dir, [string]$Tool, [bool]$ShouldPass, [string]$Reason) {
  $output = & $Verify -AppDir $Dir -Objdump $Tool *>&1 | Out-String
  $passed = $LASTEXITCODE -eq 0
  if ($passed -ne $ShouldPass) {
    $failures.Add("${Name}: expected $(if ($ShouldPass) { 'pass' } else { 'failure' }), got $(if ($passed) { 'pass' } else { 'failure' })`n$output")
  } elseif ($Reason -and -not $output.Contains($Reason)) {
    $failures.Add("${Name}: failed, but without '$Reason'`n$output")
  } else {
    Write-Host "ok  $Name"
  }
}

New-Item -ItemType Directory -Force -Path $Work | Out-Null
try {
  Invoke-Case "good stage passes" (New-StageCopy "good") $Objdump $true ""

  $stale = New-StageCopy "stale"
  Copy-Item (Join-Path $stale "libBowlingboxCore.dll") (Join-Path $stale "stale.dll")
  Invoke-Case "stale file rejected" $stale $Objdump $false "unexpected file: stale.dll"

  $silent = New-FakeObjdump "objdump-silent" 'Write-Output "file format pei-x86-64"'
  Invoke-Case "unparsed imports rejected" (New-StageCopy "silent") $silent $false "no imports parsed"

  $evil = New-FakeObjdump "objdump-evil" '& $real @args; Write-Output "`tDLL Name: evil.dll"'
  Invoke-Case "foreign import rejected" (New-StageCopy "evil") $evil $false "imports evil.dll"
} finally {
  Remove-Item $Work -Recurse -Force -ErrorAction SilentlyContinue
}

if ($failures.Count -gt 0) {
  Write-Host "`nverify_installer_stage.ps1 tests FAILED:"
  $failures | ForEach-Object { Write-Host "  $_" }
  exit 1
}
Write-Host "`nverify_installer_stage.ps1 tests passed"
exit 0
