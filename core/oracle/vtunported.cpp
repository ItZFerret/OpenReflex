// vtunported.cpp -- oracle adapters (group rx_vtun) for the ports of w-vtunported / w-vtunported2: the targets of vtable-slot calls the
// product could reach unported (core/tools/vtreach_allow.tsv, sigcheck_vt.reach_pass), and the forced-call probe that verifies them:
// none of them is reached by the reference worlds, so the probe (rx_VtunProbeHook, Engine::Update 0084ed20) builds their arguments at one
// frame and calls the ORIGINAL address; with REFLEX_PORT=rx_vtun,rx_VtunProbeHook REFLEX_PORT_VERIFY=1 every call runs the core function and
// then the original on the same state (memory and declared registers compared). Each case runs on a snapshot of the guest memory and the
// machine, restored afterwards. RX_VTUN_PROBE=all|<case,...> RX_VTUN_FRAME=<n> (default 600).
#include "oracle.h"
#include "fx_oracle.h"
#include "anim_oracle.h"
#include "f80/f80.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <limits>
extern "C" {
#include "../game/build_asset_release.h"
#include "../game/build_asset.h"
#include "../game/loop_input.h"
#include "../game/world_node.h"
#include "../game/world_misc.h"
#include "../game/phys_coll_shape.h"
}
using namespace reflex_port;
using rx_oracle::f32;
#define G "rx_vtun"

static void push_st0(reflex_vm::Machine& m, float v) { m.fpu.push_back(reflex_f80::from_f32(rx_oracle::bits(v))); }

