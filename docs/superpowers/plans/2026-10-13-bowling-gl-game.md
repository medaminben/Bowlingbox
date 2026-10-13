# Bowling GL Game Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace "type the number of pins" with a playable single-player bowling game: aim, charge, throw a ball down a 3D lane, rigid-body pins fall, and `Sandbox::Gamebox::Bowlingbox` scores it.

**Architecture:** Three layers on top of the existing `Core` and `Gamebox` libraries. `Sandbox::Alley` owns the Bullet physics world and the throw state machine and bridges to `Bowlingbox`. `Sandbox::Render` draws the scene and the scoreboard with OpenGL 3.3 core (GLAD, GLM, stb_easy_font). The `BowlingGL` wxWidgets application is glue: `wxGLCanvas`, input mapping, 60 Hz timer.

**Tech Stack:** C++20, CMake ≥ 3.29, wxWidgets 3.2.8 (FetchContent), Bullet3 3.25 (FetchContent), GLM 1.0.1 (FetchContent), GLAD 2 (in-tree, generated), stb_easy_font (in-tree), GoogleTest (existing).

Spec: `docs/superpowers/specs/2026-10-13-bowling-gl-game-design.md`.

## Global Constraints

- Language C++20; `CMAKE_CXX_STANDARD 20`, extensions off (already set in `cmake/Common.cmake`).
- Qt must not be required for the new application; `BUILD_QT_UI` defaults to OFF but the Qt app stays in the tree.
- Project code compiles with `-Wall -Wextra -Wpedantic -Werror` (`sandbox_set_warnings`). All third-party headers are consumed as SYSTEM includes.
- Every library operation that can fail returns `Result<T, Sc::Error<>>` from `Sandbox/Core/Core.h`; no exception crosses a library boundary.
- `Alley` has no wx/OpenGL dependency. `Render` has no Bullet/bowling-rule dependency. Only the application links `OpenGL::GL` and wx.
- Units: metres, kilograms, seconds. Axes: x lateral, y up, z along the lane towards the pins. Foul line at z = 0, ball starts at z = −1.
- Physics constants (from spec): lane half-width 0.525, length 18.3, approach 2.0, gutter width 0.24 and depth 0.05; ball r = 0.109, 7 kg; pins 0.38 m tall, max r 0.06, 1.5 kg, COM 0.12 m above base, spacing 0.3048, head pin z = 17.4; gravity −9.81; fixed step 1/120 with `stepSimulation(dt, 4, 1/120)`.
- Phase timings: power oscillates 0→1→0 at 1 Hz; launch speed `4 + 8·power` m/s; settling timeout 3 s; FrameDone shown 1.5 s; aim offset clamped to ±0.7 m (ball fully over the gutter: 0.7 − 0.109 > 0.525), aim angle to ±6°.
- Scoreboard encoding identical to console/Qt: −4/−3 blank, −2 `X`, −1 `/`, 0 `-` (but `0` in the score row), otherwise the number.
- Test executables are named `test_<lowercase>` (enforced by `build_gtest_executable`).
- Build directory is `build/` in the source tree (`cmake/BuildLocation.cmake` hard-codes output under `${CMAKE_SOURCE_DIR}/build/`). Configure with `cmake -S . -B build`.
- Commit after every task with the message given in the task.
- Known Gamebox quirk, deliberately preserved: after a strike in frame 10, `pinsLeft()` reports 10 after the first bonus ball, so `Alley` resets the full rack for the last bonus ball. Do not change Gamebox scoring.

## Deviations from the spec (approved assumptions)

1. `Snapshot` gains `std::array<bool,10> pinInPlay` (pin still physically on the deck or in the pit, standing or lying). Fallen pins stay in play until the phase returns to Aiming, which is what the renderer uses for visibility.
2. `Lane` exposes `rolls()`, `frameScores()`, `totalScore()`, `isGameOver()`, `pinsLeft()` instead of `const Bowlingbox&` because `Bowlingbox` has no const methods.
3. The camera is still "behind the ball" but its z follows the ball during Rolling/Settling/FrameDone (with smoothing) so the pins are visible when they fall; otherwise pins 20 m away are a few pixels tall. A single constant `kFollowCamera` in `BowlingCanvas.cpp` switches it off.
4. Render splits CPU geometry (`Geometry.h`, testable) from GL upload (`GlMesh`, private), and the scoreboard maths (`ScoreboardLayout.h`, testable) from GL drawing (`Overlay`).
5. `kMaxAimOffset` is 0.7 m instead of 0.6 m so that a max-offset ball never touches the lane edge or pin 10 (deterministic gutter test). The gutter outer wall sits at x = ±0.845 to leave room.

## File structure

```
3rdparty/
  CMakeLists.txt                     glad::glad, stb::easy_font targets
  glad/include/glad/gl.h, include/KHR/khrplatform.h, src/gl.c   (generated)
  stb/stb_easy_font.h                (downloaded)
cmake/
  BulletSupport.cmake  GlmSupport.cmake  WxSupport.cmake        (new)
  BuildUtils.cmake                   WX_ui branch in create_application
  Common.cmake                       include new support files
  modules/LibraryGenerator.cmake     wx template branch, skip existing app dirs
  template/app/lib_wx_ui_CMakeLists.txt.in, lib_Wx_UI.cpp.in    (new)
libraries/
  Gamebox/…                          roll() returns bool
  Alley/CMakeLists.txt
  Alley/include/Sandbox/Alley/Alley.h          public API
  Alley/src/World.h, World.cpp                 Bullet world
  Alley/src/Lane_impl.h, Lane_impl.cpp         phase machine + Gamebox bridge
  Alley/src/Alley.cpp                          pimpl forwarding
  Alley/test/src/test_Alley.cpp
  Render/CMakeLists.txt
  Render/include/Sandbox/Render/Geometry.h     MeshData + generators (no GL)
  Render/include/Sandbox/Render/ScoreboardLayout.h  grid maths + glyphs (no GL)
  Render/include/Sandbox/Render/Render.h       Context, Camera, SceneRenderer, Overlay
  Render/src/Geometry.cpp, ScoreboardLayout.cpp
  Render/src/GlShader.h/.cpp, GlMesh.h/.cpp, ContextData.h, Render.cpp
  Render/test/src/test_Render.cpp
applications/bowlingGame/
  CMakeLists.txt                     add_subdirectory(bowlingGameGL)
  README.md                          GL section
  bowlingGameGL/CMakeLists.txt
  bowlingGameGL/main.cpp
  bowlingGameGL/BowlingFrame.h/.cpp
  bowlingGameGL/BowlingCanvas.h/.cpp
  bowlingGameGL/InputMapper.h/.cpp
  bowlingGameGL/SceneAdapter.h/.cpp
  bowlingGameGL/test/test_InputMapper.cpp
```

---

### Task 1: `Bowlingbox::roll` reports acceptance

**Files:**
- Modify: `libraries/Gamebox/include/Sandbox/Gamebox/Gamebox.h:16`
- Modify: `libraries/Gamebox/src/Gamebox.cpp:25-28`
- Modify: `libraries/Gamebox/CMakeLists.txt:4` (version)
- Test: `libraries/Gamebox/test/src/test_Gamebox.cpp`

**Interfaces:**
- Produces: `bool Sandbox::Gamebox::Bowlingbox::roll(const int& pins)` — `true` when the roll was recorded, `false` when the game is already over.

- [ ] **Step 1: Write the failing tests**

Append to `libraries/Gamebox/test/src/test_Gamebox.cpp`:

```cpp
TEST_F(test_gamebox, roll_returns_true_while_playing) {
    EXPECT_TRUE(bowlingGame->roll(3));
    EXPECT_TRUE(bowlingGame->roll(4));
}

TEST_F(test_gamebox, roll_returns_false_when_game_is_over) {
    roll_n_times(20, 0);
    EXPECT_FALSE(bowlingGame->roll(1));
}
```

- [ ] **Step 2: Run to verify they fail**

Run: `cmake --build build -j8 2>&1 | tail -5`
Expected: compile error `could not convert 'bowlingGame->...roll(3)' from 'void' to 'bool'`.

- [ ] **Step 3: Change the signature and forward the bool**

`libraries/Gamebox/include/Sandbox/Gamebox/Gamebox.h` line 16:

```cpp
    bool SANDBOX_GAMEBOX_API roll(const int&);
```

`libraries/Gamebox/src/Gamebox.cpp` lines 25–28:

```cpp
bool Sandbox::Gamebox::Bowlingbox::roll(const int& pins) {
    // Pass the number of pins rolled to the BowlingSet object to update the game state
    return _gameSet->roll(pins);
}
```

`libraries/Gamebox/CMakeLists.txt` line 4: `project(Gamebox VERSION 1.2.0 LANGUAGES CXX)`

- [ ] **Step 4: Run tests**

Run: `cmake --build build -j8 && ctest --test-dir build -R gamebox --output-on-failure | tail -3`
Expected: `100% tests passed, 0 tests failed out of 16`.

- [ ] **Step 5: Commit**

```bash
git add libraries/Gamebox
git commit -m "feat(gamebox): roll() returns whether the roll was accepted"
```

---

### Task 2: Third-party dependencies (Bullet, GLM, GLAD, stb)

**Files:**
- Create: `cmake/BulletSupport.cmake`, `cmake/GlmSupport.cmake`, `3rdparty/CMakeLists.txt`, `3rdparty/README.md`
- Generate: `3rdparty/glad/include/glad/gl.h`, `3rdparty/glad/include/KHR/khrplatform.h`, `3rdparty/glad/src/gl.c`
- Download: `3rdparty/stb/stb_easy_font.h`
- Modify: `cmake/Common.cmake`, `CMakeLists.txt`

**Interfaces:**
- Produces CMake targets: `Bullet::Bullet` (BulletDynamics+BulletCollision+LinearMath, include `<btBulletDynamicsCommon.h>`), `glm::glm` (header-only), `glad::glad` (static, include `<glad/gl.h>`, provides `gladLoaderLoadGL()`), `stb::easy_font` (include `<stb_easy_font.h>`).

- [ ] **Step 1: Create `cmake/BulletSupport.cmake`**

```cmake
# Bullet3 rigid-body physics, fetched and built statically with -fPIC so it
# can be linked into the shared Sandbox::Alley library.
include(FetchContent)

set(SANDBOX_BULLET_VERSION "3.25" CACHE STRING "Bullet3 release tag")

FetchContent_Declare(
    bullet3
    URL https://github.com/bulletphysics/bullet3/archive/refs/tags/${SANDBOX_BULLET_VERSION}.tar.gz
    SYSTEM
)

# Bullet's option() calls must not override these (CMP0077 NEW is set in Common.cmake).
# BUILD_SHARED_LIBS is restored afterwards so Sandbox libraries stay shared.
set(_sandbox_saved_shared ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
foreach(opt
        BUILD_BULLET2_DEMOS BUILD_CPU_DEMOS BUILD_OPENGL3_DEMOS BUILD_EXTRAS
        BUILD_UNIT_TESTS BUILD_BULLET3 BUILD_CLSOCKET BUILD_ENET BUILD_EGL
        BUILD_PYBULLET USE_GRAPHICAL_BENCHMARK USE_GLUT INSTALL_LIBS INSTALL_CMAKE_FILES)
    set(${opt} OFF)
endforeach()

FetchContent_MakeAvailable(bullet3)
set(BUILD_SHARED_LIBS ${_sandbox_saved_shared})

add_library(sandbox_bullet INTERFACE)
target_link_libraries(sandbox_bullet INTERFACE BulletDynamics BulletCollision LinearMath)
target_include_directories(sandbox_bullet SYSTEM INTERFACE ${bullet3_SOURCE_DIR}/src)
add_library(Bullet::Bullet ALIAS sandbox_bullet)
```

- [ ] **Step 2: Create `cmake/GlmSupport.cmake`**

```cmake
# GLM maths, header-only.
include(FetchContent)

set(SANDBOX_GLM_VERSION "1.0.1" CACHE STRING "GLM release tag")

FetchContent_Declare(
    glm
    URL https://github.com/g-truc/glm/archive/refs/tags/${SANDBOX_GLM_VERSION}.tar.gz
    SYSTEM
)
set(GLM_BUILD_LIBRARY OFF)
set(GLM_BUILD_TESTS OFF)
set(GLM_BUILD_INSTALL OFF)
set(GLM_ENABLE_CXX_20 ON)
FetchContent_MakeAvailable(glm)
```

- [ ] **Step 3: Generate GLAD and download stb_easy_font**

```bash
cd /home/amin/Dokumente/dev/Gamebox
python3 -m venv /tmp/gladenv && /tmp/gladenv/bin/pip install -q glad2
/tmp/gladenv/bin/python -m glad --api='gl:core=3.3' --extensions='' --out-path=3rdparty/glad c --loader
mkdir -p 3rdparty/stb
curl -sL -o 3rdparty/stb/stb_easy_font.h https://raw.githubusercontent.com/nothings/stb/master/stb_easy_font.h
ls 3rdparty/glad/include/glad/gl.h 3rdparty/glad/include/KHR/khrplatform.h 3rdparty/glad/src/gl.c 3rdparty/stb/stb_easy_font.h
grep -c gladLoaderLoadGL 3rdparty/glad/src/gl.c
```
Expected: the four files listed; grep count ≥ 1.

- [ ] **Step 4: Create `3rdparty/CMakeLists.txt` and `3rdparty/README.md`**

`3rdparty/CMakeLists.txt`:

```cmake
# In-tree third-party code (no network needed).
enable_language(C)

add_library(sandbox_glad STATIC glad/src/gl.c)
target_include_directories(sandbox_glad SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/glad/include)
target_link_libraries(sandbox_glad PUBLIC ${CMAKE_DL_LIBS})
set_target_properties(sandbox_glad PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(glad::glad ALIAS sandbox_glad)

add_library(sandbox_stb INTERFACE)
target_include_directories(sandbox_stb SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/stb)
add_library(stb::easy_font ALIAS sandbox_stb)
```

`3rdparty/README.md`:

```markdown
# In-tree third-party code

| Directory | Project | Licence | How to regenerate |
|---|---|---|---|
| `glad/` | GLAD 2 loader, OpenGL 3.3 core, with `gladLoaderLoadGL()` | MIT / Apache-2.0 (generator), public domain (output) | `python -m glad --api='gl:core=3.3' --extensions='' --out-path=3rdparty/glad c --loader` |
| `stb/` | `stb_easy_font.h` from https://github.com/nothings/stb | public domain / MIT | download the header |

wxWidgets, Bullet3, GLM and GoogleTest are fetched at configure time (see `cmake/*Support.cmake`).
```

- [ ] **Step 5: Wire into `cmake/Common.cmake` and the root `CMakeLists.txt`**

In `cmake/Common.cmake`, after `include(Coverage)` (line 13) add:

```cmake
# Fetched projects must honour our normal variables in their option() calls
# (CMP0077) and must not drop them when creating cache entries (CMP0126).
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0126 NEW)
```

and replace the block

```cmake
if(BUILD_QT_UI)
    include(QtSupport)
endif()
```

with

```cmake
include(BulletSupport)   # Sandbox::Alley always needs it
include(GlmSupport)      # Sandbox::Render always needs it
if(BUILD_QT_UI)
    include(QtSupport)
endif()
```

In the root `CMakeLists.txt`, before `if(EXISTS "${libraries_dir}/CMakeLists.txt")` add:

```cmake
add_subdirectory(${CMAKE_SOURCE_DIR}/3rdparty)
```

- [ ] **Step 6: Configure and build the dependencies**

Run: `cmake -S . -B build 2>&1 | tail -5 && cmake --build build -j8 --target BulletDynamics sandbox_glad 2>&1 | tail -3`
Expected: `Configuring done`, `Generating done`, then `[100%] Built target BulletDynamics` and `sandbox_glad`.

If configure fails with "Compatibility with CMake < 3.5 has been removed", add `set(CMAKE_POLICY_VERSION_MINIMUM 3.5)` above `FetchContent_MakeAvailable(bullet3)` in `BulletSupport.cmake` and reconfigure.

- [ ] **Step 7: Run the existing tests to confirm nothing regressed**

Run: `cmake --build build -j8 && ctest --test-dir build | tail -3`
Expected: `100% tests passed`.

- [ ] **Step 8: Commit**

```bash
git add cmake/BulletSupport.cmake cmake/GlmSupport.cmake cmake/Common.cmake CMakeLists.txt 3rdparty
git commit -m "build: add Bullet3, GLM (FetchContent) and in-tree GLAD/stb_easy_font"
```

---

### Task 3: `Sandbox::Alley` — public API, phase machine, Bullet world

**Files:**
- Create: `libraries/Alley/CMakeLists.txt`
- Create: `libraries/Alley/include/Sandbox/Alley/Alley.h`
- Create: `libraries/Alley/src/World.h`, `libraries/Alley/src/World.cpp`
- Create: `libraries/Alley/src/Lane_impl.h`, `libraries/Alley/src/Lane_impl.cpp`
- Create: `libraries/Alley/src/Alley.cpp`
- Create: `libraries/Alley/test/src/test_Alley.cpp`
- Modify: `libraries/CMakeLists.txt:6`

**Interfaces:**
- Consumes: `bool Bowlingbox::roll(const int&)` (Task 1), `Bullet::Bullet` (Task 2), `Result<>`/`Sc::Error<>` from Core.
- Produces (used by Tasks 4, 8):
  - `enum class Sandbox::Alley::Phase { Aiming, Charging, Rolling, Settling, FrameDone, GameOver }`
  - `struct LaneDimensions` + `inline constexpr LaneDimensions kLane`, `kMaxAimOffset = 0.6f`, `kMaxAimAngle = 6.f`
  - `struct Pose { std::array<float,3> position; std::array<float,4> rotation /*x,y,z,w*/; }`
  - `struct Snapshot { Phase phase; Pose ball; std::array<Pose,10> pins; std::array<bool,10> pinStanding, pinInPlay; float aimOffset, aimAngle, power; int lastPinsDown; }`
  - `class Lane` with `Result<> moveAim(float)`, `Result<> rotateAim(float)`, `Result<> startCharge()`, `Result<> release()`, `Result<> newGame()`, `void step(float) noexcept`, `Snapshot snapshot() const`, `std::vector<int> rolls() const`, `std::vector<int> frameScores() const`, `int totalScore() const`, `bool isGameOver() const`, `int pinsLeft() const`.

- [ ] **Step 1: Create the library CMake file and register it**

`libraries/Alley/CMakeLists.txt`:

```cmake
project(Alley VERSION 1.0.0 LANGUAGES CXX)

