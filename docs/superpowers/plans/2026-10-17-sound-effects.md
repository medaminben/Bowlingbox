# Sound Effects Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** BowlingGL plays a rolling ball, pin hits and clatter, a gutter drop, strike / spare / miss stingers, control feedback and a per-theme ambience, with a mute toggle.

**Architecture:** Bullet reports new contacts as `Alley::Impact` events through `IPhysicsWorld::drainImpacts()`. A new `Bowlingbox::Audio` library wraps miniaudio behind `IAudioOutput` (with a silent fallback) and embeds 14 CC0 WAV samples. A pure `SoundDirector` in `bowlinggl_core` turns snapshots, the score sheet and impacts into cues; `BowlingCanvas` wires it into the timer and adds mute controls.

**Tech Stack:** C++20, CMake ≥ 3.29, Bullet3 3.25, miniaudio 0.11.25 (vendored), stb_vorbis (vendored, dev tool only), wxWidgets 3.2, GoogleTest.

**Spec:** `docs/superpowers/specs/2026-10-17-sound-effects-design.md`

## Global Constraints

- Language and warnings: C++20; project targets build with `-Wall -Wextra -Wpedantic -Werror` (`bowlingbox_set_warnings`). Unused parameters stay unnamed.
- miniaudio is pinned to tag `0.11.25`; `stb_vorbis.c` to stb commit `2c980bb59875b0d32144a71867fbdebb2f77cd20`.
- The shipped miniaudio build defines `MA_NO_ENCODING`, `MA_NO_GENERATION`, `MA_NO_NULL`; it must not define `MA_NO_ENGINE`, `MA_NO_NODE_GRAPH` or `MA_NO_RESOURCE_MANAGER`.
- Assets: CC0 only; mono, 22050 Hz, signed 16-bit WAV; one-shots 0.05–3 s; loops 0.5–12 s; all of `3rdparty/sounds/*.wav` together < 3 MB; each file listed in `3rdparty/sounds/SOURCES.md`.
- No test opens an audio device. CI and headless runs must pass.
- Physics results must not change: every existing test in `test_bowlingbox_alley` stays green unmodified.
- Scope: BowlingGL only. Console and BowlingDisplay are untouched.
- Work happens in a worktree on branch `feature/sound-effects` created from the current `feature/modern-graphics` HEAD. The main checkout has unrelated uncommitted edits; never stage files a task does not list.
- Build and test commands (from the worktree root): `cmake -S . -B build && cmake --build build -j` and the test binaries in `build/bin/` (`test_bowlingbox_alley`, `test_bowlingbox_audio`, `test_bowlinggl`), or `ctest --test-dir build`.

## Review Focus

1. **Spurious clatter at release.** The first Bullet sub-step after a rack reset creates new resting contacts for all ten pins (~0.12 N·s each). Expected: silence until the ball reaches the rack. Pinned by `no_pin_impacts_before_the_ball_reaches_the_rack` (Task 1).
2. **Impact flood on a strike.** A strike produces dozens of contacts per tick. Expected: at most 4 impact sounds per tick and no machine-gun effect across consecutive ticks; Bullet buffer capped at 256. Pinned by `at_most_four_impact_sounds_per_update_strongest_first`, `the_same_kind_waits_for_its_cooldown` (Task 6) and `a_pocket_throw_reports_ball_pin_impacts` (Task 1).
3. **Ten after a gutter ball.** Expected: the Spare stinger, matching the HUD's "SPARE!". Pinned by `throw_result_ten_after_a_gutter_ball_is_a_spare` (Task 5) and `ten_after_a_gutter_ball_plays_the_spare_stinger` (Task 6).
4. **No audio device.** Expected: the game starts, prints exactly one `sound disabled: ...` line on stderr and runs silently. `MA_NO_NULL` makes `ma_engine_init` fail instead of picking the null backend; the canvas falls back to `makeSilentOutput()`. Pinned by `silent_output_tracks_mute_and_ignores_cues` (Task 4) plus the manual check in Task 7.
5. **Holding `M`.** Key auto-repeat must not flicker mute on and off. Expected: one toggle per press. Pinned by `m_toggles_mute_in_every_phase` (Task 5) and the auto-repeat guard plus manual check in Task 7.

---

### Task 1: Alley impact events

**Files:**
- Modify: `libraries/Alley/include/Bowlingbox/Alley/Types.h`
- Modify: `libraries/Alley/include/Bowlingbox/Alley/PhysicsWorld.h`
- Modify: `libraries/Alley/include/Bowlingbox/Alley/Alley.h`
- Modify: `libraries/Alley/src/Alley.cpp`
- Modify: `libraries/Alley/src/Lane_impl.h`
- Modify: `libraries/Alley/src/BulletWorld.h`
- Modify: `libraries/Alley/src/BulletWorld.cpp`
- Test: `libraries/Alley/test/src/test_Alley.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces:
  - `enum class Bowlingbox::Alley::Body { Ball, Pin, Lane, Gutter, Pit, Other };`
  - `struct Bowlingbox::Alley::Impact { Body a; Body b; float impulse; std::array<float, 3> position; };` with `a <= b` from Bullet.
  - `virtual std::vector<Impact> IPhysicsWorld::drainImpacts() = 0;`
  - `std::vector<Impact> Lane::drainImpacts();`

- [ ] **Step 1: Write the failing tests**

In `test_Alley.cpp`, add `#include <utility>` to the includes. In the `FakeWorld` class add a public member and the override (next to `allSlow`):

```cpp
    std::vector<Impact> queued;
    std::vector<Impact> drainImpacts() override { return std::exchange(queued, {}); }
```

Add these tests. The first three go after the last `TEST_F(test_alley, ...)` that uses the real Bullet lane (before the `// ---- injected collaborators` block); the last one goes at the end of the file.

```cpp
TEST_F(test_alley, a_pocket_throw_reports_ball_pin_impacts) {
    lane.moveAim(kPocketOffset);
    lane.startCharge();
    lane.step(chargeTimeFor(kStrikePower));
    lane.release();
    bool ballHitPin = false;
    for (int i = 0; i < 60 * 4 && !ballHitPin; ++i) {
        lane.step(kDt);
        const auto impacts = lane.drainImpacts();
        EXPECT_LE(impacts.size(), 256u);
        for (const Impact& im : impacts) {
            EXPECT_LE(im.a, im.b);
            EXPECT_GE(im.impulse, 0.3f);
            if (im.a == Body::Ball && im.b == Body::Pin)
                ballHitPin = true;
        }
    }
    EXPECT_TRUE(ballHitPin);
    EXPECT_TRUE(lane.drainImpacts().empty());
}

TEST_F(test_alley, no_pin_impacts_before_the_ball_reaches_the_rack) {
    lane.startCharge();
    lane.step(chargeTimeFor(0.05f));  // 4.4 m/s: about 4 s to the head pin
    lane.release();
    for (int i = 0; i < 60; ++i)
        lane.step(kDt);
    for (const Impact& im : lane.drainImpacts()) {
        EXPECT_NE(im.a, Body::Pin);
        EXPECT_NE(im.b, Body::Pin);
    }
}

TEST_F(test_alley, new_game_drops_pending_impacts) {
    lane.moveAim(kPocketOffset);
    lane.startCharge();
    lane.step(chargeTimeFor(kStrikePower));
    lane.release();
    for (int i = 0; i < 60 * 3; ++i)
        lane.step(kDt);
    lane.newGame();
    EXPECT_TRUE(lane.drainImpacts().empty());
}

TEST(test_alley_injected, impacts_are_forwarded_once) {
    auto l = makeInjectedLane();
    l.world->queued.push_back({Body::Ball, Body::Pin, 2.f, {0.f, 0.2f, 17.4f}});
    const auto first = l.lane->drainImpacts();
    ASSERT_EQ(first.size(), 1u);
    EXPECT_EQ(first[0].a, Body::Ball);
    EXPECT_EQ(first[0].b, Body::Pin);
    EXPECT_FLOAT_EQ(first[0].impulse, 2.f);
    EXPECT_TRUE(l.lane->drainImpacts().empty());
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `cmake --build build --target test_bowlingbox_alley 2>&1 | tail -5`
Expected: compile errors such as `'Impact' was not declared` / `'drainImpacts' is not a member of 'Bowlingbox::Alley::Lane'`.

- [ ] **Step 3: Add the types and interfaces**

In `Types.h`, after `struct Pose { ... };` add:

```cpp
/// What a rigid body is, for impact reporting.
enum class Body { Ball, Pin, Lane, Gutter, Pit, Other };

/// A new contact between two bodies in one physics sub-step.
struct Impact {
    Body a = Body::Other;  // a <= b in enum order
    Body b = Body::Other;
    float impulse = 0.f;                            // N·s, Bullet applied impulse
    std::array<float, 3> position {0.f, 0.f, 0.f};  // world, metres
};
```

In `PhysicsWorld.h`, add `#include <vector>` and, after `allSlow()`:

```cpp
    /// Impacts recorded since the last call, oldest first; empties the buffer.
    virtual std::vector<Impact> drainImpacts() = 0;
```

In `Alley.h`, add `#include <vector>` and in the `// Read.` group:

```cpp
    /// New physics contacts since the last call (for sound), oldest first.
    std::vector<Impact> drainImpacts();
```

In `Alley.cpp`, after `scoreSheet()`:

```cpp
std::vector<Impact> Lane::drainImpacts() { return _impl->drainImpacts(); }
```

In `Lane_impl.h`, after `snapshot()`:

```cpp
    std::vector<Impact> drainImpacts() { return _world->drainImpacts(); }
```

In `BulletWorld.h`, after `bool allSlow() const override;`:

```cpp
    std::vector<Impact> drainImpacts() override;
```

- [ ] **Step 4: Record impacts in Bullet**

In `BulletWorld.cpp`:

Add `#include <algorithm>` and `#include <utility>` to the includes and, in the anonymous namespace after `kStandingCos`:

```cpp
constexpr float kImpactRecordThreshold = 0.3f;  // N·s; a resting pin pushes ~0.12 per sub-step
constexpr std::size_t kMaxImpacts = 256;

Body tagOf(const btCollisionObject* o) {
    const int i = o->getUserIndex();
    return (i >= 0 && i <= static_cast<int>(Body::Other)) ? static_cast<Body>(i) : Body::Other;
}
```

In `struct BulletWorld::Impl`, add a member after `inPlay`:

```cpp
    std::vector<Impact> impacts;
```

Change the constructor to register the tick callback:

```cpp
    Impl() {
        build();
        world.setInternalTickCallback(&Impl::onTick, this);
    }
```

Add these members to `Impl` (after `slow`):

```cpp
    static void onTick(btDynamicsWorld* w, btScalar) {
        static_cast<Impl*>(w->getWorldUserInfo())->collectImpacts();
    }

    /// Bullet refreshes a new contact point once in the sub-step that adds it,
    /// so lifetime <= 1 means "new in this sub-step".
    void collectImpacts() {
        const int manifolds = dispatcher.getNumManifolds();
        for (int m = 0; m < manifolds; ++m) {
            const btPersistentManifold* pm = dispatcher.getManifoldByIndexInternal(m);
            const Body a = tagOf(pm->getBody0());
            const Body b = tagOf(pm->getBody1());
            for (int c = 0; c < pm->getNumContacts(); ++c) {
                const btManifoldPoint& pt = pm->getContactPoint(c);
                if (pt.getLifeTime() > 1 || pt.getAppliedImpulse() < kImpactRecordThreshold)
                    continue;
                if (impacts.size() >= kMaxImpacts)
                    return;
                const btVector3& p = pt.getPositionWorldOnB();
                Impact im {std::min(a, b), std::max(a, b), static_cast<float>(pt.getAppliedImpulse()),
                           {static_cast<float>(p.x()), static_cast<float>(p.y()), static_cast<float>(p.z())}};
                impacts.push_back(im);
            }
        }
    }
```

Give every body a tag. Change `addStatic` to take the tag first and set it:

```cpp
    void addStatic(Body tag, btCollisionShape* shape, const btTransform& t, float friction,
                   float rolling, float restitution) {
        motions.push_back(std::make_unique<btDefaultMotionState>(t));
        btRigidBody::btRigidBodyConstructionInfo info(0.f, motions.back().get(), shape);
        info.m_friction = friction;
        info.m_rollingFriction = rolling;
        info.m_restitution = restitution;
        statics.push_back(std::make_unique<btRigidBody>(info));
        statics.back()->setUserIndex(static_cast<int>(tag));
        world.addRigidBody(statics.back().get());
    }
```

