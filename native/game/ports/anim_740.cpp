// Character / object animation ports (group "anim"), transliterated instruction by instruction from MXReflex.exe by the
// anim lifter (see native/game/notes/anim.md). Each function keeps the original's operations, order and memory traffic.
#include <stdexcept>
#include <utility>
#include "lift_rt.h"

// 00743f00 Fn_00743f00
//   original FUN_00743f00; 0 instructions per frame (plan); register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00743f00, Fn_00743f00, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_743f11 = 0;
    uint32_t fA_743f33 = 0;
    uint32_t fB_743f33 = 0;
    uint32_t fA_743f40 = 0;
    uint32_t fB_743f40 = 0;
    uint32_t fA_743f49 = 0;
    uint32_t fB_743f49 = 0;
    uint32_t fA_743f61 = 0;
    uint32_t fB_743f61 = 0;
    uint32_t fA_743f6f = 0;
    uint32_t fB_743f6f = 0;
    uint32_t fA_743f78 = 0;
    uint32_t fB_743f78 = 0;
    uint32_t fA_743f90 = 0;
    uint32_t fB_743f90 = 0;
    uint32_t fA_743f9e = 0;
    uint32_t fB_743f9e = 0;
    uint32_t fA_743fa7 = 0;
    uint32_t fB_743fa7 = 0;
    uint32_t fA_743fbf = 0;
    uint32_t fB_743fbf = 0;
    uint32_t fA_743fcd = 0;
    uint32_t fB_743fcd = 0;
    uint32_t fA_743fd6 = 0;
    uint32_t fB_743fd6 = 0;
    uint32_t fA_743fee = 0;
    uint32_t fB_743fee = 0;
    uint32_t fA_743ffb = 0;
    uint32_t fB_743ffb = 0;
    uint32_t fA_743fff = 0;
    uint32_t fB_743fff = 0;
    uint32_t fA_74401b = 0;
    uint32_t fB_74401b = 0;
    uint32_t fR_744062 = 0;
    uint32_t fA_744076 = 0;
    uint32_t fB_744076 = 0;
    uint32_t fA_744083 = 0;
    uint32_t fB_744083 = 0;
    uint32_t fA_744092 = 0;
    uint32_t fB_744092 = 0;
    uint32_t fA_74409a = 0;
    uint32_t fB_74409a = 0;
    uint32_t fA_7440c9 = 0;
    uint32_t fB_7440c9 = 0;
    uint32_t fA_7440d7 = 0;
    uint32_t fB_7440d7 = 0;
    uint32_t fA_7440e9 = 0;
    uint32_t fB_7440e9 = 0;
    uint32_t fR_7440fc = 0;
    uint32_t fA_744100 = 0;
    uint32_t fB_744100 = 0;
    uint32_t fR_74412f = 0;
    uint32_t fA_744140 = 0;
    uint32_t fB_744140 = 0;
    uint32_t fA_744152 = 0;
    uint32_t fB_744152 = 0;
    uint32_t fR_744165 = 0;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x18u);
    ebp = ebp + 0xfu;
    ebp = (ebp >> 4);
    ebp = (ebp << 4);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    fR_743f11 = (ebp & ebp);
    if (!(fR_743f11 == 0)) goto L_74405e;
    eax = rd32(m, esp + 0x14u);
    ecx = rd32(m, esp + 0x18u);
    eax = eax + 0x3cu;
    wr32(m, esp + 0x20u, 0x2u);
L_743f30:
    edi = rd32(m, eax - 0x8u);
    fA_743f33 = edi;
    fB_743f33 = 0x1u;
    if (fA_743f33 == fB_743f33) goto L_743f5e;
    esi = rd32(m, eax);
    edx = rd32(m, eax - 0xcu);
    ebx = (esi + edx);
    fA_743f40 = ecx;
    fB_743f40 = ebx;
    if (fA_743f40 < fB_743f40) goto L_743f5e;
    edx = uint32_t(int32_t(edx) * int32_t(edi));
    edx = edx + esi;
    fA_743f49 = ecx;
    fB_743f49 = edx;
    if (fA_743f49 > fB_743f49) goto L_743f5e;
    wr32(m, eax + 0x8u, (rd32(m, eax + 0x8u) - 1u));
    edx = rd32(m, eax + 0x8u);
    esi = rd32(m, eax + 0x4u);
    wr32(m, esi + edx * 4, ecx);
    ebp = 0x1u;
L_743f5e:
    edi = rd32(m, eax + 0x10u);
    fA_743f61 = edi;
    fB_743f61 = 0x1u;
    if (fA_743f61 == fB_743f61) goto L_743f8d;
    esi = rd32(m, eax + 0x18u);
    edx = rd32(m, eax + 0xcu);
    ebx = (edx + esi);
    fA_743f6f = ecx;
    fB_743f6f = ebx;
    if (fA_743f6f < fB_743f6f) goto L_743f8d;
    edx = uint32_t(int32_t(edx) * int32_t(edi));
    edx = edx + esi;
    fA_743f78 = ecx;
    fB_743f78 = edx;
    if (fA_743f78 > fB_743f78) goto L_743f8d;
    wr32(m, eax + 0x20u, (rd32(m, eax + 0x20u) - 1u));
    edx = rd32(m, eax + 0x20u);
    esi = rd32(m, eax + 0x1cu);
    wr32(m, esi + edx * 4, ecx);
    ebp = 0x1u;
L_743f8d:
    edi = rd32(m, eax + 0x28u);
    fA_743f90 = edi;
    fB_743f90 = 0x1u;
    if (fA_743f90 == fB_743f90) goto L_743fbc;
    esi = rd32(m, eax + 0x30u);
    edx = rd32(m, eax + 0x24u);
    ebx = (edx + esi);
    fA_743f9e = ecx;
    fB_743f9e = ebx;
    if (fA_743f9e < fB_743f9e) goto L_743fbc;
    edx = uint32_t(int32_t(edx) * int32_t(edi));
    edx = edx + esi;
    fA_743fa7 = ecx;
    fB_743fa7 = edx;
    if (fA_743fa7 > fB_743fa7) goto L_743fbc;
    wr32(m, eax + 0x38u, (rd32(m, eax + 0x38u) - 1u));
    edx = rd32(m, eax + 0x38u);
    esi = rd32(m, eax + 0x34u);
    wr32(m, esi + edx * 4, ecx);
    ebp = 0x1u;
L_743fbc:
    edi = rd32(m, eax + 0x40u);
    fA_743fbf = edi;
    fB_743fbf = 0x1u;
    if (fA_743fbf == fB_743fbf) goto L_743feb;
    esi = rd32(m, eax + 0x48u);
    edx = rd32(m, eax + 0x3cu);
    ebx = (edx + esi);
    fA_743fcd = ecx;
    fB_743fcd = ebx;
    if (fA_743fcd < fB_743fcd) goto L_743feb;
    edx = uint32_t(int32_t(edx) * int32_t(edi));
    edx = edx + esi;
    fA_743fd6 = ecx;
    fB_743fd6 = edx;
    if (fA_743fd6 > fB_743fd6) goto L_743feb;
    wr32(m, eax + 0x50u, (rd32(m, eax + 0x50u) - 1u));
    edx = rd32(m, eax + 0x50u);
    esi = rd32(m, eax + 0x4cu);
    wr32(m, esi + edx * 4, ecx);
    ebp = 0x1u;
L_743feb:
    eax = eax + 0x60u;
    fA_743fee = rd32(m, esp + 0x20u);
    fB_743fee = 0x1u;
    wr32(m, esp + 0x20u, fA_743fee - fB_743fee);
    if (fA_743fee != fB_743fee) goto L_743f30;
    edx = 0u;
    fA_743ffb = ebp;
    fB_743ffb = edx;
    if (fA_743ffb != fB_743ffb) goto L_744057;
    fA_743fff = ecx;
    fB_743fff = edx;
    if (fA_743fff == fB_743fff) goto L_744057;
    eax = rd32(m, esp + 0x14u);
    esi = rd32(m, eax + 0xf8u);
    esi = (esi - 1u);
    esi = (esi & ecx);
    ecx = ecx - esi;
    ecx = ecx - rd32(m, eax + 0xfcu);
    wr8(m, ecx + 0xcu, (edx & 0xffu));
    fA_74401b = rd32(m, eax + 0x108u);
    fB_74401b = edx;
    if (fA_74401b != fB_74401b) goto L_74403c;
    wr32(m, eax + 0x108u, ecx);
    edi = rd32(m, esp); esp += 4;
    wr32(m, ecx + 0x8u, edx);
    eax = rd32(m, eax + 0x108u);
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    wr32(m, eax + 0x4u, edx);
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_74403c:
    wr32(m, ecx + 0x8u, edx);
    edx = rd32(m, eax + 0x108u);
    wr32(m, ecx + 0x4u, edx);
    edx = rd32(m, eax + 0x108u);
    wr32(m, edx + 0x8u, ecx);
    wr32(m, eax + 0x108u, ecx);
L_744057:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_74405e:
    esi = rd32(m, esp + 0x18u);
    fR_744062 = (esi & esi);
    if ((fR_744062 == 0)) goto L_744139;
    ecx = rd32(m, esp + 0x14u);
    edi = 0u;
    ecx = ecx + 0x3cu;
L_744073:
    ebx = rd32(m, ecx - 0x8u);
    fA_744076 = ebx;
    fB_744076 = 0x1u;
    if (fA_744076 == fB_744076) goto L_744096;
    edx = rd32(m, ecx);
    eax = rd32(m, ecx - 0xcu);
    esi = (eax + edx);
    fA_744083 = rd32(m, esp + 0x18u);
    fB_744083 = esi;
    esi = rd32(m, esp + 0x18u);
    if (fA_744083 < fB_744083) goto L_744096;
    eax = uint32_t(int32_t(eax) * int32_t(ebx));
    eax = eax + edx;
    fA_744092 = esi;
    fB_744092 = eax;
    if (fA_744092 <= fB_744092) goto L_7440b7;
L_744096:
    edi = (edi + 1u);
    ecx = ecx + 0x18u;
    fA_74409a = edi;
    fB_74409a = 0x8u;
    if (int32_t(sext(fA_74409a, 4)) < int32_t(sext(fB_74409a, 4))) goto L_744073;
    ecx = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ecx + 0xf0u;
    esp -= 4; wr32(m, esp, ecx);
    edi = esi;
    LIFT_CALL(0x823900u, 0x7440b2u);  // FUN_00823900
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_7440b7:
    edx = rd32(m, esp + 0x14u);
    eax = (edi + edi * 2 + 0x6u);
    ebx = rd32(m, edx + eax * 8);
    eax = (edx + eax * 8);
    wr32(m, esp + 0x14u, eax);
    fA_7440c9 = ebx;
    fB_7440c9 = ebp;
    if (fA_7440c9 != fB_7440c9) goto L_7440d4;
    edi = rd32(m, esp); esp += 4;
    eax = esi;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_7440d4:
    ecx = (ebp - 0x10u);
    fA_7440d7 = ecx;
    fB_7440d7 = 0x70u;
    if (fA_7440d7 > fB_7440d7) goto L_744121;
    eax = ebp;
    eax = uint32_t(int32_t(eax) >> 4);
    eax = (eax + 1u);
    eax = (eax + eax * 2);
    ecx = rd32(m, edx + eax * 8 + 0x14u);
    fA_7440e9 = ecx;
    fB_7440e9 = rd32(m, edx + eax * 8 + 0x4u);
    eax = (edx + eax * 8);
    if (fA_7440e9 >= fB_7440e9) goto L_744121;
    edi = rd32(m, eax + 0x10u);
    edi = rd32(m, edi + ecx * 4);
    ecx = (ecx + 1u);
    wr32(m, eax + 0x14u, ecx);
    fR_7440fc = (edi & edi);
    if ((fR_7440fc == 0)) goto L_744121;
L_744100:
    fA_744100 = ebx;
    fB_744100 = ebp;
    if (fA_744100 >= fB_744100) goto L_744106;
    ebp = ebx;
L_744106:
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x8fa070u, 0x74410eu);  // MSVCR90.DLL::memcpy
    eax = rd32(m, esp + 0x20u);
    esp = esp + 0xcu;
    LIFT_CALL(0x823b20u, 0x74411au);  // FUN_00823b20
    eax = edi;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_744121:
    esp -= 4; wr32(m, esp, ebp);
    ecx = (edx + 0xf0u);
    LIFT_CALL(0x8237e0u, 0x74412du);  // FUN_008237e0
    edi = eax;
    fR_74412f = (edi & edi);
    if ((fR_74412f == 0)) goto L_744057;
    goto L_744100;
L_744139:
    esi = rd32(m, esp + 0x14u);
    edx = (ebp - 0x10u);
    fA_744140 = edx;
    fB_744140 = 0x70u;
    if (fA_744140 > fB_744140) goto L_744169;
    eax = ebp;
    eax = uint32_t(int32_t(eax) >> 4);
    eax = (eax + 1u);
    eax = (eax + eax * 2);
    edx = rd32(m, esi + eax * 8 + 0x14u);
    fA_744152 = edx;
    fB_744152 = rd32(m, esi + eax * 8 + 0x4u);
    ecx = (esi + eax * 8);
    if (fA_744152 >= fB_744152) goto L_744169;
    eax = rd32(m, ecx + 0x10u);
    eax = rd32(m, eax + edx * 4);
    edx = (edx + 1u);
    wr32(m, ecx + 0x14u, edx);
    fR_744165 = (eax & eax);
    if (!(fR_744165 == 0)) goto L_744175;
L_744169:
    esp -= 4; wr32(m, esp, ebp);
    ecx = (esi + 0xf0u);
    LIFT_CALL(0x8237e0u, 0x744175u);  // FUN_008237e0
L_744175:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 007473f0 Fn_007473f0
//   original FUN_007473f0; 64 instructions per frame (plan); register inputs: none; returns with ret 8
REFLEX_PORT_FN(0x007473f0, Fn_007473f0, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_747401 = 0;
    uint32_t fB_747401 = 0;
    uint32_t fR_74741a = 0;
    uint32_t fA_74746d = 0;
    uint32_t fB_74746d = 0;
    FCmp fC_74748d{};
    uint32_t fR_7474cd = 0;
    uint32_t fR_7474e2 = 0;
    uint32_t fA_7474ea = 0;
    uint32_t fB_7474ea = 0;
    uint32_t fR_7474fb = 0;
    uint32_t fA_747502 = 0;
    uint32_t fB_747502 = 0;
    FCmp fC_747517{};
    uint32_t fA_74752e = 0;
    uint32_t fB_74752e = 0;
    uint32_t fR_747539 = 0;
    uint32_t fR_74756a = 0;
    uint32_t fA_74757e = 0;
    uint32_t fB_74757e = 0;
    uint32_t fR_7475c2 = 0;
    uint32_t fA_74766b = 0;
    uint32_t fB_74766b = 0;
    uint32_t fA_747687 = 0;
    uint32_t fB_747687 = 0;
    uint32_t fA_74768b = 0;
    uint32_t fB_74768b = 0;
    uint32_t fR_7476a6 = 0;
    uint32_t fR_747702 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    eax = rd32(m, 0xa95844u);
    ecx = rd32(m, eax + 0x10u);
    esp = esp - 0x14u;
    fA_747401 = rd32(m, ecx + 0xcu);
    fB_747401 = 0x0u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    if (fA_747401 != fB_747401) goto L_747716;
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0x20u);
    ecx = ebx;
    LIFT_CALL(eax, 0x74741au);
    fR_74741a = (eax & eax);
    if (!(fR_74741a == 0)) goto L_747716;
    x0 = movss_m(rd32(m, ebx + 0x10u));
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0xcu)));
    edx = rd32(m, ebx);
    x0 = addss(x0, rd32(m, ebp + 0xcu));
    eax = rd32(m, edx + 0x8u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = ebx;
    wr32(m, esp, fst32(m));
    fpop(m);
    wr32(m, ebx + 0x10u, x0.l(0));
    LIFT_CALL(eax, 0x747441u);
    ecx = rd32(m, ebx + 0x14u);
    edx = rd32(m, ecx + 0x234u);
    edx = edx + 0x20150u;
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x74b000u, 0x747456u);  // FUN_0074b000
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0xcu)));
    eax = rd32(m, ebx + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    edi = (ebx + 0x20u);
    esp -= 4; wr32(m, esp, ecx);
    esi = edi;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x74cf70u, 0x74746bu);  // FUN_0074cf70
    esi = 0u;
    fA_74746d = rd32(m, ebx + 0x151b8u);
    fB_74746d = esi;
    if (fA_74746d == fB_74746d) goto L_7474a0;
    x0 = movss_m(rd32(m, ebx + 0x151a4u));
    x0 = subss(x0, rd32(m, ebx + 0x151bcu));
    x1 = movss_m(rd32(m, ebx + 0x151a8u));
    fC_74748d = fcmp32(x1.l(0), x0.l(0));
    if (fc_b(fC_74748d)) goto L_7474a0;
    ecx = rd32(m, ebx + 0x14u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = 0x95b7c4u;
    LIFT_CALL(0x55e990u, 0x7474a0u);  // FUN_0055e990
L_7474a0:
    eax = rd32(m, ebx + 0x14u);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x559e60u, 0x7474a9u);  // FUN_00559e60
    edx = rd32(m, eax + 0x74u);
    eax = rd32(m, edx + 0x1f0u);
    ecx = rd32(m, eax * 4 + 0x9c52c4u);
    edx = rd32(m, ecx + 0x32fcu);
    ecx = ecx + 0x32fcu;
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, edx + 0x4u);
    esp -= 4; wr32(m, esp, 0x7u);
    LIFT_CALL(eax, 0x7474cdu);
    fR_7474cd = (eax & eax);
    if ((fR_7474cd == 0)) goto L_7474e6;
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0x14u);
    ecx = ebx;
    LIFT_CALL(eax, 0x7474dau);
    wr32(m, esp + 0xcu, 0x1u);
    fR_7474e2 = (eax & eax);
    if (!(fR_7474e2 == 0)) goto L_7474ea;
L_7474e6:
    wr32(m, esp + 0xcu, esi);
L_7474ea:
    fA_7474ea = rd32(m, 0xa7f748u);
    fB_7474ea = esi;
    if (fA_7474ea == fB_7474ea) goto L_74750f;
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0x14u);
    ecx = ebx;
    LIFT_CALL(eax, 0x7474fbu);
    fR_7474fb = (eax & eax);
    if ((fR_7474fb == 0)) goto L_74750f;
    ecx = rd32(m, ebx + 0x14u);
    fA_747502 = rd32(m, ecx + 0xff06cu);
    fB_747502 = esi;
    if (fA_747502 != fB_747502) goto L_74750f;
    esi = 0x1u;
L_74750f:
    x0 = movss_m(rd32(m, edi + 0x15188u));
    fC_747517 = fcmp32(x0.l(0), rd32(m, edi + 0x15184u));
    wr32(m, 0xa7f748u, 0x0u);
    if (!(fc_b(fC_747517) || fc_z(fC_747517))) goto L_7475de;
    fA_74752e = rd32(m, esp + 0xcu);
    fB_74752e = 0x0u;
    if (fA_74752e != fB_74752e) goto L_7475de;
    fR_747539 = (esi & esi);
    if (!(fR_747539 == 0)) goto L_7475de;
    edx = rd32(m, ebx + 0x14u);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0xcu)));
    eax = rd32(m, edx + 0x234u);
    ecx = (eax + 0x218f0u);
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x4cu);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(edx, 0x74755eu);
    esi = rd32(m, ebx + 0x14u);
    esp -= 4; wr32(m, esp, 0x0u);
    eax = esi;
    LIFT_CALL(0x559e60u, 0x74756au);  // FUN_00559e60
    fR_74756a = (eax & eax);
    if ((fR_74756a == 0)) goto L_747716;
    esp -= 4; wr32(m, esp, 0x0u);
    eax = esi;
    LIFT_CALL(0x559e60u, 0x74757bu);  // FUN_00559e60
    eax = rd32(m, eax + 0x74u);
    fA_74757e = rd8(m, eax + 0x21cu);
    fB_74757e = 0x0u;
    if (fA_74757e != fB_74757e) goto L_747716;
    esi = rd32(m, esi + 0x234u);
    ecx = rd32(m, esi + 0x20138u);
    edx = rd32(m, ecx + 0x19b8u);
    esp -= 4; wr32(m, esp, edx);
    esi = esi + 0x218f0u;
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x63dba0u, 0x7475aau);  // FUN_0063dba0
    eax = rd32(m, ebx + 0x14u);
    ecx = rd32(m, eax + 0x234u);
    edx = rd32(m, ecx + 0x20138u);
    eax = rd32(m, edx + 0x19b8u);
    eax = rd32(m, eax + 0x64u);
    fR_7475c2 = (eax & eax);
    if ((fR_7475c2 == 0)) goto L_747716;
    x0 = X{};
    wr32(m, eax + 0x254u, x0.l(0));
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(8);
L_7475de:
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    ecx = (esp + 0x10u);
    wr32(m, esp + 0x10u, x0.l(0));
    wr32(m, esp + 0x14u, x0.l(0));
    wr32(m, esp + 0x18u, x0.l(0));
    wr32(m, esp + 0x1cu, x0.l(0));
    LIFT_CALL(0x55ea60u, 0x747607u);  // FUN_0055ea60
    ecx = rd32(m, ebx + 0x14u);
    edi = 0u;
    wr32(m, ebx + 0x151b8u, edi);
    edx = rd32(m, ecx + 0x234u);
    edx = edx + 0x20150u;
    LIFT_CALL(0x74b450u, 0x747623u);  // FUN_0074b450
    edx = rd32(m, ebx + 0x14u);
    eax = rd32(m, edx + 0x234u);
    eax = eax + 0x20150u;
    LIFT_CALL(0x74b480u, 0x747636u);  // FUN_0074b480
    eax = rd32(m, ebx + 0x14u);
    eax = rd32(m, eax + 0x234u);
    eax = eax + 0x20150u;
    LIFT_CALL(0x74b4f0u, 0x747649u);  // FUN_0074b4f0
    ecx = rd32(m, ebx + 0x14u);
    eax = rd32(m, ecx + 0x234u);
    eax = eax + 0x20150u;
    LIFT_CALL(0x74b060u, 0x74765cu);  // FUN_0074b060
    edx = rd32(m, ebx + 0x14u);
    eax = rd32(m, edx + 0x234u);
    eax = rd32(m, eax + 0x203e0u);
    fA_74766b = eax;
    fB_74766b = edi;
    if (fA_74766b == fB_74766b) goto L_747679;
    wr8(m, eax + 0xa2u, 0x0u);
    wr32(m, eax + 0x70u, edi);
L_747679:
    LIFT_CALL(0x47c3f0u, 0x74767eu);  // FUN_0047c3f0
    ecx = rd32(m, ebx + 0x14u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x55e2b0u, 0x747687u);  // FUN_0055e2b0
    fA_747687 = esi;
    fB_747687 = edi;
    if (fA_747687 == fB_747687) goto L_7476c0;
    fA_74768b = rd32(m, 0xa957d4u);
    fB_74768b = edi;
    if (fA_74768b == fB_74768b) goto L_74769d;
    esp -= 4; wr32(m, esp, 0x9419c4u);
    LIFT_CALL(0x80c880u, 0x74769du);  // LocSoundList_StopOwner
L_74769d:
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0x10u);
L_7476a2:
    ecx = ebx;
    LIFT_CALL(eax, 0x7476a6u);
    fR_7476a6 = (eax & eax);
    if ((fR_7476a6 == 0)) goto L_7476c7;
    edi = rd32(m, ebx + 0x151b4u);
    esi = ebx;
    LIFT_CALL(0x747720u, 0x7476b7u);  // FUN_00747720
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(8);
L_7476c0:
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0xcu);
    goto L_7476a2;
L_7476c7:
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0x18u);
    ecx = ebx;
    LIFT_CALL(eax, 0x7476d0u);
    edx = rd32(m, ebx + 0x14u);
    edi = 0x1u;
    LIFT_CALL(0x559e20u, 0x7476ddu);  // FUN_00559e20
    ecx = rd32(m, 0xa946b8u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x757660u, 0x7476eau);  // FUN_00757660
    edx = rd32(m, ebx + 0x14u);
    eax = rd32(m, edx + 0x234u);
    ecx = rd32(m, eax + 0x20138u);
    edx = rd32(m, ecx + 0x19b8u);
    eax = rd32(m, edx + 0x64u);
    fR_747702 = (eax & eax);
    if ((fR_747702 == 0)) goto L_747716;
    x0 = movss_m(rd32(m, 0x931d94u) /* 0.10000000149011612 */);
    wr32(m, eax + 0x254u, x0.l(0));
L_747716:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 00747720 Fn_00747720
//   original FUN_00747720; 0 instructions per frame (plan); register inputs: ecx esi edi; returns with ret 0
REFLEX_PORT_FN(0x00747720, Fn_00747720, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_747734 = 0;
    esp -= 4; wr32(m, esp, ecx);
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x4u);
    esp -= 4; wr32(m, esp, edi);
    ecx = esi;
    LIFT_CALL(edx, 0x74772bu);
    edx = edi;
    ecx = esi;
    LIFT_CALL(0x747750u, 0x747734u);  // FUN_00747750
    fR_747734 = (eax & eax);
    if ((fR_747734 == 0)) goto L_74774d;
    x0 = X{};
    wr32(m, esi + 0x151a8u, x0.l(0));
    wr32(m, esi + 0x151acu, 0x0u);
