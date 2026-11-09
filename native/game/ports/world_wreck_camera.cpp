// World group, wreck share: MXThirdPersonCamera impulses (0x00649bc0, 0x006467f0, 0x00651000-0x00653300): the camera's reaction
// to bike impacts (shake / kick vectors from tuned curves 0x00438ee0 per camera mode and rider class). Locals that overlap or
// that callees read are kept at the original's stack addresses (F = the aligned frame base).
#include "world_wreck_util.h"

using namespace world;
using namespace wreck;
using reflex_port::OUT_EAX;
using reflex_port::OUT_ECX;
using reflex_port::OUT_EDX;
using reflex_port::OUT_XMM_ALL;

// 00649bc0 MXThirdPersonCamera::vf22: the type id 0x1003.
REFLEX_PORT_FN(0x00649bc0, MXThirdPersonCamera_vf22, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    return m.ret(0, 0x1003);
}

// 006467f0 (EAX = camera): the camera mode index 0..5 of its mode name (+0x2d8) against six names (0x0091610c compare), else 0.
REFLEX_PORT_FN(0x006467f0, Camera_ModeIndex, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    Regs r(m);
    const uint32_t sp = r.esp - 12;
    r.edi = r.u32(0x91610c);
    r.esi = r.eax + 0x2d8;
    r.ebx = 0;
    static const uint32_t names[6] = {0x958980, 0x9589b8, 0x941b28, 0x958a60, 0x958a6c, 0x958a78};
    for (uint32_t i = 0; i < 6; i++) {
        r.call(sp, r.edi, {r.esi, names[i]});
        const bool hit = (r.eax & 0xff) != 0;
        if (i == 5) { r.eax = hit ? 5 : r.ebx; break; }
        if (hit) { r.eax = i; break; }
    }
    return r.ret(0);
}

namespace {

// The impact direction test shared by 0x006510a0 / 0x00653300: v (x at a, y at a + 4, z at a + 8 of the frame copy) against the
// camera's forward / side axes (+0xb0.. / +0xc0..); +0x6b4 = 0, or the side kick 0x977520 / 0x977508.
void side_kick(Regs& r, uint32_t cam, uint32_t vx, uint32_t vy, uint32_t vz) {
    X* x = r.x;
    x[6] = r.ss(cam + 0xc4); x[3] = r.ss(cam + 0xc0); x[4] = r.ss(cam + 0xb0); x[5] = r.ss(cam + 0xb4); x[7] = r.ss(cam + 0xc8);
    x[1] = r.ss(vy); x[2] = r.ss(vx);
    x[0] = x[1]; x[0] = mulss(x[0], x[6]);
    x[6] = x[2]; x[6] = mulss(x[6], x[3]);
    x[3] = r.ss(vz);
    x[2] = mulss(x[2], x[4]);
    x[0] = addss(x[0], x[6]);
    x[1] = mulss(x[1], x[5]);
    x[1] = addss(x[1], x[2]);
    x[2] = r.ss(cam + 0xb8);
    x[6] = x[3]; x[3] = mulss(x[3], x[2]);
    x[2] = r.ss(0x9774a8);
    x[1] = addss(x[1], x[3]);
    x[3] = X{};
    x[6] = mulss(x[6], x[7]);
    x[0] = addss(x[0], x[6]);
    const bool a = c_ja(x[1].f(0), x[0].f(0));
    x[2] = subss(x[2], x[1]);
    r.w32(cam + 0x6b4, x[3].u[0]);
    if (a) {
        if (c_ja(x[2].f(0), x[1].f(0))) x[0] = r.ss(0x977508);
        else x[0] = r.ss(0x977520);
        r.w32(cam + 0x6b4, x[0].u[0]);
    } else if (c_ja(x[2].f(0), x[0].f(0))) {
        x[0] = r.ss(0x977508);
        r.w32(cam + 0x6b4, x[0].u[0]);
    }
}

// The curve table of a camera / rider class (ESI for 0x00438ee0): base + class * 0x1da80 + (mode * 2 + split) * 0xa0, with
// class 3 (+0x194) mapped by +0x170 to 3 / 8, split = (0xa946b8 +0x15b24 == 2).
void curve_regs(Regs& r, uint32_t cam, uint32_t base) {
    r.eax = r.u32(r.u32(r.edi + 0x344) + 0x1ac);
    r.esi = r.u32(r.eax + 0x194);
    if (r.esi == 3) { r.esi = r.u32(r.eax + 0x170); r.esi = (r.esi - 8) != 0 ? 8u : 3u; }
    r.ecx = r.u32(0xa946b8);
    r.eax = r.u32(r.ecx + 0x15b24);
    r.eax -= 1;
    if (r.eax == 0) r.ebx = 0;
    else { r.eax -= 1; r.ebx = r.eax == 0 ? 1 : 0; }
    (void)cam; (void)base;
}
uint32_t curve_eval(Regs& r, uint32_t sp, uint32_t base, uint32_t mag_addr) {
    r.eax = r.edi;
    r.call(sp, 0x006467f0);
    r.esi = r.esi * 0x1da80;
    r.x[0] = r.ss(mag_addr);
    r.eax = r.ebx + r.eax * 2;
    r.edx = (r.eax * 5) << 5;
    r.esi = r.edx + r.esi + base;
    r.call(sp, 0x00438ee0);
    return r.x[0].u[0];
}
// |v| splat (the exe's dot / shufps / sqrtps sequence on xmm0 / xmm1 from xmm2 = v)
void length3(X* x, const X& v) {
    x[2] = v;
    x[0] = mulps(x[2], x[2]);
    x[1] = shufps(x[0], x[0], 0x66);
    x[0] = addss(x[0], x[1]);
    x[1] = shufps(x[1], x[1], 0x55);
    x[0] = addss(x[0], x[1]);
}

}  // namespace

