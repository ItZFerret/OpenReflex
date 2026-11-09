// dtor_probe.cpp -- the oracle's probe for the destructors the activity teardown reaches (core/game/build_activity_dtors.c,
// core/game/anim_rider_dtors.c): MXActivityFreeRide vf3 / sdtor, MXActivityWaypointRace vf3, Activity::sdtor, MXActivityRaceStub::
// sdtor, RavenVehicle / Motocross::sdtor, RiderAnimation::sdtor (and its chain), ScenarioAnimEntity::sdtor. No reference world
// deletes these objects (0 calls). A frame hook (Engine::Update 0084ed20, outside every verified call) calls the ORIGINAL
// addresses (read from the live objects' vtables where the call is virtual) on the live objects; with the ports installed Verify
// runs the core function and then the original on the same state and compares memory and registers. Each case runs on a snapshot
// of the whole guest memory and machine state, restored afterwards (the run continues on the untouched world).
//   REFLEX_PORT=<ports>,rx_DtorProbeHook REFLEX_PORT_VERIFY=1 RX_DT_PROBE=<cases> [RX_DT_FRAME=<n>]
//   <ports>: the adapters of the teardown files by name (a group such as rx_loop would install Engine_Update at the hook's address) --
//   anim_rider_dtors.cpp build_activity_dtors.cpp build_activity_teardown.cpp phys_shape_dtors.cpp phys_static_objects.cpp
//   anim_entity_dtors.cpp anim_scene.cpp.
//   cases (RX_DT_PROBE; "none" = the baseline run of a REFLEX_ORIG_CENSUS comparison; empty = a v r s d c h f o):
//     a activity: vf3, vf6(0), vf0(1) of the player's activity (Free Ride, Waypoint, race: the area subscene's entities are released
//       by vf6; the subscene's entities and whether their release virtuals are ported are printed); Activity / RaceStub sdtors
//     v vehicle sdtors, r rider animation sdtor, d the rider animation chain's pieces, c vehicle vf9 / camera entities, s scenario anims
//     h collision shapes: every class of the shape registry (0xd6d8e0) deleted with flags 0 / 1 and the allocator lock flag 0 / 1, the
//       shape handle; f made-up shape classes (StaticRays, Triangles, Composite, StaticComposite, DynamicComposite)
//     k the entity classes the PRODUCT factory (rx_scene_create_entity) builds, with the ported state of vf0 / vf2 / vf9 of each
//     o entities built by the core's factory (RX_DT_IDS=hex,..): vf0(0) vf0(1) vf9 vf2 on a fresh object, Verify against the original
//     b the live BillboardCrowd entities of the world: vf0(1) of each in turn (the last frees the shared render event), RX_DT_BILLBOARD_FIRST=<n>
//   The collision shapes are no longer pinned (the old behaviour: RX_DT_PIN=1 gives every dynamic record's shape an extra reference).
//   REFLEX_ORIG_CENSUS=1 counts the core's calls of originals; "probe-call" lines count the probe's own, so census - probe-call is
//   what the product would stop at.
#include "oracle.h"
#include <array>
#include <csetjmp>
#include <csignal>
#include <map>
#include <functional>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
using namespace reflex_port;

// REFLEX_ORIG_CENSUS counts every rx_call_original(_ex); the probe's own calls (the entry points it drives) are counted here and printed
// ("probe-call"), so that census minus probe-call is what the CORE called through the original (what the product would stop at)
namespace { std::map<uint32_t, uint64_t> g_probe_calls; }
#define rx_call_original_ex(fn, ...) (g_probe_calls[(fn)]++, rx_call_original_ex((fn), __VA_ARGS__))

