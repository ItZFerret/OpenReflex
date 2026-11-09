// Character / object animation ports (group "anim"), transliterated instruction by instruction from MXReflex.exe by the
// anim lifter (see native/game/notes/anim.md). Each function keeps the original's operations, order and memory traffic.
#include <stdexcept>
#include <utility>
#include "lift_rt.h"

// 006a5b30 Fn_006a5b30
//   original FUN_006a5b30; 1455 instructions per frame (plan); register inputs: none; returns with ret 8
REFLEX_PORT_FN(0x006a5b30, Fn_006a5b30, "anim", LIFT_OUTS) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x10u);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x10u)));
    esp -= 4; wr32(m, esp, esi);
    wr32(m, ebx + 0x10u, fst32(m));
    fpop(m);
    esp -= 4; wr32(m, esp, edi);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x14u)));
    ecx = (ebp + 0x70u);
    wr32(m, ebx + 0x14u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x18u)));
    wr32(m, ebx + 0x18u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x20u)));
    wr32(m, ebx + 0x20u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x24u)));
    wr32(m, ebx + 0x24u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x28u)));
    wr32(m, ebx + 0x28u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x30u)));
    wr32(m, ebx + 0x30u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x34u)));
    wr32(m, ebx + 0x34u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x38u)));
    wr32(m, ebx + 0x38u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x40u)));
    wr32(m, ebx + 0x40u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x44u)));
    wr32(m, ebx + 0x44u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x48u)));
    wr32(m, ebx + 0x48u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x50u)));
    wr32(m, ebx + 0x50u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x54u)));
    wr32(m, ebx + 0x54u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x58u)));
    wr32(m, ebx + 0x58u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x5cu)));
    wr32(m, ebx + 0x5cu, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x60u)));
    wr32(m, ebx + 0x60u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x64u)));
    wr32(m, ebx + 0x64u, fst32(m));
    fpop(m);
    eax = rd32(m, ebp + 0x68u);
    wr32(m, ebx + 0x68u, eax);
    eax = (ebx + 0x70u);
    LIFT_CALL(0x63ddd0u, 0x6a5bb9u);  // FUN_0063ddd0
    ecx = rd32(m, ebp + 0xd0u);
    wr32(m, ebx + 0xd0u, ecx);
    edx = rd32(m, ebp + 0xd4u);
    wr32(m, ebx + 0xd4u, edx);
    eax = rd32(m, ebp + 0xd8u);
    wr32(m, ebx + 0xd8u, eax);
    ecx = rd32(m, ebp + 0xdcu);
    wr32(m, ebx + 0xdcu, ecx);
    esi = (ebp + 0xe0u);
    edi = (ebx + 0xe0u);
    ecx = 0x10u;
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    esi = (ebp + 0x120u);
    edi = (ebx + 0x120u);
    ecx = 0x10u;
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    esi = (ebp + 0x160u);
    edi = (ebx + 0x160u);
    ecx = 0x10u;
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x1a0u)));
    wr32(m, ebx + 0x1a0u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x1a4u)));
    wr32(m, ebx + 0x1a4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x1a8u)));
    wr32(m, ebx + 0x1a8u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x1b0u)));
    wr32(m, ebx + 0x1b0u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x1b4u)));
    wr32(m, ebx + 0x1b4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x1b8u)));
    wr32(m, ebx + 0x1b8u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x1c0u)));
    wr32(m, ebx + 0x1c0u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x1c4u)));
    edi = rd32(m, esp); esp += 4;
    wr32(m, ebx + 0x1c4u, fst32(m));
    fpop(m);
    esi = rd32(m, esp); esp += 4;
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x1c8u)));
    eax = ebx;
    wr32(m, ebx + 0x1c8u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x1d0u)));
    wr32(m, ebx + 0x1d0u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x1d4u)));
    wr32(m, ebx + 0x1d4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ebp + 0x1d8u)));
    ebp = rd32(m, esp); esp += 4;
    wr32(m, ebx + 0x1d8u, fst32(m));
    fpop(m);
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 006aa750 ClipRecorder_ClearClips
//   original ClipRecorder_ClearClips; 0 instructions per frame (plan); register inputs: eax; returns with ret 0
REFLEX_PORT_FN(0x006aa750, ClipRecorder_ClearClips, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_6aa7a0 = 0;
    uint32_t fB_6aa7a0 = 0;
    eax = (eax | 0xffffffffu);
    ecx = 0xd323c8u;
    wr32(m, 0xa7f630u, eax);
    wr32(m, 0xa7f634u, eax);
    wr32(m, 0xa7f638u, eax);
    wr32(m, 0xa7f63cu, eax);
    wr32(m, 0xa7f640u, eax);
    wr32(m, 0xa7f644u, eax);
    wr32(m, 0xa7f648u, eax);
    wr32(m, 0xa7f64cu, eax);
    wr32(m, 0xa7f650u, eax);
    wr32(m, 0xa7f654u, eax);
    wr32(m, 0xa7f658u, eax);
    wr32(m, 0xa7f65cu, eax);
L_6aa794:
    wr32(m, ecx, 0x0u);
    ecx = ecx + 0x35d0u;
    fA_6aa7a0 = ecx;
    fB_6aa7a0 = 0xd5a988u;
    if (int32_t(sext(fA_6aa7a0, 4)) < int32_t(sext(fB_6aa7a0, 4))) goto L_6aa794;
    LIFT_RET(0);
}

// 006aa7b0 ClipRecorder_StartRecording
//   original ClipRecorder_StartRecording; 2 instructions per frame (plan); register inputs: edx; returns with ret 4
REFLEX_PORT_FN(0x006aa7b0, ClipRecorder_StartRecording, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_6aa837 = 0;
    uint32_t fB_6aa837 = 0;
    x1 = X{};
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x8u);
    x0 = movss_m(rd32(m, ebx + 0x294u));
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edx = (edx | 0xffffffffu);
    eax = 0xbf800000u;
    ecx = 0x96u;
    edi = 0xa7f128u;
    while (ecx) { wr32(m, edi, eax); edi += 4; ecx--; }
    eax = edx;
    ecx = 0x96u;
    edi = 0xa7f3d8u;
    wr32(m, 0xa942b8u, ebx);
    wr32(m, 0xa942a4u, x0.l(0));
    wr32(m, 0xa942b4u, 0x1u);
    wr32(m, 0x9c4070u, edx);
    wr32(m, 0x9c4074u, edx);
    wr32(m, 0xa942c0u, x1.l(0));
    while (ecx) { wr32(m, edi, eax); edi += 4; ecx--; }
    LIFT_CALL(0x6aa750u, 0x6aa815u);  // ClipRecorder_ClearClips
    x0 = movss_m(rd32(m, 0x9cd9e8u));
    esi = 0u;
    wr32(m, 0xa959a8u, esi);
    eax = 0xd2f160u;
L_6aa830:
    wr32(m, eax, x0.l(0));
    eax = eax - 0xffffff80u;
    fA_6aa837 = eax;
    fB_6aa837 = 0xd32360u;
    if (int32_t(sext(fA_6aa837, 4)) < int32_t(sext(fB_6aa837, 4))) goto L_6aa830;
    LIFT_CALL(0x6ab530u, 0x6aa843u);  // FUN_006ab530
    x0 = movss_m(rd32(m, ebx + 0x294u));
    wr32(m, 0xa7f128u, x0.l(0));
    wr32(m, 0xa942bcu, 0xa7f128u);
    LIFT_CALL(0x6abcb0u, 0x6aa862u);  // ClipRecorder_Snapshot
    edi = rd32(m, esp); esp += 4;
    wr32(m, 0xa7f384u, esi);
    wr32(m, 0xa7f388u, esi);
    wr32(m, 0xa7f38cu, esi);
    wr32(m, 0xa7f390u, esi);
    wr32(m, 0xa7f398u, esi);
    wr32(m, 0xa7f39cu, esi);
    wr32(m, 0xa7f3a0u, esi);
    wr32(m, 0xa7f3a4u, esi);
    esi = rd32(m, esp); esp += 4;
    wr32(m, 0xa7f3d8u, eax);
    wr32(m, 0xa942c4u, eax);
    wr32(m, 0xa942b0u, 0x1u);
    wr32(m, 0xa942a8u, x1.l(0));
    wr32(m, 0xa942acu, x1.l(0));
    wr32(m, 0xa942ccu, x1.l(0));
    wr32(m, 0xa942d0u, x1.l(0));
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 006aab70 ClipRecorder_Record
//   original ClipRecorder_Record; 90 instructions per frame (plan); register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x006aab70, ClipRecorder_Record, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    FCmp fC_6aab9a{};
    uint32_t fA_6aabd4 = 0;
    uint32_t fB_6aabd4 = 0;
    FCmp fC_6aac08{};
    uint32_t fA_6aac30 = 0;
    uint32_t fB_6aac30 = 0;
    FCmp fC_6aac75{};
    uint32_t fR_6aac8b = 0;
    esp -= 4; wr32(m, esp, ecx);
    x2 = movss_m(rd32(m, esp + 0x8u));
    x0 = movss_m(rd32(m, 0xa942a8u));
    x1 = movss_m(rd32(m, 0xa942acu));
    x0 = addss(x0, x2.l(0));
    x1 = addss(x1, x2.l(0));
    x2 = movss_m(rd32(m, 0x977950u) /* 0.06666667014360428 */);
    esp -= 4; wr32(m, esp, esi);
    esi = 0u;
    fC_6aab9a = fcmp32(x0.l(0), x2.l(0));
    wr32(m, 0xa942a8u, x0.l(0));
    wr32(m, 0xa942acu, x1.l(0));
    if (fc_b(fC_6aab9a)) goto L_6aac08;
    ecx = rd32(m, 0xa942b8u);
    esi = rd32(m, 0xa942b0u);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x294u)));
    eax = (esi * 4 + 0xa7f128u);
    wr32(m, eax, fst32(m));
    fpop(m);
    wr32(m, 0xa942bcu, eax);
    LIFT_CALL(0x6abcb0u, 0x6aabd4u);  // ClipRecorder_Snapshot
    fA_6aabd4 = esi;
    fB_6aabd4 = 0x95u;
    wr32(m, esi * 4 + 0xa7f3d8u, eax);
    wr32(m, 0xa942c4u, eax);
    if (int32_t(sext(fA_6aabd4, 4)) < int32_t(sext(fB_6aabd4, 4))) goto L_6aabf4;
    wr32(m, 0xa942b0u, 0x0u);
    goto L_6aabfb;
