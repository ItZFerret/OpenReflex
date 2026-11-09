/* Development-only (w-fsfix): the trick hold / release case on the original, for the field report "tricks sit on a dead timer and don't
 * stop when you let go of the buttons".
 *
 * A genuine emulated XInput provider (selected as rx_TrickPadHook; exclusive with the other pad hooks): RT 255 once the local player's
 * activity is racing (+0x21c >= 2); on each of the local rider's first RX_TRICK_COUNT (default 3) take-offs that last more than 3 frames
 * (the tracker K = player + 0xb58, K + 0x88 airborne, as the frame before saw it) the trick modifier LB is held and the right stick draws
 * the shipped gesture "323" (9OClockNac_L_Air): left 5 frames, centre 2, down 5, centre 2, then left held. RX_TRICK_RELEASE=<n> lets go
 * n frames after that last direction began (RX_TRICK_RELMODE 0 both, 1 the stick only, 2 LB only); -1 (default) holds until landing.
 * Frames are the game clock in 1/60 s (the boot runs 60 Hz frames). The game's own pad / keymap / gesture / trick / animation code runs
 * unchanged. One trace line per frame (RX_TRICK_TRACE=<file>, default stderr) while a trick or its input is live: the same fields as the
 * product driver (w-fsfix tools/trick_hold_ride.c): the gesture state K + 0x5c, the hold flag K + 0x18, the mode K + 8, the rider's trick
 * state +0xaf10, its hold flags +0xaf30 / +0xaf34, the performance animator's time (+0xaf14 -> +0x98) and the clip end +0xaf20. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
using namespace reflex_port;
namespace {
uint32_t rd(reflex_vm::Machine& m, uint32_t a) { return m.mem.mapped(a) ? uint32_t(m.mem.read(a, 4)) : 0; }
float rf(reflex_vm::Machine& m, uint32_t a) { uint32_t b = rd(m, a); float f; std::memcpy(&f, &b, 4); return f; }
int envi(const char* n, int d) { const char* v = std::getenv(n); return v && *v ? std::atoi(v) : d; }
FILE* trace() {
    static FILE* f = nullptr;
    if (!f) { const char* p = std::getenv("RX_TRICK_TRACE"); f = p && *p ? std::fopen(p, "w") : stderr; }
    return f;
}
/* the local human player (api/rx_session.c local_player) or 0 */
uint32_t local_player(reflex_vm::Machine& m) {
    const uint32_t g = rd(m, 0xa946b8);
    if (!g || !m.mem.mapped(g + 0x1541c)) return 0;
    const uint32_t count = uint32_t(m.mem.read(g + 0x1541c, 1));
    for (uint32_t i = 0; i < count && i < 8; i++) {
        const uint32_t slot = rd(m, g + 0x153ec + 4 * i);
        if (slot >= 16) continue;
        const uint32_t p = g + 0x7460 + slot * 0x12a0;
        if (rd(m, p + 0x10) != 0) continue;
        return rd(m, p + 0x1ac) && rd(m, p + 0x1b4) ? p : 0;
    }
    return 0;
}
struct Pad { uint16_t buttons = 0; uint8_t lt = 0, rt = 0; int16_t rx = 0, ry = 0; };
Pad g_pad;
long g_frame = -1;
int g_phase = 0, g_pf = 0, g_air_frames = 0, g_tricks = 0;

/* RX_TRICK_RIDERS=<n>: every n frames, each player slot with a vehicle: its type (+0x10: 0 human, 1 computer), the simulation's
 * position (S + 0xf0) and speed (+0x150 of the vehicle): whether the original's Freestyle moves its computer riders */
void riders_trace(reflex_vm::Machine& m, long frame) {
    const int every = envi("RX_TRICK_RIDERS", 0);
    if (every <= 0 || frame % every != 0) return;
    const uint32_t g = rd(m, 0xa946b8);
    if (!g) return;
    std::fprintf(trace(), "riders f%ld:", frame);
    for (uint32_t k = 0; k < 16; k++) {
        const uint32_t p = g + 0x7460 + k * 0x12a0, v = rd(m, p + 0x1ac);
        if (!v) continue;
        const uint32_t S = rd(m, v + 0x234);
        std::fprintf(trace(), " [%u t%u pos %.1f %.1f %.1f v %.1f]", k, rd(m, p + 0x10), S ? double(rf(m, S + 0xf0)) : 0.0, S ? double(rf(m, S + 0xf4)) : 0.0,
                     S ? double(rf(m, S + 0xf8)) : 0.0, double(rf(m, v + 0x150)));
    }
    std::fprintf(trace(), "\n");
}

