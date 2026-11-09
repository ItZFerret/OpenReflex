// Particle / world-entity ports (group "fx"), transliterated instruction by instruction from MXReflex.exe by the fx
// lifter (a copy of the bike lifter, see native/game/notes/fx.md). Each function keeps the original's operations, order and
// memory traffic (stack frame included).
#include <stdexcept>
#include <utility>
#include "lift_rt.h"

// 006c0590 PopDownBale__vf22
//   original PopDownBale::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x006c0590, PopDownBale__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x1032u;
    LIFT_RET(0);
}

// 006c08f0 PopDownBale__vf19
//   original PopDownBale::vf19; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x006c08f0, PopDownBale__vf19, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_6c0906 = 0;
    uint32_t fB_6c0906 = 0;
    uint32_t fA_6c0918 = 0;
    uint32_t fB_6c0918 = 0;
    uint32_t fA_6c092d = 0;
    uint32_t fB_6c092d = 0;
    uint32_t fA_6c093d = 0;
    uint32_t fB_6c093d = 0;
    uint32_t fA_6c0955 = 0;
    uint32_t fB_6c0955 = 0;
    uint32_t fA_6c0962 = 0;
    uint32_t fB_6c0962 = 0;
    FCmp fC_6c09f5{};
    uint32_t fR_6c0a04 = 0;
    uint32_t fR_6c0a20 = 0;
    uint32_t fA_6c0a36 = 0;
    uint32_t fB_6c0a36 = 0;
    FCmp fC_6c0ab3{};
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x38u;
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    eax = rd32(m, esi + 0x10u);
    ecx = rd32(m, 0xa95844u);
    esp -= 4; wr32(m, esp, edi);
    fA_6c0906 = eax;
    fB_6c0906 = rd32(m, ecx + 0x20u);
    if (fA_6c0906 != fB_6c0906) goto L_6c0918;
    eax = rd32(m, 0xa946b8u);
    eax = rd32(m, eax + 0x5a54u);
    goto L_6c092d;
L_6c0918:
    fA_6c0918 = eax;
    fB_6c0918 = rd32(m, ecx + 0x1cu);
    if (fA_6c0918 != fB_6c0918) goto L_6c092b;
    edx = rd32(m, 0xa946b8u);
    eax = rd32(m, edx + 0x20c8u);
    goto L_6c092d;
L_6c092b:
    eax = 0u;
L_6c092d:
    fA_6c092d = rd32(m, eax + 0x20014u);
    fB_6c092d = 0x0u;
    if (fA_6c092d != fB_6c092d) goto L_6c0b51;
    eax = rd32(m, ecx + 0x10u);
    fA_6c093d = rd32(m, eax + 0xcu);
    fB_6c093d = 0x0u;
    if (fA_6c093d != fB_6c093d) goto L_6c0b51;
    edx = rd32(m, esi);
    edx = rd32(m, edx + 0x44u);
    eax = (esp + 0x30u);
    esp -= 4; wr32(m, esp, eax);
    ecx = esi;
    LIFT_CALL(edx, 0x6c0955u);
    fA_6c0955 = rd32(m, esi + 0x178u);
    fB_6c0955 = 0x0u;
    if (fA_6c0955 != fB_6c0955) goto L_6c0b51;
    fA_6c0962 = rd32(m, esi + 0x174u);
    fB_6c0962 = 0x1u;
    if (fA_6c0962 != fB_6c0962) goto L_6c0b51;
    x0 = ldx(m, esi + 0x1d0u);
    stx(m, esp + 0x20u, x0);
    x1 = movss_m(rd32(m, esp + 0x24u));
    x1 = subss(x1, rd32(m, esp + 0x34u));
    x2 = movss_m(rd32(m, esp + 0x28u));
    x2 = subss(x2, rd32(m, esp + 0x38u));
    x0 = movss_m(rd32(m, esp + 0x20u));
    x0 = subss(x0, rd32(m, esp + 0x30u));
    x3 = x1;
    wr32(m, esp + 0x18u, x2.l(0));
    x3 = mulss(x3, x1.l(0));
    wr32(m, esp + 0x14u, x1.l(0));
    x2 = mulss(x2, x2.l(0));
    x1 = x0;
    x1 = mulss(x1, x0.l(0));
    x2 = addss(x2, x3.l(0));
    x2 = addss(x2, x1.l(0));
    wr32(m, esp + 0x8u, x2.l(0));
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x8u)));
    wr32(m, esp + 0x10u, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x1e0u));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    wr32(m, esp + 0x10u, x0.l(0));
    LIFT_CALL(0x42c230u, 0x6c09ecu);  // FUN_0042c230
    x1 = movss_m(rd32(m, esp + 0x10u));
    esp = esp + 0x4u;
    fC_6c09f5 = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_6c09f5) || fc_z(fC_6c09f5))) goto L_6c0aad;
    eax = rd32(m, esi + 0x158u);
    fR_6c0a04 = (eax & eax);
    if ((fR_6c0a04 == 0)) goto L_6c0a1a;
    LIFT_CALL(0x418f30u, 0x6c0a0du);  // PhysicsHandle_Body
    x0 = ldx(m, esi + 0x1d0u);
    eax = eax + 0x40u;
    stx(m, eax, x0);
L_6c0a1a:
    eax = rd32(m, esi + 0x158u);
    fR_6c0a20 = (eax & eax);
    if ((fR_6c0a20 == 0)) goto L_6c0a36;
    LIFT_CALL(0x418f30u, 0x6c0a29u);  // PhysicsHandle_Body
    x0 = ldx(m, esi + 0x1d0u);
    eax = eax + 0x60u;
    stx(m, eax, x0);
L_6c0a36:
    fA_6c0a36 = rd32(m, esi + 0x158u);
    fB_6c0a36 = 0x0u;
    if (fA_6c0a36 == fB_6c0a36) goto L_6c0a67;
    eax = rd32(m, esi + 0x158u);
    edi = rd32(m, eax + 0x4u);
    LIFT_CALL(0x418f30u, 0x6c0a4du);  // PhysicsHandle_Body
    esp -= 4; wr32(m, esp, edi);
    edx = eax;
    LIFT_CALL(0x76a8b0u, 0x6c0a55u);  // FUN_0076a8b0
    eax = rd32(m, esi + 0x158u);
    LIFT_CALL(0x418f30u, 0x6c0a60u);  // PhysicsHandle_Body
    wr8(m, eax + 0xb0u, 0x0u);
L_6c0a67:
    x0 = ldx(m, esi + 0x1d0u);
    stx(m, esp + 0x20u, x0);
    x0 = movss_m(rd32(m, esp + 0x20u));
    wr32(m, esi + 0x60u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x24u));
    wr32(m, esi + 0x64u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x28u));
    wr32(m, esi + 0x178u, 0x1u);
    wr8(m, esi + 0xa2u, 0x0u);
    wr32(m, esi + 0x68u, x0.l(0));
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_6c0aad:
    x0 = movss_m(rd32(m, esp + 0x8u));
    fC_6c0ab3 = fcmp32(x0.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    wr32(m, esp + 0x8u, x0.l(0));
    if ((fc_b(fC_6c0ab3) || fc_z(fC_6c0ab3))) goto L_6c0b06;
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x8u)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x42c230u, 0x6c0acfu);  // FUN_0042c230
    x3 = movss_m(rd32(m, esp + 0x1cu));
    x1 = x0;
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x0 = divss(x0, x1.l(0));
    x1 = x0;
    x1 = mulss(x1, rd32(m, esp + 0x14u));
    x4 = x1;
    x1 = movss_m(rd32(m, esp + 0x18u));
    x1 = mulss(x1, x0.l(0));
    esp = esp + 0x4u;
    x2 = x1;
    x3 = mulss(x3, x0.l(0));
    goto L_6c0b18;
L_6c0b06:
    x3 = movss_m(rd32(m, esp + 0x18u));
    x2 = movss_m(rd32(m, esp + 0x14u));
    x4 = movss_m(rd32(m, esp + 0x10u));
L_6c0b18:
    x1 = movss_m(rd32(m, esp + 0xcu));
    x0 = x1;
    x2 = mulss(x2, x1.l(0));
    x2 = addss(x2, rd32(m, esp + 0x34u));
    x3 = mulss(x3, x1.l(0));
    x0 = mulss(x0, x4.l(0));
    x0 = addss(x0, rd32(m, esp + 0x30u));
    x1 = x3;
    x1 = addss(x1, rd32(m, esp + 0x38u));
    wr32(m, esi + 0x60u, x0.l(0));
    wr32(m, esi + 0x64u, x2.l(0));
    wr32(m, esi + 0x68u, x1.l(0));
L_6c0b51:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 006c0db0 PopUpBale__vf22
//   original PopUpBale::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x006c0db0, PopUpBale__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x1031u;
    LIFT_RET(0);
}

// 006c1250 PopUpBale__vf19
//   original PopUpBale::vf19; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x006c1250, PopUpBale__vf19, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_6c1266 = 0;
    uint32_t fB_6c1266 = 0;
    uint32_t fA_6c1278 = 0;
    uint32_t fB_6c1278 = 0;
    uint32_t fA_6c128d = 0;
    uint32_t fB_6c128d = 0;
    uint32_t fA_6c129d = 0;
    uint32_t fB_6c129d = 0;
    uint32_t fA_6c12b5 = 0;
    uint32_t fB_6c12b5 = 0;
    uint32_t fA_6c12c7 = 0;
    uint32_t fB_6c12c7 = 0;
    FCmp fC_6c1359{};
    uint32_t fR_6c1368 = 0;
    uint32_t fR_6c1384 = 0;
    uint32_t fR_6c13a0 = 0;
    FCmp fC_6c13f6{};
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x38u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    eax = rd32(m, esi + 0x10u);
    ecx = rd32(m, 0xa95844u);
    fA_6c1266 = eax;
    fB_6c1266 = rd32(m, ecx + 0x20u);
    if (fA_6c1266 != fB_6c1266) goto L_6c1278;
    eax = rd32(m, 0xa946b8u);
    eax = rd32(m, eax + 0x5a54u);
    goto L_6c128d;
L_6c1278:
    fA_6c1278 = eax;
    fB_6c1278 = rd32(m, ecx + 0x1cu);
    if (fA_6c1278 != fB_6c1278) goto L_6c128b;
    edx = rd32(m, 0xa946b8u);
    eax = rd32(m, edx + 0x20c8u);
    goto L_6c128d;
L_6c128b:
    eax = 0u;
L_6c128d:
    fA_6c128d = rd32(m, eax + 0x20014u);
    fB_6c128d = 0x0u;
    if (fA_6c128d != fB_6c128d) goto L_6c1494;
    eax = rd32(m, ecx + 0x10u);
    fA_6c129d = rd32(m, eax + 0xcu);
    fB_6c129d = 0x0u;
    if (fA_6c129d != fB_6c129d) goto L_6c1494;
    edx = rd32(m, esi);
    edx = rd32(m, edx + 0x44u);
    eax = (esp + 0x30u);
    esp -= 4; wr32(m, esp, eax);
    ecx = esi;
    LIFT_CALL(edx, 0x6c12b5u);
    fA_6c12b5 = rd32(m, esi + 0x174u);
    fB_6c12b5 = 0x0u;
    if (fA_6c12b5 != fB_6c12b5) goto L_6c1494;
    ebx = 0x1u;
    fA_6c12c7 = rd32(m, esi + 0x170u);
    fB_6c12c7 = ebx;
    if (fA_6c12c7 != fB_6c12c7) goto L_6c1494;
    x0 = ldx(m, esi + 0x1d0u);
    stx(m, esp + 0x20u, x0);
    x1 = movss_m(rd32(m, esp + 0x24u));
    x1 = subss(x1, rd32(m, esp + 0x34u));
    x2 = movss_m(rd32(m, esp + 0x28u));
    x2 = subss(x2, rd32(m, esp + 0x38u));
    x0 = movss_m(rd32(m, esp + 0x20u));
    x0 = subss(x0, rd32(m, esp + 0x30u));
    x3 = x1;
    wr32(m, esp + 0x18u, x2.l(0));
    x3 = mulss(x3, x1.l(0));
    wr32(m, esp + 0x14u, x1.l(0));
    x2 = mulss(x2, x2.l(0));
    x1 = x0;
    x1 = mulss(x1, x0.l(0));
    x2 = addss(x2, x3.l(0));
    x2 = addss(x2, x1.l(0));
    wr32(m, esp + 0x8u, x2.l(0));
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x8u)));
    wr32(m, esp + 0x10u, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x1e0u));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    wr32(m, esp + 0x10u, x0.l(0));
    LIFT_CALL(0x42c230u, 0x6c1350u);  // FUN_0042c230
    x1 = movss_m(rd32(m, esp + 0x10u));
    esp = esp + 0x4u;
    fC_6c1359 = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_6c1359) || fc_z(fC_6c1359))) goto L_6c13f0;
    eax = rd32(m, esi + 0x158u);
    fR_6c1368 = (eax & eax);
    if ((fR_6c1368 == 0)) goto L_6c137e;
    LIFT_CALL(0x418f30u, 0x6c1371u);  // PhysicsHandle_Body
    x0 = ldx(m, esi + 0x1d0u);
    eax = eax + 0x40u;
    stx(m, eax, x0);
L_6c137e:
    eax = rd32(m, esi + 0x158u);
    fR_6c1384 = (eax & eax);
    if ((fR_6c1384 == 0)) goto L_6c139a;
    LIFT_CALL(0x418f30u, 0x6c138du);  // PhysicsHandle_Body
    x0 = ldx(m, esi + 0x1d0u);
    eax = eax + 0x60u;
    stx(m, eax, x0);
L_6c139a:
    eax = rd32(m, esi + 0x158u);
    fR_6c13a0 = (eax & eax);
    if ((fR_6c13a0 == 0)) goto L_6c13af;
    LIFT_CALL(0x418f30u, 0x6c13a9u);  // PhysicsHandle_Body
    wr8(m, eax + 0xb0u, (ebx & 0xffu));
L_6c13af:
    x0 = ldx(m, esi + 0x1d0u);
    stx(m, esp + 0x20u, x0);
    x0 = movss_m(rd32(m, esp + 0x20u));
    wr32(m, esi + 0x60u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x24u));
    wr32(m, esi + 0x64u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x28u));
    wr32(m, esi + 0x174u, ebx);
    wr8(m, esi + 0xa2u, (ebx & 0xffu));
    wr32(m, esi + 0x68u, x0.l(0));
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_6c13f0:
    x0 = movss_m(rd32(m, esp + 0x8u));
    fC_6c13f6 = fcmp32(x0.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    wr32(m, esp + 0x8u, x0.l(0));
    if ((fc_b(fC_6c13f6) || fc_z(fC_6c13f6))) goto L_6c1449;
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x8u)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x42c230u, 0x6c1412u);  // FUN_0042c230
    x3 = movss_m(rd32(m, esp + 0x1cu));
    x1 = x0;
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x0 = divss(x0, x1.l(0));
    x1 = x0;
    x1 = mulss(x1, rd32(m, esp + 0x14u));
    x4 = x1;
    x1 = movss_m(rd32(m, esp + 0x18u));
    x1 = mulss(x1, x0.l(0));
    esp = esp + 0x4u;
    x2 = x1;
    x3 = mulss(x3, x0.l(0));
    goto L_6c145b;
L_6c1449:
    x3 = movss_m(rd32(m, esp + 0x18u));
    x2 = movss_m(rd32(m, esp + 0x14u));
    x4 = movss_m(rd32(m, esp + 0x10u));
L_6c145b:
    x1 = movss_m(rd32(m, esp + 0xcu));
    x0 = x1;
    x2 = mulss(x2, x1.l(0));
    x2 = addss(x2, rd32(m, esp + 0x34u));
    x3 = mulss(x3, x1.l(0));
    x0 = mulss(x0, x4.l(0));
    x0 = addss(x0, rd32(m, esp + 0x30u));
    x1 = x3;
    x1 = addss(x1, rd32(m, esp + 0x38u));
    wr32(m, esi + 0x60u, x0.l(0));
    wr32(m, esi + 0x64u, x2.l(0));
    wr32(m, esi + 0x68u, x1.l(0));
L_6c1494:
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 006c15a0 ScoreTowerFlagHolder__vf22
//   original ScoreTowerFlagHolder::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x006c15a0, ScoreTowerFlagHolder__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x1039u;
    LIFT_RET(0);
}

// 006c2a30 TrackSideFlagHolder__vf22
//   original TrackSideFlagHolder::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x006c2a30, TrackSideFlagHolder__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x1038u;
    LIFT_RET(0);
}

// 006c2eb0 TrackSideFlagHolder__vf19
//   original TrackSideFlagHolder::vf19; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x006c2eb0, TrackSideFlagHolder__vf19, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_6c2ebc = 0;
    uint32_t fB_6c2ebc = 0;
    uint32_t fA_6c2ece = 0;
    uint32_t fB_6c2ece = 0;
    uint32_t fA_6c2ee3 = 0;
    uint32_t fB_6c2ee3 = 0;
    uint32_t fA_6c2eef = 0;
    uint32_t fB_6c2eef = 0;
    uint32_t fA_6c2ef5 = 0;
    uint32_t fB_6c2ef5 = 0;
    uint32_t fA_6c2f18 = 0;
    uint32_t fB_6c2f18 = 0;
    uint32_t fA_6c2f1d = 0;
    uint32_t fB_6c2f1d = 0;
    esp -= 4; wr32(m, esp, edi);
    edi = ecx;
    eax = rd32(m, edi + 0x10u);
    ecx = rd32(m, 0xa95844u);
    fA_6c2ebc = eax;
    fB_6c2ebc = rd32(m, ecx + 0x20u);
    if (fA_6c2ebc != fB_6c2ebc) goto L_6c2ece;
    eax = rd32(m, 0xa946b8u);
    eax = rd32(m, eax + 0x5a54u);
    goto L_6c2ee3;
L_6c2ece:
    fA_6c2ece = eax;
    fB_6c2ece = rd32(m, ecx + 0x1cu);
    if (fA_6c2ece != fB_6c2ece) goto L_6c2ee1;
    edx = rd32(m, 0xa946b8u);
    eax = rd32(m, edx + 0x20c8u);
    goto L_6c2ee3;
L_6c2ee1:
    eax = 0u;
L_6c2ee3:
    fA_6c2ee3 = rd32(m, eax + 0x20014u);
    fB_6c2ee3 = 0x0u;
    if (fA_6c2ee3 != fB_6c2ee3) goto L_6c2f50;
    eax = rd32(m, ecx + 0x10u);
    fA_6c2eef = rd32(m, eax + 0xcu);
    fB_6c2eef = 0x0u;
    if (fA_6c2eef != fB_6c2eef) goto L_6c2f50;
    fA_6c2ef5 = rd32(m, edi + 0x360u);
    fB_6c2ef5 = 0x0u;
    if (fA_6c2ef5 != fB_6c2ef5) goto L_6c2f50;
    x1 = movss_m(rd32(m, esp + 0x8u));
    esp -= 4; wr32(m, esp, esi);
    esi = (edi + 0x148u);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x835430u, 0x6c2f11u);  // Character_SampleKeyframes
    eax = rd32(m, edi + 0x370u);
    eax = (eax - 1u);
    fA_6c2f18 = eax;
    fB_6c2f18 = 0x1u;
    if (fA_6c2f18 > fB_6c2f18) goto L_6c2f4f;
    fA_6c2f1d = rd32(m, edi + 0x16cu);
    fB_6c2f1d = 0x0u;
    if (fA_6c2f1d == fB_6c2f1d) goto L_6c2f4f;
    eax = rd32(m, edi + 0x358u);
    x0 = X{};
    wr32(m, esi + 0x24u, 0x0u);
    wr32(m, esi + 0x28u, 0x0u);
    wr32(m, esi + 0x2cu, eax);
    wr32(m, esi + 0x3cu, x0.l(0));
    wr32(m, edi + 0x370u, 0x0u);
L_6c2f4f:
    esi = rd32(m, esp); esp += 4;
L_6c2f50:
    edi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 006c5960 rtti_shared_vf_006c5960_006c5960
//   original rtti_shared_vf_006c5960; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x006c5960, rtti_shared_vf_006c5960_006c5960, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = ecx;
    ecx = rd32(m, esp + 0x4u);
    ecx = rd32(m, ecx);
    edx = rd32(m, ecx + 0x4u);
    ecx = rd32(m, ecx);
    esp -= 4; wr32(m, esp, edx);
    edx = rd32(m, eax + 0x10u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = rd32(m, eax + 0x14u);
    ecx = ecx + rd32(m, eax + 0xcu);
    LIFT_CALL(edx, 0x6c597au);
    LIFT_RET(4);
}

// 006c8230 Fn_006c8230
//   original FUN_006c8230; register inputs: edx esi edi; returns with ret 0
REFLEX_PORT_FN(0x006c8230, Fn_006c8230, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fR_6c8234 = 0;
    uint32_t fR_6c8248 = 0;
    eax = esi;
    ecx = edi;
    fR_6c8234 = (esi & esi);
    if ((false || (fR_6c8234 == 0))) goto L_6c824d;
    esp -= 4; wr32(m, esp, ebx);
L_6c8240:
    ebx = rd32(m, edx);
    wr32(m, ecx, ebx);
    eax = (eax - 1u);
    ecx = ecx + 0x4u;
    fR_6c8248 = (eax & eax);
    if (!(false || (fR_6c8248 == 0))) goto L_6c8240;
    ebx = rd32(m, esp); esp += 4;
L_6c824d:
    eax = (edi + esi * 4);
    LIFT_RET(0);
}

// 006c8570 Fn_006c8570
//   original FUN_006c8570; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x006c8570, Fn_006c8570, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_6c85a0 = 0;
    uint32_t fB_6c85a0 = 0;
    uint32_t fA_6c85bc = 0;
    uint32_t fB_6c85bc = 0;
    uint32_t fR_6c85c8 = 0;
    uint32_t fR_6c85d3 = 0;
    uint32_t fA_6c85de = 0;
    uint32_t fB_6c85de = 0;
    uint32_t fA_6c85f2 = 0;
    uint32_t fB_6c85f2 = 0;
    uint32_t FL = 0;
    uint32_t fR_6c85f6 = 0;
    uint32_t fA_6c85fd = 0;
    uint32_t fB_6c85fd = 0;
    uint32_t fR_6c8608 = 0;
    uint32_t fA_6c8610 = 0;
    uint32_t fB_6c8610 = 0;
    uint32_t fK_6c8610 = 0;
    uint32_t fR_6c8615 = 0;
    uint32_t fA_6c8622 = 0;
    uint32_t fB_6c8622 = 0;
    esp = esp - 0x68u;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp + 0x64u, eax);
    eax = rd32(m, esp + 0x6cu);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, 0x63u);
    esp -= 4; wr32(m, esp, eax);
    eax = (esp + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    ebx = 0u;
    LIFT_CALL(rd32(m, 0x916260u) /* 8.286626112932593e-39 */, 0x6c8595u);
    esp = esp + 0xcu;
    wr8(m, esp + 0x6fu, (ebx & 0xffu));
    eax = (esp + 0xcu);
    fA_6c85a0 = rd8(m, esp + 0xcu);
    fB_6c85a0 = (ebx & 0xffu);
    if (fA_6c85a0 == fB_6c85a0) goto L_6c85c0;
    goto L_6c85b0;
L_6c85b0:
    ecx = rd8(m, eax);
    edx = (edx & 0xffffff00u) | ((rd8(m, ecx + 0x9b77c8u)) & 0xffu);
    wr8(m, eax, (edx & 0xffu));
    eax = (eax + 1u);
    fA_6c85bc = rd8(m, eax);
    fB_6c85bc = (ebx & 0xffu);
    if (fA_6c85bc != fB_6c85bc) goto L_6c85b0;
L_6c85c0:
    ebp = rd32(m, 0xd5aad8u);
    edi = 0u;
    fR_6c85c8 = (ebp & ebp);
    if (((fR_6c85c8 == 0) || sgn(fR_6c85c8, 4) != false)) goto L_6c8627;
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, 0xd5aad4u);
L_6c85d3:
    fR_6c85d3 = (ebx & ebx);
    if (!(fR_6c85d3 == 0)) goto L_6c8626;
    eax = rd32(m, esp + 0x80u);
    fA_6c85de = rd32(m, esi + 0x64u);
    fB_6c85de = eax;
    if (fA_6c85de != fB_6c85de) goto L_6c861b;
    ecx = esi;
    eax = (esp + 0x10u);
L_6c85f0:
    edx = (edx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    fA_6c85f2 = (edx & 0xffu);
    fB_6c85f2 = rd8(m, ecx);
    FL = fl_sub(fA_6c85f2, fB_6c85f2, 1);
    if (fA_6c85f2 != fB_6c85f2) goto L_6c8610;
    fR_6c85f6 = ((edx & 0xffu) & (edx & 0xffu));
    if ((fR_6c85f6 == 0)) goto L_6c860c;
    edx = (edx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    fA_6c85fd = (edx & 0xffu);
    fB_6c85fd = rd8(m, ecx + 0x1u);
    FL = fl_sub(fA_6c85fd, fB_6c85fd, 1);
    if (fA_6c85fd != fB_6c85fd) goto L_6c8610;
    eax = eax + 0x2u;
    ecx = ecx + 0x2u;
    fR_6c8608 = ((edx & 0xffu) & (edx & 0xffu));
    if (!(fR_6c8608 == 0)) goto L_6c85f0;
L_6c860c:
    eax = 0u;
    goto L_6c8615;
L_6c8610:
    fA_6c8610 = eax; fB_6c8610 = eax; fK_6c8610 = (fl_cc(2, FL)) ? 1u : 0u;
    eax = fA_6c8610 - fB_6c8610 - fK_6c8610;
    eax = eax - 0xffffffffu - ((uint64_t(fA_6c8610) < uint64_t(fB_6c8610) + uint64_t(fK_6c8610)) ? 1u : 0u);
L_6c8615:
    fR_6c8615 = (eax & eax);
    if (!(fR_6c8615 == 0)) goto L_6c861b;
    ebx = esi;
L_6c861b:
    edi = (edi + 1u);
    esi = esi + 0xc0u;
    fA_6c8622 = edi;
    fB_6c8622 = ebp;
    if (int32_t(sext(fA_6c8622, 4)) < int32_t(sext(fB_6c8622, 4))) goto L_6c85d3;
L_6c8626:
    esi = rd32(m, esp); esp += 4;
L_6c8627:
    ecx = rd32(m, esp + 0x70u);
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = ebx;
    ebx = rd32(m, esp); esp += 4;
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x6c8637u);  // __security_check_cookie
    esp = esp + 0x68u;
    LIFT_RET(0);
}

// 006c9100 Fn_006c9100
//   original FUN_006c9100; register inputs: eax; returns with ret 0
REFLEX_PORT_FN(0x006c9100, Fn_006c9100, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    x0 = X{};
    ecx = 0u;
    wr32(m, eax + 0x8u, ecx);
    wr32(m, eax + 0x1cu, ecx);
    wr32(m, eax + 0x20u, ecx);
    wr32(m, eax, 0x1u);
    wr32(m, eax + 0x88u, ecx);
    wr32(m, eax + 0x84u, ecx);
    wr32(m, eax + 0x64u, ecx);
    wr32(m, eax + 0x68u, ecx);
    wr32(m, eax + 0x18u, ecx);
    wr32(m, eax + 0x90u, ecx);
    wr32(m, eax + 0x14u, x0.l(0));
    wr32(m, eax + 0xcu, ecx);
    wr32(m, eax + 0x6cu, x0.l(0));
    wr32(m, eax + 0x5cu, 0x1u);
    wr32(m, eax + 0x38u, 0xffffffffu);
    wr32(m, eax + 0x30u, ecx);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x48u)));
    wr32(m, eax + 0x44u, fst32(m));
    fpop(m);
    wr32(m, eax + 0x3cu, ecx);
    wr32(m, eax + 0x40u, x0.l(0));
    wr32(m, eax + 0x4cu, ecx);
    edx = (edx & 0xffffff00u) | ((0x4eu) & 0xffu);
    wr8(m, eax + 0x34u, (edx & 0xffu));
    wr8(m, eax + 0x35u, (edx & 0xffu));
    wr32(m, eax + 0x60u, ecx);
    wr32(m, eax + 0x24u, ecx);
    wr32(m, eax + 0x70u, x0.l(0));
    wr32(m, eax + 0x74u, x0.l(0));
    wr32(m, eax + 0x78u, x0.l(0));
    wr32(m, eax + 0x7cu, x0.l(0));
    wr32(m, eax + 0x28u, ecx);
    LIFT_RET(0);
}

// 006c9190 Fn_006c9190
//   original FUN_006c9190; register inputs: esi; returns with ret 0
REFLEX_PORT_FN(0x006c9190, Fn_006c9190, "fx", reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fA_6c919c = 0;
    uint32_t fB_6c919c = 0;
    uint32_t fA_6c91aa = 0;
    uint32_t fB_6c91aa = 0;
    uint32_t fA_6c91c1 = 0;
    uint32_t fB_6c91c1 = 0;
    uint32_t fA_6c91c6 = 0;
    uint32_t fB_6c91c6 = 0;
    uint32_t fA_6c91dc = 0;
    uint32_t fB_6c91dc = 0;
    uint32_t fA_6c91e3 = 0;
    uint32_t fB_6c91e3 = 0;
    uint32_t fA_6c91e8 = 0;
    uint32_t fB_6c91e8 = 0;
    uint32_t fA_6c91ed = 0;
    uint32_t fB_6c91ed = 0;
    eax = rd32(m, esi + 0x4u);
    ecx = rd32(m, eax + 0x1acu);
    esp -= 4; wr32(m, esp, edi);
    edi = 0u;
    fA_6c919c = rd32(m, ecx + 0x280u);
    fB_6c919c = edi;
    if (fA_6c919c != fB_6c919c) goto L_6c91b7;
    edx = rd32(m, eax + 0x1b4u);
    fA_6c91aa = rd32(m, edx + 0x5b57cu);
    fB_6c91aa = 0xcu;
    if (fA_6c91aa == fB_6c91aa) goto L_6c923f;
L_6c91b7:
    wr32(m, esi + 0x90u, 0x1u);
    fA_6c91c1 = rd32(m, esi + 0x68u);
    fB_6c91c1 = edi;
    if (fA_6c91c1 != fB_6c91c1) goto L_6c91cb;
    fA_6c91c6 = rd32(m, esi + 0x1cu);
    fB_6c91c6 = edi;
    if (fA_6c91c6 == fB_6c91c6) goto L_6c923f;
L_6c91cb:
    ecx = rd32(m, esi + 0x1cu);
    esp -= 4; wr32(m, esp, eax);
    edx = 0x960764u;
    LIFT_CALL(0x6cd750u, 0x6c91d9u);  // FUN_006cd750
    ecx = rd32(m, esi + 0x1cu);
    fA_6c91dc = ecx;
    fB_6c91dc = edi;
    if (fA_6c91dc == fB_6c91dc) goto L_6c9203;
    eax = rd32(m, esi + 0x4u);
    fA_6c91e3 = rd32(m, eax + 0x4u);
    fB_6c91e3 = edi;
    if (fA_6c91e3 == fB_6c91e3) goto L_6c9203;
    fA_6c91e8 = rd32(m, eax + 0x10u);
    fB_6c91e8 = edi;
    if (fA_6c91e8 != fB_6c91e8) goto L_6c9203;
    fA_6c91ed = rd32(m, ecx + 0x64u);
    fB_6c91ed = edi;
    if (fA_6c91ed == fB_6c91ed) goto L_6c9203;
    eax = rd32(m, eax + 0x1f0u);
    ecx = rd32(m, 0xa946c0u);
    LIFT_CALL(0x764f80u, 0x6c9203u);  // MXRecords_FailedJump
L_6c9203:
    x0 = X{};
    wr32(m, esi + 0x1cu, edi);
    wr32(m, esi + 0x8u, edi);
    wr32(m, esi + 0x5cu, 0x1u);
    wr32(m, esi + 0x38u, 0xffffffffu);
    wr32(m, esi + 0x30u, edi);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x48u)));
    eax = (eax & 0xffffff00u) | ((0x4eu) & 0xffu);
    wr32(m, esi + 0x44u, fst32(m));
    fpop(m);
    wr8(m, esi + 0x34u, (eax & 0xffu));
    wr8(m, esi + 0x35u, (eax & 0xffu));
    wr32(m, esi + 0x3cu, edi);
    wr32(m, esi + 0x40u, x0.l(0));
    wr32(m, esi + 0x4cu, edi);
    wr32(m, esi + 0xcu, edi);
    wr32(m, esi + 0x68u, edi);
    wr32(m, esi + 0x64u, edi);
L_6c923f:
    edi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 006c9250 Fn_006c9250
//   original FUN_006c9250; register inputs: ecx esi; returns with ret 0
REFLEX_PORT_FN(0x006c9250, Fn_006c9250, "fx", reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_6c9251 = 0;
    uint32_t fB_6c9251 = 0;
    uint32_t fA_6c925d = 0;
    uint32_t fB_6c925d = 0;
    uint32_t fA_6c928c = 0;
    uint32_t fB_6c928c = 0;
    FCmp fC_6c92b3{};
    uint32_t fR_6c92c9 = 0;
    uint32_t fA_6c9320 = 0;
    uint32_t fB_6c9320 = 0;
    esp -= 4; wr32(m, esp, ecx);
    fA_6c9251 = rd32(m, esi + 0x1cu);
    fB_6c9251 = 0x0u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, edi);
    if (fA_6c9251 == fB_6c9251) goto L_6c9340;
    fA_6c925d = rd32(m, esi + 0x8u);
    fB_6c925d = 0x0u;
    if (fA_6c925d == fB_6c925d) goto L_6c9370;
    eax = rd32(m, esi + 0x20u);
    wr32(m, eax + 0x4cu, 0x1u);
    eax = rd32(m, eax + 0x44u);
    x0 = movss_m(rd32(m, eax + 0x30u));
    ecx = rd32(m, esi + 0x20u);
    wr32(m, ecx + 0x4cu, 0x0u);
    edx = rd32(m, esi + 0x4u);
    eax = rd32(m, edx + 0x1b4u);
    fA_6c928c = rd32(m, eax + 0xaf10u);
    fB_6c928c = 0x3u;
    wr32(m, esp + 0x8u, x0.l(0));
    if (fA_6c928c != fB_6c928c) goto L_6c92a2;
    eax = eax + 0x48cu;
    goto L_6c92a7;
L_6c92a2:
    eax = eax + 0x46cu;