Update the calls in `build()`: the lane box gets `Body::Lane`; both gutter boxes and both gutter outer walls get `Body::Gutter`; the pit floor, back wall and both pit side walls get `Body::Pit`. For example:

```cpp
        addStatic(Body::Lane, keep(std::make_unique<btBoxShape>(btVector3(d.halfWidth, 0.05f, laneLen / 2))),
                  at(0.f, -0.05f, (d.length - d.approach) / 2), 1.f, 1.f, 1.f);
```

After `ball = makeDynamic(...)` add `ball->setUserIndex(static_cast<int>(Body::Ball));`. After `pins[i] = makeDynamic(...)` add `pins[i]->setUserIndex(static_cast<int>(Body::Pin));`.

In `removePin`, after `_p->inPlay[i] = false;` add `_p->impacts.clear();`. (`rebuild()` replaces `Impl`, which already empties the buffer.)

Add the override at the end of the file, before `pinPositions()`:

```cpp
std::vector<Impact> BulletWorld::drainImpacts() {
    return std::exchange(_p->impacts, {});
}
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `cmake --build build --target test_bowlingbox_alley && ./build/bin/test_bowlingbox_alley`
Expected: all tests PASS, including the four new ones and every pre-existing one (determinism).

If `no_pin_impacts_before_the_ball_reaches_the_rack` fails, print the offending impulses; if they are resting contacts just above 0.3 N·s, raise `kImpactRecordThreshold` to 0.4 and keep `SoundDirector::kMinImpulse` (Task 6) equal to it.

- [ ] **Step 6: Commit**

```bash
git add libraries/Alley/include/Bowlingbox/Alley/Types.h libraries/Alley/include/Bowlingbox/Alley/PhysicsWorld.h \
        libraries/Alley/include/Bowlingbox/Alley/Alley.h libraries/Alley/src/Alley.cpp libraries/Alley/src/Lane_impl.h \
        libraries/Alley/src/BulletWorld.h libraries/Alley/src/BulletWorld.cpp libraries/Alley/test/src/test_Alley.cpp
git commit -m "feat(alley): report new physics contacts as impact events"
```

---

### Task 2: Vendor miniaudio and stb_vorbis; sound_convert tool

**Files:**
- Create: `3rdparty/miniaudio/miniaudio.h`, `3rdparty/miniaudio/miniaudio.c` (downloaded)
- Create: `3rdparty/stb/stb_vorbis.c` (downloaded)
- Create: `3rdparty/sounds/tool/sound_convert.c`
- Create: `licenses/miniaudio.txt` (downloaded)
- Modify: `3rdparty/CMakeLists.txt`
- Modify: `3rdparty/README.md`
- Modify: `licenses/README.md`

**Interfaces:**
- Produces: CMake target `miniaudio::miniaudio` (static, PIC, public include dir `3rdparty/miniaudio`, public defines `MA_NO_ENCODING MA_NO_GENERATION MA_NO_NULL`); executable target `sound_convert` (not built by default) at `build/bin/sound_convert`.

- [ ] **Step 1: Download the sources**

```bash
curl -fsSL -o 3rdparty/miniaudio/miniaudio.h --create-dirs https://raw.githubusercontent.com/mackron/miniaudio/0.11.25/miniaudio.h
curl -fsSL -o 3rdparty/miniaudio/miniaudio.c https://raw.githubusercontent.com/mackron/miniaudio/0.11.25/miniaudio.c
curl -fsSL -o licenses/miniaudio.txt https://raw.githubusercontent.com/mackron/miniaudio/0.11.25/LICENSE
curl -fsSL -o 3rdparty/stb/stb_vorbis.c https://raw.githubusercontent.com/nothings/stb/2c980bb59875b0d32144a71867fbdebb2f77cd20/stb_vorbis.c
head -2 3rdparty/miniaudio/miniaudio.c
grep -m1 "MA_VERSION_REVISION" 3rdparty/miniaudio/miniaudio.h
```

Expected: `miniaudio.c` starts with `#define MINIAUDIO_IMPLEMENTATION` / `#include "miniaudio.h"`; the header reports revision `25`.

- [ ] **Step 2: Add the CMake targets**

Append to `3rdparty/CMakeLists.txt`:

```cmake
# miniaudio: playback for Bowlingbox::Audio. No null backend, so a machine
# without an audio device fails ma_engine_init and the game falls back to silence.
find_package(Threads REQUIRED)
add_library(bowlingbox_miniaudio STATIC miniaudio/miniaudio.c)
target_include_directories(bowlingbox_miniaudio SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/miniaudio)
target_compile_definitions(bowlingbox_miniaudio PUBLIC MA_NO_ENCODING MA_NO_GENERATION MA_NO_NULL)
target_link_libraries(bowlingbox_miniaudio PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
if(UNIX)
    target_link_libraries(bowlingbox_miniaudio PUBLIC m)
endif()
set_target_properties(bowlingbox_miniaudio PROPERTIES POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
add_library(miniaudio::miniaudio ALIAS bowlingbox_miniaudio)

# Dev tool that turns downloaded samples into 3rdparty/sounds/*.wav. Not shipped:
# cmake --build build --target sound_convert
add_executable(sound_convert EXCLUDE_FROM_ALL sounds/tool/sound_convert.c)
target_include_directories(sound_convert SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/miniaudio ${CMAKE_CURRENT_SOURCE_DIR}/stb)
target_link_libraries(sound_convert PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
if(UNIX)
    target_link_libraries(sound_convert PRIVATE m)
endif()
```

- [ ] **Step 3: Write the converter**

Create `3rdparty/sounds/tool/sound_convert.c`:

```c
/* sound_convert: turns any file miniaudio can decode (WAV, FLAC, MP3, Ogg Vorbis)
 * into the format the game embeds: mono, 22050 Hz, signed 16-bit WAV, peak -1 dBFS.
 * Dev tool only (cmake --build build --target sound_convert); never shipped.
 *
 *   sound_convert <in> <out.wav> [--start S] [--length S] [--loop]
 *
 * --start/--length cut a section (seconds). Without --loop both ends get 5 ms fades.
 * With --loop the last X seconds are crossfaded into the start (X = min(0.5 s, length / 4)),
 * so the result repeats without a click; the output is X shorter than the section. */
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#define MA_NO_DEVICE_IO
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#undef STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RATE 22050
#define CHUNK 4096

static int fail(const char* what, const char* detail) {
    fprintf(stderr, "sound_convert: %s %s\n", what, detail);
    return 1;
}

int main(int argc, char** argv) {
    double start = 0.0, length = 0.0;
    int loop = 0;
    if (argc < 3) {
        fprintf(stderr, "usage: sound_convert <in> <out.wav> [--start S] [--length S] [--loop]\n");
        return 2;
    }
    for (int i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--start") == 0 && i + 1 < argc)
            start = atof(argv[++i]);
        else if (strcmp(argv[i], "--length") == 0 && i + 1 < argc)
            length = atof(argv[++i]);
        else if (strcmp(argv[i], "--loop") == 0)
            loop = 1;
        else
            return fail("unknown option", argv[i]);
    }

    ma_decoder_config dconf = ma_decoder_config_init(ma_format_f32, 1, RATE);
    ma_decoder dec;
    if (ma_decoder_init_file(argv[1], &dconf, &dec) != MA_SUCCESS)
        return fail("cannot decode", argv[1]);
    size_t total = 0, cap = 0;
    float* pcm = NULL;
    for (;;) {
        if (total + CHUNK > cap) {
            cap = cap ? cap * 2 : (size_t)1 << 16;
            float* grown = realloc(pcm, cap * sizeof(float));
            if (!grown)
                return fail("out of memory", "");
            pcm = grown;
        }
        ma_uint64 got = 0;
        ma_decoder_read_pcm_frames(&dec, pcm + total, CHUNK, &got);
        total += (size_t)got;
        if (got < CHUNK)
            break;
    }
    ma_decoder_uninit(&dec);

    const size_t first = (size_t)(start * RATE);
    if (first >= total)
        return fail("--start is past the end of", argv[1]);
    size_t n = total - first;
    if (length > 0.0 && (size_t)(length * RATE) < n)
        n = (size_t)(length * RATE);
    float* s = pcm + first;

    if (loop) {
        size_t x = (size_t)(0.5 * RATE);
        if (x > n / 4)
            x = n / 4;
        for (size_t i = 0; i < x; ++i) {
            const float w = (float)i / (float)x;
            s[i] = s[i] * w + s[n - x + i] * (1.f - w);
        }
        n -= x;
    } else {
        size_t f = (size_t)(0.005 * RATE);
        if (f > n / 2)
            f = n / 2;
        for (size_t i = 0; i < f; ++i) {
            const float w = (float)i / (float)f;
            s[i] *= w;
            s[n - 1 - i] *= w;
        }
    }

    float peak = 0.f;
    for (size_t i = 0; i < n; ++i)
        if (fabsf(s[i]) > peak)
            peak = fabsf(s[i]);
    const float gain = peak > 0.f ? 0.8912509f / peak : 1.f; /* -1 dBFS */

    ma_int16* out = malloc(n * sizeof(ma_int16));
    if (!out)
        return fail("out of memory", "");
    for (size_t i = 0; i < n; ++i)
        out[i] = (ma_int16)lrintf(s[i] * gain * 32767.f);

    ma_encoder_config econf = ma_encoder_config_init(ma_encoding_format_wav, ma_format_s16, 1, RATE);
    ma_encoder enc;
    if (ma_encoder_init_file(argv[2], &econf, &enc) != MA_SUCCESS)
        return fail("cannot write", argv[2]);
    ma_uint64 written = 0;
    ma_encoder_write_pcm_frames(&enc, out, n, &written);
    ma_encoder_uninit(&enc);
    printf("%s: %.2f s -> %s: %.2f s, gain %.2f\n", argv[1], (double)total / RATE, argv[2],
           (double)n / RATE, (double)gain);
    free(out);
    free(pcm);
    return written == n ? 0 : fail("short write to", argv[2]);
}
```

- [ ] **Step 4: Build and smoke-test the tool**

```bash
cmake -S . -B build && cmake --build build --target sound_convert bowlingbox_miniaudio
mkdir -p /tmp/bb-sounds && cd /tmp/bb-sounds
curl -fsSLO https://opengameart.org/sites/default/files/qubodup-bowling-roll.7z && cmake -E tar xf qubodup-bowling-roll.7z
cd - && ./build/bin/sound_convert /tmp/bb-sounds/qubodup-bowling-roll/qubodup-bowling_roll-nofadeout.ogg /tmp/bb-sounds/test.wav --start 0.3 --length 3.6 --loop
file /tmp/bb-sounds/test.wav
```

Expected: the tool prints `... 4.11 s -> /tmp/bb-sounds/test.wav: 3.10 s, gain ...`, and `file` reports `RIFF (little-endian) data, WAVE audio, Microsoft PCM, 16 bit, mono 22050 Hz`.

- [ ] **Step 5: Document the vendored code**

In `3rdparty/README.md`, add rows to the table:

```markdown
| `miniaudio/` | miniaudio 0.11.25 (`miniaudio.h`, `miniaudio.c`) from https://github.com/mackron/miniaudio | Unlicense (public domain) OR MIT-0 | download both files at tag `0.11.25` |
| `stb/stb_vorbis.c` | Ogg Vorbis decoder from https://github.com/nothings/stb, commit `2c980bb` — used only by the `sound_convert` dev tool, not shipped | public domain / MIT | download the file at that commit |
| `sounds/` | 14 CC0 samples embedded by Bowlingbox::Audio; provenance in `sounds/SOURCES.md` | CC0 1.0 | `sounds/SOURCES.md` lists the exact `sound_convert` command per file |
```

In `licenses/README.md`, in the "In-tree" table add:

```markdown
| [miniaudio](https://github.com/mackron/miniaudio) | 0.11.25 | Audio (statically linked) | Unlicense OR MIT-0 | [miniaudio.txt](miniaudio.txt) |
| [stb_vorbis](https://github.com/nothings/stb) | commit 2c980bb | `sound_convert` dev tool only, not shipped | MIT OR Unlicense (public domain) | [stb.txt](stb.txt) |
| Sound effects (Kenney, OpenGameArt) | see `3rdparty/sounds/SOURCES.md` | Audio (embedded) | CC0 1.0 | [sounds.txt](sounds.txt) |
```

(`sounds.txt` is created in Task 3.)

- [ ] **Step 6: Commit**

```bash
git add 3rdparty/miniaudio 3rdparty/stb/stb_vorbis.c 3rdparty/sounds/tool/sound_convert.c 3rdparty/CMakeLists.txt \
        3rdparty/README.md licenses/miniaudio.txt licenses/README.md
git commit -m "build: vendor miniaudio and stb_vorbis, add the sound_convert dev tool"
```