void frame_step(reflex_vm::Machine& m, long frame) {
    riders_trace(m, frame);
    const uint32_t g = rd(m, 0xa946b8);
    const uint32_t A = g ? rd(m, g + 0x7460 + 0x94) : 0;
    const bool racing = A && int32_t(rd(m, A + 0x21c)) >= 2;
    const uint32_t p = racing ? local_player(m) : 0;
    g_pad = Pad();
    if (!p) return;
    const uint32_t K = p + 0xb58, ra = rd(m, p + 0x1b4);
    const bool air = rd(m, K + 0x88) != 0;
    g_pad.rt = 255;
    g_air_frames = air ? g_air_frames + 1 : 0;
    if (g_phase == 0 && air && g_air_frames > 3 && g_tricks < envi("RX_TRICK_COUNT", 3)) { g_phase = 1; g_pf = 0; g_tricks++; }
    if (g_phase >= 1) {
        int lb = 1, rx = 0, ry = 0;
        if (g_pf < 5) rx = -30000; else if (g_pf < 7) {} else if (g_pf < 12) ry = -30000; else if (g_pf < 14) {} else rx = -30000;
        const int rel = envi("RX_TRICK_RELEASE", -1), relmode = envi("RX_TRICK_RELMODE", 0);
        if (rel >= 0 && g_pf >= 14 + rel) { if (relmode == 0 || relmode == 1) { rx = 0; ry = 0; } if (relmode == 0 || relmode == 2) lb = 0; }
        if (lb) g_pad.buttons |= 0x0100;
        g_pad.rx = int16_t(rx); g_pad.ry = int16_t(ry);
        g_pf++;
        if (!air && g_pf > 20) g_phase = 0;
    }
    if (g_phase >= 1 || rd(m, K + 8) != 0 || rd(m, ra + 0xaf10) != 0) {
        const uint32_t anim = rd(m, ra + 0xaf14);
        std::fprintf(trace(), "f%ld pf%d air%d lb%d rs(%d,%d) gest%u flag%u mode%u trick=%s ra.st%u hold%u w%u anim98=%.3f end=%.3f\n", frame, g_pf,
                     int(air), (g_pad.buttons & 0x100) != 0, g_pad.rx, g_pad.ry, rd(m, K + 0x5c), rd(m, K + 0x18), rd(m, K + 8),
                     rd(m, K + 0x1c) ? "(t)" : "-", rd(m, ra + 0xaf10), rd(m, ra + 0xaf30), rd(m, ra + 0xaf34),
                     anim ? double(rf(m, anim + 0x98)) : -1.0, double(rf(m, ra + 0xaf20)));
        std::fflush(trace());
    }
}
}  // namespace

RX_PORT(0x008ea364, TrickPadHook, "thunk", 0) {
    static bool installed = false;
    if (installed) return reflex_vm::RUN_ORIGINAL;
    installed = true;
    m.imports["xinput1_3.dll!#4"] = [](reflex_vm::Machine& mm) {
        if (mm.arg(0) != 0) return mm.ret(12, 1167);
        const uint8_t caps[20] = {1, 1, 0, 0, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xc0, 0xff, 0xc0, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff};
        mm.mem.write_bytes(mm.arg(2), caps, sizeof caps);
        return mm.ret(12, 0);
    };
    m.imports["xinput1_3.dll!#2"] = [](reflex_vm::Machine& mm) {
        if (mm.arg(0) != 0) return mm.ret(8, 1167);
        const uint32_t out = mm.arg(1), ctx = uint32_t(mm.mem.read(0xa95844, 4));
        const uint32_t timing = uint32_t(mm.mem.read(ctx + 0x10, 4)), clock = uint32_t(mm.mem.read(timing + 0x3c, 4));
        float seconds; std::memcpy(&seconds, &clock, 4);
        const long frame = long(std::floor(double(seconds) * 60.0 + 0.5));
        if (frame != g_frame) { g_frame = frame; frame_step(mm, frame); }   /* one script step per game frame */
        mm.mem.write(out, 4, clock);
        mm.mem.write(out + 4, 2, g_pad.buttons); mm.mem.write(out + 6, 1, g_pad.lt); mm.mem.write(out + 7, 1, g_pad.rt);
        mm.mem.write(out + 8, 2, 0); mm.mem.write(out + 10, 2, 0);
        mm.mem.write(out + 12, 2, uint16_t(g_pad.rx)); mm.mem.write(out + 14, 2, uint16_t(g_pad.ry));
        return mm.ret(8, 0);
    };
    m.imports["xinput1_3.dll!#3"] = [](reflex_vm::Machine& mm) { return mm.ret(8, mm.arg(0) == 0 ? 0 : 1167); };
    std::fprintf(stderr, "[trick] installed the trick hold XInput provider\n");
    return reflex_vm::RUN_ORIGINAL;
}