L_6c92a7:
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x63b8f0u, 0x6c92adu);  // FUN_0063b8f0
    x1 = movss_m(rd32(m, esp + 0x8u));
    fC_6c92b3 = fcmp32(x1.l(0), x0.l(0));
    if (!(fc_b(fC_6c92b3) || fc_z(fC_6c92b3))) goto L_6c9320;
    eax = rd32(m, esi + 0x1cu);
    eax = eax + 0x44u;
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x6c8570u, 0x6c92c6u);  // FUN_006c8570
    esp = esp + 0x8u;
    fR_6c92c9 = (eax & eax);
    if ((fR_6c92c9 == 0)) goto L_6c92ec;
    ecx = rd32(m, esi + 0x4u);
    x0 = X{};
    esp -= 4; wr32(m, esp, ecx);
    ecx = eax;
    edx = 0x960758u;
    wr32(m, esi + 0x1cu, eax);
    wr32(m, esi + 0x14u, x0.l(0));
    LIFT_CALL(0x6cd750u, 0x6c92e8u);  // FUN_006cd750
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_6c92ec:
    edx = rd32(m, esi + 0x4u);
    ebx = rd32(m, 0xa94304u);
    esp -= 4; wr32(m, esp, 0x9e1a7cu);
    ecx = 0x9cbeb4u;
    wr32(m, esp + 0xcu, edx);
    LIFT_CALL(rd32(m, 0x916388u) /* 8.287789190657982e-39 */, 0x6c9309u);
    esp -= 4; wr32(m, esp, 0x4u);
    eax = (esp + 0xcu);
    esp -= 4; wr32(m, esp, eax);
    edi = 0x9407c4u;
    eax = ebx;
    LIFT_CALL(0x4020c0u, 0x6c931cu);  // FUN_004020c0
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_6c9320:
    fA_6c9320 = rd32(m, 0xa94308u);
    fB_6c9320 = 0x0u;
    if (fA_6c9320 != fB_6c9320) goto L_6c9370;
    ecx = rd32(m, esi + 0x4u);
    edx = rd32(m, ecx + 0x1b4u);
    wr32(m, edx + 0x5b568u, 0x17u);
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_6c9340:
    eax = rd32(m, esi + 0x4u);
    ebx = rd32(m, 0xa94304u);
    esp -= 4; wr32(m, esp, 0x9e1a7cu);
    ecx = 0x9cbeb4u;
    wr32(m, esp + 0xcu, eax);
    LIFT_CALL(rd32(m, 0x916388u) /* 8.287789190657982e-39 */, 0x6c935du);
    esp -= 4; wr32(m, esp, 0x4u);
    ecx = (esp + 0xcu);
    esp -= 4; wr32(m, esp, ecx);
    edi = 0x9407c4u;
    eax = ebx;
    LIFT_CALL(0x4020c0u, 0x6c9370u);  // FUN_004020c0
L_6c9370:
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 006c9380 Fn_006c9380
//   original FUN_006c9380; register inputs: ecx eax; returns with ret 4
REFLEX_PORT_FN(0x006c9380, Fn_006c9380, "fx", 0) {
    LIFT_ENTRY();
    uint32_t fA_6c938a = 0;
    uint32_t fB_6c938a = 0;
    uint32_t fA_6c9393 = 0;
    uint32_t fB_6c9393 = 0;
    uint32_t fA_6c9398 = 0;
    uint32_t fB_6c9398 = 0;
    uint32_t fR_6c93b4 = 0;
    uint32_t fR_6c93e9 = 0;
    uint32_t fA_6c93ed = 0;
    uint32_t fB_6c93ed = 0;
    FCmp fC_6c93f3{};
    uint32_t fR_6c940f = 0;
    uint32_t fR_6c9445 = 0;
    uint32_t fA_6c9449 = 0;
    uint32_t fB_6c9449 = 0;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    eax = rd32(m, esi + 0x84u);
    fA_6c938a = eax;
    fB_6c938a = 0x0u;
    eax = fA_6c938a - fB_6c938a;
    if (fA_6c938a == fB_6c938a) goto L_6c9432;
    fA_6c9393 = eax;
    fB_6c9393 = 0x1u;
    eax = fA_6c9393 - fB_6c9393;
    if (fA_6c9393 == fB_6c9393) goto L_6c93d0;
    fA_6c9398 = eax;
    fB_6c9398 = 0x1u;
    eax = fA_6c9398 - fB_6c9398;
    if (fA_6c9398 != fB_6c9398) goto L_6c947b;
    eax = rd32(m, esi + 0x4u);
    ecx = rd32(m, eax + 0x1acu);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0xacu);
    LIFT_CALL(eax, 0x6c93b4u);
    fR_6c93b4 = (eax & eax);
    if (!(fR_6c93b4 == 0)) goto L_6c947b;
    LIFT_CALL(0x6c9250u, 0x6c93c1u);  // FUN_006c9250
    wr32(m, esi + 0x84u, 0x0u);
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_6c93d0:
    x0 = movss_m(rd32(m, esi + 0x8cu));
    eax = rd32(m, esi + 0x1cu);
    x0 = addss(x0, rd32(m, esp + 0xcu));
    wr32(m, esi + 0x8cu, x0.l(0));
    fR_6c93e9 = (eax & eax);
    if ((fR_6c93e9 == 0)) goto L_6c93f3;
    fA_6c93ed = rd32(m, eax + 0x64u);
    fB_6c93ed = 0x0u;
    if (fA_6c93ed != fB_6c93ed) goto L_6c941e;
L_6c93f3:
    fC_6c93f3 = fcmp32(x0.l(0), rd32(m, 0x92ff84u) /* 0.25 */);
    if (!(fc_b(fC_6c93f3) || fc_z(fC_6c93f3))) goto L_6c941e;
    ecx = rd32(m, esi + 0x4u);
    ecx = rd32(m, ecx + 0x1acu);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0xacu);
    LIFT_CALL(eax, 0x6c940fu);
    fR_6c940f = (eax & eax);
    if (!(fR_6c940f == 0)) goto L_6c947b;
    wr32(m, esi + 0x84u, eax);
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_6c941e:
    LIFT_CALL(0x6c9480u, 0x6c9423u);  // FUN_006c9480
    wr32(m, esi + 0x84u, 0x2u);
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_6c9432:
    ecx = rd32(m, esi + 0x4u);
    ecx = rd32(m, ecx + 0x1acu);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0xacu);
    LIFT_CALL(eax, 0x6c9445u);
    fR_6c9445 = (eax & eax);
    if ((fR_6c9445 == 0)) goto L_6c947b;
    fA_6c9449 = rd32(m, esi + 0x88u);
    fB_6c9449 = 0x0u;
    if (fA_6c9449 != fB_6c9449) goto L_6c947b;
    ecx = rd32(m, esi + 0x4u);
    x0 = X{};
    wr32(m, esi + 0x8cu, x0.l(0));
    eax = rd32(m, ecx + 0x1f0u);
    ecx = rd32(m, 0xa946c0u);
    LIFT_CALL(0x764f40u, 0x6c9471u);  // MXRecords_AttemptedJump
    wr32(m, esi + 0x84u, 0x1u);
L_6c947b:
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 006c9480 Fn_006c9480
//   original FUN_006c9480; register inputs: ecx esi; returns with ret 0
REFLEX_PORT_FN(0x006c9480, Fn_006c9480, "fx", reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fR_6c9486 = 0;
    uint32_t fA_6c948a = 0;
    uint32_t fB_6c948a = 0;
    uint32_t fR_6c949e = 0;
    esp -= 4; wr32(m, esp, ecx);
    eax = rd32(m, esi + 0x1cu);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, edi);
    fR_6c9486 = (eax & eax);
    if ((fR_6c9486 == 0)) goto L_6c94d0;
    fA_6c948a = rd32(m, eax + 0x64u);
    fB_6c948a = 0x0u;
    if (fA_6c948a != fB_6c948a) goto L_6c950b;
    eax = eax + 0x44u;
    esp -= 4; wr32(m, esp, 0x1u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x6c8570u, 0x6c949bu);  // FUN_006c8570
    esp = esp + 0x8u;
    fR_6c949e = (eax & eax);
    if ((fR_6c949e == 0)) goto L_6c94b9;
    ecx = rd32(m, esi + 0x4u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = eax;
    edx = 0x960748u;
    wr32(m, esi + 0x1cu, eax);
    LIFT_CALL(0x6cd750u, 0x6c94b5u);  // FUN_006cd750
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_6c94b9:
    edx = rd32(m, esi + 0x4u);
    eax = rd32(m, edx + 0x1b4u);
    wr32(m, eax + 0x5b568u, 0x2u);
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_6c94d0:
    ecx = rd32(m, esi + 0x4u);
    ebx = rd32(m, 0xa94304u);
    wr32(m, esp + 0x8u, ecx);
    esp -= 4; wr32(m, esp, 0x9e1a7cu);
    ecx = 0x9cbeb4u;
    LIFT_CALL(rd32(m, 0x916388u) /* 8.287789190657982e-39 */, 0x6c94edu);
    esp -= 4; wr32(m, esp, 0x4u);
    edx = (esp + 0xcu);
    esp -= 4; wr32(m, esp, edx);
    edi = 0x960738u;
    eax = ebx;
    LIFT_CALL(0x4020c0u, 0x6c9500u);  // FUN_004020c0
    x0 = X{};
    wr32(m, esi + 0x80u, x0.l(0));
L_6c950b:
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 006c9510 Fn_006c9510
//   original FUN_006c9510; register inputs: none; returns with ret 8
REFLEX_PORT_FN(0x006c9510, Fn_006c9510, "fx", 0) {
    LIFT_ENTRY();
    uint32_t fA_6c951b = 0;
    uint32_t fB_6c951b = 0;
    uint32_t fA_6c9523 = 0;
    uint32_t fB_6c9523 = 0;
    uint32_t fA_6c954b = 0;
    uint32_t fB_6c954b = 0;
    uint32_t fA_6c9559 = 0;
    uint32_t fB_6c9559 = 0;
    uint32_t fA_6c955d = 0;
    uint32_t fB_6c955d = 0;
    uint32_t fA_6c95b4 = 0;
    uint32_t fB_6c95b4 = 0;
    uint32_t fA_6c95b8 = 0;
    uint32_t fB_6c95b8 = 0;
    FCmp fC_6c95c5{};
    uint32_t fA_6c9628 = 0;
    uint32_t fB_6c9628 = 0;
    FCmp fC_6c9636{};
    uint32_t fA_6c963f = 0;
    uint32_t fB_6c963f = 0;
    uint32_t fR_6c9673 = 0;
    uint32_t fR_6c9699 = 0;
    uint32_t fR_6c96ba = 0;
    FCmp fC_6c970c{};
    FCmp fC_6c9713{};
    uint32_t fA_6c971b = 0;
    uint32_t fB_6c971b = 0;
    FCmp fC_6c972f{};
    uint32_t fR_6c9733 = 0;
    esp = esp - 0x10u;
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x18u);
    eax = rd32(m, ebp + 0x4u);
    fA_6c951b = rd32(m, eax + 0x4u);
    fB_6c951b = 0x0u;
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    if (fA_6c951b == fB_6c951b) goto L_6c9529;
    fA_6c9523 = rd32(m, eax + 0x10u);
    fB_6c9523 = 0x0u;
    if (fA_6c9523 == fB_6c9523) goto L_6c9538;
L_6c9529:
    eax = rd32(m, ebp + 0x24u);
    wr32(m, ebp + 0x60u, eax);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    esp = esp + 0x10u;
    LIFT_RET(8);
L_6c9538:
    esi = rd32(m, eax + 0x1f0u);
    LIFT_CALL(0x548660u, 0x6c9543u);  // FUN_00548660
    eax = rd32(m, ebp + 0x5cu);
    edi = 0x1u;
    fA_6c954b = eax;
    fB_6c954b = edi;
    eax = fA_6c954b - fB_6c954b;
    wr32(m, esp + 0x20u, x0.l(0));
    if (fA_6c954b == fB_6c954b) goto L_6c972c;
    fA_6c9559 = eax;
    fB_6c9559 = edi;
    eax = fA_6c9559 - fB_6c9559;
    if (fA_6c9559 == fB_6c9559) goto L_6c95dc;
    fA_6c955d = eax;
    fB_6c955d = edi;
    eax = fA_6c955d - fB_6c955d;
    if (fA_6c955d != fB_6c955d) goto L_6c9767;
    edx = rd32(m, ebp + 0x4u);
    eax = rd32(m, edx + 0x1f0u);
    ecx = (esp + 0x10u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x548810u, 0x6c9578u);  // FUN_00548810
    eax = rd32(m, ebp + 0x4u);
    ecx = rd32(m, eax + 0x1acu);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0xacu);
    esp = esp + 0x4u;
    LIFT_CALL(eax, 0x6c958eu);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x14u)));
    esp -= 4; wr32(m, esp, eax);
    esp = esp - 0xcu;
    wr32(m, esp + 0x8u, fst32(m));
    fpop(m);
    esi = (ebp + 0x30u);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x20u)));
    wr32(m, esp + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x34u)));
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x6cd2c0u, 0x6c95b1u);  // FUN_006cd2c0
    eax = (eax & 0xffffff00u) | ((rd8(m, ebp + 0x34u)) & 0xffu);
    fA_6c95b4 = (eax & 0xffu);
    fB_6c95b4 = 0x4eu;
    if (fA_6c95b4 == fB_6c95b4) goto L_6c95d0;
    fA_6c95b8 = (eax & 0xffu);
    fB_6c95b8 = rd8(m, ebp + 0x35u);
    if (fA_6c95b8 != fB_6c95b8) goto L_6c95d0;
L_6c95bd:
    x0 = movss_m(rd32(m, 0x931d94u) /* 0.10000000149011612 */);
    fC_6c95c5 = fcmp32(x0.l(0), rd32(m, esp + 0x20u));
    if ((fc_b(fC_6c95c5) || fc_z(fC_6c95c5))) goto L_6c9767;
L_6c95d0:
    wr32(m, ebp + 0x5cu, edi);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    esp = esp + 0x10u;
    LIFT_RET(8);
L_6c95dc:
    edx = rd32(m, ebp + 0x4u);
    eax = rd32(m, edx + 0x1f0u);
    ecx = (esp + 0x10u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x548810u, 0x6c95efu);  // FUN_00548810
    eax = rd32(m, ebp + 0x4u);
    ecx = rd32(m, eax + 0x1acu);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0xacu);
    esp = esp + 0x4u;
    esi = (ebp + 0x30u);
    LIFT_CALL(eax, 0x6c9608u);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x14u)));
    esp -= 4; wr32(m, esp, eax);
    esp = esp - 0xcu;
    wr32(m, esp + 0x8u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x20u)));
    wr32(m, esp + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x34u)));
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x6cd2c0u, 0x6c9628u);  // FUN_006cd2c0
    fA_6c9628 = rd32(m, ebp + 0x3cu);
    fB_6c9628 = 0x0u;
    if (fA_6c9628 != fB_6c9628) goto L_6c95bd;
    x0 = movss_m(rd32(m, ebp + 0x44u));
    x1 = X{};
    fC_6c9636 = fcmp32(x0.l(0), x1.l(0));
    if ((fc_b(fC_6c9636) || fc_z(fC_6c9636))) goto L_6c96f4;
    fA_6c963f = rd32(m, ebp + 0x4cu);
    fB_6c963f = 0x0u;
    if (fA_6c963f == fB_6c963f) goto L_6c96f4;
    ecx = rd32(m, ebp + 0x4u);
    ecx = rd32(m, ecx + 0x1acu);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0xacu);
    LIFT_CALL(eax, 0x6c965cu);
    esp -= 4; wr32(m, esp, eax);
    edi = esi;
    LIFT_CALL(0x6c9aa0u, 0x6c9664u);  // FUN_006c9aa0
    edi = eax;
    eax = esi;
    edx = (eax + 0x1u);
    goto L_6c9670;
L_6c9670:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    eax = (eax + 1u);
    fR_6c9673 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_6c9673 == 0)) goto L_6c9670;
    ecx = rd32(m, ebp + 0x94u);
    eax = eax - edx;
    esp -= 4; wr32(m, esp, eax);
    edx = (ebp + ecx * 8 + 0x98u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x8fa070u, 0x6c968eu);  // MSVCR90.DLL::memcpy
    ecx = rd32(m, ebp + 0x94u);
    eax = 0u;
    esp = esp + 0xcu;
    fR_6c9699 = (edi & edi);
    eax = (eax & 0xffffff00u) | (((!(fR_6c9699 == 0)) ? 1u : 0u) & 0xffu);
    wr32(m, ebp + ecx * 8 + 0x9cu, eax);
    eax = rd32(m, ebp + 0x94u);
    eax = (eax + 1u);
    edx = (eax >> 31) ? 0xffffffffu : 0;
    ecx = 0xfu;
    { uint32_t d_ = ecx; sdiv32(eax, edx, d_); }
    wr32(m, ebp + 0x94u, edx);
    fR_6c96ba = (edi & edi);
    if ((fR_6c96ba == 0)) goto L_6c96de;
    edx = rd32(m, ebp + 0x4u);
    ecx = rd32(m, edx + 0x1acu);
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0xacu);
    LIFT_CALL(edx, 0x6c96d1u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x6c8570u, 0x6c96d8u);  // FUN_006c8570
    esp = esp + 0x8u;
    wr32(m, ebp + 0x60u, eax);
L_6c96de:
    eax = (eax & 0xffffff00u) | ((rd8(m, ebp + 0x34u)) & 0xffu);
    wr8(m, ebp + 0x58u, (eax & 0xffu));
    wr32(m, ebp + 0x5cu, 0x3u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    esp = esp + 0x10u;
    LIFT_RET(8);
L_6c96f4:
    x2 = movss_m(rd32(m, ebp + 0x2cu));
    x2 = addss(x2, rd32(m, esp + 0x24u));
    wr32(m, ebp + 0x2cu, x2.l(0));
    x2 = movss_m(rd32(m, 0x931d94u) /* 0.10000000149011612 */);
    fC_6c970c = fcmp32(x2.l(0), rd32(m, esp + 0x20u));
    if (!(fc_b(fC_6c970c) || fc_z(fC_6c970c))) goto L_6c9718;
    fC_6c9713 = fcmp32(x1.l(0), x0.l(0));
    if (fc_b(fC_6c9713)) goto L_6c9767;
L_6c9718:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, ebp + 0x34u)) & 0xffu);
    fA_6c971b = (ecx & 0xffu);
    fB_6c971b = rd8(m, ebp + 0x35u);
    if (fA_6c971b == fB_6c971b) goto L_6c9767;
    wr32(m, ebp + 0x5cu, edi);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    esp = esp + 0x10u;
    LIFT_RET(8);
L_6c972c:
    x1 = X{};
    fC_6c972f = fcmp32(x0.l(0), x1.l(0));
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_6c972f) | 2u) << 8);
    fR_6c9733 = (((eax >> 8) & 0xffu) & 0x44u);
    if (!parity8(fR_6c9733)) goto L_6c9767;
    wr32(m, ebp + 0x5cu, 0x2u);
    wr32(m, ebp + 0x38u, 0x0u);
    wr32(m, ebp + 0x30u, 0x0u);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x48u)));
    wr32(m, ebp + 0x44u, fst32(m));
    fpop(m);
    wr32(m, ebp + 0x40u, x1.l(0));
    wr32(m, ebp + 0x4cu, 0x0u);
    wr32(m, ebp + 0x3cu, edi);
    wr32(m, ebp + 0x2cu, x1.l(0));
L_6c9767:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    esp = esp + 0x10u;
    LIFT_RET(8);
}

// 006c9770 Fn_006c9770
//   original FUN_006c9770; register inputs: ecx eax; returns with ret 4
REFLEX_PORT_FN(0x006c9770, Fn_006c9770, "fx", reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fA_6c9777 = 0;
    uint32_t fB_6c9777 = 0;
    uint32_t fA_6c9784 = 0;
    uint32_t fB_6c9784 = 0;
    uint32_t fA_6c9791 = 0;
    uint32_t fB_6c9791 = 0;
    uint32_t fR_6c97b0 = 0;
    uint32_t fA_6c97c5 = 0;
    uint32_t fB_6c97c5 = 0;
    uint32_t fA_6c97d7 = 0;
    uint32_t fB_6c97d7 = 0;
    uint32_t fR_6c97ec = 0;
    uint32_t fA_6c97f6 = 0;
    uint32_t fB_6c97f6 = 0;
    uint32_t fA_6c9803 = 0;
    uint32_t fB_6c9803 = 0;
    uint32_t fA_6c9815 = 0;
    uint32_t fB_6c9815 = 0;
    uint32_t fA_6c9861 = 0;
    uint32_t fB_6c9861 = 0;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    eax = rd32(m, esi + 0x4u);
    fA_6c9777 = rd32(m, eax + 0x1acu);
    fB_6c9777 = 0x0u;
    if (fA_6c9777 == fB_6c9777) goto L_6c98a4;
    fA_6c9784 = rd32(m, eax + 0x1b4u);
    fB_6c9784 = 0x0u;
    if (fA_6c9784 == fB_6c9784) goto L_6c98a4;
    fA_6c9791 = rd32(m, 0xd5ab38u);
    fB_6c9791 = 0x0u;
    if (fA_6c9791 == fB_6c9791) goto L_6c98a4;
    eax = rd32(m, eax + 0x1acu);
    edx = rd32(m, eax);
    ecx = eax;
    eax = rd32(m, edx + 0xacu);
    LIFT_CALL(eax, 0x6c97b0u);
    fR_6c97b0 = (eax & eax);
    if (!(fR_6c97b0 == 0)) goto L_6c97b7;
    wr32(m, esi + 0x28u, eax);
L_6c97b7:
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0xcu)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x6c9510u, 0x6c97c5u);  // FUN_006c9510
    fA_6c97c5 = rd32(m, esi + 0x90u);
    fB_6c97c5 = 0x0u;
    if (fA_6c97c5 == fB_6c97c5) goto L_6c9803;
    eax = rd32(m, esi + 0x4u);
    ecx = rd32(m, eax + 0x1acu);
    fA_6c97d7 = rd32(m, ecx + 0x280u);
    fB_6c97d7 = 0x0u;
    if (fA_6c97d7 != fB_6c97d7) goto L_6c97f6;
    eax = ecx;
    edx = rd32(m, eax);
    eax = rd32(m, edx + 0xacu);
    LIFT_CALL(eax, 0x6c97ecu);
    fR_6c97ec = (eax & eax);
    if (!(fR_6c97ec == 0)) goto L_6c97f6;
    wr32(m, esi + 0x90u, eax);
L_6c97f6:
    fA_6c97f6 = rd32(m, esi + 0x90u);
    fB_6c97f6 = 0x0u;
    if (fA_6c97f6 != fB_6c97f6) goto L_6c98a4;
L_6c9803:
    fA_6c9803 = rd32(m, esi);
    fB_6c9803 = 0x0u;
    if (fA_6c9803 == fB_6c9803) goto L_6c98a4;
    ecx = rd32(m, esi + 0x4u);
    edx = rd32(m, ecx + 0x1acu);
    fA_6c9815 = rd32(m, edx + 0x194u);
    fB_6c9815 = 0x1u;
    if (fA_6c9815 > fB_6c9815) goto L_6c98a4;
    LIFT_CALL(0x6c9190u, 0x6c9827u);  // FUN_006c9190
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0xcu)));
    esp -= 4; wr32(m, esp, ecx);
    eax = esi;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x6c9380u, 0x6c9836u);  // FUN_006c9380
    eax = rd32(m, esi + 0x4u);
    ecx = rd32(m, eax + 0x1acu);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0xacu);
    LIFT_CALL(eax, 0x6c9849u);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0xcu)));
    wr32(m, esi + 0x88u, eax);
    esp -= 4; wr32(m, esp, ecx);
    eax = esi;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x6ca0c0u, 0x6c985eu);  // FUN_006ca0c0
    eax = rd32(m, esi + 0x8u);
    fA_6c9861 = eax;
    fB_6c9861 = 0x3u;
    if (fA_6c9861 > fB_6c9861) goto L_6c98a4;
    switch (eax) { case 0: goto L_6c986d; case 1: goto L_6c9879; case 2: goto L_6c988d; case 3: goto L_6c989f; default: throw std::runtime_error("jump table"); }
L_6c986d:
    eax = esi;
    LIFT_CALL(0x6c9b00u, 0x6c9874u);  // FUN_006c9b00
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_6c9879:
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0xcu)));
    esp -= 4; wr32(m, esp, ecx);
    eax = esi;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x6c9b60u, 0x6c9888u);  // FUN_006c9b60
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_6c988d:
    x0 = movss_m(rd32(m, esp + 0xcu));
    eax = esi;
    LIFT_CALL(0x6c9cc0u, 0x6c989au);  // FUN_006c9cc0
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_6c989f:
    LIFT_CALL(0x6c9da0u, 0x6c98a4u);  // FUN_006c9da0
L_6c98a4:
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 006c9940 Fn_006c9940
//   original FUN_006c9940; register inputs: eax; returns with ret 0
REFLEX_PORT_FN(0x006c9940, Fn_006c9940, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fA_6c9951 = 0;
    uint32_t fB_6c9951 = 0;
    uint32_t fA_6c9975 = 0;
    uint32_t fB_6c9975 = 0;
    uint32_t fA_6c997a = 0;
    uint32_t fB_6c997a = 0;
    FCmp fC_6c9987{};
    uint32_t fR_6c99a2 = 0;
    uint32_t fR_6c99b5 = 0;
    FCmp fC_6c99c5{};
    uint32_t fR_6c99c9 = 0;
    uint32_t fR_6c99d4 = 0;
    FCmp fC_6c99dc{};
    uint32_t fR_6c99e0 = 0;
    uint32_t fA_6c99ea = 0;
    uint32_t fB_6c99ea = 0;
    uint32_t fA_6c99f8 = 0;
    uint32_t fB_6c99f8 = 0;
    uint32_t fA_6c9a06 = 0;
    uint32_t fB_6c9a06 = 0;
    uint32_t fA_6c9a0e = 0;
    uint32_t fB_6c9a0e = 0;
    uint32_t fR_6c9a17 = 0;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    eax = rd32(m, 0xa946b8u);
    ecx = rd32(m, eax + 0x745cu);
    ebx = 0u;
    fA_6c9951 = rd32(m, ecx + 0x288u);
    fB_6c9951 = ebx;
    if (fA_6c9951 != fB_6c9951) goto L_6c9a30;
    edx = rd32(m, esi + 0x4u);
    ecx = rd32(m, edx + 0x1acu);
    eax = rd32(m, ecx + 0x234u);
    eax = rd32(m, eax + 0x2de4u);
    esp -= 4; wr32(m, esp, edi);
    edi = 0u;
    fA_6c9975 = eax;
    fB_6c9975 = 0x4u;
    if (fA_6c9975 == fB_6c9975) goto L_6c99e5;
    fA_6c997a = eax;
    fB_6c997a = 0x3u;
    if (fA_6c997a == fB_6c997a) goto L_6c99e5;
    x0 = movss_m(rd32(m, ecx + 0x150u));
    fC_6c9987 = fcmp32(x0.l(0), rd32(m, 0x92fd88u) /* 10.0 */);
    if (fc_b(fC_6c9987)) goto L_6c99ea;
    ecx = edx;
    ecx = rd32(m, ecx + 0x1acu);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0xacu);
    LIFT_CALL(eax, 0x6c99a2u);
    fR_6c99a2 = (eax & eax);
    if ((fR_6c99a2 == 0)) goto L_6c99ea;
    ecx = rd32(m, esi + 0x4u);
    ecx = rd32(m, ecx + 0x1b4u);
    eax = rd32(m, ecx + 0x58cu);
    fR_6c99b5 = (eax & eax);
    if ((fR_6c99b5 == 0)) goto L_6c99ea;
    x0 = movss_m(rd32(m, eax));
    x1 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    fC_6c99c5 = fcmp32(x0.l(0), x1.l(0));
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_6c99c5) | 2u) << 8);
    fR_6c99c9 = (((eax >> 8) & 0xffu) & 0x44u);
    if (parity8(fR_6c99c9)) goto L_6c99ea;
    ecx = rd32(m, ecx + 0x598u);
    fR_6c99d4 = (ecx & ecx);
    if ((fR_6c99d4 == 0)) goto L_6c99ea;
    x0 = movss_m(rd32(m, ecx));
    fC_6c99dc = fcmp32(x0.l(0), x1.l(0));
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_6c99dc) | 2u) << 8);
    fR_6c99e0 = (((eax >> 8) & 0xffu) & 0x44u);
    if (parity8(fR_6c99e0)) goto L_6c99ea;
L_6c99e5:
    edi = 0x1u;
L_6c99ea:
    fA_6c99ea = rd32(m, esi + 0x1cu);
    fB_6c99ea = ebx;
    if (fA_6c99ea != fB_6c99ea) goto L_6c9a2a;
    esi = rd32(m, esi + 0x4u);
    eax = rd32(m, esi + 0x1b4u);
    fA_6c99f8 = rd32(m, eax + 0xaf10u);
    fB_6c99f8 = ebx;
    if (fA_6c99f8 != fB_6c99f8) goto L_6c9a2a;
    edx = rd32(m, esi + 0x1acu);
    fA_6c9a06 = rd32(m, edx + 0x280u);
    fB_6c9a06 = ebx;
    if (fA_6c9a06 != fB_6c9a06) goto L_6c9a24;
    fA_6c9a0e = rd32(m, eax + 0x5b57cu);
    fB_6c9a0e = 0xcu;
    if (fA_6c9a0e != fB_6c9a0e) goto L_6c9a24;
    fR_6c9a17 = (edi & edi);
    if ((fR_6c9a17 == 0)) goto L_6c9a24;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    eax = 0x1u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_6c9a24:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_6c9a2a:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    eax = ebx;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_6c9a30:
    esi = rd32(m, esp); esp += 4;
    eax = ebx;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 006c9b00 Fn_006c9b00
//   original FUN_006c9b00; register inputs: eax; returns with ret 0
REFLEX_PORT_FN(0x006c9b00, Fn_006c9b00, "fx", reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_6c9b04 = 0;
    uint32_t fB_6c9b04 = 0;
    uint32_t fR_6c9b2e = 0;
    uint32_t fR_6c9b35 = 0;
    uint32_t fA_6c9b39 = 0;
    uint32_t fB_6c9b39 = 0;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    fA_6c9b04 = rd32(m, esi + 0x5cu);
    fB_6c9b04 = 0x0u;
    esp -= 4; wr32(m, esp, edi);
    if (fA_6c9b04 != fB_6c9b04) goto L_6c9b12;
    wr32(m, esi + 0x5cu, 0x1u);
L_6c9b12:
    eax = rd32(m, esi + 0x4u);
    ecx = rd32(m, eax + 0x1acu);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0xacu);
    LIFT_CALL(eax, 0x6c9b25u);
    ebx = eax;
    eax = esi;
    LIFT_CALL(0x6c9940u, 0x6c9b2eu);  // FUN_006c9940
    fR_6c9b2e = (eax & eax);
    if ((fR_6c9b2e == 0)) goto L_6c9b4e;
    edi = rd32(m, esi + 0x60u);
    fR_6c9b35 = (edi & edi);
    if ((fR_6c9b35 == 0)) goto L_6c9b55;
    fA_6c9b39 = rd32(m, edi + 0x64u);
    fB_6c9b39 = ebx;
    if (fA_6c9b39 != fB_6c9b39) goto L_6c9b55;
    LIFT_CALL(0x6c98c0u, 0x6c9b43u);  // FUN_006c98c0
    wr32(m, esi + 0x28u, 0x1u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_6c9b4e:
    wr32(m, esi + 0x60u, 0x0u);
L_6c9b55:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 006c9ed0 Fn_006c9ed0
//   original FUN_006c9ed0; register inputs: none; returns with ret 4
REFLEX_PORT_FN(0x006c9ed0, Fn_006c9ed0, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fA_6c9ee8 = 0;
    uint32_t fB_6c9ee8 = 0;
    uint32_t fR_6c9efc = 0;
    uint32_t fA_6c9f13 = 0;
    uint32_t fB_6c9f13 = 0;
    uint32_t fR_6c9f27 = 0;
    uint32_t fR_6c9f44 = 0;
    uint32_t fA_6c9f55 = 0;
    uint32_t fB_6c9f55 = 0;
    uint32_t fA_6c9f61 = 0;
    uint32_t fB_6c9f61 = 0;
    uint32_t fA_6c9f66 = 0;
    uint32_t fB_6c9f66 = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x8u);
    eax = rd32(m, ebx + 0x4u);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, eax + 0x1acu);
    ecx = rd32(m, esi + 0x234u);
    esp -= 4; wr32(m, esp, edi);
    edi = 0u;
    fA_6c9ee8 = rd32(m, ecx + 0x2eb8u);
    fB_6c9ee8 = edi;
    if (fA_6c9ee8 == fB_6c9ee8) goto L_6c9f0d;
    edx = rd32(m, esi);
    eax = rd32(m, edx + 0xacu);
    ecx = esi;
    LIFT_CALL(eax, 0x6c9efcu);
    fR_6c9efc = (eax & eax);
    if (!(fR_6c9efc == 0)) goto L_6c9f0d;
    edi = 0x1u;
    eax = edi;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_6c9f0d:
    ecx = rd32(m, esi + 0x234u);
    fA_6c9f13 = rd32(m, ecx + 0x2eb4u);
    fB_6c9f13 = edi;
    if (fA_6c9f13 == fB_6c9f13) goto L_6c9f38;
    edx = rd32(m, esi);
    eax = rd32(m, edx + 0xacu);
    ecx = esi;
    LIFT_CALL(eax, 0x6c9f27u);
    fR_6c9f27 = (eax & eax);
    if (!(fR_6c9f27 == 0)) goto L_6c9f38;
    edi = 0x2u;
    eax = edi;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_6c9f38:
    edx = rd32(m, esi);
    eax = rd32(m, edx + 0xacu);
    ecx = esi;
    LIFT_CALL(eax, 0x6c9f44u);
    fR_6c9f44 = (eax & eax);
    if ((fR_6c9f44 == 0)) goto L_6c9f8a;
    edx = rd32(m, esi);
    eax = rd32(m, edx + 0xe8u);
    ecx = esi;
    LIFT_CALL(eax, 0x6c9f54u);
    eax = (eax - 1u);
    fA_6c9f55 = eax;
    fB_6c9f55 = 0x7u;
    if (fA_6c9f55 > fB_6c9f55) goto L_6c9f8a;
    switch (eax) { case 0: goto L_6c9f61; case 1: goto L_6c9f61; case 2: goto L_6c9f61; case 3: goto L_6c9f61; case 4: goto L_6c9f61; case 5: goto L_6c9f61; case 6: goto L_6c9f78; case 7: goto L_6c9f85; default: throw std::runtime_error("jump table"); }
L_6c9f61:
    fA_6c9f61 = rd32(m, ebx + 0x1cu);
    fB_6c9f61 = edi;
    if (fA_6c9f61 != fB_6c9f61) goto L_6c9f8a;
    fA_6c9f66 = rd32(m, ebx + 0x28u);
    fB_6c9f66 = edi;
    if (fA_6c9f66 != fB_6c9f66) goto L_6c9f8a;
    edi = 0x5u;
    eax = edi;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_6c9f78:
    edi = 0x3u;
    eax = edi;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_6c9f85:
    edi = 0x4u;
L_6c9f8a:
    eax = edi;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 006c9fc0 Fn_006c9fc0
//   original FUN_006c9fc0; register inputs: edx esi; returns with ret 0
REFLEX_PORT_FN(0x006c9fc0, Fn_006c9fc0, "fx", reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_6c9fe0 = 0;
    uint32_t fB_6c9fe0 = 0;
    x0 = X{};
    esp -= 4; wr32(m, esp, edi);
    ecx = edx;
    wr32(m, esi + 0x64u, edx);
    wr32(m, esi + 0x80u, x0.l(0));
    wr32(m, esi + 0x6cu, x0.l(0));
    LIFT_CALL(0x6cd000u, 0x6c9fdbu);  // FUN_006cd000
    ecx = (edx - 0x1u);
    edi = 0u;
    fA_6c9fe0 = ecx;
    fB_6c9fe0 = 0x4u;
    if (fA_6c9fe0 > fB_6c9fe0) goto L_6c9ff5;
    switch (ecx) { case 0: goto L_6c9fec; case 1: goto L_6c9fec; case 2: goto L_6c9ff0; case 3: goto L_6c9ff0; case 4: goto L_6c9ff0; default: throw std::runtime_error("jump table"); }
L_6c9fec:
    edi = 0u;
    goto L_6c9ff5;
L_6c9ff0:
    edi = 0x1u;
L_6c9ff5:
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x6c8570u, 0x6c9ffcu);  // FUN_006c8570
    ecx = rd32(m, esi + 0x4u);
    edx = rd32(m, esi + 0x64u);
    esp = esp + 0x8u;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, 0x9607a0u);
    ecx = eax;
    wr32(m, esi + 0x68u, eax);
    LIFT_CALL(0x6cd810u, 0x6ca015u);  // FUN_006cd810
    edi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 006ca030 Fn_006ca030