// 006510a0 (EDI = camera, stdcall 4: impact vector; returns AL): within the shake band (+0x60c .. +0x610): side kick, two curves of
// |v| (0xb0bfc4 / 0xb0c884 tables) scale the camera's shake vectors (+0x630 / +0x640 / +0x650), started by 0x006516a0.
REFLEX_PORT_FN(0x006510a0, Camera_ImpactShake, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Regs r(m);
    X* x = r.x;
    const uint32_t E = r.esp, F = ((E - 4) & ~15u) - 0x48, sp = F - 8;
    const uint32_t cam = r.edi;
    r.eax = m.arg(0);
    length3(x, r.ld(r.eax));
    x[1] = r.ss(cam + 0x60c);
    x[0] = shufps(x[0], x[0], 0);
    x[0] = sqrtps(x[0]);
    r.st(F + 0x18, x[0]);
    x[0] = r.ss(F + 0x18);
    auto fail = [&] { setlo8(r.eax, 0); r.ecx = r.u32(0x9b45e4); return r.ret(4); };
    if (c_ja(x[1].f(0), x[0].f(0))) return fail();
    if (c_ja(x[0].f(0), r.ss(cam + 0x610).f(0))) return fail();
    r.st(F + 8, x[2]);
    side_kick(r, cam, F + 8, F + 0xc, F + 0x10);
    curve_regs(r, cam, 0);
    curve_eval(r, sp, 0xb0bfc4, F + 0x18);
    curve_regs(r, cam, 0);
    x[0] = shufps(x[0], x[0], 0);
    r.eax = r.edi;
    r.st(F + 8, x[0]);
    curve_eval(r, sp, 0xb0c884, F + 0x18);
    x[1] = r.ld(F + 8);
    const uint32_t f614 = fcopy32(m, r.u32(cam + 0x614));
    x[2] = mulps(r.ld(cam + 0x640), x[1]);
    r.st(F + 0x28, x[2]);
    x[2] = mulps(r.ld(cam + 0x630), x[1]);
    x[1] = r.ld(cam + 0x650);
    r.eax = F + 0x18;
    x[0] = shufps(x[0], x[0], 0);
    x[1] = mulps(x[1], x[0]);
    r.edx = F + 8; r.ecx = F + 0x28;
    r.st(F + 8, x[2]); r.st(F + 0x18, x[1]);
    r.w8(cam + 0x700, 0);
    r.call(sp, 0x006516a0, {cam, F + 0x18, f614});
    r.ecx = r.u32(0x9b45e4);
    setlo8(r.eax, 1);
    return r.ret(4);
}

