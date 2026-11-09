// Particle / world-entity ports (group "fx"), transliterated instruction by instruction from MXReflex.exe by the fx
// lifter (a copy of the bike lifter, see native/game/notes/fx.md). Each function keeps the original's operations, order and
// memory traffic (stack frame included).
#include <stdexcept>
#include <utility>
#include "lift_rt.h"

// 00401060 Fn_00401060
//   original FUN_00401060; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x00401060, Fn_00401060, "fx", reflex_port::OUT_ECX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = rd32(m, esp + 0x4u);
    edx = rd32(m, eax);
    wr32(m, ecx, edx);
    LIFT_RET(4);
}

// 00401070 Fn_00401070
//   original FUN_00401070; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x00401070, Fn_00401070, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fR_401072 = 0;
    eax = rd32(m, ecx);
    fR_401072 = (eax & eax);
    if ((fR_401072 == 0)) goto L_401079;
    eax = rd32(m, eax);
    LIFT_RET(0);
L_401079:
    eax = 0u;
    LIFT_RET(0);
}

// 004010e0 Fn_004010e0
//   original FUN_004010e0; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x004010e0, Fn_004010e0, "fx", reflex_port::OUT_EAX) {
    LIFT_ENTRY();

    eax = rd32(m, esp + 0x8u);
    LIFT_RET(0);
}

// 00401830 Fn_00401830
//   original FUN_00401830; register inputs: ecx eax; returns with ret 0
REFLEX_PORT_FN(0x00401830, Fn_00401830, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x48u;
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    x6 = movss_m(rd32(m, eax));
    x5 = movss_m(rd32(m, eax + 0x10u));
    x3 = movss_m(rd32(m, ecx));
    x1 = movss_m(rd32(m, ecx + 0x8u));
    x2 = movss_m(rd32(m, ecx + 0x4u));
    x0 = movss_m(rd32(m, ecx + 0xcu));
    x4 = movss_m(rd32(m, eax + 0x30u));
    x7 = x6;
    x7 = mulss(x7, x3.l(0));
    x3 = x5;
    x3 = mulss(x3, x2.l(0));
    x7 = addss(x7, x3.l(0));
    x2 = movss_m(rd32(m, eax + 0x20u));
    x3 = x1;
    x3 = mulss(x3, x2.l(0));
    x7 = addss(x7, x3.l(0));
    x3 = movss_m(rd32(m, eax + 0x14u));
    x2 = x0;
    x2 = mulss(x2, x4.l(0));
    x7 = addss(x7, x2.l(0));
    x2 = movss_m(rd32(m, eax + 0x4u));
    wr32(m, esp + 0x10u, x7.l(0));
    x7 = x2;
    x2 = movss_m(rd32(m, ecx));
    x7 = mulss(x7, x2.l(0));
    x2 = movss_m(rd32(m, ecx + 0x4u));
    x3 = mulss(x3, x2.l(0));
    x7 = addss(x7, x3.l(0));
    x3 = movss_m(rd32(m, eax + 0x24u));
    x3 = mulss(x3, x1.l(0));
    x7 = addss(x7, x3.l(0));
    x3 = movss_m(rd32(m, eax + 0x34u));
    x3 = mulss(x3, x0.l(0));
    x7 = addss(x7, x3.l(0));
    x3 = movss_m(rd32(m, eax + 0x8u));
    wr32(m, esp + 0x14u, x7.l(0));
    x7 = x3;
    x3 = movss_m(rd32(m, ecx));
    x7 = mulss(x7, x3.l(0));
    x3 = movss_m(rd32(m, eax + 0x18u));
    x3 = mulss(x3, x2.l(0));
    x7 = addss(x7, x3.l(0));
    x3 = movss_m(rd32(m, eax + 0x28u));
    x3 = mulss(x3, x1.l(0));
    x7 = addss(x7, x3.l(0));
    x3 = movss_m(rd32(m, eax + 0x38u));
    x3 = mulss(x3, x0.l(0));
    x7 = addss(x7, x3.l(0));
    x3 = movss_m(rd32(m, eax + 0xcu));
    wr32(m, esp + 0x18u, x7.l(0));
    x7 = x3;
    x3 = movss_m(rd32(m, ecx));
    x7 = mulss(x7, x3.l(0));
    x3 = movss_m(rd32(m, eax + 0x1cu));
    x3 = mulss(x3, x2.l(0));
    x2 = movss_m(rd32(m, eax + 0x2cu));
    x7 = addss(x7, x3.l(0));
    x3 = movss_m(rd32(m, ecx + 0x14u));
    x2 = mulss(x2, x1.l(0));
    x1 = movss_m(rd32(m, eax + 0x3cu));
    x7 = addss(x7, x2.l(0));
    x2 = movss_m(rd32(m, ecx + 0x18u));
    x1 = mulss(x1, x0.l(0));
    x0 = movss_m(rd32(m, ecx + 0x10u));
    x7 = addss(x7, x1.l(0));
    x1 = movss_m(rd32(m, ecx + 0x1cu));
    wr32(m, esp + 0x1cu, x7.l(0));
    x7 = x0;
    x7 = mulss(x7, x6.l(0));
    x6 = x3;
    x6 = mulss(x6, x5.l(0));
    x5 = movss_m(rd32(m, eax + 0x20u));
    x7 = addss(x7, x6.l(0));
    x6 = x2;
    x6 = mulss(x6, x5.l(0));
    x5 = x1;
    x7 = addss(x7, x6.l(0));
    x5 = mulss(x5, x4.l(0));
    x7 = addss(x7, x5.l(0));
    x5 = movss_m(rd32(m, eax + 0x4u));
    wr32(m, esp + 0x20u, x7.l(0));
    x6 = x0;
    x6 = mulss(x6, x5.l(0));
    x5 = movss_m(rd32(m, eax + 0x14u));
    x7 = x3;
    x7 = mulss(x7, x5.l(0));
    x6 = addss(x6, x7.l(0));
    x5 = movss_m(rd32(m, eax + 0x24u));
    x7 = x2;
    x7 = mulss(x7, x5.l(0));
    x6 = addss(x6, x7.l(0));
    x5 = movss_m(rd32(m, eax + 0x34u));
    x7 = x1;
    x7 = mulss(x7, x5.l(0));
    x6 = addss(x6, x7.l(0));
    x5 = movss_m(rd32(m, eax + 0x8u));
    wr32(m, esp + 0x24u, x6.l(0));
    x6 = x0;
    x6 = mulss(x6, x5.l(0));
    x5 = movss_m(rd32(m, eax + 0x18u));
    x7 = x3;
    x7 = mulss(x7, x5.l(0));
    x5 = movss_m(rd32(m, eax + 0x28u));
    x6 = addss(x6, x7.l(0));
    x7 = x2;
    x7 = mulss(x7, x5.l(0));
    x5 = movss_m(rd32(m, eax + 0x38u));
    x6 = addss(x6, x7.l(0));
    x7 = x1;
    x7 = mulss(x7, x5.l(0));
    x5 = movss_m(rd32(m, eax + 0xcu));
    x0 = mulss(x0, x5.l(0));
    x5 = movss_m(rd32(m, eax + 0x1cu));
    x6 = addss(x6, x7.l(0));
    x3 = mulss(x3, x5.l(0));
    x5 = movss_m(rd32(m, eax));
    x0 = addss(x0, x3.l(0));
    x3 = movss_m(rd32(m, eax + 0x2cu));
    wr32(m, esp + 0x28u, x6.l(0));
    x2 = mulss(x2, x3.l(0));
    x3 = movss_m(rd32(m, ecx + 0x24u));
    x0 = addss(x0, x2.l(0));
    x2 = movss_m(rd32(m, eax + 0x3cu));
    x1 = mulss(x1, x2.l(0));
    x2 = movss_m(rd32(m, ecx + 0x28u));
    x0 = addss(x0, x1.l(0));
    x1 = movss_m(rd32(m, ecx + 0x2cu));
    wr32(m, esp + 0x2cu, x0.l(0));
    x0 = movss_m(rd32(m, ecx + 0x20u));
    x6 = x0;
    x6 = mulss(x6, x5.l(0));
    x5 = movss_m(rd32(m, eax + 0x10u));
    x7 = x3;
    x7 = mulss(x7, x5.l(0));
    x5 = movss_m(rd32(m, eax + 0x20u));
    x6 = addss(x6, x7.l(0));
    x7 = x2;
    x7 = mulss(x7, x5.l(0));
    x6 = addss(x6, x7.l(0));
    x5 = x1;
    x5 = mulss(x5, x4.l(0));
    x6 = addss(x6, x5.l(0));
    x5 = movss_m(rd32(m, eax + 0x4u));
    wr32(m, esp + 0x30u, x6.l(0));
    x6 = x0;
    x6 = mulss(x6, x5.l(0));
    x5 = movss_m(rd32(m, eax + 0x14u));
    x7 = x3;
    x7 = mulss(x7, x5.l(0));
    x5 = movss_m(rd32(m, eax + 0x24u));
    x6 = addss(x6, x7.l(0));
    x7 = x2;
    x7 = mulss(x7, x5.l(0));
    x5 = movss_m(rd32(m, eax + 0x34u));
    x6 = addss(x6, x7.l(0));
    x7 = x1;
    x7 = mulss(x7, x5.l(0));
    x6 = addss(x6, x7.l(0));
    wr32(m, esp + 0x34u, x6.l(0));
    x6 = x0;
    x5 = movss_m(rd32(m, eax + 0x8u));
    x6 = mulss(x6, x5.l(0));
    x5 = movss_m(rd32(m, eax + 0x18u));
    x7 = x3;
    x7 = mulss(x7, x5.l(0));
    x5 = movss_m(rd32(m, eax + 0x28u));
    x6 = addss(x6, x7.l(0));
    x7 = x2;
    x7 = mulss(x7, x5.l(0));
    x5 = movss_m(rd32(m, eax + 0x38u));
    x6 = addss(x6, x7.l(0));
    x7 = x1;
    x7 = mulss(x7, x5.l(0));
    x5 = movss_m(rd32(m, eax + 0xcu));
    x0 = mulss(x0, x5.l(0));
    x5 = movss_m(rd32(m, eax + 0x1cu));
    x3 = mulss(x3, x5.l(0));
    x5 = movss_m(rd32(m, eax));
    x6 = addss(x6, x7.l(0));
    wr32(m, esp + 0x38u, x6.l(0));
    x0 = addss(x0, x3.l(0));
    x3 = movss_m(rd32(m, eax + 0x2cu));
    x2 = mulss(x2, x3.l(0));
    x3 = movss_m(rd32(m, ecx + 0x34u));
    x0 = addss(x0, x2.l(0));
    x2 = movss_m(rd32(m, eax + 0x3cu));
    x1 = mulss(x1, x2.l(0));
    x2 = movss_m(rd32(m, ecx + 0x38u));
    x0 = addss(x0, x1.l(0));
    x1 = movss_m(rd32(m, ecx + 0x3cu));
    wr32(m, esp + 0x3cu, x0.l(0));
    x0 = movss_m(rd32(m, ecx + 0x30u));
    x6 = x0;
    x6 = mulss(x6, x5.l(0));
    x5 = movss_m(rd32(m, eax + 0x10u));
    x7 = x3;
    x7 = mulss(x7, x5.l(0));
    x5 = movss_m(rd32(m, eax + 0x20u));
    x6 = addss(x6, x7.l(0));
    x7 = x2;
    x7 = mulss(x7, x5.l(0));
    x6 = addss(x6, x7.l(0));
    x5 = x1;
    x5 = mulss(x5, x4.l(0));
    x4 = movss_m(rd32(m, eax + 0x4u));
    x6 = addss(x6, x5.l(0));
    wr32(m, esp + 0x40u, x6.l(0));
    x5 = x0;
    x5 = mulss(x5, x4.l(0));
    x4 = movss_m(rd32(m, eax + 0x14u));
    x6 = x3;
    x6 = mulss(x6, x4.l(0));
    x4 = movss_m(rd32(m, eax + 0x24u));
    x5 = addss(x5, x6.l(0));
    x6 = x2;
    x6 = mulss(x6, x4.l(0));
    x4 = movss_m(rd32(m, eax + 0x34u));
    x5 = addss(x5, x6.l(0));
    x6 = x1;
    x6 = mulss(x6, x4.l(0));
    x4 = movss_m(rd32(m, eax + 0x8u));
    x5 = addss(x5, x6.l(0));
    wr32(m, esp + 0x44u, x5.l(0));
    x5 = x0;
    x5 = mulss(x5, x4.l(0));
    x4 = movss_m(rd32(m, eax + 0x18u));
    x6 = x3;
    x6 = mulss(x6, x4.l(0));
    x4 = movss_m(rd32(m, eax + 0x28u));
    x5 = addss(x5, x6.l(0));
    x6 = x2;
    x6 = mulss(x6, x4.l(0));
    x4 = movss_m(rd32(m, eax + 0x38u));
    x5 = addss(x5, x6.l(0));
    x6 = x1;
    x6 = mulss(x6, x4.l(0));
    x4 = movss_m(rd32(m, eax + 0xcu));
    x5 = addss(x5, x6.l(0));
    x0 = mulss(x0, x4.l(0));
    x4 = movss_m(rd32(m, eax + 0x1cu));
    wr32(m, esp + 0x48u, x5.l(0));
    edi = rd32(m, ebp + 0x8u);
    x3 = mulss(x3, x4.l(0));
    x0 = addss(x0, x3.l(0));
    x3 = movss_m(rd32(m, eax + 0x2cu));
    x2 = mulss(x2, x3.l(0));
    x0 = addss(x0, x2.l(0));
    x2 = movss_m(rd32(m, eax + 0x3cu));
    x1 = mulss(x1, x2.l(0));
    x0 = addss(x0, x1.l(0));
    ecx = 0x10u;
    esi = (esp + 0x10u);
    wr32(m, esp + 0x4cu, x0.l(0));
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00401c80 Fn_00401c80
//   original FUN_00401c80; register inputs: eax edx; returns with ret 0
REFLEX_PORT_FN(0x00401c80, Fn_00401c80, "fx", reflex_port::OUT_EAX) {
    LIFT_ENTRY();
    uint32_t fA_401c8e = 0;
    uint32_t fB_401c8e = 0;
    ecx = rd32(m, eax + 0x44u);
    wr32(m, eax + 0x4cu, 0x1u);
    wr32(m, edx, ecx);
    ecx = 0u;
    fA_401c8e = rd32(m, eax + 0x44u);
    fB_401c8e = ecx;
    ecx = (ecx & 0xffffff00u) | (((fA_401c8e != fB_401c8e) ? 1u : 0u) & 0xffu);
    eax = ecx;
    LIFT_RET(0);
}

// 00401ca0 Fn_00401ca0
//   original FUN_00401ca0; register inputs: eax; returns with ret 0
REFLEX_PORT_FN(0x00401ca0, Fn_00401ca0, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    wr32(m, eax + 0x4cu, 0x0u);
    LIFT_RET(0);
}

// 00401ce0 Entity__vf15
//   original Entity::vf15; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x00401ce0, Entity__vf15, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esp + 0xcu);
    esi = (ecx + 0x30u);
    ecx = 0x10u;
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00401d00 Entity__vf16
//   original Entity::vf16; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x00401d00, Entity__vf16, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, edi);
    edi = (ecx + 0x30u);
    ecx = 0x10u;
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00401d40 IRenderable__vf7
//   original IRenderable::vf7; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00401d40, IRenderable__vf7, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    fpush(m, reflex_f80::from_f32(rd32(m, 0x977564u) /* 15000.0 */));
    LIFT_RET(0);
}

// 00401e40 ModelEntity__vf23
//   original ModelEntity::vf23; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x00401e40, ModelEntity__vf23, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = (ecx + 0xa0u);
    LIFT_RET(0);
}

// 00401ea0 ModelEntity__vf9
//   original ModelEntity::vf9; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x00401ea0, ModelEntity__vf9, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fR_401eb0 = 0;
    uint32_t fR_401ec2 = 0;
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    ecx = rd32(m, 0xa95824u);
    wr32(m, esi + 0x70u, 0x0u);
    fR_401eb0 = (ecx & ecx);
    if ((fR_401eb0 == 0)) goto L_401ebc;
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x4u);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(edx, 0x401ebcu);
L_401ebc:
    ecx = rd32(m, 0xa95828u);
    fR_401ec2 = (ecx & ecx);
    if ((fR_401ec2 == 0)) goto L_401ece;
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x4u);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(edx, 0x401eceu);
L_401ece:
    eax = rd32(m, esi + 0x18u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x78fc50u, 0x401ed7u);  // ReflexAlignedFree
    esp = esp + 0x4u;
    esi = esi + 0xa0u;
    LIFT_CALL(0x7f1aa0u, 0x401ee5u);  // 
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00401ef0 rtti_shared_vf_00401ef0_00401ef0
//   original rtti_shared_vf_00401ef0; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x00401ef0, rtti_shared_vf_00401ef0_00401ef0, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fR_401ef0 = 0;
    fR_401ef0 = (ecx & ecx);
    if ((fR_401ef0 == 0)) goto L_401efb;
    eax = (ecx + 0x90u);
    LIFT_RET(0);
L_401efb:
    eax = 0u;
    LIFT_RET(0);
}

// 00401f00 ModelEntity__vf1_o90
//   original ModelEntity::vf1_o90; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x00401f00, ModelEntity__vf1_o90, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = rd16(m, ecx + 0x20u);
    eax = (eax >> 3);
    eax = (eax & 0x3u);
    LIFT_RET(0);
}

// 00401f50 ModelEntity__vf6_o90
//   original ModelEntity::vf6_o90; register inputs: ecx; returns with ret 12
REFLEX_PORT_FN(0x00401f50, ModelEntity__vf6_o90, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = rd32(m, esp + 0x8u);
    edx = rd32(m, esp + 0x4u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, edx);
    edx = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, 0x0u);
    ecx = ecx + 0x10u;
    LIFT_CALL(0x7f7f40u, 0x401f68u);  // Model_Bounds
    LIFT_RET(12);
}

// 00402160 Table_ColumnIndex
//   original Table_ColumnIndex; register inputs: eax; returns with ret 4
REFLEX_PORT_FN(0x00402160, Table_ColumnIndex, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fR_402188 = 0;
    uint32_t fA_40218c = 0;
    uint32_t fB_40218c = 0;
    uint32_t fA_40219a = 0;
    uint32_t fB_40219a = 0;
    uint32_t fR_40219e = 0;
    uint32_t fA_4021a8 = 0;
    uint32_t fB_4021a8 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0xcu;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    esp -= 4; wr32(m, esp, edi);
    eax = (ebp + 0x8u);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0x14u);
    esi = esi + 0x1cu;
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x416e40u, 0x40217fu);  // FUN_00416e40
    edi = rd32(m, esp + 0x10u);
    ebx = rd32(m, esi + 0x18u);
    esi = rd32(m, esi);
    fR_402188 = (edi & edi);
    if ((fR_402188 == 0)) goto L_402190;
    fA_40218c = edi;
    fB_40218c = esi;
    if (fA_40218c == fB_40218c) goto L_402196;
L_402190:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x402196u);
L_402196:
    esi = rd32(m, esp + 0x14u);
    fA_40219a = esi;
    fB_40219a = ebx;
    if (fA_40219a == fB_40219a) goto L_4021c3;
    fR_40219e = (edi & edi);
    if (!(fR_40219e == 0)) goto L_4021bf;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4021a8u);
L_4021a8:
    fA_4021a8 = esi;
    fB_4021a8 = rd32(m, edi + 0x18u);
    if (fA_4021a8 != fB_4021a8) goto L_4021b3;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4021b3u);
