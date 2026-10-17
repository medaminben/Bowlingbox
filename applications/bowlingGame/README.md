# [Bowling Game](https://codingdojo.org/kata/Bowling/) challenge

Three front-ends for the `Bowlingbox::Score` score calculator.
See also the [manual](../../docs/MANUAL.md) and the [architecture](../../docs/ARCHITECTURE.md).

## BowlingGL (wxWidgets + OpenGL) — play it

Aim, charge and throw; Bullet3 decides which pins fall; Bowlingbox keeps the score.

| Action | Keyboard | Mouse |
|---|---|---|
| Move ball left/right | ← / → | move horizontally while aiming |
| Aim angle | A / D | right-button drag |
| Charge power | hold Space | hold left button |
| Throw | release Space | release left button |
| New game | N or Ctrl+N | Game ▸ New Game |
| Change scenery | T | View ▸ Theme |
| Mute / unmute sound | M | Sound ▸ Mute |
| Volume up / down | + / − | Sound ▸ Volume up / down |
| Background ambience on / off | B | Sound ▸ Background ambience |
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
7. `--theme=neon`, `--theme=classic`, `--theme=outdoor`: each starts in that scenery; `--theme=space` warns and starts Neon Hall.
8. Press `T` three times: Neon → Classic → Outdoor → Neon, and View ▸ Theme follows; choosing from the menu works too.
9. In every theme: a gutter ball at full offset and flying pins stay visible; the aim guide is translucent and the pin necks are red; the frame rate stays smooth.
10. With sound on, a pocket throw plays: release thud, rolling that rises with speed, pin knocks, then the strike chime (a spare plays a bell, a gutter ball nothing extra); the ambience matches the theme and changes with `T` (with a chime).
11. `M` mutes everything and unmutes; holding `M` toggles once; Sound ▸ Mute stays in sync; muted + `T` stays silent. `+` / `−` step the volume by 10% (and unmute), `B` stops and restarts the ambience; the status bar shows the state. Quit and restart: the settings come back.
12. `--mute` starts silent with Sound ▸ Mute checked. With no audio device (e.g. `env -u PULSE_SERVER -u XDG_RUNTIME_DIR ALSA_CONFIG_PATH=/dev/null ./build/bin/BowlingGL` on a machine without PipeWire/Pulse access) one `sound disabled:` line appears and the game runs.

### Scenery

Three themes surround the lane. Pick one at start or switch while playing (`T` or View ▸ Theme):

| Theme | `--theme=` | Look |
|---|---|---|
| Neon Hall (default) | `neon` | Dark-blue hall, cyan and purple neon, glowing pin screens, two lanes on each side |
| Classic Alley | `classic` | Warm, bright hall with ceiling lights and a score monitor above the pins |
| Outdoor | `outdoor` | Terrace under the sky with fences and trees, one lane on each side |

```bash
./build/bin/BowlingGL --theme=classic
```

An unknown name prints the valid ones and starts Neon Hall. The decor is drawn only; it never
touches the physics.

`./build/bin/BowlingGL --mute` starts with the sound off. Without an audio device the game prints `sound disabled: ...` once and runs silently.

## Architecture

Front-ends depend on interfaces; the concrete classes are picked in `main` (or a default constructor).

| Interface | Library | Default implementation | Used by |
|---|---|---|---|
| `Score::IScoreSheet` (read) / `Score::IBowlingGame` (+ `roll`, `reset`) | Score | `Score::ScoreCalculator` | all three apps, `Alley::Lane` |
| `Alley::IPhysicsWorld` | Alley | Bullet3 (`makeBulletWorld()`) | `Alley::Lane` |
| `Render::ICanvas2D` | Render | `Render::Overlay` (OpenGL) | `paintScoreboard`, BowlingGL `drawHud` |
| `Audio::IAudioOutput` | Audio | miniaudio (`createDeviceOutput()`), `makeSilentOutput()` without a device | BowlingGL `SoundDirector` |
| `Console::ITerminal` | BowlingConsole | `SystemTerminal` (stdin/stdout) | `Console::Screen` |

`Score/Notation.h` owns the 21-slot roll encoding (`Mark::Strike`, `Spare`, ...) and its
"X / -" notation, and `Score/Frames.h` cuts the slots into ten `FrameView`s. Render, the console
and BowlingDisplay all draw from `Score::frames()`. Inject fakes to test without Bullet, OpenGL or a terminal:
see `test_alley_injected`, `test_render_scoreboard` and `test_hud`.

Each app keeps everything except its entry point in a static library (`bowlingconsole_core`,
`bowlingdisplay_core`, `bowlinggl_core`) that the app and its tests link.

BowlingGL: `BowlingCanvas` only owns GL resources, the timer and wx events. `FixedStep` turns timer
ticks into 1/60 s physics steps. Keyboard and mouse input both go `InputMapper` →
`Command {type, amount}` → `LaneCommands::execute`. The camera is `FollowCamera`, the HUD is
`drawHud` + `HudText`, and `hudLayout()` sizes its bands.
`Render::buildProps()` lays out each theme's decor as plain data (`Environment.h`), so the
corridor and camera clearances are tested without GL; `SceneRenderer` draws it with fog and glow.

BowlingConsole: `Console::Session` holds the game flow (menu, playing, game over) with no I/O;
`Console::Screen` draws it on an `ITerminal` with `banner()` and `renderScoreSheet()`.

## BowlingConsole

![image](https://github.com/medaminben/Sandbox/assets/166343832/ec0dd7c8-fc45-4f40-b6a6-ebd51d441b65)

Text UI; type the number of pins. Built with `BUILD_APPS=ON`.

## BowlingDisplay (wxWidgets)

![bdisplay](https://github.com/medaminben/Sandbox/assets/166343832/bb91928f-58ca-4893-8179-2c4ee2be4679)

Built with `BUILD_WX_UI=ON`, next to BowlingGL. It uses `wx::core` and `wx::base`, not OpenGL.
`test_bowlingdisplay` opens the window, so it needs a display or `xvfb-run`.
