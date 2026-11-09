// Engine core services (0x0084xxxx, 0x0088xxxx): engine frame, database, job pool helpers.
// Group "engine". Transliterated instruction by instruction from MXReflex.exe by the bike lifter (engine copy, see
// native/game/notes/engine.md): the original's operations, order and memory traffic, stack frame included.
#include <stdexcept>
#include <utility>
#include "lift_rt.h"

// 008403b0 Fn_008403b0
//   original FUN_008403b0; register inputs: ecx eax; returns with ret 12
REFLEX_PORT_FN(0x008403b0, Fn_008403b0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_8403d3 = 0;
    uint32_t fB_8403d3 = 0;
    uint32_t fR_8403ed = 0;
    uint32_t fA_840409 = 0;
    uint32_t fB_840409 = 0;
    esp -= 4; wr32(m, esp, ecx);
    ecx = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = eax;
    eax = rd32(m, edi + 0xcu);
    wr32(m, ecx + 0xccu, eax);
    eax = rd32(m, esp + 0x14u);
    esi = rd32(m, eax + 0x44u);
    ebx = 0u;
    wr32(m, eax + 0x4cu, 0x1u);
    fA_8403d3 = rd32(m, esi + 0x10u);
    fB_8403d3 = ebx;
    if (int32_t(sext(fA_8403d3, 4)) <= int32_t(sext(fB_8403d3, 4))) goto L_84041b;
    esp -= 4; wr32(m, esp, ebp);
    ebp = 0u;
    ecx = ecx + 0x4u;
    wr32(m, esp + 0x10u, ecx);
L_8403e2:
    eax = rd32(m, esi + 0x14u);
    eax = eax + ebp;
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x7de9e0u, 0x8403edu);  // FUN_007de9e0
    fR_8403ed = (eax & eax);
    if ((fR_8403ed == 0)) goto L_8403f9;
    eax = eax - rd32(m, edi + 0x8u);
    eax = uint32_t(int32_t(eax) >> 4);
    goto L_8403fc;
L_8403f9:
    eax = (eax | 0xffffffffu);
L_8403fc:
    ecx = rd32(m, esp + 0x10u);
    wr32(m, ecx, eax);
    ebx = (ebx + 1u);
    ecx = ecx + 0x4u;
    ebp = ebp + 0x20u;
    fA_840409 = ebx;
    fB_840409 = rd32(m, esi + 0x10u);
    wr32(m, esp + 0x10u, ecx);
    if (int32_t(sext(fA_840409, 4)) < int32_t(sext(fB_840409, 4))) goto L_8403e2;
    eax = rd32(m, esp + 0x18u);
    ecx = rd32(m, esp + 0x20u);
    ebp = rd32(m, esp); esp += 4;
L_84041b:
    edx = rd32(m, esp + 0x18u);
    edi = rd32(m, esp); esp += 4;
    wr32(m, eax + 0x4cu, 0x0u);
    esi = rd32(m, esp); esp += 4;
    wr32(m, ecx, eax);
    wr32(m, ecx + 0xd0u, edx);
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(12);
}

// 008424e0 Fn_008424e0
//   original FUN_008424e0; register inputs: eax; returns with ret 8
REFLEX_PORT_FN(0x008424e0, Fn_008424e0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_8424fd = 0;
    uint32_t fB_8424fd = 0;
    uint32_t fR_842515 = 0;
    uint32_t fA_842539 = 0;
    uint32_t fB_842539 = 0;
    uint32_t fA_842546 = 0;
    uint32_t fB_842546 = 0;
    uint32_t fR_842550 = 0;
    uint32_t fA_84255c = 0;
    uint32_t fB_84255c = 0;
    uint32_t fA_84256e = 0;
    uint32_t fB_84256e = 0;
    uint32_t fR_842572 = 0;
    uint32_t fA_84257e = 0;
    uint32_t fB_84257e = 0;
    uint32_t fR_84259f = 0;
    uint32_t fA_8425ab = 0;
    uint32_t fB_8425ab = 0;
    uint32_t fA_8425b8 = 0;
    uint32_t fB_8425b8 = 0;
    esp = esp - 0x1cu;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    eax = rd32(m, esp + 0x30u);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x842630u, 0x8424f2u);  // FUN_00842630
    edi = eax;
    eax = rd32(m, esi + 0x34u);
    eax = eax - rd32(m, esi + 0x30u);
    eax = uint32_t(int32_t(eax) >> 3);
    fA_8424fd = edi;
    fB_8424fd = eax;
    if (fA_8424fd < fB_8424fd) goto L_842507;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x842507u);
L_842507:
    ecx = rd32(m, esi + 0x30u);
    ebx = rd32(m, ecx + edi * 8 + 0x4u);
    ebp = (esi + 0x8u);
    wr32(m, esp + 0x14u, ebp);
    fR_842515 = (ebp & ebp);
    if (!(fR_842515 == 0)) goto L_84251f;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x84251fu);
L_84251f:
    ebp = rd32(m, ebp);
    edi = (edi + 1u);
    wr32(m, esp + 0x20u, ebp);
    wr32(m, esp + 0x24u, ebx);
    wr32(m, esp + 0x18u, edi);
L_842530:
    edx = rd32(m, esi + 0x34u);
    edx = edx - rd32(m, esi + 0x30u);
    edx = uint32_t(int32_t(edx) >> 3);
    fA_842539 = edi;
    fB_842539 = edx;
    if (fA_842539 < fB_842539) goto L_842543;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x842543u);
L_842543:
    eax = rd32(m, esi + 0x30u);
    fA_842546 = rd32(m, eax + edi * 8 + 0x4u);
    fB_842546 = ebx;
    if (fA_842546 == fB_842546) goto L_84260f;
    fR_842550 = (ebp & ebp);
    if (!(fR_842550 == 0)) goto L_842591;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x84255au);
    eax = 0u;
L_84255c:
    fA_84255c = ebx;
    fB_84255c = rd32(m, eax + 0x14u);
    if (fA_84255c != fB_84255c) goto L_842567;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x842567u);
L_842567:
    ecx = rd32(m, ebx + 0x8u);
    edi = rd32(m, esp + 0x34u);
    fA_84256e = ecx;
    fB_84256e = rd32(m, edi);
    if (fA_84256e >= fB_84256e) goto L_84259b;
    fR_842572 = (ebp & ebp);
    if (!(fR_842572 == 0)) goto L_842596;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x84257cu);
    eax = 0u;
L_84257e:
    fA_84257e = ebx;
    fB_84257e = rd32(m, eax + 0x14u);
    if (fA_84257e != fB_84257e) goto L_842589;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x842589u);
L_842589:
    ebx = rd32(m, ebx);
    edi = rd32(m, esp + 0x18u);
    goto L_842530;
L_842591:
    eax = rd32(m, ebp);
    goto L_84255c;
L_842596:
    eax = rd32(m, ebp);
    goto L_84257e;
L_84259b:
    wr32(m, esp + 0x24u, ebx);
    fR_84259f = (ebp & ebp);
    if (!(fR_84259f == 0)) goto L_8425ec;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x8425a9u);
    eax = 0u;
L_8425ab:
    fA_8425ab = ebx;
    fB_8425ab = rd32(m, eax + 0x14u);
    if (fA_8425ab != fB_8425ab) goto L_8425b6;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x8425b6u);
L_8425b6:
    edx = rd32(m, edi);
    fA_8425b8 = edx;
    fB_8425b8 = rd32(m, ebx + 0x8u);
    if (fA_8425b8 >= fB_8425b8) goto L_8425f1;
    eax = rd32(m, esp + 0x14u);
    ecx = rd32(m, eax + 0x14u);
    edx = rd32(m, eax);
    wr32(m, esp + 0x1cu, ecx);
    ecx = rd32(m, esp + 0x30u);
    eax = (esp + 0x18u);
    wr32(m, esp + 0x18u, edx);
    edx = rd32(m, eax);
    eax = rd32(m, eax + 0x4u);
    wr32(m, ecx + 0x4u, eax);
    wr32(m, ecx, edx);
    eax = ecx;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x1cu;
    LIFT_RET(8);
L_8425ec:
    eax = rd32(m, ebp);
    goto L_8425ab;
L_8425f1:
    ecx = rd32(m, esp + 0x30u);
    eax = (esp + 0x20u);
    edx = rd32(m, eax);
    eax = rd32(m, eax + 0x4u);
    wr32(m, ecx + 0x4u, eax);
    wr32(m, ecx, edx);
    eax = ecx;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x1cu;
    LIFT_RET(8);
L_84260f:
    ecx = rd32(m, esp + 0x14u);
    edx = rd32(m, ecx + 0x14u);
    eax = rd32(m, esp + 0x30u);
    ecx = rd32(m, ecx);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    wr32(m, esp + 0x18u, ebx);
    wr32(m, eax + 0x4u, edx);
    wr32(m, eax, ecx);
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x1cu;
    LIFT_RET(8);
}

// 00842630 Fn_00842630
//   original FUN_00842630; register inputs: ecx eax esi; returns with ret 0
REFLEX_PORT_FN(0x00842630, Fn_00842630, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_84265a = 0;
    uint32_t fB_84265a = 0;
    uint32_t fA_84266b = 0;
    uint32_t fB_84266b = 0;
    esp -= 4; wr32(m, esp, ecx);
    ecx = rd32(m, eax);
    ecx = (ecx ^ 0xdeadbeefu);
    ecx = (ecx & 0x7fffffffu);
    esp -= 4; wr32(m, esp, 0x1f31du);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(rd32(m, 0x916380u) /* 8.287747151704053e-39 */, 0x84264bu);
    edx = uint32_t(int32_t(edx) * int32_t(0x41a7u));
    eax = uint32_t(int32_t(eax) * int32_t(0xb14u));
    esp = esp + 0x8u;
    fA_84265a = edx;
    fB_84265a = eax;
    edx = fA_84265a - fB_84265a;
    if (!sgn(((fA_84265a - fB_84265a)), 4)) goto L_842664;
    edx = edx + 0x7fffffffu;
L_842664:
    ecx = rd32(m, esi + 0x3cu);
    eax = ecx;
    eax = (eax & edx);
    fA_84266b = rd32(m, esi + 0x40u);
    fB_84266b = eax;
    if (fA_84266b > fB_84266b) goto L_842679;
    ecx = (ecx >> 1);
    edx = (edx | 0xffffffffu);
    edx = edx - ecx;
    eax = eax + edx;
L_842679:
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00842df0 Fn_00842df0
//   original FUN_00842df0; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00842df0, Fn_00842df0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_842e2f = 0;
    uint32_t fR_842e3f = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp -= 4; wr32(m, esp, 0xffffffffu);
    esp -= 4; wr32(m, esp, 0x902070u);
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
    esp -= 4; wr32(m, esp, 0x10u);
    LIFT_CALL(0x78fbe0u, 0x842e22u);  // ReflexAlignedAlloc
    esp = esp + 0x4u;
    wr32(m, ebp - 0x14u, eax);
    wr32(m, ebp - 0x4u, 0x0u);
    fR_842e2f = (eax & eax);
    if ((fR_842e2f == 0)) goto L_842e35;
    wr32(m, eax, eax);
L_842e35:
    wr32(m, ebp - 0x18u, 0x1u);
    ecx = (eax + 0x4u);
    fR_842e3f = (ecx & ecx);
    if ((fR_842e3f == 0)) goto L_842e45;
    wr32(m, ecx, eax);
L_842e45:
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

// 00842ff0 Fn_00842ff0
//   original FUN_00842ff0; register inputs: ecx; returns with ret 8
REFLEX_PORT_FN(0x00842ff0, Fn_00842ff0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_843031 = 0;
    uint32_t fR_843044 = 0;
    uint32_t fR_843057 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp -= 4; wr32(m, esp, 0xffffffffu);
    esp -= 4; wr32(m, esp, 0x902c20u);
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
    esi = ecx;
    esp -= 4; wr32(m, esp, 0x10u);
    LIFT_CALL(0x78fbe0u, 0x843024u);  // ReflexAlignedAlloc
    esp = esp + 0x4u;
    wr32(m, ebp - 0x18u, eax);
    wr32(m, ebp - 0x4u, 0x0u);
    fR_843031 = (eax & eax);
    if ((fR_843031 == 0)) goto L_84303a;
    ecx = rd32(m, ebp + 0x8u);
    wr32(m, eax, ecx);
L_84303a:
    wr32(m, ebp - 0x14u, 0x1u);
    ecx = (eax + 0x4u);
    fR_843044 = (ecx & ecx);
    if ((fR_843044 == 0)) goto L_84304d;
    edx = rd32(m, ebp + 0xcu);
    wr32(m, ecx, edx);
L_84304d:
    wr32(m, ebp - 0x14u, 0x2u);
    ecx = (eax + 0x8u);
    fR_843057 = (ecx & ecx);
    if ((fR_843057 == 0)) goto L_843065;
    edx = rd32(m, esi);
    wr32(m, ecx, edx);
    edx = rd32(m, esi + 0x4u);
    wr32(m, ecx + 0x4u, edx);
L_843065:
    wr32(m, ebp - 0x4u, 0xffffffffu);
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

// 00843350 Fn_00843350
//   original FUN_00843350; register inputs: eax edi; returns with ret 4
REFLEX_PORT_FN(0x00843350, Fn_00843350, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_843368 = 0;
    uint32_t fA_843372 = 0;
    uint32_t fB_843372 = 0;
    uint32_t fR_843382 = 0;
    uint32_t fR_843389 = 0;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    eax = rd32(m, edi);
    LIFT_CALL(0x5c3400u, 0x84335cu);  // FUN_005c3400
    edx = 0u;
    ebx = eax;
    { uint32_t d_ = rd32(m, esi); udiv32(eax, edx, d_); }
    eax = rd32(m, esi + 0xcu);
    esi = rd32(m, eax + edx * 4);
    fR_843368 = (esi & esi);
    if ((fR_843368 == 0)) goto L_84338d;
    ebp = rd32(m, 0x9162bcu) /* 8.287018476502604e-39 */;
L_843372:
    fA_843372 = rd32(m, esi + 0x8u);
    fB_843372 = ebx;
    if (fA_843372 != fB_843372) goto L_843386;
    eax = rd32(m, edi);
    ecx = rd32(m, esi);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(ebp, 0x84337fu);
    esp = esp + 0x8u;
    fR_843382 = (eax & eax);
    if ((fR_843382 == 0)) goto L_843395;
L_843386:
    esi = rd32(m, esi + 0xcu);
    fR_843389 = (esi & esi);
    if (!(fR_843389 == 0)) goto L_843372;
L_84338d:
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_843395:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, esi + 0x4u)) & 0xffu);
    edx = rd32(m, esp + 0x10u);
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    wr8(m, edx, (ecx & 0xffu));
    eax = 0x1u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 008435f0 Fn_008435f0
