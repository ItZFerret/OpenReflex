/* crt_nan_probe.cpp -- the oracle's probe for the NaN paths of floor 785a2f90 / ceil 785a2da0 (785a3023 / 785a2e33 ->
 * ___libm_error_support 785a8fa1) and their game callers, development only (w-crtnan). Natural runs do not reach these paths:
 * the product reached floor's NaN path once, on a long headless Waypoint ride (FR_Gibson, frame 52,527).
 *   REFLEX_PORT=rx_NanProbeHook RX_NAN_PROBE=1 [RX_NAN_PROBE_FRAME=<n> (5)]            parts A and B (B unchecked without the ports)
 *   REFLEX_PORT_VERIFY=1 REFLEX_PORT=rx_crt,rx_world,rx_phys,rx_NanProbeHook RX_NAN_PROBE=b   part B only, under Verify
 * Part A: for a table of doubles (quiet / signalling NaNs of both signs with payloads, infinities, zeros, normal values) the original
 * floor / ceil run (crt_bypass: the adapter lets the original run) on an argument slot at the probe's stack, with marker values in
 * EBX / EBP / ESI / EDI; then the port (crt_floor_args_ex / crt_ceil_args_ex) on the same slot with the same caller registers.
 * Compared bit for bit: st(0), EAX, the argument slot, errno (the thread's ptd + 8; restored after each call) and the dead stack
 * the call leaves from A-0x60 to A-4 (pre-filled with 0xcccccccc); once with msvcr90's default _matherr, once with the EXE's installed. Part B: calls of the original entry points with the same values
 * (floor / ceil) and of PackedGrid_SampleUV 007f8e00 and TerrainMaterialAt 007c5120 (its 007f8e00 branch) on a private grid with NaN
 * coordinates: with REFLEX_PORT_VERIFY the adapters check the ports on them (the verify summary's counts). */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
extern "C" {
#include "../game/crt.h"
#include "../game/crt_util.h"
}
using namespace reflex_port;

