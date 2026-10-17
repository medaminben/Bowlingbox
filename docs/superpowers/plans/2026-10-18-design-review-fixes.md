# Design review fixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix six findings from the 2026-10-17 design review so that misused results, missing command cases and ownership mistakes fail at compile time instead of at runtime.

**Architecture:** Each task is one finding and leaves the tree building and green. Task 1 hardens `Result` in Core. Task 2 gives `IBowlingGame::roll` a `[[nodiscard]]` reason enum and keeps Score independent of Core. Tasks 3–6 are local to the two front-ends: one exhaustive command dispatch, a testable key latch, reference ownership in the console, and a single theme→sound mapping.

**Tech Stack:** C++20, CMake + Ninja, GoogleTest, wxWidgets 3.2 (BowlingGL, BowlingDisplay).

**Spec:** No separate spec file. The findings are copied into "Background" below, and `docs/ARCHITECTURE.md` describes the current design.

## Background: the findings this plan fixes

| # | Finding | Task |
|---|---|---|
| 1 | `IBowlingGame::roll` returns `bool`, while `Lane` returns `Result<>`, so the reason for a refused roll is lost | Task 2 |
| 2 | `Result`: `operator()`/`error()` move the value out on every read; there is no `[[nodiscard]]`; it lives in the global namespace; it is ambiguous when `T == ET`; it has a `static_assert` for 64-bit in a vocabulary header | Task 1 |
| 3 | Command dispatch is split between `BowlingCanvas::apply` (`if` chain plus a `default:` that assumes "ambience") and `LaneCommands::execute` | Task 3 |
| 4 | Key auto-repeat latching (`_themeKeyHeld`, …, hard-coded `'T'`/`'M'`/`'B'`) lives in the canvas and repeats `InputMapper`'s bindings | Task 4 |
| 5 | `Console::Session`/`Screen` share a `shared_ptr<IBowlingGame>` although they only use the game | Task 5 |
| 7 | `SoundDirector` includes Render only to map a `ThemeId` to an ambience | Task 6 |

### Deliberately not in this plan

- **Finding 6 (`IScoreSheet::rolls()`/`scores()` return `std::vector` by value).** The review called this harmless at this size. Changing it breaks the public interface and every test that compares vectors, and nothing gains from it. Revisit only if a profile of BowlingGL shows these calls. The fix would then be `std::array<int, kRollSlots>` / `std::array<int, kFrames>`.
- **Finding 8 (split `libraries/Render/src/Render.cpp`, 665 lines).** It is not unwieldy yet, and `ShadowMap`/`PostChain` already show the pattern to follow. Do it as part of the next render feature that touches the main pass, not as a standalone refactor.

## Global Constraints

- C++20; project targets build with `-Wall -Wextra -Wpedantic -Werror`, so every new warning is a build failure. Tasks 1–3 rely on that.
- `Score` and `Core` stay independent: neither links the other (`docs/ARCHITECTURE.md`, "Overview"). Score must not include `Bowlingbox/Core/Core.h`.
- Public API breaks bump the library's major version in its `project()` call: Core `1.1.0` → `2.0.0` (Task 1), Score `1.2.0` → `2.0.0` (Task 2). The release level of the PR is the user's choice; never add PR labels.
- All build output stays under `<repo>/build/` (existing tree: `build`, Ninja, Debug, `BUILD_WX_UI=ON`).
- Console text must not change: `test_bowlingconsole` compares it against golden output.
- The working tree has uncommitted changes that belong to the user. Before Task 1, ask the user to commit or stash them, and never add them to a task's commit.
- Commit message style: `type(scope): summary`, as in `git log`; end each message with the session's attribution trailer.
- After each task, update the matching lines in `docs/ARCHITECTURE.md` in the same commit (each task names them).

Commands used throughout:

```bash
cmake --build build -j                                           # build everything
./build/bin/<test_binary> --gtest_filter='<suite>.*'             # one suite
xvfb-run -a ctest --test-dir build -L unit --output-on-failure  # full unit suite, as in CI
```

## Review Focus

1. **Reading an error twice.** `BowlingCanvas::ensureRenderer` logs `created.error()` and then shows it in a dialog. Both reads must return the same text. Pinned by `test_core_result.error_can_be_read_twice` (Task 1).
2. **Taking a move-only value.** `Context::create()` and the GL shaders return `Result<unique_ptr<…>>` / `Result<GlShader>`. They must still be movable out of a `Result`. Pinned by `test_core_result.move_only_value_is_taken_with_std_move` (Task 1).
3. **A bad count after the game is over.** `roll(11)` and `roll(-1)` on a finished game must report `GameOver`, not `TooManyPins`/`Negative`, so callers see the real reason. Pinned by `test_score.game_over_is_reported_before_a_bad_count` (Task 2).
4. **Shift changes while a toggle key is held.** A key-down of `'t'` followed by a key-up of `'T'` must unlatch, or the theme key goes dead. Pinned by `test_key_latch.release_matches_either_case` (Task 4).
5. **Focus lost while a toggle key is held** (alt-tab with `T` down): no key-up arrives, and the next press must still work. Pinned by `test_key_latch.clear_forgets_held_keys` (Task 4), plus the `wxEVT_KILL_FOCUS` binding.

---

### Task 1: Harden `Result` (Core 2.0.0)

**Files:**
- Modify: `libraries/Core/include/Bowlingbox/Core/Core.h` (whole file)
- Modify: `libraries/Core/CMakeLists.txt:1` (version)
- Modify: `CMakeLists.txt` (root, right after `project(Bowlingbox …)` on line 7)
- Modify: `libraries/Core/test/src/test_Core.cpp` (whole file)
- Modify (compile-driven, see Step 6): `applications/bowlingGame/bowlingGameGL/LaneCommands.h`, `applications/bowlingGame/bowlingGameGL/LaneCommands.cpp`, `applications/bowlingGame/bowlingGameGL/BowlingCanvas.cpp:16,101`, `libraries/Render/src/Render.cpp:47,63,70,114-116,129-130`, `libraries/Render/src/GlShader.cpp:31,34`, `libraries/Render/test/src/test_FontAtlas.cpp:16`, `libraries/Alley/test/src/test_Alley.cpp` (23 ignored results, lines 31–35, 87–108, 162–230, 302–304), plus any other file the compiler names
- Modify: `docs/ARCHITECTURE.md:85-88` (the `Result<T, E>` bullet)

**Interfaces:**
- Produces: `Bowlingbox::Result<T = std::monostate, ET = Bowlingbox::Core::Error<>>`, a `[[nodiscard]]` class with `isValue()`, `isError()`, `T& value() &`, `const T& value() const&`, `T&& value() &&`, `error() const&` → `const E&`, `error() &&` → `E&&`. `Bowlingbox::Core::Error<E>` has `value() const&` → `const E&` and `value() &&` → `E&&`. Both `operator()`s are removed.

