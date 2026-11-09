// Character / object animation ports (group "anim"), transliterated instruction by instruction from MXReflex.exe by the
// anim lifter (see native/game/notes/anim.md). Each function keeps the original's operations, order and memory traffic.
#include <stdexcept>
#include <utility>
#include "lift_rt.h"

// 007d1cd0 Scene_ProfileScope: profile scope marker ("Scene:%s" when a name is given)
//   original FUN_007d1cd0; 1320 instructions per frame (plan); register inputs: none; returns with ret 4
REFLEX_PORT_FN(0x007d1cd0, Scene_ProfileScope, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_7d1ceb = 0;
    esp = esp - 0x108u;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp + 0x104u, eax);
    eax = rd32(m, esp + 0x10cu);
    fR_7d1ceb = (eax & eax);
    if ((fR_7d1ceb == 0)) goto L_7d1d21;
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, 0x971fa8u);
    esi = 0x104u;
    edi = (esp + 0x10u);
    LIFT_CALL(0x78fd60u, 0x7d1d05u);  // FUN_0078fd60
    esp = esp + 0x8u;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp + 0x104u);
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x7d1d18u);  // __security_check_cookie
    esp = esp + 0x108u;
    LIFT_RET(4);
L_7d1d21:
    esp -= 4; wr32(m, esp, 0x103u);
    eax = (esp + 0x4u);
    esp -= 4; wr32(m, esp, 0x971fb4u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x916260u) /* 8.286626112932593e-39 */, 0x7d1d36u);
    ecx = rd32(m, esp + 0x110u);
    esp = esp + 0xcu;
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x7d1d47u);  // __security_check_cookie
    esp = esp + 0x108u;
    LIFT_RET(4);
}

// 007d2190 Fn_007d2190
//   original FUN_007d2190; 260 instructions per frame (plan); register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x007d2190, Fn_007d2190, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7d219f = 0;
    uint32_t fB_7d219f = 0;
    uint32_t fA_7d21be = 0;
    uint32_t fB_7d21be = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esi = 0u;
    esp -= 4; wr32(m, esp, edi);
    fA_7d219f = rd32(m, ebx + 0x19d0u);
    fB_7d219f = esi;
    if (fA_7d219f <= fB_7d219f) goto L_7d21c6;
    ebx = ebx + 0x19b8u;
L_7d21b0:
    edi = rd32(m, ebx);
    LIFT_CALL(0x794d10u, 0x7d21b7u);  // FUN_00794d10
    eax = rd32(m, ebp + 0x8u);
    esi = (esi + 1u);
    ebx = ebx + 0x4u;
    fA_7d21be = esi;
    fB_7d21be = rd32(m, eax + 0x19d0u);
    if (fA_7d21be < fB_7d21be) goto L_7d21b0;
L_7d21c6:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 007d21d0 Fn_007d21d0
//   original FUN_007d21d0; 390 instructions per frame (plan); register inputs: edi; returns with ret 0
REFLEX_PORT_FN(0x007d21d0, Fn_007d21d0, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7d21d3 = 0;
    uint32_t fB_7d21d3 = 0;
    uint32_t fR_7d2218 = 0;
    uint32_t fA_7d223f = 0;
    uint32_t fB_7d223f = 0;
    FCmp fC_7d2259{};
    FCmp fC_7d2265{};
    uint32_t fA_7d2287 = 0;
    uint32_t fB_7d2287 = 0;
    esp = esp - 0x8u;
    fA_7d21d3 = rd32(m, edi + 0x34u);
    fB_7d21d3 = 0x0u;
    if (fA_7d21d3 == fB_7d21d3) goto L_7d2298;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, 0x971fbcu);
    LIFT_CALL(0x7d1cd0u, 0x7d21e9u);  // FUN_007d1cd0
    esi = rd32(m, edi + 0x2c7cu);
    eax = rd32(m, 0xa95844u);
    esi = esi - rd32(m, edi + 0x2c78u);
    eax = rd32(m, eax + 0x10u);
    x0 = movss_m(rd32(m, eax + 0x40u));
    esi = uint32_t(int32_t(esi) >> 2);
    wr32(m, esp + 0x8u, x0.l(0));
    x0 = movss_m(rd32(m, eax + 0x48u));
    ebx = 0u;
    wr32(m, esp + 0xcu, x0.l(0));
    fR_7d2218 = (esi & esi);
    if (((fR_7d2218 == 0) || sgn(fR_7d2218, 4) != false)) goto L_7d228c;
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    goto L_7d2230;
L_7d2230:
    ecx = rd32(m, edi + 0x2c7cu);
    ecx = ecx - rd32(m, edi + 0x2c78u);
    ecx = uint32_t(int32_t(ecx) >> 2);
    fA_7d223f = ebx;
    fB_7d223f = ecx;
    if (fA_7d223f < fB_7d223f) goto L_7d2245;
    LIFT_CALL(ebp, 0x7d2245u);
L_7d2245:
    edx = rd32(m, edi + 0x2c78u);
    ecx = rd32(m, edx + ebx * 4);
    x0 = movss_m(rd32(m, esp + 0xcu));
    x0 = subss(x0, rd32(m, ecx + 0x24u));
    fC_7d2259 = fcmp32(x0.l(0), rd32(m, ecx + 0x20u));
    if (fc_b(fC_7d2259)) goto L_7d2286;
    x1 = movss_m(rd32(m, esp + 0x10u));
    fC_7d2265 = fcmp32(x0.l(0), x1.l(0));
    if ((fc_b(fC_7d2265) || fc_z(fC_7d2265))) goto L_7d226d;
    x0 = x1;
L_7d226d:
    eax = rd32(m, ecx);
    x1 = movss_m(rd32(m, edi + 0x2d14u));
    edx = rd32(m, eax + 0x54u);
    esp -= 4; wr32(m, esp, ecx);
    x1 = mulss(x1, x0.l(0));
    wr32(m, esp, x1.l(0));
    LIFT_CALL(edx, 0x7d2286u);
L_7d2286:
    ebx = (ebx + 1u);
    fA_7d2287 = ebx;
    fB_7d2287 = esi;
    if (int32_t(sext(fA_7d2287, 4)) < int32_t(sext(fB_7d2287, 4))) goto L_7d2230;
    ebp = rd32(m, esp); esp += 4;
L_7d228c:
    esp -= 4; wr32(m, esp, 0x971fbcu);
    LIFT_CALL(0x7d1cd0u, 0x7d2296u);  // FUN_007d1cd0
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
L_7d2298:
    esp = esp + 0x8u;
    LIFT_RET(0);
}

// 007d23a0 SceneManager_Tick: SceneManager tick ("SceneManager::Tick" profile scope): runs each scene ticker entity's vf19 with its elapsed time (clamped) times the scene's time scale
//   original FUN_007d23a0; 28119 instructions per frame (plan); register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x007d23a0, SceneManager_Tick, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7d23a6 = 0;
    uint32_t fB_7d23a6 = 0;
    uint32_t fR_7d240b = 0;
    uint32_t fA_7d2419 = 0;
    uint32_t fB_7d2419 = 0;
    FCmp fC_7d2434{};
    FCmp fC_7d2440{};
    uint32_t fA_7d2471 = 0;
    uint32_t fB_7d2471 = 0;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0xcu);
    fA_7d23a6 = rd32(m, ebp + 0x34u);
    fB_7d23a6 = 0x0u;
    if (fA_7d23a6 == fB_7d23a6) goto L_7d2489;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, 0x971fbcu);
    LIFT_CALL(0x7d1cd0u, 0x7d23bdu);  // FUN_007d1cd0
    eax = rd32(m, 0xa95844u);
    eax = rd32(m, eax + 0x10u);
    x0 = movss_m(rd32(m, eax + 0x40u));
    wr32(m, esp + 0x18u, x0.l(0));
    x0 = movss_m(rd32(m, eax + 0x48u));
    esi = (ebp + 0x4u);
    esp -= 4; wr32(m, esp, esi);
    wr32(m, esp + 0x14u, x0.l(0));
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x7d23e5u);
    ecx = (ebp + 0x2c3cu);
    edi = (ebp + 0x2c10u);
    esp -= 4; wr32(m, esp, ecx);
    eax = edi;
    LIFT_CALL(0x7f8fa0u, 0x7d23f9u);  // FUN_007f8fa0
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x7d2400u);
    esi = rd32(m, edi + 0x10u);
    esi = esi - rd32(m, edi + 0xcu);
    ebx = 0u;
    esi = uint32_t(int32_t(esi) >> 2);
    fR_7d240b = (esi & esi);
    if (((fR_7d240b == 0) || sgn(fR_7d240b, 4) != false)) goto L_7d2475;
L_7d2410:
    edx = rd32(m, edi + 0x10u);
    edx = edx - rd32(m, edi + 0xcu);
    edx = uint32_t(int32_t(edx) >> 2);
    fA_7d2419 = ebx;
    fB_7d2419 = edx;
    if (fA_7d2419 < fB_7d2419) goto L_7d2423;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d2423u);
L_7d2423:
    eax = rd32(m, edi + 0xcu);
    ecx = rd32(m, eax + ebx * 4);
    x0 = movss_m(rd32(m, esp + 0x18u));
    x0 = subss(x0, rd32(m, ecx + 0x24u));
    fC_7d2434 = fcmp32(x0.l(0), rd32(m, ecx + 0x20u));
    if (fc_b(fC_7d2434)) goto L_7d2470;
    x1 = movss_m(rd32(m, esp + 0x10u));
    fC_7d2440 = fcmp32(x0.l(0), x1.l(0));
    if ((fc_b(fC_7d2440) || fc_z(fC_7d2440))) goto L_7d2448;
    x0 = x1;
L_7d2448:
    edx = rd32(m, 0xa95844u);
    eax = rd32(m, edx + 0x10u);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x40u)));
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x4cu);
    wr32(m, ecx + 0x24u, fst32(m));
    fpop(m);
    x1 = movss_m(rd32(m, ebp + 0x2d14u));
    esp -= 4; wr32(m, esp, ecx);
    x1 = mulss(x1, x0.l(0));
    wr32(m, esp, x1.l(0));
    LIFT_CALL(eax, 0x7d2470u);
L_7d2470:
    ebx = (ebx + 1u);
    fA_7d2471 = ebx;
    fB_7d2471 = esi;
    if (int32_t(sext(fA_7d2471, 4)) < int32_t(sext(fB_7d2471, 4))) goto L_7d2410;
L_7d2475:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    esp = esp + 0x4u;
    wr32(m, esp + 0x4u, 0x971fbcu);
    LIFT_TAILJMP(0x7d1cd0u);  // FUN_007d1cd0
L_7d2489:
    ebp = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 007d2490 Fn_007d2490
//   original FUN_007d2490; 1854 instructions per frame (plan); register inputs: none; returns with ret 4
REFLEX_PORT_FN(0x007d2490, Fn_007d2490, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_7d24a8 = 0;
    uint32_t fA_7d24cf = 0;
    uint32_t fB_7d24cf = 0;
    uint32_t fA_7d24e6 = 0;
    uint32_t fB_7d24e6 = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, ebx + 0x2c64u);
    edi = edi - rd32(m, ebx + 0x2c60u);
    esi = 0u;
    edi = uint32_t(int32_t(edi) >> 2);
    fR_7d24a8 = (edi & edi);
    if (((fR_7d24a8 == 0) || sgn(fR_7d24a8, 4) != false)) goto L_7d24eb;
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, 0x916224u) /* 8.286396299984444e-39 */;
    goto L_7d24c0;
L_7d24c0:
    eax = rd32(m, ebx + 0x2c64u);
    eax = eax - rd32(m, ebx + 0x2c60u);
    eax = uint32_t(int32_t(eax) >> 2);
    fA_7d24cf = esi;
    fB_7d24cf = eax;
    if (fA_7d24cf < fB_7d24cf) goto L_7d24d5;
    LIFT_CALL(ebp, 0x7d24d5u);
L_7d24d5:
    ecx = rd32(m, ebx + 0x2c60u);
    ecx = rd32(m, ecx + esi * 4);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x50u);
    LIFT_CALL(eax, 0x7d24e5u);
    esi = (esi + 1u);
    fA_7d24e6 = esi;
    fB_7d24e6 = edi;
    if (int32_t(sext(fA_7d24e6, 4)) < int32_t(sext(fB_7d24e6, 4))) goto L_7d24c0;
    ebp = rd32(m, esp); esp += 4;
L_7d24eb:
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x7d2190u, 0x7d24f1u);  // FUN_007d2190
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 007d2920 Fn_007d2920
//   original FUN_007d2920; 148 instructions per frame (plan); register inputs: ecx eax; returns with ret 16
REFLEX_PORT_FN(0x007d2920, Fn_007d2920, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_7d2939 = 0;
    uint32_t fA_7d2950 = 0;
    uint32_t fB_7d2950 = 0;
    uint32_t fA_7d2958 = 0;
    uint32_t fB_7d2958 = 0;
    uint32_t fA_7d2960 = 0;
    uint32_t fB_7d2960 = 0;
    uint32_t fA_7d2973 = 0;
    uint32_t fB_7d2973 = 0;
    uint32_t fA_7d2988 = 0;
    uint32_t fB_7d2988 = 0;
    uint32_t fA_7d298c = 0;
    uint32_t fB_7d298c = 0;
    uint32_t fA_7d2999 = 0;
    uint32_t fB_7d2999 = 0;
    uint32_t fR_7d29b7 = 0;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    eax = (esi + 0x4u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0x14u, eax);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x7d2935u);
    eax = rd32(m, esp + 0x24u);
    fR_7d2939 = (eax & eax);
    if ((fR_7d2939 == 0)) goto L_7d2943;
    wr32(m, eax, 0x0u);
L_7d2943:
    ebx = 0u;
    ebp = 0u;
    esi = esi + 0x140u;
L_7d2950:
    fA_7d2950 = rd32(m, esi + 0x4u);
    fB_7d2950 = 0x0u;
    if (fA_7d2950 == fB_7d2950) goto L_7d2992;
    edi = 0u;
    fA_7d2958 = rd32(m, esi);
    fB_7d2958 = edi;
    if (int32_t(sext(fA_7d2958, 4)) <= int32_t(sext(fB_7d2958, 4))) goto L_7d298c;
L_7d2960:
    fA_7d2960 = ebx;
    fB_7d2960 = rd32(m, esp + 0x20u);
    if (int32_t(sext(fA_7d2960, 4)) >= int32_t(sext(fB_7d2960, 4))) goto L_7d29b3;
    eax = rd32(m, esi - 0x4u);
    ecx = rd32(m, eax + edi * 4);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x58u);
    LIFT_CALL(eax, 0x7d2973u);
    fA_7d2973 = eax;
    fB_7d2973 = rd32(m, esp + 0x18u);
    if (fA_7d2973 != fB_7d2973) goto L_7d2987;
    ecx = rd32(m, esi - 0x4u);
    edx = rd32(m, ecx + edi * 4);
    eax = rd32(m, esp + 0x1cu);
    wr32(m, eax + ebx * 4, edx);
    ebx = (ebx + 1u);
L_7d2987:
    edi = (edi + 1u);
    fA_7d2988 = edi;
    fB_7d2988 = rd32(m, esi);
    if (int32_t(sext(fA_7d2988, 4)) < int32_t(sext(fB_7d2988, 4))) goto L_7d2960;
L_7d298c:
    fA_7d298c = ebx;
    fB_7d298c = rd32(m, esp + 0x20u);
    if (int32_t(sext(fA_7d298c, 4)) >= int32_t(sext(fB_7d298c, 4))) goto L_7d29b3;
L_7d2992:
    ebp = (ebp + 1u);
    esi = esi + 0x110u;
    fA_7d2999 = ebp;
    fB_7d2999 = 0x18u;
    if (fA_7d2999 < fB_7d2999) goto L_7d2950;
    ecx = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x7d29a9u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = ebx;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(16);
L_7d29b3:
    eax = rd32(m, esp + 0x24u);
    fR_7d29b7 = (eax & eax);
    if ((fR_7d29b7 == 0)) goto L_7d29c1;
    wr32(m, eax, 0x1u);
L_7d29c1:
    ecx = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x7d29ccu);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = ebx;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(16);
}

// 007d2da0 Fn_007d2da0
//   original FUN_007d2da0; 60 instructions per frame (plan); register inputs: none; returns with ret 8
REFLEX_PORT_FN(0x007d2da0, Fn_007d2da0, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_7d2dca = 0;
    uint32_t fR_7d2dd5 = 0;
    uint32_t fA_7d2de2 = 0;
    uint32_t fB_7d2de2 = 0;
    uint32_t FL = 0;
    uint32_t fR_7d2de6 = 0;
    uint32_t fA_7d2ded = 0;
    uint32_t fB_7d2ded = 0;
    uint32_t fR_7d2df8 = 0;
    uint32_t fA_7d2e00 = 0;
    uint32_t fB_7d2e00 = 0;
    uint32_t fK_7d2e00 = 0;
    uint32_t fR_7d2e05 = 0;
    uint32_t fA_7d2e0d = 0;
    uint32_t fB_7d2e0d = 0;
    uint32_t fA_7d2e1f = 0;
    uint32_t fB_7d2e1f = 0;
    esp = esp - 0x8u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, esp + 0x18u);
    eax = (esi + 0x4u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0x18u, eax);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x7d2db9u);
    eax = (esi + 0x13cu);
    ebp = 0u;
    wr32(m, esp + 0x10u, eax);
L_7d2dc5:
    ebx = rd32(m, eax + 0x4u);
    esi = 0u;
    fR_7d2dca = (ebx & ebx);
    if (((fR_7d2dca == 0) || sgn(fR_7d2dca, 4) != false)) goto L_7d2e15;
    edi = rd32(m, eax);
L_7d2dd0:
    eax = rd32(m, edi);
    eax = rd32(m, eax + 0x18u);
    fR_7d2dd5 = (eax & eax);
    if ((fR_7d2dd5 == 0)) goto L_7d2e09;
    ecx = rd32(m, esp + 0x20u);
L_7d2de0:
    edx = (edx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    fA_7d2de2 = (edx & 0xffu);
    fB_7d2de2 = rd8(m, ecx);
    FL = fl_sub(fA_7d2de2, fB_7d2de2, 1);
    if (fA_7d2de2 != fB_7d2de2) goto L_7d2e00;
    fR_7d2de6 = ((edx & 0xffu) & (edx & 0xffu));
    if ((fR_7d2de6 == 0)) goto L_7d2dfc;
    edx = (edx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    fA_7d2ded = (edx & 0xffu);
    fB_7d2ded = rd8(m, ecx + 0x1u);
    FL = fl_sub(fA_7d2ded, fB_7d2ded, 1);
    if (fA_7d2ded != fB_7d2ded) goto L_7d2e00;
    eax = eax + 0x2u;
    ecx = ecx + 0x2u;
    fR_7d2df8 = ((edx & 0xffu) & (edx & 0xffu));
    if (!(fR_7d2df8 == 0)) goto L_7d2de0;
L_7d2dfc:
    eax = 0u;
    goto L_7d2e05;
L_7d2e00:
    fA_7d2e00 = eax; fB_7d2e00 = eax; fK_7d2e00 = (fl_cc(2, FL)) ? 1u : 0u;
    eax = fA_7d2e00 - fB_7d2e00 - fK_7d2e00;
    eax = eax - 0xffffffffu - ((uint64_t(fA_7d2e00) < uint64_t(fB_7d2e00) + uint64_t(fK_7d2e00)) ? 1u : 0u);
L_7d2e05:
    fR_7d2e05 = (eax & eax);
    if ((fR_7d2e05 == 0)) goto L_7d2e3b;
L_7d2e09:
    esi = (esi + 1u);
    edi = edi + 0x4u;
    fA_7d2e0d = esi;
    fB_7d2e0d = ebx;
    if (int32_t(sext(fA_7d2e0d, 4)) < int32_t(sext(fB_7d2e0d, 4))) goto L_7d2dd0;
    eax = rd32(m, esp + 0x10u);
L_7d2e15:
    ebp = (ebp + 1u);
    eax = eax + 0x110u;
    wr32(m, esp + 0x10u, eax);
    fA_7d2e1f = ebp;
    fB_7d2e1f = 0x18u;
    if (fA_7d2e1f < fB_7d2e1f) goto L_7d2dc5;
    ecx = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x7d2e2fu);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x8u;
    LIFT_RET(8);
L_7d2e3b:
    ecx = rd32(m, esp + 0x1cu);
    ebp = uint32_t(int32_t(ebp) * int32_t(0x110u));
    eax = rd32(m, esp + 0x14u);
    edx = rd32(m, ecx + ebp + 0x13cu);
    esi = rd32(m, edx + esi * 4);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x7d2e5au);
    edi = rd32(m, esp); esp += 4;
    eax = esi;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x8u;
    LIFT_RET(8);
}

// 007d2e70 Fn_007d2e70
//   original FUN_007d2e70; 2335 instructions per frame (plan); register inputs: none; returns with ret 12
REFLEX_PORT_FN(0x007d2e70, Fn_007d2e70, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7d2e94 = 0;
    uint32_t fB_7d2e94 = 0;
    uint32_t fA_7d2ea5 = 0;
    uint32_t fB_7d2ea5 = 0;
    uint32_t fR_7d2eb4 = 0;
    uint32_t fA_7d2ec2 = 0;
    uint32_t fB_7d2ec2 = 0;
    uint32_t FL = 0;
    uint32_t fR_7d2ec6 = 0;
    uint32_t fA_7d2ecd = 0;
    uint32_t fB_7d2ecd = 0;
    uint32_t fR_7d2ed8 = 0;
    uint32_t fA_7d2ee0 = 0;
    uint32_t fB_7d2ee0 = 0;
    uint32_t fK_7d2ee0 = 0;
    uint32_t fR_7d2ee5 = 0;
    uint32_t fA_7d2eea = 0;
    uint32_t fB_7d2eea = 0;
    uint32_t fA_7d2ef5 = 0;
    uint32_t fB_7d2ef5 = 0;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esp + 0x14u);
    eax = (edi + 0x4u);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0x20u, eax);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x7d2e8au);
    ebx = 0u;
    edi = edi + 0x140u;