L_6aabf4:
    esi = (esi + 1u);
    wr32(m, 0xa942b0u, esi);
L_6aabfb:
    x0 = X{};
    wr32(m, 0xa942a8u, x0.l(0));
    goto L_6aac55;
L_6aac08:
    fC_6aac08 = fcmp32(x1.l(0), x2.l(0));
    if (fc_b(fC_6aac08)) goto L_6aac62;
    eax = rd32(m, 0xa942b0u);
    edx = rd32(m, 0xa942b8u);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x294u)));
    ecx = (eax * 4 + 0xa7f128u);
    wr32(m, ecx, fst32(m));
    fpop(m);
    wr32(m, 0xa942bcu, ecx);
    ecx = (ecx | 0xffffffffu);
    fA_6aac30 = eax;
    fB_6aac30 = 0x95u;
    wr32(m, 0xa942c4u, ecx);
    wr32(m, eax * 4 + 0xa7f3d8u, ecx);
    if (int32_t(sext(fA_6aac30, 4)) < int32_t(sext(fB_6aac30, 4))) goto L_6aac4c;
    wr32(m, 0xa942b0u, esi);
    goto L_6aac52;
L_6aac4c:
    eax = (eax + 1u);
    wr32(m, 0xa942b0u, eax);
L_6aac52:
    x0 = X{};
L_6aac55:
    esi = 0x1u;
    wr32(m, 0xa942acu, x0.l(0));
L_6aac62:
    LIFT_CALL(0x6ab5b0u, 0x6aac67u);  // ClipRecorder_PlaceClips
    x0 = movss_m(rd32(m, 0xa942c0u));
    x0 = addss(x0, rd32(m, esp + 0xcu));
    fC_6aac75 = fcmp32(x0.l(0), rd32(m, 0x92fd88u) /* 10.0 */);
    wr32(m, 0xa942c0u, x0.l(0));
    if ((fc_b(fC_6aac75) || fc_z(fC_6aac75))) goto L_6aac8b;
    LIFT_CALL(0x6ab560u, 0x6aac8bu);  // ClipRecorder_FillerClip
L_6aac8b:
    fR_6aac8b = (esi & esi);
    if ((fR_6aac8b == 0)) goto L_6aac94;
    LIFT_CALL(0x6ab220u, 0x6aac94u);  // ClipRecorder_CommitCandidate
L_6aac94:
    esi = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 006aadc0 ClipRecorder_FindSlot
//   original ClipRecorder_FindSlot; 0 instructions per frame (plan); register inputs: edx; returns with ret 0
REFLEX_PORT_FN(0x006aadc0, ClipRecorder_FindSlot, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_6aadd5 = 0;
    uint32_t fB_6aadd5 = 0;
    uint32_t fA_6aaddf = 0;
    uint32_t fB_6aaddf = 0;
    uint32_t fA_6aadf0 = 0;
    uint32_t fB_6aadf0 = 0;
    uint32_t fA_6aae04 = 0;
    uint32_t fB_6aae04 = 0;
    uint32_t fA_6aae08 = 0;
    uint32_t fB_6aae08 = 0;
    uint32_t fA_6aae13 = 0;
    uint32_t fB_6aae13 = 0;
    uint32_t fR_6aae2d = 0;
    uint32_t fR_6aae36 = 0;
    uint32_t fA_6aae47 = 0;
    uint32_t fB_6aae47 = 0;
    uint32_t fA_6aae56 = 0;
    uint32_t fB_6aae56 = 0;
    uint32_t fR_6aae5d = 0;
    esp = esp - 0x30u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x38u);
    esp -= 4; wr32(m, esp, esi);
    esi = 0u;
    edx = (edx | 0xffffffffu);
    ecx = 0u;
    eax = 0xd323c8u;
L_6aadd5:
    fA_6aadd5 = rd32(m, eax);
    fB_6aadd5 = esi;
    if (fA_6aadd5 == fB_6aadd5) goto L_6aade8;
    eax = eax + 0x35d0u;
    ecx = (ecx + 1u);
    fA_6aaddf = eax;
    fB_6aaddf = 0xd5a988u;
    if (int32_t(sext(fA_6aaddf, 4)) < int32_t(sext(fB_6aaddf, 4))) goto L_6aadd5;
    goto L_6aadfa;
L_6aade8:
    esi = ecx;
    esi = uint32_t(int32_t(esi) * int32_t(0x35d0u));
    fA_6aadf0 = esi;
    fB_6aadf0 = 0xd323c0u;
    esi = fA_6aadf0 + fB_6aadf0;
    edx = ecx;
    if (!(((fA_6aadf0 + fB_6aadf0)) == 0)) goto L_6aae2d;
L_6aadfa:
    ecx = 0u;
    eax = 0xd323c8u;
    esp -= 4; wr32(m, esp, edi);
L_6aae02:
    edi = rd32(m, eax);
    fA_6aae04 = ebx;
    fB_6aae04 = edi;
    if (int32_t(sext(fA_6aae04, 4)) <= int32_t(sext(fB_6aae04, 4))) goto L_6aae0d;
    fA_6aae08 = edi;
    fB_6aae08 = 0x8u;
    if (fA_6aae08 != fB_6aae08) goto L_6aae1c;
L_6aae0d:
    eax = eax + 0x35d0u;
    ecx = (ecx + 1u);
    fA_6aae13 = eax;
    fB_6aae13 = 0xd5a988u;
    if (int32_t(sext(fA_6aae13, 4)) < int32_t(sext(fB_6aae13, 4))) goto L_6aae02;
    goto L_6aae2c;
L_6aae1c:
    esi = ecx;
    esi = uint32_t(int32_t(esi) * int32_t(0x35d0u));
    esi = esi + 0xd323c0u;
    edx = ecx;
L_6aae2c:
    edi = rd32(m, esp); esp += 4;
L_6aae2d:
    fR_6aae2d = (edx & edx);
    if ((sgn(fR_6aae2d, 4) != false)) goto L_6aae36;
    LIFT_CALL(0x6abb30u, 0x6aae36u);  // ClipRecorder_OrderClip
L_6aae36:
    fR_6aae36 = (esi & esi);
    if (!(fR_6aae36 == 0)) goto L_6aaecb;
    edx = 0u;
    ecx = 0u;
    eax = 0xd323c8u;
L_6aae47:
    fA_6aae47 = ebx;
    fB_6aae47 = rd32(m, eax);
    if (fA_6aae47 != fB_6aae47) goto L_6aae50;
    wr32(m, esp + edx * 4 + 0x8u, ecx);
    edx = (edx + 1u);
L_6aae50:
    eax = eax + 0x35d0u;
    ecx = (ecx + 1u);
    fA_6aae56 = eax;
    fB_6aae56 = 0xd5a988u;
    if (int32_t(sext(fA_6aae56, 4)) < int32_t(sext(fB_6aae56, 4))) goto L_6aae47;
    fR_6aae5d = (edx & edx);
    if (!(fR_6aae5d == 0)) goto L_6aae69;
    esi = rd32(m, esp); esp += 4;
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x30u;
    LIFT_RET(0);
L_6aae69:
    eax = rd32(m, 0x9cb860u);
    eax = uint32_t(int32_t(eax) * int32_t(0x41c64e6du));
    eax = eax + 0x3039u;
    wr32(m, 0x9cb860u, eax);
    eax = (eax >> 16);
    eax = (eax & 0x7fffu);
    wr32(m, 0x9cb864u, (rd32(m, 0x9cb864u) + 1u));
    x0 = cvtsi2ss(x0, eax);
    x0 = cvtss2sd(x0, x0.l(0));
    x0 = mulsd(x0, rd64(m, 0x977708u) /* -3.0517578125e-05 */);
    x1 = cvtsi2ss(x1, edx);
    x0 = cvtsd2ss(x0, x0.lo);
    x0 = mulss(x0, x1.l(0));
    eax = cvttss2si(x0.l(0));
    eax = eax + eax;
    eax = eax + eax;
    ecx = (esp + 0x8u);
    ecx = ecx - eax;
    edx = rd32(m, ecx);
    esi = edx;
    esi = uint32_t(int32_t(esi) * int32_t(0x35d0u));
    esi = esi + 0xd323c0u;
    LIFT_CALL(0x6abb30u, 0x6aaecbu);  // ClipRecorder_OrderClip
L_6aaecb:
    eax = esi;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x30u;
    LIFT_RET(0);
}

