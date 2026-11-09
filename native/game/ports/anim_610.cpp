// Character / object animation ports (group "anim"), transliterated instruction by instruction from MXReflex.exe by the
// anim lifter (see native/game/notes/anim.md). Each function keeps the original's operations, order and memory traffic.
#include <stdexcept>
#include <utility>
#include "lift_rt.h"

// 00612850 Fn_00612850
//   original FUN_00612850; 0 instructions per frame (plan); register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x00612850, Fn_00612850, "anim", LIFT_OUTS) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = esp - 0x18u;
    esp -= 4; wr32(m, esp, esi);
    wr32(m, ebp - 0x18u, ecx);
    esp -= 4; wr32(m, esp, ecx);
    eax = esp;
    LIFT_CALL(0x7e3be0u, 0x612862u);  // rtti_shared_vf_007e3be0
    esi = rd32(m, ebp - 0x18u);
    LIFT_CALL(0x594080u, 0x61286au);  // FUN_00594080
    LIFT_CALL(0x842df0u, 0x61286fu);  // FUN_00842df0
    ecx = rd32(m, ebp - 0x18u);
    wr32(m, ecx + 0x14u, eax);
    edx = rd32(m, ebp - 0x18u);
    wr32(m, edx + 0x18u, 0x0u);
    eax = rd32(m, ebp - 0x18u);
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00612890 Fn_00612890
//   original FUN_00612890; 1 instructions per frame (plan); register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x00612890, Fn_00612890, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_6128a7 = 0;
    uint32_t fB_6128a7 = 0;
    uint32_t fR_6128b6 = 0;
    uint32_t fA_6128e4 = 0;
    uint32_t fB_6128e4 = 0;
    uint32_t fA_61291b = 0;
    uint32_t fB_61291b = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = esp - 0x104u;
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    wr32(m, ebp - 0x104u, ecx);
    eax = rd32(m, ebp - 0x104u);
    fA_6128a7 = eax;
    fB_6128a7 = rd32(m, ebp + 0x8u);
    if (fA_6128a7 != fB_6128a7) goto L_6128b1;
    goto L_6129ea;
L_6128b1:
    ecx = 0x1u;
    fR_6128b6 = (ecx & ecx);
    if ((fR_6128b6 == 0)) goto L_61293f;
    esi = rd32(m, ebp + 0x8u);
    edi = rd32(m, ebp - 0x104u);
    LIFT_CALL(0x5f3520u, 0x6128ccu);  // FUN_005f3520
    edx = rd32(m, ebp + 0x8u);
    edx = edx + 0x14u;
    wr32(m, ebp - 0x54u, edx);
    eax = rd32(m, ebp - 0x104u);
    eax = eax + 0x14u;
    wr32(m, ebp - 0x50u, eax);
    ecx = rd32(m, ebp - 0x50u);
    fA_6128e4 = ecx;
    fB_6128e4 = rd32(m, ebp - 0x54u);
    if (fA_6128e4 == fB_6128e4) goto L_612903;
    edx = rd32(m, ebp - 0x50u);
    eax = rd32(m, edx);
    wr32(m, ebp - 0x4cu, eax);
    ecx = rd32(m, ebp - 0x50u);
    edx = rd32(m, ebp - 0x54u);
    eax = rd32(m, edx);
    wr32(m, ecx, eax);
    ecx = rd32(m, ebp - 0x54u);
    edx = rd32(m, ebp - 0x4cu);
    wr32(m, ecx, edx);
L_612903:
    eax = rd32(m, ebp + 0x8u);
    eax = eax + 0x18u;
    wr32(m, ebp - 0x60u, eax);
    ecx = rd32(m, ebp - 0x104u);
    ecx = ecx + 0x18u;
    wr32(m, ebp - 0x5cu, ecx);
    edx = rd32(m, ebp - 0x5cu);
    fA_61291b = edx;
    fB_61291b = rd32(m, ebp - 0x60u);
    if (fA_61291b == fB_61291b) goto L_61293a;
    eax = rd32(m, ebp - 0x5cu);
    ecx = rd32(m, eax);
    wr32(m, ebp - 0x58u, ecx);
    edx = rd32(m, ebp - 0x5cu);
    eax = rd32(m, ebp - 0x60u);
    ecx = rd32(m, eax);
    wr32(m, edx, ecx);
    edx = rd32(m, ebp - 0x60u);
    eax = rd32(m, ebp - 0x58u);
    wr32(m, edx, eax);
L_61293a:
    goto L_6129ea;