set(TARGET_NAME ${PROJECT_NAME})
set(TST_DISCOVER ON)
###############################################################
# Manage project files
###############################################################
set(TARGET_HEADERS
    src/World.h
    src/Lane_impl.h
    include/Sandbox/Alley/Alley.h
)
set(TARGET_SOURCES
    src/World.cpp
    src/Lane_impl.cpp
    src/Alley.cpp
)
set(TARGET_RSC)
# Bullet is an implementation detail; BUILD_INTERFACE keeps it out of the
# installed export set.
set(PRIVATE_DEPS
    $<BUILD_INTERFACE:Bullet::Bullet>
)
set(PUBLIC_DEPS
    ${CMAKE_ROOT_NAME}::Core
    ${CMAKE_ROOT_NAME}::Gamebox
)
set(TARGET_FILES ${TARGET_HEADERS} ${TARGET_SOURCES})
###############################################################
# Manage testing files
###############################################################
set(TST_SOURCES
    test/src/test_Alley.cpp
)
set(TEST_DEPS)
###############################################################
# build
###############################################################
create_library(
    LIB_NAME             ${TARGET_NAME}
    LIB_FILES            ${TARGET_FILES}
    PRIVATE_DEPENDENCIES ${PRIVATE_DEPS}
    PUBLIC_DEPENDENCIES  ${PUBLIC_DEPS}
    TEST_DISCOVER        ${TST_DISCOVER}
    TEST_SOURCES         ${TST_SOURCES}
    TEST_DEPENDENCIES    ${TEST_DEPS}
    LIB_RSC              ${TARGET_RSC}
)
```

`libraries/CMakeLists.txt` line 6: `set(_sandbox_library_dirs Gamebox Core Alley Render Pot Tools Geo Codility)` (Render is created in Task 5; it is skipped until then).

- [ ] **Step 2: Write the public header `libraries/Alley/include/Sandbox/Alley/Alley.h`**

```cpp
#ifndef SANDBOX_ALLEY_H
#define SANDBOX_ALLEY_H
#include <Sandbox/Alley/Sandbox_Alley_export.h>
#include <Sandbox/Core/Core.h>
#include <array>
#include <memory>
#include <vector>

namespace Sandbox {
namespace Alley {

/// Throw state machine phases (see spec §3.2).
enum class Phase { Aiming, Charging, Rolling, Settling, FrameDone, GameOver };

/// Physical dimensions shared with the renderer (metres).
struct LaneDimensions {
    float halfWidth    = 0.525f;
    float length       = 18.3f;   // foul line (z = 0) to end of pin deck
    float approach     = 2.0f;    // walkable area before the foul line (negative z)
    float gutterWidth  = 0.24f;
    float gutterDepth  = 0.05f;
    float ballRadius   = 0.109f;
    float pinHeight    = 0.38f;
    float pinMaxRadius = 0.06f;
    float pinComHeight = 0.12f;   // rigid-body origin above the pin base
    float pinSpacing   = 0.3048f;
    float headPinZ     = 17.4f;
};
inline constexpr LaneDimensions kLane {};
inline constexpr float kMaxAimOffset = 0.7f;  // ball fully over the gutter (0.7 - r > halfWidth)
inline constexpr float kMaxAimAngle = 6.0f;   // degrees
inline constexpr float kBallStartZ = -1.0f;

struct Pose {
    std::array<float, 3> position {0.f, 0.f, 0.f};
    std::array<float, 4> rotation {0.f, 0.f, 0.f, 1.f};  // quaternion x, y, z, w
};

struct Snapshot {
    Phase phase = Phase::Aiming;
    Pose ball;
    std::array<Pose, 10> pins;
    std::array<bool, 10> pinStanding {};  // upright on the deck right now
    std::array<bool, 10> pinInPlay {};    // still part of the world (standing or lying)
    float aimOffset = 0.f;                // lateral ball position, m, [-kMaxAimOffset, +]
    float aimAngle = 0.f;                 // degrees, [-kMaxAimAngle, +]
    float power = 0.f;                    // [0, 1]
    int lastPinsDown = -1;                // pins credited for the last throw, -1 if none yet
};

class LaneImpl;

class SANDBOX_ALLEY_API Lane {
public:
    Lane();
    ~Lane();
    Lane(const Lane&) = delete;
    Lane& operator=(const Lane&) = delete;

    // Input. Valid only in Aiming / Charging (newGame: any phase); otherwise Error.
    Result<> moveAim(float deltaOffset);
    Result<> rotateAim(float deltaAngleDeg);
    Result<> startCharge();  // Aiming  -> Charging
    Result<> release();      // Charging -> Rolling
    Result<> newGame();      // any      -> Aiming

    // Time.
    void step(float dtSeconds) noexcept;

    // Read.
    Snapshot snapshot() const;
    std::vector<int> rolls() const;        // 21-slot Gamebox encoding
    std::vector<int> frameScores() const;  // 10 running scores, -3 when not yet known
    int totalScore() const;
    bool isGameOver() const;
    int pinsLeft() const;

private:
    std::unique_ptr<LaneImpl> _impl;
};

}  // namespace Alley
}  // namespace Sandbox
#endif  // SANDBOX_ALLEY_H
```

- [ ] **Step 3: Write the failing phase-machine tests `libraries/Alley/test/src/test_Alley.cpp`**

```cpp
#include <Sandbox/Alley/Alley.h>
#include <gtest/gtest.h>
#include <cmath>

using namespace Sandbox::Alley;

namespace {
constexpr float kPi = 3.14159265358979f;
constexpr float kDt = 1.f / 60.f;

/// Time at which the 1 Hz charge curve power(t) = (1 - cos 2πt)/2 reaches `power`.
float chargeTimeFor(float power) {
    return std::acos(1.f - 2.f * power) / (2.f * kPi);
}

int standingCount(const Snapshot& s) {
    int n = 0;
    for (bool b : s.pinStanding)
        n += b ? 1 : 0;
    return n;
}

/// Full throw: aim, charge to `power`, release, step until the ball is scored
/// and the lane is back in Aiming (or GameOver).
void throwBall(Lane& lane, float offset, float angleDeg, float power) {
    const auto s = lane.snapshot();
    lane.moveAim(offset - s.aimOffset);
    lane.rotateAim(angleDeg - s.aimAngle);
    lane.startCharge();
    lane.step(chargeTimeFor(power));
    lane.release();
    for (int i = 0; i < 60 * 20; ++i) {
        const auto phase = lane.snapshot().phase;
        if (phase == Phase::Aiming || phase == Phase::GameOver)
            return;
        lane.step(kDt);
    }
}
}  // namespace

class test_alley : public ::testing::Test {
public:
    Lane lane;
};

TEST_F(test_alley, initial_state_is_aiming_with_full_rack) {
    const auto s = lane.snapshot();
    EXPECT_EQ(s.phase, Phase::Aiming);
    EXPECT_EQ(standingCount(s), 10);
    EXPECT_EQ(s.lastPinsDown, -1);
    EXPECT_FLOAT_EQ(s.aimOffset, 0.f);
    EXPECT_EQ(lane.totalScore(), 0);
    EXPECT_FALSE(lane.isGameOver());
}

TEST_F(test_alley, release_in_aiming_is_an_error) {
    auto r = lane.release();
    EXPECT_TRUE(r.isError());
    EXPECT_STREQ(r.error().c_str(), "not charging");
}

TEST_F(test_alley, start_charge_twice_is_an_error) {
    EXPECT_TRUE(lane.startCharge().isValue());
    auto r = lane.startCharge();
    EXPECT_TRUE(r.isError());
    EXPECT_STREQ(r.error().c_str(), "not aiming");
}

TEST_F(test_alley, move_aim_clamps_to_range) {
    EXPECT_TRUE(lane.moveAim(5.f).isValue());
    EXPECT_FLOAT_EQ(lane.snapshot().aimOffset, kMaxAimOffset);
    EXPECT_FLOAT_EQ(lane.snapshot().ball.position[0], kMaxAimOffset);
    EXPECT_TRUE(lane.rotateAim(-90.f).isValue());
    EXPECT_FLOAT_EQ(lane.snapshot().aimAngle, -kMaxAimAngle);
}

TEST_F(test_alley, aim_while_rolling_is_an_error) {
    lane.startCharge();
    lane.step(0.25f);
    lane.release();
    EXPECT_EQ(lane.snapshot().phase, Phase::Rolling);
    auto r = lane.moveAim(0.1f);
    EXPECT_TRUE(r.isError());
    EXPECT_STREQ(r.error().c_str(), "ball in motion");
}

TEST_F(test_alley, charging_power_follows_one_hertz_cosine) {
    lane.startCharge();
    lane.step(0.25f);
    EXPECT_NEAR(lane.snapshot().power, 0.5f, 1e-4f);
    lane.step(0.25f);
    EXPECT_NEAR(lane.snapshot().power, 1.0f, 1e-4f);
    lane.step(0.5f);
    EXPECT_NEAR(lane.snapshot().power, 0.0f, 1e-4f);
}

TEST_F(test_alley, new_game_resets_everything) {
    lane.moveAim(0.3f);
    lane.startCharge();
    EXPECT_TRUE(lane.newGame().isValue());
    const auto s = lane.snapshot();
    EXPECT_EQ(s.phase, Phase::Aiming);
    EXPECT_FLOAT_EQ(s.aimOffset, 0.f);
    EXPECT_EQ(standingCount(s), 10);
    EXPECT_EQ(lane.totalScore(), 0);
}
```

- [ ] **Step 4: Configure to verify the tests fail**

Run: `cmake -S . -B build 2>&1 | tail -3 && cmake --build build -j8 --target test_sandbox_alley 2>&1 | grep -E 'error|Error' | head -3`
Expected: errors about missing `World.h` / `Lane_impl.h` / `Alley.cpp` sources (files do not exist yet).

- [ ] **Step 5: Write `libraries/Alley/src/World.h`**

```cpp
#ifndef SANDBOX_ALLEY_WORLD_H
#define SANDBOX_ALLEY_WORLD_H
#include <Sandbox/Alley/Alley.h>
#include <array>
#include <memory>
#include <utility>

namespace Sandbox {
namespace Alley {

/// Bullet rigid-body world: static lane, gutters and pit; one ball; ten pins.
/// Knows nothing about bowling rules or phases.
class World {
public:
    World();
    ~World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    void step(float dt);  // stepSimulation(dt, 4, 1/120)

    void placeBall(float offset);  // at rest on the approach, z = kBallStartZ
    void launchBall(float offset, float angleDeg, float speed);

    void resetRack();     // all 10 pins standing and in play
    void removePin(int i);  // take a pin out of the world

    Pose ballPose() const;
    Pose pinPose(int i) const;
    bool isPinStanding(int i) const;
    bool isPinInPlay(int i) const;
    bool ballInPit() const;
    float ballSpeed() const;
    bool allSlow() const;  // every body in the world is (nearly) at rest

    /// Standard rack: (x, z) of pins 1..10 in metres.
    static std::array<std::pair<float, float>, 10> pinPositions();

private:
    struct Impl;
    std::unique_ptr<Impl> _p;
};

}  // namespace Alley
}  // namespace Sandbox
#endif  // SANDBOX_ALLEY_WORLD_H
```

- [ ] **Step 6: Write `libraries/Alley/src/World.cpp`**

```cpp
#include "World.h"
#include <btBulletDynamicsCommon.h>
#include <cmath>
#include <vector>

namespace Sandbox {
namespace Alley {
namespace {
constexpr float kPi = 3.14159265358979f;
constexpr float kPitDepth = 0.8f;
constexpr float kPitLength = 2.4f;
constexpr float kBallMass = 7.0f;
constexpr float kPinMass = 1.5f;
constexpr float kSlowLinear = 0.05f;   // m/s
constexpr float kSlowAngular = 0.2f;   // rad/s
constexpr float kStandingCos = 0.7071f;  // cos 45°

Pose toPose(const btTransform& t) {
    Pose p;
    const btVector3& o = t.getOrigin();
    p.position = {o.x(), o.y(), o.z()};
    const btQuaternion q = t.getRotation();
    p.rotation = {q.x(), q.y(), q.z(), q.w()};
    return p;
}

btTransform at(float x, float y, float z) {
    btTransform t;
    t.setIdentity();
    t.setOrigin(btVector3(x, y, z));
    return t;
}
}  // namespace

struct World::Impl {
    btDefaultCollisionConfiguration config;
    btCollisionDispatcher dispatcher {&config};
    btDbvtBroadphase broadphase;
    btSequentialImpulseConstraintSolver solver;
    btDiscreteDynamicsWorld world {&dispatcher, &broadphase, &solver, &config};

    std::vector<std::unique_ptr<btCollisionShape>> shapes;
    std::vector<std::unique_ptr<btDefaultMotionState>> motions;
    std::vector<std::unique_ptr<btRigidBody>> statics;
    std::unique_ptr<btRigidBody> ball;
    std::array<std::unique_ptr<btRigidBody>, 10> pins;
    std::array<bool, 10> inPlay {};

    Impl() { build(); }
    ~Impl() {
        // Bodies must leave the world before they are destroyed.
        for (int i = 0; i < 10; ++i)
            if (inPlay[i])
                world.removeRigidBody(pins[i].get());
        world.removeRigidBody(ball.get());
        for (auto& b : statics)
            world.removeRigidBody(b.get());
    }

    btCollisionShape* keep(std::unique_ptr<btCollisionShape> s) {
        shapes.push_back(std::move(s));
        return shapes.back().get();
    }

    void addStatic(btCollisionShape* shape, const btTransform& t, float friction, float rolling,
                   float restitution) {
        motions.push_back(std::make_unique<btDefaultMotionState>(t));
        btRigidBody::btRigidBodyConstructionInfo info(0.f, motions.back().get(), shape);
        info.m_friction = friction;
        info.m_rollingFriction = rolling;
        info.m_restitution = restitution;
        statics.push_back(std::make_unique<btRigidBody>(info));
        world.addRigidBody(statics.back().get());
    }

    std::unique_ptr<btRigidBody> makeDynamic(btCollisionShape* shape, float mass,
                                             const btTransform& t, float friction,
                                             float rolling, float restitution) {
        btVector3 inertia(0, 0, 0);
        shape->calculateLocalInertia(mass, inertia);
        motions.push_back(std::make_unique<btDefaultMotionState>(t));
        btRigidBody::btRigidBodyConstructionInfo info(mass, motions.back().get(), shape, inertia);
        info.m_friction = friction;
        info.m_rollingFriction = rolling;
        info.m_restitution = restitution;
        auto body = std::make_unique<btRigidBody>(info);
        body->setSleepingThresholds(0.05f, 0.1f);
        return body;
    }

    void build() {
        world.setGravity(btVector3(0, -9.81f, 0));
        const LaneDimensions& d = kLane;
        const float outerX = d.halfWidth + d.gutterWidth;

        // Bullet multiplies the friction/restitution of both bodies, so the
        // static surfaces use 1.0 and the dynamic bodies carry the real value.
        // Lane: approach + lane, top surface at y = 0.
        const float laneLen = d.length + d.approach;
        addStatic(keep(std::make_unique<btBoxShape>(btVector3(d.halfWidth, 0.05f, laneLen / 2))),
                  at(0.f, -0.05f, (d.length - d.approach) / 2), 1.f, 1.f, 1.f);
        // Gutters, gutterDepth below the lane.
        auto* gutter = keep(std::make_unique<btBoxShape>(
            btVector3(d.gutterWidth / 2, 0.05f, d.length / 2)));
        for (float sx : {-1.f, 1.f})
            addStatic(gutter, at(sx * (d.halfWidth + d.gutterWidth / 2), -d.gutterDepth - 0.05f,
                                 d.length / 2), 1.f, 1.f, 1.f);
        // Gutter outer walls so the ball cannot leave the alley sideways. Inner
        // face at outerX + 0.06 = 0.825 > kMaxAimOffset + ballRadius (0.809).
        auto* wall = keep(std::make_unique<btBoxShape>(btVector3(0.02f, 0.15f, d.length / 2)));
        for (float sx : {-1.f, 1.f})
            addStatic(wall, at(sx * (outerX + 0.08f), 0.05f, d.length / 2), 1.f, 1.f, 1.f);
        // Pit behind the deck: floor, back wall, side walls.
        const float pitZ = d.length + kPitLength / 2;
        addStatic(keep(std::make_unique<btBoxShape>(btVector3(outerX + 0.12f, 0.05f, kPitLength / 2))),
                  at(0.f, -kPitDepth - 0.05f, pitZ), 1.f, 1.f, 0.2f);
        addStatic(keep(std::make_unique<btBoxShape>(btVector3(outerX + 0.12f, 0.6f, 0.05f))),
                  at(0.f, -0.3f, d.length + kPitLength + 0.05f), 1.f, 1.f, 0.2f);
        auto* pitSide = keep(std::make_unique<btBoxShape>(btVector3(0.05f, 0.6f, kPitLength / 2)));
        for (float sx : {-1.f, 1.f})
            addStatic(pitSide, at(sx * (outerX + 0.17f), -0.3f, pitZ), 1.f, 1.f, 0.2f);

        // Ball.
        auto* ballShape = keep(std::make_unique<btSphereShape>(d.ballRadius));
        ball = makeDynamic(ballShape, kBallMass, at(0.f, d.ballRadius, kBallStartZ), 0.1f, 0.005f, 0.1f);
        ball->setCcdMotionThreshold(0.05f);
        ball->setCcdSweptSphereRadius(d.ballRadius * 0.8f);
        world.addRigidBody(ball.get());

        // Pins: two stacked cylinders, origin pinComHeight above the base.
        auto pinShape = std::make_unique<btCompoundShape>();
        auto* lower = keep(std::make_unique<btCylinderShape>(btVector3(d.pinMaxRadius, 0.10f, d.pinMaxRadius)));
        auto* upper = keep(std::make_unique<btCylinderShape>(btVector3(0.035f, 0.09f, 0.035f)));
        pinShape->addChildShape(at(0.f, 0.10f - d.pinComHeight, 0.f), lower);
        pinShape->addChildShape(at(0.f, 0.29f - d.pinComHeight, 0.f), upper);
        auto* pinShapePtr = keep(std::move(pinShape));
        const auto positions = World::pinPositions();
        for (int i = 0; i < 10; ++i) {
            pins[i] = makeDynamic(pinShapePtr, kPinMass,
                                  at(positions[i].first, d.pinComHeight, positions[i].second),
                                  0.5f, 0.05f, 0.3f);
            pins[i]->setCcdMotionThreshold(0.02f);
            pins[i]->setCcdSweptSphereRadius(0.03f);
            world.addRigidBody(pins[i].get());
            inPlay[i] = true;
        }
    }

    static void teleport(btRigidBody& body, const btTransform& t) {
        body.setWorldTransform(t);
        body.getMotionState()->setWorldTransform(t);
        body.setLinearVelocity(btVector3(0, 0, 0));
        body.setAngularVelocity(btVector3(0, 0, 0));
        body.clearForces();
        body.activate(true);
    }

