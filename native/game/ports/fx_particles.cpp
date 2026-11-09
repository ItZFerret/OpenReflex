// Particle / world-entity ports (group "fx"), transliterated instruction by instruction from MXReflex.exe by the fx
// lifter (a copy of the bike lifter, see native/game/notes/fx.md). Each function keeps the original's operations, order and
// memory traffic (stack frame included).
#include <stdexcept>
#include <utility>
#include "lift_rt.h"

// 00691ba0 PlatformParticleManager2D__vf7
//   original PlatformParticleManager2D::vf7; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x00691ba0, PlatformParticleManager2D__vf7, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_691ba2 = 0;
    uint32_t fB_691ba2 = 0;
    eax = 0u;
    fA_691ba2 = rd32(m, ecx + 0xa8u);
    fB_691ba2 = 0x1u;
    eax = (eax & 0xffffff00u) | (((fA_691ba2 != fB_691ba2) ? 1u : 0u) & 0xffu);
    LIFT_RET(0);
}

// 00691bb0 ParticleEntity__vf22
//   original ParticleEntity::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00691bb0, ParticleEntity__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x1030u;
    LIFT_RET(0);
}

// 00691bd0 Fn_00691bd0
//   original FUN_00691bd0; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00691bd0, Fn_00691bd0, "fx", reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fR_691beb = 0;
    uint32_t fA_691bf4 = 0;
    uint32_t fB_691bf4 = 0;
    uint32_t fR_691c03 = 0;
    uint32_t fA_691c1f = 0;
    uint32_t fB_691c1f = 0;
    eax = rd32(m, 0xa95844u);
    ecx = rd32(m, eax + 0x4u);
    edx = rd32(m, eax + ecx * 4 + 0x1cu);
    eax = rd32(m, edx + 0x2cc4u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, eax * 4 + 0xa94264u);
    fR_691beb = (edi & edi);
    if ((fR_691beb == 0)) goto L_691c27;
    esi = 0xc11e90u;
L_691bf4:
    fA_691bf4 = rd32(m, esi + 0x220u);
    fB_691bf4 = edi;
    wr32(m, esi + 0x220u, edi);
    eax = (eax & 0xffffff00u) | (((fA_691bf4 != fB_691bf4) ? 1u : 0u) & 0xffu);
    fR_691c03 = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_691c03 == 0)) goto L_691c0c;
    LIFT_CALL(0x697870u, 0x691c0cu);  // FUN_00697870
L_691c0c:
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0xcu)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x697a00u, 0x691c19u);  // FUN_00697a00
    esi = esi + 0x17c30u;
    fA_691c1f = esi;
    fB_691c1f = 0xd2f0d0u;
    if (int32_t(sext(fA_691c1f, 4)) < int32_t(sext(fB_691c1f, 4))) goto L_691bf4;
L_691c27:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00691d80 rtti_shared_vf_00691d80_00691d80
//   original rtti_shared_vf_00691d80; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x00691d80, rtti_shared_vf_00691d80_00691d80, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = (ecx + 0x10u);
    LIFT_RET(0);
}

// 00691d90 rtti_shared_vf_00691d90_00691d90
//   original rtti_shared_vf_00691d90; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x00691d90, rtti_shared_vf_00691d90_00691d90, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fR_691d94 = 0;
    uint32_t fA_691d98 = 0;
    uint32_t fB_691d98 = 0;
    eax = rd32(m, esp + 0x4u);
    fR_691d94 = (eax & eax);
    if ((fR_691d94 == 0)) goto L_691dab;
    fA_691d98 = rd32(m, ecx + 0x2cu);
    fB_691d98 = 0x0u;
    wr32(m, ecx + 0x2cu, eax);
    if (fA_691d98 != fB_691d98) goto L_691dae;
    wr32(m, ecx + 0x30u, 0x1u);
    LIFT_RET(4);
L_691dab:
    wr32(m, ecx + 0x2cu, eax);
L_691dae:
    LIFT_RET(4);
}

// 00692110 PlaceableParticleEffect__vf22
//   original PlaceableParticleEffect::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00692110, PlaceableParticleEffect__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x102au;
    LIFT_RET(0);
}

// 00692120 FireworksParticleEffect__vf22
//   original FireworksParticleEffect::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00692120, FireworksParticleEffect__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x102eu;
    LIFT_RET(0);
}

// 00692130 TriggeredPlaceableParticleEffect__vf22
//   original TriggeredPlaceableParticleEffect::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00692130, TriggeredPlaceableParticleEffect__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x102fu;
    LIFT_RET(0);
}

// 006922c0 Fn_006922c0
//   original FUN_006922c0; register inputs: esi; returns with ret 4
REFLEX_PORT_FN(0x006922c0, Fn_006922c0, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    FCmp fC_6922c6{};
    x0 = movss_m(rd32(m, esp + 0x4u));
    fC_6922c6 = fcmp32(x0.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    wr32(m, esi + 0xb0u, x0.l(0));
    wr32(m, esi + 0x2a0u, x0.l(0));
    if ((fc_b(fC_6922c6) || fc_z(fC_6922c6))) goto L_6922f8;
    eax = rd32(m, esi + 0x90u);
    edx = rd32(m, eax + 0x14u);
    ecx = (esi + 0x90u);
    esp -= 4; wr32(m, esp, 0x1u);
    LIFT_CALL(edx, 0x6922f2u);
    x0 = movss_m(rd32(m, esp + 0x4u));
L_6922f8:
    eax = rd32(m, esi + 0xb8u);
    ecx = rd32(m, esi + 0xb4u);
    eax = uint32_t(int32_t(eax) * int32_t(0x1f0u));
    edx = rd32(m, ecx + 0x44u);
    wr32(m, ecx + 0x4cu, 0x1u);
    eax = eax + rd32(m, edx + 0x10u);
    x1 = movss_m(rd32(m, eax + 0xc8u));
    x1 = addss(x1, rd32(m, eax + 0xc4u));
    x1 = addss(x1, x0.l(0));
    wr32(m, esi + 0x2a8u, x1.l(0));
    wr32(m, ecx + 0x4cu, 0x0u);
    LIFT_RET(4);
}

// 00692340 FireworksParticleEffect__vf19
//   original FireworksParticleEffect::vf19; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x00692340, FireworksParticleEffect__vf19, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_692343 = 0;
    uint32_t fB_692343 = 0;
    FCmp fC_692354{};
    FCmp fC_692389{};
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    fA_692343 = rd32(m, esi + 0xbcu);
    fB_692343 = 0x0u;
    if (fA_692343 == fB_692343) goto L_6923b8;
    x0 = movss_m(rd32(m, esi + 0x2a0u));
    fC_692354 = fcmp32(x0.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    if ((fc_b(fC_692354) || fc_z(fC_692354))) goto L_692378;
    x0 = subss(x0, rd32(m, esp + 0x8u));
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x8u)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    wr32(m, esi + 0x2a0u, x0.l(0));
    LIFT_CALL(0x696940u, 0x692378u);  // PlaceableParticleEffect::vf19
L_692378:
    x0 = movss_m(rd32(m, esi + 0x2a8u));
    x0 = subss(x0, rd32(m, esp + 0x8u));
    x1 = X{};
    fC_692389 = fcmp32(x1.l(0), x0.l(0));
    wr32(m, esi + 0x2a8u, x0.l(0));
    if (fc_b(fC_692389)) goto L_6923b8;
    eax = rd32(m, esi + 0x90u);
    edx = rd32(m, eax + 0x14u);
    ecx = (esi + 0x90u);
    esp -= 4; wr32(m, esp, 0x0u);
    LIFT_CALL(edx, 0x6923a9u);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x8u)));
    esp -= 4; wr32(m, esp, ecx);
    ecx = esi;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x696940u, 0x6923b8u);  // PlaceableParticleEffect::vf19
L_6923b8:
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00692520 TriggeredPlaceableParticleEffect__vf19
//   original TriggeredPlaceableParticleEffect::vf19; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x00692520, TriggeredPlaceableParticleEffect__vf19, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_692523 = 0;
    uint32_t fB_692523 = 0;
    FCmp fC_692534{};
    FCmp fC_692569{};
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    fA_692523 = rd32(m, esi + 0xbcu);
    fB_692523 = 0x0u;
    if (fA_692523 == fB_692523) goto L_692598;
    x0 = movss_m(rd32(m, esi + 0x2a0u));
    fC_692534 = fcmp32(x0.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    if ((fc_b(fC_692534) || fc_z(fC_692534))) goto L_692558;
    x0 = subss(x0, rd32(m, esp + 0x8u));
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x8u)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    wr32(m, esi + 0x2a0u, x0.l(0));
    LIFT_CALL(0x696940u, 0x692558u);  // PlaceableParticleEffect::vf19
L_692558:
    x0 = movss_m(rd32(m, esi + 0x2a4u));
    x0 = subss(x0, rd32(m, esp + 0x8u));
    x1 = X{};
    fC_692569 = fcmp32(x1.l(0), x0.l(0));
    wr32(m, esi + 0x2a4u, x0.l(0));
    if (fc_b(fC_692569)) goto L_692598;
    eax = rd32(m, esi + 0x90u);
    edx = rd32(m, eax + 0x14u);
    ecx = (esi + 0x90u);
    esp -= 4; wr32(m, esp, 0x0u);
    LIFT_CALL(edx, 0x692589u);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x8u)));
    esp -= 4; wr32(m, esp, ecx);
    ecx = esi;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x696940u, 0x692598u);  // PlaceableParticleEffect::vf19
L_692598:
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 006925a0 JobPool_Instance
//   original JobPool_Instance; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x006925a0, JobPool_Instance, "fx", reflex_port::OUT_EAX) {
    LIFT_ENTRY();
    uint32_t fR_6925a5 = 0;
    eax = 0x1u;
    fR_6925a5 = (rd8(m, 0xd94ad8u) & (eax & 0xffu));
    if (!(fR_6925a5 == 0)) goto L_6925ca;
    wr32(m, 0xd94ad8u, (rd32(m, 0xd94ad8u) | eax));
    ecx = 0xd94ae0u;
    LIFT_CALL(0x881a70u, 0x6925bdu);  // FUN_00881a70
    esp -= 4; wr32(m, esp, 0x912b10u);
    LIFT_CALL(0x8f9fbfu, 0x6925c7u);  // _atexit
    esp = esp + 0x4u;
L_6925ca:
    eax = 0xd94ae0u;
    LIFT_RET(0);
}

// 006925f0 Particle_UpdateJob
//   original Particle_UpdateJob; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x006925f0, Particle_UpdateJob, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_692654 = 0;
    uint32_t fB_692654 = 0;
    FCmp fC_6926a8{};
    uint32_t fR_6926b9 = 0;
    FCmp fC_692732{};
    uint32_t fR_692858 = 0;
    FCmp fC_6929df{};
    FCmp fC_692a02{};
    uint32_t fA_692a78 = 0;
    uint32_t fB_692a78 = 0;
    uint32_t fR_692a97 = 0;
    uint32_t fA_692aaf = 0;
    uint32_t fB_692aaf = 0;
    uint32_t fR_692abf = 0;
    uint32_t fR_692aca = 0;
    uint32_t fR_692ad6 = 0;
    uint32_t fA_692ada = 0;
    uint32_t fB_692ada = 0;
    uint32_t fA_692ae8 = 0;
    uint32_t fB_692ae8 = 0;
    uint32_t fR_692aff = 0;
    uint32_t fA_692b22 = 0;
    uint32_t fB_692b22 = 0;
    uint32_t fA_692b3b = 0;
    uint32_t fB_692b3b = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x74u;
    eax = rd32(m, ebp + 0x8u);
    ecx = rd32(m, FS_BASE + 0x2cu);
    edx = rd32(m, ecx);
    x0 = movss_m(rd32(m, eax + 0x10u));
    ecx = rd32(m, eax + 0x8u);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, eax + 0x4u);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, eax);
    wr32(m, edx + 0x8u, eax);
    edx = rd32(m, edi + 0x14u);
    wr32(m, esp + 0x5cu, x0.l(0));
    x0 = movss_m(rd32(m, eax + 0x14u));
    eax = (edi + 0xbcu);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0x2cu, edi);
    wr32(m, esp + 0x28u, esi);
    wr32(m, esp + 0x54u, ecx);
    wr32(m, esp + 0x44u, x0.l(0));
    wr32(m, esp + 0x68u, edx);
    LIFT_CALL(rd32(m, 0x916080u) /* 8.295241295891262e-39 */, 0x692649u);
    ebx = eax;
    eax = rd32(m, FS_BASE + 0x2cu);
    ecx = rd32(m, eax);
    ebx = (ebx - 1u);
    fA_692654 = ebx;
    fB_692654 = esi;
    wr32(m, ecx + 0x4u, ebx);
    if (int32_t(sext(fA_692654, 4)) >= int32_t(sext(fB_692654, 4))) goto L_692a8a;
L_692660:
    edx = rd32(m, esp + 0x50u);
    x3 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    esi = ebx;
    esi = (esi << 7);
    esi = esi + rd32(m, esp + 0x64u);
    eax = rd32(m, esi + 0x5cu);
    eax = rd32(m, eax);
    edi = rd32(m, eax + 0x10u);
    x0 = movss_m(rd32(m, eax + 0x14u));
    edi = uint32_t(int32_t(edi) * int32_t(0x1f0u));
    x2 = movss_m(rd32(m, esi + 0x44u));
    x1 = movss_m(rd32(m, esi + 0x48u));
    edi = edi + rd32(m, edx + 0x10u);
    wr32(m, esp + 0x54u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x5cu));
    x4 = x2;
    x4 = addss(x4, x1.l(0));
    fC_6926a8 = fcmp32(x0.l(0), x4.l(0));
    if (fc_b(fC_6926a8)) goto L_6926b2;
    x4 = x3;
    goto L_6926b5;
L_6926b2:
    x4 = X{};
L_6926b5:
    eax = cvttss2si(x4.l(0));
    fR_6926b9 = (eax & eax);
    if ((fR_6926b9 == 0)) goto L_6926dd;
    esi = rd32(m, esp + 0x28u);
    ecx = (esi + 0xb8u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(rd32(m, 0x916080u) /* 8.295241295891262e-39 */, 0x6926ceu);
    edx = rd32(m, esi + 0xb4u);
    eax = (eax - 1u);
    wr32(m, edx + eax * 4, ebx);
    goto L_692a68;
L_6926dd:
    x4 = movss_m(rd32(m, edi + 0x140u));
    wr32(m, esp + 0x60u, x4.l(0));
    x4 = movss_m(rd32(m, edi + 0x148u));
    wr32(m, esp + 0x3cu, x4.l(0));
    x4 = movss_m(rd32(m, edi + 0x150u));
    wr32(m, esp + 0x48u, x4.l(0));
    x4 = movss_m(rd32(m, edi + 0x134u));
    wr32(m, esp + 0x4cu, x4.l(0));
    x0 = subss(x0, x2.l(0));
    x4 = X{};
    x4 = movss_r(x4, x1);
    x1 = x4;
    x1 = rcpss(x1, x1.l(0));
    stx(m, esp + 0x70u, x1);
    x0 = mulss(x0, rd32(m, esp + 0x70u));
    fC_692732 = fcmp32(x3.l(0), x0.l(0));
    wr32(m, esp + 0x2cu, x0.l(0));
    if (!(fc_b(fC_692732) || fc_z(fC_692732))) goto L_692746;
    x0 = x3;
    wr32(m, esp + 0x2cu, x0.l(0));
L_692746:
    x0 = mulss(x0, rd32(m, 0x977700u) /* 9.0 */);
    wr32(m, esp + 0x58u, x0.l(0));
    ebx = cvttss2si(x0.l(0));
    x0 = cvtps2pd(x0.lo);
    x0 = subsd(x0, rd64(m, 0x977558u) /* 0.49999999999 */);
    wr64(m, esp + 0x68u, x0.lo);
    fpush(m, reflex_f80::from_f64(rd64(m, esp + 0x68u)));
    esp = esp - 0x10u;
    wr64(m, esp + 0x8u, fst64(m));
    fpop(m);
    fpush(m, reflex_f80::from_f64(rd64(m, 0x9774e8u) /* 0.5 */));
    wr64(m, esp, fst64(m));
    fpop(m);
    LIFT_CALL(rd32(m, 0x916230u) /* 8.28646636490766e-39 */, 0x692783u);
    st(m, 0) = farith(m, 0, st(m, 0), reflex_f80::from_f64(rd64(m, esp + 0x78u)));
    esp = esp + 0x10u;
    LIFT_CALL(0x8fa0d0u, 0x69278fu);  // FUN_008fa0d0
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x154u)));
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x2cu)));
    x1 = cvtsi2ss(x1, eax);
    st(m, 1) = farith(m, 1, st(m, 1), st(m, 0));
    x0 = movss_m(rd32(m, esp + 0x58u));
    wr16(m, esp + 0x16u, uint32_t(m.fpu_cw));
    x5 = movss_m(rd32(m, esp + 0x40u));
    eax = rd16(m, esp + 0x16u);
    std::swap(st(m, 0), st(m, 1));
    st(m, 0) = farith(m, 0, st(m, 0), reflex_f80::from_f32(rd32(m, esp + 0x48u)));
    eax = (eax | 0xc00u);
    wr32(m, esp + 0x18u, eax);
    x0 = subss(x0, x1.l(0));
    x1 = movss_m(rd32(m, esi + 0x38u));
    m.fpu_cw = uint16_t(rd16(m, esp + 0x18u));
    x6 = x0;
    x0 = movss_m(rd32(m, esi + 0x3cu));
    x0 = mulss(x0, x5.l(0));
    x1 = subss(x1, x0.l(0));
    x4 = movss_r(x4, x5);
    wr64(m, esp + 0x70u, uint64_t(fist(m, 8, frc(m))));
    fpop(m);
    x4 = shufps(x4, x4, 0x0);
    wr32(m, esp + 0x44u, x6.l(0));
    eax = rd32(m, esp + 0x70u);
    wr32(m, esp + 0x70u, eax);
    m.fpu_cw = uint16_t(rd16(m, esp + 0x16u));
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x14cu)));
    wr16(m, esp + 0x16u, uint32_t(m.fpu_cw));
    st(m, 0) = farith(m, 1, st(m, 0), st(m, 1));
    eax = rd16(m, esp + 0x16u);
    eax = (eax | 0xc00u);
    wr32(m, esp + 0x18u, eax);
    st(m, 0) = farith(m, 0, st(m, 0), reflex_f80::from_f32(rd32(m, esp + 0x3cu)));
    m.fpu_cw = uint16_t(rd16(m, esp + 0x18u));
    wr64(m, esp + 0x30u, uint64_t(fist(m, 8, frc(m))));
    fpop(m);
    ecx = rd32(m, esp + 0x30u);
    wr32(m, esp + 0x30u, ecx);
    m.fpu_cw = uint16_t(rd16(m, esp + 0x16u));
    st(m, 0) = farith(m, 1, st(m, 0), reflex_f80::from_f32(rd32(m, edi + 0x144u)));
    wr16(m, esp + 0x16u, uint32_t(m.fpu_cw));
    wr32(m, esi + 0x38u, x1.l(0));
    st(m, 0) = farith(m, 0, st(m, 0), reflex_f80::from_f32(rd32(m, esp + 0x60u)));
    x3 = ldx(m, esi + 0x20u);
    eax = rd16(m, esp + 0x16u);
    x0 = ldx(m, esi + 0x10u);
    x7 = ldx(m, esi);
    eax = (eax | 0xc00u);
    fR_692858 = (rd8(m, 0xdb1400u) & 0x1u);
    wr32(m, esp + 0x18u, eax);
    m.fpu_cw = uint16_t(rd16(m, esp + 0x18u));
    x3 = mulps(x3, x4);
    x3 = addps(x3, x0);
    wr64(m, esp + 0x18u, uint64_t(fist(m, 8, frc(m))));
    fpop(m);
    ecx = rd32(m, esp + 0x18u);
    m.fpu_cw = uint16_t(rd16(m, esp + 0x16u));
    if (!(fR_692858 == 0)) goto L_692895;
    x0 = movss_m(rd32(m, 0x9776fcu) /* -0.0012127001537010074 */);
    wr32(m, 0xdb1400u, (rd32(m, 0xdb1400u) | 0x1u));
    x0 = shufps(x0, x0, 0x0);
    stx(m, 0xdb13f0u, x0);
