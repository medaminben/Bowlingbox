# Sound Effects — Design

Date: 2026-10-17. Branch: `feature/sound-effects`, based on `feature/modern-graphics`.

## Goal

BowlingGL is silent. It should sound like a bowling alley: the ball rolls, pins crash in proportion
to the hit, a gutter ball drops, strikes and spares are celebrated, the controls give audible
feedback, and each scenery has its own background ambience. The player can mute it.

## Decisions

- Scope: BowlingGL only. The console (`bowlingGameCMD`) and score-sheet (`bowlingGameUI`) front-ends
  stay silent.
- Backend: **miniaudio**, vendored in `3rdparty/miniaudio/` (public domain / MIT-0). It mixes any
  number of voices, sets volume, pitch and pan per voice, decodes WAV/FLAC/MP3 from memory and talks
  to PipeWire/PulseAudio/ALSA on Linux (loaded at run time, no dev packages) and WASAPI on Windows.
  `wxSound` was rejected: the system wxGTK 3.2.8 is built with `wxUSE_LIBSDL 0`, so it only has the
  OSS backend (`/dev/dsp`, absent on this machine), and it plays one sound at a time with no volume
  or pitch control.
- Triggers: snapshot diffs **plus** physics impact events reported by Bullet, so a light tap clinks
  and a strike crashes.
- Assets: CC0 recorded samples, committed as mono FLAC in `3rdparty/sounds/`, embedded into the
  binary like the Rubik font. Sources: Kenney.nl packs and OpenGameArt.org entries licensed CC0
  (no login needed). Every file is listed with URL, author and licence in
  `3rdparty/sounds/SOURCES.md`.
- Controls: `M` toggles mute, View ▸ **Mute sound** (check item) does the same, `--mute` starts muted.
  No volume sliders.
- No audio device (CI, headless, broken driver): the game starts silently and prints one line to
  stderr. No dialog.
- Physics results must not change: impact collection only reads contact manifolds.

## Sound catalogue

| `SoundId` | File | Kind | Trigger |
|---|---|---|---|
| `Roll` | `roll.flac` | loop (slot `Roll`) | ball moving on the lane or in a gutter |
| `GutterDrop` | `gutter_drop.flac` | one-shot | ball crosses \|x\| > `kLane.halfWidth` while rolling |
| `PinHit` | `pin_hit.flac` | one-shot | ball↔pin impact |
| `PinClatter` | `pin_clatter.flac` | one-shot | pin↔pin, pin↔lane, pin↔gutter, pin↔pit impact |
| `ChargeTone` | `charge.flac` | loop (slot `Charge`) | phase `Charging` |
| `Release` | `release.flac` | one-shot | phase `Charging` → `Rolling` |
| `Strike` | `strike.flac` | one-shot | enter `FrameDone`, all 10 pins of a full rack down |
| `Spare` | `spare.flac` | one-shot | enter `FrameDone`, all remaining pins (< 10) down |
| `GutterJingle` | `miss.flac` | one-shot | enter `FrameDone` with 0 pins down |
| `NewGame` | `new_game.flac` | one-shot | `SoundDirector::onNewGame()` |
| `ThemeSwitch` | `theme_switch.flac` | one-shot | `setTheme(id, announce = true)` |
| `AmbientNeon` | `ambient_neon.flac` | loop (slot `Ambient`) | theme Neon Hall |
| `AmbientClassic` | `ambient_classic.flac` | loop (slot `Ambient`) | theme Classic Alley |
| `AmbientOutdoor` | `ambient_outdoor.flac` | loop (slot `Ambient`) | theme Outdoor |

Asset rules: mono, 44.1 kHz or 22.05 kHz FLAC; one-shots ≤ 3 s; loops 5–20 s and seamless; total
size of `3rdparty/sounds/*.flac` < 3 MB. Peak-normalise to −1 dBFS; the code sets relative levels.

## Alley: impact events

`Types.h` gains:

```cpp
enum class Body { Ball, Pin, Lane, Gutter, Pit, Other };

struct Impact {
    Body a = Body::Other;
    Body b = Body::Other;
    float impulse = 0.f;                       // N·s, Bullet applied impulse
    std::array<float, 3> position {0.f, 0.f, 0.f};  // world, metres
};
```

`IPhysicsWorld` gains `virtual std::vector<Impact> drainImpacts() = 0;` — returns impacts recorded
since the last call, oldest first, and empties the buffer. `Lane` gains
`std::vector<Impact> drainImpacts();` which forwards to the world.

