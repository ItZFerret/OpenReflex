// race_ridepath_probe.cpp -- the oracle's probe for the ride-path ports no reference world reaches (development only): the AI
// BehaviorOfftrack state 1 aim (00529190) and the way back (00529330), and the player's reset over a "reset" terrain material
// (00550b70). A frame hook (thunk, Engine::Update 0084ed20, outside every verified call) at the last requested frame calls the
// original addresses on live game objects, so with the port installed Verify runs the core function and then the original on the
// same state and compares memory and the declared registers exactly as for a call from the game.
//   REFLEX_PORT=rx_race,rx_RidePathProbeHook REFLEX_PORT_VERIFY=1 RX_RIDE_PROBE=<groups> [RX_RIDE_PROBE_FRAME=<n>]
// groups (letters): i print what the probe finds (activity state, players, drivers, behaviour classes), o the offtrack calls,
// r the reset-over-material call, n the BehaviorNavigateTrack modes 1 / 2 (005278e0 / 00527fc0) and their offsets (00528bd0 /
// 00528a70) on a monitor corner the probe builds on the live objects, f the BehaviorBlueYellowFlag state 1 (00522a70). No game
// logic here: only the state the original functions
// read is set (the behaviour's state +0x30, the player's reset timer +0x116c, the monitor's corner fields) and put back after
// each call.
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
extern "C" {
#include "../game/race.h"
}
using namespace reflex_port;