- [ ] **Step 1: Write the failing tests**

Replace `libraries/Core/test/src/test_Core.cpp` with:

```cpp
#include <Bowlingbox/Core/Core.h>
// Include 3rd party headers
#include <gtest/gtest.h>
// Include system headers
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

using Bowlingbox::Result;
using Bowlingbox::Core::Error;

static_assert(std::is_same_v<Result<>, Bowlingbox::Result<std::monostate, Error<std::string>>>,
              "Result lives in namespace Bowlingbox with Core::Error<> as the default error");

TEST(test_core_result, result_properties) {
    auto result = Result<int>(5);
    EXPECT_EQ(result.value(), 5);
    EXPECT_TRUE(result.isValue());
    EXPECT_FALSE(result.isError());
}

TEST(test_core_result, error_properties) {
    const auto error = Error<>("error");
    auto result = Result<int>(error);
    EXPECT_EQ(result.error(), error.value());
    EXPECT_FALSE(result.isValue());
    EXPECT_TRUE(result.isError());
}

TEST(test_core_result, void_result_properties) {
    auto ok = Result<>(std::monostate {});
    EXPECT_TRUE(ok.isValue());
    auto refused = Result<>(Error<>("refused"));
    EXPECT_TRUE(refused.isError());
    EXPECT_EQ(refused.error(), "refused");
}

TEST(test_core_result, value_can_be_read_twice) {
    auto result = Result<std::string>(std::string("pins"));
    EXPECT_EQ(result.value(), "pins");
    EXPECT_EQ(result.value(), "pins");
}

TEST(test_core_result, error_can_be_read_twice) {
    auto result = Result<int>(Error<>("no GL context"));
    EXPECT_EQ(result.error(), "no GL context");
    EXPECT_EQ(result.error(), "no GL context");
}

TEST(test_core_result, move_only_value_is_taken_with_std_move) {
    auto result = Result<std::unique_ptr<int>>(std::make_unique<int>(7));
    std::unique_ptr<int> taken = std::move(result).value();
    ASSERT_NE(taken, nullptr);
    EXPECT_EQ(*taken, 7);
}

TEST(test_core_result, const_result_is_readable) {
    const auto result = Result<int>(3);
    EXPECT_EQ(result.value(), 3);
}
```

- [ ] **Step 2: Run the test build to verify it fails**

Run: `cmake --build build --target test_bowlingbox_core`
Expected: compile errors, e.g. `'Result' is not a member of 'Bowlingbox'` and `has no member named 'value'`.

- [ ] **Step 3: Replace `Core.h`**

Replace `libraries/Core/include/Bowlingbox/Core/Core.h` with:

```cpp
#ifndef BOWLINGBOX_CORE_H
#define BOWLINGBOX_CORE_H
#include <Bowlingbox/Core/Bowlingbox_Core_export.h>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace Bowlingbox {
namespace Core {
/**
 * @brief An error wrapper.
 *
 * @tparam E Any movable error type.
 */
template <typename E = std::string>
struct Error {
public:
    constexpr Error(E error) : _error(std::move(error)) {}
    constexpr const E& value() const& noexcept { return _error; }
    constexpr E&& value() && noexcept { return std::move(_error); }

protected:
    E _error;
};
}  // namespace Core

/**
 * @brief A value or an error. Reading never moves: `r.value()` and `r.error()` return
 *        references, and `std::move(r).value()` takes the value out.
 *
 * @tparam T The value type.
 * @tparam ET The error type; it must have `value()` like Core::Error.
 */
template <typename T = std::monostate, typename ET = Core::Error<>>
class [[nodiscard]] Result {
    static_assert(!std::is_same_v<T, ET>, "Result needs distinct value and error types");

public:
    constexpr Result(T value) : _result(std::in_place_index<0>, std::move(value)) {}
    constexpr Result(ET error) : _result(std::in_place_index<1>, std::move(error)) {}
    constexpr bool isValue() const noexcept { return _result.index() == 0; }
    constexpr bool isError() const noexcept { return _result.index() == 1; }
    constexpr T& value() & { return std::get<0>(_result); }
    constexpr const T& value() const& { return std::get<0>(_result); }
    constexpr T&& value() && { return std::get<0>(std::move(_result)); }
    constexpr decltype(auto) error() const& { return std::get<1>(_result).value(); }
    constexpr decltype(auto) error() && { return std::get<1>(std::move(_result)).value(); }

private:
    std::variant<T, ET> _result;
};
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_CORE_H
```

- [ ] **Step 4: Move the 64-bit check to CMake and bump Core**

In the root `CMakeLists.txt`, directly after the `project(Bowlingbox VERSION ${BOWLINGBOX_VERSION} LANGUAGES CXX)` line, add:

```cmake
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Bowlingbox needs a 64-bit toolchain (CMAKE_SIZEOF_VOID_P is ${CMAKE_SIZEOF_VOID_P})")
endif()
```

In `libraries/Core/CMakeLists.txt`, change `project(Core VERSION 1.1.0 LANGUAGES CXX)` to `project(Core VERSION 2.0.0 LANGUAGES CXX)`.

- [ ] **Step 5: Run the Core tests to verify they pass**

Run: `cmake --build build --target test_bowlingbox_core && ./build/bin/test_bowlingbox_core`
Expected: `[  PASSED  ] 7 tests.`

- [ ] **Step 6: Fix every call site the compiler reports**

Run: `cmake --build build -j -k 0 2>&1 | grep -E 'error:' | sort -u`

Apply these rules until the build is clean:

| Compiler message | Fix |
|---|---|
| `'Result' does not name a type` / `was not declared` (code outside `namespace Bowlingbox`) | In a header write `Bowlingbox::Result<>` (e.g. `LaneCommands.h`: `Bowlingbox::Result<> execute(Command command, Bowlingbox::Alley::Lane& lane);`). In a `.cpp` add `using Bowlingbox::Result;` after the includes. Never put a `using` in a header. |
| `no match for call to '(Bowlingbox::Result<…>) ()'` where the value is **stored or returned** | `x()` → `std::move(x).value()`. Known sites: `Render.cpp:47` `d.composite = std::move(composite).value();`, `:63` `st.slot = std::move(built).value();`, `:70`, `:114-116`, `:129-130` (same form), `BowlingCanvas.cpp:16` `return std::move(device).value();`, `:101` `_ctx = std::move(created).value();`, `test_FontAtlas.cpp:16` `return std::move(baked).value();` |
| same message where the value is **only read** | `x()` → `x.value()`. Known sites: `GlShader.cpp:31` `glDeleteShader(vs.value());`, `:34` `const GLuint vsId = vs.value(), fsId = fs.value();` |
| `no match for call to '(Bowlingbox::Core::Error<…>) ()'` | `e()` → `e.value()` |
| `ignoring return value … declared with attribute 'nodiscard'` **in a test** | Wrap the call so the test checks it: `ASSERT_TRUE(lane.startCharge().isValue());` (or `EXPECT_TRUE(…)` inside a function that returns a value). In `test_Alley.cpp` this covers `throwBall` (lines 31–35), `throwAndWait` (~302–304) and the TEST bodies at 87–108 and 162–230. |
| same, **in production code** | Handle the error. None are known today: `BowlingFrame.cpp:116` calls the `void` `BowlingCanvas::newGame()`. |

