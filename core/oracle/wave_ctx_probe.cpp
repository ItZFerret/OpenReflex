// wave_ctx_probe.cpp -- the oracle's probe for the w-ctx wave (2026-10-26): the original functions that the core called through the
// macro / helper forms of rx_call_original (WORLD_CTX, pl_call, act_call_fn, tracked_call, ...) and that the five reference worlds
// never reach (development only; see core/plan/QUEUE.md "w-ctx"). Same device as wave_hidden_probe.cpp: a frame hook (Engine::Update
// 0084ed20, outside every verified call) calls the ORIGINAL addresses on the live game objects (or on fixtures in dead stack), each
// case on a snapshot of the whole guest memory and machine state that is restored afterwards, so with the ports installed Verify runs
// the core function and then the original on the same state and compares memory and the declared registers.
//   REFLEX_PORT=rx_world,rx_CtxProbeHook REFLEX_PORT_VERIFY=1 RX_CTX_PROBE=<cases, comma separated> [RX_CTX_FRAME=<n>]
// Cases:
//   savedpose 00460070 on the live vehicle: the saved-pose flag and quaternion over unit / scaled / non-finite values
//   water     0076dec0 (the water contact of a generic object) on fixture objects
//   keyed     0076e6c0 (the rider body vs keyed body impulse) on the live keyed bodies and fixture rider bodies
//   scripted  00548960 (the scripted wreck type) and its helpers 00548850 / 005488b0 over every combination of the seven key maps'
//             answers (the key maps' vtables are replaced by fixture vtables answering 0 or 1)
//   reset     00550cc0 (the reset by a "ResetPlayer" trigger) directly, and through the VehicleEntered handler 0054f0b0
//   eliminate 00512c80 (a driver eliminated) and 0053ac80 ("Player Eliminated") on the live drivers
//   stuntend  00623060 (the rider controller's PhysicsStuntEnd outputs) on fixture controllers
//   rotate    00500480 / 004ffe80 / 00765c90 / 00765ce0 (std::map red-black rotations) on random fixture trees
//   findentity 007d2c80 (the scene manager's entity lookup by subscene) on the live scenes
//   damage    0047a970 (a contact hits the vehicle's damage parts) with its callees 0047ba80 / 00691c30 / 00697b10 / 006971f0, on a fixture
//             behaviour whose parts are real nodes of the live vehicle's model; the part entities are null (0047a630 / 0047a830 do nothing there)
//   dmgstate  prints the vehicle damage manager (0xdb1410): behaviours with a vehicle, its type, its parts' states
//   spring    0047aff0 (a swinging part's spring) and 00681750 on the live damage entries (UTV / buggy / truck worlds)
//   exp       the core's crt_exp_x87 against the msvcr90 _CIexp 7859ac2c (x on the x87 stack) over random operands
//   partinfo  prints the damage parts' entities, body handles and shapes, the keyed vector / active / contact list counts (w-partdtor)
//   release   0047bbe0 for every vehicle of the damage manager (00479dd0 -> part vf0 00403bb0 / 0041c7b0 -> 00402360 -> 00416fe0 ...,
//             Database vf22 0084ce00, the shape handle vf1 00849630)
//   relentry  00479dd0 with every other part activated first (0047a630: the inlined 0047a830 path); vehclear 0054a500 on the local player
//             (-> 0047bbe0); RX_CTX_ACTIVE=1 activates every other part before the release case too
//   partmove  0047bc20 -> 0047a460 (the part bodies into the active scene's world; with the UI world: out and back)
//   keyedrm   the pieces one by one: 007685f0 / 0076a9a0 / 0076aa50 / 00417c80 / 00416fe0 (an entry of another world) / 00402360 /
//             00416f90 / 00849630 / 00403bb0 / Database vf22 on the live physics world and parts
//   drain     every vehicle released (0047bbe0), then Database vf33 0084cc80 on both parities (w-frametail: vf31 / vf32 0084af90 /
//             0084d9c0 / the handle destructors on the parts' real model handles; RX_CTX_REFS=1: every handle keeps a reference)
//   hsdtor    the three asset handle destructors 00668b20 / 005e15c0 / 007726b0 on fixture handles, flags 0 / 1
//   clock     00892120 (the game clock under its lock) directly, and 00892170 with a clock that would step backwards (may_step 0
//             retries through 00892120)
#include "oracle.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
using namespace reflex_port;