// ---- adapters ----
// key maps: AnalogKeyMap::vf6 (thiscall i, dt; RET 8; void), DebugKeyMap::vf4 (thiscall mask, pad; RET 8 -> st0)
RX_PORT(0x008117e0, AnalogKeyMap_vf6, G, OUT_FPU) { rx_analog_keymap_sample_plain(m.r[1], m.arg(0), f32(m.arg(1))); return m.ret(8); }
RX_PORT(0x006d78a0, DebugKeyMap_vf4, G, OUT_FPU) { push_st0(m, rx_debug_keymap_value(m.r[1], m.arg(0), m.arg(1))); return m.ret(8); }
// RiderAnimation::vf17 (thiscall out; RET 4) -> EAX; Database::vf1 (thiscall) -> EAX
RX_PORT(0x00617d70, RiderAnimation_vf17, G, OUT_EAX) { m.r[0] = rx_world_rider_animation_position(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0084a7a0, Database_vf1, G, OUT_EAX) { m.r[0] = rx_database_is_owner_thread(m.r[1]); return m.ret(0); }
// shape bounds (thiscall q*, pos*, out min*, out max*; RET 0x10): HFTerrain leaves EAX = out min, ECX = out max; DynamicRays EDX = out min
RX_PORT(0x008448e0, CollisionShapeHFTerrain_vf2, G, OUT_EAX | OUT_ECX) {
    phys_hfterrain_bounds(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    m.r[0] = m.arg(2); m.r[1] = m.arg(3); return m.ret(0x10);
}
RX_PORT(0x00845100, CollisionShapeDynamicRays_vf2, G, OUT_EDX) {
    phys_dynrays_bounds(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    m.r[2] = m.arg(2); return m.ret(0x10);
}
// the AssetIO getters (thiscall asset) -> EAX

RX_PORT(0x004eef70, V_table_data_asset_get_io, G, OUT_EAX) { m.r[0] = rx_table_data_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x0053a030, V_tdf_asset_get_io, G, OUT_EAX) { m.r[0] = rx_tdf_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x005940a0, V_bink_asset_get_io, G, OUT_EAX) { m.r[0] = rx_bink_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x005c3650, V_localization_asset_get_io, G, OUT_EAX) { m.r[0] = rx_localization_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x00661ee0, V_xenon_forest_asset_get_io, G, OUT_EAX) { m.r[0] = rx_xenon_forest_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x0066c6f0, V_xenon_grass_asset_get_io, G, OUT_EAX) { m.r[0] = rx_xenon_grass_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x00672000, V_xenon_tree_asset_get_io, G, OUT_EAX) { m.r[0] = rx_xenon_tree_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x006937c0, V_particle_attrib_asset_get_io, G, OUT_EAX) { m.r[0] = rx_particle_attrib_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x006ce7c0, V_adv_asset_get_io, G, OUT_EAX) { m.r[0] = rx_adv_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x006d89e0, V_terrain_collision_asset_get_io, G, OUT_EAX) { m.r[0] = rx_terrain_collision_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x006d9a60, V_xenon_terrain_asset_get_io, G, OUT_EAX) { m.r[0] = rx_xenon_terrain_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x0070ccf0, V_lua_asset_get_io, G, OUT_EAX) { m.r[0] = rx_lua_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x007a9ab0, V_xenon_morph_asset_get_io, G, OUT_EAX) { m.r[0] = rx_xenon_morph_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x007d3e60, V_sub_scene_asset_get_io, G, OUT_EAX) { m.r[0] = rx_sub_scene_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x007d5550, V_banner_asset_get_io, G, OUT_EAX) { m.r[0] = rx_banner_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x007e1a10, V_particle_asset_get_io, G, OUT_EAX) { m.r[0] = rx_particle_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x007f02f0, V_xenon_water_asset_get_io, G, OUT_EAX) { m.r[0] = rx_xenon_water_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x007f86d0, V_packed_data_asset_get_io, G, OUT_EAX) { m.r[0] = rx_packed_data_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x007fa7a0, V_cell_asset_get_io, G, OUT_EAX) { m.r[0] = rx_cell_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x00808640, V_sound_asset_get_io, G, OUT_EAX) { m.r[0] = rx_sound_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x00808980, V_sound_bank_asset_get_io, G, OUT_EAX) { m.r[0] = rx_sound_bank_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x00808af0, V_miles_sound_dat_asset_get_io, G, OUT_EAX) { m.r[0] = rx_miles_sound_dat_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x00818ec0, V_collision_asset_get_io, G, OUT_EAX) { m.r[0] = rx_collision_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x0082af80, V_action_asset_get_io, G, OUT_EAX) { m.r[0] = rx_action_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x0082bd70, V_anim_asset_get_io, G, OUT_EAX) { m.r[0] = rx_anim_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x0083f600, V_performance_asset_get_io, G, OUT_EAX) { m.r[0] = rx_performance_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x007d5530, V_iasset_sdtor, G, OUT_EAX) { FxBind fb_(m); AnimBind ab_(m); m.r[0] = rx_iasset_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x007f84b0, V_iasset_io_sdtor, G, OUT_EAX) { FxBind fb_(m); AnimBind ab_(m); m.r[0] = rx_iasset_io_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0083f630, V_performance_asset_sdtor, G, OUT_EAX) { FxBind fb_(m); AnimBind ab_(m); m.r[0] = rx_performance_asset_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00818e80, V_collision_asset_sdtor, G, OUT_EAX) { FxBind fb_(m); AnimBind ab_(m); m.r[0] = rx_collision_asset_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x007f8590, V_packed_data_asset_sdtor, G, OUT_EAX) { FxBind fb_(m); AnimBind ab_(m); m.r[0] = rx_packed_data_asset_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00767ec0, V_collision_geometry_sdtor, G, OUT_EAX) { FxBind fb_(m); AnimBind ab_(m); m.r[0] = rx_collision_geometry_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00818c00, V_collision_geometry_box_sdtor, G, OUT_EAX) { FxBind fb_(m); AnimBind ab_(m); m.r[0] = rx_collision_geometry_box_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00818a80, V_collision_geometry_rays_sdtor, G, OUT_EAX) { FxBind fb_(m); AnimBind ab_(m); m.r[0] = rx_collision_geometry_rays_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00818b90, V_collision_geometry_triangles_sdtor, G, OUT_EAX) { FxBind fb_(m); AnimBind ab_(m); m.r[0] = rx_collision_geometry_triangles_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00818e60, V_collision_geometry_split_sdtor, G, OUT_EAX) { FxBind fb_(m); AnimBind ab_(m); m.r[0] = rx_collision_geometry_split_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00819dc0, V_collision_geometry_composite_sdtor, G, OUT_EAX) { FxBind fb_(m); AnimBind ab_(m); m.r[0] = rx_collision_geometry_composite_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00839ec0, V_anim_track_free0, G, 0) { FxBind fb_(m); rx_anim_track_free0(m.r[1]); return m.ret(0); }
RX_PORT(0x00839f00, V_anim_track_free10, G, 0) { FxBind fb_(m); rx_anim_track_free10(m.r[1]); return m.ret(0); }
RX_PORT(0x0082bbf0, V_anim_asset_sdtor, G, OUT_EAX) { FxBind fb_(m); AnimBind ab_(m); m.r[0] = rx_anim_asset_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x004eed70, V_table_list_remove, G, OUT_EAX) { FxBind fb_(m); m.r[0] = rx_table_list_remove(m.arg(0)); return m.ret(4); }
RX_PORT(0x004f01b0, V_table_data_asset_io_destroy, G, 0) { FxBind fb_(m); AnimBind ab_(m); rx_table_data_asset_io_destroy(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00822250, V_bxml_asset_io_destroy, G, 0) { FxBind fb_(m); AnimBind ab_(m); rx_bxml_asset_io_destroy(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00824fe0, V_xml_reader_dtor, G, 0) { FxBind fb_(m); AnimBind ab_(m); rx_xml_reader_dtor(m.r[6]); return m.ret(0); }
RX_PORT(0x00822170, V_bxml_asset_sdtor, G, OUT_EAX) { FxBind fb_(m); AnimBind ab_(m); m.r[0] = rx_bxml_asset_sdtor(m.r[1], m.arg(0)); return m.ret(4); }

// ---- the forced-call probe ----
namespace {
uint32_t g_frame = 0;
reflex_vm::Machine* g_m = nullptr;
bool mapped(uint32_t a, uint32_t n = 4) { return g_m && g_m->mem.mapped(a) && g_m->mem.mapped(a + n - 1); }
uint32_t rd(uint32_t a) { return mapped(a) ? rx_rd32(a) : 0; }
void wr(uint32_t a, uint32_t v) { if (mapped(a)) rx_wr32(a, v); }
void wrf(uint32_t a, float v) { wr(a, rx_oracle::bits(v)); }
rx_regs regs() { rx_regs r; std::memset(&r, 0, sizeof r); return r; }
uint32_t call(uint32_t fn, rx_regs in, std::initializer_list<uint32_t> args = {}) {
    std::vector<uint32_t> a(args);
    rx_regs out = regs();
    rx_call_original_ex(fn, &in, &out, int(a.size()), a.empty() ? nullptr : a.data());
    return out.r[0];
}
uint32_t thiscall(uint32_t fn, uint32_t self, std::initializer_list<uint32_t> args = {}) { rx_regs in = regs(); in.r[1] = self; return call(fn, in, args); }
uint32_t halloc(uint32_t n) {   // a zeroed game-heap block (ReflexAlignedAlloc 0078fbe0)
    const uint32_t a[1] = {n};
    const uint32_t p = rx_call_original(0x0078fbe0, RX_CDECL, 0, 1, a);
    for (uint32_t i = 0; i < n; i += 4) wr(p + i, 0);
    return p;
}
uint32_t vec4(float x, float y, float z, float w) {   // a 16-byte aligned block holding (x, y, z, w)
    const uint32_t p = halloc(16); wrf(p, x); wrf(p + 4, y); wrf(p + 8, z); wrf(p + 12, w); return p;
}
struct Pages {
    reflex_vm::Memory& memory;
    struct Page { uint32_t address; std::array<uint8_t, 4096> bytes; };
    std::vector<Page> saved;
    std::vector<uint8_t> was;
    explicit Pages(reflex_vm::Memory& m) : memory(m), was(1u << 20, 0) {
        for (uint32_t i = 0; i < (1u << 20); i++) if (m.mapped(i << 12)) {
            was[i] = 1; saved.push_back(Page{}); auto& p = saved.back(); p.address = i << 12;
            std::memcpy(p.bytes.data(), m.page(p.address), 4096);
        }
    }
    void restore() {
        for (auto& p : saved) std::memcpy(memory.page(p.address), p.bytes.data(), 4096);
        for (uint32_t i = 0; i < (1u << 20); i++) if (!was[i] && memory.mapped(i << 12)) memory.unmap(i << 12, 4096);
    }
};
void run_case(reflex_vm::Machine& m, const std::string& name, const std::function<void()>& fn) {
    uint32_t regs_[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
    std::memcpy(regs_, m.r, sizeof regs_); std::memcpy(xmm, m.xmm, sizeof xmm); std::memcpy(mmx, m.mmx, sizeof mmx);
    auto flags = m.flags; auto fpu = m.fpu; auto cw = m.fpu_cw, sw = m.fpu_sw; auto mxcsr = m.mxcsr; bool mmx_active = m.mmx_active;
    auto host_restore = snapshot_host_state(m);
    if (!host_restore) { std::fprintf(stderr, "[vtun] ERROR missing reversible host state\n"); return; }
    Pages pages(m.mem);
    std::fprintf(stderr, "[vtun] case %s\n", name.c_str());
    fn();
    pages.restore();
    host_restore();
    std::memcpy(m.r, regs_, sizeof regs_); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
    m.flags = flags; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
}
const float NaN = std::numeric_limits<float>::quiet_NaN();
uint32_t local_vehicle() {   // the local human's vehicle (the MXGame player slots, +0x1ac)
    const uint32_t g = rd(0x00a946b8);
    for (uint32_t s = 0; g && s < 16; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (rd(p + 4) && !rd(p + 0x10) && rd(p + 0x1ac)) return rd(p + 0x1ac);
    }
    return 0;
}
// a geometry of `vt` (0x20 bytes) with a data block at +0xc
uint32_t geometry(uint32_t vt) { const uint32_t g = halloc(0x20); wr(g, vt); wr(g + 0xc, halloc(0x40)); return g; }
// an AnimAsset-style counted array: n elements of `size`, each with a block at +off
uint32_t counted(uint32_t n, uint32_t size, uint32_t off) {
    const uint32_t b = halloc(4 + n * size); wr(b, n);
    for (uint32_t i = 0; i < n; i++) wr(b + 4 + i * size + off, halloc(0x20));
    return b + 4;
}
uint32_t name_hash() {   // a string hash with two buckets, one chained entry (rx_name_hash_free / 007fa1e0)
    const uint32_t t = halloc(0x20), b = halloc(8), e = halloc(0x10), e2 = halloc(0x10), al = halloc(8);
    wr(al, 0x009753b0);                       /* a StdMemoryAllocator: the entries go back through its vf2 */
    wr(t, 2); wr(t + 0xc, b); wr(b + 4, e); wr(e + 0xc, e2); wr(t + 0x18, al); wr(t + 0x1c, 1);
    return t;
}

using Case = std::function<void()>;
std::map<std::string, Case>& cases() {
    static std::map<std::string, Case> c = {
        {"keymaps", [] {
            // the analog key maps of the key registry (0xd92f50: +0xc / +0x10 the KeyMap vector), every binding, repeat off (+8 = 0)
            const uint32_t reg = 0x00d92f50; int n = 0;
            for (uint32_t p = rd(reg + 0xc); p < rd(reg + 0x10); p += 4) {
                const uint32_t km = rd(p);
                if (rd(km) != 0x00974bdc) continue;
                for (uint32_t i = 0; i < 4; i++) { wr(km + 8, 0); thiscall(0x008117e0, km, {i, rx_oracle::bits(0.016f)}); }
                if (++n == 8) break;
            }
            // the analog key map with its range moved over the input's value (state 1) and NaN bounds
            for (uint32_t p = rd(reg + 0xc); p < rd(reg + 0x10); p += 4) {
                const uint32_t km = rd(p);
                if (rd(km) != 0x00974bdc) continue;
                wrf(km + 0xa4, -1.0f); wrf(km + 0xa8, 1.0f); thiscall(0x008117e0, km, {0, 0});
                wrf(km + 0xa4, NaN); thiscall(0x008117e0, km, {0, 0});
                break;
            }
            // DebugKeyMap::vf4 on the three debug action maps, debug keys on and off (0x9b716d)
            for (uint32_t km : {0x00d5da78u, 0x00d5b108u, 0x00d5cd58u})
                for (uint32_t off : {0u, 1u}) { rx_wr8(0x009b716d, uint8_t(off)); thiscall(0x006d78a0, km, {7, 0}); thiscall(0x006d78a0, km, {7, 1}); }
        }},
        {"rider", [] {   // RiderAnimation::vf17 on the local rider (vehicle +0x1a0)
            const uint32_t v = local_vehicle(), ra = v ? rd(v + 0x1a0) : 0;
            if (!ra) { std::fprintf(stderr, "[vtun] no rider animation\n"); return; }
            const uint32_t out = halloc(16);
            thiscall(0x00617d70, ra, {out});
        }},
        {"database", [] { const uint32_t db = rd(rd(0x00a95844) + 8); thiscall(0x0084a7a0, db); const uint32_t own = rd(db + 0x100); wr(db + 0x100, own + 1); thiscall(0x0084a7a0, db); }},
        {"getters", [] {
            static const uint32_t a[] = {0x004eef70, 0x0053a030, 0x005940a0, 0x005c3650, 0x00661ee0, 0x0066c6f0, 0x00672000, 0x006937c0, 0x006ce7c0,
                                         0x006d89e0, 0x006d9a60, 0x0070ccf0, 0x007a9ab0, 0x007d3e60, 0x007d5550, 0x007e1a10, 0x007f02f0, 0x007f86d0,
                                         0x007fa7a0, 0x00808640, 0x00808980, 0x00808af0, 0x00818ec0, 0x0082af80, 0x0082bd70, 0x0083f600};
            for (uint32_t f : a) thiscall(f, 0);
        }},
        {"shapes", [] {
            const uint32_t q1 = vec4(0.0f, 0.0f, 0.0f, 1.0f), q2 = vec4(0.18f, -0.42f, 0.33f, 0.82f), qn = vec4(NaN, 0.5f, -0.25f, 0.7f);
            const uint32_t pos = vec4(12.5f, -3.0f, 100.25f, 7.0f), mn = halloc(16), mx = halloc(16);
            for (uint32_t q : {q1, q2, qn}) thiscall(0x008448e0, 0, {q, pos, mn, mx});
            const uint32_t s = halloc(0x30), rays = halloc(0x20 * 5);
            const float v[5][8] = {{0, 0, 0, 1, 0, -1, 0, 1}, {1.5f, 0.2f, -2, 1, 1.5f, -0.8f, -2, 1}, {-1, 3, 4, 0, NaN, 1, 2, 1},
                                   {100, -50, 25, 1, 101, -51, 24, 1}, {-0.0f, 0, 0, 1, 0, 0, -0.0f, 1}};
            for (int r = 0; r < 5; r++) for (int k = 0; k < 8; k++) wrf(rays + 0x20 * r + 4 * k, v[r][k]);
            wr(s + 0x24, rays);
            for (uint32_t n : {0u, 1u, 2u, 5u}) { wr(s + 0x20, n); for (uint32_t q : {q1, q2, qn}) thiscall(0x00845100, s, {q, pos, mn, mx}); }
            // the vehicle's own dynamic-rays shapes, when the local vehicle has them (vtable slot 2 = 00845100)
            const uint32_t veh = local_vehicle();
            int found = 0;
            for (uint32_t o = 0; veh && o < 0x3000 && found < 4; o += 4) {
                const uint32_t p = rd(veh + o);
                if (mapped(p, 0x30) && mapped(rd(p), 0xc) && rd(rd(p) + 8) == 0x00845100) { thiscall(0x00845100, p, {q2, pos, mn, mx}); found++; }
            }
            std::fprintf(stderr, "[vtun] vehicle dynamic-rays shapes: %d\n", found);
        }},
        {"geometry", [] {
            for (uint32_t f : {1u, 0u}) {
                thiscall(0x00767ec0, geometry(0x0096dd50), {f});
                thiscall(0x00818c00, geometry(0x00974f64), {f});
                thiscall(0x00818a80, geometry(0x00974f54), {f});
                thiscall(0x00818b90, geometry(0x00974f5c), {f});
                const uint32_t sp = geometry(0x00974f7c), kids = halloc(12);
                wr(kids, geometry(0x00974f5c)); wr(kids + 4, 0); wr(kids + 8, geometry(0x00974f64));
                wr(sp + 0x10, kids); wr(sp + 0x14, 3);
                thiscall(0x00818e60, sp, {f});
                const uint32_t c = halloc(0xa0), cv = halloc(12);
                wr(c, 0x00974f74);
                wr(cv, geometry(0x00974f64)); wr(cv + 4, 0); wr(cv + 8, geometry(0x00974f54));
                wr(c + 0x48, cv); wr(c + 0x4c, cv + 12); wr(c + 0x50, cv + 12); wr(c + 0x3c, halloc(4));
                wr(c + 0x30, halloc(0x20)); wr(c + 0x24, halloc(4)); wr(c + 0xc, halloc(4));
                for (uint32_t o : {0x54u, 0x6cu, 0x84u}) wr(c + o, halloc(4));
                thiscall(0x00819dc0, c, {f});
            }
        }},
        {"assets", [] {
            for (uint32_t f : {1u, 0u}) {
                thiscall(0x007d5530, halloc(0x20), {f});
                thiscall(0x007f84b0, halloc(0x10), {f});
                { const uint32_t a = halloc(0x20); wr(a, 0x00975fec); wr(a + 0x18, halloc(0x40)); thiscall(0x0083f630, a, {f}); }
                { const uint32_t a = halloc(0x20); wr(a, 0x00974f28); wr(a + 0x10, geometry(0x00974f64)); thiscall(0x00818e80, a, {f}); }
                { const uint32_t a = halloc(0x20); wr(a, 0x00974f28); thiscall(0x00818e80, a, {f}); }
                { const uint32_t a = halloc(0xa0); wr(a, 0x00972ba4); wr(a + 0x88, halloc(0x10)); wr(a + 0x90, halloc(0x10)); thiscall(0x007f8590, a, {f}); }
                for (int shared = 0; shared < 2; shared++) {
                    const uint32_t a = halloc(0x70);
                    wr(a, 0x00975ebc); wr(a + 0x14, halloc(0x40));
                    if (shared) wr(a + 0x58, halloc(0x40));
                    wr(a + 0x1c, counted(3, 0x14, 0)); wr(a + 0x24, counted(2, 0x18, 0));
                    wr(a + 0x2c, counted(2, 0x14, 0x10)); wr(a + 0x34, counted(1, 0x14, 0x10)); wr(a + 0x40, counted(4, 0x14, 0x10));
                    const uint32_t x = halloc(0x40); wr(x + 0x3c, halloc(0x20)); wr(a + 0x64, x);
                    thiscall(0x0082bbf0, a, {f});
                }
            }
        }},
        {"tables", [] {
            // a fake loaded-table list with the fake table in the middle, then TableDataAssetIO::vf4 on it and on a non-table asset
            const uint32_t list = halloc(0x10), arr = halloc(0x10), t = halloc(0x40), u = halloc(0x40), o = halloc(0x40);
            wr(t, 0x0093eb54); wr(o, 0x00975fec); wr(o + 0x18, halloc(0x10));
            wr(arr, u); wr(arr + 4, t); wr(arr + 8, u); wr(list + 4, arr); wr(list + 8, 3);
            wr(0x00a89cfc, list);
            rx_regs in = regs(); call(0x004eed70, in, {u});
            thiscall(0x004f01b0, 0, {t});
            thiscall(0x004f01b0, 0, {o});
            thiscall(0x004f01b0, 0, {0});
        }},
        {"bxml", [] {
            // a BXMLAsset: an XMLReader (constructed by the original 00824e70) with its five blocks and a string hash, the IAsset at +0x460
            for (int k = 0; k < 3; k++) {
                const uint32_t o = halloc(0x470);
                rx_regs in = regs(); in.r[6] = o; call(0x00824e70, in, {0});
                wr(o, 0x00975524); wr(o + 0x460, 0x0097566c);
                for (uint32_t off : {8u, 0xcu, 0x14u, 0x1cu, 0x24u}) if (k != 1 || off == 8u) wr(o + off, halloc(0x20));
                if (k != 2) wr(o + 0x10, name_hash());
                if (k == 2) wr(o + 0x45c, 1);    /* an allocator: only the hash contents go */
                if (k == 0) thiscall(0x00822250, 0, {o + 0x460}); else thiscall(0x00822170, o, {1u});
            }
            thiscall(0x00822250, 0, {0});
            // the XMLReader destructor on its own (ESI = the reader), with and without an allocator
            for (int k = 0; k < 2; k++) {
                const uint32_t r = halloc(0x470);
                rx_regs in = regs(); in.r[6] = r; call(0x00824e70, in, {0});
                wr(r + 0xc, halloc(0x20)); wr(r + 0x24, halloc(0x20)); wr(r + 0x10, name_hash());
                if (k) wr(r + 0x45c, 1);
                rx_regs d = regs(); d.r[6] = r; call(0x00824fe0, d);
            }
            // the AnimAsset array elements' destructors on their own
            for (int k = 0; k < 2; k++) {
                const uint32_t e = halloc(0x20);
                if (k) { wr(e, halloc(0x10)); wr(e + 0x10, halloc(0x10)); }
                thiscall(0x00839ec0, e); thiscall(0x00839f00, e);
            }
        }},
    };
    return c;
}
}  // namespace

RX_PORT(0x0084ed20, VtunProbeHook, "thunk", 0) {
    g_m = &m;
    ++g_frame;
    const char* sel = std::getenv("RX_VTUN_PROBE");
    const uint32_t at = std::getenv("RX_VTUN_FRAME") ? uint32_t(std::atoi(std::getenv("RX_VTUN_FRAME"))) : 600;
    if (sel && g_frame == at) {
        std::string s(sel);
        if (s == "all") s = "keymaps,rider,database,getters,shapes,geometry,assets,tables,bxml";
        size_t pos = 0;
        while (pos <= s.size()) {
            size_t e = s.find(',', pos);
            if (e == std::string::npos) e = s.size();
            const std::string t = s.substr(pos, e - pos);
            pos = e + 1;
            if (t.empty()) continue;
            const auto it = cases().find(t);
            if (it == cases().end()) { std::fprintf(stderr, "[vtun] unknown case %s\n", t.c_str()); continue; }
            run_case(m, t, it->second);
        }
        std::fprintf(stderr, "[vtun] frame %u: cases done\n", g_frame);
    }
    return reflex_vm::RUN_ORIGINAL;
}