L_4021b3:
    eax = rd32(m, esi + 0x10u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_4021bf:
    edi = rd32(m, edi);
    goto L_4021a8;
L_4021c3:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    eax = (eax | 0xffffffffu);
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 004021d0 Fn_004021d0
//   original FUN_004021d0; register inputs: eax; returns with ret 0
REFLEX_PORT_FN(0x004021d0, Fn_004021d0, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX) {
    LIFT_ENTRY();
    uint32_t fR_4021de = 0;
    uint32_t fA_4021eb = 0;
    uint32_t fB_4021eb = 0;
    uint32_t fR_4021f5 = 0;
    uint32_t fA_4021fb = 0;
    uint32_t fB_4021fb = 0;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    eax = rd32(m, esi + 0x14u);
    eax = eax - rd32(m, esi + 0x10u);
    esp -= 4; wr32(m, esp, edi);
    eax = uint32_t(int32_t(eax) >> 2);
    fR_4021de = (eax & eax);
    if ((false || (fR_4021de == 0))) goto L_402210;
    edi = rd32(m, esi + 0x10u);
    ebx = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    fA_4021eb = edi;
    fB_4021eb = rd32(m, esi + 0x14u);
    if (fA_4021eb <= fB_4021eb) goto L_4021f2;
    LIFT_CALL(ebx, 0x4021f2u);
L_4021f2:
    esi = rd32(m, esi + 0x4u);
    fR_4021f5 = (esi & esi);
    if (!(fR_4021f5 == 0)) goto L_40220c;
    LIFT_CALL(ebx, 0x4021fbu);
L_4021fb:
    fA_4021fb = edi;
    fB_4021fb = rd32(m, esi + 0x10u);
    if (fA_4021fb < fB_4021fb) goto L_402202;
    LIFT_CALL(ebx, 0x402202u);
L_402202:
    eax = rd32(m, edi);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_TAILJMP(0x4eb830u);  // FUN_004eb830
L_40220c:
    esi = rd32(m, esi);
    goto L_4021fb;
L_402210:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00402220 Database_FindTable
//   original Database_FindTable; register inputs: eax; returns with ret 4
REFLEX_PORT_FN(0x00402220, Database_FindTable, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX | (reflex_port::OUT_XMM0 << 0) | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fR_402248 = 0;
    uint32_t fA_40224c = 0;
    uint32_t fB_40224c = 0;
    uint32_t fA_40225a = 0;
    uint32_t fB_40225a = 0;
    uint32_t fR_40225e = 0;
    uint32_t fA_402268 = 0;
    uint32_t fB_402268 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0xcu;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    esp -= 4; wr32(m, esp, edi);
    eax = (ebp + 0x8u);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0x14u);
    esi = esi + 0x18u;
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x416e40u, 0x40223fu);  // FUN_00416e40
    edi = rd32(m, esp + 0x10u);
    ebx = rd32(m, esi + 0x18u);
    esi = rd32(m, esi);
    fR_402248 = (edi & edi);
    if ((fR_402248 == 0)) goto L_402250;
    fA_40224c = edi;
    fB_40224c = esi;
    if (fA_40224c == fB_40224c) goto L_402256;
L_402250:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x402256u);
L_402256:
    esi = rd32(m, esp + 0x14u);
    fA_40225a = esi;
    fB_40225a = ebx;
    if (fA_40225a == fB_40225a) goto L_402283;
    fR_40225e = (edi & edi);
    if (!(fR_40225e == 0)) goto L_40227f;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x402268u);
L_402268:
    fA_402268 = esi;
    fB_402268 = rd32(m, edi + 0x18u);
    if (fA_402268 != fB_402268) goto L_402273;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x402273u);
L_402273:
    eax = rd32(m, esi + 0x10u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_40227f:
    edi = rd32(m, edi);
    goto L_402268;
L_402283:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 004022d0 _vector_constructor_iterator_
//   original `vector_constructor_iterator'; register inputs: none; returns with ret 16
REFLEX_PORT_FN(0x004022d0, _vector_constructor_iterator_, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_4022d5 = 0;
    uint32_t fB_4022d5 = 0;
    uint32_t fA_4022f6 = 0;
    uint32_t fB_4022f6 = 0;
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esp + 0x10u);
    fA_4022d5 = edi;
    fB_4022d5 = 0x1u;
    edi = fA_4022d5 - fB_4022d5;
    if (sgn(((fA_4022d5 - fB_4022d5)), 4)) goto L_4022fe;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x18u);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, esp + 0x14u);
L_4022f0:
    ecx = esi;
    LIFT_CALL(ebx, 0x4022f4u);
    esi = esi + ebp;
    fA_4022f6 = edi;
    fB_4022f6 = 0x1u;
    edi = fA_4022f6 - fB_4022f6;
    if (!sgn(((fA_4022f6 - fB_4022f6)), 4)) goto L_4022f0;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
L_4022fe:
    edi = rd32(m, esp); esp += 4;
    LIFT_RET(16);
}

// 004023c0 Fn_004023c0
//   original FUN_004023c0; register inputs: eax; returns with ret 0
REFLEX_PORT_FN(0x004023c0, Fn_004023c0, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = rd32(m, eax + 0x10u);
    LIFT_RET(0);
}

// 00402420 Fn_00402420
//   original FUN_00402420; register inputs: ecx; returns with ret 8
REFLEX_PORT_FN(0x00402420, Fn_00402420, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX | (reflex_port::OUT_XMM0 << 0) | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fR_402425 = 0;
    uint32_t fA_402444 = 0;
    uint32_t fB_402444 = 0;
    uint32_t fA_40246c = 0;
    uint32_t fB_40246c = 0;
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esp + 0xcu);
    fR_402425 = (edi & edi);
    if (!(fR_402425 == 0)) goto L_402437;
    eax = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x8841d0u, 0x402433u);  // FUN_008841d0
    edi = rd32(m, esp); esp += 4;
    LIFT_RET(8);
L_402437:
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, ecx + 0x30u);
    ecx = rd32(m, esi + 0x10u);
    ecx = ecx - rd32(m, esi + 0xcu);
    ecx = uint32_t(int32_t(ecx) >> 2);
    fA_402444 = edi;
    fB_402444 = ecx;
    if (fA_402444 < fB_402444) goto L_402463;
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0xe0000003u);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0x0u);
    LIFT_CALL(0x884390u, 0x402458u);  // FUN_00884390
    esp = esp + 0x10u;
    esi = rd32(m, esp); esp += 4;
    eax = (eax | 0xffffffffu);
    edi = rd32(m, esp); esp += 4;
    LIFT_RET(8);
L_402463:
    edx = rd32(m, esi + 0x10u);
    edx = edx - rd32(m, esi + 0xcu);
    edx = uint32_t(int32_t(edx) >> 2);
    fA_40246c = edi;
    fB_40246c = edx;
    if (fA_40246c < fB_40246c) goto L_402476;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x402476u);
L_402476:
    eax = rd32(m, esi + 0xcu);
    ecx = rd32(m, esp + 0xcu);
    eax = (eax + edi * 4);
    esp -= 4; wr32(m, esp, ecx);
    ecx = rd32(m, eax);
    LIFT_CALL(0x8841d0u, 0x402488u);  // FUN_008841d0
    esi = rd32(m, esp); esp += 4;
    edi = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 00402500 Fn_00402500
//   original FUN_00402500; register inputs: none; returns with ret 16
REFLEX_PORT_FN(0x00402500, Fn_00402500, "fx", reflex_port::OUT_EAX) {
    LIFT_ENTRY();
    uint32_t fA_402516 = 0;
    uint32_t fB_402516 = 0;
    uint32_t fA_402526 = 0;
    uint32_t fB_402526 = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esp + 0x18u);
    eax = ebx;
    eax = uint32_t(int32_t(eax) * int32_t(edi));
    esi = esi + eax;
    fA_402516 = edi;
    fB_402516 = 0x1u;
    edi = fA_402516 - fB_402516;
    if (sgn(((fA_402516 - fB_402516)), 4)) goto L_40252c;
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x20u);
L_402520:
    esi = esi - ebx;
    ecx = esi;
    LIFT_CALL(ebp, 0x402526u);
    fA_402526 = edi;
    fB_402526 = 0x1u;
    edi = fA_402526 - fB_402526;
    if (!sgn(((fA_402526 - fB_402526)), 4)) goto L_402520;
    ebp = rd32(m, esp); esp += 4;
L_40252c:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(16);
}

// 004027e0 Fn_004027e0
//   original FUN_004027e0; register inputs: eax esi; returns with ret 4
REFLEX_PORT_FN(0x004027e0, Fn_004027e0, "fx", reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fR_4027e8 = 0;
    uint32_t fA_4027f0 = 0;
    uint32_t fB_4027f0 = 0;
    uint32_t fA_402842 = 0;
    uint32_t fB_402842 = 0;
    uint32_t fA_402854 = 0;
    uint32_t fB_402854 = 0;
    uint32_t fA_40286d = 0;
    uint32_t fB_40286d = 0;
    uint32_t fA_402887 = 0;
    uint32_t fB_402887 = 0;
    uint32_t fA_4028a1 = 0;
    uint32_t fB_4028a1 = 0;
    uint32_t fA_4028be = 0;
    uint32_t fB_4028be = 0;
    uint32_t fA_4028c2 = 0;
    uint32_t fB_4028c2 = 0;
    uint32_t fA_4028d0 = 0;
    uint32_t fB_4028d0 = 0;
    uint32_t fA_4028ea = 0;
    uint32_t fB_4028ea = 0;
    uint32_t fA_402927 = 0;
    uint32_t fB_402927 = 0;
    uint32_t fA_402940 = 0;
    uint32_t fB_402940 = 0;
    uint32_t fA_40295a = 0;
    uint32_t fB_40295a = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, edi);
    edi = eax;
    fR_4027e8 = (ebx & ebx);
    if ((fR_4027e8 == 0)) goto L_40296a;
    fA_4027f0 = rd32(m, esi + 0x11cu);
    fB_4027f0 = 0x0u;
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esi + 0x120u);
    wr32(m, esi + 0x104u, edi);
    if (fA_4027f0 == fB_4027f0) goto L_4028fb;
    edi = (edi + 1u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, 0xffffffffu);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0xfde9u);
    LIFT_CALL(rd32(m, 0x9160a4u) /* 8.295510345196412e-39 */, 0x40281du);
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    eax = 0u;
    wr16(m, ebp + edi * 2 - 0x2u, (eax & 0xffffu));
    ecx = rd32(m, esi + 0x104u);
    edx = rd32(m, esi + 0x120u);
    wr16(m, edx + ecx * 2, (eax & 0xffffu));
    ecx = rd32(m, esi + 0x104u);
    fA_402842 = ecx;
    fB_402842 = 0x4u;
    wr32(m, esi + 0x114u, x0.l(0));
    edx = (edx & 0xffffff00u) | ((0xau) & 0xffu);
    if (int32_t(sext(fA_402842, 4)) < int32_t(sext(fB_402842, 4))) goto L_4028c2;
    edi = (ecx - 0x3u);
L_402854:
    fA_402854 = rd8(m, ebx + eax);
    fB_402854 = (edx & 0xffu);
    if (fA_402854 != fB_402854) goto L_40286d;
    x1 = movss_m(rd32(m, esi + 0x114u));
    x1 = addss(x1, x0.l(0));
    wr32(m, esi + 0x114u, x1.l(0));
L_40286d:
    fA_40286d = rd8(m, ebx + eax + 0x1u);
    fB_40286d = (edx & 0xffu);
    if (fA_40286d != fB_40286d) goto L_402887;
    x1 = movss_m(rd32(m, esi + 0x114u));
    x1 = addss(x1, x0.l(0));
    wr32(m, esi + 0x114u, x1.l(0));
L_402887:
    fA_402887 = rd8(m, ebx + eax + 0x2u);
    fB_402887 = (edx & 0xffu);
    if (fA_402887 != fB_402887) goto L_4028a1;
    x1 = movss_m(rd32(m, esi + 0x114u));
    x1 = addss(x1, x0.l(0));
    wr32(m, esi + 0x114u, x1.l(0));
L_4028a1:
    fA_4028a1 = rd8(m, ebx + eax + 0x3u);
    fB_4028a1 = (edx & 0xffu);
    if (fA_4028a1 != fB_4028a1) goto L_4028bb;
    x1 = movss_m(rd32(m, esi + 0x114u));
    x1 = addss(x1, x0.l(0));
    wr32(m, esi + 0x114u, x1.l(0));
L_4028bb:
    eax = eax + 0x4u;
    fA_4028be = eax;
    fB_4028be = edi;
    if (int32_t(sext(fA_4028be, 4)) < int32_t(sext(fB_4028be, 4))) goto L_402854;
L_4028c2:
    fA_4028c2 = eax;
    fB_4028c2 = ecx;
    if (int32_t(sext(fA_4028c2, 4)) >= int32_t(sext(fB_4028c2, 4))) goto L_402962;
L_4028d0:
    fA_4028d0 = rd8(m, eax + ebx);
    fB_4028d0 = (edx & 0xffu);
    if (fA_4028d0 != fB_4028d0) goto L_4028e9;
    x1 = movss_m(rd32(m, esi + 0x114u));
    x1 = addss(x1, x0.l(0));
    wr32(m, esi + 0x114u, x1.l(0));
L_4028e9:
    eax = (eax + 1u);
    fA_4028ea = eax;
    fB_4028ea = ecx;
    if (int32_t(sext(fA_4028ea, 4)) < int32_t(sext(fB_4028ea, 4))) goto L_4028d0;
    ebp = rd32(m, esp); esp += 4;
    edi = rd32(m, esp); esp += 4;
    wr8(m, esi + 0xfau, 0xc3u);
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_4028fb:
    ecx = edi;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    LIFT_CALL(rd32(m, 0x916260u) /* 8.286626112932593e-39 */, 0x402906u);
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    wr8(m, edi + ebp, 0x0u);
    eax = rd32(m, esi + 0x104u);
    edx = rd32(m, esi + 0x120u);
    wr8(m, edx + eax, 0x0u);
    eax = 0u;
    esp = esp + 0xcu;
    fA_402927 = rd32(m, esi + 0x104u);
    fB_402927 = eax;
    wr32(m, esi + 0x114u, x0.l(0));
    if (int32_t(sext(fA_402927, 4)) <= int32_t(sext(fB_402927, 4))) goto L_402962;
    ecx = rd32(m, esi + 0x120u);
    edx = (edx & 0xffffff00u) | ((0xau) & 0xffu);
L_402940:
    fA_402940 = rd8(m, ecx + eax);
    fB_402940 = (edx & 0xffu);
    if (fA_402940 != fB_402940) goto L_402959;
    x1 = movss_m(rd32(m, esi + 0x114u));
    x1 = addss(x1, x0.l(0));
    wr32(m, esi + 0x114u, x1.l(0));
L_402959:
    eax = (eax + 1u);
    fA_40295a = eax;
    fB_40295a = rd32(m, esi + 0x104u);
    if (int32_t(sext(fA_40295a, 4)) < int32_t(sext(fB_40295a, 4))) goto L_402940;
L_402962:
    wr8(m, esi + 0xfau, 0xc3u);
    ebp = rd32(m, esp); esp += 4;
L_40296a:
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00402970 Fn_00402970
//   original FUN_00402970; register inputs: edx ebx; returns with ret 4
REFLEX_PORT_FN(0x00402970, Fn_00402970, "fx", 0) {
    LIFT_ENTRY();
    uint32_t fR_402970 = 0;
    uint32_t fR_402986 = 0;
    fR_402970 = (edx & edx);
    if ((fR_402970 == 0)) goto L_40299b;
    eax = edx;
    esp -= 4; wr32(m, esp, esi);
    esi = (eax + 0x2u);
L_402980:
    ecx = (ecx & 0xffff0000u) | ((rd16(m, eax)) & 0xffffu);
    eax = eax + 0x2u;
    fR_402986 = ((ecx & 0xffffu) & (ecx & 0xffffu));
    if (!(fR_402986 == 0)) goto L_402980;
    eax = eax - esi;
    esi = rd32(m, esp + 0x8u);
    eax = uint32_t(int32_t(eax) >> 1);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x4029a0u, 0x40299au);  // FUN_004029a0
    esi = rd32(m, esp); esp += 4;
L_40299b:
    LIFT_RET(4);
}

// 004029a0 Fn_004029a0
//   original FUN_004029a0; register inputs: esi; returns with ret 8
REFLEX_PORT_FN(0x004029a0, Fn_004029a0, "fx", reflex_port::OUT_ECX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fR_4029aa = 0;
    uint32_t fA_4029b2 = 0;
    uint32_t fB_4029b2 = 0;
    uint32_t fA_402a00 = 0;
    uint32_t fB_402a00 = 0;
    uint32_t fA_402a25 = 0;
    uint32_t fB_402a25 = 0;
    uint32_t fA_402a3f = 0;
    uint32_t fB_402a3f = 0;
    uint32_t fA_402a59 = 0;
    uint32_t fB_402a59 = 0;
    uint32_t fA_402a72 = 0;
    uint32_t fB_402a72 = 0;
    uint32_t fA_402a8f = 0;
    uint32_t fB_402a8f = 0;
    uint32_t fA_402a94 = 0;
    uint32_t fB_402a94 = 0;
    uint32_t fA_402a98 = 0;
    uint32_t fB_402a98 = 0;
    uint32_t fA_402ab4 = 0;
    uint32_t fB_402ab4 = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0xcu);
    fR_4029aa = (ebp & ebp);
    if ((fR_4029aa == 0)) goto L_402ac0;
    fA_4029b2 = rd32(m, esi + 0x11cu);
    fB_4029b2 = 0x0u;
    if (fA_4029b2 == fB_4029b2) goto L_402ab9;
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esi + 0x120u);
    eax = ebx;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, edi);
    wr32(m, esi + 0x104u, ebx);
    LIFT_CALL(rd32(m, 0x916340u) /* 8.287469694608116e-39 */, 0x4029d7u);
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    ecx = 0u;
    wr16(m, edi + ebx * 2, (ecx & 0xffffu));
    edx = rd32(m, esi + 0x104u);
    eax = rd32(m, esi + 0x120u);
    wr16(m, eax + edx * 2, (ecx & 0xffffu));
    edi = rd32(m, esi + 0x104u);
    esp = esp + 0xcu;
    edx = 0u;
    fA_402a00 = edi;
    fB_402a00 = 0x4u;
    wr32(m, esi + 0x114u, x0.l(0));
    ebx = (ecx + 0xau);
    if (int32_t(sext(fA_402a00, 4)) < int32_t(sext(fB_402a00, 4))) goto L_402a94;
    ecx = (edi - 0x4u);
    ecx = (ecx >> 2);
    ecx = (ecx + 1u);
    eax = (ebp + 0x4u);
    edx = (ecx * 4);
L_402a25:
    fA_402a25 = rd16(m, eax - 0x4u);
    fB_402a25 = (ebx & 0xffffu);
    if (fA_402a25 != fB_402a25) goto L_402a3f;
    x1 = movss_m(rd32(m, esi + 0x114u));
    x1 = addss(x1, x0.l(0));
    wr32(m, esi + 0x114u, x1.l(0));
L_402a3f:
    fA_402a3f = rd16(m, eax - 0x2u);
    fB_402a3f = (ebx & 0xffffu);
    if (fA_402a3f != fB_402a3f) goto L_402a59;
    x1 = movss_m(rd32(m, esi + 0x114u));
    x1 = addss(x1, x0.l(0));
    wr32(m, esi + 0x114u, x1.l(0));
L_402a59:
    fA_402a59 = rd16(m, eax);
    fB_402a59 = (ebx & 0xffffu);
    if (fA_402a59 != fB_402a59) goto L_402a72;
    x1 = movss_m(rd32(m, esi + 0x114u));
    x1 = addss(x1, x0.l(0));
    wr32(m, esi + 0x114u, x1.l(0));
L_402a72:
    fA_402a72 = rd16(m, eax + 0x2u);
    fB_402a72 = (ebx & 0xffffu);
    if (fA_402a72 != fB_402a72) goto L_402a8c;
    x1 = movss_m(rd32(m, esi + 0x114u));
    x1 = addss(x1, x0.l(0));
    wr32(m, esi + 0x114u, x1.l(0));
L_402a8c:
    eax = eax + 0x8u;
    fA_402a8f = ecx;
    fB_402a8f = 0x1u;
    ecx = fA_402a8f - fB_402a8f;
    if (fA_402a8f != fB_402a8f) goto L_402a25;
L_402a94:
    fA_402a94 = edx;
    fB_402a94 = edi;
    if (int32_t(sext(fA_402a94, 4)) >= int32_t(sext(fB_402a94, 4))) goto L_402ab8;
