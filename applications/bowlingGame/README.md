# [Bowling Game](https://codingdojo.org/kata/Bowling/) challenge

Three front-ends for the `Sandbox::Gamebox` score calculator.

## BowlingGL (wxWidgets + OpenGL) — play it

Aim, charge and throw; Bullet3 decides which pins fall; Gamebox keeps the score.
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
wxWidgets 3.2.8, Bullet3 3.25, GLM and GoogleTest are downloaded at configure time
(`SB_FETCH_OFFLINE=ON` reuses a populated `build/_deps`).

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build
./build/bin/BowlingGL
```

On Wayland sessions, if the canvas stays black: `GDK_BACKEND=x11 ./build/bin/BowlingGL`.

Manual checklist after changes to Alley/Render/app:
1. Launch: lane, 10 pins, ball and an empty scoreboard are visible.
2. Throw from offset ≈ 0.078 m with ~80 % power: strike.
3. Throw at the far edge (hold `→`): gutter, `-` in the scoreboard.
4. Partial hit, then second ball: the standing pins stay where they were.
5. Resize the window: scoreboard and lane rescale.
6. `N` during a roll restarts; `Esc` exits with nothing on stderr.

## BowlingConsole

![image](https://github.com/medaminben/Sandbox/assets/166343832/ec0dd7c8-fc45-4f40-b6a6-ebd51d441b65)

Text UI; type the number of pins. Built with `BUILD_APPS=ON`.

## BowlingDisplay (Qt, optional)

![bdisplay](https://github.com/medaminben/Sandbox/assets/166343832/bb91928f-58ca-4893-8179-2c4ee2be4679)

Kept for comparison; off by default because of the Qt licence. Build with `-DBUILD_QT_UI=ON` (needs Qt 6).
