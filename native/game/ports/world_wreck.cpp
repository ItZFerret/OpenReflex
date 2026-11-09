// World group, wreck share: float-property broadcasts (0x0063b8xx) and the uniform cell grid of 0x00668xxx.
#include "world_wreck_util.h"

using namespace world;
using namespace wreck;
using reflex_port::OUT_EAX;
using reflex_port::OUT_ECX;
using reflex_port::OUT_EDX;
using reflex_port::OUT_XMM_ALL;

namespace {

// 0063b830 (stdcall 8: arg 0 = listener vector, arg 1 = float*): for every 8-byte element {float* target, int enabled} with
// enabled != 0, *target = *value (fld / fstp). std::vector with checked iterators: the vector's +0 points to a proxy whose +0 is
// the container (begin +0xc, end +0x10); every check calls the CRT abort as the original does. `bits` is *value; EDX at entry
// as the caller leaves it. Sets ECX / EDX, returns EAX.
uint32_t listeners_set(Machine& m, uint32_t v, uint32_t bits) {
    Guest g{m};
    uint32_t eax = g.u32(v + 0x10), esi = g.u32(v + 0xc), ecx = eax - esi, edx = m.r[EDX];
    if ((ecx & 0xfffffff8u) != 0) {
        if (esi > eax) crt_abort(m);
        const uint32_t edi = g.u32(v);
        uint32_t ebx = esi;
        for (;;) {
            esi = g.u32(v + 0x10);
            if (g.u32(v + 0xc) > esi) crt_abort(m);
            eax = g.u32(v);
            if (edi == 0 || edi != eax) crt_abort(m);
            if (ebx == esi) break;
            auto check = [&] {
                if (edi == 0) crt_abort(m);
                eax = g.u32(edi);
                if (!(ebx < g.u32(eax + 0x10))) crt_abort(m);
            };
            check();
            if (g.u32(ebx + 4) != 0) {
                check();
                edx = g.u32(ebx);
                g.u32(edx, fcopy32(m, bits));
            }
            check();
            ebx += 8;
        }
    }
    m.r[ECX] = ecx; m.r[EDX] = edx;
    return eax;
}

// 0063b8f0 (stdcall 4: arg = listener vector): xmm0 = the first enabled listener's value (movss), 0 when none. Sets ECX / EDX,
// returns EAX.
uint32_t listeners_get(Machine& m, uint32_t v) {
    Guest g{m};
    uint32_t eax = g.u32(v + 0x10), edi = g.u32(v + 0xc), ecx = eax - edi;
    X x0{};
    if ((ecx & 0xfffffff8u) != 0) {
        if (edi > eax) crt_abort(m);
        uint32_t esi = g.u32(v), ebx = edi;
        for (;;) {
            edi = g.u32(v + 0x10);
            if (g.u32(v + 0xc) > edi) crt_abort(m);
            eax = g.u32(v);
            if (esi == 0 || esi != eax) crt_abort(m);
            if (ebx == edi) break;
            if (esi == 0) { crt_abort(m); eax = 0; } else eax = g.u32(esi);
            if (!(ebx < g.u32(eax + 0x10))) crt_abort(m);
            if (g.u32(ebx + 4) != 0) {
                if (esi == 0) crt_abort(m); else esi = g.u32(esi);
                if (!(ebx < g.u32(esi + 0x10))) crt_abort(m);
                const uint32_t edx = g.u32(ebx);
                x0 = ss(g, edx);
                m.r[EDX] = edx;
                break;
            }
            if (esi == 0) { crt_abort(m); eax = 0; } else eax = g.u32(esi);
            if (!(ebx < g.u32(eax + 0x10))) crt_abort(m);
            ebx += 8;
        }
    }
    xw(g, 0, x0);
    m.r[ECX] = ecx;
    return eax;
}

// The ranged float of 0x0063b0f0 / 0x0063b160: {min +0, max +4, rate +8, limit +0xc, value +0x10, listeners +0x14}. The value
// moves toward the input by min(rate, limit) / rate of the gap; the listeners get its clamped position in [min, max] as 0..1.
// Leaves xmm0..3 as the original; returns the broadcast value.
uint32_t ranged_step(Machine& m, uint32_t obj, uint32_t in) {
    Guest g{m};
    X x3 = ss(g, obj + 8), x0 = ss(g, obj + 0xc), x1 = x3, x2;
    if (c_ja(x1.f(0), x0.f(0))) x1 = x0;
    x2 = ss(g, obj + 0x10);
    x0 = ss(g, in);
    x0 = subss(x0, x2); x1 = divss(x1, x3); x0 = mulss(x0, x1); x0 = addss(x0, x2);
    g.u32(obj + 0x10, x0.u[0]);
    x1 = ss(g, obj + 4);
    x2 = ss(g, obj);
    if (!c_jb(x0.f(0), x1.f(0))) x0 = x1;
    if (!c_jb(x2.f(0), x0.f(0))) x0 = x2;
    x0 = subss(x0, x2); x1 = subss(x1, x2); x0 = divss(x0, x1);
    xw(g, 0, x0); xw(g, 1, x1); xw(g, 2, x2); xw(g, 3, x3);
    return x0.u[0];
}

}  // namespace