L_402a98:
    fA_402a98 = rd16(m, ebp + edx * 2);
    fB_402a98 = (ebx & 0xffffu);
    if (fA_402a98 != fB_402a98) goto L_402ab3;
    x1 = movss_m(rd32(m, esi + 0x114u));
    x1 = addss(x1, x0.l(0));
    wr32(m, esi + 0x114u, x1.l(0));
L_402ab3:
    edx = (edx + 1u);
    fA_402ab4 = edx;
    fB_402ab4 = edi;
    if (int32_t(sext(fA_402ab4, 4)) < int32_t(sext(fB_402ab4, 4))) goto L_402a98;
L_402ab8:
    edi = rd32(m, esp); esp += 4;
L_402ab9:
    wr8(m, esi + 0xfau, 0xc3u);
L_402ac0:
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 00402ad0 Fn_00402ad0
//   original FUN_00402ad0; register inputs: ecx xmm0; returns with ret 0
REFLEX_PORT_FN(0x00402ad0, Fn_00402ad0, "fx", 0) {
    LIFT_ENTRY();
    FCmp fC_402ad8{};
    uint32_t fR_402adc = 0;
    x1 = movss_m(rd32(m, ecx + 0x118u));
    fC_402ad8 = fcmp32(x1.l(0), x0.l(0));
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_402ad8) | 2u) << 8);
    fR_402adc = (((eax >> 8) & 0xffu) & 0x44u);
    if (!parity8(fR_402adc)) goto L_402ae9;
    wr32(m, ecx + 0x118u, x0.l(0));
L_402ae9:
    LIFT_RET(0);
}

// 00402af0 Fn_00402af0
//   original FUN_00402af0; register inputs: eax; returns with ret 0
REFLEX_PORT_FN(0x00402af0, Fn_00402af0, "fx", reflex_port::OUT_EAX) {
    LIFT_ENTRY();

    eax = rd32(m, eax + 0x68u);
    LIFT_RET(0);
}

// 00402b10 Fn_00402b10
//   original FUN_00402b10; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x00402b10, Fn_00402b10, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fR_402b16 = 0;
    esp -= 4; wr32(m, esp, ecx);
    eax = 0x1u;
    fR_402b16 = (rd8(m, 0x9e30fcu) & (eax & 0xffu));
    if (!(fR_402b16 == 0)) goto L_402b36;
    wr32(m, 0x9e30fcu, (rd32(m, 0x9e30fcu) | eax));
    LIFT_CALL(0x402be0u, 0x402b29u);  // FUN_00402be0
    esp -= 4; wr32(m, esp, 0x911d40u);
    LIFT_CALL(0x8f9fbfu, 0x402b33u);  // _atexit
    esp = esp + 0x4u;
L_402b36:
    eax = 0x9e30e4u;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00402c20 Fn_00402c20
//   original FUN_00402c20; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00402c20, Fn_00402c20, "fx", reflex_port::OUT_EAX) {
    LIFT_ENTRY();

    eax = rd32(m, 0xa93fccu);
    LIFT_RET(0);
}

// 00402c90 Entity__vf17
//   original Entity::vf17; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x00402c90, Entity__vf17, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x60u)));
    eax = rd32(m, esp + 0x4u);
    wr32(m, eax, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x64u)));
    wr32(m, eax + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x68u)));
    wr32(m, eax + 0x8u, fst32(m));
    fpop(m);
    LIFT_RET(4);
}

// 00402cb0 BehaviorBlueYellowFlag__vf5
//   original BehaviorBlueYellowFlag::vf5; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00402cb0, BehaviorBlueYellowFlag__vf5, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0xau;
    LIFT_RET(0);
}

// 00402d90 Fn_00402d90
//   original FUN_00402d90; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00402d90, Fn_00402d90, "fx", reflex_port::OUT_EAX) {
    LIFT_ENTRY();
    uint32_t fR_402d95 = 0;
    eax = 0x1u;
    fR_402d95 = (rd8(m, 0x9e30e0u) & (eax & 0xffu));
    if (!(fR_402d95 == 0)) goto L_402dad;
    wr32(m, 0x9e30e0u, (rd32(m, 0x9e30e0u) | eax));
    wr32(m, 0x9e30dcu, 0x0u);
L_402dad:
    eax = 0x9e30d4u;
    LIFT_RET(0);
}

// 00402ef0 FreeRideStartEntity__vf22
//   original FreeRideStartEntity::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00402ef0, FreeRideStartEntity__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x1014u;
    LIFT_RET(0);
}

// 00403500 rtti_shared_vf_00403500_00403500
//   original rtti_shared_vf_00403500; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00403500, rtti_shared_vf_00403500_00403500, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x3u;
    LIFT_RET(0);
}

// 004035a0 GeneralStorageEntity__vf22
//   original GeneralStorageEntity::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x004035a0, GeneralStorageEntity__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x1eu;
    LIFT_RET(0);
}

// 00403670 LocalizedPackageReferenceEntity__vf22
//   original LocalizedPackageReferenceEntity::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00403670, LocalizedPackageReferenceEntity__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x1094u;
    LIFT_RET(0);
}

// 00403680 BehaviorRoamWorld__vf5
//   original BehaviorRoamWorld::vf5; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00403680, BehaviorRoamWorld__vf5, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0xeu;
    LIFT_RET(0);
}

// 00403c00 MXTrigger__vf22
//   original MXTrigger::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00403c00, MXTrigger__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x1028u;
    LIFT_RET(0);
}

// 00403c10 Fn_00403c10
//   original FUN_00403c10; register inputs: ecx eax; returns with ret 0
REFLEX_PORT_FN(0x00403c10, Fn_00403c10, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x1cu;
    esp -= 4; wr32(m, esp, esi);
    x4 = movss_m(rd32(m, ecx + 0x4u));
    x3 = movss_m(rd32(m, ecx + 0x8u));
    x5 = movss_m(rd32(m, ecx));
    x0 = movss_m(rd32(m, eax + 0x250u));
    x1 = movss_m(rd32(m, eax + 0x260u));
    x2 = movss_m(rd32(m, eax + 0x254u));
    x1 = mulss(x1, x3.l(0));
    x0 = mulss(x0, x4.l(0));
    x0 = addss(x0, x1.l(0));
    x1 = movss_m(rd32(m, eax + 0x240u));
    x1 = mulss(x1, x5.l(0));
    x2 = mulss(x2, x4.l(0));
    x0 = addss(x0, x1.l(0));
    x1 = movss_m(rd32(m, eax + 0x244u));
    x0 = addss(x0, rd32(m, eax + 0x270u));
    x1 = mulss(x1, x5.l(0));
    x1 = addss(x1, x2.l(0));
    x2 = movss_m(rd32(m, eax + 0x264u));
    x2 = mulss(x2, x3.l(0));
    x1 = addss(x1, x2.l(0));
    x2 = movss_m(rd32(m, eax + 0x248u));
    x1 = addss(x1, rd32(m, eax + 0x274u));
    x2 = mulss(x2, x5.l(0));
    x5 = movss_m(rd32(m, eax + 0x258u));
    x5 = mulss(x5, x4.l(0));
    x4 = movss_m(rd32(m, eax + 0x268u));
    x2 = addss(x2, x5.l(0));
    x4 = mulss(x4, x3.l(0));
    x2 = addss(x2, x4.l(0));
    x2 = addss(x2, rd32(m, eax + 0x278u));
    esi = (esp + 0x10u);
    wr32(m, esp + 0x10u, x0.l(0));
    wr32(m, esp + 0x14u, x1.l(0));
    wr32(m, esp + 0x18u, x2.l(0));
    LIFT_CALL(0x403cf0u, 0x403cdfu);  // FUN_00403cf0
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00403cf0 Fn_00403cf0
//   original FUN_00403cf0; register inputs: ecx eax esi; returns with ret 0
REFLEX_PORT_FN(0x00403cf0, Fn_00403cf0, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_403cf7 = 0;
    uint32_t fB_403cf7 = 0;
    uint32_t fA_403cfc = 0;
    uint32_t fB_403cfc = 0;
    uint32_t fA_403d01 = 0;
    uint32_t fB_403d01 = 0;
    FCmp fC_403d2d{};
    FCmp fC_403d70{};
    uint32_t FL = 0;
    FCmp fC_403d86{};
    FCmp fC_403d9d{};
    FCmp fC_403db4{};
    esp -= 4; wr32(m, esp, ecx);
    eax = rd32(m, eax + 0xb8u);
    fA_403cf7 = eax;
    fB_403cf7 = 0x0u;
    eax = fA_403cf7 - fB_403cf7;
    if (fA_403cf7 == fB_403cf7) goto L_403d75;
    fA_403cfc = eax;
    fB_403cfc = 0x1u;
    eax = fA_403cfc - fB_403cfc;
    if (fA_403cfc == fB_403cfc) goto L_403d3d;
    fA_403d01 = eax;
    fB_403d01 = 0x1u;
    eax = fA_403d01 - fB_403d01;
    if (fA_403d01 == fB_403d01) goto L_403d0a;
    eax = (eax & 0xffffff00u) | ((0u) & 0xffu);
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_403d0a:
    x1 = movss_m(rd32(m, esi));
    x0 = movss_m(rd32(m, esi + 0x8u));
    x2 = x1;
    x2 = mulss(x2, x1.l(0));
    x1 = x0;
    x1 = mulss(x1, x0.l(0));
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x2 = addss(x2, x1.l(0));
    fC_403d2d = fcmp32(x0.l(0), x2.l(0));
    if ((fc_b(fC_403d2d) || fc_z(fC_403d2d))) goto L_403dc0;
    x0 = movss_m(rd32(m, esi + 0x4u));
    goto L_403da7;
L_403d3d:
    x1 = movss_m(rd32(m, esi + 0x4u));
    x0 = movss_m(rd32(m, esi + 0x8u));
    x2 = movss_m(rd32(m, esi));
    x3 = x1;
    x3 = mulss(x3, x1.l(0));
    x1 = x0;
    x1 = mulss(x1, x0.l(0));
    x0 = x2;
    x0 = mulss(x0, x2.l(0));
    x3 = addss(x3, x1.l(0));
    x3 = addss(x3, x0.l(0));
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    fC_403d70 = fcmp32(x0.l(0), x3.l(0));
    FL = fc_flags(fC_403d70);
    goto L_403db7;
L_403d75:
    x0 = movss_m(rd32(m, esi));
    LIFT_CALL(0x401000u, 0x403d7eu);  // FUN_00401000
    x1 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    fC_403d86 = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_403d86) || fc_z(fC_403d86))) goto L_403dc0;
    x0 = movss_m(rd32(m, esi + 0x4u));
    LIFT_CALL(0x401000u, 0x403d95u);  // FUN_00401000
    x1 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    fC_403d9d = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_403d9d) || fc_z(fC_403d9d))) goto L_403dc0;
    x0 = movss_m(rd32(m, esi + 0x8u));
L_403da7:
    LIFT_CALL(0x401000u, 0x403dacu);  // FUN_00401000
    x1 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    fC_403db4 = fcmp32(x1.l(0), x0.l(0));
    FL = fc_flags(fC_403db4);
L_403db7:
    if (fl_cc(6, FL)) goto L_403dc0;
    eax = 0x1u;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_403dc0:
    eax = 0u;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00403dd0 MudManager__vf22
//   original MudManager::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00403dd0, MudManager__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x104du;
    LIFT_RET(0);
}

// 00403e40 MXHitDiscoveryCollectable__vf22
//   original MXHitDiscoveryCollectable::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00403e40, MXHitDiscoveryCollectable__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x106du;
    LIFT_RET(0);
}

// 00403e80 MXChallengesSpecEntity__vf22
//   original MXChallengesSpecEntity::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00403e80, MXChallengesSpecEntity__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x1019u;
    LIFT_RET(0);
}

// 00403ee0 AnchorEntity__vf22
//   original AnchorEntity::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00403ee0, AnchorEntity__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x101au;
    LIFT_RET(0);
}

// 00403fe0 UIGateSelectionEntity__vf22
//   original UIGateSelectionEntity::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00403fe0, UIGateSelectionEntity__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x107fu;
    LIFT_RET(0);
}

// 00404020 UIGateSelectionCameraEntity__vf19
//   original UIGateSelectionCameraEntity::vf19; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x00404020, UIGateSelectionCameraEntity__vf19, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_40402a = 0;
    uint32_t fB_40402a = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    fA_40402a = rd32(m, esi + 0x4a0u);
    fB_40402a = 0x0u;
    if (fA_40402a == fB_40402a) goto L_40404f;
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x8u)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x656070u, 0x40403fu);  // MXUIPartsViewCamera::vf19
    eax = rd32(m, esi + 0x10u);
    ecx = rd32(m, eax + 0x19b8u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x63dba0u, 0x40404fu);  // FUN_0063dba0
L_40404f:
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00404060 UIGateSelectionCameraEntity__vf22
//   original UIGateSelectionCameraEntity::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00404060, UIGateSelectionCameraEntity__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x1080u;
    LIFT_RET(0);
}

// 00404340 OverlayColorEntity__vf22
//   original OverlayColorEntity::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00404340, OverlayColorEntity__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x1063u;
    LIFT_RET(0);
}

// 004043b0 HDRSettingsEntity__vf22
//   original HDRSettingsEntity::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x004043b0, HDRSettingsEntity__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x1064u;
    LIFT_RET(0);
}

// 00404470 ShadowMapEntity__vf22
//   original ShadowMapEntity::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00404470, ShadowMapEntity__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x1065u;
    LIFT_RET(0);
}

// 00404480 VolumeFogEntity__vf22
//   original VolumeFogEntity::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00404480, VolumeFogEntity__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x105fu;
    LIFT_RET(0);
}

// 004044f0 TDFEntity__vf22
//   original TDFEntity::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x004044f0, TDFEntity__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x1076u;
    LIFT_RET(0);
}

// 00404560 VideoEntity__vf22
//   original VideoEntity::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00404560, VideoEntity__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x1077u;
    LIFT_RET(0);
}

// 00404580 InstancedAnimationEntity__vf22
//   original InstancedAnimationEntity::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00404580, InstancedAnimationEntity__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x1051u;
    LIFT_RET(0);
}

// 00404670 MusicTrackListEntity__vf22
//   original MusicTrackListEntity::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00404670, MusicTrackListEntity__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x107cu;
    LIFT_RET(0);
}

// 00408c50 MudRecord_Update
//   original MudRecord_Update; register inputs: esi; returns with ret 0
REFLEX_PORT_FN(0x00408c50, MudRecord_Update, "fx", reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fA_408c76 = 0;
    uint32_t fB_408c76 = 0;
    uint32_t fA_408c8c = 0;
    uint32_t fB_408c8c = 0;
    uint32_t fR_408ca1 = 0;
    uint32_t fA_408cb8 = 0;
    uint32_t fB_408cb8 = 0;
    uint32_t fA_408cd4 = 0;
    uint32_t fB_408cd4 = 0;
    uint32_t fA_408ce4 = 0;
    uint32_t fB_408ce4 = 0;
    FCmp fC_408d1a{};
    FCmp fC_408d24{};
    FCmp fC_408d63{};
    uint32_t fR_408d85 = 0;
    FCmp fC_408da7{};
    FCmp fC_408db0{};
    FCmp fC_408dcb{};
    FCmp fC_408dd4{};
    FCmp fC_408e1c{};
    uint32_t fR_408e20 = 0;
    FCmp fC_408e3a{};
    FCmp fC_408e44{};
    FCmp fC_408e96{};
    FCmp fC_408ea0{};
    FCmp fC_408ec7{};
    uint32_t fR_408ecb = 0;
    uint32_t fA_408ed7 = 0;
    uint32_t fB_408ed7 = 0;
    uint32_t fA_408ee7 = 0;
    uint32_t fB_408ee7 = 0;
    FCmp fC_408f2f{};
    FCmp fC_408f34{};
    uint32_t fA_408f60 = 0;
    uint32_t fB_408f60 = 0;
    uint32_t fA_408f70 = 0;
    uint32_t fB_408f70 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x18u;
    eax = rd32(m, 0xa95844u);
    ecx = rd32(m, eax + 0x20u);
    x0 = movss_m(rd32(m, 0xa89bc4u));
    esp -= 4; wr32(m, esp, ebx);
    ebx = 0x1u;
    esp -= 4; wr32(m, esp, edi);
    wr32(m, esp + 0xcu, x0.l(0));
    fA_408c76 = rd32(m, ecx + 0x34u);
    fB_408c76 = ebx;
    if (fA_408c76 != fB_408c76) goto L_408c89;
    edx = rd32(m, 0xa946b8u);
    eax = rd32(m, edx + 0x5a54u);
    goto L_408ca1;
L_408c89:
    eax = rd32(m, eax + 0x1cu);
    fA_408c8c = rd32(m, eax + 0x34u);
    fB_408c8c = ebx;
    if (fA_408c8c != fB_408c8c) goto L_408fa5;
    ecx = rd32(m, 0xa946b8u);
    eax = rd32(m, ecx + 0x20c8u);
L_408ca1:
    fR_408ca1 = (eax & eax);
    if ((fR_408ca1 == 0)) goto L_408fa5;
    ecx = rd32(m, esi + 0x30u);
    edx = rd32(m, ecx);
    edx = rd32(m, edx + 0x44u);
    eax = (esp + 0x10u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(edx, 0x408cb8u);
    fA_408cb8 = rd32(m, 0xa97164u);
    fB_408cb8 = 0x0u;
    if (fA_408cb8 == fB_408cb8) goto L_408d58;
    ecx = rd32(m, 0xa95844u);
    eax = rd32(m, ecx + 0x20u);
    edx = rd32(m, 0xa946b8u);
    fA_408cd4 = rd32(m, eax + 0x34u);
    fB_408cd4 = ebx;
    if (fA_408cd4 != fB_408cd4) goto L_408ce1;
    eax = rd32(m, edx + 0x5a54u);
    goto L_408cf3;
L_408ce1:
    eax = rd32(m, ecx + 0x1cu);
    fA_408ce4 = rd32(m, eax + 0x34u);
    fB_408ce4 = ebx;
    if (fA_408ce4 != fB_408ce4) goto L_408cf1;
    eax = rd32(m, edx + 0x20c8u);
    goto L_408cf3;
L_408cf1:
    eax = 0u;
L_408cf3:
    eax = rd32(m, eax + 0x2214cu);
    x0 = movss_m(rd32(m, eax + 0xa0u));
    x0 = mulss(x0, rd32(m, esp + 0xcu));
    x1 = movss_m(rd32(m, esi));
    x5 = X{};
    x1 = subss(x1, x0.l(0));
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    fC_408d1a = fcmp32(x1.l(0), x0.l(0));
    if (fc_b(fC_408d1a)) goto L_408d24;
    x1 = x0;
    goto L_408d2c;
L_408d24:
    fC_408d24 = fcmp32(x5.l(0), x1.l(0));
    if (fc_b(fC_408d24)) goto L_408d2c;
    x1 = x5;
L_408d2c:
    wr32(m, esi, x1.l(0));
    wr32(m, esi + 0x4u, x1.l(0));
    wr32(m, esi + 0x8u, x1.l(0));
    wr32(m, esi + 0xcu, x1.l(0));
    wr32(m, esi + 0x10u, x0.l(0));
    wr32(m, esi + 0x14u, x0.l(0));
    wr32(m, esi + 0x18u, x0.l(0));
    wr32(m, esi + 0x1cu, x0.l(0));
    goto L_408ec2;
L_408d58:
    ecx = rd32(m, esi + 0x30u);
    x0 = movss_m(rd32(m, ecx + 0x150u));
    fC_408d63 = fcmp32(x0.l(0), rd32(m, 0x9c6074u));
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x5 = X{};
    if (fc_b(fC_408d63)) goto L_408e6c;
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0xacu);
    LIFT_CALL(eax, 0x408d85u);
    fR_408d85 = (eax & eax);
    if (!(fR_408d85 == 0)) goto L_408e61;
    LIFT_CALL(0x402d90u, 0x408d92u);  // FUN_00402d90
    edx = eax;
    LIFT_CALL(0x402dc0u, 0x408d99u);  // FUN_00402dc0
    x1 = movss_m(rd32(m, eax + 0x44d8u));
    x0 = movss_m(rd32(m, esp + 0x10u));
    fC_408da7 = fcmp32(x1.l(0), x0.l(0));
    if (!(fc_b(fC_408da7) || fc_z(fC_408da7))) goto L_408e61;
    fC_408db0 = fcmp32(x0.l(0), rd32(m, eax + 0x44e0u));
    if (!(fc_b(fC_408db0) || fc_z(fC_408db0))) goto L_408e61;
    x1 = movss_m(rd32(m, eax + 0x44dcu));
    x0 = movss_m(rd32(m, esp + 0x18u));
    fC_408dcb = fcmp32(x1.l(0), x0.l(0));
    if (!(fc_b(fC_408dcb) || fc_z(fC_408dcb))) goto L_408e61;
    fC_408dd4 = fcmp32(x0.l(0), rd32(m, eax + 0x44e4u));
    if (!(fc_b(fC_408dd4) || fc_z(fC_408dd4))) goto L_408e61;
    LIFT_CALL(0x402d90u, 0x408de6u);  // FUN_00402d90
    edx = eax;
    LIFT_CALL(0x402dc0u, 0x408dedu);  // FUN_00402dc0
    edi = eax;
    eax = 0x2u;
    edx = (esp + 0x10u);
    LIFT_CALL(0x7c5120u, 0x408dfdu);  // FUN_007c5120
    edx = rd32(m, 0xa946b8u);
    edi = eax;
    LIFT_CALL(0x75c580u, 0x408e0au);  // FUN_0075c580
    ecx = rd32(m, eax + 0x2214cu);
    x0 = movss_m(rd32(m, ecx + edi * 4 + 0xe8u));
    x5 = X{};
    fC_408e1c = fcmp32(x0.l(0), x5.l(0));
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_408e1c) | 2u) << 8);
    fR_408e20 = (((eax >> 8) & 0xffu) & 0x44u);
    if (!parity8(fR_408e20)) goto L_408e74;
    x0 = mulss(x0, rd32(m, esp + 0xcu));
    x0 = addss(x0, rd32(m, esi));
    x1 = x0;
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    fC_408e3a = fcmp32(x1.l(0), x0.l(0));
    if (fc_b(fC_408e3a)) goto L_408e44;
    x1 = x0;
    goto L_408e4c;
