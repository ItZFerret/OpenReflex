// Vehicle simulation ports (group "bike"), transliterated instruction by instruction from MXReflex.exe by the bike
// lifter (see native/game/notes/bike.md). Each function keeps the original's operations, order and memory traffic.
#include <stdexcept>
#include <utility>
#include "lift_rt.h"

// 004300a0 ModelNodeRef_SetMatrix: ECX = node reference, stack 4x4 matrix: stored in the owner's matrix table, then 7db7e0
//   marks the hierarchy dirty.
//   original FUN_004300a0; register inputs: ecx; ret 4
REFLEX_PORT_FN(0x004300a0, ModelNodeRef_SetMatrix, "bike", LIFT_OUTS) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    ecx = rd32(m, ecx);
    eax = rd32(m, ebp + 0x8u);
    x0 = ldx(m, eax);
    x1 = ldx(m, eax + 0x10u);
    x2 = ldx(m, eax + 0x20u);
    x3 = ldx(m, eax + 0x30u);
    eax = rd8(m, ecx + 0xbu);
    edx = rd32(m, ecx);
    eax = (eax << 6);
    eax = eax + rd32(m, edx + 0x18u);
    stx(m, eax, x0);
    stx(m, eax + 0x10u, x1);
    stx(m, eax + 0x20u, x2);
    stx(m, eax + 0x30u, x3);
    LIFT_CALL(0x7db7e0u, 0x4300dau);  // FUN_007db7e0
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 004301f0 rtti_shared_vf_004301f0: Shared slot (8 classes, ECX = this, stack out): copies the vec3 this+0xc0..0xc8.
//   original rtti_shared_vf_004301f0; register inputs: ecx; ret 4
REFLEX_PORT_FN(0x004301f0, rtti_shared_vf_004301f0, "bike", LIFT_OUTS) {
    LIFT_ENTRY();

    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0xc0u)));
    eax = rd32(m, esp + 0x4u);
    wr32(m, eax, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0xc4u)));
    wr32(m, eax + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0xc8u)));
    wr32(m, eax + 0x8u, fst32(m));
    fpop(m);
    LIFT_RET(4);
}

// 00430270 RavenVehicle_vf22: RavenVehicle slot 22: returns 0x1082.
//   original RavenVehicle::vf22; register inputs: none; ret 0
REFLEX_PORT_FN(0x00430270, RavenVehicle_vf22, "bike", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = 0x1082u;
    LIFT_RET(0);
}

// 00430490 RiderRecord_SetVec90: ECX = record, EAX = vec3 source: record+0x90..0x98.
//   original FUN_00430490; register inputs: ecx eax; ret 0
REFLEX_PORT_FN(0x00430490, RiderRecord_SetVec90, "bike", LIFT_OUTS) {
    LIFT_ENTRY();

    fpush(m, reflex_f80::from_f32(rd32(m, eax)));
    wr32(m, ecx + 0x90u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x4u)));
    wr32(m, ecx + 0x94u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x8u)));
    wr32(m, ecx + 0x98u, fst32(m));
    fpop(m);
    LIFT_RET(0);
}

// 004304b0 RiderRecord_SetFloatA0: EAX = record, xmm0 -> record+0xa0.
//   original FUN_004304b0; register inputs: eax xmm0; ret 0
REFLEX_PORT_FN(0x004304b0, RiderRecord_SetFloatA0, "bike", LIFT_OUTS) {
    LIFT_ENTRY();

    wr32(m, eax + 0xa0u, x0.l(0));
    LIFT_RET(0);
}

// 004304c0 RiderRecord_SetFloatA4: EAX = record, xmm0 -> record+0xa4.
//   original FUN_004304c0; register inputs: eax xmm0; ret 0
REFLEX_PORT_FN(0x004304c0, RiderRecord_SetFloatA4, "bike", LIFT_OUTS) {
    LIFT_ENTRY();

    wr32(m, eax + 0xa4u, x0.l(0));
    LIFT_RET(0);
}

// 004304d0 RiderRecord_SetFloatA8: EAX = record, xmm0 -> record+0xa8.
//   original FUN_004304d0; register inputs: eax xmm0; ret 0
REFLEX_PORT_FN(0x004304d0, RiderRecord_SetFloatA8, "bike", LIFT_OUTS) {
    LIFT_ENTRY();

    wr32(m, eax + 0xa8u, x0.l(0));
    LIFT_RET(0);
}

// 004304e0 RiderRecord_SetFloat88: EAX = record, xmm0 -> record+0x88.
//   original FUN_004304e0; register inputs: eax xmm0; ret 0
REFLEX_PORT_FN(0x004304e0, RiderRecord_SetFloat88, "bike", LIFT_OUTS) {
    LIFT_ENTRY();

    wr32(m, eax + 0x88u, x0.l(0));
    LIFT_RET(0);
}

// 004304f0 RiderRecord_SetFloatAC: EAX = record, xmm0 -> record+0xac.
//   original FUN_004304f0; register inputs: eax xmm0; ret 0
REFLEX_PORT_FN(0x004304f0, RiderRecord_SetFloatAC, "bike", LIFT_OUTS) {
    LIFT_ENTRY();

    wr32(m, eax + 0xacu, x0.l(0));
    LIFT_RET(0);
}

// 00430500 RiderRecord_SetVec60: ECX = record, EAX = vec3 source: record+0x60..0x68 (x87 copies).
//   original FUN_00430500; register inputs: ecx eax; ret 0
REFLEX_PORT_FN(0x00430500, RiderRecord_SetVec60, "bike", LIFT_OUTS) {
    LIFT_ENTRY();

    fpush(m, reflex_f80::from_f32(rd32(m, eax)));
    wr32(m, ecx + 0x60u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x4u)));
    wr32(m, ecx + 0x64u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x8u)));
    wr32(m, ecx + 0x68u, fst32(m));
    fpop(m);
    LIFT_RET(0);
}

// 00430520 RiderRecord_SetVec70: ECX = record, EAX = vec3 source: record+0x70..0x78.
//   original FUN_00430520; register inputs: ecx eax; ret 0
REFLEX_PORT_FN(0x00430520, RiderRecord_SetVec70, "bike", LIFT_OUTS) {
    LIFT_ENTRY();

    fpush(m, reflex_f80::from_f32(rd32(m, eax)));
    wr32(m, ecx + 0x70u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x4u)));
    wr32(m, ecx + 0x74u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x8u)));
    wr32(m, ecx + 0x78u, fst32(m));
    fpop(m);
    LIFT_RET(0);
}

// 00430540 RiderRecord_SetVec80: ECX = record, EAX = vec3 source: record+0x80..0x88.
//   original FUN_00430540; register inputs: ecx eax; ret 0
REFLEX_PORT_FN(0x00430540, RiderRecord_SetVec80, "bike", LIFT_OUTS) {
    LIFT_ENTRY();

    fpush(m, reflex_f80::from_f32(rd32(m, eax)));
    wr32(m, ecx + 0x80u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x4u)));
    wr32(m, ecx + 0x84u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x8u)));
    wr32(m, ecx + 0x88u, fst32(m));
    fpop(m);
    LIFT_RET(0);
}

// 00430810 Lerp_Clamped: xmm2 = t, stack a, b: t <= 0 -> a, t >= 1 -> b, else a + (b - a) * t; result xmm0.
//   original FUN_00430810; register inputs: xmm2; ret 0
REFLEX_PORT_FN(0x00430810, Lerp_Clamped, "bike", LIFT_OUTS) {
    LIFT_ENTRY();
    FCmp fC_430813{};
    FCmp fC_43081f{};
    x0 = X{};
    fC_430813 = fcmp32(x0.l(0), x2.l(0));
    if (fc_b(fC_430813)) goto L_43081f;
    x0 = movss_m(rd32(m, esp + 0x4u));
    LIFT_RET(0);
L_43081f:
    fC_43081f = fcmp32(x2.l(0), rd32(m, 0x931f24u) /* 1.0 */);
    x0 = movss_m(rd32(m, esp + 0x8u));
    if (!fc_b(fC_43081f)) goto L_430840;
    x1 = movss_m(rd32(m, esp + 0x4u));
    x0 = subss(x0, x1.l(0));
    x0 = mulss(x0, x2.l(0));
    x0 = addss(x0, x1.l(0));
L_430840:
    LIFT_RET(0);
}

// 00430850 Simulation_InitInputBlock: EAX = 64-byte input block: all zero except +0x38 (brake-hold latch) = 1.
//   original FUN_00430850; register inputs: eax; ret 0
REFLEX_PORT_FN(0x00430850, Simulation_InitInputBlock, "bike", LIFT_OUTS) {
    LIFT_ENTRY();

    x0 = X{};
    ecx = 0u;
    wr32(m, eax, x0.l(0));
    wr32(m, eax + 0x4u, x0.l(0));
    wr32(m, eax + 0x8u, x0.l(0));
    wr32(m, eax + 0xcu, x0.l(0));
    wr32(m, eax + 0x10u, x0.l(0));
    wr32(m, eax + 0x14u, x0.l(0));
    wr32(m, eax + 0x18u, x0.l(0));
    wr32(m, eax + 0x1cu, x0.l(0));
    wr32(m, eax + 0x20u, ecx);
    wr32(m, eax + 0x24u, ecx);
    wr32(m, eax + 0x28u, ecx);
    wr32(m, eax + 0x2cu, ecx);
    wr32(m, eax + 0x30u, ecx);
    wr32(m, eax + 0x34u, ecx);
    wr32(m, eax + 0x38u, 0x1u);
    wr32(m, eax + 0x3cu, ecx);
    LIFT_RET(0);
}

// 004308a0 Simulation_ReadControllerInput: EAX = controller index, EDI = 64-byte input block: samples the action axes and
//   buttons (simulation-input.md).
//   original Simulation_ReadControllerInput; register inputs: eax edi; ret 0
REFLEX_PORT_FN(0x004308a0, Simulation_ReadControllerInput, "bike", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4308a8 = 0;
    uint32_t fB_4308a8 = 0;
    FCmp fC_430968{};
    uint32_t fR_43096d = 0;
    FCmp fC_430989{};
    uint32_t fR_4309b3 = 0;
    uint32_t fR_4309cc = 0;
    uint32_t fA_430a2d = 0;
    uint32_t fB_430a2d = 0;
    uint32_t fR_430a4a = 0;
    uint32_t fA_430a6d = 0;
    uint32_t fB_430a6d = 0;
    uint32_t fA_430aae = 0;
    uint32_t fR_430aae = 0;
    uint32_t fA_430ace = 0;
    uint32_t fR_430ace = 0;
    uint32_t fA_430ad4 = 0;
    uint32_t fB_430ad4 = 0;
    FCmp fC_430aea{};
    FCmp fC_430afa{};
    FCmp fC_430b11{};
    FCmp fC_430b2c{};
    FCmp fC_430b36{};
    esp = esp - 0x10u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    fA_4308a8 = esi;
    fB_4308a8 = 0xffffffffu;
    if (fA_4308a8 == fB_4308a8) goto L_430b42;
    ecx = rd32(m, 0xa95844u);
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x8u);
    LIFT_CALL(edx, 0x4308beu);
    LIFT_CALL(0x548fc0u, 0x4308c3u);  // FUN_00548fc0
    ebx = eax;
    LIFT_CALL(0x549050u, 0x4308cau);  // FUN_00549050
    ebp = eax;
    eax = rd32(m, ebx);
    edx = rd32(m, eax + 0x10u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, 0x7u);
    ecx = ebx;
    LIFT_CALL(edx, 0x4308d8u);
    wr32(m, esp + 0xcu, fst32(m));
    fpop(m);
    eax = rd32(m, ebp);
    edx = rd32(m, eax + 0x10u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, 0x7u);
    ecx = ebp;
    LIFT_CALL(edx, 0x4308e9u);
    wr32(m, esp + 0x10u, fst32(m));
    fpop(m);
    eax = esi;
    LIFT_CALL(0x548f20u, 0x4308f4u);  // FUN_00548f20
    ebx = eax;
    eax = esi;
    LIFT_CALL(0x548f70u, 0x4308fdu);  // FUN_00548f70
    ebp = eax;
    eax = rd32(m, ebx);
    edx = rd32(m, eax + 0x10u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, 0x7u);
    ecx = ebx;
    LIFT_CALL(edx, 0x43090bu);
    wr32(m, esp + 0x14u, fst32(m));
    fpop(m);
    eax = rd32(m, ebp);
    edx = rd32(m, eax + 0x10u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, 0x7u);
    ecx = ebp;
    LIFT_CALL(edx, 0x43091cu);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x14u)));
    wr32(m, edi + 0x10u, fst32(m));
    fpop(m);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, 0x7u);
    wr32(m, edi + 0x14u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x14u)));
    wr32(m, edi + 0x18u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x18u)));
    wr32(m, edi + 0x1cu, fst32(m));
    fpop(m);
    eax = rd32(m, esi * 4 + 0x9c52c4u);
    ecx = (eax + 0x348u);
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x10u);
    LIFT_CALL(edx, 0x43094bu);
    wr32(m, edi, fst32(m));
    fpop(m);
    eax = rd32(m, esi * 4 + 0x9c52c4u);
    ecx = (eax + 0x748u);
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x10u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, 0x7u);
    LIFT_CALL(edx, 0x430964u);
    fpush(m, reflex_f80::from_int(0));
    std::swap(st(m, 0), st(m, 1));
    fC_430968 = f80cmp(st(m, 0), st(m, 1));
    fpop(m);
    st(m, 0) = st(m, 0);
    fpop(m);
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_430968) | 2u) << 8);
    fR_43096d = (((eax >> 8) & 0xffu) & 0x44u);
    if (!parity8(fR_43096d)) goto L_43097c;
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    goto L_43097f;
L_43097c:
    x0 = X{};
L_43097f:
    wr32(m, edi + 0x4u, x0.l(0));
    LIFT_CALL(0x548660u, 0x430989u);  // FUN_00548660
    fC_430989 = fcmp32(x0.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    if ((fc_b(fC_430989) || fc_z(fC_430989))) goto L_430999;
    eax = 0x1u;
    goto L_43099b;
L_430999:
    eax = 0u;
L_43099b:
    wr32(m, edi + 0x20u, eax);
    eax = rd32(m, esi * 4 + 0x9c52c4u);
    ecx = (eax + 0x1134u);
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x2cu);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(edx, 0x4309b3u);
    fR_4309b3 = (eax & eax);
    if (!(fR_4309b3 == 0)) goto L_4309d0;
    eax = rd32(m, esi * 4 + 0x9c52c4u);
    ecx = (eax + 0x11d8u);
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x2cu);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(edx, 0x4309ccu);
    fR_4309cc = (eax & eax);
    if ((fR_4309cc == 0)) goto L_4309f9;
L_4309d0:
    eax = rd32(m, esi * 4 + 0x9c52c4u);
    ecx = (eax + 0x1134u);
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x10u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, 0x7u);
    LIFT_CALL(edx, 0x4309e7u);
    wr32(m, edi + 0x8u, fst32(m));
    fpop(m);
    eax = rd32(m, esi * 4 + 0x9c52c4u);
    ecx = (eax + 0x11d8u);
    goto L_430a20;
L_4309f9:
    eax = rd32(m, esi * 4 + 0x9c52c4u);
    ecx = (eax + 0x148u);
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x10u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, 0x7u);
    LIFT_CALL(edx, 0x430a10u);
    wr32(m, edi + 0x8u, fst32(m));
    fpop(m);
    eax = rd32(m, esi * 4 + 0x9c52c4u);
    ecx = (eax + 0x148u);
L_430a20:
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x10u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, 0x7u);
    LIFT_CALL(edx, 0x430a2au);
    wr32(m, edi + 0xcu, fst32(m));
    fpop(m);
    fA_430a2d = rd32(m, edi + 0x3cu);
    fB_430a2d = 0x0u;
    if (fA_430a2d == fB_430a2d) goto L_430a65;
    eax = rd32(m, esi * 4 + 0x9c52c4u);
    ecx = (eax + 0x800u);
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, 0x7u);
    LIFT_CALL(edx, 0x430a4au);
    fR_430a4a = (eax & eax);
    if ((fR_430a4a == 0)) goto L_430a65;
    x0 = X{};
    wr32(m, edi + 0x8u, x0.l(0));
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    wr32(m, edi + 0xcu, x0.l(0));
    goto L_430a6d;
L_430a65:
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
L_430a6d:
    fA_430a6d = rd32(m, edi + 0x38u);
    fB_430a6d = 0x0u;
    if (fA_430a6d == fB_430a6d) goto L_430a7d;
    wr32(m, edi + 0x8u, x0.l(0));
    wr32(m, edi + 0xcu, x0.l(0));
L_430a7d:
    eax = rd32(m, esi * 4 + 0x9c52c4u);
    ecx = (eax + 0x548u);
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, 0x7u);
    LIFT_CALL(edx, 0x430a94u);
    wr32(m, edi + 0x2cu, eax);
    eax = rd32(m, esi * 4 + 0x9c52c4u);
    ecx = (eax + 0xfecu);
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x4u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, 0x7u);
    LIFT_CALL(edx, 0x430aaeu);
    fA_430aae = eax;
    fR_430aae = (0u - fA_430aae);
    eax = fR_430aae;
    eax = eax - eax - ((fA_430aae != 0) ? 1u : 0u);
    eax = (0u - eax);
    wr32(m, edi + 0x30u, eax);
    eax = rd32(m, esi * 4 + 0x9c52c4u);
    ecx = (eax + 0x1090u);
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x4u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, 0x7u);
    LIFT_CALL(edx, 0x430aceu);
    fA_430ace = eax;
    fR_430ace = (0u - fA_430ace);
    eax = fR_430ace;
    eax = eax - eax - ((fA_430ace != 0) ? 1u : 0u);
    eax = (0u - eax);
    fA_430ad4 = rd32(m, edi + 0x28u);
    fB_430ad4 = 0x0u;
    wr32(m, edi + 0x34u, eax);
    if (fA_430ad4 == fB_430ad4) goto L_430b25;
    LIFT_CALL(0x5485e0u, 0x430ae2u);  // FUN_005485e0
    wr32(m, esp + 0xcu, fst32(m));
    fpush(m, reflex_f80::from_int(0));
    std::swap(st(m, 0), st(m, 1));
    fC_430aea = f80cmp(st(m, 0), st(m, 1));
    fpop(m);
    st(m, 0) = st(m, 0);
    fpop(m);
    if ((fc_b(fC_430aea) || fc_z(fC_430aea))) goto L_430b08;
    x0 = movss_m(rd32(m, edi));
    x1 = movss_m(rd32(m, esp + 0xcu));
    fC_430afa = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_430afa) || fc_z(fC_430afa))) goto L_430b02;
    x0 = x1;