    static bool slow(const btRigidBody& b) {
        return !b.isActive() || (b.getLinearVelocity().length() < kSlowLinear &&
                                 b.getAngularVelocity().length() < kSlowAngular);
    }
};

World::World() : _p(std::make_unique<Impl>()) {}
World::~World() = default;

void World::step(float dt) {
    _p->world.stepSimulation(dt, 4, 1.f / 120.f);
}

void World::placeBall(float offset) {
    Impl::teleport(*_p->ball, at(offset, kLane.ballRadius, kBallStartZ));
}

void World::launchBall(float offset, float angleDeg, float speed) {
    placeBall(offset);
    const float a = angleDeg * kPi / 180.f;
    const btVector3 dir(std::sin(a), 0.f, std::cos(a));
    const btVector3 up(0.f, 1.f, 0.f);
    _p->ball->setLinearVelocity(dir * speed);
    // Rolling without slipping: omega = (v / r) * (dir x up).
    _p->ball->setAngularVelocity(dir.cross(up) * (speed / kLane.ballRadius));
    _p->ball->activate(true);
}

void World::resetRack() {
    const auto positions = pinPositions();
    for (int i = 0; i < 10; ++i) {
        if (!_p->inPlay[i]) {
            _p->world.addRigidBody(_p->pins[i].get());
            _p->inPlay[i] = true;
        }
        Impl::teleport(*_p->pins[i], at(positions[i].first, kLane.pinComHeight, positions[i].second));
    }
}

void World::removePin(int i) {
    if (i < 0 || i >= 10 || !_p->inPlay[i])
        return;
    _p->world.removeRigidBody(_p->pins[i].get());
    _p->inPlay[i] = false;
}

Pose World::ballPose() const {
    return toPose(_p->ball->getWorldTransform());
}

Pose World::pinPose(int i) const {
    return toPose(_p->pins[i]->getWorldTransform());
}

bool World::isPinStanding(int i) const {
    if (!_p->inPlay[i])
        return false;
    const btTransform& t = _p->pins[i]->getWorldTransform();
    const btVector3 up = t.getBasis().getColumn(1);
    const btVector3& o = t.getOrigin();
    return up.y() > kStandingCos && std::abs(o.x()) < kLane.halfWidth + 0.1f &&
           o.z() < kLane.length + 0.05f && o.y() > 0.f;
}

bool World::isPinInPlay(int i) const {
    return _p->inPlay[i];
}

bool World::ballInPit() const {
    return _p->ball->getWorldTransform().getOrigin().y() < -0.3f;
}

float World::ballSpeed() const {
    return _p->ball->getLinearVelocity().length();
}

bool World::allSlow() const {
    if (!Impl::slow(*_p->ball))
        return false;
    for (int i = 0; i < 10; ++i)
        if (_p->inPlay[i] && !Impl::slow(*_p->pins[i]))
            return false;
    return true;
}

std::array<std::pair<float, float>, 10> World::pinPositions() {
    const float s = kLane.pinSpacing;
    const float row = s * 0.8660254f;  // s * cos 30°
    const float z0 = kLane.headPinZ;
    return {{{0.f, z0},
             {-s / 2, z0 + row}, {s / 2, z0 + row},
             {-s, z0 + 2 * row}, {0.f, z0 + 2 * row}, {s, z0 + 2 * row},
             {-1.5f * s, z0 + 3 * row}, {-s / 2, z0 + 3 * row}, {s / 2, z0 + 3 * row}, {1.5f * s, z0 + 3 * row}}};
}

}  // namespace Alley
}  // namespace Sandbox
```

- [ ] **Step 7: Write `libraries/Alley/src/Lane_impl.h`**

```cpp
#ifndef SANDBOX_ALLEY_LANE_IMPL_H
#define SANDBOX_ALLEY_LANE_IMPL_H
#include <Sandbox/Alley/Alley.h>
#include <Sandbox/Gamebox/Gamebox.h>
#include "World.h"
#include <memory>

namespace Sandbox {
namespace Alley {

/// Throw state machine + bridge to the Gamebox score calculator.
class LaneImpl {
public:
    LaneImpl();

    Result<> moveAim(float delta);
    Result<> rotateAim(float delta);
    Result<> startCharge();
    Result<> release();
    Result<> newGame();

    void step(float dt) noexcept;
    Snapshot snapshot() const;
    Gamebox::Bowlingbox& game() const { return *_game; }

private:
    bool canAim() const { return _phase == Phase::Aiming || _phase == Phase::Charging; }
    void enter(Phase p);
    void finishThrow();      // Settling -> FrameDone (or GameOver on rejected roll)
    void prepareNextBall();  // FrameDone -> Aiming

    std::unique_ptr<Gamebox::Bowlingbox> _game;
    World _world;
    Phase _phase = Phase::Aiming;
    float _aimOffset = 0.f;
    float _aimAngle = 0.f;
    float _power = 0.f;
    float _phaseTime = 0.f;
    int _lastPinsDown = -1;
};

}  // namespace Alley
}  // namespace Sandbox
#endif  // SANDBOX_ALLEY_LANE_IMPL_H
```

- [ ] **Step 8: Write `libraries/Alley/src/Lane_impl.cpp`**

```cpp
#include "Lane_impl.h"
#include <algorithm>
#include <cmath>

namespace Sandbox {
namespace Alley {
namespace {
constexpr float kPi = 3.14159265358979f;
constexpr float kChargeHz = 1.0f;
constexpr float kMinSpeed = 4.0f;
constexpr float kSpeedRange = 8.0f;
constexpr float kRollingTimeout = 8.0f;
constexpr float kBallStoppedSpeed = 0.1f;
constexpr float kSettleTimeout = 3.0f;
constexpr float kFrameDoneDuration = 1.5f;

Result<> ok() {
    return Result<>(std::monostate {});
}
Result<> fail(const char* message) {
    return Result<>(Sc::Error<>(std::string(message)));
}
}  // namespace

LaneImpl::LaneImpl() : _game(std::make_unique<Gamebox::Bowlingbox>()) {
    _game->reset();  // Bowlingbox does not initialise its roll array itself
    _world.resetRack();
    _world.placeBall(0.f);
}

Result<> LaneImpl::moveAim(float delta) {
    if (!canAim())
        return fail("ball in motion");
    _aimOffset = std::clamp(_aimOffset + delta, -kMaxAimOffset, kMaxAimOffset);
    _world.placeBall(_aimOffset);
    return ok();
}

Result<> LaneImpl::rotateAim(float delta) {
    if (!canAim())
        return fail("ball in motion");
    _aimAngle = std::clamp(_aimAngle + delta, -kMaxAimAngle, kMaxAimAngle);
    return ok();
}

Result<> LaneImpl::startCharge() {
    if (_phase != Phase::Aiming)
        return fail("not aiming");
    _power = 0.f;
    enter(Phase::Charging);
    return ok();
}

Result<> LaneImpl::release() {
    if (_phase != Phase::Charging)
        return fail("not charging");
    _world.launchBall(_aimOffset, _aimAngle, kMinSpeed + kSpeedRange * _power);
    enter(Phase::Rolling);
    return ok();
}

Result<> LaneImpl::newGame() {
    _game->reset();
    _world.resetRack();
    _aimOffset = 0.f;
    _aimAngle = 0.f;
    _power = 0.f;
    _lastPinsDown = -1;
    _world.placeBall(0.f);
    enter(Phase::Aiming);
    return ok();
}

void LaneImpl::step(float dt) noexcept {
    _phaseTime += dt;
    switch (_phase) {
        case Phase::Charging:
            _power = 0.5f * (1.f - std::cos(2.f * kPi * kChargeHz * _phaseTime));
            break;
        case Phase::Rolling:
            _world.step(dt);
            if (_world.ballInPit() || (_phaseTime > 1.f && _world.ballSpeed() < kBallStoppedSpeed) ||
                _phaseTime > kRollingTimeout)
                enter(Phase::Settling);
            break;
        case Phase::Settling:
            _world.step(dt);
            if (_world.allSlow() || _phaseTime > kSettleTimeout)
                finishThrow();
            break;
        case Phase::FrameDone:
            if (_phaseTime >= kFrameDoneDuration) {
                if (_game->isOver())
                    enter(Phase::GameOver);
                else
                    prepareNextBall();
            }
            break;
        case Phase::Aiming:
        case Phase::GameOver:
            break;
    }
}

void LaneImpl::enter(Phase p) {
    _phase = p;
    _phaseTime = 0.f;
}

void LaneImpl::finishThrow() {
    int down = 0;
    for (int i = 0; i < 10; ++i)
        if (_world.isPinInPlay(i) && !_world.isPinStanding(i))
            ++down;
    if (!_game->roll(down)) {
        // Gamebox refused the roll (only possible when the game is already
        // over, which the FrameDone transition prevents). Fail safe.
        _lastPinsDown = -1;
        enter(Phase::GameOver);
        return;
    }
    _lastPinsDown = down;
    enter(Phase::FrameDone);
}

void LaneImpl::prepareNextBall() {
    if (_game->pinsLeft() == 10) {
        _world.resetRack();
    } else {
        for (int i = 0; i < 10; ++i)
            if (_world.isPinInPlay(i) && !_world.isPinStanding(i))
                _world.removePin(i);
    }
    _world.placeBall(_aimOffset);
    enter(Phase::Aiming);
}

Snapshot LaneImpl::snapshot() const {
    Snapshot s;
    s.phase = _phase;
    s.ball = _world.ballPose();
    for (int i = 0; i < 10; ++i) {
        s.pins[i] = _world.pinPose(i);
        s.pinStanding[i] = _world.isPinStanding(i);
        s.pinInPlay[i] = _world.isPinInPlay(i);
    }
    s.aimOffset = _aimOffset;
    s.aimAngle = _aimAngle;
    s.power = _power;
    s.lastPinsDown = _lastPinsDown;
    return s;
}

}  // namespace Alley
}  // namespace Sandbox
```

- [ ] **Step 9: Write `libraries/Alley/src/Alley.cpp`**

```cpp
#include <Sandbox/Alley/Alley.h>
#include "Lane_impl.h"

namespace Sandbox {
namespace Alley {

Lane::Lane() : _impl(std::make_unique<LaneImpl>()) {}
Lane::~Lane() = default;

Result<> Lane::moveAim(float d) { return _impl->moveAim(d); }
Result<> Lane::rotateAim(float d) { return _impl->rotateAim(d); }
Result<> Lane::startCharge() { return _impl->startCharge(); }
Result<> Lane::release() { return _impl->release(); }
Result<> Lane::newGame() { return _impl->newGame(); }

void Lane::step(float dt) noexcept { _impl->step(dt); }

Snapshot Lane::snapshot() const { return _impl->snapshot(); }
std::vector<int> Lane::rolls() const { return _impl->game().rolls(); }
std::vector<int> Lane::frameScores() const { return _impl->game().scores(); }
int Lane::totalScore() const { return _impl->game().score(); }
bool Lane::isGameOver() const { return _impl->game().isOver(); }
int Lane::pinsLeft() const { return _impl->game().pinsLeft(); }

}  // namespace Alley
}  // namespace Sandbox
```

- [ ] **Step 10: Build and run the Alley tests**

Run: `cmake -S . -B build 2>&1 | tail -2 && cmake --build build -j8 2>&1 | grep -E 'error|warning' ; ctest --test-dir build -R alley --output-on-failure | tail -3`
Expected: no compiler diagnostics; `100% tests passed, 0 tests failed out of 7`.

If `-Werror` trips inside our files, fix the file; if it trips inside Bullet headers, the include is not SYSTEM — check that `Bullet::Bullet` is the only Bullet include path.

- [ ] **Step 11: Commit**

```bash
git add libraries/Alley libraries/CMakeLists.txt
git commit -m "feat(alley): Lane phase machine and Bullet world"
```

---

### Task 4: `Sandbox::Alley` — throw outcomes (gutter, strike, determinism, full games)

**Files:**
- Modify: `libraries/Alley/test/src/test_Alley.cpp`
- Possibly tune: `libraries/Alley/src/World.cpp` (friction/restitution/CCD constants only)

**Interfaces:**
- Consumes: `Lane`, `throwBall()` helper from Task 3.
- Produces: `kPocketOffset` constant documented in the test file (used by the manual README checklist in Task 9).

- [ ] **Step 1: Add the outcome tests**

Append to `libraries/Alley/test/src/test_Alley.cpp` (inside the anonymous namespace add the constant, the tests after the existing ones):

```cpp
namespace {
/// Lateral offset that produces a strike with power 0.7 and angle 0 in this
/// deterministic world. Tuned once in Task 4; the README uses the same value.
constexpr float kPocketOffset = 0.09f;
constexpr float kStrikePower = 0.7f;
}  // namespace

TEST_F(test_alley, gutter_ball_scores_zero_and_keeps_the_rack) {
    throwBall(lane, kMaxAimOffset, 0.f, 0.6f);
    const auto s = lane.snapshot();
    EXPECT_EQ(s.phase, Phase::Aiming);
    EXPECT_EQ(s.lastPinsDown, 0);
    EXPECT_EQ(standingCount(s), 10);
    EXPECT_EQ(lane.rolls()[0], 0);  // Miss
    EXPECT_EQ(lane.pinsLeft(), 10);
}

TEST_F(test_alley, pocket_throw_is_a_strike) {
    throwBall(lane, kPocketOffset, 0.f, kStrikePower);
    const auto s = lane.snapshot();
    EXPECT_EQ(s.lastPinsDown, 10);
    EXPECT_EQ(lane.rolls()[0], -2);  // Strike
    EXPECT_EQ(standingCount(s), 10);  // fresh rack for the next frame
    EXPECT_EQ(lane.frameScores()[0], -3);  // strike bonus not yet known
}

TEST_F(test_alley, same_inputs_give_identical_results) {
    Lane other;
    throwBall(lane, 0.2f, 1.5f, 0.5f);
    throwBall(other, 0.2f, 1.5f, 0.5f);
    const auto a = lane.snapshot();
    const auto b = other.snapshot();
    EXPECT_EQ(a.lastPinsDown, b.lastPinsDown);
    EXPECT_EQ(a.pinStanding, b.pinStanding);
    for (int i = 0; i < 10; ++i)
        EXPECT_EQ(a.pins[i].position, b.pins[i].position);
}

TEST_F(test_alley, second_ball_keeps_standing_pins) {
    throwBall(lane, 0.42f, 0.f, 0.4f);  // edge hit: some pins down, some standing
    const auto s = lane.snapshot();
    const int down = s.lastPinsDown;
    ASSERT_GE(down, 0);
    if (down > 0 && down < 10) {
        EXPECT_EQ(standingCount(s), 10 - down);
        EXPECT_EQ(lane.pinsLeft(), 10 - down);
        for (int i = 0; i < 10; ++i)
            EXPECT_EQ(s.pinInPlay[i], s.pinStanding[i]);  // fallen pins were removed
    }
}

TEST_F(test_alley, fallen_pins_stay_visible_until_aiming) {
    lane.moveAim(kPocketOffset);
    lane.startCharge();
    lane.step(chargeTimeFor(kStrikePower));
    lane.release();
    for (int i = 0; i < 60 * 20 && lane.snapshot().phase != Phase::FrameDone; ++i)
        lane.step(kDt);
    const auto s = lane.snapshot();
    ASSERT_EQ(s.phase, Phase::FrameDone);
    for (bool inPlay : s.pinInPlay)
        EXPECT_TRUE(inPlay);
}

TEST_F(test_alley, twenty_gutter_balls_end_the_game_with_zero) {
    for (int i = 0; i < 20; ++i)
        throwBall(lane, kMaxAimOffset, 0.f, 0.5f);
    EXPECT_EQ(lane.snapshot().phase, Phase::GameOver);
    EXPECT_TRUE(lane.isGameOver());
    EXPECT_EQ(lane.totalScore(), 0);
    EXPECT_TRUE(lane.startCharge().isError());
}

TEST_F(test_alley, twelve_strikes_score_three_hundred) {
    for (int i = 0; i < 12; ++i)
        throwBall(lane, kPocketOffset, 0.f, kStrikePower);
    EXPECT_EQ(lane.snapshot().phase, Phase::GameOver);
    EXPECT_EQ(lane.totalScore(), 300);
}
```

- [ ] **Step 2: Build and run**

Run: `cmake --build build -j8 2>&1 | grep -E 'error' ; ctest --test-dir build -R alley --output-on-failure | tail -20`
Expected: gutter, determinism, second-ball, visibility and twenty-gutter tests pass immediately. `pocket_throw_is_a_strike` and `twelve_strikes` may fail with `lastPinsDown` < 10.

- [ ] **Step 3: Tune until the pocket throw is a strike (only if Step 2 failed)**

Write a throw-away probe outside the repo, e.g. `/tmp/probe.cpp`, compiled against the build tree, that prints `lastPinsDown` for offsets 0.00..0.20 in 0.01 steps and powers 0.5..0.9:

```cpp
#include <Sandbox/Alley/Alley.h>
#include <cmath>
#include <cstdio>
using namespace Sandbox::Alley;
int main() {
    for (float power : {0.5f, 0.6f, 0.7f, 0.8f, 0.9f})
        for (float off = 0.f; off <= 0.2f; off += 0.01f) {
            Lane lane;
            lane.moveAim(off);
            lane.startCharge();
            lane.step(std::acos(1.f - 2.f * power) / (2.f * 3.14159265f));
            lane.release();
            for (int i = 0; i < 1200 && lane.snapshot().phase != Phase::Aiming && lane.snapshot().phase != Phase::GameOver; ++i)
                lane.step(1.f / 60.f);
            std::printf("power %.1f offset %.2f -> %d\n", power, off, lane.snapshot().lastPinsDown);
        }
}
```

```bash
g++ -std=c++20 /tmp/probe.cpp -I libraries/Alley/include -I build/libraries/Alley/include -I libraries/Core/include -I build/libraries/Core/include -I libraries/Gamebox/include -I build/libraries/Gamebox/include -L build/lib -lSandboxAlley -lSandboxGamebox -lSandboxCore -Wl,-rpath,$PWD/build/lib -o /tmp/probe && /tmp/probe | sort -t'>' -k2 -n | tail -15
```

Pick an (offset, power) pair that yields 10 and set `kPocketOffset` / `kStrikePower` in the test accordingly. If no pair reaches 10, adjust in `World.cpp` in this order, one at a time, re-running the probe each time: pin restitution 0.3 → 0.5; pin friction 0.5 → 0.3; pin COM `pinComHeight` stays (it is shared with the renderer) but lower cylinder half-height 0.10 → 0.08; ball mass 7 → 7.26. Delete `/tmp/probe*` afterwards.

- [ ] **Step 4: Run all tests**

Run: `ctest --test-dir build --output-on-failure | tail -3`
Expected: `100% tests passed`.

- [ ] **Step 5: Commit**

```bash
git add libraries/Alley
git commit -m "test(alley): throw outcomes, determinism and full-game flows"
```

---

### Task 5: `Sandbox::Render` — CPU geometry and scoreboard layout (no GL)

**Files:**
- Create: `libraries/Render/CMakeLists.txt`
- Create: `libraries/Render/include/Sandbox/Render/Geometry.h`, `libraries/Render/src/Geometry.cpp`
- Create: `libraries/Render/include/Sandbox/Render/ScoreboardLayout.h`, `libraries/Render/src/ScoreboardLayout.cpp`
- Create: `libraries/Render/test/src/test_Render.cpp`
- Create (empty placeholders filled in Task 6): `libraries/Render/include/Sandbox/Render/Render.h`, `libraries/Render/src/Render.cpp`, `libraries/Render/src/GlShader.h/.cpp`, `libraries/Render/src/GlMesh.h/.cpp`, `libraries/Render/src/ContextData.h` — **not yet**: this task lists only the non-GL sources in CMake; Task 6 adds the rest.

**Interfaces:**
- Produces:
  - `struct Sandbox::Render::MeshData { std::vector<float> vertices /* x y z nx ny nz */; std::vector<unsigned> indices; std::size_t vertexCount() const; }`
  - `MeshData boxData(float hx, float hy, float hz)`, `sphereData(float r, int segments)`, `cylinderData(float r, float height, int segments)`, `latheData(const std::vector<std::pair<float,float>>& profile, int segments)`, `pinData(float height, float maxRadius, float comHeight, int segments = 24)`
  - `struct Rect { float x, y, w, h; }`, `struct ScoreboardLayout { float originX, originY, cell; Rect frameRect(int) const; Rect rollCellRect(int) const; Rect scoreRect(int) const; float totalWidth() const; }`, `std::string glyphFor(int value, bool isScore = false)`

- [ ] **Step 1: Create `libraries/Render/CMakeLists.txt`**

```cmake
project(Render VERSION 1.0.0 LANGUAGES CXX)