L_408e44:
    fC_408e44 = fcmp32(x5.l(0), x1.l(0));
    if (fc_b(fC_408e44)) goto L_408e4c;
    x1 = x5;
L_408e4c:
    wr32(m, esi, x1.l(0));
    wr32(m, esi + 0x4u, x1.l(0));
    wr32(m, esi + 0x8u, x1.l(0));
    wr32(m, esi + 0xcu, x1.l(0));
    goto L_408e7c;
L_408e61:
    x5 = X{};
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
L_408e6c:
    edx = rd32(m, 0xa946b8u);
    goto L_408e7c;
L_408e74:
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
L_408e7c:
    x1 = movss_m(rd32(m, esi + 0x10u));
    x2 = x0;
    x2 = divss(x2, rd32(m, 0x9c5da4u));
    x2 = mulss(x2, rd32(m, esp + 0xcu));
    x1 = subss(x1, x2.l(0));
    fC_408e96 = fcmp32(x1.l(0), x0.l(0));
    if (fc_b(fC_408e96)) goto L_408ea0;
    x1 = x0;
    goto L_408ea8;
L_408ea0:
    fC_408ea0 = fcmp32(x5.l(0), x1.l(0));
    if (fc_b(fC_408ea0)) goto L_408ea8;
    x1 = x5;
L_408ea8:
    ecx = rd32(m, 0xa95844u);
    wr32(m, esi + 0x10u, x1.l(0));
    wr32(m, esi + 0x14u, x1.l(0));
    wr32(m, esi + 0x18u, x1.l(0));
    wr32(m, esi + 0x1cu, x1.l(0));
L_408ec2:
    x1 = movss_m(rd32(m, esi + 0x10u));
    fC_408ec7 = fcmp32(x1.l(0), x5.l(0));
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_408ec7) | 2u) << 8);
    fR_408ecb = (((eax >> 8) & 0xffu) & 0x44u);
    eax = rd32(m, ecx + 0x20u);
    if (!parity8(fR_408ecb)) goto L_408f60;
    fA_408ed7 = rd32(m, eax + 0x34u);
    fB_408ed7 = ebx;
    if (fA_408ed7 != fB_408ed7) goto L_408ee4;
    eax = rd32(m, edx + 0x5a54u);
    goto L_408ef6;
L_408ee4:
    ecx = rd32(m, ecx + 0x1cu);
    fA_408ee7 = rd32(m, ecx + 0x34u);
    fB_408ee7 = ebx;
    if (fA_408ee7 != fB_408ee7) goto L_408ef4;
    eax = rd32(m, edx + 0x20c8u);
    goto L_408ef6;
L_408ef4:
    eax = 0u;
L_408ef6:
    eax = rd32(m, eax + 0x2214cu);
    x2 = movss_m(rd32(m, eax + 0x90u));
    x3 = movss_m(rd32(m, eax + 0x94u));
    x4 = movss_m(rd32(m, eax + 0x98u));
    x6 = x0;
    x6 = subss(x6, rd32(m, 0x9c5a54u));
    eax = eax + 0x90u;
    x6 = mulss(x6, x1.l(0));
    x1 = x0;
    x1 = subss(x1, x6.l(0));
    fC_408f2f = fcmp32(x1.l(0), x0.l(0));
    if (!fc_b(fC_408f2f)) goto L_408f3f;
    fC_408f34 = fcmp32(x5.l(0), x1.l(0));
    x0 = x1;
    if (fc_b(fC_408f34)) goto L_408f3f;
    x0 = x5;
L_408f3f:
    x2 = mulss(x2, x0.l(0));
    x3 = mulss(x3, x0.l(0));
    x4 = mulss(x4, x0.l(0));
    wr32(m, esi + 0x20u, x2.l(0));
    wr32(m, esi + 0x24u, x3.l(0));
    wr32(m, esi + 0x28u, x4.l(0));
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_408f60:
    fA_408f60 = rd32(m, eax + 0x34u);
    fB_408f60 = ebx;
    if (fA_408f60 != fB_408f60) goto L_408f6d;
    eax = rd32(m, edx + 0x5a54u);
    goto L_408f7f;
L_408f6d:
    ecx = rd32(m, ecx + 0x1cu);
    fA_408f70 = rd32(m, ecx + 0x34u);
    fB_408f70 = ebx;
    if (fA_408f70 != fB_408f70) goto L_408f7d;
    eax = rd32(m, edx + 0x20c8u);
    goto L_408f7f;
L_408f7d:
    eax = 0u;
L_408f7f:
    eax = rd32(m, eax + 0x2214cu);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x90u)));
    eax = eax + 0x90u;
    wr32(m, esi + 0x20u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x4u)));
    wr32(m, esi + 0x24u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x8u)));
    wr32(m, esi + 0x28u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0xcu)));
    wr32(m, esi + 0x2cu, fst32(m));
    fpop(m);
L_408fa5:
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0040a650 MXTrigger__vf19
//   original MXTrigger::vf19; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x0040a650, MXTrigger__vf19, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_40a65d = 0;
    uint32_t fB_40a65d = 0;
    uint32_t fR_40a67e = 0;
    uint32_t fA_40a69d = 0;
    uint32_t fB_40a69d = 0;
    uint32_t fA_40a6a6 = 0;
    uint32_t fB_40a6a6 = 0;
    uint32_t fA_40a6b1 = 0;
    uint32_t fB_40a6b1 = 0;
    uint32_t fR_40a6fe = 0;
    uint32_t fA_40a715 = 0;
    uint32_t fB_40a715 = 0;
    uint32_t fA_40a726 = 0;
    uint32_t fB_40a726 = 0;
    uint32_t fA_40a73f = 0;
    uint32_t fB_40a73f = 0;
    uint32_t fA_40a748 = 0;
    uint32_t fB_40a748 = 0;
    uint32_t fR_40a77f = 0;
    uint32_t fA_40a793 = 0;
    uint32_t fB_40a793 = 0;
    uint32_t fA_40a79c = 0;
    uint32_t fB_40a79c = 0;
    uint32_t fR_40a7c2 = 0;
    uint32_t fA_40a7d7 = 0;
    uint32_t fB_40a7d7 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x24u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    fA_40a65d = rd8(m, esi + 0x210u);
    fB_40a65d = 0x0u;
    esp -= 4; wr32(m, esp, edi);
    if (fA_40a65d == fB_40a65d) goto L_40a7e2;
    eax = rd32(m, 0xa95844u);
    ecx = rd32(m, eax + 0x10u);
    eax = rd32(m, ecx + 0x58u);
    edx = 0u;
    { uint32_t d_ = rd32(m, esi + 0x20cu); udiv32(eax, edx, d_); }
    fR_40a67e = (edx & edx);
    if ((fR_40a67e == 0)) goto L_40a690;
    LIFT_CALL(0x40a600u, 0x40a687u);  // FUN_0040a600
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_40a690:
    ecx = esi;
    LIFT_CALL(0x40a870u, 0x40a697u);  // FUN_0040a870
    eax = rd32(m, esi + 0xbcu);
    fA_40a69d = eax;
    fB_40a69d = 0x0u;
    eax = fA_40a69d - fB_40a69d;
    if (fA_40a69d == fB_40a69d) goto L_40a762;
    fA_40a6a6 = eax;
    fB_40a6a6 = 0x1u;
    eax = fA_40a6a6 - fB_40a6a6;
    if (fA_40a6a6 != fB_40a6a6) goto L_40a7dd;
    ebx = 0u;
    fA_40a6b1 = rd32(m, 0xa95d78u);
    fB_40a6b1 = ebx;
    if (int32_t(sext(fA_40a6b1, 4)) <= int32_t(sext(fB_40a6b1, 4))) goto L_40a7dd;
L_40a6c0:
    edx = rd32(m, 0xa95d74u);
    edi = rd32(m, edx + ebx * 4);
    eax = rd32(m, edi);
    edx = rd32(m, eax + 0x38u);
    ecx = edi;
    LIFT_CALL(edx, 0x40a6d2u);
    x0 = movss_m(rd32(m, eax + 0x30u));
    wr32(m, esp + 0x20u, x0.l(0));
    x0 = movss_m(rd32(m, eax + 0x34u));
    wr32(m, esp + 0x24u, x0.l(0));
    x0 = movss_m(rd32(m, eax + 0x38u));
    ecx = (esp + 0x20u);
    eax = esi;
    wr32(m, esp + 0x28u, x0.l(0));
    LIFT_CALL(0x403c10u, 0x40a6feu);  // FUN_00403c10
    fR_40a6fe = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_40a6fe == 0)) goto L_40a713;
    esp -= 4; wr32(m, esp, edi);
    edi = esi;
    wr8(m, esi + 0x218u, 0x1u);
    LIFT_CALL(0x40a900u, 0x40a711u);  // FUN_0040a900
    goto L_40a747;
L_40a713:
    eax = 0u;
    fA_40a715 = rd32(m, esi + 0xa8u);
    fB_40a715 = eax;
    if (int32_t(sext(fA_40a715, 4)) <= int32_t(sext(fB_40a715, 4))) goto L_40a747;
L_40a720:
    ecx = rd32(m, esi + 0xa4u);
    fA_40a726 = rd32(m, ecx + eax * 4);
    fB_40a726 = edi;
    if (fA_40a726 != fB_40a726) goto L_40a73e;
    edx = rd32(m, esi + 0xa8u);
    edx = rd32(m, ecx + edx * 4 - 0x4u);
    wr32(m, ecx + eax * 4, edx);
    wr32(m, esi + 0xa8u, (rd32(m, esi + 0xa8u) - 1u));
L_40a73e:
    eax = (eax + 1u);
    fA_40a73f = eax;
    fB_40a73f = rd32(m, esi + 0xa8u);
    if (int32_t(sext(fA_40a73f, 4)) < int32_t(sext(fB_40a73f, 4))) goto L_40a720;
L_40a747:
    ebx = (ebx + 1u);
    fA_40a748 = ebx;
    fB_40a748 = rd32(m, 0xa95d78u);
    if (int32_t(sext(fA_40a748, 4)) < int32_t(sext(fB_40a748, 4))) goto L_40a6c0;
    LIFT_CALL(0x40a980u, 0x40a759u);  // FUN_0040a980
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_40a762:
    edx = rd32(m, 0xa946b8u);
    eax = rd8(m, edx + 0x1541cu);
    edx = edx + 0x7460u;
    ebx = 0u;
    wr32(m, esp + 0x18u, edx);
    wr32(m, esp + 0x1cu, eax);
    fR_40a77f = (eax & eax);
    if (((fR_40a77f == 0) || sgn(fR_40a77f, 4) != false)) goto L_40a7dd;
    goto L_40a789;
L_40a785:
    edx = rd32(m, esp + 0x18u);
L_40a789:
    esp -= 4; wr32(m, esp, 0x3u);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x552b90u, 0x40a791u);  // FUN_00552b90
    edi = eax;
    fA_40a793 = rd32(m, edi + 0x1acu);
    fB_40a793 = 0x0u;
    if (fA_40a793 == fB_40a793) goto L_40a7d6;
    fA_40a79c = rd32(m, edi + 0x94u);
    fB_40a79c = 0x0u;
    if (fA_40a79c == fB_40a79c) goto L_40a7d6;
    ecx = rd32(m, edi + 0x1acu);
    eax = rd32(m, ecx);
    eax = rd32(m, eax + 0x44u);
    edx = (esp + 0x20u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(eax, 0x40a7b7u);
    ecx = (esp + 0x20u);
    eax = esi;
    LIFT_CALL(0x403c10u, 0x40a7c2u);  // FUN_00403c10
    fR_40a7c2 = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_40a7c2 == 0)) goto L_40a7d6;
    eax = edi;
    ecx = esi;
    wr8(m, esi + 0x218u, 0x1u);
    LIFT_CALL(0x40a8a0u, 0x40a7d6u);  // FUN_0040a8a0
L_40a7d6:
    ebx = (ebx + 1u);
    fA_40a7d7 = ebx;
    fB_40a7d7 = rd32(m, esp + 0x1cu);
    if (int32_t(sext(fA_40a7d7, 4)) < int32_t(sext(fB_40a7d7, 4))) goto L_40a785;
L_40a7dd:
    LIFT_CALL(0x40a980u, 0x40a7e2u);  // FUN_0040a980
L_40a7e2:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 0040a7f0 Fn_0040a7f0
//   original FUN_0040a7f0; register inputs: edi; returns with ret 4
REFLEX_PORT_FN(0x0040a7f0, Fn_0040a7f0, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fR_40a800 = 0;
    uint32_t fR_40a804 = 0;
    uint32_t fA_40a80e = 0;
    uint32_t fB_40a80e = 0;
    uint32_t fA_40a818 = 0;
    uint32_t fB_40a818 = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, edi + 0x94u);
    eax = 0u;
    ecx = 0u;
    fR_40a800 = (esi & esi);
    if (((fR_40a800 == 0) || sgn(fR_40a800, 4) != false)) goto L_40a81c;
L_40a804:
    fR_40a804 = (eax & eax);
    if (!(fR_40a804 == 0)) goto L_40a81c;
    edx = rd32(m, edi + 0x90u);
    fA_40a80e = rd32(m, edx + ecx * 8);
    fB_40a80e = ebx;
    edx = (edx + ecx * 8);
    if (fA_40a80e == fB_40a80e) eax = edx;
    ecx = (ecx + 1u);
    fA_40a818 = ecx;
    fB_40a818 = esi;
    if (int32_t(sext(fA_40a818, 4)) < int32_t(sext(fB_40a818, 4))) goto L_40a804;
L_40a81c:
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 0040a830 Fn_0040a830
//   original FUN_0040a830; register inputs: eax; returns with ret 4
REFLEX_PORT_FN(0x0040a830, Fn_0040a830, "fx", reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fA_40a83f = 0;
    uint32_t fB_40a83f = 0;
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    eax = rd32(m, esi + 0x98u);
    esi = esi + 0x90u;
    fA_40a83f = rd32(m, esi + 0x4u);
    fB_40a83f = eax;
    esp -= 4; wr32(m, esp, edi);
    edi = 0x1u;
    if (int32_t(sext(fA_40a83f, 4)) < int32_t(sext(fB_40a83f, 4))) goto L_40a855;
    ecx = rd32(m, esi + 0xcu);
    ecx = ecx + eax;
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x41a4e0u, 0x40a855u);  // FUN_0041a4e0
L_40a855:
    ecx = rd32(m, esi + 0x4u);
    eax = rd32(m, esi);
    edx = rd32(m, esp + 0xcu);
    wr32(m, eax + ecx * 8, edx);
    wr32(m, eax + ecx * 8 + 0x4u, edi);
    wr32(m, esi + 0x4u, rd32(m, esi + 0x4u) + edi);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 0040a870 Fn_0040a870
//   original FUN_0040a870; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x0040a870, Fn_0040a870, "fx", reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fA_40a874 = 0;
    uint32_t fB_40a874 = 0;
    uint32_t fA_40a88b = 0;
    uint32_t fB_40a88b = 0;
    edx = 0u;
    eax = 0u;
    fA_40a874 = rd32(m, ecx + 0x94u);
    fB_40a874 = edx;
    if (int32_t(sext(fA_40a874, 4)) <= int32_t(sext(fB_40a874, 4))) goto L_40a894;
    esp -= 4; wr32(m, esp, esi);
L_40a880:
    esi = rd32(m, ecx + 0x90u);
    wr32(m, esi + eax * 8 + 0x4u, edx);
    eax = (eax + 1u);
    fA_40a88b = eax;
    fB_40a88b = rd32(m, ecx + 0x94u);
    if (int32_t(sext(fA_40a88b, 4)) < int32_t(sext(fB_40a88b, 4))) goto L_40a880;
    esi = rd32(m, esp); esp += 4;
L_40a894:
    LIFT_RET(0);
}

// 0040a8a0 Fn_0040a8a0
//   original FUN_0040a8a0; register inputs: ecx eax; returns with ret 0
REFLEX_PORT_FN(0x0040a8a0, Fn_0040a8a0, "fx", reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fR_40a8af = 0;
    uint32_t fR_40a8bb = 0;
    uint32_t fR_40a8e5 = 0;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    esi = eax;
    edi = ecx;
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x40a7f0u, 0x40a8adu);  // FUN_0040a7f0
    ebx = eax;
    fR_40a8af = (ebx & ebx);
    if ((fR_40a8af == 0)) goto L_40a8d7;
    eax = rd32(m, ebx);
    eax = rd32(m, eax + 0x1acu);
    fR_40a8bb = (eax & eax);
    if ((fR_40a8bb == 0)) goto L_40a8cc;
    esp -= 4; wr32(m, esp, eax);
    ecx = edi;
    edx = 0x93609cu;
    LIFT_CALL(0x419820u, 0x40a8ccu);  // FUN_00419820
