/* trickland_probe.cpp -- the oracle A/B for the trick landing (agent "trickland"): does the ORIGINAL game wreck when the
 * rider lands while the 9 O'Clock Nac trick animation is running? Development probe only (the misc_probe device).
 *
 * The five reference worlds are driven by keys, never by a pad, so no reference run starts a trick. The probe therefore
 *   - scripts pad 0 for the reference world exactly like the product ride's `trickair` input (W held by REFLEX_HOLD_W, LB held
 *     and the right stick walked "323" while the tracker says airborne, released once the trick starts): the game's own gesture
 *     classifier, matcher, trick start and playback run in the ORIGINAL;
 *   - prints per frame the state the product ride prints (tracker mode / current trick, the rider's trick state +0xaf10 and the
 *     clip time, the vehicle wreck word +0x280, the wreck checker's impact force / pending type, the score), so the product's
 *     ride and the original can be read side by side.
 * Nothing is poked except the input (no game logic, no state construction).
 *
 *   REFLEX_HOLD_W=1 REFLEX_PORT="rx_TrickLandPadHook,rx_TrickLandFrame" RX_TRICKLAND=1 RX_TRICKLAND_FRAMES=<n> \
 *     <reflex_boot> <install> <crt> <save> 2400 <log>
 */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
extern "C" {
#include "../game/world_rider.h"
}

namespace {
reflex_vm::Machine* g_m = nullptr;
bool mapped(uint32_t a, uint32_t n = 4) { return g_m && g_m->mem.mapped(a) && g_m->mem.mapped(a + n - 1); }
uint32_t rd(uint32_t a) { return mapped(a) ? rx_rd32(a) : 0; }
float f32at(uint32_t a) { return rx_oracle::f32(rd(a)); }
uint32_t bits(float f) { return rx_oracle::bits(f); }
rx_regs regs0() { rx_regs r; std::memset(&r, 0, sizeof r); return r; }
rx_regs call_orig(uint32_t fn, const rx_regs& in, int n = 0, const uint32_t* args = nullptr) {
    rx_regs out = regs0();
    rx_call_original_ex(fn, &in, &out, n, args);
    return out;
}
/* a string on the game stack below the current SP (the callee's frame goes below it) */
struct StackStr {
    uint32_t sp, at;
    StackStr(const char* s, uint32_t n = 0x40) : sp(rx_sp()), at(sp - n) {
        rx_set_sp(at);
        uint32_t i = 0;
        for (; s[i] && i + 1 < n; i++) rx_wr8(at + i, uint8_t(s[i]));
        rx_wr8(at + i, 0);
    }
    ~StackStr() { rx_set_sp(sp); }
};
/* the aerial 9 O'Clock Nac trick record (006c8570) and the trick start (006c98c0) on the live tracker: the product ride's
 * gesture would reach the same call; this only removes the input timing from the comparison (RX_TRICKLAND_START=1) */
bool start_trick(uint32_t K) {
    static uint32_t trick = 0;
    if (!trick) {
        StackStr nm("9OClockNac_L_Air", 0x40);
        const uint32_t a[2] = {nm.at, 1};
        trick = call_orig(0x006c8570u, regs0(), 2, a).r[0];
        std::fprintf(stderr, "[tl] trick record %08x (aerial 9OClockNac_L_Air)\n", trick);
    }
    if (!trick) return false;
    rx_regs in = regs0(); in.r[6] = K; in.r[7] = trick;
    call_orig(0x006c98c0u, in, 0, nullptr);
    return true;
}

/* the first live local human player (the ride's pick: kind 0, a driver and a vehicle) */
uint32_t pick_player(void) {
    const uint32_t g = rd(0x00a946b8u);
    if (!g) return 0;
    for (uint32_t i = 0; i < 12; i++) {
        const uint32_t p = rx_world_players_find(g + 0x7460u, i, 0);
        if (p && rd(p + 4) && rd(p + 0x1ac) && rd(p + 0x1b4)) return p;
    }
    return 0;
}
/* the first enabled target's value of a listener vector (the ride's read) */
float listener_value(uint32_t vec) {
    if (!vec) return 0.0f;
    const uint32_t e = rd(vec + 0xc);
    if (!e) return 0.0f;
    const uint32_t t = rd(e);
    return t ? f32at(t) : 0.0f;
}
}  // namespace

