#include "GpuTimer.h"
#include <glad/gl.h>
#include <cstdio>

namespace Bowlingbox {
namespace Render {

GpuTimer::GpuTimer() {
    for (auto& set : _queries)
        glGenQueries(kPassCount, set.data());
}
GpuTimer::~GpuTimer() {
    for (auto& set : _queries)
        glDeleteQueries(kPassCount, set.data());
}

void GpuTimer::begin(Pass pass) {
    const auto p = static_cast<std::size_t>(pass);
    if (_issued[_write][p])  // the GPU is kFrames behind: drop this measurement rather than wait
        return;
    glBeginQuery(GL_TIME_ELAPSED, _queries[_write][p]);
    _issued[_write][p] = true;
    _open = true;
}
void GpuTimer::end() {
    if (_open)
        glEndQuery(GL_TIME_ELAPSED);
    _open = false;
}

void GpuTimer::collect(std::size_t set) {
    for (std::size_t p = 0; p < kPassCount; ++p) {
        if (!_issued[set][p])
            continue;
        GLint ready = GL_FALSE;
        glGetQueryObjectiv(_queries[set][p], GL_QUERY_RESULT_AVAILABLE, &ready);
        if (!ready)
            continue;
        GLuint64 ns = 0;
        glGetQueryObjectui64v(_queries[set][p], GL_QUERY_RESULT, &ns);
        _sumMs[p] += static_cast<double>(ns) / 1e6;
        ++_samples[p];
        _issued[set][p] = false;
    }
}

void GpuTimer::endFrame() {
    for (std::size_t set = 0; set < kFrames; ++set)
        if (set != _write)
            collect(set);
    _write = (_write + 1) % kFrames;
    ++_frames;
    const auto now = std::chrono::steady_clock::now();
    if (now - _last >= std::chrono::seconds(1)) {
        const auto avg = [this](Pass p) { return _samples[p] > 0 ? _sumMs[p] / _samples[p] : 0.0; };
        std::fprintf(stderr, "render: shadow %.2f ms, reflection %.2f ms, main %.2f ms, post %.2f ms (GPU, %d frames)\n",
                     avg(Shadow), avg(Reflection), avg(Main), avg(Post), _frames);
        _sumMs = {};
        _samples = {};
        _frames = 0;
        _last = now;
    }
}

}  // namespace Render
}  // namespace Bowlingbox