L_40a8cc:
    wr32(m, ebx + 0x4u, 0x1u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_40a8d7:
    esp -= 4; wr32(m, esp, esi);
    eax = edi;
    LIFT_CALL(0x40a830u, 0x40a8dfu);  // FUN_0040a830
    esi = rd32(m, esi + 0x1acu);
    fR_40a8e5 = (esi & esi);
    if ((fR_40a8e5 == 0)) goto L_40a8f6;
    esp -= 4; wr32(m, esp, esi);
    ecx = edi;
    edx = 0x935e84u;
    LIFT_CALL(0x419820u, 0x40a8f6u);  // FUN_00419820
L_40a8f6:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0040a980 Fn_0040a980
//   original FUN_0040a980; register inputs: ecx esi; returns with ret 0
REFLEX_PORT_FN(0x0040a980, Fn_0040a980, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_40a985 = 0;
    uint32_t fB_40a985 = 0;
    uint32_t fA_40a996 = 0;
    uint32_t fB_40a996 = 0;
    uint32_t fR_40a9c1 = 0;
    uint32_t fA_40a9d3 = 0;
    uint32_t fB_40a9d3 = 0;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, edi);
    edi = 0u;
    fA_40a985 = rd32(m, esi + 0x94u);
    fB_40a985 = edi;
    if (int32_t(sext(fA_40a985, 4)) <= int32_t(sext(fB_40a985, 4))) goto L_40a9db;
L_40a990:
    eax = rd32(m, esi + 0x90u);
    fA_40a996 = rd32(m, eax + edi * 8 + 0x4u);
    fB_40a996 = 0x0u;
    edx = rd32(m, eax + edi * 8);
    if (fA_40a996 != fB_40a996) goto L_40a9d2;
    ecx = rd32(m, esi + 0x94u);
    ebp = rd32(m, eax + ecx * 8 - 0x8u);
    wr32(m, eax + edi * 8, ebp);
    ecx = rd32(m, eax + ecx * 8 - 0x4u);
    wr32(m, eax + edi * 8 + 0x4u, ecx);
    wr32(m, esi + 0x94u, (rd32(m, esi + 0x94u) - 1u));
    eax = rd32(m, edx + 0x1acu);
    fR_40a9c1 = (eax & eax);
    if ((fR_40a9c1 == 0)) goto L_40a9d2;
    esp -= 4; wr32(m, esp, eax);
    ecx = esi;
    edx = 0x935e94u;
    LIFT_CALL(0x419820u, 0x40a9d2u);  // FUN_00419820
L_40a9d2:
    edi = (edi + 1u);
    fA_40a9d3 = edi;
    fB_40a9d3 = rd32(m, esi + 0x94u);
    if (int32_t(sext(fA_40a9d3, 4)) < int32_t(sext(fB_40a9d3, 4))) goto L_40a990;
L_40a9db:
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0040b770 OverlayColorEntity__vf19
//   original OverlayColorEntity::vf19; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x0040b770, OverlayColorEntity__vf19, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_40b77b = 0;
    uint32_t fB_40b77b = 0;
    FCmp fC_40b79d{};
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0xcu;
    eax = ecx;
    fA_40b77b = rd32(m, eax + 0x10cu);
    fB_40b77b = 0x0u;
    esp -= 4; wr32(m, esp, esi);
    if (fA_40b77b == fB_40b77b) goto L_40b9c1;
    x1 = movss_m(rd32(m, eax + 0x110u));
    x0 = movss_m(rd32(m, ebp + 0x8u));
    x2 = X{};
    x1 = subss(x1, x0.l(0));
    fC_40b79d = fcmp32(x2.l(0), x1.l(0));
    wr32(m, eax + 0x110u, x1.l(0));
    esi = (eax + 0x90u);
    edx = (eax + 0xa0u);
    if ((fc_b(fC_40b79d) || fc_z(fC_40b79d))) goto L_40b850;
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x120u)));
    ecx = (eax + 0xb0u);
    wr32(m, esi, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x124u)));
    wr32(m, esi + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x128u)));
    wr32(m, esi + 0x8u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x12cu)));
    wr32(m, esi + 0xcu, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x130u)));
    wr32(m, edx, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x134u)));
    wr32(m, edx + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x138u)));
    wr32(m, edx + 0x8u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x13cu)));
    wr32(m, edx + 0xcu, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x140u)));
    wr32(m, ecx, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x144u)));
    wr32(m, ecx + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x148u)));
    wr32(m, ecx + 0x8u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x14cu)));
    wr32(m, ecx + 0xcu, fst32(m));
    fpop(m);
    wr32(m, eax + 0x10cu, 0x0u);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x150u)));
    wr32(m, eax + 0xc0u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x154u)));
    wr32(m, eax + 0xc4u, fst32(m));
    fpop(m);
    goto L_40b9b3;
L_40b850:
    x1 = movss_m(rd32(m, eax + 0x160u));
    x2 = movss_m(rd32(m, eax + 0x164u));
    x3 = movss_m(rd32(m, eax + 0x168u));
    x4 = movss_m(rd32(m, eax + 0x16cu));
    x5 = movss_m(rd32(m, esi));
    x1 = mulss(x1, x0.l(0));
    x5 = addss(x5, x1.l(0));
    x1 = movss_m(rd32(m, esi + 0x4u));
    wr32(m, esi, x5.l(0));
    x2 = mulss(x2, x0.l(0));
    x1 = addss(x1, x2.l(0));
    wr32(m, esi + 0x4u, x1.l(0));
    x1 = movss_m(rd32(m, esi + 0x8u));
    x3 = mulss(x3, x0.l(0));
    x1 = addss(x1, x3.l(0));
    wr32(m, esi + 0x8u, x1.l(0));
    x1 = movss_m(rd32(m, esi + 0xcu));
    x4 = mulss(x4, x0.l(0));
    x1 = addss(x1, x4.l(0));
    wr32(m, esi + 0xcu, x1.l(0));
    x2 = movss_m(rd32(m, eax + 0x174u));
    x3 = movss_m(rd32(m, eax + 0x178u));
    x4 = movss_m(rd32(m, eax + 0x17cu));
    x5 = movss_m(rd32(m, edx));
    x1 = x0;
    x1 = mulss(x1, rd32(m, eax + 0x170u));
    x5 = addss(x5, x1.l(0));
    x1 = movss_m(rd32(m, edx + 0x4u));
    wr32(m, edx, x5.l(0));
    x2 = mulss(x2, x0.l(0));
    x1 = addss(x1, x2.l(0));
    wr32(m, edx + 0x4u, x1.l(0));
    x1 = movss_m(rd32(m, edx + 0x8u));
    x3 = mulss(x3, x0.l(0));
    x1 = addss(x1, x3.l(0));
    wr32(m, edx + 0x8u, x1.l(0));
    x1 = movss_m(rd32(m, edx + 0xcu));
    ecx = (eax + 0xb0u);
    x4 = mulss(x4, x0.l(0));
    x1 = addss(x1, x4.l(0));
    wr32(m, edx + 0xcu, x1.l(0));
    x4 = movss_m(rd32(m, eax + 0x180u));
    x1 = movss_m(rd32(m, eax + 0x184u));
    x2 = movss_m(rd32(m, eax + 0x188u));
    x3 = movss_m(rd32(m, eax + 0x18cu));
    x5 = movss_m(rd32(m, ecx));
    x1 = mulss(x1, x0.l(0));
    x1 = addss(x1, rd32(m, ecx + 0x4u));
    wr32(m, ecx + 0x4u, x1.l(0));
    x4 = mulss(x4, x0.l(0));
    x2 = mulss(x2, x0.l(0));
    x2 = addss(x2, rd32(m, ecx + 0x8u));
    wr32(m, ecx + 0x8u, x2.l(0));
    x3 = mulss(x3, x0.l(0));
    x3 = addss(x3, rd32(m, ecx + 0xcu));
    wr32(m, ecx + 0xcu, x3.l(0));
    x5 = addss(x5, x4.l(0));
    wr32(m, ecx, x5.l(0));
    x1 = movss_m(rd32(m, eax + 0x190u));
    x1 = mulss(x1, x0.l(0));
    x1 = addss(x1, rd32(m, eax + 0xc0u));
    wr32(m, eax + 0xc0u, x1.l(0));
    x1 = movss_m(rd32(m, eax + 0x194u));
    x1 = mulss(x1, x0.l(0));
    x1 = addss(x1, rd32(m, eax + 0xc4u));
    wr32(m, eax + 0xc4u, x1.l(0));
L_40b9b3:
    eax = rd32(m, 0xa95844u);
    eax = rd32(m, eax + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x40b1d0u, 0x40b9c1u);  // OverlayColor_SetSceneColors
L_40b9c1:
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00410c60 ScrollingTextureEntity__vf22
//   original ScrollingTextureEntity::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00410c60, ScrollingTextureEntity__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x1078u;
    LIFT_RET(0);
}

// 00410dd0 ScrollingTextureEntity__vf19
//   original ScrollingTextureEntity::vf19; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x00410dd0, ScrollingTextureEntity__vf19, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    FCmp fC_410dee{};
    uint32_t fR_410df2 = 0;
    FCmp fC_410e08{};
    FCmp fC_410e3d{};
    uint32_t fR_410e41 = 0;
    FCmp fC_410e58{};
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    x2 = movss_m(rd32(m, ecx + 0xab0u));
    x5 = X{};
    x3 = movss_m(rd32(m, ebp + 0x8u));
    x4 = movss_m(rd32(m, 0x977570u) /* nan */);
    fC_410dee = fcmp32(x2.l(0), x5.l(0));
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_410dee) | 2u) << 8);
    fR_410df2 = (((eax >> 8) & 0xffu) & 0x44u);
    if (!parity8(fR_410df2)) goto L_410e35;
    x0 = x3;
    x0 = addss(x0, rd32(m, ecx + 0xab8u));
    x1 = x2;
    x1 = andps(x1, x4);
    fC_410e08 = fcmp32(x0.l(0), x1.l(0));
    wr32(m, ecx + 0xab8u, x0.l(0));
    if (fc_b(fC_410e08)) goto L_410e21;
    x0 = subss(x0, x1.l(0));
    wr32(m, ecx + 0xab8u, x0.l(0));
L_410e21:
    x0 = movss_m(rd32(m, ecx + 0xab8u));
    x0 = divss(x0, x2.l(0));
    wr32(m, ecx + 0xaa0u, x0.l(0));
L_410e35:
    x2 = movss_m(rd32(m, ecx + 0xab4u));
    fC_410e3d = fcmp32(x2.l(0), x5.l(0));
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_410e3d) | 2u) << 8);
    fR_410e41 = (((eax >> 8) & 0xffu) & 0x44u);
    if (!parity8(fR_410e41)) goto L_410e85;
    x0 = movss_m(rd32(m, ecx + 0xabcu));
    x0 = addss(x0, x3.l(0));
    x1 = x2;
    x1 = andps(x1, x4);
    fC_410e58 = fcmp32(x0.l(0), x1.l(0));
    wr32(m, ecx + 0xabcu, x0.l(0));
    if (fc_b(fC_410e58)) goto L_410e71;
    x0 = subss(x0, x1.l(0));
    wr32(m, ecx + 0xabcu, x0.l(0));
L_410e71:
    x0 = movss_m(rd32(m, ecx + 0xabcu));
    x0 = divss(x0, x2.l(0));
    wr32(m, ecx + 0xaa4u, x0.l(0));
L_410e85:
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00411040 rtti_shared_vf_00411040_00411040
//   original rtti_shared_vf_00411040; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00411040, rtti_shared_vf_00411040_00411040, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x2u;
    LIFT_RET(0);
}

// 00413a80 TimedRaceSection__vf19
//   original TimedRaceSection::vf19; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x00413a80, TimedRaceSection__vf19, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_413a89 = 0;
    uint32_t fB_413a89 = 0;
    uint32_t fA_413a9f = 0;
    uint32_t fB_413a9f = 0;
    uint32_t fA_413abb = 0;
    uint32_t fB_413abb = 0;
    uint32_t fA_413ad6 = 0;
    uint32_t fB_413ad6 = 0;
    uint32_t fR_413af8 = 0;
    uint32_t fA_413afc = 0;
    uint32_t fB_413afc = 0;
    uint32_t fR_413b1e = 0;
    uint32_t fA_413b2c = 0;
    uint32_t fB_413b2c = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x24u;
    fA_413a89 = rd32(m, 0xa942b4u);
    fB_413a89 = 0x2u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    esi = ecx;
    if (fA_413a89 == fB_413a89) goto L_413b37;
    ebx = 0u;
    eax = 0u;
    fA_413a9f = rd32(m, esi + 0x94u);
    fB_413a9f = ebx;
    if (int32_t(sext(fA_413a9f, 4)) <= int32_t(sext(fB_413a9f, 4))) goto L_413ac3;
    goto L_413ab0;
L_413ab0:
    ecx = rd32(m, esi + 0x90u);
    wr32(m, ecx + eax * 8 + 0x4u, ebx);
    eax = (eax + 1u);
    fA_413abb = eax;
    fB_413abb = rd32(m, esi + 0x94u);
    if (int32_t(sext(fA_413abb, 4)) < int32_t(sext(fB_413abb, 4))) goto L_413ab0;
L_413ac3:
    edx = rd32(m, 0xa946b8u);
    eax = rd8(m, edx + 0x1541cu);
    edx = edx + 0x7460u;
    fA_413ad6 = eax;
    fB_413ad6 = ebx;
    wr32(m, esp + 0x18u, edx);
    wr32(m, esp + 0x1cu, eax);
    if (int32_t(sext(fA_413ad6, 4)) <= int32_t(sext(fB_413ad6, 4))) goto L_413b32;
    goto L_413ae8;
L_413ae4:
    edx = rd32(m, esp + 0x18u);
L_413ae8:
    esp -= 4; wr32(m, esp, 0x3u);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x552b90u, 0x413af0u);  // FUN_00552b90
    edi = eax;
    eax = rd32(m, edi + 0x1acu);
    fR_413af8 = (eax & eax);
    if ((fR_413af8 == 0)) goto L_413b2b;
    fA_413afc = rd32(m, eax + 0x280u);
    fB_413afc = 0x0u;
    if (fA_413afc != fB_413afc) goto L_413b2b;
    ecx = eax;
    edx = rd32(m, ecx);
    edx = rd32(m, edx + 0x44u);
    eax = (esp + 0x20u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(edx, 0x413b13u);
    ecx = (esp + 0x20u);
    eax = esi;
    LIFT_CALL(0x403c10u, 0x413b1eu);  // FUN_00403c10
    fR_413b1e = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_413b1e == 0)) goto L_413b2b;
    eax = edi;
    ecx = esi;
    LIFT_CALL(0x413b40u, 0x413b2bu);  // FUN_00413b40
L_413b2b:
    ebx = (ebx + 1u);
    fA_413b2c = ebx;
    fB_413b2c = rd32(m, esp + 0x1cu);
    if (int32_t(sext(fA_413b2c, 4)) < int32_t(sext(fB_413b2c, 4))) goto L_413ae4;
L_413b32:
    LIFT_CALL(0x40a980u, 0x413b37u);  // FUN_0040a980
L_413b37:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00413b40 Fn_00413b40
//   original FUN_00413b40; register inputs: ecx eax; returns with ret 0
REFLEX_PORT_FN(0x00413b40, Fn_00413b40, "fx", reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fR_413b4d = 0;
    uint32_t fR_413b6a = 0;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    esi = eax;
    edi = ecx;
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x40a7f0u, 0x413b4du);  // FUN_0040a7f0
    fR_413b4d = (eax & eax);
    if ((fR_413b4d == 0)) goto L_413b5c;
    wr32(m, eax + 0x4u, 0x1u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_413b5c:
    esp -= 4; wr32(m, esp, esi);
    eax = edi;
    LIFT_CALL(0x40a830u, 0x413b64u);  // FUN_0040a830
    esi = rd32(m, esi + 0x1acu);
    fR_413b6a = (esi & esi);
    if ((fR_413b6a == 0)) goto L_413b76;
    esp -= 4; wr32(m, esp, esi);
    ecx = edi;
    LIFT_CALL(0x419a10u, 0x413b76u);  // FUN_00419a10
L_413b76:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00413d20 Fn_00413d20
//   original FUN_00413d20; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x00413d20, Fn_00413d20, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX) {
    LIFT_ENTRY();
    uint32_t fR_413d26 = 0;
    esp -= 4; wr32(m, esp, ecx);
    eax = 0x1u;
    fR_413d26 = (rd8(m, 0x9e275cu) & (eax & 0xffu));
    if (!(fR_413d26 == 0)) goto L_413d46;
    wr32(m, 0x9e275cu, (rd32(m, 0x9e275cu) | eax));
    LIFT_CALL(0x5b2370u, 0x413d39u);  // MXSeriesManager_Construct
    esp -= 4; wr32(m, esp, 0x911da0u);
    LIFT_CALL(0x8f9fbfu, 0x413d43u);  // _atexit
    esp = esp + 0x4u;
L_413d46:
    eax = 0x9e2188u;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00415320 Fn_00415320
//   original FUN_00415320; register inputs: esi; returns with ret 0
REFLEX_PORT_FN(0x00415320, Fn_00415320, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX) {
    LIFT_ENTRY();
    uint32_t fA_415329 = 0;
    uint32_t fB_415329 = 0;
    eax = rd32(m, esi + 0xa4u);
    esp -= 4; wr32(m, esp, edi);
    edi = 0u;
    fA_415329 = eax;
    fB_415329 = edi;
    if (fA_415329 == fB_415329) goto L_415336;
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x78fc50u, 0x415333u);  // ReflexAlignedFree
    esp = esp + 0x4u;
L_415336:
    eax = rd32(m, esi + 0x98u);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esi + 0xa4u, edi);
    wr32(m, esi + 0xa8u, edi);
    wr32(m, esi + 0xacu, edi);
    LIFT_CALL(0x78fc50u, 0x415354u);  // ReflexAlignedFree
    esp = esp + 0x4u;
    edi = (esi + 0x88u);
    LIFT_CALL(0x4eedc0u, 0x415362u);  // FUN_004eedc0
    edi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00416e40 Fn_00416e40
//   original FUN_00416e40; register inputs: esi; returns with ret 8
REFLEX_PORT_FN(0x00416e40, Fn_00416e40, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX | (reflex_port::OUT_XMM0 << 0)) {
    LIFT_ENTRY();
    uint32_t fR_416e58 = 0;
    uint32_t fR_416e6b = 0;
    uint32_t fA_416e6f = 0;
    uint32_t fB_416e6f = 0;
    uint32_t fA_416e79 = 0;
    uint32_t fB_416e79 = 0;
    uint32_t fR_416e8e = 0;
    esp = esp - 0x18u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x28u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, ebp);
    eax = esi;
    LIFT_CALL(0x417da0u, 0x416e52u);  // FUN_00417da0
    ebx = eax;
    wr32(m, esp + 0x14u, ebx);
    fR_416e58 = (esi & esi);
    if (!(fR_416e58 == 0)) goto L_416e62;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x416e62u);
L_416e62:
    eax = rd32(m, esi);
    edi = rd32(m, esi + 0x18u);
    wr32(m, esp + 0x10u, eax);
    fR_416e6b = (eax & eax);
    if ((fR_416e6b == 0)) goto L_416e73;
    fA_416e6f = eax;
    fB_416e6f = eax;
    if (fA_416e6f == fB_416e6f) goto L_416e79;
L_416e73:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x416e79u);
L_416e79:
    fA_416e79 = ebx;
    fB_416e79 = edi;
    if (fA_416e79 == fB_416e79) goto L_416e98;
    eax = rd32(m, ebx + 0xcu);
    ecx = rd32(m, ebp);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(rd32(m, 0x9162bcu) /* 8.287018476502604e-39 */, 0x416e8bu);
    esp = esp + 0x8u;
    fR_416e8e = (eax & eax);
    if ((sgn(fR_416e8e, 4) != false)) goto L_416e98;
    eax = (esp + 0x10u);
    goto L_416ea9;
L_416e98:
    eax = rd32(m, esi);
    edx = rd32(m, esi + 0x18u);
    wr32(m, esp + 0x18u, eax);
    wr32(m, esp + 0x1cu, edx);
    eax = (esp + 0x18u);
L_416ea9:
    edx = rd32(m, eax);
    ecx = rd32(m, esp + 0x28u);
    eax = rd32(m, eax + 0x4u);
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    wr32(m, ecx + 0x4u, eax);
    wr32(m, ecx, edx);
    eax = ecx;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(8);
}

// 004172e0 ConVar_HU_ConVarTraits_H__vf2
//   original ConVar_HU_ConVarTraits_H::vf2; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x004172e0, ConVar_HU_ConVarTraits_H__vf2, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fR_41730d = 0;
    uint32_t fA_417311 = 0;
    uint32_t fB_417311 = 0;
    uint32_t fR_41731e = 0;
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, 0x916388u) /* 8.287789190657982e-39 */;
    esi = ecx;
    esp -= 4; wr32(m, esp, 0x9e1a7cu);
    ecx = 0x9cbb50u;
    LIFT_CALL(edi, 0x4172f6u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, 0x9e1a7cu);
    ecx = 0x9cbb50u;
    LIFT_CALL(edi, 0x417303u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x9162bcu) /* 8.287018476502604e-39 */, 0x41730au);
    esp = esp + 0x8u;
    fR_41730d = (eax & eax);
    if (!(fR_41730d == 0)) goto L_417331;
    fA_417311 = rd32(m, esi + 0x24u);
    fB_417311 = eax;
    edx = rd32(m, esp + 0xcu);
    eax = (eax & 0xffffff00u) | (((fA_417311 == fB_417311) ? 1u : 0u) & 0xffu);
    ecx = 0u;
    edi = rd32(m, esp); esp += 4;
    fR_41731e = (eax & eax);
    ecx = (ecx & 0xffffff00u) | (((!(fR_41731e == 0)) ? 1u : 0u) & 0xffu);
    wr32(m, esi + 0x24u, eax);
    eax = 0x1u;
    esi = rd32(m, esp); esp += 4;
    wr32(m, edx, ecx);
    LIFT_RET(4);
