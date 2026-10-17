#include <Bowlingbox/Audio/Audio.h>
#include <miniaudio.h>
#include <algorithm>
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
        applyMaster();
    }
    bool muted() const override { return _muted; }
    void setVolume(float volume) override {
        _volume = std::clamp(volume, 0.f, 1.f);
        applyMaster();
    }
    float volume() const override { return _volume; }

private:
    struct OneShot {
        ma_sound sound {};
        std::uint64_t serial = 0;  // 0: not initialised
    };
    struct Loop {
        ma_sound sound {};
        std::optional<SoundId> id;
    };

    void applyMaster() {
        ma_engine_set_volume(&_engine, _muted ? 0.f : _volume);  // loops keep running underneath
    }

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
    float _volume = 1.f;
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