L_7d2e92:
    esi = 0u;
    fA_7d2e94 = rd32(m, edi);
    fB_7d2e94 = esi;
    if (int32_t(sext(fA_7d2e94, 4)) <= int32_t(sext(fB_7d2e94, 4))) goto L_7d2eee;
L_7d2e98:
    eax = rd32(m, edi - 0x4u);
    ecx = rd32(m, eax + esi * 4);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x58u);
    LIFT_CALL(eax, 0x7d2ea5u);
    fA_7d2ea5 = eax;
    fB_7d2ea5 = rd32(m, esp + 0x18u);
    if (fA_7d2ea5 != fB_7d2ea5) goto L_7d2ee9;
    ecx = rd32(m, edi - 0x4u);
    edx = rd32(m, ecx + esi * 4);
    eax = rd32(m, edx + 0x18u);
    fR_7d2eb4 = (eax & eax);
    if ((fR_7d2eb4 == 0)) goto L_7d2ee9;
    ecx = ebp;
L_7d2ec0:
    edx = (edx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    fA_7d2ec2 = (edx & 0xffu);
    fB_7d2ec2 = rd8(m, ecx);
    FL = fl_sub(fA_7d2ec2, fB_7d2ec2, 1);
    if (fA_7d2ec2 != fB_7d2ec2) goto L_7d2ee0;
    fR_7d2ec6 = ((edx & 0xffu) & (edx & 0xffu));
    if ((fR_7d2ec6 == 0)) goto L_7d2edc;
    edx = (edx & 0xffffff00u) | ((rd8(m, eax + 0x1u)) & 0xffu);
    fA_7d2ecd = (edx & 0xffu);
    fB_7d2ecd = rd8(m, ecx + 0x1u);
    FL = fl_sub(fA_7d2ecd, fB_7d2ecd, 1);
    if (fA_7d2ecd != fB_7d2ecd) goto L_7d2ee0;
    eax = eax + 0x2u;
    ecx = ecx + 0x2u;
    fR_7d2ed8 = ((edx & 0xffu) & (edx & 0xffu));
    if (!(fR_7d2ed8 == 0)) goto L_7d2ec0;
L_7d2edc:
    eax = 0u;
    goto L_7d2ee5;
L_7d2ee0:
    fA_7d2ee0 = eax; fB_7d2ee0 = eax; fK_7d2ee0 = (fl_cc(2, FL)) ? 1u : 0u;
    eax = fA_7d2ee0 - fB_7d2ee0 - fK_7d2ee0;
    eax = eax - 0xffffffffu - ((uint64_t(fA_7d2ee0) < uint64_t(fB_7d2ee0) + uint64_t(fK_7d2ee0)) ? 1u : 0u);
L_7d2ee5:
    fR_7d2ee5 = (eax & eax);
    if ((fR_7d2ee5 == 0)) goto L_7d2f0e;
L_7d2ee9:
    esi = (esi + 1u);
    fA_7d2eea = esi;
    fB_7d2eea = rd32(m, edi);
    if (int32_t(sext(fA_7d2eea, 4)) < int32_t(sext(fB_7d2eea, 4))) goto L_7d2e98;
L_7d2eee:
    ebx = (ebx + 1u);
    edi = edi + 0x110u;
    fA_7d2ef5 = ebx;
    fB_7d2ef5 = 0x18u;
    if (fA_7d2ef5 < fB_7d2ef5) goto L_7d2e92;
    eax = rd32(m, esp + 0x1cu);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x7d2f05u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(12);
L_7d2f0e:
    eax = rd32(m, esp + 0x14u);
    ebx = uint32_t(int32_t(ebx) * int32_t(0x110u));
    edx = rd32(m, esp + 0x1cu);
    ecx = rd32(m, ebx + eax + 0x13cu);
    esi = rd32(m, ecx + esi * 4);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x7d2f2du);
    edi = rd32(m, esp); esp += 4;
    eax = esi;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(12);
}

// 007d2f70 Fn_007d2f70
//   original FUN_007d2f70; 0 instructions per frame (plan); register inputs: edi; returns with ret 4
REFLEX_PORT_FN(0x007d2f70, Fn_007d2f70, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7d2f85 = 0;
    uint32_t fB_7d2f85 = 0;
    uint32_t fR_7d2faa = 0;
    uint32_t fR_7d2fd8 = 0;
    uint32_t fA_7d2fe2 = 0;
    uint32_t fB_7d2fe2 = 0;
    uint32_t fA_7d3013 = 0;
    uint32_t fB_7d3013 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0x8u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x7d3580u, 0x7d2f85u);  // FUN_007d3580
    fA_7d2f85 = rd32(m, edi + 0x1cu);
    fB_7d2f85 = 0x0u;
    if (fA_7d2f85 == fB_7d2f85) goto L_7d2f93;
    esp -= 4; wr32(m, esp, edi);
    eax = ebx;
    LIFT_CALL(0x7d2500u, 0x7d2f93u);  // FUN_007d2500
L_7d2f93:
    esi = (ebx + 0x4u);
    esp -= 4; wr32(m, esp, esi);
    wr32(m, esp + 0x10u, esi);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x7d2fa1u);
    eax = rd32(m, edi);
    edx = rd32(m, eax + 0x64u);
    ecx = edi;
    LIFT_CALL(edx, 0x7d2faau);
    fR_7d2faa = (eax & eax);
    if ((fR_7d2faa == 0)) goto L_7d2fcf;
    esp -= 4; wr32(m, esp, 0x971fa0u);
    LIFT_CALL(0x7d1cd0u, 0x7d2fb8u);  // FUN_007d1cd0
    esp -= 4; wr32(m, esp, edi);
    eax = (ebx + 0x1a18u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x7dfdd0u, 0x7d2fc5u);  // FUN_007dfdd0
    esp -= 4; wr32(m, esp, 0x971fa0u);
    LIFT_CALL(0x7d1cd0u, 0x7d2fcfu);  // FUN_007d1cd0
L_7d2fcf:
    edx = rd32(m, edi);
    eax = rd32(m, edx + 0x2cu);
    ecx = edi;
    LIFT_CALL(eax, 0x7d2fd8u);
    fR_7d2fd8 = (eax & eax);
    if ((fR_7d2fd8 == 0)) goto L_7d300a;
    eax = rd32(m, ebx + 0x2c04u);
    fA_7d2fe2 = rd32(m, ebx + 0x2c00u);
    fB_7d2fe2 = eax;
    esi = (ebx + 0x2bfcu);
    if (int32_t(sext(fA_7d2fe2, 4)) < int32_t(sext(fB_7d2fe2, 4))) goto L_7d2ffb;
    ecx = rd32(m, esi + 0xcu);
    ecx = ecx + eax;
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x7cff20u, 0x7d2ffbu);  // FUN_007cff20
L_7d2ffb:
    edx = rd32(m, esi + 0x4u);
    eax = rd32(m, esi);
    wr32(m, eax + edx * 4, edi);
    wr32(m, esi + 0x4u, (rd32(m, esi + 0x4u) + 1u));
    esi = rd32(m, esp + 0xcu);
L_7d300a:
    edx = rd32(m, edi);
    eax = rd32(m, edx + 0x58u);
    ecx = edi;
    LIFT_CALL(eax, 0x7d3013u);
    fA_7d3013 = eax;
    fB_7d3013 = 0xeu;
    if (fA_7d3013 != fB_7d3013) goto L_7d301e;
    wr32(m, ebx + 0x2cc0u, edi);
L_7d301e:
    edx = rd32(m, edi);
    eax = rd32(m, edx + 0x4u);
    ecx = edi;
    LIFT_CALL(eax, 0x7d3027u);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x7d302eu);
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 007d3040 Fn_007d3040
//   original FUN_007d3040; 0 instructions per frame (plan); register inputs: esi; returns with ret 4
REFLEX_PORT_FN(0x007d3040, Fn_007d3040, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7d3079 = 0;
    uint32_t fB_7d3079 = 0;
    uint32_t fR_7d30be = 0;
    uint32_t fR_7d30eb = 0;
    uint32_t fR_7d30f7 = 0;
    uint32_t fA_7d3101 = 0;
    uint32_t fB_7d3101 = 0;
    uint32_t fA_7d3109 = 0;
    uint32_t fB_7d3109 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0x8u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, 0x91600cu) /* 8.29485173491818e-39 */;
    eax = (ebx + 0x4u);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0xcu, eax);
    LIFT_CALL(edi, 0x7d305eu);
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x8u);
    ecx = esi;
    LIFT_CALL(edx, 0x7d3067u);
    eax = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x7d3072u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x7d3850u, 0x7d3079u);  // FUN_007d3850
    fA_7d3079 = rd32(m, esi + 0x1cu);
    fB_7d3079 = 0x0u;
    if (fA_7d3079 == fB_7d3079) goto L_7d30ae;
    ecx = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(edi, 0x7d3086u);
    edx = rd32(m, esi + 0x18u);
    eax = (esp + 0xcu);
    esp -= 4; wr32(m, esp, eax);
    edi = (ebx + 0x19f8u);
    wr32(m, esp + 0x10u, edx);
    LIFT_CALL(0x84dd30u, 0x7d309du);  // FUN_0084dd30
    ecx = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x7d30a8u);
    edi = rd32(m, 0x91600cu) /* 8.29485173491818e-39 */;
L_7d30ae:
    edx = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(edi, 0x7d30b5u);
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x64u);
    ecx = esi;
    LIFT_CALL(edx, 0x7d30beu);
    fR_7d30be = (eax & eax);
    if ((fR_7d30be == 0)) goto L_7d30e2;
    esp -= 4; wr32(m, esp, 0x971fa0u);
    LIFT_CALL(0x7d1cd0u, 0x7d30ccu);  // FUN_007d1cd0
    esp -= 4; wr32(m, esp, esi);
    eax = (ebx + 0x1a18u);
    LIFT_CALL(0x7e0190u, 0x7d30d8u);  // FUN_007e0190
    esp -= 4; wr32(m, esp, 0x971fa0u);
    LIFT_CALL(0x7d1cd0u, 0x7d30e2u);  // FUN_007d1cd0
L_7d30e2:
    eax = rd32(m, esi);
    edx = rd32(m, eax + 0x2cu);
    ecx = esi;
    LIFT_CALL(edx, 0x7d30ebu);
    fR_7d30eb = (eax & eax);
    if ((fR_7d30eb == 0)) goto L_7d3129;
    ecx = rd32(m, ebx + 0x2c00u);
    eax = 0u;
    fR_7d30f7 = (ecx & ecx);
    if (((fR_7d30f7 == 0) || sgn(fR_7d30f7, 4) != false)) goto L_7d310d;
    edx = rd32(m, ebx + 0x2bfcu);
L_7d3101:
    fA_7d3101 = rd32(m, edx);
    fB_7d3101 = esi;
    if (fA_7d3101 == fB_7d3101) goto L_7d3110;
    eax = (eax + 1u);
    edx = edx + 0x4u;
    fA_7d3109 = eax;
    fB_7d3109 = ecx;
    if (int32_t(sext(fA_7d3109, 4)) < int32_t(sext(fB_7d3109, 4))) goto L_7d3101;
L_7d310d:
    eax = (eax | 0xffffffffu);
L_7d3110:
    edx = rd32(m, ebx + 0x2c00u);
    ecx = rd32(m, ebx + 0x2bfcu);
    edx = rd32(m, ecx + edx * 4 - 0x4u);
    wr32(m, ecx + eax * 4, edx);
    wr32(m, ebx + 0x2c00u, (rd32(m, ebx + 0x2c00u) - 1u));
L_7d3129:
    eax = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x7d3134u);
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 007d3580 Fn_007d3580
//   original FUN_007d3580; 0 instructions per frame (plan); register inputs: none; returns with ret 8
REFLEX_PORT_FN(0x007d3580, Fn_007d3580, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7d359f = 0;
    uint32_t fB_7d359f = 0;
    FCmp fC_7d35ae{};
    uint32_t fR_7d35b6 = 0;
    uint32_t fR_7d35e1 = 0;
    uint32_t fA_7d35f1 = 0;
    uint32_t fB_7d35f1 = 0;
    uint32_t fA_7d3602 = 0;
    uint32_t fB_7d3602 = 0;
    uint32_t fR_7d3618 = 0;
    FCmp fC_7d362f{};
    uint32_t fR_7d3644 = 0;
    uint32_t fA_7d3669 = 0;
    uint32_t fB_7d3669 = 0;
    uint32_t fA_7d367e = 0;
    uint32_t fB_7d367e = 0;
    uint32_t fA_7d3697 = 0;
    uint32_t fB_7d3697 = 0;
    uint32_t fR_7d36ad = 0;
    FCmp fC_7d36ca{};
    uint32_t fR_7d36df = 0;
    uint32_t fA_7d370c = 0;
    uint32_t fB_7d370c = 0;
    uint32_t fA_7d371e = 0;
    uint32_t fB_7d371e = 0;
    uint32_t fA_7d372f = 0;
    uint32_t fB_7d372f = 0;
    uint32_t fR_7d3745 = 0;
    FCmp fC_7d375c{};
    uint32_t fR_7d3771 = 0;
    uint32_t fA_7d3790 = 0;
    uint32_t fB_7d3790 = 0;
    uint32_t fA_7d37a6 = 0;
    uint32_t fB_7d37a6 = 0;
    uint32_t fA_7d37b7 = 0;
    uint32_t fB_7d37b7 = 0;
    uint32_t fR_7d37cd = 0;
    FCmp fC_7d37ea{};
    uint32_t fR_7d37ff = 0;
    esp = esp - 0x14u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x20u);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, 0x91600cu) /* 8.29485173491818e-39 */;
    eax = (ebp + 0x4u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0x18u, eax);
    LIFT_CALL(esi, 0x7d359bu);
    edi = rd32(m, esp + 0x2cu);
    fA_7d359f = rd32(m, edi + 0x70u);
    fB_7d359f = 0x0u;
    if (fA_7d359f == fB_7d359f) goto L_7d382b;
    x0 = movss_m(rd32(m, edi + 0x78u));
    fC_7d35ae = fcmp32(x0.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_7d35ae) | 2u) << 8);
    fR_7d35b6 = (((eax >> 8) & 0xffu) & 0x44u);
    if (parity8(fR_7d35b6)) goto L_7d35d8;
    x0 = movss_m(rd32(m, 0xa95838u));
    x0 = addss(x0, rd32(m, 0x931d94u) /* 0.10000000149011612 */);
    wr32(m, 0xa95838u, x0.l(0));
    wr32(m, edi + 0x78u, x0.l(0));
L_7d35d8:
    eax = rd32(m, edi);
    edx = rd32(m, eax + 0x48u);
    ecx = edi;
    LIFT_CALL(edx, 0x7d35e1u);
    fR_7d35e1 = (eax & eax);
    if (!(fR_7d35e1 == 0)) goto L_7d3660;
    edi = rd32(m, ebp + 0x2c48u);
    ebp = ebp + 0x2c3cu;
    fA_7d35f1 = edi;
    fB_7d35f1 = rd32(m, ebp + 0x10u);
    if (fA_7d35f1 <= fB_7d35f1) goto L_7d35fc;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d35fcu);
L_7d35fc:
    esi = rd32(m, ebp + 0x10u);
    ebx = rd32(m, ebp);
    fA_7d3602 = rd32(m, ebp + 0xcu);
    fB_7d3602 = esi;
    if (fA_7d3602 <= fB_7d3602) goto L_7d360d;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d360du);
L_7d360d:
    esi = esi - edi;
    esi = uint32_t(int32_t(esi) >> 2);
    ecx = esi;
    wr32(m, esp + 0x1cu, edi);
    fR_7d3618 = (ecx & ecx);
    if (((fR_7d3618 == 0) || sgn(fR_7d3618, 4) != false)) goto L_7d3648;
    eax = rd32(m, esp + 0x2cu);
    x0 = movss_m(rd32(m, eax + 0x78u));
L_7d3625:
    eax = ecx;
    edx = (eax >> 31) ? 0xffffffffu : 0;
    eax = eax - edx;
    eax = uint32_t(int32_t(eax) >> 1);
    edx = rd32(m, edi + eax * 4);
    fC_7d362f = fcmp32(x0.l(0), rd32(m, edx + 0x78u));
    if ((fc_b(fC_7d362f) || fc_z(fC_7d362f))) goto L_7d3642;
    edx = (edx | 0xffffffffu);
    edx = edx - eax;
    edi = (edi + eax * 4 + 0x4u);
    ecx = ecx + edx;
    goto L_7d3644;
L_7d3642:
    ecx = eax;
L_7d3644:
    fR_7d3644 = (ecx & ecx);
    if ((!(fR_7d3644 == 0) && sgn(fR_7d3644, 4) == false)) goto L_7d3625;
L_7d3648:
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, ebx);
    eax = (esp + 0x34u);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0x24u);
    esp -= 4; wr32(m, esp, ecx);
    eax = ebp;
    LIFT_CALL(0x7f90a0u, 0x7d365bu);  // FUN_007f90a0
    goto L_7d3704;
L_7d3660:
    edx = rd32(m, edi);
    eax = rd32(m, edx + 0x48u);
    ecx = edi;
    LIFT_CALL(eax, 0x7d3669u);
    fA_7d3669 = eax;
    fB_7d3669 = 0x1u;
    if (fA_7d3669 != fB_7d3669) goto L_7d370c;
    eax = (ebp + 0x1cu);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(esi, 0x7d3678u);
    edi = rd32(m, ebp + 0x2c90u);
    fA_7d367e = edi;
    fB_7d367e = rd32(m, ebp + 0x2c94u);
    ebx = (ebp + 0x2c84u);
    if (fA_7d367e <= fB_7d367e) goto L_7d3692;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3692u);
L_7d3692:
    esi = rd32(m, ebx + 0x10u);
    ebp = rd32(m, ebx);
    fA_7d3697 = rd32(m, ebx + 0xcu);
    fB_7d3697 = esi;
    if (fA_7d3697 <= fB_7d3697) goto L_7d36a2;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d36a2u);
L_7d36a2:
    esi = esi - edi;
    esi = uint32_t(int32_t(esi) >> 2);
    ecx = esi;
    wr32(m, esp + 0x1cu, edi);
    fR_7d36ad = (ecx & ecx);
    if (((fR_7d36ad == 0) || sgn(fR_7d36ad, 4) != false)) goto L_7d36e3;
    edx = rd32(m, esp + 0x2cu);
    x0 = movss_m(rd32(m, edx + 0x78u));
L_7d36c0:
    eax = ecx;
    edx = (eax >> 31) ? 0xffffffffu : 0;
    eax = eax - edx;
    eax = uint32_t(int32_t(eax) >> 1);
    edx = rd32(m, edi + eax * 4);
    fC_7d36ca = fcmp32(x0.l(0), rd32(m, edx + 0x78u));
    if ((fc_b(fC_7d36ca) || fc_z(fC_7d36ca))) goto L_7d36dd;
    edx = (edx | 0xffffffffu);
    edx = edx - eax;
    edi = (edi + eax * 4 + 0x4u);
    ecx = ecx + edx;
    goto L_7d36df;
L_7d36dd:
    ecx = eax;
L_7d36df:
    fR_7d36df = (ecx & ecx);
    if ((!(fR_7d36df == 0) && sgn(fR_7d36df, 4) == false)) goto L_7d36c0;
L_7d36e3:
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, ebp);
    eax = (esp + 0x34u);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0x24u);
    esp -= 4; wr32(m, esp, ecx);
    eax = ebx;
    LIFT_CALL(0x7f90a0u, 0x7d36f6u);  // FUN_007f90a0
    eax = rd32(m, esp + 0x28u);
    eax = eax + 0x1cu;
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x7d3704u);
L_7d3704:
    ebp = rd32(m, esp + 0x28u);
    edi = rd32(m, esp + 0x2cu);
L_7d370c:
    fA_7d370c = rd32(m, edi + 0x28u);
    fB_7d370c = 0x0u;
    if (fA_7d370c == fB_7d370c) goto L_7d3790;
    edi = rd32(m, ebp + 0x2c60u);
    ebp = ebp + 0x2c54u;
    fA_7d371e = edi;
    fB_7d371e = rd32(m, ebp + 0x10u);
    if (fA_7d371e <= fB_7d371e) goto L_7d3729;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3729u);
L_7d3729:
    esi = rd32(m, ebp + 0x10u);
    ebx = rd32(m, ebp);
    fA_7d372f = rd32(m, ebp + 0xcu);
    fB_7d372f = esi;
    if (fA_7d372f <= fB_7d372f) goto L_7d373a;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d373au);
L_7d373a:
    esi = esi - edi;
    esi = uint32_t(int32_t(esi) >> 2);
    ecx = esi;
    wr32(m, esp + 0x1cu, edi);
    fR_7d3745 = (ecx & ecx);
    if (((fR_7d3745 == 0) || sgn(fR_7d3745, 4) != false)) goto L_7d3775;
    edx = rd32(m, esp + 0x2cu);
    x0 = movss_m(rd32(m, edx + 0x78u));
L_7d3752:
    eax = ecx;
    edx = (eax >> 31) ? 0xffffffffu : 0;
    eax = eax - edx;
    eax = uint32_t(int32_t(eax) >> 1);
    edx = rd32(m, edi + eax * 4);
    fC_7d375c = fcmp32(x0.l(0), rd32(m, edx + 0x78u));
    if ((fc_b(fC_7d375c) || fc_z(fC_7d375c))) goto L_7d376f;
    edx = (edx | 0xffffffffu);
    edx = edx - eax;
    edi = (edi + eax * 4 + 0x4u);
    ecx = ecx + edx;
    goto L_7d3771;
