// Ambient animated entities of the original world: what the original renderer draws of the decorative characters and objects
// after each frame (crew chiefs, pit crews, score-tower people and flaggers, looping / spline / scenario animations, roaming
// people, the instanced grandstand crowd, billboard crowds). Read only; the animation is the original's own (the classes' vf19
// updates run, World::set_ambient_mask). Names in _re/names/76_ambient.tsv.
//
// Recovered (MXReflex.exe 1.1.0.11):
//  - Every class below but two derives from CharacterEntity (vtable 0x976134; ModelEntity -> Entity, IRenderable at +0x90,
//    IRenderEvent at +0x140). Layout: +0x14 entity id (scene entityId), +0x30 entity world matrix (scene Matrix), +0xa0 model
//    instance (ModelInstance_Init 007f12c0: +0xa0 LOD alpha, +0xa1 LOD index, +0xa2 render enabled (0 = hidden: idle crew chiefs,
//    scenario actors waiting for their scenario), +0xb0 flags, +0xc0 world matrix copy, +0x100 ModelAssetHandle (name inline at
//    +4, ModelAsset at +0x44: +0x118 LOD count, +0x120 draw distance)), +0x144 AnimAssetHandle, +0x148 Character / action context
//    (CharacterAnim_Init 00834de0), +0x200 the instance's own ModelAsset copy (vtable 0x972844) whose node set at +0x210 is the
//    animated skeleton: +0x218 node records (16 bytes: +0 node set, +0xb index, +0xc flags bit 0 = world valid, +0xd first child,
//    +0xe next sibling, +0xf parent), +0x21c node count n, +0x228 local matrices, +0x22c world matrices, +0x234 inverse bind.
//    Record n is the entity node: its world matrix is the entity placement and the parent of node 0, W[i] = L[i] * W[parent]
//    (row vectors). A second copy of the set at +0x250 is the render copy. The animation job (AnimEntitySingleAnimLoop::vf19
//    006ba250 -> JobPool job 006ba170, CrewChief::vf19 006bd6e0 -> 006bd6a0, ...) writes local and world matrices of every node.
//  - CharacterEntity::vf2_o90 (00839b60) submits Model_SubmitSkinned (007f4e50): culled beyond ModelAsset+0x120 / renderer
//    divisor ([[0xa95844 + 0x1c + 4 [0xa95844+4]] + 0x19d0]), alpha faded from 0.7 (0x9c4b4c) of that distance squared.
//  - InstancedAnimationEntity (0x939d24; AnimEntitySingleAnimLoopInstanced, the grandstand crowd): not a ModelEntity. vf5
//    (00412480) shares one template AnimEntitySingleAnimLoop per (LoopingAnimation, ModelAsset) pair (table 0xa95d88, count
//    0xa96d88, 16 bytes: anim, model, template, flag); the first instance of a pair owns it (+0x9f0, its vf19 004137b0 runs the
//    template's vf19) and the others reference it (+0x9f4). +0xa00 is the instance's matrix relative to the template's pose
//    (inverse(template entity matrix) * instance matrix, row vectors): the render event vf0_o98 (00413840) draws the template's
//    skinned model (Model_RenderInstance 007f5680) with it. +0xa64 draw distance.
//  - BillboardCrowdEntity (0x960654, ModelEntity): a static crowd section model drawn by XenonBillboardCrowdRenderEvent
//    (006c6cd0) with the Crowd shader (StaticVertexShader + CrowdPixelShader, CameraFlashVS/PS): +0x140 animation phase
//    (fmod clock, vf19 006c6650; shader constant c65 when animated), +0x144 "Is Non Animated", +0x148 "Tier Number",
//    +0x14c CameraFlash texture, +0x17c..+0x180 camera flashes (16 bytes: active, age, duration, slot), spawned with the
//    game's own LCG 0x9cae3c (not the CRT rand()).
#include "ambient.h"
#include "vm.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace reflex_vm {