// 00651320 (EDI = camera, stdcall 4: impact vector; returns AL): within the kick band (+0x61c .. +0x620): two curves of |v|
// (0xb0f0c4 / 0xb0f984) scale the kick vectors (+0x660 / +0x670), started by 0x00651940 with duration +0x624.
REFLEX_PORT_FN(0x00651320, Camera_ImpactKick, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Regs r(m);
    X* x = r.x;
    const uint32_t E = r.esp, F = ((E - 4) & ~15u) - 0x38, sp = F - 8;
    const uint32_t cam = r.edi;
    r.eax = m.arg(0);
    x[1] = r.ld(r.eax);
    x[1] = mulps(x[1], x[1]);
    x[0] = shufps(x[1], x[1], 0x66);
    x[1] = addss(x[1], x[0]);
    x[0] = shufps(x[0], x[0], 0x55);
    x[1] = addss(x[1], x[0]);
    x[0] = x[1];
    x[1] = r.ss(cam + 0x61c);
    x[0] = shufps(x[0], x[0], 0);
    x[0] = sqrtps(x[0]);
    r.st(F + 8, x[0]);
    x[0] = r.ss(F + 8);
    auto fail = [&] { setlo8(r.eax, 0); r.ecx = r.u32(0x9b45e4); return r.ret(4); };
    if (c_ja(x[1].f(0), x[0].f(0))) return fail();
    if (c_ja(x[0].f(0), r.ss(cam + 0x620).f(0))) return fail();
    curve_regs(r, cam, 0);
    curve_eval(r, sp, 0xb0f0c4, F + 8);
    r.eax = r.u32(r.u32(r.edi + 0x344) + 0x1ac);
    r.w32(F + 4, x[0].u[0]);
    curve_regs(r, cam, 0);
    curve_eval(r, sp, 0xb0f984, F + 8);
    const uint32_t f624 = fcopy32(m, r.u32(cam + 0x624));
    x[1] = r.ld(cam + 0x670);
    r.w32(F, x[0].u[0]);
    x[0] = r.ss(F + 4);
    const uint32_t c2 = fcopy32(m, r.u32(F));
    x[0] = shufps(x[0], x[0], 0);
    x[1] = mulps(x[1], x[0]);
    r.eax = F + 8;
    r.st(F + 0x18, x[1]);
    x[1] = r.ld(cam + 0x660);
    x[1] = mulps(x[1], x[0]);
    r.edx = F + 0x18;
    r.st(F + 8, x[1]);
    r.call(sp, 0x00651940, {cam, F + 8, c2, f624});
    r.ecx = r.u32(0x9b45e4);
    setlo8(r.eax, 1);
    return r.ret(4);
}

// 00651940 (stdcall 16: camera, kick vector*, strength, duration; EDX = second vector*): arms the kick (+0x700, +0x6a0,
// +0x710 / +0x720 vectors, +0x6a4 duration, +0x6a8 = 0, +0x708 strength).
REFLEX_PORT_FN(0x00651940, Camera_StartKick, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Regs r(m);
    const uint32_t E = r.esp;
    r.eax = m.arg(0);
    setlo8(r.ecx, 1);
    r.w8(r.eax + 0x700, 1); r.w8(r.eax + 0x6a0, 1);
    r.ecx = m.arg(1);
    r.st(r.eax + 0x710, r.ld(r.ecx));
    r.st(r.eax + 0x720, r.ld(r.edx));
    r.w32(r.eax + 0x6a4, r.u32(E + 0x10));
    r.w32(r.eax + 0x6a8, 0);
    r.x[0] = r.ss(E + 0xc);
    r.w32(r.eax + 0x708, r.x[0].u[0]);
    return r.ret(0x10);
}