L_7d376f:
    ecx = eax;
L_7d3771:
    fR_7d3771 = (ecx & ecx);
    if ((!(fR_7d3771 == 0) && sgn(fR_7d3771, 4) == false)) goto L_7d3752;
L_7d3775:
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, ebx);
    eax = (esp + 0x34u);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0x24u);
    esp -= 4; wr32(m, esp, ecx);
    eax = ebp;
    LIFT_CALL(0x7f90a0u, 0x7d3788u);  // FUN_007f90a0
    edi = rd32(m, esp + 0x2cu);
    ebp = rd32(m, esp + 0x28u);
L_7d3790:
    fA_7d3790 = rd32(m, edi + 0x2cu);
    fB_7d3790 = 0x0u;
    if (fA_7d3790 == fB_7d3790) goto L_7d383b;
    edi = rd32(m, ebp + 0x2c78u);
    ebp = ebp + 0x2c6cu;
    fA_7d37a6 = edi;
    fB_7d37a6 = rd32(m, ebp + 0x10u);
    if (fA_7d37a6 <= fB_7d37a6) goto L_7d37b1;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d37b1u);
L_7d37b1:
    esi = rd32(m, ebp + 0x10u);
    ebx = rd32(m, ebp);
    fA_7d37b7 = rd32(m, ebp + 0xcu);
    fB_7d37b7 = esi;
    if (fA_7d37b7 <= fB_7d37b7) goto L_7d37c2;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d37c2u);
L_7d37c2:
    esi = esi - edi;
    esi = uint32_t(int32_t(esi) >> 2);
    ecx = esi;
    wr32(m, esp + 0x1cu, edi);
    fR_7d37cd = (ecx & ecx);
    if (((fR_7d37cd == 0) || sgn(fR_7d37cd, 4) != false)) goto L_7d3803;
    edx = rd32(m, esp + 0x2cu);
    x0 = movss_m(rd32(m, edx + 0x78u));
L_7d37e0:
    eax = ecx;
    edx = (eax >> 31) ? 0xffffffffu : 0;
    eax = eax - edx;
    eax = uint32_t(int32_t(eax) >> 1);
    edx = rd32(m, edi + eax * 4);
    fC_7d37ea = fcmp32(x0.l(0), rd32(m, edx + 0x78u));
    if ((fc_b(fC_7d37ea) || fc_z(fC_7d37ea))) goto L_7d37fd;
    edx = (edx | 0xffffffffu);
    edx = edx - eax;
    edi = (edi + eax * 4 + 0x4u);
    ecx = ecx + edx;
    goto L_7d37ff;
L_7d37fd:
    ecx = eax;
L_7d37ff:
    fR_7d37ff = (ecx & ecx);
    if ((!(fR_7d37ff == 0) && sgn(fR_7d37ff, 4) == false)) goto L_7d37e0;
L_7d3803:
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, ebx);
    eax = (esp + 0x34u);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0x24u);
    esp -= 4; wr32(m, esp, ecx);
    eax = ebp;
    LIFT_CALL(0x7f90a0u, 0x7d3816u);  // FUN_007f90a0
    eax = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x7d3821u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x14u;
    LIFT_RET(8);
L_7d382b:
    edx = (esp + 0x2cu);
    esp -= 4; wr32(m, esp, edx);
    esi = (ebp + 0x2c28u);
    LIFT_CALL(0x544cc0u, 0x7d383bu);  // FUN_00544cc0
L_7d383b:
    eax = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x7d3846u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x14u;
    LIFT_RET(8);
}

// 007d3850 Fn_007d3850
//   original FUN_007d3850; 2 instructions per frame (plan); register inputs: none; returns with ret 8
REFLEX_PORT_FN(0x007d3850, Fn_007d3850, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7d387f = 0;
    uint32_t fB_7d387f = 0;
    uint32_t fR_7d3899 = 0;
    uint32_t fA_7d389d = 0;
    uint32_t fB_7d389d = 0;
    uint32_t fA_7d38ab = 0;
    uint32_t fB_7d38ab = 0;
    uint32_t fA_7d38ed = 0;
    uint32_t fB_7d38ed = 0;
    uint32_t fR_7d38fa = 0;
    uint32_t fA_7d38fe = 0;
    uint32_t fB_7d38fe = 0;
    uint32_t fA_7d390c = 0;
    uint32_t fB_7d390c = 0;
    uint32_t fA_7d3949 = 0;
    uint32_t fB_7d3949 = 0;
    uint32_t fR_7d3956 = 0;
    uint32_t fA_7d395a = 0;
    uint32_t fB_7d395a = 0;
    uint32_t fA_7d3968 = 0;
    uint32_t fB_7d3968 = 0;
    uint32_t fA_7d399c = 0;
    uint32_t fB_7d399c = 0;
    uint32_t fR_7d39a9 = 0;
    uint32_t fA_7d39ad = 0;
    uint32_t fB_7d39ad = 0;
    uint32_t fA_7d39bb = 0;
    uint32_t fB_7d39bb = 0;
    uint32_t fR_7d39d2 = 0;
    uint32_t fA_7d39e4 = 0;
    uint32_t fB_7d39e4 = 0;
    uint32_t fA_7d39ec = 0;
    uint32_t fB_7d39ec = 0;
    uint32_t fA_7d3a05 = 0;
    uint32_t fB_7d3a05 = 0;
    esp = esp - 0x1cu;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x24u);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    eax = (ebx + 0x4u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0x18u, eax);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x7d3869u);
    eax = rd32(m, esp + 0x34u);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0x1cu);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x7d3a40u, 0x7d3879u);  // FUN_007d3a40
    edi = rd32(m, ebx + 0x2c4cu);
    fA_7d387f = rd32(m, ebx + 0x2c48u);
    fB_7d387f = edi;
    esi = (ebx + 0x2c3cu);
    if (fA_7d387f <= fB_7d387f) goto L_7d3893;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3893u);
L_7d3893:
    ebp = rd32(m, esp + 0x18u);
    eax = rd32(m, esi);
    fR_7d3899 = (ebp & ebp);
    if ((fR_7d3899 == 0)) goto L_7d38a1;
    fA_7d389d = ebp;
    fB_7d389d = eax;
    if (fA_7d389d == fB_7d389d) goto L_7d38a7;
L_7d38a1:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d38a7u);
L_7d38a7:
    eax = rd32(m, esp + 0x1cu);
    fA_7d38ab = eax;
    fB_7d38ab = edi;
    if (fA_7d38ab == fB_7d38ab) goto L_7d38ba;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ebp);
    edi = (esp + 0x28u);
    LIFT_CALL(0x7f9140u, 0x7d38bau);  // FUN_007f9140
L_7d38ba:
    ebp = (ebx + 0x1cu);
    esp -= 4; wr32(m, esp, ebp);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x7d38c4u);
    edx = rd32(m, esp + 0x34u);
    esp -= 4; wr32(m, esp, edx);
    eax = (esp + 0x24u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x7d3b50u, 0x7d38d4u);  // FUN_007d3b50
    ecx = rd32(m, eax + 0x4u);
    edi = rd32(m, eax);
    eax = rd32(m, ebx + 0x2c94u);
    esi = (ebx + 0x2c84u);
    wr32(m, esp + 0x1cu, ecx);
    wr32(m, esp + 0x30u, eax);
    fA_7d38ed = rd32(m, esi + 0xcu);
    fB_7d38ed = eax;
    if (fA_7d38ed <= fB_7d38ed) goto L_7d38f8;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d38f8u);
L_7d38f8:
    eax = rd32(m, esi);
    fR_7d38fa = (edi & edi);
    if ((fR_7d38fa == 0)) goto L_7d3902;
    fA_7d38fe = edi;
    fB_7d38fe = eax;
    if (fA_7d38fe == fB_7d38fe) goto L_7d3908;
L_7d3902:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3908u);
L_7d3908:
    eax = rd32(m, esp + 0x1cu);
    fA_7d390c = eax;
    fB_7d390c = rd32(m, esp + 0x30u);
    if (fA_7d390c == fB_7d390c) goto L_7d391d;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, edi);
    edi = (esp + 0x28u);
    LIFT_CALL(0x7f9140u, 0x7d391du);  // FUN_007f9140
L_7d391d:
    esp -= 4; wr32(m, esp, ebp);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x7d3924u);
    edx = rd32(m, esp + 0x34u);
    esp -= 4; wr32(m, esp, edx);
    eax = (esp + 0x24u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x7d3c40u, 0x7d3934u);  // FUN_007d3c40
    edi = rd32(m, ebx + 0x2c64u);
    ecx = rd32(m, eax + 0x4u);
    ebp = rd32(m, eax);
    esi = (ebx + 0x2c54u);
    wr32(m, esp + 0x1cu, ecx);
    fA_7d3949 = rd32(m, esi + 0xcu);
    fB_7d3949 = edi;
    if (fA_7d3949 <= fB_7d3949) goto L_7d3954;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3954u);
L_7d3954:
    eax = rd32(m, esi);
    fR_7d3956 = (ebp & ebp);
    if ((fR_7d3956 == 0)) goto L_7d395e;
    fA_7d395a = ebp;
    fB_7d395a = eax;
    if (fA_7d395a == fB_7d395a) goto L_7d3964;
L_7d395e:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3964u);
L_7d3964:
    eax = rd32(m, esp + 0x1cu);
    fA_7d3968 = eax;
    fB_7d3968 = edi;
    if (fA_7d3968 == fB_7d3968) goto L_7d3977;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ebp);
    edi = (esp + 0x28u);
    LIFT_CALL(0x7f9140u, 0x7d3977u);  // FUN_007f9140
L_7d3977:
    edx = rd32(m, esp + 0x34u);
    esp -= 4; wr32(m, esp, edx);
    eax = (esp + 0x24u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x7d3d30u, 0x7d3987u);  // FUN_007d3d30
    ebp = rd32(m, ebx + 0x2c7cu);
    ecx = rd32(m, eax + 0x4u);
    edi = rd32(m, eax);
    esi = (ebx + 0x2c6cu);
    wr32(m, esp + 0x1cu, ecx);
    fA_7d399c = rd32(m, esi + 0xcu);
    fB_7d399c = ebp;
    if (fA_7d399c <= fB_7d399c) goto L_7d39a7;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d39a7u);
L_7d39a7:
    eax = rd32(m, esi);
    fR_7d39a9 = (edi & edi);
    if ((fR_7d39a9 == 0)) goto L_7d39b1;
    fA_7d39ad = edi;
    fB_7d39ad = eax;
    if (fA_7d39ad == fB_7d39ad) goto L_7d39b7;
L_7d39b1:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d39b7u);
L_7d39b7:
    eax = rd32(m, esp + 0x1cu);
    fA_7d39bb = eax;
    fB_7d39bb = ebp;
    if (fA_7d39bb == fB_7d39bb) goto L_7d39ca;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, edi);
    edi = (esp + 0x28u);
    LIFT_CALL(0x7f9140u, 0x7d39cau);  // FUN_007f9140
L_7d39ca:
    edx = rd32(m, ebx + 0x2c2cu);
    eax = 0u;
    fR_7d39d2 = (edx & edx);
    if (((fR_7d39d2 == 0) || sgn(fR_7d39d2, 4) != false)) goto L_7d3a1d;
    ecx = rd32(m, ebx + 0x2c28u);
L_7d39e0:
    esi = rd32(m, esp + 0x34u);
    fA_7d39e4 = rd32(m, ecx);
    fB_7d39e4 = esi;
    if (fA_7d39e4 == fB_7d39e4) goto L_7d3a05;
    eax = (eax + 1u);
    ecx = ecx + 0x4u;
    fA_7d39ec = eax;
    fB_7d39ec = edx;
    if (int32_t(sext(fA_7d39ec, 4)) < int32_t(sext(fB_7d39ec, 4))) goto L_7d39e0;
    eax = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x7d39fbu);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x1cu;
    LIFT_RET(8);
L_7d3a05:
    fA_7d3a05 = eax;
    fB_7d3a05 = 0xffffffffu;
    if (fA_7d3a05 == fB_7d3a05) goto L_7d3a1d;
    ecx = rd32(m, ebx + 0x2c28u);
    edx = rd32(m, ecx + edx * 4 - 0x4u);
    wr32(m, ecx + eax * 4, edx);
    wr32(m, ebx + 0x2c2cu, (rd32(m, ebx + 0x2c2cu) - 1u));
L_7d3a1d:
    eax = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x7d3a28u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x1cu;
    LIFT_RET(8);
}

// 007d3a40 Fn_007d3a40
//   original FUN_007d3a40; 5 instructions per frame (plan); register inputs: none; returns with ret 12
REFLEX_PORT_FN(0x007d3a40, Fn_007d3a40, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7d3a63 = 0;
    uint32_t fB_7d3a63 = 0;
    uint32_t fA_7d3a81 = 0;
    uint32_t fB_7d3a81 = 0;
    uint32_t fR_7d3aa0 = 0;
    uint32_t fA_7d3aa4 = 0;
    uint32_t fB_7d3aa4 = 0;
    uint32_t fA_7d3ab0 = 0;
    uint32_t fB_7d3ab0 = 0;
    uint32_t fR_7d3ab6 = 0;
    uint32_t fA_7d3ac2 = 0;
    uint32_t fB_7d3ac2 = 0;
    uint32_t fA_7d3ad1 = 0;
    uint32_t fB_7d3ad1 = 0;
    uint32_t fR_7d3ad5 = 0;
    uint32_t fA_7d3ae1 = 0;
    uint32_t fB_7d3ae1 = 0;
    uint32_t fA_7d3b1c = 0;
    uint32_t fB_7d3b1c = 0;
    esp = esp - 0x8u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x18u);
    esp -= 4; wr32(m, esp, esi);
    eax = (ebx + 0x4u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0x24u, eax);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x7d3a5du);
    esi = rd32(m, ebx + 0x2c48u);
    fA_7d3a63 = esi;
    fB_7d3a63 = rd32(m, ebx + 0x2c4cu);
    if (fA_7d3a63 <= fB_7d3a63) goto L_7d3a71;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3a71u);
L_7d3a71:
    eax = rd32(m, ebx + 0x2c4cu);
    edi = rd32(m, ebx + 0x2c3cu);
    wr32(m, esp + 0x1cu, eax);
    fA_7d3a81 = rd32(m, ebx + 0x2c48u);
    fB_7d3a81 = eax;
    if (fA_7d3a81 <= fB_7d3a81) goto L_7d3a8f;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3a8fu);
L_7d3a8f:
    eax = rd32(m, ebx + 0x2c3cu);
    wr32(m, esp + 0x10u, eax);
L_7d3aa0:
    fR_7d3aa0 = (edi & edi);
    if ((fR_7d3aa0 == 0)) goto L_7d3aaa;
    fA_7d3aa4 = edi;
    fB_7d3aa4 = rd32(m, esp + 0x10u);
    if (fA_7d3aa4 == fB_7d3aa4) goto L_7d3ab0;
L_7d3aaa:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3ab0u);
L_7d3ab0:
    fA_7d3ab0 = esi;
    fB_7d3ab0 = rd32(m, esp + 0x1cu);
    if (fA_7d3ab0 == fB_7d3ab0) goto L_7d3b16;
    fR_7d3ab6 = (edi & edi);
    if (!(fR_7d3ab6 == 0)) goto L_7d3af1;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3ac0u);
    eax = 0u;
L_7d3ac2:
    fA_7d3ac2 = esi;
    fB_7d3ac2 = rd32(m, eax + 0x10u);
    if (fA_7d3ac2 < fB_7d3ac2) goto L_7d3acd;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3acdu);
L_7d3acd:
    ecx = rd32(m, esp + 0x24u);
    fA_7d3ad1 = rd32(m, esi);
    fB_7d3ad1 = ecx;
    if (fA_7d3ad1 == fB_7d3ad1) goto L_7d3af9;
    fR_7d3ad5 = (edi & edi);
    if (!(fR_7d3ad5 == 0)) goto L_7d3af5;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3adfu);
    eax = 0u;
L_7d3ae1:
    fA_7d3ae1 = esi;
    fB_7d3ae1 = rd32(m, eax + 0x10u);
    if (fA_7d3ae1 < fB_7d3ae1) goto L_7d3aec;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3aecu);
L_7d3aec:
    esi = esi + 0x4u;
    goto L_7d3aa0;
L_7d3af1:
    eax = rd32(m, edi);
    goto L_7d3ac2;
L_7d3af5:
    eax = rd32(m, edi);
    goto L_7d3ae1;
L_7d3af9:
    edx = rd32(m, esp + 0x20u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x7d3b04u);
    wr32(m, ebp, edi);
    eax = ebp;
    wr32(m, ebp + 0x4u, esi);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x8u;
    LIFT_RET(12);
L_7d3b16:
    esi = rd32(m, ebx + 0x2c4cu);
    fA_7d3b1c = rd32(m, ebx + 0x2c48u);
    fB_7d3b1c = esi;
    if (fA_7d3b1c <= fB_7d3b1c) goto L_7d3b2a;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3b2au);
L_7d3b2a:
    eax = rd32(m, esp + 0x20u);
    ebx = rd32(m, ebx + 0x2c3cu);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x7d3b3bu);
    edi = rd32(m, esp); esp += 4;
    wr32(m, ebp + 0x4u, esi);
    esi = rd32(m, esp); esp += 4;
    wr32(m, ebp, ebx);
    eax = ebp;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x8u;
    LIFT_RET(12);
}

// 007d3b50 Fn_007d3b50
//   original FUN_007d3b50; 1 instructions per frame (plan); register inputs: none; returns with ret 12
REFLEX_PORT_FN(0x007d3b50, Fn_007d3b50, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7d3b65 = 0;
    uint32_t fB_7d3b65 = 0;
    uint32_t fA_7d3b83 = 0;
    uint32_t fB_7d3b83 = 0;
    uint32_t fR_7d3ba0 = 0;
    uint32_t fA_7d3ba4 = 0;
    uint32_t fB_7d3ba4 = 0;
    uint32_t fA_7d3bb0 = 0;
    uint32_t fB_7d3bb0 = 0;
    uint32_t fR_7d3bb6 = 0;
    uint32_t fA_7d3bc2 = 0;
    uint32_t fB_7d3bc2 = 0;
    uint32_t fA_7d3bd1 = 0;
    uint32_t fB_7d3bd1 = 0;
    uint32_t fR_7d3bd5 = 0;
    uint32_t fA_7d3be1 = 0;
    uint32_t fB_7d3be1 = 0;
    uint32_t fA_7d3c11 = 0;
    uint32_t fB_7d3c11 = 0;
    esp = esp - 0x8u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x18u);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, ebx + 0x2c90u);
    esp -= 4; wr32(m, esp, edi);
    fA_7d3b65 = esi;
    fB_7d3b65 = rd32(m, ebx + 0x2c94u);
    if (fA_7d3b65 <= fB_7d3b65) goto L_7d3b73;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3b73u);
L_7d3b73:
    eax = rd32(m, ebx + 0x2c94u);
    edi = rd32(m, ebx + 0x2c84u);
    wr32(m, esp + 0x20u, eax);
    fA_7d3b83 = rd32(m, ebx + 0x2c90u);
    fB_7d3b83 = eax;
    if (fA_7d3b83 <= fB_7d3b83) goto L_7d3b91;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3b91u);
L_7d3b91:
    eax = rd32(m, ebx + 0x2c84u);
    wr32(m, esp + 0x10u, eax);
    goto L_7d3ba0;
L_7d3ba0:
    fR_7d3ba0 = (edi & edi);
    if ((fR_7d3ba0 == 0)) goto L_7d3baa;
    fA_7d3ba4 = edi;
    fB_7d3ba4 = rd32(m, esp + 0x10u);
    if (fA_7d3ba4 == fB_7d3ba4) goto L_7d3bb0;
L_7d3baa:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3bb0u);
L_7d3bb0:
    fA_7d3bb0 = esi;
    fB_7d3bb0 = rd32(m, esp + 0x20u);
    if (fA_7d3bb0 == fB_7d3bb0) goto L_7d3c0b;
    fR_7d3bb6 = (edi & edi);
    if (!(fR_7d3bb6 == 0)) goto L_7d3bf1;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3bc0u);
    eax = 0u;
L_7d3bc2:
    fA_7d3bc2 = esi;
    fB_7d3bc2 = rd32(m, eax + 0x10u);
    if (fA_7d3bc2 < fB_7d3bc2) goto L_7d3bcd;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3bcdu);
L_7d3bcd:
    ecx = rd32(m, esp + 0x24u);
    fA_7d3bd1 = rd32(m, esi);
    fB_7d3bd1 = ecx;
    if (fA_7d3bd1 == fB_7d3bd1) goto L_7d3bf9;
    fR_7d3bd5 = (edi & edi);
    if (!(fR_7d3bd5 == 0)) goto L_7d3bf5;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3bdfu);
    eax = 0u;
L_7d3be1:
    fA_7d3be1 = esi;
    fB_7d3be1 = rd32(m, eax + 0x10u);
    if (fA_7d3be1 < fB_7d3be1) goto L_7d3bec;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3becu);
L_7d3bec:
    esi = esi + 0x4u;
    goto L_7d3ba0;
L_7d3bf1:
    eax = rd32(m, edi);
    goto L_7d3bc2;
L_7d3bf5:
    eax = rd32(m, edi);
    goto L_7d3be1;
L_7d3bf9:
    wr32(m, ebp, edi);
    eax = ebp;
    wr32(m, ebp + 0x4u, esi);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x8u;
    LIFT_RET(12);
L_7d3c0b:
    esi = rd32(m, ebx + 0x2c94u);
    fA_7d3c11 = rd32(m, ebx + 0x2c90u);
    fB_7d3c11 = esi;
    if (fA_7d3c11 <= fB_7d3c11) goto L_7d3c1f;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3c1fu);