L_692895:
    eax = rd32(m, edi + 0x110u);
    wr16(m, esp + 0x16u, uint32_t(m.fpu_cw));
    eax = eax + ebx;
    x2 = ldx(m, 0xd61670u);
    eax = eax + eax;
    fpush(m, reflex_f80::from_f32(rd32(m, eax + eax + 0xa817b0u)));
    eax = eax + eax;
    wr32(m, esp + 0x18u, fst32(m));
    fpop(m);
    x0 = x3;
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0xa817b4u)));
    eax = rd16(m, esp + 0x16u);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x18u)));
    x0 = mulps(x0, x3);
    x1 = x0;
    st(m, 1) = farith(m, 4, st(m, 1), st(m, 0));
    x1 = shufps(x1, x0, 0x99);
    std::swap(st(m, 0), st(m, 1));
    x0 = addss(x0, x1.l(0));
    st(m, 0) = farith(m, 1, st(m, 0), reflex_f80::from_f32(rd32(m, esp + 0x44u)));
    x1 = shufps(x1, x1, 0x55);
    x0 = addss(x0, x1.l(0));
    x1 = x0;
    x1 = shufps(x1, x0, 0x0);
    st(m, 1) = farith(m, 0, st(m, 1), st(m, 0));
    fpop(m);
    x0 = movss_m(rd32(m, 0x9774a4u) /* 9.999999747378752e-06 */);
    x0 = shufps(x0, x0, 0x0);
    st(m, 0) = farith(m, 1, st(m, 0), reflex_f80::from_f32(rd32(m, edi + 0x138u)));
    x1 = addps(x1, x0);
    x0 = movss_m(rd32(m, edi + 0x15cu));
    x0 = shufps(x0, x0, 0x0);
    st(m, 0) = farith(m, 0, st(m, 0), reflex_f80::from_f32(rd32(m, esp + 0x4cu)));
    x0 = mulps(x0, x1);
    x0 = mulps(x0, ldx(m, 0xdb13f0u));
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x54u)));
    st(m, 0) = farith(m, 1, st(m, 0), reflex_f80::from_f32(rd32(m, 0x92e104u) /* 255.0 */));
    eax = (eax | 0xc00u);
    wr32(m, esp + 0x18u, eax);
    eax = rd32(m, edi + 0x10cu);
    x1 = rsqrtps(x1, x1);
    st(m, 1) = farith(m, 1, st(m, 1), st(m, 0));
    fpop(m);
    m.fpu_cw = uint16_t(rd16(m, esp + 0x18u));
    x0 = mulps(x0, x1);
    x1 = ldx(m, 0xd61650u);
    x0 = addps(x0, x1);
    x2 = maxps(x2, x0);
    eax = eax + ebx;
    eax = eax + eax;
    wr64(m, esp + 0x18u, uint64_t(fist(m, 8, frc(m))));
    fpop(m);
    x1 = movss_m(rd32(m, eax + eax + 0xa817b0u));
    x0 = movss_m(rd32(m, eax + eax + 0xa817b4u));
    edx = rd32(m, esp + 0x18u);
    x0 = subss(x0, x1.l(0));
    eax = eax + eax;
    m.fpu_cw = uint16_t(rd16(m, esp + 0x16u));
    x0 = mulss(x0, x6.l(0));
    x0 = addss(x0, x1.l(0));
    x0 = mulss(x0, rd32(m, edi + 0x130u));
    x0 = addss(x0, rd32(m, edi + 0x12cu));
    wr32(m, esi + 0x40u, x0.l(0));
    x1 = movss_m(rd32(m, edi + 0x13cu));
    x1 = mulss(x1, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x58u));
    x2 = mulps(x2, x3);
    wr32(m, esi + 0x40u, x1.l(0));
    x0 = mulss(x0, x5.l(0));
    x0 = addss(x0, rd32(m, esi + 0x54u));
    x3 = x2;
    x3 = mulps(x3, x4);
    wr32(m, esi + 0x54u, x0.l(0));
    x4 = movss_m(rd32(m, edi + 0xccu));
    x1 = movss_m(rd32(m, edi + 0xd0u));
    x1 = addss(x1, x4.l(0));
    x1 = subss(x1, rd32(m, 0x9775a4u) /* 9.999999747378752e-05 */);
    fC_6929df = fcmp32(x0.l(0), x1.l(0));
    wr32(m, esp + 0x18u, edx);
    x3 = addps(x3, x7);
    if (fc_b(fC_6929df)) goto L_692a02;
    x4 = addss(x4, x0.l(0));
    x0 = movss_m(rd32(m, edi + 0xe4u));
    x0 = mulss(x0, x1.l(0));
    x4 = subss(x4, x0.l(0));
    x0 = x4;
L_692a02:
    fC_692a02 = fcmp32(x0.l(0), x1.l(0));
    wr32(m, esi + 0x54u, x0.l(0));
    if (fc_b(fC_692a02)) goto L_692a0f;
    x0 = x1;
L_692a0f:
    edx = rd8(m, esi + 0x4eu);
    wr32(m, esi + 0x54u, x0.l(0));
    edi = rd32(m, edi + 0x158u);
    eax = edi;
    edx = (edx & edi);
    eax = ~eax;
    ebx = eax;
    ebx = (ebx & ecx);
    ecx = rd32(m, esp + 0x18u);
    edx = (edx | ebx);
    ecx = (ecx << 8);
    edx = (edx | ecx);
    ecx = rd8(m, esi + 0x4du);
    ecx = (ecx & edi);
    edi = (edi & rd32(m, esi + 0x4cu));
    edx = (edx << 8);
    edx = (edx | ecx);
    ecx = eax;
    ecx = (ecx & rd32(m, esp + 0x30u));
    eax = (eax & rd32(m, esp + 0x70u));
    edx = (edx | ecx);
    edx = (edx << 8);
    edi = (edi & 0xffu);
    edx = (edx | edi);
    edx = (edx | eax);
    wr32(m, esi + 0x4cu, edx);
    stx(m, esi + 0x10u, x2);
    stx(m, esi, x3);
    esi = rd32(m, esp + 0x28u);
L_692a68:
    eax = (esi + 0xbcu);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x916080u) /* 8.295241295891262e-39 */, 0x692a75u);
    ebx = eax;
    ebx = (ebx - 1u);
    fA_692a78 = ebx;
    fB_692a78 = rd32(m, esp + 0x24u);
    if (int32_t(sext(fA_692a78, 4)) < int32_t(sext(fB_692a78, 4))) goto L_692660;
    esi = rd32(m, esp + 0x24u);
    edi = rd32(m, esp + 0x28u);
L_692a8a:
    edx = (edi + 0xc4u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(rd32(m, 0x916084u) /* 8.295274927054406e-39 */, 0x692a97u);
    fR_692a97 = (eax & eax);
    if (!(fR_692a97 == 0)) goto L_692b40;
    ebx = rd32(m, edi + 0xb8u);
    edx = rd32(m, edi + 0x14u);
    ebx = (ebx - 1u);
    wr32(m, 0xa94270u, rd32(m, 0xa94270u) - ebx);
    fA_692aaf = ebx;
    fB_692aaf = esi;
    wr32(m, esp + 0x30u, edx);
    if (int32_t(sext(fA_692aaf, 4)) < int32_t(sext(fB_692aaf, 4))) goto L_692abf;
    wr32(m, esp + 0x24u, eax);
    esi = eax;
    goto L_692b03;
L_692abf:
    fR_692abf = (ebx & ebx);
    if (((fR_692abf == 0) || sgn(fR_692abf, 4) != false)) goto L_692b03;
    ecx = esi;
    ecx = (ecx << 7);
    ecx = ecx + edx;
L_692aca:
    fR_692aca = (esi & esi);
    if (((fR_692aca == 0) || sgn(fR_692aca, 4) != false)) goto L_692b03;
    esi = (esi - 1u);
    ecx = ecx + 0xffffff80u;
    wr32(m, esp + 0x70u, ecx);
    fR_692ad6 = (esi & esi);
    if ((sgn(fR_692ad6, 4) != false)) goto L_692afe;
    fA_692ada = esi;
    fB_692ada = rd32(m, edi + 0x1cu);
    if (int32_t(sext(fA_692ada, 4)) >= int32_t(sext(fB_692ada, 4))) goto L_692afe;
    eax = rd32(m, edi + 0xb4u);
    eax = rd32(m, eax + ebx * 4);
    fA_692ae8 = eax;
    fB_692ae8 = esi;
    if (int32_t(sext(fA_692ae8, 4)) >= int32_t(sext(fB_692ae8, 4))) goto L_692afe;
    eax = (eax << 7);
    eax = eax + edx;
    LIFT_CALL(0x692b50u, 0x692af6u);  // Particle_Copy
    ecx = rd32(m, esp + 0x70u);
    edx = rd32(m, esp + 0x30u);
L_692afe:
    ebx = (ebx - 1u);
    fR_692aff = (ebx & ebx);
    if ((!(fR_692aff == 0) && sgn(fR_692aff, 4) == false)) goto L_692aca;
L_692b03:
    wr32(m, edi + 0xacu, esi);
    esp -= 4; wr32(m, esp, 0x1u);
    wr32(m, edi + 0xb0u, esi);
    esi = rd32(m, 0x91609cu) /* 8.295429069885481e-39 */;
    esp -= 4; wr32(m, esp, 0x0u);
    edi = edi + 0xc8u;
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(esi, 0x692b22u);
    fA_692b22 = eax;
    fB_692b22 = 0x1u;
    if (fA_692b22 == fB_692b22) goto L_692b40;
    ebx = rd32(m, 0x916020u) /* 8.295000272555398e-39 */;
L_692b30:
    esp -= 4; wr32(m, esp, 0x0u);
    LIFT_CALL(ebx, 0x692b34u);
    esp -= 4; wr32(m, esp, 0x1u);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(esi, 0x692b3bu);
    fA_692b3b = eax;
    fB_692b3b = 0x1u;
    if (fA_692b3b != fB_692b3b) goto L_692b30;
L_692b40:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00692b50 Particle_Copy
//   original Particle_Copy; register inputs: ecx eax; returns with ret 0
REFLEX_PORT_FN(0x00692b50, Particle_Copy, "fx", reflex_port::OUT_ECX) {
    LIFT_ENTRY();

    fpush(m, reflex_f80::from_f32(rd32(m, ecx)));
    wr32(m, eax, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x4u)));
    wr32(m, eax + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x8u)));
    wr32(m, eax + 0x8u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x10u)));
    wr32(m, eax + 0x10u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x14u)));
    wr32(m, eax + 0x14u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x18u)));
    wr32(m, eax + 0x18u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x20u)));
    wr32(m, eax + 0x20u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x24u)));
    wr32(m, eax + 0x24u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x28u)));
    wr32(m, eax + 0x28u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x30u)));
    wr32(m, eax + 0x30u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x34u)));
    wr32(m, eax + 0x34u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x38u)));
    wr32(m, eax + 0x38u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x3cu)));
    wr32(m, eax + 0x3cu, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x40u)));
    wr32(m, eax + 0x40u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x44u)));
    wr32(m, eax + 0x44u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x48u)));
    wr32(m, eax + 0x48u, fst32(m));
    fpop(m);
    edx = rd32(m, ecx + 0x4cu);
    wr32(m, eax + 0x4cu, edx);
    edx = rd32(m, ecx + 0x50u);
    wr32(m, eax + 0x50u, edx);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x54u)));
    wr32(m, eax + 0x54u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x58u)));
    wr32(m, eax + 0x58u, fst32(m));
    fpop(m);
    edx = rd32(m, ecx + 0x5cu);
    wr32(m, eax + 0x5cu, edx);
    edx = rd32(m, ecx + 0x60u);
    wr32(m, eax + 0x60u, edx);
    edx = rd32(m, ecx + 0x64u);
    wr32(m, eax + 0x64u, edx);
    fpush(m, reflex_f80::from_f64(rd64(m, ecx + 0x68u)));
    wr64(m, eax + 0x68u, fst64(m));
    fpop(m);
    edx = rd32(m, ecx + 0x78u);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x70u)));
    wr32(m, eax + 0x78u, edx);
    wr32(m, eax + 0x70u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x74u)));
    wr32(m, eax + 0x74u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x70u)));
    wr32(m, eax + 0x70u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x74u)));
    wr32(m, eax + 0x74u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x78u)));
    wr32(m, eax + 0x78u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x7cu)));
    wr32(m, eax + 0x7cu, fst32(m));
    fpop(m);
    LIFT_RET(0);
}

// 00693560 Fn_00693560
//   original FUN_00693560; register inputs: ecx eax esi; returns with ret 24
REFLEX_PORT_FN(0x00693560, Fn_00693560, "fx", 0) {
    LIFT_ENTRY();
    uint32_t fA_693560 = 0;
    uint32_t fB_693560 = 0;
    uint32_t fA_693581 = 0;
    uint32_t fB_693581 = 0;
    uint32_t fA_6935c9 = 0;
    uint32_t fB_6935c9 = 0;
    FCmp fC_6935e1{};
    uint32_t fA_693601 = 0;
    uint32_t fB_693601 = 0;
    uint32_t fA_69364c = 0;
    uint32_t fB_69364c = 0;
    FCmp fC_693664{};
    uint32_t fA_693684 = 0;
    uint32_t fB_693684 = 0;
    uint32_t fA_693698 = 0;
    uint32_t fB_693698 = 0;
    FCmp fC_6936ad{};
    uint32_t fA_6936cd = 0;
    uint32_t fB_6936cd = 0;
    uint32_t fA_6936dd = 0;
    uint32_t fB_6936dd = 0;
    FCmp fC_6936f2{};
    uint32_t fA_69371c = 0;
    uint32_t fB_69371c = 0;
    uint32_t fA_69376a = 0;
    uint32_t fB_69376a = 0;
    FCmp fC_693782{};
    fA_693560 = rd8(m, esp + 0x8u);
    fB_693560 = 0x0u;
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x8u);
    wr32(m, esp + 0x8u, x0.l(0));
    if (fA_693560 != fB_693560) goto L_69371c;
    eax = uint32_t(int32_t(eax) * int32_t(0x70u));
    fA_693581 = rd8(m, esi + 0x5u);
    fB_693581 = 0x0u;
    esp -= 4; wr32(m, esp, edi);
    edi = (eax + ebx);
    if (fA_693581 == fB_693581) goto L_6935fe;
    x2 = movss_m(rd32(m, edi + 0x50u));
    x1 = movss_m(rd32(m, edi + 0x54u));
    x0 = movss_m(rd32(m, edi + 0x58u));
    x3 = x2;
    x3 = mulss(x3, x2.l(0));
    x2 = x1;
    x2 = mulss(x2, x1.l(0));
    x1 = x0;
    x3 = addss(x3, x2.l(0));
    x1 = mulss(x1, x0.l(0));
    esp -= 4; wr32(m, esp, ecx);
    x3 = addss(x3, x1.l(0));
    wr32(m, esp, x3.l(0));
    LIFT_CALL(0x42c230u, 0x6935c2u);  // FUN_0042c230
    x1 = movss_m(rd32(m, esi));
    esp = esp + 0x4u;
    fA_6935c9 = rd8(m, esi + 0x4u);
    fB_6935c9 = 0x0u;
    x1 = mulss(x1, x0.l(0));
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    if (fA_6935c9 != fB_6935c9) goto L_6935e1;
    x1 = addss(x1, x0.l(0));
    goto L_693601;
L_6935e1:
    fC_6935e1 = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_6935e1) || fc_z(fC_6935e1))) goto L_6935f2;
    x2 = x1;
    x1 = x0;
    x1 = divss(x1, x2.l(0));
    goto L_693601;
L_6935f2:
    x2 = x0;
    x1 = x0;
    x1 = divss(x1, x2.l(0));
    goto L_693601;
L_6935fe:
    x1 = x0;
L_693601:
    fA_693601 = rd8(m, esi + 0xdu);
    fB_693601 = 0x0u;
    wr32(m, esp + 0xcu, x1.l(0));
    if (fA_693601 == fB_693601) goto L_693681;
    x2 = movss_m(rd32(m, edi + 0x40u));
    x1 = movss_m(rd32(m, edi + 0x44u));
    x0 = movss_m(rd32(m, edi + 0x48u));
    x3 = x2;
    x3 = mulss(x3, x2.l(0));
    x2 = x1;
    x2 = mulss(x2, x1.l(0));
    x1 = x0;
    x3 = addss(x3, x2.l(0));
    x1 = mulss(x1, x0.l(0));
    esp -= 4; wr32(m, esp, ecx);
    x3 = addss(x3, x1.l(0));
    wr32(m, esp, x3.l(0));
    LIFT_CALL(0x42c230u, 0x693644u);  // FUN_0042c230
    x1 = movss_m(rd32(m, esi + 0x8u));
    esp = esp + 0x4u;
    fA_69364c = rd8(m, esi + 0xcu);
    fB_69364c = 0x0u;
    x1 = mulss(x1, x0.l(0));
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    if (fA_69364c != fB_69364c) goto L_693664;
    x1 = addss(x1, x0.l(0));
    goto L_693684;
L_693664:
    fC_693664 = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_693664) || fc_z(fC_693664))) goto L_693675;
    x2 = x1;
    x1 = x0;
    x1 = divss(x1, x2.l(0));
    goto L_693684;
L_693675:
    x2 = x0;
    x1 = x0;
    x1 = divss(x1, x2.l(0));
    goto L_693684;
L_693681:
    x1 = x0;
L_693684:
    fA_693684 = rd8(m, esi + 0x1du);
    fB_693684 = 0x0u;
    x1 = mulss(x1, rd32(m, esp + 0xcu));
    x2 = movss_m(rd32(m, edi + 0x60u));
    x3 = x1;
    if (fA_693684 == fB_693684) goto L_6936ca;
    fA_693698 = rd8(m, esi + 0x1cu);
    fB_693698 = 0x0u;
    x1 = movss_m(rd32(m, esi + 0x18u));
    x1 = mulss(x1, x2.l(0));
    if (fA_693698 != fB_693698) goto L_6936ad;
    x1 = addss(x1, x0.l(0));
    goto L_6936cd;
L_6936ad:
    fC_6936ad = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_6936ad) || fc_z(fC_6936ad))) goto L_6936be;
    x2 = x1;
    x1 = x0;
    x1 = divss(x1, x2.l(0));
    goto L_6936cd;
L_6936be:
    x2 = x0;
    x1 = x0;
    x1 = divss(x1, x2.l(0));
    goto L_6936cd;
L_6936ca:
    x1 = x0;
L_6936cd:
    fA_6936cd = rd8(m, esi + 0x25u);
    fB_6936cd = 0x0u;
    x1 = mulss(x1, x3.l(0));
    x3 = movss_m(rd32(m, edi + 0x64u));
    edi = rd32(m, esp); esp += 4;
    if (fA_6936cd == fB_6936cd) goto L_69370f;
    fA_6936dd = rd8(m, esi + 0x24u);
    fB_6936dd = 0x0u;
    x2 = movss_m(rd32(m, esi + 0x20u));
    x2 = mulss(x2, x3.l(0));
    if (fA_6936dd != fB_6936dd) goto L_6936f2;
    x2 = addss(x2, x0.l(0));
    goto L_693712;
L_6936f2:
    fC_6936f2 = fcmp32(x2.l(0), x0.l(0));
    if ((fc_b(fC_6936f2) || fc_z(fC_6936f2))) goto L_693703;
    x3 = x2;
    x2 = x0;
    x2 = divss(x2, x3.l(0));
    goto L_693712;
L_693703:
    x3 = x0;
    x2 = x0;
    x2 = divss(x2, x3.l(0));
    goto L_693712;
L_69370f:
    x2 = x0;
L_693712:
    x2 = mulss(x2, x1.l(0));
    wr32(m, esp + 0x8u, x2.l(0));
L_69371c:
    fA_69371c = rd8(m, esi + 0x15u);
    fB_69371c = 0x0u;
    if (fA_69371c == fB_69371c) goto L_693791;
    x2 = movss_m(rd32(m, ebx + 0x1f0u));
    x1 = movss_m(rd32(m, ebx + 0x1f4u));
    x0 = movss_m(rd32(m, ebx + 0x1f8u));
    x3 = x2;
    x3 = mulss(x3, x2.l(0));
    x2 = x1;
    x2 = mulss(x2, x1.l(0));
    x1 = x0;
    x3 = addss(x3, x2.l(0));
    x1 = mulss(x1, x0.l(0));
    esp -= 4; wr32(m, esp, ecx);
    x3 = addss(x3, x1.l(0));
    wr32(m, esp, x3.l(0));
    LIFT_CALL(0x42c230u, 0x693762u);  // FUN_0042c230
    x0 = mulss(x0, rd32(m, esi + 0x10u));
    esp = esp + 0x4u;
    fA_69376a = rd8(m, esi + 0x14u);
    fB_69376a = 0x0u;
    if (fA_69376a != fB_69376a) goto L_69377a;
    x0 = addss(x0, rd32(m, 0x931f24u) /* 1.0 */);
    goto L_693791;
L_69377a:
    x1 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    fC_693782 = fcmp32(x0.l(0), x1.l(0));
    if (!(fc_b(fC_693782) || fc_z(fC_693782))) goto L_69378a;
    x0 = x1;
L_69378a:
    x1 = divss(x1, x0.l(0));
    x0 = x1;
L_693791:
    x0 = mulss(x0, rd32(m, esp + 0x8u));
    eax = rd32(m, esp + 0x18u);
    ecx = rd32(m, esp + 0x1cu);
    x1 = x0;
    x1 = mulss(x1, rd32(m, esp + 0x10u));
    x0 = mulss(x0, rd32(m, esp + 0x14u));
    wr32(m, eax, x1.l(0));
    x0 = subss(x0, x1.l(0));
    wr32(m, ecx, x0.l(0));
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(24);
}