L_417331:
    edi = rd32(m, esp); esp += 4;
    eax = 0u;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00417590 Fn_00417590
//   original FUN_00417590; register inputs: ecx esi; returns with ret 0
REFLEX_PORT_FN(0x00417590, Fn_00417590, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fR_417594 = 0;
    esp -= 4; wr32(m, esp, ecx);
    eax = rd32(m, esi + 0xcu);
    fR_417594 = (eax & eax);
    if ((fR_417594 == 0)) goto L_4175b2;
    ecx = rd32(m, esp);
    edx = rd32(m, esi + 0x10u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x41ac70u, 0x4175a6u);  // FUN_0041ac70
    eax = rd32(m, esi + 0xcu);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x78fc50u, 0x4175afu);  // ReflexAlignedFree
    esp = esp + 0x10u;
L_4175b2:
    ecx = rd32(m, esi);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esi + 0xcu, 0x0u);
    wr32(m, esi + 0x10u, 0x0u);
    wr32(m, esi + 0x14u, 0x0u);
    LIFT_CALL(0x78fc50u, 0x4175cfu);  // ReflexAlignedFree
    esp = esp + 0x8u;
    LIFT_RET(0);
}

// 004175e0 Fn_004175e0
//   original FUN_004175e0; register inputs: edi; returns with ret 4
REFLEX_PORT_FN(0x004175e0, Fn_004175e0, "fx", reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fR_4175ed = 0;
    uint32_t fA_417623 = 0;
    uint32_t fB_417623 = 0;
    uint32_t fA_417654 = 0;
    uint32_t fB_417654 = 0;
    esp = esp - 0xcu;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, edi + 0xcu);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x18u);
    esp -= 4; wr32(m, esp, esi);
    fR_4175ed = (ebx & ebx);
    if (!(fR_4175ed == 0)) goto L_4175f5;
    ecx = 0u;
    goto L_41760b;
L_4175f5:
    ecx = rd32(m, edi + 0x14u);
    ecx = ecx - ebx;
    eax = 0x2e8ba2e9u;
    { int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); }
    edx = uint32_t(int32_t(edx) >> 5);
    ecx = edx;
    ecx = (ecx >> 31);
    ecx = ecx + edx;
L_41760b:
    esi = rd32(m, edi + 0x10u);
    edx = esi;
    edx = edx - ebx;
    eax = 0x2e8ba2e9u;
    { int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(edx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); }
    edx = uint32_t(int32_t(edx) >> 5);
    eax = edx;
    eax = (eax >> 31);
    eax = eax + edx;
    fA_417623 = eax;
    fB_417623 = ecx;
    if (fA_417623 >= fB_417623) goto L_417654;
    wr8(m, esp + 0x1cu, 0x0u);
    ecx = rd32(m, esp + 0x1cu);
    edx = rd32(m, esp + 0x1cu);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, 0x1u);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x41abd0u, 0x41763fu);  // FUN_0041abd0
    esp = esp + 0x14u;
    esi = esi + 0xb0u;
    wr32(m, edi + 0x10u, esi);
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0xcu;
    LIFT_RET(4);
L_417654:
    fA_417654 = ebx;
    fB_417654 = esi;
    if (fA_417654 <= fB_417654) goto L_41765e;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x41765eu);
L_41765e:
    eax = rd32(m, edi);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ebp);
    eax = (esp + 0x1cu);
    esp -= 4; wr32(m, esp, eax);
    eax = edi;
    LIFT_CALL(0x417b60u, 0x41766fu);  // FUN_00417b60
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0xcu;
    LIFT_RET(4);
}

// 00417b60 Fn_00417b60
//   original FUN_00417b60; register inputs: eax; returns with ret 16
REFLEX_PORT_FN(0x00417b60, Fn_00417b60, "fx", reflex_port::OUT_ECX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_417b86 = 0;
    uint32_t fB_417b86 = 0;
    uint32_t fA_417b8e = 0;
    uint32_t fB_417b8e = 0;
    uint32_t fR_417b9e = 0;
    uint32_t fA_417ba2 = 0;
    uint32_t fB_417ba2 = 0;
    uint32_t fA_417bdb = 0;
    uint32_t fB_417bdb = 0;
    esp = esp - 0xcu;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x18u);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    ebx = rd32(m, esi + 0x10u);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esi + 0xcu);
    ecx = ebx;
    ecx = ecx - edi;
    eax = 0x2e8ba2e9u;
    { int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); }
    edx = uint32_t(int32_t(edx) >> 5);
    eax = edx;
    eax = (eax >> 31);
    fA_417b86 = eax;
    fB_417b86 = edx;
    eax = fA_417b86 + fB_417b86;
    if (!(((fA_417b86 + fB_417b86)) == 0)) goto L_417b8e;
    edi = 0u;
    goto L_417bc3;
L_417b8e:
    fA_417b8e = edi;
    fB_417b8e = ebx;
    if (fA_417b8e <= fB_417b8e) goto L_417b98;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x417b98u);
L_417b98:
    ecx = rd32(m, esp + 0x28u);
    eax = rd32(m, esi);
    fR_417b9e = (ecx & ecx);
    if ((fR_417b9e == 0)) goto L_417ba6;
    fA_417ba2 = ecx;
    fB_417ba2 = eax;
    if (fA_417ba2 == fB_417ba2) goto L_417bac;
L_417ba6:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x417bacu);
L_417bac:
    ecx = rd32(m, esp + 0x2cu);
    ecx = ecx - edi;
    eax = 0x2e8ba2e9u;
    { int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); }
    edx = uint32_t(int32_t(edx) >> 5);
    edi = edx;
    edi = (edi >> 31);
    edi = edi + edx;
L_417bc3:
    ecx = rd32(m, esp + 0x2cu);
    edx = rd32(m, esp + 0x28u);
    eax = rd32(m, esp + 0x24u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x418470u, 0x417bd8u);  // FUN_00418470
    ebx = rd32(m, esi + 0xcu);
    fA_417bdb = ebx;
    fB_417bdb = rd32(m, esi + 0x10u);
    if (fA_417bdb <= fB_417bdb) goto L_417be6;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x417be6u);
L_417be6:
    esi = rd32(m, esi);
    wr32(m, esp + 0x10u, esi);
    esp -= 4; wr32(m, esp, edi);
    esi = (esp + 0x14u);
    wr32(m, esp + 0x18u, ebx);
    LIFT_CALL(0x418e00u, 0x417bfau);  // FUN_00418e00
    ecx = rd32(m, esp + 0x10u);
    edx = rd32(m, esp + 0x14u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    wr32(m, ebp, ecx);
    wr32(m, ebp + 0x4u, edx);
    eax = ebp;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0xcu;
    LIFT_RET(16);
}

// 00417c20 Fn_00417c20
//   original FUN_00417c20; register inputs: ecx edx esi edi; returns with ret 0
REFLEX_PORT_FN(0x00417c20, Fn_00417c20, "fx", reflex_port::OUT_EAX) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, ecx);
    wr8(m, esp, 0x0u);
    eax = rd32(m, esp);
    ecx = rd32(m, esp);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x41abd0u, 0x417c35u);  // FUN_0041abd0
    eax = esi;
    eax = uint32_t(int32_t(eax) * int32_t(0xb0u));
    eax = eax + edi;
    esp = esp + 0x18u;
    LIFT_RET(0);
}

// 00417da0 Fn_00417da0
//   original FUN_00417da0; register inputs: eax; returns with ret 4
REFLEX_PORT_FN(0x00417da0, Fn_00417da0, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | (reflex_port::OUT_XMM0 << 0)) {
    LIFT_ENTRY();
    uint32_t fA_417dac = 0;
    uint32_t fB_417dac = 0;
    uint32_t fR_417dcd = 0;
    uint32_t fA_417dda = 0;
    uint32_t fB_417dda = 0;
    eax = rd32(m, eax + 0x18u);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, eax + 0x4u);
    fA_417dac = rd8(m, esi + 0x15u);
    fB_417dac = 0x0u;
    esp -= 4; wr32(m, esp, edi);
    edi = eax;
    if (fA_417dac != fB_417dac) goto L_417de3;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, 0x9162bcu) /* 8.287018476502604e-39 */;
L_417dc0:
    ecx = rd32(m, ebp);
    edx = rd32(m, esi + 0xcu);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(ebx, 0x417dcau);
    esp = esp + 0x8u;
    fR_417dcd = (eax & eax);
    if ((sgn(fR_417dcd, 4) == false)) goto L_417dd6;
    esi = rd32(m, esi + 0x8u);
    goto L_417dda;
L_417dd6:
    edi = esi;
    esi = rd32(m, esi);
L_417dda:
    fA_417dda = rd8(m, esi + 0x15u);
    fB_417dda = 0x0u;
    if (fA_417dda == fB_417dda) goto L_417dc0;
    eax = edi;
    ebx = rd32(m, esp); esp += 4;
L_417de3:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00417df0 Fn_00417df0
//   original FUN_00417df0; register inputs: esi; returns with ret 0
REFLEX_PORT_FN(0x00417df0, Fn_00417df0, "fx", reflex_port::OUT_ECX | reflex_port::OUT_EDX | (reflex_port::OUT_XMM0 << 0)) {
    LIFT_ENTRY();
    uint32_t fA_417df0 = 0;
    uint32_t fB_417df0 = 0;
    uint32_t fA_417e01 = 0;
    uint32_t fB_417e01 = 0;
    uint32_t fA_417e0e = 0;
    uint32_t fB_417e0e = 0;
    uint32_t fA_417e16 = 0;
    uint32_t fB_417e16 = 0;
    uint32_t fA_417e24 = 0;
    uint32_t fB_417e24 = 0;
    uint32_t fA_417e32 = 0;
    uint32_t fB_417e32 = 0;
    uint32_t fA_417e3b = 0;
    uint32_t fB_417e3b = 0;
    uint32_t fA_417e48 = 0;
    uint32_t fB_417e48 = 0;
    fA_417df0 = rd32(m, esi);
    fB_417df0 = 0x0u;
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    if (fA_417df0 != fB_417df0) goto L_417dfe;
    LIFT_CALL(edi, 0x417dfeu);
L_417dfe:
    eax = rd32(m, esi + 0x4u);
    fA_417e01 = rd8(m, eax + 0x19u);
    fB_417e01 = 0x0u;
    if (fA_417e01 == fB_417e01) goto L_417e0b;
    LIFT_CALL(edi, 0x417e09u);
    edi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_417e0b:
    ecx = rd32(m, eax + 0x8u);
    fA_417e0e = rd8(m, ecx + 0x19u);
    fB_417e0e = 0x0u;
    if (fA_417e0e != fB_417e0e) goto L_417e2f;
    eax = rd32(m, ecx);
    fA_417e16 = rd8(m, eax + 0x19u);
    fB_417e16 = 0x0u;
    if (fA_417e16 != fB_417e16) goto L_417e2a;
L_417e20:
    ecx = eax;
    eax = rd32(m, ecx);
    fA_417e24 = rd8(m, eax + 0x19u);
    fB_417e24 = 0x0u;
    if (fA_417e24 == fB_417e24) goto L_417e20;
L_417e2a:
    wr32(m, esi + 0x4u, ecx);
    edi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_417e2f:
    eax = rd32(m, eax + 0x4u);
    fA_417e32 = rd8(m, eax + 0x19u);
    fB_417e32 = 0x0u;
    if (fA_417e32 != fB_417e32) goto L_417e4e;
L_417e38:
    ecx = rd32(m, esi + 0x4u);
    fA_417e3b = ecx;
    fB_417e3b = rd32(m, eax + 0x8u);
    if (fA_417e3b != fB_417e3b) goto L_417e4e;
    wr32(m, esi + 0x4u, eax);
    edx = eax;
    eax = rd32(m, edx + 0x4u);
    fA_417e48 = rd8(m, eax + 0x19u);
    fB_417e48 = 0x0u;
    if (fA_417e48 == fB_417e48) goto L_417e38;
L_417e4e:
    wr32(m, esi + 0x4u, eax);
    edi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00418470 Fn_00418470
//   original FUN_00418470; register inputs: none; returns with ret 16
REFLEX_PORT_FN(0x00418470, Fn_00418470, "fx", reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fR_4184b6 = 0;
    uint32_t fA_4184f5 = 0;
    uint32_t fB_4184f5 = 0;
    uint32_t fA_418500 = 0;
    uint32_t fB_418500 = 0;
    uint32_t fA_418513 = 0;
    uint32_t fB_418513 = 0;
    uint32_t fA_418531 = 0;
    uint32_t fB_418531 = 0;
    uint32_t fR_4185b5 = 0;
    uint32_t fA_418646 = 0;
    uint32_t fB_418646 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp -= 4; wr32(m, esp, 0xffffffffu);
    esp -= 4; wr32(m, esp, 0x903240u);
    eax = rd32(m, FS_BASE);
    esp -= 4; wr32(m, esp, eax);
    esp = esp - 0xccu;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ ebp);
    wr32(m, ebp - 0x14u, eax);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    eax = (ebp - 0xcu);
    wr32(m, FS_BASE, eax);
    wr32(m, ebp - 0x10u, esp);
    ebx = rd32(m, ebp + 0x8u);
    wr32(m, ebp - 0xccu, ebx);
    eax = rd32(m, ebp + 0xcu);
    wr32(m, ebp - 0xd4u, eax);
    ecx = rd32(m, ebx + 0xcu);
    fR_4184b6 = (ecx & ecx);
    if (!(fR_4184b6 == 0)) goto L_4184be;
    edi = 0u;
    goto L_4184d6;
L_4184be:
    edx = rd32(m, ebx + 0x14u);
    edx = edx - ecx;
    eax = 0x2e8ba2e9u;
    { int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(edx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); }
    edx = uint32_t(int32_t(edx) >> 5);
    eax = edx;
    eax = (eax >> 31);
    eax = eax + edx;
    edi = eax;
L_4184d6:
    esi = rd32(m, ebx + 0x10u);
    edx = esi;
    edx = edx - ecx;
    eax = 0x2e8ba2e9u;
    { int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(edx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); }
    edx = uint32_t(int32_t(edx) >> 5);
    eax = edx;
    eax = (eax >> 31);
    eax = eax + edx;
    ecx = 0x1745d17u;
    ecx = ecx - eax;
    fA_4184f5 = ecx;
    fB_4184f5 = 0x1u;
    if (fA_4184f5 >= fB_4184f5) goto L_4184ff;
    LIFT_CALL(0x851560u, 0x4184ffu);  // FUN_00851560
L_4184ff:
    eax = (eax + 1u);
    fA_418500 = edi;
    fB_418500 = eax;
    if (fA_418500 >= fB_418500) goto L_418632;
    ecx = edi;
    ecx = (ecx >> 1);
    edx = 0x1745d17u;
    edx = edx - ecx;
    fA_418513 = edx;
    fB_418513 = edi;
    if (fA_418513 >= fB_418513) goto L_418529;
    wr32(m, ebp - 0xd0u, 0x0u);
    edi = rd32(m, ebp - 0xd0u);
    goto L_418531;
L_418529:
    edi = edi + ecx;
    wr32(m, ebp - 0xd0u, edi);
L_418531:
    fA_418531 = edi;
    fB_418531 = eax;
    if (fA_418531 >= fB_418531) goto L_41853d;
    wr32(m, ebp - 0xd0u, eax);
    edi = eax;
L_41853d:
    ecx = edi;
    LIFT_CALL(0x419cd0u, 0x418544u);  // FUN_00419cd0
    wr32(m, ebp - 0xccu, eax);
    wr32(m, ebp - 0xd8u, eax);
    wr32(m, ebp - 0x4u, 0x0u);
    ecx = rd32(m, ebp + 0x14u);
    esp -= 4; wr32(m, esp, ecx);
    edx = rd32(m, ebx + 0xcu);
    esp -= 4; wr32(m, esp, edx);
    edx = eax;
    LIFT_CALL(0x419c70u, 0x418566u);  // FUN_00419c70
    wr32(m, ebp - 0xd8u, eax);
    edx = rd32(m, ebp - 0xd4u);
    esi = 0x1u;
    edi = eax;
    LIFT_CALL(0x417c20u, 0x41857eu);  // FUN_00417c20
    wr32(m, ebp - 0xd8u, eax);
    ecx = rd32(m, ebx + 0x10u);
    esp -= 4; wr32(m, esp, ecx);
    edx = rd32(m, ebp + 0x14u);
    esp -= 4; wr32(m, esp, edx);
    edx = eax;
    LIFT_CALL(0x419c70u, 0x418593u);  // FUN_00419c70
    wr32(m, ebp - 0x4u, 0xffffffffu);
    ecx = rd32(m, ebx + 0x10u);
    esi = rd32(m, ebx + 0xcu);
    ecx = ecx - esi;
    eax = 0x2e8ba2e9u;
    { int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); }
    edx = uint32_t(int32_t(edx) >> 5);
    eax = edx;
    eax = (eax >> 31);
    edi = (edx + eax + 0x1u);
    fR_4185b5 = (esi & esi);
    if ((fR_4185b5 == 0)) goto L_4185d9;
    ecx = rd32(m, ebp - 0xccu);
    esp -= 4; wr32(m, esp, ecx);
    eax = rd32(m, ebx + 0x10u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x41ac70u, 0x4185cau);  // FUN_0041ac70
    esp = esp + 0xcu;
    edx = rd32(m, ebx + 0xcu);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x78fc50u, 0x4185d6u);  // ReflexAlignedFree
    esp = esp + 0x4u;
L_4185d9:
    eax = rd32(m, ebp - 0xd0u);
    eax = uint32_t(int32_t(eax) * int32_t(0xb0u));
    ecx = rd32(m, ebp - 0xccu);
    eax = eax + ecx;
    wr32(m, ebx + 0x14u, eax);
    edi = uint32_t(int32_t(edi) * int32_t(0xb0u));
    edi = edi + ecx;
    wr32(m, ebx + 0x10u, edi);
    wr32(m, ebx + 0xcu, ecx);
    goto L_418779;
L_418632:
    esi = esi - rd32(m, ebp + 0x14u);
    eax = 0x2e8ba2e9u;
    { int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(esi)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); }
    edx = uint32_t(int32_t(edx) >> 5);
    eax = edx;
    eax = (eax >> 31);
    eax = eax + edx;
    fA_418646 = eax;
    fB_418646 = 0x1u;
    if (fA_418646 >= fB_418646) goto L_418707;
    ecx = rd32(m, ebp - 0xd4u);
    esp -= 4; wr32(m, esp, ecx);
    edx = (ebp - 0xc8u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x4187a0u, 0x418662u);  // FUN_004187a0
    eax = rd32(m, ebp + 0x14u);
    edx = (eax + 0xb0u);
    ecx = rd32(m, ebx + 0x10u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x419c70u, 0x418675u);  // FUN_00419c70
    wr32(m, ebp - 0x4u, 0x2u);
    edi = rd32(m, ebx + 0x10u);
    ecx = edi;
    ecx = ecx - rd32(m, ebp + 0x14u);
    eax = 0x2e8ba2e9u;
    { int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); }
    edx = uint32_t(int32_t(edx) >> 5);
    eax = edx;
    eax = (eax >> 31);
    eax = eax + edx;
    esi = 0x1u;
    esi = esi - eax;
    edx = (ebp - 0xc8u);
    LIFT_CALL(0x417c20u, 0x4186a7u);  // FUN_00417c20
    wr32(m, ebp - 0x4u, 0xffffffffu);
    wr32(m, ebx + 0x10u, rd32(m, ebx + 0x10u) + 0xb0u);
    ebx = rd32(m, ebx + 0x10u);
    ecx = (ebp - 0xc8u);
    esp -= 4; wr32(m, esp, ecx);
    edi = (ebx - 0xb0u);
    eax = rd32(m, ebp + 0x14u);
    LIFT_CALL(0x419ca0u, 0x4186cdu);  // FUN_00419ca0
    esp = esp + 0x4u;
    goto L_41876e;