L_430b02:
    wr32(m, edi, x0.l(0));
    goto L_430b25;
L_430b08:
    x0 = movss_m(rd32(m, esp + 0xcu));
    x1 = X{};
    fC_430b11 = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_430b11) || fc_z(fC_430b11))) goto L_430b25;
    LIFT_CALL(0x401000u, 0x430b1bu);  // FUN_00401000
    wr32(m, edi + 0x8u, x0.l(0));
    wr32(m, edi + 0xcu, x0.l(0));
L_430b25:
    x1 = movss_m(rd32(m, edi));
    x0 = X{};
    fC_430b2c = fcmp32(x1.l(0), x0.l(0));
    if (!(fc_b(fC_430b2c) || fc_z(fC_430b2c))) goto L_430b3b;
    x1 = movss_m(rd32(m, edi + 0x4u));
    fC_430b36 = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_430b36) || fc_z(fC_430b36))) goto L_430b42;
L_430b3b:
    wr32(m, edi + 0x38u, 0x0u);
L_430b42:
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x10u;
    LIFT_RET(0);
}

// 00430eb0 RavenSimulation_vf1: Slot 1 (ECX = S, stack frameDt): pending tuning reload (slot 8), then UpdateFixedSteps when
//   S+b0.
//   original RavenSimulation::vf1; register inputs: ecx; ret 4
REFLEX_PORT_FN(0x00430eb0, RavenSimulation_vf1, "bike", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_430ed4 = 0;
    uint32_t fB_430ed4 = 0;
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x20u);
    LIFT_CALL(edx, 0x430ebau);
    ecx = rd32(m, 0xa95844u);
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x8u);
    LIFT_CALL(edx, 0x430ec7u);
    edx = rd32(m, eax);
    esp -= 4; wr32(m, esp, 0x0u);
    ecx = eax;
    eax = rd32(m, edx + 0x7cu);
    esp -= 4; wr32(m, esp, 0x26u);
    LIFT_CALL(eax, 0x430ed4u);
    fA_430ed4 = rd32(m, esi + 0xb0u);
    fB_430ed4 = 0x0u;
    if (fA_430ed4 == fB_430ed4) goto L_430eec;
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x8u)));
    esp -= 4; wr32(m, esp, ecx);
    ecx = esi;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x430ef0u, 0x430eecu);  // RavenSimulation::UpdateFixedSteps
L_430eec:
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00430ef0 RavenSimulation_UpdateFixedSteps: Fixed-step scheduler (ECX = S, stack frameDt): ceil((frameDt - lead) / fixedDt)
//   steps capped by S+b8, 431d60 per step, then interpolation (physics-timing.md).
//   original RavenSimulation::UpdateFixedSteps; register inputs: ecx; ret 4
REFLEX_PORT_FN(0x00430ef0, RavenSimulation_UpdateFixedSteps, "bike", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_430f10 = 0;
    uint32_t fB_430f10 = 0;
    uint32_t fA_430f17 = 0;
    uint32_t fB_430f17 = 0;
    FCmp fC_430f3a{};
    uint32_t fA_430f49 = 0;
    uint32_t fB_430f49 = 0;
    FCmp fC_43103b{};
    FCmp fC_43108e{};
    FCmp fC_431191{};
    FCmp fC_43125a{};
    FCmp fC_431337{};
    uint32_t fA_4314c6 = 0;
    uint32_t fB_4314c6 = 0;
    uint32_t fA_431501 = 0;
    uint32_t fB_431501 = 0;
    uint32_t fA_43152b = 0;
    uint32_t fB_43152b = 0;
    uint32_t fA_431537 = 0;
    uint32_t fB_431537 = 0;
    uint32_t fA_431540 = 0;
    uint32_t fB_431540 = 0;
    uint32_t fA_431545 = 0;
    uint32_t fB_431545 = 0;
    uint32_t fA_43154d = 0;
    uint32_t fB_43154d = 0;
    uint32_t fA_431594 = 0;
    uint32_t fB_431594 = 0;
    uint32_t fA_43159d = 0;
    uint32_t fB_43159d = 0;
    uint32_t fA_4315a8 = 0;
    uint32_t fB_4315a8 = 0;
    uint32_t fA_4315b6 = 0;
    uint32_t fB_4315b6 = 0;
    uint32_t fA_4315eb = 0;
    uint32_t fB_4315eb = 0;
    uint32_t fA_431615 = 0;
    uint32_t fB_431615 = 0;
    uint32_t fR_431623 = 0;
    uint32_t fA_431629 = 0;
    uint32_t fB_431629 = 0;
    uint32_t fA_431650 = 0;
    uint32_t fB_431650 = 0;
    uint32_t fR_43165e = 0;
    uint32_t fA_43166a = 0;
    uint32_t fB_43166a = 0;
    uint32_t fR_43167b = 0;
    uint32_t fA_4316ad = 0;
    uint32_t fB_4316ad = 0;
    uint32_t fA_4316bd = 0;
    uint32_t fB_4316bd = 0;
    uint32_t fA_4316eb = 0;
    uint32_t fB_4316eb = 0;
    uint32_t fA_4316f3 = 0;
    uint32_t fB_4316f3 = 0;
    FCmp fC_4317d5{};
    FCmp fC_431828{};
    FCmp fC_43192b{};
    FCmp fC_4319f4{};
    FCmp fC_431ad1{};
    uint32_t fA_431c5d = 0;
    uint32_t fB_431c5d = 0;
    uint32_t fA_431c69 = 0;
    uint32_t fB_431c69 = 0;
    uint32_t fA_431c6f = 0;
    uint32_t fB_431c6f = 0;
    uint32_t fR_431cad = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x104u;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp + 0x100u, eax);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    ebx = 0u;
    fA_430f10 = rd32(m, esi + 0x4u);
    fB_430f10 = 0x1u;
    esp -= 4; wr32(m, esp, edi);
    if (fA_430f10 != fB_430f10) goto L_430f23;
    fA_430f17 = rd32(m, esi + 0x237cu);
    fB_430f17 = ebx;
    if (fA_430f17 == fB_430f17) goto L_431cd4;
L_430f23:
    x1 = movss_m(rd32(m, ebp + 0x8u));
    x2 = movss_m(rd32(m, esi + 0xc0u));
    x3 = X{};
    x0 = x1;
    x0 = subss(x0, x2.l(0));
    fC_430f3a = fcmp32(x3.l(0), x0.l(0));
    wr32(m, esp + 0x14u, x0.l(0));
    if (fc_b(fC_430f3a)) goto L_4314e9;
    fA_430f49 = rd32(m, esi + 0xecu);
    fB_430f49 = ebx;
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x2 = subss(x2, x1.l(0));
    x1 = x0;
    wr32(m, esi + 0xc0u, x2.l(0));
    x2 = divss(x2, rd32(m, esi + 0xbcu));
    x1 = subss(x1, x2.l(0));
    x0 = subss(x0, x1.l(0));
    wr32(m, esp + 0x4cu, x1.l(0));
    wr32(m, esi + 0xb4u, ebx);
    x1 = shufps(x1, x1, 0x0);
    x0 = shufps(x0, x0, 0x0);
    stx(m, esp + 0x60u, x0);
    stx(m, esp + 0x70u, x1);
    if (int32_t(sext(fA_430f49, 4)) <= int32_t(sext(fB_430f49, 4))) goto L_431cd4;
    x2 = X{};
    stx(m, esp + 0x80u, x2);
    edi = (esi + 0xf0u);
    goto L_430fba;
L_430fb0:
    x0 = ldx(m, esp + 0x60u);
    x1 = ldx(m, esp + 0x70u);
L_430fba:
    x2 = ldx(m, edi + 0x80u);
    x2 = mulps(x2, x0);
    x0 = ldx(m, edi);
    x1 = mulps(x1, x0);
    x2 = addps(x2, x1);
    stx(m, edi + 0x90u, x2);
    x0 = ldx(m, edi + 0x6c0u);
    stx(m, esp + 0x20u, x0);
    x0 = ldx(m, edi + 0x640u);
    x1 = movss_m(rd32(m, esp + 0x24u));
    stx(m, esp + 0x30u, x0);
    x1 = mulss(x1, rd32(m, esp + 0x34u));
    x0 = movss_m(rd32(m, esp + 0x30u));
    x0 = mulss(x0, rd32(m, esp + 0x20u));
    x0 = addss(x0, x1.l(0));
    x1 = movss_m(rd32(m, esp + 0x28u));
    x1 = mulss(x1, rd32(m, esp + 0x38u));
    x0 = addss(x0, x1.l(0));
    x1 = movss_m(rd32(m, esp + 0x3cu));
    x1 = mulss(x1, rd32(m, esp + 0x2cu));
    x0 = addss(x0, x1.l(0));
    LIFT_CALL(0x401000u, 0x43102du);  // FUN_00401000
    wr32(m, esp + 0x10u, x0.l(0));
    LIFT_CALL(0x401040u, 0x431038u);  // FUN_00401040
    x3 = x0;
    fC_43103b = fcmp32(x3.l(0), rd32(m, 0x9ddfc8u));
    wr32(m, esp + 0x44u, x3.l(0));
    if (fc_b(fC_43103b)) goto L_43108b;
    x0 = cvtss2sd(x0, rd32(m, esp + 0x10u));
    x1 = x0;
    x1 = mulsd(x1, x0.lo);
    x0 = cvtsd2ss(x0, x1.lo);
    x1 = movsd_m(rd64(m, 0x977638u) /* 1.0 */);
    x0 = cvtss2sd(x0, x0.l(0));
    x1 = subsd(x1, x0.lo);
    x0 = cvtpd2ps(x1);
    LIFT_CALL(0x401040u, 0x431075u);  // FUN_00401040
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, x0.l(0));
    LIFT_CALL(0x42c230u, 0x431080u);  // FUN_0042c230
    x3 = movss_m(rd32(m, esp + 0x48u));
    esp = esp + 0x4u;
    goto L_43108e;
L_43108b:
    x0 = x3;
L_43108e:
    fC_43108e = fcmp32(x3.l(0), rd32(m, 0x9ddfc8u));
    x5 = movsd_m(rd64(m, 0x9777b0u) /* 0.5648093223571777 */);
    x1 = cvtss2sd(x1, x0.l(0));
    x2 = x1;
    x2 = mulsd(x2, x1.lo);
    x1 = cvtsd2ss(x1, x2.lo);
    x2 = cvtss2sd(x2, x1.l(0));
    x4 = x2;
    x4 = mulsd(x4, x2.lo);
    x2 = cvtsd2ss(x2, x4.lo);
    x4 = cvtss2sd(x4, x1.l(0));
    x4 = mulsd(x4, rd64(m, 0x9777a8u) /* 0.04992850869894028 */);
    x5 = subsd(x5, x4.lo);
    x4 = cvtpd2ps(x5);
    x5 = cvtss2sd(x5, x4.l(0));
    x4 = cvtss2sd(x4, x2.l(0));
    x5 = mulsd(x5, x4.lo);
    x4 = movsd_m(rd64(m, 0x977638u) /* 1.0 */);
    x6 = cvtss2sd(x6, x1.l(0));
    x6 = mulsd(x6, rd64(m, 0x9777a0u) /* 1.4430354833602905 */);
    x7 = x4;
    x7 = subsd(x7, x6.lo);
    x6 = cvtsd2ss(x6, x7.lo);
    x7 = movsd_m(rd64(m, 0x977798u) /* 0.3993034064769745 */);
    x6 = cvtss2sd(x6, x6.l(0));
    x5 = addsd(x5, x6.lo);
    x6 = x4;
    x6 = divsd(x6, x5.lo);
    x5 = cvtsd2ss(x5, x6.lo);
    x6 = cvtss2sd(x6, x1.l(0));
    x6 = mulsd(x6, rd64(m, 0x977790u) /* 0.0193784236907959 */);
    x7 = subsd(x7, x6.lo);
    x2 = cvtss2sd(x2, x2.l(0));
    x1 = cvtss2sd(x1, x1.l(0));
    x1 = mulsd(x1, rd64(m, 0x977788u) /* 1.2763687372207642 */);
    x6 = cvtpd2ps(x7);
    x6 = cvtss2sd(x6, x6.l(0));
    x6 = mulsd(x6, x2.lo);
    x2 = x4;
    x2 = subsd(x2, x1.lo);
    x1 = cvtsd2ss(x1, x2.lo);
    x1 = cvtss2sd(x1, x1.l(0));
    x0 = cvtss2sd(x0, x0.l(0));
    x6 = addsd(x6, x1.lo);
    x6 = mulsd(x6, x0.lo);
    x0 = cvtsd2ss(x0, x6.lo);
    x5 = cvtss2sd(x5, x5.l(0));
    x0 = cvtss2sd(x0, x0.l(0));
    x5 = mulsd(x5, x0.lo);
    x1 = cvtpd2ps(x5);
    if (fc_b(fC_43108e)) goto L_43118b;
    x0 = cvtss2sd(x0, x1.l(0));
    x1 = movsd_m(rd64(m, 0x977510u) /* 1.5707963705062866 */);
    x1 = subsd(x1, x0.lo);
    x1 = cvtpd2ps(x1);
L_43118b:
    x0 = movss_m(rd32(m, esp + 0x10u));
    fC_431191 = fcmp32(x0.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    if (fc_b(fC_431191)) goto L_43119f;
    x0 = x1;
    goto L_4311ab;
L_43119f:
    x0 = movss_m(rd32(m, 0x9774a8u) /* -0.0 */);
    x0 = subss(x0, x1.l(0));
L_4311ab:
    x1 = movsd_m(rd64(m, 0x977510u) /* 1.5707963705062866 */);
    x0 = cvtps2pd(x0.lo);
    x1 = subsd(x1, x0.lo);
    x0 = cvtpd2ps(x1);
    wr32(m, esp + 0x48u, x0.l(0));
    x0 = addss(x0, rd32(m, 0x977508u) /* 1.5707963705062866 */);
    LIFT_CALL(0x401000u, 0x4311d1u);  // FUN_00401000
    x1 = x0;
    x1 = mulss(x1, rd32(m, 0x9774a0u) /* 0.15915493667125702 */);
    eax = cvttss2si(x1.l(0));
    x1 = cvtsi2ss(x1, eax);
    x1 = mulss(x1, rd32(m, 0x97749cu) /* 6.2831854820251465 */);
    x0 = subss(x0, x1.l(0));
    x1 = movss_m(rd32(m, 0x977498u) /* 3.1415927410125732 */);
    x1 = subss(x1, x0.l(0));
    x0 = x1;
    x0 = mulss(x0, x1.l(0));
    x2 = x0;
    x2 = mulss(x2, rd32(m, 0x935d50u) /* -2.1972959984850604e-07 */);
    x2 = addss(x2, rd32(m, 0x935d48u) /* 2.4202940039685927e-05 */);
    x1 = x0;
    x1 = mulss(x1, x0.l(0));
    x3 = x0;
    x3 = mulss(x3, rd32(m, 0x935d44u) /* -0.0013858790043741465 */);
    x0 = mulss(x0, rd32(m, 0x935d40u) /* -0.4999941885471344 */);
    x3 = addss(x3, rd32(m, 0x935d4cu) /* 0.04165977984666824 */);
    x0 = addss(x0, rd32(m, 0x935d3cu) /* 0.9999992251396179 */);
    x2 = mulss(x2, x1.l(0));
    x2 = addss(x2, x3.l(0));
    x3 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x2 = mulss(x2, x1.l(0));
    x1 = movss_m(rd32(m, esp + 0x10u));
    fC_43125a = fcmp32(x1.l(0), x3.l(0));
    x2 = addss(x2, x0.l(0));
    x0 = x3;
    x0 = divss(x0, x2.l(0));
    wr32(m, esp + 0x1cu, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x48u));
    x2 = x0;
    x2 = mulss(x2, rd32(m, esp + 0x4cu));
    wr32(m, esp + 0x14u, x2.l(0));
    if (fc_b(fC_43125a)) goto L_43128d;
    x1 = x3;
    goto L_431331;
L_43128d:
    x0 = subss(x0, x2.l(0));
    x0 = addss(x0, rd32(m, 0x977508u) /* 1.5707963705062866 */);
    LIFT_CALL(0x401000u, 0x43129eu);  // FUN_00401000
    x1 = x0;
    x1 = mulss(x1, rd32(m, 0x9774a0u) /* 0.15915493667125702 */);
    ecx = cvttss2si(x1.l(0));
    x1 = cvtsi2ss(x1, ecx);
    x1 = mulss(x1, rd32(m, 0x97749cu) /* 6.2831854820251465 */);
    x0 = subss(x0, x1.l(0));
    x1 = movss_m(rd32(m, 0x977498u) /* 3.1415927410125732 */);
    x1 = subss(x1, x0.l(0));
    x0 = x1;
    x0 = mulss(x0, x1.l(0));
    x1 = x0;
    x1 = mulss(x1, rd32(m, 0x935d50u) /* -2.1972959984850604e-07 */);
    x1 = addss(x1, rd32(m, 0x935d48u) /* 2.4202940039685927e-05 */);
    x2 = x0;
    x2 = mulss(x2, x0.l(0));
    x1 = mulss(x1, x2.l(0));
    x3 = x0;
    x3 = mulss(x3, rd32(m, 0x935d44u) /* -0.0013858790043741465 */);
    x3 = addss(x3, rd32(m, 0x935d4cu) /* 0.04165977984666824 */);
    x0 = mulss(x0, rd32(m, 0x935d40u) /* -0.4999941885471344 */);
    x0 = addss(x0, rd32(m, 0x935d3cu) /* 0.9999992251396179 */);
    x1 = addss(x1, x3.l(0));
    x3 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x1 = mulss(x1, x2.l(0));
    x2 = movss_m(rd32(m, esp + 0x14u));
    x1 = addss(x1, x0.l(0));
    x1 = mulss(x1, rd32(m, esp + 0x1cu));