// 006941a0 Fn_006941a0
//   original FUN_006941a0; register inputs: ecx esi edi; returns with ret 8
REFLEX_PORT_FN(0x006941a0, Fn_006941a0, "fx", reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    FCmp fC_69421e{};
    FCmp fC_6942fb{};
    FCmp fC_6943d3{};
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x1cu;
    x1 = movss_m(rd32(m, ebp + 0xcu));
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x3 = movss_m(rd32(m, edi));
    x4 = movss_m(rd32(m, edi + 0x4u));
    x5 = movss_m(rd32(m, edi + 0x8u));
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0x8u);
    x2 = movss_m(rd32(m, ebx));
    x0 = subss(x0, x1.l(0));
    x3 = mulss(x3, x1.l(0));
    x2 = mulss(x2, x0.l(0));
    x2 = addss(x2, x3.l(0));
    x3 = movss_m(rd32(m, ebx + 0x4u));
    x4 = mulss(x4, x1.l(0));
    x5 = mulss(x5, x1.l(0));
    x3 = mulss(x3, x0.l(0));
    x3 = addss(x3, x4.l(0));
    x4 = movss_m(rd32(m, ebx + 0x8u));
    x4 = mulss(x4, x0.l(0));
    x4 = addss(x4, x5.l(0));
    x5 = x4;
    x5 = mulss(x5, x4.l(0));
    x6 = x3;
    x6 = mulss(x6, x3.l(0));
    x5 = addss(x5, x6.l(0));
    x6 = x2;
    x6 = mulss(x6, x2.l(0));
    x5 = addss(x5, x6.l(0));
    x6 = X{};
    fC_69421e = fcmp32(x5.l(0), x6.l(0));
    wr32(m, esp + 0x8u, x0.l(0));
    wr32(m, esp + 0x10u, x2.l(0));
    wr32(m, esp + 0x14u, x3.l(0));
    wr32(m, esp + 0x18u, x4.l(0));
    wr32(m, esp + 0xcu, x5.l(0));
    if ((fc_b(fC_69421e) || fc_z(fC_69421e))) goto L_694289;
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0xcu)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x42c230u, 0x69424eu);  // FUN_0042c230
    x4 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x1 = movss_m(rd32(m, ebp + 0xcu));
    x6 = X{};
    x4 = divss(x4, x0.l(0));
    x0 = x4;
    x0 = mulss(x0, rd32(m, esp + 0x14u));
    x2 = x0;
    x0 = x4;
    x0 = mulss(x0, rd32(m, esp + 0x18u));
    x4 = mulss(x4, rd32(m, esp + 0x1cu));
    x3 = x0;
    x0 = movss_m(rd32(m, esp + 0xcu));
    esp = esp + 0x4u;
L_694289:
    wr32(m, esi + 0x4u, x3.l(0));
    wr32(m, esi + 0x8u, x4.l(0));
    wr32(m, esi, x2.l(0));
    wr32(m, esi + 0xcu, x6.l(0));
    x2 = movss_m(rd32(m, ebx + 0x10u));
    x3 = movss_m(rd32(m, edi + 0x10u));
    x4 = movss_m(rd32(m, edi + 0x14u));
    x5 = movss_m(rd32(m, edi + 0x18u));
    x3 = mulss(x3, x1.l(0));
    x2 = mulss(x2, x0.l(0));
    x2 = addss(x2, x3.l(0));
    x3 = movss_m(rd32(m, ebx + 0x14u));
    x4 = mulss(x4, x1.l(0));
    x5 = mulss(x5, x1.l(0));
    x3 = mulss(x3, x0.l(0));
    x3 = addss(x3, x4.l(0));
    x4 = movss_m(rd32(m, ebx + 0x18u));
    x4 = mulss(x4, x0.l(0));
    x4 = addss(x4, x5.l(0));
    x5 = x4;
    x5 = mulss(x5, x4.l(0));
    x7 = x3;
    x7 = mulss(x7, x3.l(0));
    x5 = addss(x5, x7.l(0));
    x7 = x2;
    x7 = mulss(x7, x2.l(0));
    x5 = addss(x5, x7.l(0));
    fC_6942fb = fcmp32(x5.l(0), x6.l(0));
    wr32(m, esp + 0x10u, x2.l(0));
    wr32(m, esp + 0x14u, x3.l(0));
    wr32(m, esp + 0x18u, x4.l(0));
    wr32(m, esp + 0xcu, x5.l(0));
    if ((fc_b(fC_6942fb) || fc_z(fC_6942fb))) goto L_694360;
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0xcu)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x42c230u, 0x694325u);  // FUN_0042c230
    x4 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x1 = movss_m(rd32(m, ebp + 0xcu));
    x6 = X{};
    x4 = divss(x4, x0.l(0));
    x0 = x4;
    x0 = mulss(x0, rd32(m, esp + 0x14u));
    x2 = x0;
    x0 = x4;
    x0 = mulss(x0, rd32(m, esp + 0x18u));
    x4 = mulss(x4, rd32(m, esp + 0x1cu));
    x3 = x0;
    x0 = movss_m(rd32(m, esp + 0xcu));
    esp = esp + 0x4u;
L_694360:
    wr32(m, esi + 0x14u, x3.l(0));
    wr32(m, esi + 0x18u, x4.l(0));
    wr32(m, esi + 0x10u, x2.l(0));
    wr32(m, esi + 0x1cu, x6.l(0));
    x2 = movss_m(rd32(m, ebx + 0x20u));
    x3 = movss_m(rd32(m, edi + 0x20u));
    x4 = movss_m(rd32(m, edi + 0x24u));
    x5 = movss_m(rd32(m, edi + 0x28u));
    x3 = mulss(x3, x1.l(0));
    x2 = mulss(x2, x0.l(0));
    x2 = addss(x2, x3.l(0));
    x3 = movss_m(rd32(m, ebx + 0x24u));
    x4 = mulss(x4, x1.l(0));
    x5 = mulss(x5, x1.l(0));
    x3 = mulss(x3, x0.l(0));
    x3 = addss(x3, x4.l(0));
    x4 = movss_m(rd32(m, ebx + 0x28u));
    x4 = mulss(x4, x0.l(0));
    x4 = addss(x4, x5.l(0));
    x5 = x4;
    x5 = mulss(x5, x4.l(0));
    x7 = x3;
    x7 = mulss(x7, x3.l(0));
    x5 = addss(x5, x7.l(0));
    x7 = x2;
    x7 = mulss(x7, x2.l(0));
    x5 = addss(x5, x7.l(0));
    fC_6943d3 = fcmp32(x5.l(0), x6.l(0));
    wr32(m, esp + 0x10u, x2.l(0));
    wr32(m, esp + 0x14u, x3.l(0));
    wr32(m, esp + 0x18u, x4.l(0));
    wr32(m, esp + 0xcu, x5.l(0));
    if ((fc_b(fC_6943d3) || fc_z(fC_6943d3))) goto L_694438;
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0xcu)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x42c230u, 0x6943fdu);  // FUN_0042c230
    x4 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x1 = movss_m(rd32(m, ebp + 0xcu));
    x6 = X{};
    x4 = divss(x4, x0.l(0));
    x0 = x4;
    x0 = mulss(x0, rd32(m, esp + 0x14u));
    x2 = x0;
    x0 = x4;
    x0 = mulss(x0, rd32(m, esp + 0x18u));
    x4 = mulss(x4, rd32(m, esp + 0x1cu));
    x3 = x0;
    x0 = movss_m(rd32(m, esp + 0xcu));
    esp = esp + 0x4u;
L_694438:
    wr32(m, esi + 0x20u, x2.l(0));
    wr32(m, esi + 0x24u, x3.l(0));
    wr32(m, esi + 0x28u, x4.l(0));
    wr32(m, esi + 0x2cu, x6.l(0));
    x2 = movss_m(rd32(m, ebx + 0x30u));
    x3 = movss_m(rd32(m, edi + 0x30u));
    x2 = mulss(x2, x0.l(0));
    x3 = mulss(x3, x1.l(0));
    x2 = addss(x2, x3.l(0));
    wr32(m, esi + 0x30u, x2.l(0));
    x2 = movss_m(rd32(m, ebx + 0x34u));
    x3 = movss_m(rd32(m, edi + 0x34u));
    x2 = mulss(x2, x0.l(0));
    x3 = mulss(x3, x1.l(0));
    x2 = addss(x2, x3.l(0));
    wr32(m, esi + 0x34u, x2.l(0));
    x2 = movss_m(rd32(m, ebx + 0x38u));
    x2 = mulss(x2, x0.l(0));
    x0 = movss_m(rd32(m, edi + 0x38u));
    x0 = mulss(x0, x1.l(0));
    x2 = addss(x2, x0.l(0));
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    wr32(m, esi + 0x38u, x2.l(0));
    wr32(m, esi + 0x3cu, x0.l(0));
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 006944c0 Fn_006944c0
//   original FUN_006944c0; register inputs: ecx eax; returns with ret 12
REFLEX_PORT_FN(0x006944c0, Fn_006944c0, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fA_6944d7 = 0;
    uint32_t fB_6944d7 = 0;
    FCmp fC_694554{};
    uint32_t fR_6945a0 = 0;
    uint32_t fA_6945af = 0;
    uint32_t fB_6945af = 0;
    uint32_t fA_6945b9 = 0;
    uint32_t fB_6945b9 = 0;
    uint32_t fA_69467b = 0;
    uint32_t fB_69467b = 0;
    uint32_t fA_694687 = 0;
    uint32_t fB_694687 = 0;
    uint32_t fR_694885 = 0;
    uint32_t fR_6949e7 = 0;
    uint32_t fA_694a17 = 0;
    uint32_t fB_694a17 = 0;
    uint32_t fA_694a6c = 0;
    uint32_t fB_694a6c = 0;
    uint32_t fA_694aeb = 0;
    uint32_t fB_694aeb = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x64u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = eax;
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x1cu);
    LIFT_CALL(edx, 0x6944d5u);
    ecx = 0u;
    fA_6944d7 = eax;
    fB_6944d7 = 0x2u;
    ecx = (ecx & 0xffffff00u) | (((fA_6944d7 == fB_6944d7) ? 1u : 0u) & 0xffu);
    edx = 0x1u;
    esi = rd32(m, ebp + 0x10u);
    x1 = movss_m(rd32(m, 0x977954u) /* 0.01666666753590107 */);
    wr32(m, esp + 0x2cu, ecx);
    ecx = rd32(m, ebp + 0x8u);
    eax = rd32(m, ecx + 0x14u);
    wr32(m, eax + 0x4cu, edx);
    ebx = rd32(m, ecx + 0x18u);
    eax = rd32(m, eax + 0x44u);
    ebx = uint32_t(int32_t(ebx) * int32_t(0x1f0u));
    ebx = ebx + rd32(m, eax + 0x10u);
    wr32(m, 0x9cb864u, rd32(m, 0x9cb864u) + edx);
    wr32(m, esp + 0x28u, eax);
    eax = rd32(m, 0x9cb860u);
    eax = uint32_t(int32_t(eax) * int32_t(0x41c64e6du));
    eax = eax + 0x3039u;
    wr32(m, 0x9cb860u, eax);
    eax = (eax >> 16);
    eax = (eax & 0x7fffu);
    x0 = cvtsi2ss(x0, eax);
    x0 = cvtss2sd(x0, x0.l(0));
    x0 = mulsd(x0, rd64(m, 0x9776a0u) /* 3.0517578125e-05 */);
    x0 = cvtsd2ss(x0, x0.lo);
    x0 = mulss(x0, rd32(m, ebx + 0xc8u));
    x0 = addss(x0, rd32(m, ebx + 0xc4u));
    fC_694554 = fcmp32(x1.l(0), x0.l(0));
    wr32(m, esi + 0x48u, x0.l(0));
    if ((fc_b(fC_694554) || fc_z(fC_694554))) goto L_694569;
    eax = 0u;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(12);
L_694569:
    eax = rd32(m, 0x9cb860u);
    wr32(m, 0x9cb864u, rd32(m, 0x9cb864u) + 0x10u);
    eax = uint32_t(int32_t(eax) * int32_t(0x41c64e6du));
    eax = eax + 0x3039u;
    edx = 0u;
L_694582:
    ecx = eax;
    eax = uint32_t(int32_t(eax) * int32_t(0x41c64e6du));
    ecx = (ecx >> 16);
    ecx = (ecx & 0x7fffu);
    eax = eax + 0x3039u;
    wr32(m, esp + 0x10u, ecx);
    fpush(m, reflex_f80::from_int(int64_t(int32_t(sext(rd32(m, esp + 0x10u), 4)))));
    fR_6945a0 = (ecx & ecx);
    if ((sgn(fR_6945a0, 4) == false)) goto L_6945aa;
    st(m, 0) = farith(m, 0, st(m, 0), reflex_f80::from_f32(rd32(m, 0x97731cu) /* 4294967296.0 */));
L_6945aa:
    wr32(m, esp + edx * 4 + 0x30u, fst32(m));
    fpop(m);
    edx = (edx + 1u);
    fA_6945af = edx;
    fB_6945af = 0x10u;
    if (int32_t(sext(fA_6945af, 4)) < int32_t(sext(fB_6945af, 4))) goto L_694582;
    wr32(m, 0x9cb860u, eax);
    fA_6945b9 = rd32(m, ebx + 0x158u);
    fB_6945b9 = 0x0u;
    if (fA_6945b9 == fB_6945b9) goto L_69467b;
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x30u)));
    st(m, 0) = farith(m, 1, st(m, 0), reflex_f80::from_f32(rd32(m, 0x977a90u) /* 3.0517578125e-05 */));
    wr32(m, esp + 0xcu, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0x144u)));
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0xcu)));
    st(m, 1) = farith(m, 1, st(m, 1), st(m, 0));
    wr16(m, esp + 0xcu, uint32_t(m.fpu_cw));
    eax = rd16(m, esp + 0xcu);
    std::swap(st(m, 0), st(m, 1));
    st(m, 0) = farith(m, 0, st(m, 0), reflex_f80::from_f32(rd32(m, ebx + 0x140u)));
    eax = (eax | 0xc00u);
    wr32(m, esp + 0x10u, eax);
    m.fpu_cw = uint16_t(rd16(m, esp + 0x10u));
    wr64(m, esp + 0x10u, uint64_t(fist(m, 8, frc(m))));
    fpop(m);
    edx = rd32(m, esp + 0x10u);
    edx = (edx | 0xffffff00u);
    edx = (edx << 8);
    m.fpu_cw = uint16_t(rd16(m, esp + 0xcu));
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0x14cu)));
    wr16(m, esp + 0xcu, uint32_t(m.fpu_cw));
    st(m, 0) = farith(m, 1, st(m, 0), st(m, 1));
    eax = rd16(m, esp + 0xcu);
    eax = (eax | 0xc00u);
    wr32(m, esp + 0x10u, eax);
    st(m, 0) = farith(m, 0, st(m, 0), reflex_f80::from_f32(rd32(m, ebx + 0x148u)));
    m.fpu_cw = uint16_t(rd16(m, esp + 0x10u));
    wr64(m, esp + 0x10u, uint64_t(fist(m, 8, frc(m))));
    fpop(m);
    eax = rd32(m, esp + 0x10u);
    edx = (edx | eax);
    edx = (edx << 8);
    m.fpu_cw = uint16_t(rd16(m, esp + 0xcu));
    st(m, 0) = farith(m, 1, st(m, 0), reflex_f80::from_f32(rd32(m, ebx + 0x154u)));
    wr16(m, esp + 0xcu, uint32_t(m.fpu_cw));
    st(m, 0) = farith(m, 0, st(m, 0), reflex_f80::from_f32(rd32(m, ebx + 0x150u)));
    eax = rd16(m, esp + 0xcu);
    eax = (eax | 0xc00u);
    wr32(m, esp + 0x10u, eax);
    m.fpu_cw = uint16_t(rd16(m, esp + 0x10u));
    wr64(m, esp + 0x10u, uint64_t(fist(m, 8, frc(m))));
    fpop(m);
    ecx = rd32(m, esp + 0x10u);
    edx = (edx | ecx);
    wr32(m, esi + 0x4cu, edx);
    m.fpu_cw = uint16_t(rd16(m, esp + 0xcu));
L_69467b:
    fA_69467b = rd8(m, ebx + 0x161u);
    fB_69467b = 0x0u;
    if (fA_69467b == fB_69467b) goto L_6946a1;
    eax = rd32(m, ebp + 0xcu);
    fA_694687 = eax;
    fB_694687 = 0xfu;
    if (fA_694687 > fB_694687) goto L_694699;
    edx = rd32(m, esp + 0x28u);
    eax = rd32(m, edx + eax * 4 + 0x20u);
    wr32(m, esi + 0x50u, eax);
    goto L_6946a8;
L_694699:
    eax = (eax | 0xffffffffu);
    wr32(m, esi + 0x50u, eax);
    goto L_6946a8;
L_6946a1:
    wr32(m, esi + 0x50u, 0xffffffffu);
L_6946a8:
    x4 = movss_m(rd32(m, ebx + 0x30u));
    x0 = movss_m(rd32(m, 0x977a90u) /* 3.0517578125e-05 */);
    x1 = movss_m(rd32(m, esp + 0x34u));
    x2 = movss_m(rd32(m, esp + 0x38u));
    x3 = movss_m(rd32(m, esp + 0x3cu));
    x1 = mulss(x1, x0.l(0));
    x4 = mulss(x4, x1.l(0));
    x1 = movss_m(rd32(m, ebx + 0x34u));
    x3 = mulss(x3, x0.l(0));
    x2 = mulss(x2, x0.l(0));
    x1 = mulss(x1, x2.l(0));
    x2 = movss_m(rd32(m, ebx + 0x38u));
    x2 = mulss(x2, x3.l(0));
    x3 = movss_m(rd32(m, ebx + 0x20u));
    x3 = addss(x3, x4.l(0));
    wr32(m, esi, x3.l(0));
    x3 = movss_m(rd32(m, ebx + 0x24u));
    x3 = addss(x3, x1.l(0));
    wr32(m, esi + 0x4u, x3.l(0));
    x1 = movss_m(rd32(m, ebx + 0x28u));
    x1 = addss(x1, x2.l(0));
    wr32(m, esi + 0x8u, x1.l(0));
    x4 = movss_m(rd32(m, ebx + 0x60u));
    x1 = movss_m(rd32(m, esp + 0x40u));
    x2 = movss_m(rd32(m, esp + 0x44u));
    x3 = movss_m(rd32(m, esp + 0x48u));
    x1 = mulss(x1, x0.l(0));
    x4 = mulss(x4, x1.l(0));
    x1 = movss_m(rd32(m, ebx + 0x64u));
    x2 = mulss(x2, x0.l(0));
    x1 = mulss(x1, x2.l(0));
    x2 = movss_m(rd32(m, ebx + 0x68u));
    x3 = mulss(x3, x0.l(0));
    x2 = mulss(x2, x3.l(0));
    x3 = movss_m(rd32(m, ebx + 0x50u));
    eax = (esi + 0x10u);
    x3 = addss(x3, x4.l(0));
    wr32(m, eax, x3.l(0));
    x3 = movss_m(rd32(m, ebx + 0x54u));
    x3 = addss(x3, x1.l(0));
    wr32(m, eax + 0x4u, x3.l(0));
    x1 = movss_m(rd32(m, ebx + 0x58u));
    x1 = addss(x1, x2.l(0));
    wr32(m, eax + 0x8u, x1.l(0));
    x1 = movss_m(rd32(m, eax));
    x1 = addss(x1, rd32(m, ebx + 0x40u));
    x2 = movss_m(rd32(m, esp + 0x4cu));
    x3 = movss_m(rd32(m, esp + 0x50u));
    wr32(m, eax, x1.l(0));
    x1 = movss_m(rd32(m, eax + 0x4u));
    x1 = addss(x1, rd32(m, ebx + 0x44u));
    x4 = movss_m(rd32(m, esp + 0x54u));
    wr32(m, eax + 0x4u, x1.l(0));
    x1 = movss_m(rd32(m, eax + 0x8u));
    x1 = addss(x1, rd32(m, ebx + 0x48u));
    wr32(m, eax + 0x8u, x1.l(0));
    x1 = movss_m(rd32(m, ebx + 0x80u));
    x2 = mulss(x2, x0.l(0));
    x1 = mulss(x1, x2.l(0));
    x1 = addss(x1, rd32(m, ebx + 0x70u));
    x2 = movss_m(rd32(m, ebx + 0x84u));
    x3 = mulss(x3, x0.l(0));
    x2 = mulss(x2, x3.l(0));
    x3 = movss_m(rd32(m, ebx + 0x88u));
    wr32(m, esi + 0x20u, x1.l(0));
    x1 = movss_m(rd32(m, ebx + 0x74u));
    x1 = addss(x1, x2.l(0));
    wr32(m, esi + 0x24u, x1.l(0));
    x1 = movss_m(rd32(m, ebx + 0x78u));
    x4 = mulss(x4, x0.l(0));
    x3 = mulss(x3, x4.l(0));
    x1 = addss(x1, x3.l(0));
    wr32(m, esi + 0x28u, x1.l(0));
    x4 = movss_m(rd32(m, eax + 0x4u));
    x1 = movss_m(rd32(m, eax));
    x5 = movss_m(rd32(m, eax + 0x8u));
    x2 = x4;
    x2 = mulss(x2, rd32(m, edi + 0x10u));
    x3 = x1;
    x3 = mulss(x3, rd32(m, edi));
    x2 = addss(x2, x3.l(0));
    edx = rd32(m, ebp + 0x8u);
    x3 = x5;
    x3 = mulss(x3, rd32(m, edi + 0x20u));
    x2 = addss(x2, x3.l(0));
    x3 = x1;
    x3 = mulss(x3, rd32(m, edi + 0x4u));
    x1 = mulss(x1, rd32(m, edi + 0x8u));
    x6 = x4;
    x6 = mulss(x6, rd32(m, edi + 0x14u));
    x4 = mulss(x4, rd32(m, edi + 0x18u));
    x3 = addss(x3, x6.l(0));
    x6 = x5;
    x6 = mulss(x6, rd32(m, edi + 0x24u));
    x5 = mulss(x5, rd32(m, edi + 0x28u));
    x1 = addss(x1, x4.l(0));
    x3 = addss(x3, x6.l(0));
    x1 = addss(x1, x5.l(0));
    wr32(m, eax, x2.l(0));
    wr32(m, eax + 0x4u, x3.l(0));
    wr32(m, eax + 0x8u, x1.l(0));
    ecx = rd32(m, edx + 0x90u);
    fR_694885 = (ecx & ecx);
    if ((fR_694885 == 0)) goto L_6948d3;
    x1 = movss_m(rd32(m, ebx + 0xe8u));
    x2 = movss_m(rd32(m, ecx));
    x3 = movss_m(rd32(m, ecx + 0x4u));
    x4 = movss_m(rd32(m, ecx + 0x8u));
    x2 = mulss(x2, x1.l(0));
    x3 = mulss(x3, x1.l(0));
    x4 = mulss(x4, x1.l(0));
    x1 = movss_m(rd32(m, eax));
    x1 = addss(x1, x2.l(0));
    wr32(m, eax, x1.l(0));
    x1 = movss_m(rd32(m, eax + 0x4u));
    x1 = addss(x1, x3.l(0));
    wr32(m, eax + 0x4u, x1.l(0));
    x1 = movss_m(rd32(m, eax + 0x8u));
    x1 = addss(x1, x4.l(0));
    wr32(m, eax + 0x8u, x1.l(0));
