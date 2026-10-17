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

std::optional<SoundId> stingerFor(HudText::ThrowResult r) {
    switch (r) {
        case HudText::ThrowResult::Strike: return SoundId::Strike;
        case HudText::ThrowResult::Spare:  return SoundId::Spare;
        case HudText::ThrowResult::Gutter:  // the gutter drop already says it
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

void SoundDirector::setAmbient(SoundId ambient, bool announce) {
    _ambient = ambient;
    updateAmbience();
    if (announce)
        _out.play(SoundId::ThemeSwitch, {kChimeVolume, 1.f, 0.f});
}

void SoundDirector::setAmbienceEnabled(bool enabled) {
    _ambience = enabled;
    updateAmbience();
}

void SoundDirector::updateAmbience() {
    if (_ambience && _ambient)
        _out.setLoop(LoopSlot::Ambient, *_ambient, {kAmbientVolume, 1.f, 0.f});
    else
        _out.setLoop(LoopSlot::Ambient, std::nullopt, {});
}

void SoundDirector::playTransitions(const Snapshot& prev, const Snapshot& s,
                                    const Bowlingbox::Score::IScoreSheet& sheet) {
    // Not just Charging -> Rolling: charge and release can both land inside one timer tick.
    if (prev.phase != Phase::Rolling && prev.phase != Phase::Settling && s.phase == Phase::Rolling) {
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
    _out.setLoop(LoopSlot::Roll, SoundId::Roll, {kRollVolume * (0.25f + 0.75f * t), pitch, pan(p[0])});
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