L_74774d:
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00747750 Fn_00747750
//   original FUN_00747750; 0 instructions per frame (plan); register inputs: ecx edx; returns with ret 0
REFLEX_PORT_FN(0x00747750, Fn_00747750, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_74779d = 0;
    uint32_t fB_74779d = 0;
    uint32_t fA_7477ab = 0;
    uint32_t fB_7477ab = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0x114u;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp + 0x110u, eax);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = ecx;
    ecx = rd32(m, edi + 0x14u);
    eax = (esp + 0x10u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edx);
    ebx = 0u;
    LIFT_CALL(0x747940u, 0x747780u);  // FUN_00747940
    eax = rd32(m, 0xa95844u);
    ecx = rd32(m, eax + 0x8u);
    edx = rd32(m, ecx);
    edx = rd32(m, edx + 0x6cu);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, 0x6c6d7862u);
    eax = (esp + 0x1cu);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(edx, 0x74779bu);
    esi = eax;
    fA_74779d = esi;
    fB_74779d = ebx;
    if (fA_74779d == fB_74779d) goto L_7477f6;
    eax = rd32(m, esi + 0x44u);
    wr32(m, esi + 0x4cu, 0x1u);
    fA_7477ab = eax;
    fB_7477ab = ebx;
    if (fA_7477ab == fB_7477ab) goto L_7477d4;
    eax = eax + 0xfffffba0u;
    edi = edi + 0x20u;
    LIFT_CALL(0x74ce40u, 0x7477bcu);  // FUN_0074ce40
    wr32(m, esi + 0x4cu, ebx);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp + 0x110u);
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x7477d0u);  // __security_check_cookie
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_7477d4:
    eax = 0u;
    edi = edi + 0x20u;
    LIFT_CALL(0x74ce40u, 0x7477deu);  // FUN_0074ce40
    wr32(m, esi + 0x4cu, ebx);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp + 0x110u);
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x7477f2u);  // __security_check_cookie
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_7477f6:
    ecx = rd32(m, esp + 0x11cu);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    eax = ebx;
    ebx = rd32(m, esp); esp += 4;
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x747809u);  // __security_check_cookie
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00747940 Fn_00747940
//   original FUN_00747940; 0 instructions per frame (plan); register inputs: edi; returns with ret 12
REFLEX_PORT_FN(0x00747940, Fn_00747940, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_747970 = 0;
    uint32_t fB_747970 = 0;
    uint32_t fR_747994 = 0;
    uint32_t fR_7479ac = 0;
    uint32_t fR_7479d4 = 0;
    uint32_t fR_7479f7 = 0;
    uint32_t fR_747a1a = 0;
    uint32_t fR_747a34 = 0;
    uint32_t fR_747a57 = 0;
    uint32_t fR_747a74 = 0;
    uint32_t fR_747a8c = 0;
    uint32_t fR_747aca = 0;
    uint32_t fR_747ad9 = 0;
    uint32_t fR_747af7 = 0;
    uint32_t fR_747b58 = 0;
    uint32_t fR_747b64 = 0;
    uint32_t fR_747b84 = 0;
    uint32_t fR_747b94 = 0;
    uint32_t fR_747bbf = 0;
    uint32_t fR_747bca = 0;
    uint32_t fR_747bf2 = 0;
    uint32_t fR_747c04 = 0;
    uint32_t fR_747c2c = 0;
    uint32_t fR_747c37 = 0;
    uint32_t fR_747c4c = 0;
    uint32_t fR_747c64 = 0;
    uint32_t fA_747c85 = 0;
    uint32_t fB_747c85 = 0;
    uint32_t FL = 0;
    uint32_t fR_747c89 = 0;
    uint32_t fA_747c90 = 0;
    uint32_t fB_747c90 = 0;
    uint32_t fR_747c9b = 0;
    uint32_t fA_747ca3 = 0;
    uint32_t fB_747ca3 = 0;
    uint32_t fK_747ca3 = 0;
    uint32_t fR_747ca8 = 0;
    uint32_t fR_747cb4 = 0;
    uint32_t fA_747cd9 = 0;
    uint32_t fB_747cd9 = 0;
    uint32_t fR_747cdd = 0;
    uint32_t fA_747ce4 = 0;
    uint32_t fB_747ce4 = 0;
    uint32_t fR_747cef = 0;
    uint32_t fA_747cf7 = 0;
    uint32_t fB_747cf7 = 0;
    uint32_t fK_747cf7 = 0;
    uint32_t fR_747cfc = 0;
    uint32_t fR_747d07 = 0;
    uint32_t fA_747d32 = 0;
    uint32_t fB_747d32 = 0;
    uint32_t fR_747d36 = 0;
    uint32_t fA_747d3d = 0;
    uint32_t fB_747d3d = 0;
    uint32_t fR_747d48 = 0;
    uint32_t fA_747d50 = 0;
    uint32_t fB_747d50 = 0;
    uint32_t fK_747d50 = 0;
    uint32_t fR_747d55 = 0;
    uint32_t fR_747d64 = 0;
    uint32_t fA_747d89 = 0;
    uint32_t fB_747d89 = 0;
    uint32_t fR_747d8d = 0;
    uint32_t fA_747d94 = 0;
    uint32_t fB_747d94 = 0;
    uint32_t fR_747d9f = 0;
    uint32_t fA_747da7 = 0;
    uint32_t fB_747da7 = 0;
    uint32_t fK_747da7 = 0;
    uint32_t fR_747dac = 0;
    uint32_t fR_747db7 = 0;
    uint32_t fA_747de2 = 0;
    uint32_t fB_747de2 = 0;
    uint32_t fR_747de6 = 0;
    uint32_t fA_747ded = 0;
    uint32_t fB_747ded = 0;
    uint32_t fR_747df8 = 0;
    uint32_t fA_747e00 = 0;
    uint32_t fB_747e00 = 0;
    uint32_t fK_747e00 = 0;
    uint32_t fR_747e05 = 0;
    uint32_t fR_747e14 = 0;
    uint32_t fA_747e39 = 0;
    uint32_t fB_747e39 = 0;
    uint32_t fR_747e3d = 0;
    uint32_t fA_747e44 = 0;
    uint32_t fB_747e44 = 0;
    uint32_t fR_747e4f = 0;
    uint32_t fA_747e57 = 0;
    uint32_t fB_747e57 = 0;
    uint32_t fK_747e57 = 0;
    uint32_t fR_747e5c = 0;
    uint32_t fR_747e67 = 0;
    uint32_t fA_747e92 = 0;
    uint32_t fB_747e92 = 0;
    uint32_t fR_747e96 = 0;
    uint32_t fA_747e9d = 0;
    uint32_t fB_747e9d = 0;
    uint32_t fR_747ea8 = 0;
    uint32_t fA_747eb0 = 0;
    uint32_t fB_747eb0 = 0;
    uint32_t fK_747eb0 = 0;
    uint32_t fR_747eb5 = 0;
    uint32_t fR_747ec4 = 0;
    uint32_t fA_747ee9 = 0;
    uint32_t fB_747ee9 = 0;
    uint32_t fR_747eed = 0;
    uint32_t fA_747ef4 = 0;
    uint32_t fB_747ef4 = 0;
    uint32_t fR_747eff = 0;
    uint32_t fA_747f07 = 0;
    uint32_t fB_747f07 = 0;
    uint32_t fK_747f07 = 0;
    uint32_t fR_747f0c = 0;
    uint32_t fR_747f17 = 0;
    uint32_t fA_747f42 = 0;
    uint32_t fB_747f42 = 0;
    uint32_t fR_747f46 = 0;
    uint32_t fA_747f4d = 0;
    uint32_t fB_747f4d = 0;
    uint32_t fR_747f58 = 0;
    uint32_t fA_747f60 = 0;
    uint32_t fB_747f60 = 0;
    uint32_t fK_747f60 = 0;
    uint32_t fR_747f65 = 0;
    uint32_t fR_747f74 = 0;
    uint32_t fA_747f99 = 0;
    uint32_t fB_747f99 = 0;
    uint32_t fR_747f9d = 0;
    uint32_t fA_747fa4 = 0;
    uint32_t fB_747fa4 = 0;
    uint32_t fR_747faf = 0;
    uint32_t fA_747fb7 = 0;
    uint32_t fB_747fb7 = 0;
    uint32_t fK_747fb7 = 0;
    uint32_t fR_747fbc = 0;
    uint32_t fR_747fc7 = 0;
    uint32_t fA_747ff2 = 0;
    uint32_t fB_747ff2 = 0;
    uint32_t fR_747ff6 = 0;
    uint32_t fA_747ffd = 0;
    uint32_t fB_747ffd = 0;
    uint32_t fR_748008 = 0;
    uint32_t fA_748010 = 0;
    uint32_t fB_748010 = 0;
    uint32_t fK_748010 = 0;
    uint32_t fR_748015 = 0;
    uint32_t fR_748024 = 0;
    uint32_t fA_748049 = 0;
    uint32_t fB_748049 = 0;
    uint32_t fR_74804d = 0;
    uint32_t fA_748054 = 0;
    uint32_t fB_748054 = 0;
    uint32_t fR_74805f = 0;
    uint32_t fA_748067 = 0;
    uint32_t fB_748067 = 0;
    uint32_t fK_748067 = 0;
    uint32_t fR_74806c = 0;
    uint32_t fR_748077 = 0;
    uint32_t fA_7480a2 = 0;
    uint32_t fB_7480a2 = 0;
    uint32_t fR_7480a6 = 0;
    uint32_t fA_7480ad = 0;
    uint32_t fB_7480ad = 0;
    uint32_t fR_7480b8 = 0;
    uint32_t fA_7480c0 = 0;
    uint32_t fB_7480c0 = 0;
    uint32_t fK_7480c0 = 0;
    uint32_t fR_7480c5 = 0;
    uint32_t fR_7480d4 = 0;
    uint32_t fA_7480f9 = 0;
    uint32_t fB_7480f9 = 0;
    uint32_t fR_7480fd = 0;
    uint32_t fA_748104 = 0;
    uint32_t fB_748104 = 0;
    uint32_t fR_74810f = 0;
    uint32_t fA_748117 = 0;
    uint32_t fB_748117 = 0;
    uint32_t fK_748117 = 0;
    uint32_t fR_74811c = 0;
    uint32_t fR_748127 = 0;
    uint32_t fA_748152 = 0;
    uint32_t fB_748152 = 0;
    uint32_t fR_748156 = 0;
    uint32_t fA_74815d = 0;
    uint32_t fB_74815d = 0;
    uint32_t fR_748168 = 0;
    uint32_t fA_748170 = 0;
    uint32_t fB_748170 = 0;
    uint32_t fK_748170 = 0;
    uint32_t fR_748175 = 0;
    uint32_t fR_748184 = 0;
    uint32_t fA_7481a9 = 0;
    uint32_t fB_7481a9 = 0;
    uint32_t fR_7481ad = 0;
    uint32_t fA_7481b4 = 0;
    uint32_t fB_7481b4 = 0;
    uint32_t fR_7481bf = 0;
    uint32_t fA_7481c7 = 0;
    uint32_t fB_7481c7 = 0;
    uint32_t fK_7481c7 = 0;
    uint32_t fR_7481cc = 0;
    uint32_t fR_7481d7 = 0;
    uint32_t fA_748202 = 0;
    uint32_t fB_748202 = 0;
    uint32_t fR_748206 = 0;
    uint32_t fA_74820d = 0;
    uint32_t fB_74820d = 0;
    uint32_t fR_748218 = 0;
    uint32_t fA_748220 = 0;
    uint32_t fB_748220 = 0;
    uint32_t fK_748220 = 0;
    uint32_t fR_748225 = 0;
    uint32_t fR_748234 = 0;
    uint32_t fA_748259 = 0;
    uint32_t fB_748259 = 0;
    uint32_t fR_74825d = 0;
    uint32_t fA_748264 = 0;
    uint32_t fB_748264 = 0;
    uint32_t fR_74826f = 0;
    uint32_t fA_748277 = 0;
    uint32_t fB_748277 = 0;
    uint32_t fK_748277 = 0;
    uint32_t fR_74827c = 0;
    uint32_t fR_748287 = 0;
    uint32_t fA_7482b2 = 0;
    uint32_t fB_7482b2 = 0;
    uint32_t fR_7482b6 = 0;
    uint32_t fA_7482bd = 0;
    uint32_t fB_7482bd = 0;
    uint32_t fR_7482c8 = 0;
    uint32_t fA_7482d0 = 0;
    uint32_t fB_7482d0 = 0;
    uint32_t fK_7482d0 = 0;
    uint32_t fR_7482d5 = 0;
    uint32_t fR_7482e4 = 0;
    uint32_t fA_748309 = 0;
    uint32_t fB_748309 = 0;
    uint32_t fR_74830d = 0;
    uint32_t fA_748314 = 0;
    uint32_t fB_748314 = 0;
    uint32_t fR_74831f = 0;
    uint32_t fA_748327 = 0;
    uint32_t fB_748327 = 0;
    uint32_t fK_748327 = 0;
    uint32_t fR_74832c = 0;
    uint32_t fR_748337 = 0;
    uint32_t fA_748362 = 0;
    uint32_t fB_748362 = 0;
    uint32_t fR_748366 = 0;
    uint32_t fA_74836d = 0;
    uint32_t fB_74836d = 0;
    uint32_t fR_748378 = 0;
    uint32_t fA_748380 = 0;
    uint32_t fB_748380 = 0;
    uint32_t fK_748380 = 0;
    uint32_t fR_748385 = 0;
    uint32_t fR_748394 = 0;
    uint32_t fA_7483b9 = 0;
    uint32_t fB_7483b9 = 0;
    uint32_t fR_7483bd = 0;
    uint32_t fA_7483c4 = 0;
    uint32_t fB_7483c4 = 0;
    uint32_t fR_7483cf = 0;
    uint32_t fA_7483d7 = 0;
    uint32_t fB_7483d7 = 0;
    uint32_t fK_7483d7 = 0;
    uint32_t fR_7483dc = 0;
    uint32_t fR_7483e7 = 0;
    uint32_t fA_748412 = 0;
    uint32_t fB_748412 = 0;
    uint32_t fR_748416 = 0;
    uint32_t fA_74841d = 0;
    uint32_t fB_74841d = 0;
    uint32_t fR_748428 = 0;
    uint32_t fA_748430 = 0;
    uint32_t fB_748430 = 0;
    uint32_t fK_748430 = 0;
    uint32_t fR_748435 = 0;
    uint32_t fR_748444 = 0;
    uint32_t fA_748469 = 0;
    uint32_t fB_748469 = 0;
    uint32_t fR_74846d = 0;
    uint32_t fA_748474 = 0;
    uint32_t fB_748474 = 0;
    uint32_t fR_74847f = 0;
    uint32_t fA_748487 = 0;
    uint32_t fB_748487 = 0;
    uint32_t fK_748487 = 0;
    uint32_t fR_74848c = 0;
    uint32_t fR_748497 = 0;
    uint32_t fA_7484c2 = 0;
    uint32_t fB_7484c2 = 0;
    uint32_t fR_7484c6 = 0;
    uint32_t fA_7484cd = 0;
    uint32_t fB_7484cd = 0;
    uint32_t fR_7484d8 = 0;
    uint32_t fA_7484e0 = 0;
    uint32_t fB_7484e0 = 0;
    uint32_t fK_7484e0 = 0;
    uint32_t fR_7484e5 = 0;
    uint32_t fR_7484f4 = 0;
    uint32_t fA_748519 = 0;
    uint32_t fB_748519 = 0;
    uint32_t fR_74851d = 0;
    uint32_t fA_748524 = 0;
    uint32_t fB_748524 = 0;
    uint32_t fR_74852f = 0;
    uint32_t fA_748537 = 0;
    uint32_t fB_748537 = 0;
    uint32_t fK_748537 = 0;
    uint32_t fR_74853c = 0;
    uint32_t fR_748547 = 0;
    uint32_t fA_748572 = 0;
    uint32_t fB_748572 = 0;
    uint32_t fR_748576 = 0;
    uint32_t fA_74857d = 0;
    uint32_t fB_74857d = 0;
    uint32_t fR_748588 = 0;
    uint32_t fA_748590 = 0;
    uint32_t fB_748590 = 0;
    uint32_t fK_748590 = 0;
    uint32_t fR_748595 = 0;
    uint32_t fR_7485a4 = 0;
    uint32_t fA_7485c9 = 0;
    uint32_t fB_7485c9 = 0;
    uint32_t fR_7485cd = 0;
    uint32_t fA_7485d4 = 0;
    uint32_t fB_7485d4 = 0;
    uint32_t fR_7485df = 0;
    uint32_t fA_7485ea = 0;
    uint32_t fB_7485ea = 0;
    uint32_t fK_7485ea = 0;
    uint32_t fR_7485ef = 0;
    uint32_t fR_7485fa = 0;
    uint32_t fA_748622 = 0;
    uint32_t fB_748622 = 0;
    uint32_t fR_748626 = 0;
    uint32_t fA_74862d = 0;
    uint32_t fB_74862d = 0;
    uint32_t fR_748638 = 0;
    uint32_t fA_748643 = 0;
    uint32_t fB_748643 = 0;
    uint32_t fK_748643 = 0;
    uint32_t fR_748648 = 0;
    uint32_t fR_748654 = 0;
    uint32_t fA_748679 = 0;
    uint32_t fB_748679 = 0;
    uint32_t fR_74867d = 0;
    uint32_t fA_748684 = 0;
    uint32_t fB_748684 = 0;
    uint32_t fR_74868f = 0;
    uint32_t fA_74869a = 0;
    uint32_t fB_74869a = 0;
    uint32_t fK_74869a = 0;
    uint32_t fR_74869f = 0;
    uint32_t fR_7486aa = 0;
    uint32_t fA_7486d2 = 0;
    uint32_t fB_7486d2 = 0;
    uint32_t fR_7486d6 = 0;
    uint32_t fA_7486dd = 0;
    uint32_t fB_7486dd = 0;
    uint32_t fR_7486e8 = 0;
    uint32_t fA_7486f3 = 0;
    uint32_t fB_7486f3 = 0;
    uint32_t fK_7486f3 = 0;
    uint32_t fR_7486f8 = 0;
    uint32_t fR_748704 = 0;
    uint32_t fA_748729 = 0;
    uint32_t fB_748729 = 0;
    uint32_t fR_74872d = 0;
    uint32_t fA_748734 = 0;
    uint32_t fB_748734 = 0;
    uint32_t fR_74873f = 0;
    uint32_t fA_74874a = 0;
    uint32_t fB_74874a = 0;
    uint32_t fK_74874a = 0;
    uint32_t fR_74874f = 0;
    uint32_t fR_74875a = 0;
    uint32_t fA_748782 = 0;
    uint32_t fB_748782 = 0;
    uint32_t fR_748786 = 0;
    uint32_t fA_74878d = 0;
    uint32_t fB_74878d = 0;
    uint32_t fR_74879c = 0;
    uint32_t fA_7487a7 = 0;
    uint32_t fB_7487a7 = 0;
    uint32_t fK_7487a7 = 0;
    uint32_t fR_7487ac = 0;
    uint32_t fR_7487bb = 0;
    uint32_t fA_7487d2 = 0;
    uint32_t fB_7487d2 = 0;
    uint32_t fR_7487d6 = 0;
    uint32_t fA_7487dd = 0;
    uint32_t fB_7487dd = 0;
    uint32_t fR_7487ec = 0;
    uint32_t fA_7487f7 = 0;
    uint32_t fB_7487f7 = 0;
    uint32_t fK_7487f7 = 0;
    uint32_t fR_7487fc = 0;
    uint32_t fR_748807 = 0;
    uint32_t fA_748832 = 0;
    uint32_t fB_748832 = 0;
    uint32_t fR_748836 = 0;
    uint32_t fA_748841 = 0;
    uint32_t fB_748841 = 0;
    uint32_t fR_748850 = 0;
    uint32_t fA_74885b = 0;
    uint32_t fB_74885b = 0;
    uint32_t fK_74885b = 0;
    uint32_t fR_748860 = 0;
    uint32_t fR_74886b = 0;
    uint32_t fA_748892 = 0;
    uint32_t fB_748892 = 0;
    uint32_t fR_748896 = 0;
    uint32_t fA_7488a1 = 0;
    uint32_t fB_7488a1 = 0;
    uint32_t fR_7488b0 = 0;
    uint32_t fA_7488bb = 0;
    uint32_t fB_7488bb = 0;
    uint32_t fK_7488bb = 0;
    uint32_t fR_7488c0 = 0;
    uint32_t fR_7488cb = 0;
    uint32_t fA_7488f2 = 0;
    uint32_t fB_7488f2 = 0;
    uint32_t fR_7488f6 = 0;
    uint32_t fA_748901 = 0;
    uint32_t fB_748901 = 0;
    uint32_t fR_748910 = 0;
    uint32_t fA_74891b = 0;
    uint32_t fB_74891b = 0;
    uint32_t fK_74891b = 0;
    uint32_t fR_748920 = 0;
    uint32_t fR_74892b = 0;
    uint32_t fA_748952 = 0;
    uint32_t fB_748952 = 0;
    uint32_t fR_74895a = 0;
    uint32_t fA_748965 = 0;
    uint32_t fB_748965 = 0;
    uint32_t fR_748974 = 0;
    uint32_t fA_74897f = 0;
    uint32_t fB_74897f = 0;
    uint32_t fK_74897f = 0;
    uint32_t fR_748984 = 0;
    uint32_t fR_748994 = 0;
    uint32_t fA_7489c2 = 0;
    uint32_t fB_7489c2 = 0;
    uint32_t fR_7489ca = 0;
    uint32_t fA_7489d5 = 0;
    uint32_t fB_7489d5 = 0;
    uint32_t fR_7489e4 = 0;
    uint32_t fA_7489ef = 0;
    uint32_t fB_7489ef = 0;
    uint32_t fK_7489ef = 0;
    uint32_t fR_7489f4 = 0;
    uint32_t fR_748a04 = 0;
    uint32_t fA_748a32 = 0;
    uint32_t fB_748a32 = 0;
    uint32_t fR_748a3a = 0;
    uint32_t fA_748a45 = 0;
    uint32_t fB_748a45 = 0;
    uint32_t fR_748a54 = 0;
    uint32_t fA_748a5f = 0;
    uint32_t fB_748a5f = 0;
    uint32_t fK_748a5f = 0;
    uint32_t fR_748a64 = 0;
    uint32_t fR_748a74 = 0;
    uint32_t fA_748aa2 = 0;
    uint32_t fB_748aa2 = 0;
    uint32_t fR_748aaa = 0;
    uint32_t fA_748ab5 = 0;
    uint32_t fB_748ab5 = 0;
    uint32_t fR_748ac4 = 0;
    uint32_t fA_748acf = 0;
    uint32_t fB_748acf = 0;
    uint32_t fK_748acf = 0;
    uint32_t fR_748ad4 = 0;
    uint32_t fR_748ae4 = 0;
    uint32_t fA_748b12 = 0;
    uint32_t fB_748b12 = 0;
    uint32_t fR_748b1a = 0;
    uint32_t fA_748b25 = 0;
    uint32_t fB_748b25 = 0;
    uint32_t fR_748b34 = 0;
    uint32_t fA_748b3f = 0;
    uint32_t fB_748b3f = 0;
    uint32_t fK_748b3f = 0;
    uint32_t fR_748b44 = 0;
    uint32_t fR_748b54 = 0;
    uint32_t fA_748b82 = 0;
    uint32_t fB_748b82 = 0;
    uint32_t fR_748b8a = 0;
    uint32_t fA_748b95 = 0;
    uint32_t fB_748b95 = 0;
    uint32_t fR_748ba4 = 0;
    uint32_t fA_748baf = 0;
    uint32_t fB_748baf = 0;
    uint32_t fK_748baf = 0;
    uint32_t fR_748bb4 = 0;
    uint32_t fR_748bc4 = 0;
    uint32_t fA_748bf2 = 0;
    uint32_t fB_748bf2 = 0;
    uint32_t fR_748bfa = 0;
    uint32_t fA_748c05 = 0;
    uint32_t fB_748c05 = 0;
    uint32_t fR_748c14 = 0;
    uint32_t fA_748c1f = 0;
    uint32_t fB_748c1f = 0;
    uint32_t fK_748c1f = 0;
    uint32_t fR_748c24 = 0;
    uint32_t fR_748c34 = 0;
    uint32_t fA_748c62 = 0;
    uint32_t fB_748c62 = 0;
    uint32_t fR_748c6a = 0;
    uint32_t fA_748c75 = 0;
    uint32_t fB_748c75 = 0;
    uint32_t fR_748c84 = 0;
    uint32_t fA_748c8f = 0;
    uint32_t fB_748c8f = 0;
    uint32_t fK_748c8f = 0;
    uint32_t fR_748c94 = 0;
    uint32_t fR_748ca4 = 0;
    uint32_t fA_748cd2 = 0;
    uint32_t fB_748cd2 = 0;
    uint32_t fR_748cda = 0;
    uint32_t fA_748ce5 = 0;
    uint32_t fB_748ce5 = 0;
    uint32_t fR_748cf4 = 0;
    uint32_t fA_748cff = 0;
    uint32_t fB_748cff = 0;
    uint32_t fK_748cff = 0;
    uint32_t fR_748d04 = 0;
    uint32_t fR_748d14 = 0;
    uint32_t fA_748d42 = 0;
    uint32_t fB_748d42 = 0;
    uint32_t fR_748d4a = 0;
    uint32_t fA_748d55 = 0;
    uint32_t fB_748d55 = 0;
    uint32_t fR_748d64 = 0;
    uint32_t fA_748d6f = 0;
    uint32_t fB_748d6f = 0;
    uint32_t fK_748d6f = 0;
    uint32_t fR_748d74 = 0;
    uint32_t fR_748d84 = 0;
    uint32_t fA_748d96 = 0;
    uint32_t fB_748d96 = 0;
    uint32_t fR_748d9e = 0;
    uint32_t fA_748da9 = 0;
    uint32_t fB_748da9 = 0;
    uint32_t fR_748db8 = 0;
    uint32_t fA_748dc3 = 0;
    uint32_t fB_748dc3 = 0;
    uint32_t fK_748dc3 = 0;
    uint32_t fR_748dc8 = 0;
    uint32_t fR_748dd7 = 0;
    uint32_t fA_748e02 = 0;
    uint32_t fB_748e02 = 0;
    uint32_t fR_748e0a = 0;
    uint32_t fA_748e15 = 0;
    uint32_t fB_748e15 = 0;
    uint32_t fR_748e24 = 0;
    uint32_t fA_748e2f = 0;
    uint32_t fB_748e2f = 0;
    uint32_t fK_748e2f = 0;
    uint32_t fR_748e34 = 0;
    uint32_t fR_748e44 = 0;
    uint32_t fA_748e72 = 0;
    uint32_t fB_748e72 = 0;
    uint32_t fR_748e7a = 0;
    uint32_t fA_748e85 = 0;
    uint32_t fB_748e85 = 0;
    uint32_t fR_748e94 = 0;
    uint32_t fA_748e9f = 0;
    uint32_t fB_748e9f = 0;
    uint32_t fK_748e9f = 0;
    uint32_t fR_748ea4 = 0;
    uint32_t fR_748eb4 = 0;
    uint32_t fA_748ee2 = 0;
    uint32_t fB_748ee2 = 0;
    uint32_t fR_748eea = 0;
    uint32_t fA_748ef5 = 0;
    uint32_t fB_748ef5 = 0;
    uint32_t fR_748f04 = 0;
    uint32_t fA_748f0f = 0;
    uint32_t fB_748f0f = 0;
    uint32_t fK_748f0f = 0;
    uint32_t fR_748f14 = 0;
    uint32_t fR_748f24 = 0;
    uint32_t fA_748f52 = 0;
    uint32_t fB_748f52 = 0;
    uint32_t fR_748f5a = 0;
    uint32_t fA_748f65 = 0;
    uint32_t fB_748f65 = 0;
    uint32_t fR_748f74 = 0;
    uint32_t fA_748f7f = 0;
    uint32_t fB_748f7f = 0;
    uint32_t fK_748f7f = 0;
    uint32_t fR_748f84 = 0;
    uint32_t fR_748f94 = 0;
    uint32_t fA_748fc2 = 0;
    uint32_t fB_748fc2 = 0;
    uint32_t fR_748fca = 0;
    uint32_t fA_748fd5 = 0;
    uint32_t fB_748fd5 = 0;
    uint32_t fR_748fe4 = 0;
    uint32_t fA_748fef = 0;
    uint32_t fB_748fef = 0;
    uint32_t fK_748fef = 0;
    uint32_t fR_748ff4 = 0;
    uint32_t fR_749004 = 0;
    uint32_t fA_749032 = 0;
    uint32_t fB_749032 = 0;
    uint32_t fR_74903a = 0;
    uint32_t fA_749045 = 0;
    uint32_t fB_749045 = 0;
    uint32_t fR_749054 = 0;
    uint32_t fA_74905f = 0;
    uint32_t fB_74905f = 0;
    uint32_t fK_74905f = 0;
    uint32_t fR_749064 = 0;
    uint32_t fR_749074 = 0;
    uint32_t fR_74908f = 0;
    uint32_t fA_749097 = 0;
    uint32_t fB_749097 = 0;
    uint32_t fR_7490a4 = 0;
    uint32_t fA_7490a8 = 0;
    uint32_t fB_7490a8 = 0;
    uint32_t fR_7490ca = 0;
    uint32_t fR_7490d9 = 0;
    uint32_t fR_7490f7 = 0;
    uint32_t fR_74910a = 0;
    uint32_t fA_74911d = 0;
    uint32_t fB_74911d = 0;
    uint32_t fR_749134 = 0;
    uint32_t fR_749167 = 0;
    uint32_t fR_7491a4 = 0;
    uint32_t fR_7491f4 = 0;
    uint32_t fR_749234 = 0;
    uint32_t fR_749274 = 0;
    uint32_t fR_7492a5 = 0;
    uint32_t fR_7492e4 = 0;
    uint32_t fR_749324 = 0;
    uint32_t fR_749364 = 0;
    uint32_t fR_7493a4 = 0;
    uint32_t fR_7493e7 = 0;
    uint32_t fR_749424 = 0;
    uint32_t fR_749469 = 0;
    esp = esp - 0x14u;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp + 0x10u, eax);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x20u);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x28u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, 0x104u);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, ebp);
    LIFT_CALL(0x8fa06au, 0x747967u);  // MSVCR90.DLL::memset
    eax = rd32(m, ebx + 0x23cu);
    esp = esp + 0xcu;
    fA_747970 = eax;
    fB_747970 = 0x3cu;
    if (fA_747970 > fB_747970) goto L_747c45;
    eax = rd8(m, eax + 0x7494d8u);
    switch (eax) { case 0: goto L_747987; case 1: goto L_7479a5; case 2: goto L_747a2b; case 3: goto L_747a50; case 4: goto L_747a85; case 5: goto L_747af0; case 6: goto L_747b13; case 7: goto L_7479f0; case 8: goto L_747a13; case 9: goto L_7479c8; case 10: goto L_747a67; case 11: goto L_747c45; default: throw std::runtime_error("jump table"); }
L_747987:
    edi = ebp;
    edi = (edi - 1u);
L_747990:
    eax = (eax & 0xffffff00u) | ((rd8(m, edi + 0x1u)) & 0xffu);
    edi = (edi + 1u);
    fR_747994 = ((eax & 0xffu) & (eax & 0xffu));
    if (!(fR_747994 == 0)) goto L_747990;
    ecx = rd32(m, 0x9420ecu) /* 7.737608584997844e-39 */;
    wr32(m, edi, ecx);
    goto L_747c5a;
L_7479a5:
    eax = ebp;
    eax = (eax - 1u);
L_7479a8:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_7479ac = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_7479ac == 0)) goto L_7479a8;
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c688u)) & 0xffffu);
    wr16(m, eax, (edx & 0xffffu));
    ecx = (ecx & 0xffffff00u) | ((rd8(m, 0x96c68au)) & 0xffu);
    wr8(m, eax + 0x2u, (ecx & 0xffu));
    goto L_747c5a;
L_7479c8:
    eax = ebp;
    eax = (eax - 1u);
    goto L_7479d0;
L_7479d0:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_7479d4 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_7479d4 == 0)) goto L_7479d0;
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c68cu)) & 0xffffu);
    wr16(m, eax, (edx & 0xffffu));
    ecx = (ecx & 0xffffff00u) | ((rd8(m, 0x96c68eu)) & 0xffu);
    wr8(m, eax + 0x2u, (ecx & 0xffu));
    goto L_747c5a;
L_7479f0:
    eax = ebp;
    eax = (eax - 1u);
L_7479f3:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_7479f7 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_7479f7 == 0)) goto L_7479f3;
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c690u)) & 0xffffu);
    wr16(m, eax, (edx & 0xffffu));
    ecx = (ecx & 0xffffff00u) | ((rd8(m, 0x96c692u)) & 0xffu);
    wr8(m, eax + 0x2u, (ecx & 0xffu));
    goto L_747c5a;
L_747a13:
    edi = ebp;
    edi = (edi - 1u);
L_747a16:
    eax = (eax & 0xffffff00u) | ((rd8(m, edi + 0x1u)) & 0xffu);
    edi = (edi + 1u);
    fR_747a1a = ((eax & 0xffu) & (eax & 0xffu));
    if (!(fR_747a1a == 0)) goto L_747a16;
    edx = rd32(m, 0x93c734u) /* 7.928105302133553e-39 */;
    wr32(m, edi, edx);
    goto L_747c5a;
L_747a2b:
    eax = ebp;
    eax = (eax - 1u);
    edi = edi;
L_747a30:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_747a34 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_747a34 == 0)) goto L_747a30;
    ecx = (ecx & 0xffff0000u) | ((rd16(m, 0x96c694u)) & 0xffffu);
    wr16(m, eax, (ecx & 0xffffu));
    edx = (edx & 0xffffff00u) | ((rd8(m, 0x96c696u)) & 0xffu);
    wr8(m, eax + 0x2u, (edx & 0xffu));
    goto L_747c5a;
L_747a50:
    edi = ebp;
    edi = (edi - 1u);
L_747a53:
    eax = (eax & 0xffffff00u) | ((rd8(m, edi + 0x1u)) & 0xffu);
    edi = (edi + 1u);
    fR_747a57 = ((eax & 0xffu) & (eax & 0xffu));
    if (!(fR_747a57 == 0)) goto L_747a53;
    eax = rd32(m, 0x96c698u) /* 8.1092441481245e-39 */;
    wr32(m, edi, eax);
    goto L_747c5a;
L_747a67:
    edi = ebp;
    edi = (edi - 1u);
L_747a70:
    eax = (eax & 0xffffff00u) | ((rd8(m, edi + 0x1u)) & 0xffu);
    edi = (edi + 1u);
    fR_747a74 = ((eax & 0xffu) & (eax & 0xffu));
    if (!(fR_747a74 == 0)) goto L_747a70;
    ecx = rd32(m, 0x96c69cu) /* 6.823547201912623e-39 */;
    wr32(m, edi, ecx);
    goto L_747c5a;
L_747a85:
    eax = ebp;
    eax = (eax - 1u);
L_747a88:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_747a8c = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_747a8c == 0)) goto L_747a88;
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c6a0u)) & 0xffffu);
    wr16(m, eax, (edx & 0xffffu));
    ecx = (ecx & 0xffffff00u) | ((rd8(m, 0x96c6a2u)) & 0xffu);
    wr8(m, eax + 0x2u, (ecx & 0xffu));
    edx = rd32(m, ebx + 0x220u);
    edx = edx + 0x31u;
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, 0x9660c0u);
    esi = 0x2u;
    edi = (esp + 0x18u);
    LIFT_CALL(0x78fd60u, 0x747ac0u);  // FUN_0078fd60
    eax = edi;
    esp = esp + 0x8u;
    ecx = eax;
L_747ac7:
    edx = (edx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    eax = (eax + 1u);
    fR_747aca = ((edx & 0xffu) & (edx & 0xffu));
    if (!(fR_747aca == 0)) goto L_747ac7;
    edi = ebp;
    eax = eax - ecx;
    esi = ecx;
    edi = (edi - 1u);
L_747ad5:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, edi + 0x1u)) & 0xffu);
    edi = (edi + 1u);
    fR_747ad9 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_747ad9 == 0)) goto L_747ad5;
    ecx = eax;
    ecx = (ecx >> 2);
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    ecx = eax;
    ecx = (ecx & 0x3u);
    while (ecx) { wr8(m, edi, rd8(m, esi)); esi += 1; edi += 1; ecx--; }
    goto L_747c5a;
L_747af0:
    eax = ebp;
    eax = (eax - 1u);
L_747af3:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_747af7 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_747af7 == 0)) goto L_747af3;
    ecx = (ecx & 0xffff0000u) | ((rd16(m, 0x96c6a4u)) & 0xffffu);
    wr16(m, eax, (ecx & 0xffffu));
    edx = (edx & 0xffffff00u) | ((rd8(m, 0x96c6a6u)) & 0xffu);
    wr8(m, eax + 0x2u, (edx & 0xffu));
    goto L_747c5a;
L_747b13:
    eax = rd32(m, ebx + 0x234u);
    ecx = sext(rd8(m, eax + 0x20032u), 1);
    edx = sext(rd8(m, eax + 0x20031u), 1);
    eax = eax + 0x20030u;
    eax = sext(rd8(m, eax), 1);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, 0x9420e4u);
    esi = 0x4u;
    edi = (esp + 0x24u);
    LIFT_CALL(0x78fd60u, 0x747b45u);  // FUN_0078fd60
    esi = rd32(m, 0x9162bcu) /* 8.287018476502604e-39 */;
    ecx = edi;
    esp -= 4; wr32(m, esp, 0x9420ecu);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(esi, 0x747b55u);
    esp = esp + 0x18u;
    fR_747b58 = (eax & eax);
    if (!(fR_747b58 == 0)) goto L_747b75;
    edi = ebp;
    edi = (edi - 1u);
L_747b60:
    eax = (eax & 0xffffff00u) | ((rd8(m, edi + 0x1u)) & 0xffu);
    edi = (edi + 1u);
    fR_747b64 = ((eax & 0xffu) & (eax & 0xffu));
    if (!(fR_747b64 == 0)) goto L_747b60;
    edx = rd32(m, 0x9420ecu) /* 7.737608584997844e-39 */;
    wr32(m, edi, edx);
    goto L_747c5a;
L_747b75:
    eax = (esp + 0x14u);
    esp -= 4; wr32(m, esp, 0x9420f0u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(esi, 0x747b81u);
    esp = esp + 0x8u;
    fR_747b84 = (eax & eax);
    if (!(fR_747b84 == 0)) goto L_747bb0;
    eax = ebp;
    eax = (eax - 1u);
    goto L_747b90;
L_747b90:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_747b94 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_747b94 == 0)) goto L_747b90;
    ecx = (ecx & 0xffff0000u) | ((rd16(m, 0x96c688u)) & 0xffffu);
    wr16(m, eax, (ecx & 0xffffu));
    edx = (edx & 0xffffff00u) | ((rd8(m, 0x96c68au)) & 0xffu);
    wr8(m, eax + 0x2u, (edx & 0xffu));
    goto L_747c5a;
L_747bb0:
    eax = (esp + 0x14u);
    esp -= 4; wr32(m, esp, 0x9420f4u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(esi, 0x747bbcu);
    esp = esp + 0x8u;
    fR_747bbf = (eax & eax);
    if (!(fR_747bbf == 0)) goto L_747be3;
    eax = ebp;
    eax = (eax - 1u);
L_747bc6:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_747bca = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_747bca == 0)) goto L_747bc6;
    ecx = (ecx & 0xffff0000u) | ((rd16(m, 0x96c68cu)) & 0xffffu);
    wr16(m, eax, (ecx & 0xffffu));
    edx = (edx & 0xffffff00u) | ((rd8(m, 0x96c68eu)) & 0xffu);
    wr8(m, eax + 0x2u, (edx & 0xffu));
    goto L_747c5a;
L_747be3:
    eax = (esp + 0x14u);
    esp -= 4; wr32(m, esp, 0x9420f8u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(esi, 0x747befu);
    esp = esp + 0x8u;
    fR_747bf2 = (eax & eax);
    if (!(fR_747bf2 == 0)) goto L_747c1d;
    eax = ebp;
    eax = (eax - 1u);
L_747c00:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_747c04 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_747c04 == 0)) goto L_747c00;
    ecx = (ecx & 0xffff0000u) | ((rd16(m, 0x96c690u)) & 0xffffu);
    wr16(m, eax, (ecx & 0xffffu));
    edx = (edx & 0xffffff00u) | ((rd8(m, 0x96c692u)) & 0xffu);
    wr8(m, eax + 0x2u, (edx & 0xffu));
    goto L_747c5a;
L_747c1d:
    eax = (esp + 0x14u);
    esp -= 4; wr32(m, esp, 0x93c734u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(esi, 0x747c29u);
    esp = esp + 0x8u;
    fR_747c2c = (eax & eax);
    if (!(fR_747c2c == 0)) goto L_747c45;
    edi = ebp;
    edi = (edi - 1u);
L_747c33:
    eax = (eax & 0xffffff00u) | ((rd8(m, edi + 0x1u)) & 0xffu);
    edi = (edi + 1u);
    fR_747c37 = ((eax & 0xffu) & (eax & 0xffu));
    if (!(fR_747c37 == 0)) goto L_747c33;
    ecx = rd32(m, 0x93c734u) /* 7.928105302133553e-39 */;
    wr32(m, edi, ecx);
    goto L_747c5a;
L_747c45:
    edi = ebp;
    edi = (edi - 1u);
L_747c48:
    eax = (eax & 0xffffff00u) | ((rd8(m, edi + 0x1u)) & 0xffu);
    edi = (edi + 1u);
    fR_747c4c = ((eax & 0xffu) & (eax & 0xffu));
    if (!(fR_747c4c == 0)) goto L_747c48;
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c6a8u)) & 0xffffu);
    wr16(m, edi, (edx & 0xffffu));
L_747c5a:
    edi = ebp;
    edi = (edi - 1u);
L_747c60:
    eax = (eax & 0xffffff00u) | ((rd8(m, edi + 0x1u)) & 0xffu);
    edi = (edi + 1u);
    fR_747c64 = ((eax & 0xffu) & (eax & 0xffu));
    if (!(fR_747c64 == 0)) goto L_747c60;
    eax = (eax & 0xffff0000u) | ((rd16(m, 0x94f118u)) & 0xffffu);
    wr16(m, edi, (eax & 0xffffu));
    eax = rd32(m, ebx + 0x234u);
    eax = eax + 0x20030u;
    edx = 0x96c6acu;
    ecx = eax;
L_747c83:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_747c85 = (ebx & 0xffu);
    fB_747c85 = rd8(m, edx);
    FL = fl_sub(fA_747c85, fB_747c85, 1);
    if (fA_747c85 != fB_747c85) goto L_747ca3;
    fR_747c89 = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_747c89 == 0)) goto L_747c9f;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_747c90 = (ebx & 0xffu);
    fB_747c90 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_747c90, fB_747c90, 1);
    if (fA_747c90 != fB_747c90) goto L_747ca3;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_747c9b = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_747c9b == 0)) goto L_747c83;
L_747c9f:
    ecx = 0u;
    goto L_747ca8;
L_747ca3:
    fA_747ca3 = ecx; fB_747ca3 = ecx; fK_747ca3 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_747ca3 - fB_747ca3 - fK_747ca3;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_747ca3) < uint64_t(fB_747ca3) + uint64_t(fK_747ca3)) ? 1u : 0u);
L_747ca8:
    fR_747ca8 = (ecx & ecx);
    if (!(fR_747ca8 == 0)) goto L_747cd0;
    eax = ebp;
    eax = (eax - 1u);