REFLEX_PORT_FN(0x0063b830, FloatListeners_Set, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    Guest g{m};
    uint32_t r = listeners_set(m, m.arg(0), g.u32(m.arg(1)));
    return m.ret(8, r);
}
REFLEX_PORT_FN(0x0063b8f0, FloatListeners_Get, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    uint32_t r = listeners_get(m, m.arg(0));
    return m.ret(4, r);
}
// 0063b0e0 (EAX = ranged float): its listeners' value (0x0063b8f0 on +0x14).
REFLEX_PORT_FN(0x0063b0e0, RangedFloat_Get, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    uint32_t r = listeners_get(m, m.r[EAX] + 0x14);
    return m.ret(0, r);
}
// 0063b0f0 (ESI = ranged float, EAX = float* input): step and broadcast; EAX = ESI, ECX = the broadcast value (its stack slot).
REFLEX_PORT_FN(0x0063b0f0, RangedFloat_Step, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    const uint32_t obj = m.r[ESI];
    uint32_t v = ranged_step(m, obj, m.r[EAX]);
    m.r[EDX] = obj + 0x14;
    listeners_set(m, obj + 0x14, v);
    m.r[ECX] = v;
    return m.ret(0, obj);
}
// 0063b160 (EAX = ranged float, ECX = float* input): as 0x0063b0f0; returns 1 when it has listeners, ECX = the broadcast value.
REFLEX_PORT_FN(0x0063b160, RangedFloat_StepAny, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Guest g{m};
    const uint32_t obj = m.r[EAX];
    uint32_t v = ranged_step(m, obj, m.r[ECX]);
    m.r[EDX] = m.r[ESP] - 4;
    listeners_set(m, obj + 0x14, v);
    m.r[ECX] = v;
    const uint32_t n = (g.u32(obj + 0x24) - g.u32(obj + 0x20)) & 0xfffffff8u;
    return m.ret(0, n != 0 ? 1u : 0u);
}
// 0063b480 (EAX = {min, max, float* out}, ECX = float* input): *out = clamp(input, min, max) when out is set.
REFLEX_PORT_FN(0x0063b480, ClampedFloat_Write, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Guest g{m};
    const uint32_t obj = m.r[EAX];
    X x0 = ss(g, m.r[ECX]), x1 = ss(g, obj + 4), x2;
    x2 = ss(g, obj);
    if (!c_jb(x0.f(0), x1.f(0))) x0 = x1;
    if (!c_jb(x2.f(0), x0.f(0))) x0 = x2;
    const uint32_t out = g.u32(obj + 8);
    if (out) g.u32(out, x0.u[0]);
    xw(g, 0, x0); xw(g, 1, x1); xw(g, 2, x2);
    m.r[ECX] = out;
    return m.ret(0, obj);
}
// 0063b500 (ECX = {min, max, float* src}, arg = float*; ret 4): xmm0 = clamp((src ? *src : 0) - *arg, min, max).
REFLEX_PORT_FN(0x0063b500, ClampedFloat_Diff, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Guest g{m};
    const uint32_t obj = m.r[ECX], a = m.arg(0), src = g.u32(obj + 8);
    X x0 = src ? ss(g, src) : X{}, x1, x2;
    x0 = subss(x0, ss(g, a));
    x1 = ss(g, obj + 4);
    x2 = ss(g, obj);
    if (!c_jb(x0.f(0), x1.f(0))) x0 = x1;
    if (!c_jb(x2.f(0), x0.f(0))) x0 = x2;
    xw(g, 0, x0); xw(g, 1, x1); xw(g, 2, x2);
    return m.ret(4, a);
}
// 0063b540 (ESI = {min, max, listeners +8}, EAX = float*): broadcast clamp(*in, min, max); EAX = ESI, ECX = the value.
REFLEX_PORT_FN(0x0063b540, ClampedFloat_Set, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Guest g{m};
    const uint32_t obj = m.r[ESI];
    X x0 = ss(g, m.r[EAX]), x1 = ss(g, obj + 4), x2;
    x2 = ss(g, obj);
    if (!c_jb(x0.f(0), x1.f(0))) x0 = x1;
    const uint32_t v = c_jb(x2.f(0), x0.f(0)) ? x0.u[0] : x2.u[0];
    xw(g, 0, x0); xw(g, 1, x1); xw(g, 2, x2);
    m.r[EDX] = obj + 8;
    listeners_set(m, obj + 8, v);
    m.r[ECX] = v;
    return m.ret(0, obj);
}
// 0063b590 (ESI = {min, max, listeners +8}, arg = float* delta; ret 4): broadcast clamp(current + *delta, min, max), the value
// also written into the argument slot (the original passes its address).
REFLEX_PORT_FN(0x0063b590, ClampedFloat_Add, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Guest g{m};
    const uint32_t obj = m.r[ESI], a = m.arg(0), ecx0 = m.r[ECX], slot = m.r[ESP] + 4;
    listeners_get(m, obj + 8);
    X x0 = xr(g, 0), x1 = ss(g, a), x2 = ss(g, obj);
    x1 = addss(x1, x0);
    x0 = ss(g, obj + 4);
    if (!c_jae(x1.f(0), x0.f(0))) x0 = x1;
    const uint32_t v = c_jb(x2.f(0), x0.f(0)) ? x0.u[0] : x2.u[0];
    xw(g, 0, x0); xw(g, 1, x1); xw(g, 2, x2);
    g.u32(slot, v);
    listeners_set(m, obj + 8, v);
    m.r[ECX] = ecx0;
    return m.ret(4, obj);
}
// 0063b650 (ESI = {min, max, rate +0xc (float*), listeners +0x10}, arg = float*; ret 4): broadcast current + *arg * rate wrapped
// into [min, max] (once), written into the argument slot too.
REFLEX_PORT_FN(0x0063b650, WrappedFloat_Add, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Guest g{m};
    const uint32_t obj = m.r[ESI], a = m.arg(0), ecx0 = m.r[ECX], slot = m.r[ESP] + 4;
    listeners_get(m, obj + 0x10);
    X x0 = xr(g, 0), x1, x2;
    const uint32_t rp = g.u32(obj + 0xc);
    x2 = rp ? ss(g, rp) : X{};
    x1 = ss(g, a);
    x1 = mulss(x1, x2); x1 = addss(x1, x0);
    x0 = ss(g, obj + 4);
    if (c_ja(x1.f(0), x0.f(0))) x1 = subss(x1, x0);
    else {
        x2 = ss(g, obj);
        if (c_ja(x2.f(0), x1.f(0))) { x1 = subss(x1, x2); x1 = addss(x1, x0); }
    }
    xw(g, 0, x0); xw(g, 1, x1); xw(g, 2, x2);
    g.u32(slot, x1.u[0]);
    listeners_set(m, obj + 0x10, x1.u[0]);
    m.r[ECX] = ecx0;
    return m.ret(4, obj);
}
// 0063b6c0 (EAX = {?, byte* flag +4, short* out +8}, EDX = short*): *flag = 1, *out = *EDX (each when set).
REFLEX_PORT_FN(0x0063b6c0, ShortProperty_Set, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    Guest g{m};
    const uint32_t obj = m.r[EAX];
    uint32_t ecx = g.u32(obj + 4);
    if (ecx) g.u8(ecx, 1);
    ecx = g.u32(obj + 8);
    if (ecx) {
        const uint32_t w = uint32_t(m.mem.read(m.r[EDX], 2));
        m.r[EDX] = (m.r[EDX] & 0xffff0000u) | w;
        m.mem.write(ecx, 2, w);
    }
    m.r[ECX] = ecx;
    return m.ret(0);
}

