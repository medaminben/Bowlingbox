#include "Shaders.h"

namespace Bowlingbox::Render::shaders {

const char* const kToneMapGlsl = R"(
vec3 acesFit(vec3 x) {  // Narkowicz 2015
    return clamp(x * (2.51 * x + 0.03) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}
vec3 linearToSrgb(vec3 c) {
    return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), c));
}
vec3 toDisplay(vec3 hdr, float exposure) { return linearToSrgb(acesFit(hdr * exposure)); }
)";

const char* const kLitVs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uModel; uniform mat4 uView; uniform mat4 uProj; uniform mat3 uNormalMat;
uniform mat4 uLightVP;
uniform vec4 uClipPlane;  // world space; (0, 0, 0, 1) keeps everything
out vec3 vNormal; out vec3 vWorldPos; out vec3 vLocalPos; out vec3 vViewPos; out vec4 vLightPos;
void main() {
    vec4 world = uModel * vec4(aPos, 1.0);
    vWorldPos = world.xyz;
    vLocalPos = aPos;
    vNormal = normalize(uNormalMat * aNormal);
    vec4 viewPos = uView * world;
    vViewPos = viewPos.xyz;
    vLightPos = uLightVP * world;
    gl_ClipDistance[0] = dot(world, uClipPlane);
    gl_Position = uProj * viewPos;
})";

const char* const kFlatVs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() { gl_Position = uMVP * vec4(aPos.xy, 0.0, 1.0); })";

const char* const kFlatFs = R"(#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() { FragColor = uColor; })";

// One triangle that covers the viewport; vT runs 0 (bottom) to 1 (top).
const char* const kSkyVs = R"(#version 330 core
out float vT;
void main() {
    vec2 p = vec2(gl_VertexID == 1 ? 3.0 : -1.0, gl_VertexID == 2 ? 3.0 : -1.0);
    vT = (p.y + 1.0) * 0.5;
    gl_Position = vec4(p, 0.0, 1.0);
})";

namespace {
const char* const kLitBody = R"(
in vec3 vNormal; in vec3 vWorldPos; in vec3 vLocalPos; in vec3 vViewPos; in vec4 vLightPos;

uniform vec3 uColor;  // linear
uniform vec3 uBandColor; uniform vec2 uBandRange;  // pin neck stripe, object-space y
uniform bool uEmissive; uniform float uEmissiveStrength;
uniform float uRoughness; uniform float uSpecular;
uniform int uPattern;  // 0 none, 1 wood, 2 ball swirl
uniform float uAlpha;

uniform vec3 uCameraPos;
uniform vec3 uLightDir; uniform vec3 uKeyColor;  // keyColor * keyIntensity
uniform vec3 uSkyAmbient; uniform vec3 uGroundAmbient;
uniform int uPointCount;
uniform vec3 uPointPos[4]; uniform vec3 uPointColor[4]; uniform float uPointRange[4];

uniform bool uFog; uniform vec3 uFogColor; uniform float uFogStart; uniform float uFogEnd;

uniform bool uShadows; uniform sampler2DShadow uShadowMap;  // texture unit 1
uniform bool uReflect; uniform sampler2D uReflection;       // texture unit 2
uniform vec2 uViewportSize;
uniform float uLaneHalfWidth; uniform float uDeckZ;
uniform float uBoard;  // board width: 2 * laneHalfWidth / 39

uniform bool uToneMap; uniform float uExposure;

out vec4 FragColor;

const float PI = 3.14159265;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float hash3(vec3 p) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p), u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), u.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), u.x), u.y);
}
float valueNoise3(vec3 p) {
    vec3 i = floor(p), f = fract(p), u = f * f * (3.0 - 2.0 * f);
    float a = mix(hash3(i), hash3(i + vec3(1, 0, 0)), u.x);
    float b = mix(hash3(i + vec3(0, 1, 0)), hash3(i + vec3(1, 1, 0)), u.x);
    float c = mix(hash3(i + vec3(0, 0, 1)), hash3(i + vec3(1, 0, 1)), u.x);
    float d = mix(hash3(i + vec3(0, 1, 1)), hash3(i + vec3(1, 1, 1)), u.x);
    return mix(mix(a, b, u.y), mix(c, d, u.y), u.z);
}
float fbm3(vec3 p) {
    float sum = 0.0, amp = 0.5;
    for (int i = 0; i < 4; ++i) { sum += amp * valueNoise3(p); p *= 2.03; amp *= 0.5; }
    return sum;
}