// 006aaf20 ClipRecorder_SaveClip
//   original ClipRecorder_SaveClip; 18 instructions per frame (plan); register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x006aaf20, ClipRecorder_SaveClip, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_6aaf4a = 0;
    uint32_t fA_6aaf58 = 0;
    uint32_t fB_6aaf58 = 0;
    FCmp fC_6aaf79{};
    uint32_t fR_6aaf7e = 0;
    uint32_t fA_6aaf8a = 0;
    uint32_t fB_6aaf8a = 0;
    uint32_t fA_6aaf93 = 0;
    uint32_t fB_6aaf93 = 0;
    uint32_t fA_6aaf9b = 0;
    uint32_t fB_6aaf9b = 0;
    uint32_t fA_6aafaa = 0;
    uint32_t fB_6aafaa = 0;
    uint32_t fA_6aafb3 = 0;
    uint32_t fB_6aafb3 = 0;
    uint32_t fA_6aafbb = 0;
    uint32_t fB_6aafbb = 0;
    uint32_t fA_6aafc7 = 0;
    uint32_t fB_6aafc7 = 0;
    uint32_t fA_6aafd0 = 0;
    uint32_t fB_6aafd0 = 0;
    FCmp fC_6aafe7{};
    FCmp fC_6aaff3{};
    uint32_t fA_6aaffc = 0;
    uint32_t fB_6aaffc = 0;
    FCmp fC_6ab017{};
    uint32_t fA_6ab028 = 0;
    uint32_t fB_6ab028 = 0;
    uint32_t fA_6ab068 = 0;
    uint32_t fB_6ab068 = 0;
    FCmp fC_6ab091{};
    FCmp fC_6ab0a1{};
    uint32_t fA_6ab0bd = 0;
    uint32_t fB_6ab0bd = 0;
    eax = rd32(m, 0xa942b8u);
    edx = rd32(m, eax + 0x26cu);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    esi = (ebp + 0x10u);
    edi = (ebx + 0xcu);
    ecx = 0xe4u;
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    ecx = rd32(m, 0xa942b0u);
    fR_6aaf4a = (ecx & ecx);
    if (!(fR_6aaf4a == 0)) goto L_6aaf55;
    eax = 0x95u;
    goto L_6aaf60;
L_6aaf55:
    eax = (ecx - 0x1u);
    fA_6aaf58 = eax;
    fB_6aaf58 = ecx;
    if (fA_6aaf58 == fB_6aaf58) goto L_6ab102;
L_6aaf60:
    x0 = movss_m(rd32(m, ebp));
    goto L_6aaf70;
L_6aaf70:
    x1 = movss_m(rd32(m, eax * 4 + 0xa7f128u));
    fC_6aaf79 = fcmp32(x1.l(0), x0.l(0));
    if ((fc_b(fC_6aaf79) || fc_z(fC_6aaf79))) goto L_6aaf93;
    fR_6aaf7e = (eax & eax);
    if (!(fR_6aaf7e == 0)) goto L_6aaf89;
    eax = 0x95u;
    goto L_6aaf8a;
L_6aaf89:
    eax = (eax - 1u);
L_6aaf8a:
    fA_6aaf8a = eax;
    fB_6aaf8a = ecx;
    if (fA_6aaf8a != fB_6aaf8a) goto L_6aaf70;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_6aaf93:
    fA_6aaf93 = eax;
    fB_6aaf93 = ecx;
    if (fA_6aaf93 == fB_6aaf93) goto L_6ab102;
    fA_6aaf9b = eax;
    fB_6aaf9b = 0x95u;
    if (int32_t(sext(fA_6aaf9b, 4)) < int32_t(sext(fB_6aaf9b, 4))) goto L_6aafa6;
    eax = 0u;
    goto L_6aafa7;
L_6aafa6:
    eax = (eax + 1u);
L_6aafa7:
    esi = (esi | 0xffffffffu);
    fA_6aafaa = rd32(m, eax * 4 + 0xa7f3d8u);
    fB_6aafaa = esi;
    if (fA_6aafaa != fB_6aafaa) goto L_6aafd0;
L_6aafb3:
    fA_6aafb3 = eax;
    fB_6aafb3 = ecx;
    if (fA_6aafb3 == fB_6aafb3) goto L_6ab102;
    fA_6aafbb = eax;
    fB_6aafbb = 0x95u;
    if (int32_t(sext(fA_6aafbb, 4)) < int32_t(sext(fB_6aafbb, 4))) goto L_6aafc6;
    eax = 0u;
    goto L_6aafc7;
L_6aafc6:
    eax = (eax + 1u);
L_6aafc7:
    fA_6aafc7 = rd32(m, eax * 4 + 0xa7f3d8u);
    fB_6aafc7 = esi;
    if (fA_6aafc7 == fB_6aafc7) goto L_6aafb3;
L_6aafd0:
    fA_6aafd0 = eax;
    fB_6aafd0 = ecx;
    if (fA_6aafd0 == fB_6aafd0) goto L_6ab102;
    esi = (eax * 4);
    x1 = movss_m(rd32(m, esi + 0xa7f128u));
    fC_6aafe7 = fcmp32(x1.l(0), x0.l(0));
    ecx = esi;
    if (fc_b(fC_6aafe7)) goto L_6ab01c;
    x1 = movss_m(rd32(m, ebp + 0x4u));
L_6aaff3:
    fC_6aaff3 = fcmp32(x1.l(0), rd32(m, ecx + 0xa7f128u));
    if (fc_b(fC_6aaff3)) goto L_6ab01c;
    fA_6aaffc = eax;
    fB_6aaffc = 0x95u;
    if (int32_t(sext(fA_6aaffc, 4)) < int32_t(sext(fB_6aaffc, 4))) goto L_6ab007;
    eax = 0u;
    goto L_6ab008;
L_6ab007:
    eax = (eax + 1u);
L_6ab008:
    ecx = (eax * 4);
    x2 = movss_m(rd32(m, ecx + 0xa7f128u));
    fC_6ab017 = fcmp32(x2.l(0), x0.l(0));
    if (!fc_b(fC_6ab017)) goto L_6aaff3;
L_6ab01c:
    eax = rd32(m, esi + 0xa7f3d8u);
    ecx = rd32(m, 0xa942c4u);
    fA_6ab028 = eax;
    fB_6ab028 = ecx;
    if (int32_t(sext(fA_6ab028, 4)) >= int32_t(sext(fB_6ab028, 4))) goto L_6ab032;
    ecx = ecx - eax;
    ebp = ecx;
    goto L_6ab03c;
L_6ab032:
    ebp = rd32(m, 0xa942dcu);
    ebp = ebp - eax;
    ebp = ebp + ecx;
L_6ab03c:
    ecx = rd32(m, ebx + 0x39cu);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x6ac050u, 0x6ab049u);  // ClipRecorder_CopySamples
    x0 = movss_m(rd32(m, 0x9cd9e8u));
    edx = (ebx + 0x3b0u);
    esp = esp + 0x8u;
    eax = edx;
    ecx = 0x64u;
L_6ab061:
    wr32(m, eax, x0.l(0));
    eax = eax - 0xffffff80u;
    fA_6ab068 = ecx;
    fB_6ab068 = 0x1u;
    ecx = fA_6ab068 - fB_6ab068;
    if (fA_6ab068 != fB_6ab068) goto L_6ab061;
    esi = rd32(m, 0xa942c8u);
    edi = edx;
    ebx = (ecx + 0x64u);
    goto L_6ab080;
L_6ab080:
    ecx = rd32(m, esp + 0x14u);
    edx = esi;
    edx = (edx << 7);
    x0 = movss_m(rd32(m, edx + 0xd2f160u));
    fC_6ab091 = fcmp32(x0.l(0), rd32(m, ecx));
    eax = (edx + 0xd2f160u);
    if (fc_b(fC_6ab091)) goto L_6ab0b2;
    x1 = movss_m(rd32(m, ecx + 0x4u));
    fC_6ab0a1 = fcmp32(x1.l(0), x0.l(0));
    if (fc_b(fC_6ab0a1)) goto L_6ab0b2;
    LIFT_CALL(0x6ab110u, 0x6ab0abu);  // FUN_006ab110
    ecx = rd32(m, esp + 0x14u);
    edi = edi - 0xffffff80u;
L_6ab0b2:
    eax = (esi + 0x1u);
    edx = (eax >> 31) ? 0xffffffffu : 0;
    esi = 0x64u;
    { uint32_t d_ = esi; sdiv32(eax, edx, d_); }
    fA_6ab0bd = ebx;
    fB_6ab0bd = 0x1u;
    ebx = fA_6ab0bd - fB_6ab0bd;
    esi = edx;
    if (fA_6ab0bd != fB_6ab0bd) goto L_6ab080;
    edx = rd32(m, ecx + 0x8u);
    eax = rd32(m, esp + 0x18u);
    wr32(m, eax + 0x8u, edx);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx)));
    x0 = movss_m(rd32(m, ecx + 0x4u));
    wr32(m, eax + 0x35b0u, fst32(m));
    fpop(m);
    edx = rd32(m, 0xa942b8u);
    wr32(m, eax + 0x35b4u, x0.l(0));
    x0 = X{};
    wr32(m, eax + 0x4u, edx);
    ecx = rd32(m, ecx + 0xcu);
    wr32(m, eax, ecx);
    wr32(m, eax + 0x3a0u, ebp);
    wr32(m, 0xa942c0u, x0.l(0));