L_6948d3:
    x4 = movss_m(rd32(m, esi + 0x4u));
    x1 = movss_m(rd32(m, esi));
    x3 = movss_m(rd32(m, edi));
    x5 = movss_m(rd32(m, esi + 0x8u));
    x6 = movss_m(rd32(m, edi + 0x14u));
    x3 = mulss(x3, x1.l(0));
    x2 = x4;
    x2 = mulss(x2, rd32(m, edi + 0x10u));
    x2 = addss(x2, x3.l(0));
    x3 = movss_m(rd32(m, edi + 0x20u));
    x3 = mulss(x3, x5.l(0));
    x2 = addss(x2, x3.l(0));
    x2 = addss(x2, rd32(m, edi + 0x30u));
    x3 = movss_m(rd32(m, edi + 0x4u));
    x3 = mulss(x3, x1.l(0));
    x1 = mulss(x1, rd32(m, edi + 0x8u));
    x6 = mulss(x6, x4.l(0));
    x4 = mulss(x4, rd32(m, edi + 0x18u));
    x3 = addss(x3, x6.l(0));
    x1 = addss(x1, x4.l(0));
    x4 = movss_m(rd32(m, edi + 0x28u));
    x4 = mulss(x4, x5.l(0));
    x1 = addss(x1, x4.l(0));
    x1 = addss(x1, rd32(m, edi + 0x38u));
    x6 = x5;
    x6 = mulss(x6, rd32(m, edi + 0x24u));
    x3 = addss(x3, x6.l(0));
    x3 = addss(x3, rd32(m, edi + 0x34u));
    wr32(m, esi, x2.l(0));
    wr32(m, esi + 0x4u, x3.l(0));
    wr32(m, esi + 0x8u, x1.l(0));
    x4 = movss_m(rd32(m, esi + 0x24u));
    x5 = movss_m(rd32(m, esi + 0x20u));
    x1 = movss_m(rd32(m, edi));
    x2 = movss_m(rd32(m, edi + 0x10u));
    x6 = movss_m(rd32(m, esi + 0x28u));
    x3 = movss_m(rd32(m, edi + 0x14u));
    x2 = mulss(x2, x4.l(0));
    x1 = mulss(x1, x5.l(0));
    x1 = addss(x1, x2.l(0));
    x2 = movss_m(rd32(m, edi + 0x20u));
    x3 = mulss(x3, x4.l(0));
    x2 = mulss(x2, x6.l(0));
    x1 = addss(x1, x2.l(0));
    x2 = movss_m(rd32(m, edi + 0x4u));
    x2 = mulss(x2, x5.l(0));
    x2 = addss(x2, x3.l(0));
    x3 = x6;
    x3 = mulss(x3, rd32(m, edi + 0x24u));
    x2 = addss(x2, x3.l(0));
    x3 = movss_m(rd32(m, edi + 0x8u));
    x3 = mulss(x3, x5.l(0));
    x5 = movss_m(rd32(m, edi + 0x18u));
    x5 = mulss(x5, x4.l(0));
    x4 = movss_m(rd32(m, edi + 0x28u));
    x3 = addss(x3, x5.l(0));
    x4 = mulss(x4, x6.l(0));
    x3 = addss(x3, x4.l(0));
    wr32(m, esi + 0x20u, x1.l(0));
    wr32(m, esi + 0x24u, x2.l(0));
    wr32(m, esi + 0x28u, x3.l(0));
    ecx = rd32(m, edx + 0xa0u);
    fR_6949e7 = (ecx & ecx);
    if ((fR_6949e7 == 0)) goto L_694a17;
    x1 = movss_m(rd32(m, ecx));
    x1 = addss(x1, rd32(m, esi + 0x20u));
    wr32(m, esi + 0x20u, x1.l(0));
    x1 = movss_m(rd32(m, esi + 0x24u));
    x1 = addss(x1, rd32(m, ecx + 0x4u));
    wr32(m, esi + 0x24u, x1.l(0));
    x1 = movss_m(rd32(m, esi + 0x28u));
    x1 = addss(x1, rd32(m, ecx + 0x8u));
    wr32(m, esi + 0x28u, x1.l(0));
L_694a17:
    fA_694a17 = rd32(m, esp + 0x2cu);
    fB_694a17 = 0x0u;
    if (fA_694a17 == fB_694a17) goto L_694a3b;
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0xa0u)));
    wr32(m, esi + 0x70u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0xa4u)));
    wr32(m, esi + 0x74u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0xa8u)));
    wr32(m, esi + 0x78u, fst32(m));
    fpop(m);
    goto L_694a4d;
L_694a3b:
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0x98u)));
    wr32(m, esi + 0x70u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0x9cu)));
    wr32(m, esi + 0x74u, fst32(m));
    fpop(m);
L_694a4d:
    x1 = movss_m(rd32(m, esp + 0x58u));
    x1 = mulss(x1, x0.l(0));
    x1 = mulss(x1, rd32(m, ebx + 0xb4u));
    x1 = addss(x1, rd32(m, ebx + 0xb0u));
    wr32(m, esi + 0x38u, x1.l(0));
    fA_694a6c = rd8(m, ebx + 0xc0u);
    fB_694a6c = 0x0u;
    wr32(m, esp + 0x2cu, x1.l(0));
    if (fA_694a6c == fB_694a6c) goto L_694ab8;
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    edi = eax;
    esi = (esp + 0x10u);
    wr32(m, esp + 0x10u, x0.l(0));
    wr32(m, esp + 0x14u, x0.l(0));
    wr32(m, esp + 0x18u, x0.l(0));
    LIFT_CALL(0x672fe0u, 0x694aa0u);  // FUN_00672fe0
    eax = rd32(m, ebp + 0x10u);
    x0 = addss(x0, rd32(m, esp + 0x2cu));
    wr32(m, eax + 0x38u, x0.l(0));
    x0 = movss_m(rd32(m, 0x977a90u) /* 3.0517578125e-05 */);
    esi = eax;
L_694ab8:
    x1 = movss_m(rd32(m, esp + 0x6cu));
    x2 = movss_m(rd32(m, esp + 0x5cu));
    x3 = movss_m(rd32(m, esp + 0x60u));
    x1 = mulss(x1, x0.l(0));
    x1 = mulss(x1, rd32(m, ebx + 0xbcu));
    x1 = addss(x1, rd32(m, ebx + 0xb8u));
    wr32(m, esi + 0x3cu, x1.l(0));
    x1 = X{};
    wr32(m, esi + 0x40u, x1.l(0));
    fA_694aeb = rd8(m, ebx + 0x128u);
    fB_694aeb = 0x0u;
    x2 = mulss(x2, x0.l(0));
    x3 = mulss(x3, x0.l(0));
    if (fA_694aeb == fB_694aeb) goto L_694aff;
    x3 = x2;
L_694aff:
    x1 = movss_m(rd32(m, ebx + 0x120u));
    ecx = rd32(m, 0xa95844u);
    x1 = mulss(x1, x2.l(0));
    x2 = movss_m(rd32(m, ebx + 0x124u));
    x2 = mulss(x2, x3.l(0));
    x3 = movss_m(rd32(m, ebx + 0x118u));
    x3 = addss(x3, x1.l(0));
    wr32(m, esi + 0x30u, x3.l(0));
    x1 = movss_m(rd32(m, ebx + 0x11cu));
    x1 = addss(x1, x2.l(0));
    wr32(m, esi + 0x34u, x1.l(0));
    x1 = movss_m(rd32(m, esp + 0x64u));
    x1 = mulss(x1, x0.l(0));
    x1 = mulss(x1, rd32(m, ebx + 0xd8u));
    x1 = addss(x1, rd32(m, ebx + 0xd4u));
    wr32(m, esi + 0x54u, x1.l(0));
    x1 = movss_m(rd32(m, esp + 0x68u));
    x1 = mulss(x1, x0.l(0));
    x1 = mulss(x1, rd32(m, ebx + 0xe0u));
    x1 = addss(x1, rd32(m, ebx + 0xdcu));
    wr32(m, esi + 0x58u, x1.l(0));
    eax = rd32(m, ecx + 0x10u);
    LIFT_CALL(0x65ca30u, 0x694b85u);  // FUN_0065ca30
    edi = rd32(m, esp); esp += 4;
    wr32(m, esi + 0x44u, x0.l(0));
    esi = rd32(m, esp); esp += 4;
    eax = 0x1u;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(12);
}

// 00694ba0 Fn_00694ba0
//   original FUN_00694ba0; register inputs: ecx; returns with ret 16
REFLEX_PORT_FN(0x00694ba0, Fn_00694ba0, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fA_694bb1 = 0;
    uint32_t fB_694bb1 = 0;
    uint32_t fA_694bc6 = 0;
    uint32_t fB_694bc6 = 0;
    uint32_t fA_694bdb = 0;
    uint32_t fB_694bdb = 0;
    uint32_t fA_694bef = 0;
    uint32_t fB_694bef = 0;
    uint32_t fA_694c25 = 0;
    uint32_t fB_694c25 = 0;
    FCmp fC_694c79{};
    uint32_t fA_694c8b = 0;
    uint32_t fB_694c8b = 0;
    uint32_t fA_694d74 = 0;
    uint32_t fB_694d74 = 0;
    uint32_t fA_694d80 = 0;
    uint32_t fB_694d80 = 0;
    FCmp fC_694f43{};
    uint32_t fR_6950b1 = 0;
    uint32_t fA_6950ef = 0;
    uint32_t fB_6950ef = 0;
    uint32_t fA_6952df = 0;
    uint32_t fB_6952df = 0;
    uint32_t fA_695458 = 0;
    uint32_t fB_695458 = 0;
    uint32_t fA_695545 = 0;
    uint32_t fB_695545 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    eax = rd32(m, 0xa95844u);
    edx = rd32(m, eax + 0x10u);
    esp = esp - 0x24u;
    fA_694bb1 = rd32(m, edx + 0xcu);
    fB_694bb1 = 0x0u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    if (fA_694bb1 != fB_694bb1) goto L_695643;
    edx = rd32(m, eax + 0x20u);
    esi = 0x1u;
    fA_694bc6 = rd32(m, edx + 0x34u);
    fB_694bc6 = esi;
    if (fA_694bc6 != fB_694bc6) goto L_694bd8;
    eax = rd32(m, 0xa946b8u);
    eax = rd32(m, eax + 0x5a54u);
    goto L_694bef;
L_694bd8:
    edx = rd32(m, eax + 0x1cu);
    fA_694bdb = rd32(m, edx + 0x34u);
    fB_694bdb = esi;
    if (fA_694bdb != fB_694bdb) goto L_694bed;
    eax = rd32(m, 0xa946b8u);
    eax = rd32(m, eax + 0x20c8u);
    goto L_694bef;
L_694bed:
    eax = 0u;
L_694bef:
    fA_694bef = rd32(m, eax + 0x20014u);
    fB_694bef = 0x0u;
    if (fA_694bef != fB_694bef) goto L_695643;
    edx = rd32(m, ebp + 0x8u);
    eax = rd32(m, edx + 0x14u);
    edi = rd32(m, eax + 0x44u);
    wr32(m, eax + 0x4cu, esi);
    ebx = rd32(m, edx + 0x18u);
    edx = rd32(m, ecx);
    ebx = uint32_t(int32_t(ebx) * int32_t(0x1f0u));
    eax = rd32(m, edx + 0x1cu);
    ebx = ebx + rd32(m, edi + 0x10u);
    LIFT_CALL(eax, 0x694c1bu);
    x2 = movss_m(rd32(m, 0x977954u) /* 0.01666666753590107 */);
    ecx = 0u;
    fA_694c25 = eax;
    fB_694c25 = 0x2u;
    eax = rd32(m, 0x9cb860u);
    ecx = (ecx & 0xffffff00u) | (((fA_694c25 == fB_694c25) ? 1u : 0u) & 0xffu);
    eax = uint32_t(int32_t(eax) * int32_t(0x41c64e6du));
    wr32(m, 0x9cb864u, rd32(m, 0x9cb864u) + esi);
    esi = rd32(m, ebp + 0x14u);
    eax = eax + 0x3039u;
    wr32(m, 0x9cb860u, eax);
    eax = (eax >> 16);
    eax = (eax & 0x7fffu);
    x0 = cvtsi2ss(x0, eax);
    x1 = cvtss2sd(x1, x0.l(0));
    x0 = movsd_m(rd64(m, 0x9776a0u) /* 3.0517578125e-05 */);
    x1 = mulsd(x1, x0.lo);
    x1 = cvtsd2ss(x1, x1.lo);
    x1 = mulss(x1, rd32(m, ebx + 0xc8u));
    x1 = addss(x1, rd32(m, ebx + 0xc4u));
    fC_694c79 = fcmp32(x2.l(0), x1.l(0));
    wr32(m, esp + 0x2cu, ecx);
    wr32(m, esi + 0x48u, x1.l(0));
    if (!(fc_b(fC_694c79) || fc_z(fC_694c79))) goto L_695643;
    fA_694c8b = rd32(m, ebx + 0x158u);
    fB_694c8b = 0x0u;
    if (fA_694c8b == fB_694c8b) goto L_694d74;
    eax = rd32(m, 0x9cb860u);
    wr32(m, 0x9cb864u, (rd32(m, 0x9cb864u) + 1u));
    eax = uint32_t(int32_t(eax) * int32_t(0x41c64e6du));
    eax = eax + 0x3039u;
    wr32(m, 0x9cb860u, eax);
    eax = (eax >> 16);
    eax = (eax & 0x7fffu);
    wr32(m, esp + 0xcu, eax);
    fpush(m, reflex_f80::from_int(int64_t(int32_t(sext(rd32(m, esp + 0xcu), 4)))));
    st(m, 0) = farith(m, 1, st(m, 0), reflex_f80::from_f64(rd64(m, 0x9776a0u) /* 3.0517578125e-05 */));
    wr32(m, esp + 0xcu, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0x144u)));
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0xcu)));
    st(m, 1) = farith(m, 1, st(m, 1), st(m, 0));
    wr16(m, esp + 0xcu, uint32_t(m.fpu_cw));
    eax = rd16(m, esp + 0xcu);
    std::swap(st(m, 0), st(m, 1));
    st(m, 0) = farith(m, 0, st(m, 0), reflex_f80::from_f32(rd32(m, ebx + 0x140u)));
    eax = (eax | 0xc00u);
    wr32(m, esp + 0x10u, eax);
    m.fpu_cw = uint16_t(rd16(m, esp + 0x10u));
    wr64(m, esp + 0x10u, uint64_t(fist(m, 8, frc(m))));
    fpop(m);
    edx = rd32(m, esp + 0x10u);
    edx = (edx | 0xffffff00u);
    edx = (edx << 8);
    m.fpu_cw = uint16_t(rd16(m, esp + 0xcu));
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0x14cu)));
    wr16(m, esp + 0xcu, uint32_t(m.fpu_cw));
    st(m, 0) = farith(m, 1, st(m, 0), st(m, 1));
    eax = rd16(m, esp + 0xcu);
    eax = (eax | 0xc00u);
    wr32(m, esp + 0x10u, eax);
    st(m, 0) = farith(m, 0, st(m, 0), reflex_f80::from_f32(rd32(m, ebx + 0x148u)));
    m.fpu_cw = uint16_t(rd16(m, esp + 0x10u));
    wr64(m, esp + 0x10u, uint64_t(fist(m, 8, frc(m))));
    fpop(m);
    eax = rd32(m, esp + 0x10u);
    edx = (edx | eax);
    edx = (edx << 8);
    m.fpu_cw = uint16_t(rd16(m, esp + 0xcu));
    st(m, 0) = farith(m, 1, st(m, 0), reflex_f80::from_f32(rd32(m, ebx + 0x154u)));
    wr16(m, esp + 0xcu, uint32_t(m.fpu_cw));
    st(m, 0) = farith(m, 0, st(m, 0), reflex_f80::from_f32(rd32(m, ebx + 0x150u)));
    eax = rd16(m, esp + 0xcu);
    eax = (eax | 0xc00u);
    wr32(m, esp + 0x10u, eax);
    m.fpu_cw = uint16_t(rd16(m, esp + 0x10u));
    wr64(m, esp + 0x10u, uint64_t(fist(m, 8, frc(m))));
    fpop(m);
    ecx = rd32(m, esp + 0x10u);
    edx = (edx | ecx);
    wr32(m, esi + 0x4cu, edx);
    m.fpu_cw = uint16_t(rd16(m, esp + 0xcu));
L_694d74:
    fA_694d74 = rd8(m, ebx + 0x161u);
    fB_694d74 = 0x0u;
    if (fA_694d74 == fB_694d74) goto L_694d96;
    eax = rd32(m, ebp + 0x10u);
    fA_694d80 = eax;
    fB_694d80 = 0xfu;
    if (fA_694d80 > fB_694d80) goto L_694d8e;
    edi = rd32(m, edi + eax * 4 + 0x20u);
    wr32(m, esi + 0x50u, edi);
    goto L_694d9d;
L_694d8e:
    edi = (edi | 0xffffffffu);
    wr32(m, esi + 0x50u, edi);
    goto L_694d9d;
L_694d96:
    wr32(m, esi + 0x50u, 0xffffffffu);
L_694d9d:
    eax = rd32(m, 0x9cb860u);
    eax = uint32_t(int32_t(eax) * int32_t(0x41c64e6du));
    eax = eax + 0x3039u;
    edx = eax;
    eax = uint32_t(int32_t(eax) * int32_t(0x41c64e6du));
    ecx = rd32(m, 0x9cb864u);
    edx = (edx >> 16);
    eax = eax + 0x3039u;
    edx = (edx & 0x7fffu);
    x1 = cvtsi2ss(x1, edx);
    edx = eax;
    eax = uint32_t(int32_t(eax) * int32_t(0x41c64e6du));
    x1 = cvtss2sd(x1, x1.l(0));
    x1 = mulsd(x1, x0.lo);
    x4 = cvtsd2ss(x4, x1.lo);
    eax = eax + 0x3039u;
    wr32(m, 0x9cb860u, eax);
    edx = (edx >> 16);
    edx = (edx & 0x7fffu);
    eax = (eax >> 16);
    eax = (eax & 0x7fffu);
    x1 = cvtsi2ss(x1, edx);
    x1 = cvtss2sd(x1, x1.l(0));
    x1 = mulsd(x1, x0.lo);
    x3 = cvtsd2ss(x3, x1.lo);
    ecx = (ecx + 1u);
    x1 = cvtsi2ss(x1, eax);
    x1 = cvtss2sd(x1, x1.l(0));
    x1 = mulsd(x1, x0.lo);
    x2 = cvtsd2ss(x2, x1.lo);
    ecx = (ecx + 1u);
    ecx = (ecx + 1u);
    wr32(m, 0x9cb864u, ecx);
    x1 = movss_m(rd32(m, ebx + 0x30u));
    x1 = mulss(x1, x2.l(0));
    x2 = movss_m(rd32(m, ebx + 0x34u));
    x2 = mulss(x2, x3.l(0));
    x3 = movss_m(rd32(m, ebx + 0x38u));
    x3 = mulss(x3, x4.l(0));
    x4 = movss_m(rd32(m, ebx + 0x20u));
    x4 = addss(x4, x1.l(0));
    wr32(m, esi, x4.l(0));
    x1 = movss_m(rd32(m, ebx + 0x24u));
    x1 = addss(x1, x2.l(0));
    wr32(m, esi + 0x4u, x1.l(0));
    x1 = movss_m(rd32(m, ebx + 0x28u));
    x1 = addss(x1, x3.l(0));
    wr32(m, esi + 0x8u, x1.l(0));
    eax = rd32(m, 0x9cb860u);
    eax = uint32_t(int32_t(eax) * int32_t(0x41c64e6du));
    ecx = rd32(m, 0x9cb864u);
    eax = eax + 0x3039u;
    edx = eax;
    eax = uint32_t(int32_t(eax) * int32_t(0x41c64e6du));
    edx = (edx >> 16);
    edx = (edx & 0x7fffu);
    eax = eax + 0x3039u;
    x1 = cvtsi2ss(x1, edx);
    edx = eax;
    eax = uint32_t(int32_t(eax) * int32_t(0x41c64e6du));
    ecx = (ecx + 1u);
    eax = eax + 0x3039u;
    ecx = (ecx + 1u);
    edx = (edx >> 16);
    wr32(m, 0x9cb860u, eax);
    edx = (edx & 0x7fffu);
    ecx = (ecx + 1u);
    eax = (eax >> 16);
    eax = (eax & 0x7fffu);
    x1 = cvtps2pd(x1.lo);
    x2 = cvtsi2ss(x2, edx);
    x2 = cvtps2pd(x2.lo);
    x3 = cvtsi2ss(x3, eax);
    x1 = mulsd(x1, x0.lo);
    x2 = mulsd(x2, x0.lo);
    x3 = cvtps2pd(x3.lo);
    x1 = cvtpd2ps(x1);
    x2 = cvtpd2ps(x2);
    wr32(m, 0x9cb864u, ecx);
    edi = (esi + 0x10u);
    x3 = mulsd(x3, x0.lo);
    x6 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x4 = cvtpd2ps(x3);
    x3 = movss_m(rd32(m, 0x9322ccu) /* 0.5 */);
    x1 = subss(x1, x3.l(0));
    x4 = subss(x4, x3.l(0));
    x2 = subss(x2, x3.l(0));
    wr32(m, edi + 0x8u, x1.l(0));
    x1 = x4;
    wr32(m, edi, x4.l(0));
    wr32(m, edi + 0x4u, x2.l(0));
    x3 = movss_m(rd32(m, edi + 0x8u));
    x4 = x2;
    wr32(m, esp + 0xcu, x1.l(0));
    x4 = mulss(x4, x2.l(0));
    x1 = mulss(x1, x1.l(0));
    x2 = x3;
    x1 = addss(x1, x4.l(0));
    x2 = mulss(x2, x3.l(0));
    x1 = addss(x1, x2.l(0));
    fC_694f43 = fcmp32(x1.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    wr32(m, esp + 0x10u, x1.l(0));
    if ((fc_b(fC_694f43) || fc_z(fC_694f43))) goto L_694fa6;
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x10u)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x42c230u, 0x694f5fu);  // FUN_0042c230
    x6 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x1 = x0;
    x0 = x6;
    x0 = divss(x0, x1.l(0));
    x1 = movss_m(rd32(m, esp + 0x10u));
    x1 = mulss(x1, x0.l(0));
    wr32(m, edi, x1.l(0));
    x1 = movss_m(rd32(m, edi + 0x4u));
    x1 = mulss(x1, x0.l(0));
    wr32(m, edi + 0x4u, x1.l(0));
    x1 = movss_m(rd32(m, edi + 0x8u));
    x1 = mulss(x1, x0.l(0));
    x0 = movsd_m(rd64(m, 0x9776a0u) /* 3.0517578125e-05 */);
    esp = esp + 0x4u;
    wr32(m, edi + 0x8u, x1.l(0));
