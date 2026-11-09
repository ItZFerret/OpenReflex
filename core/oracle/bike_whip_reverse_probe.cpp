/* Development-only: the whip reversal case (field report "how fast the bike turns back from whipping to the other side").
 *
 * A genuine emulated XInput provider (selected as rx_WhipReversePadHook, exclusive with WhipPadHook / PadSimHook): RT 255 from the
 * racing start, sticks neutral until the local MX simulation's first natural take-off (S+8d98 rising after >= 0.5 s grounded), then,
 * on the game clock from that take-off:
 *   [0, T1)        full lateral whip   (RX_WHIP_STICKS: both (default) / right / left; RX_WHIP_SIDE: +1 (default) or -1)
 *   [T1, T2)       the opposite side   (RX_WHIP_RAMP_MS > 0: a linear sweep over that time instead of a snap)
 *   [T2, ...)      neutral             (RX_WHIP_T1 / RX_WHIP_T2 in seconds, defaults 0.25 / 0.55)
 * once, on the RX_WHIP_JUMP-th qualifying take-off (default 1; every qualifying take-off is logged). The game's own pad / keymap / controls processing runs unchanged.
 *
 * Step trace (RX_WHIP_TRACE=1): one line per fixed physics step at the entry of 00431d60 (ESI = S) of the local simulation: the
 * airborne / first-air flags, the four control axes, angular velocity, orientation, the three air lag filters (roll / pitch / yaw,
 * 5 words each), the assist fades, the air timer, the stick history, the take-off orientation, the gesture flags, the latches,
 * as raw hex words. The original run selects the thunk rx_WhipStepTrace (the original step runs after it); the native run selects
 * rx_bike minus the two schedulers, so the core's step is entered through its adapter, which calls whip_trace_step first.
 * Identical trace files = the same state at the entry of every step. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
using namespace reflex_port;
namespace {
uint32_t rd(reflex_vm::Machine& m, uint32_t a) { return uint32_t(m.mem.read(a, 4)); }
float rf(reflex_vm::Machine& m, uint32_t a) { uint32_t b = rd(m, a); float f; std::memcpy(&f, &b, 4); return f; }
double envd(const char* n, double d) { const char* v = std::getenv(n); return v && *v ? std::atof(v) : d; }

/* the local human player's MX simulation (api/rx_session.c local_player), 0 when none */
uint32_t local_sim(reflex_vm::Machine& m) {
    const uint32_t g = rd(m, 0xa946b8);
    if (!g || !m.mem.mapped(g + 0x1541c)) return 0;
    const uint32_t count = uint32_t(m.mem.read(g + 0x1541c, 1));
    for (uint32_t i = 0; i < count && i < 8; i++) {
        const uint32_t slot = rd(m, g + 0x153ec + 4 * i);
        if (slot >= 16) continue;
        const uint32_t p = g + 0x7460 + slot * 0x12a0;
        if (rd(m, p + 0x10) != 0) continue;
        const uint32_t v = rd(m, p + 0x1ac);
        if (!v || !m.mem.mapped(v + 0x234)) return 0;
        const uint32_t s = rd(m, v + 0x234);
        return s && m.mem.mapped(s + 0x8fbc) && rd(m, s) == 0x93c8a4 ? s : 0;
    }
    return 0;
}
unsigned long long steps;
/* w-parity4w: the four-wheel classes of core/tools/gridpair_line.inc's pair_fw (its table, read through this machine) */
struct pair_fw_class { uint32_t vt, size, bodies, wheel[4], spin; };
const pair_fw_class* pair_fw_of(reflex_vm::Machine& m, uint32_t S) {
    static const pair_fw_class k[4] = {{0x0093d054u, 0x9bd0, 6, {0x9140, 0x9290, 0x93e0, 0x9530}, 0x9680},
                                       {0x0093d43cu, 0x8850, 5, {0x8080, 0x81d0, 0x8320, 0x8470}, 0x85c0},
                                       {0x0093d24cu, 0x8840, 5, {0x8070, 0x81c0, 0x8310, 0x8460}, 0x85b0},
                                       {0x0093c93cu, 0x8930, 5, {0x8080, 0x81d0, 0x8320, 0x8470}, 0x85c0}};
    const uint32_t vt = S && m.mem.mapped(S) ? rd(m, S) : 0;
    for (const auto& c : k) if (c.vt == vt) return &c;
    return nullptr;
}
FILE* trace_file() {
    static FILE* f = nullptr;
    if (!f) { const char* p = std::getenv("RX_WHIP_TRACE_FILE"); f = p && *p ? std::fopen(p, "w") : stderr; }
    return f;
}
}  // namespace