If wrapping a test call in `ASSERT_TRUE` makes that test fail, the helper is being called in a state where the lane refuses the command. Read the test. If the refusal is intended, write `static_cast<void>(lane.startCharge());  // refused in GameOver on purpose`. Otherwise you have found a real test bug: stop and report it.

- [ ] **Step 7: Run the full unit suite**

Run: `cmake --build build -j && xvfb-run -a ctest --test-dir build -L unit --output-on-failure`
Expected: `100% tests passed`.

- [ ] **Step 8: Update the architecture doc**

In `docs/ARCHITECTURE.md`, replace the `Result<T, E>` bullet (lines 85–88, starting `` - `Result<T, E>`: a `std::variant` ``) with:

```markdown
- `Bowlingbox::Result<T, E>`: a value or an error, with `isValue()`, `isError()`, `value()` and
  `error()`. Reading does not move: `r.value()` returns a reference, and `std::move(r).value()`
  takes the value out. The class is `[[nodiscard]]`, so an ignored refusal fails the `-Werror`
  build. `Result<>` (value type `std::monostate`) is the return type of commands that can be
  refused. Code inside `namespace Bowlingbox` writes `Result<>`; code outside writes
  `Bowlingbox::Result<>` or adds `using Bowlingbox::Result;` in a `.cpp`.
```

Also check the build section for a mention of a 64-bit requirement, and add one line there if there is none: "The root `CMakeLists.txt` stops on a toolchain that is not 64-bit."

- [ ] **Step 9: Commit**

```bash
git add CMakeLists.txt libraries/Core libraries/Alley libraries/Render libraries/Audio applications docs/ARCHITECTURE.md
git commit -m "refactor(core): make Result nodiscard, non-destructive and namespaced

Reads no longer move the value out, ignored refusals fail the build, and
Result no longer sits in the global namespace of consumers. Core 2.0.0."
```

---

### Task 2: `roll()` reports why it refused (Score 2.0.0)

**Files:**
- Modify: `libraries/Score/include/Bowlingbox/Score/IBowlingGame.h`
- Modify: `libraries/Score/include/Bowlingbox/Score/Score.h:20`
- Modify: `libraries/Score/src/Score.cpp:15-22`
- Modify: `libraries/Score/src/Score_impl.h:3,13`
- Modify: `libraries/Score/src/Score_impl.cpp:7-17`
- Modify: `libraries/Score/CMakeLists.txt:4` (version)
- Modify: `libraries/Alley/src/Lane_impl.cpp:114`
- Modify: `applications/bowlingGame/bowlingGameCMD/session.cpp:46`
- Modify: `applications/bowlingGame/bowlingGameUI/ScoreSheetFrame.cpp:100-103`
- Modify: `ci/consumer/main.cpp`
- Test: `libraries/Score/test/src/test_Score.cpp`, plus the fakes and roll calls in `libraries/Alley/test/src/test_Alley.cpp:286`, `applications/bowlingGame/bowlingGameUI/test/test_Display.cpp:17`, `applications/bowlingGame/bowlingGameCMD/test/test_ScoreGrid.cpp:15`, `applications/bowlingGame/bowlingGameGL/test/test_SoundDirector.cpp:188-215`, `applications/bowlingGame/bowlingGameGL/test/test_Gameplay.cpp:138-202`
- Modify: `docs/ARCHITECTURE.md:42-43,76-78`

**Interfaces:**
- Consumes: nothing from Task 1 (Score must not use Core).
- Produces: `enum class [[nodiscard]] Bowlingbox::Score::RollStatus { Accepted, GameOver, Negative, TooManyPins };`, `BOWLINGBOX_SCORE_API const char* Bowlingbox::Score::describe(RollStatus)`, and `virtual RollStatus IBowlingGame::roll(int pins) = 0;`

- [ ] **Step 1: Write the failing tests**

In `libraries/Score/test/src/test_Score.cpp`, replace the two tests `roll_returns_true_while_playing` and `roll_returns_false_when_game_is_over` with:

```cpp
TEST_F(test_score, roll_is_accepted_while_playing) {
    EXPECT_EQ(bowlingGame->roll(3), RollStatus::Accepted);
    EXPECT_EQ(bowlingGame->roll(4), RollStatus::Accepted);
}

TEST_F(test_score, roll_is_refused_when_game_is_over) {
    roll_n_times(20, 0);
    EXPECT_EQ(bowlingGame->roll(1), RollStatus::GameOver);
}

TEST_F(test_score, game_over_is_reported_before_a_bad_count) {
    roll_n_times(20, 0);
    EXPECT_EQ(bowlingGame->roll(11), RollStatus::GameOver);
    EXPECT_EQ(bowlingGame->roll(-1), RollStatus::GameOver);
}

TEST(test_score_status, describe_names_every_status) {
    EXPECT_STREQ(describe(RollStatus::Accepted), "accepted");
    EXPECT_STREQ(describe(RollStatus::GameOver), "the game is over");
    EXPECT_STREQ(describe(RollStatus::Negative), "a roll cannot be negative");
    EXPECT_STREQ(describe(RollStatus::TooManyPins), "more pins than are standing");
}
```

In the same file, change the existing refusal checks to name the status:
- line ~165 `ASSERT_TRUE(game.roll(pins)) << …` → `ASSERT_EQ(game.roll(pins), RollStatus::Accepted) << …`
- line ~179 `EXPECT_FALSE(game.roll(8));` → `EXPECT_EQ(game.roll(8), RollStatus::TooManyPins);`
- lines ~231–235 →
  ```cpp
  EXPECT_EQ(game.roll(11), RollStatus::TooManyPins);
  EXPECT_EQ(game.roll(-1), RollStatus::Negative);
  EXPECT_EQ(game.roll(7), RollStatus::Accepted);
  EXPECT_EQ(game.roll(4), RollStatus::TooManyPins);  // 7 + 4 > 10 in one frame
  EXPECT_EQ(game.roll(3), RollStatus::Accepted);
  ```