L_694fa6:
    x1 = movss_m(rd32(m, ebx + 0x40u));
    x1 = addss(x1, rd32(m, edi));
    wr32(m, edi, x1.l(0));
    x1 = movss_m(rd32(m, ebx + 0x44u));
    x1 = addss(x1, rd32(m, edi + 0x4u));
    wr32(m, edi + 0x4u, x1.l(0));
    x1 = movss_m(rd32(m, ebx + 0x48u));
    x1 = addss(x1, rd32(m, edi + 0x8u));
    wr32(m, edi + 0x8u, x1.l(0));
    eax = rd32(m, 0x9cb860u);
    eax = uint32_t(int32_t(eax) * int32_t(0x41c64e6du));
    ecx = rd32(m, 0x9cb864u);
    eax = eax + 0x3039u;
    edx = eax;
    eax = uint32_t(int32_t(eax) * int32_t(0x41c64e6du));
    edx = (edx >> 16);
    edx = (edx & 0x7fffu);
    eax = eax + 0x3039u;
    x1 = cvtsi2ss(x1, edx);
    edx = eax;
    eax = uint32_t(int32_t(eax) * int32_t(0x41c64e6du));
    x1 = cvtps2pd(x1.lo);
    x1 = mulsd(x1, x0.lo);
    x4 = cvtpd2ps(x1);
    edx = (edx >> 16);
    eax = eax + 0x3039u;
    wr32(m, 0x9cb860u, eax);
    ecx = (ecx + 1u);
    edx = (edx & 0x7fffu);
    ecx = (ecx + 1u);
    x1 = cvtsi2ss(x1, edx);
    x1 = cvtss2sd(x1, x1.l(0));
    x1 = mulsd(x1, x0.lo);
    x3 = cvtpd2ps(x1);
    eax = (eax >> 16);
    edx = 0x1u;
    ecx = ecx + edx;
    wr32(m, 0x9cb864u, ecx);
    x2 = movss_m(rd32(m, ebx + 0x84u));
    ecx = rd32(m, ebp + 0x8u);
    eax = (eax & 0x7fffu);
    x1 = cvtsi2ss(x1, eax);
    x1 = cvtss2sd(x1, x1.l(0));
    x1 = mulsd(x1, x0.lo);
    x1 = cvtpd2ps(x1);
    x1 = mulss(x1, rd32(m, ebx + 0x80u));
    x2 = mulss(x2, x3.l(0));
    x3 = movss_m(rd32(m, ebx + 0x88u));
    x3 = mulss(x3, x4.l(0));
    x4 = movss_m(rd32(m, ebx + 0x70u));
    x4 = addss(x4, x1.l(0));
    wr32(m, esi + 0x20u, x4.l(0));
    x1 = movss_m(rd32(m, ebx + 0x74u));
    x1 = addss(x1, x2.l(0));
    wr32(m, esi + 0x24u, x1.l(0));
    x1 = movss_m(rd32(m, ebx + 0x78u));
    x1 = addss(x1, x3.l(0));
    wr32(m, esi + 0x28u, x1.l(0));
    eax = rd32(m, ecx + 0xa0u);
    fR_6950b1 = (eax & eax);
    if ((fR_6950b1 == 0)) goto L_6950df;
    x1 = x4;
    x1 = addss(x1, rd32(m, eax));
    wr32(m, esi + 0x20u, x1.l(0));
    x1 = movss_m(rd32(m, esi + 0x24u));
    x1 = addss(x1, rd32(m, eax + 0x4u));
    wr32(m, esi + 0x24u, x1.l(0));
    x1 = movss_m(rd32(m, esi + 0x28u));
    x1 = addss(x1, rd32(m, eax + 0x8u));
    wr32(m, esi + 0x28u, x1.l(0));
L_6950df:
    eax = rd32(m, 0x9cb860u);
    eax = uint32_t(int32_t(eax) * int32_t(0x41c64e6du));
    eax = eax + 0x3039u;
    fA_6950ef = rd32(m, ecx + 0x90u);
    fB_6950ef = 0x0u;
    wr32(m, 0x9cb860u, eax);
    if (fA_6950ef != fB_6950ef) goto L_6951c5;
    wr32(m, 0x9cb864u, rd32(m, 0x9cb864u) + edx);
    x2 = movss_m(rd32(m, edi));
    eax = (eax >> 16);
    eax = (eax & 0x7fffu);
    x1 = cvtsi2ss(x1, eax);
    eax = rd32(m, ebp + 0xcu);
    x1 = cvtps2pd(x1.lo);
    x1 = mulsd(x1, x0.lo);
    x1 = cvtpd2ps(x1);
    x1 = mulss(x1, rd32(m, ebx + 0x100u));
    x1 = addss(x1, rd32(m, ebx + 0xfcu));
    x2 = mulss(x2, x1.l(0));
    wr32(m, edi, x2.l(0));
    x2 = x1;
    x1 = mulss(x1, rd32(m, edi + 0x8u));
    x2 = mulss(x2, rd32(m, edi + 0x4u));
    wr32(m, edi + 0x8u, x1.l(0));
    wr32(m, edi + 0x4u, x2.l(0));
    x1 = movss_m(rd32(m, edi));
    x3 = movss_m(rd32(m, edi + 0x8u));
    x4 = movss_m(rd32(m, eax));
    x7 = movss_m(rd32(m, eax + 0x14u));
    x4 = mulss(x4, x1.l(0));
    x5 = x2;
    x5 = mulss(x5, rd32(m, eax + 0x10u));
    x4 = addss(x4, x5.l(0));
    x5 = x3;
    x5 = mulss(x5, rd32(m, eax + 0x20u));
    x4 = addss(x4, x5.l(0));
    x5 = movss_m(rd32(m, eax + 0x4u));
    x5 = mulss(x5, x1.l(0));
    x1 = mulss(x1, rd32(m, eax + 0x8u));
    x7 = mulss(x7, x2.l(0));
    x2 = mulss(x2, rd32(m, eax + 0x18u));
    x5 = addss(x5, x7.l(0));
    x7 = movss_m(rd32(m, eax + 0x24u));
    x7 = mulss(x7, x3.l(0));
    x3 = mulss(x3, rd32(m, eax + 0x28u));
    x5 = addss(x5, x7.l(0));
    x1 = addss(x1, x2.l(0));
    x1 = addss(x1, x3.l(0));
    wr32(m, edi, x4.l(0));
    wr32(m, edi + 0x4u, x5.l(0));
    goto L_6952df;
L_6951c5:
    wr32(m, 0x9cb864u, rd32(m, 0x9cb864u) + edx);
    eax = (eax >> 16);
    eax = (eax & 0x7fffu);
    x1 = cvtsi2ss(x1, eax);
    eax = rd32(m, ebp + 0xcu);
    x1 = cvtps2pd(x1.lo);
    x1 = mulsd(x1, x0.lo);
    x1 = cvtpd2ps(x1);
    x1 = mulss(x1, rd32(m, ebx + 0x100u));
    x1 = addss(x1, rd32(m, ebx + 0xfcu));
    x2 = x1;
    x2 = mulss(x2, rd32(m, edi));
    wr32(m, edi, x2.l(0));
    x2 = movss_m(rd32(m, edi + 0x4u));
    x2 = mulss(x2, x1.l(0));
    wr32(m, edi + 0x4u, x2.l(0));
    x2 = movss_m(rd32(m, edi + 0x8u));
    x2 = mulss(x2, x1.l(0));
    wr32(m, edi + 0x8u, x2.l(0));
    x2 = movss_m(rd32(m, edi + 0x4u));
    x1 = movss_m(rd32(m, edi));
    x4 = movss_m(rd32(m, eax));
    x3 = movss_m(rd32(m, edi + 0x8u));
    x7 = movss_m(rd32(m, eax + 0x14u));
    x4 = mulss(x4, x1.l(0));
    x5 = x2;
    x5 = mulss(x5, rd32(m, eax + 0x10u));
    x4 = addss(x4, x5.l(0));
    x5 = x3;
    x5 = mulss(x5, rd32(m, eax + 0x20u));
    x4 = addss(x4, x5.l(0));
    x5 = movss_m(rd32(m, eax + 0x4u));
    x5 = mulss(x5, x1.l(0));
    x1 = mulss(x1, rd32(m, eax + 0x8u));
    x7 = mulss(x7, x2.l(0));
    x2 = mulss(x2, rd32(m, eax + 0x18u));
    x1 = addss(x1, x2.l(0));
    x5 = addss(x5, x7.l(0));
    x7 = movss_m(rd32(m, eax + 0x24u));
    x7 = mulss(x7, x3.l(0));
    x3 = mulss(x3, rd32(m, eax + 0x28u));
    x1 = addss(x1, x3.l(0));
    wr32(m, edi + 0x8u, x1.l(0));
    wr32(m, edi, x4.l(0));
    x5 = addss(x5, x7.l(0));
    wr32(m, edi + 0x4u, x5.l(0));
    eax = rd32(m, ecx + 0x90u);
    x1 = movss_m(rd32(m, ebx + 0xe8u));
    x3 = movss_m(rd32(m, eax + 0x4u));
    x4 = movss_m(rd32(m, eax + 0x8u));
    x2 = x1;
    x2 = mulss(x2, rd32(m, eax));
    eax = rd32(m, ebp + 0xcu);
    x3 = mulss(x3, x1.l(0));
    x4 = mulss(x4, x1.l(0));
    x1 = movss_m(rd32(m, edi));
    x1 = addss(x1, x2.l(0));
    wr32(m, edi, x1.l(0));
    x1 = movss_m(rd32(m, edi + 0x4u));
    x1 = addss(x1, x3.l(0));
    wr32(m, edi + 0x4u, x1.l(0));
    x1 = movss_m(rd32(m, edi + 0x8u));
    x1 = addss(x1, x4.l(0));
L_6952df:
    fA_6952df = rd32(m, esp + 0x2cu);
    fB_6952df = 0x0u;
    wr32(m, edi + 0x8u, x1.l(0));
    x4 = movss_m(rd32(m, esi + 0x4u));
    x1 = movss_m(rd32(m, esi));
    x5 = movss_m(rd32(m, esi + 0x8u));
    x3 = x1;
    x3 = mulss(x3, rd32(m, eax));
    x2 = x4;
    x2 = mulss(x2, rd32(m, eax + 0x10u));
    x2 = addss(x2, x3.l(0));
    x3 = x5;
    x3 = mulss(x3, rd32(m, eax + 0x20u));
    x2 = addss(x2, x3.l(0));
    x2 = addss(x2, rd32(m, eax + 0x30u));
    x3 = x1;
    x3 = mulss(x3, rd32(m, eax + 0x4u));
    x1 = mulss(x1, rd32(m, eax + 0x8u));
    x7 = x4;
    x7 = mulss(x7, rd32(m, eax + 0x14u));
    x4 = mulss(x4, rd32(m, eax + 0x18u));
    x3 = addss(x3, x7.l(0));
    x1 = addss(x1, x4.l(0));
    x7 = x5;
    x7 = mulss(x7, rd32(m, eax + 0x24u));
    x5 = mulss(x5, rd32(m, eax + 0x28u));
    x1 = addss(x1, x5.l(0));
    x1 = addss(x1, rd32(m, eax + 0x38u));
    x3 = addss(x3, x7.l(0));
    x3 = addss(x3, rd32(m, eax + 0x34u));
    wr32(m, esi, x2.l(0));
    wr32(m, esi + 0x8u, x1.l(0));
    wr32(m, esi + 0x4u, x3.l(0));
    x3 = movss_m(rd32(m, esi + 0x24u));
    x1 = movss_m(rd32(m, esi + 0x20u));
    x4 = movss_m(rd32(m, esi + 0x28u));
    x5 = movss_m(rd32(m, eax));
    x5 = mulss(x5, x1.l(0));
    x2 = x3;
    x2 = mulss(x2, rd32(m, eax + 0x10u));
    x5 = addss(x5, x2.l(0));
    x2 = x4;
    x2 = mulss(x2, rd32(m, eax + 0x20u));
    x5 = addss(x5, x2.l(0));
    x2 = x1;
    x2 = mulss(x2, rd32(m, eax + 0x4u));
    x1 = mulss(x1, rd32(m, eax + 0x8u));
    x7 = x3;
    x7 = mulss(x7, rd32(m, eax + 0x14u));
    x3 = mulss(x3, rd32(m, eax + 0x18u));
    x2 = addss(x2, x7.l(0));
    x7 = movss_m(rd32(m, eax + 0x24u));
    x7 = mulss(x7, x4.l(0));
    x4 = mulss(x4, rd32(m, eax + 0x28u));
    x1 = addss(x1, x3.l(0));
    x2 = addss(x2, x7.l(0));
    x1 = addss(x1, x4.l(0));
    wr32(m, esi + 0x20u, x5.l(0));
    wr32(m, esi + 0x24u, x2.l(0));
    wr32(m, esi + 0x28u, x1.l(0));
    if (fA_6952df == fB_6952df) goto L_6953ff;
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0xa0u)));
    wr32(m, esi + 0x70u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0xa4u)));
    wr32(m, esi + 0x74u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0xa8u)));
    wr32(m, esi + 0x78u, fst32(m));
    fpop(m);
    goto L_695411;
L_6953ff:
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0x98u)));
    wr32(m, esi + 0x70u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0x9cu)));
    wr32(m, esi + 0x74u, fst32(m));
    fpop(m);
L_695411:
    eax = rd32(m, 0x9cb860u);
    wr32(m, 0x9cb864u, rd32(m, 0x9cb864u) + edx);
    eax = uint32_t(int32_t(eax) * int32_t(0x41c64e6du));
    eax = eax + 0x3039u;
    wr32(m, 0x9cb860u, eax);
    eax = (eax >> 16);
    eax = (eax & 0x7fffu);
    x1 = cvtsi2ss(x1, eax);
    x1 = cvtps2pd(x1.lo);
    x1 = mulsd(x1, x0.lo);
    x1 = cvtpd2ps(x1);
    x1 = mulss(x1, rd32(m, ebx + 0xb4u));
    x1 = addss(x1, rd32(m, ebx + 0xb0u));
    wr32(m, esi + 0x38u, x1.l(0));
    fA_695458 = rd8(m, ebx + 0xc0u);
    fB_695458 = 0x0u;
    wr32(m, esp + 0x2cu, x1.l(0));
    if (fA_695458 == fB_695458) goto L_69549f;
    esi = (esp + 0x10u);
    wr32(m, esp + 0x10u, x6.l(0));
    wr32(m, esp + 0x14u, x6.l(0));
    wr32(m, esp + 0x18u, x6.l(0));
    LIFT_CALL(0x672fe0u, 0x695482u);  // FUN_00672fe0
    eax = rd32(m, ebp + 0x14u);
    x0 = addss(x0, rd32(m, esp + 0x2cu));
    wr32(m, eax + 0x38u, x0.l(0));
    x0 = movsd_m(rd64(m, 0x9776a0u) /* 3.0517578125e-05 */);
    esi = eax;
    edx = 0x1u;
L_69549f:
    eax = rd32(m, 0x9cb860u);
    wr32(m, 0x9cb864u, rd32(m, 0x9cb864u) + edx);
    eax = uint32_t(int32_t(eax) * int32_t(0x41c64e6du));
    eax = eax + 0x3039u;
    wr32(m, 0x9cb860u, eax);
    eax = (eax >> 16);
    eax = (eax & 0x7fffu);
    x1 = cvtsi2ss(x1, eax);
    x1 = cvtss2sd(x1, x1.l(0));
    x1 = mulsd(x1, x0.lo);
    x1 = cvtsd2ss(x1, x1.lo);
    x1 = mulss(x1, rd32(m, ebx + 0xbcu));
    x1 = addss(x1, rd32(m, ebx + 0xb8u));
    wr32(m, esi + 0x3cu, x1.l(0));
    x1 = X{};
    wr32(m, esi + 0x40u, x1.l(0));
    eax = rd32(m, 0x9cb860u);
    ecx = rd32(m, 0x9cb864u);
    eax = uint32_t(int32_t(eax) * int32_t(0x41c64e6du));
    eax = eax + 0x3039u;
    edi = eax;
    eax = uint32_t(int32_t(eax) * int32_t(0x41c64e6du));
    eax = eax + 0x3039u;
    edi = (edi >> 16);
    edi = (edi & 0x7fffu);
    wr32(m, 0x9cb860u, eax);
    eax = (eax >> 16);
    ecx = ecx + edx;
    x1 = cvtsi2ss(x1, edi);
    x1 = cvtps2pd(x1.lo);
    x1 = mulsd(x1, x0.lo);
    x3 = cvtpd2ps(x1);
    eax = (eax & 0x7fffu);
    ecx = ecx + edx;
    x1 = cvtsi2ss(x1, eax);
    wr32(m, 0x9cb864u, ecx);
    fA_695545 = rd8(m, ebx + 0x128u);
    fB_695545 = 0x0u;
    x1 = cvtss2sd(x1, x1.l(0));
    x1 = mulsd(x1, x0.lo);
    x2 = cvtpd2ps(x1);
    if (fA_695545 == fB_695545) goto L_69555d;
    x3 = x2;
L_69555d:
    x1 = movss_m(rd32(m, ebx + 0x120u));
    x1 = mulss(x1, x2.l(0));
    x1 = addss(x1, rd32(m, ebx + 0x118u));
    x2 = movss_m(rd32(m, ebx + 0x124u));
    wr32(m, esi + 0x30u, x1.l(0));
    x1 = movss_m(rd32(m, ebx + 0x11cu));
    x2 = mulss(x2, x3.l(0));
    x1 = addss(x1, x2.l(0));
    wr32(m, esi + 0x34u, x1.l(0));
    eax = rd32(m, 0x9cb860u);
    wr32(m, 0x9cb864u, rd32(m, 0x9cb864u) + edx);
    eax = uint32_t(int32_t(eax) * int32_t(0x41c64e6du));
    eax = eax + 0x3039u;
    wr32(m, 0x9cb860u, eax);
    eax = (eax >> 16);
    eax = (eax & 0x7fffu);
    x1 = cvtsi2ss(x1, eax);
    x1 = cvtps2pd(x1.lo);
    x1 = mulsd(x1, x0.lo);
    x1 = cvtpd2ps(x1);
    x1 = mulss(x1, rd32(m, ebx + 0xd8u));
    x1 = addss(x1, rd32(m, ebx + 0xd4u));
    wr32(m, esi + 0x54u, x1.l(0));
    eax = rd32(m, 0x9cb860u);
    wr32(m, 0x9cb864u, rd32(m, 0x9cb864u) + edx);
    eax = uint32_t(int32_t(eax) * int32_t(0x41c64e6du));
    ecx = rd32(m, 0xa95844u);
    eax = eax + 0x3039u;
    wr32(m, 0x9cb860u, eax);
    eax = (eax >> 16);
    eax = (eax & 0x7fffu);
    x1 = cvtsi2ss(x1, eax);
    x1 = cvtss2sd(x1, x1.l(0));
    x1 = mulsd(x1, x0.lo);
    x0 = cvtpd2ps(x1);
    x0 = mulss(x0, rd32(m, ebx + 0xe0u));
    x0 = addss(x0, rd32(m, ebx + 0xdcu));
    wr32(m, esi + 0x58u, x0.l(0));
    eax = rd32(m, ecx + 0x10u);
    LIFT_CALL(0x65ca30u, 0x695630u);  // FUN_0065ca30
    wr32(m, esi + 0x44u, x0.l(0));
    eax = 0x1u;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(16);
L_695643:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(16);
}