set(TARGET_NAME ${PROJECT_NAME})
set(TST_DISCOVER ON)
###############################################################
# Manage project files
###############################################################
set(TARGET_HEADERS
    include/Sandbox/Render/Geometry.h
    include/Sandbox/Render/ScoreboardLayout.h
)
set(TARGET_SOURCES
    src/Geometry.cpp
    src/ScoreboardLayout.cpp
)
set(TARGET_RSC)
set(PRIVATE_DEPS)
set(PUBLIC_DEPS
    ${CMAKE_ROOT_NAME}::Core
)
set(TARGET_FILES ${TARGET_HEADERS} ${TARGET_SOURCES})
###############################################################
# Manage testing files
###############################################################
set(TST_SOURCES
    test/src/test_Render.cpp
)
set(TEST_DEPS)
###############################################################
# build
###############################################################
create_library(
    LIB_NAME             ${TARGET_NAME}
    LIB_FILES            ${TARGET_FILES}
    PRIVATE_DEPENDENCIES ${PRIVATE_DEPS}
    PUBLIC_DEPENDENCIES  ${PUBLIC_DEPS}
    TEST_DISCOVER        ${TST_DISCOVER}
    TEST_SOURCES         ${TST_SOURCES}
    TEST_DEPENDENCIES    ${TEST_DEPS}
    LIB_RSC              ${TARGET_RSC}
)
```

- [ ] **Step 2: Write the failing tests `libraries/Render/test/src/test_Render.cpp`**

```cpp
#include <Sandbox/Render/Geometry.h>
#include <Sandbox/Render/ScoreboardLayout.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>

using namespace Sandbox::Render;

namespace {
void expectWellFormed(const MeshData& m) {
    ASSERT_EQ(m.vertices.size() % 6, 0u);
    ASSERT_EQ(m.indices.size() % 3, 0u);
    const unsigned n = static_cast<unsigned>(m.vertexCount());
    for (unsigned idx : m.indices)
        EXPECT_LT(idx, n);
    for (std::size_t v = 0; v < m.vertexCount(); ++v) {
        const float nx = m.vertices[v * 6 + 3], ny = m.vertices[v * 6 + 4], nz = m.vertices[v * 6 + 5];
        EXPECT_NEAR(std::sqrt(nx * nx + ny * ny + nz * nz), 1.f, 1e-4f);
    }
}
float minY(const MeshData& m) {
    float y = 1e9f;
    for (std::size_t v = 0; v < m.vertexCount(); ++v)
        y = std::min(y, m.vertices[v * 6 + 1]);
    return y;
}
float maxY(const MeshData& m) {
    float y = -1e9f;
    for (std::size_t v = 0; v < m.vertexCount(); ++v)
        y = std::max(y, m.vertices[v * 6 + 1]);
    return y;
}
}  // namespace

TEST(test_render_geometry, box_has_24_vertices_and_12_triangles) {
    const auto m = boxData(1.f, 2.f, 3.f);
    EXPECT_EQ(m.vertexCount(), 24u);
    EXPECT_EQ(m.indices.size(), 36u);
    expectWellFormed(m);
    EXPECT_FLOAT_EQ(minY(m), -2.f);
    EXPECT_FLOAT_EQ(maxY(m), 2.f);
}

TEST(test_render_geometry, sphere_is_well_formed_and_bounded) {
    const auto m = sphereData(0.5f, 16);
    expectWellFormed(m);
    EXPECT_NEAR(minY(m), -0.5f, 1e-5f);
    EXPECT_NEAR(maxY(m), 0.5f, 1e-5f);
}

TEST(test_render_geometry, cylinder_is_centred_on_origin) {
    const auto m = cylinderData(0.3f, 1.0f, 12);
    expectWellFormed(m);
    EXPECT_NEAR(minY(m), -0.5f, 1e-5f);
    EXPECT_NEAR(maxY(m), 0.5f, 1e-5f);
}

TEST(test_render_geometry, pin_spans_from_base_to_top_relative_to_com) {
    const auto m = pinData(0.38f, 0.06f, 0.12f);
    expectWellFormed(m);
    EXPECT_NEAR(minY(m), -0.12f, 1e-5f);
    EXPECT_NEAR(maxY(m), 0.26f, 1e-5f);
}

TEST(test_render_scoreboard, glyphs_follow_gamebox_encoding) {
    EXPECT_EQ(glyphFor(-4), "");
    EXPECT_EQ(glyphFor(-3), "");
    EXPECT_EQ(glyphFor(-2), "X");
    EXPECT_EQ(glyphFor(-1), "/");
    EXPECT_EQ(glyphFor(0), "-");
    EXPECT_EQ(glyphFor(0, true), "0");
    EXPECT_EQ(glyphFor(7), "7");
    EXPECT_EQ(glyphFor(300, true), "300");
}

TEST(test_render_scoreboard, frames_are_two_cells_wide_except_the_last) {
    const ScoreboardLayout l {10.f, 20.f, 30.f};
    const Rect f0 = l.frameRect(0);
    EXPECT_FLOAT_EQ(f0.x, 10.f);
    EXPECT_FLOAT_EQ(f0.y, 20.f);
    EXPECT_FLOAT_EQ(f0.w, 60.f);
    EXPECT_FLOAT_EQ(f0.h, 60.f);
    const Rect f9 = l.frameRect(9);
    EXPECT_FLOAT_EQ(f9.x, 10.f + 9 * 60.f);
    EXPECT_FLOAT_EQ(f9.w, 90.f);
    EXPECT_FLOAT_EQ(l.totalWidth(), 21 * 30.f);
}

TEST(test_render_scoreboard, roll_cells_map_the_21_slot_encoding) {
    const ScoreboardLayout l {0.f, 0.f, 10.f};
    const Rect r0 = l.rollCellRect(0);
    EXPECT_FLOAT_EQ(r0.x, 0.f);
    EXPECT_FLOAT_EQ(r0.w, 10.f);
    const Rect r3 = l.rollCellRect(3);  // frame 1, second roll
    EXPECT_FLOAT_EQ(r3.x, 30.f);
    const Rect r20 = l.rollCellRect(20);  // frame 9, third roll
    EXPECT_FLOAT_EQ(r20.x, 180.f + 20.f);
    const Rect s4 = l.scoreRect(4);
    EXPECT_FLOAT_EQ(s4.x, 80.f);
    EXPECT_FLOAT_EQ(s4.y, 10.f);
    EXPECT_FLOAT_EQ(s4.w, 20.f);
}
```

- [ ] **Step 3: Configure to verify the tests fail**

Run: `cmake -S . -B build 2>&1 | tail -2 && cmake --build build -j8 --target test_sandbox_render 2>&1 | grep -E 'error' | head -3`
Expected: `fatal error: Sandbox/Render/Geometry.h: No such file or directory` (or missing source errors).

- [ ] **Step 4: Write `libraries/Render/include/Sandbox/Render/Geometry.h`**

```cpp
#ifndef SANDBOX_RENDER_GEOMETRY_H
#define SANDBOX_RENDER_GEOMETRY_H
#include <Sandbox/Render/Sandbox_Render_export.h>
#include <cstddef>
#include <utility>
#include <vector>

namespace Sandbox {
namespace Render {

/// Interleaved triangle mesh: position xyz + normal xyz per vertex.
struct MeshData {
    std::vector<float> vertices;
    std::vector<unsigned> indices;
    std::size_t vertexCount() const { return vertices.size() / 6; }
};

SANDBOX_RENDER_API MeshData boxData(float hx, float hy, float hz);
SANDBOX_RENDER_API MeshData sphereData(float radius, int segments);
/// Cylinder along y, centred at the origin.
SANDBOX_RENDER_API MeshData cylinderData(float radius, float height, int segments);
/// Surface of revolution around y. profile = (radius, y) pairs from bottom to top.
SANDBOX_RENDER_API MeshData latheData(const std::vector<std::pair<float, float>>& profile,
                                      int segments);
/// Bowling pin with its origin comHeight above the base (so it matches the
/// physics body). Spans y in [-comHeight, height - comHeight].
SANDBOX_RENDER_API MeshData pinData(float height, float maxRadius, float comHeight,
                                    int segments = 24);

}  // namespace Render
}  // namespace Sandbox
#endif  // SANDBOX_RENDER_GEOMETRY_H
```

- [ ] **Step 5: Write `libraries/Render/src/Geometry.cpp`**

```cpp
#include <Sandbox/Render/Geometry.h>
#include <cmath>

namespace Sandbox {
namespace Render {
namespace {
constexpr float kPi = 3.14159265358979f;

void push(MeshData& m, float x, float y, float z, float nx, float ny, float nz) {
    m.vertices.insert(m.vertices.end(), {x, y, z, nx, ny, nz});
}
void tri(MeshData& m, unsigned a, unsigned b, unsigned c) {
    m.indices.insert(m.indices.end(), {a, b, c});
}
}  // namespace

MeshData boxData(float hx, float hy, float hz) {
    MeshData m;
    struct Face { float n[3]; float u[3]; float v[3]; };
    const Face faces[6] = {
        {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}},  {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
        {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}},  {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}},
        {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},   {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}},
    };
    const float h[3] = {hx, hy, hz};
    for (const Face& f : faces) {
        const unsigned base = static_cast<unsigned>(m.vertexCount());
        for (int corner = 0; corner < 4; ++corner) {
            const float su = (corner == 1 || corner == 2) ? 1.f : -1.f;
            const float sv = (corner >= 2) ? 1.f : -1.f;
            float p[3];
            for (int k = 0; k < 3; ++k)
                p[k] = (f.n[k] + su * f.u[k] + sv * f.v[k]) * h[k];
            push(m, p[0], p[1], p[2], f.n[0], f.n[1], f.n[2]);
        }
        tri(m, base, base + 1, base + 2);
        tri(m, base, base + 2, base + 3);
    }
    return m;
}

MeshData latheData(const std::vector<std::pair<float, float>>& profile, int segments) {
    MeshData m;
    const int rings = static_cast<int>(profile.size());
    if (rings < 2 || segments < 3)
        return m;
    for (int i = 0; i < rings; ++i) {
        // Normal from the profile tangent (radial, up).
        const int prev = i > 0 ? i - 1 : i;
        const int next = i < rings - 1 ? i + 1 : i;
        const float dr = profile[next].first - profile[prev].first;
        const float dy = profile[next].second - profile[prev].second;
        float nr = dy, ny = -dr;
        const float len = std::sqrt(nr * nr + ny * ny);
        if (len > 1e-6f) { nr /= len; ny /= len; } else { nr = 0.f; ny = 1.f; }
        if (profile[i].first <= 1e-6f) { nr = 0.f; ny = (i == 0) ? -1.f : 1.f; }
        for (int s = 0; s <= segments; ++s) {
            const float a = 2.f * kPi * static_cast<float>(s) / static_cast<float>(segments);
            const float c = std::cos(a), sn = std::sin(a);
            push(m, profile[i].first * c, profile[i].second, profile[i].first * sn,
                 nr * c, ny, nr * sn);
        }
    }
    const unsigned stride = static_cast<unsigned>(segments + 1);
    for (int i = 0; i < rings - 1; ++i)
        for (int s = 0; s < segments; ++s) {
            const unsigned a = static_cast<unsigned>(i) * stride + static_cast<unsigned>(s);
            const unsigned b = a + 1, c = a + stride, d = c + 1;
            tri(m, a, c, b);
            tri(m, b, c, d);
        }
    return m;
}

MeshData sphereData(float radius, int segments) {
    std::vector<std::pair<float, float>> profile;
    const int stacks = segments / 2 < 2 ? 2 : segments / 2;
    for (int i = 0; i <= stacks; ++i) {
        const float t = kPi * static_cast<float>(i) / static_cast<float>(stacks);  // 0 (bottom) .. pi (top)
        profile.emplace_back(radius * std::sin(t), -radius * std::cos(t));
    }
    return latheData(profile, segments);
}

MeshData cylinderData(float radius, float height, int segments) {
    const float h = height / 2;
    // Duplicate rings at the rims give hard edges between caps and side.
    return latheData({{0.f, -h}, {radius, -h}, {radius, -h}, {radius, h}, {radius, h}, {0.f, h}},
                     segments);
}

MeshData pinData(float height, float maxRadius, float comHeight, int segments) {
    // (radius fraction, height fraction) of a ten-pin.
    const std::pair<float, float> shape[] = {
        {0.0f, 0.00f}, {0.45f, 0.00f}, {0.70f, 0.05f}, {0.95f, 0.20f}, {1.00f, 0.30f},
        {0.90f, 0.45f}, {0.60f, 0.60f}, {0.45f, 0.70f}, {0.50f, 0.80f}, {0.55f, 0.90f},
        {0.35f, 0.98f}, {0.0f, 1.00f},
    };
    std::vector<std::pair<float, float>> profile;
    for (const auto& [r, y] : shape)
        profile.emplace_back(r * maxRadius, y * height - comHeight);
    return latheData(profile, segments);
}

}  // namespace Render
}  // namespace Sandbox
```

- [ ] **Step 6: Write `libraries/Render/include/Sandbox/Render/ScoreboardLayout.h` and `src/ScoreboardLayout.cpp`**

Header:

```cpp
#ifndef SANDBOX_RENDER_SCOREBOARD_LAYOUT_H
#define SANDBOX_RENDER_SCOREBOARD_LAYOUT_H
#include <Sandbox/Render/Sandbox_Render_export.h>
#include <string>

namespace Sandbox {
namespace Render {

struct Rect {
    float x, y, w, h;
};

/// Pixel geometry of the 10-frame grid. Frames 0..8 are 2 cells wide, frame 9
/// is 3 cells wide; the top row holds the rolls, the bottom row the score.
struct SANDBOX_RENDER_API ScoreboardLayout {
    float originX;
    float originY;
    float cell;

    Rect frameRect(int frame) const;      // 0..9
    Rect rollCellRect(int roll) const;    // 0..20, Gamebox 21-slot encoding
    Rect scoreRect(int frame) const;      // 0..9
    float totalWidth() const { return cell * 21.f; }
};

/// Gamebox value -> text. -4/-3 blank, -2 "X", -1 "/", 0 "-" (or "0" for scores).
SANDBOX_RENDER_API std::string glyphFor(int value, bool isScore = false);

}  // namespace Render
}  // namespace Sandbox
#endif  // SANDBOX_RENDER_SCOREBOARD_LAYOUT_H
```

Source:

```cpp
#include <Sandbox/Render/ScoreboardLayout.h>
#include <algorithm>