L_6ab102:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 006ab110 Fn_006ab110
//   original FUN_006ab110; 1 instructions per frame (plan); register inputs: eax edi; returns with ret 0
REFLEX_PORT_FN(0x006ab110, Fn_006ab110, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_6ab14d = 0;
    uint32_t fB_6ab14d = 0;
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    fpush(m, reflex_f80::from_f32(rd32(m, esi)));
    wr32(m, edi, fst32(m));
    fpop(m);
    eax = rd32(m, esi + 0x4u);
    wr32(m, edi + 0x4u, eax);
    ecx = rd32(m, esi + 0x8u);
    wr32(m, edi + 0x8u, ecx);
    ecx = (esi + 0x10u);
    eax = (edi + 0x10u);
    LIFT_CALL(0x6ab160u, 0x6ab12eu);  // FUN_006ab160
    edx = rd32(m, esi + 0x60u);
    eax = (edi + 0x60u);
    wr32(m, eax, edx);
    ecx = rd32(m, esi + 0x60u);
    wr32(m, eax, ecx);
    edx = rd32(m, esi + 0x60u);
    wr32(m, eax, edx);
    esi = esi - edi;
    ecx = 0x15u;
L_6ab147:
    edx = (edx & 0xffffff00u) | ((rd8(m, esi + eax)) & 0xffu);
    wr8(m, eax, (edx & 0xffu));
    eax = (eax + 1u);
    fA_6ab14d = ecx;
    fB_6ab14d = 0x1u;
    ecx = fA_6ab14d - fB_6ab14d;
    if (fA_6ab14d != fB_6ab14d) goto L_6ab147;
    eax = edi;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 006ab160 Fn_006ab160
//   original FUN_006ab160; 0 instructions per frame (plan); register inputs: ecx eax; returns with ret 0
REFLEX_PORT_FN(0x006ab160, Fn_006ab160, "anim", LIFT_OUTS) {
    LIFT_ENTRY();

    edx = rd32(m, ecx);
    wr32(m, eax, edx);
    edx = rd32(m, ecx + 0x4u);
    wr32(m, eax + 0x4u, edx);
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
    edx = rd32(m, ecx + 0x40u);
    wr32(m, eax + 0x40u, edx);
    edx = rd32(m, ecx + 0x44u);
    wr32(m, eax + 0x44u, edx);
    ecx = rd32(m, ecx + 0x48u);
    wr32(m, eax + 0x48u, ecx);
    LIFT_RET(0);
}

// 006ab1c0 Fn_006ab1c0
//   original FUN_006ab1c0; 0 instructions per frame (plan); register inputs: eax; returns with ret 0
REFLEX_PORT_FN(0x006ab1c0, Fn_006ab1c0, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_6ab1d1 = 0;
    uint32_t fB_6ab1d1 = 0;
    uint32_t fA_6ab1ee = 0;
    uint32_t fB_6ab1ee = 0;
    uint32_t fA_6ab1f7 = 0;
    uint32_t fB_6ab1f7 = 0;
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, 0xa942b8u);
    esi = eax;
    ecx = edi;
    LIFT_CALL(0x55a050u, 0x6ab1d1u);  // FUN_0055a050
    fA_6ab1d1 = rd32(m, esi + 0x908u);
    fB_6ab1d1 = 0x0u;
    if (fA_6ab1d1 == fB_6ab1d1) goto L_6ab1e5;
    ecx = rd32(m, edi + 0x27cu);
    eax = rd32(m, ecx + eax * 4);
    goto L_6ab1ee;
L_6ab1e5:
    edx = rd32(m, edi + 0x268u);
    eax = rd32(m, edx + eax * 4);
L_6ab1ee:
    fA_6ab1ee = rd32(m, 0xa959a8u);
    fB_6ab1ee = 0x0u;
    if (fA_6ab1ee == fB_6ab1ee) goto L_6ab217;
    fA_6ab1f7 = eax;
    fB_6ab1f7 = rd32(m, 0xa959acu);
    if (fA_6ab1f7 != fB_6ab1f7) goto L_6ab217;
    x0 = movss_m(rd32(m, edi + 0x294u));
    x0 = subss(x0, rd32(m, 0x92ff84u) /* 0.25 */);
    wr32(m, 0xa959a4u, x0.l(0));
L_6ab217:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 006ab220 ClipRecorder_CommitCandidate
//   original ClipRecorder_CommitCandidate; 7 instructions per frame (plan); register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x006ab220, ClipRecorder_CommitCandidate, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_6ab227 = 0;
    uint32_t fR_6ab230 = 0;
    FCmp fC_6ab238{};
    uint32_t fA_6ab241 = 0;
    uint32_t fB_6ab241 = 0;
    uint32_t fR_6ab257 = 0;
    uint32_t fR_6ab264 = 0;
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, 0xa959a8u);
    fR_6ab227 = (esi & esi);
    if ((fR_6ab227 == 0)) goto L_6ab28a;
    eax = rd32(m, 0xa942bcu);
    fR_6ab230 = (eax & eax);
    if ((fR_6ab230 == 0)) goto L_6ab28a;
    x0 = movss_m(rd32(m, eax));
    fC_6ab238 = fcmp32(x0.l(0), rd32(m, 0xa959a4u));
    if (fc_b(fC_6ab238)) goto L_6ab28a;
    fA_6ab241 = rd32(m, 0xa942c4u);
    fB_6ab241 = 0xffffffffu;
    if (fA_6ab241 == fB_6ab241) goto L_6ab28a;
    x0 = movss_m(rd32(m, 0xa959a0u));
    LIFT_CALL(0x6abae0u, 0x6ab257u);  // FUN_006abae0
    fR_6ab257 = (eax & eax);
    if (!(fR_6ab257 == 0)) goto L_6ab276;
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x6aadc0u, 0x6ab261u);  // ClipRecorder_FindSlot
    esp = esp + 0x4u;
    fR_6ab264 = (eax & eax);
    if ((fR_6ab264 == 0)) goto L_6ab276;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, 0xa959a0u);
    LIFT_CALL(0x6aaf20u, 0x6ab273u);  // ClipRecorder_SaveClip
    esp = esp + 0x8u;
L_6ab276:
    wr32(m, 0xa959acu, 0x0u);
    wr32(m, 0xa959a8u, 0x0u);
L_6ab28a:
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 006ab530 Fn_006ab530
//   original FUN_006ab530; 0 instructions per frame (plan); register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x006ab530, Fn_006ab530, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_6ab53e = 0;
    uint32_t fA_6ab557 = 0;
    uint32_t fB_6ab557 = 0;
    edx = rd32(m, 0xa942b8u);
    ecx = rd32(m, edx + 0x26cu);
    eax = 0u;
    fR_6ab53e = (ecx & ecx);
    if (((fR_6ab53e == 0) || sgn(fR_6ab53e, 4) != false)) goto L_6ab55c;
    esp -= 4; wr32(m, esp, esi);
L_6ab543:
    esi = rd32(m, edx + 0x268u);
    esi = rd32(m, esi + eax * 4);
    esi = rd32(m, esi + 0x78u);
    wr32(m, eax * 4 + 0xa7f3a8u, esi);
    eax = (eax + 1u);
    fA_6ab557 = eax;
    fB_6ab557 = ecx;
    if (int32_t(sext(fA_6ab557, 4)) < int32_t(sext(fB_6ab557, 4))) goto L_6ab543;
    esi = rd32(m, esp); esp += 4;
L_6ab55c:
    LIFT_RET(0);
}

// 006ab5b0 ClipRecorder_PlaceClips
//   original ClipRecorder_PlaceClips; 272 instructions per frame (plan); register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x006ab5b0, ClipRecorder_PlaceClips, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_6ab5b0 = 0;
    uint32_t fB_6ab5b0 = 0;
    uint32_t fA_6ab5bc = 0;
    uint32_t fB_6ab5bc = 0;
    FCmp fC_6ab5d7{};
    uint32_t fR_6ab5ec = 0;
    uint32_t fA_6ab600 = 0;
    uint32_t fB_6ab600 = 0;
    uint32_t fA_6ab60a = 0;
    uint32_t fB_6ab60a = 0;
    uint32_t fA_6ab614 = 0;
    uint32_t fB_6ab614 = 0;
    uint32_t fA_6ab62a = 0;
    uint32_t fB_6ab62a = 0;
    uint32_t fA_6ab62e = 0;
    uint32_t fB_6ab62e = 0;
    uint32_t fA_6ab6a0 = 0;
    uint32_t fB_6ab6a0 = 0;
    uint32_t fR_6ab6ae = 0;
    uint32_t fA_6ab6c6 = 0;
    uint32_t fB_6ab6c6 = 0;
    fA_6ab5b0 = rd32(m, 0xa7f388u);
    fB_6ab5b0 = 0x0u;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    if (fA_6ab5b0 != fB_6ab5b0) goto L_6ab5c9;
    fA_6ab5bc = rd32(m, 0xa7f38cu);
    fB_6ab5bc = 0x0u;
    if (fA_6ab5bc == fB_6ab5bc) goto L_6ab6ca;
L_6ab5c9:
    edx = rd32(m, 0xa942b8u);
    x0 = movss_m(rd32(m, edx + 0x294u));
    fC_6ab5d7 = fcmp32(x0.l(0), rd32(m, 0x930004u) /* 7.0 */);
    ebx = rd32(m, edx + 0x26cu);
    if ((fc_b(fC_6ab5d7) || fc_z(fC_6ab5d7))) goto L_6ab6ac;
    edi = 0u;
    fR_6ab5ec = (ebx & ebx);
    if (((fR_6ab5ec == 0) || sgn(fR_6ab5ec, 4) != false)) goto L_6ab6ca;
L_6ab5f4:
    eax = rd32(m, edx + 0x268u);
    esi = rd32(m, eax + edi * 4);
    eax = rd32(m, esi + 0x74u);
    fA_6ab600 = rd32(m, eax + 0x4u);
    fB_6ab600 = 0x0u;
    if (fA_6ab600 == fB_6ab600) goto L_6ab69f;
    fA_6ab60a = rd32(m, eax + 0x10u);
    fB_6ab60a = 0x0u;
    if (fA_6ab60a != fB_6ab60a) goto L_6ab69f;
    fA_6ab614 = rd32(m, 0xa959acu);
    fB_6ab614 = esi;
    if (fA_6ab614 == fB_6ab614) goto L_6ab69f;
    eax = rd32(m, edi * 4 + 0xa7f3a8u);
    ecx = rd32(m, esi + 0x78u);
    fA_6ab62a = ecx;
    fB_6ab62a = eax;
    if (fA_6ab62a >= fB_6ab62a) goto L_6ab681;
    fA_6ab62e = rd32(m, 0xd5a9a4u);
    fB_6ab62e = 0x0u;
    if (fA_6ab62e == fB_6ab62e) goto L_6ab675;
    x0 = movss_m(rd32(m, edx + 0x294u));
    x1 = x0;
    x1 = addss(x1, rd32(m, 0x9c40c4u));
    x0 = subss(x0, rd32(m, 0x9c40c0u));
    esp = esp - 0x8u;
    wr32(m, esp + 0x4u, x1.l(0));
    edx = esi;
    ecx = 0x3u;
    wr32(m, esp, x0.l(0));
    LIFT_CALL(0x6aacf0u, 0x6ab66cu);  // ClipRecorder_Candidate
    edx = rd32(m, 0xa942b8u);
    esp = esp + 0x8u;
