// World group, wreck share: the WreckChecker (0x00637xxx): crash detection per rider from the bike's orientation, the rates of its
// angles and the impact forces, choosing a wreck type and handing it to the rider (0x0054e8f0).
#include "world_wreck_util.h"

using namespace world;
using namespace wreck;
using reflex_port::OUT_EAX;
using reflex_port::OUT_ECX;
using reflex_port::OUT_EDX;
using reflex_port::OUT_XMM_ALL;

namespace {

// fstp dword: st(0) to float (control word rounding), popped.
uint32_t fstp32(Machine& m) {
    uint32_t v = reflex_f80::to_f32(m.fpu.back(), frc(m));
    m.fpu.pop_back();
    return v;
}

// 00637ee0 (ECX = wreck type - 1, 0..0x22): the rider reaction class of a wreck type (jump table 0x637f50; EAX = the class).
bool wreck_class(Machine& m, uint32_t idx, uint32_t& out) {
    Guest g{m};
    switch (g.u32(idx * 4 + 0x637f50)) {
        case 0x637ee7: out = 0x11; return true;
        case 0x637eed: out = 0x12; return true;
        case 0x637ef3: out = 0x13; return true;
        case 0x637ef9: out = 0x14; return true;
        case 0x637eff: out = 0xd; return true;
        case 0x637f05: out = 0xe; return true;
        case 0x637f0b: out = 0xf; return true;
        case 0x637f11: out = 0x10; return true;
        case 0x637f17: out = 9; return true;
        case 0x637f1d: out = 0xa; return true;
        case 0x637f23: out = 0xb; return true;
        case 0x637f29: out = 0xc; return true;
        case 0x637f2f: out = 4; return true;
        case 0x637f35: out = 7; return true;
        case 0x637f3b: out = 6; return true;
        case 0x637f41: out = 5; return true;
        case 0x637f47: out = 1; return true;
    }
    return false;
}

// 00637ff0 (ECX = class - 1, 0..0x13; EAX = {int, float} out): the wreck animation of a class and its weight (jump table
// 0x6380fc); xmm0 = the weight (movss).
bool wreck_anim(Machine& m, uint32_t idx, uint32_t out) {
    Guest g{m};
    uint32_t v, c = 0x931f24;
    switch (g.u32(idx * 4 + 0x6380fc)) {
        case 0x637ff7: v = 7; break;
        case 0x63800b: v = 0xa; break;
        case 0x63801f: v = 4; break;
        case 0x638033: v = 1; break;
        case 0x638047: v = 8; break;
        case 0x63805b: v = 0xb; break;
        case 0x63806f: v = 5; break;
        case 0x638083: v = 2; break;
        case 0x638097: v = 9; break;
        case 0x6380ab: v = 0xc; break;
        case 0x6380bf: v = 3; break;
        case 0x6380d3: v = 9; c = 0x9774a4; break;
        case 0x6380e7: v = 6; break;
        default: return false;
    }
    X x0 = ss(g, c);
    g.u32(out, v);
    g.u32(out + 4, x0.u[0]);
    xw(g, 0, x0);
    return true;
}

}  // namespace

REFLEX_PORT_FN(0x00637ee0, WreckChecker_TypeClass, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    uint32_t v;
    if (!wreck_class(m, m.r[ECX], v)) return reflex_vm::RUN_ORIGINAL;
    return m.ret(0, v);
}
REFLEX_PORT_FN(0x00637ff0, WreckChecker_ClassAnim, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    if (!wreck_anim(m, m.r[ECX], m.r[EAX])) return reflex_vm::RUN_ORIGINAL;
    return m.ret(0);
}