L_61293f:
    esi = rd32(m, ebp + 0x8u);
    edi = rd32(m, ebp - 0x104u);
    LIFT_CALL(0x5f3520u, 0x61294du);  // FUN_005f3520
    eax = (ebp - 0x8u);
    ecx = rd32(m, ebp - 0x104u);
    LIFT_CALL(0x5c0470u, 0x61295bu);  // FUN_005c0470
    ecx = rd32(m, ebp - 0x8u);
    wr32(m, ebp - 0x10u, ecx);
    edx = rd32(m, ebp - 0x4u);
    wr32(m, ebp - 0xcu, edx);
    eax = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, eax);
    ecx = rd32(m, ebp - 0xcu);
    esp -= 4; wr32(m, esp, ecx);
    edx = rd32(m, ebp - 0x10u);
    esp -= 4; wr32(m, esp, edx);
    ecx = rd32(m, ebp - 0x104u);
    LIFT_CALL(0x612a00u, 0x61297eu);  // FUN_00612a00
    edi = rd32(m, ebp - 0x104u);
    eax = rd32(m, ebp - 0x104u);
    eax = rd32(m, eax + 0x14u);
    esi = (ebp - 0x20u);
    LIFT_CALL(0x5c0540u, 0x612995u);  // FUN_005c0540
    ecx = rd32(m, ebp - 0x20u);
    edx = rd32(m, ebp - 0x1cu);
    wr32(m, ebp - 0x18u, ecx);
    wr32(m, ebp - 0x14u, edx);
    eax = rd32(m, ebp - 0x8u);
    wr32(m, ebp - 0x28u, eax);
    ecx = rd32(m, ebp - 0x4u);
    wr32(m, ebp - 0x24u, ecx);
    eax = (ebp - 0x38u);
    ecx = rd32(m, ebp + 0x8u);
    LIFT_CALL(0x5c0470u, 0x6129b8u);  // FUN_005c0470
    edx = rd32(m, eax);
    eax = rd32(m, eax + 0x4u);
    wr32(m, ebp - 0x30u, edx);
    wr32(m, ebp - 0x2cu, eax);
    ecx = rd32(m, ebp - 0x14u);
    esp -= 4; wr32(m, esp, ecx);
    edx = rd32(m, ebp - 0x18u);
    esp -= 4; wr32(m, esp, edx);
    eax = rd32(m, ebp - 0x24u);
    esp -= 4; wr32(m, esp, eax);
    ecx = rd32(m, ebp - 0x28u);
    esp -= 4; wr32(m, esp, ecx);
    edx = rd32(m, ebp - 0x104u);
    esp -= 4; wr32(m, esp, edx);
    eax = rd32(m, ebp - 0x2cu);
    esp -= 4; wr32(m, esp, eax);
    ecx = rd32(m, ebp - 0x30u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = rd32(m, ebp + 0x8u);
    LIFT_CALL(0x612e70u, 0x6129eau);  // FUN_00612e70
L_6129ea:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00612a00 Fn_00612a00
//   original FUN_00612a00; 0 instructions per frame (plan); register inputs: ecx; returns with ret 12
REFLEX_PORT_FN(0x00612a00, Fn_00612a00, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_612a17 = 0;
    uint32_t fB_612a17 = 0;
    uint32_t fA_612a21 = 0;
    uint32_t fB_612a21 = 0;
    uint32_t fR_612a2b = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = esp - 0x94u;
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    wr32(m, ebp - 0x94u, ecx);
    eax = rd32(m, ebp - 0x94u);
    fA_612a17 = eax;
    fB_612a17 = rd32(m, ebp + 0x10u);
    if (fA_612a17 == fB_612a17) goto L_612a92;
    ecx = rd32(m, ebp + 0x10u);
    edx = 0u;
    fA_612a21 = rd32(m, ecx + 0x18u);
    fB_612a21 = 0x0u;
    edx = (edx & 0xffffff00u) | (((fA_612a21 == fB_612a21) ? 1u : 0u) & 0xffu);
    eax = (edx & 0xffu);
    fR_612a2b = (eax & eax);
    if (!(fR_612a2b == 0)) goto L_612a92;
    edi = rd32(m, ebp + 0x10u);
    ecx = rd32(m, ebp + 0x10u);
    eax = rd32(m, ecx + 0x14u);
    esi = (ebp - 0x10u);
    LIFT_CALL(0x5c0540u, 0x612a40u);  // FUN_005c0540
    edx = rd32(m, ebp - 0x10u);
    eax = rd32(m, ebp - 0xcu);
    wr32(m, ebp - 0x8u, edx);
    wr32(m, ebp - 0x4u, eax);
    eax = (ebp - 0x20u);
    ecx = rd32(m, ebp + 0x10u);
    LIFT_CALL(0x5c0470u, 0x612a57u);  // FUN_005c0470
    ecx = rd32(m, eax);
    edx = rd32(m, eax + 0x4u);
    wr32(m, ebp - 0x18u, ecx);
    wr32(m, ebp - 0x14u, edx);
    esp -= 4; wr32(m, esp, 0x0u);
    eax = rd32(m, ebp + 0x10u);
    ecx = rd32(m, eax + 0x18u);
    esp -= 4; wr32(m, esp, ecx);
    edx = rd32(m, ebp - 0x4u);
    esp -= 4; wr32(m, esp, edx);
    eax = rd32(m, ebp - 0x8u);
    esp -= 4; wr32(m, esp, eax);
    ecx = rd32(m, ebp - 0x14u);
    esp -= 4; wr32(m, esp, ecx);
    edx = rd32(m, ebp - 0x18u);
    esp -= 4; wr32(m, esp, edx);
    eax = rd32(m, ebp + 0x10u);
    esp -= 4; wr32(m, esp, eax);
    ecx = rd32(m, ebp + 0xcu);
    esp -= 4; wr32(m, esp, ecx);
    edx = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, edx);
    ecx = rd32(m, ebp - 0x94u);
    LIFT_CALL(0x612c50u, 0x612a92u);  // FUN_00612c50
L_612a92:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(12);
}

// 00612aa0 Fn_00612aa0
//   original FUN_00612aa0; 1 instructions per frame (plan); register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x00612aa0, Fn_00612aa0, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_612ab4 = 0;
    uint32_t fB_612ab4 = 0;
    uint32_t fR_612b0f = 0;
    uint32_t fR_612b25 = 0;
    uint32_t fA_612b4d = 0;
    uint32_t fB_612b4d = 0;
    uint32_t fR_612b55 = 0;
    uint32_t fR_612be5 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = esp - 0xb8u;
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    wr32(m, ebp - 0xb8u, ecx);
    eax = rd32(m, ebp + 0x8u);
    fA_612ab4 = eax;
    fB_612ab4 = rd32(m, ebp - 0xb8u);
    if (fA_612ab4 == fB_612ab4) goto L_612c3d;
    eax = (ebp - 0x20u);
    ecx = rd32(m, ebp - 0xb8u);
    LIFT_CALL(0x5c0470u, 0x612aceu);  // FUN_005c0470
    edi = rd32(m, ebp - 0xb8u);
    ecx = rd32(m, ebp - 0xb8u);
    eax = rd32(m, ecx + 0x14u);
    esi = (ebp - 0x18u);
    LIFT_CALL(0x5c0540u, 0x612ae5u);  // FUN_005c0540
    eax = (ebp - 0x8u);
    ecx = rd32(m, ebp + 0x8u);
    LIFT_CALL(0x5c0470u, 0x612af0u);  // FUN_005c0470
    edi = rd32(m, ebp + 0x8u);
    edx = rd32(m, ebp + 0x8u);
    eax = rd32(m, edx + 0x14u);
    esi = (ebp - 0x10u);
    LIFT_CALL(0x5c0540u, 0x612b01u);  // FUN_005c0540
