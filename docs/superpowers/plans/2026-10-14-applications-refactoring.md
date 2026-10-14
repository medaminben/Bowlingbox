# Applications Refactoring Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Tasks inside one wave are independent and are meant to be dispatched in parallel (superpowers:dispatching-parallel-agents), each in its own git worktree (superpowers:using-git-worktrees).

**Goal:** Refactor the three bowling front-ends (BowlingConsole, BowlingDisplay, BowlingGL) and their CMake setup so that logic is separated from I/O, the roll encoding is walked in one place, and the defects found in the 2026-10-14 review are fixed.

**Architecture:** Wave 1 lays the foundations in parallel: direct per-app CMake files with an app-local static library (T1), and `Score::frames()` (T2), which splits the 21 roll slots into ten frames. Wave 2 refactors the three apps in parallel on top of those (T3 console, T4 GL input, T5 Qt). Wave 3 finishes BowlingGL's loop and HUD (T6) and updates the docs (T7). A final whole-branch review closes the plan.

**Tech Stack:** C++20, CMake ≥ 3.29, GoogleTest 1.15.2, wxWidgets 3.2 (system `libwxgtk3.2-dev`), Qt 6.8 (system `qt6-base-dev`), Bullet3 3.25, GLM.

**Spec:** `docs/superpowers/specs/2026-10-14-applications-refactoring.md` (finding IDs C1–C7, K1–K8, Q1–Q5 and G1–G9 are used below).

## Global Constraints

- C++20, no compiler extensions. Project targets build with `-Wall -Wextra -Wpedantic -Werror` through `bowlingbox_set_warnings(<target>)`, which also applies to the new app libraries. A new warning is a build failure.
- Public library API lives in `libraries/<Lib>/include/Bowlingbox/<Lib>/`, written as nested `namespace Bowlingbox { namespace <Lib> {`. Everything else stays in `src/`.
- The roll encoding does not change: 21 slots, 10 frame totals, `Mark` sentinels Empty −4, None −3, Strike −2, Spare −1, Miss 0.
- Commands that can be refused return `Result<>`, or `bool` for `IBowlingGame::roll`, and do not throw.
- Test executables are created with `build_gtest_executable`, and their names start with `test_` and are lowercase.
- Visible behaviour stays the same except for the fixes this plan names (K3, K8, G4, G8, G9). For the console this means:
  - The score grid and banner text match the golden strings in Task 3 character for character.
  - Rows wrap at today's widths: 10 frames from 151 columns, 5 from 76, 2 from 31, otherwise 1.
  - Success messages stay as the screen shows them today: `You have scored: N` and `Game Over!\t You have scored: N`, each followed by a blank line and with no `Your choice:` echo.
  - The only message changes allowed are "Invalid choice!" for a number in the menu or after game over (K3, which today shows `starting `), and accepting input with surrounding blanks or a trailing `\r` (K8).
- Library code changes only in `libraries/Score` (new `Frames.h`) and `libraries/Render/src/ScoreboardPainter.cpp`.
- Every build folder, worktree and scratch tree lives under `/home/amin/Dokumente/dev/Bowlingbox/build/`, never in `/tmp`, `mktemp` directories or sibling folders.
- Every commit message ends with the trailer line `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. Console input with a trailing `\r` or surrounding blanks (pasted text, or stdin piped from a Windows file): `" 7 "` and `"7\r"` must roll 7. Test: Task 3 `blanks_and_carriage_return_are_ignored`.
2. A number typed after the console game is over must answer "Invalid choice!" instead of being swallowed. Test: Task 3 `perfect_game_ends_and_rejects_more_rolls`.
3. An unknown or tiny terminal width (0, negative, 10 columns) must draw one frame per row without throwing (`std::string` with a negative length throws). Tests: Task 3 `unknown_or_tiny_width_still_renders` and `banner_fits_the_terminal`.
4. A mouse move over a zero-width canvas (minimised window) must not jump the aim. Test: Task 4 `zero_width_canvas_does_not_move_the_ball`.
5. A long stall (window drag, suspend/resume, debugger) must not fast-forward the physics. Test: Task 6 `long_stall_runs_at_most_five_steps_and_drops_the_rest`.

---

## Execution setup (controller)

### Branches and waves

Integration branch: `refactor/apps`, created from `main`. Each task works in its own worktree on branch `refactor/<task-id>` cut from the current tip of `refactor/apps`. After a task passes its review, the controller merges it with `git merge --no-ff refactor/<task-id>` into `refactor/apps`, then rebuilds and runs `ctest` in the main worktree's integration build (see below) before starting the next wave.

| Wave | Tasks (parallel) | Starts after |
|---|---|---|
| 1 | T1 App build structure, T2 `Score::frames()` | — |
| 2 | T3 Console, T4 GL input, T5 Qt | T1 and T2 merged |
| 3 | T6 GL loop and HUD, T7 Docs | T3, T4 and T5 merged |
| Final | Whole-branch review, then a PR to `main` once your human partner approves | T6 and T7 merged |

```bash
cd /home/amin/Dokumente/dev/Bowlingbox
git switch -c refactor/apps main
git add docs/superpowers/specs/2026-10-14-applications-refactoring.md docs/superpowers/plans/2026-10-14-applications-refactoring.md
git commit -m "docs: applications refactoring spec and plan

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### File ownership

No file is owned by more than one task in the same wave. An implementer must not edit files outside their task's list; if a change elsewhere seems necessary, stop and report it.

| Task | Owns |
|---|---|
| T1 | `applications/bowlingGame/CMakeLists.txt`, `applications/bowlingGame/bowlingGameCMD/CMakeLists.txt`, `applications/bowlingGame/bowlingGameUI/CMakeLists.txt`, `applications/bowlingGame/bowlingGameGL/CMakeLists.txt`, `cmake/BuildUtils.cmake`, `cmake/template/app/lib_console_CMakeLists.txt.in`, `cmake/template/app/lib_qt_ui_CMakeLists.txt.in`, `cmake/template/app/lib_wx_ui_CMakeLists.txt.in` |
| T2 | `libraries/Score/CMakeLists.txt`, `libraries/Score/include/Bowlingbox/Score/Score.h`, new `libraries/Score/include/Bowlingbox/Score/Frames.h`, new `libraries/Score/src/Frames.cpp`, `libraries/Score/test/src/test_Score.cpp`, `libraries/Render/src/ScoreboardPainter.cpp` |
| T3 | everything under `applications/bowlingGame/bowlingGameCMD/` |
| T4 | `applications/bowlingGame/bowlingGameGL/`: `Command.h`, `InputMapper.h`, `InputMapper.cpp`, `LaneCommands.h`, `LaneCommands.cpp`, `BowlingCanvas.cpp`, `test/test_InputMapper.cpp`, `test/test_Gameplay.cpp` |
| T5 | everything under `applications/bowlingGame/bowlingGameUI/` |
| T6 | `applications/bowlingGame/bowlingGameGL/`: `CMakeLists.txt`, new `FixedStep.h`, new `FixedStep.cpp`, `Hud.h`, `Hud.cpp`, `HudText.h`, `HudText.cpp`, `FollowCamera.h`, `FollowCamera.cpp`, `SceneAdapter.cpp`, `BowlingCanvas.h`, `BowlingCanvas.cpp`, `BowlingFrame.cpp`, `main.cpp`, `test/test_Gameplay.cpp` |
| T7 | `applications/bowlingGame/README.md`, `docs/ARCHITECTURE.md`, `docs/MANUAL.md` |

### Worktree and build setup (every task)

The controller creates each worktree under the repository's git-ignored `build/` folder:

```bash
cd /home/amin/Dokumente/dev/Bowlingbox
git worktree add build/worktrees/<task-id> -b refactor/<task-id> refactor/apps
cd build/worktrees/<task-id>
```

Reuse the already downloaded dependency sources read-only, and keep the build directory inside the worktree (`build/worktrees/<task-id>/build/`):

```bash
export FETCHCONTENT_FULLY_DISCONNECTED=ON
D=/home/amin/Dokumente/dev/Bowlingbox/build/_deps
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DGENERATE_SCAFFOLD=OFF \
      -DBUILD_QT_UI=<QT> -DBUILD_WX_UI=<WX> \
      -DFETCHCONTENT_SOURCE_DIR_BULLET3=$D/bullet3-src \
      -DFETCHCONTENT_SOURCE_DIR_GLM=$D/glm-src \
      -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=$D/googletest-src
```

| Task | `<QT>` | `<WX>` |
|---|---|---|
| T1 | ON | ON |
| T2 | OFF | OFF |
| T3 | OFF | OFF |
| T4 | OFF | ON |
| T5 | ON | OFF |
| T6 | OFF | ON |
| T7 | no build | no build |

wxWidgets and Qt come from the system packages (`libwxgtk3.2-dev`, `qt6-base-dev`), so no toolkit is built from source. Binaries land in `build/bin/` inside the worktree. Any extra build folder a task needs also goes under the worktree's `build/` (for example `build/t1-noqt`). Never build in the main checkout's `build/` from a task.

Integration build for the controller, in the main worktree after each merge:

```bash
cd /home/amin/Dokumente/dev/Bowlingbox
cmake -S . -B build/integration -DCMAKE_BUILD_TYPE=Debug -DGENERATE_SCAFFOLD=OFF -DBUILD_QT_UI=ON -DBUILD_WX_UI=ON \
      -DFETCHCONTENT_SOURCE_DIR_BULLET3=build/_deps/bullet3-src \
      -DFETCHCONTENT_SOURCE_DIR_GLM=build/_deps/glm-src \
      -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=build/_deps/googletest-src
cmake --build build/integration -j && ctest --test-dir build/integration --output-on-failure
```

---

## Wave 1

### Task 1: Direct app CMake files with an app-local library

Fixes C1–C6. After this task each app directory calls `create_application` itself, and everything except `main` (plus the wx window classes for BowlingGL) is compiled once into a static library that both the executable and its test link.

**Files:**
- Modify: `cmake/BuildUtils.cmake` (delete `check_project_name` and `add_project` at lines 3–23; replace `create_application` at lines 139–207)
- Modify: `applications/bowlingGame/CMakeLists.txt` (full rewrite)
- Modify: `applications/bowlingGame/bowlingGameCMD/CMakeLists.txt` (full rewrite)
- Modify: `applications/bowlingGame/bowlingGameUI/CMakeLists.txt` (full rewrite)
- Modify: `applications/bowlingGame/bowlingGameGL/CMakeLists.txt` (full rewrite)
- Modify: `cmake/template/app/lib_console_CMakeLists.txt.in`, `lib_qt_ui_CMakeLists.txt.in`, `lib_wx_ui_CMakeLists.txt.in` (full rewrites)

**Interfaces:**
- Consumes: nothing new.
- Produces:
  - `create_application(NAME <n> ENTRY Console|QT_ui|WX_ui [HEADERS …] [SOURCES …] [RESOURCES …] [UI <file>] [BUILD_ARGS …] [DEPENDENCIES …])`. It stops with `FATAL_ERROR` for an unknown ENTRY, for `QT_ui` without `BUILD_QT_UI`, and for `WX_ui` without `BUILD_WX_UI`. Callers guard toolkit apps with `if(BUILD_QT_UI)` / `if(BUILD_WX_UI)`.
  - Static library targets `bowlingconsole_core`, `bowlingdisplay_core` and `bowlinggl_core`, each with its app directory as a PUBLIC include directory. Wave 2 tasks add their new source files to these lists.
  - Test targets keep their names: `test_bowlingconsole` and `test_bowlinggl`.

- [ ] **Step 1: Write the failing check for `create_application`**

Create a throwaway CMake project under the worktree's `build/` that calls `create_application` with a bad ENTRY, and one with `QT_ui` while `BUILD_QT_UI` is unset:

```bash
mkdir -p build/t1-entry
cat > build/t1-entry/CMakeLists.txt <<'EOF'
cmake_minimum_required(VERSION 3.29)
project(entry_check LANGUAGES CXX)
list(APPEND CMAKE_MODULE_PATH ${REPO}/cmake ${REPO}/cmake/modules)
include(GNUInstallDirs)
include(BuildUtils)
create_application(NAME bogus ENTRY ${ENTRY} SOURCES main.cpp)
EOF
cmake -S build/t1-entry -B build/t1-entry/out-bogus -DREPO=$PWD -DENTRY=Bogus; echo "exit $?"
cmake -S build/t1-entry -B build/t1-entry/out-qt -DREPO=$PWD -DENTRY=QT_ui; echo "exit $?"
```

- [ ] **Step 2: Run it and confirm the current code accepts both silently**

Expected now: both configure runs end with `-- Generating done` and `exit 0`. This is the bug: nothing is built and nothing is reported.

- [ ] **Step 3: Replace the helpers in `cmake/BuildUtils.cmake`**

Delete `check_project_name` and `add_project` (lines 3–23, from `function(check_project_name)` through the `endmacro()` that closes `add_project`). CMake already refuses two targets with the same name. Keep `get_raw_target_name` and `create_library` unchanged. Replace the whole `create_application` block, including its banner comment, with:

```cmake
# ---------------------------------------------------------------------------
# create_application — executable of kind ENTRY (Console, QT_ui or WX_ui).
# Callers guard QT_ui / WX_ui apps with if(BUILD_QT_UI) / if(BUILD_WX_UI).
# ---------------------------------------------------------------------------
function(create_application)
    # Empty keyword args (e.g. UI with no value) are allowed for app scaffolds.
    if(POLICY CMP0174)
        cmake_policy(SET CMP0174 NEW)
    endif()

    set(options)
    set(single_value_args NAME ENTRY UI)
    set(list_args HEADERS SOURCES RESOURCES BUILD_ARGS DEPENDENCIES)
    cmake_parse_arguments(PARSE_ARGV 0 app
        "${options}" "${single_value_args}" "${list_args}")

    foreach(arg IN LISTS app_UNPARSED_ARGUMENTS)
        message(WARNING "unparsed argument: ${arg}")
    endforeach()

    set(_entries Console QT_ui WX_ui)
    if(NOT app_ENTRY IN_LIST _entries)
        message(FATAL_ERROR "${app_NAME}: ENTRY must be one of ${_entries}, got '${app_ENTRY}'")
    endif()
    if(app_ENTRY STREQUAL "QT_ui" AND NOT BUILD_QT_UI)
        message(FATAL_ERROR "${app_NAME}: ENTRY QT_ui needs BUILD_QT_UI=ON; guard the call with if(BUILD_QT_UI)")
    endif()
    if(app_ENTRY STREQUAL "WX_ui" AND NOT BUILD_WX_UI)
        message(FATAL_ERROR "${app_NAME}: ENTRY WX_ui needs BUILD_WX_UI=ON; guard the call with if(BUILD_WX_UI)")
    endif()

    set(PROJECT_FILES ${app_HEADERS} ${app_SOURCES} ${app_RESOURCES})
    if(DEFINED app_UI AND NOT app_UI STREQUAL "")
        list(APPEND PROJECT_FILES ${app_UI})
    endif()

    if(app_ENTRY STREQUAL "QT_ui")
        qt_add_executable(${app_NAME} ${app_BUILD_ARGS} ${PROJECT_FILES})
    else()
        add_executable(${app_NAME} ${app_BUILD_ARGS} ${PROJECT_FILES})
    endif()
    target_link_libraries(${app_NAME} PRIVATE ${app_DEPENDENCIES})
    if(NOT app_ENTRY STREQUAL "Console")
        set_target_properties(${app_NAME} PROPERTIES
            WIN32_EXECUTABLE TRUE
            MACOSX_BUNDLE TRUE
        )
    endif()
    bowlingbox_set_warnings(${app_NAME})
    install(TARGETS ${app_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endfunction()
```

- [ ] **Step 4: Run the check again and confirm both calls now fail loudly**

Run the two `cmake -S build/t1-entry …` commands from Step 1 again (delete `build/t1-entry/out-*` first).
Expected: `exit 1` for both, with `bogus: ENTRY must be one of Console;QT_ui;WX_ui, got 'Bogus'` and `bogus: ENTRY QT_ui needs BUILD_QT_UI=ON; guard the call with if(BUILD_QT_UI)`.

- [ ] **Step 5: Rewrite `applications/bowlingGame/CMakeLists.txt`**

```cmake
project(BowlingGame LANGUAGES CXX)

# BUILD_APPS is checked by the root CMakeLists.txt before this directory is added.
add_subdirectory(bowlingGameCMD)
if(BUILD_QT_UI)
    add_subdirectory(bowlingGameUI)
endif()
if(BUILD_WX_UI)
    add_subdirectory(bowlingGameGL)
endif()
```

- [ ] **Step 6: Rewrite `applications/bowlingGame/bowlingGameCMD/CMakeLists.txt`**

```cmake
# BowlingConsole: text front-end. Everything except main() lives in
# bowlingconsole_core, so the tests link the same code as the app.
add_library(bowlingconsole_core STATIC
    bowlingScreen.h bowlingScreen.cpp
    scoreGrid.h scoreGrid.cpp
    screenTools.h screenTools.cpp
    terminal.h terminal.cpp
)
target_include_directories(bowlingconsole_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bowlingconsole_core PUBLIC ${CMAKE_ROOT_NAME}::Score)
bowlingbox_set_warnings(bowlingconsole_core)

create_application(
    NAME         BowlingConsole
    ENTRY        Console
    SOURCES      console.cpp
    DEPENDENCIES bowlingconsole_core
)

# Window-free tests: the game loop against a scripted terminal.
if(BUILD_TESTING)
    build_gtest_executable(
        NAME     test_bowlingconsole
        SRC      test/test_Console.cpp
        DEPENDS  bowlingconsole_core GTest::gtest GTest::gtest_main
        DISCOVER ON
    )
endif()
```

- [ ] **Step 7: Rewrite `applications/bowlingGame/bowlingGameUI/CMakeLists.txt`**

`MANUAL_FINALIZATION` is dropped because nothing called `qt_finalize_target` (C6). AUTOUIC and AUTOMOC are switched on globally by `cmake/QtSupport.cmake`.

```cmake
# BowlingDisplay: Qt Widgets score sheet (BUILD_QT_UI=ON). The form and its
# logic live in bowlingdisplay_core, so tests can open it without main().
add_library(bowlingdisplay_core STATIC
    display.h display.cpp display.ui
)
target_include_directories(bowlingdisplay_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bowlingdisplay_core PUBLIC
    ${CMAKE_ROOT_NAME}::Score
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Core
)
bowlingbox_set_warnings(bowlingdisplay_core)

create_application(
    NAME         BowlingDisplay
    ENTRY        QT_ui
    SOURCES      main.cpp
    DEPENDENCIES bowlingdisplay_core
)
```

- [ ] **Step 8: Rewrite `applications/bowlingGame/bowlingGameGL/CMakeLists.txt`**

```cmake
# BowlingGL: wxWidgets + OpenGL game (BUILD_WX_UI=ON). The window-free units
# live in bowlinggl_core; the executable adds the wx window classes and main().
add_library(bowlinggl_core STATIC
    Command.h
    InputMapper.h InputMapper.cpp
    LaneCommands.h LaneCommands.cpp
    FollowCamera.h FollowCamera.cpp
    HudText.h HudText.cpp
    Hud.h Hud.cpp
    SceneAdapter.h SceneAdapter.cpp
)
target_include_directories(bowlinggl_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bowlinggl_core PUBLIC
    ${CMAKE_ROOT_NAME}::Alley
    ${CMAKE_ROOT_NAME}::Render
    wx::core
    wx::base
)
bowlingbox_set_warnings(bowlinggl_core)

create_application(
    NAME         BowlingGL
    ENTRY        WX_ui
    SOURCES      BowlingCanvas.h BowlingCanvas.cpp
                 BowlingFrame.h BowlingFrame.cpp
                 main.cpp
    DEPENDENCIES bowlinggl_core wx::gl OpenGL::GL
)

# Window-free unit tests: input mapping, lane commands, camera rig, HUD.
if(BUILD_TESTING)
    build_gtest_executable(
        NAME     test_bowlinggl
        SRC      test/test_InputMapper.cpp test/test_Gameplay.cpp
        DEPENDS  bowlinggl_core GTest::gtest GTest::gmock GTest::gmock_main
        DISCOVER ON
    )
endif()
```

- [ ] **Step 9: Rewrite the three scaffold templates**

`cmake/template/app/lib_console_CMakeLists.txt.in`:

```cmake
project(${lib_NAME}_Console VERSION 1.1.1 LANGUAGES CXX)

create_application(
    NAME         ${lib_NAME}_Console
    ENTRY        Console
    SOURCES      ${lib_NAME}_Console.cpp
    DEPENDENCIES ${VAR_MARKER_START}CMAKE_ROOT_NAME${VAR_MARKER_END}::${lib_NAME}
)
```

`cmake/template/app/lib_wx_ui_CMakeLists.txt.in`:

```cmake
project(${lib_NAME}_Wx_UI VERSION 1.0.0 LANGUAGES CXX)

if(BUILD_WX_UI)
    create_application(
        NAME         ${lib_NAME}_Wx_UI
        ENTRY        WX_ui
        SOURCES      ${lib_NAME}_Wx_UI.cpp
        DEPENDENCIES ${VAR_MARKER_START}CMAKE_ROOT_NAME${VAR_MARKER_END}::${lib_NAME}
                     wx::core
                     wx::base
    )
endif()
```

`cmake/template/app/lib_qt_ui_CMakeLists.txt.in`:

```cmake
project(${lib_NAME}_Qt_UI VERSION 1.1.1 LANGUAGES CXX)

if(BUILD_QT_UI)
    create_application(
        NAME         ${lib_NAME}_Qt_UI
        ENTRY        QT_ui
        HEADERS      mainwindow.h
        SOURCES      ${lib_NAME}_Qt_UI.cpp mainwindow.cpp
        UI           mainwindow.ui
        DEPENDENCIES ${VAR_MARKER_START}CMAKE_ROOT_NAME${VAR_MARKER_END}::${lib_NAME}
                     Qt${VAR_MARKER_START}QT_VERSION_MAJOR${VAR_MARKER_END}::Widgets
                     Qt${VAR_MARKER_START}QT_VERSION_MAJOR${VAR_MARKER_END}::Gui
                     Qt${VAR_MARKER_START}QT_VERSION_MAJOR${VAR_MARKER_END}::Core
    )
endif()
```

Render the templates the way `cmake/modules/LibraryGenerator.cmake` does, and check the output:

```bash
mkdir -p build/t1-scaffold
cat > build/t1-scaffold/render.cmake <<'EOF'
set(VAR_MARKER_START [[${]])
set(VAR_MARKER_END })
set(lib_NAME Demo)
foreach(kind console qt_ui wx_ui)
    configure_file(${SRC}/cmake/template/app/lib_${kind}_CMakeLists.txt.in ${OUT}/${kind}/CMakeLists.txt)
endforeach()
EOF
cmake -DSRC=$PWD -DOUT=$PWD/build/t1-scaffold -P build/t1-scaffold/render.cmake
grep -l "add_project\|PROJECT_LIST" build/t1-scaffold/*/CMakeLists.txt; echo "stale: $?"
cat build/t1-scaffold/console/CMakeLists.txt
```

Expected: `stale: 1` (no file matches). The console file contains `NAME Demo_Console` and `DEPENDENCIES ${CMAKE_ROOT_NAME}::Demo`.

- [ ] **Step 10: Configure, build and test everything**

```bash
cmake -S . -B build   # with the setup flags, QT=ON WX=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
cmake --build build --target help | grep -E "BowlingConsole|BowlingDisplay|BowlingGL|_core"
```

Expected: the build succeeds and every test passes (console 3, GL 14, plus the library suites). The target list shows `BowlingConsole`, `BowlingDisplay`, `BowlingGL`, `bowlingconsole_core`, `bowlingdisplay_core` and `bowlinggl_core`.

- [ ] **Step 11: Check that the switches mean what they say (C1)**

```bash
cmake -S . -B build/t1-noqt -DBUILD_QT_UI=OFF -DBUILD_WX_UI=OFF   # plus the setup flags
cmake --build build/t1-noqt --target help | grep -cE "BowlingDisplay|BowlingGL"
cmake -S . -B build/t1-noapps -DBUILD_APPS=OFF -DBUILD_QT_UI=ON   # plus the setup flags
cmake --build build/t1-noapps --target help | grep -cE "Bowling(Console|Display|GL)"
```

Expected: `0` both times (configure only, no build needed).

- [ ] **Step 12: Commit**

```bash
git add cmake/BuildUtils.cmake cmake/template/app/*CMakeLists.txt.in \
        applications/bowlingGame/CMakeLists.txt applications/bowlingGame/*/CMakeLists.txt
git commit -m "build: direct app CMake files with app-local core libraries

create_application now rejects unknown or disabled ENTRY kinds; the
PARENT_SCOPE variable loop, add_project and MANUAL_FINALIZATION are gone.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Task 2: `Score::frames()` — one walk over the roll slots

Fixes C7 for Render; Tasks 3 and 5 use it in the console and Qt.

**Files:**
- Create: `libraries/Score/include/Bowlingbox/Score/Frames.h`
- Create: `libraries/Score/src/Frames.cpp`
- Modify: `libraries/Score/CMakeLists.txt` (`TARGET_HEADERS`, `TARGET_SOURCES`)
- Modify: `libraries/Score/include/Bowlingbox/Score/Score.h` (add one include)
- Modify: `libraries/Render/src/ScoreboardPainter.cpp:27-43` (the frame loop)
- Test: `libraries/Score/test/src/test_Score.cpp` (append)

**Interfaces:**
- Consumes: `IScoreSheet::rolls()`, `IScoreSheet::scores()`, `kFrames`, `Mark`, `toInt` (existing).
- Produces, in `<Bowlingbox/Score/Frames.h>`, namespace `Bowlingbox::Score`:
  ```cpp
  struct FrameView {
      int index = 0;                                                       // 0..9
      std::array<int, 3> rolls {toInt(Mark::None), toInt(Mark::None), toInt(Mark::None)};
      int rollCount = 2;                                                   // 3 for the tenth frame
      int score = toInt(Mark::None);                                       // running total or a Mark
      bool isLast() const noexcept { return index == kFrames - 1; }
  };
  std::array<FrameView, kFrames> frames(const std::vector<int>& rolls, const std::vector<int>& scores);
  std::array<FrameView, kFrames> frames(const IScoreSheet& sheet);
  ```
  `rolls[r]` holds slot `2 * index + r`. Slots missing from short vectors read as `Mark::None`. `Score.h` includes `Frames.h`.

- [ ] **Step 1: Write the failing tests**

Append to `libraries/Score/test/src/test_Score.cpp`, and add `#include <Bowlingbox/Score/Frames.h>` and `#include <vector>` to its include block:

```cpp
// ------------------------------------------------------------ frames()
TEST(test_score_frames, empty_sheet_has_ten_blank_frames) {
    ScoreCalculator game;
    const auto views = frames(game);
    for (int f = 0; f < kFrames; ++f) {
        const FrameView& view = views[static_cast<std::size_t>(f)];
        EXPECT_EQ(view.index, f);
        EXPECT_EQ(view.rollCount, f == kFrames - 1 ? 3 : 2);
        EXPECT_EQ(view.isLast(), f == kFrames - 1);
        EXPECT_EQ(view.rolls[0], toInt(Mark::None));
        EXPECT_EQ(view.score, toInt(Mark::None));
    }
}

TEST(test_score_frames, frames_follow_the_21_slot_encoding) {
    ScoreCalculator game;
    for (int pins : {10, 7, 3})
        game.roll(pins);
    const auto views = frames(game);
    EXPECT_EQ(views[0].rolls[0], toInt(Mark::Strike));
    EXPECT_EQ(views[0].rolls[1], toInt(Mark::Empty));
    EXPECT_EQ(views[0].score, 20);
    EXPECT_EQ(views[1].rolls[0], 7);
    EXPECT_EQ(views[1].rolls[1], toInt(Mark::Spare));
    EXPECT_EQ(views[1].score, toInt(Mark::None));
}

TEST(test_score_frames, tenth_frame_has_three_rolls) {
    ScoreCalculator game;
    for (int i = 0; i < 18; ++i)
        game.roll(0);
    for (int pins : {7, 3, 5})
        game.roll(pins);
    const FrameView& last = frames(game)[9];
    EXPECT_EQ(last.rollCount, 3);
    EXPECT_EQ(last.rolls[0], 7);
    EXPECT_EQ(last.rolls[1], toInt(Mark::Spare));
    EXPECT_EQ(last.rolls[2], 5);
    EXPECT_EQ(last.score, 15);
}

TEST(test_score_frames, short_vectors_read_as_not_played) {
    const auto views = frames(std::vector<int> {3}, std::vector<int> {});
    EXPECT_EQ(views[0].rolls[0], 3);
    EXPECT_EQ(views[0].rolls[1], toInt(Mark::None));
    EXPECT_EQ(views[9].rolls[2], toInt(Mark::None));
    EXPECT_EQ(views[0].score, toInt(Mark::None));
}
```

- [ ] **Step 2: Run them and confirm they fail**

Run: `cmake --build build --target test_bowlingbox_score`
Expected: compile error, `Bowlingbox/Score/Frames.h: No such file or directory`.

- [ ] **Step 3: Write `Frames.h`**

`libraries/Score/include/Bowlingbox/Score/Frames.h`:

```cpp
#ifndef BOWLINGBOX_SCORE_FRAMES_H
#define BOWLINGBOX_SCORE_FRAMES_H
#include <Bowlingbox/Score/Bowlingbox_Score_export.h>
#include <Bowlingbox/Score/IBowlingGame.h>
#include <Bowlingbox/Score/Notation.h>
#include <array>
#include <vector>

namespace Bowlingbox {
namespace Score {

/// One frame of the score sheet, cut out of the 21 roll slots.
struct FrameView {
    int index = 0;  // 0..9
    /// Slot values (pins or Mark) of slots 2 * index + 0, 1 and, for the tenth frame, 2.
    std::array<int, 3> rolls {toInt(Mark::None), toInt(Mark::None), toInt(Mark::None)};
    int rollCount = 2;               // 3 for the tenth frame
    int score = toInt(Mark::None);   // running total, or a Mark while not final
    bool isLast() const noexcept { return index == kFrames - 1; }
};

/// Splits the roll slots and frame totals into ten frames. Missing entries read as Mark::None.
BOWLINGBOX_SCORE_API std::array<FrameView, kFrames> frames(const std::vector<int>& rolls,
                                                           const std::vector<int>& scores);
BOWLINGBOX_SCORE_API std::array<FrameView, kFrames> frames(const IScoreSheet& sheet);

}  // namespace Score
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_SCORE_FRAMES_H
```

- [ ] **Step 4: Write `Frames.cpp`**

`libraries/Score/src/Frames.cpp`:

```cpp
#include <Bowlingbox/Score/Frames.h>
#include <cstddef>

namespace Bowlingbox {
namespace Score {

namespace {
int valueAt(const std::vector<int>& values, int i) {
    return i < static_cast<int>(values.size()) ? values[static_cast<std::size_t>(i)] : toInt(Mark::None);
}
}  // namespace

std::array<FrameView, kFrames> frames(const std::vector<int>& rolls, const std::vector<int>& scores) {
    std::array<FrameView, kFrames> out {};
    for (int f = 0; f < kFrames; ++f) {
        FrameView& view = out[static_cast<std::size_t>(f)];
        view.index = f;
        view.rollCount = f == kFrames - 1 ? 3 : 2;
        for (int r = 0; r < view.rollCount; ++r)
            view.rolls[static_cast<std::size_t>(r)] = valueAt(rolls, 2 * f + r);
        view.score = valueAt(scores, f);
    }
    return out;
}

std::array<FrameView, kFrames> frames(const IScoreSheet& sheet) {
    return frames(sheet.rolls(), sheet.scores());
}

}  // namespace Score
}  // namespace Bowlingbox
```

- [ ] **Step 5: Register the files and include the header from `Score.h`**

In `libraries/Score/CMakeLists.txt`, add `include/Bowlingbox/Score/Frames.h` to `TARGET_HEADERS` (after `IBowlingGame.h`) and `src/Frames.cpp` to `TARGET_SOURCES` (first entry). In `libraries/Score/include/Bowlingbox/Score/Score.h`, add `#include <Bowlingbox/Score/Frames.h>` after the `IBowlingGame.h` include.

- [ ] **Step 6: Run the tests and confirm they pass**

Run: `cmake -S . -B build && cmake --build build --target test_bowlingbox_score && ./build/bin/test_bowlingbox_score --gtest_filter='test_score_frames.*'`
Expected: 4 tests PASS. Then run the whole binary: all PASS.

- [ ] **Step 7: Use `frames()` in the Render scoreboard painter**

Run `./build/bin/test_bowlingbox_render` first and note that it passes. In `libraries/Render/src/ScoreboardPainter.cpp`, add `#include <Bowlingbox/Score/Frames.h>` and replace the `for (int f = 0; f < 10; ++f) { … }` loop with:

```cpp
    for (const Score::FrameView& frame : Score::frames(rolls, scores)) {
        const Rect fr = layout.frameRect(frame.index);
        canvas.strokeRect(fr, t, line);
        for (int c = 0; c < frame.rollCount; ++c) {
            const Rect rc = layout.rollCellRect(2 * frame.index + c);
            if (c > 0)
                canvas.drawRect({rc.x, rc.y}, {t, rc.h}, line);
            centred(rc, glyphFor(frame.rolls[static_cast<std::size_t>(c)]));
        }
        const Rect sr = layout.scoreRect(frame.index);
        canvas.drawRect({sr.x, sr.y}, {sr.w, t}, line);
        centred(sr, glyphFor(frame.score, true));
    }
```

Missing slots now read as `Mark::None`, whose glyph is `""`, and `centred` already skips empty strings, so the output is unchanged.

- [ ] **Step 8: Run the Render and Alley tests**

Run: `cmake --build build -j && ./build/bin/test_bowlingbox_render && ./build/bin/test_bowlingbox_alley`
Expected: all PASS, including `test_render_scoreboard.painter_writes_marks_scores_and_total`.

- [ ] **Step 9: Commit**

```bash
git add libraries/Score libraries/Render/src/ScoreboardPainter.cpp
git commit -m "feat(score): frames() splits the roll slots into ten FrameViews

Render's scoreboard painter uses it instead of its own slot arithmetic.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

## Wave 2

### Task 3: Console — `Session` state machine and pure score grid

Fixes K1–K8. `Session` holds the game flow without any I/O. `Screen` only draws and reads lines. The frame boxes are drawn by pure functions built on `Score::frames()`. `screenTools` is replaced by `banner`.

**Files (all under `applications/bowlingGame/bowlingGameCMD/`):**
- Create: `session.h`, `session.cpp`, `banner.h`, `banner.cpp`, `test/test_Session.cpp`, `test/test_ScoreGrid.cpp`
- Rewrite: `scoreGrid.h`, `scoreGrid.cpp`, `bowlingScreen.h`, `bowlingScreen.cpp`
- Delete: `screenTools.h`, `screenTools.cpp`
- Modify: `CMakeLists.txt` (source lists), `test/test_Console.cpp` (add a banner test)
- Unchanged: `terminal.h`, `terminal.cpp`, `console.cpp`

**Interfaces:**
- Consumes: `Bowlingbox::Score::FrameView`, `frames(const IScoreSheet&)` (Task 2); target `bowlingconsole_core` (Task 1).
- Produces (namespace `Console`):
  - `class Session { enum class Mode { Menu, Playing, GameOver }; explicit Session(std::shared_ptr<IBowlingGame>); void handle(std::string_view); Mode mode() const; bool quit() const; const std::string& message() const; std::string prompt() const; const IScoreSheet& sheet() const; }`
  - `std::array<std::string, 5> drawFrame(const Bowlingbox::Score::FrameView&)`
  - `std::string renderScoreSheet(const Bowlingbox::Score::IScoreSheet&, int width)`
  - `std::string banner(int width)`
  - `Screen(std::shared_ptr<IBowlingGame>, ITerminal&)` and `void Screen::start()` (signatures unchanged).

- [ ] **Step 1: Write the failing `Session` tests**

`test/test_Session.cpp`:

```cpp
#include "session.h"
#include <Bowlingbox/Score/Score.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>

using Bowlingbox::Score::Mark;
using Bowlingbox::Score::ScoreCalculator;
using Bowlingbox::Score::toInt;
using Mode = Console::Session::Mode;

class test_session : public ::testing::Test {
public:
    std::shared_ptr<ScoreCalculator> game = std::make_shared<ScoreCalculator>();
    Console::Session session {game};
};

TEST_F(test_session, starts_in_the_menu) {
    EXPECT_EQ(session.mode(), Mode::Menu);
    EXPECT_EQ(session.message(), "Welcome to the Bowling Game!");
    EXPECT_EQ(session.prompt(), "Select a choice:\n q - quit p - play to continue...");
    EXPECT_FALSE(session.quit());
}

TEST_F(test_session, play_then_roll) {
    session.handle("p");
    EXPECT_EQ(session.mode(), Mode::Playing);
    EXPECT_EQ(session.message(), "New game started! Good Luck!");
    session.handle("7");
    EXPECT_EQ(game->rolls()[0], 7);
    EXPECT_EQ(session.message(), "You have scored: 7\n");  // today's text: no echo, trailing blank line
    EXPECT_EQ(session.prompt(),
              "Enter a number between 0 and 3\nor the letter:  q - quit r - restart to continue...");
}

TEST_F(test_session, number_in_the_menu_is_rejected) {
    session.handle("5");
    EXPECT_EQ(session.mode(), Mode::Menu);
    EXPECT_EQ(session.message(), "Your choice: 5\nInvalid choice!");
    EXPECT_EQ(game->rolls()[0], toInt(Mark::None));
}

TEST_F(test_session, blanks_and_carriage_return_are_ignored) {
    session.handle(" p ");
    session.handle("7\r");
    session.handle("\t2 ");
    EXPECT_EQ(game->rolls()[0], 7);
    EXPECT_EQ(game->rolls()[1], 2);
}

TEST_F(test_session, out_of_range_and_garbage_are_rejected) {
    session.handle("p");
    for (const char* bad : {"-1", "11", "abc", "7x", "", "99999999999"}) {
        session.handle(bad);
        EXPECT_EQ(session.message(), "Your choice: " + std::string(bad) + "\nInvalid choice!") << bad;
    }
    session.handle("7");
    session.handle("4");  // only 3 pins standing
    EXPECT_EQ(game->rolls()[0], 7);
    EXPECT_EQ(game->rolls()[1], toInt(Mark::None));
    EXPECT_EQ(session.mode(), Mode::Playing);
}

TEST_F(test_session, perfect_game_ends_and_rejects_more_rolls) {
    session.handle("p");
    for (int i = 0; i < 12; ++i)
        session.handle("10");
    EXPECT_EQ(session.mode(), Mode::GameOver);
    EXPECT_EQ(session.message(), "Game Over!\t You have scored: 300\n");
    EXPECT_EQ(session.prompt(), "Select a choice:\n q - quit p - play to continue...");
    session.handle("5");
    EXPECT_EQ(session.message(), "Your choice: 5\nInvalid choice!");
    EXPECT_EQ(game->score(), 300);
}

