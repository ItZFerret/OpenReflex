// Block JIT (see jit.h). AArch64 macOS only; elsewhere every lookup declines.
//
// Translated block: guest registers stay in the Machine (m.r[], m.flags, m.xmm[]), so each instruction is a short
// load/compute/store sequence and calls back into C++ need no register spilling:
//   x19 Machine*, x20 page table (resident pages), x21 write table (dirty, writable pages), x22 &m.xmm[0],
//   x23 parity table (x86 PF bit per byte value); w9..w17 / v0..v7 temporaries.
// Memory accesses take an inline path when the page is resident (writes: dirty and writable) and the access does not
// cross the page, else a helper (Memory::read / write: same semantics, faults become exceptions caught there).
// Instructions that are not translated call the interpreter's handler for that instruction (JitHelpers::exec); a
// handler whose next pc is not the fall-through ends the block there. Faults end the block with bit 32 set and the
// exception in m.jit_exception. The instruction counter m.steps advances by the instructions completed.
#include "jit.h"
#include "vm.h"
#include <algorithm>
#include <cstdio>
#include <string>
#include <functional>
#include <memory>
#include <cstdlib>
#include <cstring>
#include <cmath>
#if defined(__aarch64__) && defined(__APPLE__)
#include <libkern/OSCacheControl.h>
#include <pthread.h>
#include <sys/mman.h>
#define REFLEX_JIT_HOST 1
#endif

