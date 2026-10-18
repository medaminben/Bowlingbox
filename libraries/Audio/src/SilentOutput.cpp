#include <Bowlingbox/Audio/Audio.h>
#include <algorithm>

namespace Bowlingbox::Audio {
namespace {
class SilentOutput final : public IAudioOutput {
public:
    void play(SoundId, const Voice&) override {}
    void setLoop(LoopSlot, std::optional<SoundId>, const Voice&) override {}
    void setMuted(bool muted) override { _muted = muted; }
    bool muted() const override { return _muted; }
    void setVolume(float volume) override { _volume = std::clamp(volume, 0.f, 1.f); }
    float volume() const override { return _volume; }

private:
    bool _muted = false;
    float _volume = 1.f;
};
}  // namespace

std::unique_ptr<IAudioOutput> makeSilentOutput() {
    return std::make_unique<SilentOutput>();
}

}  // namespace Bowlingbox::Audio