namespace Sandbox {
namespace Render {

Rect ScoreboardLayout::frameRect(int frame) const {
    const float w = (frame == 9 ? 3.f : 2.f) * cell;
    return {originX + 2.f * cell * static_cast<float>(frame), originY, w, 2.f * cell};
}

Rect ScoreboardLayout::rollCellRect(int roll) const {
    const int frame = std::min(roll / 2, 9);
    const int within = roll - 2 * frame;  // 0, 1 (or 2 in the last frame)
    const Rect f = frameRect(frame);
    return {f.x + cell * static_cast<float>(within), f.y, cell, cell};
}

Rect ScoreboardLayout::scoreRect(int frame) const {
    const Rect f = frameRect(frame);
    return {f.x, f.y + cell, f.w, cell};
}

std::string glyphFor(int value, bool isScore) {
    switch (value) {
        case -4:
        case -3:
            return "";
        case -2:
            return "X";
        case -1:
            return "/";
        case 0:
            return isScore ? "0" : "-";
        default:
            return std::to_string(value);
    }
}

}  // namespace Render
}  // namespace Sandbox
```

- [ ] **Step 7: Build and run**

Run: `cmake -S . -B build 2>&1 | tail -2 && cmake --build build -j8 2>&1 | grep -E 'error|warning'; ctest --test-dir build -R render --output-on-failure | tail -3`
Expected: `100% tests passed, 0 tests failed out of 7`.

- [ ] **Step 8: Commit**

```bash
git add libraries/Render
git commit -m "feat(render): CPU mesh generators and scoreboard layout"
```

---

### Task 6: `Sandbox::Render` — OpenGL context, shaders, scene and overlay

**Files:**
- Create: `libraries/Render/include/Sandbox/Render/Render.h`
- Create: `libraries/Render/src/GlShader.h`, `GlShader.cpp`, `GlMesh.h`, `GlMesh.cpp`, `ContextData.h`, `Render.cpp`
- Modify: `libraries/Render/CMakeLists.txt`

**Interfaces:**
- Consumes: `MeshData` generators, `ScoreboardLayout`, `glyphFor` (Task 5); `glad::glad`, `glm::glm`, `stb::easy_font` (Task 2).
- Produces (used by Task 8):
  - `struct Transform { glm::vec3 position; glm::quat rotation; glm::vec3 scale; glm::mat4 matrix() const; }`
  - `struct SceneGeometry { float laneHalfWidth, laneLength, approach, gutterWidth, gutterDepth, ballRadius, pinHeight, pinMaxRadius, pinComHeight; }`
  - `struct SceneDescription { Transform ball; std::array<Transform,10> pins; std::array<bool,10> pinVisible; bool showAimGuide; float aimOffset; float aimAngleDeg; }`
  - `class Context { static Result<std::unique_ptr<Context>> create(); void resize(int,int); int width() const; int height() const; void beginFrame(const glm::vec3& clear); void endFrame(); }`
  - `class Camera { void setViewport(int w,int h); void lookAt(glm::vec3 eye, glm::vec3 target); glm::mat4 view() const; glm::mat4 projection() const; }`
  - `class SceneRenderer { SceneRenderer(Context&, const SceneGeometry&); void draw(const Camera&, const SceneDescription&, int viewportY, int viewportH); }`
  - `class Overlay { explicit Overlay(Context&); void begin(int w,int h); void drawText(std::string_view, glm::vec2 px, float scale, glm::vec3 rgb); glm::vec2 measureText(std::string_view, float scale) const; void drawRect(glm::vec2 px, glm::vec2 size, glm::vec4 rgba); void strokeRect(const Rect&, float thickness, glm::vec4 rgba); void drawScoreboard(const std::vector<int>& rolls, const std::vector<int>& scores, int total, const ScoreboardLayout&); }`

- [ ] **Step 1: Add GL sources and dependencies to `libraries/Render/CMakeLists.txt`**

Replace the header/source/deps blocks with:

```cmake
set(TARGET_HEADERS
    src/GlShader.h
    src/GlMesh.h
    src/ContextData.h
    include/Sandbox/Render/Geometry.h
    include/Sandbox/Render/ScoreboardLayout.h
    include/Sandbox/Render/Render.h
)
set(TARGET_SOURCES
    src/Geometry.cpp
    src/ScoreboardLayout.cpp
    src/GlShader.cpp
    src/GlMesh.cpp
    src/Render.cpp
)
set(TARGET_RSC)
# GLAD and stb are implementation details; GLM appears in the public header.
# BUILD_INTERFACE keeps the fetched/in-tree targets out of the install export.
set(PRIVATE_DEPS
    $<BUILD_INTERFACE:glad::glad>
    $<BUILD_INTERFACE:stb::easy_font>
)
set(PUBLIC_DEPS
    ${CMAKE_ROOT_NAME}::Core
    $<BUILD_INTERFACE:glm::glm>
)
```

- [ ] **Step 2: Write `libraries/Render/include/Sandbox/Render/Render.h`**

```cpp
#ifndef SANDBOX_RENDER_H
#define SANDBOX_RENDER_H
#include <Sandbox/Render/Sandbox_Render_export.h>
#include <Sandbox/Render/ScoreboardLayout.h>
#include <Sandbox/Core/Core.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <array>
#include <memory>
#include <string_view>
#include <vector>

namespace Sandbox {
namespace Render {

struct SANDBOX_RENDER_API Transform {
    glm::vec3 position {0.f};
    glm::quat rotation {1.f, 0.f, 0.f, 0.f};  // w, x, y, z
    glm::vec3 scale {1.f};
    glm::mat4 matrix() const;
};

/// Physical sizes (metres); copied from Alley::kLane by the application.
struct SceneGeometry {
    float laneHalfWidth, laneLength, approach, gutterWidth, gutterDepth;
    float ballRadius, pinHeight, pinMaxRadius, pinComHeight;
};

struct SceneDescription {
    Transform ball;
    std::array<Transform, 10> pins;
    std::array<bool, 10> pinVisible {};
    bool showAimGuide = false;
    float aimOffset = 0.f;    // metres, ball start x
    float aimAngleDeg = 0.f;  // rotation about y
};

struct ContextData;

/// One per GL context. Loads GL 3.3 core via GLAD and owns the shader programs.
class SANDBOX_RENDER_API Context {
public:
    static Result<std::unique_ptr<Context>> create();  // needs a current GL context
    ~Context();
    void resize(int width, int height);
    int width() const;
    int height() const;
    void beginFrame(const glm::vec3& clearColor);
    void endFrame();
    ContextData& data();

private:
    Context();
    std::unique_ptr<ContextData> _d;
};

class SANDBOX_RENDER_API Camera {
public:
    void setViewport(int width, int height);
    void lookAt(glm::vec3 eye, glm::vec3 target);
    glm::mat4 view() const;
    glm::mat4 projection() const;

private:
    glm::vec3 _eye {0.f, 1.8f, -3.5f};
    glm::vec3 _target {0.f, 0.f, 6.f};
    float _aspect = 16.f / 9.f;
};

/// Lane, gutters, ball, pins, aim guide.
class SANDBOX_RENDER_API SceneRenderer {
public:
    SceneRenderer(Context& ctx, const SceneGeometry& geometry);
    ~SceneRenderer();
    void draw(const Camera& camera, const SceneDescription& scene, int viewportY, int viewportH);

private:
    struct Impl;
    std::unique_ptr<Impl> _p;
};

/// 2D layer in pixels, origin top-left, drawn after the scene.
class SANDBOX_RENDER_API Overlay {
public:
    explicit Overlay(Context& ctx);
    ~Overlay();
    void begin(int width, int height);
    void drawText(std::string_view text, glm::vec2 px, float scale, glm::vec3 rgb);
    glm::vec2 measureText(std::string_view text, float scale) const;
    void drawRect(glm::vec2 px, glm::vec2 size, glm::vec4 rgba);
    void strokeRect(const Rect& r, float thickness, glm::vec4 rgba);
    void drawScoreboard(const std::vector<int>& rolls, const std::vector<int>& scores, int total,
                        const ScoreboardLayout& layout);

private:
    struct Impl;
    std::unique_ptr<Impl> _p;
};

}  // namespace Render
}  // namespace Sandbox
#endif  // SANDBOX_RENDER_H
```

- [ ] **Step 3: Write `libraries/Render/src/GlShader.h` and `GlShader.cpp`**

Header:

```cpp
#ifndef SANDBOX_RENDER_GLSHADER_H
#define SANDBOX_RENDER_GLSHADER_H
#include <Sandbox/Core/Core.h>
#include <glm/glm.hpp>
#include <memory>

namespace Sandbox {
namespace Render {

class GlShader {
public:
    static Result<std::unique_ptr<GlShader>> build(const char* vertexSrc, const char* fragmentSrc);
    ~GlShader();
    GlShader(const GlShader&) = delete;
    GlShader& operator=(const GlShader&) = delete;

    void use() const;
    void setMat4(const char* name, const glm::mat4& m) const;
    void setMat3(const char* name, const glm::mat3& m) const;
    void setVec2(const char* name, const glm::vec2& v) const;
    void setVec3(const char* name, const glm::vec3& v) const;
    void setVec4(const char* name, const glm::vec4& v) const;

private:
    explicit GlShader(unsigned program) : _program(program) {}
    unsigned _program;
};

}  // namespace Render
}  // namespace Sandbox
#endif  // SANDBOX_RENDER_GLSHADER_H
```

Source:

```cpp
#include "GlShader.h"
#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <string>

namespace Sandbox {
namespace Render {
namespace {
Result<unsigned> compile(GLenum type, const char* src) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_FALSE) {
        char log[2048] = {};
        glGetShaderInfoLog(shader, sizeof log, nullptr, log);
        glDeleteShader(shader);
        return Sc::Error<>(std::string(type == GL_VERTEX_SHADER ? "vertex: " : "fragment: ") + log);
    }
    return static_cast<unsigned>(shader);
}
}  // namespace

Result<std::unique_ptr<GlShader>> GlShader::build(const char* vertexSrc, const char* fragmentSrc) {
    auto vs = compile(GL_VERTEX_SHADER, vertexSrc);
    if (vs.isError())
        return Sc::Error<>(vs.error());
    auto fs = compile(GL_FRAGMENT_SHADER, fragmentSrc);
    if (fs.isError()) {
        glDeleteShader(vs());
        return Sc::Error<>(fs.error());
    }
    const GLuint vsId = vs(), fsId = fs();
    const GLuint program = glCreateProgram();
    glAttachShader(program, vsId);
    glAttachShader(program, fsId);
    glLinkProgram(program);
    glDeleteShader(vsId);
    glDeleteShader(fsId);
    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok == GL_FALSE) {
        char log[2048] = {};
        glGetProgramInfoLog(program, sizeof log, nullptr, log);
        glDeleteProgram(program);
        return Sc::Error<>(std::string("link: ") + log);
    }
    return std::unique_ptr<GlShader>(new GlShader(program));
}

GlShader::~GlShader() {
    glDeleteProgram(_program);
}

void GlShader::use() const {
    glUseProgram(_program);
}
void GlShader::setMat4(const char* name, const glm::mat4& m) const {
    glUniformMatrix4fv(glGetUniformLocation(_program, name), 1, GL_FALSE, glm::value_ptr(m));
}
void GlShader::setMat3(const char* name, const glm::mat3& m) const {
    glUniformMatrix3fv(glGetUniformLocation(_program, name), 1, GL_FALSE, glm::value_ptr(m));
}
void GlShader::setVec2(const char* name, const glm::vec2& v) const {
    glUniform2fv(glGetUniformLocation(_program, name), 1, glm::value_ptr(v));
}
void GlShader::setVec3(const char* name, const glm::vec3& v) const {
    glUniform3fv(glGetUniformLocation(_program, name), 1, glm::value_ptr(v));
}
void GlShader::setVec4(const char* name, const glm::vec4& v) const {
    glUniform4fv(glGetUniformLocation(_program, name), 1, glm::value_ptr(v));
}

}  // namespace Render
}  // namespace Sandbox
```

- [ ] **Step 4: Write `libraries/Render/src/GlMesh.h` and `GlMesh.cpp`**

Header:

```cpp
#ifndef SANDBOX_RENDER_GLMESH_H
#define SANDBOX_RENDER_GLMESH_H
#include <Sandbox/Render/Geometry.h>

namespace Sandbox {
namespace Render {

/// VAO/VBO/EBO for a MeshData: location 0 = position, location 1 = normal.
class GlMesh {
public:
    explicit GlMesh(const MeshData& data);
    ~GlMesh();
    GlMesh(const GlMesh&) = delete;
    GlMesh& operator=(const GlMesh&) = delete;
    void draw() const;

private:
    unsigned _vao = 0, _vbo = 0, _ebo = 0;
    int _count = 0;
};

}  // namespace Render
}  // namespace Sandbox
#endif  // SANDBOX_RENDER_GLMESH_H
```

Source:

```cpp
#include "GlMesh.h"
#include <glad/gl.h>
#include <cstdint>

namespace Sandbox {
namespace Render {

GlMesh::GlMesh(const MeshData& data) : _count(static_cast<int>(data.indices.size())) {
    glGenVertexArrays(1, &_vao);
    glGenBuffers(1, &_vbo);
    glGenBuffers(1, &_ebo);
    glBindVertexArray(_vao);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.vertices.size() * sizeof(float)),
                 data.vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.indices.size() * sizeof(unsigned)),
                 data.indices.data(), GL_STATIC_DRAW);
    const GLsizei stride = 6 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(static_cast<std::uintptr_t>(3 * sizeof(float))));
    glBindVertexArray(0);
}

GlMesh::~GlMesh() {
    glDeleteBuffers(1, &_ebo);
    glDeleteBuffers(1, &_vbo);
    glDeleteVertexArrays(1, &_vao);
}

void GlMesh::draw() const {
    glBindVertexArray(_vao);
    glDrawElements(GL_TRIANGLES, _count, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

}  // namespace Render
}  // namespace Sandbox
```

- [ ] **Step 5: Write `libraries/Render/src/ContextData.h`**

```cpp
#ifndef SANDBOX_RENDER_CONTEXT_DATA_H
#define SANDBOX_RENDER_CONTEXT_DATA_H
#include "GlShader.h"
#include <memory>

namespace Sandbox {
namespace Render {

struct ContextData {
    std::unique_ptr<GlShader> lit;   // 3D, Lambert + colour band
    std::unique_ptr<GlShader> flat;  // 2D overlay
    int width = 1;
    int height = 1;
};

}  // namespace Render
}  // namespace Sandbox
#endif  // SANDBOX_RENDER_CONTEXT_DATA_H
```

- [ ] **Step 6: Write `libraries/Render/src/Render.cpp`**

```cpp
#include <Sandbox/Render/Render.h>
#include "ContextData.h"
#include "GlMesh.h"
#include <glad/gl.h>
#include <stb_easy_font.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>
#include <cmath>
#include <string>

namespace Sandbox {
namespace Render {
namespace {

const char* kLitVs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uModel; uniform mat4 uView; uniform mat4 uProj; uniform mat3 uNormalMat;
out vec3 vNormal; out float vLocalY;
void main() {
    vNormal = normalize(uNormalMat * aNormal);
    vLocalY = aPos.y;
    gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);
})";

const char* kLitFs = R"(#version 330 core
in vec3 vNormal; in float vLocalY;
uniform vec4 uColor; uniform vec4 uBandColor; uniform vec2 uBandRange; uniform vec3 uLightDir;
out vec4 FragColor;
void main() {
    vec4 base = (vLocalY >= uBandRange.x && vLocalY <= uBandRange.y) ? uBandColor : uColor;
    float diff = max(dot(normalize(vNormal), normalize(-uLightDir)), 0.0);
    FragColor = vec4(base.rgb * (0.35 + 0.65 * diff), base.a);
})";

const char* kFlatVs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() { gl_Position = uMVP * vec4(aPos.xy, 0.0, 1.0); })";

const char* kFlatFs = R"(#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() { FragColor = uColor; })";

const glm::vec2 kNoBand {1.f, -1.f};  // min > max: matches no y
const glm::vec3 kLightDir {-0.3f, -1.f, 0.4f};

struct Vertex2D {
    float x, y, z;
    unsigned char rgba[4];
};
}  // namespace

// ---------------------------------------------------------------- Transform
glm::mat4 Transform::matrix() const {
    return glm::translate(glm::mat4(1.f), position) * glm::mat4_cast(rotation) *
           glm::scale(glm::mat4(1.f), scale);
}

// ---------------------------------------------------------------- Context
Context::Context() : _d(std::make_unique<ContextData>()) {}
Context::~Context() = default;

Result<std::unique_ptr<Context>> Context::create() {
    const int version = gladLoaderLoadGL();
    if (version == 0)
        return Sc::Error<>("could not load OpenGL function pointers (is a GL context current?)");
    const int major = GLAD_VERSION_MAJOR(version), minor = GLAD_VERSION_MINOR(version);
    if (major * 10 + minor < 33)
        return Sc::Error<>("OpenGL 3.3 core required, got " + std::to_string(major) + "." +
                           std::to_string(minor));
    auto lit = GlShader::build(kLitVs, kLitFs);
    if (lit.isError())
        return Sc::Error<>("lit shader: " + lit.error());
    auto flat = GlShader::build(kFlatVs, kFlatFs);
    if (flat.isError())
        return Sc::Error<>("flat shader: " + flat.error());
    std::unique_ptr<Context> ctx(new Context());
    ctx->_d->lit = lit();
    ctx->_d->flat = flat();
    return Result<std::unique_ptr<Context>>(std::move(ctx));
}

void Context::resize(int width, int height) {
    _d->width = width > 0 ? width : 1;
    _d->height = height > 0 ? height : 1;
}
int Context::width() const { return _d->width; }
int Context::height() const { return _d->height; }
ContextData& Context::data() { return *_d; }

void Context::beginFrame(const glm::vec3& c) {
    glViewport(0, 0, _d->width, _d->height);
    glClearColor(c.r, c.g, c.b, 1.f);
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
void Context::endFrame() {
    glBindVertexArray(0);
    glUseProgram(0);
}

// ---------------------------------------------------------------- Camera
void Camera::setViewport(int width, int height) {
    _aspect = static_cast<float>(width > 0 ? width : 1) / static_cast<float>(height > 0 ? height : 1);
}
void Camera::lookAt(glm::vec3 eye, glm::vec3 target) {
    _eye = eye;
    _target = target;
}
glm::mat4 Camera::view() const {
    return glm::lookAt(_eye, _target, glm::vec3(0.f, 1.f, 0.f));
}
glm::mat4 Camera::projection() const {
    return glm::perspective(glm::radians(45.f), _aspect, 0.1f, 100.f);
}

// ---------------------------------------------------------------- SceneRenderer
struct SceneRenderer::Impl {
    Context& ctx;
    SceneGeometry g;
    GlMesh lane, gutter, wall, ball, pin, guide;

    explicit Impl(Context& c, const SceneGeometry& geo)
        : ctx(c), g(geo),
          lane(boxData(geo.laneHalfWidth, 0.05f, (geo.laneLength + geo.approach) / 2)),
          gutter(boxData(geo.gutterWidth / 2, 0.05f, geo.laneLength / 2)),
          wall(boxData(0.02f, 0.15f, geo.laneLength / 2)),
          ball(sphereData(geo.ballRadius, 32)),
          pin(pinData(geo.pinHeight, geo.pinMaxRadius, geo.pinComHeight)),
          guide(boxData(0.01f, 0.003f, 0.5f)) {}

    void drawMesh(const GlMesh& mesh, const glm::mat4& model, glm::vec4 color,
                  glm::vec2 band = kNoBand, glm::vec4 bandColor = glm::vec4(1.f)) {
        GlShader& s = *ctx.data().lit;
        s.setMat4("uModel", model);
        s.setMat3("uNormalMat", glm::inverseTranspose(glm::mat3(model)));
        s.setVec4("uColor", color);
        s.setVec2("uBandRange", band);
        s.setVec4("uBandColor", bandColor);
        mesh.draw();
    }
    static glm::mat4 at(float x, float y, float z) {
        return glm::translate(glm::mat4(1.f), glm::vec3(x, y, z));
    }
};

SceneRenderer::SceneRenderer(Context& ctx, const SceneGeometry& geometry)
    : _p(std::make_unique<Impl>(ctx, geometry)) {}
SceneRenderer::~SceneRenderer() = default;

void SceneRenderer::draw(const Camera& camera, const SceneDescription& scene, int viewportY,
                         int viewportH) {
    Impl& p = *_p;
    const SceneGeometry& g = p.g;
    glViewport(0, viewportY, p.ctx.width(), viewportH);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    GlShader& s = *p.ctx.data().lit;
    s.use();
    s.setMat4("uView", camera.view());
    s.setMat4("uProj", camera.projection());
    s.setVec3("uLightDir", kLightDir);

    const float outerX = g.laneHalfWidth + g.gutterWidth;
    p.drawMesh(p.lane, Impl::at(0.f, -0.05f, (g.laneLength - g.approach) / 2), {0.76f, 0.60f, 0.38f, 1.f});
    for (float sx : {-1.f, 1.f}) {
        p.drawMesh(p.gutter, Impl::at(sx * (g.laneHalfWidth + g.gutterWidth / 2), -g.gutterDepth - 0.05f, g.laneLength / 2),
                   {0.25f, 0.25f, 0.28f, 1.f});
        p.drawMesh(p.wall, Impl::at(sx * (outerX + 0.02f), 0.05f, g.laneLength / 2), {0.18f, 0.18f, 0.2f, 1.f});
    }
    const glm::vec2 neck {0.55f * g.pinHeight - g.pinComHeight, 0.65f * g.pinHeight - g.pinComHeight};
    for (int i = 0; i < 10; ++i)
        if (scene.pinVisible[i])
            p.drawMesh(p.pin, scene.pins[i].matrix(), {0.95f, 0.95f, 0.92f, 1.f}, neck, {0.85f, 0.1f, 0.1f, 1.f});
    p.drawMesh(p.ball, scene.ball.matrix(), {0.15f, 0.25f, 0.75f, 1.f});

    if (scene.showAimGuide) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        const float length = g.laneLength + 1.f;  // from the ball start (z = -1) to the deck end
        const float a = glm::radians(scene.aimAngleDeg);
        const glm::vec3 dir(std::sin(a), 0.f, std::cos(a));
        const glm::vec3 start(scene.aimOffset, 0.004f, -1.f);
        glm::mat4 model = glm::translate(glm::mat4(1.f), start + dir * (length / 2)) *
                          glm::rotate(glm::mat4(1.f), a, glm::vec3(0.f, 1.f, 0.f)) *
                          glm::scale(glm::mat4(1.f), glm::vec3(1.f, 1.f, length));
        p.drawMesh(p.guide, model, {1.f, 1.f, 0.3f, 0.35f});
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}

// ---------------------------------------------------------------- Overlay
struct Overlay::Impl {
    Context& ctx;
    unsigned vao = 0, vbo = 0, ebo = 0;
    glm::mat4 proj {1.f};
    std::vector<char> textBuffer = std::vector<char>(200000);
    std::vector<unsigned> indices;

    explicit Impl(Context& c) : ctx(c) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), nullptr);
        glBindVertexArray(0);
    }
    ~Impl() {
        glDeleteBuffers(1, &ebo);
        glDeleteBuffers(1, &vbo);
        glDeleteVertexArrays(1, &vao);
    }

    /// Upload `quads` quads of Vertex2D and draw them as triangles.
    void drawQuads(const void* vertices, int quads, const glm::mat4& model, glm::vec4 color) {
        indices.clear();
        for (int q = 0; q < quads; ++q) {
            const unsigned b = static_cast<unsigned>(q) * 4;
            indices.insert(indices.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
        }
        GlShader& s = *ctx.data().flat;
        s.use();
        s.setMat4("uMVP", proj * model);
        s.setVec4("uColor", color);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(quads * 4 * sizeof(Vertex2D)), vertices, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned)), indices.data(), GL_DYNAMIC_DRAW);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }
};