L_6ab675:
    ecx = rd32(m, esi + 0x78u);
    wr32(m, edi * 4 + 0xa7f3a8u, ecx);
    goto L_6ab69f;
L_6ab681:
    if (fA_6ab62a <= fB_6ab62a) goto L_6ab69f;
    edx = esi;
    ecx = 0x2u;
    LIFT_CALL(0x6aaca0u, 0x6ab68fu);  // ClipRecorder_CandidateOfType
    edx = rd32(m, esi + 0x78u);
    wr32(m, edi * 4 + 0xa7f3a8u, edx);
    edx = rd32(m, 0xa942b8u);
L_6ab69f:
    edi = (edi + 1u);
    fA_6ab6a0 = edi;
    fB_6ab6a0 = ebx;
    if (int32_t(sext(fA_6ab6a0, 4)) < int32_t(sext(fB_6ab6a0, 4))) goto L_6ab5f4;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_6ab6ac:
    eax = 0u;
    fR_6ab6ae = (ebx & ebx);
    if (((fR_6ab6ae == 0) || sgn(fR_6ab6ae, 4) != false)) goto L_6ab6ca;
L_6ab6b2:
    ecx = rd32(m, edx + 0x268u);
    ecx = rd32(m, ecx + eax * 4);
    ecx = rd32(m, ecx + 0x78u);
    wr32(m, eax * 4 + 0xa7f3a8u, ecx);
    eax = (eax + 1u);
    fA_6ab6c6 = eax;
    fB_6ab6c6 = ebx;
    if (int32_t(sext(fA_6ab6c6, 4)) < int32_t(sext(fB_6ab6c6, 4))) goto L_6ab6b2;
L_6ab6ca:
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 006ab800 ClipRecorder_LogReset
//   original ClipRecorder_LogReset; 0 instructions per frame (plan); register inputs: eax; returns with ret 4
REFLEX_PORT_FN(0x006ab800, ClipRecorder_LogReset, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_6ab805 = 0;
    uint32_t fB_6ab805 = 0;
    uint32_t fA_6ab80c = 0;
    uint32_t fB_6ab80c = 0;
    uint32_t fA_6ab84b = 0;
    uint32_t fB_6ab84b = 0;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    fA_6ab805 = rd32(m, esi + 0x4u);
    fB_6ab805 = 0x0u;
    esp -= 4; wr32(m, esp, edi);
    if (fA_6ab805 == fB_6ab805) goto L_6ab817;
    fA_6ab80c = rd32(m, esi + 0x10u);
    fB_6ab80c = 0x0u;
    if (fA_6ab80c != fB_6ab80c) goto L_6ab817;
    LIFT_CALL(0x6ab1c0u, 0x6ab817u);  // FUN_006ab1c0
L_6ab817:
    ebp = rd32(m, 0xa942c8u);
    edi = ebp;
    edi = (edi << 7);
    wr32(m, edi + 0xd2f164u, 0x5u);
    eax = rd32(m, esi + 0x94u);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x294u)));
    wr32(m, edi + 0xd2f160u, fst32(m));
    fpop(m);
    ebx = rd32(m, esi + 0x94u);
    ecx = ebx;
    LIFT_CALL(0x55a050u, 0x6ab84bu);  // FUN_0055a050
    fA_6ab84b = rd32(m, esi + 0x908u);
    fB_6ab84b = 0x0u;
    if (fA_6ab84b == fB_6ab84b) goto L_6ab85f;
    ecx = rd32(m, ebx + 0x27cu);
    eax = rd32(m, ecx + eax * 4);
    goto L_6ab868;
L_6ab85f:
    edx = rd32(m, ebx + 0x268u);
    eax = rd32(m, edx + eax * 4);
L_6ab868:
    ecx = rd32(m, esp + 0x14u);
    wr32(m, edi + 0xd2f168u, eax);
    eax = (edi + 0xd2f170u);
    LIFT_CALL(0x6ab160u, 0x6ab87du);  // FUN_006ab160
    eax = (ebp + 0x1u);
    edx = (eax >> 31) ? 0xffffffffu : 0;
    ecx = 0x64u;
    { uint32_t d_ = ecx; sdiv32(eax, edx, d_); }
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    wr32(m, 0xa942c8u, edx);
    LIFT_RET(4);
}

// 006aba80 ClipRecorder_LogEffect
//   original ClipRecorder_LogEffect; 0 instructions per frame (plan); register inputs: edx; returns with ret 0
REFLEX_PORT_FN(0x006aba80, ClipRecorder_LogEffect, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_6aba80 = 0;
    uint32_t fB_6aba80 = 0;
    fA_6aba80 = rd32(m, 0xa942b4u);
    fB_6aba80 = 0x1u;
    if (fA_6aba80 != fB_6aba80) goto L_6abade;
    eax = rd32(m, 0xa942c8u);
    ecx = rd32(m, 0xa942b8u);
    esp -= 4; wr32(m, esp, esi);
    eax = (eax << 7);
    esp -= 4; wr32(m, esp, edi);
    wr32(m, eax + 0xd2f164u, 0x8u);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx + 0x294u)));
    esp -= 4; wr32(m, esp, edx);
    wr32(m, eax + 0xd2f160u, fst32(m));
    fpop(m);
    esp -= 4; wr32(m, esp, 0x9419e4u);
    edi = (eax + 0xd2f1c0u);
    esi = 0x15u;
    LIFT_CALL(0x78fd60u, 0x6abac5u);  // FUN_0078fd60
    eax = rd32(m, 0xa942c8u);
    eax = (eax + 1u);
    esp = esp + 0x8u;
    edx = (eax >> 31) ? 0xffffffffu : 0;
    ecx = 0x64u;
    { uint32_t d_ = ecx; sdiv32(eax, edx, d_); }
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    wr32(m, 0xa942c8u, edx);
L_6abade:
    LIFT_RET(0);
}

// 006abae0 Fn_006abae0
//   original FUN_006abae0; 0 instructions per frame (plan); register inputs: xmm0; returns with ret 0
REFLEX_PORT_FN(0x006abae0, Fn_006abae0, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_6abaf0 = 0;
    uint32_t fB_6abaf0 = 0;
    FCmp fC_6abaf8{};
    FCmp fC_6abb02{};
    uint32_t fA_6abb0e = 0;
    uint32_t fB_6abb0e = 0;
    eax = 0u;
    edx = 0u;
    ecx = 0xd35970u;
L_6abaf0:
    fA_6abaf0 = rd32(m, ecx - 0x35a8u);
    fB_6abaf0 = eax;
    if (fA_6abaf0 == fB_6abaf0) goto L_6abb07;
    fC_6abaf8 = fcmp32(x0.l(0), rd32(m, ecx));
    if (fc_b(fC_6abaf8)) goto L_6abb07;
    x1 = movss_m(rd32(m, ecx + 0x4u));
    fC_6abb02 = fcmp32(x1.l(0), x0.l(0));
    if (!fc_b(fC_6abb02)) goto L_6abb17;
L_6abb07:
    ecx = ecx + 0x35d0u;
    edx = (edx + 1u);
    fA_6abb0e = ecx;
    fB_6abb0e = 0xd5df30u;
    if (int32_t(sext(fA_6abb0e, 4)) < int32_t(sext(fB_6abb0e, 4))) goto L_6abaf0;
    LIFT_RET(0);
L_6abb17:
    edx = uint32_t(int32_t(edx) * int32_t(0x35d0u));
    edx = edx + 0xd323c0u;
    eax = edx;
    LIFT_RET(0);
}