// Dark-red inlay on the main lane: 7 arrows about 4.6 m down and 7 dots on the approach,
// on boards 5, 10, ... 35 of 39. Board 20 is the middle board, so the pattern is symmetric
// and it doesn't matter which edge counts as board 1.
float marking(vec3 p) {
    if (abs(p.x) > uLaneHalfWidth) return 0.0;
    float fromEdge = (p.x + uLaneHalfWidth) / uBoard;
    float board = floor(fromEdge) + 1.0;                        // 1 .. 39
    if (board < 5.0 || board > 35.0 || mod(board, 5.0) != 0.0) return 0.0;
    float u = fract(fromEdge) - 0.5;                            // -0.5 .. 0.5 across the board
    float tip = 4.3 + 0.6 * (1.0 - abs(board - 20.0) / 15.0);   // the middle arrow is furthest
    float along = (tip - p.z) / 0.15;                           // 0 at the tip, 1 at the base
    float arrow = (along >= 0.0 && along <= 1.0 && abs(u) <= 0.5 * along) ? 1.0 : 0.0;
    float spot = length(vec2(u * uBoard, p.z + 0.15)) < 0.008 ? 1.0 : 0.0;
    return max(arrow, spot);
}

// Boards are counted from the main lane's edge, so its 39 boards line up with both edges.
// Neighbour lanes reuse the same grid; it's off their edges by under a millimetre.
vec3 wood(vec3 base, vec3 p) {
    float fromEdge = (p.x + uLaneHalfWidth) / uBoard;
    float board = floor(fromEdge);
    float tint = 1.0 + 0.16 * (hash(vec2(board, 7.0)) - 0.5);
    float phase = hash(vec2(board, 13.0)) * 50.0;
    float fine = clamp(1.0 - fwidth(p.x) * 750.0, 0.0, 1.0);  // fade grain finer than a pixel
    float grain = 0.6 * noise(vec2(p.x * 400.0, p.z * 3.0 + phase))
                + 0.4 * fine * noise(vec2(p.x * 1500.0, p.z * 9.0 + phase));
    float streak = smoothstep(0.75, 0.95, noise(vec2(board * 3.1, p.z * 0.4 + phase)));
    vec3 c = base * tint * (0.92 + 0.12 * grain) * (1.0 - 0.18 * streak);
    float seam = smoothstep(0.92, 1.0, abs(fract(fromEdge) - 0.5) * 2.0);
    c *= 1.0 - 0.35 * seam * clamp(1.0 - fwidth(p.x) / uBoard, 0.0, 1.0);
    if (p.z < 0.0) c *= vec3(1.04, 1.02, 0.97);           // approach
    else if (p.z > uDeckZ) c *= vec3(0.96, 0.94, 0.92);   // pin deck
    return mix(c, vec3(0.30, 0.02, 0.02), marking(p));
}

// Two-tone marbling in object space, so it rolls with the ball, plus three finger holes.
vec3 swirl(vec3 base, vec3 p) {
    vec3 d = normalize(p);
    float warp = fbm3(d * 2.5 + vec3(3.1));
    float m = fbm3(d * 3.0 + 2.0 * vec3(warp));
    vec3 c = mix(base, base * 0.35, smoothstep(0.45, 0.55, m));
    vec3 holes[3] = vec3[3](normalize(vec3(0.0, -0.15, 1.0)), normalize(vec3(-0.06, 0.12, 1.0)),
                            normalize(vec3(0.06, 0.12, 1.0)));
    for (int i = 0; i < 3; ++i) {
        float a = acos(clamp(dot(d, holes[i]), -1.0, 1.0));  // radians from the hole centre
        float r = i == 0 ? 0.11 : 0.09;
        c *= mix(0.08, 1.0, smoothstep(r - 0.02, r + 0.01, a));
    }
    return c;
}

float distributionGgx(float nDotH, float a) {
    float a2 = a * a;
    float d = nDotH * nDotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}