// 00638150 (stdcall 12: a, b, c floats): 0x9774a4 when |c| > |a|, else (a - c) / (a - b) * k1 + k0 (0x92ff84, 0x935aa8): how far
// a measure is past its threshold. fabs through 0x00401000.
REFLEX_PORT_FN(0x00638150, WreckChecker_Excess, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Guest g{m};
    const uint32_t E = m.r[ESP], sp = E - 8, A = E + 4, B = E + 8, C = E + 0xc;
    Frame f = Frame::flat(m, 0, 8);
    xw(g, 0, ss(g, C));
    f.call_at(sp, 0x00401000);
    const X c_abs = xr(g, 0);
    g.u32(sp + 4, c_abs.u[0]);
    xw(g, 0, ss(g, A));
    f.call_at(sp, 0x00401000);
    X x0 = xr(g, 0), x1 = ss_bits(c_abs.u[0]);
    if (c_ja(x1.f(0), x0.f(0))) {
        x0 = ss(g, 0x9774a4);
    } else {
        x1 = ss(g, A);
        x0 = x1;
        x0 = subss(x0, ss(g, C));
        x1 = subss(x1, ss(g, B));
        x0 = divss(x0, x1);
        x0 = mulss(x0, ss(g, 0x92ff84));
        x0 = addss(x0, ss(g, 0x935aa8));
    }
    xw(g, 0, x0); xw(g, 1, x1);
    return m.ret(0xc);
}

