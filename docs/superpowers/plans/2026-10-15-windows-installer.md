# Bowling Box Windows Installer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A manually dispatched GitHub Actions workflow builds `BowlingBox_Setup_<version>.exe`, a per-user Inno Setup installer for Bowling Box (`BowlingGL.exe` plus our four library DLLs). It has no runtime prerequisites.

**Architecture:** MSYS2 UCRT64 GCC builds BowlingGL with third-party code (wxWidgets, Bullet3, glad, stb, GoogleTest, GCC runtime) linked statically and Score/Core/Alley/Render as DLLs. A CMake install component, `BowlingBox`, stages exactly the shipped files. A PowerShell script builds, runs the unit tests, stages, verifies every PE import, generates wizard art from the splash and runs Inno Setup.

**Tech Stack:** CMake ≥ 3.29, Ninja, MSYS2 UCRT64 GCC, PowerShell 7, .NET System.Drawing, Inno Setup ≥ 6.3, GitHub Actions (`windows-latest`).

**Spec:** `docs/superpowers/specs/2026-10-15-windows-installer-design.md`

**Worktree:** `/home/amin/Dokumente/dev/Bowlingbox/build/worktrees/windows-installer` (branch `feature/windows-installer`). Run every command from there.

## Global Constraints

- Product name shown to users: **Bowling Box**. Executable stays `BowlingGL.exe`.
- Shipped files, exactly: `BowlingGL.exe`, `libBowlingboxScore.dll`, `libBowlingboxCore.dll`, `libBowlingboxAlley.dll`, `libBowlingboxRender.dll`, `LICENSE`, `THIRD_PARTY_NOTICES.txt`.
- BowlingConsole and BowlingDisplay are not built, staged or shipped by the installer build.
- Third-party code is static, including the GCC runtime (`libstdc++`, `libgcc`, `winpthread`, `ssp`). Our four libraries are shared.
- No staged file may import `libstdc++*`, `libgcc*`, `libwinpthread*` or `libssp*`.
- Install: per-user, `DefaultDirName={autopf}\Bowling Box`, `PrivilegesRequired=lowest`, no `PrivilegesRequiredOverridesAllowed`.
- Inno Setup 6.3 or newer.
- Workflow trigger: `workflow_dispatch` only (a temporary branch `push` trigger exists during development and is removed in Task 5).
- All build output under `build/windows/`. Installer at `build/windows/dist/BowlingBox_Setup_<version>.exe`.
- Version comes from `project(Bowlingbox VERSION x.y.z …)` in `CMakeLists.txt`, and must match `^\d+\.\d+\.\d+$`.
- Wizard art comes from `docs/Neon-Bowling-phone.png` only. Generated BMPs are not committed.
- Linux CI (`.github/workflows/ci.yml`) and `ci/install-smoke.sh` stay green and unchanged.
- No manual and no `licenses\` folder in the install.
- Pushing to `origin` is outward-facing. **Ask the user before the first push of this branch.** Later pushes of the same branch during this plan are covered by that approval.

## Review Focus

1. **A runner image update or wx change adds a new system DLL import.** The stage check must fail and name the DLL and the importing file, not pass silently or fail with a vague error. This is covered by the verify script's message format (Task 3) and its negative self-test.
2. **A stale file left in the stage from an earlier run** (an old test exe or a renamed DLL). The installer must not ship it. The script wipes the stage before installing, and the verify script rejects anything outside the exact shipped set (Task 3, self-test in the workflow).
3. **Upgrading while Bowling Box is running.** The user expects setup to close the game and replace the files, not fail half-way. `CloseApplications=yes` (Task 4). Item 3 of the hand check list (Task 5).
4. **Uninstall leaves nothing behind.** The `Bowling Box` folder, Start menu entry and desktop shortcut are gone. Hand check list item 5 (Task 5).
5. **High-DPI screens** (150 %/200 %). The wizard images must be sharp and undistorted. The three resolutions are generated with checked dimensions (Task 4), and hand check list item 1 looks at it at 150 % scaling (Task 5).

---

## File Structure

| File | Responsibility | Task |
|---|---|---|
| `cmake/Common.cmake` | `BOWLINGBOX_STATIC_RUNTIME` option, `BOWLINGBOX_RUNTIME_COMPONENT` name | 1, 2 |
| `cmake/GTestSupport.cmake` | GoogleTest always static | 1 |
| `cmake/WxSupport.cmake` | Windows feature set for the wx source build | 1 |
| `cmake/windows-installer.cmake` | initial cache for the installer build | 1 |
| `cmake/BuildUtils.cmake` | `COMPONENT` on apps, DLLs into the runtime component | 2 |
| `applications/bowlingGame/bowlingGameGL/CMakeLists.txt` | BowlingGL joins the component | 2 |
| `cmake/modules/InstallConfig.cmake` | `LICENSE` + notices into the component on WIN32 | 2 |
| `licenses/*` | licence texts (committed for the first time), plus `mingw-w64.txt` | 2 |
| `installer/THIRD_PARTY_NOTICES.txt` | notices shipped with the binary | 2 |
| `scripts/verify_installer_stage.ps1` | exact-file-set + PE import check | 3 |
| `scripts/build_windows_installer.ps1` | configure, build, test, stage, verify, art, package | 3, 4 |
| `.github/workflows/windows-installer.yml` | the Windows job | 3, 4, 5 |
| `scripts/make_installer_art.ps1` | wizard BMPs from the splash | 4 |
| `installer/bowlingbox.iss` | Inno Setup script | 4 |
| `docs/Neon-Bowling-phone.png` | splash (committed for the first time) | 4 |
| `docs/windows-installer.md`, `README.md`, `licenses/README.md` | docs | 2, 5 |

---

### Task 1: Static third-party code and the static-runtime option

**Files:**
- Modify: `cmake/Common.cmake` (after `include(Coverage)`)
- Modify: `cmake/GTestSupport.cmake`
- Modify: `cmake/WxSupport.cmake` (the `foreach(lib wxUSE_LIBPNG …)` / `find_package(ZLIB REQUIRED)` block)
- Create: `cmake/windows-installer.cmake`

**Interfaces:**
- Produces: CMake option `BOWLINGBOX_STATIC_RUNTIME` (BOOL, default OFF), and the initial cache file `cmake/windows-installer.cmake` used by Task 3's script as `-C cmake/windows-installer.cmake`.

- [ ] **Step 1: Configure a Linux reference build and record that GoogleTest is currently shared**

```bash
cmake -S . -B build/t1 -G Ninja -DGENERATE_SCAFFOLD=OFF \
  -DFETCHCONTENT_BASE_DIR=/home/amin/Dokumente/dev/Bowlingbox/build/_deps
cmake --build build/t1 -j"$(nproc)" --target gtest
find build/t1 -name 'libgtest*'
```
Expected: a `libgtest.so*` is listed. This is the failing state: GoogleTest is third-party but builds shared.

- [ ] **Step 2: Make GoogleTest static**

In `cmake/GTestSupport.cmake`, replace

```cmake
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
```

with

```cmake
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Third-party code is linked statically; BUILD_SHARED_LIBS stays ON for the
# Bowlingbox libraries, so restore it afterwards (as BulletSupport.cmake does).
set(_bowlingbox_gtest_saved_shared ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
FetchContent_MakeAvailable(googletest)
set(BUILD_SHARED_LIBS ${_bowlingbox_gtest_saved_shared})
```

- [ ] **Step 3: Add the static-runtime option**

In `cmake/Common.cmake`, directly after `include(Coverage)`, add:

```cmake
# Installer builds link the GCC runtime into every module, so BowlingGL.exe and
# the Bowlingbox DLLs import no libstdc++/libgcc/winpthread DLLs. On PE targets
# -static only changes how -l names are searched; import libraries passed by
# full path (our libBowlingbox*.dll.a) and the system import archives still link.
option(BOWLINGBOX_STATIC_RUNTIME "Link the GCC runtime statically (MinGW GCC only)" OFF)
if(BOWLINGBOX_STATIC_RUNTIME)
    if(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        string(APPEND CMAKE_EXE_LINKER_FLAGS    " -static-libgcc -static-libstdc++ -static")
        string(APPEND CMAKE_SHARED_LINKER_FLAGS " -static-libgcc -static-libstdc++ -static")
    else()
        message(WARNING "BOWLINGBOX_STATIC_RUNTIME applies to MinGW GCC only; ignored for "
                        "${CMAKE_CXX_COMPILER_ID} on ${CMAKE_SYSTEM_NAME}")
    endif()
endif()
```

- [ ] **Step 4: Give the wx source build its Windows feature set**

In `cmake/WxSupport.cmake`, replace

```cmake
foreach(lib wxUSE_LIBPNG wxUSE_LIBJPEG wxUSE_EXPAT wxUSE_REGEX)
    set(${lib} builtin CACHE STRING "" FORCE)
endforeach()
find_package(ZLIB REQUIRED)
set(wxUSE_ZLIB sys CACHE STRING "" FORCE)
```

with

```cmake
if(WIN32)
    # BowlingGL uses no wx image, XML or regex classes. Leaving them out keeps
    # libjpeg, Expat and PCRE2 (whose licences need binary notices) out of the
    # Windows installer. wx turns wxUSE_XML off with Expat. Windows has no
    # system zlib, so wx builds its own.
    foreach(lib wxUSE_LIBPNG wxUSE_LIBJPEG wxUSE_EXPAT wxUSE_REGEX)
        set(${lib} OFF CACHE STRING "" FORCE)
    endforeach()
    set(wxUSE_ZLIB builtin CACHE STRING "" FORCE)
else()
    foreach(lib wxUSE_LIBPNG wxUSE_LIBJPEG wxUSE_EXPAT wxUSE_REGEX)
        set(${lib} builtin CACHE STRING "" FORCE)
    endforeach()
    find_package(ZLIB REQUIRED)
    set(wxUSE_ZLIB sys CACHE STRING "" FORCE)
endif()
```

Also update the header comment's third bullet in the same file to read:

```cmake
#   3. FetchContent: download and build it statically from source. On Linux the
#      host then needs GTK3 development headers (libgtk-3-dev) and OpenGL headers
#      (libgl1-mesa-dev); on Windows (MinGW) nothing beyond the compiler.
```

- [ ] **Step 5: Create the installer cache file**

`cmake/windows-installer.cmake`:

```cmake
# Initial cache for the Windows installer build (scripts/build_windows_installer.ps1):
#   cmake -S . -B build/windows -G Ninja -C cmake/windows-installer.cmake
# Third-party code static (wx from source, GCC runtime), Bowlingbox libraries shared.
set(CMAKE_BUILD_TYPE          Release CACHE STRING "")
set(BUILD_SHARED_LIBS         ON      CACHE BOOL "")
set(BUILD_APPS                ON      CACHE BOOL "")
set(BUILD_WX_UI               ON      CACHE BOOL "")
set(BUILD_TESTING             ON      CACHE BOOL "")
set(GENERATE_SCAFFOLD         OFF     CACHE BOOL "")
set(BOWLINGBOX_USE_SYSTEM_WX  OFF     CACHE BOOL "")
set(BOWLINGBOX_STATIC_RUNTIME ON      CACHE BOOL "")
```

- [ ] **Step 6: Verify on Linux: GoogleTest static, tests green, option warns**

```bash
cmake -S . -B build/t1 -G Ninja -DGENERATE_SCAFFOLD=OFF \
  -DFETCHCONTENT_BASE_DIR=/home/amin/Dokumente/dev/Bowlingbox/build/_deps
cmake --build build/t1 -j"$(nproc)"
find build/t1 -name 'libgtest*'
xvfb-run -a ctest --test-dir build/t1 --label-regex unit --output-on-failure --no-tests=error
ci/install-smoke.sh build/t1
cmake -S . -B build/t1-rt -G Ninja -DGENERATE_SCAFFOLD=OFF -DBOWLINGBOX_STATIC_RUNTIME=ON \
  -DFETCHCONTENT_BASE_DIR=/home/amin/Dokumente/dev/Bowlingbox/build/_deps 2>&1 | grep -A1 'BOWLINGBOX_STATIC_RUNTIME'
```
Expected: only `libgtest.a` (no `.so`), all tests pass, "install smoke test passed", and the warning `BOWLINGBOX_STATIC_RUNTIME applies to MinGW GCC only; ignored for GNU on Linux`. If the build uses a stale cache from Step 1, delete `build/t1` and rerun. The Windows branch of `WxSupport.cmake` is proven in Task 3.

- [ ] **Step 7: Commit**

```bash
git add cmake/Common.cmake cmake/GTestSupport.cmake cmake/WxSupport.cmake cmake/windows-installer.cmake
git commit -m "build: static GoogleTest, static GCC runtime option, Windows wx feature set

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: The `BowlingBox` install component, licences and notices

**Files:**
- Modify: `cmake/Common.cmake` (add the component name next to the option from Task 1)
- Modify: `cmake/BuildUtils.cmake` (`create_library` install, `create_application` args + install)
- Modify: `applications/bowlingGame/bowlingGameGL/CMakeLists.txt` (`create_application(NAME BowlingGL …)`)
- Modify: `cmake/modules/InstallConfig.cmake`
- Create: `licenses/` (copy from the main checkout), `licenses/mingw-w64.txt`
- Modify: `licenses/README.md`
- Create: `installer/THIRD_PARTY_NOTICES.txt`

**Interfaces:**
- Consumes: nothing from Task 1 except that it is committed.
- Produces: `cmake --install <build> --component BowlingBox --prefix <stage>` puts the shipped files in `<stage>/bin`. The CMake variable `BOWLINGBOX_RUNTIME_COMPONENT` (value `BowlingBox`). `create_application(... COMPONENT <name>)`.

- [ ] **Step 1: Show the failing state: the component installs nothing**

```bash
rm -rf build/t1/stage-test
cmake --install build/t1 --component BowlingBox --prefix build/t1/stage-test
find build/t1/stage-test -type f 2>/dev/null | wc -l
```
Expected: `0`.

- [ ] **Step 2: Define the component name**

In `cmake/Common.cmake`, directly after the `BOWLINGBOX_STATIC_RUNTIME` block from Task 1:

```cmake
# Install component holding exactly what the Windows installer ships:
# BowlingGL, the Bowlingbox DLLs, LICENSE and THIRD_PARTY_NOTICES.txt.
set(BOWLINGBOX_RUNTIME_COMPONENT BowlingBox)
```

- [ ] **Step 3: DLLs into the component**

In `cmake/BuildUtils.cmake`, `create_library`, change

```cmake
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
```

(inside `install(TARGETS ${LIBRARY_NAME_RAW} EXPORT …)`) to

```cmake
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                COMPONENT ${BOWLINGBOX_RUNTIME_COMPONENT}
```

On Linux the shared object is a `LIBRARY` artifact, not `RUNTIME`, so this only affects Windows DLLs.

- [ ] **Step 4: `COMPONENT` argument on `create_application`**

In `create_application`, change `set(single_value_args NAME ENTRY UI)` to `set(single_value_args NAME ENTRY UI COMPONENT)`, and replace the install call with:

```cmake
    # COMPONENT puts the app into a named install component (the Windows
    # installer stages BOWLINGBOX_RUNTIME_COMPONENT only).
    set(_component_args)
    if(app_COMPONENT)
        set(_component_args COMPONENT ${app_COMPONENT})
    endif()
    install(TARGETS ${app_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} ${_component_args}
        BUNDLE  DESTINATION ${CMAKE_INSTALL_BINDIR} ${_component_args}
    )
```

- [ ] **Step 5: BowlingGL joins the component**

In `applications/bowlingGame/bowlingGameGL/CMakeLists.txt`:

```cmake
create_application(
    NAME         BowlingGL
    ENTRY        WX_ui
    COMPONENT    ${BOWLINGBOX_RUNTIME_COMPONENT}
    SOURCES      BowlingCanvas.h BowlingCanvas.cpp
                 BowlingFrame.h BowlingFrame.cpp
                 main.cpp
    DEPENDENCIES bowlinggl_core wx::gl OpenGL::GL
)
```

- [ ] **Step 6: Commit the licence texts and add the mingw-w64 runtime licence**

```bash
cp -r /home/amin/Dokumente/dev/Bowlingbox/licenses ./licenses
{
  echo "mingw-w64 runtime (CRT startup code and winpthreads), statically linked into"
  echo "the Windows build by BOWLINGBOX_STATIC_RUNTIME."
  echo "Source: https://github.com/mingw-w64/mingw-w64"
  echo
  echo "=============================================================================="
  echo "COPYING.MinGW-w64-runtime.txt"
  echo "=============================================================================="
  curl -fsSL https://raw.githubusercontent.com/mingw-w64/mingw-w64/master/COPYING.MinGW-w64-runtime/COPYING.MinGW-w64-runtime.txt
  echo
  echo "=============================================================================="
  echo "mingw-w64-libraries/winpthreads/COPYING"
  echo "=============================================================================="
  curl -fsSL https://raw.githubusercontent.com/mingw-w64/mingw-w64/master/mingw-w64-libraries/winpthreads/COPYING
} > licenses/mingw-w64.txt
head -5 licenses/mingw-w64.txt; grep -c . licenses/mingw-w64.txt
```
Expected: the header lines, and a line count well above 50.

- [ ] **Step 7: Write `installer/THIRD_PARTY_NOTICES.txt`**

```bash
mkdir -p installer
{
cat <<'EOF'
Bowling Box — third-party notices
=================================

Bowling Box is MIT licensed (see LICENSE). BowlingGL.exe and the
libBowlingbox*.dll files contain the following third-party code, linked
statically.

Components whose licences need no notice in binary copies:
  - wxWidgets 3.2.8        wxWindows Library Licence 3.1 (binary exception)
  - Bullet3 3.25           Zlib
  - stb_easy_font 1.1      public domain (Unlicense option)
  - zlib, NanoSVG          Zlib (built into wxWidgets)
  - libgcc, libstdc++      GPL-3.0 with the GCC Runtime Library Exception

Components whose notices follow in full:
  - GLM 1.0.1              MIT (or Happy Bunny License)
  - glad 2 OpenGL loader   (WTFPL OR CC0-1.0) AND Apache-2.0; khrplatform.h: Khronos MIT-style
  - mingw-w64 runtime      CRT startup code and winpthreads

EOF
for f in glm glad mingw-w64; do
  printf '\n\n'
  cat "licenses/$f.txt"
done
} > installer/THIRD_PARTY_NOTICES.txt
grep -c 'Apache License' installer/THIRD_PARTY_NOTICES.txt
grep -c 'G-Truc' installer/THIRD_PARTY_NOTICES.txt
grep -ci 'winpthreads' installer/THIRD_PARTY_NOTICES.txt
```
Expected: each count ≥ 1.

- [ ] **Step 8: Install `LICENSE` and the notices on Windows**

Append to `cmake/modules/InstallConfig.cmake`:

```cmake

# Windows installer: the licence files ship beside BowlingGL.exe.
if(WIN32)
    install(
        FILES
            "${CMAKE_SOURCE_DIR}/LICENSE"
            "${CMAKE_SOURCE_DIR}/installer/THIRD_PARTY_NOTICES.txt"
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT ${BOWLINGBOX_RUNTIME_COMPONENT}
    )
endif()
```

- [ ] **Step 9: Document the Windows licence situation**

Append to `licenses/README.md`, before `## Updating`:

```markdown
## Windows installer

The Windows build (`cmake/windows-installer.cmake`) differs in three ways:

- wxWidgets is built from source with `wxUSE_LIBPNG`, `wxUSE_LIBJPEG`,
  `wxUSE_EXPAT` and `wxUSE_REGEX` off and its own zlib, so libpng, libjpeg,
  Expat and PCRE2 are not in the binary.
- The GCC runtime is linked statically (`BOWLINGBOX_STATIC_RUNTIME`), which adds
  the mingw-w64 runtime: [mingw-w64.txt](mingw-w64.txt).
- GoogleTest is built statically on every platform; it is still tests-only.

The installer ships one file, `installer/THIRD_PARTY_NOTICES.txt`, holding the
full texts of GLM, glad (with Apache-2.0 and `khrplatform.h`) and the mingw-w64
runtime, the components whose licences require a notice in binary copies. When
one of those licence files changes, regenerate the notices file with the
commands in `docs/windows-installer.md`.
```

- [ ] **Step 10: Verify on Linux**

```bash
cmake --build build/t1 -j"$(nproc)"
rm -rf build/t1/stage-test
cmake --install build/t1 --component BowlingBox --prefix build/t1/stage-test
find build/t1/stage-test -type f
ci/install-smoke.sh build/t1
```
Expected: exactly one file, `build/t1/stage-test/bin/BowlingGL` (on Linux the libraries are `LIBRARY` artifacts and `LICENSE` is WIN32-only), and "install smoke test passed".

- [ ] **Step 11: Commit**

```bash
git add cmake/Common.cmake cmake/BuildUtils.cmake cmake/modules/InstallConfig.cmake \
  applications/bowlingGame/bowlingGameGL/CMakeLists.txt licenses installer/THIRD_PARTY_NOTICES.txt
git commit -m "build: BowlingBox install component, licence texts and third-party notices

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Windows build, unit tests, stage and verification in CI

This task proves the static link on the runner (spec: "the plan's first Windows task").

**Files:**
- Create: `scripts/verify_installer_stage.ps1`
- Create: `scripts/build_windows_installer.ps1` (build/test/stage/verify; the packaging steps are added in Task 4)
- Create: `.github/workflows/windows-installer.yml`

**Interfaces:**
- Consumes: `cmake/windows-installer.cmake` (Task 1), component `BowlingBox` (Task 2).
- Produces:
  - `scripts/verify_installer_stage.ps1 -AppDir <dir> [-Objdump <path>]`: exit 0 or 1.
  - `scripts/build_windows_installer.ps1 [-BuildDir] [-Msys2Root] [-InnoSetupDir] [-FetchContentDir] [-SkipBuild] [-SkipTests] [-SkipInstaller]`. The staged app folder is `<BuildDir>\installer\stage\bin`.

- [ ] **Step 1: Write the stage verifier**

`scripts/verify_installer_stage.ps1`:

```powershell
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
  "libBowlingboxRender.dll"
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
```

`-contains` and `-notcontains` are case-insensitive in PowerShell, which matches how Windows resolves DLL names.

- [ ] **Step 2: Write the build script (build, test, stage, verify)**

`scripts/build_windows_installer.ps1`:

```powershell
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
  if ($FetchContentDir) { $configure += "-DFETCHCONTENT_BASE_DIR=$FetchContentDir" }
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

throw "Packaging is added in Task 4; run with -SkipInstaller."
```

- [ ] **Step 3: Write the workflow, with a temporary branch trigger**

`.github/workflows/windows-installer.yml`:

```yaml
name: Windows installer

# Manual for now. Tag and release triggers come once versioning is automated.
on:
  workflow_dispatch:
  # TEMPORARY (removed in the last task of the installer plan): workflow_dispatch
  # only works once the workflow is on the default branch.
  push:
    branches:
      - feature/windows-installer

concurrency:
  group: windows-installer
  cancel-in-progress: true

permissions:
  contents: read

jobs:
  build-installer:
    runs-on: windows-latest
    timeout-minutes: 90

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Setup MSYS2 (UCRT64 GCC, CMake, Ninja, binutils)
        id: msys2
        uses: msys2/setup-msys2@v2
        with:
          msystem: UCRT64
          update: true
          install: >-
            mingw-w64-ucrt-x86_64-gcc
            mingw-w64-ucrt-x86_64-cmake
            mingw-w64-ucrt-x86_64-ninja
            mingw-w64-ucrt-x86_64-binutils

      - name: Cache FetchContent
        uses: actions/cache@v4
        with:
          path: .cache/fetchcontent
          key: fetchcontent-win-v1-${{ hashFiles('cmake/GTestSupport.cmake', 'cmake/BulletSupport.cmake', 'cmake/GlmSupport.cmake', 'cmake/WxSupport.cmake', 'cmake/windows-installer.cmake') }}
          restore-keys: |
            fetchcontent-win-v1-

      - name: Build, test and stage
        shell: pwsh
        env:
          MSYS2_LOCATION: ${{ steps.msys2.outputs.msys2-location }}
        run: |
          ./scripts/build_windows_installer.ps1 `
            -FetchContentDir "${{ github.workspace }}/.cache/fetchcontent" `
            -SkipInstaller

      # The stage check must reject a file outside the shipped set; prove it
      # does, so a broken check cannot pass every build.
      - name: Stage check rejects an extra file
        shell: pwsh
        env:
          MSYS2_LOCATION: ${{ steps.msys2.outputs.msys2-location }}
        run: |
          $probe = Join-Path $env:RUNNER_TEMP "stage-probe"
          Copy-Item build/windows/installer/stage/bin $probe -Recurse
          Copy-Item (Join-Path $probe "libBowlingboxCore.dll") (Join-Path $probe "stale.dll")
          ./scripts/verify_installer_stage.ps1 -AppDir $probe `
            -Objdump (Join-Path $env:MSYS2_LOCATION "ucrt64\bin\objdump.exe")
          if ($LASTEXITCODE -eq 0) { throw "verify_installer_stage.ps1 accepted stale.dll" }
          Write-Host "Rejected as expected."
          exit 0

      - name: Upload staged folder
        uses: actions/upload-artifact@v4
        with:
          name: BowlingBox_stage
          path: build/windows/installer/stage/bin
          if-no-files-found: error
```

- [ ] **Step 4: Commit, ask before pushing, push, and watch both workflows**

```bash
git add scripts/verify_installer_stage.ps1 scripts/build_windows_installer.ps1 .github/workflows/windows-installer.yml
git commit -m "ci: build, test and stage Bowling Box on Windows

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

**Ask the user for approval to push `feature/windows-installer` to `origin`.** Then:

```bash
git push -u origin feature/windows-installer
gh run list --branch feature/windows-installer --limit 4
gh run watch <windows-installer run id> --exit-status
gh run watch <CI run id> --exit-status
```
Expected: both succeed. In the Windows log, "Stage verification passed: 7 files" lists `BowlingGL.exe` importing only allowlisted system DLLs and the four `libBowlingbox*.dll`. "Rejected as expected." appears.

- [ ] **Step 5: If the Windows run fails, fix it from the evidence**

Get the log with `gh run view <id> --log-failed`, then match the failure:

| Failure | Fix |
|---|---|
| Link error involving `-static` and `libBowlingbox*.dll.a` | Fallback from the spec. In `cmake/Common.cmake`, replace the two `string(APPEND …)` lines with `string(APPEND CMAKE_EXE_LINKER_FLAGS " -static-libgcc -static-libstdc++")`, the same for `CMAKE_SHARED_LINKER_FLAGS`, and `set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic")`. Update the comment above them to say why. |
| `imports libwinpthread-1.dll` / `libssp-0.dll` after the fallback | Add `-lssp` inside the `-Bstatic … -Bdynamic` pair (ssp), or check the link line with `cmake --build … -v` to see where the driver's `-lpthread` lands. |
| `imports X, which is neither shipped nor an allowed Windows system DLL`, where X is a Windows component (lives in `C:\Windows\System32` on every Windows 10/11, not a redistributable) | Add X to `$SystemDlls` with a comment naming the wx feature that needs it. Never add `vcruntime*`, `msvcp*` or anything under `Program Files`. |
| wx configure or compile error with `wxUSE_LIBPNG=OFF` | Set `wxUSE_LIBPNG` to `builtin` in the WIN32 branch of `WxSupport.cmake` (libpng needs no binary notice), and update the comment there and in `licenses/README.md`. |
| `-Werror` warning in our code that Linux GCC does not raise | Fix the code; do not relax the flags. |
| A unit test fails on Windows only | Use `superpowers:systematic-debugging`. If it is a cross-DLL runtime problem, stop and tell the user: the spec's fallback (shared runtime in every module plus three shipped DLLs) changes the shipped set. |

Commit each fix with a message that names the evidence, push, and rerun until green.

- [ ] **Step 6: Double-check the staged binaries locally**

```bash
mkdir -p build/t3 && cd build/t3 && rm -rf stage
gh run download <windows-installer run id> --name BowlingBox_stage --dir stage
for f in stage/*.exe stage/*.dll; do echo "== $f"; objdump -p "$f" | grep 'DLL Name'; done
cd ../..
```
Expected: the same imports as the CI log, with no `libstdc++`, `libgcc`, `libwinpthread` or `libssp`. (Linux `objdump` reads PE.)

---

### Task 4: Wizard art, Inno Setup script and the installer artifact

**Files:**
- Create: `scripts/make_installer_art.ps1`
- Create: `installer/bowlingbox.iss`
- Add: `docs/Neon-Bowling-phone.png` (copy from the main checkout)
- Modify: `scripts/build_windows_installer.ps1` (replace the final `throw`)
- Modify: `.github/workflows/windows-installer.yml` (Inno Setup install and installer upload)

**Interfaces:**
- Consumes: `build_windows_installer.ps1` stage at `<BuildDir>\installer\stage\bin` (Task 3).
- Produces:
  - `scripts/make_installer_art.ps1 -Source <png> -OutDir <dir>`, which writes `wizard-164.bmp`, `wizard-246.bmp`, `wizard-328.bmp`, `small-55.bmp`, `small-83.bmp` and `small-110.bmp`.
  - `installer/bowlingbox.iss` with the defines `StageDir`, `ArtDir`, `OutputDir` and `MyAppVersion`.
  - Artifact `BowlingBox_Setup` containing `BowlingBox_Setup_<version>.exe`.

- [ ] **Step 1: Add the splash to the branch**

```bash
cp /home/amin/Dokumente/dev/Bowlingbox/docs/Neon-Bowling-phone.png docs/
file docs/Neon-Bowling-phone.png
```
Expected: `PNG image data, 941 x 1672`.

- [ ] **Step 2: Write the art generator, with its own dimension check**

`scripts/make_installer_art.ps1`:

```powershell
#Requires -Version 7
<#
.SYNOPSIS
  Derives the Inno Setup wizard images from the Bowling Box splash.

.DESCRIPTION
  Large image (Welcome/Finished pages): the splash cropped to Inno's 164:314
  aspect around its horizontal centre, at 100/150/200 % DPI.
  Small image (top right of inner pages): a square around the logo at the same
  three scales. Inno Setup picks the size that matches the screen.
  24-bit BMP, which every Inno Setup version reads.
#>
param(
  [Parameter(Mandatory = $true)][string]$Source,
  [Parameter(Mandatory = $true)][string]$OutDir
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

# The cube and the BOWLINGBOX wordmark span about y 240-620 of the 941x1672
# splash; this 600x600 square centred at (470, 430) holds both.
$LogoCrop = [System.Drawing.Rectangle]::new(170, 130, 600, 600)
$LargeSizes = @(@(164, 314), @(246, 471), @(328, 628))
$SmallSizes = @(55, 83, 110)

function Save-Scaled {
  param($Image, [System.Drawing.Rectangle]$Crop, [int]$Width, [int]$Height, [string]$Path)
  $bitmap = [System.Drawing.Bitmap]::new($Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
  $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
  $attributes = [System.Drawing.Imaging.ImageAttributes]::new()
  try {
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    # Without this, bicubic sampling blends a dark fringe in from outside the crop.
    $attributes.SetWrapMode([System.Drawing.Drawing2D.WrapMode]::TileFlipXY)
    $graphics.DrawImage($Image, [System.Drawing.Rectangle]::new(0, 0, $Width, $Height),
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
  if ($image.Height -lt $LogoCrop.Bottom -or $image.Width -lt $LogoCrop.Right) {
    throw "$Source is $($image.Width)x$($image.Height); the logo crop needs at least $($LogoCrop.Right)x$($LogoCrop.Bottom)."
  }
  $cropWidth = [int][Math]::Round($image.Height * 164 / 314)
  if ($cropWidth -gt $image.Width) { throw "$Source is too wide-short for the 164:314 wizard image." }
  $largeCrop = [System.Drawing.Rectangle]::new([int](($image.Width - $cropWidth) / 2), 0, $cropWidth, $image.Height)

  $expected = @{}
  foreach ($size in $LargeSizes) {
    $path = Join-Path $OutDir "wizard-$($size[0]).bmp"
    Save-Scaled $image $largeCrop $size[0] $size[1] $path
    $expected[$path] = $size
  }
  foreach ($side in $SmallSizes) {
    $path = Join-Path $OutDir "small-$side.bmp"
    Save-Scaled $image $LogoCrop $side $side $path
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
```

- [ ] **Step 3: Write the Inno Setup script**

`installer/bowlingbox.iss`:

```iss
; Bowling Box — per-user Windows installer (Inno Setup 6.3 or newer).
;
; Built by scripts\build_windows_installer.ps1, which passes StageDir (the
; staged BowlingBox install component), ArtDir (wizard BMPs generated from
; docs\Neon-Bowling-phone.png), OutputDir and MyAppVersion (from
; project(Bowlingbox VERSION ...) in CMakeLists.txt).
;
; Bowling Box keeps no user data in its folder: an upgrade overwrites the
; program files and an uninstall removes them.

#ifndef StageDir
  #define StageDir "..\build\windows\installer\stage\bin"
#endif
#ifndef ArtDir
  #define ArtDir "..\build\windows\installer\art"
#endif
#ifndef OutputDir
  #define OutputDir "..\build\windows\dist"
#endif
#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif

#define MyAppName "Bowling Box"
#define MyAppExeName "BowlingGL.exe"

; Bowling Box's own AppId. Never change it: it is how this installer
; recognises a copy it installed earlier and upgrades it in place.
#define MyAppId "{35C7CD0F-A6C2-4FD0-B3A1-E896DFA6B142}"

[Setup]
AppId={{#MyAppId}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppName}
VersionInfoVersion={#MyAppVersion}
DefaultDirName={autopf}\{#MyAppName}
DisableProgramGroupPage=yes
; Always per-user: no UAC prompt. {autopf} is {userpf} in this mode.
PrivilegesRequired=lowest
OutputDir={#OutputDir}
OutputBaseFilename=BowlingBox_Setup_{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
WizardImageFile={#ArtDir}\wizard-164.bmp,{#ArtDir}\wizard-246.bmp,{#ArtDir}\wizard-328.bmp
WizardSmallImageFile={#ArtDir}\small-55.bmp,{#ArtDir}\small-83.bmp,{#ArtDir}\small-110.bmp
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName} {#MyAppVersion}
; A running BowlingGL.exe holds its DLLs open; let the Restart Manager close it.
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
```

- [ ] **Step 4: Add the packaging steps to the build script**

In `scripts/build_windows_installer.ps1`, replace the last line

```powershell
throw "Packaging is added in Task 4; run with -SkipInstaller."
```

with:

```powershell
function Get-ProjectVersion {
  $text = Get-Content (Join-Path $RepoRoot "CMakeLists.txt") -Raw
  $match = [regex]::Match($text, 'project\(\s*Bowlingbox\s+VERSION\s+([^\s)]+)')
  if (-not $match.Success) { throw "No 'project(Bowlingbox VERSION x.y.z' in CMakeLists.txt." }
  $version = $match.Groups[1].Value
  if ($version -notmatch '^\d+\.\d+\.\d+$') { throw "Unexpected project version '$version'." }
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
  # x64compatible and the multi-resolution wizard images need 6.3.
  $info = (Get-Item $iscc).VersionInfo
  if ([version]::new($info.FileMajorPart, $info.FileMinorPart) -lt [version]"6.3") {
    throw "Inno Setup $($info.FileVersion) at $iscc is too old; 6.3 or newer is required."
  }
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
```

- [ ] **Step 5: Package in CI**

In `.github/workflows/windows-installer.yml`:

1. Insert before "Build, test and stage":

```yaml
      - name: Install Inno Setup
        run: choco install innosetup -y --no-progress
```

2. Rename "Build, test and stage" to "Build, test, stage and package", and replace its `run:` block (dropping `-SkipInstaller`) with:

```yaml
        run: |
          ./scripts/build_windows_installer.ps1 `
            -FetchContentDir "${{ github.workspace }}/.cache/fetchcontent" `
            -InnoSetupDir "${env:ProgramFiles(x86)}\Inno Setup 6"
```

3. Append after "Upload staged folder":

```yaml
      - name: Upload installer
        uses: actions/upload-artifact@v4
        with:
          name: BowlingBox_Setup
          path: build/windows/dist/BowlingBox_Setup_*.exe
          if-no-files-found: error
```

- [ ] **Step 6: Commit, push, watch**

```bash
git add docs/Neon-Bowling-phone.png scripts/make_installer_art.ps1 installer/bowlingbox.iss \
  scripts/build_windows_installer.ps1 .github/workflows/windows-installer.yml
git commit -m "feat(installer): Inno Setup installer for Bowling Box with splash wizard art

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git push
gh run watch <windows-installer run id> --exit-status
```
Expected: success. The log shows the six BMPs with their sizes, `Inno Setup (Bowling Box 1.4.0)`, and `Installer: …\BowlingBox_Setup_1.4.0.exe`. If Inno fails, read its line-numbered error in `gh run view <id> --log-failed` and fix the `.iss` file.

- [ ] **Step 7: Inspect the artifact**

```bash
mkdir -p build/t4 && rm -rf build/t4/setup
gh run download <windows-installer run id> --name BowlingBox_Setup --dir build/t4/setup
ls -la build/t4/setup
file build/t4/setup/*.exe
```
Expected: `BowlingBox_Setup_1.4.0.exe`, a `PE32 executable (GUI)`, a few MB in size.

---

### Task 5: Documentation, hand check list, and workflow_dispatch only

**Files:**
- Create: `docs/windows-installer.md`
- Modify: `README.md` (Quick start section)
- Modify: `.github/workflows/windows-installer.yml` (remove the temporary trigger)

**Interfaces:**
- Consumes: everything above.
- Produces: the final, manual-only workflow.

- [ ] **Step 1: Write `docs/windows-installer.md`**

````markdown
# Windows installer

`BowlingBox_Setup_<version>.exe` installs **Bowling Box** for the current user,
with nothing else to install first: no Visual C++ Redistributable, no MinGW, no
wxWidgets.

## What ships

```
%LOCALAPPDATA%\Programs\Bowling Box\
  BowlingGL.exe              the game; wxWidgets and the GCC runtime linked in
  libBowlingboxScore.dll     ┐
  libBowlingboxCore.dll      │ the Bowlingbox libraries, shared
  libBowlingboxAlley.dll     │ (Bullet3 linked into Alley)
  libBowlingboxRender.dll    ┘ (glad and stb linked into Render)
  LICENSE
  THIRD_PARTY_NOTICES.txt
```

Third-party code is linked statically and the Bowlingbox libraries stay shared.
BowlingConsole and BowlingDisplay test the libraries and are not part of the
release. Start menu entry (and optional desktop shortcut): *Bowling Box*.

`THIRD_PARTY_NOTICES.txt` carries the licences that require a notice in binary
copies (GLM, glad, the mingw-w64 runtime). The Windows wxWidgets build leaves
out PNG, JPEG, XML and regex support, which BowlingGL does not use, so their
libraries need no notice. See [licenses/README.md](../licenses/README.md).

## Getting an installer

Actions → **Windows installer** → *Run workflow* (on `main`). When it finishes,
download the `BowlingBox_Setup` artifact. From a terminal:

```bash
gh workflow run windows-installer.yml --ref main
gh run watch "$(gh run list --workflow windows-installer.yml --limit 1 --json databaseId -q '.[0].databaseId')"
gh run download --name BowlingBox_Setup --dir build/windows/dist
```

The workflow is manual until versioning is automated; the version is
`project(Bowlingbox VERSION …)` in `CMakeLists.txt`.

## Building on a Windows PC

Install MSYS2 and, in its UCRT64 shell,
`pacman -S mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,binutils}`, plus Inno Setup
6.3 or newer. Then, in PowerShell 7 from the repository root:

```powershell
pwsh -File scripts\build_windows_installer.ps1
```

It configures `build\windows` with `cmake\windows-installer.cmake`, builds
BowlingGL and the window-free unit tests, runs them, stages the `BowlingBox`
install component, checks every DLL import (`scripts\verify_installer_stage.ps1`),
generates the wizard images from `docs\Neon-Bowling-phone.png` and writes
`build\windows\dist\BowlingBox_Setup_<version>.exe`. `-SkipInstaller` stops
after the stage check.

### The stage check

The staged folder must hold exactly the seven files above. Every DLL they
import must be shipped beside them or be on the script's list of Windows system
DLLs, and none may import `libstdc++`, `libgcc`, `libwinpthread` or `libssp`.
A new import fails the build with its name; add it to the list only if it is a
part of every Windows 10/11 installation.

### Regenerating THIRD_PARTY_NOTICES.txt

After changing `licenses/glm.txt`, `glad.txt` or `mingw-w64.txt`, rebuild the
notices file: keep its header (everything before the first licence text) and
append the three files in that order, separated by two blank lines.

## Hand check list

CI cannot start the game (its Windows runner has no OpenGL 3.3). Before handing
out an installer, on a Windows 10/11 PC with a GPU:

1. Set display scaling to 150 %, run the installer. The splash art on the
   Welcome page is sharp and not stretched; no UAC prompt appears.
2. Finish with *Launch Bowling Box* ticked. The game starts; roll one frame.
3. Leave the game running and run the same installer again. Setup offers to
   close Bowling Box, closes it, and finishes.
4. Start *Bowling Box* from the Start menu; it runs.
5. Uninstall from Settings → Apps. `%LOCALAPPDATA%\Programs\Bowling Box`, the
   Start menu entry and the desktop shortcut are gone.
````

- [ ] **Step 2: Point to it from the README**

In `README.md`, after the paragraph that ends "…every build option, and troubleshooting." add:

```markdown
On Windows, install Bowling Box with the installer from the **Windows installer** workflow;
[docs/windows-installer.md](docs/windows-installer.md) explains how it is built.
```

- [ ] **Step 3: Remove the temporary trigger**

In `.github/workflows/windows-installer.yml`, delete these lines so `on:` holds only `workflow_dispatch:`:

```yaml
  # TEMPORARY (removed in the last task of the installer plan): workflow_dispatch
  # only works once the workflow is on the default branch.
  push:
    branches:
      - feature/windows-installer
```

Then check:

```bash
grep -n 'push\|TEMPORARY\|feature/windows-installer' .github/workflows/windows-installer.yml
```
Expected: no output.

- [ ] **Step 4: Commit and push**

```bash
git add docs/windows-installer.md README.md .github/workflows/windows-installer.yml
git commit -m "docs: Windows installer guide; installer workflow is manual only

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git push
gh run watch <CI run id> --exit-status
```
Expected: Linux CI green. No Windows installer run starts, since the trigger is gone. The last green Windows run is Task 4's.

- [ ] **Step 5: Hand the hand check list to the user**

Report the Task 4 artifact path (`build/t4/setup/BowlingBox_Setup_1.4.0.exe`) and ask the user to run the hand check list on a Windows PC. It is the only check of the running game.
