# Bowlingbox manual

How to build Bowlingbox, play the three bowling games, and work on the code. For how the code is
structured, see [ARCHITECTURE.md](ARCHITECTURE.md).

- [1. Requirements](#1-requirements)
- [2. Building](#2-building)
- [3. Playing BowlingGL](#3-playing-bowlinggl)
- [4. Playing BowlingConsole](#4-playing-bowlingconsole)
- [5. BowlingDisplay (wx)](#5-bowlingdisplay-wx)
- [6. Scoring rules](#6-scoring-rules)
- [7. Troubleshooting](#7-troubleshooting)
- [8. Developer guide](#8-developer-guide)
- [Third-party licences](#third-party-licences)

## 1. Requirements

| Tool | Version |
|---|---|
| C++ compiler | C++20: GCC 11+ or Clang 14+ (Linux is the tested platform) |
| CMake | 3.29 or newer |
| Git | to fetch GoogleTest |
| Network access | on the first configure (or a pre-filled `build/_deps`, see [Offline builds](#offline-builds)) |

System packages on Debian/Ubuntu:

```bash
sudo apt install build-essential cmake git ninja-build \
                 libgtk-3-dev libgl1-mesa-dev libx11-dev zlib1g-dev
```

- `libgtk-3-dev`, `libgl1-mesa-dev`, `libx11-dev` and `zlib1g-dev` are needed for the wx apps
  (`BUILD_WX_UI=ON`): BowlingGL and BowlingDisplay.

Bullet3 3.25, GLM 1.0.1 and GoogleTest 1.15.2 are downloaded and built during configure. You do
not install them yourself.

wxWidgets is taken from the system when an installed wxWidgets 3.2 or newer is found (a CMake
package, or `wx-config`). If none is found, wxWidgets 3.2.8 is downloaded and built from source.
To use the system package on Debian/Ubuntu:

```bash
sudo apt install libwxgtk3.2-dev
```

## 2. Building

### Standard build

```bash
cmake -S . -B build -G Ninja
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The first configure and build take several minutes, because Bullet (and wxWidgets, when it is not
installed on the system) is compiled from source. Later builds are incremental.

Executables are written to `<build-dir>/bin/` (`build/bin/` with the commands above):

| Binary | What it is |
|---|---|
| `BowlingGL` | 3D bowling game (wxWidgets + OpenGL) |
| `BowlingConsole` | Text bowling score keeper |
| `BowlingDisplay` | wx score display (with `BUILD_WX_UI=ON`) |
| `test_*` | GoogleTest suites |

### Build options

Pass options as `-D<OPTION>=ON|OFF` on the first `cmake` call.

| Option | Default | Effect |
|---|---|---|
| `BUILD_APPS` | `ON` | Build the applications |
| `BUILD_WX_UI` | `ON` | Build `BowlingGL` and `BowlingDisplay` (uses the system wxWidgets, or fetches it) |
| `BOWLINGBOX_USE_SYSTEM_WX` | `ON` | Use an installed wxWidgets if found; `OFF` always builds it from source |
| `BUILD_TESTING` | `ON` | Fetch GoogleTest and build the test executables |
| `BUILD_SHARED_LIBS` | `ON` | Bowlingbox libraries as shared (`.so` / `.dll`) instead of static |
| `GENERATE_SCAFFOLD` | `ON` | Generate missing libraries listed in `BOWLINGBOX_LIBRARIES` from templates |
| `ENABLE_COVERAGE` | `OFF` | Add `--coverage` and a `coverage` target (GCC/Clang) |
| `ENABLE_CLANG_TIDY` | `OFF` | Run `clang-tidy` during compilation |
| `BUILD_VERBOSE_OUTPUT` | `OFF` | Verbose makefiles and CMake variable dumps |
| `BOWLINGBOX_FETCH_OFFLINE` | `OFF` | Use only already-downloaded dependencies |

The dependency versions can be overridden with `BOWLINGBOX_BULLET_VERSION`, `BOWLINGBOX_GLM_VERSION`,
`BOWLINGBOX_WX_VERSION` and `BOWLINGBOX_GTEST_VERSION`.

### Common variants

```bash
# Console game and libraries only (no wxWidgets download, no GTK needed)
cmake -S . -B build -DBUILD_WX_UI=OFF

# Release build
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
```

Keep extra build directories under `build/` (for example `build/release`). It is git-ignored, and each directory has its own `bin/` and `lib/`.

### Offline builds

After one successful online configure, `build/_deps` contains every downloaded dependency. Then:

```bash
cmake -S . -B build -DBOWLINGBOX_FETCH_OFFLINE=ON
```

You can also share a download cache between build directories with
`-DFETCHCONTENT_BASE_DIR=/path/to/cache`, as CI does.

### Installing the libraries

```bash
cmake --install build --prefix ~/.local
```

This installs the Bowlingbox libraries, headers and `lib/cmake/Bowlingbox/BowlingboxConfig.cmake`, so other
projects can use `find_package(Bowlingbox)` and link `Bowlingbox::Score`, `Bowlingbox::Alley` and so on.

CI checks this on every run with `ci/install-smoke.sh`. Code that includes
`Bowlingbox/Render/Render.h` must also find GLM itself.

## 3. Playing BowlingGL

```bash
./build/bin/BowlingGL
```

The window shows the lane with ten pins and the ball. The scoreboard is at the top, and the power
bar, a hint and the result of the last throw are at the bottom. The status bar shows the phase, the score
and the number of pins standing.

### Throwing a ball

1. **Position**: move the ball left or right on the approach.
2. **Aim**: turn the throwing angle (up to ±6°). A translucent cyan line on the lane shows the
   direction.
3. **Charge**: hold Space (or the left mouse button). The power bar rises to 100 % and falls back
   to 0 once a second.
4. **Throw**: let go when the power is where you want it. More power means a faster ball (4 to
   12 m/s).
5. Watch the ball and the camera follow it down the lane. When everything is at rest, the fallen
   pins are counted and scored. After a short pause the next ball is ready.

On the second ball of a frame, the pins that are still standing stay exactly where they were.

### Controls

| Action | Keyboard | Mouse |
|---|---|---|
| Move ball left / right | ← / → | move horizontally while aiming |
| Turn aim angle | A / D | drag with the right button |
| Charge power | hold Space | hold the left button |
| Throw | release Space | release the left button |
| New game | N or Ctrl+N | Game ▸ New Game |
| Change scenery | T | View ▸ Theme |
| Mute / unmute sound | M | Sound ▸ Mute |
| Volume up / down | + / − | Sound ▸ Volume up / down |
| Background ambience on / off | B | Sound ▸ Background ambience |
| Show controls | | Help ▸ Controls |
| Quit | Esc or Ctrl+Q | Game ▸ Quit |

With the keyboard you can still move and aim while charging; mouse aiming works only before you start charging. Inputs that make no sense in the current phase (for example aiming while the ball rolls) are ignored.

### Tips

- A centred throw at about 80 % power from an offset of about 0.08 m usually hits the pocket.
- Holding → or ← moves the ball over the gutter. That throw goes straight into the gutter and is
  scored `-`.

### Scenery

BowlingGL draws one of three themes around the lane: Neon Hall (default), Classic Alley and
Outdoor. Press `T` or use View ▸ Theme to switch. Start in a theme with:

```bash
./build/bin/BowlingGL --theme=outdoor     # neon, classic or outdoor; case does not matter
```

An unknown name prints `unknown theme '<name>'; use neon, classic or outdoor` and starts Neon Hall.

### Sound

BowlingGL plays the ball, the pins, a chime for a strike and a bell for a spare, and a background
ambience for each theme: a bowling alley (Classic), an arcade (Neon) and a garden (Outdoor).
The Sound menu and keys control it: `M` mutes, `+` / `−` change the volume in 10% steps, `B` turns the
background ambience off and on. The status bar shows the volume, or "sound muted". The game remembers
these settings (`~/.BowlingGL` on Linux, the registry on Windows); `--mute` starts muted for one
session. Without an audio device the game prints `sound disabled: <reason>` once and stays silent.

### Graphics

BowlingGL renders with soft shadows, a reflecting lane, glow (bloom) and a filmic tone curve.
If your graphics card can't do this, it falls back to a simpler look by itself, and prints the reason
to the terminal. Two environment variables help when something looks wrong:

| Variable | Effect |
|---|---|
| `BOWLINGBOX_RENDER=basic` | Use the simpler look: same lighting and materials, no shadows, reflection or glow. |
| `BOWLINGBOX_RENDER_STATS=1` | Print the GPU time of each render pass once a second. |

The GPU times only show the real cost when the graphics card is fully loaded: with a small window it
clocks down until the work fills the frame, so the numbers look larger. Measure fullscreen.

## 4. Playing BowlingConsole

```bash
./build/bin/BowlingConsole
```

BowlingConsole is a score keeper for real games: type the number of pins each ball knocked down.

| Input | When | Effect |
|---|---|---|
| `p` | anywhere | start a new game |
| `0` … `n` | in a game | record a roll; `n` is the number of pins still standing |
| `r` | anywhere | restart the game |
| `q` | anywhere | quit |

Blanks around the input are ignored. Anything else is rejected with "Invalid choice!": a pin count larger than
the number of pins standing, a number before a game is started, or a number after the game is over.
The program also ends when input ends (for example when stdin is a pipe). After each roll the score
grid is redrawn with the current total; it shows 10, 5, 2 or 1 frames per row depending on the
terminal width. When the tenth frame is complete, the final score is shown.

## 5. BowlingDisplay (wx)

The default wx build (`BUILD_WX_UI=ON`) produces `./build/bin/BowlingDisplay` next to BowlingGL.
It is a form-based score display backed by the same `ScoreCalculator`. It uses `wx::core` and
`wx::base`, not OpenGL.

`test_bowlingdisplay` opens that window, so it needs a display or `xvfb-run`.

## 6. Scoring rules

Bowlingbox scores standard ten-pin bowling:

- A game has 10 frames. In each frame you get up to two balls to knock down 10 pins.
- **Strike** (`X`): all 10 pins with the first ball. The frame scores 10 plus the next two balls.
- **Spare** (`/`): all 10 pins with two balls. The frame scores 10 plus the next ball.
- **Open frame**: the frame scores the number of pins knocked down.
- **Miss** (`-`): a ball that knocks down no pins.
- **Tenth frame**: a strike earns two bonus balls and a spare one, so the frame has up to three
  balls. The rack is set up again whenever it is cleared; otherwise the next ball faces the pins
  still standing.
- A ball can never knock down more pins than are standing. The calculator refuses such input.
- A perfect game is 12 strikes = 300.

On the scoreboard, the top row shows each ball and the bottom row shows the running total. A
frame's total appears once its bonus balls have been played.

## 7. Troubleshooting

| Symptom | Cause and fix |
|---|---|
| `Package 'gtk+-3.0' not found` / OpenGL not found | Install `libgtk-3-dev libgl1-mesa-dev`, or build without the wx apps (`-DBUILD_WX_UI=OFF`) |
| `DSO missing from command line` (X11) | Install `libx11-dev` and configure again |
| Configure hangs or fails while downloading | Check network or proxy access to github.com. Once `build/_deps` is filled, use `-DBOWLINGBOX_FETCH_OFFLINE=ON` |
| BowlingGL crashes at start on Wayland | The app switches to `GDK_BACKEND=x11` (XWayland) on its own. Make sure XWayland is available. Only set `GDK_BACKEND=wayland` if your wx/GL stack supports it |
| Black window or an OpenGL error dialog | The GPU or driver lacks OpenGL 3.3 core. Run `glxinfo -B` and update the Mesa or vendor driver |
| `CMake 3.29 or higher is required` | Install a newer CMake (for example `pip install cmake`, or from cmake.org) |
| Build fails on a warning | Project targets use `-Werror`. Fix the warning; do not disable the flag |

## 8. Developer guide

### Running tests

```bash
ctest --test-dir build --output-on-failure            # everything
ctest --test-dir build -L unit                        # only the unit label (as in CI)
ctest --test-dir build -R test_alley                  # by name
./build/bin/test_bowlingbox_score --gtest_filter='*strike*'
```

After changes to Alley, Render or BowlingGL, also go through the manual checklist in
[applications/bowlingGame/README.md](../applications/bowlingGame/README.md).

### Coverage

```bash
cmake -S . -B build/coverage -DENABLE_COVERAGE=ON
cmake --build build/coverage -j
cmake --build build/coverage --target coverage   # needs lcov + genhtml
xdg-open build/coverage/coverage/html/index.html
```

### Static analysis

```bash
cmake -S . -B build/tidy -DENABLE_CLANG_TIDY=ON
cmake --build build/tidy
```

`compile_commands.json` is always generated in the build directory for editors and clangd.

### Adding a library

1. Add its name to `BOWLINGBOX_LIBRARIES` in the root `CMakeLists.txt`, and to
   `_bowlingbox_library_dirs` in `libraries/CMakeLists.txt`.
2. Configure with `GENERATE_SCAFFOLD=ON`. This creates `libraries/<Name>/` from
   `cmake/template/lib`: a public header under `include/Bowlingbox/<Name>/`, a pimpl source, a test,
   and a `CMakeLists.txt` that calls `create_library`.
3. Add dependencies to `PUBLIC_DEPS` (types that appear in your public headers) or `PRIVATE_DEPS`
   (implementation only). Wrap third-party targets in `$<BUILD_INTERFACE:…>` if they should not
   be part of the installed package.
4. Put tests in `test/src/test_<Name>.cpp`. They build as `test_bowlingbox_<name>` and get the `unit`
   label.

### Conventions

- C++20, no compiler extensions, warnings as errors.
- Public API in `include/Bowlingbox/<Lib>/`, namespaced `Bowlingbox::<Lib>`, and exported with
  `BOWLINGBOX_<LIB>_API`. Everything else stays in `src/`.
- Depend on interfaces (`IBowlingGame`, `IPhysicsWorld`, `ICanvas2D`, `ITerminal`). Choose the
  concrete class in `main` or a default constructor, and inject fakes in tests.
- Commands that can be refused return `Result<>` and do not throw.
- Test names must start with `test_` and be lowercase.

### Continuous integration

Every push to `main` or `feature/**`, and every pull request to `main`, builds and tests on Ubuntu
24.04 (`.github/workflows/ci.yml`). Add the `automerge` label to a pull request to squash-merge it
automatically once CI is green. The merge only happens if the pull request still points at the commit CI tested. A merge made by the workflow does not start another CI run on `main`.

### Versioning

Every pull request merged into `main` gets its own [semantic version](https://semver.org).
`.github/workflows/version.yml` tags the merge commit `vX.Y.Z` and publishes a GitHub release
for it. A label on the pull request chooses the bump:

| Label | Bump | Use for |
|---|---|---|
| `semver:major` | 1.4.2 → 2.0.0 | Breaking changes to the public library API |
| `semver:minor` | 1.4.2 → 1.5.0 | New features |
| `semver:patch` | 1.4.2 → 1.4.3 | Fixes, refactors (also the default when no `semver:` label is set) |
| `semver:none` | no release | Docs or CI only |

While the pull request is open, the Version check shows the version it will get. The check fails
if more than one `semver:` label is set. Merges made by the `automerge` workflow are versioned
too.

CMake takes `Bowlingbox_VERSION` (and the version of the installed package) from the nearest
`vX.Y.Z` tag, through `cmake/GitVersion.cmake`. Without git or tags, for example in a shallow
clone, it uses the fallback version in that file. To set the version by hand, pass
`-DBOWLINGBOX_VERSION=X.Y.Z`. Each library keeps its own version in its `project()` call,
and that version sets the shared library's `SOVERSION`. Run `git fetch --tags` and reconfigure
to pick up a new release in an existing build tree.

## Third-party licences

Bowlingbox itself is MIT licensed (see [LICENSE](../LICENSE)). It uses:

| Component | Licence |
|---|---|
| Bullet3 | zlib |
| wxWidgets | wxWindows Library Licence (LGPL with a static-linking exception) |
| GLM | MIT (or Happy Bunny) |
| GoogleTest | BSD-3-Clause (tests only, not shipped) |
| GLAD 2 | generated loader (WTFPL OR CC0-1.0) AND Apache-2.0 |
| stb_easy_font | MIT or public domain (Unlicense) |

See [licenses/](../licenses/README.md) for versions, the libraries bundled into a
source-built wxWidgets, and the full licence texts.