---

### Task 3: Sound assets

**Files:**
- Create: `3rdparty/sounds/*.wav` (14 files)
- Create: `3rdparty/sounds/SOURCES.md`
- Create: `licenses/sounds.txt`

**Interfaces:**
- Consumes: `build/bin/sound_convert` from Task 2.
- Produces: the 14 files `roll.wav gutter_drop.wav pin_hit.wav pin_clatter.wav charge.wav release.wav strike.wav spare.wav miss.wav new_game.wav theme_switch.wav ambient_neon.wav ambient_classic.wav ambient_outdoor.wav` in `3rdparty/sounds/`, each listed in backticks in `SOURCES.md`.

- [ ] **Step 1: Download the source packs**

```bash
cd /tmp/bb-sounds
for u in \
  https://kenney.nl/media/pages/assets/impact-sounds/87b4ddecda-1677589768/kenney_impact-sounds.zip \
  https://kenney.nl/media/pages/assets/interface-sounds/fa43c1dd4d-1677589452/kenney_interface-sounds.zip \
  https://kenney.nl/media/pages/assets/music-jingles/f37e530b9e-1677590399/kenney_music-jingles.zip \
  https://kenney.nl/media/pages/assets/sci-fi-sounds/6b296f9ecf-1677589334/kenney_sci-fi-sounds.zip \
  https://opengameart.org/sites/default/files/crowd_shouting_0.ogg \
  https://opengameart.org/sites/default/files/amb_morning_0.ogg; do curl -fsSLO "$u"; done
for z in kenney_*.zip; do unzip -oq "$z" -d kenney; done
cd -
```

If a Kenney URL has changed, open the pack page (`https://kenney.nl/assets/<pack>`) and use its current download link; the file names below stay the same.

- [ ] **Step 2: Convert all 14 files**

```bash
SC=./build/bin/sound_convert; IN=/tmp/bb-sounds; OUT=3rdparty/sounds
k() { find "$IN/kenney" -name "$1" | head -1; }
$SC "$IN/qubodup-bowling-roll/qubodup-bowling_roll-nofadeout.ogg" $OUT/roll.wav --start 0.3 --length 3.6 --loop
$SC "$(k impactPlank_medium_000.ogg)"  $OUT/gutter_drop.wav
$SC "$(k impactWood_heavy_000.ogg)"    $OUT/pin_hit.wav
$SC "$(k impactWood_medium_001.ogg)"   $OUT/pin_clatter.wav
$SC "$(k forceField_000.ogg)"          $OUT/charge.wav --loop
$SC "$(k impactSoft_heavy_000.ogg)"    $OUT/release.wav
$SC "$(k jingles_HIT03.ogg)"           $OUT/strike.wav
$SC "$(k jingles_HIT00.ogg)"           $OUT/spare.wav
$SC "$(k jingles_PIZZI03.ogg)"         $OUT/miss.wav
$SC "$(k confirmation_001.ogg)"        $OUT/new_game.wav
$SC "$(k switch_002.ogg)"              $OUT/theme_switch.wav
$SC "$(k spaceEngineLow_000.ogg)"      $OUT/ambient_neon.wav --loop
$SC "$IN/crowd_shouting_0.ogg"         $OUT/ambient_classic.wav --start 2 --length 12.4 --loop
$SC "$IN/amb_morning_0.ogg"            $OUT/ambient_outdoor.wav --start 5 --length 12.4 --loop
ls -l $OUT/*.wav | wc -l && du -cb $OUT/*.wav | tail -1
```

Expected: 14 files; total under 3 145 728 bytes (about 1.7 MB). Loops come out at roughly roll 3.1 s, charge 0.7 s, neon 4.5 s, classic 11.9 s, outdoor 11.9 s. These are first picks chosen by name and length; the human partner confirms them by ear in Task 7 and may swap a file for another one from the same packs (rerun the line, update `SOURCES.md`).

- [ ] **Step 3: Write `3rdparty/sounds/SOURCES.md`**

```markdown
# Sound sources

Every file here is CC0 1.0 (public domain dedication). Each was converted with
`build/bin/sound_convert <source> <file> [options]` (mono, 22050 Hz, 16-bit, peak −1 dBFS);
see `tool/sound_convert.c`. CMake refuses to configure if a `.wav` here is missing from this list.

| File | Source | Original file | Author | Licence | Options |
|---|---|---|---|---|---|
| `roll.wav` | https://opengameart.org/content/bowling-ball-rolling | `qubodup-bowling_roll-nofadeout.ogg` | qubodup | CC0 1.0 | `--start 0.3 --length 3.6 --loop` |
| `gutter_drop.wav` | https://kenney.nl/assets/impact-sounds | `impactPlank_medium_000.ogg` | Kenney (kenney.nl) | CC0 1.0 | |
| `pin_hit.wav` | https://kenney.nl/assets/impact-sounds | `impactWood_heavy_000.ogg` | Kenney (kenney.nl) | CC0 1.0 | |
| `pin_clatter.wav` | https://kenney.nl/assets/impact-sounds | `impactWood_medium_001.ogg` | Kenney (kenney.nl) | CC0 1.0 | |
| `charge.wav` | https://kenney.nl/assets/sci-fi-sounds | `forceField_000.ogg` | Kenney (kenney.nl) | CC0 1.0 | `--loop` |
| `release.wav` | https://kenney.nl/assets/impact-sounds | `impactSoft_heavy_000.ogg` | Kenney (kenney.nl) | CC0 1.0 | |
| `strike.wav` | https://kenney.nl/assets/music-jingles | `jingles_HIT03.ogg` | Kenney (kenney.nl) | CC0 1.0 | |
| `spare.wav` | https://kenney.nl/assets/music-jingles | `jingles_HIT00.ogg` | Kenney (kenney.nl) | CC0 1.0 | |
| `miss.wav` | https://kenney.nl/assets/music-jingles | `jingles_PIZZI03.ogg` | Kenney (kenney.nl) | CC0 1.0 | |
| `new_game.wav` | https://kenney.nl/assets/interface-sounds | `confirmation_001.ogg` | Kenney (kenney.nl) | CC0 1.0 | |
| `theme_switch.wav` | https://kenney.nl/assets/interface-sounds | `switch_002.ogg` | Kenney (kenney.nl) | CC0 1.0 | |
| `ambient_neon.wav` | https://kenney.nl/assets/sci-fi-sounds | `spaceEngineLow_000.ogg` | Kenney (kenney.nl) | CC0 1.0 | `--loop` |
| `ambient_classic.wav` | https://opengameart.org/content/crowd-shoutingspeaking-ambience | `crowd_shouting_0.ogg` | StarNinjas | CC0 1.0 | `--start 2 --length 12.4 --loop` |
| `ambient_outdoor.wav` | https://opengameart.org/content/amb-morning-sounds-perfect-loop | `amb_morning_0.ogg` | Kresiek The Furry | CC0 1.0 | `--start 5 --length 12.4 --loop` |
```

- [ ] **Step 4: Write `licenses/sounds.txt`**

```bash
{ echo "Bowlingbox sound effects (3rdparty/sounds, embedded in libBowlingboxAudio)"; echo;
  echo "All files are dedicated to the public domain under CC0 1.0. Attribution is not required;"
  echo "the authors are credited in 3rdparty/sounds/SOURCES.md:"; echo
  echo "  Kenney (kenney.nl): impact-sounds, interface-sounds, music-jingles, sci-fi-sounds"
  echo "  qubodup (OpenGameArt): bowling-ball-rolling"
  echo "  StarNinjas (OpenGameArt): crowd-shoutingspeaking-ambience"
  echo "  Kresiek The Furry (OpenGameArt): amb-morning-sounds-perfect-loop"; echo
  echo "================================================================================"
  curl -fsSL https://creativecommons.org/publicdomain/zero/1.0/legalcode.txt; } > licenses/sounds.txt
head -3 licenses/sounds.txt && grep -c "Creative Commons" licenses/sounds.txt
```

Expected: the header lines, and a count ≥ 1.

- [ ] **Step 5: Commit**

```bash
git add 3rdparty/sounds/*.wav 3rdparty/sounds/SOURCES.md licenses/sounds.txt
git commit -m "assets: CC0 sound effects for BowlingGL"
```

---

### Task 4: Bowlingbox::Audio library

**Files:**
- Modify: `cmake/EmbedFile.cmake`
- Modify: `CMakeLists.txt` (top level, `BOWLINGBOX_LIBRARIES`)
- Modify: `libraries/CMakeLists.txt`
- Create: `libraries/Audio/CMakeLists.txt`
- Create: `libraries/Audio/include/Bowlingbox/Audio/Audio.h`
- Create: `libraries/Audio/src/EmbeddedSounds.h`
- Create: `libraries/Audio/src/Catalogue.cpp`
- Create: `libraries/Audio/src/SilentOutput.cpp`
- Create: `libraries/Audio/src/DeviceOutput.cpp`
- Test: `libraries/Audio/test/src/test_Audio.cpp`

**Interfaces:**
- Consumes: `miniaudio::miniaudio` (Task 2); `3rdparty/sounds/*.wav` and `SOURCES.md` (Task 3); `Result`/`Bowlingbox::Core::Error` from `Bowlingbox/Core/Core.h`.
- Produces (namespace `Bowlingbox::Audio`, header `<Bowlingbox/Audio/Audio.h>`, target `Bowlingbox::Audio`):
  - `enum class SoundId { Roll, GutterDrop, PinHit, PinClatter, ChargeTone, Release, Strike, Spare, GutterJingle, NewGame, ThemeSwitch, AmbientNeon, AmbientClassic, AmbientOutdoor };`
  - `kSoundCount = 14`, `kAllSounds`, `enum class LoopSlot { Roll, Charge, Ambient }`, `kLoopSlotCount = 3`
  - `struct Voice { float volume = 1.f; float pitch = 1.f; float pan = 0.f; bool operator==(const Voice&) const = default; };`
  - `struct SoundInfo { int channels; int sampleRate; float seconds; };`
  - `class IAudioOutput { play(SoundId, const Voice&); setLoop(LoopSlot, std::optional<SoundId>, const Voice&); setMuted(bool); muted() const; }`
  - `fileName(SoundId)`, `embeddedData(SoundId)`, `isLoop(SoundId)`, `decodeInfo(SoundId)`, `createDeviceOutput()`, `makeSilentOutput()`

The library directory must exist on disk before the next configure, or the scaffold generator creates a template library and app in its place. Create all files in this task before running CMake.

- [ ] **Step 1: Write the failing test**

Create `libraries/Audio/test/src/test_Audio.cpp`:

```cpp
#include <Bowlingbox/Audio/Audio.h>
#include <gtest/gtest.h>
#include <cstddef>
#include <set>
#include <string>

using namespace Bowlingbox::Audio;

TEST(test_audio_catalogue, every_sound_has_a_unique_wav_file_and_data) {
    std::set<std::string> names;
    for (SoundId id : kAllSounds) {
        const std::string name(fileName(id));
        EXPECT_TRUE(name.ends_with(".wav")) << name;
        EXPECT_TRUE(names.insert(name).second) << name;
        EXPECT_GT(embeddedData(id).size(), 44u) << name;  // more than a WAV header
    }
    EXPECT_EQ(names.size(), static_cast<std::size_t>(kSoundCount));
}

TEST(test_audio_catalogue, loops_are_roll_charge_and_the_ambiences) {
    for (SoundId id : kAllSounds) {
        const bool expected = id == SoundId::Roll || id == SoundId::ChargeTone || id == SoundId::AmbientNeon ||
                              id == SoundId::AmbientClassic || id == SoundId::AmbientOutdoor;
        EXPECT_EQ(isLoop(id), expected) << fileName(id);
    }
}

TEST(test_audio_catalogue, every_sound_decodes_to_mono_22050_within_its_length_limit) {
    for (SoundId id : kAllSounds) {
        auto decoded = decodeInfo(id);
        ASSERT_TRUE(decoded.isValue()) << fileName(id);
        const SoundInfo info = decoded();
        EXPECT_EQ(info.channels, 1) << fileName(id);
        EXPECT_EQ(info.sampleRate, 22050) << fileName(id);
        if (isLoop(id)) {
            EXPECT_GE(info.seconds, 0.5f) << fileName(id);
            EXPECT_LE(info.seconds, 12.f) << fileName(id);
        } else {
            EXPECT_GE(info.seconds, 0.05f) << fileName(id);
            EXPECT_LE(info.seconds, 3.f) << fileName(id);
        }
    }
}

TEST(test_audio_catalogue, embedded_sounds_stay_under_three_megabytes) {
    std::size_t total = 0;
    for (SoundId id : kAllSounds)
        total += embeddedData(id).size();
    EXPECT_LT(total, 3u * 1024u * 1024u);
}

TEST(test_audio_silent, silent_output_tracks_mute_and_ignores_cues) {
    auto out = makeSilentOutput();
    ASSERT_NE(out, nullptr);
    EXPECT_FALSE(out->muted());
    out->play(SoundId::Strike, Voice {});
    out->setLoop(LoopSlot::Ambient, SoundId::AmbientNeon, Voice {0.35f, 1.f, 0.f});
    out->setLoop(LoopSlot::Ambient, std::nullopt, Voice {});
    out->setMuted(true);
    EXPECT_TRUE(out->muted());
    out->setMuted(false);
    EXPECT_FALSE(out->muted());
}
```

