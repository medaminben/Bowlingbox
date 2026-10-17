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
  libBowlingboxRender.dll    │ (glad and stb linked into Render)
  libBowlingboxAudio.dll     ┘ (miniaudio linked into Audio)
  LICENSE
  THIRD_PARTY_NOTICES.txt
```

Third-party code is linked statically and the Bowlingbox libraries stay shared.
BowlingConsole and BowlingDisplay test the libraries and are not part of the
release. Start menu entry (and optional desktop shortcut): *Bowling Box*.

`THIRD_PARTY_NOTICES.txt` carries the licences that require a notice in binary
copies (GLM, glad, the mingw-w64 runtime). The Windows wxWidgets build leaves
out JPEG, XML and regex support, which BowlingGL does not use, so libjpeg, Expat
and PCRE2 are not in the binary. (wxMSW requires libpng, which needs no notice.) See [licenses/README.md](../licenses/README.md).

## Getting an installer

Actions → **Windows installer** → *Run workflow* (on `main`). When it finishes,
download the `BowlingBox_Setup` artifact. From a terminal:

```bash
gh workflow run windows-installer.yml --ref main
gh run watch "$(gh run list --workflow windows-installer.yml --limit 1 --json databaseId -q '.[0].databaseId')"
gh run download --name BowlingBox_Setup --dir build/windows/dist
```

The workflow is manual for now. The version is the one CMake configures: the
nearest `vX.Y.Z` tag, which `.github/workflows/version.yml` creates for every
merged pull request (see `cmake/GitVersion.cmake`).

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

The staged folder must hold exactly the eight files above. Every DLL they
import must be shipped beside them or be on the script's list of Windows system
DLLs, and none may import `libstdc++`, `libgcc`, `libwinpthread` or `libssp`.
A new import fails the build with its name; add it to the list only if it is a
part of every Windows 10/11 installation.

`scripts\test_verify_installer_stage.ps1` (run by the workflow after the build)
proves the check still rejects a stale file, objdump output it cannot parse,
and a foreign import. It also runs on Linux with PowerShell 7 and binutils
against a downloaded `BowlingBox_stage` artifact.

### Regenerating THIRD_PARTY_NOTICES.txt

After changing `licenses/glm.txt`, `glad.txt` or `mingw-w64.txt`, rebuild the
notices file: keep its header (everything before the first licence text) and
append the three files in that order, separated by two blank lines.

## Manual checklist

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