- every other roll whose result is ignored (`roll_n_times`, `rollAll`, the loops at ~269, ~291, ~304, ~111–112, ~40–95, ~143) → `EXPECT_EQ(<call>, RollStatus::Accepted);`

- [ ] **Step 2: Run the test build to verify it fails**

Run: `cmake --build build --target test_bowlingbox_score`
Expected: compile error `'RollStatus' was not declared in this scope`.

- [ ] **Step 3: Add `RollStatus` to the interface**

In `libraries/Score/include/Bowlingbox/Score/IBowlingGame.h`, inside `namespace Score` before `class IScoreSheet`, add:

```cpp
/// The outcome of IBowlingGame::roll(). When a ball breaks several rules, GameOver wins.
enum class [[nodiscard]] RollStatus { Accepted, GameOver, Negative, TooManyPins };

/// A short English description, e.g. "more pins than are standing".
BOWLINGBOX_SCORE_API const char* describe(RollStatus status);
```

and change `virtual bool roll(int pins) = 0;` to `virtual RollStatus roll(int pins) = 0;`

In `Score.h:20`: `RollStatus roll(int pins) override;`

In `Score_impl.h`: add `#include <Bowlingbox/Score/IBowlingGame.h>` after the `Notation.h` include, and change `bool roll(int pins);` to `RollStatus roll(int pins);`

- [ ] **Step 4: Implement it**

Replace `BowlingSet::roll` in `libraries/Score/src/Score_impl.cpp` (and its comment) with:

```cpp
/**
 * @brief Records a ball.
 * @return Accepted, or why it was refused; the game is unchanged then.
 */
RollStatus BowlingSet::roll(int pins) {
    const Position p = position();
    if (p.frame == kFrames)
        return RollStatus::GameOver;
    if (pins < 0)
        return RollStatus::Negative;
    if (pins > p.standing)
        return RollStatus::TooManyPins;
    _balls[_count++] = pins;
    return RollStatus::Accepted;
}
```

Replace `ScoreCalculator::roll` in `libraries/Score/src/Score.cpp` with the following, and add `describe` after it:

```cpp
/**
 * @brief Rolls a number of pins in the bowling game.
 *
 * @param pins The number of pins knocked down by this ball.
 * @return Accepted, or why the ball was refused (the game is unchanged then).
 */
Bowlingbox::Score::RollStatus Bowlingbox::Score::ScoreCalculator::roll(int pins) {
    return _gameSet->roll(pins);
}

const char* Bowlingbox::Score::describe(RollStatus status) {
    switch (status) {
        case RollStatus::Accepted:    return "accepted";
        case RollStatus::GameOver:    return "the game is over";
        case RollStatus::Negative:    return "a roll cannot be negative";
        case RollStatus::TooManyPins: return "more pins than are standing";
    }
    return "unknown roll status";
}
```

In `libraries/Score/CMakeLists.txt`, change `project(Score VERSION 1.2.0 LANGUAGES CXX)` to `project(Score VERSION 2.0.0 LANGUAGES CXX)`.

- [ ] **Step 5: Run the Score tests**

Run: `cmake --build build --target test_bowlingbox_score && ./build/bin/test_bowlingbox_score`
Expected: all tests pass.

- [ ] **Step 6: Update the callers and fakes**

`libraries/Alley/src/Lane_impl.cpp:114`:
```cpp
    if (_game->roll(down) != Score::RollStatus::Accepted) {
```

`applications/bowlingGame/bowlingGameCMD/session.cpp:46` (the message text stays the same):
```cpp
    if (_mode != Mode::Playing || !pins || _game->roll(*pins) != Bowlingbox::Score::RollStatus::Accepted) {
```

`applications/bowlingGame/bowlingGameUI/ScoreSheetFrame.cpp:100-103`:
```cpp
void MainWindow::roll(int pins) {
    // refresh() disables the pin buttons above pinsLeft(), so a refusal leaves nothing to redraw.
    if (_game->roll(pins) != Bowlingbox::Score::RollStatus::Accepted)
        return;
    refresh();
}
```

`ci/consumer/main.cpp`:
```cpp
#include <Bowlingbox/Score/Score.h>

int main() {
    Bowlingbox::Score::ScoreCalculator game;
    for (int i = 0; i < 12; ++i)
        if (game.roll(10) != Bowlingbox::Score::RollStatus::Accepted)
            return 1;
    return game.score() == 300 ? 0 : 1;
}
```

`FakeGame` in `test_Alley.cpp:286`:
```cpp
    Bowlingbox::Score::RollStatus roll(int pins) override {
        if (isOver())
            return Bowlingbox::Score::RollStatus::GameOver;
        recorded.push_back(pins);
        return Bowlingbox::Score::RollStatus::Accepted;
    }
```

`CountingGame` in `test_Display.cpp:17`:
```cpp
    Bowlingbox::Score::RollStatus roll(int pins) override { return _inner.roll(pins); }
```

Ignored rolls in `test_ScoreGrid.cpp:15`, `test_SoundDirector.cpp:188-215` and `test_Gameplay.cpp:138-202` → `EXPECT_EQ(<call>, Bowlingbox::Score::RollStatus::Accepted);`

Then run `cmake --build build -j -k 0 2>&1 | grep -E 'error:|nodiscard' | sort -u` and fix anything left with the same pattern.

- [ ] **Step 7: Run the full suite and the install smoke test**

Run: `cmake --build build -j && xvfb-run -a ctest --test-dir build -L unit --output-on-failure && ci/install-smoke.sh build`
Expected: `100% tests passed` and `install smoke test passed`.

- [ ] **Step 8: Update the architecture doc**

`docs/ARCHITECTURE.md:42-43`: replace "the scoring rules report a refused roll with `bool`, not with `Core::Result`." with "the scoring rules report a refused roll with their own `RollStatus`, not with `Result`."

`docs/ARCHITECTURE.md:76`: change the sentence starting **Rules enforced.** so that it begins: "`roll()` returns `RollStatus::Accepted`, or refuses with `GameOver` (checked first), `Negative` or `TooManyPins` (more than `pinsLeft()`); `describe()` names each status." Keep the rest of the paragraph. Add `RollStatus`, `describe` to the `IBowlingGame.h` row of the Score header table.

- [ ] **Step 9: Commit**

```bash
git add libraries/Score libraries/Alley applications ci/consumer docs/ARCHITECTURE.md
git commit -m "feat(score): roll() returns a nodiscard RollStatus instead of bool

Callers learn why a ball was refused, and ignoring the outcome no longer
compiles. Score stays free of Core. Score 2.0.0."
```

---

### Task 3: One exhaustive command dispatch