L_418707:
    eax = rd32(m, ebp - 0xd4u);
    esp -= 4; wr32(m, esp, eax);
    ecx = (ebp - 0xc8u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x4187a0u, 0x41871au);  // FUN_004187a0
    esi = rd32(m, ebx + 0x10u);
    edi = (esi - 0xb0u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edx = esi;
    LIFT_CALL(0x419c70u, 0x41872cu);  // FUN_00419c70
    wr32(m, ebx + 0x10u, eax);
    wr8(m, ebp - 0xccu, 0x0u);
    edx = rd32(m, ebp - 0xccu);
    esp -= 4; wr32(m, esp, edx);
    eax = rd32(m, ebp - 0xccu);
    esp -= 4; wr32(m, esp, eax);
    ecx = rd32(m, ebp - 0xccu);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, esi);
    eax = edi;
    edi = rd32(m, ebp + 0x14u);
    LIFT_CALL(0x41afc0u, 0x418756u);  // FUN_0041afc0
    edx = (ebp - 0xc8u);
    esp -= 4; wr32(m, esp, edx);
    eax = rd32(m, ebp + 0x14u);
    edi = (eax + 0xb0u);
    LIFT_CALL(0x419ca0u, 0x41876bu);  // FUN_00419ca0
    esp = esp + 0x14u;
L_41876e:
    esi = (ebp - 0xc8u);
    LIFT_CALL(0x415320u, 0x418779u);  // FUN_00415320
L_418779:
    ecx = rd32(m, ebp - 0xcu);
    wr32(m, FS_BASE, ecx);
    ecx = rd32(m, esp); esp += 4;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, ebp - 0x14u);
    ecx = (ecx ^ ebp);
    LIFT_CALL(0x8f9efeu, 0x418791u);  // __security_check_cookie
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(16);
}

// 004187a0 Fn_004187a0
//   original FUN_004187a0; register inputs: ecx; returns with ret 8
REFLEX_PORT_FN(0x004187a0, Fn_004187a0, "fx", reflex_port::OUT_ECX) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    ecx = 0x20u;
    esi = ebp;
    edi = ebx;
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    eax = rd32(m, ebp + 0x80u);
    wr32(m, ebx + 0x80u, eax);
    ecx = rd32(m, ebp + 0x84u);
    edx = (ebx + 0x88u);
    edi = (ebp + 0x88u);
    esp -= 4; wr32(m, esp, edx);
    wr32(m, ebx + 0x84u, ecx);
    LIFT_CALL(0x4eee60u, 0x4187e2u);  // FUN_004eee60
    ebp = ebp + 0x98u;
    esp -= 4; wr32(m, esp, ebp);
    eax = (ebx + 0x98u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x418800u, 0x4187f5u);  // FUN_00418800
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = ebx;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 00418800 Fn_00418800
//   original FUN_00418800; register inputs: none; returns with ret 8
REFLEX_PORT_FN(0x00418800, Fn_00418800, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | (reflex_port::OUT_XMM0 << 0) | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_41883d = 0;
    uint32_t fB_41883d = 0;
    uint32_t fA_41885b = 0;
    uint32_t fB_41885b = 0;
    uint32_t fA_418863 = 0;
    uint32_t fB_418863 = 0;
    uint32_t fA_41888e = 0;
    uint32_t fB_41888e = 0;
    uint32_t fA_4188a9 = 0;
    uint32_t fB_4188a9 = 0;
    uint32_t fR_4188dd = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp -= 4; wr32(m, esp, 0xffffffffu);
    esp -= 4; wr32(m, esp, 0x902b40u);
    eax = rd32(m, FS_BASE);
    esp -= 4; wr32(m, esp, eax);
    esp = esp - 0x24u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ ebp);
    esp -= 4; wr32(m, esp, eax);
    eax = (ebp - 0xcu);
    wr32(m, FS_BASE, eax);
    wr32(m, ebp - 0x10u, esp);
    ebx = rd32(m, ebp + 0xcu);
    edi = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, 0x4u);
    LIFT_CALL(0x78fbe0u, 0x418838u);  // ReflexAlignedAlloc
    esp = esp + 0x4u;
    ecx = 0u;
    fA_41883d = eax;
    fB_41883d = ecx;
    if (fA_41883d == fB_41883d) goto L_418845;
    wr32(m, eax, edi);
    goto L_418847;
L_418845:
    eax = 0u;
L_418847:
    wr32(m, edi, eax);
    esi = rd32(m, ebx + 0x10u);
    esi = esi - rd32(m, ebx + 0xcu);
    esi = uint32_t(int32_t(esi) >> 2);
    wr32(m, edi + 0xcu, ecx);
    wr32(m, edi + 0x10u, ecx);
    wr32(m, edi + 0x14u, ecx);
    fA_41885b = esi;
    fB_41885b = ecx;
    if (fA_41885b == fB_41885b) goto L_4188f8;
    fA_418863 = esi;
    fB_418863 = 0x3fffffffu;
    if (fA_418863 <= fB_418863) goto L_418870;
    LIFT_CALL(0x851560u, 0x418870u);  // FUN_00851560
L_418870:
    ecx = esi;
    LIFT_CALL(0x42bf30u, 0x418877u);  // FUN_0042bf30
    wr32(m, edi + 0xcu, eax);
    wr32(m, edi + 0x10u, eax);
    eax = (eax + esi * 4);
    wr32(m, edi + 0x14u, eax);
    ecx = 0u;
    wr32(m, ebp - 0x4u, ecx);
    esi = rd32(m, ebx + 0x10u);
    wr32(m, ebp - 0x18u, ecx);
    fA_41888e = rd32(m, ebx + 0xcu);
    fB_41888e = esi;
    if (fA_41888e <= fB_41888e) goto L_41889b;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x418899u);
    ecx = 0u;
L_41889b:
    edx = rd32(m, ebx);
    wr32(m, ebp - 0x18u, edx);
    eax = rd32(m, ebx + 0xcu);
    wr32(m, ebp + 0xcu, eax);
    wr32(m, ebp - 0x20u, ecx);
    fA_4188a9 = eax;
    fB_4188a9 = rd32(m, ebx + 0x10u);
    if (fA_4188a9 <= fB_4188a9) goto L_4188b7;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4188b4u);
    eax = rd32(m, ebp + 0xcu);
L_4188b7:
    ebx = rd32(m, ebx);
    wr32(m, ebp - 0x20u, ebx);
    edx = rd32(m, edi + 0xcu);
    ecx = rd32(m, ebp - 0x18u);
    wr32(m, ebp - 0x28u, ecx);
    wr32(m, ebp - 0x24u, esi);
    wr32(m, ebp - 0x30u, ebx);
    wr32(m, ebp - 0x2cu, eax);
    esi = esi - eax;
    esi = uint32_t(int32_t(esi) >> 2);
    ecx = (esi * 4);
    ebx = (ecx + edx);
    fR_4188dd = (esi & esi);
    if ((false || (fR_4188dd == 0))) goto L_4188ee;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(rd32(m, 0x916200u) /* 8.286124448082365e-39 */, 0x4188ebu);
    esp = esp + 0x10u;
L_4188ee:
    wr32(m, edi + 0x10u, ebx);
    wr32(m, ebp - 0x4u, 0xffffffffu);
L_4188f8:
    eax = edi;
    ecx = rd32(m, ebp - 0xcu);
    wr32(m, FS_BASE, ecx);
    ecx = rd32(m, esp); esp += 4;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 00418e00 Fn_00418e00
//   original FUN_00418e00; register inputs: esi; returns with ret 4
REFLEX_PORT_FN(0x00418e00, Fn_00418e00, "fx", reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fR_418e0a = 0;
    uint32_t fR_418e12 = 0;
    uint32_t fA_418e2b = 0;
    uint32_t fB_418e2b = 0;
    uint32_t fR_418e30 = 0;
    uint32_t fA_418e3a = 0;
    uint32_t fB_418e3a = 0;
    eax = rd32(m, esi);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    esp -= 4; wr32(m, esp, edi);
    fR_418e0a = (eax & eax);
    if (!(fR_418e0a == 0)) goto L_418e16;
    LIFT_CALL(ebx, 0x418e10u);
    eax = rd32(m, esi);
    fR_418e12 = (eax & eax);
    if ((fR_418e12 == 0)) goto L_418e1a;
L_418e16:
    edx = rd32(m, eax);
    goto L_418e1c;
L_418e1a:
    edx = 0u;
L_418e1c:
    edi = rd32(m, esp + 0xcu);
    ecx = rd32(m, esi + 0x4u);
    edi = uint32_t(int32_t(edi) * int32_t(0xb0u));
    ecx = ecx + edi;
    fA_418e2b = ecx;
    fB_418e2b = rd32(m, edx + 0x10u);
    if (fA_418e2b > fB_418e2b) goto L_418e3f;
    fR_418e30 = (eax & eax);
    if ((fR_418e30 == 0)) goto L_418e38;
    eax = rd32(m, eax);
    goto L_418e3a;
L_418e38:
    eax = 0u;
L_418e3a:
    fA_418e3a = ecx;
    fB_418e3a = rd32(m, eax + 0xcu);
    if (fA_418e3a >= fB_418e3a) goto L_418e41;
L_418e3f:
    LIFT_CALL(ebx, 0x418e41u);
L_418e41:
    wr32(m, esi + 0x4u, rd32(m, esi + 0x4u) + edi);
    edi = rd32(m, esp); esp += 4;
    eax = esi;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00418e50 Fn_00418e50
//   original FUN_00418e50; register inputs: eax; returns with ret 4
REFLEX_PORT_FN(0x00418e50, Fn_00418e50, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX | (reflex_port::OUT_XMM0 << 0) | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fR_418e7a = 0;
    uint32_t fA_418e7e = 0;
    uint32_t fB_418e7e = 0;
    uint32_t fA_418e88 = 0;
    uint32_t fB_418e88 = 0;
    uint32_t fR_418e8c = 0;
    uint32_t fA_418e96 = 0;
    uint32_t fB_418e96 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0xcu;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, ebx + 0x44u);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0x14u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x419ea0u, 0x418e6du);  // FUN_00419ea0
    esi = rd32(m, eax);
    edi = rd32(m, eax + 0x4u);
    eax = rd32(m, ebx + 0x44u);
    ebx = rd32(m, eax + 0x18u);
    eax = rd32(m, eax);
    fR_418e7a = (esi & esi);
    if ((fR_418e7a == 0)) goto L_418e82;
    fA_418e7e = esi;
    fB_418e7e = eax;
    if (fA_418e7e == fB_418e7e) goto L_418e88;
L_418e82:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x418e88u);
L_418e88:
    fA_418e88 = edi;
    fB_418e88 = ebx;
    if (fA_418e88 == fB_418e88) goto L_418eb1;
    fR_418e8c = (esi & esi);
    if (!(fR_418e8c == 0)) goto L_418ead;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x418e96u);
L_418e96:
    fA_418e96 = edi;
    fB_418e96 = rd32(m, esi + 0x18u);
    if (fA_418e96 != fB_418e96) goto L_418ea1;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x418ea1u);
L_418ea1:
    eax = (edi + 0x10u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_418ead:
    esi = rd32(m, esi);
    goto L_418e96;
L_418eb1:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    eax = 0xae1c1cu;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00418ec0 Fn_00418ec0
//   original FUN_00418ec0; register inputs: eax; returns with ret 4
REFLEX_PORT_FN(0x00418ec0, Fn_00418ec0, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX | (reflex_port::OUT_XMM0 << 0) | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fR_418eea = 0;
    uint32_t fA_418eee = 0;
    uint32_t fB_418eee = 0;
    uint32_t fA_418ef8 = 0;
    uint32_t fB_418ef8 = 0;
    uint32_t fR_418efc = 0;
    uint32_t fA_418f06 = 0;
    uint32_t fB_418f06 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0xcu;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, ebx + 0x48u);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0x14u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x763270u, 0x418eddu);  // FUN_00763270
    esi = rd32(m, eax);
    edi = rd32(m, eax + 0x4u);
    eax = rd32(m, ebx + 0x48u);
    ebx = rd32(m, eax + 0x18u);
    eax = rd32(m, eax);
    fR_418eea = (esi & esi);
    if ((fR_418eea == 0)) goto L_418ef2;
    fA_418eee = esi;
    fB_418eee = eax;
    if (fA_418eee == fB_418eee) goto L_418ef8;
L_418ef2:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x418ef8u);
L_418ef8:
    fA_418ef8 = edi;
    fB_418ef8 = ebx;
    if (fA_418ef8 == fB_418ef8) goto L_418f21;
    fR_418efc = (esi & esi);
    if (!(fR_418efc == 0)) goto L_418f1d;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x418f06u);
L_418f06:
    fA_418f06 = edi;
    fB_418f06 = rd32(m, esi + 0x18u);
    if (fA_418f06 != fB_418f06) goto L_418f11;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x418f11u);
L_418f11:
    eax = (edi + 0x10u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_418f1d:
    esi = rd32(m, esi);
    goto L_418f06;
L_418f21:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    eax = 0x9c9ff4u;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00419820 Fn_00419820
//   original FUN_00419820; register inputs: ecx edx; returns with ret 4
REFLEX_PORT_FN(0x00419820, Fn_00419820, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    esp = esp - 0x10u;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp + 0xcu, eax);
    eax = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, 0xa89bc8u);
    esp -= 4; wr32(m, esp, edi);
    wr32(m, esp + 0x10u, ecx);
    esp -= 4; wr32(m, esp, 0x9e1a7cu);
    ecx = 0x9cbb7cu;
    edi = edx;
    wr32(m, esp + 0x10u, eax);
    LIFT_CALL(rd32(m, 0x916388u) /* 8.287789190657982e-39 */, 0x419854u);
    esp -= 4; wr32(m, esp, 0x8u);
    edx = (esp + 0x10u);
    esp -= 4; wr32(m, esp, edx);
    eax = esi;
    LIFT_CALL(0x4020c0u, 0x419862u);  // FUN_004020c0
    ecx = rd32(m, esp + 0x14u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x41986fu);  // __security_check_cookie
    esp = esp + 0x10u;
    LIFT_RET(4);
}

// 004199a0 Fn_004199a0
//   original FUN_004199a0; register inputs: ecx eax; returns with ret 4
REFLEX_PORT_FN(0x004199a0, Fn_004199a0, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, eax);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, eax + 0x8u);
    eax = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    eax = esi;
    LIFT_CALL(0x402160u, 0x4199b5u);  // Table_ColumnIndex
    edi = eax;
    esp -= 4; wr32(m, esp, ebx);
    eax = esi;
    LIFT_CALL(0x41aaf0u, 0x4199bfu);  // FUN_0041aaf0
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00419a70 Fn_00419a70
//   original FUN_00419a70; register inputs: ecx eax esi; returns with ret 4
REFLEX_PORT_FN(0x00419a70, Fn_00419a70, "fx", reflex_port::OUT_EAX) {
    LIFT_ENTRY();
    uint32_t fA_419aa6 = 0;
    uint32_t fB_419aa6 = 0;
    esp -= 4; wr32(m, esp, ebx);
    wr32(m, esi + 0x80u, 0xffffffffu);
    wr32(m, esi + 0x84u, eax);
    eax = rd32(m, ecx);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, 0x4u);
    wr32(m, esi + 0x88u, 0x1u);
    wr32(m, esi + 0x90u, eax);
    edi = (esi + 0x98u);
    LIFT_CALL(0x78fbe0u, 0x419aa1u);  // ReflexAlignedAlloc
    ebx = 0u;
    esp = esp + 0x4u;
    fA_419aa6 = eax;
    fB_419aa6 = ebx;
    if (fA_419aa6 == fB_419aa6) goto L_419aae;
    wr32(m, eax, edi);
    goto L_419ab0;
L_419aae:
    eax = 0u;
L_419ab0:
    edx = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, 0x7fu);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, esi);
    wr32(m, edi, eax);
    wr32(m, edi + 0xcu, ebx);
    wr32(m, edi + 0x10u, ebx);
    wr32(m, edi + 0x14u, ebx);
    LIFT_CALL(rd32(m, 0x916260u) /* 8.286626112932593e-39 */, 0x419ac9u);
    esp = esp + 0xcu;
    edi = rd32(m, esp); esp += 4;
    wr8(m, esi + 0x7fu, (ebx & 0xffu));
    eax = esi;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00419ae0 Fn_00419ae0
//   original FUN_00419ae0; register inputs: ecx eax esi; returns with ret 4
REFLEX_PORT_FN(0x00419ae0, Fn_00419ae0, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX) {
    LIFT_ENTRY();
    uint32_t fA_419af5 = 0;
    uint32_t fB_419af5 = 0;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    eax = esi;
    LIFT_CALL(0x402160u, 0x419aeau);  // Table_ColumnIndex
    ecx = rd32(m, esi + 0x14u);
    ecx = ecx - rd32(m, esi + 0x10u);
    edi = eax;
    ecx = uint32_t(int32_t(ecx) >> 2);
    fA_419af5 = edi;
    fB_419af5 = ecx;
    if (fA_419af5 < fB_419af5) goto L_419aff;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x419affu);
L_419aff:
    edx = rd32(m, esi + 0x10u);
    eax = (edx + edi * 4);
    eax = rd32(m, eax);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, esp + 0x10u);
    LIFT_CALL(0x418ec0u, 0x419b11u);  // FUN_00418ec0
    edi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00419c70 Fn_00419c70
//   original FUN_00419c70; register inputs: ecx edx; returns with ret 8
REFLEX_PORT_FN(0x00419c70, Fn_00419c70, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, ecx);
    ecx = rd32(m, esp + 0xcu);
    wr8(m, esp, 0x0u);
    eax = rd32(m, esp);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x41b090u, 0x419c8eu);  // FUN_0041b090
    esp = esp + 0x18u;
    LIFT_RET(8);
}

// 00419cd0 Fn_00419cd0
//   original FUN_00419cd0; register inputs: ecx eax; returns with ret 0
REFLEX_PORT_FN(0x00419cd0, Fn_00419cd0, "fx", reflex_port::OUT_EAX) {
    LIFT_ENTRY();
    uint32_t fR_419cd3 = 0;
    uint32_t fA_419cf3 = 0;
    uint32_t fB_419cf3 = 0;
    esp = esp - 0x10u;
    fR_419cd3 = (ecx & ecx);
    if (!(false || (fR_419cd3 == 0))) goto L_419cec;
    ecx = 0u;
L_419cd9:
    ecx = uint32_t(int32_t(ecx) * int32_t(0xb0u));
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x78fbe0u, 0x419ce5u);  // ReflexAlignedAlloc
    esp = esp + 0x4u;
    esp = esp + 0x10u;
    LIFT_RET(0);
L_419cec:
    eax = (eax | 0xffffffffu);
    edx = 0u;
    { uint32_t d_ = ecx; udiv32(eax, edx, d_); }
    fA_419cf3 = eax;
    fB_419cf3 = 0xb0u;
    if (fA_419cf3 >= fB_419cf3) goto L_419cd9;
    eax = (esp);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0x8u);
    wr32(m, esp + 0x4u, 0x0u);
    LIFT_CALL(rd32(m, 0x916214u) /* 8.28627859091344e-39 */, 0x419d10u);
    esp -= 4; wr32(m, esp, 0x99bcbcu);
    ecx = (esp + 0x8u);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp + 0xcu, 0x93707cu);
    LIFT_CALL(0x8fa07cu, 0x419d27u);  // MSVCR90.DLL::_CxxThrowException
    throw std::runtime_error("int3 reached");
}

