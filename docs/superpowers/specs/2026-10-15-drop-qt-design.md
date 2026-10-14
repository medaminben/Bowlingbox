# Drop Qt — Design

Date: 2026-10-15. Branch: `refactor/drop-qt`, based on `refactor/apps` (PR #3).

## Goal

Bowlingbox has no Qt dependency at all: no option, no CMake module, no code, no template. BowlingDisplay, the
only Qt app, is rebuilt with wxWidgets and keeps its look and behaviour.

## Decisions

- BowlingDisplay stays a separate executable, built with `BUILD_WX_UI=ON` next to BowlingGL. It needs
  `wx::core` and `wx::base`, not OpenGL.
- The window is laid out in C++ with sizers, like `BowlingFrame`. No XRC (`wxUSE_XRC` is off in the
  source build of wx) and no form designer.
- The window is tested as a real wx window. CI runs the tests under `xvfb-run`.

## BowlingDisplay (wx)

Files in `applications/bowlingGame/bowlingGameUI/`:

| File | Content |
|------|---------|
| `ScoreSheetFrame.h/.cpp` | `BowlingDisplay::MainWindow : wxFrame` (replaces `display.h/.cpp/.ui`) |
| `main.cpp` | `wxApp` that shows a `MainWindow` |
| `test/test_Display.cpp` | the five existing tests, ported to wx |

Layout (fixed size 1200×200, title `https://github.com/medaminben`):

- Top row: static box "Pins Down" (`pinsGroup`) with buttons `btn_0` … `btn_10` labelled `0` … `10`,
  then the restart button `btn_restart` labelled `⟲`.
- Bottom row: ten untitled boxes `frame1` … `frame10`. Each has the roll cells `lblRoll1Frame{f}`,
  `lblRoll2Frame{f}` (frame 10 also `lblRoll3Frame10`) above the frame score `lblScoreFrame{f}`. Then the
  box `GameScore` with `lblScore` in a 20 pt bold font. Cells are centred.

Every widget is created with its name set, so `wxWindow::FindWindowByName(name, window)` finds it.
`pinsGroup` is a `wxStaticBox` (via `wxStaticBoxSizer`); the buttons are its children, so disabling it
disables them.

Behaviour (unchanged from the Qt app):

- `MainWindow(wxWindow* parent = nullptr, std::unique_ptr<IBowlingGame> game = nullptr)`; the game
  defaults to `ScoreCalculator`.
- Pin button `n` calls `roll(n)`; `btn_restart` calls `restart()` (`reset()` exactly once); both end in
  `refresh()`.
- `updateGrid()` fills the cells from `Score::frames()` and `notation()`, and `lblScore` from `score()`.
- `updatePinButtons()`: `pinsGroup` is disabled when the game is over; button `n` is shown when the game
  is over or `n <= pinsLeft()`. After showing or hiding buttons the pins sizer is laid out again.

## Build

```cmake
add_library(bowlingdisplay_core STATIC ScoreSheetFrame.h ScoreSheetFrame.cpp)
target_link_libraries(bowlingdisplay_core PUBLIC ${CMAKE_ROOT_NAME}::Score wx::core wx::base)
create_application(NAME BowlingDisplay ENTRY WX_ui SOURCES main.cpp DEPENDENCIES bowlingdisplay_core)
```

`applications/bowlingGame/CMakeLists.txt` adds `bowlingGameUI` inside `if(BUILD_WX_UI)`.

## Tests

`test_bowlingdisplay` keeps the five tests: pin buttons fill the sheet, only standing pin counts are
offered, the tenth frame shows three rolls, game over disables the pins, restart resets once and clears
the sheet.

- A click sends `wxEVT_BUTTON` with the button's id and the button as event object to the button
  (`ProcessWindowEvent`), which is what a real click delivers.
- Hidden means `!IsShown()`, disabled means `!IsEnabled()`.
- `main()` calls `InitGoogleTest` first. With `--gtest_list_tests` (test discovery at build time) it
  runs gtest without starting wx, so the build needs no display. Otherwise it calls `wxEntryStart`; if
  that fails it prints `test_bowlingdisplay needs a display; run it under xvfb-run` and returns 1. It
  calls `wxEntryCleanup` after the tests.
- Each test destroys its window before it ends.
- Labels: `unit;gui`. `build_gtest_executable` gets an optional `LABELS` argument (default `unit`).

## CI

- `apt-get install` adds `xvfb`.
- Test step: `xvfb-run -a ctest --output-on-failure --label-regex unit`.
- Configure drops `-DBUILD_QT_UI=OFF`.

## Qt removal

| Where | Change |
|-------|--------|
| `CMakeLists.txt` | remove `option(BUILD_QT_UI …)` |
| `cmake/Common.cmake` | remove `include(QtSupport)` block |
| `cmake/QtSupport.cmake` | delete |
| `cmake/BuildUtils.cmake` | `create_application`: ENTRY is `Console` or `WX_ui`; remove the `QT_ui` check and the `qt_add_executable` branch |
| `cmake/modules/LibraryGenerator.cmake` | remove the Qt app branch |
| `cmake/template/app/` | delete `lib_qt_ui_CMakeLists.txt.in`, `lib_Qt_UI.cpp.in`, `lib_mainwindow.cpp.in`, `lib_mainwindow.h.in`, `lib_mainwindow.ui.in` |
| `bowlingGameUI/display.{h,cpp,ui}` | delete (replaced by `ScoreSheetFrame`) |
| `README.md`, `applications/bowlingGame/README.md`, `docs/ARCHITECTURE.md`, `docs/MANUAL.md` | BowlingDisplay is wx and built with `BUILD_WX_UI`; drop `BUILD_QT_UI`, Qt 6 and its licence row; note that `test_bowlingdisplay` needs a display or `xvfb-run` |

An old cache that still sets `BUILD_QT_UI` only gets CMake's unused-variable warning; no shim.

## Done when

- `git grep -i -E '\bqt|QT_'` outside `docs/superpowers/` finds nothing.
- A fresh configure with `BUILD_WX_UI=ON` logs no Qt and builds `BowlingDisplay`; with `BUILD_WX_UI=OFF`
  neither wx app is created.
- Full build: no warnings, every test passes (110, including `test_bowlingdisplay` 5), both on a real
  display and under `xvfb-run`.
- `BowlingDisplay` started by hand looks like the Qt form and plays a full game.

## Out of scope

Score, Alley and Render libraries; BowlingGL and BowlingConsole; the wx scaffold template.