//   original FUN_008435f0; register inputs: ecx eax edx xmm0; returns with ret 0
REFLEX_PORT_FN(0x008435f0, Fn_008435f0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_8435fc = 0;
    FCmp fC_84360a{};
    FCmp fC_84361f{};
    FCmp fC_843635{};
    FCmp fC_843641{};
    FCmp fC_84364a{};
    uint32_t fR_843657 = 0;
    uint32_t fA_84365b = 0;
    uint32_t fB_84365b = 0;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, esi);
    fR_8435fc = (edx & edx);
    if ((fR_8435fc == 0)) goto L_843692;
    x1 = movss_m(rd32(m, eax + edx * 8 - 0x8u));
    fC_84360a = fcmp32(x0.l(0), x1.l(0));
    ecx = (eax + edx * 8 - 0x8u);
    wr32(m, esp + 0xcu, ecx);
    if (fc_b(fC_84360a)) goto L_84361b;
    ecx = edx;
    goto L_843657;
L_84361b:
    x2 = movss_m(rd32(m, eax));
    fC_84361f = fcmp32(x2.l(0), x0.l(0));
    if (!(fc_b(fC_84361f) || fc_z(fC_84361f))) goto L_843692;
    esp -= 4; wr32(m, esp, edi);
    edi = 0u;
    esi = edx;
L_843630:
    ecx = (esi + edi);
    ecx = uint32_t(int32_t(ecx) >> 1);
    fC_843635 = fcmp32(x0.l(0), rd32(m, eax + ecx * 8));
    if (fc_b(fC_843635)) goto L_84364a;
    x2 = movss_m(rd32(m, eax + ecx * 8 + 0x8u));
    fC_843641 = fcmp32(x2.l(0), x0.l(0));
    if (!(fc_b(fC_843641) || fc_z(fC_843641))) goto L_843655;
    edi = ecx;
    goto L_843630;
L_84364a:
    fC_84364a = fcmp32(x0.l(0), rd32(m, eax + ecx * 8 - 0x8u));
    if (!fc_b(fC_84364a)) goto L_843656;
    esi = ecx;
    goto L_843630;
L_843655:
    ecx = (ecx + 1u);
L_843656:
    edi = rd32(m, esp); esp += 4;
L_843657:
    fR_843657 = (ecx & ecx);
    if ((fR_843657 == 0)) goto L_843692;
    fA_84365b = ecx;
    fB_84365b = edx;
    if (fA_84365b == fB_84365b) goto L_8436bc;
    x2 = movss_m(rd32(m, eax + ecx * 8));
    edx = rd32(m, esp + 0x14u);
    eax = (eax + ecx * 8);
    x1 = movss_m(rd32(m, eax - 0x8u));
    ecx = (eax - 0x8u);
    esi = rd32(m, esp); esp += 4;
    wr32(m, ebp, eax);
    x0 = subss(x0, x1.l(0));
    x2 = subss(x2, x1.l(0));
    x0 = divss(x0, x2.l(0));
    ebp = rd32(m, esp); esp += 4;
    wr32(m, ebx, x0.l(0));
    wr32(m, edx, ecx);
    eax = 0x1u;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_843692:
    esi = rd32(m, esp + 0x14u);
    x1 = movss_m(rd32(m, eax));
    x0 = subss(x0, rd32(m, eax + edx * 8 - 0x8u));
    x1 = subss(x1, rd32(m, eax + 0x8u));
    ecx = (eax + 0x8u);
    wr32(m, esi, ecx);
    esi = rd32(m, esp); esp += 4;
    wr32(m, ebp, eax);
    x0 = divss(x0, x1.l(0));
    ebp = rd32(m, esp); esp += 4;
    wr32(m, ebx, x0.l(0));
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_8436bc:
    esi = rd32(m, esp + 0x14u);
    edx = (eax + edx * 8 - 0x10u);
    wr32(m, esi, edx);
    edx = rd32(m, esp + 0xcu);
    x0 = subss(x0, x1.l(0));
    x1 = movss_m(rd32(m, eax + ecx * 8 - 0x8u));
    x1 = subss(x1, rd32(m, eax + ecx * 8 - 0x10u));
    esi = rd32(m, esp); esp += 4;
    wr32(m, ebp, edx);
    x0 = divss(x0, x1.l(0));
    ebp = rd32(m, esp); esp += 4;
    wr32(m, ebx, x0.l(0));
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00843a00 XenonCore__vf4
//   original XenonCore::vf4; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x00843a00, XenonCore__vf4, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = (ecx + 0x3cu);
    LIFT_RET(0);
}

// 00844170 Fn_00844170
//   original FUN_00844170; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x00844170, Fn_00844170, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_8441a7 = 0;
    uint32_t fA_8441b1 = 0;
    uint32_t fB_8441b1 = 0;
    uint32_t fA_8441c3 = 0;
    uint32_t fB_8441c3 = 0;
    uint32_t fA_8441d4 = 0;
    uint32_t fB_8441d4 = 0;
    uint32_t fA_8441ee = 0;
    uint32_t fB_8441ee = 0;
    uint32_t fA_844223 = 0;
    uint32_t fB_844223 = 0;
    esp = esp - 0xcu;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = ecx;
    esp -= 4; wr32(m, esp, 0xd6d8f8u);
    wr32(m, esp + 0x1cu, edi);
    wr32(m, edi, 0x976584u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x84418eu);
    edx = rd32(m, 0xd6d8f0u);
    ecx = rd32(m, 0xd6d8ecu);
    eax = edx;
    eax = eax - ecx;
    eax = uint32_t(int32_t(eax) >> 2);
    esi = 0u;
    wr32(m, esp + 0x14u, eax);
    fR_8441a7 = (eax & eax);
    if ((false || (fR_8441a7 == 0))) goto L_844227;
    ebp = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    fA_8441b1 = esi;
    fB_8441b1 = eax;
    if (fA_8441b1 < fB_8441b1) goto L_8441c3;
    LIFT_CALL(ebp, 0x8441b7u);
    edx = rd32(m, 0xd6d8f0u);
    ecx = rd32(m, 0xd6d8ecu);
L_8441c3:
    fA_8441c3 = rd32(m, ecx + esi * 4);
    fB_8441c3 = edi;
    if (fA_8441c3 != fB_8441c3) goto L_84421b;
    edi = rd32(m, esp + 0x14u);
    eax = edx;
    eax = eax - ecx;
    edi = (edi - 1u);
    eax = uint32_t(int32_t(eax) >> 2);
    fA_8441d4 = edi;
    fB_8441d4 = eax;
    if (fA_8441d4 < fB_8441d4) goto L_8441e6;
    LIFT_CALL(ebp, 0x8441dau);
    edx = rd32(m, 0xd6d8f0u);
    ecx = rd32(m, 0xd6d8ecu);
L_8441e6:
    edx = edx - ecx;
    edx = uint32_t(int32_t(edx) >> 2);
    ebx = (ecx + edi * 4);
    fA_8441ee = esi;
    fB_8441ee = edx;
    if (fA_8441ee < fB_8441ee) goto L_8441fa;
    LIFT_CALL(ebp, 0x8441f4u);
    ecx = rd32(m, 0xd6d8ecu);
L_8441fa:
    edx = rd32(m, ebx);
    eax = edi;
    edi = 0xd6d8e0u;
    wr32(m, ecx + esi * 4, edx);
    LIFT_CALL(0x8499d0u, 0x84420bu);  // FUN_008499d0
    edx = rd32(m, 0xd6d8f0u);
    ecx = rd32(m, 0xd6d8ecu);
    edi = rd32(m, esp + 0x18u);
L_84421b:
    eax = edx;
    eax = eax - ecx;
    esi = (esi + 1u);
    eax = uint32_t(int32_t(eax) >> 2);
    fA_844223 = esi;
    fB_844223 = eax;
    if (fA_844223 < fB_844223) goto L_8441c3;
L_844227:
    esp -= 4; wr32(m, esp, 0xd6d8f8u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x844232u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0xcu;
    LIFT_RET(0);
}

// 00845050 RavenPhysics__CollisionShapeDynamicRays__vf1
//   original RavenPhysics::CollisionShapeDynamicRays::vf1; register inputs: ecx; returns with ret 8
REFLEX_PORT_FN(0x00845050, RavenPhysics__CollisionShapeDynamicRays__vf1, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_845063 = 0;
    uint32_t fA_845084 = 0;
    uint32_t fB_845084 = 0;
    uint32_t fA_8450e9 = 0;
    uint32_t fB_8450e9 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp -= 4; wr32(m, esp, ecx);
    edx = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, ebp + 0xcu);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, ecx + 0x20u);
    fR_845063 = (edi & edi);
    if (!(fR_845063 == 0)) goto L_845084;
    x0 = ldx(m, 0xd61670u);
    stx(m, edx, x0);
    x0 = ldx(m, 0xd61670u);
    stx(m, esi, x0);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(8);
L_845084:
    fA_845084 = edi;
    fB_845084 = 0x1u;
    eax = rd32(m, ecx + 0x24u);
    x0 = ldx(m, eax + 0x10u);
    x1 = ldx(m, eax);
    x1 = minps(x1, x0);
    stx(m, edx, x1);
    eax = rd32(m, ecx + 0x24u);
    x0 = ldx(m, eax + 0x10u);
    x1 = ldx(m, eax);
    x1 = maxps(x1, x0);
    stx(m, esi, x1);
    if (int32_t(sext(fA_845084, 4)) <= int32_t(sext(fB_845084, 4))) goto L_8450f4;
    eax = 0x20u;
    edi = (edi - 1u);
L_8450b0:
    ebx = rd32(m, ecx + 0x24u);
    x1 = ldx(m, ebx + eax);
    x0 = ldx(m, edx);
    x0 = minps(x0, x1);
    stx(m, edx, x0);
    ebx = rd32(m, ecx + 0x24u);
    x1 = ldx(m, ebx + eax + 0x10u);
    x0 = minps(x0, x1);
    stx(m, edx, x0);
    ebx = rd32(m, ecx + 0x24u);
    x1 = ldx(m, ebx + eax);
    x0 = ldx(m, esi);
    x0 = maxps(x0, x1);
    stx(m, esi, x0);
    ebx = rd32(m, ecx + 0x24u);
    x1 = ldx(m, ebx + eax + 0x10u);
    eax = eax + 0x20u;
    fA_8450e9 = edi;
    fB_8450e9 = 0x1u;
    edi = fA_8450e9 - fB_8450e9;
    x0 = maxps(x0, x1);
    stx(m, esi, x0);
    if (fA_8450e9 != fB_8450e9) goto L_8450b0;
L_8450f4:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 00849660 Fn_00849660
//   original FUN_00849660; register inputs: edi; returns with ret 4
REFLEX_PORT_FN(0x00849660, Fn_00849660, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_849671 = 0;
    uint32_t fA_84968b = 0;
    uint32_t fB_84968b = 0;
    uint32_t fA_8496a1 = 0;
    uint32_t fB_8496a1 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    ecx = rd32(m, edi + 0xcu);
    esp = esp - 0x8u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    fR_849671 = (ecx & ecx);
    if (!(fR_849671 == 0)) goto L_849679;
    eax = 0u;
    goto L_849681;
L_849679:
    eax = rd32(m, edi + 0x14u);
    eax = eax - ecx;
    eax = uint32_t(int32_t(eax) >> 2);
L_849681:
    esi = rd32(m, edi + 0x10u);
    edx = esi;
    edx = edx - ecx;
    edx = uint32_t(int32_t(edx) >> 2);
    fA_84968b = edx;
    fB_84968b = eax;
    if (fA_84968b >= fB_84968b) goto L_8496a1;
    eax = rd32(m, ebx);
    wr32(m, esi, eax);
    esi = esi + 0x4u;
    wr32(m, edi + 0x10u, esi);
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_8496a1:
    fA_8496a1 = ecx;
    fB_8496a1 = esi;
    if (fA_8496a1 <= fB_8496a1) goto L_8496ab;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x8496abu);