L_7d3c1f:
    eax = rd32(m, ebx + 0x2c84u);
    edi = rd32(m, esp); esp += 4;
    wr32(m, ebp + 0x4u, esi);
    wr32(m, ebp, eax);
    esi = rd32(m, esp); esp += 4;
    eax = ebp;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x8u;
    LIFT_RET(12);
}

// 007d3c40 Fn_007d3c40
//   original FUN_007d3c40; 0 instructions per frame (plan); register inputs: none; returns with ret 12
REFLEX_PORT_FN(0x007d3c40, Fn_007d3c40, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7d3c55 = 0;
    uint32_t fB_7d3c55 = 0;
    uint32_t fA_7d3c73 = 0;
    uint32_t fB_7d3c73 = 0;
    uint32_t fR_7d3c90 = 0;
    uint32_t fA_7d3c94 = 0;
    uint32_t fB_7d3c94 = 0;
    uint32_t fA_7d3ca0 = 0;
    uint32_t fB_7d3ca0 = 0;
    uint32_t fR_7d3ca6 = 0;
    uint32_t fA_7d3cb2 = 0;
    uint32_t fB_7d3cb2 = 0;
    uint32_t fA_7d3cc1 = 0;
    uint32_t fB_7d3cc1 = 0;
    uint32_t fR_7d3cc5 = 0;
    uint32_t fA_7d3cd1 = 0;
    uint32_t fB_7d3cd1 = 0;
    uint32_t fA_7d3d01 = 0;
    uint32_t fB_7d3d01 = 0;
    esp = esp - 0x8u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x18u);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, ebx + 0x2c60u);
    esp -= 4; wr32(m, esp, edi);
    fA_7d3c55 = esi;
    fB_7d3c55 = rd32(m, ebx + 0x2c64u);
    if (fA_7d3c55 <= fB_7d3c55) goto L_7d3c63;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3c63u);
L_7d3c63:
    eax = rd32(m, ebx + 0x2c64u);
    edi = rd32(m, ebx + 0x2c54u);
    wr32(m, esp + 0x20u, eax);
    fA_7d3c73 = rd32(m, ebx + 0x2c60u);
    fB_7d3c73 = eax;
    if (fA_7d3c73 <= fB_7d3c73) goto L_7d3c81;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3c81u);
L_7d3c81:
    eax = rd32(m, ebx + 0x2c54u);
    wr32(m, esp + 0x10u, eax);
    goto L_7d3c90;
L_7d3c90:
    fR_7d3c90 = (edi & edi);
    if ((fR_7d3c90 == 0)) goto L_7d3c9a;
    fA_7d3c94 = edi;
    fB_7d3c94 = rd32(m, esp + 0x10u);
    if (fA_7d3c94 == fB_7d3c94) goto L_7d3ca0;
L_7d3c9a:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3ca0u);
L_7d3ca0:
    fA_7d3ca0 = esi;
    fB_7d3ca0 = rd32(m, esp + 0x20u);
    if (fA_7d3ca0 == fB_7d3ca0) goto L_7d3cfb;
    fR_7d3ca6 = (edi & edi);
    if (!(fR_7d3ca6 == 0)) goto L_7d3ce1;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3cb0u);
    eax = 0u;
L_7d3cb2:
    fA_7d3cb2 = esi;
    fB_7d3cb2 = rd32(m, eax + 0x10u);
    if (fA_7d3cb2 < fB_7d3cb2) goto L_7d3cbd;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3cbdu);
L_7d3cbd:
    ecx = rd32(m, esp + 0x24u);
    fA_7d3cc1 = rd32(m, esi);
    fB_7d3cc1 = ecx;
    if (fA_7d3cc1 == fB_7d3cc1) goto L_7d3ce9;
    fR_7d3cc5 = (edi & edi);
    if (!(fR_7d3cc5 == 0)) goto L_7d3ce5;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3ccfu);
    eax = 0u;
L_7d3cd1:
    fA_7d3cd1 = esi;
    fB_7d3cd1 = rd32(m, eax + 0x10u);
    if (fA_7d3cd1 < fB_7d3cd1) goto L_7d3cdc;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3cdcu);
L_7d3cdc:
    esi = esi + 0x4u;
    goto L_7d3c90;
L_7d3ce1:
    eax = rd32(m, edi);
    goto L_7d3cb2;
L_7d3ce5:
    eax = rd32(m, edi);
    goto L_7d3cd1;
L_7d3ce9:
    wr32(m, ebp, edi);
    eax = ebp;
    wr32(m, ebp + 0x4u, esi);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x8u;
    LIFT_RET(12);
L_7d3cfb:
    esi = rd32(m, ebx + 0x2c64u);
    fA_7d3d01 = rd32(m, ebx + 0x2c60u);
    fB_7d3d01 = esi;
    if (fA_7d3d01 <= fB_7d3d01) goto L_7d3d0f;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3d0fu);
L_7d3d0f:
    eax = rd32(m, ebx + 0x2c54u);
    edi = rd32(m, esp); esp += 4;
    wr32(m, ebp + 0x4u, esi);
    wr32(m, ebp, eax);
    esi = rd32(m, esp); esp += 4;
    eax = ebp;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x8u;
    LIFT_RET(12);
}

// 007d3d30 Fn_007d3d30
//   original FUN_007d3d30; 0 instructions per frame (plan); register inputs: none; returns with ret 12
REFLEX_PORT_FN(0x007d3d30, Fn_007d3d30, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7d3d45 = 0;
    uint32_t fB_7d3d45 = 0;
    uint32_t fA_7d3d63 = 0;
    uint32_t fB_7d3d63 = 0;
    uint32_t fR_7d3d80 = 0;
    uint32_t fA_7d3d84 = 0;
    uint32_t fB_7d3d84 = 0;
    uint32_t fA_7d3d90 = 0;
    uint32_t fB_7d3d90 = 0;
    uint32_t fR_7d3d96 = 0;
    uint32_t fA_7d3da2 = 0;
    uint32_t fB_7d3da2 = 0;
    uint32_t fA_7d3db1 = 0;
    uint32_t fB_7d3db1 = 0;
    uint32_t fR_7d3db5 = 0;
    uint32_t fA_7d3dc1 = 0;
    uint32_t fB_7d3dc1 = 0;
    uint32_t fA_7d3df1 = 0;
    uint32_t fB_7d3df1 = 0;
    esp = esp - 0x8u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x18u);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, ebx + 0x2c78u);
    esp -= 4; wr32(m, esp, edi);
    fA_7d3d45 = esi;
    fB_7d3d45 = rd32(m, ebx + 0x2c7cu);
    if (fA_7d3d45 <= fB_7d3d45) goto L_7d3d53;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3d53u);
L_7d3d53:
    eax = rd32(m, ebx + 0x2c7cu);
    edi = rd32(m, ebx + 0x2c6cu);
    wr32(m, esp + 0x20u, eax);
    fA_7d3d63 = rd32(m, ebx + 0x2c78u);
    fB_7d3d63 = eax;
    if (fA_7d3d63 <= fB_7d3d63) goto L_7d3d71;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3d71u);
L_7d3d71:
    eax = rd32(m, ebx + 0x2c6cu);
    wr32(m, esp + 0x10u, eax);
    goto L_7d3d80;
L_7d3d80:
    fR_7d3d80 = (edi & edi);
    if ((fR_7d3d80 == 0)) goto L_7d3d8a;
    fA_7d3d84 = edi;
    fB_7d3d84 = rd32(m, esp + 0x10u);
    if (fA_7d3d84 == fB_7d3d84) goto L_7d3d90;
L_7d3d8a:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3d90u);
L_7d3d90:
    fA_7d3d90 = esi;
    fB_7d3d90 = rd32(m, esp + 0x20u);
    if (fA_7d3d90 == fB_7d3d90) goto L_7d3deb;
    fR_7d3d96 = (edi & edi);
    if (!(fR_7d3d96 == 0)) goto L_7d3dd1;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3da0u);
    eax = 0u;
L_7d3da2:
    fA_7d3da2 = esi;
    fB_7d3da2 = rd32(m, eax + 0x10u);
    if (fA_7d3da2 < fB_7d3da2) goto L_7d3dad;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3dadu);
L_7d3dad:
    ecx = rd32(m, esp + 0x24u);
    fA_7d3db1 = rd32(m, esi);
    fB_7d3db1 = ecx;
    if (fA_7d3db1 == fB_7d3db1) goto L_7d3dd9;
    fR_7d3db5 = (edi & edi);
    if (!(fR_7d3db5 == 0)) goto L_7d3dd5;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3dbfu);
    eax = 0u;
L_7d3dc1:
    fA_7d3dc1 = esi;
    fB_7d3dc1 = rd32(m, eax + 0x10u);
    if (fA_7d3dc1 < fB_7d3dc1) goto L_7d3dcc;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3dccu);
L_7d3dcc:
    esi = esi + 0x4u;
    goto L_7d3d80;
L_7d3dd1:
    eax = rd32(m, edi);
    goto L_7d3da2;
L_7d3dd5:
    eax = rd32(m, edi);
    goto L_7d3dc1;
L_7d3dd9:
    wr32(m, ebp, edi);
    eax = ebp;
    wr32(m, ebp + 0x4u, esi);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x8u;
    LIFT_RET(12);
L_7d3deb:
    esi = rd32(m, ebx + 0x2c7cu);
    fA_7d3df1 = rd32(m, ebx + 0x2c78u);
    fB_7d3df1 = esi;
    if (fA_7d3df1 <= fB_7d3df1) goto L_7d3dff;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x7d3dffu);
L_7d3dff:
    eax = rd32(m, ebx + 0x2c6cu);
    edi = rd32(m, esp); esp += 4;
    wr32(m, ebp + 0x4u, esi);
    wr32(m, ebp, eax);
    esi = rd32(m, esp); esp += 4;
    eax = ebp;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x8u;
    LIFT_RET(12);
}

// 007d4030 Fn_007d4030
//   original FUN_007d4030; 0 instructions per frame (plan); register inputs: ecx esi; returns with ret 0
REFLEX_PORT_FN(0x007d4030, Fn_007d4030, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7d4040 = 0;
    uint32_t fB_7d4040 = 0;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edi);
    wr32(m, esi, 0x972844u);
    eax = rd32(m, esi + 0x130u);
    edi = 0u;
    fA_7d4040 = eax;
    fB_7d4040 = edi;
    if (fA_7d4040 == fB_7d4040) goto L_7d4064;
    ecx = rd32(m, esp + 0x4u);
    edx = rd32(m, esi + 0x134u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x545120u, 0x7d4055u);  // FUN_00545120
    eax = rd32(m, esi + 0x130u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x78fc50u, 0x7d4061u);  // ReflexAlignedFree
    esp = esp + 0xcu;
L_7d4064:
    ecx = rd32(m, esi + 0x124u);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esi + 0x130u, edi);
    wr32(m, esi + 0x134u, edi);
    wr32(m, esi + 0x138u, edi);
    LIFT_CALL(0x78fc50u, 0x7d4082u);  // ReflexAlignedFree
    esp = esp + 0x4u;
    edi = (esi + 0x50u);
    LIFT_CALL(0x7dd4d0u, 0x7d408du);  // 
    edi = (esi + 0x10u);
    LIFT_CALL(0x7dd4d0u, 0x7d4095u);  // 
    wr32(m, esi, 0x93eb78u);
    edi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 007d6320 Fn_007d6320
//   original FUN_007d6320; 0 instructions per frame (plan); register inputs: esi edi; returns with ret 0
REFLEX_PORT_FN(0x007d6320, Fn_007d6320, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7d6333 = 0;
    uint32_t fB_7d6333 = 0;
    uint32_t fR_7d633e = 0;
    uint32_t fR_7d6347 = 0;
    uint32_t fR_7d6356 = 0;
    uint32_t fR_7d635f = 0;
    eax = rd32(m, esi + 0x34u);
    ecx = rd32(m, esi + 0x2cu);
    edx = eax;
    edx = edx - rd32(m, esi + 0x30u);
    esp -= 4; wr32(m, esp, ebx);
    ebx = 0x1u;
    edx = edx + ebx;
    fA_7d6333 = edx;
    fB_7d6333 = ecx;
    if (fA_7d6333 <= fB_7d6333) goto L_7d633b;
    eax = eax - ecx;
    goto L_7d6369;
L_7d633b:
    ecx = rd32(m, esi + 0x54u);
    fR_7d633e = (ecx & ecx);
    if ((fR_7d633e == 0)) goto L_7d6353;
    LIFT_CALL(0x7d5e70u, 0x7d6347u);  // FUN_007d5e70
    fR_7d6347 = (eax & eax);
    if ((fR_7d6347 == 0)) goto L_7d6353;
    wr32(m, esi + 0x34u, rd32(m, esi + 0x34u) + ebx);
    eax = rd32(m, esi + 0x34u);
    goto L_7d6369;
L_7d6353:
    ecx = rd32(m, esi + 0x50u);
    fR_7d6356 = (ecx & ecx);
    if ((fR_7d6356 == 0)) goto L_7d638d;
    LIFT_CALL(0x7d60f0u, 0x7d635fu);  // FUN_007d60f0
    fR_7d635f = (eax & eax);
    if ((fR_7d635f == 0)) goto L_7d638d;
    wr32(m, esi + 0x30u, (rd32(m, esi + 0x30u) - 1u));
    eax = rd32(m, esi + 0x30u);
L_7d6369:
    ecx = rd32(m, edi);
    eax = (eax << 5);
    eax = eax + rd32(m, esi + 0x64u);
    wr32(m, eax, ecx);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x4u)));
    wr32(m, eax + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + 0x8u)));
    wr32(m, eax + 0x8u, fst32(m));
    fpop(m);
    edx = rd32(m, edi + 0xcu);
    wr32(m, eax + 0xcu, edx);
    x0 = ldx(m, edi + 0x10u);
    stx(m, eax + 0x10u, x0);
L_7d638d:
    wr32(m, esi + 0x2cu, rd32(m, esi + 0x2cu) + ebx);
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 007d7230 Fn_007d7230
//   original FUN_007d7230; 0 instructions per frame (plan); register inputs: ecx eax xmm0; returns with ret 4
REFLEX_PORT_FN(0x007d7230, Fn_007d7230, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7d725f = 0;
    uint32_t fB_7d725f = 0;
    uint32_t fA_7d72cb = 0;
    uint32_t fB_7d72cb = 0;
    uint32_t fA_7d72dd = 0;
    uint32_t fB_7d72dd = 0;
    uint32_t fR_7d72e9 = 0;
    uint32_t fR_7d72f1 = 0;
    uint32_t fR_7d730c = 0;
    uint32_t fA_7d731b = 0;
    uint32_t fB_7d731b = 0;
    uint32_t fA_7d7337 = 0;
    uint32_t fB_7d7337 = 0;
    uint32_t fA_7d7349 = 0;
    uint32_t fB_7d7349 = 0;
    uint32_t fR_7d7356 = 0;
    uint32_t fA_7d736f = 0;
    uint32_t fB_7d736f = 0;
    uint32_t fR_7d7381 = 0;
    uint32_t fA_7d7392 = 0;
    uint32_t fB_7d7392 = 0;
    uint32_t fA_7d739f = 0;
    uint32_t fB_7d739f = 0;
    uint32_t fA_7d73b3 = 0;
    uint32_t fB_7d73b3 = 0;
    uint32_t fA_7d73c5 = 0;
    uint32_t fB_7d73c5 = 0;
    uint32_t fA_7d73e2 = 0;
    uint32_t fB_7d73e2 = 0;
    uint32_t fR_7d73ec = 0;
    uint32_t fA_7d73fd = 0;
    uint32_t fB_7d73fd = 0;
    uint32_t fA_7d7408 = 0;
    uint32_t fB_7d7408 = 0;
    uint32_t fA_7d741a = 0;
    uint32_t fB_7d741a = 0;
    esp = esp - 0x28u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x30u);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    esp = esp - 0x8u;
    x0 = cvtss2sd(x0, x0.l(0));
    edi = eax;
    esi = ecx;
    wr64(m, esp, x0.lo);
    LIFT_CALL(rd32(m, 0x916238u) /* 8.286497193473875e-39 */, 0x7d7251u);
    esp = esp + 0x8u;
    LIFT_CALL(0x8fa0d0u, 0x7d7259u);  // FUN_008fa0d0
    ecx = rd32(m, esi + 0x18u);
    edx = (ecx + ecx);
    fA_7d725f = eax;
    fB_7d725f = edx;
    wr32(m, esp + 0x1cu, eax);
    wr32(m, esp + 0x28u, ecx);
    if (int32_t(sext(fA_7d725f, 4)) <= int32_t(sext(fB_7d725f, 4))) goto L_7d7289;
    eax = rd32(m, esi + 0xf90u);
    eax = uint32_t(int32_t(eax) * int32_t(rd32(m, esi + 0xf8cu)));
    wr32(m, ebx, eax);
    eax = 0x1u;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x28u;
    LIFT_RET(4);
L_7d7289:
    x0 = cvtsi2ss(x0, rd32(m, esi + 0x10u));
    x1 = movss_m(rd32(m, edi));
    x1 = subss(x1, x0.l(0));
    eax = cvttss2si(x1.l(0));
    wr32(m, esp + 0x2cu, eax);
    edx = (eax >> 31) ? 0xffffffffu : 0;
    { uint32_t d_ = ecx; sdiv32(eax, edx, d_); }
    x1 = movss_m(rd32(m, edi + 0x8u));
    x0 = cvtsi2ss(x0, rd32(m, esi + 0x14u));
    ecx = rd32(m, esi + 0x1cu);
    x1 = subss(x1, x0.l(0));
    ebp = cvttss2si(x1.l(0));
    wr32(m, esp + 0x14u, ecx);
    wr32(m, esp + 0x18u, ebp);
    edi = eax;
    eax = ebp;
    edx = (eax >> 31) ? 0xffffffffu : 0;
    { uint32_t d_ = ecx; sdiv32(eax, edx, d_); }
    ecx = rd32(m, esi + 0xf8cu);
    fA_7d72cb = edi;
    fB_7d72cb = ecx;
    wr32(m, esp + 0x34u, ecx);
    if (int32_t(sext(fA_7d72cb, 4)) >= int32_t(sext(fB_7d72cb, 4))) goto L_7d742f;
    edx = rd32(m, esi + 0xf90u);
    fA_7d72dd = eax;
    fB_7d72dd = edx;
    wr32(m, esp + 0x24u, edx);
    if (int32_t(sext(fA_7d72dd, 4)) >= int32_t(sext(fB_7d72dd, 4))) goto L_7d742f;
    fR_7d72e9 = (edi & edi);
    if ((sgn(fR_7d72e9, 4) != false)) goto L_7d742f;
    fR_7d72f1 = (eax & eax);
    if ((sgn(fR_7d72f1, 4) != false)) goto L_7d742f;
    esi = edx;
    esi = uint32_t(int32_t(esi) * int32_t(edi));
    edx = (esi + eax);
    wr32(m, esp + 0x20u, esi);
    wr32(m, ebx, edx);
    ecx = 0x1u;
    fR_7d730c = (eax & eax);
    if ((fR_7d730c == 0)) goto L_7d732e;
    esi = rd32(m, esp + 0x14u);
    ebp = ebp - rd32(m, esp + 0x1cu);
    esi = uint32_t(int32_t(esi) * int32_t(eax));
    fA_7d731b = ebp;
    fB_7d731b = esi;
    ebp = rd32(m, esp + 0x18u);
    if (int32_t(sext(fA_7d731b, 4)) >= int32_t(sext(fB_7d731b, 4))) goto L_7d732e;
    ecx = (edx - 0x1u);
    wr32(m, ebx + 0x4u, ecx);
    ecx = 0x2u;
L_7d732e:
    esi = rd32(m, esp + 0x24u);
    esi = (esi - 1u);
    wr32(m, esp + 0x30u, esi);
    fA_7d7337 = eax;
    fB_7d7337 = esi;
    esi = rd32(m, esp + 0x1cu);
    if (int32_t(sext(fA_7d7337, 4)) >= int32_t(sext(fB_7d7337, 4))) goto L_7d7356;
    ebx = (eax + 0x1u);
    ebx = uint32_t(int32_t(ebx) * int32_t(rd32(m, esp + 0x14u)));
    ebp = ebp + esi;
    fA_7d7349 = ebp;
    fB_7d7349 = ebx;
    ebx = rd32(m, esp + 0x3cu);
    if (int32_t(sext(fA_7d7349, 4)) <= int32_t(sext(fB_7d7349, 4))) goto L_7d7356;
    edx = (edx + 1u);
    wr32(m, ebx + ecx * 4, edx);
    ecx = (ecx + 1u);
L_7d7356:
    fR_7d7356 = (edi & edi);
    if ((fR_7d7356 == 0)) goto L_7d73c0;
    edx = rd32(m, esp + 0x24u);
    wr32(m, esp + 0x20u, rd32(m, esp + 0x20u) - edx);
    edx = rd32(m, esp + 0x28u);
    ebp = rd32(m, esp + 0x2cu);
    edx = uint32_t(int32_t(edx) * int32_t(edi));
    ebp = ebp - esi;
    fA_7d736f = ebp;
    fB_7d736f = edx;
    if (int32_t(sext(fA_7d736f, 4)) >= int32_t(sext(fB_7d736f, 4))) goto L_7d73c0;
    edx = rd32(m, esp + 0x20u);
    edx = edx + eax;
    wr32(m, ebx + ecx * 4, edx);
    ecx = (ecx + 1u);
    wr32(m, esp + 0x3cu, edx);
    fR_7d7381 = (eax & eax);
    if ((fR_7d7381 == 0)) goto L_7d739f;
    edx = rd32(m, esp + 0x14u);
    ebp = rd32(m, esp + 0x18u);
    edx = uint32_t(int32_t(edx) * int32_t(eax));
    ebp = ebp - esi;
    fA_7d7392 = ebp;
    fB_7d7392 = edx;
    if (int32_t(sext(fA_7d7392, 4)) >= int32_t(sext(fB_7d7392, 4))) goto L_7d739f;
    edx = rd32(m, esp + 0x3cu);
    edx = (edx - 1u);
    wr32(m, ebx + ecx * 4, edx);
    ecx = (ecx + 1u);