**Files:**
- Modify: `applications/bowlingGame/bowlingGameGL/Command.h`
- Modify: `applications/bowlingGame/bowlingGameGL/SoundSettings.h`
- Modify: `applications/bowlingGame/bowlingGameGL/BowlingCanvas.cpp:197-223` (`apply`)
- Modify: `applications/bowlingGame/bowlingGameGL/LaneCommands.h:6-8` (comment)
- Test: `applications/bowlingGame/bowlingGameGL/test/test_Gameplay.cpp`
- Modify: `docs/ARCHITECTURE.md:235,237` (rows `Command.h`, `LaneCommands`)

**Interfaces:**
- Consumes: `Bowlingbox::Result<>` (Task 1) via `LaneCommands::execute`.
- Produces: `enum class CommandTarget { None, Lane, View, Sound, App };`, `constexpr CommandTarget targetOf(Command::Type)` in `Command.h`, and `SoundSettings SoundSettings::after(Command::Type) const`.

- [ ] **Step 1: Write the failing tests**

Add to `test_Gameplay.cpp` after `test_lane_commands.sound_commands_leave_the_lane_alone`:

```cpp
TEST(test_command_target, every_command_has_one_owner) {
    using T = Command::Type;
    EXPECT_EQ(targetOf(T::None), CommandTarget::None);
    for (auto t : {T::MoveAim, T::RotateAim, T::StartCharge, T::Release, T::NewGame})
        EXPECT_EQ(targetOf(t), CommandTarget::Lane);
    EXPECT_EQ(targetOf(T::CycleTheme), CommandTarget::View);
    for (auto t : {T::ToggleMute, T::VolumeUp, T::VolumeDown, T::ToggleAmbience})
        EXPECT_EQ(targetOf(t), CommandTarget::Sound);
    EXPECT_EQ(targetOf(T::Quit), CommandTarget::App);
}

TEST(test_sound_settings, after_applies_each_sound_command) {
    SoundSettings s;
    EXPECT_TRUE(s.after(Command::Type::ToggleMute).muted);
    EXPECT_EQ(s.after(Command::Type::VolumeUp).volume, 90);
    EXPECT_EQ(s.after(Command::Type::VolumeDown).volume, 70);
    EXPECT_FALSE(s.after(Command::Type::ToggleAmbience).ambience);
}

TEST(test_sound_settings, after_ignores_other_commands) {
    const SoundSettings s;
    for (auto t : {Command::Type::None, Command::Type::NewGame, Command::Type::CycleTheme, Command::Type::Quit})
        EXPECT_EQ(s.after(t), s);
}
```

- [ ] **Step 2: Run the test build to verify it fails**

Run: `cmake --build build --target test_bowlinggl`
Expected: compile error `'targetOf' was not declared in this scope`.

- [ ] **Step 3: Implement `targetOf` and `SoundSettings::after`**

In `Command.h`, after `struct Command { … };`, add:

```cpp
/// Which part of BowlingGL handles a command.
enum class CommandTarget { None, Lane, View, Sound, App };

/// No default case: a new Command::Type does not compile (-Wswitch, -Werror) until it has a target.
constexpr CommandTarget targetOf(Command::Type type) {
    switch (type) {
        case Command::Type::None:           return CommandTarget::None;
        case Command::Type::MoveAim:
        case Command::Type::RotateAim:
        case Command::Type::StartCharge:
        case Command::Type::Release:
        case Command::Type::NewGame:        return CommandTarget::Lane;
        case Command::Type::CycleTheme:     return CommandTarget::View;
        case Command::Type::ToggleMute:
        case Command::Type::VolumeUp:
        case Command::Type::VolumeDown:
        case Command::Type::ToggleAmbience: return CommandTarget::Sound;
        case Command::Type::Quit:           return CommandTarget::App;
    }
    return CommandTarget::None;
}
```

In `SoundSettings.h`, add `#include "Command.h"` after `#include <algorithm>`, and add this member after `gain()`:

```cpp
    /// The settings after a sound command; any other command leaves them unchanged.
    SoundSettings after(Command::Type type) const {
        SoundSettings next = *this;
        switch (type) {
            case Command::Type::ToggleMute:     next.muted = !next.muted; break;
            case Command::Type::VolumeUp:       next.stepVolume(1); break;
            case Command::Type::VolumeDown:     next.stepVolume(-1); break;
            case Command::Type::ToggleAmbience: next.ambience = !next.ambience; break;
            default:                            break;
        }
        return next;
    }
```

- [ ] **Step 4: Run the tests**

Run: `cmake --build build --target test_bowlinggl && ./build/bin/test_bowlinggl --gtest_filter='test_command_target.*:test_sound_settings.*'`
Expected: all pass.

- [ ] **Step 5: Route `BowlingCanvas::apply` through `targetOf`**

Replace `BowlingCanvas::apply` in `BowlingCanvas.cpp` with:

```cpp
void BowlingCanvas::apply(Command command) {
    switch (targetOf(command.type)) {
        case CommandTarget::None:
            return;
        case CommandTarget::App:  // Quit
            if (onQuit) onQuit();
            return;
        case CommandTarget::View:  // CycleTheme
            setTheme(Render::nextTheme(_theme));
            return;
        case CommandTarget::Sound:
            setSoundSettings(_soundSettings.after(command.type));
            return;
        case CommandTarget::Lane:
            break;
    }
    auto r = LaneCommands::execute(command, _lane);
    if (r.isError())
        wxLogDebug("command rejected: %s", r.error().c_str());
    else if (command.type == Command::Type::NewGame)
        _sound.onNewGame();
}
```

In `LaneCommands.h`, replace the comment above `namespace LaneCommands` with:

```cpp
/// Applies a CommandTarget::Lane command to the lane, using its amount for MoveAim / RotateAim.
/// Any other command succeeds without effect; BowlingCanvas::apply never passes one.
```

- [ ] **Step 6: Run the full suite and smoke-test the game by hand**

Run: `cmake --build build -j && xvfb-run -a ctest --test-dir build -L unit --output-on-failure`
Expected: `100% tests passed`.

Then run `./build/bin/BowlingGL` and check: `T` cycles the theme, `M` mutes, `+`/`-` change the volume, `B` toggles ambience, `N` starts a new game, `Esc` quits.

- [ ] **Step 7: Update the architecture doc and commit**

`docs/ARCHITECTURE.md:235` (`Command.h` row): append "; `targetOf()` sorts each type into `CommandTarget` (`Lane`, `View`, `Sound`, `App`), with no default case". `:237` (`LaneCommands` row): "`execute(Command, Lane&)` → `Result<>` for `CommandTarget::Lane` commands".

```bash
git add applications/bowlingGame/bowlingGameGL docs/ARCHITECTURE.md
git commit -m "refactor(bowlinggl): dispatch commands through one exhaustive targetOf()

A new Command::Type now fails to compile until it has an owner, and the
sound settings change is a tested pure function."
```

---