L_612b01:
    esi = (ebp - 0x18u);
    edi = (ebp - 0x20u);
    LIFT_CALL(0x5bb950u, 0x612b0cu);  // FUN_005bb950
    eax = (eax & 0xffu);
    fR_612b0f = (eax & eax);
    if ((fR_612b0f == 0)) goto L_612bd7;
    esi = (ebp - 0x10u);
    edi = (ebp - 0x8u);
    LIFT_CALL(0x5bb950u, 0x612b22u);  // FUN_005bb950
    ecx = (eax & 0xffu);
    fR_612b25 = (ecx & ecx);
    if ((fR_612b25 == 0)) goto L_612bd7;
    esi = (ebp - 0x20u);
    LIFT_CALL(0x585c60u, 0x612b35u);  // FUN_00585c60
    wr32(m, ebp - 0x5cu, eax);
    esi = (ebp - 0x8u);
    LIFT_CALL(0x585c60u, 0x612b40u);  // FUN_00585c60
    wr32(m, ebp - 0x60u, eax);
    edx = rd32(m, ebp - 0x60u);
    eax = rd32(m, ebp - 0x5cu);
    ecx = rd32(m, edx);
    edx = 0u;
    fA_612b4d = ecx;
    fB_612b4d = rd32(m, eax);
    edx = (edx & 0xffffff00u) | (((int32_t(sext(fA_612b4d, 4)) < int32_t(sext(fB_612b4d, 4))) ? 1u : 0u) & 0xffu);
    eax = (edx & 0xffu);
    fR_612b55 = (eax & eax);
    if ((fR_612b55 == 0)) goto L_612bca;
    ecx = rd32(m, ebp - 0x8u);
    wr32(m, ebp - 0x28u, ecx);
    edx = rd32(m, ebp - 0x4u);
    wr32(m, ebp - 0x24u, edx);
    esi = (ebp - 0x28u);
    LIFT_CALL(0x706600u, 0x612b6du);  // FUN_00706600
    eax = rd32(m, ebp - 0x28u);
    ecx = rd32(m, ebp - 0x24u);
    wr32(m, ebp - 0x30u, eax);
    wr32(m, ebp - 0x2cu, ecx);
    edx = rd32(m, ebp - 0x8u);
    wr32(m, ebp - 0x38u, edx);
    eax = rd32(m, ebp - 0x4u);
    wr32(m, ebp - 0x34u, eax);
    ecx = rd32(m, ebp - 0x20u);
    wr32(m, ebp - 0x40u, ecx);
    edx = rd32(m, ebp - 0x1cu);
    wr32(m, ebp - 0x3cu, edx);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0x1u);
    eax = rd32(m, ebp - 0x2cu);
    esp -= 4; wr32(m, esp, eax);
    ecx = rd32(m, ebp - 0x30u);
    esp -= 4; wr32(m, esp, ecx);
    edx = rd32(m, ebp - 0x34u);
    esp -= 4; wr32(m, esp, edx);
    eax = rd32(m, ebp - 0x38u);
    esp -= 4; wr32(m, esp, eax);
    ecx = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, ecx);
    edx = rd32(m, ebp - 0x3cu);
    esp -= 4; wr32(m, esp, edx);
    eax = rd32(m, ebp - 0x40u);
    esp -= 4; wr32(m, esp, eax);
    ecx = rd32(m, ebp - 0xb8u);
    LIFT_CALL(0x612c50u, 0x612bbcu);  // FUN_00612c50
    ecx = rd32(m, ebp - 0x28u);
    wr32(m, ebp - 0x8u, ecx);
    edx = rd32(m, ebp - 0x24u);
    wr32(m, ebp - 0x4u, edx);
    goto L_612bd2;
L_612bca:
    esi = (ebp - 0x20u);
    LIFT_CALL(0x706600u, 0x612bd2u);  // FUN_00706600
L_612bd2:
    goto L_612b01;
L_612bd7:
    esi = (ebp - 0x10u);
    edi = (ebp - 0x8u);
    LIFT_CALL(0x5bb950u, 0x612be2u);  // FUN_005bb950
    eax = (eax & 0xffu);
    fR_612be5 = (eax & eax);
    if ((fR_612be5 == 0)) goto L_612c3d;
    ecx = rd32(m, ebp - 0x10u);
    wr32(m, ebp - 0x48u, ecx);
    edx = rd32(m, ebp - 0xcu);
    wr32(m, ebp - 0x44u, edx);
    eax = rd32(m, ebp - 0x8u);
    wr32(m, ebp - 0x50u, eax);
    ecx = rd32(m, ebp - 0x4u);
    wr32(m, ebp - 0x4cu, ecx);
    edx = rd32(m, ebp - 0x18u);
    wr32(m, ebp - 0x58u, edx);
    eax = rd32(m, ebp - 0x14u);
    wr32(m, ebp - 0x54u, eax);
    esp -= 4; wr32(m, esp, 0x0u);
    ecx = rd32(m, ebp + 0x8u);
    edx = rd32(m, ecx + 0x18u);
    esp -= 4; wr32(m, esp, edx);
    eax = rd32(m, ebp - 0x44u);
    esp -= 4; wr32(m, esp, eax);
    ecx = rd32(m, ebp - 0x48u);
    esp -= 4; wr32(m, esp, ecx);
    edx = rd32(m, ebp - 0x4cu);
    esp -= 4; wr32(m, esp, edx);
    eax = rd32(m, ebp - 0x50u);
    esp -= 4; wr32(m, esp, eax);
    ecx = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, ecx);
    edx = rd32(m, ebp - 0x54u);
    esp -= 4; wr32(m, esp, edx);
    eax = rd32(m, ebp - 0x58u);
    esp -= 4; wr32(m, esp, eax);
    ecx = rd32(m, ebp - 0xb8u);
    LIFT_CALL(0x612c50u, 0x612c3du);  // FUN_00612c50