extern "C" void* rx_fn(uint32_t);
extern "C" uint32_t rx_scene_create_entity(uint32_t id);   // the core's entity factory (build_scene.c)   // the core registry: is a virtual a ported function (else the product stops there)
namespace {
uint32_t g_frame = 0;
reflex_vm::Machine* g_m = nullptr;
bool mapped(uint32_t a, uint32_t n = 4) { return g_m && g_m->mem.mapped(a) && g_m->mem.mapped(a + n - 1); }
uint32_t rd(uint32_t a) { return mapped(a) ? rx_rd32(a) : 0; }
rx_regs regs() { rx_regs r; std::memset(&r, 0, sizeof r); return r; }

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

uint32_t find_player();
// a case body runs between the snapshot and its restore
template <class F> void with_snapshot(reflex_vm::Machine& m, const char* what, F body) {
    uint32_t r[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
    std::memcpy(r, m.r, sizeof r); std::memcpy(xmm, m.xmm, sizeof xmm); std::memcpy(mmx, m.mmx, sizeof mmx);
    auto flags = m.flags; auto fpu = m.fpu; auto cw = m.fpu_cw, sw = m.fpu_sw; auto mxcsr = m.mxcsr; bool mmx_active = m.mmx_active;
    auto host_restore = snapshot_host_state(m);
    if (!host_restore) { std::fprintf(stderr, "[dt] ERROR missing reversible host state\n"); return; }
    Pages pages(m.mem);
    std::fprintf(stderr, "[dt] case %s\n", what);
    /* MXRecords_Leave (00764560, reached from Activity_OnPlayerLeft for the local human) runs the records owner 007647f0 (its oracle
     * calls are closed since w-actarea, records_leave_highlights.c): by default its object [0xa946c0] is null for the case
     * (RX_DT_RECORDS=1 keeps it) */
    if (!std::getenv("RX_DT_RECORDS")) rx_wr32(0x00a946c0, 0);
    /* the collision shapes' destructors (RavenPhysics::CollisionShape*::sdtor, core/game/phys_shape_dtors.c) are ported: the shapes of
     * the physics world's dynamic records (vector +0x1954, 0xe0-byte records, the shape handle's target at +0x2c) lose their last
     * reference in the body removals and are deleted. RX_DT_PIN=1 gives every one an extra reference again (the old behaviour of this
     * probe, from before the shape destructors) */
    if (std::getenv("RX_DT_PIN")) {
        const uint32_t V = rd(find_player() + 0x1ac);
        const uint32_t pw = V ? rd(rd(V + 0x188) + 0x20134) : 0;
        if (pw) {
            const uint32_t b = rd(pw + 0x1954 + 0xc), e = rd(pw + 0x1954 + 0x10);
            for (uint32_t r = b; r && r < e; r += 0xe0) { const uint32_t sh = rd(r + 0x2c); if (sh && mapped(sh + 4)) rx_wr32(sh + 4, rd(sh + 4) + 1); }
        }
    }
    body();
    pages.restore();
    host_restore();
    std::memcpy(m.r, r, sizeof r); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
    m.flags = flags; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
}

// thiscall of an original address
uint32_t thiscall(uint32_t fn, uint32_t ecx, int n = 0, const uint32_t* a = nullptr) {
    rx_regs in = regs(), out = regs();
    in.r[1] = ecx;
    rx_call_original_ex(fn, &in, &out, n, a);
    return out.r[0];
}
uint32_t vslot(uint32_t obj, uint32_t byte_off) { return rd(rd(obj) + byte_off); }

uint32_t find_player() {
    const uint32_t g = rd(0x00a946b8);
    if (!g) return 0;
    for (uint32_t s = 0; s < 12; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (!mapped(p, 0x12a0)) continue;
        if (rd(p + 4) && rd(p + 0x10) == 0 && rd(p + 0x1ac) && rd(p + 0x94)) return p;
    }
    return 0;
}
}  // namespace

RX_PORT(0x0084ed20, DtorProbeHook, "thunk", 0) {
    if (!std::getenv("RX_DT_PROBE") || !std::strcmp(std::getenv("RX_DT_PROBE"), "none")) return reflex_vm::RUN_ORIGINAL;   // "none": the baseline run of a census
    static const uint32_t at = std::getenv("RX_DT_FRAME") ? uint32_t(std::atoi(std::getenv("RX_DT_FRAME"))) : 600;
    if (++g_frame != at) return reflex_vm::RUN_ORIGINAL;
    g_m = &m;
        const char* which = std::getenv("RX_DT_PROBE");
    if (!*which || *which == '1') which = "avrsdchfob";
    const uint32_t P = find_player();
    if (!P) { std::fprintf(stderr, "[dt] no local player at frame %u\n", g_frame); return reflex_vm::RUN_ORIGINAL; }
    const uint32_t A = rd(P + 0x94), V = rd(P + 0x1ac), R = V ? rd(V + 0x1a0) : 0;
    std::fprintf(stderr, "[dt] player %08x activity %08x (vt %08x) vehicle %08x (vt %08x) rider %08x (vt %08x)\n", P, A, rd(A), V, rd(V),
                 R, rd(R));
    const uint32_t one = 1, zero = 0;
    if (R) {
        const uint32_t ch = rd(R + 0x144), ex = ch ? rd(ch + 0x224) : 0, sub = ex ? rd(ex + 0x28) : 0;
        std::fprintf(stderr, "[dt] rider +0x144 %08x (vt %08x) +0x224 %08x (vt %08x) +0x28 %08x (vt %08x)\n", ch, rd(ch), ex, rd(ex), sub, rd(sub));
    }
    if (A) {
        // the scene's subscene records (scene = [world + 0x20138], records +0x38, 0x18 x 0x110: +0 name, +0x104 entity array, +0x108 count, +0x10c used)
        const uint32_t world = rd(A + 0x234), scene = world ? rd(world + 0x20138) : 0;
        std::fprintf(stderr, "[dt] activity +0xec8 %u type %u subscene name \"%s\"\n", rd(A + 0xec8), rd(A + 0x23c), mapped(A + 0x118, 8) ? (const char*)m.mem.page(A + 0x118 & ~0xfffu) + (A + 0x118 & 0xfffu) : "?");
        for (uint32_t k = 0; scene && k < 0x18; k++) {
            const uint32_t rec = scene + 0x38 + 0x110 * k;
            if (!rd(rec + 0x10c)) continue;
            std::fprintf(stderr, "[dt] subscene %u at %08x name \"%s\" entities %u\n", k, rec, (const char*)m.mem.page(rec & ~0xfffu) + (rec & 0xfffu), rd(rec + 0x108));
            for (uint32_t i = 0; i < rd(rec + 0x108); i++) { const uint32_t e = rd(rd(rec + 0x104) + 4 * i); 
                // the entity's release virtuals the area teardown calls: vf2 (removed, slot 2) and vf9 (slot 9), then vf0 (1); "-" = not ported
                const uint32_t vt = rd(e), f0 = rd(vt), f2 = rd(vt + 8), f9 = rd(vt + 0x24);
                std::fprintf(stderr, "[dt]   entity %08x vtable %08x sdtor %08x%s vf2 %08x%s vf9 %08x%s\n", e, vt, f0, rx_fn(f0) ? "" : " (-)", f2, rx_fn(f2) ? "" : " (-)", f9, rx_fn(f9) ? "" : " (-)"); }
        }
    }
    if (const char* peek = std::getenv("RX_DT_PEEK")) {   // RX_DT_PEEK=addr,addr: the vtable and slot 0 of objects at those addresses (hex)
        char* e = nullptr;
        for (const char* q = peek; *q;) { const uint32_t a = uint32_t(std::strtoul(q, &e, 16)); std::fprintf(stderr, "[dt] peek %08x vtable %08x sdtor %08x\n", a, rd(a), rd(rd(a))); q = *e ? e + 1 : e; }
    }
    if (std::strchr(which, 'a')) {
        const uint32_t vt = rd(A);
        if (vt == 0x0094329c || vt == 0x00943c14) {   // MXActivityFreeRide / MXActivityWaypointRace: vf3, vf6(0), vf0(1)
            with_snapshot(m, "activity vf3", [&] { thiscall(vslot(A, 0xc), A); });
            with_snapshot(m, "activity vf3 + vf6 + vf0(1)", [&] {
                thiscall(vslot(A, 0xc), A);
                thiscall(vslot(A, 0x18), A, 1, &zero);
                thiscall(vslot(A, 0), A, 1, &one);
            });
            with_snapshot(m, "activity vf0(0)", [&] { thiscall(vslot(A, 0), A, 1, &zero); });
            if (vt == 0x00943c14) with_snapshot(m, "Waypoint unlisten 0058a420 (EAX = channel)", [&] {
                rx_regs in = regs(), out = regs(); in.r[0] = rd(A + 0x290);
                rx_call_original_ex(0x0058a420, &in, &out, 1, &A); });
        }
        if (vt == 0x00943554 && !std::getenv("RX_DT_NO_RACE_AREA")) {   // MXActivityRace (vf3 00564c10, vf6, vf0 00564af0): the area teardown of a race
            with_snapshot(m, "race activity vf3 + vf6 + vf0(1)", [&] {
                thiscall(vslot(A, 0xc), A);
                thiscall(vslot(A, 0x18), A, 1, &zero);
                thiscall(vslot(A, 0), A, 1, &one);
            });
        }
        if (vt == 0x00943554) {   // a race activity: the base destructors on it (a different class: exercises the code)
            for (uint32_t fl : {0u, 1u}) {
                with_snapshot(m, "Activity::sdtor", [&] { thiscall(0x00556640, A, 1, &fl); });
                with_snapshot(m, "RaceStub::sdtor", [&] { thiscall(0x005659c0, A, 1, &fl); });
            }
        }
        if (vt == 0x0094329c) {
            with_snapshot(m, "FreeRide destroy 0055fee0 (ECX)", [&] { thiscall(0x0055fee0, A); });
            for (uint32_t fl : {0u, 1u}) {
                with_snapshot(m, "Activity::sdtor on the free ride", [&] { thiscall(0x00556640, A, 1, &fl); });
                with_snapshot(m, "FreeRide::sdtor", [&] { thiscall(0x0055fec0, A, 1, &fl); });
            }
        }
    }
    if (std::strchr(which, 'v') && V) {
        for (uint32_t fl : {0u, 1u}) {
            with_snapshot(m, "Motocross::sdtor", [&] { if (rd(V) == 0x0093cd84) thiscall(vslot(V, 0), V, 1, &fl); else std::fprintf(stderr, "[dt] vehicle vtable %08x is not Motocross\n", rd(V)); });
            with_snapshot(m, "RavenVehicle::sdtor on the vehicle", [&] { thiscall(0x0045d3a0, V, 1, &fl); });
        }
    }
    if (std::strchr(which, 'v') && V && rd(V + 0x16d4)) {
        const uint32_t sim = rd(V + 0x16d4);
        std::fprintf(stderr, "[dt] motocross simulation %08x (vt %08x)\n", sim, rd(sim));
        for (uint32_t fl : {0u, 1u}) with_snapshot(m, "RavenSimulationMotocross::sdtor", [&] { thiscall(0x0043b450, sim, 1, &fl); });
        with_snapshot(m, "RavenSimulationMotocross destruct 0043c040", [&] { thiscall(0x0043c040, sim); });
    }
    if (std::strchr(which, 'r') && R) {
        for (uint32_t fl : {0u, 1u})
            with_snapshot(m, "RiderAnimation::sdtor", [&] { if (rd(R) == 0x00958724) thiscall(vslot(R, 0), R, 1, &fl); else std::fprintf(stderr, "[dt] rider vtable %08x is not RiderAnimation\n", rd(R)); });
    }
    if (std::strchr(which, 'd') && R) {
        // the RiderAnimation chain's pieces directly (each on a snapshot), with the registers the original's callers set
        const uint32_t ch = rd(R + 0x144), ex = ch ? rd(ch + 0x224) : 0, ik = ex ? rd(ex + 0x28) : 0;
        const uint32_t chains = ik ? rd(ik + 0x64) : 0, nchain = chains ? rd(chains - 0x10) : 0;
        auto with_regs = [&](const char* what, uint32_t fn, int reg, uint32_t value) {
            with_snapshot(m, what, [&] { rx_regs in = regs(), out = regs(); in.r[reg] = value; rx_call_original_ex(fn, &in, &out, 0, nullptr); });
        };
        with_regs("clip slots clear 006b74d0 (EAX)", 0x006b74d0, 0, R + 0xc940);
        with_regs("clip slots clear tail 006b70b0 (ESI)", 0x006b70b0, 6, R + 0xc940);
        with_regs("pose job release 006372a0 (ESI)", 0x006372a0, 6, R + 0x5b54c);
        for (uint32_t off : {0xafdcu, 0xafbcu, 0xaf9cu, 0xaf7cu}) with_regs("name map release 00618c80 (ESI)", 0x00618c80, 6, R + off);
        with_regs("rider sound set 004245f0 (EAX)", 0x004245f0, 0, R + 0x5d0);
        if (ch) with_regs("character buffers 0082e040 (ESI)", 0x0082e040, 6, ch);
        with_regs("character entity 0082d100 (ECX)", 0x0082d100, 1, R);
        if (ik) with_regs("IK animator 006afcf0 (ECX)", 0x006afcf0, 1, ik);
        if (ex) with_regs("expression animator 006aca10 (ECX)", 0x006aca10, 1, ex);
        if (nchain) with_regs("IK chain 006afc00 (ECX)", 0x006afc00, 1, chains + 0x80 * (nchain - 1));
        // 00804e40 on made-up sound instances (state 0 / 1 / 3 / 2 without a channel)
        for (uint32_t st : {0u, 1u, 2u, 3u})
            with_snapshot(m, "sound instance stop 00804e40 (ESI)", [&] {
                const uint32_t inst = rx_sp() - 0x400;
                for (uint32_t o = 0; o < 0x200; o += 4) rx_wr32(inst + o, 0);
                rx_wr32(inst, st);
                rx_regs in = regs(), out = regs(); in.r[6] = inst;
                rx_call_original_ex(0x00804e40, &in, &out, 0, nullptr);
            });
    }
    if (std::strchr(which, 'c')) {
        // the vehicle's removal (vf9), camera entities (MXCameraEntity 0x95a35c / MXThirdPersonCamera 0x95a8a4: vf9, sdtor) and
        // AnimationEntity (0x97605c) vf9, on the live objects found by their vtables
        if (R && rd(R) == 0x00958724) with_snapshot(m, "RiderAnimation::vf9 (AnimationEntity::vf9)", [&] { thiscall(vslot(R, 0x24), R); });
        if (V && rd(V) == 0x0093cd84) with_snapshot(m, "Motocross::vf9", [&] { thiscall(vslot(V, 0x24), V); });
        for (uint32_t vt : {0x0095a35cu, 0x0095a8a4u, 0x0097605cu}) {
            std::vector<uint32_t> found;
            for (uint32_t pg = 0x2000; pg < (1u << 20) && found.size() < 3; pg++) {
                if (!m.mem.mapped(pg << 12)) continue;
                for (uint32_t o = 0; o < 4096 && found.size() < 3; o += 4) if (rx_rd32((pg << 12) + o) == vt) found.push_back((pg << 12) + o);
            }
            std::fprintf(stderr, "[dt] vtable %08x: %zu instance(s)\n", vt, found.size());
            for (uint32_t e : found) {
                with_snapshot(m, "entity vf9", [&] { thiscall(vslot(e, 0x24), e); });
                if (vt != 0x0097605c) for (uint32_t fl : {0u, 1u}) with_snapshot(m, "entity sdtor", [&] { thiscall(vslot(e, 0), e, 1, &fl); });
            }
        }
    }
    if (std::strchr(which, 's')) {
        // ScenarioAnimEntity instances (vtable 0x96cee4 at +0, 0x96cf64 at +0x90, 0x96cfa4 at +0x140)
        std::vector<uint32_t> found;
        for (uint32_t pg = 0; pg < (1u << 20) && found.size() < 4; pg++) {
            if (!m.mem.mapped(pg << 12)) continue;
            for (uint32_t o = 0; o < 4096; o += 4) {
                const uint32_t a = (pg << 12) + o;
                if (rx_rd32(a) == 0x0096cee4 && mapped(a + 0x140, 4) && rx_rd32(a + 0x90) == 0x0096cf64 && rx_rd32(a + 0x140) == 0x0096cfa4) found.push_back(a);
            }
        }
        std::fprintf(stderr, "[dt] %zu ScenarioAnimEntity instance(s)\n", found.size());
        for (uint32_t e : found) for (uint32_t fl : {0u, 1u})
            with_snapshot(m, "ScenarioAnimEntity::sdtor", [&] { thiscall(0x006b7720, e, 1, &fl); });
    }
    if (std::strchr(which, 'h')) {
        // the collision shapes: every class found in the shape registry (vector 0xd6d8e0: begin 0xd6d8ec, end 0xd6d8f0), up to
        // RX_DT_PER_CLASS (3) instances each, deleted through vf0 with the flags 0 and 1, with the allocator lock flag [0xda67d8]
        // as it is and flipped (the locked frees), then the shape handle (00849830) on a made-up handle: a shared shape (count
        // 2: only released) and the last reference
        const uint32_t b = rd(0x00d6d8ec), e = rd(0x00d6d8f0);
        std::map<uint32_t, std::vector<uint32_t>> classes;
        for (uint32_t p = b; p && p < e; p += 4) { const uint32_t sh = rd(p); if (sh && mapped(sh, 8)) classes[rd(sh)].push_back(sh); }
        const size_t per = std::getenv("RX_DT_PER_CLASS") ? size_t(std::atoi(std::getenv("RX_DT_PER_CLASS"))) : 3;
        std::fprintf(stderr, "[dt] %zu shapes in the registry, %zu classes\n", size_t((e - b) / 4), classes.size());
        for (auto& kv : classes) {
            std::fprintf(stderr, "[dt] shape class vtable %08x slot0 %08x: %zu instance(s), ref %u\n", kv.first, rd(kv.first), kv.second.size(), rd(kv.second[0] + 4));
            for (size_t i = 0; i < kv.second.size() && i < per; i++) {
                const uint32_t sh = kv.second[i];
                for (uint32_t fl : {0u, 1u}) for (uint32_t lock : {0u, 1u})
                    with_snapshot(m, "shape sdtor", [&] { rx_wr8(0x00da67d8, uint8_t(lock)); thiscall(rd(kv.first), sh, 1, &fl); });
            }
        }
        if (!classes.empty()) {
            const uint32_t sh = classes.begin()->second[0];
            for (uint32_t refs : {2u, 1u}) for (uint32_t fl : {0u, 1u})
                with_snapshot(m, "shape handle sdtor", [&] {
                    uint32_t h = 0;
                    if (fl) { rx_regs in = regs(), out = regs(); const uint32_t sz = 8; rx_call_original_ex(0x0078fbe0, &in, &out, 1, &sz); h = out.r[0]; }
                    else h = rx_sp() - 0x400;
                    rx_wr32(h, 0x009765b4); rx_wr32(h + 4, sh); rx_wr32(sh + 4, refs);
                    thiscall(0x00849830, h, 1, &fl);
                });
        }
    }
    if (std::strchr(which, 'k')) {
        // every entity class the PRODUCT can build (the core's factory, run here): its release virtuals vf0 (scalar deleting destructor), vf2
        // (removed) and vf9 (released), as the area teardown calls them; "(-)" = not a ported function (the product stops there). A class the
        // factory does not build (outside_core: it returns 0, or an id it aborts on) is reported as such.
        static const uint32_t ids[] = {2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x1001, 0x1002, 0x1003, 0x1004, 0x1005, 0x1006, 0x1007, 0x1008, 0x1009, 0x100a, 0x100b, 0x100c, 0x100d, 0x100e, 0x100f, 0x1010, 0x1011, 0x1012, 0x1013, 0x1014, 0x1015, 0x1016, 0x1017, 0x1018, 0x1019, 0x101a, 0x101b, 0x101c, 0x101d, 0x101e, 0x101f, 0x1020, 0x1021, 0x1022, 0x1023, 0x1024, 0x1025, 0x1026, 0x1027, 0x1028, 0x1029, 0x102a, 0x102b, 0x102c, 0x102d, 0x102e, 0x102f, 0x1030, 0x1031, 0x1032, 0x1033, 0x1034, 0x1035, 0x1036, 0x1037, 0x1038, 0x1039, 0x103a, 0x103b, 0x103c, 0x103d, 0x103e, 0x103f, 0x1040, 0x1041, 0x1042, 0x1043, 0x1044, 0x1045, 0x1046, 0x1047, 0x1048, 0x1049, 0x104a, 0x104b, 0x104c, 0x104d, 0x104e, 0x104f, 0x1050, 0x1051, 0x1052, 0x1053, 0x1054, 0x1055, 0x1056, 0x1057, 0x1058, 0x1059, 0x105a, 0x105b, 0x105c, 0x105d, 0x105e, 0x105f, 0x1060, 0x1061, 0x1062, 0x1063, 0x1064, 0x1065, 0x1066, 0x1067, 0x1068, 0x1069, 0x106a, 0x106b, 0x106c, 0x106d, 0x106e, 0x106f, 0x1070, 0x1071, 0x1072, 0x1073, 0x1074, 0x1075, 0x1076, 0x1077, 0x1078, 0x1079, 0x107a, 0x107b, 0x107c, 0x107d, 0x107e, 0x107f, 0x1080, 0x1081, 0x1082, 0x1083, 0x1084, 0x1085, 0x1086, 0x1087, 0x1088, 0x1089, 0x108a, 0x108b, 0x108c, 0x108d, 0x108e, 0x108f, 0x1090, 0x1091, 0x1092, 0x1093, 0x1094};
        static sigjmp_buf jb;
        for (uint32_t id : ids) {
            with_snapshot(m, "factory class", [&] {
                struct sigaction sa, old;
                std::memset(&sa, 0, sizeof sa);
                sa.sa_handler = [](int) { siglongjmp(jb, 1); };
                sigaction(SIGABRT, &sa, &old);
                if (sigsetjmp(jb, 1) == 0) {
                    const uint32_t e = rx_scene_create_entity(id);
                    if (!e) std::fprintf(stderr, "[dt] class id %#x: the factory builds no object (outside_core)\n", id);
                    else {
                        const uint32_t vt = rd(e), f0 = rd(vt), f2 = rd(vt + 8), f9 = rd(vt + 0x24);
                        std::fprintf(stderr, "[dt] class id %#x vtable %08x vf0 %08x%s vf2 %08x%s vf9 %08x%s\n", id, vt, f0, rx_fn(f0) ? "" : " (-)", f2, rx_fn(f2) ? "" : " (-)", f9, rx_fn(f9) ? "" : " (-)");
                    }
                } else std::fprintf(stderr, "[dt] class id %#x: the factory aborts (not built)\n", id);
                sigaction(SIGABRT, &old, nullptr);
            });
        }
    }
    if (std::strchr(which, 'o')) {
        // objects the core's factory builds (class ids RX_DT_IDS=hex,hex..; default the ported ones below): each one's scalar deleting
        // destructor (vf0) with the flags 0 / 1, its release vf9 and its removal vf2 -- the ORIGINAL entry points on a core-built object, so
        // Verify runs the ported function and then the original on the same fresh entity (classes the area subscenes of other levels hold
        // but the reference worlds do not: e.g. TriggeredPlaceableParticleEffect in the Supercross / FMX waypoint areas)
        std::vector<uint32_t> want;
        if (const char* l = std::getenv("RX_DT_IDS")) { char* e = nullptr; for (const char* q = l; *q;) { want.push_back(uint32_t(std::strtoul(q, &e, 16))); q = *e ? e + 1 : e; } }
        else want = {0x102f, 0x102e, 0x102a, 0x1028, 0x1076, 0x107c, 0x104b, 0x103b, 0x1052, 0x1059, 0x1030, 0xe, 0x101a, 0x3};
        static sigjmp_buf jb2;
        for (uint32_t id : want) {
            for (int what = 0; what < 4; what++) {   // 0 vf0(0), 1 vf0(1), 2 vf9, 3 vf2
                with_snapshot(m, "factory object", [&] {
                    struct sigaction sa, old;
                    std::memset(&sa, 0, sizeof sa);
                    sa.sa_handler = [](int) { siglongjmp(jb2, 1); };
                    sigaction(SIGABRT, &sa, &old);
                    if (sigsetjmp(jb2, 1) == 0) {
                        const uint32_t e = rx_scene_create_entity(id);
                        if (e) {
                            const uint32_t vt = rd(e), zero = 0, one = 1;
                            std::fprintf(stderr, "[dt] object id %#x %08x vtable %08x: %s\n", id, e, vt, what == 0 ? "vf0(0)" : what == 1 ? "vf0(1)" : what == 2 ? "vf9" : "vf2");
                            if (what == 0) thiscall(rd(vt), e, 1, &zero);
                            else if (what == 1) thiscall(rd(vt), e, 1, &one);
                            else if (what == 2) thiscall(rd(vt + 0x24), e);
                            else thiscall(rd(vt + 8), e);
                        }
                    }
                    sigaction(SIGABRT, &old, nullptr);
                });
            }
        }
    }
    if (std::strchr(which, 'b')) {
        // the live BillboardCrowdEntity objects of the world (vtable 0x00960654 / 0x009606cc, build_scene_crowd.c): all of them deleted in
        // turn with the scalar deleting destructor (flags 1), the original entry point; the last one frees the shared render event
        // (006c5c80), every one releases its shader objects. RX_DT_BILLBOARD_FIRST=<n> deletes only the first n (the event stays).
        std::vector<uint32_t> crowds;
        for (uint32_t pg = 0; pg < (1u << 20); pg++) if (m.mem.mapped(pg << 12))
            for (uint32_t o = 0; o < 0x1000; o += 16) { const uint32_t a = (pg << 12) + o; if (rd(a) == 0x00960654 && mapped(a + 0x90) && rd(a + 0x90) == 0x009606cc) crowds.push_back(a); }
        std::fprintf(stderr, "[dt] %zu live BillboardCrowd entities, shared event %08x count %u\n", crowds.size(), rd(0x00a942f4), rd(0x00a942f8));
        size_t n = crowds.size();
        if (const char* f = std::getenv("RX_DT_BILLBOARD_FIRST")) n = std::min<size_t>(n, size_t(std::atoi(f)));
        with_snapshot(m, "billboard crowds", [&] {
            /* the shader objects (+0x158 .. +0x16c) are the boot's Direct3D stand-ins: a world image has no device to answer their Release
             * (the call would leave the mapped code); they are cleared here, the Release itself is the plain vtable slot 2 call */
            for (size_t i = 0; i < n; i++) for (uint32_t k = 0x158; k <= 0x16c; k += 4) rx_wr32(crowds[i] + k, 0);
            for (size_t i = 0; i < n; i++) thiscall(rd(rd(crowds[i])), crowds[i], 1, &one);
            std::fprintf(stderr, "[dt] after %zu deletions: event %08x count %u\n", n, rd(0x00a942f4), rd(0x00a942f8));
        });
    }
    if (std::strchr(which, 'f')) {
        // shape classes the worlds do not hold, made up in guest memory (zeroed blocks from ReflexAlignedAlloc 0078fbe0, the layouts of
        // the destructors): StaticRays, Triangles (index block), Composite (children, vectors, child array), StaticComposite (pool block),
        // DynamicComposite (name strings); each with the flags 0 / 1 and the lock flag [0xda67d8] 0 / 1
        auto alloc = [&](uint32_t n) { rx_regs in = regs(), out = regs(); rx_call_original_ex(0x0078fbe0, &in, &out, 1, &n); for (uint32_t o = 0; o < n; o += 4) rx_wr32(out.r[0] + o, 0); return out.r[0]; };
        auto vector_record = [&](uint32_t v, bool with_block) {   // {+0 proxy block, +0xc begin, +0x10 end, +0x14 capacity end}
            rx_wr32(v, alloc(16));
            if (with_block) { const uint32_t blk = alloc(32); rx_wr32(v + 0xc, blk); rx_wr32(v + 0x10, blk + 16); rx_wr32(v + 0x14, blk + 32); }
        };
        auto composite_body = [&](uint32_t vt, uint32_t size) {
            const uint32_t sh = alloc(size);
            rx_wr32(sh, vt);
            rx_wr32(sh + 0x20, alloc(16));
            const uint32_t kids = alloc(16); rx_wr32(sh + 0x2c, kids); rx_wr32(sh + 0x30, kids + 12); rx_wr32(sh + 0x34, kids + 16);
            const uint32_t c = alloc(0x40); rx_wr32(c, 0x00976750); rx_wr32(c + 4, 1);   // a Circle child (deleted by vf0)
            rx_wr32(kids + 4, c);                                                         // child 0 and 2 stay null
            vector_record(sh + 0x38, true); vector_record(sh + 0x50, false); vector_record(sh + 0x68, true);
            return sh;
        };
        struct Case { const char* name; uint32_t fn; std::function<uint32_t()> make; };
        std::vector<Case> cases = {
            {"StaticRays", 0x00844f70, [&] { const uint32_t sh = alloc(0x40); rx_wr32(sh, 0x0097662c); rx_wr32(sh + 0x24, alloc(32)); return sh; }},
            {"Triangles", 0x00844a90, [&] { const uint32_t sh = alloc(0x40); rx_wr32(sh, 0x00976608); rx_wr32(sh + 0x24, alloc(32)); return sh; }},
            {"Composite", 0x00847c60, [&] { return composite_body(0x00976774, 0x90); }},
            {"StaticComposite", 0x00848ec0, [&] {
                const uint32_t sh = composite_body(0x009767a0, 0xb0), pool = sh + 0x80;
                rx_wr32(pool, alloc(16)); rx_wr32(pool + 0x18, alloc(16)); rx_wr32(pool + 0x24, alloc(16)); rx_wr32(pool + 0x28, rd(pool + 0x24) + 8); rx_wr32(pool + 0x2c, rd(pool + 0x24) + 8);
                rx_regs nin = regs(), nout = regs(); const uint32_t nsz = 32; rx_call_original_ex(0x78583b39, &nin, &nout, 1, &nsz);   // operator new: the pool's element block is allocator<>::allocate
                const uint32_t el = nout.r[0]; rx_wr32(pool + 0xc, el); rx_wr32(pool + 0x10, el + 16); rx_wr32(pool + 0x14, el + 32);
                return sh; }},
            {"DynamicComposite", 0x00849040, [&] {
                const uint32_t sh = composite_body(0x009767cc, 0xb0), names = sh + 0x80;
                rx_wr32(names, alloc(16));
                const uint32_t arr = alloc(0x38); rx_wr32(names + 0xc, arr); rx_wr32(names + 0x10, arr + 0x38); rx_wr32(names + 0x14, arr + 0x38);
                for (uint32_t k = 0; k < 2; k++) { rx_wr32(arr + 0x1c * k + 0x14, 0); rx_wr32(arr + 0x1c * k + 0x18, 0xf); }   // empty std::strings (small buffer)
                return sh; }},
        };
        for (auto& c : cases) for (uint32_t fl : {0u, 1u}) for (uint32_t lock : {0u, 1u})
            with_snapshot(m, c.name, [&] { rx_wr8(0x00da67d8, uint8_t(lock)); const uint32_t sh = c.make(); thiscall(c.fn, sh, 1, &fl); });
    }
    for (auto& kv : g_probe_calls) std::fprintf(stderr, "probe-call %08x %llu\n", kv.first, (unsigned long long)kv.second);
    std::fprintf(stderr, "[dt] frame %u done\n", g_frame);
    return reflex_vm::RUN_ORIGINAL;
}