namespace {
struct Kind { uint32_t vt0, vt90; const char* name; };
// Order = bits of World::set_ambient_mask (runtime.cpp classes[]) for the first 10.
constexpr Kind KINDS[] = {
    {0x95f674, 0x95f6f4, "AnimEntitySingleAnimLoop"},
    {0x96cee4, 0x96cf64, "ScenarioAnimEntity"},
    {0x95f83c, 0x95f8bc, "AnimEntitySplineAnimLoop"},
    {0x95f4e4, 0x95f564, "CrewChief"},
    {0x939d24, 0x939d9c, "InstancedAnimationEntity"},
    {0x95f73c, 0x95f7bc, "AnimEntitySingleAnimLoopWithParticles"},
    {0x95f904, 0x95f984, "AnimEntitySplineAnimLoopWithParticles"},
    {0x960dec, 0x960e6c, "LandAnimalProximity"},
    {0x960654, 0x9606cc, "BillboardCrowdEntity"},
    {0x960094, 0x960114, "ScoreTowerFlagHolder"},
    // always simulated (not in the skip list)
    {0x960214, 0x960294, "TrackSideFlagHolder"},
    {0x95f9cc, 0x95fa4c, "AnimEntitySplineWithStopLogic"},
    {0x95fa94, 0x95fb14, "AnimEntitySplineWithStopLogicWithParticles"},
    {0x95f5ac, 0x95f62c, "AnimEntityProximity"},
    {0x95fb5c, 0x95fbdc, "AnimEntityTrigger"},
    {0x960c6c, 0x960cec, "AirAnimalSoaring"},
};
constexpr int KIND_COUNT = int(sizeof KINDS / sizeof KINDS[0]);
constexpr int INSTANCED = 4, BILLBOARD = 8;
constexpr uint32_t MODEL_ASSET_VT = 0x972844, RENDERER = 0xa95844, FADE_START = 0x9c4b4c;
constexpr uint32_t TEXTURE_HANDLE_VT = 0x9719c8, TEXTURE_ASSET_VT = 0x9462d0;
constexpr int CREW_CHIEF = 3;
enum : uint32_t { VISIBLE = 1, SKINNED = 2, INSTANCE = 4, TEMPLATE = 8, ANIMATED = 16, COMPLETED = 32 };

float as_float(uint32_t v) { float f; std::memcpy(&f, &v, 4); return f; }
void mul(const float* a, const float* b, float* o) {   // o = a * b, 4x4 row-major (row vectors: local * parent world)
    float r[16];
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
        r[i * 4 + j] = (a[i * 4 + 3] * b[12 + j] + a[i * 4 + 1] * b[4 + j]) + (a[i * 4 + 2] * b[8 + j] + a[i * 4] * b[j]);   // 007db955: (w r3 + y r1) + (z r2 + x r0)
    std::memcpy(o, r, sizeof r);
}
}  // namespace

int ambient_kind_count() { return KIND_COUNT; }
uint32_t ambient_kind_vtable(int k) { return k >= 0 && k < KIND_COUNT ? KINDS[k].vt0 : 0; }
const char* ambient_kind_name(int k) { return k >= 0 && k < KIND_COUNT ? KINDS[k].name : ""; }

// Objects are found once (the level's entities live as long as the world): every heap page, both vtables of the class
// (+0 and the IRenderable at +0x90) must match.
void AmbientScene::scan(Machine& m) {
    objects_.clear(); texture_handles_.clear();
    std::unordered_map<uint32_t, int> by_vt;
    for (int k = 0; k < KIND_COUNT; k++) by_vt[KINDS[k].vt0] = k;
    for (uint64_t page = 0x00e00000; page < 0x80000000ull; page += 0x1000) {
        uint32_t pa = uint32_t(page);
        if (!m.mem.mapped(pa)) continue;
        uint8_t buf[4096]; m.mem.read_block(pa, buf, 4096);
        for (uint32_t off = 0; off < 4096; off += 4) {
            uint32_t v; std::memcpy(&v, buf + off, 4);
            if (v == TEXTURE_HANDLE_VT && off + 0x48 <= 4096) {   // texture handles (name inline at +4, TextureAsset at +0x44)
                uint32_t data; std::memcpy(&data, buf + off + 0x44, 4);
                if (data) texture_handles_.emplace(data, pa + off);
                continue;
            }
            if (v < 0x939d24 || v > 0x96cee4) continue;
            auto it = by_vt.find(v); if (it == by_vt.end()) continue;
            uint32_t a = pa + off;
            if (!m.mem.mapped(a + 0x94) || uint32_t(m.mem.read(a + 0x90, 4)) != KINDS[it->second].vt90) continue;
            objects_.push_back({a, it->second});
        }
    }
    scanned_ = true;
}