- [ ] **Step 2: Add a namespace option to the embed helper**

In `cmake/EmbedFile.cmake`, update the header comment and function:

```cmake
# bowlingbox_embed_file(INPUT <file> OUTPUT <file.cpp> SYMBOL <name> [NAMESPACE <ns>])
#
# Writes a C++ source that defines, in namespace <ns> (default Bowlingbox::Render::embedded),
#   extern const unsigned char <name>[];
#   extern const std::size_t   <name>Size;
# holding the bytes of <file>. Runs at configure time; editing <file> re-runs configure.
function(bowlingbox_embed_file)
    cmake_parse_arguments(PARSE_ARGV 0 arg "" "INPUT;OUTPUT;SYMBOL;NAMESPACE" "")
    if(NOT arg_NAMESPACE)
        set(arg_NAMESPACE Bowlingbox::Render::embedded)
    endif()
```

and in the `file(WRITE ...)` call replace both occurrences of `Bowlingbox::Render::embedded` with `${arg_NAMESPACE}`:

```cmake
        "namespace ${arg_NAMESPACE} {\n"
        ...
        "}  // namespace ${arg_NAMESPACE}\n")
```

- [ ] **Step 3: Register the library**

Top-level `CMakeLists.txt`:

```cmake
set(BOWLINGBOX_LIBRARIES
    Score
    Core
    Alley
    Audio
    Render
)
```

`libraries/CMakeLists.txt`:

```cmake
set(_bowlingbox_library_dirs Score Core Alley Audio Render)
```

- [ ] **Step 4: Write the library CMake file**

Create `libraries/Audio/CMakeLists.txt`:

```cmake
project(Audio VERSION 1.0.0 LANGUAGES CXX)

set(TARGET_NAME ${PROJECT_NAME})
set(TST_DISCOVER ON)
###############################################################
# Manage project files
###############################################################
include(EmbedFile)
set(SOUNDS_DIR ${CMAKE_SOURCE_DIR}/3rdparty/sounds)
# One entry per Audio::SoundId, in enum order (src/Catalogue.cpp relies on it).
set(SOUND_NAMES
    roll gutter_drop pin_hit pin_clatter charge release strike spare miss
    new_game theme_switch ambient_neon ambient_classic ambient_outdoor
)
file(READ ${SOUNDS_DIR}/SOURCES.md SOUND_PROVENANCE)
file(GLOB SOUND_FILES_ON_DISK RELATIVE ${SOUNDS_DIR} ${SOUNDS_DIR}/*.wav)
foreach(wav IN LISTS SOUND_FILES_ON_DISK)
    string(FIND "${SOUND_PROVENANCE}" "`${wav}`" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "3rdparty/sounds/${wav} is not listed in 3rdparty/sounds/SOURCES.md")
    endif()
endforeach()
set(SOUND_SOURCES)
foreach(name IN LISTS SOUND_NAMES)
    if(NOT EXISTS ${SOUNDS_DIR}/${name}.wav)
        message(FATAL_ERROR "missing sound 3rdparty/sounds/${name}.wav")
    endif()
    set(out ${CMAKE_CURRENT_BINARY_DIR}/generated/Sound_${name}.cpp)
    bowlingbox_embed_file(
        INPUT     ${SOUNDS_DIR}/${name}.wav
        OUTPUT    ${out}
        SYMBOL    kSound_${name}
        NAMESPACE Bowlingbox::Audio::embedded
    )
    list(APPEND SOUND_SOURCES ${out})
endforeach()
set(TARGET_HEADERS
    src/EmbeddedSounds.h
    include/Bowlingbox/Audio/Audio.h
)
set(TARGET_SOURCES
    src/Catalogue.cpp
    src/SilentOutput.cpp
    src/DeviceOutput.cpp
    ${SOUND_SOURCES}
)
set(TARGET_RSC)
# miniaudio is an implementation detail; BUILD_INTERFACE keeps it out of the
# installed export set.
set(PRIVATE_DEPS
    $<BUILD_INTERFACE:miniaudio::miniaudio>
)
set(PUBLIC_DEPS
    ${CMAKE_ROOT_NAME}::Core
)
set(TARGET_FILES ${TARGET_HEADERS} ${TARGET_SOURCES})
###############################################################
# Manage testing files
###############################################################
set(TST_SOURCES
    test/src/test_Audio.cpp
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

- [ ] **Step 5: Write the public header**

Create `libraries/Audio/include/Bowlingbox/Audio/Audio.h`:

```cpp
#ifndef BOWLINGBOX_AUDIO_H
#define BOWLINGBOX_AUDIO_H
#include <Bowlingbox/Audio/Bowlingbox_Audio_export.h>
#include <Bowlingbox/Core/Core.h>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace Bowlingbox::Audio {

/// Every embedded sound. Files live in 3rdparty/sounds (see SOURCES.md there).
enum class SoundId {
    Roll, GutterDrop, PinHit, PinClatter, ChargeTone, Release, Strike, Spare,
    GutterJingle, NewGame, ThemeSwitch, AmbientNeon, AmbientClassic, AmbientOutdoor
};
inline constexpr int kSoundCount = 14;
inline constexpr std::array<SoundId, kSoundCount> kAllSounds {
    SoundId::Roll, SoundId::GutterDrop, SoundId::PinHit, SoundId::PinClatter, SoundId::ChargeTone,
    SoundId::Release, SoundId::Strike, SoundId::Spare, SoundId::GutterJingle, SoundId::NewGame,
    SoundId::ThemeSwitch, SoundId::AmbientNeon, SoundId::AmbientClassic, SoundId::AmbientOutdoor};

/// Each slot plays at most one looping sound.
enum class LoopSlot { Roll, Charge, Ambient };
inline constexpr int kLoopSlotCount = 3;

struct Voice {
    float volume = 1.f;  // [0, 1]
    float pitch = 1.f;   // playback rate, > 0
    float pan = 0.f;     // -1 left .. +1 right
    bool operator==(const Voice&) const = default;
};

struct SoundInfo {
    int channels = 0;
    int sampleRate = 0;
    float seconds = 0.f;
};

class BOWLINGBOX_AUDIO_API IAudioOutput {
public:
    virtual ~IAudioOutput() = default;
    virtual void play(SoundId id, const Voice& voice) = 0;
    /// nullopt stops the slot; the same id updates the voice without restarting;
    /// a different id restarts the slot with that sound.
    virtual void setLoop(LoopSlot slot, std::optional<SoundId> id, const Voice& voice) = 0;
    virtual void setMuted(bool muted) = 0;
    virtual bool muted() const = 0;
};

BOWLINGBOX_AUDIO_API std::string_view fileName(SoundId id);  // "roll.wav"
BOWLINGBOX_AUDIO_API std::span<const unsigned char> embeddedData(SoundId id);
BOWLINGBOX_AUDIO_API bool isLoop(SoundId id);
/// Decodes the embedded file without an audio device.
BOWLINGBOX_AUDIO_API Result<SoundInfo> decodeInfo(SoundId id);

/// Plays through the default audio device; fails if there is none.
BOWLINGBOX_AUDIO_API Result<std::unique_ptr<IAudioOutput>> createDeviceOutput();
/// Plays nothing; used when there is no device.
BOWLINGBOX_AUDIO_API std::unique_ptr<IAudioOutput> makeSilentOutput();

}  // namespace Bowlingbox::Audio
#endif  // BOWLINGBOX_AUDIO_H
```

- [ ] **Step 6: Write the catalogue**

Create `libraries/Audio/src/EmbeddedSounds.h`:

```cpp
#ifndef BOWLINGBOX_AUDIO_EMBEDDED_SOUNDS_H
#define BOWLINGBOX_AUDIO_EMBEDDED_SOUNDS_H
#include <cstddef>

/// 3rdparty/sounds/*.wav, generated by cmake/EmbedFile.cmake.
namespace Bowlingbox::Audio::embedded {
extern const unsigned char kSound_roll[];
extern const std::size_t kSound_rollSize;
extern const unsigned char kSound_gutter_drop[];
extern const std::size_t kSound_gutter_dropSize;
extern const unsigned char kSound_pin_hit[];
extern const std::size_t kSound_pin_hitSize;
extern const unsigned char kSound_pin_clatter[];
extern const std::size_t kSound_pin_clatterSize;
extern const unsigned char kSound_charge[];
extern const std::size_t kSound_chargeSize;
extern const unsigned char kSound_release[];
extern const std::size_t kSound_releaseSize;
extern const unsigned char kSound_strike[];
extern const std::size_t kSound_strikeSize;
extern const unsigned char kSound_spare[];
extern const std::size_t kSound_spareSize;
extern const unsigned char kSound_miss[];
extern const std::size_t kSound_missSize;
extern const unsigned char kSound_new_game[];
extern const std::size_t kSound_new_gameSize;
extern const unsigned char kSound_theme_switch[];
extern const std::size_t kSound_theme_switchSize;
extern const unsigned char kSound_ambient_neon[];
extern const std::size_t kSound_ambient_neonSize;
extern const unsigned char kSound_ambient_classic[];
extern const std::size_t kSound_ambient_classicSize;
extern const unsigned char kSound_ambient_outdoor[];
extern const std::size_t kSound_ambient_outdoorSize;
}  // namespace Bowlingbox::Audio::embedded
#endif  // BOWLINGBOX_AUDIO_EMBEDDED_SOUNDS_H
```

Create `libraries/Audio/src/Catalogue.cpp`:

```cpp
#include "EmbeddedSounds.h"
#include <Bowlingbox/Audio/Audio.h>
#include <miniaudio.h>
#include <array>
#include <cstddef>
#include <string>

namespace Bowlingbox::Audio {
namespace {
struct Entry {
    std::string_view file;
    const unsigned char* data;
    const std::size_t* size;
    bool loop;
};

// Same order as SoundId.
const std::array<Entry, kSoundCount> kEntries {{
    {"roll.wav", embedded::kSound_roll, &embedded::kSound_rollSize, true},
    {"gutter_drop.wav", embedded::kSound_gutter_drop, &embedded::kSound_gutter_dropSize, false},
    {"pin_hit.wav", embedded::kSound_pin_hit, &embedded::kSound_pin_hitSize, false},
    {"pin_clatter.wav", embedded::kSound_pin_clatter, &embedded::kSound_pin_clatterSize, false},
    {"charge.wav", embedded::kSound_charge, &embedded::kSound_chargeSize, true},
    {"release.wav", embedded::kSound_release, &embedded::kSound_releaseSize, false},
    {"strike.wav", embedded::kSound_strike, &embedded::kSound_strikeSize, false},
    {"spare.wav", embedded::kSound_spare, &embedded::kSound_spareSize, false},
    {"miss.wav", embedded::kSound_miss, &embedded::kSound_missSize, false},
    {"new_game.wav", embedded::kSound_new_game, &embedded::kSound_new_gameSize, false},
    {"theme_switch.wav", embedded::kSound_theme_switch, &embedded::kSound_theme_switchSize, false},
    {"ambient_neon.wav", embedded::kSound_ambient_neon, &embedded::kSound_ambient_neonSize, true},
    {"ambient_classic.wav", embedded::kSound_ambient_classic, &embedded::kSound_ambient_classicSize, true},
    {"ambient_outdoor.wav", embedded::kSound_ambient_outdoor, &embedded::kSound_ambient_outdoorSize, true},
}};

const Entry& entry(SoundId id) {
    return kEntries[static_cast<std::size_t>(id)];
}

Core::Error<> decodeError(SoundId id, ma_result r) {
    return Core::Error<>(std::string(fileName(id)) + ": " + ma_result_description(r));
}
}  // namespace

std::string_view fileName(SoundId id) {
    return entry(id).file;
}

std::span<const unsigned char> embeddedData(SoundId id) {
    const Entry& e = entry(id);
    return {e.data, *e.size};
}

bool isLoop(SoundId id) {
    return entry(id).loop;
}

Result<SoundInfo> decodeInfo(SoundId id) {
    const auto data = embeddedData(id);
    ma_decoder decoder;
    if (const ma_result r = ma_decoder_init_memory(data.data(), data.size(), nullptr, &decoder); r != MA_SUCCESS)
        return decodeError(id, r);
    ma_uint64 frames = 0;
    const ma_result r = ma_decoder_get_length_in_pcm_frames(&decoder, &frames);
    const SoundInfo info {static_cast<int>(decoder.outputChannels), static_cast<int>(decoder.outputSampleRate),
                          static_cast<float>(frames) / static_cast<float>(decoder.outputSampleRate)};
    ma_decoder_uninit(&decoder);
    if (r != MA_SUCCESS)
        return decodeError(id, r);
    return info;
}

}  // namespace Bowlingbox::Audio
```

- [ ] **Step 7: Write the silent output**

Create `libraries/Audio/src/SilentOutput.cpp`:

```cpp
#include <Bowlingbox/Audio/Audio.h>

