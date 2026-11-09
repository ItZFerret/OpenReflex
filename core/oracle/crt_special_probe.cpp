/* crt_special_probe.cpp -- the oracle's probe for the special operands of the libm entry points (w-crtnan2), development only:
 * _CIatan2 7859aa6a, _CIfmod 7859ac8a, _CIpow 7859afc0, __libm_sse2_pow 7859e9d7, __libm_sse2_sin 7859fb0c, _CIexp 7859ac2c,
 * on NaNs (quiet / signalling, both signs), infinities, zeros, subnormals, huge and ordinary values (all pairs for the two-operand
 * functions). For each case the original runs (crt_bypass: an rx_crt adapter lets it run), then the port on the same operands; the
 * x87 / xmm result, errno (the thread's ptd + 8, reset to a marker before each call) and the registers the adapters declare are
 * compared. A case the port still declines (it runs the original: crt_fallbacks) is counted as declined, not compared.
 *   REFLEX_PORT=rx_SpecialProbeHook RX_SPECIAL_PROBE=<fn list: atan2,fmod,pow,spow,sin,exp | all> [RX_SPECIAL_SHOW=<n>] on a reference world
 * Then with REFLEX_PORT_VERIFY=1 REFLEX_PORT=rx_crt,rx_SpecialProbeHook RX_SPECIAL_PROBE=verify: the same operands through the original
 * entry points (no bypass), so the rx_crt adapters check the ports under Verify. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
extern "C" {
#include "../game/crt.h"
#include "../game/crt_util.h"
}
using namespace reflex_port;

namespace {
unsigned g_frame = 0;
uint64_t db(double d) { uint64_t b; std::memcpy(&b, &d, 8); return b; }
double bd(uint64_t b) { double d; std::memcpy(&d, &b, 8); return d; }
const uint64_t D[] = {
    0xfff8000000000000ull, 0x7ff8000000000000ull, 0x7ff4000000000000ull, 0xfff0000000000001ull, 0x7ff8000012345678ull,
    0x7ff0000000000000ull, 0xfff0000000000000ull, 0x0000000000000000ull, 0x8000000000000000ull,
    0x3ff0000000000000ull, 0xbff0000000000000ull, 0x3fe0000000000000ull, 0xc004000000000000ull /* -2.5 */,
    0x4000000000000000ull, 0x4008000000000000ull, 0xc008000000000000ull /* -3 */, 0x7e37e43c8800759cull /* 1e300 */,
    0xfe37e43c8800759cull, 0x01a56e1fc2f8f359ull /* 1e-300 */, 0x0000000000000001ull, 0x800fffffffffffffull,
    0x40f86a0000000000ull /* 1e5 */, 0x4415af1d78b58c40ull /* 1e20 */, 0x4400000000000000ull /* 2^65 */,
    0xc085e00000000000ull /* -700 */, 0x4086300000000000ull /* 710 */, 0xc089000000000000ull /* -800 */,
    0x3fd0000000000000ull /* 0.25 */, 0x3c90000000000000ull /* 2^-54 */, 0x4059000000000000ull /* 100 */,
    0x408ffc0000000000ull /* 1023.5 */, 0xc090c80000000000ull /* -1074 */, 0xc090800000000000ull /* -1056 */, 0x3ff0000000000001ull,
    0xc086232bdd7abcd2ull /* -708.4 */, 0xc0874910d52d3051ull /* -745.1 */, 0x40862e42fefa39efull /* 709.78 */, 0x3fefffffffffffffull,
    0x43e0000000000000ull /* 2^63 */, 0xc3e0000000000001ull, 0x7fefffffffffffffull /* max */, 0x43f0000000000000ull /* 2^64 */, 0x45a0000000000000ull,
};
constexpr int ND = sizeof D / sizeof D[0];
bool same_f80(const rx_f80& a, const rx_f80& b) {
    return a.kind == b.kind && a.sign == b.sign && (a.kind == 1 || (a.mant == b.mant && (a.mant == 0 || a.exp == b.exp)));
}
std::string f80s(const rx_f80& a) {
    char b[64]; std::snprintf(b, sizeof b, "{k%u s%u %016llx e%d}", a.kind, a.sign, (unsigned long long)a.mant, a.exp); return b;
}