L_431331:
    x0 = movss_m(rd32(m, esp + 0x10u));
    fC_431337 = fcmp32(x0.l(0), x3.l(0));
    wr32(m, esp + 0x18u, x1.l(0));
    if (fc_b(fC_431337)) goto L_43134a;
    x0 = X{};
    goto L_4313e2;
L_43134a:
    x0 = x2;
    x0 = addss(x0, rd32(m, 0x977508u) /* 1.5707963705062866 */);
    LIFT_CALL(0x401000u, 0x43135au);  // FUN_00401000
    x1 = x0;
    x1 = mulss(x1, rd32(m, 0x9774a0u) /* 0.15915493667125702 */);
    edx = cvttss2si(x1.l(0));
    x1 = cvtsi2ss(x1, edx);
    x1 = mulss(x1, rd32(m, 0x97749cu) /* 6.2831854820251465 */);
    x0 = subss(x0, x1.l(0));
    x1 = movss_m(rd32(m, 0x977498u) /* 3.1415927410125732 */);
    x1 = subss(x1, x0.l(0));
    x1 = mulss(x1, x1.l(0));
    x0 = x1;
    x0 = mulss(x0, rd32(m, 0x935d50u) /* -2.1972959984850604e-07 */);
    x0 = addss(x0, rd32(m, 0x935d48u) /* 2.4202940039685927e-05 */);
    x2 = x1;
    x2 = mulss(x2, x1.l(0));
    x3 = x1;
    x3 = mulss(x3, rd32(m, 0x935d44u) /* -0.0013858790043741465 */);
    x3 = addss(x3, rd32(m, 0x935d4cu) /* 0.04165977984666824 */);
    x1 = mulss(x1, rd32(m, 0x935d40u) /* -0.4999941885471344 */);
    x1 = addss(x1, rd32(m, 0x935d3cu) /* 0.9999992251396179 */);
    x0 = mulss(x0, x2.l(0));
    x0 = addss(x0, x3.l(0));
    x0 = mulss(x0, x2.l(0));
    x0 = addss(x0, x1.l(0));
    x0 = mulss(x0, rd32(m, esp + 0x1cu));
    x1 = movss_m(rd32(m, esp + 0x18u));
L_4313e2:
    x2 = movss_m(rd32(m, esp + 0x3cu));
    x3 = movss_m(rd32(m, esp + 0x2cu));
    x2 = mulss(x2, x0.l(0));
    x3 = mulss(x3, x1.l(0));
    x2 = addss(x2, x3.l(0));
    x3 = movss_m(rd32(m, esp + 0x20u));
    wr32(m, esp + 0x5cu, x2.l(0));
    x2 = movss_m(rd32(m, esp + 0x30u));
    x2 = mulss(x2, x0.l(0));
    x3 = mulss(x3, x1.l(0));
    x2 = addss(x2, x3.l(0));
    x3 = movss_m(rd32(m, esp + 0x34u));
    x3 = mulss(x3, x0.l(0));
    wr32(m, esp + 0x50u, x2.l(0));
    x2 = movss_m(rd32(m, esp + 0x24u));
    x2 = mulss(x2, x1.l(0));
    x2 = addss(x2, x3.l(0));
    wr32(m, esp + 0x54u, x2.l(0));
    x2 = movss_m(rd32(m, esp + 0x28u));
    x2 = mulss(x2, x1.l(0));
    x1 = movss_m(rd32(m, esp + 0x38u));
    x1 = mulss(x1, x0.l(0));
    x2 = addss(x2, x1.l(0));
    wr32(m, esp + 0x58u, x2.l(0));
    x1 = ldx(m, esp + 0x50u);
    x3 = x1;
    x3 = mulps(x3, x1);
    x0 = x3;
    x0 = shufps(x0, x3, 0xee);
    x2 = x0;
    x2 = addps(x2, x3);
    stx(m, edi + 0x6d0u, x1);
    x3 = ldx(m, 0x92dc20u);
    x2 = shufps(x2, x2, 0x40);
    x0 = shufps(x0, x2, 0xf0);
    x0 = addps(x0, x2);
    x0 = shufps(x0, x0, 0xaa);
    x2 = sqrtps(x2, x0);
    x0 = cmpps<4>(x0, x3);
    x3 = ldx(m, esp + 0x80u);
    x3 = cmpps<4>(x3, x2);
    x1 = divps(x1, x2);
    x3 = andps(x3, x1);
    x1 = ldx(m, 0x92dc30u);
    x3 = andps(x3, x0);
    x0 = andnps(x0, x1);
    x3 = orps(x3, x0);
    stx(m, edi + 0x6d0u, x3);
    ebx = (ebx + 1u);
    edi = edi + 0xa0u;
    fA_4314c6 = ebx;
    fB_4314c6 = rd32(m, esi + 0xecu);
    if (int32_t(sext(fA_4314c6, 4)) < int32_t(sext(fB_4314c6, 4))) goto L_430fb0;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp + 0x100u);
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x4314e3u);  // __security_check_cookie
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_4314e9:
    x0 = divss(x0, rd32(m, esi + 0xbcu));
    LIFT_CALL(0x42c1d0u, 0x4314f6u);  // FUN_0042c1d0
    LIFT_CALL(0x8fa0d0u, 0x4314fbu);  // FUN_008fa0d0
    ecx = rd32(m, esi + 0xb8u);
    fA_431501 = eax;
    fB_431501 = ecx;
    wr32(m, esi + 0xb4u, eax);
    if (int32_t(sext(fA_431501, 4)) <= int32_t(sext(fB_431501, 4))) goto L_431516;
    x0 = X{};
    wr32(m, esi + 0xb4u, ecx);
    goto L_431528;
L_431516:
    x0 = cvtsi2ss(x0, eax);
    x0 = mulss(x0, rd32(m, esi + 0xbcu));
    x0 = subss(x0, rd32(m, esp + 0x14u));
L_431528:
    eax = rd32(m, esi + 0x4u);
    fA_43152b = eax;
    fB_43152b = ebx;
    eax = fA_43152b - fB_43152b;
    wr32(m, esi + 0xc0u, x0.l(0));
    if (fA_43152b == fB_43152b) goto L_431545;
    fA_431537 = eax;
    fB_431537 = 0x1u;
    eax = fA_431537 - fB_431537;
    if (fA_431537 == fB_431537) goto L_431629;
    fA_431540 = eax;
    fB_431540 = 0x1u;
    eax = fA_431540 - fB_431540;
    if (fA_431540 != fB_431540) goto L_431561;
L_431545:
    fA_431545 = rd32(m, esi + 0x2de0u);
    fB_431545 = ebx;
    if (fA_431545 == fB_431545) goto L_431561;
    fA_43154d = rd32(m, esi + 0x2e7cu);
    fB_43154d = ebx;
    if (fA_43154d != fB_43154d) goto L_431561;
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x88u);
    ecx = esi;
    LIFT_CALL(edx, 0x431561u);
L_431561:
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0xcu);
    ecx = esi;
    wr32(m, esi + 0x14u, ebx);
    wr32(m, esi + 0x18u, ebx);
    wr32(m, esi + 0x2370u, ebx);
    wr32(m, esi + 0x2e74u, ebx);
    wr32(m, esi + 0x2e78u, ebx);
    wr32(m, esi + 0x10u, ebx);
    wr32(m, esi + 0xcu, ebx);
    wr32(m, esi + 0x2e9cu, ebx);
    LIFT_CALL(edx, 0x43158eu);
    eax = rd32(m, esi + 0x2de4u);
    fA_431594 = eax;
    fB_431594 = 0x3u;
    if (fA_431594 == fB_431594) goto L_4316bb;
    fA_43159d = eax;
    fB_43159d = 0x4u;
    if (fA_43159d == fB_43159d) goto L_4316bb;
    edi = 0u;
    fA_4315a8 = rd32(m, esi + 0xb4u);
    fB_4315a8 = ebx;
    if (int32_t(sext(fA_4315a8, 4)) <= int32_t(sext(fB_4315a8, 4))) goto L_4316f3;
L_4315b4:
    ecx = 0u;
    fA_4315b6 = rd32(m, esi + 0xecu);
    fB_4315b6 = ebx;
    if (int32_t(sext(fA_4315b6, 4)) <= int32_t(sext(fB_4315b6, 4))) goto L_4315f3;
    eax = (esi + 0x170u);
    goto L_4315d0;
L_4315d0:
    x0 = ldx(m, eax - 0x80u);
    stx(m, eax, x0);
    x0 = ldx(m, eax + 0x5c0u);
    stx(m, eax + 0x640u, x0);
    ecx = (ecx + 1u);
    eax = eax + 0xa0u;
    fA_4315eb = ecx;
    fB_4315eb = rd32(m, esi + 0xecu);
    if (int32_t(sext(fA_4315eb, 4)) < int32_t(sext(fB_4315eb, 4))) goto L_4315d0;
L_4315f3:
    eax = rd32(m, esi);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0xbcu)));
    edx = rd32(m, eax + 0x18u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = esi;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(edx, 0x431606u);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0xbcu)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x431d60u, 0x431615u);  // RavenSimulation_IntegrateStep
    fA_431615 = rd32(m, esi + 0x14u);
    fB_431615 = ebx;
    if (fA_431615 != fB_431615) goto L_431648;
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x10u);
    ecx = esi;
    LIFT_CALL(edx, 0x431623u);
    fR_431623 = (eax & eax);
    if (!(fR_431623 == 0)) goto L_431648;
    goto L_43164d;
L_431629:
    fA_431629 = rd32(m, esi + 0x2e7cu);
    fB_431629 = ebx;
    if (fA_431629 != fB_431629) goto L_431561;
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x8u)));
    esp -= 4; wr32(m, esp, ecx);
    edi = esi;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x432680u, 0x431643u);  // FUN_00432680
    goto L_431561;
L_431648:
    eax = 0x1u;
L_43164d:
    wr32(m, esi + 0x14u, eax);
    fA_431650 = rd32(m, esi + 0x18u);
    fB_431650 = ebx;
    if (fA_431650 != fB_431650) goto L_431662;
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x14u);
    ecx = esi;
    LIFT_CALL(edx, 0x43165eu);
    fR_43165e = (eax & eax);
    if ((fR_43165e == 0)) goto L_431667;
L_431662:
    eax = 0x1u;
L_431667:
    wr32(m, esi + 0x18u, eax);
    fA_43166a = rd32(m, esi + 0x2e74u);
    fB_43166a = ebx;
    if (fA_43166a != fB_43166a) goto L_43167f;
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x70u);
    ecx = esi;
    LIFT_CALL(edx, 0x43167bu);
    fR_43167b = (eax & eax);
    if ((fR_43167b == 0)) goto L_431684;
L_43167f:
    eax = 0x1u;
L_431684:
    wr32(m, esi + 0x2e74u, eax);
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x6cu);
    ecx = esi;
    LIFT_CALL(edx, 0x431693u);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0xbcu)));
    wr32(m, esi + 0x2e78u, eax);
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x1cu);
    esp -= 4; wr32(m, esp, ecx);
    ecx = esi;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(edx, 0x4316acu);
    edi = (edi + 1u);
    fA_4316ad = edi;
    fB_4316ad = rd32(m, esi + 0xb4u);
    if (int32_t(sext(fA_4316ad, 4)) < int32_t(sext(fB_4316ad, 4))) goto L_4315b4;
    goto L_4316f3;
L_4316bb:
    ecx = 0u;
    fA_4316bd = rd32(m, esi + 0xecu);
    fB_4316bd = ebx;
    if (int32_t(sext(fA_4316bd, 4)) <= int32_t(sext(fB_4316bd, 4))) goto L_4316f3;
    eax = (esi + 0x170u);
    goto L_4316d0;
L_4316d0:
    x0 = ldx(m, eax - 0x80u);
    stx(m, eax, x0);
    x0 = ldx(m, eax + 0x5c0u);
    stx(m, eax + 0x640u, x0);
    ecx = (ecx + 1u);
    eax = eax + 0xa0u;
    fA_4316eb = ecx;
    fB_4316eb = rd32(m, esi + 0xecu);
    if (int32_t(sext(fA_4316eb, 4)) < int32_t(sext(fB_4316eb, 4))) goto L_4316d0;
L_4316f3:
    fA_4316f3 = rd32(m, esi + 0xecu);
    fB_4316f3 = 0x0u;
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x2 = movss_m(rd32(m, esi + 0xc0u));
    x2 = divss(x2, rd32(m, esi + 0xbcu));
    x1 = x0;
    x1 = subss(x1, x2.l(0));
    x0 = subss(x0, x1.l(0));
    wr32(m, esp + 0x48u, x1.l(0));
    x1 = shufps(x1, x1, 0x0);
    x0 = shufps(x0, x0, 0x0);
    stx(m, esp + 0x60u, x0);
    stx(m, esp + 0x70u, x1);
    if (int32_t(sext(fA_4316f3, 4)) <= int32_t(sext(fB_4316f3, 4))) goto L_431c69;
    x2 = X{};
    stx(m, esp + 0x80u, x2);
    edi = (esi + 0x170u);
    goto L_43175a;
L_431750:
    x1 = ldx(m, esp + 0x70u);
    x0 = ldx(m, esp + 0x60u);
L_43175a:
    x2 = ldx(m, edi - 0x80u);
    x2 = mulps(x2, x1);
    x1 = ldx(m, edi);
    x0 = mulps(x0, x1);
    x2 = addps(x2, x0);
    stx(m, edi + 0x10u, x2);
    x0 = ldx(m, edi + 0x640u);
    stx(m, esp + 0x20u, x0);
    x0 = ldx(m, edi + 0x5c0u);
    stx(m, esp + 0x30u, x0);
    x1 = movss_m(rd32(m, esp + 0x30u));
    x1 = mulss(x1, rd32(m, esp + 0x20u));
    x0 = movss_m(rd32(m, esp + 0x34u));
    x0 = mulss(x0, rd32(m, esp + 0x24u));
    x0 = addss(x0, x1.l(0));
    x1 = movss_m(rd32(m, esp + 0x38u));
    x1 = mulss(x1, rd32(m, esp + 0x28u));
    x0 = addss(x0, x1.l(0));
    x1 = movss_m(rd32(m, esp + 0x2cu));
    x1 = mulss(x1, rd32(m, esp + 0x3cu));
    x0 = addss(x0, x1.l(0));
    LIFT_CALL(0x401000u, 0x4317c7u);  // FUN_00401000
    wr32(m, esp + 0x10u, x0.l(0));
    LIFT_CALL(0x401040u, 0x4317d2u);  // FUN_00401040
    x3 = x0;
    fC_4317d5 = fcmp32(x3.l(0), rd32(m, 0x9ddfc8u));
    wr32(m, esp + 0x14u, x3.l(0));
    if (fc_b(fC_4317d5)) goto L_431825;
    x0 = cvtss2sd(x0, rd32(m, esp + 0x10u));
    x1 = x0;
    x1 = mulsd(x1, x0.lo);
    x0 = cvtsd2ss(x0, x1.lo);
    x1 = movsd_m(rd64(m, 0x977638u) /* 1.0 */);
    x0 = cvtss2sd(x0, x0.l(0));
    x1 = subsd(x1, x0.lo);
    x0 = cvtpd2ps(x1);
    LIFT_CALL(0x401040u, 0x43180fu);  // FUN_00401040
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, x0.l(0));
    LIFT_CALL(0x42c230u, 0x43181au);  // FUN_0042c230
    x3 = movss_m(rd32(m, esp + 0x18u));
    esp = esp + 0x4u;
    goto L_431828;
L_431825:
    x0 = x3;