namespace {
unsigned g_frame = 0;
uint64_t db(double d) { uint64_t b; std::memcpy(&b, &d, 8); return b; }
double bd(uint64_t b) { double d; std::memcpy(&d, &b, 8); return d; }
uint32_t alloc(uint32_t size) {
    const uint32_t args[] = {size};
    const uint32_t p = rx_call_original(0x0078fbe0, RX_CDECL, 0, 1, args);
    for (uint32_t i = 0; i < size; i += 4) rx_wr32(p + i, 0);
    return p;
}
const uint64_t XS[] = {
    0xfff8000000000000ull,   /* the x86 default NaN (negative): what an SSE / x87 invalid operation gives */
    0x7ff8000000000000ull,   /* +qNaN */
    0xfff8000012345678ull,   /* -qNaN with a payload */
    0x7ffc000000000001ull,   /* +qNaN with a payload */
    0x7ff0000000000001ull,   /* +sNaN (quieted by the x87 loads) */
    0xfff4000000000000ull,   /* -sNaN */
    0x7ff0000000000000ull,   /* +inf */
    0xfff0000000000000ull,   /* -inf */
    0x0000000000000000ull, 0x8000000000000000ull,
    0x3fe0000000000000ull,   /* 0.5 */
    0xbfe0000000000000ull,   /* -0.5 */
    0x4059000000000000ull,   /* 100 */
    0x40590ccccccccccdull,   /* 100.2 */
    0xc0590ccccccccccdull,   /* -100.2 */
    0x4340000000000001ull,   /* 2^53 + 2 */
};
constexpr uint32_t MARK_EBX = 0xb0b0b0b0u, MARK_EBP = 0x0ebb0ebbu, MARK_ESI = 0x51515151u, MARK_EDI = 0xd1d1d1d1u;
constexpr uint32_t LO = 0x60, HI = 0x04;   /* the compared dead stack: [A-LO, A-HI) */

struct Res { uint64_t st0, slot; uint32_t eax, err, nomem; uint32_t frame[(LO - HI) / 4]; };

void fill(uint32_t A) { for (uint32_t o = HI; o <= LO + 0x20; o += 4) rx_wr32(A - o, 0xccccccccu); }
void grab(uint32_t A, Res& r, uint32_t errno_at) {
    r.slot = rx_rd32(A) | (uint64_t(rx_rd32(A + 4)) << 32);
    r.err = rx_rd32(errno_at); r.nomem = rx_rd32(0x785b7358u);
    for (uint32_t i = 0; i < (LO - HI) / 4; i++) r.frame[i] = rx_rd32(A - LO + 4 * i);
}

void part_a(reflex_vm::Machine& m) {
    const rx_addr ptd = crt_ptd();
    const uint32_t errno_at = ptd ? ptd + 8 : 0x785b7358u;
    std::fprintf(stderr, "[nanprobe] matherr flag [785bc644]=%08x handler [785bd7ac]=%08x (decoded %08x), [785b7000]=%08x, ptd %08x errno %u\n",
                 rx_rd32(0x785bc644u), rx_rd32(0x785bd7acu), rx_rd32(0x785bc644u) ? crt_decode_ptr(rx_rd32(0x785bd7acu)) : 0u,
                 rx_rd32(0x785b7000u), ptd, rx_rd32(errno_at));
    int total = 0, bad = 0;
    for (int f = 0; f < 2; f++) {
        const uint32_t fn = f ? 0x785a2da0u : 0x785a2f90u;
        for (uint64_t xb : XS) {
            total++;
            const uint32_t e0 = rx_rd32(errno_at), n0 = rx_rd32(0x785b7358u);
            const uint32_t sp0 = m.r[4], A = sp0 - 8;            /* call_function: args at sp0 - 8, return slot sp0 - 12 */
            const uint32_t args[2] = {uint32_t(xb), uint32_t(xb >> 32)};
            Res o{}, p{};
            /* the original */
            fill(A);
            rx_regs in{}, out{};
            in.r[3] = MARK_EBX; in.r[5] = MARK_EBP; in.r[6] = MARK_ESI; in.r[7] = MARK_EDI;
            crt_bypass = fn;                                      /* the rx_crt adapter (if on) lets the original run */
            rx_call_original_ex(fn, &in, &out, 2, args);
            crt_bypass = 0;
            o.st0 = out.fpu_depth ? db(rx_f80_to_f64(out.st[0], 0)) : 0xdeadull;
            o.eax = out.r[0];
            grab(A, o, errno_at);
            rx_wr32(errno_at, e0); rx_wr32(0x785b7358u, n0);
            /* the port, on the same slot with the same caller registers */
            fill(A);
            rx_wr32(A, args[0]); rx_wr32(A + 4, args[1]); rx_wr32(A - 4, 0);
            uint32_t keep[8]; std::memcpy(keep, crt_caller_regs, sizeof keep);
            crt_caller_regs[3] = MARK_EBX; crt_caller_regs[5] = MARK_EBP; crt_caller_regs[6] = MARK_ESI; crt_caller_regs[7] = MARK_EDI;
            rx_set_sp(A - 4);                                     /* the adapter's ESP: floor's entry */
            const double r = f ? crt_ceil_args_ex(A, m.fpu_cw, m.mxcsr, &p.eax) : crt_floor_args_ex(A, m.fpu_cw, m.mxcsr, &p.eax);
            rx_set_sp(sp0);
            std::memcpy(crt_caller_regs, keep, sizeof keep);
            p.st0 = db(r);
            grab(A, p, errno_at);
            rx_wr32(errno_at, e0); rx_wr32(0x785b7358u, n0);
            bool same = o.st0 == p.st0 && o.slot == p.slot && o.eax == p.eax && o.err == p.err && o.nomem == p.nomem;
            int fdiff = -1;
            for (uint32_t i = 0; i < (LO - HI) / 4; i++) if (o.frame[i] != p.frame[i]) { same = false; if (fdiff < 0) fdiff = int(i); }
            if (!same) {
                bad++;
                std::fprintf(stderr, "[nanprobe] MISMATCH %s(%016llx): st0 %016llx/%016llx slot %016llx/%016llx eax %08x/%08x errno %u/%u",
                             f ? "ceil" : "floor", (unsigned long long)xb, (unsigned long long)p.st0, (unsigned long long)o.st0,
                             (unsigned long long)p.slot, (unsigned long long)o.slot, p.eax, o.eax, p.err, o.err);
                if (fdiff >= 0) std::fprintf(stderr, " frame A-%#x port %08x orig %08x", LO - 4 * uint32_t(fdiff), p.frame[fdiff], o.frame[fdiff]);
                std::fprintf(stderr, "\n");
            } else if (xb == XS[0] && f == 0) {
                std::fprintf(stderr, "[nanprobe] floor(default NaN) = %016llx, eax %08x (errno at %08x = %u), slot %016llx\n",
                             (unsigned long long)o.st0, o.eax, errno_at, o.err, (unsigned long long)o.slot);
            }
        }
    }
    std::fprintf(stderr, "[nanprobe] part A (%s): %d cases (floor / ceil x %zu inputs), mismatches %d\n",
                 rx_rd32(0x785bc644u) ? "the EXE's _matherr installed" : "msvcr90's default _matherr", total, sizeof XS / sizeof XS[0], bad);
}
/* part A again with the EXE's handler installed as its pre_c_init 009005c6 installs it (__setusermatherr 78541bff(_matherr 009008e5):
 * flag 1, the handler encoded). The world images have no EncodePointer (an unmodelled import): the handler is stored as the value the
 * thread's _decode_pointer maps to 009008e5 (the world runtime's DecodePointer stand-in at ptd + 0x1fc returns its argument). */
void part_a_user(reflex_vm::Machine& m) {
    const uint32_t flag = rx_rd32(0x785bc644u), handler = rx_rd32(0x785bd7acu);
    rx_wr32(0x785bc644u, 1); rx_wr32(0x785bd7acu, 0x009008e5u);
    const uint32_t a[1] = {0x009008e5u};
    const uint32_t dec = rx_call_original(0x78543607u, RX_CDECL, 0, 1, a);
    std::fprintf(stderr, "[nanprobe] ptd+0x1fc %08x, _decode_pointer(009008e5) = %08x\n", rx_rd32(crt_ptd() + 0x1fc), dec);
    if (dec != 0x009008e5u) { rx_wr32(0x785bc644u, flag); rx_wr32(0x785bd7acu, handler); return; }
    part_a(m);
    rx_wr32(0x785bc644u, flag); rx_wr32(0x785bd7acu, handler);
}

/* part B: the game entry points on NaN inputs (checked by the adapters under REFLEX_PORT_VERIFY) */
void part_b() {
    unsigned n = 0;
    for (uint64_t xb : XS) for (uint32_t fn : {0x785a2f90u, 0x785a2da0u}) {
        const uint32_t args[2] = {uint32_t(xb), uint32_t(xb >> 32)};
        rx_regs in{}, out{};
        in.r[3] = MARK_EBX; in.r[5] = MARK_EBP; in.r[6] = MARK_ESI; in.r[7] = MARK_EDI;
        rx_call_original_ex(fn, &in, &out, 2, args); n++;
    }
    const uint32_t grid = alloc(0x100), data = alloc(2048);
    for (uint32_t i = 0; i < 512; ++i) rx_wr32(data + 4 * i, 0x9e3779b9u * (i + 1));
    for (unsigned l = 0; l < 4; l++) {
        rx_wr32(grid + 0x48 + 4 * l, rx_oracle::bits(8.0f + float(l)));
        rx_wr32(grid + 0x38 + 4 * l, rx_oracle::bits(9.0f));
        rx_wr32(grid + 0x68 + 4 * l, 4 + 3 * l);
        rx_wr32(grid + 0x88 + 4 * l, data);
    }
    const uint32_t fnan[] = {0xffc00000u, 0x7fc00000u, 0x7fa00000u, 0xff812345u, 0x3e800000u /* 0.25 */};
    unsigned ns = 0;
    for (unsigned l = 0; l < 4; l++) for (uint32_t ub : fnan) for (uint32_t vb : fnan) {
        rx_regs in{}, out{};
        in.r[0] = l; in.r[1] = grid;
        in.r[3] = MARK_EBX; in.r[5] = MARK_EBP; in.r[6] = MARK_ESI; in.r[7] = MARK_EDI;
        const uint32_t args[] = {ub, vb};
        rx_call_original_ex(0x007f8e00, &in, &out, 2, args); ns++;
    }
    /* TerrainMaterialAt with the plain grid (+0x10 = 0: the 007f8e00 branch) and NaN point coordinates */
    const uint32_t terrain = alloc(0x4500), materials = alloc(0x60), point = alloc(16);
    rx_wr32(terrain + 0x44ac, materials); rx_wr32(materials + 0x44, grid);
    rx_wr32(terrain + 0x44d0, rx_oracle::bits(2.0f)); rx_wr32(terrain + 0x44d4, rx_oracle::bits(2.0f));
    unsigned nt = 0;
    for (unsigned l = 0; l < 4; l++) for (uint32_t xb : fnan) for (uint32_t zb : fnan) {
        rx_wr32(point, xb); rx_wr32(point + 8, zb);
        rx_regs in{}, out{};
        in.r[0] = l; in.r[2] = point; in.r[7] = terrain;
        in.r[3] = MARK_EBX; in.r[5] = MARK_EBP; in.r[6] = MARK_ESI;
        rx_call_original_ex(0x007c5120, &in, &out, 0, nullptr); nt++;
    }
    std::fprintf(stderr, "[nanprobe] part B: %u floor / ceil calls, %u PackedGrid_SampleUV calls, %u TerrainMaterialAt calls (NaN u / v)\n", n, ns, nt);
}
}  // namespace