L_747cb0:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_747cb4 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_747cb4 == 0)) goto L_747cb0;
    ecx = rd32(m, 0x96c6b8u) /* 13.956116676330566 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c6bcu)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_747cd0:
    edx = 0x96c6c0u;
    ecx = eax;
L_747cd7:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_747cd9 = (ebx & 0xffu);
    fB_747cd9 = rd8(m, edx);
    FL = fl_sub(fA_747cd9, fB_747cd9, 1);
    if (fA_747cd9 != fB_747cd9) goto L_747cf7;
    fR_747cdd = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_747cdd == 0)) goto L_747cf3;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_747ce4 = (ebx & 0xffu);
    fB_747ce4 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_747ce4, fB_747ce4, 1);
    if (fA_747ce4 != fB_747ce4) goto L_747cf7;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_747cef = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_747cef == 0)) goto L_747cd7;
L_747cf3:
    ecx = 0u;
    goto L_747cfc;
L_747cf7:
    fA_747cf7 = ecx; fB_747cf7 = ecx; fK_747cf7 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_747cf7 - fB_747cf7 - fK_747cf7;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_747cf7) < uint64_t(fB_747cf7) + uint64_t(fK_747cf7)) ? 1u : 0u);
L_747cfc:
    fR_747cfc = (ecx & ecx);
    if (!(fR_747cfc == 0)) goto L_747d23;
    eax = ebp;
    eax = (eax - 1u);
L_747d03:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_747d07 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_747d07 == 0)) goto L_747d03;
    ecx = rd32(m, 0x96c6ccu) /* 13.9551420211792 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c6d0u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_747d23:
    edx = 0x96c6d4u;
    ecx = eax;
L_747d30:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_747d32 = (ebx & 0xffu);
    fB_747d32 = rd8(m, edx);
    FL = fl_sub(fA_747d32, fB_747d32, 1);
    if (fA_747d32 != fB_747d32) goto L_747d50;
    fR_747d36 = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_747d36 == 0)) goto L_747d4c;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_747d3d = (ebx & 0xffu);
    fB_747d3d = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_747d3d, fB_747d3d, 1);
    if (fA_747d3d != fB_747d3d) goto L_747d50;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_747d48 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_747d48 == 0)) goto L_747d30;
L_747d4c:
    ecx = 0u;
    goto L_747d55;
L_747d50:
    fA_747d50 = ecx; fB_747d50 = ecx; fK_747d50 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_747d50 - fB_747d50 - fK_747d50;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_747d50) < uint64_t(fB_747d50) + uint64_t(fK_747d50)) ? 1u : 0u);
L_747d55:
    fR_747d55 = (ecx & ecx);
    if (!(fR_747d55 == 0)) goto L_747d80;
    eax = ebp;
    eax = (eax - 1u);
L_747d60:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_747d64 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_747d64 == 0)) goto L_747d60;
    ecx = rd32(m, 0x96c6e0u) /* 13.955389976501465 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c6e4u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_747d80:
    edx = 0x96c6e8u;
    ecx = eax;
L_747d87:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_747d89 = (ebx & 0xffu);
    fB_747d89 = rd8(m, edx);
    FL = fl_sub(fA_747d89, fB_747d89, 1);
    if (fA_747d89 != fB_747d89) goto L_747da7;
    fR_747d8d = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_747d8d == 0)) goto L_747da3;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_747d94 = (ebx & 0xffu);
    fB_747d94 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_747d94, fB_747d94, 1);
    if (fA_747d94 != fB_747d94) goto L_747da7;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_747d9f = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_747d9f == 0)) goto L_747d87;
L_747da3:
    ecx = 0u;
    goto L_747dac;
L_747da7:
    fA_747da7 = ecx; fB_747da7 = ecx; fK_747da7 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_747da7 - fB_747da7 - fK_747da7;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_747da7) < uint64_t(fB_747da7) + uint64_t(fK_747da7)) ? 1u : 0u);
L_747dac:
    fR_747dac = (ecx & ecx);
    if (!(fR_747dac == 0)) goto L_747dd3;
    eax = ebp;
    eax = (eax - 1u);
L_747db3:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_747db7 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_747db7 == 0)) goto L_747db3;
    ecx = rd32(m, 0x96c6f8u) /* 13.958559036254883 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c6fcu)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_747dd3:
    edx = 0x96c700u;
    ecx = eax;
L_747de0:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_747de2 = (ebx & 0xffu);
    fB_747de2 = rd8(m, edx);
    FL = fl_sub(fA_747de2, fB_747de2, 1);
    if (fA_747de2 != fB_747de2) goto L_747e00;
    fR_747de6 = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_747de6 == 0)) goto L_747dfc;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_747ded = (ebx & 0xffu);
    fB_747ded = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_747ded, fB_747ded, 1);
    if (fA_747ded != fB_747ded) goto L_747e00;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_747df8 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_747df8 == 0)) goto L_747de0;
L_747dfc:
    ecx = 0u;
    goto L_747e05;
L_747e00:
    fA_747e00 = ecx; fB_747e00 = ecx; fK_747e00 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_747e00 - fB_747e00 - fK_747e00;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_747e00) < uint64_t(fB_747e00) + uint64_t(fK_747e00)) ? 1u : 0u);
L_747e05:
    fR_747e05 = (ecx & ecx);
    if (!(fR_747e05 == 0)) goto L_747e30;
    eax = ebp;
    eax = (eax - 1u);
L_747e10:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_747e14 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_747e14 == 0)) goto L_747e10;
    ecx = rd32(m, 0x96c710u) /* 55.83423614501953 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c714u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_747e30:
    edx = 0x96c718u;
    ecx = eax;
L_747e37:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_747e39 = (ebx & 0xffu);
    fB_747e39 = rd8(m, edx);
    FL = fl_sub(fA_747e39, fB_747e39, 1);
    if (fA_747e39 != fB_747e39) goto L_747e57;
    fR_747e3d = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_747e3d == 0)) goto L_747e53;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_747e44 = (ebx & 0xffu);
    fB_747e44 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_747e44, fB_747e44, 1);
    if (fA_747e44 != fB_747e44) goto L_747e57;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_747e4f = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_747e4f == 0)) goto L_747e37;
L_747e53:
    ecx = 0u;
    goto L_747e5c;
L_747e57:
    fA_747e57 = ecx; fB_747e57 = ecx; fK_747e57 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_747e57 - fB_747e57 - fK_747e57;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_747e57) < uint64_t(fB_747e57) + uint64_t(fK_747e57)) ? 1u : 0u);
L_747e5c:
    fR_747e5c = (ecx & ecx);
    if (!(fR_747e5c == 0)) goto L_747e83;
    eax = ebp;
    eax = (eax - 1u);
L_747e63:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_747e67 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_747e67 == 0)) goto L_747e63;
    ecx = rd32(m, 0x96c728u) /* 223.33694458007812 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c72cu)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_747e83:
    edx = 0x96c730u;
    ecx = eax;
L_747e90:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_747e92 = (ebx & 0xffu);
    fB_747e92 = rd8(m, edx);
    FL = fl_sub(fA_747e92, fB_747e92, 1);
    if (fA_747e92 != fB_747e92) goto L_747eb0;
    fR_747e96 = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_747e96 == 0)) goto L_747eac;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_747e9d = (ebx & 0xffu);
    fB_747e9d = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_747e9d, fB_747e9d, 1);
    if (fA_747e9d != fB_747e9d) goto L_747eb0;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_747ea8 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_747ea8 == 0)) goto L_747e90;
L_747eac:
    ecx = 0u;
    goto L_747eb5;
L_747eb0:
    fA_747eb0 = ecx; fB_747eb0 = ecx; fK_747eb0 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_747eb0 - fB_747eb0 - fK_747eb0;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_747eb0) < uint64_t(fB_747eb0) + uint64_t(fK_747eb0)) ? 1u : 0u);
L_747eb5:
    fR_747eb5 = (ecx & ecx);
    if (!(fR_747eb5 == 0)) goto L_747ee0;
    eax = ebp;
    eax = (eax - 1u);
L_747ec0:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_747ec4 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_747ec4 == 0)) goto L_747ec0;
    ecx = rd32(m, 0x96c73cu) /* 13.956851959228516 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c740u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_747ee0:
    edx = 0x96c744u;
    ecx = eax;
L_747ee7:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_747ee9 = (ebx & 0xffu);
    fB_747ee9 = rd8(m, edx);
    FL = fl_sub(fA_747ee9, fB_747ee9, 1);
    if (fA_747ee9 != fB_747ee9) goto L_747f07;
    fR_747eed = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_747eed == 0)) goto L_747f03;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_747ef4 = (ebx & 0xffu);
    fB_747ef4 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_747ef4, fB_747ef4, 1);
    if (fA_747ef4 != fB_747ef4) goto L_747f07;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_747eff = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_747eff == 0)) goto L_747ee7;
L_747f03:
    ecx = 0u;
    goto L_747f0c;
L_747f07:
    fA_747f07 = ecx; fB_747f07 = ecx; fK_747f07 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_747f07 - fB_747f07 - fK_747f07;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_747f07) < uint64_t(fB_747f07) + uint64_t(fK_747f07)) ? 1u : 0u);
L_747f0c:
    fR_747f0c = (ecx & ecx);
    if (!(fR_747f0c == 0)) goto L_747f33;
    eax = ebp;
    eax = (eax - 1u);
L_747f13:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_747f17 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_747f17 == 0)) goto L_747f13;
    ecx = rd32(m, 0x96c750u) /* 55.82740783691406 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c754u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_747f33:
    edx = 0x96c758u;
    ecx = eax;
L_747f40:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_747f42 = (ebx & 0xffu);
    fB_747f42 = rd8(m, edx);
    FL = fl_sub(fA_747f42, fB_747f42, 1);
    if (fA_747f42 != fB_747f42) goto L_747f60;
    fR_747f46 = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_747f46 == 0)) goto L_747f5c;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_747f4d = (ebx & 0xffu);
    fB_747f4d = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_747f4d, fB_747f4d, 1);
    if (fA_747f4d != fB_747f4d) goto L_747f60;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_747f58 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_747f58 == 0)) goto L_747f40;
L_747f5c:
    ecx = 0u;
    goto L_747f65;
L_747f60:
    fA_747f60 = ecx; fB_747f60 = ecx; fK_747f60 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_747f60 - fB_747f60 - fK_747f60;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_747f60) < uint64_t(fB_747f60) + uint64_t(fK_747f60)) ? 1u : 0u);
L_747f65:
    fR_747f65 = (ecx & ecx);
    if (!(fR_747f65 == 0)) goto L_747f90;
    eax = ebp;
    eax = (eax - 1u);
L_747f70:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_747f74 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_747f74 == 0)) goto L_747f70;
    ecx = rd32(m, 0x96c764u) /* 13.954408645629883 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c768u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_747f90:
    edx = 0x96c76cu;
    ecx = eax;
L_747f97:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_747f99 = (ebx & 0xffu);
    fB_747f99 = rd8(m, edx);
    FL = fl_sub(fA_747f99, fB_747f99, 1);
    if (fA_747f99 != fB_747f99) goto L_747fb7;
    fR_747f9d = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_747f9d == 0)) goto L_747fb3;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_747fa4 = (ebx & 0xffu);
    fB_747fa4 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_747fa4, fB_747fa4, 1);
    if (fA_747fa4 != fB_747fa4) goto L_747fb7;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_747faf = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_747faf == 0)) goto L_747f97;
L_747fb3:
    ecx = 0u;
    goto L_747fbc;
L_747fb7:
    fA_747fb7 = ecx; fB_747fb7 = ecx; fK_747fb7 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_747fb7 - fB_747fb7 - fK_747fb7;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_747fb7) < uint64_t(fB_747fb7) + uint64_t(fK_747fb7)) ? 1u : 0u);
L_747fbc:
    fR_747fbc = (ecx & ecx);
    if (!(fR_747fbc == 0)) goto L_747fe3;
    eax = ebp;
    eax = (eax - 1u);
L_747fc3:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_747fc7 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_747fc7 == 0)) goto L_747fc3;
    ecx = rd32(m, 0x96c77cu) /* 55.81763458251953 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c780u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_747fe3:
    edx = 0x96c784u;
    ecx = eax;
L_747ff0:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_747ff2 = (ebx & 0xffu);
    fB_747ff2 = rd8(m, edx);
    FL = fl_sub(fA_747ff2, fB_747ff2, 1);
    if (fA_747ff2 != fB_747ff2) goto L_748010;
    fR_747ff6 = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_747ff6 == 0)) goto L_74800c;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_747ffd = (ebx & 0xffu);
    fB_747ffd = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_747ffd, fB_747ffd, 1);
    if (fA_747ffd != fB_747ffd) goto L_748010;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_748008 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_748008 == 0)) goto L_747ff0;
L_74800c:
    ecx = 0u;
    goto L_748015;
L_748010:
    fA_748010 = ecx; fB_748010 = ecx; fK_748010 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748010 - fB_748010 - fK_748010;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748010) < uint64_t(fB_748010) + uint64_t(fK_748010)) ? 1u : 0u);
L_748015:
    fR_748015 = (ecx & ecx);
    if (!(fR_748015 == 0)) goto L_748040;
    eax = ebp;
    eax = (eax - 1u);
L_748020:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748024 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748024 == 0)) goto L_748020;
    ecx = rd32(m, 0x96c790u) /* 13.9568510055542 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c794u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_748040:
    edx = 0x96c798u;
    ecx = eax;
L_748047:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748049 = (ebx & 0xffu);
    fB_748049 = rd8(m, edx);
    FL = fl_sub(fA_748049, fB_748049, 1);
    if (fA_748049 != fB_748049) goto L_748067;
    fR_74804d = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_74804d == 0)) goto L_748063;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_748054 = (ebx & 0xffu);
    fB_748054 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_748054, fB_748054, 1);
    if (fA_748054 != fB_748054) goto L_748067;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_74805f = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_74805f == 0)) goto L_748047;
L_748063:
    ecx = 0u;
    goto L_74806c;
L_748067:
    fA_748067 = ecx; fB_748067 = ecx; fK_748067 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748067 - fB_748067 - fK_748067;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748067) < uint64_t(fB_748067) + uint64_t(fK_748067)) ? 1u : 0u);
L_74806c:
    fR_74806c = (ecx & ecx);
    if (!(fR_74806c == 0)) goto L_748093;
    eax = ebp;
    eax = (eax - 1u);
L_748073:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748077 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748077 == 0)) goto L_748073;
    ecx = rd32(m, 0x96c7a8u) /* 55.8274040222168 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c7acu)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_748093:
    edx = 0x96c7b0u;
    ecx = eax;
L_7480a0:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_7480a2 = (ebx & 0xffu);
    fB_7480a2 = rd8(m, edx);
    FL = fl_sub(fA_7480a2, fB_7480a2, 1);
    if (fA_7480a2 != fB_7480a2) goto L_7480c0;
    fR_7480a6 = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_7480a6 == 0)) goto L_7480bc;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_7480ad = (ebx & 0xffu);
    fB_7480ad = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_7480ad, fB_7480ad, 1);
    if (fA_7480ad != fB_7480ad) goto L_7480c0;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_7480b8 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_7480b8 == 0)) goto L_7480a0;
L_7480bc:
    ecx = 0u;
    goto L_7480c5;
L_7480c0:
    fA_7480c0 = ecx; fB_7480c0 = ecx; fK_7480c0 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_7480c0 - fB_7480c0 - fK_7480c0;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_7480c0) < uint64_t(fB_7480c0) + uint64_t(fK_7480c0)) ? 1u : 0u);
L_7480c5:
    fR_7480c5 = (ecx & ecx);
    if (!(fR_7480c5 == 0)) goto L_7480f0;
    eax = ebp;
    eax = (eax - 1u);
L_7480d0:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_7480d4 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_7480d4 == 0)) goto L_7480d0;
    ecx = rd32(m, 0x96c7bcu) /* 13.953433990478516 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c7c0u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_7480f0:
    edx = 0x96c7c4u;
    ecx = eax;
L_7480f7:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_7480f9 = (ebx & 0xffu);
    fB_7480f9 = rd8(m, edx);
    FL = fl_sub(fA_7480f9, fB_7480f9, 1);
    if (fA_7480f9 != fB_7480f9) goto L_748117;
    fR_7480fd = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_7480fd == 0)) goto L_748113;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_748104 = (ebx & 0xffu);
    fB_748104 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_748104, fB_748104, 1);
    if (fA_748104 != fB_748104) goto L_748117;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_74810f = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_74810f == 0)) goto L_7480f7;
L_748113:
    ecx = 0u;
    goto L_74811c;
L_748117:
    fA_748117 = ecx; fB_748117 = ecx; fK_748117 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748117 - fB_748117 - fK_748117;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748117) < uint64_t(fB_748117) + uint64_t(fK_748117)) ? 1u : 0u);
L_74811c:
    fR_74811c = (ecx & ecx);
    if (!(fR_74811c == 0)) goto L_748143;
    eax = ebp;
    eax = (eax - 1u);
L_748123:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748127 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748127 == 0)) goto L_748123;
    ecx = rd32(m, 0x96c7d0u) /* 55.81373596191406 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c7d4u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_748143:
    edx = 0x96c7d8u;
    ecx = eax;
L_748150:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748152 = (ebx & 0xffu);
    fB_748152 = rd8(m, edx);
    FL = fl_sub(fA_748152, fB_748152, 1);
    if (fA_748152 != fB_748152) goto L_748170;
    fR_748156 = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_748156 == 0)) goto L_74816c;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_74815d = (ebx & 0xffu);
    fB_74815d = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_74815d, fB_74815d, 1);
    if (fA_74815d != fB_74815d) goto L_748170;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_748168 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_748168 == 0)) goto L_748150;
L_74816c:
    ecx = 0u;
    goto L_748175;
L_748170:
    fA_748170 = ecx; fB_748170 = ecx; fK_748170 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748170 - fB_748170 - fK_748170;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748170) < uint64_t(fB_748170) + uint64_t(fK_748170)) ? 1u : 0u);
L_748175:
    fR_748175 = (ecx & ecx);
    if (!(fR_748175 == 0)) goto L_7481a0;
    eax = ebp;
    eax = (eax - 1u);
L_748180:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748184 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748184 == 0)) goto L_748180;
    ecx = rd32(m, 0x96c7e4u) /* 13.954423904418945 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c7e8u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_7481a0:
    edx = 0x96c7ecu;
    ecx = eax;
L_7481a7:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_7481a9 = (ebx & 0xffu);
    fB_7481a9 = rd8(m, edx);
    FL = fl_sub(fA_7481a9, fB_7481a9, 1);
    if (fA_7481a9 != fB_7481a9) goto L_7481c7;
    fR_7481ad = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_7481ad == 0)) goto L_7481c3;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_7481b4 = (ebx & 0xffu);
    fB_7481b4 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_7481b4, fB_7481b4, 1);
    if (fA_7481b4 != fB_7481b4) goto L_7481c7;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_7481bf = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_7481bf == 0)) goto L_7481a7;
L_7481c3:
    ecx = 0u;
    goto L_7481cc;
L_7481c7:
    fA_7481c7 = ecx; fB_7481c7 = ecx; fK_7481c7 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_7481c7 - fB_7481c7 - fK_7481c7;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_7481c7) < uint64_t(fB_7481c7) + uint64_t(fK_7481c7)) ? 1u : 0u);
L_7481cc:
    fR_7481cc = (ecx & ecx);
    if (!(fR_7481cc == 0)) goto L_7481f3;
    eax = ebp;
    eax = (eax - 1u);
L_7481d3:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_7481d7 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_7481d7 == 0)) goto L_7481d3;
    ecx = rd32(m, 0x96c7f8u) /* 55.81769561767578 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c7fcu)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_7481f3:
    edx = 0x96c800u;
    ecx = eax;
L_748200:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748202 = (ebx & 0xffu);
    fB_748202 = rd8(m, edx);
    FL = fl_sub(fA_748202, fB_748202, 1);
    if (fA_748202 != fB_748202) goto L_748220;
    fR_748206 = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_748206 == 0)) goto L_74821c;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_74820d = (ebx & 0xffu);
    fB_74820d = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_74820d, fB_74820d, 1);
    if (fA_74820d != fB_74820d) goto L_748220;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_748218 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_748218 == 0)) goto L_748200;
L_74821c:
    ecx = 0u;
    goto L_748225;
L_748220:
    fA_748220 = ecx; fB_748220 = ecx; fK_748220 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748220 - fB_748220 - fK_748220;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748220) < uint64_t(fB_748220) + uint64_t(fK_748220)) ? 1u : 0u);
L_748225:
    fR_748225 = (ecx & ecx);
    if (!(fR_748225 == 0)) goto L_748250;
    eax = ebp;
    eax = (eax - 1u);
L_748230:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748234 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748234 == 0)) goto L_748230;
    ecx = rd32(m, 0x96c810u) /* 13.958086967468262 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c814u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_748250:
    edx = 0x96c818u;
    ecx = eax;
L_748257:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748259 = (ebx & 0xffu);
    fB_748259 = rd8(m, edx);
    FL = fl_sub(fA_748259, fB_748259, 1);
    if (fA_748259 != fB_748259) goto L_748277;
    fR_74825d = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_74825d == 0)) goto L_748273;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_748264 = (ebx & 0xffu);
    fB_748264 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_748264, fB_748264, 1);
    if (fA_748264 != fB_748264) goto L_748277;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_74826f = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_74826f == 0)) goto L_748257;
L_748273:
    ecx = 0u;
    goto L_74827c;
L_748277:
    fA_748277 = ecx; fB_748277 = ecx; fK_748277 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748277 - fB_748277 - fK_748277;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748277) < uint64_t(fB_748277) + uint64_t(fK_748277)) ? 1u : 0u);
L_74827c:
    fR_74827c = (ecx & ecx);
    if (!(fR_74827c == 0)) goto L_7482a3;
    eax = ebp;
    eax = (eax - 1u);
L_748283:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748287 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748287 == 0)) goto L_748283;
    ecx = rd32(m, 0x96c828u) /* 55.83234786987305 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c82cu)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_7482a3:
    edx = 0x96c830u;
    ecx = eax;
L_7482b0:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_7482b2 = (ebx & 0xffu);
    fB_7482b2 = rd8(m, edx);
    FL = fl_sub(fA_7482b2, fB_7482b2, 1);
    if (fA_7482b2 != fB_7482b2) goto L_7482d0;
    fR_7482b6 = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_7482b6 == 0)) goto L_7482cc;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_7482bd = (ebx & 0xffu);
    fB_7482bd = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_7482bd, fB_7482bd, 1);
    if (fA_7482bd != fB_7482bd) goto L_7482d0;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_7482c8 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_7482c8 == 0)) goto L_7482b0;
L_7482cc:
    ecx = 0u;
    goto L_7482d5;
L_7482d0:
    fA_7482d0 = ecx; fB_7482d0 = ecx; fK_7482d0 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_7482d0 - fB_7482d0 - fK_7482d0;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_7482d0) < uint64_t(fB_7482d0) + uint64_t(fK_7482d0)) ? 1u : 0u);
L_7482d5:
    fR_7482d5 = (ecx & ecx);
    if (!(fR_7482d5 == 0)) goto L_748300;
    eax = ebp;
    eax = (eax - 1u);
L_7482e0:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_7482e4 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_7482e4 == 0)) goto L_7482e0;
    ecx = rd32(m, 0x96c83cu) /* 13.953441619873047 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c840u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_748300:
    edx = 0x96c844u;
    ecx = eax;
L_748307:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748309 = (ebx & 0xffu);
    fB_748309 = rd8(m, edx);
    FL = fl_sub(fA_748309, fB_748309, 1);
    if (fA_748309 != fB_748309) goto L_748327;
    fR_74830d = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_74830d == 0)) goto L_748323;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_748314 = (ebx & 0xffu);
    fB_748314 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_748314, fB_748314, 1);
    if (fA_748314 != fB_748314) goto L_748327;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_74831f = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_74831f == 0)) goto L_748307;
L_748323:
    ecx = 0u;
    goto L_74832c;
L_748327:
    fA_748327 = ecx; fB_748327 = ecx; fK_748327 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748327 - fB_748327 - fK_748327;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748327) < uint64_t(fB_748327) + uint64_t(fK_748327)) ? 1u : 0u);
L_74832c:
    fR_74832c = (ecx & ecx);
    if (!(fR_74832c == 0)) goto L_748353;
    eax = ebp;
    eax = (eax - 1u);
L_748333:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748337 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748337 == 0)) goto L_748333;
    ecx = rd32(m, 0x96c854u) /* 13.953441619873047 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c858u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_748353:
    edx = 0x96c85cu;
    ecx = eax;
L_748360:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748362 = (ebx & 0xffu);
    fB_748362 = rd8(m, edx);
    FL = fl_sub(fA_748362, fB_748362, 1);
    if (fA_748362 != fB_748362) goto L_748380;
    fR_748366 = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_748366 == 0)) goto L_74837c;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_74836d = (ebx & 0xffu);
    fB_74836d = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_74836d, fB_74836d, 1);
    if (fA_74836d != fB_74836d) goto L_748380;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_748378 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_748378 == 0)) goto L_748360;
L_74837c:
    ecx = 0u;
    goto L_748385;
L_748380:
    fA_748380 = ecx; fB_748380 = ecx; fK_748380 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748380 - fB_748380 - fK_748380;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748380) < uint64_t(fB_748380) + uint64_t(fK_748380)) ? 1u : 0u);
L_748385:
    fR_748385 = (ecx & ecx);
    if (!(fR_748385 == 0)) goto L_7483b0;
    eax = ebp;
    eax = (eax - 1u);
L_748390:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748394 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748394 == 0)) goto L_748390;
    ecx = rd32(m, 0x96c868u) /* 55.81376647949219 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c86cu)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_7483b0:
    edx = 0x96c870u;
    ecx = eax;
L_7483b7:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_7483b9 = (ebx & 0xffu);
    fB_7483b9 = rd8(m, edx);
    FL = fl_sub(fA_7483b9, fB_7483b9, 1);
    if (fA_7483b9 != fB_7483b9) goto L_7483d7;
    fR_7483bd = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_7483bd == 0)) goto L_7483d3;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_7483c4 = (ebx & 0xffu);
    fB_7483c4 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_7483c4, fB_7483c4, 1);
    if (fA_7483c4 != fB_7483c4) goto L_7483d7;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_7483cf = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_7483cf == 0)) goto L_7483b7;
L_7483d3:
    ecx = 0u;
    goto L_7483dc;
L_7483d7:
    fA_7483d7 = ecx; fB_7483d7 = ecx; fK_7483d7 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_7483d7 - fB_7483d7 - fK_7483d7;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_7483d7) < uint64_t(fB_7483d7) + uint64_t(fK_7483d7)) ? 1u : 0u);
L_7483dc:
    fR_7483dc = (ecx & ecx);
    if (!(fR_7483dc == 0)) goto L_748403;
    eax = ebp;
    eax = (eax - 1u);
L_7483e3:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_7483e7 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_7483e7 == 0)) goto L_7483e3;
    ecx = rd32(m, 0x96c880u) /* 55.81376647949219 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c884u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_748403:
    edx = 0x96c888u;
    ecx = eax;
L_748410:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748412 = (ebx & 0xffu);
    fB_748412 = rd8(m, edx);
    FL = fl_sub(fA_748412, fB_748412, 1);
    if (fA_748412 != fB_748412) goto L_748430;
    fR_748416 = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_748416 == 0)) goto L_74842c;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_74841d = (ebx & 0xffu);
    fB_74841d = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_74841d, fB_74841d, 1);
    if (fA_74841d != fB_74841d) goto L_748430;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_748428 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_748428 == 0)) goto L_748410;
L_74842c:
    ecx = 0u;
    goto L_748435;
L_748430:
    fA_748430 = ecx; fB_748430 = ecx; fK_748430 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748430 - fB_748430 - fK_748430;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748430) < uint64_t(fB_748430) + uint64_t(fK_748430)) ? 1u : 0u);
L_748435:
    fR_748435 = (ecx & ecx);
    if (!(fR_748435 == 0)) goto L_748460;
    eax = ebp;
    eax = (eax - 1u);
L_748440:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748444 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748444 == 0)) goto L_748440;
    ecx = rd32(m, 0x96c898u) /* 13.954168319702148 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c89cu)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_748460:
    edx = 0x96c8a0u;
    ecx = eax;
L_748467:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748469 = (ebx & 0xffu);
    fB_748469 = rd8(m, edx);
    FL = fl_sub(fA_748469, fB_748469, 1);
    if (fA_748469 != fB_748469) goto L_748487;
    fR_74846d = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_74846d == 0)) goto L_748483;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_748474 = (ebx & 0xffu);
    fB_748474 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_748474, fB_748474, 1);
    if (fA_748474 != fB_748474) goto L_748487;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_74847f = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_74847f == 0)) goto L_748467;
L_748483:
    ecx = 0u;
    goto L_74848c;
L_748487:
    fA_748487 = ecx; fB_748487 = ecx; fK_748487 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748487 - fB_748487 - fK_748487;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748487) < uint64_t(fB_748487) + uint64_t(fK_748487)) ? 1u : 0u);
L_74848c:
    fR_74848c = (ecx & ecx);
    if (!(fR_74848c == 0)) goto L_7484b3;
    eax = ebp;
    eax = (eax - 1u);
L_748493:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748497 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748497 == 0)) goto L_748493;
    ecx = rd32(m, 0x96c8b0u) /* 55.816673278808594 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c8b4u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_7484b3:
    edx = 0x96c8b8u;
    ecx = eax;
L_7484c0:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_7484c2 = (ebx & 0xffu);
    fB_7484c2 = rd8(m, edx);
    FL = fl_sub(fA_7484c2, fB_7484c2, 1);
    if (fA_7484c2 != fB_7484c2) goto L_7484e0;
    fR_7484c6 = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_7484c6 == 0)) goto L_7484dc;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_7484cd = (ebx & 0xffu);
    fB_7484cd = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_7484cd, fB_7484cd, 1);
    if (fA_7484cd != fB_7484cd) goto L_7484e0;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_7484d8 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_7484d8 == 0)) goto L_7484c0;
L_7484dc:
    ecx = 0u;
    goto L_7484e5;
L_7484e0:
    fA_7484e0 = ecx; fB_7484e0 = ecx; fK_7484e0 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_7484e0 - fB_7484e0 - fK_7484e0;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_7484e0) < uint64_t(fB_7484e0) + uint64_t(fK_7484e0)) ? 1u : 0u);
L_7484e5:
    fR_7484e5 = (ecx & ecx);
    if (!(fR_7484e5 == 0)) goto L_748510;
    eax = ebp;
    eax = (eax - 1u);
L_7484f0:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_7484f4 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_7484f4 == 0)) goto L_7484f0;
    ecx = rd32(m, 0x96c8c4u) /* 13.957586288452148 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c8c8u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_748510:
    edx = 0x96c8ccu;
    ecx = eax;
L_748517:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748519 = (ebx & 0xffu);
    fB_748519 = rd8(m, edx);
    FL = fl_sub(fA_748519, fB_748519, 1);
    if (fA_748519 != fB_748519) goto L_748537;
    fR_74851d = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_74851d == 0)) goto L_748533;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_748524 = (ebx & 0xffu);
    fB_748524 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_748524, fB_748524, 1);
    if (fA_748524 != fB_748524) goto L_748537;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_74852f = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_74852f == 0)) goto L_748517;
L_748533:
    ecx = 0u;
    goto L_74853c;
L_748537:
    fA_748537 = ecx; fB_748537 = ecx; fK_748537 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748537 - fB_748537 - fK_748537;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748537) < uint64_t(fB_748537) + uint64_t(fK_748537)) ? 1u : 0u);
L_74853c:
    fR_74853c = (ecx & ecx);
    if (!(fR_74853c == 0)) goto L_748563;
    eax = ebp;
    eax = (eax - 1u);
L_748543:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748547 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748547 == 0)) goto L_748543;
    ecx = rd32(m, 0x96c8d8u) /* 55.830345153808594 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c8dcu)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_748563:
    edx = 0x96c8e0u;
    ecx = eax;
L_748570:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748572 = (ebx & 0xffu);
    fB_748572 = rd8(m, edx);
    FL = fl_sub(fA_748572, fB_748572, 1);
    if (fA_748572 != fB_748572) goto L_748590;
    fR_748576 = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_748576 == 0)) goto L_74858c;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_74857d = (ebx & 0xffu);
    fB_74857d = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_74857d, fB_74857d, 1);
    if (fA_74857d != fB_74857d) goto L_748590;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_748588 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_748588 == 0)) goto L_748570;
L_74858c:
    ecx = 0u;
    goto L_748595;
L_748590:
    fA_748590 = ecx; fB_748590 = ecx; fK_748590 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748590 - fB_748590 - fK_748590;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748590) < uint64_t(fB_748590) + uint64_t(fK_748590)) ? 1u : 0u);
L_748595:
    fR_748595 = (ecx & ecx);
    if (!(fR_748595 == 0)) goto L_7485c0;
    eax = ebp;
    eax = (eax - 1u);
L_7485a0:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_7485a4 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_7485a4 == 0)) goto L_7485a0;
    ecx = rd32(m, 0x96c8ecu) /* 13.95539379119873 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c8f0u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_7485c0:
    edx = 0x96c8f4u;
    ecx = eax;
L_7485c7:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_7485c9 = (ebx & 0xffu);
    fB_7485c9 = rd8(m, edx);
    FL = fl_sub(fA_7485c9, fB_7485c9, 1);
    if (fA_7485c9 != fB_7485c9) goto L_7485ea;
    fR_7485cd = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_7485cd == 0)) goto L_7485e3;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_7485d4 = (ebx & 0xffu);
    fB_7485d4 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_7485d4, fB_7485d4, 1);
    if (fA_7485d4 != fB_7485d4) goto L_7485ea;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_7485df = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_7485df == 0)) goto L_7485c7;
L_7485e3:
    ecx = 0u;
    goto L_7485ef;
L_7485ea:
    fA_7485ea = ecx; fB_7485ea = ecx; fK_7485ea = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_7485ea - fB_7485ea - fK_7485ea;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_7485ea) < uint64_t(fB_7485ea) + uint64_t(fK_7485ea)) ? 1u : 0u);
L_7485ef:
    fR_7485ef = (ecx & ecx);
    if (!(fR_7485ef == 0)) goto L_748616;
    eax = ebp;
    eax = (eax - 1u);
L_7485f6:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_7485fa = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_7485fa == 0)) goto L_7485f6;
    ecx = rd32(m, 0x96c904u) /* 55.82157516479492 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c908u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_748616:
    edx = 0x96c90cu;
    ecx = eax;
L_748620:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748622 = (ebx & 0xffu);
    fB_748622 = rd8(m, edx);
    FL = fl_sub(fA_748622, fB_748622, 1);
    if (fA_748622 != fB_748622) goto L_748643;
    fR_748626 = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_748626 == 0)) goto L_74863c;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_74862d = (ebx & 0xffu);
    fB_74862d = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_74862d, fB_74862d, 1);
    if (fA_74862d != fB_74862d) goto L_748643;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_748638 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_748638 == 0)) goto L_748620;
L_74863c:
    ecx = 0u;
    goto L_748648;
L_748643:
    fA_748643 = ecx; fB_748643 = ecx; fK_748643 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748643 - fB_748643 - fK_748643;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748643) < uint64_t(fB_748643) + uint64_t(fK_748643)) ? 1u : 0u);
L_748648:
    fR_748648 = (ecx & ecx);
    if (!(fR_748648 == 0)) goto L_748670;
    eax = ebp;
    eax = (eax - 1u);
L_748650:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748654 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748654 == 0)) goto L_748650;
    ecx = rd32(m, 0x96c8ecu) /* 13.95539379119873 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c8f0u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_748670:
    edx = 0x96c91cu;
    ecx = eax;
L_748677:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748679 = (ebx & 0xffu);
    fB_748679 = rd8(m, edx);
    FL = fl_sub(fA_748679, fB_748679, 1);
    if (fA_748679 != fB_748679) goto L_74869a;
    fR_74867d = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_74867d == 0)) goto L_748693;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_748684 = (ebx & 0xffu);
    fB_748684 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_748684, fB_748684, 1);
    if (fA_748684 != fB_748684) goto L_74869a;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_74868f = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_74868f == 0)) goto L_748677;
L_748693:
    ecx = 0u;
    goto L_74869f;
L_74869a:
    fA_74869a = ecx; fB_74869a = ecx; fK_74869a = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_74869a - fB_74869a - fK_74869a;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_74869a) < uint64_t(fB_74869a) + uint64_t(fK_74869a)) ? 1u : 0u);
L_74869f:
    fR_74869f = (ecx & ecx);
    if (!(fR_74869f == 0)) goto L_7486c6;
    eax = ebp;
    eax = (eax - 1u);
L_7486a6:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_7486aa = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_7486aa == 0)) goto L_7486a6;
    ecx = rd32(m, 0x96c92cu) /* 13.953442573547363 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c930u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_7486c6:
    edx = 0x96c934u;
    ecx = eax;
L_7486d0:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_7486d2 = (ebx & 0xffu);
    fB_7486d2 = rd8(m, edx);
    FL = fl_sub(fA_7486d2, fB_7486d2, 1);
    if (fA_7486d2 != fB_7486d2) goto L_7486f3;
    fR_7486d6 = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_7486d6 == 0)) goto L_7486ec;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_7486dd = (ebx & 0xffu);
    fB_7486dd = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_7486dd, fB_7486dd, 1);
    if (fA_7486dd != fB_7486dd) goto L_7486f3;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_7486e8 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_7486e8 == 0)) goto L_7486d0;
L_7486ec:
    ecx = 0u;
    goto L_7486f8;
L_7486f3:
    fA_7486f3 = ecx; fB_7486f3 = ecx; fK_7486f3 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_7486f3 - fB_7486f3 - fK_7486f3;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_7486f3) < uint64_t(fB_7486f3) + uint64_t(fK_7486f3)) ? 1u : 0u);
L_7486f8:
    fR_7486f8 = (ecx & ecx);
    if (!(fR_7486f8 == 0)) goto L_748720;
    eax = ebp;
    eax = (eax - 1u);
L_748700:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748704 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748704 == 0)) goto L_748700;
    ecx = rd32(m, 0x96c944u) /* 55.81377029418945 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c948u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_748720:
    edx = 0x96c94cu;
    ecx = eax;
L_748727:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748729 = (ebx & 0xffu);
    fB_748729 = rd8(m, edx);
    FL = fl_sub(fA_748729, fB_748729, 1);
    if (fA_748729 != fB_748729) goto L_74874a;
    fR_74872d = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_74872d == 0)) goto L_748743;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_748734 = (ebx & 0xffu);
    fB_748734 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_748734, fB_748734, 1);
    if (fA_748734 != fB_748734) goto L_74874a;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_74873f = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_74873f == 0)) goto L_748727;
L_748743:
    ecx = 0u;
    goto L_74874f;
L_74874a:
    fA_74874a = ecx; fB_74874a = ecx; fK_74874a = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_74874a - fB_74874a - fK_74874a;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_74874a) < uint64_t(fB_74874a) + uint64_t(fK_74874a)) ? 1u : 0u);
L_74874f:
    fR_74874f = (ecx & ecx);
    if (!(fR_74874f == 0)) goto L_748776;
    eax = ebp;
    eax = (eax - 1u);
L_748756:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_74875a = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_74875a == 0)) goto L_748756;
    ecx = rd32(m, 0x96c95cu) /* 13.957110404968262 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c960u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_748776:
    edx = 0x96c964u;
    ecx = eax;
L_748780:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748782 = (ebx & 0xffu);
    fB_748782 = rd8(m, edx);
    FL = fl_sub(fA_748782, fB_748782, 1);
    if (fA_748782 != fB_748782) goto L_7487a7;
    fR_748786 = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_748786 == 0)) goto L_7487a0;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_74878d = (ebx & 0xffu);
    fB_74878d = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_74878d, fB_74878d, 1);
    if (fA_74878d != fB_74878d) goto L_7487a7;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_74879c = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_74879c == 0)) goto L_748780;
L_7487a0:
    ecx = 0u;
    goto L_7487ac;
L_7487a7:
    fA_7487a7 = ecx; fB_7487a7 = ecx; fK_7487a7 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_7487a7 - fB_7487a7 - fK_7487a7;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_7487a7) < uint64_t(fB_7487a7) + uint64_t(fK_7487a7)) ? 1u : 0u);
L_7487ac:
    fR_7487ac = (ecx & ecx);
    if (!(fR_7487ac == 0)) goto L_7487c4;
    eax = ebp;
    eax = (eax - 1u);
L_7487b7:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_7487bb = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_7487bb == 0)) goto L_7487b7;
    goto L_74886f;
L_7487c4:
    edx = 0x96c97cu;
    ecx = eax;
    goto L_7487d0;
L_7487d0:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_7487d2 = (ebx & 0xffu);
    fB_7487d2 = rd8(m, edx);
    FL = fl_sub(fA_7487d2, fB_7487d2, 1);
    if (fA_7487d2 != fB_7487d2) goto L_7487f7;
    fR_7487d6 = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_7487d6 == 0)) goto L_7487f0;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_7487dd = (ebx & 0xffu);
    fB_7487dd = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_7487dd, fB_7487dd, 1);
    if (fA_7487dd != fB_7487dd) goto L_7487f7;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_7487ec = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_7487ec == 0)) goto L_7487d0;
L_7487f0:
    ecx = 0u;
    goto L_7487fc;
L_7487f7:
    fA_7487f7 = ecx; fB_7487f7 = ecx; fK_7487f7 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_7487f7 - fB_7487f7 - fK_7487f7;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_7487f7) < uint64_t(fB_7487f7) + uint64_t(fK_7487f7)) ? 1u : 0u);
L_7487fc:
    fR_7487fc = (ecx & ecx);
    if (!(fR_7487fc == 0)) goto L_748823;
    eax = ebp;
    eax = (eax - 1u);
L_748803:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748807 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748807 == 0)) goto L_748803;
    ecx = rd32(m, 0x96c95cu) /* 13.957110404968262 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c960u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_748823:
    edx = 0x96c98cu;
    ecx = eax;
L_748830:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748832 = (ebx & 0xffu);
    fB_748832 = rd8(m, edx);
    FL = fl_sub(fA_748832, fB_748832, 1);
    if (fA_748832 != fB_748832) goto L_74885b;
    fR_748836 = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_748836 == 0)) goto L_748854;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_748841 = (ebx & 0xffu);
    fB_748841 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_748841, fB_748841, 1);
    if (fA_748841 != fB_748841) goto L_74885b;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_748850 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_748850 == 0)) goto L_748830;
L_748854:
    ecx = 0u;
    goto L_748860;
L_74885b:
    fA_74885b = ecx; fB_74885b = ecx; fK_74885b = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_74885b - fB_74885b - fK_74885b;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_74885b) < uint64_t(fB_74885b) + uint64_t(fK_74885b)) ? 1u : 0u);
L_748860:
    fR_748860 = (ecx & ecx);
    if (!(fR_748860 == 0)) goto L_748887;
    eax = ebp;
    eax = (eax - 1u);
L_748867:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_74886b = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_74886b == 0)) goto L_748867;
L_74886f:
    ecx = rd32(m, 0x96c974u) /* 55.82844161987305 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c978u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_748887:
    edx = 0x96c9a0u;
    ecx = eax;
    edi = edi;
L_748890:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748892 = (ebx & 0xffu);
    fB_748892 = rd8(m, edx);
    FL = fl_sub(fA_748892, fB_748892, 1);
    if (fA_748892 != fB_748892) goto L_7488bb;
    fR_748896 = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_748896 == 0)) goto L_7488b4;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_7488a1 = (ebx & 0xffu);
    fB_7488a1 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_7488a1, fB_7488a1, 1);
    if (fA_7488a1 != fB_7488a1) goto L_7488bb;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_7488b0 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_7488b0 == 0)) goto L_748890;
L_7488b4:
    ecx = 0u;
    goto L_7488c0;
L_7488bb:
    fA_7488bb = ecx; fB_7488bb = ecx; fK_7488bb = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_7488bb - fB_7488bb - fK_7488bb;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_7488bb) < uint64_t(fB_7488bb) + uint64_t(fK_7488bb)) ? 1u : 0u);
L_7488c0:
    fR_7488c0 = (ecx & ecx);
    if (!(fR_7488c0 == 0)) goto L_7488e7;
    eax = ebp;
    eax = (eax - 1u);
L_7488c7:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_7488cb = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_7488cb == 0)) goto L_7488c7;
    ecx = rd32(m, 0x96c9acu) /* 13.953437805175781 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c9b0u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_7488e7:
    edx = 0x96c9b4u;
    ecx = eax;
    edi = edi;
L_7488f0:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_7488f2 = (ebx & 0xffu);
    fB_7488f2 = rd8(m, edx);
    FL = fl_sub(fA_7488f2, fB_7488f2, 1);
    if (fA_7488f2 != fB_7488f2) goto L_74891b;
    fR_7488f6 = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_7488f6 == 0)) goto L_748914;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_748901 = (ebx & 0xffu);
    fB_748901 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_748901, fB_748901, 1);
    if (fA_748901 != fB_748901) goto L_74891b;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_748910 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_748910 == 0)) goto L_7488f0;
L_748914:
    ecx = 0u;
    goto L_748920;
L_74891b:
    fA_74891b = ecx; fB_74891b = ecx; fK_74891b = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_74891b - fB_74891b - fK_74891b;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_74891b) < uint64_t(fB_74891b) + uint64_t(fK_74891b)) ? 1u : 0u);
L_748920:
    fR_748920 = (ecx & ecx);
    if (!(fR_748920 == 0)) goto L_748947;
    eax = ebp;
    eax = (eax - 1u);
L_748927:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_74892b = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_74892b == 0)) goto L_748927;
    ecx = rd32(m, 0x96c9c4u) /* 13.953437805175781 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c9c8u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_748947:
    edx = 0x96c9ccu;
    ecx = eax;
    edi = edi;
L_748950:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748952 = (ebx & 0xffu);
    fB_748952 = rd8(m, edx);
    FL = fl_sub(fA_748952, fB_748952, 1);
    if (fA_748952 != fB_748952) goto L_74897f;
    fR_74895a = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_74895a == 0)) goto L_748978;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_748965 = (ebx & 0xffu);
    fB_748965 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_748965, fB_748965, 1);
    if (fA_748965 != fB_748965) goto L_74897f;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_748974 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_748974 == 0)) goto L_748950;
L_748978:
    ecx = 0u;
    goto L_748984;
L_74897f:
    fA_74897f = ecx; fB_74897f = ecx; fK_74897f = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_74897f - fB_74897f - fK_74897f;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_74897f) < uint64_t(fB_74897f) + uint64_t(fK_74897f)) ? 1u : 0u);
L_748984:
    fR_748984 = (ecx & ecx);
    if (!(fR_748984 == 0)) goto L_7489b0;
    eax = ebp;
    eax = (eax - 1u);
    goto L_748990;
L_748990:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748994 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748994 == 0)) goto L_748990;
    ecx = rd32(m, 0x96c9dcu) /* 13.959300994873047 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c9e0u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_7489b0:
    edx = 0x96c9e4u;
    ecx = eax;
    goto L_7489c0;
L_7489c0:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_7489c2 = (ebx & 0xffu);
    fB_7489c2 = rd8(m, edx);
    FL = fl_sub(fA_7489c2, fB_7489c2, 1);
    if (fA_7489c2 != fB_7489c2) goto L_7489ef;
    fR_7489ca = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_7489ca == 0)) goto L_7489e8;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_7489d5 = (ebx & 0xffu);
    fB_7489d5 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_7489d5, fB_7489d5, 1);
    if (fA_7489d5 != fB_7489d5) goto L_7489ef;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_7489e4 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_7489e4 == 0)) goto L_7489c0;
L_7489e8:
    ecx = 0u;
    goto L_7489f4;
L_7489ef:
    fA_7489ef = ecx; fB_7489ef = ecx; fK_7489ef = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_7489ef - fB_7489ef - fK_7489ef;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_7489ef) < uint64_t(fB_7489ef) + uint64_t(fK_7489ef)) ? 1u : 0u);
L_7489f4:
    fR_7489f4 = (ecx & ecx);
    if (!(fR_7489f4 == 0)) goto L_748a20;
    eax = ebp;
    eax = (eax - 1u);
L_748a00:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748a04 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748a04 == 0)) goto L_748a00;
    ecx = rd32(m, 0x96c9f4u) /* 13.959300994873047 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c9f8u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_748a20:
    edx = 0x96c9fcu;
    ecx = eax;
    goto L_748a30;
L_748a30:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748a32 = (ebx & 0xffu);
    fB_748a32 = rd8(m, edx);
    FL = fl_sub(fA_748a32, fB_748a32, 1);
    if (fA_748a32 != fB_748a32) goto L_748a5f;
    fR_748a3a = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_748a3a == 0)) goto L_748a58;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_748a45 = (ebx & 0xffu);
    fB_748a45 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_748a45, fB_748a45, 1);
    if (fA_748a45 != fB_748a45) goto L_748a5f;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_748a54 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_748a54 == 0)) goto L_748a30;
L_748a58:
    ecx = 0u;
    goto L_748a64;
L_748a5f:
    fA_748a5f = ecx; fB_748a5f = ecx; fK_748a5f = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748a5f - fB_748a5f - fK_748a5f;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748a5f) < uint64_t(fB_748a5f) + uint64_t(fK_748a5f)) ? 1u : 0u);
L_748a64:
    fR_748a64 = (ecx & ecx);
    if (!(fR_748a64 == 0)) goto L_748a90;
    eax = ebp;
    eax = (eax - 1u);
L_748a70:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748a74 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748a74 == 0)) goto L_748a70;
    ecx = rd32(m, 0x96ca08u) /* 13.954427719116211 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96ca0cu)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_748a90:
    edx = 0x96ca10u;
    ecx = eax;
    goto L_748aa0;
L_748aa0:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748aa2 = (ebx & 0xffu);
    fB_748aa2 = rd8(m, edx);
    FL = fl_sub(fA_748aa2, fB_748aa2, 1);
    if (fA_748aa2 != fB_748aa2) goto L_748acf;
    fR_748aaa = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_748aaa == 0)) goto L_748ac8;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_748ab5 = (ebx & 0xffu);
    fB_748ab5 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_748ab5, fB_748ab5, 1);
    if (fA_748ab5 != fB_748ab5) goto L_748acf;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_748ac4 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_748ac4 == 0)) goto L_748aa0;
L_748ac8:
    ecx = 0u;
    goto L_748ad4;
L_748acf:
    fA_748acf = ecx; fB_748acf = ecx; fK_748acf = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748acf - fB_748acf - fK_748acf;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748acf) < uint64_t(fB_748acf) + uint64_t(fK_748acf)) ? 1u : 0u);
L_748ad4:
    fR_748ad4 = (ecx & ecx);
    if (!(fR_748ad4 == 0)) goto L_748b00;
    eax = ebp;
    eax = (eax - 1u);
L_748ae0:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748ae4 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748ae4 == 0)) goto L_748ae0;
    ecx = rd32(m, 0x96ca1cu) /* 13.954427719116211 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96ca20u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_748b00:
    edx = 0x93de84u;
    ecx = eax;
    goto L_748b10;
L_748b10:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748b12 = (ebx & 0xffu);
    fB_748b12 = rd8(m, edx);
    FL = fl_sub(fA_748b12, fB_748b12, 1);
    if (fA_748b12 != fB_748b12) goto L_748b3f;
    fR_748b1a = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_748b1a == 0)) goto L_748b38;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_748b25 = (ebx & 0xffu);
    fB_748b25 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_748b25, fB_748b25, 1);
    if (fA_748b25 != fB_748b25) goto L_748b3f;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_748b34 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_748b34 == 0)) goto L_748b10;
L_748b38:
    ecx = 0u;
    goto L_748b44;
L_748b3f:
    fA_748b3f = ecx; fB_748b3f = ecx; fK_748b3f = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748b3f - fB_748b3f - fK_748b3f;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748b3f) < uint64_t(fB_748b3f) + uint64_t(fK_748b3f)) ? 1u : 0u);
L_748b44:
    fR_748b44 = (ecx & ecx);
    if (!(fR_748b44 == 0)) goto L_748b70;
    eax = ebp;
    eax = (eax - 1u);
L_748b50:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748b54 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748b54 == 0)) goto L_748b50;
    ecx = rd32(m, 0x96ca24u) /* 13.955403327941895 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96ca28u)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    goto L_749103;
L_748b70:
    edx = 0x96ca2cu;
    ecx = eax;
    goto L_748b80;
L_748b80:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748b82 = (ebx & 0xffu);
    fB_748b82 = rd8(m, edx);
    FL = fl_sub(fA_748b82, fB_748b82, 1);
    if (fA_748b82 != fB_748b82) goto L_748baf;
    fR_748b8a = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_748b8a == 0)) goto L_748ba8;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_748b95 = (ebx & 0xffu);
    fB_748b95 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_748b95, fB_748b95, 1);
    if (fA_748b95 != fB_748b95) goto L_748baf;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_748ba4 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_748ba4 == 0)) goto L_748b80;
L_748ba8:
    ecx = 0u;
    goto L_748bb4;
L_748baf:
    fA_748baf = ecx; fB_748baf = ecx; fK_748baf = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748baf - fB_748baf - fK_748baf;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748baf) < uint64_t(fB_748baf) + uint64_t(fK_748baf)) ? 1u : 0u);
L_748bb4:
    fR_748bb4 = (ecx & ecx);
    if (!(fR_748bb4 == 0)) goto L_748be9;
    eax = ebp;
    eax = (eax - 1u);
L_748bc0:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748bc4 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748bc4 == 0)) goto L_748bc0;
    ecx = rd32(m, 0x96ca38u) /* 1.479411696821443e+19 */;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96ca3cu)) & 0xffffu);
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    ecx = (ecx & 0xffffff00u) | ((rd8(m, 0x96ca3eu)) & 0xffu);
    wr8(m, eax + 0x6u, (ecx & 0xffu));
    goto L_749103;