//   original FUN_006ca030; register inputs: ecx eax; returns with ret 0
REFLEX_PORT_FN(0x006ca030, Fn_006ca030, "fx", reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fR_6ca037 = 0;
    esp -= 4; wr32(m, esp, ecx);
    ecx = rd32(m, eax + 0x68u);
    edx = rd32(m, eax + 0x64u);
    fR_6ca037 = (ecx & ecx);
    if ((fR_6ca037 == 0)) goto L_6ca067;
    x0 = X{};
    wr32(m, eax + 0x64u, 0x0u);
    wr32(m, eax + 0x68u, 0x0u);
    wr32(m, eax + 0x6cu, x0.l(0));
    wr32(m, eax + 0x80u, x0.l(0));
    eax = rd32(m, eax + 0x4u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, 0x942794u);
    LIFT_CALL(0x6cd810u, 0x6ca067u);  // FUN_006cd810
L_6ca067:
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 006ca070 Fn_006ca070
//   original FUN_006ca070; register inputs: eax; returns with ret 0
REFLEX_PORT_FN(0x006ca070, Fn_006ca070, "fx", reflex_port::OUT_EDX | (reflex_port::OUT_XMM0 << 0) | (reflex_port::OUT_XMM0 << 1)) {
    LIFT_ENTRY();
    uint32_t fA_6ca079 = 0;
    uint32_t fB_6ca079 = 0;
    x0 = movss_m(rd32(m, 0x92ff84u) /* 0.25 */);
    eax = (eax - 1u);
    fA_6ca079 = eax;
    fB_6ca079 = 0x4u;
    if (fA_6ca079 > fB_6ca079) goto L_6ca0a3;
    switch (eax) { case 0: goto L_6ca085; case 1: goto L_6ca08e; case 2: goto L_6ca097; case 3: goto L_6ca097; case 4: goto L_6ca0a0; default: throw std::runtime_error("jump table"); }
L_6ca085:
    x0 = movss_m(rd32(m, 0x9c3e1cu));
    LIFT_RET(0);
L_6ca08e:
    x0 = movss_m(rd32(m, 0x9c3e20u));
    LIFT_RET(0);
L_6ca097:
    x0 = movss_m(rd32(m, 0x9322ccu) /* 0.5 */);
    LIFT_RET(0);
L_6ca0a0:
    x0 = X{};
L_6ca0a3:
    LIFT_RET(0);
}

// 006ca0c0 Fn_006ca0c0
//   original FUN_006ca0c0; register inputs: ecx eax; returns with ret 4
REFLEX_PORT_FN(0x006ca0c0, Fn_006ca0c0, "fx", reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fA_6ca0c7 = 0;
    uint32_t fB_6ca0c7 = 0;
    uint32_t fA_6ca0e0 = 0;
    uint32_t fB_6ca0e0 = 0;
    uint32_t fR_6ca0f1 = 0;
    FCmp fC_6ca10a{};
    uint32_t fR_6ca155 = 0;
    FCmp fC_6ca167{};
    FCmp fC_6ca172{};
    uint32_t fR_6ca17a = 0;
    uint32_t fA_6ca18e = 0;
    uint32_t fB_6ca18e = 0;
    uint32_t FL = 0;
    uint32_t fR_6ca19d = 0;
    uint32_t fR_6ca1bb = 0;
    FCmp fC_6ca1d9{};
    uint32_t fR_6ca208 = 0;
    uint32_t fR_6ca232 = 0;
    FCmp fC_6ca23f{};
    uint32_t fR_6ca264 = 0;
    FCmp fC_6ca26d{};
    uint32_t fR_6ca292 = 0;
    FCmp fC_6ca29b{};
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    eax = rd32(m, esi + 0x64u);
    fA_6ca0c7 = eax;
    fB_6ca0c7 = 0x5u;
    if (fA_6ca0c7 > fB_6ca0c7) goto L_6ca2c7;
    switch (eax) { case 0: goto L_6ca0d7; case 1: goto L_6ca12a; case 2: goto L_6ca1f2; case 3: goto L_6ca21c; case 4: goto L_6ca27c; case 5: goto L_6ca24e; default: throw std::runtime_error("jump table"); }
L_6ca0d7:
    eax = rd32(m, esi + 0x4u);
    ecx = rd32(m, eax + 0x1acu);
    fA_6ca0e0 = rd32(m, ecx + 0x280u);
    fB_6ca0e0 = 0x0u;
    if (fA_6ca0e0 != fB_6ca0e0) goto L_6ca11d;
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x6c9ed0u, 0x6ca0efu);  // FUN_006c9ed0
    edx = eax;
    fR_6ca0f1 = (edx & edx);
    if ((fR_6ca0f1 == 0)) goto L_6ca11d;
    x1 = movss_m(rd32(m, esi + 0x6cu));
    x1 = addss(x1, rd32(m, esp + 0xcu));
    wr32(m, esi + 0x6cu, x1.l(0));
    LIFT_CALL(0x6ca070u, 0x6ca10au);  // FUN_006ca070
    fC_6ca10a = fcmp32(x1.l(0), x0.l(0));
    if (fc_b(fC_6ca10a)) goto L_6ca2c7;
    LIFT_CALL(0x6c9fc0u, 0x6ca118u);  // FUN_006c9fc0
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_6ca11d:
    x0 = X{};
    wr32(m, esi + 0x6cu, x0.l(0));
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_6ca12a:
    x0 = movss_m(rd32(m, esi + 0x6cu));
    edx = rd32(m, esi + 0x4u);
    x0 = addss(x0, rd32(m, esp + 0xcu));
    wr32(m, esi + 0x6cu, x0.l(0));
    ecx = rd32(m, edx + 0x1acu);
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0xacu);
    LIFT_CALL(edx, 0x6ca14du);
    x0 = movss_m(rd32(m, esi + 0x80u));
    fR_6ca155 = (eax & eax);
    if ((fR_6ca155 == 0)) goto L_6ca172;
    x0 = addss(x0, rd32(m, esp + 0xcu));
    wr32(m, esi + 0x80u, x0.l(0));
    fC_6ca167 = fcmp32(x0.l(0), rd32(m, 0x9c3e2cu));
    if ((fc_b(fC_6ca167) || fc_z(fC_6ca167))) goto L_6ca1a8;
    goto L_6ca1a1;
L_6ca172:
    fC_6ca172 = fcmp32(x0.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_6ca172) | 2u) << 8);
    fR_6ca17a = (((eax >> 8) & 0xffu) & 0x44u);
    if (!parity8(fR_6ca17a)) goto L_6ca197;
    eax = rd32(m, esi + 0x4u);
    ecx = rd32(m, eax + 0x1acu);
    edx = rd32(m, ecx + 0x234u);
    fA_6ca18e = rd32(m, edx + 0x2eb8u);
    fB_6ca18e = 0x0u;
    FL = fl_sub(fA_6ca18e, fB_6ca18e, 4);
    goto L_6ca19f;
L_6ca197:
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x6c9ed0u, 0x6ca19du);  // FUN_006c9ed0
    fR_6ca19d = (eax & eax);
    FL = fl_logic(fR_6ca19d, 4);
L_6ca19f:
    if (fl_cc(5, FL)) goto L_6ca1a8;
L_6ca1a1:
    eax = esi;
    LIFT_CALL(0x6ca030u, 0x6ca1a8u);  // FUN_006ca030
L_6ca1a8:
    eax = rd32(m, esi + 0x4u);
    ecx = rd32(m, eax + 0x1acu);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0xacu);
    LIFT_CALL(eax, 0x6ca1bbu);
    fR_6ca1bb = (eax & eax);
    if ((fR_6ca1bb == 0)) goto L_6ca2c7;
    x0 = movss_m(rd32(m, esi + 0x80u));
    x0 = addss(x0, rd32(m, esp + 0xcu));
    wr32(m, esi + 0x80u, x0.l(0));
    fC_6ca1d9 = fcmp32(x0.l(0), rd32(m, 0x9c3e2cu));
    if ((fc_b(fC_6ca1d9) || fc_z(fC_6ca1d9))) goto L_6ca2c7;
    eax = esi;
    LIFT_CALL(0x6ca030u, 0x6ca1edu);  // FUN_006ca030
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_6ca1f2:
    x0 = movss_m(rd32(m, esi + 0x6cu));
    x0 = addss(x0, rd32(m, esp + 0xcu));
    esp -= 4; wr32(m, esp, esi);
    wr32(m, esi + 0x6cu, x0.l(0));
    LIFT_CALL(0x6c9ed0u, 0x6ca208u);  // FUN_006c9ed0
    fR_6ca208 = (eax & eax);
    if (!(fR_6ca208 == 0)) goto L_6ca2c7;
L_6ca210:
    eax = esi;
    LIFT_CALL(0x6ca030u, 0x6ca217u);  // FUN_006ca030
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_6ca21c:
    x0 = movss_m(rd32(m, esi + 0x6cu));
    x0 = addss(x0, rd32(m, esp + 0xcu));
    esp -= 4; wr32(m, esp, esi);
    wr32(m, esi + 0x6cu, x0.l(0));
    LIFT_CALL(0x6c9ed0u, 0x6ca232u);  // FUN_006c9ed0
    fR_6ca232 = (eax & eax);
    if (!(fR_6ca232 == 0)) goto L_6ca2c7;
    x0 = movss_m(rd32(m, esi + 0x6cu));
    fC_6ca23f = fcmp32(x0.l(0), rd32(m, 0x9322ccu) /* 0.5 */);
    if (!(fc_b(fC_6ca23f) || fc_z(fC_6ca23f))) goto L_6ca210;
    ecx = rd32(m, esi + 0x4u);
    esp -= 4; wr32(m, esp, ecx);
    goto L_6ca2ac;
L_6ca24e:
    x0 = movss_m(rd32(m, esi + 0x6cu));
    x0 = addss(x0, rd32(m, esp + 0xcu));
    esp -= 4; wr32(m, esp, esi);
    wr32(m, esi + 0x6cu, x0.l(0));
    LIFT_CALL(0x6c9ed0u, 0x6ca264u);  // FUN_006c9ed0
    fR_6ca264 = (eax & eax);
    if (!(fR_6ca264 == 0)) goto L_6ca2c7;
    x0 = movss_m(rd32(m, esi + 0x6cu));
    fC_6ca26d = fcmp32(x0.l(0), rd32(m, 0xa9430cu));
    if (!(fc_b(fC_6ca26d) || fc_z(fC_6ca26d))) goto L_6ca210;
    edx = rd32(m, esi + 0x4u);
    esp -= 4; wr32(m, esp, edx);
    goto L_6ca2ac;
L_6ca27c:
    x0 = movss_m(rd32(m, esi + 0x6cu));
    x0 = addss(x0, rd32(m, esp + 0xcu));
    esp -= 4; wr32(m, esp, esi);
    wr32(m, esi + 0x6cu, x0.l(0));
    LIFT_CALL(0x6c9ed0u, 0x6ca292u);  // FUN_006c9ed0
    fR_6ca292 = (eax & eax);
    if (!(fR_6ca292 == 0)) goto L_6ca2c7;
    x0 = movss_m(rd32(m, esi + 0x6cu));
    fC_6ca29b = fcmp32(x0.l(0), rd32(m, 0x9322ccu) /* 0.5 */);
    if (!(fc_b(fC_6ca29b) || fc_z(fC_6ca29b))) goto L_6ca210;
    eax = rd32(m, esi + 0x4u);
    esp -= 4; wr32(m, esp, eax);
L_6ca2ac:
    ecx = rd32(m, esi + 0x68u);
    edx = 0x960764u;
    LIFT_CALL(0x6cd750u, 0x6ca2b9u);  // FUN_006cd750
    wr32(m, esi + 0x68u, 0x0u);
    wr32(m, esi + 0x64u, 0x0u);
L_6ca2c7:
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 006cab80 Fn_006cab80
//   original FUN_006cab80; register inputs: eax; returns with ret 4
REFLEX_PORT_FN(0x006cab80, Fn_006cab80, "fx", reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fA_6cab86 = 0;
    uint32_t fB_6cab86 = 0;
    uint32_t fA_6cab94 = 0;
    uint32_t fB_6cab94 = 0;
    uint32_t fA_6cab9c = 0;
    uint32_t fB_6cab9c = 0;
    uint32_t fA_6caba8 = 0;
    uint32_t fB_6caba8 = 0;
    uint32_t fA_6cabb1 = 0;
    uint32_t fB_6cabb1 = 0;
    uint32_t fA_6cabb6 = 0;
    uint32_t fB_6cabb6 = 0;
    uint32_t fA_6cabbf = 0;
    uint32_t fB_6cabbf = 0;
    uint32_t fR_6cabed = 0;
    uint32_t fA_6cac22 = 0;
    uint32_t fB_6cac22 = 0;
    uint32_t fA_6cac2a = 0;
    uint32_t fB_6cac2a = 0;
    uint32_t fA_6cac46 = 0;
    uint32_t fB_6cac46 = 0;
    uint32_t fA_6cac4a = 0;
    uint32_t fB_6cac4a = 0;
    uint32_t fA_6cac4f = 0;
    uint32_t fB_6cac4f = 0;
    FCmp fC_6cac60{};
    uint32_t fA_6cac7c = 0;
    uint32_t fB_6cac7c = 0;
    uint32_t fA_6cac85 = 0;
    uint32_t fB_6cac85 = 0;
    uint32_t fR_6cacd3 = 0;
    FCmp fC_6cace5{};
    FCmp fC_6cad1e{};
    uint32_t fA_6cad29 = 0;
    uint32_t fB_6cad29 = 0;
    FCmp fC_6cad39{};
    uint32_t fA_6cad65 = 0;
    uint32_t fB_6cad65 = 0;
    FCmp fC_6cad75{};
    FCmp fC_6cad7a{};
    uint32_t fA_6cad85 = 0;
    uint32_t fB_6cad85 = 0;
    uint32_t fA_6cad8a = 0;
    uint32_t fB_6cad8a = 0;
    uint32_t fA_6cad8f = 0;
    uint32_t fB_6cad8f = 0;
    uint32_t fA_6cad94 = 0;
    uint32_t fB_6cad94 = 0;
    uint32_t fA_6cad9f = 0;
    uint32_t fB_6cad9f = 0;
    FCmp fC_6cadaf{};
    FCmp fC_6cadc2{};
    uint32_t fA_6cadc7 = 0;
    uint32_t fB_6cadc7 = 0;
    uint32_t fA_6cadcf = 0;
    uint32_t fB_6cadcf = 0;
    uint32_t fA_6cadd7 = 0;
    uint32_t fB_6cadd7 = 0;
    uint32_t fA_6cae01 = 0;
    uint32_t fB_6cae01 = 0;
    uint32_t fA_6cae17 = 0;
    uint32_t fB_6cae17 = 0;
    FCmp fC_6cae27{};
    uint32_t fA_6cae4b = 0;
    uint32_t fB_6cae4b = 0;
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    esi = eax;
    edi = 0u;
    fA_6cab86 = rd32(m, esi);
    fB_6cab86 = edi;
    if (fA_6cab86 == fB_6cab86) goto L_6cae4b;
    eax = rd32(m, esi + 0x128u);
    fA_6cab94 = eax;
    fB_6cab94 = edi;
    if (fA_6cab94 == fB_6cab94) goto L_6cae44;
    fA_6cab9c = rd32(m, eax + 0x1acu);
    fB_6cab9c = edi;
    if (fA_6cab9c == fB_6cab9c) goto L_6cae44;
    fA_6caba8 = rd32(m, eax + 0x4u);
    fB_6caba8 = edi;
    if (fA_6caba8 == fB_6caba8) goto L_6cae44;
    fA_6cabb1 = rd32(m, eax + 0x10u);
    fB_6cabb1 = edi;
    if (fA_6cabb1 == fB_6cabb1) goto L_6cabc9;
    fA_6cabb6 = rd32(m, eax + 0x4u);
    fB_6cabb6 = edi;
    if (fA_6cabb6 == fB_6cabb6) goto L_6cae44;
    fA_6cabbf = rd32(m, eax + 0x10u);
    fB_6cabbf = 0x1u;
    if (fA_6cabbf != fB_6cabbf) goto L_6cae44;
L_6cabc9:
    LIFT_CALL(0x6cb720u, 0x6cabceu);  // FUN_006cb720
    eax = rd32(m, esi + 0x128u);
    ecx = rd32(m, eax + 0x1acu);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0xacu);
    LIFT_CALL(eax, 0x6cabe4u);
    x2 = X{};
    x1 = movss_m(rd32(m, esp + 0xcu));
    fR_6cabed = (eax & eax);
    if ((fR_6cabed == 0)) goto L_6cac0f;
    x0 = movss_m(rd32(m, esi + 0x70u));
    x0 = addss(x0, x1.l(0));
    wr32(m, esi + 0x70u, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x74u));
    x0 = addss(x0, x1.l(0));
    wr32(m, esi + 0x74u, x0.l(0));
    goto L_6cac40;
L_6cac0f:
    x0 = movss_m(rd32(m, esi + 0x6cu));
    x0 = addss(x0, x1.l(0));
    wr32(m, esi + 0x6cu, x0.l(0));
    wr32(m, esi + 0x74u, x2.l(0));
    fA_6cac22 = rd32(m, esi + 0x30cu);
    fB_6cac22 = edi;
    if (fA_6cac22 != fB_6cac22) goto L_6cac40;
    fA_6cac2a = rd32(m, esi + 0x2dcu);
    fB_6cac2a = edi;
    if (fA_6cac2a != fB_6cac2a) goto L_6cac40;
    x0 = movss_m(rd32(m, esi + 0x58u));
    x0 = addss(x0, x1.l(0));
    wr32(m, esi + 0x58u, x0.l(0));
L_6cac40:
    eax = rd32(m, esi + 0x124u);
    fA_6cac46 = eax;
    fB_6cac46 = edi;
    eax = fA_6cac46 - fB_6cac46;
    if (fA_6cac46 == fB_6cac46) goto L_6cac9b;
    fA_6cac4a = eax;
    fB_6cac4a = 0x1u;
    eax = fA_6cac4a - fB_6cac4a;
    if (fA_6cac4a == fB_6cac4a) goto L_6cac76;
    fA_6cac4f = eax;
    fB_6cac4f = 0x1u;
    eax = fA_6cac4f - fB_6cac4f;
    if (fA_6cac4f != fB_6cac4f) goto L_6cac9b;
    x0 = movss_m(rd32(m, esi + 0x2d0u));
    x0 = subss(x0, x1.l(0));
    fC_6cac60 = fcmp32(x2.l(0), x0.l(0));
    wr32(m, esi + 0x2d0u, x0.l(0));
    if (fc_b(fC_6cac60)) goto L_6cac9b;
    ecx = esi;
    LIFT_CALL(0x6caaf0u, 0x6cac74u);  // FUN_006caaf0
    goto L_6cac9b;
L_6cac76:
    ecx = rd32(m, esi + 0x128u);
    fA_6cac7c = rd32(m, ecx + 0xb60u);
    fB_6cac7c = 0x2u;
    if (fA_6cac7c == fB_6cac7c) goto L_6cac8d;
    fA_6cac85 = rd32(m, esi + 0x2dcu);
    fB_6cac85 = edi;
    if (fA_6cac85 == fB_6cac85) goto L_6cac9b;
L_6cac8d:
    x0 = movss_m(rd32(m, esi + 0x68u));
    x0 = addss(x0, x1.l(0));
    wr32(m, esi + 0x68u, x0.l(0));
L_6cac9b:
    edx = rd32(m, esi + 0x128u);
    ecx = rd32(m, edx + 0x1acu);
    eax = rd32(m, ecx);
    eax = rd32(m, eax + 0x44u);
    edx = (esi + 0x130u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(eax, 0x6cacb5u);
    ecx = rd32(m, esi + 0x128u);
    ecx = rd32(m, ecx + 0x1acu);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0xacu);
    LIFT_CALL(eax, 0x6caccbu);
    x1 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    fR_6cacd3 = (eax & eax);
    if ((fR_6cacd3 == 0)) goto L_6cacea;
    ecx = rd32(m, esi + 0x128u);
    x0 = movss_m(rd32(m, ecx + 0x1230u));
    fC_6cace5 = fcmp32(x0.l(0), x1.l(0));
    if (!(fc_b(fC_6cace5) || fc_z(fC_6cace5))) goto L_6cad05;
L_6cacea:
    x0 = movss_m(rd32(m, esp + 0xcu));
    x0 = addss(x0, rd32(m, esi + 0x314u));
    wr32(m, esi + 0x314u, x0.l(0));
    x0 = X{};
    goto L_6cad10;
L_6cad05:
    x0 = X{};
    wr32(m, esi + 0x314u, x0.l(0));
L_6cad10:
    eax = rd32(m, esi + 0x128u);
    x2 = movss_m(rd32(m, eax + 0x116cu));
    fC_6cad1e = fcmp32(x2.l(0), x0.l(0));
    if ((fc_b(fC_6cad1e) || fc_z(fC_6cad1e))) goto L_6cad29;
    wr32(m, esi + 0x41cu, edi);
L_6cad29:
    fA_6cad29 = rd32(m, esi + 0x41cu);
    fB_6cad29 = edi;
    if (fA_6cad29 == fB_6cad29) goto L_6cad9f;
    x2 = movss_m(rd32(m, esi + 0x314u));
    fC_6cad39 = fcmp32(x2.l(0), x1.l(0));
    if ((fc_b(fC_6cad39) || fc_z(fC_6cad39))) goto L_6cad5f;
    edx = rd32(m, eax + 0x1f0u);
    eax = rd32(m, 0xd92edcu);
    esp -= 4; wr32(m, esp, 0x20u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x6d7980u, 0x6cad52u);  // MXHighlightTracking_AddProgress
    x1 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x0 = X{};
    goto L_6cad99;
L_6cad5f:
    ecx = rd32(m, eax + 0x1acu);
    fA_6cad65 = rd32(m, ecx + 0x280u);
    fB_6cad65 = edi;
    if (fA_6cad65 != fB_6cad65) goto L_6cad99;
    x2 = movss_m(rd32(m, eax + 0xb38u));
    fC_6cad75 = fcmp32(x2.l(0), x0.l(0));
    if ((fc_b(fC_6cad75) || fc_z(fC_6cad75))) goto L_6cad9f;
    fC_6cad7a = fcmp32(x2.l(0), x0.l(0));
    if ((fc_b(fC_6cad7a) || fc_z(fC_6cad7a))) goto L_6cad99;
    eax = rd32(m, eax + 0xb44u);
    fA_6cad85 = eax;
    fB_6cad85 = 0x1u;
    if (fA_6cad85 == fB_6cad85) goto L_6cad99;
    fA_6cad8a = eax;
    fB_6cad8a = 0x4u;
    if (fA_6cad8a == fB_6cad8a) goto L_6cad99;
    fA_6cad8f = eax;
    fB_6cad8f = 0x7u;
    if (fA_6cad8f == fB_6cad8f) goto L_6cad99;
    fA_6cad94 = eax;
    fB_6cad94 = 0xau;
    if (fA_6cad94 != fB_6cad94) goto L_6cad9f;
L_6cad99:
    wr32(m, esi + 0x41cu, edi);
L_6cad9f:
    fA_6cad9f = rd32(m, esi + 0x420u);
    fB_6cad9f = edi;
    if (fA_6cad9f == fB_6cad9f) goto L_6cae0b;
    x2 = movss_m(rd32(m, esi + 0x314u));
    fC_6cadaf = fcmp32(x2.l(0), x1.l(0));
    if ((fc_b(fC_6cadaf) || fc_z(fC_6cadaf))) goto L_6cadf5;
    eax = rd32(m, esi + 0x128u);
    x1 = movss_m(rd32(m, eax + 0xb38u));
    fC_6cadc2 = fcmp32(x1.l(0), x0.l(0));
    if (!(fc_b(fC_6cadc2) || fc_z(fC_6cadc2))) goto L_6cadf5;
    fA_6cadc7 = rd32(m, esi + 0x424u);
    fB_6cadc7 = edi;
    if (fA_6cadc7 == fB_6cadc7) goto L_6cae2c;
    fA_6cadcf = rd32(m, esi + 0x428u);
    fB_6cadcf = edi;
    if (fA_6cadcf == fB_6cadcf) goto L_6cae2c;
    fA_6cadd7 = rd32(m, esi + 0x42cu);
    fB_6cadd7 = edi;
    if (fA_6cadd7 == fB_6cadd7) goto L_6cae2c;
    edx = rd32(m, eax + 0x1f0u);
    eax = rd32(m, 0xd92edcu);
    esp -= 4; wr32(m, esp, 0x21u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x6d7980u, 0x6cadf3u);  // MXHighlightTracking_AddProgress
    goto L_6cae2c;
L_6cadf5:
    ecx = rd32(m, esi + 0x128u);
    edx = rd32(m, ecx + 0x1acu);
    fA_6cae01 = rd32(m, edx + 0x280u);
    fB_6cae01 = edi;
    if (fA_6cae01 == fB_6cae01) goto L_6cae44;
    goto L_6cae2c;
L_6cae0b:
    eax = rd32(m, esi + 0x128u);
    ecx = rd32(m, eax + 0x1acu);
    fA_6cae17 = rd32(m, ecx + 0x280u);
    fB_6cae17 = edi;
    if (fA_6cae17 != fB_6cae17) goto L_6cae2c;
    x0 = movss_m(rd32(m, esi + 0x314u));
    fC_6cae27 = fcmp32(x0.l(0), x1.l(0));
    if ((fc_b(fC_6cae27) || fc_z(fC_6cae27))) goto L_6cae44;
L_6cae2c:
    wr32(m, esi + 0x420u, edi);
    wr32(m, esi + 0x424u, edi);
    wr32(m, esi + 0x428u, edi);
    wr32(m, esi + 0x42cu, edi);
L_6cae44:
    eax = esi;
    LIFT_CALL(0x6cae70u, 0x6cae4bu);  // FUN_006cae70
L_6cae4b:
    fA_6cae4b = rd32(m, 0xd5ab64u);
    fB_6cae4b = edi;
    if (fA_6cae4b == fB_6cae4b) goto L_6cae5e;
    LIFT_CALL(0x6cc8b0u, 0x6cae58u);  // FUN_006cc8b0
    wr32(m, 0xd5ab64u, edi);
L_6cae5e:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 006cae70 Fn_006cae70
//   original FUN_006cae70; register inputs: eax; returns with ret 
REFLEX_PORT_FN(0x006cae70, Fn_006cae70, "fx", reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    FCmp fC_6cae75{};
    uint32_t fA_6cae9c = 0;
    uint32_t fB_6cae9c = 0;
    uint32_t fA_6caeb8 = 0;
    uint32_t fB_6caeb8 = 0;
    uint32_t fA_6caed4 = 0;
    uint32_t fB_6caed4 = 0;
    uint32_t fA_6caef0 = 0;
    uint32_t fB_6caef0 = 0;
    x0 = movss_m(rd32(m, eax + 0x58u));
    fC_6cae75 = fcmp32(x0.l(0), rd32(m, 0x9c3e48u));
    esp -= 4; wr32(m, esp, esi);
    if ((fc_b(fC_6cae75) || fc_z(fC_6cae75))) goto L_6caf0e;
    ecx = rd32(m, eax + 0x128u);
    esi = rd32(m, ecx + 0x94u);
    x0 = movss_m(rd32(m, 0x9c3e4cu));
    edx = 0x2u;
    fA_6cae9c = rd32(m, esi + 0x21cu);
    fB_6cae9c = edx;
    if (fA_6cae9c != fB_6cae9c) goto L_6caeb2;
    x1 = movss_m(rd32(m, eax + 0x34u));
    x1 = addss(x1, x0.l(0));
    wr32(m, eax + 0x34u, x1.l(0));
L_6caeb2:
    esi = rd32(m, ecx + 0x94u);
    fA_6caeb8 = rd32(m, esi + 0x21cu);
    fB_6caeb8 = edx;
    if (fA_6caeb8 != fB_6caeb8) goto L_6caece;
    x1 = movss_m(rd32(m, eax + 0x30u));
    x1 = addss(x1, x0.l(0));
    wr32(m, eax + 0x30u, x1.l(0));
L_6caece:
    esi = rd32(m, ecx + 0x94u);
    fA_6caed4 = rd32(m, esi + 0x21cu);
    fB_6caed4 = edx;
    if (fA_6caed4 != fB_6caed4) goto L_6caeea;
    x1 = movss_m(rd32(m, eax + 0x38u));
    x1 = addss(x1, x0.l(0));
    wr32(m, eax + 0x38u, x1.l(0));
L_6caeea:
    ecx = rd32(m, ecx + 0x94u);
    fA_6caef0 = rd32(m, ecx + 0x21cu);
    fB_6caef0 = edx;
    if (fA_6caef0 != fB_6caef0) goto L_6caf06;
    x1 = movss_m(rd32(m, eax + 0x28u));
    x1 = addss(x1, x0.l(0));
    wr32(m, eax + 0x28u, x1.l(0));
L_6caf06:
    x0 = X{};
    wr32(m, eax + 0x58u, x0.l(0));
L_6caf0e:
    esi = rd32(m, esp); esp += 4;
    LIFT_TAILJMP(0x6cc4b0u);  // FUN_006cc4b0
}

// 006cb180 Fn_006cb180
//   original FUN_006cb180; register inputs: edx; returns with ret 0
REFLEX_PORT_FN(0x006cb180, Fn_006cb180, "fx", 0) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, edi);
    edi = (edx + 0x140u);
    ecx = 0x64u;
    eax = 0u;
    wr32(m, edx + 0x2d8u, 0x0u);
    while (ecx) { wr32(m, edi, eax); edi += 4; ecx--; }
    wr32(m, edx + 0x124u, 0x1u);
    edi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 006cb500 Fn_006cb500
//   original FUN_006cb500; register inputs: ecx edx; returns with ret 4
REFLEX_PORT_FN(0x006cb500, Fn_006cb500, "fx", reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fR_6cb506 = 0;
    FCmp fC_6cb514{};
    FCmp fC_6cb519{};
    uint32_t fR_6cb51d = 0;
    FCmp fC_6cb528{};
    eax = rd32(m, ecx + 0x430u);
    fR_6cb506 = (eax & eax);
    if ((fR_6cb506 == 0)) goto L_6cb542;
    x0 = movss_m(rd32(m, edx + 0x78u));
    x1 = movss_m(rd32(m, eax + 0x78u));
    fC_6cb514 = fcmp32(x0.l(0), x1.l(0));
    if (!(fc_b(fC_6cb514) || fc_z(fC_6cb514))) goto L_6cb542;
    fC_6cb519 = fcmp32(x0.l(0), x1.l(0));
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_6cb519) | 2u) << 8);
    fR_6cb51d = (((eax >> 8) & 0xffu) & 0x44u);
    if (parity8(fR_6cb51d)) goto L_6cb556;
    x0 = movss_m(rd32(m, esp + 0x4u));
    fC_6cb528 = fcmp32(x0.l(0), rd32(m, ecx + 0x434u));
    if ((fc_b(fC_6cb528) || fc_z(fC_6cb528))) goto L_6cb556;
    wr32(m, ecx + 0x430u, edx);
    wr32(m, ecx + 0x434u, x0.l(0));
    LIFT_RET(4);
L_6cb542:
    x0 = movss_m(rd32(m, esp + 0x4u));
    wr32(m, ecx + 0x430u, edx);
    wr32(m, ecx + 0x434u, x0.l(0));
L_6cb556:
    LIFT_RET(4);
}

// 006cb6a0 Fn_006cb6a0
//   original FUN_006cb6a0; register inputs: ecx; returns with ret 8
REFLEX_PORT_FN(0x006cb6a0, Fn_006cb6a0, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_6cb6a4 = 0;
    uint32_t fB_6cb6a4 = 0;
    uint32_t fA_6cb6b0 = 0;
    uint32_t fB_6cb6b0 = 0;
    uint32_t fA_6cb6b6 = 0;
    uint32_t fB_6cb6b6 = 0;
    uint32_t fA_6cb6bc = 0;
    uint32_t fB_6cb6bc = 0;
    uint32_t fA_6cb6c2 = 0;
    uint32_t fB_6cb6c2 = 0;
    uint32_t fA_6cb6c8 = 0;
    uint32_t fB_6cb6c8 = 0;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    fA_6cb6a4 = rd32(m, esi);
    fB_6cb6a4 = 0x0u;
    esp -= 4; wr32(m, esp, edi);
    if (fA_6cb6a4 == fB_6cb6a4) goto L_6cb717;
    eax = rd32(m, esi + 0x128u);
    fA_6cb6b0 = rd32(m, esp + 0x10u);
    fB_6cb6b0 = eax;
    if (fA_6cb6b0 != fB_6cb6b0) goto L_6cb717;
    fA_6cb6b6 = rd32(m, eax + 0x4u);
    fB_6cb6b6 = 0x0u;
    if (fA_6cb6b6 == fB_6cb6b6) goto L_6cb717;
    fA_6cb6bc = rd32(m, eax + 0x10u);
    fB_6cb6bc = 0x0u;
    if (fA_6cb6bc == fB_6cb6bc) goto L_6cb6ce;
    fA_6cb6c2 = rd32(m, eax + 0x4u);
    fB_6cb6c2 = 0x0u;
    if (fA_6cb6c2 == fB_6cb6c2) goto L_6cb717;
    fA_6cb6c8 = rd32(m, eax + 0x10u);
    fB_6cb6c8 = 0x1u;
    if (fA_6cb6c8 != fB_6cb6c8) goto L_6cb717;
L_6cb6ce:
    ebx = rd32(m, 0xa94318u);
    esp -= 4; wr32(m, esp, 0x9e1a7cu);
    ecx = 0x9cbeb4u;
    wr32(m, esp + 0x14u, eax);
    LIFT_CALL(rd32(m, 0x916388u) /* 8.287789190657982e-39 */, 0x6cb6e8u);
    esp -= 4; wr32(m, esp, 0x4u);
    eax = (esp + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    edi = 0x92fd20u;
    eax = ebx;
    LIFT_CALL(0x4020c0u, 0x6cb6fbu);  // FUN_004020c0
    x0 = X{};
    wr32(m, esi + 0x2dcu, 0x0u);
    wr32(m, esi + 0x124u, 0x0u);
    wr32(m, esi + 0x68u, x0.l(0));
L_6cb717:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 006cb720 Fn_006cb720
//   original FUN_006cb720; register inputs: esi; returns with ret 0
REFLEX_PORT_FN(0x006cb720, Fn_006cb720, "fx", 0) {
    LIFT_ENTRY();
    uint32_t fA_6cb734 = 0;
    uint32_t fB_6cb734 = 0;
    uint32_t fA_6cb741 = 0;
    uint32_t fB_6cb741 = 0;
    uint32_t fA_6cb749 = 0;
    uint32_t fB_6cb749 = 0;
    uint32_t fA_6cb75b = 0;
    uint32_t fB_6cb75b = 0;
    FCmp fC_6cb7a0{};
    uint32_t fA_6cb80e = 0;
    uint32_t fB_6cb80e = 0;
    uint32_t fA_6cb831 = 0;
    uint32_t fB_6cb831 = 0;
    eax = rd32(m, esi + 0x128u);
    ecx = rd32(m, eax + 0x1acu);
    esp = esp - 0x8u;
    esp -= 4; wr32(m, esp, ebx);
    edx = 0u;
    ebx = 0u;
    fA_6cb734 = rd32(m, ecx + 0x280u);
    fB_6cb734 = ebx;
    esp -= 4; wr32(m, esp, ebp);
    edx = (edx & 0xffffff00u) | (((fA_6cb734 != fB_6cb734) ? 1u : 0u) & 0xffu);
    esp -= 4; wr32(m, esp, edi);
    ebp = edx;
    fA_6cb741 = ebp;
    fB_6cb741 = ebx;
    if (fA_6cb741 == fB_6cb741) goto L_6cb854;
    fA_6cb749 = rd32(m, esi + 0x30cu);
    fB_6cb749 = ebx;
    if (fA_6cb749 != fB_6cb749) goto L_6cb854;
    x0 = X{};
    wr32(m, esi + 0x54u, ebx);
    fA_6cb75b = rd32(m, esi + 0x124u);
    fB_6cb75b = ebx;
    if (fA_6cb75b == fB_6cb75b) goto L_6cb798;
    ecx = rd32(m, 0xa94318u);
    wr32(m, esp + 0x10u, ecx);
    esp -= 4; wr32(m, esp, 0x9e1a7cu);
    ecx = 0x9cbeb4u;
    wr32(m, esp + 0x10u, eax);
    LIFT_CALL(rd32(m, 0x916388u) /* 8.287789190657982e-39 */, 0x6cb781u);
    eax = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, 0x4u);
    edx = (esp + 0x10u);
    esp -= 4; wr32(m, esp, edx);
    edi = 0x92fd20u;
    LIFT_CALL(0x4020c0u, 0x6cb796u);  // FUN_004020c0
    goto L_6cb7b6;
L_6cb798:
    x1 = movss_m(rd32(m, esi + 0x2d0u));
    fC_6cb7a0 = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_6cb7a0) || fc_z(fC_6cb7a0))) goto L_6cb7b9;
    ecx = rd32(m, 0xa94318u);
    esp -= 4; wr32(m, esp, ecx);
    edi = 0x92fce0u;
    LIFT_CALL(0x588b50u, 0x6cb7b6u);  // FUN_00588b50