// 006516a0 (stdcall 12: camera, scale vector*, duration; ECX / EDX = the two shake vectors): arms the shake (+0x6c0 / +0x6d0 /
// +0x6e0 = scale * 0x977720) and its two phase amplitudes +0x6b0 / +0x6ac: a cos-like polynomial of the sum's x / y reduced into
// one period (sign kept), divided by the matching sine-like polynomial.
REFLEX_PORT_FN(0x006516a0, Camera_StartShake, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Regs r(m);
    X* x = r.x;
    const uint32_t E = r.esp;
    auto C = [&](uint32_t a) { return r.ss(a); };
    r.eax = m.arg(0);
    x[2] = X{};
    r.w8(r.eax + 0x700, 0); r.w8(r.eax + 0x6a0, 1);
    x[0] = r.ld(r.ecx);
    r.ecx = m.arg(1);
    r.st(r.eax + 0x6c0, x[0]);
    x[0] = r.ld(r.edx);
    r.st(r.eax + 0x6d0, x[0]);
    x[0] = r.ss(E + 0xc);
    r.w32(r.eax + 0x6a4, x[0].u[0]);
    x[0] = C(0x977720);
    r.w32(r.eax + 0x6a8, x[2].u[0]);
    x[1] = r.ld(r.ecx);
    x[0] = shufps(x[0], x[0], 0);
    x[0] = mulps(x[0], x[1]);
    r.st(r.eax + 0x6e0, x[0]);
    x[1] = r.ld(r.eax + 0x6c0);
    x[0] = r.ld(r.eax + 0x6d0);
    x[0] = addps(x[0], x[1]);
    const X sum = x[0];
    x[1] = ss_bits(sum.u[0]);
    x[0] = c_jb(x[1].f(0), x[2].f(0)) ? C(0x9cd9e8) : C(0x931f24);
    x[4] = C(0x977498);
    const X sgn = x[0];
    x[0] = mulss(x[0], x[1]);
    x[1] = mulss(x[0], C(0x9774a0));
    r.edx = uint32_t(x86::cvtt(x[1].f(0)));
    x[1] = cvtsi2ss(x[1], int32_t(r.edx));
    x[1] = mulss(x[1], C(0x97749c));
    x[0] = subss(x[0], x[1]);
    x[2] = subss(x[4], x[0]);
    x[0] = mulss(x[2], x[2]);
    x[5] = mulss(x[0], C(0x935d50)); x[5] = addss(x[5], C(0x935d48));
    x[1] = mulss(x[0], x[0]);
    x[5] = mulss(x[5], x[1]);
    x[3] = mulss(x[0], C(0x935d44)); x[3] = addss(x[3], C(0x935d4c));
    x[5] = addss(x[5], x[3]);
    x[3] = mulss(x[0], C(0x935d40)); x[3] = addss(x[3], C(0x935d3c));
    x[5] = mulss(x[5], x[1]);
    x[5] = addss(x[5], x[3]);
    x[3] = C(0x9774a8);
    x[6] = mulss(x[0], C(0x935d68)); x[6] = addss(x[6], C(0x935d64)); x[6] = mulss(x[6], x[1]);
    x[7] = mulss(x[0], C(0x935d60));
    x[0] = mulss(x[0], C(0x935d58)); x[0] = addss(x[0], C(0x935d54));
    x[3] = subss(x[3], x[5]);
    x[5] = C(0x935d5c);
    x[7] = addss(x[7], x[5]);
    x[6] = addss(x[6], x[7]);
    x[6] = mulss(x[6], x[1]);
    x[1] = ss_bits(sum.u[1]);
    const bool neg = c_jb(x[1].f(0), C(0x92e12c).f(0));
    x[6] = addss(x[6], x[0]); x[6] = mulss(x[6], x[2]); x[6] = mulss(x[6], sgn); x[6] = divss(x[6], x[3]);
    r.w32(r.eax + 0x6b0, x[6].u[0]);
    x[3] = neg ? C(0x9cd9e8) : C(0x931f24);
    x[0] = mulss(x[3], x[1]);
    x[1] = mulss(x[0], C(0x9774a0));
    r.ecx = uint32_t(x86::cvtt(x[1].f(0)));
    x[1] = cvtsi2ss(x[1], int32_t(r.ecx));
    x[1] = mulss(x[1], C(0x97749c));
    x[0] = subss(x[0], x[1]);
    x[4] = subss(x[4], x[0]);
    x[2] = x[4];
    x[0] = mulss(x[2], x[2]);
    x[6] = mulss(x[0], C(0x935d50)); x[6] = addss(x[6], C(0x935d48));
    x[1] = mulss(x[0], x[0]);
    x[6] = mulss(x[6], x[1]);
    x[4] = mulss(x[0], C(0x935d44)); x[4] = addss(x[4], C(0x935d4c));
    x[6] = addss(x[6], x[4]);
    x[6] = mulss(x[6], x[1]);
    x[4] = mulss(x[0], C(0x935d40)); x[4] = addss(x[4], C(0x935d3c));
    x[6] = addss(x[6], x[4]);
    x[4] = C(0x9774a8); x[4] = subss(x[4], x[6]);
    x[6] = mulss(x[0], C(0x935d68)); x[6] = addss(x[6], C(0x935d64)); x[6] = mulss(x[6], x[1]);
    x[7] = mulss(x[0], C(0x935d60));
    x[0] = mulss(x[0], C(0x935d58)); x[0] = addss(x[0], C(0x935d54));
    x[7] = addss(x[7], x[5]);
    x[6] = addss(x[6], x[7]); x[6] = mulss(x[6], x[1]); x[6] = addss(x[6], x[0]); x[6] = mulss(x[6], x[2]); x[6] = mulss(x[6], x[3]);
    x[6] = divss(x[6], x[4]);
    r.w32(r.eax + 0x6ac, x[6].u[0]);
    return r.ret(0xc);
}