L_748be9:
    edx = 0x96ca40u;
    ecx = eax;
L_748bf0:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748bf2 = (ebx & 0xffu);
    fB_748bf2 = rd8(m, edx);
    FL = fl_sub(fA_748bf2, fB_748bf2, 1);
    if (fA_748bf2 != fB_748bf2) goto L_748c1f;
    fR_748bfa = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_748bfa == 0)) goto L_748c18;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_748c05 = (ebx & 0xffu);
    fB_748c05 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_748c05, fB_748c05, 1);
    if (fA_748c05 != fB_748c05) goto L_748c1f;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_748c14 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_748c14 == 0)) goto L_748bf0;
L_748c18:
    ecx = 0u;
    goto L_748c24;
L_748c1f:
    fA_748c1f = ecx; fB_748c1f = ecx; fK_748c1f = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748c1f - fB_748c1f - fK_748c1f;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748c1f) < uint64_t(fB_748c1f) + uint64_t(fK_748c1f)) ? 1u : 0u);
L_748c24:
    fR_748c24 = (ecx & ecx);
    if (!(fR_748c24 == 0)) goto L_748c50;
    eax = ebp;
    eax = (eax - 1u);
L_748c30:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748c34 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748c34 == 0)) goto L_748c30;
    edx = rd32(m, 0x96ca4cu) /* 13.955157279968262 */;
    wr32(m, eax, edx);
    ecx = (ecx & 0xffff0000u) | ((rd16(m, 0x96ca50u)) & 0xffffu);
    wr16(m, eax + 0x4u, (ecx & 0xffffu));
    goto L_749103;
L_748c50:
    edx = 0x96ca54u;
    ecx = eax;
    goto L_748c60;
L_748c60:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748c62 = (ebx & 0xffu);
    fB_748c62 = rd8(m, edx);
    FL = fl_sub(fA_748c62, fB_748c62, 1);
    if (fA_748c62 != fB_748c62) goto L_748c8f;
    fR_748c6a = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_748c6a == 0)) goto L_748c88;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_748c75 = (ebx & 0xffu);
    fB_748c75 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_748c75, fB_748c75, 1);
    if (fA_748c75 != fB_748c75) goto L_748c8f;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_748c84 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_748c84 == 0)) goto L_748c60;
L_748c88:
    ecx = 0u;
    goto L_748c94;
L_748c8f:
    fA_748c8f = ecx; fB_748c8f = ecx; fK_748c8f = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748c8f - fB_748c8f - fK_748c8f;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748c8f) < uint64_t(fB_748c8f) + uint64_t(fK_748c8f)) ? 1u : 0u);
L_748c94:
    fR_748c94 = (ecx & ecx);
    if (!(fR_748c94 == 0)) goto L_748cc0;
    eax = ebp;
    eax = (eax - 1u);
L_748ca0:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748ca4 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748ca4 == 0)) goto L_748ca0;
    edx = rd32(m, 0x96ca64u) /* 13.955157279968262 */;
    wr32(m, eax, edx);
    ecx = (ecx & 0xffff0000u) | ((rd16(m, 0x96ca68u)) & 0xffffu);
    wr16(m, eax + 0x4u, (ecx & 0xffffu));
    goto L_749103;
L_748cc0:
    edx = 0x96ca6cu;
    ecx = eax;
    goto L_748cd0;
L_748cd0:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748cd2 = (ebx & 0xffu);
    fB_748cd2 = rd8(m, edx);
    FL = fl_sub(fA_748cd2, fB_748cd2, 1);
    if (fA_748cd2 != fB_748cd2) goto L_748cff;
    fR_748cda = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_748cda == 0)) goto L_748cf8;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_748ce5 = (ebx & 0xffu);
    fB_748ce5 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_748ce5, fB_748ce5, 1);
    if (fA_748ce5 != fB_748ce5) goto L_748cff;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_748cf4 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_748cf4 == 0)) goto L_748cd0;
L_748cf8:
    ecx = 0u;
    goto L_748d04;
L_748cff:
    fA_748cff = ecx; fB_748cff = ecx; fK_748cff = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748cff - fB_748cff - fK_748cff;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748cff) < uint64_t(fB_748cff) + uint64_t(fK_748cff)) ? 1u : 0u);
L_748d04:
    fR_748d04 = (ecx & ecx);
    if (!(fR_748d04 == 0)) goto L_748d30;
    eax = ebp;
    eax = (eax - 1u);
L_748d10:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748d14 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748d14 == 0)) goto L_748d10;
    edx = rd32(m, 0x96ca7cu) /* 55.82062911987305 */;
    wr32(m, eax, edx);
    ecx = (ecx & 0xffff0000u) | ((rd16(m, 0x96ca80u)) & 0xffffu);
    wr16(m, eax + 0x4u, (ecx & 0xffffu));
    goto L_749103;
L_748d30:
    edx = 0x96ca84u;
    ecx = eax;
    goto L_748d40;
L_748d40:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748d42 = (ebx & 0xffu);
    fB_748d42 = rd8(m, edx);
    FL = fl_sub(fA_748d42, fB_748d42, 1);
    if (fA_748d42 != fB_748d42) goto L_748d6f;
    fR_748d4a = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_748d4a == 0)) goto L_748d68;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_748d55 = (ebx & 0xffu);
    fB_748d55 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_748d55, fB_748d55, 1);
    if (fA_748d55 != fB_748d55) goto L_748d6f;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_748d64 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_748d64 == 0)) goto L_748d40;
L_748d68:
    ecx = 0u;
    goto L_748d74;
L_748d6f:
    fA_748d6f = ecx; fB_748d6f = ecx; fK_748d6f = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748d6f - fB_748d6f - fK_748d6f;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748d6f) < uint64_t(fB_748d6f) + uint64_t(fK_748d6f)) ? 1u : 0u);
L_748d74:
    fR_748d74 = (ecx & ecx);
    if (!(fR_748d74 == 0)) goto L_748d8d;
    eax = ebp;
    eax = (eax - 1u);
L_748d80:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748d84 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748d84 == 0)) goto L_748d80;
    goto L_748f28;
L_748d8d:
    edx = 0x96ca9cu;
    ecx = eax;
L_748d94:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748d96 = (ebx & 0xffu);
    fB_748d96 = rd8(m, edx);
    FL = fl_sub(fA_748d96, fB_748d96, 1);
    if (fA_748d96 != fB_748d96) goto L_748dc3;
    fR_748d9e = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_748d9e == 0)) goto L_748dbc;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_748da9 = (ebx & 0xffu);
    fB_748da9 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_748da9, fB_748da9, 1);
    if (fA_748da9 != fB_748da9) goto L_748dc3;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_748db8 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_748db8 == 0)) goto L_748d94;
L_748dbc:
    ecx = 0u;
    goto L_748dc8;
L_748dc3:
    fA_748dc3 = ecx; fB_748dc3 = ecx; fK_748dc3 = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748dc3 - fB_748dc3 - fK_748dc3;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748dc3) < uint64_t(fB_748dc3) + uint64_t(fK_748dc3)) ? 1u : 0u);
L_748dc8:
    fR_748dc8 = (ecx & ecx);
    if (!(fR_748dc8 == 0)) goto L_748df3;
    eax = ebp;
    eax = (eax - 1u);
L_748dd3:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748dd7 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748dd7 == 0)) goto L_748dd3;
    edx = rd32(m, 0x96ca4cu) /* 13.955157279968262 */;
    wr32(m, eax, edx);
    ecx = (ecx & 0xffff0000u) | ((rd16(m, 0x96ca50u)) & 0xffffu);
    wr16(m, eax + 0x4u, (ecx & 0xffffu));
    goto L_749103;
L_748df3:
    edx = 0x96caa8u;
    ecx = eax;
L_748e00:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748e02 = (ebx & 0xffu);
    fB_748e02 = rd8(m, edx);
    FL = fl_sub(fA_748e02, fB_748e02, 1);
    if (fA_748e02 != fB_748e02) goto L_748e2f;
    fR_748e0a = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_748e0a == 0)) goto L_748e28;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_748e15 = (ebx & 0xffu);
    fB_748e15 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_748e15, fB_748e15, 1);
    if (fA_748e15 != fB_748e15) goto L_748e2f;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_748e24 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_748e24 == 0)) goto L_748e00;
L_748e28:
    ecx = 0u;
    goto L_748e34;
L_748e2f:
    fA_748e2f = ecx; fB_748e2f = ecx; fK_748e2f = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748e2f - fB_748e2f - fK_748e2f;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748e2f) < uint64_t(fB_748e2f) + uint64_t(fK_748e2f)) ? 1u : 0u);
L_748e34:
    fR_748e34 = (ecx & ecx);
    if (!(fR_748e34 == 0)) goto L_748e60;
    eax = ebp;
    eax = (eax - 1u);
L_748e40:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748e44 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748e44 == 0)) goto L_748e40;
    edx = rd32(m, 0x96ca64u) /* 13.955157279968262 */;
    wr32(m, eax, edx);
    ecx = (ecx & 0xffff0000u) | ((rd16(m, 0x96ca68u)) & 0xffffu);
    wr16(m, eax + 0x4u, (ecx & 0xffffu));
    goto L_749103;
L_748e60:
    edx = 0x96cab8u;
    ecx = eax;
    goto L_748e70;
L_748e70:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748e72 = (ebx & 0xffu);
    fB_748e72 = rd8(m, edx);
    FL = fl_sub(fA_748e72, fB_748e72, 1);
    if (fA_748e72 != fB_748e72) goto L_748e9f;
    fR_748e7a = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_748e7a == 0)) goto L_748e98;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_748e85 = (ebx & 0xffu);
    fB_748e85 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_748e85, fB_748e85, 1);
    if (fA_748e85 != fB_748e85) goto L_748e9f;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_748e94 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_748e94 == 0)) goto L_748e70;
L_748e98:
    ecx = 0u;
    goto L_748ea4;
L_748e9f:
    fA_748e9f = ecx; fB_748e9f = ecx; fK_748e9f = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748e9f - fB_748e9f - fK_748e9f;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748e9f) < uint64_t(fB_748e9f) + uint64_t(fK_748e9f)) ? 1u : 0u);
L_748ea4:
    fR_748ea4 = (ecx & ecx);
    if (!(fR_748ea4 == 0)) goto L_748ed0;
    eax = ebp;
    eax = (eax - 1u);
L_748eb0:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748eb4 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748eb4 == 0)) goto L_748eb0;
    edx = rd32(m, 0x96ca7cu) /* 55.82062911987305 */;
    wr32(m, eax, edx);
    ecx = (ecx & 0xffff0000u) | ((rd16(m, 0x96ca80u)) & 0xffffu);
    wr16(m, eax + 0x4u, (ecx & 0xffffu));
    goto L_749103;
L_748ed0:
    edx = 0x96cac4u;
    ecx = eax;
    goto L_748ee0;
L_748ee0:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748ee2 = (ebx & 0xffu);
    fB_748ee2 = rd8(m, edx);
    FL = fl_sub(fA_748ee2, fB_748ee2, 1);
    if (fA_748ee2 != fB_748ee2) goto L_748f0f;
    fR_748eea = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_748eea == 0)) goto L_748f08;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_748ef5 = (ebx & 0xffu);
    fB_748ef5 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_748ef5, fB_748ef5, 1);
    if (fA_748ef5 != fB_748ef5) goto L_748f0f;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_748f04 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_748f04 == 0)) goto L_748ee0;
L_748f08:
    ecx = 0u;
    goto L_748f14;
L_748f0f:
    fA_748f0f = ecx; fB_748f0f = ecx; fK_748f0f = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748f0f - fB_748f0f - fK_748f0f;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748f0f) < uint64_t(fB_748f0f) + uint64_t(fK_748f0f)) ? 1u : 0u);
L_748f14:
    fR_748f14 = (ecx & ecx);
    if (!(fR_748f14 == 0)) goto L_748f40;
    eax = ebp;
    eax = (eax - 1u);
L_748f20:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748f24 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748f24 == 0)) goto L_748f20;
L_748f28:
    edx = rd32(m, 0x96ca94u) /* 55.82062911987305 */;
    wr32(m, eax, edx);
    ecx = (ecx & 0xffff0000u) | ((rd16(m, 0x96ca98u)) & 0xffffu);
    wr16(m, eax + 0x4u, (ecx & 0xffffu));
    goto L_749103;
L_748f40:
    edx = 0x96cad4u;
    ecx = eax;
    goto L_748f50;
L_748f50:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748f52 = (ebx & 0xffu);
    fB_748f52 = rd8(m, edx);
    FL = fl_sub(fA_748f52, fB_748f52, 1);
    if (fA_748f52 != fB_748f52) goto L_748f7f;
    fR_748f5a = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_748f5a == 0)) goto L_748f78;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_748f65 = (ebx & 0xffu);
    fB_748f65 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_748f65, fB_748f65, 1);
    if (fA_748f65 != fB_748f65) goto L_748f7f;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_748f74 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_748f74 == 0)) goto L_748f50;
L_748f78:
    ecx = 0u;
    goto L_748f84;
L_748f7f:
    fA_748f7f = ecx; fB_748f7f = ecx; fK_748f7f = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748f7f - fB_748f7f - fK_748f7f;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748f7f) < uint64_t(fB_748f7f) + uint64_t(fK_748f7f)) ? 1u : 0u);
L_748f84:
    fR_748f84 = (ecx & ecx);
    if (!(fR_748f84 == 0)) goto L_748fb0;
    eax = ebp;
    eax = (eax - 1u);
L_748f90:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_748f94 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_748f94 == 0)) goto L_748f90;
    edx = rd32(m, 0x96c6b8u) /* 13.956116676330566 */;
    wr32(m, eax, edx);
    ecx = (ecx & 0xffff0000u) | ((rd16(m, 0x96c6bcu)) & 0xffffu);
    wr16(m, eax + 0x4u, (ecx & 0xffffu));
    goto L_749103;
L_748fb0:
    edx = 0x96cae0u;
    ecx = eax;
    goto L_748fc0;
L_748fc0:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_748fc2 = (ebx & 0xffu);
    fB_748fc2 = rd8(m, edx);
    FL = fl_sub(fA_748fc2, fB_748fc2, 1);
    if (fA_748fc2 != fB_748fc2) goto L_748fef;
    fR_748fca = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_748fca == 0)) goto L_748fe8;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_748fd5 = (ebx & 0xffu);
    fB_748fd5 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_748fd5, fB_748fd5, 1);
    if (fA_748fd5 != fB_748fd5) goto L_748fef;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_748fe4 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_748fe4 == 0)) goto L_748fc0;
L_748fe8:
    ecx = 0u;
    goto L_748ff4;
L_748fef:
    fA_748fef = ecx; fB_748fef = ecx; fK_748fef = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_748fef - fB_748fef - fK_748fef;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_748fef) < uint64_t(fB_748fef) + uint64_t(fK_748fef)) ? 1u : 0u);
L_748ff4:
    fR_748ff4 = (ecx & ecx);
    if (!(fR_748ff4 == 0)) goto L_749020;
    eax = ebp;
    eax = (eax - 1u);
L_749000:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_749004 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_749004 == 0)) goto L_749000;
    edx = rd32(m, 0x96c6ccu) /* 13.9551420211792 */;
    wr32(m, eax, edx);
    ecx = (ecx & 0xffff0000u) | ((rd16(m, 0x96c6d0u)) & 0xffffu);
    wr16(m, eax + 0x4u, (ecx & 0xffffu));
    goto L_749103;
L_749020:
    edx = 0x96caecu;
    ecx = eax;
    goto L_749030;
L_749030:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fA_749032 = (ebx & 0xffu);
    fB_749032 = rd8(m, edx);
    FL = fl_sub(fA_749032, fB_749032, 1);
    if (fA_749032 != fB_749032) goto L_74905f;
    fR_74903a = ((ebx & 0xffu) & (ebx & 0xffu));
    if ((fR_74903a == 0)) goto L_749058;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx + 0x1u)) & 0xffu);
    fA_749045 = (ebx & 0xffu);
    fB_749045 = rd8(m, edx + 0x1u);
    FL = fl_sub(fA_749045, fB_749045, 1);
    if (fA_749045 != fB_749045) goto L_74905f;
    ecx = ecx + 0x2u;
    edx = edx + 0x2u;
    fR_749054 = ((ebx & 0xffu) & (ebx & 0xffu));
    if (!(fR_749054 == 0)) goto L_749030;
L_749058:
    ecx = 0u;
    goto L_749064;
L_74905f:
    fA_74905f = ecx; fB_74905f = ecx; fK_74905f = (fl_cc(2, FL)) ? 1u : 0u;
    ecx = fA_74905f - fB_74905f - fK_74905f;
    ecx = ecx - 0xffffffffu - ((uint64_t(fA_74905f) < uint64_t(fB_74905f) + uint64_t(fK_74905f)) ? 1u : 0u);
L_749064:
    fR_749064 = (ecx & ecx);
    if (!(fR_749064 == 0)) goto L_74908d;
    eax = ebp;
    eax = (eax - 1u);
L_749070:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_749074 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_749074 == 0)) goto L_749070;
    edx = rd32(m, 0x96c6e0u) /* 13.955389976501465 */;
    wr32(m, eax, edx);
    ecx = (ecx & 0xffff0000u) | ((rd16(m, 0x96c6e4u)) & 0xffffu);
    wr16(m, eax + 0x4u, (ecx & 0xffffu));
    goto L_749103;
L_74908d:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    fR_74908f = ((ecx & 0xffu) & (ecx & 0xffu));
    if ((fR_74908f == 0)) goto L_7490a8;
L_749097:
    fA_749097 = (ecx & 0xffu);
    fB_749097 = 0x5fu;
    if (fA_749097 == fB_749097) goto L_7490ad;
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_7490a4 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_7490a4 == 0)) goto L_749097;
L_7490a8:
    fA_7490a8 = rd8(m, eax);
    fB_7490a8 = 0x5fu;
    if (fA_7490a8 != fB_7490a8) goto L_7490f0;
L_7490ad:
    edx = rd32(m, eax + 0x1u);
    eax = (eax + 1u);
    eax = (eax & 0xffffff00u) | ((rd8(m, eax + 0x4u)) & 0xffu);
    wr8(m, esp + 0x18u, (eax & 0xffu));
    eax = (esp + 0x14u);
    wr32(m, esp + 0x14u, edx);
    wr8(m, esp + 0x19u, 0x0u);
    ecx = eax;
L_7490c7:
    edx = (edx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    eax = (eax + 1u);
    fR_7490ca = ((edx & 0xffu) & (edx & 0xffu));
    if (!(fR_7490ca == 0)) goto L_7490c7;
    edi = ebp;
    eax = eax - ecx;
    esi = ecx;
    edi = (edi - 1u);
L_7490d5:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, edi + 0x1u)) & 0xffu);
    edi = (edi + 1u);
    fR_7490d9 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_7490d9 == 0)) goto L_7490d5;
    ecx = eax;
    ecx = (ecx >> 2);
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    ecx = eax;
    ecx = (ecx & 0x3u);
    while (ecx) { wr8(m, edi, rd8(m, esi)); esi += 1; edi += 1; ecx--; }
    goto L_749103;
L_7490f0:
    edi = ebp;
    edi = (edi - 1u);
L_7490f3:
    eax = (eax & 0xffffff00u) | ((rd8(m, edi + 0x1u)) & 0xffu);
    edi = (edi + 1u);
    fR_7490f7 = ((eax & 0xffu) & (eax & 0xffu));
    if (!(fR_7490f7 == 0)) goto L_7490f3;
    ecx = rd32(m, 0x96caf8u) /* 8.115726554820467e-39 */;
    wr32(m, edi, ecx);