struct Ctx { reflex_vm::Machine* m; uint32_t errno_at; int show; bool verify; };
struct Tally { const char* name; int total = 0, declined = 0, bad = 0, regbad = 0; };
void report(const Tally& t) {
    std::fprintf(stderr, "[specprobe] %-6s %4d cases: ported %4d (mismatches %d, register differences %d), declined %d\n", t.name, t.total,
                 t.total - t.declined, t.bad, t.regbad, t.declined);
}
const uint32_t ERRNO_MARK = 0x5au;

/* an x87 two-operand entry: st(1) = a, st(0) = b */
template <class F> void x87_2(Ctx& c, const char* name, uint32_t fn, F port) {
    Tally t; t.name = name;
    for (int i = 0; i < ND; i++) for (int j = 0; j < ND; j++) {
        const rx_f80 a = rx_f80_from_f64(bd(D[i])), b = rx_f80_from_f64(bd(D[j]));
        rx_regs in{}, out{};
        in.fpu_in = 2; in.fpu[0] = a; in.fpu[1] = b;
        rx_wr32(c.errno_at, ERRNO_MARK);
        if (!c.verify) crt_bypass = fn;
        rx_call_original_ex(fn, &in, &out, 0, nullptr);
        crt_bypass = 0;
        if (c.verify) continue;
        t.total++;
        const uint32_t oerr = rx_rd32(c.errno_at);
        rx_wr32(c.errno_at, ERRNO_MARK);
        const unsigned f0 = crt_fallbacks;
        uint32_t regs[3] = {0, 0, 0};
        const rx_f80 r = port(a, b, c.m->fpu_cw, regs);
        const uint32_t perr = rx_rd32(c.errno_at);
        if (crt_fallbacks != f0) {
            t.declined++;
            if (t.declined <= c.show) std::fprintf(stderr, "[specprobe] %s DECLINED %016llx %016llx: orig %s errno %u eax %08x ecx %08x edx %08x\n", name,
                (unsigned long long)D[i], (unsigned long long)D[j], f80s(out.st[0]).c_str(), oerr, out.r[0], out.r[1], out.r[2]);
            continue;
        }
        if (!same_f80(r, out.st[0]) || perr != oerr || out.fpu_depth != 1) {
            t.bad++;
            if (t.bad <= c.show) std::fprintf(stderr, "[specprobe] %s MISMATCH %016llx %016llx: port %s errno %u, orig %s errno %u depth %d\n", name,
                (unsigned long long)D[i], (unsigned long long)D[j], f80s(r).c_str(), perr, f80s(out.st[0]).c_str(), oerr, out.fpu_depth);
        } else if (regs[0] != out.r[0] || regs[1] != out.r[1] || regs[2] != out.r[2]) {
            t.regbad++;
            if (t.regbad <= c.show) std::fprintf(stderr, "[specprobe] %s regs %016llx %016llx: port %08x %08x %08x orig %08x %08x %08x\n", name,
                (unsigned long long)D[i], (unsigned long long)D[j], regs[0], regs[1], regs[2], out.r[0], out.r[1], out.r[2]);
        }
    }
    if (!c.verify) report(t);
}

