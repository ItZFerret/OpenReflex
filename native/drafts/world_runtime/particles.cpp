// Particles of the original world (PlatformParticleManager2D, XenonParticleManager2D.cpp): the live particles of the
// two managers after each frame, in the order and form the original renderer draws them. Read only; the simulation is
// the original's own (the manager update 00698510 is simply not skipped, World::set_particles).
//
// Recovered (MXReflex.exe 1.1.0.11; names in _re/names/74_particles.tsv):
//  - Two static managers, set up by ParticleEntity::vf5 (00695de0) with ParticleManager2D_Init (00697ce0, EAX = type):
//    0xc11bd0 type 2 "normal" (SRCALPHA / INVSRCALPHA), 0xc11d80 type 1 "additive" (SRCALPHA / ONE). ParticleEntity::vf21
//    (006960c0) updates both (00698510, thiscall(dt)); emitters spawn through PlatformParticleManager2D::vf1 (00698780).
//  - Manager: +0x14 particle array (0x80 bytes each), +0x18 render copy, +0x1c capacity - 2, +0x70 ParticleAsset handle
//    (+0x44 data: +0x10 attribute table 0x1f0 per emitter attribute, +0x18 frame table 8 bytes per frame, +0x1c frame count,
//    +0x60 ShadowLuminance float4, +0x70 ParticleLuminance float4), +0x8c atlas TextureAsset handle (name inline at +4),
//    +0xa8 type, +0xac live count (after the update job's compaction), +0xb0 count incl. this frame's spawns.
//  - Particle (Particle_UpdateJob 006925f0): +0x00 position xyzw, +0x10 velocity, +0x20 acceleration, +0x30/+0x34 size
//    (quad half extents), +0x38 rotation (radians), +0x3c rotation rate, +0x40 scale (scale curve x ScaleRatio), +0x44 birth
//    time, +0x48 life, +0x4c colour D3DCOLOR (alpha/colour curves), +0x50 second colour (D3DCOLOR, multiplied), +0x54 frame
//    (float, animated), +0x58 frame rate, +0x5c emitter (-> controller: +0x10 attribute index, +0x14 alpha scale),
//    +0x70/+0x74 quad pivot. +0x60..+0x6c and +0x78 are the instance builder's scratch (bucket, next) in the render copy.
//  - Render (PlatformParticleManager2D::vf0_oc 00698890): one instanced DrawIndexedPrimitive per manager, stream 0 = 4 corner
//    ids, stream 1 = 0x34-byte instances built by ParticleManager2D_BuildInstances (00699a50 / 00699e70): POSITION float3
//    (+0x00), TEXCOORD0 FLOAT16_4 frame (scale u, v, offset u, v), TEXCOORD1 float4 (size x, y, pivot x, y), TEXCOORD2 float2
//    (scale, rotation), COLOR0, COLOR1. The instances are sorted far to near into 10000 view-depth buckets
//    (bucket = (z_view - near) / (far - near) * 10000 + 1, buckets 1..10000 drawn, last spawned first within a bucket), at
//    most 3000 per manager (vf7 0x691ba0 returns 0). Shader PyroParticles (EngineDependencies): UncompressedNoLightVS +
//    NoLightPS (soft) / NoLightNoSoftParticlesPS; see Unity OriginalParticles.cs / ReflexParticles.shader for the math.
#include "runtime.h"
#include "vm.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>