L_749103:
    edi = ebp;
    edi = (edi - 1u);
L_749106:
    eax = (eax & 0xffffff00u) | ((rd8(m, edi + 0x1u)) & 0xffu);
    edi = (edi + 1u);
    fR_74910a = ((eax & 0xffu) & (eax & 0xffu));
    if (!(fR_74910a == 0)) goto L_749106;
    eax = rd32(m, esp + 0x28u);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x94f118u)) & 0xffffu);
    eax = (eax - 1u);
    wr16(m, edi, (edx & 0xffffu));
    fA_74911d = eax;
    fB_74911d = 0xdu;
    if (fA_74911d > fB_74911d) goto L_749492;
    switch (eax) { case 0: goto L_74912d; case 1: goto L_749160; case 2: goto L_74919c; case 3: goto L_7491eb; case 4: goto L_749227; case 5: goto L_74929e; case 6: goto L_7492da; case 7: goto L_749319; case 8: goto L_749357; case 9: goto L_749399; case 10: goto L_7493e0; case 11: goto L_74941a; case 12: goto L_749267; case 13: goto L_749462; default: throw std::runtime_error("jump table"); }
L_74912d:
    ebp = (ebp - 1u);
    eax = ebp;
L_749130:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_749134 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_749134 == 0)) goto L_749130;
    ecx = rd32(m, 0x952a1cu) /* 4.8414508140364195e+30 */;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    wr32(m, eax, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x952a20u)) & 0xffffu);
    ebp = rd32(m, esp); esp += 4;
    wr16(m, eax + 0x4u, (edx & 0xffffu));
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp + 0x10u);
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x74915au);  // __security_check_cookie
    esp = esp + 0x14u;
    LIFT_RET(12);
L_749160:
    ebp = (ebp - 1u);
    eax = ebp;
L_749163:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_749167 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_749167 == 0)) goto L_749163;
    ecx = rd32(m, 0x95b9e4u) /* 4.465584291922945e+30 */;
    wr32(m, eax, ecx);
    edx = rd32(m, 0x95b9e8u) /* 1.8050608361294031e+28 */;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    wr32(m, eax + 0x4u, edx);
    ecx = (ecx & 0xffff0000u) | ((rd16(m, 0x95b9ecu)) & 0xffffu);
    ebp = rd32(m, esp); esp += 4;
    wr16(m, eax + 0x8u, (ecx & 0xffffu));
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp + 0x10u);
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x749196u);  // __security_check_cookie
    esp = esp + 0x14u;
    LIFT_RET(12);
L_74919c:
    ebp = (ebp - 1u);
    eax = ebp;
L_7491a0:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_7491a4 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_7491a4 == 0)) goto L_7491a0;
    edx = rd32(m, 0x96c654u) /* 4.465584291922945e+30 */;
    wr32(m, eax, edx);
    ecx = rd32(m, 0x96c658u) /* 2.8168868658672357e+20 */;
    wr32(m, eax + 0x4u, ecx);
    edx = rd32(m, 0x96c65cu) /* 4.543812672784388e+30 */;
    wr32(m, eax + 0x8u, edx);
    ecx = (ecx & 0xffff0000u) | ((rd16(m, 0x96c660u)) & 0xffffu);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    wr16(m, eax + 0xcu, (ecx & 0xffffu));
    edx = (edx & 0xffffff00u) | ((rd8(m, 0x96c662u)) & 0xffu);
    ebp = rd32(m, esp); esp += 4;
    wr8(m, eax + 0xeu, (edx & 0xffu));
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp + 0x10u);
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x7491e5u);  // __security_check_cookie
    esp = esp + 0x14u;
    LIFT_RET(12);
L_7491eb:
    ebp = (ebp - 1u);
    eax = ebp;
    edi = edi;
L_7491f0:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_7491f4 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_7491f4 == 0)) goto L_7491f0;
    ecx = rd32(m, 0x96c648u) /* 7.501781389493335e+28 */;
    wr32(m, eax, ecx);
    edx = rd32(m, 0x96c64cu) /* 6.948111171270704e+22 */;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    wr32(m, eax + 0x4u, edx);
    ecx = (ecx & 0xffffff00u) | ((rd8(m, 0x96c650u)) & 0xffu);
    ebp = rd32(m, esp); esp += 4;
    wr8(m, eax + 0x8u, (ecx & 0xffu));
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp + 0x10u);
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x749221u);  // __security_check_cookie
    esp = esp + 0x14u;
    LIFT_RET(12);
L_749227:
    ebp = (ebp - 1u);
    eax = ebp;
L_749230:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_749234 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_749234 == 0)) goto L_749230;
    edx = rd32(m, 0x96c63cu) /* 7.501781389493335e+28 */;
    wr32(m, eax, edx);
    ecx = rd32(m, 0x96c640u) /* 1.7559333474084634e+25 */;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    wr32(m, eax + 0x4u, ecx);
    edx = rd32(m, 0x96c644u) /* 9.314266940446733e-39 */;
    ebp = rd32(m, esp); esp += 4;
    wr32(m, eax + 0x8u, edx);
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp + 0x10u);
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x749261u);  // __security_check_cookie
    esp = esp + 0x14u;
    LIFT_RET(12);
L_749267:
    ebp = (ebp - 1u);
    eax = ebp;
L_749270:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_749274 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_749274 == 0)) goto L_749270;
    ecx = rd32(m, 0x96cafcu) /* 4.541398145691163e+30 */;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    wr32(m, eax, ecx);
    edx = rd32(m, 0x96cb00u) /* 1.0597469575392112e-38 */;
    ebp = rd32(m, esp); esp += 4;
    wr32(m, eax + 0x4u, edx);
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp + 0x10u);
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x749298u);  // __security_check_cookie
    esp = esp + 0x14u;
    LIFT_RET(12);
L_74929e:
    ebp = (ebp - 1u);
    eax = ebp;
L_7492a1:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_7492a5 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_7492a5 == 0)) goto L_7492a1;
    ecx = rd32(m, 0x96cb04u) /* 6.233177790864557e+22 */;
    wr32(m, eax, ecx);
    edx = rd32(m, 0x96cb08u) /* 4.620563746294262e+30 */;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    wr32(m, eax + 0x4u, edx);
    ecx = (ecx & 0xffff0000u) | ((rd16(m, 0x96cb0cu)) & 0xffffu);
    ebp = rd32(m, esp); esp += 4;
    wr16(m, eax + 0x8u, (ecx & 0xffffu));
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp + 0x10u);
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x7492d4u);  // __security_check_cookie
    esp = esp + 0x14u;
    LIFT_RET(12);
L_7492da:
    ebp = (ebp - 1u);
    eax = ebp;
L_7492e0:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_7492e4 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_7492e4 == 0)) goto L_7492e0;
    edx = rd32(m, 0x96cb10u) /* 15258.3349609375 */;
    wr32(m, eax, edx);
    ecx = rd32(m, 0x96cb14u) /* 1.8494332154114998e+31 */;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    wr32(m, eax + 0x4u, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96cb18u)) & 0xffffu);
    ebp = rd32(m, esp); esp += 4;
    wr16(m, eax + 0x8u, (edx & 0xffffu));
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp + 0x10u);
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x749313u);  // __security_check_cookie
    esp = esp + 0x14u;
    LIFT_RET(12);
L_749319:
    ebp = (ebp - 1u);
    eax = ebp;
L_749320:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_749324 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_749324 == 0)) goto L_749320;
    ecx = rd32(m, 0x96cb1cu) /* 249992560.0 */;
    wr32(m, eax, ecx);
    edx = rd32(m, 0x96cb20u) /* 1.7595148287070903e+22 */;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    wr32(m, eax + 0x4u, edx);
    ecx = (ecx & 0xffffff00u) | ((rd8(m, 0x96cb24u)) & 0xffu);
    ebp = rd32(m, esp); esp += 4;
    wr8(m, eax + 0x8u, (ecx & 0xffu));
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp + 0x10u);
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x749351u);  // __security_check_cookie
    esp = esp + 0x14u;
    LIFT_RET(12);
L_749357:
    ebp = (ebp - 1u);
    eax = ebp;
L_749360:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_749364 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_749364 == 0)) goto L_749360;
    edx = rd32(m, 0x96cb28u) /* 15999523840.0 */;
    wr32(m, eax, edx);
    ecx = rd32(m, 0x96cb2cu) /* 2.9585991292114724e+32 */;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    wr32(m, eax + 0x4u, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96cb30u)) & 0xffffu);
    ebp = rd32(m, esp); esp += 4;
    wr16(m, eax + 0x8u, (edx & 0xffffu));
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp + 0x10u);
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x749393u);  // __security_check_cookie
    esp = esp + 0x14u;
    LIFT_RET(12);
L_749399:
    ebp = (ebp - 1u);
    eax = ebp;
L_7493a0:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_7493a4 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_7493a4 == 0)) goto L_7493a0;
    ecx = rd32(m, 0x96c62cu) /* 244133.359375 */;
    wr32(m, eax, ecx);
    edx = rd32(m, 0x96c630u) /* 244.3804473876953 */;
    wr32(m, eax + 0x4u, edx);
    ecx = rd32(m, 0x96c634u) /* 2.7084158167366705e+23 */;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    wr32(m, eax + 0x8u, ecx);
    edx = (edx & 0xffffff00u) | ((rd8(m, 0x96c638u)) & 0xffu);
    ebp = rd32(m, esp); esp += 4;
    wr8(m, eax + 0xcu, (edx & 0xffu));
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp + 0x10u);
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x7493dau);  // __security_check_cookie
    esp = esp + 0x14u;
    LIFT_RET(12);
L_7493e0:
    ebp = (ebp - 1u);
    eax = ebp;
L_7493e3:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_7493e7 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_7493e7 == 0)) goto L_7493e3;
    ecx = rd32(m, 0x96c620u) /* 7.184925751916578e+22 */;
    wr32(m, eax, ecx);
    edx = rd32(m, 0x96c624u) /* 2.8576616784462525e+26 */;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    wr32(m, eax + 0x4u, edx);
    ecx = (ecx & 0xffffff00u) | ((rd8(m, 0x96c628u)) & 0xffu);
    ebp = rd32(m, esp); esp += 4;
    wr8(m, eax + 0x8u, (ecx & 0xffu));
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp + 0x10u);
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x749414u);  // __security_check_cookie
    esp = esp + 0x14u;
    LIFT_RET(12);
L_74941a:
    ebp = (ebp - 1u);
    eax = ebp;
L_749420:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_749424 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_749424 == 0)) goto L_749420;
    edx = rd32(m, 0x96c610u) /* 7.184925751916578e+22 */;
    wr32(m, eax, edx);
    ecx = rd32(m, 0x96c614u) /* 7.143072637744178e+31 */;
    wr32(m, eax + 0x4u, ecx);
    edx = rd32(m, 0x96c618u) /* 6.8889667484443354e+22 */;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    wr32(m, eax + 0x8u, edx);
    ecx = (ecx & 0xffff0000u) | ((rd16(m, 0x96c61cu)) & 0xffffu);
    ebp = rd32(m, esp); esp += 4;
    wr16(m, eax + 0xcu, (ecx & 0xffffu));
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp + 0x10u);
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x74945cu);  // __security_check_cookie
    esp = esp + 0x14u;
    LIFT_RET(12);
L_749462:
    ebp = (ebp - 1u);
    eax = ebp;
L_749465:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_749469 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_749469 == 0)) goto L_749465;
    edx = rd32(m, 0x96c604u) /* 4.8419942261923363e+30 */;
    wr32(m, eax, edx);
    ecx = rd32(m, 0x96c608u) /* 6.32684763605905e+28 */;
    wr32(m, eax + 0x4u, ecx);
    edx = (edx & 0xffff0000u) | ((rd16(m, 0x96c60cu)) & 0xffffu);
    wr16(m, eax + 0x8u, (edx & 0xffffu));
    ecx = (ecx & 0xffffff00u) | ((rd8(m, 0x96c60eu)) & 0xffu);
    wr8(m, eax + 0xau, (ecx & 0xffu));
L_749492:
    ecx = rd32(m, esp + 0x20u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x7494a1u);  // __security_check_cookie
    esp = esp + 0x14u;
    LIFT_RET(12);
}

// 00749590 Fn_00749590
//   original FUN_00749590; 0 instructions per frame (plan); register inputs: edx edi; returns with ret 
REFLEX_PORT_FN(0x00749590, Fn_00749590, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_74959c = 0;
    uint32_t fA_7495c6 = 0;
    uint32_t fB_7495c6 = 0;
    uint32_t fR_7495d2 = 0;
    eax = rd32(m, edi + 0x14u);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, eax + 0x26cu);
    eax = 0u;
    fR_74959c = (esi & esi);
    if (((fR_74959c == 0) || sgn(fR_74959c, 4) != false)) goto L_7495d1;
    esp -= 4; wr32(m, esp, ebx);
L_7495a1:
    ecx = rd32(m, edi + 0x14u);
    ecx = rd32(m, ecx + 0x268u);
    ecx = rd32(m, ecx + eax * 4);
    ebx = rd32(m, ecx + 0x74u);
    ebx = rd32(m, ebx + 0x1acu);
    wr32(m, ebx + 0x1010u, edx);
    ecx = rd32(m, ecx + 0x74u);
    ecx = rd32(m, ecx + 0x1b4u);
    eax = (eax + 1u);
    fA_7495c6 = eax;
    fB_7495c6 = esi;
    wr32(m, ecx + 0x5c4e0u, edx);
    if (int32_t(sext(fA_7495c6, 4)) < int32_t(sext(fB_7495c6, 4))) goto L_7495a1;
    ebx = rd32(m, esp); esp += 4;
L_7495d1:
    esi = rd32(m, esp); esp += 4;
    fR_7495d2 = (edx & edx);
    if ((fR_7495d2 == 0)) goto L_7495e9;
    edx = rd32(m, edi + 0x14u);
    eax = rd32(m, edx + 0x234u);
    eax = eax + 0x20150u;
    LIFT_TAILJMP(0x74b5a0u);  // FUN_0074b5a0
L_7495e9:
    eax = rd32(m, edi + 0x14u);
    eax = rd32(m, eax + 0x234u);
    eax = eax + 0x20150u;
    LIFT_TAILJMP(0x74b610u);  // FUN_0074b610
}

// 007496d0 Fn_007496d0
//   original FUN_007496d0; 0 instructions per frame (plan); register inputs: ecx eax; returns with ret 4
REFLEX_PORT_FN(0x007496d0, Fn_007496d0, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_7496d2 = 0;
    uint32_t fA_7496d6 = 0;
    uint32_t fB_7496d6 = 0;
    uint32_t fR_7496db = 0;
    uint32_t fA_749702 = 0;
    uint32_t fB_749702 = 0;
    uint32_t fA_749710 = 0;
    uint32_t fB_749710 = 0;
    uint32_t fA_749727 = 0;
    uint32_t fB_749727 = 0;
    uint32_t fA_74972f = 0;
    uint32_t fB_74972f = 0;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, esi);
    fR_7496d2 = (eax & eax);
    if (((fR_7496d2 == 0) || sgn(fR_7496d2, 4) != false)) goto L_749745;
    fA_7496d6 = eax;
    fB_7496d6 = 0x3u;
    if (int32_t(sext(fA_7496d6, 4)) > int32_t(sext(fB_7496d6, 4))) goto L_749745;
    fR_7496db = (rd8(m, 0x9e275cu) & 0x1u);
    esi = 0x5u;
    if (!(fR_7496db == 0)) goto L_749702;
    wr32(m, 0x9e275cu, (rd32(m, 0x9e275cu) | 0x1u));
    LIFT_CALL(0x5b2370u, 0x7496f5u);  // MXSeriesManager_Construct
    esp -= 4; wr32(m, esp, 0x911da0u);
    LIFT_CALL(0x8f9fbfu, 0x7496ffu);  // _atexit
    esp = esp + 0x4u;
L_749702:
    fA_749702 = rd32(m, 0x9e2318u);
    fB_749702 = 0x2u;
    if (fA_749702 == fB_749702) goto L_749719;
    LIFT_CALL(0x413d20u, 0x749710u);  // FUN_00413d20
    fA_749710 = rd32(m, eax + 0x190u);
    fB_749710 = 0x1u;
    if (fA_749710 != fB_749710) goto L_74973b;
L_749719:
    LIFT_CALL(0x413d20u, 0x74971eu);  // FUN_00413d20
    eax = rd32(m, eax + 0x18cu);
    ecx = rd32(m, eax + 0xcu);
    fA_749727 = ecx;
    fB_749727 = 0x2u;
    if (fA_749727 != fB_749727) goto L_74972f;
    ecx = rd32(m, eax + 0x10u);
L_74972f:
    fA_74972f = ecx;
    fB_74972f = 0x7u;
    if (fA_74972f > fB_74972f) goto L_74973b;
    LIFT_CALL(0x749680u, 0x749739u);  // FUN_00749680
    esi = eax;
L_74973b:
    eax = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x749600u, 0x749745u);  // FUN_00749600
L_749745:
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00749750 Fn_00749750
//   original FUN_00749750; 0 instructions per frame (plan); register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x00749750, Fn_00749750, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_749756 = 0;
    uint32_t fB_749756 = 0;
    edx = rd32(m, ecx + 0x1acu);
    fA_749756 = rd32(m, edx + 0x194u);
    fB_749756 = 0x1u;
    eax = 0x5u;
    if (fA_749756 > fB_749756) goto L_749769;
    eax = 0x4u;
L_749769:
    LIFT_RET(0);
}

// 00749770 ScenarioAnimEntity__vf22
//   original ScenarioAnimEntity::vf22; 11 instructions per frame (plan); register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00749770, ScenarioAnimEntity__vf22, "anim", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = 0x104fu;
    LIFT_RET(0);
}

// 007497b0 RaceIntroScenario__vf3
//   original RaceIntroScenario::vf3; 0 instructions per frame (plan); register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x007497b0, RaceIntroScenario__vf3, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7497b9 = 0;
    uint32_t fB_7497b9 = 0;
    uint32_t fA_7497d0 = 0;
    uint32_t fB_7497d0 = 0;
    uint32_t fA_7497e5 = 0;
    uint32_t fB_7497e5 = 0;
    uint32_t fA_749835 = 0;
    uint32_t fB_749835 = 0;
    uint32_t fA_749860 = 0;
    uint32_t fB_749860 = 0;
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    eax = rd32(m, esi + 0x151b4u);
    fA_7497b9 = eax;
    fB_7497b9 = rd32(m, 0xd610a0u);
    if (fA_7497b9 != fB_7497b9) goto L_7497c8;
    eax = 0x1u;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_7497c8:
    esp -= 4; wr32(m, esp, ebx);
    eax = (eax - 1u);
    esp -= 4; wr32(m, esp, ebp);
    ebp = 0x1u;
    fA_7497d0 = eax;
    fB_7497d0 = 0x5u;
    if (fA_7497d0 > fB_7497d0) goto L_749874;
    switch (eax) { case 0: goto L_7497e0; case 1: goto L_7497fe; case 2: goto L_7497fe; case 3: goto L_74981c; case 4: goto L_74981c; case 5: goto L_74984f; default: throw std::runtime_error("jump table"); }
L_7497e0:
    eax = rd32(m, esi + 0x14u);
    ecx = 0u;
    fA_7497e5 = rd32(m, eax + 0x23cu);
    fB_7497e5 = 0x11u;
    eax = ebp;
    ecx = (ecx & 0xffffff00u) | (((fA_7497e5 == fB_7497e5) ? 1u : 0u) & 0xffu);
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = ecx + 0x2u;
    wr32(m, esi + 0x151b4u, ecx);
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_7497fe:
    eax = rd32(m, esi + 0x14u);
    esp -= 4; wr32(m, esp, 0x0u);
L_749803:
    LIFT_CALL(0x559e60u, 0x749808u);  // FUN_00559e60
    ecx = rd32(m, eax + 0x74u);
    LIFT_CALL(0x749750u, 0x749810u);  // FUN_00749750
    wr32(m, esi + 0x151b4u, eax);
    eax = ebp;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_74981c:
    ebx = rd32(m, esi + 0x14u);
    esp -= 4; wr32(m, esp, edi);
    edi = ebp;
    edx = ebx;
    LIFT_CALL(0x559e20u, 0x749829u);  // FUN_00559e20
    wr32(m, esi + 0x151c0u, rd32(m, esi + 0x151c0u) + ebp);
    ecx = rd32(m, esi + 0x151c0u);
    fA_749835 = ecx;
    fB_749835 = eax;
    edi = rd32(m, esp); esp += 4;
    if (int32_t(sext(fA_749835, 4)) < int32_t(sext(fB_749835, 4))) goto L_74984a;
    eax = ebp;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    wr32(m, esi + 0x151b4u, 0x6u);
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_74984a:
    esp -= 4; wr32(m, esp, ecx);
    eax = ebx;
    goto L_749803;
L_74984f:
    edx = rd32(m, esi + 0x14u);
    eax = rd32(m, edx + 0x234u);
    eax = rd32(m, eax + 0x203e0u);
    ebp = 0u;
    fA_749860 = eax;
    fB_749860 = ebp;
    if (fA_749860 == fB_749860) goto L_74986e;
    wr32(m, eax + 0x484u, 0x4u);
L_74986e:
    wr32(m, esi + 0x151b4u, ebp);
L_749874:
    eax = ebp;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00749910 Fn_00749910
//   original FUN_00749910; 0 instructions per frame (plan); register inputs: none; returns with ret 8
REFLEX_PORT_FN(0x00749910, Fn_00749910, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_74993f = 0;
    uint32_t fB_74993f = 0;
    uint32_t fA_749944 = 0;
    uint32_t fB_749944 = 0;
    uint32_t fR_749952 = 0;
    uint32_t fA_74995a = 0;
    uint32_t fB_74995a = 0;
    uint32_t fA_74996a = 0;
    uint32_t fB_74996a = 0;
    uint32_t fA_74997c = 0;
    uint32_t fB_74997c = 0;
    uint32_t fA_74998c = 0;
    uint32_t fB_74998c = 0;
    uint32_t fA_7499b8 = 0;
    uint32_t fB_7499b8 = 0;
    uint32_t fR_7499cc = 0;
    uint32_t fA_749a1a = 0;
    uint32_t fB_749a1a = 0;
    uint32_t fR_749a46 = 0;
    uint32_t fR_749a68 = 0;
    uint32_t fA_749ace = 0;
    uint32_t fB_749ace = 0;
    uint32_t fA_749af6 = 0;
    uint32_t fB_749af6 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x64u;
    eax = rd32(m, ebp + 0x8u);
    eax = rd32(m, eax + 0x14u);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, eax + 0x26cu);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, eax + 0x234u);
    eax = rd32(m, ebp + 0xcu);
    edi = edi + 0x20150u;
    wr32(m, esp + 0x1cu, edi);
    wr32(m, esp + 0x18u, ebx);
    fA_74993f = eax;
    fB_74993f = 0x4u;
    if (fA_74993f == fB_74993f) goto L_74994d;
    fA_749944 = eax;
    fB_749944 = 0x5u;
    if (fA_749944 == fB_749944) goto L_74994d;
    esi = 0u;
    goto L_749952;
L_74994d:
    esi = 0x1u;
L_749952:
    fR_749952 = (eax & eax);
    if (((fR_749952 == 0) || sgn(fR_749952, 4) != false)) goto L_749b00;
    fA_74995a = eax;
    fB_74995a = 0x6u;
    if (int32_t(sext(fA_74995a, 4)) > int32_t(sext(fB_74995a, 4))) goto L_749b00;
    eax = edi;
    LIFT_CALL(0x74b480u, 0x74996au);  // FUN_0074b480
    fA_74996a = rd32(m, edi + 0x172cu);
    fB_74996a = ebx;
    if (int32_t(sext(fA_74996a, 4)) < int32_t(sext(fB_74996a, 4))) goto L_749b00;
    ebx = 0u;
    wr32(m, esp + 0x14u, ebx);
    fA_74997c = esi;
    fB_74997c = ebx;
    if (fA_74997c == fB_74997c) goto L_74998c;
    eax = rd32(m, ebp + 0x8u);
    LIFT_CALL(0x74d6c0u, 0x749988u);  // FUN_0074d6c0
    wr32(m, esp + 0x14u, eax);
L_74998c:
    fA_74998c = rd32(m, esp + 0x18u);
    fB_74998c = ebx;
    if (int32_t(sext(fA_74998c, 4)) <= int32_t(sext(fB_74998c, 4))) goto L_749b00;
    ecx = (edi + 0x16ecu);
    wr32(m, esp + 0x10u, ecx);
    goto L_7499a6;
L_7499a2:
    edi = rd32(m, esp + 0x1cu);
L_7499a6:
    edx = rd32(m, ebp + 0x8u);
    eax = rd32(m, edx + 0x14u);
    ecx = rd32(m, eax + 0x268u);
    edx = rd32(m, ecx + ebx * 4);
    esi = rd32(m, edx + 0x74u);
    fA_7499b8 = esi;
    fB_7499b8 = rd32(m, esp + 0x14u);
    if (fA_7499b8 != fB_7499b8) goto L_7499c6;
    edi = rd32(m, edi + 0x1730u);
    goto L_7499cc;
L_7499c6:
    eax = rd32(m, esp + 0x10u);
    edi = rd32(m, eax);
L_7499cc:
    fR_7499cc = (edi & edi);
    if ((fR_7499cc == 0)) goto L_749af0;
    ecx = rd32(m, esi + 0x1acu);
    edx = rd32(m, ecx);
    edx = rd32(m, edx + 0x3cu);
    eax = (esp + 0x30u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(edx, 0x7499e6u);
    x0 = X{};
    x1 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    ecx = rd32(m, esi + 0x1acu);
    eax = 0x1u;
    wr32(m, esp + 0x40u, x0.l(0));
    wr32(m, esp + 0x44u, x1.l(0));
    wr32(m, esp + 0x48u, x0.l(0));
    wr32(m, esp + 0x34u, x0.l(0));
    wr32(m, esp + 0x54u, x0.l(0));
    fA_749a1a = rd32(m, ecx + 0x194u);
    fB_749a1a = eax;
    if (fA_749a1a <= fB_749a1a) goto L_749a98;
    x0 = movss_m(rd32(m, esp + 0x60u));
    wr32(m, esp + 0x20u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x64u));
    wr32(m, esp + 0x24u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x68u));
    wr32(m, esp + 0x28u, x0.l(0));
    fR_749a46 = (rd8(m, 0x9e30e0u) & (eax & 0xffu));
    if (!(fR_749a46 == 0)) goto L_749a5e;
    wr32(m, 0x9e30e0u, (rd32(m, 0x9e30e0u) | eax));
    wr32(m, 0x9e30dcu, 0x0u);
L_749a5e:
    edx = 0x9e30d4u;
    LIFT_CALL(0x402dc0u, 0x749a68u);  // FUN_00402dc0
    fR_749a68 = (eax & eax);
    if ((fR_749a68 == 0)) goto L_749a78;
    edx = (esp + 0x20u);
    esp -= 4; wr32(m, esp, edx);
    ecx = eax;
    LIFT_CALL(0x7c5950u, 0x749a78u);  // FUN_007c5950
L_749a78:
    x0 = movss_m(rd32(m, esp + 0x20u));
    x1 = movss_m(rd32(m, esp + 0x28u));
    wr32(m, esp + 0x60u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x24u));
    wr32(m, esp + 0x68u, x1.l(0));
    goto L_749a9e;
L_749a98:
    x0 = movss_m(rd32(m, esp + 0x64u));
L_749a9e:
    eax = rd32(m, edi);
    x1 = movss_m(rd32(m, 0x9cb14cu));
    edx = rd32(m, eax + 0x40u);
    ecx = (esp + 0x30u);
    esp -= 4; wr32(m, esp, ecx);
    x1 = addss(x1, x0.l(0));
    ecx = edi;
    wr32(m, esp + 0x68u, x1.l(0));
    LIFT_CALL(edx, 0x749abeu);
    eax = rd32(m, esi + 0x1acu);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x6bd760u, 0x749acau);  // CrewChief_SetShirt
    eax = rd32(m, ebp + 0xcu);
    eax = (eax - 1u);
    fA_749ace = eax;
    fB_749ace = 0x5u;
    if (fA_749ace > fB_749ace) goto L_749adf;
    esp -= 4; wr32(m, esp, eax);
    eax = 0u;
    ecx = esi;
    LIFT_CALL(0x7498a0u, 0x749addu);  // FUN_007498a0
    goto L_749ae1;
L_749adf:
    eax = 0u;
L_749ae1:
    ecx = rd32(m, ebp + 0x8u);
    edx = rd32(m, ecx + 0x14u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, edx);
    ecx = eax;
    LIFT_CALL(0x55e360u, 0x749af0u);  // FUN_0055e360
L_749af0:
    wr32(m, esp + 0x10u, rd32(m, esp + 0x10u) + 0x4u);
    ebx = (ebx + 1u);
    fA_749af6 = ebx;
    fB_749af6 = rd32(m, esp + 0x18u);
    if (int32_t(sext(fA_749af6, 4)) < int32_t(sext(fB_749af6, 4))) goto L_7499a2;
L_749b00:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 00749b10 Fn_00749b10
//   original FUN_00749b10; 0 instructions per frame (plan); register inputs: eax; returns with ret 4
REFLEX_PORT_FN(0x00749b10, Fn_00749b10, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_749b14 = 0;
    uint32_t fA_749b18 = 0;
    uint32_t fB_749b18 = 0;
    uint32_t fA_749b2c = 0;
    uint32_t fB_749b2c = 0;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    fR_749b14 = (eax & eax);
    if (((fR_749b14 == 0) || sgn(fR_749b14, 4) != false)) goto L_749b8d;
    fA_749b18 = eax;
    fB_749b18 = 0x6u;
    if (int32_t(sext(fA_749b18, 4)) > int32_t(sext(fB_749b18, 4))) goto L_749b8d;
    ebx = 0u;
L_749b20:
    eax = rd32(m, 0xa946b8u);
    ecx = rd8(m, eax + 0x1541cu);
    fA_749b2c = ebx;
    fB_749b2c = ecx;
    if (int32_t(sext(fA_749b2c, 4)) >= int32_t(sext(fB_749b2c, 4))) goto L_749b8d;
    esp -= 4; wr32(m, esp, 0x3u);
    esp -= 4; wr32(m, esp, ebx);
    edx = (eax + 0x7460u);
    LIFT_CALL(0x552b90u, 0x749b3eu);  // FUN_00552b90
    esi = eax;
    esp -= 4; wr32(m, esp, 0x1u);
    eax = 0u;
    ecx = esi;
    LIFT_CALL(0x749ba0u, 0x749b4bu);  // FUN_00749ba0
    edx = rd32(m, esp + 0x14u);
    ebp = eax;
    eax = rd32(m, edx + 0x14u);
    edi = rd32(m, eax + 0x234u);
    edi = edi + 0x20150u;
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x74b080u, 0x749b66u);  // FUN_0074b080
    esi = rd32(m, eax);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, esi);
    edx = 0u;
    ecx = edi;
    LIFT_CALL(0x74be10u, 0x749b77u);  // FUN_0074be10
    fpush(m, reflex_f80::from_f32(rd32(m, 0x9d3bfcu)));
    edx = rd32(m, esi);
    eax = rd32(m, edx + 0x4cu);
    esp -= 4; wr32(m, esp, ecx);
    ecx = esi;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(eax, 0x749b8au);
    ebx = (ebx + 1u);
    goto L_749b20;
L_749b8d:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00749df0 RaceIntroScenario__vf1
//   original RaceIntroScenario::vf1; 0 instructions per frame (plan); register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x00749df0, RaceIntroScenario__vf1, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_749e1f = 0;
    uint32_t fA_749e25 = 0;
    uint32_t fB_749e25 = 0;
    uint32_t fA_749e2a = 0;
    uint32_t fB_749e2a = 0;
    uint32_t fA_749e52 = 0;
    uint32_t fB_749e52 = 0;
    uint32_t fA_749e5b = 0;
    uint32_t fB_749e5b = 0;
    uint32_t fA_749e60 = 0;
    uint32_t fB_749e60 = 0;
    uint32_t fR_749e78 = 0;
    uint32_t fR_749f15 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x54u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, edi);
    ebx = ecx;
    esp -= 4; wr32(m, esp, ebx);
    eax = esi;
    LIFT_CALL(0x7496d0u, 0x749e09u);  // FUN_007496d0
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x749910u, 0x749e10u);  // FUN_00749910
    eax = rd32(m, ebx + 0x14u);
    ecx = rd32(m, eax + 0x234u);
    eax = rd32(m, ecx + 0x203e0u);
    fR_749e1f = (eax & eax);
    if ((fR_749e1f == 0)) goto L_749e4a;
    ecx = esi;
    fA_749e25 = ecx;
    fB_749e25 = 0x1u;
    ecx = fA_749e25 - fB_749e25;
    if (fA_749e25 == fB_749e25) goto L_749e40;
    fA_749e2a = ecx;
    fB_749e2a = 0x5u;
    ecx = fA_749e2a - fB_749e2a;
    if (fA_749e2a != fB_749e2a) goto L_749e4a;
    wr32(m, eax + 0x484u, 0x3u);
    LIFT_CALL(0x6b7fa0u, 0x749e3eu);  // FUN_006b7fa0
    goto L_749e4a;
L_749e40:
    wr32(m, eax + 0x484u, 0x2u);
L_749e4a:
    esp -= 4; wr32(m, esp, ebx);
    eax = esi;
    LIFT_CALL(0x749b10u, 0x749e52u);  // FUN_00749b10
    fA_749e52 = esi;
    fB_749e52 = 0x4u;
    if (int32_t(sext(fA_749e52, 4)) < int32_t(sext(fB_749e52, 4))) goto L_749fa4;
    fA_749e5b = esi;
    fB_749e5b = 0x5u;
    if (int32_t(sext(fA_749e5b, 4)) <= int32_t(sext(fB_749e5b, 4))) goto L_749eaf;
    fA_749e60 = esi;
    fB_749e60 = 0x6u;
    if (fA_749e60 != fB_749e60) goto L_749fa4;
    edx = rd32(m, ebx + 0x14u);
    eax = rd32(m, edx + 0x234u);
    ecx = rd32(m, eax + 0x203e0u);
    fR_749e78 = (ecx & ecx);
    if ((fR_749e78 == 0)) goto L_749fa4;
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x38u);
    LIFT_CALL(eax, 0x749e87u);
    ecx = rd32(m, ebx + 0x14u);
    edi = rd32(m, ecx + 0x234u);
    edi = edi + 0x20150u;
    ecx = 0x10u;
    esi = eax;
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    edx = rd32(m, ebx + 0x14u);
    edi = rd32(m, edx + 0x234u);
    esi = eax;
    goto L_749f97;