/* called by the native step adapter (bike_sim.cpp) and by the thunk below */
void whip_trace_step(reflex_vm::Machine& m, uint32_t S, uint32_t dt_bits) {
    if (!std::getenv("RX_WHIP_TRACE")) return;
    if (const char* ts = std::getenv("RX_WHIP_TRACE_SLOT")) {   /* w-gridpair: the simulation of player slot k instead of the local's */
        const uint32_t g = rd(m, 0xa946b8), v = g ? rd(m, g + 0x7460 + uint32_t(std::atoi(ts)) * 0x12a0 + 0x1ac) : 0;
        if (!v || S != rd(m, v + 0x234)) return;
    } else if (S != local_sim(m)) return;
    FILE* f = trace_file();
    if (const pair_fw_class* c = pair_fw_of(m, S)) {   /* w-parity4w: a four-wheel sim: bike_parity_probe.c's four-wheel step line */
        std::fprintf(f, "%llu dt %08x fw %08x in", steps++, dt_bits, c->vt);
        for (uint32_t o = 0x1c; o < 0x5c; o += 4) std::fprintf(f, " %08x", rd(m, S + o));
        std::fprintf(f, " B"); for (uint32_t o = 0xf0; o < 0xf0 + c->bodies * 0xa0; o += 4) std::fprintf(f, " %08x", rd(m, S + o));
        std::fprintf(f, " Q"); for (uint32_t o = 0x730; o < 0x730 + c->bodies * 0xa0; o += 4) std::fprintf(f, " %08x", rd(m, S + o));
        for (int w = 0; w < 4; w++) { std::fprintf(f, " W%d", w); for (uint32_t o = 0; o < 0x150; o += 4) std::fprintf(f, " %08x", rd(m, S + c->wheel[w] + o)); }
        std::fprintf(f, " spin"); for (uint32_t k = 0; k < 4; k++) std::fprintf(f, " %08x", rd(m, S + c->spin + 4 * k));
        std::fprintf(f, " X"); for (uint32_t o = 0x2ec0; o < 0x2ef0; o += 4) std::fprintf(f, " %08x", rd(m, S + o));
        std::fprintf(f, " off %u", rd(m, S + 0x2374));
        if (c->vt == 0x0093d054u) {   /* the ATVs' wheel-query triangle buffer and its count (bike_parity_probe.c) */
            const uint32_t n = rd(m, S + 0x9130);
            std::fprintf(f, " T %u", n);
            for (uint32_t o = 0; n <= 200 && o < n * 48; o += 4) std::fprintf(f, " %08x", rd(m, 0x00ad6720u + o));
        } else {   /* the family's wheel-query triangle buffer and its count (bike_parity_probe.c) */
            const uint32_t tb = c->vt == 0x0093d43cu ? 0x00ad3fb0u : c->vt == 0x0093d24cu ? 0x00ad7550u : 0x00ad3230u;
            const uint32_t n = rd(m, S + (c->vt == 0x0093d24cu ? 0x8060 : 0x8070));
            std::fprintf(f, " T %u", n);
            for (uint32_t o = 0; n <= 200 && o < n * 48; o += 4) std::fprintf(f, " %08x", rd(m, tb + o));
        }
        std::fprintf(f, "\n");
        return;
    }
    const uint32_t C = rd(m, S + 0x5c30);
    std::fprintf(f, "%llu dt %08x air %u first %u c", steps++, dt_bits, rd(m, S + 0x8d98), rd(m, S + 0x8d9c));
    for (uint32_t o = 0x68; o <= 0x74; o += 4) std::fprintf(f, " %08x", rd(m, C + o));
    std::fprintf(f, " w"); for (uint32_t k = 0; k < 3; k++) std::fprintf(f, " %08x", rd(m, S + 0x740 + 4 * k));
    std::fprintf(f, " q"); for (uint32_t k = 0; k < 4; k++) std::fprintf(f, " %08x", rd(m, S + 0x730 + 4 * k));
    std::fprintf(f, " flt"); for (uint32_t k = 0; k < 15; k++) std::fprintf(f, " %08x", rd(m, S + 0x8b78 + 4 * k));
    std::fprintf(f, " fade %08x %08x t %08x", rd(m, S + 0x8bd4), rd(m, S + 0x8bd8), rd(m, S + 0x8b74));
    std::fprintf(f, " hist"); for (uint32_t k = 0; k < 6; k++) std::fprintf(f, " %08x", rd(m, S + 0x8bbc + 4 * k));
    std::fprintf(f, " to"); for (uint32_t k = 0; k < 4; k++) std::fprintf(f, " %08x", rd(m, S + 0x8c10 + 4 * k));
    std::fprintf(f, " g %u %u %u %u", rd(m, S + 0x8d40), rd(m, S + 0x8d44), rd(m, S + 0x8d58), rd(m, S + 0x8d5c));
    std::fprintf(f, " l"); for (uint32_t o : {0x8c60u, 0x8c64u, 0x8c68u, 0x8cd0u, 0x8cd4u, 0x8cd8u}) std::fprintf(f, " %u", rd(m, S + o));
    std::fprintf(f, " pos"); for (uint32_t k = 0; k < 3; k++) std::fprintf(f, " %08x", rd(m, S + 0xf0 + 4 * k));
    std::fprintf(f, " v"); for (uint32_t k = 0; k < 3; k++) std::fprintf(f, " %08x", rd(m, S + 0x100 + 4 * k));
    for (uint32_t W : {0x8790u, 0x88e0u}) {   /* rear, front wheel contact records: active, lateral / rolling grip flags, contact velocity */
        std::fprintf(f, " %s %u %u %u", W == 0x8790u ? "rear" : "front", rd(m, S + W + 0x100), rd(m, S + W + 0xc0), rd(m, S + W + 0xc4));
        for (uint32_t k = 0; k < 3; k++) std::fprintf(f, " %08x", rd(m, S + W + 0x80 + 4 * k));
    }
    std::fprintf(f, " grip %08x %08x", rd(m, S + 0x88a0), rd(m, S + 0x88a4));
    if (std::getenv("RX_WHIP_TRACE_WIDE")) {   /* w-gridpair: the input block S+1c..5c, both wheel records (0x150 bytes) and the bodies' */
        std::fprintf(f, " in"); for (uint32_t o = 0x1c; o < 0x5c; o += 4) std::fprintf(f, " %08x", rd(m, S + o));
        std::fprintf(f, " Wr"); for (uint32_t o = 0; o < 0x150; o += 4) std::fprintf(f, " %08x", rd(m, S + 0x8790 + o));
        std::fprintf(f, " Wf"); for (uint32_t o = 0; o < 0x150; o += 4) std::fprintf(f, " %08x", rd(m, S + 0x88e0 + o));
        std::fprintf(f, " B"); for (uint32_t o = 0xf0; o < 0xf0 + 4 * 0xa0; o += 4) std::fprintf(f, " %08x", rd(m, S + o));
        /* w-parity: the external object and the contact results the previous step took (S+2ebc .. S+2eec) */
        std::fprintf(f, " X"); for (uint32_t o = 0x2ec0; o < 0x2ef0; o += 4) std::fprintf(f, " %08x", rd(m, S + o));
    }
    std::fprintf(f, "\n");
}