L_8496ab:
    eax = rd32(m, edi);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ebx);
    ecx = (esp + 0x14u);
    esp -= 4; wr32(m, esp, ecx);
    eax = edi;
    LIFT_CALL(0x6c8120u, 0x8496bcu);  // FUN_006c8120
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 008499d0 Fn_008499d0
//   original FUN_008499d0; register inputs: eax edi; returns with ret 0
REFLEX_PORT_FN(0x008499d0, Fn_008499d0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_8499f2 = 0;
    uint32_t fB_8499f2 = 0;
    uint32_t fA_8499f6 = 0;
    uint32_t fB_8499f6 = 0;
    uint32_t fA_849a23 = 0;
    uint32_t fB_849a23 = 0;
    uint32_t fA_849a3a = 0;
    uint32_t fB_849a3a = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0x10u;
    ecx = rd32(m, edi + 0xcu);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, edi + 0x10u);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    eax = ebx;
    eax = eax - ecx;
    eax = uint32_t(int32_t(eax) >> 2);
    wr32(m, esp + 0x8u, 0x0u);
    fA_8499f2 = esi;
    fB_8499f2 = eax;
    if (fA_8499f2 <= fB_8499f2) goto L_849a21;
    fA_8499f6 = ecx;
    fB_8499f6 = ebx;
    if (fA_8499f6 <= fB_8499f6) goto L_849a00;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x849a00u);
L_849a00:
    ecx = rd32(m, edi + 0x10u);
    eax = rd32(m, edi);
    ecx = ecx - rd32(m, edi + 0xcu);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, eax);
    eax = (esp + 0x10u);
    ecx = uint32_t(int32_t(ecx) >> 2);
    esp -= 4; wr32(m, esp, eax);
    esi = esi - ecx;
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x6c8260u, 0x849a1bu);  // FUN_006c8260
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_849a21:
    if (fA_8499f2 >= fB_8499f2) goto L_849a79;
    fA_849a23 = ecx;
    fB_849a23 = ebx;
    if (fA_849a23 <= fB_849a23) goto L_849a2d;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x849a2du);
L_849a2d:
    ecx = rd32(m, edi + 0xcu);
    edx = rd32(m, edi);
    wr32(m, esp + 0x10u, edx);
    wr32(m, esp + 0x8u, ecx);
    fA_849a3a = ecx;
    fB_849a3a = rd32(m, edi + 0x10u);
    if (fA_849a3a <= fB_849a3a) goto L_849a49;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x849a45u);
    ecx = rd32(m, esp + 0x8u);
L_849a49:
    eax = rd32(m, edi);
    esp -= 4; wr32(m, esp, esi);
    esi = (esp + 0xcu);
    wr32(m, esp + 0xcu, eax);
    wr32(m, esp + 0x10u, ecx);
    LIFT_CALL(0x842fa0u, 0x849a5du);  // FUN_00842fa0
    eax = rd32(m, esp + 0x10u);
    ecx = rd32(m, esp + 0xcu);
    edx = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edx);
    eax = (esp + 0x20u);
    esp -= 4; wr32(m, esp, eax);
    esi = edi;
    LIFT_CALL(0x7cf450u, 0x849a79u);  // FUN_007cf450
L_849a79:
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0084a1f0 Fn_0084a1f0
//   original FUN_0084a1f0; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x0084a1f0, Fn_0084a1f0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    ecx = rd32(m, esp + 0x4u);
    eax = ecx;
    eax = (eax << 15);
    eax = ~eax;
    eax = eax + ecx;
    ecx = eax;
    ecx = uint32_t(int32_t(ecx) >> 10);
    ecx = (ecx ^ eax);
    ecx = (ecx + ecx * 8);
    eax = ecx;
    eax = uint32_t(int32_t(eax) >> 6);
    ecx = (ecx ^ eax);
    edx = ecx;
    edx = (edx << 11);
    edx = ~edx;
    ecx = ecx + edx;
    edx = rd32(m, esp + 0x8u);
    eax = edx;
    eax = uint32_t(int32_t(eax) >> 31);
    eax = edx;
    eax = (eax << 15);
    eax = ~eax;
    edx = edx + eax;
    eax = edx;
    eax = uint32_t(int32_t(eax) >> 10);
    eax = (eax ^ edx);
    edx = (eax + eax * 8);
    eax = edx;
    eax = uint32_t(int32_t(eax) >> 6);
    edx = (edx ^ eax);
    eax = edx;
    eax = (eax << 11);
    eax = ~eax;
    edx = edx + eax;
    eax = edx;
    eax = uint32_t(int32_t(eax) >> 16);
    eax = (eax ^ edx);
    edx = ecx;
    edx = uint32_t(int32_t(edx) >> 16);
    edx = (edx ^ ecx);
    eax = eax + edx;
    LIFT_RET(0);
}

// 0084a7c0 Database__vf16
//   original Database::vf16; register inputs: ecx; returns with ret 16
REFLEX_PORT_FN(0x0084a7c0, Database__vf16, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, 0x0u);
    edx = (esp + 0x18u);
    esp -= 4; wr32(m, esp, edx);
    edx = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, 0x1u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x84c190u, 0x84a7deu);  // FUN_0084c190
    LIFT_RET(16);
}

// 0084a860 Database__vf10
//   original Database::vf10; register inputs: ecx; returns with ret 8
REFLEX_PORT_FN(0x0084a860, Database__vf10, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_84a871 = 0;
    edx = rd32(m, esp + 0x8u);
    eax = rd32(m, ecx);
    eax = rd32(m, eax + 0x20u);
    esp -= 4; wr32(m, esp, edx);
    edx = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(eax, 0x84a871u);
    fR_84a871 = (eax & eax);
    if ((fR_84a871 == 0)) goto L_84a87e;
    eax = rd32(m, eax + 0x94u);
    LIFT_RET(8);
L_84a87e:
    eax = 0u;
    LIFT_RET(8);
}

// 0084a890 Database__vf8
//   original Database::vf8; register inputs: ecx; returns with ret 8
REFLEX_PORT_FN(0x0084a890, Database__vf8, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_84a8c8 = 0;
    uint32_t fA_84a8ce = 0;
    uint32_t fB_84a8ce = 0;
    uint32_t fR_84a8f3 = 0;
    uint32_t fA_84a8fe = 0;
    uint32_t fB_84a8fe = 0;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    esp -= 4; wr32(m, esp, edi);
    ebx = (esi + 0x62b54u);
    esp -= 4; wr32(m, esp, ebx);
    wr32(m, esp + 0x14u, 0x0u);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x84a8acu);
    eax = (esp + 0x10u);
    esp -= 4; wr32(m, esp, eax);
    eax = (esi + 0x24u);
    edi = (esp + 0x1cu);
    LIFT_CALL(0x7fa250u, 0x84a8bdu);  // FUN_007fa250
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x84a8c4u);
    esi = rd32(m, esp + 0x10u);
    fR_84a8c8 = (esi & esi);
    if ((fR_84a8c8 == 0)) goto L_84a906;
    edi = 0u;
    fA_84a8ce = rd32(m, esi + 0x14cu);
    fB_84a8ce = edi;
    if (fA_84a8ce <= fB_84a8ce) goto L_84a906;
    ebp = rd32(m, esp + 0x1cu);
    ebx = 0u;
L_84a8e0:
    ecx = rd32(m, esi + 0x148u);
    ecx = ecx + ebx;
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(rd32(m, 0x9162bcu) /* 8.287018476502604e-39 */, 0x84a8f0u);
    esp = esp + 0x8u;
    fR_84a8f3 = (eax & eax);
    if ((fR_84a8f3 == 0)) goto L_84a910;
    edi = (edi + 1u);
    ebx = ebx + 0xa0u;
    fA_84a8fe = edi;
    fB_84a8fe = rd32(m, esi + 0x14cu);
    if (fA_84a8fe < fB_84a8fe) goto L_84a8e0;
L_84a906:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(8);
L_84a910:
    eax = (edi + edi * 4);
    edi = rd32(m, esp); esp += 4;
    eax = (eax << 5);
    eax = eax + rd32(m, esi + 0x148u);
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 0084ac70 Database__vf27
//   original Database::vf27; register inputs: ecx; returns with ret 16
REFLEX_PORT_FN(0x0084ac70, Database__vf27, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_84ac98 = 0;
    uint32_t fR_84acba = 0;
    esp = esp - 0x1cu;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp + 0x18u, eax);
    eax = rd32(m, esp + 0x20u);
    esp -= 4; wr32(m, esp, esi);
    edx = (esp + 0x4u);
    wr32(m, esp + 0x8u, eax);
    esp -= 4; wr32(m, esp, edx);
    esi = (ecx + 0x44u);
    eax = (esp + 0x2cu);
    LIFT_CALL(0x84dea0u, 0x84ac98u);  // FUN_0084dea0
    fR_84ac98 = (eax & eax);
    if ((fR_84ac98 == 0)) goto L_84acdf;
    esi = rd32(m, esp + 0x4u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x84aca8u);
    eax = (esp + 0x8u);
    esp -= 4; wr32(m, esp, eax);
    eax = (esi + 0x18u);
    edi = (esp + 0x10u);
    LIFT_CALL(0x7fa250u, 0x84acb9u);  // FUN_007fa250
    edi = rd32(m, esp); esp += 4;
    fR_84acba = (eax & eax);
    if (!(fR_84acba == 0)) goto L_84acf3;
    esp -= 4; wr32(m, esp, 0x8u);
    ecx = (esp + 0x2cu);
    esp -= 4; wr32(m, esp, ecx);
    edx = (esp + 0x14u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(rd32(m, 0x916260u) /* 8.286626112932593e-39 */, 0x84acd0u);
    esp = esp + 0xcu;
    esp -= 4; wr32(m, esp, esi);
    wr8(m, esp + 0x18u, 0x0u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x84acdfu);
L_84acdf:
    eax = 0u;
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp + 0x18u);
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x84acedu);  // __security_check_cookie
    esp = esp + 0x1cu;
    LIFT_RET(16);
L_84acf3:
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x84acfau);
    ecx = rd32(m, esp + 0x1cu);
    eax = rd32(m, esp + 0x4u);
    esi = rd32(m, esp); esp += 4;
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x84ad0au);  // __security_check_cookie
    esp = esp + 0x1cu;
    LIFT_RET(16);
}

// 0084c190 Fn_0084c190
//   original FUN_0084c190; register inputs: none; returns with ret 28
REFLEX_PORT_FN(0x0084c190, Fn_0084c190, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_84c1d8 = 0;
    uint32_t fB_84c1d8 = 0;
    uint32_t fR_84c22a = 0;
    uint32_t fR_84c376 = 0;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x18u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esp + 0x14u);
    eax = (edi + 0x62b24u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x84c1a9u);
    edx = edi;
    LIFT_CALL(0x84c5b0u, 0x84c1b0u);  // FUN_0084c5b0
    wr32(m, esp + 0x20u, eax);
    eax = uint32_t(int32_t(eax) * int32_t(0x194u));
    ebx = (eax + edi);
    esp -= 4; wr32(m, esp, 0x194u);
    esi = (ebx + 0x104u);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x8fa06au, 0x84c1d0u);  // MSVCR90.DLL::memset
    eax = rd32(m, esp + 0x24u);
    eax = (eax - 1u);
    esp = esp + 0xcu;
    fA_84c1d8 = eax;
    fB_84c1d8 = 0x5u;
    if (fA_84c1d8 > fB_84c1d8) goto L_84c345;
    switch (eax) { case 0: goto L_84c1e8; case 1: goto L_84c250; case 2: goto L_84c299; case 3: goto L_84c2eb; case 4: goto L_84c31d; case 5: goto L_84c33f; default: throw std::runtime_error("jump table"); }
L_84c1e8:
    ecx = rd32(m, esp + 0x1cu);
    eax = rd32(m, esp + 0x2cu);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esi, 0x1u);
    esp -= 4; wr32(m, esp, 0x9419e4u);
    edi = (ebx + 0x210u);
    esi = 0x3fu;
    wr32(m, ebx + 0x108u, eax);
    LIFT_CALL(0x78fd60u, 0x84c212u);  // FUN_0078fd60
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, 0x9419e4u);
    edi = (ebx + 0x250u);
    LIFT_CALL(0x78fd60u, 0x84c223u);  // FUN_0078fd60
    eax = rd32(m, esp + 0x34u);
    esp = esp + 0x10u;
    fR_84c22a = (eax & eax);
    if ((fR_84c22a == 0)) goto L_84c23f;
    eax = rd32(m, eax);
    edi = rd32(m, esp + 0x14u);
    wr32(m, ebx + 0x10cu, eax);
    goto L_84c345;
