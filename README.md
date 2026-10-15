# Bowlingbox

[![CI](https://github.com/medaminben/Bowlingbox/actions/workflows/ci.yml/badge.svg)](https://github.com/medaminben/Bowlingbox/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

A C++20 bowling game built from small, testable libraries: a ten-pin score calculator,
a Bullet3 physics lane, an OpenGL renderer, and three front-ends (wxWidgets/OpenGL,
console, and a wxWidgets score display).

![BowlingGL](docs/Bowlingbox_%20Neon%20Strike.png)

## How it started

Bowlingbox began as the [Bowling Game kata](https://codingdojo.org/kata/Bowling/): given the
pins knocked down by each roll, compute the score of a ten-pin game. The answer to that kata is
the [`Score`](libraries/Score) library, and the rest of the project is built around it. Physics,
rendering and the three front-ends all feed rolls into the same `Bowlingbox::Score` calculator
and display what it returns. The [bowling apps README](applications/bowlingGame/README.md)
describes the front-ends.

## Quick start

Debian/Ubuntu:

```bash
sudo apt install build-essential cmake git libgtk-3-dev libgl1-mesa-dev libx11-dev zlib1g-dev
cmake -S . -B build
cmake --build build -j
ctest --test-dir build
./build/bin/BowlingGL        # 3D game
./build/bin/BowlingConsole   # text game
```

CMake 3.29 or newer is required. Bullet3, GLM and GoogleTest are downloaded during the first
configure. An installed wxWidgets 3.2 (`sudo apt install libwxgtk3.2-dev`) is used if present;
otherwise wxWidgets is downloaded and built too. The [manual](docs/MANUAL.md) covers controls,
every build option, and troubleshooting.

On Windows, install Bowling Box with `BowlingBox_Setup_<version>.exe` from the
[latest release](https://github.com/medaminben/Bowlingbox/releases/latest);
[docs/windows-installer.md](docs/windows-installer.md) explains how it is built.

## Repository layout

| Path | Contents |
|---|---|
| [`libraries/Score`](libraries/Score) | `Bowlingbox::Score`: `ScoreCalculator` score calculator, `IBowlingGame` interface, X / - notation |
| [`libraries/Core`](libraries/Core) | `Bowlingbox::Core`: `Result<T, E>` / `Error<E>` types |
| [`libraries/Alley`](libraries/Alley) | `Bowlingbox::Alley`: the throw state machine (`Lane`) on top of a swappable physics world (Bullet3) |
| [`libraries/Render`](libraries/Render) | `Bowlingbox::Render`: OpenGL 3.3 scene renderer, 2D overlay, scoreboard painter |
| [`applications/bowlingGame`](applications/bowlingGame) | `BowlingGL` (wxWidgets + OpenGL), `BowlingConsole`, `BowlingDisplay` (wxWidgets) |
| [`cmake`](cmake) | Build helpers, dependency fetching, scaffold templates, install/export config |
| [`3rdparty`](3rdparty) | In-tree GLAD loader and `stb_easy_font` |
| [`licenses`](licenses) | Third-party licence list and texts |
| [`docs`](docs) | Architecture, manual, design specs and plans |

## Main build options

| Option | Default | Effect |
|---|---|---|
| `BUILD_APPS` | `ON` | Build the applications |
| `BUILD_WX_UI` | `ON` | Build `BowlingGL` and `BowlingDisplay` (system wxWidgets, or fetched and built) |
| `BUILD_TESTING` | `ON` | Build the GoogleTest suites |
| `BUILD_SHARED_LIBS` | `ON` | Build the Bowlingbox libraries as shared libraries |

The [manual](docs/MANUAL.md#build-options) lists the full set.

## Documentation

- [Manual](docs/MANUAL.md): building, playing, troubleshooting, developer workflow
- [Architecture](docs/ARCHITECTURE.md): libraries, interfaces, data flow, build system
- [Bowling apps](applications/bowlingGame/README.md): per-app notes and the manual test checklist

## Contributing

Bug reports and pull requests are welcome. [CONTRIBUTING.md](CONTRIBUTING.md) explains how to build,
test and submit a change. Everyone follows the [Code of Conduct](CODE_OF_CONDUCT.md). Report security
problems privately as described in [SECURITY.md](SECURITY.md).

## License

Bowlingbox is released under the [MIT License](LICENSE). Third-party components keep their own
licences; [licenses/](licenses/README.md) lists them with their full licence texts.