namespace {

// 00668910 (thiscall 8: this = grid, arg 0 = point (16 bytes), arg 1 = cell): 1 when the point's x / y lanes (z from the constant
// 0xd61670) lie in the cell's box (centre +0x60, half extent +0x70 of the 128-byte cell record), else 0. Registers as the original.
uint32_t grid_cell_contains(Machine& m, uint32_t self, uint32_t p, uint32_t cell) {
    Guest g{m};
    uint32_t eax = (cell << 7) + g.u32(self + 4), edx = g.u32(self + 4), ecx = self;
    if (g.u32(eax + 8) == 0) { m.r[ECX] = ecx; m.r[EDX] = edx; return 0; }
    X x2 = ld(g, eax + 0x70), x0 = ld(g, eax + 0x60), x3 = ld(g, p), x1, x4;
    x1 = addps(x2, x0);
    x0 = subps(x0, x2);
    x2 = ld(g, 0xd61670);
    x4 = movss(shufps(x2, x3, 0xe5), x3); x3 = x4;
    x4 = movss(shufps(x2, x1, 0xe5), x1); x1 = x4;
    x4 = cmpleps(x3, x1);
    ecx = x4.u[1]; edx = x4.u[2] + ecx + x4.u[0];
    uint32_t r = 0;
    if (edx == 0xfffffffdu) {
        x2 = movss(shufps(x2, x0, 0xe5), x0); x0 = x2;
        x0 = cmpleps(x0, x3);
        eax = x0.u[1]; ecx = x0.u[2] + eax + x0.u[0];
        if (ecx == edx) r = 1;
    }
    xw(g, 0, x0); xw(g, 1, x1); xw(g, 2, x2); xw(g, 3, x3); xw(g, 4, x4);
    m.r[ECX] = ecx; m.r[EDX] = edx;
    return r;
}

// 00668820 (ECX = grid, EDX = point): the cell index of the point, -1 outside the grid. Sets [[grid]+0x9c]+0x4c to 1 while it
// reads the level's bounds ([+0x44] + 0x24af0 ...), then 0.
uint32_t grid_cell_of(Machine& m, uint32_t self, uint32_t p) {
    Guest g{m};
    uint32_t eax = g.u32(g.u32(self) + 0x9c);
    X x2 = ss(g, 0x9cd9e8), x0, x1, x3;
    g.u32(eax + 0x4c, 1);
    eax = g.u32(eax + 0x44);
    x0 = ss(g, eax + 0x24af0); x1 = ss(g, eax + 0x24af8); x3 = ss(g, eax + 0x24b08);
    x1 = mulss(x1, x2); x0 = mulss(x0, x2);
    x2 = ss(g, eax + 0x24b00); x2 = addss(x2, x0);
    x0 = ss(g, p + 8);
    x3 = addss(x3, x1);
    x1 = ss(g, p);
    x1 = subss(x1, x2);
    x2 = X{};
    x0 = subss(x0, x3);
    uint32_t r;
    bool fail = c_ja(x2.f(0), x0.f(0)) || c_ja(x2.f(0), x1.f(0));
    if (!fail) {
        x3 = ss(g, eax + 0x24af8); x2 = ss(g, 0x92e330); x3 = mulss(x3, x2);
        fail = c_ja(x0.f(0), x3.f(0));
        if (!fail) {
            x3 = ss(g, eax + 0x24af0); x3 = mulss(x3, x2);
            fail = c_ja(x1.f(0), x3.f(0));
        }
    }
    if (!fail) {
        eax = g.u32(g.u32(self) + 0x9c);
        x2 = ss(g, self + 0x18); x2 = mulss(x2, x0);
        x0 = ss(g, self + 0x1c);
        g.u32(eax + 0x4c, 0);
        eax = g.u32(self + 0xc);
        x0 = mulss(x0, x1);
        int32_t esi = x86::cvtt(x0.f(0));
        eax = eax * uint32_t(esi);
        uint32_t edx = uint32_t(x86::cvtt(x2.f(0)));
        r = eax + edx;
        m.r[ECX] = self; m.r[EDX] = edx;
    } else {
        uint32_t ecx = g.u32(self), edx = g.u32(ecx + 0x9c);
        g.u32(edx + 0x4c, 0);
        r = 0xffffffffu;
        m.r[ECX] = ecx; m.r[EDX] = edx;
    }
    xw(g, 0, x0); xw(g, 1, x1); xw(g, 2, x2); xw(g, 3, x3);
    return r;
}

// 00668710 (ECX = grid, EDX = cell): the 3x3 neighbourhood of the cell into +0x188.. (centre, up, up-right, right, down-right,
// down, down-left, left, up-left; -1 outside the grid of width +0xc and cell count +0x10), the 9 flags +0x1ac.. = 9 and the hit
// count +0x1d0 = 0. Returns 9; EDX = the last remainder.
uint32_t grid_neighbours(Machine& m, uint32_t self, uint32_t cell) {
    Guest g{m};
    const uint32_t ebx = g.u32(self + 0xc), edx = cell;
    const uint32_t edi = ebx + edx;
    const uint32_t up = edx - ebx;
    g.u32(self + 0x198, edi + 1);
    g.u32(self + 0x18c, up);
    g.u32(self + 0x190, up + 1);
    g.u32(self + 0x1a0, edi - 1);
    g.u32(self + 0x1a8, up - 1);
    g.u32(self + 0x188, edx);
    g.u32(self + 0x194, edx + 1);
    g.u32(self + 0x19c, edi);
    g.u32(self + 0x1a4, edx - 1);
    const uint32_t none = 0xffffffffu;
    if (int32_t(up) < 0) { g.u32(self + 0x1a8, none); g.u32(self + 0x190, none); g.u32(self + 0x18c, none); }
    if (int32_t(edi) > int32_t(g.u32(self + 0x10) - 1)) { g.u32(self + 0x1a0, none); g.u32(self + 0x19c, none); g.u32(self + 0x198, none); }
    if (int32_t(edx + 1) % int32_t(ebx) == 0) { g.u32(self + 0x198, none); g.u32(self + 0x194, none); g.u32(self + 0x190, none); }
    const int32_t rem = int32_t(edx - 1) % int32_t(ebx);
    if (rem == int32_t(ebx - 1)) { g.u32(self + 0x1a8, none); g.u32(self + 0x1a4, none); g.u32(self + 0x1a0, none); }
    for (int i = 0; i < 9; i++) g.u32(self + 0x1ac + 4 * i, 9);
    g.u32(self + 0x1d0, 0);
    m.r[ECX] = self; m.r[EDX] = uint32_t(rem);
    return 9;
}

// 006689c0 (EDI = grid, stack arg = point; ret 4): tests the point against the neighbourhood cells (+0x188..), setting each
// flag (+0x1ac..) to 0 / 1 and counting hits in +0x1d0; the loop bound shrinks once a side cell hits (the original's ladder).
// Returns AL = 1 when the centre cell holds the point, else AL = (no hit); ECX = the security cookie (its check leaves it).
uint32_t grid_test_neighbours(Machine& m, uint32_t self, uint32_t p) {
    Guest g{m};
    uint32_t ebx = 9, eax = p;
    bool flag = false;
    xw(g, 0, ld(g, p));
    for (uint32_t esi = 0; esi < ebx; esi++) {
        g.u32(self + esi * 4 + 0x1ac, 0);
        if (flag && (esi == 4 || esi == 5 || esi == 6)) continue;
        eax = g.u32(self + esi * 4 + 0x188);
        if (int32_t(eax) < 0) continue;
        eax = grid_cell_contains(m, self, p, eax);
        if (eax == 0) continue;
        eax = 1;
        g.u32(self + esi * 4 + 0x1ac, 1);
        g.u32(self + 0x1d0, g.u32(self + 0x1d0) + 1);
        if (esi == 1) flag = true;
        else if (esi == 2) ebx = 4;
        else if (esi == 3 || esi == 4) ebx = 6;
    }
    if (g.u32(self + 0x1ac) == 1) eax = (eax & 0xffffff00u) | 1;
    else eax = (eax & 0xffffff00u) | (g.u32(self + 0x1d0) == 0 ? 1u : 0u);
    m.r[ECX] = g.u32(0x9b45e4);
    return eax;
}

// 00668690 (EAX = grid, stack arg = point; ret 4): locate the point's cell and its neighbourhood; when the centre cell does not
// hold it, recentre on the first neighbour that does and test again. Returns -1 outside the grid, else 0.
uint32_t grid_locate(Machine& m, uint32_t self, uint32_t p) {
    uint32_t r = grid_cell_of(m, self, p);
    if (int32_t(r) < 0) return 0xffffffffu;
    grid_neighbours(m, self, r);
    uint32_t al = grid_test_neighbours(m, self, p);
    if ((al & 0xff) == 0) {
        Guest g{m};
        uint32_t esi = 0;
        for (uint32_t eax = 1; eax < 9; eax++)
            if (g.u32(self + 0x1ac + 4 * eax) == 1) { esi = g.u32(self + eax * 4 + 0x188); break; }
        grid_neighbours(m, self, esi);
        grid_test_neighbours(m, self, p);
    }
    return 0;
}

// 00668650 (EAX = grid, EDX = int list ending in -1): appends each neighbourhood cell whose flag is 1 to the list unless present
// (no new terminator is written: the caller's list is pre-filled with -1). ECX = the last slot index, EAX the last cell.
void grid_collect(Machine& m, uint32_t self, uint32_t list) {
    Guest g{m};
    uint32_t eax = m.r[EAX], ecx = m.r[ECX];
    for (uint32_t esi = self + 0x188, n = 0; n < 9; n++, esi += 4) {
        if (g.u32(esi + 0x24) != 1) continue;
        ecx = 0;
        if (g.u32(list) != 0xffffffffu) {
            const uint32_t edi = g.u32(esi);
            while (g.u32(list + ecx * 4) != edi) {
                ecx++;
                if (g.u32(list + ecx * 4) == 0xffffffffu) break;
            }
        }
        eax = g.u32(esi);
        g.u32(list + ecx * 4, eax);
    }
    m.r[EAX] = eax; m.r[ECX] = ecx;
}

}  // namespace