void spow(Ctx& c) {
    Tally t; t.name = "spow";
    for (int i = 0; i < ND; i++) for (int j = 0; j < ND; j++) {
        rx_regs in{}, out{};
        std::memcpy(in.xmm[0].v, &D[i], 8); std::memcpy(in.xmm[1].v, &D[j], 8);
        rx_wr32(c.errno_at, ERRNO_MARK);
        if (!c.verify) crt_bypass = 0x7859e9d7u;
        rx_call_original_ex(0x7859e9d7u, &in, &out, 0, nullptr);
        crt_bypass = 0;
        if (c.verify) continue;
        t.total++;
        const uint32_t oerr = rx_rd32(c.errno_at);
        uint64_t ol[2]; std::memcpy(ol, out.xmm[0].v, 16);
        rx_wr32(c.errno_at, ERRNO_MARK);
        const unsigned f0 = crt_fallbacks;
        uint64_t pl[2] = {0, 0}; uint32_t regs[3] = {0, 0, 0};
        crt_libm_sse2_pow_lanes(bd(D[i]), bd(D[j]), nullptr, pl, regs);
        const uint32_t perr = rx_rd32(c.errno_at);
        if (crt_fallbacks != f0) {
            t.declined++;
            if (t.declined <= c.show) std::fprintf(stderr, "[specprobe] spow DECLINED %016llx %016llx: orig %016llx %016llx errno %u eax %08x ecx %08x edx %08x\n",
                (unsigned long long)D[i], (unsigned long long)D[j], (unsigned long long)ol[0], (unsigned long long)ol[1], oerr, out.r[0], out.r[1], out.r[2]);
            continue;
        }
        if (pl[0] != ol[0] || pl[1] != ol[1] || perr != oerr) {
            t.bad++;
            if (t.bad <= c.show) std::fprintf(stderr, "[specprobe] spow MISMATCH %016llx %016llx: port %016llx %016llx errno %u, orig %016llx %016llx errno %u\n",
                (unsigned long long)D[i], (unsigned long long)D[j], (unsigned long long)pl[0], (unsigned long long)pl[1], perr,
                (unsigned long long)ol[0], (unsigned long long)ol[1], oerr);
        } else if (regs[0] != out.r[0] || regs[1] != out.r[1] || regs[2] != out.r[2]) {
            t.regbad++;
            if (t.regbad <= c.show) std::fprintf(stderr, "[specprobe] spow regs %016llx %016llx: port %08x %08x %08x orig %08x %08x %08x\n",
                (unsigned long long)D[i], (unsigned long long)D[j], regs[0], regs[1], regs[2], out.r[0], out.r[1], out.r[2]);
        }
    }
    if (!c.verify) report(t);
}

