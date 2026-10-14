# Applications Refactoring — Review Findings

Date: 2026-10-14. Scope: `applications/bowlingGame/` (BowlingConsole, BowlingDisplay, BowlingGL) and the
CMake helpers they use. Baseline: `test_bowlingconsole` 3/3 and `test_bowlinggl` 14/14 pass on `main` (9bbbf6b).

## Goals

1. The three front-ends build from short, direct CMake files; build switches mean what they say.
2. The 21-slot roll encoding is walked in one place (Score), not once per front-end.
3. Each front-end separates logic from I/O so the logic is unit-tested without a terminal, window or GL.
4. No visible behaviour changes except the defects listed below.

## Findings

### Cross-cutting

- C1. `set(BUILD_BowlingDisplay ${BUILD_QT_UI} AND ${BUILD_APPS})` builds the list `OFF;AND;OFF`, which
  `if()` treats as true. Only the second check in `create_application` keeps Qt off.
- C2. `add_project(... EXEC_LIST)` follows `set(EXEC_LIST)`, so the duplicate-name check never fires.
  CMake already refuses duplicate target names.
- C3. `create_application` silently does nothing for an unknown `ENTRY` or a disabled toolkit.
- C4. Each app CMakeLists is ~65 lines of `PARENT_SCOPE` variables consumed by a loop in
  `applications/bowlingGame/CMakeLists.txt`. The scaffold templates in `cmake/template/app/` copy the same pattern.
- C5. Test targets re-list the app's source files. An app-local static library shared by the
  executable and the test removes the duplication.
- C6. The Qt app passes `MANUAL_FINALIZATION` but never calls `qt_finalize_target`.
- C7. The loop over 10 frames / 21 roll slots (frame 10 has three) is written in the console
  (`scoreGrid.cpp`), Qt (`display.cpp`) and Render (`ScoreboardPainter.cpp`).

### BowlingConsole (`bowlingGameCMD`)

- K1. `plotMenu()` changes the game status while drawing; `setStatus()` both changes state and writes messages.
- K2. Seven `Status` values (incl. the typo `FristStart`) where three modes are needed; `Typo` is a message, not a state.
- K3. A number typed after game over is swallowed: the screen shows `"starting "` instead of "Invalid choice!".
- K4. Input is validated against `pinsLeft()` by hand and the `bool` from `IBowlingGame::roll` is ignored.
- K5. `Frame` keeps both cell strings and the 5-line art, with public mutators; `ScoreGrid` keeps state that
  `update()` overwrites anyway.
- K6. `ScoreGrid::render` has opaque width arithmetic, copies frames in the inner loop and has dead code.
- K7. `screenTools`: `toConsole` and `isNumber` unused; `valueToConsole` range check `value / 1000 > 1`
  is wrong; unused locals; `const int&` / `const bool&` parameters.
- K8. Input with surrounding blanks or a trailing `\r` (pasted, or piped from Windows) is rejected.

### BowlingDisplay (`bowlingGameUI`)

- Q1. 40 hand-written `append` calls to collect widgets.
- Q2. Restart relies on `connectSlotsByName` (`on_btn_restart_clicked`).
- Q3. `frame_index == 9`, `throwPins()` (it updates button visibility), `i <= kPins` over a list.
- Q4. `ui` is a public raw pointer deleted by hand; class `DisplayF` in global namespace.
- Q5. No tests.

### BowlingGL (`bowlingGameGL`)

- G1. Mouse aiming calls `Lane` directly, bypassing `Command` / `LaneCommands`, because `Command` has no amount.
- G2. `LaneCommands` includes `InputMapper` only for step constants.
- G3. `setenv` in `main.cpp` is POSIX-only; MSVC builds fail.
- G4. The simulation advances a fixed 1/60 s per timer tick, so game speed follows timer jitter.
- G5. HUD band fractions live in `BowlingCanvas::renderFrame`; colours and camera numbers are unnamed;
  `SceneAdapter` loops to a literal 10.
- G6. Control help text exists in the Controls dialog, `HudText::hint` and the README.
- G7. The status bar text is set 60 times a second even when unchanged.
- G8. A mouse move over a zero-width canvas maps to a 1.2 m × dx aim jump.
- G9. `_lastMouse` is not updated during a left-drag.

## Out of scope

Library behaviour (Score rules, Alley physics, Render drawing) and CI workflows.
