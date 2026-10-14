# [Bowling Game](https://codingdojo.org/kata/Bowling/) challenge

Three front-ends for the `Bowlingbox::Score` score calculator.
See also the [manual](../../docs/MANUAL.md) and the [architecture](../../docs/ARCHITECTURE.md).

## BowlingGL (wxWidgets + OpenGL) — play it

Aim, charge and throw; Bullet3 decides which pins fall; Bowlingbox keeps the score.
Design: [spec](../../docs/superpowers/specs/2026-10-13-bowling-gl-game-design.md).

| Action | Keyboard | Mouse |
|---|---|---|
| Move ball left/right | ← / → | move horizontally while aiming |
| Aim angle | A / D | right-button drag |
| Charge power | hold Space | hold left button |
| Throw | release Space | release left button |
| New game | N or Ctrl+N | Game ▸ New Game |
| Quit | Esc or Ctrl+Q | Game ▸ Quit |

Prerequisites (Debian/Ubuntu): `sudo apt install build-essential cmake libgtk-3-dev libgl1-mesa-dev`.
Bullet3 3.25, GLM and GoogleTest are downloaded at configure time, and wxWidgets 3.2.8 too unless
`libwxgtk3.2-dev` is installed (`BOWLINGBOX_FETCH_OFFLINE=ON` reuses a populated `build/_deps`).

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build
./build/bin/BowlingGL
```

On Wayland, the app picks `GDK_BACKEND=x11` automatically (GLX via XWayland). Override with
`GDK_BACKEND=wayland ./build/bin/BowlingGL` only if you know your stack supports it.

Manual checklist after changes to Alley/Render/app:
1. Launch: lane, 10 pins, ball and an empty scoreboard are visible.
2. Throw from offset ≈ 0.078 m with ~80 % power: strike.
3. Throw at the far edge (hold `→`): gutter, `-` in the scoreboard.
4. Partial hit, then second ball: the standing pins stay where they were.
5. Resize the window: scoreboard and lane rescale.
6. `N` during a roll restarts; `Esc` exits with nothing on stderr.

## Architecture

Front-ends depend on interfaces; the concrete classes are picked in `main` (or a default constructor).

| Interface | Library | Default implementation | Used by |
|---|---|---|---|
| `Score::IScoreSheet` (read) / `Score::IBowlingGame` (+ `roll`, `reset`) | Score | `Score::ScoreCalculator` | all three apps, `Alley::Lane` |
| `Alley::IPhysicsWorld` | Alley | Bullet3 (`makeBulletWorld()`) | `Alley::Lane` |
| `Render::ICanvas2D` | Render | `Render::Overlay` (OpenGL) | `paintScoreboard`, BowlingGL `drawHud` |
| `Console::ITerminal` | BowlingConsole | `SystemTerminal` (stdin/stdout) | `Console::Screen` |

`Score/Notation.h` owns the 21-slot roll encoding (`Mark::Strike`, `Spare`, ...) and its
"X / -" notation, and `Score/Frames.h` cuts the slots into ten `FrameView`s. Render, the console
and Qt all draw from `Score::frames()`. Inject fakes to test without Bullet, OpenGL or a terminal:
see `test_alley_injected`, `test_render_scoreboard` and `test_hud`.

Each app keeps everything except its entry point in a static library (`bowlingconsole_core`,
`bowlingdisplay_core`, `bowlinggl_core`) that the app and its tests link.

BowlingGL: `BowlingCanvas` only owns GL resources, the timer and wx events. `FixedStep` turns timer
ticks into 1/60 s physics steps. Keyboard and mouse input both go `InputMapper` →
`Command {type, amount}` → `LaneCommands::execute`. The camera is `FollowCamera`, the HUD is
`drawHud` + `HudText`, and `hudLayout()` sizes its bands.

BowlingConsole: `Console::Session` holds the game flow (menu, playing, game over) with no I/O;
`Console::Screen` draws it on an `ITerminal` with `banner()` and `renderScoreSheet()`.

## BowlingConsole

![image](https://github.com/medaminben/Sandbox/assets/166343832/ec0dd7c8-fc45-4f40-b6a6-ebd51d441b65)

Text UI; type the number of pins. Built with `BUILD_APPS=ON`.

## BowlingDisplay (Qt, optional)

![bdisplay](https://github.com/medaminben/Sandbox/assets/166343832/bb91928f-58ca-4893-8179-2c4ee2be4679)

Kept for comparison; off by default because of the Qt licence. Build with `-DBUILD_QT_UI=ON` (needs Qt 6).
`test_bowlingdisplay` opens the form on Qt's offscreen platform, so it needs no display.