`BulletWorld`:
- Each rigid body gets a `Body` tag in its user index.
- An internal tick callback runs after every Bullet sub-step and, for every contact point with
  `getLifeTime() == 1` (new this sub-step) and `getAppliedImpulse() >= kImpactRecordThreshold`
  (0.02 N·s), records an `Impact` (pair order: the lower `Body` enum value first).
- Buffer cap 256; when full, further impacts in that tick are dropped.
- `rebuild()` and `removePin()` clear the buffer.

## Audio library

New library `Bowlingbox::Audio` (`libraries/Audio`), created by hand (so the scaffold generator
skips it) and listed in `BOWLINGBOX_LIBRARIES` and `libraries/CMakeLists.txt`.

```cpp
namespace Bowlingbox::Audio {
enum class SoundId { Roll, GutterDrop, PinHit, PinClatter, ChargeTone, Release, Strike, Spare,
                     GutterJingle, NewGame, ThemeSwitch, AmbientNeon, AmbientClassic, AmbientOutdoor };
inline constexpr int kSoundCount = 14;
enum class LoopSlot { Roll, Charge, Ambient };

struct Voice {
    float volume = 1.f;  // [0, 1]
    float pitch = 1.f;   // playback rate, > 0
    float pan = 0.f;     // [-1 left, +1 right]
    bool operator==(const Voice&) const = default;
};

class IAudioOutput {
public:
    virtual ~IAudioOutput() = default;
    virtual void play(SoundId id, const Voice& voice = {}) = 0;
    /// nullopt stops the slot; the same id updates the voice without restarting;
    /// a different id restarts the slot with that sound.
    virtual void setLoop(LoopSlot slot, std::optional<SoundId> id, const Voice& voice = {}) = 0;
    virtual void setMuted(bool muted) = 0;
    virtual bool muted() const = 0;
};

Result<std::unique_ptr<IAudioOutput>> createDeviceOutput();  // miniaudio engine
std::unique_ptr<IAudioOutput> makeSilentOutput();            // never fails, plays nothing
std::string_view fileName(SoundId id);                       // e.g. "roll.flac"
std::span<const unsigned char> embeddedData(SoundId id);     // FLAC bytes
}
```

- Device output: one `ma_engine`; the 14 embedded FLACs are registered with the resource manager
  via `ma_resource_manager_register_encoded_data` under their file names and decoded on load.
  One-shots use a pool of at most 16 `ma_sound`s; when all are busy, the oldest one stops and is
  reused. Mute sets the engine volume to 0 (loops keep running, so unmuting resumes in place).
- Silent output: records nothing, `muted()` reflects `setMuted`.
- `cmake/EmbedFile.cmake` gains an optional `NAMESPACE` argument; the default stays
  `Bowlingbox::Render::embedded`, Audio uses `Bowlingbox::Audio::embedded`.
- Configure fails with a clear message if a `.flac` in `3rdparty/sounds/` is missing from
  `SOURCES.md` or a `SoundId` file is missing on disk.
- miniaudio is compiled in one private C translation unit with `MA_NO_ENCODING` and
  `MA_NO_GENERATION` defined; `MA_NO_ENGINE`, `MA_NO_NODE_GRAPH` and `MA_NO_RESOURCE_MANAGER` must
  stay undefined (the engine needs them). Linux links `Threads::Threads`, `m` and
  `${CMAKE_DL_LIBS}`.

## SoundDirector

Pure, window-free class in `bowlinggl_core`; owns no audio resources.

```cpp
class SoundDirector {
public:
    explicit SoundDirector(Bowlingbox::Audio::IAudioOutput& out);
    void update(const Bowlingbox::Alley::Snapshot& s,
                std::span<const Bowlingbox::Alley::Impact> impacts, float dt);
    void onNewGame();
    void setTheme(Bowlingbox::Render::ThemeId id, bool announce);
};
```

Rules (constants live in `SoundDirector.h`):

- **Roll loop** is active while phase is `Rolling` or `Settling`, the ball y > −0.1 (not in the pit)
  and ball speed > 0.2 m/s. Speed is the ball position change divided by `dt` (kept from the last
  tick when `dt == 0`). `t = clamp(speed / 12, 0, 1)`; volume `0.25 + 0.75·t`, pitch `0.8 + 0.4·t`,
  ×0.85 while \|x\| > `kLane.halfWidth`; pan `clamp(x / 0.765, −1, 1)`. Otherwise the slot is stopped.