L_612c3d:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00612c50 Fn_00612c50
//   original FUN_00612c50; 1 instructions per frame (plan); register inputs: ecx; returns with ret 36
REFLEX_PORT_FN(0x00612c50, Fn_00612c50, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_612c65 = 0;
    uint32_t fA_612c73 = 0;
    uint32_t fB_612c73 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = esp - 0x90u;
    esp -= 4; wr32(m, esp, esi);
    wr32(m, ebp - 0x90u, ecx);
    eax = 0x1u;
    fR_612c65 = (eax & eax);
    if ((fR_612c65 == 0)) goto L_612d5b;
    ecx = rd32(m, ebp - 0x90u);
    fA_612c73 = ecx;
    fB_612c73 = rd32(m, ebp + 0x10u);
    if (fA_612c73 == fB_612c73) goto L_612c95;
    ecx = rd32(m, ebp + 0x24u);
    edx = rd32(m, ebp - 0x90u);
    LIFT_CALL(0x5c0900u, 0x612c86u);  // FUN_005c0900
    edx = rd32(m, ebp + 0x10u);
    eax = rd32(m, edx + 0x18u);
    eax = eax - rd32(m, ebp + 0x24u);
    ecx = rd32(m, ebp + 0x10u);
    wr32(m, ecx + 0x18u, eax);
L_612c95:
    eax = (ebp + 0x1cu);
    LIFT_CALL(0x7fa6c0u, 0x612c9du);  // FUN_007fa6c0
    esi = eax;
    eax = (ebp + 0x14u);
    LIFT_CALL(0x7fa6c0u, 0x612ca7u);  // FUN_007fa6c0
    LIFT_CALL(0x5c0970u, 0x612cacu);  // FUN_005c0970
    eax = rd32(m, eax);
    LIFT_CALL(0x7e3be0u, 0x612cb3u);  // rtti_shared_vf_007e3be0
    wr32(m, eax, esi);
    eax = (ebp + 0x8u);
    LIFT_CALL(0x7fa6c0u, 0x612cbdu);  // FUN_007fa6c0
    esi = eax;
    eax = (ebp + 0x1cu);
    LIFT_CALL(0x7fa6c0u, 0x612cc7u);  // FUN_007fa6c0
    LIFT_CALL(0x5c0970u, 0x612cccu);  // FUN_005c0970
    eax = rd32(m, eax);
    LIFT_CALL(0x7e3be0u, 0x612cd3u);  // rtti_shared_vf_007e3be0
    wr32(m, eax, esi);
    eax = (ebp + 0x14u);
    LIFT_CALL(0x7fa6c0u, 0x612cddu);  // FUN_007fa6c0
    esi = eax;
    eax = (ebp + 0x8u);
    LIFT_CALL(0x7fa6c0u, 0x612ce7u);  // FUN_007fa6c0
    LIFT_CALL(0x5c0970u, 0x612cecu);  // FUN_005c0970
    eax = rd32(m, eax);
    LIFT_CALL(0x7e3be0u, 0x612cf3u);  // rtti_shared_vf_007e3be0
    wr32(m, eax, esi);
    eax = (ebp + 0x8u);
    LIFT_CALL(0x7fa6c0u, 0x612cfdu);  // FUN_007fa6c0
    LIFT_CALL(0x5c0970u, 0x612d02u);  // FUN_005c0970
    edx = rd32(m, eax);
    wr32(m, ebp - 0x4u, edx);
    eax = (ebp + 0x1cu);
    LIFT_CALL(0x7fa6c0u, 0x612d0fu);  // FUN_007fa6c0
    LIFT_CALL(0x5c0970u, 0x612d14u);  // FUN_005c0970
    esi = eax;
    eax = (ebp + 0x8u);
    LIFT_CALL(0x7fa6c0u, 0x612d1eu);  // FUN_007fa6c0
    LIFT_CALL(0x5c0970u, 0x612d23u);  // FUN_005c0970
    ecx = rd32(m, esi);
    wr32(m, eax, ecx);
    eax = (ebp + 0x14u);
    LIFT_CALL(0x7fa6c0u, 0x612d2fu);  // FUN_007fa6c0
    LIFT_CALL(0x5c0970u, 0x612d34u);  // FUN_005c0970
    esi = eax;
    eax = (ebp + 0x1cu);
    LIFT_CALL(0x7fa6c0u, 0x612d3eu);  // FUN_007fa6c0
    LIFT_CALL(0x5c0970u, 0x612d43u);  // FUN_005c0970
    edx = rd32(m, esi);
    wr32(m, eax, edx);
    eax = (ebp + 0x14u);
    LIFT_CALL(0x7fa6c0u, 0x612d4fu);  // FUN_007fa6c0
    LIFT_CALL(0x5c0970u, 0x612d54u);  // FUN_005c0970
    ecx = rd32(m, ebp - 0x4u);
    wr32(m, eax, ecx);
    goto L_612d9b;
L_612d5b:
    edx = rd32(m, ebp + 0x20u);
    esp -= 4; wr32(m, esp, edx);
    eax = rd32(m, ebp + 0x1cu);
    esp -= 4; wr32(m, esp, eax);
    ecx = rd32(m, ebp + 0x18u);
    esp -= 4; wr32(m, esp, ecx);
    edx = rd32(m, ebp + 0x14u);
    esp -= 4; wr32(m, esp, edx);
    eax = rd32(m, ebp + 0xcu);
    esp -= 4; wr32(m, esp, eax);
    ecx = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, ecx);
    edx = rd32(m, ebp - 0x90u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x5c1db0u, 0x612d7fu);  // FUN_005c1db0
    eax = rd32(m, ebp + 0x20u);
    esp -= 4; wr32(m, esp, eax);
    ecx = rd32(m, ebp + 0x1cu);
    esp -= 4; wr32(m, esp, ecx);
    edx = rd32(m, ebp + 0x18u);
    esp -= 4; wr32(m, esp, edx);
    eax = rd32(m, ebp + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    ecx = (ebp - 0xcu);
    esp -= 4; wr32(m, esp, ecx);
    ecx = rd32(m, ebp + 0x10u);
    LIFT_CALL(0x612db0u, 0x612d9bu);  // FUN_00612db0
L_612d9b:
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(36);
}

// 00616f30 Fn_00616f30
//   original FUN_00616f30; 265 instructions per frame (plan); register inputs: eax; returns with ret 4
REFLEX_PORT_FN(0x00616f30, Fn_00616f30, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_616f57 = 0;
    uint32_t fB_616f57 = 0;
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    esp -= 4; wr32(m, esp, edi);
    esi = esi + 0x4u;
    edi = 0x4u;
L_616f40:
    eax = rd32(m, esi);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0xcu)));
    ecx = rd32(m, eax + 0x28u);
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x1cu);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(edx, 0x616f54u);
    esi = esi + 0x4u;
    fA_616f57 = edi;
    fB_616f57 = 0x1u;
    edi = fA_616f57 - fB_616f57;
    if (fA_616f57 != fB_616f57) goto L_616f40;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00617e10 AnimationEntity__vf29
//   original AnimationEntity::vf29; 679 instructions per frame (plan); register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x00617e10, AnimationEntity__vf29, "anim", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = (ecx + 0x180u);
    LIFT_RET(0);
}

// 00617fd0 CharacterEntity__vf14
//   original CharacterEntity::vf14; 0 instructions per frame (plan); register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x00617fd0, CharacterEntity__vf14, "anim", LIFT_OUTS) {
    LIFT_ENTRY();

    ecx = rd32(m, ecx + 0x210u);
    eax = rd8(m, ecx + 0xbu);
    ecx = rd32(m, ecx);
    eax = (eax << 6);
    eax = eax + rd32(m, ecx + 0x18u);
    LIFT_RET(0);
}

