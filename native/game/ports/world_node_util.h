// Helpers of the world group's "node" ports (0x0078-0x008b: node hierarchy, key maps, input, audio).
#pragma once
#include "../port.h"
#include "world_util.h"
#include "f80/f80.h"

namespace world_node {

using namespace world;

inline X splat(const X& a, int i) { return X::splat(a.u[i]); }

// The node evaluator's 4x4 products (row i of the local matrix L times the parent world matrix R, rows R0..R3), with the xmm
// registers the original leaves (xmm0 = L row 3, xmm1..4 = the result rows, xmm5..7 scratch as listed).
struct MatOut { X x[8]; };

// 007db955 form: (w R3 + y R1) + (z R2 + x R0). Leaves xmm5 = z R2 + x R0 (row 3), xmm6 = x R0 (row 3), xmm7 = R0.
inline MatOut mat_mul_a(const X L[4], const X R[4]) {
    MatOut o;
    X x5, x6;
    for (int i = 0; i < 4; i++) {
        X a = addps(mulps(splat(L[i], 3), R[3]), mulps(splat(L[i], 1), R[1]));
        X xr0 = mulps(splat(L[i], 0), R[0]);
        X b = addps(mulps(splat(L[i], 2), R[2]), xr0);
        o.x[1 + i] = addps(a, b);
        x5 = b; x6 = xr0;
    }
    o.x[0] = L[3]; o.x[5] = x5; o.x[6] = x6; o.x[7] = R[0];
    return o;
}
// 007dbab0 form: (z R2 + x R0) + (w R3 + y R1). Leaves xmm5 = y R1 (row 3), xmm6 = w R3 + y R1 (row 3), xmm7 = y R1 (row 2).
inline MatOut mat_mul_b(const X L[4], const X R[4]) {
    MatOut o;
    X yr1[4], b3{};
    for (int i = 0; i < 4; i++) {
        X a = addps(mulps(splat(L[i], 2), R[2]), mulps(splat(L[i], 0), R[0]));
        yr1[i] = mulps(splat(L[i], 1), R[1]);
        X b = addps(mulps(splat(L[i], 3), R[3]), yr1[i]);
        o.x[1 + i] = addps(a, b);
        b3 = b;
    }
    o.x[0] = L[3]; o.x[5] = yr1[3]; o.x[6] = b3; o.x[7] = yr1[2];
    return o;
}
inline void ld4(const Guest& g, uint32_t a, X out[4]) { for (int i = 0; i < 4; i++) out[i] = ld(g, a + 16 * i); }
inline void put_xmm(Guest& g, const MatOut& o) { for (int i = 0; i < 8; i++) xw(g, i, o.x[i]); }


// Scalar double (SSE2 sd ops on the low lane) with the x86 NaN rule: the first NaN operand quieted, else the second, else the
// default NaN 0xfff8000000000000. Conversions keep the NaN payload (quieted).
inline uint64_t dbits(double d) { uint64_t b; std::memcpy(&b, &d, 8); return b; }
inline double dflt(uint64_t b) { double d; std::memcpy(&d, &b, 8); return d; }
inline bool dnan_bits(uint64_t b) { return (b & 0x7fffffffffffffffull) > 0x7ff0000000000000ull; }
inline double dnan_rule(double a, double b) {
    uint64_t x = dbits(a), y = dbits(b);
    return dflt(dnan_bits(x) ? (x | 0x0008000000000000ull) : dnan_bits(y) ? (y | 0x0008000000000000ull) : 0xfff8000000000000ull);
}
inline double dadd(double a, double b) { double r = a + b; return r != r ? dnan_rule(a, b) : r; }
inline double dsub(double a, double b) { double r = a - b; return r != r ? dnan_rule(a, b) : r; }
inline double dmul(double a, double b) { double r = a * b; return r != r ? dnan_rule(a, b) : r; }
inline double ddiv(double a, double b) { double r = a / b; return r != r ? dnan_rule(a, b) : r; }
// cvtss2sd / cvtps2pd lane
inline double f2d(uint32_t v) {
    if (x86::isnan_bits(v)) return dflt((uint64_t(v >> 31) << 63) | 0x7ff8000000000000ull | (uint64_t(v & 0x3fffffu) << 29));
    return double(x86::flt(v));
}
// cvtsd2ss / cvtpd2ps lane (round to nearest even)
inline uint32_t d2f(double d) {
    uint64_t b = dbits(d);
    if (dnan_bits(b)) return uint32_t(b >> 63) << 31 | 0x7fc00000u | uint32_t((b >> 29) & 0x3fffffu);
    return x86::bits(float(d));
}


// x87 precision / rounding control of the machine's control word (as Machine::fbits / frc), and st(0)
inline int fbits(const reflex_vm::Machine& m) { int pc = (m.fpu_cw >> 8) & 3; return pc == 0 ? 24 : pc == 2 ? 53 : 64; }
inline int frc(const reflex_vm::Machine& m) { return (m.fpu_cw >> 10) & 3; }
inline reflex_f80::F80& st0(reflex_vm::Machine& m) { return m.fpu.back(); }
// fistp (qword / dword) of an F80 with rounding control rc, as the interpreter's (vm.cpp): integer indefinite when invalid.
inline int64_t f80_fist(const reflex_f80::F80& v, int rc, int size) {
    bool valid = v.kind == 0; int64_t n = 0;
    if (valid) {
        reflex_f80::F80 ri = reflex_f80::round_to_integer(v, rc);
        if (ri.mant != 0) {
            int shift = 63 - ri.exp;
            if (ri.exp > 63 || (ri.exp == 63 && !(ri.sign && ri.mant == (1ull << 63)))) valid = false;
            else if (ri.exp == 63) n = INT64_MIN;
            if (valid && ri.exp < 63) { uint64_t mag = ri.mant >> shift; n = ri.sign ? -int64_t(mag) : int64_t(mag); }
        }
    }
    if (size == 8) { if (!valid) n = INT64_MIN; }
    else { int64_t lim = int64_t(1) << (8 * size - 1); if (!valid || n < -lim || n >= lim) n = -lim; }
    return n;
}
// comiss a, b: CF (below or unordered)
inline bool comiss_cf(uint32_t a, uint32_t b) {
    if (x86::isnan_bits(a) || x86::isnan_bits(b)) return true;
    return x86::flt(a) < x86::flt(b);
}

}  // namespace world_node