float visibilitySmith(float nDotV, float nDotL, float a) {  // includes 1 / (4 nDotV nDotL)
    float k = a * 0.5;
    return 0.25 / ((nDotV * (1.0 - k) + k) * (nDotL * (1.0 - k) + k));
}
vec3 brdf(vec3 n, vec3 v, vec3 l, vec3 albedo, vec3 radiance) {
    float nDotL = max(dot(n, l), 0.0);
    if (nDotL <= 0.0) return vec3(0.0);
    vec3 h = normalize(v + l);
    float nDotV = max(dot(n, v), 1e-4), nDotH = max(dot(n, h), 0.0), vDotH = max(dot(v, h), 0.0);
    float a = max(uRoughness * uRoughness, 0.002);
    vec3 f = vec3(uSpecular) + (1.0 - vec3(uSpecular)) * pow(1.0 - vDotH, 5.0);
    vec3 spec = distributionGgx(nDotH, a) * visibilitySmith(nDotV, nDotL, a) * f;
    vec3 diff = (1.0 - f) * albedo / PI;
    return (diff + spec) * radiance * nDotL;
}

float keyLightVisible(vec3 n) {
    if (!uShadows) return 1.0;
    vec3 p = vLightPos.xyz / vLightPos.w * 0.5 + 0.5;
    if (any(lessThan(p, vec3(0.0))) || any(greaterThan(p, vec3(1.0)))) return 1.0;  // outside: lit
    float bias = max(0.0025 * (1.0 - max(dot(n, -normalize(uLightDir)), 0.0)), 0.0005);
    vec2 texel = 1.0 / vec2(textureSize(uShadowMap, 0));
    float lit = 0.0;
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            lit += texture(uShadowMap, vec3(p.xy + vec2(x, y) * texel, p.z - bias));
    return lit / 9.0;
}

void main() {
    vec3 base = (vLocalPos.y >= uBandRange.x && vLocalPos.y <= uBandRange.y) ? uBandColor : uColor;
    if (uPattern == 1) base = wood(base, vWorldPos);
    else if (uPattern == 2) base = swirl(base, vLocalPos);

    vec3 rgb;
    if (uEmissive) {
        rgb = base * uEmissiveStrength;
    } else {
        vec3 n = normalize(vNormal);
        vec3 v = normalize(uCameraPos - vWorldPos);
        rgb = mix(uGroundAmbient, uSkyAmbient, n.y * 0.5 + 0.5) * base;
        rgb += brdf(n, v, -normalize(uLightDir), base, uKeyColor) * keyLightVisible(n);
        for (int i = 0; i < uPointCount; ++i) {
            vec3 toLight = uPointPos[i] - vWorldPos;
            float d = length(toLight);
            float window = clamp(1.0 - pow(d / uPointRange[i], 4.0), 0.0, 1.0);
            rgb += brdf(n, v, toLight / d, base, uPointColor[i] * window * window / (d * d + 1.0));
        }
        if (uReflect) {
            vec2 uv = gl_FragCoord.xy / uViewportSize;
            float o = uRoughness * 0.004;
            vec4 r = 0.25 * (texture(uReflection, uv + vec2(o, 0.0)) + texture(uReflection, uv - vec2(o, 0.0))
                           + texture(uReflection, uv + vec2(0.0, o)) + texture(uReflection, uv - vec2(0.0, o)));
            float fresnel = uSpecular + (1.0 - uSpecular) * pow(1.0 - max(dot(n, v), 0.0), 5.0);
            float k = clamp(fresnel * (1.0 - uRoughness) * 0.6, 0.0, 1.0);
            // r is premultiplied (cleared to transparent black): a plain mix would darken soft edges.
            rgb = rgb * (1.0 - k * r.a) + k * r.rgb;
        }
    }
    if (uFog) {
        float f = clamp((length(vViewPos) - uFogStart) / (uFogEnd - uFogStart), 0.0, 1.0);
        rgb = mix(rgb, uFogColor, uEmissive ? 0.5 * f : f);
    }
    FragColor = vec4(uToneMap ? toDisplay(rgb, uExposure) : rgb, uAlpha);
}
)";

const char* const kSkyBody = R"(
in float vT;
uniform vec3 uTop; uniform vec3 uBottom;  // linear
uniform bool uToneMap; uniform float uExposure;
out vec4 FragColor;
void main() {
    vec3 c = mix(uBottom, uTop, clamp(vT, 0.0, 1.0));
    FragColor = vec4(uToneMap ? toDisplay(c, uExposure) : c, 1.0);
}
)";
}  // namespace

std::string litFs() {
    return std::string("#version 330 core\n") + kToneMapGlsl + kLitBody;
}
std::string skyFs() {
    return std::string("#version 330 core\n") + kToneMapGlsl + kSkyBody;
}

}  // namespace Bowlingbox::Render::shaders