// 00617ff0 CharacterEntity__vf15
//   original CharacterEntity::vf15; 50 instructions per frame (plan); register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x00617ff0, CharacterEntity__vf15, "anim", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = rd32(m, ecx + 0x210u);
    esp -= 4; wr32(m, esp, esi);
    esi = rd8(m, eax + 0xbu);
    eax = rd32(m, eax);
    esi = (esi << 6);
    esi = esi + rd32(m, eax + 0x18u);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esp + 0xcu);
    ecx = 0x10u;
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00618020 CharacterEntity__vf17
//   original CharacterEntity::vf17; 1534 instructions per frame (plan); register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x00618020, CharacterEntity__vf17, "anim", LIFT_OUTS) {
    LIFT_ENTRY();

    ecx = rd32(m, ecx + 0x210u);
    eax = rd8(m, ecx + 0xbu);
    ecx = rd32(m, ecx);
    eax = (eax << 6);
    eax = eax + rd32(m, ecx + 0x18u);
    ecx = rd32(m, esp + 0x4u);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x30u)));
    wr32(m, ecx, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x34u)));
    wr32(m, ecx + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x38u)));
    wr32(m, ecx + 0x8u, fst32(m));
    fpop(m);
    LIFT_RET(4);
}

// 00618070 CharacterEntity__vf16
//   original CharacterEntity::vf16; 6655 instructions per frame (plan); register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x00618070, CharacterEntity__vf16, "anim", LIFT_OUTS) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    ecx = rd32(m, ecx + 0x210u);
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
    LIFT_CALL(0x7db7e0u, 0x6180aeu);  // FUN_007db7e0
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 006180c0 CharacterEntity__vf28
//   original CharacterEntity::vf28; 0 instructions per frame (plan); register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x006180c0, CharacterEntity__vf28, "anim", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = (ecx + 0x210u);
    LIFT_RET(0);
}

// 006180d0 RiderCharacter__vf22
//   original RiderCharacter::vf22; 0 instructions per frame (plan); register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x006180d0, RiderCharacter__vf22, "anim", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = 0x1081u;
    LIFT_RET(0);
}

// 00618100 CharacterEntity__vf14_o90
//   original CharacterEntity::vf14_o90; 328 instructions per frame (plan); register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x00618100, CharacterEntity__vf14_o90, "anim", LIFT_OUTS) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = (ecx + 0x180u);
    esi = (ecx + 0x1c0u);
    LIFT_CALL(0x7dd930u, 0x618113u);  // FUN_007dd930
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00619050 Fn_00619050
//   original FUN_00619050; 1 instructions per frame (plan); register inputs: ecx edi; returns with ret 0
REFLEX_PORT_FN(0x00619050, Fn_00619050, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_619063 = 0;
    uint32_t fB_619063 = 0;
    uint32_t fA_619070 = 0;
    uint32_t fB_619070 = 0;
    uint32_t fA_619084 = 0;
    uint32_t fB_619084 = 0;
    uint32_t fA_619091 = 0;
    uint32_t fB_619091 = 0;
    FCmp fC_6190af{};
    FCmp fC_6190bf{};
    uint32_t fA_6190cd = 0;
    uint32_t fB_6190cd = 0;
    FCmp fC_6190dd{};
    FCmp fC_6190ed{};
    uint32_t fA_6190fb = 0;
    uint32_t fB_6190fb = 0;
    uint32_t fA_619106 = 0;
    uint32_t fB_619106 = 0;
    FCmp fC_619131{};
    FCmp fC_619141{};
    FCmp fC_619170{};
    FCmp fC_619180{};
    FCmp fC_6191ef{};
    FCmp fC_6191ff{};
    uint32_t fA_61920d = 0;
    uint32_t fB_61920d = 0;
    FCmp fC_61921d{};
    FCmp fC_61922d{};
    uint32_t fA_61923b = 0;
    uint32_t fB_61923b = 0;
    FCmp fC_61924b{};
    FCmp fC_61925b{};
    uint32_t fA_619269 = 0;
    uint32_t fB_619269 = 0;
    FCmp fC_619279{};
    FCmp fC_619289{};
    uint32_t fA_619297 = 0;
    uint32_t fB_619297 = 0;
    FCmp fC_6192a7{};
    FCmp fC_6192b7{};
    uint32_t fA_6192c5 = 0;
    uint32_t fB_6192c5 = 0;
    FCmp fC_6192d5{};
    FCmp fC_6192e5{};
    uint32_t fA_6192f3 = 0;
    uint32_t fB_6192f3 = 0;
    uint32_t fA_6192fb = 0;
    uint32_t fB_6192fb = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    eax = rd32(m, edi + 0x564u);
    esp = esp - 0x8u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = 0u;
    esp -= 4; wr32(m, esp, esi);
    fA_619063 = eax;
    fB_619063 = ebx;
    if (fA_619063 == fB_619063) goto L_61906a;
    wr8(m, eax, 0x1u);
L_61906a:
    eax = rd32(m, edi + 0x568u);
    fA_619070 = eax;
    fB_619070 = ebx;
    if (fA_619070 == fB_619070) goto L_61907e;
    ecx = (ecx & 0xffff0000u) | ((rd16(m, edi + 0x5acu)) & 0xffffu);
    wr16(m, eax, (ecx & 0xffffu));
L_61907e:
    eax = rd32(m, edi + 0x57cu);
    fA_619084 = eax;
    fB_619084 = ebx;
    if (fA_619084 == fB_619084) goto L_61908b;
    wr8(m, eax, 0x1u);
L_61908b:
    eax = rd32(m, edi + 0x580u);
    fA_619091 = eax;
    fB_619091 = ebx;
    if (fA_619091 == fB_619091) goto L_61909f;
    edx = (edx & 0xffff0000u) | ((rd16(m, edi + 0x5a0u)) & 0xffffu);
    wr16(m, eax, (edx & 0xffffu));
L_61909f:
    x0 = movss_m(rd32(m, edi + 0x588u));
    x1 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    fC_6190af = fcmp32(x1.l(0), x0.l(0));
    x2 = movss_m(rd32(m, edi + 0x584u));
    if (!fc_b(fC_6190af)) goto L_6190bf;
    x0 = x1;
L_6190bf:
    fC_6190bf = fcmp32(x2.l(0), x0.l(0));
    if (fc_b(fC_6190bf)) goto L_6190c7;
    x0 = x2;
L_6190c7:
    eax = rd32(m, edi + 0x58cu);
    fA_6190cd = eax;
    fB_6190cd = ebx;
    if (fA_6190cd == fB_6190cd) goto L_6190d5;
    wr32(m, eax, x0.l(0));
L_6190d5:
    x0 = movss_m(rd32(m, edi + 0x594u));
    fC_6190dd = fcmp32(x1.l(0), x0.l(0));
    x2 = movss_m(rd32(m, edi + 0x590u));
    if (!fc_b(fC_6190dd)) goto L_6190ed;
    x0 = x1;
L_6190ed:
    fC_6190ed = fcmp32(x2.l(0), x0.l(0));
    if (fc_b(fC_6190ed)) goto L_6190f5;
    x0 = x2;
L_6190f5:
    eax = rd32(m, edi + 0x598u);
    fA_6190fb = eax;
    fB_6190fb = ebx;
    if (fA_6190fb == fB_6190fb) goto L_619103;
    wr32(m, eax, x0.l(0));
L_619103:
    x4 = X{};
    fA_619106 = rd32(m, edi + 0xaf08u);
    fB_619106 = ebx;
    if (fA_619106 != fB_619106) goto L_6193e6;
    eax = edi;
    LIFT_CALL(0x621400u, 0x619119u);  // FUN_00621400
    ecx = edi;
    LIFT_CALL(0x621320u, 0x619120u);  // FUN_00621320
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x621490u, 0x619126u);  // FUN_00621490
    x0 = movss_m(rd32(m, edi + 0x468u));
    x1 = X{};
    fC_619131 = fcmp32(x1.l(0), x0.l(0));
    x2 = movss_m(rd32(m, edi + 0x464u));
    if (!fc_b(fC_619131)) goto L_619141;
    x0 = x1;