// 00695650 Fn_00695650
//   original FUN_00695650; register inputs: eax edi; returns with ret 0
REFLEX_PORT_FN(0x00695650, Fn_00695650, "fx", 0) {
    LIFT_ENTRY();
    uint32_t fA_695688 = 0;
    uint32_t fB_695688 = 0;
    uint32_t fR_695692 = 0;
    uint32_t fA_69569a = 0;
    uint32_t fB_69569a = 0;
    FCmp fC_6956e4{};
    uint32_t fA_69575b = 0;
    uint32_t fB_69575b = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x1cu;
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    x0 = movss_m(rd32(m, esi + 0x60u));
    wr32(m, esp + 0x10u, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x64u));
    eax = (esp + 0x10u);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0x18u, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x68u));
    esp -= 4; wr32(m, esp, esi);
    wr32(m, esp + 0x20u, x0.l(0));
    LIFT_CALL(0x695b00u, 0x695688u);  // FUN_00695b00
    fA_695688 = rd32(m, esi + 0x1cu);
    fB_695688 = 0x0u;
    if (fA_695688 == fB_695688) goto L_69575b;
    fR_695692 = (eax & eax);
    if ((fR_695692 == 0)) goto L_69575b;
    fA_69569a = rd32(m, esi + 0x7cu);
    fB_69569a = 0x0u;
    if (fA_69569a != fB_69569a) goto L_69576e;
    ecx = rd32(m, esi + 0x14u);
    x1 = movss_m(rd32(m, 0x9322ccu) /* 0.5 */);
    wr32(m, ecx + 0x4cu, 0x1u);
    eax = rd32(m, esi + 0x18u);
    ecx = rd32(m, ecx + 0x44u);
    eax = uint32_t(int32_t(eax) * int32_t(0x1f0u));
    eax = eax + rd32(m, ecx + 0x10u);
    x0 = movss_m(rd32(m, eax + 0x108u));
    x0 = mulss(x0, x1.l(0));
    x0 = addss(x0, rd32(m, eax + 0x104u));
    wr32(m, esp + 0x10u, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x10u));
    fC_6956e4 = fcmp32(x0.l(0), rd32(m, 0x9774a4u) /* 9.999999747378752e-06 */);
    if (!fc_b(fC_6956e4)) goto L_695701;
    x0 = movss_m(rd32(m, eax + 0xc8u));
    x0 = mulss(x0, x1.l(0));
    x0 = addss(x0, rd32(m, eax + 0xc4u));
L_695701:
    edx = rd32(m, edi);
    edx = rd32(m, edx + 0x10u);
    wr32(m, esp + 0xcu, x0.l(0));
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0xcu)));
    eax = (esi + 0x70u);
    st(m, 0) = farith(m, 1, st(m, 0), reflex_f80::from_f32(rd32(m, esp + 0x10u)));
    esp -= 4; wr32(m, esp, eax);
    wr16(m, esp + 0x10u, uint32_t(m.fpu_cw));
    eax = rd16(m, esp + 0x10u);
    st(m, 0) = farith(m, 1, st(m, 0), reflex_f80::from_f32(rd32(m, 0x935aa8u) /* 0.75 */));
    eax = (eax | 0xc00u);
    wr32(m, esp + 0x14u, eax);
    eax = rd32(m, esi + 0x94u);
    m.fpu_cw = uint16_t(rd16(m, esp + 0x14u));
    wr64(m, esp + 0x14u, uint64_t(fist(m, 8, frc(m))));
    fpop(m);
    ecx = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, eax);
    ecx = edi;
    m.fpu_cw = uint16_t(rd16(m, esp + 0x18u));
    LIFT_CALL(edx, 0x69574cu);
    eax = rd32(m, esi + 0x14u);
    wr32(m, eax + 0x4cu, 0x0u);
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_69575b:
    fA_69575b = rd32(m, esi + 0x7cu);
    fB_69575b = 0x0u;
    if (fA_69575b <= fB_69575b) goto L_69576e;
    edx = rd32(m, edi);
    eax = rd32(m, edx + 0x14u);
    esi = esi + 0x70u;
    esp -= 4; wr32(m, esp, esi);
    ecx = edi;
    LIFT_CALL(eax, 0x69576eu);
L_69576e:
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00695780 Fn_00695780
//   original FUN_00695780; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00695780, Fn_00695780, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_69579a = 0;
    uint32_t fB_69579a = 0;
    FCmp fC_6957d8{};
    uint32_t fA_6957ea = 0;
    uint32_t fB_6957ea = 0;
    uint32_t fR_69580a = 0;
    uint32_t fA_695812 = 0;
    uint32_t fB_695812 = 0;
    uint32_t fA_695825 = 0;
    uint32_t fB_695825 = 0;
    uint32_t fA_69583a = 0;
    uint32_t fB_69583a = 0;
    uint32_t fA_695852 = 0;
    uint32_t fB_695852 = 0;
    uint32_t fA_695927 = 0;
    uint32_t fB_695927 = 0;
    uint32_t fR_69593a = 0;
    uint32_t fA_695973 = 0;
    uint32_t fB_695973 = 0;
    uint32_t fA_695979 = 0;
    uint32_t fB_695979 = 0;
    FCmp fC_6959a5{};
    uint32_t fA_6959b9 = 0;
    uint32_t fB_6959b9 = 0;
    uint32_t fR_6959de = 0;
    uint32_t fA_6959fc = 0;
    uint32_t fB_6959fc = 0;
    uint32_t fR_695a0c = 0;
    uint32_t fA_695a16 = 0;
    uint32_t fB_695a16 = 0;
    uint32_t fA_695a1c = 0;
    uint32_t fB_695a1c = 0;
    uint32_t fA_695a3e = 0;
    uint32_t fB_695a3e = 0;
    uint32_t fR_695a5d = 0;
    uint32_t fA_695a76 = 0;
    uint32_t fB_695a76 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xffffff80u);
    esp = esp - 0x1f4u;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp + 0x1f0u, eax);
    fA_69579a = rd32(m, 0xc11bc4u);
    fB_69579a = 0x0u;
    eax = rd32(m, ebp + 0x8u);
    ecx = rd32(m, eax + 0xcu);
    edx = rd32(m, eax + 0x4u);
    x0 = movss_m(rd32(m, eax));
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, eax + 0x14u);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, eax + 0x8u);
    wr32(m, esp + 0x30u, ecx);
    ecx = rd32(m, eax + 0x10u);
    esp -= 4; wr32(m, esp, edi);
    wr32(m, esp + 0x30u, edx);
    wr32(m, esp + 0x2cu, esi);
    wr32(m, esp + 0x28u, ecx);
    if (fA_69579a != fB_69579a) goto L_695a93;
    x1 = movss_m(rd32(m, 0x977bb4u) /* 0.0333000011742115 */);
    fC_6957d8 = fcmp32(x0.l(0), x1.l(0));
    if (fc_b(fC_6957d8)) goto L_69599d;
    x0 = x1;
L_6957e4:
    wr32(m, esp + 0x24u, x0.l(0));
L_6957ea:
    fA_6957ea = rd32(m, ebx + 0x20u);
    fB_6957ea = 0x0u;
    if (fA_6957ea == fB_6957ea) goto L_69580a;
    eax = rd32(m, ebx + 0x94u);
    edi = (ebx + 0x30u);
    ecx = 0x10u;
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    wr32(m, ebx + 0x20u, 0x0u);
    wr32(m, ebx + 0x70u, eax);
L_69580a:
    fR_69580a = (edx & edx);
    if ((fR_69580a == 0)) goto L_695a93;
    fA_695812 = rd32(m, ebx + 0x14u);
    fB_695812 = 0x0u;
    if (fA_695812 == fB_695812) goto L_695a93;
    ecx = rd32(m, 0xa95844u);
    edx = rd32(m, ecx + 0x10u);
    fA_695825 = rd32(m, edx + 0xcu);
    fB_695825 = 0x0u;
    if (fA_695825 != fB_695825) goto L_695a93;
    edx = rd32(m, 0xa946b8u);
    LIFT_CALL(0x75c580u, 0x69583au);  // FUN_0075c580
    fA_69583a = rd32(m, eax + 0x20014u);
    fB_69583a = 0x0u;
    if (fA_69583a != fB_69583a) goto L_695a93;
    edi = rd32(m, esp + 0x30u);
    eax = ebx;
    LIFT_CALL(0x695650u, 0x695852u);  // FUN_00695650
    fA_695852 = rd32(m, ebx + 0x1cu);
    fB_695852 = 0x0u;
    if (fA_695852 == fB_695852) goto L_695a93;
    eax = rd32(m, ebx + 0x14u);
    edx = 0x1u;
    wr32(m, eax + 0x4cu, edx);
    ecx = rd32(m, ebx + 0x18u);
    eax = rd32(m, eax + 0x44u);
    ecx = uint32_t(int32_t(ecx) * int32_t(0x1f0u));
    ecx = ecx + rd32(m, eax + 0x10u);
    eax = rd32(m, 0x9cb860u);
    wr32(m, 0x9cb864u, rd32(m, 0x9cb864u) + edx);
    eax = uint32_t(int32_t(eax) * int32_t(0x41c64e6du));
    x1 = movss_m(rd32(m, 0x9c46b8u));
    x1 = mulss(x1, rd32(m, esp + 0x24u));
    eax = eax + 0x3039u;
    wr32(m, 0x9cb860u, eax);
    eax = (eax >> 16);
    eax = (eax & 0x7fffu);
    x0 = cvtsi2ss(x0, eax);
    eax = rd32(m, 0xa95844u);
    x0 = cvtss2sd(x0, x0.l(0));
    x0 = mulsd(x0, rd64(m, 0x9776a0u) /* 3.0517578125e-05 */);
    x0 = cvtsd2ss(x0, x0.lo);
    x0 = mulss(x0, rd32(m, ecx + 0x108u));
    x0 = addss(x0, rd32(m, ecx + 0x104u));
    ecx = rd32(m, eax + 0x4u);
    ecx = rd32(m, eax + ecx * 4 + 0x1cu);
    x0 = mulss(x0, x1.l(0));
    x0 = addss(x0, rd32(m, ebx + 0x98u));
    eax = cvttss2si(x0.l(0));
    edx = (eax >> 31) ? 0xffffffffu : 0;
    { uint32_t d_ = rd32(m, ecx + 0x19d0u); sdiv32(eax, edx, d_); }
    edx = (esp + 0x178u);
    esi = eax;
    wr32(m, 0xa94270u, rd32(m, 0xa94270u) + esi);
    esp -= 4; wr32(m, esp, esi);
    x1 = cvtsi2ss(x1, esi);
    esp -= 4; wr32(m, esp, 0x95dc24u);
    x0 = subss(x0, x1.l(0));
    esp -= 4; wr32(m, esp, edx);
    wr32(m, esp + 0x48u, esi);
    wr32(m, esp + 0x44u, x1.l(0));
    wr32(m, ebx + 0x98u, x0.l(0));
    LIFT_CALL(rd32(m, 0x916204u) /* 8.286141263663936e-39 */, 0x695924u);
    esp = esp + 0xcu;
    fA_695927 = rd32(m, esp + 0x34u);
    fB_695927 = 0x0u;
    if (fA_695927 == fB_695927) goto L_695a0c;
    wr32(m, esp + 0x24u, 0x0u);
    fR_69593a = (esi & esi);
    if (((fR_69593a == 0) || sgn(fR_69593a, 4) != false)) goto L_695a7b;
L_695942:
    x0 = cvtsi2ss(x0, rd32(m, esp + 0x24u));
    x0 = addss(x0, rd32(m, 0x931f24u) /* 1.0 */);
    x0 = divss(x0, rd32(m, esp + 0x38u));
    edi = rd32(m, esp + 0x2cu);
    esp -= 4; wr32(m, esp, ecx);
    eax = (ebx + 0x30u);
    wr32(m, esp, x0.l(0));
    esp -= 4; wr32(m, esp, eax);
    esi = (esp + 0x48u);
    LIFT_CALL(0x6941a0u, 0x69596du);  // FUN_006941a0
    eax = rd32(m, ebx + 0x9cu);
    fA_695973 = eax;
    fB_695973 = 0x0u;
    eax = fA_695973 - fB_695973;
    if (fA_695973 == fB_695973) goto L_6959b9;
    eax = (eax - 1u);
    fA_695979 = rd32(m, ebx + 0x14u);
    fB_695979 = 0x0u;
    if (fA_695979 == fB_695979) goto L_6959f7;
    ecx = rd32(m, esp + 0x28u);
    eax = (esp + 0x80u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    edx = esi;
    esi = rd32(m, esp + 0x38u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, ebx);
    ecx = esi;
    LIFT_CALL(0x694ba0u, 0x69599bu);  // FUN_00694ba0
    goto L_6959de;
L_69599d:
    x1 = movss_m(rd32(m, 0x97743cu) /* 0.0010000000474974513 */);
    fC_6959a5 = fcmp32(x1.l(0), x0.l(0));
    if (fc_b(fC_6959a5)) goto L_6957e4;
    wr32(m, esp + 0x24u, x1.l(0));
    goto L_6957ea;
L_6959b9:
    fA_6959b9 = rd32(m, ebx + 0x14u);
    fB_6959b9 = 0x0u;
    if (fA_6959b9 == fB_6959b9) goto L_6959f7;
    ecx = rd32(m, esp + 0x28u);
    eax = (esp + 0x80u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    ecx = rd32(m, esp + 0x38u);
    esp -= 4; wr32(m, esp, ebx);
    eax = (esp + 0x4cu);
    LIFT_CALL(0x6944c0u, 0x6959dau);  // FUN_006944c0
    esi = rd32(m, esp + 0x30u);
L_6959de:
    fR_6959de = (eax & eax);
    if ((fR_6959de == 0)) goto L_6959f7;
    edx = rd32(m, esi);
    edx = rd32(m, edx + 0x4u);
    eax = (esp + 0x80u);
    esp -= 4; wr32(m, esp, eax);
    ecx = (ebx + 0x70u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = esi;
    LIFT_CALL(edx, 0x6959f7u);
L_6959f7:
    eax = rd32(m, esp + 0x24u);
    eax = (eax + 1u);
    fA_6959fc = eax;
    fB_6959fc = rd32(m, esp + 0x3cu);
    wr32(m, esp + 0x24u, eax);
    if (int32_t(sext(fA_6959fc, 4)) < int32_t(sext(fB_6959fc, 4))) goto L_695942;
    goto L_695a7b;
L_695a0c:
    fR_695a0c = (esi & esi);
    if (((fR_695a0c == 0) || sgn(fR_695a0c, 4) != false)) goto L_695a7b;
L_695a10:
    eax = rd32(m, ebx + 0x9cu);
    fA_695a16 = eax;
    fB_695a16 = 0x0u;
    eax = fA_695a16 - fB_695a16;
    if (fA_695a16 == fB_695a16) goto L_695a3e;
    eax = (eax - 1u);
    fA_695a1c = rd32(m, ebx + 0x14u);
    fB_695a1c = 0x0u;
    if (fA_695a1c == fB_695a1c) goto L_695a76;
    ecx = rd32(m, esp + 0x28u);
    edx = rd32(m, esp + 0x2cu);
    eax = (esp + 0x80u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, ebx);
    ecx = edi;
    LIFT_CALL(0x694ba0u, 0x695a3cu);  // FUN_00694ba0
    goto L_695a5d;
L_695a3e:
    fA_695a3e = rd32(m, ebx + 0x14u);
    fB_695a3e = 0x0u;
    if (fA_695a3e == fB_695a3e) goto L_695a76;
    ecx = rd32(m, esp + 0x28u);
    eax = (esp + 0x80u);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, esp + 0x30u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebx);
    ecx = edi;
    LIFT_CALL(0x6944c0u, 0x695a5du);  // FUN_006944c0
L_695a5d:
    fR_695a5d = (eax & eax);
    if ((fR_695a5d == 0)) goto L_695a76;
    edx = rd32(m, edi);
    edx = rd32(m, edx + 0x4u);
    eax = (esp + 0x80u);
    esp -= 4; wr32(m, esp, eax);
    ecx = (ebx + 0x70u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = edi;
    LIFT_CALL(edx, 0x695a76u);
L_695a76:
    fA_695a76 = esi;
    fB_695a76 = 0x1u;
    esi = fA_695a76 - fB_695a76;
    if (fA_695a76 != fB_695a76) goto L_695a10;
L_695a7b:
    esi = rd32(m, esp + 0x2cu);
    eax = rd32(m, ebx + 0x14u);
    edi = (ebx + 0x30u);
    ecx = 0x10u;
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    wr32(m, eax + 0x4cu, 0x0u);
L_695a93:
    ecx = rd32(m, esp + 0x1fcu);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x695aa4u);  // __security_check_cookie
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00695ab0 ParticleEmitter__vf0
//   original ParticleEmitter::vf0; register inputs: ecx; returns with ret 20
REFLEX_PORT_FN(0x00695ab0, ParticleEmitter__vf0, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0x40u;
    eax = rd32(m, ebp + 0xcu);
    edx = rd32(m, ebp + 0x10u);
    x0 = movss_m(rd32(m, ebp + 0x8u));
    wr32(m, esp + 0x4u, eax);
    eax = rd32(m, ebp + 0x14u);
    wr32(m, esp + 0xcu, eax);
    wr32(m, esp + 0x8u, edx);
    edx = rd32(m, ebp + 0x18u);
    eax = (esp);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0x4u, x0.l(0));
    wr32(m, esp + 0x14u, edx);
    wr32(m, esp + 0x18u, ecx);
    LIFT_CALL(0x695780u, 0x695aedu);  // FUN_00695780
    esp = esp + 0x4u;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(20);
}

// 00695b00 Fn_00695b00
//   original FUN_00695b00; register inputs: none; returns with ret 8
REFLEX_PORT_FN(0x00695b00, Fn_00695b00, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fA_695b61 = 0;
    uint32_t fB_695b61 = 0;
    uint32_t fR_695b73 = 0;
    uint32_t fA_695b7b = 0;
    uint32_t fB_695b7b = 0;
    uint32_t fR_695b8a = 0;
    uint32_t fA_695bd1 = 0;
    uint32_t fB_695bd1 = 0;
    uint32_t fA_695c0b = 0;
    uint32_t fB_695c0b = 0;
    uint32_t fA_695c1d = 0;
    uint32_t fB_695c1d = 0;
    uint32_t fA_695c2b = 0;
    uint32_t fB_695c2b = 0;
    uint32_t fA_695c37 = 0;
    uint32_t fB_695c37 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x94u;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp + 0x90u, eax);
    eax = rd32(m, ebp + 0xcu);
    x0 = movss_m(rd32(m, 0x92e12cu) /* 0.0 */);
    ecx = rd32(m, ebp + 0x8u);
    x2 = ldx(m, eax);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = 0u;
    x0 = shufps(x0, x0, 0x0);
    stx(m, esp + 0x40u, x0);
    edx = (esp + 0x50u);
    ecx = ecx + 0xa4u;
    esi = (edi + 0x4u);
    goto L_695b50;
L_695b50:
    x0 = movss_m(rd32(m, ecx));
    x0 = shufps(x0, x0, 0x0);
    stx(m, edx, x0);
    ecx = ecx + 0x4u;
    edx = edx + 0x10u;
    fA_695b61 = esi;
    fB_695b61 = 0x1u;
    esi = fA_695b61 - fB_695b61;
    if (fA_695b61 != fB_695b61) goto L_695b50;
    ecx = rd32(m, 0xa95844u);
    edx = rd32(m, ecx + 0x4u);
    ecx = rd32(m, ecx + edx * 4 + 0x1cu);
    fR_695b73 = (ecx & ecx);
    if ((fR_695b73 == 0)) goto L_695c46;
    fA_695b7b = rd32(m, ecx + 0x34u);
    fB_695b7b = edi;
    if (fA_695b7b == fB_695b7b) goto L_695c46;
    edx = rd32(m, ecx + 0x19d0u);
    fR_695b8a = (edx & edx);
    if (((fR_695b8a == 0) || sgn(fR_695b8a, 4) != false)) goto L_695c46;
    ecx = ecx + 0x19b8u;
    wr32(m, esp + 0x18u, ecx);
    wr32(m, esp + 0x1cu, edx);
L_695ba0:
    eax = rd32(m, ecx);
    ecx = rd32(m, eax);
    x0 = ldx(m, ecx + 0x10u);
    x0 = subps(x0, x2);
    x0 = mulps(x0, x0);
    x1 = x0;
    x1 = shufps(x1, x0, 0x99);
    x3 = x1;
    x0 = addss(x0, x1.l(0));
    x3 = shufps(x3, x1, 0x55);
    x0 = addss(x0, x3.l(0));
    x0 = shufps(x0, x0, 0x0);
    esi = 0u;
    ecx = (esp + 0x50u);
L_695bce:
    edx = rd32(m, ebp + 0x8u);
    fA_695bd1 = rd8(m, edx + esi + 0xb4u);
    fB_695bd1 = 0x0u;
    if (fA_695bd1 == fB_695bd1) goto L_695c27;
    x1 = ldx(m, ecx - 0x10u);
    x1 = cmpps<2>(x1, x0);
    stx(m, esp + 0x30u, x1);
    x1 = ldx(m, ecx);
    ebx = rd32(m, esp + 0x38u);
    x3 = x0;
    x3 = cmpps<2>(x3, x1);
    stx(m, esp + 0x20u, x3);
    eax = rd32(m, esp + 0x24u);
    edx = rd32(m, esp + 0x28u);
    edx = edx + eax;
    edx = edx + rd32(m, esp + 0x20u);
    eax = 0u;
    fA_695c0b = edx;
    fB_695c0b = 0xfffffffdu;
    edx = rd32(m, esp + 0x34u);
    eax = (eax & 0xffffff00u) | (((fA_695c0b == fB_695c0b) ? 1u : 0u) & 0xffu);
    ebx = ebx + edx;
    ebx = ebx + rd32(m, esp + 0x30u);
    edx = 0u;
    fA_695c1d = ebx;
    fB_695c1d = 0xfffffffdu;
    edx = (edx & 0xffffff00u) | (((fA_695c1d == fB_695c1d) ? 1u : 0u) & 0xffu);
    eax = (eax & edx);
    edi = (edi | eax);
L_695c27:
    esi = (esi + 1u);
    ecx = ecx + 0x10u;
    fA_695c2b = esi;
    fB_695c2b = 0x4u;
    if (int32_t(sext(fA_695c2b, 4)) < int32_t(sext(fB_695c2b, 4))) goto L_695bce;
    ecx = rd32(m, esp + 0x18u);
    ecx = ecx + 0x4u;
    fA_695c37 = rd32(m, esp + 0x1cu);
    fB_695c37 = 0x1u;
    wr32(m, esp + 0x1cu, fA_695c37 - fB_695c37);
    wr32(m, esp + 0x18u, ecx);
    if (fA_695c37 != fB_695c37) goto L_695ba0;
L_695c46:
    ecx = rd32(m, esp + 0x9cu);
    eax = edi;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x695c59u);  // __security_check_cookie
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 006960c0 ParticleEntity__UpdateManagers
//   original ParticleEntity::UpdateManagers; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x006960c0, ParticleEntity__UpdateManagers, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp -= 4; wr32(m, esp, ecx);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x8u)));
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    esp -= 4; wr32(m, esp, ecx);
    ecx = rd32(m, esi + 0xc0u);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x698510u, 0x6960dcu);  // ParticleManager2D_Update
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x8u)));
    esp -= 4; wr32(m, esp, ecx);
    ecx = rd32(m, esi + 0xc4u);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x698510u, 0x6960eeu);  // ParticleManager2D_Update
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00696100 ParticleEntity__Tick
//   original ParticleEntity::Tick; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x00696100, ParticleEntity__Tick, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    ebp = rd32(m, esi + 0xb0u);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esi + 0xc0u);
    ebx = (edi + 0xf4u);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x696120u);
    wr32(m, edi + 0x70u, ebp);
    ebp = rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */;
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(ebp, 0x69612cu);
    edi = rd32(m, esi + 0xc4u);
    eax = rd32(m, esi + 0xb0u);
    ebx = (edi + 0xf4u);
    esp -= 4; wr32(m, esp, ebx);
    wr32(m, esp + 0x14u, eax);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x696149u);
    ecx = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, ebx);
    wr32(m, edi + 0x70u, ecx);
    LIFT_CALL(ebp, 0x696153u);
    edx = rd32(m, esi + 0xc0u);
    eax = rd32(m, esi + 0xb8u);
    wr32(m, edx + 0x8cu, eax);
    ecx = rd32(m, esi + 0xc4u);
    edx = rd32(m, esi + 0xb8u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    wr32(m, ecx + 0x8cu, edx);
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 006968e0 PlaceableParticleEffect__vf5_o90
//   original PlaceableParticleEffect::vf5_o90; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x006968e0, PlaceableParticleEffect__vf5_o90, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fR_6968e4 = 0;
    uint32_t fA_6968e8 = 0;
    uint32_t fB_6968e8 = 0;
    uint32_t fR_6968ff = 0;
    uint32_t fR_696903 = 0;
    uint32_t fA_69691f = 0;
    uint32_t fB_69691f = 0;
    eax = rd32(m, esp + 0x4u);
    fR_6968e4 = (eax & eax);
    if ((fR_6968e4 == 0)) goto L_6968f5;
    fA_6968e8 = rd32(m, ecx + 0x2cu);
    fB_6968e8 = 0x0u;
    if (fA_6968e8 != fB_6968e8) goto L_6968f5;
    wr32(m, ecx + 0x30u, 0x1u);
L_6968f5:
    esp -= 4; wr32(m, esp, esi);
    wr32(m, ecx + 0x2cu, eax);
    esi = rd32(m, ecx + 0x204u);
    fR_6968ff = (esi & esi);
    if ((fR_6968ff == 0)) goto L_69693c;
    fR_696903 = (eax & eax);
    if ((fR_696903 == 0)) goto L_696912;
    eax = esi;
    LIFT_CALL(0x678bd0u, 0x69690eu);  // FUN_00678bd0
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_696912:
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, 0xd67d1cu);
    LIFT_CALL(rd32(m, 0x91607cu) /* 8.295210467325047e-39 */, 0x69691fu);
    fA_69691f = edi;
    fB_69691f = eax;
    edi = rd32(m, esp); esp += 4;
    if (fA_69691f != fB_69691f) goto L_69692d;
    LIFT_CALL(0x80ae10u, 0x696929u);  // FUN_0080ae10
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_69692d:
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, 0x1u);
    LIFT_CALL(0x80c280u, 0x696939u);  // FUN_0080c280
    esp = esp + 0x10u;