L_749eaf:
    eax = rd32(m, ebx + 0x151c0u);
    esi = rd32(m, ebx + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    eax = esi;
    LIFT_CALL(0x559e60u, 0x749ec0u);  // FUN_00559e60
    ecx = rd32(m, eax + 0x74u);
    edi = rd32(m, esi + 0x234u);
    esp -= 4; wr32(m, esp, ecx);
    edi = edi + 0x20150u;
    LIFT_CALL(0x74b080u, 0x749ed5u);  // FUN_0074b080
    ecx = rd32(m, eax);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x38u);
    wr32(m, esp + 0x18u, ecx);
    LIFT_CALL(eax, 0x749ee2u);
    ecx = rd32(m, ebx + 0x14u);
    edi = rd32(m, ecx + 0x234u);
    esi = eax;
    edi = edi + 0x20150u;
    ecx = 0x10u;
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    ecx = rd32(m, esp + 0x18u);
    edx = rd32(m, ecx);
    wr32(m, esp + 0x1cu, eax);
    eax = rd32(m, edx + 0x70u);
    LIFT_CALL(eax, 0x749f09u);
    esi = eax;
    esp -= 4; wr32(m, esp, 0x9581fcu);
    LIFT_CALL(0x7de980u, 0x749f15u);  // FUN_007de980
    fR_749f15 = (eax & eax);
    if ((fR_749f15 == 0)) goto L_749f8a;
    esp -= 4; wr32(m, esp, 0x9581fcu);
    edi = esi;
    LIFT_CALL(0x7de9e0u, 0x749f25u);  // FUN_007de9e0
    ecx = rd32(m, esp + 0x18u);
    edx = rd32(m, ecx);
    wr32(m, esp + 0x1cu, eax);
    eax = rd32(m, edx + 0x38u);
    LIFT_CALL(eax, 0x749f34u);
    esi = eax;
    ecx = 0x10u;
    edi = (esp + 0x20u);
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    esi = rd32(m, esp + 0x1cu);
    esp -= 4; wr32(m, esp, 0x0u);
    ecx = esi;
    LIFT_CALL(0x7db890u, 0x749f4eu);  // Node_EvaluateWorld
    eax = rd8(m, esi + 0xbu);
    ecx = rd32(m, esi);
    edx = rd32(m, ebx + 0x14u);
    edi = rd32(m, edx + 0x234u);
    eax = (eax << 6);
    eax = eax + rd32(m, ecx + 0x1cu);
    esi = (esp + 0x20u);
    x0 = movss_m(rd32(m, eax + 0x30u));
    wr32(m, esp + 0x50u, x0.l(0));
    x0 = movss_m(rd32(m, eax + 0x34u));
    wr32(m, esp + 0x54u, x0.l(0));
    x0 = movss_m(rd32(m, eax + 0x38u));
    wr32(m, esp + 0x58u, x0.l(0));
    goto L_749f97;
L_749f8a:
    eax = rd32(m, ebx + 0x14u);
    edi = rd32(m, eax + 0x234u);
    esi = rd32(m, esp + 0x1cu);
L_749f97:
    ecx = 0x10u;
    edi = edi + 0x21d50u;
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
L_749fa4:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 0074a9f0 Fn_0074a9f0
//   original FUN_0074a9f0; 0 instructions per frame (plan); register inputs: none; returns with ret 4
REFLEX_PORT_FN(0x0074a9f0, Fn_0074a9f0, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_74aa3a = 0;
    uint32_t fR_74aa61 = 0;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    esi = (ebp + 0x3e0u);
    esp -= 4; wr32(m, esp, esi);
    wr32(m, esp + 0x18u, esi);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x74aa09u);
    eax = rd32(m, 0xa95844u);
    x0 = movss_m(rd32(m, 0x9cd9e8u));
    esp -= 4; wr32(m, esp, 0x0u);
    wr32(m, ebp + 0x478u, x0.l(0));
    ecx = rd32(m, eax + 0x8u);
    edx = rd32(m, ecx);
    edx = rd32(m, edx + 0x6cu);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0x6d696e61u);
    eax = (ebp + 0x3f8u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(edx, 0x74aa38u);
    edi = eax;
    fR_74aa3a = (edi & edi);
    if ((fR_74aa3a == 0)) goto L_74aa96;
    eax = rd32(m, ebp);
    edx = rd32(m, eax + 0x3cu);
    ecx = (ebp + 0x390u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = ebp;
    LIFT_CALL(edx, 0x74aa4fu);
    esi = (ebp + 0x148u);
    eax = esi;
    LIFT_CALL(0x835000u, 0x74aa5cu);  // FUN_00835000
    ebx = 0x1u;
    fR_74aa61 = (eax & eax);
    if (!(fR_74aa61 == 0)) goto L_74aa80;
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x834eb0u, 0x74aa6cu);  // CharacterAnim_SetAnimation
    ecx = rd32(m, eax);
    wr32(m, ecx + 0x4cu, ebx);
    ecx = rd32(m, ecx + 0x44u);
    wr32(m, ecx + 0x54u, ebx);
    edx = rd32(m, eax);
    wr32(m, edx + 0x4cu, 0x0u);
L_74aa80:
    x1 = X{};
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x837510u, 0x74aa89u);  // FUN_00837510
    esi = rd32(m, esp + 0x14u);
    wr8(m, ebp + 0xa2u, (ebx & 0xffu));
    wr32(m, ebp + 0x70u, ebx);
L_74aa96:
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x74aa9du);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 0074aab0 Fn_0074aab0
//   original FUN_0074aab0; 4878 instructions per frame (plan); register inputs: esi; returns with ret 4
REFLEX_PORT_FN(0x0074aab0, Fn_0074aab0, "anim", LIFT_OUTS) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, edi);
    edi = (esi + 0x3e0u);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x74aabfu);
    eax = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, 0x7fu);
    esp -= 4; wr32(m, esp, eax);
    ebx = (esi + 0x3f8u);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(rd32(m, 0x916260u) /* 8.286626112932593e-39 */, 0x74aad3u);
    fpush(m, reflex_f80::from_int(0));
    x0 = movss_m(rd32(m, 0x9322ccu) /* 0.5 */);
    wr32(m, esp + 0x8u, fst32(m));
    fpop(m);
    wr8(m, ebx + 0x7fu, 0x0u);
    edx = rd32(m, esi);
    eax = rd32(m, edx + 0x4cu);
    esp = esp + 0x8u;
    ecx = esi;
    wr32(m, esi + 0x478u, x0.l(0));
    LIFT_CALL(eax, 0x74aaf9u);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x74ab00u);
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 0074ab10 ScenarioAnimEntity_Job: JobPool job submitted by ScenarioAnimEntity::vf19 (arg 0 = {entity, dt}); takes the entity's critical section at +0x3e0
//   original ?; 0 instructions per frame (plan); register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x0074ab10, ScenarioAnimEntity_Job, "anim", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = rd32(m, esp + 0x4u);
    x0 = movss_m(rd32(m, eax + 0x4u));
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, eax);
    esp -= 4; wr32(m, esp, edi);
    edi = (esi + 0x3e0u);
    esp -= 4; wr32(m, esp, edi);
    wr32(m, esp + 0x10u, x0.l(0));
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x74ab30u);
    x1 = movss_m(rd32(m, esp + 0xcu));
    esi = esi + 0x148u;
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x835430u, 0x74ab42u);  // Character_SampleKeyframes
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x74ab49u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0074af60 Fn_0074af60
//   original FUN_0074af60; 0 instructions per frame (plan); register inputs: eax edx; returns with ret 4
REFLEX_PORT_FN(0x0074af60, Fn_0074af60, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_74af80 = 0;
    uint32_t fB_74af80 = 0;
    uint32_t fA_74af89 = 0;
    uint32_t fB_74af89 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x44u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    esp -= 4; wr32(m, esp, edi);
    eax = 0u;
    ecx = (edx + 0x1734u);
L_74af80:
    fA_74af80 = rd32(m, ecx);
    fB_74af80 = 0x0u;
    if (fA_74af80 == fB_74af80) goto L_74af97;
    eax = (eax + 1u);
    ecx = ecx + 0x8u;
    fA_74af89 = eax;
    fB_74af89 = 0xcu;
    if (int32_t(sext(fA_74af89, 4)) < int32_t(sext(fB_74af89, 4))) goto L_74af80;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_74af97:
    wr32(m, edx + eax * 8 + 0x1738u, esi);
    wr32(m, edx + eax * 8 + 0x1734u, ebx);
    esi = esi + 0x240u;
    ecx = 0x10u;
    edi = (esp + 0x10u);
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    eax = rd32(m, ebx);
    edx = rd32(m, eax + 0x40u);
    ecx = (esp + 0x10u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = ebx;
    LIFT_CALL(edx, 0x74afc4u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 0074b000 Fn_0074b000
//   original FUN_0074b000; 54 instructions per frame (plan); register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x0074b000, Fn_0074b000, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_74b020 = 0;
    uint32_t fB_74b020 = 0;
    uint32_t fA_74b04a = 0;
    uint32_t fB_74b04a = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x44u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    ebx = ebx + 0x1734u;
    wr32(m, esp + 0xcu, 0xcu);
L_74b020:
    fA_74b020 = rd32(m, ebx);
    fB_74b020 = 0x0u;
    if (fA_74b020 == fB_74b020) goto L_74b047;
    eax = rd32(m, ebx + 0x4u);
    esi = (eax + 0x240u);
    ecx = 0x10u;
    edi = (esp + 0x10u);
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    ecx = rd32(m, ebx);
    eax = rd32(m, ecx);
    eax = rd32(m, eax + 0x40u);
    edx = (esp + 0x10u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(eax, 0x74b047u);
L_74b047:
    ebx = ebx + 0x8u;
    fA_74b04a = rd32(m, esp + 0xcu);
    fB_74b04a = 0x1u;
    wr32(m, esp + 0xcu, fA_74b04a - fB_74b04a);
    if (fA_74b04a != fB_74b04a) goto L_74b020;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 0074b060 Fn_0074b060
//   original FUN_0074b060; 0 instructions per frame (plan); register inputs: eax; returns with ret 0
REFLEX_PORT_FN(0x0074b060, Fn_0074b060, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_74b078 = 0;
    uint32_t fB_74b078 = 0;
    eax = eax + 0x1738u;
    ecx = 0xcu;
    edx = 0u;
L_74b070:
    wr32(m, eax - 0x4u, edx);
    wr32(m, eax, edx);
    eax = eax + 0x8u;
    fA_74b078 = ecx;
    fB_74b078 = 0x1u;
    ecx = fA_74b078 - fB_74b078;
    if (fA_74b078 != fB_74b078) goto L_74b070;
    LIFT_RET(0);
}

// 0074b080 Fn_0074b080
//   original FUN_0074b080; 0 instructions per frame (plan); register inputs: edi; returns with ret 4
REFLEX_PORT_FN(0x0074b080, Fn_0074b080, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_74b092 = 0;
    uint32_t fA_74b096 = 0;
    uint32_t fB_74b096 = 0;
    uint32_t fA_74b09e = 0;
    uint32_t fB_74b09e = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x8u);
    eax = 0u;
    esp -= 4; wr32(m, esp, esi);
    ecx = 0u;
    edx = (edi + 0x54u);
L_74b090:
    esi = rd32(m, edx);
    fR_74b092 = (esi & esi);
    if ((fR_74b092 == 0)) goto L_74b0b2;
    fA_74b096 = esi;
    fB_74b096 = ebx;
    if (fA_74b096 == fB_74b096) goto L_74b0a8;
    ecx = (ecx + 1u);
    edx = edx + 0x30u;
    fA_74b09e = ecx;
    fB_74b09e = 0xcu;
    if (fA_74b09e < fB_74b09e) goto L_74b090;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_74b0a8:
    eax = (ecx + ecx * 2);
    eax = (eax << 4);
    eax = (eax + edi + 0x50u);
L_74b0b2:
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 0074b0c0 Fn_0074b0c0
//   original FUN_0074b0c0; 4 instructions per frame (plan); register inputs: none; returns with ret 8
REFLEX_PORT_FN(0x0074b0c0, Fn_0074b0c0, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_74b0ca = 0;
    uint32_t fB_74b0ca = 0;
    uint32_t fR_74b0f0 = 0;
    uint32_t fA_74b0f8 = 0;
    uint32_t fB_74b0f8 = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    eax = 0u;
    esi = 0u;
    fA_74b0ca = rd32(m, ebx + 0x744u);
    fB_74b0ca = eax;
    if (int32_t(sext(fA_74b0ca, 4)) <= int32_t(sext(fB_74b0ca, 4))) goto L_74b112;
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, 0x9162bcu) /* 8.287018476502604e-39 */;
    esp -= 4; wr32(m, esp, edi);
    edi = (ebx + 0x294u);
L_74b0e0:
    eax = rd32(m, edi);
    ecx = rd32(m, esp + 0x18u);
    eax = rd32(m, eax + 0x18u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(ebp, 0x74b0edu);
    esp = esp + 0x8u;
    fR_74b0f0 = (eax & eax);
    if ((fR_74b0f0 == 0)) goto L_74b109;
    esi = (esi + 1u);
    edi = edi + 0x4u;
    fA_74b0f8 = esi;
    fB_74b0f8 = rd32(m, ebx + 0x744u);
    if (int32_t(sext(fA_74b0f8, 4)) < int32_t(sext(fB_74b0f8, 4))) goto L_74b0e0;
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(8);
L_74b109:
    eax = rd32(m, ebx + esi * 4 + 0x294u);
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
L_74b112:
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 0074b120 Fn_0074b120
//   original FUN_0074b120; 0 instructions per frame (plan); register inputs: none; returns with ret 8
REFLEX_PORT_FN(0x0074b120, Fn_0074b120, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_74b12a = 0;
    uint32_t fB_74b12a = 0;
    uint32_t fR_74b150 = 0;
    uint32_t fA_74b158 = 0;
    uint32_t fB_74b158 = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    eax = 0u;
    esi = 0u;
    fA_74b12a = rd32(m, ebx + 0x172cu);
    fB_74b12a = eax;
    if (int32_t(sext(fA_74b12a, 4)) <= int32_t(sext(fB_74b12a, 4))) goto L_74b172;
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, 0x9162bcu) /* 8.287018476502604e-39 */;
    esp -= 4; wr32(m, esp, edi);
    edi = (ebx + 0x16ecu);
L_74b140:
    eax = rd32(m, edi);
    eax = rd32(m, eax + 0x18u);
    ecx = rd32(m, esp + 0x18u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(ebp, 0x74b14du);
    esp = esp + 0x8u;
    fR_74b150 = (eax & eax);
    if ((fR_74b150 == 0)) goto L_74b169;
    esi = (esi + 1u);
    edi = edi + 0x4u;
    fA_74b158 = esi;
    fB_74b158 = rd32(m, ebx + 0x172cu);
    if (int32_t(sext(fA_74b158, 4)) < int32_t(sext(fB_74b158, 4))) goto L_74b140;
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(8);
L_74b169:
    eax = rd32(m, ebx + esi * 4 + 0x16ecu);
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
L_74b172:
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 0074b200 Fn_0074b200
//   original FUN_0074b200; 0 instructions per frame (plan); register inputs: eax; returns with ret 60
REFLEX_PORT_FN(0x0074b200, Fn_0074b200, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_74b221 = 0;
    uint32_t fA_74b22c = 0;
    uint32_t fB_74b22c = 0;
    uint32_t fA_74b23a = 0;
    uint32_t fB_74b23a = 0;
    uint32_t fA_74b242 = 0;
    uint32_t fB_74b242 = 0;
    uint32_t fR_74b285 = 0;
    FCmp fC_74b2a1{};
    uint32_t fR_74b373 = 0;
    FCmp fC_74b3ae{};
    uint32_t fR_74b3e1 = 0;
    FCmp fC_74b411{};
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x43cu;
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    eax = rd32(m, 0xa95844u);
    ecx = rd32(m, eax + 0x4u);
    edx = rd32(m, eax + ecx * 4 + 0x1cu);
    eax = rd32(m, edx + 0x19b8u);
    fR_74b221 = (eax & eax);
    if ((fR_74b221 == 0)) goto L_74b43d;
    ecx = rd32(m, ebp + 0x8u);
    fA_74b22c = ecx;
    fB_74b22c = 0x0u;
    ecx = fA_74b22c - fB_74b22c;
    if (fA_74b22c == fB_74b22c) goto L_74b3d5;
    edx = 0x1u;
    fA_74b23a = ecx;
    fB_74b23a = edx;
    ecx = fA_74b23a - fB_74b23a;
    if (fA_74b23a == fB_74b23a) goto L_74b367;
    fA_74b242 = ecx;
    fB_74b242 = edx;
    ecx = fA_74b242 - fB_74b242;
    if (fA_74b242 != fB_74b242) goto L_74b43d;
    esp -= 4; wr32(m, esp, 0x3f8u);
    esp -= 4; wr32(m, esp, ecx);
    eax = (esp + 0x4cu);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0x4cu, ecx);
    LIFT_CALL(0x8fa06au, 0x74b25eu);  // MSVCR90.DLL::memset
    esp = esp + 0xcu;
    LIFT_CALL(0x75c530u, 0x74b266u);  // FUN_0075c530
    eax = rd32(m, eax + 0x20138u);
    ecx = (esp + 0xcu);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, 0xffu);
    edx = (esp + 0x48u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, 0x1063u);
    LIFT_CALL(0x7d2920u, 0x74b285u);  // FUN_007d2920
    fR_74b285 = (eax & eax);
    if (((fR_74b285 == 0) || sgn(fR_74b285, 4) != false)) goto L_74b43d;
    x1 = movss_m(rd32(m, 0x9322acu) /* 0.009999999776482582 */);
    x0 = cvtsi2ss(x0, esi);
    x0 = mulss(x0, rd32(m, 0x977470u) /* 0.03333333507180214 */);
    fC_74b2a1 = fcmp32(x0.l(0), x1.l(0));
    if ((fc_b(fC_74b2a1) || fc_z(fC_74b2a1))) goto L_74b2ae;
    wr32(m, esp + 0xcu, x0.l(0));
    goto L_74b2b4;
L_74b2ae:
    wr32(m, esp + 0xcu, x1.l(0));
L_74b2b4:
    x0 = movss_m(rd32(m, ebp + 0x1cu));
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0xcu)));
    eax = rd32(m, esp + eax * 4 + 0x3cu);
    wr32(m, esp + 0x30u, x0.l(0));
    x0 = movss_m(rd32(m, ebp + 0x20u));
    wr32(m, esp + 0x34u, x0.l(0));
    x0 = movss_m(rd32(m, ebp + 0x24u));
    x1 = movss_m(rd32(m, ebp + 0x40u));
    wr32(m, esp + 0x38u, x0.l(0));
    x0 = movss_m(rd32(m, ebp + 0x28u));
    wr32(m, esp + 0x3cu, x0.l(0));
    x0 = movss_m(rd32(m, ebp + 0x2cu));
    wr32(m, esp + 0x10u, x0.l(0));
    x0 = movss_m(rd32(m, ebp + 0x30u));
    wr32(m, esp + 0x14u, x0.l(0));
    x0 = movss_m(rd32(m, ebp + 0x34u));
    wr32(m, esp + 0x18u, x0.l(0));
    x0 = movss_m(rd32(m, ebp + 0x38u));
    wr32(m, esp + 0x1cu, x0.l(0));
    x0 = movss_m(rd32(m, ebp + 0xcu));
    wr32(m, esp + 0x20u, x0.l(0));
    x0 = movss_m(rd32(m, ebp + 0x10u));
    wr32(m, esp + 0x24u, x0.l(0));
    x0 = movss_m(rd32(m, ebp + 0x14u));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp + 0x2cu, x0.l(0));
    wr32(m, esp, fst32(m));
    fpop(m);
    x0 = movss_m(rd32(m, ebp + 0x18u));
    wr32(m, esp + 0x30u, x0.l(0));
    x0 = movss_m(rd32(m, ebp + 0x3cu));
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0x38u);
    edx = (esp + 0x18u);
    esi = (esp + 0x28u);
    LIFT_CALL(0x40b4e0u, 0x74b360u);  // OverlayColorEntity_StartBlend
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(60);
L_74b367:
    ecx = rd32(m, eax + 0x70u);
    eax = (ecx + 0x10u);
    wr32(m, ecx, 0x0u);
    fR_74b373 = (eax & eax);
    if ((fR_74b373 == 0)) goto L_74b43d;
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x1 = movss_m(rd32(m, 0x9322acu) /* 0.009999999776482582 */);
    wr32(m, eax + 0x30u, edx);
    wr32(m, eax + 0x20u, x0.l(0));
    wr32(m, eax + 0x24u, x0.l(0));
    wr32(m, eax + 0x28u, x0.l(0));
    wr32(m, eax + 0x2cu, x0.l(0));
    x0 = cvtsi2ss(x0, esi);
    x0 = mulss(x0, rd32(m, 0x977470u) /* 0.03333333507180214 */);
    fC_74b3ae = fcmp32(x0.l(0), x1.l(0));
    if (!(fc_b(fC_74b3ae) || fc_z(fC_74b3ae))) goto L_74b3b6;
    x0 = x1;
L_74b3b6:
    wr32(m, eax + 0xcu, x0.l(0));
    x0 = X{};
    wr32(m, eax + 0x10u, x0.l(0));
    wr32(m, eax + 0x18u, x0.l(0));
    wr32(m, eax + 0x8u, edx);
    wr32(m, eax + 0x4u, edx);
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(60);
L_74b3d5:
    ecx = rd32(m, eax + 0x70u);
    eax = (ecx + 0x10u);
    wr32(m, ecx, 0x0u);
    fR_74b3e1 = (eax & eax);
    if ((fR_74b3e1 == 0)) goto L_74b43d;
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x1 = X{};
    x2 = movss_m(rd32(m, 0x9322acu) /* 0.009999999776482582 */);
    edx = 0x1u;
    wr32(m, eax + 0x30u, edx);
    wr32(m, eax + 0x2cu, x0.l(0));
    x0 = cvtsi2ss(x0, esi);
    x0 = mulss(x0, rd32(m, 0x977470u) /* 0.03333333507180214 */);
    fC_74b411 = fcmp32(x0.l(0), x2.l(0));
    wr32(m, eax + 0x20u, x1.l(0));
    wr32(m, eax + 0x24u, x1.l(0));
    wr32(m, eax + 0x28u, x1.l(0));
    if (!(fc_b(fC_74b411) || fc_z(fC_74b411))) goto L_74b428;
    x0 = x2;
L_74b428:
    wr32(m, eax + 0xcu, x0.l(0));
    wr32(m, eax + 0x10u, x1.l(0));
    wr32(m, eax + 0x18u, x1.l(0));
    wr32(m, eax + 0x8u, edx);
    wr32(m, eax + 0x4u, edx);
L_74b43d:
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(60);
}

// 0074b450 Fn_0074b450
//   original FUN_0074b450; 0 instructions per frame (plan); register inputs: edx; returns with ret 0
REFLEX_PORT_FN(0x0074b450, Fn_0074b450, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_74b455 = 0;
    uint32_t fB_74b455 = 0;
    uint32_t fA_74b475 = 0;
    uint32_t fB_74b475 = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = 0u;
    ecx = 0u;
    fA_74b455 = rd32(m, edx + 0x744u);
    fB_74b455 = ebx;
    if (int32_t(sext(fA_74b455, 4)) <= int32_t(sext(fB_74b455, 4))) goto L_74b47e;
    eax = (edx + 0x294u);
    esp -= 4; wr32(m, esp, esi);
L_74b464:
    esi = rd32(m, eax);
    wr32(m, esi + 0x70u, ebx);
    esi = rd32(m, eax);
    ecx = (ecx + 1u);
    wr8(m, esi + 0xa2u, (ebx & 0xffu));
    eax = eax + 0x4u;
    fA_74b475 = ecx;
    fB_74b475 = rd32(m, edx + 0x744u);
    if (int32_t(sext(fA_74b475, 4)) < int32_t(sext(fB_74b475, 4))) goto L_74b464;
    esi = rd32(m, esp); esp += 4;
L_74b47e:
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0074b480 Fn_0074b480
//   original FUN_0074b480; 0 instructions per frame (plan); register inputs: eax; returns with ret 0
REFLEX_PORT_FN(0x0074b480, Fn_0074b480, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_74b488 = 0;
    uint32_t fB_74b488 = 0;
    uint32_t fA_74b4b9 = 0;
    uint32_t fB_74b4b9 = 0;
    uint32_t fA_74b4c7 = 0;
    uint32_t fB_74b4c7 = 0;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    ebx = 0u;
    ecx = 0u;
    fA_74b488 = rd32(m, esi + 0x172cu);
    fB_74b488 = ebx;
    if (int32_t(sext(fA_74b488, 4)) <= int32_t(sext(fB_74b488, 4))) goto L_74b4c1;
    edx = (esi + 0x16ecu);
    goto L_74b4a0;
L_74b4a0:
    eax = rd32(m, edx);
    ecx = (ecx + 1u);
    wr32(m, eax + 0x350u, 0x1u);
    wr32(m, eax + 0x70u, ebx);
    wr8(m, eax + 0xa2u, (ebx & 0xffu));
    edx = edx + 0x4u;
    fA_74b4b9 = ecx;
    fB_74b4b9 = rd32(m, esi + 0x172cu);
    if (int32_t(sext(fA_74b4b9, 4)) < int32_t(sext(fB_74b4b9, 4))) goto L_74b4a0;
L_74b4c1:
    esi = rd32(m, esi + 0x1730u);
    fA_74b4c7 = esi;
    fB_74b4c7 = ebx;
    if (fA_74b4c7 == fB_74b4c7) goto L_74b4de;
    wr32(m, esi + 0x350u, 0x1u);
    wr32(m, esi + 0x70u, ebx);
    wr8(m, esi + 0xa2u, (ebx & 0xffu));
L_74b4de:
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0074b4f0 Fn_0074b4f0
//   original FUN_0074b4f0; 0 instructions per frame (plan); register inputs: eax; returns with ret 0
REFLEX_PORT_FN(0x0074b4f0, Fn_0074b4f0, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_74b500 = 0;
    uint32_t fB_74b500 = 0;
    uint32_t fA_74b514 = 0;
    uint32_t fB_74b514 = 0;
    uint32_t fA_74b525 = 0;
    uint32_t fB_74b525 = 0;
    uint32_t fA_74b539 = 0;
    uint32_t fB_74b539 = 0;
    uint32_t fA_74b54d = 0;
    uint32_t fB_74b54d = 0;
    uint32_t fA_74b56a = 0;
    uint32_t fB_74b56a = 0;
    uint32_t fA_74b58c = 0;
    uint32_t fB_74b58c = 0;
    eax = eax - 0xffffff80u;
    edx = 0x2u;
    ecx = 0u;
    esp -= 4; wr32(m, esp, esi);
    goto L_74b500;
L_74b500:
    fA_74b500 = rd32(m, eax - 0x30u);
    fB_74b500 = ecx;
    if (fA_74b500 == fB_74b500) goto L_74b514;
    esi = rd32(m, eax - 0x30u);
    wr32(m, esi + 0x70u, ecx);
    esi = rd32(m, eax - 0x30u);
    wr8(m, esi + 0xa2u, (ecx & 0xffu));
L_74b514:
    fA_74b514 = rd32(m, eax);
    fB_74b514 = ecx;
    if (fA_74b514 == fB_74b514) goto L_74b525;
    esi = rd32(m, eax);
    wr32(m, esi + 0x70u, ecx);
    esi = rd32(m, eax);
    wr8(m, esi + 0xa2u, (ecx & 0xffu));
L_74b525:
    fA_74b525 = rd32(m, eax + 0x30u);
    fB_74b525 = ecx;
    if (fA_74b525 == fB_74b525) goto L_74b539;
    esi = rd32(m, eax + 0x30u);
    wr32(m, esi + 0x70u, ecx);
    esi = rd32(m, eax + 0x30u);
    wr8(m, esi + 0xa2u, (ecx & 0xffu));
L_74b539:
    fA_74b539 = rd32(m, eax + 0x60u);
    fB_74b539 = ecx;
    if (fA_74b539 == fB_74b539) goto L_74b54d;
    esi = rd32(m, eax + 0x60u);
    wr32(m, esi + 0x70u, ecx);
    esi = rd32(m, eax + 0x60u);
    wr8(m, esi + 0xa2u, (ecx & 0xffu));
L_74b54d:
    fA_74b54d = rd32(m, eax + 0x90u);
    fB_74b54d = ecx;
    if (fA_74b54d == fB_74b54d) goto L_74b56a;
    esi = rd32(m, eax + 0x90u);
    wr32(m, esi + 0x70u, ecx);
    esi = rd32(m, eax + 0x90u);
    wr8(m, esi + 0xa2u, (ecx & 0xffu));
L_74b56a:
    fA_74b56a = rd32(m, eax + 0xc0u);
    fB_74b56a = ecx;
    if (fA_74b56a == fB_74b56a) goto L_74b587;
    esi = rd32(m, eax + 0xc0u);
    wr32(m, esi + 0x70u, ecx);
    esi = rd32(m, eax + 0xc0u);
    wr8(m, esi + 0xa2u, (ecx & 0xffu));
L_74b587:
    eax = eax + 0x120u;
    fA_74b58c = edx;
    fB_74b58c = 0x1u;
    edx = fA_74b58c - fB_74b58c;
    if (fA_74b58c != fB_74b58c) goto L_74b500;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0074b610 Fn_0074b610
//   original FUN_0074b610; 0 instructions per frame (plan); register inputs: eax; returns with ret 0
REFLEX_PORT_FN(0x0074b610, Fn_0074b610, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_74b623 = 0;
    uint32_t fB_74b623 = 0;
    uint32_t fA_74b62f = 0;
    uint32_t fB_74b62f = 0;
    uint32_t fA_74b63c = 0;
    uint32_t fB_74b63c = 0;
    uint32_t fA_74b649 = 0;
    uint32_t fB_74b649 = 0;
    uint32_t fA_74b659 = 0;
    uint32_t fB_74b659 = 0;
    uint32_t fA_74b669 = 0;
    uint32_t fB_74b669 = 0;
    uint32_t fA_74b678 = 0;
    uint32_t fB_74b678 = 0;
    esp -= 4; wr32(m, esp, esi);
    eax = eax - 0xffffff80u;
    esi = 0x2u;
    edx = 0u;
    goto L_74b620;
L_74b620:
    ecx = rd32(m, eax - 0x30u);
    fA_74b623 = ecx;
    fB_74b623 = edx;
    if (fA_74b623 == fB_74b623) goto L_74b62d;
    wr32(m, ecx + 0x378u, edx);
L_74b62d:
    ecx = rd32(m, eax);
    fA_74b62f = ecx;
    fB_74b62f = edx;
    if (fA_74b62f == fB_74b62f) goto L_74b639;
    wr32(m, ecx + 0x378u, edx);
L_74b639:
    ecx = rd32(m, eax + 0x30u);
    fA_74b63c = ecx;
    fB_74b63c = edx;
    if (fA_74b63c == fB_74b63c) goto L_74b646;
    wr32(m, ecx + 0x378u, edx);
L_74b646:
    ecx = rd32(m, eax + 0x60u);
    fA_74b649 = ecx;
    fB_74b649 = edx;
    if (fA_74b649 == fB_74b649) goto L_74b653;
    wr32(m, ecx + 0x378u, edx);
L_74b653:
    ecx = rd32(m, eax + 0x90u);
    fA_74b659 = ecx;
    fB_74b659 = edx;
    if (fA_74b659 == fB_74b659) goto L_74b663;
    wr32(m, ecx + 0x378u, edx);
L_74b663:
    ecx = rd32(m, eax + 0xc0u);
    fA_74b669 = ecx;
    fB_74b669 = edx;
    if (fA_74b669 == fB_74b669) goto L_74b673;
    wr32(m, ecx + 0x378u, edx);
L_74b673:
    eax = eax + 0x120u;
    fA_74b678 = esi;
    fB_74b678 = 0x1u;
    esi = fA_74b678 - fB_74b678;
    if (fA_74b678 != fB_74b678) goto L_74b620;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0074b680 Fn_0074b680
//   original FUN_0074b680; 166 instructions per frame (plan); register inputs: none; returns with ret 8
REFLEX_PORT_FN(0x0074b680, Fn_0074b680, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_74b68a = 0;
    uint32_t fB_74b68a = 0;
    uint32_t fR_74b6b0 = 0;
    uint32_t fA_74b6b8 = 0;
    uint32_t fB_74b6b8 = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    eax = 0u;
    esi = 0u;
    fA_74b68a = rd32(m, ebx + 0x16e8u);
    fB_74b68a = eax;
    if (int32_t(sext(fA_74b68a, 4)) <= int32_t(sext(fB_74b68a, 4))) goto L_74b6d2;
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, 0x9162bcu) /* 8.287018476502604e-39 */;
    esp -= 4; wr32(m, esp, edi);
    edi = (ebx + 0x748u);
L_74b6a0:
    eax = rd32(m, edi);
    ecx = rd32(m, esp + 0x18u);
    eax = rd32(m, eax + 0x18u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(ebp, 0x74b6adu);
    esp = esp + 0x8u;
    fR_74b6b0 = (eax & eax);
    if ((fR_74b6b0 == 0)) goto L_74b6c9;
    esi = (esi + 1u);
    edi = edi + 0x4u;
    fA_74b6b8 = esi;
    fB_74b6b8 = rd32(m, ebx + 0x16e8u);
    if (int32_t(sext(fA_74b6b8, 4)) < int32_t(sext(fB_74b6b8, 4))) goto L_74b6a0;
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(8);
L_74b6c9:
    eax = rd32(m, ebx + esi * 4 + 0x748u);
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
L_74b6d2:
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 0074b750 Fn_0074b750
//   original FUN_0074b750; 4 instructions per frame (plan); register inputs: eax; returns with ret 8
REFLEX_PORT_FN(0x0074b750, Fn_0074b750, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_74b755 = 0;
    uint32_t fB_74b755 = 0;
    uint32_t fR_74b778 = 0;
    uint32_t fR_74b792 = 0;
    uint32_t fR_74b7a9 = 0;
    uint32_t fA_74b7af = 0;
    uint32_t fB_74b7af = 0;
    uint32_t fR_74b7ca = 0;
    uint32_t fR_74b7f3 = 0;
    uint32_t fR_74b805 = 0;
    uint32_t fR_74b823 = 0;
    uint32_t fA_74b829 = 0;
    uint32_t fB_74b829 = 0;
    uint32_t fA_74b83d = 0;
    uint32_t fB_74b83d = 0;
    uint32_t fA_74b855 = 0;
    uint32_t fB_74b855 = 0;
    uint32_t fR_74b883 = 0;
    uint32_t fR_74b895 = 0;
    uint32_t fR_74b8b3 = 0;
    uint32_t fA_74b8b9 = 0;
    uint32_t fB_74b8b9 = 0;
    uint32_t fA_74b8cb = 0;
    uint32_t fB_74b8cb = 0;
    uint32_t fA_74b8d4 = 0;
    uint32_t fB_74b8d4 = 0;
    uint32_t fR_74b8e7 = 0;
    uint32_t fR_74b905 = 0;
    uint32_t fR_74b917 = 0;
    uint32_t fR_74b933 = 0;
    uint32_t fA_74b939 = 0;
    uint32_t fB_74b939 = 0;
    uint32_t fA_74b94b = 0;
    uint32_t fB_74b94b = 0;
    uint32_t fR_74b95a = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x8u);
    fA_74b755 = rd32(m, ebp + 0x40u);
    fB_74b755 = 0x0u;
    esp -= 4; wr32(m, esp, edi);
    edi = eax;
    if (fA_74b755 != fB_74b755) goto L_74b765;
    edi = rd32(m, esp); esp += 4;
    eax = 0u;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(8);
L_74b765:
    eax = 0x96cca8u;
    wr32(m, esp + 0xcu, 0x0u);
    edx = (eax + 0x1u);
L_74b775:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    eax = (eax + 1u);
    fR_74b778 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_74b778 == 0)) goto L_74b775;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, 0x916414u) /* 8.294448160960454e-39 */;
    eax = eax - edx;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, 0x96cca8u);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(esi, 0x74b78fu);
    esp = esp + 0xcu;
    fR_74b792 = (eax & eax);
    if (!(fR_74b792 == 0)) goto L_74b7e1;
    eax = 0x96cca8u;
    wr32(m, esp + 0x14u, 0x1u);
    edx = (eax + 0x1u);
L_74b7a6:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    eax = (eax + 1u);
    fR_74b7a9 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_74b7a9 == 0)) goto L_74b7a6;
    eax = eax - edx;
    fA_74b7af = eax;
    fB_74b7af = edi;
    eax = fA_74b7af + fB_74b7af;
    if ((((fA_74b7af + fB_74b7af)) == 0)) goto L_74b964;
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x916220u) /* 8.286385089596729e-39 */, 0x74b7beu);
    esp = esp + 0x4u;
    eax = (eax - 1u);
    esp -= 4; wr32(m, esp, eax);
    ecx = ebp;
    LIFT_CALL(0x74b6e0u, 0x74b7cau);  // FUN_0074b6e0
    fR_74b7ca = (eax & eax);
    if ((fR_74b7ca == 0)) goto L_74b964;
    esp -= 4; wr32(m, esp, eax);
    edi = ebp;
    LIFT_CALL(0x74b080u, 0x74b7dau);  // FUN_0074b080
    eax = rd32(m, eax);
    goto L_74b95a;