// 006373e0 (stdcall 8: checker, dt): the wreck check of one rider. Register file kept in m.r (callees take ESI / EDI / EAX / ECX
// inputs: 0x00550650 ESI = rider, 0x00548b70 / 0x00548960 ESI = player index, 0x0054e8f0 ECX = class, EDI = rider, EAX = {anim,
// weight}, 0x0054ed10 EDI = rider), the original's frame at its addresses, xmm registers flushed / reloaded around every call.
REFLEX_PORT_FN(0x006373e0, WreckChecker_Update, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Guest g{m};
    Frame f(m, 0xc4, 3);
    const uint32_t P = f.esp;
    uint32_t& eax = m.r[EAX]; uint32_t& ecx = m.r[ECX]; uint32_t& edx = m.r[EDX];
    uint32_t& ebx = m.r[EBX]; uint32_t& esi = m.r[ESI]; uint32_t& edi = m.r[EDI];
    const uint32_t DT = m.r[ESP] + 8;
    X x0 = xr(g, 0), x1 = xr(g, 1), x2 = xr(g, 2), x3 = xr(g, 3);
    auto flush = [&] { xw(g, 0, x0); xw(g, 1, x1); xw(g, 2, x2); xw(g, 3, x3); };
    auto reload = [&] { x0 = xr(g, 0); x1 = xr(g, 1); x2 = xr(g, 2); x3 = xr(g, 3); };
    auto call = [&](uint32_t addr, std::initializer_list<uint32_t> args = {}) {
        flush();
        uint32_t s = args.size() ? P : P;   // every call site pushes its arguments from ESP = P
        uint32_t keep = m.r[ESP]; m.r[ESP] = s;
        uint32_t a[8]; int n = 0; for (uint32_t v : args) a[n++] = v;
        uint32_t r = m.call_function(addr, a, n, -1, -1);
        m.r[ESP] = keep;
        reload();
        return r;
    };
    // virtual call; how the original leaves EAX / EDX at the call: 'A' edx = vtable, eax = target; 'B' eax = vtable, edx = target;
    // 'C' eax = target, edx = extra; 'D' edx = target, eax = extra.
    auto vcall = [&](uint32_t obj, uint32_t slot, std::initializer_list<uint32_t> args = {}, char mode = 'A', uint32_t extra = 0) {
        ecx = obj;
        const uint32_t vt = g.u32(obj), t = g.u32(vt + slot);
        switch (mode) {
            case 'A': edx = vt; eax = t; break;
            case 'B': eax = vt; edx = t; break;
            case 'C': eax = t; edx = extra; break;
            default: edx = t; eax = extra; break;
        }
        return call(t, args);
    };
    auto rd = [&](uint32_t a) { return ss(g, a); };
    auto wr = [&](uint32_t a, const X& v) { g.u32(a, v.u[0]); };
    auto excess = [&](uint32_t a, uint32_t b, uint32_t c) {   // 0x00638150 with its three stack floats copied through x87
        const uint32_t va = fcopy32(m, g.u32(a)), vb = fcopy32(m, g.u32(b)), vc = fcopy32(m, g.u32(c));
        call(0x00638150, {va, vb, vc});
        x2 = x0;
    };
    ebx = m.arg(0);
    bool flag;
    auto finish = [&] {
        ecx = g.u32(0x9b45e4);
        flush();
        f.restore_saved();
        return m.ret(8);
    };
    if (g.u32(ebx + 4) == 0) return finish();
    eax = g.u32(ebx + 8);
    if (eax == 0) return finish();
    if (g.u32(0xa942b4) == 2) return finish();
    ecx = g.u32(eax + 0x190);
    if (g.u32(ecx + 0x94) == 0) return finish();
    if (g.u32(ecx + 4) == 0) return finish();
    eax = ecx;
    if (vcall(g.u32(eax + 0x94), 0x54) == 0) return finish();
    // impact timers +0x28 (counting down) and +0x24 (decaying force)
    ecx = g.u32(ebx + 8);
    edx = g.u32(ecx + 0x234);
    x0 = rd(ebx + 0x28);
    x2 = rd(DT);
    x1 = X{};
    eax = g.u32(edx + 0x14);
    x0 = subss(x0, x2);
    wr(ebx + 0x28, x0);
    if (c_ja(x1.f(0), x0.f(0))) x0 = x1;
    wr(ebx + 0x28, x0);
    if (eax != 0 && !c_jb(x1.f(0), x0.f(0))) {
        x0 = x2;
        x0 = addss(x0, rd(ebx + 0x24));
    } else {
        x0 = rd(ebx + 0x24);
        x2 = mulss(x2, rd(0x935aa8));
        x0 = subss(x0, x2);
        wr(ebx + 0x24, x0);
        if (c_ja(x1.f(0), x0.f(0))) x0 = x1;
    }
    wr(ebx + 0x24, x0);
    edi = 0x17;
    if (eax != 0) {
        eax = g.u32(g.u32(ebx + 4) + 0x5bab8);
        bool to514 = false;
        if (eax == 1 || eax == 0) {
            x0 = rd(ebx + 0x24);
            if (c_ja(x0.f(0), rd(0x92ff84).f(0))) {
                if (g.u32(ebx + 0x1c) == 0) g.u32(ebx + 0x1c, edi);
                to514 = true;
            }
        }
        if (!to514) {
            x0 = rd(ebx + 0x24);
            if (c_ja(x0.f(0), rd(0x931f24).f(0))) {
                flag = g.u32(0xa942b4) == 2;
                esi = g.u32(ecx + 0x190);
                if (!flag) { call(0x00550650); x1 = X{}; }
                to514 = true;
            }
        }
        if (to514) {
            x0 = rd(0x9322c0);
            wr(ebx + 0x28, x0);
            wr(ebx + 0x24, x1);
        }
    }
    ecx = g.u32(ebx + 8);
    edx = g.u32(ecx + 0x234);
    if (g.u32(edx + 0x18) != 0) {
        eax = g.u32(g.u32(ebx + 4) + 0x5bab8);
        if (eax == 1 || eax == 0) {
            if (g.u32(ebx + 0x1c) == 0) g.u32(ebx + 0x1c, edi);
        } else {
            flag = g.u32(0xa942b4) == 2;
            ecx = g.u32(ecx + 0x190);
            if (!flag) { esi = ecx; call(0x00550650); }
        }
    }
    ecx = g.u32(ebx + 8);
    eax = g.u32(ecx + 0x190);
    esi = g.u32(eax + 0x1f0);
    if (int32_t(esi) >= 0) {
        eax = g.u32(eax + 0x94);
        if (eax != 0 && g.u32(eax + 0x21c) == 2) {
            call(0x00548b70);
            if ((eax & 0xff) != 0) {
                if (g.u32(ebx + 0x18) != 0) g.u32(ebx + 0x18, 0);
                else {
                    ecx = g.u32(0xa93fcc);
                    g.u32(ebx + 0x18, 1);
                    if (g.u32(ecx + 0x298) == 0) {
                        eax = g.u32(ebx + 8);
                        esi = g.u32(eax + 0x190);
                        edx = g.u32(esi + 0x94);
                        if (g.u32(edx + 0xfe8) == 0 && g.u32(ecx + 0x288) == 0) {
                            eax = g.u32(eax + 0x194);
                            if (eax == 0 || eax == 1) {
                                if (g.u32(ebx + 0x1c) == 0) g.u32(ebx + 0x1c, edi);
                            } else if (g.u32(0xa942b4) != 2) call(0x00550650);
                        }
                    }
                }
            } else if (g.u32(ebx + 0x18) != 0) g.u32(ebx + 0x18, 0);
        }
    }
    eax = g.u32(g.u32(ebx + 4) + 0x5bab8);
    if (eax != 1 && eax != 0) {
        if (g.u32(ebx + 0x1c) != 0x16) return finish();
        flag = g.u32(0xa942b4) == 2;
        ecx = g.u32(ebx + 8);
        esi = g.u32(ecx + 0x190);
        if (flag) return finish();
        call(0x00550650);
        return finish();
    }
    // orientation and rates
    vcall(g.u32(ebx + 8), 0x9c, {P + 0xa0}, 'D', P + 0xa0);
    vcall(g.u32(ebx + 8), 0x94, {P + 0xb0}, 'C', P + 0xb0);
    g.u32(P + 0x34, fcopy32(m, g.u32(P + 0xb4)));
    x0 = rd(P + 0xa0);
    ecx = g.u32(ebx + 8); edx = g.u32(ecx); eax = g.u32(edx + 0xe0);
    wr(P + 0x80, x0);
    x0 = rd(P + 0xa4); wr(P + 0x84, x0);
    x0 = rd(P + 0xa8);
    wr(P + 0x88, x0);
    call(eax, {2}); g.u32(P + 0x48, fstp32(m));
    vcall(g.u32(ebx + 8), 0xe0, {0}); g.u32(P + 0x28, fstp32(m));
    vcall(g.u32(ebx + 8), 0xe0, {1}); g.u32(P + 0x58, fstp32(m));
    vcall(g.u32(ebx + 8), 0x94, {P + 0x90}, 'D', P + 0x90);
    esi = g.u32(ebx + 0x10);
    {
        const uint32_t a2 = fcopy32(m, g.u32(esi + 8));
        x1 = rd(esi + 4); x0 = rd(esi);
        const uint32_t a1 = fcopy32(m, g.u32(esi + 0xc)), a0 = fcopy32(m, g.u32(P + 0x34));
        call(0x00424370, {a0, a1, a2});
    }
    // thresholds scaled by the speed factor (cdecl call: ESP = P - 12 while the first eight are stored)
    {
        static const uint32_t dst[] = {0x54, 0x64, 0x50, 0x6c, 0x60, 0x70, 0x4c, 0x7c, 0x5c, 0x78, 0x44, 0x68, 0x40, 0x30, 0x1c, 0x24, 0x20, 0x2c};
        for (int i = 0; i < 18; i++) {
            x1 = rd(esi + 0x18 + 4 * i);
            x1 = mulss(x1, x0);
            wr(P + dst[i], x1);
        }
    }
    ecx = g.u32(ebx + 8); eax = g.u32(ecx); edx = g.u32(eax + 0xe4);
    call(edx, {0}); g.u32(P + 0x18, fstp32(m));
    vcall(g.u32(ebx + 8), 0xe4, {1}, 'B'); g.u32(P + 0x74, fstp32(m));
    x0 = rd(P + 0x18);
    eax = g.u32(ebx + 0x1c);
    x2 = rd(0x931f24);
    x1 = x0;
    x0 = mulss(x0, rd(P + 0x24));
    x1 = mulss(x1, rd(P + 0x1c));
    wr(P + 0x24, x0);
    x0 = rd(P + 0x74);
    wr(P + 0x1c, x1);
    x1 = x0;
    x1 = mulss(x1, rd(P + 0x20));
    x0 = mulss(x0, rd(P + 0x2c));
    esi = 0;
    wr(P + 0x20, x1); wr(P + 0x2c, x0); wr(P + 0x18, x2);
    // choose the wreck type (ESI)
    auto y_low = [&]() {   // the bike's up-vector y below the constant 0x935890 (all three lanes)
        x0 = ld(g, P + 0x90);
        x1 = shufps(x0, x0, 0x55);
        x0 = rd(0x935890); x0 = shufps(x0, x0, 0);
        x1 = cmpltps(x1, x0);
        st(g, P + 0x80, x1);
        return x1.u[2] + x1.u[1] + x1.u[0];
    };
    if (eax != 0) {
        esi = eax;
        eax = g.u32(ebx + 0x20);
        g.u32(ebx + 0x1c, 0);
        if (int32_t(eax) < 0) x2 = rd(0x9322ac);
        else if (eax == 0) { eax = g.u32(ebx + 0x10); x2 = rd(eax + 0x10); x2 = addss(x2, rd(0x97743c)); }
        else if (eax == 1) { ecx = g.u32(ebx + 0x10); x2 = rd(ecx + 0x14); x2 = addss(x2, rd(0x97743c)); }
        else { edx = g.u32(ebx + 0x10); x2 = rd(edx + 0x14); x2 = subss(x2, rd(0x97743c)); }
        g.u32(ebx + 0x20, 0xffffffffu);
        goto d46;
    }
    x0 = rd(P + 0x80);
    if (c_ja(x0.f(0), rd(P + 0x50).f(0))) { esi = 0xb; excess(P + 0x6c, P + 0x50, P + 0x80); goto d46; }
    x3 = rd(P + 0x54);
    if (c_ja(x3.f(0), x0.f(0))) { esi = 0xc; excess(P + 0x64, P + 0x54, P + 0x80); goto d46; }
    x3 = rd(P + 0x60);
    x0 = rd(P + 0x84);
    if (c_ja(x3.f(0), x0.f(0))) {
        if (vcall(g.u32(ebx + 8), 0xb0, {}, 'B') != 0) goto d40;
        esi = eax + 0xf; excess(P + 0x70, P + 0x60, P + 0x84); goto d46;
    }
    if (c_ja(x0.f(0), rd(P + 0x4c).f(0))) {
        if (vcall(g.u32(ebx + 8), 0xb0, {}, 'B') != 0) goto d40;
        esi = eax + 0x10; excess(P + 0x7c, P + 0x4c, P + 0x84); goto d46;
    }
    eax = g.u32(g.u32(ebx + 4) + 0x5bab8);
    if (eax == 1) {
        x0 = rd(P + 0x5c);
        if (c_ja(x0.f(0), rd(P + 0x88).f(0))) { esi = eax + 0xc; excess(P + 0x78, P + 0x5c, P + 0x88); goto d46; }
        x0 = rd(P + 0x88);
        if (c_ja(x0.f(0), rd(P + 0x44).f(0))) { esi = eax + 0xd; excess(P + 0x68, P + 0x44, P + 0x88); goto d46; }
    }
    x0 = rd(P + 0x48);
    if (c_ja(x0.f(0), rd(P + 0x40).f(0))) {
        if (vcall(g.u32(ebx + 8), 0xac) == 0) {
            const uint32_t s = y_low();
            ecx = x1.u[1]; edx = s;
            if (s == 0xfffffffdu) {
                eax = g.u32(0x9cb860);
                g.u32(0x9cb864, g.u32(0x9cb864) + 1);
                eax = eax * 0x41c64e6du + 0x3039u;
                g.u32(0x9cb860, eax);
                eax = (eax >> 16) & 0x7fff;
                eax &= 0x80000003u;
                eax -= 1;
                if (eax <= 3) esi = 0x11 + eax;
                excess(P + 0x30, P + 0x40, P + 0x48);
                goto d46;
            }
        }
        x2 = rd(P + 0x18); x1 = rd(P + 0x20);
    }
    x0 = rd(P + 0x28);
    if (c_ja(x0.f(0), rd(P + 0x1c).f(0))) {
        if (vcall(g.u32(ebx + 8), 0xac, {}, 'B') == 0) {
            const uint32_t s = y_low();
            eax = x1.u[1]; ecx = s;
            if (s == 0xfffffffdu) { esi = ecx + 0x16; excess(P + 0x24, P + 0x1c, P + 0x28); goto d46; }
        }
        x2 = rd(P + 0x18); x1 = rd(P + 0x20);
    }
    x0 = rd(P + 0x58);
    if (c_ja(x0.f(0), x1.f(0))) {
        if (vcall(g.u32(ebx + 8), 0xac) == 0) {
            const uint32_t s = y_low();
            ecx = x1.u[1]; edx = s;
            if (s == 0xfffffffdu) { esi = eax + 0x14; excess(P + 0x2c, P + 0x20, P + 0x58); goto d46; }
        }
        x2 = rd(P + 0x18);
    }
    if (g.u32(0xaf6c00) == esi) goto d46;
    eax = g.u32(g.u32(ebx + 4) + 0x5b540);
    if (int32_t(eax) <= -1) goto d46;
    esi = eax;
    call(0x00548960);
    esi = eax;
d40:
    x2 = rd(P + 0x18);
d46:
    x3 = X{};
    x0 = rd(P + 0x1c);
    eax = lahf_ucomiss(eax, x0.f(0), 0.0f);
    if (x0.f(0) == 0.0f) {
        esi = 0x13;
        excess(P + 0x24, P + 0x1c, P + 0x28);
        x3 = X{};
    } else if (esi == 0) return finish();
    // the class, its animation and weight, scaled by the impact
    ecx = esi - 1;
    if (ecx <= 0x22) {
        eax = 1;
        uint32_t v;
        if (!wreck_class(m, ecx, v)) { flush(); eax = g.call(0x00637ee0, {}, ecx, 1); reload(); }
        else eax = v;
        edi = eax;
    } else edi = 1;
    ecx = edi - 1;
    if (ecx <= 0x13) {
        eax = P + 0x30;
        flush();
        if (!wreck_anim(m, ecx, eax)) g.call(0x00637ff0, {}, ecx, eax);
        reload();
    }
    x1 = rd(P + 0x34);
    if (g.u32(0xaf6c00) == 0) {
        eax = g.u32(ebx + 0x10);
        const float lim = rd(eax + 0x10).f(0);
        x1 = mulss(x1, x2);
        wr(P + 0x34, x1);
        if (c_ja(x2.f(0), lim)) g.u32(P + 0x30, g.u32(P + 0x30) - 2);
        else if (c_ja(x2.f(0), rd(eax + 0x14).f(0))) g.u32(P + 0x30, g.u32(P + 0x30) - 1);
    }
    x0 = rd(0x9774a4);
    if (!c_jb(x0.f(0), x1.f(0))) {
        edx = g.u32(ebx + 8);
        ecx = edi;
        edi = g.u32(edx + 0x190);
        eax = P + 0x30;
        call(0x0054e8f0, {esi});
        eax = g.u32(ebx + 8);
        edi = g.u32(eax + 0x190);
        call(0x0054ed10);
        return finish();
    }
    eax = g.u32(ebx + 8);
    edx = g.u32(eax + 0x190);
    if (!c_jb(x3.f(0), rd(edx + 0xb38).f(0))) {
        ecx = edi;
        eax = P + 0x30;
        edi = edx;
        call(0x0054e8f0, {esi});
        return finish();
    }
    x0 = rd(ebx + 0x14);
    x0 = addss(x0, rd(DT));
    wr(ebx + 0x14, x0);
    ecx = g.u32(eax + 0x190);
    if (esi == g.u32(ecx + 0xb34)) return finish();
    if (g.u32(0xaf6c00) != 0) return finish();
    if (!c_ja(x0.f(0), rd(0x9322c8).f(0))) return finish();
    wr(ebx + 0x14, x3);
    eax = g.u32(eax + 0x190);
    x0 = rd(eax + 0xb38);
    x0 = subss(x0, x1);
    wr(eax + 0xb38, x0);
    if (!c_ja(x3.f(0), x0.f(0))) return finish();
    edi = eax;
    call(0x0054ed10);
    return finish();
}