L_69693c:
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00696940 PlaceableParticleEffect__vf19
//   original PlaceableParticleEffect::vf19; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x00696940, PlaceableParticleEffect__vf19, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_69694e = 0;
    uint32_t fB_69694e = 0;
    uint32_t fA_696967 = 0;
    uint32_t fB_696967 = 0;
    uint32_t fA_69696f = 0;
    uint32_t fB_69696f = 0;
    uint32_t fA_696992 = 0;
    uint32_t fB_696992 = 0;
    uint32_t fA_6969bc = 0;
    uint32_t fB_6969bc = 0;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    eax = rd32(m, esi + 0x294u);
    ebx = 0u;
    esp -= 4; wr32(m, esp, edi);
    fA_69694e = eax;
    fB_69694e = ebx;
    if (fA_69694e == fB_69694e) goto L_696957;
    LIFT_CALL(0x678980u, 0x696957u);  // FUN_00678980
L_696957:
    eax = rd32(m, esi + 0x10u);
    ecx = rd32(m, eax + 0x2cc4u);
    ebp = rd32(m, ecx * 4 + 0xa94264u);
    fA_696967 = rd32(m, esi + 0x94u);
    fB_696967 = ebx;
    if (fA_696967 != fB_696967) goto L_6969c4;
    fA_69696f = ebp;
    fB_69696f = ebx;
    if (fA_69696f == fB_69696f) goto L_6969bc;
    edi = rd32(m, ebp + 0xb0u);
    eax = rd32(m, edi + 0x44u);
    wr32(m, edi + 0x4cu, 0x1u);
    edx = rd32(m, esi + 0x9cu);
    ecx = rd32(m, eax + 0x10u);
    edx = uint32_t(int32_t(edx) * int32_t(0x1f0u));
    fA_696992 = rd8(m, edx + ecx + 0x160u);
    fB_696992 = (ebx & 0xffu);
    if (fA_696992 != fB_696992) goto L_696a03;
    edx = rd32(m, esi + 0x90u);
    eax = rd32(m, edx + 0x14u);
    ecx = (esi + 0x90u);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(eax, 0x6969adu);
    eax = rd32(m, ebp + 0xc0u);
L_6969b3:
    wr32(m, esi + 0x94u, eax);
    wr32(m, edi + 0x4cu, ebx);
L_6969bc:
    fA_6969bc = rd32(m, esi + 0x94u);
    fB_6969bc = ebx;
    if (fA_6969bc == fB_6969bc) goto L_6969fc;
L_6969c4:
    ebp = rd32(m, ebp + 0xb0u);
    wr32(m, ebp + 0x4cu, ebx);
    edx = rd32(m, esi);
    eax = rd32(m, edx + 0x38u);
    ebx = rd32(m, esi + 0xa0u);
    esp -= 4; wr32(m, esp, 0xffffffffu);
    edi = (esi + 0xa0u);
    esp -= 4; wr32(m, esp, 0x1u);
    ecx = esi;
    LIFT_CALL(eax, 0x6969e6u);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x1cu)));
    ecx = rd32(m, esi + 0x94u);
    edx = rd32(m, ebx);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ecx);
    ecx = edi;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(edx, 0x6969fcu);
L_6969fc:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_696a03:
    eax = rd32(m, ebp + 0xc4u);
    goto L_6969b3;
}

// 00696d80 Fn_00696d80
//   original FUN_00696d80; register inputs: eax edi; returns with ret 4
REFLEX_PORT_FN(0x00696d80, Fn_00696d80, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fR_696d94 = 0;
    uint32_t fA_696d9c = 0;
    uint32_t fB_696d9c = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x18u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    ecx = edi;
    esi = eax;
    LIFT_CALL(0x697460u, 0x696d94u);  // FUN_00697460
    fR_696d94 = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_696d94 == 0)) goto L_6971cb;
    fA_696d9c = rd8(m, edi + 0x68u);
    fB_696d9c = 0x0u;
    x3 = movss_m(rd32(m, 0x92e330u) /* 2.0 */);
    x2 = movss_m(rd32(m, esi + 0x1e0u));
    x4 = movss_m(rd32(m, esi + 0x1e4u));
    x1 = movss_m(rd32(m, esi + 0x1e8u));
    x0 = x2;
    x0 = mulss(x0, x3.l(0));
    x2 = mulss(x2, x0.l(0));
    x5 = x4;
    x5 = mulss(x5, x3.l(0));
    x6 = x1;
    x6 = mulss(x6, x3.l(0));
    wr32(m, esp + 0x14u, x2.l(0));
    x2 = x4;
    x3 = x1;
    x3 = mulss(x3, x6.l(0));
    x2 = mulss(x2, x5.l(0));
    x4 = mulss(x4, x0.l(0));
    if (fA_696d9c == fB_696d9c) goto L_696f92;
    wr32(m, esp + 0xcu, x6.l(0));
    x6 = movss_m(rd32(m, esi + 0x1ecu));
    x7 = x6;
    x7 = mulss(x7, x0.l(0));
    wr32(m, esp + 0x18u, x7.l(0));
    x7 = x6;
    x6 = mulss(x6, rd32(m, esp + 0xcu));
    x7 = mulss(x7, x5.l(0));
    wr32(m, esp + 0x10u, x7.l(0));
    x7 = x4;
    x4 = x1;
    x1 = mulss(x1, x5.l(0));
    x5 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x4 = mulss(x4, x0.l(0));
    wr32(m, esp + 0xcu, x6.l(0));
    x0 = x3;
    x0 = addss(x0, x2.l(0));
    x6 = x5;
    x6 = subss(x6, x0.l(0));
    wr32(m, edi + 0x10u, x6.l(0));
    x6 = movss_m(rd32(m, esp + 0x14u));
    x0 = x7;
    x0 = addss(x0, rd32(m, esp + 0xcu));
    x7 = subss(x7, rd32(m, esp + 0xcu));
    wr32(m, edi + 0x20u, x7.l(0));
    wr32(m, edi + 0x14u, x0.l(0));
    wr32(m, edi + 0x4cu, x5.l(0));
    x3 = addss(x3, x6.l(0));
    x7 = x5;
    x7 = subss(x7, x3.l(0));
    x3 = movss_m(rd32(m, esp + 0x18u));
    wr32(m, edi + 0x24u, x7.l(0));
    x7 = x1;
    x1 = subss(x1, x3.l(0));
    wr32(m, edi + 0x34u, x1.l(0));
    x0 = x4;
    x0 = subss(x0, rd32(m, esp + 0x10u));
    x4 = addss(x4, rd32(m, esp + 0x10u));
    wr32(m, edi + 0x18u, x0.l(0));
    x0 = X{};
    wr32(m, edi + 0x1cu, x0.l(0));
    wr32(m, edi + 0x2cu, x0.l(0));
    wr32(m, edi + 0x3cu, x0.l(0));
    wr32(m, edi + 0x40u, x0.l(0));
    wr32(m, edi + 0x44u, x0.l(0));
    wr32(m, edi + 0x48u, x0.l(0));
    wr32(m, edi + 0x30u, x4.l(0));
    x2 = addss(x2, x6.l(0));
    x1 = x5;
    x1 = subss(x1, x2.l(0));
    wr32(m, edi + 0x38u, x1.l(0));
    x7 = addss(x7, x3.l(0));
    wr32(m, edi + 0x28u, x7.l(0));
    x3 = movss_m(rd32(m, edi + 0xd8u));
    x4 = movss_m(rd32(m, edi + 0xd4u));
    x5 = movss_m(rd32(m, edi + 0xd0u));
    x0 = movss_m(rd32(m, edi + 0x20u));
    x1 = movss_m(rd32(m, edi + 0x30u));
    x2 = movss_m(rd32(m, edi + 0x24u));
    x1 = mulss(x1, x3.l(0));
    x0 = mulss(x0, x4.l(0));
    x0 = addss(x0, x1.l(0));
    x1 = x5;
    x1 = mulss(x1, rd32(m, edi + 0x10u));
    x0 = addss(x0, x1.l(0));
    x1 = movss_m(rd32(m, edi + 0x14u));
    x0 = addss(x0, rd32(m, edi + 0x40u));
    x2 = mulss(x2, x4.l(0));
    x1 = mulss(x1, x5.l(0));
    x1 = addss(x1, x2.l(0));
    x2 = movss_m(rd32(m, edi + 0x34u));
    x2 = mulss(x2, x3.l(0));
    x1 = addss(x1, x2.l(0));
    x2 = movss_m(rd32(m, edi + 0x18u));
    x1 = addss(x1, rd32(m, edi + 0x44u));
    x2 = mulss(x2, x5.l(0));
    x5 = x7;
    x5 = mulss(x5, x4.l(0));
    x4 = movss_m(rd32(m, edi + 0x38u));
    x4 = mulss(x4, x3.l(0));
    x3 = movss_m(rd32(m, esi + 0x1d0u));
    x3 = addss(x3, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x1d4u));
    x0 = addss(x0, x1.l(0));
    x1 = movss_m(rd32(m, esi + 0x1d8u));
    goto L_69712a;
L_696f92:
    wr32(m, esp + 0x18u, x6.l(0));
    x6 = movss_m(rd32(m, esi + 0x1ecu));
    x7 = x6;
    x7 = mulss(x7, x0.l(0));
    wr32(m, esp + 0x1cu, x7.l(0));
    x7 = x6;
    x6 = mulss(x6, rd32(m, esp + 0x18u));
    x7 = mulss(x7, x5.l(0));
    wr32(m, esp + 0xcu, x7.l(0));
    x7 = x4;
    x4 = x1;
    x1 = mulss(x1, x5.l(0));
    x5 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x4 = mulss(x4, x0.l(0));
    wr32(m, esp + 0x10u, x6.l(0));
    x0 = x3;
    x0 = addss(x0, x2.l(0));
    x6 = x5;
    x6 = subss(x6, x0.l(0));
    wr32(m, edi + 0x10u, x6.l(0));
    x6 = movss_m(rd32(m, esp + 0x14u));
    x0 = x7;
    x0 = addss(x0, rd32(m, esp + 0x10u));
    x7 = subss(x7, rd32(m, esp + 0x10u));
    wr32(m, edi + 0x20u, x7.l(0));
    wr32(m, edi + 0x14u, x0.l(0));
    wr32(m, edi + 0x4cu, x5.l(0));
    x7 = x5;
    x3 = addss(x3, x6.l(0));
    x7 = subss(x7, x3.l(0));
    x3 = movss_m(rd32(m, esp + 0x1cu));
    wr32(m, edi + 0x24u, x7.l(0));
    x7 = x1;
    x1 = subss(x1, x3.l(0));
    wr32(m, edi + 0x34u, x1.l(0));
    x0 = x4;
    x0 = subss(x0, rd32(m, esp + 0xcu));
    x4 = addss(x4, rd32(m, esp + 0xcu));
    wr32(m, edi + 0x18u, x0.l(0));
    x0 = X{};
    wr32(m, edi + 0x1cu, x0.l(0));
    wr32(m, edi + 0x2cu, x0.l(0));
    wr32(m, edi + 0x3cu, x0.l(0));
    wr32(m, edi + 0x40u, x0.l(0));
    wr32(m, edi + 0x44u, x0.l(0));
    wr32(m, edi + 0x48u, x0.l(0));
    wr32(m, edi + 0x30u, x4.l(0));
    x2 = addss(x2, x6.l(0));
    x1 = x5;
    x1 = subss(x1, x2.l(0));
    wr32(m, edi + 0x38u, x1.l(0));
    x7 = addss(x7, x3.l(0));
    wr32(m, edi + 0x28u, x7.l(0));
    x4 = movss_m(rd32(m, edi + 0xd4u));
    x3 = movss_m(rd32(m, edi + 0xd8u));
    x5 = movss_m(rd32(m, edi + 0xd0u));
    x0 = movss_m(rd32(m, edi + 0x20u));
    x1 = movss_m(rd32(m, edi + 0x30u));
    x2 = movss_m(rd32(m, edi + 0x24u));
    eax = rd32(m, edi + 0x64u);
    x1 = mulss(x1, x3.l(0));
    x0 = mulss(x0, x4.l(0));
    x0 = addss(x0, x1.l(0));
    x1 = x5;
    x1 = mulss(x1, rd32(m, edi + 0x10u));
    x0 = addss(x0, x1.l(0));
    x1 = movss_m(rd32(m, edi + 0x14u));
    x0 = addss(x0, rd32(m, edi + 0x40u));
    x2 = mulss(x2, x4.l(0));
    x1 = mulss(x1, x5.l(0));
    x1 = addss(x1, x2.l(0));
    x2 = movss_m(rd32(m, edi + 0x34u));
    x2 = mulss(x2, x3.l(0));
    x1 = addss(x1, x2.l(0));
    x2 = movss_m(rd32(m, edi + 0x18u));
    x1 = addss(x1, rd32(m, edi + 0x44u));
    x2 = mulss(x2, x5.l(0));
    x5 = x7;
    x5 = mulss(x5, x4.l(0));
    x4 = movss_m(rd32(m, edi + 0x38u));
    x4 = mulss(x4, x3.l(0));
    eax = uint32_t(int32_t(eax) * int32_t(0x70u));
    x3 = movss_m(rd32(m, eax + esi + 0x30u));
    eax = (eax + esi + 0x30u);
    x3 = addss(x3, x0.l(0));
    x0 = movss_m(rd32(m, eax + 0x4u));
    x0 = addss(x0, x1.l(0));
    x1 = movss_m(rd32(m, eax + 0x8u));
L_69712a:
    x2 = addss(x2, x5.l(0));
    x2 = addss(x2, x4.l(0));
    x2 = addss(x2, rd32(m, edi + 0x48u));
    wr32(m, edi + 0x44u, x0.l(0));
    wr32(m, edi + 0x40u, x3.l(0));
    x1 = addss(x1, x2.l(0));
    wr32(m, edi + 0x48u, x1.l(0));
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x1f0u)));
    wr32(m, edi + 0x50u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x1f4u)));
    wr32(m, edi + 0x54u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x1f8u)));
    wr32(m, edi + 0x58u, fst32(m));
    fpop(m);
    ecx = rd32(m, edi + 0x60u);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x10u);
    LIFT_CALL(eax, 0x69716fu);
    ebx = rd32(m, eax + 0x14u);
    eax = rd32(m, eax + 0x18u);
    ecx = rd32(m, ebx + 0x44u);
    eax = uint32_t(int32_t(eax) * int32_t(0x1f0u));
    wr32(m, ebx + 0x4cu, 0x1u);
    eax = eax + rd32(m, ecx + 0x10u);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x16cu)));
    edx = (eax + 0x108u);
    esp -= 4; wr32(m, esp, edx);
    edx = rd8(m, edi + 0x68u);
    ecx = (eax + 0x104u);
    esp -= 4; wr32(m, esp, ecx);
    esp = esp - 0x8u;
    wr32(m, esp + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x168u)));
    eax = rd32(m, edi + 0x64u);
    wr32(m, esp, fst32(m));
    fpop(m);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, esi);
    esi = (edi + 0xa8u);
    LIFT_CALL(0x693560u, 0x6971c0u);  // FUN_00693560
    wr32(m, ebx + 0x4cu, 0x0u);
    esp -= 4; wr32(m, esp, 0x1u);
    goto L_6971cd;
L_6971cb:
    esp -= 4; wr32(m, esp, 0x0u);