// 00653300 MXThirdPersonCamera::vf34 (thiscall, stdcall 8: impact vector*, duration): side kick and a shake from three fixed
// ramps of |v| (0x00424370), started by 0x006516a0.
REFLEX_PORT_FN(0x00653300, MXThirdPersonCamera_vf34, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Regs r(m);
    X* x = r.x;
    const uint32_t E = r.esp, F = ((E - 4) & ~15u) - 0x4c, sp = F - 4;
    r.eax = m.arg(0);
    length3(x, r.ld(r.eax));
    r.st(F + 0x1c, x[2]);
    x[1] = r.ss(F + 0x20); x[2] = r.ss(F + 0x1c);
    x[0] = shufps(x[0], x[0], 0);
    x[0] = sqrtps(x[0]);
    r.st(F + 0xc, x[0]);
    r.esi = r.ecx;
    const uint32_t cam = r.esi;
    side_kick(r, cam, F + 0x1c, F + 0x20, F + 0x24);
    const uint32_t mag = fcopy32(m, r.u32(F + 0xc));
    auto ramp = [&](uint32_t hi, uint32_t lo) {
        const uint32_t a2 = fcopy32(m, r.u32(hi)), a1 = fcopy32(m, r.u32(lo));
        x[1] = r.ss(0x931f2c);
        r.call(sp, 0x00424370, {mag, a1, a2});
    };
    x[0] = r.ss(0x92fd88);
    ramp(0x931da8, 0x9322c8);
    x[0] = shufps(x[0], x[0], 0);
    r.st(F + 0x2c, x[0]);
    x[0] = r.ss(0x92fd88);
    ramp(0x931d94, 0x9cf5a8);
    x[0] = shufps(x[0], x[0], 0);
    r.st(F + 0x1c, x[0]);
    x[0] = r.ss(0x92fd88);
    ramp(0x9774d4, 0x9322d0);
    const uint32_t dur = fcopy32(m, r.u32(E + 8));
    r.eax = F + 0xc;
    r.edx = F + 0x1c;
    x[0] = shufps(x[0], x[0], 0);
    r.ecx = F + 0x2c;
    r.st(F + 0xc, x[0]);
    r.w8(cam + 0x700, 0);
    r.call(sp, 0x006516a0, {cam, F + 0xc, dur});
    r.ecx = r.u32(0x9b45e4);
    return r.ret(8);
}

// 00651000 MXThirdPersonCamera::vf33 (thiscall, stdcall 16: ?, shake vector*, impact vector*, no-kick flag): unless the rider is
// down: a shake (0x006510a0, when enabled +0x608 and armed +0x392), else a kick (0x00651320, +0x618) or the fallback
// 0x006514e0 (+0x5f8); +0x392 = the impact points down (y < 0).
REFLEX_PORT_FN(0x00651000, MXThirdPersonCamera_vf33, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Regs r(m);
    const uint32_t E = r.esp, F = ((E - 4) & ~15u) - 0x14, P = F - 12;
    r.edi = r.ecx;
    r.eax = r.u32(r.edi + 0x344);
    r.ecx = r.u32(r.eax + 0x1ac);
    if (r.u32(r.ecx + 0x280) != 0) return r.ret(0x10);
    r.esi = m.arg(2);
    r.x[0] = r.ld(r.esi);
    r.st(P + 0x10, r.x[0]);
    r.x[0] = X{};
    setlo8(r.ebx, 0);
    if (c_ja(r.x[0].f(0), r.ss(P + 0x14).f(0))) setlo8(r.ebx, 1);
    bool done = false;
    if (r.u8(r.edi + 0x608) != 0 && r.u8(r.edi + 0x392) != 0) {
        r.edx = m.arg(1);
        r.call(P, 0x006510a0, {r.edx});
        done = (r.eax & 0xff) != 0;
    }
    if (!done && (m.arg(3) & 0xff) == 0) {
        if (r.u8(r.edi + 0x618) != 0) { r.call(P, 0x00651320, {r.esi}); done = (r.eax & 0xff) != 0; }
        if (!done && r.u8(r.edi + 0x5f8) != 0) r.call(P, 0x006514e0, {r.esi});
    }
    r.w8(r.edi + 0x392, uint8_t(r.ebx));
    return r.ret(0x10);
}