### Task 4: `InputMapper::KeyLatch` replaces the canvas key flags

**Files:**
- Modify: `applications/bowlingGame/bowlingGameGL/InputMapper.h`, `applications/bowlingGame/bowlingGameGL/InputMapper.cpp`
- Modify: `applications/bowlingGame/bowlingGameGL/BowlingCanvas.h` (remove `_themeKeyHeld`, `_muteKeyHeld`, `_ambienceKeyHeld`; add `_keys`)
- Modify: `applications/bowlingGame/bowlingGameGL/BowlingCanvas.cpp` (constructor `Bind`s, `OnKeyDown`, `OnKeyUp`)
- Test: `applications/bowlingGame/bowlingGameGL/test/test_InputMapper.cpp`
- Modify: `docs/ARCHITECTURE.md:236` (`InputMapper` row)

**Interfaces:**
- Consumes: `Command`, `Command::Type` (`Command.h`).
- Produces: `bool InputMapper::isToggle(Command::Type)`, and `class InputMapper::KeyLatch { bool accept(int keyCode, const Command&); void release(int keyCode); void clear(); }`.

- [ ] **Step 1: Write the failing tests**

Append to `test_InputMapper.cpp`:

```cpp
TEST(test_key_latch, toggles_fire_once_per_press) {
    InputMapper::KeyLatch latch;
    EXPECT_TRUE(latch.accept('T', cmd(Type::CycleTheme)));
    EXPECT_FALSE(latch.accept('T', cmd(Type::CycleTheme)));  // auto-repeat
    latch.release('T');
    EXPECT_TRUE(latch.accept('T', cmd(Type::CycleTheme)));
}

TEST(test_key_latch, movement_keys_repeat) {
    InputMapper::KeyLatch latch;
    const Command left = cmd(Type::MoveAim, -InputMapper::kKeyOffsetStep);
    EXPECT_TRUE(latch.accept(WXK_LEFT, left));
    EXPECT_TRUE(latch.accept(WXK_LEFT, left));
}

TEST(test_key_latch, keys_latch_independently) {
    InputMapper::KeyLatch latch;
    EXPECT_TRUE(latch.accept('M', cmd(Type::ToggleMute)));
    EXPECT_TRUE(latch.accept('B', cmd(Type::ToggleAmbience)));
    EXPECT_FALSE(latch.accept('M', cmd(Type::ToggleMute)));
}

TEST(test_key_latch, release_matches_either_case) {
    InputMapper::KeyLatch latch;
    EXPECT_TRUE(latch.accept('t', cmd(Type::CycleTheme)));
    latch.release('T');
    EXPECT_TRUE(latch.accept('t', cmd(Type::CycleTheme)));
}

TEST(test_key_latch, clear_forgets_held_keys) {
    InputMapper::KeyLatch latch;
    EXPECT_TRUE(latch.accept('T', cmd(Type::CycleTheme)));
    latch.clear();  // focus lost: no key-up will come
    EXPECT_TRUE(latch.accept('T', cmd(Type::CycleTheme)));
}

TEST(test_key_latch, only_theme_mute_and_ambience_are_toggles) {
    EXPECT_TRUE(InputMapper::isToggle(Type::CycleTheme));
    EXPECT_TRUE(InputMapper::isToggle(Type::ToggleMute));
    EXPECT_TRUE(InputMapper::isToggle(Type::ToggleAmbience));
    EXPECT_FALSE(InputMapper::isToggle(Type::VolumeUp));
    EXPECT_FALSE(InputMapper::isToggle(Type::NewGame));
    EXPECT_FALSE(InputMapper::isToggle(Type::None));
}
```

- [ ] **Step 2: Run the test build to verify it fails**

Run: `cmake --build build --target test_bowlinggl`
Expected: compile error `'KeyLatch' is not a member of 'InputMapper'`.

- [ ] **Step 3: Implement `KeyLatch`**

In `InputMapper.h`, add `#include <set>` after the existing includes, and add inside `namespace InputMapper`, after `mouseMove`:

```cpp
/// Theme, mute and ambience switch state, so they fire once per key press.
bool isToggle(Command::Type type);

/// wx repeats key-down while a key is held; for toggles that would flash through themes.
class KeyLatch {
public:
    /// False for a repeated key-down of a held toggle key, true for everything else.
    bool accept(int keyCode, const Command& command);
    void release(int keyCode);
    void clear() { _held.clear(); }  // focus lost: the key-ups will not arrive

private:
    std::set<int> _held;  // upper-cased key codes
};
```

In `InputMapper.cpp`, extend the anonymous namespace and add the definitions inside `namespace InputMapper`:

```cpp
namespace {
bool canAim(Phase p) {
    return p == Phase::Aiming || p == Phase::Charging;
}

int upperCased(int keyCode) {
    return (keyCode >= 'a' && keyCode <= 'z') ? keyCode - 'a' + 'A' : keyCode;
}
}  // namespace
```

```cpp
bool isToggle(Type type) {
    return type == Type::CycleTheme || type == Type::ToggleMute || type == Type::ToggleAmbience;
}

bool KeyLatch::accept(int keyCode, const Command& command) {
    if (!isToggle(command.type))
        return true;
    return _held.insert(upperCased(keyCode)).second;
}

void KeyLatch::release(int keyCode) {
    _held.erase(upperCased(keyCode));
}
```

- [ ] **Step 4: Run the tests**

Run: `cmake --build build --target test_bowlinggl && ./build/bin/test_bowlinggl --gtest_filter='test_key_latch.*:test_input_mapper.*'`
Expected: all pass.

- [ ] **Step 5: Use the latch in the canvas**

In `BowlingCanvas.h`: add `#include "InputMapper.h"`. Delete the three members `_themeKeyHeld`, `_muteKeyHeld`, `_ambienceKeyHeld` with their comments, and add in their place:

```cpp
    InputMapper::KeyLatch _keys;  // toggle keys fire once per press
```

In `BowlingCanvas.cpp`, after `Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent&) {});` in the constructor, add:

```cpp
    Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& e) { _keys.clear(); e.Skip(); });
```

Replace `OnKeyDown` and `OnKeyUp` with:

```cpp
void BowlingCanvas::OnKeyDown(wxKeyEvent& e) {
    const Command c = InputMapper::keyDown(e.GetKeyCode(), e.ControlDown(), _lane.snapshot().phase);
    if (c.type == Command::Type::None) {
        e.Skip();
        return;
    }
    if (_keys.accept(e.GetKeyCode(), c))
        apply(c);
}

void BowlingCanvas::OnKeyUp(wxKeyEvent& e) {
    _keys.release(e.GetKeyCode());
    const Command c = InputMapper::keyUp(e.GetKeyCode(), _lane.snapshot().phase);
    if (c.type == Command::Type::None)
        e.Skip();
    else
        apply(c);
}
```