Overlay::Overlay(Context& ctx) : _p(std::make_unique<Impl>(ctx)) {}
Overlay::~Overlay() = default;

void Overlay::begin(int width, int height) {
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    _p->proj = glm::ortho(0.f, static_cast<float>(width), static_cast<float>(height), 0.f, -1.f, 1.f);
}

void Overlay::drawText(std::string_view text, glm::vec2 px, float scale, glm::vec3 rgb) {
    if (text.empty())
        return;
    std::string s(text);
    const int quads = stb_easy_font_print(0.f, 0.f, s.data(), nullptr, _p->textBuffer.data(),
                                          static_cast<int>(_p->textBuffer.size()));
    const glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(px, 0.f)) *
                            glm::scale(glm::mat4(1.f), glm::vec3(scale, scale, 1.f));
    _p->drawQuads(_p->textBuffer.data(), quads, model, glm::vec4(rgb, 1.f));
}

glm::vec2 Overlay::measureText(std::string_view text, float scale) const {
    std::string s(text);
    return {static_cast<float>(stb_easy_font_width(s.data())) * scale,
            static_cast<float>(stb_easy_font_height(s.data())) * scale};
}

void Overlay::drawRect(glm::vec2 px, glm::vec2 size, glm::vec4 rgba) {
    const Vertex2D v[4] = {{px.x, px.y, 0.f, {}}, {px.x + size.x, px.y, 0.f, {}},
                           {px.x + size.x, px.y + size.y, 0.f, {}}, {px.x, px.y + size.y, 0.f, {}}};
    _p->drawQuads(v, 1, glm::mat4(1.f), rgba);
}

void Overlay::strokeRect(const Rect& r, float t, glm::vec4 rgba) {
    drawRect({r.x, r.y}, {r.w, t}, rgba);
    drawRect({r.x, r.y + r.h - t}, {r.w, t}, rgba);
    drawRect({r.x, r.y}, {t, r.h}, rgba);
    drawRect({r.x + r.w - t, r.y}, {t, r.h}, rgba);
}

void Overlay::drawScoreboard(const std::vector<int>& rolls, const std::vector<int>& scores, int total,
                             const ScoreboardLayout& layout) {
    const glm::vec4 line {0.9f, 0.9f, 0.9f, 1.f};
    const glm::vec3 ink {1.f, 1.f, 1.f};
    const float textScale = layout.cell / 20.f;
    const float t = std::max(1.f, layout.cell / 30.f);

    auto centred = [&](const Rect& r, const std::string& s) {
        if (s.empty())
            return;
        const glm::vec2 size = measureText(s, textScale);
        drawText(s, {r.x + (r.w - size.x) / 2, r.y + (r.h - size.y) / 2}, textScale, ink);
    };

    for (int f = 0; f < 10; ++f) {
        const Rect fr = layout.frameRect(f);
        strokeRect(fr, t, line);
        const int cells = f == 9 ? 3 : 2;
        for (int c = 0; c < cells; ++c) {
            const int roll = 2 * f + c;
            const Rect rc = layout.rollCellRect(roll);
            if (c > 0)
                drawRect({rc.x, rc.y}, {t, rc.h}, line);
            if (roll < static_cast<int>(rolls.size()))
                centred(rc, glyphFor(rolls[static_cast<std::size_t>(roll)]));
        }
        const Rect sr = layout.scoreRect(f);
        drawRect({sr.x, sr.y}, {sr.w, t}, line);
        if (f < static_cast<int>(scores.size()))
            centred(sr, glyphFor(scores[static_cast<std::size_t>(f)], true));
    }
    const Rect totalRect {layout.originX + layout.totalWidth() + layout.cell * 0.5f, layout.originY,
                          layout.cell * 4.f, layout.cell * 2.f};
    centred(totalRect, "Total " + std::to_string(total));
}

}  // namespace Render
}  // namespace Sandbox
```

- [ ] **Step 7: Build**

Run: `cmake -S . -B build 2>&1 | tail -2 && cmake --build build -j8 2>&1 | grep -E 'error|warning' ; ctest --test-dir build | tail -2`
Expected: no diagnostics; `100% tests passed`.

If GLM triggers `-Wpedantic` diagnostics (anonymous structs), the include path is not SYSTEM: confirm `glm::glm` comes from `FetchContent_Declare(... SYSTEM)` and that no other `target_include_directories` points at `build/_deps/glm-src`.

- [ ] **Step 8: Commit**

```bash
git add libraries/Render
git commit -m "feat(render): OpenGL 3.3 context, lit/flat shaders, scene and overlay"
```

---

### Task 7: wxWidgets support and the `WX_ui` application entry

**Files:**
- Create: `cmake/WxSupport.cmake`
- Modify: `cmake/Common.cmake`, `cmake/BuildUtils.cmake:152-203`, `CMakeLists.txt`

**Interfaces:**
- Produces: option `BUILD_WX_UI` (ON); targets `wx::base`, `wx::core`, `wx::gl`, `OpenGL::GL`; `create_application(ENTRY WX_ui …)`.

- [ ] **Step 1: Install the host prerequisite**

```bash
sudo apt install -y libgtk-3-dev libgl1-mesa-dev
pkg-config --modversion gtk+-3.0
```
Expected: a GTK version ≥ 3.24.

- [ ] **Step 2: Create `cmake/WxSupport.cmake`**

```cmake
# wxWidgets 3.2 built statically from source (FetchContent). Host needs GTK3
# development headers (libgtk-3-dev) and OpenGL headers (libgl1-mesa-dev).
include(FetchContent)
find_package(OpenGL REQUIRED)

set(SANDBOX_WX_VERSION "3.2.8" CACHE STRING "wxWidgets release")

FetchContent_Declare(
    wxWidgets
    URL https://github.com/wxWidgets/wxWidgets/releases/download/v${SANDBOX_WX_VERSION}/wxWidgets-${SANDBOX_WX_VERSION}.tar.bz2
    SYSTEM
)

# wx declares its options with its own wx_option() helper, so force cache
# entries rather than relying on normal variables.
set(wxBUILD_SHARED      OFF CACHE BOOL   "" FORCE)
set(wxBUILD_SAMPLES     OFF CACHE STRING "" FORCE)
set(wxBUILD_TESTS       OFF CACHE STRING "" FORCE)
set(wxBUILD_DEMOS       OFF CACHE BOOL   "" FORCE)
set(wxBUILD_INSTALL     OFF CACHE BOOL   "" FORCE)
set(wxBUILD_MONOLITHIC  OFF CACHE BOOL   "" FORCE)
set(wxUSE_OPENGL        ON  CACHE BOOL   "" FORCE)
set(wxUSE_GLCANVAS_EGL  OFF CACHE BOOL   "" FORCE)  # GLX is what GLAD's loader expects
# Keep the dependency surface small.
foreach(feature wxUSE_WEBVIEW wxUSE_MEDIACTRL wxUSE_STC wxUSE_RIBBON wxUSE_RICHTEXT
                wxUSE_PROPGRID wxUSE_AUI wxUSE_XRC wxUSE_HTML wxUSE_SECRETSTORE
                wxUSE_LIBSDL wxUSE_LIBTIFF wxUSE_LIBLZMA)
    set(${feature} OFF CACHE BOOL "" FORCE)
endforeach()
foreach(lib wxUSE_LIBPNG wxUSE_LIBJPEG wxUSE_ZLIB wxUSE_EXPAT wxUSE_REGEX)
    set(${lib} builtin CACHE STRING "" FORCE)
endforeach()

FetchContent_MakeAvailable(wxWidgets)
```

- [ ] **Step 3: Include it from `cmake/Common.cmake`**

After the `if(BUILD_QT_UI) … endif()` block add:

```cmake
if(BUILD_WX_UI)
    include(WxSupport)
endif()
```

- [ ] **Step 4: Add the option and defaults in the root `CMakeLists.txt`**

Change `project(Sandbox VERSION 1.3.0 …)` to `1.4.0`, change `option(BUILD_QT_UI … ON)` to `OFF`, and add after it:

```cmake
option(BUILD_WX_UI           "Build wxWidgets based applications"          ON)
```

- [ ] **Step 5: Add the `WX_ui` branch to `create_application` in `cmake/BuildUtils.cmake`**

Change the FATAL_ERROR message (line 169) to:

```cmake
        message(FATAL_ERROR "${app_NAME} is missing ENTRY [Console|QT_ui|WX_ui]")