TEST_F(test_session, restart_and_quit) {
    session.handle("p");
    session.handle("7");
    session.handle("r");
    EXPECT_EQ(game->rolls()[0], toInt(Mark::None));
    EXPECT_EQ(session.mode(), Mode::Playing);
    session.handle("q\r");
    EXPECT_TRUE(session.quit());
}
```

- [ ] **Step 2: Write the failing score grid tests**

The golden strings below were captured from the current `ScoreGrid` on `main` (9bbbf6b), so they pin today's output.

`test/test_ScoreGrid.cpp`:

```cpp
#include "scoreGrid.h"
#include <Bowlingbox/Score/Score.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

using namespace Bowlingbox::Score;

namespace {
/// X | 7/ | 9- | X | -8 | 8/ | -6 | X | X | X81 = 167
void play(IBowlingGame& game) {
    for (int pins : {10, 7, 3, 9, 0, 10, 0, 8, 8, 2, 0, 6, 10, 10, 10, 8, 1})
        game.roll(pins);
}

std::size_t lineCount(const std::string& text) {
    return static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n'));
}
}  // namespace

TEST(test_score_grid, eighty_columns_give_two_rows_of_five) {
    ScoreCalculator game;
    play(game);
    const std::string expected =
        " .-----------. .-----------. .-----------. .-----------. .-----------.\n"
        " |  X  |     | |  7  |  /  | |  9  |  -  | |  X  |     | |  -  |  8  |\n"
        " |     '-----| |     '-----| |     '-----| |     '-----| |     '-----|\n"
        " |     20    | |     39    | |     48    | |     66    | |     74    |\n"
        " '-----------' '-----------' '-----------' '-----------' '-----------'\n"
        " .-----------. .-----------. .-----------. .-----------. .-----------------.\n"
        " |  8  |  /  | |  -  |  6  | |  X  |     | |  X  |     | |  X  |  8  |  1  |\n"
        " |     '-----| |     '-----| |     '-----| |     '-----| |     '-----'-----|\n"
        " |     84    | |     90    | |    120    | |    148    | |       167       |\n"
        " '-----------' '-----------' '-----------' '-----------' '-----------------'\n";
    EXPECT_EQ(Console::renderScoreSheet(game, 80), expected);
}

TEST(test_score_grid, wide_terminal_gets_one_row) {
    ScoreCalculator game;
    play(game);
    const std::string expected =
        " .-----------. .-----------. .-----------. .-----------. .-----------. .-----------. .-----------. .-----------. .-----------. .-----------------.\n"
        " |  X  |     | |  7  |  /  | |  9  |  -  | |  X  |     | |  -  |  8  | |  8  |  /  | |  -  |  6  | |  X  |     | |  X  |     | |  X  |  8  |  1  |\n"
        " |     '-----| |     '-----| |     '-----| |     '-----| |     '-----| |     '-----| |     '-----| |     '-----| |     '-----| |     '-----'-----|\n"
        " |     20    | |     39    | |     48    | |     66    | |     74    | |     84    | |     90    | |    120    | |    148    | |       167       |\n"
        " '-----------' '-----------' '-----------' '-----------' '-----------' '-----------' '-----------' '-----------' '-----------' '-----------------'\n";
    EXPECT_EQ(Console::renderScoreSheet(game, 200), expected);
}

TEST(test_score_grid, narrow_terminals_wrap_to_two_or_one_frame) {
    ScoreCalculator game;
    play(game);
    EXPECT_EQ(lineCount(Console::renderScoreSheet(game, 40)), 25u);  // 5 rows of 2
    EXPECT_EQ(lineCount(Console::renderScoreSheet(game, 20)), 50u);  // 10 rows of 1
}

TEST(test_score_grid, rows_wrap_at_todays_widths) {
    ScoreCalculator game;
    play(game);
    // 5 lines per row: 10 rows of 1, 5 rows of 2, 2 rows of 5, 1 row of 10.
    const std::pair<int, std::size_t> cases[] = {{30, 50u},  {31, 25u},  {33, 25u}, {75, 25u},
                                                 {76, 10u},  {146, 10u}, {150, 10u}, {151, 5u}};
    for (const auto& [width, lines] : cases)
        EXPECT_EQ(lineCount(Console::renderScoreSheet(game, width)), lines) << "width " << width;
}

TEST(test_score_grid, unknown_or_tiny_width_still_renders) {
    ScoreCalculator game;
    play(game);
    EXPECT_EQ(lineCount(Console::renderScoreSheet(game, 0)), 50u);
    EXPECT_EQ(lineCount(Console::renderScoreSheet(game, -5)), 50u);
}

TEST(test_score_grid, empty_sheet_draws_blank_boxes) {
    ScoreCalculator game;
    const auto box = Console::drawFrame(frames(game)[0]);
    EXPECT_EQ(box[0], ".-----------.");
    EXPECT_EQ(box[1], "|     |     |");
    EXPECT_EQ(box[2], "|     '-----|");
    EXPECT_EQ(box[3], "|           |");
    EXPECT_EQ(box[4], "'-----------'");
}

TEST(test_score_grid, tenth_frame_box_is_wider_and_shows_three_rolls) {
    ScoreCalculator game;
    play(game);
    const auto box = Console::drawFrame(frames(game)[9]);
    EXPECT_EQ(box[0], ".-----------------.");
    EXPECT_EQ(box[1], "|  X  |  8  |  1  |");
    EXPECT_EQ(box[2], "|     '-----'-----|");
    EXPECT_EQ(box[3], "|       167       |");
    EXPECT_EQ(box[4], "'-----------------'");
}
```

- [ ] **Step 3: Add the banner and screen-text tests to `test/test_Console.cpp`**

Add `#include "banner.h"` to the includes and append the tests below. The two screen tests pin today's success text (no echo, then a blank line) and the K3 fix.

```cpp
TEST(test_console, banner_matches_todays_text) {
    // Captured from plotIntr() on main (9bbbf6b) at 80 columns: 14 spaces on each side of the title.
    const std::string title = "Execution of BowlingGame from Bowlingbox Library";
    const std::string expected = std::string(79, '#') + "\n" +
                                 std::string(14, ' ') + title + std::string(14, ' ') + "\n" +
                                 std::string(79, '#') + "\n" +
                                 "This console game is an interface to the bowling game implemented under the "
                                 "Bowlingbox library.\n"
                                 "=About=\n"
                                 " Made by: github.com/medaminben\n" +
                                 std::string(79, '~') + "\n\n";
    EXPECT_EQ(Console::banner(80), expected);
}

TEST(test_console, banner_fits_the_terminal) {
    const std::string title = "Execution of BowlingGame from Bowlingbox Library";
    // Today's title padding: none up to 52 columns, then one space each side at 53 and 54.
    EXPECT_NE(Console::banner(52).find("\n" + title + "\n"), std::string::npos);
    EXPECT_NE(Console::banner(54).find("\n " + title + " \n"), std::string::npos);
    EXPECT_NO_THROW(Console::banner(0));
    EXPECT_NO_THROW(Console::banner(-5));
    EXPECT_EQ(Console::banner(10).substr(0, 10), std::string(9, '#') + "\n");
}

TEST(test_console, a_roll_shows_the_score_then_a_blank_line) {
    ScriptedTerminal terminal({"p", "7"});
    auto game = std::make_shared<Bowlingbox::Score::ScoreCalculator>();
    Console::Screen screen(game, terminal);
    screen.start();
    EXPECT_NE(terminal.output.find("You have scored: 7\n\nEnter a number between 0 and 3\n"), std::string::npos);
    EXPECT_EQ(terminal.output.find("Your choice: 7"), std::string::npos);
}

TEST(test_console, a_number_after_game_over_is_invalid) {
    std::vector<std::string> lines {"p"};
    lines.insert(lines.end(), 12, "10");
    lines.push_back("5");
    ScriptedTerminal terminal(lines);
    auto game = std::make_shared<Bowlingbox::Score::ScoreCalculator>();
    Console::Screen screen(game, terminal);
    screen.start();
    EXPECT_NE(terminal.output.find("Game Over!\t You have scored: 300\n\nSelect a choice:\n"), std::string::npos);
    EXPECT_NE(terminal.output.find("Your choice: 5\nInvalid choice!\nSelect a choice:\n"), std::string::npos);
    EXPECT_EQ(terminal.output.find("starting"), std::string::npos);
}
```

- [ ] **Step 4: Register the new tests and confirm they fail to build**

In `CMakeLists.txt`, change the test `SRC` line to:

```cmake
        SRC      test/test_Console.cpp test/test_Session.cpp test/test_ScoreGrid.cpp
```

Run: `cmake -S . -B build && cmake --build build --target test_bowlingconsole`
Expected: compile errors: `session.h: No such file or directory`, `banner.h: No such file or directory`, and `renderScoreSheet` / `drawFrame` not declared.

- [ ] **Step 5: Write `session.h` and `session.cpp`**

`session.h`:

```cpp
#ifndef CONSOLE_SESSION_H
#define CONSOLE_SESSION_H
#include <Bowlingbox/Score/IBowlingGame.h>
#include <memory>
#include <string>
#include <string_view>

namespace Console {

/// Game flow of the console front-end, without any I/O: one line of input in,
/// one message out.
class Session {
public:
    enum class Mode { Menu, Playing, GameOver };

    explicit Session(std::shared_ptr<Bowlingbox::Score::IBowlingGame> game);

    /// Applies one line of input: "q" quits, "p" / "r" start a new game and a
    /// number rolls while playing. Surrounding blanks and '\r' are ignored.
    void handle(std::string_view input);

    Mode mode() const { return _mode; }
    bool quit() const { return _quit; }
    /// Result of the last input, e.g. "You have scored: 7\n" or "Your choice: x\nInvalid choice!".
    const std::string& message() const { return _message; }
    /// The menu line for the current mode.
    std::string prompt() const;
    const Bowlingbox::Score::IScoreSheet& sheet() const { return *_game; }

private:
    std::shared_ptr<Bowlingbox::Score::IBowlingGame> _game;
    Mode _mode = Mode::Menu;
    bool _quit = false;
    std::string _message = "Welcome to the Bowling Game!";
};
}  // namespace Console
#endif  // CONSOLE_SESSION_H
```

`session.cpp`:

```cpp
#include "session.h"
#include <charconv>
#include <optional>
#include <utility>

namespace Console {
namespace {
std::string_view trim(std::string_view text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos)
        return {};
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

/// A whole number that fits an int, or std::nullopt.
std::optional<int> parsePins(std::string_view text) {
    if (text.empty())
        return std::nullopt;
    int pins = 0;
    const char* const end = text.data() + text.size();
    const auto [parsedEnd, error] = std::from_chars(text.data(), end, pins);
    if (error != std::errc() || parsedEnd != end)
        return std::nullopt;
    return pins;
}
}  // namespace

Session::Session(std::shared_ptr<Bowlingbox::Score::IBowlingGame> game) : _game(std::move(game)) {}

void Session::handle(std::string_view input) {
    const std::string_view choice = trim(input);
    if (choice == "q") {
        _quit = true;
        return;
    }
    if (choice == "p" || choice == "r") {
        _game->reset();
        _mode = Mode::Playing;
        _message = "New game started! Good Luck!";
        return;
    }

    const auto pins = parsePins(choice);
    // roll() refuses counts outside 0..pinsLeft(); no need to check them here.
    if (_mode != Mode::Playing || !pins || !_game->roll(*pins)) {
        _message = "Your choice: " + std::string(choice) + "\nInvalid choice!";
        return;
    }
    // Same text as before the refactor: no echo, and a trailing newline that the
    // screen shows as a blank line before the prompt.
    const std::string total = std::to_string(_game->score());
    if (_game->isOver()) {
        _mode = Mode::GameOver;
        _message = "Game Over!\t You have scored: " + total + "\n";
    } else {
        _message = "You have scored: " + total + "\n";
    }
}

std::string Session::prompt() const {
    if (_mode == Mode::Playing)
        return "Enter a number between 0 and " + std::to_string(_game->pinsLeft()) +
               "\nor the letter:  q - quit r - restart to continue...";
    return "Select a choice:\n q - quit p - play to continue...";
}

}  // namespace Console
```

- [ ] **Step 6: Rewrite `scoreGrid.h` and `scoreGrid.cpp`**

`scoreGrid.h`:

```cpp
#ifndef SCORE_GRID_H
#define SCORE_GRID_H
#include <Bowlingbox/Score/Frames.h>
#include <Bowlingbox/Score/IBowlingGame.h>
#include <array>
#include <string>

namespace Console {

/// Five text lines for one frame box: 13 columns, 19 for the tenth frame.
std::array<std::string, 5> drawFrame(const Bowlingbox::Score::FrameView& frame);

/// The ten frame boxes, each preceded by a space, wrapped to fit `width`
/// columns: 10, 5, 2 or 1 frames per row.
std::string renderScoreSheet(const Bowlingbox::Score::IScoreSheet& sheet, int width);

}  // namespace Console
#endif  // SCORE_GRID_H
```

`scoreGrid.cpp`:

```cpp
#include "scoreGrid.h"
#include <Bowlingbox/Score/Notation.h>
#include <algorithm>
#include <cstddef>

namespace Console {
namespace {
/// Score sheet notation centred in three columns; "   " when blank.
std::string cell(int value, bool isScore = false) {
    const std::string text = Bowlingbox::Score::notation(value, isScore);
    switch (text.size()) {
        case 0:
            return "   ";
        case 1:
            return " " + text + " ";
        case 2:
            return " " + text;
        default:
            return text;
    }
}

/// Frames per row, at the same widths as before the refactor. Those came from
/// ((width - 16) / 15) + 1 rounded down to 10, 5, 2 or 1. A row of 2 is 34
/// columns wide, so at 31-33 columns the terminal wraps it; that is kept on purpose.
int framesPerRow(int width) {
    if (width >= 151)
        return 10;
    if (width >= 76)
        return 5;
    if (width >= 31)
        return 2;
    return 1;
}
}  // namespace

std::array<std::string, 5> drawFrame(const Bowlingbox::Score::FrameView& frame) {
    const std::string first = cell(frame.rolls[0]);
    const std::string second = cell(frame.rolls[1]);
    const std::string score = cell(frame.score, true);
    if (frame.isLast()) {
        const std::string third = cell(frame.rolls[2]);
        return {".-----------------.",
                "| " + first + " | " + second + " | " + third + " |",
                "|     '-----'-----|",
                "|       " + score + "       |",
                "'-----------------'"};
    }
    return {".-----------.",
            "| " + first + " | " + second + " |",
            "|     '-----|",
            "|    " + score + "    |",
            "'-----------'"};
}

std::string renderScoreSheet(const Bowlingbox::Score::IScoreSheet& sheet, int width) {
    using Bowlingbox::Score::kFrames;
    const auto frames = Bowlingbox::Score::frames(sheet);
    const int perRow = framesPerRow(width);
    std::string out;
    for (int rowStart = 0; rowStart < kFrames; rowStart += perRow) {
        std::array<std::string, 5> lines;
        const int rowEnd = std::min(rowStart + perRow, kFrames);
        for (int f = rowStart; f < rowEnd; ++f) {
            const auto box = drawFrame(frames[static_cast<std::size_t>(f)]);
            for (std::size_t k = 0; k < lines.size(); ++k)
                lines[k] += " " + box[k];
        }
        for (const auto& line : lines)
            out += line + "\n";
    }
    return out;
}

}  // namespace Console
```

- [ ] **Step 7: Write `banner.h` and `banner.cpp`, and delete `screenTools`**

`banner.h`:

```cpp
#ifndef CONSOLE_BANNER_H
#define CONSOLE_BANNER_H
#include <string>

namespace Console {
/// Title, introduction and credits between bars one column narrower than
/// `width`; the same text the console printed before the refactor.
std::string banner(int width);
}  // namespace Console
#endif  // CONSOLE_BANNER_H
```

`banner.cpp`:

```cpp
#include "banner.h"
#include <algorithm>
#include <cstddef>

namespace Console {

std::string banner(int width) {
    const std::string title = "Execution of BowlingGame from Bowlingbox Library";
    const auto barWidth = static_cast<std::size_t>(std::max(width - 1, 0));
    // Today's padding, on both sides: (width - title - 1) / 2 - 1 spaces, 14 at 80 columns.
    const int padding = (width - static_cast<int>(title.size()) - 1) / 2 - 1;
    const std::string pad(static_cast<std::size_t>(std::max(padding, 0)), ' ');
    const std::string hashBar(barWidth, '#');
    return hashBar + "\n" + pad + title + pad + "\n" + hashBar + "\n" +
           "This console game is an interface to the bowling game implemented under the Bowlingbox library.\n"
           "=About=\n Made by: github.com/medaminben\n" +
           std::string(barWidth, '~') + "\n\n";
}

}  // namespace Console
```

Delete the old helpers: `git rm screenTools.h screenTools.cpp`.

- [ ] **Step 8: Rewrite `bowlingScreen.h` and `bowlingScreen.cpp`**

`bowlingScreen.h`:

```cpp
#ifndef BOWLING_SCREEN_H
#define BOWLING_SCREEN_H
#include <memory>

#include <Bowlingbox/Score/IBowlingGame.h>

#include "session.h"
#include "terminal.h"

namespace Console {

/// Console front-end: draws the banner, score sheet and menu, and feeds input
/// lines to a Session. Scoring and terminal I/O are injected.
class Screen {
public:
    Screen(std::shared_ptr<Bowlingbox::Score::IBowlingGame> game, ITerminal& terminal);
    /// Runs until "q" or the end of input.
    void start();

private:
    void draw();

    Session _session;
    ITerminal& _terminal;
};
}  // namespace Console

#endif  // BOWLING_SCREEN_H
```

`bowlingScreen.cpp`:

```cpp
#include "bowlingScreen.h"
#include "banner.h"
#include "scoreGrid.h"
#include <utility>

namespace Console {

Screen::Screen(std::shared_ptr<Bowlingbox::Score::IBowlingGame> game, ITerminal& terminal)
    : _session(std::move(game)), _terminal(terminal) {}

void Screen::start() {
    while (!_session.quit()) {
        draw();
        const auto line = _terminal.readLine();
        if (!line)  // end of input: nothing more will come
            return;
        _session.handle(*line);
    }
}

void Screen::draw() {
    const int width = _terminal.width();
    if (_terminal.clear())
        _terminal.write(banner(width));
    _terminal.write(renderScoreSheet(_session.sheet(), width));
    _terminal.write(_session.message() + "\n" + _session.prompt() + "\n");
}

}  // namespace Console
```

- [ ] **Step 9: Update the core library source list**

In `CMakeLists.txt`, replace the `add_library(bowlingconsole_core …)` block with:

```cmake
add_library(bowlingconsole_core STATIC
    banner.h banner.cpp
    bowlingScreen.h bowlingScreen.cpp
    scoreGrid.h scoreGrid.cpp
    session.h session.cpp
    terminal.h terminal.cpp
)
```

- [ ] **Step 10: Run all console tests**

Run: `cmake -S . -B build && cmake --build build --target test_bowlingconsole BowlingConsole && ./build/bin/test_bowlingconsole`
Expected: all PASS. That is 7 `test_session`, 7 `test_score_grid` and 7 `test_console` tests (the 3 existing ones plus the 2 banner and 2 screen-text tests).

- [ ] **Step 11: Smoke-test the real binary**

Run: `printf 'p\n10\n7\r\n3\n5\nq\n' | ./build/bin/BowlingConsole | tail -14`
Expected: the last grid shows `X` in frame 1, `7 /` in frame 2 and `5` in frame 3. The last message is `You have scored: 40` followed by a blank line and the `Enter a number between 0 and 5` prompt, with no `Your choice:` echo. There is no `Invalid choice!` anywhere in the output, which shows the `7\r` line was accepted.

- [ ] **Step 12: Commit**

```bash
git add -A applications/bowlingGame/bowlingGameCMD
git commit -m "refactor(console): Session state machine and pure score grid

Numbers after game over and input with blanks or CR are now handled;
ScoreGrid/Frame/screenTools are replaced by drawFrame/renderScoreSheet/banner.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Task 4: BowlingGL — commands with an amount, mouse through the pipeline

Fixes G1, G2, G8 and G9. `Command` carries an amount, so keyboard and mouse both go `InputMapper` → `Command` → `LaneCommands::execute`.

**Files (under `applications/bowlingGame/bowlingGameGL/`):**
- Rewrite: `Command.h`, `InputMapper.h`, `InputMapper.cpp`, `LaneCommands.cpp`, `test/test_InputMapper.cpp`
- Modify: `LaneCommands.h` (doc comment only), `BowlingCanvas.cpp` (`newGame`, `OnMouse`, `apply`), `test/test_Gameplay.cpp` (the two `test_lane_commands` tests)

**Interfaces:**
- Consumes: target `bowlinggl_core` (Task 1); `Alley::Lane::moveAim/rotateAim/startCharge/release/newGame` (existing).
- Produces:
  ```cpp
  struct Command {
      enum class Type { None, MoveAim, RotateAim, StartCharge, Release, NewGame, Quit };
      Type type = Type::None;
      float amount = 0.f;  // MoveAim: metres, RotateAim: degrees
      bool operator==(const Command&) const = default;
  };
  namespace InputMapper {
  constexpr float kKeyOffsetStep = 0.02f, kKeyAngleStep = 0.25f, kMouseOffsetSpan = 1.2f, kMouseAngleStep = 0.05f;
  Command keyDown(int keyCode, bool ctrl, Phase phase);
  Command keyUp(int keyCode, Phase phase);
  Command mouseDown(bool leftButton, Phase phase);
  Command mouseUp(bool leftButton, Phase phase);
  Command mouseMove(int dx, bool leftDown, bool rightDown, int canvasWidthPx, Phase phase);
  }
  Result<> LaneCommands::execute(Command command, Bowlingbox::Alley::Lane& lane);
  ```
  `mouseDxToOffset` and `mouseDxToAngle` are removed. Task 6 edits `BowlingCanvas.cpp` and `test_Gameplay.cpp` after this task is merged.

- [ ] **Step 1: Rewrite `test/test_InputMapper.cpp` (failing)**

```cpp
#include "InputMapper.h"
#include <wx/defs.h>
#include <gtest/gtest.h>
#include <ostream>

using Bowlingbox::Alley::Phase;
using Type = Command::Type;

// Readable failure messages for EXPECT_EQ on commands.
void PrintTo(const Command& c, std::ostream* os) {
    *os << "Command{type " << static_cast<int>(c.type) << ", amount " << c.amount << "}";
}

namespace {
Command cmd(Type type, float amount = 0.f) {
    return {type, amount};
}
}  // namespace

TEST(test_input_mapper, arrows_move_while_aiming_or_charging) {
    EXPECT_EQ(InputMapper::keyDown(WXK_LEFT, false, Phase::Aiming), cmd(Type::MoveAim, -InputMapper::kKeyOffsetStep));
    EXPECT_EQ(InputMapper::keyDown(WXK_RIGHT, false, Phase::Charging), cmd(Type::MoveAim, InputMapper::kKeyOffsetStep));
    EXPECT_EQ(InputMapper::keyDown(WXK_LEFT, false, Phase::Rolling), cmd(Type::None));
}

TEST(test_input_mapper, a_and_d_rotate_the_aim) {
    EXPECT_EQ(InputMapper::keyDown('A', false, Phase::Aiming), cmd(Type::RotateAim, -InputMapper::kKeyAngleStep));
    EXPECT_EQ(InputMapper::keyDown('d', false, Phase::Aiming), cmd(Type::RotateAim, InputMapper::kKeyAngleStep));
    EXPECT_EQ(InputMapper::keyDown('D', false, Phase::GameOver), cmd(Type::None));
}

TEST(test_input_mapper, space_charges_and_releases) {
    EXPECT_EQ(InputMapper::keyDown(WXK_SPACE, false, Phase::Aiming), cmd(Type::StartCharge));
    EXPECT_EQ(InputMapper::keyDown(WXK_SPACE, false, Phase::Charging), cmd(Type::None));  // key repeat
    EXPECT_EQ(InputMapper::keyUp(WXK_SPACE, Phase::Charging), cmd(Type::Release));
    EXPECT_EQ(InputMapper::keyUp(WXK_SPACE, Phase::Aiming), cmd(Type::None));
}

TEST(test_input_mapper, new_game_and_quit_work_in_any_phase) {
    EXPECT_EQ(InputMapper::keyDown('N', false, Phase::Rolling), cmd(Type::NewGame));
    EXPECT_EQ(InputMapper::keyDown('N', true, Phase::GameOver), cmd(Type::NewGame));
    EXPECT_EQ(InputMapper::keyDown(WXK_ESCAPE, false, Phase::Settling), cmd(Type::Quit));
    EXPECT_EQ(InputMapper::keyDown('Q', true, Phase::Aiming), cmd(Type::Quit));
    EXPECT_EQ(InputMapper::keyDown('Q', false, Phase::Aiming), cmd(Type::None));
}

TEST(test_input_mapper, mouse_buttons_mirror_space) {
    EXPECT_EQ(InputMapper::mouseDown(true, Phase::Aiming), cmd(Type::StartCharge));
    EXPECT_EQ(InputMapper::mouseDown(false, Phase::Aiming), cmd(Type::None));
    EXPECT_EQ(InputMapper::mouseUp(true, Phase::Charging), cmd(Type::Release));
    EXPECT_EQ(InputMapper::mouseUp(true, Phase::Rolling), cmd(Type::None));
}

TEST(test_input_mapper, mouse_move_shifts_or_turns_only_while_aiming) {
    const Command move = InputMapper::mouseMove(100, false, false, 1000, Phase::Aiming);
    EXPECT_EQ(move.type, Type::MoveAim);
    EXPECT_NEAR(move.amount, 0.12f, 1e-5f);  // 1.2 m across the canvas
    const Command turn = InputMapper::mouseMove(10, false, true, 1000, Phase::Aiming);
    EXPECT_EQ(turn.type, Type::RotateAim);
    EXPECT_NEAR(turn.amount, 0.5f, 1e-5f);   // 0.05° per pixel
    EXPECT_EQ(InputMapper::mouseMove(10, true, false, 1000, Phase::Aiming), cmd(Type::None));  // left-drag
    EXPECT_EQ(InputMapper::mouseMove(10, false, false, 1000, Phase::Rolling), cmd(Type::None));
    EXPECT_EQ(InputMapper::mouseMove(0, false, false, 1000, Phase::Aiming), cmd(Type::None));
}

