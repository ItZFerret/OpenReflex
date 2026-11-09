// Engine services outside the core ranges (0x004cxxxx..0x004fxxxx, 0x0066xxxx, 0x006e..0x007bxxxx, 0x007exxxx, 0x008bxxxx..).
// Group "engine". Transliterated instruction by instruction from MXReflex.exe by the bike lifter (engine copy, see
// native/game/notes/engine.md): the original's operations, order and memory traffic, stack frame included.
#include <stdexcept>
#include <utility>
#include "lift_rt.h"

// 004db070 Fn_004db070
//   original FUN_004db070; register inputs: ecx eax esi; returns with ret 0
REFLEX_PORT_FN(0x004db070, Fn_004db070, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    FCmp fC_4db098{};
    FCmp fC_4db0e5{};
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x10u;
    fpush(m, reflex_f80::from_f32(rd32(m, eax)));
    wr32(m, esi + 0x80u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x4u)));
    wr32(m, esi + 0x84u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x8u)));
    wr32(m, esi + 0x88u, fst32(m));
    fpop(m);
    x0 = movss_m(rd32(m, esi + 0x50u));
    fC_4db098 = fcmp32(x0.l(0), rd32(m, 0x935aa8u) /* 0.75 */);
    wr32(m, esp + 0xcu, x0.l(0));
    if (fc_b(fC_4db098)) goto L_4db14e;
    x1 = movss_m(rd32(m, esi + 0x88u));
    x1 = subss(x1, rd32(m, esi + 0x78u));
    x0 = movss_m(rd32(m, esi + 0x80u));
    x0 = subss(x0, rd32(m, esi + 0x70u));
    x2 = x1;
    x2 = mulss(x2, x1.l(0));
    x1 = x0;
    x1 = mulss(x1, x0.l(0));
    esp -= 4; wr32(m, esp, ecx);
    x2 = addss(x2, x1.l(0));
    wr32(m, esp, x2.l(0));
    LIFT_CALL(0x42c230u, 0x4db0e2u);  // FUN_0042c230
    esp = esp + 0x4u;
    fC_4db0e5 = fcmp32(x0.l(0), rd32(m, 0x935aacu) /* 20.0 */);
    if ((fc_b(fC_4db0e5) || fc_z(fC_4db0e5))) goto L_4db14e;
    x0 = movss_m(rd32(m, esp + 0xcu));
    wr32(m, esi + 0x30u, x0.l(0));
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x70u)));
    wr32(m, esi, fst32(m));
    fpop(m);
    eax = 0x1u;
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x74u)));
    wr32(m, esi + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x78u)));
    wr32(m, esi + 0x8u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x80u)));
    wr32(m, esi + 0x10u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x84u)));
    wr32(m, esi + 0x14u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x88u)));
    wr32(m, esi + 0x18u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x90u)));
    wr32(m, esi + 0x20u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x94u)));
    wr32(m, esi + 0x24u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x98u)));
    wr32(m, esi + 0x28u, fst32(m));
    fpop(m);
    wr32(m, esi + 0x44u, rd32(m, esi + 0x44u) + eax);
    wr32(m, esi + 0xa0u, eax);
L_4db14e:
    x0 = X{};
    wr32(m, esi + 0x50u, x0.l(0));
    wr32(m, esi + 0x4cu, x0.l(0));
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 004db160 Fn_004db160
//   original FUN_004db160; register inputs: eax edi; returns with ret 4
REFLEX_PORT_FN(0x004db160, Fn_004db160, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4db193 = 0;
    uint32_t fB_4db193 = 0;
    uint32_t fR_4db199 = 0;
    uint32_t fA_4db1b1 = 0;
    uint32_t fB_4db1b1 = 0;
    uint32_t fR_4db1b7 = 0;
    FCmp fC_4db1de{};
    uint32_t fR_4db219 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x18u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    wr32(m, esi + 0xa0u, 0x0u);
    eax = rd32(m, edi);
    edx = rd32(m, eax + 0xacu);
    ecx = edi;
    LIFT_CALL(edx, 0x4db183u);
    ebx = eax;
    eax = rd32(m, edi);
    edx = rd32(m, eax + 0x44u);
    ecx = (esp + 0x10u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = edi;
    LIFT_CALL(edx, 0x4db193u);
    fA_4db193 = rd32(m, esi + 0x40u);
    fB_4db193 = 0x0u;
    if (fA_4db193 == fB_4db193) goto L_4db1b1;
    fR_4db199 = (ebx & ebx);
    if (!(fR_4db199 == 0)) goto L_4db263;
    x0 = X{};
    wr32(m, esi + 0x40u, ebx);
    wr32(m, esi + 0x50u, x0.l(0));
    goto L_4db25e;
L_4db1b1:
    fA_4db1b1 = rd32(m, esi + 0x48u);
    fB_4db1b1 = 0x0u;
    if (fA_4db1b1 == fB_4db1b1) goto L_4db20d;
    fR_4db1b7 = (ebx & ebx);
    if (!(fR_4db1b7 == 0)) goto L_4db1c9;
    eax = (esp + 0x10u);
    LIFT_CALL(0x4db070u, 0x4db1c4u);  // FUN_004db070
    goto L_4db263;
L_4db1c9:
    x0 = movss_m(rd32(m, esi + 0x50u));
    x0 = addss(x0, rd32(m, ebp + 0x8u));
    wr32(m, esi + 0x50u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x14u));
    fC_4db1de = fcmp32(x0.l(0), rd32(m, esi + 0x94u));
    if ((fc_b(fC_4db1de) || fc_z(fC_4db1de))) goto L_4db263;
    x1 = movss_m(rd32(m, esp + 0x10u));
    wr32(m, esi + 0x94u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x18u));
    wr32(m, esi + 0x90u, x1.l(0));
    wr32(m, esi + 0x98u, x0.l(0));
    goto L_4db263;
L_4db20d:
    eax = rd32(m, edi);
    edx = rd32(m, eax + 0xacu);
    ecx = edi;
    LIFT_CALL(edx, 0x4db219u);
    fR_4db219 = (eax & eax);
    if ((fR_4db219 == 0)) goto L_4db254;
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x60u)));
    x0 = X{};
    wr32(m, esi + 0x70u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x64u)));
    wr32(m, esi + 0x74u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x68u)));
    wr32(m, esi + 0x78u, fst32(m));
    fpop(m);
    wr32(m, esi + 0x50u, x0.l(0));
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x70u)));
    wr32(m, esi + 0x90u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x74u)));
    wr32(m, esi + 0x94u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esi + 0x78u)));
    wr32(m, esi + 0x98u, fst32(m));
    fpop(m);
    goto L_4db263;
L_4db254:
    x0 = movss_m(rd32(m, esi + 0x4cu));
    x0 = addss(x0, rd32(m, ebp + 0x8u));
L_4db25e:
    wr32(m, esi + 0x4cu, x0.l(0));
L_4db263:
    x0 = movss_m(rd32(m, esp + 0x10u));
    wr32(m, esi + 0x60u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x14u));
    wr32(m, esi + 0x64u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x18u));
    wr32(m, esi + 0x68u, x0.l(0));
    wr32(m, esi + 0x48u, ebx);
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 004db590 Fn_004db590
//   original FUN_004db590; register inputs: esi edi; returns with ret 0
REFLEX_PORT_FN(0x004db590, Fn_004db590, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4db59a = 0;
    uint32_t fB_4db59a = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, edi + 0x10u);
    wr32(m, esi, 0x0u);
    fA_4db59a = rd32(m, edi + 0xcu);
    fB_4db59a = ebx;
    if (fA_4db59a <= fB_4db59a) goto L_4db5a5;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4db5a5u);
L_4db5a5:
    eax = rd32(m, edi);
    wr32(m, esi, eax);
    wr32(m, esi + 0x4u, ebx);
    eax = esi;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 004dc090 Fn_004dc090
//   original FUN_004dc090; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x004dc090, Fn_004dc090, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = ecx;
    wr32(m, eax, 0x0u);
    LIFT_RET(0);
}

// 004dc0f0 Fn_004dc0f0
//   original FUN_004dc0f0; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x004dc0f0, Fn_004dc0f0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_4dc0f5 = 0;
    edx = 0x1u;
    fR_4dc0f5 = (rd8(m, 0xd92ed8u) & (edx & 0xffu));
    if (!(fR_4dc0f5 == 0)) goto L_4dc22c;
    eax = 0u;
    wr32(m, 0xd92ed8u, (rd32(m, 0xd92ed8u) | edx));
    ecx = (ecx | 0xffffffffu);
    esp -= 4; wr32(m, esp, 0x41b310u);
    esp -= 4; wr32(m, esp, 0x8u);
    esp -= 4; wr32(m, esp, 0x18u);
    esp -= 4; wr32(m, esp, 0xd92e18u);
    wr32(m, 0xd92d58u, eax);
    wr32(m, 0xd92d5cu, edx);
    wr32(m, 0xd92d60u, eax);
    wr32(m, 0xd92d64u, eax);
    wr32(m, 0xd92d68u, eax);
    wr32(m, 0xd92d6cu, ecx);
    wr32(m, 0xd92d70u, eax);
    wr32(m, 0xd92d74u, edx);
    wr32(m, 0xd92d78u, eax);
    wr32(m, 0xd92d7cu, eax);
    wr32(m, 0xd92d80u, eax);
    wr32(m, 0xd92d84u, ecx);
    wr32(m, 0xd92d88u, eax);
    wr32(m, 0xd92d8cu, edx);
    wr32(m, 0xd92d90u, eax);
    wr32(m, 0xd92d94u, eax);
    wr32(m, 0xd92d98u, eax);
    wr32(m, 0xd92d9cu, ecx);
    wr32(m, 0xd92da0u, eax);
    wr32(m, 0xd92da4u, edx);
    wr32(m, 0xd92da8u, eax);
    wr32(m, 0xd92dacu, eax);
    wr32(m, 0xd92db0u, eax);
    wr32(m, 0xd92db4u, ecx);
    wr32(m, 0xd92db8u, eax);
    wr32(m, 0xd92dbcu, edx);
    wr32(m, 0xd92dc0u, eax);
    wr32(m, 0xd92dc4u, eax);
    wr32(m, 0xd92dc8u, eax);
    wr32(m, 0xd92dccu, ecx);
    wr32(m, 0xd92dd0u, eax);
    wr32(m, 0xd92dd4u, edx);
    wr32(m, 0xd92dd8u, eax);
    wr32(m, 0xd92ddcu, eax);
    wr32(m, 0xd92de0u, eax);
    wr32(m, 0xd92de4u, ecx);
    wr32(m, 0xd92de8u, eax);
    wr32(m, 0xd92decu, edx);
    wr32(m, 0xd92df0u, eax);
    wr32(m, 0xd92df4u, eax);
    wr32(m, 0xd92df8u, eax);
    wr32(m, 0xd92dfcu, ecx);
    wr32(m, 0xd92e00u, eax);
    wr32(m, 0xd92e04u, edx);
    wr32(m, 0xd92e08u, eax);
    wr32(m, 0xd92e0cu, eax);
    wr32(m, 0xd92e10u, eax);
    wr32(m, 0xd92e14u, ecx);
    LIFT_CALL(0x4022d0u, 0x4dc21fu);  // `vector_constructor_iterator'
    esp -= 4; wr32(m, esp, 0x911fc0u);
    LIFT_CALL(0x8f9fbfu, 0x4dc229u);  // _atexit
    esp = esp + 0x4u;
L_4dc22c:
    eax = 0xd92d58u;
    LIFT_RET(0);
}

// 004dc3c0 Fn_004dc3c0
//   original FUN_004dc3c0; register inputs: edi; returns with ret 4
REFLEX_PORT_FN(0x004dc3c0, Fn_004dc3c0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4dc3cb = 0;
    uint32_t fB_4dc3cb = 0;
    uint32_t fA_4dc3ed = 0;
    uint32_t fB_4dc3ed = 0;
    uint32_t fR_4dc403 = 0;
    wr32(m, edi, 0x4u);
    LIFT_CALL(0x4dc0f0u, 0x4dc3cbu);  // FUN_004dc0f0
    fA_4dc3cb = rd8(m, 0xda67d8u);
    fB_4dc3cb = 0x0u;
    if (fA_4dc3cb == fB_4dc3cb) goto L_4dc3df;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x4dc3dfu);
L_4dc3df:
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, 0x10u);
    esp -= 4; wr32(m, esp, 0x1cu);
    LIFT_CALL(rd32(m, 0x916358u) /* 8.287573390694476e-39 */, 0x4dc3eau);
    esp = esp + 0x8u;
    fA_4dc3ed = rd8(m, 0xda67d8u);
    fB_4dc3ed = 0x0u;
    esi = eax;
    if (fA_4dc3ed == fB_4dc3ed) goto L_4dc403;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4dc403u);
L_4dc403:
    fR_4dc403 = (esi & esi);
    if ((fR_4dc403 == 0)) goto L_4dc432;
    eax = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, 0xffffffffu);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esi + 0x18u, 0xfu);
    wr32(m, esi + 0x14u, 0x0u);
    eax = esi;
    wr8(m, esi + 0x4u, 0x0u);
    LIFT_CALL(0x4f1860u, 0x4dc429u);  // FUN_004f1860
    wr32(m, edi + 0x8u, esi);
    eax = edi;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_4dc432:
    eax = 0u;
    wr32(m, edi + 0x8u, eax);
    eax = edi;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 004dc440 Fn_004dc440
//   original FUN_004dc440; register inputs: esi; returns with ret 4
REFLEX_PORT_FN(0x004dc440, Fn_004dc440, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = 0u;
    wr32(m, esi + 0x14u, eax);
    esp -= 4; wr32(m, esp, 0xffffffffu);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esi + 0x18u, 0xfu);
    wr8(m, esi + 0x4u, (eax & 0xffu));
    eax = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, eax);
    eax = esi;
    LIFT_CALL(0x4f1860u, 0x4dc45eu);  // FUN_004f1860
    eax = esi;
    LIFT_RET(4);
}

// 004dc570 Fn_004dc570
//   original FUN_004dc570; register inputs: esi; returns with ret 4
REFLEX_PORT_FN(0x004dc570, Fn_004dc570, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_4dc575 = 0;
    LIFT_CALL(0x4f4a30u, 0x4dc575u);  // FUN_004f4a30
    fR_4dc575 = (eax & eax);
    if ((fR_4dc575 == 0)) goto L_4dc57d;
    wr32(m, eax, esi);
    goto L_4dc57f;
L_4dc57d:
    eax = 0u;
L_4dc57f:
    wr32(m, esi, eax);
    LIFT_CALL(0x4f4ea0u, 0x4dc586u);  // FUN_004f4ea0
    wr32(m, esi + 0x18u, eax);
    wr8(m, eax + 0x19u, 0x1u);
    eax = rd32(m, esi + 0x18u);
    wr32(m, eax + 0x4u, eax);
    eax = rd32(m, esi + 0x18u);
    wr32(m, eax, eax);
    eax = rd32(m, esi + 0x18u);
    wr32(m, eax + 0x8u, eax);
    eax = rd32(m, esp + 0x4u);
    wr32(m, esi + 0x24u, eax);
    wr32(m, esi + 0x1cu, 0x0u);
    wr32(m, esi + 0x20u, 0x0u);
    eax = esi;
    LIFT_RET(4);
}

// 004dc5c0 Fn_004dc5c0
//   original FUN_004dc5c0; register inputs: esi edi; returns with ret 4
REFLEX_PORT_FN(0x004dc5c0, Fn_004dc5c0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_4dc5c8 = 0;
    esp = esp - 0x18u;
    LIFT_CALL(0x4f4a30u, 0x4dc5c8u);  // FUN_004f4a30
    fR_4dc5c8 = (eax & eax);
    if ((fR_4dc5c8 == 0)) goto L_4dc5d0;
    wr32(m, eax, esi);
    goto L_4dc5d2;
L_4dc5d0:
    eax = 0u;
L_4dc5d2:
    wr32(m, esi, eax);
    LIFT_CALL(0x4f4ea0u, 0x4dc5d9u);  // FUN_004f4ea0
    wr32(m, esi + 0x18u, eax);
    wr8(m, eax + 0x19u, 0x1u);
    eax = rd32(m, esi + 0x18u);
    wr32(m, eax + 0x4u, eax);
    eax = rd32(m, esi + 0x18u);
    wr32(m, eax, eax);
    eax = rd32(m, esi + 0x18u);
    wr32(m, eax + 0x8u, eax);
    eax = rd32(m, esp + 0x1cu);
    ecx = (esp + 0xcu);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp + 0x4u, eax);
    esp -= 4; wr32(m, esp, esi);
    eax = (esp + 0x8u);
    wr32(m, esi + 0x1cu, 0x0u);
    wr32(m, esi + 0x20u, 0x0u);
    wr32(m, esi + 0x24u, edi);
    wr32(m, esp + 0xcu, esi);
    wr32(m, esp + 0x10u, edi);
    LIFT_CALL(0x4f06c0u, 0x4dc621u);  // FUN_004f06c0
    eax = esi;
    esp = esp + 0x18u;
    LIFT_RET(4);
}

// 004dc660 Fn_004dc660
//   original FUN_004dc660; register inputs: ecx eax; returns with ret 0
REFLEX_PORT_FN(0x004dc660, Fn_004dc660, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_4dc68d = 0;
    uint32_t fA_4dc691 = 0;
    uint32_t fB_4dc691 = 0;
    uint32_t fA_4dc69d = 0;
    uint32_t fB_4dc69d = 0;
    esp = esp - 0x1cu;
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    ecx = rd32(m, esi + 0x24u);
    esp -= 4; wr32(m, esp, edi);
    edx = (esp + 0xcu);
    wr32(m, esp + 0x14u, eax);
    esp -= 4; wr32(m, esp, edx);
    eax = (esp + 0x18u);
    wr32(m, esp + 0x1cu, esi);
    wr32(m, esp + 0x20u, ecx);
    LIFT_CALL(0x4f0860u, 0x4dc684u);  // FUN_004f0860
    eax = rd32(m, esp + 0xcu);
    edi = rd32(m, esi + 0x18u);
    esi = rd32(m, esi);
    fR_4dc68d = (eax & eax);
    if ((fR_4dc68d == 0)) goto L_4dc695;
    fA_4dc691 = eax;
    fB_4dc691 = esi;
    if (fA_4dc691 == fB_4dc691) goto L_4dc69b;
L_4dc695:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4dc69bu);
L_4dc69b:
    eax = 0u;
    fA_4dc69d = rd32(m, esp + 0x10u);
    fB_4dc69d = edi;
    edi = rd32(m, esp); esp += 4;
    eax = (eax & 0xffffff00u) | (((fA_4dc69d != fB_4dc69d) ? 1u : 0u) & 0xffu);
    esi = rd32(m, esp); esp += 4;
    esp = esp + 0x1cu;
    LIFT_RET(0);
}

// 004dc710 Fn_004dc710
//   original FUN_004dc710; register inputs: none; returns with ret 12
REFLEX_PORT_FN(0x004dc710, Fn_004dc710, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_4dc722 = 0;
    uint32_t fA_4dc72b = 0;
    uint32_t fB_4dc72b = 0;
    uint32_t fA_4dc7c6 = 0;
    uint32_t fB_4dc7c6 = 0;
    uint32_t fA_4dc7cc = 0;
    uint32_t fB_4dc7cc = 0;
    uint32_t fA_4dc7e2 = 0;
    uint32_t fB_4dc7e2 = 0;
    uint32_t fA_4dc7ec = 0;
    uint32_t fB_4dc7ec = 0;
    uint32_t fA_4dc838 = 0;
    uint32_t fB_4dc838 = 0;
    uint32_t fA_4dc862 = 0;
    uint32_t fB_4dc862 = 0;
    uint32_t fR_4dc8a5 = 0;
    uint32_t fA_4dc8a9 = 0;
    uint32_t fB_4dc8a9 = 0;
    uint32_t fA_4dc8b3 = 0;
    uint32_t fB_4dc8b3 = 0;
    uint32_t fA_4dc909 = 0;
    uint32_t fB_4dc909 = 0;
    uint32_t fA_4dc953 = 0;
    uint32_t fB_4dc953 = 0;
    uint32_t fR_4dc98e = 0;
    uint32_t fA_4dc992 = 0;
    uint32_t fB_4dc992 = 0;
    uint32_t fA_4dc99c = 0;
    uint32_t fB_4dc99c = 0;
    uint32_t fA_4dc9d7 = 0;
    uint32_t fB_4dc9d7 = 0;
    esp = esp - 0x40u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x48u);
    eax = rd32(m, ebx + 0x2cu);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x50u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    fR_4dc722 = (eax & eax);
    if ((fR_4dc722 == 0)) goto L_4dc7bb;
    eax = (eax - 1u);
    fA_4dc72b = eax;
    fB_4dc72b = 0x5u;
    if (fA_4dc72b > fB_4dc72b) goto L_4dca03;
    switch (eax) { case 0: goto L_4dc73b; case 1: goto L_4dc754; case 2: goto L_4dca03; case 3: goto L_4dc76d; case 4: goto L_4dc787; case 5: goto L_4dc7a1; default: throw std::runtime_error("jump table"); }
L_4dc73b:
    eax = rd32(m, esp + 0x5cu);
    eax = eax + 0x8u;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x4f5330u, 0x4dc74au);  // FUN_004f5330
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x40u;
    LIFT_RET(12);
L_4dc754:
    ecx = rd32(m, esp + 0x5cu);
    ecx = ecx + 0x8u;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x4f5650u, 0x4dc763u);  // FUN_004f5650
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x40u;
    LIFT_RET(12);
L_4dc76d:
    edx = rd32(m, esp + 0x5cu);
    eax = rd32(m, edx + 0x8u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ebx);
    eax = ebp;
    LIFT_CALL(0x4f5970u, 0x4dc77du);  // FUN_004f5970
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x40u;
    LIFT_RET(12);
L_4dc787:
    ecx = rd32(m, esp + 0x5cu);
    edx = rd32(m, ecx + 0x8u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, ebp);
    eax = ebx;
    LIFT_CALL(0x4f5c50u, 0x4dc797u);  // FUN_004f5c50
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x40u;
    LIFT_RET(12);
L_4dc7a1:
    eax = rd32(m, esp + 0x5cu);
    eax = eax + 0x8u;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ebp);
    eax = ebx;
    LIFT_CALL(0x4f5f80u, 0x4dc7b1u);  // FUN_004f5f80
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x40u;
    LIFT_RET(12);
L_4dc7bb:
    esp -= 4; wr32(m, esp, 0xae1c44u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x4dc7c6u);
    fA_4dc7c6 = rd32(m, ebx + 0x44u);
    fB_4dc7c6 = 0x0u;
    if (fA_4dc7c6 != fB_4dc7c6) goto L_4dc7d6;
    fA_4dc7cc = rd32(m, ebx + 0x48u);
    fB_4dc7cc = 0x0u;
    if (fA_4dc7cc == fB_4dc7cc) goto L_4dc9cb;
L_4dc7d6:
    esi = rd32(m, ebx + 0x40u);
    ecx = rd32(m, esi + 0x10u);
    ecx = ecx - rd32(m, esi + 0xcu);
    ecx = uint32_t(int32_t(ecx) >> 4);
    fA_4dc7e2 = rd32(m, ebx + 0x48u);
    fB_4dc7e2 = 0x0u;
    if (fA_4dc7e2 != fB_4dc7e2) goto L_4dc909;
    fA_4dc7ec = ebp;
    fB_4dc7ec = ecx;
    if (fA_4dc7ec < fB_4dc7ec) goto L_4dc7f6;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4dc7f6u);
L_4dc7f6:
    edx = rd32(m, esi + 0xcu);
    esi = rd32(m, ebx + 0x44u);
    eax = ebp;
    eax = (eax << 4);
    edx = edx + eax;
    wr32(m, esp + 0x58u, eax);
    esp -= 4; wr32(m, esp, edx);
    eax = (esp + 0x18u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x4f1180u, 0x4dc812u);  // FUN_004f1180
    esi = (esp + 0x14u);
    LIFT_CALL(0x4fdfc0u, 0x4dc81bu);  // FUN_004fdfc0
    esi = eax;
    ecx = rd32(m, esi + 0x34u);
    esi = esi + 0x10u;
    wr32(m, esp + 0x24u, ecx);
    ecx = (esp + 0x1cu);
    wr32(m, esp + 0x1cu, ebp);
    wr32(m, esp + 0x20u, esi);
    LIFT_CALL(0x4f0800u, 0x4dc838u);  // FUN_004f0800
    fA_4dc838 = rd32(m, esi + 0x1cu);
    fB_4dc838 = 0x0u;
    if (fA_4dc838 != fB_4dc838) goto L_4dc856;
    edx = rd32(m, esp + 0x18u);
    eax = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, edx);
    edx = rd32(m, ebx + 0x44u);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0x24u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x4f0e40u, 0x4dc856u);  // FUN_004f0e40
L_4dc856:
    esi = rd32(m, ebx + 0x40u);
    eax = rd32(m, esi + 0x10u);
    eax = eax - rd32(m, esi + 0xcu);
    eax = uint32_t(int32_t(eax) >> 4);
    fA_4dc862 = ebp;
    fB_4dc862 = eax;
    if (fA_4dc862 < fB_4dc862) goto L_4dc86c;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4dc86cu);
L_4dc86c:
    edi = rd32(m, esi + 0xcu);
    edi = edi + rd32(m, esp + 0x58u);
    LIFT_CALL(0x4eedc0u, 0x4dc878u);  // FUN_004eedc0
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esp + 0x60u);
    LIFT_CALL(0x4eee60u, 0x4dc882u);  // FUN_004eee60
    esi = rd32(m, ebx + 0x44u);
    esp -= 4; wr32(m, esp, edi);
    ecx = (esp + 0x20u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x4f1180u, 0x4dc890u);  // FUN_004f1180
    ecx = rd32(m, eax);
    esi = rd32(m, eax + 0x4u);
    eax = rd32(m, ebx + 0x44u);
    edi = rd32(m, eax + 0x18u);
    eax = rd32(m, eax);
    wr32(m, esp + 0x14u, ecx);
    wr32(m, esp + 0x18u, esi);
    fR_4dc8a5 = (ecx & ecx);
    if ((fR_4dc8a5 == 0)) goto L_4dc8ad;
    fA_4dc8a9 = ecx;
    fB_4dc8a9 = eax;
    if (fA_4dc8a9 == fB_4dc8a9) goto L_4dc8b3;
L_4dc8ad:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4dc8b3u);
L_4dc8b3:
    fA_4dc8b3 = esi;
    fB_4dc8b3 = edi;
    if (fA_4dc8b3 != fB_4dc8b3) goto L_4dc8f1;
    edx = rd32(m, ebx + 0x44u);
    edi = rd32(m, ebx + 0x20u);
    esp -= 4; wr32(m, esp, ebp);
    esi = (esp + 0x2cu);
    wr32(m, esp + 0x5cu, edx);
    LIFT_CALL(0x4dc5c0u, 0x4dc8cbu);  // FUN_004dc5c0
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, esp + 0x60u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, esp + 0x64u);
    LIFT_CALL(0x4f0cc0u, 0x4dc8dbu);  // FUN_004f0cc0
    esp = esp + 0x4u;
    edi = eax;
    LIFT_CALL(0x4ebe60u, 0x4dc8e5u);  // FUN_004ebe60
    edi = esi;
    LIFT_CALL(0x4ebde0u, 0x4dc8ecu);  // FUN_004ebde0
    goto L_4dc9f8;
L_4dc8f1:
    esi = (esp + 0x14u);
    LIFT_CALL(0x4fdfc0u, 0x4dc8fau);  // FUN_004fdfc0
    eax = eax + 0x10u;
    ecx = ebp;
    LIFT_CALL(0x4dc630u, 0x4dc904u);  // FUN_004dc630
    goto L_4dc9f8;
L_4dc909:
    fA_4dc909 = ebp;
    fB_4dc909 = ecx;
    if (fA_4dc909 < fB_4dc909) goto L_4dc913;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4dc913u);
L_4dc913:
    edx = rd32(m, esi + 0xcu);
    esi = rd32(m, ebx + 0x48u);
    eax = ebp;
    eax = (eax << 4);
    edx = edx + eax;
    wr32(m, esp + 0x58u, eax);
    esp -= 4; wr32(m, esp, edx);
    eax = (esp + 0x18u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x4f15c0u, 0x4dc92fu);  // FUN_004f15c0
    ecx = rd32(m, esp + 0x18u);
    edx = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = rd32(m, ebx + 0x48u);
    esp -= 4; wr32(m, esp, edx);
    eax = (esp + 0x24u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x4f12c0u, 0x4dc947u);  // FUN_004f12c0
    esi = rd32(m, ebx + 0x40u);
    edx = rd32(m, esi + 0x10u);
    edx = edx - rd32(m, esi + 0xcu);
    edx = uint32_t(int32_t(edx) >> 4);
    fA_4dc953 = ebp;
    fB_4dc953 = edx;
    if (fA_4dc953 < fB_4dc953) goto L_4dc95d;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4dc95du);
L_4dc95d:
    edi = rd32(m, esi + 0xcu);
    edi = edi + rd32(m, esp + 0x58u);
    LIFT_CALL(0x4eedc0u, 0x4dc969u);  // FUN_004eedc0
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esp + 0x60u);
    LIFT_CALL(0x4eee60u, 0x4dc973u);  // FUN_004eee60
    esi = rd32(m, ebx + 0x48u);
    esp -= 4; wr32(m, esp, edi);
    eax = (esp + 0x20u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x4f15c0u, 0x4dc981u);  // FUN_004f15c0
    ecx = rd32(m, eax);
    esi = rd32(m, eax + 0x4u);
    eax = rd32(m, ebx + 0x48u);
    edi = rd32(m, eax + 0x18u);
    eax = rd32(m, eax);
    fR_4dc98e = (ecx & ecx);
    if ((fR_4dc98e == 0)) goto L_4dc996;
    fA_4dc992 = ecx;
    fB_4dc992 = eax;
    if (fA_4dc992 == fB_4dc992) goto L_4dc99c;
L_4dc996:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4dc99cu);
L_4dc99c:
    fA_4dc99c = esi;
    fB_4dc99c = edi;
    if (fA_4dc99c != fB_4dc99c) goto L_4dc9f8;
    esi = rd32(m, ebx + 0x20u);
    ebx = rd32(m, ebx + 0x48u);
    eax = rd32(m, esp + 0x5cu);
    esp -= 4; wr32(m, esp, ebx);
    wr32(m, esp + 0x20u, ebp);
    wr32(m, esp + 0x24u, 0x0u);
    LIFT_CALL(0x4f1200u, 0x4dc9bcu);  // FUN_004f1200
    x0 = movsd_m(rd64(m, esp + 0x1cu));
    wr64(m, eax, x0.lo);
    wr32(m, eax + 0x8u, esi);
    goto L_4dc9f8;
L_4dc9cb:
    ebx = rd32(m, ebx + 0x40u);
    ecx = rd32(m, ebx + 0x10u);
    ecx = ecx - rd32(m, ebx + 0xcu);
    ecx = uint32_t(int32_t(ecx) >> 4);
    fA_4dc9d7 = ebp;
    fB_4dc9d7 = ecx;
    if (fA_4dc9d7 < fB_4dc9d7) goto L_4dc9e1;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4dc9e1u);
L_4dc9e1:
    ebp = (ebp << 4);
    ebp = ebp + rd32(m, ebx + 0xcu);
    edi = ebp;
    LIFT_CALL(0x4eedc0u, 0x4dc9eeu);  // FUN_004eedc0
    edi = rd32(m, esp + 0x5cu);
    esp -= 4; wr32(m, esp, ebp);
    LIFT_CALL(0x4eee60u, 0x4dc9f8u);  // FUN_004eee60
L_4dc9f8:
    esp -= 4; wr32(m, esp, 0xae1c44u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4dca03u);
L_4dca03:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x40u;
    LIFT_RET(12);
}

// 004dca30 Fn_004dca30
//   original FUN_004dca30; register inputs: ecx eax; returns with ret 0
REFLEX_PORT_FN(0x004dca30, Fn_004dca30, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_4dca3c = 0;
    uint32_t fA_4dca41 = 0;
    uint32_t fB_4dca41 = 0;
    uint32_t fR_4dcac1 = 0;
    uint32_t fA_4dcac5 = 0;
    uint32_t fB_4dcac5 = 0;
    uint32_t fA_4dcad9 = 0;
    uint32_t fB_4dcad9 = 0;
    uint32_t fR_4dcadd = 0;
    uint32_t fA_4dcae3 = 0;
    uint32_t fB_4dcae3 = 0;
    esp = esp - 0xcu;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = ecx;
    ecx = rd32(m, edi + 0x2cu);
    fR_4dca3c = (ecx & ecx);
    if ((fR_4dca3c == 0)) goto L_4dcaa6;
    ecx = (ecx - 1u);
    fA_4dca41 = ecx;
    fB_4dca41 = 0x5u;
    if (fA_4dca41 > fB_4dca41) goto L_4dcaf9;
    switch (ecx) { case 0: goto L_4dca51; case 1: goto L_4dca62; case 2: goto L_4dcaf9; case 3: goto L_4dca73; case 4: goto L_4dca84; case 5: goto L_4dca95; default: throw std::runtime_error("jump table"); }
L_4dca51:
    eax = eax + 0x8u;
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x418e50u, 0x4dca5au);  // FUN_00418e50
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0xcu;
    LIFT_RET(0);
L_4dca62:
    eax = eax + 0x8u;
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x4f6270u, 0x4dca6bu);  // FUN_004f6270
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0xcu;
    LIFT_RET(0);
L_4dca73:
    eax = rd32(m, eax + 0x8u);
    ecx = edi;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0xcu;
    LIFT_TAILJMP(0x4f62e0u);  // FUN_004f62e0
L_4dca84:
    eax = rd32(m, eax + 0x8u);
    ecx = edi;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0xcu;
    LIFT_TAILJMP(0x4f6350u);  // FUN_004f6350
L_4dca95:
    eax = eax + 0x8u;
    ecx = edi;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0xcu;
    LIFT_TAILJMP(0x4f63c0u);  // FUN_004f63c0
L_4dcaa6:
    esi = rd32(m, edi + 0x44u);
    esp -= 4; wr32(m, esp, eax);
    eax = (esp + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x4f1180u, 0x4dcab4u);  // FUN_004f1180
    edi = rd32(m, edi + 0x44u);
    esi = rd32(m, eax);
    ebp = rd32(m, edi + 0x18u);
    ebx = rd32(m, eax + 0x4u);
    edi = rd32(m, edi);
    fR_4dcac1 = (esi & esi);
    if ((fR_4dcac1 == 0)) goto L_4dcac9;
    fA_4dcac5 = esi;
    fB_4dcac5 = edi;
    if (fA_4dcac5 == fB_4dcac5) goto L_4dcad3;
L_4dcac9:
    edi = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    LIFT_CALL(edi, 0x4dcad1u);
    goto L_4dcad9;
L_4dcad3:
    edi = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
L_4dcad9:
    fA_4dcad9 = ebx;
    fB_4dcad9 = ebp;
    if (fA_4dcad9 == fB_4dcad9) goto L_4dcaf9;
    fR_4dcadd = (esi & esi);
    if (!(fR_4dcadd == 0)) goto L_4dcaf5;
    LIFT_CALL(edi, 0x4dcae3u);
L_4dcae3:
    fA_4dcae3 = ebx;
    fB_4dcae3 = rd32(m, esi + 0x18u);
    if (fA_4dcae3 != fB_4dcae3) goto L_4dcaea;
    LIFT_CALL(edi, 0x4dcaeau);
L_4dcaea:
    eax = (ebx + 0x20u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0xcu;
    LIFT_RET(0);
L_4dcaf5:
    esi = rd32(m, esi);
    goto L_4dcae3;
L_4dcaf9:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = 0xae1c1cu;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0xcu;
    LIFT_RET(0);
}

// 004dcb20 Fn_004dcb20
//   original FUN_004dcb20; register inputs: ecx eax; returns with ret 0
REFLEX_PORT_FN(0x004dcb20, Fn_004dcb20, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_4dcb2c = 0;
    uint32_t fA_4dcb31 = 0;
    uint32_t fB_4dcb31 = 0;
    uint32_t fR_4dcbb1 = 0;
    uint32_t fA_4dcbb5 = 0;
    uint32_t fB_4dcbb5 = 0;
    uint32_t fA_4dcbc9 = 0;
    uint32_t fB_4dcbc9 = 0;
    uint32_t fR_4dcbcd = 0;
    uint32_t fA_4dcbd3 = 0;
    uint32_t fB_4dcbd3 = 0;
    esp = esp - 0xcu;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = ecx;
    ecx = rd32(m, edi + 0x2cu);
    fR_4dcb2c = (ecx & ecx);
    if ((fR_4dcb2c == 0)) goto L_4dcb96;
    ecx = (ecx - 1u);
    fA_4dcb31 = ecx;
    fB_4dcb31 = 0x5u;
    if (fA_4dcb31 > fB_4dcb31) goto L_4dcbe9;
    switch (ecx) { case 0: goto L_4dcb41; case 1: goto L_4dcb52; case 2: goto L_4dcbe9; case 3: goto L_4dcb63; case 4: goto L_4dcb74; case 5: goto L_4dcb85; default: throw std::runtime_error("jump table"); }
L_4dcb41:
    eax = eax + 0x8u;
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x418ec0u, 0x4dcb4au);  // FUN_00418ec0
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0xcu;
    LIFT_RET(0);
L_4dcb52:
    eax = eax + 0x8u;
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x4f6430u, 0x4dcb5bu);  // FUN_004f6430
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0xcu;
    LIFT_RET(0);
L_4dcb63:
    eax = rd32(m, eax + 0x8u);
    ecx = edi;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0xcu;
    LIFT_TAILJMP(0x4f64a0u);  // FUN_004f64a0
L_4dcb74:
    eax = rd32(m, eax + 0x8u);
    ecx = edi;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0xcu;
    LIFT_TAILJMP(0x4f6510u);  // FUN_004f6510
L_4dcb85:
    eax = eax + 0x8u;
    ecx = edi;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0xcu;
    LIFT_TAILJMP(0x4f6580u);  // FUN_004f6580
L_4dcb96:
    esi = rd32(m, edi + 0x48u);
    esp -= 4; wr32(m, esp, eax);
    eax = (esp + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x4f15c0u, 0x4dcba4u);  // FUN_004f15c0
    edi = rd32(m, edi + 0x48u);
    esi = rd32(m, eax);
    ebp = rd32(m, edi + 0x18u);
    ebx = rd32(m, eax + 0x4u);
    edi = rd32(m, edi);
    fR_4dcbb1 = (esi & esi);
    if ((fR_4dcbb1 == 0)) goto L_4dcbb9;
    fA_4dcbb5 = esi;
    fB_4dcbb5 = edi;
    if (fA_4dcbb5 == fB_4dcbb5) goto L_4dcbc3;
L_4dcbb9:
    edi = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    LIFT_CALL(edi, 0x4dcbc1u);
    goto L_4dcbc9;
L_4dcbc3:
    edi = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
L_4dcbc9:
    fA_4dcbc9 = ebx;
    fB_4dcbc9 = ebp;
    if (fA_4dcbc9 == fB_4dcbc9) goto L_4dcbe9;
    fR_4dcbcd = (esi & esi);
    if (!(fR_4dcbcd == 0)) goto L_4dcbe5;
    LIFT_CALL(edi, 0x4dcbd3u);
L_4dcbd3:
    fA_4dcbd3 = ebx;
    fB_4dcbd3 = rd32(m, esi + 0x18u);
    if (fA_4dcbd3 != fB_4dcbd3) goto L_4dcbda;
    LIFT_CALL(edi, 0x4dcbdau);
L_4dcbda:
    eax = (ebx + 0x20u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0xcu;
    LIFT_RET(0);
L_4dcbe5:
    esi = rd32(m, esi);
    goto L_4dcbd3;
L_4dcbe9:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = 0x9c9ff4u;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0xcu;
    LIFT_RET(0);
}

// 004dce30 AssetHandle_Name
//   original AssetHandle_Name; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x004dce30, AssetHandle_Name, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = (ecx + 0x4u);
    LIFT_RET(0);
}

// 004dd3d0 Fn_004dd3d0
//   original FUN_004dd3d0; register inputs: none; returns with ret 4
REFLEX_PORT_FN(0x004dd3d0, Fn_004dd3d0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4dd3dd = 0;
    uint32_t fB_4dd3dd = 0;
    uint32_t fA_4dd41f = 0;
    uint32_t fB_4dd41f = 0;
    uint32_t fA_4dd43a = 0;
    uint32_t fB_4dd43a = 0;
    uint32_t fA_4dd44d = 0;
    uint32_t fB_4dd44d = 0;
    eax = rd32(m, 0xae0e70u);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    fA_4dd3dd = eax;
    fB_4dd3dd = rd32(m, 0xae0e7cu);
    if (fA_4dd3dd == fB_4dd3dd) goto L_4dd452;
    ebx = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    esi = 0u;
    wr32(m, 0xae0e7cu, eax);
    wr32(m, esp + 0x14u, esi);
    goto L_4dd400;
L_4dd400:
    esp -= 4; wr32(m, esp, 0x93ded0u);
    eax = 0xae0e08u;
    LIFT_CALL(0x402160u, 0x4dd40fu);  // Table_ColumnIndex
    edi = eax;
    eax = rd32(m, 0xae0e1cu);
    eax = eax - rd32(m, 0xae0e18u);
    eax = uint32_t(int32_t(eax) >> 2);
    fA_4dd41f = edi;
    fB_4dd41f = eax;
    if (fA_4dd41f < fB_4dd41f) goto L_4dd425;
    LIFT_CALL(ebx, 0x4dd425u);
L_4dd425:
    ecx = rd32(m, 0xae0e18u);
    edx = rd32(m, ecx + edi * 4);
    esp -= 4; wr32(m, esp, edx);
    eax = (esp + 0x18u);
    LIFT_CALL(0x418e50u, 0x4dd438u);  // FUN_00418e50
    ecx = 0u;
    fA_4dd43a = ecx;
    fB_4dd43a = rd32(m, eax + 0x1cu);
    edx = edx - edx - (fA_4dd43a < fB_4dd43a ? 1u : 0u);
    edx = (0u - edx);
    wr32(m, esi * 4 + 0xae0e80u, edx);
    esi = (esi + 1u);
    wr32(m, esp + 0x14u, esi);
    fA_4dd44d = esi;
    fB_4dd44d = 0x4u;
    if (fA_4dd44d < fB_4dd44d) goto L_4dd400;
L_4dd452:
    eax = rd32(m, ebp * 4 + 0xae0e80u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 004e7780 Fn_004e7780
//   original FUN_004e7780; register inputs: none; returns with ret 4
REFLEX_PORT_FN(0x004e7780, Fn_004e7780, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4e77ac = 0;
    uint32_t fB_4e77ac = 0;
    uint32_t fA_4e77fe = 0;
    uint32_t fB_4e77fe = 0;
    uint32_t fA_4e785c = 0;
    uint32_t fB_4e785c = 0;
    uint32_t fA_4e7864 = 0;
    uint32_t fB_4e7864 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0xdcu;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, 0x935e08u);
    eax = 0xae0910u;
    LIFT_CALL(0x402220u, 0x4e779eu);  // Database_FindTable
    esp -= 4; wr32(m, esp, 0x4u);
    esi = eax;
    LIFT_CALL(0x78fbe0u, 0x4e77a7u);  // ReflexAlignedAlloc
    ebx = 0u;
    esp = esp + 0x4u;
    fA_4e77ac = eax;
    fB_4e77ac = ebx;
    if (fA_4e77ac == fB_4e77ac) goto L_4e77bc;
    ecx = (esp + 0x20u);
    wr32(m, eax, ecx);
    wr32(m, esp + 0x20u, eax);
    goto L_4e77c0;
L_4e77bc:
    wr32(m, esp + 0x20u, ebx);
L_4e77c0:
    edx = rd32(m, ebp + 0x8u);
    eax = rd32(m, edx * 4 + 0x9ccd40u);
    esp -= 4; wr32(m, esp, 0x93e52cu);
    ecx = (esp + 0x3cu);
    wr32(m, esp + 0x14u, eax);
    esp -= 4; wr32(m, esp, ecx);
    eax = (esp + 0x18u);
    wr32(m, esp + 0x34u, ebx);
    wr32(m, esp + 0x38u, ebx);
    wr32(m, esp + 0x3cu, ebx);
    LIFT_CALL(0x4f7480u, 0x4e77edu);  // FUN_004f7480
    esp -= 4; wr32(m, esp, eax);
    edi = (esp + 0x24u);
    LIFT_CALL(0x4175e0u, 0x4e77f7u);  // FUN_004175e0
    eax = rd32(m, esp + 0xdcu);
    fA_4e77fe = eax;
    fB_4e77fe = ebx;
    if (fA_4e77fe == fB_4e77fe) goto L_4e780b;
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x78fc50u, 0x4e7808u);  // ReflexAlignedFree
    esp = esp + 0x4u;
L_4e780b:
    edx = rd32(m, esp + 0xd0u);
    esp -= 4; wr32(m, esp, edx);
    wr32(m, esp + 0xe0u, ebx);
    wr32(m, esp + 0xe4u, ebx);
    wr32(m, esp + 0xe8u, ebx);
    LIFT_CALL(0x78fc50u, 0x4e782du);  // ReflexAlignedFree
    esp = esp + 0x4u;
    edi = (esp + 0xc0u);
    LIFT_CALL(0x4eedc0u, 0x4e783cu);  // FUN_004eedc0
    eax = (esp + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0x24u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, esi);
    wr32(m, esp + 0x20u, 0xffffffffu);
    wr32(m, esp + 0x28u, ebx);
    LIFT_CALL(0x4ed410u, 0x4e7858u);  // FUN_004ed410
    esi = rd32(m, esp + 0x1cu);
    fA_4e785c = esi;
    fB_4e785c = ebx;
    if (fA_4e785c == fB_4e785c) goto L_4e7895;
    edi = rd32(m, esp + 0x14u);
    fA_4e7864 = edi;
    fB_4e7864 = 0xffffffffu;
    if (fA_4e7864 == fB_4e7864) goto L_4e7895;
    esp -= 4; wr32(m, esp, 0x936e90u);
    eax = esi;
    LIFT_CALL(0x402160u, 0x4e7875u);  // Table_ColumnIndex
    esp -= 4; wr32(m, esp, edi);
    edi = eax;
    eax = esi;
    LIFT_CALL(0x41aaf0u, 0x4e787fu);  // FUN_0041aaf0
    edi = rd32(m, eax);
    esi = (esp + 0x20u);
    LIFT_CALL(0x417590u, 0x4e788au);  // FUN_00417590
    eax = edi;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_4e7895:
    esi = (esp + 0x20u);
    LIFT_CALL(0x417590u, 0x4e789eu);  // FUN_00417590
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    eax = (eax | 0xffffffffu);
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 004eb4e0 Fn_004eb4e0
//   original FUN_004eb4e0; register inputs: ecx eax; returns with ret 0
REFLEX_PORT_FN(0x004eb4e0, Fn_004eb4e0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4eb4e4 = 0;
    uint32_t fB_4eb4e4 = 0;
    esp -= 4; wr32(m, esp, ecx);
    ecx = rd32(m, eax + 0x2cu);
    fA_4eb4e4 = ecx;
    fB_4eb4e4 = 0x6u;
    if (fA_4eb4e4 > fB_4eb4e4) goto L_4eb532;
    switch (ecx) { case 0: goto L_4eb4f0; case 1: goto L_4eb4fb; case 2: goto L_4eb506; case 3: goto L_4eb532; case 4: goto L_4eb511; case 5: goto L_4eb51c; case 6: goto L_4eb528; default: throw std::runtime_error("jump table"); }
L_4eb4f0:
    ecx = (eax + 0x30u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x4f7530u, 0x4eb4f9u);  // FUN_004f7530
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_4eb4fb:
    edx = (eax + 0x38u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x4f76b0u, 0x4eb504u);  // FUN_004f76b0
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_4eb506:
    ecx = (eax + 0x38u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x4f7830u, 0x4eb50fu);  // FUN_004f7830
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_4eb511:
    edx = rd32(m, eax + 0x38u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x4f79b0u, 0x4eb51au);  // FUN_004f79b0
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_4eb51c:
    ecx = rd32(m, eax + 0x38u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x4f7b40u, 0x4eb526u);  // FUN_004f7b40
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_4eb528:
    edx = (eax + 0x38u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x4f7ce0u, 0x4eb532u);  // FUN_004f7ce0
L_4eb532:
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 004eb830 Fn_004eb830
//   original FUN_004eb830; register inputs: eax; returns with ret 0
REFLEX_PORT_FN(0x004eb830, Fn_004eb830, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4eb844 = 0;
    uint32_t fB_4eb844 = 0;
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, 0xae1c44u);
    edi = eax;
    esi = 0u;
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x4eb841u);
    eax = rd32(m, edi + 0x2cu);
    fA_4eb844 = eax;
    fB_4eb844 = 0x6u;
    if (fA_4eb844 > fB_4eb844) goto L_4eb8ee;
    switch (eax) { case 0: goto L_4eb854; case 1: goto L_4eb870; case 2: goto L_4eb870; case 3: goto L_4eb8ee; case 4: goto L_4eb88c; case 5: goto L_4eb8b8; case 6: goto L_4eb8e2; default: throw std::runtime_error("jump table"); }
L_4eb854:
    edi = rd32(m, edi + 0x40u);
    esi = rd32(m, edi + 0x10u);
    esi = esi - rd32(m, edi + 0xcu);
    esp -= 4; wr32(m, esp, 0xae1c44u);
    esi = uint32_t(int32_t(esi) >> 4);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4eb86bu);
    edi = rd32(m, esp); esp += 4;
    eax = esi;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_4eb870:
    edi = rd32(m, edi + 0x40u);
    esi = rd32(m, edi + 0x10u);
    esi = esi - rd32(m, edi + 0xcu);
    esp -= 4; wr32(m, esp, 0xae1c44u);
    esi = uint32_t(int32_t(esi) >> 2);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4eb887u);
    edi = rd32(m, esp); esp += 4;
    eax = esi;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_4eb88c:
    edi = rd32(m, edi + 0x40u);
    ecx = rd32(m, edi + 0x10u);
    ecx = ecx - rd32(m, edi + 0xcu);
    eax = 0x92492493u;
    { int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); }
    edx = edx + ecx;
    edx = uint32_t(int32_t(edx) >> 4);
    esi = edx;
    esi = (esi >> 31);
    esp -= 4; wr32(m, esp, 0xae1c44u);
    esi = esi + edx;
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4eb8b3u);
    edi = rd32(m, esp); esp += 4;
    eax = esi;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_4eb8b8:
    edi = rd32(m, edi + 0x40u);
    ecx = rd32(m, edi + 0x10u);
    ecx = ecx - rd32(m, edi + 0xcu);
    eax = 0x2aaaaaabu;
    { int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); }
    edx = uint32_t(int32_t(edx) >> 2);
    esi = edx;
    esi = (esi >> 31);
    esp -= 4; wr32(m, esp, 0xae1c44u);
    esi = esi + edx;
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4eb8ddu);
    edi = rd32(m, esp); esp += 4;
    eax = esi;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_4eb8e2:
    edi = rd32(m, edi + 0x40u);
    esi = rd32(m, edi + 0x10u);
    esi = esi - rd32(m, edi + 0xcu);
    esi = uint32_t(int32_t(esi) >> 3);
L_4eb8ee:
    esp -= 4; wr32(m, esp, 0xae1c44u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4eb8f9u);
    edi = rd32(m, esp); esp += 4;
    eax = esi;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 004eb9f0 Fn_004eb9f0
//   original FUN_004eb9f0; register inputs: ecx eax; returns with ret 8
REFLEX_PORT_FN(0x004eb9f0, Fn_004eb9f0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4eba03 = 0;
    uint32_t fB_4eba03 = 0;
    uint32_t fA_4eba32 = 0;
    uint32_t fB_4eba32 = 0;
    uint32_t fA_4eba4d = 0;
    uint32_t fB_4eba4d = 0;
    uint32_t fA_4eba6d = 0;
    uint32_t fB_4eba6d = 0;
    uint32_t fA_4eba85 = 0;
    uint32_t fB_4eba85 = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    eax = rd32(m, ebp + 0x2cu);
    esp -= 4; wr32(m, esp, edi);
    edi = ecx;
    fA_4eba03 = eax;
    fB_4eba03 = 0x6u;
    if (fA_4eba03 > fB_4eba03) goto L_4eba9a;
    switch (eax) { case 0: goto L_4eba13; case 1: goto L_4eba2c; case 2: goto L_4eba47; case 3: goto L_4eba9a; case 4: goto L_4eba62; case 5: goto L_4eba9a; case 6: goto L_4eba7f; default: throw std::runtime_error("jump table"); }
L_4eba13:
    esp -= 4; wr32(m, esp, ebp);
    eax = edi;
    LIFT_CALL(0x4fa840u, 0x4eba1bu);  // FUN_004fa840
    esp -= 4; wr32(m, esp, ebp);
    ecx = eax;
    edx = ebx;
    LIFT_CALL(0x4dccb0u, 0x4eba25u);  // FUN_004dccb0
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(8);
L_4eba2c:
    esp -= 4; wr32(m, esp, ebp);
    LIFT_CALL(0x4fa890u, 0x4eba32u);  // FUN_004fa890
    fA_4eba32 = ebx;
    fB_4eba32 = 0x5u;
    if (fA_4eba32 > fB_4eba32) goto L_4eba9a;
    esp -= 4; wr32(m, esp, ebx);
    ecx = (esi + 0x8u);
    LIFT_CALL(0x4f6740u, 0x4eba40u);  // FUN_004f6740
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(8);
L_4eba47:
    esp -= 4; wr32(m, esp, ebp);
    LIFT_CALL(0x4fa890u, 0x4eba4du);  // FUN_004fa890
    fA_4eba4d = ebx;
    fB_4eba4d = 0x5u;
    if (fA_4eba4d > fB_4eba4d) goto L_4eba9a;
    esp -= 4; wr32(m, esp, ebx);
    ecx = (esi + 0x8u);
    LIFT_CALL(0x4f67c0u, 0x4eba5bu);  // FUN_004f67c0
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(8);
L_4eba62:
    esp -= 4; wr32(m, esp, ebp);
    LIFT_CALL(0x41a480u, 0x4eba68u);  // FUN_0041a480
    ecx = eax;
    eax = rd32(m, esi + 0x8u);
    fA_4eba6d = ebx;
    fB_4eba6d = 0x5u;
    if (fA_4eba6d > fB_4eba6d) goto L_4eba9a;
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x4f6850u, 0x4eba78u);  // FUN_004f6850
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(8);
L_4eba7f:
    esp -= 4; wr32(m, esp, ebp);
    LIFT_CALL(0x4fa920u, 0x4eba85u);  // FUN_004fa920
    fA_4eba85 = ebx;
    fB_4eba85 = 0x5u;
    if (fA_4eba85 > fB_4eba85) goto L_4eba9a;
    esp -= 4; wr32(m, esp, ebx);
    ecx = (esi + 0x8u);
    LIFT_CALL(0x4f6920u, 0x4eba93u);  // FUN_004f6920
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(8);
L_4eba9a:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = (eax & 0xffffff00u) | ((0u) & 0xffu);
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 004ebce0 Fn_004ebce0
//   original FUN_004ebce0; register inputs: edi; returns with ret 4
REFLEX_PORT_FN(0x004ebce0, Fn_004ebce0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4ebcf1 = 0;
    uint32_t fB_4ebcf1 = 0;
    uint32_t fA_4ebd26 = 0;
    uint32_t fB_4ebd26 = 0;
    uint32_t fA_4ebd35 = 0;
    uint32_t fB_4ebd35 = 0;
    uint32_t fR_4ebd65 = 0;
    uint32_t fA_4ebd69 = 0;
    uint32_t fB_4ebd69 = 0;
    uint32_t fA_4ebd73 = 0;
    uint32_t fB_4ebd73 = 0;
    uint32_t fR_4ebd79 = 0;
    uint32_t fA_4ebd87 = 0;
    uint32_t fB_4ebd87 = 0;
    esp = esp - 0x24u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x2cu);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x4f4a30u, 0x4ebcefu);  // FUN_004f4a30
    ebp = 0u;
    fA_4ebcf1 = eax;
    fB_4ebcf1 = ebp;
    if (fA_4ebcf1 == fB_4ebcf1) goto L_4ebcf9;
    wr32(m, eax, edi);
    goto L_4ebcfb;
L_4ebcf9:
    eax = 0u;
L_4ebcfb:
    wr32(m, edi, eax);
    LIFT_CALL(0x4f4ea0u, 0x4ebd02u);  // FUN_004f4ea0
    wr32(m, edi + 0x18u, eax);
    wr8(m, eax + 0x19u, 0x1u);
    eax = rd32(m, edi + 0x18u);
    wr32(m, eax + 0x4u, eax);
    eax = rd32(m, edi + 0x18u);
    wr32(m, eax, eax);
    eax = rd32(m, edi + 0x18u);
    wr32(m, eax + 0x8u, eax);
    wr32(m, edi + 0x1cu, ebp);
    wr32(m, edi + 0x20u, ebp);
    eax = rd32(m, ebx + 0x24u);
    wr32(m, edi + 0x24u, eax);
    fA_4ebd26 = rd32(m, ebx + 0x20u);
    fB_4ebd26 = ebp;
    if (fA_4ebd26 == fB_4ebd26) goto L_4ebd50;
    esp -= 4; wr32(m, esp, 0x18u);
    LIFT_CALL(0x78fbe0u, 0x4ebd32u);  // ReflexAlignedAlloc
    esp = esp + 0x4u;
    fA_4ebd35 = eax;
    fB_4ebd35 = ebp;
    if (fA_4ebd35 == fB_4ebd35) goto L_4ebd42;
    esi = eax;
    LIFT_CALL(0x5442c0u, 0x4ebd40u);  // FUN_005442c0
    goto L_4ebd44;
L_4ebd42:
    eax = 0u;
L_4ebd44:
    wr32(m, edi + 0x20u, eax);
    ecx = rd32(m, ebx + 0x20u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x4f0940u, 0x4ebd50u);  // FUN_004f0940
L_4ebd50:
    edx = rd32(m, ebx + 0x18u);
    eax = rd32(m, edx);
    esi = rd32(m, ebx);
    wr32(m, esp + 0x14u, eax);
    wr32(m, esp + 0x10u, esi);
L_4ebd60:
    ebp = rd32(m, ebx + 0x18u);
    eax = rd32(m, ebx);
    fR_4ebd65 = (esi & esi);
    if ((fR_4ebd65 == 0)) goto L_4ebd6d;
    fA_4ebd69 = esi;
    fB_4ebd69 = eax;
    if (fA_4ebd69 == fB_4ebd69) goto L_4ebd73;
L_4ebd6d:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ebd73u);
L_4ebd73:
    fA_4ebd73 = rd32(m, esp + 0x14u);
    fB_4ebd73 = ebp;
    if (fA_4ebd73 == fB_4ebd73) goto L_4ebdca;
    fR_4ebd79 = (esi & esi);
    if (!(fR_4ebd79 == 0)) goto L_4ebdc6;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ebd83u);
L_4ebd83:
    ecx = rd32(m, esp + 0x14u);
    fA_4ebd87 = ecx;
    fB_4ebd87 = rd32(m, esi + 0x18u);
    if (fA_4ebd87 != fB_4ebd87) goto L_4ebd92;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ebd92u);
L_4ebd92:
    edx = rd32(m, esp + 0x14u);
    eax = rd32(m, edx + 0xcu);
    ecx = rd32(m, edi + 0x24u);
    edx = (esp + 0x24u);
    esp -= 4; wr32(m, esp, edx);
    wr32(m, esp + 0x1cu, eax);
    esp -= 4; wr32(m, esp, edi);
    eax = (esp + 0x20u);
    wr32(m, esp + 0x24u, edi);
    wr32(m, esp + 0x28u, ecx);
    LIFT_CALL(0x4f06c0u, 0x4ebdb7u);  // FUN_004f06c0
    esi = (esp + 0x10u);
    LIFT_CALL(0x417df0u, 0x4ebdc0u);  // FUN_00417df0
    esi = rd32(m, esp + 0x10u);
    goto L_4ebd60;
L_4ebdc6:
    esi = rd32(m, esi);
    goto L_4ebd83;
L_4ebdca:
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = edi;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x24u;
    LIFT_RET(4);
}

// 004ebe60 Fn_004ebe60
//   original FUN_004ebe60; register inputs: edi; returns with ret 4
REFLEX_PORT_FN(0x004ebe60, Fn_004ebe60, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_4ebe6f = 0;
    uint32_t fA_4ebe7f = 0;
    uint32_t fB_4ebe7f = 0;
    uint32_t fR_4ebe8f = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x8u);
    eax = rd32(m, ebx + 0x24u);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, edi + 0x20u);
    wr32(m, edi + 0x24u, eax);
    fR_4ebe6f = (esi & esi);
    if ((fR_4ebe6f == 0)) goto L_4ebe7f;
    LIFT_CALL(0x4ebe10u, 0x4ebe78u);  // FUN_004ebe10
    wr32(m, edi + 0x20u, 0x0u);
L_4ebe7f:
    fA_4ebe7f = rd32(m, ebx + 0x20u);
    fB_4ebe7f = 0x0u;
    if (fA_4ebe7f == fB_4ebe7f) goto L_4ebeaa;
    esp -= 4; wr32(m, esp, 0x18u);
    LIFT_CALL(0x78fbe0u, 0x4ebe8cu);  // ReflexAlignedAlloc
    esp = esp + 0x4u;
    fR_4ebe8f = (eax & eax);
    if ((fR_4ebe8f == 0)) goto L_4ebe9c;
    esi = eax;
    LIFT_CALL(0x5442c0u, 0x4ebe9au);  // FUN_005442c0
    goto L_4ebe9e;
L_4ebe9c:
    eax = 0u;
L_4ebe9e:
    wr32(m, edi + 0x20u, eax);
    ecx = rd32(m, ebx + 0x20u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x4f0940u, 0x4ebeaau);  // FUN_004f0940
L_4ebeaa:
    edx = rd32(m, edi + 0x18u);
    eax = rd32(m, edx + 0x4u);
    esp -= 4; wr32(m, esp, eax);
    ecx = edi;
    LIFT_CALL(0x4f2020u, 0x4ebeb8u);  // FUN_004f2020
    eax = rd32(m, edi + 0x18u);
    wr32(m, eax + 0x4u, eax);
    eax = rd32(m, edi + 0x18u);
    wr32(m, edi + 0x1cu, 0x0u);
    wr32(m, eax, eax);
    eax = rd32(m, edi + 0x18u);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, edi);
    wr32(m, eax + 0x8u, eax);
    LIFT_CALL(0x4ebee0u, 0x4ebed7u);  // FUN_004ebee0
    esi = rd32(m, esp); esp += 4;
    eax = edi;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 004ebee0 Fn_004ebee0
//   original FUN_004ebee0; register inputs: none; returns with ret 8
REFLEX_PORT_FN(0x004ebee0, Fn_004ebee0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_4ebf05 = 0;
    uint32_t fA_4ebf09 = 0;
    uint32_t fB_4ebf09 = 0;
    uint32_t fA_4ebf13 = 0;
    uint32_t fB_4ebf13 = 0;
    uint32_t fR_4ebf17 = 0;
    uint32_t fA_4ebf21 = 0;
    uint32_t fB_4ebf21 = 0;
    esp = esp - 0x24u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x30u);
    eax = rd32(m, ebx + 0x18u);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, eax);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, ebx);
    esp -= 4; wr32(m, esp, edi);
    wr32(m, esp + 0x14u, ebp);
    wr32(m, esp + 0x10u, esi);
L_4ebf00:
    edi = rd32(m, ebx + 0x18u);
    eax = rd32(m, ebx);
    fR_4ebf05 = (esi & esi);
    if ((fR_4ebf05 == 0)) goto L_4ebf0d;
    fA_4ebf09 = esi;
    fB_4ebf09 = eax;
    if (fA_4ebf09 == fB_4ebf09) goto L_4ebf13;
L_4ebf0d:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ebf13u);
L_4ebf13:
    fA_4ebf13 = ebp;
    fB_4ebf13 = edi;
    if (fA_4ebf13 == fB_4ebf13) goto L_4ebf68;
    fR_4ebf17 = (esi & esi);
    if (!(fR_4ebf17 == 0)) goto L_4ebf64;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ebf21u);
L_4ebf21:
    fA_4ebf21 = ebp;
    fB_4ebf21 = rd32(m, esi + 0x18u);
    if (fA_4ebf21 != fB_4ebf21) goto L_4ebf2c;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ebf2cu);
L_4ebf2c:
    ecx = rd32(m, ebp + 0xcu);
    eax = rd32(m, esp + 0x38u);
    edx = rd32(m, eax + 0x24u);
    wr32(m, esp + 0x18u, ecx);
    ecx = (esp + 0x24u);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp + 0x20u, eax);
    esp -= 4; wr32(m, esp, eax);
    eax = (esp + 0x20u);
    wr32(m, esp + 0x28u, edx);
    LIFT_CALL(0x4f06c0u, 0x4ebf51u);  // FUN_004f06c0
    esi = (esp + 0x10u);
    LIFT_CALL(0x417df0u, 0x4ebf5au);  // FUN_00417df0
    ebp = rd32(m, esp + 0x14u);
    esi = rd32(m, esp + 0x10u);
    goto L_4ebf00;
L_4ebf64:
    esi = rd32(m, esi);
    goto L_4ebf21;
L_4ebf68:
    eax = rd32(m, esp + 0x38u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x24u;
    LIFT_RET(8);
}

// 004ebf80 Fn_004ebf80
//   original FUN_004ebf80; register inputs: edi; returns with ret 4
REFLEX_PORT_FN(0x004ebf80, Fn_004ebf80, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4ebf83 = 0;
    uint32_t fB_4ebf83 = 0;
    uint32_t fA_4ebf94 = 0;
    uint32_t fB_4ebf94 = 0;
    uint32_t fR_4ebfea = 0;
    uint32_t fA_4ebfee = 0;
    uint32_t fB_4ebfee = 0;
    uint32_t fA_4ebff8 = 0;
    uint32_t fB_4ebff8 = 0;
    uint32_t fR_4ebffc = 0;
    uint32_t fA_4ec008 = 0;
    uint32_t fB_4ec008 = 0;
    uint32_t fR_4ec022 = 0;
    uint32_t fR_4ec026 = 0;
    uint32_t fA_4ec030 = 0;
    uint32_t fB_4ec030 = 0;
    uint32_t fA_4ec06b = 0;
    uint32_t fB_4ec06b = 0;
    uint32_t fA_4ec0a4 = 0;
    uint32_t fB_4ec0a4 = 0;
    uint32_t fA_4ec0c2 = 0;
    uint32_t fB_4ec0c2 = 0;
    esp = esp - 0x34u;
    fA_4ebf83 = rd32(m, edi + 0x1cu);
    fB_4ebf83 = 0x0u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    if (fA_4ebf83 == fB_4ebf83) goto L_4ec0d6;
    eax = rd32(m, esp + 0x44u);
    fA_4ebf94 = rd32(m, eax + 0x1cu);
    fB_4ebf94 = 0x0u;
    if (fA_4ebf94 != fB_4ebf94) goto L_4ebfcb;
    ecx = rd32(m, edi + 0x18u);
    edx = rd32(m, ecx + 0x4u);
    esp -= 4; wr32(m, esp, edx);
    ecx = edi;
    LIFT_CALL(0x4f2020u, 0x4ebfa8u);  // FUN_004f2020
    eax = rd32(m, edi + 0x18u);
    wr32(m, eax + 0x4u, eax);
    eax = rd32(m, edi + 0x18u);
    wr32(m, edi + 0x1cu, 0x0u);
    wr32(m, eax, eax);
    eax = rd32(m, edi + 0x18u);
    wr32(m, eax + 0x8u, eax);
    eax = edi;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x34u;
    LIFT_RET(4);
L_4ebfcb:
    esi = (esp + 0x20u);
    LIFT_CALL(0x4f0630u, 0x4ebfd4u);  // FUN_004f0630
    eax = rd32(m, edi + 0x18u);
    eax = rd32(m, eax);
    esi = rd32(m, edi);
    ebp = eax;
    wr32(m, esp + 0xcu, esi);
    wr32(m, esp + 0x10u, ebp);
L_4ebfe5:
    ebx = rd32(m, edi + 0x18u);
    eax = rd32(m, edi);
    fR_4ebfea = (esi & esi);
    if ((fR_4ebfea == 0)) goto L_4ebff2;
    fA_4ebfee = esi;
    fB_4ebfee = eax;
    if (fA_4ebfee == fB_4ebfee) goto L_4ebff8;
L_4ebff2:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ebff8u);
L_4ebff8:
    fA_4ebff8 = ebp;
    fB_4ebff8 = ebx;
    if (fA_4ebff8 == fB_4ebff8) goto L_4ec067;
    fR_4ebffc = (esi & esi);
    if (!(fR_4ebffc == 0)) goto L_4ec05f;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ec006u);
    eax = 0u;
L_4ec008:
    fA_4ec008 = ebp;
    fB_4ec008 = rd32(m, eax + 0x18u);
    if (fA_4ec008 != fB_4ec008) goto L_4ec013;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ec013u);
L_4ec013:
    eax = rd32(m, ebp + 0xcu);
    ecx = rd32(m, esp + 0x44u);
    ebx = (ebp + 0xcu);
    LIFT_CALL(0x4dc660u, 0x4ec022u);  // FUN_004dc660
    fR_4ec022 = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_4ec022 == 0)) goto L_4ec04c;
    fR_4ec026 = (esi & esi);
    if (!(fR_4ec026 == 0)) goto L_4ec063;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ec030u);
L_4ec030:
    fA_4ec030 = ebp;
    fB_4ec030 = rd32(m, esi + 0x18u);
    if (fA_4ec030 != fB_4ec030) goto L_4ec03b;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ec03bu);
L_4ec03b:
    ecx = (esp + 0x14u);
    esp -= 4; wr32(m, esp, ecx);
    edx = (esp + 0x24u);
    esp -= 4; wr32(m, esp, edx);
    eax = ebx;
    LIFT_CALL(0x4f06c0u, 0x4ec04cu);  // FUN_004f06c0
L_4ec04c:
    esi = (esp + 0xcu);
    LIFT_CALL(0x417df0u, 0x4ec055u);  // FUN_00417df0
    ebp = rd32(m, esp + 0x10u);
    esi = rd32(m, esp + 0xcu);
    goto L_4ebfe5;
L_4ec05f:
    eax = rd32(m, esi);
    goto L_4ec008;
L_4ec063:
    esi = rd32(m, esi);
    goto L_4ec030;
L_4ec067:
    eax = (esp + 0x20u);
    fA_4ec06b = edi;
    fB_4ec06b = eax;
    if (fA_4ec06b == fB_4ec06b) goto L_4ec092;
    eax = rd32(m, edi + 0x18u);
    ecx = rd32(m, edi);
    edx = rd32(m, eax);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, ecx);
    ecx = (esp + 0x1cu);
    esp -= 4; wr32(m, esp, ecx);
    eax = edi;
    LIFT_CALL(0x4f1eb0u, 0x4ec086u);  // FUN_004f1eb0
    edx = (esp + 0x20u);
    esp -= 4; wr32(m, esp, edx);
    eax = edi;
    LIFT_CALL(0x4f1f90u, 0x4ec092u);  // FUN_004f1f90
L_4ec092:
    esi = (esp + 0x20u);
    LIFT_CALL(0x4f2220u, 0x4ec09bu);  // FUN_004f2220
    esi = rd32(m, esp + 0x20u);
    LIFT_CALL(0x4dc0f0u, 0x4ec0a4u);  // FUN_004dc0f0
    fA_4ec0a4 = rd8(m, 0xda67d8u);
    fB_4ec0a4 = 0x0u;
    if (fA_4ec0a4 == fB_4ec0a4) goto L_4ec0b8;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x4ec0b8u);
L_4ec0b8:
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(rd32(m, 0x916350u) /* 8.287536956934404e-39 */, 0x4ec0bfu);
    esp = esp + 0x4u;
    fA_4ec0c2 = rd8(m, 0xda67d8u);
    fB_4ec0c2 = 0x0u;
    if (fA_4ec0c2 == fB_4ec0c2) goto L_4ec0d6;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4ec0d6u);
L_4ec0d6:
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = edi;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x34u;
    LIFT_RET(4);
}

// 004ec2c0 Fn_004ec2c0
//   original FUN_004ec2c0; register inputs: none; returns with ret 12
REFLEX_PORT_FN(0x004ec2c0, Fn_004ec2c0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4ec2d4 = 0;
    uint32_t fB_4ec2d4 = 0;
    uint32_t fA_4ec2f1 = 0;
    uint32_t fB_4ec2f1 = 0;
    uint32_t fA_4ec307 = 0;
    uint32_t fB_4ec307 = 0;
    uint32_t fR_4ec358 = 0;
    uint32_t fA_4ec371 = 0;
    uint32_t fB_4ec371 = 0;
    uint32_t fA_4ec3a9 = 0;
    uint32_t fB_4ec3a9 = 0;
    uint32_t fR_4ec3d9 = 0;
    uint32_t fA_4ec3e4 = 0;
    uint32_t fB_4ec3e4 = 0;
    uint32_t fR_4ec3ea = 0;
    uint32_t fA_4ec3ee = 0;
    uint32_t fB_4ec3ee = 0;
    uint32_t fA_4ec407 = 0;
    uint32_t fB_4ec407 = 0;
    uint32_t fA_4ec42e = 0;
    uint32_t fB_4ec42e = 0;
    uint32_t fR_4ec43d = 0;
    uint32_t fA_4ec441 = 0;
    uint32_t fB_4ec441 = 0;
    uint32_t fR_4ec447 = 0;
    uint32_t fA_4ec44b = 0;
    uint32_t fB_4ec44b = 0;
    uint32_t fA_4ec45a = 0;
    uint32_t fB_4ec45a = 0;
    uint32_t fR_4ec480 = 0;
    uint32_t fA_4ec4b2 = 0;
    uint32_t fB_4ec4b2 = 0;
    uint32_t fA_4ec4c4 = 0;
    uint32_t fB_4ec4c4 = 0;
    esp = esp - 0x30u;
    eax = rd32(m, esp + 0x34u);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, eax + 0x20u);
    esp -= 4; wr32(m, esp, edi);
    edi = 0u;
    wr32(m, esp + 0x24u, esi);
    fA_4ec2d4 = esi;
    fB_4ec2d4 = edi;
    if (fA_4ec2d4 == fB_4ec2d4) goto L_4ec4ba;
    ecx = rd32(m, esi + 0x10u);
    ecx = ecx - rd32(m, esi + 0xcu);
    eax = 0x66666667u;
    { int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); }
    edx = uint32_t(int32_t(edx) >> 6);
    eax = edx;
    eax = (eax >> 31);
    fA_4ec2f1 = eax;
    fB_4ec2f1 = edx;
    eax = fA_4ec2f1 + fB_4ec2f1;
    wr32(m, esp + 0x18u, edi);
    if ((((fA_4ec2f1 + fB_4ec2f1)) == 0)) goto L_4ec4ba;
    wr32(m, esp + 0x1cu, edi);
    goto L_4ec307;
L_4ec303:
    esi = rd32(m, esp + 0x24u);
L_4ec307:
    fA_4ec307 = rd32(m, esp + 0x18u);
    fB_4ec307 = eax;
    if (fA_4ec307 < fB_4ec307) goto L_4ec313;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ec313u);
L_4ec313:
    edi = rd32(m, esi + 0xcu);
    edi = edi + rd32(m, esp + 0x1cu);
    eax = rd32(m, esp + 0x44u);
    ebx = rd32(m, eax + 0x24u);
    ecx = rd32(m, esp + 0x48u);
    eax = rd32(m, edi + 0x98u);
    edx = rd32(m, ecx);
    eax = eax - rd32(m, edi + 0x94u);
    ecx = rd32(m, esp + 0x4cu);
    esi = rd32(m, edi + 0x80u);
    wr32(m, esp + 0x28u, edx);
    edx = rd32(m, ecx);
    eax = uint32_t(int32_t(eax) >> 2);
    wr32(m, esp + 0x34u, edx);
    ebp = ebx;
    wr32(m, esp + 0x14u, esi);
    wr32(m, esp + 0x20u, 0x0u);
    fR_4ec358 = (eax & eax);
    if (!(false || (fR_4ec358 == 0))) goto L_4ec368;
    eax = (eax | 0xffffffffu);
    goto L_4ec43d;
L_4ec364:
    esi = rd32(m, esp + 0x14u);
L_4ec368:
    ecx = rd32(m, ebx + 0x14u);
    ecx = ecx - rd32(m, ebx + 0x10u);
    ecx = uint32_t(int32_t(ecx) >> 2);
    fA_4ec371 = esi;
    fB_4ec371 = ecx;
    if (fA_4ec371 < fB_4ec371) goto L_4ec37b;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ec37bu);
L_4ec37b:
    ebx = rd32(m, ebx + 0x10u);
    eax = rd32(m, esp + 0x28u);
    esi = esi + esi;
    esi = esi + esi;
    ebx = ebx + esi;
    esi = rd32(m, ebx);
    LIFT_CALL(0x4eb920u, 0x4ec38fu);  // FUN_004eb920
    edx = rd32(m, ebp + 0x14u);
    edx = edx - rd32(m, ebp + 0x10u);
    esi = rd32(m, esp + 0x14u);
    x0 = movsd_m(rd64(m, eax));
    ebx = rd32(m, eax + 0x8u);
    edx = uint32_t(int32_t(edx) >> 2);
    wr64(m, esp + 0x28u, x0.lo);
    fA_4ec3a9 = esi;
    fB_4ec3a9 = edx;
    if (fA_4ec3a9 < fB_4ec3a9) goto L_4ec3b3;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ec3b3u);
L_4ec3b3:
    ebp = rd32(m, ebp + 0x10u);
    eax = (esi * 4);
    esi = rd32(m, ebp + eax);
    ebp = ebp + eax;
    eax = rd32(m, esp + 0x34u);
    LIFT_CALL(0x4eb920u, 0x4ec3ccu);  // FUN_004eb920
    x0 = movsd_m(rd64(m, eax));
    ebp = rd32(m, eax + 0x8u);
    wr64(m, esp + 0x34u, x0.lo);
    fR_4ec3d9 = (ebx & ebx);
    if ((fR_4ec3d9 == 0)) goto L_4ec486;
    eax = (eax | 0xffffffffu);
    fA_4ec3e4 = rd32(m, esp + 0x28u);
    fB_4ec3e4 = eax;
    if (fA_4ec3e4 == fB_4ec3e4) goto L_4ec439;
    fR_4ec3ea = (ebp & ebp);
    if ((fR_4ec3ea == 0)) goto L_4ec439;
    fA_4ec3ee = rd32(m, esp + 0x34u);
    fB_4ec3ee = eax;
    if (fA_4ec3ee == fB_4ec3ee) goto L_4ec439;
    eax = rd32(m, edi + 0x98u);
    eax = eax - rd32(m, edi + 0x94u);
    esi = rd32(m, esp + 0x20u);
    eax = uint32_t(int32_t(eax) >> 2);
    fA_4ec407 = esi;
    fB_4ec407 = eax;
    if (fA_4ec407 < fB_4ec407) goto L_4ec411;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ec411u);
L_4ec411:
    ecx = rd32(m, edi + 0x94u);
    eax = rd32(m, edi + 0x98u);
    edx = rd32(m, ecx + esi * 4);
    eax = eax - ecx;
    esi = (esi + 1u);
    eax = uint32_t(int32_t(eax) >> 2);
    wr32(m, esp + 0x14u, edx);
    wr32(m, esp + 0x20u, esi);
    fA_4ec42e = esi;
    fB_4ec42e = eax;
    if (fA_4ec42e < fB_4ec42e) goto L_4ec364;
    eax = (eax | 0xffffffffu);
L_4ec439:
    esi = rd32(m, esp + 0x14u);
L_4ec43d:
    fR_4ec43d = (ebx & ebx);
    if ((fR_4ec43d == 0)) goto L_4ec486;
    fA_4ec441 = rd32(m, esp + 0x28u);
    fB_4ec441 = eax;
    if (fA_4ec441 == fB_4ec441) goto L_4ec486;
    fR_4ec447 = (ebp & ebp);
    if ((fR_4ec447 == 0)) goto L_4ec486;
    fA_4ec44b = rd32(m, esp + 0x34u);
    fB_4ec44b = eax;
    if (fA_4ec44b == fB_4ec44b) goto L_4ec486;
    ecx = rd32(m, ebx + 0x14u);
    ecx = ecx - rd32(m, ebx + 0x10u);
    ecx = uint32_t(int32_t(ecx) >> 2);
    fA_4ec45a = esi;
    fB_4ec45a = ecx;
    if (fA_4ec45a < fB_4ec45a) goto L_4ec464;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ec464u);
L_4ec464:
    eax = rd32(m, esp + 0x28u);
    edx = rd32(m, ebx + 0x10u);
    ecx = rd32(m, edi + 0x84u);
    esi = rd32(m, edx + esi * 4);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, esp + 0x38u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x4ebac0u, 0x4ec480u);  // FUN_004ebac0
    fR_4ec480 = (eax & eax);
    if ((sgn(fR_4ec480, 4) != false)) goto L_4ec4d4;
    if ((!(fR_4ec480 == 0) && sgn(fR_4ec480, 4) == false)) goto L_4ec4e0;
L_4ec486:
    eax = rd32(m, esp + 0x24u);
    ecx = rd32(m, eax + 0x10u);
    ecx = ecx - rd32(m, eax + 0xcu);
    esi = rd32(m, esp + 0x18u);
    wr32(m, esp + 0x1cu, rd32(m, esp + 0x1cu) + 0xa0u);
    eax = 0x66666667u;
    { int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); }
    edx = uint32_t(int32_t(edx) >> 6);
    eax = edx;
    eax = (eax >> 31);
    esi = (esi + 1u);
    eax = eax + edx;
    wr32(m, esp + 0x18u, esi);
    fA_4ec4b2 = esi;
    fB_4ec4b2 = eax;
    if (fA_4ec4b2 < fB_4ec4b2) goto L_4ec303;
L_4ec4ba:
    edx = rd32(m, esp + 0x48u);
    eax = rd32(m, edx);
    ecx = rd32(m, esp + 0x4cu);
    fA_4ec4c4 = eax;
    fB_4ec4c4 = rd32(m, ecx);
    eax = eax - eax - (fA_4ec4c4 < fB_4ec4c4 ? 1u : 0u);
    eax = (0u - eax);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x30u;
    LIFT_RET(12);
L_4ec4d4:
    eax = (eax & 0xffffff00u) | ((0x1u) & 0xffu);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x30u;
    LIFT_RET(12);
L_4ec4e0:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = (eax & 0xffffff00u) | ((0u) & 0xffu);
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x30u;
    LIFT_RET(12);
}

// 004ec950 Fn_004ec950
//   original FUN_004ec950; register inputs: eax; returns with ret 8
REFLEX_PORT_FN(0x004ec950, Fn_004ec950, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4ec96a = 0;
    uint32_t fB_4ec96a = 0;
    uint32_t fR_4ec9ae = 0;
    uint32_t fA_4ec9bb = 0;
    uint32_t fB_4ec9bb = 0;
    uint32_t fA_4ec9da = 0;
    uint32_t fB_4ec9da = 0;
    uint32_t fA_4ec9fd = 0;
    uint32_t fB_4ec9fd = 0;
    uint32_t fA_4eca0d = 0;
    uint32_t fB_4eca0d = 0;
    uint32_t fA_4eca3e = 0;
    uint32_t fB_4eca3e = 0;
    uint32_t fA_4eca62 = 0;
    uint32_t fB_4eca62 = 0;
    uint32_t fA_4ecab0 = 0;
    uint32_t fB_4ecab0 = 0;
    uint32_t fA_4ecad4 = 0;
    uint32_t fB_4ecad4 = 0;
    uint32_t fA_4ecaef = 0;
    uint32_t fB_4ecaef = 0;
    uint32_t fA_4ecaff = 0;
    uint32_t fB_4ecaff = 0;
    uint32_t fA_4ecb1f = 0;
    uint32_t fB_4ecb1f = 0;
    uint32_t fA_4ecb2f = 0;
    uint32_t fB_4ecb2f = 0;
    uint32_t fA_4ecb41 = 0;
    uint32_t fB_4ecb41 = 0;
    uint32_t fA_4ecb61 = 0;
    uint32_t fB_4ecb61 = 0;
    uint32_t fA_4ecb7b = 0;
    uint32_t fB_4ecb7b = 0;
    uint32_t fA_4ecb8b = 0;
    uint32_t fB_4ecb8b = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0x2cu;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, 0x4u);
    esi = eax;
    LIFT_CALL(0x78fbe0u, 0x4ec965u);  // ReflexAlignedAlloc
    ebx = 0u;
    esp = esp + 0x4u;
    fA_4ec96a = eax;
    fB_4ec96a = ebx;
    if (fA_4ec96a == fB_4ec96a) goto L_4ec97a;
    ecx = (esp + 0x20u);
    wr32(m, eax, ecx);
    wr32(m, esp + 0x20u, eax);
    goto L_4ec97e;
L_4ec97a:
    wr32(m, esp + 0x20u, ebx);
L_4ec97e:
    edx = rd32(m, ebp + 0xcu);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, esi);
    ecx = (esp + 0x28u);
    wr32(m, esp + 0x34u, ebx);
    wr32(m, esp + 0x38u, ebx);
    wr32(m, esp + 0x3cu, ebx);
    LIFT_CALL(0x4ed530u, 0x4ec998u);  // FUN_004ed530
    eax = esi;
    LIFT_CALL(0x4021d0u, 0x4ec99fu);  // FUN_004021d0
    wr32(m, esp + 0x18u, eax);
    eax = rd32(m, esi + 0x14u);
    eax = eax - rd32(m, esi + 0x10u);
    edi = 0u;
    eax = uint32_t(int32_t(eax) >> 2);
    fR_4ec9ae = (eax & eax);
    if ((false || (fR_4ec9ae == 0))) goto L_4ec9de;
L_4ec9b2:
    ecx = rd32(m, esi + 0x14u);
    ecx = ecx - rd32(m, esi + 0x10u);
    ecx = uint32_t(int32_t(ecx) >> 2);
    fA_4ec9bb = edi;
    fB_4ec9bb = ecx;
    if (fA_4ec9bb < fB_4ec9bb) goto L_4ec9c5;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ec9c5u);
L_4ec9c5:
    edx = rd32(m, esi + 0x10u);
    eax = rd32(m, edx + edi * 4);
    LIFT_CALL(0x4eb4e0u, 0x4ec9d0u);  // FUN_004eb4e0
    eax = rd32(m, esi + 0x14u);
    eax = eax - rd32(m, esi + 0x10u);
    edi = (edi + 1u);
    eax = uint32_t(int32_t(eax) >> 2);
    fA_4ec9da = edi;
    fB_4ec9da = eax;
    if (fA_4ec9da < fB_4ec9da) goto L_4ec9b2;
L_4ec9de:
    edi = rd32(m, esp + 0x2cu);
    wr32(m, esp + 0x10u, ebx);
    ebx = rd32(m, esp + 0x30u);
    ecx = ebx;
    ecx = ecx - edi;
    eax = 0x2e8ba2e9u;
    { int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); }
    edx = uint32_t(int32_t(edx) >> 5);
    eax = edx;
    eax = (eax >> 31);
    fA_4ec9fd = eax;
    fB_4ec9fd = edx;
    eax = fA_4ec9fd + fB_4ec9fd;
    if ((((fA_4ec9fd + fB_4ec9fd)) == 0)) goto L_4ecaba;
    wr32(m, esp + 0x14u, 0x0u);
    fA_4eca0d = rd32(m, esp + 0x10u);
    fB_4eca0d = eax;
    if (fA_4eca0d < fB_4eca0d) goto L_4eca21;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4eca19u);
    ebx = rd32(m, esp + 0x30u);
    edi = rd32(m, esp + 0x2cu);
L_4eca21:
    ecx = rd32(m, esp + 0x14u);
    ebx = ebx - edi;
    eax = 0x2e8ba2e9u;
    { int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ebx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); }
    edx = uint32_t(int32_t(edx) >> 5);
    eax = edx;
    eax = (eax >> 31);
    ecx = ecx + edi;
    eax = eax + edx;
    wr32(m, esp + 0x1cu, ecx);
    fA_4eca3e = rd32(m, esp + 0x10u);
    fB_4eca3e = eax;
    if (fA_4eca3e < fB_4eca3e) goto L_4eca4e;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4eca4au);
    edi = rd32(m, esp + 0x2cu);
L_4eca4e:
    edx = rd32(m, esi + 0x14u);
    edx = edx - rd32(m, esi + 0x10u);
    ecx = rd32(m, esp + 0x14u);
    edi = rd32(m, ecx + edi + 0x80u);
    edx = uint32_t(int32_t(edx) >> 2);
    fA_4eca62 = edi;
    fB_4eca62 = edx;
    if (fA_4eca62 < fB_4eca62) goto L_4eca6c;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4eca6cu);
L_4eca6c:
    eax = rd32(m, esp + 0x1cu);
    ecx = rd32(m, esp + 0x18u);
    edx = rd32(m, esi + 0x10u);
    eax = eax + 0x88u;
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, edx + edi * 4);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x4dc710u, 0x4eca87u);  // FUN_004dc710
    ebx = rd32(m, esp + 0x30u);
    edi = rd32(m, esp + 0x2cu);
    wr32(m, esp + 0x10u, (rd32(m, esp + 0x10u) + 1u));
    wr32(m, esp + 0x14u, rd32(m, esp + 0x14u) + 0xb0u);
    ecx = ebx;
    ecx = ecx - edi;
    eax = 0x2e8ba2e9u;
    { int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); }
    edx = uint32_t(int32_t(edx) >> 5);
    eax = edx;
    eax = (eax >> 31);
    eax = eax + edx;
    fA_4ecab0 = rd32(m, esp + 0x10u);
    fB_4ecab0 = eax;
    if (fA_4ecab0 < fB_4ecab0) goto L_4eca21;
L_4ecaba:
    eax = rd32(m, ebp + 0x8u);
    ecx = rd32(m, esp + 0x18u);
    wr32(m, esi + 0x68u, (rd32(m, esi + 0x68u) + 1u));
    wr32(m, eax, ecx);
    ecx = 0u;
    wr32(m, eax + 0x8u, esi);
    esi = rd32(m, 0x91600cu) /* 8.29485173491818e-39 */;
    wr32(m, eax + 0x4u, ecx);
    fA_4ecad4 = edi;
    fB_4ecad4 = ecx;
    if (fA_4ecad4 == fB_4ecad4) goto L_4ecba7;
    edx = rd32(m, esp + 0x1cu);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x41ac70u, 0x4ecae8u);  // FUN_0041ac70
    edi = rd32(m, esp + 0x38u);
    esp = esp + 0xcu;
    fA_4ecaef = rd8(m, 0xda67d8u);
    fB_4ecaef = 0x0u;
    if (fA_4ecaef == fB_4ecaef) goto L_4ecb0f;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(esi, 0x4ecaffu);
    fA_4ecaff = rd8(m, 0xda67d8u);
    fB_4ecaff = 0x0u;
    if (fA_4ecaff == fB_4ecaff) goto L_4ecb0f;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(esi, 0x4ecb0fu);
L_4ecb0f:
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(rd32(m, 0x916350u) /* 8.287536956934404e-39 */, 0x4ecb16u);
    edi = rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */;
    esp = esp + 0x4u;
    fA_4ecb1f = rd8(m, 0xda67d8u);
    fB_4ecb1f = 0x0u;
    if (fA_4ecb1f == fB_4ecb1f) goto L_4ecb3f;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(edi, 0x4ecb2fu);
    fA_4ecb2f = rd8(m, 0xda67d8u);
    fB_4ecb2f = 0x0u;
    if (fA_4ecb2f == fB_4ecb2f) goto L_4ecb3f;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(edi, 0x4ecb3fu);
L_4ecb3f:
    ecx = 0u;
L_4ecb41:
    fA_4ecb41 = rd8(m, 0xda67d8u);
    fB_4ecb41 = 0x0u;
    ebx = rd32(m, esp + 0x20u);
    wr32(m, esp + 0x2cu, ecx);
    wr32(m, esp + 0x30u, ecx);
    wr32(m, esp + 0x34u, ecx);
    if (fA_4ecb41 == fB_4ecb41) goto L_4ecb71;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(esi, 0x4ecb61u);
    fA_4ecb61 = rd8(m, 0xda67d8u);
    fB_4ecb61 = 0x0u;
    if (fA_4ecb61 == fB_4ecb61) goto L_4ecb71;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(esi, 0x4ecb71u);
L_4ecb71:
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(rd32(m, 0x916350u) /* 8.287536956934404e-39 */, 0x4ecb78u);
    esp = esp + 0x4u;
    fA_4ecb7b = rd8(m, 0xda67d8u);
    fB_4ecb7b = 0x0u;
    if (fA_4ecb7b == fB_4ecb7b) goto L_4ecb9b;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(edi, 0x4ecb8bu);
    fA_4ecb8b = rd8(m, 0xda67d8u);
    fB_4ecb8b = 0x0u;
    if (fA_4ecb8b == fB_4ecb8b) goto L_4ecb9b;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(edi, 0x4ecb9bu);
L_4ecb9b:
    eax = rd32(m, ebp + 0x8u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(8);
L_4ecba7:
    edi = rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */;
    goto L_4ecb41;
}

// 004ed2f0 Fn_004ed2f0
//   original FUN_004ed2f0; register inputs: ecx eax; returns with ret 4
REFLEX_PORT_FN(0x004ed2f0, Fn_004ed2f0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4ed306 = 0;
    uint32_t fB_4ed306 = 0;
    uint32_t fA_4ed34c = 0;
    uint32_t fB_4ed34c = 0;
    uint32_t fA_4ed372 = 0;
    uint32_t fB_4ed372 = 0;
    uint32_t fA_4ed395 = 0;
    uint32_t fB_4ed395 = 0;
    uint32_t fA_4ed3b2 = 0;
    uint32_t fB_4ed3b2 = 0;
    uint32_t fA_4ed3c9 = 0;
    uint32_t fB_4ed3c9 = 0;
    esp = esp - 0x44u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, 0x4u);
    edi = eax;
    esi = ecx;
    LIFT_CALL(0x78fbe0u, 0x4ed301u);  // ReflexAlignedAlloc
    ebx = 0u;
    esp = esp + 0x4u;
    fA_4ed306 = eax;
    fB_4ed306 = ebx;
    if (fA_4ed306 == fB_4ed306) goto L_4ed316;
    ecx = (esp + 0x10u);
    wr32(m, eax, ecx);
    wr32(m, esp + 0x10u, eax);
    goto L_4ed31a;
L_4ed316:
    wr32(m, esp + 0x10u, ebx);
L_4ed31a:
    edx = rd32(m, esp + 0x54u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, esi);
    ecx = (esp + 0x18u);
    wr32(m, esp + 0x24u, ebx);
    wr32(m, esp + 0x28u, ebx);
    wr32(m, esp + 0x2cu, ebx);
    LIFT_CALL(0x4ed530u, 0x4ed335u);  // FUN_004ed530
    ecx = rd32(m, esp + 0x20u);
    ecx = ecx - rd32(m, esp + 0x1cu);
    eax = 0x2e8ba2e9u;
    { int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); }
    edx = uint32_t(int32_t(edx) >> 5);
    eax = edx;
    eax = (eax >> 31);
    fA_4ed34c = eax;
    fB_4ed34c = edx;
    eax = fA_4ed34c + fB_4ed34c;
    if ((((fA_4ed34c + fB_4ed34c)) == 0)) goto L_4ed35e;
    esp -= 4; wr32(m, esp, edi);
    ecx = (esp + 0x14u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x4eddc0u, 0x4ed35cu);  // FUN_004eddc0
    goto L_4ed3c5;
L_4ed35e:
    esp -= 4; wr32(m, esp, esi);
    esi = (esp + 0x2cu);
    LIFT_CALL(0x4dc570u, 0x4ed368u);  // FUN_004dc570
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x4ebe60u, 0x4ed36eu);  // FUN_004ebe60
    esi = rd32(m, esp + 0x48u);
    fA_4ed372 = esi;
    fB_4ed372 = ebx;
    if (fA_4ed372 == fB_4ed372) goto L_4ed37f;
    LIFT_CALL(0x4ebe10u, 0x4ed37bu);  // FUN_004ebe10
    wr32(m, esp + 0x48u, ebx);
L_4ed37f:
    esi = (esp + 0x28u);
    wr32(m, esp + 0x4cu, ebx);
    LIFT_CALL(0x4f2220u, 0x4ed38cu);  // FUN_004f2220
    esi = rd32(m, esp + 0x28u);
    LIFT_CALL(0x4dc0f0u, 0x4ed395u);  // FUN_004dc0f0
    fA_4ed395 = rd8(m, 0xda67d8u);
    fB_4ed395 = (ebx & 0xffu);
    if (fA_4ed395 == fB_4ed395) goto L_4ed3a8;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x4ed3a8u);
L_4ed3a8:
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(rd32(m, 0x916350u) /* 8.287536956934404e-39 */, 0x4ed3afu);
    esp = esp + 0x4u;
    fA_4ed3b2 = rd8(m, 0xda67d8u);
    fB_4ed3b2 = (ebx & 0xffu);
    if (fA_4ed3b2 == fB_4ed3b2) goto L_4ed3c5;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4ed3c5u);
L_4ed3c5:
    eax = rd32(m, esp + 0x1cu);
    fA_4ed3c9 = eax;
    fB_4ed3c9 = ebx;
    if (fA_4ed3c9 == fB_4ed3c9) goto L_4ed3ed;
    edx = rd32(m, esp + 0x54u);
    ecx = rd32(m, esp + 0x20u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x41ac70u, 0x4ed3ddu);  // FUN_0041ac70
    edx = rd32(m, esp + 0x28u);
    esp = esp + 0xcu;
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x78fc50u, 0x4ed3eau);  // ReflexAlignedFree
    esp = esp + 0x4u;
L_4ed3ed:
    eax = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0x20u, ebx);
    wr32(m, esp + 0x24u, ebx);
    wr32(m, esp + 0x28u, ebx);
    LIFT_CALL(0x78fc50u, 0x4ed403u);  // ReflexAlignedFree
    esp = esp + 0x4u;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x44u;
    LIFT_RET(4);
}

// 004ed410 Fn_004ed410
//   original FUN_004ed410; register inputs: none; returns with ret 12
REFLEX_PORT_FN(0x004ed410, Fn_004ed410, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4ed422 = 0;
    uint32_t fB_4ed422 = 0;
    uint32_t fA_4ed475 = 0;
    uint32_t fB_4ed475 = 0;
    uint32_t fA_4ed48b = 0;
    uint32_t fB_4ed48b = 0;
    uint32_t fA_4ed493 = 0;
    uint32_t fB_4ed493 = 0;
    uint32_t fA_4ed4ca = 0;
    uint32_t fB_4ed4ca = 0;
    uint32_t fA_4ed4ed = 0;
    uint32_t fB_4ed4ed = 0;
    uint32_t fA_4ed50a = 0;
    uint32_t fB_4ed50a = 0;
    esp = esp - 0x28u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x3cu);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x4f4a30u, 0x4ed420u);  // FUN_004f4a30
    ebx = 0u;
    fA_4ed422 = eax;
    fB_4ed422 = ebx;
    if (fA_4ed422 == fB_4ed422) goto L_4ed432;
    ecx = (esp + 0x10u);
    wr32(m, eax, ecx);
    wr32(m, esp + 0x10u, eax);
    goto L_4ed436;
L_4ed432:
    wr32(m, esp + 0x10u, ebx);
L_4ed436:
    LIFT_CALL(0x4f4ea0u, 0x4ed43bu);  // FUN_004f4ea0
    edx = rd32(m, esp + 0x40u);
    ecx = rd32(m, esp + 0x3cu);
    wr32(m, esp + 0x28u, eax);
    wr8(m, eax + 0x19u, 0x1u);
    eax = rd32(m, esp + 0x28u);
    wr32(m, eax + 0x4u, eax);
    eax = rd32(m, esp + 0x28u);
    wr32(m, eax, eax);
    eax = rd32(m, esp + 0x28u);
    wr32(m, eax + 0x8u, eax);
    esp -= 4; wr32(m, esp, edx);
    eax = (esp + 0x14u);
    wr32(m, esp + 0x30u, ebx);
    wr32(m, esp + 0x34u, ebx);
    wr32(m, esp + 0x38u, ebx);
    LIFT_CALL(0x4ed2f0u, 0x4ed475u);  // FUN_004ed2f0
    fA_4ed475 = rd32(m, esp + 0x2cu);
    fB_4ed475 = ebx;
    if (fA_4ed475 == fB_4ed475) goto L_4ed4b0;
    eax = rd32(m, esp + 0x28u);
    esi = rd32(m, eax);
    eax = rd32(m, esp + 0x10u);
    edi = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    fA_4ed48b = eax;
    fB_4ed48b = ebx;
    if (fA_4ed48b != fB_4ed48b) goto L_4ed4ac;
    LIFT_CALL(edi, 0x4ed491u);
    eax = 0u;
L_4ed493:
    fA_4ed493 = esi;
    fB_4ed493 = rd32(m, eax + 0x18u);
    if (fA_4ed493 != fB_4ed493) goto L_4ed49a;
    LIFT_CALL(edi, 0x4ed49au);
L_4ed49a:
    x0 = movsd_m(rd64(m, esi + 0xcu));
    wr64(m, ebp, x0.lo);
    ecx = rd32(m, esi + 0x14u);
    wr32(m, ebp + 0x8u, ecx);
    goto L_4ed4c6;
L_4ed4ac:
    eax = rd32(m, eax);
    goto L_4ed493;
L_4ed4b0:
    x0 = movsd_m(rd64(m, 0x9c9ff4u));
    wr64(m, ebp, x0.lo);
    edx = rd32(m, 0x9c9ffcu);
    wr32(m, ebp + 0x8u, edx);
L_4ed4c6:
    esi = rd32(m, esp + 0x30u);
    fA_4ed4ca = esi;
    fB_4ed4ca = ebx;
    if (fA_4ed4ca == fB_4ed4ca) goto L_4ed4d7;
    LIFT_CALL(0x4ebe10u, 0x4ed4d3u);  // FUN_004ebe10
    wr32(m, esp + 0x30u, ebx);
L_4ed4d7:
    esi = (esp + 0x10u);
    wr32(m, esp + 0x34u, ebx);
    LIFT_CALL(0x4f2220u, 0x4ed4e4u);  // FUN_004f2220
    esi = rd32(m, esp + 0x10u);
    LIFT_CALL(0x4dc0f0u, 0x4ed4edu);  // FUN_004dc0f0
    fA_4ed4ed = rd8(m, 0xda67d8u);
    fB_4ed4ed = (ebx & 0xffu);
    if (fA_4ed4ed == fB_4ed4ed) goto L_4ed500;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x4ed500u);
L_4ed500:
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(rd32(m, 0x916350u) /* 8.287536956934404e-39 */, 0x4ed507u);
    esp = esp + 0x4u;
    fA_4ed50a = rd8(m, 0xda67d8u);
    fB_4ed50a = (ebx & 0xffu);
    if (fA_4ed50a == fB_4ed50a) goto L_4ed51d;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4ed51du);
L_4ed51d:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x28u;
    LIFT_RET(12);
}

// 004ed530 Fn_004ed530
//   original FUN_004ed530; register inputs: ecx; returns with ret 8
REFLEX_PORT_FN(0x004ed530, Fn_004ed530, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4ed567 = 0;
    uint32_t fB_4ed567 = 0;
    uint32_t fA_4ed57b = 0;
    uint32_t fB_4ed57b = 0;
    uint32_t fA_4ed5bf = 0;
    uint32_t fB_4ed5bf = 0;
    uint32_t fA_4ed5d8 = 0;
    uint32_t fB_4ed5d8 = 0;
    uint32_t fA_4ed5e8 = 0;
    uint32_t fB_4ed5e8 = 0;
    uint32_t fA_4ed5ec = 0;
    uint32_t fB_4ed5ec = 0;
    uint32_t fA_4ed5f6 = 0;
    uint32_t fB_4ed5f6 = 0;
    uint32_t fA_4ed600 = 0;
    uint32_t fB_4ed600 = 0;
    uint32_t fA_4ed614 = 0;
    uint32_t fB_4ed614 = 0;
    uint32_t fA_4ed673 = 0;
    uint32_t fB_4ed673 = 0;
    uint32_t fA_4ed694 = 0;
    uint32_t fB_4ed694 = 0;
    uint32_t fR_4ed6eb = 0;
    uint32_t fA_4ed734 = 0;
    uint32_t fB_4ed734 = 0;
    uint32_t fR_4ed763 = 0;
    uint32_t fA_4ed78c = 0;
    uint32_t fB_4ed78c = 0;
    uint32_t fA_4ed7a0 = 0;
    uint32_t fB_4ed7a0 = 0;
    uint32_t fR_4ed7fe = 0;
    uint32_t fA_4ed80f = 0;
    uint32_t fB_4ed80f = 0;
    uint32_t fA_4ed861 = 0;
    uint32_t fB_4ed861 = 0;
    uint32_t fA_4ed87f = 0;
    uint32_t fB_4ed87f = 0;
    uint32_t fA_4ed8b8 = 0;
    uint32_t fB_4ed8b8 = 0;
    uint32_t fA_4ed8bd = 0;
    uint32_t fB_4ed8bd = 0;
    uint32_t fA_4ed8d3 = 0;
    uint32_t fB_4ed8d3 = 0;
    uint32_t fA_4ed903 = 0;
    uint32_t fB_4ed903 = 0;
    uint32_t fA_4ed92e = 0;
    uint32_t fB_4ed92e = 0;
    uint32_t fA_4ed94c = 0;
    uint32_t fB_4ed94c = 0;
    uint32_t fA_4ed964 = 0;
    uint32_t fB_4ed964 = 0;
    uint32_t fA_4ed974 = 0;
    uint32_t fB_4ed974 = 0;
    esp = esp - 0x8e4u;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp + 0x8dcu, eax);
    eax = rd32(m, esp + 0x8e8u);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x8f4u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = ecx;
    esi = rd32(m, edi + 0x10u);
    wr32(m, esp + 0x24u, eax);
    wr32(m, esp + 0x28u, ebp);
    wr32(m, esp + 0x1cu, edi);
    fA_4ed567 = rd32(m, edi + 0xcu);
    fB_4ed567 = esi;
    if (fA_4ed567 <= fB_4ed567) goto L_4ed572;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ed572u);
L_4ed572:
    ebx = rd32(m, edi);
    edi = rd32(m, edi + 0xcu);
    ecx = rd32(m, esp + 0x1cu);
    fA_4ed57b = edi;
    fB_4ed57b = rd32(m, ecx + 0x10u);
    if (fA_4ed57b <= fB_4ed57b) goto L_4ed58a;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ed586u);
    ecx = rd32(m, esp + 0x1cu);
L_4ed58a:
    eax = rd32(m, ecx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    edi = (esp + 0x24u);
    esi = ecx;
    LIFT_CALL(0x4f2c40u, 0x4ed59bu);  // FUN_004f2c40
    ecx = rd32(m, ebp + 0x10u);
    ecx = ecx - rd32(m, ebp + 0xcu);
    eax = 0x2e8ba2e9u;
    { int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); }
    edx = uint32_t(int32_t(edx) >> 5);
    ecx = edx;
    ecx = (ecx >> 31);
    ecx = ecx + edx;
    esp -= 4; wr32(m, esp, ecx);
    ecx = rd32(m, esp + 0x20u);
    LIFT_CALL(0x4f1640u, 0x4ed5bcu);  // FUN_004f1640
    esi = rd32(m, ebp + 0xcu);
    fA_4ed5bf = esi;
    fB_4ed5bf = rd32(m, ebp + 0x10u);
    if (fA_4ed5bf <= fB_4ed5bf) goto L_4ed5ca;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ed5cau);
L_4ed5ca:
    edi = rd32(m, ebp);
    wr32(m, esp + 0x14u, edi);
    wr32(m, esp + 0x18u, esi);
L_4ed5d5:
    esi = rd32(m, ebp + 0x10u);
    fA_4ed5d8 = rd32(m, ebp + 0xcu);
    fB_4ed5d8 = esi;
    if (fA_4ed5d8 <= fB_4ed5d8) goto L_4ed5e3;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ed5e3u);
L_4ed5e3:
    eax = rd32(m, ebp);
    ebx = 0u;
    fA_4ed5e8 = edi;
    fB_4ed5e8 = ebx;
    if (fA_4ed5e8 == fB_4ed5e8) goto L_4ed5f0;
    fA_4ed5ec = edi;
    fB_4ed5ec = eax;
    if (fA_4ed5ec == fB_4ed5ec) goto L_4ed5f6;
L_4ed5f0:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ed5f6u);
L_4ed5f6:
    fA_4ed5f6 = rd32(m, esp + 0x18u);
    fB_4ed5f6 = esi;
    if (fA_4ed5f6 == fB_4ed5f6) goto L_4ed994;
    fA_4ed600 = edi;
    fB_4ed600 = ebx;
    if (fA_4ed600 != fB_4ed600) goto L_4ed76b;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ed60eu);
    eax = 0u;
L_4ed610:
    edx = rd32(m, esp + 0x18u);
    fA_4ed614 = edx;
    fB_4ed614 = rd32(m, eax + 0x10u);
    if (fA_4ed614 < fB_4ed614) goto L_4ed61f;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ed61fu);
L_4ed61f:
    esi = rd32(m, esp + 0x18u);
    ecx = 0x20u;
    edi = (esp + 0x38u);
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    esi = rd32(m, esp + 0x18u);
    eax = rd32(m, esi + 0x80u);
    ecx = rd32(m, esi + 0x84u);
    edx = (esp + 0xc0u);
    edi = (esi + 0x88u);
    esp -= 4; wr32(m, esp, edx);
    wr32(m, esp + 0xbcu, eax);
    wr32(m, esp + 0xc0u, ecx);
    LIFT_CALL(0x4eee60u, 0x4ed65fu);  // FUN_004eee60
    esi = esi + 0x98u;
    esp -= 4; wr32(m, esp, esi);
    eax = (esp + 0xd4u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x418800u, 0x4ed673u);  // FUN_00418800
    fA_4ed673 = rd32(m, esp + 0xb8u);
    fB_4ed673 = ebx;
    if (int32_t(sext(fA_4ed673, 4)) >= int32_t(sext(fB_4ed673, 4))) goto L_4ed86a;
    edi = rd32(m, 0x916248u) /* 8.28654764021859e-39 */;
    ecx = (esp + 0x38u);
    esp -= 4; wr32(m, esp, 0x2eu);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(edi, 0x4ed68fu);
    esi = eax;
    esp = esp + 0x8u;
    fA_4ed694 = esi;
    fB_4ed694 = ebx;
    if (fA_4ed694 == fB_4ed694) goto L_4ed84c;
    esp -= 4; wr32(m, esp, 0x3ffu);
    edx = (esi + 0x1u);
    esp -= 4; wr32(m, esp, edx);
    eax = (esp + 0xf0u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x916260u) /* 8.286626112932593e-39 */, 0x4ed6b3u);
    ebp = rd32(m, esp + 0x30u);
    esp = esp + 0xcu;
    ecx = (esp + 0x38u);
    wr8(m, esp + 0x4e7u, 0x0u);
    esp -= 4; wr32(m, esp, ecx);
    eax = ebp;
    wr8(m, esi, 0x0u);
    LIFT_CALL(0x402160u, 0x4ed6d1u);  // Table_ColumnIndex
    edx = (esp + 0xe8u);
    esp -= 4; wr32(m, esp, 0x2eu);
    esp -= 4; wr32(m, esp, edx);
    wr32(m, esp + 0xc0u, eax);
    ebx = eax;
    LIFT_CALL(edi, 0x4ed6e6u);
    edi = eax;
    esp = esp + 0x8u;
    fR_4ed6eb = (edi & edi);
    if ((fR_4ed6eb == 0)) goto L_4ed806;
    eax = (esp + 0x4e8u);
    eax = (eax - 1u);
    wr32(m, esp + 0x2cu, eax);
L_4ed6ff:
    eax = (esp + 0xe8u);
    esi = edi;
    esi = esi - eax;
    esi = (esi + 1u);
    ecx = (esi - 0x1u);
    esp -= 4; wr32(m, esp, ecx);
    edx = eax;
    esp -= 4; wr32(m, esp, edx);
    eax = (esp + 0x4f0u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x916260u) /* 8.286626112932593e-39 */, 0x4ed720u);
    ecx = rd32(m, esp + 0x38u);
    wr8(m, ecx + esi, 0x0u);
    edx = rd32(m, ebp + 0x14u);
    edx = edx - rd32(m, ebp + 0x10u);
    esp = esp + 0xcu;
    edx = uint32_t(int32_t(edx) >> 2);
    fA_4ed734 = ebx;
    fB_4ed734 = edx;
    if (fA_4ed734 < fB_4ed734) goto L_4ed73e;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ed73eu);
L_4ed73e:
    eax = rd32(m, ebp + 0x10u);
    ecx = rd32(m, eax + ebx * 4);
    ebp = rd32(m, ecx + 0x24u);
    edx = (esp + 0x4e8u);
    esp -= 4; wr32(m, esp, edx);
    eax = ebp;
    LIFT_CALL(0x402160u, 0x4ed756u);  // Table_ColumnIndex
    edx = rd32(m, esp + 0xdcu);
    ebx = eax;
    wr32(m, esp + 0x20u, ebx);
    fR_4ed763 = (edx & edx);
    if (!(fR_4ed763 == 0)) goto L_4ed772;
    ecx = 0u;
    goto L_4ed77e;
L_4ed76b:
    eax = rd32(m, edi);
    goto L_4ed610;
L_4ed772:
    ecx = rd32(m, esp + 0xe4u);
    ecx = ecx - edx;
    ecx = uint32_t(int32_t(ecx) >> 2);
L_4ed77e:
    eax = rd32(m, esp + 0xe0u);
    esi = eax;
    esi = esi - edx;
    esi = uint32_t(int32_t(esi) >> 2);
    fA_4ed78c = esi;
    fB_4ed78c = ecx;
    if (fA_4ed78c >= fB_4ed78c) goto L_4ed79e;
    wr32(m, eax, ebx);
    eax = eax + 0x4u;
    wr32(m, esp + 0xe0u, eax);
    goto L_4ed7c9;
L_4ed79e:
    esi = eax;
    fA_4ed7a0 = edx;
    fB_4ed7a0 = eax;
    if (fA_4ed7a0 <= fB_4ed7a0) goto L_4ed7aa;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ed7aau);
L_4ed7aa:
    eax = rd32(m, esp + 0xd0u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, eax);
    eax = (esp + 0x28u);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0x3cu);
    esp -= 4; wr32(m, esp, ecx);
    eax = (esp + 0xe0u);
    LIFT_CALL(0x4f1b60u, 0x4ed7c9u);  // FUN_004f1b60
L_4ed7c9:
    esp -= 4; wr32(m, esp, 0x3ffu);
    edi = (edi + 1u);
    edx = (esp + 0xecu);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(rd32(m, 0x916260u) /* 8.286626112932593e-39 */, 0x4ed7deu);
    esp = esp + 0xcu;
    eax = (esp + 0xe8u);
    esp -= 4; wr32(m, esp, 0x2eu);
    esp -= 4; wr32(m, esp, eax);
    wr8(m, esp + 0x4efu, 0x0u);
    LIFT_CALL(rd32(m, 0x916248u) /* 8.28654764021859e-39 */, 0x4ed7f9u);
    edi = eax;
    esp = esp + 0x8u;
    fR_4ed7fe = (edi & edi);
    if (!(fR_4ed7fe == 0)) goto L_4ed6ff;
L_4ed806:
    ecx = rd32(m, ebp + 0x14u);
    ecx = ecx - rd32(m, ebp + 0x10u);
    ecx = uint32_t(int32_t(ecx) >> 2);
    fA_4ed80f = ebx;
    fB_4ed80f = ecx;
    if (fA_4ed80f < fB_4ed80f) goto L_4ed819;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ed819u);
L_4ed819:
    edx = rd32(m, ebp + 0x10u);
    eax = rd32(m, edx + ebx * 4);
    eax = rd32(m, eax + 0x24u);
    ecx = (esp + 0xe8u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x402160u, 0x4ed82fu);  // Table_ColumnIndex
    edx = (esp + 0x20u);
    esp -= 4; wr32(m, esp, edx);
    edi = (esp + 0xd4u);
    wr32(m, esp + 0x24u, eax);
    LIFT_CALL(0x4f04e0u, 0x4ed844u);  // FUN_004f04e0
    ebp = rd32(m, esp + 0x28u);
    ebx = 0u;
    goto L_4ed861;
L_4ed84c:
    eax = (esp + 0x38u);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, esp + 0x28u);
    LIFT_CALL(0x402160u, 0x4ed85au);  // Table_ColumnIndex
    wr32(m, esp + 0xb8u, eax);
L_4ed861:
    fA_4ed861 = rd32(m, esp + 0xb8u);
    fB_4ed861 = ebx;
    if (int32_t(sext(fA_4ed861, 4)) < int32_t(sext(fB_4ed861, 4))) goto L_4ed878;
L_4ed86a:
    edi = rd32(m, esp + 0x1cu);
    ecx = (esp + 0x38u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x4175e0u, 0x4ed878u);  // FUN_004175e0
L_4ed878:
    eax = rd32(m, esp + 0xdcu);
    fA_4ed87f = eax;
    fB_4ed87f = ebx;
    if (fA_4ed87f == fB_4ed87f) goto L_4ed88c;
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x78fc50u, 0x4ed889u);  // ReflexAlignedFree
    esp = esp + 0x4u;
L_4ed88c:
    edx = rd32(m, esp + 0xd0u);
    esp -= 4; wr32(m, esp, edx);
    wr32(m, esp + 0xe0u, ebx);
    wr32(m, esp + 0xe4u, ebx);
    wr32(m, esp + 0xe8u, ebx);
    LIFT_CALL(0x78fc50u, 0x4ed8aeu);  // ReflexAlignedFree
    eax = rd32(m, esp + 0xc4u);
    esp = esp + 0x4u;
    fA_4ed8b8 = eax;
    fB_4ed8b8 = 0x4u;
    eax = fA_4ed8b8 - fB_4ed8b8;
    if (fA_4ed8b8 == fB_4ed8b8) goto L_4ed8fc;
    fA_4ed8bd = eax;
    fB_4ed8bd = 0x1u;
    eax = fA_4ed8bd - fB_4ed8bd;
    if (fA_4ed8bd != fB_4ed8bd) goto L_4ed960;
    esi = rd32(m, esp + 0xc8u);
    eax = rd32(m, esi + 0xcu);
    edi = (esi + 0xcu);
    fA_4ed8d3 = eax;
    fB_4ed8d3 = ebx;
    if (fA_4ed8d3 == fB_4ed8d3) goto L_4ed8e7;
    ecx = rd32(m, esi + 0x14u);
    ecx = ecx - eax;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esi + 0x8u);
    LIFT_CALL(rd32(m, 0x916198u) /* 8.281267547605015e-39 */, 0x4ed8e7u);
L_4ed8e7:
    edx = rd32(m, esi);
    esp -= 4; wr32(m, esp, edx);
    wr32(m, edi, ebx);
    wr32(m, esi + 0x10u, ebx);
    wr32(m, esi + 0x14u, ebx);
    LIFT_CALL(0x78fc50u, 0x4ed8f7u);  // ReflexAlignedFree
    esp = esp + 0x4u;
    goto L_4ed922;
L_4ed8fc:
    esi = rd32(m, esp + 0xc8u);
    fA_4ed903 = rd32(m, esi + 0x18u);
    fB_4ed903 = 0x10u;
    edi = (esi + 0x18u);
    if (fA_4ed903 < fB_4ed903) goto L_4ed915;
    eax = rd32(m, esi + 0x4u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x4f3320u, 0x4ed915u);  // FUN_004f3320
L_4ed915:
    wr32(m, edi, 0xfu);
    wr32(m, esi + 0x14u, ebx);
    wr8(m, esi + 0x4u, 0x0u);
L_4ed922:
    esi = rd32(m, esp + 0xc8u);
    LIFT_CALL(0x4dc0f0u, 0x4ed92eu);  // FUN_004dc0f0
    fA_4ed92e = rd8(m, 0xda67d8u);
    fB_4ed92e = 0x0u;
    if (fA_4ed92e == fB_4ed92e) goto L_4ed942;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x4ed942u);
L_4ed942:
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(rd32(m, 0x916350u) /* 8.287536956934404e-39 */, 0x4ed949u);
    esp = esp + 0x4u;
    fA_4ed94c = rd8(m, 0xda67d8u);
    fB_4ed94c = 0x0u;
    if (fA_4ed94c == fB_4ed94c) goto L_4ed960;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4ed960u);
L_4ed960:
    eax = rd32(m, esp + 0x14u);
    fA_4ed964 = eax;
    fB_4ed964 = ebx;
    if (fA_4ed964 != fB_4ed964) goto L_4ed990;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ed96eu);
    eax = 0u;
L_4ed970:
    ecx = rd32(m, esp + 0x18u);
    fA_4ed974 = ecx;
    fB_4ed974 = rd32(m, eax + 0x10u);
    if (fA_4ed974 < fB_4ed974) goto L_4ed97f;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ed97fu);
L_4ed97f:
    wr32(m, esp + 0x18u, rd32(m, esp + 0x18u) + 0xb0u);
    edi = rd32(m, esp + 0x14u);
    goto L_4ed5d5;
L_4ed990:
    eax = rd32(m, eax);
    goto L_4ed970;
L_4ed994:
    ecx = rd32(m, esp + 0x8ecu);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x4ed9a6u);  // __security_check_cookie
    esp = esp + 0x8e4u;
    LIFT_RET(8);
}

// 004eddc0 Fn_004eddc0
//   original FUN_004eddc0; register inputs: none; returns with ret 12
REFLEX_PORT_FN(0x004eddc0, Fn_004eddc0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4eddea = 0;
    uint32_t fB_4eddea = 0;
    uint32_t fA_4eddf2 = 0;
    uint32_t fB_4eddf2 = 0;
    uint32_t fA_4ede43 = 0;
    uint32_t fB_4ede43 = 0;
    uint32_t fR_4ede50 = 0;
    uint32_t fA_4ede54 = 0;
    uint32_t fB_4ede54 = 0;
    uint32_t fA_4ede5e = 0;
    uint32_t fB_4ede5e = 0;
    uint32_t fR_4ede66 = 0;
    uint32_t fA_4ede76 = 0;
    uint32_t fB_4ede76 = 0;
    uint32_t fA_4ede81 = 0;
    uint32_t fB_4ede81 = 0;
    uint32_t fR_4ede8e = 0;
    uint32_t fA_4ede9e = 0;
    uint32_t fB_4ede9e = 0;
    uint32_t fR_4edeb5 = 0;
    uint32_t fR_4edec1 = 0;
    uint32_t fA_4eded1 = 0;
    uint32_t fB_4eded1 = 0;
    uint32_t fA_4edef2 = 0;
    uint32_t fB_4edef2 = 0;
    uint32_t fA_4edf09 = 0;
    uint32_t fB_4edf09 = 0;
    uint32_t fA_4edf28 = 0;
    uint32_t fB_4edf28 = 0;
    uint32_t fA_4edf2e = 0;
    uint32_t fB_4edf2e = 0;
    uint32_t fR_4edf91 = 0;
    uint32_t fA_4edfbc = 0;
    uint32_t fB_4edfbc = 0;
    uint32_t fA_4edfda = 0;
    uint32_t fB_4edfda = 0;
    uint32_t fR_4edff4 = 0;
    uint32_t fA_4edffc = 0;
    uint32_t fB_4edffc = 0;
    uint32_t fR_4ee003 = 0;
    uint32_t fA_4ee00b = 0;
    uint32_t fB_4ee00b = 0;
    uint32_t fA_4ee028 = 0;
    uint32_t fB_4ee028 = 0;
    uint32_t fA_4ee047 = 0;
    uint32_t fB_4ee047 = 0;
    uint32_t fA_4ee084 = 0;
    uint32_t fB_4ee084 = 0;
    uint32_t fA_4ee0b3 = 0;
    uint32_t fB_4ee0b3 = 0;
    uint32_t fA_4ee0d1 = 0;
    uint32_t fB_4ee0d1 = 0;
    uint32_t fA_4ee0ec = 0;
    uint32_t fB_4ee0ec = 0;
    uint32_t fR_4ee0ff = 0;
    uint32_t fA_4ee113 = 0;
    uint32_t fB_4ee113 = 0;
    uint32_t fR_4ee13a = 0;
    uint32_t fA_4ee14a = 0;
    uint32_t fB_4ee14a = 0;
    uint32_t fA_4ee155 = 0;
    uint32_t fB_4ee155 = 0;
    uint32_t fR_4ee162 = 0;
    uint32_t fA_4ee172 = 0;
    uint32_t fB_4ee172 = 0;
    uint32_t fR_4ee189 = 0;
    uint32_t fR_4ee195 = 0;
    uint32_t fA_4ee1a5 = 0;
    uint32_t fB_4ee1a5 = 0;
    uint32_t fA_4ee1c6 = 0;
    uint32_t fB_4ee1c6 = 0;
    uint32_t fA_4ee1dd = 0;
    uint32_t fB_4ee1dd = 0;
    uint32_t fA_4ee1e3 = 0;
    uint32_t fB_4ee1e3 = 0;
    uint32_t fA_4ee1ed = 0;
    uint32_t fB_4ee1ed = 0;
    uint32_t fA_4ee203 = 0;
    uint32_t fB_4ee203 = 0;
    uint32_t fA_4ee224 = 0;
    uint32_t fB_4ee224 = 0;
    uint32_t fA_4ee28e = 0;
    uint32_t fB_4ee28e = 0;
    uint32_t fA_4ee2e1 = 0;
    uint32_t fB_4ee2e1 = 0;
    uint32_t fA_4ee318 = 0;
    uint32_t fB_4ee318 = 0;
    uint32_t fA_4ee32a = 0;
    uint32_t fB_4ee32a = 0;
    uint32_t fA_4ee354 = 0;
    uint32_t fB_4ee354 = 0;
    uint32_t fA_4ee377 = 0;
    uint32_t fB_4ee377 = 0;
    uint32_t fA_4ee395 = 0;
    uint32_t fB_4ee395 = 0;
    uint32_t fR_4ee3b9 = 0;
    uint32_t fA_4ee3d1 = 0;
    uint32_t fB_4ee3d1 = 0;
    uint32_t fR_4ee3db = 0;
    uint32_t fA_4ee3e7 = 0;
    uint32_t fB_4ee3e7 = 0;
    uint32_t fA_4ee3f5 = 0;
    uint32_t fB_4ee3f5 = 0;
    uint32_t fR_4ee417 = 0;
    uint32_t fA_4ee431 = 0;
    uint32_t fB_4ee431 = 0;
    uint32_t fR_4ee454 = 0;
    uint32_t fA_4ee460 = 0;
    uint32_t fB_4ee460 = 0;
    uint32_t fA_4ee476 = 0;
    uint32_t fB_4ee476 = 0;
    uint32_t fA_4ee491 = 0;
    uint32_t fB_4ee491 = 0;
    uint32_t fA_4ee4ad = 0;
    uint32_t fB_4ee4ad = 0;
    uint32_t fA_4ee4b7 = 0;
    uint32_t fB_4ee4b7 = 0;
    uint32_t fR_4ee4c6 = 0;
    uint32_t fA_4ee4ce = 0;
    uint32_t fB_4ee4ce = 0;
    uint32_t fA_4ee4e8 = 0;
    uint32_t fB_4ee4e8 = 0;
    uint32_t fA_4ee520 = 0;
    uint32_t fB_4ee520 = 0;
    uint32_t fA_4ee52a = 0;
    uint32_t fB_4ee52a = 0;
    uint32_t fR_4ee537 = 0;
    uint32_t fA_4ee547 = 0;
    uint32_t fB_4ee547 = 0;
    uint32_t fR_4ee552 = 0;
    uint32_t fA_4ee562 = 0;
    uint32_t fB_4ee562 = 0;
    uint32_t fA_4ee588 = 0;
    uint32_t fB_4ee588 = 0;
    uint32_t fR_4ee5ad = 0;
    uint32_t fR_4ee634 = 0;
    uint32_t fA_4ee638 = 0;
    uint32_t fB_4ee638 = 0;
    uint32_t fA_4ee64c = 0;
    uint32_t fB_4ee64c = 0;
    uint32_t fR_4ee654 = 0;
    uint32_t fA_4ee660 = 0;
    uint32_t fB_4ee660 = 0;
    uint32_t fR_4ee68a = 0;
    uint32_t fA_4ee69a = 0;
    uint32_t fB_4ee69a = 0;
    uint32_t fR_4ee6b0 = 0;
    uint32_t fA_4ee6c0 = 0;
    uint32_t fB_4ee6c0 = 0;
    uint32_t fA_4ee6da = 0;
    uint32_t fB_4ee6da = 0;
    uint32_t fA_4ee6f5 = 0;
    uint32_t fB_4ee6f5 = 0;
    uint32_t fR_4ee711 = 0;
    uint32_t fA_4ee721 = 0;
    uint32_t fB_4ee721 = 0;
    uint32_t fA_4ee73f = 0;
    uint32_t fB_4ee73f = 0;
    uint32_t fR_4ee781 = 0;
    uint32_t fA_4ee795 = 0;
    uint32_t fB_4ee795 = 0;
    uint32_t fR_4ee7b4 = 0;
    uint32_t fA_4ee7ca = 0;
    uint32_t fB_4ee7ca = 0;
    uint32_t fR_4ee7d1 = 0;
    uint32_t fA_4ee7dd = 0;
    uint32_t fB_4ee7dd = 0;
    uint32_t fA_4ee803 = 0;
    uint32_t fB_4ee803 = 0;
    uint32_t fR_4ee828 = 0;
    esp = esp - 0xc0u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, esp + 0xd4u);
    ebp = rd32(m, esi + 0x10u);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esi + 0xcu);
    ecx = ebp;
    ecx = ecx - edi;
    eax = 0x2e8ba2e9u;
    { int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); }
    edx = uint32_t(int32_t(edx) >> 5);
    eax = edx;
    eax = (eax >> 31);
    fA_4eddea = eax;
    fB_4eddea = edx;
    eax = fA_4eddea + fB_4eddea;
    if ((((fA_4eddea + fB_4eddea)) == 0)) goto L_4ee8c1;
    fA_4eddf2 = edi;
    fB_4eddf2 = ebp;
    if (fA_4eddf2 <= fB_4eddf2) goto L_4eddfc;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4eddfcu);
L_4eddfc:
    ecx = rd32(m, esp + 0xdcu);
    edx = rd32(m, ecx + 0x18u);
    eax = rd32(m, edx + 0x4u);
    ebp = rd32(m, esi);
    ebx = edi;
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0x18u, ebp);
    wr32(m, esp + 0x1cu, ebx);
    LIFT_CALL(0x4f2020u, 0x4ede1bu);  // FUN_004f2020
    ecx = rd32(m, esp + 0xdcu);
    eax = rd32(m, ecx + 0x18u);
    wr32(m, eax + 0x4u, eax);
    eax = rd32(m, ecx + 0x18u);
    wr32(m, ecx + 0x1cu, 0x0u);
    wr32(m, eax, eax);
    eax = rd32(m, ecx + 0x18u);
    wr32(m, eax + 0x8u, eax);
    wr8(m, esp + 0x13u, 0x1u);
L_4ede40:
    edi = rd32(m, esi + 0x10u);
    fA_4ede43 = rd32(m, esi + 0xcu);
    fB_4ede43 = edi;
    if (fA_4ede43 <= fB_4ede43) goto L_4ede4e;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ede4eu);
L_4ede4e:
    esi = rd32(m, esi);
    fR_4ede50 = (ebp & ebp);
    if ((fR_4ede50 == 0)) goto L_4ede58;
    fA_4ede54 = ebp;
    fB_4ede54 = esi;
    if (fA_4ede54 == fB_4ede54) goto L_4ede5e;
L_4ede58:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ede5eu);
L_4ede5e:
    fA_4ede5e = ebx;
    fB_4ede5e = edi;
    if (fA_4ede5e == fB_4ede5e) goto L_4ee8c1;
    fR_4ede66 = (ebp & ebp);
    if (!(fR_4ede66 == 0)) goto L_4edf57;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ede74u);
    eax = 0u;
L_4ede76:
    fA_4ede76 = ebx;
    fB_4ede76 = rd32(m, eax + 0x10u);
    if (fA_4ede76 < fB_4ede76) goto L_4ede81;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ede81u);
L_4ede81:
    fA_4ede81 = rd32(m, ebx + 0x84u);
    fB_4ede81 = 0x0u;
    if (fA_4ede81 != fB_4ede81) goto L_4ee13a;
    fR_4ede8e = (ebp & ebp);
    if (!(fR_4ede8e == 0)) goto L_4edf5f;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ede9cu);
    eax = 0u;
L_4ede9e:
    fA_4ede9e = ebx;
    fB_4ede9e = rd32(m, eax + 0x10u);
    if (fA_4ede9e < fB_4ede9e) goto L_4edea9;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4edea9u);
L_4edea9:
    ecx = rd32(m, ebx + 0xa8u);
    ecx = ecx - rd32(m, ebx + 0xa4u);
    fR_4edeb5 = (ecx & 0xfffffffcu);
    if (!(fR_4edeb5 == 0)) goto L_4ee13a;
    fR_4edec1 = (ebp & ebp);
    if (!(fR_4edec1 == 0)) goto L_4edf67;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4edecfu);
    eax = 0u;
L_4eded1:
    fA_4eded1 = ebx;
    fB_4eded1 = rd32(m, eax + 0x10u);
    if (fA_4eded1 < fB_4eded1) goto L_4ededc;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ededcu);
L_4ededc:
    eax = rd32(m, esp + 0xd4u);
    edx = rd32(m, eax + 0x14u);
    edx = edx - rd32(m, eax + 0x10u);
    esi = rd32(m, ebx + 0x80u);
    edx = uint32_t(int32_t(edx) >> 2);
    fA_4edef2 = esi;
    fB_4edef2 = edx;
    if (fA_4edef2 < fB_4edef2) goto L_4edefc;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4edefcu);
L_4edefc:
    eax = rd32(m, esp + 0xd4u);
    ecx = rd32(m, eax + 0x10u);
    edx = rd32(m, ecx + esi * 4);
    fA_4edf09 = rd32(m, edx + 0x48u);
    fB_4edf09 = 0x0u;
    if (fA_4edf09 == fB_4edf09) goto L_4ee13a;
    esp -= 4; wr32(m, esp, 0x0u);
    esi = (esp + 0x8cu);
    LIFT_CALL(0x4dc570u, 0x4edf21u);  // FUN_004dc570
    eax = rd32(m, esp + 0xdcu);
    fA_4edf28 = rd32(m, eax + 0x24u);
    fB_4edf28 = 0x0u;
    if (fA_4edf28 == fB_4edf28) goto L_4edf6f;
    fA_4edf2e = rd8(m, esp + 0x13u);
    fB_4edf2e = 0x0u;
    if (fA_4edf2e == fB_4edf2e) goto L_4edf6f;
    ecx = eax;
    esp -= 4; wr32(m, esp, ecx);
    edi = (esp + 0x40u);
    LIFT_CALL(0x4ebce0u, 0x4edf41u);  // FUN_004ebce0
    esp -= 4; wr32(m, esp, eax);
    edi = esi;
    LIFT_CALL(0x4ebe60u, 0x4edf49u);  // FUN_004ebe60
    edi = (esp + 0x3cu);
    LIFT_CALL(0x4ebde0u, 0x4edf52u);  // FUN_004ebde0
    goto L_4edfee;
L_4edf57:
    eax = rd32(m, ebp);
    goto L_4ede76;
L_4edf5f:
    eax = rd32(m, ebp);
    goto L_4ede9e;
L_4edf67:
    eax = rd32(m, ebp);
    goto L_4eded1;
L_4edf6f:
    edx = rd32(m, esp + 0xd4u);
    esp -= 4; wr32(m, esp, edx);
    esi = (esp + 0x40u);
    LIFT_CALL(0x4dc570u, 0x4edf80u);  // FUN_004dc570
    esp -= 4; wr32(m, esp, eax);
    edi = (esp + 0x8cu);
    LIFT_CALL(0x4ebe60u, 0x4edf8du);  // FUN_004ebe60
    esi = rd32(m, esp + 0x5cu);
    fR_4edf91 = (esi & esi);
    if ((fR_4edf91 == 0)) goto L_4edfa2;
    LIFT_CALL(0x4ebe10u, 0x4edf9au);  // FUN_004ebe10
    wr32(m, esp + 0x5cu, 0x0u);
L_4edfa2:
    esi = (esp + 0x3cu);
    wr32(m, esp + 0x60u, 0x0u);
    LIFT_CALL(0x4f2220u, 0x4edfb3u);  // FUN_004f2220
    esi = rd32(m, esp + 0x3cu);
    LIFT_CALL(0x4dc0f0u, 0x4edfbcu);  // FUN_004dc0f0
    fA_4edfbc = rd8(m, 0xda67d8u);
    fB_4edfbc = 0x0u;
    if (fA_4edfbc == fB_4edfbc) goto L_4edfd0;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x4edfd0u);
L_4edfd0:
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(rd32(m, 0x916350u) /* 8.287536956934404e-39 */, 0x4edfd7u);
    esp = esp + 0x4u;
    fA_4edfda = rd8(m, 0xda67d8u);
    fB_4edfda = 0x0u;
    if (fA_4edfda == fB_4edfda) goto L_4edfee;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4edfeeu);
L_4edfee:
    edi = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    fR_4edff4 = (ebp & ebp);
    if (!(fR_4edff4 == 0)) goto L_4ee064;
    LIFT_CALL(edi, 0x4edffau);
    eax = 0u;
L_4edffc:
    fA_4edffc = ebx;
    fB_4edffc = rd32(m, eax + 0x10u);
    if (fA_4edffc < fB_4edffc) goto L_4ee003;
    LIFT_CALL(edi, 0x4ee003u);
L_4ee003:
    fR_4ee003 = (ebp & ebp);
    if (!(fR_4ee003 == 0)) goto L_4ee069;
    LIFT_CALL(edi, 0x4ee009u);
    eax = 0u;
L_4ee00b:
    fA_4ee00b = ebx;
    fB_4ee00b = rd32(m, eax + 0x10u);
    if (fA_4ee00b < fB_4ee00b) goto L_4ee012;
    LIFT_CALL(edi, 0x4ee012u);
L_4ee012:
    ebp = rd32(m, esp + 0xd4u);
    eax = rd32(m, ebp + 0x14u);
    eax = eax - rd32(m, ebp + 0x10u);
    esi = rd32(m, ebx + 0x80u);
    eax = uint32_t(int32_t(eax) >> 2);
    fA_4ee028 = esi;
    fB_4ee028 = eax;
    if (fA_4ee028 < fB_4ee028) goto L_4ee02e;
    LIFT_CALL(edi, 0x4ee02eu);
L_4ee02e:
    ecx = rd32(m, ebp + 0x10u);
    esi = rd32(m, ecx + esi * 4);
    ebx = ebx + 0x88u;
    esp -= 4; wr32(m, esp, ebx);
    eax = (esp + 0x8cu);
    LIFT_CALL(0x4fa960u, 0x4ee047u);  // FUN_004fa960
    fA_4ee047 = rd8(m, esp + 0x13u);
    fB_4ee047 = 0x0u;
    edi = rd32(m, esp + 0xdcu);
    if (fA_4ee047 == fB_4ee047) goto L_4ee06e;
    edx = (esp + 0x88u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x4ebe60u, 0x4ee062u);  // FUN_004ebe60
    goto L_4ee07b;
L_4ee064:
    eax = rd32(m, ebp);
    goto L_4edffc;
L_4ee069:
    eax = rd32(m, ebp);
    goto L_4ee00b;
L_4ee06e:
    eax = (esp + 0x88u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x4ebf80u, 0x4ee07bu);  // FUN_004ebf80
L_4ee07b:
    esi = rd32(m, esp + 0xa8u);
    edi = 0u;
    fA_4ee084 = esi;
    fB_4ee084 = edi;
    if (fA_4ee084 == fB_4ee084) goto L_4ee094;
    LIFT_CALL(0x4ebe10u, 0x4ee08du);  // FUN_004ebe10
    wr32(m, esp + 0xa8u, edi);
L_4ee094:
    esi = (esp + 0x88u);
    wr32(m, esp + 0xacu, edi);
    LIFT_CALL(0x4f2220u, 0x4ee0a7u);  // FUN_004f2220
    esi = rd32(m, esp + 0x88u);
    LIFT_CALL(0x4dc0f0u, 0x4ee0b3u);  // FUN_004dc0f0
    fA_4ee0b3 = rd8(m, 0xda67d8u);
    fB_4ee0b3 = 0x0u;
    if (fA_4ee0b3 == fB_4ee0b3) goto L_4ee0c7;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x4ee0c7u);
L_4ee0c7:
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(rd32(m, 0x916350u) /* 8.287536956934404e-39 */, 0x4ee0ceu);
    esp = esp + 0x4u;
    fA_4ee0d1 = rd8(m, 0xda67d8u);
    fB_4ee0d1 = 0x0u;
    if (fA_4ee0d1 == fB_4ee0d1) goto L_4ee0e5;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4ee0e5u);
L_4ee0e5:
    eax = rd32(m, esp + 0xdcu);
    fA_4ee0ec = rd32(m, eax + 0x1cu);
    fB_4ee0ec = 0x0u;
    if (fA_4ee0ec == fB_4ee0ec) goto L_4ee8c1;
    eax = rd32(m, esp + 0x14u);
    wr8(m, esp + 0x13u, 0x0u);
    fR_4ee0ff = (eax & eax);
    if (!(fR_4ee0ff == 0)) goto L_4ee8ba;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee10du);
    eax = 0u;
L_4ee10f:
    ecx = rd32(m, esp + 0x18u);
    fA_4ee113 = ecx;
    fB_4ee113 = rd32(m, eax + 0x10u);
    if (fA_4ee113 < fB_4ee113) goto L_4ee11e;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee11eu);
L_4ee11e:
    wr32(m, esp + 0x18u, rd32(m, esp + 0x18u) + 0xb0u);
    ebx = rd32(m, esp + 0x18u);
    ebp = rd32(m, esp + 0x14u);
    esi = rd32(m, esp + 0xd8u);
    goto L_4ede40;
L_4ee13a:
    fR_4ee13a = (ebp & ebp);
    if (!(fR_4ee13a == 0)) goto L_4ee25b;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee148u);
    eax = 0u;
L_4ee14a:
    fA_4ee14a = ebx;
    fB_4ee14a = rd32(m, eax + 0x10u);
    if (fA_4ee14a < fB_4ee14a) goto L_4ee155;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee155u);
L_4ee155:
    fA_4ee155 = rd32(m, ebx + 0x84u);
    fB_4ee155 = 0x0u;
    if (fA_4ee155 != fB_4ee155) goto L_4ee318;
    fR_4ee162 = (ebp & ebp);
    if (!(fR_4ee162 == 0)) goto L_4ee263;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee170u);
    eax = 0u;
L_4ee172:
    fA_4ee172 = ebx;
    fB_4ee172 = rd32(m, eax + 0x10u);
    if (fA_4ee172 < fB_4ee172) goto L_4ee17d;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee17du);
L_4ee17d:
    ecx = rd32(m, ebx + 0xa8u);
    ecx = ecx - rd32(m, ebx + 0xa4u);
    fR_4ee189 = (ecx & 0xfffffffcu);
    if (!(fR_4ee189 == 0)) goto L_4ee318;
    fR_4ee195 = (ebp & ebp);
    if (!(fR_4ee195 == 0)) goto L_4ee26b;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee1a3u);
    eax = 0u;
L_4ee1a5:
    fA_4ee1a5 = ebx;
    fB_4ee1a5 = rd32(m, eax + 0x10u);
    if (fA_4ee1a5 < fB_4ee1a5) goto L_4ee1b0;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee1b0u);
L_4ee1b0:
    eax = rd32(m, esp + 0xd4u);
    edx = rd32(m, eax + 0x14u);
    edx = edx - rd32(m, eax + 0x10u);
    esi = rd32(m, ebx + 0x80u);
    edx = uint32_t(int32_t(edx) >> 2);
    fA_4ee1c6 = esi;
    fB_4ee1c6 = edx;
    if (fA_4ee1c6 < fB_4ee1c6) goto L_4ee1d0;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee1d0u);
L_4ee1d0:
    eax = rd32(m, esp + 0xd4u);
    ecx = rd32(m, eax + 0x10u);
    eax = rd32(m, ecx + esi * 4);
    fA_4ee1dd = rd32(m, eax + 0x44u);
    fB_4ee1dd = 0x0u;
    if (fA_4ee1dd != fB_4ee1dd) goto L_4ee1ed;
    fA_4ee1e3 = rd32(m, eax + 0x48u);
    fB_4ee1e3 = 0x0u;
    if (fA_4ee1e3 == fB_4ee1e3) goto L_4ee318;
L_4ee1ed:
    fA_4ee1ed = rd8(m, esp + 0x13u);
    fB_4ee1ed = 0x0u;
    esi = (esp + 0x14u);
    if (fA_4ee1ed == fB_4ee1ed) goto L_4ee2c6;
    edx = rd32(m, esp + 0xdcu);
    fA_4ee203 = rd32(m, edx + 0x24u);
    fB_4ee203 = 0x0u;
    if (fA_4ee203 != fB_4ee203) goto L_4ee273;
    LIFT_CALL(0x4f17d0u, 0x4ee20eu);  // FUN_004f17d0
    esi = rd32(m, eax + 0x80u);
    edi = rd32(m, esp + 0xd4u);
    eax = rd32(m, edi + 0x14u);
    eax = eax - rd32(m, edi + 0x10u);
    eax = uint32_t(int32_t(eax) >> 2);
    fA_4ee224 = esi;
    fB_4ee224 = eax;
    if (fA_4ee224 < fB_4ee224) goto L_4ee22e;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee22eu);
L_4ee22e:
    ecx = rd32(m, edi + 0x10u);
    edi = (ecx + esi * 4);
    esi = (esp + 0x14u);
    LIFT_CALL(0x4f17d0u, 0x4ee23du);  // FUN_004f17d0
    ecx = rd32(m, edi);
    eax = eax + 0x88u;
    LIFT_CALL(0x4dca30u, 0x4ee249u);  // FUN_004dca30
    edi = rd32(m, esp + 0xdcu);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x4ebe60u, 0x4ee256u);  // FUN_004ebe60
    goto L_4ee0e5;
L_4ee25b:
    eax = rd32(m, ebp);
    goto L_4ee14a;
L_4ee263:
    eax = rd32(m, ebp);
    goto L_4ee172;
L_4ee26b:
    eax = rd32(m, ebp);
    goto L_4ee1a5;
L_4ee273:
    LIFT_CALL(0x4f17d0u, 0x4ee278u);  // FUN_004f17d0
    edi = rd32(m, esp + 0xd4u);
    edx = rd32(m, edi + 0x14u);
    edx = edx - rd32(m, edi + 0x10u);
    esi = rd32(m, eax + 0x80u);
    edx = uint32_t(int32_t(edx) >> 2);
    fA_4ee28e = esi;
    fB_4ee28e = edx;
    if (fA_4ee28e < fB_4ee28e) goto L_4ee298;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee298u);
L_4ee298:
    eax = rd32(m, edi + 0x10u);
    edi = (eax + esi * 4);
    esi = (esp + 0x14u);
    LIFT_CALL(0x4f17d0u, 0x4ee2a7u);  // FUN_004f17d0
    ecx = rd32(m, edi);
    eax = eax + 0x88u;
    LIFT_CALL(0x4dca30u, 0x4ee2b3u);  // FUN_004dca30
    ecx = rd32(m, esp + 0xdcu);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x4ebee0u, 0x4ee2c1u);  // FUN_004ebee0
    goto L_4ee0e5;
L_4ee2c6:
    LIFT_CALL(0x4f17d0u, 0x4ee2cbu);  // FUN_004f17d0
    edi = rd32(m, esp + 0xd4u);
    edx = rd32(m, edi + 0x14u);
    edx = edx - rd32(m, edi + 0x10u);
    esi = rd32(m, eax + 0x80u);
    edx = uint32_t(int32_t(edx) >> 2);
    fA_4ee2e1 = esi;
    fB_4ee2e1 = edx;
    if (fA_4ee2e1 < fB_4ee2e1) goto L_4ee2eb;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee2ebu);
L_4ee2eb:
    eax = rd32(m, edi + 0x10u);
    edi = (eax + esi * 4);
    esi = (esp + 0x14u);
    LIFT_CALL(0x4f17d0u, 0x4ee2fau);  // FUN_004f17d0
    ecx = rd32(m, edi);
    eax = eax + 0x88u;
    LIFT_CALL(0x4dca30u, 0x4ee306u);  // FUN_004dca30
    edi = rd32(m, esp + 0xdcu);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x4ebf80u, 0x4ee313u);  // FUN_004ebf80
    goto L_4ee0e5;
L_4ee318:
    fA_4ee318 = rd8(m, esp + 0x13u);
    fB_4ee318 = 0x0u;
    ecx = rd32(m, esp + 0xdcu);
    if (fA_4ee318 == fB_4ee318) goto L_4ee608;
    fA_4ee32a = rd32(m, ecx + 0x24u);
    fB_4ee32a = 0x0u;
    ebx = rd32(m, esp + 0xd4u);
    if (fA_4ee32a != fB_4ee32a) goto L_4ee3a9;
    esp -= 4; wr32(m, esp, ebx);
    esi = (esp + 0x40u);
    LIFT_CALL(0x4dc570u, 0x4ee341u);  // FUN_004dc570
    edi = rd32(m, esp + 0xdcu);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x4ebe60u, 0x4ee34eu);  // FUN_004ebe60
    esi = rd32(m, esp + 0x5cu);
    edi = 0u;
    fA_4ee354 = esi;
    fB_4ee354 = edi;
    if (fA_4ee354 == fB_4ee354) goto L_4ee361;
    LIFT_CALL(0x4ebe10u, 0x4ee35du);  // FUN_004ebe10
    wr32(m, esp + 0x5cu, edi);
L_4ee361:
    esi = (esp + 0x3cu);
    wr32(m, esp + 0x60u, edi);
    LIFT_CALL(0x4f2220u, 0x4ee36eu);  // FUN_004f2220
    esi = rd32(m, esp + 0x3cu);
    LIFT_CALL(0x4dc0f0u, 0x4ee377u);  // FUN_004dc0f0
    fA_4ee377 = rd8(m, 0xda67d8u);
    fB_4ee377 = 0x0u;
    if (fA_4ee377 == fB_4ee377) goto L_4ee38b;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x4ee38bu);
L_4ee38b:
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(rd32(m, 0x916350u) /* 8.287536956934404e-39 */, 0x4ee392u);
    esp = esp + 0x4u;
    fA_4ee395 = rd8(m, 0xda67d8u);
    fB_4ee395 = 0x0u;
    if (fA_4ee395 == fB_4ee395) goto L_4ee3a9;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4ee3a9u);
L_4ee3a9:
    ebp = 0u;
    wr32(m, esp + 0x20u, ebp);
L_4ee3b0:
    edx = rd32(m, ebx + 0x14u);
    edx = edx - rd32(m, ebx + 0x10u);
    edx = uint32_t(int32_t(edx) >> 2);
    fR_4ee3b9 = (edx & edx);
    if ((false || (fR_4ee3b9 == 0))) goto L_4ee0e5;
    esi = rd32(m, esp + 0xd4u);
    edi = rd32(m, esi + 0x10u);
    ebx = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    fA_4ee3d1 = edi;
    fB_4ee3d1 = rd32(m, esi + 0x14u);
    if (fA_4ee3d1 <= fB_4ee3d1) goto L_4ee3d8;
    LIFT_CALL(ebx, 0x4ee3d8u);
L_4ee3d8:
    eax = rd32(m, esi + 0x4u);
    fR_4ee3db = (eax & eax);
    if (!(fR_4ee3db == 0)) goto L_4ee507;
    LIFT_CALL(ebx, 0x4ee3e5u);
    eax = 0u;
L_4ee3e7:
    fA_4ee3e7 = edi;
    fB_4ee3e7 = rd32(m, eax + 0x10u);
    if (fA_4ee3e7 < fB_4ee3e7) goto L_4ee3ee;
    LIFT_CALL(ebx, 0x4ee3eeu);
L_4ee3ee:
    eax = rd32(m, edi);
    LIFT_CALL(0x4eb830u, 0x4ee3f5u);  // FUN_004eb830
    fA_4ee3f5 = ebp;
    fB_4ee3f5 = eax;
    if (fA_4ee3f5 >= fB_4ee3f5) goto L_4ee0e5;
    eax = esi;
    wr32(m, esp + 0x6cu, eax);
    eax = rd32(m, esp + 0x14u);
    wr32(m, esp + 0x64u, ebp);
    wr32(m, esp + 0x68u, 0x0u);
    esi = (esp + 0x64u);
    fR_4ee417 = (eax & eax);
    if (!(fR_4ee417 == 0)) goto L_4ee50e;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee425u);
    eax = 0u;
L_4ee427:
    ebx = rd32(m, esp + 0x18u);
    edi = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    fA_4ee431 = ebx;
    fB_4ee431 = rd32(m, eax + 0x10u);
    if (fA_4ee431 < fB_4ee431) goto L_4ee438;
    LIFT_CALL(edi, 0x4ee438u);
L_4ee438:
    ebp = rd32(m, ebx + 0x80u);
    wr32(m, esp + 0x1cu, 0x0u);
    goto L_4ee450;
L_4ee450:
    eax = rd32(m, esp + 0x14u);
    fR_4ee454 = (eax & eax);
    if (!(fR_4ee454 == 0)) goto L_4ee515;
    LIFT_CALL(edi, 0x4ee45eu);
    eax = 0u;
L_4ee460:
    fA_4ee460 = ebx;
    fB_4ee460 = rd32(m, eax + 0x10u);
    if (fA_4ee460 < fB_4ee460) goto L_4ee467;
    LIFT_CALL(edi, 0x4ee467u);
L_4ee467:
    ecx = rd32(m, ebx + 0xa8u);
    ecx = ecx - rd32(m, ebx + 0xa4u);
    ecx = uint32_t(int32_t(ecx) >> 2);
    fA_4ee476 = rd32(m, esp + 0x1cu);
    fB_4ee476 = ecx;
    if (fA_4ee476 >= fB_4ee476) goto L_4ee520;
    edi = rd32(m, esi);
    esi = rd32(m, esi + 0x8u);
    edx = rd32(m, esi + 0x14u);
    edx = edx - rd32(m, esi + 0x10u);
    esi = esi + 0x4u;
    edx = uint32_t(int32_t(edx) >> 2);
    fA_4ee491 = ebp;
    fB_4ee491 = edx;
    if (fA_4ee491 < fB_4ee491) goto L_4ee49b;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee49bu);
L_4ee49b:
    eax = rd32(m, esi + 0xcu);
    esi = rd32(m, eax + ebp * 4);
    ecx = (eax + ebp * 4);
    eax = edi;
    LIFT_CALL(0x4eb920u, 0x4ee4abu);  // FUN_004eb920
    esi = eax;
    fA_4ee4ad = rd32(m, esi + 0x8u);
    fB_4ee4ad = 0x0u;
    if (fA_4ee4ad == fB_4ee4ad) goto L_4ee5e6;
    fA_4ee4b7 = rd32(m, esi);
    fB_4ee4b7 = 0xffffffffu;
    if (fA_4ee4b7 == fB_4ee4b7) goto L_4ee520;
    eax = rd32(m, esp + 0x14u);
    ebp = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    fR_4ee4c6 = (eax & eax);
    if (!(fR_4ee4c6 == 0)) goto L_4ee51c;
    LIFT_CALL(ebp, 0x4ee4ccu);
    eax = 0u;
L_4ee4ce:
    fA_4ee4ce = ebx;
    fB_4ee4ce = rd32(m, eax + 0x10u);
    if (fA_4ee4ce < fB_4ee4ce) goto L_4ee4d5;
    LIFT_CALL(ebp, 0x4ee4d5u);
L_4ee4d5:
    ecx = rd32(m, ebx + 0xa8u);
    ecx = ecx - rd32(m, ebx + 0xa4u);
    edi = rd32(m, esp + 0x1cu);
    ecx = uint32_t(int32_t(ecx) >> 2);
    fA_4ee4e8 = edi;
    fB_4ee4e8 = ecx;
    if (fA_4ee4e8 < fB_4ee4e8) goto L_4ee4ee;
    LIFT_CALL(ebp, 0x4ee4eeu);
L_4ee4ee:
    edx = rd32(m, ebx + 0xa4u);
    ebp = rd32(m, edx + edi * 4);
    edi = (edi + 1u);
    wr32(m, esp + 0x1cu, edi);
    edi = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    goto L_4ee450;
L_4ee507:
    eax = rd32(m, eax);
    goto L_4ee3e7;
L_4ee50e:
    eax = rd32(m, eax);
    goto L_4ee427;
L_4ee515:
    eax = rd32(m, eax);
    goto L_4ee460;
L_4ee51c:
    eax = rd32(m, eax);
    goto L_4ee4ce;
L_4ee520:
    fA_4ee520 = rd32(m, esi + 0x8u);
    fB_4ee520 = 0x0u;
    if (fA_4ee520 == fB_4ee520) goto L_4ee5e6;
    fA_4ee52a = rd32(m, esi);
    fB_4ee52a = 0xffffffffu;
    if (fA_4ee52a == fB_4ee52a) goto L_4ee5e6;
    edi = rd32(m, esp + 0x14u);
    fR_4ee537 = (edi & edi);
    if (!(fR_4ee537 == 0)) goto L_4ee5fa;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee545u);
    eax = 0u;
L_4ee547:
    fA_4ee547 = ebx;
    fB_4ee547 = rd32(m, eax + 0x10u);
    if (fA_4ee547 < fB_4ee547) goto L_4ee552;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee552u);
L_4ee552:
    fR_4ee552 = (edi & edi);
    if (!(fR_4ee552 == 0)) goto L_4ee601;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee560u);
    eax = 0u;
L_4ee562:
    fA_4ee562 = ebx;
    fB_4ee562 = rd32(m, eax + 0x10u);
    if (fA_4ee562 < fB_4ee562) goto L_4ee56d;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee56du);
L_4ee56d:
    ebx = rd32(m, esi);
    esi = rd32(m, esi + 0x8u);
    ecx = rd32(m, esi + 0x14u);
    ecx = ecx - rd32(m, esi + 0x10u);
    eax = rd32(m, esp + 0x18u);
    edi = rd32(m, eax + 0x84u);
    esi = esi + 0x4u;
    ecx = uint32_t(int32_t(ecx) >> 2);
    fA_4ee588 = ebp;
    fB_4ee588 = ecx;
    if (fA_4ee588 < fB_4ee588) goto L_4ee592;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee592u);
L_4ee592:
    edx = rd32(m, esi + 0xcu);
    ecx = rd32(m, edx + ebp * 4);
    eax = rd32(m, esp + 0x18u);
    ebp = (edx + ebp * 4);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, ecx);
    eax = eax + 0x88u;
    ecx = ebx;
    LIFT_CALL(0x4eb9f0u, 0x4ee5adu);  // FUN_004eb9f0
    fR_4ee5ad = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_4ee5ad == 0)) goto L_4ee5e6;
    edx = rd32(m, esp + 0x20u);
    eax = rd32(m, esp + 0xdcu);
    ecx = rd32(m, eax + 0x24u);
    wr32(m, esp + 0x7cu, edx);
    edx = (esp + 0xc4u);
    esp -= 4; wr32(m, esp, edx);
    wr32(m, esp + 0x84u, eax);
    esp -= 4; wr32(m, esp, eax);
    eax = (esp + 0x84u);
    wr32(m, esp + 0x8cu, ecx);
    LIFT_CALL(0x4f06c0u, 0x4ee5e6u);  // FUN_004f06c0
L_4ee5e6:
    wr32(m, esp + 0x20u, (rd32(m, esp + 0x20u) + 1u));
    ebx = rd32(m, esp + 0xd4u);
    ebp = rd32(m, esp + 0x20u);
    goto L_4ee3b0;
L_4ee5fa:
    eax = rd32(m, edi);
    goto L_4ee547;
L_4ee601:
    eax = rd32(m, edi);
    goto L_4ee562;
L_4ee608:
    eax = rd32(m, ecx + 0x18u);
    eax = rd32(m, eax);
    ecx = rd32(m, ecx);
    wr32(m, esp + 0x24u, ecx);
    wr32(m, esp + 0x28u, eax);
    goto L_4ee620;
L_4ee620:
    eax = rd32(m, esp + 0xdcu);
    edi = rd32(m, esp + 0x24u);
    esi = rd32(m, eax + 0x18u);
    ebx = rd32(m, esp + 0x28u);
    eax = rd32(m, eax);
    fR_4ee634 = (edi & edi);
    if ((fR_4ee634 == 0)) goto L_4ee63c;
    fA_4ee638 = edi;
    fB_4ee638 = eax;
    if (fA_4ee638 == fB_4ee638) goto L_4ee646;
L_4ee63c:
    ebp = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    LIFT_CALL(ebp, 0x4ee644u);
    goto L_4ee64c;
L_4ee646:
    ebp = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
L_4ee64c:
    fA_4ee64c = ebx;
    fB_4ee64c = esi;
    if (fA_4ee64c == fB_4ee64c) goto L_4ee0e5;
    fR_4ee654 = (edi & edi);
    if (!(fR_4ee654 == 0)) goto L_4ee760;
    LIFT_CALL(ebp, 0x4ee65eu);
    eax = 0u;
L_4ee660:
    fA_4ee660 = ebx;
    fB_4ee660 = rd32(m, eax + 0x18u);
    if (fA_4ee660 != fB_4ee660) goto L_4ee667;
    LIFT_CALL(ebp, 0x4ee667u);
L_4ee667:
    x0 = movsd_m(rd64(m, ebx + 0xcu));
    edx = rd32(m, ebx + 0x14u);
    ebx = rd32(m, esp + 0x14u);
    wr64(m, esp + 0xb0u, x0.lo);
    wr32(m, esp + 0xb8u, edx);
    esi = (esp + 0xb0u);
    fR_4ee68a = (ebx & ebx);
    if (!(fR_4ee68a == 0)) goto L_4ee767;
    LIFT_CALL(ebp, 0x4ee694u);
    eax = 0u;
L_4ee696:
    edi = rd32(m, esp + 0x18u);
    fA_4ee69a = edi;
    fB_4ee69a = rd32(m, eax + 0x10u);
    if (fA_4ee69a < fB_4ee69a) goto L_4ee6a1;
    LIFT_CALL(ebp, 0x4ee6a1u);
L_4ee6a1:
    ebp = rd32(m, edi + 0x80u);
    wr32(m, esp + 0x1cu, 0x0u);
L_4ee6b0:
    fR_4ee6b0 = (ebx & ebx);
    if (!(fR_4ee6b0 == 0)) goto L_4ee76e;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee6beu);
    eax = 0u;
L_4ee6c0:
    fA_4ee6c0 = edi;
    fB_4ee6c0 = rd32(m, eax + 0x10u);
    if (fA_4ee6c0 < fB_4ee6c0) goto L_4ee6cb;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee6cbu);
L_4ee6cb:
    eax = rd32(m, edi + 0xa8u);
    eax = eax - rd32(m, edi + 0xa4u);
    eax = uint32_t(int32_t(eax) >> 2);
    fA_4ee6da = rd32(m, esp + 0x1cu);
    fB_4ee6da = eax;
    if (fA_4ee6da >= fB_4ee6da) goto L_4ee779;
    edi = rd32(m, esi);
    esi = rd32(m, esi + 0x8u);
    ecx = rd32(m, esi + 0x14u);
    ecx = ecx - rd32(m, esi + 0x10u);
    esi = esi + 0x4u;
    ecx = uint32_t(int32_t(ecx) >> 2);
    fA_4ee6f5 = ebp;
    fB_4ee6f5 = ecx;
    if (fA_4ee6f5 < fB_4ee6f5) goto L_4ee6ff;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee6ffu);
L_4ee6ff:
    edx = rd32(m, esi + 0xcu);
    esi = rd32(m, edx + ebp * 4);
    ecx = (edx + ebp * 4);
    eax = edi;
    LIFT_CALL(0x4eb920u, 0x4ee70fu);  // FUN_004eb920
    esi = eax;
    fR_4ee711 = (ebx & ebx);
    if (!(fR_4ee711 == 0)) goto L_4ee775;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee71bu);
    eax = 0u;
L_4ee71d:
    ebp = rd32(m, esp + 0x18u);
    fA_4ee721 = ebp;
    fB_4ee721 = rd32(m, eax + 0x10u);
    if (fA_4ee721 < fB_4ee721) goto L_4ee72c;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee72cu);
L_4ee72c:
    eax = rd32(m, ebp + 0xa8u);
    eax = eax - rd32(m, ebp + 0xa4u);
    edi = rd32(m, esp + 0x1cu);
    eax = uint32_t(int32_t(eax) >> 2);
    fA_4ee73f = edi;
    fB_4ee73f = eax;
    if (fA_4ee73f < fB_4ee73f) goto L_4ee749;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee749u);
L_4ee749:
    ecx = rd32(m, ebp + 0xa4u);
    ebp = rd32(m, ecx + edi * 4);
    edi = (edi + 1u);
    wr32(m, esp + 0x1cu, edi);
    edi = rd32(m, esp + 0x18u);
    goto L_4ee6b0;
L_4ee760:
    eax = rd32(m, edi);
    goto L_4ee660;
L_4ee767:
    eax = rd32(m, ebx);
    goto L_4ee696;
L_4ee76e:
    eax = rd32(m, ebx);
    goto L_4ee6c0;
L_4ee775:
    eax = rd32(m, ebx);
    goto L_4ee71d;
L_4ee779:
    eax = rd32(m, esp + 0x24u);
    wr32(m, esp + 0x1cu, esi);
    fR_4ee781 = (eax & eax);
    if (!(fR_4ee781 == 0)) goto L_4ee8a5;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee78fu);
    eax = 0u;
L_4ee791:
    edx = rd32(m, esp + 0x28u);
    fA_4ee795 = edx;
    fB_4ee795 = rd32(m, eax + 0x18u);
    if (fA_4ee795 != fB_4ee795) goto L_4ee7a0;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee7a0u);
L_4ee7a0:
    eax = rd32(m, esp + 0x28u);
    ecx = rd32(m, eax + 0xcu);
    esi = (esp + 0x24u);
    wr32(m, esp + 0x20u, ecx);
    LIFT_CALL(0x417df0u, 0x4ee7b4u);  // FUN_00417df0
    fR_4ee7b4 = (ebx & ebx);
    if (!(fR_4ee7b4 == 0)) goto L_4ee8ac;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee7c2u);
    eax = 0u;
L_4ee7c4:
    esi = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    fA_4ee7ca = edi;
    fB_4ee7ca = rd32(m, eax + 0x10u);
    if (fA_4ee7ca < fB_4ee7ca) goto L_4ee7d1;
    LIFT_CALL(esi, 0x4ee7d1u);
L_4ee7d1:
    fR_4ee7d1 = (ebx & ebx);
    if (!(fR_4ee7d1 == 0)) goto L_4ee8b3;
    LIFT_CALL(esi, 0x4ee7dbu);
    eax = 0u;
L_4ee7dd:
    fA_4ee7dd = edi;
    fB_4ee7dd = rd32(m, eax + 0x10u);
    if (fA_4ee7dd < fB_4ee7dd) goto L_4ee7e4;
    LIFT_CALL(esi, 0x4ee7e4u);
L_4ee7e4:
    eax = rd32(m, esp + 0x1cu);
    esi = rd32(m, eax + 0x8u);
    ebx = rd32(m, eax);
    eax = rd32(m, esi + 0x14u);
    eax = eax - rd32(m, esi + 0x10u);
    edx = rd32(m, esp + 0x18u);
    edi = rd32(m, edx + 0x84u);
    esi = esi + 0x4u;
    eax = uint32_t(int32_t(eax) >> 2);
    fA_4ee803 = ebp;
    fB_4ee803 = eax;
    if (fA_4ee803 < fB_4ee803) goto L_4ee80d;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4ee80du);
L_4ee80d:
    ecx = rd32(m, esi + 0xcu);
    edx = rd32(m, ecx + ebp * 4);
    eax = rd32(m, esp + 0x18u);
    ebp = (ecx + ebp * 4);
    esp -= 4; wr32(m, esp, edi);
    eax = eax + 0x88u;
    esp -= 4; wr32(m, esp, edx);
    ecx = ebx;
    LIFT_CALL(0x4eb9f0u, 0x4ee828u);  // FUN_004eb9f0
    fR_4ee828 = ((eax & 0xffu) & (eax & 0xffu));
    if (!(fR_4ee828 == 0)) goto L_4ee620;
    eax = rd32(m, esp + 0x20u);
    wr32(m, esp + 0x70u, eax);
    eax = rd32(m, esp + 0xdcu);
    ecx = rd32(m, eax + 0x24u);
    edx = (esp + 0x2cu);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, eax);
    edi = (esp + 0x78u);
    wr32(m, esp + 0x7cu, eax);
    wr32(m, esp + 0x80u, ecx);
    LIFT_CALL(0x4f3c60u, 0x4ee85cu);  // FUN_004f3c60
    esi = rd32(m, esp + 0x38u);
    ebp = rd32(m, esp + 0x34u);
    ebx = rd32(m, esp + 0x30u);
    eax = rd32(m, esp + 0x2cu);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, eax);
    edi = (esp + 0x2cu);
    wr32(m, esp + 0x2cu, 0x0u);
    LIFT_CALL(0x4faa00u, 0x4ee881u);  // FUN_004faa00
    ecx = rd32(m, esp + 0x3cu);
    eax = rd32(m, esp + 0xecu);
    esp = esp + 0x10u;
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ecx);
    edx = (esp + 0xccu);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x4f1eb0u, 0x4ee8a0u);  // FUN_004f1eb0
    goto L_4ee620;
L_4ee8a5:
    eax = rd32(m, eax);
    goto L_4ee791;
L_4ee8ac:
    eax = rd32(m, ebx);
    goto L_4ee7c4;
L_4ee8b3:
    eax = rd32(m, ebx);
    goto L_4ee7dd;
L_4ee8ba:
    eax = rd32(m, eax);
    goto L_4ee10f;
L_4ee8c1:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0xc0u;
    LIFT_RET(12);
}

// 004eedc0 Fn_004eedc0
//   original FUN_004eedc0; register inputs: edi; returns with ret 0
REFLEX_PORT_FN(0x004eedc0, Fn_004eedc0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4eedc2 = 0;
    uint32_t fB_4eedc2 = 0;
    uint32_t fA_4eedc8 = 0;
    uint32_t fB_4eedc8 = 0;
    uint32_t fA_4eede8 = 0;
    uint32_t fB_4eede8 = 0;
    eax = rd32(m, edi);
    fA_4eedc2 = eax;
    fB_4eedc2 = 0x4u;
    eax = fA_4eedc2 - fB_4eedc2;
    esp -= 4; wr32(m, esp, esi);
    if (fA_4eedc2 == fB_4eedc2) goto L_4eede5;
    fA_4eedc8 = eax;
    fB_4eedc8 = 0x1u;
    eax = fA_4eedc8 - fB_4eedc8;
    if (fA_4eedc8 != fB_4eedc8) goto L_4eee14;
    esi = rd32(m, edi + 0x8u);
    LIFT_CALL(0x4eee20u, 0x4eedd5u);  // FUN_004eee20
    LIFT_CALL(0x4dc0f0u, 0x4eeddau);  // FUN_004dc0f0
    eax = rd32(m, edi + 0x8u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x775540u, 0x4eede3u);  // StdMemoryAllocator::vf2
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_4eede5:
    esi = rd32(m, edi + 0x8u);
    fA_4eede8 = rd32(m, esi + 0x18u);
    fB_4eede8 = 0x10u;
    if (fA_4eede8 < fB_4eede8) goto L_4eedf7;
    ecx = rd32(m, esi + 0x4u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x4f3320u, 0x4eedf7u);  // FUN_004f3320
L_4eedf7:
    eax = 0u;
    wr32(m, esi + 0x18u, 0xfu);
    wr32(m, esi + 0x14u, eax);
    wr8(m, esi + 0x4u, (eax & 0xffu));
    LIFT_CALL(0x4dc0f0u, 0x4eee0bu);  // FUN_004dc0f0
    edx = rd32(m, edi + 0x8u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x775540u, 0x4eee14u);  // StdMemoryAllocator::vf2
L_4eee14:
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 004eee60 Fn_004eee60
//   original FUN_004eee60; register inputs: edi; returns with ret 4
REFLEX_PORT_FN(0x004eee60, Fn_004eee60, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4eee6b = 0;
    uint32_t fB_4eee6b = 0;
    uint32_t fR_4eee9d = 0;
    uint32_t fR_4eeecc = 0;
    eax = rd32(m, edi);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x8u);
    wr32(m, ebx, eax);
    eax = (eax - 1u);
    esp -= 4; wr32(m, esp, esi);
    fA_4eee6b = eax;
    fB_4eee6b = 0x5u;
    if (fA_4eee6b > fB_4eee6b) goto L_4eeef8;
    switch (eax) { case 0: goto L_4eee7b; case 1: goto L_4eee86; case 2: goto L_4eeef8; case 3: goto L_4eee91; case 4: goto L_4eeebe; case 5: goto L_4eeeec; default: throw std::runtime_error("jump table"); }
L_4eee7b:
    eax = rd32(m, edi + 0x8u);
    wr32(m, ebx + 0x8u, eax);
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_4eee86:
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x8u)));
    wr32(m, ebx + 0x8u, fst32(m));
    fpop(m);
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_4eee91:
    LIFT_CALL(0x4dc0f0u, 0x4eee96u);  // FUN_004dc0f0
    esp -= 4; wr32(m, esp, 0x1cu);
    LIFT_CALL(0x4eef20u, 0x4eee9du);  // StdMemoryAllocator::vf1
    fR_4eee9d = (eax & eax);
    if ((fR_4eee9d == 0)) goto L_4eeeb4;
    ecx = rd32(m, edi + 0x8u);
    esp -= 4; wr32(m, esp, ecx);
    esi = eax;
    LIFT_CALL(0x4dc440u, 0x4eeeacu);  // FUN_004dc440
    wr32(m, ebx + 0x8u, eax);
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_4eeeb4:
    eax = 0u;
    wr32(m, ebx + 0x8u, eax);
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_4eeebe:
    LIFT_CALL(0x4dc0f0u, 0x4eeec3u);  // FUN_004dc0f0
    esp -= 4; wr32(m, esp, 0x18u);
    LIFT_CALL(0x4eef20u, 0x4eeecau);  // StdMemoryAllocator::vf1
    esi = eax;
    fR_4eeecc = (esi & esi);
    if ((fR_4eeecc == 0)) goto L_4eeee2;
    edx = rd32(m, edi + 0x8u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x4f0220u, 0x4eeedau);  // FUN_004f0220
    wr32(m, ebx + 0x8u, esi);
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_4eeee2:
    esi = 0u;
    wr32(m, ebx + 0x8u, esi);
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_4eeeec:
    eax = rd32(m, edi + 0x8u);
    wr32(m, ebx + 0x8u, eax);
    ecx = rd32(m, edi + 0xcu);
    wr32(m, ebx + 0xcu, ecx);
L_4eeef8:
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 004eef20 StdMemoryAllocator__vf1
//   original StdMemoryAllocator::vf1; register inputs: none; returns with ret 4
REFLEX_PORT_FN(0x004eef20, StdMemoryAllocator__vf1, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4eef20 = 0;
    uint32_t fB_4eef20 = 0;
    uint32_t fA_4eef45 = 0;
    uint32_t fB_4eef45 = 0;
    fA_4eef20 = rd8(m, 0xda67d8u);
    fB_4eef20 = 0x0u;
    if (fA_4eef20 == fB_4eef20) goto L_4eef34;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x4eef34u);
L_4eef34:
    eax = rd32(m, esp + 0x4u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, 0x10u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x916358u) /* 8.287573390694476e-39 */, 0x4eef42u);
    esp = esp + 0x8u;
    fA_4eef45 = rd8(m, 0xda67d8u);
    fB_4eef45 = 0x0u;
    esi = eax;
    if (fA_4eef45 == fB_4eef45) goto L_4eef5b;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4eef5bu);
L_4eef5b:
    eax = esi;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 004f0630 Fn_004f0630
//   original FUN_004f0630; register inputs: esi; returns with ret 0
REFLEX_PORT_FN(0x004f0630, Fn_004f0630, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_4f0635 = 0;
    LIFT_CALL(0x4f4a30u, 0x4f0635u);  // FUN_004f4a30
    fR_4f0635 = (eax & eax);
    if ((fR_4f0635 == 0)) goto L_4f063d;
    wr32(m, eax, esi);
    goto L_4f063f;
L_4f063d:
    eax = 0u;
L_4f063f:
    wr32(m, esi, eax);
    LIFT_CALL(0x4f4ea0u, 0x4f0646u);  // FUN_004f4ea0
    wr32(m, esi + 0x18u, eax);
    wr8(m, eax + 0x19u, 0x1u);
    eax = rd32(m, esi + 0x18u);
    wr32(m, eax + 0x4u, eax);
    eax = rd32(m, esi + 0x18u);
    wr32(m, eax, eax);
    eax = rd32(m, esi + 0x18u);
    wr32(m, eax + 0x8u, eax);
    wr32(m, esi + 0x1cu, 0x0u);
    eax = esi;
    LIFT_RET(0);
}

// 004f0670 Fn_004f0670
//   original FUN_004f0670; register inputs: eax; returns with ret 0
REFLEX_PORT_FN(0x004f0670, Fn_004f0670, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4f067f = 0;
    uint32_t fB_4f067f = 0;
    uint32_t fA_4f069d = 0;
    uint32_t fB_4f069d = 0;
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    LIFT_CALL(0x4f2220u, 0x4f0678u);  // FUN_004f2220
    esi = rd32(m, esi);
    LIFT_CALL(0x4dc0f0u, 0x4f067fu);  // FUN_004dc0f0
    fA_4f067f = rd8(m, 0xda67d8u);
    fB_4f067f = 0x0u;
    if (fA_4f067f == fB_4f067f) goto L_4f0693;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x4f0693u);
L_4f0693:
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(rd32(m, 0x916350u) /* 8.287536956934404e-39 */, 0x4f069au);
    esp = esp + 0x4u;
    fA_4f069d = rd8(m, 0xda67d8u);
    fB_4f069d = 0x0u;
    if (fA_4f069d == fB_4f069d) goto L_4f06b1;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4f06b1u);
L_4f06b1:
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 004f06c0 Fn_004f06c0
//   original FUN_004f06c0; register inputs: eax; returns with ret 8
REFLEX_PORT_FN(0x004f06c0, Fn_004f06c0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4f06d7 = 0;
    uint32_t fB_4f06d7 = 0;
    uint32_t fR_4f06ed = 0;
    uint32_t fA_4f06fd = 0;
    uint32_t fB_4f06fd = 0;
    uint32_t fR_4f0706 = 0;
    uint32_t fA_4f0711 = 0;
    uint32_t fB_4f0711 = 0;
    uint32_t fR_4f0729 = 0;
    uint32_t fR_4f0736 = 0;
    uint32_t fA_4f073a = 0;
    uint32_t fB_4f073a = 0;
    uint32_t fA_4f0744 = 0;
    uint32_t fB_4f0744 = 0;
    uint32_t fR_4f078d = 0;
    uint32_t fA_4f07a1 = 0;
    uint32_t fB_4f07a1 = 0;
    uint32_t fR_4f07a6 = 0;
    esp = esp - 0xcu;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x1cu);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = eax;
    eax = rd32(m, esp + 0x20u);
    eax = rd32(m, eax + 0x18u);
    esi = rd32(m, eax + 0x4u);
    fA_4f06d7 = rd8(m, esi + 0x19u);
    fB_4f06d7 = 0x0u;
    ebx = eax;
    eax = (eax & 0xffffff00u) | ((0x1u) & 0xffu);
    wr8(m, esp + 0x24u, (eax & 0xffu));
    if (fA_4f06d7 != fB_4f06d7) goto L_4f0717;
L_4f06e5:
    eax = rd32(m, edi + 0x4u);
    ebx = esi;
    ecx = (esi + 0xcu);
    fR_4f06ed = (eax & eax);
    if ((fR_4f06ed == 0)) goto L_4f06fb;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x4ec2c0u, 0x4f06f9u);  // FUN_004ec2c0
    goto L_4f0702;
L_4f06fb:
    edx = rd32(m, edi);
    fA_4f06fd = edx;
    fB_4f06fd = rd32(m, ecx);
    eax = (eax & 0xffffff00u) | (((fA_4f06fd < fB_4f06fd) ? 1u : 0u) & 0xffu);
L_4f0702:
    wr8(m, esp + 0x24u, (eax & 0xffu));
    fR_4f0706 = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_4f0706 == 0)) goto L_4f070e;
    esi = rd32(m, esi);
    goto L_4f0711;
L_4f070e:
    esi = rd32(m, esi + 0x8u);
L_4f0711:
    fA_4f0711 = rd8(m, esi + 0x19u);
    fB_4f0711 = 0x0u;
    if (fA_4f0711 == fB_4f0711) goto L_4f06e5;
L_4f0717:
    ecx = rd32(m, esp + 0x20u);
    ecx = rd32(m, ecx);
    esi = ebx;
    edx = ecx;
    wr32(m, esp + 0x14u, esi);
    wr32(m, esp + 0x10u, edx);
    fR_4f0729 = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_4f0729 == 0)) goto L_4f0787;
    eax = rd32(m, esp + 0x20u);
    eax = rd32(m, eax + 0x18u);
    esi = rd32(m, eax);
    fR_4f0736 = (edx & edx);
    if ((fR_4f0736 == 0)) goto L_4f073e;
    fA_4f073a = edx;
    fB_4f073a = ecx;
    if (fA_4f073a == fB_4f073a) goto L_4f0744;
L_4f073e:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4f0744u);
L_4f0744:
    fA_4f0744 = ebx;
    fB_4f0744 = esi;
    if (fA_4f0744 != fB_4f0744) goto L_4f0776;
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esp + 0x24u);
    esp -= 4; wr32(m, esp, 0x1u);
    ecx = (esp + 0x18u);
    esp -= 4; wr32(m, esp, ecx);
    eax = ebx;
    LIFT_CALL(0x4f2090u, 0x4f075bu);  // FUN_004f2090
    edx = rd32(m, eax);
    eax = rd32(m, eax + 0x4u);
    wr32(m, ebp + 0x4u, eax);
    wr8(m, ebp + 0x8u, 0x1u);
    wr32(m, ebp, edx);
    eax = ebp;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0xcu;
    LIFT_RET(8);
L_4f0776:
    esi = (esp + 0x10u);
    LIFT_CALL(0x4f4f90u, 0x4f077fu);  // FUN_004f4f90
    esi = rd32(m, esp + 0x14u);
    edx = rd32(m, esp + 0x10u);
L_4f0787:
    ecx = rd32(m, esi + 0x10u);
    eax = (esi + 0xcu);
    fR_4f078d = (ecx & ecx);
    if ((fR_4f078d == 0)) goto L_4f079f;
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x4ec2c0u, 0x4f0799u);  // FUN_004ec2c0
    edx = rd32(m, esp + 0x10u);
    goto L_4f07a6;
L_4f079f:
    ecx = rd32(m, eax);
    fA_4f07a1 = ecx;
    fB_4f07a1 = rd32(m, edi);
    eax = (eax & 0xffffff00u) | (((fA_4f07a1 < fB_4f07a1) ? 1u : 0u) & 0xffu);
L_4f07a6:
    fR_4f07a6 = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_4f07a6 == 0)) goto L_4f07db;
    edx = rd32(m, esp + 0x24u);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esp + 0x24u);
    esp -= 4; wr32(m, esp, edx);
    eax = (esp + 0x18u);
    esp -= 4; wr32(m, esp, eax);
    eax = ebx;
    LIFT_CALL(0x4f2090u, 0x4f07c0u);  // FUN_004f2090
    ecx = rd32(m, eax);
    edx = rd32(m, eax + 0x4u);
    wr32(m, ebp, ecx);
    wr32(m, ebp + 0x4u, edx);
    wr8(m, ebp + 0x8u, 0x1u);
    eax = ebp;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0xcu;
    LIFT_RET(8);
L_4f07db:
    edi = rd32(m, esp); esp += 4;
    wr32(m, ebp + 0x4u, esi);
    esi = rd32(m, esp); esp += 4;
    wr8(m, ebp + 0x8u, 0x0u);
    wr32(m, ebp, edx);
    eax = ebp;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0xcu;
    LIFT_RET(8);
}

// 004f0800 Fn_004f0800
//   original FUN_004f0800; register inputs: ecx esi; returns with ret 0
REFLEX_PORT_FN(0x004f0800, Fn_004f0800, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    esp = esp - 0x18u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, edi);
    eax = (esp + 0x10u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, esi);
    edi = ecx;
    LIFT_CALL(0x4f3c60u, 0x4f0813u);  // FUN_004f3c60
    ebx = rd32(m, esp + 0x1cu);
    ebp = rd32(m, esp + 0x18u);
    ecx = rd32(m, esp + 0x14u);
    edx = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edx);
    edi = (esp + 0x1cu);
    wr32(m, esp + 0x1cu, 0x0u);
    LIFT_CALL(0x4faa00u, 0x4f0838u);  // FUN_004faa00
    eax = rd32(m, esp + 0x24u);
    ecx = rd32(m, esp + 0x20u);
    esp = esp + 0x10u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    edx = (esp + 0x20u);
    esp -= 4; wr32(m, esp, edx);
    eax = esi;
    LIFT_CALL(0x4f1eb0u, 0x4f0853u);  // FUN_004f1eb0
    eax = rd32(m, esp + 0xcu);
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(0);
}

// 004f0860 Fn_004f0860
//   original FUN_004f0860; register inputs: eax esi; returns with ret 4
REFLEX_PORT_FN(0x004f0860, Fn_004f0860, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_4f0875 = 0;
    uint32_t fR_4f0888 = 0;
    uint32_t fA_4f088c = 0;
    uint32_t fB_4f088c = 0;
    uint32_t fA_4f0896 = 0;
    uint32_t fB_4f0896 = 0;
    uint32_t fR_4f08a0 = 0;
    uint32_t fA_4f08b0 = 0;
    uint32_t fB_4f08b0 = 0;
    uint32_t fR_4f08b5 = 0;
    esp = esp - 0x14u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, edi);
    edi = eax;
    eax = esi;
    LIFT_CALL(0x4f3d50u, 0x4f086fu);  // FUN_004f3d50
    ebx = eax;
    wr32(m, esp + 0x14u, ebx);
    fR_4f0875 = (esi & esi);
    if (!(fR_4f0875 == 0)) goto L_4f087f;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4f087fu);
L_4f087f:
    eax = rd32(m, esi);
    ebp = rd32(m, esi + 0x18u);
    wr32(m, esp + 0x10u, eax);
    fR_4f0888 = (eax & eax);
    if ((fR_4f0888 == 0)) goto L_4f0890;
    fA_4f088c = eax;
    fB_4f088c = eax;
    if (fA_4f088c == fB_4f088c) goto L_4f0896;
L_4f0890:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4f0896u);
L_4f0896:
    fA_4f0896 = ebx;
    fB_4f0896 = ebp;
    if (fA_4f0896 == fB_4f0896) goto L_4f08bf;
    eax = rd32(m, edi + 0x4u);
    ecx = (ebx + 0xcu);
    fR_4f08a0 = (eax & eax);
    if ((fR_4f08a0 == 0)) goto L_4f08ae;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x4ec2c0u, 0x4f08acu);  // FUN_004ec2c0
    goto L_4f08b5;
L_4f08ae:
    eax = rd32(m, edi);
    fA_4f08b0 = eax;
    fB_4f08b0 = rd32(m, ecx);
    eax = (eax & 0xffffff00u) | (((fA_4f08b0 < fB_4f08b0) ? 1u : 0u) & 0xffu);
L_4f08b5:
    fR_4f08b5 = ((eax & 0xffu) & (eax & 0xffu));
    if (!(fR_4f08b5 == 0)) goto L_4f08bf;
    eax = (esp + 0x10u);
    goto L_4f08d0;
L_4f08bf:
    ecx = rd32(m, esi + 0x18u);
    edx = rd32(m, esi);
    wr32(m, esp + 0x1cu, ecx);
    wr32(m, esp + 0x18u, edx);
    eax = (esp + 0x18u);
L_4f08d0:
    edx = rd32(m, eax);
    ecx = rd32(m, esp + 0x24u);
    eax = rd32(m, eax + 0x4u);
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    wr32(m, ecx + 0x4u, eax);
    wr32(m, ecx, edx);
    eax = ecx;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x14u;
    LIFT_RET(4);
}

// 004f1640 Fn_004f1640
//   original FUN_004f1640; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x004f1640, Fn_004f1640, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4f1670 = 0;
    uint32_t fB_4f1670 = 0;
    uint32_t fR_4f1680 = 0;
    uint32_t fA_4f169a = 0;
    uint32_t fB_4f169a = 0;
    uint32_t fR_4f1717 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp -= 4; wr32(m, esp, 0xffffffffu);
    esp -= 4; wr32(m, esp, 0x9032f0u);
    eax = rd32(m, FS_BASE);
    esp -= 4; wr32(m, esp, eax);
    esp = esp - 0x30u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ ebp);
    esp -= 4; wr32(m, esp, eax);
    eax = (ebp - 0xcu);
    wr32(m, FS_BASE, eax);
    wr32(m, ebp - 0x10u, esp);
    edi = ecx;
    ecx = rd32(m, ebp + 0x8u);
    fA_4f1670 = ecx;
    fB_4f1670 = 0x1745d17u;
    if (fA_4f1670 <= fB_4f1670) goto L_4f167d;
    LIFT_CALL(0x851560u, 0x4f167du);  // FUN_00851560
L_4f167d:
    eax = rd32(m, edi + 0xcu);
    fR_4f1680 = (eax & eax);
    if ((fR_4f1680 == 0)) goto L_4f169a;
    edx = rd32(m, edi + 0x14u);
    edx = edx - eax;
    eax = 0x2e8ba2e9u;
    { int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(edx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); }
    edx = uint32_t(int32_t(edx) >> 5);
    eax = edx;
    eax = (eax >> 31);
    eax = eax + edx;
L_4f169a:
    fA_4f169a = eax;
    fB_4f169a = ecx;
    if (fA_4f169a >= fB_4f169a) goto L_4f1757;
    LIFT_CALL(0x419cd0u, 0x4f16a7u);  // FUN_00419cd0
    wr32(m, ebp - 0x18u, eax);
    wr32(m, ebp - 0x4u, 0x0u);
    esi = (ebp - 0x30u);
    LIFT_CALL(0x4db590u, 0x4f16b9u);  // FUN_004db590
    ecx = rd32(m, eax);
    wr32(m, ebp - 0x20u, ecx);
    ebx = rd32(m, eax + 0x4u);
    esi = (ebp - 0x38u);
    LIFT_CALL(0x775700u, 0x4f16c9u);  // FUN_00775700
    edx = rd32(m, ebp - 0x20u);
    wr32(m, ebp - 0x20u, edx);
    wr32(m, ebp - 0x1cu, ebx);
    ecx = rd32(m, eax);
    wr32(m, ebp - 0x28u, ecx);
    eax = rd32(m, eax + 0x4u);
    wr32(m, ebp - 0x24u, eax);
    wr8(m, ebp - 0x1cu, 0x0u);
    edx = rd32(m, ebp - 0x1cu);
    esp -= 4; wr32(m, esp, edx);
    ecx = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, ecx);
    edx = rd32(m, ebp - 0x18u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x41b090u, 0x4f16f4u);  // FUN_0041b090
    esp = esp + 0x14u;
    wr32(m, ebp - 0x4u, 0xffffffffu);
    ecx = rd32(m, edi + 0x10u);
    ebx = rd32(m, edi + 0xcu);
    ecx = ecx - ebx;
    eax = 0x2e8ba2e9u;
    { int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); }
    edx = uint32_t(int32_t(edx) >> 5);
    esi = edx;
    esi = (esi >> 31);
    esi = esi + edx;
    fR_4f1717 = (ebx & ebx);
    if ((fR_4f1717 == 0)) goto L_4f1738;
    eax = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, edi + 0x10u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x41ac70u, 0x4f1729u);  // FUN_0041ac70
    esp = esp + 0xcu;
    ecx = rd32(m, edi + 0xcu);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x78fc50u, 0x4f1735u);  // ReflexAlignedFree
    esp = esp + 0x4u;
L_4f1738:
    eax = rd32(m, ebp + 0x8u);
    eax = uint32_t(int32_t(eax) * int32_t(0xb0u));
    ecx = rd32(m, ebp - 0x18u);
    eax = eax + ecx;
    wr32(m, edi + 0x14u, eax);
    esi = uint32_t(int32_t(esi) * int32_t(0xb0u));
    esi = esi + ecx;
    wr32(m, edi + 0x10u, esi);
    wr32(m, edi + 0xcu, ecx);
L_4f1757:
    ecx = rd32(m, ebp - 0xcu);
    wr32(m, FS_BASE, ecx);
    ecx = rd32(m, esp); esp += 4;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 004f17d0 Fn_004f17d0
//   original FUN_004f17d0; register inputs: esi; returns with ret 0
REFLEX_PORT_FN(0x004f17d0, Fn_004f17d0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_4f17d9 = 0;
    uint32_t fR_4f17e1 = 0;
    uint32_t fA_4f17ee = 0;
    uint32_t fB_4f17ee = 0;
    eax = rd32(m, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    fR_4f17d9 = (eax & eax);
    if (!(fR_4f17d9 == 0)) goto L_4f17e5;
    LIFT_CALL(edi, 0x4f17dfu);
    eax = rd32(m, esi);
    fR_4f17e1 = (eax & eax);
    if ((fR_4f17e1 == 0)) goto L_4f17e9;
L_4f17e5:
    eax = rd32(m, eax);
    goto L_4f17eb;
L_4f17e9:
    eax = 0u;
L_4f17eb:
    ecx = rd32(m, esi + 0x4u);
    fA_4f17ee = ecx;
    fB_4f17ee = rd32(m, eax + 0x10u);
    if (fA_4f17ee < fB_4f17ee) goto L_4f17fa;
    LIFT_CALL(edi, 0x4f17f5u);
    eax = rd32(m, esi + 0x4u);
    edi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_4f17fa:
    eax = ecx;
    edi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 004f1860 Fn_004f1860
//   original FUN_004f1860; register inputs: eax; returns with ret 12
REFLEX_PORT_FN(0x004f1860, Fn_004f1860, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4f186e = 0;
    uint32_t fB_4f186e = 0;
    uint32_t fA_4f1882 = 0;
    uint32_t fB_4f1882 = 0;
    uint32_t fA_4f1887 = 0;
    uint32_t fB_4f1887 = 0;
    uint32_t fR_4f18af = 0;
    uint32_t fA_4f18b3 = 0;
    uint32_t fB_4f18b3 = 0;
    uint32_t fA_4f18c7 = 0;
    uint32_t fB_4f18c7 = 0;
    uint32_t fA_4f18e1 = 0;
    uint32_t fB_4f18e1 = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    esi = eax;
    fA_4f186e = rd32(m, ebx + 0x14u);
    fB_4f186e = ebp;
    if (fA_4f186e >= fB_4f186e) goto L_4f1879;
    LIFT_CALL(rd32(m, 0x916174u) /* 8.282296100677829e-39 */, 0x4f1879u);
L_4f1879:
    edi = rd32(m, ebx + 0x14u);
    eax = rd32(m, esp + 0x1cu);
    edi = edi - ebp;
    fA_4f1882 = eax;
    fB_4f1882 = edi;
    if (fA_4f1882 < fB_4f1882) edi = eax;
    fA_4f1887 = esi;
    fB_4f1887 = ebx;
    if (fA_4f1887 != fB_4f1887) goto L_4f18a8;
    edi = edi + ebp;
    esp -= 4; wr32(m, esp, edi);
    eax = (eax | 0xffffffffu);
    LIFT_CALL(0x4f2d70u, 0x4f1896u);  // FUN_004f2d70
    esp -= 4; wr32(m, esp, 0x0u);
    eax = ebp;
    LIFT_CALL(0x4f2d70u, 0x4f189fu);  // FUN_004f2d70
    edi = rd32(m, esp); esp += 4;
    eax = esi;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(12);
L_4f18a8:
    eax = esi;
    LIFT_CALL(0x4f2df0u, 0x4f18afu);  // FUN_004f2df0
    fR_4f18af = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_4f18af == 0)) goto L_4f18f0;
    fA_4f18b3 = rd32(m, ebx + 0x18u);
    fB_4f18b3 = 0x10u;
    if (fA_4f18b3 < fB_4f18b3) goto L_4f18be;
    edx = rd32(m, ebx + 0x4u);
    goto L_4f18c1;
L_4f18be:
    edx = (ebx + 0x4u);
L_4f18c1:
    ecx = rd32(m, esi + 0x18u);
    ebx = (esi + 0x4u);
    fA_4f18c7 = ecx;
    fB_4f18c7 = 0x10u;
    if (fA_4f18c7 < fB_4f18c7) goto L_4f18d0;
    eax = rd32(m, ebx);
    goto L_4f18d2;
L_4f18d0:
    eax = ebx;
L_4f18d2:
    esp -= 4; wr32(m, esp, edi);
    edx = edx + ebp;
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x91616cu) /* 8.282391388973403e-39 */, 0x4f18deu);
    esp = esp + 0x10u;
    fA_4f18e1 = rd32(m, esi + 0x18u);
    fB_4f18e1 = 0x10u;
    wr32(m, esi + 0x14u, edi);
    if (fA_4f18e1 < fB_4f18e1) goto L_4f18ec;
    ebx = rd32(m, ebx);
L_4f18ec:
    wr8(m, ebx + edi, 0x0u);
L_4f18f0:
    edi = rd32(m, esp); esp += 4;
    eax = esi;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(12);
}

// 004f1eb0 Fn_004f1eb0
//   original FUN_004f1eb0; register inputs: eax; returns with ret 20
REFLEX_PORT_FN(0x004f1eb0, Fn_004f1eb0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_4f1ec4 = 0;
    uint32_t fA_4f1ec8 = 0;
    uint32_t fB_4f1ec8 = 0;
    uint32_t fA_4f1ed6 = 0;
    uint32_t fB_4f1ed6 = 0;
    uint32_t fR_4f1ee5 = 0;
    uint32_t fA_4f1ee9 = 0;
    uint32_t fB_4f1ee9 = 0;
    uint32_t fA_4f1ef7 = 0;
    uint32_t fB_4f1ef7 = 0;
    uint32_t fR_4f1f40 = 0;
    uint32_t fA_4f1f44 = 0;
    uint32_t fB_4f1f44 = 0;
    uint32_t fA_4f1f58 = 0;
    uint32_t fB_4f1f58 = 0;
    esp = esp - 0x8u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = eax;
    eax = rd32(m, edi + 0x18u);
    esi = rd32(m, eax);
    eax = rd32(m, edi);
    fR_4f1ec4 = (ebx & ebx);
    if ((fR_4f1ec4 == 0)) goto L_4f1ecc;
    fA_4f1ec8 = ebx;
    fB_4f1ec8 = eax;
    if (fA_4f1ec8 == fB_4f1ec8) goto L_4f1ed6;
L_4f1ecc:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4f1ed2u);
    ebx = rd32(m, esp + 0x20u);
L_4f1ed6:
    fA_4f1ed6 = rd32(m, esp + 0x24u);
    fB_4f1ed6 = esi;
    if (fA_4f1ed6 != fB_4f1ed6) goto L_4f1f40;
    ecx = rd32(m, esp + 0x28u);
    esi = rd32(m, edi + 0x18u);
    eax = rd32(m, edi);
    fR_4f1ee5 = (ecx & ecx);
    if ((fR_4f1ee5 == 0)) goto L_4f1eed;
    fA_4f1ee9 = ecx;
    fB_4f1ee9 = eax;
    if (fA_4f1ee9 == fB_4f1ee9) goto L_4f1ef7;
L_4f1eed:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4f1ef3u);
    ebx = rd32(m, esp + 0x20u);
L_4f1ef7:
    fA_4f1ef7 = rd32(m, esp + 0x2cu);
    fB_4f1ef7 = esi;
    if (fA_4f1ef7 != fB_4f1ef7) goto L_4f1f40;
    ecx = rd32(m, edi + 0x18u);
    edx = rd32(m, ecx + 0x4u);
    esp -= 4; wr32(m, esp, edx);
    ecx = edi;
    LIFT_CALL(0x4f2020u, 0x4f1f0bu);  // FUN_004f2020
    eax = rd32(m, edi + 0x18u);
    wr32(m, eax + 0x4u, eax);
    eax = rd32(m, edi + 0x18u);
    wr32(m, edi + 0x1cu, 0x0u);
    wr32(m, eax, eax);
    eax = rd32(m, edi + 0x18u);
    wr32(m, eax + 0x8u, eax);
    eax = rd32(m, edi + 0x18u);
    ecx = rd32(m, eax);
    eax = rd32(m, esp + 0x1cu);
    edx = rd32(m, edi);
    wr32(m, eax + 0x4u, ecx);
    wr32(m, eax, edx);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x8u;
    LIFT_RET(20);
L_4f1f40:
    fR_4f1f40 = (ebx & ebx);
    if ((fR_4f1f40 == 0)) goto L_4f1f4a;
    fA_4f1f44 = ebx;
    fB_4f1f44 = rd32(m, esp + 0x28u);
    if (fA_4f1f44 == fB_4f1f44) goto L_4f1f54;
L_4f1f4a:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4f1f50u);
    ebx = rd32(m, esp + 0x20u);
L_4f1f54:
    ebp = rd32(m, esp + 0x24u);
    fA_4f1f58 = ebp;
    fB_4f1f58 = rd32(m, esp + 0x2cu);
    if (fA_4f1f58 == fB_4f1f58) goto L_4f1f7a;
    esi = (esp + 0x20u);
    LIFT_CALL(0x417df0u, 0x4f1f67u);  // FUN_00417df0
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, ebx);
    eax = (esp + 0x18u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x4f38d0u, 0x4f1f74u);  // FUN_004f38d0
    ebx = rd32(m, esp + 0x20u);
    goto L_4f1f40;
L_4f1f7a:
    edi = rd32(m, edi);
    eax = rd32(m, esp + 0x1cu);
    wr32(m, eax, edi);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    wr32(m, eax + 0x4u, ebp);
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x8u;
    LIFT_RET(20);
}

// 004f1f90 Fn_004f1f90
//   original FUN_004f1f90; register inputs: eax; returns with ret 4
REFLEX_PORT_FN(0x004f1f90, Fn_004f1f90, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4f1fbc = 0;
    uint32_t fB_4f1fbc = 0;
    uint32_t fA_4f1fc4 = 0;
    uint32_t fB_4f1fc4 = 0;
    uint32_t fA_4f1fd4 = 0;
    uint32_t fB_4f1fd4 = 0;
    uint32_t fA_4f1fe5 = 0;
    uint32_t fB_4f1fe5 = 0;
    uint32_t fA_4f1ff5 = 0;
    uint32_t fB_4f1ff5 = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    eax = rd32(m, ebx + 0x18u);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esi + 0x18u);
    eax = eax + 0x4u;
    eax = rd32(m, eax);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    ecx = esi;
    LIFT_CALL(0x4f3ba0u, 0x4f1fadu);  // FUN_004f3ba0
    wr32(m, edi + 0x4u, eax);
    ecx = rd32(m, ebx + 0x1cu);
    edx = rd32(m, esi + 0x18u);
    wr32(m, esi + 0x1cu, ecx);
    eax = rd32(m, edx + 0x4u);
    fA_4f1fbc = rd8(m, eax + 0x19u);
    fB_4f1fbc = 0x0u;
    if (fA_4f1fbc != fB_4f1fbc) goto L_4f2004;
    ecx = rd32(m, eax);
    fA_4f1fc4 = rd8(m, ecx + 0x19u);
    fB_4f1fc4 = 0x0u;
    if (fA_4f1fc4 != fB_4f1fc4) goto L_4f1fda;
L_4f1fd0:
    eax = ecx;
    ecx = rd32(m, eax);
    fA_4f1fd4 = rd8(m, ecx + 0x19u);
    fB_4f1fd4 = 0x0u;
    if (fA_4f1fd4 == fB_4f1fd4) goto L_4f1fd0;
L_4f1fda:
    wr32(m, edx, eax);
    esi = rd32(m, esi + 0x18u);
    ecx = rd32(m, esi + 0x4u);
    eax = rd32(m, ecx + 0x8u);
    fA_4f1fe5 = rd8(m, eax + 0x19u);
    fB_4f1fe5 = 0x0u;
    if (fA_4f1fe5 != fB_4f1fe5) goto L_4f1ffb;
    goto L_4f1ff0;
L_4f1ff0:
    ecx = eax;
    eax = rd32(m, ecx + 0x8u);
    fA_4f1ff5 = rd8(m, eax + 0x19u);
    fB_4f1ff5 = 0x0u;
    if (fA_4f1ff5 == fB_4f1ff5) goto L_4f1ff0;
L_4f1ffb:
    edi = rd32(m, esp); esp += 4;
    wr32(m, esi + 0x8u, ecx);
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_4f2004:
    wr32(m, edx, edx);
    esi = rd32(m, esi + 0x18u);
    edi = rd32(m, esp); esp += 4;
    wr32(m, esi + 0x8u, esi);
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 004f2020 Fn_004f2020
//   original FUN_004f2020; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x004f2020, Fn_004f2020, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4f2027 = 0;
    uint32_t fB_4f2027 = 0;
    uint32_t fA_4f204a = 0;
    uint32_t fB_4f204a = 0;
    uint32_t fA_4f2064 = 0;
    uint32_t fB_4f2064 = 0;
    uint32_t fA_4f2078 = 0;
    uint32_t fB_4f2078 = 0;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esp + 0x10u);
    fA_4f2027 = rd8(m, edi + 0x19u);
    fB_4f2027 = 0x0u;
    ebx = ecx;
    esi = edi;
    if (fA_4f2027 != fB_4f2027) goto L_4f2081;
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, 0x916350u) /* 8.287536956934404e-39 */;
L_4f2038:
    eax = rd32(m, esi + 0x8u);
    esp -= 4; wr32(m, esp, eax);
    ecx = ebx;
    LIFT_CALL(0x4f2020u, 0x4f2043u);  // FUN_004f2020
    esi = rd32(m, esi);
    LIFT_CALL(0x4dc0f0u, 0x4f204au);  // FUN_004dc0f0
    fA_4f204a = rd8(m, 0xda67d8u);
    fB_4f204a = 0x0u;
    if (fA_4f204a == fB_4f204a) goto L_4f205e;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x4f205eu);
L_4f205e:
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(ebp, 0x4f2061u);
    esp = esp + 0x4u;
    fA_4f2064 = rd8(m, 0xda67d8u);
    fB_4f2064 = 0x0u;
    if (fA_4f2064 == fB_4f2064) goto L_4f2078;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4f2078u);
L_4f2078:
    fA_4f2078 = rd8(m, esi + 0x19u);
    fB_4f2078 = 0x0u;
    edi = esi;
    if (fA_4f2078 == fB_4f2078) goto L_4f2038;
    ebp = rd32(m, esp); esp += 4;
L_4f2081:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 004f2090 Fn_004f2090
//   original FUN_004f2090; register inputs: eax edi; returns with ret 12
REFLEX_PORT_FN(0x004f2090, Fn_004f2090, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4f2093 = 0;
    uint32_t fB_4f2093 = 0;
    uint32_t fA_4f2106 = 0;
    uint32_t fB_4f2106 = 0;
    uint32_t fA_4f2117 = 0;
    uint32_t fB_4f2117 = 0;
    uint32_t fA_4f2123 = 0;
    uint32_t fB_4f2123 = 0;
    uint32_t fA_4f2131 = 0;
    uint32_t fB_4f2131 = 0;
    uint32_t fA_4f213c = 0;
    uint32_t fB_4f213c = 0;
    uint32_t fA_4f2155 = 0;
    uint32_t fB_4f2155 = 0;
    uint32_t fA_4f215c = 0;
    uint32_t fB_4f215c = 0;
    uint32_t fA_4f2178 = 0;
    uint32_t fB_4f2178 = 0;
    uint32_t fA_4f21a5 = 0;
    uint32_t fB_4f21a5 = 0;
    uint32_t fA_4f21c1 = 0;
    uint32_t fB_4f21c1 = 0;
    uint32_t fA_4f21ec = 0;
    uint32_t fB_4f21ec = 0;
    esp = esp - 0x44u;
    fA_4f2093 = rd32(m, edi + 0x1cu);
    fB_4f2093 = 0x15555554u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    if (fA_4f2093 < fB_4f2093) goto L_4f20e8;
    esp -= 4; wr32(m, esp, 0x937064u);
    ecx = (esp + 0x10u);
    LIFT_CALL(rd32(m, 0x9161acu) /* 8.280765882754786e-39 */, 0x4f20b0u);
    ecx = (esp + 0x28u);
    LIFT_CALL(rd32(m, 0x916210u) /* 8.286239354556439e-39 */, 0x4f20bau);
    eax = (esp + 0xcu);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0x38u);
    wr32(m, esp + 0x2cu, 0x9177c4u);
    LIFT_CALL(rd32(m, 0x9161b0u) /* 8.28065377887764e-39 */, 0x4f20d1u);
    esp -= 4; wr32(m, esp, 0x99bc84u);
    ecx = (esp + 0x2cu);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp + 0x30u, 0x9177d0u);
    LIFT_CALL(0x8fa07cu, 0x4f20e8u);  // MSVCR90.DLL::_CxxThrowException
L_4f20e8:
    eax = rd32(m, edi + 0x18u);
    ecx = rd32(m, esp + 0x5cu);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x4f3e60u, 0x4f20f9u);  // FUN_004f3e60
    ebp = eax;
    eax = rd32(m, edi + 0x18u);
    ebx = 0x1u;
    wr32(m, edi + 0x1cu, rd32(m, edi + 0x1cu) + ebx);
    fA_4f2106 = esi;
    fB_4f2106 = eax;
    if (fA_4f2106 != fB_4f2106) goto L_4f2117;
    wr32(m, eax + 0x4u, ebp);
    edx = rd32(m, edi + 0x18u);
    wr32(m, edx, ebp);
    eax = rd32(m, edi + 0x18u);
    goto L_4f2136;
L_4f2117:
    fA_4f2117 = rd8(m, esp + 0x58u);
    fB_4f2117 = 0x0u;
    if (fA_4f2117 == fB_4f2117) goto L_4f212b;
    wr32(m, esi, ebp);
    eax = rd32(m, edi + 0x18u);
    fA_4f2123 = esi;
    fB_4f2123 = rd32(m, eax);
    if (fA_4f2123 != fB_4f2123) goto L_4f2139;
    wr32(m, eax, ebp);
    goto L_4f2139;
L_4f212b:
    wr32(m, esi + 0x8u, ebp);
    eax = rd32(m, edi + 0x18u);
    fA_4f2131 = esi;
    fB_4f2131 = rd32(m, eax + 0x8u);
    if (fA_4f2131 != fB_4f2131) goto L_4f2139;
L_4f2136:
    wr32(m, eax + 0x8u, ebp);
L_4f2139:
    ecx = rd32(m, ebp + 0x4u);
    fA_4f213c = rd8(m, ecx + 0x18u);
    fB_4f213c = 0x0u;
    eax = (ebp + 0x4u);
    esi = ebp;
    if (fA_4f213c != fB_4f213c) goto L_4f21f9;
    goto L_4f2150;
L_4f2150:
    ecx = rd32(m, eax);
    edx = rd32(m, ecx + 0x4u);
    fA_4f2155 = ecx;
    fB_4f2155 = rd32(m, edx);
    if (fA_4f2155 != fB_4f2155) goto L_4f21a3;
    edx = rd32(m, edx + 0x8u);
    fA_4f215c = rd8(m, edx + 0x18u);
    fB_4f215c = 0x0u;
    if (fA_4f215c != fB_4f215c) goto L_4f2178;
    wr8(m, ecx + 0x18u, (ebx & 0xffu));
    wr8(m, edx + 0x18u, (ebx & 0xffu));
    edx = rd32(m, eax);
    ecx = rd32(m, edx + 0x4u);
    wr8(m, ecx + 0x18u, 0x0u);
    edx = rd32(m, eax);
    esi = rd32(m, edx + 0x4u);
    goto L_4f21e9;
L_4f2178:
    fA_4f2178 = esi;
    fB_4f2178 = rd32(m, ecx + 0x8u);
    if (fA_4f2178 != fB_4f2178) goto L_4f2185;
    esp -= 4; wr32(m, esp, edi);
    esi = ecx;
    LIFT_CALL(0x4f3da0u, 0x4f2185u);  // FUN_004f3da0
L_4f2185:
    eax = rd32(m, esi + 0x4u);
    wr8(m, eax + 0x18u, (ebx & 0xffu));
    ecx = rd32(m, esi + 0x4u);
    edx = rd32(m, ecx + 0x4u);
    wr8(m, edx + 0x18u, 0x0u);
    eax = rd32(m, esi + 0x4u);
    ecx = rd32(m, eax + 0x4u);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x4f3e10u, 0x4f21a1u);  // FUN_004f3e10
    goto L_4f21e9;
L_4f21a3:
    edx = rd32(m, edx);
    fA_4f21a5 = rd8(m, edx + 0x18u);
    fB_4f21a5 = 0x0u;
    if (fA_4f21a5 != fB_4f21a5) goto L_4f21c1;
    wr8(m, ecx + 0x18u, (ebx & 0xffu));
    wr8(m, edx + 0x18u, (ebx & 0xffu));
    edx = rd32(m, eax);
    ecx = rd32(m, edx + 0x4u);
    wr8(m, ecx + 0x18u, 0x0u);
    edx = rd32(m, eax);
    esi = rd32(m, edx + 0x4u);
    goto L_4f21e9;
L_4f21c1:
    fA_4f21c1 = esi;
    fB_4f21c1 = rd32(m, ecx);
    if (fA_4f21c1 != fB_4f21c1) goto L_4f21cd;
    esp -= 4; wr32(m, esp, edi);
    esi = ecx;
    LIFT_CALL(0x4f3e10u, 0x4f21cdu);  // FUN_004f3e10
L_4f21cd:
    eax = rd32(m, esi + 0x4u);
    wr8(m, eax + 0x18u, (ebx & 0xffu));
    ecx = rd32(m, esi + 0x4u);
    edx = rd32(m, ecx + 0x4u);
    wr8(m, edx + 0x18u, 0x0u);
    eax = rd32(m, esi + 0x4u);
    ecx = rd32(m, eax + 0x4u);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x4f3da0u, 0x4f21e9u);  // FUN_004f3da0
L_4f21e9:
    ecx = rd32(m, esi + 0x4u);
    fA_4f21ec = rd8(m, ecx + 0x18u);
    fB_4f21ec = 0x0u;
    eax = (esi + 0x4u);
    if (fA_4f21ec == fB_4f21ec) goto L_4f2150;
L_4f21f9:
    edx = rd32(m, edi + 0x18u);
    eax = rd32(m, edx + 0x4u);
    wr8(m, eax + 0x18u, (ebx & 0xffu));
    eax = rd32(m, esp + 0x54u);
    ecx = rd32(m, edi);
    esi = rd32(m, esp); esp += 4;
    wr32(m, eax + 0x4u, ebp);
    ebp = rd32(m, esp); esp += 4;
    wr32(m, eax, ecx);
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x44u;
    LIFT_RET(12);
}

// 004f2220 Fn_004f2220
//   original FUN_004f2220; register inputs: esi; returns with ret 0
REFLEX_PORT_FN(0x004f2220, Fn_004f2220, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4f224c = 0;
    uint32_t fB_4f224c = 0;
    uint32_t fA_4f2269 = 0;
    uint32_t fB_4f2269 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    eax = rd32(m, esi + 0x18u);
    ecx = rd32(m, esi);
    edx = rd32(m, eax);
    esp = esp - 0x8u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, ecx);
    eax = (esp + 0x18u);
    esp -= 4; wr32(m, esp, eax);
    eax = esi;
    LIFT_CALL(0x4f1eb0u, 0x4f2242u);  // FUN_004f1eb0
    edi = rd32(m, esi + 0x18u);
    LIFT_CALL(0x4dc0f0u, 0x4f224au);  // FUN_004dc0f0
    ebx = 0u;
    fA_4f224c = rd8(m, 0xda67d8u);
    fB_4f224c = (ebx & 0xffu);
    if (fA_4f224c == fB_4f224c) goto L_4f225f;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x4f225fu);
L_4f225f:
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(rd32(m, 0x916350u) /* 8.287536956934404e-39 */, 0x4f2266u);
    esp = esp + 0x4u;
    fA_4f2269 = rd8(m, 0xda67d8u);
    fB_4f2269 = (ebx & 0xffu);
    if (fA_4f2269 == fB_4f2269) goto L_4f227c;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4f227cu);
L_4f227c:
    edi = rd32(m, esp); esp += 4;
    wr32(m, esi + 0x1cu, ebx);
    wr32(m, esi + 0x18u, ebx);
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 004f2c40 Fn_004f2c40
//   original FUN_004f2c40; register inputs: ecx esi edi; returns with ret 16
REFLEX_PORT_FN(0x004f2c40, Fn_004f2c40, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_4f2c4f = 0;
    uint32_t fA_4f2c57 = 0;
    uint32_t fB_4f2c57 = 0;
    uint32_t fA_4f2c5c = 0;
    uint32_t fB_4f2c5c = 0;
    uint32_t fA_4f2c72 = 0;
    uint32_t fB_4f2c72 = 0;
    uint32_t fA_4f2c77 = 0;
    uint32_t fB_4f2c77 = 0;
    uint32_t fR_4f2c86 = 0;
    uint32_t fA_4f2c8a = 0;
    uint32_t fB_4f2c8a = 0;
    uint32_t fA_4f2c93 = 0;
    uint32_t fB_4f2c93 = 0;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    wr32(m, edi, 0x0u);
    fR_4f2c4f = (esi & esi);
    if ((fR_4f2c4f == 0)) goto L_4f2c61;
    eax = rd32(m, esp + 0x14u);
    fA_4f2c57 = rd32(m, esi + 0xcu);
    fB_4f2c57 = eax;
    if (fA_4f2c57 > fB_4f2c57) goto L_4f2c61;
    fA_4f2c5c = eax;
    fB_4f2c5c = rd32(m, esi + 0x10u);
    if (fA_4f2c5c <= fB_4f2c5c) goto L_4f2c67;
L_4f2c61:
    LIFT_CALL(ebp, 0x4f2c63u);
    eax = rd32(m, esp + 0x14u);
L_4f2c67:
    ebx = rd32(m, esp + 0x1cu);
    ecx = rd32(m, esi);
    wr32(m, edi, ecx);
    wr32(m, edi + 0x4u, eax);
    fA_4f2c72 = rd32(m, esi + 0xcu);
    fB_4f2c72 = ebx;
    if (fA_4f2c72 > fB_4f2c72) goto L_4f2c7c;
    fA_4f2c77 = ebx;
    fB_4f2c77 = rd32(m, esi + 0x10u);
    if (fA_4f2c77 <= fB_4f2c77) goto L_4f2c82;
L_4f2c7c:
    LIFT_CALL(ebp, 0x4f2c7eu);
    ebx = rd32(m, esp + 0x1cu);
L_4f2c82:
    eax = rd32(m, edi);
    ecx = rd32(m, esi);
    fR_4f2c86 = (eax & eax);
    if ((fR_4f2c86 == 0)) goto L_4f2c8e;
    fA_4f2c8a = eax;
    fB_4f2c8a = ecx;
    if (fA_4f2c8a == fB_4f2c8a) goto L_4f2c90;
L_4f2c8e:
    LIFT_CALL(ebp, 0x4f2c90u);
L_4f2c90:
    ecx = rd32(m, edi + 0x4u);
    fA_4f2c93 = ecx;
    fB_4f2c93 = ebx;
    if (fA_4f2c93 == fB_4f2c93) goto L_4f2cd1;
    eax = rd32(m, esi + 0x10u);
    wr8(m, esp + 0x8u, 0x0u);
    edx = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, edx);
    edx = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, edx);
    edx = rd32(m, esp + 0x18u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, eax);
    eax = ebx;
    LIFT_CALL(0x507530u, 0x4f2cb7u);  // FUN_00507530
    ecx = rd32(m, esi + 0x10u);
    ebx = eax;
    eax = rd32(m, esp + 0x24u);
    esp = esp + 0x14u;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x41ac70u, 0x4f2ccbu);  // FUN_0041ac70
    esp = esp + 0xcu;
    wr32(m, esi + 0x10u, ebx);
L_4f2cd1:
    ebp = rd32(m, esp); esp += 4;
    eax = edi;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(16);
}

// 004f2ce0 Fn_004f2ce0
//   original FUN_004f2ce0; register inputs: eax esi; returns with ret 4
REFLEX_PORT_FN(0x004f2ce0, Fn_004f2ce0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_4f2ce9 = 0;
    uint32_t fA_4f2cf3 = 0;
    uint32_t fB_4f2cf3 = 0;
    uint32_t fA_4f2cfe = 0;
    uint32_t fB_4f2cfe = 0;
    uint32_t fA_4f2d02 = 0;
    uint32_t fB_4f2d02 = 0;
    uint32_t fA_4f2d12 = 0;
    uint32_t fB_4f2d12 = 0;
    uint32_t fA_4f2d16 = 0;
    uint32_t fB_4f2d16 = 0;
    uint32_t fR_4f2d36 = 0;
    uint32_t fA_4f2d40 = 0;
    uint32_t fB_4f2d40 = 0;
    uint32_t fA_4f2d58 = 0;
    uint32_t fB_4f2d58 = 0;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, edi);
    edi = eax;
    fR_4f2ce9 = (ebp & ebp);
    if ((fR_4f2ce9 == 0)) goto L_4f2d2f;
    edx = rd32(m, esi + 0x18u);
    eax = (esi + 0x4u);
    fA_4f2cf3 = edx;
    fB_4f2cf3 = 0x10u;
    if (fA_4f2cf3 < fB_4f2cf3) goto L_4f2cfc;
    ecx = rd32(m, eax);
    goto L_4f2cfe;
L_4f2cfc:
    ecx = eax;
L_4f2cfe:
    fA_4f2cfe = ebp;
    fB_4f2cfe = ecx;
    if (fA_4f2cfe < fB_4f2cfe) goto L_4f2d2f;
    fA_4f2d02 = edx;
    fB_4f2d02 = 0x10u;
    if (fA_4f2d02 < fB_4f2d02) goto L_4f2d0b;
    ecx = rd32(m, eax);
    goto L_4f2d0d;
L_4f2d0b:
    ecx = eax;
L_4f2d0d:
    ebx = rd32(m, esi + 0x14u);
    ebx = ebx + ecx;
    fA_4f2d12 = ebx;
    fB_4f2d12 = ebp;
    if (fA_4f2d12 <= fB_4f2d12) goto L_4f2d2f;
    fA_4f2d16 = edx;
    fB_4f2d16 = 0x10u;
    if (fA_4f2d16 < fB_4f2d16) goto L_4f2d1d;
    eax = rd32(m, eax);
L_4f2d1d:
    esp -= 4; wr32(m, esp, edi);
    ebp = ebp - eax;
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    eax = esi;
    LIFT_CALL(0x4f1860u, 0x4f2d29u);  // FUN_004f1860
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_4f2d2f:
    eax = esi;
    LIFT_CALL(0x4f2df0u, 0x4f2d36u);  // FUN_004f2df0
    fR_4f2d36 = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_4f2d36 == 0)) goto L_4f2d67;
    ecx = rd32(m, esi + 0x18u);
    ebx = (esi + 0x4u);
    fA_4f2d40 = ecx;
    fB_4f2d40 = 0x10u;
    if (fA_4f2d40 < fB_4f2d40) goto L_4f2d49;
    eax = rd32(m, ebx);
    goto L_4f2d4b;
L_4f2d49:
    eax = ebx;
L_4f2d4b:
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x91616cu) /* 8.282391388973403e-39 */, 0x4f2d55u);
    esp = esp + 0x10u;
    fA_4f2d58 = rd32(m, esi + 0x18u);
    fB_4f2d58 = 0x10u;
    wr32(m, esi + 0x14u, edi);
    if (fA_4f2d58 < fB_4f2d58) goto L_4f2d63;
    ebx = rd32(m, ebx);
L_4f2d63:
    wr8(m, ebx + edi, 0x0u);
L_4f2d67:
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = esi;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 004f2df0 Fn_004f2df0
//   original FUN_004f2df0; register inputs: eax edi; returns with ret 0
REFLEX_PORT_FN(0x004f2df0, Fn_004f2df0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4f2df3 = 0;
    uint32_t fB_4f2df3 = 0;
    uint32_t fA_4f2e01 = 0;
    uint32_t fB_4f2e01 = 0;
    uint32_t fA_4f2e12 = 0;
    uint32_t fB_4f2e12 = 0;
    uint32_t fR_4f2e1a = 0;
    uint32_t fA_4f2e21 = 0;
    uint32_t fB_4f2e21 = 0;
    uint32_t fA_4f2e2b = 0;
    uint32_t fB_4f2e2b = 0;
    uint32_t fA_4f2e3e = 0;
    uint32_t fB_4f2e3e = 0;
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    fA_4f2df3 = edi;
    fB_4f2df3 = 0xfffffffeu;
    if (fA_4f2df3 <= fB_4f2df3) goto L_4f2dfe;
    LIFT_CALL(rd32(m, 0x916170u) /* 8.282343744825616e-39 */, 0x4f2dfeu);
L_4f2dfe:
    eax = rd32(m, esi + 0x18u);
    fA_4f2e01 = eax;
    fB_4f2e01 = edi;
    if (fA_4f2e01 >= fB_4f2e01) goto L_4f2e1a;
    eax = rd32(m, esi + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x4f48e0u, 0x4f2e10u);  // FUN_004f48e0
    ecx = 0u;
    fA_4f2e12 = ecx;
    fB_4f2e12 = edi;
    eax = eax - eax - (fA_4f2e12 < fB_4f2e12 ? 1u : 0u);
    eax = (0u - eax);
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_4f2e1a:
    fR_4f2e1a = (edi & edi);
    if (!(fR_4f2e1a == 0)) goto L_4f2e3c;
    wr32(m, esi + 0x14u, edi);
    fA_4f2e21 = eax;
    fB_4f2e21 = 0x10u;
    if (fA_4f2e21 < fB_4f2e21) goto L_4f2e36;
    esi = rd32(m, esi + 0x4u);
    ecx = 0u;
    fA_4f2e2b = ecx;
    fB_4f2e2b = edi;
    eax = eax - eax - (fA_4f2e2b < fB_4f2e2b ? 1u : 0u);
    wr8(m, esi, 0x0u);
    eax = (0u - eax);
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_4f2e36:
    esi = esi + 0x4u;
    wr8(m, esi, 0x0u);
L_4f2e3c:
    ecx = 0u;
    fA_4f2e3e = ecx;
    fB_4f2e3e = edi;
    eax = eax - eax - (fA_4f2e3e < fB_4f2e3e ? 1u : 0u);
    eax = (0u - eax);
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 004f3ba0 Fn_004f3ba0
//   original FUN_004f3ba0; register inputs: ecx; returns with ret 8
REFLEX_PORT_FN(0x004f3ba0, Fn_004f3ba0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4f3bd9 = 0;
    uint32_t fB_4f3bd9 = 0;
    uint32_t fA_4f3bf7 = 0;
    uint32_t fB_4f3bf7 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp -= 4; wr32(m, esp, 0xffffffffu);
    esp -= 4; wr32(m, esp, 0x902a20u);
    eax = rd32(m, FS_BASE);
    esp -= 4; wr32(m, esp, eax);
    esp = esp - 0xcu;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ ebp);
    esp -= 4; wr32(m, esp, eax);
    eax = (ebp - 0xcu);
    wr32(m, FS_BASE, eax);
    wr32(m, ebp - 0x10u, esp);
    ebx = ecx;
    wr32(m, ebp - 0x18u, ebx);
    eax = rd32(m, ebx + 0x18u);
    wr32(m, ebp - 0x14u, eax);
    edi = rd32(m, ebp + 0x8u);
    fA_4f3bd9 = rd8(m, edi + 0x19u);
    fB_4f3bd9 = 0x0u;
    if (fA_4f3bd9 != fB_4f3bd9) goto L_4f3c2a;
    ecx = rd8(m, edi + 0x18u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = (edi + 0xcu);
    esp -= 4; wr32(m, esp, eax);
    edx = rd32(m, ebp + 0xcu);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x4f3e60u, 0x4f3bf2u);  // FUN_004f3e60
    esi = eax;
    eax = rd32(m, ebp - 0x14u);
    fA_4f3bf7 = rd8(m, eax + 0x19u);
    fB_4f3bf7 = 0x0u;
    if (fA_4f3bf7 == fB_4f3bf7) goto L_4f3c00;
    wr32(m, ebp - 0x14u, esi);
L_4f3c00:
    wr32(m, ebp - 0x4u, 0x0u);
    esp -= 4; wr32(m, esp, esi);
    ecx = rd32(m, edi);
    esp -= 4; wr32(m, esp, ecx);
    ecx = ebx;
    LIFT_CALL(0x4f3ba0u, 0x4f3c12u);  // FUN_004f3ba0
    wr32(m, esi, eax);
    esp -= 4; wr32(m, esp, esi);
    edx = rd32(m, edi + 0x8u);
    esp -= 4; wr32(m, esp, edx);
    ecx = ebx;
    LIFT_CALL(0x4f3ba0u, 0x4f3c20u);  // FUN_004f3ba0
    wr32(m, esi + 0x8u, eax);
    wr32(m, ebp - 0x4u, 0xffffffffu);
L_4f3c2a:
    eax = rd32(m, ebp - 0x14u);
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

// 004f3c60 Fn_004f3c60
//   original FUN_004f3c60; register inputs: edi; returns with ret 8
REFLEX_PORT_FN(0x004f3c60, Fn_004f3c60, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4f3c6e = 0;
    uint32_t fB_4f3c6e = 0;
    uint32_t fR_4f3c86 = 0;
    uint32_t fA_4f3c96 = 0;
    uint32_t fB_4f3c96 = 0;
    uint32_t fR_4f3c9b = 0;
    uint32_t fA_4f3ca4 = 0;
    uint32_t fB_4f3ca4 = 0;
    uint32_t fR_4f3cad = 0;
    uint32_t fA_4f3cbd = 0;
    uint32_t fB_4f3cbd = 0;
    uint32_t fR_4f3cc2 = 0;
    uint32_t fA_4f3cce = 0;
    uint32_t fB_4f3cce = 0;
    uint32_t fA_4f3cd4 = 0;
    uint32_t fB_4f3cd4 = 0;
    uint32_t fA_4f3ce9 = 0;
    uint32_t fB_4f3ce9 = 0;
    uint32_t fR_4f3cf6 = 0;
    uint32_t fA_4f3d06 = 0;
    uint32_t fB_4f3d06 = 0;
    uint32_t fR_4f3d0b = 0;
    uint32_t fA_4f3d18 = 0;
    uint32_t fB_4f3d18 = 0;
    esp = esp - 0x8u;
    eax = rd32(m, esp + 0xcu);
    eax = rd32(m, eax + 0x18u);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, eax + 0x4u);
    fA_4f3c6e = rd8(m, ebx + 0x19u);
    fB_4f3c6e = 0x0u;
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    wr32(m, esp + 0x10u, eax);
    ebp = eax;
    if (fA_4f3c6e != fB_4f3c6e) goto L_4f3cd4;
L_4f3c80:
    eax = rd32(m, ebx + 0x10u);
    esi = (ebx + 0xcu);
    fR_4f3c86 = (eax & eax);
    if ((fR_4f3c86 == 0)) goto L_4f3c94;
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x4ec2c0u, 0x4f3c92u);  // FUN_004ec2c0
    goto L_4f3c9b;
L_4f3c94:
    ecx = rd32(m, esi);
    fA_4f3c96 = ecx;
    fB_4f3c96 = rd32(m, edi);
    eax = (eax & 0xffffff00u) | (((fA_4f3c96 < fB_4f3c96) ? 1u : 0u) & 0xffu);
L_4f3c9b:
    fR_4f3c9b = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_4f3c9b == 0)) goto L_4f3ca4;
    ebx = rd32(m, ebx + 0x8u);
    goto L_4f3cce;
L_4f3ca4:
    fA_4f3ca4 = rd8(m, ebp + 0x19u);
    fB_4f3ca4 = 0x0u;
    if (fA_4f3ca4 == fB_4f3ca4) goto L_4f3cc8;
    eax = rd32(m, edi + 0x4u);
    fR_4f3cad = (eax & eax);
    if ((fR_4f3cad == 0)) goto L_4f3cbb;
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x4ec2c0u, 0x4f3cb9u);  // FUN_004ec2c0
    goto L_4f3cc2;
L_4f3cbb:
    edx = rd32(m, edi);
    fA_4f3cbd = edx;
    fB_4f3cbd = rd32(m, esi);
    eax = (eax & 0xffffff00u) | (((fA_4f3cbd < fB_4f3cbd) ? 1u : 0u) & 0xffu);
L_4f3cc2:
    fR_4f3cc2 = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_4f3cc2 == 0)) goto L_4f3cc8;
    ebp = ebx;
L_4f3cc8:
    wr32(m, esp + 0x10u, ebx);
    ebx = rd32(m, ebx);
L_4f3cce:
    fA_4f3cce = rd8(m, ebx + 0x19u);
    fB_4f3cce = 0x0u;
    if (fA_4f3cce == fB_4f3cce) goto L_4f3c80;
L_4f3cd4:
    fA_4f3cd4 = rd8(m, ebp + 0x19u);
    fB_4f3cd4 = 0x0u;
    if (fA_4f3cd4 == fB_4f3cd4) goto L_4f3ce6;
    eax = rd32(m, esp + 0x18u);
    ecx = rd32(m, eax + 0x18u);
    esi = rd32(m, ecx + 0x4u);
    goto L_4f3ce9;
L_4f3ce6:
    esi = rd32(m, ebp);
L_4f3ce9:
    fA_4f3ce9 = rd8(m, esi + 0x19u);
    fB_4f3ce9 = 0x0u;
    if (fA_4f3ce9 != fB_4f3ce9) goto L_4f3d1e;
L_4f3cf0:
    eax = rd32(m, edi + 0x4u);
    ecx = (esi + 0xcu);
    fR_4f3cf6 = (eax & eax);
    if ((fR_4f3cf6 == 0)) goto L_4f3d04;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x4ec2c0u, 0x4f3d02u);  // FUN_004ec2c0
    goto L_4f3d0b;
L_4f3d04:
    edx = rd32(m, edi);
    fA_4f3d06 = edx;
    fB_4f3d06 = rd32(m, ecx);
    eax = (eax & 0xffffff00u) | (((fA_4f3d06 < fB_4f3d06) ? 1u : 0u) & 0xffu);
L_4f3d0b:
    fR_4f3d0b = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_4f3d0b == 0)) goto L_4f3d15;
    ebp = esi;
    esi = rd32(m, esi);
    goto L_4f3d18;
L_4f3d15:
    esi = rd32(m, esi + 0x8u);
L_4f3d18:
    fA_4f3d18 = rd8(m, esi + 0x19u);
    fB_4f3d18 = 0x0u;
    if (fA_4f3d18 == fB_4f3d18) goto L_4f3cf0;
L_4f3d1e:
    ecx = rd32(m, esp + 0x1cu);
    eax = rd32(m, esp + 0x18u);
    eax = rd32(m, eax);
    edx = rd32(m, esp + 0x10u);
    esi = rd32(m, esp); esp += 4;
    wr32(m, ecx + 0xcu, ebp);
    ebp = rd32(m, esp); esp += 4;
    wr32(m, ecx, eax);
    wr32(m, ecx + 0x8u, eax);
    wr32(m, ecx + 0x4u, edx);
    eax = ecx;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x8u;
    LIFT_RET(8);
}

// 004f3d50 Fn_004f3d50
//   original FUN_004f3d50; register inputs: ecx eax edi; returns with ret 0
REFLEX_PORT_FN(0x004f3d50, Fn_004f3d50, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4f3d59 = 0;
    uint32_t fB_4f3d59 = 0;
    uint32_t fR_4f3d67 = 0;
    uint32_t fA_4f3d77 = 0;
    uint32_t fB_4f3d77 = 0;
    uint32_t fR_4f3d7c = 0;
    uint32_t fA_4f3d89 = 0;
    uint32_t fB_4f3d89 = 0;
    esp -= 4; wr32(m, esp, ecx);
    eax = rd32(m, eax + 0x18u);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, eax + 0x4u);
    fA_4f3d59 = rd8(m, esi + 0x19u);
    fB_4f3d59 = 0x0u;
    ebx = eax;
    if (fA_4f3d59 != fB_4f3d59) goto L_4f3d91;
L_4f3d61:
    ecx = rd32(m, esi + 0x10u);
    eax = (esi + 0xcu);
    fR_4f3d67 = (ecx & ecx);
    if ((fR_4f3d67 == 0)) goto L_4f3d75;
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x4ec2c0u, 0x4f3d73u);  // FUN_004ec2c0
    goto L_4f3d7c;
L_4f3d75:
    ecx = rd32(m, eax);
    fA_4f3d77 = ecx;
    fB_4f3d77 = rd32(m, edi);
    eax = (eax & 0xffffff00u) | (((fA_4f3d77 < fB_4f3d77) ? 1u : 0u) & 0xffu);
L_4f3d7c:
    fR_4f3d7c = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_4f3d7c == 0)) goto L_4f3d85;
    esi = rd32(m, esi + 0x8u);
    goto L_4f3d89;
L_4f3d85:
    ebx = esi;
    esi = rd32(m, esi);
L_4f3d89:
    fA_4f3d89 = rd8(m, esi + 0x19u);
    fB_4f3d89 = 0x0u;
    if (fA_4f3d89 == fB_4f3d89) goto L_4f3d61;
    eax = ebx;
L_4f3d91:
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 004f3da0 Fn_004f3da0
//   original FUN_004f3da0; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x004f3da0, Fn_004f3da0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4f3daa = 0;
    uint32_t fB_4f3daa = 0;
    uint32_t fA_4f3dc0 = 0;
    uint32_t fB_4f3dc0 = 0;
    uint32_t fA_4f3dd3 = 0;
    uint32_t fB_4f3dd3 = 0;
    eax = rd32(m, ecx + 0x8u);
    edx = rd32(m, eax);
    wr32(m, ecx + 0x8u, edx);
    edx = rd32(m, eax);
    fA_4f3daa = rd8(m, edx + 0x19u);
    fB_4f3daa = 0x0u;
    if (fA_4f3daa != fB_4f3daa) goto L_4f3db3;
    wr32(m, edx + 0x4u, ecx);
L_4f3db3:
    edx = rd32(m, ecx + 0x4u);
    wr32(m, eax + 0x4u, edx);
    edx = rd32(m, esp + 0x4u);
    edx = rd32(m, edx + 0x18u);
    fA_4f3dc0 = ecx;
    fB_4f3dc0 = rd32(m, edx + 0x4u);
    if (fA_4f3dc0 != fB_4f3dc0) goto L_4f3dd0;
    wr32(m, edx + 0x4u, eax);
    wr32(m, eax, ecx);
    wr32(m, ecx + 0x4u, eax);
    LIFT_RET(4);
L_4f3dd0:
    edx = rd32(m, ecx + 0x4u);
    fA_4f3dd3 = ecx;
    fB_4f3dd3 = rd32(m, edx);
    if (fA_4f3dd3 != fB_4f3dd3) goto L_4f3de1;
    wr32(m, edx, eax);
    wr32(m, eax, ecx);
    wr32(m, ecx + 0x4u, eax);
    LIFT_RET(4);
L_4f3de1:
    wr32(m, edx + 0x8u, eax);
    wr32(m, eax, ecx);
    wr32(m, ecx + 0x4u, eax);
    LIFT_RET(4);
}

// 004f3e60 Fn_004f3e60
//   original FUN_004f3e60; register inputs: ecx; returns with ret 16
REFLEX_PORT_FN(0x004f3e60, Fn_004f3e60, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_4f3e9c = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp -= 4; wr32(m, esp, 0xffffffffu);
    esp -= 4; wr32(m, esp, 0x901ed0u);
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
    esi = ecx;
    LIFT_CALL(0x4f4f40u, 0x4f3e92u);  // FUN_004f4f40
    wr32(m, ebp - 0x14u, eax);
    wr32(m, ebp - 0x4u, 0x0u);
    fR_4f3e9c = (eax & eax);
    if ((fR_4f3e9c == 0)) goto L_4f3eca;
    ecx = rd32(m, ebp + 0x8u);
    wr32(m, eax, ecx);
    edx = rd32(m, ebp + 0xcu);
    wr32(m, eax + 0x4u, edx);
    ecx = rd32(m, ebp + 0x10u);
    wr32(m, eax + 0x8u, ecx);
    x0 = movsd_m(rd64(m, esi));
    wr64(m, eax + 0xcu, x0.lo);
    edx = rd32(m, esi + 0x8u);
    wr32(m, eax + 0x14u, edx);
    ecx = (ecx & 0xffffff00u) | ((rd8(m, ebp + 0x14u)) & 0xffu);
    wr8(m, eax + 0x18u, (ecx & 0xffu));
    wr8(m, eax + 0x19u, 0x0u);
L_4f3eca:
    wr32(m, ebp - 0x4u, 0xffffffffu);
    ecx = rd32(m, ebp - 0xcu);
    wr32(m, FS_BASE, ecx);
    ecx = rd32(m, esp); esp += 4;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(16);
}

// 004f4a30 Fn_004f4a30
//   original FUN_004f4a30; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x004f4a30, Fn_004f4a30, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4f4a35 = 0;
    uint32_t fB_4f4a35 = 0;
    uint32_t fA_4f4a57 = 0;
    uint32_t fB_4f4a57 = 0;
    LIFT_CALL(0x4dc0f0u, 0x4f4a35u);  // FUN_004dc0f0
    fA_4f4a35 = rd8(m, 0xda67d8u);
    fB_4f4a35 = 0x0u;
    if (fA_4f4a35 == fB_4f4a35) goto L_4f4a49;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x4f4a49u);
L_4f4a49:
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, 0x10u);
    esp -= 4; wr32(m, esp, 0x4u);
    LIFT_CALL(rd32(m, 0x916358u) /* 8.287573390694476e-39 */, 0x4f4a54u);
    esp = esp + 0x8u;
    fA_4f4a57 = rd8(m, 0xda67d8u);
    fB_4f4a57 = 0x0u;
    esi = eax;
    if (fA_4f4a57 == fB_4f4a57) goto L_4f4a6d;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4f4a6du);
L_4f4a6d:
    eax = esi;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 004f4ea0 Fn_004f4ea0
//   original FUN_004f4ea0; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x004f4ea0, Fn_004f4ea0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_4f4eda = 0;
    uint32_t fR_4f4ee7 = 0;
    uint32_t fR_4f4ef4 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp -= 4; wr32(m, esp, 0xffffffffu);
    esp -= 4; wr32(m, esp, 0x9029a0u);
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
    LIFT_CALL(0x4f4f40u, 0x4f4ed0u);  // FUN_004f4f40
    wr32(m, ebp - 0x14u, eax);
    wr32(m, ebp - 0x4u, 0x0u);
    fR_4f4eda = (eax & eax);
    if ((fR_4f4eda == 0)) goto L_4f4ee4;
    wr32(m, eax, 0x0u);
L_4f4ee4:
    ecx = (eax + 0x4u);
    fR_4f4ee7 = (ecx & ecx);
    if ((fR_4f4ee7 == 0)) goto L_4f4ef1;
    wr32(m, ecx, 0x0u);
L_4f4ef1:
    ecx = (eax + 0x8u);
    fR_4f4ef4 = (ecx & ecx);
    if ((fR_4f4ef4 == 0)) goto L_4f4efe;
    wr32(m, ecx, 0x0u);
L_4f4efe:
    wr32(m, ebp - 0x4u, 0xffffffffu);
    wr8(m, eax + 0x18u, 0x1u);
    wr8(m, eax + 0x19u, 0x0u);
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

// 004f4f40 Fn_004f4f40
//   original FUN_004f4f40; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x004f4f40, Fn_004f4f40, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4f4f45 = 0;
    uint32_t fB_4f4f45 = 0;
    uint32_t fA_4f4f67 = 0;
    uint32_t fB_4f4f67 = 0;
    LIFT_CALL(0x4dc0f0u, 0x4f4f45u);  // FUN_004dc0f0
    fA_4f4f45 = rd8(m, 0xda67d8u);
    fB_4f4f45 = 0x0u;
    if (fA_4f4f45 == fB_4f4f45) goto L_4f4f59;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x4f4f59u);
L_4f4f59:
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, 0x10u);
    esp -= 4; wr32(m, esp, 0x1cu);
    LIFT_CALL(rd32(m, 0x916358u) /* 8.287573390694476e-39 */, 0x4f4f64u);
    esp = esp + 0x8u;
    fA_4f4f67 = rd8(m, 0xda67d8u);
    fB_4f4f67 = 0x0u;
    esi = eax;
    if (fA_4f4f67 == fB_4f4f67) goto L_4f4f7d;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4f4f7du);
L_4f4f7d:
    eax = esi;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 004f5330 Fn_004f5330
//   original FUN_004f5330; register inputs: none; returns with ret 12
REFLEX_PORT_FN(0x004f5330, Fn_004f5330, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4f534a = 0;
    uint32_t fB_4f534a = 0;
    uint32_t fA_4f5350 = 0;
    uint32_t fB_4f5350 = 0;
    uint32_t fA_4f5369 = 0;
    uint32_t fB_4f5369 = 0;
    uint32_t fA_4f5392 = 0;
    uint32_t fB_4f5392 = 0;
    uint32_t fA_4f53ab = 0;
    uint32_t fB_4f53ab = 0;
    uint32_t fR_4f53d3 = 0;
    uint32_t fA_4f53e7 = 0;
    uint32_t fB_4f53e7 = 0;
    uint32_t fA_4f5410 = 0;
    uint32_t fB_4f5410 = 0;
    uint32_t fA_4f5439 = 0;
    uint32_t fB_4f5439 = 0;
    uint32_t fR_4f5475 = 0;
    uint32_t fA_4f5479 = 0;
    uint32_t fB_4f5479 = 0;
    uint32_t fA_4f5483 = 0;
    uint32_t fB_4f5483 = 0;
    uint32_t fA_4f54c0 = 0;
    uint32_t fB_4f54c0 = 0;
    uint32_t fA_4f54e3 = 0;
    uint32_t fB_4f54e3 = 0;
    uint32_t fA_4f5501 = 0;
    uint32_t fB_4f5501 = 0;
    uint32_t fA_4f556b = 0;
    uint32_t fB_4f556b = 0;
    uint32_t fA_4f55b6 = 0;
    uint32_t fB_4f55b6 = 0;
    uint32_t fR_4f55f1 = 0;
    uint32_t fA_4f55f5 = 0;
    uint32_t fB_4f55f5 = 0;
    uint32_t fA_4f55ff = 0;
    uint32_t fB_4f55ff = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0x3cu;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, 0xae1c44u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x4f534au);
    fA_4f534a = rd32(m, ebx + 0x44u);
    fB_4f534a = 0x0u;
    if (fA_4f534a != fB_4f534a) goto L_4f5392;
    fA_4f5350 = rd32(m, ebx + 0x48u);
    fB_4f5350 = 0x0u;
    if (fA_4f5350 != fB_4f5350) goto L_4f555c;
    ebx = rd32(m, ebx + 0x40u);
    eax = rd32(m, ebx + 0x10u);
    eax = eax - rd32(m, ebx + 0xcu);
    esi = rd32(m, ebp + 0xcu);
    eax = uint32_t(int32_t(eax) >> 2);
    fA_4f5369 = esi;
    fB_4f5369 = eax;
    if (fA_4f5369 < fB_4f5369) goto L_4f5373;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4f5373u);
L_4f5373:
    edx = rd32(m, ebp + 0x10u);
    ecx = rd32(m, ebx + 0xcu);
    eax = rd32(m, edx);
    esp -= 4; wr32(m, esp, 0xae1c44u);
    wr32(m, ecx + esi * 4, eax);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4f5389u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(12);
L_4f5392:
    fA_4f5392 = rd32(m, ebx + 0x48u);
    fB_4f5392 = 0x0u;
    if (fA_4f5392 != fB_4f5392) goto L_4f555c;
    edi = rd32(m, ebx + 0x40u);
    ecx = rd32(m, edi + 0x10u);
    ecx = ecx - rd32(m, edi + 0xcu);
    esi = rd32(m, ebp + 0xcu);
    ecx = uint32_t(int32_t(ecx) >> 2);
    fA_4f53ab = esi;
    fB_4f53ab = ecx;
    if (fA_4f53ab < fB_4f53ab) goto L_4f53b5;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4f53b5u);
L_4f53b5:
    edx = rd32(m, edi + 0xcu);
    edi = rd32(m, ebx + 0x44u);
    eax = (esi * 4);
    edx = edx + eax;
    esp -= 4; wr32(m, esp, edx);
    eax = (esp + 0x10u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x419ea0u, 0x4f53cfu);  // FUN_00419ea0
    eax = rd32(m, esp + 0xcu);
    fR_4f53d3 = (eax & eax);
    if (!(fR_4f53d3 == 0)) goto L_4f552d;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4f53e1u);
    eax = 0u;
L_4f53e3:
    edi = rd32(m, esp + 0x10u);
    fA_4f53e7 = edi;
    fB_4f53e7 = rd32(m, eax + 0x18u);
    if (fA_4f53e7 != fB_4f53e7) goto L_4f53f2;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4f53f2u);
L_4f53f2:
    ecx = rd32(m, ebp + 0xcu);
    edx = rd32(m, edi + 0x34u);
    esi = (edi + 0x10u);
    wr32(m, esp + 0x14u, ecx);
    ecx = (esp + 0x14u);
    wr32(m, esp + 0x18u, esi);
    wr32(m, esp + 0x1cu, edx);
    LIFT_CALL(0x4f0800u, 0x4f5410u);  // FUN_004f0800
    fA_4f5410 = rd32(m, esi + 0x1cu);
    fB_4f5410 = 0x0u;
    if (fA_4f5410 != fB_4f5410) goto L_4f542a;
    eax = rd32(m, esp + 0xcu);
    edx = rd32(m, ebx + 0x44u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0x1cu);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x4fcb60u, 0x4f542au);  // FUN_004fcb60
L_4f542a:
    edi = rd32(m, ebx + 0x40u);
    eax = rd32(m, edi + 0x10u);
    eax = eax - rd32(m, edi + 0xcu);
    esi = rd32(m, ebp + 0xcu);
    eax = uint32_t(int32_t(eax) >> 2);
    fA_4f5439 = esi;
    fB_4f5439 = eax;
    if (fA_4f5439 < fB_4f5439) goto L_4f5443;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4f5443u);
L_4f5443:
    eax = rd32(m, ebp + 0x10u);
    ecx = rd32(m, edi + 0xcu);
    edx = rd32(m, eax);
    esp -= 4; wr32(m, esp, eax);
    eax = (esp + 0x18u);
    wr32(m, ecx + esi * 4, edx);
    edi = rd32(m, ebx + 0x44u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x419ea0u, 0x4f545cu);  // FUN_00419ea0
    ecx = rd32(m, eax);
    edi = rd32(m, eax + 0x4u);
    eax = rd32(m, ebx + 0x44u);
    edx = rd32(m, eax + 0x18u);
    eax = rd32(m, eax);
    wr32(m, esp + 0xcu, ecx);
    wr32(m, esp + 0x10u, edi);
    wr32(m, esp + 0x18u, edx);
    fR_4f5475 = (ecx & ecx);
    if ((fR_4f5475 == 0)) goto L_4f547d;
    fA_4f5479 = ecx;
    fB_4f5479 = eax;
    if (fA_4f5479 == fB_4f5479) goto L_4f5483;
L_4f547d:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4f5483u);
L_4f5483:
    fA_4f5483 = edi;
    fB_4f5483 = rd32(m, esp + 0x18u);
    if (fA_4f5483 != fB_4f5483) goto L_4f5534;
    eax = rd32(m, ebx + 0x44u);
    edi = rd32(m, ebx + 0x20u);
    esp -= 4; wr32(m, esp, esi);
    esi = (esp + 0x24u);
    wr32(m, esp + 0x10u, eax);
    LIFT_CALL(0x4dc5c0u, 0x4f54a1u);  // FUN_004dc5c0
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = rd32(m, ebp + 0x10u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x4fc440u, 0x4f54b0u);  // FUN_004fc440
    esp = esp + 0x4u;
    edi = eax;
    LIFT_CALL(0x4ebe60u, 0x4f54bau);  // FUN_004ebe60
    esi = rd32(m, esp + 0x40u);
    edi = 0u;
    fA_4f54c0 = esi;
    fB_4f54c0 = edi;
    if (fA_4f54c0 == fB_4f54c0) goto L_4f54cd;
    LIFT_CALL(0x4ebe10u, 0x4f54c9u);  // FUN_004ebe10
    wr32(m, esp + 0x40u, edi);
L_4f54cd:
    esi = (esp + 0x20u);
    wr32(m, esp + 0x44u, edi);
    LIFT_CALL(0x4f2220u, 0x4f54dau);  // FUN_004f2220
    esi = rd32(m, esp + 0x20u);
    LIFT_CALL(0x4dc0f0u, 0x4f54e3u);  // FUN_004dc0f0
    fA_4f54e3 = rd8(m, 0xda67d8u);
    fB_4f54e3 = 0x0u;
    if (fA_4f54e3 == fB_4f54e3) goto L_4f54f7;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x4f54f7u);
L_4f54f7:
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(rd32(m, 0x916350u) /* 8.287536956934404e-39 */, 0x4f54feu);
    esp = esp + 0x4u;
    fA_4f5501 = rd8(m, 0xda67d8u);
    fB_4f5501 = 0x0u;
    if (fA_4f5501 == fB_4f5501) goto L_4f562e;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4f5519u);
    esp -= 4; wr32(m, esp, 0xae1c44u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4f5524u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(12);
L_4f552d:
    eax = rd32(m, eax);
    goto L_4f53e3;
L_4f5534:
    esi = (esp + 0xcu);
    LIFT_CALL(0x4fc5c0u, 0x4f553du);  // FUN_004fc5c0
    ecx = rd32(m, ebp + 0xcu);
    eax = eax + 0x4u;
    LIFT_CALL(0x4dc630u, 0x4f5548u);  // FUN_004dc630
    esp -= 4; wr32(m, esp, 0xae1c44u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4f5553u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(12);
L_4f555c:
    esi = rd32(m, ebx + 0x40u);
    edx = rd32(m, esi + 0x10u);
    edx = edx - rd32(m, esi + 0xcu);
    edi = rd32(m, ebp + 0xcu);
    edx = uint32_t(int32_t(edx) >> 2);
    fA_4f556b = edi;
    fB_4f556b = edx;
    if (fA_4f556b < fB_4f556b) goto L_4f5575;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4f5575u);
L_4f5575:
    ecx = rd32(m, esi + 0xcu);
    eax = (edi * 4);
    edi = rd32(m, ebx + 0x48u);
    ecx = ecx + eax;
    esp -= 4; wr32(m, esp, ecx);
    edx = (esp + 0x10u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x763270u, 0x4f558fu);  // FUN_00763270
    eax = rd32(m, esp + 0x10u);
    ecx = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, ebx + 0x48u);
    esp -= 4; wr32(m, esp, ecx);
    edx = (esp + 0x1cu);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x4fc6b0u, 0x4f55a7u);  // FUN_004fc6b0
    esi = rd32(m, ebx + 0x40u);
    ecx = rd32(m, esi + 0x10u);
    ecx = ecx - rd32(m, esi + 0xcu);
    edi = rd32(m, ebp + 0xcu);
    ecx = uint32_t(int32_t(ecx) >> 2);
    fA_4f55b6 = edi;
    fB_4f55b6 = ecx;
    if (fA_4f55b6 < fB_4f55b6) goto L_4f55c0;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4f55c0u);
L_4f55c0:
    edx = rd32(m, esi + 0xcu);
    esi = rd32(m, ebp + 0x10u);
    ecx = rd32(m, esi);
    eax = (edi * 4);
    wr32(m, eax + edx, ecx);
    edi = rd32(m, ebx + 0x48u);
    esp -= 4; wr32(m, esp, esi);
    edx = (esp + 0x18u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x763270u, 0x4f55e0u);  // FUN_00763270
    ecx = rd32(m, eax);
    edi = rd32(m, eax + 0x4u);
    eax = rd32(m, ebx + 0x48u);
    edx = rd32(m, eax + 0x18u);
    eax = rd32(m, eax);
    wr32(m, esp + 0x18u, edx);
    fR_4f55f1 = (ecx & ecx);
    if ((fR_4f55f1 == 0)) goto L_4f55f9;
    fA_4f55f5 = ecx;
    fB_4f55f5 = eax;
    if (fA_4f55f5 == fB_4f55f5) goto L_4f55ff;
L_4f55f9:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4f55ffu);
L_4f55ff:
    fA_4f55ff = edi;
    fB_4f55ff = rd32(m, esp + 0x18u);
    if (fA_4f55ff != fB_4f55ff) goto L_4f562e;
    edi = rd32(m, ebx + 0x20u);
    ebx = rd32(m, ebx + 0x48u);
    eax = rd32(m, ebp + 0xcu);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, ebx);
    wr32(m, esp + 0x1cu, eax);
    wr32(m, esp + 0x20u, 0x0u);
    LIFT_CALL(0x4fc5f0u, 0x4f5621u);  // FUN_004fc5f0
    x0 = movsd_m(rd64(m, esp + 0x14u));
    wr64(m, eax, x0.lo);
    wr32(m, eax + 0x8u, edi);
L_4f562e:
    esp -= 4; wr32(m, esp, 0xae1c44u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4f5639u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(12);
}

// 004f64a0 Fn_004f64a0
//   original FUN_004f64a0; register inputs: ecx eax; returns with ret 0
REFLEX_PORT_FN(0x004f64a0, Fn_004f64a0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_4f64c9 = 0;
    uint32_t fA_4f64cd = 0;
    uint32_t fB_4f64cd = 0;
    uint32_t fA_4f64d7 = 0;
    uint32_t fB_4f64d7 = 0;
    uint32_t fR_4f64db = 0;
    uint32_t fA_4f64e5 = 0;
    uint32_t fB_4f64e5 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0xcu;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = ecx;
    esi = rd32(m, edi + 0x48u);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0x14u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x4fbda0u, 0x4f64bcu);  // FUN_004fbda0
    esi = rd32(m, eax);
    ebx = rd32(m, eax + 0x4u);
    eax = rd32(m, edi + 0x48u);
    edi = rd32(m, eax + 0x18u);
    eax = rd32(m, eax);
    fR_4f64c9 = (esi & esi);
    if ((fR_4f64c9 == 0)) goto L_4f64d1;
    fA_4f64cd = esi;
    fB_4f64cd = eax;
    if (fA_4f64cd == fB_4f64cd) goto L_4f64d7;
L_4f64d1:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4f64d7u);
L_4f64d7:
    fA_4f64d7 = ebx;
    fB_4f64d7 = edi;
    if (fA_4f64d7 == fB_4f64d7) goto L_4f64fe;
    fR_4f64db = (esi & esi);
    if (!(fR_4f64db == 0)) goto L_4f64fa;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4f64e5u);
L_4f64e5:
    fA_4f64e5 = ebx;
    fB_4f64e5 = rd32(m, esi + 0x18u);
    if (fA_4f64e5 != fB_4f64e5) goto L_4f64f0;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4f64f0u);
L_4f64f0:
    eax = (ebx + 0x28u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_4f64fa:
    esi = rd32(m, esi);
    goto L_4f64e5;
L_4f64fe:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    eax = 0x9c9ff4u;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 004f6740 Fn_004f6740
//   original FUN_004f6740; register inputs: ecx eax; returns with ret 4
REFLEX_PORT_FN(0x004f6740, Fn_004f6740, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4f674f = 0;
    uint32_t fB_4f674f = 0;
    uint32_t fA_4f675d = 0;
    uint32_t fB_4f675d = 0;
    uint32_t fA_4f676b = 0;
    uint32_t fB_4f676b = 0;
    uint32_t fA_4f6779 = 0;
    uint32_t fB_4f6779 = 0;
    uint32_t fA_4f6787 = 0;
    uint32_t fB_4f6787 = 0;
    uint32_t fA_4f6795 = 0;
    uint32_t fB_4f6795 = 0;
    edx = rd32(m, esp + 0x4u);
    switch (edx) { case 0: goto L_4f6783; case 1: goto L_4f6791; case 2: goto L_4f6767; case 3: goto L_4f6775; case 4: goto L_4f674b; case 5: goto L_4f6759; default: throw std::runtime_error("jump table"); }
L_4f674b:
    eax = rd32(m, eax);
    edx = 0u;
    fA_4f674f = eax;
    fB_4f674f = rd32(m, ecx);
    edx = (edx & 0xffffff00u) | (((int32_t(sext(fA_4f674f, 4)) > int32_t(sext(fB_4f674f, 4))) ? 1u : 0u) & 0xffu);
    eax = (eax & 0xffffff00u) | (((edx & 0xffu)) & 0xffu);
    LIFT_RET(4);
L_4f6759:
    eax = rd32(m, eax);
    edx = 0u;
    fA_4f675d = eax;
    fB_4f675d = rd32(m, ecx);
    edx = (edx & 0xffffff00u) | (((int32_t(sext(fA_4f675d, 4)) >= int32_t(sext(fB_4f675d, 4))) ? 1u : 0u) & 0xffu);
    eax = (eax & 0xffffff00u) | (((edx & 0xffu)) & 0xffu);
    LIFT_RET(4);
L_4f6767:
    eax = rd32(m, eax);
    edx = 0u;
    fA_4f676b = eax;
    fB_4f676b = rd32(m, ecx);
    edx = (edx & 0xffffff00u) | (((int32_t(sext(fA_4f676b, 4)) < int32_t(sext(fB_4f676b, 4))) ? 1u : 0u) & 0xffu);
    eax = (eax & 0xffffff00u) | (((edx & 0xffu)) & 0xffu);
    LIFT_RET(4);
L_4f6775:
    eax = rd32(m, eax);
    edx = 0u;
    fA_4f6779 = eax;
    fB_4f6779 = rd32(m, ecx);
    edx = (edx & 0xffffff00u) | (((int32_t(sext(fA_4f6779, 4)) <= int32_t(sext(fB_4f6779, 4))) ? 1u : 0u) & 0xffu);
    eax = (eax & 0xffffff00u) | (((edx & 0xffu)) & 0xffu);
    LIFT_RET(4);
L_4f6783:
    eax = rd32(m, eax);
    edx = 0u;
    fA_4f6787 = eax;
    fB_4f6787 = rd32(m, ecx);
    edx = (edx & 0xffffff00u) | (((fA_4f6787 == fB_4f6787) ? 1u : 0u) & 0xffu);
    eax = (eax & 0xffffff00u) | (((edx & 0xffu)) & 0xffu);
    LIFT_RET(4);
L_4f6791:
    eax = rd32(m, eax);
    edx = 0u;
    fA_4f6795 = eax;
    fB_4f6795 = rd32(m, ecx);
    edx = (edx & 0xffffff00u) | (((fA_4f6795 != fB_4f6795) ? 1u : 0u) & 0xffu);
    eax = (eax & 0xffffff00u) | (((edx & 0xffu)) & 0xffu);
    LIFT_RET(4);
}

// 004f6e60 Table_SetCellInt
//   original Table_SetCellInt; register inputs: eax esi; returns with ret 8
REFLEX_PORT_FN(0x004f6e60, Table_SetCellInt, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4f6e7f = 0;
    uint32_t fB_4f6e7f = 0;
    uint32_t fA_4f6e8f = 0;
    uint32_t fB_4f6e8f = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    eax = esi;
    LIFT_CALL(0x402160u, 0x4f6e73u);  // Table_ColumnIndex
    edi = eax;
    esp -= 4; wr32(m, esp, ebp);
    eax = esi;
    LIFT_CALL(0x41aaf0u, 0x4f6e7du);  // FUN_0041aaf0
    ecx = rd32(m, ebx);
    fA_4f6e7f = ecx;
    fB_4f6e7f = rd32(m, eax);
    if (fA_4f6e7f == fB_4f6e7f) goto L_4f6e86;
    wr32(m, esi + 0x68u, (rd32(m, esi + 0x68u) + 1u));
L_4f6e86:
    edx = rd32(m, esi + 0x14u);
    edx = edx - rd32(m, esi + 0x10u);
    edx = uint32_t(int32_t(edx) >> 2);
    fA_4f6e8f = edi;
    fB_4f6e8f = edx;
    if (fA_4f6e8f < fB_4f6e8f) goto L_4f6e99;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4f6e99u);
L_4f6e99:
    eax = rd32(m, esi + 0x10u);
    ecx = rd32(m, eax + edi * 4);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x4f5330u, 0x4f6ea7u);  // FUN_004f5330
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 004f7480 Fn_004f7480
//   original FUN_004f7480; register inputs: eax; returns with ret 8
REFLEX_PORT_FN(0x004f7480, Fn_004f7480, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4f74b7 = 0;
    uint32_t fB_4f74b7 = 0;
    uint32_t fA_4f74d5 = 0;
    uint32_t fB_4f74d5 = 0;
    uint32_t fA_4f74f6 = 0;
    uint32_t fB_4f74f6 = 0;
    esp = esp - 0x1cu;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x28u);
    ebx = 0u;
    esp -= 4; wr32(m, esp, esi);
    wr32(m, ebp + 0x80u, 0xffffffffu);
    wr32(m, ebp + 0x84u, ebx);
    esi = rd32(m, eax);
    eax = esi;
    esp -= 4; wr32(m, esp, edi);
    wr32(m, esp + 0x28u, 0xfu);
    wr32(m, esp + 0x24u, ebx);
    wr8(m, esp + 0x14u, (ebx & 0xffu));
    edx = (eax + 0x1u);
L_4f74b4:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    eax = (eax + 1u);
    fA_4f74b7 = (ecx & 0xffu);
    fB_4f74b7 = (ebx & 0xffu);
    if (fA_4f74b7 != fB_4f74b7) goto L_4f74b4;
    esp -= 4; wr32(m, esp, esi);
    eax = eax - edx;
    esi = (esp + 0x14u);
    LIFT_CALL(0x4f2ce0u, 0x4f74c7u);  // FUN_004f2ce0
    ecx = esi;
    esp -= 4; wr32(m, esp, ecx);
    edi = (ebp + 0x88u);
    LIFT_CALL(0x4dc3c0u, 0x4f74d5u);  // FUN_004dc3c0
    fA_4f74d5 = rd32(m, esp + 0x28u);
    fB_4f74d5 = 0x10u;
    if (fA_4f74d5 < fB_4f74d5) goto L_4f74e6;
    edx = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x4f3320u, 0x4f74e6u);  // FUN_004f3320
L_4f74e6:
    esp -= 4; wr32(m, esp, 0x4u);
    esi = (ebp + 0x98u);
    LIFT_CALL(0x78fbe0u, 0x4f74f3u);  // ReflexAlignedAlloc
    esp = esp + 0x4u;
    fA_4f74f6 = eax;
    fB_4f74f6 = ebx;
    if (fA_4f74f6 == fB_4f74f6) goto L_4f74fe;
    wr32(m, eax, esi);
    goto L_4f7500;
L_4f74fe:
    eax = 0u;
L_4f7500:
    wr32(m, esi, eax);
    eax = rd32(m, esp + 0x34u);
    esp -= 4; wr32(m, esp, 0x7fu);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ebp);
    wr32(m, esi + 0xcu, ebx);
    wr32(m, esi + 0x10u, ebx);
    wr32(m, esi + 0x14u, ebx);
    LIFT_CALL(rd32(m, 0x916260u) /* 8.286626112932593e-39 */, 0x4f7519u);
    esp = esp + 0xcu;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    wr8(m, ebp + 0x7fu, (ebx & 0xffu));
    eax = ebp;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x1cu;
    LIFT_RET(8);
}

// 004f76b0 Fn_004f76b0
//   original FUN_004f76b0; register inputs: eax; returns with ret 
REFLEX_PORT_FN(0x004f76b0, Fn_004f76b0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_4f76dd = 0;
    uint32_t fA_4f76e1 = 0;
    uint32_t fB_4f76e1 = 0;
    uint32_t fR_4f76ee = 0;
    uint32_t fR_4f770e = 0;
    uint32_t fA_4f7712 = 0;
    uint32_t fB_4f7712 = 0;
    uint32_t fA_4f771c = 0;
    uint32_t fB_4f771c = 0;
    uint32_t fA_4f7756 = 0;
    uint32_t fB_4f7756 = 0;
    uint32_t fA_4f7779 = 0;
    uint32_t fB_4f7779 = 0;
    uint32_t fA_4f7797 = 0;
    uint32_t fB_4f7797 = 0;
    uint32_t fR_4f77d8 = 0;
    uint32_t fA_4f77dc = 0;
    uint32_t fB_4f77dc = 0;
    uint32_t fA_4f77e6 = 0;
    uint32_t fB_4f77e6 = 0;
    esp = esp - 0x38u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x40u);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, 0xae1c44u);
    esi = eax;
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x4f76c8u);
    edi = rd32(m, esi + 0x40u);
    ebp = rd32(m, edi + 0x10u);
    ebp = ebp - rd32(m, edi + 0xcu);
    esp -= 4; wr32(m, esp, ebx);
    ebp = uint32_t(int32_t(ebp) >> 2);
    LIFT_CALL(0x4faf10u, 0x4f76dau);  // FUN_004faf10
    eax = rd32(m, esi + 0x44u);
    fR_4f76dd = (eax & eax);
    if (!(fR_4f76dd == 0)) goto L_4f76ea;
    fA_4f76e1 = rd32(m, esi + 0x48u);
    fB_4f76e1 = eax;
    if (fA_4f76e1 == fB_4f76e1) goto L_4f7812;
L_4f76ea:
    edi = rd32(m, esi + 0x48u);
    esp -= 4; wr32(m, esp, ebx);
    fR_4f76ee = (edi & edi);
    if (!(fR_4f76ee == 0)) goto L_4f77c2;
    ecx = (esp + 0x18u);
    esp -= 4; wr32(m, esp, ecx);
    edi = eax;
    LIFT_CALL(0x419ea0u, 0x4f7702u);  // FUN_00419ea0
    eax = rd32(m, esi + 0x44u);
    ecx = rd32(m, esp + 0x14u);
    edi = rd32(m, eax + 0x18u);
    eax = rd32(m, eax);
    fR_4f770e = (ecx & ecx);
    if ((fR_4f770e == 0)) goto L_4f7716;
    fA_4f7712 = ecx;
    fB_4f7712 = eax;
    if (fA_4f7712 == fB_4f7712) goto L_4f771c;
L_4f7716:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4f771cu);
L_4f771c:
    fA_4f771c = rd32(m, esp + 0x18u);
    fB_4f771c = edi;
    if (fA_4f771c != fB_4f771c) goto L_4f77ad;
    edx = rd32(m, esi + 0x44u);
    edi = rd32(m, esi + 0x20u);
    esp -= 4; wr32(m, esp, ebp);
    esi = (esp + 0x24u);
    wr32(m, esp + 0x50u, edx);
    LIFT_CALL(0x4dc5c0u, 0x4f773au);  // FUN_004dc5c0
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, esp + 0x50u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x4fc440u, 0x4f7746u);  // FUN_004fc440
    esp = esp + 0x4u;
    edi = eax;
    LIFT_CALL(0x4ebe60u, 0x4f7750u);  // FUN_004ebe60
    esi = rd32(m, esp + 0x40u);
    edi = 0u;
    fA_4f7756 = esi;
    fB_4f7756 = edi;
    if (fA_4f7756 == fB_4f7756) goto L_4f7763;
    LIFT_CALL(0x4ebe10u, 0x4f775fu);  // FUN_004ebe10
    wr32(m, esp + 0x40u, edi);
L_4f7763:
    esi = (esp + 0x20u);
    wr32(m, esp + 0x44u, edi);
    LIFT_CALL(0x4f2220u, 0x4f7770u);  // FUN_004f2220
    esi = rd32(m, esp + 0x20u);
    LIFT_CALL(0x4dc0f0u, 0x4f7779u);  // FUN_004dc0f0
    fA_4f7779 = rd8(m, 0xda67d8u);
    fB_4f7779 = 0x0u;
    if (fA_4f7779 == fB_4f7779) goto L_4f778d;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x4f778du);
L_4f778d:
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(rd32(m, 0x916350u) /* 8.287536956934404e-39 */, 0x4f7794u);
    esp = esp + 0x4u;
    fA_4f7797 = rd8(m, 0xda67d8u);
    fB_4f7797 = 0x0u;
    if (fA_4f7797 == fB_4f7797) goto L_4f7812;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4f77abu);
    goto L_4f7812;
L_4f77ad:
    esi = (esp + 0x14u);
    LIFT_CALL(0x4fc5c0u, 0x4f77b6u);  // FUN_004fc5c0
    eax = eax + 0x4u;
    ecx = ebp;
    LIFT_CALL(0x4dc630u, 0x4f77c0u);  // FUN_004dc630
    goto L_4f7812;
L_4f77c2:
    eax = (esp + 0x18u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x763270u, 0x4f77ccu);  // FUN_00763270
    eax = rd32(m, esi + 0x48u);
    ecx = rd32(m, esp + 0x14u);
    edi = rd32(m, eax + 0x18u);
    eax = rd32(m, eax);
    fR_4f77d8 = (ecx & ecx);
    if ((fR_4f77d8 == 0)) goto L_4f77e0;
    fA_4f77dc = ecx;
    fB_4f77dc = eax;
    if (fA_4f77dc == fB_4f77dc) goto L_4f77e6;
L_4f77e0:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4f77e6u);
L_4f77e6:
    fA_4f77e6 = rd32(m, esp + 0x18u);
    fB_4f77e6 = edi;
    if (fA_4f77e6 != fB_4f77e6) goto L_4f7812;
    edi = rd32(m, esi + 0x20u);
    esi = rd32(m, esi + 0x48u);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    wr32(m, esp + 0x1cu, ebp);
    wr32(m, esp + 0x20u, 0x0u);
    LIFT_CALL(0x4fc5f0u, 0x4f7805u);  // FUN_004fc5f0
    x0 = movsd_m(rd64(m, esp + 0x14u));
    wr64(m, eax, x0.lo);
    wr32(m, eax + 0x8u, edi);
L_4f7812:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x38u;
    wr32(m, esp + 0x4u, 0xae1c44u);
    LIFT_TAILJMP(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */);
}

// 004fa890 Fn_004fa890
//   original FUN_004fa890; register inputs: edi; returns with ret 4
REFLEX_PORT_FN(0x004fa890, Fn_004fa890, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4fa8ac = 0;
    uint32_t fB_4fa8ac = 0;
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, 0xae1c44u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x4fa89cu);
    eax = rd32(m, esp + 0x8u);
    esi = rd32(m, eax + 0x40u);
    ecx = rd32(m, esi + 0x10u);
    ecx = ecx - rd32(m, esi + 0xcu);
    ecx = uint32_t(int32_t(ecx) >> 2);
    fA_4fa8ac = edi;
    fB_4fa8ac = ecx;
    if (fA_4fa8ac < fB_4fa8ac) goto L_4fa8b6;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4fa8b6u);
L_4fa8b6:
    edx = rd32(m, esi + 0xcu);
    esp -= 4; wr32(m, esp, 0xae1c44u);
    esi = (edx + edi * 4);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4fa8c7u);
    eax = esi;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 004fa960 Fn_004fa960
//   original FUN_004fa960; register inputs: eax esi; returns with ret 4
REFLEX_PORT_FN(0x004fa960, Fn_004fa960, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4fa963 = 0;
    uint32_t fB_4fa963 = 0;
    uint32_t fA_4fa982 = 0;
    uint32_t fB_4fa982 = 0;
    uint32_t fA_4fa9bf = 0;
    uint32_t fB_4fa9bf = 0;
    uint32_t fA_4fa9c7 = 0;
    uint32_t fB_4fa9c7 = 0;
    uint32_t fA_4fa9ec = 0;
    uint32_t fB_4fa9ec = 0;
    esp = esp - 0x18u;
    fA_4fa963 = rd32(m, esi + 0x48u);
    fB_4fa963 = 0x0u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x20u);
    esp -= 4; wr32(m, esp, edi);
    edi = eax;
    if (fA_4fa963 != fB_4fa963) goto L_4fa990;
    eax = ebx;
    ecx = esi;
    LIFT_CALL(0x4dca30u, 0x4fa97au);  // FUN_004dca30
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x4ebe60u, 0x4fa980u);  // FUN_004ebe60
    eax = 0u;
    fA_4fa982 = rd32(m, edi + 0x1cu);
    fB_4fa982 = eax;
    eax = (eax & 0xffffff00u) | (((fA_4fa982 != fB_4fa982) ? 1u : 0u) & 0xffu);
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(4);
L_4fa990:
    eax = rd32(m, edi + 0x18u);
    ecx = rd32(m, eax + 0x4u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = edi;
    LIFT_CALL(0x4f2020u, 0x4fa99eu);  // FUN_004f2020
    eax = rd32(m, edi + 0x18u);
    wr32(m, eax + 0x4u, eax);
    eax = rd32(m, edi + 0x18u);
    wr32(m, edi + 0x1cu, 0x0u);
    wr32(m, eax, eax);
    eax = rd32(m, edi + 0x18u);
    wr32(m, eax + 0x8u, eax);
    eax = ebx;
    ecx = esi;
    LIFT_CALL(0x4dcb20u, 0x4fa9bfu);  // FUN_004dcb20
    fA_4fa9bf = rd32(m, eax + 0x8u);
    fB_4fa9bf = 0x0u;
    if (fA_4fa9bf == fB_4fa9bf) goto L_4fa9ea;
    eax = rd32(m, eax);
    fA_4fa9c7 = eax;
    fB_4fa9c7 = 0xffffffffu;
    if (fA_4fa9c7 == fB_4fa9c7) goto L_4fa9ea;
    edx = rd32(m, edi + 0x24u);
    wr32(m, esp + 0x8u, eax);
    eax = (esp + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, edi);
    eax = (esp + 0x10u);
    wr32(m, esp + 0x14u, edi);
    wr32(m, esp + 0x18u, edx);
    LIFT_CALL(0x4f06c0u, 0x4fa9eau);  // FUN_004f06c0
L_4fa9ea:
    eax = 0u;
    fA_4fa9ec = rd32(m, edi + 0x1cu);
    fB_4fa9ec = eax;
    edi = rd32(m, esp); esp += 4;
    eax = (eax & 0xffffff00u) | (((fA_4fa9ec != fB_4fa9ec) ? 1u : 0u) & 0xffu);
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(4);
}

// 004faa00 Fn_004faa00
//   original FUN_004faa00; register inputs: edi; returns with ret 0
REFLEX_PORT_FN(0x004faa00, Fn_004faa00, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_4faa20 = 0;
    uint32_t fA_4faa24 = 0;
    uint32_t fB_4faa24 = 0;
    uint32_t fA_4faa2e = 0;
    uint32_t fB_4faa2e = 0;
    esp = esp - 0x10u;
    eax = rd32(m, esp + 0x14u);
    ecx = rd32(m, esp + 0x18u);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x20u);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x28u);
    esp -= 4; wr32(m, esp, esi);
    wr32(m, esp + 0x10u, eax);
    wr32(m, esp + 0x14u, ecx);
    edi = edi;
L_4faa20:
    fR_4faa20 = (eax & eax);
    if ((fR_4faa20 == 0)) goto L_4faa28;
    fA_4faa24 = eax;
    fB_4faa24 = ebx;
    if (fA_4faa24 == fB_4faa24) goto L_4faa2e;
L_4faa28:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4faa2eu);
L_4faa2e:
    fA_4faa2e = rd32(m, esp + 0x14u);
    fB_4faa2e = ebp;
    if (fA_4faa2e == fB_4faa2e) goto L_4faa45;
    wr32(m, edi, (rd32(m, edi) + 1u));
    esi = (esp + 0x10u);
    LIFT_CALL(0x417df0u, 0x4faa3fu);  // FUN_00417df0
    eax = rd32(m, esp + 0x10u);
    goto L_4faa20;
L_4faa45:
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x10u;
    LIFT_RET(0);
}

// 004faf10 Fn_004faf10
//   original FUN_004faf10; register inputs: edi; returns with ret 4
REFLEX_PORT_FN(0x004faf10, Fn_004faf10, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_4faf1c = 0;
    uint32_t fA_4faf36 = 0;
    uint32_t fB_4faf36 = 0;
    uint32_t fA_4faf4c = 0;
    uint32_t fB_4faf4c = 0;
    ecx = rd32(m, edi + 0xcu);
    esp = esp - 0x8u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, esi);
    fR_4faf1c = (ecx & ecx);
    if (!(fR_4faf1c == 0)) goto L_4faf24;
    eax = 0u;
    goto L_4faf2c;
L_4faf24:
    eax = rd32(m, edi + 0x14u);
    eax = eax - ecx;
    eax = uint32_t(int32_t(eax) >> 2);
L_4faf2c:
    esi = rd32(m, edi + 0x10u);
    edx = esi;
    edx = edx - ecx;
    edx = uint32_t(int32_t(edx) >> 2);
    fA_4faf36 = edx;
    fB_4faf36 = eax;
    if (fA_4faf36 >= fB_4faf36) goto L_4faf4c;
    eax = rd32(m, ebx);
    wr32(m, esi, eax);
    esi = esi + 0x4u;
    wr32(m, edi + 0x10u, esi);
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x8u;
    LIFT_RET(4);
L_4faf4c:
    fA_4faf4c = ecx;
    fB_4faf4c = esi;
    if (fA_4faf4c <= fB_4faf4c) goto L_4faf56;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4faf56u);
L_4faf56:
    eax = rd32(m, edi);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ebx);
    ecx = (esp + 0x14u);
    esp -= 4; wr32(m, esp, ecx);
    eax = edi;
    LIFT_CALL(0x4fea30u, 0x4faf67u);  // FUN_004fea30
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x8u;
    LIFT_RET(4);
}

// 004fbda0 Fn_004fbda0
//   original FUN_004fbda0; register inputs: esi; returns with ret 8
REFLEX_PORT_FN(0x004fbda0, Fn_004fbda0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_4fbdb8 = 0;
    uint32_t fR_4fbdcb = 0;
    uint32_t fA_4fbdcf = 0;
    uint32_t fB_4fbdcf = 0;
    uint32_t fA_4fbdd9 = 0;
    uint32_t fB_4fbdd9 = 0;
    uint32_t fA_4fbde2 = 0;
    uint32_t fB_4fbde2 = 0;
    uint32_t fA_4fbdee = 0;
    uint32_t fB_4fbdee = 0;
    uint32_t fR_4fbe06 = 0;
    esp = esp - 0x18u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x28u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, ebp);
    eax = esi;
    LIFT_CALL(0x502870u, 0x4fbdb2u);  // FUN_00502870
    edi = eax;
    wr32(m, esp + 0x14u, edi);
    fR_4fbdb8 = (esi & esi);
    if (!(fR_4fbdb8 == 0)) goto L_4fbdc2;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4fbdc2u);
L_4fbdc2:
    eax = rd32(m, esi);
    ebx = rd32(m, esi + 0x18u);
    wr32(m, esp + 0x10u, eax);
    fR_4fbdcb = (eax & eax);
    if ((fR_4fbdcb == 0)) goto L_4fbdd3;
    fA_4fbdcf = eax;
    fB_4fbdcf = eax;
    if (fA_4fbdcf == fB_4fbdcf) goto L_4fbdd9;
L_4fbdd3:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4fbdd9u);
L_4fbdd9:
    fA_4fbdd9 = edi;
    fB_4fbdd9 = ebx;
    if (fA_4fbdd9 == fB_4fbdd9) goto L_4fbe10;
    eax = 0x10u;
    fA_4fbde2 = rd32(m, edi + 0x24u);
    fB_4fbde2 = eax;
    if (fA_4fbde2 < fB_4fbde2) goto L_4fbdec;
    edi = rd32(m, edi + 0x10u);
    goto L_4fbdee;
L_4fbdec:
    edi = edi + eax;
L_4fbdee:
    fA_4fbdee = rd32(m, ebp + 0x18u);
    fB_4fbdee = eax;
    if (fA_4fbdee < fB_4fbdee) goto L_4fbdf8;
    eax = rd32(m, ebp + 0x4u);
    goto L_4fbdfb;
L_4fbdf8:
    eax = (ebp + 0x4u);
L_4fbdfb:
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x9162bcu) /* 8.287018476502604e-39 */, 0x4fbe03u);
    esp = esp + 0x8u;
    fR_4fbe06 = (eax & eax);
    if ((sgn(fR_4fbe06, 4) != false)) goto L_4fbe10;
    eax = (esp + 0x10u);
    goto L_4fbe21;
L_4fbe10:
    eax = rd32(m, esi + 0x18u);
    ecx = rd32(m, esi);
    wr32(m, esp + 0x1cu, eax);
    wr32(m, esp + 0x18u, ecx);
    eax = (esp + 0x18u);
L_4fbe21:
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

// 004fc440 Fn_004fc440
//   original FUN_004fc440; register inputs: eax; returns with ret 4
REFLEX_PORT_FN(0x004fc440, Fn_004fc440, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4fc44e = 0;
    uint32_t fB_4fc44e = 0;
    uint32_t fA_4fc460 = 0;
    uint32_t fB_4fc460 = 0;
    uint32_t fA_4fc46e = 0;
    uint32_t fB_4fc46e = 0;
    uint32_t fR_4fc47d = 0;
    uint32_t fA_4fc481 = 0;
    uint32_t fB_4fc481 = 0;
    uint32_t fA_4fc48b = 0;
    uint32_t fB_4fc48b = 0;
    uint32_t fA_4fc493 = 0;
    uint32_t fB_4fc493 = 0;
    uint32_t fR_4fc4a1 = 0;
    uint32_t fA_4fc519 = 0;
    uint32_t fB_4fc519 = 0;
    uint32_t fA_4fc537 = 0;
    uint32_t fB_4fc537 = 0;
    uint32_t fA_4fc55a = 0;
    uint32_t fB_4fc55a = 0;
    uint32_t fA_4fc578 = 0;
    uint32_t fB_4fc578 = 0;
    uint32_t fR_4fc58c = 0;
    uint32_t fA_4fc596 = 0;
    uint32_t fB_4fc596 = 0;
    esp = esp - 0x60u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    ecx = rd32(m, esi + 0x18u);
    eax = rd32(m, ecx + 0x4u);
    fA_4fc44e = rd8(m, eax + 0x39u);
    fB_4fc44e = 0x0u;
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esp + 0x74u);
    ebx = ecx;
    if (fA_4fc44e != fB_4fc44e) goto L_4fc474;
    ecx = rd32(m, edi);
L_4fc460:
    fA_4fc460 = rd32(m, eax + 0xcu);
    fB_4fc460 = ecx;
    if (int32_t(sext(fA_4fc460, 4)) >= int32_t(sext(fB_4fc460, 4))) goto L_4fc46a;
    eax = rd32(m, eax + 0x8u);
    goto L_4fc46e;
L_4fc46a:
    ebx = eax;
    eax = rd32(m, eax);
L_4fc46e:
    fA_4fc46e = rd8(m, eax + 0x39u);
    fB_4fc46e = 0x0u;
    if (fA_4fc46e == fB_4fc46e) goto L_4fc460;
L_4fc474:
    ebp = rd32(m, esi);
    eax = rd32(m, esi + 0x18u);
    wr32(m, esp + 0x18u, eax);
    fR_4fc47d = (ebp & ebp);
    if ((fR_4fc47d == 0)) goto L_4fc485;
    fA_4fc481 = ebp;
    fB_4fc481 = ebp;
    if (fA_4fc481 == fB_4fc481) goto L_4fc48b;
L_4fc485:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4fc48bu);
L_4fc48b:
    fA_4fc48b = ebx;
    fB_4fc48b = rd32(m, esp + 0x18u);
    if (fA_4fc48b == fB_4fc48b) goto L_4fc49c;
    ecx = rd32(m, edi);
    fA_4fc493 = ecx;
    fB_4fc493 = rd32(m, ebx + 0xcu);
    if (int32_t(sext(fA_4fc493, 4)) >= int32_t(sext(fB_4fc493, 4))) goto L_4fc58c;
L_4fc49c:
    LIFT_CALL(0x4f4a30u, 0x4fc4a1u);  // FUN_004f4a30
    fR_4fc4a1 = (eax & eax);
    if ((fR_4fc4a1 == 0)) goto L_4fc4b1;
    edx = (esp + 0x1cu);
    wr32(m, eax, edx);
    wr32(m, esp + 0x1cu, eax);
    goto L_4fc4b9;
L_4fc4b1:
    wr32(m, esp + 0x1cu, 0x0u);
L_4fc4b9:
    LIFT_CALL(0x4f4ea0u, 0x4fc4beu);  // FUN_004f4ea0
    wr32(m, esp + 0x34u, eax);
    wr8(m, eax + 0x19u, 0x1u);
    eax = rd32(m, esp + 0x34u);
    wr32(m, eax + 0x4u, eax);
    eax = rd32(m, esp + 0x34u);
    wr32(m, eax, eax);
    eax = rd32(m, esp + 0x34u);
    wr32(m, eax + 0x8u, eax);
    eax = 0u;
    wr32(m, esp + 0x38u, eax);
    wr32(m, esp + 0x3cu, eax);
    wr32(m, esp + 0x40u, eax);
    eax = rd32(m, edi);
    ecx = (esp + 0x1cu);
    esp -= 4; wr32(m, esp, ecx);
    edi = (esp + 0x4cu);
    wr32(m, esp + 0x48u, eax);
    LIFT_CALL(0x4ebce0u, 0x4fc4fcu);  // FUN_004ebce0
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    edx = (esp + 0x4cu);
    esp -= 4; wr32(m, esp, edx);
    eax = (esp + 0x20u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x4ffcb0u, 0x4fc50eu);  // FUN_004ffcb0
    esi = rd32(m, esp + 0x68u);
    ebp = rd32(m, eax);
    ebx = rd32(m, eax + 0x4u);
    edi = 0u;
    fA_4fc519 = esi;
    fB_4fc519 = edi;
    if (fA_4fc519 == fB_4fc519) goto L_4fc526;
    LIFT_CALL(0x4ebe10u, 0x4fc522u);  // FUN_004ebe10
    wr32(m, esp + 0x68u, edi);
L_4fc526:
    eax = (esp + 0x48u);
    wr32(m, esp + 0x6cu, edi);
    LIFT_CALL(0x4f0670u, 0x4fc533u);  // FUN_004f0670
    esi = rd32(m, esp + 0x3cu);
    fA_4fc537 = esi;
    fB_4fc537 = edi;
    if (fA_4fc537 == fB_4fc537) goto L_4fc544;
    LIFT_CALL(0x4ebe10u, 0x4fc540u);  // FUN_004ebe10
    wr32(m, esp + 0x3cu, edi);
L_4fc544:
    esi = (esp + 0x1cu);
    wr32(m, esp + 0x40u, edi);
    LIFT_CALL(0x4f2220u, 0x4fc551u);  // FUN_004f2220
    esi = rd32(m, esp + 0x1cu);
    LIFT_CALL(0x4dc0f0u, 0x4fc55au);  // FUN_004dc0f0
    fA_4fc55a = rd8(m, 0xda67d8u);
    fB_4fc55a = 0x0u;
    if (fA_4fc55a == fB_4fc55a) goto L_4fc56e;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x4fc56eu);
L_4fc56e:
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(rd32(m, 0x916350u) /* 8.287536956934404e-39 */, 0x4fc575u);
    esp = esp + 0x4u;
    fA_4fc578 = rd8(m, 0xda67d8u);
    fB_4fc578 = 0x0u;
    if (fA_4fc578 == fB_4fc578) goto L_4fc58c;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4fc58cu);
L_4fc58c:
    fR_4fc58c = (ebp & ebp);
    if (!(fR_4fc58c == 0)) goto L_4fc5ae;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4fc596u);
L_4fc596:
    fA_4fc596 = ebx;
    fB_4fc596 = rd32(m, ebp + 0x18u);
    if (fA_4fc596 != fB_4fc596) goto L_4fc5a1;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4fc5a1u);
L_4fc5a1:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = (ebx + 0x10u);
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x60u;
    LIFT_RET(4);
L_4fc5ae:
    ebp = rd32(m, ebp);
    goto L_4fc596;
}

// 004fc5f0 Fn_004fc5f0
//   original FUN_004fc5f0; register inputs: none; returns with ret 8
REFLEX_PORT_FN(0x004fc5f0, Fn_004fc5f0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4fc602 = 0;
    uint32_t fB_4fc602 = 0;
    uint32_t fA_4fc610 = 0;
    uint32_t fB_4fc610 = 0;
    uint32_t fA_4fc61e = 0;
    uint32_t fB_4fc61e = 0;
    uint32_t fR_4fc633 = 0;
    uint32_t fA_4fc637 = 0;
    uint32_t fB_4fc637 = 0;
    uint32_t fA_4fc647 = 0;
    uint32_t fB_4fc647 = 0;
    uint32_t fA_4fc64f = 0;
    uint32_t fB_4fc64f = 0;
    uint32_t fR_4fc68f = 0;
    uint32_t fA_4fc695 = 0;
    uint32_t fB_4fc695 = 0;
    edx = rd32(m, esp + 0x8u);
    esp = esp - 0x24u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x2cu);
    ecx = rd32(m, ebx + 0x18u);
    eax = rd32(m, ecx + 0x4u);
    fA_4fc602 = rd8(m, eax + 0x1du);
    fB_4fc602 = 0x0u;
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = ecx;
    if (fA_4fc602 != fB_4fc602) goto L_4fc624;
    ecx = rd32(m, edx);
L_4fc610:
    fA_4fc610 = rd32(m, eax + 0xcu);
    fB_4fc610 = ecx;
    if (int32_t(sext(fA_4fc610, 4)) >= int32_t(sext(fB_4fc610, 4))) goto L_4fc61a;
    eax = rd32(m, eax + 0x8u);
    goto L_4fc61e;
L_4fc61a:
    edi = eax;
    eax = rd32(m, eax);
L_4fc61e:
    fA_4fc61e = rd8(m, eax + 0x1du);
    fB_4fc61e = 0x0u;
    if (fA_4fc61e == fB_4fc61e) goto L_4fc610;
L_4fc624:
    esi = rd32(m, ebx);
    eax = rd32(m, ebx + 0x18u);
    ebp = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    wr32(m, esp + 0x18u, eax);
    fR_4fc633 = (esi & esi);
    if ((fR_4fc633 == 0)) goto L_4fc63b;
    fA_4fc637 = esi;
    fB_4fc637 = esi;
    if (fA_4fc637 == fB_4fc637) goto L_4fc647;
L_4fc63b:
    LIFT_CALL(ebp, 0x4fc63du);
    edx = rd32(m, esp + 0x3cu);
    ebp = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
L_4fc647:
    fA_4fc647 = edi;
    fB_4fc647 = rd32(m, esp + 0x18u);
    if (fA_4fc647 == fB_4fc647) goto L_4fc654;
    ecx = rd32(m, edx);
    fA_4fc64f = ecx;
    fB_4fc64f = rd32(m, edi + 0xcu);
    if (int32_t(sext(fA_4fc64f, 4)) >= int32_t(sext(fB_4fc64f, 4))) goto L_4fc68f;
L_4fc654:
    edx = rd32(m, edx);
    eax = 0u;
    esp -= 4; wr32(m, esp, edi);
    wr32(m, esp + 0x1cu, eax);
    esp -= 4; wr32(m, esp, esi);
    wr32(m, esp + 0x34u, eax);
    eax = (esp + 0x28u);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0x20u);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp + 0x24u, 0xffffffffu);
    x0 = movsd_m(rd64(m, esp + 0x24u));
    esp -= 4; wr32(m, esp, ebx);
    wr32(m, esp + 0x34u, edx);
    wr64(m, esp + 0x38u, x0.lo);
    LIFT_CALL(0x4fff20u, 0x4fc68au);  // FUN_004fff20
    esi = rd32(m, eax);
    edi = rd32(m, eax + 0x4u);
L_4fc68f:
    fR_4fc68f = (esi & esi);
    if (!(fR_4fc68f == 0)) goto L_4fc6a9;
    LIFT_CALL(ebp, 0x4fc695u);
L_4fc695:
    fA_4fc695 = edi;
    fB_4fc695 = rd32(m, esi + 0x18u);
    if (fA_4fc695 != fB_4fc695) goto L_4fc69c;
    LIFT_CALL(ebp, 0x4fc69cu);
L_4fc69c:
    eax = (edi + 0x10u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x24u;
    LIFT_RET(8);
L_4fc6a9:
    esi = rd32(m, esi);
    goto L_4fc695;
}

// 004fc6b0 Fn_004fc6b0
//   original FUN_004fc6b0; register inputs: none; returns with ret 16
REFLEX_PORT_FN(0x004fc6b0, Fn_004fc6b0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4fc6b7 = 0;
    uint32_t fB_4fc6b7 = 0;
    uint32_t fA_4fc716 = 0;
    uint32_t fB_4fc716 = 0;
    uint32_t fA_4fc724 = 0;
    uint32_t fB_4fc724 = 0;
    uint32_t fA_4fc738 = 0;
    uint32_t fB_4fc738 = 0;
    uint32_t fA_4fc73c = 0;
    uint32_t fB_4fc73c = 0;
    uint32_t fA_4fc74f = 0;
    uint32_t fB_4fc74f = 0;
    uint32_t fA_4fc759 = 0;
    uint32_t fB_4fc759 = 0;
    uint32_t fA_4fc767 = 0;
    uint32_t fB_4fc767 = 0;
    uint32_t fA_4fc76b = 0;
    uint32_t fB_4fc76b = 0;
    uint32_t fA_4fc777 = 0;
    uint32_t fB_4fc777 = 0;
    uint32_t fA_4fc784 = 0;
    uint32_t fB_4fc784 = 0;
    uint32_t fA_4fc793 = 0;
    uint32_t fB_4fc793 = 0;
    uint32_t fA_4fc798 = 0;
    uint32_t fB_4fc798 = 0;
    uint32_t fA_4fc7b9 = 0;
    uint32_t fB_4fc7b9 = 0;
    uint32_t fA_4fc7c2 = 0;
    uint32_t fB_4fc7c2 = 0;
    uint32_t fA_4fc7e2 = 0;
    uint32_t fB_4fc7e2 = 0;
    uint32_t fA_4fc7ef = 0;
    uint32_t fB_4fc7ef = 0;
    uint32_t fA_4fc806 = 0;
    uint32_t fB_4fc806 = 0;
    uint32_t fA_4fc814 = 0;
    uint32_t fB_4fc814 = 0;
    uint32_t fA_4fc824 = 0;
    uint32_t fB_4fc824 = 0;
    uint32_t fA_4fc830 = 0;
    uint32_t fB_4fc830 = 0;
    uint32_t fA_4fc83b = 0;
    uint32_t fB_4fc83b = 0;
    uint32_t fA_4fc842 = 0;
    uint32_t fB_4fc842 = 0;
    uint32_t fA_4fc85e = 0;
    uint32_t fB_4fc85e = 0;
    uint32_t fA_4fc86a = 0;
    uint32_t fB_4fc86a = 0;
    uint32_t fA_4fc872 = 0;
    uint32_t fB_4fc872 = 0;
    uint32_t fA_4fc87a = 0;
    uint32_t fB_4fc87a = 0;
    uint32_t fA_4fc8b5 = 0;
    uint32_t fB_4fc8b5 = 0;
    uint32_t fA_4fc8d0 = 0;
    uint32_t fB_4fc8d0 = 0;
    uint32_t fA_4fc8d9 = 0;
    uint32_t fB_4fc8d9 = 0;
    uint32_t fA_4fc8e0 = 0;
    uint32_t fB_4fc8e0 = 0;
    uint32_t fA_4fc8f5 = 0;
    uint32_t fB_4fc8f5 = 0;
    uint32_t fA_4fc902 = 0;
    uint32_t fB_4fc902 = 0;
    uint32_t fA_4fc93f = 0;
    uint32_t fB_4fc93f = 0;
    uint32_t fA_4fc95d = 0;
    uint32_t fB_4fc95d = 0;
    uint32_t fR_4fc978 = 0;
    eax = rd32(m, esp + 0x10u);
    esp = esp - 0x4cu;
    fA_4fc6b7 = rd8(m, eax + 0x1du);
    fB_4fc6b7 = 0x0u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    if (fA_4fc6b7 == fB_4fc6b7) goto L_4fc708;
    esp -= 4; wr32(m, esp, 0x937034u);
    ecx = (esp + 0x18u);
    LIFT_CALL(rd32(m, 0x9161acu) /* 8.280765882754786e-39 */, 0x4fc6d0u);
    ecx = (esp + 0x30u);
    LIFT_CALL(rd32(m, 0x916210u) /* 8.286239354556439e-39 */, 0x4fc6dau);
    eax = (esp + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0x40u);
    wr32(m, esp + 0x34u, 0x9177c4u);
    LIFT_CALL(rd32(m, 0x9161b0u) /* 8.28065377887764e-39 */, 0x4fc6f1u);
    esp -= 4; wr32(m, esp, 0x99bc48u);
    ecx = (esp + 0x34u);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp + 0x38u, 0x9177dcu);
    LIFT_CALL(0x8fa07cu, 0x4fc708u);  // MSVCR90.DLL::_CxxThrowException
L_4fc708:
    esi = (esp + 0x68u);
    ebp = eax;
    LIFT_CALL(0x503750u, 0x4fc713u);  // FUN_00503750
    ecx = rd32(m, ebp);
    fA_4fc716 = rd8(m, ecx + 0x1du);
    fB_4fc716 = 0x0u;
    if (fA_4fc716 == fB_4fc716) goto L_4fc721;
    edi = rd32(m, ebp + 0x8u);
    goto L_4fc73c;
L_4fc721:
    edx = rd32(m, ebp + 0x8u);
    fA_4fc724 = rd8(m, edx + 0x1du);
    fB_4fc724 = 0x0u;
    if (fA_4fc724 == fB_4fc724) goto L_4fc72e;
    edi = ecx;
    goto L_4fc73c;
L_4fc72e:
    eax = rd32(m, esp + 0x6cu);
    edi = rd32(m, eax + 0x8u);
    edx = (eax + 0x8u);
    fA_4fc738 = eax;
    fB_4fc738 = ebp;
    if (fA_4fc738 != fB_4fc738) goto L_4fc7b1;
L_4fc73c:
    fA_4fc73c = rd8(m, edi + 0x1du);
    fB_4fc73c = 0x0u;
    esi = rd32(m, ebp + 0x4u);
    if (fA_4fc73c != fB_4fc73c) goto L_4fc748;
    wr32(m, edi + 0x4u, esi);
L_4fc748:
    ecx = rd32(m, esp + 0x60u);
    eax = rd32(m, ecx + 0x18u);
    fA_4fc74f = rd32(m, eax + 0x4u);
    fB_4fc74f = ebp;
    if (fA_4fc74f != fB_4fc74f) goto L_4fc759;
    wr32(m, eax + 0x4u, edi);
    goto L_4fc764;
L_4fc759:
    fA_4fc759 = rd32(m, esi);
    fB_4fc759 = ebp;
    if (fA_4fc759 != fB_4fc759) goto L_4fc761;
    wr32(m, esi, edi);
    goto L_4fc764;
L_4fc761:
    wr32(m, esi + 0x8u, edi);
L_4fc764:
    edx = rd32(m, ecx + 0x18u);
    fA_4fc767 = rd32(m, edx);
    fB_4fc767 = ebp;
    if (fA_4fc767 != fB_4fc767) goto L_4fc78c;
    fA_4fc76b = rd8(m, edi + 0x1du);
    fB_4fc76b = 0x0u;
    if (fA_4fc76b == fB_4fc76b) goto L_4fc775;
    ecx = esi;
    goto L_4fc78a;
L_4fc775:
    eax = rd32(m, edi);
    fA_4fc777 = rd8(m, eax + 0x1du);
    fB_4fc777 = 0x0u;
    ecx = edi;
    if (fA_4fc777 != fB_4fc777) goto L_4fc78a;
L_4fc780:
    ecx = eax;
    eax = rd32(m, ecx);
    fA_4fc784 = rd8(m, eax + 0x1du);
    fB_4fc784 = 0x0u;
    if (fA_4fc784 == fB_4fc784) goto L_4fc780;
L_4fc78a:
    wr32(m, edx, ecx);
L_4fc78c:
    eax = rd32(m, esp + 0x60u);
    ebx = rd32(m, eax + 0x18u);
    fA_4fc793 = rd32(m, ebx + 0x8u);
    fB_4fc793 = ebp;
    if (fA_4fc793 != fB_4fc793) goto L_4fc812;
    fA_4fc798 = rd8(m, edi + 0x1du);
    fB_4fc798 = 0x0u;
    if (fA_4fc798 == fB_4fc798) goto L_4fc7a5;
    eax = esi;
    wr32(m, ebx + 0x8u, eax);
    goto L_4fc812;
L_4fc7a5:
    eax = edi;
    LIFT_CALL(0x5006e0u, 0x4fc7acu);  // FUN_005006e0
    wr32(m, ebx + 0x8u, eax);
    goto L_4fc812;
L_4fc7b1:
    wr32(m, ecx + 0x4u, eax);
    ecx = rd32(m, ebp);
    wr32(m, eax, ecx);
    fA_4fc7b9 = eax;
    fB_4fc7b9 = rd32(m, ebp + 0x8u);
    if (fA_4fc7b9 != fB_4fc7b9) goto L_4fc7c2;
    esi = eax;
    goto L_4fc7db;
L_4fc7c2:
    fA_4fc7c2 = rd8(m, edi + 0x1du);
    fB_4fc7c2 = 0x0u;
    esi = rd32(m, eax + 0x4u);
    if (fA_4fc7c2 != fB_4fc7c2) goto L_4fc7ce;
    wr32(m, edi + 0x4u, esi);
L_4fc7ce:
    wr32(m, esi, edi);
    ecx = rd32(m, ebp + 0x8u);
    wr32(m, edx, ecx);
    edx = rd32(m, ebp + 0x8u);
    wr32(m, edx + 0x4u, eax);
L_4fc7db:
    ecx = rd32(m, esp + 0x60u);
    ecx = rd32(m, ecx + 0x18u);
    fA_4fc7e2 = rd32(m, ecx + 0x4u);
    fB_4fc7e2 = ebp;
    if (fA_4fc7e2 != fB_4fc7e2) goto L_4fc7ec;
    wr32(m, ecx + 0x4u, eax);
    goto L_4fc7fa;
L_4fc7ec:
    ecx = rd32(m, ebp + 0x4u);
    fA_4fc7ef = rd32(m, ecx);
    fB_4fc7ef = ebp;
    if (fA_4fc7ef != fB_4fc7ef) goto L_4fc7f7;
    wr32(m, ecx, eax);
    goto L_4fc7fa;
L_4fc7f7:
    wr32(m, ecx + 0x8u, eax);
L_4fc7fa:
    edx = rd32(m, ebp + 0x4u);
    wr32(m, eax + 0x4u, edx);
    ecx = (ebp + 0x1cu);
    eax = eax + 0x1cu;
    fA_4fc806 = eax;
    fB_4fc806 = ecx;
    if (fA_4fc806 == fB_4fc806) goto L_4fc812;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    edx = (edx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    wr8(m, eax, (ebx & 0xffu));
    wr8(m, ecx, (edx & 0xffu));
L_4fc812:
    ebx = (ebx & 0xffffff00u) | ((0x1u) & 0xffu);
    fA_4fc814 = rd8(m, ebp + 0x1cu);
    fB_4fc814 = (ebx & 0xffu);
    if (fA_4fc814 != fB_4fc814) goto L_4fc93a;
    eax = rd32(m, esp + 0x60u);
    ecx = rd32(m, eax + 0x18u);
    fA_4fc824 = edi;
    fB_4fc824 = rd32(m, ecx + 0x4u);
    if (fA_4fc824 == fB_4fc824) goto L_4fc937;
L_4fc830:
    fA_4fc830 = rd8(m, edi + 0x1cu);
    fB_4fc830 = (ebx & 0xffu);
    if (fA_4fc830 != fB_4fc830) goto L_4fc937;
    ecx = rd32(m, esi);
    fA_4fc83b = edi;
    fB_4fc83b = ecx;
    if (fA_4fc83b != fB_4fc83b) goto L_4fc8b5;
    ecx = rd32(m, esi + 0x8u);
    fA_4fc842 = rd8(m, ecx + 0x1cu);
    fB_4fc842 = 0x0u;
    if (fA_4fc842 != fB_4fc842) goto L_4fc85e;
    edx = rd32(m, esp + 0x60u);
    wr8(m, ecx + 0x1cu, (ebx & 0xffu));
    esp -= 4; wr32(m, esp, edx);
    ecx = esi;
    wr8(m, esi + 0x1cu, 0x0u);
    LIFT_CALL(0x765c90u, 0x4fc85bu);  // FUN_00765c90
    ecx = rd32(m, esi + 0x8u);
L_4fc85e:
    fA_4fc85e = rd8(m, ecx + 0x1du);
    fB_4fc85e = 0x0u;
    if (fA_4fc85e != fB_4fc85e) goto L_4fc8e9;
    eax = rd32(m, ecx);
    fA_4fc86a = rd8(m, eax + 0x1cu);
    fB_4fc86a = (ebx & 0xffu);
    if (fA_4fc86a != fB_4fc86a) goto L_4fc877;
    edx = rd32(m, ecx + 0x8u);
    fA_4fc872 = rd8(m, edx + 0x1cu);
    fB_4fc872 = (ebx & 0xffu);
    if (fA_4fc872 == fB_4fc872) goto L_4fc8e5;
L_4fc877:
    eax = rd32(m, ecx + 0x8u);
    fA_4fc87a = rd8(m, eax + 0x1cu);
    fB_4fc87a = (ebx & 0xffu);
    if (fA_4fc87a != fB_4fc87a) goto L_4fc895;
    edx = rd32(m, ecx);
    eax = rd32(m, esp + 0x60u);
    wr8(m, edx + 0x1cu, (ebx & 0xffu));
    esp -= 4; wr32(m, esp, eax);
    wr8(m, ecx + 0x1cu, 0x0u);
    LIFT_CALL(0x765ce0u, 0x4fc892u);  // FUN_00765ce0
    ecx = rd32(m, esi + 0x8u);
L_4fc895:
    edx = (edx & 0xffffff00u) | ((rd8(m, esi + 0x1cu)) & 0xffu);
    wr8(m, ecx + 0x1cu, (edx & 0xffu));
    wr8(m, esi + 0x1cu, (ebx & 0xffu));
    eax = rd32(m, ecx + 0x8u);
    ecx = rd32(m, esp + 0x60u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = esi;
    wr8(m, eax + 0x1cu, (ebx & 0xffu));
    LIFT_CALL(0x765c90u, 0x4fc8b0u);  // FUN_00765c90
    goto L_4fc937;
L_4fc8b5:
    fA_4fc8b5 = rd8(m, ecx + 0x1cu);
    fB_4fc8b5 = 0x0u;
    if (fA_4fc8b5 != fB_4fc8b5) goto L_4fc8d0;
    eax = rd32(m, esp + 0x60u);
    wr8(m, ecx + 0x1cu, (ebx & 0xffu));
    esp -= 4; wr32(m, esp, eax);
    ecx = esi;
    wr8(m, esi + 0x1cu, 0x0u);
    LIFT_CALL(0x765ce0u, 0x4fc8ceu);  // FUN_00765ce0
    ecx = rd32(m, esi);
L_4fc8d0:
    fA_4fc8d0 = rd8(m, ecx + 0x1du);
    fB_4fc8d0 = 0x0u;
    if (fA_4fc8d0 != fB_4fc8d0) goto L_4fc8e9;
    edx = rd32(m, ecx + 0x8u);
    fA_4fc8d9 = rd8(m, edx + 0x1cu);
    fB_4fc8d9 = (ebx & 0xffu);
    if (fA_4fc8d9 != fB_4fc8d9) goto L_4fc900;
    eax = rd32(m, ecx);
    fA_4fc8e0 = rd8(m, eax + 0x1cu);
    fB_4fc8e0 = (ebx & 0xffu);
    if (fA_4fc8e0 != fB_4fc8e0) goto L_4fc900;
L_4fc8e5:
    wr8(m, ecx + 0x1cu, 0x0u);
L_4fc8e9:
    ecx = rd32(m, esp + 0x60u);
    edx = rd32(m, ecx + 0x18u);
    edi = esi;
    esi = rd32(m, esi + 0x4u);
    fA_4fc8f5 = edi;
    fB_4fc8f5 = rd32(m, edx + 0x4u);
    if (fA_4fc8f5 != fB_4fc8f5) goto L_4fc830;
    goto L_4fc937;
L_4fc900:
    edx = rd32(m, ecx);
    fA_4fc902 = rd8(m, edx + 0x1cu);
    fB_4fc902 = (ebx & 0xffu);
    if (fA_4fc902 != fB_4fc902) goto L_4fc91d;
    eax = rd32(m, ecx + 0x8u);
    edx = rd32(m, esp + 0x60u);
    wr8(m, eax + 0x1cu, (ebx & 0xffu));
    esp -= 4; wr32(m, esp, edx);
    wr8(m, ecx + 0x1cu, 0x0u);
    LIFT_CALL(0x765c90u, 0x4fc91bu);  // FUN_00765c90
    ecx = rd32(m, esi);
L_4fc91d:
    eax = (eax & 0xffffff00u) | ((rd8(m, esi + 0x1cu)) & 0xffu);
    edx = rd32(m, esp + 0x60u);
    wr8(m, ecx + 0x1cu, (eax & 0xffu));
    wr8(m, esi + 0x1cu, (ebx & 0xffu));
    ecx = rd32(m, ecx);
    wr8(m, ecx + 0x1cu, (ebx & 0xffu));
    esp -= 4; wr32(m, esp, edx);
    ecx = esi;
    LIFT_CALL(0x765ce0u, 0x4fc937u);  // FUN_00765ce0
L_4fc937:
    wr8(m, edi + 0x1cu, (ebx & 0xffu));
L_4fc93a:
    LIFT_CALL(0x4dc0f0u, 0x4fc93fu);  // FUN_004dc0f0
    fA_4fc93f = rd8(m, 0xda67d8u);
    fB_4fc93f = 0x0u;
    if (fA_4fc93f == fB_4fc93f) goto L_4fc953;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x4fc953u);
L_4fc953:
    esp -= 4; wr32(m, esp, ebp);
    LIFT_CALL(rd32(m, 0x916350u) /* 8.287536956934404e-39 */, 0x4fc95au);
    esp = esp + 0x4u;
    fA_4fc95d = rd8(m, 0xda67d8u);
    fB_4fc95d = 0x0u;
    if (fA_4fc95d == fB_4fc95d) goto L_4fc971;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4fc971u);
L_4fc971:
    ecx = rd32(m, esp + 0x60u);
    eax = rd32(m, ecx + 0x1cu);
    fR_4fc978 = (eax & eax);
    if ((false || (fR_4fc978 == 0))) goto L_4fc980;
    eax = (eax - 1u);
    wr32(m, ecx + 0x1cu, eax);
L_4fc980:
    edx = rd32(m, esp + 0x64u);
    eax = rd32(m, esp + 0x6cu);
    ecx = rd32(m, ecx);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    wr32(m, edx + 0x4u, eax);
    wr32(m, edx, ecx);
    eax = edx;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x4cu;
    LIFT_RET(16);
}

// 004fcb60 Fn_004fcb60
//   original FUN_004fcb60; register inputs: none; returns with ret 16
REFLEX_PORT_FN(0x004fcb60, Fn_004fcb60, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4fcb67 = 0;
    uint32_t fB_4fcb67 = 0;
    uint32_t fA_4fcbc9 = 0;
    uint32_t fB_4fcbc9 = 0;
    uint32_t fA_4fcbd7 = 0;
    uint32_t fB_4fcbd7 = 0;
    uint32_t fA_4fcbeb = 0;
    uint32_t fB_4fcbeb = 0;
    uint32_t fA_4fcbef = 0;
    uint32_t fB_4fcbef = 0;
    uint32_t fA_4fcc02 = 0;
    uint32_t fB_4fcc02 = 0;
    uint32_t fA_4fcc0c = 0;
    uint32_t fB_4fcc0c = 0;
    uint32_t fA_4fcc1a = 0;
    uint32_t fB_4fcc1a = 0;
    uint32_t fA_4fcc1e = 0;
    uint32_t fB_4fcc1e = 0;
    uint32_t fA_4fcc2a = 0;
    uint32_t fB_4fcc2a = 0;
    uint32_t fA_4fcc36 = 0;
    uint32_t fB_4fcc36 = 0;
    uint32_t fA_4fcc45 = 0;
    uint32_t fB_4fcc45 = 0;
    uint32_t fA_4fcc4a = 0;
    uint32_t fB_4fcc4a = 0;
    uint32_t fA_4fcc6a = 0;
    uint32_t fB_4fcc6a = 0;
    uint32_t fA_4fcc73 = 0;
    uint32_t fB_4fcc73 = 0;
    uint32_t fA_4fcc93 = 0;
    uint32_t fB_4fcc93 = 0;
    uint32_t fA_4fcca0 = 0;
    uint32_t fB_4fcca0 = 0;
    uint32_t fA_4fccbb = 0;
    uint32_t fB_4fccbb = 0;
    uint32_t fA_4fcccd = 0;
    uint32_t fB_4fcccd = 0;
    uint32_t fA_4fccd9 = 0;
    uint32_t fB_4fccd9 = 0;
    uint32_t fA_4fcce2 = 0;
    uint32_t fB_4fcce2 = 0;
    uint32_t fA_4fcced = 0;
    uint32_t fB_4fcced = 0;
    uint32_t fA_4fccf4 = 0;
    uint32_t fB_4fccf4 = 0;
    uint32_t fA_4fcd0c = 0;
    uint32_t fB_4fcd0c = 0;
    uint32_t fA_4fcd14 = 0;
    uint32_t fB_4fcd14 = 0;
    uint32_t fA_4fcd1c = 0;
    uint32_t fB_4fcd1c = 0;
    uint32_t fA_4fcd24 = 0;
    uint32_t fB_4fcd24 = 0;
    uint32_t fA_4fcd54 = 0;
    uint32_t fB_4fcd54 = 0;
    uint32_t fA_4fcd6b = 0;
    uint32_t fB_4fcd6b = 0;
    uint32_t fA_4fcd74 = 0;
    uint32_t fB_4fcd74 = 0;
    uint32_t fA_4fcd7b = 0;
    uint32_t fB_4fcd7b = 0;
    uint32_t fA_4fcd8c = 0;
    uint32_t fB_4fcd8c = 0;
    uint32_t fA_4fcd99 = 0;
    uint32_t fB_4fcd99 = 0;
    uint32_t fA_4fcdd5 = 0;
    uint32_t fB_4fcdd5 = 0;
    uint32_t fA_4fcdf2 = 0;
    uint32_t fB_4fcdf2 = 0;
    uint32_t fA_4fce1a = 0;
    uint32_t fB_4fce1a = 0;
    uint32_t fA_4fce2f = 0;
    uint32_t fB_4fce2f = 0;
    uint32_t fA_4fce49 = 0;
    uint32_t fB_4fce49 = 0;
    uint32_t fA_4fce60 = 0;
    uint32_t fB_4fce60 = 0;
    eax = rd32(m, esp + 0x10u);
    esp = esp - 0x4cu;
    fA_4fcb67 = rd8(m, eax + 0x39u);
    fB_4fcb67 = 0x0u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    if (fA_4fcb67 == fB_4fcb67) goto L_4fcbb8;
    esp -= 4; wr32(m, esp, 0x937034u);
    ecx = (esp + 0x18u);
    LIFT_CALL(rd32(m, 0x9161acu) /* 8.280765882754786e-39 */, 0x4fcb80u);
    ecx = (esp + 0x30u);
    LIFT_CALL(rd32(m, 0x916210u) /* 8.286239354556439e-39 */, 0x4fcb8au);
    eax = (esp + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0x40u);
    wr32(m, esp + 0x34u, 0x9177c4u);
    LIFT_CALL(rd32(m, 0x9161b0u) /* 8.28065377887764e-39 */, 0x4fcba1u);
    esp -= 4; wr32(m, esp, 0x99bc48u);
    ecx = (esp + 0x34u);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp + 0x38u, 0x9177dcu);
    LIFT_CALL(0x8fa07cu, 0x4fcbb8u);  // MSVCR90.DLL::_CxxThrowException
L_4fcbb8:
    ebx = eax;
    esi = (esp + 0x68u);
    wr32(m, esp + 0x10u, ebx);
    LIFT_CALL(0x503440u, 0x4fcbc7u);  // FUN_00503440
    ecx = rd32(m, ebx);
    fA_4fcbc9 = rd8(m, ecx + 0x39u);
    fB_4fcbc9 = 0x0u;
    if (fA_4fcbc9 == fB_4fcbc9) goto L_4fcbd4;
    edi = rd32(m, ebx + 0x8u);
    goto L_4fcbef;
L_4fcbd4:
    edx = rd32(m, ebx + 0x8u);
    fA_4fcbd7 = rd8(m, edx + 0x39u);
    fB_4fcbd7 = 0x0u;
    if (fA_4fcbd7 == fB_4fcbd7) goto L_4fcbe1;
    edi = ecx;
    goto L_4fcbef;
L_4fcbe1:
    eax = rd32(m, esp + 0x6cu);
    edi = rd32(m, eax + 0x8u);
    edx = (eax + 0x8u);
    fA_4fcbeb = eax;
    fB_4fcbeb = ebx;
    if (fA_4fcbeb != fB_4fcbeb) goto L_4fcc63;
L_4fcbef:
    fA_4fcbef = rd8(m, edi + 0x39u);
    fB_4fcbef = 0x0u;
    esi = rd32(m, ebx + 0x4u);
    if (fA_4fcbef != fB_4fcbef) goto L_4fcbfb;
    wr32(m, edi + 0x4u, esi);
L_4fcbfb:
    ebp = rd32(m, esp + 0x60u);
    eax = rd32(m, ebp + 0x18u);
    fA_4fcc02 = rd32(m, eax + 0x4u);
    fB_4fcc02 = ebx;
    if (fA_4fcc02 != fB_4fcc02) goto L_4fcc0c;
    wr32(m, eax + 0x4u, edi);
    goto L_4fcc17;
L_4fcc0c:
    fA_4fcc0c = rd32(m, esi);
    fB_4fcc0c = ebx;
    if (fA_4fcc0c != fB_4fcc0c) goto L_4fcc14;
    wr32(m, esi, edi);
    goto L_4fcc17;
L_4fcc14:
    wr32(m, esi + 0x8u, edi);
L_4fcc17:
    edx = rd32(m, ebp + 0x18u);
    fA_4fcc1a = rd32(m, edx);
    fB_4fcc1a = ebx;
    if (fA_4fcc1a != fB_4fcc1a) goto L_4fcc3e;
    fA_4fcc1e = rd8(m, edi + 0x39u);
    fB_4fcc1e = 0x0u;
    if (fA_4fcc1e == fB_4fcc1e) goto L_4fcc28;
    ecx = esi;
    goto L_4fcc3c;
L_4fcc28:
    eax = rd32(m, edi);
    fA_4fcc2a = rd8(m, eax + 0x39u);
    fB_4fcc2a = 0x0u;
    ecx = edi;
    if (fA_4fcc2a != fB_4fcc2a) goto L_4fcc3c;
L_4fcc32:
    ecx = eax;
    eax = rd32(m, ecx);
    fA_4fcc36 = rd8(m, eax + 0x39u);
    fB_4fcc36 = 0x0u;
    if (fA_4fcc36 == fB_4fcc36) goto L_4fcc32;
L_4fcc3c:
    wr32(m, edx, ecx);
L_4fcc3e:
    ebx = rd32(m, ebp + 0x18u);
    eax = rd32(m, esp + 0x10u);
    fA_4fcc45 = rd32(m, ebx + 0x8u);
    fB_4fcc45 = eax;
    if (fA_4fcc45 != fB_4fcc45) goto L_4fccc7;
    fA_4fcc4a = rd8(m, edi + 0x39u);
    fB_4fcc4a = 0x0u;
    if (fA_4fcc4a == fB_4fcc4a) goto L_4fcc57;
    eax = esi;
    wr32(m, ebx + 0x8u, eax);
    goto L_4fccc7;
L_4fcc57:
    eax = edi;
    LIFT_CALL(0x5004d0u, 0x4fcc5eu);  // FUN_005004d0
    wr32(m, ebx + 0x8u, eax);
    goto L_4fccc7;
L_4fcc63:
    wr32(m, ecx + 0x4u, eax);
    ecx = rd32(m, ebx);
    wr32(m, eax, ecx);
    fA_4fcc6a = eax;
    fB_4fcc6a = rd32(m, ebx + 0x8u);
    if (fA_4fcc6a != fB_4fcc6a) goto L_4fcc73;
    esi = eax;
    goto L_4fcc8c;
L_4fcc73:
    fA_4fcc73 = rd8(m, edi + 0x39u);
    fB_4fcc73 = 0x0u;
    esi = rd32(m, eax + 0x4u);
    if (fA_4fcc73 != fB_4fcc73) goto L_4fcc7f;
    wr32(m, edi + 0x4u, esi);
L_4fcc7f:
    wr32(m, esi, edi);
    ecx = rd32(m, ebx + 0x8u);
    wr32(m, edx, ecx);
    edx = rd32(m, ebx + 0x8u);
    wr32(m, edx + 0x4u, eax);
L_4fcc8c:
    ecx = rd32(m, esp + 0x60u);
    ecx = rd32(m, ecx + 0x18u);
    fA_4fcc93 = rd32(m, ecx + 0x4u);
    fB_4fcc93 = ebx;
    if (fA_4fcc93 != fB_4fcc93) goto L_4fcc9d;
    wr32(m, ecx + 0x4u, eax);
    goto L_4fccab;
L_4fcc9d:
    ecx = rd32(m, ebx + 0x4u);
    fA_4fcca0 = rd32(m, ecx);
    fB_4fcca0 = ebx;
    if (fA_4fcca0 != fB_4fcca0) goto L_4fcca8;
    wr32(m, ecx, eax);
    goto L_4fccab;
L_4fcca8:
    wr32(m, ecx + 0x8u, eax);
L_4fccab:
    edx = rd32(m, ebx + 0x4u);
    ebp = rd32(m, esp + 0x60u);
    wr32(m, eax + 0x4u, edx);
    ecx = (ebx + 0x38u);
    eax = eax + 0x38u;
    fA_4fccbb = eax;
    fB_4fccbb = ecx;
    if (fA_4fccbb == fB_4fccbb) goto L_4fccc7;
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    edx = (edx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    wr8(m, eax, (ebx & 0xffu));
    wr8(m, ecx, (edx & 0xffu));
L_4fccc7:
    eax = rd32(m, esp + 0x10u);
    ebx = (ebx & 0xffffff00u) | ((0x1u) & 0xffu);
    fA_4fcccd = rd8(m, eax + 0x38u);
    fB_4fcccd = (ebx & 0xffu);
    if (fA_4fcccd != fB_4fcccd) goto L_4fcdc9;
    ecx = rd32(m, ebp + 0x18u);
    fA_4fccd9 = edi;
    fB_4fccd9 = rd32(m, ecx + 0x4u);
    if (fA_4fccd9 == fB_4fccd9) goto L_4fcdc6;
L_4fcce2:
    fA_4fcce2 = rd8(m, edi + 0x38u);
    fB_4fcce2 = (ebx & 0xffu);
    if (fA_4fcce2 != fB_4fcce2) goto L_4fcdc6;
    ecx = rd32(m, esi);
    fA_4fcced = edi;
    fB_4fcced = ecx;
    if (fA_4fcced != fB_4fcced) goto L_4fcd54;
    ecx = rd32(m, esi + 0x8u);
    fA_4fccf4 = rd8(m, ecx + 0x38u);
    fB_4fccf4 = 0x0u;
    if (fA_4fccf4 != fB_4fccf4) goto L_4fcd0c;
    wr8(m, ecx + 0x38u, (ebx & 0xffu));
    esp -= 4; wr32(m, esp, ebp);
    ecx = esi;
    wr8(m, esi + 0x38u, 0x0u);
    LIFT_CALL(0x500480u, 0x4fcd09u);  // FUN_00500480
    ecx = rd32(m, esi + 0x8u);
L_4fcd0c:
    fA_4fcd0c = rd8(m, ecx + 0x39u);
    fB_4fcd0c = 0x0u;
    if (fA_4fcd0c != fB_4fcd0c) goto L_4fcd84;
    edx = rd32(m, ecx);
    fA_4fcd14 = rd8(m, edx + 0x38u);
    fB_4fcd14 = (ebx & 0xffu);
    if (fA_4fcd14 != fB_4fcd14) goto L_4fcd21;
    eax = rd32(m, ecx + 0x8u);
    fA_4fcd1c = rd8(m, eax + 0x38u);
    fB_4fcd1c = (ebx & 0xffu);
    if (fA_4fcd1c == fB_4fcd1c) goto L_4fcd80;
L_4fcd21:
    edx = rd32(m, ecx + 0x8u);
    fA_4fcd24 = rd8(m, edx + 0x38u);
    fB_4fcd24 = (ebx & 0xffu);
    if (fA_4fcd24 != fB_4fcd24) goto L_4fcd3b;
    eax = rd32(m, ecx);
    wr8(m, eax + 0x38u, (ebx & 0xffu));
    esp -= 4; wr32(m, esp, ebp);
    wr8(m, ecx + 0x38u, 0x0u);
    LIFT_CALL(0x4ffe80u, 0x4fcd38u);  // FUN_004ffe80
    ecx = rd32(m, esi + 0x8u);
L_4fcd3b:
    edx = (edx & 0xffffff00u) | ((rd8(m, esi + 0x38u)) & 0xffu);
    wr8(m, ecx + 0x38u, (edx & 0xffu));
    wr8(m, esi + 0x38u, (ebx & 0xffu));
    eax = rd32(m, ecx + 0x8u);
    esp -= 4; wr32(m, esp, ebp);
    ecx = esi;
    wr8(m, eax + 0x38u, (ebx & 0xffu));
    LIFT_CALL(0x500480u, 0x4fcd52u);  // FUN_00500480
    goto L_4fcdc6;
L_4fcd54:
    fA_4fcd54 = rd8(m, ecx + 0x38u);
    fB_4fcd54 = 0x0u;
    if (fA_4fcd54 != fB_4fcd54) goto L_4fcd6b;
    wr8(m, ecx + 0x38u, (ebx & 0xffu));
    esp -= 4; wr32(m, esp, ebp);
    ecx = esi;
    wr8(m, esi + 0x38u, 0x0u);
    LIFT_CALL(0x4ffe80u, 0x4fcd69u);  // FUN_004ffe80
    ecx = rd32(m, esi);
L_4fcd6b:
    fA_4fcd6b = rd8(m, ecx + 0x39u);
    fB_4fcd6b = 0x0u;
    if (fA_4fcd6b != fB_4fcd6b) goto L_4fcd84;
    edx = rd32(m, ecx + 0x8u);
    fA_4fcd74 = rd8(m, edx + 0x38u);
    fB_4fcd74 = (ebx & 0xffu);
    if (fA_4fcd74 != fB_4fcd74) goto L_4fcd97;
    eax = rd32(m, ecx);
    fA_4fcd7b = rd8(m, eax + 0x38u);
    fB_4fcd7b = (ebx & 0xffu);
    if (fA_4fcd7b != fB_4fcd7b) goto L_4fcd97;
L_4fcd80:
    wr8(m, ecx + 0x38u, 0x0u);
L_4fcd84:
    ecx = rd32(m, ebp + 0x18u);
    edi = esi;
    esi = rd32(m, esi + 0x4u);
    fA_4fcd8c = edi;
    fB_4fcd8c = rd32(m, ecx + 0x4u);
    if (fA_4fcd8c != fB_4fcd8c) goto L_4fcce2;
    goto L_4fcdc6;
L_4fcd97:
    edx = rd32(m, ecx);
    fA_4fcd99 = rd8(m, edx + 0x38u);
    fB_4fcd99 = (ebx & 0xffu);
    if (fA_4fcd99 != fB_4fcd99) goto L_4fcdb0;
    eax = rd32(m, ecx + 0x8u);
    wr8(m, eax + 0x38u, (ebx & 0xffu));
    esp -= 4; wr32(m, esp, ebp);
    wr8(m, ecx + 0x38u, 0x0u);
    LIFT_CALL(0x500480u, 0x4fcdaeu);  // FUN_00500480
    ecx = rd32(m, esi);
L_4fcdb0:
    edx = (edx & 0xffffff00u) | ((rd8(m, esi + 0x38u)) & 0xffu);
    wr8(m, ecx + 0x38u, (edx & 0xffu));
    wr8(m, esi + 0x38u, (ebx & 0xffu));
    eax = rd32(m, ecx);
    esp -= 4; wr32(m, esp, ebp);
    ecx = esi;
    wr8(m, eax + 0x38u, (ebx & 0xffu));
    LIFT_CALL(0x4ffe80u, 0x4fcdc6u);  // FUN_004ffe80
L_4fcdc6:
    wr8(m, edi + 0x38u, (ebx & 0xffu));
L_4fcdc9:
    edi = rd32(m, esp + 0x10u);
    esi = rd32(m, edi + 0x30u);
    edi = edi + 0x10u;
    ebp = 0u;
    fA_4fcdd5 = esi;
    fB_4fcdd5 = ebp;
    if (fA_4fcdd5 == fB_4fcdd5) goto L_4fcde1;
    LIFT_CALL(0x4ebe10u, 0x4fcddeu);  // FUN_004ebe10
    wr32(m, edi + 0x20u, ebp);
L_4fcde1:
    esi = edi;
    wr32(m, edi + 0x24u, ebp);
    LIFT_CALL(0x4f2220u, 0x4fcdebu);  // FUN_004f2220
    edi = rd32(m, edi);
    LIFT_CALL(0x4dc0f0u, 0x4fcdf2u);  // FUN_004dc0f0
    fA_4fcdf2 = rd8(m, 0xda67d8u);
    fB_4fcdf2 = 0x0u;
    ebx = rd32(m, 0x91600cu) /* 8.29485173491818e-39 */;
    if (fA_4fcdf2 == fB_4fcdf2) goto L_4fce08;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(ebx, 0x4fce08u);
L_4fce08:
    esi = rd32(m, 0x916350u) /* 8.287536956934404e-39 */;
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(esi, 0x4fce11u);
    edi = rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */;
    esp = esp + 0x4u;
    fA_4fce1a = rd8(m, 0xda67d8u);
    fB_4fce1a = 0x0u;
    if (fA_4fce1a == fB_4fce1a) goto L_4fce2a;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(edi, 0x4fce2au);
L_4fce2a:
    LIFT_CALL(0x4dc0f0u, 0x4fce2fu);  // FUN_004dc0f0
    fA_4fce2f = rd8(m, 0xda67d8u);
    fB_4fce2f = 0x0u;
    if (fA_4fce2f == fB_4fce2f) goto L_4fce3f;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(ebx, 0x4fce3fu);
L_4fce3f:
    ecx = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(esi, 0x4fce46u);
    esp = esp + 0x4u;
    fA_4fce49 = rd8(m, 0xda67d8u);
    fB_4fce49 = 0x0u;
    if (fA_4fce49 == fB_4fce49) goto L_4fce59;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(edi, 0x4fce59u);
L_4fce59:
    ecx = rd32(m, esp + 0x60u);
    eax = rd32(m, ecx + 0x1cu);
    fA_4fce60 = eax;
    fB_4fce60 = ebp;
    if (fA_4fce60 <= fB_4fce60) goto L_4fce68;
    eax = (eax - 1u);
    wr32(m, ecx + 0x1cu, eax);
L_4fce68:
    edx = rd32(m, esp + 0x64u);
    eax = rd32(m, esp + 0x6cu);
    ecx = rd32(m, ecx);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    wr32(m, edx + 0x4u, eax);
    wr32(m, edx, ecx);
    eax = edx;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x4cu;
    LIFT_RET(16);
}

// 004fea30 Fn_004fea30
//   original FUN_004fea30; register inputs: eax; returns with ret 16
REFLEX_PORT_FN(0x004fea30, Fn_004fea30, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_4fea47 = 0;
    uint32_t fA_4fea53 = 0;
    uint32_t fB_4fea53 = 0;
    uint32_t fR_4fea63 = 0;
    uint32_t fA_4fea67 = 0;
    uint32_t fB_4fea67 = 0;
    uint32_t fA_4fea92 = 0;
    uint32_t fB_4fea92 = 0;
    esp = esp - 0xcu;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x18u);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    eax = rd32(m, esi + 0x10u);
    ebx = rd32(m, esi + 0xcu);
    ecx = eax;
    ecx = ecx - ebx;
    esp -= 4; wr32(m, esp, edi);
    fR_4fea47 = (ecx & 0xfffffffcu);
    if (!(fR_4fea47 == 0)) goto L_4fea53;
    edi = 0u;
    goto L_4fea7a;
L_4fea53:
    fA_4fea53 = ebx;
    fB_4fea53 = eax;
    if (fA_4fea53 <= fB_4fea53) goto L_4fea5d;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4fea5du);
L_4fea5d:
    ecx = rd32(m, esp + 0x28u);
    eax = rd32(m, esi);
    fR_4fea63 = (ecx & ecx);
    if ((fR_4fea63 == 0)) goto L_4fea6b;
    fA_4fea67 = ecx;
    fB_4fea67 = eax;
    if (fA_4fea67 == fB_4fea67) goto L_4fea71;
L_4fea6b:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4fea71u);
L_4fea71:
    edi = rd32(m, esp + 0x2cu);
    edi = edi - ebx;
    edi = uint32_t(int32_t(edi) >> 2);
L_4fea7a:
    edx = rd32(m, esp + 0x2cu);
    eax = rd32(m, esp + 0x28u);
    ecx = rd32(m, esp + 0x24u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x501a30u, 0x4fea8fu);  // FUN_00501a30
    ebx = rd32(m, esi + 0xcu);
    fA_4fea92 = ebx;
    fB_4fea92 = rd32(m, esi + 0x10u);
    if (fA_4fea92 <= fB_4fea92) goto L_4fea9d;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x4fea9du);
L_4fea9d:
    esi = rd32(m, esi);
    wr32(m, esp + 0x10u, esi);
    esp -= 4; wr32(m, esp, edi);
    esi = (esp + 0x14u);
    wr32(m, esp + 0x18u, ebx);
    LIFT_CALL(0x842fa0u, 0x4feab1u);  // FUN_00842fa0
    eax = rd32(m, esp + 0x14u);
    edx = rd32(m, esp + 0x10u);
    edi = rd32(m, esp); esp += 4;
    wr32(m, ebp + 0x4u, eax);
    esi = rd32(m, esp); esp += 4;
    wr32(m, ebp, edx);
    eax = ebp;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0xcu;
    LIFT_RET(16);
}

// 004fead0 Fn_004fead0
//   original FUN_004fead0; register inputs: none; returns with ret 4
REFLEX_PORT_FN(0x004fead0, Fn_004fead0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4fead5 = 0;
    uint32_t fB_4fead5 = 0;
    uint32_t fA_4feb01 = 0;
    uint32_t fB_4feb01 = 0;
    LIFT_CALL(0x4dc0f0u, 0x4fead5u);  // FUN_004dc0f0
    fA_4fead5 = rd8(m, 0xda67d8u);
    fB_4fead5 = 0x0u;
    if (fA_4fead5 == fB_4fead5) goto L_4feae9;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x4feae9u);
L_4feae9:
    eax = rd32(m, esp + 0x4u);
    esp -= 4; wr32(m, esp, esi);
    ecx = (eax * 4);
    esp -= 4; wr32(m, esp, 0x10u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(rd32(m, 0x916358u) /* 8.287573390694476e-39 */, 0x4feafeu);
    esp = esp + 0x8u;
    fA_4feb01 = rd8(m, 0xda67d8u);
    fB_4feb01 = 0x0u;
    esi = eax;
    if (fA_4feb01 == fB_4feb01) goto L_4feb17;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x4feb17u);
L_4feb17:
    eax = esi;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 004ffcb0 Fn_004ffcb0
//   original FUN_004ffcb0; register inputs: none; returns with ret 20
REFLEX_PORT_FN(0x004ffcb0, Fn_004ffcb0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4ffcb8 = 0;
    uint32_t fB_4ffcb8 = 0;
    uint32_t fR_4ffcf4 = 0;
    uint32_t fA_4ffcf8 = 0;
    uint32_t fB_4ffcf8 = 0;
    uint32_t fA_4ffd06 = 0;
    uint32_t fB_4ffd06 = 0;
    uint32_t fA_4ffd10 = 0;
    uint32_t fB_4ffd10 = 0;
    uint32_t fR_4ffd35 = 0;
    uint32_t fA_4ffd39 = 0;
    uint32_t fB_4ffd39 = 0;
    uint32_t fA_4ffd47 = 0;
    uint32_t fB_4ffd47 = 0;
    uint32_t fA_4ffd58 = 0;
    uint32_t fB_4ffd58 = 0;
    uint32_t fA_4ffd7f = 0;
    uint32_t fB_4ffd7f = 0;
    uint32_t FL = 0;
    uint32_t fA_4ffd9b = 0;
    uint32_t fB_4ffd9b = 0;
    uint32_t fA_4ffda3 = 0;
    uint32_t fB_4ffda3 = 0;
    uint32_t fA_4ffdcc = 0;
    uint32_t fB_4ffdcc = 0;
    uint32_t fR_4ffe04 = 0;
    uint32_t fA_4ffe0e = 0;
    uint32_t fB_4ffe0e = 0;
    uint32_t fA_4ffe1a = 0;
    uint32_t fB_4ffe1a = 0;
    esp = esp - 0x18u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x20u);
    fA_4ffcb8 = rd32(m, ebx + 0x1cu);
    fB_4ffcb8 = 0x0u;
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x28u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    if (fA_4ffcb8 != fB_4ffcb8) goto L_4ffce3;
    eax = rd32(m, esp + 0x34u);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, ebx + 0x18u);
    esp -= 4; wr32(m, esp, 0x1u);
    esp -= 4; wr32(m, esp, ebp);
    edi = ebx;
    LIFT_CALL(0x502e30u, 0x4ffcd7u);  // FUN_00502e30
    eax = ebp;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(20);
L_4ffce3:
    ecx = rd32(m, ebx + 0x18u);
    esi = rd32(m, ecx);
    ecx = rd32(m, esp + 0x38u);
    eax = rd32(m, ebx);
    edi = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    fR_4ffcf4 = (ecx & ecx);
    if ((fR_4ffcf4 == 0)) goto L_4ffcfc;
    fA_4ffcf8 = ecx;
    fB_4ffcf8 = eax;
    if (fA_4ffcf8 == fB_4ffcf8) goto L_4ffd02;
L_4ffcfc:
    LIFT_CALL(edi, 0x4ffcfeu);
    ecx = rd32(m, esp + 0x38u);
L_4ffd02:
    eax = rd32(m, esp + 0x3cu);
    fA_4ffd06 = eax;
    fB_4ffd06 = esi;
    if (fA_4ffd06 != fB_4ffd06) goto L_4ffd30;
    ecx = rd32(m, esp + 0x34u);
    edx = rd32(m, ecx);
    fA_4ffd10 = edx;
    fB_4ffd10 = rd32(m, eax + 0xcu);
    if (int32_t(sext(fA_4ffd10, 4)) >= int32_t(sext(fB_4ffd10, 4))) goto L_4ffe5a;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, 0x1u);
    esp -= 4; wr32(m, esp, ebp);
    edi = ebx;
    LIFT_CALL(0x502e30u, 0x4ffd24u);  // FUN_00502e30
    eax = ebp;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(20);
L_4ffd30:
    esi = rd32(m, ebx + 0x18u);
    edx = rd32(m, ebx);
    fR_4ffd35 = (ecx & ecx);
    if ((fR_4ffd35 == 0)) goto L_4ffd3d;
    fA_4ffd39 = ecx;
    fB_4ffd39 = edx;
    if (fA_4ffd39 == fB_4ffd39) goto L_4ffd47;
L_4ffd3d:
    LIFT_CALL(edi, 0x4ffd3fu);
    eax = rd32(m, esp + 0x3cu);
    ecx = rd32(m, esp + 0x38u);
L_4ffd47:
    fA_4ffd47 = eax;
    fB_4ffd47 = esi;
    if (fA_4ffd47 != fB_4ffd47) goto L_4ffd79;
    eax = rd32(m, ebx + 0x18u);
    eax = rd32(m, eax + 0x8u);
    ecx = rd32(m, eax + 0xcu);
    edx = rd32(m, esp + 0x34u);
    fA_4ffd58 = ecx;
    fB_4ffd58 = rd32(m, edx);
    if (int32_t(sext(fA_4ffd58, 4)) >= int32_t(sext(fB_4ffd58, 4))) goto L_4ffe56;
    ecx = edx;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, ebp);
    edi = ebx;
    LIFT_CALL(0x502e30u, 0x4ffd6du);  // FUN_00502e30
    eax = ebp;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(20);
L_4ffd79:
    edi = rd32(m, esp + 0x34u);
    edx = rd32(m, edi);
    fA_4ffd7f = rd32(m, eax + 0xcu);
    fB_4ffd7f = edx;
    FL = fl_sub(fA_4ffd7f, fB_4ffd7f, 4);
    if (int32_t(sext(fA_4ffd7f, 4)) <= int32_t(sext(fB_4ffd7f, 4))) goto L_4ffdcf;
    esi = (esp + 0x14u);
    wr32(m, esp + 0x14u, ecx);
    wr32(m, esp + 0x18u, eax);
    LIFT_CALL(0x505280u, 0x4ffd95u);  // FUN_00505280
    edx = rd32(m, edi);
    eax = rd32(m, esp + 0x18u);
    fA_4ffd9b = rd32(m, eax + 0xcu);
    fB_4ffd9b = edx;
    if (int32_t(sext(fA_4ffd9b, 4)) >= int32_t(sext(fB_4ffd9b, 4))) goto L_4ffdc4;
    edx = rd32(m, eax + 0x8u);
    fA_4ffda3 = rd8(m, edx + 0x39u);
    fB_4ffda3 = 0x0u;
    esp -= 4; wr32(m, esp, edi);
    edi = ebx;
    if (fA_4ffda3 != fB_4ffda3) goto L_4ffe27;
    eax = rd32(m, esp + 0x40u);
    esp -= 4; wr32(m, esp, 0x1u);
    esp -= 4; wr32(m, esp, ebp);
    LIFT_CALL(0x502e30u, 0x4ffdb8u);  // FUN_00502e30
    eax = ebp;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(20);
L_4ffdc4:
    eax = rd32(m, esp + 0x3cu);
    ecx = rd32(m, esp + 0x38u);
    fA_4ffdcc = rd32(m, eax + 0xcu);
    fB_4ffdcc = edx;
    FL = fl_sub(fA_4ffdcc, fB_4ffdcc, 4);
L_4ffdcf:
    if (fl_cc(13, FL)) goto L_4ffe56;
    wr32(m, esp + 0x14u, ecx);
    ecx = rd32(m, ebx);
    wr32(m, esp + 0x18u, eax);
    eax = rd32(m, ebx + 0x18u);
    esi = (esp + 0x14u);
    wr32(m, esp + 0x20u, eax);
    wr32(m, esp + 0x1cu, ecx);
    LIFT_CALL(0x503440u, 0x4ffdf3u);  // FUN_00503440
    esi = (esp + 0x1cu);
    edi = (esp + 0x14u);
    LIFT_CALL(0x4fd0c0u, 0x4ffe00u);  // FUN_004fd0c0
    ecx = rd32(m, esp + 0x18u);
    fR_4ffe04 = ((eax & 0xffu) & (eax & 0xffu));
    if (!(fR_4ffe04 == 0)) goto L_4ffe13;
    edx = rd32(m, esp + 0x34u);
    eax = rd32(m, edx);
    fA_4ffe0e = eax;
    fB_4ffe0e = rd32(m, ecx + 0xcu);
    if (int32_t(sext(fA_4ffe0e, 4)) >= int32_t(sext(fB_4ffe0e, 4))) goto L_4ffe56;
L_4ffe13:
    eax = rd32(m, esp + 0x3cu);
    edx = rd32(m, eax + 0x8u);
    fA_4ffe1a = rd8(m, edx + 0x39u);
    fB_4ffe1a = 0x0u;
    edi = ebx;
    if (fA_4ffe1a == fB_4ffe1a) goto L_4ffe3b;
    ecx = rd32(m, esp + 0x34u);
    esp -= 4; wr32(m, esp, ecx);
L_4ffe27:
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, ebp);
    LIFT_CALL(0x502e30u, 0x4ffe2fu);  // FUN_00502e30
    eax = ebp;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(20);
L_4ffe3b:
    edx = rd32(m, esp + 0x34u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, 0x1u);
    esp -= 4; wr32(m, esp, ebp);
    eax = ecx;
    LIFT_CALL(0x502e30u, 0x4ffe4au);  // FUN_00502e30
    eax = ebp;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(20);
L_4ffe56:
    ecx = rd32(m, esp + 0x34u);
L_4ffe5a:
    esp -= 4; wr32(m, esp, ecx);
    eax = (esp + 0x20u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x502d30u, 0x4ffe66u);  // FUN_00502d30
    ecx = rd32(m, eax);
    edi = rd32(m, esp); esp += 4;
    wr32(m, ebp, ecx);
    edx = rd32(m, eax + 0x4u);
    esi = rd32(m, esp); esp += 4;
    wr32(m, ebp + 0x4u, edx);
    eax = ebp;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(20);
}

// 004fff20 Fn_004fff20
//   original FUN_004fff20; register inputs: none; returns with ret 20
REFLEX_PORT_FN(0x004fff20, Fn_004fff20, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_4fff28 = 0;
    uint32_t fB_4fff28 = 0;
    uint32_t fR_4fff64 = 0;
    uint32_t fA_4fff68 = 0;
    uint32_t fB_4fff68 = 0;
    uint32_t fA_4fff76 = 0;
    uint32_t fB_4fff76 = 0;
    uint32_t fA_4fff80 = 0;
    uint32_t fB_4fff80 = 0;
    uint32_t fR_4fffa5 = 0;
    uint32_t fA_4fffa9 = 0;
    uint32_t fB_4fffa9 = 0;
    uint32_t fA_4fffb7 = 0;
    uint32_t fB_4fffb7 = 0;
    uint32_t fA_4fffc8 = 0;
    uint32_t fB_4fffc8 = 0;
    uint32_t fA_4fffef = 0;
    uint32_t fB_4fffef = 0;
    uint32_t FL = 0;
    uint32_t fA_50000b = 0;
    uint32_t fB_50000b = 0;
    uint32_t fA_500013 = 0;
    uint32_t fB_500013 = 0;
    uint32_t fA_50003c = 0;
    uint32_t fB_50003c = 0;
    uint32_t fR_500074 = 0;
    uint32_t fA_50007e = 0;
    uint32_t fB_50007e = 0;
    uint32_t fA_50008a = 0;
    uint32_t fB_50008a = 0;
    esp = esp - 0x18u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x20u);
    fA_4fff28 = rd32(m, ebx + 0x1cu);
    fB_4fff28 = 0x0u;
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x28u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    if (fA_4fff28 != fB_4fff28) goto L_4fff53;
    eax = rd32(m, esp + 0x34u);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, ebx + 0x18u);
    esp -= 4; wr32(m, esp, 0x1u);
    esp -= 4; wr32(m, esp, ebp);
    edi = ebx;
    LIFT_CALL(0x5035c0u, 0x4fff47u);  // FUN_005035c0
    eax = ebp;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(20);
L_4fff53:
    ecx = rd32(m, ebx + 0x18u);
    esi = rd32(m, ecx);
    ecx = rd32(m, esp + 0x38u);
    eax = rd32(m, ebx);
    edi = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    fR_4fff64 = (ecx & ecx);
    if ((fR_4fff64 == 0)) goto L_4fff6c;
    fA_4fff68 = ecx;
    fB_4fff68 = eax;
    if (fA_4fff68 == fB_4fff68) goto L_4fff72;
L_4fff6c:
    LIFT_CALL(edi, 0x4fff6eu);
    ecx = rd32(m, esp + 0x38u);
L_4fff72:
    eax = rd32(m, esp + 0x3cu);
    fA_4fff76 = eax;
    fB_4fff76 = esi;
    if (fA_4fff76 != fB_4fff76) goto L_4fffa0;
    ecx = rd32(m, esp + 0x34u);
    edx = rd32(m, ecx);
    fA_4fff80 = edx;
    fB_4fff80 = rd32(m, eax + 0xcu);
    if (int32_t(sext(fA_4fff80, 4)) >= int32_t(sext(fB_4fff80, 4))) goto L_5000ca;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, 0x1u);
    esp -= 4; wr32(m, esp, ebp);
    edi = ebx;
    LIFT_CALL(0x5035c0u, 0x4fff94u);  // FUN_005035c0
    eax = ebp;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(20);
L_4fffa0:
    esi = rd32(m, ebx + 0x18u);
    edx = rd32(m, ebx);
    fR_4fffa5 = (ecx & ecx);
    if ((fR_4fffa5 == 0)) goto L_4fffad;
    fA_4fffa9 = ecx;
    fB_4fffa9 = edx;
    if (fA_4fffa9 == fB_4fffa9) goto L_4fffb7;
L_4fffad:
    LIFT_CALL(edi, 0x4fffafu);
    eax = rd32(m, esp + 0x3cu);
    ecx = rd32(m, esp + 0x38u);
L_4fffb7:
    fA_4fffb7 = eax;
    fB_4fffb7 = esi;
    if (fA_4fffb7 != fB_4fffb7) goto L_4fffe9;
    eax = rd32(m, ebx + 0x18u);
    eax = rd32(m, eax + 0x8u);
    ecx = rd32(m, eax + 0xcu);
    edx = rd32(m, esp + 0x34u);
    fA_4fffc8 = ecx;
    fB_4fffc8 = rd32(m, edx);
    if (int32_t(sext(fA_4fffc8, 4)) >= int32_t(sext(fB_4fffc8, 4))) goto L_5000c6;
    ecx = edx;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, ebp);
    edi = ebx;
    LIFT_CALL(0x5035c0u, 0x4fffddu);  // FUN_005035c0
    eax = ebp;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(20);
L_4fffe9:
    edi = rd32(m, esp + 0x34u);
    edx = rd32(m, edi);
    fA_4fffef = rd32(m, eax + 0xcu);
    fB_4fffef = edx;
    FL = fl_sub(fA_4fffef, fB_4fffef, 4);
    if (int32_t(sext(fA_4fffef, 4)) <= int32_t(sext(fB_4fffef, 4))) goto L_50003f;
    esi = (esp + 0x14u);
    wr32(m, esp + 0x14u, ecx);
    wr32(m, esp + 0x18u, eax);
    LIFT_CALL(0x505450u, 0x500005u);  // FUN_00505450
    edx = rd32(m, edi);
    eax = rd32(m, esp + 0x18u);
    fA_50000b = rd32(m, eax + 0xcu);
    fB_50000b = edx;
    if (int32_t(sext(fA_50000b, 4)) >= int32_t(sext(fB_50000b, 4))) goto L_500034;
    edx = rd32(m, eax + 0x8u);
    fA_500013 = rd8(m, edx + 0x1du);
    fB_500013 = 0x0u;
    esp -= 4; wr32(m, esp, edi);
    edi = ebx;
    if (fA_500013 != fB_500013) goto L_500097;
    eax = rd32(m, esp + 0x40u);
    esp -= 4; wr32(m, esp, 0x1u);
    esp -= 4; wr32(m, esp, ebp);
    LIFT_CALL(0x5035c0u, 0x500028u);  // FUN_005035c0
    eax = ebp;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(20);
L_500034:
    eax = rd32(m, esp + 0x3cu);
    ecx = rd32(m, esp + 0x38u);
    fA_50003c = rd32(m, eax + 0xcu);
    fB_50003c = edx;
    FL = fl_sub(fA_50003c, fB_50003c, 4);
L_50003f:
    if (fl_cc(13, FL)) goto L_5000c6;
    wr32(m, esp + 0x14u, ecx);
    ecx = rd32(m, ebx);
    wr32(m, esp + 0x18u, eax);
    eax = rd32(m, ebx + 0x18u);
    esi = (esp + 0x14u);
    wr32(m, esp + 0x20u, eax);
    wr32(m, esp + 0x1cu, ecx);
    LIFT_CALL(0x503750u, 0x500063u);  // FUN_00503750
    esi = (esp + 0x1cu);
    edi = (esp + 0x14u);
    LIFT_CALL(0x4fd0c0u, 0x500070u);  // FUN_004fd0c0
    ecx = rd32(m, esp + 0x18u);
    fR_500074 = ((eax & 0xffu) & (eax & 0xffu));
    if (!(fR_500074 == 0)) goto L_500083;
    edx = rd32(m, esp + 0x34u);
    eax = rd32(m, edx);
    fA_50007e = eax;
    fB_50007e = rd32(m, ecx + 0xcu);
    if (int32_t(sext(fA_50007e, 4)) >= int32_t(sext(fB_50007e, 4))) goto L_5000c6;
L_500083:
    eax = rd32(m, esp + 0x3cu);
    edx = rd32(m, eax + 0x8u);
    fA_50008a = rd8(m, edx + 0x1du);
    fB_50008a = 0x0u;
    edi = ebx;
    if (fA_50008a == fB_50008a) goto L_5000ab;
    ecx = rd32(m, esp + 0x34u);
    esp -= 4; wr32(m, esp, ecx);
L_500097:
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, ebp);
    LIFT_CALL(0x5035c0u, 0x50009fu);  // FUN_005035c0
    eax = ebp;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(20);
L_5000ab:
    edx = rd32(m, esp + 0x34u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, 0x1u);
    esp -= 4; wr32(m, esp, ebp);
    eax = ecx;
    LIFT_CALL(0x5035c0u, 0x5000bau);  // FUN_005035c0
    eax = ebp;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(20);
L_5000c6:
    ecx = rd32(m, esp + 0x34u);
L_5000ca:
    esp -= 4; wr32(m, esp, ecx);
    eax = (esp + 0x20u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x502fc0u, 0x5000d6u);  // FUN_00502fc0
    ecx = rd32(m, eax);
    edi = rd32(m, esp); esp += 4;
    wr32(m, ebp, ecx);
    edx = rd32(m, eax + 0x4u);
    esi = rd32(m, esp); esp += 4;
    wr32(m, ebp + 0x4u, edx);
    eax = ebp;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x18u;
    LIFT_RET(20);
}

// 00662dd0 ForestSystem__vf19
//   original ForestSystem::vf19; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x00662dd0, ForestSystem__vf19, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_662ddc = 0;
    uint32_t fB_662ddc = 0;
    uint32_t fA_662dee = 0;
    uint32_t fB_662dee = 0;
    uint32_t fA_662e03 = 0;
    uint32_t fB_662e03 = 0;
    uint32_t fA_662e10 = 0;
    uint32_t fB_662e10 = 0;
    uint32_t fR_662e27 = 0;
    uint32_t fA_662e2d = 0;
    uint32_t fB_662e2d = 0;
    uint32_t fA_662e3c = 0;
    uint32_t fB_662e3c = 0;
    uint32_t fR_662e53 = 0;
    uint32_t fA_662e59 = 0;
    uint32_t fB_662e59 = 0;
    uint32_t fA_662e68 = 0;
    uint32_t fB_662e68 = 0;
    uint32_t fR_662e7f = 0;
    uint32_t fA_662e85 = 0;
    uint32_t fB_662e85 = 0;
    uint32_t fA_662e94 = 0;
    uint32_t fB_662e94 = 0;
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    eax = rd32(m, esi + 0x10u);
    ecx = rd32(m, 0xa95844u);
    fA_662ddc = eax;
    fB_662ddc = rd32(m, ecx + 0x20u);
    if (fA_662ddc != fB_662ddc) goto L_662dee;
    eax = rd32(m, 0xa946b8u);
    eax = rd32(m, eax + 0x5a54u);
    goto L_662e03;
L_662dee:
    fA_662dee = eax;
    fB_662dee = rd32(m, ecx + 0x1cu);
    if (fA_662dee != fB_662dee) goto L_662e01;
    ecx = rd32(m, 0xa946b8u);
    eax = rd32(m, ecx + 0x20c8u);
    goto L_662e03;
L_662e01:
    eax = 0u;
L_662e03:
    fA_662e03 = rd32(m, eax + 0x20014u);
    fB_662e03 = 0x0u;
    if (fA_662e03 != fB_662e03) goto L_662eb7;
    fA_662e10 = rd8(m, 0x9b716du);
    fB_662e10 = 0x0u;
    if (fA_662e10 != fB_662e10) goto L_662e94;
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0x7u);
    ecx = 0xd60688u;
    LIFT_CALL(0x811420u, 0x662e27u);  // KeyMap::vf1
    fR_662e27 = (eax & eax);
    if ((fR_662e27 == 0)) goto L_662e3c;
    edx = 0u;
    fA_662e2d = rd32(m, esi + 0x31c4u);
    fB_662e2d = edx;
    edx = (edx & 0xffffff00u) | (((fA_662e2d == fB_662e2d) ? 1u : 0u) & 0xffu);
    wr32(m, esi + 0x31c4u, edx);
L_662e3c:
    fA_662e3c = rd8(m, 0x9b716du);
    fB_662e3c = 0x0u;
    if (fA_662e3c != fB_662e3c) goto L_662e94;
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0x7u);
    ecx = 0xd5db20u;
    LIFT_CALL(0x811420u, 0x662e53u);  // KeyMap::vf1
    fR_662e53 = (eax & eax);
    if ((fR_662e53 == 0)) goto L_662e68;
    eax = 0u;
    fA_662e59 = rd32(m, esi + 0x31c8u);
    fB_662e59 = eax;
    eax = (eax & 0xffffff00u) | (((fA_662e59 == fB_662e59) ? 1u : 0u) & 0xffu);
    wr32(m, esi + 0x31c8u, eax);
L_662e68:
    fA_662e68 = rd8(m, 0x9b716du);
    fB_662e68 = 0x0u;
    if (fA_662e68 != fB_662e68) goto L_662e94;
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0x7u);
    ecx = 0xd5fc08u;
    LIFT_CALL(0x811420u, 0x662e7fu);  // KeyMap::vf1
    fR_662e7f = (eax & eax);
    if ((fR_662e7f == 0)) goto L_662e94;
    ecx = 0u;
    fA_662e85 = rd32(m, esi + 0x31ccu);
    fB_662e85 = ecx;
    ecx = (ecx & 0xffffff00u) | (((fA_662e85 == fB_662e85) ? 1u : 0u) & 0xffu);
    wr32(m, esi + 0x31ccu, ecx);
L_662e94:
    fA_662e94 = rd32(m, esi + 0x31ccu);
    fB_662e94 = 0x0u;
    if (fA_662e94 == fB_662e94) goto L_662eb7;
    eax = rd32(m, esi + 0x9cu);
    wr32(m, eax + 0x4cu, 0x1u);
    edx = rd32(m, esi + 0x9cu);
    wr32(m, edx + 0x4cu, 0x0u);
L_662eb7:
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 006e0ab0 NetSignInManager__vf2
//   original NetSignInManager::vf2; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x006e0ab0, NetSignInManager__vf2, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_6e0ab0 = 0;
    uint32_t fB_6e0ab0 = 0;
    uint32_t fA_6e0ad5 = 0;
    uint32_t fB_6e0ad5 = 0;
    fA_6e0ab0 = rd32(m, ecx + 0x10cu);
    fB_6e0ab0 = 0x0u;
    if (fA_6e0ab0 == fB_6e0ab0) goto L_6e0ad5;
    eax = rd32(m, ecx + 0x10cu);
    edx = rd32(m, ecx + eax * 4 + 0x104u);
    wr32(m, ecx + 0x108u, edx);
    wr32(m, ecx + 0x10cu, (rd32(m, ecx + 0x10cu) - 1u));
    LIFT_RET(4);
L_6e0ad5:
    fA_6e0ad5 = rd32(m, ecx + 0x114u);
    fB_6e0ad5 = 0x0u;
    if (fA_6e0ad5 == fB_6e0ad5) goto L_6e0af7;
    eax = rd32(m, ecx + 0x114u);
    edx = rd32(m, ecx + eax * 4 + 0x10cu);
    wr32(m, ecx + 0x110u, edx);
    wr32(m, ecx + 0x114u, (rd32(m, ecx + 0x114u) - 1u));
L_6e0af7:
    LIFT_RET(4);
}

// 006e0c50 Fn_006e0c50
//   original FUN_006e0c50; register inputs: eax; returns with ret 0
REFLEX_PORT_FN(0x006e0c50, Fn_006e0c50, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_6e0c50 = 0;
    uint32_t fB_6e0c50 = 0;
    uint32_t fA_6e0c56 = 0;
    uint32_t fB_6e0c56 = 0;
    uint32_t fA_6e0c5c = 0;
    uint32_t fB_6e0c5c = 0;
    uint32_t fA_6e0c72 = 0;
    uint32_t fB_6e0c72 = 0;
    fA_6e0c50 = rd32(m, eax + 0x58u);
    fB_6e0c50 = 0x0u;
    if (fA_6e0c50 != fB_6e0c50) goto L_6e0c7b;
    fA_6e0c56 = rd32(m, eax + 0x5cu);
    fB_6e0c56 = 0x0u;
    if (fA_6e0c56 != fB_6e0c56) goto L_6e0c7b;
    fA_6e0c5c = rd32(m, 0xa9596cu);
    fB_6e0c5c = 0x0u;
    if (fA_6e0c5c == fB_6e0c5c) goto L_6e0c7b;
    ecx = rd32(m, 0xa95960u);
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x10u);
    LIFT_CALL(edx, 0x6e0c72u);
    fA_6e0c72 = rd8(m, eax + 0x34u);
    fB_6e0c72 = 0x0u;
    if (fA_6e0c72 != fB_6e0c72) goto L_6e0c7b;
    eax = 0u;
    LIFT_RET(0);
L_6e0c7b:
    eax = 0x1u;
    LIFT_RET(0);
}

// 006e0f30 PCSignInManager__vf2
//   original PCSignInManager::vf2; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x006e0f30, PCSignInManager__vf2, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_6e0f41 = 0;
    uint32_t fB_6e0f41 = 0;
    uint32_t fR_6e0f57 = 0;
    uint32_t fR_6e0fbb = 0;
    uint32_t fR_6e0fe1 = 0;
    uint32_t fA_6e0ff4 = 0;
    uint32_t fB_6e0ff4 = 0;
    uint32_t fA_6e1022 = 0;
    uint32_t fB_6e1022 = 0;
    uint32_t fR_6e1047 = 0;
    uint32_t fR_6e1056 = 0;
    uint32_t fA_6e107b = 0;
    uint32_t fB_6e107b = 0;
    uint32_t fA_6e1084 = 0;
    uint32_t fB_6e1084 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0xcu;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = ecx;
    eax = rd32(m, edi + 0x34u);
    fA_6e0f41 = eax;
    fB_6e0f41 = 0x3u;
    if (fA_6e0f41 > fB_6e0f41) goto L_6e10b1;
    switch (eax) { case 0: goto L_6e0f51; case 1: goto L_6e103f; case 2: goto L_6e107b; case 3: goto L_6e107b; default: throw std::runtime_error("jump table"); }
L_6e0f51:
    LIFT_CALL(rd32(m, 0x9166dcu) /* 8.279883064722262e-39 */, 0x6e0f57u);
    fR_6e0f57 = (eax & eax);
    if ((fR_6e0f57 == 0)) goto L_6e10b1;
    ebx = rd32(m, 0x9166c8u) /* 8.279829815380617e-39 */;
    LIFT_CALL(ebx, 0x6e0f67u);
    edx = rd32(m, eax);
    esp -= 4; wr32(m, esp, 0x11u);
    esi = (edi + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    ecx = eax;
    eax = rd32(m, edx);
    esp -= 4; wr32(m, esp, 0xffffffffu);
    LIFT_CALL(eax, 0x6e0f77u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0x0u);
    LIFT_CALL(rd32(m, 0x9160a4u) /* 8.295510345196412e-39 */, 0x6e0f82u);
    LIFT_CALL(rd32(m, 0x9166dcu) /* 8.279883064722262e-39 */, 0x6e0f88u);
    edx = rd32(m, eax);
    edx = rd32(m, edx + 0x8u);
    ecx = (esp + 0x10u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = eax;
    LIFT_CALL(edx, 0x6e0f96u);
    ecx = rd32(m, eax);
    wr32(m, edi + 0x38u, ecx);
    edx = rd32(m, eax + 0x4u);
    wr32(m, edi + 0x3cu, edx);
    LIFT_CALL(ebx, 0x6e0fa3u);
    edx = rd32(m, eax);
    ebx = rd32(m, 0xd92ee0u);
    ecx = eax;
    eax = rd32(m, edx);
    LIFT_CALL(eax, 0x6e0fb1u);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, ebx);
    ecx = eax;
    LIFT_CALL(0x616f70u, 0x6e0fbbu);  // App_IsProfileNameValid
    fR_6e0fbb = (eax & eax);
    if (!(fR_6e0fbb == 0)) goto L_6e0fd0;
    esp -= 4; wr32(m, esp, 0x962998u);
    esp -= 4; wr32(m, esp, 0x11u);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(rd32(m, 0x916278u) /* 8.286712993437381e-39 */, 0x6e0fcdu);
    esp = esp + 0xcu;
L_6e0fd0:
    LIFT_CALL(rd32(m, 0x9166dcu) /* 8.279883064722262e-39 */, 0x6e0fd6u);
    edx = rd32(m, eax);
    ecx = eax;
    eax = rd32(m, edx + 0x4u);
    LIFT_CALL(eax, 0x6e0fdfu);
    ecx = 0u;
    fR_6e0fe1 = ((eax & 0xffu) & (eax & 0xffu));
    ecx = (ecx & 0xffffff00u) | (((!(fR_6e0fe1 == 0)) ? 1u : 0u) & 0xffu);
    ecx = (ecx + 1u);
    eax = ecx;
    ecx = 0x2u;
    wr32(m, edi + 0x120u, eax);
    fA_6e0ff4 = eax;
    fB_6e0ff4 = ecx;
    if (fA_6e0ff4 != fB_6e0ff4) goto L_6e101d;
    wr32(m, edi + 0x34u, ecx);
    ecx = (edi + 0x108u);
    esi = 0u;
    LIFT_CALL(0x702f40u, 0x6e1008u);  // FUN_00702f40
    LIFT_CALL(0x6de1c0u, 0x6e100du);  // FUN_006de1c0
    edx = rd32(m, eax);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, 0x3u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x707530u, 0x6e1018u);  // FUN_00707530
    goto L_6e10b1;
L_6e101d:
    esi = 0x1u;
    fA_6e1022 = eax;
    fB_6e1022 = esi;
    if (fA_6e1022 != fB_6e1022) goto L_6e10b1;
    wr32(m, edi + 0x34u, esi);
    LIFT_CALL(0x6de1c0u, 0x6e1032u);  // FUN_006de1c0
    eax = rd32(m, eax);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x707530u, 0x6e103du);  // FUN_00707530
    goto L_6e10b1;
L_6e103f:
    esi = rd32(m, 0x9166dcu) /* 8.279883064722262e-39 */;
    LIFT_CALL(esi, 0x6e1047u);
    fR_6e1047 = (eax & eax);
    if ((fR_6e1047 == 0)) goto L_6e10b1;
    LIFT_CALL(esi, 0x6e104du);
    edx = rd32(m, eax);
    ecx = eax;
    eax = rd32(m, edx + 0x4u);
    LIFT_CALL(eax, 0x6e1056u);
    fR_6e1056 = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_6e1056 == 0)) goto L_6e10b1;
    ecx = 0x2u;
    wr32(m, edi + 0x34u, ecx);
    wr32(m, edi + 0x120u, ecx);
    LIFT_CALL(0x6de1c0u, 0x6e106du);  // FUN_006de1c0
    ecx = rd32(m, eax);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0x3u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x707530u, 0x6e1079u);  // FUN_00707530
    goto L_6e10b1;
L_6e107b:
    fA_6e107b = rd32(m, edi + 0x120u);
    fB_6e107b = 0x3u;
    if (fA_6e107b != fB_6e107b) goto L_6e10b1;
    fA_6e1084 = eax;
    fB_6e1084 = 0x2u;
    if (fA_6e1084 != fB_6e1084) goto L_6e1096;
    ecx = (edi + 0x110u);
    esi = 0u;
    LIFT_CALL(0x702f40u, 0x6e1096u);  // FUN_00702f40
L_6e1096:
    esi = 0x1u;
    wr32(m, edi + 0x34u, esi);
    wr32(m, edi + 0x120u, esi);
    LIFT_CALL(0x6de1c0u, 0x6e10a9u);  // FUN_006de1c0
    edx = rd32(m, eax);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x707610u, 0x6e10b1u);  // FUN_00707610
L_6e10b1:
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x8u)));
    esp -= 4; wr32(m, esp, ecx);
    ecx = edi;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x6e0ab0u, 0x6e10bfu);  // NetSignInManager::vf2
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 006e1fe0 NetworkSession__vf2
//   original NetworkSession::vf2; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x006e1fe0, NetworkSession__vf2, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_6e1ff3 = 0;
    uint32_t fB_6e1ff3 = 0;
    FCmp fC_6e2011{};
    uint32_t fA_6e203b = 0;
    uint32_t fB_6e203b = 0;
    uint32_t fA_6e2044 = 0;
    uint32_t fB_6e2044 = 0;
    FCmp fC_6e2050{};
    uint32_t fA_6e205f = 0;
    uint32_t fB_6e205f = 0;
    uint32_t fA_6e2063 = 0;
    uint32_t fB_6e2063 = 0;
    uint32_t fA_6e20b0 = 0;
    uint32_t fB_6e20b0 = 0;
    uint32_t fA_6e20c4 = 0;
    uint32_t fB_6e20c4 = 0;
    uint32_t fA_6e20cc = 0;
    uint32_t fB_6e20cc = 0;
    uint32_t fA_6e20ee = 0;
    uint32_t fB_6e20ee = 0;
    uint32_t fA_6e20f9 = 0;
    uint32_t fB_6e20f9 = 0;
    uint32_t fA_6e2101 = 0;
    uint32_t fB_6e2101 = 0;
    uint32_t fA_6e2124 = 0;
    uint32_t fB_6e2124 = 0;
    uint32_t fA_6e212c = 0;
    uint32_t fB_6e212c = 0;
    uint32_t fA_6e215c = 0;
    uint32_t fB_6e215c = 0;
    uint32_t fR_6e217d = 0;
    uint32_t fA_6e2181 = 0;
    uint32_t fB_6e2181 = 0;
    uint32_t fA_6e21a4 = 0;
    uint32_t fB_6e21a4 = 0;
    uint32_t fA_6e21ce = 0;
    uint32_t fB_6e21ce = 0;
    uint32_t fA_6e21f1 = 0;
    uint32_t fB_6e21f1 = 0;
    uint32_t fR_6e220f = 0;
    uint32_t fA_6e221e = 0;
    uint32_t fB_6e221e = 0;
    uint32_t fR_6e2256 = 0;
    uint32_t fA_6e2266 = 0;
    uint32_t fR_6e2266 = 0;
    uint32_t fA_6e226e = 0;
    uint32_t fB_6e226e = 0;
    uint32_t fA_6e22ba = 0;
    uint32_t fB_6e22ba = 0;
    uint32_t fA_6e22ea = 0;
    uint32_t fB_6e22ea = 0;
    uint32_t fR_6e22f9 = 0;
    uint32_t fA_6e230b = 0;
    uint32_t fB_6e230b = 0;
    uint32_t fA_6e2316 = 0;
    uint32_t fB_6e2316 = 0;
    uint32_t fA_6e2325 = 0;
    uint32_t fB_6e2325 = 0;
    uint32_t fR_6e233c = 0;
    uint32_t fA_6e2340 = 0;
    uint32_t fB_6e2340 = 0;
    uint32_t fA_6e2381 = 0;
    uint32_t fB_6e2381 = 0;
    uint32_t fA_6e2396 = 0;
    uint32_t fB_6e2396 = 0;
    uint32_t fA_6e23ad = 0;
    uint32_t fB_6e23ad = 0;
    uint32_t fA_6e23c7 = 0;
    uint32_t fB_6e23c7 = 0;
    uint32_t fA_6e23d9 = 0;
    uint32_t fB_6e23d9 = 0;
    uint32_t fA_6e23e7 = 0;
    uint32_t fB_6e23e7 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0x34u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    ebx = ecx;
    eax = rd32(m, ebx + 0x78u);
    esp -= 4; wr32(m, esp, edi);
    edi = 0u;
    fA_6e1ff3 = rd32(m, eax + 0x8u);
    fB_6e1ff3 = edi;
    if (fA_6e1ff3 == fB_6e1ff3) goto L_6e2006;
    ecx = rd32(m, eax + 0x8u);
    edx = rd32(m, ecx);
    eax = rd32(m, eax + 0x4u);
    edx = rd32(m, edx + 0x8u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(edx, 0x6e2006u);
L_6e2006:
    x0 = movss_m(rd32(m, ebx + 0x1c5cu));
    x2 = X{};
    fC_6e2011 = fcmp32(x0.l(0), x2.l(0));
    x1 = movss_m(rd32(m, ebp + 0x8u));
    if (fc_b(fC_6e2011)) goto L_6e2027;
    x0 = subss(x0, x1.l(0));
    wr32(m, ebx + 0x1c5cu, x0.l(0));
L_6e2027:
    x0 = movss_m(rd32(m, 0xd9d6c4u));
    x0 = subss(x0, x1.l(0));
    wr32(m, 0xd9d6c4u, x0.l(0));
    fA_6e203b = rd32(m, ebx + 0x7cu);
    fB_6e203b = edi;
    if (fA_6e203b == fB_6e203b) goto L_6e22ea;
    fA_6e2044 = rd32(m, ebx + 0x1c4cu);
    fB_6e2044 = edi;
    if (fA_6e2044 == fB_6e2044) goto L_6e22ea;
    fC_6e2050 = fcmp32(x2.l(0), x0.l(0));
    if (fc_b(fC_6e2050)) goto L_6e22ea;
    eax = rd32(m, ebx + 0x1a0u);
    fA_6e205f = eax;
    fB_6e205f = edi;
    if (int32_t(sext(fA_6e205f, 4)) < int32_t(sext(fB_6e205f, 4))) goto L_6e207e;
    fA_6e2063 = eax;
    fB_6e2063 = 0x1u;
    if (fA_6e2063 >= fB_6e2063) goto L_6e207e;
    ecx = rd32(m, ebx + 0x1dcu);
    eax = (eax << 4);
    edx = (ecx + eax + 0x3c8u);
    wr32(m, esp + 0xcu, edx);
    goto L_6e2082;
L_6e207e:
    wr32(m, esp + 0xcu, edi);
L_6e2082:
    esi = (esp + 0x1cu);
    LIFT_CALL(0x5544c0u, 0x6e208bu);  // FUN_005544c0
    ecx = esi;
    LIFT_CALL(0x508880u, 0x6e2092u);  // FUN_00508880
    ecx = esi;
    LIFT_CALL(0x88c440u, 0x6e2099u);  // FUN_0088c440
    ecx = esi;
    wr32(m, ebx + 0x1d8u, eax);
    wr32(m, ebx + 0x1c50u, edi);
    LIFT_CALL(0x508880u, 0x6e20acu);  // FUN_00508880
    ecx = rd32(m, esp + 0x24u);
    fA_6e20b0 = ecx;
    fB_6e20b0 = edi;
    if (fA_6e20b0 == fB_6e20b0) goto L_6e22c2;
    goto L_6e20c0;
L_6e20c0:
    esi = rd32(m, esp + 0xcu);
    fA_6e20c4 = esi;
    fB_6e20c4 = edi;
    if (fA_6e20c4 == fB_6e20c4) goto L_6e215c;
    fA_6e20cc = ecx;
    fB_6e20cc = edi;
    if (fA_6e20cc != fB_6e20cc) goto L_6e20e8;
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, 0xa0000007u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x884390u, 0x6e20ddu);  // FUN_00884390
    ecx = rd32(m, esp + 0x34u);
    esp = esp + 0x10u;
    eax = 0u;
    goto L_6e20ec;
L_6e20e8:
    eax = rd32(m, esp + 0x20u);
L_6e20ec:
    edx = rd32(m, esi);
    fA_6e20ee = edx;
    fB_6e20ee = rd32(m, eax + 0xd8u);
    if (fA_6e20ee != fB_6e20ee) goto L_6e215c;
    edx = rd32(m, esi + 0x4u);
    fA_6e20f9 = edx;
    fB_6e20f9 = rd32(m, eax + 0xdcu);
    if (fA_6e20f9 != fB_6e20f9) goto L_6e215c;
    fA_6e2101 = ecx;
    fB_6e2101 = edi;
    if (fA_6e2101 != fB_6e2101) goto L_6e211d;
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, 0xa0000007u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x884390u, 0x6e2112u);  // FUN_00884390
    ecx = rd32(m, esp + 0x34u);
    esp = esp + 0x10u;
    eax = 0u;
    goto L_6e2121;
L_6e211d:
    eax = rd32(m, esp + 0x20u);
L_6e2121:
    edx = rd32(m, esi + 0xcu);
    fA_6e2124 = edx;
    fB_6e2124 = rd32(m, eax + 0xc4u);
    if (fA_6e2124 == fB_6e2124) goto L_6e215c;
    fA_6e212c = ecx;
    fB_6e212c = edi;
    if (fA_6e212c != fB_6e212c) goto L_6e2144;
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, 0xa0000007u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x884390u, 0x6e213du);  // FUN_00884390
    esp = esp + 0x10u;
    eax = 0u;
    goto L_6e2148;
L_6e2144:
    eax = rd32(m, esp + 0x20u);
L_6e2148:
    ecx = rd32(m, esi + 0xcu);
    wr32(m, eax + 0xc4u, ecx);
    wr8(m, eax + 0x124u, 0x1u);
    ecx = rd32(m, esp + 0x24u);
L_6e215c:
    fA_6e215c = ecx;
    fB_6e215c = edi;
    if (fA_6e215c != fB_6e215c) goto L_6e2174;
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, 0xa0000007u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x884390u, 0x6e216du);  // FUN_00884390
    esp = esp + 0x10u;
    ecx = 0u;
    goto L_6e2178;
L_6e2174:
    ecx = rd32(m, esp + 0x20u);
L_6e2178:
    LIFT_CALL(0x884950u, 0x6e217du);  // FUN_00884950
    fR_6e217d = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_6e217d == 0)) goto L_6e21ce;
    fA_6e2181 = rd32(m, esp + 0x24u);
    fB_6e2181 = edi;
    if (fA_6e2181 != fB_6e2181) goto L_6e219b;
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, 0xa0000007u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x884390u, 0x6e2194u);  // FUN_00884390
    esp = esp + 0x10u;
    eax = 0u;
    goto L_6e219f;
L_6e219b:
    eax = rd32(m, esp + 0x20u);
L_6e219f:
    LIFT_CALL(0x6f2560u, 0x6e21a4u);  // FUN_006f2560
    fA_6e21a4 = rd32(m, esp + 0x24u);
    fB_6e21a4 = edi;
    if (fA_6e21a4 != fB_6e21a4) goto L_6e21c3;
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, 0xa0000007u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x884390u, 0x6e21b7u);  // FUN_00884390
    esp = esp + 0x10u;
    ecx = 0u;
    LIFT_CALL(0x555540u, 0x6e21c1u);  // FUN_00555540
    goto L_6e21f1;
L_6e21c3:
    ecx = rd32(m, esp + 0x20u);
    LIFT_CALL(0x555540u, 0x6e21ccu);  // FUN_00555540
    goto L_6e21f1;
L_6e21ce:
    fA_6e21ce = rd32(m, esp + 0x24u);
    fB_6e21ce = edi;
    if (fA_6e21ce != fB_6e21ce) goto L_6e21e8;
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, 0xa0000007u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x884390u, 0x6e21e1u);  // FUN_00884390
    esp = esp + 0x10u;
    ecx = 0u;
    goto L_6e21ec;
L_6e21e8:
    ecx = rd32(m, esp + 0x20u);
L_6e21ec:
    LIFT_CALL(0x5088c0u, 0x6e21f1u);  // FUN_005088c0
L_6e21f1:
    fA_6e21f1 = rd32(m, esp + 0x24u);
    fB_6e21f1 = edi;
    if (fA_6e21f1 != fB_6e21f1) goto L_6e220b;
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, 0xa0000007u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x884390u, 0x6e2204u);  // FUN_00884390
    esp = esp + 0x10u;
    eax = 0u;
    goto L_6e220f;
L_6e220b:
    eax = rd32(m, esp + 0x20u);
L_6e220f:
    fR_6e220f = (rd8(m, eax + 0x129u) & 0x1u);
    if ((fR_6e220f == 0)) goto L_6e221e;
    wr32(m, ebx + 0x1c50u, (rd32(m, ebx + 0x1c50u) + 1u));
L_6e221e:
    fA_6e221e = rd32(m, esp + 0x24u);
    fB_6e221e = edi;
    if (fA_6e221e != fB_6e221e) goto L_6e2238;
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, 0xa0000007u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x884390u, 0x6e2231u);  // FUN_00884390
    esp = esp + 0x10u;
    eax = 0u;
    goto L_6e223c;
L_6e2238:
    eax = rd32(m, esp + 0x20u);
L_6e223c:
    eax = rd32(m, eax + 0x14u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = esp;
    wr32(m, ecx, eax);
    ecx = (esp + 0x14u);
    LIFT_CALL(0x88bcd0u, 0x6e224du);  // FUN_0088bcd0
    ecx = (esp + 0x10u);
    LIFT_CALL(0x701af0u, 0x6e2256u);  // FUN_00701af0
    fR_6e2256 = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_6e2256 == 0)) goto L_6e22a3;
    ecx = (esp + 0x10u);
    LIFT_CALL(0x701af0u, 0x6e2263u);  // FUN_00701af0
    esi = (eax & 0xffu);
    fA_6e2266 = esi;
    fR_6e2266 = (0u - fA_6e2266);
    esi = fR_6e2266;
    esi = esi - esi - ((fA_6e2266 != 0) ? 1u : 0u);
    esi = (esi & rd32(m, esp + 0x10u));
    fA_6e226e = rd32(m, esp + 0x24u);
    fB_6e226e = edi;
    if (fA_6e226e != fB_6e226e) goto L_6e2288;
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, 0xa0000007u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x884390u, 0x6e2281u);  // FUN_00884390
    esp = esp + 0x10u;
    eax = 0u;
    goto L_6e228c;
L_6e2288:
    eax = rd32(m, esp + 0x20u);
L_6e228c:
    edi = sext(rd8(m, eax + 0x139u), 1);
    ecx = esi;
    LIFT_CALL(0x89c7f0u, 0x6e229au);  // FUN_0089c7f0
    wr32(m, ebx + edi * 4 + 0x1c68u, eax);
    edi = 0u;
L_6e22a3:
    esp -= 4; wr32(m, esp, edi);
    ecx = (esp + 0x20u);
    LIFT_CALL(0x88c1e0u, 0x6e22adu);  // FUN_0088c1e0
    ecx = (esp + 0x10u);
    LIFT_CALL(0x88bac0u, 0x6e22b6u);  // FUN_0088bac0
    ecx = rd32(m, esp + 0x24u);
    fA_6e22ba = ecx;
    fB_6e22ba = edi;
    if (fA_6e22ba != fB_6e22ba) goto L_6e20c0;
L_6e22c2:
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x8u)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    wr32(m, 0xd9d6c4u, x0.l(0));
    LIFT_CALL(0x6f5130u, 0x6e22deu);  // FUN_006f5130
    esp = esp + 0x4u;
    ecx = (esp + 0x1cu);
    LIFT_CALL(0x88c190u, 0x6e22eau);  // FUN_0088c190
L_6e22ea:
    fA_6e22ea = rd32(m, ebx + 0x1c3cu);
    fB_6e22ea = edi;
    if (fA_6e22ea != fB_6e22ea) goto L_6e2316;
    LIFT_CALL(0x8953c0u, 0x6e22f7u);  // FUN_008953c0
    edx = 0u;
    fR_6e22f9 = ((eax & 0xffu) & (eax & 0xffu));
    edx = (edx & 0xffffff00u) | (((!(fR_6e22f9 == 0)) ? 1u : 0u) & 0xffu);
    wr32(m, ebx + 0x1c4cu, edx);
    LIFT_CALL(0x8953c0u, 0x6e2309u);  // FUN_008953c0
    ecx = 0u;
    fA_6e230b = (eax & 0xffu);
    fB_6e230b = 0x1u;
    ecx = (ecx & 0xffffff00u) | (((fA_6e230b == fB_6e230b) ? 1u : 0u) & 0xffu);
    wr32(m, ebx + 0x1c48u, ecx);
L_6e2316:
    fA_6e2316 = rd32(m, ebx + 0x1bcu);
    fB_6e2316 = edi;
    if (fA_6e2316 == fB_6e2316) goto L_6e2396;
    eax = rd32(m, ebx + 0x1b8u);
    eax = (eax - 1u);
    fA_6e2325 = eax;
    fB_6e2325 = 0x3u;
    if (fA_6e2325 > fB_6e2325) goto L_6e2381;
    switch (eax) { case 0: goto L_6e2372; case 1: goto L_6e2331; case 2: goto L_6e236b; case 3: goto L_6e2372; default: throw std::runtime_error("jump table"); }
L_6e2331:
    ecx = rd32(m, 0xa946b8u);
    LIFT_CALL(0x75c5f0u, 0x6e233cu);  // FUN_0075c5f0
    fR_6e233c = (eax & eax);
    if (!(fR_6e233c == 0)) goto L_6e2381;
    fA_6e2340 = rd32(m, ebx + 0x1c3cu);
    fB_6e2340 = edi;
    if (fA_6e2340 != fB_6e2340) goto L_6e2357;
    eax = rd32(m, 0xa93fccu);
    esp -= 4; wr32(m, esp, 0x962a10u);
    LIFT_CALL(0x59d370u, 0x6e2357u);  // FUN_0059d370
L_6e2357:
    wr32(m, ebx + 0x1bcu, edi);
    wr32(m, ebx + 0x1b8u, edi);
    wr32(m, ebx + 0x1c0u, edi);
    goto L_6e2381;
L_6e236b:
    esp -= 4; wr32(m, esp, 0x962a10u);
    goto L_6e2377;
L_6e2372:
    esp -= 4; wr32(m, esp, 0x9629f8u);
L_6e2377:
    eax = rd32(m, 0xa93fccu);
    LIFT_CALL(0x59d370u, 0x6e2381u);  // FUN_0059d370
L_6e2381:
    fA_6e2381 = rd32(m, ebx + 0x1b8u);
    fB_6e2381 = 0x2u;
    if (fA_6e2381 == fB_6e2381) goto L_6e2396;
    wr32(m, ebx + 0x1bcu, edi);
    wr32(m, ebx + 0x1b8u, edi);
L_6e2396:
    fA_6e2396 = rd32(m, ebx + 0x1c20u);
    fB_6e2396 = edi;
    if (fA_6e2396 == fB_6e2396) goto L_6e23c1;
    esi = rd32(m, ebx + 0x78u);
    eax = 0x1000u;
    ecx = esi;
    LIFT_CALL(0x702d80u, 0x6e23adu);  // FUN_00702d80
    fA_6e23ad = rd32(m, esi + 0x8u);
    fB_6e23ad = eax;
    if (fA_6e23ad != fB_6e23ad) goto L_6e23c1;
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0x3cu);
    ecx = ebx;
    LIFT_CALL(eax, 0x6e23bbu);
    wr32(m, ebx + 0x1c20u, edi);
L_6e23c1:
    ecx = rd32(m, ebx + 0x1dcu);
    fA_6e23c7 = ecx;
    fB_6e23c7 = edi;
    if (fA_6e23c7 == fB_6e23c7) goto L_6e23d9;
    edx = rd32(m, ecx);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x8u)));
    eax = rd32(m, edx + 0x8u);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(eax, 0x6e23d9u);
L_6e23d9:
    fA_6e23d9 = rd32(m, ebx + 0x1c64u);
    fB_6e23d9 = edi;
    if (fA_6e23d9 == fB_6e23d9) goto L_6e23fa;
    esi = rd32(m, 0xa946b8u);
    fA_6e23e7 = rd32(m, esi + 0x15b54u);
    fB_6e23e7 = edi;
    if (fA_6e23e7 == fB_6e23e7) goto L_6e23fa;
    LIFT_CALL(0x75c430u, 0x6e23f4u);  // Game_SetupActivityArea
    wr32(m, ebx + 0x1c64u, edi);
L_6e23fa:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 006e3a90 NetworkSession__vf12
//   original NetworkSession::vf12; register inputs: ecx; returns with ret 12
REFLEX_PORT_FN(0x006e3a90, NetworkSession__vf12, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_6e3abf = 0;
    uint32_t fB_6e3abf = 0;
    uint32_t fA_6e3acc = 0;
    uint32_t fB_6e3acc = 0;
    uint32_t fR_6e3ade = 0;
    uint32_t fA_6e3afe = 0;
    uint32_t fB_6e3afe = 0;
    uint32_t fR_6e3b19 = 0;
    uint32_t fR_6e3b20 = 0;
    uint32_t fA_6e3b38 = 0;
    uint32_t fB_6e3b38 = 0;
    uint32_t fA_6e3b67 = 0;
    uint32_t fB_6e3b67 = 0;
    uint32_t fA_6e3ba4 = 0;
    uint32_t fB_6e3ba4 = 0;
    esp = esp - 0x14u;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp + 0x10u, eax);
    eax = rd32(m, esp + 0x20u);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, 0x935de8u);
    esi = 0xfu;
    edi = (esp + 0x18u);
    ebx = ecx;
    LIFT_CALL(0x78fd60u, 0x6e3abcu);  // FUN_0078fd60
    esp = esp + 0x8u;
    fA_6e3abf = rd32(m, ebx + 0x1c4cu);
    fB_6e3abf = 0x0u;
    if (fA_6e3abf == fB_6e3abf) goto L_6e3bb4;
    fA_6e3acc = rd32(m, ebx + 0x1c3cu);
    fB_6e3acc = 0x0u;
    if (fA_6e3acc != fB_6e3acc) goto L_6e3bb4;
    eax = 0x1u;
    fR_6e3ade = (rd8(m, 0xdcf304u) & (eax & 0xffu));
    if (!(fR_6e3ade == 0)) goto L_6e3afe;
    wr32(m, 0xdcf304u, (rd32(m, 0xdcf304u) | eax));
    LIFT_CALL(0x6de1f0u, 0x6e3af1u);  // FUN_006de1f0
    esp -= 4; wr32(m, esp, 0x913e00u);
    LIFT_CALL(0x8f9fbfu, 0x6e3afbu);  // _atexit
    esp = esp + 0x4u;
L_6e3afe:
    fA_6e3afe = rd32(m, 0xda6d08u);
    fB_6e3afe = 0x0u;
    if (fA_6e3afe == fB_6e3afe) goto L_6e3b33;
    LIFT_CALL(0x7a0760u, 0x6e3b0cu);  // rtti_shared_vf_007a0760
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, 0x3u);
    ecx = 0x9df584u;
    LIFT_CALL(0x402420u, 0x6e3b19u);  // FUN_00402420
    fR_6e3b19 = (eax & eax);
    if ((fR_6e3b19 == 0)) goto L_6e3b24;
    eax = rd32(m, eax + 0xcu);
    fR_6e3b20 = (eax & eax);
    if (!(fR_6e3b20 == 0)) goto L_6e3b2b;
L_6e3b24:
    eax = 0u;
    ebp = (eax + 0x3cu);
    goto L_6e3b38;
L_6e3b2b:
    eax = rd32(m, eax + 0x8u);
    ebp = (eax + 0x3cu);
    goto L_6e3b38;
L_6e3b33:
    ebp = 0xda6cf0u;
L_6e3b38:
    fA_6e3b38 = rd8(m, 0x9df580u);
    fB_6e3b38 = 0x0u;
    if (fA_6e3b38 != fB_6e3b38) goto L_6e3b48;
    ecx = ebp;
    LIFT_CALL(0x883d80u, 0x6e3b48u);  // FUN_00883d80
L_6e3b48:
    LIFT_CALL(0x895290u, 0x6e3b4du);  // FUN_00895290
    esi = rd32(m, esp + 0x2cu);
    edx = rd32(m, esi * 4 + 0x9dc770u);
    ecx = (esp + 0x10u);
    esp -= 4; wr32(m, esp, ecx);
    ebx = eax;
    esp -= 4; wr32(m, esp, edx);
    ecx = ebx;
    LIFT_CALL(0x891400u, 0x6e3b67u);  // FUN_00891400
    fA_6e3b67 = esi;
    fB_6e3b67 = 0xcu;
    if (fA_6e3b67 != fB_6e3b67) goto L_6e3b9d;
    ecx = rd32(m, esp + 0x30u);
    LIFT_CALL(0x6e58f0u, 0x6e3b75u);  // FUN_006e58f0
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, 0x935de8u);
    esi = 0xfu;
    edi = (esp + 0x18u);
    LIFT_CALL(0x78fd60u, 0x6e3b89u);  // FUN_0078fd60
    ecx = rd32(m, 0x9dc778u);
    esp = esp + 0x8u;
    eax = edi;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    ecx = ebx;
    LIFT_CALL(0x891400u, 0x6e3b9du);  // FUN_00891400
L_6e3b9d:
    ecx = ebx;
    LIFT_CALL(0x890e60u, 0x6e3ba4u);  // FUN_00890e60
    fA_6e3ba4 = rd8(m, 0x9df580u);
    fB_6e3ba4 = 0x0u;
    if (fA_6e3ba4 != fB_6e3ba4) goto L_6e3bb4;
    ecx = ebp;
    LIFT_CALL(0x883d90u, 0x6e3bb4u);  // FUN_00883d90
L_6e3bb4:
    ecx = rd32(m, esp + 0x20u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x6e3bc3u);  // __security_check_cookie
    esp = esp + 0x14u;
    LIFT_RET(12);
}

// 006e7810 rtti_shared_vf_006e7810
//   original rtti_shared_vf_006e7810; register inputs: none; returns with ret 4
REFLEX_PORT_FN(0x006e7810, rtti_shared_vf_006e7810, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = 0x1u;
    LIFT_RET(4);
}

// 006e7cf0 NetworkStats__vf2
//   original NetworkStats::vf2; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x006e7cf0, NetworkStats__vf2, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_6e7d07 = 0;
    uint32_t fB_6e7d07 = 0;
    uint32_t fR_6e7d39 = 0;
    uint32_t fA_6e7d3d = 0;
    uint32_t fB_6e7d3d = 0;
    uint32_t fA_6e7d42 = 0;
    uint32_t fB_6e7d42 = 0;
    uint32_t fA_6e7d4f = 0;
    uint32_t fB_6e7d4f = 0;
    uint32_t fA_6e7d62 = 0;
    uint32_t fB_6e7d62 = 0;
    uint32_t fA_6e7d6f = 0;
    uint32_t fB_6e7d6f = 0;
    uint32_t fA_6e7d97 = 0;
    uint32_t fB_6e7d97 = 0;
    uint32_t fR_6e7db5 = 0;
    uint32_t fA_6e7dcc = 0;
    uint32_t fB_6e7dcc = 0;
    uint32_t fA_6e7de1 = 0;
    uint32_t fB_6e7de1 = 0;
    uint32_t fR_6e7e08 = 0;
    uint32_t fR_6e7e10 = 0;
    uint32_t fA_6e7e18 = 0;
    uint32_t fB_6e7e18 = 0;
    uint32_t fA_6e7e30 = 0;
    uint32_t fB_6e7e30 = 0;
    uint32_t fA_6e7e4c = 0;
    uint32_t fB_6e7e4c = 0;
    uint32_t fR_6e7eb4 = 0;
    uint32_t fA_6e7eb8 = 0;
    uint32_t fB_6e7eb8 = 0;
    uint32_t fA_6e7ec4 = 0;
    uint32_t fB_6e7ec4 = 0;
    uint32_t fR_6e7ec8 = 0;
    uint32_t fA_6e7ed2 = 0;
    uint32_t fB_6e7ed2 = 0;
    uint32_t fR_6e7f40 = 0;
    uint32_t fA_6e7f6b = 0;
    uint32_t fB_6e7f6b = 0;
    uint32_t fA_6e7f89 = 0;
    uint32_t fB_6e7f89 = 0;
    uint32_t fA_6e7faa = 0;
    uint32_t fB_6e7faa = 0;
    uint32_t fA_6e8014 = 0;
    uint32_t fB_6e8014 = 0;
    uint32_t fA_6e8021 = 0;
    uint32_t fB_6e8021 = 0;
    uint32_t fA_6e8033 = 0;
    uint32_t fB_6e8033 = 0;
    uint32_t fR_6e8067 = 0;
    uint32_t fA_6e806f = 0;
    uint32_t fB_6e806f = 0;
    uint32_t fR_6e8078 = 0;
    uint32_t fR_6e80cb = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0x68u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    ebp = ecx;
    eax = rd32(m, ebp + 0x50u);
    eax = (eax - 1u);
    wr32(m, esp + 0x14u, ebp);
    fA_6e7d07 = eax;
    fB_6e7d07 = 0x5u;
    if (fA_6e7d07 > fB_6e7d07) goto L_6e7d33;
    switch (eax) { case 0: goto L_6e7d13; case 1: goto L_6e7d1b; case 2: goto L_6e7d23; case 3: goto L_6e7d23; case 4: goto L_6e7d2b; case 5: goto L_6e7d23; default: throw std::runtime_error("jump table"); }
L_6e7d13:
    eax = rd32(m, ebp);
    edx = rd32(m, eax + 0x48u);
    goto L_6e7d31;
L_6e7d1b:
    eax = rd32(m, ebp);
    edx = rd32(m, eax + 0x4cu);
    goto L_6e7d31;
L_6e7d23:
    eax = rd32(m, ebp);
    edx = rd32(m, eax + 0x40u);
    goto L_6e7d31;
L_6e7d2b:
    eax = rd32(m, ebp);
    edx = rd32(m, eax + 0x44u);
L_6e7d31:
    LIFT_CALL(edx, 0x6e7d33u);
L_6e7d33:
    eax = rd32(m, ebp + 0x3a0u);
    fR_6e7d39 = (eax & eax);
    if (((fR_6e7d39 == 0) || sgn(fR_6e7d39, 4) != false)) goto L_6e7d62;
    fA_6e7d3d = eax;
    fB_6e7d3d = 0x2u;
    if (int32_t(sext(fA_6e7d3d, 4)) <= int32_t(sext(fB_6e7d3d, 4))) goto L_6e7d4f;
    fA_6e7d42 = eax;
    fB_6e7d42 = 0x3u;
    if (fA_6e7d42 != fB_6e7d42) goto L_6e7d62;
    esp -= 4; wr32(m, esp, ebp);
    LIFT_CALL(0x6e8a30u, 0x6e7d4du);  // FUN_006e8a30
    goto L_6e7d62;
L_6e7d4f:
    fA_6e7d4f = rd32(m, ebp + 0x3a4u);
    fB_6e7d4f = 0x1u;
    if (fA_6e7d4f == fB_6e7d4f) goto L_6e7d62;
    wr32(m, ebp + 0x3a0u, 0x0u);
L_6e7d62:
    fA_6e7d62 = rd32(m, ebp + 0x380u);
    fB_6e7d62 = 0x0u;
    if (fA_6e7d62 == fB_6e7d62) goto L_6e8014;
    fA_6e7d6f = rd32(m, ebp + 0x50u);
    fB_6e7d6f = 0x0u;
    if (fA_6e7d6f != fB_6e7d6f) goto L_6e8014;
    LIFT_CALL(0x6f8930u, 0x6e7d7eu);  // FUN_006f8930
    eax = (eax + 1u);
    wr32(m, esp + 0x24u, eax);
    LIFT_CALL(0x6f8a00u, 0x6e7d88u);  // FUN_006f8a00
    edi = eax;
    wr32(m, esp + 0x20u, edi);
    LIFT_CALL(0x6f8860u, 0x6e7d93u);  // FUN_006f8860
    ebx = eax;
    eax = 0u;
    fA_6e7d97 = ebx;
    fB_6e7d97 = 0x14u;
    if (fA_6e7d97 > fB_6e7d97) goto L_6e7db1;
    ecx = rd8(m, ebx + 0x6e813cu);
    switch (ecx) { case 0: goto L_6e7daa; case 1: goto L_6e7db3; case 2: goto L_6e7db1; default: throw std::runtime_error("jump table"); }
L_6e7daa:
    eax = 0x1u;
    goto L_6e7db3;
L_6e7db1:
    eax = 0u;
L_6e7db3:
    esi = eax;
    fR_6e7db5 = (eax & eax);
    if (!(fR_6e7db5 == 0)) goto L_6e7dbf;
    wr32(m, ebp + 0x380u, eax);
L_6e7dbf:
    eax = rd32(m, 0xa95844u);
    edx = rd32(m, eax + 0x20u);
    ecx = 0x1u;
    fA_6e7dcc = rd32(m, edx + 0x34u);
    fB_6e7dcc = ecx;
    if (fA_6e7dcc != fB_6e7dcc) goto L_6e7dde;
    eax = rd32(m, 0xa946b8u);
    eax = rd32(m, eax + 0x5a54u);
    goto L_6e7df5;
L_6e7dde:
    edx = rd32(m, eax + 0x1cu);
    fA_6e7de1 = rd32(m, edx + 0x34u);
    fB_6e7de1 = ecx;
    if (fA_6e7de1 != fB_6e7de1) goto L_6e7df3;
    eax = rd32(m, 0xa946b8u);
    eax = rd32(m, eax + 0x20c8u);
    goto L_6e7df5;
L_6e7df3:
    eax = 0u;
L_6e7df5:
    esp -= 4; wr32(m, esp, edi);
    eax = eax + 0x21da0u;
    edi = ebx;
    LIFT_CALL(0x55f630u, 0x6e7e02u);  // FUN_0055f630
    edi = eax;
    wr32(m, esp + 0x18u, edi);
    fR_6e7e08 = (esi & esi);
    if ((fR_6e7e08 == 0)) goto L_6e8014;
    fR_6e7e10 = (edi & edi);
    if ((fR_6e7e10 == 0)) goto L_6e8014;
    fA_6e7e18 = rd32(m, edi + 0x26cu);
    fB_6e7e18 = 0x0u;
    if (int32_t(sext(fA_6e7e18, 4)) <= int32_t(sext(fB_6e7e18, 4))) goto L_6e8014;
    ecx = (esp + 0x30u);
    LIFT_CALL(0x7034e0u, 0x6e7e2eu);  // FUN_007034e0
    eax = 0u;
    fA_6e7e30 = rd32(m, edi + 0x26cu);
    fB_6e7e30 = eax;
    wr32(m, esp + 0x1cu, eax);
    if (int32_t(sext(fA_6e7e30, 4)) <= int32_t(sext(fB_6e7e30, 4))) goto L_6e7fba;
L_6e7e40:
    ecx = rd32(m, edi + 0x268u);
    edx = rd32(m, ecx + eax * 4);
    eax = rd32(m, edx + 0x74u);
    fA_6e7e4c = rd32(m, eax + 0x10u);
    fB_6e7e4c = 0x0u;
    if (fA_6e7e4c != fB_6e7e4c) goto L_6e7fa5;
    ecx = rd32(m, eax + 0x1acu);
    eax = rd32(m, 0xa946b8u);
    edi = rd32(m, ecx + 0x18cu);
    eax = eax + 0x154b0u;
    LIFT_CALL(0x542ff0u, 0x6e7e71u);  // FUN_00542ff0
    edx = rd32(m, eax + 0x8u);
    edi = rd32(m, edx + 0x4u);
    esp -= 4; wr32(m, esp, 0x0u);
    esi = (esp + 0x54u);
    LIFT_CALL(0x4dc570u, 0x6e7e82u);  // FUN_004dc570
    eax = rd32(m, esp + 0x20u);
    ecx = rd32(m, esp + 0x24u);
    esp -= 4; wr32(m, esp, 0x1u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, ebx);
    edx = esi;
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x6e8350u, 0x6e7e98u);  // FUN_006e8350
    eax = rd32(m, esp + 0x68u);
    esi = rd32(m, eax);
    edi = rd32(m, esp + 0x50u);
    wr32(m, esp + 0x28u, edi);
    wr32(m, esp + 0x2cu, esi);
L_6e7eb0:
    ebp = rd32(m, esp + 0x68u);
    fR_6e7eb4 = (edi & edi);
    if ((fR_6e7eb4 == 0)) goto L_6e7ebe;
    fA_6e7eb8 = edi;
    fB_6e7eb8 = rd32(m, esp + 0x50u);
    if (fA_6e7eb8 == fB_6e7eb8) goto L_6e7ec4;
L_6e7ebe:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x6e7ec4u);
L_6e7ec4:
    fA_6e7ec4 = esi;
    fB_6e7ec4 = ebp;
    if (fA_6e7ec4 == fB_6e7ec4) goto L_6e7f3c;
    fR_6e7ec8 = (edi & edi);
    if (!(fR_6e7ec8 == 0)) goto L_6e7f38;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x6e7ed2u);
L_6e7ed2:
    fA_6e7ed2 = esi;
    fB_6e7ed2 = rd32(m, edi + 0x18u);
    if (fA_6e7ed2 != fB_6e7ed2) goto L_6e7edd;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x6e7eddu);
L_6e7edd:
    ebp = rd32(m, esi + 0xcu);
    esi = rd32(m, esi + 0x14u);
    esp -= 4; wr32(m, esp, 0x93e2d8u);
    eax = esi;
    LIFT_CALL(0x402160u, 0x6e7eefu);  // Table_ColumnIndex
    edi = eax;
    esp -= 4; wr32(m, esp, ebp);
    eax = esi;
    LIFT_CALL(0x41aaf0u, 0x6e7ef9u);  // FUN_0041aaf0
    ecx = rd32(m, esp + 0x44u);
    edx = rd32(m, ecx + 0x4u);
    edi = (ecx + 0x4u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x5ba990u, 0x6e7f0bu);  // FUN_005ba990
    ecx = 0x1u;
    edx = (esp + 0x30u);
    esi = eax;
    LIFT_CALL(0x706360u, 0x6e7f1bu);  // FUN_00706360
    wr32(m, edi, esi);
    eax = rd32(m, esi + 0x4u);
    wr32(m, eax, esi);
    esi = (esp + 0x28u);
    LIFT_CALL(0x417df0u, 0x6e7f2bu);  // FUN_00417df0
    esi = rd32(m, esp + 0x2cu);
    edi = rd32(m, esp + 0x28u);
    goto L_6e7eb0;
L_6e7f38:
    edi = rd32(m, edi);
    goto L_6e7ed2;
L_6e7f3c:
    esi = rd32(m, esp + 0x70u);
    fR_6e7f40 = (esi & esi);
    if ((fR_6e7f40 == 0)) goto L_6e7f51;
    LIFT_CALL(0x4ebe10u, 0x6e7f49u);  // FUN_004ebe10
    wr32(m, esp + 0x70u, 0x0u);
L_6e7f51:
    esi = (esp + 0x50u);
    wr32(m, esp + 0x74u, 0x0u);
    LIFT_CALL(0x4f2220u, 0x6e7f62u);  // FUN_004f2220
    esi = rd32(m, esp + 0x50u);
    LIFT_CALL(0x4dc0f0u, 0x6e7f6bu);  // FUN_004dc0f0
    fA_6e7f6b = rd8(m, 0xda67d8u);
    fB_6e7f6b = 0x0u;
    if (fA_6e7f6b == fB_6e7f6b) goto L_6e7f7f;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x6e7f7fu);
L_6e7f7f:
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(rd32(m, 0x916350u) /* 8.287536956934404e-39 */, 0x6e7f86u);
    esp = esp + 0x4u;
    fA_6e7f89 = rd8(m, 0xda67d8u);
    fB_6e7f89 = 0x0u;
    if (fA_6e7f89 == fB_6e7f89) goto L_6e7f9d;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x6e7f9du);
L_6e7f9d:
    edi = rd32(m, esp + 0x18u);
    ebp = rd32(m, esp + 0x14u);
L_6e7fa5:
    eax = rd32(m, esp + 0x1cu);
    eax = (eax + 1u);
    fA_6e7faa = eax;
    fB_6e7faa = rd32(m, edi + 0x26cu);
    wr32(m, esp + 0x1cu, eax);
    if (int32_t(sext(fA_6e7faa, 4)) < int32_t(sext(fB_6e7faa, 4))) goto L_6e7e40;
L_6e7fba:
    ecx = (esp + 0x30u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x703600u, 0x6e7fc4u);  // FUN_00703600
    edx = (esp + 0x30u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x703510u, 0x6e7fceu);  // FUN_00703510
    eax = rd32(m, ebp);
    edx = rd32(m, eax + 0x20u);
    ecx = (esp + 0x30u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, edi);
    ecx = ebp;
    LIFT_CALL(edx, 0x6e7fdfu);
    esi = (esp + 0x30u);
    wr32(m, ebp + 0x380u, 0x0u);
    LIFT_CALL(0x593ef0u, 0x6e7ff2u);  // FUN_00593ef0
    eax = rd32(m, esp + 0x44u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x78fc50u, 0x6e7ffcu);  // ReflexAlignedFree
    ecx = rd32(m, esp + 0x34u);
    esp = esp + 0x4u;
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp + 0x48u, 0x0u);
    LIFT_CALL(0x78fc50u, 0x6e8011u);  // ReflexAlignedFree
    esp = esp + 0x4u;
L_6e8014:
    fA_6e8014 = rd32(m, ebp + 0x384u);
    fB_6e8014 = 0x0u;
    if (fA_6e8014 == fB_6e8014) goto L_6e810e;
    fA_6e8021 = rd32(m, ebp + 0x3a0u);
    fB_6e8021 = 0x0u;
    if (fA_6e8021 != fB_6e8021) goto L_6e810e;
    LIFT_CALL(0x6f8860u, 0x6e8033u);  // FUN_006f8860
    fA_6e8033 = eax;
    fB_6e8033 = 0x14u;
    if (fA_6e8033 > fB_6e8033) goto L_6e8104;
    edx = rd8(m, eax + 0x6e815cu);
    switch (edx) { case 0: goto L_6e804a; case 1: goto L_6e8104; default: throw std::runtime_error("jump table"); }
L_6e804a:
    esi = (esp + 0x30u);
    LIFT_CALL(0x7029f0u, 0x6e8053u);  // FUN_007029f0
    esi = (esp + 0x50u);
    LIFT_CALL(0x5544c0u, 0x6e805cu);  // FUN_005544c0
    ecx = esi;
    LIFT_CALL(0x508880u, 0x6e8063u);  // FUN_00508880
    ecx = rd32(m, esp + 0x58u);
    fR_6e8067 = (ecx & ecx);
    if ((fR_6e8067 == 0)) goto L_6e80cf;
L_6e806b:
    eax = rd32(m, esp + 0x54u);
    fA_6e806f = rd8(m, eax + 0x138u);
    fB_6e806f = 0x0u;
    if (fA_6e806f != fB_6e806f) goto L_6e80bc;
    fR_6e8078 = (ecx & ecx);
    if (!(fR_6e8078 == 0)) goto L_6e8090;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, 0xa0000007u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x884390u, 0x6e8089u);  // FUN_00884390
    esp = esp + 0x10u;
    eax = 0u;
    goto L_6e8094;
L_6e8090:
    eax = rd32(m, esp + 0x54u);
L_6e8094:
    edx = rd32(m, esp + 0x44u);
    edi = (edx + 0x4u);
    ecx = (eax + 0xd8u);
    eax = rd32(m, edi);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x705cc0u, 0x6e80aau);  // FUN_00705cc0
    ecx = (esp + 0x30u);
    esi = eax;
    LIFT_CALL(0x705d70u, 0x6e80b5u);  // FUN_00705d70
    wr32(m, edi, esi);
    ecx = rd32(m, esi + 0x4u);
    wr32(m, ecx, esi);
L_6e80bc:
    esp -= 4; wr32(m, esp, 0x0u);
    ecx = (esp + 0x54u);
    LIFT_CALL(0x88c1e0u, 0x6e80c7u);  // FUN_0088c1e0
    ecx = rd32(m, esp + 0x58u);
    fR_6e80cb = (ecx & ecx);
    if (!(fR_6e80cb == 0)) goto L_6e806b;
L_6e80cf:
    edx = rd32(m, ebp);
    edx = rd32(m, edx + 0x34u);
    eax = (esp + 0x30u);
    esp -= 4; wr32(m, esp, eax);
    ecx = ebp;
    LIFT_CALL(edx, 0x6e80deu);
    ecx = (esp + 0x50u);
    wr32(m, ebp + 0x384u, 0x0u);
    LIFT_CALL(0x88c190u, 0x6e80f1u);  // FUN_0088c190
    eax = (esp + 0x30u);
    LIFT_CALL(0x6ddc40u, 0x6e80fau);  // FUN_006ddc40
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_6e8104:
    wr32(m, ebp + 0x384u, 0x0u);
L_6e810e:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 006e9820 PCSession__vf2
//   original PCSession::vf2; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x006e9820, PCSession__vf2, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_6e9823 = 0;
    uint32_t fB_6e9823 = 0;
    uint32_t fR_6e983b = 0;
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    fA_6e9823 = rd32(m, esi + 0x1d10u);
    fB_6e9823 = 0x0u;
    if (fA_6e9823 == fB_6e9823) goto L_6e9866;
    eax = 0x1u;
    wr32(m, esi + 0x1d10u, 0x0u);
    fR_6e983b = (rd8(m, 0xdcf304u) & (eax & 0xffu));
    if (!(fR_6e983b == 0)) goto L_6e985b;
    wr32(m, 0xdcf304u, (rd32(m, 0xdcf304u) | eax));
    LIFT_CALL(0x6de1f0u, 0x6e984eu);  // FUN_006de1f0
    esp -= 4; wr32(m, esp, 0x913e00u);
    LIFT_CALL(0x8f9fbfu, 0x6e9858u);  // _atexit
    esp = esp + 0x4u;
L_6e985b:
    eax = rd32(m, 0xda6ca0u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x7082c0u, 0x6e9866u);  // FUN_007082c0
L_6e9866:
    LIFT_CALL(rd32(m, 0x9166b8u) /* 8.280166127012055e-39 */, 0x6e986cu);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x8u)));
    esp -= 4; wr32(m, esp, ecx);
    ecx = esi;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x6e1fe0u, 0x6e987bu);  // NetworkSession::vf2
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 006e9e00 PCSession__vf12
//   original PCSession::vf12; register inputs: ecx; returns with ret 12
REFLEX_PORT_FN(0x006e9e00, PCSession__vf12, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_6e9e13 = 0;
    uint32_t fA_6e9e33 = 0;
    uint32_t fB_6e9e33 = 0;
    uint32_t fA_6e9e46 = 0;
    uint32_t fB_6e9e46 = 0;
    uint32_t fR_6e9e53 = 0;
    uint32_t fA_6e9e5e = 0;
    uint32_t fB_6e9e5e = 0;
    uint32_t fA_6e9e69 = 0;
    uint32_t fB_6e9e69 = 0;
    uint32_t fA_6e9e74 = 0;
    uint32_t fB_6e9e74 = 0;
    uint32_t fR_6e9e87 = 0;
    uint32_t fR_6e9e90 = 0;
    uint32_t fA_6e9e9c = 0;
    uint32_t fB_6e9e9c = 0;
    uint32_t fA_6e9f25 = 0;
    uint32_t fB_6e9f25 = 0;
    uint32_t fA_6e9f7e = 0;
    uint32_t fB_6e9f7e = 0;
    uint32_t fA_6e9f85 = 0;
    uint32_t fB_6e9f85 = 0;
    uint32_t fA_6e9f91 = 0;
    uint32_t fB_6e9f91 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0x14u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    ebx = 0x1u;
    esp -= 4; wr32(m, esp, edi);
    esi = ecx;
    fR_6e9e13 = (rd8(m, 0xdcf304u) & (ebx & 0xffu));
    if (!(fR_6e9e13 == 0)) goto L_6e9e33;
    wr32(m, 0xdcf304u, (rd32(m, 0xdcf304u) | ebx));
    LIFT_CALL(0x6de1f0u, 0x6e9e26u);  // FUN_006de1f0
    esp -= 4; wr32(m, esp, 0x913e00u);
    LIFT_CALL(0x8f9fbfu, 0x6e9e30u);  // _atexit
    esp = esp + 0x4u;
L_6e9e33:
    fA_6e9e33 = rd32(m, esi + 0x1c4cu);
    fB_6e9e33 = 0x0u;
    if (fA_6e9e33 == fB_6e9e33) goto L_6e9fe9;
    ecx = rd32(m, esi + 0x1b0u);
    fA_6e9e46 = ecx;
    fB_6e9e46 = ebx;
    if (fA_6e9e46 == fB_6e9e46) goto L_6e9fe9;
    eax = rd32(m, 0xda6cacu);
    fR_6e9e53 = (eax & eax);
    if ((fR_6e9e53 == 0)) goto L_6e9fe9;
    edx = rd32(m, ebp + 0x8u);
    fA_6e9e5e = edx;
    fB_6e9e5e = ebx;
    if (fA_6e9e5e > fB_6e9e5e) goto L_6e9fe9;
    edx = (edx << 6);
    fA_6e9e69 = rd32(m, edx + eax + 0x34u);
    fB_6e9e69 = 0x2u;
    if (int32_t(sext(fA_6e9e69, 4)) < int32_t(sext(fB_6e9e69, 4))) goto L_6e9fe9;
    fA_6e9e74 = rd32(m, esi + 0x1c3cu);
    fB_6e9e74 = 0x0u;
    eax = rd32(m, esi + 0x1d0cu);
    if (fA_6e9e74 != fB_6e9e74) goto L_6e9f25;
    fR_6e9e87 = ((ecx & 0xffu) & 0xcu);
    if ((fR_6e9e87 == 0)) goto L_6e9f25;
    fR_6e9e90 = (eax & eax);
    if ((fR_6e9e90 == 0)) goto L_6e9f25;
    ecx = rd32(m, ebp + 0xcu);
    ecx = (ecx - 1u);
    fA_6e9e9c = ecx;
    fB_6e9e9c = 0x26u;
    if (fA_6e9e9c > fB_6e9e9c) goto L_6e9f25;
    ecx = rd8(m, ecx + 0x6ea044u);
    switch (ecx) { case 0: goto L_6e9eb3; case 1: goto L_6e9ec3; case 2: goto L_6e9f07; case 3: goto L_6e9ebb; case 4: goto L_6e9eff; case 5: goto L_6e9ecb; case 6: goto L_6e9ed3; case 7: goto L_6e9edb; case 8: goto L_6e9ee5; case 9: goto L_6e9eef; case 10: goto L_6e9ef7; case 11: goto L_6e9f17; case 12: goto L_6e9f1f; case 13: goto L_6e9f0f; case 14: goto L_6e9f25; default: throw std::runtime_error("jump table"); }
L_6e9eb3:
    edx = rd32(m, ebp + 0x10u);
    wr32(m, eax + 0x28u, edx);
    goto L_6e9f25;
L_6e9ebb:
    ecx = rd32(m, ebp + 0x10u);
    wr32(m, eax + 0x2cu, ecx);
    goto L_6e9f25;
L_6e9ec3:
    edx = rd32(m, ebp + 0x10u);
    wr32(m, eax + 0x34u, edx);
    goto L_6e9f25;
L_6e9ecb:
    ecx = rd32(m, ebp + 0x10u);
    wr32(m, eax + 0x3cu, ecx);
    goto L_6e9f25;
L_6e9ed3:
    edx = rd32(m, ebp + 0x10u);
    wr32(m, eax + 0x4cu, edx);
    goto L_6e9f25;
L_6e9edb:
    ecx = (ecx & 0xffff0000u) | ((rd16(m, ebp + 0x10u)) & 0xffffu);
    wr16(m, eax + 0x12u, (ecx & 0xffffu));
    goto L_6e9f25;
L_6e9ee5:
    edx = (edx & 0xffff0000u) | ((rd16(m, ebp + 0x10u)) & 0xffffu);
    wr16(m, eax + 0x10u, (edx & 0xffffu));
    goto L_6e9f25;
L_6e9eef:
    ecx = rd32(m, ebp + 0x10u);
    wr32(m, eax + 0x40u, ecx);
    goto L_6e9f25;
L_6e9ef7:
    edx = rd32(m, ebp + 0x10u);
    wr32(m, eax + 0x44u, edx);
    goto L_6e9f25;
L_6e9eff:
    ecx = rd32(m, ebp + 0x10u);
    wr32(m, eax + 0x30u, ecx);
    goto L_6e9f25;
L_6e9f07:
    edx = (edx & 0xffffff00u) | ((rd8(m, ebp + 0x10u)) & 0xffu);
    wr8(m, eax + 0x38u, (edx & 0xffu));
    goto L_6e9f25;
L_6e9f0f:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, ebp + 0x10u)) & 0xffu);
    wr8(m, eax + 0x59u, (ecx & 0xffu));
    goto L_6e9f25;
L_6e9f17:
    edx = rd32(m, ebp + 0x10u);
    wr32(m, eax + 0x54u, edx);
    goto L_6e9f25;
L_6e9f1f:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, ebp + 0x10u)) & 0xffu);
    wr8(m, eax + 0x58u, (ecx & 0xffu));
L_6e9f25:
    fA_6e9f25 = rd32(m, esi + 0x1c48u);
    fB_6e9f25 = 0x0u;
    if (fA_6e9f25 == fB_6e9f25) goto L_6e9fe9;
    LIFT_CALL(rd32(m, 0x9166dcu) /* 8.279883064722262e-39 */, 0x6e9f38u);
    edx = rd32(m, eax);
    edx = rd32(m, edx + 0x8u);
    ecx = (esp + 0x10u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = eax;
    LIFT_CALL(edx, 0x6e9f46u);
    edi = rd32(m, eax);
    eax = rd32(m, eax + 0x4u);
    wr32(m, esp + 0x14u, eax);
    LIFT_CALL(rd32(m, 0x9166b0u) /* 8.28009325949191e-39 */, 0x6e9f55u);
    edx = rd32(m, esi + 0x1d00u);
    ebx = rd32(m, esi + 0x1d04u);
    esp = esp - 0x8u;
    ecx = esp;
    wr32(m, ecx, edx);
    wr32(m, ecx + 0x4u, ebx);
    edx = rd32(m, eax);
    edx = rd32(m, edx + 0x8cu);
    ecx = (esp + 0x20u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = eax;
    LIFT_CALL(edx, 0x6e9f7cu);
    ecx = rd32(m, eax);
    fA_6e9f7e = ecx;
    fB_6e9f7e = edi;
    if (fA_6e9f7e != fB_6e9f7e) goto L_6e9fe9;
    edx = rd32(m, eax + 0x4u);
    fA_6e9f85 = edx;
    fB_6e9f85 = rd32(m, esp + 0x14u);
    if (fA_6e9f85 != fB_6e9f85) goto L_6e9fe9;
    edi = rd32(m, ebp + 0xcu);
    eax = rd32(m, ebp + 0x10u);
    fA_6e9f91 = edi;
    fB_6e9f91 = 0xfu;
    if (fA_6e9f91 != fB_6e9f91) goto L_6e9fa3;
    edx = (eax >> 31) ? 0xffffffffu : 0;
    eax = (eax ^ edx);
    eax = eax - edx;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, 0x94be60u);
    goto L_6e9fa9;
L_6e9fa3:
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, 0x935de8u);
L_6e9fa9:
    esp -= 4; wr32(m, esp, 0xdcf4a8u);
    LIFT_CALL(rd32(m, 0x916204u) /* 8.286141263663936e-39 */, 0x6e9fb4u);
    esp = esp + 0xcu;
    LIFT_CALL(rd32(m, 0x9166b0u) /* 8.28009325949191e-39 */, 0x6e9fbdu);
    ecx = rd32(m, edi * 4 + 0x9dc770u);
    edx = rd32(m, esi + 0x1d00u);
    edi = rd32(m, esi + 0x1d04u);
    esp -= 4; wr32(m, esp, 0xdcf4a8u);
    esp -= 4; wr32(m, esp, ecx);
    esp = esp - 0x8u;
    ecx = esp;
    wr32(m, ecx, edx);
    wr32(m, ecx + 0x4u, edi);
    edx = rd32(m, eax);
    ecx = eax;
    eax = rd32(m, edx + 0x50u);
    LIFT_CALL(eax, 0x6e9fe9u);
L_6e9fe9:
    ecx = rd32(m, ebp + 0x10u);
    edx = rd32(m, ebp + 0xcu);
    eax = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, eax);
    ecx = esi;
    LIFT_CALL(0x6e3a90u, 0x6e9ffcu);  // NetworkSession::vf12
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(12);
}

// 006ebf50 PCStats__vf2
//   original PCStats::vf2; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x006ebf50, PCStats__vf2, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x4u)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x6e7cf0u, 0x6ebf5du);  // NetworkStats::vf2
    LIFT_RET(4);
}

// 006ed7e0 Fn_006ed7e0
//   original FUN_006ed7e0; register inputs: esi; returns with ret 4
REFLEX_PORT_FN(0x006ed7e0, Fn_006ed7e0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_6ed7e3 = 0;
    uint32_t fB_6ed7e3 = 0;
    uint32_t fA_6ed7ec = 0;
    uint32_t fB_6ed7ec = 0;
    FCmp fC_6ed803{};
    uint32_t fA_6ed811 = 0;
    uint32_t fB_6ed811 = 0;
    uint32_t fA_6ed819 = 0;
    uint32_t fB_6ed819 = 0;
    uint32_t fA_6ed824 = 0;
    uint32_t fB_6ed824 = 0;
    uint32_t fA_6ed845 = 0;
    uint32_t fB_6ed845 = 0;
    eax = rd32(m, esi + 0xcu);
    fA_6ed7e3 = rd32(m, eax + 0x1c4cu);
    fB_6ed7e3 = 0x0u;
    if (fA_6ed7e3 == fB_6ed7e3) goto L_6ed7f5;
    fA_6ed7ec = rd32(m, eax + 0x1b0u);
    fB_6ed7ec = 0x1u;
    if (fA_6ed7ec != fB_6ed7ec) goto L_6ed854;
L_6ed7f5:
    x0 = movss_m(rd32(m, esi + 0x8u));
    x0 = subss(x0, rd32(m, esp + 0x4u));
    x1 = X{};
    fC_6ed803 = fcmp32(x1.l(0), x0.l(0));
    wr32(m, esi + 0x8u, x0.l(0));
    if (fc_b(fC_6ed803)) goto L_6ed845;
    ecx = 0u;
    eax = 0u;
L_6ed811:
    fA_6ed811 = ecx;
    fB_6ed811 = 0x1u;
    if (fA_6ed811 > fB_6ed811) goto L_6ed820;
    edx = rd32(m, esi + 0x10u);
    fA_6ed819 = rd32(m, edx + eax + 0x34u);
    fB_6ed819 = 0x2u;
    if (fA_6ed819 == fB_6ed819) goto L_6ed82b;
L_6ed820:
    eax = eax + 0x40u;
    ecx = (ecx + 1u);
    fA_6ed824 = eax;
    fB_6ed824 = 0x40u;
    if (fA_6ed824 < fB_6ed824) goto L_6ed811;
    goto L_6ed845;
L_6ed82b:
    esp -= 4; wr32(m, esp, esi);
    wr32(m, esi + 0x3cu, 0x0u);
    LIFT_CALL(0x6ed860u, 0x6ed838u);  // FUN_006ed860
    x0 = movss_m(rd32(m, 0x92f8e4u) /* 3600.0 */);
    wr32(m, esi + 0x8u, x0.l(0));
L_6ed845:
    fA_6ed845 = rd32(m, esi + 0x3cu);
    fB_6ed845 = 0x0u;
    if (fA_6ed845 == fB_6ed845) goto L_6ed854;
    wr32(m, esp + 0x4u, esi);
    LIFT_TAILJMP(0x6ed8f0u);  // FUN_006ed8f0
L_6ed854:
    LIFT_RET(4);
}

// 006ee820 Fn_006ee820
//   original FUN_006ee820; register inputs: ecx esi; returns with ret 4
REFLEX_PORT_FN(0x006ee820, Fn_006ee820, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_6ee824 = 0;
    uint32_t fB_6ee824 = 0;
    uint32_t fA_6ee82e = 0;
    uint32_t fB_6ee82e = 0;
    FCmp fC_6ee849{};
    uint32_t fA_6ee857 = 0;
    uint32_t fB_6ee857 = 0;
    uint32_t fA_6ee85f = 0;
    uint32_t fB_6ee85f = 0;
    uint32_t fA_6ee86a = 0;
    uint32_t fB_6ee86a = 0;
    uint32_t fA_6ee8b2 = 0;
    uint32_t fB_6ee8b2 = 0;
    esp -= 4; wr32(m, esp, ecx);
    eax = rd32(m, esi + 0xcu);
    fA_6ee824 = rd32(m, eax + 0x1c4cu);
    fB_6ee824 = 0x0u;
    esp -= 4; wr32(m, esp, edi);
    if (fA_6ee824 == fB_6ee824) goto L_6ee83b;
    fA_6ee82e = rd32(m, eax + 0x1b0u);
    fB_6ee82e = 0x1u;
    if (fA_6ee82e != fB_6ee82e) goto L_6ee8c1;
L_6ee83b:
    x0 = movss_m(rd32(m, esi + 0x8u));
    x0 = subss(x0, rd32(m, esp + 0xcu));
    x1 = X{};
    fC_6ee849 = fcmp32(x1.l(0), x0.l(0));
    wr32(m, esi + 0x8u, x0.l(0));
    if (fc_b(fC_6ee849)) goto L_6ee8b2;
    ecx = 0u;
    eax = 0u;
L_6ee857:
    fA_6ee857 = ecx;
    fB_6ee857 = 0x1u;
    if (fA_6ee857 > fB_6ee857) goto L_6ee866;
    edx = rd32(m, esi + 0x10u);
    fA_6ee85f = rd32(m, edx + eax + 0x34u);
    fB_6ee85f = 0x2u;
    if (fA_6ee85f == fB_6ee85f) goto L_6ee871;
L_6ee866:
    eax = eax + 0x40u;
    ecx = (ecx + 1u);
    fA_6ee86a = eax;
    fB_6ee86a = 0x40u;
    if (fA_6ee86a < fB_6ee86a) goto L_6ee857;
    goto L_6ee8b2;
L_6ee871:
    edi = (esi + 0x1cu);
    wr32(m, esi + 0x8cu, 0x0u);
    wr32(m, esi + 0x18u, 0x0u);
    LIFT_CALL(0x7027d0u, 0x6ee88au);  // FUN_007027d0
    x0 = movss_m(rd32(m, 0x92f8e8u) /* 28800.0 */);
    wr32(m, esi + 0x18u, 0x2u);
    wr32(m, esi + 0x90u, 0x1u);
    wr32(m, esi + 0x8cu, 0x1u);
    wr32(m, esi + 0x8u, x0.l(0));
L_6ee8b2:
    fA_6ee8b2 = rd32(m, esi + 0x8cu);
    fB_6ee8b2 = 0x0u;
    if (fA_6ee8b2 == fB_6ee8b2) goto L_6ee8c1;
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x6ee900u, 0x6ee8c1u);  // FUN_006ee900
L_6ee8c1:
    edi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00703070 Fn_00703070
//   original FUN_00703070; register inputs: eax; returns with ret 0
REFLEX_PORT_FN(0x00703070, Fn_00703070, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_703072 = 0;
    uint32_t fR_7030a8 = 0;
    eax = rd32(m, eax);
    fR_703072 = (eax & eax);
    if (!(fR_703072 == 0)) goto L_703089;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, 0xa0030004u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x884390u, 0x703083u);  // FUN_00884390
    esp = esp + 0x10u;
    eax = (eax & 0xffffff00u) | ((0u) & 0xffu);
    LIFT_RET(0);
L_703089:
    eax = rd32(m, eax + 0x50u);
    eax = (eax >> 22);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x8847a0u, 0x703095u);  // thunk_FUN_008a32a0
    ecx = rd32(m, 0xa9435cu);
    edx = rd32(m, eax);
    edx = rd32(m, edx + 0x3cu);
    esp = esp + 0x4u;
    esp -= 4; wr32(m, esp, ecx);
    ecx = eax;
    LIFT_CALL(edx, 0x7030a8u);
    fR_7030a8 = ((eax & 0xffu) & (eax & 0xffu));
    if (!(fR_7030a8 == 0)) goto L_7030c2;
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0xe003000cu);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0x0u);
    LIFT_CALL(0x884390u, 0x7030bcu);  // FUN_00884390
    esp = esp + 0x10u;
    eax = (eax & 0xffffff00u) | ((0u) & 0xffu);
    LIFT_RET(0);
L_7030c2:
    eax = (eax & 0xffffff00u) | ((0x1u) & 0xffu);
    LIFT_RET(0);
}

// 00705150 Fn_00705150
//   original FUN_00705150; register inputs: eax; returns with ret 20
REFLEX_PORT_FN(0x00705150, Fn_00705150, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_705164 = 0;
    uint32_t fA_705168 = 0;
    uint32_t fB_705168 = 0;
    uint32_t fA_705176 = 0;
    uint32_t fB_705176 = 0;
    uint32_t fR_705185 = 0;
    uint32_t fA_705189 = 0;
    uint32_t fB_705189 = 0;
    uint32_t fA_705197 = 0;
    uint32_t fB_705197 = 0;
    uint32_t fR_7051e0 = 0;
    uint32_t fA_7051e4 = 0;
    uint32_t fB_7051e4 = 0;
    uint32_t fA_7051f8 = 0;
    uint32_t fB_7051f8 = 0;
    esp = esp - 0x8u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = eax;
    eax = rd32(m, edi + 0x18u);
    esi = rd32(m, eax);
    eax = rd32(m, edi);
    fR_705164 = (ebx & ebx);
    if ((fR_705164 == 0)) goto L_70516c;
    fA_705168 = ebx;
    fB_705168 = eax;
    if (fA_705168 == fB_705168) goto L_705176;
L_70516c:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x705172u);
    ebx = rd32(m, esp + 0x20u);
L_705176:
    fA_705176 = rd32(m, esp + 0x24u);
    fB_705176 = esi;
    if (fA_705176 != fB_705176) goto L_7051e0;
    ecx = rd32(m, esp + 0x28u);
    esi = rd32(m, edi + 0x18u);
    eax = rd32(m, edi);
    fR_705185 = (ecx & ecx);
    if ((fR_705185 == 0)) goto L_70518d;
    fA_705189 = ecx;
    fB_705189 = eax;
    if (fA_705189 == fB_705189) goto L_705197;
L_70518d:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x705193u);
    ebx = rd32(m, esp + 0x20u);
L_705197:
    fA_705197 = rd32(m, esp + 0x2cu);
    fB_705197 = esi;
    if (fA_705197 != fB_705197) goto L_7051e0;
    ecx = rd32(m, edi + 0x18u);
    edx = rd32(m, ecx + 0x4u);
    esp -= 4; wr32(m, esp, edx);
    ecx = edi;
    LIFT_CALL(0x68f300u, 0x7051abu);  // FUN_0068f300
    eax = rd32(m, edi + 0x18u);
    wr32(m, eax + 0x4u, eax);
    eax = rd32(m, edi + 0x18u);
    wr32(m, edi + 0x1cu, 0x0u);
    wr32(m, eax, eax);
    eax = rd32(m, edi + 0x18u);
    wr32(m, eax + 0x8u, eax);
    eax = rd32(m, edi + 0x18u);
    ecx = rd32(m, eax);
    eax = rd32(m, esp + 0x1cu);
    edx = rd32(m, edi);
    wr32(m, eax + 0x4u, ecx);
    wr32(m, eax, edx);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x8u;
    LIFT_RET(20);
L_7051e0:
    fR_7051e0 = (ebx & ebx);
    if ((fR_7051e0 == 0)) goto L_7051ea;
    fA_7051e4 = ebx;
    fB_7051e4 = rd32(m, esp + 0x28u);
    if (fA_7051e4 == fB_7051e4) goto L_7051f4;
L_7051ea:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7051f0u);
    ebx = rd32(m, esp + 0x20u);
L_7051f4:
    ebp = rd32(m, esp + 0x24u);
    fA_7051f8 = ebp;
    fB_7051f8 = rd32(m, esp + 0x2cu);
    if (fA_7051f8 == fB_7051f8) goto L_70521a;
    esi = (esp + 0x20u);
    LIFT_CALL(0x690280u, 0x705207u);  // FUN_00690280
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, ebx);
    eax = (esp + 0x18u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x706030u, 0x705214u);  // FUN_00706030
    ebx = rd32(m, esp + 0x20u);
    goto L_7051e0;
L_70521a:
    edi = rd32(m, edi);
    eax = rd32(m, esp + 0x1cu);
    wr32(m, eax, edi);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    wr32(m, eax + 0x4u, ebp);
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x8u;
    LIFT_RET(20);
}

// 00705f70 Fn_00705f70
//   original FUN_00705f70; register inputs: none; returns with ret 12
REFLEX_PORT_FN(0x00705f70, Fn_00705f70, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_705f82 = 0;
    uint32_t fB_705f82 = 0;
    uint32_t fR_705f9e = 0;
    uint32_t fA_705fa7 = 0;
    uint32_t fB_705fa7 = 0;
    uint32_t fR_705fb8 = 0;
    uint32_t fA_705fc4 = 0;
    uint32_t fB_705fc4 = 0;
    uint32_t fA_705fce = 0;
    uint32_t fB_705fce = 0;
    uint32_t fA_705fde = 0;
    uint32_t fB_705fde = 0;
    uint32_t fR_705ff2 = 0;
    uint32_t fA_705fff = 0;
    uint32_t fB_705fff = 0;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esp + 0x14u);
    eax = rd32(m, edi + 0x18u);
    esi = rd32(m, eax + 0x4u);
    fA_705f82 = rd8(m, esi + 0x2du);
    fB_705f82 = 0x0u;
    wr32(m, esp + 0x1cu, eax);
    ebx = eax;
    if (fA_705f82 != fB_705f82) goto L_705fce;
    edi = edi;
L_705f90:
    edi = (esi + 0xcu);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(rd32(m, 0x9161bcu) /* 8.280267020501487e-39 */, 0x705f9bu);
    esp = esp + 0x8u;
    fR_705f9e = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_705f9e == 0)) goto L_705fa7;
    esi = rd32(m, esi + 0x8u);
    goto L_705fc4;
L_705fa7:
    fA_705fa7 = rd8(m, ebx + 0x2du);
    fB_705fa7 = 0x0u;
    if (fA_705fa7 == fB_705fa7) goto L_705fbe;
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, ebp);
    LIFT_CALL(rd32(m, 0x9161bcu) /* 8.280267020501487e-39 */, 0x705fb5u);
    esp = esp + 0x8u;
    fR_705fb8 = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_705fb8 == 0)) goto L_705fbe;
    ebx = esi;
L_705fbe:
    wr32(m, esp + 0x1cu, esi);
    esi = rd32(m, esi);
L_705fc4:
    fA_705fc4 = rd8(m, esi + 0x2du);
    fB_705fc4 = 0x0u;
    if (fA_705fc4 == fB_705fc4) goto L_705f90;
    edi = rd32(m, esp + 0x14u);
L_705fce:
    fA_705fce = rd8(m, ebx + 0x2du);
    fB_705fce = 0x0u;
    if (fA_705fce == fB_705fce) goto L_705fdc;
    eax = rd32(m, edi + 0x18u);
    esi = rd32(m, eax + 0x4u);
    goto L_705fde;
L_705fdc:
    esi = rd32(m, ebx);
L_705fde:
    fA_705fde = rd8(m, esi + 0x2du);
    fB_705fde = 0x0u;
    if (fA_705fde != fB_705fde) goto L_706005;
L_705fe4:
    ecx = (esi + 0xcu);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebp);
    LIFT_CALL(rd32(m, 0x9161bcu) /* 8.280267020501487e-39 */, 0x705fefu);
    esp = esp + 0x8u;
    fR_705ff2 = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_705ff2 == 0)) goto L_705ffc;
    ebx = esi;
    esi = rd32(m, esi);
    goto L_705fff;
L_705ffc:
    esi = rd32(m, esi + 0x8u);
L_705fff:
    fA_705fff = rd8(m, esi + 0x2du);
    fB_705fff = 0x0u;
    if (fA_705fff == fB_705fff) goto L_705fe4;
L_706005:
    eax = rd32(m, edi);
    ecx = rd32(m, esp + 0x18u);
    edx = rd32(m, esp + 0x1cu);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    wr32(m, ecx, eax);
    wr32(m, ecx + 0x8u, eax);
    wr32(m, ecx + 0xcu, ebx);
    wr32(m, ecx + 0x4u, edx);
    eax = ecx;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(12);
}

// 00706600 Fn_00706600
//   original FUN_00706600; register inputs: esi; returns with ret 0
REFLEX_PORT_FN(0x00706600, Fn_00706600, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_706609 = 0;
    uint32_t fR_706611 = 0;
    uint32_t fA_70661e = 0;
    uint32_t fB_70661e = 0;
    eax = rd32(m, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    fR_706609 = (eax & eax);
    if (!(fR_706609 == 0)) goto L_706615;
    LIFT_CALL(edi, 0x70660fu);
    eax = rd32(m, esi);
    fR_706611 = (eax & eax);
    if ((fR_706611 == 0)) goto L_706619;
L_706615:
    eax = rd32(m, eax);
    goto L_70661b;
L_706619:
    eax = 0u;
L_70661b:
    ecx = rd32(m, esi + 0x4u);
    fA_70661e = ecx;
    fB_70661e = rd32(m, eax + 0x14u);
    if (fA_70661e != fB_70661e) goto L_706625;
    LIFT_CALL(edi, 0x706625u);
L_706625:
    edx = rd32(m, esi + 0x4u);
    eax = rd32(m, edx);
    wr32(m, esi + 0x4u, eax);
    eax = esi;
    edi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00709390 Fn_00709390
//   original FUN_00709390; register inputs: edi; returns with ret 0
REFLEX_PORT_FN(0x00709390, Fn_00709390, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_7093b0 = 0;
    uint32_t fA_7093b4 = 0;
    uint32_t fB_7093b4 = 0;
    uint32_t fA_7093be = 0;
    uint32_t fB_7093be = 0;
    esp = esp - 0x10u;
    eax = rd32(m, esp + 0x14u);
    ecx = rd32(m, esp + 0x18u);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x20u);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x28u);
    esp -= 4; wr32(m, esp, esi);
    wr32(m, esp + 0x10u, eax);
    wr32(m, esp + 0x14u, ecx);
    edi = edi;
L_7093b0:
    fR_7093b0 = (eax & eax);
    if ((fR_7093b0 == 0)) goto L_7093b8;
    fA_7093b4 = eax;
    fB_7093b4 = ebx;
    if (fA_7093b4 == fB_7093b4) goto L_7093be;
L_7093b8:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7093beu);
L_7093be:
    fA_7093be = rd32(m, esp + 0x14u);
    fB_7093be = ebp;
    if (fA_7093be == fB_7093be) goto L_7093d5;
    wr32(m, edi, (rd32(m, edi) + 1u));
    esi = (esp + 0x10u);
    LIFT_CALL(0x593dc0u, 0x7093cfu);  // FUN_00593dc0
    eax = rd32(m, esp + 0x10u);
    goto L_7093b0;
L_7093d5:
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x10u;
    LIFT_RET(0);
}

// 00760330 Fn_00760330
//   original FUN_00760330; register inputs: eax; returns with ret 0
REFLEX_PORT_FN(0x00760330, Fn_00760330, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_760339 = 0;
    uint32_t fB_760339 = 0;
    uint32_t fA_76034d = 0;
    uint32_t fB_76034d = 0;
    uint32_t fR_76035d = 0;
    uint32_t fA_760368 = 0;
    uint32_t fB_760368 = 0;
    uint32_t fA_760371 = 0;
    uint32_t fB_760371 = 0;
    uint32_t fA_760376 = 0;
    uint32_t fB_760376 = 0;
    uint32_t fA_76037b = 0;
    uint32_t fB_76037b = 0;
    uint32_t fA_760380 = 0;
    uint32_t fB_760380 = 0;
    uint32_t fA_760385 = 0;
    uint32_t fB_760385 = 0;
    uint32_t fA_76038f = 0;
    uint32_t fB_76038f = 0;
    uint32_t fR_760399 = 0;
    uint32_t fR_7603a5 = 0;
    uint32_t FL = 0;
    uint32_t fA_7603b8 = 0;
    uint32_t fB_7603b8 = 0;
    uint32_t fR_7603bd = 0;
    uint32_t fA_7603c1 = 0;
    uint32_t fB_7603c1 = 0;
    uint32_t fA_7603c6 = 0;
    uint32_t fB_7603c6 = 0;
    uint32_t fA_7603cb = 0;
    uint32_t fB_7603cb = 0;
    uint32_t fA_7603d8 = 0;
    uint32_t fB_7603d8 = 0;
    uint32_t fR_7603dc = 0;
    uint32_t fA_7603e8 = 0;
    uint32_t fB_7603e8 = 0;
    ecx = rd32(m, 0xa95844u);
    edx = rd32(m, ecx + 0x20u);
    fA_760339 = rd32(m, edx + 0x34u);
    fB_760339 = 0x1u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    if (fA_760339 != fB_760339) goto L_76034a;
    ebx = rd32(m, eax + 0x5a54u);
    goto L_76035d;
L_76034a:
    ecx = rd32(m, ecx + 0x1cu);
    fA_76034d = rd32(m, ecx + 0x34u);
    fB_76034d = 0x1u;
    if (fA_76034d != fB_76034d) goto L_7603f5;
    ebx = rd32(m, eax + 0x20c8u);
L_76035d:
    fR_76035d = (ebx & ebx);
    if ((fR_76035d == 0)) goto L_7603f5;
    eax = rd32(m, eax + 0x18u);
    fA_760368 = eax;
    fB_760368 = 0x3u;
    if (fA_760368 == fB_760368) goto L_7603f5;
    fA_760371 = eax;
    fB_760371 = 0x4u;
    if (fA_760371 == fB_760371) goto L_7603f5;
    fA_760376 = eax;
    fB_760376 = 0x5u;
    if (fA_760376 == fB_760376) goto L_7603f5;
    fA_76037b = eax;
    fB_76037b = 0x7u;
    if (fA_76037b == fB_76037b) goto L_7603f5;
    fA_760380 = eax;
    fB_760380 = 0x8u;
    if (fA_760380 == fB_760380) goto L_7603f5;
    fA_760385 = eax;
    fB_760385 = 0x6u;
    if (fA_760385 != fB_760385) goto L_76039d;
    LIFT_CALL(0x75ec80u, 0x76038fu);  // MXGame_PlayerMode
    fA_76038f = eax;
    fB_76038f = 0x1u;
    if (fA_76038f == fB_76038f) goto L_7603f5;
    LIFT_CALL(0x75ec80u, 0x760399u);  // MXGame_PlayerMode
    fR_760399 = (eax & eax);
    if ((fR_760399 == 0)) goto L_7603f5;
L_76039d:
    edi = rd32(m, ebx + 0x21da8u);
    esi = 0u;
    fR_7603a5 = (edi & edi);
    FL = fl_logic(fR_7603a5, 4);
    if (((fR_7603a5 == 0) || sgn(fR_7603a5, 4) != false)) goto L_7603de;
    edx = rd32(m, ebx + 0x21da4u);
L_7603b0:
    ecx = rd32(m, edx);
    eax = rd32(m, ecx + 0x21cu);
    fA_7603b8 = eax;
    fB_7603b8 = 0x5u;
    if (fA_7603b8 == fB_7603b8) goto L_7603cb;
    fR_7603bd = (eax & eax);
    if ((fR_7603bd == 0)) goto L_7603cb;
    fA_7603c1 = eax;
    fB_7603c1 = 0x3u;
    if (fA_7603c1 == fB_7603c1) goto L_7603cb;
    fA_7603c6 = eax;
    fB_7603c6 = 0x4u;
    if (fA_7603c6 != fB_7603c6) goto L_7603d4;
L_7603cb:
    fA_7603cb = rd32(m, ecx + 0x238u);
    fB_7603cb = 0x0u;
    if (fA_7603cb != fB_7603cb) goto L_7603f5;
L_7603d4:
    esi = (esi + 1u);
    edx = edx + 0x4u;
    fA_7603d8 = esi;
    fB_7603d8 = edi;
    if (int32_t(sext(fA_7603d8, 4)) < int32_t(sext(fB_7603d8, 4))) goto L_7603b0;
    fR_7603dc = (edi & edi);
    FL = fl_logic(fR_7603dc, 4);
L_7603de:
    if (fl_cc(4, FL)) goto L_7603f5;
    edx = rd32(m, 0xa946b8u);
    eax = 0u;
    fA_7603e8 = rd8(m, edx + 0x15b20u);
    fB_7603e8 = (eax & 0xffu);
    eax = (eax & 0xffffff00u) | (((fA_7603e8 == fB_7603e8) ? 1u : 0u) & 0xffu);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_7603f5:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 007607d0 BehaviorMonitorCorner__vf5
//   original BehaviorMonitorCorner::vf5; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x007607d0, BehaviorMonitorCorner__vf5, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = 0x12u;
    LIFT_RET(0);
}

// 00763270 Fn_00763270
//   original FUN_00763270; register inputs: edi; returns with ret 8
REFLEX_PORT_FN(0x00763270, Fn_00763270, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_763279 = 0;
    uint32_t fB_763279 = 0;
    uint32_t fA_763290 = 0;
    uint32_t fB_763290 = 0;
    uint32_t fA_76329e = 0;
    uint32_t fB_76329e = 0;
    uint32_t fR_7632b1 = 0;
    uint32_t fA_7632b5 = 0;
    uint32_t fB_7632b5 = 0;
    uint32_t fA_7632bf = 0;
    uint32_t fB_7632bf = 0;
    uint32_t fA_7632c6 = 0;
    uint32_t fB_7632c6 = 0;
    ecx = rd32(m, edi + 0x18u);
    eax = rd32(m, ecx + 0x4u);
    esp = esp - 0x18u;
    fA_763279 = rd8(m, eax + 0x1du);
    fB_763279 = 0x0u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x28u);
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    if (fA_763279 != fB_763279) goto L_7632a4;
    ecx = rd32(m, ebp);
    goto L_763290;
L_763290:
    fA_763290 = rd32(m, eax + 0xcu);
    fB_763290 = ecx;
    if (int32_t(sext(fA_763290, 4)) >= int32_t(sext(fB_763290, 4))) goto L_76329a;
    eax = rd32(m, eax + 0x8u);
    goto L_76329e;
L_76329a:
    esi = eax;
    eax = rd32(m, eax);
L_76329e:
    fA_76329e = rd8(m, eax + 0x1du);
    fB_76329e = 0x0u;
    if (fA_76329e == fB_76329e) goto L_763290;
L_7632a4:
    eax = rd32(m, edi);
    ebx = rd32(m, edi + 0x18u);
    wr32(m, esp + 0x14u, esi);
    wr32(m, esp + 0x10u, eax);
    fR_7632b1 = (eax & eax);
    if ((fR_7632b1 == 0)) goto L_7632b9;
    fA_7632b5 = eax;
    fB_7632b5 = eax;
    if (fA_7632b5 == fB_7632b5) goto L_7632bf;
L_7632b9:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7632bfu);
L_7632bf:
    fA_7632bf = esi;
    fB_7632bf = ebx;
    if (fA_7632bf == fB_7632bf) goto L_7632d1;
    eax = rd32(m, ebp);
    fA_7632c6 = eax;
    fB_7632c6 = rd32(m, esi + 0xcu);
    if (int32_t(sext(fA_7632c6, 4)) < int32_t(sext(fB_7632c6, 4))) goto L_7632d1;
    eax = (esp + 0x10u);
    goto L_7632e2;
L_7632d1:
    ecx = rd32(m, edi + 0x18u);
    edx = rd32(m, edi);
    wr32(m, esp + 0x1cu, ecx);
    wr32(m, esp + 0x18u, edx);
    eax = (esp + 0x18u);
L_7632e2:
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

// 00764b90 MXRecords_LongestWheelie
//   original MXRecords_LongestWheelie; register inputs: ecx eax; returns with ret 4
REFLEX_PORT_FN(0x00764b90, MXRecords_LongestWheelie, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_764b98 = 0;
    uint32_t fB_764b98 = 0;
    uint32_t fA_764ba7 = 0;
    uint32_t fB_764ba7 = 0;
    uint32_t fA_764bc4 = 0;
    uint32_t fB_764bc4 = 0;
    uint32_t fA_764bde = 0;
    uint32_t fB_764bde = 0;
    uint32_t fA_764bee = 0;
    uint32_t fB_764bee = 0;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    fA_764b98 = eax;
    fB_764b98 = 0xffffffffu;
    if (fA_764b98 == fB_764b98) goto L_764c16;
    eax = (eax + eax * 2);
    ebx = rd32(m, ecx + eax * 8 + 0x4u);
    eax = (ecx + eax * 8);
    fA_764ba7 = ebx;
    fB_764ba7 = 0xffffffffu;
    if (fA_764ba7 == fB_764ba7) goto L_764c16;
    esi = rd32(m, eax);
    esp -= 4; wr32(m, esp, 0x96daccu);
    eax = esi;
    LIFT_CALL(0x402160u, 0x764bbau);  // Table_ColumnIndex
    edi = eax;
    esp -= 4; wr32(m, esp, ebx);
    eax = esi;
    LIFT_CALL(0x41aaf0u, 0x764bc4u);  // FUN_0041aaf0
    fA_764bc4 = ebp;
    fB_764bc4 = rd32(m, eax);
    if (int32_t(sext(fA_764bc4, 4)) <= int32_t(sext(fB_764bc4, 4))) goto L_764c16;
    esp -= 4; wr32(m, esp, 0x96daccu);
    eax = esi;
    LIFT_CALL(0x402160u, 0x764bd4u);  // Table_ColumnIndex
    edi = eax;
    esp -= 4; wr32(m, esp, ebx);
    eax = esi;
    LIFT_CALL(0x41aaf0u, 0x764bdeu);  // FUN_0041aaf0
    fA_764bde = ebp;
    fB_764bde = rd32(m, eax);
    if (fA_764bde == fB_764bde) goto L_764be5;
    wr32(m, esi + 0x68u, (rd32(m, esi + 0x68u) + 1u));
L_764be5:
    edx = rd32(m, esi + 0x14u);
    edx = edx - rd32(m, esi + 0x10u);
    edx = uint32_t(int32_t(edx) >> 2);
    fA_764bee = edi;
    fB_764bee = edx;
    if (fA_764bee < fB_764bee) goto L_764bf8;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x764bf8u);
L_764bf8:
    ecx = rd32(m, esi + 0x10u);
    edx = rd32(m, ecx + edi * 4);
    eax = (esp + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x4f5330u, 0x764c0au);  // FUN_004f5330
    eax = 0x1u;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_764c16:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00764ef0 MXRecords_HoleshotWon
//   original MXRecords_HoleshotWon; register inputs: ecx eax; returns with ret 0
REFLEX_PORT_FN(0x00764ef0, MXRecords_HoleshotWon, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_764ef4 = 0;
    uint32_t fB_764ef4 = 0;
    uint32_t fA_764f02 = 0;
    uint32_t fB_764f02 = 0;
    esp = esp - 0x10u;
    esp -= 4; wr32(m, esp, esi);
    fA_764ef4 = eax;
    fB_764ef4 = 0xffffffffu;
    if (fA_764ef4 == fB_764ef4) goto L_764f34;
    eax = (eax + eax * 2);
    esi = (ecx + eax * 8);
    eax = rd32(m, esi + 0x4u);
    fA_764f02 = eax;
    fB_764f02 = 0xffffffffu;
    if (fA_764f02 == fB_764f02) goto L_764f34;
    edx = rd32(m, esi);
    esp -= 4; wr32(m, esp, 0x96da08u);
    wr32(m, esp + 0xcu, eax);
    eax = (esp + 0xcu);
    esp -= 4; wr32(m, esp, 0x1u);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0x18u, 0x0u);
    wr32(m, esp + 0x1cu, edx);
    LIFT_CALL(0x764fc0u, 0x764f2au);  // MXRecords_AddInt
    esp = esp + 0xcu;
    wr32(m, esi + 0x14u, 0x1u);
L_764f34:
    esi = rd32(m, esp); esp += 4;
    esp = esp + 0x10u;
    LIFT_RET(0);
}

// 00764f40 MXRecords_AttemptedJump
//   original MXRecords_AttemptedJump; register inputs: ecx eax; returns with ret 0
REFLEX_PORT_FN(0x00764f40, MXRecords_AttemptedJump, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_764f43 = 0;
    uint32_t fB_764f43 = 0;
    uint32_t fA_764f51 = 0;
    uint32_t fB_764f51 = 0;
    esp = esp - 0xcu;
    fA_764f43 = eax;
    fB_764f43 = 0xffffffffu;
    if (fA_764f43 == fB_764f43) goto L_764f7c;
    eax = (eax + eax * 2);
    eax = (ecx + eax * 8);
    ecx = rd32(m, eax + 0x4u);
    fA_764f51 = ecx;
    fB_764f51 = 0xffffffffu;
    if (fA_764f51 == fB_764f51) goto L_764f7c;
    edx = rd32(m, eax);
    esp -= 4; wr32(m, esp, 0x96d9b8u);
    eax = (esp + 0x4u);
    esp -= 4; wr32(m, esp, 0x1u);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0xcu, ecx);
    wr32(m, esp + 0x10u, 0x0u);
    wr32(m, esp + 0x14u, edx);
    LIFT_CALL(0x764fc0u, 0x764f79u);  // MXRecords_AddInt
    esp = esp + 0xcu;
L_764f7c:
    esp = esp + 0xcu;
    LIFT_RET(0);
}

// 00764fc0 MXRecords_AddInt
//   original MXRecords_AddInt; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00764fc0, MXRecords_AddInt, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_764ff0 = 0;
    uint32_t fB_764ff0 = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, ebx + 0x8u);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, ebx);
    esp -= 4; wr32(m, esp, ebp);
    eax = esi;
    LIFT_CALL(0x402160u, 0x764fd9u);  // Table_ColumnIndex
    esp -= 4; wr32(m, esp, edi);
    edi = eax;
    eax = esi;
    LIFT_CALL(0x41aaf0u, 0x764fe3u);  // FUN_0041aaf0
    ecx = rd32(m, eax);
    edx = rd32(m, esp + 0x18u);
    eax = 0x7fffffffu;
    eax = eax - ecx;
    fA_764ff0 = edx;
    fB_764ff0 = eax;
    if (int32_t(sext(fA_764ff0, 4)) >= int32_t(sext(fB_764ff0, 4))) goto L_764ff6;
    eax = edx;
L_764ff6:
    esi = rd32(m, ebx + 0x8u);
    eax = eax + ecx;
    ecx = rd32(m, ebx);
    wr32(m, esp + 0x14u, eax);
    eax = (esp + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    eax = ebp;
    LIFT_CALL(0x4f6e60u, 0x76500eu);  // Table_SetCellInt
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00766ec0 Fn_00766ec0
//   original FUN_00766ec0; register inputs: none; returns with ret 4
REFLEX_PORT_FN(0x00766ec0, Fn_00766ec0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_766ed9 = 0;
    uint32_t fB_766ed9 = 0;
    uint32_t fR_766efd = 0;
    uint32_t fA_766f12 = 0;
    uint32_t fB_766f12 = 0;
    uint32_t fA_766f18 = 0;
    uint32_t fB_766f18 = 0;
    uint32_t fA_766f24 = 0;
    uint32_t fB_766f24 = 0;
    uint32_t fA_766f2c = 0;
    uint32_t fB_766f2c = 0;
    uint32_t fA_766f36 = 0;
    uint32_t fB_766f36 = 0;
    uint32_t fA_766f3f = 0;
    uint32_t fB_766f3f = 0;
    uint32_t fA_766f48 = 0;
    uint32_t fB_766f48 = 0;
    uint32_t fA_766f51 = 0;
    uint32_t fB_766f51 = 0;
    uint32_t fA_766f62 = 0;
    uint32_t fB_766f62 = 0;
    uint32_t fA_766f6b = 0;
    uint32_t fB_766f6b = 0;
    uint32_t fR_766f7e = 0;
    uint32_t fA_766f89 = 0;
    uint32_t fB_766f89 = 0;
    uint32_t fA_766fc6 = 0;
    uint32_t fB_766fc6 = 0;
    uint32_t fA_766fd1 = 0;
    uint32_t fB_766fd1 = 0;
    uint32_t fA_766fe2 = 0;
    uint32_t fB_766fe2 = 0;
    uint32_t fA_767009 = 0;
    uint32_t fB_767009 = 0;
    uint32_t fR_767023 = 0;
    uint32_t fA_76703c = 0;
    uint32_t fB_76703c = 0;
    uint32_t fA_767045 = 0;
    uint32_t fB_767045 = 0;
    uint32_t fA_76704e = 0;
    uint32_t fB_76704e = 0;
    uint32_t fR_767062 = 0;
    uint32_t fA_767078 = 0;
    uint32_t fB_767078 = 0;
    uint32_t fA_767087 = 0;
    uint32_t fB_767087 = 0;
    uint32_t fA_767093 = 0;
    uint32_t fB_767093 = 0;
    uint32_t fA_7670b7 = 0;
    uint32_t fB_7670b7 = 0;
    uint32_t fR_7670d1 = 0;
    uint32_t fR_7670f4 = 0;
    uint32_t fA_7670f8 = 0;
    uint32_t fB_7670f8 = 0;
    uint32_t fA_767104 = 0;
    uint32_t fB_767104 = 0;
    uint32_t fA_76710c = 0;
    uint32_t fB_76710c = 0;
    uint32_t fA_767116 = 0;
    uint32_t fB_767116 = 0;
    uint32_t fA_76711f = 0;
    uint32_t fB_76711f = 0;
    uint32_t fA_767128 = 0;
    uint32_t fB_767128 = 0;
    uint32_t fA_767131 = 0;
    uint32_t fB_767131 = 0;
    uint32_t fA_76713a = 0;
    uint32_t fB_76713a = 0;
    uint32_t fA_767156 = 0;
    uint32_t fB_767156 = 0;
    uint32_t fA_76715f = 0;
    uint32_t fB_76715f = 0;
    uint32_t fA_767168 = 0;
    uint32_t fB_767168 = 0;
    uint32_t fA_76717b = 0;
    uint32_t fB_76717b = 0;
    uint32_t fA_767194 = 0;
    uint32_t fB_767194 = 0;
    uint32_t fA_7671a5 = 0;
    uint32_t fB_7671a5 = 0;
    uint32_t fR_7671ba = 0;
    uint32_t fA_7671cd = 0;
    uint32_t fB_7671cd = 0;
    uint32_t fA_7671e6 = 0;
    uint32_t fB_7671e6 = 0;
    uint32_t fA_7671f3 = 0;
    uint32_t fB_7671f3 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0xcu;
    edx = rd32(m, 0xa946b8u);
    eax = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, ebx);
    edx = edx + 0x7460u;
    fA_766ed9 = rd32(m, eax + 0x20018u);
    fB_766ed9 = 0x0u;
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    wr32(m, esp + 0x10u, edx);
    if (fA_766ed9 != fB_766ed9) goto L_7670c4;
    ecx = rd8(m, edx + 0xdfbcu);
    eax = 0u;
    wr32(m, esp + 0x14u, ecx);
    wr32(m, esp + 0xcu, eax);
    fR_766efd = (ecx & ecx);
    if ((false || (fR_766efd == 0))) goto L_767013;
L_766f05:
    esp -= 4; wr32(m, esp, 0x3u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x552b90u, 0x766f0du);  // FUN_00552b90
    ebx = rd32(m, ebp + 0x8u);
    esi = eax;
    fA_766f12 = rd32(m, esi + 0x10u);
    fB_766f12 = 0x0u;
    if (fA_766f12 == fB_766f12) goto L_766f36;
    fA_766f18 = rd32(m, ebx + 0x20018u);
    fB_766f18 = 0x0u;
    if (fA_766f18 == fB_766f18) goto L_766f36;
    eax = (eax | 0xffffffffu);
    fA_766f24 = rd32(m, esi + 0x1c4u);
    fB_766f24 = eax;
    if (int32_t(sext(fA_766f24, 4)) > int32_t(sext(fB_766f24, 4))) goto L_766f5a;
    fA_766f2c = rd32(m, esi + 0x1c0u);
    fB_766f2c = eax;
    if (int32_t(sext(fA_766f2c, 4)) <= int32_t(sext(fB_766f2c, 4))) goto L_766f6b;
    goto L_766f5a;
L_766f36:
    fA_766f36 = rd32(m, esi + 0x1c4u);
    fB_766f36 = 0xffffffffu;
    if (int32_t(sext(fA_766f36, 4)) > int32_t(sext(fB_766f36, 4))) goto L_766f5a;
    fA_766f3f = rd32(m, esi + 0x1c0u);
    fB_766f3f = 0xffffffffu;
    if (int32_t(sext(fA_766f3f, 4)) > int32_t(sext(fB_766f3f, 4))) goto L_766f5a;
    fA_766f48 = rd32(m, esi + 0x1acu);
    fB_766f48 = 0x0u;
    if (fA_766f48 == fB_766f48) goto L_766f5a;
    fA_766f51 = rd32(m, esi + 0x1b4u);
    fB_766f51 = 0x0u;
    if (fA_766f51 != fB_766f51) goto L_766f6b;
L_766f5a:
    esp -= 4; wr32(m, esp, ebx);
    eax = esi;
    LIFT_CALL(0x767200u, 0x766f62u);  // PlayerSlot_SpawnStep
    fA_766f62 = eax;
    fB_766f62 = 0x4u;
    if (fA_766f62 != fB_766f62) goto L_7671f7;
L_766f6b:
    fA_766f6b = rd32(m, esi + 0x1bcu);
    fB_766f6b = 0x3du;
    if (fA_766f6b == fB_766f6b) goto L_766ffc;
    edi = rd32(m, esi + 0x94u);
    fR_766f7e = (edi & edi);
    if ((fR_766f7e == 0)) goto L_766fad;
    ecx = edi;
    LIFT_CALL(0x55a050u, 0x766f89u);  // FUN_0055a050
    fA_766f89 = rd32(m, esi + 0x908u);
    fB_766f89 = 0x0u;
    if (fA_766f89 == fB_766f89) goto L_766f9d;
    ecx = rd32(m, edi + 0x27cu);
    eax = rd32(m, ecx + eax * 4);
    goto L_766fa6;
L_766f9d:
    edx = rd32(m, edi + 0x268u);
    eax = rd32(m, edx + eax * 4);
L_766fa6:
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x55c1d0u, 0x766fadu);  // Activity_OnPlayerLeft
L_766fad:
    eax = sext(rd8(m, esi + 0x1c8u), 1);
    edi = rd32(m, esi + 0x1bcu);
    esp -= 4; wr32(m, esp, eax);
    eax = (ebx + 0x21da0u);
    LIFT_CALL(0x55f630u, 0x766fc6u);  // FUN_0055f630
    fA_766fc6 = rd32(m, esi + 0x1acu);
    fB_766fc6 = 0x0u;
    edi = eax;
    if (fA_766fc6 == fB_766fc6) goto L_766ffc;
    fA_766fd1 = rd32(m, esi + 0x1b4u);
    fB_766fd1 = 0x0u;
    if (fA_766fd1 == fB_766fd1) goto L_766ffc;
    esp -= 4; wr32(m, esp, edi);
    ecx = esi;
    LIFT_CALL(0x55b610u, 0x766fe2u);  // Activity_OnPlayerJoined
    fA_766fe2 = rd32(m, edi + 0x21cu);
    fB_766fe2 = 0x5u;
    if (fA_766fe2 != fB_766fe2) goto L_766ff2;
    wr32(m, edi + 0x10u, 0x2u);
L_766ff2:
    wr32(m, esi + 0x1bcu, 0x3du);
L_766ffc:
    eax = rd32(m, esp + 0xcu);
    edx = rd32(m, esp + 0x10u);
    eax = (eax + 1u);
    wr32(m, esp + 0xcu, eax);
    fA_767009 = eax;
    fB_767009 = rd32(m, esp + 0x14u);
    if (fA_767009 < fB_767009) goto L_766f05;
L_767013:
    ebx = 0u;
    edi = (ebx + 0x3u);
    esi = edx;
    wr32(m, esp + 0xcu, ebx);
    LIFT_CALL(0x552ce0u, 0x767023u);  // FUN_00552ce0
    fR_767023 = (eax & eax);
    if ((false || (fR_767023 == 0))) goto L_7671af;
    goto L_767030;
L_767030:
    edx = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, 0x3u);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x552b90u, 0x76703cu);  // FUN_00552b90
    fA_76703c = rd32(m, eax + 0x1bcu);
    fB_76703c = 0x3du;
    if (fA_76703c == fB_76703c) goto L_7670a4;
    fA_767045 = rd32(m, eax + 0x1acu);
    fB_767045 = 0x0u;
    if (fA_767045 == fB_767045) goto L_767057;
    fA_76704e = rd32(m, eax + 0x1b4u);
    fB_76704e = 0x0u;
    if (fA_76704e != fB_76704e) goto L_7670a4;
L_767057:
    edx = rd32(m, ebp + 0x8u);
    edi = rd32(m, edx + 0x21da8u);
    ecx = 0u;
    fR_767062 = (edi & edi);
    if (((fR_767062 == 0) || sgn(fR_767062, 4) != false)) goto L_767097;
    esi = rd32(m, edx + 0x21da4u);
L_767070:
    edx = rd32(m, esi);
    ebx = rd32(m, eax + 0x1bcu);
    fA_767078 = ebx;
    fB_767078 = rd32(m, edx + 0x23cu);
    if (fA_767078 != fB_767078) goto L_76708f;
    ebx = sext(rd8(m, eax + 0x1c8u), 1);
    fA_767087 = ebx;
    fB_767087 = rd32(m, edx + 0x220u);
    if (fA_767087 == fB_767087) goto L_767099;
L_76708f:
    ecx = (ecx + 1u);
    esi = esi + 0x4u;
    fA_767093 = ecx;
    fB_767093 = edi;
    if (int32_t(sext(fA_767093, 4)) < int32_t(sext(fB_767093, 4))) goto L_767070;
L_767097:
    edx = 0u;
L_767099:
    ebx = rd32(m, esp + 0xcu);
    wr32(m, edx + 0x10u, 0x1u);
L_7670a4:
    esi = rd32(m, esp + 0x10u);
    ebx = (ebx + 1u);
    edi = 0x3u;
    wr32(m, esp + 0xcu, ebx);
    LIFT_CALL(0x552ce0u, 0x7670b7u);  // FUN_00552ce0
    fA_7670b7 = ebx;
    fB_7670b7 = eax;
    if (fA_7670b7 < fB_7670b7) goto L_767030;
    goto L_7671af;
L_7670c4:
    eax = rd8(m, edx + 0xdfbcu);
    ebx = 0u;
    wr32(m, esp + 0x14u, eax);
    fR_7670d1 = (eax & eax);
    if ((false || (fR_7670d1 == 0))) goto L_7671af;
    goto L_7670e4;
L_7670e0:
    edx = rd32(m, esp + 0x10u);
L_7670e4:
    esp -= 4; wr32(m, esp, 0x3u);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x552b90u, 0x7670ecu);  // FUN_00552b90
    edi = rd32(m, ebp + 0x8u);
    esi = eax;
    eax = rd32(m, esi + 0x10u);
    fR_7670f4 = (eax & eax);
    if ((fR_7670f4 == 0)) goto L_767116;
    fA_7670f8 = rd32(m, edi + 0x20018u);
    fB_7670f8 = 0x0u;
    if (fA_7670f8 == fB_7670f8) goto L_767116;
    ecx = (ecx | 0xffffffffu);
    fA_767104 = rd32(m, esi + 0x1c4u);
    fB_767104 = ecx;
    if (int32_t(sext(fA_767104, 4)) > int32_t(sext(fB_767104, 4))) goto L_76713a;
    fA_76710c = rd32(m, esi + 0x1c0u);
    fB_76710c = ecx;
    if (int32_t(sext(fA_76710c, 4)) <= int32_t(sext(fB_76710c, 4))) goto L_76715f;
    goto L_76713a;
L_767116:
    fA_767116 = rd32(m, esi + 0x1c4u);
    fB_767116 = 0xffffffffu;
    if (int32_t(sext(fA_767116, 4)) > int32_t(sext(fB_767116, 4))) goto L_76713a;
    fA_76711f = rd32(m, esi + 0x1c0u);
    fB_76711f = 0xffffffffu;
    if (int32_t(sext(fA_76711f, 4)) > int32_t(sext(fB_76711f, 4))) goto L_76713a;
    fA_767128 = rd32(m, esi + 0x1acu);
    fB_767128 = 0x0u;
    if (fA_767128 == fB_767128) goto L_76713a;
    fA_767131 = rd32(m, esi + 0x1b4u);
    fB_767131 = 0x0u;
    if (fA_767131 != fB_767131) goto L_76715f;
L_76713a:
    fA_76713a = eax;
    fB_76713a = 0x1u;
    esp -= 4; wr32(m, esp, edi);
    eax = esi;
    if (fA_76713a != fB_76713a) goto L_767151;
    LIFT_CALL(0x767200u, 0x767147u);  // PlayerSlot_SpawnStep
    esp -= 4; wr32(m, esp, edi);
    eax = esi;
    LIFT_CALL(0x767200u, 0x76714fu);  // PlayerSlot_SpawnStep
    goto L_76715f;
L_767151:
    LIFT_CALL(0x767200u, 0x767156u);  // PlayerSlot_SpawnStep
    fA_767156 = eax;
    fB_767156 = 0x4u;
    if (fA_767156 != fB_767156) goto L_7671f7;
L_76715f:
    fA_76715f = rd32(m, esi + 0x94u);
    fB_76715f = 0x0u;
    if (fA_76715f != fB_76715f) goto L_7671a4;
    fA_767168 = rd32(m, esi + 0x1acu);
    fB_767168 = 0x0u;
    wr32(m, esi + 0x9cu, 0x3eu);
    if (fA_767168 == fB_767168) goto L_7671a4;
    fA_76717b = rd32(m, esi + 0x1b4u);
    fB_76717b = 0x0u;
    if (fA_76717b == fB_76717b) goto L_7671a4;
    ecx = rd32(m, edi + 0x21da4u);
    edi = rd32(m, ecx);
    esp -= 4; wr32(m, esp, edi);
    ecx = esi;
    LIFT_CALL(0x55b610u, 0x767194u);  // Activity_OnPlayerJoined
    fA_767194 = rd32(m, edi + 0x21cu);
    fB_767194 = 0x5u;
    if (fA_767194 != fB_767194) goto L_7671a4;
    wr32(m, edi + 0x10u, 0x2u);
L_7671a4:
    ebx = (ebx + 1u);
    fA_7671a5 = ebx;
    fB_7671a5 = rd32(m, esp + 0x14u);
    if (fA_7671a5 < fB_7671a5) goto L_7670e0;
L_7671af:
    edx = rd32(m, ebp + 0x8u);
    ebx = rd32(m, edx + 0x21da8u);
    edi = 0u;
    fR_7671ba = (ebx & ebx);
    if ((false || (fR_7671ba == 0))) goto L_7671f7;
L_7671be:
    eax = rd32(m, ebp + 0x8u);
    ecx = rd32(m, eax + 0x21da4u);
    esi = rd32(m, ecx + edi * 4);
    eax = rd32(m, esi + 0x10u);
    fA_7671cd = eax;
    fB_7671cd = 0x2u;
    if (fA_7671cd != fB_7671cd) goto L_7671e6;
    edx = rd32(m, esi);
    eax = rd32(m, edx + 0x44u);
    ecx = esi;
    LIFT_CALL(eax, 0x7671dbu);
    edx = rd32(m, esi);
    eax = rd32(m, edx + 0x3cu);
    ecx = esi;
    LIFT_CALL(eax, 0x7671e4u);
    goto L_7671eb;
L_7671e6:
    fA_7671e6 = eax;
    fB_7671e6 = 0x1u;
    if (fA_7671e6 != fB_7671e6) goto L_7671f2;
L_7671eb:
    wr32(m, esi + 0x10u, 0x0u);
L_7671f2:
    edi = (edi + 1u);
    fA_7671f3 = edi;
    fB_7671f3 = ebx;
    if (fA_7671f3 < fB_7671f3) goto L_7671be;
L_7671f7:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 007674b0 MXWorld__vf5: MXWorld::vf5: the world update (World_PhysicsUpdate, then the rider animation jobs kicked and waited).
//   original MXWorld::vf5; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x007674b0, MXWorld__vf5, "engine_mt", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_7674c1 = 0;
    uint32_t fR_7674cf = 0;
    uint32_t fR_767501 = 0;
    FCmp fC_767536{};
    uint32_t fR_767549 = 0;
    uint32_t fA_767573 = 0;
    uint32_t fB_767573 = 0;
    uint32_t fA_767584 = 0;
    uint32_t fB_767584 = 0;
    uint32_t fR_7675a0 = 0;
    uint32_t fA_7675e9 = 0;
    uint32_t fB_7675e9 = 0;
    uint32_t fA_767643 = 0;
    uint32_t fB_767643 = 0;
    uint32_t fA_76764c = 0;
    uint32_t fB_76764c = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    ebx = 0x1u;
    esp -= 4; wr32(m, esp, edi);
    edi = ecx;
    fR_7674c1 = (rd8(m, 0xdcf748u) & (ebx & 0xffu));
    if (!(fR_7674c1 == 0)) goto L_767501;
    wr32(m, 0xdcf748u, (rd32(m, 0xdcf748u) | ebx));
    fR_7674cf = (rd8(m, 0xdcf304u) & (ebx & 0xffu));
    if (!(fR_7674cf == 0)) goto L_7674ef;
    wr32(m, 0xdcf304u, (rd32(m, 0xdcf304u) | ebx));
    LIFT_CALL(0x6de1f0u, 0x7674e2u);  // FUN_006de1f0
    esp -= 4; wr32(m, esp, 0x913e00u);
    LIFT_CALL(0x8f9fbfu, 0x7674ecu);  // _atexit
    esp = esp + 0x4u;
L_7674ef:
    eax = 0xda6ca0u;
    LIFT_CALL(0x6def20u, 0x7674f9u);  // FUN_006def20
    wr32(m, 0xdcf744u, x0.l(0));
L_767501:
    fR_767501 = (rd8(m, 0xdcf304u) & (ebx & 0xffu));
    if (!(fR_767501 == 0)) goto L_767521;
    wr32(m, 0xdcf304u, (rd32(m, 0xdcf304u) | ebx));
    LIFT_CALL(0x6de1f0u, 0x767514u);  // FUN_006de1f0
    esp -= 4; wr32(m, esp, 0x913e00u);
    LIFT_CALL(0x8f9fbfu, 0x76751eu);  // _atexit
    esp = esp + 0x4u;
L_767521:
    eax = 0xda6ca0u;
    LIFT_CALL(0x6def20u, 0x76752bu);  // FUN_006def20
    x0 = subss(x0, rd32(m, 0xdcf744u));
    x1 = X{};
    fC_767536 = fcmp32(x0.l(0), x1.l(0));
    if ((fc_b(fC_767536) || fc_z(fC_767536))) goto L_767543;
    wr32(m, esp + 0xcu, x0.l(0));
    goto L_767549;
L_767543:
    wr32(m, esp + 0xcu, x1.l(0));
L_767549:
    fR_767549 = (rd8(m, 0xdcf304u) & (ebx & 0xffu));
    if (!(fR_767549 == 0)) goto L_767569;
    wr32(m, 0xdcf304u, (rd32(m, 0xdcf304u) | ebx));
    LIFT_CALL(0x6de1f0u, 0x76755cu);  // FUN_006de1f0
    esp -= 4; wr32(m, esp, 0x913e00u);
    LIFT_CALL(0x8f9fbfu, 0x767566u);  // _atexit
    esp = esp + 0x4u;
L_767569:
    eax = 0xda6ca0u;
    LIFT_CALL(0x6def20u, 0x767573u);  // FUN_006def20
    fA_767573 = rd32(m, edi + 0x20014u);
    fB_767573 = 0x0u;
    wr32(m, 0xdcf744u, x0.l(0));
    if (fA_767573 == fB_767573) goto L_7675c3;
    fA_767584 = rd32(m, edi + 0x21dfcu);
    fB_767584 = 0x0u;
    if (fA_767584 == fB_767584) goto L_7675b2;
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x8u)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x7678b0u, 0x76759au);  // ReplayCameras_Update
    ecx = rd32(m, edi + 0x21dfcu);
    fR_7675a0 = (ecx & ecx);
    if ((fR_7675a0 == 0)) goto L_7675b2;
    eax = rd32(m, ecx);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x8u)));
    edx = rd32(m, eax + 0x4cu);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(edx, 0x7675b2u);
L_7675b2:
    wr32(m, 0xa89bf0u, ebx);
    eax = ebx;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_7675c3:
    x0 = movss_m(rd32(m, ebp + 0x8u));
    x0 = cvtps2pd(x0.lo);
    x0 = addsd(x0, rd64(m, edi + 0x20020u));
    esp -= 4; wr32(m, esp, edi);
    wr64(m, edi + 0x20020u, x0.lo);
    LIFT_CALL(0x766ec0u, 0x7675e1u);  // FUN_00766ec0
    eax = rd32(m, 0xa95844u);
    ecx = rd32(m, eax + 0x10u);
    fA_7675e9 = rd32(m, ecx + 0xcu);
    fB_7675e9 = 0x0u;
    if (fA_7675e9 != fB_7675e9) goto L_767603;
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x8u)));
    eax = rd32(m, edi + 0x20134u);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x769b80u, 0x767601u);  // World_PhysicsUpdate
    goto L_767609;
L_767603:
    wr32(m, 0xa89bf0u, ebx);
L_767609:
    LIFT_CALL(0x61d060u, 0x76760eu);  // RiderAnimation_KickJobs
    LIFT_CALL(0x61d130u, 0x767613u);  // RiderAnimation_WaitJobs
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0xcu)));
    esp -= 4; wr32(m, esp, ecx);
    esi = (edi + 0x21da0u);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x55f700u, 0x767626u);  // FUN_0055f700
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x8u)));
    eax = rd32(m, 0xa946b8u);
    esp -= 4; wr32(m, esp, ecx);
    eax = eax + 0x7460u;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x552a10u, 0x76763cu);  // FUN_00552a10
    esi = edi;
    LIFT_CALL(0x767690u, 0x767643u);  // FUN_00767690
    fA_767643 = rd32(m, edi + 0x22130u);
    fB_767643 = 0x0u;
    if (fA_767643 == fB_767643) goto L_767660;
    fA_76764c = rd32(m, edi + 0x2210cu);
    fB_76764c = 0x0u;
    ecx = (edi + 0x21e10u);
    if (fA_76764c == fB_76764c) goto L_767660;
    LIFT_CALL(0x5399e0u, 0x767660u);  // FUN_005399e0
L_767660:
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x8u)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x7678b0u, 0x76766du);  // ReplayCameras_Update
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x8u)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x674e90u, 0x767679u);  // FUN_00674e90
    esp = esp + 0x4u;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    eax = ebx;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00767690 Fn_00767690
//   original FUN_00767690; register inputs: ecx esi; returns with ret 0
REFLEX_PORT_FN(0x00767690, Fn_00767690, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_767691 = 0;
    uint32_t fB_767691 = 0;
    uint32_t fR_7676a8 = 0;
    FCmp fC_7676b4{};
    uint32_t fA_7676c8 = 0;
    uint32_t fB_7676c8 = 0;
    esp -= 4; wr32(m, esp, ecx);
    fA_767691 = rd8(m, 0x9b716du);
    fB_767691 = 0x0u;
    if (fA_767691 != fB_767691) goto L_7676df;
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0x7u);
    ecx = 0xd5bee8u;
    LIFT_CALL(0x8115f0u, 0x7676a8u);  // KeyMap::vf2
    fR_7676a8 = (eax & eax);
    if ((fR_7676a8 == 0)) goto L_7676df;
    x0 = movsd_m(rd64(m, esi + 0x20020u));
    fC_7676b4 = fcmp64(x0.lo, rd64(m, esi + 0x20028u));
    if ((fc_b(fC_7676b4) || fc_z(fC_7676b4))) goto L_7676df;
    x0 = addsd(x0, rd64(m, 0x977638u) /* 1.0 */);
    eax = 0u;
    fA_7676c8 = rd32(m, esi + 0x21dc0u);
    fB_7676c8 = eax;
    wr64(m, esi + 0x20028u, x0.lo);
    eax = (eax & 0xffffff00u) | (((fA_7676c8 == fB_7676c8) ? 1u : 0u) & 0xffu);
    wr32(m, esi + 0x21dc0u, eax);
L_7676df:
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 007678b0 ReplayCameras_Update
//   original ReplayCameras_Update; register inputs: none; returns with ret 8
REFLEX_PORT_FN(0x007678b0, ReplayCameras_Update, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7678c4 = 0;
    uint32_t fB_7678c4 = 0;
    uint32_t fR_7678e7 = 0;
    uint32_t fA_7678eb = 0;
    uint32_t fB_7678eb = 0;
    uint32_t fR_7678fb = 0;
    uint32_t fA_767903 = 0;
    uint32_t fB_767903 = 0;
    uint32_t fA_767920 = 0;
    uint32_t fB_767920 = 0;
    uint32_t fR_767930 = 0;
    uint32_t fA_767948 = 0;
    uint32_t fB_767948 = 0;
    uint32_t fA_76794f = 0;
    uint32_t fB_76794f = 0;
    uint32_t fA_76795e = 0;
    uint32_t fB_76795e = 0;
    uint32_t fR_76796a = 0;
    uint32_t fR_767974 = 0;
    uint32_t fR_7679b9 = 0;
    uint32_t fA_7679bd = 0;
    uint32_t fB_7679bd = 0;
    FCmp fC_7679d7{};
    uint32_t fR_767a02 = 0;
    FCmp fC_767a1a{};
    uint32_t fR_767a34 = 0;
    uint32_t fA_767a3e = 0;
    uint32_t fB_767a3e = 0;
    uint32_t fR_767a42 = 0;
    uint32_t fR_767a53 = 0;
    uint32_t fR_767a7f = 0;
    uint32_t fA_767a83 = 0;
    uint32_t fB_767a83 = 0;
    uint32_t fR_767aab = 0;
    uint32_t fA_767aaf = 0;
    uint32_t fB_767aaf = 0;
    uint32_t fA_767acb = 0;
    uint32_t fB_767acb = 0;
    uint32_t fR_767ad8 = 0;
    uint32_t fA_767afc = 0;
    uint32_t fB_767afc = 0;
    uint32_t fR_767b0b = 0;
    uint32_t fR_767b22 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x14u;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp + 0x10u, eax);
    fA_7678c4 = rd8(m, 0x9b716du);
    fB_7678c4 = 0x0u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, ebx + 0x21de0u);
    esp -= 4; wr32(m, esp, edi);
    if (fA_7678c4 != fB_7678c4) goto L_7678fb;
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0x7u);
    ecx = 0xd5e270u;
    LIFT_CALL(0x811420u, 0x7678e7u);  // KeyMap::vf1
    fR_7678e7 = (eax & eax);
    if ((fR_7678e7 == 0)) goto L_7678fb;
    fA_7678eb = rd8(m, ebx + 0x21de5u);
    fB_7678eb = 0x0u;
    eax = (eax & 0xffffff00u) | (((fA_7678eb == fB_7678eb) ? 1u : 0u) & 0xffu);
    wr8(m, ebx + 0x21de5u, (eax & 0xffu));
L_7678fb:
    fR_7678fb = (esi & esi);
    if (!(fR_7678fb == 0)) goto L_7679af;
    fA_767903 = rd8(m, ebx + 0x21de5u);
    fB_767903 = 0x0u;
    if (fA_767903 == fB_767903) goto L_76796e;
    edx = rd32(m, 0xa946b8u);
    edx = edx + 0x7460u;
    ecx = (edx + 0xdf8cu);
    eax = 0u;
    fA_767920 = rd32(m, ecx);
    fB_767920 = 0xffffffffu;
    if (fA_767920 == fB_767920) goto L_767964;
    esi = rd8(m, edx + 0xdfbcu);
    wr32(m, esp + 0x10u, eax);
    fR_767930 = (esi & esi);
    if ((false || (fR_767930 == 0))) goto L_767962;
    goto L_767940;
L_767940:
    edi = rd32(m, ecx);
    edi = uint32_t(int32_t(edi) * int32_t(0x12a0u));
    fA_767948 = rd32(m, edi + edx + 0x10u);
    fB_767948 = 0x0u;
    if (fA_767948 != fB_767948) goto L_76795a;
    fA_76794f = rd32(m, esp + 0x10u);
    fB_76794f = 0x0u;
    if (fA_76794f == fB_76794f) goto L_76799e;
    wr32(m, esp + 0x10u, (rd32(m, esp + 0x10u) + 1u));
L_76795a:
    eax = (eax + 1u);
    ecx = ecx + 0x4u;
    fA_76795e = eax;
    fB_76795e = esi;
    if (fA_76795e < fB_76795e) goto L_767940;
L_767962:
    eax = 0u;
L_767964:
    esi = rd32(m, eax + 0x1acu);
    fR_76796a = (esi & esi);
    if (!(fR_76796a == 0)) goto L_7679af;
L_76796e:
    ecx = rd32(m, ebx + 0x21dfcu);
    fR_767974 = (ecx & ecx);
    if ((fR_767974 == 0)) goto L_76797f;
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x74u);
    LIFT_CALL(eax, 0x76797fu);
L_76797f:
    x0 = movss_m(rd32(m, 0x977d64u) /* 9999999.0 */);
    wr32(m, ebx + 0x21dfcu, 0x0u);
    wr32(m, ebx + 0x21e00u, x0.l(0));
    goto L_767afc;
L_76799e:
    eax = rd32(m, edx + eax * 4 + 0xdf8cu);
    eax = uint32_t(int32_t(eax) * int32_t(0x12a0u));
    eax = eax + edx;
    goto L_767964;
L_7679af:
    esi = rd32(m, esi + 0x190u);
    wr32(m, esp + 0x18u, esi);
    fR_7679b9 = (esi & esi);
    if ((fR_7679b9 == 0)) goto L_7679ca;
    fA_7679bd = rd8(m, esi + 0x21cu);
    fB_7679bd = 0x0u;
    if (fA_7679bd != fB_7679bd) goto L_767afc;
L_7679ca:
    x0 = movss_m(rd32(m, ebp + 0xcu));
    x0 = addss(x0, rd32(m, ebx + 0x21e00u));
    fC_7679d7 = fcmp32(x0.l(0), rd32(m, 0x9322ccu) /* 0.5 */);
    wr32(m, ebx + 0x21e00u, x0.l(0));
    if ((fc_b(fC_7679d7) || fc_z(fC_7679d7))) goto L_767aa5;
    esi = rd32(m, ebx + 0x21dd8u);
    x0 = movss_m(rd32(m, 0x977494u) /* 3.4028234663852886e+38 */);
    edi = 0u;
    wr32(m, esp + 0x10u, x0.l(0));
    fR_767a02 = (esi & esi);
    if ((fR_767a02 == 0)) goto L_767a38;
L_767a06:
    edx = rd32(m, esi);
    eax = rd32(m, edx + 0x94u);
    ecx = esi;
    LIFT_CALL(eax, 0x767a12u);
    wr32(m, esp + 0x14u, fst32(m));
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x10u)));
    fC_767a1a = f80cmp(st(m, 0), st(m, 1));
    fpop(m);
    st(m, 0) = st(m, 0);
    fpop(m);
    if ((fc_b(fC_767a1a) || fc_z(fC_767a1a))) goto L_767a2e;
    x0 = movss_m(rd32(m, esp + 0x14u));
    wr32(m, esp + 0x10u, x0.l(0));
    edi = esi;
L_767a2e:
    esi = rd32(m, esi + 0x310u);
    fR_767a34 = (esi & esi);
    if (!(fR_767a34 == 0)) goto L_767a06;
L_767a38:
    ecx = rd32(m, ebx + 0x21dfcu);
    fA_767a3e = ecx;
    fB_767a3e = edi;
    if (fA_767a3e == fB_767a3e) goto L_767a7f;
    fR_767a42 = (ecx & ecx);
    if ((fR_767a42 == 0)) goto L_767a4d;
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x74u);
    LIFT_CALL(eax, 0x767a4du);
L_767a4d:
    wr32(m, ebx + 0x21dfcu, edi);
    fR_767a53 = (edi & edi);
    if ((fR_767a53 == 0)) goto L_767a72;
    edx = rd32(m, edi);
    eax = rd32(m, edx + 0x70u);
    esp -= 4; wr32(m, esp, 0x0u);
    ecx = edi;
    LIFT_CALL(eax, 0x767a62u);
    ecx = rd32(m, ebx + 0x21dfcu);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x98u);
    LIFT_CALL(eax, 0x767a72u);
L_767a72:
    x0 = X{};
    wr32(m, ebx + 0x21e00u, x0.l(0));
    goto L_767aa5;
L_767a7f:
    fR_767a7f = (ecx & ecx);
    if ((fR_767a7f == 0)) goto L_767aa5;
    fA_767a83 = rd32(m, ecx + 0x2f4u);
    fB_767a83 = 0x0u;
    if (fA_767a83 != fB_767a83) goto L_767aa5;
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x70u);
    esp -= 4; wr32(m, esp, 0x0u);
    LIFT_CALL(eax, 0x767a95u);
    ecx = rd32(m, ebx + 0x21dfcu);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x98u);
    LIFT_CALL(eax, 0x767aa5u);
L_767aa5:
    eax = rd32(m, ebx + 0x21dfcu);
    fR_767aab = (eax & eax);
    if ((fR_767aab == 0)) goto L_767acb;
    fA_767aaf = rd32(m, 0xd61334u);
    fB_767aaf = 0x0u;
    if (fA_767aaf != fB_767aaf) goto L_767acb;
    ecx = rd32(m, ebx + 0x20138u);
    edx = rd32(m, ecx + 0x19b8u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x63dba0u, 0x767acbu);  // FUN_0063dba0
L_767acb:
    fA_767acb = rd8(m, ebx + 0x21de4u);
    fB_767acb = 0x0u;
    if (fA_767acb == fB_767acb) goto L_767afc;
    eax = rd32(m, esp + 0x18u);
    fR_767ad8 = (eax & eax);
    if ((fR_767ad8 == 0)) goto L_767afc;
    ecx = rd32(m, ebx + 0x20138u);
    edx = rd32(m, ecx + 0x19b8u);
    ecx = rd32(m, eax + 0x988u);
    esp -= 4; wr32(m, esp, edx);
    edx = rd32(m, eax + ecx * 4 + 0x974u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x63dba0u, 0x767afcu);  // FUN_0063dba0
L_767afc:
    fA_767afc = rd8(m, 0xa9434fu);
    fB_767afc = 0x0u;
    if (fA_767afc == fB_767afc) goto L_767b26;
    esi = rd32(m, ebx + 0x21dd8u);
    fR_767b0b = (esi & esi);
    if ((fR_767b0b == 0)) goto L_767b26;
L_767b10:
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x9cu);
    ecx = esi;
    LIFT_CALL(edx, 0x767b1cu);
    esi = rd32(m, esi + 0x310u);
    fR_767b22 = (esi & esi);
    if (!(fR_767b22 == 0)) goto L_767b10;
L_767b26:
    ecx = rd32(m, esp + 0x1cu);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x767b34u);  // __security_check_cookie
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 00775540 StdMemoryAllocator__vf2
//   original StdMemoryAllocator::vf2; register inputs: none; returns with ret 4
REFLEX_PORT_FN(0x00775540, StdMemoryAllocator__vf2, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_775540 = 0;
    uint32_t fB_775540 = 0;
    uint32_t fA_775562 = 0;
    uint32_t fB_775562 = 0;
    fA_775540 = rd8(m, 0xda67d8u);
    fB_775540 = 0x0u;
    if (fA_775540 == fB_775540) goto L_775554;
    esp -= 4; wr32(m, esp, 0xd61704u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x775554u);
L_775554:
    eax = rd32(m, esp + 0x4u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x916350u) /* 8.287536956934404e-39 */, 0x77555fu);
    esp = esp + 0x4u;
    fA_775562 = rd8(m, 0xda67d8u);
    fB_775562 = 0x0u;
    if (fA_775562 == fB_775562) goto L_775579;
    wr32(m, esp + 0x4u, 0xd61704u);
    LIFT_TAILJMP(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */);
L_775579:
    LIFT_RET(4);
}

// 00775700 Fn_00775700
//   original FUN_00775700; register inputs: esi edi; returns with ret 0
REFLEX_PORT_FN(0x00775700, Fn_00775700, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_77570a = 0;
    uint32_t fB_77570a = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, edi + 0xcu);
    wr32(m, esi, 0x0u);
    fA_77570a = ebx;
    fB_77570a = rd32(m, edi + 0x10u);
    if (fA_77570a <= fB_77570a) goto L_775715;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x775715u);
L_775715:
    eax = rd32(m, edi);
    wr32(m, esi, eax);
    wr32(m, esi + 0x4u, ebx);
    eax = esi;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00776d20 Fn_00776d20
//   original FUN_00776d20; register inputs: eax edx edi; returns with ret 0
REFLEX_PORT_FN(0x00776d20, Fn_00776d20, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_776d30 = 0;
    eax = eax - edi;
    eax = uint32_t(int32_t(eax) >> 2);
    ecx = (eax * 4);
    esp -= 4; wr32(m, esp, esi);
    esi = (ecx + edx);
    fR_776d30 = (eax & eax);
    if ((false || (fR_776d30 == 0))) goto L_776d41;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(rd32(m, 0x916200u) /* 8.286124448082365e-39 */, 0x776d3eu);
    esp = esp + 0x10u;
L_776d41:
    eax = esi;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00779440 Fn_00779440
//   original FUN_00779440; register inputs: ecx eax esi; returns with ret 0
REFLEX_PORT_FN(0x00779440, Fn_00779440, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_779499 = 0;
    uint32_t fB_779499 = 0;
    uint32_t fA_7794ad = 0;
    uint32_t fB_7794ad = 0;
    uint32_t fA_7794bb = 0;
    uint32_t fB_7794bb = 0;
    uint32_t fA_7794c4 = 0;
    uint32_t fB_7794c4 = 0;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esp + 0x14u);
    switch (ecx) { case 0: goto L_779452; case 1: goto L_77945f; case 2: goto L_77946b; case 3: goto L_779478; case 4: goto L_779484; case 5: goto L_779493; case 6: goto L_7794da; case 7: goto L_7794e6; default: throw std::runtime_error("jump table"); }
L_779452:
    wr32(m, esi + 0x40u, 0xfffffffdu);
    eax = edi;
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_77945f:
    edx = rd32(m, esi + 0x44u);
    wr32(m, esi + 0x40u, edx);
    eax = edi;
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_77946b:
    eax = ebx;
    LIFT_CALL(0x784e30u, 0x779472u);  // FUN_00784e30
    eax = edi;
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_779478:
    edi = rd32(m, esi + 0x44u);
    edi = (edi >> 10);
    eax = edi;
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_779484:
    edi = rd32(m, esi + 0x44u);
    edi = (edi & 0x3ffu);
    eax = edi;
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_779493:
    ecx = rd32(m, esi + 0x44u);
    eax = (eax << 10);
    fA_779499 = eax;
    fB_779499 = ecx;
    if (fA_779499 > fB_779499) goto L_7794a6;
    edx = ecx;
    edx = edx - eax;
    wr32(m, esi + 0x40u, edx);
    goto L_7794ad;
L_7794a6:
    wr32(m, esi + 0x40u, 0x0u);
L_7794ad:
    fA_7794ad = rd32(m, esi + 0x40u);
    fB_7794ad = ecx;
    if (fA_7794ad > fB_7794ad) goto L_7794f2;
L_7794b2:
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x784da0u, 0x7794b8u);  // FUN_00784da0
    esp = esp + 0x4u;
    fA_7794bb = rd8(m, esi + 0x15u);
    fB_7794bb = 0x0u;
    if (fA_7794bb == fB_7794bb) goto L_7794cf;
    eax = rd32(m, esi + 0x40u);
    fA_7794c4 = eax;
    fB_7794c4 = rd32(m, esi + 0x44u);
    if (fA_7794c4 <= fB_7794c4) goto L_7794b2;
    eax = edi;
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_7794cf:
    edi = 0x1u;
    eax = edi;
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_7794da:
    edi = rd32(m, esi + 0x50u);
    wr32(m, esi + 0x50u, eax);
    eax = edi;
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_7794e6:
    edi = rd32(m, esi + 0x54u);
    wr32(m, esi + 0x54u, eax);
    eax = edi;
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_7794f2:
    eax = edi;
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00783d70 Fn_00783d70
//   original FUN_00783d70; register inputs: edx esi; returns with ret 0
REFLEX_PORT_FN(0x00783d70, Fn_00783d70, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_783d90 = 0;
    uint32_t fB_783d90 = 0;
    uint32_t fA_783d95 = 0;
    uint32_t fB_783d95 = 0;
    uint32_t fR_783d9d = 0;
    ecx = (ecx & 0xffffff00u) | ((rd8(m, edx + 0x7u)) & 0xffu);
    eax = 0x1u;
    eax = (eax << ((ecx & 0xffu) & 31u));
    ecx = rd32(m, edx + 0x10u);
    eax = (eax - 1u);
    eax = (eax & rd32(m, esi + 0x8u));
    eax = (eax + eax * 4);
    eax = (ecx + eax * 4);
    ecx = 0x4u;
L_783d90:
    fA_783d90 = rd32(m, eax + 0xcu);
    fB_783d90 = ecx;
    if (fA_783d90 != fB_783d90) goto L_783d9a;
    fA_783d95 = rd32(m, eax + 0x8u);
    fB_783d95 = esi;
    if (fA_783d95 == fB_783d95) goto L_783da6;
L_783d9a:
    eax = rd32(m, eax + 0x10u);
    fR_783d9d = (eax & eax);
    if (!(fR_783d9d == 0)) goto L_783d90;
    eax = 0x92e59cu;
L_783da6:
    LIFT_RET(0);
}

// 00783fb0 Lua_GcReallyMarkObject: Lua 5.1 reallymarkobject (recursive).
//   original FUN_00783fb0; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00783fb0, Lua_GcReallyMarkObject, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_783fc0 = 0;
    uint32_t fB_783fc0 = 0;
    uint32_t fR_783fde = 0;
    uint32_t fR_783fe2 = 0;
    uint32_t fR_783ff5 = 0;
    uint32_t fA_784006 = 0;
    uint32_t fB_784006 = 0;
    uint32_t fA_784011 = 0;
    uint32_t fB_784011 = 0;
    uint32_t fR_784019 = 0;
    uint32_t fA_78402c = 0;
    uint32_t fB_78402c = 0;
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, esp + 0xcu);
    eax = rd8(m, esi + 0x4u);
    wr8(m, esi + 0x5u, (rd8(m, esi + 0x5u) & 0xfcu));
    eax = eax - 0x4u;
    fA_783fc0 = eax;
    fB_783fc0 = 0x6u;
    if (fA_783fc0 > fB_783fc0) goto L_784066;
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esp + 0xcu);
    edi = edi;
L_783fd0:
    switch (eax) { case 0: goto L_784065; case 1: goto L_784044; case 2: goto L_784038; case 3: goto L_783fd7; case 4: goto L_784050; case 5: goto L_78405c; case 6: goto L_78400e; default: throw std::runtime_error("jump table"); }
L_783fd7:
    eax = rd32(m, esi + 0x8u);
    wr8(m, esi + 0x5u, (rd8(m, esi + 0x5u) | 0x4u));
    fR_783fde = (eax & eax);
    if ((fR_783fde == 0)) goto L_783ff2;
    fR_783fe2 = (rd8(m, eax + 0x5u) & 0x3u);
    if ((fR_783fe2 == 0)) goto L_783ff2;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x783fb0u, 0x783fefu);  // FUN_00783fb0
    esp = esp + 0x8u;
L_783ff2:
    esi = rd32(m, esi + 0xcu);
    fR_783ff5 = (rd8(m, esi + 0x5u) & 0x3u);
    if ((fR_783ff5 == 0)) goto L_784065;
    wr8(m, esi + 0x5u, (rd8(m, esi + 0x5u) & 0xfcu));
    eax = rd8(m, esi + 0x4u);
    eax = eax - 0x4u;
    fA_784006 = eax;
    fB_784006 = 0x6u;
    if (fA_784006 <= fB_784006) goto L_783fd0;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_78400e:
    eax = rd32(m, esi + 0x8u);
    fA_784011 = rd32(m, eax + 0x4u);
    fB_784011 = 0x4u;
    if (int32_t(sext(fA_784011, 4)) < int32_t(sext(fB_784011, 4))) goto L_784029;
    eax = rd32(m, eax);
    fR_784019 = (rd8(m, eax + 0x5u) & 0x3u);
    if ((fR_784019 == 0)) goto L_784029;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x783fb0u, 0x784026u);  // FUN_00783fb0
    esp = esp + 0x8u;
L_784029:
    eax = (esi + 0xcu);
    fA_78402c = rd32(m, esi + 0x8u);
    fB_78402c = eax;
    if (fA_78402c != fB_78402c) goto L_784065;
    wr8(m, esi + 0x5u, (rd8(m, esi + 0x5u) | 0x4u));
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_784038:
    ecx = rd32(m, edi + 0x24u);
    wr32(m, esi + 0x8u, ecx);
    wr32(m, edi + 0x24u, esi);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_784044:
    edx = rd32(m, edi + 0x24u);
    wr32(m, esi + 0x18u, edx);
    wr32(m, edi + 0x24u, esi);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_784050:
    eax = rd32(m, edi + 0x24u);
    wr32(m, esi + 0x5cu, eax);
    wr32(m, edi + 0x24u, esi);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_78405c:
    ecx = rd32(m, edi + 0x24u);
    wr32(m, esi + 0x44u, ecx);
    wr32(m, edi + 0x24u, esi);
L_784065:
    edi = rd32(m, esp); esp += 4;
L_784066:
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00784090 Fn_00784090
//   original FUN_00784090; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00784090, Fn_00784090, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_7840ac = 0;
    uint32_t fR_7840b9 = 0;
    uint32_t fA_7840be = 0;
    uint32_t fB_7840be = 0;
    uint32_t fR_7840c5 = 0;
    uint32_t fR_7840d1 = 0;
    uint32_t fR_7840d5 = 0;
    uint32_t fA_7840ed = 0;
    uint32_t fB_7840ed = 0;
    uint32_t fR_784120 = 0;
    uint32_t fR_784137 = 0;
    esp = esp - 0x8u;
    eax = rd32(m, esp + 0xcu);
    eax = rd32(m, eax + 0x10u);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, eax + 0x64u);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, ebp);
    ecx = 0u;
    wr32(m, esp + 0xcu, eax);
    wr32(m, esp + 0x8u, ecx);
    fR_7840ac = (edi & edi);
    if ((fR_7840ac == 0)) goto L_78414b;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
L_7840b6:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, edi + 0x5u)) & 0xffu);
    fR_7840b9 = ((ebx & 0xffu) & 0x3u);
    if (!(fR_7840b9 == 0)) goto L_7840c5;
    fA_7840be = rd32(m, esp + 0x20u);
    fB_7840be = 0x0u;
    if (fA_7840be == fB_7840be) goto L_7840ca;
L_7840c5:
    fR_7840c5 = ((ebx & 0xffu) & 0x8u);
    if ((fR_7840c5 == 0)) goto L_7840ce;
L_7840ca:
    ebp = edi;
    goto L_784134;
L_7840ce:
    edx = rd32(m, edi + 0x8u);
    fR_7840d1 = (edx & edx);
    if ((fR_7840d1 == 0)) goto L_7840f7;
    fR_7840d5 = (rd8(m, edx + 0x6u) & 0x4u);
    if (!(fR_7840d5 == 0)) goto L_7840f7;
    ecx = rd32(m, esp + 0x1cu);
    eax = rd32(m, ecx + 0x10u);
    esi = rd32(m, eax + 0xa8u);
    LIFT_CALL(0x783d70u, 0x7840edu);  // FUN_00783d70
    fA_7840ed = rd32(m, eax + 0x4u);
    fB_7840ed = 0x0u;
    if (fA_7840ed != fB_7840ed) goto L_7840ff;
    wr8(m, edx + 0x6u, (rd8(m, edx + 0x6u) | 0x4u));
L_7840f7:
    wr8(m, edi + 0x5u, (rd8(m, edi + 0x5u) | 0x8u));
    ebp = edi;
    goto L_784134;
L_7840ff:
    ecx = rd32(m, edi + 0x10u);
    edx = rd32(m, esp + 0x10u);
    eax = (edx + ecx + 0x18u);
    ecx = rd32(m, edi);
    ebx = (ebx & 0xffffff00u) | ((((ebx & 0xffu) | 0x8u)) & 0xffu);
    wr8(m, edi + 0x5u, (ebx & 0xffu));
    wr32(m, ebp, ecx);
    ecx = rd32(m, esp + 0x14u);
    wr32(m, esp + 0x10u, eax);
    eax = rd32(m, ecx + 0x30u);
    fR_784120 = (eax & eax);
    if (!(fR_784120 == 0)) goto L_784128;
    wr32(m, edi, edi);
    goto L_784131;
L_784128:
    edx = rd32(m, eax);
    wr32(m, edi, edx);
    eax = rd32(m, ecx + 0x30u);
    wr32(m, eax, edi);
L_784131:
    wr32(m, ecx + 0x30u, edi);
L_784134:
    edi = rd32(m, ebp);
    fR_784137 = (edi & edi);
    if (!(fR_784137 == 0)) goto L_7840b6;
    eax = rd32(m, esp + 0x10u);
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    esp = esp + 0x8u;
    LIFT_RET(0);
L_78414b:
    edi = rd32(m, esp); esp += 4;
    eax = ecx;
    ebp = rd32(m, esp); esp += 4;
    esp = esp + 0x8u;
    LIFT_RET(0);
}

// 00784160 Lua_GcTraverseTable: Lua 5.1 traversetable: weak mode from the metatable's __mode (strchr 'k' / 'v'), marks entries.
//   original FUN_00784160; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x00784160, Lua_GcTraverseTable, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_784176 = 0;
    uint32_t fB_784176 = 0;
    uint32_t fR_78417a = 0;
    uint32_t fA_784197 = 0;
    uint32_t fB_784197 = 0;
    uint32_t fR_78419f = 0;
    uint32_t fA_7841b4 = 0;
    uint32_t fB_7841b4 = 0;
    uint32_t fA_7841c3 = 0;
    uint32_t fB_7841c3 = 0;
    uint32_t fA_7841c7 = 0;
    uint32_t fB_7841c7 = 0;
    uint32_t fA_7841df = 0;
    uint32_t fR_7841df = 0;
    uint32_t fA_7841f8 = 0;
    uint32_t fR_7841f8 = 0;
    uint32_t fR_784202 = 0;
    uint32_t fR_784206 = 0;
    uint32_t fR_78422e = 0;
    uint32_t fR_784232 = 0;
    uint32_t fR_784241 = 0;
    uint32_t fR_784248 = 0;
    uint32_t fA_784255 = 0;
    uint32_t fB_784255 = 0;
    uint32_t fR_784260 = 0;
    uint32_t fR_784270 = 0;
    uint32_t fR_78427e = 0;
    uint32_t fA_784299 = 0;
    uint32_t fB_784299 = 0;
    uint32_t fA_78429f = 0;
    uint32_t fB_78429f = 0;
    uint32_t fA_7842ae = 0;
    uint32_t fB_7842ae = 0;
    uint32_t fA_7842b5 = 0;
    uint32_t fB_7842b5 = 0;
    uint32_t fR_7842be = 0;
    uint32_t fA_7842d2 = 0;
    uint32_t fB_7842d2 = 0;
    uint32_t fA_7842d9 = 0;
    uint32_t fB_7842d9 = 0;
    uint32_t fR_7842e1 = 0;
    uint32_t fR_7842f5 = 0;
    uint32_t fA_7842f9 = 0;
    uint32_t fB_7842f9 = 0;
    uint32_t fA_784304 = 0;
    uint32_t fB_784304 = 0;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x10u);
    eax = rd32(m, ebx + 0x8u);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = 0u;
    wr32(m, esp + 0x1cu, edi);
    wr32(m, esp + 0x10u, edi);
    fA_784176 = eax;
    fB_784176 = edi;
    if (fA_784176 == fB_784176) goto L_784190;
    fR_78417a = (rd8(m, eax + 0x5u) & 0x3u);
    if ((fR_78417a == 0)) goto L_784190;
    ebp = rd32(m, esp + 0x18u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ebp);
    LIFT_CALL(0x783fb0u, 0x78418bu);  // FUN_00783fb0
    esp = esp + 0x8u;
    goto L_784194;
L_784190:
    ebp = rd32(m, esp + 0x18u);
L_784194:
    edx = rd32(m, ebx + 0x8u);
    fA_784197 = edx;
    fB_784197 = edi;
    if (fA_784197 == fB_784197) goto L_784245;
    fR_78419f = (rd8(m, edx + 0x6u) & 0x8u);
    if ((fR_78419f == 0)) goto L_7841a9;
    esi = 0u;
    goto L_7841c3;
L_7841a9:
    esi = rd32(m, ebp + 0xacu);
    LIFT_CALL(0x783d70u, 0x7841b4u);  // FUN_00783d70
    fA_7841b4 = rd32(m, eax + 0x4u);
    fB_7841b4 = edi;
    if (fA_7841b4 != fB_7841b4) goto L_7841c1;
    wr8(m, edx + 0x6u, (rd8(m, edx + 0x6u) | 0x8u));
    esi = 0u;
    goto L_7841c3;
L_7841c1:
    esi = eax;
L_7841c3:
    fA_7841c3 = esi;
    fB_7841c3 = edi;
    if (fA_7841c3 == fB_7841c3) goto L_784245;
    fA_7841c7 = rd32(m, esi + 0x4u);
    fB_7841c7 = 0x4u;
    if (fA_7841c7 != fB_7841c7) goto L_784245;
    eax = rd32(m, esi);
    edi = rd32(m, 0x916248u) /* 8.28654764021859e-39 */;
    eax = eax + 0x10u;
    esp -= 4; wr32(m, esp, 0x6bu);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(edi, 0x7841ddu);
    ecx = rd32(m, esi);
    fA_7841df = eax;
    fR_7841df = (0u - fA_7841df);
    eax = fR_7841df;
    eax = eax - eax - ((fA_7841df != 0) ? 1u : 0u);
    ecx = ecx + 0x10u;
    eax = (0u - eax);
    esp -= 4; wr32(m, esp, 0x76u);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp + 0x2cu, eax);
    LIFT_CALL(edi, 0x7841f1u);
    ecx = rd32(m, esp + 0x2cu);
    esp = esp + 0x10u;
    fA_7841f8 = eax;
    fR_7841f8 = (0u - fA_7841f8);
    eax = fR_7841f8;
    eax = eax - eax - ((fA_7841f8 != 0) ? 1u : 0u);
    eax = (0u - eax);
    wr32(m, esp + 0x10u, eax);
    fR_784202 = (ecx & ecx);
    if (!(fR_784202 == 0)) goto L_78420a;
    fR_784206 = (eax & eax);
    if ((fR_784206 == 0)) goto L_78422e;
L_78420a:
    edx = (edx & 0xffffff00u) | (((eax & 0xffu)) & 0xffu);
    edx = (edx & 0xffffff00u) | ((((edx & 0xffu) + (edx & 0xffu)) & 0xffu) & 0xffu);
    edx = (edx & 0xffffff00u) | ((((edx & 0xffu) | (ecx & 0xffu))) & 0xffu);
    ecx = (ecx & 0xffffff00u) | ((rd8(m, ebx + 0x5u)) & 0xffu);
    edx = (edx & 0xffffff00u) | ((((edx & 0xffu) + (edx & 0xffu)) & 0xffu) & 0xffu);
    edx = (edx & 0xffffff00u) | ((((edx & 0xffu) + (edx & 0xffu)) & 0xffu) & 0xffu);
    edx = (edx & 0xffffff00u) | ((((edx & 0xffu) + (edx & 0xffu)) & 0xffu) & 0xffu);
    ecx = (ecx & 0xffffff00u) | ((((ecx & 0xffu) & 0xe7u)) & 0xffu);
    edx = (edx & 0xffffff00u) | ((((edx & 0xffu) | (ecx & 0xffu))) & 0xffu);
    ecx = rd32(m, esp + 0x1cu);
    wr8(m, ebx + 0x5u, (edx & 0xffu));
    edx = rd32(m, ebp + 0x2cu);
    wr32(m, ebx + 0x18u, edx);
    wr32(m, ebp + 0x2cu, ebx);
L_78422e:
    fR_78422e = (ecx & ecx);
    if ((fR_78422e == 0)) goto L_784241;
    fR_784232 = (eax & eax);
    if ((fR_784232 == 0)) goto L_784245;
L_784236:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = 0x1u;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_784241:
    fR_784241 = (eax & eax);
    if (!(fR_784241 == 0)) goto L_784274;
L_784245:
    esi = rd32(m, ebx + 0x1cu);
    fR_784248 = (esi & esi);
    if ((fR_784248 == 0)) goto L_784274;
    edi = 0x4u;
L_784251:
    eax = rd32(m, ebx + 0xcu);
    esi = (esi - 1u);
    fA_784255 = rd32(m, eax + esi * 8 + 0x4u);
    fB_784255 = edi;
    eax = (eax + esi * 8);
    if (int32_t(sext(fA_784255, 4)) < int32_t(sext(fB_784255, 4))) goto L_784270;
    eax = rd32(m, eax);
    fR_784260 = (rd8(m, eax + 0x5u) & 0x3u);
    if ((fR_784260 == 0)) goto L_784270;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ebp);
    LIFT_CALL(0x783fb0u, 0x78426du);  // FUN_00783fb0
    esp = esp + 0x8u;
L_784270:
    fR_784270 = (esi & esi);
    if (!(fR_784270 == 0)) goto L_784251;
L_784274:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, ebx + 0x7u)) & 0xffu);
    edi = 0x1u;
    edi = (edi << ((ecx & 0xffu) & 31u));
    fR_78427e = (edi & edi);
    if ((fR_78427e == 0)) goto L_7842f9;
    ebp = (edi + edi * 4);
    ebp = ebp + ebp;
    ebp = ebp + ebp;
L_784290:
    esi = rd32(m, ebx + 0x10u);
    ebp = ebp - 0x14u;
    esi = esi + ebp;
    edi = (edi - 1u);
    fA_784299 = rd32(m, esi + 0x4u);
    fB_784299 = 0x0u;
    if (fA_784299 != fB_784299) goto L_7842ae;
    fA_78429f = rd32(m, esi + 0xcu);
    fB_78429f = 0x4u;
    if (int32_t(sext(fA_78429f, 4)) < int32_t(sext(fB_78429f, 4))) goto L_7842f5;
    wr32(m, esi + 0xcu, 0xbu);
    goto L_7842f5;
L_7842ae:
    fA_7842ae = rd32(m, esp + 0x1cu);
    fB_7842ae = 0x0u;
    if (fA_7842ae != fB_7842ae) goto L_7842d2;
    fA_7842b5 = rd32(m, esi + 0xcu);
    fB_7842b5 = 0x4u;
    if (int32_t(sext(fA_7842b5, 4)) < int32_t(sext(fB_7842b5, 4))) goto L_7842d2;
    eax = rd32(m, esi + 0x8u);
    fR_7842be = (rd8(m, eax + 0x5u) & 0x3u);
    if ((fR_7842be == 0)) goto L_7842d2;
    ecx = rd32(m, esp + 0x18u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x783fb0u, 0x7842cfu);  // FUN_00783fb0
    esp = esp + 0x8u;
L_7842d2:
    fA_7842d2 = rd32(m, esp + 0x10u);
    fB_7842d2 = 0x0u;
    if (fA_7842d2 != fB_7842d2) goto L_7842f5;
    fA_7842d9 = rd32(m, esi + 0x4u);
    fB_7842d9 = 0x4u;
    if (int32_t(sext(fA_7842d9, 4)) < int32_t(sext(fB_7842d9, 4))) goto L_7842f5;
    esi = rd32(m, esi);
    fR_7842e1 = (rd8(m, esi + 0x5u) & 0x3u);
    if ((fR_7842e1 == 0)) goto L_7842f5;
    edx = rd32(m, esp + 0x18u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x783fb0u, 0x7842f2u);  // FUN_00783fb0
    esp = esp + 0x8u;
L_7842f5:
    fR_7842f5 = (edi & edi);
    if (!(fR_7842f5 == 0)) goto L_784290;
L_7842f9:
    fA_7842f9 = rd32(m, esp + 0x1cu);
    fB_7842f9 = 0x0u;
    if (fA_7842f9 != fB_7842f9) goto L_784236;
    fA_784304 = rd32(m, esp + 0x10u);
    fB_784304 = 0x0u;
    if (fA_784304 != fB_784304) goto L_784236;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00784320 Fn_00784320
//   original FUN_00784320; register inputs: esi; returns with ret 0
REFLEX_PORT_FN(0x00784320, Fn_00784320, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_784320 = 0;
    uint32_t fB_784320 = 0;
    uint32_t fA_784331 = 0;
    uint32_t fB_784331 = 0;
    uint32_t fA_784343 = 0;
    uint32_t fB_784343 = 0;
    uint32_t fR_78434f = 0;
    uint32_t fA_784363 = 0;
    uint32_t fB_784363 = 0;
    uint32_t fA_78436a = 0;
    uint32_t fB_78436a = 0;
    uint32_t fA_784373 = 0;
    uint32_t fB_784373 = 0;
    uint32_t fA_784383 = 0;
    uint32_t fB_784383 = 0;
    uint32_t fA_78438a = 0;
    uint32_t fB_78438a = 0;
    uint32_t fA_784393 = 0;
    uint32_t fB_784393 = 0;
    uint32_t fR_78439e = 0;
    uint32_t fA_7843b2 = 0;
    uint32_t fB_7843b2 = 0;
    uint32_t fA_7843b9 = 0;
    uint32_t fB_7843b9 = 0;
    uint32_t fA_7843c3 = 0;
    uint32_t fB_7843c3 = 0;
    uint32_t fA_7843df = 0;
    uint32_t fB_7843df = 0;
    fA_784320 = rd32(m, esi + 0x20u);
    fB_784320 = 0x0u;
    if (fA_784320 == fB_784320) goto L_78432d;
    eax = rd32(m, esi + 0x20u);
    wr8(m, eax + 0x5u, (rd8(m, eax + 0x5u) & 0xfcu));
L_78432d:
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, edi);
    edi = 0u;
    fA_784331 = rd32(m, esi + 0x28u);
    fB_784331 = edi;
    ebx = (ebx & 0xffffff00u) | ((0x3u) & 0xffu);
    if (int32_t(sext(fA_784331, 4)) <= int32_t(sext(fB_784331, 4))) goto L_784368;
    goto L_784340;
L_784340:
    eax = rd32(m, esi + 0x8u);
    fA_784343 = rd32(m, eax + edi * 8 + 0x4u);
    fB_784343 = 0x4u;
    eax = (eax + edi * 8);
    if (int32_t(sext(fA_784343, 4)) < int32_t(sext(fB_784343, 4))) goto L_784362;
    eax = rd32(m, eax);
    fR_78434f = (rd8(m, eax + 0x5u) & (ebx & 0xffu));
    if ((fR_78434f == 0)) goto L_784362;
    ecx = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x783fb0u, 0x78435fu);  // FUN_00783fb0
    esp = esp + 0x8u;
L_784362:
    edi = (edi + 1u);
    fA_784363 = edi;
    fB_784363 = rd32(m, esi + 0x28u);
    if (int32_t(sext(fA_784363, 4)) < int32_t(sext(fB_784363, 4))) goto L_784340;
L_784368:
    eax = 0u;
    fA_78436a = rd32(m, esi + 0x24u);
    fB_78436a = eax;
    if (int32_t(sext(fA_78436a, 4)) <= int32_t(sext(fB_78436a, 4))) goto L_784388;
L_784370:
    edx = rd32(m, esi + 0x1cu);
    fA_784373 = rd32(m, edx + eax * 4);
    fB_784373 = 0x0u;
    if (fA_784373 == fB_784373) goto L_784382;
    ecx = edx;
    ecx = rd32(m, ecx + eax * 4);
    wr8(m, ecx + 0x5u, (rd8(m, ecx + 0x5u) & 0xfcu));
L_784382:
    eax = (eax + 1u);
    fA_784383 = eax;
    fB_784383 = rd32(m, esi + 0x24u);
    if (int32_t(sext(fA_784383, 4)) < int32_t(sext(fB_784383, 4))) goto L_784370;
L_784388:
    edi = 0u;
    fA_78438a = rd32(m, esi + 0x34u);
    fB_78438a = edi;
    if (int32_t(sext(fA_78438a, 4)) <= int32_t(sext(fB_78438a, 4))) goto L_7843b7;
L_784390:
    edx = rd32(m, esi + 0x10u);
    fA_784393 = rd32(m, edx + edi * 4);
    fB_784393 = 0x0u;
    eax = (edx + edi * 4);
    if (fA_784393 == fB_784393) goto L_7843b1;
    eax = rd32(m, eax);
    fR_78439e = (rd8(m, eax + 0x5u) & (ebx & 0xffu));
    if ((fR_78439e == 0)) goto L_7843b1;
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x783fb0u, 0x7843aeu);  // FUN_00783fb0
    esp = esp + 0x8u;
L_7843b1:
    edi = (edi + 1u);
    fA_7843b2 = edi;
    fB_7843b2 = rd32(m, esi + 0x34u);
    if (int32_t(sext(fA_7843b2, 4)) < int32_t(sext(fB_7843b2, 4))) goto L_784390;
L_7843b7:
    edi = 0u;
    fA_7843b9 = rd32(m, esi + 0x38u);
    fB_7843b9 = edi;
    if (int32_t(sext(fA_7843b9, 4)) <= int32_t(sext(fB_7843b9, 4))) goto L_7843e4;
    eax = 0u;
L_7843c0:
    ecx = rd32(m, esi + 0x18u);
    fA_7843c3 = rd32(m, eax + ecx);
    fB_7843c3 = 0x0u;
    if (fA_7843c3 == fB_7843c3) goto L_7843db;
    edx = ecx;
    ecx = (edx + eax);
    edx = rd32(m, ecx);
    edx = (edx & 0xffffff00u) | ((rd8(m, edx + 0x5u)) & 0xffu);
    ecx = rd32(m, ecx);
    edx = (edx & 0xffffff00u) | ((((edx & 0xffu) & 0xfcu)) & 0xffu);
    wr8(m, ecx + 0x5u, (edx & 0xffu));
L_7843db:
    edi = (edi + 1u);
    eax = eax + 0xcu;
    fA_7843df = edi;
    fB_7843df = rd32(m, esi + 0x38u);
    if (int32_t(sext(fA_7843df, 4)) < int32_t(sext(fB_7843df, 4))) goto L_7843c0;
L_7843e4:
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 007843f0 Fn_007843f0
//   original FUN_007843f0; register inputs: esi; returns with ret 0
REFLEX_PORT_FN(0x007843f0, Fn_007843f0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_7843f3 = 0;
    uint32_t fA_78440a = 0;
    uint32_t fB_78440a = 0;
    uint32_t fA_784412 = 0;
    uint32_t fB_784412 = 0;
    uint32_t fA_784420 = 0;
    uint32_t fB_784420 = 0;
    uint32_t fR_784428 = 0;
    uint32_t fA_784440 = 0;
    uint32_t fB_784440 = 0;
    uint32_t fR_78444b = 0;
    uint32_t fA_78445d = 0;
    uint32_t fB_78445d = 0;
    uint32_t fR_784469 = 0;
    uint32_t fA_784481 = 0;
    uint32_t fB_784481 = 0;
    eax = rd32(m, esi + 0xcu);
    fR_7843f3 = (rd8(m, eax + 0x5u) & 0x3u);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, edi);
    if ((fR_7843f3 == 0)) goto L_78440a;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x783fb0u, 0x784407u);  // FUN_00783fb0
    esp = esp + 0x8u;
L_78440a:
    fA_78440a = rd8(m, esi + 0x6u);
    fB_78440a = 0x0u;
    if (fA_78440a == fB_78440a) goto L_784448;
    ebp = 0u;
    fA_784412 = rd8(m, esi + 0x7u);
    fB_784412 = 0x0u;
    if (fA_784412 <= fB_784412) goto L_784485;
    edi = (esi + 0x14u);
    goto L_784420;
L_784420:
    fA_784420 = rd32(m, edi + 0x4u);
    fB_784420 = 0x4u;
    if (int32_t(sext(fA_784420, 4)) < int32_t(sext(fB_784420, 4))) goto L_784438;
    eax = rd32(m, edi);
    fR_784428 = (rd8(m, eax + 0x5u) & 0x3u);
    if ((fR_784428 == 0)) goto L_784438;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x783fb0u, 0x784435u);  // FUN_00783fb0
    esp = esp + 0x8u;
L_784438:
    eax = rd8(m, esi + 0x7u);
    ebp = (ebp + 1u);
    edi = edi + 0x8u;
    fA_784440 = ebp;
    fB_784440 = eax;
    if (int32_t(sext(fA_784440, 4)) < int32_t(sext(fB_784440, 4))) goto L_784420;
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_784448:
    eax = rd32(m, esi + 0x10u);
    fR_78444b = (rd8(m, eax + 0x5u) & 0x3u);
    if ((fR_78444b == 0)) goto L_78445b;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x783fb0u, 0x784458u);  // FUN_00783fb0
    esp = esp + 0x8u;
L_78445b:
    edi = 0u;
    fA_78445d = rd8(m, esi + 0x7u);
    fB_78445d = 0x0u;
    if (fA_78445d <= fB_78445d) goto L_784485;
    ebp = (esi + 0x14u);
L_784466:
    eax = rd32(m, ebp);
    fR_784469 = (rd8(m, eax + 0x5u) & 0x3u);
    if ((fR_784469 == 0)) goto L_784479;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x783fb0u, 0x784476u);  // FUN_00783fb0
    esp = esp + 0x8u;
L_784479:
    ecx = rd8(m, esi + 0x7u);
    edi = (edi + 1u);
    ebp = ebp + 0x4u;
    fA_784481 = edi;
    fB_784481 = ecx;
    if (int32_t(sext(fA_784481, 4)) < int32_t(sext(fB_784481, 4))) goto L_784466;
L_784485:
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00784490 Fn_00784490
//   original FUN_00784490; register inputs: ecx eax; returns with ret 0
REFLEX_PORT_FN(0x00784490, Fn_00784490, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_78449e = 0;
    uint32_t fB_78449e = 0;
    uint32_t fA_7844c3 = 0;
    uint32_t fB_7844c3 = 0;
    uint32_t fA_7844c7 = 0;
    uint32_t fB_7844c7 = 0;
    uint32_t fA_7844e7 = 0;
    uint32_t fB_7844e7 = 0;
    uint32_t fA_7844eb = 0;
    uint32_t fB_7844eb = 0;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    eax = eax - rd32(m, esi + 0x20u);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esi + 0x30u);
    eax = uint32_t(int32_t(eax) >> 3);
    fA_78449e = edi;
    fB_78449e = 0x4e20u;
    ebx = eax;
    if (int32_t(sext(fA_78449e, 4)) > int32_t(sext(fB_78449e, 4))) goto L_7844fe;
    ecx = rd32(m, esi + 0x14u);
    ecx = ecx - rd32(m, esi + 0x28u);
    eax = 0x2aaaaaabu;
    { int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); }
    edx = uint32_t(int32_t(edx) >> 2);
    eax = edx;
    eax = (eax >> 31);
    eax = eax + edx;
    eax = eax + eax;
    eax = eax + eax;
    fA_7844c3 = eax;
    fB_7844c3 = edi;
    if (int32_t(sext(fA_7844c3, 4)) >= int32_t(sext(fB_7844c3, 4))) goto L_7844dd;
    fA_7844c7 = edi;
    fB_7844c7 = 0x10u;
    if (int32_t(sext(fA_7844c7, 4)) <= int32_t(sext(fB_7844c7, 4))) goto L_7844dd;
    eax = edi;
    edx = (eax >> 31) ? 0xffffffffu : 0;
    eax = eax - edx;
    eax = uint32_t(int32_t(eax) >> 1);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x787640u, 0x7844dau);  // FUN_00787640
    esp = esp + 0x8u;
L_7844dd:
    eax = rd32(m, esi + 0x2cu);
    ecx = (ebx * 4);
    fA_7844e7 = ecx;
    fB_7844e7 = eax;
    if (int32_t(sext(fA_7844e7, 4)) >= int32_t(sext(fB_7844e7, 4))) goto L_7844fe;
    fA_7844eb = eax;
    fB_7844eb = 0x5au;
    if (int32_t(sext(fA_7844eb, 4)) <= int32_t(sext(fB_7844eb, 4))) goto L_7844fe;
    edx = (eax >> 31) ? 0xffffffffu : 0;
    eax = eax - edx;
    eax = uint32_t(int32_t(eax) >> 1);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x7875e0u, 0x7844fbu);  // FUN_007875e0
    esp = esp + 0x4u;
L_7844fe:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00784510 Fn_00784510
//   original FUN_00784510; register inputs: ecx edi; returns with ret 0
REFLEX_PORT_FN(0x00784510, Fn_00784510, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_784511 = 0;
    uint32_t fB_784511 = 0;
    uint32_t fR_78451d = 0;
    uint32_t fA_78453c = 0;
    uint32_t fB_78453c = 0;
    uint32_t fA_784543 = 0;
    uint32_t fB_784543 = 0;
    uint32_t fA_78454b = 0;
    uint32_t fB_78454b = 0;
    uint32_t fA_784552 = 0;
    uint32_t fB_784552 = 0;
    uint32_t fA_784556 = 0;
    uint32_t fB_784556 = 0;
    uint32_t fR_78455e = 0;
    uint32_t fA_784575 = 0;
    uint32_t fB_784575 = 0;
    uint32_t fA_78457a = 0;
    uint32_t fB_78457a = 0;
    uint32_t fA_784586 = 0;
    uint32_t fB_784586 = 0;
    esp -= 4; wr32(m, esp, ecx);
    fA_784511 = rd32(m, edi + 0x4cu);
    fB_784511 = 0x4u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    if (int32_t(sext(fA_784511, 4)) < int32_t(sext(fB_784511, 4))) goto L_784531;
    eax = rd32(m, edi + 0x48u);
    fR_78451d = (rd8(m, eax + 0x5u) & 0x3u);
    if ((fR_78451d == 0)) goto L_784531;
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, esp + 0x18u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x783fb0u, 0x78452eu);  // FUN_00783fb0
    esp = esp + 0x8u;
L_784531:
    eax = rd32(m, edi + 0x28u);
    edx = rd32(m, edi + 0x14u);
    ebx = rd32(m, edi + 0x8u);
    ebp = ebx;
    fA_78453c = eax;
    fB_78453c = edx;
    if (fA_78453c > fB_78453c) goto L_78454f;
L_784540:
    ecx = rd32(m, eax + 0x8u);
    fA_784543 = ebp;
    fB_784543 = ecx;
    if (fA_784543 < fB_784543) ebp = ecx;
    eax = eax + 0x18u;
    fA_78454b = eax;
    fB_78454b = edx;
    if (fA_78454b <= fB_78454b) goto L_784540;
L_78454f:
    esi = rd32(m, edi + 0x20u);
    fA_784552 = esi;
    fB_784552 = ebx;
    if (fA_784552 >= fB_784552) goto L_78457a;
L_784556:
    fA_784556 = rd32(m, esi + 0x4u);
    fB_784556 = 0x4u;
    if (int32_t(sext(fA_784556, 4)) < int32_t(sext(fB_784556, 4))) goto L_784572;
    eax = rd32(m, esi);
    fR_78455e = (rd8(m, eax + 0x5u) & 0x3u);
    if ((fR_78455e == 0)) goto L_784572;
    ecx = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x783fb0u, 0x78456fu);  // FUN_00783fb0
    esp = esp + 0x8u;
L_784572:
    esi = esi + 0x8u;
    fA_784575 = esi;
    fB_784575 = rd32(m, edi + 0x8u);
    if (fA_784575 < fB_784575) goto L_784556;
L_78457a:
    fA_78457a = esi;
    fB_78457a = ebp;
    if (fA_78457a > fB_78457a) goto L_78458a;
    eax = 0u;
L_784580:
    wr32(m, esi + 0x4u, eax);
    esi = esi + 0x8u;
    fA_784586 = esi;
    fB_784586 = ebp;
    if (fA_784586 <= fB_784586) goto L_784580;
L_78458a:
    eax = ebp;
    ecx = edi;
    LIFT_CALL(0x784490u, 0x784593u);  // FUN_00784490
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 007845a0 Lua_GcPropagateMark: Lua 5.1 propagatemark: traverses the next gray object (table, function, thread, proto).
//   original FUN_007845a0; register inputs: ecx eax; returns with ret 0
REFLEX_PORT_FN(0x007845a0, Lua_GcPropagateMark, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7845b1 = 0;
    uint32_t fB_7845b1 = 0;
    uint32_t fR_7845d1 = 0;
    uint32_t fA_78460a = 0;
    uint32_t fB_78460a = 0;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, eax + 0x24u);
    ecx = rd8(m, esi + 0x4u);
    wr8(m, esi + 0x5u, (rd8(m, esi + 0x5u) | 0x4u));
    ecx = ecx + 0xfffffffbu;
    esp -= 4; wr32(m, esp, edi);
    fA_7845b1 = ecx;
    fB_7845b1 = 0x4u;
    if (fA_7845b1 > fB_7845b1) goto L_78468a;
    switch (ecx) { case 0: goto L_7845c1; case 1: goto L_7845f7; case 2: goto L_78468a; case 3: goto L_784626; case 4: goto L_784658; default: throw std::runtime_error("jump table"); }
L_7845c1:
    ecx = rd32(m, esi + 0x18u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, eax + 0x24u, ecx);
    LIFT_CALL(0x784160u, 0x7845ceu);  // FUN_00784160
    esp = esp + 0x8u;
    fR_7845d1 = (eax & eax);
    if ((fR_7845d1 == 0)) goto L_7845d9;
    wr8(m, esi + 0x5u, (rd8(m, esi + 0x5u) & 0xfbu));
L_7845d9:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, esi + 0x7u)) & 0xffu);
    eax = 0x1u;
    eax = (eax << ((ecx & 0xffu) & 31u));
    edx = (eax + eax * 4);
    eax = rd32(m, esi + 0x1cu);
    ecx = (eax * 8 + 0x20u);
    eax = (ecx + edx * 4);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_7845f7:
    edx = rd32(m, esi + 0x8u);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, eax + 0x24u, edx);
    LIFT_CALL(0x7843f0u, 0x784603u);  // FUN_007843f0
    eax = rd8(m, esi + 0x7u);
    esp = esp + 0x4u;
    fA_78460a = rd8(m, esi + 0x6u);
    fB_78460a = 0x0u;
    if (fA_78460a == fB_78460a) goto L_78461b;
    eax = (eax * 8 + 0x14u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_78461b:
    eax = (eax * 4 + 0x14u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_784626:
    ecx = rd32(m, esi + 0x5cu);
    edx = rd32(m, eax + 0x28u);
    wr32(m, eax + 0x24u, ecx);
    wr32(m, esi + 0x5cu, edx);
    wr32(m, eax + 0x28u, esi);
    wr8(m, esi + 0x5u, (rd8(m, esi + 0x5u) & 0xfbu));
    esp -= 4; wr32(m, esp, eax);
    edi = esi;
    LIFT_CALL(0x784510u, 0x784641u);  // FUN_00784510
    eax = rd32(m, esi + 0x30u);
    eax = (eax + eax * 2);
    esp = esp + 0x4u;
    eax = eax + rd32(m, esi + 0x2cu);
    eax = (eax * 8 + 0x68u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_784658:
    ecx = rd32(m, esi + 0x44u);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, eax + 0x24u, ecx);
    LIFT_CALL(0x784320u, 0x784664u);  // FUN_00784320
    eax = rd32(m, esi + 0x38u);
    edx = (eax + eax * 2);
    eax = rd32(m, esi + 0x28u);
    eax = (edx + eax * 2);
    eax = eax + rd32(m, esi + 0x34u);
    esp = esp + 0x4u;
    eax = eax + rd32(m, esi + 0x30u);
    eax = eax + rd32(m, esi + 0x2cu);
    eax = eax + rd32(m, esi + 0x24u);
    eax = (eax * 4 + 0x4cu);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_78468a:
    edi = rd32(m, esp); esp += 4;
    eax = 0u;
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 007846b0 Fn_007846b0
//   original FUN_007846b0; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x007846b0, Fn_007846b0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_7846b5 = 0;
    uint32_t fR_7846c5 = 0;
    uint32_t fR_7846ce = 0;
    uint32_t fA_7846da = 0;
    uint32_t fB_7846da = 0;
    uint32_t fR_7846f0 = 0;
    uint32_t fA_7846f5 = 0;
    uint32_t fB_7846f5 = 0;
    uint32_t fR_7846fa = 0;
    uint32_t fR_784706 = 0;
    uint32_t fR_784714 = 0;
    uint32_t fA_784729 = 0;
    uint32_t fB_784729 = 0;
    uint32_t fA_784732 = 0;
    uint32_t fB_784732 = 0;
    uint32_t fR_784741 = 0;
    uint32_t fA_78474a = 0;
    uint32_t fB_78474a = 0;
    uint32_t fR_78475d = 0;
    uint32_t fA_784762 = 0;
    uint32_t fB_784762 = 0;
    uint32_t fR_784767 = 0;
    uint32_t fA_78476c = 0;
    uint32_t fB_78476c = 0;
    uint32_t fR_78477f = 0;
    uint32_t fR_784786 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x8u);
    fR_7846b5 = (ebp & ebp);
    if ((fR_7846b5 == 0)) goto L_784791;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    ebx = 0x4u;
L_7846c5:
    fR_7846c5 = (rd8(m, ebp + 0x5u) & 0x10u);
    esi = rd32(m, ebp + 0x1cu);
    if ((fR_7846c5 == 0)) goto L_78470a;
    fR_7846ce = (esi & esi);
    if ((fR_7846ce == 0)) goto L_78470a;
L_7846d2:
    eax = rd32(m, ebp + 0xcu);
    esi = (esi - 1u);
    ecx = rd32(m, eax + esi * 8 + 0x4u);
    fA_7846da = ecx;
    fB_7846da = ebx;
    eax = (eax + esi * 8);
    if (int32_t(sext(fA_7846da, 4)) < int32_t(sext(fB_7846da, 4))) goto L_784706;
    if (fA_7846da != fB_7846da) goto L_7846eb;
    eax = rd32(m, eax);
    wr8(m, eax + 0x5u, (rd8(m, eax + 0x5u) & 0xfcu));
    goto L_784706;
L_7846eb:
    edx = rd32(m, eax);
    edx = (edx & 0xffffff00u) | ((rd8(m, edx + 0x5u)) & 0xffu);
    fR_7846f0 = ((edx & 0xffu) & 0x3u);
    if (!(fR_7846f0 == 0)) goto L_7846ff;
    fA_7846f5 = ecx;
    fB_7846f5 = 0x7u;
    if (fA_7846f5 != fB_7846f5) goto L_784706;
    fR_7846fa = ((edx & 0xffu) & 0x8u);
    if ((fR_7846fa == 0)) goto L_784706;
L_7846ff:
    wr32(m, eax + 0x4u, 0x0u);
L_784706:
    fR_784706 = (esi & esi);
    if (!(fR_784706 == 0)) goto L_7846d2;
L_78470a:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, ebp + 0x7u)) & 0xffu);
    esi = 0x1u;
    esi = (esi << ((ecx & 0xffu) & 31u));
    fR_784714 = (esi & esi);
    if ((fR_784714 == 0)) goto L_784783;
    edi = (esi + esi * 4);
    edi = edi + edi;
    edi = edi + edi;
L_784720:
    eax = rd32(m, ebp + 0x10u);
    edi = edi - 0x14u;
    eax = eax + edi;
    esi = (esi - 1u);
    fA_784729 = rd32(m, eax + 0x4u);
    fB_784729 = 0x0u;
    if (fA_784729 == fB_784729) goto L_78477f;
    ecx = rd32(m, eax + 0xcu);
    fA_784732 = ecx;
    fB_784732 = ebx;
    if (int32_t(sext(fA_784732, 4)) < int32_t(sext(fB_784732, 4))) goto L_784747;
    ecx = rd32(m, eax + 0x8u);
    if (fA_784732 != fB_784732) goto L_784741;
    wr8(m, ecx + 0x5u, (rd8(m, ecx + 0x5u) & 0xfcu));
    goto L_784747;
L_784741:
    fR_784741 = (rd8(m, ecx + 0x5u) & 0x3u);
    if (!(fR_784741 == 0)) goto L_78476c;
L_784747:
    ecx = rd32(m, eax + 0x4u);
    fA_78474a = ecx;
    fB_78474a = ebx;
    if (int32_t(sext(fA_78474a, 4)) < int32_t(sext(fB_78474a, 4))) goto L_78477f;
    if (fA_78474a != fB_78474a) goto L_784758;
    eax = rd32(m, eax);
    wr8(m, eax + 0x5u, (rd8(m, eax + 0x5u) & 0xfcu));
    goto L_78477f;
L_784758:
    edx = rd32(m, eax);
    edx = (edx & 0xffffff00u) | ((rd8(m, edx + 0x5u)) & 0xffu);
    fR_78475d = ((edx & 0xffu) & 0x3u);
    if (!(fR_78475d == 0)) goto L_78476c;
    fA_784762 = ecx;
    fB_784762 = 0x7u;
    if (fA_784762 != fB_784762) goto L_78477f;
    fR_784767 = ((edx & 0xffu) & 0x8u);
    if ((fR_784767 == 0)) goto L_78477f;
L_78476c:
    fA_78476c = rd32(m, eax + 0xcu);
    fB_78476c = ebx;
    wr32(m, eax + 0x4u, 0x0u);
    if (int32_t(sext(fA_78476c, 4)) < int32_t(sext(fB_78476c, 4))) goto L_78477f;
    wr32(m, eax + 0xcu, 0xbu);
L_78477f:
    fR_78477f = (esi & esi);
    if (!(fR_78477f == 0)) goto L_784720;
L_784783:
    ebp = rd32(m, ebp + 0x18u);
    fR_784786 = (ebp & ebp);
    if (!(fR_784786 == 0)) goto L_7846c5;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
L_784791:
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00784870 Lua_GcSweepList: Lua 5.1 sweeplist.
//   original FUN_00784870; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x00784870, Lua_GcSweepList, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_784890 = 0;
    uint32_t fR_78489d = 0;
    uint32_t fA_7848a1 = 0;
    uint32_t fB_7848a1 = 0;
    uint32_t fR_7848c6 = 0;
    uint32_t fA_7848df = 0;
    uint32_t fB_7848df = 0;
    uint32_t fR_7848f5 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp -= 4; wr32(m, esp, ecx);
    eax = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, eax + 0x10u);
    eax = rd8(m, ebx + 0x14u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, ebp + 0xcu);
    esi = rd32(m, edi);
    eax = (eax ^ 0x3u);
    wr32(m, esp + 0xcu, eax);
    fR_784890 = (esi & esi);
    if ((fR_784890 == 0)) goto L_7848f9;
L_784894:
    eax = rd32(m, ebp + 0x10u);
    ecx = eax;
    eax = (eax - 1u);
    wr32(m, ebp + 0x10u, eax);
    fR_78489d = (ecx & ecx);
    if ((false || (fR_78489d == 0))) goto L_7848f9;
    fA_7848a1 = rd8(m, esi + 0x4u);
    fB_7848a1 = 0x8u;
    if (fA_7848a1 != fB_7848a1) goto L_7848b9;
    eax = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, 0xfffffffdu);
    edx = (esi + 0x58u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x784870u, 0x7848b6u);  // FUN_00784870
    esp = esp + 0xcu;
L_7848b9:
    eax = (eax & 0xffffff00u) | ((rd8(m, esi + 0x5u)) & 0xffu);
    edx = rd32(m, esp + 0xcu);
    ecx = (eax & 0xffu);
    ecx = (ecx ^ 0x3u);
    fR_7848c6 = (edx & ecx);
    if ((fR_7848c6 == 0)) goto L_7848db;
    ecx = (ecx & 0xffffff00u) | ((rd8(m, ebx + 0x14u)) & 0xffu);
    eax = (eax & 0xffffff00u) | ((((eax & 0xffu) & 0xf8u)) & 0xffu);
    ecx = (ecx & 0xffffff00u) | ((((ecx & 0xffu) & 0x3u)) & 0xffu);
    eax = (eax & 0xffffff00u) | ((((eax & 0xffu) | (ecx & 0xffu))) & 0xffu);
    wr8(m, esi + 0x5u, (eax & 0xffu));
    edi = esi;
    goto L_7848f3;
L_7848db:
    edx = rd32(m, esi);
    wr32(m, edi, edx);
    fA_7848df = esi;
    fB_7848df = rd32(m, ebx + 0x1cu);
    if (fA_7848df != fB_7848df) goto L_7848e9;
    eax = rd32(m, esi);
    wr32(m, ebx + 0x1cu, eax);
L_7848e9:
    ecx = rd32(m, ebp + 0x8u);
    eax = esi;
    LIFT_CALL(0x7847a0u, 0x7848f3u);  // FUN_007847a0
L_7848f3:
    esi = rd32(m, edi);
    fR_7848f5 = (esi & esi);
    if (!(fR_7848f5 == 0)) goto L_784894;
L_7848f9:
    eax = edi;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00784910 Fn_00784910
//   original FUN_00784910; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x00784910, Fn_00784910, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_78492e = 0;
    uint32_t fB_78492e = 0;
    uint32_t fA_784933 = 0;
    uint32_t fB_784933 = 0;
    uint32_t fA_78494c = 0;
    uint32_t fB_78494c = 0;
    uint32_t fA_784958 = 0;
    uint32_t fB_784958 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, ebx + 0x10u);
    ecx = rd32(m, esi + 0x8u);
    eax = ecx;
    edx = (eax >> 31) ? 0xffffffffu : 0;
    edx = (edx & 0x3u);
    eax = eax + edx;
    eax = uint32_t(int32_t(eax) >> 2);
    esp -= 4; wr32(m, esp, edi);
    fA_78492e = rd32(m, esi + 0x4u);
    fB_78492e = eax;
    if (fA_78492e >= fB_78492e) goto L_784949;
    fA_784933 = ecx;
    fB_784933 = 0x40u;
    if (int32_t(sext(fA_784933, 4)) <= int32_t(sext(fB_784933, 4))) goto L_784949;
    eax = ecx;
    edx = (eax >> 31) ? 0xffffffffu : 0;
    eax = eax - edx;
    eax = uint32_t(int32_t(eax) >> 1);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x7882a0u, 0x784946u);  // FUN_007882a0
    esp = esp + 0x8u;
L_784949:
    eax = rd32(m, esi + 0x3cu);
    fA_78494c = eax;
    fB_78494c = 0x40u;
    if (fA_78494c <= fB_78494c) goto L_78498f;
    edi = eax;
    edi = (edi >> 1);
    ecx = (edi + 0x1u);
    fA_784958 = ecx;
    fB_784958 = 0xfffffffdu;
    if (fA_784958 > fB_784958) goto L_784979;
    edx = rd32(m, esi + 0x34u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x78a0a0u, 0x784969u);  // FUN_0078a0a0
    esp = esp + 0x10u;
    wr32(m, esi + 0x34u, eax);
    wr32(m, esi + 0x3cu, edi);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_784979:
    esp -= 4; wr32(m, esp, 0x96ee60u);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x77d680u, 0x784984u);  // FUN_0077d680
    esp = esp + 0x8u;
    eax = 0u;
    wr32(m, esi + 0x34u, eax);
    wr32(m, esi + 0x3cu, edi);
L_78498f:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00784ab0 Fn_00784ab0
//   original FUN_00784ab0; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00784ab0, Fn_00784ab0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_784ac2 = 0;
    uint32_t fR_784ac6 = 0;
    uint32_t fA_784ad9 = 0;
    uint32_t fB_784ad9 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    esi = (ebp + 0x7cu);
    edi = 0x9u;
L_784ac0:
    eax = rd32(m, esi);
    fR_784ac2 = (eax & eax);
    if ((fR_784ac2 == 0)) goto L_784ad6;
    fR_784ac6 = (rd8(m, eax + 0x5u) & 0x3u);
    if ((fR_784ac6 == 0)) goto L_784ad6;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ebp);
    LIFT_CALL(0x783fb0u, 0x784ad3u);  // FUN_00783fb0
    esp = esp + 0x8u;
L_784ad6:
    esi = esi + 0x4u;
    fA_784ad9 = edi;
    fB_784ad9 = 0x1u;
    edi = fA_784ad9 - fB_784ad9;
    if (fA_784ad9 != fB_784ad9) goto L_784ac0;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00784af0 Lua_GcMarkRoot: Lua 5.1 markroot.
//   original FUN_00784af0; register inputs: edi; returns with ret 0
REFLEX_PORT_FN(0x00784af0, Lua_GcMarkRoot, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_784b02 = 0;
    uint32_t fA_784b15 = 0;
    uint32_t fB_784b15 = 0;
    uint32_t fR_784b1e = 0;
    uint32_t fA_784b31 = 0;
    uint32_t fB_784b31 = 0;
    uint32_t fR_784b3a = 0;
    eax = 0u;
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, edi + 0x10u);
    wr32(m, esi + 0x24u, eax);
    wr32(m, esi + 0x28u, eax);
    wr32(m, esi + 0x2cu, eax);
    eax = rd32(m, esi + 0x64u);
    fR_784b02 = (rd8(m, eax + 0x5u) & 0x3u);
    if ((fR_784b02 == 0)) goto L_784b12;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x783fb0u, 0x784b0fu);  // FUN_00783fb0
    esp = esp + 0x8u;
L_784b12:
    eax = rd32(m, esi + 0x64u);
    fA_784b15 = rd32(m, eax + 0x4cu);
    fB_784b15 = 0x4u;
    if (int32_t(sext(fA_784b15, 4)) < int32_t(sext(fB_784b15, 4))) goto L_784b2e;
    eax = rd32(m, eax + 0x48u);
    fR_784b1e = (rd8(m, eax + 0x5u) & 0x3u);
    if ((fR_784b1e == 0)) goto L_784b2e;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x783fb0u, 0x784b2bu);  // FUN_00783fb0
    esp = esp + 0x8u;
L_784b2e:
    eax = rd32(m, edi + 0x10u);
    fA_784b31 = rd32(m, eax + 0x60u);
    fB_784b31 = 0x4u;
    if (int32_t(sext(fA_784b31, 4)) < int32_t(sext(fB_784b31, 4))) goto L_784b4a;
    eax = rd32(m, eax + 0x5cu);
    fR_784b3a = (rd8(m, eax + 0x5u) & 0x3u);
    if ((fR_784b3a == 0)) goto L_784b4a;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x783fb0u, 0x784b47u);  // FUN_00783fb0
    esp = esp + 0x8u;
L_784b4a:
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x784ab0u, 0x784b50u);  // FUN_00784ab0
    esp = esp + 0x4u;
    wr8(m, esi + 0x15u, 0x1u);
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00784b60 Fn_00784b60
//   original FUN_00784b60; register inputs: edi; returns with ret 0
REFLEX_PORT_FN(0x00784b60, Fn_00784b60, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_784b68 = 0;
    uint32_t fB_784b68 = 0;
    uint32_t fR_784b70 = 0;
    uint32_t fA_784b79 = 0;
    uint32_t fB_784b79 = 0;
    uint32_t fR_784b81 = 0;
    uint32_t fA_784b94 = 0;
    uint32_t fB_784b94 = 0;
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, edi + 0x78u);
    ebp = (edi + 0x68u);
    fA_784b68 = esi;
    fB_784b68 = ebp;
    if (fA_784b68 == fB_784b68) goto L_784b98;
L_784b70:
    fR_784b70 = (rd8(m, esi + 0x5u) & 0x7u);
    if (!(fR_784b70 == 0)) goto L_784b91;
    eax = rd32(m, esi + 0x8u);
    fA_784b79 = rd32(m, eax + 0x4u);
    fB_784b79 = 0x4u;
    if (int32_t(sext(fA_784b79, 4)) < int32_t(sext(fB_784b79, 4))) goto L_784b91;
    eax = rd32(m, eax);
    fR_784b81 = (rd8(m, eax + 0x5u) & 0x3u);
    if ((fR_784b81 == 0)) goto L_784b91;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x783fb0u, 0x784b8eu);  // FUN_00783fb0
    esp = esp + 0x8u;
L_784b91:
    esi = rd32(m, esi + 0x10u);
    fA_784b94 = esi;
    fB_784b94 = ebp;
    if (fA_784b94 != fB_784b94) goto L_784b70;
L_784b98:
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00784ba0 Lua_GcAtomic: Lua 5.1 atomic.
//   original FUN_00784ba0; register inputs: eax; returns with ret 0
REFLEX_PORT_FN(0x00784ba0, Lua_GcAtomic, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_784bad = 0;
    uint32_t fB_784bad = 0;
    uint32_t fA_784bba = 0;
    uint32_t fB_784bba = 0;
    uint32_t fR_784bcd = 0;
    uint32_t fA_784be6 = 0;
    uint32_t fB_784be6 = 0;
    uint32_t fA_784bf7 = 0;
    uint32_t fB_784bf7 = 0;
    uint32_t fR_784c0a = 0;
    uint32_t fA_784c17 = 0;
    uint32_t fB_784c17 = 0;
    uint32_t fR_784c2d = 0;
    uint32_t fA_784c4d = 0;
    uint32_t fB_784c4d = 0;
    uint32_t fA_784c54 = 0;
    uint32_t fB_784c54 = 0;
    uint32_t fA_784c69 = 0;
    uint32_t fB_784c69 = 0;
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esi + 0x10u);
    LIFT_CALL(0x784b60u, 0x784badu);  // FUN_00784b60
    fA_784bad = rd32(m, edi + 0x24u);
    fB_784bad = 0x0u;
    if (fA_784bad == fB_784bad) goto L_784bc0;
L_784bb3:
    eax = edi;
    LIFT_CALL(0x7845a0u, 0x784bbau);  // FUN_007845a0
    fA_784bba = rd32(m, edi + 0x24u);
    fB_784bba = 0x0u;
    if (fA_784bba != fB_784bba) goto L_784bb3;
L_784bc0:
    eax = rd32(m, edi + 0x2cu);
    wr32(m, edi + 0x24u, eax);
    wr32(m, edi + 0x2cu, 0x0u);
    fR_784bcd = (rd8(m, esi + 0x5u) & 0x3u);
    if ((fR_784bcd == 0)) goto L_784bdd;
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x783fb0u, 0x784bdau);  // FUN_00783fb0
    esp = esp + 0x8u;
L_784bdd:
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x784ab0u, 0x784be3u);  // FUN_00784ab0
    esp = esp + 0x4u;
    fA_784be6 = rd32(m, edi + 0x24u);
    fB_784be6 = 0x0u;
    if (fA_784be6 == fB_784be6) goto L_784bfd;
L_784bf0:
    eax = edi;
    LIFT_CALL(0x7845a0u, 0x784bf7u);  // FUN_007845a0
    fA_784bf7 = rd32(m, edi + 0x24u);
    fB_784bf7 = 0x0u;
    if (fA_784bf7 != fB_784bf7) goto L_784bf0;
L_784bfd:
    ecx = rd32(m, edi + 0x28u);
    wr32(m, edi + 0x24u, ecx);
    wr32(m, edi + 0x28u, 0x0u);
    fR_784c0a = (ecx & ecx);
    if ((fR_784c0a == 0)) goto L_784c1d;
    edi = edi;
L_784c10:
    eax = edi;
    LIFT_CALL(0x7845a0u, 0x784c17u);  // FUN_007845a0
    fA_784c17 = rd32(m, edi + 0x24u);
    fB_784c17 = 0x0u;
    if (fA_784c17 != fB_784c17) goto L_784c10;
L_784c1d:
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x784090u, 0x784c25u);  // FUN_00784090
    esi = rd32(m, edi + 0x30u);
    esp = esp + 0x8u;
    ebp = eax;
    fR_784c2d = (esi & esi);
    if ((fR_784c2d == 0)) goto L_784c52;
L_784c31:
    esi = rd32(m, esi);
    edx = (edx & 0xffffff00u) | ((rd8(m, esi + 0x5u)) & 0xffu);
    eax = (eax & 0xffffff00u) | ((rd8(m, edi + 0x14u)) & 0xffu);
    edx = (edx & 0xffffff00u) | ((((edx & 0xffu) & 0xf8u)) & 0xffu);
    eax = (eax & 0xffffff00u) | ((((eax & 0xffu) & 0x3u)) & 0xffu);
    edx = (edx & 0xffffff00u) | ((((edx & 0xffu) | (eax & 0xffu))) & 0xffu);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    wr8(m, esi + 0x5u, (edx & 0xffu));
    LIFT_CALL(0x783fb0u, 0x784c4au);  // FUN_00783fb0
    esp = esp + 0x8u;
    fA_784c4d = esi;
    fB_784c4d = rd32(m, edi + 0x30u);
    if (fA_784c4d != fB_784c4d) goto L_784c31;
L_784c52:
    esi = 0u;
    fA_784c54 = rd32(m, edi + 0x24u);
    fB_784c54 = esi;
    if (fA_784c54 == fB_784c54) goto L_784c6f;
L_784c60:
    eax = edi;
    LIFT_CALL(0x7845a0u, 0x784c67u);  // FUN_007845a0
    esi = esi + eax;
    fA_784c69 = rd32(m, edi + 0x24u);
    fB_784c69 = 0x0u;
    if (fA_784c69 != fB_784c69) goto L_784c60;
L_784c6f:
    ecx = rd32(m, edi + 0x2cu);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x7846b0u, 0x784c78u);  // FUN_007846b0
    eax = rd32(m, edi + 0x44u);
    wr8(m, edi + 0x14u, (rd8(m, edi + 0x14u) ^ 0x3u));
    eax = eax - esi;
    esp = esp + 0x4u;
    edx = (edi + 0x1cu);
    eax = eax - ebp;
    wr32(m, edi + 0x18u, 0x0u);
    wr32(m, edi + 0x20u, edx);
    wr8(m, edi + 0x15u, 0x2u);
    wr32(m, edi + 0x48u, eax);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00784ca0 Lua_GcSingleStep: Lua 5.1 singlestep (EAX lua_State): the collector state machine (markroot, propagate / atomic, sweepstring, sweep, finalize).
//   original FUN_00784ca0; register inputs: eax; returns with ret 0
REFLEX_PORT_FN(0x00784ca0, Lua_GcSingleStep, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_784cac = 0;
    uint32_t fB_784cac = 0;
    uint32_t fA_784cc7 = 0;
    uint32_t fB_784cc7 = 0;
    uint32_t fA_784d02 = 0;
    uint32_t fB_784d02 = 0;
    uint32_t fA_784d31 = 0;
    uint32_t fB_784d31 = 0;
    uint32_t fA_784d56 = 0;
    uint32_t fB_784d56 = 0;
    uint32_t fA_784d63 = 0;
    uint32_t fB_784d63 = 0;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = eax;
    esi = rd32(m, edi + 0x10u);
    eax = rd8(m, esi + 0x15u);
    fA_784cac = eax;
    fB_784cac = 0x4u;
    if (fA_784cac > fB_784cac) goto L_784d7d;
    switch (eax) { case 0: goto L_784cbc; case 1: goto L_784cc7; case 2: goto L_784ce4; case 3: goto L_784d1c; case 4: goto L_784d54; default: throw std::runtime_error("jump table"); }
L_784cbc:
    LIFT_CALL(0x784af0u, 0x784cc1u);  // FUN_00784af0
    eax = 0u;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_784cc7:
    fA_784cc7 = rd32(m, esi + 0x24u);
    fB_784cc7 = 0x0u;
    if (fA_784cc7 == fB_784cc7) goto L_784cd7;
    eax = esi;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_TAILJMP(0x7845a0u);  // FUN_007845a0
L_784cd7:
    eax = edi;
    LIFT_CALL(0x784ba0u, 0x784cdeu);  // FUN_00784ba0
    eax = 0u;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_784ce4:
    eax = rd32(m, esi + 0x18u);
    ecx = rd32(m, esi);
    ebx = rd32(m, esi + 0x44u);
    ecx = (ecx + eax * 4);
    esp -= 4; wr32(m, esp, 0xfffffffdu);
    esp -= 4; wr32(m, esp, ecx);
    eax = (eax + 1u);
    esp -= 4; wr32(m, esp, edi);
    wr32(m, esi + 0x18u, eax);
    LIFT_CALL(0x784870u, 0x784cfcu);  // FUN_00784870
    edx = rd32(m, esi + 0x18u);
    esp = esp + 0xcu;
    fA_784d02 = edx;
    fB_784d02 = rd32(m, esi + 0x8u);
    if (int32_t(sext(fA_784d02, 4)) < int32_t(sext(fB_784d02, 4))) goto L_784d0b;
    wr8(m, esi + 0x15u, 0x3u);
L_784d0b:
    eax = rd32(m, esi + 0x44u);
    eax = eax - ebx;
    wr32(m, esi + 0x48u, rd32(m, esi + 0x48u) + eax);
    eax = 0xau;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_784d1c:
    ecx = rd32(m, esi + 0x20u);
    ebx = rd32(m, esi + 0x44u);
    esp -= 4; wr32(m, esp, 0x28u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x784870u, 0x784d2bu);  // FUN_00784870
    esp = esp + 0xcu;
    wr32(m, esi + 0x20u, eax);
    fA_784d31 = rd32(m, eax);
    fB_784d31 = 0x0u;
    if (fA_784d31 != fB_784d31) goto L_784d43;
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x784910u, 0x784d3cu);  // FUN_00784910
    esp = esp + 0x4u;
    wr8(m, esi + 0x15u, 0x4u);
L_784d43:
    edx = rd32(m, esi + 0x44u);
    edx = edx - ebx;
    wr32(m, esi + 0x48u, rd32(m, esi + 0x48u) + edx);
    eax = 0x190u;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_784d54:
    eax = 0u;
    fA_784d56 = rd32(m, esi + 0x30u);
    fB_784d56 = eax;
    if (fA_784d56 == fB_784d56) goto L_784d77;
    LIFT_CALL(0x7849a0u, 0x784d60u);  // FUN_007849a0
    eax = rd32(m, esi + 0x48u);
    fA_784d63 = eax;
    fB_784d63 = 0x64u;
    if (fA_784d63 <= fB_784d63) goto L_784d6e;
    eax = eax + 0xffffff9cu;
    wr32(m, esi + 0x48u, eax);
L_784d6e:
    eax = 0x64u;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_784d77:
    wr8(m, esi + 0x15u, (eax & 0xffu));
    wr32(m, esi + 0x4cu, eax);
L_784d7d:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00784da0 Lua_GcStep: Lua 5.1 luaC_step: gcdept += totalbytes - GCthreshold, single steps for gcstepmul * 10, then the next threshold.
//   original FUN_00784da0; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x00784da0, Lua_GcStep, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_784db6 = 0;
    uint32_t fB_784db6 = 0;
    uint32_t fA_784dd1 = 0;
    uint32_t fB_784dd1 = 0;
    uint32_t fR_784dd7 = 0;
    uint32_t fA_784ddb = 0;
    uint32_t fB_784ddb = 0;
    uint32_t fA_784de4 = 0;
    uint32_t fB_784de4 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, ebx + 0x10u);
    eax = rd32(m, esi + 0x54u);
    esp -= 4; wr32(m, esp, edi);
    edi = (eax + eax * 4);
    fA_784db6 = edi;
    fB_784db6 = edi;
    edi = fA_784db6 + fB_784db6;
    if (!(((fA_784db6 + fB_784db6)) == 0)) goto L_784dbf;
    edi = 0x7ffffffeu;
L_784dbf:
    eax = rd32(m, esi + 0x44u);
    eax = eax - rd32(m, esi + 0x40u);
    wr32(m, esi + 0x4cu, rd32(m, esi + 0x4cu) + eax);
L_784dc8:
    eax = ebx;
    LIFT_CALL(0x784ca0u, 0x784dcfu);  // FUN_00784ca0
    edi = edi - eax;
    fA_784dd1 = rd8(m, esi + 0x15u);
    fB_784dd1 = 0x0u;
    if (fA_784dd1 == fB_784dd1) goto L_784e13;
    fR_784dd7 = (edi & edi);
    if ((!(fR_784dd7 == 0) && sgn(fR_784dd7, 4) == false)) goto L_784dc8;
    fA_784ddb = rd8(m, esi + 0x15u);
    fB_784ddb = 0x0u;
    if (fA_784ddb == fB_784ddb) goto L_784e13;
    eax = rd32(m, esi + 0x4cu);
    fA_784de4 = eax;
    fB_784de4 = 0x400u;
    if (fA_784de4 >= fB_784de4) goto L_784dfe;
    ecx = rd32(m, esi + 0x44u);
    ecx = ecx + 0x400u;
    wr32(m, esi + 0x40u, ecx);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_784dfe:
    edx = rd32(m, esi + 0x44u);
    eax = eax + 0xfffffc00u;
    wr32(m, esi + 0x4cu, eax);
    wr32(m, esi + 0x40u, edx);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_784e13:
    eax = 0x51eb851fu;
    { uint64_t p_ = uint64_t(eax) * uint64_t(rd32(m, esi + 0x48u)); eax = uint32_t(p_); edx = uint32_t(p_ >> 32); }
    edx = (edx >> 5);
    edx = uint32_t(int32_t(edx) * int32_t(rd32(m, esi + 0x50u)));
    edi = rd32(m, esp); esp += 4;
    wr32(m, esi + 0x40u, edx);
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0078fcf0 Fn_0078fcf0
//   original FUN_0078fcf0; register inputs: eax edi; returns with ret 0
REFLEX_PORT_FN(0x0078fcf0, Fn_0078fcf0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_78fcfe = 0;
    uint32_t fA_78fd1a = 0;
    uint32_t fB_78fd1a = 0;
    uint32_t fR_78fd2a = 0;
    uint32_t fA_78fd3b = 0;
    uint32_t fB_78fd3b = 0;
    uint32_t fR_78fd4a = 0;
    ecx = rd8(m, edi);
    edx = (edx & 0xffffff00u) | ((rd8(m, ecx + 0x9b77c8u)) & 0xffu);
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    fR_78fcfe = ((ecx & 0xffu) & (ecx & 0xffu));
    if ((fR_78fcfe == 0)) goto L_78fd4e;
    ebp = sext((edx & 0xffu), 1);
    goto L_78fd10;
L_78fd10:
    edx = sext((ecx & 0xffu), 1);
    ecx = rd8(m, edx + 0x9b77c8u);
    fA_78fd1a = ecx;
    fB_78fd1a = ebp;
    if (fA_78fd1a != fB_78fd1a) goto L_78fd46;
    ecx = (edi + 0x1u);
    edx = eax;
    edx = edx - ecx;
    esi = (edx + 0x1u);
L_78fd28:
    edx = (edx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    fR_78fd2a = ((edx & 0xffu) & (edx & 0xffu));
    if ((fR_78fd2a == 0)) goto L_78fd50;
    ebx = rd8(m, esi + ecx);
    ebx = (ebx & 0xffffff00u) | ((rd8(m, ebx + 0x9b77c8u)) & 0xffu);
    edx = (edx & 0xffu);
    fA_78fd3b = (ebx & 0xffu);
    fB_78fd3b = rd8(m, edx + 0x9b77c8u);
    if (fA_78fd3b != fB_78fd3b) goto L_78fd46;
    ecx = (ecx + 1u);
    goto L_78fd28;
L_78fd46:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    eax = (eax + 1u);
    fR_78fd4a = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_78fd4a == 0)) goto L_78fd10;
L_78fd4e:
    eax = 0u;
L_78fd50:
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0078fd60 Fn_0078fd60
//   original FUN_0078fd60; register inputs: esi edi; returns with ret 0
REFLEX_PORT_FN(0x0078fd60, Fn_0078fd60, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_78fd75 = 0;
    uint32_t fA_78fd79 = 0;
    uint32_t fB_78fd79 = 0;
    ecx = rd32(m, esp + 0x4u);
    eax = (esp + 0x8u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(rd32(m, 0x9162acu) /* 8.286934398594744e-39 */, 0x78fd72u);
    esp = esp + 0x10u;
    fR_78fd75 = (eax & eax);
    if (((fR_78fd75 == 0) || sgn(fR_78fd75, 4) != false)) goto L_78fd82;
    fA_78fd79 = eax;
    fB_78fd79 = esi;
    if (fA_78fd79 >= fB_78fd79) goto L_78fd82;
    wr8(m, eax + edi, 0x0u);
    LIFT_RET(0);
L_78fd82:
    wr8(m, edi + esi - 0x1u, 0x0u);
    LIFT_RET(0);
}

// 0078fd90 Fn_0078fd90
//   original FUN_0078fd90; register inputs: ecx esi edi; returns with ret 0
REFLEX_PORT_FN(0x0078fd90, Fn_0078fd90, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = (esi - 0x1u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(rd32(m, 0x916260u) /* 8.286626112932593e-39 */, 0x78fd9cu);
    esp = esp + 0xcu;
    wr8(m, edi + esi - 0x1u, 0x0u);
    LIFT_RET(0);
}

// 0078fde0 Fn_0078fde0
//   original FUN_0078fde0; register inputs: edi; returns with ret 0
REFLEX_PORT_FN(0x0078fde0, Fn_0078fde0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_78fdf3 = 0;
    uint32_t fR_78fe03 = 0;
    uint32_t fA_78fe18 = 0;
    uint32_t fR_78fe18 = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x8u);
    eax = ebx;
    esp -= 4; wr32(m, esp, esi);
    edx = (eax + 0x1u);
    goto L_78fdf0;
L_78fdf0:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    eax = (eax + 1u);
    fR_78fdf3 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_78fdf3 == 0)) goto L_78fdf0;
    eax = eax - edx;
    ecx = eax;
    eax = edi;
    esi = (eax + 0x1u);
L_78fe00:
    edx = (edx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    eax = (eax + 1u);
    fR_78fe03 = ((edx & 0xffu) & (edx & 0xffu));
    if (!(fR_78fe03 == 0)) goto L_78fe00;
    eax = eax - esi;
    ecx = ecx - eax;
    ecx = ecx + ebx;
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(rd32(m, 0x9162bcu) /* 8.287018476502604e-39 */, 0x78fe15u);
    esp = esp + 0x8u;
    fA_78fe18 = eax;
    fR_78fe18 = (0u - fA_78fe18);
    eax = fR_78fe18;
    eax = eax - eax - ((fA_78fe18 != 0) ? 1u : 0u);
    esi = rd32(m, esp); esp += 4;
    eax = (eax + 1u);
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0078ffb0 Fn_0078ffb0
//   original FUN_0078ffb0; register inputs: ecx eax; returns with ret 0
REFLEX_PORT_FN(0x0078ffb0, Fn_0078ffb0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    edx = rd32(m, esp + 0x4u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(rd32(m, 0x916354u) /* 8.287559377709833e-39 */, 0x78ffbdu);
    esp = esp + 0xcu;
    LIFT_RET(0);
}

// 0078ffd0 Fn_0078ffd0
//   original FUN_0078ffd0; register inputs: ecx esi edi; returns with ret 0
REFLEX_PORT_FN(0x0078ffd0, Fn_0078ffd0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = (esi - 0x1u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(rd32(m, 0x916340u) /* 8.287469694608116e-39 */, 0x78ffdcu);
    edx = 0u;
    esp = esp + 0xcu;
    wr16(m, edi + esi * 2 - 0x2u, (edx & 0xffffu));
    LIFT_RET(0);
}

// 00790020 Fn_00790020
//   original FUN_00790020; register inputs: eax esi edi; returns with ret 0
REFLEX_PORT_FN(0x00790020, Fn_00790020, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, 0xffffffffu);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0xfde9u);
    LIFT_CALL(rd32(m, 0x9160a8u) /* 8.295541173762627e-39 */, 0x790036u);
    wr8(m, edi + esi - 0x1u, 0x0u);
    LIFT_RET(0);
}

// 007902b0 Fn_007902b0
//   original FUN_007902b0; register inputs: ecx eax esi; returns with ret 0
REFLEX_PORT_FN(0x007902b0, Fn_007902b0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_7902bb = 0;
    uint32_t fA_7902c1 = 0;
    uint32_t fR_7902c1 = 0;
    wr32(m, eax, ecx);
    ecx = esi;
    esp -= 4; wr32(m, esp, edi);
    edi = (ecx + 0x1u);
L_7902b8:
    edx = (edx & 0xffffff00u) | ((rd8(m, ecx)) & 0xffu);
    ecx = (ecx + 1u);
    fR_7902bb = ((edx & 0xffu) & (edx & 0xffu));
    if (!(fR_7902bb == 0)) goto L_7902b8;
    ecx = ecx - edi;
    fA_7902c1 = ecx;
    fR_7902c1 = (0u - fA_7902c1);
    ecx = fR_7902c1;
    ecx = ecx - ecx - ((fA_7902c1 != 0) ? 1u : 0u);
    ecx = (ecx & esi);
    wr32(m, eax + 0x8u, ecx);
    edi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 007902d0 Fn_007902d0
//   original FUN_007902d0; register inputs: esi; returns with ret 0
REFLEX_PORT_FN(0x007902d0, Fn_007902d0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_7902d3 = 0;
    uint32_t fA_7902d7 = 0;
    uint32_t fB_7902d7 = 0;
    uint32_t fR_7902ed = 0;
    uint32_t fR_790301 = 0;
    uint32_t fA_790308 = 0;
    uint32_t fB_790308 = 0;
    uint32_t fR_790329 = 0;
    uint32_t fA_790336 = 0;
    uint32_t fB_790336 = 0;
    eax = rd32(m, esi + 0x8u);
    fR_7902d3 = (eax & eax);
    if ((fR_7902d3 == 0)) goto L_790315;
    fA_7902d7 = rd32(m, esi);
    fB_7902d7 = 0x0u;
    if (fA_7902d7 == fB_7902d7) goto L_790318;
    eax = sext(rd8(m, eax), 1);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, 0x916248u) /* 8.28654764021859e-39 */;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(ebp, 0x7902eau);
    esp = esp + 0x8u;
    fR_7902ed = (eax & eax);
    if ((fR_7902ed == 0)) goto L_790305;
L_7902f1:
    wr32(m, esi + 0x8u, (rd32(m, esi + 0x8u) + 1u));
    ecx = rd32(m, esi + 0x8u);
    edx = sext(rd8(m, ecx), 1);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(ebp, 0x7902feu);
    esp = esp + 0x8u;
    fR_790301 = (eax & eax);
    if (!(fR_790301 == 0)) goto L_7902f1;
L_790305:
    eax = rd32(m, esi + 0x8u);
    fA_790308 = rd8(m, eax);
    fB_790308 = 0x0u;
    ebp = rd32(m, esp); esp += 4;
    if (fA_790308 != fB_790308) goto L_790318;
    wr32(m, esi + 0x8u, 0x0u);
L_790315:
    eax = 0u;
    LIFT_RET(0);
L_790318:
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esi + 0x4u, eax);
    LIFT_CALL(rd32(m, 0x916284u) /* 8.286763440182097e-39 */, 0x790323u);
    esp = esp + 0x8u;
    wr32(m, esi + 0x8u, eax);
    fR_790329 = (eax & eax);
    if ((fR_790329 == 0)) goto L_790342;
    wr8(m, eax, 0x0u);
    wr32(m, esi + 0x8u, (rd32(m, esi + 0x8u) + 1u));
    eax = rd32(m, esi + 0x8u);
    fA_790336 = rd8(m, eax);
    fB_790336 = 0x0u;
    if (fA_790336 != fB_790336) goto L_790342;
    wr32(m, esi + 0x8u, 0x0u);
L_790342:
    eax = rd32(m, esi + 0x4u);
    LIFT_RET(0);
}

// 00790b30 Fn_00790b30
//   original FUN_00790b30; register inputs: esi edi; returns with ret 0
REFLEX_PORT_FN(0x00790b30, Fn_00790b30, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x10u)));
    ecx = (esi + 0x1a0u);
    wr32(m, esi + 0x10u, fst32(m));
    fpop(m);
    esp -= 4; wr32(m, esp, ecx);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x14u)));
    ecx = edi;
    wr32(m, esi + 0x14u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x18u)));
    wr32(m, esi + 0x18u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x40u)));
    wr32(m, esi + 0x40u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x44u)));
    wr32(m, esi + 0x44u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x48u)));
    wr32(m, esi + 0x48u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x30u)));
    wr32(m, esi + 0x30u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x34u)));
    wr32(m, esi + 0x34u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x38u)));
    wr32(m, esi + 0x38u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x20u)));
    wr32(m, esi + 0x20u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x24u)));
    wr32(m, esi + 0x24u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x28u)));
    wr32(m, esi + 0x28u, fst32(m));
    fpop(m);
    eax = rd32(m, edi);
    edx = rd32(m, eax);
    LIFT_CALL(edx, 0x790b87u);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x1d0u)));
    wr32(m, esi + 0x1d0u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x1d4u)));
    wr32(m, esi + 0x1d4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x1d8u)));
    wr32(m, esi + 0x1d8u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x1c0u)));
    wr32(m, esi + 0x1c0u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x1c4u)));
    wr32(m, esi + 0x1c4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x1c8u)));
    wr32(m, esi + 0x1c8u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x1b0u)));
    wr32(m, esi + 0x1b0u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x1b4u)));
    wr32(m, esi + 0x1b4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x1b8u)));
    wr32(m, esi + 0x1b8u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x50u)));
    wr32(m, esi + 0x50u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x54u)));
    wr32(m, esi + 0x54u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x58u)));
    wr32(m, esi + 0x58u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x5cu)));
    wr32(m, esi + 0x5cu, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x60u)));
    wr32(m, esi + 0x60u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x64u)));
    wr32(m, esi + 0x64u, fst32(m));
    fpop(m);
    eax = rd32(m, edi + 0x68u);
    wr32(m, esi + 0x68u, eax);
    eax = 0u;
    wr32(m, esi + 0xd4u, eax);
    wr32(m, esi + 0xd0u, eax);
    wr32(m, esi + 0xdcu, eax);
    wr32(m, esi + 0xd8u, eax);
    LIFT_RET(0);
}

// 00790cb0 Fn_00790cb0
//   original FUN_00790cb0; register inputs: ecx eax; returns with ret 0
REFLEX_PORT_FN(0x00790cb0, Fn_00790cb0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    fpush(m, reflex_f80::from_f32(rd32(m, eax)));
    x0 = movss_m(rd32(m, eax + 0x4u));
    x1 = movss_m(rd32(m, eax + 0x8u));
    wr32(m, ecx + 0x1a0u, fst32(m));
    fpop(m);
    wr32(m, ecx + 0x1a4u, x0.l(0));
    wr32(m, ecx + 0x1a8u, x1.l(0));
    LIFT_RET(0);
}

// 00790ce0 Fn_00790ce0
//   original FUN_00790ce0; register inputs: ecx esi xmm0; returns with ret 20
REFLEX_PORT_FN(0x00790ce0, Fn_00790ce0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    FCmp fC_790d2f{};
    FCmp fC_790e29{};
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x10u;
    wr32(m, esi + 0x40u, x0.l(0));
    x0 = movss_m(rd32(m, ebp + 0x8u));
    wr32(m, esi + 0x44u, x0.l(0));
    x0 = movss_m(rd32(m, ebp + 0xcu));
    wr32(m, esi + 0x48u, x0.l(0));
    x2 = movss_m(rd32(m, esi + 0x44u));
    x1 = x0;
    x0 = movss_m(rd32(m, esi + 0x40u));
    x3 = x2;
    wr32(m, esp + 0xcu, x0.l(0));
    x3 = mulss(x3, x2.l(0));
    x0 = mulss(x0, x0.l(0));
    x2 = x1;
    x0 = addss(x0, x3.l(0));
    x2 = mulss(x2, x1.l(0));
    x0 = addss(x0, x2.l(0));
    fC_790d2f = fcmp32(x0.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    wr32(m, esp + 0x8u, x0.l(0));
    if ((fc_b(fC_790d2f) || fc_z(fC_790d2f))) goto L_790d88;
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x8u)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x42c230u, 0x790d4bu);  // FUN_0042c230
    x1 = x0;
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x0 = divss(x0, x1.l(0));
    x1 = movss_m(rd32(m, esp + 0x10u));
    x1 = mulss(x1, x0.l(0));
    wr32(m, esi + 0x40u, x1.l(0));
    x1 = movss_m(rd32(m, esi + 0x44u));
    x1 = mulss(x1, x0.l(0));
    wr32(m, esi + 0x44u, x1.l(0));
    x1 = movss_m(rd32(m, esi + 0x48u));
    x1 = mulss(x1, x0.l(0));
    esp = esp + 0x4u;
    wr32(m, esi + 0x48u, x1.l(0));
L_790d88:
    x0 = movss_m(rd32(m, ebp + 0x10u));
    wr32(m, esi + 0x30u, x0.l(0));
    x0 = movss_m(rd32(m, ebp + 0x14u));
    wr32(m, esi + 0x34u, x0.l(0));
    x0 = movss_m(rd32(m, ebp + 0x18u));
    wr32(m, esi + 0x38u, x0.l(0));
    x5 = movss_m(rd32(m, esi + 0x48u));
    x7 = movss_m(rd32(m, esi + 0x44u));
    x4 = movss_m(rd32(m, esi + 0x34u));
    x2 = movss_m(rd32(m, esi + 0x40u));
    x6 = x0;
    x3 = x2;
    x1 = x5;
    x1 = mulss(x1, x4.l(0));
    x0 = x7;
    x0 = mulss(x0, x6.l(0));
    x1 = subss(x1, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x30u));
    x3 = mulss(x3, x6.l(0));
    x6 = x0;
    x0 = mulss(x0, x7.l(0));
    x2 = mulss(x2, x4.l(0));
    x0 = subss(x0, x2.l(0));
    wr32(m, esi + 0x28u, x0.l(0));
    x6 = mulss(x6, x5.l(0));
    x3 = subss(x3, x6.l(0));
    wr32(m, esi + 0x20u, x1.l(0));
    x1 = x0;
    wr32(m, esi + 0x24u, x3.l(0));
    x0 = movss_m(rd32(m, esi + 0x20u));
    x2 = x3;
    wr32(m, esp + 0x8u, x0.l(0));
    x3 = mulss(x3, x2.l(0));
    x0 = mulss(x0, x0.l(0));
    x2 = x1;
    x0 = addss(x0, x3.l(0));
    x2 = mulss(x2, x1.l(0));
    x0 = addss(x0, x2.l(0));
    fC_790e29 = fcmp32(x0.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    wr32(m, esp + 0xcu, x0.l(0));
    if ((fc_b(fC_790e29) || fc_z(fC_790e29))) goto L_790e7d;
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0xcu)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x42c230u, 0x790e45u);  // FUN_0042c230
    x1 = x0;
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x0 = divss(x0, x1.l(0));
    x1 = movss_m(rd32(m, esp + 0xcu));
    x1 = mulss(x1, x0.l(0));
    wr32(m, esi + 0x20u, x1.l(0));
    x1 = x0;
    x1 = mulss(x1, rd32(m, esi + 0x24u));
    wr32(m, esi + 0x24u, x1.l(0));
    x0 = mulss(x0, rd32(m, esi + 0x28u));
    esp = esp + 0x4u;
    wr32(m, esi + 0x28u, x0.l(0));
L_790e7d:
    x5 = movss_m(rd32(m, esi + 0x28u));
    x6 = movss_m(rd32(m, esi + 0x48u));
    x7 = movss_m(rd32(m, esi + 0x24u));
    x4 = movss_m(rd32(m, esi + 0x44u));
    x2 = movss_m(rd32(m, esi + 0x20u));
    x1 = x5;
    x0 = x7;
    x0 = mulss(x0, x6.l(0));
    x1 = mulss(x1, x4.l(0));
    x1 = subss(x1, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x40u));
    x3 = x2;
    x3 = mulss(x3, x6.l(0));
    x6 = x0;
    eax = 0u;
    x6 = mulss(x6, x5.l(0));
    x0 = mulss(x0, x7.l(0));
    x2 = mulss(x2, x4.l(0));
    wr32(m, esi + 0x30u, x1.l(0));
    x3 = subss(x3, x6.l(0));
    wr32(m, esi + 0x34u, x3.l(0));
    x0 = subss(x0, x2.l(0));
    wr32(m, esi + 0x38u, x0.l(0));
    wr32(m, esi + 0xd4u, eax);
    wr32(m, esi + 0xd0u, eax);
    wr32(m, esi + 0xdcu, eax);
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(20);
}

// 00790f40 Fn_00790f40
//   original FUN_00790f40; register inputs: ecx esi xmm0; returns with ret 20
REFLEX_PORT_FN(0x00790f40, Fn_00790f40, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    FCmp fC_790f9e{};
    FCmp fC_7910cb{};
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x10u;
    wr32(m, esi + 0x1d0u, x0.l(0));
    x0 = movss_m(rd32(m, ebp + 0x8u));
    wr32(m, esi + 0x1d4u, x0.l(0));
    x0 = movss_m(rd32(m, ebp + 0xcu));
    wr32(m, esi + 0x1d8u, x0.l(0));
    x2 = movss_m(rd32(m, esi + 0x1d4u));
    x1 = x0;
    x0 = movss_m(rd32(m, esi + 0x1d0u));
    x3 = x2;
    wr32(m, esp + 0xcu, x0.l(0));
    x3 = mulss(x3, x2.l(0));
    x0 = mulss(x0, x0.l(0));
    x2 = x1;
    x0 = addss(x0, x3.l(0));
    x2 = mulss(x2, x1.l(0));
    x0 = addss(x0, x2.l(0));
    fC_790f9e = fcmp32(x0.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    wr32(m, esp + 0x8u, x0.l(0));
    if ((fc_b(fC_790f9e) || fc_z(fC_790f9e))) goto L_791006;
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x8u)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x42c230u, 0x790fbau);  // FUN_0042c230
    x1 = x0;
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x0 = divss(x0, x1.l(0));
    x1 = movss_m(rd32(m, esp + 0x10u));
    x1 = mulss(x1, x0.l(0));
    wr32(m, esi + 0x1d0u, x1.l(0));
    x1 = movss_m(rd32(m, esi + 0x1d4u));
    x1 = mulss(x1, x0.l(0));
    wr32(m, esi + 0x1d4u, x1.l(0));
    x1 = movss_m(rd32(m, esi + 0x1d8u));
    x1 = mulss(x1, x0.l(0));
    esp = esp + 0x4u;
    wr32(m, esi + 0x1d8u, x1.l(0));
L_791006:
    x0 = movss_m(rd32(m, ebp + 0x10u));
    wr32(m, esi + 0x1c0u, x0.l(0));
    x0 = movss_m(rd32(m, ebp + 0x14u));
    wr32(m, esi + 0x1c4u, x0.l(0));
    x0 = movss_m(rd32(m, ebp + 0x18u));
    wr32(m, esi + 0x1c8u, x0.l(0));
    x5 = movss_m(rd32(m, esi + 0x1d8u));
    x7 = movss_m(rd32(m, esi + 0x1d4u));
    x4 = movss_m(rd32(m, esi + 0x1c4u));
    x2 = movss_m(rd32(m, esi + 0x1d0u));
    x6 = x0;
    x3 = x2;
    x1 = x5;
    x1 = mulss(x1, x4.l(0));
    x0 = x7;
    x0 = mulss(x0, x6.l(0));
    x1 = subss(x1, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x1c0u));
    x3 = mulss(x3, x6.l(0));
    x6 = x0;
    x0 = mulss(x0, x7.l(0));
    x2 = mulss(x2, x4.l(0));
    x0 = subss(x0, x2.l(0));
    wr32(m, esi + 0x1b8u, x0.l(0));
    x6 = mulss(x6, x5.l(0));
    x3 = subss(x3, x6.l(0));
    wr32(m, esi + 0x1b0u, x1.l(0));
    x1 = x0;
    wr32(m, esi + 0x1b4u, x3.l(0));
    x0 = movss_m(rd32(m, esi + 0x1b0u));
    x2 = x3;
    wr32(m, esp + 0x8u, x0.l(0));
    x3 = mulss(x3, x2.l(0));
    x0 = mulss(x0, x0.l(0));
    x2 = x1;
    x0 = addss(x0, x3.l(0));
    x2 = mulss(x2, x1.l(0));
    x0 = addss(x0, x2.l(0));
    fC_7910cb = fcmp32(x0.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    wr32(m, esp + 0xcu, x0.l(0));
    if ((fc_b(fC_7910cb) || fc_z(fC_7910cb))) goto L_79112e;
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0xcu)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x42c230u, 0x7910e7u);  // FUN_0042c230
    x1 = x0;
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x0 = divss(x0, x1.l(0));
    x1 = movss_m(rd32(m, esp + 0xcu));
    x1 = mulss(x1, x0.l(0));
    wr32(m, esi + 0x1b0u, x1.l(0));
    x1 = x0;
    x1 = mulss(x1, rd32(m, esi + 0x1b4u));
    wr32(m, esi + 0x1b4u, x1.l(0));
    x0 = mulss(x0, rd32(m, esi + 0x1b8u));
    esp = esp + 0x4u;
    wr32(m, esi + 0x1b8u, x0.l(0));
L_79112e:
    x5 = movss_m(rd32(m, esi + 0x1b8u));
    x6 = movss_m(rd32(m, esi + 0x1d8u));
    x7 = movss_m(rd32(m, esi + 0x1b4u));
    x4 = movss_m(rd32(m, esi + 0x1d4u));
    x2 = movss_m(rd32(m, esi + 0x1b0u));
    x1 = x5;
    x0 = x7;
    x0 = mulss(x0, x6.l(0));
    x1 = mulss(x1, x4.l(0));
    x1 = subss(x1, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x1d0u));
    x3 = x2;
    x3 = mulss(x3, x6.l(0));
    x6 = x0;
    x6 = mulss(x6, x5.l(0));
    x0 = mulss(x0, x7.l(0));
    x2 = mulss(x2, x4.l(0));
    x3 = subss(x3, x6.l(0));
    x0 = subss(x0, x2.l(0));
    wr32(m, esi + 0x1c0u, x1.l(0));
    wr32(m, esi + 0x1c4u, x3.l(0));
    wr32(m, esi + 0x1c8u, x0.l(0));
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(20);
}

// 007911b0 Fn_007911b0
//   original FUN_007911b0; register inputs: ecx eax; returns with ret 0
REFLEX_PORT_FN(0x007911b0, Fn_007911b0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x8u)));
    x0 = movss_m(rd32(m, ecx));
    esp = esp - 0x14u;
    wr32(m, esp + 0x10u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x4u)));
    wr32(m, esp + 0xcu, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax)));
    wr32(m, esp + 0x8u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x8u)));
    wr32(m, esp + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x4u)));
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x790f40u, 0x7911ddu);  // FUN_00790f40
    LIFT_RET(0);
}

// 007911e0 Fn_007911e0
//   original FUN_007911e0; register inputs: eax xmm0; returns with ret 0
REFLEX_PORT_FN(0x007911e0, Fn_007911e0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    ecx = 0u;
    wr32(m, eax + 0xd4u, ecx);
    wr32(m, eax + 0xdcu, ecx);
    wr32(m, eax + 0x50u, x0.l(0));
    wr32(m, eax + 0xd8u, ecx);
    LIFT_RET(0);
}

// 00792330 Fn_00792330
//   original FUN_00792330; register inputs: ecx eax; returns with ret 0
REFLEX_PORT_FN(0x00792330, Fn_00792330, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x10u)));
    wr32(m, eax, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x14u)));
    wr32(m, eax + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x18u)));
    wr32(m, eax + 0x8u, fst32(m));
    fpop(m);
    LIFT_RET(0);
}

// 00792350 Camera__vf0
//   original Camera::vf0; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x00792350, Camera__vf0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x1a0u)));
    eax = rd32(m, esp + 0x4u);
    wr32(m, eax, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x1a4u)));
    wr32(m, eax + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x1a8u)));
    wr32(m, eax + 0x8u, fst32(m));
    fpop(m);
    LIFT_RET(4);
}

// 00792380 Fn_00792380
//   original FUN_00792380; register inputs: ecx eax; returns with ret 0
REFLEX_PORT_FN(0x00792380, Fn_00792380, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x40u)));
    wr32(m, eax, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x44u)));
    wr32(m, eax + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x48u)));
    wr32(m, eax + 0x8u, fst32(m));
    fpop(m);
    LIFT_RET(0);
}

// 007923a0 Fn_007923a0
//   original FUN_007923a0; register inputs: ecx eax; returns with ret 0
REFLEX_PORT_FN(0x007923a0, Fn_007923a0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x30u)));
    wr32(m, eax, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x34u)));
    wr32(m, eax + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x38u)));
    wr32(m, eax + 0x8u, fst32(m));
    fpop(m);
    LIFT_RET(0);
}

// 00794d10 Fn_00794d10
//   original FUN_00794d10; register inputs: edi; returns with ret 0
REFLEX_PORT_FN(0x00794d10, Fn_00794d10, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_794d19 = 0;
    uint32_t fB_794d19 = 0;
    uint32_t fA_794d3c = 0;
    uint32_t fB_794d3c = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x1cu;
    fA_794d19 = rd32(m, edi + 0x80u);
    fB_794d19 = 0x0u;
    esp -= 4; wr32(m, esp, esi);
    if (fA_794d19 == fB_794d19) goto L_794dd3;
    esi = rd32(m, edi);
    eax = rd32(m, esi);
    edx = rd32(m, eax);
    ecx = (esp + 0x10u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = esi;
    LIFT_CALL(edx, 0x794d36u);
    eax = rd32(m, edi + 0x80u);
    fA_794d3c = rd32(m, eax + 0x70u);
    fB_794d3c = 0x0u;
    x0 = movss_m(rd32(m, esi + 0x1d0u));
    x1 = movss_m(rd32(m, esi + 0x1d4u));
    x2 = movss_m(rd32(m, esi + 0x1d8u));
    x3 = movss_m(rd32(m, esi + 0x1c0u));
    x4 = movss_m(rd32(m, esi + 0x1c4u));
    x5 = movss_m(rd32(m, esi + 0x1c8u));
    if (fA_794d3c == fB_794d3c) goto L_794d84;
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x10u)));
    wr32(m, eax + 0x60u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x14u)));
    wr32(m, eax + 0x64u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x18u)));
    wr32(m, eax + 0x68u, fst32(m));
    fpop(m);
L_794d84:
    x6 = movss_m(rd32(m, esp + 0x10u));
    wr32(m, eax + 0x10u, x6.l(0));
    x6 = movss_m(rd32(m, esp + 0x14u));
    wr32(m, eax + 0x14u, x6.l(0));
    x6 = movss_m(rd32(m, esp + 0x18u));
    wr32(m, eax + 0x18u, x6.l(0));
    eax = rd32(m, edi + 0x80u);
    eax = eax + 0x20u;
    wr32(m, eax, x0.l(0));
    wr32(m, eax + 0x4u, x1.l(0));
    wr32(m, eax + 0x8u, x2.l(0));
    eax = rd32(m, edi + 0x80u);
    eax = eax + 0x30u;
    wr32(m, eax, x3.l(0));
    wr32(m, eax + 0x4u, x4.l(0));
    wr32(m, eax + 0x8u, x5.l(0));
L_794dd3:
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00794f70 rtti_shared_vf_00794f70
//   original rtti_shared_vf_00794f70; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00794f70, rtti_shared_vf_00794f70, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = 0x1u;
    LIFT_RET(0);
}

// 007ae4e0 Renderer_EnsureDeviceReady
//   original Renderer_EnsureDeviceReady; register inputs: none; returns with ret 4
REFLEX_PORT_FN(0x007ae4e0, Renderer_EnsureDeviceReady, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7ae502 = 0;
    uint32_t fB_7ae502 = 0;
    uint32_t fR_7ae510 = 0;
    uint32_t fR_7ae536 = 0;
    uint32_t fA_7ae58c = 0;
    uint32_t fB_7ae58c = 0;
    uint32_t fA_7ae5bf = 0;
    uint32_t fB_7ae5bf = 0;
    uint32_t fR_7ae5e7 = 0;
    uint32_t fA_7ae606 = 0;
    uint32_t fB_7ae606 = 0;
    uint32_t fR_7ae62e = 0;
    uint32_t fA_7ae63a = 0;
    uint32_t fB_7ae63a = 0;
    uint32_t fR_7ae65b = 0;
    uint32_t fR_7ae69f = 0;
    uint32_t fR_7ae6ad = 0;
    uint32_t fA_7ae716 = 0;
    uint32_t fB_7ae716 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    ecx = rd32(m, 0xa95960u);
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x10u);
    esp = esp - 0x2cu;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, ecx + 0x1ecu);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(edx, 0x7ae502u);
    fA_7ae502 = rd8(m, eax + 0x34u);
    fB_7ae502 = 0x0u;
    if (fA_7ae502 == fB_7ae502) goto L_7ae525;
    eax = rd32(m, esi);
    ecx = rd32(m, eax + 0xcu);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(ecx, 0x7ae510u);
    fR_7ae510 = (eax & eax);
    if (!(fR_7ae510 == 0)) goto L_7ae530;
    ecx = rd32(m, 0xa95960u);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x10u);
    LIFT_CALL(eax, 0x7ae521u);
    wr8(m, eax + 0x34u, 0x0u);
L_7ae525:
    eax = (eax & 0xffffff00u) | ((0x1u) & 0xffu);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_7ae530:
    eax = rd32(m, ebx + 0x7e4u);
    fR_7ae536 = ((eax & 0xffu) & 0x1u);
    if (!(fR_7ae536 == 0)) goto L_7ae59a;
    ecx = rd32(m, 0xa95844u);
    eax = (eax | 0x1u);
    edx = (esp + 0x10u);
    wr32(m, ebx + 0x7e4u, eax);
    edi = rd32(m, ecx + 0x10u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(rd32(m, 0x916098u) /* 8.295392636125409e-39 */, 0x7ae557u);
    eax = rd32(m, esp + 0x10u);
    ecx = rd32(m, esp + 0x14u);
    edx = rd32(m, 0xa95844u);
    wr32(m, edi + 0x78u, eax);
    wr32(m, edi + 0x7cu, ecx);
    eax = rd32(m, edx + 0x10u);
    ecx = rd32(m, eax + 0x8u);
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x20u);
    LIFT_CALL(edx, 0x7ae578u);
    eax = rd32(m, 0xa95844u);
    eax = rd32(m, eax + 0x10u);
    ecx = rd32(m, eax + 0x8u);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x28u);
    esp -= 4; wr32(m, esp, 0x1u);
    LIFT_CALL(eax, 0x7ae58cu);
    fA_7ae58c = rd32(m, 0xa95844u);
    fB_7ae58c = 0x0u;
    if (fA_7ae58c == fB_7ae58c) goto L_7ae59a;
    LIFT_CALL(0x84f230u, 0x7ae59au);  // FUN_0084f230
L_7ae59a:
    ecx = rd32(m, 0xa95844u);
    eax = rd32(m, ecx + 0x10u);
    ecx = rd32(m, eax + 0x8u);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x24u);
    LIFT_CALL(eax, 0x7ae5adu);
    edi = rd32(m, 0x916020u) /* 8.295000272555398e-39 */;
    esp -= 4; wr32(m, esp, 0x64u);
    LIFT_CALL(edi, 0x7ae5b7u);
    ecx = rd32(m, esi);
    edx = rd32(m, ecx + 0xcu);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(edx, 0x7ae5bfu);
    fA_7ae5bf = eax;
    fB_7ae5bf = 0x88760869u;
    if (fA_7ae5bf == fB_7ae5bf) goto L_7ae645;
    esp -= 4; wr32(m, esp, 0x1u);
    LIFT_CALL(edi, 0x7ae5cau);
    edi = rd32(m, 0x916488u) /* 8.28845901132393e-39 */;
    esp -= 4; wr32(m, esp, 0x1u);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0x0u);
    eax = (esp + 0x2cu);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0x34u, 0x0u);
    LIFT_CALL(edi, 0x7ae5e7u);
    fR_7ae5e7 = (eax & eax);
    if ((fR_7ae5e7 == 0)) goto L_7ae632;
    goto L_7ae5f0;
L_7ae5f0:
    ecx = (esp + 0x1cu);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(rd32(m, 0x916494u) /* 8.28852066845636e-39 */, 0x7ae5fbu);
    edx = (esp + 0x1cu);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(rd32(m, 0x916474u) /* 8.288338499655998e-39 */, 0x7ae606u);
    fA_7ae606 = rd32(m, esp + 0x20u);
    fB_7ae606 = 0x12u;
    if (fA_7ae606 != fB_7ae606) goto L_7ae61f;
    eax = rd32(m, 0xa95844u);
    eax = rd32(m, eax + 0x10u);
    ecx = rd32(m, eax + 0x8u);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x8u);
    LIFT_CALL(eax, 0x7ae61fu);
L_7ae61f:
    esp -= 4; wr32(m, esp, 0x1u);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0x0u);
    ecx = (esp + 0x2cu);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(edi, 0x7ae62eu);
    fR_7ae62e = (eax & eax);
    if (!(fR_7ae62e == 0)) goto L_7ae5f0;
L_7ae632:
    edx = rd32(m, esi);
    eax = rd32(m, edx + 0xcu);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(eax, 0x7ae63au);
    fA_7ae63a = eax;
    fB_7ae63a = 0x88760869u;
    if (fA_7ae63a != fB_7ae63a) goto L_7ae72f;
L_7ae645:
    ecx = rd32(m, 0xa95844u);
    eax = rd32(m, ecx + 0x10u);
    edx = rd32(m, eax + 0xdcu);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(rd32(m, 0x916034u) /* 8.295148810192616e-39 */, 0x7ae65bu);
    fR_7ae65b = (eax & eax);
    if (!(fR_7ae65b == 0)) goto L_7ae665;
    LIFT_CALL(rd32(m, 0x916078u) /* 8.295188046549617e-39 */, 0x7ae665u);
L_7ae665:
    eax = rd32(m, 0xa95844u);
    ecx = rd32(m, eax + 0x8u);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x4cu);
    LIFT_CALL(eax, 0x7ae674u);
    LIFT_CALL(0x7ac490u, 0x7ae679u);  // FUN_007ac490
    LIFT_CALL(0x7b0da0u, 0x7ae67eu);  // FUN_007b0da0
    esp -= 4; wr32(m, esp, 0x0u);
    LIFT_CALL(rd32(m, 0x916430u) /* 8.288837361909297e-39 */, 0x7ae686u);
    ecx = rd32(m, 0xa95960u);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x58u);
    edi = rd32(m, esi);
    LIFT_CALL(eax, 0x7ae695u);
    ecx = rd32(m, edi + 0x40u);
    eax = eax + 0x38u;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(ecx, 0x7ae69fu);
    fR_7ae69f = (eax & eax);
    if ((sgn(fR_7ae69f, 4) != false)) goto L_7ae72f;
    eax = rd32(m, ebx + 0x7e4u);
    fR_7ae6ad = ((eax & 0xffu) & 0x1u);
    if ((fR_7ae6ad == 0)) goto L_7ae72f;
    eax = (eax & 0xfffffffeu);
    wr32(m, ebx + 0x7e4u, eax);
    LIFT_CALL(0x7ae740u, 0x7ae6bfu);  // FUN_007ae740
    eax = rd32(m, ebx + 0x54u);
    edx = rd32(m, esi);
    ecx = rd32(m, edx + 0x9cu);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(ecx, 0x7ae6ceu);
    edx = rd32(m, 0xa95844u);
    ecx = rd32(m, edx + 0x8u);
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x50u);
    LIFT_CALL(edx, 0x7ae6deu);
    ecx = rd32(m, 0xa95960u);
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x10u);
    LIFT_CALL(edx, 0x7ae6ebu);
    wr8(m, eax + 0x34u, 0x0u);
    eax = rd32(m, 0xa95844u);
    eax = rd32(m, eax + 0x10u);
    ecx = rd32(m, eax + 0x8u);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x20u);
    LIFT_CALL(eax, 0x7ae701u);
    ecx = rd32(m, 0xa95844u);
    eax = rd32(m, ecx + 0x10u);
    ecx = rd32(m, eax + 0x8u);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x28u);
    esp -= 4; wr32(m, esp, 0x0u);
    LIFT_CALL(eax, 0x7ae716u);
    fA_7ae716 = rd32(m, 0xa95844u);
    fB_7ae716 = 0x0u;
    if (fA_7ae716 == fB_7ae716) goto L_7ae724;
    LIFT_CALL(0x84f230u, 0x7ae724u);  // FUN_0084f230
L_7ae724:
    eax = (eax & 0xffffff00u) | ((0x1u) & 0xffu);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_7ae72f:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    eax = (eax & 0xffffff00u) | ((0u) & 0xffu);
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 007e0190 Fn_007e0190
//   original FUN_007e0190; register inputs: eax; returns with ret 4
REFLEX_PORT_FN(0x007e0190, Fn_007e0190, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_7e01d3 = 0;
    uint32_t fR_7e01e3 = 0;
    uint32_t fA_7e01f0 = 0;
    uint32_t fB_7e01f0 = 0;
    uint32_t fA_7e01f8 = 0;
    uint32_t fB_7e01f8 = 0;
    uint32_t fA_7e01fc = 0;
    uint32_t fB_7e01fc = 0;
    uint32_t fR_7e0312 = 0;
    uint32_t fA_7e0365 = 0;
    uint32_t fB_7e0365 = 0;
    uint32_t fR_7e0372 = 0;
    uint32_t fA_7e0380 = 0;
    uint32_t fB_7e0380 = 0;
    uint32_t fA_7e0388 = 0;
    uint32_t fB_7e0388 = 0;
    uint32_t fA_7e03b9 = 0;
    uint32_t fB_7e03b9 = 0;
    uint32_t fA_7e0405 = 0;
    uint32_t fB_7e0405 = 0;
    uint32_t fR_7e0469 = 0;
    uint32_t fR_7e0476 = 0;
    uint32_t fR_7e04de = 0;
    uint32_t fR_7e0552 = 0;
    uint32_t fR_7e05dc = 0;
    uint32_t fR_7e0613 = 0;
    uint32_t fA_7e0620 = 0;
    uint32_t fB_7e0620 = 0;
    uint32_t fA_7e0628 = 0;
    uint32_t fB_7e0628 = 0;
    uint32_t fA_7e0644 = 0;
    uint32_t fB_7e0644 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x64u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, 0x91600cu) /* 8.29485173491818e-39 */;
    eax = (esi + 0x1114u);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0x20u, eax);
    LIFT_CALL(edi, 0x7e01b4u);
    eax = (esi + 0x112cu);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0x1cu, eax);
    LIFT_CALL(edi, 0x7e01c1u);
    eax = rd32(m, ebx);
    edx = rd32(m, eax + 0x64u);
    ecx = ebx;
    LIFT_CALL(edx, 0x7e01cau);
    edx = rd32(m, eax);
    ecx = eax;
    eax = rd32(m, edx + 0x4u);
    LIFT_CALL(eax, 0x7e01d3u);
    fR_7e01d3 = (eax & eax);
    if (!(fR_7e01d3 == 0)) goto L_7e0353;
    ecx = rd32(m, esi + 0x1108u);
    edi = 0u;
    fR_7e01e3 = (ecx & ecx);
    if ((false || (fR_7e01e3 == 0))) goto L_7e01fc;
    eax = rd32(m, esi + 0x10fcu);
L_7e01f0:
    fA_7e01f0 = rd32(m, eax);
    fB_7e01f0 = ebx;
    if (fA_7e01f0 == fB_7e01f0) goto L_7e01fc;
    edi = (edi + 1u);
    eax = eax + 0x8u;
    fA_7e01f8 = edi;
    fB_7e01f8 = ecx;
    if (fA_7e01f8 < fB_7e01f8) goto L_7e01f0;
L_7e01fc:
    fA_7e01fc = edi;
    fB_7e01fc = ecx;
    if (fA_7e01fc >= fB_7e01fc) goto L_7e060b;
    eax = (ecx - 0x1u);
    ecx = rd32(m, esi + 0x10fcu);
    edx = (ecx + eax * 8);
    wr32(m, esi + 0x1108u, eax);
    eax = (ecx + edi * 8);
    ecx = rd32(m, esi + 0x1100u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x7df2e0u, 0x7e0226u);  // FUN_007df2e0
    eax = rd32(m, esi + 0x10fcu);
    ecx = rd32(m, esi + 0x1108u);
    edx = rd32(m, eax + ecx * 8);
    wr32(m, eax + edi * 8, edx);
    ecx = rd32(m, eax + ecx * 8 + 0x4u);
    wr32(m, eax + edi * 8 + 0x4u, ecx);
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0x64u);
    ecx = ebx;
    LIFT_CALL(eax, 0x7e0249u);
    edx = rd32(m, eax);
    edx = rd32(m, edx + 0x18u);
    ecx = (esp + 0x14u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = (esp + 0x34u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = (esp + 0x28u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = eax;
    LIFT_CALL(edx, 0x7e0261u);
    eax = rd32(m, ebx);
    edx = rd32(m, eax + 0x38u);
    ecx = ebx;
    LIFT_CALL(edx, 0x7e026au);
    x0 = movss_m(rd32(m, eax + 0x10u));
    x3 = movss_m(rd32(m, esp + 0x24u));
    x1 = movss_m(rd32(m, eax + 0x20u));
    x4 = movss_m(rd32(m, esp + 0x28u));
    x5 = movss_m(rd32(m, esp + 0x20u));
    x2 = movss_m(rd32(m, eax + 0x24u));
    x1 = mulss(x1, x4.l(0));
    x0 = mulss(x0, x3.l(0));
    x0 = addss(x0, x1.l(0));
    x1 = movss_m(rd32(m, eax));
    x1 = mulss(x1, x5.l(0));
    x0 = addss(x0, x1.l(0));
    x1 = movss_m(rd32(m, eax + 0x14u));
    x0 = addss(x0, rd32(m, eax + 0x30u));
    x2 = mulss(x2, x4.l(0));
    x1 = mulss(x1, x3.l(0));
    x1 = addss(x1, x2.l(0));
    x2 = movss_m(rd32(m, eax + 0x4u));
    x2 = mulss(x2, x5.l(0));
    x1 = addss(x1, x2.l(0));
    x2 = movss_m(rd32(m, eax + 0x18u));
    x1 = addss(x1, rd32(m, eax + 0x34u));
    x2 = mulss(x2, x3.l(0));
    x3 = movss_m(rd32(m, eax + 0x28u));
    x3 = mulss(x3, x4.l(0));
    x2 = addss(x2, x3.l(0));
    x3 = movss_m(rd32(m, eax + 0x8u));
    x3 = mulss(x3, x5.l(0));
    x2 = addss(x2, x3.l(0));
    x2 = addss(x2, rd32(m, eax + 0x38u));
    wr32(m, esp + 0x20u, x0.l(0));
    wr32(m, esp + 0x24u, x1.l(0));
    wr32(m, esp + 0x28u, x2.l(0));
    eax = rd32(m, ebx);
    edx = rd32(m, eax + 0x5cu);
    ecx = ebx;
    LIFT_CALL(edx, 0x7e030eu);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x14u)));
    fR_7e0312 = (eax & eax);
    if ((fR_7e0312 == 0)) goto L_7e0339;
    edx = rd32(m, ebx);
    eax = (esp + 0x20u);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, edx + 0x5cu);
    esp -= 4; wr32(m, esp, ecx);
    ecx = ebx;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(eax, 0x7e0328u);
    edi = rd32(m, esi + 0x10f0u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x7da410u, 0x7e0334u);  // FUN_007da410
    goto L_7e060b;
L_7e0339:
    edi = rd32(m, esi + 0x10f0u);
    ecx = (esp + 0x20u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x7da350u, 0x7e034eu);  // FUN_007da350
    goto L_7e060b;
L_7e0353:
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0x64u);
    ecx = ebx;
    LIFT_CALL(eax, 0x7e035cu);
    edx = rd32(m, eax);
    ecx = eax;
    eax = rd32(m, edx + 0x4u);
    LIFT_CALL(eax, 0x7e0365u);
    fA_7e0365 = eax;
    fB_7e0365 = 0x3u;
    if (fA_7e0365 != fB_7e0365) goto L_7e03a7;
    edx = rd32(m, esi + 0x1018u);
    eax = 0u;
    fR_7e0372 = (edx & edx);
    if (((fR_7e0372 == 0) || sgn(fR_7e0372, 4) != false)) goto L_7e038c;
    ecx = rd32(m, esi + 0x1014u);
L_7e0380:
    fA_7e0380 = rd32(m, ecx);
    fB_7e0380 = ebx;
    if (fA_7e0380 == fB_7e0380) goto L_7e038f;
    eax = (eax + 1u);
    ecx = ecx + 0x4u;
    fA_7e0388 = eax;
    fB_7e0388 = edx;
    if (int32_t(sext(fA_7e0388, 4)) < int32_t(sext(fB_7e0388, 4))) goto L_7e0380;
L_7e038c:
    eax = (eax | 0xffffffffu);
L_7e038f:
    ecx = rd32(m, esi + 0x1014u);
    edx = rd32(m, ecx + edx * 4 - 0x4u);
    wr32(m, ecx + eax * 4, edx);
    wr32(m, esi + 0x1018u, (rd32(m, esi + 0x1018u) - 1u));
    goto L_7e060b;
L_7e03a7:
    eax = rd32(m, ebx);
    edx = rd32(m, eax + 0x64u);
    ecx = ebx;
    LIFT_CALL(edx, 0x7e03b0u);
    edx = rd32(m, eax);
    ecx = eax;
    eax = rd32(m, edx + 0x4u);
    LIFT_CALL(eax, 0x7e03b9u);
    fA_7e03b9 = eax;
    fB_7e03b9 = 0x2u;
    if (fA_7e03b9 != fB_7e03b9) goto L_7e03f3;
    edx = rd32(m, esi + 0x1000u);
    edi = rd32(m, esi + 0x1004u);
    ecx = (esi + 0x1000u);
    eax = (ebp + 0x8u);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0x18u, edx);
    LIFT_CALL(0x7f9db0u, 0x7e03ddu);  // FUN_007f9db0
    ecx = rd32(m, esp + 0x14u);
    edx = rd32(m, ecx + edi * 4 - 0x4u);
    wr32(m, ecx + eax * 4, edx);
    wr32(m, esi + 0x1004u, (rd32(m, esi + 0x1004u) - 1u));
    goto L_7e060b;
L_7e03f3:
    eax = rd32(m, ebx);
    edx = rd32(m, eax + 0x64u);
    ecx = ebx;
    LIFT_CALL(edx, 0x7e03fcu);
    edx = rd32(m, eax);
    ecx = eax;
    eax = rd32(m, edx + 0x4u);
    LIFT_CALL(eax, 0x7e0405u);
    fA_7e0405 = eax;
    fB_7e0405 = 0x1u;
    if (fA_7e0405 != fB_7e0405) goto L_7e060b;
    x0 = X{};
    wr32(m, esp + 0x44u, x0.l(0));
    x0 = movss_m(rd32(m, 0xa95d50u));
    wr32(m, esp + 0x30u, x0.l(0));
    x0 = movss_m(rd32(m, 0xa95d54u));
    wr32(m, esp + 0x34u, x0.l(0));
    x0 = movss_m(rd32(m, 0xa95d58u));
    ecx = (esp + 0x30u);
    wr32(m, esp + 0x38u, x0.l(0));
    x0 = movss_m(rd32(m, 0x977564u) /* 15000.0 */);
    edi = (esi + 0x10b4u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = edi;
    wr32(m, esp + 0x44u, ebx);
    wr32(m, esp + 0x4cu, x0.l(0));
    LIFT_CALL(0x7f9e40u, 0x7e0465u);  // FUN_007f9e40
    wr32(m, esp + 0x14u, eax);
    fR_7e0469 = (eax & eax);
    if ((sgn(fR_7e0469, 4) != false)) goto L_7e04d5;
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0x5cu);
    ecx = ebx;
    LIFT_CALL(eax, 0x7e0476u);
    fR_7e0476 = (eax & eax);
    if ((fR_7e0476 == 0)) goto L_7e04a6;
    ecx = rd32(m, esp + 0x14u);
    edx = rd32(m, edi);
    ecx = (ecx << 5);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + edx + 0x14u)));
    eax = (ecx + edx);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, ebx);
    edx = rd32(m, eax + 0x5cu);
    esp -= 4; wr32(m, esp, ecx);
    ecx = ebx;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(edx, 0x7e0498u);
    edi = rd32(m, esi + 0x10f0u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x7da290u, 0x7e04a4u);  // FUN_007da290
    goto L_7e04c6;
L_7e04a6:
    ecx = rd32(m, edi);
    eax = rd32(m, esp + 0x14u);
    edi = rd32(m, esi + 0x10f0u);
    eax = (eax << 5);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + ecx + 0x14u)));
    eax = eax + ecx;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x7da1d0u, 0x7e04c6u);  // FUN_007da1d0
L_7e04c6:
    ecx = rd32(m, esp + 0x14u);
    edx = (esi + 0x10b4u);
    LIFT_CALL(0x7f9ee0u, 0x7e04d5u);  // FUN_007f9ee0
L_7e04d5:
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0x5cu);
    ecx = ebx;
    LIFT_CALL(eax, 0x7e04deu);
    fR_7e04de = (eax & eax);
    if ((fR_7e04de == 0)) goto L_7e0581;
    edx = rd32(m, ebx);
    eax = rd32(m, edx + 0x5cu);
    ecx = ebx;
    LIFT_CALL(eax, 0x7e04efu);
    x0 = X{};
    wr32(m, esp + 0x44u, x0.l(0));
    x0 = movss_m(rd32(m, 0xa95d50u));
    wr32(m, esp + 0x30u, x0.l(0));
    x0 = movss_m(rd32(m, 0xa95d54u));
    wr32(m, esp + 0x34u, x0.l(0));
    x0 = movss_m(rd32(m, 0xa95d58u));
    ecx = (esp + 0x30u);
    wr32(m, esp + 0x38u, x0.l(0));
    x0 = movss_m(rd32(m, 0x977564u) /* 15000.0 */);
    wr32(m, esp + 0x60u, eax);
    edi = (esi + 0x10dcu);
    esp -= 4; wr32(m, esp, ecx);
    eax = edi;
    wr32(m, esp + 0x44u, 0x0u);
    wr32(m, esp + 0x4cu, x0.l(0));
    LIFT_CALL(0x7f9f80u, 0x7e054eu);  // FUN_007f9f80
    wr32(m, esp + 0x14u, eax);
    fR_7e0552 = (eax & eax);
    if ((sgn(fR_7e0552, 4) != false)) goto L_7e060b;
    edx = rd32(m, edi);
    eax = (eax << 6);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + edx + 0x14u)));
    eax = eax + edx;
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, ebx);
    edx = rd32(m, eax + 0x5cu);
    esp -= 4; wr32(m, esp, ecx);
    ecx = ebx;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(edx, 0x7e0573u);
    edi = rd32(m, esi + 0x10f0u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x7da290u, 0x7e057fu);  // FUN_007da290
    goto L_7e05fc;
L_7e0581:
    x0 = X{};
    wr32(m, esp + 0x44u, x0.l(0));
    x0 = movss_m(rd32(m, 0xa95d50u));
    wr32(m, esp + 0x30u, x0.l(0));
    x0 = movss_m(rd32(m, 0xa95d54u));
    wr32(m, esp + 0x34u, x0.l(0));
    x0 = movss_m(rd32(m, 0xa95d58u));
    eax = (esp + 0x30u);
    wr32(m, esp + 0x38u, x0.l(0));
    x0 = movss_m(rd32(m, 0x977564u) /* 15000.0 */);
    edi = (esi + 0x10dcu);
    esp -= 4; wr32(m, esp, eax);
    eax = edi;
    wr32(m, esp + 0x44u, ebx);
    wr32(m, esp + 0x4cu, x0.l(0));
    LIFT_CALL(0x7f9f80u, 0x7e05d8u);  // FUN_007f9f80
    wr32(m, esp + 0x14u, eax);
    fR_7e05dc = (eax & eax);
    if ((sgn(fR_7e05dc, 4) != false)) goto L_7e060b;
    ecx = rd32(m, edi);
    edi = rd32(m, esi + 0x10f0u);
    eax = (eax << 6);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + ecx + 0x14u)));
    eax = eax + ecx;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x7da1d0u, 0x7e05fcu);  // FUN_007da1d0
L_7e05fc:
    ecx = rd32(m, esp + 0x14u);
    edx = (esi + 0x10dcu);
    LIFT_CALL(0x7f9fd0u, 0x7e060bu);  // FUN_007f9fd0
L_7e060b:
    edx = rd32(m, esi + 0x1040u);
    eax = 0u;
    fR_7e0613 = (edx & edx);
    if (((fR_7e0613 == 0) || sgn(fR_7e0613, 4) != false)) goto L_7e062c;
    ecx = rd32(m, esi + 0x103cu);
L_7e0620:
    fA_7e0620 = rd32(m, ecx);
    fB_7e0620 = ebx;
    if (fA_7e0620 == fB_7e0620) goto L_7e062f;
    eax = (eax + 1u);
    ecx = ecx + 0x4u;
    fA_7e0628 = eax;
    fB_7e0628 = edx;
    if (int32_t(sext(fA_7e0628, 4)) < int32_t(sext(fB_7e0628, 4))) goto L_7e0620;
L_7e062c:
    eax = (eax | 0xffffffffu);
L_7e062f:
    ecx = rd32(m, esi + 0x103cu);
    edx = rd32(m, ecx + edx * 4 - 0x4u);
    wr32(m, ecx + eax * 4, edx);
    wr32(m, esi + 0x1040u, (rd32(m, esi + 0x1040u) - 1u));
    eax = 0u;
    fA_7e0644 = rd32(m, esi + 0x1040u);
    fB_7e0644 = eax;
    if (fA_7e0644 != fB_7e0644) goto L_7e0658;
    wr32(m, esi + 0x10b8u, eax);
    wr32(m, esi + 0x10e0u, eax);
L_7e0658:
    eax = rd32(m, esp + 0x18u);
    esi = rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */;
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(esi, 0x7e0665u);
    ecx = rd32(m, esp + 0x1cu);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(esi, 0x7e066cu);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 007e0fc0 Fn_007e0fc0: Render_SyncRenderData: main-thread render syncs, then the render-sync worker phase (runtime hook at 007e1082).
//   original FUN_007e0fc0; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x007e0fc0, Fn_007e0fc0, "engine_mt", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_7e1022 = 0;
    uint32_t fR_7e1053 = 0;
    uint32_t fA_7e107d = 0;
    uint32_t fB_7e107d = 0;
    uint32_t fA_7e1088 = 0;
    uint32_t fB_7e1088 = 0;
    uint32_t fR_7e10b0 = 0;
    uint32_t fA_7e10bd = 0;
    uint32_t fB_7e10bd = 0;
    uint32_t fA_7e10dd = 0;
    uint32_t fB_7e10dd = 0;
    esp = esp - 0x10cu;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp + 0x108u, eax);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, 0x91600cu) /* 8.29485173491818e-39 */;
    ebp = (esi + 0x1114u);
    esp -= 4; wr32(m, esp, ebp);
    wr32(m, esp + 0x18u, ebp);
    LIFT_CALL(edi, 0x7e0fedu);
    eax = (esi + 0x112cu);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0x14u, eax);
    LIFT_CALL(edi, 0x7e0ffau);
    ebx = rd32(m, esi + 0x10ccu);
    edi = rd32(m, esi + 0x10e0u);
    edi = edi + ebx;
    edi = edi + rd32(m, esi + 0x10b8u);
    eax = (esp + 0x18u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, 0x9720f8u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x916204u) /* 8.286141263663936e-39 */, 0x7e101fu);
    esp = esp + 0xcu;
    fR_7e1022 = (edi & edi);
    if (!(fR_7e1022 == 0)) goto L_7e1030;
    ecx = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, ecx);
    goto L_7e1106;
L_7e1030:
    edx = rd32(m, esi + 0x10b8u);
    eax = rd32(m, esp + 0x120u);
    wr32(m, esi + 0x11d0u, 0x0u);
    wr32(m, esi + 0x11d4u, edx);
    wr32(m, esi + 0x11e0u, eax);
    fR_7e1053 = (ebx & ebx);
    if (((fR_7e1053 == 0) || sgn(fR_7e1053, 4) != false)) goto L_7e1082;
    edi = 0u;
L_7e1060:
    ecx = rd32(m, esi + 0x10c8u);
    ecx = rd32(m, edi + ecx + 0x10u);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x64u);
    LIFT_CALL(eax, 0x7e1071u);
    edx = rd32(m, eax);
    ecx = eax;
    eax = rd32(m, edx + 0x38u);
    LIFT_CALL(eax, 0x7e107au);
    edi = edi + 0x20u;
    fA_7e107d = ebx;
    fB_7e107d = 0x1u;
    ebx = fA_7e107d - fB_7e107d;
    if (fA_7e107d != fB_7e107d) goto L_7e1060;
L_7e1082:
    { auto it_ = m.natives.find(0x7e1082u); if (it_ != m.natives.end()) { LIFT_SYNC_OUT(); uint32_t pc_ = it_->second(m);
        if (pc_ != reflex_vm::RUN_ORIGINAL) { LIFT_SYNC_IN(); if (pc_ == 0x7e1101u) goto L_7e1101; throw std::runtime_error("hook resume"); } } }
    ecx = rd32(m, esi + 0x11dcu);
    fA_7e1088 = ecx;
    fB_7e1088 = rd32(m, esi + 0x11d4u);
    ecx = fA_7e1088 + fB_7e1088;
    if ((((fA_7e1088 + fB_7e1088)) == 0)) goto L_7e10f6;
    ebp = rd32(m, 0x916090u) /* 8.295350597171479e-39 */;
    edi = (esi + 0x1180u);
    ebx = 0x4u;
L_7e10a1:
    edx = rd32(m, edi - 0x10u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(ebp, 0x7e10a7u);
    eax = rd32(m, edi);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x916034u) /* 8.295148810192616e-39 */, 0x7e10b0u);
    fR_7e10b0 = (eax & eax);
    if (!(fR_7e10b0 == 0)) goto L_7e10ba;
    LIFT_CALL(rd32(m, 0x916078u) /* 8.295188046549617e-39 */, 0x7e10bau);
L_7e10ba:
    edi = edi + 0x4u;
    fA_7e10bd = ebx;
    fB_7e10bd = 0x1u;
    ebx = fA_7e10bd - fB_7e10bd;
    if (fA_7e10bd != fB_7e10bd) goto L_7e10a1;
    ebp = rd32(m, 0x916008u) /* 8.295117981626401e-39 */;
    edi = (esi + 0x1170u);
    ebx = 0x4u;
L_7e10d3:
    ecx = rd32(m, edi);
    esp -= 4; wr32(m, esp, 0xffffffffu);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(ebp, 0x7e10dau);
    edi = edi + 0x4u;
    fA_7e10dd = ebx;
    fB_7e10dd = 0x1u;
    ebx = fA_7e10dd - fB_7e10dd;
    if (fA_7e10dd != fB_7e10dd) goto L_7e10d3;
    ebp = rd32(m, esp + 0x14u);
    wr32(m, esi + 0x11d0u, 0x7fffffffu);
    wr32(m, esi + 0x11d4u, ebx);
L_7e10f6:
    eax = rd32(m, esi + 0x10f0u);
    LIFT_CALL(0x7da080u, 0x7e1101u);  // FUN_007da080
L_7e1101:
    edx = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, edx);
L_7e1106:
    esi = rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */;
    LIFT_CALL(esi, 0x7e110eu);
    esp -= 4; wr32(m, esp, ebp);
    LIFT_CALL(esi, 0x7e1111u);
    ecx = rd32(m, esp + 0x118u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x7e1123u);  // __security_check_cookie
    esp = esp + 0x10cu;
    LIFT_RET(4);
}

// 007e4170 WaterEntity__vf19
//   original WaterEntity::vf19; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x007e4170, WaterEntity__vf19, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7e4173 = 0;
    uint32_t fB_7e4173 = 0;
    uint32_t fA_7e4191 = 0;
    uint32_t fB_7e4191 = 0;
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    fA_7e4173 = rd32(m, esi + 0x98u);
    fB_7e4173 = 0x0u;
    if (fA_7e4173 == fB_7e4173) goto L_7e4181;
    LIFT_CALL(0x7e4090u, 0x7e4181u);  // WaterEntity_Reload
L_7e4181:
    eax = rd32(m, esi + 0x90u);
    wr32(m, eax + 0x4cu, 0x1u);
    eax = rd32(m, eax + 0x44u);
    fA_7e4191 = rd32(m, eax + 0x4a8u);
    fB_7e4191 = 0x0u;
    if (fA_7e4191 == fB_7e4191) goto L_7e41c9;
    ecx = rd32(m, 0xa95844u);
    edx = rd32(m, ecx + 0x10u);
    ecx = rd32(m, edx + 0x58u);
    x0 = movss_m(rd32(m, esp + 0x8u));
    ecx = (ecx & 0x1u);
    wr32(m, eax + ecx * 4 + 0x490u, x0.l(0));
    edx = rd32(m, esi + 0x90u);
    wr32(m, edx + 0x4cu, 0x0u);
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_7e41c9:
    eax = rd32(m, esi + 0x90u);
    wr32(m, eax + 0x4cu, 0x0u);
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 007ea020 Fn_007ea020
//   original FUN_007ea020; register inputs: none; returns with ret 24
REFLEX_PORT_FN(0x007ea020, Fn_007ea020, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    FCmp fC_7ea03c{};
    FCmp fC_7ea05b{};
    FCmp fC_7ea065{};
    FCmp fC_7ea06a{};
    uint32_t fR_7ea0e3 = 0;
    uint32_t fR_7ea0ee = 0;
    uint32_t fR_7ea123 = 0;
    uint32_t fR_7ea181 = 0;
    uint32_t fR_7ea1c6 = 0;
    uint32_t fA_7ea1d4 = 0;
    uint32_t fB_7ea1d4 = 0;
    uint32_t fR_7ea1e0 = 0;
    uint32_t fA_7ea1e8 = 0;
    uint32_t fB_7ea1e8 = 0;
    uint32_t fA_7ea1ee = 0;
    uint32_t fB_7ea1ee = 0;
    uint32_t fA_7ea207 = 0;
    uint32_t fB_7ea207 = 0;
    uint32_t fR_7ea2a1 = 0;
    uint32_t fR_7ea2b5 = 0;
    uint32_t fA_7ea2e3 = 0;
    uint32_t fB_7ea2e3 = 0;
    uint32_t fR_7ea319 = 0;
    uint32_t fA_7ea348 = 0;
    uint32_t fB_7ea348 = 0;
    FCmp fC_7ea3a9{};
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    eax = rd32(m, ebp + 0xcu);
    x0 = movss_m(rd32(m, eax));
    esp = esp - 0x34u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0x8u);
    x1 = movss_m(rd32(m, ebx + 0xb0u));
    fC_7ea03c = fcmp32(x1.l(0), x0.l(0));
    x2 = movss_m(rd32(m, ebx + 0xb8u));
    x4 = movss_m(rd32(m, ebx + 0xbcu));
    x3 = movss_m(rd32(m, ebx + 0xb4u));
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    if (!(fc_b(fC_7ea03c) || fc_z(fC_7ea03c))) goto L_7ea06f;
    fC_7ea05b = fcmp32(x0.l(0), x2.l(0));
    if (!(fc_b(fC_7ea05b) || fc_z(fC_7ea05b))) goto L_7ea06f;
    x1 = movss_m(rd32(m, eax + 0x8u));
    fC_7ea065 = fcmp32(x3.l(0), x1.l(0));
    if (!(fc_b(fC_7ea065) || fc_z(fC_7ea065))) goto L_7ea06f;
    fC_7ea06a = fcmp32(x1.l(0), x4.l(0));
    if ((fc_b(fC_7ea06a) || fc_z(fC_7ea06a))) goto L_7ea07a;
L_7ea06f:
    eax = 0u;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(24);
L_7ea07a:
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0x80u)));
    esi = rd32(m, ebp + 0x10u);
    x2 = movss_m(rd32(m, 0x977658u) /* 1000000.0 */);
    wr32(m, esi, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0x84u)));
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esi + 0x4u, fst32(m));
    fpop(m);
    eax = (ebx + 0xa0u);
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0x88u)));
    wr32(m, esp + 0x34u, x0.l(0));
    wr32(m, esp + 0x24u, x0.l(0));
    wr32(m, esi + 0x8u, fst32(m));
    fpop(m);
    x0 = movss_m(rd32(m, 0x9777d8u) /* -1000000.0 */);
    esp -= 4; wr32(m, esp, eax);
    edx = (esp + 0x28u);
    edi = (esp + 0x38u);
    wr32(m, esp + 0x40u, x1.l(0));
    wr32(m, esp + 0x3cu, x2.l(0));
    wr32(m, esp + 0x30u, x1.l(0));
    wr32(m, esp + 0x2cu, x0.l(0));
    LIFT_CALL(0x8523b0u, 0x7ea0e0u);  // FUN_008523b0
    esp = esp + 0x8u;
    fR_7ea0e3 = (eax & eax);
    if ((fR_7ea0e3 == 0)) goto L_7ea48d;
    esi = rd32(m, ebp + 0x14u);
    fR_7ea0ee = (esi & esi);
    if ((fR_7ea0ee == 0)) goto L_7ea48d;
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0x18u)));
    edi = rd32(m, ebp + 0xcu);
    ecx = rd32(m, esi + 0x4u);
    wr32(m, esp + 0x14u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0x14u)));
    wr32(m, esp + 0x18u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi)));
    st(m, 0) = farith(m, 4, st(m, 0), reflex_f80::from_f32(rd32(m, ebx + 0xa0u)));
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x18u)));
    fpush(m, reflex_f80::from_f32(rd32(m, 0x9322ccu) /* 0.5 */));
    st(m, 0) = farith(m, 1, st(m, 0), st(m, 1));
    st(m, 2) = farith(m, 0, st(m, 2), st(m, 0));
    fpop(m);
    fpush(m, reflex_f80::from_int(int64_t(int32_t(sext(rd32(m, esi + 0x4u), 4)))));
    fR_7ea123 = (ecx & ecx);
    if ((sgn(fR_7ea123, 4) == false)) goto L_7ea12d;
    st(m, 0) = farith(m, 0, st(m, 0), reflex_f80::from_f32(rd32(m, 0x97731cu) /* 4294967296.0 */));
L_7ea12d:
    st(m, 2) = farith(m, 1, st(m, 2), st(m, 0));
    fpop(m);
    esp = esp - 0x10u;
    st(m, 1) = farith(m, 6, st(m, 1), st(m, 0));
    fpop(m);
    wr32(m, esp + 0x20u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x20u)));
    wr64(m, esp + 0x8u, fst64(m));
    fpop(m);
    fpush(m, reflex_f80::from_f64(rd64(m, 0x9774e8u) /* 0.5 */));
    wr64(m, esp, fst64(m));
    fpop(m);
    LIFT_CALL(rd32(m, 0x916230u) /* 8.28646636490766e-39 */, 0x7ea14fu);
    st(m, 0) = farith(m, 0, st(m, 0), reflex_f80::from_f32(rd32(m, esp + 0x20u)));
    LIFT_CALL(0x8fa0d0u, 0x7ea158u);  // FUN_008fa0d0
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x8u)));
    st(m, 0) = farith(m, 4, st(m, 0), reflex_f80::from_f32(rd32(m, ebx + 0xa8u)));
    x0 = cvtsi2ss(x0, eax);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x24u)));
    eax = rd32(m, esi + 0x8u);
    fpush(m, reflex_f80::from_f32(rd32(m, 0x9322ccu) /* 0.5 */));
    edx = cvttss2si(x0.l(0));
    st(m, 0) = farith(m, 1, st(m, 0), st(m, 1));
    wr32(m, esp + 0x20u, edx);
    st(m, 2) = farith(m, 0, st(m, 2), st(m, 0));
    fpop(m);
    fpush(m, reflex_f80::from_int(int64_t(int32_t(sext(rd32(m, esi + 0x8u), 4)))));
    fR_7ea181 = (eax & eax);
    if ((sgn(fR_7ea181, 4) == false)) goto L_7ea18b;
    st(m, 0) = farith(m, 0, st(m, 0), reflex_f80::from_f32(rd32(m, 0x97731cu) /* 4294967296.0 */));
L_7ea18b:
    st(m, 2) = farith(m, 1, st(m, 2), st(m, 0));
    fpop(m);
    st(m, 1) = farith(m, 6, st(m, 1), st(m, 0));
    fpop(m);
    wr32(m, esp + 0x24u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x24u)));
    wr64(m, esp + 0x8u, fst64(m));
    fpop(m);
    fpush(m, reflex_f80::from_f64(rd64(m, 0x9774e8u) /* 0.5 */));
    wr64(m, esp, fst64(m));
    fpop(m);
    LIFT_CALL(rd32(m, 0x916230u) /* 8.28646636490766e-39 */, 0x7ea1aau);
    st(m, 0) = farith(m, 0, st(m, 0), reflex_f80::from_f32(rd32(m, esp + 0x24u)));
    esp = esp + 0x10u;
    LIFT_CALL(0x8fa0d0u, 0x7ea1b6u);  // FUN_008fa0d0
    edx = rd32(m, esp + 0x10u);
    x0 = cvtsi2ss(x0, eax);
    eax = rd32(m, esi + 0x4u);
    eax = (eax - 1u);
    ecx = cvttss2si(x0.l(0));
    fR_7ea1c6 = (edx & edx);
    if ((sgn(fR_7ea1c6, 4) == false)) goto L_7ea1d4;
    wr32(m, esp + 0x10u, 0x0u);
    goto L_7ea1dc;
L_7ea1d4:
    fA_7ea1d4 = edx;
    fB_7ea1d4 = eax;
    if (int32_t(sext(fA_7ea1d4, 4)) <= int32_t(sext(fB_7ea1d4, 4))) goto L_7ea1dc;
    wr32(m, esp + 0x10u, eax);
L_7ea1dc:
    eax = rd32(m, esi + 0x8u);
    eax = (eax - 1u);
    fR_7ea1e0 = (ecx & ecx);
    if ((sgn(fR_7ea1e0, 4) == false)) goto L_7ea1e8;
    ecx = 0u;
    goto L_7ea1ee;
L_7ea1e8:
    fA_7ea1e8 = ecx;
    fB_7ea1e8 = eax;
    if (int32_t(sext(fA_7ea1e8, 4)) <= int32_t(sext(fB_7ea1e8, 4))) goto L_7ea1ee;
    ecx = eax;
L_7ea1ee:
    fA_7ea1ee = rd32(m, esi + 0x10u);
    fB_7ea1ee = 0x15u;
    if (fA_7ea1ee != fB_7ea1ee) goto L_7ea488;
    edx = rd32(m, esi + 0xcu);
    eax = rd32(m, esp + 0x10u);
    edx = uint32_t(int32_t(edx) * int32_t(ecx));
    eax = (edx + eax * 4);
    eax = eax + rd32(m, esi);
    fA_7ea207 = rd8(m, eax + 0x1u);
    fB_7ea207 = 0x0u;
    if (fA_7ea207 == fB_7ea207) goto L_7ea06f;
    edx = rd8(m, eax + 0x2u);
    x3 = movss_m(rd32(m, 0x97759cu) /* 65535.0 */);
    x2 = movss_m(rd32(m, ebx + 0x84u));
    x4 = movss_m(rd32(m, ebx + 0x88u));
    x5 = movss_m(rd32(m, ebp + 0x18u));
    x6 = movss_m(rd32(m, ebp + 0x1cu));
    edx = (edx << 8);
    x0 = cvtsi2ss(x0, edx);
    edx = rd8(m, eax + 0x3u);
    x1 = cvtsi2ss(x1, edx);
    x0 = addss(x0, x1.l(0));
    x1 = movss_m(rd32(m, ebx + 0x80u));
    x0 = divss(x0, x3.l(0));
    x0 = mulss(x0, x5.l(0));
    x0 = addss(x0, x6.l(0));
    x1 = mulss(x1, x0.l(0));
    x2 = mulss(x2, x0.l(0));
    x4 = mulss(x4, x0.l(0));
    x0 = movss_m(rd32(m, edi));
    x0 = addss(x0, x1.l(0));
    wr32(m, edi, x0.l(0));
    x0 = movss_m(rd32(m, edi + 0x4u));
    x0 = addss(x0, x2.l(0));
    wr32(m, edi + 0x4u, x0.l(0));
    x0 = movss_m(rd32(m, edi + 0x8u));
    x0 = addss(x0, x4.l(0));
    wr32(m, edi + 0x8u, x0.l(0));
    edx = rd32(m, esi + 0x4u);
    wr32(m, esp + 0x1cu, edx);
    fpush(m, reflex_f80::from_int(int64_t(int32_t(sext(rd32(m, esp + 0x1cu), 4)))));
    wr32(m, esp + 0x14u, edx);
    fR_7ea2a1 = (edx & edx);
    if ((sgn(fR_7ea2a1, 4) == false)) goto L_7ea2ab;
    st(m, 0) = farith(m, 0, st(m, 0), reflex_f80::from_f32(rd32(m, 0x97731cu) /* 4294967296.0 */));
L_7ea2ab:
    st(m, 0) = farith(m, 7, st(m, 0), reflex_f80::from_f32(rd32(m, esp + 0x18u)));
    edx = eax;
    wr32(m, esp + 0x18u, fst32(m));
    fpop(m);
    fR_7ea2b5 = (ecx & ecx);
    if ((fR_7ea2b5 == 0)) goto L_7ea2bc;
    edx = edx - rd32(m, esi + 0xcu);
L_7ea2bc:
    edi = rd8(m, edx + 0x2u);
    edx = rd8(m, edx + 0x3u);
    x0 = cvtsi2ss(x0, edx);
    edx = rd32(m, esi + 0x8u);
    edi = (edi << 8);
    x1 = cvtsi2ss(x1, edi);
    x1 = addss(x1, x0.l(0));
    x1 = divss(x1, x3.l(0));
    edx = (edx - 1u);
    x1 = mulss(x1, x5.l(0));
    x1 = addss(x1, x6.l(0));
    fA_7ea2e3 = ecx;
    fB_7ea2e3 = edx;
    if (fA_7ea2e3 != fB_7ea2e3) goto L_7ea2eb;
    esi = eax;
    goto L_7ea2f0;
L_7ea2eb:
    esi = rd32(m, esi + 0xcu);
    esi = esi + eax;
L_7ea2f0:
    ecx = rd8(m, esi + 0x2u);
    edx = rd8(m, esi + 0x3u);
    ecx = (ecx << 8);
    x2 = cvtsi2ss(x2, ecx);
    x0 = cvtsi2ss(x0, edx);
    edx = rd32(m, esp + 0x10u);
    x2 = addss(x2, x0.l(0));
    x2 = divss(x2, x3.l(0));
    x2 = mulss(x2, x5.l(0));
    x2 = addss(x2, x6.l(0));
    ecx = eax;
    fR_7ea319 = (edx & edx);
    if ((fR_7ea319 == 0)) goto L_7ea320;
    ecx = (eax - 0x4u);
L_7ea320:
    esi = rd8(m, ecx + 0x2u);
    ecx = rd8(m, ecx + 0x3u);
    x0 = cvtsi2ss(x0, ecx);
    ecx = rd32(m, esp + 0x14u);
    esi = (esi << 8);
    x4 = cvtsi2ss(x4, esi);
    x4 = addss(x4, x0.l(0));
    x4 = divss(x4, x3.l(0));
    ecx = (ecx - 1u);
    x4 = mulss(x4, x5.l(0));
    x4 = addss(x4, x6.l(0));
    fA_7ea348 = edx;
    fB_7ea348 = ecx;
    if (fA_7ea348 == fB_7ea348) goto L_7ea34f;
    eax = eax + 0x4u;
L_7ea34f:
    edx = rd8(m, eax + 0x2u);
    eax = rd8(m, eax + 0x3u);
    edx = (edx << 8);
    x2 = subss(x2, x1.l(0));
    x1 = movss_m(rd32(m, 0x9774a8u) /* -0.0 */);
    x0 = cvtsi2ss(x0, edx);
    x7 = cvtsi2ss(x7, eax);
    x0 = addss(x0, x7.l(0));
    x0 = divss(x0, x3.l(0));
    x3 = movss_m(rd32(m, esp + 0x18u));
    x1 = subss(x1, x2.l(0));
    x0 = mulss(x0, x5.l(0));
    x0 = addss(x0, x6.l(0));
    x0 = subss(x0, x4.l(0));
    x2 = x1;
    x2 = mulss(x2, x1.l(0));
    x4 = x3;
    x4 = mulss(x4, x3.l(0));
    x2 = addss(x2, x4.l(0));
    x4 = x0;
    x4 = mulss(x4, x0.l(0));
    x2 = addss(x2, x4.l(0));
    fC_7ea3a9 = fcmp32(x2.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    wr32(m, esp + 0x30u, x1.l(0));
    wr32(m, esp + 0x38u, x0.l(0));
    wr32(m, esp + 0x14u, x2.l(0));
    if ((fc_b(fC_7ea3a9) || fc_z(fC_7ea3a9))) goto L_7ea3fc;
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x14u)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x42c230u, 0x7ea3d1u);  // FUN_0042c230
    x1 = x0;
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x0 = divss(x0, x1.l(0));
    x1 = movss_m(rd32(m, esp + 0x34u));
    x3 = x0;
    x3 = mulss(x3, rd32(m, esp + 0x1cu));
    x1 = mulss(x1, x0.l(0));
    x0 = mulss(x0, rd32(m, esp + 0x3cu));
    esp = esp + 0x4u;
L_7ea3fc:
    x2 = movss_m(rd32(m, ebx + 0x70u));
    x4 = movss_m(rd32(m, ebx + 0x80u));
    x5 = movss_m(rd32(m, ebx + 0x84u));
    eax = rd32(m, ebp + 0x10u);
    x4 = mulss(x4, x3.l(0));
    x2 = mulss(x2, x1.l(0));
    x2 = addss(x2, x4.l(0));
    x4 = movss_m(rd32(m, ebx + 0x90u));
    x4 = mulss(x4, x0.l(0));
    x2 = addss(x2, x4.l(0));
    x4 = movss_m(rd32(m, ebx + 0x74u));
    x5 = mulss(x5, x3.l(0));
    x4 = mulss(x4, x1.l(0));
    x4 = addss(x4, x5.l(0));
    x5 = movss_m(rd32(m, ebx + 0x94u));
    x5 = mulss(x5, x0.l(0));
    x4 = addss(x4, x5.l(0));
    x5 = movss_m(rd32(m, ebx + 0x78u));
    x5 = mulss(x5, x1.l(0));
    x1 = movss_m(rd32(m, ebx + 0x88u));
    x1 = mulss(x1, x3.l(0));
    x5 = addss(x5, x1.l(0));
    x1 = movss_m(rd32(m, ebx + 0x98u));
    x1 = mulss(x1, x0.l(0));
    x5 = addss(x5, x1.l(0));
    wr32(m, eax, x2.l(0));
    wr32(m, eax + 0x4u, x4.l(0));
    wr32(m, eax + 0x8u, x5.l(0));
L_7ea488:
    eax = 0x1u;
L_7ea48d:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(24);
}

// 008c1270 Fn_008c1270
//   original FUN_008c1270; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x008c1270, Fn_008c1270, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_8c1282 = 0;
    uint32_t fB_8c1282 = 0;
    uint32_t fA_8c1290 = 0;
    uint32_t fB_8c1290 = 0;
    uint32_t fA_8c129e = 0;
    uint32_t fB_8c129e = 0;
    uint32_t fR_8c12b4 = 0;
    uint32_t fA_8c12b8 = 0;
    uint32_t fB_8c12b8 = 0;
    uint32_t fA_8c12c8 = 0;
    uint32_t fB_8c12c8 = 0;
    uint32_t fA_8c12d0 = 0;
    uint32_t fB_8c12d0 = 0;
    uint32_t fR_8c12fb = 0;
    uint32_t fA_8c1301 = 0;
    uint32_t fB_8c1301 = 0;
    edx = rd32(m, esp + 0x4u);
    esp = esp - 0x10u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, edi);
    edi = ecx;
    ecx = rd32(m, edi + 0x18u);
    eax = rd32(m, ecx + 0x4u);
    fA_8c1282 = rd8(m, eax + 0x15u);
    fB_8c1282 = 0x0u;
    ebx = ecx;
    if (fA_8c1282 != fB_8c1282) goto L_8c12a4;
    ecx = rd32(m, edx);
L_8c1290:
    fA_8c1290 = rd32(m, eax + 0xcu);
    fB_8c1290 = ecx;
    if (fA_8c1290 >= fB_8c1290) goto L_8c129a;
    eax = rd32(m, eax + 0x8u);
    goto L_8c129e;
L_8c129a:
    ebx = eax;
    eax = rd32(m, eax);
L_8c129e:
    fA_8c129e = rd8(m, eax + 0x15u);
    fB_8c129e = 0x0u;
    if (fA_8c129e == fB_8c129e) goto L_8c1290;
L_8c12a4:
    eax = rd32(m, edi + 0x18u);
    ebp = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, edi);
    wr32(m, esp + 0x14u, eax);
    fR_8c12b4 = (esi & esi);
    if ((fR_8c12b4 == 0)) goto L_8c12bc;
    fA_8c12b8 = esi;
    fB_8c12b8 = esi;
    if (fA_8c12b8 == fB_8c12b8) goto L_8c12c8;
L_8c12bc:
    LIFT_CALL(ebp, 0x8c12beu);
    edx = rd32(m, esp + 0x24u);
    ebp = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
L_8c12c8:
    fA_8c12c8 = ebx;
    fB_8c12c8 = rd32(m, esp + 0x14u);
    if (fA_8c12c8 == fB_8c12c8) goto L_8c12d5;
    ecx = rd32(m, edx);
    fA_8c12d0 = ecx;
    fB_8c12d0 = rd32(m, ebx + 0xcu);
    if (fA_8c12d0 >= fB_8c12d0) goto L_8c12fb;
L_8c12d5:
    edx = rd32(m, edx);
    eax = (esp + 0x10u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    ecx = (esp + 0x24u);
    esp -= 4; wr32(m, esp, ecx);
    ecx = edi;
    wr32(m, esp + 0x20u, edx);
    wr32(m, esp + 0x24u, 0x0u);
    LIFT_CALL(0x8a2c70u, 0x8c12f6u);  // FUN_008a2c70
    esi = rd32(m, eax);
    ebx = rd32(m, eax + 0x4u);
L_8c12fb:
    fR_8c12fb = (esi & esi);
    if (!(fR_8c12fb == 0)) goto L_8c1315;
    LIFT_CALL(ebp, 0x8c1301u);
L_8c1301:
    fA_8c1301 = ebx;
    fB_8c1301 = rd32(m, esi + 0x18u);
    esi = rd32(m, esp); esp += 4;
    if (fA_8c1301 != fB_8c1301) goto L_8c1309;
    LIFT_CALL(ebp, 0x8c1309u);
L_8c1309:
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = (ebx + 0x10u);
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x10u;
    LIFT_RET(4);
L_8c1315:
    esi = rd32(m, esi);
    goto L_8c1301;
}

// 008d3bf0 SoundEffectEntityAmbient__vf22
//   original SoundEffectEntityAmbient::vf22; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x008d3bf0, SoundEffectEntityAmbient__vf22, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = 0x1du;
    LIFT_RET(0);
}

// 008ea364 XINPUT1_3_DLL__Ordinal_4
//   original XINPUT1_3.DLL::Ordinal_4; register inputs: none; returns with ret 
REFLEX_PORT_FN(0x008ea364, XINPUT1_3_DLL__Ordinal_4, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    LIFT_TAILJMP(rd32(m, 0x91654cu) /* -5.605193857299268e-45 */);
}