// 006abb30 ClipRecorder_OrderClip
//   original ClipRecorder_OrderClip; 0 instructions per frame (plan); register inputs: edx; returns with ret 0
REFLEX_PORT_FN(0x006abb30, ClipRecorder_OrderClip, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_6abb30 = 0;
    uint32_t fB_6abb30 = 0;
    uint32_t fA_6abb44 = 0;
    uint32_t fB_6abb44 = 0;
    uint32_t fA_6abb4d = 0;
    uint32_t fB_6abb4d = 0;
    uint32_t fA_6abb56 = 0;
    uint32_t fB_6abb56 = 0;
    uint32_t fA_6abb5f = 0;
    uint32_t fB_6abb5f = 0;
    uint32_t fA_6abb68 = 0;
    uint32_t fB_6abb68 = 0;
    uint32_t fA_6abb71 = 0;
    uint32_t fB_6abb71 = 0;
    uint32_t fA_6abb7d = 0;
    uint32_t fB_6abb7d = 0;
    uint32_t fA_6abb99 = 0;
    uint32_t fB_6abb99 = 0;
    fA_6abb30 = rd32(m, edx * 4 + 0xa7f630u);
    fB_6abb30 = 0xffffffffu;
    if (fA_6abb30 != fB_6abb30) goto L_6abb42;
    wr32(m, edx * 4 + 0xa7f630u, edx);
    LIFT_RET(0);
L_6abb42:
    eax = 0u;
L_6abb44:
    fA_6abb44 = rd32(m, eax * 4 + 0xa7f630u);
    fB_6abb44 = edx;
    if (fA_6abb44 == fB_6abb44) goto L_6abb99;
    fA_6abb4d = rd32(m, eax * 4 + 0xa7f634u);
    fB_6abb4d = edx;
    if (fA_6abb4d == fB_6abb4d) goto L_6abb84;
    fA_6abb56 = rd32(m, eax * 4 + 0xa7f638u);
    fB_6abb56 = edx;
    if (fA_6abb56 == fB_6abb56) goto L_6abb87;
    fA_6abb5f = rd32(m, eax * 4 + 0xa7f63cu);
    fB_6abb5f = edx;
    if (fA_6abb5f == fB_6abb5f) goto L_6abb8c;
    fA_6abb68 = rd32(m, eax * 4 + 0xa7f640u);
    fB_6abb68 = edx;
    if (fA_6abb68 == fB_6abb68) goto L_6abb91;
    fA_6abb71 = rd32(m, eax * 4 + 0xa7f644u);
    fB_6abb71 = edx;
    if (fA_6abb71 == fB_6abb71) goto L_6abb96;
    eax = eax + 0x6u;
    fA_6abb7d = eax;
    fB_6abb7d = 0xcu;
    if (int32_t(sext(fA_6abb7d, 4)) < int32_t(sext(fB_6abb7d, 4))) goto L_6abb44;
    goto L_6abb99;
L_6abb84:
    eax = (eax + 1u);
    goto L_6abb99;
L_6abb87:
    eax = eax + 0x2u;
    goto L_6abb99;
L_6abb8c:
    eax = eax + 0x3u;
    goto L_6abb99;
L_6abb91:
    eax = eax + 0x4u;
    goto L_6abb99;
L_6abb96:
    eax = eax + 0x5u;
L_6abb99:
    fA_6abb99 = eax;
    fB_6abb99 = 0xbu;
    if (int32_t(sext(fA_6abb99, 4)) >= int32_t(sext(fB_6abb99, 4))) goto L_6abbb9;
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    ecx = 0xbu;
    edi = (eax * 4 + 0xa7f630u);
    esi = (eax * 4 + 0xa7f634u);
    ecx = ecx - eax;
    while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
L_6abbb9:
    wr32(m, 0xa7f65cu, edx);
    LIFT_RET(0);
}

// 006abcb0 ClipRecorder_Snapshot
//   original ClipRecorder_Snapshot; 245 instructions per frame (plan); register inputs: none; returns with ret 0
REFLEX_PORT_FN(0x006abcb0, ClipRecorder_Snapshot, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_6abcdc = 0;
    uint32_t fB_6abcdc = 0;
    uint32_t fR_6abd78 = 0;
    uint32_t fR_6abe0c = 0;
    uint32_t fR_6abea0 = 0;
    uint32_t fR_6abf34 = 0;
    uint32_t fA_6abf57 = 0;
    uint32_t fB_6abf57 = 0;
    uint32_t fA_6abf65 = 0;
    uint32_t fB_6abf65 = 0;
    uint32_t fR_6abff8 = 0;
    uint32_t fA_6ac015 = 0;
    uint32_t fB_6ac015 = 0;
    uint32_t fA_6ac029 = 0;
    uint32_t fB_6ac029 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x14u;
    edx = rd32(m, 0xa942e0u);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, 0xa942b8u);
    eax = rd32(m, edi + 0x26cu);
    ecx = (edx + edx * 4);
    ecx = (ecx << 4);
    ecx = ecx + rd32(m, 0xa7f124u);
    esi = 0u;
    fA_6abcdc = eax;
    fB_6abcdc = 0x4u;
    wr32(m, esp + 0x1cu, edx);
    wr32(m, esp + 0x18u, eax);
    if (int32_t(sext(fA_6abcdc, 4)) < int32_t(sext(fB_6abcdc, 4))) goto L_6abf65;
    ebx = rd32(m, 0xa942dcu);
    ebx = (ebx + ebx * 4);
    ebx = (ebx << 4);
L_6abd00:
    eax = rd32(m, edi + 0x268u);
    edx = rd32(m, eax + esi * 4);
    eax = rd32(m, edx + 0x74u);
    edx = rd32(m, eax + 0x1acu);
    x0 = ldx(m, edx + 0x12c0u);
    stx(m, ecx, x0);
    x0 = ldx(m, edx + 0x12d0u);
    stx(m, ecx + 0x10u, x0);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12e0u)));
    wr32(m, ecx + 0x20u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12e4u)));
    wr32(m, ecx + 0x24u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12e8u)));
    wr32(m, ecx + 0x28u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12ecu)));
    wr32(m, ecx + 0x2cu, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12f0u)));
    wr32(m, ecx + 0x30u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12f4u)));
    wr32(m, ecx + 0x34u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12f8u)));
    wr32(m, ecx + 0x38u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12fcu)));
    wr32(m, ecx + 0x3cu, fst32(m));
    fpop(m);
    eax = rd32(m, edx + 0x1300u);
    wr32(m, ecx + 0x40u, eax);
    fR_6abd78 = (eax & 0x40000u);
    if ((fR_6abd78 == 0)) goto L_6abd8f;
    edx = rd32(m, edx + 0x234u);
    wr32(m, edx + 0x2e98u, 0x0u);
L_6abd8f:
    eax = rd32(m, edi + 0x268u);
    edx = rd32(m, eax + esi * 4 + 0x4u);
    eax = rd32(m, edx + 0x74u);
    edx = rd32(m, eax + 0x1acu);
    x0 = ldx(m, edx + 0x12c0u);
    stx(m, ecx + ebx, x0);
    x0 = ldx(m, edx + 0x12d0u);
    stx(m, ecx + ebx + 0x10u, x0);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12e0u)));
    ecx = ecx + ebx;
    wr32(m, ecx + 0x20u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12e4u)));
    wr32(m, ecx + 0x24u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12e8u)));
    wr32(m, ecx + 0x28u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12ecu)));
    wr32(m, ecx + 0x2cu, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12f0u)));
    wr32(m, ecx + 0x30u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12f4u)));
    wr32(m, ecx + 0x34u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12f8u)));
    wr32(m, ecx + 0x38u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12fcu)));
    wr32(m, ecx + 0x3cu, fst32(m));
    fpop(m);
    eax = rd32(m, edx + 0x1300u);
    wr32(m, ecx + 0x40u, eax);
    fR_6abe0c = (eax & 0x40000u);
    if ((fR_6abe0c == 0)) goto L_6abe23;
    edx = rd32(m, edx + 0x234u);
    wr32(m, edx + 0x2e98u, 0x0u);
L_6abe23:
    eax = rd32(m, edi + 0x268u);
    edx = rd32(m, eax + esi * 4 + 0x8u);
    eax = rd32(m, edx + 0x74u);
    edx = rd32(m, eax + 0x1acu);
    x0 = ldx(m, edx + 0x12c0u);
    stx(m, ecx + ebx, x0);
    x0 = ldx(m, edx + 0x12d0u);
    stx(m, ecx + ebx + 0x10u, x0);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12e0u)));
    ecx = ecx + ebx;
    wr32(m, ecx + 0x20u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12e4u)));
    wr32(m, ecx + 0x24u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12e8u)));
    wr32(m, ecx + 0x28u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12ecu)));
    wr32(m, ecx + 0x2cu, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12f0u)));
    wr32(m, ecx + 0x30u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12f4u)));
    wr32(m, ecx + 0x34u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12f8u)));
    wr32(m, ecx + 0x38u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12fcu)));
    wr32(m, ecx + 0x3cu, fst32(m));
    fpop(m);
    eax = rd32(m, edx + 0x1300u);
    wr32(m, ecx + 0x40u, eax);
    fR_6abea0 = (eax & 0x40000u);
    if ((fR_6abea0 == 0)) goto L_6abeb7;
    edx = rd32(m, edx + 0x234u);
    wr32(m, edx + 0x2e98u, 0x0u);
L_6abeb7:
    eax = rd32(m, edi + 0x268u);
    edx = rd32(m, eax + esi * 4 + 0xcu);
    eax = rd32(m, edx + 0x74u);
    edx = rd32(m, eax + 0x1acu);
    x0 = ldx(m, edx + 0x12c0u);
    stx(m, ecx + ebx, x0);
    x0 = ldx(m, edx + 0x12d0u);
    stx(m, ecx + ebx + 0x10u, x0);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12e0u)));
    ecx = ecx + ebx;
    wr32(m, ecx + 0x20u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12e4u)));
    wr32(m, ecx + 0x24u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12e8u)));
    wr32(m, ecx + 0x28u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12ecu)));
    wr32(m, ecx + 0x2cu, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12f0u)));
    wr32(m, ecx + 0x30u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12f4u)));
    wr32(m, ecx + 0x34u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12f8u)));
    wr32(m, ecx + 0x38u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12fcu)));
    wr32(m, ecx + 0x3cu, fst32(m));
    fpop(m);
    eax = rd32(m, edx + 0x1300u);
    wr32(m, ecx + 0x40u, eax);
    fR_6abf34 = (eax & 0x40000u);
    if ((fR_6abf34 == 0)) goto L_6abf4b;
    edx = rd32(m, edx + 0x234u);
    wr32(m, edx + 0x2e98u, 0x0u);
L_6abf4b:
    eax = rd32(m, esp + 0x18u);
    esi = esi + 0x4u;
    edx = (eax - 0x3u);
    ecx = ecx + ebx;
    fA_6abf57 = esi;
    fB_6abf57 = edx;
    if (int32_t(sext(fA_6abf57, 4)) < int32_t(sext(fB_6abf57, 4))) goto L_6abd00;
    edx = rd32(m, 0xa942e0u);