/* one-operand entries: sin (xmm0 -> xmm0, lane 0 compared; EAX / EDX), exp (_CIexp: st(0) -> st(0)) */
void one(Ctx& c, bool is_sin) {
    Tally t; t.name = is_sin ? "sin" : "exp";
    const uint32_t fn = is_sin ? 0x7859fb0cu : 0x7859ac2cu;
    for (int i = 0; i < ND; i++) {
        rx_regs in{}, out{};
        if (is_sin) std::memcpy(in.xmm[0].v, &D[i], 8);
        else { in.fpu_in = 1; in.fpu[0] = rx_f80_from_f64(bd(D[i])); }
        rx_wr32(c.errno_at, ERRNO_MARK);
        if (!c.verify) crt_bypass = fn;
        rx_call_original_ex(fn, &in, &out, 0, nullptr);
        crt_bypass = 0;
        if (c.verify) continue;
        t.total++;
        const uint32_t oerr = rx_rd32(c.errno_at);
        uint64_t ol; std::memcpy(&ol, out.xmm[0].v, 8);
        const uint64_t ores = is_sin ? ol : db(rx_f80_to_f64(out.st[0], 0));
        rx_wr32(c.errno_at, ERRNO_MARK);
        const unsigned f0 = crt_fallbacks;
        uint32_t regs[3] = {out.r[0], out.r[1], out.r[2]};
        uint64_t pres;
        if (is_sin) {
            int ok; double r = crt_libm_sse2_sin_ex(bd(D[i]), regs, &ok);
            if (!ok) { int z, ok2; r = crt_libm_sse2_sin_big(bd(D[i]), 0, c.m->fpu_cw, regs, &z, &ok2); if (!ok2) r = crt_libm_sse2_sin(bd(D[i])); }
            pres = db(r);
        }
        else pres = db(crt_exp_x87(bd(D[i])));
        const uint32_t perr = rx_rd32(c.errno_at);
        if (crt_fallbacks != f0) {
            t.declined++;
            if (t.declined <= c.show) std::fprintf(stderr, "[specprobe] %s DECLINED %016llx: orig %016llx errno %u eax %08x ecx %08x edx %08x\n", t.name,
                (unsigned long long)D[i], (unsigned long long)ores, oerr, out.r[0], out.r[1], out.r[2]);
            continue;
        }
        if (pres != ores || perr != oerr) {
            t.bad++;
            if (t.bad <= c.show) std::fprintf(stderr, "[specprobe] %s MISMATCH %016llx: port %016llx errno %u, orig %016llx errno %u\n", t.name,
                (unsigned long long)D[i], (unsigned long long)pres, perr, (unsigned long long)ores, oerr);
        }
    }
    if (!c.verify) report(t);
}
/* sprintf 78552b7e / _vsnprintf 7855471e with %f on inf / NaN (the strings, the return value, errno) */
uint32_t galloc(uint32_t size) {
    const uint32_t args[] = {size};
    const uint32_t p = rx_call_original(0x0078fbe0, RX_CDECL, 0, 1, args);
    for (uint32_t i = 0; i < size; i += 4) rx_wr32(p + i, 0);
    return p;
}
void printf_cases(Ctx& c) {
    Tally t; t.name = "printf";
    static const char* fmts[] = {"%f", "%.0f", "%.1f", "%.2f", "%.3f", "%.4f", "%.5f", "%.6f", "%.7f", "%.12f", "%10.2f", "%-12f|", "%+f",
                                 "% f", "%012.3f", "%3.2f", "%2.2f", "%.1f %d", "%5.1f/sec", "%30.20f", "%#f", "%#.0f", "%#5.0f|", "%-#8.0f|", "%#+.0f", "%#.2f"};
    static const uint64_t vals[] = {0x7ff0000000000000ull, 0xfff0000000000000ull, 0x7ff8000000000000ull, 0xfff8000000000000ull,
                                    0x7ff4000000000000ull, 0xfff4000000000001ull, 0x7ff8000000000001ull, 0xfff8000000000001ull,
                                    0x3ff8000000000000ull, 0xc059000000000000ull};
    const uint32_t buf = galloc(512), buf2 = galloc(512), fmt = galloc(64), va = galloc(32);
    for (const char* f : fmts) for (uint64_t v : vals) for (int vs = 0; vs < 2; vs++) {
        for (uint32_t i = 0; i < 64; i++) rx_wr8(fmt + i, 0);
        for (uint32_t i = 0; f[i]; i++) rx_wr8(fmt + i, uint8_t(f[i]));
        rx_wr32(va, uint32_t(v)); rx_wr32(va + 4, uint32_t(v >> 32)); rx_wr32(va + 8, 77);
        const uint32_t count = vs ? 12u : 0u;                     /* _vsnprintf with a short buffer: truncation / ERANGE */
        for (uint32_t i = 0; i < 512; i += 4) { rx_wr32(buf + i, 0xa5a5a5a5u); rx_wr32(buf2 + i, 0xa5a5a5a5u); }
        rx_wr32(c.errno_at, ERRNO_MARK);
        uint32_t o;
        if (vs) {
            const uint32_t a[4] = {buf, count, fmt, va};
            if (!c.verify) crt_bypass = 0x7855471eu;
            o = rx_call_original(0x7855471eu, RX_CDECL, 0, 4, a);
        } else {
            const uint32_t a[5] = {buf, fmt, rx_rd32(va), rx_rd32(va + 4), 77};
            if (!c.verify) crt_bypass = 0x78552b7eu;
            o = rx_call_original(0x78552b7eu, RX_CDECL, 0, 5, a);
        }
        crt_bypass = 0;
        if (c.verify) continue;
        t.total++;
        const uint32_t oerr = rx_rd32(c.errno_at);
        rx_wr32(c.errno_at, ERRNO_MARK);
        const unsigned f0 = crt_fallbacks;
        const uint32_t p = vs ? (uint32_t)crt_vsnprintf(buf2, count, fmt, va) : (uint32_t)crt_sprintf(buf2, fmt, va);
        const uint32_t perr = rx_rd32(c.errno_at);
        if (crt_fallbacks != f0) { t.declined++; continue; }
        bool same = o == p && oerr == perr;
        for (uint32_t i = 0; i < 512 && same; i++) same = rx_rd8(buf + i) == rx_rd8(buf2 + i);
        std::string os, ps;
        for (uint32_t i = 0; i < 60 && rx_rd8(buf + i) && rx_rd8(buf + i) != 0xa5; i++) os += char(rx_rd8(buf + i));
        for (uint32_t i = 0; i < 60 && rx_rd8(buf2 + i) && rx_rd8(buf2 + i) != 0xa5; i++) ps += char(rx_rd8(buf2 + i));
        if (!same) {
            t.bad++;
            if (t.bad <= c.show) std::fprintf(stderr, "[specprobe] printf MISMATCH %s %s %016llx: port '%s' %d errno %u, orig '%s' %d errno %u\n", vs ? "vsn" : "spr",
                f, (unsigned long long)v, ps.c_str(), int(p), perr, os.c_str(), int(o), oerr);
        } else if (c.show >= 100) std::fprintf(stderr, "[specprobe] printf %s %016llx -> '%s' (%d)\n", f, (unsigned long long)v, os.c_str(), int(o));
    }
    if (!c.verify) report(t);
}
}  // namespace