L_619141:
    fC_619141 = fcmp32(x2.l(0), x0.l(0));
    if (fc_b(fC_619141)) goto L_61914e;
    wr32(m, esp + 0xcu, x2.l(0));
    goto L_619154;
L_61914e:
    wr32(m, esp + 0xcu, x0.l(0));
L_619154:
    eax = (esp + 0xcu);
    esp -= 4; wr32(m, esp, eax);
    ecx = (edi + 0x46cu);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x63b830u, 0x619165u);  // FUN_0063b830
    x0 = movss_m(rd32(m, edi + 0x488u));
    x1 = X{};
    fC_619170 = fcmp32(x1.l(0), x0.l(0));
    x2 = movss_m(rd32(m, edi + 0x484u));
    if (!fc_b(fC_619170)) goto L_619180;
    x0 = x1;
L_619180:
    fC_619180 = fcmp32(x2.l(0), x0.l(0));
    if (fc_b(fC_619180)) goto L_61918d;
    wr32(m, esp + 0xcu, x2.l(0));
    goto L_619193;
L_61918d:
    wr32(m, esp + 0xcu, x0.l(0));
L_619193:
    edx = (esp + 0xcu);
    esp -= 4; wr32(m, esp, edx);
    eax = (edi + 0x48cu);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x63b830u, 0x6191a4u);  // FUN_0063b830
    x2 = movss_m(rd32(m, 0x9322ccu) /* 0.5 */);
    x3 = X{};
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    wr32(m, edi + 0x3b4u, x2.l(0));
    wr32(m, edi + 0x388u, x2.l(0));
    wr32(m, edi + 0x2d8u, x3.l(0));
    wr32(m, edi + 0x330u, x3.l(0));
    wr32(m, edi + 0x35cu, x0.l(0));
    wr32(m, edi + 0xaf50u, x2.l(0));
    x0 = movss_m(rd32(m, edi + 0x3e4u));
    fC_6191ef = fcmp32(x2.l(0), x0.l(0));
    x1 = movss_m(rd32(m, edi + 0x3e0u));
    if (!fc_b(fC_6191ef)) goto L_6191ff;
    x0 = x2;
L_6191ff:
    fC_6191ff = fcmp32(x1.l(0), x0.l(0));
    if (fc_b(fC_6191ff)) goto L_619207;
    x0 = x1;
L_619207:
    eax = rd32(m, edi + 0x3e8u);
    fA_61920d = eax;
    fB_61920d = ebx;
    if (fA_61920d == fB_61920d) goto L_619215;
    wr32(m, eax, x0.l(0));
L_619215:
    x0 = movss_m(rd32(m, edi + 0x3f0u));
    fC_61921d = fcmp32(x2.l(0), x0.l(0));
    x1 = movss_m(rd32(m, edi + 0x3ecu));
    if (!fc_b(fC_61921d)) goto L_61922d;
    x0 = x2;
L_61922d:
    fC_61922d = fcmp32(x1.l(0), x0.l(0));
    if (fc_b(fC_61922d)) goto L_619235;
    x0 = x1;
L_619235:
    eax = rd32(m, edi + 0x3f4u);
    fA_61923b = eax;
    fB_61923b = ebx;
    if (fA_61923b == fB_61923b) goto L_619243;
    wr32(m, eax, x0.l(0));
L_619243:
    x0 = movss_m(rd32(m, edi + 0x3fcu));
    fC_61924b = fcmp32(x2.l(0), x0.l(0));
    x1 = movss_m(rd32(m, edi + 0x3f8u));
    if (!fc_b(fC_61924b)) goto L_61925b;
    x0 = x2;
L_61925b:
    fC_61925b = fcmp32(x1.l(0), x0.l(0));
    if (fc_b(fC_61925b)) goto L_619263;
    x0 = x1;
L_619263:
    eax = rd32(m, edi + 0x400u);
    fA_619269 = eax;
    fB_619269 = ebx;
    if (fA_619269 == fB_619269) goto L_619271;
    wr32(m, eax, x0.l(0));
L_619271:
    x0 = movss_m(rd32(m, edi + 0x3d8u));
    fC_619279 = fcmp32(x2.l(0), x0.l(0));
    x1 = movss_m(rd32(m, edi + 0x3d4u));
    if (!fc_b(fC_619279)) goto L_619289;
    x0 = x2;
L_619289:
    fC_619289 = fcmp32(x1.l(0), x0.l(0));
    if (fc_b(fC_619289)) goto L_619291;
    x0 = x1;
L_619291:
    eax = rd32(m, edi + 0x3dcu);
    fA_619297 = eax;
    fB_619297 = ebx;
    if (fA_619297 == fB_619297) goto L_61929f;
    wr32(m, eax, x0.l(0));