L_84c23f:
    edi = rd32(m, esp + 0x14u);
    eax = 0u;
    wr32(m, ebx + 0x10cu, eax);
    goto L_84c345;
L_84c250:
    edx = rd32(m, esp + 0x1cu);
    esp -= 4; wr32(m, esp, edx);
    wr32(m, esi, 0x2u);
    esp -= 4; wr32(m, esp, 0x9419e4u);
    edi = (ebx + 0x210u);
    esi = 0x3fu;
    LIFT_CALL(0x78fd60u, 0x84c270u);  // FUN_0078fd60
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, 0x9419e4u);
    edi = (ebx + 0x250u);
    LIFT_CALL(0x78fd60u, 0x84c281u);  // FUN_0078fd60
    eax = rd32(m, esp + 0x34u);
    ecx = rd32(m, eax);
    edi = rd32(m, esp + 0x24u);
    esp = esp + 0x10u;
    wr32(m, ebx + 0x10cu, ecx);
    goto L_84c345;
L_84c299:
    edx = rd32(m, esp + 0x1cu);
    esp -= 4; wr32(m, esp, edx);
    wr32(m, esi, 0x3u);
    esp -= 4; wr32(m, esp, 0x9419e4u);
    edi = (ebx + 0x10cu);
    esi = 0x3fu;
    LIFT_CALL(0x78fd60u, 0x84c2b9u);  // FUN_0078fd60
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, 0x9419e4u);
    edi = (ebx + 0x210u);
    LIFT_CALL(0x78fd60u, 0x84c2cau);  // FUN_0078fd60
    eax = rd32(m, esp + 0x34u);
    ecx = rd32(m, eax);
    edx = rd32(m, esp + 0x38u);
    edi = rd32(m, esp + 0x24u);
    wr32(m, ebx + 0x250u, ecx);
    eax = rd32(m, edx);
    esp = esp + 0x10u;
    wr32(m, ebx + 0x254u, eax);
    goto L_84c345;
L_84c2eb:
    ecx = rd32(m, esp + 0x1cu);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esi, 0x4u);
    esp -= 4; wr32(m, esp, 0x9419e4u);
    edi = (ebx + 0x210u);
    esi = 0x3fu;
    LIFT_CALL(0x78fd60u, 0x84c30bu);  // FUN_0078fd60
    edx = rd32(m, ebp);
    edi = rd32(m, esp + 0x1cu);
    esp = esp + 0x8u;
    wr32(m, ebx + 0x10cu, edx);
    goto L_84c345;
L_84c31d:
    eax = rd32(m, esp + 0x1cu);
    ecx = rd32(m, esp + 0x2cu);
    wr32(m, esi, 0x5u);
    wr32(m, ebx + 0x290u, eax);
    wr32(m, ebx + 0x294u, ebp);
    wr32(m, ebx + 0x108u, ecx);
    goto L_84c345;
L_84c33f:
    wr32(m, esi, 0x6u);
L_84c345:
    eax = rd32(m, esp + 0x20u);
    eax = (eax + 1u);
    edx = (eax >> 31) ? 0xffffffffu : 0;
    ecx = 0x3e8u;
    { uint32_t d_ = ecx; sdiv32(eax, edx, d_); }
    eax = (edi + 0x62b24u);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, edi + 0xf4u, edx);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x84c365u);
    edx = rd32(m, edi + 0xf8u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(rd32(m, 0x916034u) /* 8.295148810192616e-39 */, 0x84c372u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    fR_84c376 = (eax & eax);
    if (!(fR_84c376 == 0)) goto L_84c380;
    LIFT_CALL(rd32(m, 0x916078u) /* 8.295188046549617e-39 */, 0x84c380u);
L_84c380:
    LIFT_RET(28);
}

// 0084c5b0 Fn_0084c5b0
//   original FUN_0084c5b0; register inputs: edx esi; returns with ret 0
REFLEX_PORT_FN(0x0084c5b0, Fn_0084c5b0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_84c5c8 = 0;
    uint32_t fB_84c5c8 = 0;
    uint32_t fA_84c5d3 = 0;
    uint32_t fB_84c5d3 = 0;
    uint32_t fA_84c5dc = 0;
    uint32_t fB_84c5dc = 0;
    ecx = rd32(m, edx + 0xf4u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    eax = ecx;
    esi = (esi | 0xffffffffu);
L_84c5c0:
    edi = eax;
    edi = uint32_t(int32_t(edi) * int32_t(0x194u));
    fA_84c5c8 = rd32(m, edi + edx + 0x104u);
    fB_84c5c8 = 0x0u;
    if (fA_84c5c8 == fB_84c5c8) goto L_84c5e2;
    eax = (eax + 1u);
    fA_84c5d3 = eax;
    fB_84c5d3 = 0x3e8u;
    if (int32_t(sext(fA_84c5d3, 4)) < int32_t(sext(fB_84c5d3, 4))) goto L_84c5dc;
    eax = 0u;
L_84c5dc:
    fA_84c5dc = eax;
    fB_84c5dc = ecx;
    if (fA_84c5dc != fB_84c5dc) goto L_84c5c0;
    eax = esi;
L_84c5e2:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0084dea0 Fn_0084dea0
//   original FUN_0084dea0; register inputs: eax esi; returns with ret 4
REFLEX_PORT_FN(0x0084dea0, Fn_0084dea0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_84dec3 = 0;
    uint32_t fA_84dec7 = 0;
    uint32_t fB_84dec7 = 0;
    uint32_t fA_84dece = 0;
    uint32_t fB_84dece = 0;
    uint32_t fA_84ded5 = 0;
    uint32_t fB_84ded5 = 0;
    uint32_t fR_84dedc = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, eax + 0x4u);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, eax);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x84a1f0u, 0x84deb4u);  // FUN_0084a1f0
    edx = 0u;
    ecx = eax;
    { uint32_t d_ = rd32(m, esi); udiv32(eax, edx, d_); }
    eax = rd32(m, esi + 0xcu);
    esp = esp + 0x8u;
    edx = rd32(m, eax + edx * 4);
    fR_84dec3 = (edx & edx);
    if ((fR_84dec3 == 0)) goto L_84dee0;
L_84dec7:
    fA_84dec7 = rd32(m, edx + 0xcu);
    fB_84dec7 = ecx;
    if (fA_84dec7 != fB_84dec7) goto L_84ded9;
    eax = rd32(m, edx);
    fA_84dece = eax;
    fB_84dece = edi;
    if (fA_84dece != fB_84dece) goto L_84ded9;
    eax = rd32(m, edx + 0x4u);
    fA_84ded5 = eax;
    fB_84ded5 = ebx;
    if (fA_84ded5 == fB_84ded5) goto L_84deea;
L_84ded9:
    edx = rd32(m, edx + 0x10u);
    fR_84dedc = (edx & edx);
    if (!(fR_84dedc == 0)) goto L_84dec7;
L_84dee0:
    eax = 0u;
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_84deea:
    ecx = rd32(m, edx + 0x8u);
    edx = rd32(m, ebp + 0x8u);
    edi = rd32(m, esp); esp += 4;
    wr32(m, edx, ecx);
    eax = 0x1u;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 0084ea00 Fn_0084ea00
//   original FUN_0084ea00; register inputs: edi; returns with ret 0
REFLEX_PORT_FN(0x0084ea00, Fn_0084ea00, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_84ea06 = 0;
    uint32_t fA_84ea3d = 0;
    uint32_t fB_84ea3d = 0;
    uint32_t fA_84ea56 = 0;
    uint32_t fB_84ea56 = 0;
    uint32_t fR_84ea6b = 0;
    eax = 0x1u;
    esp -= 4; wr32(m, esp, esi);
    fR_84ea06 = (rd8(m, 0xd94ad8u) & (eax & 0xffu));
    if (!(fR_84ea06 == 0)) goto L_84ea2b;
    wr32(m, 0xd94ad8u, (rd32(m, 0xd94ad8u) | eax));
    ecx = 0xd94ae0u;
    LIFT_CALL(0x881a70u, 0x84ea1eu);  // FUN_00881a70
    esp -= 4; wr32(m, esp, 0x912b10u);
    LIFT_CALL(0x8f9fbfu, 0x84ea28u);  // _atexit
    esp = esp + 0x4u;
L_84ea2b:
    esi = 0xd94ae0u;
    LIFT_CALL(0x881e20u, 0x84ea35u);  // JobPool_WaitIdle
    esi = rd32(m, 0x91607cu) /* 8.295210467325047e-39 */;
    LIFT_CALL(esi, 0x84ea3du);
    fA_84ea3d = eax;
    fB_84ea3d = rd32(m, edi + 0xccu);
    if (fA_84ea3d == fB_84ea3d) goto L_84ea54;
    eax = rd32(m, edi + 0xdcu);
    esp -= 4; wr32(m, esp, 0xffffffffu);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x916008u) /* 8.295117981626401e-39 */, 0x84ea54u);
L_84ea54:
    LIFT_CALL(esi, 0x84ea56u);
    fA_84ea56 = eax;
    fB_84ea56 = rd32(m, edi + 0xccu);
    if (fA_84ea56 == fB_84ea56) goto L_84ea76;
    ecx = rd32(m, edi + 0xdcu);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(rd32(m, 0x916034u) /* 8.295148810192616e-39 */, 0x84ea6bu);
    fR_84ea6b = (eax & eax);
    if (!(fR_84ea6b == 0)) goto L_84ea76;
    esi = rd32(m, esp); esp += 4;
    LIFT_TAILJMP(rd32(m, 0x916078u) /* 8.295188046549617e-39 */);
L_84ea76:
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0084ea80 Engine__UpdateTiming
//   original Engine::UpdateTiming; register inputs: esi; returns with ret 0
REFLEX_PORT_FN(0x0084ea80, Engine__UpdateTiming, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_84eaca = 0;
    uint32_t fB_84eaca = 0;
    FCmp fC_84eb00{};
    uint32_t fR_84eb19 = 0;
    FCmp fC_84eb22{};
    uint32_t fA_84eb5d = 0;
    uint32_t fB_84eb5d = 0;
    FCmp fC_84eba0{};
    uint32_t fA_84eba6 = 0;
    uint32_t fB_84eba6 = 0;
    uint32_t fA_84ebd8 = 0;
    uint32_t fB_84ebd8 = 0;
    FCmp fC_84ebfc{};
    uint32_t fA_84ec56 = 0;
    uint32_t fB_84ec56 = 0;
    uint32_t fR_84ecae = 0;
    FCmp fC_84ecc2{};
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0x40u;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp + 0x3cu, eax);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, edi);
    eax = (esp + 0x8u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x916098u) /* 8.295392636125409e-39 */, 0x84eaa1u);
    edi = rd32(m, esp + 0x8u);
    ebx = rd32(m, esp + 0xcu);
    ecx = rd32(m, esi + 0x68u);
    edx = rd32(m, esi + 0x6cu);
    eax = (esp + 0x10u);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0x1cu, edi);
    wr32(m, esp + 0x20u, ebx);
    wr32(m, esp + 0xcu, ecx);
    wr32(m, esp + 0x10u, edx);
    LIFT_CALL(rd32(m, 0x9160a0u) /* 8.295471108839411e-39 */, 0x84eacau);
    fA_84eaca = edi;
    fB_84eaca = rd32(m, esp + 0x8u);
    edi = fA_84eaca - fB_84eaca;
    x0 = movss_m(rd32(m, esi + 0x14u));
    ebx = ebx - rd32(m, esp + 0xcu) - (fA_84eaca < fB_84eaca ? 1u : 0u);
    wr32(m, esp + 0x8u, edi);
    eax = ebx;
    ebx = (ebx & 0x7fffffffu);
    eax = (eax & 0x80000000u);
    wr32(m, esp + 0xcu, ebx);
    fpush(m, reflex_f80::from_int(int64_t(rd64(m, esp + 0x8u))));
    wr32(m, esp + 0xcu, eax);
    wr32(m, esp + 0x8u, 0x0u);
    fpush(m, reflex_f80::from_int(int64_t(rd64(m, esp + 0x8u))));
    fC_84eb00 = fcmp32(x0.l(0), rd32(m, 0x977494u) /* 3.4028234663852886e+38 */);
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_84eb00) | 2u) << 8);
    st(m, 0).sign ^= 1;
    st(m, 1) = farith(m, 0, st(m, 1), st(m, 0));
    fpop(m);
    fpush(m, reflex_f80::from_int(int64_t(rd64(m, esp + 0x10u))));
    st(m, 1) = farith(m, 6, st(m, 1), st(m, 0));
    fpop(m);
    wr32(m, esp + 0x8u, fst32(m));
    wr32(m, esi + 0x18u, fst32(m));
    fpop(m);
    fR_84eb19 = (((eax >> 8) & 0xffu) & 0x44u);
    if (!parity8(fR_84eb19)) goto L_84eba6;
    fC_84eb22 = fcmp32(x0.l(0), rd32(m, esp + 0x8u));
    if ((fc_b(fC_84eb22) || fc_z(fC_84eb22))) goto L_84eba6;