L_6cb7b6:
    x0 = X{};
L_6cb7b9:
    eax = rd32(m, esi + 0x128u);
    wr32(m, esi + 0x2d0u, x0.l(0));
    wr32(m, esi + 0x2d4u, x0.l(0));
    wr32(m, esi + 0x58u, x0.l(0));
    x0 = movss_m(rd32(m, 0x9c3e44u));
    wr32(m, esi + 0x124u, ebx);
    wr32(m, esi + 0x2dcu, ebx);
    wr32(m, esi + 0x310u, ebx);
    wr32(m, esi + 0x2d8u, ebx);
    wr32(m, esi + 0x5cu, ebx);
    wr32(m, esi + 0x41cu, ebx);
    wr32(m, esi + 0x420u, ebx);
    edx = rd32(m, eax + 0x94u);
    ecx = 0x2u;
    fA_6cb80e = rd32(m, edx + 0x21cu);
    fB_6cb80e = ecx;
    if (fA_6cb80e != fB_6cb80e) goto L_6cb82b;
    x1 = x0;
    x1 = mulss(x1, rd32(m, 0x92ff84u) /* 0.25 */);
    x1 = addss(x1, rd32(m, esi + 0x28u));
    wr32(m, esi + 0x28u, x1.l(0));
L_6cb82b:
    eax = rd32(m, eax + 0x94u);
    fA_6cb831 = rd32(m, eax + 0x21cu);
    fB_6cb831 = ecx;
    wr32(m, esi + 0x30cu, ebp);
    if (fA_6cb831 != fB_6cb831) goto L_6cb85a;
    x1 = movss_m(rd32(m, esi + 0x2cu));
    x1 = addss(x1, x0.l(0));
    wr32(m, esi + 0x2cu, x1.l(0));
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x8u;
    LIFT_RET(0);
L_6cb854:
    wr32(m, esi + 0x30cu, ebp);
L_6cb85a:
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x8u;
    LIFT_RET(0);
}

// 006cb960 Fn_006cb960
//   original FUN_006cb960; register inputs: ecx eax; returns with ret 0
REFLEX_PORT_FN(0x006cb960, Fn_006cb960, "fx", reflex_port::OUT_EAX) {
    LIFT_ENTRY();
    uint32_t fA_6cb963 = 0;
    uint32_t fB_6cb963 = 0;
    esp = esp - 0x1cu;
    fA_6cb963 = rd32(m, eax + 0x64u);
    fB_6cb963 = 0x0u;
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    esi = ecx;
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0xcu);
    if (fA_6cb963 == fB_6cb963) goto L_6cb985;
    LIFT_CALL(rd32(m, 0x9161acu) /* 8.280765882754786e-39 */, 0x6cb978u);
    eax = (esp + 0x8u);
    esp -= 4; wr32(m, esp, eax);
    edi = (esi + 0x324u);
    goto L_6cb996;
L_6cb985:
    LIFT_CALL(rd32(m, 0x9161acu) /* 8.280765882754786e-39 */, 0x6cb98bu);
    ecx = (esp + 0x8u);
    esp -= 4; wr32(m, esp, ecx);
    edi = (esi + 0x344u);
L_6cb996:
    LIFT_CALL(0x6cd3f0u, 0x6cb99bu);  // FUN_006cd3f0
    esi = rd32(m, eax);
    ecx = (esp + 0x8u);
    LIFT_CALL(rd32(m, 0x9161b4u) /* 8.28055008279128e-39 */, 0x6cb9a7u);
    edi = rd32(m, esp); esp += 4;
    eax = esi;
    esi = rd32(m, esp); esp += 4;
    esp = esp + 0x1cu;
    LIFT_RET(0);
}

// 006cbad0 Fn_006cbad0
//   original FUN_006cbad0; register inputs: edi; returns with ret 0
REFLEX_PORT_FN(0x006cbad0, Fn_006cbad0, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fR_6cbb0c = 0;
    uint32_t fA_6cbb10 = 0;
    uint32_t fB_6cbb10 = 0;
    uint32_t fA_6cbb1a = 0;
    uint32_t fB_6cbb1a = 0;
    uint32_t fR_6cbb1e = 0;
    uint32_t fA_6cbb28 = 0;
    uint32_t fB_6cbb28 = 0;
    uint32_t fA_6cbb33 = 0;
    uint32_t fB_6cbb33 = 0;
    uint32_t fR_6cbb7c = 0;
    uint32_t fA_6cbb80 = 0;
    uint32_t fB_6cbb80 = 0;
    uint32_t fA_6cbb94 = 0;
    uint32_t fB_6cbb94 = 0;
    uint32_t fR_6cbb9a = 0;
    uint32_t fA_6cbba4 = 0;
    uint32_t fB_6cbba4 = 0;
    uint32_t fA_6cbbaf = 0;
    uint32_t fB_6cbbaf = 0;
    esp = esp - 0x10u;
    eax = rd32(m, edi + 0x33cu);
    eax = rd32(m, eax);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, edi + 0x324u);
    ebp = eax;
    wr32(m, esp + 0xcu, 0x0u);
    wr32(m, esp + 0x10u, esi);
    wr32(m, esp + 0x14u, ebp);
    goto L_6cbb00;
L_6cbb00:
    ebx = rd32(m, edi + 0x33cu);
    eax = rd32(m, edi + 0x324u);
    fR_6cbb0c = (esi & esi);
    if ((fR_6cbb0c == 0)) goto L_6cbb14;
    fA_6cbb10 = esi;
    fB_6cbb10 = eax;
    if (fA_6cbb10 == fB_6cbb10) goto L_6cbb1a;
L_6cbb14:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x6cbb1au);
L_6cbb1a:
    fA_6cbb1a = ebp;
    fB_6cbb1a = ebx;
    if (fA_6cbb1a == fB_6cbb1a) goto L_6cbb54;
    fR_6cbb1e = (esi & esi);
    if (!(fR_6cbb1e == 0)) goto L_6cbb50;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x6cbb28u);
L_6cbb28:
    fA_6cbb28 = ebp;
    fB_6cbb28 = rd32(m, esi + 0x18u);
    if (fA_6cbb28 != fB_6cbb28) goto L_6cbb33;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x6cbb33u);
L_6cbb33:
    fA_6cbb33 = rd32(m, ebp + 0x28u);
    fB_6cbb33 = 0x0u;
    if (int32_t(sext(fA_6cbb33, 4)) <= int32_t(sext(fB_6cbb33, 4))) goto L_6cbb3d;
    wr32(m, esp + 0xcu, (rd32(m, esp + 0xcu) + 1u));
L_6cbb3d:
    esi = (esp + 0x10u);
    LIFT_CALL(0x593dc0u, 0x6cbb46u);  // FUN_00593dc0
    ebp = rd32(m, esp + 0x14u);
    esi = rd32(m, esp + 0x10u);
    goto L_6cbb00;
L_6cbb50:
    esi = rd32(m, esi);
    goto L_6cbb28;
L_6cbb54:
    ecx = rd32(m, edi + 0x35cu);
    eax = rd32(m, ecx);
    esi = rd32(m, edi + 0x344u);
    wr32(m, esp + 0x10u, esi);
    wr32(m, esp + 0x14u, eax);
L_6cbb70:
    ebx = rd32(m, edi + 0x35cu);
    eax = rd32(m, edi + 0x344u);
    fR_6cbb7c = (esi & esi);
    if ((fR_6cbb7c == 0)) goto L_6cbb84;
    fA_6cbb80 = esi;
    fB_6cbb80 = eax;
    if (fA_6cbb80 == fB_6cbb80) goto L_6cbb8e;
L_6cbb84:
    ebp = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    LIFT_CALL(ebp, 0x6cbb8cu);
    goto L_6cbb94;
L_6cbb8e:
    ebp = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
L_6cbb94:
    fA_6cbb94 = rd32(m, esp + 0x14u);
    fB_6cbb94 = ebx;
    if (fA_6cbb94 == fB_6cbb94) goto L_6cbbcc;
    fR_6cbb9a = (esi & esi);
    if (!(fR_6cbb9a == 0)) goto L_6cbbc8;
    LIFT_CALL(ebp, 0x6cbba0u);
L_6cbba0:
    edx = rd32(m, esp + 0x14u);
    fA_6cbba4 = edx;
    fB_6cbba4 = rd32(m, esi + 0x18u);
    if (fA_6cbba4 != fB_6cbba4) goto L_6cbbab;
    LIFT_CALL(ebp, 0x6cbbabu);
L_6cbbab:
    eax = rd32(m, esp + 0x14u);
    fA_6cbbaf = rd32(m, eax + 0x28u);
    fB_6cbbaf = 0x0u;
    if (int32_t(sext(fA_6cbbaf, 4)) <= int32_t(sext(fB_6cbbaf, 4))) goto L_6cbbb9;
    wr32(m, esp + 0xcu, (rd32(m, esp + 0xcu) + 1u));
L_6cbbb9:
    esi = (esp + 0x10u);
    LIFT_CALL(0x593dc0u, 0x6cbbc2u);  // FUN_00593dc0
    esi = rd32(m, esp + 0x10u);
    goto L_6cbb70;
L_6cbbc8:
    esi = rd32(m, esi);
    goto L_6cbba0;
L_6cbbcc:
    eax = rd32(m, esp + 0xcu);
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x10u;
    LIFT_RET(0);
}

// 006cbbe0 Fn_006cbbe0
//   original FUN_006cbbe0; register inputs: ecx; returns with ret 12
REFLEX_PORT_FN(0x006cbbe0, Fn_006cbbe0, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_6cbbf4 = 0;
    uint32_t fB_6cbbf4 = 0;
    uint32_t fR_6cbc00 = 0;
    uint32_t fA_6cbc08 = 0;
    uint32_t fB_6cbc08 = 0;
    uint32_t fA_6cbc12 = 0;
    uint32_t fB_6cbc12 = 0;
    uint32_t fA_6cbc18 = 0;
    uint32_t fB_6cbc18 = 0;
    uint32_t fA_6cbc27 = 0;
    uint32_t fB_6cbc27 = 0;
    uint32_t fA_6cbc3f = 0;
    uint32_t fB_6cbc3f = 0;
    uint32_t fA_6cbc47 = 0;
    uint32_t fB_6cbc47 = 0;
    uint32_t fA_6cbc4b = 0;
    uint32_t fB_6cbc4b = 0;
    uint32_t fA_6cbc50 = 0;
    uint32_t fB_6cbc50 = 0;
    uint32_t fA_6cbc55 = 0;
    uint32_t fB_6cbc55 = 0;
    uint32_t fA_6cbc76 = 0;
    uint32_t fB_6cbc76 = 0;
    uint32_t fA_6cbc83 = 0;
    uint32_t fB_6cbc83 = 0;
    uint32_t fA_6cbc87 = 0;
    uint32_t fB_6cbc87 = 0;
    uint32_t fA_6cbc9f = 0;
    uint32_t fB_6cbc9f = 0;
    uint32_t fA_6cbcec = 0;
    uint32_t fB_6cbcec = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0x24u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    eax = rd32(m, esi + 0x128u);
    esp -= 4; wr32(m, esp, edi);
    fA_6cbbf4 = rd32(m, ebp + 0x8u);
    fB_6cbbf4 = eax;
    if (fA_6cbbf4 != fB_6cbbf4) goto L_6cbd91;
    ebx = rd32(m, ebp + 0xcu);
    fR_6cbc00 = (ebx & ebx);
    if ((fR_6cbc00 == 0)) goto L_6cbd91;
    fA_6cbc08 = rd32(m, eax + 0x4u);
    fB_6cbc08 = 0x0u;
    if (fA_6cbc08 == fB_6cbc08) goto L_6cbd91;
    fA_6cbc12 = rd32(m, eax + 0x10u);
    fB_6cbc12 = 0x0u;
    if (fA_6cbc12 == fB_6cbc12) goto L_6cbc32;
    fA_6cbc18 = rd32(m, eax + 0x4u);
    fB_6cbc18 = 0x0u;
    if (fA_6cbc18 == fB_6cbc18) goto L_6cbd91;
    edi = 0x1u;
    fA_6cbc27 = rd32(m, eax + 0x10u);
    fB_6cbc27 = edi;
    if (fA_6cbc27 != fB_6cbc27) goto L_6cbd91;
    goto L_6cbc37;
L_6cbc32:
    edi = 0x1u;
L_6cbc37:
    x0 = X{};
    wr32(m, esi + 0x58u, x0.l(0));
    fA_6cbc3f = rd32(m, esi + 0x60u);
    fB_6cbc3f = ebx;
    if (fA_6cbc3f != fB_6cbc3f) goto L_6cbc6d;
    eax = rd32(m, ebp + 0x10u);
    fA_6cbc47 = eax;
    fB_6cbc47 = edi;
    if (fA_6cbc47 == fB_6cbc47) goto L_6cbc6d;
    fA_6cbc4b = eax;
    fB_6cbc4b = 0x2u;
    if (fA_6cbc4b == fB_6cbc4b) goto L_6cbc6d;
    fA_6cbc50 = eax;
    fB_6cbc50 = 0x4u;
    if (fA_6cbc50 == fB_6cbc50) goto L_6cbc6d;
    fA_6cbc55 = eax;
    fB_6cbc55 = 0x3u;
    if (fA_6cbc55 == fB_6cbc55) goto L_6cbc6d;
    fpush(m, reflex_f80::from_f32(rd32(m, 0x9c3e70u)));
    esp -= 4; wr32(m, esp, ecx);
    eax = esi;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x6ccf60u, 0x6cbc6bu);  // FUN_006ccf60
    goto L_6cbc70;
L_6cbc6d:
    wr32(m, esi + 0x60u, ebx);
L_6cbc70:
    eax = rd32(m, esi + 0x124u);
    fA_6cbc76 = eax;
    fB_6cbc76 = 0x0u;
    eax = fA_6cbc76 - fB_6cbc76;
    wr32(m, esp + 0x10u, 0x0u);
    if (fA_6cbc76 == fB_6cbc76) goto L_6cbc98;
    fA_6cbc83 = eax;
    fB_6cbc83 = edi;
    eax = fA_6cbc83 - fB_6cbc83;
    if (fA_6cbc83 == fB_6cbc83) goto L_6cbc92;
    fA_6cbc87 = eax;
    fB_6cbc87 = edi;
    eax = fA_6cbc87 - fB_6cbc87;
    if (fA_6cbc87 != fB_6cbc87) goto L_6cbc9f;
    eax = esi;
    LIFT_CALL(0x6cb390u, 0x6cbc92u);  // FUN_006cb390
L_6cbc92:
    wr32(m, esp + 0x10u, edi);
    goto L_6cbc9f;
L_6cbc98:
    edx = esi;
    LIFT_CALL(0x6cb180u, 0x6cbc9fu);  // FUN_006cb180