L_61929f:
    x0 = movss_m(rd32(m, edi + 0x3ccu));
    fC_6192a7 = fcmp32(x3.l(0), x0.l(0));
    x1 = movss_m(rd32(m, edi + 0x3c8u));
    if (!fc_b(fC_6192a7)) goto L_6192b7;
    x0 = x3;
L_6192b7:
    fC_6192b7 = fcmp32(x1.l(0), x0.l(0));
    if (fc_b(fC_6192b7)) goto L_6192bf;
    x0 = x1;
L_6192bf:
    eax = rd32(m, edi + 0x3d0u);
    fA_6192c5 = eax;
    fB_6192c5 = ebx;
    if (fA_6192c5 == fB_6192c5) goto L_6192cd;
    wr32(m, eax, x0.l(0));
L_6192cd:
    x0 = movss_m(rd32(m, edi + 0x3c0u));
    fC_6192d5 = fcmp32(x3.l(0), x0.l(0));
    x1 = movss_m(rd32(m, edi + 0x3bcu));
    if (!fc_b(fC_6192d5)) goto L_6192e5;
    x0 = x3;
L_6192e5:
    fC_6192e5 = fcmp32(x1.l(0), x0.l(0));
    if (fc_b(fC_6192e5)) goto L_6192ed;
    x0 = x1;
L_6192ed:
    eax = rd32(m, edi + 0x3c4u);
    fA_6192f3 = eax;
    fB_6192f3 = ebx;
    if (fA_6192f3 == fB_6192f3) goto L_6192fb;
    wr32(m, eax, x0.l(0));
L_6192fb:
    fA_6192fb = rd32(m, edi + 0x5bab8u);
    fB_6192fb = ebx;
    if (fA_6192fb != fB_6192fb) goto L_619323;
    wr32(m, edi + 0x414u, x2.l(0));
    wr32(m, edi + 0x42cu, x2.l(0));
    wr32(m, edi + 0x444u, x2.l(0));
    wr32(m, edi + 0x45cu, x2.l(0));
L_619323:
    ecx = (edi + 0x46cu);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x63b360u, 0x61932fu);  // FUN_0063b360
    edx = (edi + 0x48cu);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x63b3f0u, 0x61933bu);  // FUN_0063b3f0
    x4 = X{};
    x0 = movss_m(rd32(m, 0x9322ccu) /* 0.5 */);
    wr32(m, edi + 0xaf40u, x0.l(0));
    x0 = movss_m(rd32(m, 0x92e388u) /* 0.20000000298023224 */);
    wr32(m, edi + 0xaf10u, ebx);
    wr32(m, edi + 0xaf30u, ebx);
    wr32(m, edi + 0xaf34u, 0x1u);
    wr32(m, edi + 0xaf14u, ebx);
    wr32(m, edi + 0xaf18u, ebx);
    wr32(m, edi + 0x5b2e8u, ebx);
    wr32(m, edi + 0x5b2dcu, ebx);
    wr32(m, edi + 0xaf44u, x0.l(0));
    wr32(m, edi + 0xaf48u, x0.l(0));
    wr32(m, edi + 0xaf4cu, x0.l(0));
    wr32(m, edi + 0x5bb58u, x4.l(0));
    wr32(m, edi + 0x5b2d4u, ebx);
    wr32(m, edi + 0x5b584u, 0xcu);
    wr32(m, edi + 0x5b588u, ebx);
    wr32(m, edi + 0x5bb50u, ebx);
    wr32(m, edi + 0x5baacu, x4.l(0));
    wr32(m, edi + 0x5bab4u, x4.l(0));
    wr32(m, edi + 0x5bab0u, x4.l(0));
    wr32(m, edi + 0x5b2e0u, ebx);
    wr32(m, edi + 0x5b1f4u, x4.l(0));
L_6193e6:
    fpush(m, reflex_f80::from_f32(rd32(m, 0x92ff84u) /* 0.25 */));
    esp -= 4; wr32(m, esp, ecx);
    eax = 0x6u;
    wr32(m, esp, fst32(m));
    fpop(m);
    esi = 0u;
    ecx = edi;
    wr32(m, edi + 0xaf54u, ebx);
    wr32(m, edi + 0x5bb54u, ebx);
    wr8(m, edi + 0x5b578u, (ebx & 0xffu));
    wr32(m, edi + 0x5b6d0u, ebx);
    LIFT_CALL(0x6237a0u, 0x619416u);  // FUN_006237a0
    eax = rd32(m, edi + 0x5c4u);
    wr8(m, eax + 0x5cu, (ebx & 0xffu));
    ecx = rd32(m, edi + 0x5bcu);
    LIFT_CALL(0x6b04d0u, 0x61942au);  // FUN_006b04d0
    wr32(m, edi + 0xaf00u, ebx);
    wr32(m, edi + 0x5b57cu, 0xcu);
    wr32(m, edi + 0x5b580u, 0x3u);
    wr32(m, edi + 0x5b528u, x4.l(0));
    wr32(m, edi + 0x5b52cu, x4.l(0));
    wr32(m, edi + 0x5b530u, x4.l(0));
    wr32(m, edi + 0x5b534u, x4.l(0));
    wr32(m, edi + 0x5ba64u, x4.l(0));
    eax = edi;
    wr32(m, edi + 0x5ba60u, x4.l(0));
    LIFT_CALL(0x61c5f0u, 0x61947bu);  // FUN_0061c5f0
    esi = rd32(m, esp); esp += 4;
    wr32(m, edi + 0x5b568u, ebx);
    wr32(m, edi + 0x5b570u, x4.l(0));
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0061c5f0 Fn_0061c5f0
//   original FUN_0061c5f0; 0 instructions per frame (plan); register inputs: eax; returns with ret 0
REFLEX_PORT_FN(0x0061c5f0, Fn_0061c5f0, "anim", LIFT_OUTS) {
    LIFT_ENTRY();

    x0 = X{};
    x1 = movss_m(rd32(m, 0x9322acu) /* 0.009999999776482582 */);
    ecx = 0x4u;
    wr32(m, eax + 0x5b590u, ecx);
    wr32(m, eax + 0x5b594u, ecx);
    ecx = 0u;
    wr32(m, eax + 0x5b5e8u, ecx);
    wr32(m, eax + 0x5b5ecu, ecx);
    wr32(m, eax + 0x5b5f0u, x0.l(0));
    wr32(m, eax + 0x5b5f4u, x0.l(0));
    wr32(m, eax + 0x5b5f8u, x0.l(0));
    wr32(m, eax + 0x5b5d8u, x0.l(0));
    wr32(m, eax + 0x5b5a8u, x1.l(0));
    wr32(m, eax + 0x5b760u, ecx);
    wr32(m, eax + 0x5b790u, x0.l(0));
    wr32(m, eax + 0x5b794u, x0.l(0));
    wr32(m, eax + 0x5b798u, x0.l(0));
    wr32(m, eax + 0x5b7a0u, x0.l(0));
    wr32(m, eax + 0x5b850u, ecx);
    wr32(m, eax + 0x5b880u, x0.l(0));
    wr32(m, eax + 0x5b884u, x0.l(0));
    wr32(m, eax + 0x5b888u, x0.l(0));
    wr32(m, eax + 0x5b890u, x0.l(0));
    LIFT_RET(0);
}

