/* crt_pow_probe.cpp -- the oracle's probe for the SSE2 pow kernel's special path 785a5ae7 (785a5799), development only.
 * The five reference worlds never reach the special path: the race frames' only _CIpow call is the engine's quarter power,
 * and their clutch / speed ratios stay positive normal. The first ride's steering pass drives the ratio to exactly +0 at race
 * frame 1010 instead (bike_force.c force_pow_quarter), which is the stop this probe covers.
 *
 * At a chosen frame of an On-mode run, the probe calls the original _CIpow (7859afc0) on a table of crafted x87 stacks and the
 * core kernel directly (crt_pow_kernel_raw: no original fallback, so cases the port declines are visible), and compares the
 * result doubles bit for bit. It also prints the kernel's EAX / ECX / EDX and, for the game's own input (+0, 0.25), the exact
 * result.
 *   REFLEX_PORT=PowProbeHook RX_POW_PROBE_FRAME=<n>  (default 200)  on a reference world (verify.sh ... or the CLI directly)
 * The hook is the usual Engine::Update device; it saves and restores the machine's registers / x87 stack around the calls. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
extern "C" {
#include "../game/crt.h"
}
using namespace reflex_port;

namespace {
reflex_vm::Machine* g_m = nullptr;
uint32_t g_frame = 0;

struct Saved {
    uint32_t r[8]; reflex_vm::Xmm xmm[8]; uint32_t flags; size_t fpu;
    explicit Saved(reflex_vm::Machine& m) : flags(m.flags), fpu(m.fpu.size()) {
        std::memcpy(r, m.r, sizeof r); std::memcpy(xmm, m.xmm, sizeof xmm);
    }
    void restore(reflex_vm::Machine& m) const {
        std::memcpy(m.r, r, sizeof r); std::memcpy(m.xmm, xmm, sizeof xmm); m.flags = flags; m.fpu.resize(fpu);
    }
};
rx_regs regs() { rx_regs r; std::memset(&r, 0, sizeof r); return r; }
uint64_t db(double d) { uint64_t b; std::memcpy(&b, &d, 8); return b; }
double bd(uint64_t b) { double d; std::memcpy(&d, &b, 8); return d; }

struct Case { uint64_t x, y; const char* name; };
const uint64_t XS[] = {
    0x0000000000000000ull,   /* +0: the ride's value */
    0x8000000000000000ull,   /* -0 */
    0x3fd0000000000000ull,   /* 0.25 */
    0x3fe0000000000000ull,   /* 0.5 */
    0x3fefffffffffffffull,   /* just below 1 */
    0x3ff0000000000000ull,   /* 1 */
    0x4000000000000000ull,   /* 2 */
    0x4008000000000000ull,   /* 3 */
    0xc000000000000000ull,   /* -2 (integer y only) */
    0xbff0000000000000ull,   /* -1 */
    0x0010000000000000ull,   /* smallest normal */
    0x000fffffffffffffull,   /* largest subnormal */
    0x0000000000000001ull,   /* smallest subnormal */
    0x7ff0000000000000ull,   /* +inf */
    0xfff0000000000000ull,   /* -inf */
    0x7ff8000000000000ull,   /* qNaN */
};
const uint64_t YS[] = {
    0x3fd0000000000000ull,   /* 0.25: the engine's exponent */
    0x3fe0000000000000ull,   /* 0.5 */
    0x3ff0000000000000ull,   /* 1 */
    0x4000000000000000ull,   /* 2 */
    0x4008000000000000ull,   /* 3 */
    0xbff0000000000000ull,   /* -1 */
    0xc000000000000000ull,   /* -2 */
    0xbfd0000000000000ull,   /* -0.25 */
    0x0000000000000000ull,   /* +0 */
    0x8000000000000000ull,   /* -0 */
    0x7ff0000000000000ull,   /* +inf */
    0x7ff8000000000000ull,   /* qNaN */
};

void probe_pow() {
    const uint16_t cw = g_m->fpu_cw, rc = uint16_t((cw >> 10) & 3);
    int ported = 0, skipped = 0, bad = 0, regbad = 0, total = 0;
    for (uint64_t xb : XS)
        for (uint64_t yb : YS) {
            total++;
            rx_regs in = regs(), out = regs();
            in.fpu_in = 2;                                   /* pushed in order: fpu[0] = x (st(1)), fpu[1] = y (st(0)) */
            in.fpu[0] = rx_f80_from_f64(bd(xb));
            in.fpu[1] = rx_f80_from_f64(bd(yb));
            rx_call_original_ex(0x7859afc0u, &in, &out, 0, nullptr);
            const uint64_t orig = db(rx_f80_to_f64(out.st[0], rc));
            uint32_t regs[3] = {0, 0, 0}; int ok = 0;
            const double pr = crt_pow_kernel_raw(xb, yb, regs, &ok);
            const uint64_t port = db(pr);
            if (!ok) { skipped++; continue; }
            ported++;
            if (port != orig) {
                bad++;
                if (bad <= 40)
                    std::fprintf(stderr, "[powprobe] MISMATCH x=%016llx y=%016llx port %016llx orig %016llx regs %08x %08x %08x\n",
                                 (unsigned long long)xb, (unsigned long long)yb, (unsigned long long)port,
                                 (unsigned long long)orig, regs[0], regs[1], regs[2]);
            } else if (regs[0] != out.r[0] || regs[1] != out.r[1] || regs[2] != out.r[2]) {
                /* not declared live by the game's callers; reported for the note */
                regbad++;
                if (regbad <= 10)
                    std::fprintf(stderr, "[powprobe] regs x=%016llx y=%016llx port %08x %08x %08x orig %08x %08x %08x\n",
                                 (unsigned long long)xb, (unsigned long long)yb, regs[0], regs[1], regs[2],
                                 out.r[0], out.r[1], out.r[2]);
            }
        }
    std::fprintf(stderr, "[powprobe] %d cases: ported %d, declined (original) %d, mismatches %d, register differences %d\n",
                 total, ported, skipped, bad, regbad);
    /* the game's own call: x = +0 (the clutch / speed ratio at race frame 1010), y = 0.25 (00977868) */
    rx_regs in = regs(), out = regs();
    in.fpu_in = 2; in.fpu[0] = rx_f80_from_f32(0.0f); in.fpu[1] = rx_f80_from_f64(0.25);   /* st(1) = x = +0, st(0) = y */
    rx_call_original_ex(0x7859afc0u, &in, &out, 0, nullptr);
    uint32_t regs[3] = {0, 0, 0}; int ok = 0;
    const uint64_t port = db(crt_pow_kernel_raw(0, db(0.25), regs, &ok));
    std::fprintf(stderr, "[powprobe] game case pow(+0, 0.25): port %s %016llx, original %016llx\n",
                 ok ? "handles" : "DECLINES", (unsigned long long)port,
                 (unsigned long long)db(rx_f80_to_f64(out.st[0], rc)));
}
}  // namespace

RX_PORT(0x0084ed20, PowProbeHook, "thunk", 0) {
    static const uint32_t at = std::getenv("RX_POW_PROBE_FRAME") ? uint32_t(std::atoi(std::getenv("RX_POW_PROBE_FRAME"))) : 200;
    ++g_frame;
    if (g_frame != at) return reflex_vm::RUN_ORIGINAL;
    const Saved saved(m);
    g_m = &m;
    probe_pow();
    saved.restore(m);
    return reflex_vm::RUN_ORIGINAL;
}