REFLEX_PORT_FN(0x00668910, Grid_CellContains, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    uint32_t r = grid_cell_contains(m, m.r[ECX], m.arg(0), m.arg(1));
    return m.ret(8, r);
}
REFLEX_PORT_FN(0x00668820, Grid_CellOf, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    uint32_t r = grid_cell_of(m, m.r[ECX], m.r[EDX]);
    return m.ret(0, r);
}
REFLEX_PORT_FN(0x00668710, Grid_Neighbours, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    uint32_t r = grid_neighbours(m, m.r[ECX], m.r[EDX]);
    return m.ret(0, r);
}
REFLEX_PORT_FN(0x006689c0, Grid_TestNeighbours, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    uint32_t r = grid_test_neighbours(m, m.r[EDI], m.arg(0));
    return m.ret(4, r);
}
REFLEX_PORT_FN(0x00668690, Grid_Locate, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    uint32_t r = grid_locate(m, m.r[EAX], m.arg(0));
    return m.ret(4, r);
}
REFLEX_PORT_FN(0x00668650, Grid_CollectHits, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    grid_collect(m, m.r[EAX], m.r[EDX]);
    return m.ret(0);
}

// 00667e40 (ECX = object with a bounds virtual (+0xc), EDX = receiver 2, stack: grid, point, arg2, out2 (float*), receiver 1,
// out1 (float*); ret 0x18): finds the grid cells around the four probe points (the point +- the swept extent from the virtual,
// the virtual's second vector), then for every item of every layer of those cells (48-byte records {centre, radius, ...}) whose
// scaled box overlaps the swept range: computes a radial falloff vector (cos-like polynomial of the item's +0x14 parameter and a
// fixed direction) and reports it to receiver 1 (layer flag +1) and receiver 2 (layer flag +0) through 0x0085ddd0, bracketed by
// the layer object's virtuals +0x10 (item radius) and +0x14. Locals passed to callees live at the original's stack addresses.
REFLEX_PORT_FN(0x00667e40, Grid_ReportOverlaps, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Guest g{m};
    Frame f(m, 0x164, 3);
    const uint32_t P = f.esp;
    const uint32_t self = m.r[ECX], recv2 = m.r[EDX];
    const uint32_t grid = m.arg(0), pos = m.arg(1), arg2 = m.arg(2), out2 = m.arg(3), recv1 = m.arg(4), out1 = m.arg(5);
    X x0, x1, x2, x3, x4, x5, x6, x7;
    auto flush = [&] { xw(g, 0, x0); xw(g, 1, x1); xw(g, 2, x2); xw(g, 3, x3); xw(g, 4, x4); xw(g, 5, x5); xw(g, 6, x6); xw(g, 7, x7); };
    auto reload = [&] { x0 = xr(g, 0); x1 = xr(g, 1); x2 = xr(g, 2); x3 = xr(g, 3); x4 = xr(g, 4); x5 = xr(g, 5); x6 = xr(g, 6); x7 = xr(g, 7); };
    reload();
    m.r[EBX] = pos; m.r[ESI] = P + 0x110; m.r[EDI] = grid; m.r[EDX] = g.u32(self);
    f.call_at(P, g.u32(g.u32(self) + 0xc), {arg2, pos, P + 0x110, P + 0xf0}, self, g.u32(g.u32(self) + 0xc));
    reload();
    // the four probe points P+0xa0 / 0xb0 / 0xc0 / 0xd0 (x, y, z; w left as the stack holds it)
    x1 = ld(g, pos);
    x0 = ld(g, P + 0xf0);
    x2 = ss(g, pos + 4); x4 = ss(g, 0x9cd9e8); x3 = ss(g, pos + 8);
    x0 = subps(x0, x1);
    x1 = ss(g, pos);
    const X D = x0;
    x0 = ss(g, P + 0x110); x5 = ss_bits(D.u[0]);
    g.u32(P + 0xa0, x0.u[0]);
    x0 = ss(g, P + 0x114); g.u32(P + 0xa4, x0.u[0]);
    x0 = ss(g, P + 0x118);
    x6 = addss(x1, x5);
    const uint32_t e0 = x6.u[0];
    x6 = ss_bits(D.u[1]);
    g.u32(P + 0xa8, x0.u[0]);
    x0 = ss_bits(D.u[2]);
    x7 = addss(x2, x6);
    const uint32_t e4 = x7.u[0];
    x0 = mulss(x0, x4);
    x7 = addss(x3, x0);
    const uint32_t e8 = x7.u[0];
    g.u32(P + 0xb0, e0); g.u32(P + 0xb4, e4); g.u32(P + 0xb8, e8);
    x7 = ss(g, P + 0xf0); g.u32(P + 0xc0, x7.u[0]);
    x7 = ss(g, P + 0xf4); g.u32(P + 0xc4, x7.u[0]);
    x7 = ss(g, P + 0xf8);
    x5 = mulss(x5, x4); x6 = mulss(x6, x4); x0 = mulss(x0, x4);
    x1 = addss(x1, x5); x2 = addss(x2, x6); x3 = addss(x3, x0);
    g.u32(P + 0xc8, x7.u[0]);
    g.u32(P + 0xd0, x1.u[0]); g.u32(P + 0xd4, x2.u[0]); g.u32(P + 0xd8, x3.u[0]);
    const uint32_t list = P + 0x80;
    for (int i = 0; i < 8; i++) g.u32(list + 4 * i, 0xffffffffu);
    flush();
    for (int i = 0; i < 4; i++) {
        m.r[EAX] = grid_locate(m, grid, P + 0xa0 + 16 * i);
        m.r[EDX] = list; m.r[EAX] = grid;
        grid_collect(m, grid, list);
    }
    reload();
    uint32_t eax = g.u32(list), ecx, edx = list;
    if (eax == 0xffffffffu) {
        f.restore_saved();
        m.r[EDX] = edx; m.r[ECX] = g.u32(0x9b45e4);
        flush();
        return m.ret(0x18, eax);
    }
    {
        const uint32_t c = g.u32(g.u32(grid) + 0x9c);
        const uint32_t level = g.u32(c + 0x44);
        g.u32(c + 0x4c, 1);
        uint32_t k = 0;
        for (;;) {
            if (int32_t(g.u32(grid + 8)) > 0) {
                const uint32_t rec = eax << 7;
                uint32_t edx_j = rec + 0x10;
                uint32_t flags = grid + 0x160, lay = level + 0x1d588, esi = grid + 0x24;
                for (uint32_t j = 0;;) {
                    uint32_t cells = g.u32(grid + 4);
                    x0 = ld(g, lay - 0x78);
                    const X S = x0;
                    const int32_t start = int32_t(g.u32(edx_j + cells - 4)), end = int32_t(g.u32(edx_j + cells));
                    if (start < end) {
                        uint32_t off = uint32_t(start) * 48;
                        for (uint32_t cnt = uint32_t(end - start); cnt != 0; cnt--, off += 0x30) {
                            const uint32_t cellp = rec + g.u32(grid + 4);
                            x3 = D;
                            x2 = ld(g, g.u32(cellp) + off);
                            const X centre = x2;
                            const uint32_t items = g.u32(cellp);
                            x0 = ld(g, items + off + 0x10);
                            const uint32_t item = items + off + 0x10;
                            x0 = shufps(x0, x0, 0); x0 = mulps(x0, S);
                            x1 = addps(x2, x0);
                            x5 = x3;
                            x2 = subps(x2, x0);
                            x0 = ld(g, pos);
                            x5 = addps(x5, x0);
                            x0 = subps(x0, x3);
                            x4 = shufps(x1, x1, 0xaa); x6 = movss(x4, x4);
                            x3 = shufps(x0, x0, 0xaa); x4 = movss(x3, x3); x3 = x4;
                            x6 = cmpltps(x6, x3);
                            if (x6.u[2] + x6.u[1] + x6.u[0] == 0xfffffffdu) continue;
                            x4 = shufps(x2, x2, 0xaa); x6 = movss(x4, x4);
                            x3 = shufps(x5, x5, 0xaa); x4 = movss(x3, x3); x3 = x4;
                            x3 = cmpltps(x3, x6);
                            if (x3.u[2] + x3.u[1] + x3.u[0] == 0xfffffffdu) continue;
                            x3 = movss(shufps(x0, x0, 0), x0);
                            x0 = movss(shufps(x1, x1, 0), x1);
                            x0 = cmpltps(x0, x3);
                            if (x0.u[2] + x0.u[1] + x0.u[0] == 0xfffffffdu) continue;
                            x0 = shufps(x5, x5, 0); x1 = shufps(x2, x2, 0);
                            x0 = movss(x0, x5); x1 = movss(x1, x2);
                            x0 = cmpltps(x0, x1);
                            if (x0.u[2] + x0.u[1] + x0.u[0] == 0xfffffffdu) continue;
                            // falloff vector
                            x1 = ss(g, 0x92e12c); x0 = ss(g, 0x931f24);
                            x1 = shufps(x1, x1, 0); x0 = shufps(x0, x0, 0);
                            x0 = shufps(x0, x1, 0xee); x0 = movss(x0, x1);
                            x1 = mulps(x0, x0);
                            x2 = shufps(x1, x1, 0x99);
                            x1 = addss(x1, x2);
                            x2 = shufps(x2, x2, 0x55);
                            x1 = addss(x1, x2);
                            x2 = ss(g, 0x9322cc);
                            x1 = shufps(x1, x1, 0);
                            x4 = sqrtps(x1);
                            x1 = cmpneqps(x1, ld(g, 0x92dc20));
                            x5 = x1;
                            x1 = ss(g, item + 4);
                            x1 = subss(x1, ss(g, 0x977498));
                            x2 = shufps(x2, x2, 0);
                            x7 = ld(g, 0x935ae0);
                            x3 = X{};
                            X s70 = x3;
                            x1 = shufps(x1, x1, 0);
                            x1 = mulps(x1, x2);
                            x6 = x1;
                            x1 = addps(x1, ld(g, 0x935b00));
                            x2 = subps(x3, x1); x2 = maxps(x2, x1);
                            x1 = mulps(x2, ld(g, 0x935ad0));
                            auto trunc4 = [](X& r) {
                                X t = r; int32_t a = x86::cvtt(t.f(0)), b = x86::cvtt(t.f(1)), c2 = x86::cvtt(t.f(2)), d = x86::cvtt(t.f(3));
                                r = X::fl(float(a), float(b), float(c2), float(d));
                            };
                            trunc4(x1);
                            x1 = mulps(x1, x7);
                            x2 = subps(x2, x1);
                            x1 = subps(ld(g, 0x935af0), x2);
                            x1 = mulps(x1, x1);
                            x3 = subps(x3, x6); x3 = maxps(x3, x6);
                            x2 = mulps(x1, x1);
                            const X s140 = x2;
                            x2 = mulps(x3, ld(g, 0x935ad0));
                            trunc4(x2);
                            x2 = mulps(x2, x7);
                            x3 = subps(x3, x2);
                            x2 = subps(ld(g, 0x935af0), x3);
                            x2 = mulps(x2, x2);
                            x3 = mulps(x2, x2);
                            const X se0 = x3;
                            x6 = mulps(x2, ld(g, 0x935b60)); x6 = addps(x6, ld(g, 0x935b50));
                            const bool flag1 = g.u8(flags + 1) != 0;
                            x6 = mulps(x6, x3);
                            x3 = mulps(x2, ld(g, 0x935b40)); x3 = addps(x3, ld(g, 0x935b30));
                            x6 = addps(x6, x3);
                            x6 = mulps(x6, se0);
                            x3 = ld(g, 0x935b20);
                            x2 = mulps(x2, x3); x2 = addps(x2, ld(g, 0x935b10));
                            x6 = addps(x6, x2);
                            x2 = subps(s70, x6);
                            const uint32_t w = x2.u[3];
                            x0 = divps(x0, x4);
                            x4 = cmpneqps(x4, s70);
                            x2 = mulps(x1, ld(g, 0x935b60)); x2 = addps(x2, ld(g, 0x935b50)); x2 = mulps(x2, s140);
                            x6 = mulps(x1, ld(g, 0x935b40)); x6 = addps(x6, ld(g, 0x935b30));
                            x0 = andps(x0, x4);
                            x2 = addps(x2, x6);
                            x2 = mulps(x2, s140);
                            x0 = andps(x0, x5);
                            x5 = andnps(x5, ld(g, 0x92dc30));
                            x1 = mulps(x1, x3);
                            x1 = addps(x1, ld(g, 0x935b10));
                            x0 = orps(x0, x5);
                            x2 = addps(x2, x1);
                            x2 = mulps(x2, x0);
                            x0 = x2; x0.u[3] = w;
                            auto report = [&](uint32_t obj, uint32_t recv, uint32_t edi) {
                                st(g, P + 0x130, centre);
                                st(g, P + 0x120, x0);
                                flush();
                                m.r[EBX] = pos; m.r[ESI] = esi; m.r[EDI] = item;
                                m.r[EDX] = g.u32(obj);
                                f.call_at(P, g.u32(g.u32(obj) + 0x10), {fcopy32(m, g.u32(item))}, obj, g.u32(g.u32(obj) + 0x10));
                                m.r[EBX] = pos; m.r[ESI] = esi; m.r[EDI] = edi; m.r[EDX] = self;
                                f.call_at(P, 0x0085ddd0, {self, arg2, pos, obj, P + 0x120, P + 0x130, 0}, recv, obj);
                                m.r[EBX] = pos; m.r[ESI] = esi; m.r[EDI] = edi; m.r[EAX] = g.u32(obj); m.r[EDX] = g.u32(g.u32(obj) + 0x14);
                                f.call_at(P, g.u32(g.u32(obj) + 0x14), {}, obj);
                                reload();
                            };
                            if (flag1 && recv1 != 0) {
                                const uint32_t obj = g.u32(esi + 8);
                                report(obj, recv1, item);
                                if (g.u32(recv1 + 0x6a0) != 0) g.u32(out1, g.u32(lay));
                            }
                            if (g.u8(flags) != 0 && recv2 != 0) {
                                const uint32_t obj = g.u32(esi);
                                report(obj, recv2, recv2);
                                if (g.u32(recv2 + 0x6a0) != 0) g.u32(out2, g.u32(lay - 0xc));
                            }
                        }
                    }
                    flags += 2; lay += 0x5f0; j++; edx_j += 4; esi += 0x10;
                    if (!(int32_t(j) < int32_t(g.u32(grid + 8)))) break;
                }
            }
            k++;
            eax = g.u32(list + 4 * k);
            if (eax == 0xffffffffu) break;
        }
        eax = g.u32(grid);
        edx = g.u32(eax + 0x9c);
        g.u32(edx + 0x4c, 0);
    }
    (void)ecx;
    f.restore_saved();
    m.r[EDX] = edx; m.r[ECX] = g.u32(0x9b45e4);
    flush();
    return m.ret(0x18, eax);
}