L_84eb29:
    ecx = (esp + 0x30u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(rd32(m, 0x916098u) /* 8.295392636125409e-39 */, 0x84eb34u);
    edx = rd32(m, esi + 0x68u);
    eax = rd32(m, esi + 0x6cu);
    edi = rd32(m, esp + 0x30u);
    ebx = rd32(m, esp + 0x34u);
    ecx = (esp + 0x28u);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp + 0x1cu, edi);
    wr32(m, esp + 0x20u, ebx);
    wr32(m, esp + 0xcu, edx);
    wr32(m, esp + 0x10u, eax);
    LIFT_CALL(rd32(m, 0x9160a0u) /* 8.295471108839411e-39 */, 0x84eb5du);
    fA_84eb5d = edi;
    fB_84eb5d = rd32(m, esp + 0x8u);
    edi = fA_84eb5d - fB_84eb5d;
    wr32(m, esp + 0x10u, 0x0u);
    ebx = ebx - rd32(m, esp + 0xcu) - (fA_84eb5d < fB_84eb5d ? 1u : 0u);
    wr32(m, esp + 0x38u, edi);
    eax = ebx;
    ebx = (ebx & 0x7fffffffu);
    eax = (eax & 0x80000000u);
    wr32(m, esp + 0x3cu, ebx);
    fpush(m, reflex_f80::from_int(int64_t(rd64(m, esp + 0x38u))));
    wr32(m, esp + 0x14u, eax);
    fpush(m, reflex_f80::from_int(int64_t(rd64(m, esp + 0x10u))));
    st(m, 0).sign ^= 1;
    st(m, 1) = farith(m, 0, st(m, 1), st(m, 0));
    fpop(m);
    fpush(m, reflex_f80::from_int(int64_t(rd64(m, esp + 0x28u))));
    st(m, 1) = farith(m, 6, st(m, 1), st(m, 0));
    fpop(m);
    wr32(m, esi + 0x18u, fst32(m));
    fpop(m);
    x0 = movss_m(rd32(m, esi + 0x14u));
    fC_84eba0 = fcmp32(x0.l(0), rd32(m, esi + 0x18u));
    if (!(fc_b(fC_84eba0) || fc_z(fC_84eba0))) goto L_84eb29;
L_84eba6:
    fA_84eba6 = rd32(m, esi + 0x1cu);
    fB_84eba6 = 0x0u;
    if (fA_84eba6 == fB_84eba6) goto L_84ebf2;
    edx = rd32(m, esi + 0x20u);
    x0 = movss_m(rd32(m, esi + 0x18u));
    x1 = movss_m(rd32(m, esi + 0x38u));
    x1 = subss(x1, rd32(m, esi + edx * 4 + 0x24u));
    eax = (esi + edx * 4 + 0x24u);
    x1 = addss(x1, x0.l(0));
    wr32(m, esi + 0x38u, x1.l(0));
    wr32(m, eax, x0.l(0));
    x0 = movss_m(rd32(m, esi + 0x38u));
    wr32(m, esi + 0x20u, (rd32(m, esi + 0x20u) + 1u));
    fA_84ebd8 = rd32(m, esi + 0x20u);
    fB_84ebd8 = 0x5u;
    x0 = mulss(x0, rd32(m, 0x92e388u) /* 0.20000000298023224 */);
    wr32(m, esi + 0x18u, x0.l(0));
    if (int32_t(sext(fA_84ebd8, 4)) < int32_t(sext(fB_84ebd8, 4))) goto L_84ebf2;
    wr32(m, esi + 0x20u, 0x0u);
L_84ebf2:
    x0 = movss_m(rd32(m, esi + 0x18u));
    x1 = movss_m(rd32(m, esi + 0x48u));
    fC_84ebfc = fcmp32(x0.l(0), x1.l(0));
    if (!fc_b(fC_84ebfc)) goto L_84ec04;
    x1 = x0;
L_84ec04:
    x0 = movss_m(rd32(m, esi + 0x3cu));
    edi = rd32(m, esp + 0x18u);
    ebx = rd32(m, esp + 0x1cu);
    eax = rd32(m, esi + 0x70u);
    ecx = rd32(m, esi + 0x74u);
    wr32(m, esi + 0x18u, x1.l(0));
    wr32(m, 0x9df490u, x0.l(0));
    x0 = addss(x0, x1.l(0));
    x1 = movss_m(rd32(m, esi + 0x44u));
    edx = (esp + 0x28u);
    x1 = mulss(x1, x0.l(0));
    esp -= 4; wr32(m, esp, edx);
    wr32(m, esi + 0x3cu, x0.l(0));
    wr32(m, esi + 0x68u, edi);
    wr32(m, esi + 0x6cu, ebx);
    wr32(m, esi + 0x40u, x1.l(0));
    wr32(m, esp + 0x1cu, eax);
    wr32(m, esp + 0x20u, ecx);
    LIFT_CALL(rd32(m, 0x9160a0u) /* 8.295471108839411e-39 */, 0x84ec54u);
    ecx = edi;
    fA_84ec56 = ecx;
    fB_84ec56 = rd32(m, esp + 0x18u);
    ecx = fA_84ec56 - fB_84ec56;
    eax = ebx;
    eax = eax - rd32(m, esp + 0x1cu) - (fA_84ec56 < fB_84ec56 ? 1u : 0u);
    wr32(m, esp + 0x10u, ecx);
    ecx = eax;
    eax = (eax & 0x7fffffffu);
    wr32(m, esp + 0x14u, eax);
    fpush(m, reflex_f80::from_int(int64_t(rd64(m, esp + 0x10u))));
    ecx = (ecx & 0x80000000u);
    wr32(m, esp + 0x14u, ecx);
    wr32(m, esp + 0x10u, 0x0u);
    fpush(m, reflex_f80::from_int(int64_t(rd64(m, esp + 0x10u))));
    ecx = rd32(m, esi + 0x4cu);
    edx = ecx;
    st(m, 0).sign ^= 1;
    edx = edx - rd32(m, esi + 0x80u);
    st(m, 1) = farith(m, 0, st(m, 1), st(m, 0));
    fpop(m);
    fpush(m, reflex_f80::from_int(int64_t(rd64(m, esp + 0x28u))));
    wr32(m, esp + 0x24u, edx);
    st(m, 1) = farith(m, 6, st(m, 1), st(m, 0));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, 0x9cf5b0u)));
    st(m, 1) = farith(m, 1, st(m, 1), st(m, 0));
    fpush(m, reflex_f80::from_int(int64_t(int32_t(sext(rd32(m, esp + 0x24u), 4)))));
    fR_84ecae = (edx & edx);
    if ((sgn(fR_84ecae, 4) == false)) goto L_84ecb8;
    st(m, 0) = farith(m, 0, st(m, 0), reflex_f80::from_f32(rd32(m, 0x97731cu) /* 4294967296.0 */));
L_84ecb8:
    st(m, 0) = farith(m, 6, st(m, 0), st(m, 2));
    st(m, 0) = farith(m, 1, st(m, 0), st(m, 1));
    wr32(m, esp + 0x24u, fst32(m));
    fpop(m);
    std::swap(st(m, 0), st(m, 1));
    fC_84ecc2 = f80cmp(st(m, 0), st(m, 1));
    fpop(m);
    st(m, 0) = st(m, 0);
    fpop(m);
    if ((fc_b(fC_84ecc2) || fc_z(fC_84ecc2))) goto L_84ed09;
    eax = rd32(m, 0xa95844u);
    ecx = rd32(m, eax + 0xcu);
    edx = rd32(m, ecx);
    edx = rd32(m, edx + 0x78u);
    esp -= 4; wr32(m, esp, 0x1u);
    eax = (esp + 0x28u);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, esi + 0x84u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(edx, 0x84ece5u);
    eax = rd32(m, esi + 0x4cu);
    wr32(m, esi + 0x80u, eax);
    eax = (eax + 1u);
    wr32(m, esi + 0x70u, edi);
    wr32(m, esi + 0x74u, ebx);
    wr32(m, esi + 0x4cu, eax);
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp + 0x3cu);
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x84ed05u);  // __security_check_cookie
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_84ed09:
    ecx = (ecx + 1u);
    wr32(m, esi + 0x4cu, ecx);
    ecx = rd32(m, esp + 0x44u);
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x84ed1au);  // __security_check_cookie
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0084ed20 Engine__Update: Runs the whole frame (Engine::Update): timing, input, worlds, scene tickers, job waits.
//   original Engine::Update; register inputs: none; returns with ret 4
REFLEX_PORT_FN(0x0084ed20, Engine__Update, "engine_mt", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_84ed56 = 0;
    uint32_t fA_84eda9 = 0;
    uint32_t fB_84eda9 = 0;
    uint32_t fA_84edb7 = 0;
    uint32_t fB_84edb7 = 0;
    uint32_t fA_84edca = 0;
    uint32_t fB_84edca = 0;
    uint32_t fA_84edd0 = 0;
    uint32_t fB_84edd0 = 0;
    uint32_t fR_84edf0 = 0;
    uint32_t fA_84edfd = 0;
    uint32_t fB_84edfd = 0;
    uint32_t fR_84ee0c = 0;
    uint32_t fA_84ee26 = 0;
    uint32_t fB_84ee26 = 0;
    uint32_t fA_84ee59 = 0;
    uint32_t fB_84ee59 = 0;
    uint32_t fA_84ee67 = 0;
    uint32_t fB_84ee67 = 0;
    uint32_t fR_84ee6c = 0;
    uint32_t fA_84eeaa = 0;
    uint32_t fB_84eeaa = 0;
    uint32_t fA_84eeca = 0;
    uint32_t fB_84eeca = 0;
    uint32_t fA_84eedd = 0;
    uint32_t fB_84eedd = 0;
    uint32_t fA_84eeeb = 0;
    uint32_t fB_84eeeb = 0;
    uint32_t fR_84eef0 = 0;
    uint32_t fR_84ef34 = 0;
    uint32_t fA_84ef5c = 0;
    uint32_t fB_84ef5c = 0;
    uint32_t fR_84ef6e = 0;
    uint32_t fR_84efaa = 0;
    uint32_t fA_84eff9 = 0;
    uint32_t fB_84eff9 = 0;
    uint32_t fA_84f016 = 0;
    uint32_t fB_84f016 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0x14u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    esi = ebx;
    LIFT_CALL(0x84ea80u, 0x84ed36u);  // Engine::UpdateTiming
    ecx = rd32(m, 0xa95844u);
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0x8u);
    LIFT_CALL(edx, 0x84ed43u);
    fpush(m, reflex_f80::from_f32(rd32(m, ebx + 0x18u)));
    edx = rd32(m, eax);
    esp -= 4; wr32(m, esp, ecx);
    ecx = eax;
    wr32(m, esp, fst32(m));
    fpop(m);
    eax = rd32(m, edx + 0xacu);
    LIFT_CALL(eax, 0x84ed56u);
    fR_84ed56 = (rd8(m, 0xd92f48u) & 0x1u);
    x0 = movss_m(rd32(m, ebx + 0x18u));
    wr32(m, esp + 0x1cu, x0.l(0));
    if (!(fR_84ed56 == 0)) goto L_84ed83;
    wr32(m, 0xd92f48u, (rd32(m, 0xd92f48u) | 0x1u));
    LIFT_CALL(0x812170u, 0x84ed76u);  // InputNames_Init
    esp -= 4; wr32(m, esp, 0x9127c0u);
    LIFT_CALL(0x8f9fbfu, 0x84ed80u);  // _atexit
    esp = esp + 0x4u;
L_84ed83:
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x1cu)));
    esp -= 4; wr32(m, esp, ecx);
    edi = 0xd92f50u;
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x813de0u, 0x84ed95u);  // FUN_00813de0
    esi = 0x1cu;
L_84eda0:
    ecx = rd32(m, 0xa95844u);
    edi = rd32(m, esi + ecx);
    fA_84eda9 = rd32(m, edi + 0x34u);
    fB_84eda9 = 0x0u;
    if (fA_84eda9 == fB_84eda9) goto L_84edb4;
    LIFT_CALL(0x7d21d0u, 0x84edb4u);  // FUN_007d21d0
L_84edb4:
    esi = esi + 0x4u;
    fA_84edb7 = esi;
    fB_84edb7 = 0x28u;
    if (fA_84edb7 < fB_84edb7) goto L_84eda0;
    esi = 0x1cu;
L_84edc1:
    edx = rd32(m, 0xa95844u);
    eax = rd32(m, esi + edx);
    fA_84edca = rd32(m, eax + 0x34u);
    fB_84edca = 0x0u;
    if (fA_84edca == fB_84edca) goto L_84edfa;
    fA_84edd0 = rd32(m, eax + 0x2cacu);
    fB_84edd0 = 0x0u;
    if (fA_84edd0 != fB_84edd0) goto L_84edfa;
    wr32(m, eax + 0x2cacu, 0x1u);
    eax = rd32(m, eax + 0x2ca4u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x916034u) /* 8.295148810192616e-39 */, 0x84edf0u);
    fR_84edf0 = (eax & eax);
    if (!(fR_84edf0 == 0)) goto L_84edfa;
    LIFT_CALL(rd32(m, 0x916078u) /* 8.295188046549617e-39 */, 0x84edfau);