L_6abf65:
    fA_6abf65 = esi;
    fB_6abf65 = eax;
    if (int32_t(sext(fA_6abf65, 4)) >= int32_t(sext(fB_6abf65, 4))) goto L_6ac023;
    edx = rd32(m, 0xa942dcu);
    edx = (edx + edx * 4);
    edx = (edx << 4);
    wr32(m, esp + 0x18u, edx);
L_6abf80:
    edx = rd32(m, edi + 0x268u);
    edx = rd32(m, edx + esi * 4);
    edx = rd32(m, edx + 0x74u);
    edx = rd32(m, edx + 0x1acu);
    x0 = ldx(m, edx + 0x12c0u);
    stx(m, ecx, x0);
    x0 = ldx(m, edx + 0x12d0u);
    stx(m, ecx + 0x10u, x0);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12e0u)));
    wr32(m, ecx + 0x20u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12e4u)));
    wr32(m, ecx + 0x24u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12e8u)));
    wr32(m, ecx + 0x28u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12ecu)));
    wr32(m, ecx + 0x2cu, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12f0u)));
    wr32(m, ecx + 0x30u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12f4u)));
    wr32(m, ecx + 0x34u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12f8u)));
    wr32(m, ecx + 0x38u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x12fcu)));
    wr32(m, ecx + 0x3cu, fst32(m));
    fpop(m);
    ebx = rd32(m, edx + 0x1300u);
    wr32(m, ecx + 0x40u, ebx);
    fR_6abff8 = (ebx & 0x40000u);
    if ((fR_6abff8 == 0)) goto L_6ac010;
    edx = rd32(m, edx + 0x234u);
    wr32(m, edx + 0x2e98u, 0x0u);
L_6ac010:
    ecx = ecx + rd32(m, esp + 0x18u);
    esi = (esi + 1u);
    fA_6ac015 = esi;
    fB_6ac015 = eax;
    if (int32_t(sext(fA_6ac015, 4)) < int32_t(sext(fB_6ac015, 4))) goto L_6abf80;
    edx = rd32(m, 0xa942e0u);
L_6ac023:
    eax = rd32(m, 0xa942dcu);
    eax = (eax - 1u);
    fA_6ac029 = edx;
    fB_6ac029 = eax;
    eax = rd32(m, esp + 0x1cu);
    if (int32_t(sext(fA_6ac029, 4)) < int32_t(sext(fB_6ac029, 4))) goto L_6ac042;
    wr32(m, 0xa942e0u, 0x0u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_6ac042:
    edi = rd32(m, esp); esp += 4;
    edx = (edx + 1u);
    esi = rd32(m, esp); esp += 4;
    wr32(m, 0xa942e0u, edx);
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 006ac050 ClipRecorder_CopySamples
//   original ClipRecorder_CopySamples; 1 instructions per frame (plan); register inputs: edx; returns with ret 0
REFLEX_PORT_FN(0x006ac050, ClipRecorder_CopySamples, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_6ac053 = 0;
    uint32_t fA_6ac0b1 = 0;
    uint32_t fB_6ac0b1 = 0;
    uint32_t fA_6ac0d0 = 0;
    uint32_t fB_6ac0d0 = 0;
    esp = esp - 0x8u;
    fR_6ac053 = (edx & edx);
    if (((fR_6ac053 == 0) || sgn(fR_6ac053, 4) != false)) goto L_6ac0db;
    ecx = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, 0xa942dcu);
    eax = ebp;
    eax = eax - ecx;
    ebx = (eax + eax * 4);
    eax = rd32(m, 0xa942d8u);
    eax = uint32_t(int32_t(eax) * int32_t(rd32(m, esp + 0x18u)));
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = (ecx + ecx * 4);
    ecx = rd32(m, 0xa7f124u);
    esi = (eax + eax * 4);
    ebp = (ebp + ebp * 4);
    esi = (esi << 4);
    edi = (edi << 4);
    ebx = (ebx << 4);
    ebp = (ebp << 4);
    wr32(m, esp + 0x10u, ecx);
    esi = esi + ecx;
    wr32(m, esp + 0x14u, edx);
L_6ac0a0:
    eax = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, ebx);
    eax = eax + edi;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x8fa070u, 0x6ac0aeu);  // MSVCR90.DLL::memcpy
    esp = esp + 0xcu;
    fA_6ac0b1 = rd32(m, esp + 0x1cu);
    fB_6ac0b1 = 0x0u;
    if (int32_t(sext(fA_6ac0b1, 4)) <= int32_t(sext(fB_6ac0b1, 4))) goto L_6ac0ca;
    ecx = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, ecx);
    edx = (esi + ebx);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x8fa070u, 0x6ac0c7u);  // MSVCR90.DLL::memcpy
    esp = esp + 0xcu;
L_6ac0ca:
    wr32(m, esp + 0x10u, rd32(m, esp + 0x10u) + ebp);
    esi = esi + ebp;
    fA_6ac0d0 = rd32(m, esp + 0x14u);
    fB_6ac0d0 = 0x1u;
    wr32(m, esp + 0x14u, fA_6ac0d0 - fB_6ac0d0);
    if (fA_6ac0d0 != fB_6ac0d0) goto L_6ac0a0;
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
L_6ac0db:
    esp = esp + 0x8u;
    LIFT_RET(0);
}

// 006ac630 ExpressionBoneAnimator__vf3
//   original ExpressionBoneAnimator::vf3; 380 instructions per frame (plan); register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x006ac630, ExpressionBoneAnimator__vf3, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_6ac630 = 0;
    uint32_t fB_6ac630 = 0;
    fA_6ac630 = rd32(m, ecx + 0x28u);
    fB_6ac630 = 0x0u;
    if (fA_6ac630 == fB_6ac630) goto L_6ac648;
    ecx = rd32(m, ecx + 0x28u);
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0x4u)));
    eax = rd32(m, ecx);
    edx = rd32(m, eax + 0xcu);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(edx, 0x6ac648u);
L_6ac648:
    LIFT_RET(4);
}

// 006ad090 ExpressionBoneAnimator__vf4
//   original ExpressionBoneAnimator::vf4; 42902 instructions per frame (plan); register inputs: ecx; returns with ret 12
REFLEX_PORT_FN(0x006ad090, ExpressionBoneAnimator__vf4, "anim", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_6ad0e0 = 0;
    uint32_t fB_6ad0e0 = 0;
    uint32_t fA_6ad140 = 0;
    uint32_t fB_6ad140 = 0;
    uint32_t fA_6ad278 = 0;
    uint32_t fB_6ad278 = 0;
    uint32_t fA_6ad2ca = 0;
    uint32_t fB_6ad2ca = 0;
    uint32_t fA_6ad2e7 = 0;
    uint32_t fB_6ad2e7 = 0;
    uint32_t fA_6ad2f8 = 0;
    uint32_t fB_6ad2f8 = 0;
    uint32_t fA_6ad31b = 0;
    uint32_t fB_6ad31b = 0;
    uint32_t fR_6ad32d = 0;
    uint32_t fA_6ad331 = 0;
    uint32_t fB_6ad331 = 0;
    uint32_t fA_6ad33f = 0;
    uint32_t fB_6ad33f = 0;
    uint32_t fR_6ad345 = 0;
    uint32_t fA_6ad355 = 0;
    uint32_t fB_6ad355 = 0;
    uint32_t fR_6ad371 = 0;
    uint32_t fA_6ad381 = 0;
    uint32_t fB_6ad381 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x24u;
    edx = rd32(m, ebp + 0x10u);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, edi);
    eax = esi;
    wr32(m, esp + 0x10u, ecx);
    LIFT_CALL(0x82e250u, 0x6ad0adu);  // FUN_0082e250
    edi = esi;
    LIFT_CALL(0x82e2d0u, 0x6ad0b4u);  // FUN_0082e2d0
    ebx = eax;
    edx = ebx;
    eax = esi;
    wr32(m, esp + 0x14u, ebx);
    LIFT_CALL(0x82e250u, 0x6ad0c3u);  // FUN_0082e250
    eax = rd32(m, esp + 0x10u);
    ecx = rd32(m, eax + 0x28u);
    edx = rd32(m, ecx);
    edx = rd32(m, edx + 0x10u);
    esp -= 4; wr32(m, esp, ebx);
    eax = (esp + 0x24u);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(edx, 0x6ad0d8u);
    edx = rd32(m, esi + 0x234u);
    edi = 0u;
    fA_6ad0e0 = edx;
    fB_6ad0e0 = 0x4u;
    if (int32_t(sext(fA_6ad0e0, 4)) < int32_t(sext(fB_6ad0e0, 4))) goto L_6ad278;
    esi = rd32(m, ebp + 0x10u);
    edi = ebx;
    eax = (esi + 0x44u);
    edi = edi - esi;
    esi = (edx - 0x4u);
    esi = (esi >> 2);
    esi = (esi + 1u);
    ecx = (ebx + 0x74u);
    ebx = (esi * 4);
    wr32(m, esp + 0x1cu, ebx);
    goto L_6ad110;
