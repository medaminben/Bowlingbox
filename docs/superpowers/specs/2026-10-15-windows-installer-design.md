# Windows installer for Bowling Box — design

## Goal

One `BowlingBox_Setup_<version>.exe` that installs **Bowling Box** on a Windows
x64 PC with nothing else to install first: no Visual C++ Redistributable, no
MinGW, no wxWidgets. It is built on demand by a GitHub Actions workflow, after
the pattern of the VLMS repository's `windows-installer.yml`.

## What the user decided

- The product is **Bowling Box**, and its executable is BowlingGL. The installer
  ships only that app.
- BowlingConsole and BowlingDisplay test the libraries. They are not part of the
  release, and neither is built or installed by the installer build.
- **Third-party code is linked statically**: wxWidgets, Bullet3, glad, stb, and
  the MinGW runtime (libgcc, libstdc++, winpthread).
- **Our own libraries stay shared**: Score, Core, Alley and Render ship as DLLs.
- The workflow is **manual only** (`workflow_dispatch`). Tag and release triggers
  come once versioning is automated.
- The setup wizard uses the splash image `docs/Neon-Bowling-phone.png`.
- There is no `licenses\` folder in the install, only one `THIRD_PARTY_NOTICES.txt`
  covering the components whose licences require a notice in binary copies.
- The manual is not shipped.
- All build output, including the finished `.exe`, goes under `build/windows/`.

## Installed layout

```
%LOCALAPPDATA%\Programs\Bowling Box\      ({autopf} in per-user mode)
  BowlingGL.exe              GL app + static wxWidgets + static MinGW runtime
  libBowlingboxScore.dll     ┐
  libBowlingboxCore.dll      │ our libraries (each with a static MinGW runtime)
  libBowlingboxAlley.dll     │ + Bullet3 statically inside
  libBowlingboxRender.dll    ┘ + glad and stb statically inside
  LICENSE                    Bowling Box's MIT licence
  THIRD_PARTY_NOTICES.txt
  unins000.exe, unins000.dat (Inno Setup)