// 00637320 WreckChecker::vf0 (thiscall, arg = dt; ret 4): the wreck check (0x006373e0) unless the rider is already down
// (+0x280), then the bike's 0x00429910 on +0x5d0.
REFLEX_PORT_FN(0x00637320, WreckChecker_vf0, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Guest g{m};
    const uint32_t E = m.r[ESP], self = m.r[ECX], dt = m.arg(0);
    const uint32_t sp = (((E - 4) & ~7u) - 8);   // push ebp; and esp, -8; push ecx; push esi
    const uint32_t esi0 = m.r[ESI];
    m.r[ESI] = self;
    const uint32_t eax = g.u32(self + 8);
    m.r[EAX] = eax;
    if (eax != 0 && g.u32(eax + 0x280) == 0) {
        Frame f = Frame::flat(m, 0, 0);
        f.call_at(sp, 0x006373e0, {self, fcopy32(m, dt)});
    }
    const uint32_t b = g.u32(self + 4);
    m.r[ESI] = b;
    if (b != 0) {
        Frame f = Frame::flat(m, 0, 0);
        f.call_at(sp, 0x00429910, {}, b + 0x5d0);
    }
    m.r[ESI] = esi0;
    return m.ret(4);
}

// 006367b0 (ECX = wreck state {mode +0, prev +4, ..., value +0x18, delta +0x1c, t +0x20, duration +0x24, rider +0x28},
// xmm2 = target): unless mode 2, and when the rider's controller id (+0x580 -> word) matches +0x5a0: delta = |target - value|;
// a non-zero delta starts a ramp down (value > target: mode 0) or up (mode 1) over delta * 0x9322c8, keeping the previous mode.
REFLEX_PORT_FN(0x006367b0, Fn_006367b0, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Guest g{m};
    const uint32_t c = m.r[ECX];
    uint32_t eax = m.r[EAX];
    const uint32_t edx = g.u32(c);
    m.r[EDX] = edx;
    if (edx == 2) return m.ret(0, eax);
    eax = g.u32(c + 0x28);
    const uint32_t r = g.u32(eax + 0x1b4);
    eax = g.u32(r + 0x580);
    eax = eax ? uint32_t(m.mem.read(eax, 2)) : 0;
    if (uint32_t(m.mem.read(r + 0x5a0, 2)) != (eax & 0xffff)) return m.ret(0, eax);
    X x3 = ss(g, 0x977570), x1 = ss(g, c + 0x18), x2 = xr(g, 2), x0 = x2;
    x0 = subss(x0, x1);
    x0 = andps(x0, x3);
    x3 = X{};
    g.u32(c + 0x1c, x0.u[0]);
    eax = lahf_ucomiss(eax, x0.f(0), 0.0f);
    if (!(x0.f(0) == 0.0f)) {
        if (c_ja(x1.f(0), x2.f(0))) {
            x0 = mulss(x0, ss(g, 0x9322c8));
            g.u32(c + 0x24, x0.u[0]); g.u32(c + 0x20, 0);
            if (edx != 0) g.u32(c + 4, edx);
            g.u32(c, 0);
        } else if (c_ja(x2.f(0), x1.f(0))) {
            x0 = mulss(x0, ss(g, 0x9322c8));
            g.u32(c + 0x24, x0.u[0]); g.u32(c + 0x20, 0);
            if (edx != 1) g.u32(c + 4, edx);
            g.u32(c, 1);
        }
    }
    xw(g, 0, x0); xw(g, 1, x1); xw(g, 3, x3);
    return m.ret(0, eax);
}