L_6ad110:
    ebx = rd8(m, ecx - 0x54u);
    wr8(m, eax - 0x24u, (ebx & 0xffu));
    x0 = movsd_m(rd64(m, ecx - 0x74u));
    wr64(m, eax - 0x44u, x0.lo);
    x0 = movsd_m(rd64(m, ecx - 0x6cu));
    wr64(m, eax - 0x3cu, x0.lo);
    ebx = rd8(m, ecx - 0x53u);
    wr8(m, eax - 0x23u, (ebx & 0xffu));
    eax = eax + 0xc0u;
    fpush(m, reflex_f80::from_f32(rd32(m, ecx - 0x64u)));
    ecx = ecx + 0xc0u;
    fA_6ad140 = esi;
    fB_6ad140 = 0x1u;
    esi = fA_6ad140 - fB_6ad140;
    wr32(m, eax - 0xf4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx - 0x120u)));
    wr32(m, eax - 0xf0u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx - 0x11cu)));
    wr32(m, eax - 0xecu, fst32(m));
    fpop(m);
    ebx = rd8(m, ecx - 0xe4u);
    wr8(m, eax - 0xb4u, (ebx & 0xffu));
    x0 = movsd_m(rd64(m, ecx - 0x104u));
    wr64(m, eax - 0xd4u, x0.lo);
    x0 = movsd_m(rd64(m, ecx - 0xfcu));
    wr64(m, eax - 0xccu, x0.lo);
    ebx = rd8(m, ecx - 0xe3u);
    wr8(m, eax - 0xb3u, (ebx & 0xffu));
    fpush(m, reflex_f80::from_f32(rd32(m, ecx - 0xf4u)));
    wr32(m, eax - 0xc4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, edi + eax - 0xc0u)));
    wr32(m, eax - 0xc0u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx - 0xecu)));
    wr32(m, eax - 0xbcu, fst32(m));
    fpop(m);
    ebx = rd8(m, ecx - 0xb4u);
    wr8(m, eax - 0x84u, (ebx & 0xffu));
    x0 = movsd_m(rd64(m, ecx - 0xd4u));
    wr64(m, eax - 0xa4u, x0.lo);
    x0 = movsd_m(rd64(m, ecx - 0xccu));
    wr64(m, eax - 0x9cu, x0.lo);
    ebx = rd8(m, ecx - 0xb3u);
    wr8(m, eax - 0x83u, (ebx & 0xffu));
    fpush(m, reflex_f80::from_f32(rd32(m, ecx - 0xc4u)));
    wr32(m, eax - 0x94u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx - 0xc0u)));
    wr32(m, eax - 0x90u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx - 0xbcu)));
    wr32(m, eax - 0x8cu, fst32(m));
    fpop(m);
    ebx = rd8(m, ecx - 0x84u);
    wr8(m, eax - 0x54u, (ebx & 0xffu));
    x0 = movsd_m(rd64(m, ecx - 0xa4u));
    wr64(m, eax - 0x74u, x0.lo);
    x0 = movsd_m(rd64(m, ecx - 0x9cu));
    wr64(m, eax - 0x6cu, x0.lo);
    ebx = rd8(m, ecx - 0x83u);
    wr8(m, eax - 0x53u, (ebx & 0xffu));
    fpush(m, reflex_f80::from_f32(rd32(m, ecx - 0x94u)));
    wr32(m, eax - 0x64u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx - 0x90u)));
    wr32(m, eax - 0x60u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx - 0x8cu)));
    wr32(m, eax - 0x5cu, fst32(m));
    fpop(m);
    if (fA_6ad140 != fB_6ad140) goto L_6ad110;
    ebx = rd32(m, esp + 0x14u);
    edi = rd32(m, esp + 0x1cu);
    esi = rd32(m, ebp + 0x8u);
L_6ad278:
    fA_6ad278 = edi;
    fB_6ad278 = edx;
    if (fA_6ad278 >= fB_6ad278) goto L_6ad2e3;
    eax = (edi + edi * 2);
    eax = (eax << 4);
    ecx = (eax + ebx + 0x20u);
    ebx = rd32(m, ebp + 0x10u);
    eax = (eax + ebx + 0x14u);
    ebx = rd32(m, esp + 0x14u);
    ebx = ebx - rd32(m, ebp + 0x10u);
    edx = edx - edi;
    edi = edx;
    goto L_6ad2a0;
L_6ad2a0:
    edx = rd8(m, ecx);
    wr8(m, eax + 0xcu, (edx & 0xffu));
    x0 = movsd_m(rd64(m, ecx - 0x20u));
    wr64(m, eax - 0x14u, x0.lo);
    x0 = movsd_m(rd64(m, ecx - 0x18u));
    wr64(m, eax - 0xcu, x0.lo);
    edx = rd8(m, ecx + 0x1u);
    wr8(m, eax + 0xdu, (edx & 0xffu));
    eax = eax + 0x30u;
    fpush(m, reflex_f80::from_f32(rd32(m, ecx - 0x10u)));
    ecx = ecx + 0x30u;
    fA_6ad2ca = edi;
    fB_6ad2ca = 0x1u;
    edi = fA_6ad2ca - fB_6ad2ca;
    wr32(m, eax - 0x34u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + ebx - 0x30u)));
    wr32(m, eax - 0x30u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, ecx - 0x38u)));
    wr32(m, eax - 0x2cu, fst32(m));
    fpop(m);
    if (fA_6ad2ca != fB_6ad2ca) goto L_6ad2a0;
    ebx = rd32(m, esp + 0x14u);
L_6ad2e3:
    edi = rd32(m, esp + 0x10u);
    fA_6ad2e7 = rd8(m, edi + 0x5cu);
    fB_6ad2e7 = 0x0u;
    if (fA_6ad2e7 == fB_6ad2e7) goto L_6ad39e;
    eax = rd32(m, edi + 0x50u);
    wr32(m, esp + 0x1cu, eax);
    fA_6ad2f8 = eax;
    fB_6ad2f8 = rd32(m, edi + 0x54u);
    if (fA_6ad2f8 <= fB_6ad2f8) goto L_6ad303;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x6ad303u);
L_6ad303:
    eax = rd32(m, esp + 0x1cu);
    edi = rd32(m, edi + 0x44u);
    wr32(m, esp + 0x18u, eax);
    edi = edi;
L_6ad310:
    eax = rd32(m, esp + 0x10u);
    ecx = rd32(m, eax + 0x54u);
    wr32(m, esp + 0x1cu, ecx);
    fA_6ad31b = rd32(m, eax + 0x50u);
    fB_6ad31b = ecx;
    if (fA_6ad31b <= fB_6ad31b) goto L_6ad32a;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x6ad326u);
    eax = rd32(m, esp + 0x10u);
L_6ad32a:
    eax = rd32(m, eax + 0x44u);
    fR_6ad32d = (edi & edi);
    if ((fR_6ad32d == 0)) goto L_6ad335;
    fA_6ad331 = edi;
    fB_6ad331 = eax;
    if (fA_6ad331 == fB_6ad331) goto L_6ad33b;
L_6ad335:
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x6ad33bu);
L_6ad33b:
    ecx = rd32(m, esp + 0x1cu);
    fA_6ad33f = rd32(m, esp + 0x18u);
    fB_6ad33f = ecx;
    if (fA_6ad33f == fB_6ad33f) goto L_6ad39e;
    fR_6ad345 = (edi & edi);
    if (!(fR_6ad345 == 0)) goto L_6ad396;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x6ad34fu);
    eax = 0u;
L_6ad351:
    edx = rd32(m, esp + 0x18u);
    fA_6ad355 = edx;
    fB_6ad355 = rd32(m, eax + 0x10u);
    if (fA_6ad355 < fB_6ad355) goto L_6ad360;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x6ad360u);
L_6ad360:
    eax = rd32(m, esp + 0x18u);
    ecx = rd32(m, eax);
    edx = rd32(m, ecx);
    eax = rd32(m, ebp + 0x10u);
    edx = rd32(m, edx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(edx, 0x6ad371u);
    fR_6ad371 = (edi & edi);
    if (!(fR_6ad371 == 0)) goto L_6ad39a;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x6ad37bu);
    eax = 0u;
L_6ad37d:
    ecx = rd32(m, esp + 0x18u);
    fA_6ad381 = ecx;
    fB_6ad381 = rd32(m, eax + 0x10u);
    if (fA_6ad381 < fB_6ad381) goto L_6ad38c;
    LIFT_CALL(rd32(m, 0x916224u) /* 8.286396299984444e-39 */, 0x6ad38cu);
L_6ad38c:
    wr32(m, esp + 0x18u, rd32(m, esp + 0x18u) + 0x4u);
    goto L_6ad310;
L_6ad396:
    eax = rd32(m, edi);
    goto L_6ad351;
L_6ad39a:
    eax = rd32(m, edi);
    goto L_6ad37d;
L_6ad39e:
    eax = rd32(m, ebp + 0xcu);
    x0 = movss_m(rd32(m, esp + 0x20u));
    wr32(m, eax, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x24u));
    wr32(m, eax + 0x4u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x28u));
    edi = (esi + 0x208u);
    esp -= 4; wr32(m, esp, edi);
    wr32(m, eax + 0x8u, x0.l(0));
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x6ad3ceu);
    ebx = ebx - rd32(m, esi + 0x228u);
    eax = 0x2aaaaaabu;
    { int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ebx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); }
    edx = uint32_t(int32_t(edx) >> 3);
    eax = edx;
    eax = (eax >> 31);
    eax = eax + edx;
    edx = 0u;
    { uint32_t d_ = rd32(m, esi + 0x234u); udiv32(eax, edx, d_); }
    edx = rd32(m, esi + 0x22cu);
    esp -= 4; wr32(m, esp, edi);
    wr32(m, edx + eax * 4, 0x0u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x6ad401u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(12);
}

// 006afa50 FeatherstoneAnimator__vf4
//   original FeatherstoneAnimator::vf4; 152 instructions per frame (plan); register inputs: ecx; returns with ret 
REFLEX_PORT_FN(0x006afa50, FeatherstoneAnimator__vf4, "anim", LIFT_OUTS) {
    LIFT_ENTRY();

    ecx = rd32(m, ecx + 0x30u);
    eax = rd32(m, ecx);
    eax = rd32(m, eax + 0x10u);
    LIFT_TAILJMP(eax);
}
