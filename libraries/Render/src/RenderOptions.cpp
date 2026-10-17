#include <Bowlingbox/Render/RenderOptions.h>
#include <algorithm>
#include <cctype>
#include <string_view>

namespace Bowlingbox {
namespace Render {

FrameSizes frameSizes(int width, int bandHeight) {
    const auto half = [](glm::ivec2 s) { return glm::max(s / 2, glm::ivec2(1)); };
    FrameSizes out;
    out.scene = glm::max(glm::ivec2(width, bandHeight), glm::ivec2(1));
    out.reflection = half(out.scene);
    out.bloom[0] = half(out.scene);
    for (std::size_t i = 1; i < out.bloom.size(); ++i)
        out.bloom[i] = half(out.bloom[i - 1]);
    return out;
}

RenderOptions renderOptions(const char* render, const char* stats) {
    RenderOptions out;
    if (render) {
        const std::string_view v(render), basic("basic");
        out.forceBasic = v.size() == basic.size() &&
                         std::equal(v.begin(), v.end(), basic.begin(), [](char a, char b) {
                             return std::tolower(static_cast<unsigned char>(a)) == b;
                         });
    }
    out.stats = stats && std::string_view(stats) == "1";
    return out;
}

}  // namespace Render
}  // namespace Bowlingbox