TEST(test_input_mapper, zero_width_canvas_does_not_move_the_ball) {
    EXPECT_EQ(InputMapper::mouseMove(50, false, false, 0, Phase::Aiming), cmd(Type::None));
    EXPECT_EQ(InputMapper::mouseMove(50, false, false, -1, Phase::Aiming), cmd(Type::None));
}
```

- [ ] **Step 2: Rewrite the lane command tests in `test/test_Gameplay.cpp` (failing)**

Replace the two `test_lane_commands` tests with:

```cpp
TEST(test_lane_commands, routes_commands_to_the_lane) {
    Alley::Lane lane;
    EXPECT_TRUE(LaneCommands::execute({Command::Type::MoveAim, 0.1f}, lane).isValue());
    EXPECT_NEAR(lane.snapshot().aimOffset, 0.1f, 1e-6f);
    EXPECT_TRUE(LaneCommands::execute({Command::Type::RotateAim, 2.f}, lane).isValue());
    EXPECT_NEAR(lane.snapshot().aimAngle, 2.f, 1e-6f);
    EXPECT_TRUE(LaneCommands::execute({Command::Type::StartCharge}, lane).isValue());
    EXPECT_EQ(lane.snapshot().phase, Phase::Charging);
    EXPECT_TRUE(LaneCommands::execute({Command::Type::NewGame}, lane).isValue());
    EXPECT_EQ(lane.snapshot().phase, Phase::Aiming);
}

TEST(test_lane_commands, reports_rejections_and_ignores_non_lane_commands) {
    Alley::Lane lane;
    EXPECT_TRUE(LaneCommands::execute({Command::Type::Release}, lane).isError());
    EXPECT_TRUE(LaneCommands::execute({Command::Type::Quit}, lane).isValue());
    EXPECT_TRUE(LaneCommands::execute({Command::Type::None}, lane).isValue());
}
```

- [ ] **Step 3: Run and confirm the build fails**

Run: `cmake --build build --target test_bowlinggl`
Expected: compile errors such as `'Type' is not a member of 'Command'` and `'mouseMove' is not a member of 'InputMapper'`.

- [ ] **Step 4: Rewrite `Command.h`**

```cpp
#ifndef BOWLINGGL_COMMAND_H
#define BOWLINGGL_COMMAND_H

/// A player intent, independent of the input device that produced it.
struct Command {
    enum class Type { None, MoveAim, RotateAim, StartCharge, Release, NewGame, Quit };
    Type type = Type::None;
    float amount = 0.f;  // MoveAim: metres, RotateAim: degrees; unused otherwise

    bool operator==(const Command&) const = default;
};

#endif  // BOWLINGGL_COMMAND_H
```

- [ ] **Step 5: Rewrite `InputMapper.h` and `InputMapper.cpp`**

`InputMapper.h`:

```cpp
#ifndef BOWLINGGL_INPUT_MAPPER_H
#define BOWLINGGL_INPUT_MAPPER_H
#include "Command.h"
#include <Bowlingbox/Alley/Types.h>

/// Pure mapping from wx key codes / mouse events + current phase to commands.
namespace InputMapper {
constexpr float kKeyOffsetStep = 0.02f;   // metres per key event
constexpr float kKeyAngleStep = 0.25f;    // degrees per key event
constexpr float kMouseOffsetSpan = 1.2f;  // metres across the whole canvas
constexpr float kMouseAngleStep = 0.05f;  // degrees per pixel

Command keyDown(int keyCode, bool ctrl, Bowlingbox::Alley::Phase phase);
Command keyUp(int keyCode, Bowlingbox::Alley::Phase phase);
Command mouseDown(bool leftButton, Bowlingbox::Alley::Phase phase);
Command mouseUp(bool leftButton, Bowlingbox::Alley::Phase phase);
/// The pointer moved dx pixels. While aiming, a right-drag turns the aim and a
/// move with no button shifts the ball; a left-drag (charging) does nothing.
Command mouseMove(int dx, bool leftDown, bool rightDown, int canvasWidthPx, Bowlingbox::Alley::Phase phase);
}  // namespace InputMapper
#endif  // BOWLINGGL_INPUT_MAPPER_H
```

`InputMapper.cpp`:

```cpp
#include "InputMapper.h"
#include <wx/defs.h>

using Bowlingbox::Alley::Phase;
using Type = Command::Type;

namespace {
bool canAim(Phase p) {
    return p == Phase::Aiming || p == Phase::Charging;
}
}  // namespace

namespace InputMapper {

Command keyDown(int keyCode, bool ctrl, Phase phase) {
    if (keyCode == WXK_ESCAPE || (ctrl && (keyCode == 'Q' || keyCode == 'q')))
        return {Type::Quit};
    if (keyCode == 'N' || keyCode == 'n')
        return {Type::NewGame};
    switch (keyCode) {
        case WXK_LEFT:
            return canAim(phase) ? Command {Type::MoveAim, -kKeyOffsetStep} : Command {};
        case WXK_RIGHT:
            return canAim(phase) ? Command {Type::MoveAim, kKeyOffsetStep} : Command {};
        case 'A':
        case 'a':
            return canAim(phase) ? Command {Type::RotateAim, -kKeyAngleStep} : Command {};
        case 'D':
        case 'd':
            return canAim(phase) ? Command {Type::RotateAim, kKeyAngleStep} : Command {};
        case WXK_SPACE:
            return phase == Phase::Aiming ? Command {Type::StartCharge} : Command {};
        default:
            return {};
    }
}

Command keyUp(int keyCode, Phase phase) {
    if (keyCode == WXK_SPACE && phase == Phase::Charging)
        return {Type::Release};
    return {};
}

Command mouseDown(bool leftButton, Phase phase) {
    return (leftButton && phase == Phase::Aiming) ? Command {Type::StartCharge} : Command {};
}

Command mouseUp(bool leftButton, Phase phase) {
    return (leftButton && phase == Phase::Charging) ? Command {Type::Release} : Command {};
}

Command mouseMove(int dx, bool leftDown, bool rightDown, int canvasWidthPx, Phase phase) {
    if (dx == 0 || leftDown || phase != Phase::Aiming)
        return {};
    if (rightDown)
        return {Type::RotateAim, kMouseAngleStep * static_cast<float>(dx)};
    if (canvasWidthPx <= 0)  // minimised: no meaningful scale
        return {};
    return {Type::MoveAim, kMouseOffsetSpan * static_cast<float>(dx) / static_cast<float>(canvasWidthPx)};
}

}  // namespace InputMapper
```

- [ ] **Step 6: Rewrite `LaneCommands.cpp` and update the comment in `LaneCommands.h`**

`LaneCommands.cpp` (it no longer includes `InputMapper.h`):

```cpp
#include "LaneCommands.h"

namespace LaneCommands {

Result<> execute(Command command, Bowlingbox::Alley::Lane& lane) {
    switch (command.type) {
        case Command::Type::MoveAim:     return lane.moveAim(command.amount);
        case Command::Type::RotateAim:   return lane.rotateAim(command.amount);
        case Command::Type::StartCharge: return lane.startCharge();
        case Command::Type::Release:     return lane.release();
        case Command::Type::NewGame:     return lane.newGame();
        case Command::Type::Quit:
        case Command::Type::None:
            break;
    }
    return Result<>(std::monostate {});
}

}  // namespace LaneCommands
```

In `LaneCommands.h`, change the comment above the namespace to:

```cpp
/// Applies a game command to the lane, using its amount for MoveAim / RotateAim.
/// Quit and None are not lane commands and succeed without effect; the caller handles Quit.
```

- [ ] **Step 7: Route the canvas through the pipeline**

In `BowlingCanvas.cpp`, replace `newGame`, `OnMouse` and the first lines of `apply`:

```cpp
void BowlingCanvas::newGame() {
    apply({Command::Type::NewGame});
}
```

```cpp
void BowlingCanvas::OnMouse(wxMouseEvent& e) {
    const Phase phase = _lane.snapshot().phase;
    if (e.LeftDown() || e.RightDown()) {
        SetFocus();
        apply(InputMapper::mouseDown(e.LeftDown(), phase));
    } else if (e.LeftUp() || e.RightUp()) {
        apply(InputMapper::mouseUp(e.LeftUp(), phase));
    } else if (e.Moving() || e.Dragging()) {
        const int dx = e.GetPosition().x - _lastMouse.x;
        apply(InputMapper::mouseMove(dx, e.LeftIsDown(), e.RightIsDown(), GetClientSize().x, phase));
    }
    _lastMouse = e.GetPosition();  // every event, so a left-drag leaves no stale origin
}
```

```cpp
void BowlingCanvas::apply(Command command) {
    if (command.type == Command::Type::Quit) {
        if (onQuit) onQuit();
        return;
    }
```

The rest of `apply` stays. `OnKeyDown` and `OnKeyUp` compare against `Command::None`; change both comparisons to `c.type == Command::Type::None`.

- [ ] **Step 8: Build and run the tests**

Run: `cmake --build build -j --target BowlingGL test_bowlinggl && ./build/bin/test_bowlinggl`
Expected: the app builds with no warnings, and every test passes (7 `test_input_mapper`, 2 `test_lane_commands`, plus the unchanged camera/HUD tests).

- [ ] **Step 9: Commit**

```bash
git add applications/bowlingGame/bowlingGameGL
git commit -m "refactor(gl): commands carry an amount; mouse aim goes through LaneCommands

Zero-width canvases no longer jump the aim and the mouse origin is
tracked on every event.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Task 5: BowlingDisplay (Qt) — tidy window and its first tests

Fixes Q1–Q5. Builds with `BUILD_QT_UI=ON` against the system Qt 6.

**Files (under `applications/bowlingGame/bowlingGameUI/`):**
- Rewrite: `display.h`, `display.cpp`, `main.cpp`
- Create: `test/test_Display.cpp`
- Modify: `CMakeLists.txt` (add the test block)
- Unchanged: `display.ui`

**Interfaces:**
- Consumes: `Bowlingbox::Score::frames(const IScoreSheet&)` and `FrameView` (Task 2); target `bowlingdisplay_core` (Task 1); widget object names from `display.ui`: `btn_0`…`btn_10`, `btn_restart`, `pinsGroup`, `lblScore`, `lblRoll{1,2}Frame{1..10}`, `lblRoll3Frame10`, `lblScoreFrame{1..10}`.
- Produces: `class BowlingDisplay::MainWindow : public QMainWindow` with `explicit MainWindow(QWidget* parent = nullptr, std::unique_ptr<Bowlingbox::Score::IBowlingGame> game = nullptr)`. The test executable is `test_bowlingdisplay`.

- [ ] **Step 1: Write the failing tests**

`test/test_Display.cpp`:

```cpp
#include "display.h"
#include <Bowlingbox/Score/Score.h>
#include <QApplication>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <gtest/gtest.h>
#include <initializer_list>
#include <memory>
#include <vector>

namespace {
/// ScoreCalculator that counts reset() calls.
class CountingGame final : public Bowlingbox::Score::IBowlingGame {
public:
    explicit CountingGame(int& resets) : _resets(resets) {}
    bool roll(int pins) override { return _inner.roll(pins); }
    void reset() override {
        ++_resets;
        _inner.reset();
    }
    std::vector<int> rolls() const override { return _inner.rolls(); }
    std::vector<int> scores() const override { return _inner.scores(); }
    int score() const override { return _inner.score(); }
    int frameScore(int frameIndex) const override { return _inner.frameScore(frameIndex); }
    int pinsLeft() const override { return _inner.pinsLeft(); }
    bool isOver() const override { return _inner.isOver(); }

private:
    Bowlingbox::Score::ScoreCalculator _inner;
    int& _resets;
};

QString text(QWidget& w, const QString& label) {
    return w.findChild<QLabel*>(label)->text();
}
QPushButton* pinButton(QWidget& w, int pins) {
    return w.findChild<QPushButton*>(QString("btn_%1").arg(pins));
}
void roll(QWidget& w, std::initializer_list<int> pins) {
    for (int p : pins)
        pinButton(w, p)->click();
}
}  // namespace

TEST(test_display, pin_buttons_fill_the_sheet) {
    BowlingDisplay::MainWindow w;
    roll(w, {7, 3, 10});
    EXPECT_EQ(text(w, "lblRoll1Frame1"), "7");
    EXPECT_EQ(text(w, "lblRoll2Frame1"), "/");
    EXPECT_EQ(text(w, "lblRoll1Frame2"), "X");
    EXPECT_EQ(text(w, "lblScoreFrame1"), "20");
    EXPECT_EQ(text(w, "lblScoreFrame2"), "");
}

TEST(test_display, only_standing_pin_counts_are_offered) {
    BowlingDisplay::MainWindow w;
    roll(w, {7});
    EXPECT_FALSE(pinButton(w, 3)->isHidden());
    EXPECT_TRUE(pinButton(w, 4)->isHidden());
    EXPECT_TRUE(pinButton(w, 10)->isHidden());
}

TEST(test_display, tenth_frame_shows_three_rolls) {
    BowlingDisplay::MainWindow w;
    for (int i = 0; i < 18; ++i)
        roll(w, {0});
    roll(w, {7, 3, 5});
    EXPECT_EQ(text(w, "lblRoll1Frame10"), "7");
    EXPECT_EQ(text(w, "lblRoll2Frame10"), "/");
    EXPECT_EQ(text(w, "lblRoll3Frame10"), "5");
    EXPECT_EQ(text(w, "lblScoreFrame10"), "15");
    EXPECT_EQ(text(w, "lblScore"), "15");
}

TEST(test_display, game_over_disables_the_pins) {
    BowlingDisplay::MainWindow w;
    for (int i = 0; i < 12; ++i)
        roll(w, {10});
    EXPECT_EQ(text(w, "lblScore"), "300");
    EXPECT_FALSE(w.findChild<QGroupBox*>("pinsGroup")->isEnabled());
    for (int pins = 0; pins <= 10; ++pins)
        EXPECT_FALSE(pinButton(w, pins)->isHidden()) << pins;
}

TEST(test_display, restart_resets_once_and_clears_the_sheet) {
    int resets = 0;
    BowlingDisplay::MainWindow w(nullptr, std::make_unique<CountingGame>(resets));
    roll(w, {7});
    w.findChild<QPushButton*>("btn_restart")->click();
    EXPECT_EQ(resets, 1);  // an extra connectSlotsByName connection would make this 2
    EXPECT_EQ(text(w, "lblRoll1Frame1"), "");
    EXPECT_EQ(text(w, "lblScore"), "0");
    EXPECT_FALSE(pinButton(w, 10)->isHidden());
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");  // no display needed
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

Add to the end of `CMakeLists.txt`:

```cmake
# Opens the form on Qt's offscreen platform; no display needed.
if(BUILD_TESTING)
    build_gtest_executable(
        NAME     test_bowlingdisplay
        SRC      test/test_Display.cpp
        DEPENDS  bowlingdisplay_core GTest::gtest
        DISCOVER ON
    )
endif()
```

- [ ] **Step 2: Run and confirm the build fails**

Run: `cmake -S . -B build && cmake --build build --target test_bowlingdisplay`
Expected: compile error, `'BowlingDisplay' has not been declared`.

- [ ] **Step 3: Rewrite `display.h`**

```cpp
/**
  @file display.h
  @author Mohamed Lamine Ben Hassine
  @mail mohamed.ben-hassine@mailfence.com
  @details the declaration of the Qt score sheet window
*/

#ifndef DISPLAY_H
#define DISPLAY_H
#include <QLabel>
#include <QList>
#include <QMainWindow>
#include <QPushButton>

#include <Bowlingbox/Score/IBowlingGame.h>
#include <memory>

namespace Ui {
class Display;
}

namespace BowlingDisplay {

/// Qt score sheet: one button per pin count, the 21 roll cells and the 10 frame scores.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    /// `game` defaults to a Bowlingbox::Score::ScoreCalculator.
    explicit MainWindow(QWidget* parent = nullptr,
                        std::unique_ptr<Bowlingbox::Score::IBowlingGame> game = nullptr);
    ~MainWindow() override;

private:
    void roll(int pins);
    void restart();
    void refresh();
    void updateGrid();
    void updatePinButtons();

    std::unique_ptr<Ui::Display> _ui;
    std::unique_ptr<Bowlingbox::Score::IBowlingGame> _game;
    QList<QPushButton*> _pinButtons;  // _pinButtons[i] rolls i pins
    QList<QLabel*> _rollLabels;       // the 21 roll slots, in Score encoding order
    QList<QLabel*> _scoreLabels;      // the 10 frame totals
};
}  // namespace BowlingDisplay
#endif  // DISPLAY_H
```

- [ ] **Step 4: Rewrite `display.cpp`**

```cpp
/**
  @file display.cpp
  @author Mohamed Lamine Ben Hassine
  @mail mohamed.ben-hassine@mailfence.com
  @details the Qt score sheet window: rolls go to the game, the sheet is redrawn from it
*/
#include "display.h"
#include "ui_display.h"
#include <Bowlingbox/Score/Frames.h>
#include <Bowlingbox/Score/Score.h>
#include <cstddef>