L_6cbc9f:
    fA_6cbc9f = rd32(m, ebx + 0x64u);
    fB_6cbc9f = 0x0u;
    esp -= 4; wr32(m, esp, ebx);
    ecx = (esp + 0x18u);
    if (fA_6cbc9f == fB_6cbc9f) goto L_6cbcbd;
    LIFT_CALL(rd32(m, 0x9161acu) /* 8.280765882754786e-39 */, 0x6cbcb0u);
    eax = (esp + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    edi = (esi + 0x324u);
    goto L_6cbcce;
L_6cbcbd:
    LIFT_CALL(rd32(m, 0x9161acu) /* 8.280765882754786e-39 */, 0x6cbcc3u);
    ecx = (esp + 0x14u);
    esp -= 4; wr32(m, esp, ecx);
    edi = (esi + 0x344u);
L_6cbcce:
    LIFT_CALL(0x6cd3f0u, 0x6cbcd3u);  // FUN_006cd3f0
    wr32(m, eax, (rd32(m, eax) + 1u));
    ecx = (esp + 0x14u);
    LIFT_CALL(rd32(m, 0x9161b4u) /* 8.28055008279128e-39 */, 0x6cbcdfu);
    edi = ebx;
    eax = esi;
    LIFT_CALL(0x6cc1f0u, 0x6cbce8u);  // FUN_006cc1f0
    eax = rd32(m, ebp + 0x10u);
    eax = (eax - 1u);
    fA_6cbcec = eax;
    fB_6cbcec = 0x3u;
    if (fA_6cbcec > fB_6cbcec) goto L_6cbd6b;
    switch (eax) { case 0: goto L_6cbcf8; case 1: goto L_6cbd25; case 2: goto L_6cbd55; case 3: goto L_6cbd61; default: throw std::runtime_error("jump table"); }
L_6cbcf8:
    edx = rd32(m, esi + 0x128u);
    eax = 0x1u;
    wr32(m, esi + 0x2e0u, eax);
    wr32(m, esi + 0x2dcu, eax);
    ecx = rd32(m, edx + 0x1acu);
    eax = rd32(m, ecx);
    eax = rd32(m, eax + 0x44u);
    edx = (esi + 0x2f0u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(eax, 0x6cbd23u);
    goto L_6cbd6b;
L_6cbd25:
    ecx = rd32(m, esi + 0x128u);
    wr32(m, esi + 0x2e0u, 0x0u);
    wr32(m, esi + 0x2dcu, 0x1u);
    ecx = rd32(m, ecx + 0x1acu);
    edx = rd32(m, ecx);
    edx = rd32(m, edx + 0x44u);
    eax = (esi + 0x2f0u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(edx, 0x6cbd53u);
    goto L_6cbd6b;
L_6cbd55:
    wr32(m, esi + 0x300u, 0x0u);
    goto L_6cbd6b;
L_6cbd61:
    wr32(m, esi + 0x304u, 0x0u);
L_6cbd6b:
    eax = ebx;
    ecx = esi;
    LIFT_CALL(0x6cb960u, 0x6cbd74u);  // FUN_006cb960
    ecx = rd32(m, esi + 0x128u);
    esi = rd32(m, 0xa94318u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, 0x92fd00u);
    edx = eax;
    LIFT_CALL(0x58d7d0u, 0x6cbd91u);  // FUN_0058d7d0
L_6cbd91:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(12);
}

// 006cbdb0 Fn_006cbdb0
//   original FUN_006cbdb0; register inputs: ecx; returns with ret 12
REFLEX_PORT_FN(0x006cbdb0, Fn_006cbdb0, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_6cbdc7 = 0;
    uint32_t fB_6cbdc7 = 0;
    uint32_t fR_6cbdd2 = 0;
    uint32_t fA_6cbdda = 0;
    uint32_t fB_6cbdda = 0;
    uint32_t fA_6cbde4 = 0;
    uint32_t fB_6cbde4 = 0;
    uint32_t fA_6cbdea = 0;
    uint32_t fB_6cbdea = 0;
    uint32_t fA_6cbdf4 = 0;
    uint32_t fB_6cbdf4 = 0;
    uint32_t fA_6cbe26 = 0;
    uint32_t fB_6cbe26 = 0;
    uint32_t fA_6cbe30 = 0;
    uint32_t fB_6cbe30 = 0;
    uint32_t fA_6cbe3a = 0;
    uint32_t fB_6cbe3a = 0;
    uint32_t fA_6cbec0 = 0;
    uint32_t fB_6cbec0 = 0;
    FCmp fC_6cbef9{};
    FCmp fC_6cbf3c{};
    uint32_t fA_6cbf7a = 0;
    uint32_t fB_6cbf7a = 0;
    uint32_t fA_6cbf87 = 0;
    uint32_t fB_6cbf87 = 0;
    uint32_t fA_6cbf93 = 0;
    uint32_t fB_6cbf93 = 0;
    uint32_t fA_6cbfc5 = 0;
    uint32_t fB_6cbfc5 = 0;
    uint32_t fA_6cbfd2 = 0;
    uint32_t fB_6cbfd2 = 0;
    uint32_t FL = 0;
    uint32_t fR_6cbfd6 = 0;
    uint32_t fA_6cbfdd = 0;
    uint32_t fB_6cbfdd = 0;
    uint32_t fR_6cbfe8 = 0;
    uint32_t fA_6cbff0 = 0;
    uint32_t fB_6cbff0 = 0;
    uint32_t fK_6cbff0 = 0;
    uint32_t fR_6cbff5 = 0;
    uint32_t fA_6cc002 = 0;
    uint32_t fB_6cc002 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x14u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    eax = rd32(m, esi + 0x128u);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, ebp + 0x8u);
    fA_6cbdc7 = edi;
    fB_6cbdc7 = eax;
    if (fA_6cbdc7 != fB_6cbdc7) goto L_6cc034;
    ebx = rd32(m, ebp + 0xcu);
    fR_6cbdd2 = (ebx & ebx);
    if ((fR_6cbdd2 == 0)) goto L_6cc034;
    fA_6cbdda = rd32(m, eax + 0x4u);
    fB_6cbdda = 0x0u;
    if (fA_6cbdda == fB_6cbdda) goto L_6cc034;
    fA_6cbde4 = rd32(m, eax + 0x10u);
    fB_6cbde4 = 0x0u;
    if (fA_6cbde4 == fB_6cbde4) goto L_6cbdfe;
    fA_6cbdea = rd32(m, eax + 0x4u);
    fB_6cbdea = 0x0u;
    if (fA_6cbdea == fB_6cbdea) goto L_6cc034;
    fA_6cbdf4 = rd32(m, eax + 0x10u);
    fB_6cbdf4 = 0x1u;
    if (fA_6cbdf4 != fB_6cbdf4) goto L_6cc034;
L_6cbdfe:
    wr32(m, esi + 0x418u, (rd32(m, esi + 0x418u) + 1u));
    fpush(m, reflex_f80::from_int(0));
    x0 = movss_m(rd32(m, ebx + 0x78u));
    x0 = addss(x0, rd32(m, esi + 0x78u));
    esp -= 4; wr32(m, esp, ecx);
    edx = ebx;
    wr32(m, esp, fst32(m));
    fpop(m);
    wr32(m, esi + 0x78u, x0.l(0));
    LIFT_CALL(0x6cb500u, 0x6cbe20u);  // FUN_006cb500
    eax = rd32(m, esi + 0x128u);
    fA_6cbe26 = rd32(m, eax + 0x4u);
    fB_6cbe26 = 0x0u;
    if (fA_6cbe26 == fB_6cbe26) goto L_6cbf60;
    fA_6cbe30 = rd32(m, eax + 0x10u);
    fB_6cbe30 = 0x0u;
    if (fA_6cbe30 != fB_6cbe30) goto L_6cbf60;
    fA_6cbe3a = rd32(m, esi + 0x2dcu);
    fB_6cbe3a = 0x0u;
    if (fA_6cbe3a == fB_6cbe3a) goto L_6cbf60;
    ecx = rd32(m, edi + 0x1acu);
    eax = rd32(m, ecx);
    eax = rd32(m, eax + 0x44u);
    edx = (esp + 0x10u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(eax, 0x6cbe59u);
    x2 = movss_m(rd32(m, esp + 0x18u));
    x2 = subss(x2, rd32(m, esi + 0x2f8u));
    x1 = movss_m(rd32(m, esp + 0x14u));
    x1 = subss(x1, rd32(m, esi + 0x2f4u));
    x0 = movss_m(rd32(m, esp + 0x10u));
    x0 = subss(x0, rd32(m, esi + 0x2f0u));
    x3 = x2;
    x3 = mulss(x3, x2.l(0));
    wr32(m, esp + 0x18u, x2.l(0));
    x2 = x1;
    x2 = mulss(x2, x1.l(0));
    wr32(m, esp + 0x14u, x1.l(0));
    x1 = x0;
    x3 = addss(x3, x2.l(0));
    x1 = mulss(x1, x0.l(0));
    esp -= 4; wr32(m, esp, ecx);
    x3 = addss(x3, x1.l(0));
    wr32(m, esp + 0x14u, x0.l(0));
    wr32(m, esp, x3.l(0));
    LIFT_CALL(0x42c230u, 0x6cbebdu);  // FUN_0042c230
    esp = esp + 0x4u;
    fA_6cbec0 = rd32(m, esi + 0x2e0u);
    fB_6cbec0 = 0x0u;
    wr32(m, esp + 0xcu, x0.l(0));
    wr32(m, esi + 0x308u, x0.l(0));
    if (fA_6cbec0 == fB_6cbec0) goto L_6cbf1a;
    edx = rd32(m, esi + 0x128u);
    eax = rd32(m, edx + 0x1f0u);
    ecx = cvttss2si(x0.l(0));
    esp -= 4; wr32(m, esp, ecx);
    ecx = rd32(m, 0xa946c0u);
    LIFT_CALL(0x764b90u, 0x6cbef3u);  // MXRecords_LongestWheelie
    x0 = movss_m(rd32(m, esp + 0xcu));
    fC_6cbef9 = fcmp32(x0.l(0), rd32(m, 0x9d7b34u));
    if (fc_b(fC_6cbef9)) goto L_6cbf60;
    eax = rd32(m, esi + 0x128u);
    ecx = rd32(m, eax + 0x1f0u);
    edx = rd32(m, 0xd92edcu);
    esp -= 4; wr32(m, esp, 0x2eu);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edx);
    goto L_6cbf5b;
L_6cbf1a:
    ecx = rd32(m, esi + 0x128u);
    eax = cvttss2si(x0.l(0));
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, ecx + 0x1f0u);
    ecx = rd32(m, 0xa946c0u);
    LIFT_CALL(0x764c20u, 0x6cbf36u);  // MXRecords_LongestStoppie
    x0 = movss_m(rd32(m, esp + 0xcu));
    fC_6cbf3c = fcmp32(x0.l(0), rd32(m, 0x977b0cu) /* 75.0 */);
    if (fc_b(fC_6cbf3c)) goto L_6cbf60;
    edx = rd32(m, esi + 0x128u);
    eax = rd32(m, edx + 0x1f0u);
    ecx = rd32(m, 0xd92edcu);
    esp -= 4; wr32(m, esp, 0x2fu);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
L_6cbf5b:
    LIFT_CALL(0x6d7980u, 0x6cbf60u);  // MXHighlightTracking_AddProgress
L_6cbf60:
    edx = rd32(m, esi + 0x128u);
    eax = 0u;
    wr32(m, esi + 0x2dcu, eax);
    wr32(m, esi + 0x300u, eax);
    wr32(m, esi + 0x304u, eax);
    fA_6cbf7a = rd32(m, edx + 0xb74u);
    fB_6cbf7a = eax;
    if (fA_6cbf7a != fB_6cbf7a) goto L_6cbf87;
    LIFT_CALL(0x6cc040u, 0x6cbf87u);  // FUN_006cc040
L_6cbf87:
    fA_6cbf87 = rd32(m, ebx + 0x64u);
    fB_6cbf87 = 0x0u;
    if (fA_6cbf87 == fB_6cbf87) goto L_6cbfaf;
    eax = rd32(m, esi + 0x128u);
    fA_6cbf93 = rd32(m, eax + 0x1f0u);
    fB_6cbf93 = 0xffffffffu;
    if (int32_t(sext(fA_6cbf93, 4)) <= int32_t(sext(fB_6cbf93, 4))) goto L_6cbfaf;
    ecx = ebx;
    eax = esi;
    wr32(m, esi + 0x41cu, 0x1u);
    LIFT_CALL(0x6ccef0u, 0x6cbfafu);  // FUN_006ccef0
L_6cbfaf:
    ecx = (esi + 0x404u);
    edi = 0u;
    ebx = (esi + 0x364u);
    wr32(m, esp + 0xcu, ecx);
L_6cbfc1:
    edx = rd32(m, esp + 0xcu);
    fA_6cbfc5 = rd32(m, edx);
    fB_6cbfc5 = 0x0u;
    eax = rd32(m, ebp + 0xcu);
    if (fA_6cbfc5 == fB_6cbfc5) goto L_6cc010;
    ecx = ebx;
L_6cbfd0:
    edx = (edx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    fA_6cbfd2 = (edx & 0xffu);
    fB_6cbfd2 = rd8(m, ecx);
    FL = fl_sub(fA_6cbfd2, fB_6cbfd2, 1);
    if (fA_6cbfd2 != fB_6cbfd2) goto L_6cbff0;
    fR_6cbfd6 = ((edx & 0xffu) & (edx & 0xffu));
    if ((fR_6cbfd6 == 0)) goto L_6cbfec;
    edx = (edx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    fA_6cbfdd = (edx & 0xffu);
    fB_6cbfdd = rd8(m, ecx + 0x1u);
    FL = fl_sub(fA_6cbfdd, fB_6cbfdd, 1);
    if (fA_6cbfdd != fB_6cbfdd) goto L_6cbff0;
    eax = eax + 0x2u;
    ecx = ecx + 0x2u;
    fR_6cbfe8 = ((edx & 0xffu) & (edx & 0xffu));
    if (!(fR_6cbfe8 == 0)) goto L_6cbfd0;
L_6cbfec:
    eax = 0u;
    goto L_6cbff5;
L_6cbff0:
    fA_6cbff0 = eax; fB_6cbff0 = eax; fK_6cbff0 = (fl_cc(2, FL)) ? 1u : 0u;
    eax = fA_6cbff0 - fB_6cbff0 - fK_6cbff0;
    eax = eax - 0xffffffffu - ((uint64_t(fA_6cbff0) < uint64_t(fB_6cbff0) + uint64_t(fK_6cbff0)) ? 1u : 0u);
L_6cbff5:
    fR_6cbff5 = (eax & eax);
    if ((fR_6cbff5 == 0)) goto L_6cc034;
    wr32(m, esp + 0xcu, rd32(m, esp + 0xcu) + 0x4u);
    edi = (edi + 1u);
    ebx = ebx + 0x20u;
    fA_6cc002 = edi;
    fB_6cc002 = 0x5u;
    if (int32_t(sext(fA_6cc002, 4)) < int32_t(sext(fB_6cc002, 4))) goto L_6cbfc1;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(12);
L_6cc010:
    ecx = edi;
    esp -= 4; wr32(m, esp, 0x20u);
    ecx = (ecx << 5);
    esp -= 4; wr32(m, esp, eax);
    edx = (ecx + esi + 0x364u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(rd32(m, 0x916260u) /* 8.286626112932593e-39 */, 0x6cc026u);
    esp = esp + 0xcu;
    wr32(m, esi + edi * 4 + 0x404u, 0x1u);
L_6cc034:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(12);
}

// 006cc040 Fn_006cc040
//   original FUN_006cc040; register inputs: ecx esi; returns with ret 0
REFLEX_PORT_FN(0x006cc040, Fn_006cc040, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_6cc041 = 0;
    uint32_t fB_6cc041 = 0;
    FCmp fC_6cc06e{};
    esp -= 4; wr32(m, esp, ecx);
    fA_6cc041 = rd32(m, esi + 0x2d8u);
    fB_6cc041 = 0x0u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, edi);
    if (fA_6cc041 != fB_6cc041) goto L_6cc056;
    x0 = movss_m(rd32(m, 0x9c3e3cu));
    goto L_6cc066;
L_6cc056:
    x0 = movss_m(rd32(m, esi + 0x2d4u));
    x0 = mulss(x0, rd32(m, 0x9c3e40u));
L_6cc066:
    x1 = movss_m(rd32(m, 0x977470u) /* 0.03333333507180214 */);
    fC_6cc06e = fcmp32(x1.l(0), x0.l(0));
    wr32(m, esi + 0x2d0u, x0.l(0));
    if ((fc_b(fC_6cc06e) || fc_z(fC_6cc06e))) goto L_6cc087;
    ecx = esi;
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x4u;
    LIFT_TAILJMP(0x6caaf0u);  // FUN_006caaf0
L_6cc087:
    eax = rd32(m, esi + 0x128u);
    ebx = rd32(m, 0xa94318u);
    esp -= 4; wr32(m, esp, 0x9e1a7cu);
    ecx = 0x9cbeb4u;
    wr32(m, esi + 0x2d4u, x0.l(0));
    wr32(m, esp + 0xcu, eax);
    LIFT_CALL(rd32(m, 0x916388u) /* 8.287789190657982e-39 */, 0x6cc0afu);
    esp -= 4; wr32(m, esp, 0x4u);
    ecx = (esp + 0xcu);
    esp -= 4; wr32(m, esp, ecx);
    edi = 0x92fcc0u;
    eax = ebx;
    LIFT_CALL(0x4020c0u, 0x6cc0c2u);  // FUN_004020c0
    edi = rd32(m, esp); esp += 4;
    wr32(m, esi + 0x124u, 0x2u);
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 006cc0d0 Fn_006cc0d0
//   original FUN_006cc0d0; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x006cc0d0, Fn_006cc0d0, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_6cc0da = 0;
    uint32_t fB_6cc0da = 0;
    uint32_t fA_6cc0e4 = 0;
    uint32_t fB_6cc0e4 = 0;
    uint32_t fA_6cc0ee = 0;
    uint32_t fB_6cc0ee = 0;
    uint32_t fA_6cc0f4 = 0;
    uint32_t fB_6cc0f4 = 0;
    uint32_t fA_6cc0fe = 0;
    uint32_t fB_6cc0fe = 0;
    uint32_t fR_6cc10b = 0;
    FCmp fC_6cc114{};
    uint32_t fA_6cc123 = 0;
    uint32_t fB_6cc123 = 0;
    uint32_t fR_6cc140 = 0;
    uint32_t fA_6cc144 = 0;
    uint32_t fB_6cc144 = 0;
    uint32_t fA_6cc14a = 0;
    uint32_t fB_6cc14a = 0;
    uint32_t fA_6cc150 = 0;
    uint32_t fB_6cc150 = 0;
    uint32_t fR_6cc160 = 0;
    uint32_t fA_6cc17e = 0;
    uint32_t fB_6cc17e = 0;
    uint32_t fA_6cc183 = 0;
    uint32_t fB_6cc183 = 0;
    uint32_t fA_6cc18b = 0;
    uint32_t fB_6cc18b = 0;
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esi + 0x128u);
    fA_6cc0da = rd32(m, esp + 0xcu);
    fB_6cc0da = edi;
    if (fA_6cc0da != fB_6cc0da) goto L_6cc198;
    fA_6cc0e4 = rd32(m, edi + 0x4u);
    fB_6cc0e4 = 0x0u;
    if (fA_6cc0e4 == fB_6cc0e4) goto L_6cc198;
    fA_6cc0ee = rd32(m, edi + 0x10u);
    fB_6cc0ee = 0x0u;
    if (fA_6cc0ee == fB_6cc0ee) goto L_6cc108;
    fA_6cc0f4 = rd32(m, edi + 0x4u);
    fB_6cc0f4 = 0x0u;
    if (fA_6cc0f4 == fB_6cc0f4) goto L_6cc198;
    fA_6cc0fe = rd32(m, edi + 0x10u);
    fB_6cc0fe = 0x1u;
    if (fA_6cc0fe != fB_6cc0fe) goto L_6cc198;
L_6cc108:
    eax = rd32(m, esi + 0x5cu);
    fR_6cc10b = (eax & eax);
    if (!(fR_6cc10b == 0)) goto L_6cc144;
    x0 = movss_m(rd32(m, esi + 0x74u));
    fC_6cc114 = fcmp32(x0.l(0), rd32(m, 0x931f24u) /* 1.0 */);
    if ((fc_b(fC_6cc114) || fc_z(fC_6cc114))) goto L_6cc140;
    eax = rd32(m, edi + 0x94u);
    fA_6cc123 = rd32(m, eax + 0x21cu);
    fB_6cc123 = 0x2u;
    if (fA_6cc123 != fB_6cc123) goto L_6cc178;
    x0 = movss_m(rd32(m, esi + 0x30u));
    x0 = addss(x0, rd32(m, 0x9c3e58u));
    wr32(m, esi + 0x30u, x0.l(0));
    goto L_6cc178;
L_6cc140:
    fR_6cc140 = (eax & eax);
    if ((fR_6cc140 == 0)) goto L_6cc178;
L_6cc144:
    fA_6cc144 = rd32(m, edi + 0x4u);
    fB_6cc144 = 0x0u;
    if (fA_6cc144 == fB_6cc144) goto L_6cc178;
    fA_6cc14a = rd32(m, edi + 0x10u);
    fB_6cc14a = 0x0u;
    if (fA_6cc14a != fB_6cc14a) goto L_6cc178;
    fA_6cc150 = rd32(m, edi + 0x1f0u);
    fB_6cc150 = 0x0u;
    if (int32_t(sext(fA_6cc150, 4)) < int32_t(sext(fB_6cc150, 4))) goto L_6cc178;
    ecx = edi;
    LIFT_CALL(0x551070u, 0x6cc160u);  // FUN_00551070
    fR_6cc160 = (eax & eax);
    if (!(fR_6cc160 == 0)) goto L_6cc178;
    wr32(m, esi + 0x41cu, 0x1u);
    wr32(m, esi + 0x420u, 0x1u);
L_6cc178:
    eax = rd32(m, esi + 0x124u);
    fA_6cc17e = eax;
    fB_6cc17e = 0x1u;
    eax = fA_6cc17e - fB_6cc17e;
    if (fA_6cc17e != fB_6cc17e) goto L_6cc198;
    fA_6cc183 = rd32(m, edi + 0xbc0u);
    fB_6cc183 = eax;
    if (fA_6cc183 != fB_6cc183) goto L_6cc198;
    fA_6cc18b = rd32(m, edi + 0xb74u);
    fB_6cc18b = eax;
    if (fA_6cc18b != fB_6cc18b) goto L_6cc198;
    LIFT_CALL(0x6cc040u, 0x6cc198u);  // FUN_006cc040
L_6cc198:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 006cc1f0 Fn_006cc1f0
//   original FUN_006cc1f0; register inputs: ecx eax edi; returns with ret 0
REFLEX_PORT_FN(0x006cc1f0, Fn_006cc1f0, "fx", 0) {
    LIFT_ENTRY();
    uint32_t fR_6cc200 = 0;
    uint32_t fA_6cc206 = 0;
    uint32_t fB_6cc206 = 0;
    uint32_t fR_6cc213 = 0;
    uint32_t fA_6cc220 = 0;
    uint32_t fB_6cc220 = 0;
    uint32_t fR_6cc227 = 0;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    ebx = 0u;
    edx = 0u;
    esi = 0u;
    eax = eax + 0x140u;
L_6cc200:
    fR_6cc200 = (edx & edx);
    if (!(fR_6cc200 == 0)) goto L_6cc225;
    ecx = rd32(m, eax);
    fA_6cc206 = ecx;
    fB_6cc206 = edi;
    if (fA_6cc206 != fB_6cc206) goto L_6cc213;
    ebx = 0x1u;
    edx = ebx;
    goto L_6cc21c;
L_6cc213:
    fR_6cc213 = (ecx & ecx);
    if (!(fR_6cc213 == 0)) goto L_6cc21c;
    wr32(m, eax, edi);
    edx = (ecx + 0x1u);
L_6cc21c:
    esi = (esi + 1u);
    eax = eax + 0x4u;
    fA_6cc220 = esi;
    fB_6cc220 = 0x64u;
    if (int32_t(sext(fA_6cc220, 4)) < int32_t(sext(fB_6cc220, 4))) goto L_6cc200;
L_6cc225:
    eax = 0u;
    fR_6cc227 = (ebx & ebx);
    esi = rd32(m, esp); esp += 4;
    eax = (eax & 0xffffff00u) | ((((fR_6cc227 == 0)) ? 1u : 0u) & 0xffu);
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 006cc4b0 Fn_006cc4b0
//   original FUN_006cc4b0; register inputs: eax; returns with ret 0
REFLEX_PORT_FN(0x006cc4b0, Fn_006cc4b0, "fx", reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fR_6cc4c3 = 0;
    FCmp fC_6cc4f4{};
    FCmp fC_6cc503{};
    FCmp fC_6cc546{};
    FCmp fC_6cc550{};
    FCmp fC_6cc581{};
    FCmp fC_6cc58b{};
    FCmp fC_6cc5b2{};
    FCmp fC_6cc5de{};
    FCmp fC_6cc5e6{};
    FCmp fC_6cc603{};
    FCmp fC_6cc64d{};
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0x8u;
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    eax = rd32(m, esi + 0x418u);
    esp -= 4; wr32(m, esp, edi);
    fR_6cc4c3 = (eax & eax);
    if ((fR_6cc4c3 == 0)) goto L_6cc640;
    x6 = cvtsi2ss(x6, rd32(m, 0xa9431cu));
    x3 = X{};
    x7 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x2 = cvtsi2ss(x2, eax);
    x1 = x6;
    x1 = mulss(x1, rd32(m, 0x935860u) /* 0.4000000059604645 */);
    x0 = x2;
    x0 = divss(x0, x1.l(0));
    fC_6cc4f4 = fcmp32(x3.l(0), x0.l(0));
    if (fc_b(fC_6cc4f4)) goto L_6cc503;
    x1 = movss_m(rd32(m, 0x92e330u) /* 2.0 */);
    goto L_6cc51d;
L_6cc503:
    fC_6cc503 = fcmp32(x0.l(0), x7.l(0));
    if (fc_b(fC_6cc503)) goto L_6cc50d;
    x1 = x3;
    goto L_6cc51d;
L_6cc50d:
    x1 = movss_m(rd32(m, 0x92e330u) /* 2.0 */);
    x0 = mulss(x0, x1.l(0));
    x1 = subss(x1, x0.l(0));
L_6cc51d:
    x0 = movss_m(rd32(m, esi + 0x78u));
    x5 = movss_m(rd32(m, 0x977650u) /* 5.5 */);
    x4 = movss_m(rd32(m, 0x9cd9e4u));
    x0 = divss(x0, x2.l(0));
    x0 = subss(x0, x1.l(0));
    x1 = movss_m(rd32(m, 0x931d94u) /* 0.10000000149011612 */);
    x0 = mulss(x0, x1.l(0));
    fC_6cc546 = fcmp32(x3.l(0), x0.l(0));
    if (fc_b(fC_6cc546)) goto L_6cc550;
    x0 = x4;
    goto L_6cc567;
L_6cc550:
    fC_6cc550 = fcmp32(x0.l(0), x7.l(0));
    if (fc_b(fC_6cc550)) goto L_6cc55f;
    x0 = movss_m(rd32(m, 0x92fd88u) /* 10.0 */);
    goto L_6cc567;
L_6cc55f:
    x0 = mulss(x0, x5.l(0));
    x0 = addss(x0, x4.l(0));
L_6cc567:
    wr32(m, esi + 0x4u, x0.l(0));
    x0 = sqrtss(x0, x2.l(0));
    x0 = mulss(x0, rd32(m, 0x9d789cu));
    x0 = subss(x0, rd32(m, esi + 0x2cu));
    x0 = mulss(x0, x1.l(0));
    fC_6cc581 = fcmp32(x3.l(0), x0.l(0));
    if (fc_b(fC_6cc581)) goto L_6cc58b;
    x0 = x4;
    goto L_6cc5a2;
L_6cc58b:
    fC_6cc58b = fcmp32(x0.l(0), x7.l(0));
    if (fc_b(fC_6cc58b)) goto L_6cc59a;
    x0 = movss_m(rd32(m, 0x92fd88u) /* 10.0 */);
    goto L_6cc5a2;
L_6cc59a:
    x0 = mulss(x0, x5.l(0));
    x0 = addss(x0, x4.l(0));
L_6cc5a2:
    x6 = mulss(x6, rd32(m, 0x9c4068u));
    wr32(m, esi + 0x8u, x0.l(0));
    x0 = x6;
    fC_6cc5b2 = fcmp32(x0.l(0), x7.l(0));
    if ((fc_b(fC_6cc5b2) || fc_z(fC_6cc5b2))) goto L_6cc5bf;
    wr32(m, esp + 0xcu, x0.l(0));
    goto L_6cc5c5;
L_6cc5bf:
    wr32(m, esp + 0xcu, x7.l(0));
L_6cc5c5:
    edi = esi;
    LIFT_CALL(0x6cbad0u, 0x6cc5ccu);  // FUN_006cbad0
    x1 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x0 = cvtsi2ss(x0, eax);
    x0 = divss(x0, rd32(m, esp + 0xcu));
    fC_6cc5de = fcmp32(x0.l(0), x1.l(0));
    if (!fc_b(fC_6cc5de)) goto L_6cc608;
    x2 = X{};
    fC_6cc5e6 = fcmp32(x2.l(0), x0.l(0));
    if (fc_b(fC_6cc5e6)) goto L_6cc603;
    x0 = movss_m(rd32(m, 0x9cd9e4u));
    wr32(m, esi + 0xcu, x0.l(0));
    LIFT_CALL(0x6cc670u, 0x6cc5fdu);  // FUN_006cc670
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_6cc603:
    fC_6cc603 = fcmp32(x0.l(0), x1.l(0));
    if (fc_b(fC_6cc603)) goto L_6cc620;
L_6cc608:
    x0 = movss_m(rd32(m, 0x92fd88u) /* 10.0 */);
    wr32(m, esi + 0xcu, x0.l(0));
    LIFT_CALL(0x6cc670u, 0x6cc61au);  // FUN_006cc670
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_6cc620:
    x0 = mulss(x0, rd32(m, 0x977650u) /* 5.5 */);
    x0 = addss(x0, rd32(m, 0x9cd9e4u));
    wr32(m, esi + 0xcu, x0.l(0));
    LIFT_CALL(0x6cc670u, 0x6cc63au);  // FUN_006cc670
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_6cc640:
    LIFT_CALL(0x6cc670u, 0x6cc645u);  // FUN_006cc670
    x1 = movss_m(rd32(m, esi + 0x10u));
    x0 = X{};
    fC_6cc64d = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_6cc64d) || fc_z(fC_6cc64d))) goto L_6cc65a;
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
L_6cc65a:
    edi = rd32(m, esp); esp += 4;
    wr32(m, esi + 0xcu, x0.l(0));
    wr32(m, esi + 0x8u, x0.l(0));
    wr32(m, esi + 0x4u, x0.l(0));
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 006cc670 Fn_006cc670
//   original FUN_006cc670; register inputs: esi; returns with ret 0
REFLEX_PORT_FN(0x006cc670, Fn_006cc670, "fx", reflex_port::OUT_ECX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fR_6cc677 = 0;
    uint32_t fA_6cc690 = 0;
    uint32_t fB_6cc690 = 0;
    uint32_t fA_6cc697 = 0;
    uint32_t fB_6cc697 = 0;
    uint32_t fA_6cc69d = 0;
    uint32_t fB_6cc69d = 0;
    uint32_t fA_6cc6a4 = 0;
    uint32_t fB_6cc6a4 = 0;
    uint32_t fA_6cc6ab = 0;
    uint32_t fB_6cc6ab = 0;
    uint32_t fA_6cc6b8 = 0;
    uint32_t fB_6cc6b8 = 0;
    uint32_t fR_6cc6bd = 0;
    FCmp fC_6cc6f1{};
    FCmp fC_6cc705{};
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, 0xa9431cu);
    fR_6cc677 = (edi & edi);
    if ((fR_6cc677 == 0)) goto L_6cc72f;
    ecx = 0u;
    edx = 0u;
    eax = (esi + 0x88u);
L_6cc690:
    fA_6cc690 = rd32(m, eax - 0x4u);
    fB_6cc690 = 0x0u;
    if (fA_6cc690 == fB_6cc690) goto L_6cc6bd;
    ecx = (ecx + 1u);
    fA_6cc697 = rd32(m, eax);
    fB_6cc697 = 0x0u;
    if (fA_6cc697 == fB_6cc697) goto L_6cc6bd;
    ecx = (ecx + 1u);
    fA_6cc69d = rd32(m, eax + 0x4u);
    fB_6cc69d = 0x0u;
    if (fA_6cc69d == fB_6cc69d) goto L_6cc6bd;
    ecx = (ecx + 1u);
    fA_6cc6a4 = rd32(m, eax + 0x8u);
    fB_6cc6a4 = 0x0u;
    if (fA_6cc6a4 == fB_6cc6a4) goto L_6cc6bd;
    ecx = (ecx + 1u);
    fA_6cc6ab = rd32(m, eax + 0xcu);
    fB_6cc6ab = 0x0u;
    if (fA_6cc6ab == fB_6cc6ab) goto L_6cc6bd;
    edx = edx + 0x5u;
    ecx = (ecx + 1u);
    eax = eax + 0x14u;
    fA_6cc6b8 = edx;
    fB_6cc6b8 = 0x28u;
    if (int32_t(sext(fA_6cc6b8, 4)) < int32_t(sext(fB_6cc6b8, 4))) goto L_6cc690;
L_6cc6bd:
    fR_6cc6bd = (ecx & ecx);
    if (((fR_6cc6bd == 0) || sgn(fR_6cc6bd, 4) != false)) goto L_6cc72f;
    x2 = X{};
    x1 = cvtsi2ss(x1, edi);
    x1 = mulss(x1, rd32(m, 0x9d78a0u));
    x0 = cvtsi2ss(x0, ecx);
    x0 = divss(x0, x1.l(0));
    x1 = movss_m(rd32(m, 0x92fd88u) /* 10.0 */);
    x0 = mulss(x0, x1.l(0));
    wr32(m, esi + 0x10u, x0.l(0));
    x0 = mulss(x0, rd32(m, 0x931d94u) /* 0.10000000149011612 */);
    fC_6cc6f1 = fcmp32(x2.l(0), x0.l(0));
    if (fc_b(fC_6cc6f1)) goto L_6cc705;
    x0 = movss_m(rd32(m, 0x9cd9e4u));
    wr32(m, esi + 0x10u, x0.l(0));
    edi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_6cc705:
    fC_6cc705 = fcmp32(x0.l(0), rd32(m, 0x931f24u) /* 1.0 */);
    if (fc_b(fC_6cc705)) goto L_6cc718;
    x0 = x1;
    wr32(m, esi + 0x10u, x0.l(0));
    edi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_6cc718:
    x0 = mulss(x0, rd32(m, 0x977650u) /* 5.5 */);
    x0 = addss(x0, rd32(m, 0x9cd9e4u));
    wr32(m, esi + 0x10u, x0.l(0));
    edi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_6cc72f:
    x0 = X{};
    wr32(m, esi + 0x10u, x0.l(0));
    edi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 006ccbc0 Fn_006ccbc0
//   original FUN_006ccbc0; register inputs: eax ebx; returns with ret 0
REFLEX_PORT_FN(0x006ccbc0, Fn_006ccbc0, "fx", 0) {
    LIFT_ENTRY();
    uint32_t fR_6ccc15 = 0;
    uint32_t fA_6ccc32 = 0;
    uint32_t fB_6ccc32 = 0;
    uint32_t FL = 0;
    uint32_t fR_6ccc36 = 0;
    uint32_t fA_6ccc3d = 0;
    uint32_t fB_6ccc3d = 0;
    uint32_t fR_6ccc48 = 0;
    uint32_t fA_6ccc50 = 0;
    uint32_t fB_6ccc50 = 0;
    uint32_t fK_6ccc50 = 0;
    uint32_t fR_6ccc55 = 0;
    uint32_t fA_6ccc5b = 0;
    uint32_t fB_6ccc5b = 0;
    esp = esp - 0x400u;
    ecx = rd32(m, eax + 0x234u);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, ecx + 0x20138u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, 0x3f8u);
    edx = (esp + 0x14u);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, edx);
    wr32(m, esp + 0x18u, 0x0u);
    LIFT_CALL(0x8fa06au, 0x6ccbedu);  // MSVCR90.DLL::memset
    esp = esp + 0xcu;
    eax = (esp + 0x8u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, 0xffu);
    ecx = (esp + 0x14u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, 0x1028u);
    eax = esi;
    LIFT_CALL(0x7d2920u, 0x6ccc0bu);  // FUN_007d2920
    edi = 0u;
    esi = 0u;
    wr32(m, 0xa9431cu, edi);
    fR_6ccc15 = (eax & eax);
    if ((false || (fR_6ccc15 == 0))) goto L_6ccc66;
    esp -= 4; wr32(m, esp, ebx);
L_6ccc20:
    ecx = rd32(m, esp + esi * 4 + 0x10u);
    edx = 0x960710u;
    ecx = ecx + 0xc0u;
L_6ccc30:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_6ccc32 = (ebx & 0xffu);
    fB_6ccc32 = rd8(m, edx);
    FL = fl_sub(fA_6ccc32, fB_6ccc32, 1);
    if (fA_6ccc32 != fB_6ccc32) goto L_6ccc50;
    fR_6ccc36 = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_6ccc36 == 0)) goto L_6ccc4c;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_6ccc3d = (ebx & 0xffu);
    fB_6ccc3d = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_6ccc3d, fB_6ccc3d, 1);
    if (fA_6ccc3d != fB_6ccc3d) goto L_6ccc50;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_6ccc48 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_6ccc48 == 0)) goto L_6ccc30;
L_6ccc4c:
    ecx = 0u;
    goto L_6ccc55;
L_6ccc50:
    fA_6ccc50 = ecx; fB_6ccc50 = ecx; fK_6ccc50 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_6ccc50 - fB_6ccc50 - fK_6ccc50;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_6ccc50) < uint64_t(fB_6ccc50) + uint64_t(fK_6ccc50)) ? 1u : 0u);
L_6ccc55:
    fR_6ccc55 = (ecx & ecx);
    if (!(fR_6ccc55 == 0)) goto L_6ccc5a;
    edi = (edi + 1u);
L_6ccc5a:
    esi = (esi + 1u);
    fA_6ccc5b = esi;
    fB_6ccc5b = eax;
    if (fA_6ccc5b < fB_6ccc5b) goto L_6ccc20;
    wr32(m, 0xa9431cu, edi);
    ebx = rd32(m, esp); esp += 4;
L_6ccc66:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    esp = esp + 0x400u;
    LIFT_RET(0);
}

// 006ccf60 Fn_006ccf60
//   original FUN_006ccf60; register inputs: eax; returns with ret 4
REFLEX_PORT_FN(0x006ccf60, Fn_006ccf60, "fx", reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fA_6ccf6c = 0;
    uint32_t fB_6ccf6c = 0;
    ecx = rd32(m, eax + 0x128u);
    edx = rd32(m, ecx + 0x94u);
    fA_6ccf6c = rd32(m, edx + 0x21cu);
    fB_6ccf6c = 0x2u;
    if (fA_6ccf6c != fB_6ccf6c) goto L_6ccf85;
    x0 = movss_m(rd32(m, eax + 0x30u));
    x0 = addss(x0, rd32(m, esp + 0x4u));
    wr32(m, eax + 0x30u, x0.l(0));
L_6ccf85:
    LIFT_RET(4);
}

// 006cd000 Fn_006cd000
//   original FUN_006cd000; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x006cd000, Fn_006cd000, "fx", reflex_port::OUT_EAX | reflex_port::OUT_EDX) {
    LIFT_ENTRY();
    uint32_t fA_6cd003 = 0;
    uint32_t fB_6cd003 = 0;
    ecx = (ecx - 1u);
    eax = 0u;
    fA_6cd003 = ecx;
    fB_6cd003 = 0x4u;
    if (fA_6cd003 > fB_6cd003) goto L_6cd02c;
    switch (ecx) { case 0: goto L_6cd00f; case 1: goto L_6cd015; case 2: goto L_6cd01b; case 3: goto L_6cd021; case 4: goto L_6cd027; default: throw std::runtime_error("jump table"); }
L_6cd00f:
    eax = 0x92fbf4u;
    LIFT_RET(0);
L_6cd015:
    eax = 0x92fbfcu;
    LIFT_RET(0);
L_6cd01b:
    eax = 0x92fc04u;
    LIFT_RET(0);
L_6cd021:
    eax = 0x92fc10u;
    LIFT_RET(0);
L_6cd027:
    eax = 0x92fc1cu;
L_6cd02c:
    LIFT_RET(0);
}

// 006cd3f0 Fn_006cd3f0
//   original FUN_006cd3f0; register inputs: edi; returns with ret 4
REFLEX_PORT_FN(0x006cd3f0, Fn_006cd3f0, "fx", reflex_port::OUT_EAX) {
    LIFT_ENTRY();
    uint32_t fR_6cd404 = 0;
    uint32_t fR_6cd417 = 0;
    uint32_t fA_6cd41b = 0;
    uint32_t fB_6cd41b = 0;
    uint32_t fA_6cd425 = 0;
    uint32_t fB_6cd425 = 0;
    uint32_t fR_6cd439 = 0;
    uint32_t fR_6cd471 = 0;
    uint32_t fA_6cd47b = 0;
    uint32_t fB_6cd47b = 0;
    esp = esp - 0x2cu;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x38u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, ebp);
    eax = edi;
    LIFT_CALL(0x8519a0u, 0x6cd402u);  // FUN_008519a0
    ebx = eax;
    fR_6cd404 = (edi & edi);
    if (!(fR_6cd404 == 0)) goto L_6cd40e;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x6cd40eu);
L_6cd40e:
    esi = rd32(m, edi);
    eax = rd32(m, edi + 0x18u);
    wr32(m, esp + 0x14u, eax);
    fR_6cd417 = (esi & esi);
    if ((fR_6cd417 == 0)) goto L_6cd41f;
    fA_6cd41b = esi;
    fB_6cd41b = esi;
    if (fA_6cd41b == fB_6cd41b) goto L_6cd425;
L_6cd41f:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x6cd425u);
L_6cd425:
    fA_6cd425 = ebx;
    fB_6cd425 = rd32(m, esp + 0x14u);
    if (fA_6cd425 == fB_6cd425) goto L_6cd43d;
    ecx = (ebx + 0xcu);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebp);
    LIFT_CALL(rd32(m, 0x9161bcu) /* 8.280267020501487e-39 */, 0x6cd436u);
    esp = esp + 0x8u;
    fR_6cd439 = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_6cd439 == 0)) goto L_6cd471;
L_6cd43d:
    esp -= 4; wr32(m, esp, ebp);
    ecx = (esp + 0x1cu);
    LIFT_CALL(rd32(m, 0x9161b0u) /* 8.28065377887764e-39 */, 0x6cd448u);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    edx = (esp + 0x20u);
    esp -= 4; wr32(m, esp, edx);
    eax = (esp + 0x1cu);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, edi);
    wr32(m, esp + 0x48u, 0x0u);
    LIFT_CALL(0x6cd4a0u, 0x6cd462u);  // FUN_006cd4a0
    esi = rd32(m, eax);
    ebx = rd32(m, eax + 0x4u);
    ecx = (esp + 0x18u);
    LIFT_CALL(rd32(m, 0x9161b4u) /* 8.28055008279128e-39 */, 0x6cd471u);
L_6cd471:
    fR_6cd471 = (esi & esi);
    if (!(fR_6cd471 == 0)) goto L_6cd492;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x6cd47bu);
L_6cd47b:
    fA_6cd47b = ebx;
    fB_6cd47b = rd32(m, esi + 0x18u);
    if (fA_6cd47b != fB_6cd47b) goto L_6cd486;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x6cd486u);
L_6cd486:
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = (ebx + 0x28u);
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x2cu;
    LIFT_RET(4);
L_6cd492:
    esi = rd32(m, esi);
    goto L_6cd47b;
}

// 006cd4a0 Fn_006cd4a0
//   original FUN_006cd4a0; register inputs: none; returns with ret 20
REFLEX_PORT_FN(0x006cd4a0, Fn_006cd4a0, "fx", reflex_port::OUT_EAX) {
    LIFT_ENTRY();
    uint32_t fR_6cd4b6 = 0;
    uint32_t fR_6cd4e9 = 0;
    uint32_t fA_6cd4ed = 0;
    uint32_t fB_6cd4ed = 0;
    uint32_t fA_6cd4fb = 0;
    uint32_t fB_6cd4fb = 0;
    uint32_t fR_6cd511 = 0;
    uint32_t fR_6cd539 = 0;
    uint32_t fA_6cd53d = 0;
    uint32_t fB_6cd53d = 0;
    uint32_t fA_6cd547 = 0;
    uint32_t fB_6cd547 = 0;
    uint32_t fR_6cd563 = 0;
    uint32_t fR_6cd59e = 0;
    uint32_t fR_6cd5cd = 0;
    uint32_t fA_6cd5d4 = 0;
    uint32_t fB_6cd5d4 = 0;
    uint32_t fR_6cd61d = 0;
    uint32_t fR_6cd65c = 0;
    uint32_t fR_6cd672 = 0;
    uint32_t fA_6cd67d = 0;
    uint32_t fB_6cd67d = 0;
    esp = esp - 0x18u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x20u);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x28u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    ecx = ebx;
    LIFT_CALL(0x608390u, 0x6cd4b6u);  // FUN_00608390
    fR_6cd4b6 = (eax & eax);
    if (!(fR_6cd4b6 == 0)) goto L_6cd4d8;
    eax = rd32(m, esp + 0x34u);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, ebx + 0x18u);
    esp -= 4; wr32(m, esp, 0x1u);
    esp -= 4; wr32(m, esp, ebp);
    edi = ebx;
    LIFT_CALL(0x5ba570u, 0x6cd4ccu);  // FUN_005ba570
    eax = ebp;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(20);
L_6cd4d8:
    ecx = rd32(m, ebx + 0x18u);
    eax = rd32(m, esp + 0x38u);
    esi = rd32(m, ecx);
    ecx = rd32(m, ebx);
    edi = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    fR_6cd4e9 = (eax & eax);
    if ((fR_6cd4e9 == 0)) goto L_6cd4f1;
    fA_6cd4ed = eax;
    fB_6cd4ed = ecx;
    if (fA_6cd4ed == fB_6cd4ed) goto L_6cd4f7;
L_6cd4f1:
    LIFT_CALL(edi, 0x6cd4f3u);
    eax = rd32(m, esp + 0x38u);
L_6cd4f7:
    edx = rd32(m, esp + 0x3cu);
    fA_6cd4fb = edx;
    fB_6cd4fb = esi;
    if (fA_6cd4fb != fB_6cd4fb) goto L_6cd534;
    esi = rd32(m, esp + 0x34u);
    edx = edx + 0xcu;
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(rd32(m, 0x9161bcu) /* 8.280267020501487e-39 */, 0x6cd50eu);
    esp = esp + 0x8u;
    fR_6cd511 = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_6cd511 == 0)) goto L_6cd6bd;
    eax = rd32(m, esp + 0x3cu);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, 0x1u);
    esp -= 4; wr32(m, esp, ebp);
    edi = ebx;
    LIFT_CALL(0x5ba570u, 0x6cd528u);  // FUN_005ba570
    eax = ebp;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(20);
L_6cd534:
    esi = rd32(m, ebx + 0x18u);
    ecx = rd32(m, ebx);
    fR_6cd539 = (eax & eax);
    if ((fR_6cd539 == 0)) goto L_6cd541;
    fA_6cd53d = eax;
    fB_6cd53d = ecx;
    if (fA_6cd53d == fB_6cd53d) goto L_6cd547;
L_6cd541:
    LIFT_CALL(edi, 0x6cd543u);
    edx = rd32(m, esp + 0x3cu);
L_6cd547:
    fA_6cd547 = edx;
    fB_6cd547 = esi;
    if (fA_6cd547 != fB_6cd547) goto L_6cd58c;
    edx = rd32(m, ebx + 0x18u);
    eax = rd32(m, edx + 0x8u);
    ecx = rd32(m, esp + 0x34u);
    eax = eax + 0xcu;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x9161bcu) /* 8.280267020501487e-39 */, 0x6cd560u);
    esp = esp + 0x8u;
    fR_6cd563 = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_6cd563 == 0)) goto L_6cd6b9;
    edx = rd32(m, esp + 0x34u);
    eax = rd32(m, ebx + 0x18u);
    eax = rd32(m, eax + 0x8u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, ebp);
    edi = ebx;
    LIFT_CALL(0x5ba570u, 0x6cd580u);  // FUN_005ba570
    eax = ebp;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(20);
L_6cd58c:
    edi = rd32(m, esp + 0x34u);
    edx = edx + 0xcu;
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(rd32(m, 0x9161bcu) /* 8.280267020501487e-39 */, 0x6cd59bu);
    esp = esp + 0x8u;
    fR_6cd59e = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_6cd59e == 0)) goto L_6cd60b;
    ecx = rd32(m, esp + 0x38u);
    edx = rd32(m, esp + 0x3cu);
    esi = (esp + 0x14u);
    wr32(m, esp + 0x14u, ecx);
    wr32(m, esp + 0x18u, edx);
    LIFT_CALL(0x63bf50u, 0x6cd5bbu);  // FUN_0063bf50
    esi = rd32(m, esp + 0x18u);
    eax = (esi + 0xcu);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x9161bcu) /* 8.280267020501487e-39 */, 0x6cd5cau);
    esp = esp + 0x8u;
    fR_6cd5cd = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_6cd5cd == 0)) goto L_6cd60b;
    ecx = rd32(m, esi + 0x8u);
    fA_6cd5d4 = rd8(m, ecx + 0x2du);
    fB_6cd5d4 = 0x0u;
    esp -= 4; wr32(m, esp, edi);
    edi = ebx;
    if (fA_6cd5d4 == fB_6cd5d4) goto L_6cd5f3;
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, ebp);
    eax = esi;
    LIFT_CALL(0x5ba570u, 0x6cd5e7u);  // FUN_005ba570
    eax = ebp;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(20);
L_6cd5f3:
    eax = rd32(m, esp + 0x40u);
    esp -= 4; wr32(m, esp, 0x1u);
    esp -= 4; wr32(m, esp, ebp);
    LIFT_CALL(0x5ba570u, 0x6cd5ffu);  // FUN_005ba570
    eax = ebp;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(20);
L_6cd60b:
    edx = rd32(m, esp + 0x3cu);
    edx = edx + 0xcu;
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(rd32(m, 0x9161bcu) /* 8.280267020501487e-39 */, 0x6cd61au);
    esp = esp + 0x8u;
    fR_6cd61d = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_6cd61d == 0)) goto L_6cd6b9;
    eax = rd32(m, esp + 0x38u);
    ecx = rd32(m, esp + 0x3cu);
    edx = rd32(m, ebx + 0x18u);
    wr32(m, esp + 0x14u, eax);
    eax = rd32(m, ebx);
    esi = (esp + 0x14u);
    wr32(m, esp + 0x18u, ecx);
    wr32(m, esp + 0x20u, edx);
    wr32(m, esp + 0x1cu, eax);
    LIFT_CALL(0x593dc0u, 0x6cd64bu);  // FUN_00593dc0
    esi = (esp + 0x1cu);
    edi = (esp + 0x14u);
    LIFT_CALL(0x4fd0c0u, 0x6cd658u);  // FUN_004fd0c0
    esi = rd32(m, esp + 0x18u);
    fR_6cd65c = ((eax & 0xffu) & (eax & 0xffu));
    if (!(fR_6cd65c == 0)) goto L_6cd676;
    edx = rd32(m, esp + 0x34u);
    ecx = (esi + 0xcu);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(rd32(m, 0x9161bcu) /* 8.280267020501487e-39 */, 0x6cd66fu);
    esp = esp + 0x8u;
    fR_6cd672 = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_6cd672 == 0)) goto L_6cd6b9;
L_6cd676:
    eax = rd32(m, esp + 0x3cu);
    ecx = rd32(m, eax + 0x8u);
    fA_6cd67d = rd8(m, ecx + 0x2du);
    fB_6cd67d = 0x0u;
    edi = ebx;
    if (fA_6cd67d == fB_6cd67d) goto L_6cd69e;
    edx = rd32(m, esp + 0x34u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, ebp);
    LIFT_CALL(0x5ba570u, 0x6cd692u);  // FUN_005ba570
    eax = ebp;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(20);
L_6cd69e:
    eax = rd32(m, esp + 0x34u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, 0x1u);
    esp -= 4; wr32(m, esp, ebp);
    eax = esi;
    LIFT_CALL(0x5ba570u, 0x6cd6adu);  // FUN_005ba570
    eax = ebp;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(20);
L_6cd6b9:
    esi = rd32(m, esp + 0x34u);
L_6cd6bd:
    esp -= 4; wr32(m, esp, esi);
    ecx = (esp + 0x20u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x5ba210u, 0x6cd6c9u);  // FUN_005ba210
    edx = rd32(m, eax);
    wr32(m, ebp, edx);
    eax = rd32(m, eax + 0x4u);
    edi = rd32(m, esp); esp += 4;
    wr32(m, ebp + 0x4u, eax);
    esi = rd32(m, esp); esp += 4;
    eax = ebp;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(20);
}