L_7d739f:
    fA_7d739f = eax;
    fB_7d739f = rd32(m, esp + 0x30u);
    if (int32_t(sext(fA_7d739f, 4)) >= int32_t(sext(fB_7d739f, 4))) goto L_7d73c0;
    ebp = rd32(m, esp + 0x18u);
    edx = (eax + 0x1u);
    edx = uint32_t(int32_t(edx) * int32_t(rd32(m, esp + 0x14u)));
    ebp = ebp + esi;
    fA_7d73b3 = ebp;
    fB_7d73b3 = edx;
    if (int32_t(sext(fA_7d73b3, 4)) <= int32_t(sext(fB_7d73b3, 4))) goto L_7d73c0;
    edx = rd32(m, esp + 0x3cu);
    edx = (edx + 1u);
    wr32(m, ebx + ecx * 4, edx);
    ecx = (ecx + 1u);
L_7d73c0:
    edx = rd32(m, esp + 0x34u);
    edx = (edx - 1u);
    fA_7d73c5 = edi;
    fB_7d73c5 = edx;
    if (int32_t(sext(fA_7d73c5, 4)) >= int32_t(sext(fB_7d73c5, 4))) goto L_7d7423;
    ebp = rd32(m, esp + 0x2cu);
    edx = (edi + 0x1u);
    edi = rd32(m, esp + 0x1cu);
    esi = edx;
    edx = uint32_t(int32_t(edx) * int32_t(rd32(m, esp + 0x28u)));
    esi = uint32_t(int32_t(esi) * int32_t(rd32(m, esp + 0x24u)));
    ebp = ebp + edi;
    fA_7d73e2 = ebp;
    fB_7d73e2 = edx;
    if (int32_t(sext(fA_7d73e2, 4)) <= int32_t(sext(fB_7d73e2, 4))) goto L_7d7423;
    esi = esi + eax;
    wr32(m, ebx + ecx * 4, esi);
    ecx = (ecx + 1u);
    fR_7d73ec = (eax & eax);
    if ((fR_7d73ec == 0)) goto L_7d7408;
    edx = rd32(m, esp + 0x14u);
    ebp = rd32(m, esp + 0x18u);
    edx = uint32_t(int32_t(edx) * int32_t(eax));
    ebp = ebp - edi;
    fA_7d73fd = ebp;
    fB_7d73fd = edx;
    if (int32_t(sext(fA_7d73fd, 4)) >= int32_t(sext(fB_7d73fd, 4))) goto L_7d7408;
    edx = (esi - 0x1u);
    wr32(m, ebx + ecx * 4, edx);
    ecx = (ecx + 1u);
L_7d7408:
    fA_7d7408 = eax;
    fB_7d7408 = rd32(m, esp + 0x30u);
    if (int32_t(sext(fA_7d7408, 4)) >= int32_t(sext(fB_7d7408, 4))) goto L_7d7423;
    edx = rd32(m, esp + 0x18u);
    eax = (eax + 1u);
    eax = uint32_t(int32_t(eax) * int32_t(rd32(m, esp + 0x14u)));
    edx = edx + edi;
    fA_7d741a = edx;
    fB_7d741a = eax;
    if (int32_t(sext(fA_7d741a, 4)) <= int32_t(sext(fB_7d741a, 4))) goto L_7d7423;
    esi = (esi + 1u);
    wr32(m, ebx + ecx * 4, esi);
    ecx = (ecx + 1u);
L_7d7423:
    eax = ecx;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x28u;
    LIFT_RET(4);
L_7d742f:
    eax = rd32(m, esi + 0xf90u);
    eax = uint32_t(int32_t(eax) * int32_t(ecx));
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    wr32(m, ebx, eax);
    ebp = rd32(m, esp); esp += 4;
    eax = 0x1u;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x28u;
    LIFT_RET(4);
}

// 007da1d0 Fn_007da1d0
//   original FUN_007da1d0; 0 instructions per frame (plan); register inputs: edi; returns with ret 12
REFLEX_PORT_FN(0x007da1d0, Fn_007da1d0, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7da21b = 0;
    uint32_t fB_7da21b = 0;
    uint32_t fA_7da268 = 0;
    uint32_t fB_7da268 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x68u;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp + 0x64u, eax);
    eax = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0x10u);
    wr32(m, esp + 0x14u, eax);
    eax = (edi + 0x20u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0x18u, eax);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x7da1feu);
    x0 = movss_m(rd32(m, ebp + 0xcu));
    ecx = (esp + 0x1cu);
    esp -= 4; wr32(m, esp, ecx);
    eax = ebx;
    ecx = edi;
    wr32(m, esp + 0x14u, 0x0u);
    LIFT_CALL(0x7d7230u, 0x7da219u);  // FUN_007d7230
    esi = eax;
    fA_7da21b = esi;
    fB_7da21b = 0x1u;
    esi = fA_7da21b - fB_7da21b;
    if (sgn(((fA_7da21b - fB_7da21b)), 4)) goto L_7da26d;
    x1 = X{};
    x2 = movss_m(rd32(m, ebp + 0xcu));
    goto L_7da230;
L_7da230:
    ecx = rd32(m, esp + esi * 4 + 0x1cu);
    edx = rd32(m, esp + 0x18u);
    ecx = uint32_t(int32_t(ecx) * int32_t(0x70u));
    ecx = ecx + rd32(m, edi + 0xf78u);
    x0 = ldx(m, ebx);
    eax = (esp + 0x40u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp + 0x48u, edx);
    wr32(m, esp + 0x4cu, x2.l(0));
    stx(m, esp + 0x58u, x0);
    wr32(m, esp + 0x50u, x1.l(0));
    LIFT_CALL(0x7da810u, 0x7da264u);  // FUN_007da810
    wr32(m, esp + 0x10u, (rd32(m, esp + 0x10u) | eax));
    fA_7da268 = esi;
    fB_7da268 = 0x1u;
    esi = fA_7da268 - fB_7da268;
    if (!sgn(((fA_7da268 - fB_7da268)), 4)) goto L_7da230;
L_7da26d:
    edx = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x7da278u);
    ecx = rd32(m, esp + 0x6cu);
    eax = rd32(m, esp + 0x10u);
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x7da289u);  // __security_check_cookie
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(12);
}

// 007da410 Fn_007da410
//   original FUN_007da410; 0 instructions per frame (plan); register inputs: edi; returns with ret 12
REFLEX_PORT_FN(0x007da410, Fn_007da410, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7da45b = 0;
    uint32_t fB_7da45b = 0;
    uint32_t fA_7da4a8 = 0;
    uint32_t fB_7da4a8 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x68u;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp + 0x64u, eax);
    eax = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0x10u);
    wr32(m, esp + 0x14u, eax);
    eax = (edi + 0x20u);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0x18u, eax);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x7da43eu);
    x0 = movss_m(rd32(m, ebp + 0xcu));
    ecx = (esp + 0x1cu);
    esp -= 4; wr32(m, esp, ecx);
    eax = ebx;
    ecx = edi;
    wr32(m, esp + 0x14u, 0x0u);
    LIFT_CALL(0x7d7230u, 0x7da459u);  // FUN_007d7230
    esi = eax;
    fA_7da45b = esi;
    fB_7da45b = 0x1u;
    esi = fA_7da45b - fB_7da45b;
    if (sgn(((fA_7da45b - fB_7da45b)), 4)) goto L_7da4ad;
    x1 = X{};
    x2 = movss_m(rd32(m, ebp + 0xcu));
    goto L_7da470;
L_7da470:
    ecx = rd32(m, esp + esi * 4 + 0x1cu);
    edx = rd32(m, esp + 0x18u);
    ecx = uint32_t(int32_t(ecx) * int32_t(0x70u));
    ecx = ecx + rd32(m, edi + 0xf78u);
    x0 = ldx(m, ebx);
    eax = (esp + 0x40u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp + 0x48u, edx);
    wr32(m, esp + 0x4cu, x2.l(0));
    stx(m, esp + 0x58u, x0);
    wr32(m, esp + 0x50u, x1.l(0));
    LIFT_CALL(0x7dad20u, 0x7da4a4u);  // FUN_007dad20
    wr32(m, esp + 0x10u, (rd32(m, esp + 0x10u) | eax));
    fA_7da4a8 = esi;
    fB_7da4a8 = 0x1u;
    esi = fA_7da4a8 - fB_7da4a8;
    if (!sgn(((fA_7da4a8 - fB_7da4a8)), 4)) goto L_7da470;
L_7da4ad:
    edx = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x7da4b8u);
    ecx = rd32(m, esp + 0x6cu);
    eax = rd32(m, esp + 0x10u);
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x7da4c9u);  // __security_check_cookie
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(12);
}

// 007da740 Fn_007da740
//   original FUN_007da740; 0 instructions per frame (plan); register inputs: ecx edx; returns with ret 12
REFLEX_PORT_FN(0x007da740, Fn_007da740, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_7da791 = 0;
    uint32_t fA_7da7e5 = 0;
    uint32_t fB_7da7e5 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x74u;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp + 0x70u, eax);
    eax = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = ecx;
    wr32(m, esp + 0x24u, eax);
    eax = (edi + 0x20u);
    esi = edx;
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0x2cu, edi);
    wr32(m, esp + 0x20u, esi);
    wr32(m, esp + 0x24u, eax);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x7da778u);
    x0 = movss_m(rd32(m, ebp + 0xcu));
    ecx = (esp + 0x2cu);
    esp -= 4; wr32(m, esp, ecx);
    eax = esi;
    ecx = edi;
    LIFT_CALL(0x7d7230u, 0x7da78bu);  // FUN_007d7230
    ebx = 0u;
    wr32(m, esp + 0x18u, eax);
    fR_7da791 = (eax & eax);
    if (((fR_7da791 == 0) || sgn(fR_7da791, 4) != false)) goto L_7da7eb;
    goto L_7da7a8;
L_7da7a0:
    esi = rd32(m, esp + 0x1cu);
    edi = rd32(m, esp + 0x28u);
L_7da7a8:
    x0 = movss_m(rd32(m, ebp + 0xcu));
    edx = rd32(m, esp + 0x24u);
    wr32(m, esp + 0x54u, x0.l(0));
    x0 = ldx(m, esi);
    esi = rd32(m, esp + ebx * 4 + 0x2cu);
    esi = uint32_t(int32_t(esi) * int32_t(0x70u));
    esi = esi + rd32(m, edi + 0xf78u);
    stx(m, esp + 0x60u, x0);
    x0 = movss_m(rd32(m, ebp + 0x10u));
    edi = (esp + 0x50u);
    wr32(m, esp + 0x50u, edx);
    wr32(m, esp + 0x58u, x0.l(0));
    LIFT_CALL(0x7d6320u, 0x7da7e4u);  // FUN_007d6320
    ebx = (ebx + 1u);
    fA_7da7e5 = ebx;
    fB_7da7e5 = rd32(m, esp + 0x18u);
    if (int32_t(sext(fA_7da7e5, 4)) < int32_t(sext(fB_7da7e5, 4))) goto L_7da7a0;
L_7da7eb:
    eax = rd32(m, esp + 0x20u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x7da7f6u);
    ecx = rd32(m, esp + 0x7cu);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x7da804u);  // __security_check_cookie
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(12);
}

// 007da810 Fn_007da810
//   original FUN_007da810; 0 instructions per frame (plan); register inputs: ecx; returns with ret 8
REFLEX_PORT_FN(0x007da810, Fn_007da810, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7da821 = 0;
    uint32_t fB_7da821 = 0;
    uint32_t fA_7da843 = 0;
    uint32_t fB_7da843 = 0;
    uint32_t fA_7da886 = 0;
    uint32_t fB_7da886 = 0;
    uint32_t fA_7da8cb = 0;
    uint32_t fB_7da8cb = 0;
    uint32_t fA_7da910 = 0;
    uint32_t fB_7da910 = 0;
    uint32_t fA_7da948 = 0;
    uint32_t fB_7da948 = 0;
    uint32_t fA_7da950 = 0;
    uint32_t fB_7da950 = 0;
    uint32_t fA_7da968 = 0;
    uint32_t fB_7da968 = 0;
    uint32_t fA_7da99b = 0;
    uint32_t fB_7da99b = 0;
    uint32_t fA_7da9a1 = 0;
    uint32_t fB_7da9a1 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp -= 4; wr32(m, esp, ecx);
    eax = rd32(m, ebp + 0x8u);
    ecx = rd32(m, eax + 0x38u);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esi = 0u;
    fA_7da821 = ecx;
    fB_7da821 = 0x4u;
    esp -= 4; wr32(m, esp, edi);
    wr32(m, esp + 0xcu, ecx);
    if (int32_t(sext(fA_7da821, 4)) < int32_t(sext(fB_7da821, 4))) goto L_7da950;
L_7da82f:
    ecx = rd32(m, eax + 0x40u);
    edi = rd32(m, eax + 0x58u);
    ebx = rd32(m, ebp + 0xcu);
    ebx = rd32(m, ebx);
    edx = ecx;
    edx = edx - esi;
    edx = (edx << 5);
    edx = edx + edi;
    fA_7da843 = ebx;
    fB_7da843 = rd32(m, edx);
    if (fA_7da843 != fB_7da843) goto L_7da871;
    ecx = ecx - rd32(m, eax + 0x38u);
    ecx = (ecx + 1u);
    ecx = (ecx << 5);
    ecx = ecx + edi;
    edi = rd32(m, ecx);
    wr32(m, edx, edi);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x4u)));
    wr32(m, edx + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x8u)));
    wr32(m, edx + 0x8u, fst32(m));
    fpop(m);
    edi = rd32(m, ecx + 0xcu);
    wr32(m, edx + 0xcu, edi);
    x0 = ldx(m, ecx + 0x10u);
    stx(m, edx + 0x10u, x0);
    wr32(m, eax + 0x38u, (rd32(m, eax + 0x38u) - 1u));
L_7da871:
    ecx = rd32(m, eax + 0x40u);
    edi = rd32(m, eax + 0x58u);
    ebx = rd32(m, ebp + 0xcu);
    ebx = rd32(m, ebx);
    edx = ecx;
    edx = edx - esi;
    edx = (edx - 1u);
    edx = (edx << 5);
    edx = edx + edi;
    fA_7da886 = ebx;
    fB_7da886 = rd32(m, edx);
    if (fA_7da886 != fB_7da886) goto L_7da8b4;
    ecx = ecx - rd32(m, eax + 0x38u);
    ecx = (ecx + 1u);
    ecx = (ecx << 5);
    ecx = ecx + edi;
    edi = rd32(m, ecx);
    wr32(m, edx, edi);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x4u)));
    wr32(m, edx + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x8u)));
    wr32(m, edx + 0x8u, fst32(m));
    fpop(m);
    edi = rd32(m, ecx + 0xcu);
    wr32(m, edx + 0xcu, edi);
    x0 = ldx(m, ecx + 0x10u);
    stx(m, edx + 0x10u, x0);
    wr32(m, eax + 0x38u, (rd32(m, eax + 0x38u) - 1u));
L_7da8b4:
    ecx = rd32(m, eax + 0x40u);
    edi = rd32(m, eax + 0x58u);
    ebx = rd32(m, ebp + 0xcu);
    ebx = rd32(m, ebx);
    edx = ecx;
    edx = edx - esi;
    edx = edx - 0x2u;
    edx = (edx << 5);
    edx = edx + edi;
    fA_7da8cb = ebx;
    fB_7da8cb = rd32(m, edx);
    if (fA_7da8cb != fB_7da8cb) goto L_7da8f9;
    ecx = ecx - rd32(m, eax + 0x38u);
    ecx = (ecx + 1u);
    ecx = (ecx << 5);
    ecx = ecx + edi;
    edi = rd32(m, ecx);
    wr32(m, edx, edi);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x4u)));
    wr32(m, edx + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x8u)));
    wr32(m, edx + 0x8u, fst32(m));
    fpop(m);
    edi = rd32(m, ecx + 0xcu);
    wr32(m, edx + 0xcu, edi);
    x0 = ldx(m, ecx + 0x10u);
    stx(m, edx + 0x10u, x0);
    wr32(m, eax + 0x38u, (rd32(m, eax + 0x38u) - 1u));
L_7da8f9:
    ecx = rd32(m, eax + 0x40u);
    edi = rd32(m, eax + 0x58u);
    ebx = rd32(m, ebp + 0xcu);
    ebx = rd32(m, ebx);
    edx = ecx;
    edx = edx - esi;
    edx = edx - 0x3u;
    edx = (edx << 5);
    edx = edx + edi;
    fA_7da910 = ebx;
    fB_7da910 = rd32(m, edx);
    if (fA_7da910 != fB_7da910) goto L_7da93e;
    ecx = ecx - rd32(m, eax + 0x38u);
    ecx = (ecx + 1u);
    ecx = (ecx << 5);
    ecx = ecx + edi;
    edi = rd32(m, ecx);
    wr32(m, edx, edi);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x4u)));
    wr32(m, edx + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x8u)));
    wr32(m, edx + 0x8u, fst32(m));
    fpop(m);
    edi = rd32(m, ecx + 0xcu);
    wr32(m, edx + 0xcu, edi);
    x0 = ldx(m, ecx + 0x10u);
    stx(m, edx + 0x10u, x0);
    wr32(m, eax + 0x38u, (rd32(m, eax + 0x38u) - 1u));
L_7da93e:
    ecx = rd32(m, esp + 0xcu);
    esi = esi + 0x4u;
    edx = (ecx - 0x3u);
    fA_7da948 = esi;
    fB_7da948 = edx;
    if (int32_t(sext(fA_7da948, 4)) < int32_t(sext(fB_7da948, 4))) goto L_7da82f;
L_7da950:
    fA_7da950 = esi;
    fB_7da950 = ecx;
    if (int32_t(sext(fA_7da950, 4)) >= int32_t(sext(fB_7da950, 4))) goto L_7da99f;
L_7da954:
    ecx = rd32(m, eax + 0x40u);
    edi = rd32(m, eax + 0x58u);
    ebx = rd32(m, ebp + 0xcu);
    ebx = rd32(m, ebx);
    edx = ecx;
    edx = edx - esi;
    edx = (edx << 5);
    edx = edx + edi;
    fA_7da968 = ebx;
    fB_7da968 = rd32(m, edx);
    if (fA_7da968 != fB_7da968) goto L_7da996;
    ecx = ecx - rd32(m, eax + 0x38u);
    ecx = (ecx + 1u);
    ecx = (ecx << 5);
    ecx = ecx + edi;
    edi = rd32(m, ecx);
    wr32(m, edx, edi);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x4u)));
    wr32(m, edx + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x8u)));
    wr32(m, edx + 0x8u, fst32(m));
    fpop(m);
    edi = rd32(m, ecx + 0xcu);
    wr32(m, edx + 0xcu, edi);
    x0 = ldx(m, ecx + 0x10u);
    stx(m, edx + 0x10u, x0);
    wr32(m, eax + 0x38u, (rd32(m, eax + 0x38u) - 1u));
L_7da996:
    ecx = rd32(m, esp + 0xcu);
    esi = (esi + 1u);
    fA_7da99b = esi;
    fB_7da99b = ecx;
    if (int32_t(sext(fA_7da99b, 4)) < int32_t(sext(fB_7da99b, 4))) goto L_7da954;
L_7da99f:
    edx = 0u;
    fA_7da9a1 = ecx;
    fB_7da9a1 = rd32(m, eax + 0x38u);
    edi = rd32(m, esp); esp += 4;
    edx = (edx & 0xffffff00u) | (((fA_7da9a1 != fB_7da9a1) ? 1u : 0u) & 0xffu);
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    eax = edx;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 007dad20 Fn_007dad20
//   original FUN_007dad20; 0 instructions per frame (plan); register inputs: ecx; returns with ret 8
REFLEX_PORT_FN(0x007dad20, Fn_007dad20, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7dad31 = 0;
    uint32_t fB_7dad31 = 0;
    uint32_t fA_7dad53 = 0;
    uint32_t fB_7dad53 = 0;
    uint32_t fA_7dad96 = 0;
    uint32_t fB_7dad96 = 0;
    uint32_t fA_7daddb = 0;
    uint32_t fB_7daddb = 0;
    uint32_t fA_7dae20 = 0;
    uint32_t fB_7dae20 = 0;
    uint32_t fA_7dae58 = 0;
    uint32_t fB_7dae58 = 0;
    uint32_t fA_7dae60 = 0;
    uint32_t fB_7dae60 = 0;
    uint32_t fA_7dae78 = 0;
    uint32_t fB_7dae78 = 0;
    uint32_t fA_7daeab = 0;
    uint32_t fB_7daeab = 0;
    uint32_t fA_7daeb1 = 0;
    uint32_t fB_7daeb1 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp -= 4; wr32(m, esp, ecx);
    eax = rd32(m, ebp + 0x8u);
    ecx = rd32(m, eax + 0x2cu);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esi = 0u;
    fA_7dad31 = ecx;
    fB_7dad31 = 0x4u;
    esp -= 4; wr32(m, esp, edi);
    wr32(m, esp + 0xcu, ecx);
    if (int32_t(sext(fA_7dad31, 4)) < int32_t(sext(fB_7dad31, 4))) goto L_7dae60;
L_7dad3f:
    ecx = rd32(m, eax + 0x34u);
    edi = rd32(m, eax + 0x64u);
    ebx = rd32(m, ebp + 0xcu);
    ebx = rd32(m, ebx);
    edx = ecx;
    edx = edx - esi;
    edx = (edx << 5);
    edx = edx + edi;
    fA_7dad53 = ebx;
    fB_7dad53 = rd32(m, edx);
    if (fA_7dad53 != fB_7dad53) goto L_7dad81;
    ecx = ecx - rd32(m, eax + 0x2cu);
    ecx = (ecx + 1u);
    ecx = (ecx << 5);
    ecx = ecx + edi;
    edi = rd32(m, ecx);
    wr32(m, edx, edi);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x4u)));
    wr32(m, edx + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x8u)));
    wr32(m, edx + 0x8u, fst32(m));
    fpop(m);
    edi = rd32(m, ecx + 0xcu);
    wr32(m, edx + 0xcu, edi);
    x0 = ldx(m, ecx + 0x10u);
    stx(m, edx + 0x10u, x0);
    wr32(m, eax + 0x2cu, (rd32(m, eax + 0x2cu) - 1u));