L_431828:
    fC_431828 = fcmp32(x3.l(0), rd32(m, 0x9ddfc8u));
    x5 = movsd_m(rd64(m, 0x9777b0u) /* 0.5648093223571777 */);
    x1 = cvtss2sd(x1, x0.l(0));
    x2 = x1;
    x2 = mulsd(x2, x1.lo);
    x1 = cvtsd2ss(x1, x2.lo);
    x2 = cvtss2sd(x2, x1.l(0));
    x4 = x2;
    x4 = mulsd(x4, x2.lo);
    x2 = cvtsd2ss(x2, x4.lo);
    x4 = cvtss2sd(x4, x1.l(0));
    x4 = mulsd(x4, rd64(m, 0x9777a8u) /* 0.04992850869894028 */);
    x5 = subsd(x5, x4.lo);
    x4 = cvtpd2ps(x5);
    x5 = cvtss2sd(x5, x4.l(0));
    x4 = cvtss2sd(x4, x2.l(0));
    x5 = mulsd(x5, x4.lo);
    x4 = movsd_m(rd64(m, 0x977638u) /* 1.0 */);
    x6 = cvtss2sd(x6, x1.l(0));
    x6 = mulsd(x6, rd64(m, 0x9777a0u) /* 1.4430354833602905 */);
    x7 = x4;
    x7 = subsd(x7, x6.lo);
    x6 = cvtsd2ss(x6, x7.lo);
    x7 = movsd_m(rd64(m, 0x977798u) /* 0.3993034064769745 */);
    x6 = cvtss2sd(x6, x6.l(0));
    x5 = addsd(x5, x6.lo);
    x6 = x4;
    x6 = divsd(x6, x5.lo);
    x5 = cvtsd2ss(x5, x6.lo);
    x6 = cvtss2sd(x6, x1.l(0));
    x6 = mulsd(x6, rd64(m, 0x977790u) /* 0.0193784236907959 */);
    x7 = subsd(x7, x6.lo);
    x2 = cvtss2sd(x2, x2.l(0));
    x1 = cvtss2sd(x1, x1.l(0));
    x1 = mulsd(x1, rd64(m, 0x977788u) /* 1.2763687372207642 */);
    x6 = cvtpd2ps(x7);
    x6 = cvtss2sd(x6, x6.l(0));
    x6 = mulsd(x6, x2.lo);
    x2 = x4;
    x2 = subsd(x2, x1.lo);
    x1 = cvtsd2ss(x1, x2.lo);
    x1 = cvtss2sd(x1, x1.l(0));
    x0 = cvtss2sd(x0, x0.l(0));
    x6 = addsd(x6, x1.lo);
    x6 = mulsd(x6, x0.lo);
    x0 = cvtsd2ss(x0, x6.lo);
    x5 = cvtss2sd(x5, x5.l(0));
    x0 = cvtss2sd(x0, x0.l(0));
    x5 = mulsd(x5, x0.lo);
    x1 = cvtpd2ps(x5);
    if (fc_b(fC_431828)) goto L_431925;
    x0 = cvtss2sd(x0, x1.l(0));
    x1 = movsd_m(rd64(m, 0x977510u) /* 1.5707963705062866 */);
    x1 = subsd(x1, x0.lo);
    x1 = cvtpd2ps(x1);
L_431925:
    x0 = movss_m(rd32(m, esp + 0x10u));
    fC_43192b = fcmp32(x0.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    if (fc_b(fC_43192b)) goto L_431939;
    x0 = x1;
    goto L_431945;
L_431939:
    x0 = movss_m(rd32(m, 0x9774a8u) /* -0.0 */);
    x0 = subss(x0, x1.l(0));
L_431945:
    x1 = movsd_m(rd64(m, 0x977510u) /* 1.5707963705062866 */);
    x0 = cvtps2pd(x0.lo);
    x1 = subsd(x1, x0.lo);
    x0 = cvtpd2ps(x1);
    wr32(m, esp + 0x4cu, x0.l(0));
    x0 = addss(x0, rd32(m, 0x977508u) /* 1.5707963705062866 */);
    LIFT_CALL(0x401000u, 0x43196bu);  // FUN_00401000
    x1 = x0;
    x1 = mulss(x1, rd32(m, 0x9774a0u) /* 0.15915493667125702 */);
    eax = cvttss2si(x1.l(0));
    x1 = cvtsi2ss(x1, eax);
    x1 = mulss(x1, rd32(m, 0x97749cu) /* 6.2831854820251465 */);
    x0 = subss(x0, x1.l(0));
    x1 = movss_m(rd32(m, 0x977498u) /* 3.1415927410125732 */);
    x1 = subss(x1, x0.l(0));
    x0 = x1;
    x0 = mulss(x0, x1.l(0));
    x2 = x0;
    x2 = mulss(x2, rd32(m, 0x935d50u) /* -2.1972959984850604e-07 */);
    x2 = addss(x2, rd32(m, 0x935d48u) /* 2.4202940039685927e-05 */);
    x1 = x0;
    x1 = mulss(x1, x0.l(0));
    x3 = x0;
    x3 = mulss(x3, rd32(m, 0x935d44u) /* -0.0013858790043741465 */);
    x0 = mulss(x0, rd32(m, 0x935d40u) /* -0.4999941885471344 */);
    x3 = addss(x3, rd32(m, 0x935d4cu) /* 0.04165977984666824 */);
    x0 = addss(x0, rd32(m, 0x935d3cu) /* 0.9999992251396179 */);
    x2 = mulss(x2, x1.l(0));
    x2 = addss(x2, x3.l(0));
    x3 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x2 = mulss(x2, x1.l(0));
    x1 = movss_m(rd32(m, esp + 0x10u));
    fC_4319f4 = fcmp32(x1.l(0), x3.l(0));
    x2 = addss(x2, x0.l(0));
    x0 = x3;
    x0 = divss(x0, x2.l(0));
    wr32(m, esp + 0x18u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x4cu));
    x2 = x0;
    x2 = mulss(x2, rd32(m, esp + 0x48u));
    wr32(m, esp + 0x44u, x2.l(0));
    if (fc_b(fC_4319f4)) goto L_431a27;
    x1 = x3;
    goto L_431acb;
L_431a27:
    x0 = subss(x0, x2.l(0));
    x0 = addss(x0, rd32(m, 0x977508u) /* 1.5707963705062866 */);
    LIFT_CALL(0x401000u, 0x431a38u);  // FUN_00401000
    x1 = x0;
    x1 = mulss(x1, rd32(m, 0x9774a0u) /* 0.15915493667125702 */);
    ecx = cvttss2si(x1.l(0));
    x1 = cvtsi2ss(x1, ecx);
    x1 = mulss(x1, rd32(m, 0x97749cu) /* 6.2831854820251465 */);
    x0 = subss(x0, x1.l(0));
    x1 = movss_m(rd32(m, 0x977498u) /* 3.1415927410125732 */);
    x1 = subss(x1, x0.l(0));
    x0 = x1;
    x0 = mulss(x0, x1.l(0));
    x1 = x0;
    x1 = mulss(x1, rd32(m, 0x935d50u) /* -2.1972959984850604e-07 */);
    x1 = addss(x1, rd32(m, 0x935d48u) /* 2.4202940039685927e-05 */);
    x2 = x0;
    x2 = mulss(x2, x0.l(0));
    x1 = mulss(x1, x2.l(0));
    x3 = x0;
    x3 = mulss(x3, rd32(m, 0x935d44u) /* -0.0013858790043741465 */);
    x3 = addss(x3, rd32(m, 0x935d4cu) /* 0.04165977984666824 */);
    x0 = mulss(x0, rd32(m, 0x935d40u) /* -0.4999941885471344 */);
    x0 = addss(x0, rd32(m, 0x935d3cu) /* 0.9999992251396179 */);
    x1 = addss(x1, x3.l(0));
    x3 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x1 = mulss(x1, x2.l(0));
    x2 = movss_m(rd32(m, esp + 0x44u));
    x1 = addss(x1, x0.l(0));
    x1 = mulss(x1, rd32(m, esp + 0x18u));
L_431acb:
    x0 = movss_m(rd32(m, esp + 0x10u));
    fC_431ad1 = fcmp32(x0.l(0), x3.l(0));
    wr32(m, esp + 0x1cu, x1.l(0));
    if (fc_b(fC_431ad1)) goto L_431ae4;
    x0 = X{};
    goto L_431b7c;
L_431ae4:
    x0 = x2;
    x0 = addss(x0, rd32(m, 0x977508u) /* 1.5707963705062866 */);
    LIFT_CALL(0x401000u, 0x431af4u);  // FUN_00401000
    x1 = x0;
    x1 = mulss(x1, rd32(m, 0x9774a0u) /* 0.15915493667125702 */);
    edx = cvttss2si(x1.l(0));
    x1 = cvtsi2ss(x1, edx);
    x1 = mulss(x1, rd32(m, 0x97749cu) /* 6.2831854820251465 */);
    x0 = subss(x0, x1.l(0));
    x1 = movss_m(rd32(m, 0x977498u) /* 3.1415927410125732 */);
    x1 = subss(x1, x0.l(0));
    x1 = mulss(x1, x1.l(0));
    x0 = x1;
    x0 = mulss(x0, rd32(m, 0x935d50u) /* -2.1972959984850604e-07 */);
    x0 = addss(x0, rd32(m, 0x935d48u) /* 2.4202940039685927e-05 */);
    x2 = x1;
    x2 = mulss(x2, x1.l(0));
    x3 = x1;
    x3 = mulss(x3, rd32(m, 0x935d44u) /* -0.0013858790043741465 */);
    x3 = addss(x3, rd32(m, 0x935d4cu) /* 0.04165977984666824 */);
    x1 = mulss(x1, rd32(m, 0x935d40u) /* -0.4999941885471344 */);
    x1 = addss(x1, rd32(m, 0x935d3cu) /* 0.9999992251396179 */);
    x0 = mulss(x0, x2.l(0));
    x0 = addss(x0, x3.l(0));
    x0 = mulss(x0, x2.l(0));
    x0 = addss(x0, x1.l(0));
    x0 = mulss(x0, rd32(m, esp + 0x18u));
    x1 = movss_m(rd32(m, esp + 0x1cu));
L_431b7c:
    x2 = movss_m(rd32(m, esp + 0x2cu));
    x3 = movss_m(rd32(m, esp + 0x3cu));
    x2 = mulss(x2, x1.l(0));
    x3 = mulss(x3, x0.l(0));
    x2 = addss(x2, x3.l(0));
    x3 = movss_m(rd32(m, esp + 0x20u));
    wr32(m, esp + 0x5cu, x2.l(0));
    x2 = movss_m(rd32(m, esp + 0x30u));
    x2 = mulss(x2, x0.l(0));
    x3 = mulss(x3, x1.l(0));
    x2 = addss(x2, x3.l(0));
    x3 = movss_m(rd32(m, esp + 0x24u));
    wr32(m, esp + 0x50u, x2.l(0));
    x2 = movss_m(rd32(m, esp + 0x34u));
    x2 = mulss(x2, x0.l(0));
    x3 = mulss(x3, x1.l(0));
    x2 = addss(x2, x3.l(0));
    wr32(m, esp + 0x54u, x2.l(0));
    x2 = movss_m(rd32(m, esp + 0x38u));
    x2 = mulss(x2, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x28u));
    x0 = mulss(x0, x1.l(0));
    x2 = addss(x2, x0.l(0));
    wr32(m, esp + 0x58u, x2.l(0));
    x0 = ldx(m, esp + 0x50u);
    x3 = x0;
    x3 = mulps(x3, x0);
    x1 = x3;
    x1 = shufps(x1, x3, 0xee);
    x2 = x1;
    x2 = addps(x2, x3);
    stx(m, edi + 0x650u, x0);
    x3 = ldx(m, 0x92dc20u);
    x2 = shufps(x2, x2, 0x40);
    x1 = shufps(x1, x2, 0xf0);
    x1 = addps(x1, x2);
    x1 = shufps(x1, x1, 0xaa);
    x2 = sqrtps(x2, x1);
    x0 = divps(x0, x2);
    x2 = cmpps<4>(x2, ldx(m, esp + 0x80u));
    x0 = andps(x0, x2);
    x2 = ldx(m, 0x92dc30u);
    x1 = cmpps<4>(x1, x3);
    x0 = andps(x0, x1);
    x1 = andnps(x1, x2);
    x0 = orps(x0, x1);
    stx(m, edi + 0x650u, x0);
    ebx = (ebx + 1u);
    edi = edi + 0xa0u;
    fA_431c5d = ebx;
    fB_431c5d = rd32(m, esi + 0xecu);
    if (int32_t(sext(fA_431c5d, 4)) < int32_t(sext(fB_431c5d, 4))) goto L_431750;
L_431c69:
    fA_431c69 = rd32(m, esi + 0x4u);
    fB_431c69 = 0x1u;
    if (fA_431c69 != fB_431c69) goto L_431cd4;
    fA_431c6f = rd32(m, esi + 0x2388u);
    fB_431c6f = 0x0u;
    if (fA_431c6f != fB_431c6f) goto L_431cce;
    ecx = (esp + 0x90u);
    LIFT_CALL(0x42e560u, 0x431c84u);  // FUN_0042e560
    eax = ecx;
    esp -= 4; wr32(m, esp, eax);
    ecx = esi;
    LIFT_CALL(0x433120u, 0x431c8eu);  // FUN_00433120
    ebx = 0x13u;
    edi = (esi + 0x2be0u);
L_431ca0:
    ecx = (edi - 0x70u);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x431cf0u, 0x431ca9u);  // FUN_00431cf0
    ebx = (ebx - 1u);
    edi = edi - 0x70u;
    fR_431cad = (ebx & ebx);
    if ((!(fR_431cad == 0) && sgn(fR_431cad, 4) == false)) goto L_431ca0;
    ecx = (esi + 0x2390u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = (esp + 0x94u);
    LIFT_CALL(0x431cf0u, 0x431cc4u);  // FUN_00431cf0
    wr32(m, esi + 0x2388u, 0x1u);
L_431cce:
    wr32(m, esi + 0x2388u, (rd32(m, esi + 0x2388u) - 1u));
L_431cd4:
    ecx = rd32(m, esp + 0x10cu);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x431ce5u);  // __security_check_cookie
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00431d60 RavenSimulation_IntegrateStep: One fixed physics step (ESI = S, stack fixedDt): replay follow, slot 9, force reset
//   85c9d0, 4355d0, 435320, slot 11, mechanism forces / constraints, velocity integration, slots 12 / 15, pose integration,
//   slots 16 / 14 / 13 / 17, node slot 6.
//   original RavenSimulation_IntegrateStep; register inputs: ecx esi; ret 4
REFLEX_PORT_FN(0x00431d60, RavenSimulation_IntegrateStep, "bike", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_431d69 = 0;
    uint32_t fB_431d69 = 0;
    uint32_t fA_431da4 = 0;
    uint32_t fB_431da4 = 0;
    uint32_t fA_431df3 = 0;
    uint32_t fB_431df3 = 0;
    uint32_t fA_431dfc = 0;
    uint32_t fB_431dfc = 0;
    FCmp fC_431e96{};
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0xcu;
    fA_431d69 = rd32(m, esi + 0x2e7cu);
    fB_431d69 = 0x0u;
    esp -= 4; wr32(m, esp, edi);
    if (fA_431d69 == fB_431d69) goto L_431d81;
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x8u)));
    esp -= 4; wr32(m, esp, ecx);
    edi = esi;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x4343e0u, 0x431d81u);  // ReplayFollow_Step
L_431d81:
    eax = rd32(m, esi);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x8u)));
    edx = rd32(m, eax + 0x24u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = esi;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(edx, 0x431d91u);
    edi = (esi + 0xd0u);
    edx = edi;
    LIFT_CALL(0x85c9d0u, 0x431d9eu);  // FUN_0085c9d0
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x4355d0u, 0x431da4u);  // FUN_004355d0
    fA_431da4 = rd32(m, esi + 0x4u);
    fB_431da4 = 0x0u;
    if (fA_431da4 != fB_431da4) goto L_431db1;
    ecx = esi;
    LIFT_CALL(0x435320u, 0x431db1u);  // FUN_00435320
L_431db1:
    eax = rd32(m, esi);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x8u)));
    edx = rd32(m, eax + 0x2cu);
    esp -= 4; wr32(m, esp, ecx);
    ecx = esi;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(edx, 0x431dc1u);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x85c6d0u, 0x431dc7u);  // FUN_0085c6d0
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x85cb70u, 0x431dcdu);  // FUN_0085cb70
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x8u)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x85ca40u, 0x431dd9u);  // FUN_0085ca40
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x85c700u, 0x431ddfu);  // FUN_0085c700
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x85cf80u, 0x431de5u);  // FUN_0085cf80
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x8u)));
    esp -= 4; wr32(m, esp, ecx);
    edx = edi;
    wr32(m, esp, fst32(m));
    LIFT_CALL(0x85d270u, 0x431df3u);  // Mechanism_IntegrateVelocities
    fA_431df3 = rd32(m, esi + 0x2de0u);
    fB_431df3 = 0x0u;
    if (fA_431df3 == fB_431df3) goto L_431e05;
    fA_431dfc = rd32(m, esi + 0x2de4u);
    fB_431dfc = 0x1u;
    if (fA_431dfc != fB_431dfc) goto L_431e15;
L_431e05:
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x30u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = esi;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(edx, 0x431e12u);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x8u)));
L_431e15:
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x3cu);
    esp -= 4; wr32(m, esp, ecx);
    ecx = esi;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(edx, 0x431e22u);
    x6 = movss_m(rd32(m, ebp + 0x8u));
    ecx = edi;
    LIFT_CALL(0x85d100u, 0x431e2eu);  // Mechanism_IntegratePoses
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x40u);
    ecx = esi;
    LIFT_CALL(edx, 0x431e37u);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x85d300u, 0x431e3du);  // FUN_0085d300
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x38u);
    ecx = esi;
    LIFT_CALL(edx, 0x431e46u);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x8u)));
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x34u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = esi;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(edx, 0x431e56u);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x8u)));
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x44u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = esi;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(edx, 0x431e66u);
    x1 = ldx(m, esi + 0x100u);
    x2 = movss_m(rd32(m, esi + 0xbcu));
    x1 = mulps(x1, x1);
    x0 = x1;
    x0 = shufps(x0, x1, 0x66);
    x1 = addss(x1, x0.l(0));
    x0 = shufps(x0, x0, 0x55);
    x1 = addss(x1, x0.l(0));
    x0 = x1;
    x1 = movss_m(rd32(m, esi + 0x98u));
    fC_431e96 = fcmp32(x1.l(0), x2.l(0));
    x0 = shufps(x0, x0, 0x0);
    x0 = sqrtps(x0, x0);
    if (!(fc_b(fC_431e96) || fc_z(fC_431e96))) goto L_431ea5;
    x2 = x1;
L_431ea5:
    x3 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x3 = divss(x3, x1.l(0));
    x1 = movss_m(rd32(m, esi + 0x94u));
    x2 = mulss(x2, x3.l(0));
    x0 = subss(x0, x1.l(0));
    x0 = mulss(x0, x2.l(0));
    x0 = addss(x0, x1.l(0));
    wr32(m, esi + 0x9cu, x2.l(0));
    wr32(m, esi + 0x94u, x0.l(0));
    edi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00431ee0 BikeWheel_ContactPositionCorrection: Per contacting wheel (ECX = W, EDX = S, stack out vec4): penetration W+50 -