L_6971cd:
    ecx = rd32(m, edi + 0x60u);
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x14u);
    LIFT_CALL(edx, 0x6971d7u);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x8u)));
    ecx = rd32(m, edi + 0x60u);
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0xcu);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(edx, 0x6971e8u);
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00697460 Fn_00697460
//   original FUN_00697460; register inputs: ecx eax; returns with ret 0
REFLEX_PORT_FN(0x00697460, Fn_00697460, "fx", reflex_port::OUT_EAX) {
    LIFT_ENTRY();
    uint32_t fA_697461 = 0;
    uint32_t fB_697461 = 0;
    uint32_t fA_697488 = 0;
    uint32_t fB_697488 = 0;
    uint32_t fA_697491 = 0;
    uint32_t fB_697491 = 0;
    uint32_t fA_6974a8 = 0;
    uint32_t fB_6974a8 = 0;
    uint32_t fR_6974b1 = 0;
    uint32_t fA_6974eb = 0;
    uint32_t fB_6974eb = 0;
    uint32_t fA_697540 = 0;
    uint32_t fB_697540 = 0;
    FCmp fC_697549{};
    uint32_t FL = 0;
    FCmp fC_697567{};
    uint32_t fA_69757f = 0;
    uint32_t fB_69757f = 0;
    uint32_t fA_697599 = 0;
    uint32_t fB_697599 = 0;
    FCmp fC_6975a2{};
    FCmp fC_6975c0{};
    uint32_t fA_6975d0 = 0;
    uint32_t fB_6975d0 = 0;
    uint32_t fA_6975e0 = 0;
    uint32_t fB_6975e0 = 0;
    FCmp fC_6975e6{};
    FCmp fC_6975f6{};
    uint32_t fA_6975fe = 0;
    uint32_t fB_6975fe = 0;
    uint32_t fA_697608 = 0;
    uint32_t fB_697608 = 0;
    FCmp fC_69760e{};
    FCmp fC_69761e{};
    uint32_t fA_697626 = 0;
    uint32_t fB_697626 = 0;
    uint32_t fA_697633 = 0;
    uint32_t fB_697633 = 0;
    FCmp fC_69763c{};
    FCmp fC_697652{};
    uint32_t fA_69765a = 0;
    uint32_t fB_69765a = 0;
    uint32_t fA_69766e = 0;
    uint32_t fB_69766e = 0;
    uint32_t fR_697677 = 0;
    uint32_t fR_69767d = 0;
    uint32_t fA_697686 = 0;
    uint32_t fB_697686 = 0;
    esp -= 4; wr32(m, esp, ecx);
    fA_697461 = rd8(m, ecx + 0x68u);
    fB_697461 = 0x0u;
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    eax = (eax & 0xffffff00u) | ((0x1u) & 0xffu);
    if (fA_697461 != fB_697461) goto L_697686;
    edx = rd32(m, ecx + 0x64u);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esi + edx * 4 + 0x1c0u);
    edx = rd32(m, edx * 4 + 0x9c4740u);
    wr32(m, edx + 0x28u, edi);
    wr8(m, edx + 0x24u, (eax & 0xffu));
    fA_697488 = edi;
    fB_697488 = rd32(m, ecx + 0xe0u);
    edi = rd32(m, esp); esp += 4;
    if (fA_697488 == fB_697488) goto L_6974b9;
    fA_697491 = rd8(m, ecx + 0xa4u);
    fB_697491 = 0x0u;
    if (fA_697491 == fB_697491) goto L_69768c;
    edx = rd32(m, ecx + 0x64u);
    edx = uint32_t(int32_t(edx) * int32_t(0x70u));
    edx = rd32(m, edx + esi + 0x6cu);
    fA_6974a8 = edx;
    fB_6974a8 = rd32(m, ecx + 0xa0u);
    edx = (edx & 0xffffff00u) | (((fA_6974a8 == fB_6974a8) ? 1u : 0u) & 0xffu);
    fR_6974b1 = ((edx & 0xffu) & (edx & 0xffu));
    if ((fR_6974b1 == 0)) goto L_69768c;
L_6974b9:
    edx = rd32(m, ecx + 0x64u);
    x0 = movss_m(rd32(m, esi + 0x1f8u));
    edx = uint32_t(int32_t(edx) * int32_t(0x70u));
    x1 = movss_m(rd32(m, edx + esi + 0x54u));
    x3 = movss_m(rd32(m, edx + esi + 0x58u));
    x4 = movss_m(rd32(m, edx + esi + 0x48u));
    x5 = movss_m(rd32(m, esi + 0x1f0u));
    x2 = movss_m(rd32(m, esi + 0x1f4u));
    edx = edx + esi;
    fA_6974eb = rd8(m, ecx + 0x8du);
    fB_6974eb = 0x0u;
    x0 = mulss(x0, x0.l(0));
    wr32(m, esp + 0x4u, x0.l(0));
    x0 = movss_m(rd32(m, edx + 0x50u));
    x6 = x1;
    x6 = mulss(x6, x1.l(0));
    x1 = movss_m(rd32(m, edx + 0x40u));
    x0 = mulss(x0, x0.l(0));
    x0 = addss(x0, x6.l(0));
    x6 = movss_m(rd32(m, edx + 0x44u));
    x7 = x6;
    x7 = mulss(x7, x6.l(0));
    x6 = movss_m(rd32(m, edx + 0x60u));
    x1 = mulss(x1, x1.l(0));
    x3 = mulss(x3, x3.l(0));
    x4 = mulss(x4, x4.l(0));
    x5 = mulss(x5, x5.l(0));
    x2 = mulss(x2, x2.l(0));
    x1 = addss(x1, x7.l(0));
    if (fA_6974eb == fB_6974eb) goto L_69756c;
    fA_697540 = rd8(m, ecx + 0x8cu);
    fB_697540 = 0x0u;
    if (fA_697540 != fB_697540) goto L_69755f;
    fC_697549 = fcmp32(x6.l(0), rd32(m, ecx + 0x88u));
    FL = fc_flags(fC_697549);
L_697550:
    x7 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    if (fl_cc(3, FL)) goto L_697574;
    x6 = X{};
    goto L_697577;
L_69755f:
    x7 = movss_m(rd32(m, ecx + 0x88u));
    fC_697567 = fcmp32(x7.l(0), x6.l(0));
    FL = fc_flags(fC_697567);
    goto L_697550;
L_69756c:
    x7 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
L_697574:
    x6 = x7;
L_697577:
    edx = rd32(m, ecx + 0x64u);
    edx = uint32_t(int32_t(edx) * int32_t(0x70u));
    esi = esi + edx;
    fA_69757f = rd8(m, ecx + 0x95u);
    fB_69757f = 0x0u;
    x0 = addss(x0, x3.l(0));
    x3 = movss_m(rd32(m, esi + 0x64u));
    x2 = addss(x2, x5.l(0));
    x1 = addss(x1, x4.l(0));
    if (fA_69757f == fB_69757f) goto L_6975ca;
    fA_697599 = rd8(m, ecx + 0x94u);
    fB_697599 = 0x0u;
    if (fA_697599 != fB_697599) goto L_6975b8;
    fC_6975a2 = fcmp32(x3.l(0), rd32(m, ecx + 0x90u));
    x4 = X{};
    if (fc_b(fC_6975a2)) goto L_6975b3;
    x3 = x7;
    goto L_6975d0;
L_6975b3:
    x3 = x4;
    goto L_6975d0;
L_6975b8:
    x4 = movss_m(rd32(m, ecx + 0x90u));
    fC_6975c0 = fcmp32(x4.l(0), x3.l(0));
    if (!fc_b(fC_6975c0)) goto L_6975ca;
    x3 = X{};
    goto L_6975cd;
L_6975ca:
    x3 = x7;
L_6975cd:
    x4 = X{};
L_6975d0:
    fA_6975d0 = rd8(m, ecx + 0x75u);
    fB_6975d0 = 0x0u;
    x2 = addss(x2, rd32(m, esp + 0x4u));
    x3 = mulss(x3, x6.l(0));
    if (fA_6975d0 == fB_6975d0) goto L_6975fb;
    fA_6975e0 = rd8(m, ecx + 0x74u);
    fB_6975e0 = 0x0u;
    if (fA_6975e0 != fB_6975e0) goto L_6975f1;
    fC_6975e6 = fcmp32(x0.l(0), rd32(m, ecx + 0x70u));
    if (!fc_b(fC_6975e6)) goto L_6975fb;
L_6975ec:
    x0 = x4;
    goto L_6975fe;
L_6975f1:
    x5 = movss_m(rd32(m, ecx + 0x70u));
    fC_6975f6 = fcmp32(x5.l(0), x0.l(0));
    if (fc_b(fC_6975f6)) goto L_6975ec;
L_6975fb:
    x0 = x7;
L_6975fe:
    fA_6975fe = rd8(m, ecx + 0x7du);
    fB_6975fe = 0x0u;
    x3 = mulss(x3, x0.l(0));
    if (fA_6975fe == fB_6975fe) goto L_697623;
    fA_697608 = rd8(m, ecx + 0x7cu);
    fB_697608 = 0x0u;
    if (fA_697608 != fB_697608) goto L_697619;
    fC_69760e = fcmp32(x1.l(0), rd32(m, ecx + 0x78u));
    if (!fc_b(fC_69760e)) goto L_697623;
L_697614:
    x0 = x4;
    goto L_697626;
L_697619:
    x0 = movss_m(rd32(m, ecx + 0x78u));
    fC_69761e = fcmp32(x0.l(0), x1.l(0));
    if (fc_b(fC_69761e)) goto L_697614;
L_697623:
    x0 = x7;
L_697626:
    fA_697626 = rd8(m, ecx + 0x85u);
    fB_697626 = 0x0u;
    x3 = mulss(x3, x0.l(0));
    if (fA_697626 == fB_697626) goto L_697657;
    fA_697633 = rd8(m, ecx + 0x84u);
    fB_697633 = 0x0u;
    if (fA_697633 != fB_697633) goto L_69764a;
    fC_69763c = fcmp32(x2.l(0), rd32(m, ecx + 0x80u));
    if (!fc_b(fC_69763c)) goto L_697657;
L_697645:
    x0 = x4;
    goto L_69765a;
L_69764a:
    x0 = movss_m(rd32(m, ecx + 0x80u));
    fC_697652 = fcmp32(x0.l(0), x2.l(0));
    if (fc_b(fC_697652)) goto L_697645;
L_697657:
    x0 = x7;
L_69765a:
    fA_69765a = rd8(m, ecx + 0x9cu);
    fB_69765a = 0x0u;
    x0 = mulss(x0, x3.l(0));
    edx = cvttss2si(x0.l(0));
    if (fA_69765a == fB_69765a) goto L_69767d;
    esi = rd32(m, esi + 0x68u);
    fA_69766e = esi;
    fB_69766e = rd32(m, ecx + 0x98u);
    ecx = (ecx & 0xffffff00u) | (((fA_69766e == fB_69766e) ? 1u : 0u) & 0xffu);
    fR_697677 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_697677 == 0)) goto L_69767d;
    eax = (eax & 0xffffff00u) | ((0u) & 0xffu);
L_69767d:
    fR_69767d = (edx & edx);
    if (!(fR_69767d == 0)) goto L_69768e;
    eax = (eax & 0xffffff00u) | ((0u) & 0xffu);
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_697686:
    fA_697686 = rd32(m, ecx + 0x6cu);
    fB_697686 = 0x0u;
    if (fA_697686 == fB_697686) goto L_69768e;
L_69768c:
    eax = (eax & 0xffffff00u) | ((0u) & 0xffu);
L_69768e:
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 006979a0 Fn_006979a0
//   original FUN_006979a0; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x006979a0, Fn_006979a0, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_6979ba = 0;
    uint32_t fB_6979ba = 0;
    uint32_t fA_6979eb = 0;
    uint32_t fB_6979eb = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0xcu;
    eax = rd32(m, ebp + 0x8u);
    ecx = rd32(m, eax + 0x4u);
    x0 = movss_m(rd32(m, eax));
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, eax + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esi = 0u;
    fA_6979ba = rd32(m, ebx + 0x224u);
    fB_6979ba = esi;
    esp -= 4; wr32(m, esp, edi);
    wr32(m, esp + 0x10u, x0.l(0));
    wr32(m, esp + 0x14u, ecx);
    if (int32_t(sext(fA_6979ba, 4)) <= int32_t(sext(fB_6979ba, 4))) goto L_6979f3;
    edi = (ebx + 0x230u);
L_6979d3:
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x10u)));
    eax = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x696d80u, 0x6979e4u);  // FUN_00696d80
    esi = (esi + 1u);
    edi = edi + 0x3f0u;
    fA_6979eb = esi;
    fB_6979eb = rd32(m, ebx + 0x224u);
    if (int32_t(sext(fA_6979eb, 4)) < int32_t(sext(fB_6979eb, 4))) goto L_6979d3;
L_6979f3:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00697a00 Fn_00697a00
//   original FUN_00697a00; register inputs: esi; returns with ret 4
REFLEX_PORT_FN(0x00697a00, Fn_00697a00, "fx", reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fR_697a08 = 0;
    uint32_t fA_697a16 = 0;
    uint32_t fB_697a16 = 0;
    uint32_t fA_697a3d = 0;
    uint32_t fB_697a3d = 0;
    uint32_t fR_697a52 = 0;
    uint32_t fA_697a91 = 0;
    uint32_t fB_697a91 = 0;
    FCmp fC_697aa1{};
    uint32_t fR_697aa5 = 0;
    uint32_t fR_697aaa = 0;
    ecx = rd32(m, esi);
    esp = esp - 0x44u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, edi);
    fR_697a08 = (ecx & ecx);
    if ((fR_697a08 == 0)) goto L_697afc;
    eax = rd32(m, ecx + 0x1328u);
    fA_697a16 = rd8(m, eax + 0x14eu);
    fB_697a16 = 0x0u;
    if (fA_697a16 == fB_697a16) goto L_697afc;
    wr32(m, esi + 0x210u, 0x4u);
    edx = rd32(m, ecx);
    eax = (esi + 0x10u);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, edx + 0xccu);
    LIFT_CALL(eax, 0x697a3bu);
    ebp = 0u;
    fA_697a3d = rd32(m, esi + 0x210u);
    fB_697a3d = ebp;
    if (int32_t(sext(fA_697a3d, 4)) <= int32_t(sext(fB_697a3d, 4))) goto L_697a99;
    ecx = (esi + 0x40u);
    ebx = (esi + 0x1d0u);
    wr32(m, esp + 0xcu, ecx);
L_697a52:
    fR_697a52 = (rd8(m, 0x9e30e0u) & 0x1u);
    if (!(fR_697a52 == 0)) goto L_697a6c;
    wr32(m, 0x9e30e0u, (rd32(m, 0x9e30e0u) | 0x1u));
    wr32(m, 0x9e30dcu, 0x0u);
L_697a6c:
    edx = 0x9e30d4u;
    LIFT_CALL(0x402dc0u, 0x697a76u);  // FUN_00402dc0
    edx = rd32(m, esp + 0xcu);
    edi = eax;
    eax = 0x2u;
    LIFT_CALL(0x7c5120u, 0x697a86u);  // FUN_007c5120
    wr32(m, esp + 0xcu, rd32(m, esp + 0xcu) + 0x70u);
    wr32(m, ebx, eax);
    ebp = (ebp + 1u);
    ebx = ebx + 0x4u;
    fA_697a91 = ebp;
    fB_697a91 = rd32(m, esi + 0x210u);
    if (int32_t(sext(fA_697a91, 4)) < int32_t(sext(fB_697a91, 4))) goto L_697a52;
L_697a99:
    x0 = movss_m(rd32(m, esi + 0x1e0u));
    fC_697aa1 = fcmp32(x0.l(0), x0.l(0));
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_697aa1) | 2u) << 8);
    fR_697aa5 = (((eax >> 8) & 0xffu) & 0x44u);
    if (parity8(fR_697aa5)) goto L_697afc;
    fR_697aaa = (rd8(m, 0xd94ad8u) & 0x1u);
    x0 = movss_m(rd32(m, esp + 0x54u));
    eax = (esi + 0x10u);
    wr32(m, esp + 0x10u, x0.l(0));
    wr32(m, esp + 0x14u, eax);
    wr32(m, esp + 0x18u, esi);
    if (!(fR_697aaa == 0)) goto L_697ae8;
    wr32(m, 0xd94ad8u, (rd32(m, 0xd94ad8u) | 0x1u));
    ecx = 0xd94ae0u;
    LIFT_CALL(0x881a70u, 0x697adbu);  // FUN_00881a70
    esp -= 4; wr32(m, esp, 0x912b10u);
    LIFT_CALL(0x8f9fbfu, 0x697ae5u);  // _atexit
    esp = esp + 0x4u;
L_697ae8:
    esp -= 4; wr32(m, esp, 0x6979a0u);
    edx = (esp + 0x14u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, 0xd94ae0u);
    LIFT_CALL(0x881f00u, 0x697afcu);  // JobPool_Submit
L_697afc:
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x44u;
    LIFT_RET(4);
}

// 00698740 PlatformParticleManager2D__vf5
//   original PlatformParticleManager2D::vf5; register inputs: none; returns with ret 4
REFLEX_PORT_FN(0x00698740, PlatformParticleManager2D__vf5, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = rd32(m, esp + 0x4u);
    ecx = 0u;
    wr32(m, eax + 0xcu, ecx);
    wr32(m, eax + 0x8u, ecx);
    wr32(m, eax + 0x4u, ecx);
    wr32(m, eax + 0x10u, ecx);
    LIFT_RET(4);
}

// 00698760 PlatformParticleManager2D__vf4
//   original PlatformParticleManager2D::vf4; register inputs: none; returns with ret 12
REFLEX_PORT_FN(0x00698760, PlatformParticleManager2D__vf4, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = rd32(m, esp + 0xcu);
    ecx = rd32(m, esp + 0x8u);
    edx = rd32(m, esp + 0x4u);
    wr32(m, eax + 0xcu, ecx);
    ecx = 0u;
    wr32(m, eax + 0x8u, ecx);
    wr32(m, eax, edx);
    wr32(m, eax + 0x4u, ecx);
    eax = (ecx + 0x1u);
    LIFT_RET(12);
}

// 00698780 PlatformParticleManager2D__SpawnParticle
//   original PlatformParticleManager2D::SpawnParticle; register inputs: ecx; returns with ret 8
REFLEX_PORT_FN(0x00698780, PlatformParticleManager2D__SpawnParticle, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_698783 = 0;
    uint32_t fB_698783 = 0;
    uint32_t fA_69879f = 0;
    uint32_t fB_69879f = 0;
    uint32_t fR_6987a5 = 0;
    uint32_t fA_6987be = 0;
    uint32_t fB_6987be = 0;
    uint32_t fR_6987c4 = 0;
    uint32_t fA_6987d7 = 0;
    uint32_t fB_6987d7 = 0;
    uint32_t fA_698824 = 0;
    uint32_t fB_698824 = 0;
    esp -= 4; wr32(m, esp, edi);
    edi = ecx;
    fA_698783 = rd32(m, edi + 0x70u);
    fB_698783 = 0x0u;
    if (fA_698783 == fB_698783) goto L_698842;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esi = (edi + 0xc8u);
    ecx = 0u;
    edx = esi;
    eax = rd32(m, edx);
L_69879b:
    ebx = eax;
    ebx = (ebx | ecx);
    fA_69879f = eax;
    fB_69879f = rd32(m, edx);
    if (fA_69879f == fB_69879f) { wr32(m, edx, ebx); } else eax = fB_69879f;
    if (fA_69879f != fB_69879f) goto L_69879b;
    fR_6987a5 = (eax & eax);
    if ((fR_6987a5 == 0)) goto L_6987c9;
    ebx = rd32(m, 0x916020u) /* 8.295000272555398e-39 */;
    esp -= 4; wr32(m, esp, ebp);
L_6987b0:
    esp -= 4; wr32(m, esp, 0x0u);
    LIFT_CALL(ebx, 0x6987b4u);
    ecx = 0u;
    edx = esi;
    eax = rd32(m, edx);
L_6987ba:
    ebp = eax;
    ebp = (ebp | ecx);
    fA_6987be = eax;
    fB_6987be = rd32(m, edx);
    if (fA_6987be == fB_6987be) { wr32(m, edx, ebp); } else eax = fB_6987be;
    if (fA_6987be != fB_6987be) goto L_6987ba;
    fR_6987c4 = (eax & eax);
    if (!(fR_6987c4 == 0)) goto L_6987b0;
    ebp = rd32(m, esp); esp += 4;
L_6987c9:
    esi = (edi + 0xb0u);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(rd32(m, 0x916080u) /* 8.295241295891262e-39 */, 0x6987d6u);
    eax = (eax - 1u);
    fA_6987d7 = eax;
    fB_6987d7 = rd32(m, edi + 0x1cu);
    if (int32_t(sext(fA_6987d7, 4)) < int32_t(sext(fB_6987d7, 4))) goto L_6987e9;
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(rd32(m, 0x916084u) /* 8.295274927054406e-39 */, 0x6987e3u);
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    edi = rd32(m, esp); esp += 4;
    LIFT_RET(8);
L_6987e9:
    ecx = rd32(m, esp + 0x14u);
    eax = (eax << 7);
    eax = eax + rd32(m, edi + 0x14u);
    esi = eax;
    LIFT_CALL(0x692b50u, 0x6987fau);  // Particle_Copy
    ecx = rd32(m, esp + 0x10u);
    wr32(m, esi + 0x5cu, ecx);
    eax = rd32(m, edi + 0x70u);
    edx = rd32(m, eax + 0x44u);
    wr32(m, eax + 0x4cu, 0x1u);
    eax = rd32(m, edi + 0x70u);
    ecx = rd32(m, edx + 0x1cu);
    wr32(m, eax + 0x4cu, 0x0u);
    x0 = movss_m(rd32(m, esi + 0x54u));
    eax = cvttss2si(x0.l(0));
    fA_698824 = eax;
    fB_698824 = ecx;
    if (int32_t(sext(fA_698824, 4)) <= int32_t(sext(fB_698824, 4))) goto L_698840;
    edx = (eax >> 31) ? 0xffffffffu : 0;
    { uint32_t d_ = ecx; sdiv32(eax, edx, d_); }
    x2 = cvtsi2ss(x2, ecx);
    x1 = cvtsi2ss(x1, eax);
    x1 = mulss(x1, x2.l(0));
    x0 = subss(x0, x1.l(0));
    wr32(m, esi + 0x54u, x0.l(0));
L_698840:
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
L_698842:
    edi = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 0069b060 VehicleParticleEffectEmitter_ParticleEmitter__vf3
//   original VehicleParticleEffectEmitter_ParticleEmitter::vf3; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x0069b060, VehicleParticleEffectEmitter_ParticleEmitter__vf3, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x4u)));
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, 0xffffffffu);
    eax = ecx;
    esi = rd32(m, eax + 0xf0u);
    edx = rd32(m, eax + 0x10u);
    edx = rd32(m, edx);
    esp -= 4; wr32(m, esp, 0x1u);
    ecx = (eax + 0x10u);
    eax = rd32(m, eax + 0x4u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(edx, 0x69b084u);
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}