// 006cd750 Fn_006cd750
//   original FUN_006cd750; register inputs: ecx edx; returns with ret 4
REFLEX_PORT_FN(0x006cd750, Fn_006cd750, "fx", reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    esp = esp - 0x10u;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp + 0xcu, eax);
    eax = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, 0xa94304u);
    esp -= 4; wr32(m, esp, edi);
    wr32(m, esp + 0x10u, ecx);
    esp -= 4; wr32(m, esp, 0x9e1a7cu);
    ecx = 0x9cbf38u;
    edi = edx;
    wr32(m, esp + 0x10u, eax);
    LIFT_CALL(rd32(m, 0x916388u) /* 8.287789190657982e-39 */, 0x6cd784u);
    esp -= 4; wr32(m, esp, 0x8u);
    edx = (esp + 0x10u);
    esp -= 4; wr32(m, esp, edx);
    eax = esi;
    LIFT_CALL(0x4020c0u, 0x6cd792u);  // FUN_004020c0
    ecx = rd32(m, esp + 0x14u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x6cd79fu);  // __security_check_cookie
    esp = esp + 0x10u;
    LIFT_RET(4);
}

// 006cd810 Fn_006cd810
//   original FUN_006cd810; register inputs: ecx edx; returns with ret 8
REFLEX_PORT_FN(0x006cd810, Fn_006cd810, "fx", reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    esp = esp - 0x10u;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp + 0xcu, eax);
    eax = rd32(m, esp + 0x18u);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, 0xa94304u);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esp + 0x1cu);
    wr32(m, esp + 0xcu, ecx);
    esp -= 4; wr32(m, esp, 0x9e1a7cu);
    ecx = 0x9ce768u;
    wr32(m, esp + 0xcu, eax);
    wr32(m, esp + 0x14u, edx);
    LIFT_CALL(rd32(m, 0x916388u) /* 8.287789190657982e-39 */, 0x6cd84au);
    esp -= 4; wr32(m, esp, 0xcu);
    eax = (esp + 0xcu);
    esp -= 4; wr32(m, esp, eax);
    eax = esi;
    LIFT_CALL(0x4020c0u, 0x6cd858u);  // FUN_004020c0
    ecx = rd32(m, esp + 0x14u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x6cd865u);  // __security_check_cookie
    esp = esp + 0x10u;
    LIFT_RET(8);
}

// 006cfb30 LandAnimalProximity__vf22
//   original LandAnimalProximity::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x006cfb30, LandAnimalProximity__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x1057u;
    LIFT_RET(0);
}

// 006d07a0 LandAnimalProximity__vf29
//   original LandAnimalProximity::vf29; register inputs: ecx; returns with ret 16
REFLEX_PORT_FN(0x006d07a0, LandAnimalProximity__vf29, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_6d07b9 = 0;
    uint32_t fB_6d07b9 = 0;
    FCmp fC_6d07f0{};
    FCmp fC_6d0805{};
    FCmp fC_6d0814{};
    FCmp fC_6d0819{};
    uint32_t fA_6d0826 = 0;
    uint32_t fB_6d0826 = 0;
    FCmp fC_6d083b{};
    FCmp fC_6d084c{};
    FCmp fC_6d08c7{};
    FCmp fC_6d08ef{};
    FCmp fC_6d090c{};
    FCmp fC_6d092c{};
    FCmp fC_6d094c{};
    FCmp fC_6d0969{};
    FCmp fC_6d098c{};
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    x3 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x1 = movss_m(rd32(m, ebp + 0x8u));
    esp = esp - 0x1cu;
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    fA_6d07b9 = rd32(m, esi + 0x770u);
    fB_6d07b9 = 0x0u;
    if (fA_6d07b9 != fB_6d07b9) goto L_6d08ad;
    x2 = movss_m(rd32(m, esi + 0x35cu));
    x4 = movss_m(rd32(m, esi + 0x850u));
    x0 = x1;
    x0 = mulss(x0, rd32(m, 0x92e330u) /* 2.0 */);
    x0 = addss(x0, x2.l(0));
    x5 = x2;
    x2 = movss_m(rd32(m, esi + 0x830u));
    fC_6d07f0 = fcmp32(x4.l(0), x2.l(0));
    wr32(m, esi + 0x35cu, x0.l(0));
    if ((fc_b(fC_6d07f0) || fc_z(fC_6d07f0))) goto L_6d0814;
    x6 = movss_m(rd32(m, 0x9322ccu) /* 0.5 */);
    fC_6d0805 = fcmp32(x0.l(0), x6.l(0));
    if (fc_b(fC_6d0805)) goto L_6d0814;
    wr32(m, esi + 0x35cu, x6.l(0));
    goto L_6d0826;
L_6d0814:
    fC_6d0814 = fcmp32(x2.l(0), x4.l(0));
    if (fc_b(fC_6d0814)) goto L_6d0826;
    fC_6d0819 = fcmp32(x0.l(0), x3.l(0));
    if (fc_b(fC_6d0819)) goto L_6d0826;
    wr32(m, esi + 0x35cu, x3.l(0));
L_6d0826:
    fA_6d0826 = rd32(m, esi + 0x86cu);
    fB_6d0826 = 0x0u;
    if (fA_6d0826 == fB_6d0826) goto L_6d08dc;
    x0 = movss_m(rd32(m, 0x9d78a0u));
    fC_6d083b = fcmp32(x0.l(0), x5.l(0));
    if ((fc_b(fC_6d083b) || fc_z(fC_6d083b))) goto L_6d08dc;
    x2 = movss_m(rd32(m, esi + 0x35cu));
    fC_6d084c = fcmp32(x2.l(0), x0.l(0));
    if ((fc_b(fC_6d084c) || fc_z(fC_6d084c))) goto L_6d08dc;
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x44u);
    ecx = (esp + 0x10u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = esi;
    LIFT_CALL(edx, 0x6d0863u);
    eax = rd32(m, esi + 0x86cu);
    x0 = movss_m(rd32(m, esp + 0x10u));
    x3 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x1 = movss_m(rd32(m, ebp + 0x8u));
    eax = eax + 0x1b0u;
    wr32(m, eax, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x14u));
    wr32(m, eax + 0x4u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x18u));
    wr32(m, eax + 0x8u, x0.l(0));
    eax = rd32(m, esi + 0x86cu);
    wr32(m, eax + 0x1d8u, 0x1u);
    goto L_6d08dc;
L_6d08ad:
    x0 = movss_m(rd32(m, esi + 0x35cu));
    x2 = x1;
    x2 = mulss(x2, rd32(m, 0x935aa8u) /* 0.75 */);
    x0 = subss(x0, x2.l(0));
    x2 = X{};
    fC_6d08c7 = fcmp32(x2.l(0), x0.l(0));
    wr32(m, esi + 0x35cu, x0.l(0));
    if (fc_b(fC_6d08c7)) goto L_6d08dc;
    wr32(m, esi + 0x35cu, x2.l(0));
L_6d08dc:
    x0 = x1;
    x0 = divss(x0, rd32(m, esi + 0x83cu));
    x0 = addss(x0, rd32(m, esi + 0x838u));
    fC_6d08ef = fcmp32(x0.l(0), x3.l(0));
    wr32(m, esi + 0x838u, x0.l(0));
    if (fc_b(fC_6d08ef)) goto L_6d0919;
L_6d0900:
    x0 = movss_m(rd32(m, esi + 0x838u));
    x0 = subss(x0, x3.l(0));
    fC_6d090c = fcmp32(x0.l(0), x3.l(0));
    wr32(m, esi + 0x838u, x0.l(0));
    if (!fc_b(fC_6d090c)) goto L_6d0900;
L_6d0919:
    x0 = x1;
    x0 = divss(x0, rd32(m, esi + 0x844u));
    x0 = addss(x0, rd32(m, esi + 0x840u));
    fC_6d092c = fcmp32(x0.l(0), x3.l(0));
    wr32(m, esi + 0x840u, x0.l(0));
    if (fc_b(fC_6d092c)) goto L_6d0959;
L_6d0940:
    x0 = movss_m(rd32(m, esi + 0x840u));
    x0 = subss(x0, x3.l(0));
    fC_6d094c = fcmp32(x0.l(0), x3.l(0));
    wr32(m, esi + 0x840u, x0.l(0));
    if (!fc_b(fC_6d094c)) goto L_6d0940;
L_6d0959:
    x1 = divss(x1, rd32(m, esi + 0x84cu));
    x1 = addss(x1, rd32(m, esi + 0x848u));
    fC_6d0969 = fcmp32(x1.l(0), x3.l(0));
    wr32(m, esi + 0x848u, x1.l(0));
    if (fc_b(fC_6d0969)) goto L_6d0999;
    goto L_6d0980;
L_6d0980:
    x0 = movss_m(rd32(m, esi + 0x848u));
    x0 = subss(x0, x3.l(0));
    fC_6d098c = fcmp32(x0.l(0), x3.l(0));
    wr32(m, esi + 0x848u, x0.l(0));
    if (!fc_b(fC_6d098c)) goto L_6d0980;
L_6d0999:
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x35cu)));
    ecx = rd32(m, esi + 0x774u);
    edx = rd32(m, esi + 0x354u);
    esp = esp - 0x10u;
    wr32(m, esp + 0xcu, fst32(m));
    fpop(m);
    eax = (esi + 0x148u);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x848u)));
    wr32(m, esp + 0x8u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x840u)));
    wr32(m, esp + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x838u)));
    wr32(m, esp, fst32(m));
    fpop(m);
    esp -= 4; wr32(m, esp, ecx);
    ecx = rd32(m, esi + 0x358u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x8363b0u, 0x6d09e3u);  // FUN_008363b0
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(16);
}

// 006d09f0 LandAnimalProximity__vf20
//   original LandAnimalProximity::vf20; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x006d09f0, LandAnimalProximity__vf20, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_6d09fc = 0;
    uint32_t fB_6d09fc = 0;
    uint32_t fA_6d0a23 = 0;
    uint32_t fB_6d0a23 = 0;
    uint32_t fA_6d0a35 = 0;
    uint32_t fB_6d0a35 = 0;
    uint32_t fA_6d0a4a = 0;
    uint32_t fB_6d0a4a = 0;
    uint32_t fA_6d0a5a = 0;
    uint32_t fB_6d0a5a = 0;
    uint32_t fA_6d0a64 = 0;
    uint32_t fB_6d0a64 = 0;
    FCmp fC_6d0a71{};
    uint32_t fR_6d0a79 = 0;
    FCmp fC_6d0a8f{};
    FCmp fC_6d0b2f{};
    FCmp fC_6d0c19{};
    FCmp fC_6d0c8b{};
    FCmp fC_6d0d49{};
    FCmp fC_6d0d69{};
    FCmp fC_6d0d73{};
    FCmp fC_6d0d91{};
    FCmp fC_6d0de8{};
    FCmp fC_6d0ee3{};
    FCmp fC_6d0f23{};
    FCmp fC_6d0f2d{};
    FCmp fC_6d0f4b{};
    FCmp fC_6d0fa2{};
    FCmp fC_6d109d{};
    FCmp fC_6d10fb{};
    FCmp fC_6d110c{};
    FCmp fC_6d1360{};
    FCmp fC_6d13cb{};
    FCmp fC_6d13e0{};
    FCmp fC_6d13f2{};
    FCmp fC_6d1408{};
    FCmp fC_6d145b{};
    FCmp fC_6d1561{};
    FCmp fC_6d1595{};
    FCmp fC_6d15a7{};
    FCmp fC_6d15bd{};
    FCmp fC_6d1610{};
    FCmp fC_6d1712{};
    FCmp fC_6d1753{};
    FCmp fC_6d18a0{};
    uint32_t fR_6d1903 = 0;
    uint32_t fA_6d1941 = 0;
    uint32_t fB_6d1941 = 0;
    uint32_t fR_6d194e = 0;
    uint32_t fR_6d19ce = 0;
    FCmp fC_6d1b5a{};
    FCmp fC_6d1ba0{};
    FCmp fC_6d1bee{};
    uint32_t fA_6d1c0a = 0;
    uint32_t fB_6d1c0a = 0;
    uint32_t fA_6d1c32 = 0;
    uint32_t fB_6d1c32 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x9cu;
    fA_6d09fc = rd32(m, 0xa94324u);
    fB_6d09fc = 0x0u;
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    if (fA_6d09fc != fB_6d09fc) goto L_6d1c72;
    eax = rd32(m, esi + 0x10u);
    ecx = rd32(m, 0xa95844u);
    x0 = movss_m(rd32(m, esi + 0x878u));
    wr32(m, esp + 0x5cu, x0.l(0));
    fA_6d0a23 = eax;
    fB_6d0a23 = rd32(m, ecx + 0x20u);
    if (fA_6d0a23 != fB_6d0a23) goto L_6d0a35;
    eax = rd32(m, 0xa946b8u);
    eax = rd32(m, eax + 0x5a54u);
    goto L_6d0a4a;
L_6d0a35:
    fA_6d0a35 = eax;
    fB_6d0a35 = rd32(m, ecx + 0x1cu);
    if (fA_6d0a35 != fB_6d0a35) goto L_6d0a48;
    edx = rd32(m, 0xa946b8u);
    eax = rd32(m, edx + 0x20c8u);
    goto L_6d0a4a;
L_6d0a48:
    eax = 0u;
L_6d0a4a:
    fA_6d0a4a = rd32(m, eax + 0x20014u);
    fB_6d0a4a = 0x0u;
    if (fA_6d0a4a != fB_6d0a4a) goto L_6d1c72;
    eax = rd32(m, ecx + 0x10u);
    fA_6d0a5a = rd32(m, eax + 0xcu);
    fB_6d0a5a = 0x0u;
    if (fA_6d0a5a != fB_6d0a5a) goto L_6d1c72;
    fA_6d0a64 = rd32(m, esi + 0x77cu);
    fB_6d0a64 = 0x0u;
    if (fA_6d0a64 != fB_6d0a64) goto L_6d1c72;
    fC_6d0a71 = fcmp32(x0.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_6d0a71) | 2u) << 8);
    fR_6d0a79 = (((eax >> 8) & 0xffu) & 0x44u);
    if (!parity8(fR_6d0a79)) goto L_6d1c72;
    LIFT_CALL(0x6d2eb0u, 0x6d0a87u);  // FUN_006d2eb0
    x0 = movss_m(rd32(m, esi + 0x830u));
    fC_6d0a8f = fcmp32(x0.l(0), rd32(m, 0x977e9cu) /* 0.001500000013038516 */);
    if ((fc_b(fC_6d0a8f) || fc_z(fC_6d0a8f))) goto L_6d1c00;
    edx = rd32(m, esi);
    edx = rd32(m, edx + 0x3cu);
    eax = (esp + 0x60u);
    esp -= 4; wr32(m, esp, eax);
    ecx = esi;
    LIFT_CALL(edx, 0x6d0aaau);
    x0 = movss_m(rd32(m, esi + 0x7c0u));
    x0 = addss(x0, rd32(m, esp + 0x90u));
    x1 = movss_m(rd32(m, esi + 0x7c4u));
    x2 = movss_m(rd32(m, esi + 0x7c8u));
    x5 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    wr32(m, esp + 0x30u, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x7c4u));
    x0 = addss(x0, rd32(m, esp + 0x94u));
    wr32(m, esp + 0x34u, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x7c8u));
    x0 = addss(x0, rd32(m, esp + 0x98u));
    wr32(m, esp + 0x38u, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x7c0u));
    x3 = x1;
    wr32(m, esp + 0xcu, x0.l(0));
    x3 = mulss(x3, x1.l(0));
    x0 = mulss(x0, x0.l(0));
    x1 = x2;
    x0 = addss(x0, x3.l(0));
    x1 = mulss(x1, x2.l(0));
    x0 = addss(x0, x1.l(0));
    fC_6d0b2f = fcmp32(x0.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    wr32(m, esp + 0x8u, x0.l(0));
    if ((fc_b(fC_6d0b2f) || fc_z(fC_6d0b2f))) goto L_6d0b9a;
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x8u)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x42c230u, 0x6d0b4bu);  // FUN_0042c230
    x5 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x1 = x0;
    x0 = x5;
    x0 = divss(x0, x1.l(0));
    x1 = movss_m(rd32(m, esp + 0x10u));
    x1 = mulss(x1, x0.l(0));
    wr32(m, esi + 0x7d0u, x1.l(0));
    x1 = movss_m(rd32(m, esi + 0x7c4u));
    x1 = mulss(x1, x0.l(0));
    wr32(m, esi + 0x7d4u, x1.l(0));
    x1 = movss_m(rd32(m, esi + 0x7c8u));
    x1 = mulss(x1, x0.l(0));
    esp = esp + 0x4u;
    wr32(m, esi + 0x7d8u, x1.l(0));
L_6d0b9a:
    x2 = movss_m(rd32(m, 0x9774a8u) /* -0.0 */);
    x0 = x2;
    x0 = subss(x0, rd32(m, esi + 0x7d0u));
    x1 = x2;
    x1 = subss(x1, rd32(m, esi + 0x7d4u));
    x2 = subss(x2, rd32(m, esi + 0x7d8u));
    wr32(m, esi + 0x7d0u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x84u));
    wr32(m, esi + 0x7d4u, x1.l(0));
    x1 = movss_m(rd32(m, esp + 0x80u));
    x3 = x1;
    wr32(m, esp + 0x14u, x0.l(0));
    x3 = mulss(x3, x1.l(0));
    wr32(m, esi + 0x7d8u, x2.l(0));
    x2 = movss_m(rd32(m, esp + 0x88u));
    wr32(m, esp + 0x10u, x1.l(0));
    x0 = mulss(x0, x0.l(0));
    x1 = x2;
    x0 = addss(x0, x3.l(0));
    x1 = mulss(x1, x2.l(0));
    x0 = addss(x0, x1.l(0));
    fC_6d0c19 = fcmp32(x0.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    wr32(m, esp + 0x18u, x2.l(0));
    wr32(m, esp + 0xcu, x0.l(0));
    if ((fc_b(fC_6d0c19) || fc_z(fC_6d0c19))) goto L_6d0c83;
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0xcu)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x42c230u, 0x6d0c3bu);  // FUN_0042c230
    x5 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x1 = x0;
    x0 = x5;
    x0 = divss(x0, x1.l(0));
    x1 = x0;
    x1 = mulss(x1, rd32(m, esp + 0x84u));
    wr32(m, esp + 0x14u, x1.l(0));
    x1 = x0;
    x1 = mulss(x1, rd32(m, esp + 0x88u));
    x0 = mulss(x0, rd32(m, esp + 0x8cu));
    esp = esp + 0x4u;
    wr32(m, esp + 0x14u, x1.l(0));
    wr32(m, esp + 0x18u, x0.l(0));
L_6d0c83:
    x0 = movss_m(rd32(m, 0x977d90u) /* 0.07999999821186066 */);
    fC_6d0c8b = fcmp32(x0.l(0), rd32(m, esi + 0x830u));
    if ((fc_b(fC_6d0c8b) || fc_z(fC_6d0c8b))) goto L_6d0ca0;
    wr32(m, esi + 0x770u, 0x2u);
    goto L_6d0caa;
L_6d0ca0:
    wr32(m, esi + 0x770u, 0x0u);
L_6d0caa:
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x800u)));
    wr32(m, esi + 0x7f0u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x804u)));
    wr32(m, esi + 0x7f4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x808u)));
    wr32(m, esi + 0x7f8u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x810u)));
    wr32(m, esi + 0x800u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x814u)));
    wr32(m, esi + 0x804u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x818u)));
    wr32(m, esi + 0x808u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x820u)));
    wr32(m, esi + 0x810u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x824u)));
    wr32(m, esi + 0x814u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x828u)));
    wr32(m, esi + 0x818u, fst32(m));
    fpop(m);
    x1 = movss_m(rd32(m, esi + 0x7d0u));
    x2 = movss_m(rd32(m, esi + 0x810u));
    x3 = movss_m(rd32(m, esi + 0x818u));
    x4 = movss_m(rd32(m, esi + 0x7d8u));
    x0 = x1;
    x0 = mulss(x0, x2.l(0));
    x1 = mulss(x1, x3.l(0));
    x2 = mulss(x2, x4.l(0));
    x1 = subss(x1, x2.l(0));
    fC_6d0d49 = fcmp32(x1.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    x1 = movss_m(rd32(m, 0x9cd9e8u));
    x6 = x4;
    x6 = mulss(x6, x3.l(0));
    x0 = addss(x0, x6.l(0));
    if ((fc_b(fC_6d0d49) || fc_z(fC_6d0d49))) goto L_6d0f23;
    fC_6d0d69 = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_6d0d69) || fc_z(fC_6d0d69))) goto L_6d0d73;
    x0 = x1;
    goto L_6d0d7b;
L_6d0d73:
    fC_6d0d73 = fcmp32(x0.l(0), x5.l(0));
    if ((fc_b(fC_6d0d73) || fc_z(fC_6d0d73))) goto L_6d0d7b;
    x0 = x5;
L_6d0d7b:
    wr32(m, esp + 0x8u, x0.l(0));
    LIFT_CALL(0x401040u, 0x6d0d86u);  // FUN_00401040
    x2 = movsd_m(rd64(m, 0x977638u) /* 1.0 */);
    x4 = x0;
    fC_6d0d91 = fcmp32(x4.l(0), rd32(m, 0x9ddfc8u));
    wr32(m, esp + 0xcu, x4.l(0));
    if (fc_b(fC_6d0d91)) goto L_6d0de5;
    x0 = cvtss2sd(x0, rd32(m, esp + 0x8u));
    x1 = x0;
    x1 = mulsd(x1, x0.lo);
    x0 = cvtsd2ss(x0, x1.lo);
    x0 = cvtss2sd(x0, x0.l(0));
    x1 = x2;
    x1 = subsd(x1, x0.lo);
    x0 = cvtsd2ss(x0, x1.lo);
    LIFT_CALL(0x401040u, 0x6d0dc7u);  // FUN_00401040
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, x0.l(0));
    LIFT_CALL(0x42c230u, 0x6d0dd2u);  // FUN_0042c230
    x2 = movsd_m(rd64(m, 0x977638u) /* 1.0 */);
    x4 = movss_m(rd32(m, esp + 0x10u));
    esp = esp + 0x4u;
    goto L_6d0de8;
L_6d0de5:
    x0 = x4;
L_6d0de8:
    fC_6d0de8 = fcmp32(x4.l(0), rd32(m, 0x9ddfc8u));
    x6 = movsd_m(rd64(m, 0x9777b0u) /* 0.5648093223571777 */);
    x1 = cvtss2sd(x1, x0.l(0));
    x3 = x1;
    x3 = mulsd(x3, x1.lo);
    x1 = cvtsd2ss(x1, x3.lo);
    x3 = cvtss2sd(x3, x1.l(0));
    x5 = x3;
    x5 = mulsd(x5, x3.lo);
    x3 = cvtsd2ss(x3, x5.lo);
    x5 = cvtss2sd(x5, x1.l(0));
    x5 = mulsd(x5, rd64(m, 0x9777a8u) /* 0.04992850869894028 */);
    x6 = subsd(x6, x5.lo);
    x5 = cvtpd2ps(x6);
    x5 = cvtss2sd(x5, x5.l(0));
    x6 = cvtss2sd(x6, x3.l(0));
    x5 = mulsd(x5, x6.lo);
    x6 = cvtss2sd(x6, x1.l(0));
    x6 = mulsd(x6, rd64(m, 0x9777a0u) /* 1.4430354833602905 */);
    x7 = x2;
    x7 = subsd(x7, x6.lo);
    x6 = cvtsd2ss(x6, x7.lo);
    x7 = movsd_m(rd64(m, 0x977798u) /* 0.3993034064769745 */);
    x6 = cvtss2sd(x6, x6.l(0));
    x5 = addsd(x5, x6.lo);
    x6 = x2;
    x6 = divsd(x6, x5.lo);
    x5 = cvtpd2ps(x6);
    x6 = cvtss2sd(x6, x1.l(0));
    x6 = mulsd(x6, rd64(m, 0x977790u) /* 0.0193784236907959 */);
    x7 = subsd(x7, x6.lo);
    x1 = cvtss2sd(x1, x1.l(0));
    x1 = mulsd(x1, rd64(m, 0x977788u) /* 1.2763687372207642 */);
    x2 = subsd(x2, x1.lo);
    x6 = cvtpd2ps(x7);
    x6 = cvtss2sd(x6, x6.l(0));
    x1 = cvtpd2ps(x2);
    x2 = movsd_m(rd64(m, 0x977510u) /* 1.5707963705062866 */);
    x0 = cvtss2sd(x0, x0.l(0));
    x3 = cvtss2sd(x3, x3.l(0));
    x6 = mulsd(x6, x3.lo);
    x1 = cvtss2sd(x1, x1.l(0));
    x6 = addsd(x6, x1.lo);
    x6 = mulsd(x6, x0.lo);
    x0 = cvtpd2ps(x6);
    x0 = cvtss2sd(x0, x0.l(0));
    x5 = cvtss2sd(x5, x5.l(0));
    x5 = mulsd(x5, x0.lo);
    x0 = cvtsd2ss(x0, x5.lo);
    if (fc_b(fC_6d0de8)) goto L_6d0edd;
    x0 = cvtss2sd(x0, x0.l(0));
    x1 = x2;
    x1 = subsd(x1, x0.lo);
    x0 = cvtsd2ss(x0, x1.lo);
L_6d0edd:
    x1 = movss_m(rd32(m, esp + 0x8u));
    fC_6d0ee3 = fcmp32(x1.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    if (fc_b(fC_6d0ee3)) goto L_6d0f03;
    x1 = x0;
    x0 = cvtps2pd(x1.lo);
    x1 = x2;
    x1 = subsd(x1, x0.lo);
    x0 = cvtpd2ps(x1);
    goto L_6d10c2;
L_6d0f03:
    x1 = movss_m(rd32(m, 0x9774a8u) /* -0.0 */);
    x1 = subss(x1, x0.l(0));
    x0 = cvtps2pd(x1.lo);
    x1 = x2;
    x1 = subsd(x1, x0.lo);
    x0 = cvtpd2ps(x1);
    goto L_6d10c2;
L_6d0f23:
    fC_6d0f23 = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_6d0f23) || fc_z(fC_6d0f23))) goto L_6d0f2d;
    x0 = x1;
    goto L_6d0f35;
L_6d0f2d:
    fC_6d0f2d = fcmp32(x0.l(0), x5.l(0));
    if ((fc_b(fC_6d0f2d) || fc_z(fC_6d0f2d))) goto L_6d0f35;
    x0 = x5;
L_6d0f35:
    wr32(m, esp + 0x8u, x0.l(0));
    LIFT_CALL(0x401040u, 0x6d0f40u);  // FUN_00401040
    x2 = movsd_m(rd64(m, 0x977638u) /* 1.0 */);
    x4 = x0;
    fC_6d0f4b = fcmp32(x4.l(0), rd32(m, 0x9ddfc8u));
    wr32(m, esp + 0xcu, x4.l(0));
    if (fc_b(fC_6d0f4b)) goto L_6d0f9f;
    x0 = cvtss2sd(x0, rd32(m, esp + 0x8u));
    x1 = x0;
    x1 = mulsd(x1, x0.lo);
    x0 = cvtsd2ss(x0, x1.lo);
    x0 = cvtss2sd(x0, x0.l(0));
    x1 = x2;
    x1 = subsd(x1, x0.lo);
    x0 = cvtsd2ss(x0, x1.lo);
    LIFT_CALL(0x401040u, 0x6d0f81u);  // FUN_00401040
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, x0.l(0));
    LIFT_CALL(0x42c230u, 0x6d0f8cu);  // FUN_0042c230
    x4 = movss_m(rd32(m, esp + 0x10u));
    x2 = movsd_m(rd64(m, 0x977638u) /* 1.0 */);
    esp = esp + 0x4u;
    goto L_6d0fa2;
L_6d0f9f:
    x0 = x4;
L_6d0fa2:
    fC_6d0fa2 = fcmp32(x4.l(0), rd32(m, 0x9ddfc8u));
    x6 = movsd_m(rd64(m, 0x9777b0u) /* 0.5648093223571777 */);
    x1 = cvtss2sd(x1, x0.l(0));
    x3 = x1;
    x3 = mulsd(x3, x1.lo);
    x1 = cvtsd2ss(x1, x3.lo);
    x3 = cvtss2sd(x3, x1.l(0));
    x5 = x3;
    x5 = mulsd(x5, x3.lo);
    x3 = cvtsd2ss(x3, x5.lo);
    x5 = cvtss2sd(x5, x1.l(0));
    x5 = mulsd(x5, rd64(m, 0x9777a8u) /* 0.04992850869894028 */);
    x6 = subsd(x6, x5.lo);
    x5 = cvtpd2ps(x6);
    x5 = cvtss2sd(x5, x5.l(0));
    x6 = cvtss2sd(x6, x3.l(0));
    x5 = mulsd(x5, x6.lo);
    x6 = cvtss2sd(x6, x1.l(0));
    x6 = mulsd(x6, rd64(m, 0x9777a0u) /* 1.4430354833602905 */);
    x7 = x2;
    x7 = subsd(x7, x6.lo);
    x6 = cvtsd2ss(x6, x7.lo);
    x7 = movsd_m(rd64(m, 0x977798u) /* 0.3993034064769745 */);
    x6 = cvtss2sd(x6, x6.l(0));
    x5 = addsd(x5, x6.lo);
    x6 = x2;
    x6 = divsd(x6, x5.lo);
    x5 = cvtpd2ps(x6);
    x6 = cvtss2sd(x6, x1.l(0));
    x6 = mulsd(x6, rd64(m, 0x977790u) /* 0.0193784236907959 */);
    x7 = subsd(x7, x6.lo);
    x1 = cvtss2sd(x1, x1.l(0));
    x1 = mulsd(x1, rd64(m, 0x977788u) /* 1.2763687372207642 */);
    x2 = subsd(x2, x1.lo);
    x6 = cvtpd2ps(x7);
    x6 = cvtss2sd(x6, x6.l(0));
    x1 = cvtpd2ps(x2);
    x2 = movsd_m(rd64(m, 0x977510u) /* 1.5707963705062866 */);
    x0 = cvtss2sd(x0, x0.l(0));
    x3 = cvtss2sd(x3, x3.l(0));
    x6 = mulsd(x6, x3.lo);
    x1 = cvtss2sd(x1, x1.l(0));
    x6 = addsd(x6, x1.lo);
    x6 = mulsd(x6, x0.lo);
    x0 = cvtpd2ps(x6);
    x0 = cvtss2sd(x0, x0.l(0));
    x5 = cvtss2sd(x5, x5.l(0));
    x5 = mulsd(x5, x0.lo);
    x0 = cvtsd2ss(x0, x5.lo);
    if (fc_b(fC_6d0fa2)) goto L_6d1097;
    x0 = cvtss2sd(x0, x0.l(0));
    x1 = x2;
    x1 = subsd(x1, x0.lo);
    x0 = cvtsd2ss(x0, x1.lo);
L_6d1097:
    x1 = movss_m(rd32(m, esp + 0x8u));
    fC_6d109d = fcmp32(x1.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    if (fc_b(fC_6d109d)) goto L_6d10ab;
    x1 = x0;
    goto L_6d10b7;
L_6d10ab:
    x1 = movss_m(rd32(m, 0x9774a8u) /* -0.0 */);
    x1 = subss(x1, x0.l(0));
L_6d10b7:
    x0 = cvtps2pd(x1.lo);
    x0 = subsd(x0, x2.lo);
    x0 = cvtsd2ss(x0, x0.lo);
L_6d10c2:
    wr32(m, esp + 0x8u, x0.l(0));
    LIFT_CALL(0x401000u, 0x6d10cdu);  // FUN_00401000
    x4 = movss_m(rd32(m, 0x935d50u) /* -2.1972959984850604e-07 */);
    x6 = movss_m(rd32(m, 0x935d68u) /* -2.0362239894211598e-08 */);
    x7 = movss_m(rd32(m, 0x935d60u) /* -0.00019808630167972296 */);
    x3 = movss_m(rd32(m, 0x977498u) /* 3.1415927410125732 */);
    x5 = X{};
    x1 = x0;
    x0 = movss_m(rd32(m, 0x977bbcu) /* 0.39269909262657166 */);
    fC_6d10fb = fcmp32(x1.l(0), x0.l(0));
    x1 = movss_m(rd32(m, 0x97749cu) /* 6.2831854820251465 */);
    if ((fc_b(fC_6d10fb) || fc_z(fC_6d10fb))) goto L_6d1276;
    fC_6d110c = fcmp32(x5.l(0), rd32(m, esp + 0x8u));
    x2 = x5;
    x2 = mulss(x2, x1.l(0));
    x0 = subss(x0, x2.l(0));
    x3 = subss(x3, x0.l(0));
    x0 = x3;
    x0 = mulss(x0, x3.l(0));
    x1 = x0;
    x1 = mulss(x1, x6.l(0));
    x1 = addss(x1, rd32(m, 0x935d64u) /* 2.6997149689123034e-06 */);
    x2 = x0;
    x2 = mulss(x2, x0.l(0));
    x6 = x0;
    x6 = mulss(x6, x7.l(0));
    x6 = addss(x6, rd32(m, 0x935d5cu) /* 0.008332403376698494 */);
    x1 = mulss(x1, x2.l(0));
    x1 = addss(x1, x6.l(0));
    x6 = x0;
    x6 = mulss(x6, rd32(m, 0x935d58u) /* -0.16666549444198608 */);
    x6 = addss(x6, rd32(m, 0x935d54u) /* 0.9999995827674866 */);
    x1 = mulss(x1, x2.l(0));
    x1 = addss(x1, x6.l(0));
    x1 = mulss(x1, x3.l(0));
    x3 = x0;
    x3 = mulss(x3, x4.l(0));
    x3 = addss(x3, rd32(m, 0x935d48u) /* 2.4202940039685927e-05 */);
    x4 = x0;
    x4 = mulss(x4, rd32(m, 0x935d44u) /* -0.0013858790043741465 */);
    x4 = addss(x4, rd32(m, 0x935d4cu) /* 0.04165977984666824 */);
    x0 = mulss(x0, rd32(m, 0x935d40u) /* -0.4999941885471344 */);
    x0 = addss(x0, rd32(m, 0x935d3cu) /* 0.9999992251396179 */);
    x3 = mulss(x3, x2.l(0));
    x3 = addss(x3, x4.l(0));
    x3 = mulss(x3, x2.l(0));
    x3 = addss(x3, x0.l(0));
    if ((fc_b(fC_6d110c) || fc_z(fC_6d110c))) goto L_6d121e;
    x0 = movss_m(rd32(m, esi + 0x810u));
    x2 = movss_m(rd32(m, 0x9774a8u) /* -0.0 */);
    x6 = movss_m(rd32(m, 0x9cd9e8u));
    x2 = subss(x2, x3.l(0));
    x3 = movss_m(rd32(m, esi + 0x818u));
    x7 = x0;
    x1 = mulss(x1, x6.l(0));
    x4 = x3;
    x0 = mulss(x0, x2.l(0));
    x3 = mulss(x3, x1.l(0));
    x0 = addss(x0, x3.l(0));
    wr32(m, esi + 0x820u, x0.l(0));
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x814u)));
    x4 = mulss(x4, x2.l(0));
    wr32(m, esi + 0x824u, fst32(m));
    fpop(m);
    x7 = mulss(x7, x1.l(0));
    x4 = subss(x4, x7.l(0));
    wr32(m, esi + 0x828u, x4.l(0));
    goto L_6d12a2;