L_84edfa:
    esi = esi + 0x4u;
    fA_84edfd = esi;
    fB_84edfd = 0x28u;
    if (fA_84edfd < fB_84edfd) goto L_84edc1;
    ecx = rd32(m, ebx + 0x8u);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x18u);
    LIFT_CALL(eax, 0x84ee0cu);
    fR_84ee0c = (eax & eax);
    if (!(fR_84ee0c == 0)) goto L_84ee19;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_84ee19:
    x0 = movss_m(rd32(m, ebx + 0x18u));
    edi = 0u;
    wr32(m, esp + 0x1cu, x0.l(0));
    fA_84ee26 = rd32(m, 0x9b7430u);
    fB_84ee26 = edi;
    if (fA_84ee26 != fB_84ee26) goto L_84ee40;
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, edi);
    ecx = (esp + 0x28u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, 0x17u);
    LIFT_CALL(0x805860u, 0x84ee3du);  // Audio_PostCommand
    esp = esp + 0x14u;
L_84ee40:
    esi = 0x1cu;
    goto L_84ee50;
L_84ee50:
    edx = rd32(m, 0xa95844u);
    eax = rd32(m, esi + edx);
    fA_84ee59 = rd32(m, eax + 0x34u);
    fB_84ee59 = edi;
    if (fA_84ee59 == fB_84ee59) goto L_84ee64;
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x7d23a0u, 0x84ee64u);  // FUN_007d23a0
L_84ee64:
    esi = esi + 0x4u;
    fA_84ee67 = esi;
    fB_84ee67 = 0x28u;
    if (fA_84ee67 < fB_84ee67) goto L_84ee50;
    fR_84ee6c = (rd8(m, 0xd94ad8u) & 0x1u);
    if (!(fR_84ee6c == 0)) goto L_84ee93;
    wr32(m, 0xd94ad8u, (rd32(m, 0xd94ad8u) | 0x1u));
    ecx = 0xd94ae0u;
    LIFT_CALL(0x881a70u, 0x84ee86u);  // FUN_00881a70
    esp -= 4; wr32(m, esp, 0x912b10u);
    LIFT_CALL(0x8f9fbfu, 0x84ee90u);  // _atexit
    esp = esp + 0x4u;
L_84ee93:
    esi = 0xd94ae0u;
    LIFT_CALL(0x881e20u, 0x84ee9du);  // JobPool_WaitIdle
    esi = 0x1cu;
L_84eea2:
    eax = rd32(m, 0xa95844u);
    eax = rd32(m, esi + eax);
    fA_84eeaa = rd32(m, eax + 0x2cacu);
    fB_84eeaa = edi;
    if (fA_84eeaa == fB_84eeaa) goto L_84eec7;
    ecx = rd32(m, eax + 0x2ca8u);
    esp -= 4; wr32(m, esp, 0xffffffffu);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, eax + 0x2cacu, edi);
    LIFT_CALL(rd32(m, 0x916008u) /* 8.295117981626401e-39 */, 0x84eec7u);
L_84eec7:
    esi = esi + 0x4u;
    fA_84eeca = esi;
    fB_84eeca = 0x28u;
    if (fA_84eeca < fB_84eeca) goto L_84eea2;
    esi = 0x1cu;
L_84eed4:
    edx = rd32(m, 0xa95844u);
    eax = rd32(m, esi + edx);
    fA_84eedd = rd32(m, eax + 0x34u);
    fB_84eedd = edi;
    if (fA_84eedd == fB_84eedd) goto L_84eee8;
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x7d2490u, 0x84eee8u);  // FUN_007d2490
L_84eee8:
    esi = esi + 0x4u;
    fA_84eeeb = esi;
    fB_84eeeb = 0x28u;
    if (fA_84eeeb < fB_84eeeb) goto L_84eed4;
    fR_84eef0 = (rd8(m, 0xd94ad8u) & 0x1u);
    wr32(m, 0xd6db00u, edi);
    if (!(fR_84eef0 == 0)) goto L_84ef1d;
    wr32(m, 0xd94ad8u, (rd32(m, 0xd94ad8u) | 0x1u));
    ecx = 0xd94ae0u;
    LIFT_CALL(0x881a70u, 0x84ef10u);  // FUN_00881a70
    esp -= 4; wr32(m, esp, 0x912b10u);
    LIFT_CALL(0x8f9fbfu, 0x84ef1au);  // _atexit
    esp = esp + 0x4u;
L_84ef1d:
    esi = 0xd94ae0u;
    LIFT_CALL(0x881e20u, 0x84ef27u);  // JobPool_WaitIdle
    eax = rd32(m, ebx + 0x90u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x916034u) /* 8.295148810192616e-39 */, 0x84ef34u);
    fR_84ef34 = (eax & eax);
    if (!(fR_84ef34 == 0)) goto L_84ef3e;
    LIFT_CALL(rd32(m, 0x916078u) /* 8.295188046549617e-39 */, 0x84ef3eu);
L_84ef3e:
    edi = 0xd70f20u;
    wr32(m, esp + 0x14u, 0x24u);
    wr32(m, esp + 0x1cu, edi);
L_84ef50:
    eax = rd32(m, 0xa95844u);
    ecx = rd32(m, esp + 0x14u);
    esi = rd32(m, ecx + eax);
    fA_84ef5c = rd32(m, esi + 0x34u);
    fB_84ef5c = 0x0u;
    if (fA_84ef5c == fB_84ef5c) goto L_84ef95;
    eax = rd32(m, eax + 0x14u);
    ebx = rd32(m, ebx + 0x58u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x7ae4e0u, 0x84ef6eu);  // Renderer_EnsureDeviceReady
    fR_84ef6e = ((eax & 0xffu) & (eax & 0xffu));
    if ((fR_84ef6e == 0)) goto L_84ef92;
    esp -= 4; wr32(m, esp, 0x971fa0u);
    LIFT_CALL(0x7d1cd0u, 0x84ef7cu);  // FUN_007d1cd0
    esp -= 4; wr32(m, esp, ebx);
    ecx = (esi + 0x1a18u);
    LIFT_CALL(0x7e0fc0u, 0x84ef88u);  // FUN_007e0fc0
    esp -= 4; wr32(m, esp, 0x971fa0u);
    LIFT_CALL(0x7d1cd0u, 0x84ef92u);  // FUN_007d1cd0
L_84ef92:
    ebx = rd32(m, ebp + 0x8u);
L_84ef95:
    edx = rd32(m, esi + 0x34u);
    wr32(m, edi - 0xf0u, edx);
    eax = rd32(m, esi + 0x19d0u);
    wr32(m, edi - 0xecu, eax);
    fR_84efaa = (eax & eax);
    if ((false || (fR_84efaa == 0))) goto L_84f00b;
    esi = esi + 0x19b8u;
    wr32(m, esp + 0x10u, esi);
    ebx = edi;
    wr32(m, esp + 0x18u, eax);
L_84efbe:
    eax = rd32(m, esp + 0x10u);
    eax = rd32(m, eax);
    esi = eax;
    ecx = 0x21u;
    edi = ebx;
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    ecx = rd32(m, eax);
    esp -= 4; wr32(m, esp, ecx);
    esi = (ebx + 0x90u);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x6a5b30u, 0x84efdeu);  // FUN_006a5b30
    edx = rd32(m, ebp + 0x8u);
    wr32(m, esp + 0x10u, rd32(m, esp + 0x10u) + 0x4u);
    eax = 0x1u;
    wr32(m, ebx, esi);
    wr32(m, ebx + 0x14u, eax);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x18u)));
    ebx = ebx + 0x440u;
    fA_84eff9 = rd32(m, esp + 0x18u);
    fB_84eff9 = eax;
    wr32(m, esp + 0x18u, fA_84eff9 - fB_84eff9);
    wr32(m, ebx - 0x1d0u, fst32(m));
    fpop(m);
    if (fA_84eff9 != fB_84eff9) goto L_84efbe;
    edi = rd32(m, esp + 0x1cu);
    ebx = edx;
L_84f00b:
    wr32(m, esp + 0x14u, rd32(m, esp + 0x14u) - 0x4u);
    edi = edi - 0x1990u;
    fA_84f016 = edi;
    fB_84f016 = 0xd6dc00u;
    wr32(m, esp + 0x1cu, edi);
    if (int32_t(sext(fA_84f016, 4)) >= int32_t(sext(fB_84f016, 4))) goto L_84ef50;
    eax = rd32(m, ebx + 0x94u);
    esp -= 4; wr32(m, esp, 0xffffffffu);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x916008u) /* 8.295117981626401e-39 */, 0x84f035u);
    ecx = rd32(m, 0xa95844u);
    ecx = rd32(m, ecx + 0xcu);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0xcu);
    LIFT_CALL(eax, 0x84f045u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    eax = 0x1u;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 0084f320 Engine_TaskThread
//   original Engine_TaskThread; register inputs: edi; returns with ret 0
REFLEX_PORT_FN(0x0084f320, Engine_TaskThread, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_84f341 = 0;
    uint32_t fB_84f341 = 0;
    uint32_t fR_84f348 = 0;
    uint32_t fA_84f35e = 0;
    uint32_t fB_84f35e = 0;
    uint32_t fR_84f37d = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, 0x916008u) /* 8.295117981626401e-39 */;
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = edi;
L_84f330:
    eax = rd32(m, ebx + 0x90u);
    esp -= 4; wr32(m, esp, 0xffffffffu);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALLB(ebp, 0x84f33bu);
    eax = rd32(m, ebx + 0xc0u);
    fA_84f341 = eax;
    fB_84f341 = 0xffffffffu;
    if (fA_84f341 == fB_84f341) goto L_84f389;
    esi = 0u;
    fR_84f348 = (eax & eax);
    if ((false || (fR_84f348 == 0))) goto L_84f366;
    edi = (ebx + 0x98u);
L_84f352:
    ecx = rd32(m, edi);
    edx = rd32(m, ecx);
    eax = rd32(m, edx);
    LIFT_CALLB(eax, 0x84f35au);
    esi = (esi + 1u);
    edi = edi + 0x4u;
    fA_84f35e = esi;
    fB_84f35e = rd32(m, ebx + 0xc0u);
    if (fA_84f35e < fB_84f35e) goto L_84f352;
L_84f366:
    ecx = rd32(m, ebx + 0x94u);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, ebx + 0xc0u, 0x0u);
    LIFT_CALLB(rd32(m, 0x916034u) /* 8.295148810192616e-39 */, 0x84f37du);
    fR_84f37d = (eax & eax);
    if (!(fR_84f37d == 0)) goto L_84f330;
    LIFT_CALLB(rd32(m, 0x916078u) /* 8.295188046549617e-39 */, 0x84f387u);
    goto L_84f330;
L_84f389:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00881900 JobPool_WorkerThread
//   original JobPool_WorkerThread; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00881900, JobPool_WorkerThread, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_881909 = 0;
    uint32_t fR_881956 = 0;
    uint32_t fR_881967 = 0;
    uint32_t fA_8819a2 = 0;
    uint32_t fB_8819a2 = 0;
    uint32_t fR_8819bc = 0;
    uint32_t fR_881a39 = 0;
    uint32_t fR_881a4f = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0x44u;
    fR_881909 = (rd8(m, 0xd94ad8u) & 0x1u);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = 0x1u;
    if (!(fR_881909 == 0)) goto L_881937;
    wr32(m, 0xd94ad8u, (rd32(m, 0xd94ad8u) | edi));
    ecx = 0xd94ae0u;
    LIFT_CALLB(0x881a70u, 0x88192au);  // FUN_00881a70
    esp -= 4; wr32(m, esp, 0x912b10u);
    LIFT_CALLB(0x8f9fbfu, 0x881934u);  // _atexit
    esp = esp + 0x4u;