- [ ] **Step 6: Run the full suite and check by hand**

Run: `cmake --build build -j && xvfb-run -a ctest --test-dir build -L unit --output-on-failure`
Expected: `100% tests passed`.

Run `./build/bin/BowlingGL`:
- holding `T` changes the theme once, and releasing and pressing it again changes it again
- holding `←` keeps moving the ball
- hold `T`, alt-tab away, come back: the next `T` press changes the theme

- [ ] **Step 7: Update the architecture doc and commit**

`docs/ARCHITECTURE.md:236` (`InputMapper` row): append "; `KeyLatch` lets theme, mute and ambience fire once per key press (wx auto-repeats key-down)".

```bash
git add applications/bowlingGame/bowlingGameGL docs/ARCHITECTURE.md
git commit -m "refactor(bowlinggl): move toggle-key latching into a tested InputMapper::KeyLatch

Key bindings live only in InputMapper again, and losing focus while a
toggle key is held no longer swallows the next press."
```

---

### Task 5: The console borrows the game instead of sharing it

**Files:**
- Modify: `applications/bowlingGame/bowlingGameCMD/session.h`, `session.cpp:29`
- Modify: `applications/bowlingGame/bowlingGameCMD/bowlingScreen.h:16`, `bowlingScreen.cpp:8-9`
- Modify: `applications/bowlingGame/bowlingGameCMD/console.cpp`
- Test: `applications/bowlingGame/bowlingGameCMD/test/test_Session.cpp`, `test/test_Console.cpp`
- Modify: `docs/ARCHITECTURE.md:248`

**Interfaces:**
- Consumes: `RollStatus` (Task 2), already used in `session.cpp`.
- Produces: `explicit Console::Session(Bowlingbox::Score::IBowlingGame& game)` and `Console::Screen(Bowlingbox::Score::IBowlingGame& game, ITerminal& terminal)`. The caller owns the game and must keep it alive longer than both.

- [ ] **Step 1: Change the tests to the new constructors**

In `test_Session.cpp`, replace the fixture with:

```cpp
class test_session : public ::testing::Test {
public:
    ScoreCalculator game;
    Console::Session session {game};
};
```

Replace every `game->` with `game.` in that file, remove `#include <memory>`, and add:

```cpp
TEST(test_session_ownership, session_uses_a_game_owned_by_the_caller) {
    ScoreCalculator game;
    {
        Console::Session session {game};
        session.handle("p");
        session.handle("4");
    }
    EXPECT_EQ(game.rolls()[0], 4);  // the session wrote into the caller's game
}
```

In `test_Console.cpp`, replace each pair

```cpp
    auto game = std::make_shared<Bowlingbox::Score::ScoreCalculator>();
    Console::Screen screen(game, terminal);
```

with

```cpp
    Bowlingbox::Score::ScoreCalculator game;
    Console::Screen screen(game, terminal);
```

and replace every `game->` with `game.`.

- [ ] **Step 2: Run the test build to verify it fails**

Run: `cmake --build build --target test_bowlingconsole`
Expected: compile error `no matching function for call to 'Console::Session::Session(<brace-enclosed initializer list>)'`.

- [ ] **Step 3: Switch `Session` and `Screen` to references**

`session.h`: remove `#include <memory>`; change the constructor to `explicit Session(Bowlingbox::Score::IBowlingGame& game);`, the member to `Bowlingbox::Score::IBowlingGame& _game;`, and `sheet()` to `{ return _game; }`.

`session.cpp:29`: `Session::Session(Bowlingbox::Score::IBowlingGame& game) : _game(game) {}`. Remove `#include <utility>` if nothing else uses it, and replace every `_game->` with `_game.`.

`bowlingScreen.h`: remove `#include <memory>`; change the constructor to `Screen(Bowlingbox::Score::IBowlingGame& game, ITerminal& terminal);` and its comment to "/// Draws and runs a game owned by the caller; `game` and `terminal` must outlive the Screen."

`bowlingScreen.cpp:8-9`:
```cpp
Screen::Screen(Bowlingbox::Score::IBowlingGame& game, ITerminal& terminal)
    : _session(game), _terminal(terminal) {}
```

`console.cpp`:
```cpp
#include <Bowlingbox/Score/Score.h>
#include "bowlingScreen.h"
#include "terminal.h"

int main(void) {
    // Setup: the concrete collaborators are chosen here and nowhere else, and live as long as main.
    Bowlingbox::Score::ScoreCalculator game;
    Console::SystemTerminal terminal;
    Console::Screen screen(game, terminal);
    // loop
    screen.start();
    return 0;
}
```

- [ ] **Step 4: Run the console tests**

Run: `cmake --build build --target test_bowlingconsole && ./build/bin/test_bowlingconsole`
Expected: all pass, golden output unchanged.

- [ ] **Step 5: Run the full suite**

Run: `cmake --build build -j && xvfb-run -a ctest --test-dir build -L unit --output-on-failure`
Expected: `100% tests passed`.

- [ ] **Step 6: Update the architecture doc and commit**

`docs/ARCHITECTURE.md:248`: change "`Console::Screen` (`bowlingScreen.*`) receives an `IBowlingGame` and an `ITerminal`" to "`Console::Screen` (`bowlingScreen.*`) borrows an `IBowlingGame&` and an `ITerminal&` that `main` owns".

```bash
git add applications/bowlingGame/bowlingGameCMD docs/ARCHITECTURE.md
git commit -m "refactor(console): borrow the game by reference instead of shared_ptr

main owns the ScoreCalculator, as BowlingGL owns its audio output."
```

---

### Task 6: Theme ambience mapping leaves `SoundDirector`

**Files:**
- Create: `applications/bowlingGame/bowlingGameGL/ThemeSounds.h`, `applications/bowlingGame/bowlingGameGL/ThemeSounds.cpp`
- Modify: `applications/bowlingGame/bowlingGameGL/CMakeLists.txt:12` (add the new files to `bowlinggl_core`)
- Modify: `applications/bowlingGame/bowlingGameGL/SoundDirector.h` (drop the Render include; `setTheme` → `setAmbient`; `_theme` → `_ambient`)
- Modify: `applications/bowlingGame/bowlingGameGL/SoundDirector.cpp:24-31,89-106`
- Modify: `applications/bowlingGame/bowlingGameGL/BowlingCanvas.cpp:67-68`
- Test: `applications/bowlingGame/bowlingGameGL/test/test_SoundDirector.cpp:222-262`
- Modify: `docs/ARCHITECTURE.md:241` (`SoundDirector` row) and the BowlingGL unit table

