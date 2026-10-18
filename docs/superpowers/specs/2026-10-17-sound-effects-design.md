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
- Assets: CC0 recorded samples, committed as mono 16-bit 22.05 kHz WAV in `3rdparty/sounds/`,
  embedded into the binary like the Rubik font. Sources: Kenney.nl packs and OpenGameArt.org entries
  licensed CC0 (no login needed). Every file is listed with URL, author and licence in
  `3rdparty/sounds/SOURCES.md`. The sources are Ogg Vorbis and the machine has no ffmpeg/sox, so a
  small dev-only tool, `sound_convert` (miniaudio + `stb_vorbis`, built with
  `--target sound_convert`, never shipped), converts them: mono, 22.05 kHz, trim, peak −1 dBFS,
  5 ms edge fades, or a crossfaded seamless loop with `--loop`.
- Controls: `M` toggles mute, View ▸ **Mute sound** (check item) does the same, `--mute` starts muted.
  No volume sliders.
- No audio device (CI, headless, broken driver): the game starts silently and prints one line to
  stderr. No dialog.
- Physics results must not change: impact collection only reads contact manifolds.

## Sound catalogue

| `SoundId` | File | Kind | Trigger |
|---|---|---|---|
| `Roll` | `roll.wav` | loop (slot `Roll`) | ball moving on the lane or in a gutter |
| `GutterDrop` | `gutter_drop.wav` | one-shot | ball crosses \|x\| > `kLane.halfWidth` while rolling |
| `PinHit` | `pin_hit.wav` | one-shot | ball↔pin impact |
| `PinClatter` | `pin_clatter.wav` | one-shot | pin↔pin, pin↔lane, pin↔gutter, pin↔pit impact |
| `ChargeTone` | `charge.wav` | loop (slot `Charge`) | phase `Charging` |
| `Release` | `release.wav` | one-shot | phase `Charging` → `Rolling` |
| `Strike` | `strike.wav` | one-shot | enter `FrameDone`, `HudText::throwResult` is `Strike` |
| `Spare` | `spare.wav` | one-shot | enter `FrameDone`, `HudText::throwResult` is `Spare` |
| `GutterJingle` | `miss.wav` | one-shot | enter `FrameDone`, `HudText::throwResult` is `Gutter` (0 pins) |
| `NewGame` | `new_game.wav` | one-shot | `SoundDirector::onNewGame()` |
| `ThemeSwitch` | `theme_switch.wav` | one-shot | `setTheme(id, announce = true)` |
| `AmbientNeon` | `ambient_neon.wav` | loop (slot `Ambient`) | theme Neon Hall |
| `AmbientClassic` | `ambient_classic.wav` | loop (slot `Ambient`) | theme Classic Alley |
| `AmbientOutdoor` | `ambient_outdoor.wav` | loop (slot `Ambient`) | theme Outdoor |

Asset rules: mono, 22.05 kHz, 16-bit WAV; one-shots 0.05–3 s; loops 0.5–12 s and seamless; total
size of `3rdparty/sounds/*.wav` < 3 MB. Peak-normalised to −1 dBFS; the code sets relative levels.

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
  `getLifeTime() <= 1` (Bullet refreshes a new point once in the sub-step that adds it) and
  `getAppliedImpulse() >= kImpactRecordThreshold` (0.3 N·s), records an `Impact` (pair order: the
  lower `Body` enum value first). The threshold sits above the resting impulse of a standing pin
  (1.5 kg · 9.81 m/s² · 1/120 s ≈ 0.12 N·s), so the first sub-step after a rack reset is silent.
  Contacts involving a pin are additionally ignored until the ball reaches `kPinImpactMinBallZ`
  (head pin z − 2 m), because the spawn bounces after a rack reset reach 0.38–1.76 N·s, above the
  0.3 N·s threshold.
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
inline constexpr std::array<SoundId, kSoundCount> kAllSounds { /* every SoundId, enum order */ };
enum class LoopSlot { Roll, Charge, Ambient };
inline constexpr int kLoopSlotCount = 3;
struct SoundInfo { int channels = 0; int sampleRate = 0; float seconds = 0.f; };