```

and insert before the final `endif()` of the `if(app_ENTRY STREQUAL "Console") … elseif(BUILD_QT_UI …)` chain:

```cmake
    elseif(BUILD_WX_UI AND app_ENTRY STREQUAL "WX_ui")
        add_executable(${app_NAME} ${app_BUILD_ARGS} ${PROJECT_FILES})
        if(DEFINED app_DEPENDENCIES AND NOT app_DEPENDENCIES STREQUAL "")
            target_link_libraries(${app_NAME} PRIVATE ${app_DEPENDENCIES})
        endif()
        set_target_properties(${app_NAME} PROPERTIES
            WIN32_EXECUTABLE TRUE
            MACOSX_BUNDLE TRUE
        )
        sandbox_set_warnings(${app_NAME})
        install(TARGETS ${app_NAME}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
```

- [ ] **Step 6: Configure and build wx (first build takes several minutes)**

Run: `cmake -S . -B build 2>&1 | tail -3 && time cmake --build build -j8 --target wxgl 2>&1 | tail -2`
Expected: `Built target wxgl`. If configure reports a missing system package for a `wxUSE_*` feature, set that feature to `OFF` in `WxSupport.cmake` and reconfigure.

- [ ] **Step 7: Verify the Qt-free default and the existing tests**

Run: `grep -E '^BUILD_(QT|WX)_UI' build/CMakeCache.txt && cmake --build build -j8 && ctest --test-dir build | tail -2`
Expected: `BUILD_QT_UI:BOOL=OFF`, `BUILD_WX_UI:BOOL=ON`, all tests pass, `build/bin/BowlingDisplay` is no longer rebuilt.

- [ ] **Step 8: Commit**

```bash
git add cmake/WxSupport.cmake cmake/Common.cmake cmake/BuildUtils.cmake CMakeLists.txt
git commit -m "build: wxWidgets 3.2 via FetchContent, WX_ui application entry, Qt off by default"
```

---

### Task 8: `BowlingGL` application

**Files:**
- Create: `applications/bowlingGame/bowlingGameGL/CMakeLists.txt`
- Create: `applications/bowlingGame/bowlingGameGL/InputMapper.h`, `InputMapper.cpp`, `test/test_InputMapper.cpp`
- Create: `applications/bowlingGame/bowlingGameGL/SceneAdapter.h`, `SceneAdapter.cpp`
- Create: `applications/bowlingGame/bowlingGameGL/BowlingCanvas.h`, `BowlingCanvas.cpp`
- Create: `applications/bowlingGame/bowlingGameGL/BowlingFrame.h`, `BowlingFrame.cpp`
- Create: `applications/bowlingGame/bowlingGameGL/main.cpp`
- Modify: `applications/bowlingGame/CMakeLists.txt:7`

**Interfaces:**
- Consumes: `Sandbox::Alley::Lane`, `Snapshot`, `Phase`, `kLane` (Task 3); `Sandbox::Render::*` (Task 6); `wx::gl`, `OpenGL::GL`, `WX_ui` entry (Task 7).
- Produces: `enum class Command { None, MoveLeft, MoveRight, AngleLeft, AngleRight, StartCharge, Release, NewGame, Quit }`; `namespace InputMapper { Command keyDown(int keyCode, bool ctrl, Phase); Command keyUp(int keyCode, Phase); Command mouseDown(bool left, Phase); Command mouseUp(bool left, Phase); float mouseDxToOffset(int dx, int canvasWidthPx); float mouseDxToAngle(int dx); constexpr float kKeyOffsetStep = 0.02f; constexpr float kKeyAngleStep = 0.25f; }`.

- [ ] **Step 1: Create `applications/bowlingGame/bowlingGameGL/CMakeLists.txt`**

```cmake
set(EXECUTABLE_NAME BowlingGL)
set(EXEC_LIST)
set(THIS_DIR ${CMAKE_CURRENT_LIST_DIR})

add_project(${EXECUTABLE_NAME} EXEC_LIST) # add_guard

if(BUILD_WX_UI AND BUILD_APPS)
    set(BUILD_${EXECUTABLE_NAME} ON PARENT_SCOPE)
else()
    set(BUILD_${EXECUTABLE_NAME} OFF PARENT_SCOPE)
endif()
set(${EXECUTABLE_NAME}_ENTRY
    WX_ui
    PARENT_SCOPE
)
set(${EXECUTABLE_NAME}_HEADERS
    ${THIS_DIR}/InputMapper.h
    ${THIS_DIR}/SceneAdapter.h
    ${THIS_DIR}/BowlingCanvas.h
    ${THIS_DIR}/BowlingFrame.h
    PARENT_SCOPE
)
set(${EXECUTABLE_NAME}_SOURCES
    ${THIS_DIR}/InputMapper.cpp
    ${THIS_DIR}/SceneAdapter.cpp
    ${THIS_DIR}/BowlingCanvas.cpp
    ${THIS_DIR}/BowlingFrame.cpp
    ${THIS_DIR}/main.cpp
    PARENT_SCOPE
)
set(${EXECUTABLE_NAME}_UI PARENT_SCOPE)
set(${EXECUTABLE_NAME}_RSC PARENT_SCOPE)
set(${EXECUTABLE_NAME}_DEPS
    ${CMAKE_ROOT_NAME}::Alley
    ${CMAKE_ROOT_NAME}::Render
    wx::gl
    wx::core
    wx::base
    OpenGL::GL
    PARENT_SCOPE
)
set(${EXECUTABLE_NAME}_BUILD_ARGS PARENT_SCOPE)

set(PROJECT_LIST
    ${PROJECT_LIST}
    ${EXEC_LIST}
    PARENT_SCOPE
)

# Window-free unit test for the input mapping.
if(BUILD_TESTING AND BUILD_WX_UI)
    build_gtest_executable(
        NAME     test_bowlinggl_input
        SRC      ${THIS_DIR}/InputMapper.cpp ${THIS_DIR}/test/test_InputMapper.cpp
        DEPENDS  ${CMAKE_ROOT_NAME}::Alley wx::core wx::base
                 GTest::gtest GTest::gmock GTest::gmock_main
        DISCOVER ON
    )
    target_include_directories(test_bowlinggl_input PRIVATE ${THIS_DIR})
endif()
```

`applications/bowlingGame/CMakeLists.txt` line 7, after `add_subdirectory(bowlingGameUI)`:

```cmake
add_subdirectory(bowlingGameGL)
```

- [ ] **Step 2: Write the failing input tests `applications/bowlingGame/bowlingGameGL/test/test_InputMapper.cpp`**

```cpp
#include "InputMapper.h"
#include <wx/defs.h>
#include <gtest/gtest.h>

using Sandbox::Alley::Phase;

TEST(test_input_mapper, arrows_move_while_aiming_or_charging) {
    EXPECT_EQ(InputMapper::keyDown(WXK_LEFT, false, Phase::Aiming), Command::MoveLeft);
    EXPECT_EQ(InputMapper::keyDown(WXK_RIGHT, false, Phase::Charging), Command::MoveRight);
    EXPECT_EQ(InputMapper::keyDown(WXK_LEFT, false, Phase::Rolling), Command::None);
}

TEST(test_input_mapper, a_and_d_rotate_the_aim) {
    EXPECT_EQ(InputMapper::keyDown('A', false, Phase::Aiming), Command::AngleLeft);
    EXPECT_EQ(InputMapper::keyDown('D', false, Phase::Aiming), Command::AngleRight);
    EXPECT_EQ(InputMapper::keyDown('D', false, Phase::GameOver), Command::None);
}

TEST(test_input_mapper, space_charges_and_releases) {
    EXPECT_EQ(InputMapper::keyDown(WXK_SPACE, false, Phase::Aiming), Command::StartCharge);
    EXPECT_EQ(InputMapper::keyDown(WXK_SPACE, false, Phase::Charging), Command::None);  // key repeat
    EXPECT_EQ(InputMapper::keyUp(WXK_SPACE, Phase::Charging), Command::Release);
    EXPECT_EQ(InputMapper::keyUp(WXK_SPACE, Phase::Aiming), Command::None);
}

TEST(test_input_mapper, new_game_and_quit_work_in_any_phase) {
    EXPECT_EQ(InputMapper::keyDown('N', false, Phase::Rolling), Command::NewGame);
    EXPECT_EQ(InputMapper::keyDown('N', true, Phase::GameOver), Command::NewGame);
    EXPECT_EQ(InputMapper::keyDown(WXK_ESCAPE, false, Phase::Settling), Command::Quit);
    EXPECT_EQ(InputMapper::keyDown('Q', true, Phase::Aiming), Command::Quit);
    EXPECT_EQ(InputMapper::keyDown('Q', false, Phase::Aiming), Command::None);
}

TEST(test_input_mapper, mouse_buttons_mirror_space) {
    EXPECT_EQ(InputMapper::mouseDown(true, Phase::Aiming), Command::StartCharge);
    EXPECT_EQ(InputMapper::mouseDown(false, Phase::Aiming), Command::None);
    EXPECT_EQ(InputMapper::mouseUp(true, Phase::Charging), Command::Release);
    EXPECT_EQ(InputMapper::mouseUp(true, Phase::Rolling), Command::None);
}

TEST(test_input_mapper, mouse_deltas_scale_to_lane_units) {
    EXPECT_NEAR(InputMapper::mouseDxToOffset(100, 1000), 0.12f, 1e-5f);  // 1.2 m across the canvas
    EXPECT_NEAR(InputMapper::mouseDxToAngle(10), 0.5f, 1e-5f);           // 0.05° per pixel
}
```

- [ ] **Step 3: Configure to verify the test fails**

Run: `cmake -S . -B build 2>&1 | tail -2 && cmake --build build -j8 --target test_bowlinggl_input 2>&1 | grep -E 'error' | head -3`
Expected: `InputMapper.h: No such file or directory`.

- [ ] **Step 4: Write `InputMapper.h` and `InputMapper.cpp`**

`applications/bowlingGame/bowlingGameGL/InputMapper.h`:

```cpp
#ifndef BOWLINGGL_INPUT_MAPPER_H
#define BOWLINGGL_INPUT_MAPPER_H
#include <Sandbox/Alley/Alley.h>

enum class Command { None, MoveLeft, MoveRight, AngleLeft, AngleRight, StartCharge, Release, NewGame, Quit };

/// Pure mapping from wx key codes / mouse buttons + current phase to commands.
namespace InputMapper {
constexpr float kKeyOffsetStep = 0.02f;  // metres per key event
constexpr float kKeyAngleStep = 0.25f;   // degrees per key event

Command keyDown(int keyCode, bool ctrl, Sandbox::Alley::Phase phase);
Command keyUp(int keyCode, Sandbox::Alley::Phase phase);
Command mouseDown(bool leftButton, Sandbox::Alley::Phase phase);
Command mouseUp(bool leftButton, Sandbox::Alley::Phase phase);
float mouseDxToOffset(int dx, int canvasWidthPx);  // 1.2 m across the whole canvas
float mouseDxToAngle(int dx);                       // 0.05° per pixel
}  // namespace InputMapper
#endif  // BOWLINGGL_INPUT_MAPPER_H
```

`applications/bowlingGame/bowlingGameGL/InputMapper.cpp`:

```cpp
#include "InputMapper.h"
#include <wx/defs.h>

using Sandbox::Alley::Phase;

namespace {
bool canAim(Phase p) {
    return p == Phase::Aiming || p == Phase::Charging;
}
}  // namespace

namespace InputMapper {

Command keyDown(int keyCode, bool ctrl, Phase phase) {
    if (keyCode == WXK_ESCAPE || (ctrl && (keyCode == 'Q' || keyCode == 'q')))
        return Command::Quit;
    if (keyCode == 'N' || keyCode == 'n')
        return Command::NewGame;
    switch (keyCode) {
        case WXK_LEFT:
            return canAim(phase) ? Command::MoveLeft : Command::None;
        case WXK_RIGHT:
            return canAim(phase) ? Command::MoveRight : Command::None;
        case 'A':
        case 'a':
            return canAim(phase) ? Command::AngleLeft : Command::None;
        case 'D':
        case 'd':
            return canAim(phase) ? Command::AngleRight : Command::None;
        case WXK_SPACE:
            return phase == Phase::Aiming ? Command::StartCharge : Command::None;
        default:
            return Command::None;
    }
}

Command keyUp(int keyCode, Phase phase) {
    if (keyCode == WXK_SPACE && phase == Phase::Charging)
        return Command::Release;
    return Command::None;
}

Command mouseDown(bool leftButton, Phase phase) {
    return (leftButton && phase == Phase::Aiming) ? Command::StartCharge : Command::None;
}

Command mouseUp(bool leftButton, Phase phase) {
    return (leftButton && phase == Phase::Charging) ? Command::Release : Command::None;
}

float mouseDxToOffset(int dx, int canvasWidthPx) {
    return 1.2f * static_cast<float>(dx) / static_cast<float>(canvasWidthPx > 0 ? canvasWidthPx : 1);
}

float mouseDxToAngle(int dx) {
    return 0.05f * static_cast<float>(dx);
}

}  // namespace InputMapper
```

- [ ] **Step 5: Run the input test**

Run: `cmake --build build -j8 --target test_bowlinggl_input 2>&1 | grep -E 'error'; ctest --test-dir build -R input --output-on-failure | tail -3`
Expected: `100% tests passed, 0 tests failed out of 6`.

- [ ] **Step 6: Write `SceneAdapter.h` and `SceneAdapter.cpp`**

Header:

```cpp
#ifndef BOWLINGGL_SCENE_ADAPTER_H
#define BOWLINGGL_SCENE_ADAPTER_H
#include <Sandbox/Alley/Alley.h>
#include <Sandbox/Render/Render.h>

/// The only place that converts Alley plain floats into Render/GLM types.
namespace SceneAdapter {
Sandbox::Render::SceneGeometry geometry(const Sandbox::Alley::LaneDimensions& d);
Sandbox::Render::Transform toTransform(const Sandbox::Alley::Pose& p);
Sandbox::Render::SceneDescription toScene(const Sandbox::Alley::Snapshot& s);
}  // namespace SceneAdapter
#endif  // BOWLINGGL_SCENE_ADAPTER_H
```

Source:

```cpp
#include "SceneAdapter.h"

using namespace Sandbox;

namespace SceneAdapter {

Render::SceneGeometry geometry(const Alley::LaneDimensions& d) {
    return {d.halfWidth, d.length, d.approach, d.gutterWidth, d.gutterDepth,
            d.ballRadius, d.pinHeight, d.pinMaxRadius, d.pinComHeight};
}

Render::Transform toTransform(const Alley::Pose& p) {
    Render::Transform t;
    t.position = {p.position[0], p.position[1], p.position[2]};
    t.rotation = glm::quat(p.rotation[3], p.rotation[0], p.rotation[1], p.rotation[2]);  // w, x, y, z
    return t;
}

Render::SceneDescription toScene(const Alley::Snapshot& s) {
    Render::SceneDescription d;
    d.ball = toTransform(s.ball);
    for (int i = 0; i < 10; ++i) {
        d.pins[static_cast<std::size_t>(i)] = toTransform(s.pins[static_cast<std::size_t>(i)]);
        d.pinVisible[static_cast<std::size_t>(i)] = s.pinInPlay[static_cast<std::size_t>(i)];
    }
    d.showAimGuide = s.phase == Alley::Phase::Aiming || s.phase == Alley::Phase::Charging;
    d.aimOffset = s.aimOffset;
    d.aimAngleDeg = s.aimAngle;
    return d;
}

}  // namespace SceneAdapter
```

- [ ] **Step 7: Write `BowlingCanvas.h` and `BowlingCanvas.cpp`**

Header:

```cpp
#ifndef BOWLINGGL_BOWLING_CANVAS_H
#define BOWLINGGL_BOWLING_CANVAS_H
#include "InputMapper.h"
#include <Sandbox/Alley/Alley.h>
#include <Sandbox/Render/Render.h>
#include <wx/wx.h>
#include <wx/glcanvas.h>
#include <functional>
#include <memory>

/// GL canvas: owns the Lane, drives it at 60 Hz, draws it, maps input.
class BowlingCanvas : public wxGLCanvas {
public:
    BowlingCanvas(wxWindow* parent, const wxGLAttributes& attrs);
    ~BowlingCanvas() override;

    void newGame();
    std::function<void(const wxString&)> onStatus;  // status bar text
    std::function<void()> onQuit;                   // close the frame

private:
    void OnPaint(wxPaintEvent&);
    void OnSize(wxSizeEvent&);
    void OnTimer(wxTimerEvent&);
    void OnKeyDown(wxKeyEvent&);
    void OnKeyUp(wxKeyEvent&);
    void OnMouse(wxMouseEvent&);

    bool ensureRenderer();
    void apply(Command command);
    void renderFrame();
    void updateCamera(const Sandbox::Alley::Snapshot& s, float dt);
    void drawHud(const Sandbox::Alley::Snapshot& s, int w, int h, int topPx, int bottomPx);
    wxString statusFor(const Sandbox::Alley::Snapshot& s) const;

    std::unique_ptr<wxGLContext> _gl;
    std::unique_ptr<Sandbox::Render::Context> _ctx;
    std::unique_ptr<Sandbox::Render::SceneRenderer> _scene;
    std::unique_ptr<Sandbox::Render::Overlay> _overlay;
    Sandbox::Render::Camera _camera;
    Sandbox::Alley::Lane _lane;
    wxTimer _timer;
    float _cameraZ = -3.5f;
    wxPoint _lastMouse;
    bool _failed = false;
};
#endif  // BOWLINGGL_BOWLING_CANVAS_H
```

Source:

```cpp
#include "BowlingCanvas.h"
#include "SceneAdapter.h"
#include <algorithm>
#include <string>

using namespace Sandbox;
using Alley::Phase;

namespace {
constexpr bool kFollowCamera = true;
constexpr float kDt = 1.f / 60.f;
constexpr float kCameraHomeZ = -3.5f;
constexpr float kCameraHeight = 1.8f;
constexpr float kCameraLookAhead = 9.f;

const char* hintFor(Phase p) {
    switch (p) {
        case Phase::Aiming:   return "<- -> move   A D angle   hold Space to charge";
        case Phase::Charging: return "release to throw";
        case Phase::GameOver: return "N: new game   Esc: quit";
        default:              return "";
    }
}

/// Gamebox marks the most recent roll slot with -1 for a spare and -2 for a strike.
int lastRollValue(const std::vector<int>& rolls) {
    for (auto it = rolls.rbegin(); it != rolls.rend(); ++it)
        if (*it != -3 && *it != -4)
            return *it;
    return -3;
}

std::string resultFor(const Alley::Snapshot& s, const Alley::Lane& lane) {
    if (s.phase == Phase::GameOver)
        return "Game over - score " + std::to_string(lane.totalScore());
    if (s.phase != Phase::FrameDone || s.lastPinsDown < 0)
        return "";
    const int last = lastRollValue(lane.rolls());
    if (last == -2 || s.lastPinsDown == 10)
        return "STRIKE!";
    if (last == -1)
        return "SPARE!";
    if (s.lastPinsDown == 0)
        return "Gutter";
    return std::to_string(s.lastPinsDown) + " pins";
}
}  // namespace

BowlingCanvas::BowlingCanvas(wxWindow* parent, const wxGLAttributes& attrs)
    : wxGLCanvas(parent, attrs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS),
      _timer(this) {
    wxGLContextAttrs ca;
    ca.PlatformDefaults().CoreProfile().OGLVersion(3, 3).EndList();
    _gl = std::make_unique<wxGLContext>(this, nullptr, &ca);
    if (!_gl->IsOK()) {
        _gl.reset();
        _failed = true;
        CallAfter([this] {
            wxMessageBox("Could not create an OpenGL 3.3 core context.", "BowlingGL", wxICON_ERROR);
            if (onQuit) onQuit();
        });
    }

    Bind(wxEVT_PAINT, &BowlingCanvas::OnPaint, this);
    Bind(wxEVT_SIZE, &BowlingCanvas::OnSize, this);
    Bind(wxEVT_TIMER, &BowlingCanvas::OnTimer, this);
    Bind(wxEVT_KEY_DOWN, &BowlingCanvas::OnKeyDown, this);
    Bind(wxEVT_KEY_UP, &BowlingCanvas::OnKeyUp, this);
    for (auto evt : {wxEVT_LEFT_DOWN, wxEVT_LEFT_UP, wxEVT_RIGHT_DOWN, wxEVT_RIGHT_UP, wxEVT_MOTION})
        Bind(evt, &BowlingCanvas::OnMouse, this);
    Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent& e) { SetFocus(); e.Skip(); });
    Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent&) {});

    _timer.Start(16);
}

BowlingCanvas::~BowlingCanvas() {
    _timer.Stop();
    // GL objects need a current context to be released.
    if (_gl && IsShownOnScreen())
        SetCurrent(*_gl);
    _overlay.reset();
    _scene.reset();
    _ctx.reset();
}

void BowlingCanvas::newGame() {
    apply(Command::NewGame);
}

bool BowlingCanvas::ensureRenderer() {
    if (_ctx)
        return true;
    auto created = Render::Context::create();
    if (created.isError()) {
        _failed = true;
        const wxString msg = wxString::FromUTF8(created.error());
        CallAfter([this, msg] {
            wxMessageBox("OpenGL initialisation failed:\n" + msg, "BowlingGL", wxICON_ERROR);
            if (onQuit) onQuit();
        });
        return false;
    }
    _ctx = created();
    _scene = std::make_unique<Render::SceneRenderer>(*_ctx, SceneAdapter::geometry(Alley::kLane));
    _overlay = std::make_unique<Render::Overlay>(*_ctx);
    const double scale = GetContentScaleFactor();
    const wxSize sz = GetClientSize();
    _ctx->resize(static_cast<int>(sz.x * scale), static_cast<int>(sz.y * scale));
    return true;
}

void BowlingCanvas::OnPaint(wxPaintEvent&) {
    wxPaintDC dc(this);
    if (_failed || !_gl || !IsShownOnScreen())
        return;
    SetCurrent(*_gl);
    if (!ensureRenderer())
        return;
    renderFrame();
    SwapBuffers();
}

void BowlingCanvas::OnSize(wxSizeEvent& e) {
    e.Skip();
    if (_ctx) {
        const double scale = GetContentScaleFactor();
        const wxSize sz = GetClientSize();
        _ctx->resize(static_cast<int>(sz.x * scale), static_cast<int>(sz.y * scale));
    }
    Refresh(false);
}

void BowlingCanvas::OnTimer(wxTimerEvent&) {
    _lane.step(kDt);
    const auto s = _lane.snapshot();
    updateCamera(s, kDt);
    if (onStatus)
        onStatus(statusFor(s));
    Refresh(false);
}

void BowlingCanvas::OnKeyDown(wxKeyEvent& e) {
    const Command c = InputMapper::keyDown(e.GetKeyCode(), e.ControlDown(), _lane.snapshot().phase);
    if (c == Command::None)
        e.Skip();
    else
        apply(c);
}

void BowlingCanvas::OnKeyUp(wxKeyEvent& e) {
    const Command c = InputMapper::keyUp(e.GetKeyCode(), _lane.snapshot().phase);
    if (c == Command::None)
        e.Skip();
    else
        apply(c);
}

void BowlingCanvas::OnMouse(wxMouseEvent& e) {
    const Phase phase = _lane.snapshot().phase;
    if (e.LeftDown() || e.RightDown()) {
        SetFocus();
        _lastMouse = e.GetPosition();
        apply(InputMapper::mouseDown(e.LeftDown(), phase));
    } else if (e.LeftUp() || e.RightUp()) {
        apply(InputMapper::mouseUp(e.LeftUp(), phase));
    } else if (e.Dragging() && e.RightIsDown()) {
        const int dx = e.GetPosition().x - _lastMouse.x;
        _lastMouse = e.GetPosition();
        if (dx != 0 && phase == Phase::Aiming)
            _lane.rotateAim(InputMapper::mouseDxToAngle(dx));
    } else if (e.Moving()) {
        const int dx = e.GetPosition().x - _lastMouse.x;
        _lastMouse = e.GetPosition();
        if (dx != 0 && phase == Phase::Aiming)
            _lane.moveAim(InputMapper::mouseDxToOffset(dx, GetClientSize().x));
    }
}

void BowlingCanvas::apply(Command command) {
    Result<> r(std::monostate {});
    switch (command) {
        case Command::MoveLeft:    r = _lane.moveAim(-InputMapper::kKeyOffsetStep); break;
        case Command::MoveRight:   r = _lane.moveAim(InputMapper::kKeyOffsetStep); break;
        case Command::AngleLeft:   r = _lane.rotateAim(-InputMapper::kKeyAngleStep); break;
        case Command::AngleRight:  r = _lane.rotateAim(InputMapper::kKeyAngleStep); break;
        case Command::StartCharge: r = _lane.startCharge(); break;
        case Command::Release:     r = _lane.release(); break;
        case Command::NewGame:     r = _lane.newGame(); break;
        case Command::Quit:
            if (onQuit) onQuit();
            return;
        case Command::None:
            return;
    }
    if (r.isError())
        wxLogDebug("command rejected: %s", r.error().c_str());
}

void BowlingCanvas::updateCamera(const Alley::Snapshot& s, float dt) {
    float target = kCameraHomeZ;
    if (kFollowCamera && (s.phase == Phase::Rolling || s.phase == Phase::Settling || s.phase == Phase::FrameDone))
        target = std::clamp(s.ball.position[2] - 3.5f, kCameraHomeZ, Alley::kLane.length - 5.5f);
    _cameraZ += (target - _cameraZ) * std::min(1.f, 6.f * dt);
    _camera.lookAt({0.f, kCameraHeight, _cameraZ}, {0.f, 0.f, _cameraZ + kCameraLookAhead});
}

void BowlingCanvas::renderFrame() {
    const int w = _ctx->width(), h = _ctx->height();
    const int topPx = static_cast<int>(h * 0.18f);
    const int bottomPx = static_cast<int>(h * 0.10f);
    const int midPx = h - topPx - bottomPx;
    const auto s = _lane.snapshot();

    _ctx->beginFrame({0.07f, 0.07f, 0.10f});
    _camera.setViewport(w, midPx);
    _scene->draw(_camera, SceneAdapter::toScene(s), bottomPx, midPx);
    _overlay->begin(w, h);
    drawHud(s, w, h, topPx, bottomPx);
    _ctx->endFrame();
}