L_6d121e:
    x4 = movss_m(rd32(m, esi + 0x810u));
    x0 = movss_m(rd32(m, 0x9774a8u) /* -0.0 */);
    x0 = subss(x0, x3.l(0));
    x3 = movss_m(rd32(m, esi + 0x818u));
    x2 = x0;
    x6 = x1;
    x0 = mulss(x0, x4.l(0));
    x1 = mulss(x1, x3.l(0));
    x0 = addss(x0, x1.l(0));
    wr32(m, esi + 0x820u, x0.l(0));
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x814u)));
    x2 = mulss(x2, x3.l(0));
    wr32(m, esi + 0x824u, fst32(m));
    fpop(m);
    x6 = mulss(x6, x4.l(0));
    x2 = subss(x2, x6.l(0));
    wr32(m, esi + 0x828u, x2.l(0));
    goto L_6d129a;
L_6d1276:
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x7d0u)));
    wr32(m, esi + 0x820u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x7d4u)));
    wr32(m, esi + 0x824u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x7d8u)));
    wr32(m, esi + 0x828u, fst32(m));
    fpop(m);
L_6d129a:
    x6 = movss_m(rd32(m, 0x9cd9e8u));
L_6d12a2:
    x0 = movss_m(rd32(m, esi + 0x7f0u));
    x3 = movss_m(rd32(m, esi + 0x800u));
    x1 = movss_m(rd32(m, esi + 0x7f4u));
    x2 = movss_m(rd32(m, esi + 0x7f8u));
    x4 = movss_m(rd32(m, esi + 0x818u));
    x1 = addss(x1, x5.l(0));
    x0 = addss(x0, x5.l(0));
    x3 = addss(x3, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x804u));
    x0 = addss(x0, x1.l(0));
    x1 = movss_m(rd32(m, esi + 0x808u));
    x2 = addss(x2, x5.l(0));
    x1 = addss(x1, x2.l(0));
    x2 = movss_m(rd32(m, esi + 0x810u));
    x2 = addss(x2, x3.l(0));
    x3 = movss_m(rd32(m, esi + 0x814u));
    x3 = addss(x3, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x820u));
    x4 = addss(x4, x1.l(0));
    x1 = movss_m(rd32(m, esi + 0x824u));
    x1 = addss(x1, x3.l(0));
    x3 = movss_m(rd32(m, 0x92ff84u) /* 0.25 */);
    x0 = addss(x0, x2.l(0));
    x2 = movss_m(rd32(m, esi + 0x828u));
    x1 = mulss(x1, x3.l(0));
    x0 = mulss(x0, x3.l(0));
    x2 = addss(x2, x4.l(0));
    x2 = mulss(x2, x3.l(0));
    x1 = mulss(x1, x1.l(0));
    x3 = x0;
    x3 = mulss(x3, x0.l(0));
    x1 = addss(x1, x3.l(0));
    x3 = x2;
    x3 = mulss(x3, x2.l(0));
    x1 = addss(x1, x3.l(0));
    fC_6d1360 = fcmp32(x1.l(0), x5.l(0));
    wr32(m, esp + 0x20u, x0.l(0));
    wr32(m, esp + 0x28u, x2.l(0));
    wr32(m, esp + 0xcu, x1.l(0));
    if ((fc_b(fC_6d1360) || fc_z(fC_6d1360))) goto L_6d13ad;
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0xcu)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x42c230u, 0x6d1384u);  // FUN_0042c230
    x2 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x6 = movss_m(rd32(m, 0x9cd9e8u));
    x5 = X{};
    x2 = divss(x2, x0.l(0));
    x0 = x2;
    x0 = mulss(x0, rd32(m, esp + 0x24u));
    x2 = mulss(x2, rd32(m, esp + 0x2cu));
    esp = esp + 0x4u;
L_6d13ad:
    x4 = movss_m(rd32(m, esp + 0x18u));
    x3 = movss_m(rd32(m, esp + 0x10u));
    x7 = x0;
    x1 = x2;
    x0 = mulss(x0, x4.l(0));
    x2 = mulss(x2, x3.l(0));
    x0 = subss(x0, x2.l(0));
    fC_6d13cb = fcmp32(x0.l(0), x5.l(0));
    x1 = mulss(x1, x4.l(0));
    x7 = mulss(x7, x3.l(0));
    x1 = addss(x1, x7.l(0));
    if ((fc_b(fC_6d13cb) || fc_z(fC_6d13cb))) goto L_6d1595;
    fC_6d13e0 = fcmp32(x6.l(0), x1.l(0));
    if ((fc_b(fC_6d13e0) || fc_z(fC_6d13e0))) goto L_6d13ea;
    x1 = x6;
    goto L_6d13f7;
L_6d13ea:
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    fC_6d13f2 = fcmp32(x1.l(0), x0.l(0));
    if (!(fc_b(fC_6d13f2) || fc_z(fC_6d13f2))) goto L_6d13fa;
L_6d13f7:
    x0 = x1;
L_6d13fa:
    wr32(m, esp + 0x8u, x0.l(0));
    LIFT_CALL(0x401040u, 0x6d1405u);  // FUN_00401040
    x5 = x0;
    fC_6d1408 = fcmp32(x5.l(0), rd32(m, 0x9ddfc8u));
    wr32(m, esp + 0xcu, x5.l(0));
    if (fc_b(fC_6d1408)) goto L_6d1458;
    x0 = cvtss2sd(x0, rd32(m, esp + 0x8u));
    x1 = x0;
    x1 = mulsd(x1, x0.lo);
    x0 = cvtsd2ss(x0, x1.lo);
    x1 = movsd_m(rd64(m, 0x977638u) /* 1.0 */);
    x0 = cvtss2sd(x0, x0.l(0));
    x1 = subsd(x1, x0.lo);
    x0 = cvtpd2ps(x1);
    LIFT_CALL(0x401040u, 0x6d1442u);  // FUN_00401040
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, x0.l(0));
    LIFT_CALL(0x42c230u, 0x6d144du);  // FUN_0042c230
    x5 = movss_m(rd32(m, esp + 0x10u));
    esp = esp + 0x4u;
    goto L_6d145b;
L_6d1458:
    x0 = x5;
L_6d145b:
    fC_6d145b = fcmp32(x5.l(0), rd32(m, 0x9ddfc8u));
    x1 = cvtss2sd(x1, x0.l(0));
    x2 = x1;
    x2 = mulsd(x2, x1.lo);
    x1 = cvtsd2ss(x1, x2.lo);
    x2 = cvtss2sd(x2, x1.l(0));
    x3 = x2;
    x3 = mulsd(x3, x2.lo);
    x2 = movsd_m(rd64(m, 0x9777b0u) /* 0.5648093223571777 */);
    x4 = cvtss2sd(x4, x1.l(0));
    x4 = mulsd(x4, rd64(m, 0x9777a8u) /* 0.04992850869894028 */);
    x2 = subsd(x2, x4.lo);
    x2 = cvtpd2ps(x2);
    x4 = cvtss2sd(x4, x2.l(0));
    x3 = cvtsd2ss(x3, x3.lo);
    x2 = cvtss2sd(x2, x3.l(0));
    x4 = mulsd(x4, x2.lo);
    x2 = movsd_m(rd64(m, 0x977638u) /* 1.0 */);
    x6 = cvtss2sd(x6, x1.l(0));
    x6 = mulsd(x6, rd64(m, 0x9777a0u) /* 1.4430354833602905 */);
    x7 = x2;
    x7 = subsd(x7, x6.lo);
    x6 = cvtsd2ss(x6, x7.lo);
    x6 = cvtss2sd(x6, x6.l(0));
    x4 = addsd(x4, x6.lo);
    x6 = x2;
    x6 = divsd(x6, x4.lo);
    x7 = cvtss2sd(x7, x1.l(0));
    x7 = mulsd(x7, rd64(m, 0x977790u) /* 0.0193784236907959 */);
    x4 = cvtsd2ss(x4, x6.lo);
    x6 = cvtss2sd(x6, x4.l(0));
    x4 = movsd_m(rd64(m, 0x977798u) /* 0.3993034064769745 */);
    x4 = subsd(x4, x7.lo);
    x1 = cvtss2sd(x1, x1.l(0));
    x1 = mulsd(x1, rd64(m, 0x977788u) /* 1.2763687372207642 */);
    x4 = cvtpd2ps(x4);
    x2 = subsd(x2, x1.lo);
    x4 = cvtss2sd(x4, x4.l(0));
    x1 = cvtpd2ps(x2);
    x2 = movsd_m(rd64(m, 0x977510u) /* 1.5707963705062866 */);
    x0 = cvtss2sd(x0, x0.l(0));
    x3 = cvtss2sd(x3, x3.l(0));
    x4 = mulsd(x4, x3.lo);
    x1 = cvtss2sd(x1, x1.l(0));
    x4 = addsd(x4, x1.lo);
    x4 = mulsd(x4, x0.lo);
    x0 = cvtpd2ps(x4);
    x0 = cvtss2sd(x0, x0.l(0));
    x6 = mulsd(x6, x0.lo);
    x0 = cvtsd2ss(x0, x6.lo);
    if (fc_b(fC_6d145b)) goto L_6d1558;
    x0 = cvtss2sd(x0, x0.l(0));
    x1 = x2;
    x1 = subsd(x1, x0.lo);
    x0 = cvtsd2ss(x0, x1.lo);
L_6d1558:
    x1 = movss_m(rd32(m, esp + 0x8u));
    x6 = X{};
    fC_6d1561 = fcmp32(x1.l(0), x6.l(0));
    if (fc_b(fC_6d1561)) goto L_6d1579;
    x1 = x0;
    x0 = cvtps2pd(x1.lo);
    x2 = subsd(x2, x0.lo);
    x0 = cvtpd2ps(x2);
    goto L_6d1737;
L_6d1579:
    x1 = movss_m(rd32(m, 0x9774a8u) /* -0.0 */);
    x1 = subss(x1, x0.l(0));
    x0 = cvtps2pd(x1.lo);
    x2 = subsd(x2, x0.lo);
    x0 = cvtpd2ps(x2);
    goto L_6d1737;
L_6d1595:
    fC_6d1595 = fcmp32(x6.l(0), x1.l(0));
    if ((fc_b(fC_6d1595) || fc_z(fC_6d1595))) goto L_6d159f;
    x1 = x6;
    goto L_6d15ac;
L_6d159f:
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    fC_6d15a7 = fcmp32(x1.l(0), x0.l(0));
    if (!(fc_b(fC_6d15a7) || fc_z(fC_6d15a7))) goto L_6d15af;
L_6d15ac:
    x0 = x1;
L_6d15af:
    wr32(m, esp + 0x8u, x0.l(0));
    LIFT_CALL(0x401040u, 0x6d15bau);  // FUN_00401040
    x5 = x0;
    fC_6d15bd = fcmp32(x5.l(0), rd32(m, 0x9ddfc8u));
    wr32(m, esp + 0xcu, x5.l(0));
    if (fc_b(fC_6d15bd)) goto L_6d160d;
    x0 = cvtss2sd(x0, rd32(m, esp + 0x8u));
    x1 = x0;
    x1 = mulsd(x1, x0.lo);
    x0 = cvtsd2ss(x0, x1.lo);
    x1 = movsd_m(rd64(m, 0x977638u) /* 1.0 */);
    x0 = cvtss2sd(x0, x0.l(0));
    x1 = subsd(x1, x0.lo);
    x0 = cvtpd2ps(x1);
    LIFT_CALL(0x401040u, 0x6d15f7u);  // FUN_00401040
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, x0.l(0));
    LIFT_CALL(0x42c230u, 0x6d1602u);  // FUN_0042c230
    x5 = movss_m(rd32(m, esp + 0x10u));
    esp = esp + 0x4u;
    goto L_6d1610;
L_6d160d:
    x0 = x5;
L_6d1610:
    fC_6d1610 = fcmp32(x5.l(0), rd32(m, 0x9ddfc8u));
    x1 = cvtss2sd(x1, x0.l(0));
    x2 = x1;
    x2 = mulsd(x2, x1.lo);
    x1 = cvtsd2ss(x1, x2.lo);
    x2 = cvtss2sd(x2, x1.l(0));
    x3 = x2;
    x3 = mulsd(x3, x2.lo);
    x2 = movsd_m(rd64(m, 0x9777b0u) /* 0.5648093223571777 */);
    x4 = cvtss2sd(x4, x1.l(0));
    x4 = mulsd(x4, rd64(m, 0x9777a8u) /* 0.04992850869894028 */);
    x2 = subsd(x2, x4.lo);
    x2 = cvtpd2ps(x2);
    x4 = cvtss2sd(x4, x2.l(0));
    x3 = cvtsd2ss(x3, x3.lo);
    x2 = cvtss2sd(x2, x3.l(0));
    x4 = mulsd(x4, x2.lo);
    x2 = movsd_m(rd64(m, 0x977638u) /* 1.0 */);
    x6 = cvtss2sd(x6, x1.l(0));
    x6 = mulsd(x6, rd64(m, 0x9777a0u) /* 1.4430354833602905 */);
    x7 = x2;
    x7 = subsd(x7, x6.lo);
    x6 = cvtsd2ss(x6, x7.lo);
    x6 = cvtss2sd(x6, x6.l(0));
    x4 = addsd(x4, x6.lo);
    x6 = x2;
    x6 = divsd(x6, x4.lo);
    x7 = cvtss2sd(x7, x1.l(0));
    x7 = mulsd(x7, rd64(m, 0x977790u) /* 0.0193784236907959 */);
    x4 = cvtsd2ss(x4, x6.lo);
    x6 = cvtss2sd(x6, x4.l(0));
    x4 = movsd_m(rd64(m, 0x977798u) /* 0.3993034064769745 */);
    x4 = subsd(x4, x7.lo);
    x1 = cvtss2sd(x1, x1.l(0));
    x1 = mulsd(x1, rd64(m, 0x977788u) /* 1.2763687372207642 */);
    x4 = cvtpd2ps(x4);
    x4 = cvtss2sd(x4, x4.l(0));
    x2 = subsd(x2, x1.lo);
    x0 = cvtss2sd(x0, x0.l(0));
    x3 = cvtss2sd(x3, x3.l(0));
    x4 = mulsd(x4, x3.lo);
    x1 = cvtpd2ps(x2);
    x1 = cvtss2sd(x1, x1.l(0));
    x4 = addsd(x4, x1.lo);
    x4 = mulsd(x4, x0.lo);
    x0 = cvtpd2ps(x4);
    x0 = cvtss2sd(x0, x0.l(0));
    x6 = mulsd(x6, x0.lo);
    x0 = cvtsd2ss(x0, x6.lo);
    if (fc_b(fC_6d1610)) goto L_6d1709;
    x1 = movsd_m(rd64(m, 0x977510u) /* 1.5707963705062866 */);
    x0 = cvtss2sd(x0, x0.l(0));
    x1 = subsd(x1, x0.lo);
    x0 = cvtpd2ps(x1);
L_6d1709:
    x1 = movss_m(rd32(m, esp + 0x8u));
    x6 = X{};
    fC_6d1712 = fcmp32(x1.l(0), x6.l(0));
    if (fc_b(fC_6d1712)) goto L_6d171c;
    x1 = x0;
    goto L_6d1728;
L_6d171c:
    x1 = movss_m(rd32(m, 0x9774a8u) /* -0.0 */);
    x1 = subss(x1, x0.l(0));
L_6d1728:
    x0 = cvtps2pd(x1.lo);
    x0 = subsd(x0, rd64(m, 0x977510u) /* 1.5707963705062866 */);
    x0 = cvtsd2ss(x0, x0.lo);
L_6d1737:
    x1 = movss_m(rd32(m, esi + 0x830u));
    x1 = divss(x1, rd32(m, esi + 0x834u));
    x1 = mulss(x1, rd32(m, 0x9cc8a8u));
    x1 = mulss(x1, x0.l(0));
    fC_6d1753 = fcmp32(x1.l(0), x6.l(0));
    if (fc_b(fC_6d1753)) goto L_6d1762;
    x4 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    goto L_6d176a;
L_6d1762:
    x4 = movss_m(rd32(m, 0x9cd9e8u));
L_6d176a:
    x3 = movss_m(rd32(m, 0x977498u) /* 3.1415927410125732 */);
    x0 = x4;
    x0 = mulss(x0, x1.l(0));
    x1 = x0;
    x1 = mulss(x1, rd32(m, 0x9774a0u) /* 0.15915493667125702 */);
    eax = cvttss2si(x1.l(0));
    x1 = cvtsi2ss(x1, eax);
    x1 = mulss(x1, rd32(m, 0x97749cu) /* 6.2831854820251465 */);
    x0 = subss(x0, x1.l(0));
    x3 = subss(x3, x0.l(0));
    x0 = x3;
    x0 = mulss(x0, x3.l(0));
    x1 = x0;
    x1 = mulss(x1, rd32(m, 0x935d68u) /* -2.0362239894211598e-08 */);
    x1 = addss(x1, rd32(m, 0x935d64u) /* 2.6997149689123034e-06 */);
    x2 = x0;
    x2 = mulss(x2, x0.l(0));
    x1 = mulss(x1, x2.l(0));
    x5 = x0;
    x5 = mulss(x5, rd32(m, 0x935d60u) /* -0.00019808630167972296 */);
    x5 = addss(x5, rd32(m, 0x935d5cu) /* 0.008332403376698494 */);
    x1 = addss(x1, x5.l(0));
    x1 = mulss(x1, x2.l(0));
    x5 = x0;
    x5 = mulss(x5, rd32(m, 0x935d58u) /* -0.16666549444198608 */);
    x5 = addss(x5, rd32(m, 0x935d54u) /* 0.9999995827674866 */);
    x1 = addss(x1, x5.l(0));
    x5 = movss_m(rd32(m, esp + 0x18u));
    x1 = mulss(x1, x3.l(0));
    x1 = mulss(x1, x4.l(0));
    x3 = x0;
    x3 = mulss(x3, rd32(m, 0x935d50u) /* -2.1972959984850604e-07 */);
    x3 = addss(x3, rd32(m, 0x935d48u) /* 2.4202940039685927e-05 */);
    x3 = mulss(x3, x2.l(0));
    x4 = x0;
    x0 = mulss(x0, rd32(m, 0x935d40u) /* -0.4999941885471344 */);
    x4 = mulss(x4, rd32(m, 0x935d44u) /* -0.0013858790043741465 */);
    x0 = addss(x0, rd32(m, 0x935d3cu) /* 0.9999992251396179 */);
    x4 = addss(x4, rd32(m, 0x935d4cu) /* 0.04165977984666824 */);
    x3 = addss(x3, x4.l(0));
    x4 = movss_m(rd32(m, esp + 0x10u));
    x3 = mulss(x3, x2.l(0));
    x3 = addss(x3, x0.l(0));
    x0 = movss_m(rd32(m, 0x9774a8u) /* -0.0 */);
    x0 = subss(x0, x3.l(0));
    x2 = x1;
    x2 = mulss(x2, x5.l(0));
    x1 = mulss(x1, x4.l(0));
    x3 = x0;
    x3 = mulss(x3, x4.l(0));
    x2 = addss(x2, x3.l(0));
    x3 = movss_m(rd32(m, esp + 0x14u));
    wr32(m, esp + 0x20u, x2.l(0));
    x0 = mulss(x0, x5.l(0));
    x0 = subss(x0, x1.l(0));
    x1 = x2;
    x1 = mulss(x1, x2.l(0));
    x2 = x3;
    x2 = mulss(x2, x3.l(0));
    x1 = addss(x1, x2.l(0));
    x2 = x0;
    x2 = mulss(x2, x0.l(0));
    x1 = addss(x1, x2.l(0));
    fC_6d18a0 = fcmp32(x1.l(0), x6.l(0));
    wr32(m, esp + 0x24u, x3.l(0));
    wr32(m, esp + 0x28u, x0.l(0));
    wr32(m, esp + 0xcu, x1.l(0));
    if ((fc_b(fC_6d18a0) || fc_z(fC_6d18a0))) goto L_6d1903;
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0xcu)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x42c230u, 0x6d18c4u);  // FUN_0042c230
    x6 = X{};
    x1 = x0;
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x0 = divss(x0, x1.l(0));
    x1 = x0;
    x1 = mulss(x1, rd32(m, esp + 0x24u));
    wr32(m, esp + 0x24u, x1.l(0));
    x1 = x0;
    x1 = mulss(x1, rd32(m, esp + 0x18u));
    x0 = mulss(x0, rd32(m, esp + 0x2cu));
    esp = esp + 0x4u;
    wr32(m, esp + 0x24u, x1.l(0));
    wr32(m, esp + 0x28u, x0.l(0));
L_6d1903:
    fR_6d1903 = (rd8(m, 0x9e30e0u) & 0x1u);
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    wr32(m, esp + 0x10u, x6.l(0));
    wr32(m, esp + 0x14u, x0.l(0));
    wr32(m, esp + 0x18u, x6.l(0));
    if (!(fR_6d1903 == 0)) goto L_6d1937;
    wr32(m, 0x9e30e0u, (rd32(m, 0x9e30e0u) | 0x1u));
    wr32(m, 0x9e30dcu, 0x0u);
L_6d1937:
    edx = 0x9e30d4u;
    LIFT_CALL(0x402dc0u, 0x6d1941u);  // FUN_00402dc0
    fA_6d1941 = rd32(m, esi + 0x854u);
    fB_6d1941 = 0x0u;
    if (fA_6d1941 == fB_6d1941) goto L_6d19ce;
    fR_6d194e = (eax & eax);
    if ((fR_6d194e == 0)) goto L_6d1961;
    ecx = (esp + 0x30u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0x18u);
    LIFT_CALL(0x7c5c90u, 0x6d1961u);  // FUN_007c5c90
L_6d1961:
    x3 = movss_m(rd32(m, esp + 0x24u));
    x0 = movss_m(rd32(m, 0x9774a8u) /* -0.0 */);
    x0 = subss(x0, rd32(m, esp + 0x20u));
    x4 = movss_m(rd32(m, esp + 0x18u));
    x5 = movss_m(rd32(m, esp + 0x14u));
    x6 = movss_m(rd32(m, esp + 0x10u));
    x2 = x0;
    x2 = mulss(x2, x5.l(0));
    x1 = x3;
    x1 = mulss(x1, x4.l(0));
    x1 = subss(x1, x2.l(0));
    x2 = movss_m(rd32(m, esp + 0x28u));
    x7 = x2;
    x0 = mulss(x0, x6.l(0));
    x7 = mulss(x7, x4.l(0));
    x2 = mulss(x2, x5.l(0));
    x3 = mulss(x3, x6.l(0));
    x0 = subss(x0, x7.l(0));
    x2 = subss(x2, x3.l(0));
    wr32(m, esp + 0x20u, x1.l(0));
    wr32(m, esp + 0x24u, x0.l(0));
    wr32(m, esp + 0x28u, x2.l(0));
    goto L_6d19de;
L_6d19ce:
    fR_6d19ce = (eax & eax);
    if ((fR_6d19ce == 0)) goto L_6d19de;
    edx = (esp + 0x30u);
    esp -= 4; wr32(m, esp, edx);
    ecx = eax;
    LIFT_CALL(0x7c5950u, 0x6d19deu);  // FUN_007c5950
L_6d19de:
    eax = (esp + 0x40u);
    esp -= 4; wr32(m, esp, eax);
    eax = (esp + 0x24u);
    ecx = (esp + 0x14u);
    LIFT_CALL(0x401630u, 0x6d19f0u);  // FUN_00401630
    x0 = X{};
    x1 = movss_m(rd32(m, esp + 0x44u));
    wr32(m, esp + 0x70u, x0.l(0));
    wr32(m, esp + 0x80u, x0.l(0));
    wr32(m, esp + 0x90u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x34u));
    wr32(m, esp + 0x94u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x38u));
    edx = rd32(m, esi);
    wr32(m, esp + 0x98u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x3cu));
    edx = rd32(m, edx + 0x40u);
    wr32(m, esp + 0x9cu, x0.l(0));
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    wr32(m, esp + 0xa0u, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x7e0u));
    x1 = mulss(x1, x0.l(0));
    wr32(m, esp + 0x64u, x1.l(0));
    x1 = movss_m(rd32(m, esp + 0x48u));
    x1 = mulss(x1, x0.l(0));
    wr32(m, esp + 0x68u, x1.l(0));
    x1 = movss_m(rd32(m, esp + 0x4cu));
    x1 = mulss(x1, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x7e4u));
    wr32(m, esp + 0x6cu, x1.l(0));
    x1 = x0;
    x1 = mulss(x1, rd32(m, esp + 0x14u));
    wr32(m, esp + 0x74u, x1.l(0));
    x1 = x0;
    x1 = mulss(x1, rd32(m, esp + 0x18u));
    x0 = mulss(x0, rd32(m, esp + 0x1cu));
    wr32(m, esp + 0x78u, x1.l(0));
    wr32(m, esp + 0x7cu, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x7e8u));
    x1 = x0;
    x1 = mulss(x1, rd32(m, esp + 0x24u));
    wr32(m, esp + 0x84u, x1.l(0));
    x1 = movss_m(rd32(m, esp + 0x28u));
    esp = esp + 0x4u;
    x1 = mulss(x1, x0.l(0));
    x0 = mulss(x0, rd32(m, esp + 0x28u));
    eax = (esp + 0x60u);
    esp -= 4; wr32(m, esp, eax);
    ecx = esi;
    wr32(m, esp + 0x88u, x1.l(0));
    wr32(m, esp + 0x8cu, x0.l(0));
    LIFT_CALL(edx, 0x6d1b00u);
    x1 = movss_m(rd32(m, esi + 0x780u));
    x6 = movss_m(rd32(m, esp + 0x30u));
    x0 = movss_m(rd32(m, esi + 0x784u));
    x5 = movss_m(rd32(m, esp + 0x34u));
    x2 = movss_m(rd32(m, esi + 0x788u));
    x4 = movss_m(rd32(m, esp + 0x38u));
    x3 = movss_m(rd32(m, 0x977d64u) /* 9999999.0 */);
    x1 = subss(x1, x6.l(0));
    x7 = x1;
    x0 = subss(x0, x5.l(0));
    x7 = mulss(x7, x1.l(0));
    x2 = subss(x2, x4.l(0));
    x0 = mulss(x0, x0.l(0));
    x1 = x2;
    x0 = addss(x0, x7.l(0));
    x1 = mulss(x1, x2.l(0));
    eax = 0u;
    x0 = addss(x0, x1.l(0));
    fC_6d1b5a = fcmp32(x3.l(0), x0.l(0));
    if ((fc_b(fC_6d1b5a) || fc_z(fC_6d1b5a))) goto L_6d1b62;
    x3 = x0;
L_6d1b62:
    x1 = movss_m(rd32(m, esi + 0x790u));
    x0 = movss_m(rd32(m, esi + 0x794u));
    x2 = movss_m(rd32(m, esi + 0x798u));
    x1 = subss(x1, x6.l(0));
    x7 = x1;
    x0 = subss(x0, x5.l(0));
    x7 = mulss(x7, x1.l(0));
    x2 = subss(x2, x4.l(0));
    x0 = mulss(x0, x0.l(0));
    x1 = x2;
    x0 = addss(x0, x7.l(0));
    x1 = mulss(x1, x2.l(0));
    x0 = addss(x0, x1.l(0));
    fC_6d1ba0 = fcmp32(x3.l(0), x0.l(0));
    if ((fc_b(fC_6d1ba0) || fc_z(fC_6d1ba0))) goto L_6d1bad;
    x3 = x0;
    eax = 0x1u;
L_6d1bad:
    x1 = movss_m(rd32(m, esi + 0x7a4u));
    x2 = movss_m(rd32(m, esi + 0x7a8u));
    x0 = movss_m(rd32(m, esi + 0x7a0u));
    x2 = subss(x2, x4.l(0));
    x1 = subss(x1, x5.l(0));
    x0 = subss(x0, x6.l(0));
    x4 = x1;
    x4 = mulss(x4, x1.l(0));
    x1 = x0;
    x1 = mulss(x1, x0.l(0));
    x0 = x2;
    x4 = addss(x4, x1.l(0));
    x0 = mulss(x0, x2.l(0));
    x4 = addss(x4, x0.l(0));
    fC_6d1bee = fcmp32(x3.l(0), x4.l(0));
    if ((fc_b(fC_6d1bee) || fc_z(fC_6d1bee))) goto L_6d1bf8;
    eax = 0x2u;
L_6d1bf8:
    wr32(m, esi + 0x7b0u, eax);
    goto L_6d1c0a;
L_6d1c00:
    wr32(m, esi + 0x770u, 0x2u);
L_6d1c0a:
    fA_6d1c0a = rd32(m, esi + 0x86cu);
    fB_6d1c0a = 0x0u;
    if (fA_6d1c0a == fB_6d1c0a) goto L_6d1c72;
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x44u);
    ecx = (esp + 0x40u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = esi;
    LIFT_CALL(edx, 0x6d1c21u);
    eax = rd32(m, esi + 0x86cu);
    x0 = movss_m(rd32(m, esp + 0x40u));
    eax = eax + 0x1b0u;
    fA_6d1c32 = rd32(m, 0x9b7430u);
    fB_6d1c32 = 0x0u;
    wr32(m, eax, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x44u));
    wr32(m, eax + 0x4u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x48u));
    wr32(m, eax + 0x8u, x0.l(0));
    if (fA_6d1c32 != fB_6d1c32) goto L_6d1c72;
    ecx = rd32(m, esi + 0x86cu);
    esp -= 4; wr32(m, esp, 0x0u);
    eax = (esp + 0x60u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, 0x678b70u);
    esp -= 4; wr32(m, esp, 0xau);
    LIFT_CALL(0x805860u, 0x6d1c6fu);  // Audio_PostCommand
    esp = esp + 0x14u;
L_6d1c72:
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 006d2eb0 Fn_006d2eb0
//   original FUN_006d2eb0; register inputs: esi; returns with ret 0
REFLEX_PORT_FN(0x006d2eb0, Fn_006d2eb0, "fx", reflex_port::OUT_EAX) {
    LIFT_ENTRY();
    uint32_t fA_6d2eda = 0;
    uint32_t fB_6d2eda = 0;
    uint32_t fR_6d2efe = 0;
    uint32_t fR_6d2f0c = 0;
    FCmp fC_6d2f8c{};
    uint32_t fA_6d2ff5 = 0;
    uint32_t fB_6d2ff5 = 0;
    uint32_t fA_6d3024 = 0;
    uint32_t fB_6d3024 = 0;
    FCmp fC_6d3045{};
    FCmp fC_6d3110{};
    FCmp fC_6d31f4{};
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x48u;
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x44u);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, edi);
    ecx = (esp + 0x20u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = esi;
    LIFT_CALL(edx, 0x6d2ec9u);
    eax = rd32(m, 0xa946b8u);
    eax = rd8(m, eax + 0x1541cu);
    x4 = X{};
    ebx = 0u;
    fA_6d2eda = eax;
    fB_6d2eda = ebx;
    wr32(m, esp + 0x1cu, eax);
    wr32(m, esp + 0x14u, ebx);
    if (int32_t(sext(fA_6d2eda, 4)) <= int32_t(sext(fB_6d2eda, 4))) goto L_6d2fff;
L_6d2eea:
    edx = rd32(m, 0xa946b8u);
    esp -= 4; wr32(m, esp, 0x3u);
    esp -= 4; wr32(m, esp, ebx);
    edx = edx + 0x7460u;
    LIFT_CALL(0x552b90u, 0x6d2efeu);  // FUN_00552b90
    fR_6d2efe = (eax & eax);
    if ((fR_6d2efe == 0)) goto L_6d2ff4;
    edi = rd32(m, eax + 0x1acu);
    fR_6d2f0c = (edi & edi);
    if ((fR_6d2f0c == 0)) goto L_6d2ff4;
    edx = rd32(m, edi);
    edx = rd32(m, edx + 0x44u);
    eax = (esp + 0x40u);
    esp -= 4; wr32(m, esp, eax);
    ecx = edi;
    LIFT_CALL(edx, 0x6d2f22u);
    eax = rd32(m, edi + 0x194u);
    x0 = movss_m(rd32(m, eax * 4 + 0x92fafcu));
    x0 = addss(x0, rd32(m, esi + 0x368u));
    x1 = movss_m(rd32(m, esp + 0x28u));
    x1 = subss(x1, rd32(m, esp + 0x48u));
    wr32(m, esp + 0x18u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x20u));
    x0 = subss(x0, rd32(m, esp + 0x40u));
    x2 = x1;
    x2 = mulss(x2, x1.l(0));
    wr32(m, esp + 0x38u, x1.l(0));
    x1 = x0;
    x1 = mulss(x1, x0.l(0));
    esp -= 4; wr32(m, esp, ecx);
    x2 = addss(x2, x1.l(0));
    wr32(m, esp + 0x34u, x0.l(0));
    wr32(m, esp, x2.l(0));
    LIFT_CALL(0x42c230u, 0x6d2f80u);  // FUN_0042c230
    x1 = movss_m(rd32(m, esp + 0x1cu));
    x4 = X{};
    esp = esp + 0x4u;
    fC_6d2f8c = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_6d2f8c) || fc_z(fC_6d2f8c))) goto L_6d2ff4;
    x3 = movss_m(rd32(m, esp + 0x38u));
    x1 = subss(x1, x0.l(0));
    x1 = divss(x1, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x30u));
    x0 = mulss(x0, x1.l(0));
    x0 = addss(x0, rd32(m, esi + 0x7c0u));
    wr32(m, esi + 0x7c0u, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x7c4u));
    x2 = x1;
    x2 = mulss(x2, x4.l(0));
    x0 = addss(x0, x2.l(0));
    wr32(m, esi + 0x7c4u, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x7c8u));
    x3 = mulss(x3, x1.l(0));
    x0 = addss(x0, x3.l(0));
    wr32(m, esi + 0x7c8u, x0.l(0));
    wr32(m, esp + 0x14u, 0x1u);
L_6d2ff4:
    ebx = (ebx + 1u);
    fA_6d2ff5 = ebx;
    fB_6d2ff5 = rd32(m, esp + 0x1cu);
    if (int32_t(sext(fA_6d2ff5, 4)) < int32_t(sext(fB_6d2ff5, 4))) goto L_6d2eea;
L_6d2fff:
    eax = rd32(m, esi + 0x7b0u);
    eax = eax + 0x78u;
    eax = (eax << 4);
    x0 = movss_m(rd32(m, eax + esi));
    x1 = movss_m(rd32(m, eax + esi + 0x8u));
    x0 = subss(x0, rd32(m, esp + 0x20u));
    x1 = subss(x1, rd32(m, esp + 0x28u));
    eax = eax + esi;
    fA_6d3024 = rd32(m, esp + 0x14u);
    fB_6d3024 = 0x0u;
    if (fA_6d3024 != fB_6d3024) goto L_6d3068;
    x2 = x1;
    x3 = x0;
    x3 = mulss(x3, x0.l(0));
    x2 = mulss(x2, x1.l(0));
    x2 = addss(x2, x3.l(0));
    x3 = movss_m(rd32(m, 0x92e134u) /* 25.0 */);
    fC_6d3045 = fcmp32(x3.l(0), x2.l(0));
    if ((fc_b(fC_6d3045) || fc_z(fC_6d3045))) goto L_6d3068;
    wr32(m, esi + 0x7c0u, x4.l(0));
    wr32(m, esi + 0x7c4u, x4.l(0));
    wr32(m, esi + 0x7c8u, x4.l(0));
    x0 = x4;
    x1 = x4;