namespace reflex_vm {

const char* mnemonic_name(int i);
static std::unordered_map<const Insn*, uint64_t> g_helper_sites;   // debugging (Machine::jit_histo set): helper executions per instruction
static const bool g_x87_slow = std::getenv("REFLEX_X87_SLOW") != nullptr;   // diagnostics: every x87 op through f80.cpp
void jit_print_forms(int top) {
    std::unordered_map<std::string, uint64_t> forms;
    for (auto& [in, n] : g_helper_sites) {
        std::string f = mnemonic_name(in->mn);
        for (int i = 0; i < in->n; i++) {
            const Operand& o = in->o[i]; char b[24];
            if (o.type == 1) std::snprintf(b, sizeof b, " r%d/%d", o.rclass, o.size); else if (o.type == 2) std::snprintf(b, sizeof b, " i%d", o.size); else std::snprintf(b, sizeof b, " m%d", o.size);
            f += b;
        }
        forms[f] += n;
    }
    std::vector<std::pair<uint64_t, std::string>> v; for (auto& [f, n] : forms) v.push_back({n, f}); std::sort(v.rbegin(), v.rend());
    for (int i = 0; i < top && i < int(v.size()); i++) std::printf("%12llu %s\n", (unsigned long long)v[i].first, v[i].second.c_str());
}
struct JitHelpers {
    static uint64_t exec(Machine* m, const Insn* in) {
        if (__builtin_expect(m->jit_histo != nullptr, 0)) { m->jit_histo[in->mn]++; g_helper_sites[in]++; }
        try { return in->fast ? in->fast(*m, *in) : m->execute(*in); }
        catch (...) { m->jit_exception = std::current_exception(); return 1ull << 32; }
    }
    // Fast paths for the common x87 cases, bit-identical to f80.cpp (which stays the reference and handles everything else):
    // normal values move between F80 and the IEEE formats exactly, and add / sub / mul / div of operands that are exact doubles
    // (at most 53 significant bits, exponents far from the double limits) run on the host's double arithmetic when the result
    // is provably the one f80's rounding gives: PC 53 round-to-nearest is IEEE double rounding; PC 24 rounds that double again,
    // which equals one rounding of the exact result unless the double lies exactly on a 24-bit midpoint without being exact;
    // PC 64 only when the double result is exact. Anything else (other rounding modes, zero divisors, NaN / infinity,
    // subnormal or huge magnitudes, the midpoint case) takes the f80 path. Measured: x87 was ~20% of a 12-rider frame.
    static inline bool f80_from_f32_fast(uint32_t b, reflex_f80::F80& v) {
        if (g_x87_slow) return false;
        uint32_t ex = (b >> 23) & 0xff;
        if (ex == 0 || ex == 0xff) { if ((b & 0x7fffffff) != 0) return false; v.kind = 0; v.sign = uint8_t(b >> 31); v.mant = 0; v.exp = 0; return true; }
        v.kind = 0; v.sign = uint8_t(b >> 31); v.mant = (uint64_t(0x800000 | (b & 0x7fffff))) << 40; v.exp = int32_t(ex) - 127; return true;
    }
    static inline bool f80_from_f64_fast(uint64_t b, reflex_f80::F80& v) {
        if (g_x87_slow) return false;
        uint32_t ex = uint32_t(b >> 52) & 0x7ff;
        if (ex == 0 || ex == 0x7ff) { if ((b << 1) != 0) return false; v.kind = 0; v.sign = uint8_t(b >> 63); v.mant = 0; v.exp = 0; return true; }
        v.kind = 0; v.sign = uint8_t(b >> 63); v.mant = ((1ull << 52) | (b & ((1ull << 52) - 1))) << 11; v.exp = int32_t(ex) - 1023; return true;
    }
    // round-to-nearest store of a normal F80 into an IEEE format with mbits significand bits (24 / 53) and exponent range
    static inline bool f80_store_fast(const reflex_f80::F80& v, int mbits, int emin, int emax, uint64_t& out) {
        if (v.kind != 0 || g_x87_slow) return false;
        int width = mbits == 24 ? 32 : 64;
        uint64_t sign = uint64_t(v.sign) << (width - 1);
        if (v.mant == 0) { out = sign; return true; }
        int64_t e = v.exp; if (e < emin || e > emax) return false;
        int drop = 64 - mbits; uint64_t n = v.mant >> drop, rem = v.mant & ((1ull << drop) - 1), half = 1ull << (drop - 1);
        if (rem > half || (rem == half && (n & 1))) n++;
        if (n == (1ull << mbits)) { n >>= 1; e++; if (e > emax) return false; }
        out = sign | (uint64_t(e + (emax)) << (mbits - 1)) | (n - (1ull << (mbits - 1)));   // bias = emax
        return true;
    }
    static inline bool f80_to_double(const reflex_f80::F80& v, double& d) {
        if (v.kind != 0) return false;
        uint64_t b;
        if (v.mant == 0) b = uint64_t(v.sign) << 63;
        else { if ((v.mant & 0x7ff) != 0 || v.exp < -960 || v.exp > 960) return false; b = (uint64_t(v.sign) << 63) | (uint64_t(v.exp + 1023) << 52) | ((v.mant >> 11) & ((1ull << 52) - 1)); }
        std::memcpy(&d, &b, 8); return true;
    }
    // op as x87_apply (0 add, 1 mul, 4 x - y, 5 y - x, 6 x / y, 7 y / x); false: use f80
    static inline bool x87_apply_fast(int op, const reflex_f80::F80& x, const reflex_f80::F80& y, int bits, int rc, reflex_f80::F80& out) {
        double a, b;
        if (rc != 0 || !f80_to_double(x, a) || !f80_to_double(y, b)) return false;
        if (op == 5 || op == 7) std::swap(a, b);
        double r; int kind;   // 0 add, 1 mul, 2 div
        switch (op) {
            case 0: r = a + b; kind = 0; break;
            case 4: case 5: b = -b; r = a + b; kind = 0; break;
            case 1: r = a * b; kind = 1; break;
            case 6: case 7: if (b == 0) return false; r = a / b; kind = 2; break;
            default: return false;
        }
        uint64_t rb; std::memcpy(&rb, &r, 8);
        uint32_t rex = uint32_t(rb >> 52) & 0x7ff;
        bool zero = (rb << 1) == 0;
        if (zero && kind != 0 && a != 0 && (kind == 2 || b != 0)) return false;   // a product / quotient that underflowed the double range
        if (!zero && (rex < 1023 - 960 || rex > 1023 + 960)) return false;   // keep away from double subnormals / overflow
        auto exact = [&]() -> bool {
            if (kind == 0) { double bb = r - a, err = (a - (r - bb)) + (b - bb); return err == 0; }   // TwoSum
            if (kind == 1) return std::fma(a, b, -r) == 0;
            return std::fma(-r, b, a) == 0;                                                           // a - r*b exactly
        };
        if (bits == 64) { if (!exact()) return false; }
        else if (bits == 24 && !zero) {
            uint64_t low = rb & ((1ull << 29) - 1), half = 1ull << 28;
            if (low == half && !exact()) return false;   // a midpoint of the double: the double rounding could differ
            if (low > half || (low == half && (rb >> 29 & 1))) rb += 1ull << 29;
            rb &= ~((1ull << 29) - 1);
            rex = uint32_t(rb >> 52) & 0x7ff;
            if (rex > 1023 + 960) return false;
        } else if (bits != 53 && bits != 24) return false;
        out.kind = 0; out.sign = uint8_t(rb >> 63);
        if (zero) { out.mant = 0; out.exp = 0; }
        else { out.mant = (1ull << 63) | ((rb & ((1ull << 52) - 1)) << 11); out.exp = int32_t(rex) - 1023; }
        return true;
    }
    // x87 forms with their own entry (same contract as exec; anything unusual runs the interpreter's handler)
    static uint64_t x87_ld(Machine* m, const Insn* in, uint32_t a) {   // fld m32 / m64
        if (m->mmx_active || m->fpu.size() >= 8) return exec(m, in);
        try {
            reflex_f80::F80 v;
            if (in->o[0].size == 4) { uint32_t b = uint32_t(m->mem.read(a, 4)); if (!f80_from_f32_fast(b, v)) v = reflex_f80::from_f32(b); }
            else { uint64_t b = m->mem.read(a, 8); if (!f80_from_f64_fast(b, v)) v = reflex_f80::from_f64(b); }
            m->fpu.push_back(v);
            return in->next;
        } catch (...) { m->jit_exception = std::current_exception(); return 1ull << 32; }
    }
    static uint64_t x87_st(Machine* m, const Insn* in, uint32_t a) {   // fst / fstp m32 / m64
        if (m->mmx_active || m->fpu.empty()) return exec(m, in);
        try {
            const reflex_f80::F80& v = m->fpu.back(); int rc = m->frc(); uint64_t out;
            if (in->o[0].size == 4) m->mem.write(a, 4, rc == 0 && f80_store_fast(v, 24, -126, 127, out) ? out : reflex_f80::to_f32(v, rc));
            else m->mem.write(a, 8, rc == 0 && f80_store_fast(v, 53, -1022, 1023, out) ? out : reflex_f80::to_f64(v, rc));
            if (in->mn == M_fstp) m->fpu.pop_back();
            return in->next;
        } catch (...) { m->jit_exception = std::current_exception(); return 1ull << 32; }
    }
    static uint64_t x87_reg(Machine* m, const Insn* in, uint32_t) {   // fld st(i) / fst(p) st(i) / fxch / fld1 / fldz
        auto& f = m->fpu; size_t n = f.size();
        if (m->mmx_active) return exec(m, in);
        switch (in->mn) {
            case M_fld: { size_t i = in->o[0].ridx; if (n >= 8 || i >= n) return exec(m, in); reflex_f80::F80 v = f[n - 1 - i]; f.push_back(v); break; }
            case M_fst: case M_fstp: { size_t i = in->o[0].ridx; if (n == 0 || i >= n) return exec(m, in); f[n - 1 - i] = f.back(); if (in->mn == M_fstp) f.pop_back(); break; }
            case M_fxch: { size_t i = in->n ? in->o[in->n - 1].ridx : 1; if (n == 0 || i >= n) return exec(m, in); std::swap(f[n - 1], f[n - 1 - i]); break; }
            case M_fld1: if (n >= 8) return exec(m, in); f.push_back(reflex_f80::from_int(1)); break;
            default: if (n >= 8) return exec(m, in); f.push_back(reflex_f80::from_int(0)); break;   // fldz
        }
        return in->next;
    }
    // x87 control word, integer loads / stores, arithmetic with a memory or register operand, and the MMX int -> float
    // pattern: the interpreter's code for these forms (vm.cpp execute_fp) without its dispatch
    static uint64_t x87_cw(Machine* m, const Insn* in, uint32_t a) {   // fldcw / fnstcw m16
        if (m->mmx_active) return exec(m, in);
        try { if (in->mn == M_fldcw) m->fpu_cw = uint16_t(m->mem.read(a, 2)); else m->mem.write(a, 2, m->fpu_cw); return in->next; }
        catch (...) { m->jit_exception = std::current_exception(); return 1ull << 32; }
    }
    static uint64_t x87_fild(Machine* m, const Insn* in, uint32_t a) {   // fild m16 / m32 / m64
        if (m->mmx_active || m->fpu.size() >= 8) return exec(m, in);
        try {
            int size = in->o[0].size; uint64_t v = m->mem.read(a, size); int bits = 8 * size;
            int64_t n = bits == 64 ? int64_t(v) : int64_t((v ^ (1ull << (bits - 1))) - (1ull << (bits - 1)));
            m->fpu.push_back(reflex_f80::from_int(n));
            return in->next;
        } catch (...) { m->jit_exception = std::current_exception(); return 1ull << 32; }
    }
    static uint64_t x87_fist(Machine* m, const Insn* in, uint32_t a) {   // fist / fistp m16 / m32 / m64 (current rounding)
        if (m->mmx_active || m->fpu.empty()) return exec(m, in);
        try {
            using namespace reflex_f80;
            const F80 v = m->fpu.back(); int size = in->o[0].size; bool valid = v.kind == 0; int64_t n = 0;
            if (valid) {
                F80 ri = round_to_integer(v, m->frc());
                if (ri.mant != 0) {
                    int shift = 63 - ri.exp;
                    if (ri.exp > 63 || (ri.exp == 63 && !(ri.sign && ri.mant == (1ull << 63)))) valid = false;
                    else if (ri.exp == 63) n = INT64_MIN;
                    if (valid && ri.exp < 63) { uint64_t mag = ri.mant >> shift; n = ri.sign ? -int64_t(mag) : int64_t(mag); }
                }
            }
            if (size == 8) { if (!valid) n = INT64_MIN; }
            else { int64_t lim = int64_t(1) << (8 * size - 1); if (!valid || n < -lim || n >= lim) n = -lim; }
            m->mem.write(a, size, uint64_t(n) & (size == 8 ? ~0ull : (1ull << (8 * size)) - 1));
            if (in->mn == M_fistp) m->fpu.pop_back();
            return in->next;
        } catch (...) { m->jit_exception = std::current_exception(); return 1ull << 32; }
    }
    static reflex_f80::F80 x87_apply(int op, const reflex_f80::F80& x, const reflex_f80::F80& y, int bits, int rc) {
        using namespace reflex_f80;
        if (!g_x87_slow) { F80 r; if (x87_apply_fast(op, x, y, bits, rc, r)) return r; }
        switch (op) {
            case 0: return add(x, y, bits, rc);
            case 1: return mul(x, y, bits, rc);
            case 4: return add(x, y, bits, rc, true);
            case 5: return add(y, x, bits, rc, true);
            case 6: return div(x, y, bits, rc);
            default: return div(y, x, bits, rc);
        }
    }
    static int x87_opcode(const Insn* in, uint8_t& modrm) {   // the opcode byte after prefixes (as execute_fp)
        int k = 0;
        while (k < in->len && (in->raw[k] == 0x26 || in->raw[k] == 0x2e || in->raw[k] == 0x36 || in->raw[k] == 0x3e || in->raw[k] == 0x64 ||
                               in->raw[k] == 0x65 || in->raw[k] == 0x66 || in->raw[k] == 0x67 || in->raw[k] == 0xf2 || in->raw[k] == 0xf3)) k++;
        modrm = in->raw[k + 1]; return in->raw[k];
    }
    static uint64_t x87_arith(Machine* m, const Insn* in, uint32_t a) {   // fadd .. fdivr(p) with m32 / m64 / st(i) (not fi*)
        if (m->mmx_active || m->fpu.empty()) return exec(m, in);
        uint8_t modrm; int opcode = x87_opcode(in, modrm), reg = (modrm >> 3) & 7;
        if (reg == 2 || reg == 3) return exec(m, in);
        try {
            using namespace reflex_f80;
            auto& f = m->fpu; size_t n = f.size(); int bits = m->fbits(), rc = m->frc();
            if (modrm >= 0xc0) {
                size_t i = modrm & 7; if (i >= n) return exec(m, in);
                if (opcode == 0xd8) f[n - 1] = x87_apply(reg, f[n - 1], f[n - 1 - i], bits, rc);
                else if (opcode == 0xdc || opcode == 0xde) {
                    static const int swap[8] = {0, 1, -1, -1, 5, 4, 7, 6};
                    f[n - 1 - i] = x87_apply(swap[reg], f[n - 1 - i], f[n - 1], bits, rc);
                    if (opcode == 0xde) f.pop_back();
                } else return exec(m, in);
            } else {
                F80 src;
                if (opcode == 0xd8) { uint32_t b = uint32_t(m->mem.read(a, 4)); if (!f80_from_f32_fast(b, src)) src = from_f32(b); }
                else if (opcode == 0xdc) { uint64_t b = m->mem.read(a, 8); if (!f80_from_f64_fast(b, src)) src = from_f64(b); }
                else return exec(m, in);
                f[n - 1] = x87_apply(reg, f[n - 1], src, bits, rc);
            }
            return in->next;
        } catch (...) { m->jit_exception = std::current_exception(); return 1ull << 32; }
    }
    static uint64_t mmx_ops(Machine* m, const Insn* in, uint32_t a) {   // movq mm, m64 / cvtpi2ps xmm, mm / emms
        try {
            if (in->mn == M_emms) { if (m->mmx_active) { m->fpu.clear(); m->mmx_active = false; } return in->next; }
            if (in->mn == M_movq) { if (!m->mmx_active && !m->fpu.empty()) return exec(m, in); uint64_t v = m->mem.read(a, 8); m->mmx_active = true; m->mmx[in->o[0].ridx] = v; return in->next; }
            if (!m->mmx_active) return exec(m, in);   // cvtpi2ps
            uint64_t v = m->mmx[in->o[1].ridx];
            volatile float f0 = float(int32_t(uint32_t(v))), f1 = float(int32_t(uint32_t(v >> 32)));
            uint32_t b0, b1; float t0 = f0, t1 = f1; std::memcpy(&b0, &t0, 4); std::memcpy(&b1, &t1, 4);
            m->xmm[in->o[0].ridx].lo = uint64_t(b0) | (uint64_t(b1) << 32);
            return in->next;
        } catch (...) { m->jit_exception = std::current_exception(); return 1ull << 32; }
    }
    struct Load { uint64_t value, fault; };
    static Load load(Machine* m, uint32_t a, uint32_t n) {
        try { return {m->mem.read(a, int(n)), 0}; }
        catch (...) { m->jit_exception = std::current_exception(); return {0, 1}; }
    }
    static uint64_t store(Machine* m, uint32_t a, uint64_t v, uint32_t n) {
        try { m->mem.write(a, int(n), v); return 0; }
        catch (...) { m->jit_exception = std::current_exception(); return 1; }
    }
};

#if REFLEX_JIT_HOST
namespace {
// ---------------------------------------------------------------- AArch64 assembler (the subset used)
enum Cond { EQ = 0, NE = 1, HS = 2, LO = 3, MI = 4, PL = 5, VS = 6, VC = 7, HI = 8, LS = 9, GE = 10, LT = 11, GT = 12, LE = 13 };
constexpr int XZR = 31, SP = 31;
struct Label { int pos = -1; std::vector<std::pair<int, int>> uses; };   // uses: (instruction index, kind 0 imm19, 1 imm26)

struct Asm {
    std::vector<uint32_t> c;
    void e(uint32_t i) { c.push_back(i); }
    int here() const { return int(c.size()); }
    void bind(Label& l) {
        l.pos = here();
        for (auto [at, kind] : l.uses) patch(at, kind, l.pos);
        l.uses.clear();
    }
    void patch(int at, int kind, int target) {
        int32_t d = target - at;
        if (kind == 0) c[size_t(at)] = (c[size_t(at)] & ~(0x7ffffu << 5)) | ((uint32_t(d) & 0x7ffff) << 5);
        else c[size_t(at)] = (c[size_t(at)] & ~0x3ffffffu) | (uint32_t(d) & 0x3ffffff);
    }
    void ref(Label& l, int kind) { if (l.pos >= 0) patch(here() - 1, kind, l.pos); else l.uses.push_back({here() - 1, kind}); }
    // branches
    void b(Label& l) { e(0x14000000); ref(l, 1); }
    void bcond(int cond, Label& l) { e(0x54000000 | uint32_t(cond)); ref(l, 0); }
    void cbz(int rt, Label& l, bool x = false) { e((x ? 0xB4000000u : 0x34000000u) | uint32_t(rt)); ref(l, 0); }
    void cbnz(int rt, Label& l, bool x = false) { e((x ? 0xB5000000u : 0x35000000u) | uint32_t(rt)); ref(l, 0); }
    void blr(int rn) { e(0xD63F0000 | uint32_t(rn) << 5); }
    void ret() { e(0xD65F03C0); }
    // moves
    void movz(int rd, uint32_t imm16, int shift, bool x) { e((x ? 0xD2800000u : 0x52800000u) | uint32_t(shift / 16) << 21 | (imm16 & 0xffff) << 5 | uint32_t(rd)); }
    void movk(int rd, uint32_t imm16, int shift, bool x) { e((x ? 0xF2800000u : 0x72800000u) | uint32_t(shift / 16) << 21 | (imm16 & 0xffff) << 5 | uint32_t(rd)); }
    void mov32(int rd, uint32_t v) { movz(rd, v & 0xffff, 0, false); if (v >> 16) movk(rd, v >> 16, 16, false); }
    void mov64(int rd, uint64_t v) {
        movz(rd, uint32_t(v & 0xffff), 0, true);
        for (int s = 16; s < 64; s += 16) if ((v >> s) & 0xffff) movk(rd, uint32_t((v >> s) & 0xffff), s, true);
    }
    void movx(int rd, int rm) { e(0xAA0003E0 | uint32_t(rm) << 16 | uint32_t(rd)); }
    // arithmetic
    void addi(int rd, int rn, uint32_t imm, bool x = false) { e((x ? 0x91000000u : 0x11000000u) | (imm & 0xfff) << 10 | uint32_t(rn) << 5 | uint32_t(rd)); }
    void subi(int rd, int rn, uint32_t imm, bool x = false) { e((x ? 0xD1000000u : 0x51000000u) | (imm & 0xfff) << 10 | uint32_t(rn) << 5 | uint32_t(rd)); }
    void subsi(int rd, int rn, uint32_t imm) { e(0x71000000 | (imm & 0xfff) << 10 | uint32_t(rn) << 5 | uint32_t(rd)); }
    void addsi(int rd, int rn, uint32_t imm) { e(0x31000000 | (imm & 0xfff) << 10 | uint32_t(rn) << 5 | uint32_t(rd)); }
    void add(int rd, int rn, int rm, int lsl = 0, bool x = false) { e((x ? 0x8B000000u : 0x0B000000u) | uint32_t(rm) << 16 | uint32_t(lsl) << 10 | uint32_t(rn) << 5 | uint32_t(rd)); }
    void adds(int rd, int rn, int rm) { e(0x2B000000 | uint32_t(rm) << 16 | uint32_t(rn) << 5 | uint32_t(rd)); }
    void subs(int rd, int rn, int rm) { e(0x6B000000 | uint32_t(rm) << 16 | uint32_t(rn) << 5 | uint32_t(rd)); }
    void and_(int rd, int rn, int rm) { e(0x0A000000 | uint32_t(rm) << 16 | uint32_t(rn) << 5 | uint32_t(rd)); }
    void orr(int rd, int rn, int rm, int lsl = 0) { e(0x2A000000 | uint32_t(rm) << 16 | uint32_t(lsl) << 10 | uint32_t(rn) << 5 | uint32_t(rd)); }
    void eor(int rd, int rn, int rm, int lsr = 0) { e(0x4A000000 | 1u << 22 | uint32_t(rm) << 16 | uint32_t(lsr) << 10 | uint32_t(rn) << 5 | uint32_t(rd)); }   // eor rd, rn, rm, lsr #n
    void ands(int rd, int rn, int rm) { e(0x6A000000 | uint32_t(rm) << 16 | uint32_t(rn) << 5 | uint32_t(rd)); }
    void lsr_imm(int rd, int rn, int sh, bool x = false) { if (x) e(0xD340FC00 | uint32_t(sh) << 16 | uint32_t(rn) << 5 | uint32_t(rd)); else e(0x53007C00 | uint32_t(sh) << 16 | uint32_t(rn) << 5 | uint32_t(rd)); }
    void and_fff(int rd, int rn) { e(0x12002C00 | uint32_t(rn) << 5 | uint32_t(rd)); }   // and wd, wn, #0xfff
    void and_ff(int rd, int rn) { e(0x12001C00 | uint32_t(rn) << 5 | uint32_t(rd)); }    // and wd, wn, #0xff
    void and_1(int rd, int rn) { e(0x12000000 | uint32_t(rn) << 5 | uint32_t(rd)); }     // and wd, wn, #1
    void tst_bit(int rn, int bit) { e(0x72000000 | uint32_t((32 - bit) % 32) << 16 | uint32_t(rn) << 5 | 31); }   // ands wzr, wn, #(1 << bit)
    void cset(int rd, int cond) { e(0x1A9F07E0 | uint32_t(cond ^ 1) << 12 | uint32_t(rd)); }
    void cmpi(int rn, uint32_t imm) { subsi(31, rn, imm); }
    void cmp(int rn, int rm) { subs(31, rn, rm); }
    // loads / stores: size 0 byte, 1 half, 2 word, 3 dword
    void ldr_u(int size, int rt, int rn, uint32_t off) { e(0x39400000 | uint32_t(size) << 30 | (off >> size) << 10 | uint32_t(rn) << 5 | uint32_t(rt)); }
    void str_u(int size, int rt, int rn, uint32_t off) { e(0x39000000 | uint32_t(size) << 30 | (off >> size) << 10 | uint32_t(rn) << 5 | uint32_t(rt)); }
    void ldr_r(int size, int rt, int rn, int rm, bool scaled = false) { e(0x38606800 | uint32_t(size) << 30 | uint32_t(rm) << 16 | (scaled ? 1u : 0u) << 12 | uint32_t(rn) << 5 | uint32_t(rt)); }
    void str_r(int size, int rt, int rn, int rm) { e(0x38206800 | uint32_t(size) << 30 | uint32_t(rm) << 16 | uint32_t(rn) << 5 | uint32_t(rt)); }
    void stp_pre(int rt, int rt2, int rn, int off) { e(0xA9800000 | (uint32_t(off / 8) & 0x7f) << 15 | uint32_t(rt2) << 10 | uint32_t(rn) << 5 | uint32_t(rt)); }
    void stp(int rt, int rt2, int rn, int off) { e(0xA9000000 | (uint32_t(off / 8) & 0x7f) << 15 | uint32_t(rt2) << 10 | uint32_t(rn) << 5 | uint32_t(rt)); }
    void ldp(int rt, int rt2, int rn, int off) { e(0xA9400000 | (uint32_t(off / 8) & 0x7f) << 15 | uint32_t(rt2) << 10 | uint32_t(rn) << 5 | uint32_t(rt)); }
    void ldp_post(int rt, int rt2, int rn, int off) { e(0xA8C00000 | (uint32_t(off / 8) & 0x7f) << 15 | uint32_t(rt2) << 10 | uint32_t(rn) << 5 | uint32_t(rt)); }
    void mov_sp(int rd) { e(0x910003E0 | uint32_t(rd)); }   // add xd, sp, #0
    // SIMD
    void ldr_q(int qt, int rn, uint32_t off) { e(0x3DC00000 | (off >> 4) << 10 | uint32_t(rn) << 5 | uint32_t(qt)); }
    void str_q(int qt, int rn, uint32_t off) { e(0x3D800000 | (off >> 4) << 10 | uint32_t(rn) << 5 | uint32_t(qt)); }
    void ldr_qr(int qt, int rn, int rm) { e(0x3CE06800 | uint32_t(rm) << 16 | uint32_t(rn) << 5 | uint32_t(qt)); }
    void str_qr(int qt, int rn, int rm) { e(0x3CA06800 | uint32_t(rm) << 16 | uint32_t(rn) << 5 | uint32_t(qt)); }
    void ldr_s(int st, int rn, uint32_t off) { e(0xBD400000 | (off >> 2) << 10 | uint32_t(rn) << 5 | uint32_t(st)); }
    void str_s(int st, int rn, uint32_t off) { e(0xBD000000 | (off >> 2) << 10 | uint32_t(rn) << 5 | uint32_t(st)); }
    void fmov_s_w(int sd, int wn) { e(0x1E270000 | uint32_t(wn) << 5 | uint32_t(sd)); }
    void fmov_w_s(int wd, int sn) { e(0x1E260000 | uint32_t(sn) << 5 | uint32_t(wd)); }
    void fmov_d_x(int dd, int xn) { e(0x9E670000 | uint32_t(xn) << 5 | uint32_t(dd)); }
    void fmov_x_d(int xd, int dn) { e(0x9E660000 | uint32_t(dn) << 5 | uint32_t(xd)); }
    void ins_d1_x(int vd, int xn) { e(0x4E181C00 | uint32_t(xn) << 5 | uint32_t(vd)); }   // ins vd.d[1], xn
    void umov_x_d1(int xd, int vn) { e(0x4E183C00 | uint32_t(vn) << 5 | uint32_t(xd)); }  // umov xd, vn.d[1]
    void ins_s(int vd, int i, int vn, int j) { e(0x6E000400 | uint32_t((i << 3) | 4) << 16 | uint32_t(j << 2) << 11 | uint32_t(vn) << 5 | uint32_t(vd)); }
    void fadd4(int d, int n, int m) { e(0x4E20D400 | uint32_t(m) << 16 | uint32_t(n) << 5 | uint32_t(d)); }
    void fsub4(int d, int n, int m) { e(0x4EA0D400 | uint32_t(m) << 16 | uint32_t(n) << 5 | uint32_t(d)); }
    void fmul4(int d, int n, int m) { e(0x6E20DC00 | uint32_t(m) << 16 | uint32_t(n) << 5 | uint32_t(d)); }
    void fadds(int d, int n, int m) { e(0x1E202800 | uint32_t(m) << 16 | uint32_t(n) << 5 | uint32_t(d)); }
    void fsubs(int d, int n, int m) { e(0x1E203800 | uint32_t(m) << 16 | uint32_t(n) << 5 | uint32_t(d)); }
    void fmuls(int d, int n, int m) { e(0x1E200800 | uint32_t(m) << 16 | uint32_t(n) << 5 | uint32_t(d)); }
    void fcmps(int n, int m) { e(0x1E202000 | uint32_t(m) << 16 | uint32_t(n) << 5); }
    void fcmeq4(int d, int n, int m) { e(0x4E20E400 | uint32_t(m) << 16 | uint32_t(n) << 5 | uint32_t(d)); }
    void vand(int d, int n, int m) { e(0x4E201C00 | uint32_t(m) << 16 | uint32_t(n) << 5 | uint32_t(d)); }
    void vorr(int d, int n, int m) { e(0x4EA01C00 | uint32_t(m) << 16 | uint32_t(n) << 5 | uint32_t(d)); }
    void veor(int d, int n, int m) { e(0x6E201C00 | uint32_t(m) << 16 | uint32_t(n) << 5 | uint32_t(d)); }
    void vbic(int d, int n, int m) { e(0x4E601C00 | uint32_t(m) << 16 | uint32_t(n) << 5 | uint32_t(d)); }   // d = n & ~m
    void uminv4(int d, int n) { e(0x6EB1A800 | uint32_t(n) << 5 | uint32_t(d)); }
    // more integer forms
    void sub_(int rd, int rn, int rm) { e(0x4B000000 | uint32_t(rm) << 16 | uint32_t(rn) << 5 | uint32_t(rd)); }
    void adcs(int rd, int rn, int rm) { e(0x3A000000 | uint32_t(rm) << 16 | uint32_t(rn) << 5 | uint32_t(rd)); }
    void sbcs(int rd, int rn, int rm) { e(0x7A000000 | uint32_t(rm) << 16 | uint32_t(rn) << 5 | uint32_t(rd)); }
    void orn(int rd, int rn, int rm) { e(0x2A200000 | uint32_t(rm) << 16 | uint32_t(rn) << 5 | uint32_t(rd)); }
    void lsl_imm(int rd, int rn, int s) { e(0x53000000 | uint32_t((32 - s) & 31) << 16 | uint32_t(31 - s) << 10 | uint32_t(rn) << 5 | uint32_t(rd)); }
    void asr_imm(int rd, int rn, int s) { e(0x13007C00 | uint32_t(s) << 16 | uint32_t(rn) << 5 | uint32_t(rd)); }
    void ubfx1(int rd, int rn, int bit) { e(0x53000000 | uint32_t(bit) << 16 | uint32_t(bit) << 10 | uint32_t(rn) << 5 | uint32_t(rd)); }
    void sxtb(int rd, int rn) { e(0x13001C00 | uint32_t(rn) << 5 | uint32_t(rd)); }
    void sxth(int rd, int rn) { e(0x13003C00 | uint32_t(rn) << 5 | uint32_t(rd)); }
    void and_ffff(int rd, int rn) { e(0x12003C00 | uint32_t(rn) << 5 | uint32_t(rd)); }
    void eor_1(int rd, int rn) { e(0x52000000 | uint32_t(rn) << 5 | uint32_t(rd)); }
    void smull(int xd, int wn, int wm) { e(0x9B207C00 | uint32_t(wm) << 16 | uint32_t(wn) << 5 | uint32_t(xd)); }
    void sxtw(int xd, int wn) { e(0x93407C00 | uint32_t(wn) << 5 | uint32_t(xd)); }
    void cmpx(int xn, int xm) { e(0xEB00001F | uint32_t(xm) << 16 | uint32_t(xn) << 5); }
    // more SIMD / FP
    void v3(uint32_t op, int d, int n, int m) { e(op | uint32_t(m) << 16 | uint32_t(n) << 5 | uint32_t(d)); }   // three-register forms
    void v2(uint32_t op, int d, int n) { e(op | uint32_t(n) << 5 | uint32_t(d)); }                             // two-register forms
    void ldr_d(int dt, int rn, uint32_t off) { e(0xFD400000 | (off >> 3) << 10 | uint32_t(rn) << 5 | uint32_t(dt)); }
    void str_d(int dt, int rn, uint32_t off) { e(0xFD000000 | (off >> 3) << 10 | uint32_t(rn) << 5 | uint32_t(dt)); }
    void ins_d(int vd, int i, int vn, int j) { e(0x6E000400 | uint32_t((i << 4) | 8) << 16 | uint32_t(j << 3) << 11 | uint32_t(vn) << 5 | uint32_t(vd)); }
    void fcmpd(int n, int m) { e(0x1E602000 | uint32_t(m) << 16 | uint32_t(n) << 5); }
    void vmov(int d, int n) { if (d != n) e(0x4EA01C00 | uint32_t(n) << 16 | uint32_t(n) << 5 | uint32_t(d)); }
    void fcsel_s(int d, int n, int m, int cond) { e(0x1E200C00 | uint32_t(m) << 16 | uint32_t(cond) << 12 | uint32_t(n) << 5 | uint32_t(d)); }
};

uint8_t g_parity[256];
bool is_jcc(uint16_t mn) {
    switch (mn) {
        case M_jo: case M_jno: case M_jb: case M_jae: case M_je: case M_jne: case M_jbe: case M_ja:
        case M_js: case M_jns: case M_jp: case M_jnp: case M_jl: case M_jge: case M_jle: case M_jg: return true;
        default: return false;
    }
}
enum : uint32_t {   // AArch64 opcodes used through Asm::v3 / v2
    FDIV4 = 0x6E20FC00, FSQRT4 = 0x6EA1F800, FCMGE4 = 0x6E20E400, FCMGT4 = 0x6EA0E400, FCMEQ4 = 0x4E20E400, NOT16 = 0x6E205800,
    BSL16 = 0x6E601C00, ZIP1_4S = 0x4E803800, ZIP2_4S = 0x4E807800, CMEQ4 = 0x6EA08C00, CMGT4 = 0x4EA03400, ADD4 = 0x4EA08400, SUB4 = 0x6EA08400,
    FCVTL2D = 0x0E617800, FCVTN2S = 0x0E616800, SCVTF4 = 0x4E21D800,
    FADD_S = 0x1E202800, FSUB_S = 0x1E203800, FMUL_S = 0x1E200800, FDIV_S = 0x1E201800, FSQRT_S = 0x1E21C000,
    FADD_D = 0x1E602800, FSUB_D = 0x1E603800, FMUL_D = 0x1E600800, FDIV_D = 0x1E601800,
    SCVTF_S_W = 0x1E220000, SCVTF_D_W = 0x1E620000, FCVTZS_W_S = 0x1E380000, FCVTZS_W_D = 0x1E780000, FCVTNS_W_S = 0x1E200000, FCVTNS_W_D = 0x1E600000,
    FCVT_D_S = 0x1E22C000, FCVT_S_D = 0x1E624000,
    FMOV_DD = 0x1E604000, INS_D0_X = 0x4E081C00, INS_D1_X = 0x4E181C00, FCVTZS4 = 0x4EA1B800, FCVTNS4 = 0x4E21A800,
    MVNI_7FFFFFFF = 0x6F046400, UMAXV4 = 0x6EB0A800,
};
bool is_transfer(uint16_t mn) { return is_jcc(mn) || mn == M_jmp || mn == M_call || mn == M_ret || mn == M_jecxz; }
}  // namespace

Jit::Jit(Machine& m) : m_(m) {
    for (int i = 0; i < 256; i++) g_parity[i] = (__builtin_popcount(unsigned(i)) & 1) ? 0 : 4;   // x86 PF: even number of set bits
    cap_ = 64u << 20;
    void* p = mmap(nullptr, cap_, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANON | MAP_JIT, -1, 0);
    if (p == MAP_FAILED) { code_ = nullptr; return; }
    code_ = static_cast<uint8_t*>(p);
}
Jit::~Jit() { if (code_) munmap(code_, cap_); }
void Jit::flush() {
    map_.clear(); heat_.clear(); refused_.clear(); body_.clear(); waiting_.clear(); used_ = 0;
    for (auto& block : slots_) std::memset(block.get(), 0, 4096 * sizeof(uint64_t));
    slot_used_ = 0;
    if (cache_) for (size_t i = 0; i < 65536; i++) cache_[i] = {0, 0, nullptr, 0, 0};
}
void Jit::init_cache() { cache_.reset(new CacheEntry[65536]); for (size_t i = 0; i < 65536; i++) cache_[i] = {0, 0, nullptr, 0, 0}; }
uint64_t* Jit::new_slot() {
    size_t blk = slot_used_ / 4096, idx = slot_used_ % 4096;
    if (blk >= slots_.size()) { slots_.emplace_back(new uint64_t[4096]); std::memset(slots_.back().get(), 0, 4096 * sizeof(uint64_t)); }
    slot_used_++;
    return &slots_[blk][idx];
}

Jit::BlockFn Jit::lookup(uint32_t pc) {
    if (!cache_) init_cache();
    CacheEntry& ce = cache_[(pc ^ (pc >> 16)) & 0xffff];
    if (ce.fn && ce.pc == pc) return ce.fn;
    auto it = map_.find(pc);
    if (it != map_.end()) { ce = {pc, 0, it->second, body_[pc], 0}; return it->second; }
    if (refused_.count(pc)) return nullptr;
    if (++heat_[pc] < 2) return nullptr;
    flushed_ = false;
    BlockFn f = translate(pc);
    if (!f) { if (!flushed_) refused_[pc] = 1; return nullptr; }
    map_[pc] = f; ce = {pc, 0, f, body_[pc], 0}; return f;
}

Jit::BlockFn Jit::translate(uint32_t start) {
    Machine& m = m_;
    if (!cache_) init_cache();
    m.jit_pages = m.mem.page_table(); m.jit_wpages = m.mem.write_table(); m.jit_parity = g_parity;
    auto off = [&](const void* field) { return uint32_t(static_cast<const char*>(field) - reinterpret_cast<const char*>(&m)); };
    const uint32_t R0 = off(&m.r[0]), FLAGS = off(&m.flags), XMM = off(&m.xmm[0]), STEPS = off(&m.steps), MXCSR = off(&m.mxcsr);
    const uint32_t SLICE_END = off(&m.slice_end), PAGES = off(&m.jit_pages), WPAGES = off(&m.jit_wpages), PARITY = off(&m.jit_parity);
    const uint32_t STOP_PC = off(&m.stop_pc);
    if ((STOP_PC & 3) || STOP_PC > 16000) return nullptr;
    if (R0 > 4000 || FLAGS > 4000 || (STEPS & 7) || STEPS > 32000 || (PAGES & 7) || PAGES > 32000 || (WPAGES & 7) || (PARITY & 7) || XMM > 4000 || (SLICE_END & 7) || SLICE_END > 32000) return nullptr;
    // collect the block: up to 48 instructions, ending at a control transfer or before a native / import address
    std::vector<const Insn*> list;
    // Traces: direct jmp / call continue at their target and conditional branches at their fall-through (the taken
    // path leaves through a side exit), so registers stay cached across them; ret and indirect transfers end the block.
    static const bool traces = std::getenv("REFLEX_JIT_TRACES") != nullptr;   // measured: exact, but no faster (2026-10-14); opt-in
    for (uint32_t pc = start; list.size() < 48;) {
        if (m.natives.count(pc) || m.import_names.count(pc)) break;
        const Insn* ip;
        try { ip = &m.decode(pc); } catch (...) { break; }
        list.push_back(ip);
        uint16_t t = ip->mn;
        bool imm = ip->n && ip->o[0].type == 2;
        if (traces && imm && (t == M_jmp || t == M_call)) { pc = ip->o[0].imm; continue; }
        if (traces && imm && is_jcc(t)) { pc = ip->next; continue; }
        if (is_transfer(t)) break;
        pc = ip->next;
    }
    if (list.empty()) return nullptr;
    // Two passes: the first finds which instructions are translated (the rest run through the interpreter's handler),
    // from which the x86 flags still read later in the block are known; the second emits code that skips flags no one reads.
    std::vector<uint8_t> okv(list.size(), 1);
    std::vector<uint32_t> live_after(list.size(), 0x8d5u);
    for (int pass = 0; pass < 2; pass++) {
    Asm a;
    Label epilogue, fault;
    struct Exit { Label l; int done; bool reg; uint32_t target; };
    std::vector<std::unique_ptr<Exit>> exits;
    // exit after `done` instructions to an immediate target, or to the target in w13 (reg)
    auto exit_imm = [&](int done, uint32_t target) { auto x = std::make_unique<Exit>(); x->done = done; x->reg = false; x->target = target; Label& l = x->l; exits.push_back(std::move(x)); return &l; };
    std::vector<std::pair<std::unique_ptr<Label>, int>> faults;   // (label, done)
    auto fault_after = [&](int done) { faults.push_back({std::make_unique<Label>(), done}); return faults.back().first.get(); };
    // prologue
    a.stp_pre(29, 30, SP, -96); a.mov_sp(29); a.stp(19, 20, SP, 16); a.stp(21, 22, SP, 32); a.stp(23, 24, SP, 48); a.stp(25, 26, SP, 64); a.stp(27, 28, SP, 80);
    a.movx(19, 0);
    auto ldrx_m = [&](int rt, uint32_t o) { if (o <= 32760) a.ldr_u(3, rt, 19, o); else { a.mov32(16, o); a.ldr_r(3, rt, 19, 16); } };
    ldrx_m(20, PAGES); ldrx_m(21, WPAGES); ldrx_m(23, PARITY);
    a.addi(22, 19, XMM, true);
    // MXCSR must be round-to-nearest without FTZ/DAZ (as Fast::arith_ps / ps_fast): else decline (bit 33).
    // Chained entries from other blocks start here (x19..x23 are already set up, the frame is the first block's).
    int body_at = a.here();
    Label mx_ok;
    a.ldr_u(2, 9, 19, MXCSR & ~3u); a.mov32(10, 0xe040); a.ands(31, 9, 10); a.bcond(EQ, mx_ok);
    a.mov64(0, (2ull << 32) | start); a.b(epilogue);
    a.bind(mx_ok);
    if (m.jit_exact_slices) {   // the whole block must end before slice_end (else the interpreter steps up to it)
        Label in_slice;
        ldrx_m(9, STEPS); a.addi(9, 9, uint32_t(list.size()), true); ldrx_m(10, SLICE_END); a.cmpx(9, 10); a.bcond(LS, in_slice);
        a.mov64(0, (2ull << 32) | start); a.b(epilogue);
        a.bind(in_slice);
    }
    // Register cache within the block. Bits 0..7 of the masks: guest xmm k in host v(16 + k); bits 8..15: guest r[k] in host
    // HR[k] (x3..x8 are clobbered by calls into C++, x27/x28 are saved in the prologue). xv = cached (valid), xd = cached and
    // newer than the Machine. Calls into C++ (helpers, slow paths) see the Machine: dirty registers are written back first and
    // the cached ones reloaded after (or forgotten after a handler, which may change any register).
    static const int HR[8] = {3, 4, 5, 6, 7, 8, 27, 28};
    uint32_t xv = 0, xd = 0;
    auto spill = [&a, R0](uint32_t mask) {
        for (int k = 0; k < 8; k++) if ((mask >> k) & 1) a.str_q(16 + k, 22, 16u * uint32_t(k));
        for (int k = 0; k < 8; k++) if ((mask >> (8 + k)) & 1) a.str_u(2, HR[k], 19, R0 + 4u * uint32_t(k));
    };
    auto fill = [&a, R0](uint32_t mask) {
        for (int k = 0; k < 8; k++) if ((mask >> k) & 1) a.ldr_q(16 + k, 22, 16u * uint32_t(k));
        for (int k = 0; k < 8; k++) if ((mask >> (8 + k)) & 1) a.ldr_u(2, HR[k], 19, R0 + 4u * uint32_t(k));
    };
    auto G_ = [&](int k) { if (!((xv >> (8 + k)) & 1)) { a.ldr_u(2, HR[k], 19, R0 + 4u * uint32_t(k)); xv |= 1u << (8 + k); } return HR[k]; };
    auto ldrw_r = [&](int wt, int reg) { a.orr(wt, 31, G_(reg)); };
    auto strw_r = [&](int wt, int reg) { a.orr(HR[reg], 31, wt); xv |= 1u << (8 + reg); xd |= 1u << (8 + reg); };
    // effective address of a memory operand -> w9 (clobbers w10, w11)
    auto ea = [&](const Operand& o) {
        a.mov32(9, uint32_t(o.disp));
        if (o.fs) { a.mov32(10, 0x7ffd0000u); a.add(9, 9, 10); }
        if (o.base >= 0) { ldrw_r(10, o.base); a.add(9, 9, 10); }
        if (o.index >= 0) { ldrw_r(11, o.index); int s = o.scale == 8 ? 3 : o.scale == 4 ? 2 : o.scale == 2 ? 1 : 0; a.add(9, 9, 11, s); }
    };
    std::vector<std::function<void()>> outlines;   // out-of-line slow paths (emitted after the body)
    // Guest xmm k is cached in host v(16 + k) within the block: xv = cached (valid) registers, xd = cached and newer
    // than m.xmm. Calls into C++ clobber v16..v23: dirty ones are written back first, cached ones reloaded after.
    auto X_ = [&](int k) { if (!((xv >> k) & 1)) { a.ldr_q(16 + k, 22, 16u * uint32_t(k)); xv |= 1u << k; } return 16 + k; };
    auto Xdef = [&](int k) { xv |= 1u << k; xd |= 1u << k; return 16 + k; };
    auto Xmod = [&](int k) { X_(k); xd |= 1u << k; return 16 + k; };
    auto to_exit = [&]() { spill(xd); };   // before leaving the block
    // load n (1,2,4,8) bytes at w9 into w13/x13; done = instructions completed before this one
    auto load = [&](int n, int done) {
        int size = n == 1 ? 0 : n == 2 ? 1 : n == 4 ? 2 : 3;
        auto slow = std::make_shared<Label>(), back = std::make_shared<Label>();
        a.lsr_imm(10, 9, 12); a.ldr_r(3, 11, 20, 10, true); a.cbz(11, *slow, true);
        a.and_fff(12, 9); a.cmpi(12, uint32_t(4096 - n)); a.bcond(HI, *slow);
        a.ldr_r(size, 13, 11, 12);
        a.bind(*back);
        Label* f = fault_after(done);
        uint32_t cv = xv, cd = xd;
        outlines.push_back([&a, &spill, &fill, cv, cd, slow, back, n, f, size]() {
            a.bind(*slow);
            spill(cd);
            a.movx(25, 9);   // the address (pop / ret use w9 after the load) survives the call in x25
            a.movx(0, 19); a.orr(1, 31, 9); a.movz(2, uint32_t(n), 0, false);
            a.mov64(16, reinterpret_cast<uint64_t>(&JitHelpers::load)); a.blr(16);
            a.cbnz(1, *f, true);
            a.movx(9, 25);
            if (size == 3) a.movx(13, 0); else a.orr(13, 31, 0);
            fill(cv);
            a.b(*back);
        });
    };
    // store n bytes of w13/x13 at w9
    auto store = [&](int n, int done) {
        int size = n == 1 ? 0 : n == 2 ? 1 : n == 4 ? 2 : 3;
        auto slow = std::make_shared<Label>(), back = std::make_shared<Label>();
        a.lsr_imm(10, 9, 12); a.ldr_r(3, 11, 21, 10, true); a.cbz(11, *slow, true);
        a.and_fff(12, 9); a.cmpi(12, uint32_t(4096 - n)); a.bcond(HI, *slow);
        a.str_r(size, 13, 11, 12);
        a.bind(*back);
        Label* f = fault_after(done);
        uint32_t cv = xv, cd = xd;
        outlines.push_back([&a, &spill, &fill, cv, cd, slow, back, n, f]() {
            a.bind(*slow);
            spill(cd);
            a.movx(25, 9); a.movx(26, 13);
            a.movx(0, 19); a.orr(1, 31, 9); a.movx(2, 13); a.movz(3, uint32_t(n), 0, false);
            a.mov64(16, reinterpret_cast<uint64_t>(&JitHelpers::store)); a.blr(16);
            a.cbnz(0, *f, true);
            a.movx(9, 25); a.movx(13, 26);
            fill(cv);
            a.b(*back);
        });
    };
    // 16-byte load at w9 into q0 (vector register vt)
    auto load16 = [&](int vt, int done) {
        auto slow = std::make_shared<Label>(), back = std::make_shared<Label>();
        a.lsr_imm(10, 9, 12); a.ldr_r(3, 11, 20, 10, true); a.cbz(11, *slow, true);
        a.and_fff(12, 9); a.cmpi(12, 4096 - 16); a.bcond(HI, *slow);
        a.ldr_qr(vt, 11, 12);
        a.bind(*back);
        Label* f = fault_after(done);
        uint32_t cv = xv, cd = xd;   // vt is a temporary (v0..v7): the reload cannot overwrite it
        outlines.push_back([&a, &spill, &fill, cv, cd, slow, back, vt, f]() {
            a.bind(*slow);
            spill(cd);
            a.movx(25, 9);
            a.movx(0, 19); a.orr(1, 31, 9); a.movz(2, 8, 0, false);
            a.mov64(16, reinterpret_cast<uint64_t>(&JitHelpers::load)); a.blr(16);
            a.cbnz(1, *f, true); a.movx(24, 0);   // x24 / x25 are callee-saved: they survive the second call
            a.movx(0, 19); a.addi(1, 25, 8); a.movz(2, 8, 0, false);
            a.mov64(16, reinterpret_cast<uint64_t>(&JitHelpers::load)); a.blr(16);
            a.cbnz(1, *f, true);
            a.fmov_d_x(vt, 24); a.ins_d1_x(vt, 0); a.movx(9, 25);
            fill(cv);
            a.b(*back);
        });
    };
    // 16-byte store of vector register vt at w9 (low half first, as the interpreter)
    auto store16 = [&](int vt, int done) {
        auto slow = std::make_shared<Label>(), back = std::make_shared<Label>();
        a.lsr_imm(10, 9, 12); a.ldr_r(3, 11, 21, 10, true); a.cbz(11, *slow, true);
        a.and_fff(12, 9); a.cmpi(12, 4096 - 16); a.bcond(HI, *slow);
        a.str_qr(vt, 11, 12);
        a.bind(*back);
        Label* f = fault_after(done);
        uint32_t cv = xv, cd = xd;
        outlines.push_back([&a, &spill, &fill, cv, cd, slow, back, vt, f]() {
            a.bind(*slow);
            spill(cd);
            a.umov_x_d1(24, vt); a.movx(25, 9);
            a.movx(0, 19); a.orr(1, 31, 9); a.fmov_x_d(2, vt); a.movz(3, 8, 0, false);
            a.mov64(16, reinterpret_cast<uint64_t>(&JitHelpers::store)); a.blr(16);
            a.cbnz(0, *f, true);
            a.movx(0, 19); a.addi(1, 25, 8); a.movx(2, 24); a.movz(3, 8, 0, false);
            a.mov64(16, reinterpret_cast<uint64_t>(&JitHelpers::store)); a.blr(16);
            a.cbnz(0, *f, true);
            fill(cv);
            a.b(*back);
        });
    };
    // x86 flags after w12 = result of the ALU op whose ARM flags are live: kind 0 add, 1 sub, 2 logic (CF = OF = 0),
    // 3 inc, 4 dec (CF preserved). PF from the parity table, AF untouched (as Machine::set_logic / set_add).
    uint32_t cur_live = 0x8d5u;   // flags read after the current instruction
    auto flags_from = [&](int kind) {
        if (!(cur_live & (kind >= 3 ? 0x8c4u : 0x8c5u))) return;
        if (kind == 2) { a.cset(14, EQ); a.cset(15, MI); }
        else { a.cset(13, kind == 1 || kind == 4 ? LO : HS); a.cset(14, EQ); a.cset(15, MI); a.cset(16, VS); }
        a.and_ff(17, 12); a.ldr_r(0, 17, 23, 17);
        a.ldr_u(2, 10, 19, FLAGS);
        a.mov32(11, kind >= 3 ? ~0x8c4u : ~0x8c5u); a.and_(10, 10, 11);
        if (kind < 2) a.orr(10, 10, 13);
        a.orr(10, 10, 14, 6); a.orr(10, 10, 15, 7);
        if (kind != 2) a.orr(10, 10, 16, 11);
        a.orr(10, 10, 17);
        a.str_u(2, 10, 19, FLAGS);
    };
    // the generic path: call the interpreter's handler, end the block if it did not fall through
    auto helper_raw = [&](const Insn& in, int done, uint64_t (*fn)(Machine*, const Insn*, uint32_t)) {
        a.movx(0, 19); a.mov64(1, reinterpret_cast<uint64_t>(&in));
        if (fn) a.orr(2, 31, 9);   // the operand address (memory forms)
        a.mov64(16, fn ? reinterpret_cast<uint64_t>(fn) : reinterpret_cast<uint64_t>(&JitHelpers::exec)); a.blr(16);
        a.lsr_imm(9, 0, 32, true); a.cbnz(9, *fault_after(done), true);
        a.mov32(9, in.next); a.cmp(0, 9);
        auto x = std::make_unique<Exit>(); x->done = done + 1; x->reg = true; x->target = 0;
        Label* l = &x->l; exits.push_back(std::move(x));
        a.orr(13, 31, 0); a.bcond(NE, *l);
    };
    // inline: the handler sees and may change any xmm register
    auto helper = [&](const Insn& in, int done, uint64_t (*fn)(Machine*, const Insn*, uint32_t) = nullptr) {
        spill(xd); xd = 0;
        helper_raw(in, done, fn);
        xv = 0;
    };
    // out-of-line interpreter fallback of a translated instruction: begin_slow before its fast path may branch to sp.slow,
    // end_slow after the fast path's result is in place
    struct Slow { std::shared_ptr<Label> slow, back; uint32_t cd; };
    auto begin_slow = [&]() { return Slow{std::make_shared<Label>(), std::make_shared<Label>(), xd}; };
    auto end_slow = [&](const Slow& sp, const Insn* ip, int d) {
        a.bind(*sp.back);
        uint32_t cv = xv, cd = sp.cd;
        outlines.push_back([&, sp, cv, cd, ip, d]() { a.bind(*sp.slow); spill(cd); helper_raw(*ip, d, nullptr); fill(cv); a.b(*sp.back); });
    };
    auto R = [](const Operand& o) { return o.type == 1 && o.rclass == 1 && o.size == 4; };
    auto I = [](const Operand& o) { return o.type == 2; };
    auto Mm = [](const Operand& o, int size) { return o.type == 3 && o.size == size; };
    auto X = [](const Operand& o) { return o.type == 1 && o.rclass == 5; };
    auto G = [](const Operand& o) { return o.type == 1 && o.rclass >= 1 && o.rclass <= 4; };   // gpr 32 / 16 / 8 low / 8 high
    auto Mem = [](const Operand& o) { return o.type == 3; };
    auto Src = [&](const Operand& o, int n) { return (G(o) && o.size == n) || I(o) || (Mem(o) && o.size == n); };
    auto gld = [&](int w, const Operand& o) {   // gpr 32 / 16 / 8 low / 8 high, zero-extended
        if (o.rclass == 1) { ldrw_r(w, o.ridx); return; }
        int h = G_(o.ridx);
        if (o.rclass == 2) a.and_ffff(w, h); else if (o.rclass == 3) a.and_ff(w, h); else a.e(0x53083C00 | uint32_t(h) << 5 | uint32_t(w));   // ubfx w, h, #8, #8
    };
    auto gst = [&](int w, const Operand& o) {   // the low bits of w into the register part
        if (o.rclass == 1) { strw_r(w, o.ridx); return; }
        int h = G_(o.ridx);
        a.e((o.rclass == 2 ? 0x33003C00u : o.rclass == 3 ? 0x33001C00u : 0x33181C00u) | uint32_t(w) << 5 | uint32_t(h));   // bfi
        xd |= 1u << (8 + o.ridx);
    };
    // every guest register an operand reads (before code that runs on one path only: both paths must leave the same cache)
    auto touch = [&](const Operand& o) { if (G(o)) G_(o.ridx); if (o.type == 3) { if (o.base >= 0) G_(o.base); if (o.index >= 0) G_(o.index); } };
    // operand value (n bytes, zero-extended) -> w13 / x13; memory operands leave their address in w9
    std::function<void(const Operand&, int, int)> rd = [&](const Operand& op, int n, int done) {
        if (op.type == 3) { ea(op); load(n, done); }
        else if (op.type == 2) a.mov32(13, n == 4 ? op.imm : op.imm & ((1u << (8 * n)) - 1));
        else gld(13, op);
    };
    // x86 flags after a result in w12 with AArch64 Z / N live for it, CF in w13 and OF in w16 (0 / 1)
    auto flags_cf_of = [&]() {
        if (!(cur_live & 0x8c5u)) return;
        a.cset(14, EQ); a.cset(15, MI);
        a.and_ff(17, 12); a.ldr_r(0, 17, 23, 17);
        a.ldr_u(2, 10, 19, FLAGS); a.mov32(11, ~0x8c5u); a.and_(10, 10, 11);
        a.orr(10, 10, 13); a.orr(10, 10, 14, 6); a.orr(10, 10, 15, 7); a.orr(10, 10, 16, 11); a.orr(10, 10, 17);
        a.str_u(2, 10, 19, FLAGS);
    };
    // w11 = the condition of jcc `j` before negation (0 / 1); returns whether the condition is its negation
    auto cond_w11 = [&](uint16_t j) {
        a.ldr_u(2, 10, 19, FLAGS);
        auto bit = [&](int rd_, int b) { a.lsr_imm(rd_, 10, b); a.and_1(rd_, rd_); };
        switch (j) {   // CF bit 0, PF 2, ZF 6, SF 7, OF 11
            case M_jo: case M_jno: bit(11, 11); break;
            case M_jb: case M_jae: bit(11, 0); break;
            case M_je: case M_jne: bit(11, 6); break;
            case M_jbe: case M_ja: bit(11, 0); bit(12, 6); a.orr(11, 11, 12); break;
            case M_js: case M_jns: bit(11, 7); break;
            case M_jp: case M_jnp: bit(11, 2); break;
            case M_jl: case M_jge: bit(11, 7); bit(12, 11); a.eor(11, 11, 12); break;
            default: bit(11, 7); bit(12, 11); a.eor(11, 11, 12); bit(12, 6); a.orr(11, 11, 12); break;   // jle / jg
        }
        return j == M_jno || j == M_jae || j == M_jne || j == M_ja || j == M_jns || j == M_jnp || j == M_jge || j == M_jg;
    };
    auto jcc_for = [](uint16_t mn) -> uint16_t {
        std::string nm = mnemonic_name(mn);
        nm = "j" + nm.substr(nm.rfind("set", 0) == 0 ? 3 : 4);
        for (uint16_t k : {M_jo, M_jno, M_jb, M_jae, M_je, M_jne, M_jbe, M_ja, M_js, M_jns, M_jp, M_jnp, M_jl, M_jge, M_jle, M_jg})
            if (nm == mnemonic_name(k)) return k;
        return M_jo;
    };
    bool ended = false;
    for (size_t k = 0; k < list.size() && !ended; k++) {
        const Insn& in = *list[k]; const Operand* o = in.o; int done = int(k);
        cur_live = pass ? live_after[k] : 0x8d5u;
        const bool last = k + 1 == list.size();
        uint16_t mn = in.mn; bool ok = true;
        static const std::vector<uint8_t> disabled = [] {   // debugging: REFLEX_JIT_DISABLE=mn,mn,... runs those through the interpreter
            std::vector<uint8_t> v(M_COUNT, 0);
            if (const char* e = std::getenv("REFLEX_JIT_DISABLE")) {
                std::string list = e, item;
                for (size_t i = 0; i <= list.size(); i++) {
                    if (i == list.size() || list[i] == ',') { for (int k = 0; k < M_COUNT; k++) if (item == mnemonic_name(k)) v[size_t(k)] = 1; item.clear(); }
                    else item += list[i];
                }
            }
            return v;
        }();
        // o[1] as a full xmm value / scalar float / scalar double -> host vector register (cached, or temporary vt)
        auto xsrc128 = [&](int vt) { if (o[1].type == 1) return X_(o[1].ridx); ea(o[1]); load16(vt, done); return vt; };
        auto xsrc_s = [&](int vt) { if (o[1].type == 1) return X_(o[1].ridx); ea(o[1]); load(4, done); a.fmov_s_w(vt, 13); return vt; };
        auto xsrc_d = [&](int vt) { if (o[1].type == 1) return X_(o[1].ridx); ea(o[1]); load(8, done); a.fmov_d_x(vt, 13); return vt; };
        if (disabled[mn]) ok = false;
        else switch (mn) {
        // ---- integer moves (gpr 8/16/32, memory, immediate operands)
        case M_mov: {
            int n = o[0].size;
            if (in.n != 2 || !(n == 1 || n == 2 || n == 4) || !(G(o[0]) || Mem(o[0])) || !Src(o[1], n) || (Mem(o[0]) && Mem(o[1]))) { ok = false; break; }
            rd(o[1], n, done);
            if (Mem(o[0])) { ea(o[0]); store(n, done); } else gst(13, o[0]);
            break; }
        case M_lea: if (R(o[0]) && o[1].type == 3) { ea(o[1]); strw_r(9, o[0].ridx); } else ok = false; break;
        case M_movzx: case M_movsx: {
            int n = o[1].size;
            if (!(G(o[0]) && (o[0].size == 4 || o[0].size == 2)) || !(n == 1 || n == 2) || !(G(o[1]) || Mem(o[1]))) { ok = false; break; }
            rd(o[1], n, done);
            if (mn == M_movsx) { if (n == 1) a.sxtb(13, 13); else a.sxth(13, 13); }
            gst(13, o[0]);
            break; }
        case M_push:
            if (!Src(o[0], 4) || (G(o[0]) && o[0].size != 4) || (Mem(o[0]) && o[0].size != 4)) { ok = false; break; }
            if (I(o[0])) a.mov32(13, o[0].imm); else rd(o[0], 4, done);
            ldrw_r(9, 4); a.subi(9, 9, 4); strw_r(9, 4); store(4, done);
            break;
        case M_pop:
            if (R(o[0])) { ldrw_r(9, 4); load(4, done); a.addi(9, 9, 4); strw_r(9, 4); strw_r(13, o[0].ridx); }
            else ok = false;
            break;
        case M_leave:   // esp = ebp; ebp = pop
            ldrw_r(9, 5); strw_r(9, 4); load(4, done); a.addi(9, 9, 4); strw_r(9, 4); strw_r(13, 5);
            break;
        case M_prefetcht0: case M_prefetchnta: case M_prefetcht1: case M_prefetcht2: case M_nop: break;
        case M_call:
            if (I(o[0])) {
                a.mov32(13, in.next); ldrw_r(9, 4); a.subi(9, 9, 4); strw_r(9, 4); store(4, done);
                if (last) { to_exit(); a.b(*exit_imm(done + 1, o[0].imm)); ended = true; }   // else the trace continues at the target
            }
            else if ((R(o[0]) || Mem(o[0])) && o[0].size == 4) {
                rd(o[0], 4, done); a.movx(24, 13);
                a.mov32(13, in.next); ldrw_r(9, 4); a.subi(9, 9, 4); strw_r(9, 4); store(4, done);
                a.orr(13, 31, 24);
                auto x = std::make_unique<Exit>(); x->done = done + 1; x->reg = true; Label* l = &x->l; exits.push_back(std::move(x));
                to_exit(); a.b(*l); ended = true;
            }
            else ok = false;
            break;
        case M_ret: {
            ldrw_r(9, 4); load(4, done);
            uint32_t pop = 4 + (in.n ? o[0].imm : 0);
            if (pop < 4096) a.addi(9, 9, pop); else { a.mov32(10, pop); a.add(9, 9, 10); }
            strw_r(9, 4);
            auto x = std::make_unique<Exit>(); x->done = done + 1; x->reg = true; Label* l = &x->l; exits.push_back(std::move(x));
            to_exit(); a.b(*l); ended = true;
            break; }
        case M_jmp:
            if (I(o[0])) { if (last) { to_exit(); a.b(*exit_imm(done + 1, o[0].imm)); ended = true; } }   // else: the trace continues
            else if ((R(o[0]) || Mem(o[0])) && o[0].size == 4) {
                rd(o[0], 4, done);
                auto x = std::make_unique<Exit>(); x->done = done + 1; x->reg = true; Label* l = &x->l; exits.push_back(std::move(x));
                to_exit(); a.b(*l); ended = true;
            }
            else ok = false;
            break;
        case M_jo: case M_jno: case M_jb: case M_jae: case M_je: case M_jne: case M_jbe: case M_ja:
        case M_js: case M_jns: case M_jp: case M_jnp: case M_jl: case M_jge: case M_jle: case M_jg: {
            if (!I(o[0])) { ok = false; break; }
            bool negate = cond_w11(mn);
            if (!last) {   // trace: the taken path leaves through a side exit (registers written back there), fall-through continues
                auto side = std::make_shared<Label>(); Label* taken = exit_imm(done + 1, o[0].imm); uint32_t cd = xd;
                if (negate) a.cbz(11, *side); else a.cbnz(11, *side);
                outlines.push_back([&a, &spill, side, cd, taken]() { a.bind(*side); spill(cd); a.b(*taken); });
                break;
            }
            Label* taken = exit_imm(done + 1, o[0].imm); Label* fall = exit_imm(done + 1, in.next);
            to_exit();
            if (negate) a.cbz(11, *taken); else a.cbnz(11, *taken);
            a.b(*fall); ended = true;
            break; }
        case M_seto: case M_setno: case M_setb: case M_setae: case M_sete: case M_setne: case M_setbe: case M_seta:
        case M_sets: case M_setns: case M_setp: case M_setnp: case M_setl: case M_setge: case M_setle: case M_setg: {
            if (!((G(o[0]) || Mem(o[0])) && o[0].size == 1)) { ok = false; break; }
            if (cond_w11(jcc_for(mn))) a.eor_1(11, 11);
            a.orr(13, 31, 11);
            if (Mem(o[0])) { ea(o[0]); store(1, done); } else gst(13, o[0]);
            break; }
        case M_cmovo: case M_cmovno: case M_cmovb: case M_cmovae: case M_cmove: case M_cmovne: case M_cmovbe: case M_cmova:
        case M_cmovs: case M_cmovns: case M_cmovp: case M_cmovnp: case M_cmovl: case M_cmovge: case M_cmovle: case M_cmovg: {
            if (!(R(o[0]) && (R(o[1]) || (Mem(o[1]) && o[1].size == 4)))) { ok = false; break; }
            touch(o[0]); touch(o[1]); xd |= 1u << (8 + o[0].ridx);   // the cache is the same after either path
            Label skip;
            if (cond_w11(jcc_for(mn))) a.cbnz(11, skip); else a.cbz(11, skip);
            rd(o[1], 4, done); gst(13, o[0]);
            a.bind(skip);
            break; }
        // ---- integer ALU: x86 flags from the AArch64 flags of the operation on operands shifted to the top of the word
        case M_add: case M_adc: case M_sub: case M_sbb: case M_cmp: case M_and: case M_or: case M_xor: case M_test: {
            int n = o[0].size;
            if (in.n != 2 || !(n == 1 || n == 2 || n == 4) || !(G(o[0]) || Mem(o[0])) || !Src(o[1], n) || (Mem(o[0]) && Mem(o[1]))) { ok = false; break; }
            if ((mn == M_adc || mn == M_sbb) && n != 4) { ok = false; break; }
            bool mem0 = Mem(o[0]);
            if (mem0) { ea(o[0]); load(n, done); a.movx(24, 9); } else gld(13, o[0]);
            a.orr(26, 31, 13);
            rd(o[1], n, done);
            int sh = 32 - 8 * n;
            bool logic = mn == M_and || mn == M_or || mn == M_xor || mn == M_test;
            if (!logic) {
                bool sub = mn == M_sub || mn == M_cmp || mn == M_sbb;
                if (mn == M_adc || mn == M_sbb) {
                    a.ldr_u(2, 10, 19, FLAGS); a.and_1(10, 10);
                    if (mn == M_adc) { a.cmpi(10, 1); a.adcs(12, 26, 13); }      // C = CF
                    else { a.subs(31, 31, 10); a.sbcs(12, 26, 13); }              // C = !CF
                } else if (sh == 0) { if (sub) a.subs(12, 26, 13); else a.adds(12, 26, 13); }
                else {
                    a.lsl_imm(14, 26, sh); a.lsl_imm(15, 13, sh);
                    if (sub) { a.subs(31, 14, 15); a.sub_(12, 26, 13); } else { a.adds(31, 14, 15); a.add(12, 26, 13); }
                }
                flags_from(sub ? 1 : 0);
            } else {
                if (mn == M_or) a.orr(12, 26, 13); else if (mn == M_xor) a.eor(12, 26, 13, 0); else a.and_(12, 26, 13);
                if (sh) { a.lsl_imm(14, 12, sh); a.ands(31, 14, 14); } else a.ands(31, 12, 12);
                flags_from(2);
            }
            if (mn != M_cmp && mn != M_test) { if (mem0) { a.movx(9, 24); a.orr(13, 31, 12); store(n, done); } else gst(12, o[0]); }
            break; }
        case M_inc: case M_dec: case M_neg: case M_not: {
            int n = o[0].size;
            if (!(n == 1 || n == 2 || n == 4) || !(G(o[0]) || Mem(o[0]))) { ok = false; break; }
            bool mem0 = Mem(o[0]);
            if (mem0) { ea(o[0]); load(n, done); a.movx(24, 9); } else gld(13, o[0]);
            a.orr(26, 31, 13);
            int sh = 32 - 8 * n;
            if (mn == M_not) a.orn(12, 31, 26);
            else {
                a.lsl_imm(14, 26, sh);
                if (mn == M_neg) { a.subs(31, 31, 14); a.sub_(12, 31, 26); flags_from(1); }
                else {
                    a.mov32(15, 1u << sh);
                    if (mn == M_inc) { a.adds(31, 14, 15); a.addi(12, 26, 1); flags_from(3); }
                    else { a.subs(31, 14, 15); a.subi(12, 26, 1); flags_from(4); }
                }
            }
            if (mem0) { a.movx(9, 24); a.orr(13, 31, 12); store(n, done); } else gst(12, o[0]);
            break; }
        case M_shl: case M_sal: case M_shr: case M_sar: {
            int n = o[0].size, bits = 8 * n;
            unsigned c = in.n > 1 ? (I(o[1]) ? (o[1].imm & 0xff) & 31 : 99u) : 1u;
            if (!(n == 1 || n == 2 || n == 4) || !(G(o[0]) || Mem(o[0])) || c == 0 || c >= unsigned(bits)) { ok = false; break; }
            bool mem0 = Mem(o[0]);
            if (mem0) { ea(o[0]); load(n, done); a.movx(24, 9); } else gld(13, o[0]);
            a.orr(26, 31, 13);
            auto trim = [&](int rr) { if (n == 1) a.and_ff(rr, rr); else if (n == 2) a.and_ffff(rr, rr); };
            if (mn == M_shl || mn == M_sal) {
                a.lsl_imm(12, 26, int(c)); trim(12); a.ubfx1(13, 26, bits - int(c));
                if (c == 1) { a.ubfx1(16, 12, bits - 1); a.eor(16, 16, 13, 0); } else a.movz(16, 0, 0, false);
            } else if (mn == M_shr) {
                a.lsr_imm(12, 26, int(c)); a.ubfx1(13, 26, int(c) - 1);
                if (c == 1) a.ubfx1(16, 26, bits - 1); else a.movz(16, 0, 0, false);
            } else {
                if (n == 1) a.sxtb(14, 26); else if (n == 2) a.sxth(14, 26); else a.orr(14, 31, 26);
                a.asr_imm(12, 14, int(c)); trim(12); a.ubfx1(13, 14, int(c) - 1); a.movz(16, 0, 0, false);
            }
            a.lsl_imm(15, 12, 32 - bits); a.ands(31, 15, 15);
            flags_cf_of();
            if (mem0) { a.movx(9, 24); a.orr(13, 31, 12); store(n, done); } else gst(12, o[0]);
            break; }
        case M_imul: {
            if (o[0].size != 4 || !R(o[0])) { ok = false; break; }
            if (in.n == 3 && (R(o[1]) || (Mem(o[1]) && o[1].size == 4)) && I(o[2])) { rd(o[1], 4, done); a.orr(26, 31, 13); a.mov32(13, o[2].imm); }
            else if (in.n == 2 && (R(o[1]) || (Mem(o[1]) && o[1].size == 4))) { rd(o[1], 4, done); gld(26, o[0]); }
            else { ok = false; break; }
            a.smull(12, 26, 13);
            if (cur_live & 0x801u) {
                a.sxtw(14, 12); a.cmpx(12, 14); a.cset(13, NE);
                a.ldr_u(2, 10, 19, FLAGS); a.mov32(11, ~0x801u); a.and_(10, 10, 11); a.orr(10, 10, 13); a.orr(10, 10, 13, 11); a.str_u(2, 10, 19, FLAGS);
            }
            strw_r(12, o[0].ridx);
            break; }
        // ---- SSE moves (guest xmm k lives in host v(16 + k) while cached: X_ reads, Xdef defines, Xmod modifies)
        case M_movaps: case M_movups: case M_movdqa: case M_movdqu: case M_movapd: case M_movupd:
            if (X(o[0]) && X(o[1])) { int s_ = X_(o[1].ridx); a.vmov(Xdef(o[0].ridx), s_); }
            else if (X(o[0]) && Mem(o[1])) { ea(o[1]); load16(0, done); a.vmov(Xdef(o[0].ridx), 0); }
            else if (Mem(o[0]) && X(o[1])) { int s_ = X_(o[1].ridx); ea(o[0]); store16(s_, done); }
            else ok = false;
            break;
        case M_movss:
            if (X(o[0]) && Mem(o[1])) { ea(o[1]); load(4, done); a.fmov_s_w(Xdef(o[0].ridx), 13); }   // high lanes zeroed
            else if (Mem(o[0]) && X(o[1])) { a.fmov_w_s(13, X_(o[1].ridx)); ea(o[0]); store(4, done); }
            else if (X(o[0]) && X(o[1])) { int s_ = X_(o[1].ridx); a.ins_s(Xmod(o[0].ridx), 0, s_, 0); }
            else ok = false;
            break;
        case M_movsd: case M_movq:   // movq / movsd xmm, m64 zero the high half; movq xmm, xmm too (movsd xmm, xmm keeps it)
            if (mn == M_movq && o[0].type == 1 && o[0].rclass == 7 && Mem(o[1]) && o[1].size == 8) { ea(o[1]); helper(in, done, &JitHelpers::mmx_ops); }
            else if (X(o[0]) && Mem(o[1])) { ea(o[1]); load(8, done); a.fmov_d_x(Xdef(o[0].ridx), 13); }
            else if (Mem(o[0]) && X(o[1])) { a.fmov_x_d(13, X_(o[1].ridx)); ea(o[0]); store(8, done); }
            else if (X(o[0]) && X(o[1])) { int s_ = X_(o[1].ridx); if (mn == M_movq) a.v2(FMOV_DD, Xdef(o[0].ridx), s_); else a.ins_d(Xmod(o[0].ridx), 0, s_, 0); }
            else ok = false;
            break;
        case M_movd:
            if (X(o[0]) && (R(o[1]) || (Mem(o[1]) && o[1].size == 4))) { rd(o[1], 4, done); a.fmov_s_w(Xdef(o[0].ridx), 13); }
            else if (R(o[0]) && X(o[1])) { a.fmov_w_s(13, X_(o[1].ridx)); strw_r(13, o[0].ridx); }
            else if (Mem(o[0]) && X(o[1])) { a.fmov_w_s(13, X_(o[1].ridx)); ea(o[0]); store(4, done); }
            else ok = false;
            break;
        case M_movlps: case M_movlpd: case M_movhps: case M_movhpd: {
            bool hi = mn == M_movhps || mn == M_movhpd;
            if (X(o[0]) && Mem(o[1])) { ea(o[1]); load(8, done); a.v2(hi ? INS_D1_X : INS_D0_X, Xmod(o[0].ridx), 13); }
            else if (Mem(o[0]) && X(o[1])) { int s_ = X_(o[1].ridx); if (hi) a.umov_x_d1(13, s_); else a.fmov_x_d(13, s_); ea(o[0]); store(8, done); }
            else ok = false;
            break; }
        case M_movlhps: case M_movhlps:
            if (!(X(o[0]) && X(o[1]))) { ok = false; break; }
            { int s_ = X_(o[1].ridx), d_ = Xmod(o[0].ridx); if (mn == M_movlhps) a.ins_d(d_, 1, s_, 0); else a.ins_d(d_, 0, s_, 1); }
            break;
        // ---- SSE arithmetic: host results unless a result lane is NaN (a NaN input always gives one; x86 NaN rules
        // differ from the host's), then the interpreter's handler
        case M_mulps: case M_addps: case M_subps: case M_divps: {
            if (!X(o[0]) || !(X(o[1]) || Mem(o[1]))) { ok = false; break; }
            int b_ = xsrc128(1), d_ = X_(o[0].ridx);
            if (mn == M_mulps) a.fmul4(2, d_, b_); else if (mn == M_addps) a.fadd4(2, d_, b_); else if (mn == M_subps) a.fsub4(2, d_, b_); else a.v3(FDIV4, 2, d_, b_);
            Slow sp = begin_slow();
            a.v3(FCMEQ4, 3, 2, 2); a.uminv4(3, 3); a.fmov_w_s(9, 3); a.cbz(9, *sp.slow);
            a.vmov(Xmod(o[0].ridx), 2);
            end_slow(sp, &in, done);
            break; }
        case M_mulss: case M_addss: case M_subss: case M_divss: case M_mulsd: case M_addsd: case M_subsd: case M_divsd: {
            if (!X(o[0]) || !(X(o[1]) || Mem(o[1]))) { ok = false; break; }
            bool dbl = mn == M_mulsd || mn == M_addsd || mn == M_subsd || mn == M_divsd;
            int b_ = dbl ? xsrc_d(1) : xsrc_s(1), d_ = X_(o[0].ridx);
            uint32_t op = dbl ? (mn == M_mulsd ? FMUL_D : mn == M_addsd ? FADD_D : mn == M_subsd ? FSUB_D : FDIV_D)
                              : (mn == M_mulss ? FMUL_S : mn == M_addss ? FADD_S : mn == M_subss ? FSUB_S : FDIV_S);
            a.v3(op, 2, d_, b_);
            Slow sp = begin_slow();
            if (dbl) a.fcmpd(2, 2); else a.fcmps(2, 2);
            a.bcond(VS, *sp.slow);
            if (dbl) a.ins_d(Xmod(o[0].ridx), 0, 2, 0); else a.ins_s(Xmod(o[0].ridx), 0, 2, 0);
            end_slow(sp, &in, done);
            break; }
        case M_maxps: case M_minps: {   // x > y ? a : b  /  x < y ? a : b
            if (!X(o[0]) || !(X(o[1]) || Mem(o[1]))) { ok = false; break; }
            int b_ = xsrc128(1), d_ = X_(o[0].ridx);
            Slow sp = begin_slow();
            a.v3(FCMEQ4, 3, d_, d_); a.v3(FCMEQ4, 4, b_, b_); a.vand(3, 3, 4); a.uminv4(3, 3); a.fmov_w_s(9, 3); a.cbz(9, *sp.slow);
            if (mn == M_maxps) a.v3(FCMGT4, 3, d_, b_); else a.v3(FCMGT4, 3, b_, d_);
            a.v3(BSL16, 3, d_, b_);
            a.vmov(Xmod(o[0].ridx), 3);
            end_slow(sp, &in, done);
            break; }
        case M_maxss: case M_minss: {
            if (!X(o[0]) || !(X(o[1]) || Mem(o[1]))) { ok = false; break; }
            int b_ = xsrc_s(1), d_ = X_(o[0].ridx);
            Slow sp = begin_slow();
            a.fcmps(d_, d_); a.bcond(VS, *sp.slow); a.fcmps(b_, b_); a.bcond(VS, *sp.slow);
            a.fcmps(d_, b_); a.fcsel_s(2, d_, b_, mn == M_maxss ? GT : MI);
            a.ins_s(Xmod(o[0].ridx), 0, 2, 0);
            end_slow(sp, &in, done);
            break; }
        case M_sqrtps: case M_sqrtss: {
            if (!X(o[0]) || !(X(o[1]) || Mem(o[1]))) { ok = false; break; }
            bool ps = mn == M_sqrtps;
            int b_ = ps ? xsrc128(1) : xsrc_s(1);
            if (!ps) X_(o[0].ridx);
            a.v2(ps ? FSQRT4 : FSQRT_S, 2, b_);
            Slow sp = begin_slow();
            if (ps) { a.v3(FCMEQ4, 3, 2, 2); a.uminv4(3, 3); a.fmov_w_s(9, 3); a.cbz(9, *sp.slow); a.vmov(Xdef(o[0].ridx), 2); }
            else { a.fcmps(2, 2); a.bcond(VS, *sp.slow); a.ins_s(Xmod(o[0].ridx), 0, 2, 0); }
            end_slow(sp, &in, done);
            break; }
        case M_comiss: case M_ucomiss: case M_comisd: case M_ucomisd: {
            if (!X(o[0]) || !(X(o[1]) || Mem(o[1]))) { ok = false; break; }
            bool dbl = mn == M_comisd || mn == M_ucomisd;
            // unordered: ZF PF CF; less: CF; equal: ZF (OF SF AF cleared)
            if (!(cur_live & 0x8d5u)) break;
            int b_ = dbl ? xsrc_d(1) : xsrc_s(1), d_ = X_(o[0].ridx);
            if (dbl) a.fcmpd(d_, b_); else a.fcmps(d_, b_);
            a.cset(13, LT); a.cset(14, EQ); a.cset(15, VS); a.orr(14, 14, 15);
            a.ldr_u(2, 10, 19, FLAGS); a.mov32(11, ~0x8d5u); a.and_(10, 10, 11);
            a.orr(10, 10, 13); a.orr(10, 10, 14, 6); a.orr(10, 10, 15, 2);
            a.str_u(2, 10, 19, FLAGS);
            break; }
        case M_cmpeqps: case M_cmpltps: case M_cmpleps: case M_cmpunordps: case M_cmpneqps: case M_cmpnltps: case M_cmpnleps: case M_cmpordps:
        case M_cmpeqss: case M_cmpltss: case M_cmpless: case M_cmpunordss: case M_cmpneqss: case M_cmpnltss: case M_cmpnless: case M_cmpordss: {
            if (!X(o[0]) || !(X(o[1]) || Mem(o[1]))) { ok = false; break; }
            int idx = mn - M_cmpeqps, pred = idx % 8; bool packed = idx < 8;
            int b_ = packed ? xsrc128(1) : xsrc_s(1), d_ = X_(o[0].ridx);
            switch (pred) {
                case 0: case 4: a.v3(FCMEQ4, 2, d_, b_); break;
                case 1: case 5: a.v3(FCMGT4, 2, b_, d_); break;
                case 2: case 6: a.v3(FCMGE4, 2, b_, d_); break;
                default: a.v3(FCMEQ4, 3, d_, d_); a.v3(FCMEQ4, 4, b_, b_); a.vand(2, 3, 4); break;   // ordered
            }
            if (pred >= 3 && pred <= 6) a.v2(NOT16, 2, 2);
            if (packed) a.vmov(Xmod(o[0].ridx), 2); else a.ins_s(Xmod(o[0].ridx), 0, 2, 0);
            break; }
        case M_shufps: {
            if (!X(o[0]) || !(X(o[1]) || Mem(o[1]))) { ok = false; break; }
            int b_ = xsrc128(1), d_ = X_(o[0].ridx); unsigned imm = o[2].imm;
            a.ins_s(2, 0, d_, int(imm & 3)); a.ins_s(2, 1, d_, int((imm >> 2) & 3)); a.ins_s(2, 2, b_, int((imm >> 4) & 3)); a.ins_s(2, 3, b_, int((imm >> 6) & 3));
            a.vmov(Xmod(o[0].ridx), 2);
            break; }
        case M_pshufd: {
            if (!X(o[0]) || !(X(o[1]) || Mem(o[1]))) { ok = false; break; }
            int b_ = xsrc128(1); unsigned imm = o[2].imm;
            for (int k2 = 0; k2 < 4; k2++) a.ins_s(2, k2, b_, int((imm >> (2 * k2)) & 3));
            a.vmov(Xdef(o[0].ridx), 2);
            break; }
        case M_unpcklps: case M_unpckhps: case M_pcmpeqd: case M_pcmpgtd: case M_paddd: case M_psubd:
        case M_unpcklpd: case M_punpcklqdq: case M_unpckhpd: case M_punpckhqdq: case M_punpckldq: {
            if (!X(o[0]) || !(X(o[1]) || Mem(o[1]))) { ok = false; break; }
            int b_ = xsrc128(1), d_ = X_(o[0].ridx);
            switch (mn) {
                case M_unpcklps: case M_punpckldq: a.v3(ZIP1_4S, 2, d_, b_); break;
                case M_unpckhps: a.v3(ZIP2_4S, 2, d_, b_); break;
                case M_pcmpeqd: a.v3(CMEQ4, 2, d_, b_); break;
                case M_pcmpgtd: a.v3(CMGT4, 2, d_, b_); break;
                case M_paddd: a.v3(ADD4, 2, d_, b_); break;
                case M_psubd: a.v3(SUB4, 2, d_, b_); break;
                case M_unpcklpd: case M_punpcklqdq: a.vmov(2, d_); a.ins_d(2, 1, b_, 0); break;
                default: a.vmov(2, d_); a.ins_d(2, 0, d_, 1); a.ins_d(2, 1, b_, 1); break;   // high halves
            }
            a.vmov(Xmod(o[0].ridx), 2);
            break; }
        case M_andps: case M_andnps: case M_orps: case M_xorps: case M_andpd: case M_andnpd: case M_orpd: case M_xorpd:
        case M_pand: case M_pandn: case M_por: case M_pxor: {
            if (!X(o[0]) || !(X(o[1]) || Mem(o[1]))) { ok = false; break; }
            int b_ = xsrc128(1), d_ = X_(o[0].ridx);
            bool andn = mn == M_andnps || mn == M_andnpd || mn == M_pandn;
            if (mn == M_andps || mn == M_andpd || mn == M_pand) a.vand(2, d_, b_); else if (mn == M_orps || mn == M_orpd || mn == M_por) a.vorr(2, d_, b_);
            else if (andn) a.vbic(2, b_, d_); else a.veor(2, d_, b_);
            a.vmov(Xmod(o[0].ridx), 2);
            break; }
        // ---- SSE conversions (the interpreter converts with the host's own instructions where these are used)
        case M_cvtps2pd: case M_cvtpd2ps: case M_cvtdq2ps: {
            if (!X(o[0]) || !(X(o[1]) || Mem(o[1]))) { ok = false; break; }
            int b_ = mn == M_cvtps2pd ? xsrc_d(1) : xsrc128(1);
            a.v2(mn == M_cvtps2pd ? FCVTL2D : mn == M_cvtpd2ps ? FCVTN2S : SCVTF4, 2, b_);   // fcvtn zeroes the high half
            a.vmov(Xdef(o[0].ridx), 2);
            break; }
        case M_cvtps2dq: case M_cvttps2dq: {   // NaN / out-of-range lanes (x86: 0x80000000; the host saturates): interpreter
            if (!X(o[0]) || !(X(o[1]) || Mem(o[1]))) { ok = false; break; }
            int b_ = xsrc128(1);
            a.v2(mn == M_cvttps2dq ? FCVTZS4 : FCVTNS4, 2, b_);
            Slow sp = begin_slow();
            a.v3(FCMEQ4, 3, b_, b_); a.v2(NOT16, 3, 3); a.v2(MVNI_7FFFFFFF, 4, 0); a.v3(CMEQ4, 4, 2, 4); a.vorr(3, 3, 4);
            a.v2(UMAXV4, 3, 3); a.fmov_w_s(9, 3); a.cbnz(9, *sp.slow);
            a.vmov(Xdef(o[0].ridx), 2);
            end_slow(sp, &in, done);
            break; }
        case M_cvtsi2ss: case M_cvtsi2sd: {
            if (!X(o[0]) || !(R(o[1]) || (Mem(o[1]) && o[1].size == 4))) { ok = false; break; }
            rd(o[1], 4, done);
            if (mn == M_cvtsi2ss) { a.v2(SCVTF_S_W, 1, 13); a.ins_s(Xmod(o[0].ridx), 0, 1, 0); }
            else { a.v2(SCVTF_D_W, 1, 13); a.ins_d(Xmod(o[0].ridx), 0, 1, 0); }
            break; }
        case M_cvttss2si: case M_cvtss2si: case M_cvttsd2si: case M_cvtsd2si: {
            if (!R(o[0]) || !(X(o[1]) || Mem(o[1]))) { ok = false; break; }
            bool dbl = mn == M_cvttsd2si || mn == M_cvtsd2si, trunc = mn == M_cvttss2si || mn == M_cvttsd2si;
            int b_ = dbl ? xsrc_d(0) : xsrc_s(0);
            Slow sp = begin_slow();
            // NaN, or a saturated result (0x7fffffff: out of range, or exact for doubles): the interpreter
            if (dbl) a.fcmpd(b_, b_); else a.fcmps(b_, b_);
            a.bcond(VS, *sp.slow);
            a.v2(dbl ? (trunc ? FCVTZS_W_D : FCVTNS_W_D) : (trunc ? FCVTZS_W_S : FCVTNS_W_S), 12, b_);
            a.mov32(14, 0x7fffffffu); a.cmp(12, 14); a.bcond(EQ, *sp.slow);
            strw_r(12, o[0].ridx);
            end_slow(sp, &in, done);
            break; }
        case M_cvtss2sd: case M_cvtsd2ss: {
            if (!X(o[0]) || !(X(o[1]) || Mem(o[1]))) { ok = false; break; }
            bool tod = mn == M_cvtss2sd;
            int b_ = tod ? xsrc_s(0) : xsrc_d(0);
            X_(o[0].ridx);
            Slow sp = begin_slow();
            if (tod) a.fcmps(b_, b_); else a.fcmpd(b_, b_);
            a.bcond(VS, *sp.slow);
            if (tod) { a.v2(FCVT_D_S, 1, b_); a.ins_d(Xmod(o[0].ridx), 0, 1, 0); } else { a.v2(FCVT_S_D, 1, b_); a.ins_s(Xmod(o[0].ridx), 0, 1, 0); }
            end_slow(sp, &in, done);
            break; }
        // ---- x87 loads / stores / stack moves: their own C++ entry (no dispatch through the interpreter)
        case M_fldcw: case M_fnstcw:
            if (Mem(o[0])) { ea(o[0]); helper(in, done, &JitHelpers::x87_cw); } else ok = false;
            break;
        case M_fild:
            if (Mem(o[0]) && (o[0].size == 2 || o[0].size == 4 || o[0].size == 8)) { ea(o[0]); helper(in, done, &JitHelpers::x87_fild); } else ok = false;
            break;
        case M_fist: case M_fistp:
            if (Mem(o[0]) && (o[0].size == 2 || o[0].size == 4 || o[0].size == 8)) { ea(o[0]); helper(in, done, &JitHelpers::x87_fist); } else ok = false;
            break;
        case M_fadd: case M_fsub: case M_fmul: case M_fdiv: case M_fsubr: case M_fdivr:
        case M_faddp: case M_fsubp: case M_fmulp: case M_fdivp: case M_fsubrp: case M_fdivrp:
            if (Mem(o[0])) ea(o[0]);
            helper(in, done, &JitHelpers::x87_arith);
            break;
        case M_emms: helper(in, done, &JitHelpers::mmx_ops); break;
        case M_cvtpi2ps:
            if (X(o[0]) && o[1].type == 1 && o[1].rclass == 7) helper(in, done, &JitHelpers::mmx_ops); else ok = false;
            break;
        case M_fld: case M_fst: case M_fstp: case M_fxch: case M_fld1: case M_fldz: {
            bool memf = (mn == M_fld || mn == M_fst || mn == M_fstp) && Mem(o[0]) && (o[0].size == 4 || o[0].size == 8);
            bool regf = (mn == M_fld || mn == M_fst || mn == M_fstp) ? (o[0].type == 1 && o[0].rclass == 6) : true;
            if (memf) { ea(o[0]); helper(in, done, mn == M_fld ? &JitHelpers::x87_ld : &JitHelpers::x87_st); }
            else if (regf) helper(in, done, &JitHelpers::x87_reg);
            else ok = false;
            break; }
        default: ok = false; break;
        }
        okv[k] = uint8_t(ok);
        if (!ok) {
            helper(in, done);
            if (is_transfer(mn)) {
                // the handler's pc is the transfer target either way
                auto x = std::make_unique<Exit>(); x->done = done + 1; x->reg = true; Label* l = &x->l; exits.push_back(std::move(x));
                a.mov32(13, in.next); a.b(*l); ended = true;
            }
        }
    }
    if (!ended) { to_exit(); a.b(*exit_imm(int(list.size()), list.back()->next)); }
    if (pass == 0) {
        // flag liveness, backwards from the block's end (where every flag may be read)
        uint32_t live = 0x8d5u;
        for (size_t k = list.size(); k-- > 0;) {
            live_after[k] = live;
            uint32_t rd_ = 0, wr = 0; uint16_t mn = list[k]->mn;
            if (!okv[k]) rd_ = 0x8d5u;   // the interpreter's handler: may read any flag
            else switch (mn) {
                case M_add: case M_sub: case M_cmp: case M_and: case M_or: case M_xor: case M_test: case M_neg:
                case M_shl: case M_sal: case M_shr: case M_sar: wr = 0x8c5u; break;
                case M_adc: case M_sbb: wr = 0x8c5u; rd_ = 1; break;
                case M_inc: case M_dec: wr = 0x8c4u; break;
                case M_imul: wr = 0x801u; break;
                case M_comiss: case M_ucomiss: case M_comisd: case M_ucomisd: wr = 0x8d5u; break;
                default:
                    if (is_jcc(mn) || (mn >= M_seta && mn <= M_cmovns)) rd_ = 0x8d5u;
                    break;
            }
            live = (live & ~wr) | rd_;
        }
        continue;
    }
    // out-of-line slow paths
    for (size_t i = 0; i < outlines.size(); i++) outlines[i]();
    // exits: m.steps += done; w0 = target
    std::vector<std::pair<uint32_t, uint64_t*>> chain;   // (target pc, slot) of this block's direct exits
    for (auto& x : exits) {
        a.bind(x->l);
        ldrx_m(9, STEPS); a.addi(9, 9, uint32_t(x->done), true);
        if (STEPS <= 32760) a.str_u(3, 9, 19, STEPS); else { a.mov32(16, STEPS); a.str_r(3, 9, 19, 16); }
        if (x->reg) {
            // indirect exit: the lookup cache (pc, fn, body) from generated code; a miss returns to the dispatcher
            Label miss;
            // a nested call's return address (Machine::stop_pc: lifted ports' calls, Verify) goes back to the dispatcher, which
            // ends the run there when ESP is above stop_esp (never chain past it)
            a.ldr_u(2, 17, 19, STOP_PC); a.cmp(17, 13); a.bcond(EQ, miss);
            a.lsr_imm(10, 13, 16); a.eor(10, 10, 13, 0); a.e(0x12003C00 | 10u << 5 | 10u);   // and w10, w10, #0xffff
            a.mov64(16, reinterpret_cast<uint64_t>(cache_.get())); a.add(16, 16, 10, 5, true);  // x16 = &cache[idx] (32-byte entries)
            a.ldr_u(2, 17, 16, 0); a.cmp(17, 13); a.bcond(NE, miss);
            a.ldr_u(3, 17, 16, 16); a.cbz(17, miss, true);
            a.e(0xD61F0000 | 17u << 5);   // br x17
            a.bind(miss);
            a.orr(0, 31, 13); a.b(epilogue);
            continue;
        }
        // direct exit: jump straight into the target block's body once it is translated (slot != 0)
        uint64_t* slot = new_slot(); chain.push_back({x->target, slot});
        Label to_dispatch;
        a.mov64(16, reinterpret_cast<uint64_t>(slot)); a.ldr_u(3, 17, 16, 0); a.cbz(17, to_dispatch, true);
        a.e(0xD61F0000 | 17u << 5);   // br x17
        a.bind(to_dispatch);
        a.mov32(0, x->target);
        a.b(epilogue);
    }
    for (auto& [l, done] : faults) {
        a.bind(*l);
        ldrx_m(9, STEPS); a.addi(9, 9, uint32_t(done), true);
        if (STEPS <= 32760) a.str_u(3, 9, 19, STEPS); else { a.mov32(16, STEPS); a.str_r(3, 9, 19, 16); }
        a.mov64(0, 1ull << 32);
        a.b(epilogue);
    }
    a.bind(epilogue);
    a.ldp(27, 28, SP, 80); a.ldp(25, 26, SP, 64); a.ldp(23, 24, SP, 48); a.ldp(21, 22, SP, 32); a.ldp(19, 20, SP, 16); a.ldp_post(29, 30, SP, 96); a.ret();
    // install
    size_t bytes = a.c.size() * 4;
    if (used_ + bytes > cap_) { flush(); flushed_ = true; return nullptr; }   // this block's chain slots were reset: translate again later
    uint8_t* dst = code_ + used_;
    pthread_jit_write_protect_np(0);
    std::memcpy(dst, a.c.data(), bytes);
    pthread_jit_write_protect_np(1);
    sys_icache_invalidate(dst, bytes);
    used_ += (bytes + 15) & ~size_t(15);
    nblocks_++;
    if (static FILE* map = std::getenv("REFLEX_JIT_MAP") ? std::fopen(std::getenv("REFLEX_JIT_MAP"), "w") : nullptr; map) {   // debugging: host code -> guest block
        std::fprintf(map, "%llx %zx %08x %zu\n", (unsigned long long)reinterpret_cast<uint64_t>(dst), bytes, start, list.size()); std::fflush(map);
    }
    uint64_t body = reinterpret_cast<uint64_t>(dst) + uint64_t(body_at) * 4;
    body_[start] = body;
    auto w = waiting_.find(start);
    if (w != waiting_.end()) { for (uint64_t* sl : w->second) *sl = body; waiting_.erase(w); }
    for (auto& [target, sl] : chain) {
        auto b = body_.find(target);
        if (b != body_.end()) *sl = b->second; else waiting_[target].push_back(sl);
    }
    return reinterpret_cast<BlockFn>(dst);
    }
    return nullptr;
}
#else
Jit::Jit(Machine& m) : m_(m) {}
Jit::~Jit() {}
void Jit::flush() {}
Jit::BlockFn Jit::lookup(uint32_t) { return nullptr; }
Jit::BlockFn Jit::translate(uint32_t) { return nullptr; }
#endif
}  // namespace reflex_vm