/* the scripted pad: the reference world passes no pads, so the XInput imports are replaced (the PadSimHook device,
 * loop_input.cpp). "323": left 8 frames, centre 8, down 8, centre 8, left 8, centre 8 -> the shipped mapping
 * 9OClockNac_L_Air. LB (0x0100, the remap's StuntModifier) is held while the rider is airborne and no trick runs yet. */
RX_PORT(0x008ea364, TrickLandPadHook, "thunk", 0) {
    static bool done = false;
    if (!done && std::getenv("RX_TRICKLAND")) {
        done = true;
        m.imports["xinput1_3.dll!#4"] = [](reflex_vm::Machine& mm) {          // XInputGetCapabilities (user, flags, caps)
            const uint32_t i = mm.arg(0), out = mm.arg(2);
            if (i != 0) return mm.ret(12, 1167);
            const uint8_t caps[20] = {1, 1, 0, 0, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xc0, 0xff, 0xc0, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff};
            mm.mem.write_bytes(out, caps, sizeof caps);
            return mm.ret(12, 0);
        };
        m.imports["xinput1_3.dll!#2"] = [](reflex_vm::Machine& mm) {          // XInputGetState (user, state)
            const uint32_t i = mm.arg(0), out = mm.arg(1);
            if (i != 0) return mm.ret(8, 1167);
            static uint32_t clock = 0, air_start = 0;
            static uint32_t prev_air = 0;
            const uint32_t p = uint32_t(mm.mem.read(0x00a946b8u, 4));
            uint32_t K = 0;
            for (uint32_t k = 0; k < 12 && p; k++) {
                const uint32_t pp = p + 0x7460u + 0x12a0u * k;
                if (uint32_t(mm.mem.read(pp + 4, 4)) && uint32_t(mm.mem.read(pp + 0x1ac, 4))) { K = pp + 0xb58u; break; }
            }
            const uint32_t air = K ? uint32_t(mm.mem.read(K + 0x88u, 4)) : 0;
            const uint32_t mode = K ? uint32_t(mm.mem.read(K + 8u, 4)) : 0;
            if (air && !prev_air) air_start = clock;
            prev_air = air;
            uint16_t buttons = 0;
            int16_t rx = 0, ry = 0;
            if (air && mode == 0 && !std::getenv("RX_TRICKLAND_START")) {
                buttons = 0x0100;
                switch (((clock - air_start) / 8u) % 6u) {
                case 0: case 4: rx = -24000; break;   /* '3' (left) */
                case 2: ry = -24000; break;           /* '2' (down) */
                default: break;                       /* centre */
                }
            }
            clock++;
            const uint32_t t = uint32_t(mm.mem.read(0x00a95844u, 4));
            const uint32_t timing = t ? uint32_t(mm.mem.read(t + 0x10, 4)) : 0;
            const uint32_t gclock = timing ? uint32_t(mm.mem.read(timing + 0x3c, 4)) : 0;
            mm.mem.write(out, 4, gclock); mm.mem.write(out + 4, 2, buttons); mm.mem.write(out + 6, 1, 0); mm.mem.write(out + 7, 1, 0);
            mm.mem.write(out + 8, 2, uint16_t(0));       // thumbLX
            mm.mem.write(out + 10, 2, uint16_t(0));      // thumbLY
            mm.mem.write(out + 12, 2, uint16_t(rx));     // thumbRX
            mm.mem.write(out + 14, 2, uint16_t(ry));     // thumbRY
            return mm.ret(8, 0);
        };
        m.imports["xinput1_3.dll!#3"] = [](reflex_vm::Machine& mm) { return mm.ret(8, mm.arg(0) == 0 ? 0 : 1167); };   // XInputSetState
    }
    return reflex_vm::RUN_ORIGINAL;
}