namespace {
uint32_t g_frame = 0;
reflex_vm::Machine* g_m = nullptr;
bool mapped(uint32_t a, uint32_t n = 4) { return g_m && a && g_m->mem.mapped(a) && g_m->mem.mapped(a + n - 1); }
uint32_t rd(uint32_t a) { return mapped(a) ? rx_rd32(a) : 0; }
uint32_t rd16(uint32_t a) { return mapped(a, 2) ? rx_rd16(a) : 0; }
void wr(uint32_t a, uint32_t v) { if (mapped(a)) rx_wr32(a, v); }
uint32_t fb(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
float fl(uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; }
void wf(uint32_t a, float f) { wr(a, fb(f)); }

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

struct World {
    uint32_t game = 0, player = 0, vehicle = 0, scratch = 0;
};

World find_world(reflex_vm::Machine& m) {
    World w;
    w.game = rd(0x00a946b8);
    w.scratch = (m.r[4] - 0x8000) & ~0xfu;   // dead stack: the probe's scratch area
    if (!w.game) return w;
    for (uint32_t s = 0; s < 16; s++) {
        const uint32_t p = w.game + 0x7460 + 0x12a0 * s;
        if (!mapped(p, 0x12a0)) continue;
        if (rd(p + 4) == 0 || rd(p + 0x10) != 0) continue;   // a local human
        w.player = p;
        w.vehicle = rd(p + 0x1ac);
        if (w.vehicle) break;
    }
    return w;
}

rx_regs regs() { rx_regs r; std::memset(&r, 0, sizeof r); return r; }
rx_regs call_ex(uint32_t fn, const rx_regs& in, int n = 0, const uint32_t* args = nullptr) {
    rx_regs out = regs();
    rx_call_original_ex(fn, &in, &out, n, args);
    return out;
}
void say(const char* what) { std::fprintf(stderr, "[ctx] %s\n", what); }

uint32_t physics_world(const World& w) {
    for (uint32_t off : {0x5a54u, 0x20c8u}) {
        const uint32_t level = rd(w.game + off);
        if (!mapped(level + 0x20134, 4)) continue;
        const uint32_t world = rd(level + 0x20134);
        if (mapped(world, 0xcce0)) return world;
    }
    return 0;
}

uint32_t g_lcg = 12345;
uint32_t rnd() { g_lcg = g_lcg * 1664525u + 1013904223u; return g_lcg >> 8; }
float rf(float lo, float hi) { return lo + (hi - lo) * float(rnd() % 100000) / 100000.0f; }
bool skip(const char* what) { const char* e = std::getenv("RX_CTX_SKIP"); return e && std::strstr(e, what); }

// ---- 00460070 ----------------------------------------------------------------------------------------------------------------------------
void case_savedpose(World& w) {
    say("savedpose");
    const uint32_t v = w.vehicle;
    if (!v) { say("savedpose: no vehicle"); return; }
    struct Q { float x, y, z, w; };
    // A NaN that reaches 0045f600 goes through the emulator's x87 (fld / fstp), which stores a payload-less NaN where the hardware (and
    // the core) keep the payload: those fixtures are behind RX_CTX_NAN=1 (documented artifact, as RX_HID_NAN in wave_hidden_probe.cpp).
    const bool with_nan = std::getenv("RX_CTX_NAN") != nullptr;
    const float nan = std::nanf(""), inf = INFINITY;
    std::vector<Q> qs = {{0, 0, 0, 1}, {0, 0.7071068f, 0, 0.7071068f}, {0.5f, 0.5f, 0.5f, 0.5f}, {0.1f, -0.2f, 0.3f, 0.9f}, {0, 0, 0, 0},
                         {0.1f, 0.2f, 0.3f, 0.4f}, {1, 2, 3, 4}, {with_nan ? nan : 0.5f, 0, 0, 1}, {0, with_nan ? inf : 4.0f, 0, 1}, {0, 0, 0.3f, with_nan ? nan : 0.25f}, {-0.0f, -0.0f, -0.0f, -1.0f},
                         {1e-30f, with_nan ? 1e30f : 1e10f, 0, 1}, {0.3f, 0.3f, 0.3f, 0.7f}};
    for (int i = 0; i < 40; i++) qs.push_back(Q{rf(-1, 1), rf(-1, 1), rf(-1, 1), rf(-1, 1)});
    for (int i = 0; i < 20; i++) {   // normalized random rotations
        Q q{rf(-1, 1), rf(-1, 1), rf(-1, 1), rf(-1, 1)};
        const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
        if (n > 0.01f) qs.push_back(Q{q.x / n, q.y / n, q.z / n, q.w / n});
    }
    int count = 0;
    for (const Q& q : qs) for (int pos = 0; pos < 3; pos++) for (uint32_t flag : {1u, 0u, 7u}) {
        wr(v + 0x180, flag);
        wf(v + 0x210, q.x); wf(v + 0x214, q.y); wf(v + 0x218, q.z); wf(v + 0x21c, q.w);
        if (pos == 0) { wf(v + 0x200, 10.0f); wf(v + 0x204, 2.0f); wf(v + 0x208, -30.0f); wf(v + 0x20c, 1.0f); }
        else if (pos == 1) { wf(v + 0x200, rf(-500, 500)); wf(v + 0x204, rf(-50, 50)); wf(v + 0x208, rf(-500, 500)); wf(v + 0x20c, 0.0f); }
        else { wf(v + 0x200, 20000.0f); wf(v + 0x204, 0.0f); wf(v + 0x208, 0.0f); wf(v + 0x20c, with_nan ? nan : 3.0f); }
        rx_regs in = regs(); in.r[0] = v; in.r[3] = v; in.r[6] = rd(v + 0x16d4);
        call_ex(0x00460070, in);
        ++count;
    }
    std::fprintf(stderr, "[ctx] savedpose: %d calls (vehicle %08x)\n", count, v);
}

// ---- 0076dec0 ----------------------------------------------------------------------------------------------------------------------------
// a generic dynamic object (0xe0 bytes) as a fixture: +0 mass, +0x10.. inertia, +0x24 material id, +0x40 centre, +0x50 orientation,
// +0x60 velocity, +0x70 angular velocity, +0x80 / +0x90 / +0xa0 the contact accumulators
void fill_body(uint32_t b, uint32_t material, bool heavy) {
    for (uint32_t k = 0; k < 0xe0; k += 4) wr(b + k, 0);
    wf(b + 0, heavy ? rf(50, 500) : rf(0.5f, 5));
    wf(b + 0x10, rf(0.5f, 40)); wf(b + 0x14, rf(0.5f, 40)); wf(b + 0x18, rf(0.5f, 40)); wf(b + 0x1c, 1.0f);
    wr(b + 0x24, material);
    wf(b + 0x40, rf(-50, 50)); wf(b + 0x44, rf(-5, 5)); wf(b + 0x48, rf(-50, 50)); wf(b + 0x4c, 0.0f);
    float x, y, z, q, n;
    do { x = rf(-1, 1); y = rf(-1, 1); z = rf(-1, 1); q = rf(-1, 1); n = std::sqrt(x * x + y * y + z * z + q * q); } while (n < 0.05f);
    wf(b + 0x50, x / n); wf(b + 0x54, y / n); wf(b + 0x58, z / n); wf(b + 0x5c, q / n);
    wf(b + 0x60, rf(-20, 20)); wf(b + 0x64, rf(-20, 20)); wf(b + 0x68, rf(-20, 20)); wf(b + 0x6c, 0.0f);
    wf(b + 0x70, rf(-8, 8)); wf(b + 0x74, rf(-8, 8)); wf(b + 0x78, rf(-8, 8)); wf(b + 0x7c, 0.0f);
    wf(b + 0x80, rf(-1, 1)); wf(b + 0x84, rf(-1, 1)); wf(b + 0x88, rf(-1, 1)); wf(b + 0x8c, 0.0f);
    wf(b + 0x90, rf(-1, 1)); wf(b + 0x94, rf(-1, 1)); wf(b + 0x98, rf(-1, 1)); wf(b + 0x9c, 0.0f);
}
void fill_contact(uint32_t c, int kind) {
    wf(c, rf(-50, 50)); wf(c + 4, rf(-5, 5)); wf(c + 8, rf(-50, 50)); wf(c + 12, 0.0f);
    float nx = rf(-1, 1), ny = rf(-1, 1), nz = rf(-1, 1);
    if (kind == 1) { nx = 0; ny = 1; nz = 0; }            // straight up
    if (kind == 2) { nx = 0; ny = 0; nz = 0; }            // degenerate
    if (kind == 3) { nx = 0.6f; ny = 0; nz = 0.8f; }
    const float n = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (n > 0) { nx /= n; ny /= n; nz /= n; }
    wf(c + 0x10, nx); wf(c + 0x14, ny); wf(c + 0x18, nz); wf(c + 0x1c, 0.0f);
}

void case_water(World& w) {
    say("water");
    const uint32_t world = physics_world(w);
    if (!world) { say("water: no physics world"); return; }
    const uint32_t A = w.scratch + 0x100, contact = w.scratch + 0x200;
    const uint32_t water_material = rd(world + 0xccac);
    int count = 0;
    for (uint32_t material : {water_material, 0u, 1u, 2u}) for (int kind = 0; kind < 4; kind++) for (int rep = 0; rep < 12; rep++) {
        fill_body(A, material, rep & 1);
        fill_contact(contact, kind);
        if (rep == 5) { wf(A + 0x60, 0); wf(A + 0x64, 0); wf(A + 0x68, 0); wf(A + 0x70, 0); wf(A + 0x74, 0); wf(A + 0x78, 0); }
        if (rep == 6) { wf(A + 0x60, -9); wf(A + 0x64, -9); wf(A + 0x68, 0); }
        rx_regs in = regs(); in.r[1] = world; in.r[7] = A; in.r[6] = world;
        uint32_t a[1] = {contact};
        call_ex(0x0076dec0, in, 1, a);
        ++count;
    }
    std::fprintf(stderr, "[ctx] water: %d calls\n", count);
}

// ---- 0076e6c0 ----------------------------------------------------------------------------------------------------------------------------
void case_keyed(World& w) {
    say("keyed");
    const uint32_t world = physics_world(w);
    if (!world) { say("keyed: no physics world"); return; }
    const uint32_t begin = rd(world + 0x19a0), end = rd(world + 0x19a4);
    std::vector<uint32_t> keyed;
    for (uint32_t o = begin; o < end && mapped(o, 0xe0) && keyed.size() < 24; o += 0xe0) keyed.push_back(o);
    std::fprintf(stderr, "[ctx] keyed: %zu live keyed bodies\n", keyed.size());
    if (keyed.empty()) { say("keyed: none"); return; }
    const uint32_t R = w.scratch + 0x100, contact = w.scratch + 0x200;
    int count = 0;
    for (uint32_t K : keyed) {
        const uint32_t saved_state = rd(K + 0x84);
        const int32_t index = int32_t(rd(K + 0x80));
        // states: as is, integrating (bit 1), asleep (bits 1 clear) with and without the pushable bit 5
        const uint32_t states[] = {saved_state, saved_state | 2u, saved_state & ~2u & ~0x20u, (saved_state & ~2u) | 0x20u, (saved_state & ~0x22u)};
        for (uint32_t st : states) for (int kind = 0; kind < 3; kind++) for (int speed = 0; speed < 4; speed++) {
            // an asleep keyed body goes through Kinematic_Activate, which needs a valid body (inactive: +0x80 < 0) or an active one
            if ((st & 2u) == 0 && (st & 0x20u) == 0 && index >= 200) continue;
            if ((st & 2u) && (index < 0 || index >= 200)) continue;   // an integrating body has a kinematic pose slot
            fill_body(R, rd(K + 0x24) % 3, speed & 1);
            fill_contact(contact, kind);
            const float s = speed == 0 ? 0.0f : speed == 1 ? 0.5f : speed == 2 ? 5.0f : 60.0f;
            wf(R + 0x60, s); wf(R + 0x64, s * 0.5f); wf(R + 0x68, -s);
            wr(K + 0x84, st);
            rx_regs in = regs(); in.r[1] = contact; in.r[2] = K; in.r[3] = R; in.r[6] = K; in.r[7] = world;
            uint32_t a[2] = {world, R};
            call_ex(0x0076e6c0, in, 2, a);
            ++count;
        }
        wr(K + 0x84, saved_state);
    }
    std::fprintf(stderr, "[ctx] keyed: %d calls\n", count);
}

// ---- 00548960 / 00548850 / 005488b0 ------------------------------------------------------------------------------------------------------
void case_scripted(World& w) {
    say("scripted");
    const uint32_t k_yes = 0x0055ec8c, k_no = 0x0055ec94;   // mov eax, 1; ret 8  /  xor eax, eax; ret 8
    const uint32_t vt_yes = w.scratch + 0x100, vt_no = w.scratch + 0x140;
    for (uint32_t i = 0; i < 16; i += 4) { wr(vt_yes + i, k_yes); wr(vt_no + i, k_no); }
    const uint32_t input_offsets[7] = {0x4f3c, 0x4fe0, 0x5084, 0x2ba0, 0x2fa0, 0x31a0, 0x2da0};
    int count = 0;
    for (uint32_t slot = 0; slot < 2; slot++) {
        const uint32_t input = rd(0x009c52c4u + 4 * slot);
        if (!input) continue;
        for (uint32_t mask = 0; mask < 128; mask++) {
            for (int k = 0; k < 7; k++) wr(input + input_offsets[k], (mask >> k) & 1 ? vt_yes : vt_no);
            { rx_regs in = regs(); in.r[6] = slot; call_ex(0x00548960, in); ++count; }
            { rx_regs in = regs(); in.r[0] = slot; call_ex(0x00548850, in); ++count; }
            { rx_regs in = regs(); in.r[0] = slot; call_ex(0x005488b0, in); ++count; }
        }
    }
    std::fprintf(stderr, "[ctx] scripted: %d calls\n", count);
}

// ---- 00892120 ----------------------------------------------------------------------------------------------------------------------------
void case_clock(World& w) {
    say("clock");
    const uint32_t out = w.scratch + 0x100;
    int count = 0;
    for (uint32_t flag : {0u, 1u}) for (int rep = 0; rep < 3; rep++) {
        const uint32_t saved_flag = rd(0x009df580);
        wr(0x009df580, flag);
        wr(out, 0xdeadbeef); wr(out + 4, 0xdeadbeef);
        uint32_t a[1] = {out};
        call_ex(0x00892120, regs(), 1, a);
        ++count;
        wr(0x009df580, saved_flag);
    }
    // 00892170 with the last value returned (0x9dfaa8, int64) ahead of the clock: the value would go backwards
    for (uint32_t may_step : {0u, 1u}) for (uint32_t flag : {0u, 1u}) for (uint32_t ahead : {1u, 5000u, 0x7fffffffu}) {
        const uint32_t now = rd(0x009dfaa8), saved_flag = rd(0x009df580);
        wr(0x009dfaa8, now + ahead); wr(0x009dfaac, 0);
        wr(0x009df580, flag);
        wr(out, 0xdeadbeef); wr(out + 4, 0xdeadbeef);
        uint32_t a[2] = {out, may_step};
        call_ex(0x00892170, regs(), 2, a);
        ++count;
        wr(0x009df580, saved_flag);
    }
    std::fprintf(stderr, "[ctx] clock: %d calls\n", count);
}

// ---- 00550cc0 / 0054f0b0 ------------------------------------------------------------------------------------------------------------------
void case_reset(World& w) {
    say("reset");
    const uint32_t p = w.player;
    if (!p) { say("reset: no player"); return; }
    const uint32_t trigger = w.scratch + 0x200;   // a fixture trigger entity: the name at +0xc0
    const char* name = "ResetPlayer";
    for (uint32_t i = 0; i < 0x100; i += 4) wr(trigger + i, 0);
    for (uint32_t i = 0; name[i]; i++) rx_wr8(trigger + 0xc0 + i, uint8_t(name[i]));
    rx_wr8(trigger + 0xc0 + 11, 0);
    int count = 0;
    const float nan = std::nanf("");
    for (float timer : {0.0f, -1.0f, 0.5f, 3.0f, nan}) for (uint32_t mode : {0u, 1u, 2u}) {
        wf(p + 0x116c, timer);
        const uint32_t saved_mode = rd(0x00a942b4);
        wr(0x00a942b4, mode);
        { rx_regs in = regs(); in.r[6] = p; call_ex(0x00550cc0, in); ++count; }
        uint32_t a[2] = {rd(p + 0x1ac), trigger};
        { rx_regs in = regs(); in.r[1] = p; call_ex(0x0054f0b0, in, 2, a); ++count; }
        wr(0x00a942b4, saved_mode);
    }
    std::fprintf(stderr, "[ctx] reset: %d calls\n", count);
}

// ---- 00512c80 / 0053ac80 ------------------------------------------------------------------------------------------------------------------
void case_eliminate(World& w) {
    say("eliminate");
    const uint32_t act = rd(w.player + 0x94);
    if (!act || !mapped(act, 0x300)) { say("eliminate: no activity"); return; }
    const uint32_t drivers = rd(act + 0x268), count = rd(act + 0x26c);
    std::fprintf(stderr, "[ctx] eliminate: activity %08x, %u drivers\n", act, count);
    int calls = 0;
    for (uint32_t i = 0; i < count && i < 4; i++) {
        const uint32_t d = rd(drivers + 4 * i);
        if (!d || !mapped(d, 0x1200)) continue;
        for (uint32_t remaining : {0u, 1u, 7u, 0xffffffffu}) {
            rx_regs in = regs(); in.r[0] = remaining; in.r[1] = d; in.r[6] = 0x1234;
            call_ex(0x00512c80, in);
            ++calls;
        }
        { rx_regs in = regs(); in.r[6] = rd(act + 0x290); in.r[1] = d; in.r[2] = rd(act + 0x2d0); uint32_t a[1] = {act}; call_ex(0x0053ac80, in, 1, a); ++calls; }
    }
    std::fprintf(stderr, "[ctx] eliminate: %d calls\n", calls);
}

// ---- 00623060 ----------------------------------------------------------------------------------------------------------------------------
void case_stuntend(World& w) {
    say("stuntend");
    const uint32_t e = w.scratch + 0x100, flag = w.scratch + 0x1000, word = w.scratch + 0x1010, cell = w.scratch + 0x1020;
    const float nan = std::nanf("");
    const float vals[] = {-5.0f, -0.0f, 0.0f, 0.25f, 3.0f, nan, -INFINITY, INFINITY};
    int count = 0;
    for (float upper : vals) for (float lower : vals) for (int nulls = 0; nulls < 8; nulls++) {
        for (uint32_t k = 0; k < 0x600; k += 4) wr(e + k, 0);
        wr(e + 0x564, (nulls & 1) ? 0 : flag); wr(e + 0x568, (nulls & 2) ? 0 : word); wr(e + 0x58c, (nulls & 4) ? 0 : cell);
        wr(e + 0x5ac, 0xabcd1234u); wf(e + 0x588, upper); wf(e + 0x584, lower);
        wr(flag, 0xaaaaaaaau); wr(word, 0xbbbbbbbbu); wr(cell, 0xccccccccu);
        rx_regs in = regs(); in.r[0] = e; in.r[7] = 3; in.r[6] = w.player;
        call_ex(0x00623060, in);
        ++count;
    }
    std::fprintf(stderr, "[ctx] stuntend: %d calls\n", count);
}

// ---- the std::map rotations ----------------------------------------------------------------------------------------------------------
void case_rotate(World& w) {
    say("rotate");
    struct Kind { uint32_t left, right, nil, node_size; };
    const Kind kinds[2] = {{0x00500480, 0x004ffe80, 0x39, 0x3c}, {0x00765c90, 0x00765ce0, 0x1d, 0x20}};
    int count = 0;
    for (const Kind& k : kinds) for (int round = 0; round < 6; round++) {
        const uint32_t tree = w.scratch + 0x100, nodes = w.scratch + 0x1000;
        const uint32_t head = nodes;                       // node 0 is the head (nil flag set)
        for (uint32_t i = 0; i < 24 * k.node_size; i += 4) wr(nodes + i, 0);
        wr(tree + 0x18, head);
        rx_wr8(head + k.nil, 1);
        // a random binary tree of 12 nodes: node i (1..12) is inserted below an existing node
        uint32_t root = 0;
        const int n = 12;
        for (int i = 1; i <= n; i++) {
            const uint32_t node = nodes + i * k.node_size;
            wr(node, head); wr(node + 8, head); wr(node + 4, head);
            if (!root) { root = node; wr(head + 4, root); wr(node + 4, head); continue; }
            uint32_t cur = root;
            for (;;) {
                const bool go_left = (rnd() & 1) != 0;
                const uint32_t child = rd(cur + (go_left ? 0 : 8));
                if (child == head) { wr(cur + (go_left ? 0 : 8), node); wr(node + 4, cur); break; }
                cur = child;
            }
        }
        wr(head, nodes + 1 * k.node_size); wr(head + 8, nodes + n * k.node_size);   // (leftmost / rightmost: not read by the rotations)
        for (int step = 0; step < 80; step++) {
            const uint32_t x = nodes + (1 + rnd() % n) * k.node_size;
            const bool do_left = (rnd() & 1) != 0;
            const uint32_t child = rd(x + (do_left ? 8 : 0));
            if (child == head) continue;                        // nothing to rotate up
            rx_regs in = regs(); in.r[1] = x;
            uint32_t a[1] = {tree};
            call_ex(do_left ? k.left : k.right, in, 1, a);
            ++count;
        }
    }
    std::fprintf(stderr, "[ctx] rotate: %d calls\n", count);
}

// ---- 007d2c80 ----------------------------------------------------------------------------------------------------------------------------
void case_findentity(World& w) {
    say("findentity");
    const uint32_t ctx = rd(0x00a95844);
    const uint32_t none = w.scratch + 0x100;
    const char* missing = "NoSuchEntityName";
    for (uint32_t i = 0; missing[i]; i++) rx_wr8(none + i, uint8_t(missing[i]));
    rx_wr8(none + 16, 0);
    int count = 0, hits = 0;
    for (uint32_t si = 0; si < 3; si++) {
        const uint32_t scene = rd(ctx + 0x1c + 4 * si);
        if (!scene || !mapped(scene, 0x2d20)) continue;
        for (uint32_t b = 0; b < 0x18; b++) {
            const uint32_t rec = scene + 0x38 + 0x110 * b;
            if (!rd(rec + 0x10c)) continue;
            const uint32_t n = rd(rec + 0x108), arr = rd(rec + 0x104);
            for (uint32_t k = 0; k < n && k < 4; k++) {
                const uint32_t ent = rd(arr + 4 * k);
                const uint32_t name = ent ? rd(ent + 0x18) : 0;
                if (!name) continue;
                for (uint32_t sub : {rec, 0u, none}) {
                    uint32_t a[3] = {scene, name, sub};
                    const uint32_t r = call_ex(0x007d2c80, regs(), 3, a).r[0];
                    ++count; if (r) ++hits;
                }
            }
        }
        { uint32_t a[3] = {scene, none, 0}; call_ex(0x007d2c80, regs(), 3, a); ++count; }
    }
    std::fprintf(stderr, "[ctx] findentity: %d calls, %d found\n", count, hits);
}

// ---- 0047a970 --------------------------------------------------------------------------------------------------------------------------
std::string node_name(uint32_t rec) {
    std::string r;
    if (!rec || rd16(rec + 8) == 0xffff) return r;
    const uint32_t owner = rd(rec), p = rd(owner + 0x10) + rd16(rec + 8);
    for (uint32_t i = 0; i < 0x80 && rx_rd8(p + i); i++) r.push_back(char(rx_rd8(p + i)));
    return r;
}
void case_damage(World& w) {
    say("damage");
    const uint32_t V = w.vehicle;
    if (!V) { say("damage: no vehicle"); return; }
    const uint32_t model = rd(V + 0x1328), hier = model + 0x10, records = rd(hier + 8), count = rd(hier + 0xc);
    std::vector<uint32_t> named;
    for (uint32_t i = 0; i < count && i < 200; i++) { const uint32_t r = records + 16 * i; if (!node_name(r).empty()) named.push_back(r); }
    std::fprintf(stderr, "[ctx] damage: %zu named nodes of the vehicle model\n", named.size());
    if (named.size() < 4) { say("damage: too few nodes"); return; }
    for (uint32_t set = 0x00c11e90; int32_t(set) < int32_t(0x00d2f0d0); set += 0x17c30) {   // diagnostics: the effect sets bound to a vehicle
        if (!rd(set)) continue;
        std::fprintf(stderr, "[ctx] damage: effect set %08x vehicle %08x%s manager %08x records %u ids:", set, rd(set), rd(set) == V ? " (the live one)" : "", rd(set + 0x220), rd(set + 0x224));
        for (uint32_t j = 0; j < rd(set + 0x224) && j < 24; j++) std::fprintf(stderr, " %u", rd(set + 0x230 + 0x3f0 * j + 0x6c));
        std::fprintf(stderr, "\n");
    }
    // the MX bike's records all have effect id 0: give them ids 0 / 1 / 2, thresholds and offsets so that the hit effects find records (restored with the snapshot)
    for (uint32_t set = 0x00c11e90; int32_t(set) < int32_t(0x00d2f0d0); set += 0x17c30) {
        if (rd(set) != V) continue;
        for (uint32_t j = 0; j < rd(set + 0x224) && j < 24; j++) {
            const uint32_t rec = set + 0x230 + 0x3f0 * j;
            wr(rec + 0x6c, j % 3);
            rx_wr8(rec + 0x85, uint8_t((j % 4) == 1 || (j % 5) == 2)); rx_wr8(rec + 0x84, uint8_t(j & 1));
            wf(rec + 0x80, (j % 2) ? 2.0f * float(j) : 400.0f);
            wf(rec + 0xd0, rf(-1, 1)); wf(rec + 0xd4, rf(-1, 1)); wf(rec + 0xd8, rf(-1, 1));
        }
    }
    const uint32_t B = w.scratch + 0x1000, names = w.scratch + 0x2000, ev = w.scratch + 0x100, strs = w.scratch + 0x3000;
    const char* missing = "NoSuchNodeAnywhere";
    for (uint32_t i = 0; missing[i]; i++) rx_wr8(strs + 0x100 + i, uint8_t(missing[i]));
    rx_wr8(strs + 0x100 + 18, 0);
    int count_calls = 0;
    struct Case { uint32_t state, hits; };
    const Case cases[] = {{0, 0}, {0, 0x20}, {0, 0x28}, {0, 0xf0}, {1, 0}, {1, 0x7a}, {1, 0x90}, {2, 0xcf}, {2, 0x40}, {2, 0xe0}, {3, 0xff}, {0, 0x10}};
    const float strengths[] = {2.0f, 5.0f, 15.0f, 30.0f, 50.0f, std::nanf(""), 3.0f, 10.0f};
    uint32_t pos[3];
    for (int k = 0; k < 3; k++) pos[k] = rd(V + 0xc0 + 4 * k);
    for (const Case& c : cases) for (float strength : strengths) for (int variant = 0; variant < 4; variant++) {
        for (uint32_t k = 0; k < 0x410; k += 4) wr(B + k, 0);
        const uint32_t n = 3;
        wr(B, n); wr(B + 4, names); wr(B + 0x3fc, V);
        for (uint32_t i = 0; i < n; i++) {
            const uint32_t rec1 = named[1 + i], rec2 = named[(2 + i) % named.size()];
            const std::string n1 = node_name(rec1), n2 = (variant == 1) ? std::string(missing) : node_name(rec2);
            for (uint32_t k = 0; k < 0x350; k += 4) wr(names + 0x350 * i + k, 0);
            for (size_t j = 0; j <= n1.size(); j++) rx_wr8(names + 0x350 * i + j, j < n1.size() ? uint8_t(n1[j]) : 0);
            for (size_t j = 0; j <= n2.size(); j++) rx_wr8(names + 0x350 * i + 0x104 + j, j < n2.size() ? uint8_t(n2[j]) : 0);
            wr(B + 0x370 + 4 * i, i == 2 && variant == 2 ? 0 : rec1);
            wr(B + 0xc + 4 * i, c.state); wr(B + 0x110 + 4 * i, c.hits + 7 * i);
            wr(B + 0x3b0 + 4 * i, i == 1 ? 1 : 2);
        }
        for (uint32_t k = 0; k < 0x40; k += 4) wr(ev + k, 0);
        wr(ev + 0x10, pos[0]); wr(ev + 0x14, pos[1]); wr(ev + 0x18, pos[2]);
        wf(ev + 0x10, fl(pos[0]) + 0.3f);
        wf(ev + 0x24, strength); wr(ev + 0x20, variant == 2 ? 1 : 0);
        rx_regs in = regs(); in.r[1] = ev; in.r[7] = ev; in.r[3] = B;
        uint32_t a[1] = {B};
        call_ex(0x0047a970, in, 1, a);
        ++count_calls;
        if (variant != 3) { call_ex(0x0047a880, regs(), 1, a); ++count_calls; }   // the parts put back (on the state the hit left)
    }
    // the hit-point counter and the emission on their own
    for (float st : {-1.0f, 3.0f, 3.5f, 10.0f, 12.0f, 25.0f, 26.0f, 35.0f, 36.0f, 1e9f, std::nanf("")}) for (uint32_t idx : {0u, 1u, 7u}) {
        for (uint32_t k = 0; k < 0x410; k += 4) wr(B + k, 0);
        wr(B + 0x110 + 4 * idx, 5);
        rx_regs in = regs(); in.r[1] = B; in.r[0] = idx; in.xmm[0].v[0] = st;
        call_ex(0x0047ba80, in);
        ++count_calls;
    }
    {
        const uint32_t q = w.scratch + 0x400, off = w.scratch + 0x440, p = w.scratch + 0x460;
        const float nan = std::nanf("");
        for (int round = 0; round < 24; round++) {
            float x, y, z, ww, n2;
            do { x = rf(-1, 1); y = rf(-1, 1); z = rf(-1, 1); ww = rf(-1, 1); n2 = std::sqrt(x * x + y * y + z * z + ww * ww); } while (n2 < 0.05f);
            wf(q, x / n2); wf(q + 4, y / n2); wf(q + 8, z / n2); wf(q + 12, ww / n2);
            wf(off, rf(-3, 3)); wf(off + 4, rf(-3, 3)); wf(off + 8, rf(-3, 3));
            wf(p, round == 5 ? nan : rf(-50, 50)); wf(p + 4, rf(-5, 5)); wf(p + 8, rf(-50, 50));
            uint32_t a[6] = {V, uint32_t(1 + (round % 3)), p, q, off, fb(rf(0.0f, 60.0f))};
            call_ex(0x00691c30, regs(), 6, a);
            ++count_calls;
        }
    }
    std::fprintf(stderr, "[ctx] damage: %d calls\n", count_calls);
}

void case_dmgstate(World& w) {
    { const uint32_t db = rd(rd(0x00a95844) + 8); std::fprintf(stderr, "[ctx] database %08x vtable %08x\n", db, rd(db)); }
    say("dmgstate");
    const uint32_t mgr = 0x00db1410;
    std::fprintf(stderr, "[ctx] dmg player vehicle %08x type %u\n", w.vehicle, w.vehicle ? rd(w.vehicle + 0x194) : 99);
    for (uint32_t k = 0; k < 12; k++) {
        const uint32_t b = mgr + 0x10 + 0x410 * k;
        const uint32_t v = rd(b + 0x3fc);
        std::fprintf(stderr, "[ctx] dmg %u: parts %u vehicle %08x type %u names %08x table %08x states", k, rd(b), v, v ? rd(v + 0x194) : 99, rd(b + 4), rd(b + 8));
        for (uint32_t i = 0; i < rd(b) && i < 8; i++) std::fprintf(stderr, " %u", rd(b + 0xc + 4 * i));
        std::fprintf(stderr, " entities");
        for (uint32_t i = 0; i < rd(b) && i < 8; i++) std::fprintf(stderr, " %08x", rd(b + 0x2c + 4 * i));
        std::fprintf(stderr, "\n");
        if (rd(b)) for (uint32_t i = 0; i < rd(b) && i < 8; i++) {
            const uint32_t d = rd(b + 8) + 0x1c * i, len = rd(d + 0x14), cap = rd(d + 0x18);
            const uint32_t p = cap >= 16 ? rd(d + 4) : d + 4;
            std::string s2; for (uint32_t j = 0; j < len && j < 400; j++) s2.push_back(char(rx_rd8(p + j)));
            std::string nm; for (uint32_t j = 0; j < 40 && rx_rd8(rd(b + 4) + 0x350 * i + j); j++) nm.push_back(char(rx_rd8(rd(b + 4) + 0x350 * i + j)));
            std::fprintf(stderr, "[ctx]   part %u: desc(%u/%u) \"%s\" name0 \"%s\"\n", i, len, cap, s2.c_str(), nm.c_str());
        }
    }
}


// ---- 0047aff0 / 00681750: the part spring, on the live entries of the damage manager --------------------------------------------------------
void case_spring(World& w) {
    say("spring");
    (void)w;
    const uint32_t mgr = 0x00db1410;
    int count = 0, entries = 0;
    const float dts[] = {0.0f, 1.0f / 60.0f, 0.03f, 0.1f, 0.25f, -0.05f, std::nanf("")};
    for (uint32_t k = 0; k < 12; k++) {
        const uint32_t E = mgr + 0x10 + 0x410 * k;
        if (!rd(E + 0x3fc)) continue;
        ++entries;
        const uint32_t n = rd(E), T = rd(E + 4);
        for (uint32_t i = 0; i < n && i < 8; i++) {
            if (!rd(E + 0x370 + 4 * i)) continue;
            const uint32_t R = T + 0x350 * i;
            for (int round = 0; round < 24; round++) {
                const float lo = fl(rd(R + 0x330)), hi = fl(rd(R + 0x334));
                float angle = rf(lo - 0.6f, hi + 0.6f);
                if (round % 6 == 4) angle = lo; if (round % 6 == 5) angle = hi;
                wf(E + 0x230 + 4 * i, angle);
                wf(E + 0x250 + 4 * i, round % 3 == 0 ? rf(-60, 60) : rf(-6, 6));
                for (int c = 0; c < 3; c++) { wf(E + 0x270 + 0x20 * i + 4 * c, rf(-4, 4)); wf(E + 0x280 + 0x20 * i + 4 * c, rf(-4, 4)); }
                float x, y, z, ww, n2;
                do { x = rf(-1, 1); y = rf(-1, 1); z = rf(-1, 1); ww = rf(-1, 1); n2 = std::sqrt(x * x + y * y + z * z + ww * ww); } while (n2 < 0.05f);
                wf(E + 0x130 + 0x10 * i, x / n2); wf(E + 0x134 + 0x10 * i, y / n2); wf(E + 0x138 + 0x10 * i, z / n2); wf(E + 0x13c + 0x10 * i, ww / n2);
                if (round % 8 == 7) { wf(R + 0x310, 0); wf(R + 0x314, 0); wf(R + 0x318, 0); }     // a zero hinge axis
                if (round % 8 == 3) { wf(R + 0x310, rf(-2, 2)); wf(R + 0x314, rf(-2, 2)); wf(R + 0x318, rf(-2, 2)); }
                const float dt = dts[(round + i) % 7];
                uint32_t a[3] = {E, i, fb(dt)};
                call_ex(0x0047aff0, regs(), 3, a);
                ++count;
            }
        }
        { rx_regs in = regs(); in.r[6] = rd(E + 0x3fc); call_ex(0x00681750, in); ++count; }
    }
    { rx_regs in = regs(); in.r[6] = 0x12345678; call_ex(0x00681750, in); ++count; }
    std::fprintf(stderr, "[ctx] spring: %d calls over %d entries\n", count, entries);
}


// ---- _CIexp ---------------------------------------------------------------------------------------------------------------------------
extern "C" double crt_exp_x87(double x);
void case_exp(World& w) {
    (void)w;
    say("exp");
    int count = 0, bad = 0;
    for (int n = 0; n < 6000; n++) {
        double x;
        switch (n % 6) {
            case 0: x = -double(rf(0.0f, 0.1f)) * double(rf(0.0f, 60.0f)); break;       // the spring's range
            case 1: x = double(rf(-20, 20)); break;
            case 2: x = double(rf(-700, 700)); break;
            case 3: x = double(rf(-1, 1)) * 1e-6; break;
            case 4: x = -double(rf(0.0f, 1.0f)) * 1e-10; break;
            default: x = double(rf(-2, 2)) * double(rf(-2, 2)); break;
        }
        if (n < 8) x = (n == 0) ? 0.0 : (n == 1) ? -0.0 : (n == 2) ? 1.0 : (n == 3) ? -1.0 : (n == 4) ? 709.5 : (n == 5) ? -745.0 : (n == 6) ? 1e-300 : 36.0;
        rx_regs in = regs(); in.fpu_in = 1; in.fpu[0] = rx_f80_from_f64(x);
        rx_regs out = call_ex(0x7859ac2c, in);
        const double orig = rx_f80_to_f64(out.st[0], 0);
        const double mine = crt_exp_x87(x);
        uint64_t a, b; std::memcpy(&a, &orig, 8); std::memcpy(&b, &mine, 8);
        ++count;
        if (a != b) { if (++bad <= 10) std::fprintf(stderr, "[ctx] exp(%a): original %016llx mine %016llx\n", x, (unsigned long long)a, (unsigned long long)b); }
    }
    std::fprintf(stderr, "[ctx] exp: %d operands, %d differ\n", count, bad);
}


// ---- 00479680 --------------------------------------------------------------------------------------------------------------------------
void case_parse(World& w) {
    say("parse");
    const uint32_t mgr = 0x00db1410;
    int count = 0;
    for (uint32_t k = 0; k < 12; k++) {
        const uint32_t E = mgr + 0x10 + 0x410 * k;
        if (!rd(E + 0x3fc)) continue;
        for (uint32_t i = 0; i < rd(E) && i < 8; i++) {
            const uint32_t out = w.scratch + 0x200, vec = w.scratch + 0x240;
            rx_regs in = regs(); in.r[1] = out; in.r[2] = vec;
            uint32_t a[1] = {rd(E + 8) + 0x1c * i};
            call_ex(0x00479680, in, 1, a);
            ++count;
        }
    }
    std::fprintf(stderr, "[ctx] parse: %d calls\n", count);
}


// ---- 007dda20 / 007ddfa0 / 007dd890 / 007dea70 ------------------------------------------------------------------------------------------------
void case_extract(World& w) {
    say("extract");
    const uint32_t V = w.vehicle;
    if (!V) { say("extract: no vehicle"); return; }
    const uint32_t model = rd(V + 0x1328), hier = model + 0x10, count = rd(hier + 0xc);
    std::vector<uint32_t> named;
    for (uint32_t i = 0; i < count && i < 200; i++) { const uint32_t r = rd(hier + 8) + 16 * i; if (!node_name(r).empty()) named.push_back(r); }
    const uint32_t dst = w.scratch + 0x100, names = w.scratch + 0x200, map = w.scratch + 0x1000;
    int calls = 0;
    auto fresh = [&]() { for (uint32_t k = 0; k < 0x50; k += 4) wr(dst + k, 0); };
    for (size_t n = 0; n < named.size(); n++) {
        const std::string nm = node_name(named[n]);
        for (size_t j = 0; j <= nm.size(); j++) rx_wr8(names + j, j < nm.size() ? uint8_t(nm[j]) : 0);
        fresh();
        rx_regs in = regs(); in.r[6] = hier; uint32_t a[2] = {dst, names};
        call_ex(0x007dda20, in, 2, a); ++calls;
        if (n % 4 == 0) { call_ex(0x007dda20, in, 2, a); ++calls; }      // the destination now has nodes
    }
    for (const char* bad : {"NoSuchNode", "OBJ_Roof"}) {
        for (uint32_t j = 0; bad[j]; j++) rx_wr8(names + j, uint8_t(bad[j])); rx_wr8(names + strlen(bad), 0);
        fresh(); rx_regs in = regs(); in.r[6] = hier; uint32_t a[2] = {dst, names}; call_ex(0x007dda20, in, 2, a); ++calls;
    }
    // maps on their own: all nodes, a random subset, one node, none; the second call on a used destination releases it first
    for (int variant = 0; variant < 8; variant++) {
        uint32_t next = 0;
        for (uint32_t i = 0; i < count; i++) {
            bool take = variant == 0 || (variant == 1 && (rnd() & 1)) || (variant == 2 && i == 0) || (variant == 3 && i == 3) || (variant >= 4 && (rnd() % 3) != 0);
            wr(map + 4 * i, take ? next++ : 0xffffffffu);
        }
        fresh();
        rx_regs in = regs(); in.r[1] = hier; uint32_t a[2] = {dst, map};
        call_ex(0x007ddfa0, in, 2, a); ++calls;
        if (variant & 1) { call_ex(0x007ddfa0, in, 2, a); ++calls; }
    }
    { rx_regs in = regs(); in.r[1] = hier; in.r[7] = rd(hier + 8) + 16; call_ex(0x007dea70, in); ++calls; }
    { rx_regs in = regs(); in.r[6] = dst; call_ex(0x007dd890, in); ++calls; }
    std::fprintf(stderr, "[ctx] extract: %d calls over %zu named nodes\n", calls, named.size());
}


// ---- 00479ef0 / 00479ce0 / 0047ae00 / 00479a00 / 0047bb90 ---------------------------------------------------------------------------------------
void case_splitpart(World& w) {
    say("splitpart");
    (void)w;
    const uint32_t mgr = 0x00db1410;
    int calls = 0;
    for (uint32_t k = 0; k < 12; k++) {
        const uint32_t E = mgr + 0x10 + 0x410 * k;
        if (!rd(E + 0x3fc)) continue;
        for (uint32_t i = 0; i < rd(E) && i < 8; i++) {
            rx_regs in = regs(); in.r[1] = E; uint32_t a[1] = {i};
            call_ex(0x00479ef0, in, 1, a); ++calls;
        }
        { uint32_t a[1] = {E}; call_ex(0x0047ae00, regs(), 1, a); ++calls; }
    }
    std::fprintf(stderr, "[ctx] splitpart: %d calls\n", calls);
}


void case_bind(World& w) {
    say("bind");
    const uint32_t mgr = 0x00db1410, V = w.vehicle;
    if (!V) { say("bind: no vehicle"); return; }
    int calls = 0;
    for (uint32_t k = 0; k < 12; k++) {
        const uint32_t E = mgr + 0x10 + 0x410 * k;
        if (rd(E + 0x3fc)) continue;
        { rx_regs in = regs(); in.r[0] = V; in.r[1] = E; call_ex(0x00479a00, in); ++calls; }
        { rx_regs in = regs(); in.r[0] = V; in.r[6] = mgr; call_ex(0x0047bb90, in); ++calls; }
        break;
    }
    std::fprintf(stderr, "[ctx] bind: %d calls\n", calls);
}


void case_entityvf(World& w) {
    say("entityvf");
    const uint32_t mgr = 0x00db1410;
    int calls = 0;
    for (uint32_t k = 0; k < 12; k++) {
        const uint32_t E = mgr + 0x10 + 0x410 * k;
        if (!rd(E + 0x3fc)) continue;
        for (uint32_t i = 0; i < rd(E) && i < 8; i++) {
            const uint32_t ent = rd(E + 0x2c + 4 * i);
            if (!ent) continue;
            for (int round = 0; round < 6; round++) {
                const uint32_t pos = w.scratch + 0x300, q = w.scratch + 0x320;
                for (uint32_t c = 0; c < 4; c++) wf(pos + 4 * c, rf(-50, 50));
                float x, y, z, ww, n2;
                do { x = rf(-1, 1); y = rf(-1, 1); z = rf(-1, 1); ww = rf(-1, 1); n2 = std::sqrt(x * x + y * y + z * z + ww * ww); } while (n2 < 0.05f);
                wf(q, x / n2); wf(q + 4, y / n2); wf(q + 8, z / n2); wf(q + 12, ww / n2);
                if (round == 5) wf(ent + 0x170, std::nanf(""));
                rx_regs in = regs(); in.r[1] = ent; uint32_t a[2] = {pos, q};
                call_ex(0x00430670, in, 2, a); ++calls;
            }
            { rx_regs in = regs(); in.r[1] = ent; uint32_t a[1] = {0}; call_ex(0x0041cc60, in, 1, a); ++calls; }
            { rx_regs in = regs(); in.r[1] = ent; call_ex(0x0041cbe0, in); ++calls; }
        }
    }
    std::fprintf(stderr, "[ctx] entityvf: %d calls\n", calls);
}

// ---- w-partdtor: the damage manager's entries released, their part entities destroyed, the part bodies moved between worlds ----------
const uint32_t kDmgMgr = 0x00db1410;
uint32_t keyed_count(uint32_t world) { return world ? (rd(world + 0x19a4) - rd(world + 0x19a0)) / 0xe0 : 0; }
uint32_t other_physics_world(const World& w, uint32_t not_this) {
    for (uint32_t off : {0x5a54u, 0x20c8u}) {
        const uint32_t level = rd(w.game + off);
        if (!mapped(level + 0x20134, 4)) continue;
        const uint32_t world = rd(level + 0x20134);
        if (world != not_this && mapped(world, 0xcce0)) return world;
    }
    return 0;
}
uint32_t db_pending(uint32_t parity) { const uint32_t db = rd(rd(0x00a95844) + 8); return (rd(db + 0x74 + 0x18 * parity) - rd(db + 0x70 + 0x18 * parity)) / 4; }
void print_parts(const char* when, const World& w) {
    const uint32_t pw = physics_world(w), ow = other_physics_world(w, pw);
    std::fprintf(stderr, "[ctx] %s: world %08x keyed %u active %u contact %u; other world %08x keyed %u active %u; db pending %u / %u\n", when, pw, keyed_count(pw),
                 rd(pw + 0x19d4), (rd(pw + 0xcc10) - rd(pw + 0xcc0c)) / 4, ow, keyed_count(ow), ow ? rd(ow + 0x19d4) : 0, db_pending(0), db_pending(1));
    for (uint32_t k = 0; k < 12; k++) {
        const uint32_t E = kDmgMgr + 0x10 + 0x410 * k, v = rd(E + 0x3fc);
        if (!v && !rd(E + 0x2c)) continue;
        std::fprintf(stderr, "[ctx]   entry %u vehicle %08x type %u parts %u:", k, v, v ? rd(v + 0x194) : 99, rd(E));
        for (uint32_t i = 0; i < rd(E) && i < 8; i++) {
            const uint32_t ent = rd(E + 0x2c + 4 * i), t = ent ? rd(ent + 0x158) : 0, sh = rd(E + 0x50 + 8 * i);
            std::fprintf(stderr, " [%u: ent %08x st %u act %u body %08x w %08x i %u rec %08x refs %u shape %08x/%u snd %08x]", i, ent, rd(E + 0xc + 4 * i), rx_rd8(E + 0x3d0 + i),
                         t, t ? rd(t + 4) : 0, t ? rd(t + 8) : 0, t ? rd(rd(t + 4) + 0x19a0) + 0xe0 * rd(t + 8) : 0, t ? rd(t) : 0, sh, sh ? rd(sh + 4) : 0, ent ? rd(ent + 0x160) : 0);
        }
        std::fprintf(stderr, "\n");
    }
}
void case_partinfo(World& w) { say("partinfo"); print_parts("partinfo", w); }

// 0047bbe0 (EDX = the manager, ESI = the vehicle) for every vehicle of the manager: 00479dd0 -> the parts' vf9 / vf23, Database vf22,
// vf0(1) 00403bb0 (0041c7b0 -> 00402360 -> 00416fe0 -> 00417830 / 00417c80 / 0076a9a0, 008825f0, 0042c080), the shape handle's vf1 00849630
void case_release(World& w) {
    say("release");
    print_parts("release before", w);
    int calls = 0;
    std::vector<uint32_t> vehicles;
    for (uint32_t k = 0; k < 12; k++) { const uint32_t v = rd(kDmgMgr + 0x10 + 0x410 * k + 0x3fc); if (v) vehicles.push_back(v); }
    // the even entries first: the odd ones' parts then sit in the middle of the keyed vector and the removal moves the last record
    std::vector<uint32_t> order;
    for (size_t n = 0; n < vehicles.size(); n += 2) order.push_back(vehicles[n]);
    for (size_t n = 1; n < vehicles.size(); n += 2) order.push_back(vehicles[n]);
    if (std::getenv("RX_CTX_ACTIVE")) for (uint32_t k = 0; k < 12; k++) {   // every other part activated first (0047a630)
        const uint32_t E = kDmgMgr + 0x10 + 0x410 * k;
        if (!rd(E + 0x3fc)) continue;
        for (uint32_t i = 0; i < rd(E) && i < 8; i++) if ((i + k) & 1) { rx_regs in = regs(); in.r[1] = i; uint32_t a[1] = {E}; call_ex(0x0047a630, in, 1, a); }
        print_parts("release activated", w);
    }
    for (uint32_t v : order) { rx_regs in = regs(); in.r[2] = kDmgMgr; in.r[6] = v; call_ex(0x0047bbe0, in); ++calls; }
    { rx_regs in = regs(); in.r[2] = kDmgMgr; in.r[6] = 0x12345678; call_ex(0x0047bbe0, in); ++calls; }   // no entry of that vehicle
    print_parts("release after", w);
    std::fprintf(stderr, "[ctx] release: %d calls over %zu vehicles\n", calls, vehicles.size());
}

// 00479dd0 directly with "active" parts (+0x3d0 + i: the inlined 0047a830) on every other part, twice (the second finds no parts)
void case_relentry(World& w) {
    say("relentry");
    int calls = 0;
    for (uint32_t k = 0; k < 12; k++) {
        const uint32_t E = kDmgMgr + 0x10 + 0x410 * k;
        if (!rd(E + 0x3fc)) continue;
        for (uint32_t i = 0; i < rd(E) && i < 8; i++) if ((i + k) & 1) {   // the part activated as a hit does it (0047a630: scene, kinematic body)
            rx_regs in = regs(); in.r[1] = i; uint32_t a[1] = {E}; call_ex(0x0047a630, in, 1, a);
        }
        print_parts("relentry activated", w);
        for (int again = 0; again < 2; again++) { rx_regs in = regs(); in.r[1] = E; call_ex(0x00479dd0, in); ++calls; }
    }
    print_parts("relentry after", w);
    std::fprintf(stderr, "[ctx] relentry: %d calls\n", calls);
}

// w-frametail: the end-of-frame removal of what the release queued. Every vehicle of the damage manager released (0047bbe0 -> 00479dd0:
// the parts' model handles queued by Database vf22 with the update counter [[0xa95844] + 0x10] + 0x58), then Database vf33 0084cc80
// for the queued parity and the other one, as Engine::Frame 0084f260 runs it after the render (the render counter +0x54: the same
// parity as +0x58 during the update). vf33 -> vf31 0084af30 (the ModelAsset's last reference: ModelAsset::sdtor) and vf32 0084af90
// (the handle out of the "model" table: 0084dea0 / 0084dd30, IAssetHandle::vf0 00668b20), the list cleared by 0084d9c0. A second
// round on the same parity finds the list empty.
void case_drain(World& w) {
    say("drain");
    const uint32_t ctx = rd(0x00a95844), db = rd(ctx + 8), eng = rd(ctx + 0x10);
    const uint32_t frame = rd(eng + 0x58);
    if (std::getenv("RX_CTX_ACTIVE")) for (uint32_t k = 0; k < 12; k++) {
        const uint32_t E = kDmgMgr + 0x10 + 0x410 * k;
        if (!rd(E + 0x3fc)) continue;
        for (uint32_t i = 0; i < rd(E) && i < 8; i++) if ((i + k) & 1) { rx_regs in = regs(); in.r[1] = i; uint32_t a[1] = {E}; call_ex(0x0047a630, in, 1, a); }
    }
    for (uint32_t k = 0; k < 12; k++) {
        const uint32_t v = rd(kDmgMgr + 0x10 + 0x410 * k + 0x3fc);
        if (!v) continue;
        rx_regs in = regs(); in.r[2] = kDmgMgr; in.r[6] = v; call_ex(0x0047bbe0, in);
    }
    print_parts("drain queued", w);
    if (std::getenv("RX_CTX_REFS")) {   // every queued handle keeps one more reference: vf32 only drops it (the handle stays in its table)
        for (uint32_t p = 0; p < 2; p++)
            for (uint32_t a = rd(db + 0x70 + 0x18 * p); a < rd(db + 0x74 + 0x18 * p); a += 4) wr(rd(a) + 0x48, rd(rd(a) + 0x48) + 1);
        say("drain: queued handles referenced once more");
    }
    int calls = 0;
    for (uint32_t f : {frame + 1, frame, frame}) {   // the other parity (empty), the queued one, the queued one again (now empty)
        const uint32_t before = db_pending(f & 1);
        rx_regs in = regs(); in.r[1] = db; uint32_t a[1] = {f};
        call_ex(0x0084cc80, in, 1, a); ++calls;
        std::fprintf(stderr, "[ctx] drain: vf33(frame %u) parity %u: %u pending -> %u\n", f, f & 1, before, db_pending(f & 1));
    }
    std::fprintf(stderr, "[ctx] drain: %d calls (frame %u)\n", calls, frame);
}

// w-frametail: the three handle destructors (vtable slot 0) on fixture handles of their classes (ReflexAlignedAlloc 0078fbe0 blocks, the
// vector at +0x50 of the split-triangle handle empty), flags 0 and 1: IAssetHandle::vf0 00668b20 (model 0x00972898), IconGeometryAssetHandle
// 005e15c0 (-> 0084a2c0; icongeom 0x00972bf8), SplitTriangleCollisionAssetHandle 007726b0 (-> 0081f6e0; collis 0x0096dd90)
void case_hsdtor(World& w) {
    (void)w;
    say("hsdtor");
    struct D { uint32_t fn, vt; } ds[] = {{0x00668b20, 0x00972898}, {0x005e15c0, 0x00972bf8}, {0x007726b0, 0x0096dd90}};
    int calls = 0;
    for (const D& d : ds) for (uint32_t flags : {0u, 1u}) {
        uint32_t size[1] = {0x70};
        const uint32_t h = call_ex(0x0078fbe0, regs(), 1, size).r[0];
        if (!mapped(h, 0x70)) { say("hsdtor: no block"); continue; }
        for (uint32_t k = 0; k < 0x70; k += 4) wr(h + k, 0);
        wr(h, d.vt); wr(h + 0x48, 0);
        if (d.fn == 0x007726b0) {   // the vector at +0x50: a proxy block of its own (freed by 0081f6e0), no elements
            const uint32_t proxy = call_ex(0x0078fbe0, regs(), 1, size).r[0];
            wr(h + 0x50, proxy);
        }
        rx_regs in = regs(); in.r[1] = h; uint32_t a[1] = {flags};
        const uint32_t r = call_ex(d.fn, in, 1, a).r[0]; ++calls;
        std::fprintf(stderr, "[ctx] hsdtor %08x flags %u: handle %08x -> %08x, vtable %08x\n", d.fn, flags, h, r, flags ? 0 : rd(h));
    }
    std::fprintf(stderr, "[ctx] hsdtor: %d calls\n", calls);
}

// 0054a500 (stdcall the player; RET 4): the local player's whole vehicle clear (-> 0047bbe0)
void case_vehclear(World& w) {
    say("vehclear");
    if (!w.player || !w.vehicle) { say("vehclear: no player vehicle"); return; }
    uint32_t a[1] = {w.player};
    call_ex(0x0054a500, regs(), 1, a);
    print_parts("vehclear after", w);
    std::fprintf(stderr, "[ctx] vehclear: 1 call (player %08x vehicle %08x)\n", w.player, w.vehicle);
}

// 0047bc20 (EAX = the manager; stdcall the vehicle) -> 0047a460: into the active scene's world; then with the UI scene active (the parts'
// bodies leave the event world for the UI world) and back
void case_partmove(World& w) {
    say("partmove");
    int calls = 0;
    const uint32_t ctx = rd(0x00a95844), ev = rd(ctx + 0x20), ui = rd(ctx + 0x1c);
    const uint32_t ev_on = rd(ev + 0x34), ui_on = rd(ui + 0x34);
    const uint32_t uiw = mapped(rd(w.game + 0x20c8) + 0x20134, 4) ? rd(rd(w.game + 0x20c8) + 0x20134) : 0;
    const bool other = uiw != 0 && uiw != physics_world(w) && mapped(uiw, 0xcce0);
    for (int round = 0; round < (other ? 3 : 1); round++) {
        if (round == 1) { wr(ev + 0x34, 0); wr(ui + 0x34, 1); }
        if (round == 2) { wr(ev + 0x34, ev_on); wr(ui + 0x34, ui_on); }
        for (uint32_t k = 0; k < 12; k++) {
            const uint32_t v = rd(kDmgMgr + 0x10 + 0x410 * k + 0x3fc);
            if (!v) continue;
            rx_regs in = regs(); in.r[0] = kDmgMgr; uint32_t a[1] = {v};
            call_ex(0x0047bc20, in, 1, a); ++calls;
        }
        print_parts(round == 0 ? "partmove same world" : round == 1 ? "partmove into the UI world" : "partmove back", w);
    }
    { rx_regs in = regs(); in.r[0] = kDmgMgr; uint32_t a[1] = {0x12345678}; call_ex(0x0047bc20, in, 1, a); ++calls; }
    std::fprintf(stderr, "[ctx] partmove: %d calls (UI world %08x)\n", calls, other ? uiw : 0);
}

// the pieces one by one on the live physics world: Kinematic_Activate some keyed bodies, then 007685f0 / 0076a9a0 / 0076aa50 / 00417c80 /
// 00416fe0 with a tracking entry of the other world (00417830 -> 0076aa50 there) / 00402360 / 00416f90 / 00849630 / 00403bb0 / Database vf22
void case_keyedrm(World& w) {
    say("keyedrm");
    const uint32_t pw = physics_world(w), ow = other_physics_world(w, pw);
    if (!pw) { say("keyedrm: no physics world"); return; }
    int calls = 0;
    const uint32_t vel = w.scratch + 0x400;
    for (uint32_t c = 0; c < 16; c += 4) wr(vel + c, 0);
    for (uint32_t i = 0, n = 0; i < keyed_count(pw) && n < 12; i += 3) {                // a few more active kinematic bodies
        const uint32_t r = rd(pw + 0x19a0) + 0xe0 * i;
        if (int32_t(rd(r + 0x80)) >= 0 || rd(pw + 0x19d4) >= 190) continue;
        rx_regs in = regs(); in.r[1] = r; uint32_t a[2] = {pw, vel};
        call_ex(0x00770b00, in, 2, a); ++n;
    }
    print_parts("keyedrm activated", w);
    std::vector<uint32_t> active;
    for (uint32_t i = 0; i < keyed_count(pw); i++) if (int32_t(rd(rd(pw + 0x19a0) + 0xe0 * i + 0x80)) >= 0) active.push_back(i);
    {   // 007685f0 on the contact list: a present value replaced and back, an absent one
        const uint32_t cl = pw + 0xcc00, n = (rd(cl + 0x10) - rd(cl + 0xc)) / 4;
        rx_regs in = regs(); in.r[7] = cl;
        uint32_t a[2] = {n ? rd(rd(cl + 0xc)) : 1u, 0x0badf00du}; call_ex(0x007685f0, in, 2, a); ++calls;
        uint32_t b[2] = {0x0badf00du, a[0]}; call_ex(0x007685f0, in, 2, b); ++calls;
        uint32_t c[2] = {0x77777777u, 0x66666666u}; call_ex(0x007685f0, in, 2, c); ++calls;
    }
    for (size_t j = 0; j + 1 < active.size() && j < 4; j += 2) {                      // 0076a9a0 and back
        rx_regs in = regs(); in.r[0] = active[j]; in.r[6] = pw; uint32_t a[1] = {active[j + 1]};
        call_ex(0x0076a9a0, in, 1, a); ++calls;
        rx_regs in2 = regs(); in2.r[0] = active[j + 1]; in2.r[6] = pw; uint32_t b[1] = {active[j]};
        call_ex(0x0076a9a0, in2, 1, b); ++calls;
    }
    if (!active.empty()) for (int j = 0; j < 3; j++) {                                    // 0076aa50: the first, a middle and the last active body
        const uint32_t idx = active[j == 0 ? 0 : j == 1 ? active.size() / 2 : active.size() - 1];
        rx_regs in = regs(); in.r[1] = idx; in.r[6] = pw; call_ex(0x0076aa50, in); ++calls;
    }
    if (keyed_count(pw) >= 2) {                                                           // 00417c80 on a scratch copy of a live record
        const uint32_t d = w.scratch + 0x600, s0 = rd(pw + 0x19a0), s1 = s0 + 0xe0;
        for (uint32_t o = 0; o < 0xe0; o += 4) wr(d + o, rd(s0 + o));
        if (rd(d + 0x2c)) wr(rd(d + 0x2c) + 4, rd(rd(d + 0x2c) + 4) + 1);             // the copy holds its own shape reference
        for (int again = 0; again < 2; again++) { rx_regs in = regs(); in.r[6] = d; in.r[7] = s1; call_ex(0x00417c80, in); ++calls; }
    }
    const uint32_t db = rd(rd(0x00a95844) + 8);
    uint32_t vf22 = 0;
    for (uint32_t k = 0; k < 12; k++) {                                                   // the parts piece by piece
        const uint32_t E = kDmgMgr + 0x10 + 0x410 * k;
        if (!rd(E + 0x3fc)) continue;
        for (uint32_t i = 0; i < rd(E) && i < 8; i++) {
            const uint32_t ent = rd(E + 0x2c + 4 * i);
            if (!ent) continue;
            if (i % 3 != 2) { rx_regs in = regs(); in.r[1] = ent + 0x154; call_ex(0x00416f90, in); ++calls; }
            if (i % 3 == 0) { rx_regs in = regs(); in.r[1] = ent + 0x154; call_ex(0x00416f90, in); ++calls; }   // a cleared handle
            for (int again = 0; again < 2; again++) { rx_regs in = regs(); in.r[1] = E + 0x4c + 8 * i; call_ex(0x00849630, in); ++calls; }
            { rx_regs in = regs(); in.r[1] = ent; uint32_t a[1] = {(i & 1) ? 0u : 1u}; call_ex(0x00403bb0, in, 1, a); ++calls; }
            wr(E + 0x2c + 4 * i, 0);
            { rx_regs in = regs(); in.r[1] = db; uint32_t a[2] = {0x1000 + vf22, vf22 & 3}; call_ex(0x0084ce00, in, 2, a); ++calls; ++vf22; }
        }
    }
    if (ow && keyed_count(ow) > 0 && keyed_count(pw) > 2) {                              // the last record's entry says "the other world"
        const uint32_t last = rd(pw + 0x19a0) + 0xe0 * (keyed_count(pw) - 1), t = rd(last + 0x30);
        if (mapped(t, 12)) {
            const uint32_t keep4 = rd(t + 4), keep8 = rd(t + 8);
            wr(t + 4, ow); wr(t + 8, keyed_count(ow) - 1);
            rx_regs in = regs(); in.r[6] = pw + 0x1994; uint32_t a[1] = {1};
            call_ex(0x00416fe0, in, 1, a); ++calls;
            std::fprintf(stderr, "[ctx] keyedrm: other-world entry (was %08x / %u) now %08x / %u\n", keep4, keep8, rd(t + 4), rd(t + 8));
        }
    }
    for (int round = 0; round < 3 && keyed_count(pw) > 2; round++) {                      // 00402360: the last, the first, a middle record
        const uint32_t n = keyed_count(pw), idx = round == 0 ? n - 1 : round == 1 ? 0 : n / 2;
        rx_regs in = regs(); in.r[7] = idx; uint32_t a[1] = {pw};
        call_ex(0x00402360, in, 1, a); ++calls;
    }
    print_parts("keyedrm removed", w);
    for (; vf22 < 40; vf22++) { rx_regs in = regs(); in.r[1] = db; uint32_t a[2] = {0x2000 + vf22, vf22 % 5}; call_ex(0x0084ce00, in, 2, a); ++calls; }
    print_parts("keyedrm after", w);
    std::fprintf(stderr, "[ctx] keyedrm: %d calls (%zu active bodies)\n", calls, active.size());
}

struct CaseDef { const char* name; void (*fn)(World&); };
const CaseDef kCases[] = {
    {"savedpose", case_savedpose}, {"water", case_water}, {"keyed", case_keyed}, {"scripted", case_scripted}, {"clock", case_clock}, {"reset", case_reset}, {"eliminate", case_eliminate}, {"stuntend", case_stuntend}, {"rotate", case_rotate}, {"findentity", case_findentity}, {"damage", case_damage}, {"dmgstate", case_dmgstate}, {"spring", case_spring}, {"entityvf", case_entityvf}, {"bind", case_bind}, {"splitpart", case_splitpart}, {"extract", case_extract}, {"parse", case_parse}, {"exp", case_exp}, {"partinfo", case_partinfo}, {"release", case_release}, {"relentry", case_relentry}, {"vehclear", case_vehclear}, {"partmove", case_partmove}, {"keyedrm", case_keyedrm}, {"drain", case_drain}, {"hsdtor", case_hsdtor},
};

void run_case(reflex_vm::Machine& m, const CaseDef& c, World w) {
    uint32_t regs_[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
    std::memcpy(regs_, m.r, sizeof regs_); std::memcpy(xmm, m.xmm, sizeof xmm); std::memcpy(mmx, m.mmx, sizeof mmx);
    auto flags = m.flags; auto fpu = m.fpu; auto cw = m.fpu_cw, sw = m.fpu_sw; auto mxcsr = m.mxcsr; bool mmx_active = m.mmx_active;
    auto host_restore = snapshot_host_state(m);
    if (!host_restore) { std::fprintf(stderr, "[ctx] ERROR missing reversible host state\n"); return; }
    Pages pages(m.mem);
    c.fn(w);
    pages.restore();
    host_restore();
    std::memcpy(m.r, regs_, sizeof regs_); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
    m.flags = flags; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
}
}  // namespace

RX_PORT(0x0084ed20, CtxProbeHook, "thunk", 0) {
    const char* cases = std::getenv("RX_CTX_PROBE");
    if (!cases) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_CTX_FRAME") ? uint32_t(std::atoi(std::getenv("RX_CTX_FRAME"))) : 600;
    if (++g_frame != at) return reflex_vm::RUN_ORIGINAL;
    g_m = &m;
    const World w = find_world(m);
    std::fprintf(stderr, "[ctx] frame %u: game %08x player %08x vehicle %08x\n", g_frame, w.game, w.player, w.vehicle);
    const std::string list(cases);
    for (const CaseDef& c : kCases) {
        size_t pos = 0;
        while (pos <= list.size()) {
            size_t e = list.find(',', pos); if (e == std::string::npos) e = list.size();
            if (list.compare(pos, e - pos, c.name) == 0 && e - pos == std::strlen(c.name)) { run_case(m, c, w); break; }
            pos = e + 1;
        }
    }
    return reflex_vm::RUN_ORIGINAL;
}