L_7dad81:
    ecx = rd32(m, eax + 0x34u);
    edi = rd32(m, eax + 0x64u);
    ebx = rd32(m, ebp + 0xcu);
    ebx = rd32(m, ebx);
    edx = ecx;
    edx = edx - esi;
    edx = (edx - 1u);
    edx = (edx << 5);
    edx = edx + edi;
    fA_7dad96 = ebx;
    fB_7dad96 = rd32(m, edx);
    if (fA_7dad96 != fB_7dad96) goto L_7dadc4;
    ecx = ecx - rd32(m, eax + 0x2cu);
    ecx = (ecx + 1u);
    ecx = (ecx << 5);
    ecx = ecx + edi;
    edi = rd32(m, ecx);
    wr32(m, edx, edi);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x4u)));
    wr32(m, edx + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x8u)));
    wr32(m, edx + 0x8u, fst32(m));
    fpop(m);
    edi = rd32(m, ecx + 0xcu);
    wr32(m, edx + 0xcu, edi);
    x0 = ldx(m, ecx + 0x10u);
    stx(m, edx + 0x10u, x0);
    wr32(m, eax + 0x2cu, (rd32(m, eax + 0x2cu) - 1u));
L_7dadc4:
    ecx = rd32(m, eax + 0x34u);
    edi = rd32(m, eax + 0x64u);
    ebx = rd32(m, ebp + 0xcu);
    ebx = rd32(m, ebx);
    edx = ecx;
    edx = edx - esi;
    edx = edx - 0x2u;
    edx = (edx << 5);
    edx = edx + edi;
    fA_7daddb = ebx;
    fB_7daddb = rd32(m, edx);
    if (fA_7daddb != fB_7daddb) goto L_7dae09;
    ecx = ecx - rd32(m, eax + 0x2cu);
    ecx = (ecx + 1u);
    ecx = (ecx << 5);
    ecx = ecx + edi;
    edi = rd32(m, ecx);
    wr32(m, edx, edi);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x4u)));
    wr32(m, edx + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x8u)));
    wr32(m, edx + 0x8u, fst32(m));
    fpop(m);
    edi = rd32(m, ecx + 0xcu);
    wr32(m, edx + 0xcu, edi);
    x0 = ldx(m, ecx + 0x10u);
    stx(m, edx + 0x10u, x0);
    wr32(m, eax + 0x2cu, (rd32(m, eax + 0x2cu) - 1u));
L_7dae09:
    ecx = rd32(m, eax + 0x34u);
    edi = rd32(m, eax + 0x64u);
    ebx = rd32(m, ebp + 0xcu);
    ebx = rd32(m, ebx);
    edx = ecx;
    edx = edx - esi;
    edx = edx - 0x3u;
    edx = (edx << 5);
    edx = edx + edi;
    fA_7dae20 = ebx;
    fB_7dae20 = rd32(m, edx);
    if (fA_7dae20 != fB_7dae20) goto L_7dae4e;
    ecx = ecx - rd32(m, eax + 0x2cu);
    ecx = (ecx + 1u);
    ecx = (ecx << 5);
    ecx = ecx + edi;
    edi = rd32(m, ecx);
    wr32(m, edx, edi);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x4u)));
    wr32(m, edx + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x8u)));
    wr32(m, edx + 0x8u, fst32(m));
    fpop(m);
    edi = rd32(m, ecx + 0xcu);
    wr32(m, edx + 0xcu, edi);
    x0 = ldx(m, ecx + 0x10u);
    stx(m, edx + 0x10u, x0);
    wr32(m, eax + 0x2cu, (rd32(m, eax + 0x2cu) - 1u));
L_7dae4e:
    ecx = rd32(m, esp + 0xcu);
    esi = esi + 0x4u;
    edx = (ecx - 0x3u);
    fA_7dae58 = esi;
    fB_7dae58 = edx;
    if (int32_t(sext(fA_7dae58, 4)) < int32_t(sext(fB_7dae58, 4))) goto L_7dad3f;
L_7dae60:
    fA_7dae60 = esi;
    fB_7dae60 = ecx;
    if (int32_t(sext(fA_7dae60, 4)) >= int32_t(sext(fB_7dae60, 4))) goto L_7daeaf;
L_7dae64:
    ecx = rd32(m, eax + 0x34u);
    edi = rd32(m, eax + 0x64u);
    ebx = rd32(m, ebp + 0xcu);
    ebx = rd32(m, ebx);
    edx = ecx;
    edx = edx - esi;
    edx = (edx << 5);
    edx = edx + edi;
    fA_7dae78 = ebx;
    fB_7dae78 = rd32(m, edx);
    if (fA_7dae78 != fB_7dae78) goto L_7daea6;
    ecx = ecx - rd32(m, eax + 0x2cu);
    ecx = (ecx + 1u);
    ecx = (ecx << 5);
    ecx = ecx + edi;
    edi = rd32(m, ecx);
    wr32(m, edx, edi);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x4u)));
    wr32(m, edx + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x8u)));
    wr32(m, edx + 0x8u, fst32(m));
    fpop(m);
    edi = rd32(m, ecx + 0xcu);
    wr32(m, edx + 0xcu, edi);
    x0 = ldx(m, ecx + 0x10u);
    stx(m, edx + 0x10u, x0);
    wr32(m, eax + 0x2cu, (rd32(m, eax + 0x2cu) - 1u));
L_7daea6:
    ecx = rd32(m, esp + 0xcu);
    esi = (esi + 1u);
    fA_7daeab = esi;
    fB_7daeab = ecx;
    if (int32_t(sext(fA_7daeab, 4)) < int32_t(sext(fB_7daeab, 4))) goto L_7dae64;
L_7daeaf:
    edx = 0u;
    fA_7daeb1 = ecx;
    fB_7daeb1 = rd32(m, eax + 0x2cu);
    edi = rd32(m, esp); esp += 4;
    edx = (edx & 0xffffff00u) | (((fA_7daeb1 != fB_7daeb1) ? 1u : 0u) & 0xffu);
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    eax = edx;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 007dbde0 Fn_007dbde0
//   original FUN_007dbde0; 3374 instructions per frame (plan); register inputs: esi; returns with ret 0
REFLEX_PORT_FN(0x007dbde0, Fn_007dbde0, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_7dbdf1 = 0;
    uint32_t fA_7dbdfb = 0;
    uint32_t fB_7dbdfb = 0;
    uint32_t fA_7dc18a = 0;
    uint32_t fB_7dc18a = 0;
    uint32_t fR_7dc208 = 0;
    uint32_t fA_7dc3dd = 0;
    uint32_t fB_7dc3dd = 0;
    uint32_t fR_7dc41a = 0;
    uint32_t fA_7dc442 = 0;
    uint32_t fB_7dc442 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    eax = (eax & 0xffffff00u) | ((rd8(m, esi + 0xcu)) & 0xffu);
    esp = esp - 0xc8u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, edi);
    fR_7dbdf1 = ((eax & 0xffu) & 0xcu);
    if (!(fR_7dbdf1 == 0)) goto L_7dc54d;
    eax = (eax & 0xffffff00u) | ((((eax & 0xffu) | 0x4u)) & 0xffu);
    fA_7dbdfb = rd8(m, esi + 0xeu);
    fB_7dbdfb = 0xffu;
    wr8(m, esi + 0xcu, (eax & 0xffu));
    if (fA_7dbdfb != fB_7dbdfb) goto L_7dbe59;
    eax = rd8(m, esi + 0xbu);
    ecx = rd32(m, esi);
    edx = rd32(m, ecx + 0x20u);
    eax = (eax + eax * 4);
    eax = (eax << 4);
    x0 = ldx(m, eax + edx);
    stx(m, eax + edx + 0x20u, x0);
    ecx = rd32(m, esi);
    eax = eax + edx;
    eax = rd8(m, esi + 0xbu);
    edx = rd32(m, ecx + 0x20u);
    eax = (eax + eax * 4);
    eax = (eax << 4);
    x0 = ldx(m, eax + edx + 0x10u);
    eax = eax + edx;
    stx(m, eax + 0x30u, x0);
    eax = rd8(m, esi + 0xbu);
    ecx = rd32(m, esi);
    edx = rd32(m, ecx + 0x20u);
    eax = (eax + eax * 4);
    eax = (eax << 4);
    eax = eax + edx;
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x40u)));
    wr32(m, eax + 0x44u, fst32(m));
    fpop(m);
    wr8(m, esi + 0xcu, (rd8(m, esi + 0xcu) | 0x4u));
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_7dbe59:
    esp -= 4; wr32(m, esp, 0x1u);
    ecx = esi;
    LIFT_CALL(0x7db890u, 0x7dbe62u);  // Node_EvaluateWorld
    edx = rd8(m, esi + 0xbu);
    ecx = rd32(m, esi);
    eax = edx;
    eax = (eax << 6);
    eax = eax + rd32(m, ecx + 0x1cu);
    x1 = X{};
    stx(m, esp + 0x30u, x1);
    x4 = ldx(m, eax);
    x3 = ldx(m, eax + 0x30u);
    x5 = ldx(m, eax + 0x10u);
    x6 = ldx(m, eax + 0x20u);
    x0 = x4;
    x0 = shufps(x0, x5, 0x44);
    x7 = x0;
    x2 = x6;
    x2 = shufps(x2, x3, 0x44);
    x0 = shufps(x0, x2, 0xdd);
    x7 = shufps(x7, x2, 0x88);
    stx(m, esp + 0x50u, x0);
    x1 = subps(x1, x3);
    x2 = x4;
    x2 = shufps(x2, x5, 0xee);
    x0 = x6;
    x0 = shufps(x0, x3, 0xee);
    x2 = shufps(x2, x0, 0x88);
    x0 = ldx(m, 0xd61660u);
    x3 = ldx(m, esp + 0x50u);
    stx(m, esp + 0x20u, x0);
    eax = rd32(m, esp + 0x20u);
    stx(m, esp + 0x10u, x0);
    stx(m, esp + 0x80u, x0);
    stx(m, esp + 0xa0u, x1);
    stx(m, esp + 0x90u, x4);
    x0 = movss_m(rd32(m, esp + 0x94u));
    x0 = cvtps2pd(x0.lo);
    stx(m, esp + 0xc0u, x7);
    wr32(m, esp + 0xccu, eax);
    eax = rd32(m, esp + 0x10u);
    stx(m, esp + 0x10u, x2);
    x2 = movss_m(rd32(m, esp + 0xa4u));
    x2 = cvtps2pd(x2.lo);
    x0 = mulsd(x0, x2.lo);
    x2 = movss_m(rd32(m, esp + 0x90u));
    x2 = cvtps2pd(x2.lo);
    stx(m, esp + 0x20u, x3);
    x3 = movss_m(rd32(m, esp + 0xa0u));
    x3 = cvtps2pd(x3.lo);
    x2 = mulsd(x2, x3.lo);
    x3 = movss_m(rd32(m, esp + 0xa8u));
    x0 = addsd(x0, x2.lo);
    x2 = movss_m(rd32(m, esp + 0x98u));
    x2 = cvtps2pd(x2.lo);
    x3 = cvtps2pd(x3.lo);
    x2 = mulsd(x2, x3.lo);
    x0 = addsd(x0, x2.lo);
    x0 = cvtpd2ps(x0);
    wr32(m, esp + 0xa0u, x0.l(0));
    wr32(m, esp + 0xa4u, x0.l(0));
    wr32(m, esp + 0xa8u, x0.l(0));
    wr32(m, esp + 0xacu, x0.l(0));
    stx(m, esp + 0x90u, x5);
    x0 = movss_m(rd32(m, esp + 0x94u));
    stx(m, esp + 0xb0u, x1);
    x2 = movss_m(rd32(m, esp + 0xb4u));
    x3 = movss_m(rd32(m, esp + 0xb0u));
    x2 = cvtps2pd(x2.lo);
    x0 = cvtps2pd(x0.lo);
    x0 = mulsd(x0, x2.lo);
    x2 = movss_m(rd32(m, esp + 0x90u));
    x2 = cvtps2pd(x2.lo);
    x3 = cvtps2pd(x3.lo);
    x2 = mulsd(x2, x3.lo);
    wr32(m, esp + 0x2cu, eax);
    eax = rd32(m, esp + 0x80u);
    x0 = addsd(x0, x2.lo);
    x2 = movss_m(rd32(m, esp + 0x98u));
    wr32(m, esp + 0x1cu, eax);
    x2 = cvtps2pd(x2.lo);
    x3 = movss_m(rd32(m, esp + 0xb8u));
    stx(m, esp + 0x80u, x1);
    x1 = movss_m(rd32(m, esp + 0x84u));
    stx(m, esp + 0x90u, x6);
    x1 = cvtps2pd(x1.lo);
    x3 = cvtps2pd(x3.lo);
    x2 = mulsd(x2, x3.lo);
    x0 = addsd(x0, x2.lo);
    x2 = movss_m(rd32(m, esp + 0x80u));
    x0 = cvtpd2ps(x0);
    wr32(m, esp + 0xb0u, x0.l(0));
    wr32(m, esp + 0xb4u, x0.l(0));
    wr32(m, esp + 0xb8u, x0.l(0));
    wr32(m, esp + 0xbcu, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x94u));
    x0 = cvtps2pd(x0.lo);
    x0 = mulsd(x0, x1.lo);
    x1 = movss_m(rd32(m, esp + 0x90u));
    x1 = cvtps2pd(x1.lo);
    x2 = cvtps2pd(x2.lo);
    x1 = mulsd(x1, x2.lo);
    x2 = movss_m(rd32(m, esp + 0x88u));
    x0 = addsd(x0, x1.lo);
    x1 = movss_m(rd32(m, esp + 0x98u));
    x1 = cvtps2pd(x1.lo);
    x2 = cvtps2pd(x2.lo);
    x1 = mulsd(x1, x2.lo);
    x0 = addsd(x0, x1.lo);
    x1 = ldx(m, 0xd61670u);
    x0 = cvtpd2ps(x0);
    wr32(m, esp + 0x90u, x0.l(0));
    wr32(m, esp + 0x94u, x0.l(0));
    wr32(m, esp + 0x98u, x0.l(0));
    wr32(m, esp + 0x9cu, x0.l(0));
    x0 = ldx(m, esp + 0xa0u);
    stx(m, esp + 0xa0u, x0);
    eax = rd32(m, esp + 0xa4u);
    x0 = ldx(m, 0xd61690u);
    stx(m, esp + 0x80u, x0);
    x0 = ldx(m, esp + 0xb0u);
    stx(m, esp + 0xa0u, x0);
    wr32(m, esp + 0x80u, eax);
    x0 = ldx(m, esp + 0x80u);
    eax = rd32(m, esp + 0xa8u);
    stx(m, esp + 0x80u, x0);
    x0 = ldx(m, esp + 0x90u);
    wr32(m, esp + 0x84u, eax);
    stx(m, esp + 0xa0u, x0);
    x0 = ldx(m, esp + 0x80u);
    eax = rd32(m, esp + 0xacu);
    stx(m, esp + 0x90u, x0);
    wr32(m, esp + 0x98u, eax);
    eax = (edx + edx * 4);
    edx = rd32(m, ecx + 0x20u);
    eax = (eax << 4);
    x0 = ldx(m, eax + edx + 0x10u);
    x0 = cmpps<0>(x0, x1);
    stx(m, esp + 0x80u, x0);
    edx = rd32(m, esp + 0x84u);
    edi = rd32(m, esp + 0x88u);
    edi = edi + edx;
    edi = edi + rd32(m, esp + 0x80u);
    ebx = 0x1u;
    fA_7dc18a = edi;
    fB_7dc18a = 0xfffffffdu;
    if (fA_7dc18a == fB_7dc18a) goto L_7dc1c4;
    edx = rd32(m, ecx + 0x20u);
    x0 = ldx(m, edx + eax + 0x10u);
    x2 = ldx(m, edx + eax);
    edx = edx + eax;
    edx = rd32(m, ecx + 0x20u);
    x1 = ldx(m, eax + edx + 0x10u);
    eax = eax + edx;
    x2 = subps(x2, x0);
    x0 = ldx(m, eax);
    x1 = addps(x1, x0);
    stx(m, esp + 0xa0u, x2);
    stx(m, esp + 0xb0u, x1);
    ebx = 0u;
    goto L_7dc1f2;
L_7dc1c4:
    x0 = ldx(m, 0xd615e0u);
    stx(m, esp + 0xa0u, x0);
    x0 = ldx(m, 0xd615f0u);
    x2 = ldx(m, esp + 0xa0u);
    stx(m, esp + 0xb0u, x0);
    x1 = ldx(m, esp + 0xb0u);
L_7dc1f2:
    edi = rd8(m, esi + 0xeu);
    edi = (edi << 4);
    edi = edi + rd32(m, ecx + 0x8u);
    LIFT_CALL(0x7dea70u, 0x7dc201u);  // FUN_007dea70
    edi = rd32(m, esi);
    LIFT_CALL(0x7deaa0u, 0x7dc208u);  // FUN_007deaa0
    fR_7dc208 = (eax & eax);
    if ((fR_7dc208 == 0)) goto L_7dc432;
    x7 = ldx(m, esp + 0xc0u);
L_7dc218:
    ecx = rd8(m, eax + 0xbu);
    edx = rd32(m, eax);
    x5 = ldx(m, esp + 0x20u);
    x3 = ldx(m, esp + 0x90u);
    x4 = ldx(m, esp + 0x10u);
    ecx = (ecx << 6);
    ecx = ecx + rd32(m, edx + 0x1cu);
    eax = rd8(m, eax + 0xbu);
    x0 = ldx(m, ecx);
    x1 = x0;
    x1 = shufps(x1, x0, 0xff);
    x1 = mulps(x1, x3);
    x2 = x0;
    x2 = shufps(x2, x0, 0x55);
    x2 = mulps(x2, x5);
    x1 = addps(x1, x2);
    x2 = x0;
    x2 = shufps(x2, x0, 0xaa);
    x2 = mulps(x2, x4);
    x6 = x0;
    x6 = shufps(x6, x0, 0x0);
    x0 = ldx(m, ecx + 0x10u);
    x6 = mulps(x6, x7);
    x2 = addps(x2, x6);
    x1 = addps(x1, x2);
    x2 = x0;
    x2 = shufps(x2, x0, 0xff);
    x2 = mulps(x2, x3);
    x3 = x0;
    x3 = shufps(x3, x0, 0x55);
    x3 = mulps(x3, x5);
    x2 = addps(x2, x3);
    x3 = x0;
    x3 = shufps(x3, x0, 0xaa);
    x3 = mulps(x3, x4);
    x6 = x0;
    x6 = shufps(x6, x0, 0x0);
    x0 = ldx(m, ecx + 0x20u);
    x6 = mulps(x6, x7);
    x3 = addps(x3, x6);
    x2 = addps(x2, x3);
    x3 = x0;
    x3 = shufps(x3, x0, 0xff);
    x3 = mulps(x3, ldx(m, esp + 0x90u));
    x6 = x0;
    x6 = shufps(x6, x0, 0x55);
    x6 = mulps(x6, x5);
    x3 = addps(x3, x6);
    x6 = x0;
    x6 = shufps(x6, x0, 0xaa);
    x6 = mulps(x6, x4);
    x4 = x0;
    x4 = shufps(x4, x0, 0x0);
    x0 = ldx(m, ecx + 0x30u);
    ecx = rd32(m, edx + 0x20u);
    x4 = mulps(x4, x7);
    x6 = addps(x6, x4);
    x3 = addps(x3, x6);
    x4 = x0;
    x4 = shufps(x4, x0, 0xff);
    x4 = mulps(x4, ldx(m, esp + 0x90u));
    x6 = x0;
    x6 = shufps(x6, x0, 0x55);
    x6 = mulps(x6, x5);
    eax = (eax + eax * 4);
    x4 = addps(x4, x6);
    x5 = x0;
    x5 = shufps(x5, x0, 0xaa);
    x5 = mulps(x5, ldx(m, esp + 0x10u));
    x6 = x0;
    x6 = shufps(x6, x0, 0x0);
    eax = (eax << 4);
    x0 = ldx(m, eax + ecx);
    eax = eax + ecx;
    stx(m, esp + 0xc0u, x0);
    stx(m, esp + 0x80u, x0);
    x6 = mulps(x6, x7);
    x5 = addps(x5, x6);
    x0 = shufps(x0, x0, 0x0);
    x4 = addps(x4, x5);
    x0 = mulps(x0, x1);
    x5 = movss_m(rd32(m, esp + 0xc4u));
    x5 = shufps(x5, x5, 0x0);
    x5 = mulps(x5, x2);
    x0 = addps(x0, x5);
    x5 = x0;
    x0 = movss_m(rd32(m, esp + 0x88u));
    x0 = shufps(x0, x0, 0x0);
    x0 = mulps(x0, x3);
    x0 = addps(x0, x5);
    x0 = addps(x0, x4);
    x4 = ldx(m, eax + 0x10u);
    x6 = x4;
    x6 = shufps(x6, x4, 0x55);
    x6 = mulps(x6, x2);
    x2 = x4;
    x2 = shufps(x2, x4, 0xaa);
    x2 = mulps(x2, x3);
    x3 = ldx(m, esp + 0x30u);
    x5 = x4;
    x5 = shufps(x5, x4, 0x0);
    x5 = mulps(x5, x1);
    x1 = x3;
    x4 = x3;
    x1 = subps(x1, x6);
    x3 = subps(x3, x2);
    x3 = maxps(x3, x2);
    x1 = maxps(x1, x6);
    x4 = subps(x4, x5);
    x4 = maxps(x4, x5);
    x1 = addps(x1, x4);
    x1 = addps(x1, x3);
    x3 = ldx(m, 0xd61670u);
    x2 = x1;
    x2 = cmpps<0>(x2, x3);
    stx(m, esp + 0x80u, x2);
    edx = rd32(m, esp + 0x80u);
    eax = rd32(m, esp + 0x88u);
    eax = eax + edx;
    eax = eax + rd32(m, esp + 0x84u);
    fA_7dc3dd = eax;
    fB_7dc3dd = 0xfffffffdu;
    if (fA_7dc3dd == fB_7dc3dd) goto L_7dc413;
    x3 = ldx(m, esp + 0xa0u);
    x2 = x0;
    x2 = subps(x2, x1);
    x1 = addps(x1, x0);
    x0 = ldx(m, esp + 0xb0u);
    x3 = minps(x3, x2);
    x0 = maxps(x0, x1);
    stx(m, esp + 0xa0u, x3);
    stx(m, esp + 0xb0u, x0);
    ebx = 0u;