- **Gutter drop** plays once per throw on the first tick with \|ball x\| > `kLane.halfWidth` during
  `Rolling`. Volume 0.7, pan from x.
- **Impacts**: ignore those below 0.05 N·s and any involving `Body::Other`. Ball↔pin → `PinHit`,
  volume `clamp(impulse / 4.0, 0.15, 1)`. Other pairs with at least one pin → `PinClatter`, volume
  `clamp(impulse / 1.5, 0.1, 0.8)`. Ball↔lane/gutter/pit → no sound. Per update at most 4 impact
  sounds, strongest first; each of the two kinds has a 30 ms cooldown (measured in summed `dt`).
  Pan from impact x. Clatter pitch cycles deterministically through {1.0, 0.96, 1.04, 0.98, 1.02}.
- **Charge loop** while phase is `Charging`: volume 0.5, pitch `0.8 + 0.6·power`. Stopped otherwise.
- **Release** on the `Charging` → `Rolling` transition, volume 0.8. The number of pins in play is
  recorded at this moment (`pinsAtRelease`).
- **Result stinger** on entering `FrameDone`: `lastPinsDown == 10 && pinsAtRelease == 10` → `Strike`;
  `lastPinsDown == pinsAtRelease && pinsAtRelease > 0` → `Spare`; `lastPinsDown == 0` → `GutterJingle`.
  Volume 0.8.
- **New game** plays `NewGame` (0.6), stops `Roll` and `Charge`, and resets throw state.
- **Theme**: `setTheme` sets the `Ambient` slot to the theme's loop at volume 0.35; with `announce`
  it also plays `ThemeSwitch` (0.6).
- The first `update` call only records the snapshot (no transitions are inferred from a default).

## App wiring

- `Command::Type::ToggleMute`; `InputMapper::keyDown` maps `M`/`m` to it in every phase.
  `LaneCommands::execute` treats it like `CycleTheme` (no lane effect).
- `BowlingCanvas` owns `std::unique_ptr<Audio::IAudioOutput>` and a `SoundDirector`. In the
  constructor it calls `createDeviceOutput()`; on error it prints
  `sound disabled: <reason>` to stderr once and uses `makeSilentOutput()`.
- `OnTimer`: after stepping the lane, `_sound.update(snapshot, _lane.drainImpacts(), steps·kDt)`.
- `newGame()` / the `NewGame` command call `_sound.onNewGame()`; `setTheme` calls
  `_sound.setTheme(id, announce)` with `announce = false` for the initial theme.
- `M` is guarded against key auto-repeat like `T`. `setMuted(bool)` on the canvas updates the output
  and calls `onMuteChanged(bool)`, which `BowlingFrame` uses to keep View ▸ Mute sound checked.
- `--mute` (`wxCmdLineParser::AddSwitch`) is passed to `BowlingFrame`, which mutes the canvas.
- `HudText::controlsHelp()` gains `M: mute sound`; the status line is unchanged.

## Licensing and docs

- `licenses/miniaudio.txt` (its public domain / MIT-0 text), `licenses/sounds.txt` (CC0 1.0 text
  plus the per-file attribution list from `SOURCES.md`), `licenses/README.md`,
  `3rdparty/README.md` and `installer/THIRD_PARTY_NOTICES.txt` updated.
- `applications/bowlingGame/README.md`: the `M` key, `--mute`, and three manual checklist items:
  sounds play during a strike throw; `M` mutes and unmutes; `--mute` starts silent.

## Testing

- **Alley:** `FakeWorld` returns queued impacts; `Lane::drainImpacts()` forwards them and empties.
  Bullet integration test: a centred full-power throw yields at least one ball↔pin impact and
  `drainImpacts()` is empty right after; `rebuild()` clears pending impacts. Existing physics tests
  stay green (determinism).
- **Audio:** every `SoundId` has a non-empty file name and embedded data; each decodes with
  `ma_decoder_init_memory` (no device) to mono, 22.05 or 44.1 kHz, within the duration limits;
  silent output honours `setMuted`. No test opens a device.
- **SoundDirector:** a recording fake `IAudioOutput` checks each catalogue trigger, the roll voice
  formulae, the impulse thresholds and the 4-per-update cap, the cooldown, the first-update rule,
  theme announce on/off, and that `onNewGame` stops loops.
- **InputMapper:** `M` → `ToggleMute` in every phase.

## Out of scope

Volume sliders, settings persistence, 3D positional audio, sound in the console or score-sheet
front-ends, music.