L_6d3068:
    x2 = movss_m(rd32(m, esi + 0x7c0u));
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x7c4u)));
    x2 = addss(x2, x0.l(0));
    wr32(m, esi + 0x7c4u, fst32(m));
    fpop(m);
    x0 = movss_m(rd32(m, esi + 0x7c8u));
    x0 = addss(x0, x1.l(0));
    wr32(m, esi + 0x7c8u, x0.l(0));
    x0 = movss_m(rd32(m, 0x9cc8a8u));
    wr32(m, esi + 0x7c0u, x2.l(0));
    x1 = x2;
    x1 = mulss(x1, x0.l(0));
    wr32(m, esi + 0x7c0u, x1.l(0));
    x1 = movss_m(rd32(m, esi + 0x7c4u));
    x1 = mulss(x1, x0.l(0));
    wr32(m, esi + 0x7c4u, x1.l(0));
    x1 = movss_m(rd32(m, esi + 0x7c8u));
    x1 = mulss(x1, x0.l(0));
    wr32(m, esi + 0x7c8u, x1.l(0));
    x1 = movss_m(rd32(m, esi + 0x7c4u));
    x2 = movss_m(rd32(m, esi + 0x7c8u));
    x0 = movss_m(rd32(m, esi + 0x7c0u));
    x3 = x2;
    x3 = mulss(x3, x2.l(0));
    x5 = x1;
    x5 = mulss(x5, x1.l(0));
    x3 = addss(x3, x5.l(0));
    x5 = x0;
    x5 = mulss(x5, x0.l(0));
    x3 = addss(x3, x5.l(0));
    fC_6d3110 = fcmp32(x3.l(0), x4.l(0));
    wr32(m, esp + 0x20u, x0.l(0));
    wr32(m, esp + 0x24u, x1.l(0));
    wr32(m, esp + 0x28u, x2.l(0));
    wr32(m, esp + 0x1cu, x3.l(0));
    if ((fc_b(fC_6d3110) || fc_z(fC_6d3110))) goto L_6d3169;
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x1cu)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x42c230u, 0x6d313au);  // FUN_0042c230
    x2 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x1 = movss_m(rd32(m, esp + 0x28u));
    x3 = movss_m(rd32(m, esp + 0x2cu));
    x2 = divss(x2, x0.l(0));
    x0 = x2;
    x0 = mulss(x0, rd32(m, esp + 0x24u));
    x1 = mulss(x1, x2.l(0));
    x3 = mulss(x3, x2.l(0));
    esp = esp + 0x4u;
    x2 = x3;
L_6d3169:
    x3 = movss_m(rd32(m, 0x9322ccu) /* 0.5 */);
    x0 = mulss(x0, x3.l(0));
    x1 = mulss(x1, x3.l(0));
    x2 = mulss(x2, x3.l(0));
    x3 = movss_m(rd32(m, esi + 0x7c0u));
    x3 = subss(x3, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x7c4u));
    x0 = subss(x0, x1.l(0));
    wr32(m, esi + 0x7c4u, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x7c8u));
    x0 = subss(x0, x2.l(0));
    wr32(m, esi + 0x7c0u, x3.l(0));
    x2 = x3;
    x3 = mulss(x3, x2.l(0));
    wr32(m, esi + 0x7c8u, x0.l(0));
    x1 = movss_m(rd32(m, esi + 0x7c4u));
    x2 = x1;
    x2 = mulss(x2, x1.l(0));
    x1 = x0;
    x3 = addss(x3, x2.l(0));
    x1 = mulss(x1, x0.l(0));
    esp -= 4; wr32(m, esp, ecx);
    x3 = addss(x3, x1.l(0));
    wr32(m, esp, x3.l(0));
    LIFT_CALL(0x42c230u, 0x6d31e9u);  // FUN_0042c230
    x1 = movss_m(rd32(m, esi + 0x834u));
    esp = esp + 0x4u;
    fC_6d31f4 = fcmp32(x0.l(0), x1.l(0));
    wr32(m, esi + 0x830u, x0.l(0));
    if ((fc_b(fC_6d31f4) || fc_z(fC_6d31f4))) goto L_6d324d;
    x1 = divss(x1, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x7c0u));
    x0 = mulss(x0, x1.l(0));
    wr32(m, esi + 0x7c0u, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x7c4u));
    x0 = mulss(x0, x1.l(0));
    wr32(m, esi + 0x7c4u, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x7c8u));
    x0 = mulss(x0, x1.l(0));
    wr32(m, esi + 0x7c8u, x0.l(0));
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x834u)));
    wr32(m, esi + 0x830u, fst32(m));
    fpop(m);
L_6d324d:
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 006d32c0 LensFlareEntity__vf22
//   original LensFlareEntity::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x006d32c0, LensFlareEntity__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x1061u;
    LIFT_RET(0);
}

// 006d3c40 XenonOnlineContent__vf2
//   original XenonOnlineContent::vf2; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x006d3c40, XenonOnlineContent__vf2, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x6d3d30u, 0x6d3c4cu);  // FUN_006d3d30
    eax = rd32(m, 0xa94328u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x5933a0u, 0x6d3c57u);  // FUN_005933a0
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 006d3d30 Fn_006d3d30
//   original FUN_006d3d30; register inputs: none; returns with ret 4
REFLEX_PORT_FN(0x006d3d30, Fn_006d3d30, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_6d3d4e = 0;
    uint32_t fB_6d3d4e = 0;
    uint32_t fA_6d3de4 = 0;
    uint32_t fB_6d3de4 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0x71cu;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp + 0x718u, eax);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0x8u);
    fA_6d3d4e = rd8(m, ebx + 0x24u);
    fB_6d3d4e = 0x0u;
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    if (fA_6d3d4e != fB_6d3d4e) goto L_6d3e16;
    esp -= 4; wr32(m, esp, 0x103u);
    eax = (esp + 0x61du);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, eax);
    wr8(m, esp + 0x624u, 0x0u);
    LIFT_CALL(0x8fa06au, 0x6d3d76u);  // MSVCR90.DLL::memset
    esp = esp + 0xcu;
    esp -= 4; wr32(m, esp, 0xa899a0u);
    esp -= 4; wr32(m, esp, 0x961070u);
    esi = 0x104u;
    edi = (esp + 0x620u);
    LIFT_CALL(0x78fd60u, 0x6d3d94u);  // FUN_0078fd60
    esp -= 4; wr32(m, esp, 0x96107cu);
    ecx = (esp + 0x24u);
    esp -= 4; wr32(m, esp, 0x104u);
    esi = 0u;
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp + 0x24u, esi);
    LIFT_CALL(rd32(m, 0x916268u) /* 8.286654138901879e-39 */, 0x6d3dafu);
    esp = esp + 0x14u;
    esp -= 4; wr32(m, esp, 0x103u);
    edx = edi;
    esp -= 4; wr32(m, esp, edx);
    eax = (esp + 0x420u);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0x20u, esi);
    LIFT_CALL(rd32(m, 0x916260u) /* 8.286626112932593e-39 */, 0x6d3dccu);
    esp = esp + 0xcu;
    esp -= 4; wr32(m, esp, 0xf44u);
    wr8(m, esp + 0x51fu, 0x0u);
    LIFT_CALL(0x78fbe0u, 0x6d3de1u);  // ReflexAlignedAlloc
    esp = esp + 0x4u;
    fA_6d3de4 = eax;
    fB_6d3de4 = esi;
    if (fA_6d3de4 == fB_6d3de4) goto L_6d3df6;
    esp -= 4; wr32(m, esp, ebx);
    ecx = (esp + 0x14u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x6d3e30u, 0x6d3df4u);  // FUN_006d3e30
    goto L_6d3df8;
L_6d3df6:
    eax = 0u;
L_6d3df8:
    edx = rd32(m, eax);
    wr32(m, esp + 0xcu, eax);
    ecx = eax;
    eax = rd32(m, edx + 0x14u);
    LIFT_CALL(eax, 0x6d3e05u);
    ecx = (esp + 0xcu);
    esp -= 4; wr32(m, esp, ecx);
    edi = (ebx + 0xcu);
    LIFT_CALL(0x8423d0u, 0x6d3e12u);  // FUN_008423d0
    wr8(m, ebx + 0x24u, 0x1u);
L_6d3e16:
    ecx = rd32(m, esp + 0x724u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x6d3e27u);  // __security_check_cookie
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 006d4a70 PCStorage__vf7
//   original PCStorage::vf7; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x006d4a70, PCStorage__vf7, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_6d4a73 = 0;
    uint32_t fB_6d4a73 = 0;
    uint32_t fA_6d4a79 = 0;
    uint32_t fB_6d4a79 = 0;
    uint32_t fR_6d4a89 = 0;
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    fA_6d4a73 = rd8(m, esi + 0xcu);
    fB_6d4a73 = 0x0u;
    if (fA_6d4a73 == fB_6d4a73) goto L_6d4ab2;
    fA_6d4a79 = rd32(m, esi + 0x4u);
    fB_6d4a79 = 0x0u;
    if (fA_6d4a79 != fB_6d4a79) goto L_6d4a9d;
    esp -= 4; wr32(m, esp, 0x1cu);
    LIFT_CALL(0x78fbe0u, 0x6d4a86u);  // ReflexAlignedAlloc
    esp = esp + 0x4u;
    fR_6d4a89 = (eax & eax);
    if ((fR_6d4a89 == 0)) goto L_6d4a98;
    esp -= 4; wr32(m, esp, edi);
    edi = eax;
    LIFT_CALL(0x6d4ac0u, 0x6d4a95u);  // FUN_006d4ac0
    edi = rd32(m, esp); esp += 4;
    goto L_6d4a9a;
L_6d4a98:
    eax = 0u;
L_6d4a9a:
    wr32(m, esi + 0x4u, eax);
L_6d4a9d:
    eax = rd32(m, esi + 0x8u);
    wr8(m, esi + 0xcu, 0x0u);
    ecx = esi;
    esi = rd32(m, 0xa94330u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x6d5600u, 0x6d4ab2u);  // FUN_006d5600
L_6d4ab2:
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 006d7840 KeyMap__vf9
//   original KeyMap::vf9; register inputs: ecx; returns with ret 8
REFLEX_PORT_FN(0x006d7840, KeyMap__vf9, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = rd32(m, esp + 0x4u);
    edx = rd32(m, esp + 0x8u);
    eax = (eax + eax * 8);
    wr32(m, ecx + eax * 4 + 0x34u, edx);
    LIFT_RET(8);
}

// 006d7860 KeyMap__vf10
//   original KeyMap::vf10; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x006d7860, KeyMap__vf10, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = rd32(m, esp + 0x4u);
    eax = (eax + eax * 8);
    eax = rd32(m, ecx + eax * 4 + 0x34u);
    LIFT_RET(4);
}

// 006d7980 MXHighlightTracking_AddProgress
//   original MXHighlightTracking_AddProgress; register inputs: none; returns with ret 12
REFLEX_PORT_FN(0x006d7980, MXHighlightTracking_AddProgress, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_6d798f = 0;
    uint32_t fB_6d798f = 0;
    uint32_t fR_6d79dc = 0;
    uint32_t fR_6d79e4 = 0;
    uint32_t fA_6d79ec = 0;
    uint32_t fB_6d79ec = 0;
    uint32_t fA_6d7a02 = 0;
    uint32_t fB_6d7a02 = 0;
    uint32_t fA_6d7a0c = 0;
    uint32_t fB_6d7a0c = 0;
    uint32_t fR_6d7a47 = 0;
    uint32_t fA_6d7a4f = 0;
    uint32_t fB_6d7a4f = 0;
    uint32_t fA_6d7adf = 0;
    uint32_t fB_6d7adf = 0;
    uint32_t fR_6d7b5d = 0;
    uint32_t fA_6d7b61 = 0;
    uint32_t fB_6d7b61 = 0;
    uint32_t fA_6d7b6d = 0;
    uint32_t fB_6d7b6d = 0;
    uint32_t fR_6d7b77 = 0;
    uint32_t fA_6d7b85 = 0;
    uint32_t fB_6d7b85 = 0;
    uint32_t fA_6d7bab = 0;
    uint32_t fB_6d7bab = 0;
    uint32_t fA_6d7bdf = 0;
    uint32_t fB_6d7bdf = 0;
    uint32_t fA_6d7c00 = 0;
    uint32_t fB_6d7c00 = 0;
    uint32_t fA_6d7c10 = 0;
    uint32_t fB_6d7c10 = 0;
    uint32_t fR_6d7c37 = 0;
    uint32_t fR_6d7c46 = 0;
    uint32_t fA_6d7c8a = 0;
    uint32_t fB_6d7c8a = 0;
    uint32_t fA_6d7c9a = 0;
    uint32_t fB_6d7c9a = 0;
    uint32_t fA_6d7cc2 = 0;
    uint32_t fB_6d7cc2 = 0;
    uint32_t fA_6d7cd8 = 0;
    uint32_t fB_6d7cd8 = 0;
    uint32_t fA_6d7cfb = 0;
    uint32_t fB_6d7cfb = 0;
    uint32_t fA_6d7d19 = 0;
    uint32_t fB_6d7d19 = 0;
    esp = esp - 0x118u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x124u);
    fA_6d798f = rd32(m, ebp + 0x4u);
    fB_6d798f = 0x0u;
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    if (fA_6d798f == fB_6d798f) goto L_6d7d48;
    edi = rd32(m, esp + 0x134u);
    eax = (edi + 0x1u);
    wr32(m, esp + 0x14u, eax);
    esp -= 4; wr32(m, esp, 0x961fd8u);
    eax = 0xae0910u;
    LIFT_CALL(0x402220u, 0x6d79b8u);  // Database_FindTable
    esi = eax;
    eax = rd32(m, esp + 0x130u);
    eax = uint32_t(int32_t(eax) * int32_t(0x1f4u));
    esp -= 4; wr32(m, esp, 0x961fe4u);
    eax = eax + 0xae10f0u;
    LIFT_CALL(0x402220u, 0x6d79d6u);  // Database_FindTable
    ebx = eax;
    wr32(m, esp + 0x18u, ebx);
    fR_6d79dc = (esi & esi);
    if ((fR_6d79dc == 0)) goto L_6d7d48;
    fR_6d79e4 = (ebx & ebx);
    if ((fR_6d79e4 == 0)) goto L_6d7d48;
    fA_6d79ec = rd32(m, ebp + edi * 4 + 0x8u);
    fB_6d79ec = 0xffffffffu;
    if (fA_6d79ec != fB_6d79ec) goto L_6d7a25;
    ecx = (esp + 0x14u);
    esp -= 4; wr32(m, esp, ecx);
    eax = 0x961ff4u;
    LIFT_CALL(0x419ae0u, 0x6d7a02u);  // FUN_00419ae0
    fA_6d7a02 = rd32(m, eax + 0x8u);
    fB_6d7a02 = 0x0u;
    if (fA_6d7a02 == fB_6d7a02) goto L_6d7d48;
    fA_6d7a0c = rd32(m, eax);
    fB_6d7a0c = 0xffffffffu;
    if (fA_6d7a0c == fB_6d7a0c) goto L_6d7d48;
    esp -= 4; wr32(m, esp, 0x962000u);
    LIFT_CALL(0x4199a0u, 0x6d7a1fu);  // FUN_004199a0
    edx = rd32(m, eax);
    wr32(m, ebp + edi * 4 + 0x8u, edx);
L_6d7a25:
    eax = (esp + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    eax = 0x961ff4u;
    esi = ebx;
    LIFT_CALL(0x419ae0u, 0x6d7a36u);  // FUN_00419ae0
    ebp = rd32(m, eax + 0x8u);
    x0 = movsd_m(rd64(m, eax));
    wr64(m, esp + 0x20u, x0.lo);
    wr32(m, esp + 0x28u, ebp);
    fR_6d7a47 = (ebp & ebp);
    if ((fR_6d7a47 == 0)) goto L_6d7a58;
    esi = rd32(m, esp + 0x20u);
    fA_6d7a4f = esi;
    fB_6d7a4f = 0xffffffffu;
    if (fA_6d7a4f != fB_6d7a4f) goto L_6d7bc8;
L_6d7a58:
    esi = (esp + 0x2cu);
    ebx = 0x1u;
    LIFT_CALL(0x5442c0u, 0x6d7a66u);  // FUN_005442c0
    esp -= 4; wr32(m, esp, 0x961ff4u);
    ecx = (esp + 0x18u);
    eax = 0u;
    esi = (esp + 0x7cu);
    LIFT_CALL(0x419a70u, 0x6d7a7au);  // FUN_00419a70
    esp -= 4; wr32(m, esp, eax);
    edi = (esp + 0x30u);
    LIFT_CALL(0x4175e0u, 0x6d7a84u);  // FUN_004175e0
    LIFT_CALL(0x415320u, 0x6d7a89u);  // FUN_00415320
    esp -= 4; wr32(m, esp, 0x962008u);
    ecx = (esp + 0x18u);
    eax = 0u;
    esi = (esp + 0x7cu);
    wr32(m, esp + 0x18u, ebx);
    LIFT_CALL(0x419a70u, 0x6d7aa1u);  // FUN_00419a70
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x4175e0u, 0x6d7aa7u);  // FUN_004175e0
    LIFT_CALL(0x415320u, 0x6d7aacu);  // FUN_00415320
    eax = rd32(m, esp + 0x18u);
    ecx = edi;
    esp -= 4; wr32(m, esp, ecx);
    edx = (esp + 0x70u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x4ec950u, 0x6d7abdu);  // FUN_004ec950
    x0 = movsd_m(rd64(m, eax));
    ebp = rd32(m, eax + 0x8u);
    esi = edi;
    wr64(m, esp + 0x20u, x0.lo);
    LIFT_CALL(0x417590u, 0x6d7ad1u);  // FUN_00417590
L_6d7ad1:
    edi = rd32(m, esp + 0x134u);
L_6d7ad8:
    edx = rd32(m, esp + 0x12cu);
    fA_6d7adf = ebx;
    fB_6d7adf = rd32(m, edx + edi * 4 + 0x8u);
    if (int32_t(sext(fA_6d7adf, 4)) < int32_t(sext(fB_6d7adf, 4))) goto L_6d7d48;
    esi = (esp + 0x2cu);
    LIFT_CALL(0x5442c0u, 0x6d7af2u);  // FUN_005442c0
    esp -= 4; wr32(m, esp, 0x962018u);
    ecx = (esp + 0x18u);
    eax = 0x4u;
    esi = (esp + 0x7cu);
    wr32(m, esp + 0x18u, 0x0u);
    LIFT_CALL(0x419a70u, 0x6d7b11u);  // FUN_00419a70
    esp -= 4; wr32(m, esp, eax);
    edi = (esp + 0x30u);
    LIFT_CALL(0x4175e0u, 0x6d7b1bu);  // FUN_004175e0
    LIFT_CALL(0x415320u, 0x6d7b20u);  // FUN_00415320
    esp -= 4; wr32(m, esp, 0x0u);
    esi = (esp + 0x48u);
    LIFT_CALL(0x4dc570u, 0x6d7b2bu);  // FUN_004dc570
    ebx = rd32(m, esp + 0x18u);
    eax = edi;
    esp -= 4; wr32(m, esp, eax);
    eax = esi;
    ecx = ebx;
    LIFT_CALL(0x4ed2f0u, 0x6d7b3bu);  // FUN_004ed2f0
    ecx = rd32(m, esp + 0x5cu);
    edi = rd32(m, ecx);
    esi = rd32(m, esp + 0x44u);
    wr32(m, esp + 0x14u, 0x0u);
    wr32(m, esp + 0x1cu, edi);
    wr32(m, esp + 0x18u, esi);
L_6d7b55:
    edx = rd32(m, esp + 0x5cu);
    wr32(m, esp + 0x70u, edx);
    fR_6d7b5d = (esi & esi);
    if ((fR_6d7b5d == 0)) goto L_6d7b67;
    fA_6d7b61 = esi;
    fB_6d7b61 = rd32(m, esp + 0x44u);
    if (fA_6d7b61 == fB_6d7b61) goto L_6d7b6d;
L_6d7b67:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x6d7b6du);
L_6d7b6d:
    fA_6d7b6d = edi;
    fB_6d7b6d = rd32(m, esp + 0x70u);
    if (fA_6d7b6d == fB_6d7b6d) goto L_6d7c67;
    fR_6d7b77 = (esi & esi);
    if (!(fR_6d7b77 == 0)) goto L_6d7c60;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x6d7b85u);
L_6d7b85:
    fA_6d7b85 = edi;
    fB_6d7b85 = rd32(m, esi + 0x18u);
    if (fA_6d7b85 != fB_6d7b85) goto L_6d7b90;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x6d7b90u);
L_6d7b90:
    esi = rd32(m, edi + 0xcu);
    esp -= 4; wr32(m, esp, 0x962018u);
    eax = ebx;
    LIFT_CALL(0x402160u, 0x6d7b9fu);  // Table_ColumnIndex
    edi = eax;
    esp -= 4; wr32(m, esp, esi);
    eax = ebx;
    LIFT_CALL(0x41aaf0u, 0x6d7ba9u);  // FUN_0041aaf0
    eax = rd32(m, eax);
    fA_6d7bab = rd32(m, esp + 0x14u);
    fB_6d7bab = eax;
    if (int32_t(sext(fA_6d7bab, 4)) > int32_t(sext(fB_6d7bab, 4))) goto L_6d7bb5;
    wr32(m, esp + 0x14u, eax);
L_6d7bb5:
    esi = (esp + 0x18u);
    LIFT_CALL(0x417df0u, 0x6d7bbeu);  // FUN_00417df0
    edi = rd32(m, esp + 0x1cu);
    esi = rd32(m, esp + 0x18u);
    goto L_6d7b55;
L_6d7bc8:
    esp -= 4; wr32(m, esp, 0x962008u);
    eax = (esp + 0x24u);
    LIFT_CALL(0x4199a0u, 0x6d7bd6u);  // FUN_004199a0
    ebx = rd32(m, eax);
    eax = rd32(m, esp + 0x12cu);
    fA_6d7bdf = ebx;
    fB_6d7bdf = rd32(m, eax + edi * 4 + 0x8u);
    if (int32_t(sext(fA_6d7bdf, 4)) >= int32_t(sext(fB_6d7bdf, 4))) goto L_6d7c31;
    ebx = (ebx + 1u);
    esp -= 4; wr32(m, esp, 0x962008u);
    eax = ebp;
    wr32(m, esp + 0x18u, ebx);
    LIFT_CALL(0x402160u, 0x6d7bf6u);  // Table_ColumnIndex
    edi = eax;
    esp -= 4; wr32(m, esp, esi);
    eax = ebp;
    LIFT_CALL(0x41aaf0u, 0x6d7c00u);  // FUN_0041aaf0
    fA_6d7c00 = ebx;
    fB_6d7c00 = rd32(m, eax);
    if (fA_6d7c00 == fB_6d7c00) goto L_6d7c07;
    wr32(m, ebp + 0x68u, (rd32(m, ebp + 0x68u) + 1u));
L_6d7c07:
    ecx = rd32(m, ebp + 0x14u);
    ecx = ecx - rd32(m, ebp + 0x10u);
    ecx = uint32_t(int32_t(ecx) >> 2);
    fA_6d7c10 = edi;
    fB_6d7c10 = ecx;
    if (fA_6d7c10 < fB_6d7c10) goto L_6d7c1a;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x6d7c1au);
L_6d7c1a:
    eax = rd32(m, ebp + 0x10u);
    ecx = rd32(m, eax + edi * 4);
    edx = (esp + 0x14u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x4f5330u, 0x6d7c2cu);  // FUN_004f5330
    goto L_6d7ad1;
L_6d7c31:
    LIFT_CALL(rd32(m, 0x9166a4u) /* 8.280000773793265e-39 */, 0x6d7c37u);
    fR_6d7c37 = (eax & eax);
    if ((fR_6d7c37 == 0)) goto L_6d7ad8;
    esi = edi;
    LIFT_CALL(0x6d8830u, 0x6d7c46u);  // PCOnlineAchievements_IsAchieved
    fR_6d7c46 = (eax & eax);
    if ((fR_6d7c46 == 0)) goto L_6d7ad8;
    eax = 0x1u;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x118u;
    LIFT_RET(12);
L_6d7c60:
    esi = rd32(m, esi);
    goto L_6d7b85;
L_6d7c67:
    esi = rd32(m, esp + 0x14u);
    esi = (esi + 1u);
    esp -= 4; wr32(m, esp, 0x962018u);
    eax = ebp;
    wr32(m, esp + 0x1cu, esi);
    LIFT_CALL(0x402160u, 0x6d7c7cu);  // Table_ColumnIndex
    ebx = rd32(m, esp + 0x20u);
    edi = eax;
    esp -= 4; wr32(m, esp, ebx);
    eax = ebp;
    LIFT_CALL(0x41aaf0u, 0x6d7c8au);  // FUN_0041aaf0
    fA_6d7c8a = esi;
    fB_6d7c8a = rd32(m, eax);
    if (fA_6d7c8a == fB_6d7c8a) goto L_6d7c91;
    wr32(m, ebp + 0x68u, (rd32(m, ebp + 0x68u) + 1u));
L_6d7c91:
    eax = rd32(m, ebp + 0x14u);
    eax = eax - rd32(m, ebp + 0x10u);
    eax = uint32_t(int32_t(eax) >> 2);
    fA_6d7c9a = edi;
    fB_6d7c9a = eax;
    if (fA_6d7c9a < fB_6d7c9a) goto L_6d7ca4;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x6d7ca4u);
L_6d7ca4:
    edx = rd32(m, ebp + 0x10u);
    eax = rd32(m, edx + edi * 4);
    ecx = (esp + 0x18u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x4f5330u, 0x6d7cb6u);  // FUN_004f5330
    ecx = rd32(m, esp + 0x12cu);
    eax = rd32(m, ecx + 0x4u);
    edi = 0u;
    fA_6d7cc2 = rd32(m, eax + 0x8u);
    fB_6d7cc2 = edi;
    if (fA_6d7cc2 == fB_6d7cc2) goto L_6d7cd4;
    esi = rd32(m, esp + 0x134u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x6d87d0u, 0x6d7cd4u);  // PCOnlineAchievements_Unlock
L_6d7cd4:
    esi = rd32(m, esp + 0x64u);
    fA_6d7cd8 = esi;
    fB_6d7cd8 = edi;
    if (fA_6d7cd8 == fB_6d7cd8) goto L_6d7ce5;
    LIFT_CALL(0x4ebe10u, 0x6d7ce1u);  // FUN_004ebe10
    wr32(m, esp + 0x64u, edi);
L_6d7ce5:
    esi = (esp + 0x44u);
    wr32(m, esp + 0x68u, edi);
    LIFT_CALL(0x4f2220u, 0x6d7cf2u);  // FUN_004f2220
    esi = rd32(m, esp + 0x44u);
    LIFT_CALL(0x4dc0f0u, 0x6d7cfbu);  // FUN_004dc0f0
    fA_6d7cfb = rd8(m, 0xda67d8u);
    fB_6d7cfb = 0x0u;
    if (fA_6d7cfb == fB_6d7cfb) goto L_6d7d0f;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x6d7d0fu);
L_6d7d0f:
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(rd32(m, 0x916350u) /* 8.287536956934404e-39 */, 0x6d7d16u);
    esp = esp + 0x4u;
    fA_6d7d19 = rd8(m, 0xda67d8u);
    fB_6d7d19 = 0x0u;
    if (fA_6d7d19 == fB_6d7d19) goto L_6d7d2d;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x6d7d2du);
L_6d7d2d:
    esi = (esp + 0x2cu);
    LIFT_CALL(0x417590u, 0x6d7d36u);  // FUN_00417590
    eax = 0x1u;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x118u;
    LIFT_RET(12);
L_6d7d48:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x118u;
    LIFT_RET(12);
}

// 006de1c0 Fn_006de1c0
//   original FUN_006de1c0; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x006de1c0, Fn_006de1c0, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX | (reflex_port::OUT_XMM0 << 0) | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fR_6de1c5 = 0;
    eax = 0x1u;
    fR_6de1c5 = (rd8(m, 0xdcf304u) & (eax & 0xffu));
    if (!(fR_6de1c5 == 0)) goto L_6de1e5;
    wr32(m, 0xdcf304u, (rd32(m, 0xdcf304u) | eax));
    LIFT_CALL(0x6de1f0u, 0x6de1d8u);  // FUN_006de1f0
    esp -= 4; wr32(m, esp, 0x913e00u);
    LIFT_CALL(0x8f9fbfu, 0x6de1e2u);  // _atexit
    esp = esp + 0x4u;
L_6de1e5:
    eax = 0xda6ca0u;
    LIFT_RET(0);
}

// 006de690 Fn_006de690
//   original FUN_006de690; register inputs: ecx edi; returns with ret 4
REFLEX_PORT_FN(0x006de690, Fn_006de690, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    FCmp fC_6de6ab{};
    uint32_t fA_6de708 = 0;
    uint32_t fB_6de708 = 0;
    uint32_t fA_6de71b = 0;
    uint32_t fB_6de71b = 0;
    uint32_t fR_6de72e = 0;
    uint32_t fA_6de740 = 0;
    uint32_t fB_6de740 = 0;
    uint32_t fA_6de752 = 0;
    uint32_t fB_6de752 = 0;
    uint32_t fA_6de758 = 0;
    uint32_t fB_6de758 = 0;
    uint32_t fA_6de75e = 0;
    uint32_t fB_6de75e = 0;
    esp -= 4; wr32(m, esp, ecx);
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp, eax);
    eax = rd32(m, edi);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x5933a0u, 0x6de6a6u);  // FUN_005933a0
    x0 = movss_m(rd32(m, edi + 0x64u));
    fC_6de6ab = fcmp32(x0.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    if (fc_b(fC_6de6ab)) goto L_6de6bf;
    x0 = subss(x0, rd32(m, esp + 0x14u));
    wr32(m, edi + 0x64u, x0.l(0));
L_6de6bf:
    ecx = rd32(m, edi + 0xcu);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x14u)));
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x8u);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(eax, 0x6de6d1u);
    ecx = rd32(m, edi + 0x10u);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x14u)));
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x8u);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(eax, 0x6de6e3u);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x14u)));
    esi = rd32(m, edi + 0x44u);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x6ed7e0u, 0x6de6f3u);  // FUN_006ed7e0
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x14u)));
    esi = rd32(m, edi + 0x48u);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x6ee820u, 0x6de703u);  // FUN_006ee820
    ecx = rd32(m, edi + 0x10u);
    edx = 0u;
    fA_6de708 = rd32(m, ecx + 0x7cu);
    fB_6de708 = edx;
    edx = (edx & 0xffffff00u) | (((fA_6de708 != fB_6de708) ? 1u : 0u) & 0xffu);
    wr32(m, edi + 0x68u, edx);
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x88u);
    LIFT_CALL(edx, 0x6de71bu);
    fA_6de71b = rd32(m, edi + 0x74u);
    fB_6de71b = 0x0u;
    if (fA_6de71b == fB_6de71b) goto L_6de779;
    ebx = rd32(m, 0xa946b8u);
    ecx = ebx;
    LIFT_CALL(0x75c5f0u, 0x6de72eu);  // FUN_0075c5f0
    fR_6de72e = (eax & eax);
    if (!(fR_6de72e == 0)) goto L_6de779;
    ebp = rd8(m, ebx + 0x1541cu);
    esi = 0u;
    goto L_6de740;
L_6de740:
    fA_6de740 = esi;
    fB_6de740 = ebp;
    if (int32_t(sext(fA_6de740, 4)) >= int32_t(sext(fB_6de740, 4))) goto L_6de772;
    esp -= 4; wr32(m, esp, 0x3u);
    esp -= 4; wr32(m, esp, esi);
    edx = (ebx + 0x7460u);
    LIFT_CALL(0x552b90u, 0x6de752u);  // FUN_00552b90
    fA_6de752 = rd32(m, eax + 0x4u);
    fB_6de752 = 0x0u;
    if (fA_6de752 == fB_6de752) goto L_6de767;
    fA_6de758 = rd32(m, eax + 0x10u);
    fB_6de758 = 0x0u;
    if (fA_6de758 != fB_6de758) goto L_6de767;
    fA_6de75e = rd32(m, eax + 0xb2cu);
    fB_6de75e = 0x0u;
    if (fA_6de75e != fB_6de75e) goto L_6de76a;
L_6de767:
    esi = (esi + 1u);
    goto L_6de740;
L_6de76a:
    esp -= 4; wr32(m, esp, 0x1u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x54de80u, 0x6de772u);  // FUN_0054de80
L_6de772:
    wr32(m, edi + 0x74u, 0x0u);
L_6de779:
    ecx = rd32(m, esp + 0xcu);
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x6de787u);  // __security_check_cookie
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 006decd0 Fn_006decd0
//   original FUN_006decd0; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x006decd0, Fn_006decd0, "fx", reflex_port::OUT_EAX | reflex_port::OUT_EDX | (reflex_port::OUT_XMM0 << 0) | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fR_6decf1 = 0;
    uint32_t fA_6ded01 = 0;
    uint32_t fR_6ded01 = 0;
    uint32_t fR_6ded0e = 0;
    uint32_t fA_6ded22 = 0;
    uint32_t fR_6ded22 = 0;
    esp = esp - 0xcu;
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, ecx);
    ecx = rd32(m, 0xd60d54u);
    eax = esp;
    wr32(m, eax, ecx);
    ecx = (esp + 0x8u);
    LIFT_CALL(0x88bcd0u, 0x6dece8u);  // FUN_0088bcd0
    eax = (esp + 0x4u);
    LIFT_CALL(0x703070u, 0x6decf1u);  // FUN_00703070
    fR_6decf1 = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_6decf1 == 0)) goto L_6ded33;
    eax = (esp + 0x4u);
    LIFT_CALL(0x703070u, 0x6decfeu);  // FUN_00703070
    ecx = (eax & 0xffu);
    fA_6ded01 = ecx;
    fR_6ded01 = (0u - fA_6ded01);
    ecx = fR_6ded01;
    ecx = ecx - ecx - ((fA_6ded01 != 0) ? 1u : 0u);
    ecx = (ecx & rd32(m, esp + 0x4u));
    LIFT_CALL(0x884950u, 0x6ded0eu);  // FUN_00884950
    fR_6ded0e = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_6ded0e == 0)) goto L_6ded33;
    eax = (esp + 0x4u);
    LIFT_CALL(0x703070u, 0x6ded1bu);  // FUN_00703070
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x14u)));
    edi = (eax & 0xffu);
    fA_6ded22 = edi;
    fR_6ded22 = (0u - fA_6ded22);
    edi = fR_6ded22;
    edi = edi - edi - ((fA_6ded22 != 0) ? 1u : 0u);
    edi = (edi & rd32(m, esp + 0x4u));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x6fbfe0u, 0x6ded33u);  // FUN_006fbfe0
L_6ded33:
    ecx = (esp + 0x4u);
    LIFT_CALL(0x88bac0u, 0x6ded3cu);  // FUN_0088bac0
    edi = rd32(m, esp); esp += 4;
    esp = esp + 0xcu;
    LIFT_RET(4);
}
