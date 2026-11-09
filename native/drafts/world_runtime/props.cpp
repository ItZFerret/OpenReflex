// Physics props and animated textures of the original world (props.h, _re/names/87_props.tsv, _re/notes/props.md). Read only.
//
// Recovered (MXReflex.exe 1.1.0.11):
//  - PhysicsEntity (vtable 0x93743c, IRenderable 0x9374c4; ModelEntity -> Entity) is the scene's PhysicsRSData: round / square bales,
//    tyre stacks, cones, barrels, fences ... Layout as every ModelEntity (ambient.cpp): +0x14 entity id (scene Id), +0x30 entity world
//    matrix (row vectors), +0xa0 ModelInstance (+0xa2 render enabled), +0xc0 its world matrix copy. PhysicsEntity::vf5 (0041c8a0)
//    reads FemaleCollisionAsset +0x144, MaleCollisionAsset +0x140, CollisionAudioMaterial +0x148, CollisionAudioHardness / soft
//    collision +0x14c, AIAvoid (+0x74 bit 3), PhysicsType +0x150 (FUN_00850bf0: the physics material table), AudioEmitter +0x160,
//    then creates the rigid body (FUN_0041ccf0 -> FUN_00769670): +0x158 is the IntrusiveHandle of the RavenPhysics object
//    (handle +4 physics world, +8 index into the world's 0xe0-byte body records at world+0x19a0, FUN_00418f30): record +0x40 position,
//    +0x50 orientation, +0x60 / +0x70 velocities, +0xb0 enabled (vf1 0041cbc0 / vf2 0041cbe0), +0xb1 / +0xb2 flags.
//    The physics write-back is vf30 (004039a0: position + quaternion -> the matrix at +0x30, q -> rows as 007db x 2 formulas);
//    vf28 (004037c0) / vf16 (00403720) set the entity and push the pose into the body, vf31 (00403b20) reads it.
//  - PopUpBale (0x95ffcc / 0x960054) and PopDownBale (0x95ff04 / 0x95ff8c) are PhysicsEntity subclasses: PopUpBale::vf5 (006c0e70)
//    hides the bale (+0xa2 = 0) 5 m under its scene position (+0x1c0 home, +0x1d0 start); vf19 (006c1250) raises it once its
//    LinkedTriggerID trigger fired (+0x174 = 1): the row-3 translation +0x60..+0x68 moves up by ~+0x1e0 per tick, and at the top the
//    body is placed and enabled, +0xa2 = 1. MXStepUpBarEntity (0x939034 / 0x9390bc) and VehicleDamagePhysicsEntity (0x93ccac /
//    0x93cd34; parts split off a vehicle model at run time by FUN_00479ef0, name "%x_%x_%s") share the layout.
//  - ScrollingTextureEntity (0x939ba4 / 0x939c1c / 0x939c5c, ScrollingTextureRSData): vf5 (00410c70) adds the material parameter
//    "gTextureOffset" (vertex constant c85 of every scrolling template) bound to +0xaa0 (u, v); +0xab0 / +0xab4 = "U / V Seconds For
//    Wrap" times -1 (0x9cd9e8) when "Reverse U / V"; vf19 (00410dd0) per tick: a += dt, a -= |T| when a >= |T|, offset = a / T
//    (accumulators +0xab8 / +0xabc). The vertex programs add it to uv0 (Template_Animated_UV_nonlit_alpha 050c: add o3.xy, c85, v3).
//  - TextureAnimEntity (0x973ccc, TextureAnimControllerRSData, no IRenderable): +0x90 TextureName (0x80 chars), +0x110 BeginFrame,
//    +0x114 EndFrame, +0x118 LoopStartFrame, +0x11c mode (1 cycles, 2 ping-pong), +0x120 seconds per frame (1 / FramesPerSecond),
//    +0x124 accumulator, +0x12c ping-pong direction, +0x130 the texture handle (found by name on the first tick, vf19 0080f660):
//    handle +0x44 = the texture data, data +0x14 frame count, +0x18 the frame drawn (FUN_0080f570 steps it). The texture resource
//    holds every frame (header word 3 = frames, word 4 = levels of all frames, 16-byte frame descriptors from +0x28).
#include "props.h"
#include "vm.h"
#include <cstring>
#include <unordered_map>