L_7dc413:
    edi = rd32(m, esi);
    LIFT_CALL(0x7deaa0u, 0x7dc41au);  // FUN_007deaa0
    fR_7dc41a = (eax & eax);
    if (!(fR_7dc41a == 0)) goto L_7dc218;
    x1 = ldx(m, esp + 0xb0u);
    x2 = ldx(m, esp + 0xa0u);
L_7dc432:
    eax = rd32(m, esi);
    ecx = 0u;
    wr32(m, eax + 0x4u, ecx);
    wr32(m, eax + 0x30u, ecx);
    eax = rd8(m, esi + 0xbu);
    edx = rd32(m, esi);
    fA_7dc442 = ebx;
    fB_7dc442 = ecx;
    ecx = (eax + eax * 4);
    eax = rd32(m, edx + 0x20u);
    if (fA_7dc442 != fB_7dc442) goto L_7dc507;
    x0 = movss_m(rd32(m, 0x9322ccu) /* 0.5 */);
    ecx = (ecx << 4);
    x3 = x1;
    x3 = addps(x3, x2);
    stx(m, ecx + eax + 0x20u, x3);
    eax = rd8(m, esi + 0xbu);
    edx = rd32(m, esi);
    ecx = (eax + eax * 4);
    eax = rd32(m, edx + 0x20u);
    x1 = subps(x1, x2);
    ecx = (ecx << 4);
    stx(m, ecx + eax + 0x30u, x1);
    eax = rd8(m, esi + 0xbu);
    edx = rd32(m, esi);
    ecx = (eax + eax * 4);
    eax = rd32(m, edx + 0x20u);
    ecx = (ecx << 4);
    x2 = ldx(m, ecx + eax + 0x20u);
    eax = (ecx + eax + 0x20u);
    x0 = shufps(x0, x0, 0x0);
    x1 = x0;
    x1 = mulps(x1, x2);
    stx(m, eax, x1);
    eax = rd8(m, esi + 0xbu);
    edx = rd32(m, esi);
    ecx = (eax + eax * 4);
    eax = rd32(m, edx + 0x20u);
    ecx = (ecx << 4);
    x1 = ldx(m, ecx + eax + 0x30u);
    x0 = mulps(x0, x1);
    stx(m, ecx + eax + 0x30u, x0);
    edx = rd32(m, esi);
    eax = (ecx + eax + 0x30u);
    eax = rd8(m, esi + 0xbu);
    ecx = (eax + eax * 4);
    eax = rd32(m, edx + 0x20u);
    ecx = (ecx << 4);
    x1 = ldx(m, eax + ecx + 0x30u);
    eax = eax + ecx;
    x1 = mulps(x1, x1);
    x0 = x1;
    x0 = shufps(x0, x1, 0x66);
    x1 = addss(x1, x0.l(0));
    x0 = shufps(x0, x0, 0x55);
    x1 = addss(x1, x0.l(0));
    x0 = x1;
    x0 = shufps(x0, x0, 0x0);
    x0 = sqrtps(x0, x0);
    wr32(m, eax + 0x44u, x0.l(0));
    wr8(m, esi + 0xcu, (rd8(m, esi + 0xcu) | 0x4u));
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_7dc507:
    x0 = ldx(m, 0xd61670u);
    ecx = (ecx << 4);
    stx(m, ecx + eax + 0x20u, x0);
    eax = rd8(m, esi + 0xbu);
    edx = rd32(m, esi);
    x0 = ldx(m, 0xd61670u);
    ecx = (eax + eax * 4);
    eax = rd32(m, edx + 0x20u);
    ecx = (ecx << 4);
    stx(m, ecx + eax + 0x30u, x0);
    eax = rd8(m, esi + 0xbu);
    edx = rd32(m, esi);
    x0 = X{};
    ecx = (eax + eax * 4);
    eax = rd32(m, edx + 0x20u);
    ecx = (ecx << 4);
    wr32(m, ecx + eax + 0x44u, x0.l(0));
    wr8(m, esi + 0xcu, (rd8(m, esi + 0xcu) | 0x4u));
L_7dc54d:
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 007dcdf0 Node_SetLocalRotation: ESI = node record, arg 0 = quaternion (x y z w); RET 4: the local rotation of the record from the quaternion (2 = [0x92e330], 1 = [0x931f24]), local translation row kept, then Node_Invalidate
//   original FUN_007dcdf0; 1891280 instructions per frame (plan); register inputs: esi; returns with ret 4
REFLEX_PORT_FN(0x007dcdf0, Node_SetLocalRotation, "anim", LIFT_OUTS) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x50u;
    eax = rd32(m, ebp + 0x8u);
    x1 = movss_m(rd32(m, eax + 0x8u));
    x4 = movss_m(rd32(m, eax + 0x4u));
    x2 = movss_m(rd32(m, eax));
    x3 = movss_m(rd32(m, 0x92e330u) /* 2.0 */);
    x6 = x1;
    x6 = mulss(x6, x3.l(0));
    x0 = x2;
    x0 = mulss(x0, x3.l(0));
    wr32(m, esp, x6.l(0));
    x5 = x4;
    x5 = mulss(x5, x3.l(0));
    x3 = x1;
    x3 = mulss(x3, x6.l(0));
    x6 = movss_m(rd32(m, eax + 0xcu));
    eax = rd8(m, esi + 0xbu);
    x7 = x6;
    ecx = rd32(m, esi);
    x7 = mulss(x7, x0.l(0));
    edx = rd32(m, ecx + 0x18u);
    x2 = mulss(x2, x0.l(0));
    wr32(m, esp + 0xcu, x7.l(0));
    x7 = x6;
    x6 = mulss(x6, rd32(m, esp));
    x7 = mulss(x7, x5.l(0));
    wr32(m, esp + 0x8u, x2.l(0));
    x2 = x4;
    x4 = mulss(x4, x0.l(0));
    x2 = mulss(x2, x5.l(0));
    wr32(m, esp + 0x4u, x7.l(0));
    wr32(m, esp, x6.l(0));
    x7 = x4;
    x4 = x1;
    x1 = mulss(x1, x5.l(0));
    x4 = mulss(x4, x0.l(0));
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x5 = x3;
    x5 = addss(x5, x2.l(0));
    x6 = x0;
    x6 = subss(x6, x5.l(0));
    x5 = x7;
    x7 = subss(x7, rd32(m, esp));
    x5 = addss(x5, rd32(m, esp));
    wr32(m, esp + 0x10u, x6.l(0));
    wr32(m, esp + 0x20u, x7.l(0));
    x7 = movss_m(rd32(m, esp + 0x8u));
    x3 = addss(x3, x7.l(0));
    x6 = x0;
    x6 = subss(x6, x3.l(0));
    x3 = movss_m(rd32(m, esp + 0xcu));
    wr32(m, esp + 0x24u, x6.l(0));
    x6 = x1;
    x1 = subss(x1, x3.l(0));
    wr32(m, esp + 0x34u, x1.l(0));
    x1 = x0;
    wr32(m, esp + 0x14u, x5.l(0));
    x2 = addss(x2, x7.l(0));
    x1 = subss(x1, x2.l(0));
    wr32(m, esp + 0x38u, x1.l(0));
    x1 = movss_m(rd32(m, 0xa95d50u));
    x5 = x4;
    x5 = subss(x5, rd32(m, esp + 0x4u));
    x4 = addss(x4, rd32(m, esp + 0x4u));
    wr32(m, esp + 0x40u, x1.l(0));
    x1 = movss_m(rd32(m, 0xa95d54u));
    wr32(m, esp + 0x18u, x5.l(0));
    x5 = X{};
    wr32(m, esp + 0x44u, x1.l(0));
    x1 = movss_m(rd32(m, 0xa95d58u));
    eax = (eax << 6);
    wr32(m, esp + 0x1cu, x5.l(0));
    eax = eax + edx;
    x6 = addss(x6, x3.l(0));
    wr32(m, esp + 0x48u, x1.l(0));
    x1 = ldx(m, esp + 0x10u);
    wr32(m, esp + 0x4cu, x0.l(0));
    x0 = ldx(m, eax + 0x30u);
    wr32(m, esp + 0x28u, x6.l(0));
    wr32(m, esp + 0x2cu, x5.l(0));
    wr32(m, esp + 0x30u, x4.l(0));
    wr32(m, esp + 0x3cu, x5.l(0));
    stx(m, eax, x1);
    x1 = ldx(m, esp + 0x20u);
    stx(m, eax + 0x10u, x1);
    x1 = ldx(m, esp + 0x30u);
    stx(m, eax + 0x20u, x1);
    x1 = ldx(m, esp + 0x40u);
    ecx = esi;
    stx(m, eax + 0x30u, x1);
    LIFT_CALL(0x7db7e0u, 0x7dcf95u);  // FUN_007db7e0
    eax = rd8(m, esi + 0xbu);
    ecx = rd32(m, esi);
    edx = rd32(m, ecx + 0x18u);
    eax = (eax << 6);
    stx(m, eax + edx + 0x30u, x0);
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 007dd160 Fn_007dd160
//   original FUN_007dd160; 72 instructions per frame (plan); register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x007dd160, Fn_007dd160, "anim", LIFT_OUTS) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x10u;
    eax = rd32(m, ebp + 0x8u);
    x0 = ldx(m, eax);
    eax = rd8(m, ecx + 0xbu);
    edx = rd32(m, ecx);
    eax = (eax << 6);
    eax = eax + rd32(m, edx + 0x18u);
    stx(m, esp, x0);
    x0 = movss_m(rd32(m, esp));
    wr32(m, eax + 0x30u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x4u));
    wr32(m, eax + 0x34u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x8u));
    wr32(m, eax + 0x38u, x0.l(0));
    LIFT_CALL(0x7db7e0u, 0x7dd1a4u);  // FUN_007db7e0
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 007dd380 Fn_007dd380
//   original FUN_007dd380; 24569 instructions per frame (plan); register inputs: ecx eax; returns with ret 0
REFLEX_PORT_FN(0x007dd380, Fn_007dd380, "anim", LIFT_OUTS) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, ecx);
    ecx = rd8(m, eax + 0xbu);
    edx = rd32(m, eax);
    ecx = (ecx << 6);
    ecx = ecx + rd32(m, edx + 0x18u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x401740u, 0x7dd393u);  // FUN_00401740
    esp = esp + 0x4u;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 007dd450 ModelAsset_RootBounds
//   original ModelAsset_RootBounds; 9639 instructions per frame (plan); register inputs: eax edi; returns with ret 8
REFLEX_PORT_FN(0x007dd450, ModelAsset_RootBounds, "anim", LIFT_OUTS) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    LIFT_CALL(0x7dbde0u, 0x7dd45du);  // FUN_007dbde0
    eax = rd8(m, esi + 0xbu);
    ecx = rd32(m, esi);
    edx = rd32(m, ecx + 0x20u);
    eax = (eax + eax * 4);
    eax = (eax << 4);
    eax = (eax + edx + 0x20u);
    fpush(m, reflex_f80::from_f32(rd32(m, eax)));
    wr32(m, ebx, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x4u)));
    wr32(m, ebx + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x8u)));
    wr32(m, ebx + 0x8u, fst32(m));
    fpop(m);
    eax = rd8(m, esi + 0xbu);
    ecx = rd32(m, esi);
    edx = rd32(m, ecx + 0x20u);
    eax = (eax + eax * 4);
    eax = (eax << 4);
    eax = (eax + edx + 0x30u);
    fpush(m, reflex_f80::from_f32(rd32(m, eax)));
    wr32(m, edi, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x4u)));
    wr32(m, edi + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x8u)));
    wr32(m, edi + 0x8u, fst32(m));
    fpop(m);
    eax = rd8(m, esi + 0xbu);
    ecx = rd32(m, esi);
    edx = rd32(m, ecx + 0x20u);
    eax = (eax + eax * 4);
    eax = (eax << 4);
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    fpush(m, reflex_f80::from_f32(rd32(m, eax + edx + 0x44u)));
    eax = rd32(m, esp + 0x8u);
    wr32(m, eax, fst32(m));
    fpop(m);
    LIFT_RET(8);
}

// 007dd930 NodeSet_CopyFrom: ESI = destination node set, EDI = source set: evaluates the source's world matrices, copies local / world / bounds arrays, flags with bits 0 and 2 cleared
//   original FUN_007dd930; 13940 instructions per frame (plan); register inputs: ecx esi edi; returns with ret 0
REFLEX_PORT_FN(0x007dd930, NodeSet_CopyFrom, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7dd937 = 0;
    uint32_t fB_7dd937 = 0;
    uint32_t fA_7dd95d = 0;
    uint32_t fB_7dd95d = 0;
    uint32_t fR_7dd9b1 = 0;
    uint32_t fR_7dd9c6 = 0;
    uint32_t fR_7dd9dc = 0;
    uint32_t fA_7dda0b = 0;
    uint32_t fB_7dda0b = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp -= 4; wr32(m, esp, ecx);
    fA_7dd937 = rd32(m, edi + 0xcu);
    fB_7dd937 = 0x0u;
    esp -= 4; wr32(m, esp, ebx);
    if (int32_t(sext(fA_7dd937, 4)) <= int32_t(sext(fB_7dd937, 4))) goto L_7dda10;
    wr32(m, esi + 0x30u, 0x0u);
    ebx = rd16(m, edi + 0x38u);
    ebx = ebx + rd32(m, edi + 0xcu);
    ecx = rd32(m, edi);
    esp -= 4; wr32(m, esp, 0x1u);
    wr32(m, esp + 0x8u, ebx);
    LIFT_CALL(0x7db890u, 0x7dd95du);  // Node_EvaluateWorld
    fA_7dd95d = rd32(m, esi + 0xcu);
    fB_7dd95d = 0x0u;
    if (fA_7dd95d != fB_7dd95d) goto L_7dd96e;
    esp -= 4; wr32(m, esp, 0x0u);
    edx = edi;
    ecx = esi;
    LIFT_CALL(0x7ddb40u, 0x7dd96eu);  // FUN_007ddb40
L_7dd96e:
    eax = rd32(m, edi + 0x18u);
    ecx = rd32(m, esi + 0x18u);
    ebx = (ebx << 6);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x8fa070u, 0x7dd97fu);  // MSVCR90.DLL::memcpy
    edx = rd32(m, edi + 0x1cu);
    eax = rd32(m, esi + 0x1cu);
    esp = esp + 0xcu;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x8fa070u, 0x7dd990u);  // MSVCR90.DLL::memcpy
    eax = rd32(m, esp + 0x10u);
    edx = rd32(m, edi + 0x20u);
    ecx = (eax + eax * 4);
    eax = rd32(m, esi + 0x20u);
    esp = esp + 0xcu;
    ecx = (ecx << 4);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x8fa070u, 0x7dd9abu);  // MSVCR90.DLL::memcpy
    eax = rd32(m, edi + 0x24u);
    esp = esp + 0xcu;
    fR_7dd9b1 = (eax & eax);
    if ((fR_7dd9b1 == 0)) goto L_7dd9c3;
    ecx = rd32(m, esi + 0x24u);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x8fa070u, 0x7dd9c0u);  // MSVCR90.DLL::memcpy
    esp = esp + 0xcu;
L_7dd9c3:
    eax = rd32(m, edi + 0x28u);
    fR_7dd9c6 = (eax & eax);
    if ((fR_7dd9c6 == 0)) goto L_7dd9d8;
    edx = rd32(m, esi + 0x28u);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x8fa070u, 0x7dd9d5u);  // MSVCR90.DLL::memcpy
    esp = esp + 0xcu;
L_7dd9d8:
    edx = rd32(m, esp + 0x4u);
    fR_7dd9dc = (edx & edx);
    if (((fR_7dd9dc == 0) || sgn(fR_7dd9dc, 4) != false)) goto L_7dda10;
    eax = 0u;
L_7dd9e2:
    ecx = rd32(m, edi + 0x8u);
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + ecx + 0xcu)) & 0xffu);
    ebx = rd32(m, esi + 0x8u);
    wr8(m, eax + ebx + 0xcu, (ecx & 0xffu));
    ecx = rd32(m, esi + 0x8u);
    wr8(m, eax + ecx + 0xcu, (rd8(m, eax + ecx + 0xcu) & 0xfeu));
    ecx = (eax + ecx + 0xcu);
    ecx = rd32(m, esi + 0x8u);
    wr8(m, eax + ecx + 0xcu, (rd8(m, eax + ecx + 0xcu) & 0xfbu));
    ecx = (eax + ecx + 0xcu);
    eax = eax + 0x10u;
    fA_7dda0b = edx;
    fB_7dda0b = 0x1u;
    edx = fA_7dda0b - fB_7dda0b;
    if (fA_7dda0b != fB_7dda0b) goto L_7dd9e2;
L_7dda10:
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 007de980 Fn_007de980
//   original FUN_007de980; 0 instructions per frame (plan); register inputs: esi; returns with ret 4
REFLEX_PORT_FN(0x007de980, Fn_007de980, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7de983 = 0;
    uint32_t fB_7de983 = 0;
    uint32_t fA_7de99a = 0;
    uint32_t fB_7de99a = 0;
    uint32_t fR_7de9ba = 0;
    uint32_t fA_7de9c2 = 0;
    uint32_t fB_7de9c2 = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = 0u;
    fA_7de983 = rd32(m, esi + 0xcu);
    fB_7de983 = ebx;
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, edi);
    if (int32_t(sext(fA_7de983, 4)) <= int32_t(sext(fB_7de983, 4))) goto L_7de9c7;
    ebp = rd32(m, 0x9162bcu) /* 8.287018476502604e-39 */;
    edi = 0u;