namespace {
uint32_t g_frame = 0;
reflex_vm::Machine* g_m = nullptr;
bool mapped(uint32_t a, uint32_t n = 4) { return g_m && g_m->mem.mapped(a) && g_m->mem.mapped(a + n - 1); }
uint32_t rd(uint32_t a) { return mapped(a) ? rx_rd32(a) : 0; }
void wr(uint32_t a, uint32_t v) { rx_wr32(a, v); }
struct Saved {
    uint32_t r[8]; reflex_vm::Xmm xmm[8]; uint32_t flags; size_t fpu;
    explicit Saved(reflex_vm::Machine& m) : flags(m.flags), fpu(m.fpu.size()) { std::memcpy(r, m.r, sizeof r); std::memcpy(xmm, m.xmm, sizeof xmm); }
    void restore(reflex_vm::Machine& m) const { std::memcpy(m.r, r, sizeof r); std::memcpy(m.xmm, xmm, sizeof xmm); m.flags = flags; m.fpu.resize(fpu); }
};
rx_regs regs() { rx_regs r; std::memset(&r, 0, sizeof r); return r; }
rx_regs call(uint32_t fn, const rx_regs& in, int n = 0, const uint32_t* args = nullptr) {
    rx_regs out = regs();
    rx_call_original_ex(fn, &in, &out, n, args);
    return out;
}
// a field the probe sets and puts back (live game data)
struct Poke { uint32_t a, old; Poke(uint32_t addr, uint32_t v) : a(addr), old(rd(addr)) { wr(addr, v); } ~Poke() { wr(a, old); } };

const uint32_t kOfftrackVf5 = 0x00529140u;   // BehaviorOfftrack::vf5 (the type id 7, shared by the four offtrack classes)
uint32_t g_game() { return rd(0x00a946b8); }
uint32_t g_activity(uint32_t p) { return rd(p + 0x94); }

void info() {
    const uint32_t g = g_game();
    if (!g) { std::fprintf(stderr, "[ridepath] no game object\n"); return; }
    const uint32_t nslots = uint8_t(rx_rd8(g + 0x1541c));
    std::fprintf(stderr, "[ridepath] game %08x slots %u\n", g, nslots);
    for (uint32_t s = 0; s < nslots && s < 16; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (!mapped(p, 0x12a0)) continue;
        const uint32_t act = g_activity(p);
        std::fprintf(stderr, "[ridepath] slot %u player %08x local %u ai %u act %08x state %d veh %08x\n", s, p, rd(p + 4), rd(p + 0x10),
                     act, act ? int(rd(act + 0x21c)) : -1, rd(p + 0x1ac));
        if (!act) continue;
        const int32_t n = int32_t(rd(act + 0x26c));
        std::fprintf(stderr, "[ridepath]   drivers %d\n", n);
        for (int32_t i = 0; i < n && i < 32; i++) {
            const uint32_t d = rd(rd(act + 0x268) + 4 * uint32_t(i));
            if (!d) continue;
            std::fprintf(stderr, "[ridepath]   driver %d %08x clock %08x behaviours %u track %08x node %08x %08x %08x prog %08x\n", i, d, rd(d + 0x130), rd(d + 0xee0),
                         rd(d + 0x10), rd(d + 0x14), rd(d + 0x18), rd(d + 0x1c), rd(d + 0x90));
            for (uint32_t k = 0; k < rd(d + 0xee0) && k < 40; k++) {
                const uint32_t b = rd(d + 0xe8 + 4 * k);
                if (!b) continue;
                const uint32_t fn5 = rd(rd(b) + 0x14);
                std::fprintf(stderr, "[ridepath]     behaviour %u %08x vt %08x vf5 %08x state %d\n", k, b, rd(b), fn5, int(rd(b + 0x30)));
            }
        }
    }
}

// 00529150 / 00529190 / 00529330 on every AI driver's offtrack behaviour: the behaviour's own entry (watch), then the aim and
// the way back (states 2 / 3 / 4), as the driver loop would run them.
void probe_offtrack() {
    const uint32_t g = g_game();
    if (!g) return;
    const uint32_t nslots = uint8_t(rx_rd8(g + 0x1541c));
    int found = 0, calls = 0;
    uint32_t first_b = 0, first_d = 0;
    for (uint32_t s = 0; s < nslots && s < 16; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (!mapped(p, 0x12a0)) continue;
        const uint32_t act = g_activity(p);
        if (!act) continue;
        const int32_t n = int32_t(rd(act + 0x26c));
        for (int32_t i = 0; i < n && i < 32; i++) {
            const uint32_t d = rd(rd(act + 0x268) + 4 * uint32_t(i));
            if (!d) continue;
            for (uint32_t k = 0; k < rd(d + 0xee0) && k < 40; k++) {
                const uint32_t b = rd(d + 0xe8 + 4 * k);
                if (!b || rd(rd(b) + 0x14) != kOfftrackVf5) continue;   // the behaviour's vf5 slot (thiscall, no args)
                if (rd(b + 0x30) != 0 || !rd(d + 0x1c)) continue;   // one left-the-track run per behaviour
                ++found;
                std::fprintf(stderr, "[ridepath] offtrack behaviour %08x driver %08x rules %08x %08x node %08x\n", b, d, rd(b + 0x1c), rd(b + 0x20), rd(d + 0x1c));
                if (!first_b) { first_b = b; first_d = d; }
                {   // the watcher: remembers the driver's track position and goes to state 1 when the rule says off track
                    rx_regs in = regs(); in.r[0] = b;
                    call(0x00529150, in);
                    if (rd(b + 0x30) == 0) {   // the rule reports on track: the three stores 00529150 makes (the nodes are the driver's)
                        wr(b + 0x10, rd(d + 0x14));
                        wr(b + 0x14, rd(d + 0x18));
                        wr(b + 0x18, rd(d + 0x1c));
                        wr(b + 0x30, 1);
                    }
                }
                {   // state 1: the aim (writes the point +0x28 / +0x2c and the state)
                    rx_regs in = regs(); in.r[6] = b;
                    call(0x00529190, in);
                    ++calls;
                    std::fprintf(stderr, "[ridepath] 00529190 b %08x -> state %d point %08x %08x\n", b, int(rd(b + 0x30)), rd(b + 0x28), rd(b + 0x2c));
                }
                for (uint32_t st = 2; st <= 4; st++) {
                    const uint32_t st0 = rd(b + 0x30);
                    wr(b + 0x30, st);
                    const uint32_t a[1] = {b};
                    rx_regs in = regs(); in.r[1] = b;
                    call(0x00529330, in, 1, a);
                    ++calls;
                    std::fprintf(stderr, "[ridepath] 00529330 b %08x (from state %u, set %u) -> %d aim %08x %08x\n", b, st0, st,
                                 int(rd(b + 0x30)), rd(b + 0x28), rd(b + 0x2c));
                }
            }
        }
        if (found) break;
    }
    std::fprintf(stderr, "[ridepath] offtrack: %d behaviours, %d calls\n", found, calls);
    if (!first_b) return;
    // the aim against a point ahead of / behind the vehicle (the field 00529190 reads): toward it (state 3, the point kept) and
    // away from it (state 2, the look-ahead)
    const uint32_t src = rd(first_b + 0x1c) ? rd(first_b + 0x1c) + 0x10 : rd(first_b + 0x20) + 0x90;
    if (!src) return;
    const uint32_t veh = rd(rd(first_d + 0x74) + 0x1ac);
    const float vx = rx_oracle::f32(rd(veh + 0xc0)), vz = rx_oracle::f32(rd(veh + 0xc8));
    const float hx = rx_oracle::f32(rd(veh + 0x130)), hz = rx_oracle::f32(rd(veh + 0x138)), k = 20.0f;
    for (int ahead = 1; ahead >= 0; ahead--) {
        Poke px(src + 0x30, rx_oracle::bits(vx + (ahead ? k : -k) * hx));
        Poke pz(src + 0x38, rx_oracle::bits(vz + (ahead ? k : -k) * hz));
        { rx_regs in = regs(); in.r[6] = first_b; wr(first_b + 0x30, 1); call(0x00529190, in); }
        std::fprintf(stderr, "[ridepath] 00529190 point %s -> state %d\n", ahead ? "ahead" : "behind", int(rd(first_b + 0x30)));
        wr(first_b + 0x30, 3);                  // state 3: the point kept when the heading test passes
        const uint32_t a[1] = {first_b};
        rx_regs in = regs(); in.r[1] = first_b;
        call(0x00529330, in, 1, a);
        std::fprintf(stderr, "[ridepath] 00529330 state 3 -> %d aim %08x %08x\n", int(rd(first_b + 0x30)), rd(first_b + 0x28), rd(first_b + 0x2c));
    }
    {   // still off the track: the rule stays 0 -> the tail returns without resetting the state
        const uint32_t r2 = rd(first_b + 0x20);
        if (r2) {
            Poke off(r2 + 0x10, 0);
            wr(first_b + 0x30, 2);
            const uint32_t a[1] = {first_b};
            rx_regs in = regs(); in.r[1] = first_b;
            call(0x00529330, in, 1, a);
            std::fprintf(stderr, "[ridepath] 00529330 off track -> state %d\n", int(rd(first_b + 0x30)));
        }
    }
    // the track rule (+0x1c) the AI's own objects do not have in these worlds: a rule block on the game heap (its point at
    // rule + 0x10 + 0x30 / 0x38, the on-track flag at +0x50), 20 units ahead of the vehicle
    uint32_t rule = 0;
    {
        const uint32_t a[1] = {0x80};
        rule = rx_call_original(0x0078fbe0, RX_CDECL, 0, 1, a);   // ReflexAlignedAlloc
        for (uint32_t i = 0; i < 0x80; i += 4) wr(rule + i, 0);
        wr(rule + 0x40, rx_oracle::bits(vx + 20.0f * hx));
        wr(rule + 0x48, rx_oracle::bits(vz + 20.0f * hz));
        wr(rule + 0x50, 1);
        Poke pr(first_b + 0x1c, rule);
        wr(first_b + 0x30, 1);
        { rx_regs in = regs(); in.r[6] = first_b; call(0x00529190, in); }
        std::fprintf(stderr, "[ridepath] 00529190 track rule -> state %d point %08x %08x\n", int(rd(first_b + 0x30)), rd(first_b + 0x28), rd(first_b + 0x2c));
        wr(first_b + 0x30, 3);
        const uint32_t a2[1] = {first_b};
        rx_regs in = regs(); in.r[1] = first_b;
        call(0x00529330, in, 1, a2);
        std::fprintf(stderr, "[ridepath] 00529330 track rule (on track) -> state %d\n", int(rd(first_b + 0x30)));
    }
    {   // the section advance: the node run ends (a section's last node, when the vehicle is past it)
        for (uint32_t s2 = 0; s2 < nslots && s2 < 16; s2++) {
            const uint32_t p2 = g + 0x7460 + 0x12a0 * s2;
            if (!mapped(p2, 0x12a0)) continue;
            const uint32_t act2 = g_activity(p2);
            if (!act2) continue;
            const int32_t n2 = int32_t(rd(act2 + 0x26c));
            for (int32_t i2 = 0; i2 < n2 && i2 < 32; i2++) {
                const uint32_t d2 = rd(rd(act2 + 0x268) + 4 * uint32_t(i2));
                if (!d2) continue;
                uint32_t last = rd(d2 + 0x1c), prev = 0;
                for (int k2 = 0; k2 < 4096 && last; k2++) {
                    const uint32_t nx = rd(last + 0x64);
                    if (!nx) break;
                    prev = last; last = nx;
                }
                if (!last) continue;
                const uint32_t veh2 = rd(rd(d2 + 0x74) + 0x1ac);
                const float dz2 = rx_oracle::f32(rd(veh2 + 0xc8)) - rx_oracle::f32(rd(last + 8));
                const float dx2 = rx_oracle::f32(rd(veh2 + 0xc0)) - rx_oracle::f32(rd(last + 0));
                const float dot = rx_oracle::f32(rd(last + 0x58)) * dz2 + rx_oracle::f32(rd(last + 0x50)) * dx2;
                if (!(dot > 0.0f)) {   // the vehicle is not there yet: put it past the section's last node (the block reads the track only)
                    Poke px2(veh2 + 0xc0, rx_oracle::bits(rx_oracle::f32(rd(last + 0)) + 100.0f * rx_oracle::f32(rd(last + 0x50))));
                    Poke pz2(veh2 + 0xc8, rx_oracle::bits(rx_oracle::f32(rd(last + 8)) + 100.0f * rx_oracle::f32(rd(last + 0x58))));
                    Poke pn3(first_b + 0x18, last);
                    Poke ps3(first_b + 0x10, rd(d2 + 0x14));
                    wr(first_b + 0x30, 2);
                    const uint32_t a3[1] = {first_b};
                    rx_regs in3 = regs(); in3.r[1] = first_b;
                    call(0x00529330, in3, 1, a3);
                    std::fprintf(stderr, "[ridepath] 00529330 section advance (veh past last node %08x) -> section %08x node %08x\n", last, rd(first_b + 0x10), rd(first_b + 0x18));
                    s2 = 99; break;
                }
                (void)prev;
                Poke pn(first_b + 0x18, last);
                Poke ps(first_b + 0x10, rd(d2 + 0x14));
                wr(first_b + 0x30, 2);
                const uint32_t a[1] = {first_b};
                rx_regs in = regs(); in.r[1] = first_b;
                call(0x00529330, in, 1, a);
                std::fprintf(stderr, "[ridepath] 00529330 section advance (last node %08x) -> section %08x node %08x\n", last, rd(first_b + 0x10), rd(first_b + 0x18));
                s2 = 99; break;
            }
            if (s2 == 99) break;
        }
    }
    {   // the return-to-track tail's vf6 on +0x0c (a live behaviour) and the speed branch (the vehicle over 58.6664)
        const uint32_t beh0 = rd(first_d + 0xe8);
        Poke pc(first_b + 0xc, beh0);
        const uint32_t veh2 = rd(rd(first_d + 0x74) + 0x1ac);
        Poke spd(veh2 + 0x150, rx_oracle::bits(100.0f));
        wr(first_b + 0x30, 2);
        const uint32_t a[1] = {first_b};
        rx_regs in = regs(); in.r[1] = first_b;
        call(0x00529330, in, 1, a);
        std::fprintf(stderr, "[ridepath] 00529330 on track + fast -> state %d steer %08x %08x\n", int(rd(first_b + 0x30)), rd(first_d + 0x60), rd(first_d + 0x64));
    }
}

// 00550b70 on a local human player (or the first with a vehicle): the release timer cleared and, when the activity is not
// running, its state set to 2 so the reset body (005501f0 -> 0054b720 / 0054fff0 / 00550280) runs as it does in a race.
void probe_reset() {
    const uint32_t g = g_game();
    if (!g) return;
    const uint32_t nslots = uint8_t(rx_rd8(g + 0x1541c));
    uint32_t pick = 0;
    for (uint32_t s = 0; s < nslots && s < 16 && !pick; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (!mapped(p, 0x12a0) || !rd(p + 0x1ac) || !g_activity(p)) continue;
        if (rd(p + 4) != 0 && rd(p + 0x10) == 0) { pick = p; break; }
    }
    for (uint32_t s = 0; s < nslots && s < 16 && !pick; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (mapped(p, 0x12a0) && rd(p + 0x1ac) && g_activity(p)) pick = p;
    }
    if (!pick) { std::fprintf(stderr, "[ridepath] reset: no player\n"); return; }
    const uint32_t act = g_activity(pick);
    Poke p_act((act + 0x21c), 2);                 // rx_player_reset_to only acts in the running states
    Poke p_timer(pick + 0x116c, 0);
    std::fprintf(stderr, "[ridepath] 00550b70 p %08x act %08x state %d veh %08x timer %08x\n", pick, act, int(rd(act + 0x21c)), rd(pick + 0x1ac), rd(pick + 0x116c));
    rx_regs in = regs(); in.r[6] = pick;
    call(0x00550b70, in);
    std::fprintf(stderr, "[ridepath] 00550b70 -> reason %d timer %08x pos %08x %08x %08x\n", int(rd(pick + 0x1170)), rd(pick + 0x116c),
                 rd(pick + 0x1180), rd(pick + 0x1184), rd(pick + 0x1188));
    // without a respawn point (the driver's progress node cleared): the vehicle's own position / quaternion axes
    {
        const int32_t n = int32_t(rd(act + 0x26c));
        uint32_t drv = 0;
        for (int32_t i = 0; i < n && i < 32 && !drv; i++) {
            const uint32_t d = rd(rd(act + 0x268) + 4 * uint32_t(i));
            if (d && rd(d + 0x74) == pick) drv = d;
        }
        if (!drv) { std::fprintf(stderr, "[ridepath] reset fallback: no driver\n"); return; }
        Poke prog(drv + 0x90, 0);
        Poke p_timer2(pick + 0x116c, 0);
        Poke p_act2(act + 0x21c, 2);
        rx_regs in2 = regs(); in2.r[6] = pick;
        call(0x00550b70, in2);
        std::fprintf(stderr, "[ridepath] 00550b70 fallback -> reason %d timer %08x pos %08x %08x %08x orient %08x %08x %08x\n",
                     int(rd(pick + 0x1170)), rd(pick + 0x116c), rd(pick + 0x1180), rd(pick + 0x1184), rd(pick + 0x1188),
                     rd(pick + 0x1190), rd(pick + 0x1194), rd(pick + 0x1198));
    }
}
// ---- group n: BehaviorNavigateTrack modes 1 / 2 (005278e0 / 00527fc0) and their offset helpers (00528bd0 / 00528a70) ----
// No reference world updates the AI drivers, so at one frame the probe builds the monitor-corner state the driver loop
// would have (BehaviorMonitorCorner, the navigate behaviour's +0x34: state 1 / 2, the section / node / direction / length)
// on the live objects and calls the originals there. Every call is a per-call Verify of its core port.
struct PokeBits { uint32_t a, old; PokeBits(uint32_t addr, uint32_t set, uint32_t clr) : a(addr), old(rd(addr)) { wr(addr, (old | set) & ~clr); } ~PokeBits() { wr(a, old); } };
struct PokeMany { uint32_t a[20], old[20]; int n = 0;
    void add(uint32_t addr, uint32_t set, uint32_t clr) { a[n] = addr; old[n] = rd(addr); wr(addr, (old[n] | set) & ~clr); n++; }
    ~PokeMany() { for (int i = n - 1; i >= 0; i--) wr(a[i], old[i]); }
};

void probe_navigate() {
    const uint32_t g = g_game();
    if (!g) return;
    const uint32_t nslots = uint8_t(rx_rd8(g + 0x1541c));
    uint32_t bn = 0, drv = 0, mon = 0;
    for (uint32_t s = 0; s < nslots && s < 16 && !bn; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (!mapped(p, 0x12a0)) continue;
        const uint32_t act = g_activity(p);
        if (!act) continue;
        const int32_t n = int32_t(rd(act + 0x26c));
        for (int32_t i = 0; i < n && i < 32 && !bn; i++) {
            const uint32_t d = rd(rd(act + 0x268) + 4 * uint32_t(i));
            if (!d) continue;
            uint32_t nb = 0, mb = 0;
            for (uint32_t k = 0; k < rd(d + 0xee0) && k < 40; k++) {
                const uint32_t b = rd(d + 0xe8 + 4 * k);
                if (!b) continue;
                if (rd(b) == 0x0093f8b0u) nb = b;      // BehaviorNavigateTrack
                if (rd(b) == 0x0093f9c8u) mb = b;      // BehaviorMonitorCorner
            }
            if (nb) { bn = nb; drv = d; mon = rd(nb + 0x34) ? rd(nb + 0x34) : mb; }
        }
    }
    if (!bn) { std::fprintf(stderr, "[ainav] no navigate behaviour\n"); return; }
    const uint32_t veh = rd(rd(drv + 0x74) + 0x1ac);
    const uint32_t node = rd(drv + 0x1c), prev = rd(drv + 0x18), sec = rd(drv + 0x14);
    if (!mon || !node || !veh) { std::fprintf(stderr, "[ainav] navigate %08x monitor %08x node %08x veh %08x (incomplete)\n", bn, mon, node, veh); return; }
    const uint32_t koff = 4u * (rd(veh + 0x198) + 4u * rd(veh + 0x194)) + 0x68u;
    const uint32_t idx = rd(rd(drv + 0x74) + 0x8f4) + (rd(drv + 0x328) * 3u + rd(drv + 0x32c)) * 4u;
    std::fprintf(stderr, "[ainav] navigate %08x driver %08x monitor %08x veh %08x node %08x prev %08x sec %08x koff %x"
                         " b4 %d side %d b10 %d b28 %d mon st %d dir %d len %08x; tun i %u: 9e4538 %08x 9e45f8 %08x a04958 %08x 9e35e8 %08x 9e3288 %08x 9e3f98 %08x 9e42f8 %08x speed %08x\n",
                 bn, drv, mon, veh, node, prev, sec, koff, int(rd(bn + 4)), int(rd(bn + 0xc)), int(rd(bn + 0x10)),
                 int(rd(bn + 0x28)), int(rd(mon + 0xc)), int(rd(mon + 0x1c)), rd(mon + 0x20), idx,
                 rd(0x9e4538u + 4u * idx), rd(0x9e45f8u + 4u * idx), rd(0xa04958u + 4u * idx), rd(0x9e35e8u + 4u * idx),
                 rd(0x9e3288u + 4u * idx), rd(0x9e3f98u + 4u * idx), rd(0x9e42f8u + 4u * idx), rd(veh + 0x150));
    int calls = 0;
    // the call sites' shape: the behaviour is pushed (and left in EAX); RET 4
    auto nav = [&](uint32_t addr, uint32_t beh) {
        const uint32_t a[1] = {beh};
        rx_regs in = regs(); in.r[0] = beh;
        call(addr, in, 1, a);
        ++calls;
    };
    // the helpers: EAX = the EAX-side, EDX = node, ESI = behaviour; stack: side, random; RET 8
    auto off = [&](uint32_t addr, uint32_t eax, uint32_t edx, uint32_t beh, uint32_t side, float rnd) {
        const uint32_t a[2] = {side, rx_oracle::bits(rnd)};
        rx_regs in = regs(); in.r[0] = eax; in.r[2] = edx; in.r[6] = beh;
        call(addr, in, 2, a);
        ++calls;
    };
    // 00528940 (EDI = behaviour, EDX = node, stack side; RET 4) -> xmm0: the plain line offset the two modes use too
    auto lineoff = [&](uint32_t node_, uint32_t beh, uint32_t side) {
        const uint32_t a[1] = {side};
        rx_regs in = regs(); in.r[2] = node_; in.r[7] = beh;
        call(0x00528940, in, 1, a);
        ++calls;
    };
    // a node run's 0x200 corner flags (mode 2's nearest node is somewhere in the run the look-ahead lands on)
    uint32_t run[16];
    int nrun = 0;
    for (uint32_t q = node; q && nrun < 16; q = rd(q + 0x64)) run[nrun++] = q;
    const uint32_t pl = rd(drv + 0x74);
    // a hard turn (|heading . target| < -0.75): the vehicle on one edge of the corner node (its lateral fraction 0 / 1),
    // then the heading straight away from the target the first call wrote (+0x20 / +0x24)
    auto hardturn = [&](uint32_t addr, uint32_t ex, uint32_t ez) {
        { Poke px(veh + 0xc0, rd(node + ex)); Poke pz(veh + 0xc8, rd(node + ez)); nav(addr, bn); }
        { Poke px(veh + 0xc0, rd(node + ex)); Poke pz(veh + 0xc8, rd(node + ez)); nav(addr, bn); }
        {
            Poke px(veh + 0xc0, rd(node + ex)); Poke pz(veh + 0xc8, rd(node + ez));
            Poke h1(veh + 0x130, rx_oracle::bits(rx_oracle::f32(rd(veh + 0xc0)) - rx_oracle::f32(rd(bn + 0x20))));
            Poke h2(veh + 0x138, rx_oracle::bits(rx_oracle::f32(rd(veh + 0xc8)) - rx_oracle::f32(rd(bn + 0x24))));
            nav(addr, bn);
        }
    };
    // the monitoring corner the driver loop would have found: node / direction / length
    {
        Poke p0(mon + 0xc, 1);
        Poke p1(mon + 0x10, sec);
        Poke p2(mon + 0x14, node);
        Poke p3(mon + 0x18, node);
        Poke p4(mon + 0x1c, 1);
        Poke p5(mon + 0x20, rx_oracle::bits(40.0f));
        Poke p6(bn + 4, 1);
        Poke p7(bn + 0xc, 0xffffffffu);
        Poke p8(bn + 0x10, 0);
        Poke p9(bn + 0x28, 0);
        Poke p10(bn + 0x40, rx_oracle::bits(0.25f));
        Poke p11(bn + 0x44, rx_oracle::bits(0.5f));
        Poke p12(bn + 0x48, rx_oracle::bits(0.75f));
        Poke p13(drv + 0x310, 0);
        // the helpers on their own: the corner direction +-1, the side the same / the other, both edge tables, the three ability
        // multipliers (player +0x8f4 <= 3 and +0x8f8 0 / 1 / 2)
        off(0x00528bd0, 1, node, bn, 1, 0.5f);
        off(0x00528bd0, 1, node, bn, 0xffffffffu, 0.25f);
        off(0x00528bd0, 0xffffffffu, node, bn, 0xffffffffu, 0.75f);
        off(0x00528bd0, 0xffffffffu, node, bn, 0, 0.0f);
        off(0x00528a70, 1, node, bn, 1, 0.5f);
        off(0x00528a70, 1, node, bn, 0xffffffffu, 0.25f);
        off(0x00528a70, 0xffffffffu, node, bn, 0xffffffffu, 0.75f);
        off(0x00528a70, 0xffffffffu, node, bn, 0, 0.0f);
        {
            Poke a0(pl + 0x8f4, 0);
            Poke a1(pl + 0x8f8, 0); off(0x00528bd0, 1, node, bn, 1, 0.5f); off(0x00528a70, 1, node, bn, 1, 0.5f);
        }
        { Poke a1(pl + 0x8f8, 1); off(0x00528bd0, 1, node, bn, 1, 0.5f); off(0x00528a70, 1, node, bn, 1, 0.5f); }
        { Poke a1(pl + 0x8f8, 2); off(0x00528bd0, 1, node, bn, 1, 0.5f); off(0x00528a70, 1, node, bn, 1, 0.5f); }
        // 00528940's side switch / node switch / +1 edge branches
        { Poke l1(bn + 0x14, 0xffffffffu); lineoff(node, bn, 1); }
        { Poke l1(bn + 0x14, 1); Poke l2(bn + 0x18, node); lineoff(node, bn, 1); }
        { Poke l1(bn + 0x14, 1); Poke l2(bn + 0x18, run[nrun > 1 ? 1 : 0]); lineoff(node, bn, 1); }
        // mode 1: the corner offset (the node's 0x400 clear) / the line offset (set)
        nav(0x005278e0, bn);
        { PokeBits pf(node + koff, 0x400u, 0); nav(0x005278e0, bn); }
        // the 3D look-ahead branch: the vehicle at the corner node (distance 0 < the tuning)
        { Poke px(veh + 0xc0, rd(node + 0)); Poke pz(veh + 0xc8, rd(node + 8)); nav(0x005278e0, bn); }
        // the plain look point: the vehicle far ahead of the corner node (distance > 0xa04958)
        {
            Poke px(veh + 0xc0, rd(node + 0) + 100 * rd(node + 0x50));
            Poke pz(veh + 0xc8, rd(node + 8) + 100 * rd(node + 0x58));
            nav(0x005278e0, bn);
        }
        // the +0x28 tails
        { Poke ps(bn + 0x28, 1); nav(0x005278e0, bn); }
        { Poke ps(bn + 0x28, 0xffffffffu); nav(0x005278e0, bn); }
        // the hard turn: the heading against the corner direction, the vehicle on the right / left edge (lateral 1 / 0)
        hardturn(0x005278e0, 0x20, 0x28);
        hardturn(0x005278e0, 0x10, 0x18);
        // the driver's forced side (+0x310) and the linked line (+0x30 + 0x7c 2 / 3)
        { Poke ps1(drv + 0x310, 1); nav(0x005278e0, bn); }
        { Poke ps1(drv + 0x310, 0xffffffffu); nav(0x005278e0, bn); }
        {
            Poke pl30(bn + 0x30, bn);
            PokeBits lk(bn + 0x7c, 2, 0);
            nav(0x005278e0, bn);
            { Poke fs(drv + 0x310, 1); nav(0x005278e0, bn); }          /* forced + linked: neither branch */
            { Poke fs(drv + 0x310, 0xffffffffu); nav(0x005278e0, bn); }
        }
        // the take-line ready branch (+0x2c): its own fields, the driver's lane / heading and the speed
        {
            Poke tl0(bn + 0x2c, bn);
            Poke tl1(bn + 0x14, 0);
            Poke tl2(drv + 0x314, 1);
            Poke tl3(bn + 0x1c, 0xc2c80000u /* -100.0f: the driver's heading x is above it */);
            Poke sp(veh + 0x150, rx_oracle::bits(100.0f));
            nav(0x005278e0, bn);
        }
        // mode 2: a corner node (0x200) with the previous node's 0x400 clear / set, then a plain node with it clear / set
        Poke p14(mon + 0xc, 2);
        Poke p15(bn + 0x10, 0);
        {
            PokeMany cm; for (int i = 0; i < nrun; i++) cm.add(run[i] + koff, 0x200u, 0);
            { PokeBits pp(prev + koff, 0, 0x400u); nav(0x00527fc0, bn); }
            { PokeBits pp(prev + koff, 0x400u, 0); nav(0x00527fc0, bn); }
        }
        {
            PokeMany pm; for (int i = 0; i < nrun; i++) pm.add(run[i] + koff, 0, 0x200u);
            { PokeBits pp(prev + koff, 0, 0x400u); nav(0x00527fc0, bn); }
            { PokeBits pp(prev + koff, 0x400u, 0); nav(0x00527fc0, bn); }
        }
        // the side lock: +0x10 nonzero, the speed below / above the threshold (0x9cd9b8), the run distance
        { Poke plk(bn + 0x10, 0xffffffffu); Poke sp(veh + 0x150, 0); nav(0x00527fc0, bn); }
        { Poke plk(bn + 0x10, 0xffffffffu); Poke sp(veh + 0x150, rx_oracle::bits(100.0f)); nav(0x00527fc0, bn); }
        // the +0x28 tails and the hard turn
        { Poke p16(bn + 0x28, 1); nav(0x00527fc0, bn); }
        { Poke p17(bn + 0x28, 0xffffffffu); nav(0x00527fc0, bn); }
        {
            Poke ph1(veh + 0x130, rd(node + 0x50) ^ 0x80000000u);
            Poke ph2(veh + 0x138, rd(node + 0x58) ^ 0x80000000u);
            Poke pxx(veh + 0xc0, rd(node + 0x20));
            Poke pzz(veh + 0xc8, rd(node + 0x28));
            nav(0x00527fc0, bn);
        }
        // the inlined side pick's forced / linked branches (the original's pass)
        { Poke fs(drv + 0x310, 1); nav(0x00527fc0, bn); }
        { Poke fs(drv + 0x310, 0xffffffffu); nav(0x00527fc0, bn); }
        {
            Poke pl30(bn + 0x30, bn);
            PokeBits lk(bn + 0x7c, 3, 0);
            nav(0x00527fc0, bn);
            { Poke fs(drv + 0x310, 1); nav(0x00527fc0, bn); }
        }
        // the airborne gate: the bike's vf43 word (+0x8d98) set, with and without the +0x38 linked behaviour
        {
            const uint32_t sim = rd(veh + 0x234);
            PokeMany air; if (sim) air.add(sim + 0x8d98, 1, 0);
            { PokeBits no38(bn + 0x38, 0, 0); nav(0x00527fc0, bn); }
            { Poke o38(bn + 0x38, bn); PokeBits o4c(bn + 0x4c, 4, 0); nav(0x00527fc0, bn); }
            { Poke o38(bn + 0x38, bn); PokeBits o4c(bn + 0x4c, 0, 4); nav(0x00527fc0, bn); }
        }
        // the take-line ready branch (+0x2c) of the inlined side pick
        {
            Poke tl0(bn + 0x2c, bn);
            Poke tl1(bn + 0x14, 0);
            Poke tl2(drv + 0x314, 1);
            Poke tl3(bn + 0x1c, 0xc2c80000u /* -100.0f */);
            Poke sp(veh + 0x150, rx_oracle::bits(100.0f));
            nav(0x00527fc0, bn);
        }
        // the hard turns (right / left edge) and the vehicle back at its own position for the rest
        hardturn(0x00527fc0, 0x20, 0x28);
        hardturn(0x00527fc0, 0x10, 0x18);
        // the |heading . target| second test: a heading along / across the target's world direction, steer + / -
        {
            const float tx0 = rd(node + 0) ? rx_oracle::f32(rd(node + 0)) : 1.0f;
            const float tz0 = rx_oracle::f32(rd(node + 8));
            Poke h1(veh + 0x130, rx_oracle::bits(tx0));
            Poke h2(veh + 0x138, rx_oracle::bits(tz0));
            Poke s1(drv + 0x60, rx_oracle::bits(1.0f));
            nav(0x00527fc0, bn);
            { Poke s2(drv + 0x60, rx_oracle::bits(-1.0f)); nav(0x00527fc0, bn); }
        }
        {
            const float tx0 = -rx_oracle::f32(rd(node + 8));
            const float tz0 = rx_oracle::f32(rd(node + 0));
            Poke h1(veh + 0x130, rx_oracle::bits(tx0));
            Poke h2(veh + 0x138, rx_oracle::bits(tz0));
            Poke s1(drv + 0x60, rx_oracle::bits(1.0f));
            nav(0x00527fc0, bn);
            { Poke s2(drv + 0x60, rx_oracle::bits(-1.0f)); nav(0x00527fc0, bn); }
        }
        // ... and eight headings around the direction to the node (the steering push stays, |dot| crosses 0x931da8)
        {
            const double vx = rx_oracle::f32(rd(veh + 0xc0)), vz = rx_oracle::f32(rd(veh + 0xc8));
            const double dx = rx_oracle::f32(rd(node + 0)) - vx, dz = rx_oracle::f32(rd(node + 8)) - vz;
            for (int k = 0; k < 8; k++) {
                const double a = k * 0.39269908169872414;   /* pi / 8 */
                const double c = std::cos(a), s = std::sin(a);
                Poke h1(veh + 0x130, rx_oracle::bits(float(c * dx - s * dz)));
                Poke h2(veh + 0x138, rx_oracle::bits(float(s * dx + c * dz)));
                Poke s1(drv + 0x60, rx_oracle::bits(1.0f));
                nav(0x00527fc0, bn);
                { Poke s2(drv + 0x60, rx_oracle::bits(-1.0f)); nav(0x00527fc0, bn); }
            }
        }
    }
    std::fprintf(stderr, "[ainav] navigate probe: %d calls\n", calls);
}

// ---- group c: BehaviorMonitorCorner states 1 / 2 (00526ab0 / 00526b00) ----
// The driver loop reaches state 1 after watch() (00526a30) found a corner ahead and state 2 once the entry node is behind it; no
// reference world updates the AI drivers, so the probe sets the corner state on the live monitor behaviour (and the driver's nodes)
// and calls the originals there. Every call is a per-call Verify of its core port.
void probe_monitor() {
    const uint32_t g = g_game();
    if (!g) return;
    const uint32_t nslots = uint8_t(rx_rd8(g + 0x1541c));
    uint32_t mon = 0, drv = 0;
    for (uint32_t s = 0; s < nslots && s < 16 && !mon; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (!mapped(p, 0x12a0)) continue;
        const uint32_t act = g_activity(p);
        if (!act) continue;
        const int32_t n = int32_t(rd(act + 0x26c));
        for (int32_t i = 0; i < n && i < 32 && !mon; i++) {
            const uint32_t d = rd(rd(act + 0x268) + 4 * uint32_t(i));
            if (!d) continue;
            for (uint32_t k = 0; k < rd(d + 0xee0) && k < 40; k++) {
                const uint32_t b = rd(d + 0xe8 + 4 * k);
                if (b && rd(b) == 0x0093f9c8u) { mon = b; drv = d; break; }   // BehaviorMonitorCorner
            }
        }
    }
    if (!mon) { std::fprintf(stderr, "[aimon] no monitor-corner behaviour\n"); return; }
    const uint32_t veh = rd(rd(drv + 0x74) + 0x1ac);
    const uint32_t node = rd(drv + 0x1c), prev = rd(drv + 0x18);
    if (!veh || !node || !prev) { std::fprintf(stderr, "[aimon] monitor %08x driver %08x node %08x prev %08x veh %08x (incomplete)\n", mon, drv, node, prev, veh); return; }
    const uint32_t koff = 4u * (rd(veh + 0x198) + 4u * rd(veh + 0x194)) + 0x68u;
    const uint32_t idx = rd(rd(drv + 0x74) + 0x8f4) + (rd(drv + 0x328) * 3u + rd(drv + 0x32c)) * 4u;
    std::fprintf(stderr, "[aimon] monitor %08x driver %08x veh %08x node %08x prev %08x sec %08x koff %x state %d corner %08x last %08x dir %d len %08x;"
                         " tun i %u: 9e44d8 %08x\n",
                 mon, drv, veh, node, prev, rd(drv + 0x14), koff, int(rd(mon + 0xc)), rd(mon + 0x14), rd(mon + 0x18),
                 int(rd(mon + 0x1c)), rd(mon + 0x20), idx, rd(0x9e44d8u + 4u * idx));
    int calls = 0;
    auto state1 = [&]() { rx_regs in = regs(); in.r[0] = mon; call(0x00526ab0, in); ++calls; };
    auto state2 = [&]() { rx_regs in = regs(); in.r[1] = mon; call(0x00526b00, in); ++calls; };
    // 00526ab0: the corner's entry node is the driver's current node -> state 2 at once (the je)
    {
        Poke p0(mon + 0xc, 1);
        Poke p1(mon + 0x14, prev);
        Poke p2(drv + 0x18, prev);
        state1();
        std::fprintf(stderr, "[aimon] 00526ab0 entry == current node -> state %d\n", int(rd(mon + 0xc)));
    }
    // ... entry node behind it but the corner still within the tuning distance ahead -> stays in state 1 (the call's nonzero result)
    {
        uint32_t n2 = node;
        if (!rd(n2 + 0x64)) n2 = rd(prev + 0x64) ? prev : 0;   // a start node with a successor (the flag test needs one)
        if (n2) {
            Poke p0(mon + 0xc, 1);
            Poke p1(mon + 0x14, prev);
            Poke p2(drv + 0x18, 0);
            Poke p3(drv + 0x1c, n2);
            PokeBits pf(n2 + koff, 0x200u, 0);          // the driver's next node is a corner for this vehicle class
            state1();
            std::fprintf(stderr, "[aimon] 00526ab0 corner %08x ahead -> state %d\n", n2, int(rd(mon + 0xc)));
        } else {
            std::fprintf(stderr, "[aimon] 00526ab0 corner-ahead branch: no node with a successor\n");
        }
    }
    // ... no corner within the distance (the search starts at the run's last node) -> state 2 (the search's zero result)
    {
        uint32_t last = node, q = node;
        for (int i = 0; i < 4096 && q; i++) { last = q; q = rd(q + 0x64); }
        Poke p0(mon + 0xc, 1);
        Poke p1(mon + 0x14, prev);
        Poke p2(drv + 0x18, 0);
        Poke p3(drv + 0x1c, last);
        state1();
        std::fprintf(stderr, "[aimon] 00526ab0 no corner in reach -> state %d\n", int(rd(mon + 0xc)));
    }
    // 00526b00: the current node is still a corner -> state 2 kept (the jne)
    {
        Poke p0(mon + 0xc, 2);
        PokeBits pf(prev + koff, 0x200u, 0);
        state2();
        std::fprintf(stderr, "[aimon] 00526b00 corner node -> state %d corner %08x\n", int(rd(mon + 0xc)), rd(mon + 0x14));
    }
    // ... the run's first plain node -> the behaviour's vf6 runs (00526950 clears +0x0c .. +0x20)
    {
        Poke p0(mon + 0xc, 2);
        Poke p1(mon + 0x10, rd(drv + 0x14));
        Poke p2(mon + 0x14, node);
        PokeBits clr(prev + koff, 0, 0x200u);
        state2();
        std::fprintf(stderr, "[aimon] 00526b00 plain node -> state %d corner %08x last %08x dir %d len %08x\n", int(rd(mon + 0xc)),
                     rd(mon + 0x14), rd(mon + 0x18), int(rd(mon + 0x1c)), rd(mon + 0x20));
    }
    std::fprintf(stderr, "[aimon] monitor probe: %d calls\n", calls);
}

// ---- group b: BehaviorHandleBraking modes 1 / 2 (00524a40 / 00524af0) and their node-side helper 00526dd0 ----
// The driver loop picks the mode from the behaviour's +0x4c object (+0x0c); at the ride's stop it was 1. The probe sets the cast
// count / recorded sides / edge side on a live braking behaviour (and its +0x4c / +0x50 objects) and calls the originals there.
void probe_braking() {
    const uint32_t g = g_game();
    if (!g) return;
    const uint32_t nslots = uint8_t(rx_rd8(g + 0x1541c));
    uint32_t bb = 0, drv = 0;
    for (uint32_t s = 0; s < nslots && s < 16 && !bb; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (!mapped(p, 0x12a0)) continue;
        const uint32_t act = g_activity(p);
        if (!act) continue;
        const int32_t n = int32_t(rd(act + 0x26c));
        for (int32_t i = 0; i < n && i < 32 && !bb; i++) {
            const uint32_t d = rd(rd(act + 0x268) + 4 * uint32_t(i));
            if (!d) continue;
            for (uint32_t k = 0; k < rd(d + 0xee0) && k < 40; k++) {
                const uint32_t b = rd(d + 0xe8 + 4 * k);
                if (b && rd(b) == 0x009401a8u) { bb = b; drv = d; break; }   // BehaviorHandleBraking
            }
        }
    }
    if (!bb) { std::fprintf(stderr, "[aimon] no braking behaviour\n"); return; }
    const uint32_t m = rd(bb + 0x4c), e50 = rd(bb + 0x50), veh = rd(rd(drv + 0x74) + 0x1ac);
    if (!m || !e50 || !veh) { std::fprintf(stderr, "[aimon] braking %08x mode %08x edge %08x veh %08x (incomplete)\n", bb, m, e50, veh); return; }
    const uint32_t d2 = rd(m + 8), node = rd(d2 + 0x18), veh2 = rd(rd(d2 + 0x74) + 0x1ac);
    std::fprintf(stderr, "[aimon] braking %08x driver %08x mode %08x (driver %08x +0xc %d +0x1c %d) edge %08x (+0xc %d) driver2 %08x node %08x veh2 %08x"
                         " count %u sides %08x %08x fracs %08x %08x meat %08x side %d thr %08x\n",
                 bb, drv, m, d2, int(rd(m + 0xc)), int(rd(m + 0x1c)), e50, int(rd(e50 + 0xc)), d2, node, veh2,
                 rd(bb + 0x10), rd(bb + 0x18), rd(bb + 0x20), rd(bb + 0x14), rd(bb + 0x1c), rd(bb + 0x40), int(rd(bb + 0x44)),
                 rd(drv + 0x64));
    int calls = 0;
    auto cast1 = [&]() { rx_regs in = regs(); in.r[0] = bb; call(0x00524a40, in); ++calls; };
    auto cast2 = [&]() { rx_regs in = regs(); in.r[0] = bb; call(0x00524af0, in); ++calls; };
    auto side_of = [&](uint32_t obj, int32_t sd) {
        const uint32_t a[1] = {uint32_t(sd)};
        rx_regs in = regs(); in.r[0] = obj;
        const rx_regs o = call(0x00526dd0, in, 1, a);
        ++calls;
        return o.r[0];
    };
    // 00526dd0 on its own: the side multiplies the cross product, so +side / -side give the two outcomes
    Poke p1(m + 0x1c, 1);
    const uint32_t s_plus = side_of(m, 1);
    Poke p2(m + 0x1c, 0);
    const uint32_t s_zero = side_of(m, 0);
    Poke p3(m + 0x1c, 0xffffffffu);
    const uint32_t s_minus = side_of(m, 0xffffffffu);
    std::fprintf(stderr, "[aimon] 00526dd0 side +1 -> %u, 0 -> %u, -1 -> %u (node %08x veh %08x)\n", s_plus, s_zero, s_minus, node, veh2);
    // the side that makes the test true (for mode 2's centre branch): +1 unless the geometry's cross is negative
    const int32_t past_side = s_plus ? 1 : -1;
    // 00524a40: the two cast sides; count 0 -> 1 and count 1 -> 2 with the same / a different first side; the tails
    {
        Poke c(bb + 0x10, 0);
        Poke sd(e50 + 0xc, 1);
        Poke mo(m + 0x1c, 1);                       // the mode object's side equals the edge side: cast at the centre
        Poke fl(bb + 0x44, 0);
        Poke th(drv + 0x64, 0x3f800000u);
        cast1();
        std::fprintf(stderr, "[aimon] 00524a40 equal sides -> count %u sides %08x %08x fracs %08x %08x meat %08x side %d thr %08x\n",
                     rd(bb + 0x10), rd(bb + 0x18), rd(bb + 0x20), rd(bb + 0x14), rd(bb + 0x1c), rd(bb + 0x40), int(rd(bb + 0x44)), rd(drv + 0x64));
    }
    {
        Poke c(bb + 0x10, 0);
        Poke sd(e50 + 0xc, 1);
        Poke mo(m + 0x1c, 0);                       // different: cast at the edge side
        Poke fl(bb + 0x44, 0);
        Poke th(drv + 0x64, 0x3f800000u);
        cast1();
        std::fprintf(stderr, "[aimon] 00524a40 edge side -> count %u side %08x meat %08x edge %d thr %08x\n",
                     rd(bb + 0x10), rd(bb + 0x18), rd(bb + 0x40), int(rd(bb + 0x44)), rd(drv + 0x64));
    }
    {
        Poke c(bb + 0x10, 1);
        Poke sd(e50 + 0xc, 1);
        Poke mo(m + 0x1c, 1);                       // equal again (recorded 0): first entry 0 -> same sides -> the mean
        Poke f0(bb + 0x14, 0x3f000000u);
        Poke s0(bb + 0x18, 0);
        Poke fl(bb + 0x44, 0);
        Poke th(drv + 0x64, 0x3f800000u);
        cast1();
        std::fprintf(stderr, "[aimon] 00524a40 second cast same side -> count %u meat %08x side %d thr %08x\n",
                     rd(bb + 0x10), rd(bb + 0x40), int(rd(bb + 0x44)), rd(drv + 0x64));
    }
    {
        Poke c(bb + 0x10, 1);
        Poke sd(e50 + 0xc, 1);
        Poke mo(m + 0x1c, 1);
        Poke f0(bb + 0x14, 0x3f000000u);
        Poke s0(bb + 0x18, 1);                      // first entry side 1 vs this cast's 0 -> no common edge (1.01)
        Poke fl(bb + 0x44, 0);
        Poke th(drv + 0x64, 0x3f800000u);
        cast1();
        std::fprintf(stderr, "[aimon] 00524a40 second cast other side -> count %u meat %08x side %d thr %08x\n",
                     rd(bb + 0x10), rd(bb + 0x40), int(rd(bb + 0x44)), rd(drv + 0x64));
    }
    {
        Poke c(bb + 0x10, 0);
        Poke sd(e50 + 0xc, 1);
        Poke mo(m + 0x1c, 0);
        Poke fl(bb + 0x44, 1);                      // a braking edge already set: the tail applies it though the cast's side != 0
        Poke me(bb + 0x40, 0x3f400000u);
        Poke th(drv + 0x64, 0x3f800000u);
        cast1();
        std::fprintf(stderr, "[aimon] 00524a40 edge set -> thr %08x\n", rd(drv + 0x64));
    }
    // 00524af0: past the side -> centre cast; not past -> the edge side; the tails
    {
        Poke c(bb + 0x10, 0);
        Poke sd(e50 + 0xc, 1);
        Poke mo(m + 0x1c, uint32_t(past_side));
        Poke fl(bb + 0x44, 0);
        Poke th(drv + 0x64, 0x3f800000u);
        cast2();
        std::fprintf(stderr, "[aimon] 00524af0 past side %d -> count %u sides %08x %08x meat %08x side %d thr %08x\n", past_side,
                     rd(bb + 0x10), rd(bb + 0x18), rd(bb + 0x20), rd(bb + 0x40), int(rd(bb + 0x44)), rd(drv + 0x64));
    }
    {
        Poke c(bb + 0x10, 0);
        Poke sd(e50 + 0xc, 1);
        Poke mo(m + 0x1c, 0);                       // the side is 0: never past -> cast at the edge side
        Poke fl(bb + 0x44, 0);
        Poke th(drv + 0x64, 0x3f800000u);
        cast2();
        std::fprintf(stderr, "[aimon] 00524af0 not past -> count %u side %08x meat %08x edge %d thr %08x\n",
                     rd(bb + 0x10), rd(bb + 0x18), rd(bb + 0x40), int(rd(bb + 0x44)), rd(drv + 0x64));
    }
    {
        Poke c(bb + 0x10, 1);
        Poke sd(e50 + 0xc, 1);
        Poke mo(m + 0x1c, uint32_t(past_side));     // the centre cast recorded 0; first entry 0 -> same -> the mean
        Poke f0(bb + 0x14, 0x3f000000u);
        Poke s0(bb + 0x18, 0);
        Poke fl(bb + 0x44, 0);
        Poke th(drv + 0x64, 0x3f800000u);
        cast2();
        std::fprintf(stderr, "[aimon] 00524af0 second cast same side -> count %u meat %08x side %d thr %08x\n",
                     rd(bb + 0x10), rd(bb + 0x40), int(rd(bb + 0x44)), rd(drv + 0x64));
    }
    {
        Poke c(bb + 0x10, 1);
        Poke sd(e50 + 0xc, 1);
        Poke mo(m + 0x1c, uint32_t(past_side));
        Poke f0(bb + 0x14, 0x3f000000u);
        Poke s0(bb + 0x18, 1);                      // first entry 1 vs 0 -> 1.01
        Poke fl(bb + 0x44, 0);
        Poke th(drv + 0x64, 0x3f800000u);
        cast2();
        std::fprintf(stderr, "[aimon] 00524af0 second cast other side -> count %u meat %08x side %d thr %08x\n",
                     rd(bb + 0x10), rd(bb + 0x40), int(rd(bb + 0x44)), rd(drv + 0x64));
    }
    {
        Poke c(bb + 0x10, 0);
        Poke sd(e50 + 0xc, 1);
        Poke mo(m + 0x1c, 0);
        Poke fl(bb + 0x44, 1);
        Poke me(bb + 0x40, 0x3f400000u);
        Poke th(drv + 0x64, 0x3f800000u);
        cast2();
        std::fprintf(stderr, "[aimon] 00524af0 edge set -> thr %08x\n", rd(drv + 0x64));
    }
    std::fprintf(stderr, "[aimon] braking probe: %d calls\n", calls);
}
// ---- group f: BehaviorBlueYellowFlag state 1 (00522a70) ----
// No reference world laps a rider (the activity stays at the grid), so the probe sets state 1 on a live blue/yellow flag
// behaviour, picks a live driver as the rider being let past (+0x14) and puts its vehicle at known offsets; the heading is
// pinned so the resets depend on the probe's geometry, not the frame's pose. Every call is a per-call Verify of the port.
void probe_blue_flag() {
    const uint32_t g = g_game();
    if (!g) return;
    const uint32_t nslots = uint8_t(rx_rd8(g + 0x1541c));
    uint32_t fb = 0, drv = 0;
    for (uint32_t s = 0; s < nslots && s < 16 && !fb; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (!mapped(p, 0x12a0)) continue;
        const uint32_t act = g_activity(p);
        if (!act) continue;
        const int32_t n = int32_t(rd(act + 0x26c));
        for (int32_t i = 0; i < n && i < 32 && !fb; i++) {
            const uint32_t d = rd(rd(act + 0x268) + 4 * uint32_t(i));
            if (!d) continue;
            for (uint32_t k = 0; k < rd(d + 0xee0) && k < 40; k++) {
                const uint32_t b = rd(d + 0xe8 + 4 * k);
                if (b && rd(b) == 0x00940248u) { fb = b; drv = d; break; }   // BehaviorBlueYellowFlag
            }
        }
    }
    if (!fb) { std::fprintf(stderr, "[aiflag] no blue/yellow flag behaviour\n"); return; }
    const uint32_t mev = rd(rd(drv + 0x74) + 0x1ac);
    uint32_t tdrv = 0, tv = 0;
    for (uint32_t s = 0; s < nslots && s < 16 && !tdrv; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (!mapped(p, 0x12a0)) continue;
        const uint32_t act = g_activity(p);
        if (!act) continue;
        const int32_t n = int32_t(rd(act + 0x26c));
        for (int32_t i = 0; i < n && i < 32 && !tdrv; i++) {
            const uint32_t d = rd(rd(act + 0x268) + 4 * uint32_t(i));
            if (!d || d == drv) continue;
            const uint32_t v = rd(rd(d + 0x74) + 0x1ac);
            if (v && v != mev) { tdrv = d; tv = v; }
        }
    }
    if (!tv) { std::fprintf(stderr, "[aiflag] flag %08x driver %08x veh %08x: no second driver\n", fb, drv, mev); return; }
    std::fprintf(stderr, "[aiflag] flag %08x driver %08x veh %08x target %08x veh %08x state %d side %d target0 %08x"
                         " steer %08x throttle %08x pos %08x %08x heading %08x %08x %08x\n",
                 fb, drv, mev, tdrv, tv, int(rd(fb + 0xc)), int(rd(fb + 0x10)), rd(fb + 0x14),
                 rd(drv + 0x60), rd(drv + 0x3c0), rd(mev + 0xc0), rd(mev + 0xc8),
                 rd(mev + 0x130), rd(mev + 0x134), rd(mev + 0x138));
    const float vx = rx_oracle::f32(rd(mev + 0xc0)), vz = rx_oracle::f32(rd(mev + 0xc8));
    int calls = 0;
    auto move = [&](const char* what, int32_t side) {
        Poke ps(fb + 0x10, uint32_t(side));
        Poke pt(fb + 0x14, tdrv);            // the reset clears it
        rx_regs in = regs(); in.r[6] = fb;
        call(0x00522a70, in);
        ++calls;
        std::fprintf(stderr, "[aiflag] 00522a70 %-26s side %2d -> state %d target %08x steer %08x throttle %08x\n", what, side,
                     int(rd(fb + 0xc)), rd(fb + 0x14), rd(drv + 0x60), rd(drv + 0x3c0));
    };
    {   // the rider's heading (1, 0, 0) so the heading test is the target's x offset
        Poke h1(mev + 0x130, rx_oracle::bits(1.0f));
        Poke h2(mev + 0x134, rx_oracle::bits(0.0f));
        Poke h3(mev + 0x138, rx_oracle::bits(0.0f));
        Poke t0(fb + 0x14, tdrv);
        {   // 20 units straight ahead: dot 1 > 0.707 and within 45 -> the reset
            Poke px(tv + 0xc0, rx_oracle::bits(vx + 20.0f)); Poke pz(tv + 0xc8, rx_oracle::bits(vz));
            move("ahead / side +1", 1);
        }
        {   // 20 units behind: dot -1 -> the state kept (-0.75)
            Poke px(tv + 0xc0, rx_oracle::bits(vx - 20.0f)); Poke pz(tv + 0xc8, rx_oracle::bits(vz));
            move("behind / side -1", -1);
        }
        {   // 20 units to the side: dot 0 -> the state kept, no steering (side 0)
            Poke px(tv + 0xc0, rx_oracle::bits(vx)); Poke pz(tv + 0xc8, rx_oracle::bits(vz + 20.0f));
            move("beside / side 0", 0);
        }
        {   // 50 units ahead: 2500 > 2025 -> the reset by distance (and an unmatched side value)
            Poke px(tv + 0xc0, rx_oracle::bits(vx + 50.0f)); Poke pz(tv + 0xc8, rx_oracle::bits(vz));
            move("50 ahead / side 2", 2);
        }
        {   // the same position: d2 = 0 (no normalization) -> dot 0, the state kept
            Poke px(tv + 0xc0, rx_oracle::bits(vx)); Poke pz(tv + 0xc8, rx_oracle::bits(vz));
            move("same position / side +1", 1);
        }
        {   // heading y = inf: the y term is heading y x 0 = NaN -> dot NaN, comiss / jbe keeps the state (the term is 0, not dropped)
            Poke hy(mev + 0x134, 0x7f800000u);
            Poke px(tv + 0xc0, rx_oracle::bits(vx + 20.0f)); Poke pz(tv + 0xc8, rx_oracle::bits(vz));
            move("ahead / heading y inf", 1);
        }
        {   // 20 units ahead with a NaN heading x: dot NaN -> the state kept
            Poke hx(mev + 0x130, 0x7fc00000u);
            Poke px(tv + 0xc0, rx_oracle::bits(vx + 20.0f)); Poke pz(tv + 0xc8, rx_oracle::bits(vz));
            move("ahead / heading x NaN", 1);
        }
    }
    {   // the vf4's state 1 call site (005225c0 -> 00522a70) through the original's own frame (the switch's depth)
        Poke u0(mev + 0x144, rx_oracle::bits(1.0f));
        Poke st(fb + 0xc, 1);
        Poke ps(fb + 0x10, 1);
        Poke pt(fb + 0x14, tdrv);
        Poke px(tv + 0xc0, rx_oracle::bits(vx - 20.0f)); Poke pz(tv + 0xc8, rx_oracle::bits(vz));
        const uint32_t a[1] = { rx_oracle::bits(1.0f / 60.0f) };
        rx_regs in = regs(); in.r[1] = fb;
        call(0x005225c0, in, 1, a);
        ++calls;
        std::fprintf(stderr, "[aiflag] 005225c0 state 1 -> state %d target %08x steer %08x throttle %08x\n", int(rd(fb + 0xc)),
                     rd(fb + 0x14), rd(drv + 0x60), rd(drv + 0x3c0));
    }
    std::fprintf(stderr, "[aiflag] blue flag probe: %d calls\n", calls);
}
// ---- group g: BehaviorFixForwardProgress's manoeuvres (00523a70 / 00523ca0 / 00523ed0 / 005240d0 / 00524050) ----
// The driver loop reaches these only when a rider is stuck (throttle asked, speed under the limit for the timeout); the probe
// sets the positions / headings / race type / activity kind on a live behaviour and calls the originals there.
void probe_fix_progress() {
    const uint32_t g = g_game();
    if (!g) return;
    const uint32_t nslots = uint8_t(rx_rd8(g + 0x1541c));
    uint32_t fb = 0, drv = 0;
    for (uint32_t s = 0; s < nslots && s < 16 && !fb; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (!mapped(p, 0x12a0)) continue;
        const uint32_t act = g_activity(p);
        if (!act) continue;
        const int32_t n = int32_t(rd(act + 0x26c));
        for (int32_t i = 0; i < n && i < 32 && !fb; i++) {
            const uint32_t d = rd(rd(act + 0x268) + 4 * uint32_t(i));
            if (!d) continue;
            for (uint32_t k = 0; k < rd(d + 0xee0) && k < 40; k++) {
                const uint32_t b = rd(d + 0xe8 + 4 * k);
                if (b && rd(b) == 0x0093fbbcu) { fb = b; drv = d; break; }   // BehaviorFixForwardProgress
            }
        }
    }
    if (!fb) { std::fprintf(stderr, "[aifix] no fix-forward-progress behaviour\n"); return; }
    const uint32_t veh = rd(rd(drv + 0x74) + 0x1ac), act = rd(rd(drv + 0x74) + 0x94);
    uint32_t node = rd(fb + 0x5c);
    if (!node) node = rd(drv + 0x1c);
    const uint32_t cur = rd(drv + 0x18);
    if (!veh || !act || !node || !cur) { std::fprintf(stderr, "[aifix] fix %08x driver %08x veh %08x act %08x node %08x cur %08x (incomplete)\n",
                                                    fb, drv, veh, act, node, cur); return; }
    Poke pn(fb + 0x5c, node);
    const float vx = rx_oracle::f32(rd(veh + 0xc0)), vy = rx_oracle::f32(rd(veh + 0xc4)), vz = rx_oracle::f32(rd(veh + 0xc8));
    std::fprintf(stderr, "[aifix] fix %08x driver %08x veh %08x act %08x kind %d node %08x cur %08x stage %d type %d"
                         " sides %d %d pos %08x %08x %08x\n",
                 fb, drv, veh, act, int(rd(act + 0x23c)), node, cur, int(rd(fb + 0x18)), int(rd(fb + 0x60)),
                 int(rd(fb + 0x54)), int(rd(fb + 0x58)), rd(veh + 0xc0), rd(veh + 0xc4), rd(veh + 0xc8));
    int calls = 0;
    // 00524050 (EAX = behaviour) -> EAX: the activity kinds of the jump table, both answers
    auto race_type = [&](uint32_t kind) {
        Poke pk(act + 0x23c, kind);
        rx_regs in = regs(); in.r[0] = fb;
        const rx_regs o = call(0x00524050, in);
        ++calls;
        std::fprintf(stderr, "[aifix] 00524050 kind %2u -> race type %d\n", kind, int(o.r[0]));
    };
    race_type(1); race_type(5); race_type(0x13); race_type(0x14);
    // 00523a70 (ECX = position, EDX = side, stack behaviour; RET 4) -> EAX
    auto steer_away = [&](const char* what, uint32_t pos, int32_t side) {
        const uint32_t a[1] = { fb };
        rx_regs in = regs(); in.r[1] = pos; in.r[2] = uint32_t(side);
        const rx_regs o = call(0x00523a70, in, 1, a);
        ++calls;
        std::fprintf(stderr, "[aifix] 00523a70 %-28s side %2d -> %d steer %08x thr %08x\n", what, side,
                     int(o.r[0]), rd(drv + 0x60), rd(drv + 0x3c0));
    };
    // 00523ca0 (EAX = position, ESI = behaviour, stack side; RET 4) -> EAX
    auto back_up = [&](const char* what, uint32_t pos, int32_t side) {
        const uint32_t a[1] = { uint32_t(side) };
        rx_regs in = regs(); in.r[0] = pos; in.r[6] = fb;
        const rx_regs o = call(0x00523ca0, in, 1, a);
        ++calls;
        std::fprintf(stderr, "[aifix] 00523ca0 %-28s side %2d -> %d steer %08x\n", what, side, int(o.r[0]), rd(drv + 0x60));
    };
    // 00523ed0 / 005240d0 (ESI = behaviour) -> EAX
    auto side_of = [&]() {
        rx_regs in = regs(); in.r[6] = fb;
        const rx_regs o = call(0x00523ed0, in);
        ++calls;
        return o.r[0];
    };
    auto near_of = [&]() {
        rx_regs in = regs(); in.r[6] = fb;
        const rx_regs o = call(0x005240d0, in);
        ++calls;
        return o.r[0];
    };
    {   // the heading (1, 0, 0) and the current node's direction the same, so the tests are the probe's geometry
        Poke h1(veh + 0x130, rx_oracle::bits(1.0f));
        Poke h2(veh + 0x134, rx_oracle::bits(0.0f));
        Poke h3(veh + 0x138, rx_oracle::bits(0.0f));
        Poke n1(cur + 0x50, rx_oracle::bits(1.0f));
        Poke n2(cur + 0x54, rx_oracle::bits(0.0f));
        Poke n3(cur + 0x58, rx_oracle::bits(0.0f));
        Poke px(fb + 0x40, rx_oracle::bits(vx + 40.0f)); Poke pz(fb + 0x48, rx_oracle::bits(vz));
        Poke py(fb + 0x44, rx_oracle::bits(vy));
        Poke t1(fb + 0x60, 1);
        steer_away("+40 / type 1 / side 0", fb + 0x40, 0);
        { Poke q(fb + 0x40, rx_oracle::bits(vx + 20.0f)); steer_away("+20 / type 1 / side 0", fb + 0x40, 0); }
        steer_away("+20 / type 1 / side 1", fb + 0x40, 1);
        { Poke q(fb + 0x40, rx_oracle::bits(vx - 20.0f)); steer_away("-20 / type 1 / side 1", fb + 0x40, 1); }
        { Poke q(fb + 0x40, rx_oracle::bits(vx + 3.0f)); steer_away("+3 / type 1 / side 1", fb + 0x40, 1); }
        { Poke q(fb + 0x40, 0x7fc00000u); steer_away("NaN / type 1 / side -1", fb + 0x40, -1); }
        {
        Poke t2(fb + 0x60, 0);
        { Poke q(fb + 0x40, rx_oracle::bits(vx + 40.0f)); steer_away("+40 / type 0 / side 0", fb + 0x40, 0); }
        { Poke q(fb + 0x40, rx_oracle::bits(vx + 20.0f)); steer_away("+20 / type 0 / side 1", fb + 0x40, 1); }
        { Poke q(fb + 0x40, rx_oracle::bits(vx)); Poke r(fb + 0x48, rx_oracle::bits(vz + 20.0f)); steer_away("sideways / type 0", fb + 0x40, 1); }
        { Poke q(fb + 0x40, rx_oracle::bits(vx + 12.0f)); Poke r(fb + 0x48, rx_oracle::bits(vz)); steer_away("+12 / type 0 / side 1", fb + 0x40, 1); }
        { Poke hx(veh + 0x130, 0x7fc00000u); Poke q(fb + 0x40, rx_oracle::bits(vx + 20.0f)); steer_away("+20 / type 0 / heading NaN", fb + 0x40, 1); }
        }
        // 00523ca0: 100 behind -> 1, 10 behind -> 0, ahead / sideways / no heading -> 0
        { Poke q(fb + 0x40, rx_oracle::bits(vx - 100.0f)); Poke r(fb + 0x48, rx_oracle::bits(vz)); back_up("-100", fb + 0x40, 1); }
        { Poke q(fb + 0x40, rx_oracle::bits(vx - 10.0f)); back_up("-10", fb + 0x40, 1); }
        { Poke q(fb + 0x40, rx_oracle::bits(vx + 100.0f)); back_up("+100", fb + 0x40, -1); }
        { Poke q(fb + 0x40, rx_oracle::bits(vx)); Poke r(fb + 0x48, rx_oracle::bits(vz + 100.0f)); back_up("+100 sideways", fb + 0x40, 1); }
        { Poke h1b(veh + 0x130, rx_oracle::bits(0.0f)); Poke h3b(veh + 0x138, rx_oracle::bits(0.0f));
          Poke q(fb + 0x40, rx_oracle::bits(vx - 100.0f)); Poke r(fb + 0x48, rx_oracle::bits(vz)); back_up("-100 / no heading", fb + 0x40, 1); }
        { Poke hx(veh + 0x130, 0x7fc00000u); Poke q(fb + 0x40, rx_oracle::bits(vx - 100.0f)); back_up("-100 / heading NaN", fb + 0x40, 1); }
        // 00523ed0: the +0x5c object's direction (0, 0) and edge point; the vehicle at (10, 0)
        { Poke v1(veh + 0xc0, rx_oracle::bits(10.0f)); Poke v3(veh + 0xc8, rx_oracle::bits(0.0f));
          Poke d1(node + 0x50, rx_oracle::bits(0.0f)); Poke d3(node + 0x58, rx_oracle::bits(0.0f));
          auto edge = [&](const char* what, float ex, float ez) {
              Poke e1(node + 0x20, rx_oracle::bits(ex)); Poke e3(node + 0x28, rx_oracle::bits(ez));
              const uint32_t r = side_of();
              std::fprintf(stderr, "[aifix] 00523ed0 %-20s edge %g %g -> %d sides %d %d\n", what, ex, ez, int(int32_t(r)),
                           int(rd(fb + 0x54)), int(rd(fb + 0x58)));
          };
          edge("s>0 t<0", 1.0f, 1.0f);
          edge("s>0 t 0", 1.0f, 0.0f);
          edge("s 0 t>0", 0.0f, -1.0f);
          edge("s<0 t<0", -1.0f, 1.0f);
          edge("s NaN", std::nanf(""), 0.0f);
        }
        // 005240d0: the type-6 proximity (kind 6, the object's direction at its +0x50 / +0x54 / +0x58)
        { Poke v1(veh + 0xc0, rx_oracle::bits(vx)); Poke v2(veh + 0xc4, rx_oracle::bits(vy)); Poke v3(veh + 0xc8, rx_oracle::bits(vz));
          Poke d1(node + 0x50, rx_oracle::bits(vx + 50.0f)); Poke d2(node + 0x54, rx_oracle::bits(vy)); Poke d3(node + 0x58, rx_oracle::bits(vz));
          Poke pk(act + 0x23c, 6);
          std::fprintf(stderr, "[aifix] 005240d0 kind 6 near -> %d\n", int(near_of()));
          { Poke f1(node + 0x50, rx_oracle::bits(vx + 200.0f)); std::fprintf(stderr, "[aifix] 005240d0 kind 6 far -> %d\n", int(near_of())); }
          { Poke pk2(act + 0x23c, 1); std::fprintf(stderr, "[aifix] 005240d0 kind 1 -> %d\n", int(near_of())); }
        }
        // the vf4's manoeuvre call sites (00523570 -> 00523a70 / 00523ca0) through the original's own frame
        {
            Poke pa(act + 0x21c, 2);
            Poke pe(fb + 4, 1);
            Poke q(fb + 0x40, rx_oracle::bits(vx - 100.0f)); Poke r(fb + 0x48, rx_oracle::bits(vz));
            Poke s1(fb + 0x58, 1);
            Poke st(fb + 0x18, 1);
            const uint32_t a[1] = { rx_oracle::bits(1.0f / 60.0f) };
            rx_regs in = regs(); in.r[1] = fb;
            call(0x00523570, in, 1, a);
            ++calls;
            std::fprintf(stderr, "[aifix] 00523570 stage 1 -> stage %d steer %08x thr %08x\n", int(rd(fb + 0x18)), rd(drv + 0x60), rd(drv + 0x3c0));
        }
        {
            Poke pa(act + 0x21c, 2);
            Poke pe(fb + 4, 1);
            Poke s2(fb + 0x54, 1);
            Poke st(fb + 0x18, 2);
            const uint32_t a[1] = { rx_oracle::bits(1.0f / 60.0f) };
            rx_regs in = regs(); in.r[1] = fb;
            call(0x00523570, in, 1, a);
            ++calls;
            std::fprintf(stderr, "[aifix] 00523570 stage 2 -> stage %d steer %08x\n", int(rd(fb + 0x18)), rd(drv + 0x60));
        }
        // The second manoeuvre set uses +0x30 / +0x50 and has separate direct call sites in vf4.
        for (uint32_t stage = 1; stage <= 2; ++stage) {
            Poke pa(act + 0x21c, 2), pe(fb + 4, 1), pv(veh + 0x280, 0);
            Poke throttle(drv + 0x3c0, 0), primary(fb + 0x18, 0), secondary(fb + 0x14, stage);
            Poke sx(fb + 0x30, rx_oracle::bits(vx - 100.0f)), sy(fb + 0x34, rx_oracle::bits(vy));
            Poke sz(fb + 0x38, rx_oracle::bits(vz)), side(fb + 0x50, 1);
            const uint32_t a[1] = { rx_oracle::bits(1.0f / 60.0f) };
            rx_regs in = regs(); in.r[1] = fb;
            call(0x00523570, in, 1, a);
            ++calls;
            std::fprintf(stderr, "[aifix] 00523570 secondary %u -> stage %d\n", stage, int(rd(fb + 0x14)));
        }
        // A stuck type-6 driver takes the proximity and side-selection call sites before steering away.
        {
            Poke pa(act + 0x21c, 2), pk(act + 0x23c, 6), pe(fb + 4, 1), pv(veh + 0x280, 0);
            Poke type(fb + 0x60, 0), primary(fb + 0x18, 0), secondary(fb + 0x14, 0);
            Poke throttle(drv + 0x3c0, rx_oracle::bits(1.0f)), limit(fb + 0x10, rx_oracle::bits(1.0f));
            Poke clock(drv + 0x130, rx_oracle::bits(100.0f)), deadline(fb + 0x20, rx_oracle::bits(99.0f));
            Poke v1(veh + 0x160, 0), v2(veh + 0x164, 0), v3(veh + 0x168, 0);
            Poke n1(node + 0x50, rx_oracle::bits(vx + 50.0f)), n2(node + 0x54, rx_oracle::bits(vy));
            Poke n3(node + 0x58, rx_oracle::bits(vz)), ex(node + 0x20, rx_oracle::bits(1.0f)), ez(node + 0x28, 0);
            const uint32_t a[1] = { rx_oracle::bits(1.0f / 60.0f) };
            rx_regs in = regs(); in.r[1] = fb;
            call(0x00523570, in, 1, a);
            ++calls;
            std::fprintf(stderr, "[aifix] 00523570 stuck type 6 -> stage %d sides %d %d\n", int(rd(fb + 0x18)),
                         int(rd(fb + 0x54)), int(rd(fb + 0x58)));
        }
    }
    std::fprintf(stderr, "[aifix] fix progress probe: %d calls\n", calls);
}
}  // namespace

RX_PORT(0x0084ed20, RidePathProbeHook, "thunk", 0) {
    static const char* groups = std::getenv("RX_RIDE_PROBE");
    if (!groups || !*groups) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_RIDE_PROBE_FRAME") ? uint32_t(std::atoi(std::getenv("RX_RIDE_PROBE_FRAME"))) : 250;
    ++g_frame;
    if (g_frame != at) return reflex_vm::RUN_ORIGINAL;
    const Saved saved(m);
    g_m = &m;
    if (std::strchr(groups, 'i')) info();
    if (std::strchr(groups, 'o')) probe_offtrack();
    if (std::strchr(groups, 'r')) probe_reset();
    if (std::strchr(groups, 'n')) probe_navigate();
    if (std::strchr(groups, 'c')) probe_monitor();
    if (std::strchr(groups, 'b')) probe_braking();
    if (std::strchr(groups, 'f')) probe_blue_flag();
    if (std::strchr(groups, 'g')) probe_fix_progress();
    saved.restore(m);
    std::fprintf(stderr, "[ridepath] frame %u done\n", g_frame);
    return reflex_vm::RUN_ORIGINAL;
}
