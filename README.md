# Gamebox

[![CI](https://github.com/medaminben/Gamebox/actions/workflows/ci.yml/badge.svg)](https://github.com/medaminben/Gamebox/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

A C++20 bowling game built from small, testable libraries: a ten-pin score calculator,
a Bullet3 physics lane, an OpenGL renderer, and three front-ends (wxWidgets/OpenGL,
console, optional Qt).

![BowlingGL](docs/Bowlingbox_%20Neon-Kegeln%20im%20Spielmodus.png)

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

CMake 3.29 or newer is required. wxWidgets, Bullet3, GLM and GoogleTest are downloaded during
the first configure. The [manual](docs/MANUAL.md) covers controls, every build option, and
troubleshooting.

## Repository layout

| Path | Contents |
|---|---|
| [`libraries/Score`](libraries/Score) | `Gamebox::Score`: score calculator (`Bowlingbox`), `IBowlingGame` interface, X / - notation |
| [`libraries/Core`](libraries/Core) | `Gamebox::Core`: `Result<T, E>` / `Error<E>` types |
| [`libraries/Alley`](libraries/Alley) | `Gamebox::Alley`: the throw state machine (`Lane`) on top of a swappable physics world (Bullet3) |
| [`libraries/Render`](libraries/Render) | `Gamebox::Render`: OpenGL 3.3 scene renderer, 2D overlay, scoreboard painter |
| [`applications/bowlingGame`](applications/bowlingGame) | `BowlingGL` (wxWidgets + OpenGL), `BowlingConsole`, `BowlingDisplay` (Qt) |
| [`cmake`](cmake) | Build helpers, dependency fetching, scaffold templates, install/export config |
| [`3rdparty`](3rdparty) | In-tree GLAD loader and `stb_easy_font` |
| [`docs`](docs) | Architecture, manual, design specs and plans |

## Main build options

| Option | Default | Effect |
|---|---|---|
| `BUILD_APPS` | `ON` | Build the applications |
| `BUILD_WX_UI` | `ON` | Build `BowlingGL` (fetches and builds wxWidgets) |
| `BUILD_QT_UI` | `OFF` | Build `BowlingDisplay` (needs Qt 6 installed) |
| `BUILD_TESTING` | `ON` | Build the GoogleTest suites |
| `BUILD_SHARED_LIBS` | `ON` | Build the Gamebox libraries as shared libraries |

The [manual](docs/MANUAL.md#build-options) lists the full set.

## Documentation

- [Manual](docs/MANUAL.md): building, playing, troubleshooting, developer workflow
- [Architecture](docs/ARCHITECTURE.md): libraries, interfaces, data flow, build system
- [Bowling apps](applications/bowlingGame/README.md): per-app notes and the manual test checklist
- [BowlingGL design spec](docs/superpowers/specs/2026-10-13-bowling-gl-game-design.md)

## License

Gamebox is released under the [MIT License](LICENSE). Third-party components keep their own
licences; see [3rdparty/README.md](3rdparty/README.md) and the [manual](docs/MANUAL.md#third-party-licences).