void AmbientScene::collect(Machine& m, std::vector<AmbientEntity>& out, std::vector<float>& matrices, AmbientInfo& info) {
    out.clear(); matrices.clear(); std::memset(&info, 0, sizeof info);
    if (!scanned_) scan(m);
    auto u32 = [&](uint32_t a) { return m.mem.mapped(a) ? uint32_t(m.mem.read(a, 4)) : 0u; };
    auto u8 = [&](uint32_t a) { return m.mem.mapped(a) ? uint32_t(m.mem.read(a, 1)) : 0u; };
    uint32_t R = u32(RENDERER), divisor = R ? u32(u32(R + 0x1c + 4 * u32(R + 4)) + 0x19d0) : 1;
    if (!divisor) divisor = 1;
    info.lod_divisor = float(divisor); info.fade_start = as_float(u32(FADE_START));
    // crowd templates first (an instance refers to its template by list index)
    std::unordered_map<uint32_t, int> index;   // object address -> out index
    std::unordered_map<uint32_t, bool> is_template;
    for (auto& o : objects_)
        if (o.kind == INSTANCED && u32(o.address) == KINDS[INSTANCED].vt0) {
            uint32_t t = u32(o.address + 0x9f0); if (!t) t = u32(o.address + 0x9f4);
            if (t) is_template[t] = true;
        }
    auto model_name = [&](uint32_t handle, char* dst) {
        std::memset(dst, 0, 64);
        if (handle && m.mem.mapped(handle + 4)) m.mem.read_bytes(handle + 4, dst, 63);
        dst[63] = 0;
    };
    auto lod = [&](uint32_t handle) {
        uint32_t md = handle ? u32(handle + 0x44) : 0;
        return md ? float(u32(md + 0x120) / divisor) : 0.0f;
    };
    std::vector<float> L, W; std::vector<uint8_t> rec;
    auto character = [&](uint32_t a, int kind) {
        AmbientEntity e{}; e.object = a; e.entity_id = u32(a + 0x14); e.kind = kind; e.source = -1;
        uint32_t handle = u32(a + 0x100);
        model_name(handle, e.model); e.lod_distance = lod(handle);
        if (u8(a + 0xa2)) e.flags |= VISIBLE;
        if (is_template.count(a)) e.flags |= TEMPLATE;
        m.mem.read_bytes(a + 0x30, e.matrix, 64);
        if (kind == CREW_CHIEF) {
            // the shirt (FUN_006bd760, CrewChief_ShirtColor_Default -> the local rider's crew chief texture, driver +0x94): the texture
            // override map at +0x388 (std::map: head +0x38c, size +0x390; node +0xc key, +0x10 value = TextureAsset data)
            uint32_t head = u32(a + 0x38c), size = u32(a + 0x390);
            uint32_t node = size ? u32(head + 4) : 0;   // the root node (one entry)
            uint32_t value = node && node != head ? u32(node + 0x10) : 0;
            if (value && u32(value) == TEXTURE_ASSET_VT) {
                auto it = texture_handles_.find(value);
                if (it != texture_handles_.end()) { m.mem.read_bytes(it->second + 4, e.texture, 63); e.texture[63] = 0; }
                uint32_t pkg = u32(value + 0xc);
                if (pkg && m.mem.mapped(pkg)) { m.mem.read_bytes(pkg, e.texture_package, 63); e.texture_package[63] = 0; }
            }
        }
        if (kind == BILLBOARD) {
            // XenonBillboardCrowdRenderEvent (006c74a6 ..): c65 time = +0x140; c71 isAnimating = "Is Non Animated" == 0; c66 emptyChairRatio =
            // ((tier - 1) * 0.133 + (4 - tier)) (double) * (1/3) * crowd density [0x9c3ed8], 100 when the density >= 100 or tier 0
            int32_t tier = int32_t(u32(a + 0x148));
            float density = as_float(u32(0x9c3ed8)), ratio = float(double(tier - 1) * 0.133 + double(4 - tier)) * as_float(u32(0x92e214)) * density;
            if (density >= as_float(u32(0x93005c)) || tier == 0) ratio = as_float(u32(0x93005c));
            e.param[0] = as_float(u32(a + 0x140)); e.param[1] = ratio; e.param[2] = float(u32(a + 0x144) != 0);
            if (!u32(a + 0x144)) e.flags |= ANIMATED;
            uint32_t b = u32(a + 0x17c), en = u32(a + 0x180); int active = 0;
            for (uint32_t f = b; b && f < en && f < b + 64 * 16; f += 16) active += u8(f) != 0;
            e.param[3] = float(active);
            return e;
        }
        // the animated skeleton (node set of the instance's ModelAsset copy)
        uint32_t n = u32(a + 0x21c), records = u32(a + 0x218), lw = u32(a + 0x228), ww = u32(a + 0x22c);
        if (u32(a + 0x200) != MODEL_ASSET_VT || n == 0 || n > 255 || !records || !lw || !ww) return e;
        L.resize(size_t(n + 1) * 16); W.resize(size_t(n + 1) * 16); rec.resize(size_t(n + 1) * 16);
        m.mem.read_block(lw, L.data(), L.size() * 4); m.mem.read_block(ww, W.data(), W.size() * 4); m.mem.read_block(records, rec.data(), rec.size());
        // nodes the animation left invalid (record +0xc bit 0 clear): completed here like the lazy evaluator 007db890 does
        std::vector<uint8_t> done(n + 1);
        for (uint32_t i = 0; i <= n; i++) done[i] = rec[i * 16 + 0xc] & 1;
        for (uint32_t i = 0; i < n; i++) {
            if (done[i]) continue;
            uint32_t chain[256]; int depth = 0; uint32_t j = i;
            while (j <= n && !done[j] && depth < 256) { chain[depth++] = j; uint32_t p = rec[j * 16 + 0xf]; if (p == 0xff) break; j = p; }
            for (int d = depth - 1; d >= 0; d--) {
                uint32_t c = chain[d], p = rec[c * 16 + 0xf];
                if (p == 0xff || p > n) std::memcpy(&W[c * 16], &L[c * 16], 64); else mul(&L[c * 16], &W[p * 16], &W[c * 16]);
                done[c] = 1;
            }
            e.flags |= COMPLETED;
        }
        e.flags |= SKINNED; e.bones = int32_t(n); e.first_matrix = int32_t(matrices.size() / 16);
        matrices.insert(matrices.end(), W.begin(), W.begin() + size_t(n) * 16);
        std::memcpy(e.matrix, &W[size_t(n) * 16], 64);   // the entity node: the placement the skeleton hangs from
        return e;
    };
    for (int pass = 0; pass < 2; pass++)
        for (auto& o : objects_) {
            uint32_t a = o.address;
            if (u32(a) != KINDS[o.kind].vt0 || u32(a + 0x90) != KINDS[o.kind].vt90) continue;   // freed / reused
            bool templ = is_template.count(a) != 0;
            if ((pass == 0) != templ) continue;
            if (o.kind == INSTANCED) {
                uint32_t t = u32(a + 0x9f0); if (!t) t = u32(a + 0x9f4);
                auto it = index.find(t); if (it == index.end()) continue;
                AmbientEntity e{}; e.object = a; e.entity_id = u32(a + 0x14); e.kind = o.kind; e.source = it->second;
                e.flags = INSTANCE | (out[size_t(it->second)].flags & VISIBLE);
                m.mem.read_bytes(a + 0xa00, e.matrix, 64);
                std::memcpy(e.model, out[size_t(it->second)].model, 64);
                e.lod_distance = as_float(u32(a + 0xa64));
                e.param[0] = float(u8(a + 0xa6c));   // LOD index the template was drawn with for this instance
                out.push_back(e);
                continue;
            }
            index[a] = int(out.size());
            out.push_back(character(a, o.kind));
        }
    info.entities = int32_t(out.size()); info.matrices = int32_t(matrices.size() / 16); info.objects = int32_t(objects_.size());
}

}  // namespace reflex_vm