L_881937:
    esi = rd32(m, ebp + 0x8u);
    eax = rd32(m, esi * 4 + 0xd9d35cu);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALLB(rd32(m, 0x916090u) /* 8.295350597171479e-39 */, 0x881948u);
    ecx = rd32(m, esi * 4 + 0xd9d374u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALLB(rd32(m, 0x916034u) /* 8.295148810192616e-39 */, 0x881956u);
    fR_881956 = (eax & eax);
    if (!(fR_881956 == 0)) goto L_881960;
    LIFT_CALLB(rd32(m, 0x916078u) /* 8.295188046549617e-39 */, 0x881960u);
L_881960:
    edx = rd32(m, esi * 4 + 0xd9d3a4u);
    fR_881967 = (edx & edx);
    if (!(fR_881967 == 0)) goto L_881a41;
L_881970:
    eax = rd32(m, esi * 4 + 0xd9d35cu);
    esp -= 4; wr32(m, esp, 0xffffffffu);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALLB(rd32(m, 0x916008u) /* 8.295117981626401e-39 */, 0x881980u);
    esp -= 4; wr32(m, esp, 0xd9d320u);
    wr32(m, esi * 4 + 0xd9d3bcu, edi);
    LIFT_CALLB(rd32(m, 0x916080u) /* 8.295241295891262e-39 */, 0x881992u);
L_881992:
    esp -= 4; wr32(m, esp, 0xd94ae0u);
    LIFT_CALLB(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x88199du);
    eax = rd32(m, 0xd9d318u);
    fA_8819a2 = eax;
    fB_8819a2 = 0x200u;
    if (int32_t(sext(fA_8819a2, 4)) < int32_t(sext(fB_8819a2, 4))) goto L_8819b0;
    eax = 0u;
    wr32(m, 0xd9d318u, eax);
L_8819b0:
    ecx = rd32(m, eax * 4 + 0xd9cb10u);
    esp -= 4; wr32(m, esp, 0xd94ae0u);
    fR_8819bc = (ecx & ecx);
    if ((fR_8819bc == 0)) goto L_881a16;
    wr32(m, 0xd9d318u, rd32(m, 0xd9d318u) + edi);
    ebx = rd32(m, eax * 4 + 0xd9cb10u);
    esi = eax;
    esi = (esi << 6);
    esi = esi + 0xd94b10u;
    ecx = 0x10u;
    edi = (esp + 0x14u);
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    wr32(m, eax * 4 + 0xd9cb10u, 0x0u);
    LIFT_CALLB(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x8819f4u);
    edx = (esp + 0x10u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALLB(ebx, 0x8819fbu);
    esp = esp + 0x4u;
    esp -= 4; wr32(m, esp, 0xd9d310u);
    LIFT_CALLB(rd32(m, 0x916084u) /* 8.295274927054406e-39 */, 0x881a09u);
    esi = rd32(m, ebp + 0x8u);
    edi = 0x1u;
    goto L_881992;
L_881a16:
    LIFT_CALLB(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x881a1cu);
    esp -= 4; wr32(m, esp, 0xd9d320u);
    LIFT_CALLB(rd32(m, 0x916084u) /* 8.295274927054406e-39 */, 0x881a27u);
    wr32(m, esi * 4 + 0xd9d3bcu, 0x0u);
    eax = rd32(m, esi * 4 + 0xd9d3a4u);
    fR_881a39 = (eax & eax);
    if ((fR_881a39 == 0)) goto L_881970;
L_881a41:
    ecx = rd32(m, esi * 4 + 0xd9d38cu);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALLB(rd32(m, 0x916034u) /* 8.295148810192616e-39 */, 0x881a4fu);
    fR_881a4f = (eax & eax);
    if (!(fR_881a4f == 0)) goto L_881a59;
    LIFT_CALLB(rd32(m, 0x916078u) /* 8.295188046549617e-39 */, 0x881a59u);
L_881a59:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00881e20 JobPool_WaitIdle: Waits until the engine job pool is idle (wakes workers, Sleep(1) polling).
//   original JobPool_WaitIdle; register inputs: esi; returns with ret 0
REFLEX_PORT_FN(0x00881e20, JobPool_WaitIdle, "engine_mt", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_881e53 = 0;
    uint32_t fB_881e53 = 0;
    uint32_t fR_881e59 = 0;
    uint32_t fR_881e77 = 0;
    uint32_t fR_881e81 = 0;
    uint32_t fA_881e89 = 0;
    uint32_t fB_881e89 = 0;
    uint32_t fR_881e9e = 0;
    uint32_t fA_881eb2 = 0;
    uint32_t fB_881eb2 = 0;
    uint32_t fA_881edc = 0;
    uint32_t fB_881edc = 0;
    uint32_t fR_881ee2 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0x8u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, edi);
    edi = (esi + 0x18u);
    esp -= 4; wr32(m, esp, edi);
    wr32(m, esp + 0x10u, edi);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x881e39u);
    eax = (esi + 0x8830u);
    wr32(m, 0xa9598cu, 0x0u);
    ecx = 0u;
    edx = eax;
    eax = rd32(m, edx);
L_881e4f:
    ebx = eax;
    ebx = (ebx | ecx);
    fA_881e53 = eax;
    fB_881e53 = rd32(m, edx);
    if (fA_881e53 == fB_881e53) { wr32(m, edx, ebx); } else eax = fB_881e53;
    if (fA_881e53 != fB_881e53) goto L_881e4f;
    fR_881e59 = (eax & eax);
    if ((fR_881e59 == 0)) goto L_881eea;
    edi = rd32(m, 0x916020u) /* 8.295000272555398e-39 */;
L_881e67:
    ecx = 0u;
    eax = (esi + 0x88dcu);
L_881e70:
    edx = 0x1u;
    edx = (edx << ((ecx & 0xffu) & 31u));
    fR_881e77 = (rd32(m, esi + 0x8844u) & edx);
    if ((fR_881e77 == 0)) goto L_881e85;
    edx = rd32(m, eax);
    fR_881e81 = (edx & edx);
    if ((fR_881e81 == 0)) goto L_881e90;
L_881e85:
    ecx = (ecx + 1u);
    eax = eax + 0x4u;
    fA_881e89 = ecx;
    fB_881e89 = 0x2u;
    if (int32_t(sext(fA_881e89, 4)) < int32_t(sext(fB_881e89, 4))) goto L_881e70;
    goto L_881ea8;
L_881e90:
    eax = rd32(m, esi + ecx * 4 + 0x887cu);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x916034u) /* 8.295148810192616e-39 */, 0x881e9eu);
    fR_881e9e = (eax & eax);
    if (!(fR_881e9e == 0)) goto L_881ea8;
    LIFT_CALL(rd32(m, 0x916078u) /* 8.295188046549617e-39 */, 0x881ea8u);
L_881ea8:
    esp -= 4; wr32(m, esp, 0x1u);
    LIFT_CALL(edi, 0x881eacu);
    eax = rd32(m, 0xa9598cu);
    eax = (eax + 1u);
    fA_881eb2 = eax;
    fB_881eb2 = 0xau;
    wr32(m, 0xa9598cu, eax);
    if (int32_t(sext(fA_881eb2, 4)) <= int32_t(sext(fB_881eb2, 4))) goto L_881ecc;
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x881d80u, 0x881ec2u);  // FUN_00881d80
    wr32(m, 0xa9598cu, 0x0u);
L_881ecc:
    eax = (esi + 0x8830u);
    ecx = 0u;
    edx = eax;
    eax = rd32(m, edx);
L_881ed8:
    ebx = eax;
    ebx = (ebx | ecx);
    fA_881edc = eax;
    fB_881edc = rd32(m, edx);
    if (fA_881edc == fB_881edc) { wr32(m, edx, ebx); } else eax = fB_881edc;
    if (fA_881edc != fB_881edc) goto L_881ed8;
    fR_881ee2 = (eax & eax);
    if (!(fR_881ee2 == 0)) goto L_881e67;
    edi = rd32(m, esp + 0xcu);
L_881eea:
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x881ef1u);
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00881f00 JobPool_Submit: Queues a job in the engine job pool and wakes an idle worker (SetEvent).
//   original JobPool_Submit; register inputs: none; returns with ret 12
REFLEX_PORT_FN(0x00881f00, JobPool_Submit, "engine_mt", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_881f00 = 0;
    uint32_t fB_881f00 = 0;
    uint32_t fA_881f24 = 0;
    uint32_t fB_881f24 = 0;
    uint32_t fR_881f48 = 0;
    uint32_t fR_881f77 = 0;
    uint32_t fR_881f81 = 0;
    uint32_t fA_881f89 = 0;
    uint32_t fB_881f89 = 0;
    uint32_t fR_881f9e = 0;
    uint32_t fR_881fc3 = 0;
    uint32_t fA_88201b = 0;
    uint32_t fB_88201b = 0;
    uint32_t fA_882021 = 0;
    uint32_t fB_882021 = 0;
    uint32_t fR_882037 = 0;
    uint32_t fR_882041 = 0;
    uint32_t fA_882049 = 0;
    uint32_t fB_882049 = 0;
    uint32_t fR_882068 = 0;
    fA_881f00 = rd32(m, esp + 0xcu);
    fB_881f00 = 0x0u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x8u);
    if (fA_881f00 == fB_881f00) goto L_88207a;
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, 0x91600cu) /* 8.29485173491818e-39 */;
    eax = (ebx + 0x18u);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0x10u, eax);
    LIFT_CALL(esi, 0x881f21u);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(esi, 0x881f24u);
    fA_881f24 = rd32(m, ebx + 0x8834u);
    fB_881f24 = 0x200u;
    if (int32_t(sext(fA_881f24, 4)) < int32_t(sext(fB_881f24, 4))) goto L_881f3a;
    wr32(m, ebx + 0x8834u, 0x0u);
L_881f3a:
    eax = rd32(m, ebx + 0x8834u);
    ecx = rd32(m, ebx + eax * 4 + 0x8030u);
    esp -= 4; wr32(m, esp, edi);
    fR_881f48 = (ecx & ecx);
    if ((fR_881f48 == 0)) goto L_881fc8;
    edi = rd32(m, 0x916020u) /* 8.295000272555398e-39 */;
    esp -= 4; wr32(m, esp, ebp);
    ebp = (ebx + 0x88dcu);
L_881f60:
    ecx = 0u;
    eax = ebp;
    goto L_881f70;
L_881f70:
    edx = 0x1u;
    edx = (edx << ((ecx & 0xffu) & 31u));
    fR_881f77 = (rd32(m, ebx + 0x8844u) & edx);
    if ((fR_881f77 == 0)) goto L_881f85;
    edx = rd32(m, eax);
    fR_881f81 = (edx & edx);
    if ((fR_881f81 == 0)) goto L_881f90;
L_881f85:
    ecx = (ecx + 1u);
    eax = eax + 0x4u;
    fA_881f89 = ecx;
    fB_881f89 = 0x2u;
    if (int32_t(sext(fA_881f89, 4)) < int32_t(sext(fB_881f89, 4))) goto L_881f70;
    goto L_881fa8;
L_881f90:
    eax = rd32(m, ebx + ecx * 4 + 0x887cu);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x916034u) /* 8.295148810192616e-39 */, 0x881f9eu);
    fR_881f9e = (eax & eax);
    if (!(fR_881f9e == 0)) goto L_881fa8;
    LIFT_CALL(rd32(m, 0x916078u) /* 8.295188046549617e-39 */, 0x881fa8u);
L_881fa8:
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x881fafu);
    esp -= 4; wr32(m, esp, 0x1u);
    LIFT_CALL(edi, 0x881fb3u);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(esi, 0x881fb6u);
    ecx = rd32(m, ebx + 0x8834u);
    edx = rd32(m, ebx + ecx * 4 + 0x8030u);
    fR_881fc3 = (edx & edx);
    if (!(fR_881fc3 == 0)) goto L_881f60;
    ebp = rd32(m, esp); esp += 4;
L_881fc8:
    eax = rd32(m, ebx + 0x8834u);
    ecx = rd32(m, esp + 0x18u);
    esi = rd32(m, esp + 0x14u);
    wr32(m, ebx + eax * 4 + 0x8030u, ecx);
    edx = rd32(m, ebx + 0x8834u);
    edx = (edx << 6);
    edi = (edx + ebx + 0x30u);
    eax = (ebx + 0x8830u);
    ecx = 0x10u;
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    wr32(m, ebx + 0x8834u, (rd32(m, ebx + 0x8834u) + 1u));
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x916080u) /* 8.295241295891262e-39 */, 0x882004u);
    esi = rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */;
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(esi, 0x88200du);
    ecx = 0u;
    edx = (ebx + 0x8840u);
    eax = rd32(m, edx);
L_882017:
    edi = eax;
    edi = (edi | ecx);
    fA_88201b = eax;
    fB_88201b = rd32(m, edx);
    if (fA_88201b == fB_88201b) { wr32(m, edx, edi); } else eax = fB_88201b;
    if (fA_88201b != fB_88201b) goto L_882017;
    fA_882021 = eax;
    fB_882021 = 0x3u;
    edi = rd32(m, esp); esp += 4;
    if (int32_t(sext(fA_882021, 4)) >= int32_t(sext(fB_882021, 4))) goto L_882072;
    eax = (ebx + 0x88dcu);
L_882030:
    edx = 0x1u;
    edx = (edx << ((ecx & 0xffu) & 31u));
    fR_882037 = (rd32(m, ebx + 0x8844u) & edx);
    if ((fR_882037 == 0)) goto L_882045;
    edx = rd32(m, eax);
    fR_882041 = (edx & edx);
    if ((fR_882041 == 0)) goto L_88205a;