namespace Bowlingbox::Audio {
namespace {
class SilentOutput final : public IAudioOutput {
public:
    void play(SoundId, const Voice&) override {}
    void setLoop(LoopSlot, std::optional<SoundId>, const Voice&) override {}
    void setMuted(bool muted) override { _muted = muted; }
    bool muted() const override { return _muted; }

private:
    bool _muted = false;
};
}  // namespace

std::unique_ptr<IAudioOutput> makeSilentOutput() {
    return std::make_unique<SilentOutput>();
}

}  // namespace Bowlingbox::Audio
```

- [ ] **Step 8: Write the device output**

Create `libraries/Audio/src/DeviceOutput.cpp`:

```cpp
#include <Bowlingbox/Audio/Audio.h>
#include <miniaudio.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace Bowlingbox::Audio {
namespace {
constexpr int kOneShotVoices = 16;
constexpr ma_uint32 kSoundFlags = MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION;

Core::Error<> failure(const char* what, ma_result r) {
    return Core::Error<>(std::string(what) + ": " + ma_result_description(r));
}

void apply(ma_sound& s, const Voice& v) {
    ma_sound_set_volume(&s, v.volume);
    ma_sound_set_pitch(&s, v.pitch);
    ma_sound_set_pan(&s, v.pan);
}

/// miniaudio engine with the embedded sounds registered by file name.
/// ma_sound objects must not move once initialised, so this lives on the heap
/// and is neither copyable nor movable.
class DeviceOutput final : public IAudioOutput {
public:
    DeviceOutput() = default;
    DeviceOutput(const DeviceOutput&) = delete;
    DeviceOutput& operator=(const DeviceOutput&) = delete;
    ~DeviceOutput() override {
        for (auto& v : _voices)
            if (v.serial != 0)
                ma_sound_uninit(&v.sound);
        for (auto& l : _loops)
            if (l.id)
                ma_sound_uninit(&l.sound);
        if (_engineReady)
            ma_engine_uninit(&_engine);
    }

    Result<> init() {
        if (const ma_result r = ma_engine_init(nullptr, &_engine); r != MA_SUCCESS)
            return failure("no audio device", r);
        _engineReady = true;
        ma_resource_manager* rm = ma_engine_get_resource_manager(&_engine);
        for (SoundId id : kAllSounds) {
            const auto data = embeddedData(id);
            const std::string name(fileName(id));
            if (const ma_result r = ma_resource_manager_register_encoded_data(rm, name.c_str(), data.data(), data.size());
                r != MA_SUCCESS)
                return failure("cannot register sound", r);
        }
        return Result<>(std::monostate {});
    }

    void play(SoundId id, const Voice& voice) override {
        OneShot& v = freeVoice();
        if (v.serial != 0) {
            ma_sound_uninit(&v.sound);
            v.serial = 0;
        }
        if (!load(id, v.sound))
            return;
        v.serial = ++_serial;
        apply(v.sound, voice);
        ma_sound_start(&v.sound);
    }

    void setLoop(LoopSlot slot, std::optional<SoundId> id, const Voice& voice) override {
        Loop& l = _loops[static_cast<std::size_t>(slot)];
        if (l.id == id) {
            if (id)
                apply(l.sound, voice);
            return;
        }
        if (l.id) {
            ma_sound_uninit(&l.sound);
            l.id.reset();
        }
        if (!id || !load(*id, l.sound))
            return;
        l.id = id;
        ma_sound_set_looping(&l.sound, MA_TRUE);
        apply(l.sound, voice);
        ma_sound_start(&l.sound);
    }

    void setMuted(bool muted) override {
        _muted = muted;
        ma_engine_set_volume(&_engine, muted ? 0.f : 1.f);  // loops keep running underneath
    }
    bool muted() const override { return _muted; }

private:
    struct OneShot {
        ma_sound sound {};
        std::uint64_t serial = 0;  // 0: not initialised
    };
    struct Loop {
        ma_sound sound {};
        std::optional<SoundId> id;
    };

    bool load(SoundId id, ma_sound& s) {
        const std::string name(fileName(id));
        return ma_sound_init_from_file(&_engine, name.c_str(), kSoundFlags, nullptr, nullptr, &s) == MA_SUCCESS;
    }

    /// A finished or unused voice, else the oldest playing one.
    OneShot& freeVoice() {
        OneShot* oldest = &_voices[0];
        for (auto& v : _voices) {
            if (v.serial == 0 || ma_sound_at_end(&v.sound))
                return v;
            if (v.serial < oldest->serial)
                oldest = &v;
        }
        return *oldest;
    }

    ma_engine _engine {};
    bool _engineReady = false;
    bool _muted = false;
    std::uint64_t _serial = 0;
    std::array<OneShot, kOneShotVoices> _voices {};
    std::array<Loop, kLoopSlotCount> _loops {};
};
}  // namespace

Result<std::unique_ptr<IAudioOutput>> createDeviceOutput() {
    auto out = std::make_unique<DeviceOutput>();
    auto ready = out->init();
    if (ready.isError())
        return Core::Error<>(ready.error());
    return std::unique_ptr<IAudioOutput>(std::move(out));
}

}  // namespace Bowlingbox::Audio
```

- [ ] **Step 9: Configure, build and run the tests**

Run: `cmake -S . -B build 2>&1 | grep -E "scaffold|Audio|sound" ; cmake --build build -j && ./build/bin/test_bowlingbox_audio`
Expected: configure prints `scaffold: Audio already present` (and no `generating Audio`); all five tests PASS.

Then check the provenance guard: `cp 3rdparty/sounds/miss.wav 3rdparty/sounds/extra.wav && cmake -S . -B build; rm 3rdparty/sounds/extra.wav && cmake -S . -B build`
Expected: the first configure fails with `3rdparty/sounds/extra.wav is not listed in 3rdparty/sounds/SOURCES.md`; the second succeeds.

Also confirm the Render font embedding still works: `./build/bin/test_bowlingbox_render` PASS.

- [ ] **Step 10: Commit**

```bash
git add cmake/EmbedFile.cmake CMakeLists.txt libraries/CMakeLists.txt libraries/Audio
git commit -m "feat(audio): Bowlingbox::Audio with embedded sounds, miniaudio output and a silent fallback"
```

---

### Task 5: Throw result and the mute command

**Files:**
- Modify: `applications/bowlingGame/bowlingGameGL/HudText.h`
- Modify: `applications/bowlingGame/bowlingGameGL/HudText.cpp`
- Modify: `applications/bowlingGame/bowlingGameGL/Command.h`
- Modify: `applications/bowlingGame/bowlingGameGL/InputMapper.cpp`
- Modify: `applications/bowlingGame/bowlingGameGL/LaneCommands.h`
- Modify: `applications/bowlingGame/bowlingGameGL/LaneCommands.cpp`
- Test: `applications/bowlingGame/bowlingGameGL/test/test_Gameplay.cpp`
- Test: `applications/bowlingGame/bowlingGameGL/test/test_InputMapper.cpp`

**Interfaces:**
- Produces:
  - `enum class HudText::ThrowResult { None, Strike, Spare, Gutter, Pins };`
  - `HudText::ThrowResult HudText::throwResult(const Bowlingbox::Alley::Snapshot&, const Bowlingbox::Score::IScoreSheet&);`
  - `Command::Type::ToggleMute`, produced by `InputMapper::keyDown('M' | 'm', ...)` in every phase.

- [ ] **Step 1: Write the failing tests**

In `test_InputMapper.cpp`, add:

```cpp
TEST(test_input_mapper, m_toggles_mute_in_every_phase) {
    for (Phase p : {Phase::Aiming, Phase::Charging, Phase::Rolling, Phase::Settling, Phase::FrameDone, Phase::GameOver}) {
        EXPECT_EQ(InputMapper::keyDown('M', false, p), cmd(Type::ToggleMute));
        EXPECT_EQ(InputMapper::keyDown('m', false, p), cmd(Type::ToggleMute));
        EXPECT_EQ(InputMapper::keyUp('M', p), cmd(Type::None));
    }
}
```

In `test_Gameplay.cpp`, add (next to the other `test_hud_text` and `test_lane_commands` tests):

```cpp
TEST(test_hud_text, throw_result_ten_after_a_gutter_ball_is_a_spare) {
    Score::ScoreCalculator sheet;
    auto s = snapshotIn(Phase::FrameDone);
    sheet.roll(0);
    s.lastPinsDown = 0;
    EXPECT_EQ(HudText::throwResult(s, sheet), HudText::ThrowResult::Gutter);
    sheet.roll(10);
    s.lastPinsDown = 10;
    EXPECT_EQ(HudText::throwResult(s, sheet), HudText::ThrowResult::Spare);
}

TEST(test_hud_text, throw_result_names_strikes_and_counts_and_nothing_outside_frame_done) {
    Score::ScoreCalculator sheet;
    auto s = snapshotIn(Phase::FrameDone);
    sheet.roll(10);
    s.lastPinsDown = 10;
    EXPECT_EQ(HudText::throwResult(s, sheet), HudText::ThrowResult::Strike);
    sheet.roll(4);
    s.lastPinsDown = 4;
    EXPECT_EQ(HudText::throwResult(s, sheet), HudText::ThrowResult::Pins);
    EXPECT_EQ(HudText::throwResult(snapshotIn(Phase::Rolling), sheet), HudText::ThrowResult::None);
}

TEST(test_lane_commands, toggle_mute_leaves_the_lane_alone) {
    Alley::Lane lane;
    const Alley::Snapshot before = lane.snapshot();
    EXPECT_TRUE(LaneCommands::execute({Command::Type::ToggleMute}, lane).isValue());
    EXPECT_EQ(lane.snapshot().phase, before.phase);
}
```

And extend the key list in `controls_help_names_every_binding`:

```cpp
    for (const char* key : {"Left / Right", "A / D", "Space", "N", "Esc", "Ctrl+Q", "T: change the scenery", "M: mute sound"})
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `cmake --build build --target test_bowlinggl 2>&1 | tail -5`
Expected: compile errors `'ToggleMute' is not a member of 'Command::Type'` and `'throwResult' is not a member of 'HudText'`.

- [ ] **Step 3: Implement**

`Command.h`:

```cpp
    enum class Type { None, MoveAim, RotateAim, StartCharge, Release, NewGame, CycleTheme, ToggleMute, Quit };
```

`InputMapper.cpp`, after the `'T'` line in `keyDown`:

```cpp
    if (keyCode == 'M' || keyCode == 'm')
        return {Type::ToggleMute};
```

`LaneCommands.h` comment:

```cpp
/// Applies a game command to the lane, using its amount for MoveAim / RotateAim.
/// CycleTheme, ToggleMute, Quit and None are not lane commands and succeed without
/// effect; the caller handles CycleTheme, ToggleMute and Quit.
```

`LaneCommands.cpp`, add the case to the no-op group:

```cpp
        case Command::Type::CycleTheme:
        case Command::Type::ToggleMute:
        case Command::Type::Quit:
        case Command::Type::None:
            break;
```

`HudText.h`, before `result`:

```cpp
enum class ThrowResult { None, Strike, Spare, Gutter, Pins };
/// What the last throw was, by the sheet's marks (a ten on a second ball is a
/// spare); None outside FrameDone.
ThrowResult throwResult(const Bowlingbox::Alley::Snapshot& s, const Bowlingbox::Score::IScoreSheet& sheet);
```

`HudText.cpp`, replace `result` with:

```cpp
ThrowResult throwResult(const Bowlingbox::Alley::Snapshot& s, const Bowlingbox::Score::IScoreSheet& sheet) {
    if (s.phase != Phase::FrameDone || s.lastPinsDown < 0)
        return ThrowResult::None;
    const int last = lastRollValue(sheet.rolls());
    // The sheet's mark decides: ten pins on a second ball are a spare.
    if (last == toInt(Mark::Spare))
        return ThrowResult::Spare;
    if (last == toInt(Mark::Strike) || s.lastPinsDown == Bowlingbox::Score::kPins)
        return ThrowResult::Strike;
    if (s.lastPinsDown == 0)
        return ThrowResult::Gutter;
    return ThrowResult::Pins;
}

std::string result(const Bowlingbox::Alley::Snapshot& s, const Bowlingbox::Score::IScoreSheet& sheet) {
    if (s.phase == Phase::GameOver)
        return "Game over - score " + std::to_string(sheet.score());
    switch (throwResult(s, sheet)) {
        case ThrowResult::Strike: return "STRIKE!";
        case ThrowResult::Spare:  return "SPARE!";
        case ThrowResult::Gutter: return "Gutter";
        case ThrowResult::Pins:   return std::to_string(s.lastPinsDown) + " pins";
        case ThrowResult::None:   break;
    }
    return "";
}
```

`HudText.cpp`, in `controlsHelp()`, after the `T` line:

```cpp
           "T: change the scenery\n"
           "M: mute sound\n"
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cmake --build build --target test_bowlinggl && ./build/bin/test_bowlinggl`
Expected: all PASS, including the pre-existing `result_names_strikes_spares_and_gutters` and `ten_pins_after_a_gutter_ball_is_a_spare`.

- [ ] **Step 5: Commit**

```bash
git add applications/bowlingGame/bowlingGameGL/HudText.h applications/bowlingGame/bowlingGameGL/HudText.cpp \
        applications/bowlingGame/bowlingGameGL/Command.h applications/bowlingGame/bowlingGameGL/InputMapper.cpp \
        applications/bowlingGame/bowlingGameGL/LaneCommands.h applications/bowlingGame/bowlingGameGL/LaneCommands.cpp \
        applications/bowlingGame/bowlingGameGL/test/test_Gameplay.cpp applications/bowlingGame/bowlingGameGL/test/test_InputMapper.cpp
git commit -m "feat(gl): ThrowResult for the HUD and sound; M toggles mute"
```

---

### Task 6: SoundDirector

**Files:**
- Create: `applications/bowlingGame/bowlingGameGL/SoundDirector.h`
- Create: `applications/bowlingGame/bowlingGameGL/SoundDirector.cpp`
- Modify: `applications/bowlingGame/bowlingGameGL/CMakeLists.txt`
- Test: `applications/bowlingGame/bowlingGameGL/test/test_SoundDirector.cpp`

**Interfaces:**
- Consumes: `Alley::Snapshot`, `Alley::Impact`, `Alley::Body`, `Alley::kLane` (Task 1); `Audio::IAudioOutput`, `SoundId`, `LoopSlot`, `Voice` (Task 4); `HudText::throwResult` (Task 5); `Render::ThemeId`.
- Produces:

```cpp
class SoundDirector {
public:
    explicit SoundDirector(Bowlingbox::Audio::IAudioOutput& out);
    void update(const Bowlingbox::Alley::Snapshot& s, const Bowlingbox::Score::IScoreSheet& sheet,
                std::span<const Bowlingbox::Alley::Impact> impacts, float dt);
    void onNewGame();
    void setTheme(Bowlingbox::Render::ThemeId id, bool announce);
};
```

- [ ] **Step 1: Write the failing tests**

Create `applications/bowlingGame/bowlingGameGL/test/test_SoundDirector.cpp`:

```cpp
#include "SoundDirector.h"
#include <Bowlingbox/Score/Score.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <optional>
#include <utility>
#include <vector>

using namespace Bowlingbox;
using Alley::Body;
using Alley::Impact;
using Alley::Phase;
using Audio::LoopSlot;
using Audio::SoundId;
using Audio::Voice;

namespace {
constexpr float kDt = 1.f / 60.f;

class RecordingOutput final : public Audio::IAudioOutput {
public:
    std::vector<std::pair<SoundId, Voice>> played;
    std::array<std::optional<SoundId>, Audio::kLoopSlotCount> loops {};
    std::array<Voice, Audio::kLoopSlotCount> loopVoices {};

    void play(SoundId id, const Voice& v) override { played.emplace_back(id, v); }
    void setLoop(LoopSlot slot, std::optional<SoundId> id, const Voice& v) override {
        loops[static_cast<std::size_t>(slot)] = id;
        loopVoices[static_cast<std::size_t>(slot)] = v;
    }
    void setMuted(bool m) override { _muted = m; }
    bool muted() const override { return _muted; }

    int count(SoundId id) const {
        return static_cast<int>(std::count_if(played.begin(), played.end(), [id](const auto& p) { return p.first == id; }));
    }
    std::optional<SoundId> loop(LoopSlot s) const { return loops[static_cast<std::size_t>(s)]; }
    const Voice& loopVoice(LoopSlot s) const { return loopVoices[static_cast<std::size_t>(s)]; }

private:
    bool _muted = false;
};

Alley::Snapshot at(Phase p, float x = 0.f, float z = 0.f, float y = 0.109f) {
    Alley::Snapshot s;
    s.phase = p;
    s.ball.position = {x, y, z};
    return s;
}

struct Fixture {
    RecordingOutput out;
    SoundDirector director {out};
    Score::ScoreCalculator sheet;
    void update(const Alley::Snapshot& s, std::vector<Impact> impacts = {}, float dt = kDt) {
        director.update(s, sheet, impacts, dt);
    }
};
}  // namespace

TEST(test_sound_director, first_update_only_records_the_snapshot) {
    Fixture f;
    f.update(at(Phase::Rolling, 0.f, 5.f), {{Body::Ball, Body::Pin, 3.f, {}}});
    EXPECT_TRUE(f.out.played.empty());
    EXPECT_FALSE(f.out.loop(LoopSlot::Roll));
}

TEST(test_sound_director, release_plays_once_when_charging_turns_into_rolling) {
    Fixture f;
    f.update(at(Phase::Charging));
    f.update(at(Phase::Rolling, 0.f, 0.1f));
    f.update(at(Phase::Rolling, 0.f, 0.2f));
    EXPECT_EQ(f.out.count(SoundId::Release), 1);
}

TEST(test_sound_director, charge_loop_pitch_follows_power_and_stops_after_release) {
    Fixture f;
    f.update(at(Phase::Aiming));
    auto s = at(Phase::Charging);
    s.power = 0.5f;
    f.update(s);
    EXPECT_EQ(f.out.loop(LoopSlot::Charge), SoundId::ChargeTone);
    EXPECT_NEAR(f.out.loopVoice(LoopSlot::Charge).pitch, 1.1f, 1e-5f);
    EXPECT_NEAR(f.out.loopVoice(LoopSlot::Charge).volume, 0.5f, 1e-5f);
    f.update(at(Phase::Rolling, 0.f, 0.1f));
    EXPECT_FALSE(f.out.loop(LoopSlot::Charge));
}

TEST(test_sound_director, roll_loop_volume_pitch_and_pan_follow_the_ball) {
    Fixture f;
    f.update(at(Phase::Rolling, 0.3825f, 0.f));
    f.update(at(Phase::Rolling, 0.3825f, 0.1f));  // 0.1 m in 1/60 s = 6 m/s
    ASSERT_EQ(f.out.loop(LoopSlot::Roll), SoundId::Roll);
    const Voice& v = f.out.loopVoice(LoopSlot::Roll);
    EXPECT_NEAR(v.volume, 0.625f, 1e-4f);  // 0.25 + 0.75 * 0.5
    EXPECT_NEAR(v.pitch, 1.0f, 1e-4f);     // 0.8 + 0.4 * 0.5
    EXPECT_NEAR(v.pan, 0.5f, 1e-4f);       // 0.3825 / 0.765
}

TEST(test_sound_director, gutter_lowers_the_roll_pitch_and_drops_once) {
    Fixture f;
    f.update(at(Phase::Rolling, 0.6f, 0.f));
    f.update(at(Phase::Rolling, 0.6f, 0.1f));
    f.update(at(Phase::Rolling, 0.6f, 0.2f));
    EXPECT_NEAR(f.out.loopVoice(LoopSlot::Roll).pitch, 0.85f, 1e-4f);
    EXPECT_EQ(f.out.count(SoundId::GutterDrop), 1);
}

TEST(test_sound_director, roll_stops_in_the_pit_when_slow_and_outside_rolling) {
    Fixture f;
    f.update(at(Phase::Rolling, 0.f, 0.f));
    f.update(at(Phase::Rolling, 0.f, 0.1f));
    ASSERT_TRUE(f.out.loop(LoopSlot::Roll));
    f.update(at(Phase::Settling, 0.f, 18.5f, -0.5f));  // in the pit
    EXPECT_FALSE(f.out.loop(LoopSlot::Roll));
    f.update(at(Phase::Settling, 0.f, 18.f));
    f.update(at(Phase::Settling, 0.f, 18.f));  // on the lane but not moving
    EXPECT_FALSE(f.out.loop(LoopSlot::Roll));
    f.update(at(Phase::Aiming, 0.f, 5.f));  // teleport while aiming: never a roll
    EXPECT_FALSE(f.out.loop(LoopSlot::Roll));
}

TEST(test_sound_director, impacts_map_to_hit_and_clatter_with_scaled_volume_and_pan) {
    Fixture f;
    f.update(at(Phase::Rolling));
    f.update(at(Phase::Rolling), {{Body::Pin, Body::Ball, 2.f, {0.3825f, 0.2f, 17.4f}},  // order does not matter
                                  {Body::Pin, Body::Lane, 0.75f, {-0.3825f, 0.f, 17.7f}},
                                  {Body::Pin, Body::Pin, 0.2f, {}},     // below threshold
                                  {Body::Ball, Body::Lane, 5.f, {}},    // no sound
                                  {Body::Pin, Body::Other, 5.f, {}}});  // unknown body
    ASSERT_EQ(f.out.played.size(), 2u);
    EXPECT_EQ(f.out.played[0].first, SoundId::PinHit);
    EXPECT_NEAR(f.out.played[0].second.volume, 0.5f, 1e-5f);
    EXPECT_NEAR(f.out.played[0].second.pan, 0.5f, 1e-4f);
    EXPECT_EQ(f.out.played[1].first, SoundId::PinClatter);
    EXPECT_NEAR(f.out.played[1].second.volume, 0.5f, 1e-5f);
    EXPECT_NEAR(f.out.played[1].second.pan, -0.5f, 1e-4f);
}

TEST(test_sound_director, at_most_four_impact_sounds_per_update_strongest_first) {
    Fixture f;
    f.update(at(Phase::Rolling));
    std::vector<Impact> burst;
    for (float impulse : {0.4f, 0.9f, 0.5f, 1.2f, 0.6f, 0.7f})
        burst.push_back({Body::Pin, Body::Pin, impulse, {}});
    f.update(at(Phase::Rolling), burst);
    ASSERT_EQ(f.out.played.size(), 4u);
    EXPECT_NEAR(f.out.played[0].second.volume, 0.8f, 1e-5f);  // 1.2 / 1.5 capped at 0.8
    EXPECT_NEAR(f.out.played[3].second.volume, 0.6f / 1.5f, 1e-5f);
    EXPECT_NEAR(f.out.played[0].second.pitch, 1.0f, 1e-5f);
    EXPECT_NEAR(f.out.played[1].second.pitch, 0.96f, 1e-5f);
}

TEST(test_sound_director, the_same_kind_waits_for_its_cooldown) {
    Fixture f;
    const std::vector<Impact> clatter {{Body::Pin, Body::Pin, 1.f, {}}};
    f.update(at(Phase::Rolling));
    f.update(at(Phase::Rolling), clatter);  // plays
    f.update(at(Phase::Rolling), clatter);  // 16 ms later: too soon
    f.update(at(Phase::Rolling), clatter);  // 33 ms after the first: plays
    EXPECT_EQ(f.out.count(SoundId::PinClatter), 2);
    f.update(at(Phase::Rolling), {{Body::Ball, Body::Pin, 1.f, {}}});  // the other kind is independent
    EXPECT_EQ(f.out.count(SoundId::PinHit), 1);
}

TEST(test_sound_director, strike_stinger_plays_once_on_entering_frame_done) {
    Fixture f;
    f.update(at(Phase::Settling));
    f.sheet.roll(10);
    auto s = at(Phase::FrameDone);
    s.lastPinsDown = 10;
    f.update(s);
    f.update(s);
    EXPECT_EQ(f.out.count(SoundId::Strike), 1);
}

TEST(test_sound_director, ten_after_a_gutter_ball_plays_the_spare_stinger) {
    Fixture f;
    f.update(at(Phase::Settling));
    f.sheet.roll(0);
    auto s = at(Phase::FrameDone);
    s.lastPinsDown = 0;
    f.update(s);
    EXPECT_EQ(f.out.count(SoundId::GutterJingle), 1);
    f.update(at(Phase::Settling));
    f.sheet.roll(10);
    s.lastPinsDown = 10;
    f.update(s);
    EXPECT_EQ(f.out.count(SoundId::Spare), 1);
    EXPECT_EQ(f.out.count(SoundId::Strike), 0);
}

TEST(test_sound_director, a_plain_count_plays_no_stinger) {
    Fixture f;
    f.update(at(Phase::Settling));
    f.sheet.roll(7);
    auto s = at(Phase::FrameDone);
    s.lastPinsDown = 7;
    f.update(s);
    EXPECT_TRUE(f.out.played.empty());
}

TEST(test_sound_director, theme_sets_the_ambience_and_announces_only_when_asked) {
    Fixture f;
    f.director.setTheme(Render::ThemeId::ClassicAlley, false);
    EXPECT_EQ(f.out.loop(LoopSlot::Ambient), SoundId::AmbientClassic);
    EXPECT_NEAR(f.out.loopVoice(LoopSlot::Ambient).volume, 0.35f, 1e-5f);
    EXPECT_EQ(f.out.count(SoundId::ThemeSwitch), 0);
    f.director.setTheme(Render::ThemeId::Outdoor, true);
    EXPECT_EQ(f.out.loop(LoopSlot::Ambient), SoundId::AmbientOutdoor);
    EXPECT_EQ(f.out.count(SoundId::ThemeSwitch), 1);
    f.director.setTheme(Render::ThemeId::NeonHall, true);
    EXPECT_EQ(f.out.loop(LoopSlot::Ambient), SoundId::AmbientNeon);
}

TEST(test_sound_director, new_game_chimes_and_stops_roll_and_charge_but_not_the_ambience) {
    Fixture f;
    f.director.setTheme(Render::ThemeId::NeonHall, false);
    f.update(at(Phase::Rolling, 0.f, 0.f));
    f.update(at(Phase::Rolling, 0.f, 0.1f));
    ASSERT_TRUE(f.out.loop(LoopSlot::Roll));
    f.director.onNewGame();
    EXPECT_EQ(f.out.count(SoundId::NewGame), 1);
    EXPECT_FALSE(f.out.loop(LoopSlot::Roll));
    EXPECT_FALSE(f.out.loop(LoopSlot::Charge));
    EXPECT_EQ(f.out.loop(LoopSlot::Ambient), SoundId::AmbientNeon);
}
```