//   W+60 projected onto the normal W+40 (fully / partly by its Y), scaled by [9cd970], gated against the up axis.
//   original FUN_00431ee0; register inputs: ecx edx; ret 4
REFLEX_PORT_FN(0x00431ee0, BikeWheel_ContactPositionCorrection, "bike", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_43200f = 0;
    uint32_t fB_43200f = 0;
    uint32_t fA_432032 = 0;
    uint32_t fB_432032 = 0;
    uint32_t fA_432113 = 0;
    uint32_t fB_432113 = 0;
    uint32_t fA_432123 = 0;
    uint32_t fB_432123 = 0;
    uint32_t fA_432146 = 0;
    uint32_t fB_432146 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x18u;
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    x1 = ldx(m, edx + 0x730u);
    x2 = ldx(m, 0xd616a0u);
    x0 = ldx(m, 0x92db80u);
    x0 = andps(x0, x2);
    x2 = ldx(m, 0x93a220u);
    x7 = ldx(m, 0x93a200u);
    x3 = x0;
    x3 = shufps(x3, x0, 0x0);
    eax = rd32(m, ebp + 0x8u);
    x5 = x1;
    x5 = mulps(x5, x2);
    x4 = x0;
    x4 = shufps(x4, x0, 0x55);
    x2 = x5;
    x2 = shufps(x2, x5, 0x1b);
    x3 = mulps(x3, x2);
    x3 = mulps(x3, ldx(m, 0x93a1f0u));
    x2 = shufps(x2, x2, 0xb1);
    x4 = mulps(x4, x2);
    x4 = mulps(x4, x7);
    x6 = x0;
    x6 = shufps(x6, x0, 0xaa);
    x2 = shufps(x2, x2, 0x1b);
    x2 = mulps(x2, x6);
    x6 = ldx(m, 0x93a210u);
    x2 = mulps(x2, x6);
    x2 = addps(x2, x4);
    x4 = x0;
    x4 = shufps(x4, x0, 0xff);
    x4 = mulps(x4, x5);
    x4 = addps(x4, x3);
    x2 = addps(x2, x4);
    x3 = x1;
    x3 = shufps(x3, x1, 0x0);
    x0 = x2;
    x0 = shufps(x0, x2, 0x1b);
    x3 = mulps(x3, x0);
    x3 = mulps(x3, ldx(m, 0x93a1f0u));
    x0 = shufps(x0, x0, 0xb1);
    x4 = x1;
    x4 = shufps(x4, x1, 0x55);
    x4 = mulps(x4, x0);
    x5 = x1;
    x5 = shufps(x5, x1, 0xaa);
    x4 = mulps(x4, x7);
    x0 = shufps(x0, x0, 0x1b);
    x0 = mulps(x0, x5);
    x0 = mulps(x0, x6);
    x0 = addps(x0, x4);
    x4 = x1;
    x4 = shufps(x4, x1, 0xff);
    x1 = movss_m(rd32(m, 0x92e12cu) /* 0.0 */);
    x4 = mulps(x4, x2);
    x1 = shufps(x1, x1, 0x0);
    stx(m, eax, x1);
    x2 = ldx(m, ecx + 0x60u);
    x1 = ldx(m, ecx + 0x50u);
    x4 = addps(x4, x3);
    x3 = movss_m(rd32(m, 0x931da8u) /* 0.6000000238418579 */);
    x1 = subps(x1, x2);
    x2 = ldx(m, ecx + 0x40u);
    x2 = shufps(x2, x2, 0x55);
    x3 = shufps(x3, x3, 0x0);
    x5 = x2;
    x5 = cmpps<1>(x5, x3);
    stx(m, esp + 0x10u, x5);
    esi = rd32(m, esp + 0x14u);
    edi = rd32(m, esp + 0x18u);
    edi = edi + esi;
    edi = edi + rd32(m, esp + 0x10u);
    x0 = addps(x0, x4);
    x4 = movss_m(rd32(m, 0x935860u) /* 0.4000000059604645 */);
    x4 = shufps(x4, x4, 0x0);
    fA_43200f = edi;
    fB_43200f = 0xfffffffdu;
    if (fA_43200f != fB_43200f) goto L_43209c;
    x5 = x2;
    x5 = cmpps<1>(x5, x4);
    stx(m, esp + 0x10u, x5);
    esi = rd32(m, esp + 0x14u);
    edi = rd32(m, esp + 0x18u);
    edi = edi + esi;
    edi = edi + rd32(m, esp + 0x10u);
    fA_432032 = edi;
    fB_432032 = 0xfffffffdu;
    if (fA_432032 != fB_432032) goto L_432060;
    x3 = ldx(m, ecx + 0x40u);
    x2 = x3;
    x2 = mulps(x2, x1);
    x1 = x2;
    x1 = shufps(x1, x2, 0x99);
    x2 = addss(x2, x1.l(0));
    x1 = shufps(x1, x1, 0x55);
    x2 = addss(x2, x1.l(0));
    x1 = x2;
    x1 = shufps(x1, x2, 0x0);
    x1 = mulps(x1, x3);
    goto L_43209c;
L_432060:
    x7 = ldx(m, ecx + 0x40u);
    x5 = x7;
    x5 = mulps(x5, x1);
    x6 = x5;
    x6 = shufps(x6, x5, 0x99);
    x5 = addss(x5, x6.l(0));
    x6 = shufps(x6, x6, 0x55);
    x5 = addss(x5, x6.l(0));
    x6 = x5;
    x6 = shufps(x6, x5, 0x0);
    x6 = mulps(x6, x7);
    x6 = subps(x6, x1);
    x2 = subps(x2, x3);
    x4 = subps(x4, x3);
    x2 = divps(x2, x4);
    x6 = mulps(x6, x2);
    x6 = addps(x6, x1);
    x1 = x6;
L_43209c:
    x2 = movss_m(rd32(m, 0x9cd970u));
    x2 = shufps(x2, x2, 0x0);
    x2 = mulps(x2, x1);
    x3 = x2;
    x1 = x3;
    x1 = mulps(x1, x0);
    x2 = x1;
    x2 = shufps(x2, x1, 0x99);
    x1 = addss(x1, x2.l(0));
    x2 = shufps(x2, x2, 0x55);
    x1 = addss(x1, x2.l(0));
    x2 = ldx(m, 0xd616a0u);
    x1 = shufps(x1, x1, 0x0);
    x4 = x1;
    x1 = x3;
    x1 = mulps(x1, x2);
    x2 = x1;
    x2 = shufps(x2, x1, 0x99);
    x1 = addss(x1, x2.l(0));
    x2 = shufps(x2, x2, 0x55);
    x1 = addss(x1, x2.l(0));
    x2 = x1;
    x2 = shufps(x2, x1, 0x0);
    x1 = ldx(m, 0xd61670u);
    x2 = cmpps<1>(x2, x1);
    stx(m, esp + 0x10u, x2);
    esi = rd32(m, esp + 0x14u);
    edi = rd32(m, esp + 0x18u);
    edi = edi + esi;
    edi = edi + rd32(m, esp + 0x10u);
    fA_432113 = edi;
    fB_432113 = 0xfffffffdu;
    if (fA_432113 != fB_432113) goto L_432123;
    x0 = X{};
    x0 = subps(x0, x3);
    stx(m, eax, x0);
    goto L_432159;
L_432123:
    fA_432123 = rd32(m, ecx + 0x108u);
    fB_432123 = 0x0u;
    if (fA_432123 != fB_432123) goto L_432156;
    x2 = x4;
    x2 = cmpps<1>(x2, x1);
    stx(m, esp + 0x10u, x2);
    esi = rd32(m, esp + 0x14u);
    edi = rd32(m, esp + 0x18u);
    edi = edi + esi;
    edi = edi + rd32(m, esp + 0x10u);
    fA_432146 = edi;
    fB_432146 = 0xfffffffdu;
    if (fA_432146 == fB_432146) goto L_432156;
    x4 = mulps(x4, x0);
    x3 = subps(x3, x4);
    stx(m, eax, x3);
    goto L_432160;
L_432156:
    stx(m, eax, x3);
L_432159:
    x4 = ldx(m, 0xd61670u);
L_432160:
    ecx = rd32(m, ecx);
    ecx = (ecx + ecx * 4);
    ecx = (ecx << 5);
    x0 = ldx(m, ecx + edx + 0xf0u);
    ecx = (ecx + edx + 0xf0u);
    edi = rd32(m, esp); esp += 4;
    x0 = addps(x0, x4);
    stx(m, ecx, x0);
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 004321a0 RavenSimulation_vf28: Slot 28 (ECX = S): 1 on any driving activity, rider off or a large external impulse.
//   original RavenSimulation::vf28; register inputs: ecx; ret 0
REFLEX_PORT_FN(0x004321a0, RavenSimulation_vf28, "bike", LIFT_OUTS) {
    LIFT_ENTRY();
    FCmp fC_4321b1{};
    uint32_t fR_4321b7 = 0;
    FCmp fC_4321c1{};
    uint32_t fR_4321c5 = 0;
    FCmp fC_4321cf{};
    uint32_t fR_4321d3 = 0;
    FCmp fC_4321dd{};
    uint32_t fR_4321e1 = 0;
    FCmp fC_4321f4{};
    uint32_t fA_4321f9 = 0;
    uint32_t fB_4321f9 = 0;
    FCmp fC_432204{};
    FCmp fC_43220e{};
    FCmp fC_432218{};
    FCmp fC_432222{};
    FCmp fC_43222c{};
    uint32_t fA_432231 = 0;
    uint32_t fB_432231 = 0;
    uint32_t fA_43228d = 0;
    uint32_t fB_43228d = 0;
    uint32_t fA_4322e3 = 0;
    uint32_t fB_4322e3 = 0;
    uint32_t fA_432326 = 0;
    uint32_t fB_432326 = 0;
    uint32_t fR_432334 = 0;
    uint32_t fR_432338 = 0;
    uint32_t fR_43233c = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    x1 = movss_m(rd32(m, ecx + 0x2cu));
    x0 = X{};
    esp = esp - 0x18u;
    fC_4321b1 = fcmp32(x1.l(0), x0.l(0));
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_4321b1) | 2u) << 8);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    fR_4321b7 = (((eax >> 8) & 0xffu) & 0x44u);
    if (parity8(fR_4321b7)) goto L_4321ea;
    x1 = movss_m(rd32(m, ecx + 0x30u));
    fC_4321c1 = fcmp32(x1.l(0), x0.l(0));
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_4321c1) | 2u) << 8);
    fR_4321c5 = (((eax >> 8) & 0xffu) & 0x44u);
    if (parity8(fR_4321c5)) goto L_4321ea;
    x1 = movss_m(rd32(m, ecx + 0x34u));
    fC_4321cf = fcmp32(x1.l(0), x0.l(0));
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_4321cf) | 2u) << 8);
    fR_4321d3 = (((eax >> 8) & 0xffu) & 0x44u);
    if (parity8(fR_4321d3)) goto L_4321ea;
    x1 = movss_m(rd32(m, ecx + 0x38u));
    fC_4321dd = fcmp32(x1.l(0), x0.l(0));
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_4321dd) | 2u) << 8);
    fR_4321e1 = (((eax >> 8) & 0xffu) & 0x44u);
    if (parity8(fR_4321e1)) goto L_4321ea;
    esi = 0u;
    goto L_4321ef;
L_4321ea:
    esi = 0x1u;
L_4321ef:
    x1 = movss_m(rd32(m, ecx + 0x1cu));
    fC_4321f4 = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_4321f4) || fc_z(fC_4321f4))) goto L_432213;
    fA_4321f9 = rd32(m, ecx + 0x48u);
    fB_4321f9 = 0x0u;
    if (fA_4321f9 != fB_4321f9) goto L_432213;
    x1 = movss_m(rd32(m, ecx + 0x24u));
    fC_432204 = fcmp32(x1.l(0), x0.l(0));
    if (!(fc_b(fC_432204) || fc_z(fC_432204))) goto L_432213;
    x1 = movss_m(rd32(m, ecx + 0x28u));
    fC_43220e = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_43220e) || fc_z(fC_43220e))) goto L_43223e;
L_432213:
    x1 = movss_m(rd32(m, ecx + 0x20u));
    fC_432218 = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_432218) || fc_z(fC_432218))) goto L_432231;
    x1 = movss_m(rd32(m, ecx + 0x24u));
    fC_432222 = fcmp32(x1.l(0), x0.l(0));
    if (!(fc_b(fC_432222) || fc_z(fC_432222))) goto L_432231;
    x1 = movss_m(rd32(m, ecx + 0x28u));
    fC_43222c = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_43222c) || fc_z(fC_43222c))) goto L_43223e;
L_432231:
    fA_432231 = rd32(m, ecx + 0x2374u);
    fB_432231 = 0x0u;
    if (fA_432231 != fB_432231) goto L_43223e;
    edx = 0u;
    goto L_432243;
L_43223e:
    edx = 0x1u;
L_432243:
    x1 = ldx(m, ecx + 0x2ec0u);
    x1 = mulps(x1, x1);
    x0 = x1;
    x0 = shufps(x0, x1, 0x66);
    x1 = addss(x1, x0.l(0));
    x0 = shufps(x0, x0, 0x55);
    x1 = addss(x1, x0.l(0));
    x0 = x1;
    x0 = shufps(x0, x0, 0x0);
    x1 = sqrtps(x1, x0);
    x0 = movss_m(rd32(m, 0x92fe48u) /* 3.0 */);
    x0 = shufps(x0, x0, 0x0);
    x0 = cmpps<1>(x0, x1);
    stx(m, esp + 0x10u, x0);
    eax = rd32(m, esp + 0x14u);
    edi = rd32(m, esp + 0x18u);
    edi = edi + eax;
    edi = edi + rd32(m, esp + 0x10u);
    fA_43228d = edi;
    fB_43228d = 0xfffffffdu;
    if (fA_43228d == fB_43228d) goto L_43232f;
    x1 = ldx(m, ecx + 0x2ed0u);
    x1 = mulps(x1, x1);
    x0 = x1;
    x0 = shufps(x0, x1, 0x66);
    x1 = addss(x1, x0.l(0));
    x0 = shufps(x0, x0, 0x55);
    x1 = addss(x1, x0.l(0));
    x0 = x1;
    x1 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x1 = shufps(x1, x1, 0x0);
    x0 = shufps(x0, x0, 0x0);
    x0 = sqrtps(x0, x0);
    x2 = x1;
    x2 = cmpps<1>(x2, x0);
    stx(m, esp + 0x10u, x2);
    eax = rd32(m, esp + 0x14u);
    edi = rd32(m, esp + 0x18u);
    edi = edi + eax;
    edi = edi + rd32(m, esp + 0x10u);
    fA_4322e3 = edi;
    fB_4322e3 = 0xfffffffdu;
    if (fA_4322e3 == fB_4322e3) goto L_43232f;
    x2 = ldx(m, ecx + 0x2ee0u);
    x2 = mulps(x2, x2);
    x0 = x2;
    x0 = shufps(x0, x2, 0x66);
    x2 = addss(x2, x0.l(0));
    x0 = shufps(x0, x0, 0x55);
    x2 = addss(x2, x0.l(0));
    x0 = x2;
    x0 = shufps(x0, x0, 0x0);
    x0 = sqrtps(x0, x0);
    x1 = cmpps<1>(x1, x0);
    stx(m, esp + 0x10u, x1);
    ecx = rd32(m, esp + 0x14u);
    eax = rd32(m, esp + 0x18u);
    eax = eax + ecx;
    eax = eax + rd32(m, esp + 0x10u);
    fA_432326 = eax;
    fB_432326 = 0xfffffffdu;
    if (fA_432326 == fB_432326) goto L_43232f;
    eax = 0u;
    goto L_432334;
L_43232f:
    eax = 0x1u;