L_7de992:
    eax = rd32(m, esi + 0x8u);
    ecx = 0xffffu;
    fA_7de99a = rd16(m, eax + edi + 0x8u);
    fB_7de99a = (ecx & 0xffffu);
    if (fA_7de99a == fB_7de99a) goto L_7de9be;
    ecx = rd32(m, esp + 0x10u);
    edx = eax;
    eax = (edx + edi);
    edx = rd16(m, eax + 0x8u);
    eax = rd32(m, eax);
    edx = edx + rd32(m, eax + 0x10u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(ebp, 0x7de9b7u);
    esp = esp + 0x8u;
    fR_7de9ba = (eax & eax);
    if ((fR_7de9ba == 0)) goto L_7de9cf;
L_7de9be:
    ebx = (ebx + 1u);
    edi = edi + 0x10u;
    fA_7de9c2 = ebx;
    fB_7de9c2 = rd32(m, esi + 0xcu);
    if (int32_t(sext(fA_7de9c2, 4)) < int32_t(sext(fB_7de9c2, 4))) goto L_7de992;
L_7de9c7:
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_7de9cf:
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = 0x1u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 007de9e0 Fn_007de9e0
//   original FUN_007de9e0; 0 instructions per frame (plan); register inputs: edi; returns with ret 4
REFLEX_PORT_FN(0x007de9e0, Fn_007de9e0, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_7de9e8 = 0;
    uint32_t fB_7de9e8 = 0;
    uint32_t fA_7dea0f = 0;
    uint32_t fB_7dea0f = 0;
    uint32_t fR_7dea27 = 0;
    uint32_t fA_7dea2f = 0;
    uint32_t fB_7dea2f = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = 0u;
    esp -= 4; wr32(m, esp, esi);
    eax = 0u;
    esi = 0u;
    fA_7de9e8 = rd32(m, edi + 0xcu);
    fB_7de9e8 = ebx;
    wr32(m, edi + 0x30u, ebx);
    if (int32_t(sext(fA_7de9e8, 4)) <= int32_t(sext(fB_7de9e8, 4))) goto L_7dea45;
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, 0x9162bcu) /* 8.287018476502604e-39 */;
    goto L_7dea00;
L_7dea00:
    eax = rd32(m, edi + 0x8u);
    ecx = rd16(m, eax + ebx + 0x8u);
    eax = eax + ebx;
    edx = 0xffffu;
    fA_7dea0f = (ecx & 0xffffu);
    fB_7dea0f = (edx & 0xffffu);
    if (fA_7dea0f == fB_7dea0f) goto L_7dea2b;
    eax = rd32(m, eax);
    edx = rd32(m, esp + 0x10u);
    ecx = (ecx & 0xffffu);
    ecx = ecx + rd32(m, eax + 0x10u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(ebp, 0x7dea24u);
    esp = esp + 0x8u;
    fR_7dea27 = (eax & eax);
    if ((fR_7dea27 == 0)) goto L_7dea3c;
L_7dea2b:
    esi = (esi + 1u);
    ebx = ebx + 0x10u;
    fA_7dea2f = esi;
    fB_7dea2f = rd32(m, edi + 0xcu);
    if (int32_t(sext(fA_7dea2f, 4)) < int32_t(sext(fB_7dea2f, 4))) goto L_7dea00;
    ebp = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_7dea3c:
    esi = (esi << 4);
    esi = esi + rd32(m, edi + 0x8u);
    ebp = rd32(m, esp); esp += 4;
    eax = esi;
L_7dea45:
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 007deaa0 NodeSet_NextUnvisited: EDI = node set: advances the set's traversal cursor (+4) depth-first to the next record without the visited bit (bit 1 of +0xc), marking the current one visited
//   original FUN_007deaa0; 26773 instructions per frame (plan); register inputs: edi; returns with ret 0
REFLEX_PORT_FN(0x007deaa0, NodeSet_NextUnvisited, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_7deaa3 = 0;
    uint32_t fA_7deab8 = 0;
    uint32_t fB_7deab8 = 0;
    uint32_t fR_7deaca = 0;
    uint32_t fA_7dead7 = 0;
    uint32_t fB_7dead7 = 0;
    uint32_t fR_7deae9 = 0;
    uint32_t fA_7deaee = 0;
    uint32_t fB_7deaee = 0;
    uint32_t fR_7deb0d = 0;
    uint32_t fA_7deb12 = 0;
    uint32_t fB_7deb12 = 0;
    uint32_t fR_7deb2d = 0;
    uint32_t fA_7deb32 = 0;
    uint32_t fB_7deb32 = 0;
    uint32_t fA_7deb40 = 0;
    uint32_t fB_7deb40 = 0;
    uint32_t fA_7deb46 = 0;
    uint32_t fB_7deb46 = 0;
    eax = rd32(m, edi + 0x4u);
    fR_7deaa3 = (eax & eax);
    if (!(fR_7deaa3 == 0)) goto L_7deaa8;
    LIFT_RET(0);
L_7deaa8:
    esp -= 4; wr32(m, esp, ebx);
    ebx = (ebx & 0xffffff00u) | ((0x2u) & 0xffu);
    wr8(m, eax + 0xcu, (rd8(m, eax + 0xcu) | (ebx & 0xffu)));
    wr32(m, edi + 0x4u, 0x0u);
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0xeu)) & 0xffu);
    fA_7deab8 = (ecx & 0xffu);
    fB_7deab8 = 0xffu;
    if (fA_7deab8 == fB_7deab8) goto L_7dead4;
    edx = rd32(m, eax);
    edx = rd32(m, edx + 0x8u);
    ecx = (ecx & 0xffu);
    ecx = (ecx << 4);
    ecx = ecx + edx;
    fR_7deaca = (rd8(m, ecx + 0xcu) & (ebx & 0xffu));
    if (!(fR_7deaca == 0)) goto L_7dead4;
L_7deacf:
    wr32(m, edi + 0x4u, ecx);
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_7dead4:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax + 0xdu)) & 0xffu);
    fA_7dead7 = (ecx & 0xffu);
    fB_7dead7 = 0xffu;
    if (fA_7dead7 == fB_7dead7) goto L_7deaee;
    edx = rd32(m, eax);
    edx = rd32(m, edx + 0x8u);
    ecx = (ecx & 0xffu);
    ecx = (ecx << 4);
    ecx = ecx + edx;
    fR_7deae9 = (rd8(m, ecx + 0xcu) & (ebx & 0xffu));
    if ((fR_7deae9 == 0)) goto L_7deacf;
L_7deaee:
    fA_7deaee = rd8(m, eax + 0xfu);
    fB_7deaee = 0xffu;
    ecx = (eax + 0xfu);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    if (fA_7deaee == fB_7deaee) goto L_7deb4f;
L_7deb00:
    ecx = rd8(m, ecx);
    edx = rd32(m, esi);
    ecx = (ecx << 4);
    ecx = ecx + rd32(m, edx + 0x8u);
    esi = ecx;
    fR_7deb0d = (rd8(m, esi + 0xcu) & (ebx & 0xffu));
    if ((fR_7deb0d == 0)) goto L_7deb4f;
    fA_7deb12 = rd8(m, esi + 0xdu);
    fB_7deb12 = 0xffu;
    edx = (esi + 0xdu);
    if (fA_7deb12 == fB_7deb12) goto L_7deb40;
    goto L_7deb20;
L_7deb20:
    edx = rd8(m, edx);
    ecx = rd32(m, ecx);
    edx = (edx << 4);
    edx = edx + rd32(m, ecx + 0x8u);
    ecx = edx;
    fR_7deb2d = (rd8(m, ecx + 0xcu) & (ebx & 0xffu));
    if ((fR_7deb2d == 0)) goto L_7deb3d;
    fA_7deb32 = rd8(m, ecx + 0xdu);
    fB_7deb32 = 0xffu;
    edx = (ecx + 0xdu);
    if (fA_7deb32 != fB_7deb32) goto L_7deb20;
    goto L_7deb40;
L_7deb3d:
    wr32(m, edi + 0x4u, ecx);
L_7deb40:
    fA_7deb40 = rd32(m, edi + 0x4u);
    fB_7deb40 = 0x0u;
    if (fA_7deb40 != fB_7deb40) goto L_7deb4f;
    fA_7deb46 = rd8(m, esi + 0xfu);
    fB_7deb46 = 0xffu;
    ecx = (esi + 0xfu);
    if (fA_7deb46 != fB_7deb46) goto L_7deb00;
L_7deb4f:
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 007df2e0 Fn_007df2e0
//   original FUN_007df2e0; 0 instructions per frame (plan); register inputs: ecx; returns with ret 8
REFLEX_PORT_FN(0x007df2e0, Fn_007df2e0, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_7df2f0 = 0;
    uint32_t fR_7df33d = 0;
    uint32_t fR_7df346 = 0;
    uint32_t fA_7df350 = 0;
    uint32_t fB_7df350 = 0;
    uint32_t fA_7df358 = 0;
    uint32_t fB_7df358 = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    ecx = rd32(m, esi + 0x24u);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esp + 0x14u);
    fR_7df2f0 = (ecx & ecx);
    if ((fR_7df2f0 == 0)) goto L_7df341;
L_7df2f4:
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x7df2e0u, 0x7df2fbu);  // FUN_007df2e0
    ecx = rd32(m, esi + 0x28u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x7df2e0u, 0x7df305u);  // FUN_007df2e0
    ecx = rd32(m, esi + 0x2cu);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x7df2e0u, 0x7df30fu);  // FUN_007df2e0
    ecx = rd32(m, esi + 0x30u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x7df2e0u, 0x7df319u);  // FUN_007df2e0
    ecx = rd32(m, esi + 0x34u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x7df2e0u, 0x7df323u);  // FUN_007df2e0
    ecx = rd32(m, esi + 0x38u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x7df2e0u, 0x7df32du);  // FUN_007df2e0
    ecx = rd32(m, esi + 0x3cu);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x7df2e0u, 0x7df337u);  // FUN_007df2e0
    esi = rd32(m, esi + 0x40u);
    ecx = rd32(m, esi + 0x24u);
    fR_7df33d = (ecx & ecx);
    if (!(fR_7df33d == 0)) goto L_7df2f4;
L_7df341:
    edx = rd32(m, esi + 0x20u);
    eax = 0u;
    fR_7df346 = (edx & edx);
    if ((false || (fR_7df346 == 0))) goto L_7df366;
    ecx = (esi + 0x44u);
L_7df350:
    fA_7df350 = rd32(m, ecx);
    fB_7df350 = edi;
    if (fA_7df350 == fB_7df350) goto L_7df362;
    eax = (eax + 1u);
    ecx = ecx + 0x4u;
    fA_7df358 = eax;
    fB_7df358 = edx;
    if (fA_7df358 < fB_7df358) goto L_7df350;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(8);
L_7df362:
    wr32(m, esi + eax * 4 + 0x44u, ebx);
L_7df366:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 007dfdd0 Fn_007dfdd0
//   original FUN_007dfdd0; 0 instructions per frame (plan); register inputs: none; returns with ret 8
REFLEX_PORT_FN(0x007dfdd0, Fn_007dfdd0, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_7dfe16 = 0;
    uint32_t fR_7dff0e = 0;
    uint32_t fA_7dff87 = 0;
    uint32_t fB_7dff87 = 0;
    uint32_t fA_7dffb2 = 0;
    uint32_t fB_7dffb2 = 0;
    uint32_t fA_7dffdd = 0;
    uint32_t fB_7dffdd = 0;
    uint32_t fR_7e00f7 = 0;
    uint32_t fR_7e0116 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x54u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, 0x91600cu) /* 8.29485173491818e-39 */;
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, ebp + 0xcu);
    eax = (ebx + 0x1114u);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0x20u, eax);
    LIFT_CALL(esi, 0x7dfdf5u);
    eax = (ebx + 0x112cu);
    esp -= 4; wr32(m, esp, eax);
    wr32(m, esp + 0x1cu, eax);
    LIFT_CALL(esi, 0x7dfe02u);
    eax = rd32(m, edi);
    edx = rd32(m, eax + 0x64u);
    ecx = edi;
    LIFT_CALL(edx, 0x7dfe0bu);
    edx = rd32(m, eax);
    ecx = eax;
    eax = rd32(m, edx + 0x4u);
    LIFT_CALL(eax, 0x7dfe14u);
    edx = rd32(m, edi);
    fR_7dfe16 = (eax & eax);
    eax = rd32(m, edx + 0x64u);
    ecx = edi;
    if (!(fR_7dfe16 == 0)) goto L_7dff7c;
    LIFT_CALL(eax, 0x7dfe25u);
    esi = eax;
    edx = rd32(m, esi);
    eax = rd32(m, edx + 0x1cu);
    ecx = esi;
    LIFT_CALL(eax, 0x7dfe30u);
    st(m, 0) = st(m, 0);
    fpop(m);
    edx = rd32(m, esi);
    edx = rd32(m, edx + 0x18u);
    eax = (esp + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0x54u);
    esp -= 4; wr32(m, esp, ecx);
    eax = (esp + 0x38u);
    esp -= 4; wr32(m, esp, eax);
    ecx = esi;
    LIFT_CALL(edx, 0x7dfe4au);
    eax = rd32(m, edi);
    edx = rd32(m, eax + 0x38u);
    ecx = edi;
    LIFT_CALL(edx, 0x7dfe53u);
    x0 = movss_m(rd32(m, eax + 0x10u));
    x3 = movss_m(rd32(m, esp + 0x34u));
    x1 = movss_m(rd32(m, eax + 0x20u));
    x4 = movss_m(rd32(m, esp + 0x38u));
    x5 = movss_m(rd32(m, esp + 0x30u));
    x2 = movss_m(rd32(m, eax + 0x4u));
    ecx = rd32(m, ebx + 0x10fcu);
    x1 = mulss(x1, x4.l(0));
    x0 = mulss(x0, x3.l(0));
    x0 = addss(x0, x1.l(0));
    x1 = movss_m(rd32(m, eax));
    x1 = mulss(x1, x5.l(0));
    x0 = addss(x0, x1.l(0));
    x1 = movss_m(rd32(m, eax + 0x14u));
    x0 = addss(x0, rd32(m, eax + 0x30u));
    x2 = mulss(x2, x5.l(0));
    x1 = mulss(x1, x3.l(0));
    x1 = addss(x1, x2.l(0));
    x2 = movss_m(rd32(m, eax + 0x24u));
    x2 = mulss(x2, x4.l(0));
    x1 = addss(x1, x2.l(0));
    x2 = movss_m(rd32(m, eax + 0x18u));
    x1 = addss(x1, rd32(m, eax + 0x34u));
    x2 = mulss(x2, x3.l(0));
    x3 = movss_m(rd32(m, eax + 0x8u));
    x3 = mulss(x3, x5.l(0));
    x2 = addss(x2, x3.l(0));
    x3 = movss_m(rd32(m, eax + 0x28u));
    x3 = mulss(x3, x4.l(0));
    x2 = addss(x2, x3.l(0));
    x2 = addss(x2, rd32(m, eax + 0x38u));
    eax = rd32(m, ebx + 0x1108u);
    wr32(m, ecx + eax * 8, edi);
    wr32(m, ebx + 0x1108u, (rd32(m, ebx + 0x1108u) + 1u));
    edx = rd32(m, edi);
    eax = rd32(m, edx + 0x5cu);
    ecx = edi;
    wr32(m, esp + 0x20u, x0.l(0));
    wr32(m, esp + 0x24u, x1.l(0));
    wr32(m, esp + 0x28u, x2.l(0));
    LIFT_CALL(eax, 0x7dff0cu);
    edx = rd32(m, edi);
    fR_7dff0e = (eax & eax);
    eax = rd32(m, edx + 0x64u);
    ecx = edi;
    if ((fR_7dff0e == 0)) goto L_7dff4e;
    LIFT_CALL(eax, 0x7dff19u);
    edx = rd32(m, eax);
    ecx = eax;
    eax = rd32(m, edx + 0x1cu);
    LIFT_CALL(eax, 0x7dff22u);
    edx = rd32(m, edi);
    eax = rd32(m, edx + 0x5cu);
    esp = esp - 0x8u;
    wr32(m, esp + 0x4u, fst32(m));
    fpop(m);
    ecx = edi;
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x1cu)));
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(eax, 0x7dff39u);
    ecx = rd32(m, ebx + 0x10f0u);
    esp -= 4; wr32(m, esp, eax);
    edx = (esp + 0x2cu);
    LIFT_CALL(0x7da740u, 0x7dff49u);  // FUN_007da740
    goto L_7e0145;
L_7dff4e:
    LIFT_CALL(eax, 0x7dff50u);
    edx = rd32(m, eax);
    ecx = eax;
    eax = rd32(m, edx + 0x1cu);
    LIFT_CALL(eax, 0x7dff59u);
    ecx = rd32(m, ebx + 0x10f0u);
    esp = esp - 0x8u;
    wr32(m, esp + 0x4u, fst32(m));
    fpop(m);
    edx = (esp + 0x28u);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x1cu)));
    wr32(m, esp, fst32(m));
    fpop(m);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(0x7da670u, 0x7dff77u);  // FUN_007da670
    goto L_7e0145;
L_7dff7c:
    LIFT_CALL(eax, 0x7dff7eu);
    edx = rd32(m, eax);
    ecx = eax;
    eax = rd32(m, edx + 0x4u);
    LIFT_CALL(eax, 0x7dff87u);
    fA_7dff87 = eax;
    fB_7dff87 = 0x3u;
    if (fA_7dff87 != fB_7dff87) goto L_7dffa0;
    ecx = (ebp + 0xcu);
    esp -= 4; wr32(m, esp, ecx);
    esi = (ebx + 0x1014u);
    LIFT_CALL(0x544cc0u, 0x7dff9bu);  // FUN_00544cc0
    goto L_7e0145;
L_7dffa0:
    edx = rd32(m, edi);
    eax = rd32(m, edx + 0x64u);
    ecx = edi;
    LIFT_CALL(eax, 0x7dffa9u);
    edx = rd32(m, eax);
    ecx = eax;
    eax = rd32(m, edx + 0x4u);
    LIFT_CALL(eax, 0x7dffb2u);
    fA_7dffb2 = eax;
    fB_7dffb2 = 0x2u;
    if (fA_7dffb2 != fB_7dffb2) goto L_7dffcb;
    ecx = (ebp + 0xcu);
    esp -= 4; wr32(m, esp, ecx);
    esi = (ebx + 0x1000u);
    LIFT_CALL(0x544cc0u, 0x7dffc6u);  // FUN_00544cc0
    goto L_7e0145;
L_7dffcb:
    edx = rd32(m, edi);
    eax = rd32(m, edx + 0x64u);
    ecx = edi;
    LIFT_CALL(eax, 0x7dffd4u);
    edx = rd32(m, eax);
    ecx = eax;
    eax = rd32(m, edx + 0x4u);
    LIFT_CALL(eax, 0x7dffddu);
    fA_7dffdd = eax;
    fB_7dffdd = 0x1u;
    if (fA_7dffdd != fB_7dffdd) goto L_7e0145;
    edx = rd32(m, edi);
    eax = rd32(m, edx + 0x64u);
    ecx = edi;
    LIFT_CALL(eax, 0x7dffefu);
    esi = eax;
    edx = rd32(m, esi);
    eax = rd32(m, edx + 0x1cu);
    ecx = esi;
    LIFT_CALL(eax, 0x7dfffau);
    wr32(m, esp + 0x10u, fst32(m));
    fpop(m);
    edx = rd32(m, esi);
    edx = rd32(m, edx + 0x18u);
    eax = (esp + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0x54u);
    esp -= 4; wr32(m, esp, ecx);
    eax = (esp + 0x28u);
    esp -= 4; wr32(m, esp, eax);
    ecx = esi;
    LIFT_CALL(edx, 0x7e0016u);
    eax = rd32(m, edi);
    edx = rd32(m, eax + 0x38u);
    ecx = edi;
    LIFT_CALL(edx, 0x7e001fu);
    x0 = movss_m(rd32(m, eax + 0x20u));
    x3 = movss_m(rd32(m, esp + 0x28u));
    x1 = movss_m(rd32(m, eax + 0x10u));
    x4 = movss_m(rd32(m, esp + 0x24u));
    x5 = movss_m(rd32(m, esp + 0x20u));
    x2 = movss_m(rd32(m, eax + 0x14u));
    x0 = mulss(x0, x3.l(0));
    x1 = mulss(x1, x4.l(0));
    x0 = addss(x0, x1.l(0));
    x1 = movss_m(rd32(m, eax));
    x1 = mulss(x1, x5.l(0));
    x0 = addss(x0, x1.l(0));
    x1 = movss_m(rd32(m, eax + 0x24u));
    x0 = addss(x0, rd32(m, eax + 0x30u));
    x1 = mulss(x1, x3.l(0));
    x2 = mulss(x2, x4.l(0));
    x1 = addss(x1, x2.l(0));
    x2 = movss_m(rd32(m, eax + 0x4u));
    x2 = mulss(x2, x5.l(0));
    x1 = addss(x1, x2.l(0));
    x2 = movss_m(rd32(m, eax + 0x28u));
    x1 = addss(x1, rd32(m, eax + 0x34u));
    x2 = mulss(x2, x3.l(0));
    x3 = movss_m(rd32(m, eax + 0x18u));
    x3 = mulss(x3, x4.l(0));
    x2 = addss(x2, x3.l(0));
    x3 = movss_m(rd32(m, eax + 0x8u));
    x3 = mulss(x3, x5.l(0));
    x2 = addss(x2, x3.l(0));
    x2 = addss(x2, rd32(m, eax + 0x38u));
    x3 = movss_m(rd32(m, esp + 0x14u));
    wr32(m, esp + 0x20u, x0.l(0));
    wr32(m, esp + 0x30u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x10u));
    eax = (ebx + 0x10b4u);
    esi = (esp + 0x30u);
    wr32(m, esp + 0x24u, x1.l(0));
    wr32(m, esp + 0x28u, x2.l(0));
    wr32(m, esp + 0x40u, edi);
    wr32(m, esp + 0x44u, x3.l(0));
    wr32(m, esp + 0x34u, x1.l(0));
    wr32(m, esp + 0x38u, x2.l(0));
    wr32(m, esp + 0x48u, x0.l(0));
    LIFT_CALL(0x7f9e70u, 0x7e00f7u);  // FUN_007f9e70
    fR_7e00f7 = (eax & eax);
    if ((fR_7e00f7 == 0)) goto L_7e0145;
    eax = rd32(m, edi);
    edx = rd32(m, eax + 0x5cu);
    ecx = edi;
    LIFT_CALL(edx, 0x7e0104u);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x10u)));
    esp = esp - 0x8u;
    wr32(m, esp + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x1cu)));
    wr32(m, esp, fst32(m));
    fpop(m);
    fR_7e0116 = (eax & eax);
    if ((fR_7e0116 == 0)) goto L_7e0135;
    eax = rd32(m, edi);
    edx = rd32(m, eax + 0x5cu);
    ecx = edi;
    LIFT_CALL(edx, 0x7e0123u);
    ecx = rd32(m, ebx + 0x10f0u);
    esp -= 4; wr32(m, esp, eax);
    edx = (esp + 0x2cu);
    LIFT_CALL(0x7da5a0u, 0x7e0133u);  // FUN_007da5a0
    goto L_7e0145;
L_7e0135:
    ecx = rd32(m, ebx + 0x10f0u);
    esp -= 4; wr32(m, esp, edi);
    edx = (esp + 0x2cu);
    LIFT_CALL(0x7da4d0u, 0x7e0145u);  // FUN_007da4d0
L_7e0145:
    eax = (ebp + 0xcu);
    esp -= 4; wr32(m, esp, eax);
    esi = (ebx + 0x103cu);
    LIFT_CALL(0x544cc0u, 0x7e0154u);  // FUN_00544cc0
    edx = rd32(m, edi);
    eax = rd32(m, edx + 0x64u);
    ecx = edi;
    LIFT_CALL(eax, 0x7e015du);
    edx = rd32(m, eax);
    ecx = eax;
    eax = rd32(m, edx + 0x38u);
    LIFT_CALL(eax, 0x7e0166u);
    ecx = rd32(m, esp + 0x18u);
    esi = rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */;
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(esi, 0x7e0173u);
    edx = rd32(m, esp + 0x1cu);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(esi, 0x7e017au);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}