Register it in `applications/bowlingGame/bowlingGameGL/CMakeLists.txt` (test `SRC`), and add the director to the core library:

```cmake
add_library(bowlinggl_core STATIC
    Command.h
    FixedStep.h FixedStep.cpp
    InputMapper.h InputMapper.cpp
    LaneCommands.h LaneCommands.cpp
    FollowCamera.h FollowCamera.cpp
    HudText.h HudText.cpp
    Hud.h Hud.cpp
    SceneAdapter.h SceneAdapter.cpp
    SoundDirector.h SoundDirector.cpp
)
target_include_directories(bowlinggl_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bowlinggl_core PUBLIC
    ${CMAKE_ROOT_NAME}::Alley
    ${CMAKE_ROOT_NAME}::Audio
    ${CMAKE_ROOT_NAME}::Render
    wx::core
    wx::base
)
```

```cmake
        SRC      test/test_InputMapper.cpp test/test_Gameplay.cpp test/test_SoundDirector.cpp
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `cmake -S . -B build && cmake --build build --target test_bowlinggl 2>&1 | tail -5`
Expected: build error `SoundDirector.h: No such file or directory` (or CMake reporting the missing `SoundDirector.cpp`).

- [ ] **Step 3: Write the header**

Create `applications/bowlingGame/bowlingGameGL/SoundDirector.h`:

```cpp
#ifndef BOWLINGGL_SOUND_DIRECTOR_H
#define BOWLINGGL_SOUND_DIRECTOR_H
#include <Bowlingbox/Alley/Types.h>
#include <Bowlingbox/Audio/Audio.h>
#include <Bowlingbox/Render/Environment.h>
#include <Bowlingbox/Score/IBowlingGame.h>
#include <optional>
#include <span>

/// Turns lane snapshots, the score sheet and physics impacts into sound cues.
/// Pure logic: it only talks to the IAudioOutput it was given.
class SoundDirector {
public:
    static constexpr float kRollFullSpeed = 12.f;   // m/s: loudest, highest roll
    static constexpr float kRollMinSpeed = 0.2f;    // m/s
    static constexpr float kPitY = -0.1f;           // ball centre below this: in the pit
    static constexpr float kPanHalfWidth = 0.765f;  // lane half width + gutter
    static constexpr float kGutterPitch = 0.85f;
    static constexpr float kMinImpulse = 0.3f;      // N·s, matches BulletWorld's record threshold
    static constexpr float kHitFullImpulse = 4.f;
    static constexpr float kClatterFullImpulse = 1.5f;
    static constexpr int kMaxImpactsPerUpdate = 4;
    static constexpr float kImpactCooldown = 0.03f;  // s, per kind
    static constexpr float kAmbientVolume = 0.35f;
    static constexpr float kChargeVolume = 0.5f;
    static constexpr float kReleaseVolume = 0.8f;
    static constexpr float kGutterVolume = 0.7f;
    static constexpr float kStingerVolume = 0.8f;
    static constexpr float kChimeVolume = 0.6f;

    explicit SoundDirector(Bowlingbox::Audio::IAudioOutput& out);

    /// Call once per timer tick after stepping the lane; dt is the simulated time of the tick.
    void update(const Bowlingbox::Alley::Snapshot& s, const Bowlingbox::Score::IScoreSheet& sheet,
                std::span<const Bowlingbox::Alley::Impact> impacts, float dt);
    void onNewGame();
    /// Switches the ambience; announce plays the theme-switch chime.
    void setTheme(Bowlingbox::Render::ThemeId id, bool announce);

private:
    void playTransitions(const Bowlingbox::Alley::Snapshot& prev, const Bowlingbox::Alley::Snapshot& s,
                         const Bowlingbox::Score::IScoreSheet& sheet);
    void updateCharge(const Bowlingbox::Alley::Snapshot& s);
    void updateRoll(const Bowlingbox::Alley::Snapshot& s);
    void playImpacts(std::span<const Bowlingbox::Alley::Impact> impacts);

    Bowlingbox::Audio::IAudioOutput& _out;
    std::optional<Bowlingbox::Alley::Snapshot> _prev;
    float _ballSpeed = 0.f;
    bool _gutterPlayed = false;
    float _sinceHit = kImpactCooldown;
    float _sinceClatter = kImpactCooldown;
    int _clatterIndex = 0;
};
#endif  // BOWLINGGL_SOUND_DIRECTOR_H
```

- [ ] **Step 4: Write the implementation**

Create `applications/bowlingGame/bowlingGameGL/SoundDirector.cpp`:

```cpp
#include "SoundDirector.h"
#include "HudText.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

using Bowlingbox::Alley::Body;
using Bowlingbox::Alley::Impact;
using Bowlingbox::Alley::Phase;
using Bowlingbox::Alley::Snapshot;
using Bowlingbox::Audio::LoopSlot;
using Bowlingbox::Audio::SoundId;
using Bowlingbox::Audio::Voice;

namespace {
constexpr std::array<float, 5> kClatterPitches {1.0f, 0.96f, 1.04f, 0.98f, 1.02f};

float pan(float x) {
    return std::clamp(x / SoundDirector::kPanHalfWidth, -1.f, 1.f);
}

SoundId ambientFor(Bowlingbox::Render::ThemeId id) {
    switch (id) {
        case Bowlingbox::Render::ThemeId::ClassicAlley: return SoundId::AmbientClassic;
        case Bowlingbox::Render::ThemeId::Outdoor:      return SoundId::AmbientOutdoor;
        case Bowlingbox::Render::ThemeId::NeonHall:     break;
    }
    return SoundId::AmbientNeon;
}

std::optional<SoundId> stingerFor(HudText::ThrowResult r) {
    switch (r) {
        case HudText::ThrowResult::Strike: return SoundId::Strike;
        case HudText::ThrowResult::Spare:  return SoundId::Spare;
        case HudText::ThrowResult::Gutter: return SoundId::GutterJingle;
        case HudText::ThrowResult::Pins:
        case HudText::ThrowResult::None:   break;
    }
    return std::nullopt;
}

enum class ImpactKind { None, Hit, Clatter };

ImpactKind kindOf(const Impact& i) {
    if (i.a == Body::Other || i.b == Body::Other || i.impulse < SoundDirector::kMinImpulse)
        return ImpactKind::None;
    const bool ballPin = (i.a == Body::Ball && i.b == Body::Pin) || (i.a == Body::Pin && i.b == Body::Ball);
    if (ballPin)
        return ImpactKind::Hit;
    if (i.a == Body::Pin || i.b == Body::Pin)
        return ImpactKind::Clatter;
    return ImpactKind::None;
}
}  // namespace

SoundDirector::SoundDirector(Bowlingbox::Audio::IAudioOutput& out) : _out(out) {}

void SoundDirector::update(const Snapshot& s, const Bowlingbox::Score::IScoreSheet& sheet,
                           std::span<const Impact> impacts, float dt) {
    _sinceHit += dt;
    _sinceClatter += dt;
    if (!_prev) {
        _prev = s;  // nothing to compare with yet
        return;
    }
    if (dt > 0.f) {
        const auto& a = _prev->ball.position;
        const auto& b = s.ball.position;
        const float dx = b[0] - a[0], dy = b[1] - a[1], dz = b[2] - a[2];
        _ballSpeed = std::sqrt(dx * dx + dy * dy + dz * dz) / dt;
    }
    playTransitions(*_prev, s, sheet);
    updateCharge(s);
    updateRoll(s);
    playImpacts(impacts);
    _prev = s;
}

void SoundDirector::onNewGame() {
    _out.play(SoundId::NewGame, {kChimeVolume, 1.f, 0.f});
    _out.setLoop(LoopSlot::Roll, std::nullopt, {});
    _out.setLoop(LoopSlot::Charge, std::nullopt, {});
    _prev.reset();
    _gutterPlayed = false;
}

void SoundDirector::setTheme(Bowlingbox::Render::ThemeId id, bool announce) {
    _out.setLoop(LoopSlot::Ambient, ambientFor(id), {kAmbientVolume, 1.f, 0.f});
    if (announce)
        _out.play(SoundId::ThemeSwitch, {kChimeVolume, 1.f, 0.f});
}

void SoundDirector::playTransitions(const Snapshot& prev, const Snapshot& s,
                                    const Bowlingbox::Score::IScoreSheet& sheet) {
    if (prev.phase == Phase::Charging && s.phase == Phase::Rolling) {
        _gutterPlayed = false;
        _out.play(SoundId::Release, {kReleaseVolume, 1.f, pan(s.ball.position[0])});
    }
    if (prev.phase != Phase::FrameDone && s.phase == Phase::FrameDone)
        if (const auto id = stingerFor(HudText::throwResult(s, sheet)))
            _out.play(*id, {kStingerVolume, 1.f, 0.f});
}

void SoundDirector::updateCharge(const Snapshot& s) {
    if (s.phase == Phase::Charging)
        _out.setLoop(LoopSlot::Charge, SoundId::ChargeTone, {kChargeVolume, 0.8f + 0.6f * s.power, 0.f});
    else
        _out.setLoop(LoopSlot::Charge, std::nullopt, {});
}

void SoundDirector::updateRoll(const Snapshot& s) {
    const auto& p = s.ball.position;
    const bool inPlay = s.phase == Phase::Rolling || s.phase == Phase::Settling;
    const bool inGutter = std::abs(p[0]) > Bowlingbox::Alley::kLane.halfWidth;
    if (inGutter && s.phase == Phase::Rolling && !_gutterPlayed) {
        _gutterPlayed = true;
        _out.play(SoundId::GutterDrop, {kGutterVolume, 1.f, pan(p[0])});
    }
    if (!inPlay || p[1] <= kPitY || _ballSpeed <= kRollMinSpeed) {
        _out.setLoop(LoopSlot::Roll, std::nullopt, {});
        return;
    }
    const float t = std::clamp(_ballSpeed / kRollFullSpeed, 0.f, 1.f);
    float pitch = 0.8f + 0.4f * t;
    if (inGutter)
        pitch *= kGutterPitch;
    _out.setLoop(LoopSlot::Roll, SoundId::Roll, {0.25f + 0.75f * t, pitch, pan(p[0])});
}