void BowlingCanvas::drawHud(const Alley::Snapshot& s, int w, int h, int topPx, int bottomPx) {
    const float margin = w * 0.02f;
    float cell = (w - 2 * margin) / 25.f;  // 21 cells + room for the total
    cell = std::min(cell, (topPx - 2 * margin) / 2.f);
    const Render::ScoreboardLayout layout {margin, margin, cell};
    _overlay->drawScoreboard(_lane.rolls(), _lane.frameScores(), _lane.totalScore(), layout);

    // Power bar (left of the bottom band).
    const float barY = h - bottomPx * 0.7f, barH = bottomPx * 0.4f, barW = w * 0.3f;
    _overlay->strokeRect({margin, barY, barW, barH}, 2.f, {0.9f, 0.9f, 0.9f, 1.f});
    _overlay->drawRect({margin + 2.f, barY + 2.f}, {(barW - 4.f) * s.power, barH - 4.f},
                       {1.f - s.power, s.power, 0.2f, 1.f});

    // Hint (centre) and result (right).
    const float textScale = std::max(1.f, bottomPx / 40.f);
    const std::string hint = hintFor(s.phase);
    const glm::vec2 hintSize = _overlay->measureText(hint, textScale);
    _overlay->drawText(hint, {(w - hintSize.x) / 2, barY + (barH - hintSize.y) / 2}, textScale, {0.85f, 0.85f, 0.85f});
    const std::string result = resultFor(s, _lane);
    const glm::vec2 resSize = _overlay->measureText(result, textScale * 1.5f);
    _overlay->drawText(result, {w - margin - resSize.x, barY + (barH - resSize.y) / 2}, textScale * 1.5f, {1.f, 0.9f, 0.3f});
}

wxString BowlingCanvas::statusFor(const Alley::Snapshot& s) const {
    const char* phase = "";
    switch (s.phase) {
        case Phase::Aiming:    phase = "Aiming"; break;
        case Phase::Charging:  phase = "Charging"; break;
        case Phase::Rolling:   phase = "Rolling"; break;
        case Phase::Settling:  phase = "Settling"; break;
        case Phase::FrameDone: phase = "Frame done"; break;
        case Phase::GameOver:  phase = "Game over"; break;
    }
    return wxString::Format("%s   |   score %d   |   pins standing %d", phase, _lane.totalScore(),
                            static_cast<int>(std::count(s.pinStanding.begin(), s.pinStanding.end(), true)));
}
```

- [ ] **Step 8: Write `BowlingFrame.h`, `BowlingFrame.cpp` and `main.cpp`**

`BowlingFrame.h`:

```cpp
#ifndef BOWLINGGL_BOWLING_FRAME_H
#define BOWLINGGL_BOWLING_FRAME_H
#include <wx/wx.h>

class BowlingCanvas;

class BowlingFrame : public wxFrame {
public:
    BowlingFrame();

private:
    void OnNewGame(wxCommandEvent&);
    void OnQuit(wxCommandEvent&);
    void OnControls(wxCommandEvent&);
    void OnAbout(wxCommandEvent&);

    BowlingCanvas* _canvas = nullptr;
};
#endif  // BOWLINGGL_BOWLING_FRAME_H
```

`BowlingFrame.cpp`:

```cpp
#include "BowlingFrame.h"
#include "BowlingCanvas.h"
#include <wx/glcanvas.h>

BowlingFrame::BowlingFrame()
    : wxFrame(nullptr, wxID_ANY, "Bowling GL", wxDefaultPosition, wxSize(1280, 800)) {
    auto* game = new wxMenu;
    game->Append(wxID_NEW, "&New Game\tCtrl+N");
    game->AppendSeparator();
    game->Append(wxID_EXIT, "&Quit\tCtrl+Q");
    auto* help = new wxMenu;
    help->Append(wxID_HELP, "&Controls");
    help->Append(wxID_ABOUT, "&About");
    auto* bar = new wxMenuBar;
    bar->Append(game, "&Game");
    bar->Append(help, "&Help");
    SetMenuBar(bar);
    CreateStatusBar();

    Bind(wxEVT_MENU, &BowlingFrame::OnNewGame, this, wxID_NEW);
    Bind(wxEVT_MENU, &BowlingFrame::OnQuit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &BowlingFrame::OnControls, this, wxID_HELP);
    Bind(wxEVT_MENU, &BowlingFrame::OnAbout, this, wxID_ABOUT);

    wxGLAttributes attrs;
    attrs.PlatformDefaults().RGBA().DoubleBuffer().Depth(24).EndList();
    if (!wxGLCanvas::IsDisplaySupported(attrs)) {
        attrs.Reset();
        attrs.PlatformDefaults().RGBA().DoubleBuffer().Depth(16).EndList();
    }
    _canvas = new BowlingCanvas(this, attrs);
    _canvas->onStatus = [this](const wxString& text) { SetStatusText(text); };
    _canvas->onQuit = [this] { Close(true); };

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(_canvas, 1, wxEXPAND);
    SetSizer(sizer);
    _canvas->SetFocus();
}

void BowlingFrame::OnNewGame(wxCommandEvent&) {
    _canvas->newGame();
}

void BowlingFrame::OnQuit(wxCommandEvent&) {
    Close(true);
}

void BowlingFrame::OnControls(wxCommandEvent&) {
    wxMessageBox("Left / Right or mouse move: position the ball\n"
                 "A / D or right-drag: aim angle\n"
                 "Hold Space or left button: charge, release to throw\n"
                 "N: new game    Esc / Ctrl+Q: quit",
                 "Controls", wxOK | wxICON_INFORMATION, this);
}

void BowlingFrame::OnAbout(wxCommandEvent&) {
    wxMessageBox("Bowling GL\nwxWidgets + OpenGL front-end for the Gamebox score calculator.\n"
                 "Physics: Bullet3.",
                 "About", wxOK | wxICON_INFORMATION, this);
}
```

`main.cpp`:

```cpp
#include "BowlingFrame.h"
#include <wx/wx.h>

class BowlingApp : public wxApp {
public:
    bool OnInit() override {
        if (!wxApp::OnInit())
            return false;
        auto* frame = new BowlingFrame();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(BowlingApp);
```

- [ ] **Step 9: Build and run the full test suite**

Run: `cmake -S . -B build 2>&1 | tail -2 && cmake --build build -j8 2>&1 | grep -E 'error|warning' ; ctest --test-dir build | tail -3`
Expected: no diagnostics; all tests pass (Core 4 + Gamebox 16 + Alley 14 + Render 7 + input 6 = 47).

- [ ] **Step 10: Manual smoke test**

```bash
./build/bin/BowlingGL
```
Check: window with lane, 10 pins, blue ball, empty scoreboard; ← → move the ball; A/D rotate the yellow guide; hold Space → power bar oscillates; release → ball rolls, camera follows, pins fall, scoreboard fills, result text appears; second ball keeps standing pins; `N` restarts; `Esc` quits with no GL errors on stderr. Resize the window: layout rescales. On Wayland, if the canvas stays black run `GDK_BACKEND=x11 ./build/bin/BowlingGL`.

- [ ] **Step 11: Commit**

```bash
git add applications/bowlingGame/CMakeLists.txt applications/bowlingGame/bowlingGameGL
git commit -m "feat(app): BowlingGL wxWidgets/OpenGL bowling game"
```

---

### Task 9: Documentation, scaffold templates, final verification

**Files:**
- Modify: `applications/bowlingGame/README.md`
- Create: `cmake/template/app/lib_wx_ui_CMakeLists.txt.in`, `cmake/template/app/lib_Wx_UI.cpp.in`
- Modify: `cmake/modules/LibraryGenerator.cmake:103-134`, `CMakeLists.txt` (`SANDBOX_LIBRARIES`)

**Interfaces:**
- Consumes: everything above.
- Produces: README with controls, prerequisites and the manual checklist; `GENERATE_SCAFFOLD` stays harmless for `Alley`/`Render`.

- [ ] **Step 1: Rewrite `applications/bowlingGame/README.md`**

```markdown
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
2. Throw from offset ≈ 0.09 m (one `→` press ≈ 0.02 m) with ~70 % power: strike.
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
```

- [ ] **Step 2: Add wx scaffold templates**

`cmake/template/app/lib_wx_ui_CMakeLists.txt.in`:

```cmake
project(${lib_NAME}_Wx_UI VERSION 1.0.0 LANGUAGES CXX)

set(EXECUTABLE_NAME ${VAR_MARKER_START}PROJECT_NAME${VAR_MARKER_END})
add_project(${VAR_MARKER_START}EXECUTABLE_NAME${VAR_MARKER_END} PROJECT_LIST)

set(BUILD_${VAR_MARKER_START}EXECUTABLE_NAME${VAR_MARKER_END} ${VAR_MARKER_START}BUILD_APPS${VAR_MARKER_END} AND ${VAR_MARKER_START}BUILD_WX_UI${VAR_MARKER_END})
set(${VAR_MARKER_START}EXECUTABLE_NAME${VAR_MARKER_END}_ENTRY "WX_ui")
set(${VAR_MARKER_START}EXECUTABLE_NAME${VAR_MARKER_END}_HEADERS)
set(${VAR_MARKER_START}EXECUTABLE_NAME${VAR_MARKER_END}_SOURCES
    ${lib_NAME}_Wx_UI.cpp
)
set(${VAR_MARKER_START}EXECUTABLE_NAME${VAR_MARKER_END}_UI)
set(${VAR_MARKER_START}EXECUTABLE_NAME${VAR_MARKER_END}_RSC)
set(${VAR_MARKER_START}EXECUTABLE_NAME${VAR_MARKER_END}_DEPS
    ${VAR_MARKER_START}CMAKE_ROOT_NAME${VAR_MARKER_END}::${lib_NAME}
    wx::core
    wx::base
)
set(${VAR_MARKER_START}EXECUTABLE_NAME${VAR_MARKER_END}_BUILD_ARGS)

foreach(item IN LISTS PROJECT_LIST)
    if(BUILD_${VAR_MARKER_START}item${VAR_MARKER_END})
        create_application(
            NAME         ${VAR_MARKER_START}item${VAR_MARKER_END}
            ENTRY        ${VAR_MARKER_START}${VAR_MARKER_START}item${VAR_MARKER_END}_ENTRY${VAR_MARKER_END}
            HEADERS      ${VAR_MARKER_START}${VAR_MARKER_START}item${VAR_MARKER_END}_HEADERS${VAR_MARKER_END}
            SOURCES      ${VAR_MARKER_START}${VAR_MARKER_START}item${VAR_MARKER_END}_SOURCES${VAR_MARKER_END}
            RESOURCES    ${VAR_MARKER_START}${VAR_MARKER_START}item${VAR_MARKER_END}_RSC${VAR_MARKER_END}
            UI           ${VAR_MARKER_START}${VAR_MARKER_START}item${VAR_MARKER_END}_UI${VAR_MARKER_END}
            BUILD_ARGS   ${VAR_MARKER_START}${VAR_MARKER_START}item${VAR_MARKER_END}_BUILD_ARGS${VAR_MARKER_END}
            DEPENDENCIES ${VAR_MARKER_START}${VAR_MARKER_START}item${VAR_MARKER_END}_DEPS${VAR_MARKER_END}
        )
    endif()
endforeach()
```

`cmake/template/app/lib_Wx_UI.cpp.in`:

```cpp
#include <${lib_ROOT_NAME}/${lib_NAME}/${lib_NAME}.h>
#include <wx/wx.h>

class ${lib_NAME}App : public wxApp {
public:
    bool OnInit() override {
        auto* frame = new wxFrame(nullptr, wxID_ANY, "${lib_NAME}", wxDefaultPosition, wxSize(640, 480));
        frame->CreateStatusBar();
        frame->SetStatusText("${lib_NAME} wx scaffold");
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(${lib_NAME}App);
```

- [ ] **Step 3: Teach `generate_library` about wx and existing app directories**

In `cmake/modules/LibraryGenerator.cmake`, replace lines 103–134 (from `if(NOT DEFINED lib_APPS_DIR …` through the Qt `endif()`) with:

```cmake
    if(NOT DEFINED lib_APPS_DIR OR lib_APPS_DIR STREQUAL "")
        return()
    endif()

    # Hand-written applications win over scaffolds.
    if(IS_DIRECTORY ${lib_APPS_DIR}/${lib_NAME})
        message(STATUS "scaffold: apps for ${lib_NAME} already present")
        return()
    endif()

    message(STATUS "scaffold: generating apps for ${lib_NAME}")

    file(WRITE ${lib_APPS_DIR}/${lib_NAME}/CMakeLists.txt "")
    file(APPEND ${lib_APPS_DIR}/${lib_NAME}/CMakeLists.txt
        "\nadd_subdirectory(${lib_NAME}_Console)\n")

    configure_file(${lib_CFG_SRC}/app/lib_console_CMakeLists.txt.in
        ${lib_APPS_DIR}/${lib_NAME}/${lib_NAME}_Console/CMakeLists.txt)
    configure_file(${lib_CFG_SRC}/app/lib_Console.cpp.in
        ${lib_APPS_DIR}/${lib_NAME}/${lib_NAME}_Console/${lib_NAME}_Console.cpp)

    if(BUILD_WX_UI)
        file(APPEND ${lib_APPS_DIR}/${lib_NAME}/CMakeLists.txt
            "\nadd_subdirectory(${lib_NAME}_Wx_UI)\n")
        configure_file(${lib_CFG_SRC}/app/lib_wx_ui_CMakeLists.txt.in
            ${lib_APPS_DIR}/${lib_NAME}/${lib_NAME}_Wx_UI/CMakeLists.txt)
        configure_file(${lib_CFG_SRC}/app/lib_Wx_UI.cpp.in
            ${lib_APPS_DIR}/${lib_NAME}/${lib_NAME}_Wx_UI/${lib_NAME}_Wx_UI.cpp)
    endif()

    if(Qt${QT_VERSION_MAJOR}_FOUND)
        file(APPEND ${lib_APPS_DIR}/${lib_NAME}/CMakeLists.txt
            "\nadd_subdirectory(${lib_NAME}_Qt_UI)\n")

        configure_file(${lib_CFG_SRC}/app/lib_qt_ui_CMakeLists.txt.in
            ${lib_APPS_DIR}/${lib_NAME}/${lib_NAME}_Qt_UI/CMakeLists.txt)
        configure_file(${lib_CFG_SRC}/app/lib_Qt_UI.cpp.in
            ${lib_APPS_DIR}/${lib_NAME}/${lib_NAME}_Qt_UI/${lib_NAME}_Qt_UI.cpp)
        configure_file(${lib_CFG_SRC}/app/lib_mainwindow.cpp.in
            ${lib_APPS_DIR}/${lib_NAME}/${lib_NAME}_Qt_UI/mainwindow.cpp)
        configure_file(${lib_CFG_SRC}/app/lib_mainwindow.h.in
            ${lib_APPS_DIR}/${lib_NAME}/${lib_NAME}_Qt_UI/mainwindow.h)
        configure_file(${lib_CFG_SRC}/app/lib_mainwindow.ui.in
            ${lib_APPS_DIR}/${lib_NAME}/${lib_NAME}_Qt_UI/mainwindow.ui)
    endif()
```

In the root `CMakeLists.txt` set:

```cmake
set(SANDBOX_LIBRARIES
    Gamebox
    Core
    Alley
    Render
)
```

- [ ] **Step 4: Verify the scaffold is a no-op and the default build is green**

Run: `cmake -S . -B build 2>&1 | grep -E 'scaffold|error' ; git status --short | grep -v '^ M\|^A ' ; cmake --build build -j8 2>&1 | grep -E 'error|warning'; ctest --test-dir build | tail -2`
Expected: four `scaffold: … already present` lines, no untracked generated directories, no diagnostics, `100% tests passed`.

- [ ] **Step 5: Verify the GUI-free configuration**

`BuildLocation.cmake` writes all binaries under `build/` regardless of the build directory, so toggle the option in place instead of using a second build tree:

Run: `cmake -S . -B build -DBUILD_WX_UI=OFF 2>&1 | tail -2 && cmake --build build -j8 2>&1 | grep -E 'error' ; ctest --test-dir build | tail -2`
Expected: configures without touching wx, builds `BowlingConsole` and all libraries, `100% tests passed` (41 tests: no input-mapper test).

Then restore the default: `cmake -S . -B build -DBUILD_WX_UI=ON && cmake --build build -j8 && ctest --test-dir build | tail -2`
Expected: `100% tests passed` (47 tests) and `build/bin/BowlingGL` present.

- [ ] **Step 6: Commit**

```bash
git add applications/bowlingGame/README.md cmake/template/app/lib_wx_ui_CMakeLists.txt.in cmake/template/app/lib_Wx_UI.cpp.in cmake/modules/LibraryGenerator.cmake CMakeLists.txt
git commit -m "docs+build: BowlingGL README, wx scaffold templates, Alley/Render in library list"
```

---

## Self-review against the spec

- §2 architecture / dependency rules → Tasks 3, 5, 6, 8 (Alley has no GL/wx deps; Render has no Bullet dep; only the app links `OpenGL::GL` and wx). `Bowlingbox::roll` → `bool` → Task 1.
- §3.1 API → Task 3 Step 2 (with the two approved deviations: `pinInPlay`, forwarding getters). §3.2 phase machine, pin-fall rule, rack reset via `pinsLeft()`, error strings → Task 3 Step 8. §3.3 physics constants → Task 3 Step 6. §3.4 tests → Tasks 3 and 4 (plus the extra `fallen_pins_stay_visible_until_aiming`).
- §4.1 API → Task 6 Step 2 (plus `strokeRect`, `measureText`, `begin`, `SceneGeometry`). §4.2 shaders → Task 6 Step 6. §4.3 layout bands 18/72/10 % → Task 8 `renderFrame`. §4.4 tests → Task 5.
- §5 app files, game loop, controls table, overlay messages → Task 8. Follow camera is deviation 3.
- §6 build: WxSupport/BulletSupport/GlmSupport, in-tree glad/stb, `WX_ui` entry, `BUILD_WX_UI`, `BUILD_QT_UI` OFF, version 1.4.0, Gamebox 1.2.0, README, scaffold templates → Tasks 2, 7, 9. `libraries/CMakeLists.txt` already tolerates missing dirs; Task 3 adds Alley/Render to its list.
- §7 error policy → `Result<>` everywhere, `step` noexcept, fatal GL error via `wxMessageBox` (Task 8).
- §8 testing → all test files above; both `BUILD_WX_UI=ON/OFF` verified in Task 9.
- §9 acceptance → Task 8 Step 10 and README checklist.