L_432334:
    fR_432334 = (esi & esi);
    if (!(fR_432334 == 0)) goto L_432346;
    fR_432338 = (edx & edx);
    if (!(fR_432338 == 0)) goto L_432346;
    fR_43233c = (eax & eax);
    if (!(fR_43233c == 0)) goto L_432346;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_432346:
    edi = rd32(m, esp); esp += 4;
    eax = 0x1u;
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00432360 RavenSimulation_vf26: Slot 26, leave-rest test (ECX = S).
//   original RavenSimulation::vf26; register inputs: ecx; ret 0
REFLEX_PORT_FN(0x00432360, RavenSimulation_vf26, "bike", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4323bd = 0;
    uint32_t fB_4323bd = 0;
    uint32_t fA_432413 = 0;
    uint32_t fB_432413 = 0;
    uint32_t fA_432456 = 0;
    uint32_t fB_432456 = 0;
    FCmp fC_43246c{};
    uint32_t fA_432471 = 0;
    uint32_t fB_432471 = 0;
    FCmp fC_43247c{};
    FCmp fC_432486{};
    FCmp fC_432490{};
    FCmp fC_43249a{};
    FCmp fC_4324a4{};
    uint32_t fR_4324a9 = 0;
    uint32_t fA_4324ad = 0;
    uint32_t fB_4324ad = 0;
    uint32_t fR_4324b6 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x1cu;
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x78u);
    LIFT_CALL(edx, 0x432373u);
    x1 = ldx(m, esi + 0x2ec0u);
    x1 = mulps(x1, x1);
    x0 = x1;
    x0 = shufps(x0, x1, 0x66);
    x1 = addss(x1, x0.l(0));
    x0 = shufps(x0, x0, 0x55);
    x1 = addss(x1, x0.l(0));
    x0 = x1;
    x0 = shufps(x0, x0, 0x0);
    x1 = sqrtps(x1, x0);
    x0 = movss_m(rd32(m, 0x92fe48u) /* 3.0 */);
    x0 = shufps(x0, x0, 0x0);
    x0 = cmpps<1>(x0, x1);
    stx(m, esp + 0x10u, x0);
    ecx = rd32(m, esp + 0x14u);
    edx = rd32(m, esp + 0x18u);
    edx = edx + ecx;
    edx = edx + rd32(m, esp + 0x10u);
    fA_4323bd = edx;
    fB_4323bd = 0xfffffffdu;
    if (fA_4323bd == fB_4323bd) goto L_43245f;
    x1 = ldx(m, esi + 0x2ed0u);
    x1 = mulps(x1, x1);
    x0 = x1;
    x0 = shufps(x0, x1, 0x66);
    x1 = addss(x1, x0.l(0));
    x0 = shufps(x0, x0, 0x55);
    x1 = addss(x1, x0.l(0));
    x0 = x1;
    x1 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x1 = shufps(x1, x1, 0x0);
    x0 = shufps(x0, x0, 0x0);
    x0 = sqrtps(x0, x0);
    x2 = x1;
    x2 = cmpps<1>(x2, x0);
    stx(m, esp + 0x10u, x2);
    ecx = rd32(m, esp + 0x14u);
    edx = rd32(m, esp + 0x18u);
    edx = edx + ecx;
    edx = edx + rd32(m, esp + 0x10u);
    fA_432413 = edx;
    fB_432413 = 0xfffffffdu;
    if (fA_432413 == fB_432413) goto L_43245f;
    x2 = ldx(m, esi + 0x2ee0u);
    x2 = mulps(x2, x2);
    x0 = x2;
    x0 = shufps(x0, x2, 0x66);
    x2 = addss(x2, x0.l(0));
    x0 = shufps(x0, x0, 0x55);
    x2 = addss(x2, x0.l(0));
    x0 = x2;
    x0 = shufps(x0, x0, 0x0);
    x0 = sqrtps(x0, x0);
    x1 = cmpps<1>(x1, x0);
    stx(m, esp + 0x10u, x1);
    ecx = rd32(m, esp + 0x14u);
    edx = rd32(m, esp + 0x18u);
    edx = edx + ecx;
    edx = edx + rd32(m, esp + 0x10u);
    fA_432456 = edx;
    fB_432456 = 0xfffffffdu;
    if (fA_432456 == fB_432456) goto L_43245f;
    ecx = 0u;
    goto L_432464;
L_43245f:
    ecx = 0x1u;
L_432464:
    x1 = movss_m(rd32(m, esi + 0x1cu));
    x0 = X{};
    fC_43246c = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_43246c) || fc_z(fC_43246c))) goto L_43248b;
    fA_432471 = rd32(m, esi + 0x48u);
    fB_432471 = 0x0u;
    if (fA_432471 != fB_432471) goto L_43248b;
    x1 = movss_m(rd32(m, esi + 0x24u));
    fC_43247c = fcmp32(x1.l(0), x0.l(0));
    if (!(fc_b(fC_43247c) || fc_z(fC_43247c))) goto L_43248b;
    x1 = movss_m(rd32(m, esi + 0x28u));
    fC_432486 = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_432486) || fc_z(fC_432486))) goto L_4324c1;
L_43248b:
    x1 = movss_m(rd32(m, esi + 0x20u));
    fC_432490 = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_432490) || fc_z(fC_432490))) goto L_4324a9;
    x1 = movss_m(rd32(m, esi + 0x24u));
    fC_43249a = fcmp32(x1.l(0), x0.l(0));
    if (!(fc_b(fC_43249a) || fc_z(fC_43249a))) goto L_4324a9;
    x1 = movss_m(rd32(m, esi + 0x28u));
    fC_4324a4 = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_4324a4) || fc_z(fC_4324a4))) goto L_4324c1;
L_4324a9:
    fR_4324a9 = (eax & eax);
    if ((fR_4324a9 == 0)) goto L_4324c1;
    fA_4324ad = rd32(m, esi + 0x2374u);
    fB_4324ad = 0x0u;
    if (fA_4324ad != fB_4324ad) goto L_4324c1;
    fR_4324b6 = (ecx & ecx);
    if (!(fR_4324b6 == 0)) goto L_4324c1;
    eax = 0u;
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_4324c1:
    eax = 0x1u;
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 004324d0 RavenSimulation_vf25: Slot 25, enter-rest test (ECX = S).
//   original RavenSimulation::vf25; register inputs: ecx; ret 0
REFLEX_PORT_FN(0x004324d0, RavenSimulation_vf25, "bike", LIFT_OUTS) {
    LIFT_ENTRY();
    FCmp fC_432551{};
    uint32_t fA_432556 = 0;
    uint32_t fB_432556 = 0;
    FCmp fC_432560{};
    FCmp fC_43256a{};
    FCmp fC_432578{};
    FCmp fC_432582{};
    FCmp fC_43258c{};
    uint32_t fR_432595 = 0;
    uint32_t fA_43259d = 0;
    uint32_t fB_43259d = 0;
    FCmp fC_4325b1{};
    FCmp fC_4325bd{};
    FCmp fC_4325c7{};
    uint32_t fA_4325cc = 0;
    uint32_t fB_4325cc = 0;
    uint32_t fA_4325d4 = 0;
    uint32_t fB_4325d4 = 0;
    uint32_t fR_4325e5 = 0;
    FCmp fC_4325f1{};
    uint32_t fA_4325f8 = 0;
    uint32_t fB_4325f8 = 0;
    uint32_t fR_432609 = 0;
    uint32_t fR_432624 = 0;
    uint32_t fA_43265e = 0;
    uint32_t fB_43265e = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x44u;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp + 0x40u, eax);
    x0 = movss_m(rd32(m, 0x977770u) /* 0.9962000250816345 */);
    esp -= 4; wr32(m, esp, ebx);
    x0 = shufps(x0, x0, 0x0);
    stx(m, esp + 0x18u, x0);
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    x1 = ldx(m, esi + 0x100u);
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x74u);
    x1 = mulps(x1, x1);
    x0 = x1;
    x0 = shufps(x0, x1, 0x66);
    x1 = addss(x1, x0.l(0));
    x0 = shufps(x0, x0, 0x55);
    x1 = addss(x1, x0.l(0));
    x0 = x1;
    x0 = shufps(x0, x0, 0x0);
    x0 = sqrtps(x0, x0);
    wr32(m, esp + 0x18u, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x94u));
    esp -= 4; wr32(m, esp, edi);
    ebx = 0u;
    wr32(m, esp + 0x18u, x0.l(0));
    LIFT_CALL(edx, 0x43253eu);
    edi = eax;
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x78u);
    ecx = esi;
    LIFT_CALL(edx, 0x432549u);
    x1 = movss_m(rd32(m, esi + 0x1cu));
    x0 = X{};
    fC_432551 = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_432551) || fc_z(fC_432551))) goto L_432573;
    fA_432556 = rd32(m, esi + 0x48u);
    fB_432556 = ebx;
    if (fA_432556 != fB_432556) goto L_432573;
    x1 = movss_m(rd32(m, esi + 0x24u));
    fC_432560 = fcmp32(x1.l(0), x0.l(0));
    if (!(fc_b(fC_432560) || fc_z(fC_432560))) goto L_432573;
    x1 = movss_m(rd32(m, esi + 0x28u));
    fC_43256a = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_43256a) || fc_z(fC_43256a))) goto L_432663;
L_432573:
    x1 = movss_m(rd32(m, esi + 0x20u));
    fC_432578 = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_432578) || fc_z(fC_432578))) goto L_432595;
    x1 = movss_m(rd32(m, esi + 0x24u));
    fC_432582 = fcmp32(x1.l(0), x0.l(0));
    if (!(fc_b(fC_432582) || fc_z(fC_432582))) goto L_432595;
    x1 = movss_m(rd32(m, esi + 0x28u));
    fC_43258c = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_43258c) || fc_z(fC_43258c))) goto L_432663;
L_432595:
    fR_432595 = (eax & eax);
    if ((fR_432595 == 0)) goto L_432663;
    fA_43259d = rd32(m, esi + 0x2374u);
    fB_43259d = ebx;
    if (fA_43259d != fB_43259d) goto L_432663;
    x1 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    fC_4325b1 = fcmp32(x1.l(0), rd32(m, esp + 0x1cu));
    if ((fc_b(fC_4325b1) || fc_z(fC_4325b1))) goto L_4325e3;
    x1 = movss_m(rd32(m, esi + 0x24u));
    fC_4325bd = fcmp32(x1.l(0), x0.l(0));
    if (!(fc_b(fC_4325bd) || fc_z(fC_4325bd))) goto L_4325cc;
    x1 = movss_m(rd32(m, esi + 0x28u));
    fC_4325c7 = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_4325c7) || fc_z(fC_4325c7))) goto L_4325e3;
L_4325cc:
    fA_4325cc = rd32(m, esi + 0xa8u);
    fB_4325cc = ebx;
    if (fA_4325cc == fB_4325cc) goto L_4325e3;
    fA_4325d4 = rd32(m, esi + 0x2e7cu);
    fB_4325d4 = ebx;
    if (fA_4325d4 != fB_4325d4) goto L_4325e3;
    ecx = 0x1u;
    goto L_4325e5;
L_4325e3:
    ecx = 0u;
L_4325e5:
    fR_4325e5 = (edi & edi);
    if ((fR_4325e5 == 0)) goto L_432607;
    x0 = movss_m(rd32(m, 0x9322ccu) /* 0.5 */);
    fC_4325f1 = fcmp32(x0.l(0), rd32(m, esp + 0x18u));
    if ((fc_b(fC_4325f1) || fc_z(fC_4325f1))) goto L_432607;
    fA_4325f8 = rd32(m, esi + 0x2e7cu);
    fB_4325f8 = ebx;
    if (fA_4325f8 != fB_4325f8) goto L_432607;
    eax = 0x1u;
    goto L_432609;
L_432607:
    eax = 0u;
L_432609:
    fR_432609 = (ecx & ecx);
    if ((fR_432609 == 0)) goto L_432624;
    eax = 0x1u;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp + 0x40u);
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x432620u);  // __security_check_cookie
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_432624:
    fR_432624 = (eax & eax);
    if ((fR_432624 == 0)) goto L_432663;
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x7cu);
    ecx = (esp + 0x30u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = esi;
    LIFT_CALL(edx, 0x432636u);
    x0 = ldx(m, eax);
    x1 = ldx(m, esp + 0x20u);
    x0 = shufps(x0, x0, 0x55);
    x1 = cmpps<1>(x1, x0);
    stx(m, esp + 0x20u, x1);
    eax = rd32(m, esp + 0x24u);
    ecx = rd32(m, esp + 0x28u);
    ecx = ecx + eax;
    ecx = ecx + rd32(m, esp + 0x20u);
    eax = 0x1u;
    fA_43265e = ecx;
    fB_43265e = 0xfffffffdu;
    if (fA_43265e == fB_43265e) goto L_432665;
L_432663:
    eax = ebx;
L_432665:
    ecx = rd32(m, esp + 0x4cu);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x432673u);  // __security_check_cookie
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00433120 Simulation_GetReplayState: ECX = S, stack out record: position, quaternion, velocity, angular velocity, time,
//   flags and the 7 input axes at full precision.
//   original FUN_00433120; register inputs: ecx; ret 4
REFLEX_PORT_FN(0x00433120, Simulation_GetReplayState, "bike", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_43319a = 0;
    uint32_t fB_43319a = 0;
    FCmp fC_4331ac{};
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    eax = rd32(m, ebp + 0x8u);
    x0 = ldx(m, ecx + 0x180u);
    stx(m, eax, x0);
    x0 = ldx(m, ecx + 0x7c0u);
    stx(m, eax + 0x10u, x0);
    x0 = ldx(m, ecx + 0x100u);
    stx(m, eax + 0x20u, x0);
    x0 = ldx(m, ecx + 0x740u);
    stx(m, eax + 0x30u, x0);
    x0 = movss_m(rd32(m, 0xa89be8u));
    x0 = addss(x0, rd32(m, 0xa89be4u));
    wr32(m, eax + 0x40u, x0.l(0));
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x1cu)));
    wr32(m, eax + 0x48u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x24u)));
    wr32(m, eax + 0x4cu, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x28u)));
    wr32(m, eax + 0x50u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x2cu)));
    wr32(m, eax + 0x54u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x30u)));
    wr32(m, eax + 0x58u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x34u)));
    wr32(m, eax + 0x5cu, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x38u)));
    wr32(m, eax + 0x44u, 0x0u);
    wr32(m, eax + 0x60u, fst32(m));
    fpop(m);
    fA_43319a = rd32(m, ecx + 0x48u);
    fB_43319a = 0x0u;
    if (fA_43319a == fB_43319a) goto L_4331a7;
    wr32(m, eax + 0x44u, 0x10000u);
L_4331a7:
    x0 = movss_m(rd32(m, ecx + 0x20u));
    fC_4331ac = fcmp32(x0.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    if ((fc_b(fC_4331ac) || fc_z(fC_4331ac))) goto L_4331bc;
    wr32(m, eax + 0x44u, (rd32(m, eax + 0x44u) | 0x20000u));
L_4331bc:
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00433380 RavenSimulation_vf24: Slot 24, set input (ECX = S, stack 64-byte block): copies it to S+1c, zeroes non-finite
//   axes, brake-hold and reverse rules.
//   original RavenSimulation::vf24; register inputs: ecx; ret 4
REFLEX_PORT_FN(0x00433380, RavenSimulation_vf24, "bike", LIFT_OUTS) {
    LIFT_ENTRY();
    FCmp fC_4333a7{};
    uint32_t fR_4333ab = 0;
    uint32_t fR_4333c0 = 0;
    FCmp fC_4333d1{};
    uint32_t fR_4333d5 = 0;
    uint32_t fR_4333ea = 0;
    FCmp fC_4333fb{};
    uint32_t fR_4333ff = 0;
    uint32_t fR_433414 = 0;
    FCmp fC_433425{};
    uint32_t fR_433429 = 0;
    uint32_t fR_43343e = 0;
    FCmp fC_43344f{};
    uint32_t fR_433453 = 0;
    uint32_t fR_433468 = 0;
    FCmp fC_433479{};
    uint32_t fR_43347d = 0;
    uint32_t fR_433492 = 0;
    FCmp fC_4334a3{};
    uint32_t fR_4334a7 = 0;
    uint32_t fR_4334bc = 0;
    FCmp fC_4334cd{};
    uint32_t fR_4334d1 = 0;
    uint32_t fR_4334e6 = 0;
    FCmp fC_4334fa{};
    uint32_t fA_433506 = 0;
    uint32_t fB_433506 = 0;
    FCmp fC_433523{};
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xffffffc0u);
    esp = esp - 0x34u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, ebp + 0x8u);
    ebx = ecx;
    eax = (ebx + 0x1cu);
    esp -= 4; wr32(m, esp, edi);
    edi = eax;
    ecx = 0x10u;
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    x0 = movss_m(rd32(m, eax));
    esi = rd32(m, 0x916240u) /* 8.286522416846233e-39 */;
    fC_4333a7 = fcmp32(x0.l(0), x0.l(0));
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_4333a7) | 2u) << 8);
    fR_4333ab = (((eax >> 8) & 0xffu) & 0x44u);
    if (parity8(fR_4333ab)) goto L_4333c4;
    esp = esp - 0x8u;
    x0 = cvtps2pd(x0.lo);
    wr64(m, esp, x0.lo);
    LIFT_CALL(esi, 0x4333bdu);
    esp = esp + 0x8u;
    fR_4333c0 = (eax & eax);
    if (!(fR_4333c0 == 0)) goto L_4333cc;
L_4333c4:
    x0 = X{};
    wr32(m, ebx + 0x1cu, x0.l(0));
L_4333cc:
    x0 = movss_m(rd32(m, ebx + 0x20u));
    fC_4333d1 = fcmp32(x0.l(0), x0.l(0));
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_4333d1) | 2u) << 8);
    fR_4333d5 = (((eax >> 8) & 0xffu) & 0x44u);
    if (parity8(fR_4333d5)) goto L_4333ee;
    esp = esp - 0x8u;
    x0 = cvtps2pd(x0.lo);
    wr64(m, esp, x0.lo);
    LIFT_CALL(esi, 0x4333e7u);
    esp = esp + 0x8u;
    fR_4333ea = (eax & eax);
    if (!(fR_4333ea == 0)) goto L_4333f6;
L_4333ee:
    x0 = X{};
    wr32(m, ebx + 0x20u, x0.l(0));
L_4333f6:
    x0 = movss_m(rd32(m, ebx + 0x24u));
    fC_4333fb = fcmp32(x0.l(0), x0.l(0));
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_4333fb) | 2u) << 8);
    fR_4333ff = (((eax >> 8) & 0xffu) & 0x44u);
    if (parity8(fR_4333ff)) goto L_433418;
    esp = esp - 0x8u;
    x0 = cvtps2pd(x0.lo);
    wr64(m, esp, x0.lo);
    LIFT_CALL(esi, 0x433411u);
    esp = esp + 0x8u;
    fR_433414 = (eax & eax);
    if (!(fR_433414 == 0)) goto L_433420;
L_433418:
    x0 = X{};
    wr32(m, ebx + 0x24u, x0.l(0));
L_433420:
    x0 = movss_m(rd32(m, ebx + 0x28u));
    fC_433425 = fcmp32(x0.l(0), x0.l(0));
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_433425) | 2u) << 8);
    fR_433429 = (((eax >> 8) & 0xffu) & 0x44u);
    if (parity8(fR_433429)) goto L_433442;
    esp = esp - 0x8u;
    x0 = cvtps2pd(x0.lo);
    wr64(m, esp, x0.lo);
    LIFT_CALL(esi, 0x43343bu);
    esp = esp + 0x8u;
    fR_43343e = (eax & eax);
    if (!(fR_43343e == 0)) goto L_43344a;
L_433442:
    x0 = X{};
    wr32(m, ebx + 0x28u, x0.l(0));