// 0061d060 RiderAnimation_KickJobs
//   original RiderAnimation_KickJobs; 1609 instructions per frame (plan); register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x0061d060, RiderAnimation_KickJobs, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_61d065 = 0;
    uint32_t fR_61d086 = 0;
    uint32_t fA_61d08a = 0;
    uint32_t fB_61d08a = 0;
    uint32_t fR_61d09d = 0;
    uint32_t fA_61d0aa = 0;
    uint32_t fB_61d0aa = 0;
    uint32_t fR_61d0b7 = 0;
    uint32_t fA_61d0bb = 0;
    uint32_t fB_61d0bb = 0;
    uint32_t fR_61d0ce = 0;
    uint32_t fA_61d0db = 0;
    uint32_t fB_61d0db = 0;
    uint32_t fA_61d0f0 = 0;
    uint32_t fB_61d0f0 = 0;
    uint32_t fR_61d0ff = 0;
    uint32_t fR_61d108 = 0;
    uint32_t fA_61d115 = 0;
    uint32_t fB_61d115 = 0;
    eax = rd32(m, 0xa94080u);
    fR_61d065 = (eax & eax);
    if (((fR_61d065 == 0) || sgn(fR_61d065, 4) != false)) goto L_61d127;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esi = 0u;
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, 0x916034u) /* 8.295148810192616e-39 */;
    ebx = (esi + 0x3u);
    goto L_61d080;
L_61d080:
    eax = rd32(m, esi + 0xa9408cu);
    fR_61d086 = (eax & eax);
    if ((fR_61d086 == 0)) goto L_61d0a7;
    fA_61d08a = rd32(m, eax + 0x2c0u);
    fB_61d08a = ebx;
    if (fA_61d08a == fB_61d08a) goto L_61d0a7;
    eax = rd32(m, esi + 0xa940bcu);
    ecx = rd32(m, eax);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(edi, 0x61d09du);
    fR_61d09d = (eax & eax);
    if (!(fR_61d09d == 0)) goto L_61d0a7;
    LIFT_CALL(rd32(m, 0x916078u) /* 8.295188046549617e-39 */, 0x61d0a7u);
L_61d0a7:
    esi = esi + 0x4u;
    fA_61d0aa = esi;
    fB_61d0aa = 0x30u;
    if (int32_t(sext(fA_61d0aa, 4)) < int32_t(sext(fB_61d0aa, 4))) goto L_61d080;
    esi = 0u;
L_61d0b1:
    eax = rd32(m, esi + 0xa9408cu);
    fR_61d0b7 = (eax & eax);
    if ((fR_61d0b7 == 0)) goto L_61d0d8;
    fA_61d0bb = rd32(m, eax + 0x2c0u);
    fB_61d0bb = ebx;
    if (fA_61d0bb != fB_61d0bb) goto L_61d0d8;
    eax = rd32(m, esi + 0xa940bcu);
    edx = rd32(m, eax);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(edi, 0x61d0ceu);
    fR_61d0ce = (eax & eax);
    if (!(fR_61d0ce == 0)) goto L_61d0d8;
    LIFT_CALL(rd32(m, 0x916078u) /* 8.295188046549617e-39 */, 0x61d0d8u);
L_61d0d8:
    esi = esi + 0x4u;
    fA_61d0db = esi;
    fB_61d0db = 0x30u;
    if (int32_t(sext(fA_61d0db, 4)) < int32_t(sext(fB_61d0db, 4))) goto L_61d0b1;
    wr32(m, 0xa9417cu, 0x0u);
    esi = 0u;
L_61d0f0:
    fA_61d0f0 = rd32(m, esi + 0xa9408cu);
    fB_61d0f0 = 0x0u;
    if (fA_61d0f0 == fB_61d0f0) goto L_61d112;
    eax = rd32(m, esi + 0xa940ecu);
    fR_61d0ff = (eax & eax);
    if ((fR_61d0ff == 0)) goto L_61d112;
    eax = rd32(m, eax);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(edi, 0x61d108u);
    fR_61d108 = (eax & eax);
    if (!(fR_61d108 == 0)) goto L_61d112;
    LIFT_CALL(rd32(m, 0x916078u) /* 8.295188046549617e-39 */, 0x61d112u);
L_61d112:
    esi = esi + 0x4u;
    fA_61d115 = esi;
    fB_61d115 = 0x30u;
    if (int32_t(sext(fA_61d115, 4)) < int32_t(sext(fB_61d115, 4))) goto L_61d0f0;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    wr32(m, 0xa94180u, 0x0u);
    ebx = rd32(m, esp); esp += 4;
L_61d127:
    LIFT_RET(0);
}

// 0061d130 RiderAnimation_WaitJobs
//   original RiderAnimation_WaitJobs; 25 instructions per frame (plan); register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x0061d130, RiderAnimation_WaitJobs, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_61d135 = 0;
    uint32_t fR_61d14a = 0;
    eax = rd32(m, 0xa94080u);
    fR_61d135 = (eax & eax);
    if (((fR_61d135 == 0) || sgn(fR_61d135, 4) != false)) goto L_61d14f;
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, 0x916020u) /* 8.295000272555398e-39 */;
L_61d140:
    esp -= 4; wr32(m, esp, 0x0u);
    LIFT_CALL(esi, 0x61d144u);
    ecx = rd32(m, 0xa94080u);
    fR_61d14a = (ecx & ecx);
    if ((!(fR_61d14a == 0) && sgn(fR_61d14a, 4) == false)) goto L_61d140;
    esi = rd32(m, esp); esp += 4;
L_61d14f:
    wr32(m, 0xa94080u, 0x0u);
    LIFT_RET(0);
}