RX_PORT(0x0084ed20, SpecialProbeHook, "thunk", 0) {
    const char* which = std::getenv("RX_SPECIAL_PROBE");
    static const unsigned at = std::getenv("RX_SPECIAL_PROBE_FRAME") ? unsigned(std::atoi(std::getenv("RX_SPECIAL_PROBE_FRAME"))) : 5;
    if (!which || ++g_frame != at) return reflex_vm::RUN_ORIGINAL;
    uint32_t regs[8]; reflex_vm::Xmm xmm[8];
    std::memcpy(regs, m.r, sizeof regs); std::memcpy(xmm, m.xmm, sizeof xmm);
    const uint32_t flags = m.flags; auto fpu = m.fpu;
    const rx_addr ptd = crt_ptd();
    Ctx c{&m, ptd ? ptd + 8 : 0x785b7358u, std::getenv("RX_SPECIAL_SHOW") ? std::atoi(std::getenv("RX_SPECIAL_SHOW")) : 8, std::strcmp(which, "verify") == 0};
    const uint32_t errno0 = rx_rd32(c.errno_at);
    const std::string w = c.verify ? "all" : which;
    auto on = [&](const char* n) { return w == "all" || ("," + w + ",").find("," + std::string(n) + ",") != std::string::npos; };
    std::fprintf(stderr, "[specprobe] cw %04x mxcsr %08x [785bc614] %08x [785b7000] %08x\n", m.fpu_cw, m.mxcsr, rx_rd32(0x785bc614u), rx_rd32(0x785b7000u));
    if (on("atan2")) x87_2(c, "atan2", 0x7859aa6au, [](rx_f80 a, rx_f80 b, uint16_t cw, uint32_t* r) { return crt_ci_atan2_ex(a, b, cw, r); });
    if (on("fmod")) x87_2(c, "fmod", 0x7859ac8au, [](rx_f80 a, rx_f80 b, uint16_t cw, uint32_t* r) { return crt_ci_fmod_ex(a, b, cw, r); });
    if (on("pow")) x87_2(c, "pow", 0x7859afc0u, [](rx_f80 a, rx_f80 b, uint16_t cw, uint32_t* r) { return crt_ci_pow_ex(a, b, cw, 0x1f80, r); });
    if (on("spow")) spow(c);
    if (on("sin")) one(c, true);
    if (on("exp")) one(c, false);
    if (on("printf")) printf_cases(c);
    rx_wr32(c.errno_at, errno0);
    std::memcpy(m.r, regs, sizeof regs); std::memcpy(m.xmm, xmm, sizeof xmm); m.flags = flags; m.fpu = fpu;
    return reflex_vm::RUN_ORIGINAL;
}