L_882045:
    ecx = (ecx + 1u);
    eax = eax + 0x4u;
    fA_882049 = ecx;
    fB_882049 = 0x2u;
    if (int32_t(sext(fA_882049, 4)) < int32_t(sext(fB_882049, 4))) goto L_882030;
    ecx = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(esi, 0x882055u);
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(12);
L_88205a:
    eax = rd32(m, ebx + ecx * 4 + 0x887cu);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x916034u) /* 8.295148810192616e-39 */, 0x882068u);
    fR_882068 = (eax & eax);
    if (!(fR_882068 == 0)) goto L_882072;
    LIFT_CALL(rd32(m, 0x916078u) /* 8.295188046549617e-39 */, 0x882072u);
L_882072:
    ecx = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(esi, 0x882079u);
    esi = rd32(m, esp); esp += 4;
L_88207a:
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(12);
}

// 00883ac0 Fn_00883ac0
//   original FUN_00883ac0; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x00883ac0, Fn_00883ac0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = rd32(m, ecx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x883ac9u);
    LIFT_RET(0);
}

// 00883ad0 Fn_00883ad0
//   original FUN_00883ad0; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x00883ad0, Fn_00883ad0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = rd32(m, ecx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x883ad9u);
    LIFT_RET(0);
}

// 00883d80 Fn_00883d80
//   original FUN_00883d80; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00883d80, Fn_00883d80, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_883d80 = 0;
    uint32_t fB_883d80 = 0;
    fA_883d80 = rd8(m, 0x9df580u);
    fB_883d80 = 0x0u;
    if (fA_883d80 != fB_883d80) goto L_883d8e;
    LIFT_TAILJMP(0x883ac0u);  // FUN_00883ac0
L_883d8e:
    LIFT_RET(0);
}

// 00883d90 Fn_00883d90
//   original FUN_00883d90; register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x00883d90, Fn_00883d90, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_883d90 = 0;
    uint32_t fB_883d90 = 0;
    fA_883d90 = rd8(m, 0x9df580u);
    fB_883d90 = 0x0u;
    if (fA_883d90 != fB_883d90) goto L_883d9e;
    LIFT_TAILJMP(0x883ad0u);  // FUN_00883ad0
L_883d9e:
    LIFT_RET(0);
}

// 008841d0 Fn_008841d0
//   original FUN_008841d0; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x008841d0, Fn_008841d0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_8841d4 = 0;
    uint32_t fB_8841d4 = 0;
    eax = rd32(m, esp + 0x4u);
    fA_8841d4 = eax;
    fB_8841d4 = 0xbu;
    if (fA_8841d4 < fB_8841d4) goto L_8841f2;
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0xe0000003u);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0x0u);
    LIFT_CALL(0x884390u, 0x8841e9u);  // FUN_00884390
    esp = esp + 0x10u;
    eax = (eax | 0xffffffffu);
    LIFT_RET(4);
L_8841f2:
    eax = rd32(m, ecx + eax * 4);
    LIFT_RET(4);
}

// 00884390 Fn_00884390
//   original FUN_00884390; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x00884390, Fn_00884390, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_884391 = 0;
    uint32_t fB_884391 = 0;
    uint32_t fA_8843c0 = 0;
    uint32_t fB_8843c0 = 0;
    uint32_t fA_8843d3 = 0;
    uint32_t fB_8843d3 = 0;
    uint32_t fA_884404 = 0;
    uint32_t fB_884404 = 0;
    esp -= 4; wr32(m, esp, ecx);
    fA_884391 = rd8(m, 0x9df580u);
    fB_884391 = 0x0u;
    if (fA_884391 != fB_884391) goto L_8843a4;
    ecx = 0x9df638u;
    LIFT_CALL(0x883d80u, 0x8843a4u);  // FUN_00883d80
L_8843a4:
    LIFT_CALL(0x8a0500u, 0x8843a9u);  // KERNEL32.DLL::GetCurrentThreadId
    wr32(m, esp, eax);
    eax = (esp);
    esp -= 4; wr32(m, esp, eax);
    ecx = 0x9df64cu;
    LIFT_CALL(0x8c1270u, 0x8843bau);  // FUN_008c1270
    ecx = rd32(m, esp + 0x10u);
    wr32(m, eax, ecx);
    fA_8843c0 = rd8(m, 0x9df580u);
    fB_8843c0 = 0x0u;
    if (fA_8843c0 != fB_8843c0) goto L_8843e6;
    ecx = 0x9df638u;
    LIFT_CALL(0x883d90u, 0x8843d3u);  // FUN_00883d90
    fA_8843d3 = rd8(m, 0x9df580u);
    fB_8843d3 = 0x0u;
    if (fA_8843d3 != fB_8843d3) goto L_8843e6;
    ecx = 0x9df5f0u;
    LIFT_CALL(0x883d80u, 0x8843e6u);  // FUN_00883d80
L_8843e6:
    LIFT_CALL(0x8a0500u, 0x8843ebu);  // KERNEL32.DLL::GetCurrentThreadId
    edx = (esp + 0x10u);
    esp -= 4; wr32(m, esp, edx);
    ecx = 0x9df604u;
    wr32(m, esp + 0x14u, eax);
    LIFT_CALL(0x8c1270u, 0x8843feu);  // FUN_008c1270
    ecx = rd32(m, esp + 0x14u);
    wr32(m, eax, ecx);
    fA_884404 = rd8(m, 0x9df580u);
    fB_884404 = 0x0u;
    if (fA_884404 != fB_884404) goto L_88441a;
    ecx = 0x9df5f0u;
    esp = esp + 0x4u;
    LIFT_TAILJMP(0x883d90u);  // FUN_00883d90
L_88441a:
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00885050 Fn_00885050
//   original FUN_00885050; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x00885050, Fn_00885050, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_885052 = 0;
    uint32_t fR_885088 = 0;
    eax = rd32(m, ecx);
    fR_885052 = (eax & eax);
    if (!(fR_885052 == 0)) goto L_885069;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, 0xa0030004u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x884390u, 0x885063u);  // FUN_00884390
    esp = esp + 0x10u;
    eax = (eax & 0xffffff00u) | ((0u) & 0xffu);
    LIFT_RET(0);
L_885069:
    eax = rd32(m, eax + 0x50u);
    eax = (eax >> 22);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x8a32a0u, 0x885075u);  // FUN_008a32a0
    ecx = rd32(m, 0x9dfd9cu);
    edx = rd32(m, eax);
    edx = rd32(m, edx + 0x3cu);
    esp = esp + 0x4u;
    esp -= 4; wr32(m, esp, ecx);
    ecx = eax;
    LIFT_CALL(edx, 0x885088u);
    fR_885088 = ((eax & 0xffu) & (eax & 0xffu));
    if (!(fR_885088 == 0)) goto L_8850a2;
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0xe003000cu);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0x0u);
    LIFT_CALL(0x884390u, 0x88509cu);  // FUN_00884390
    esp = esp + 0x10u;
    eax = (eax & 0xffffff00u) | ((0u) & 0xffu);
    LIFT_RET(0);
L_8850a2:
    eax = (eax & 0xffffff00u) | ((0x1u) & 0xffu);
    LIFT_RET(0);
}

// 0088bac0 Fn_0088bac0
//   original FUN_0088bac0; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x0088bac0, Fn_0088bac0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_88bac5 = 0;
    uint32_t fA_88bac9 = 0;
    uint32_t fB_88bac9 = 0;
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    ecx = rd32(m, esi);
    fR_88bac5 = (ecx & ecx);
    if ((fR_88bac5 == 0)) goto L_88bae7;
    fA_88bac9 = rd8(m, esi + 0x8u);
    fB_88bac9 = 0x0u;
    if (fA_88bac9 == fB_88bac9) goto L_88badc;
    LIFT_CALL(0x88b8f0u, 0x88bad4u);  // FUN_0088b8f0
    wr32(m, esi, 0x0u);
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_88badc:
    LIFT_CALL(0x885310u, 0x88bae1u);  // FUN_00885310
    wr32(m, esi, 0x0u);
L_88bae7:
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0088bb50 Fn_0088bb50
//   original FUN_0088bb50; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x0088bb50, Fn_0088bb50, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_88bb64 = 0;
    uint32_t fB_88bb64 = 0;
    uint32_t fR_88bb70 = 0;
    uint32_t fR_88bb8a = 0;
    uint32_t fR_88bb91 = 0;
    uint32_t fA_88bb9f = 0;
    uint32_t fB_88bb9f = 0;
    uint32_t fR_88bbd9 = 0;
    uint32_t fA_88bbdd = 0;
    uint32_t fB_88bbdd = 0;
    uint32_t fA_88bbeb = 0;
    uint32_t fB_88bbeb = 0;
    uint32_t fR_88bbef = 0;
    uint32_t fA_88bbf9 = 0;
    uint32_t fB_88bbf9 = 0;
    uint32_t fA_88bc0f = 0;
    uint32_t fB_88bc0f = 0;
    esp = esp - 0x14u;
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    ebp = ecx;
    wr32(m, esp + 0x8u, 0x0u);
    LIFT_CALL(0x8a7310u, 0x88bb64u);  // FUN_008a7310
    fA_88bb64 = rd32(m, ebp + 0x4u);
    fB_88bb64 = 0x0u;
    esi = eax;
    if (fA_88bb64 == fB_88bb64) goto L_88bc2c;
    fR_88bb70 = (esi & esi);
    if ((fR_88bb70 == 0)) goto L_88bc2c;
    LIFT_CALL(0x7a0760u, 0x88bb7du);  // rtti_shared_vf_007a0760
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, 0x3u);
    ecx = 0x9df584u;
    LIFT_CALL(0x402420u, 0x88bb8au);  // FUN_00402420
    fR_88bb8a = (eax & eax);
    if ((fR_88bb8a == 0)) goto L_88bb95;
    eax = rd32(m, eax + 0xcu);
    fR_88bb91 = (eax & eax);
    if (!(fR_88bb91 == 0)) goto L_88bb99;
L_88bb95:
    eax = 0u;
    goto L_88bb9c;
L_88bb99:
    eax = rd32(m, eax + 0x8u);
L_88bb9c:
    eax = eax + 0x3cu;
    fA_88bb9f = rd8(m, 0x9df580u);
    fB_88bb9f = 0x0u;
    wr32(m, esp + 0x18u, eax);
    if (fA_88bb9f != fB_88bb9f) goto L_88bbb3;
    ecx = eax;
    LIFT_CALL(0x883d80u, 0x88bbb3u);  // FUN_00883d80
L_88bbb3:
    eax = rd32(m, ebp + 0x4u);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, edi);
    ecx = (esp + 0x10u);
    esp -= 4; wr32(m, esp, ecx);
    edx = (esp + 0x18u);
    esi = esi + 0x4u;
    esp -= 4; wr32(m, esp, edx);
    ecx = esi;
    wr32(m, esp + 0x18u, eax);
    LIFT_CALL(0x8864b0u, 0x88bbd0u);  // FUN_008864b0
    edi = rd32(m, esp + 0x14u);
    ebx = rd32(m, esi + 0x18u);
    esi = rd32(m, esi);
    fR_88bbd9 = (edi & edi);
    if ((fR_88bbd9 == 0)) goto L_88bbe1;
    fA_88bbdd = edi;
    fB_88bbdd = esi;
    if (fA_88bbdd == fB_88bbdd) goto L_88bbe7;
L_88bbe1:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x88bbe7u);
L_88bbe7:
    esi = rd32(m, esp + 0x18u);
    fA_88bbeb = esi;
    fB_88bbeb = ebx;
    if (fA_88bbeb == fB_88bbeb) goto L_88bc0f;
    fR_88bbef = (edi & edi);
    if (!(fR_88bbef == 0)) goto L_88bc28;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x88bbf9u);
L_88bbf9:
    fA_88bbf9 = esi;
    fB_88bbf9 = rd32(m, edi + 0x18u);
    if (fA_88bbf9 != fB_88bbf9) goto L_88bc04;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x88bc04u);
L_88bc04:
    eax = rd32(m, esi + 0x10u);
    esp -= 4; wr32(m, esp, eax);
    ecx = ebp;
    LIFT_CALL(0x88b960u, 0x88bc0fu);  // FUN_0088b960
L_88bc0f:
    fA_88bc0f = rd8(m, 0x9df580u);
    fB_88bc0f = 0x0u;
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    if (fA_88bc0f != fB_88bc0f) goto L_88bc2c;
    ecx = rd32(m, esp + 0x18u);
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    esp = esp + 0x14u;
    LIFT_TAILJMP(0x883d90u);  // FUN_00883d90
L_88bc28:
    edi = rd32(m, edi);
    goto L_88bbf9;
L_88bc2c:
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    esp = esp + 0x14u;
    LIFT_RET(0);
}

// 0088bcd0 Fn_0088bcd0
//   original FUN_0088bcd0; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x0088bcd0, Fn_0088bcd0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    eax = 0u;
    wr32(m, esi, eax);
    eax = rd32(m, esp + 0x8u);
    wr8(m, esi + 0x8u, 0x1u);
    wr32(m, esi + 0x4u, eax);
    LIFT_CALL(0x88bb50u, 0x88bce7u);  // FUN_0088bb50
    eax = esi;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 0088fa20 Entity__vf14
//   original Entity::vf14; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x0088fa20, Entity__vf14, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = (ecx + 0x30u);
    LIFT_RET(0);
}