**Interfaces:**
- Produces: `Bowlingbox::Audio::SoundId ambientFor(Bowlingbox::Render::ThemeId id);` (`ThemeSounds.h`), and `void SoundDirector::setAmbient(Bowlingbox::Audio::SoundId ambient, bool announce);`

- [ ] **Step 1: Write the failing tests**

In `test_SoundDirector.cpp`, add `#include "ThemeSounds.h"` after `#include "SoundDirector.h"`. Rename and rewrite the theme test:

```cpp
TEST(test_sound_director, ambient_sets_the_loop_and_announces_only_when_asked) {
    Fixture f;
    f.director.setAmbient(SoundId::AmbientClassic, false);
    EXPECT_EQ(f.out.loop(LoopSlot::Ambient), SoundId::AmbientClassic);
    EXPECT_NEAR(f.out.loopVoice(LoopSlot::Ambient).volume, SoundDirector::kAmbientVolume, 1e-5f);
    EXPECT_EQ(f.out.count(SoundId::ThemeSwitch), 0);
    f.director.setAmbient(SoundId::AmbientOutdoor, true);
    EXPECT_EQ(f.out.loop(LoopSlot::Ambient), SoundId::AmbientOutdoor);
    EXPECT_EQ(f.out.count(SoundId::ThemeSwitch), 1);
    f.director.setAmbient(SoundId::AmbientNeon, true);
    EXPECT_EQ(f.out.loop(LoopSlot::Ambient), SoundId::AmbientNeon);
}

TEST(test_theme_sounds, every_theme_has_its_own_ambience) {
    EXPECT_EQ(ambientFor(Render::ThemeId::NeonHall), SoundId::AmbientNeon);
    EXPECT_EQ(ambientFor(Render::ThemeId::ClassicAlley), SoundId::AmbientClassic);
    EXPECT_EQ(ambientFor(Render::ThemeId::Outdoor), SoundId::AmbientOutdoor);
}
```

In the other tests in that file, replace every `f.director.setTheme(Render::ThemeId::X, b)` with `f.director.setAmbient(SoundId::AmbientY, b)`, using NeonHall→`AmbientNeon`, ClassicAlley→`AmbientClassic`, Outdoor→`AmbientOutdoor`.

- [ ] **Step 2: Run the test build to verify it fails**

Run: `cmake --build build --target test_bowlinggl`
Expected: compile error `ThemeSounds.h: No such file or directory`.

- [ ] **Step 3: Create `ThemeSounds`**

`applications/bowlingGame/bowlingGameGL/ThemeSounds.h`:

```cpp
#ifndef BOWLINGGL_THEME_SOUNDS_H
#define BOWLINGGL_THEME_SOUNDS_H
#include <Bowlingbox/Audio/Audio.h>
#include <Bowlingbox/Render/Environment.h>

/// The ambience loop of each theme: the one place where Render themes meet Audio sounds.
Bowlingbox::Audio::SoundId ambientFor(Bowlingbox::Render::ThemeId id);
#endif  // BOWLINGGL_THEME_SOUNDS_H
```

`applications/bowlingGame/bowlingGameGL/ThemeSounds.cpp`:

```cpp
#include "ThemeSounds.h"

using Bowlingbox::Audio::SoundId;
using Bowlingbox::Render::ThemeId;

SoundId ambientFor(ThemeId id) {
    switch (id) {
        case ThemeId::NeonHall:     return SoundId::AmbientNeon;
        case ThemeId::ClassicAlley: return SoundId::AmbientClassic;
        case ThemeId::Outdoor:      return SoundId::AmbientOutdoor;
    }
    return SoundId::AmbientNeon;
}
```

In `CMakeLists.txt`, add `ThemeSounds.h ThemeSounds.cpp` on a new line after `SoundSettings.h` in `add_library(bowlinggl_core STATIC …)`.

- [ ] **Step 4: Change `SoundDirector`**

`SoundDirector.h`: remove `#include <Bowlingbox/Render/Environment.h>`; replace `void setTheme(Bowlingbox::Render::ThemeId id, bool announce);` with

```cpp
    /// Plays `ambient` as the background loop (if ambience is on); `announce` adds the theme chime.
    void setAmbient(Bowlingbox::Audio::SoundId ambient, bool announce);
```

and replace the member `std::optional<Bowlingbox::Render::ThemeId> _theme;  // unset until the first setTheme` with

```cpp
    std::optional<Bowlingbox::Audio::SoundId> _ambient;  // unset until the first setAmbient
```

`SoundDirector.cpp`: delete `ambientFor` (lines 24–31), and replace `setTheme` and `updateAmbience` with:

```cpp
void SoundDirector::setAmbient(SoundId ambient, bool announce) {
    _ambient = ambient;
    updateAmbience();
    if (announce)
        _out.play(SoundId::ThemeSwitch, {kChimeVolume, 1.f, 0.f});
}
```

```cpp
void SoundDirector::updateAmbience() {
    if (_ambience && _ambient)
        _out.setLoop(LoopSlot::Ambient, *_ambient, {kAmbientVolume, 1.f, 0.f});
    else
        _out.setLoop(LoopSlot::Ambient, std::nullopt, {});
}
```

`BowlingCanvas.cpp`: add `#include "ThemeSounds.h"` after `#include "SceneAdapter.h"`, and in `setTheme` change `_sound.setTheme(id, _themeChosen);` to `_sound.setAmbient(ambientFor(id), _themeChosen);`.

- [ ] **Step 5: Run the tests and confirm Render is gone from SoundDirector**

Run: `cmake --build build --target test_bowlinggl && ./build/bin/test_bowlinggl --gtest_filter='test_sound_director.*:test_theme_sounds.*'`
Expected: all pass.

Run: `grep -n 'Render' applications/bowlingGame/bowlingGameGL/SoundDirector.h applications/bowlingGame/bowlingGameGL/SoundDirector.cpp`
Expected: no output.

- [ ] **Step 6: Run the full suite and listen**

Run: `cmake --build build -j && xvfb-run -a ctest --test-dir build -L unit --output-on-failure`
Expected: `100% tests passed`.

Run `./build/bin/BowlingGL`: the first theme starts its ambience without a chime, and `T` switches the ambience and chimes.

- [ ] **Step 7: Update the architecture doc and commit**

`docs/ARCHITECTURE.md:241` (`SoundDirector` row): replace "ambience" at the end of the list with "the ambience loop it is given". Add a row after it to the BowlingGL unit table: `` | `ThemeSounds` | `ambientFor(ThemeId)`: the ambience loop of each theme, so SoundDirector does not depend on Render | ``.

```bash
git add applications/bowlingGame/bowlingGameGL docs/ARCHITECTURE.md
git commit -m "refactor(bowlinggl): map themes to ambience outside SoundDirector

SoundDirector now speaks only Audio and Alley; ThemeSounds is the one
place that ties a Render theme to a sound."
```