namespace reflex_vm {

namespace {
constexpr uint32_t MANAGERS[2] = {0xc11bd0, 0xc11d80};   // normal (type 2), additive (type 1)
constexpr uint32_t RENDER_CAMERA = 0xa7b1d8;             // [ ] -> render camera: +0x10 pos, +0x40 forward, +0x58 near, +0x5c far
constexpr uint32_t SOFT_PARTICLES = 0xc11d04;            // soft particles enabled (NoLightPS with the depth fade)
constexpr uint32_t FADE_STATE = 0xc11d10;                // per view: area threshold, fade width (adaptive, 16 bytes per view)
constexpr uint32_t ALPHA_KILL = 0x9c475c;                // 4/255
constexpr uint32_t DRAW_CAP = 3000;                      // (vf7() ? 7000 : 0) + 3000, vf7 = 0x691ba0 returns 0
constexpr int BUCKETS = 10000;

float as_float(uint32_t v) { float f; std::memcpy(&f, &v, 4); return f; }
float half_to_float(uint16_t h) {
    uint32_t s = uint32_t(h >> 15) << 31, e = (h >> 10) & 31, m = h & 1023, bits;
    if (e == 0) { if (!m) bits = s; else { e = 113; while (!(m & 1024)) { m <<= 1; e--; } bits = s | (e << 23) | ((m & 1023) << 13); } }
    else if (e == 31) bits = s | 0x7f800000 | (m << 13);
    else bits = s | ((e + 112) << 23) | (m << 13);
    return as_float(bits);
}
}  // namespace

void World::particle_export(std::vector<Particle>& out, ParticleInfo& info) {
    Machine& m = machine();
    out.clear();
    std::memset(&info, 0, sizeof info);
    auto u32 = [&](uint32_t a) { return m.mem.mapped(a) ? uint32_t(m.mem.read(a, 4)) : 0u; };
    auto f32 = [&](uint32_t a) { return as_float(u32(a)); };
    // the render camera (the instance builder's view: depth along the view direction, near / far of the same camera)
    uint32_t cam = u32(RENDER_CAMERA);
    float pos[3] = {}, fwd[3] = {0, 0, 1}, near_ = 0.1f, far_ = 1000.0f;
    if (cam) { for (int i = 0; i < 3; i++) { pos[i] = f32(cam + 0x10 + 4 * i); fwd[i] = f32(cam + 0x40 + 4 * i); } near_ = f32(cam + 0x58); far_ = f32(cam + 0x5c); }
    if (!(far_ > near_) || !std::isfinite(far_ - near_)) { near_ = 0.1f; far_ = 1000.0f; }
    float vw = -(pos[0] * fwd[0] + pos[1] * fwd[1] + pos[2] * fwd[2]);
    float bucket_scale = 1.0f / (far_ - near_) * float(BUCKETS);
    info.near_plane = near_; info.far_plane = far_;
    info.soft = u32(SOFT_PARTICLES) != 0;
    // g_ScreenAreaToFadeAndAlphaKill (c100) as 00698890 sets it for the main view (0): area fade between a - b and a, then
    // alpha kill. a adapts to the measured particle GPU time (target RendererRegistry TargetRenderTimeMicroSeconds 3000):
    // x (2 - (t / target)^...) per frame within [0.001, 8], reset to 0.8 / 0.12 when a view was not drawn last frame. The image
    // carries the value the boot's own frames reached (8 on any current GPU: the fade never applies to normal sizes).
    {
        float a = f32(FADE_STATE), b = f32(FADE_STATE + 4);
        float lo = std::max(a - b, 0.0f), k = 1.0f / (a - lo);
        if (std::isfinite(k) && std::isfinite(a)) { info.fade[0] = k * -1.0f; info.fade[1] = k * a; }
        else { info.fade[0] = 0.0f; info.fade[1] = 1.0f; }   // no fade (00698890 for views >= 4)
        info.fade[2] = f32(ALPHA_KILL); info.fade[3] = 0;
    }
    std::vector<uint8_t> raw;
    std::vector<uint16_t> frames;
    std::vector<int32_t> bucket;
    std::vector<uint32_t> order;
    for (int k = 0; k < 2; k++) {
        uint32_t M = MANAGERS[k], handle = u32(M + 0x70), arr = u32(M + 0x14);
        uint32_t n = u32(M + 0xac), cap = u32(M + 0x1c) + 2;
        if (!handle || !arr || !n) continue;
        uint32_t ad = u32(handle + 0x44);
        if (!ad) continue;
        if (!info.texture[0]) {
            uint32_t tex = u32(M + 0x8c);
            for (int i = 0; tex && i < 63; i++) { char c = char(m.mem.read(tex + 4 + uint32_t(i), 1)); if (!c) break; info.texture[i] = c; }
            // c101 g_NoLightLuminance = (PL * SL, 1), c102 g_LightLuminanceDelta = (PL * (1 - SL), 0) (00699423..006994ef)
            for (int i = 0; i < 3; i++) {
                float sl = f32(ad + 0x60 + 4 * i), pl = f32(ad + 0x70 + 4 * i);
                info.luminance[i] = pl * sl; info.luminance_delta[i] = pl * (1.0f - sl);
            }
            info.luminance[3] = 1.0f; info.luminance_delta[3] = 0.0f;
        }
        n = std::min(n, cap);
        raw.resize(size_t(n) * 0x80);
        m.mem.read_block(arr, raw.data(), raw.size());
        uint32_t ft = u32(ad + 0x18), nf = u32(ad + 0x1c);
        frames.assign(size_t(nf) * 4, 0);
        if (nf && ft) m.mem.read_block(ft, frames.data(), size_t(nf) * 8);
        // view-depth buckets (00699a50): int(min(max((z . p + w - near) * 10000 / (far - near) + 1, 0), 10001))
        bucket.resize(n); order.clear();
        for (uint32_t i = 0; i < n; i++) {
            const float* p = reinterpret_cast<const float*>(&raw[size_t(i) * 0x80]);
            float d = (p[2] * fwd[2] + p[1] * fwd[1] + p[0] * fwd[0] + (vw - near_)) * bucket_scale + 1.0f;
            if (!(d < float(BUCKETS + 1))) d = float(BUCKETS + 1);   // NaN takes the clamp too (minss order)
            if (d < 0.0f) d = 0.0f;
            bucket[i] = int32_t(d);
            if (bucket[i] >= 1 && bucket[i] <= BUCKETS) order.push_back(i);
        }
        // far to near; within a bucket the list is head-inserted in index order, so the last particle comes first
        std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) { return bucket[a] != bucket[b] ? bucket[a] > bucket[b] : a > b; });
        if (order.size() > DRAW_CAP) order.resize(DRAW_CAP);
        for (uint32_t i : order) {
            const uint8_t* q = &raw[size_t(i) * 0x80];
            auto F = [&](int o) { float v; std::memcpy(&v, q + o, 4); return v; };
            auto U = [&](int o) { uint32_t v; std::memcpy(&v, q + o, 4); return v; };
            Particle r{};
            r.position[0] = F(0); r.position[1] = F(4); r.position[2] = F(8);
            r.size[0] = F(0x30); r.size[1] = F(0x34); r.pivot[0] = F(0x70); r.pivot[1] = F(0x74);
            r.scale = F(0x40); r.rotation = F(0x38);
            r.color = U(0x4c); r.color1 = U(0x50);
            // frame: (uint)(float) +0x54, out of range -> the last frame (00699e70)
            float fr = F(0x54);
            int32_t fi = std::isfinite(fr) && std::fabs(fr) < 2.1e9f ? int32_t(fr) : -1;
            if (nf) {
                uint32_t e = (fi >= 0 && uint32_t(fi) < nf) ? uint32_t(fi) : nf - 1;
                const uint16_t* h = &frames[size_t(e) * 4];
                r.uv[0] = half_to_float(h[2]); r.uv[1] = half_to_float(h[3]); r.uv[2] = half_to_float(h[0]); r.uv[3] = half_to_float(h[1]);
            }
            r.flags = uint32_t(k);   // bit 0: additive manager
            out.push_back(r);
        }
        if (k == 0) info.normal = int32_t(out.size()); else info.additive = int32_t(out.size()) - info.normal;
        info.live += int32_t(n);
    }
}

}  // namespace reflex_vm