namespace reflex_vm {

namespace {
struct Kind { uint32_t vt0, vt90; const char* name; };
constexpr Kind KINDS[] = {
    {0x93743c, 0x9374c4, "PhysicsEntity"},
    {0x95ffcc, 0x960054, "PopUpBale"},
    {0x95ff04, 0x95ff8c, "PopDownBale"},
    {0x939034, 0x9390bc, "MXStepUpBarEntity"},
    {0x93ccac, 0x93cd34, "VehicleDamagePhysicsEntity"},
};
constexpr int KIND_COUNT = int(sizeof KINDS / sizeof KINDS[0]);
constexpr uint32_t SCROLL_VT = 0x939ba4, SCROLL_VT90 = 0x939c1c, TEXANIM_VT = 0x973ccc;
enum : uint32_t { VISIBLE = 1, MOVED = 2, NEW = 4 };

float as_float(uint32_t v) { float f; std::memcpy(&f, &v, 4); return f; }
}  // namespace

int prop_kind_count() { return KIND_COUNT; }
const char* prop_kind_name(int k) { return k >= 0 && k < KIND_COUNT ? KINDS[k].name : ""; }

// Found once per world (the level's entities live as long as the world; VehicleDamagePhysicsEntity parts appear later: rescan):
// every mapped page from the heap base, both vtables of the class must match.
void PropScene::scan(Machine& m) {
    objects_.clear(); textures_.clear();
    std::unordered_map<uint32_t, int> by_vt;
    for (int k = 0; k < KIND_COUNT; k++) by_vt[KINDS[k].vt0] = k;
    for (uint64_t page = 0x00e00000; page < 0x80000000ull; page += 0x1000) {
        uint32_t pa = uint32_t(page);
        if (!m.mem.mapped(pa)) continue;
        uint8_t buf[4096]; m.mem.read_block(pa, buf, 4096);
        for (uint32_t off = 0; off < 4096; off += 4) {
            uint32_t v; std::memcpy(&v, buf + off, 4);
            if (v < 0x93743c || v > 0x973ccc) continue;   // PhysicsEntity .. TextureAnimEntity
            uint32_t a = pa + off;
            if (v == SCROLL_VT || v == TEXANIM_VT) {
                if (v == SCROLL_VT && (!m.mem.mapped(a + 0xac0) || uint32_t(m.mem.read(a + 0x90, 4)) != SCROLL_VT90)) continue;
                if (v == TEXANIM_VT && !m.mem.mapped(a + 0x134)) continue;
                Object o{}; o.address = a; o.kind = v == SCROLL_VT ? 0 : 1; textures_.push_back(o);
                continue;
            }
            auto it = by_vt.find(v); if (it == by_vt.end()) continue;
            if (!m.mem.mapped(a + 0x164) || uint32_t(m.mem.read(a + 0x90, 4)) != KINDS[it->second].vt90) continue;
            Object o{}; o.address = a; o.kind = it->second; o.flags = NEW;
            objects_.push_back(o);
        }
    }
    scanned_ = true;
}

void PropScene::collect(Machine& m, PropEntity* out, uint32_t max, bool all, PropInfo& info) {
    std::memset(&info, 0, sizeof info);
    if (!scanned_) scan(m);
    uint32_t n = 0, pending = 0;
    for (auto& o : objects_) {
        uint32_t a = o.address;
        uint32_t vt0, vt90, flags; float mat[16];
        const uint8_t* page = (a & 0xfff) + 0xa4 <= 0x1000 ? m.mem.peek(a) : nullptr;   // resident and within one page: read in place
        if (page) {
            const uint8_t* p = page + (a & 0xfff);
            std::memcpy(&vt0, p, 4); std::memcpy(&vt90, p + 0x90, 4); std::memcpy(mat, p + 0x30, 64); flags = p[0xa2] ? VISIBLE : 0u;
        } else {
            vt0 = uint32_t(m.mem.read(a, 4)); vt90 = uint32_t(m.mem.read(a + 0x90, 4));
            m.mem.read_bytes(a + 0x30, mat, 64); flags = uint32_t(m.mem.read(a + 0xa2, 1)) ? VISIBLE : 0u;
        }
        if (vt0 != KINDS[o.kind].vt0 || vt90 != KINDS[o.kind].vt90) continue;   // freed / reused
        info.props++;
        bool changed = all || !o.reported || std::memcmp(mat, o.matrix, 64) != 0 || (o.flags & VISIBLE) != flags;
        if (!o.reported && !o.first_set) { std::memcpy(o.first, mat, 64); o.first_set = true; }
        if (!changed) continue;
        if (n >= max) { pending++; continue; }
        PropEntity& e = out[n++];
        e.object = a; e.entity_id = uint32_t(m.mem.read(a + 0x14, 4)); e.kind = o.kind;
        e.flags = flags | (o.reported ? 0u : NEW) | (std::memcmp(mat, o.first, 64) != 0 ? MOVED : 0u);
        std::memcpy(e.matrix, mat, 64);
        std::memcpy(o.matrix, mat, 64);
        o.flags = flags; o.reported = true;
    }
    info.reported = int32_t(n); info.objects = int32_t(objects_.size()); info.pending = int32_t(pending);
}

void PropScene::textures(Machine& m, std::vector<TextureAnimState>& out) {
    out.clear();
    if (!scanned_) scan(m);
    auto u32 = [&](uint32_t a) { return m.mem.mapped(a) ? uint32_t(m.mem.read(a, 4)) : 0u; };
    for (auto& o : textures_) {
        uint32_t a = o.address;
        TextureAnimState t{}; t.object = a; t.entity_id = u32(a + 0x14); t.kind = o.kind;
        if (o.kind == 0) {
            if (u32(a) != SCROLL_VT || u32(a + 0x90) != SCROLL_VT90) continue;
            if (m.mem.read(a + 0xa2, 1)) t.flags |= VISIBLE;
            t.offset[0] = as_float(u32(a + 0xaa0)); t.offset[1] = as_float(u32(a + 0xaa4));
            t.param[0] = as_float(u32(a + 0xab0)); t.param[1] = as_float(u32(a + 0xab4));   // signed periods (negative = reversed)
            t.param[2] = as_float(u32(a + 0xab8)); t.param[3] = as_float(u32(a + 0xabc));   // accumulators
        } else {
            if (u32(a) != TEXANIM_VT) continue;
            m.mem.read_bytes(a + 0x90, t.target, 63); t.target[63] = 0;
            uint32_t handle = u32(a + 0x130), data = handle ? u32(handle + 0x44) : 0;
            t.flags |= VISIBLE;
            t.param[0] = data ? float(int32_t(u32(data + 0x18))) : float(int32_t(u32(a + 0x110)));   // frame drawn
            t.param[1] = data ? float(int32_t(u32(data + 0x14))) : 0.0f;                           // frame count
            float spf = as_float(u32(a + 0x120)); t.param[2] = spf > 0 ? 1.0f / spf : 0.0f;          // frames per second
            t.param[3] = float(int32_t(u32(a + 0x11c)));                                             // 1 cycles, 2 ping-pong
        }
        out.push_back(t);
    }
}

}  // namespace reflex_vm