struct Voice {
    float volume = 1.f;  // [0, 1]
    float pitch = 1.f;   // playback rate, > 0
    float pan = 0.f;     // [-1 left, +1 right]
    bool operator==(const Voice&) const = default;
};

class IAudioOutput {
public:
    virtual ~IAudioOutput() = default;
    virtual void play(SoundId id, const Voice& voice) = 0;
    /// nullopt stops the slot; the same id updates the voice without restarting;
    /// a different id restarts the slot with that sound.
    virtual void setLoop(LoopSlot slot, std::optional<SoundId> id, const Voice& voice) = 0;
    virtual void setMuted(bool muted) = 0;
    virtual bool muted() const = 0;
};

Result<std::unique_ptr<IAudioOutput>> createDeviceOutput();  // miniaudio engine
std::unique_ptr<IAudioOutput> makeSilentOutput();            // never fails, plays nothing
std::string_view fileName(SoundId id);                       // e.g. "roll.wav"
std::span<const unsigned char> embeddedData(SoundId id);     // WAV bytes
bool isLoop(SoundId id);                                     // Roll, ChargeTone, Ambient*
Result<SoundInfo> decodeInfo(SoundId id);                    // headless decode, for tests
}
```

No default arguments on the virtual functions; callers always pass a `Voice`.

- Device output: one `ma_engine`; the 14 embedded WAVs are registered with the resource manager
  via `ma_resource_manager_register_encoded_data` under their file names and decoded on load.
  One-shots use a pool of at most 16 `ma_sound`s; when all are busy, the oldest one stops and is
  reused. Mute sets the engine volume to 0 (loops keep running, so unmuting resumes in place).
- Silent output: records nothing, `muted()` reflects `setMuted`.
- `cmake/EmbedFile.cmake` gains an optional `NAMESPACE` argument; the default stays
  `Bowlingbox::Render::embedded`, Audio uses `Bowlingbox::Audio::embedded`.
- Configure fails with a clear message if a `.wav` in `3rdparty/sounds/` is missing from
  `SOURCES.md` or a `SoundId` file is missing on disk.
- miniaudio 0.11.25 is compiled as a static C library (`miniaudio::miniaudio`) with
  `MA_NO_ENCODING`, `MA_NO_GENERATION` and `MA_NO_NULL` defined (without the null backend,
  `ma_engine_init` fails when there is no real device, which triggers the silent fallback); `MA_NO_ENGINE`, `MA_NO_NODE_GRAPH` and `MA_NO_RESOURCE_MANAGER` must
  stay undefined (the engine needs them). Linux links `Threads::Threads`, `m` and
  `${CMAKE_DL_LIBS}`.

## SoundDirector

Pure, window-free class in `bowlinggl_core`; owns no audio resources.

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

Rules (constants live in `SoundDirector.h`):

- **Roll loop** is active while phase is `Rolling` or `Settling`, the ball y > −0.1 (not in the pit)
  and ball speed > 0.2 m/s. Speed is the ball position change divided by `dt` (kept from the last
  tick when `dt == 0`). `t = clamp(speed / 12, 0, 1)`; volume `0.25 + 0.75·t`, pitch `0.8 + 0.4·t`,
  ×0.85 while \|x\| > `kLane.halfWidth`; pan `clamp(x / 0.765, −1, 1)`. Otherwise the slot is stopped.
- **Gutter drop** plays once per throw on the first tick with \|ball x\| > `kLane.halfWidth` during
  `Rolling`. Volume 0.7, pan from x.
- **Impacts**: ignore those below 0.3 N·s and any involving `Body::Other`; pair order does not
  matter. Ball↔pin → `PinHit`, volume `clamp(impulse / 4.0, 0.15, 1)`. Other pairs with at least
  one pin → `PinClatter`, volume `clamp(impulse / 1.5, 0.1, 0.8)`. Ball↔lane/gutter/pit → no sound.
  Per update at most 4 impact sounds, strongest first. Cooldown: each kind has a timer that grows
  by `dt` at the start of every update; a kind may play in this update only if its timer ≥ 30 ms,
  and the timer resets to 0 after an update in which it played (so several pins falling in one
  tick all sound, but the same kind cannot fire on two consecutive 16 ms ticks). Pan from impact x.
  Clatter pitch cycles deterministically through {1.0, 0.96, 1.04, 0.98, 1.02}.
- **Charge loop** while phase is `Charging`: volume 0.5, pitch `0.8 + 0.6·power`. Stopped otherwise.
- **Release** on the `Charging` → `Rolling` transition, volume 0.8, pan from ball x.
- **Result stinger** on entering `FrameDone`, from `HudText::throwResult(snapshot, sheet)` (a new
  enum `ThrowResult { None, Strike, Spare, Gutter, Pins }` that `HudText::result` now also uses,
  so a ten after a gutter ball is a spare in both places): `Strike` → `Strike`, `Spare` → `Spare`,
  `Gutter` → `GutterJingle`. Volume 0.8.
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
- `OnTimer`: after stepping the lane,
  `_sound.update(snapshot, _lane.scoreSheet(), _lane.drainImpacts(), steps·kDt)`.
- `newGame()` / the `NewGame` command call `_sound.onNewGame()`; `setTheme` calls
  `_sound.setTheme(id, announce)` with `announce = false` for the initial theme.
- `M` is guarded against key auto-repeat like `T`. `setMuted(bool)` on the canvas updates the output
  and calls `onMuteChanged(bool)`, which `BowlingFrame` uses to keep View ▸ Mute sound checked.
- `--mute` (`wxCmdLineParser::AddSwitch`) is passed to `BowlingFrame`, which mutes the canvas.
- `HudText::controlsHelp()` gains `M: mute sound`; the status line is unchanged.

## Licensing and docs

- `licenses/miniaudio.txt` (its public domain / MIT-0 text), `licenses/sounds.txt` (CC0 1.0 text
  plus the per-file attribution list from `SOURCES.md`), `stb_vorbis` added to the stb row of
  `licenses/README.md` as tool-only, `licenses/README.md`,
  `3rdparty/README.md` and `installer/THIRD_PARTY_NOTICES.txt` updated.
- `applications/bowlingGame/README.md`: the `M` key, `--mute`, and three manual checklist items:
  sounds play during a strike throw; `M` mutes and unmutes; `--mute` starts silent.

## Testing

- **Alley:** `FakeWorld` returns queued impacts; `Lane::drainImpacts()` forwards them and empties.
  Bullet integration test: a pocket throw yields at least one ball↔pin impact and
  `drainImpacts()` is empty right after; the first second of a slow throw (ball still far from
  the rack) yields no impact involving a pin; `rebuild()` clears pending impacts. Existing physics tests
  stay green (determinism).
- **Audio:** every `SoundId` has a unique `.wav` file name and embedded data; each decodes with
  `decodeInfo` (no device) to mono 22.05 kHz within the duration limits; the total stays < 3 MB;
  silent output honours `setMuted`. No test opens a device.
- **SoundDirector:** a recording fake `IAudioOutput` checks each catalogue trigger, the roll voice
  formulae, the impulse thresholds and the 4-per-update cap, the cooldown, the first-update rule,
  theme announce on/off, and that `onNewGame` stops loops.
- **HudText:** `throwResult` returns `Spare` for a ten after a gutter ball and `None` outside
  `FrameDone`.
- **InputMapper:** `M` → `ToggleMute` in every phase.

## Out of scope

Volume sliders, settings persistence, 3D positional audio, sound in the console or score-sheet
front-ends, music.