L_43344a:
    x0 = movss_m(rd32(m, ebx + 0x2cu));
    fC_43344f = fcmp32(x0.l(0), x0.l(0));
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_43344f) | 2u) << 8);
    fR_433453 = (((eax >> 8) & 0xffu) & 0x44u);
    if (parity8(fR_433453)) goto L_43346c;
    esp = esp - 0x8u;
    x0 = cvtps2pd(x0.lo);
    wr64(m, esp, x0.lo);
    LIFT_CALL(esi, 0x433465u);
    esp = esp + 0x8u;
    fR_433468 = (eax & eax);
    if (!(fR_433468 == 0)) goto L_433474;
L_43346c:
    x0 = X{};
    wr32(m, ebx + 0x2cu, x0.l(0));
L_433474:
    x0 = movss_m(rd32(m, ebx + 0x30u));
    fC_433479 = fcmp32(x0.l(0), x0.l(0));
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_433479) | 2u) << 8);
    fR_43347d = (((eax >> 8) & 0xffu) & 0x44u);
    if (parity8(fR_43347d)) goto L_433496;
    esp = esp - 0x8u;
    x0 = cvtps2pd(x0.lo);
    wr64(m, esp, x0.lo);
    LIFT_CALL(esi, 0x43348fu);
    esp = esp + 0x8u;
    fR_433492 = (eax & eax);
    if (!(fR_433492 == 0)) goto L_43349e;
L_433496:
    x0 = X{};
    wr32(m, ebx + 0x30u, x0.l(0));
L_43349e:
    x0 = movss_m(rd32(m, ebx + 0x34u));
    fC_4334a3 = fcmp32(x0.l(0), x0.l(0));
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_4334a3) | 2u) << 8);
    fR_4334a7 = (((eax >> 8) & 0xffu) & 0x44u);
    if (parity8(fR_4334a7)) goto L_4334c0;
    esp = esp - 0x8u;
    x0 = cvtps2pd(x0.lo);
    wr64(m, esp, x0.lo);
    LIFT_CALL(esi, 0x4334b9u);
    esp = esp + 0x8u;
    fR_4334bc = (eax & eax);
    if (!(fR_4334bc == 0)) goto L_4334c8;
L_4334c0:
    x0 = X{};
    wr32(m, ebx + 0x34u, x0.l(0));
L_4334c8:
    x0 = movss_m(rd32(m, ebx + 0x38u));
    fC_4334cd = fcmp32(x0.l(0), x0.l(0));
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_4334cd) | 2u) << 8);
    fR_4334d1 = (((eax >> 8) & 0xffu) & 0x44u);
    if (parity8(fR_4334d1)) goto L_4334ea;
    esp = esp - 0x8u;
    x0 = cvtps2pd(x0.lo);
    wr64(m, esp, x0.lo);
    LIFT_CALL(esi, 0x4334e3u);
    esp = esp + 0x8u;
    fR_4334e6 = (eax & eax);
    if (!(fR_4334e6 == 0)) goto L_4334f2;
L_4334ea:
    x0 = X{};
    wr32(m, ebx + 0x38u, x0.l(0));
L_4334f2:
    x0 = movss_m(rd32(m, ebx + 0x1cu));
    x1 = X{};
    fC_4334fa = fcmp32(x0.l(0), x1.l(0));
    if ((fc_b(fC_4334fa) || fc_z(fC_4334fa))) goto L_433506;
    wr32(m, ebx + 0x54u, 0x0u);
L_433506:
    fA_433506 = rd32(m, ebx + 0x54u);
    fB_433506 = 0x0u;
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    if (fA_433506 == fB_433506) goto L_43351e;
    wr32(m, ebx + 0x24u, x0.l(0));
    wr32(m, ebx + 0x28u, x0.l(0));
