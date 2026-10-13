# Gamebox manual

How to build Gamebox, play the three bowling games, and work on the code. For how the code is
structured, see [ARCHITECTURE.md](ARCHITECTURE.md).

- [1. Requirements](#1-requirements)
- [2. Building](#2-building)
- [3. Playing BowlingGL](#3-playing-bowlinggl)
- [4. Playing BowlingConsole](#4-playing-bowlingconsole)
- [5. BowlingDisplay (Qt)](#5-bowlingdisplay-qt)
- [6. Scoring rules](#6-scoring-rules)
- [7. Troubleshooting](#7-troubleshooting)
- [8. Developer guide](#8-developer-guide)
- [Third-party licences](#third-party-licences)

## 1. Requirements

| Tool | Version |
|---|---|
| C++ compiler | C++20: GCC 11+, Clang 14+ or MSVC 2022 |
| CMake | 3.29 or newer |
| Git | to fetch GoogleTest |
| Network access | on the first configure (or a pre-filled `build/_deps`, see [Offline builds](#offline-builds)) |

System packages on Debian/Ubuntu:

```bash
sudo apt install build-essential cmake git ninja-build \
                 libgtk-3-dev libgl1-mesa-dev libx11-dev zlib1g-dev
```

- `libgtk-3-dev`, `libgl1-mesa-dev`, `libx11-dev` and `zlib1g-dev` are needed only for BowlingGL
  (`BUILD_WX_UI=ON`).
- Qt 6 is needed only with `BUILD_QT_UI=ON`.

wxWidgets 3.2.8, Bullet3 3.25, GLM 1.0.1 and GoogleTest 1.15.2 are downloaded and built during
configure. You do not install them yourself.

## 2. Building

### Standard build

```bash
cmake -S . -B build -G Ninja
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The first configure and build take several minutes, because wxWidgets and Bullet are compiled from
source. Later builds are incremental.

Executables are written to `build/bin/`:

| Binary | What it is |
|---|---|
| `BowlingGL` | 3D bowling game (wxWidgets + OpenGL) |
| `BowlingConsole` | Text bowling score keeper |
| `BowlingDisplay` | Qt score display (only with `BUILD_QT_UI=ON`) |
| `test_*` | GoogleTest suites |

### Build options

Pass options as `-D<OPTION>=ON|OFF` on the first `cmake` call.

| Option | Default | Effect |
|---|---|---|
| `BUILD_APPS` | `ON` | Build the applications |
| `BUILD_WX_UI` | `ON` | Fetch wxWidgets and build `BowlingGL` |
| `BUILD_QT_UI` | `OFF` | Build `BowlingDisplay`; needs Qt 6 |
| `BUILD_TESTING` | `ON` | Fetch GoogleTest and build the test executables |
| `BUILD_SHARED_LIBS` | `ON` | Gamebox libraries as shared (`.so` / `.dll`) instead of static |
| `GENERATE_SCAFFOLD` | `ON` | Generate missing libraries listed in `GAMEBOX_LIBRARIES` from templates |
| `ENABLE_COVERAGE` | `OFF` | Add `--coverage` and a `coverage` target (GCC/Clang) |
| `ENABLE_CLANG_TIDY` | `OFF` | Run `clang-tidy` during compilation |
| `BUILD_VERBOSE_OUTPUT` | `OFF` | Verbose makefiles and CMake variable dumps |
| `SB_FETCH_OFFLINE` | `OFF` | Use only already-downloaded dependencies |

The dependency versions can be overridden with `GAMEBOX_BULLET_VERSION`, `GAMEBOX_GLM_VERSION`,
`GAMEBOX_WX_VERSION` and `GAMEBOX_GTEST_VERSION`.

### Common variants

```bash
# Console game and libraries only (no wxWidgets download, no GTK needed)
cmake -S . -B build -DBUILD_WX_UI=OFF

# Release build
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release

# With the Qt display (point CMake to your Qt if it is not found)
cmake -S . -B build -DBUILD_QT_UI=ON -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64
```

### Offline builds

After one successful online configure, `build/_deps` contains every downloaded dependency. Then:

```bash
cmake -S . -B build -DSB_FETCH_OFFLINE=ON
```

You can also share a download cache between build directories with
`-DFETCHCONTENT_BASE_DIR=/path/to/cache`, as CI does.

### Installing the libraries

```bash
cmake --install build --prefix ~/.local
```

This installs the Gamebox libraries, headers and `lib/cmake/Gamebox/GameboxConfig.cmake`, so other
projects can use `find_package(Gamebox)` and link `Gamebox::Score`, `Gamebox::Alley` and so on.

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
| Show controls | | Help ▸ Controls |
| Quit | Esc or Ctrl+Q | Game ▸ Quit |

You can move and aim while charging. Inputs that make no sense in the current phase (for example
aiming while the ball rolls) are ignored.

### Tips

- A centred throw at about 80 % power from an offset of about 0.08 m usually hits the pocket.
- Holding → or ← moves the ball over the gutter. That throw goes straight into the gutter and is
  scored `-`.

## 4. Playing BowlingConsole

```bash
./build/bin/BowlingConsole
```

BowlingConsole is a score keeper for real games: type the number of pins each ball knocked down.

| Input | When | Effect |
|---|---|---|
| `p` | menu | start a game |
| `0` … `n` | in a game | record a roll; `n` is the number of pins still standing |
| `r` | in a game | restart the game |
| `q` | anywhere | quit |

Anything else is rejected with "Invalid choice!". After each roll the score grid is redrawn with
the current total. When the tenth frame is complete, the final score is shown.

## 5. BowlingDisplay (Qt)

Build it with `-DBUILD_QT_UI=ON` and run `./build/bin/BowlingDisplay`. It is a form-based score
display backed by the same `Bowlingbox` calculator. It is kept for comparison with the other two
front-ends.

## 6. Scoring rules

Gamebox scores standard ten-pin bowling:

- A game has 10 frames. In each frame you get up to two balls to knock down 10 pins.
- **Strike** (`X`): all 10 pins with the first ball. The frame scores 10 plus the next two balls.
- **Spare** (`/`): all 10 pins with two balls. The frame scores 10 plus the next ball.
- **Open frame**: the frame scores the number of pins knocked down.
- **Miss** (`-`): a ball that knocks down no pins.
- **Tenth frame**: a strike or spare earns bonus balls, up to three balls in the frame.
- A perfect game is 12 strikes = 300.

On the scoreboard, the top row shows each ball and the bottom row shows the running total. A
frame's total appears once its bonus balls have been played.

## 7. Troubleshooting

| Symptom | Cause and fix |
|---|---|
| `Package 'gtk+-3.0' not found` / OpenGL not found | Install `libgtk-3-dev libgl1-mesa-dev`, or build without BowlingGL (`-DBUILD_WX_UI=OFF`) |
| `DSO missing from command line` (X11) | Install `libx11-dev` and configure again |
| Configure hangs or fails while downloading | Check network or proxy access to github.com. Once `build/_deps` is filled, use `-DSB_FETCH_OFFLINE=ON` |
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
./build/bin/test_gamebox_score --gtest_filter='*strike*'
```

After changes to Alley, Render or BowlingGL, also go through the manual checklist in
[applications/bowlingGame/README.md](../applications/bowlingGame/README.md).

### Coverage

```bash
cmake -S . -B build-cov -DENABLE_COVERAGE=ON
cmake --build build-cov -j
cmake --build build-cov --target coverage   # needs lcov + genhtml
xdg-open build-cov/coverage/html/index.html
```

### Static analysis

```bash
cmake -S . -B build-tidy -DENABLE_CLANG_TIDY=ON
cmake --build build-tidy
```

`compile_commands.json` is always generated in the build directory for editors and clangd.

### Adding a library

1. Add its name to `GAMEBOX_LIBRARIES` in the root `CMakeLists.txt`, and to
   `_gamebox_library_dirs` in `libraries/CMakeLists.txt`.
2. Configure with `GENERATE_SCAFFOLD=ON`. This creates `libraries/<Name>/` from
   `cmake/template/lib`: a public header under `include/Gamebox/<Name>/`, a pimpl source, a test,
   and a `CMakeLists.txt` that calls `create_library`.
3. Add dependencies to `PUBLIC_DEPS` (types that appear in your public headers) or `PRIVATE_DEPS`
   (implementation only). Wrap third-party targets in `$<BUILD_INTERFACE:…>` if they should not
   be part of the installed package.
4. Put tests in `test/src/test_<Name>.cpp`. They build as `test_gamebox_<name>` and get the `unit`
   label.

### Conventions

- C++20, no compiler extensions, warnings as errors.
- Public API in `include/Gamebox/<Lib>/`, namespaced `Gamebox::<Lib>`, and exported with
  `GAMEBOX_<LIB>_API`. Everything else stays in `src/`.
- Depend on interfaces (`IBowlingGame`, `IPhysicsWorld`, `ICanvas2D`, `ITerminal`). Choose the
  concrete class in `main` or a default constructor, and inject fakes in tests.
- Commands that can be refused return `Result<>` and do not throw.
- Test names must start with `test_` and be lowercase.

### Continuous integration

Every push to `main` or `feature/**`, and every pull request to `main`, builds and tests on Ubuntu
24.04 (`.github/workflows/ci.yml`). Add the `automerge` label to a pull request to squash-merge it
automatically once CI is green.

## Third-party licences

Gamebox itself is MIT licensed (see [LICENSE](../LICENSE)). It uses:

| Component | Licence |
|---|---|
| Bullet3 | zlib |
| wxWidgets | wxWindows Library Licence (LGPL with a static-linking exception) |
| GLM | MIT (or Happy Bunny) |
| GoogleTest | BSD-3-Clause (tests only, not shipped) |
| GLAD 2 | generated code public domain / generator MIT and Apache-2.0 |
| stb_easy_font | public domain / MIT |
| Qt 6 (optional) | LGPL-3.0 / GPL / commercial |

See [3rdparty/README.md](../3rdparty/README.md) for the in-tree components.