void SoundDirector::playImpacts(std::span<const Impact> impacts) {
    std::vector<const Impact*> loud;
    for (const Impact& i : impacts)
        if (kindOf(i) != ImpactKind::None)
            loud.push_back(&i);
    std::stable_sort(loud.begin(), loud.end(), [](const Impact* x, const Impact* y) { return x->impulse > y->impulse; });

    const bool hitReady = _sinceHit >= kImpactCooldown;
    const bool clatterReady = _sinceClatter >= kImpactCooldown;
    bool hitPlayed = false;
    bool clatterPlayed = false;
    int played = 0;
    for (const Impact* i : loud) {
        if (played == kMaxImpactsPerUpdate)
            break;
        const float x = pan(i->position[0]);
        if (kindOf(*i) == ImpactKind::Hit) {
            if (!hitReady)
                continue;
            _out.play(SoundId::PinHit, {std::clamp(i->impulse / kHitFullImpulse, 0.15f, 1.f), 1.f, x});
            hitPlayed = true;
        } else {
            if (!clatterReady)
                continue;
            const float pitch = kClatterPitches[static_cast<std::size_t>(_clatterIndex)];
            _clatterIndex = (_clatterIndex + 1) % static_cast<int>(kClatterPitches.size());
            _out.play(SoundId::PinClatter, {std::clamp(i->impulse / kClatterFullImpulse, 0.1f, 0.8f), pitch, x});
            clatterPlayed = true;
        }
        ++played;
    }
    if (hitPlayed)
        _sinceHit = 0.f;
    if (clatterPlayed)
        _sinceClatter = 0.f;
}
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `cmake --build build --target test_bowlinggl && ./build/bin/test_bowlinggl --gtest_filter='test_sound_director.*'`
Expected: all 14 `test_sound_director` tests PASS. Then run `./build/bin/test_bowlinggl` with no filter: everything PASS.

- [ ] **Step 6: Commit**

```bash
git add applications/bowlingGame/bowlingGameGL/SoundDirector.h applications/bowlingGame/bowlingGameGL/SoundDirector.cpp \
        applications/bowlingGame/bowlingGameGL/CMakeLists.txt applications/bowlingGame/bowlingGameGL/test/test_SoundDirector.cpp
git commit -m "feat(gl): SoundDirector maps snapshots, results and impacts to sound cues"
```

---

### Task 7: Wire sound into BowlingGL; docs and notices

**Files:**
- Modify: `applications/bowlingGame/bowlingGameGL/BowlingCanvas.h`
- Modify: `applications/bowlingGame/bowlingGameGL/BowlingCanvas.cpp`
- Modify: `applications/bowlingGame/bowlingGameGL/BowlingFrame.h`
- Modify: `applications/bowlingGame/bowlingGameGL/BowlingFrame.cpp`
- Modify: `applications/bowlingGame/bowlingGameGL/main.cpp`
- Modify: `applications/bowlingGame/README.md`
- Modify: `docs/MANUAL.md`
- Modify: `docs/ARCHITECTURE.md`
- Modify: `installer/THIRD_PARTY_NOTICES.txt`

**Interfaces:**
- Consumes: `SoundDirector` (Task 6), `Audio::createDeviceOutput`, `Audio::makeSilentOutput` (Task 4), `Lane::drainImpacts` (Task 1), `Command::Type::ToggleMute` (Task 5).
- Produces: `BowlingCanvas::setMuted(bool)`, `BowlingCanvas::muted() const`, `BowlingCanvas::onMuteChanged`; `BowlingFrame(Render::ThemeId initialTheme = NeonHall, bool muted = false)`; `--mute` switch.

This task is wx window code with no unit tests in the project; verification is the build, the full test suite and the manual checks in Step 6.

- [ ] **Step 1: Canvas header**

In `BowlingCanvas.h` add the includes:

```cpp
#include "SoundDirector.h"
#include <Bowlingbox/Audio/Audio.h>
```

In the public section, after `onThemeChanged`:

```cpp
    void setMuted(bool muted);
    bool muted() const;
    std::function<void(bool)> onMuteChanged;  // menu sync
```

At the end of the private members (after `_themeKeyHeld`), keeping this order so the director is destroyed before its output:

```cpp
    bool _muteKeyHeld = false;   // auto-repeat would otherwise flicker the mute
    bool _themeChosen = false;   // the first setTheme starts the ambience without a chime
    std::unique_ptr<Bowlingbox::Audio::IAudioOutput> _audio;
    SoundDirector _sound;        // talks to *_audio
```

- [ ] **Step 2: Canvas implementation**

In `BowlingCanvas.cpp`, add an anonymous namespace after `using Alley::Phase;`:

```cpp
namespace {
std::unique_ptr<Audio::IAudioOutput> createAudio() {
    auto device = Audio::createDeviceOutput();
    if (device.isValue())
        return device();
    std::fprintf(stderr, "sound disabled: %s\n", device.error().c_str());
    return Audio::makeSilentOutput();
}
}  // namespace
```

Constructor initialiser list:

```cpp
    : wxGLCanvas(parent, attrs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS),
      _timer(this),
      _audio(createAudio()),
      _sound(*_audio) {
```

`setTheme`, first lines:

```cpp
void BowlingCanvas::setTheme(Render::ThemeId id) {
    _sound.setTheme(id, _themeChosen);
    _themeChosen = true;
    _theme = id;
```

Add after `setTheme`:

```cpp
void BowlingCanvas::setMuted(bool muted) {
    _audio->setMuted(muted);
    if (onMuteChanged)
        onMuteChanged(muted);
}

bool BowlingCanvas::muted() const {
    return _audio->muted();
}
```

`OnTimer`, replace the snapshot line with:

```cpp
    const auto s = _lane.snapshot();
    const auto impacts = _lane.drainImpacts();
    _sound.update(s, _lane.scoreSheet(), impacts, static_cast<float>(steps) * FixedStep::kDt);
```

`OnKeyDown`, after the `CycleTheme` guard:

```cpp
    if (c.type == Command::Type::ToggleMute) {
        if (_muteKeyHeld) return;
        _muteKeyHeld = true;
    }
```

`OnKeyUp`, after the `'T'` line:

```cpp
    if (e.GetKeyCode() == 'M' || e.GetKeyCode() == 'm') _muteKeyHeld = false;
```

`apply`, after the `CycleTheme` block and around the lane call:

```cpp
    if (command.type == Command::Type::ToggleMute) {
        setMuted(!muted());
        return;
    }
    auto r = LaneCommands::execute(command, _lane);
    if (r.isError())
        wxLogDebug("command rejected: %s", r.error().c_str());
    else if (command.type == Command::Type::NewGame)
        _sound.onNewGame();
```

- [ ] **Step 3: Frame and command line**

`BowlingFrame.h`:

```cpp
    explicit BowlingFrame(Bowlingbox::Render::ThemeId initialTheme = Bowlingbox::Render::ThemeId::NeonHall,
                          bool muted = false);
```

and a private handler `void OnMute(wxCommandEvent&);`.

`BowlingFrame.cpp`: in the anonymous namespace add `constexpr int kMuteMenuId = kThemeMenuBase + 100;  // after the theme range`. Change the constructor signature to `BowlingFrame::BowlingFrame(Bowlingbox::Render::ThemeId initialTheme, bool muted)`. After `view->AppendSubMenu(themes, "&Theme");` add:

```cpp
    view->AppendCheckItem(kMuteMenuId, "&Mute sound");
```

After the other `Bind` calls:

```cpp
    Bind(wxEVT_MENU, &BowlingFrame::OnMute, this, kMuteMenuId);
```

After `_canvas->setTheme(initialTheme);`:

```cpp
    _canvas->onMuteChanged = [this](bool m) { GetMenuBar()->Check(kMuteMenuId, m); };
    if (muted)
        _canvas->setMuted(true);
```

Add the handler:

```cpp
void BowlingFrame::OnMute(wxCommandEvent& e) {
    _canvas->setMuted(e.IsChecked());
}
```

`main.cpp`: in `OnInitCmdLine` add `parser.AddLongSwitch("mute", "start with the sound muted");`; in `OnCmdLineParsed` add `_muted = parser.Found("mute");`; add a member `bool _muted = false;`; in `OnInit` use `new BowlingFrame(_theme, _muted)`.

- [ ] **Step 4: Build and run every test**

Run: `cmake --build build -j && ctest --test-dir build --output-on-failure`
Expected: build succeeds without warnings; all tests pass.

- [ ] **Step 5: Docs and notices**

`applications/bowlingGame/README.md`: add the row `| Mute / unmute sound | M | View ▸ Mute sound |` after "Change scenery"; after the `--theme` paragraph add `` `./build/bin/BowlingGL --mute` starts with the sound off. Without an audio device the game prints `sound disabled: ...` once and runs silently. ``; append to the manual checklist:

```markdown
10. With sound on, a pocket throw plays: release thud, rolling that rises with speed, pin hits and clatter, then the strike jingle; the ambience matches the theme and changes with `T` (with a chime).
11. `M` mutes everything and unmutes; holding `M` toggles once; View ▸ Mute sound stays in sync; muted + `T` stays silent.
12. `--mute` starts silent with View ▸ Mute sound checked. With no audio device (e.g. `env -u PULSE_SERVER -u XDG_RUNTIME_DIR ALSA_CONFIG_PATH=/dev/null ./build/bin/BowlingGL` on a machine without PipeWire/Pulse access) one `sound disabled:` line appears and the game runs.
```

`docs/MANUAL.md`: add `| Mute / unmute sound | M | View ▸ Mute sound |` after the "Change scenery" row, and after the Scenery section add:

```markdown
### Sound

BowlingGL plays the ball, the pins, strike / spare / miss jingles and a background ambience for each
theme. Press `M` or use View ▸ Mute sound to mute; `--mute` starts muted. Without an audio device
the game prints `sound disabled: <reason>` once and stays silent.
```

`docs/ARCHITECTURE.md`: in the mermaid graph add `AUDIO[Bowlingbox::Audio<br/>miniaudio + embedded sounds]` to `Libraries`, `MINIAUDIO[miniaudio]` to `Third-party`, and the edges `GL --> AUDIO`, `AUDIO --> MINIAUDIO`, `AUDIO --> CORE`. After the Render section add:

```markdown
### Bowlingbox::Audio

Sound output behind `IAudioOutput` (`play`, `setLoop` with three slots, `setMuted`).
`createDeviceOutput()` uses miniaudio and the 14 WAV files embedded from `3rdparty/sounds`;
`makeSilentOutput()` is the fallback without a device. BowlingGL's `SoundDirector` decides what
plays from the lane snapshot, the score sheet and `Lane::drainImpacts()` (new Bullet contacts).
```

`installer/THIRD_PARTY_NOTICES.txt`, in "Components whose licences need no notice in binary copies", add:

```text
  - miniaudio 0.11.25      public domain (Unlicense option)
  - sound effects          CC0 1.0 (Kenney, qubodup, StarNinjas, Kresiek The Furry)
```

- [ ] **Step 6: Manual playtest (human partner)**

Run `./build/bin/BowlingGL` and walk the README checklist items 1–12. Confirm by ear that each sample fits; for any that does not, pick another file from the same pack, rerun its `sound_convert` line from `3rdparty/sounds/SOURCES.md`, update that row, rebuild and re-run `./build/bin/test_bowlingbox_audio`. If impact volumes feel off, adjust `kHitFullImpulse` / `kClatterFullImpulse` in `SoundDirector.h` and update the expected volumes in `test_SoundDirector.cpp` to match.

- [ ] **Step 7: Commit**

```bash
git add applications/bowlingGame/bowlingGameGL/BowlingCanvas.h applications/bowlingGame/bowlingGameGL/BowlingCanvas.cpp \
        applications/bowlingGame/bowlingGameGL/BowlingFrame.h applications/bowlingGame/bowlingGameGL/BowlingFrame.cpp \
        applications/bowlingGame/bowlingGameGL/main.cpp applications/bowlingGame/README.md docs/MANUAL.md \
        docs/ARCHITECTURE.md installer/THIRD_PARTY_NOTICES.txt
git commit -m "feat(gl): play sound effects, M / View menu / --mute to silence them"
```