L_43351e:
    x2 = movss_m(rd32(m, ebx + 0x20u));
    fC_433523 = fcmp32(x2.l(0), x1.l(0));
    if ((fc_b(fC_433523) || fc_z(fC_433523))) goto L_433536;
    wr32(m, ebx + 0x20u, x0.l(0));
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_433536:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    wr32(m, ebx + 0x20u, x1.l(0));
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00433550 ReplayRecord_Encode: ESI = out record (V+0x12c0), stack S: per-frame replay record (half4 + residuals, time,
//   inputs, flags; replay.md).
//   original ReplayRecord_Encode; register inputs: esi; ret 4
REFLEX_PORT_FN(0x00433550, ReplayRecord_Encode, "bike", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_43366d = 0;
    uint32_t fB_43366d = 0;
    uint32_t fA_43367c = 0;
    uint32_t fB_43367c = 0;
    uint32_t fA_4336e0 = 0;
    uint32_t fB_4336e0 = 0;
    uint32_t fA_4336ef = 0;
    uint32_t fB_4336ef = 0;
    uint32_t fA_433753 = 0;
    uint32_t fB_433753 = 0;
    uint32_t fA_433762 = 0;
    uint32_t fB_433762 = 0;
    uint32_t fA_4337c4 = 0;
    uint32_t fB_4337c4 = 0;
    uint32_t fA_4337d3 = 0;
    uint32_t fB_4337d3 = 0;
    uint32_t fA_433854 = 0;
    uint32_t fB_433854 = 0;
    uint32_t fA_433863 = 0;
    uint32_t fB_433863 = 0;
    uint32_t fA_4338cd = 0;
    uint32_t fB_4338cd = 0;
    uint32_t fA_4338dc = 0;
    uint32_t fB_4338dc = 0;
    uint32_t fA_433946 = 0;
    uint32_t fB_433946 = 0;
    uint32_t fA_433955 = 0;
    uint32_t fB_433955 = 0;
    uint32_t fA_4339bd = 0;
    uint32_t fB_4339bd = 0;
    uint32_t fA_4339cc = 0;
    uint32_t fB_4339cc = 0;
    uint32_t fA_433a65 = 0;
    uint32_t fB_433a65 = 0;
    uint32_t fA_433a74 = 0;
    uint32_t fB_433a74 = 0;
    uint32_t fA_433ad8 = 0;
    uint32_t fB_433ad8 = 0;
    uint32_t fA_433ae7 = 0;
    uint32_t fB_433ae7 = 0;
    uint32_t fA_433b4b = 0;
    uint32_t fB_433b4b = 0;
    uint32_t fA_433b5a = 0;
    uint32_t fB_433b5a = 0;
    uint32_t fA_433bbc = 0;
    uint32_t fB_433bbc = 0;
    uint32_t fA_433bcb = 0;
    uint32_t fB_433bcb = 0;
    uint32_t fA_433c4d = 0;
    uint32_t fB_433c4d = 0;
    uint32_t fA_433c5c = 0;
    uint32_t fB_433c5c = 0;
    uint32_t fA_433cc6 = 0;
    uint32_t fB_433cc6 = 0;
    uint32_t fA_433cd5 = 0;
    uint32_t fB_433cd5 = 0;
    uint32_t fA_433d3f = 0;
    uint32_t fB_433d3f = 0;
    uint32_t fA_433d4e = 0;
    uint32_t fB_433d4e = 0;
    uint32_t fA_433db6 = 0;
    uint32_t fB_433db6 = 0;
    uint32_t fA_433dc5 = 0;
    uint32_t fB_433dc5 = 0;
    uint32_t fA_433e6d = 0;
    uint32_t fB_433e6d = 0;
    FCmp fC_433e7f{};
    uint32_t fA_433e8f = 0;
    uint32_t fB_433e8f = 0;
    uint32_t fA_433e9f = 0;
    uint32_t fB_433e9f = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x68u;
    x1 = ldx(m, 0xd61660u);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0x8u);
    x2 = ldx(m, ebx + 0x100u);
    x0 = ldx(m, ebx + 0x180u);
    stx(m, esp + 0x1cu, x1);
    eax = rd32(m, esp + 0x28u);
    stx(m, esp + 0x1cu, x1);
    ecx = rd32(m, esp + 0x28u);
    stx(m, esp + 0xcu, x2);
    x2 = ldx(m, ebx + 0x740u);
    stx(m, esp + 0x1cu, x1);
    edx = rd32(m, esp + 0x28u);
    stx(m, esp + 0x2cu, x2);
    x2 = ldx(m, ebx + 0x7c0u);
    stx(m, esp + 0x4cu, x0);
    esp -= 4; wr32(m, esp, edi);
    wr32(m, esp + 0xcu, ebx);
    wr32(m, esp + 0x5cu, eax);
    wr32(m, esp + 0x1cu, ecx);
    wr32(m, esp + 0x3cu, edx);
    stx(m, esp + 0x60u, x2);
    stx(m, esp + 0x20u, x1);
    LIFT_CALL(0x42ccd0u, 0x4335cau);  // FUN_0042ccd0
    eax = rd32(m, esp + 0x2cu);
    stx(m, esp + 0x40u, x0);
    x0 = ldx(m, esp + 0x50u);
    wr32(m, esp + 0x4cu, eax);
    x1 = ldx(m, esp + 0x40u);
    x0 = subps(x0, x1);
    stx(m, esp + 0x40u, x0);
    ecx = rd32(m, esp + 0x40u);
    stx(m, esp + 0x50u, x0);
    edx = rd32(m, esp + 0x54u);
    stx(m, esp + 0x20u, x1);
    x1 = ldx(m, esp + 0x10u);
    stx(m, esp + 0x40u, x1);
    wr32(m, esp + 0x4cu, edx);
    x1 = ldx(m, esp + 0x40u);
    stx(m, esp + 0x40u, x0);
    x0 = ldx(m, esp + 0x30u);
    eax = rd32(m, esp + 0x48u);
    stx(m, esp + 0x30u, x0);
    wr32(m, esp + 0x3cu, eax);
    x0 = ldx(m, esp + 0x30u);
    stx(m, esp + 0x40u, x0);
    wr32(m, esp + 0x2cu, ecx);
    x0 = ldx(m, esp + 0x20u);
    stx(m, esi, x0);
    x3 = movss_m(rd32(m, esi));
    x0 = movss_m(rd32(m, esi + 0x4u));
    x2 = movss_m(rd32(m, esi + 0xcu));
    wr32(m, esp + 0x8u, x3.l(0));
    edx = rd32(m, esp + 0x8u);
    eax = edx;
    eax = (eax >> 16);
    edx = (edx & 0x7fffffffu);
    eax = (eax & 0x8000u);
    stx(m, esp + 0x50u, x1);
    x1 = movss_m(rd32(m, esi + 0x8u));
    fA_43366d = edx;
    fB_43366d = 0x47ffefffu;
    if (fA_43366d <= fB_43366d) goto L_43367c;
    ecx = 0x7fffu;
    goto L_4336be;
L_43367c:
    fA_43367c = edx;
    fB_43367c = 0x38800000u;
    if (fA_43367c >= fB_43367c) goto L_4336a0;
    edi = edx;
    edi = (edi >> 23);
    edx = (edx & 0x7fffffu);
    ecx = 0x71u;
    ecx = ecx - edi;
    edx = (edx | 0x800000u);
    edx = (edx >> ((ecx & 0xffu) & 31u));
    goto L_4336a6;
L_4336a0:
    edx = edx - 0x38000000u;
L_4336a6:
    ecx = edx;
    ecx = (ecx >> 13);
    ecx = (ecx & 0x1u);
    ecx = (ecx + edx + 0xfffu);
    ecx = (ecx >> 13);
    ecx = (ecx & 0x7fffu);
L_4336be:
    wr32(m, esp + 0x8u, x0.l(0));
    edi = rd32(m, esp + 0x8u);
    edx = edi;
    edx = (edx >> 16);
    eax = (eax | ecx);
    edi = (edi & 0x7fffffffu);
    edx = (edx & 0x8000u);
    wr16(m, esp + 0x10u, (eax & 0xffffu));
    fA_4336e0 = edi;
    fB_4336e0 = 0x47ffefffu;
    if (fA_4336e0 <= fB_4336e0) goto L_4336ef;
    ecx = 0x7fffu;
    goto L_433731;
L_4336ef:
    fA_4336ef = edi;
    fB_4336ef = 0x38800000u;
    if (fA_4336ef >= fB_4336ef) goto L_433713;
    eax = edi;
    eax = (eax >> 23);
    edi = (edi & 0x7fffffu);
    ecx = 0x71u;
    ecx = ecx - eax;
    edi = (edi | 0x800000u);
    edi = (edi >> ((ecx & 0xffu) & 31u));
    goto L_433719;
L_433713:
    edi = edi - 0x38000000u;
L_433719:
    ecx = edi;
    ecx = (ecx >> 13);
    ecx = (ecx & 0x1u);
    ecx = (ecx + edi + 0xfffu);
    ecx = (ecx >> 13);
    ecx = (ecx & 0x7fffu);
L_433731:
    edx = (edx | ecx);
    wr32(m, esp + 0x8u, x1.l(0));
    edi = rd32(m, esp + 0x8u);
    wr16(m, esp + 0x12u, (edx & 0xffffu));
    edx = edi;
    edx = (edx >> 16);
    edi = (edi & 0x7fffffffu);
    edx = (edx & 0x8000u);
    fA_433753 = edi;
    fB_433753 = 0x47ffefffu;
    if (fA_433753 <= fB_433753) goto L_433762;
    ecx = 0x7fffu;
    goto L_4337a4;
L_433762:
    fA_433762 = edi;
    fB_433762 = 0x38800000u;
    if (fA_433762 >= fB_433762) goto L_433786;
    eax = edi;
    eax = (eax >> 23);
    edi = (edi & 0x7fffffu);
    ecx = 0x71u;
    ecx = ecx - eax;
    edi = (edi | 0x800000u);
    edi = (edi >> ((ecx & 0xffu) & 31u));
    goto L_43378c;
L_433786:
    edi = edi - 0x38000000u;
L_43378c:
    ecx = edi;
    ecx = (ecx >> 13);
    ecx = (ecx & 0x1u);
    ecx = (ecx + edi + 0xfffu);
    ecx = (ecx >> 13);
    ecx = (ecx & 0x7fffu);
L_4337a4:
    edx = (edx | ecx);
    wr32(m, esp + 0x8u, x2.l(0));
    edi = rd32(m, esp + 0x8u);
    eax = (eax & 0xffff0000u) | (((edx & 0xffffu)) & 0xffffu);
    edx = edi;
    edx = (edx >> 16);
    edi = (edi & 0x7fffffffu);
    edx = (edx & 0x8000u);
    fA_4337c4 = edi;
    fB_4337c4 = 0x47ffefffu;
    if (fA_4337c4 <= fB_4337c4) goto L_4337d3;
    ecx = 0x7fffu;
    goto L_433819;
L_4337d3:
    fA_4337d3 = edi;
    fB_4337d3 = 0x38800000u;
    if (fA_4337d3 >= fB_4337d3) goto L_4337fb;
    ebx = edi;
    ebx = (ebx >> 23);
    edi = (edi & 0x7fffffu);
    ecx = 0x71u;
    ecx = ecx - ebx;
    ebx = rd32(m, esp + 0xcu);
    edi = (edi | 0x800000u);
    edi = (edi >> ((ecx & 0xffu) & 31u));
    goto L_433801;
L_4337fb:
    edi = edi - 0x38000000u;
L_433801:
    ecx = edi;
    ecx = (ecx >> 13);
    ecx = (ecx & 0x1u);
    ecx = (ecx + edi + 0xfffu);
    ecx = (ecx >> 13);
    ecx = (ecx & 0x7fffu);
L_433819:
    x0 = movss_m(rd32(m, esp + 0x50u));
    edx = (edx | ecx);
    ecx = rd16(m, esp + 0x10u);
    wr16(m, esi + 0x6u, (edx & 0xffffu));
    wr32(m, esp + 0x8u, x0.l(0));
    wr16(m, esi + 0x4u, (eax & 0xffffu));
    edx = rd32(m, esp + 0x8u);
    eax = edx;
    wr16(m, esi, (ecx & 0xffffu));
    ecx = rd16(m, esp + 0x12u);
    eax = (eax >> 16);
    edx = (edx & 0x7fffffffu);
    eax = (eax & 0x8000u);
    wr16(m, esi + 0x2u, (ecx & 0xffffu));
    fA_433854 = edx;
    fB_433854 = 0x47ffefffu;
    if (fA_433854 <= fB_433854) goto L_433863;
    ecx = 0x7fffu;
    goto L_4338a5;
L_433863:
    fA_433863 = edx;
    fB_433863 = 0x38800000u;
    if (fA_433863 >= fB_433863) goto L_433887;
    edi = edx;
    edi = (edi >> 23);
    edx = (edx & 0x7fffffu);
    ecx = 0x71u;
    ecx = ecx - edi;
    edx = (edx | 0x800000u);
    edx = (edx >> ((ecx & 0xffu) & 31u));
    goto L_43388d;
L_433887:
    edx = edx - 0x38000000u;
L_43388d:
    ecx = edx;
    ecx = (ecx >> 13);
    ecx = (ecx & 0x1u);
    ecx = (ecx + edx + 0xfffu);
    ecx = (ecx >> 13);
    ecx = (ecx & 0x7fffu);
L_4338a5:
    x0 = movss_m(rd32(m, esp + 0x54u));
    wr32(m, esp + 0x8u, x0.l(0));
    edi = rd32(m, esp + 0x8u);
    edx = edi;
    edx = (edx >> 16);
    eax = (eax | ecx);
    edi = (edi & 0x7fffffffu);
    edx = (edx & 0x8000u);
    wr16(m, esp + 0x10u, (eax & 0xffffu));
    fA_4338cd = edi;
    fB_4338cd = 0x47ffefffu;
    if (fA_4338cd <= fB_4338cd) goto L_4338dc;
    ecx = 0x7fffu;
    goto L_43391e;
L_4338dc:
    fA_4338dc = edi;
    fB_4338dc = 0x38800000u;
    if (fA_4338dc >= fB_4338dc) goto L_433900;
    eax = edi;
    eax = (eax >> 23);
    edi = (edi & 0x7fffffu);
    ecx = 0x71u;
    ecx = ecx - eax;
    edi = (edi | 0x800000u);
    edi = (edi >> ((ecx & 0xffu) & 31u));
    goto L_433906;
L_433900:
    edi = edi - 0x38000000u;
L_433906:
    ecx = edi;
    ecx = (ecx >> 13);
    ecx = (ecx & 0x1u);
    ecx = (ecx + edi + 0xfffu);
    ecx = (ecx >> 13);
    ecx = (ecx & 0x7fffu);
L_43391e:
    x0 = movss_m(rd32(m, esp + 0x58u));
    edx = (edx | ecx);
    wr32(m, esp + 0x8u, x0.l(0));
    edi = rd32(m, esp + 0x8u);
    wr16(m, esp + 0x12u, (edx & 0xffffu));
    edx = edi;
    edx = (edx >> 16);
    edi = (edi & 0x7fffffffu);
    edx = (edx & 0x8000u);
    fA_433946 = edi;
    fB_433946 = 0x47ffefffu;
    if (fA_433946 <= fB_433946) goto L_433955;
    ecx = 0x7fffu;
    goto L_433997;
L_433955:
    fA_433955 = edi;
    fB_433955 = 0x38800000u;
    if (fA_433955 >= fB_433955) goto L_433979;
    eax = edi;
    eax = (eax >> 23);
    edi = (edi & 0x7fffffu);
    ecx = 0x71u;
    ecx = ecx - eax;
    edi = (edi | 0x800000u);
    edi = (edi >> ((ecx & 0xffu) & 31u));
    goto L_43397f;
L_433979:
    edi = edi - 0x38000000u;
L_43397f:
    ecx = edi;
    ecx = (ecx >> 13);
    ecx = (ecx & 0x1u);
    ecx = (ecx + edi + 0xfffu);
    ecx = (ecx >> 13);
    ecx = (ecx & 0x7fffu);
L_433997:
    x0 = movss_m(rd32(m, esp + 0x5cu));
    edx = (edx | ecx);
    wr32(m, esp + 0x8u, x0.l(0));
    edi = rd32(m, esp + 0x8u);
    eax = (eax & 0xffff0000u) | (((edx & 0xffffu)) & 0xffffu);
    edx = edi;
    edx = (edx >> 16);
    edi = (edi & 0x7fffffffu);
    edx = (edx & 0x8000u);
    fA_4339bd = edi;
    fB_4339bd = 0x47ffefffu;
    if (fA_4339bd <= fB_4339bd) goto L_4339cc;
    ecx = 0x7fffu;
    goto L_433a12;
L_4339cc:
    fA_4339cc = edi;
    fB_4339cc = 0x38800000u;
    if (fA_4339cc >= fB_4339cc) goto L_4339f4;
    ebx = edi;
    ebx = (ebx >> 23);
    edi = (edi & 0x7fffffu);
    ecx = 0x71u;
    ecx = ecx - ebx;
    ebx = rd32(m, esp + 0xcu);
    edi = (edi | 0x800000u);
    edi = (edi >> ((ecx & 0xffu) & 31u));
    goto L_4339fa;
L_4339f4:
    edi = edi - 0x38000000u;
L_4339fa:
    ecx = edi;
    ecx = (ecx >> 13);
    ecx = (ecx & 0x1u);
    ecx = (ecx + edi + 0xfffu);
    ecx = (ecx >> 13);
    ecx = (ecx & 0x7fffu);
L_433a12:
    x0 = ldx(m, esp + 0x60u);
    edx = (edx | ecx);
    ecx = rd16(m, esp + 0x10u);
    wr16(m, esi + 0x8u, (ecx & 0xffffu));
    ecx = rd16(m, esp + 0x12u);
    wr16(m, esi + 0xcu, (eax & 0xffffu));
    wr16(m, esi + 0xeu, (edx & 0xffffu));
    wr16(m, esi + 0xau, (ecx & 0xffffu));
    stx(m, esi + 0x10u, x0);
    x3 = movss_m(rd32(m, esi + 0x10u));
    x0 = movss_m(rd32(m, esi + 0x14u));
    x1 = movss_m(rd32(m, esi + 0x18u));
    x2 = movss_m(rd32(m, esi + 0x1cu));
    wr32(m, esp + 0x8u, x3.l(0));
    edx = rd32(m, esp + 0x8u);
    eax = edx;
    eax = (eax >> 16);
    edx = (edx & 0x7fffffffu);
    eax = (eax & 0x8000u);
    fA_433a65 = edx;
    fB_433a65 = 0x47ffefffu;
    if (fA_433a65 <= fB_433a65) goto L_433a74;
    ecx = 0x7fffu;
    goto L_433ab6;
L_433a74:
    fA_433a74 = edx;
    fB_433a74 = 0x38800000u;
    if (fA_433a74 >= fB_433a74) goto L_433a98;
    edi = edx;
    edi = (edi >> 23);
    edx = (edx & 0x7fffffu);
    ecx = 0x71u;
    ecx = ecx - edi;
    edx = (edx | 0x800000u);
    edx = (edx >> ((ecx & 0xffu) & 31u));
    goto L_433a9e;
L_433a98:
    edx = edx - 0x38000000u;
L_433a9e:
    ecx = edx;
    ecx = (ecx >> 13);
    ecx = (ecx & 0x1u);
    ecx = (ecx + edx + 0xfffu);
    ecx = (ecx >> 13);
    ecx = (ecx & 0x7fffu);
L_433ab6:
    wr32(m, esp + 0x8u, x0.l(0));
    edi = rd32(m, esp + 0x8u);
    edx = edi;
    edx = (edx >> 16);
    eax = (eax | ecx);
    edi = (edi & 0x7fffffffu);
    edx = (edx & 0x8000u);
    wr16(m, esp + 0x10u, (eax & 0xffffu));
    fA_433ad8 = edi;
    fB_433ad8 = 0x47ffefffu;
    if (fA_433ad8 <= fB_433ad8) goto L_433ae7;
    ecx = 0x7fffu;
    goto L_433b29;
L_433ae7:
    fA_433ae7 = edi;
    fB_433ae7 = 0x38800000u;
    if (fA_433ae7 >= fB_433ae7) goto L_433b0b;
    eax = edi;
    eax = (eax >> 23);
    edi = (edi & 0x7fffffu);
    ecx = 0x71u;
    ecx = ecx - eax;
    edi = (edi | 0x800000u);
    edi = (edi >> ((ecx & 0xffu) & 31u));
    goto L_433b11;
L_433b0b:
    edi = edi - 0x38000000u;
L_433b11:
    ecx = edi;
    ecx = (ecx >> 13);
    ecx = (ecx & 0x1u);
    ecx = (ecx + edi + 0xfffu);
    ecx = (ecx >> 13);
    ecx = (ecx & 0x7fffu);
L_433b29:
    edx = (edx | ecx);
    wr32(m, esp + 0x8u, x1.l(0));
    edi = rd32(m, esp + 0x8u);
    wr16(m, esp + 0x12u, (edx & 0xffffu));
    edx = edi;
    edx = (edx >> 16);
    edi = (edi & 0x7fffffffu);
    edx = (edx & 0x8000u);
    fA_433b4b = edi;
    fB_433b4b = 0x47ffefffu;
    if (fA_433b4b <= fB_433b4b) goto L_433b5a;
    ecx = 0x7fffu;
    goto L_433b9c;
L_433b5a:
    fA_433b5a = edi;
    fB_433b5a = 0x38800000u;
    if (fA_433b5a >= fB_433b5a) goto L_433b7e;
    eax = edi;
    eax = (eax >> 23);
    edi = (edi & 0x7fffffu);
    ecx = 0x71u;
    ecx = ecx - eax;
    edi = (edi | 0x800000u);
    edi = (edi >> ((ecx & 0xffu) & 31u));
    goto L_433b84;
L_433b7e:
    edi = edi - 0x38000000u;
L_433b84:
    ecx = edi;
    ecx = (ecx >> 13);
    ecx = (ecx & 0x1u);
    ecx = (ecx + edi + 0xfffu);
    ecx = (ecx >> 13);
    ecx = (ecx & 0x7fffu);
L_433b9c:
    edx = (edx | ecx);
    wr32(m, esp + 0x8u, x2.l(0));
    edi = rd32(m, esp + 0x8u);
    eax = (eax & 0xffff0000u) | (((edx & 0xffffu)) & 0xffffu);
    edx = edi;
    edx = (edx >> 16);
    edi = (edi & 0x7fffffffu);
    edx = (edx & 0x8000u);
    fA_433bbc = edi;
    fB_433bbc = 0x47ffefffu;
    if (fA_433bbc <= fB_433bbc) goto L_433bcb;
    ecx = 0x7fffu;
    goto L_433c11;
L_433bcb:
    fA_433bcb = edi;
    fB_433bcb = 0x38800000u;
    if (fA_433bcb >= fB_433bcb) goto L_433bf3;
    ebx = edi;
    ebx = (ebx >> 23);
    edi = (edi & 0x7fffffu);
    ecx = 0x71u;
    ecx = ecx - ebx;
    ebx = rd32(m, esp + 0xcu);
    edi = (edi | 0x800000u);
    edi = (edi >> ((ecx & 0xffu) & 31u));
    goto L_433bf9;
L_433bf3:
    edi = edi - 0x38000000u;
L_433bf9:
    ecx = edi;
    ecx = (ecx >> 13);
    ecx = (ecx & 0x1u);
    ecx = (ecx + edi + 0xfffu);
    ecx = (ecx >> 13);
    ecx = (ecx & 0x7fffu);
L_433c11:
    x0 = movss_m(rd32(m, esp + 0x40u));
    edx = (edx | ecx);
    ecx = rd16(m, esp + 0x10u);
    wr16(m, esi + 0x16u, (edx & 0xffffu));
    wr32(m, esp + 0x8u, x0.l(0));
    wr16(m, esi + 0x14u, (eax & 0xffffu));
    edx = rd32(m, esp + 0x8u);
    eax = edx;
    wr16(m, esi + 0x10u, (ecx & 0xffffu));
    ecx = rd16(m, esp + 0x12u);
    eax = (eax >> 16);
    edx = (edx & 0x7fffffffu);
    eax = (eax & 0x8000u);
    wr16(m, esi + 0x12u, (ecx & 0xffffu));
    fA_433c4d = edx;
    fB_433c4d = 0x47ffefffu;
    if (fA_433c4d <= fB_433c4d) goto L_433c5c;
    ecx = 0x7fffu;
    goto L_433c9e;
L_433c5c:
    fA_433c5c = edx;
    fB_433c5c = 0x38800000u;
    if (fA_433c5c >= fB_433c5c) goto L_433c80;
    edi = edx;
    edi = (edi >> 23);
    edx = (edx & 0x7fffffu);
    ecx = 0x71u;
    ecx = ecx - edi;
    edx = (edx | 0x800000u);
    edx = (edx >> ((ecx & 0xffu) & 31u));
    goto L_433c86;
L_433c80:
    edx = edx - 0x38000000u;
L_433c86:
    ecx = edx;
    ecx = (ecx >> 13);
    ecx = (ecx & 0x1u);
    ecx = (ecx + edx + 0xfffu);
    ecx = (ecx >> 13);
    ecx = (ecx & 0x7fffu);
L_433c9e:
    x0 = movss_m(rd32(m, esp + 0x44u));
    wr32(m, esp + 0x8u, x0.l(0));
    edi = rd32(m, esp + 0x8u);
    edx = edi;
    edx = (edx >> 16);
    eax = (eax | ecx);
    edi = (edi & 0x7fffffffu);
    edx = (edx & 0x8000u);
    wr16(m, esp + 0x10u, (eax & 0xffffu));
    fA_433cc6 = edi;
    fB_433cc6 = 0x47ffefffu;
    if (fA_433cc6 <= fB_433cc6) goto L_433cd5;
    ecx = 0x7fffu;
    goto L_433d17;
L_433cd5:
    fA_433cd5 = edi;
    fB_433cd5 = 0x38800000u;
    if (fA_433cd5 >= fB_433cd5) goto L_433cf9;
    eax = edi;
    eax = (eax >> 23);
    edi = (edi & 0x7fffffu);
    ecx = 0x71u;
    ecx = ecx - eax;
    edi = (edi | 0x800000u);
    edi = (edi >> ((ecx & 0xffu) & 31u));
    goto L_433cff;
L_433cf9:
    edi = edi - 0x38000000u;
L_433cff:
    ecx = edi;
    ecx = (ecx >> 13);
    ecx = (ecx & 0x1u);
    ecx = (ecx + edi + 0xfffu);
    ecx = (ecx >> 13);
    ecx = (ecx & 0x7fffu);
L_433d17:
    x0 = movss_m(rd32(m, esp + 0x48u));
    edx = (edx | ecx);
    wr32(m, esp + 0x8u, x0.l(0));
    edi = rd32(m, esp + 0x8u);
    wr16(m, esp + 0x12u, (edx & 0xffffu));
    edx = edi;
    edx = (edx >> 16);
    edi = (edi & 0x7fffffffu);
    edx = (edx & 0x8000u);
    fA_433d3f = edi;
    fB_433d3f = 0x47ffefffu;
    if (fA_433d3f <= fB_433d3f) goto L_433d4e;
    ecx = 0x7fffu;
    goto L_433d90;
L_433d4e:
    fA_433d4e = edi;
    fB_433d4e = 0x38800000u;
    if (fA_433d4e >= fB_433d4e) goto L_433d72;
    eax = edi;
    eax = (eax >> 23);
    edi = (edi & 0x7fffffu);
    ecx = 0x71u;
    ecx = ecx - eax;
    edi = (edi | 0x800000u);
    edi = (edi >> ((ecx & 0xffu) & 31u));
    goto L_433d78;
L_433d72:
    edi = edi - 0x38000000u;
L_433d78:
    ecx = edi;
    ecx = (ecx >> 13);
    ecx = (ecx & 0x1u);
    ecx = (ecx + edi + 0xfffu);
    ecx = (ecx >> 13);
    ecx = (ecx & 0x7fffu);
L_433d90:
    x0 = movss_m(rd32(m, esp + 0x4cu));
    edx = (edx | ecx);
    wr32(m, esp + 0x8u, x0.l(0));
    edi = rd32(m, esp + 0x8u);
    eax = (eax & 0xffff0000u) | (((edx & 0xffffu)) & 0xffffu);
    edx = edi;
    edx = (edx >> 16);
    edi = (edi & 0x7fffffffu);
    edx = (edx & 0x8000u);
    fA_433db6 = edi;
    fB_433db6 = 0x47ffefffu;
    if (fA_433db6 <= fB_433db6) goto L_433dc5;
    ecx = 0x7fffu;
    goto L_433e0b;
L_433dc5:
    fA_433dc5 = edi;
    fB_433dc5 = 0x38800000u;
    if (fA_433dc5 >= fB_433dc5) goto L_433ded;
    ebx = edi;
    ebx = (ebx >> 23);
    edi = (edi & 0x7fffffu);
    ecx = 0x71u;
    ecx = ecx - ebx;
    ebx = rd32(m, esp + 0xcu);
    edi = (edi | 0x800000u);
    edi = (edi >> ((ecx & 0xffu) & 31u));
    goto L_433df3;
L_433ded:
    edi = edi - 0x38000000u;
L_433df3:
    ecx = edi;
    ecx = (ecx >> 13);
    ecx = (ecx & 0x1u);
    ecx = (ecx + edi + 0xfffu);
    ecx = (ecx >> 13);
    ecx = (ecx & 0x7fffu);
L_433e0b:
    x0 = movss_m(rd32(m, 0xa89be8u));
    x0 = addss(x0, rd32(m, 0xa89be4u));
    wr16(m, esi + 0x1cu, (eax & 0xffffu));
    edx = (edx | ecx);
    ecx = rd16(m, esp + 0x10u);
    wr16(m, esi + 0x18u, (ecx & 0xffffu));
    ecx = rd16(m, esp + 0x12u);
    wr16(m, esi + 0x1au, (ecx & 0xffffu));
    wr16(m, esi + 0x1eu, (edx & 0xffffu));
    wr32(m, esi + 0x20u, x0.l(0));
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0x1cu)));
    wr32(m, esi + 0x24u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0x24u)));
    wr32(m, esi + 0x28u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0x28u)));
    wr32(m, esi + 0x2cu, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0x2cu)));
    wr32(m, esi + 0x30u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0x30u)));
    wr32(m, esi + 0x34u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0x34u)));
    wr32(m, esi + 0x38u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0x38u)));
    wr32(m, esi + 0x40u, 0x0u);
    wr32(m, esi + 0x3cu, fst32(m));
    fpop(m);
    fA_433e6d = rd32(m, ebx + 0x48u);
    fB_433e6d = 0x0u;
    if (fA_433e6d == fB_433e6d) goto L_433e7a;
    wr32(m, esi + 0x40u, 0x10000u);
L_433e7a:
    x0 = movss_m(rd32(m, ebx + 0x20u));
    fC_433e7f = fcmp32(x0.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    if ((fc_b(fC_433e7f) || fc_z(fC_433e7f))) goto L_433e8f;
    wr32(m, esi + 0x40u, (rd32(m, esi + 0x40u) | 0x20000u));
L_433e8f:
    fA_433e8f = rd32(m, ebx + 0x2e98u);
    fB_433e8f = 0x0u;
    if (fA_433e8f == fB_433e8f) goto L_433e9f;
    wr32(m, esi + 0x40u, (rd32(m, esi + 0x40u) | 0x40000u));
L_433e9f:
    fA_433e9f = rd32(m, ebx + 0x2374u);
    fB_433e9f = 0x0u;
    if (fA_433e9f == fB_433e9f) goto L_433eaf;
    wr32(m, esi + 0x40u, (rd32(m, esi + 0x40u) | 0x80000u));
L_433eaf:
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}