/* the per-frame state dump (Engine::Update 0084ed20, outside every verified call) */
RX_PORT(0x0084ed20, TrickLandFrame, "thunk", 0) {
    if (!std::getenv("RX_TRICKLAND")) return reflex_vm::RUN_ORIGINAL;
    if (!g_m) g_m = &m;
    static uint32_t frame = 0;
    static uint32_t last_mode = ~0u, last_af10 = ~0u, last_wreck = ~0u, last_trick = ~0u, last_state = ~0u;
    ++frame;
    const uint32_t p = pick_player();
    if (!p) return reflex_vm::RUN_ORIGINAL;
    const uint32_t V = rd(p + 0x1ac), R = rd(p + 0x1b4), K = p + 0xb58, S = p + 0xc70;
    const uint32_t mode = rd(K + 8), af10 = rd(R + 0xaf10), wreck = rd(V + 0x280), trick = rd(K + 0x1c);
    const uint32_t ckr = rd(R + 0x190);
    const uint32_t clip = rd(R + 0xaf14);
    const uint32_t clip_end = clip ? rd(rd(clip + 0x70) + 0x28) : 0;
    const uint32_t air = rd(K + 0x88);
    /* the takeoff stimulus: start the trick the moment the rider leaves the ground (no pad / gesture timing) */
    static uint32_t prev_air = 0;
    if (std::getenv("RX_TRICKLAND_START") && air && !prev_air && mode == 0 && wreck == 0) {
        if (start_trick(K)) std::fprintf(stderr, "[tl] f%u START at takeoff (mode %u cur %08x af10 %u)\n", frame,
                                          rd(K + 8), rd(K + 0x1c), rd(R + 0xaf10));
    }
    prev_air = air;
    /* the first line of a trick, of a wreck and the frames around them, plus a one-per-second summary */
    const bool event = mode != last_mode || af10 != last_af10 || wreck != last_wreck || trick != last_trick;
    static uint32_t last_air = ~0u;
    if (air != last_air) {
        std::fprintf(stderr, "[tl] f%u AIR %u mode %u af10 %u wreck %u speed %.2f y %.1f pos %.1f %.1f %.1f\n", frame, air,
                     mode, af10, wreck, f32at(V + 0x150), f32at(V + 0xc4), f32at(V + 0xc0), f32at(V + 0xc4), f32at(V + 0xc8));
        last_air = air;
    }
    if (event || (frame % 60) == 0) {
        const uint32_t sim = rd(V + 0x234);
        std::fprintf(stderr,
                     "[tl] f%u air %u mode %u cur %08x af10 %u clip %08x cend %.3f t %.3f prog %.3f af20 %.3f af30 %u "
                     "wreck %u (type %u stam %.3f) hit %u crash %u force %.3f pend %u speed %.2f y %.1f pose %u/%u\n",
                     frame, air, mode, trick, af10, clip, clip_end ? f32at(clip_end) : 0.0f,
                     listener_value(R + 0x46c), listener_value(R + 0x48c), f32at(R + 0xaf20), rd(R + 0xaf30),
                     wreck, rd(R + 0xb34), f32at(R + 0xb38), rd(rd(V + 0x234) + 0x14), rd(rd(V + 0x234) + 0x18),
                     ckr ? f32at(ckr + 0x24) : 0.0f, ckr ? rd(ckr + 0x1c) : 0, f32at(V + 0x150), f32at(V + 0xc4),
                     rd(R + 0x5b57c), rd(R + 0x5b2d4));
        std::fprintf(stderr, "[tl]   pose q %.4f %.4f %.4f %.4f v %.3f %.3f %.3f up %.3f %.3f %.3f\n",
                     f32at(sim + 0x730), f32at(sim + 0x734), f32at(sim + 0x738), f32at(sim + 0x73c),
                     f32at(sim + 0x100), f32at(sim + 0x104), f32at(sim + 0x108),
                     f32at(V + 0x140), f32at(V + 0x144), f32at(V + 0x148));
        last_mode = mode; last_af10 = af10; last_wreck = wreck; last_trick = trick; (void)last_state;
    }
    if ((frame % 60) == 0) {
        char rn[33];
        int i = 0;
        for (; i < 32; i++) { const uint32_t c = rd(S + 0x364u + (uint32_t)i); if (!c) break; rn[i] = (char)c; }
        rn[i] = 0;
        std::fprintf(stderr, "[tl] f%u score tricks %u score %.4f recent '%s' K10 %.3f K14 %.3f stance %.3f\n", frame,
                     rd(S + 0x418), f32at(S + 0x78), rn, f32at(K + 0x10), f32at(K + 0x14), f32at(V + 0x5bae4));
    }
    return reflex_vm::RUN_ORIGINAL;
}