L_74b7e1:
    eax = 0x96cc8cu;
    edx = (eax + 0x1u);
L_74b7f0:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    eax = (eax + 1u);
    fR_74b7f3 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_74b7f3 == 0)) goto L_74b7f0;
    eax = eax - edx;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, 0x96cc8cu);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(esi, 0x74b802u);
    esp = esp + 0xcu;
    fR_74b805 = (eax & eax);
    if (!(fR_74b805 == 0)) goto L_74b877;
    eax = 0x96cc8cu;
    wr32(m, esp + 0x14u, 0x1u);
    edx = (eax + 0x1u);
L_74b820:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    eax = (eax + 1u);
    fR_74b823 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_74b823 == 0)) goto L_74b820;
    eax = eax - edx;
    fA_74b829 = eax;
    fB_74b829 = edi;
    eax = fA_74b829 + fB_74b829;
    if ((((fA_74b829 + fB_74b829)) == 0)) goto L_74b964;
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x916220u) /* 8.286385089596729e-39 */, 0x74b838u);
    esi = eax;
    esp = esp + 0x4u;
    fA_74b83d = esi;
    fB_74b83d = 0x1u;
    esi = fA_74b83d - fB_74b83d;
    if (sgn(((fA_74b83d - fB_74b83d)), 4)) goto L_74b964;
    ebx = rd32(m, ebp + 0x40u);
    edi = 0x1u;
    edx = ebx;
    LIFT_CALL(0x559e20u, 0x74b855u);  // FUN_00559e20
    fA_74b855 = esi;
    fB_74b855 = eax;
    if (int32_t(sext(fA_74b855, 4)) >= int32_t(sext(fB_74b855, 4))) goto L_74b964;
    esp -= 4; wr32(m, esp, esi);
    eax = ebx;
    LIFT_CALL(0x559e60u, 0x74b865u);  // FUN_00559e60
    eax = rd32(m, eax + 0x74u);
    esp -= 4; wr32(m, esp, eax);
    edi = ebp;
    LIFT_CALL(0x74b080u, 0x74b870u);  // FUN_0074b080
    eax = rd32(m, eax);
    goto L_74b95a;
L_74b877:
    eax = 0x96cc98u;
    edx = (eax + 0x1u);
L_74b880:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    eax = (eax + 1u);
    fR_74b883 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_74b883 == 0)) goto L_74b880;
    eax = eax - edx;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, 0x96cc98u);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(esi, 0x74b892u);
    esp = esp + 0xcu;
    fR_74b895 = (eax & eax);
    if (!(fR_74b895 == 0)) goto L_74b8fa;
    eax = 0x96cc98u;
    wr32(m, esp + 0x14u, 0x1u);
    edx = (eax + 0x1u);
L_74b8b0:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    eax = (eax + 1u);
    fR_74b8b3 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_74b8b3 == 0)) goto L_74b8b0;
    eax = eax - edx;
    fA_74b8b9 = eax;
    fB_74b8b9 = edi;
    eax = fA_74b8b9 + fB_74b8b9;
    if ((((fA_74b8b9 + fB_74b8b9)) == 0)) goto L_74b964;
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x916220u) /* 8.286385089596729e-39 */, 0x74b8c8u);
    esp = esp + 0x4u;
    fA_74b8cb = eax;
    fB_74b8cb = 0x1u;
    eax = fA_74b8cb - fB_74b8cb;
    if (sgn(((fA_74b8cb - fB_74b8cb)), 4)) goto L_74b964;
    fA_74b8d4 = eax;
    fB_74b8d4 = 0xcu;
    if (fA_74b8d4 >= fB_74b8d4) goto L_74b964;
    ecx = rd32(m, ebp + 0x40u);
    eax = rd32(m, ecx + eax * 4 + 0x2d0u);
    fR_74b8e7 = (eax & eax);
    if ((fR_74b8e7 == 0)) goto L_74b964;
    edx = rd32(m, eax + 0x74u);
    esp -= 4; wr32(m, esp, edx);
    edi = ebp;
    LIFT_CALL(0x74b080u, 0x74b8f6u);  // FUN_0074b080
    eax = rd32(m, eax);
    goto L_74b95a;
L_74b8fa:
    eax = 0x96ccb8u;
    edx = (eax + 0x1u);
L_74b902:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    eax = (eax + 1u);
    fR_74b905 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_74b905 == 0)) goto L_74b902;
    eax = eax - edx;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, 0x96ccb8u);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(esi, 0x74b914u);
    esp = esp + 0xcu;
    fR_74b917 = (eax & eax);
    if (!(fR_74b917 == 0)) goto L_74b964;
    eax = 0x96ccb8u;
    wr32(m, esp + 0x14u, 0x1u);
    edx = (eax + 0x1u);
    goto L_74b930;
L_74b930:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    eax = (eax + 1u);
    fR_74b933 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_74b933 == 0)) goto L_74b930;
    eax = eax - edx;
    fA_74b939 = eax;
    fB_74b939 = edi;
    eax = fA_74b939 + fB_74b939;
    if ((((fA_74b939 + fB_74b939)) == 0)) goto L_74b964;
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x916220u) /* 8.286385089596729e-39 */, 0x74b944u);
    ecx = rd32(m, ebp + 0x40u);
    eax = (eax - 1u);
    esp = esp + 0x4u;
    fA_74b94b = rd32(m, ecx + 0x26cu);
    fB_74b94b = eax;
    if (int32_t(sext(fA_74b94b, 4)) <= int32_t(sext(fB_74b94b, 4))) goto L_74b964;
    eax = rd32(m, ebp + eax * 4 + 0x16ecu);
L_74b95a:
    fR_74b95a = (eax & eax);
    if ((fR_74b95a == 0)) goto L_74b964;
    edx = rd32(m, esp + 0x18u);
    wr32(m, edx, eax);
L_74b964:
    eax = rd32(m, esp + 0x14u);
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 0074b970 Fn_0074b970
//   original FUN_0074b970; 1 instructions per frame (plan); register inputs: ecx esi edi; returns with ret 0
REFLEX_PORT_FN(0x0074b970, Fn_0074b970, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_74b971 = 0;
    uint32_t fB_74b971 = 0;
    uint32_t fR_74b994 = 0;
    uint32_t fR_74b99c = 0;
    uint32_t fR_74b9b1 = 0;
    uint32_t fR_74b9bf = 0;
    uint32_t fR_74b9d2 = 0;
    uint32_t fR_74b9e0 = 0;
    uint32_t fR_74b9f4 = 0;
    uint32_t fR_74b9ff = 0;
    uint32_t fR_74ba18 = 0;
    esp -= 4; wr32(m, esp, ecx);
    fA_74b971 = rd32(m, esi + 0x40u);
    fB_74b971 = 0x0u;
    if (fA_74b971 != fB_74b971) goto L_74b97b;
    eax = 0u;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_74b97b:
    eax = (esp);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, esi);
    eax = edi;
    wr32(m, esp + 0x8u, 0x0u);
    LIFT_CALL(0x74b750u, 0x74b98fu);  // FUN_0074b750
    ecx = eax;
    eax = rd32(m, esp);
    fR_74b994 = (eax & eax);
    if (!(fR_74b994 == 0)) goto L_74ba33;
    fR_74b99c = (ecx & ecx);
    if (!(fR_74b99c == 0)) goto L_74ba33;
    eax = rd32(m, esi + 0x1730u);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, 0x9162bcu) /* 8.287018476502604e-39 */;
    fR_74b9b1 = (eax & eax);
    if ((fR_74b9b1 == 0)) goto L_74b9cc;
    eax = rd32(m, eax + 0x18u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(ebx, 0x74b9bcu);
    esp = esp + 0x8u;
    fR_74b9bf = (eax & eax);
    if (!(fR_74b9bf == 0)) goto L_74b9cc;
    eax = rd32(m, esi + 0x1730u);
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_74b9cc:
    eax = rd32(m, esi + 0x290u);
    fR_74b9d2 = (eax & eax);
    if ((fR_74b9d2 == 0)) goto L_74b9ed;
    eax = rd32(m, eax + 0x18u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(ebx, 0x74b9ddu);
    esp = esp + 0x8u;
    fR_74b9e0 = (eax & eax);
    if (!(fR_74b9e0 == 0)) goto L_74b9ed;
    eax = rd32(m, esi + 0x290u);
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_74b9ed:
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x74b120u, 0x74b9f4u);  // FUN_0074b120
    fR_74b9f4 = (eax & eax);
    if (!(fR_74b9f4 == 0)) goto L_74ba32;
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x74b0c0u, 0x74b9ffu);  // FUN_0074b0c0
    fR_74b9ff = (eax & eax);
    if (!(fR_74b9ff == 0)) goto L_74ba32;
    ecx = rd32(m, esi + 0x40u);
    eax = rd32(m, ecx + 0x234u);
    eax = eax + 0x20150u;
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x74b680u, 0x74ba18u);  // FUN_0074b680
    fR_74ba18 = (eax & eax);
    if (!(fR_74ba18 == 0)) goto L_74ba32;
    edx = rd32(m, esi + 0x40u);
    eax = rd32(m, edx + 0x234u);
    eax = rd32(m, eax + 0x20138u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x7d2da0u, 0x74ba32u);  // FUN_007d2da0
L_74ba32:
    ebx = rd32(m, esp); esp += 4;
L_74ba33:
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0074bd50 Fn_0074bd50
//   original FUN_0074bd50; 0 instructions per frame (plan); register inputs: none; returns with ret 4
REFLEX_PORT_FN(0x0074bd50, Fn_0074bd50, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_74bd61 = 0;
    uint32_t fB_74bd61 = 0;
    uint32_t fR_74bd8a = 0;
    uint32_t fR_74bdab = 0;
    uint32_t fA_74bdc0 = 0;
    uint32_t fB_74bdc0 = 0;
    uint32_t fA_74bde7 = 0;
    uint32_t fB_74bde7 = 0;
    uint32_t fR_74bdfc = 0;
    eax = rd32(m, 0xa95844u);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, eax + 0x10u);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(rd32(m, 0x91607cu) /* 8.295210467325047e-39 */, 0x74bd61u);
    fA_74bd61 = eax;
    fB_74bd61 = rd32(m, esi + 0xccu);
    if (fA_74bd61 == fB_74bd61) goto L_74bd78;
    ecx = rd32(m, esi + 0xdcu);
    esp -= 4; wr32(m, esp, 0xffffffffu);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(rd32(m, 0x916008u) /* 8.295117981626401e-39 */, 0x74bd78u);
L_74bd78:
    edi = rd32(m, esp + 0x10u);
    edi = edi + 0x50u;
    wr32(m, esp + 0x10u, edi);
    ebp = 0xcu;
L_74bd88:
    eax = rd32(m, edi);
    fR_74bd8a = (eax & eax);
    if ((fR_74bd8a == 0)) goto L_74bdbd;
    wr8(m, eax + 0xa2u, 0x0u);
    esi = rd32(m, edi);
    edx = rd32(m, esi + 0x10u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x7d3040u, 0x74bda0u);  // FUN_007d3040
    ecx = rd32(m, edi);
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x24u);
    LIFT_CALL(edx, 0x74bda9u);
    ecx = rd32(m, edi);
    fR_74bdab = (ecx & ecx);
    if ((fR_74bdab == 0)) goto L_74bdbd;
    eax = rd32(m, ecx);
    edx = rd32(m, eax);
    esp -= 4; wr32(m, esp, 0x1u);
    LIFT_CALL(edx, 0x74bdb7u);
    wr32(m, edi, 0x0u);
L_74bdbd:
    edi = edi + 0x30u;
    fA_74bdc0 = ebp;
    fB_74bdc0 = 0x1u;
    ebp = fA_74bdc0 - fB_74bdc0;
    if (fA_74bdc0 != fB_74bdc0) goto L_74bd88;
    eax = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, 0x240u);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x8fa06au, 0x74bdd5u);  // MSVCR90.DLL::memset
    ecx = rd32(m, 0xa95844u);
    esi = rd32(m, ecx + 0x10u);
    esp = esp + 0xcu;
    LIFT_CALL(rd32(m, 0x91607cu) /* 8.295210467325047e-39 */, 0x74bde7u);
    fA_74bde7 = eax;
    fB_74bde7 = rd32(m, esi + 0xccu);
    if (fA_74bde7 == fB_74bde7) goto L_74be06;
    edx = rd32(m, esi + 0xdcu);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(rd32(m, 0x916034u) /* 8.295148810192616e-39 */, 0x74bdfcu);
    fR_74bdfc = (eax & eax);
    if (!(fR_74bdfc == 0)) goto L_74be06;
    LIFT_CALL(rd32(m, 0x916078u) /* 8.295188046549617e-39 */, 0x74be06u);
L_74be06:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 0074be10 Fn_0074be10
//   original FUN_0074be10; 0 instructions per frame (plan); register inputs: ecx; returns with ret 16
REFLEX_PORT_FN(0x0074be10, Fn_0074be10, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_74be50 = 0;
    uint32_t fB_74be50 = 0;
    uint32_t fA_74be58 = 0;
    uint32_t fB_74be58 = 0;
    uint32_t fA_74be85 = 0;
    uint32_t fB_74be85 = 0;
    uint32_t fR_74be97 = 0;
    uint32_t fR_74beb5 = 0;
    uint32_t fA_74bfa9 = 0;
    uint32_t fB_74bfa9 = 0;
    uint32_t fA_74bfbe = 0;
    uint32_t fB_74bfbe = 0;
    uint32_t fA_74bfcf = 0;
    uint32_t fB_74bfcf = 0;
    uint32_t fR_74c01e = 0;
    uint32_t fA_74c085 = 0;
    uint32_t fB_74c085 = 0;
    uint32_t fA_74c08d = 0;
    uint32_t fB_74c08d = 0;
    uint32_t fA_74c0a2 = 0;
    uint32_t fB_74c0a2 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x1b4u;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp + 0x1b0u, eax);
    eax = rd32(m, ebp + 0x10u);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, ebp + 0xcu);
    esp -= 4; wr32(m, esp, edi);
    edi = ecx;
    wr32(m, esp + 0x14u, eax);
    wr32(m, esp + 0x1cu, edi);
    wr32(m, esp + 0x18u, ebx);
    eax = 0u;
    ecx = (edi + 0x50u);
L_74be50:
    fA_74be50 = rd32(m, ecx);
    fB_74be50 = ebx;
    if (fA_74be50 == fB_74be50) goto L_74be63;
    eax = (eax + 1u);
    ecx = ecx + 0x30u;
    fA_74be58 = eax;
    fB_74be58 = 0xcu;
    if (fA_74be58 < fB_74be58) goto L_74be50;
    ebx = rd32(m, esp + 0x1cu);
    goto L_74be6d;
L_74be63:
    ecx = (eax + eax * 2);
    ecx = (ecx << 4);
    ebx = (ecx + edi + 0x50u);
L_74be6d:
    eax = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0x17u);
    wr8(m, esp + 0x17u, 0x0u);
    LIFT_CALL(0x6b6a30u, 0x74be80u);  // FUN_006b6a30
    edx = 0u;
    esp = esp + 0x4u;
    fA_74be85 = eax;
    fB_74be85 = edx;
    if (fA_74be85 == fB_74be85) goto L_74be8f;
    wr32(m, esp + 0x14u, eax);
    goto L_74be97;
L_74be8f:
    ecx = rd32(m, esp + 0x14u);
    wr32(m, esp + 0x14u, ecx);
L_74be97:
    fR_74be97 = (rd8(m, esp + 0x13u) & 0x2u);
    if ((fR_74be97 == 0)) goto L_74c07b;
    eax = rd32(m, ebx);
    edx = rd32(m, ebx + 0x4u);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, edx + 0x1acu);
    edx = edi;
    LIFT_CALL(0x74af60u, 0x74beb5u);  // FUN_0074af60
    fR_74beb5 = (esi & esi);
    if ((fR_74beb5 == 0)) goto L_74bfa0;
    edx = rd32(m, esi);
    edx = rd32(m, edx + 0x3cu);
    eax = (esp + 0x70u);
    esp -= 4; wr32(m, esp, eax);
    ecx = esi;
    LIFT_CALL(edx, 0x74becbu);
    x0 = movss_m(rd32(m, esp + 0xa0u));
    eax = rd32(m, ebx + 0x4u);
    wr32(m, esp + 0x20u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0xa4u));
    wr32(m, esp + 0x24u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0xa8u));
    wr32(m, esp + 0x28u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x90u));
    wr32(m, esp + 0x30u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x94u));
    wr32(m, esp + 0x34u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x98u));
    wr32(m, esp + 0x38u, x0.l(0));
    esi = rd32(m, eax + 0x1acu);
    ecx = rd32(m, esi + 0x234u);
    edx = rd32(m, ecx);
    edx = rd32(m, edx + 0x50u);
    eax = (esp + 0x50u);
    esp -= 4; wr32(m, esp, eax);
    eax = (esp + 0x64u);
    esp -= 4; wr32(m, esp, eax);
    eax = (esp + 0x48u);
    esp -= 4; wr32(m, esp, eax);
    eax = (esp + 0x3cu);
    esp -= 4; wr32(m, esp, eax);
    eax = (esp + 0x30u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(edx, 0x74bf54u);
    eax = (esp + 0x60u);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0x44u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, esi);
    ecx = (esp + 0x5cu);
    LIFT_CALL(0x45f600u, 0x74bf68u);  // FUN_0045f600
    edx = rd32(m, ebx + 0x4u);
    ecx = rd32(m, edx + 0x1acu);
    eax = rd32(m, ecx + 0x1328u);
    wr8(m, eax + 0x148u, 0xffu);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x80u);
    esp -= 4; wr32(m, esp, 0x1u);
    LIFT_CALL(eax, 0x74bf8au);
    ecx = rd32(m, ebx + 0x4u);
    edx = rd32(m, ecx + 0x1acu);
    eax = rd32(m, edx + 0x1328u);
    wr8(m, eax + 0x14eu, 0x1u);
L_74bfa0:
    eax = rd32(m, ebx + 0x4u);
    ecx = rd32(m, eax + 0x1acu);
    fA_74bfa9 = rd32(m, ecx + 0x194u);
    fB_74bfa9 = 0x1u;
    if (fA_74bfa9 > fB_74bfa9) goto L_74c052;
    edx = ecx;
    eax = rd32(m, edx + 0x194u);
    fA_74bfbe = eax;
    fB_74bfbe = 0x0u;
    eax = fA_74bfbe - fB_74bfbe;
    esi = 0x104u;
    edi = (esp + 0xb0u);
    if (fA_74bfbe == fB_74bfbe) goto L_74bfec;
    fA_74bfcf = eax;
    fB_74bfcf = 0x1u;
    eax = fA_74bfcf - fB_74bfcf;
    if (fA_74bfcf == fB_74bfcf) goto L_74bfe0;
    eax = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, 0x941e90u);
    goto L_74bff6;
L_74bfe0:
    ecx = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, 0x941e80u);
    goto L_74bff6;
L_74bfec:
    edx = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, 0x941e88u);
L_74bff6:
    LIFT_CALL(0x78fd60u, 0x74bffbu);  // FUN_0078fd60
    eax = rd32(m, 0xa95844u);
    ecx = rd32(m, eax + 0x8u);
    edx = rd32(m, ecx);
    edx = rd32(m, edx + 0x6cu);
    esp = esp + 0x8u;
    esp -= 4; wr32(m, esp, 0x1u);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0x6d696e61u);
    eax = (esp + 0xbcu);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(edx, 0x74c01eu);
    fR_74c01e = (eax & eax);
    if ((fR_74c01e == 0)) goto L_74c04e;
    ecx = rd32(m, ebp + 0x14u);
    edx = rd32(m, ebx + 0x4u);
    edx = rd32(m, edx + 0x1acu);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x47c310u, 0x74c035u);  // FUN_0047c310
    eax = rd32(m, ebx + 0x4u);
    ecx = rd32(m, eax + 0x1acu);
    edx = rd32(m, ecx + 0x1328u);
    esp = esp + 0x8u;
    wr8(m, edx + 0x14eu, 0x1u);
L_74c04e:
    edi = rd32(m, esp + 0x1cu);
L_74c052:
    eax = rd32(m, esp + 0x18u);
    ecx = rd32(m, edi + 0x40u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    ecx = rd32(m, esp + 0x1cu);
    LIFT_CALL(0x55e360u, 0x74c064u);  // FUN_0055e360
    ecx = rd32(m, esp + 0x1bcu);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x74c075u);  // __security_check_cookie
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(16);
L_74c07b:
    ebx = rd32(m, ebx);
    eax = 0u;
    ecx = (edi + 0x1734u);
L_74c085:
    fA_74c085 = ebx;
    fB_74c085 = rd32(m, ecx);
    if (fA_74c085 == fB_74c085) goto L_74c094;
    eax = (eax + 1u);
    ecx = ecx + 0x8u;
    fA_74c08d = eax;
    fB_74c08d = 0xcu;
    if (int32_t(sext(fA_74c08d, 4)) < int32_t(sext(fB_74c08d, 4))) goto L_74c085;
    goto L_74c0a2;
L_74c094:
    wr32(m, edi + eax * 8 + 0x1734u, edx);
    wr32(m, edi + eax * 8 + 0x1738u, edx);
L_74c0a2:
    fA_74c0a2 = esi;
    fB_74c0a2 = edx;
    if (fA_74c0a2 == fB_74c0a2) goto L_74c052;
    eax = rd32(m, esp + 0x18u);
    ebx = rd32(m, eax);
    edx = rd32(m, esi);
    eax = rd32(m, edx + 0x38u);
    ecx = esi;
    ebx = ebx + 0x40u;
    LIFT_CALL(eax, 0x74c0b8u);
    ecx = rd32(m, esp + 0x18u);
    edx = rd32(m, ebx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(edx, 0x74c0c1u);
    goto L_74c052;
}

// 0074c7e0 Fn_0074c7e0
//   original FUN_0074c7e0; 2 instructions per frame (plan); register inputs: ecx edx; returns with ret 0
REFLEX_PORT_FN(0x0074c7e0, Fn_0074c7e0, "anim", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = rd32(m, ecx);
    eax = rd32(m, eax + 0x110u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(eax, 0x74c7ecu);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, 0x9419e4u);
    esi = 0x40u;
    LIFT_CALL(0x78fd60u, 0x74c7fcu);  // FUN_0078fd60
    esp = esp + 0x8u;
    eax = (esi - 0x3fu);
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0074c810 Fn_0074c810
//   original FUN_0074c810; 0 instructions per frame (plan); register inputs: ecx edx; returns with ret 4
REFLEX_PORT_FN(0x0074c810, Fn_0074c810, "anim", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = rd32(m, ecx);
    eax = rd32(m, eax + 0x114u);
    esp -= 4; wr32(m, esp, 0x1u);
    esp -= 4; wr32(m, esp, edx);
    edx = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(eax, 0x74c822u);
    eax = 0x1u;
    LIFT_RET(4);
}

// 0074c830 Fn_0074c830
//   original FUN_0074c830; 5 instructions per frame (plan); register inputs: none; returns with ret 12
REFLEX_PORT_FN(0x0074c830, Fn_0074c830, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_74c842 = 0;
    uint32_t fB_74c842 = 0;
    uint32_t fR_74c864 = 0;
    uint32_t fR_74c896 = 0;
    uint32_t fR_74c8c8 = 0;
    uint32_t fR_74c8ea = 0;
    uint32_t fR_74c91c = 0;
    uint32_t fR_74c93e = 0;
    uint32_t fR_74c972 = 0;
    uint32_t fR_74c994 = 0;
    uint32_t fR_74c9d2 = 0;
    uint32_t fR_74c9f4 = 0;
    uint32_t fR_74ca30 = 0;
    uint32_t fR_74ca52 = 0;
    uint32_t fR_74ca65 = 0;
    uint32_t fR_74ca7f = 0;
    uint32_t fR_74cab6 = 0;
    uint32_t fR_74cae2 = 0;
    uint32_t fR_74cb0e = 0;
    uint32_t fR_74cb32 = 0;
    uint32_t fR_74cb9c = 0;
    uint32_t fR_74cbbe = 0;
    uint32_t fR_74cbe3 = 0;
    uint32_t fR_74cc05 = 0;
    uint32_t fR_74cc27 = 0;
    uint32_t fR_74cc63 = 0;
    uint32_t fR_74cca0 = 0;
    uint32_t fR_74cce5 = 0;
    uint32_t fA_74ccff = 0;
    uint32_t fB_74ccff = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, esp + 0x10u);
    eax = rd32(m, esi);
    esp -= 4; wr32(m, esp, edi);
    fA_74c842 = eax;
    fB_74c842 = 0x17u;
    if (fA_74c842 > fB_74c842) goto L_74cc51;
    switch (eax) { case 0: goto L_74c852; case 1: goto L_74c884; case 2: goto L_74cd12; case 3: goto L_74c8b6; case 4: goto L_74c90a; case 5: goto L_74c960; case 6: goto L_74cd12; case 7: goto L_74c9c0; case 8: goto L_74ca1e; case 9: goto L_74c9c0; case 10: goto L_74ca1e; case 11: goto L_74caa4; case 12: goto L_74c8b6; case 13: goto L_74c90a; case 14: goto L_74c852; case 15: goto L_74cb5c; case 16: goto L_74cb8a; case 17: goto L_74cb5c; case 18: goto L_74cb73; case 19: goto L_74cb8a; case 20: goto L_74c9b6; case 21: goto L_74cc51; case 22: goto L_74cc3a; case 23: goto L_74cbf3; default: throw std::runtime_error("jump table"); }
L_74c852:
    eax = rd32(m, ebx);
    edx = rd32(m, eax + 0xe8u);
    esp -= 4; wr32(m, esp, 0x96c3dcu);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(edx, 0x74c864u);
    fR_74c864 = (eax & eax);
    if ((fR_74c864 == 0)) goto L_74cd12;
    edi = (esi + 0x48u);
    edx = eax;
    ecx = ebx;
    LIFT_CALL(0x74c7e0u, 0x74c878u);  // FUN_0074c7e0
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = 0x1u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(12);
L_74c884:
    esp -= 4; wr32(m, esp, 0x96c3d0u);
L_74c889:
    eax = rd32(m, ebx);
    edx = rd32(m, eax + 0xe8u);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(edx, 0x74c896u);
L_74c896:
    fR_74c896 = (eax & eax);
    if ((fR_74c896 == 0)) goto L_74cd12;
    edi = (esi + 0x8u);
L_74c8a1:
    edx = eax;
    ecx = ebx;
    LIFT_CALL(0x74c7e0u, 0x74c8aau);  // FUN_0074c7e0
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = 0x1u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(12);
L_74c8b6:
    eax = rd32(m, ebx);
    edx = rd32(m, eax + 0xe8u);
    esp -= 4; wr32(m, esp, 0x96c3d0u);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(edx, 0x74c8c8u);
    fR_74c8c8 = (eax & eax);
    if ((fR_74c8c8 == 0)) goto L_74c8d8;
    edi = (esi + 0x8u);
    edx = eax;
    ecx = ebx;
    LIFT_CALL(0x74c7e0u, 0x74c8d8u);  // FUN_0074c7e0
L_74c8d8:
    eax = rd32(m, ebx);
    edx = rd32(m, eax + 0xe8u);
    esp -= 4; wr32(m, esp, 0x96c3dcu);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(edx, 0x74c8eau);
    fR_74c8ea = (eax & eax);
    if ((fR_74c8ea == 0)) goto L_74cd12;
    edi = (esi + 0x48u);
    edx = eax;
    ecx = ebx;
    LIFT_CALL(0x74c7e0u, 0x74c8feu);  // FUN_0074c7e0
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = 0x1u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(12);
L_74c90a:
    eax = rd32(m, ebx);
    edx = rd32(m, eax + 0xe8u);
    esp -= 4; wr32(m, esp, 0x96c3d0u);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(edx, 0x74c91cu);
    fR_74c91c = (eax & eax);
    if ((fR_74c91c == 0)) goto L_74c92c;
    edi = (esi + 0x8u);
    edx = eax;
    ecx = ebx;
    LIFT_CALL(0x74c7e0u, 0x74c92cu);  // FUN_0074c7e0
L_74c92c:
    eax = rd32(m, ebx);
    edx = rd32(m, eax + 0xe8u);
    esp -= 4; wr32(m, esp, 0x94fc70u);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(edx, 0x74c93eu);
L_74c93e:
    fR_74c93e = (eax & eax);
    if ((fR_74c93e == 0)) goto L_74cd12;
    esp -= 4; wr32(m, esp, eax);
    edx = (esi + 0xd0u);
    ecx = ebx;
    LIFT_CALL(0x74c810u, 0x74c954u);  // FUN_0074c810
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = 0x1u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(12);
L_74c960:
    eax = rd32(m, ebx);
    edx = rd32(m, eax + 0xe8u);
    esp -= 4; wr32(m, esp, 0x95f1ecu);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(edx, 0x74c972u);
    fR_74c972 = (eax & eax);
    if ((fR_74c972 == 0)) goto L_74c982;
    edi = (esi + 0x8u);
    edx = eax;
    ecx = ebx;
    LIFT_CALL(0x74c7e0u, 0x74c982u);  // FUN_0074c7e0
L_74c982:
    eax = rd32(m, ebx);
    edx = rd32(m, eax + 0xe8u);
    esp -= 4; wr32(m, esp, 0x94fc70u);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(edx, 0x74c994u);
    fR_74c994 = (eax & eax);
    if ((fR_74c994 == 0)) goto L_74cd12;
    esp -= 4; wr32(m, esp, eax);
    edx = (esi + 0xd0u);
    ecx = ebx;
    LIFT_CALL(0x74c810u, 0x74c9aau);  // FUN_0074c810
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = 0x1u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(12);
L_74c9b6:
    esp -= 4; wr32(m, esp, 0x96c3b4u);
    goto L_74c889;
L_74c9c0:
    eax = rd32(m, ebx);
    edx = rd32(m, eax + 0xe8u);
    esp -= 4; wr32(m, esp, 0x95db5cu);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(edx, 0x74c9d2u);
    fR_74c9d2 = (eax & eax);
    if ((fR_74c9d2 == 0)) goto L_74c9e2;
    edi = (esi + 0x8u);
    edx = eax;
    ecx = ebx;
    LIFT_CALL(0x74c7e0u, 0x74c9e2u);  // FUN_0074c7e0
L_74c9e2:
    eax = rd32(m, ebx);
    edx = rd32(m, eax + 0xe8u);
    esp -= 4; wr32(m, esp, 0x93dd58u);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(edx, 0x74c9f4u);
    fR_74c9f4 = (eax & eax);
    if ((fR_74c9f4 == 0)) goto L_74cd12;
    edx = rd32(m, ebx);
    esp -= 4; wr32(m, esp, 0x1u);
    esi = esi + 0x110u;
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, edx + 0x134u);
    ecx = ebx;
    LIFT_CALL(eax, 0x74ca12u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = 0x1u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(12);
L_74ca1e:
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0xe8u);
    esp -= 4; wr32(m, esp, 0x95db5cu);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(eax, 0x74ca30u);
    fR_74ca30 = (eax & eax);
    if ((fR_74ca30 == 0)) goto L_74ca40;
    edi = (esi + 0x8u);
    edx = eax;
    ecx = ebx;
    LIFT_CALL(0x74c7e0u, 0x74ca40u);  // FUN_0074c7e0
L_74ca40:
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0xe8u);
    esp -= 4; wr32(m, esp, 0x96c3c4u);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(eax, 0x74ca52u);
    fR_74ca52 = (eax & eax);
    if ((fR_74ca52 == 0)) goto L_74ca6d;
    edi = (esi + 0x88u);
    edx = eax;
    ecx = ebx;
    LIFT_CALL(0x74c7e0u, 0x74ca65u);  // FUN_0074c7e0
    fR_74ca65 = (eax & eax);
    if (!(fR_74ca65 == 0)) goto L_74cd12;
L_74ca6d:
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0xe8u);
    esp -= 4; wr32(m, esp, 0x94fc70u);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(eax, 0x74ca7fu);
    fR_74ca7f = (eax & eax);
    if ((fR_74ca7f == 0)) goto L_74ca91;
    esp -= 4; wr32(m, esp, eax);
    edx = (esi + 0xd0u);
    ecx = ebx;
    LIFT_CALL(0x74c810u, 0x74ca91u);  // FUN_0074c810
L_74ca91:
    edi = rd32(m, esp); esp += 4;
    wr8(m, esi + 0x88u, 0x0u);
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = 0x1u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(12);
L_74caa4:
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0xe8u);
    esp -= 4; wr32(m, esp, 0x96c3a8u);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(eax, 0x74cab6u);
    fR_74cab6 = (eax & eax);
    if ((fR_74cab6 == 0)) goto L_74cad0;
    edx = rd32(m, ebx);
    edx = rd32(m, edx + 0x134u);
    esp -= 4; wr32(m, esp, 0x1u);
    ecx = (esi + 0xc8u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, eax);
    ecx = ebx;
    LIFT_CALL(edx, 0x74cad0u);
L_74cad0:
    eax = rd32(m, ebx);
    edx = rd32(m, eax + 0xe8u);
    esp -= 4; wr32(m, esp, 0x93dd58u);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(edx, 0x74cae2u);
    fR_74cae2 = (eax & eax);
    if ((fR_74cae2 == 0)) goto L_74cafc;
    edx = rd32(m, ebx);
    edx = rd32(m, edx + 0x134u);
    esp -= 4; wr32(m, esp, 0x1u);
    ecx = (esi + 0x110u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, eax);
    ecx = ebx;
    LIFT_CALL(edx, 0x74cafcu);
L_74cafc:
    eax = rd32(m, ebx);
    edx = rd32(m, eax + 0xe8u);
    esp -= 4; wr32(m, esp, 0x94fc70u);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(edx, 0x74cb0eu);
    fR_74cb0e = (eax & eax);
    if ((fR_74cb0e == 0)) goto L_74cb20;
    esp -= 4; wr32(m, esp, eax);
    edx = (esi + 0xd0u);
    ecx = ebx;
    LIFT_CALL(0x74c810u, 0x74cb20u);  // FUN_0074c810
L_74cb20:
    eax = rd32(m, ebx);
    edx = rd32(m, eax + 0xe8u);
    esp -= 4; wr32(m, esp, 0x96c3a0u);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(edx, 0x74cb32u);
    fR_74cb32 = (eax & eax);
    if ((fR_74cb32 == 0)) goto L_74cd12;
    edx = rd32(m, ebx);
    esp -= 4; wr32(m, esp, 0x1u);
    esi = esi + 0x114u;
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, edx + 0x130u);
    ecx = ebx;
    LIFT_CALL(eax, 0x74cb50u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = 0x1u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(12);
L_74cb5c:
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0xe8u);
    esp -= 4; wr32(m, esp, 0x94fc70u);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(eax, 0x74cb6eu);
    goto L_74c93e;
