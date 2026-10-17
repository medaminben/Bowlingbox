#ifndef BOWLINGBOX_RENDER_GPUTIMER_H
#define BOWLINGBOX_RENDER_GPUTIMER_H
#include <array>
#include <chrono>
#include <cstddef>

namespace Bowlingbox {
namespace Render {

/// GL_TIME_ELAPSED per pass, BOWLINGBOX_RENDER_STATS=1. The queries rotate through kFrames sets.
/// A result is read only when GL_QUERY_RESULT_AVAILABLE says it's ready, so the CPU never waits
/// on the GPU (drivers often queue 2-3 frames). The averages go to stderr about once a second.
class GpuTimer {
public:
    enum Pass { Shadow, Reflection, Main, Post, kPassCount };
    static constexpr std::size_t kFrames = 3;
    GpuTimer();
    ~GpuTimer();
    GpuTimer(const GpuTimer&) = delete;
    GpuTimer& operator=(const GpuTimer&) = delete;

    void begin(Pass pass);
    void end();
    void endFrame();

private:
    void collect(std::size_t set);  // adds every finished query of `set` to the sums

    std::array<std::array<unsigned, kPassCount>, kFrames> _queries {};
    std::array<std::array<bool, kPassCount>, kFrames> _issued {};  // begun, result not read yet
    std::array<double, kPassCount> _sumMs {};
    std::array<int, kPassCount> _samples {};  // results summed per pass since the last print
    std::size_t _write = 0;
    bool _open = false;  // begin() started a query that end() must close
    int _frames = 0;
    std::chrono::steady_clock::time_point _last = std::chrono::steady_clock::now();
};

}  // namespace Render
}  // namespace Bowlingbox
#endif  // BOWLINGBOX_RENDER_GPUTIMER_H