RX_PORT(0x00431d60, WhipStepTrace, "thunk", 0) {
    whip_trace_step(m, m.r[6], m.arg(0));
    return reflex_vm::RUN_ORIGINAL;
}

RX_PORT(0x008ea364, WhipReversePadHook, "thunk", 0) {
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
        /* RX_WHIP_QUANT=<fps> (w-gridpair): the script's clock rounded to whole frames of 1/fps s, so the product driver
         * (bike_parity_probe.c, which reads the clock before the frame, the poll here after UpdateTiming) switches phases in the same
         * frame whatever the last bits of the two clocks */
        if (const char* q = std::getenv("RX_WHIP_QUANT")) { const double fps = std::atof(q); if (fps > 0) seconds = float(std::floor(double(seconds) * fps + 0.5) / fps); }
        const uint32_t game = uint32_t(mm.mem.read(0xa946b8, 4));
        bool racing = false;
        if (game && mm.mem.mapped(game + 0x74f4)) {
            const uint32_t activity = uint32_t(mm.mem.read(game + 0x74f4, 4));
            racing = activity && mm.mem.mapped(activity + 0x21c) && int32_t(mm.mem.read(activity + 0x21c, 4)) >= 2;
            /* RX_WHIP_AFTER_COUNTDOWN=1 (w-gridpair: a baton boot from process start, where the front end's UI world activity is
             * InProgress too): racing only once the activity was seen in Pre (0) and then the countdown (1) */
            if (std::getenv("RX_WHIP_AFTER_COUNTDOWN")) {
                static int seen = 0;   /* 1 = Pre seen, 2 = countdown after it */
                const int st = activity && mm.mem.mapped(activity + 0x21c) ? int32_t(mm.mem.read(activity + 0x21c, 4)) : -1;
                if (st == 0) seen = seen < 2 ? 1 : seen; else if (st == 1 && seen == 1) seen = 2;
                racing = racing && seen == 2;
            }
        }
        static int state = 0;              /* 0 waiting for ground, 1 armed (grounded long enough), 2 whip running, 3 done */
        static float ground_since = -1, t0 = 0;
        const uint32_t S = racing ? local_sim(mm) : 0;
        const bool air = S && rd(mm, S + 0x8d98) != 0;
        if (S && state < 2) {
            static int jumps = 0;
            if (air) {
                if (state == 1) {
                    const int want = int(envd("RX_WHIP_JUMP", 1));
                    std::fprintf(stderr, "[whip-rev] take-off %d at clock %.6f%s\n", ++jumps, seconds, jumps == want ? " (whip)" : "");
                    if (jumps == want) { state = 2; t0 = seconds; } else state = 0;
                }
                ground_since = -1;
            }
            else { if (ground_since < 0) ground_since = seconds; if (seconds - ground_since >= 0.5f) state = 1; }
        }
        double lat = 0;   /* -1 .. 1 */
        if (state == 2) {
            double t = seconds - t0; const double T1 = envd("RX_WHIP_T1", 0.25), T2 = envd("RX_WHIP_T2", 0.55), ramp = envd("RX_WHIP_RAMP_MS", 0) / 1000.0;
            const double side = envd("RX_WHIP_SIDE", 1);
            if (const char* q = std::getenv("RX_WHIP_QUANT")) { const double fps = std::atof(q); if (fps > 0) t = std::floor((double(seconds) - double(t0)) * fps + 0.5) / fps; }
            if (t < T1) lat = side;
            else if (t < T2) lat = ramp > 0 && t < T1 + ramp ? side * (1 - 2 * (t - T1) / ramp) : -side;
            else { state = 3; std::fprintf(stderr, "[whip-rev] neutral at clock %.6f\n", seconds); }
        }
        /* RX_WHIP_MODE=turn (the rear-grip case): no whip; from the racing start the left stick steers in a 7 s cycle of
         * 2 s at +RX_TURN_STEER (default 0.5), 1.5 s neutral, 2 s at minus, 1.5 s neutral; RT = RX_TURN_RT (default 255); with
         * RX_TURN_BRAKE=1 the last 0.5 s of each neutral phase brakes (LT 255, RT 0) */
        int rt = racing ? 255 : 0, lt = 0;
        const char* mode = std::getenv("RX_WHIP_MODE");
        if (mode && !std::strcmp(mode, "turn")) {
            static bool anchored = false; static float start = 0;
            if (racing && !anchored) { anchored = true; start = seconds; }
            lat = 0;
            if (racing) {
                double el = double(seconds - start);
                if (const char* q = std::getenv("RX_WHIP_QUANT")) { const double fps = std::atof(q); if (fps > 0) el = std::floor((double(seconds) - double(start)) * fps + 0.5) / fps; }
                const double t = std::fmod(el, 7.0), k = envd("RX_TURN_STEER", 0.5);
                lat = t < 2 ? k : t < 3.5 ? 0 : t < 5.5 ? -k : 0;
                rt = int(envd("RX_TURN_RT", 255));
                if (envd("RX_TURN_BRAKE", 0) != 0 && ((t >= 3.0 && t < 3.5) || t >= 6.5)) { rt = 0; lt = 255; }
            }
        }
        const char* sticks = std::getenv("RX_WHIP_STICKS");
        const bool left = (mode && !std::strcmp(mode, "turn")) || !sticks || !std::strcmp(sticks, "both") || !std::strcmp(sticks, "left");
        const bool right = !(mode && !std::strcmp(mode, "turn")) && (!sticks || !std::strcmp(sticks, "both") || !std::strcmp(sticks, "right"));
        const int16_t v = int16_t(std::lround(lat * 32767));
        mm.mem.write(out, 4, clock);
        mm.mem.write(out + 4, 2, 0); mm.mem.write(out + 6, 1, uint32_t(lt)); mm.mem.write(out + 7, 1, uint32_t(rt));
        mm.mem.write(out + 8, 2, uint16_t(left ? v : 0)); mm.mem.write(out + 10, 2, 0);
        mm.mem.write(out + 12, 2, uint16_t(right ? v : 0)); mm.mem.write(out + 14, 2, 0);
        return mm.ret(8, 0);
    };
    m.imports["xinput1_3.dll!#3"] = [](reflex_vm::Machine& mm) { return mm.ret(8, mm.arg(0) == 0 ? 0 : 1167); };
    std::fprintf(stderr, "[whip-rev] installed the whip reversal XInput provider\n");
    return reflex_vm::RUN_ORIGINAL;
}
