#ifndef BOWLINGGL_SOUND_DIRECTOR_H
#define BOWLINGGL_SOUND_DIRECTOR_H
#include <Bowlingbox/Alley/Types.h>
#include <Bowlingbox/Audio/Audio.h>
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
    // Mix: the ambience files are normalised to -20 dBFS RMS, the rest to a -1 dBFS peak.
    // The pin crash is the loudest moment; the ambience sits about 15 dB under the roll.
    static constexpr float kAmbientVolume = 0.18f;
    static constexpr float kRollVolume = 0.35f;     // at full speed
    static constexpr float kChargeVolume = 0.3f;
    static constexpr float kReleaseVolume = 0.8f;
    static constexpr float kGutterVolume = 0.7f;
    static constexpr float kStingerVolume = 0.6f;
    static constexpr float kChimeVolume = 0.4f;

    explicit SoundDirector(Bowlingbox::Audio::IAudioOutput& out);

    /// Call once per timer tick after stepping the lane; dt is the simulated time of the tick.
    void update(const Bowlingbox::Alley::Snapshot& s, const Bowlingbox::Score::IScoreSheet& sheet,
                std::span<const Bowlingbox::Alley::Impact> impacts, float dt);
    void onNewGame();
    /// Switches the ambience; announce plays the theme-switch chime.
    /// Plays `ambient` as the background loop (if ambience is on); `announce` adds the theme chime.
    void setAmbient(Bowlingbox::Audio::SoundId ambient, bool announce);
    /// Starts or stops the theme's background loop; the other sounds are unaffected.
    void setAmbienceEnabled(bool enabled);
    bool ambienceEnabled() const { return _ambience; }

private:
    void playTransitions(const Bowlingbox::Alley::Snapshot& prev, const Bowlingbox::Alley::Snapshot& s,
                         const Bowlingbox::Score::IScoreSheet& sheet);
    void updateCharge(const Bowlingbox::Alley::Snapshot& s);
    void updateRoll(const Bowlingbox::Alley::Snapshot& s);
    void playImpacts(std::span<const Bowlingbox::Alley::Impact> impacts);
    void updateAmbience();

    Bowlingbox::Audio::IAudioOutput& _out;
    std::optional<Bowlingbox::Alley::Snapshot> _prev;
    std::optional<Bowlingbox::Audio::SoundId> _ambient;  // unset until the first setAmbient
    bool _ambience = true;
    float _ballSpeed = 0.f;
    bool _gutterPlayed = false;
    float _sinceHit = kImpactCooldown;
    float _sinceClatter = kImpactCooldown;
    int _clatterIndex = 0;
};
#endif  // BOWLINGGL_SOUND_DIRECTOR_H