RX_PORT(0x0084ed20, NanProbeHook, "thunk", 0) {
    static const unsigned at = std::getenv("RX_NAN_PROBE_FRAME") ? unsigned(std::atoi(std::getenv("RX_NAN_PROBE_FRAME"))) : 5;
    if (!std::getenv("RX_NAN_PROBE") || ++g_frame != at) return reflex_vm::RUN_ORIGINAL;
    uint32_t regs[8]; reflex_vm::Xmm xmm[8];
    std::memcpy(regs, m.r, sizeof regs); std::memcpy(xmm, m.xmm, sizeof xmm);
    const uint32_t flags = m.flags; auto fpu = m.fpu;
    /* RX_NAN_PROBE=b: part B only (part A's crt_bypass calls count as declined calls in the Verify summary) */
    const bool a_on = std::strcmp(std::getenv("RX_NAN_PROBE"), "b") != 0;
    if (a_on) part_a(m);
    std::memcpy(m.r, regs, sizeof regs); std::memcpy(m.xmm, xmm, sizeof xmm); m.flags = flags; m.fpu = fpu;
    if (a_on && rx_rd32(0x785bc644u) == 0) part_a_user(m);
    std::memcpy(m.r, regs, sizeof regs); std::memcpy(m.xmm, xmm, sizeof xmm); m.flags = flags; m.fpu = fpu;
    part_b();
    std::memcpy(m.r, regs, sizeof regs); std::memcpy(m.xmm, xmm, sizeof xmm); m.flags = flags; m.fpu = fpu;
    return reflex_vm::RUN_ORIGINAL;
}