// 00419ea0 Fn_00419ea0
//   original FUN_00419ea0; register inputs: edi; returns with ret 8
REFLEX_PORT_FN(0x00419ea0, Fn_00419ea0, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX | (reflex_port::OUT_XMM0 << 0)) {
    LIFT_ENTRY();
    uint32_t fA_419ea9 = 0;
    uint32_t fB_419ea9 = 0;
    uint32_t fA_419ec0 = 0;
    uint32_t fB_419ec0 = 0;
    uint32_t fA_419ece = 0;
    uint32_t fB_419ece = 0;
    uint32_t fR_419ee1 = 0;
    uint32_t fA_419ee5 = 0;
    uint32_t fB_419ee5 = 0;
    uint32_t fA_419eef = 0;
    uint32_t fB_419eef = 0;
    uint32_t fA_419ef6 = 0;
    uint32_t fB_419ef6 = 0;
    ecx = rd32(m, edi + 0x18u);
    eax = rd32(m, ecx + 0x4u);
    esp = esp - 0x18u;
    fA_419ea9 = rd8(m, eax + 0x39u);
    fB_419ea9 = 0x0u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x28u);
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    if (fA_419ea9 != fB_419ea9) goto L_419ed4;
    ecx = rd32(m, ebp);
    goto L_419ec0;
L_419ec0:
    fA_419ec0 = rd32(m, eax + 0xcu);
    fB_419ec0 = ecx;
    if (int32_t(sext(fA_419ec0, 4)) >= int32_t(sext(fB_419ec0, 4))) goto L_419eca;
    eax = rd32(m, eax + 0x8u);
    goto L_419ece;
L_419eca:
    esi = eax;
    eax = rd32(m, eax);
L_419ece:
    fA_419ece = rd8(m, eax + 0x39u);
    fB_419ece = 0x0u;
    if (fA_419ece == fB_419ece) goto L_419ec0;
L_419ed4:
    eax = rd32(m, edi);
    ebx = rd32(m, edi + 0x18u);
    wr32(m, esp + 0x14u, esi);
    wr32(m, esp + 0x10u, eax);
    fR_419ee1 = (eax & eax);
    if ((fR_419ee1 == 0)) goto L_419ee9;
    fA_419ee5 = eax;
    fB_419ee5 = eax;
    if (fA_419ee5 == fB_419ee5) goto L_419eef;
L_419ee9:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x419eefu);
L_419eef:
    fA_419eef = esi;
    fB_419eef = ebx;
    if (fA_419eef == fB_419eef) goto L_419f01;
    eax = rd32(m, ebp);
    fA_419ef6 = eax;
    fB_419ef6 = rd32(m, esi + 0xcu);
    if (int32_t(sext(fA_419ef6, 4)) < int32_t(sext(fB_419ef6, 4))) goto L_419f01;
    eax = (esp + 0x10u);
    goto L_419f12;
L_419f01:
    ecx = rd32(m, edi + 0x18u);
    edx = rd32(m, edi);
    wr32(m, esp + 0x1cu, ecx);
    wr32(m, esp + 0x18u, edx);
    eax = (esp + 0x18u);
L_419f12:
    edx = rd32(m, eax);
    ecx = rd32(m, esp + 0x28u);
    eax = rd32(m, eax + 0x4u);
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    wr32(m, ecx + 0x4u, eax);
    wr32(m, ecx, edx);
    eax = ecx;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(8);
}

// 0041aaf0 Fn_0041aaf0
//   original FUN_0041aaf0; register inputs: eax edi; returns with ret 4
REFLEX_PORT_FN(0x0041aaf0, Fn_0041aaf0, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX | (reflex_port::OUT_XMM0 << 0) | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_41ab08 = 0;
    uint32_t fB_41ab08 = 0;
    uint32_t fA_41ab2b = 0;
    uint32_t fB_41ab2b = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    eax = rd32(m, esi + 0x14u);
    eax = eax - rd32(m, esi + 0x10u);
    eax = uint32_t(int32_t(eax) >> 2);
    fA_41ab08 = edi;
    fB_41ab08 = eax;
    if (fA_41ab08 < fB_41ab08) goto L_41ab0e;
    LIFT_CALL(ebp, 0x41ab0eu);
L_41ab0e:
    ecx = rd32(m, esi + 0x10u);
    esi = rd32(m, ecx + edi * 4);
    esp -= 4; wr32(m, esp, 0xae1c44u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x41ab1fu);
    esi = rd32(m, esi + 0x40u);
    edx = rd32(m, esi + 0x10u);
    edx = edx - rd32(m, esi + 0xcu);
    edx = uint32_t(int32_t(edx) >> 2);
    fA_41ab2b = ebx;
    fB_41ab2b = edx;
    if (fA_41ab2b < fB_41ab2b) goto L_41ab31;
    LIFT_CALL(ebp, 0x41ab31u);
L_41ab31:
    eax = rd32(m, esi + 0xcu);
    esp -= 4; wr32(m, esp, 0xae1c44u);
    esi = (eax + ebx * 4);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x41ab42u);
    eax = esi;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 0041abd0 Fn_0041abd0
//   original FUN_0041abd0; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x0041abd0, Fn_0041abd0, "fx", reflex_port::OUT_ECX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fR_41ac10 = 0;
    uint32_t fR_41ac14 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp -= 4; wr32(m, esp, 0xffffffffu);
    esp -= 4; wr32(m, esp, 0x903150u);
    eax = rd32(m, FS_BASE);
    esp -= 4; wr32(m, esp, eax);
    esp = esp - 0x8u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ ebp);
    esp -= 4; wr32(m, esp, eax);
    eax = (ebp - 0xcu);
    wr32(m, FS_BASE, eax);
    wr32(m, ebp - 0x10u, esp);
    ebx = rd32(m, ebp + 0x10u);
    edi = rd32(m, ebp + 0xcu);
    esi = rd32(m, ebp + 0x8u);
    wr32(m, ebp - 0x14u, esi);
    wr32(m, ebp - 0x4u, 0x0u);
    edi = edi;
L_41ac10:
    fR_41ac10 = (edi & edi);
    if ((false || (fR_41ac10 == 0))) goto L_41ac2e;
    fR_41ac14 = (esi & esi);
    if ((fR_41ac14 == 0)) goto L_41ac1f;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x4187a0u, 0x41ac1fu);  // FUN_004187a0
L_41ac1f:
    edi = (edi - 1u);
    wr32(m, ebp + 0xcu, edi);
    esi = esi + 0xb0u;
    wr32(m, ebp + 0x8u, esi);
    goto L_41ac10;
L_41ac2e:
    wr32(m, ebp - 0x4u, 0xffffffffu);
    ecx = rd32(m, ebp - 0xcu);
    wr32(m, FS_BASE, ecx);
    ecx = rd32(m, esp); esp += 4;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0041ac70 Fn_0041ac70
//   original FUN_0041ac70; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x0041ac70, Fn_0041ac70, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_41ac75 = 0;
    uint32_t fB_41ac75 = 0;
    uint32_t fA_41ac92 = 0;
    uint32_t fB_41ac92 = 0;
    uint32_t fA_41acb6 = 0;
    uint32_t fB_41acb6 = 0;
    uint32_t fA_41acbb = 0;
    uint32_t fB_41acbb = 0;
    uint32_t fA_41acca = 0;
    uint32_t fB_41acca = 0;
    uint32_t fA_41acf7 = 0;
    uint32_t fB_41acf7 = 0;
    uint32_t fA_41ad1b = 0;
    uint32_t fB_41ad1b = 0;
    uint32_t fA_41ad34 = 0;
    uint32_t fB_41ad34 = 0;
    uint32_t fA_41ad5a = 0;
    uint32_t fB_41ad5a = 0;
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esp + 0x8u);
    fA_41ac75 = edi;
    fB_41ac75 = rd32(m, esp + 0xcu);
    if (fA_41ac75 == fB_41ac75) goto L_41ad67;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, 0x916350u) /* 8.287536956934404e-39 */;
    edi = edi + 0xa4u;
    esp -= 4; wr32(m, esp, esi);
    ebx = 0u;
L_41ac90:
    eax = rd32(m, edi);
    fA_41ac92 = eax;
    fB_41ac92 = ebx;
    if (fA_41ac92 == fB_41ac92) goto L_41ac9f;
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x78fc50u, 0x41ac9cu);  // ReflexAlignedFree
    esp = esp + 0x4u;
L_41ac9f:
    eax = rd32(m, edi - 0xcu);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, edi, ebx);
    wr32(m, edi + 0x4u, ebx);
    wr32(m, edi + 0x8u, ebx);
    LIFT_CALL(0x78fc50u, 0x41acb0u);  // ReflexAlignedFree
    eax = rd32(m, edi - 0x1cu);
    esp = esp + 0x4u;
    fA_41acb6 = eax;
    fB_41acb6 = 0x4u;
    eax = fA_41acb6 - fB_41acb6;
    if (fA_41acb6 == fB_41acb6) goto L_41acf4;
    fA_41acbb = eax;
    fB_41acbb = 0x1u;
    eax = fA_41acbb - fB_41acbb;
    if (fA_41acbb != fB_41acbb) goto L_41ad47;
    esi = rd32(m, edi - 0x14u);
    eax = rd32(m, esi + 0xcu);
    fA_41acca = eax;
    fB_41acca = ebx;
    if (fA_41acca == fB_41acca) goto L_41acde;
    ecx = rd32(m, esi + 0x14u);
    ecx = ecx - eax;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esi + 0x8u);
    LIFT_CALL(rd32(m, 0x916198u) /* 8.281267547605015e-39 */, 0x41acdeu);
L_41acde:
    edx = rd32(m, esi);
    esp -= 4; wr32(m, esp, edx);
    wr32(m, esi + 0xcu, ebx);
    wr32(m, esi + 0x10u, ebx);
    wr32(m, esi + 0x14u, ebx);
    LIFT_CALL(0x78fc50u, 0x41acefu);  // ReflexAlignedFree
    esp = esp + 0x4u;
    goto L_41ad13;
L_41acf4:
    esi = rd32(m, edi - 0x14u);
    fA_41acf7 = rd32(m, esi + 0x18u);
    fB_41acf7 = 0x10u;
    if (fA_41acf7 < fB_41acf7) goto L_41ad06;
    eax = rd32(m, esi + 0x4u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x4f3320u, 0x41ad06u);  // FUN_004f3320
L_41ad06:
    wr32(m, esi + 0x18u, 0xfu);
    wr32(m, esi + 0x14u, ebx);
    wr8(m, esi + 0x4u, (ebx & 0xffu));
L_41ad13:
    esi = rd32(m, edi - 0x14u);
    LIFT_CALL(0x4dc0f0u, 0x41ad1bu);  // FUN_004dc0f0
    fA_41ad1b = rd8(m, 0xda67d8u);
    fB_41ad1b = (ebx & 0xffu);
    if (fA_41ad1b == fB_41ad1b) goto L_41ad2e;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x41ad2eu);
L_41ad2e:
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(ebp, 0x41ad31u);
    esp = esp + 0x4u;
    fA_41ad34 = rd8(m, 0xda67d8u);
    fB_41ad34 = (ebx & 0xffu);
    if (fA_41ad34 == fB_41ad34) goto L_41ad47;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x41ad47u);
L_41ad47:
    eax = rd32(m, esp + 0x14u);
    eax = eax + 0xb0u;
    edi = edi + 0xb0u;
    wr32(m, esp + 0x14u, eax);
    fA_41ad5a = eax;
    fB_41ad5a = rd32(m, esp + 0x18u);
    if (fA_41ad5a != fB_41ad5a) goto L_41ac90;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
L_41ad67:
    edi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0041ad70 Fn_0041ad70
//   original FUN_0041ad70; register inputs: eax edx; returns with ret 0
REFLEX_PORT_FN(0x0041ad70, Fn_0041ad70, "fx", 0) {
    LIFT_ENTRY();

    ecx = rd32(m, eax);
    edx = rd32(m, edx);
    wr32(m, ecx, edx);
    wr32(m, eax, rd32(m, eax) + 0x4u);
    LIFT_RET(0);
}

// 0041b090 Fn_0041b090
//   original FUN_0041b090; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x0041b090, Fn_0041b090, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();
    uint32_t fA_41b0d0 = 0;
    uint32_t fB_41b0d0 = 0;
    uint32_t fR_41b0d4 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp -= 4; wr32(m, esp, 0xffffffffu);
    esp -= 4; wr32(m, esp, 0x903220u);
    eax = rd32(m, FS_BASE);
    esp -= 4; wr32(m, esp, eax);
    esp = esp - 0x8u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ ebp);
    esp -= 4; wr32(m, esp, eax);
    eax = (ebp - 0xcu);
    wr32(m, FS_BASE, eax);
    wr32(m, ebp - 0x10u, esp);
    esi = rd32(m, ebp + 0x10u);
    ebx = rd32(m, ebp + 0xcu);
    edi = rd32(m, ebp + 0x8u);
    wr32(m, ebp - 0x14u, esi);
    wr32(m, ebp - 0x4u, 0x0u);
    edi = edi;
L_41b0d0:
    fA_41b0d0 = edi;
    fB_41b0d0 = ebx;
    if (fA_41b0d0 == fB_41b0d0) goto L_41b0f3;
    fR_41b0d4 = (esi & esi);
    if ((fR_41b0d4 == 0)) goto L_41b0df;
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x4187a0u, 0x41b0dfu);  // FUN_004187a0
L_41b0df:
    esi = esi + 0xb0u;
    wr32(m, ebp + 0x10u, esi);
    edi = edi + 0xb0u;
    wr32(m, ebp + 0x8u, edi);
    goto L_41b0d0;
L_41b0f3:
    wr32(m, ebp - 0x4u, 0xffffffffu);
    eax = esi;
    ecx = rd32(m, ebp - 0xcu);
    wr32(m, FS_BASE, ecx);
    ecx = rd32(m, esp); esp += 4;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0041b230 LevelInfoEntity__vf22
//   original LevelInfoEntity::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x0041b230, LevelInfoEntity__vf22, "fx", reflex_port::OUT_EAX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    eax = 0x107au;
    LIFT_RET(0);
}

// 0041b410 Fn_0041b410
//   original FUN_0041b410; register inputs: none; returns with ret 4
REFLEX_PORT_FN(0x0041b410, Fn_0041b410, "fx", reflex_port::OUT_ECX) {
    LIFT_ENTRY();
    uint32_t fA_41b423 = 0;
    uint32_t fB_41b423 = 0;
    uint32_t fA_41b455 = 0;
    uint32_t fB_41b455 = 0;
    uint32_t fA_41b463 = 0;
    uint32_t fB_41b463 = 0;
    uint32_t fA_41b487 = 0;
    uint32_t fB_41b487 = 0;
    esp = esp - 0x40u;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp + 0x3cu, eax);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x48u);
    fA_41b423 = rd32(m, ebx + 0x54u);
    fB_41b423 = 0x0u;
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esi = (ebx + 0x54u);
    esp -= 4; wr32(m, esp, edi);
    if (fA_41b423 == fB_41b423) goto L_41b48c;
    ebp = rd32(m, 0x91600cu) /* 8.29485173491818e-39 */;
    edi = (ebx + 0x3cu);
L_41b438:
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(ebp, 0x41b43bu);
    eax = (esp + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x41bd10u, 0x41b445u);  // FUN_0041bd10
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x41b44cu);
    edx = rd32(m, esp + 0x34u);
    eax = 0x10u;
    fA_41b455 = rd32(m, esp + 0x48u);
    fB_41b455 = eax;
    if (fA_41b455 >= fB_41b455) goto L_41b45f;
    edx = (esp + 0x34u);
L_41b45f:
    ecx = rd32(m, esp + 0x18u);
    fA_41b463 = rd32(m, esp + 0x2cu);
    fB_41b463 = eax;
    if (fA_41b463 >= fB_41b463) goto L_41b46d;
    ecx = (esp + 0x18u);
L_41b46d:
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x41b4b0u, 0x41b473u);  // FUN_0041b4b0
    ecx = (esp + 0x30u);
    LIFT_CALL(rd32(m, 0x9161b4u) /* 8.28055008279128e-39 */, 0x41b47du);
    ecx = (esp + 0x14u);
    LIFT_CALL(rd32(m, 0x9161b4u) /* 8.28055008279128e-39 */, 0x41b487u);
    fA_41b487 = rd32(m, esi);
    fB_41b487 = 0x0u;
    if (fA_41b487 != fB_41b487) goto L_41b438;
L_41b48c:
    ecx = rd32(m, esp + 0x4cu);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x41b49bu);  // __security_check_cookie
    esp = esp + 0x40u;
    LIFT_RET(4);
}

// 0041c4b0 Fn_0041c4b0
//   original FUN_0041c4b0; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x0041c4b0, Fn_0041c4b0, "fx", reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX | reflex_port::OUT_FPU) {
    LIFT_ENTRY();

    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x4u)));
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x8u)));
    LIFT_CALL(0x9009c0u, 0x41c4bdu);  // MSVCR90.DLL::_CIatan2
    wr32(m, esp + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x4u)));
    LIFT_RET(0);
}

// 0041c580 Fn_0041c580
//   original FUN_0041c580; register inputs: xmm0; returns with ret 0
REFLEX_PORT_FN(0x0041c580, Fn_0041c580, "fx", (reflex_port::OUT_XMM0 << 0)) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x20u;
    x1 = ldx(m, esp + 0x10u);
    stx(m, esp, x0);
    { uint64_t v_ = rd64(m, esp); if (!m.mmx_active && !m.fpu.empty()) throw std::runtime_error("MMX with a live x87 stack"); m.mmx_active = true; m.mmx[0] = v_; }
    x0 = ldx(m, esp + 0x10u);
    if (!m.mmx_active) throw std::runtime_error("MMX read before MMX write");
    { uint64_t v_ = m.mmx[0]; x0.lo = uint64_t(fb(float(int32_t(uint32_t(v_))))) | (uint64_t(fb(float(int32_t(uint32_t(v_ >> 32))))) << 32); }
    { uint64_t v_ = rd64(m, esp + 0x8u); if (!m.mmx_active && !m.fpu.empty()) throw std::runtime_error("MMX with a live x87 stack"); m.mmx_active = true; m.mmx[0] = v_; }
    if (!m.mmx_active) throw std::runtime_error("MMX read before MMX write");
    { uint64_t v_ = m.mmx[0]; x1.lo = uint64_t(fb(float(int32_t(uint32_t(v_))))) | (uint64_t(fb(float(int32_t(uint32_t(v_ >> 32))))) << 32); }
    x0 = shufps(x0, x1, 0x44);
    x1 = movss_m(rd32(m, ebp + 0x8u));
    x1 = shufps(x1, x1, 0x0);
    x0 = divps(x0, x1);
    if (m.mmx_active) { m.fpu.clear(); m.mmx_active = false; }
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0041c5c0 Fn_0041c5c0
//   original FUN_0041c5c0; register inputs: xmm0 xmm1; returns with ret 0
REFLEX_PORT_FN(0x0041c5c0, Fn_0041c5c0, "fx", reflex_port::OUT_ECX | (reflex_port::OUT_XMM0 << 0)) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x20u;
    stx(m, esp, x1);
    stx(m, esp + 0x10u, x0);
    x0 = movss_m(rd32(m, esp + 0x4u));
    x1 = movss_m(rd32(m, esp + 0x14u));
    x2 = movss_m(rd32(m, esp + 0x10u));
    x1 = cvtps2pd(x1.lo);
    x0 = cvtps2pd(x0.lo);
    x0 = mulsd(x0, x1.lo);
    x1 = movss_m(rd32(m, esp));
    x1 = cvtps2pd(x1.lo);
    x2 = cvtps2pd(x2.lo);
    x1 = mulsd(x1, x2.lo);
    x2 = movss_m(rd32(m, esp + 0x18u));
    x0 = addsd(x0, x1.lo);
    x1 = movss_m(rd32(m, esp + 0x8u));
    x1 = cvtps2pd(x1.lo);
    x2 = cvtps2pd(x2.lo);
    x1 = mulsd(x1, x2.lo);
    x0 = addsd(x0, x1.lo);
    x0 = cvtpd2ps(x0);
    wr32(m, esp, x0.l(0));
    wr32(m, esp + 0x4u, x0.l(0));
    wr32(m, esp + 0x8u, x0.l(0));
    wr32(m, esp + 0xcu, x0.l(0));
    x0 = ldx(m, esp);
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}