```

Shortcuts named "Bowling Box" go in the Start menu (always) and on the desktop
(optional task, unchecked by default).

Every DLL any staged file imports is either in this folder or a Windows system
DLL. The build checks this (see *Stage verification*).

## Toolchain

MSYS2 **UCRT64** GCC on a `windows-latest` runner, with Ninja and CMake from
MSYS2. It is the same compiler family as the Linux CI, so
`bowlingbox_set_warnings` (`-Wall -Wextra -Wpedantic -Werror`) behaves the same.
The rejected alternatives were MSVC with `/MT` (a second set of warnings to fix
across every library for no gain in the result) and cross-compiling from Linux
(a fragile wxWidgets cross-build, and the binary never runs on Windows in CI).

## Build configuration

### `cmake/windows-installer.cmake` (initial cache, passed with `-C`)

```cmake
set(CMAKE_BUILD_TYPE          Release CACHE STRING "")
set(BUILD_SHARED_LIBS         ON      CACHE BOOL "")   # our libraries as DLLs
set(BUILD_APPS                ON      CACHE BOOL "")
set(BUILD_WX_UI               ON      CACHE BOOL "")
set(BUILD_TESTING             ON      CACHE BOOL "")   # unit tests run before packaging
set(GENERATE_SCAFFOLD         OFF     CACHE BOOL "")
set(BOWLINGBOX_USE_SYSTEM_WX  OFF     CACHE BOOL "")   # wxWidgets static, from source
set(BOWLINGBOX_STATIC_RUNTIME ON      CACHE BOOL "")
```

The Linux CI configuration does not change.

### `BOWLINGBOX_STATIC_RUNTIME` (new option, `cmake/Common.cmake`, default OFF)

**Requirement.** When it is ON and the compiler is MinGW GCC, the exe, each of
our four DLLs and the test executables link the GCC runtime statically. None of
them imports `libstdc++-6.dll`, `libgcc_s_seh-1.dll`, `libwinpthread-1.dll` or
`libssp-0.dll`. The stage verification enforces this. On other compilers the
option does nothing and prints a warning.

**Mechanism.** It adds `-static-libgcc -static-libstdc++ -static` to
`CMAKE_EXE_LINKER_FLAGS` and `CMAKE_SHARED_LINKER_FLAGS`. On PE targets, `-static`
only changes how `-l` names are searched (`.a` before `.dll.a` or `.dll`). Import
libraries passed by full path, as CMake passes `libBowlingbox*.dll.a`, still link.
The Windows system libraries resolve to MinGW's import archives
(`libkernel32.a`, `libopengl32.a`), so those DLLs are still imported. Bare
`-static` is used rather than a `-Wl,-Bstatic -lwinpthread -Wl,-Bdynamic`
sandwich because GCC's driver appends libstdc++ and its own implicit `-lpthread`
after the user's flags. A sandwich placed earlier is scanned before anything
needs winpthread, and the driver's later `-lpthread` then resolves to the DLL.
The plan's first Windows task proves the link on the runner. If bare `-static`
fails there, the fallback is the sandwich placed in
`CMAKE_CXX_STANDARD_LIBRARIES`, which comes at the end of the link line, checked
by the same stage verification.

**GoogleTest is linked statically.** It is third-party, and with
`BUILD_SHARED_LIBS=ON` it would otherwise build as a DLL. `cmake/GTestSupport.cmake`
saves and restores `BUILD_SHARED_LIBS` around `FetchContent_MakeAvailable`, the way
`BulletSupport.cmake` does. The Linux CI also gets a static GoogleTest, which
changes nothing visible there.

**Known trade-off.** With one runtime copy per module, std types and exceptions
cross DLL boundaries between separate libstdc++ copies. On x64 MinGW this works:
unwinding is SEH, `operator new` and `delete` both reach the shared UCRT heap, and
`type_info` compares by name. The unit tests below run against exactly this build
and exercise those boundaries. If they ever show a problem, the fallback is to
turn the option off for **every** module (the exe and all four DLLs) and ship
`libstdc++-6.dll`, `libgcc_s_seh-1.dll` and `libwinpthread-1.dll` beside them.
Then there is one runtime copy for the whole process. Turning it off only for the
DLLs would leave the exe with its own copy, and the boundary would remain.

### `cmake/WxSupport.cmake` (source-build branch only)

On `WIN32`:
- `wxUSE_ZLIB=builtin`, and no `find_package(ZLIB)`, since Windows has no system zlib.
- `wxUSE_LIBPNG`, `wxUSE_LIBJPEG`, `wxUSE_EXPAT` and `wxUSE_REGEX` set to `OFF`.
  BowlingGL uses no wx image, XML or regex classes. Dropping them takes libjpeg,
  Expat and PCRE2, all of which need binary notices, out of the exe. If wxMSW
  fails to build with `wxUSE_LIBPNG=OFF`, PNG goes back to `builtin`: libpng needs
  no binary notice.

The Linux branch keeps its current settings.

### Only BowlingGL is built

`cmake --build build/windows --target BowlingGL <unit test targets>`. The
console and display apps are configured but never compiled, because they are
not part of the release (a product decision; `terminal.cpp` has a `_WIN32` path).
No `if(WIN32)` guards are added to the app directories.

### Install component `BowlingBox`

Install rules stay as they are for the existing Linux install and consumer
smoke test, plus a component on what ships:

- `create_application` gains an optional `COMPONENT` argument, passed to its
  `install(TARGETS …)`. BowlingGL passes `COMPONENT BowlingBox`. The others keep
  the default component.
- `create_library` puts the library's `RUNTIME` destination (the DLL) in component
  `BowlingBox`. `ARCHIVE` (the import library), headers and the CMake package
  files stay in the default component.
- On `WIN32`, the root `LICENSE` and `installer/THIRD_PARTY_NOTICES.txt` are
  installed to `${CMAKE_INSTALL_BINDIR}` in component `BowlingBox`.

`cmake --install build/windows --component BowlingBox --prefix <stage>` then
produces exactly the installed layout above in `<stage>/bin`, which is the
folder Inno Setup packages (`StageDir`).

## Licences

`THIRD_PARTY_NOTICES.txt` (committed under `installer/`) has a short header
naming everything statically inside the binaries, then the full notice of each
component whose licence requires one in object-code copies:

| Component | Why it is in the file |
|---|---|
| GLM (MIT) | MIT asks for the notice in all copies |
| glad 2: Apache-2.0 part, plus `khrplatform.h` (Khronos MIT-style) | Apache-2.0 §4, MIT |
| mingw-w64 runtime, including winpthreads | statically linked, MIT/ZPL-style licence text required |

The header names, without full texts, the components that need no binary
notice: wxWidgets (wxWindows Library Licence binary exception), Bullet3 (Zlib),
stb_easy_font (public-domain option), wx's built-in zlib and NanoSVG (Zlib), and
libgcc/libstdc++ (GCC Runtime Library Exception).

The texts come from `licenses/*.txt` in the repository. The mingw-w64 text is
copied once from the MSYS2 package's licence directory into `licenses/` and
listed in `licenses/README.md`. `licenses/` (currently untracked) is committed
with this work. `licenses/README.md` gains a Windows section describing the
reduced wx feature set and the static MinGW runtime.

## Installer: `installer/bowlingbox.iss` (Inno Setup 6.3 or newer)

A cut-down version of VLMS's `vlms.iss`, without its existing-install,
backup and what's-new code. Bowling Box keeps no user data in its folder, so an
upgrade overwrites the files and an uninstall removes them.

- `AppId`: a new fixed GUID with a "never change" comment.
- `AppName=Bowling Box`. `AppVersion` and `VersionInfoVersion` come from
  `/DMyAppVersion`, which the script reads from `project(Bowlingbox VERSION x.y.z)`.
- `AppPublisher=Bowling Box`.
- `DefaultDirName={autopf}\Bowling Box`, `DisableProgramGroupPage=yes`,
  `PrivilegesRequired=lowest`, with no `PrivilegesRequiredOverridesAllowed`. The
  install is always per-user. `{autopf}` resolves to `{userpf}` in that mode, and
  stays correct if an all-users mode is ever allowed.
- `ArchitecturesAllowed` and `ArchitecturesInstallIn64BitMode` set to `x64compatible`.
- `Compression=lzma2/ultra64`, `SolidCompression=yes`, `WizardStyle=modern`.
- `CloseApplications=yes`, `RestartApplications=no`.
- `OutputDir` comes from `/DOutputDir` (`build/windows/dist`).
  `OutputBaseFilename=BowlingBox_Setup_{#MyAppVersion}`.
- `UninstallDisplayIcon={app}\BowlingGL.exe`. The exe has no icon resource yet,
  so Windows shows its default icon (adding one is out of scope).
- `[Files]`: `{#StageDir}\*`, recursive, `ignoreversion`.
- `[Icons]`: Start menu entry, plus a desktop entry under the `desktopicon` task.
- `[Run]`: "Launch Bowling Box", `postinstall nowait skipifsilent`.
- `WizardImageFile` and `WizardSmallImageFile` are comma-separated lists of the
  generated BMPs (see below), passed in as `/DArtDir`.

### Wizard art from the splash

`docs/Neon-Bowling-phone.png` (941×1672, portrait key art with the logo) is the
only source. It is committed with this work (currently untracked).
`scripts/make_installer_art.ps1` derives 24-bit BMPs with .NET `System.Drawing`,
so nothing extra needs installing.

- **Large image** (Welcome and Finished pages): cropped to the 164:314 aspect
  around the horizontal centre, then resized with high-quality bicubic to
  164×314, 246×471 and 328×628 (100/150/200 % DPI).
- **Small image** (top right of inner pages): a square crop centred on the logo,
  resized to 55, 83 and 110 px. The crop is a named constant in the script: a
  600×600 square centred at (470, 430) in source pixels, which holds the cube and
  the BOWLINGBOX wordmark (the logo spans about y 240–620).

The output goes to `build/windows/installer/art/` and is not committed.

## Build script: `scripts/build_windows_installer.ps1`

It runs in CI and on a Windows PC with MSYS2 (UCRT64 packages as in the
workflow) and Inno Setup 6.3 or newer. Parameters: `-BuildDir` (default
`build/windows`), `-Msys2Root` (default `C:\msys64`, or `$env:MSYS2_LOCATION`),
`-InnoSetupDir`, `-FetchContentDir`, `-SkipBuild`, `-SkipTests`, `-SkipInstaller`.
`$ErrorActionPreference = "Stop"`, and every native call checks `$LASTEXITCODE`.

1. **Configure**: `cmake -S . -B build/windows -G Ninja -C cmake/windows-installer.cmake`,
   plus `-DFETCHCONTENT_BASE_DIR=…` when `-FetchContentDir` is given (CI always
   passes it), with `ucrt64\bin` first on `PATH`.
2. **Build**: target `BowlingGL` and the window-free unit-test targets (the four
   library tests and `test_bowlinggl`). The plan takes the exact target names from
   `create_library` and `build_gtest_executable`. `test_bowlingconsole` (not shipped)
   and `test_bowlingdisplay` (gui) are not built.
3. **Test**: `ctest --label-regex unit --output-on-failure --no-tests=error`, the
   same filter as the Linux CI. `-R` can't select by executable: `gtest_discover_tests`
   registers GoogleTest case names, not `test_…` executable names. `--no-tests=error`
   turns a filter that matches nothing into a failure instead of a silent pass.
   `test_bowlingconsole` and `test_bowlingdisplay` are not built. The
   `<target>_NOT_BUILT` placeholders that `gtest_discover_tests` registers for
   unbuilt executables carry no labels, so the filter skips them. The plan checks
   this on the first run (`ctest -N --label-regex unit` lists only built tests).
4. **Stage**: wipe `build/windows/installer/stage`, then
   `cmake --install … --component BowlingBox`. The app folder is `stage\bin`.
5. **Verify the stage** (see below).
6. **Art**: `make_installer_art.ps1`.
7. **Version**: parse `project(Bowlingbox VERSION x.y.z …)` from `CMakeLists.txt`.
   Anything that is not `\d+\.\d+\.\d+` fails the build.
8. **Package**: `ISCC /DStageDir=… /DArtDir=… /DOutputDir=… /DMyAppVersion=… installer\bowlingbox.iss`,
   then print the path and size of `build/windows/dist/BowlingBox_Setup_<v>.exe`.

### Stage verification: `scripts/verify_installer_stage.ps1`

- **Required files**: `BowlingGL.exe`, the four `libBowlingbox*.dll`, `LICENSE`,
  `THIRD_PARTY_NOTICES.txt`.
- **Nothing extra**: the only `.exe` is `BowlingGL.exe`, and the only `.dll` files
  are the four `libBowlingbox*.dll`. Any other `.exe`, `.dll`, `.a`, `.h` or `.cmake`
  fails the build.
- **Imports**: `objdump -p` on every `.exe` and `.dll`. Each `DLL Name:` entry must
  be either a staged file or on an allowlist of Windows system DLLs, matched
  case-insensitively: `KERNEL32`, `USER32`, `GDI32`, `OPENGL32`, `COMCTL32`,
  `COMDLG32`, `SHELL32`, `SHLWAPI`, `OLE32`, `OLEAUT32`, `UUID`, `ADVAPI32`,
  `WINSPOOL`, `WINMM`, `VERSION`, `UxTheme`, `OLEACC`, `WS2_32`, `RPCRT4`,
  `IMM32`, `MSIMG32`, `ucrtbase`, `api-ms-win-*`. The plan settles the final list
  from the first real build. An import that is neither fails the build with its
  name and the importing file.
- **No MinGW runtime DLL imports**: `libstdc++*`, `libgcc*`, `libwinpthread*` and
  `libssp*` fail explicitly, with a message pointing at `BOWLINGBOX_STATIC_RUNTIME`.

## Workflow: `.github/workflows/windows-installer.yml`

```yaml
on:
  workflow_dispatch:   # manual for now; tag/release triggers once versioning is automated
concurrency: { group: windows-installer, cancel-in-progress: true }
permissions: { contents: read }
```

One job on `windows-latest`, with `timeout-minutes: 90`:
1. `actions/checkout@v4`.
2. `msys2/setup-msys2@v2`, `msystem: UCRT64`, installing `mingw-w64-ucrt-x86_64-gcc`,
   `-cmake`, `-ninja` and `-binutils`.
3. `actions/cache@v4` for `.cache/fetchcontent` (keyed on `cmake/*Support.cmake`
   and `cmake/windows-installer.cmake`). The script passes it to the configure
   step as `-DFETCHCONTENT_BASE_DIR=<workspace>/.cache/fetchcontent` (FetchContent
   reads the cache variable, not an environment variable). This saves downloads.
   The wxWidgets compile itself is not cached.
4. `choco install innosetup -y`. The script then checks that `ISCC.exe` is 6.3 or
   newer (needed for `x64compatible` and the multi-resolution wizard images) and
   fails otherwise.
5. `pwsh ./scripts/build_windows_installer.ps1`, with `MSYS2_LOCATION` from step 2.
6. `actions/upload-artifact@v4`, artifact name `BowlingBox_Setup`, path
   `build/windows/dist/BowlingBox_Setup_*.exe`, `if-no-files-found: error`.

## Documentation

- `docs/windows-installer.md`: what ships and why (static third-party, shared
  libraries, the notices file), how to run the workflow and download the
  artifact, how to build locally on Windows, and a short manual check list:
  install, launch, play a frame, uninstall, folder gone.
- `README.md`: one line pointing to it.
- `licenses/README.md`: the Windows section mentioned above.

## Testing

- The unit tests run on Windows against the shipped build configuration (step 3).
- Stage verification enforces the dependency rule (step 5).
- There is no automated launch of `BowlingGL.exe` in CI. The runner only offers
  OpenGL 1.1 (GDI generic), and the app needs 3.3 core. The first installer from
  the workflow is checked by hand on a real Windows PC with the check list in
  `docs/windows-installer.md`.
- Linux CI must stay green. The CMake changes (new option, component arguments,
  `WIN32` branches) are inert on Linux, and the existing install smoke test
  (`ci/install-smoke.sh`) still passes.

## Out of scope

- Version automation and release uploads.
- An application icon or version resource in `BowlingGL.exe`.
- Code signing.
- BowlingConsole and BowlingDisplay on Windows (not part of the release).
- 32-bit or ARM64 builds.