L_74cb73:
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0xe8u);
    esp -= 4; wr32(m, esp, 0x96c3d0u);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(eax, 0x74cb85u);
    goto L_74c896;
L_74cb8a:
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0xe8u);
    esp -= 4; wr32(m, esp, 0x96c3d0u);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(eax, 0x74cb9cu);
    fR_74cb9c = (eax & eax);
    if ((fR_74cb9c == 0)) goto L_74cbac;
    edi = (esi + 0x8u);
    edx = eax;
    ecx = ebx;
    LIFT_CALL(0x74c7e0u, 0x74cbacu);  // FUN_0074c7e0
L_74cbac:
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0xe8u);
    esp -= 4; wr32(m, esp, 0x96c3c4u);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(eax, 0x74cbbeu);
    fR_74cbbe = (eax & eax);
    if ((fR_74cbbe == 0)) goto L_74cbd1;
    edi = (esi + 0x88u);
    edx = eax;
    ecx = ebx;
    LIFT_CALL(0x74c7e0u, 0x74cbd1u);  // FUN_0074c7e0
L_74cbd1:
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0xe8u);
    esp -= 4; wr32(m, esp, 0x96c3dcu);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(eax, 0x74cbe3u);
    fR_74cbe3 = (eax & eax);
    if ((fR_74cbe3 == 0)) goto L_74cd12;
    edi = (esi + 0x48u);
    goto L_74c8a1;
L_74cbf3:
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0xe8u);
    esp -= 4; wr32(m, esp, 0x96c3d0u);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(eax, 0x74cc05u);
    fR_74cc05 = (eax & eax);
    if ((fR_74cc05 == 0)) goto L_74cc15;
    edi = (esi + 0x8u);
    edx = eax;
    ecx = ebx;
    LIFT_CALL(0x74c7e0u, 0x74cc15u);  // FUN_0074c7e0
L_74cc15:
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0xe8u);
    esp -= 4; wr32(m, esp, 0x96c3c4u);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(eax, 0x74cc27u);
    fR_74cc27 = (eax & eax);
    if ((fR_74cc27 == 0)) goto L_74cd12;
    edi = (esi + 0x88u);
    goto L_74c8a1;
L_74cc3a:
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0xe8u);
    esp -= 4; wr32(m, esp, 0x96c37cu);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(eax, 0x74cc4cu);
    goto L_74c896;
L_74cc51:
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0xe8u);
    esp -= 4; wr32(m, esp, 0x96c3d0u);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(eax, 0x74cc63u);
    fR_74cc63 = (eax & eax);
    if ((fR_74cc63 == 0)) goto L_74cc8e;
    edx = rd32(m, ebx);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, edx + 0x110u);
    ecx = ebx;
    LIFT_CALL(eax, 0x74cc74u);
    esp -= 4; wr32(m, esp, eax);
    edi = (esi + 0x8u);
    esp -= 4; wr32(m, esp, 0x9419e4u);
    esi = 0x40u;
    LIFT_CALL(0x78fd60u, 0x74cc87u);  // FUN_0078fd60
    esi = rd32(m, esp + 0x1cu);
    esp = esp + 0x8u;
L_74cc8e:
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0xe8u);
    esp -= 4; wr32(m, esp, 0x96c38cu);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    LIFT_CALL(eax, 0x74cca0u);
    fR_74cca0 = (eax & eax);
    if ((fR_74cca0 == 0)) goto L_74cccb;
    edx = rd32(m, ebx);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, edx + 0x110u);
    ecx = ebx;
    LIFT_CALL(eax, 0x74ccb1u);
    esp -= 4; wr32(m, esp, eax);
    edi = (esi + 0x48u);
    esp -= 4; wr32(m, esp, 0x9419e4u);
    esi = 0x40u;
    LIFT_CALL(0x78fd60u, 0x74ccc4u);  // FUN_0078fd60
    esi = rd32(m, esp + 0x1cu);
    esp = esp + 0x8u;
L_74cccb:
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0xe8u);
    esp -= 4; wr32(m, esp, 0x96c398u);
    esp -= 4; wr32(m, esp, ebp);
    ecx = ebx;
    wr32(m, esp + 0x1cu, 0x1u);
    LIFT_CALL(eax, 0x74cce5u);
    fR_74cce5 = (eax & eax);
    if ((fR_74cce5 == 0)) goto L_74ccfd;
    edx = rd32(m, ebx);
    edx = rd32(m, edx + 0x134u);
    esp -= 4; wr32(m, esp, 0x1u);
    ecx = (esp + 0x18u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, eax);
    ecx = ebx;
    LIFT_CALL(edx, 0x74ccfdu);
L_74ccfd:
    eax = 0u;
    fA_74ccff = rd32(m, esp + 0x14u);
    fB_74ccff = eax;
    eax = (eax & 0xffffff00u) | (((int32_t(sext(fA_74ccff, 4)) > int32_t(sext(fB_74ccff, 4))) ? 1u : 0u) & 0xffu);
    x0 = cvtsi2ss(x0, eax);
    wr32(m, esi + 0x114u, x0.l(0));
L_74cd12:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = 0x1u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(12);
}

// 0074cd80 Fn_0074cd80
//   original FUN_0074cd80; 18 instructions per frame (plan); register inputs: esi; returns with ret 12
REFLEX_PORT_FN(0x0074cd80, Fn_0074cd80, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_74cd9b = 0;
    uint32_t fR_74cdc8 = 0;
    uint32_t fA_74cdcd = 0;
    uint32_t fB_74cdcd = 0;
    uint32_t fR_74cdf3 = 0;
    uint32_t fR_74ce0a = 0;
    FCmp fC_74ce1e{};
    ecx = rd32(m, esp + 0xcu);
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0xe8u);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, 0x93eae0u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = esi;
    LIFT_CALL(edx, 0x74cd9bu);
    fR_74cd9b = (eax & eax);
    if ((fR_74cd9b == 0)) goto L_74ce35;
    edx = rd32(m, esi);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, edx + 0x110u);
    ecx = esi;
    LIFT_CALL(eax, 0x74cdb2u);
    ebx = eax;
    edi = 0u;
L_74cdb6:
    ecx = rd32(m, edi * 4 + 0x9b7e20u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(rd32(m, 0x9162bcu) /* 8.287018476502604e-39 */, 0x74cdc5u);
    esp = esp + 0x8u;
    fR_74cdc8 = (eax & eax);
    if ((fR_74cdc8 == 0)) goto L_74cdda;
    edi = (edi + 1u);
    fA_74cdcd = edi;
    fB_74cdcd = 0x18u;
    if (int32_t(sext(fA_74cdcd, 4)) < int32_t(sext(fB_74cdcd, 4))) goto L_74cdb6;
L_74cdd2:
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    eax = 0u;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(12);
L_74cdda:
    ebx = rd32(m, esp + 0x18u);
    wr32(m, ebp, edi);
    edx = rd32(m, esi);
    eax = rd32(m, edx + 0xe8u);
    esp -= 4; wr32(m, esp, 0x94fb00u);
    esp -= 4; wr32(m, esp, ebx);
    ecx = esi;
    LIFT_CALL(eax, 0x74cdf3u);
    fR_74cdf3 = (eax & eax);
    if ((fR_74cdf3 == 0)) goto L_74cdd2;
    edx = rd32(m, esi);
    esp -= 4; wr32(m, esp, 0x1u);
    edi = (ebp + 0x4u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, edx + 0x130u);
    ecx = esi;
    LIFT_CALL(eax, 0x74ce0au);
    fR_74ce0a = (eax & eax);
    if ((fR_74ce0a == 0)) goto L_74cdd2;
    ecx = rd32(m, esp + 0x10u);
    x0 = movss_m(rd32(m, ecx + 0x15184u));
    x1 = movss_m(rd32(m, edi));
    fC_74ce1e = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_74ce1e) || fc_z(fC_74ce1e))) goto L_74ce27;
    wr32(m, edi, x0.l(0));
L_74ce27:
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, ebp);
    LIFT_CALL(0x74c830u, 0x74ce2fu);  // FUN_0074c830
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(12);
L_74ce35:
    eax = 0u;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(12);
}

// 0074ce40 Fn_0074ce40
//   original FUN_0074ce40; 3 instructions per frame (plan); register inputs: eax edi; returns with ret 0
REFLEX_PORT_FN(0x0074ce40, Fn_0074ce40, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_74ce48 = 0;
    uint32_t fB_74ce48 = 0;
    uint32_t fR_74ce76 = 0;
    uint32_t fR_74ce90 = 0;
    uint32_t fR_74cea7 = 0;
    uint32_t fR_74cebf = 0;
    uint32_t fR_74ced7 = 0;
    uint32_t fR_74cf01 = 0;
    uint32_t fR_74cf1b = 0;
    uint32_t fA_74cf27 = 0;
    uint32_t fB_74cf27 = 0;
    uint32_t fR_74cf41 = 0;
    uint32_t fA_74cf57 = 0;
    uint32_t fB_74cf57 = 0;
    esp = esp - 0xcu;
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    eax = 0u;
    fA_74ce48 = esi;
    fB_74ce48 = eax;
    if (fA_74ce48 != fB_74ce48) goto L_74ce51;
    esi = rd32(m, esp); esp += 4;
    esp = esp + 0xcu;
    LIFT_RET(0);
L_74ce51:
    x0 = X{};
    wr32(m, esp + 0xcu, eax);
    wr32(m, edi + 0x15180u, eax);
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x4u);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    ecx = esi;
    wr32(m, esp + 0xcu, x0.l(0));
    ebp = 0x1u;
    LIFT_CALL(edx, 0x74ce74u);
    ebx = eax;
    fR_74ce76 = (ebx & ebx);
    if ((fR_74ce76 == 0)) goto L_74cf55;
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0xe8u);
    esp -= 4; wr32(m, esp, 0x96c3e8u);
    esp -= 4; wr32(m, esp, ebx);
    ecx = esi;
    LIFT_CALL(edx, 0x74ce90u);
    fR_74ce90 = (eax & eax);
    if ((fR_74ce90 == 0)) goto L_74cead;
    edx = rd32(m, esi);
    edx = rd32(m, edx + 0x134u);
    esp -= 4; wr32(m, esp, ebp);
    ecx = (esp + 0x18u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, eax);
    ecx = esi;
    LIFT_CALL(edx, 0x74cea7u);
    fR_74cea7 = (eax & eax);
    if (!(fR_74cea7 == 0)) goto L_74cead;
    ebp = 0u;
L_74cead:
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0xe8u);
    esp -= 4; wr32(m, esp, 0x93dd58u);
    esp -= 4; wr32(m, esp, ebx);
    ecx = esi;
    LIFT_CALL(edx, 0x74cebfu);
    fR_74cebf = (eax & eax);
    if ((fR_74cebf == 0)) goto L_74ceed;
    edx = rd32(m, esi);
    edx = rd32(m, edx + 0x130u);
    esp -= 4; wr32(m, esp, 0x1u);
    ecx = (esp + 0x10u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, eax);
    ecx = esi;
    LIFT_CALL(edx, 0x74ced7u);
    fR_74ced7 = (eax & eax);
    if ((fR_74ced7 == 0)) goto L_74ceeb;
    x0 = movss_m(rd32(m, esp + 0xcu));
    wr32(m, edi + 0x15184u, x0.l(0));
    goto L_74ceed;
L_74ceeb:
    ebp = 0u;
L_74ceed:
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x20u);
    ecx = (esp + 0x10u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, 0x946220u);
    esp -= 4; wr32(m, esp, ebx);
    ecx = esi;
    LIFT_CALL(edx, 0x74cf01u);
    fR_74cf01 = (eax & eax);
    if ((fR_74cf01 == 0)) goto L_74cf45;
L_74cf05:
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, edi + 0x15180u);
    eax = (eax + eax * 8);
    eax = (eax << 5);
    eax = eax + edi;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x74cd80u, 0x74cf1bu);  // FUN_0074cd80
    fR_74cf1b = (eax & eax);
    if (!(fR_74cf1b == 0)) goto L_74cf21;
    ebp = 0u;
L_74cf21:
    wr32(m, edi + 0x15180u, (rd32(m, edi + 0x15180u) + 1u));
    fA_74cf27 = rd32(m, edi + 0x15180u);
    fB_74cf27 = 0x12cu;
    if (int32_t(sext(fA_74cf27, 4)) >= int32_t(sext(fB_74cf27, 4))) goto L_74cf45;
    edx = rd32(m, esi);
    eax = rd32(m, esp + 0x10u);
    edx = rd32(m, edx + 0x28u);
    esp -= 4; wr32(m, esp, eax);
    ecx = esi;
    LIFT_CALL(edx, 0x74cf41u);
    fR_74cf41 = (eax & eax);
    if (!(fR_74cf41 == 0)) goto L_74cf05;
L_74cf45:
    ecx = rd32(m, esp + 0x10u);
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x24u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = esi;
    LIFT_CALL(edx, 0x74cf53u);
    goto L_74cf57;
L_74cf55:
    ebp = 0u;
L_74cf57:
    fA_74cf57 = rd32(m, edi + 0x15180u);
    fB_74cf57 = 0x0u;
    if (fA_74cf57 != fB_74cf57) goto L_74cf62;
    ebp = 0u;
L_74cf62:
    eax = ebp;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    esp = esp + 0xcu;
    LIFT_RET(0);
}

// 0074cf70 Fn_0074cf70
//   original FUN_0074cf70; 216 instructions per frame (plan); register inputs: esi; returns with ret 8
REFLEX_PORT_FN(0x0074cf70, Fn_0074cf70, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    FCmp fC_74cf78{};
    uint32_t fR_74cf87 = 0;
    uint32_t fA_74cf8e = 0;
    uint32_t fB_74cf8e = 0;
    FCmp fC_74cfa5{};
    uint32_t fR_74cfad = 0;
    uint32_t fR_74cfbb = 0;
    uint32_t fA_74cfcd = 0;
    uint32_t fB_74cfcd = 0;
    FCmp fC_74cfe5{};
    uint32_t fA_74cfec = 0;
    uint32_t fB_74cfec = 0;
    FCmp fC_74d015{};
    FCmp fC_74d024{};
    uint32_t fR_74d032 = 0;
    uint32_t fA_74d049 = 0;
    uint32_t fB_74d049 = 0;
    x0 = movss_m(rd32(m, esi + 0x15188u));
    fC_74cf78 = fcmp32(x0.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    esp -= 4; wr32(m, esp, ebx);
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_74cf78) | 2u) << 8);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, edi);
    fR_74cf87 = (((eax >> 8) & 0xffu) & 0x44u);
    if (parity8(fR_74cf87)) goto L_74cfd5;
    ebx = 0u;
    fA_74cf8e = rd32(m, esi + 0x15180u);
    fB_74cf8e = ebx;
    if (int32_t(sext(fA_74cf8e, 4)) <= int32_t(sext(fB_74cf8e, 4))) goto L_74cfd5;
    edi = esi;
    goto L_74cfa0;
L_74cfa0:
    x0 = movss_m(rd32(m, edi + 0x4u));
    fC_74cfa5 = fcmp32(x0.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_74cfa5) | 2u) << 8);
    fR_74cfad = (((eax >> 8) & 0xffu) & 0x44u);
    if (parity8(fR_74cfad)) goto L_74cfc6;
    eax = rd32(m, edi);
    eax = rd32(m, eax * 4 + 0x9c5120u);
    fR_74cfbb = (eax & eax);
    if ((fR_74cfbb == 0)) goto L_74cfc6;
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, edi);
    ecx = ebp;
    LIFT_CALL(eax, 0x74cfc6u);
L_74cfc6:
    ebx = (ebx + 1u);
    edi = edi + 0x120u;
    fA_74cfcd = ebx;
    fB_74cfcd = rd32(m, esi + 0x15180u);
    if (int32_t(sext(fA_74cfcd, 4)) < int32_t(sext(fB_74cfcd, 4))) goto L_74cfa0;
L_74cfd5:
    x0 = movss_m(rd32(m, esi + 0x15188u));
    x1 = movss_m(rd32(m, esi + 0x15184u));
    fC_74cfe5 = fcmp32(x1.l(0), x0.l(0));
    if (fc_b(fC_74cfe5)) goto L_74d051;
    ebx = 0u;
    fA_74cfec = rd32(m, esi + 0x15180u);
    fB_74cfec = ebx;
    wr32(m, esp + 0x14u, x0.l(0));
    x0 = addss(x0, rd32(m, esp + 0x10u));
    wr32(m, esi + 0x15188u, x0.l(0));
    if (int32_t(sext(fA_74cfec, 4)) <= int32_t(sext(fB_74cfec, 4))) goto L_74d051;
    edi = esi;
L_74d010:
    x0 = movss_m(rd32(m, edi + 0x4u));
    fC_74d015 = fcmp32(x0.l(0), rd32(m, esp + 0x14u));
    if ((fc_b(fC_74d015) || fc_z(fC_74d015))) goto L_74d042;
    x1 = movss_m(rd32(m, esi + 0x15188u));
    fC_74d024 = fcmp32(x1.l(0), x0.l(0));
    if (fc_b(fC_74d024)) goto L_74d042;
    ecx = rd32(m, edi);
    eax = rd32(m, ecx * 4 + 0x9c5120u);
    fR_74d032 = (eax & eax);
    if ((fR_74d032 == 0)) goto L_74d042;
    edx = rd32(m, esi + 0x1518cu);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, edi);
    ecx = ebp;
    LIFT_CALL(eax, 0x74d042u);
L_74d042:
    ebx = (ebx + 1u);
    edi = edi + 0x120u;
    fA_74d049 = ebx;
    fB_74d049 = rd32(m, esi + 0x15180u);
    if (int32_t(sext(fA_74d049, 4)) < int32_t(sext(fB_74d049, 4))) goto L_74d010;
L_74d051:
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 0074d080 FreerideIntroScenario__vf3
//   original FreerideIntroScenario::vf3; 0 instructions per frame (plan); register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x0074d080, FreerideIntroScenario__vf3, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_74d090 = 0;
    uint32_t fB_74d090 = 0;
    uint32_t fA_74d0b5 = 0;
    uint32_t fB_74d0b5 = 0;
    uint32_t fA_74d0e0 = 0;
    uint32_t fB_74d0e0 = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = ecx;
    eax = rd32(m, ebx + 0x151b4u);
    eax = (eax - 1u);
    esp -= 4; wr32(m, esp, ebp);
    ebp = 0x1u;
    fA_74d090 = eax;
    fB_74d090 = 0xcu;
    if (fA_74d090 > fB_74d090) goto L_74d149;
    eax = rd8(m, eax + 0x74d15cu);
    switch (eax) { case 0: goto L_74d0a7; case 1: goto L_74d13f; case 2: goto L_74d149; default: throw std::runtime_error("jump table"); }
L_74d0a7:
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, ebx + 0x14u);
    esi = 0u;
    edx = edi;
    LIFT_CALL(0x559df0u, 0x74d0b5u);  // FUN_00559df0
    fA_74d0b5 = eax;
    fB_74d0b5 = 0x1u;
    if (int32_t(sext(fA_74d0b5, 4)) <= int32_t(sext(fB_74d0b5, 4))) goto L_74d0c6;
    wr32(m, ebx + 0x151b4u, 0xdu);
    goto L_74d0ef;
L_74d0c6:
    esp -= 4; wr32(m, esp, 0x0u);
    eax = edi;
    LIFT_CALL(0x559e60u, 0x74d0cfu);  // FUN_00559e60
    ecx = rd32(m, eax + 0x74u);
    edx = rd32(m, ecx + 0x1acu);
    eax = rd32(m, edx + 0x194u);
    ecx = ebp;
    fA_74d0e0 = ecx;
    fB_74d0e0 = eax;
    edx = edx - edx - (fA_74d0e0 < fB_74d0e0 ? 1u : 0u);
    edx = (0u - edx);
    edx = edx + 0x4u;
    wr32(m, ebx + 0x151b4u, edx);
L_74d0ef:
    esp -= 4; wr32(m, esp, 0x0u);
    eax = edi;
    LIFT_CALL(0x559e60u, 0x74d0f8u);  // FUN_00559e60
    eax = rd32(m, eax + 0x74u);
    ecx = rd32(m, eax + 0x1acu);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x38u);
    LIFT_CALL(eax, 0x74d108u);
    ecx = rd32(m, ebx + 0x14u);
    edi = rd32(m, ecx + 0x234u);
    edi = edi + 0x20150u;
    esi = eax;
    ecx = 0x10u;
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    edx = rd32(m, ebx + 0x14u);
    edi = rd32(m, edx + 0x234u);
    edi = edi + 0x21d50u;
    esi = eax;
    ecx = 0x10u;
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    eax = ebp;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_74d13f:
    wr32(m, ebx + 0x151b4u, 0x0u);
L_74d149:
    ebp = rd32(m, esp); esp += 4;
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0074d2b0 FreerideIntroScenario__vf1
//   original FreerideIntroScenario::vf1; 0 instructions per frame (plan); register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x0074d2b0, FreerideIntroScenario__vf1, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_74d2ca = 0;
    uint32_t fB_74d2ca = 0;
    uint32_t fA_74d2d3 = 0;
    uint32_t fB_74d2d3 = 0;
    uint32_t fA_74d2d8 = 0;
    uint32_t fB_74d2d8 = 0;
    uint32_t fR_74d399 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x54u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, ebp + 0x8u);
    ebx = ecx;
    eax = edi;
    esi = ebx;
    LIFT_CALL(0x74d4c0u, 0x74d2cau);  // Rider_SetStartLineAnimation
    fA_74d2ca = edi;
    fB_74d2ca = 0x4u;
    if (int32_t(sext(fA_74d2ca, 4)) < int32_t(sext(fB_74d2ca, 4))) goto L_74d428;
    fA_74d2d3 = edi;
    fB_74d2d3 = 0x5u;
    if (int32_t(sext(fA_74d2d3, 4)) <= int32_t(sext(fB_74d2d3, 4))) goto L_74d333;
    fA_74d2d8 = edi;
    fB_74d2d8 = 0xdu;
    if (fA_74d2d8 != fB_74d2d8) goto L_74d428;
    esi = rd32(m, ebx + 0x14u);
    esp -= 4; wr32(m, esp, 0x0u);
    eax = esi;
    LIFT_CALL(0x559e60u, 0x74d2edu);  // FUN_00559e60
    eax = rd32(m, eax + 0x74u);
    edi = rd32(m, esi + 0x234u);
    esp -= 4; wr32(m, esp, eax);
    edi = edi + 0x20150u;
    LIFT_CALL(0x74b080u, 0x74d302u);  // FUN_0074b080
    ecx = rd32(m, eax);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x38u);
    LIFT_CALL(eax, 0x74d30bu);
    ecx = rd32(m, ebx + 0x14u);
    edi = rd32(m, ecx + 0x234u);
    edi = edi + 0x20150u;
    ecx = 0x10u;
    esi = eax;
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    edx = rd32(m, ebx + 0x14u);
    edi = rd32(m, edx + 0x234u);
    esi = eax;
    goto L_74d41b;
L_74d333:
    eax = rd32(m, ebx + 0x151c0u);
    esi = rd32(m, ebx + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    eax = esi;
    LIFT_CALL(0x559e60u, 0x74d344u);  // FUN_00559e60
    ecx = rd32(m, eax + 0x74u);
    edi = rd32(m, esi + 0x234u);
    esp -= 4; wr32(m, esp, ecx);
    edi = edi + 0x20150u;
    LIFT_CALL(0x74b080u, 0x74d359u);  // FUN_0074b080
    ecx = rd32(m, eax);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x38u);
    wr32(m, esp + 0x18u, ecx);
    LIFT_CALL(eax, 0x74d366u);
    ecx = rd32(m, ebx + 0x14u);
    edi = rd32(m, ecx + 0x234u);
    esi = eax;
    edi = edi + 0x20150u;
    ecx = 0x10u;
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    ecx = rd32(m, esp + 0x18u);
    edx = rd32(m, ecx);
    wr32(m, esp + 0x1cu, eax);
    eax = rd32(m, edx + 0x70u);
    LIFT_CALL(eax, 0x74d38du);
    esi = eax;
    esp -= 4; wr32(m, esp, 0x9581fcu);
    LIFT_CALL(0x7de980u, 0x74d399u);  // FUN_007de980
    fR_74d399 = (eax & eax);
    if ((fR_74d399 == 0)) goto L_74d40e;
    esp -= 4; wr32(m, esp, 0x9581fcu);
    edi = esi;
    LIFT_CALL(0x7de9e0u, 0x74d3a9u);  // FUN_007de9e0
    ecx = rd32(m, esp + 0x18u);
    edx = rd32(m, ecx);
    wr32(m, esp + 0x1cu, eax);
    eax = rd32(m, edx + 0x38u);
    LIFT_CALL(eax, 0x74d3b8u);
    esi = eax;
    ecx = 0x10u;
    edi = (esp + 0x20u);
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    esi = rd32(m, esp + 0x1cu);
    esp -= 4; wr32(m, esp, 0x0u);
    ecx = esi;
    LIFT_CALL(0x7db890u, 0x74d3d2u);  // Node_EvaluateWorld
    eax = rd8(m, esi + 0xbu);
    ecx = rd32(m, esi);
    edx = rd32(m, ebx + 0x14u);
    edi = rd32(m, edx + 0x234u);
    eax = (eax << 6);
    eax = eax + rd32(m, ecx + 0x1cu);
    esi = (esp + 0x20u);
    x0 = movss_m(rd32(m, eax + 0x30u));
    wr32(m, esp + 0x50u, x0.l(0));
    x0 = movss_m(rd32(m, eax + 0x34u));
    wr32(m, esp + 0x54u, x0.l(0));
    x0 = movss_m(rd32(m, eax + 0x38u));
    wr32(m, esp + 0x58u, x0.l(0));
    goto L_74d41b;
L_74d40e:
    eax = rd32(m, ebx + 0x14u);
    edi = rd32(m, eax + 0x234u);
    esi = rd32(m, esp + 0x1cu);
L_74d41b:
    ecx = 0x10u;
    edi = edi + 0x21d50u;
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
L_74d428:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 0074d460 FreerideIntroScenario__vf6
//   original FreerideIntroScenario::vf6; 0 instructions per frame (plan); register inputs: ecx ebx; returns with ret 0
REFLEX_PORT_FN(0x0074d460, FreerideIntroScenario__vf6, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_74d485 = 0;
    uint32_t fA_74d4a6 = 0;
    uint32_t fB_74d4a6 = 0;
    esp -= 4; wr32(m, esp, edi);
    edi = ecx;
    LIFT_CALL(0x47c3f0u, 0x74d468u);  // FUN_0047c3f0
    eax = rd32(m, edi + 0x14u);
    ecx = rd32(m, eax + 0x234u);
    edx = rd32(m, ecx + 0x20190u);
    edx = rd32(m, edx + 0x26cu);
    ecx = ecx + 0x20150u;
    eax = 0u;
    fR_74d485 = (edx & edx);
    if (((fR_74d485 == 0) || sgn(fR_74d485, 4) != false)) goto L_74d4b2;
    esp -= 4; wr32(m, esp, esi);
L_74d490:
    esi = rd32(m, ecx + 0x40u);
    esi = rd32(m, esi + 0x268u);
    esi = rd32(m, esi + eax * 4);
    esi = rd32(m, esi + 0x74u);
    esi = rd32(m, esi + 0x1b4u);
    eax = (eax + 1u);
    fA_74d4a6 = eax;
    fB_74d4a6 = edx;
    wr8(m, esi + 0xa2u, 0x1u);
    if (int32_t(sext(fA_74d4a6, 4)) < int32_t(sext(fB_74d4a6, 4))) goto L_74d490;
    esi = rd32(m, esp); esp += 4;
L_74d4b2:
    edx = 0u;
    LIFT_CALL(0x749590u, 0x74d4b9u);  // FUN_00749590
    edi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0074d4c0 Rider_SetStartLineAnimation
//   original Rider_SetStartLineAnimation; 0 instructions per frame (plan); register inputs: ecx eax esi; returns with ret 0
REFLEX_PORT_FN(0x0074d4c0, Rider_SetStartLineAnimation, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_74d4c5 = 0;
    uint32_t fB_74d4c5 = 0;
    uint32_t fA_74d4eb = 0;
    uint32_t fB_74d4eb = 0;
    uint32_t fA_74d533 = 0;
    uint32_t fB_74d533 = 0;
    uint32_t fA_74d54e = 0;
    uint32_t fB_74d54e = 0;
    uint32_t fA_74d553 = 0;
    uint32_t fB_74d553 = 0;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebx);
    eax = (eax - 1u);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, edi);
    fA_74d4c5 = eax;
    fB_74d4c5 = 0xcu;
    if (fA_74d4c5 > fB_74d4c5) goto L_74d669;
    eax = rd8(m, eax + 0x74d680u);
    switch (eax) { case 0: goto L_74d4dc; case 1: goto L_74d51e; case 2: goto L_74d5cb; case 3: goto L_74d669; default: throw std::runtime_error("jump table"); }
L_74d4dc:
    ebx = rd32(m, 0xa946b8u);
    ebp = 0u;
L_74d4e4:
    ecx = rd8(m, ebx + 0x1541cu);
    fA_74d4eb = ebp;
    fB_74d4eb = ecx;
    if (int32_t(sext(fA_74d4eb, 4)) >= int32_t(sext(fB_74d4eb, 4))) goto L_74d51e;
    esp -= 4; wr32(m, esp, 0x3u);
    esp -= 4; wr32(m, esp, ebp);
    edx = (ebx + 0x7460u);
    LIFT_CALL(0x552b90u, 0x74d4fdu);  // FUN_00552b90
    edx = rd32(m, esi + 0x14u);
    edi = rd32(m, edx + 0x234u);
    esp -= 4; wr32(m, esp, eax);
    edi = edi + 0x20150u;
    LIFT_CALL(0x74b080u, 0x74d512u);  // FUN_0074b080
    eax = rd32(m, eax);
    wr8(m, eax + 0xa2u, 0x0u);
    ebp = (ebp + 1u);
    goto L_74d4e4;
L_74d51e:
    ebx = rd32(m, esi + 0x14u);
    ebp = 0u;
    edi = (ebp + 0x1u);
    edx = ebx;
    LIFT_CALL(0x559e20u, 0x74d52du);  // FUN_00559e20
    ecx = rd32(m, esi + 0x151c0u);
    fA_74d533 = ecx;
    fB_74d533 = eax;
    if (int32_t(sext(fA_74d533, 4)) >= int32_t(sext(fB_74d533, 4))) goto L_74d542;
    esp -= 4; wr32(m, esp, ecx);
    eax = ebx;
    LIFT_CALL(0x559e60u, 0x74d53fu);  // FUN_00559e60
    ebp = rd32(m, eax + 0x74u);
L_74d542:
    ecx = rd32(m, ebp + 0x1acu);
    eax = rd32(m, ecx + 0x194u);
    fA_74d54e = eax;
    fB_74d54e = 0x0u;
    eax = fA_74d54e - fB_74d54e;
    if (fA_74d54e == fB_74d54e) goto L_74d56c;
    fA_74d553 = eax;
    fB_74d553 = 0x1u;
    eax = fA_74d553 - fB_74d553;
    if (fA_74d553 == fB_74d553) goto L_74d562;
    wr32(m, esp + 0xcu, 0x96ccc4u);
    goto L_74d574;
L_74d562:
    wr32(m, esp + 0xcu, 0x96cbe4u);
    goto L_74d574;
L_74d56c:
    wr32(m, esp + 0xcu, 0x96cbd0u);
L_74d574:
    edi = rd32(m, ebx + 0x234u);
    esp -= 4; wr32(m, esp, ebp);
    edi = edi + 0x20150u;
    LIFT_CALL(0x74b080u, 0x74d586u);  // FUN_0074b080
    edi = rd32(m, eax);
    edx = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, 0x0u);
    wr8(m, edi + 0xa2u, 0x1u);
    eax = rd32(m, esi + 0x14u);
    ecx = rd32(m, eax + 0x234u);
    esp -= 4; wr32(m, esp, edx);
    edx = rd32(m, esi + 0x151c4u);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, edi);
    ecx = ecx + 0x20150u;
    LIFT_CALL(0x74be10u, 0x74d5b3u);  // FUN_0074be10
    fpush(m, reflex_f80::from_f32(rd32(m, 0x9322acu) /* 0.009999999776482582 */));
    edx = rd32(m, edi);
    eax = rd32(m, edx + 0x4cu);
    esp -= 4; wr32(m, esp, ecx);
    ecx = edi;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(eax, 0x74d5c6u);
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_74d5cb:
    edi = rd32(m, esi + 0x14u);
    esp -= 4; wr32(m, esp, 0x0u);
    eax = edi;
    LIFT_CALL(0x559e60u, 0x74d5d7u);  // FUN_00559e60
    edx = rd32(m, eax + 0x74u);
    eax = edx;
    LIFT_CALL(0x74d690u, 0x74d5e1u);  // FUN_0074d690
    edi = rd32(m, edi + 0x234u);
    esp -= 4; wr32(m, esp, edx);
    edi = edi + 0x20150u;
    ebp = eax;
    LIFT_CALL(0x74b080u, 0x74d5f5u);  // FUN_0074b080
    eax = rd32(m, eax);
    esp -= 4; wr32(m, esp, 0x0u);
    wr8(m, eax + 0xa2u, 0x1u);
    ecx = rd32(m, esi + 0x14u);
    ecx = rd32(m, ecx + 0x234u);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, eax);
    ecx = ecx + 0x20150u;
    edx = 0u;
    LIFT_CALL(0x74be10u, 0x74d61au);  // FUN_0074be10
    edi = rd32(m, esi + 0x14u);
    esp -= 4; wr32(m, esp, 0x1u);
    eax = edi;
    LIFT_CALL(0x559e60u, 0x74d626u);  // FUN_00559e60
    edx = rd32(m, eax + 0x74u);
    eax = edx;
    LIFT_CALL(0x74d690u, 0x74d630u);  // FUN_0074d690
    edi = rd32(m, edi + 0x234u);
    esp -= 4; wr32(m, esp, edx);
    edi = edi + 0x20150u;
    ebp = eax;
    LIFT_CALL(0x74b080u, 0x74d644u);  // FUN_0074b080
    eax = rd32(m, eax);
    esp -= 4; wr32(m, esp, 0x0u);
    wr8(m, eax + 0xa2u, 0x1u);
    edx = rd32(m, esi + 0x14u);
    ecx = rd32(m, edx + 0x234u);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, eax);
    ecx = ecx + 0x20150u;
    edx = 0u;
    LIFT_CALL(0x74be10u, 0x74d669u);  // FUN_0074be10
L_74d669:
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0074d6c0 Fn_0074d6c0
//   original FUN_0074d6c0; 0 instructions per frame (plan); register inputs: eax; returns with ret 0
REFLEX_PORT_FN(0x0074d6c0, Fn_0074d6c0, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_74d6d9 = 0;
    uint32_t fB_74d6d9 = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, eax + 0x151c0u);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, eax + 0x14u);
    esp -= 4; wr32(m, esp, edi);
    ebp = 0u;
    edi = (ebp + 0x1u);
    edx = esi;
    LIFT_CALL(0x559e20u, 0x74d6d9u);  // FUN_00559e20
    fA_74d6d9 = ebx;
    fB_74d6d9 = eax;
    if (int32_t(sext(fA_74d6d9, 4)) >= int32_t(sext(fB_74d6d9, 4))) goto L_74d6ed;
    esp -= 4; wr32(m, esp, ebx);
    eax = esi;
    LIFT_CALL(0x559e60u, 0x74d6e5u);  // FUN_00559e60
    eax = rd32(m, eax + 0x74u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_74d6ed:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    eax = ebp;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}