// 00636870 (ESI = wreck state, arg = dt; ret 4): advances the ramp: mode 0 lowers value by delta * t / duration (floor 0),
// mode 1 raises it (cap 0x931f24) and ends when the rider's 0x00545340 is 0, mode 2 decays it by dt * 0x92fe4c (when +0x14) and
// ends at 0 (prev = 2, mode 0). Nothing while the rider has no player (+0x1f0 = -1).
REFLEX_PORT_FN(0x00636870, Fn_00636870, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Guest g{m};
    const uint32_t s = m.r[ESI], dtp = m.r[ESP] + 4;
    uint32_t ecx = g.u32(s + 0x28), eax = m.r[EAX];
    m.r[ECX] = ecx;
    if (g.u32(ecx + 0x1f0) == 0xffffffffu) return m.ret(4, eax);
    eax = g.u32(s);
    X x0 = xr(g, 0), x1 = xr(g, 1), x2 = xr(g, 2);
    if (eax == 0 || eax == 1) {
        x0 = ss(g, s + 0x20);
        x0 = addss(x0, ss(g, dtp));
        x1 = ss(g, s + 0x24);
        if (!c_ja(x1.f(0), x0.f(0))) x0 = x1;
        g.u32(s + 0x20, x0.u[0]);
        x0 = divss(x0, x1);
        x0 = mulss(x0, ss(g, s + 0x1c));
        x1 = ss(g, s + 0x18);
        if (eax == 0) {
            x1 = subss(x1, x0);
            x0 = X{};
            if (c_ja(x1.f(0), x0.f(0))) x0 = x1;
            g.u32(s + 0x18, x0.u[0]);
            xw(g, 0, x0); xw(g, 1, x1);
            return m.ret(4, 0);
        }
        x1 = addss(x1, x0);
        x0 = ss(g, 0x931f24);
        if (c_ja(x0.f(0), x1.f(0))) x0 = x1;
        g.u32(s + 0x18, x0.u[0]);
        xw(g, 0, x0); xw(g, 1, x1);
        m.r[EAX] = 0;
        eax = g.call(0x00545340, {}, g.u32(ecx + 0x1b4));
        if (eax == 0) { g.u32(s + 4, 1); g.u32(s, eax); }
        return m.ret(4, eax);
    }
    if (eax != 2) return m.ret(4, eax - 2);
    eax = 0;
    x1 = X{};
    if (g.u32(s + 0x14) != 0) {
        x2 = ss(g, dtp);
        x0 = ss(g, s + 0x18);
        x2 = mulss(x2, ss(g, 0x92fe4c));
        x0 = subss(x0, x2);
        g.u32(s + 0x18, x0.u[0]);
        if (!c_ja(x0.f(0), x1.f(0))) x0 = x1;
        g.u32(s + 0x18, x0.u[0]);
    }
    x0 = ss(g, s + 0x18);
    eax = lahf_ucomiss(eax, x0.f(0), x1.f(0));
    if (x0.f(0) == 0.0f) { g.u32(s + 4, 2); g.u32(s, 0); }
    xw(g, 0, x0); xw(g, 1, x1); xw(g, 2, x2);
    return m.ret(4, eax);
}

// 00632aa0 (ECX = wreck state, ESI = rider): in modes 1 / 2, plays reaction 3 (prev mode 1) or 2 through 0x00623960 /
// 0x006235e0 (weight 0x92ff84).
REFLEX_PORT_FN(0x00632aa0, Fn_00632aa0, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    Guest g{m};
    const uint32_t c = m.r[ECX], rider = m.r[ESI];
    uint32_t eax = g.u32(c) - 1;
    if (eax > 1) return m.ret(0, eax);
    const uint32_t k = g.u32(c + 4) == 1 ? 3 : 2;
    eax = g.call(0x00623960, {}, k, rider);
    if (eax == 0) return m.ret(0, eax);
    eax = g.call(0x006235e0, {fcopy32(m, g.u32(0x92ff84)), 0}, rider, k);
    return m.ret(0, eax);
}
