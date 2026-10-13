#include <Gamebox/Render/Geometry.h>
#include <cmath>

namespace Gamebox {
namespace Render {
namespace {
constexpr float kPi = 3.14159265358979f;

void push(MeshData& m, float x, float y, float z, float nx, float ny, float nz) {
    m.vertices.insert(m.vertices.end(), {x, y, z, nx, ny, nz});
}
void tri(MeshData& m, unsigned a, unsigned b, unsigned c) {
    m.indices.insert(m.indices.end(), {a, b, c});
}
}  // namespace

MeshData boxData(float hx, float hy, float hz) {
    MeshData m;
    struct Face { float n[3]; float u[3]; float v[3]; };
    const Face faces[6] = {
        {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}},  {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
        {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}},  {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}},
        {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},   {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}},
    };
    const float h[3] = {hx, hy, hz};
    for (const Face& f : faces) {
        const unsigned base = static_cast<unsigned>(m.vertexCount());
        for (int corner = 0; corner < 4; ++corner) {
            const float su = (corner == 1 || corner == 2) ? 1.f : -1.f;
            const float sv = (corner >= 2) ? 1.f : -1.f;
            float p[3];
            for (int k = 0; k < 3; ++k)
                p[k] = (f.n[k] + su * f.u[k] + sv * f.v[k]) * h[k];
            push(m, p[0], p[1], p[2], f.n[0], f.n[1], f.n[2]);
        }
        tri(m, base, base + 1, base + 2);
        tri(m, base, base + 2, base + 3);
    }
    return m;
}

MeshData latheData(const std::vector<std::pair<float, float>>& profile, int segments) {
    MeshData m;
    const int rings = static_cast<int>(profile.size());
    if (rings < 2 || segments < 3)
        return m;
    for (int i = 0; i < rings; ++i) {
        const int prev = i > 0 ? i - 1 : i;
        const int next = i < rings - 1 ? i + 1 : i;
        const float dr = profile[next].first - profile[prev].first;
        const float dy = profile[next].second - profile[prev].second;
        float nr = dy, ny = -dr;
        const float len = std::sqrt(nr * nr + ny * ny);
        if (len > 1e-6f) { nr /= len; ny /= len; } else { nr = 0.f; ny = 1.f; }
        if (profile[i].first <= 1e-6f) { nr = 0.f; ny = (i == 0) ? -1.f : 1.f; }
        for (int s = 0; s <= segments; ++s) {
            const float a = 2.f * kPi * static_cast<float>(s) / static_cast<float>(segments);
            const float c = std::cos(a), sn = std::sin(a);
            push(m, profile[i].first * c, profile[i].second, profile[i].first * sn,
                 nr * c, ny, nr * sn);
        }
    }
    const unsigned stride = static_cast<unsigned>(segments + 1);
    for (int i = 0; i < rings - 1; ++i)
        for (int s = 0; s < segments; ++s) {
            const unsigned a = static_cast<unsigned>(i) * stride + static_cast<unsigned>(s);
            const unsigned b = a + 1, c = a + stride, d = c + 1;
            tri(m, a, c, b);
            tri(m, b, c, d);
        }
    return m;
}

MeshData sphereData(float radius, int segments) {
    std::vector<std::pair<float, float>> profile;
    const int stacks = segments / 2 < 2 ? 2 : segments / 2;
    for (int i = 0; i <= stacks; ++i) {
        const float t = kPi * static_cast<float>(i) / static_cast<float>(stacks);
        profile.emplace_back(radius * std::sin(t), -radius * std::cos(t));
    }
    return latheData(profile, segments);
}

MeshData cylinderData(float radius, float height, int segments) {
    const float h = height / 2;
    return latheData({{0.f, -h}, {radius, -h}, {radius, -h}, {radius, h}, {radius, h}, {0.f, h}},
                     segments);
}

MeshData pinData(float height, float maxRadius, float comHeight, int segments) {
    const std::pair<float, float> shape[] = {
        {0.0f, 0.00f}, {0.45f, 0.00f}, {0.70f, 0.05f}, {0.95f, 0.20f}, {1.00f, 0.30f},
        {0.90f, 0.45f}, {0.60f, 0.60f}, {0.45f, 0.70f}, {0.50f, 0.80f}, {0.55f, 0.90f},
        {0.35f, 0.98f}, {0.0f, 1.00f},
    };
    std::vector<std::pair<float, float>> profile;
    for (const auto& [r, y] : shape)
        profile.emplace_back(r * maxRadius, y * height - comHeight);
    return latheData(profile, segments);
}

}  // namespace Render
}  // namespace Gamebox