namespace BowlingDisplay {
namespace {
QString cellText(int value, bool isScore = false) {
    return QString::fromStdString(Bowlingbox::Score::notation(value, isScore));
}

/// The child of `parent` called `name`; display.ui must define it.
template <typename T>
T* widget(QWidget* parent, const QString& name) {
    T* found = parent->findChild<T*>(name);
    Q_ASSERT_X(found, "BowlingDisplay", qPrintable("display.ui has no widget " + name));
    return found;
}
}  // namespace

MainWindow::MainWindow(QWidget* parent, std::unique_ptr<Bowlingbox::Score::IBowlingGame> game)
    : QMainWindow(parent),
      _ui(std::make_unique<Ui::Display>()),
      _game(game ? std::move(game) : std::make_unique<Bowlingbox::Score::ScoreCalculator>()) {
    _ui->setupUi(this);

    for (int pins = 0; pins <= Bowlingbox::Score::kPins; ++pins) {
        auto* button = widget<QPushButton>(this, QString("btn_%1").arg(pins));
        _pinButtons.append(button);
        connect(button, &QPushButton::clicked, this, [this, pins] { roll(pins); });
    }
    for (int frame = 1; frame <= Bowlingbox::Score::kFrames; ++frame) {
        const int rolls = frame == Bowlingbox::Score::kFrames ? 3 : 2;
        for (int r = 1; r <= rolls; ++r)
            _rollLabels.append(widget<QLabel>(this, QString("lblRoll%1Frame%2").arg(r).arg(frame)));
        _scoreLabels.append(widget<QLabel>(this, QString("lblScoreFrame%1").arg(frame)));
    }
    connect(_ui->btn_restart, &QPushButton::clicked, this, &MainWindow::restart);
    refresh();
}

MainWindow::~MainWindow() = default;

void MainWindow::roll(int pins) {
    _game->roll(pins);
    refresh();
}

void MainWindow::restart() {
    _game->reset();
    refresh();
}

void MainWindow::refresh() {
    updateGrid();
    updatePinButtons();
}

void MainWindow::updateGrid() {
    for (const auto& frame : Bowlingbox::Score::frames(*_game)) {
        for (int r = 0; r < frame.rollCount; ++r)
            _rollLabels.at(2 * frame.index + r)->setText(cellText(frame.rolls[static_cast<std::size_t>(r)]));
        _scoreLabels.at(frame.index)->setText(cellText(frame.score, true));
    }
    _ui->lblScore->setText(QString::number(_game->score()));
}

void MainWindow::updatePinButtons() {
    const bool over = _game->isOver();
    _ui->pinsGroup->setDisabled(over);
    const int left = _game->pinsLeft();
    for (int pins = 0; pins < _pinButtons.size(); ++pins)
        _pinButtons.at(pins)->setVisible(over || pins <= left);
}

}  // namespace BowlingDisplay
```

The restart slot is no longer named `on_btn_restart_clicked`, so `setupUi`'s `connectSlotsByName` does not connect it a second time.

- [ ] **Step 5: Update `main.cpp`**

Replace `DisplayF w;` with `BowlingDisplay::MainWindow w;`. The rest stays.

- [ ] **Step 6: Build and run the tests**

Run: `cmake --build build -j --target BowlingDisplay test_bowlingdisplay && ./build/bin/test_bowlingdisplay`
Expected: no warnings, 5 tests PASS. If the offscreen plugin is missing, the run aborts with `Could not find the Qt platform plugin "offscreen"`; stop and report it rather than switching platforms.

- [ ] **Step 7: Commit**

```bash
git add -A applications/bowlingGame/bowlingGameUI
git commit -m "refactor(qt): BowlingDisplay::MainWindow with explicit wiring and tests

Widgets are collected by name, restart is connected explicitly, ui is
owned by a unique_ptr and the sheet is drawn from Score::frames().

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

## Wave 3

### Task 6: BowlingGL — fixed time step, HUD layout, named constants, portable main

Fixes G3–G7. Branches from `refactor/apps` after Task 4 is merged, so `Command` already has a `type`.

**Files (under `applications/bowlingGame/bowlingGameGL/`):**
- Create: `FixedStep.h`, `FixedStep.cpp`
- Modify: `CMakeLists.txt` (add `FixedStep.h FixedStep.cpp` to `bowlinggl_core`), `Hud.h`, `Hud.cpp`, `HudText.h`, `HudText.cpp`, `FollowCamera.h`, `FollowCamera.cpp`, `SceneAdapter.cpp`, `BowlingCanvas.h`, `BowlingCanvas.cpp`, `BowlingFrame.cpp`, `main.cpp`
- Test: `test/test_Gameplay.cpp`

**Interfaces:**
- Consumes: `Command::Type` (Task 4); target `bowlinggl_core` (Task 1).
- Produces:
  - `class FixedStep { static constexpr float kDt = 1.f / 60.f; static constexpr int kMaxSteps = 5; int advance(float elapsedSeconds); }`
  - `HudLayout hudLayout(int width, int height);` and `int HudLayout::middlePx() const`
  - `std::string HudText::controlsHelp();`
  - `FollowCamera::kBehindBall = 3.5f`, `kDeckMargin = 5.5f`, `kEaseRate = 6.f`

- [ ] **Step 1: Write the failing tests**

In `test/test_Gameplay.cpp`, add `#include "FixedStep.h"` and append:

```cpp
// ------------------------------------------------------------ FixedStep
TEST(test_fixed_step, half_steps_accumulate) {
    FixedStep clock;
    EXPECT_EQ(clock.advance(FixedStep::kDt / 2), 0);
    EXPECT_EQ(clock.advance(FixedStep::kDt / 2), 1);
}

TEST(test_fixed_step, a_jittery_16ms_timer_still_runs_60_steps_per_second) {
    FixedStep clock;
    int steps = 0;
    for (int i = 0; i < 1000; ++i)
        steps += clock.advance(i % 2 == 0 ? 0.012f : 0.020f);  // 16 ms on average
    EXPECT_NEAR(steps, 960, 1);                                  // 16 s of game time
}

TEST(test_fixed_step, long_stall_runs_at_most_five_steps_and_drops_the_rest) {
    FixedStep clock;
    EXPECT_EQ(clock.advance(2.f), FixedStep::kMaxSteps);
    EXPECT_EQ(clock.advance(0.f), 0);
    EXPECT_EQ(clock.advance(-1.f), 0);  // a clock going backwards is ignored
}

// ------------------------------------------------------------ HUD layout and help
TEST(test_hud, layout_reserves_top_and_bottom_bands) {
    const HudLayout l = hudLayout(1000, 800);
    EXPECT_EQ(l.width, 1000);
    EXPECT_EQ(l.height, 800);
    EXPECT_EQ(l.topPx, 144);
    EXPECT_EQ(l.bottomPx, 80);
    EXPECT_EQ(l.middlePx(), 576);
}

TEST(test_hud_text, controls_help_names_every_binding) {
    const std::string help = HudText::controlsHelp();
    for (const char* key : {"Left / Right", "A / D", "Space", "N", "Esc", "Ctrl+Q"})
        EXPECT_NE(help.find(key), std::string::npos) << key;
}
```

In the two `test_follow_camera` tests, replace `8.f - 3.5f` with `8.f - FollowCamera::kBehindBall` and `Alley::kLane.length - 5.5f` with `Alley::kLane.length - FollowCamera::kDeckMargin`.

- [ ] **Step 2: Run and confirm the build fails**

Run: `cmake --build build --target test_bowlinggl`
Expected: compile errors: `FixedStep.h: No such file or directory`, and `hudLayout`, `controlsHelp`, `kBehindBall` not declared.

- [ ] **Step 3: Write `FixedStep`**

`FixedStep.h`:

```cpp
#ifndef BOWLINGGL_FIXED_STEP_H
#define BOWLINGGL_FIXED_STEP_H

/// Turns irregular timer ticks into whole physics steps of kDt, so the game
/// runs at the same speed whatever the timer jitter.
class FixedStep {
public:
    static constexpr float kDt = 1.f / 60.f;
    static constexpr int kMaxSteps = 5;  // after a stall, drop the backlog instead of fast-forwarding

    /// Adds the wall time since the last tick; returns how many kDt steps to run now.
    int advance(float elapsedSeconds);

private:
    float _pending = 0.f;
};
#endif  // BOWLINGGL_FIXED_STEP_H
```

`FixedStep.cpp`:

```cpp
#include "FixedStep.h"
#include <algorithm>

int FixedStep::advance(float elapsedSeconds) {
    _pending += std::max(elapsedSeconds, 0.f);
    const int steps = static_cast<int>(_pending / kDt);
    if (steps > kMaxSteps) {
        _pending = 0.f;
        return kMaxSteps;
    }
    _pending -= static_cast<float>(steps) * kDt;
    return steps;
}
```

In `CMakeLists.txt`, add `FixedStep.h FixedStep.cpp` as a new line in the `bowlinggl_core` source list, after `Command.h`.

- [ ] **Step 4: Add `hudLayout` and name the HUD colours**

In `Hud.h`, replace the `HudLayout` struct with:

```cpp
/// Screen bands in pixels: scoreboard on top, power bar / hint / result at the bottom.
struct HudLayout {
    int width;
    int height;
    int topPx;
    int bottomPx;
    int middlePx() const { return height - topPx - bottomPx; }  // the 3D scene
};

/// The bands for a width x height viewport: 18 % on top, 10 % at the bottom.
HudLayout hudLayout(int width, int height);
```

In `Hud.cpp`, add after the `using namespace` line:

```cpp
namespace {
const glm::vec4 kBarOutline {0.f, 0.898f, 1.f, 1.f};  // cyan, as the scoreboard lines
const glm::vec4 kPowerLow {1.f, 0.541f, 0.f, 1.f};    // orange
const glm::vec4 kPowerHigh {1.f, 0.780f, 0.f, 1.f};   // amber
const glm::vec3 kHintInk {0.85f, 0.85f, 0.85f};
const glm::vec3 kResultInk {1.f, 0.780f, 0.2f};
}  // namespace

HudLayout hudLayout(int width, int height) {
    return {width, height, static_cast<int>(static_cast<float>(height) * 0.18f),
            static_cast<int>(static_cast<float>(height) * 0.10f)};
}
```

In `drawHud`, use the names: `canvas.strokeRect({margin, barY, barW, barH}, 2.f, kBarOutline);`, `const glm::vec4 powerFill = glm::mix(kPowerLow, kPowerHigh, s.power);`, `kHintInk` as the hint colour and `kResultInk` as the result colour.

- [ ] **Step 5: Add `HudText::controlsHelp`**

In `HudText.h`, add after `status`:

```cpp
/// Text of the Help ▸ Controls dialog.
std::string controlsHelp();
```

In `HudText.cpp`, add inside the namespace:

```cpp
std::string controlsHelp() {
    return "Left / Right or mouse move: position the ball\n"
           "A / D or right-drag: aim angle\n"
           "Hold Space or left button: charge, release to throw\n"
           "N: new game    Esc / Ctrl+Q: quit";
}
```

In `BowlingFrame.cpp`, add `#include "HudText.h"` and make `OnControls` read:

```cpp
void BowlingFrame::OnControls(wxCommandEvent&) {
    wxMessageBox(wxString::FromUTF8(HudText::controlsHelp()), "Controls", wxOK | wxICON_INFORMATION, this);
}
```

- [ ] **Step 6: Name the camera constants and drop the literal 10 in `SceneAdapter`**

In `FollowCamera.h`, add below `kLookAhead`:

```cpp
    static constexpr float kBehindBall = 3.5f;  // metres between the camera and a rolling ball
    static constexpr float kDeckMargin = 5.5f;  // stop this far before the end of the deck
    static constexpr float kEaseRate = 6.f;     // 1/s: how fast the camera catches up
```

In `FollowCamera.cpp`, the body of `update` becomes:

```cpp
    float target = kHomeZ;
    if (s.phase == Phase::Rolling || s.phase == Phase::Settling || s.phase == Phase::FrameDone)
        target = std::clamp(s.ball.position[2] - kBehindBall, kHomeZ, Bowlingbox::Alley::kLane.length - kDeckMargin);
    _z += (target - _z) * std::min(1.f, kEaseRate * dt);
```

In `SceneAdapter.cpp`, replace the pin loop with:

```cpp
    static_assert(std::tuple_size_v<decltype(d.pins)> == std::tuple_size_v<decltype(s.pins)>);
    for (std::size_t i = 0; i < s.pins.size(); ++i) {
        d.pins[i] = toTransform(s.pins[i]);
        d.pinVisible[i] = s.pinInPlay[i];
    }
```

and add `#include <array>`, `#include <cstddef>` and `#include <tuple>` to its includes (`std::tuple_size_v` lives in `<tuple>`; MSVC does not pull it in through `<array>`).

- [ ] **Step 7: Run the unit tests**

Run: `cmake -S . -B build && cmake --build build --target test_bowlinggl && ./build/bin/test_bowlinggl`
Expected: all PASS, including 3 `test_fixed_step` tests, `test_hud.layout_reserves_top_and_bottom_bands` and `test_hud_text.controls_help_names_every_binding`.

- [ ] **Step 8: Use `FixedStep`, `hudLayout` and the status cache in the canvas**

In `BowlingCanvas.h`, add `#include "FixedStep.h"` and `#include <chrono>`, and these members after `_lastMouse`:

```cpp
    FixedStep _clock;
    std::chrono::steady_clock::time_point _lastTick = std::chrono::steady_clock::now();
    wxString _status;  // last text sent to onStatus
```

In `BowlingCanvas.cpp`, delete the anonymous namespace holding `kDt`. Replace `OnTimer` and `renderFrame` with:

```cpp
void BowlingCanvas::OnTimer(wxTimerEvent&) {
    const auto now = std::chrono::steady_clock::now();
    const int steps = _clock.advance(std::chrono::duration<float>(now - _lastTick).count());
    _lastTick = now;
    for (int i = 0; i < steps; ++i)
        _lane.step(FixedStep::kDt);
    const auto s = _lane.snapshot();
    _follow.update(s, static_cast<float>(steps) * FixedStep::kDt);
    _follow.applyTo(_camera);
    const wxString status = wxString::FromUTF8(HudText::status(s, _lane.scoreSheet()));
    if (onStatus && status != _status) {
        _status = status;
        onStatus(status);
    }
    Refresh(false);
}
```

```cpp
void BowlingCanvas::renderFrame() {
    const HudLayout layout = hudLayout(_ctx->width(), _ctx->height());
    const auto s = _lane.snapshot();

    _ctx->beginFrame({0.02f, 0.03f, 0.08f});  // kClearColor — splash backdrop
    _camera.setViewport(layout.width, layout.middlePx());
    _scene->draw(_camera, SceneAdapter::toScene(s), layout.bottomPx, layout.middlePx());
    _overlay->begin(layout.width, layout.height);
    drawHud(*_overlay, s, _lane.scoreSheet(), layout);
    _ctx->endFrame();
}
```

- [ ] **Step 9: Make `main.cpp` portable**

Wrap the Wayland block in `main` so that MSVC never sees `setenv`:

```cpp
int main(int argc, char* argv[]) {
#ifdef __linux__
    // wxGLCanvas uses GLX (X11). Native/wl GDK backends segfault in glXQueryVersion on Wayland sessions.
    if (std::getenv("WAYLAND_DISPLAY") != nullptr) {
        const char* gdk = std::getenv("GDK_BACKEND");
        if (gdk == nullptr || std::strcmp(gdk, "wayland") == 0)
            setenv("GDK_BACKEND", "x11", 1);
    }
#endif
    return wxEntry(argc, argv);
}
```

- [ ] **Step 10: Build the app and run all tests**

Run: `cmake --build build -j && ctest --test-dir build --output-on-failure`
Expected: no warnings, all tests PASS.

If a display is available (`echo $DISPLAY $WAYLAND_DISPLAY` prints something), run `timeout 5 ./build/bin/BowlingGL; echo "exit $?"`. Expected: the window opens, nothing is printed on stderr, and `exit 124` (killed by the timeout). With no display, skip this check and say so in the report. The controller runs the README manual checklist once at the end.

- [ ] **Step 11: Commit**

```bash
git add applications/bowlingGame/bowlingGameGL
git commit -m "refactor(gl): fixed time step, HUD layout and named constants

FixedStep decouples game speed from timer jitter, the status bar updates
only on change, the controls text has one source and main() builds on MSVC.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Task 7: Docs — README, architecture and manual

Runs in parallel with Task 6. It uses only the names fixed in the Interfaces blocks above. No build is needed.

**Files:**
- Modify: `applications/bowlingGame/README.md` (§ Architecture, § BowlingDisplay)
- Modify: `docs/ARCHITECTURE.md` (§ Bowlingbox::Score table, § Applications, the data-flow diagram, § Helper functions, § Testing)
- Modify: `docs/MANUAL.md` (§ 4 Playing BowlingConsole)

**Interfaces:**
- Consumes: the names produced by Tasks 1–6 (`Score::frames`, `FrameView`, `bowlingconsole_core` / `bowlingdisplay_core` / `bowlinggl_core`, `Console::Session`, `renderScoreSheet`, `drawFrame`, `banner`, `Command {type, amount}`, `InputMapper::mouseMove`, `FixedStep`, `hudLayout`, `HudText::controlsHelp`, `BowlingDisplay::MainWindow`, `test_bowlingdisplay`).
- Produces: documentation only.

- [ ] **Step 1: List the stale mentions**

Run: `grep -nE "DisplayF|ScoreGrid|screenTools|MoveLeft|AngleRight|PROJECT_LIST|in a loop|mouseDx|Typo|60 Hz" applications/bowlingGame/README.md docs/ARCHITECTURE.md docs/MANUAL.md`
Each hit is handled by one of the steps below. Re-run the command in Step 6.

- [ ] **Step 2: `applications/bowlingGame/README.md`**

In § Architecture, replace the two paragraphs after the interface table (from "`Score/Notation.h` owns…" to the end of the "BowlingGL: …" paragraph) with:

```markdown
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
```

In § BowlingDisplay, replace the last sentence with:

```markdown
Kept for comparison; off by default because of the Qt licence. Build with `-DBUILD_QT_UI=ON` (needs Qt 6).
`test_bowlingdisplay` opens the form on Qt's offscreen platform, so it needs no display.
```

- [ ] **Step 3: `docs/ARCHITECTURE.md` — Score and Applications**

In the `Bowlingbox::Score` header table, add a row after `Notation.h`:

```markdown
| `Frames.h` | `FrameView` and `frames()`: the 21 slots and 10 totals cut into ten frames (the tenth has three rolls) |
```

In the BowlingGL units table, replace these rows:

```markdown
| `BowlingCanvas` | `wxGLCanvas`: owns the GL resources and the `Lane`, runs the timer, forwards wx events |
| `FixedStep` | Turns irregular timer ticks into 1/60 s physics steps; drops the backlog after a stall |
| `Command.h` | `Command {type, amount}`: `MoveAim` (metres), `RotateAim` (degrees), `StartCharge`, `Release`, `NewGame`, `Quit` |
| `InputMapper` | Pure functions: key, button or mouse move + current `Phase` → `Command` |
| `Hud`, `HudText` | Scoreboard, power bar, hint and result text; `hudLayout()` sizes the bands; `HudText` holds the strings, including the Controls dialog |
```

Replace the BowlingConsole paragraph with:

```markdown
`Console::Session` is the game flow without I/O: `handle(line)` moves between `Menu`, `Playing` and
`GameOver` and sets the message; rolls are validated by `IBowlingGame::roll`. `Console::Screen`
receives an `IBowlingGame` and an `ITerminal` (`SystemTerminal` uses stdin/stdout), draws
`banner()`, `renderScoreSheet()` and the session's message and prompt, and feeds it input lines.
```

Append to the BowlingDisplay paragraph:

```markdown
`BowlingDisplay::MainWindow` finds its widgets by name in `display.ui` and draws the sheet from
`Score::frames()`.
```

- [ ] **Step 4: `docs/ARCHITECTURE.md` — data flow, build and testing**

In the sequence diagram, change `IM-->>C: Command::Release` to `IM-->>C: Command{Release}`, and `loop every 1/60 s` to `loop FixedStep: 1/60 s steps per timer tick`.

In § Helper functions, replace the `create_application` bullet with:

```markdown
- `create_application(NAME … ENTRY Console|QT_ui|WX_ui …)` creates an executable of the right kind.
  It stops with an error for an unknown ENTRY or a disabled toolkit, so each app directory calls it
  directly, inside `if(BUILD_QT_UI)` / `if(BUILD_WX_UI)` for toolkit apps. The app's logic goes in
  a static `<app>_core` library that the executable and its test both link.
```

In § Testing, replace the `test_bowlinggl` and `test_bowlingconsole` rows and add the Qt row:

```markdown
| `test_bowlinggl` | Input mapping, lane commands, fixed time step, follow camera, HUD layout, text and drawing; no window is opened |
| `test_bowlingconsole` | `Session` (menu, rolls, game over, input trimming), score grid text against golden output, banner, and the game loop with a scripted terminal |
| `test_bowlingdisplay` | The Qt form on the offscreen platform: pin buttons, tenth frame, game over, restart (only with `BUILD_QT_UI=ON`) |
```

Also add to the `test_bowlingbox_score` row: `, frames()`.

- [ ] **Step 5: `docs/MANUAL.md` § 4**

Replace the table and the paragraph after it with:

```markdown
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
```

- [ ] **Step 6: Check and commit**

Re-run the grep from Step 1. Expected: no hits except intentional ones (for example "60 Hz" if the README still describes the timer rate in prose; reword it to "1/60 s steps").

```bash
git add applications/bowlingGame/README.md docs/ARCHITECTURE.md docs/MANUAL.md
git commit -m "docs: describe the refactored apps, Score::frames and app core libraries

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

## Final: whole-branch review and hand-off

- [ ] **Step 1: Integration build and tests**

Run the integration build from "Execution setup" on `refactor/apps`. Expected: no warnings, every test passes. That is the library suites, `test_bowlingconsole` (21), `test_bowlinggl` (all, including the new FixedStep/HUD tests) and `test_bowlingdisplay` (5).

- [ ] **Step 2: Whole-branch review**

Dispatch one reviewer on the diff `main...refactor/apps` against the spec and this plan's Global Constraints and Review Focus. Fix confirmed findings on `refactor/apps` in follow-up commits.

- [ ] **Step 3: Manual checklist**

Ask your human partner to run the six-step manual checklist in `applications/bowlingGame/README.md`, plus a mouse-aim check (move, right-drag, left-press-and-release). The canvas cannot be tested without a display.

- [ ] **Step 4: Clean up and hand off**

Remove the task worktrees (`git worktree remove build/worktrees/<task-id>` for each) and delete the merged `refactor/<task-id>` branches. Then ask your human partner whether to push `refactor/apps` and open a PR to `main` (CI auto-merges green PRs). Do not push without that confirmation.
