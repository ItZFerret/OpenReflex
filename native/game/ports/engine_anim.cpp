// Skeleton / animation core of the engine (0x0082xxxx, 0x0084xxxx): bone pose arrays, keyframe sampling, bone chains.
// Group "engine". Transliterated instruction by instruction from MXReflex.exe by the bike lifter (engine copy, see
// native/game/notes/engine.md): the original's operations, order and memory traffic, stack frame included.
#include <stdexcept>
#include <utility>
#include "lift_rt.h"

// 008220e0 IMemoryAllocator__vf0
//   original IMemoryAllocator::vf0; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x008220e0, IMemoryAllocator__vf0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_8220e0 = 0;
    fR_8220e0 = (rd8(m, esp + 0x4u) & 0x1u);
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    wr32(m, esi, 0x97539cu);
    if ((fR_8220e0 == 0)) goto L_8220f9;
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x78fc50u, 0x8220f6u);  // ReflexAlignedFree
    esp = esp + 0x4u;
L_8220f9:
    eax = esi;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 008227e0 Fn_008227e0
//   original FUN_008227e0; register inputs: none; returns with ret 8
REFLEX_PORT_FN(0x008227e0, Fn_008227e0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_8227ea = 0;
    uint32_t fB_8227ea = 0;
    uint32_t fA_822812 = 0;
    uint32_t fB_822812 = 0;
    uint32_t fR_822821 = 0;
    uint32_t fR_822836 = 0;
    uint32_t fR_822848 = 0;
    uint32_t fR_822854 = 0;
    uint32_t fA_82286b = 0;
    uint32_t fB_82286b = 0;
    uint32_t fR_82287d = 0;
    uint32_t fR_822889 = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x8u);
    eax = 0x1u;
    fA_8227ea = rd32(m, ebx + 0x1150u);
    fB_8227ea = eax;
    esp -= 4; wr32(m, esp, esi);
    wr32(m, esp + 0xcu, eax);
    if (int32_t(sext(fA_8227ea, 4)) <= int32_t(sext(fB_8227ea, 4))) goto L_822879;
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, edi);
    ebp = 0x90u;
L_822802:
    eax = rd32(m, ebx + 0x9cu);
    ecx = rd32(m, eax + ebp + 0x68u);
    esi = rd32(m, eax + ebp - 0x28u);
    eax = eax + ebp;
    fA_822812 = rd32(m, ecx + 0x11cu);
    fB_822812 = 0x0u;
    ecx = rd32(m, ecx + 0x120u);
    if (fA_822812 == fB_822812) goto L_822848;
    fR_822821 = (ecx & ecx);
    if ((fR_822821 == 0)) goto L_822860;
    eax = ecx;
    edi = (eax + 0x2u);
L_822830:
    edx = (edx & 0xffff0000u) | ((rd16(m, eax)) & 0xffffu);
    eax = eax + 0x2u;
    fR_822836 = ((edx & 0xffffu) & (edx & 0xffffu));
    if (!(fR_822836 == 0)) goto L_822830;
    eax = eax - edi;
    eax = uint32_t(int32_t(eax) >> 1);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x4029a0u, 0x822846u);  // FUN_004029a0
    goto L_822860;
L_822848:
    fR_822848 = (ecx & ecx);
    if ((fR_822848 == 0)) goto L_822860;
    eax = ecx;
    edi = (eax + 0x1u);
L_822851:
    edx = (edx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    eax = (eax + 1u);
    fR_822854 = ((edx & 0xffu) & (edx & 0xffu));
    if (!(fR_822854 == 0)) goto L_822851;
    eax = eax - edi;
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x4027e0u, 0x822860u);  // FUN_004027e0
L_822860:
    eax = rd32(m, esp + 0x14u);
    eax = (eax + 1u);
    ebp = ebp + 0x90u;
    fA_82286b = eax;
    fB_82286b = rd32(m, ebx + 0x1150u);
    wr32(m, esp + 0x14u, eax);
    if (int32_t(sext(fA_82286b, 4)) < int32_t(sext(fB_82286b, 4))) goto L_822802;
    edi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
L_822879:
    esi = rd32(m, esp + 0x10u);
    fR_82287d = (esi & esi);
    if ((fR_82287d == 0)) goto L_8228ab;
    eax = esi;
    edx = (eax + 0x1u);
L_822886:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    eax = (eax + 1u);
    fR_822889 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_822889 == 0)) goto L_822886;
    ecx = rd32(m, ebx + 0x1150u);
    ecx = (ecx + ecx * 8);
    eax = eax - edx;
    edx = rd32(m, ebx + 0x9cu);
    ecx = (ecx << 4);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, edx + ecx - 0x28u);
    LIFT_CALL(0x4027e0u, 0x8228abu);  // FUN_004027e0
L_8228ab:
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 00822d80 Fn_00822d80
//   original FUN_00822d80; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x00822d80, Fn_00822d80, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_822d9d = 0;
    uint32_t fB_822d9d = 0;
    uint32_t fR_822dc9 = 0;
    uint32_t fR_822df2 = 0;
    uint32_t fA_822dfa = 0;
    uint32_t fB_822dfa = 0;
    uint32_t fA_822e03 = 0;
    uint32_t fB_822e03 = 0;
    uint32_t fA_822e0c = 0;
    uint32_t fB_822e0c = 0;
    uint32_t fR_822e2a = 0;
    uint32_t fA_822e37 = 0;
    uint32_t fB_822e37 = 0;
    uint32_t fA_822e3c = 0;
    uint32_t fB_822e3c = 0;
    uint32_t fA_822e41 = 0;
    uint32_t fB_822e41 = 0;
    uint32_t fA_822e7b = 0;
    uint32_t fB_822e7b = 0;
    uint32_t fA_822f54 = 0;
    uint32_t fB_822f54 = 0;
    uint32_t fR_822f63 = 0;
    uint32_t fA_822f76 = 0;
    uint32_t fB_822f76 = 0;
    uint32_t fR_822f93 = 0;
    uint32_t fA_822f97 = 0;
    uint32_t fB_822f97 = 0;
    uint32_t fA_82300c = 0;
    uint32_t fB_82300c = 0;
    uint32_t fA_823020 = 0;
    uint32_t fB_823020 = 0;
    uint32_t fA_82308b = 0;
    uint32_t fB_82308b = 0;
    uint32_t fA_8230a5 = 0;
    uint32_t fB_8230a5 = 0;
    uint32_t fA_82317d = 0;
    uint32_t fB_82317d = 0;
    uint32_t fA_82318f = 0;
    uint32_t fB_82318f = 0;
    uint32_t fA_8231e6 = 0;
    uint32_t fB_8231e6 = 0;
    uint32_t fA_823234 = 0;
    uint32_t fB_823234 = 0;
    uint32_t fA_823242 = 0;
    uint32_t fB_823242 = 0;
    uint32_t fA_823252 = 0;
    uint32_t fB_823252 = 0;
    uint32_t fA_82325b = 0;
    uint32_t fB_82325b = 0;
    uint32_t fR_8232a3 = 0;
    uint32_t fA_8232ac = 0;
    uint32_t fB_8232ac = 0;
    uint32_t fA_8232bf = 0;
    uint32_t fB_8232bf = 0;
    uint32_t fA_8232c4 = 0;
    uint32_t fB_8232c4 = 0;
    uint32_t fA_8232d8 = 0;
    uint32_t fB_8232d8 = 0;
    uint32_t fA_8232e2 = 0;
    uint32_t fB_8232e2 = 0;
    uint32_t fR_823336 = 0;
    uint32_t fA_82333f = 0;
    uint32_t fB_82333f = 0;
    uint32_t fA_823347 = 0;
    uint32_t fB_823347 = 0;
    uint32_t fA_823364 = 0;
    uint32_t fB_823364 = 0;
    uint32_t fR_823393 = 0;
    uint32_t fA_823399 = 0;
    uint32_t fB_823399 = 0;
    uint32_t fR_8233a5 = 0;
    uint32_t fA_8233af = 0;
    uint32_t fB_8233af = 0;
    uint32_t fA_8233c0 = 0;
    uint32_t fB_8233c0 = 0;
    uint32_t fR_8233db = 0;
    uint32_t fA_8233ec = 0;
    uint32_t fB_8233ec = 0;
    uint32_t fR_823410 = 0;
    FCmp fC_823422{};
    uint32_t fA_823453 = 0;
    uint32_t fR_823453 = 0;
    uint32_t fR_82347e = 0;
    uint32_t fR_82348a = 0;
    uint32_t fR_8234a2 = 0;
    esp = esp - 0x2a0u;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp + 0x29cu, eax);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, esp + 0x2acu);
    fA_822d9d = rd32(m, ebp + 0x114cu);
    fB_822d9d = 0x0u;
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    if (fA_822d9d == fB_822d9d) goto L_8234a9;
    eax = rd32(m, ecx);
    eax = rd32(m, eax + 0x44u);
    edx = (esp + 0x1cu);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, 0x66u);
    edx = (esp + 0x28u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(eax, 0x822dbfu);
    ecx = 0u;
    wr32(m, esp + 0x10u, eax);
    wr32(m, esp + 0x14u, ecx);
    fR_822dc9 = (eax & eax);
    if (((fR_822dc9 == 0) || sgn(fR_822dc9, 4) != false)) goto L_8233fa;
    edi = rd32(m, 0x916260u) /* 8.286626112932593e-39 */;
    goto L_822de0;
L_822de0:
    esi = rd32(m, esp + ecx * 4 + 0x20u);
    ebx = rd32(m, 0x916254u) /* 8.286586876575592e-39 */;
    ecx = (esp + ecx * 4 + 0x20u);
    wr32(m, esp + 0x18u, ecx);
    fR_822df2 = (esi & esi);
    if ((fR_822df2 == 0)) goto L_8234a9;
    fA_822dfa = esi;
    fB_822dfa = 0x47u;
    if (fA_822dfa == fB_822dfa) goto L_8234a9;
    fA_822e03 = esi;
    fB_822e03 = 0x23u;
    if (int32_t(sext(fA_822e03, 4)) < int32_t(sext(fB_822e03, 4))) goto L_822f54;
    fA_822e0c = esi;
    fB_822e0c = 0x52u;
    if (int32_t(sext(fA_822e0c, 4)) >= int32_t(sext(fB_822e0c, 4))) goto L_822f54;
    LIFT_CALL(0x5c3610u, 0x822e1au);  // FUN_005c3610
    edx = eax;
    eax = esi;
    esi = (esp + 0x240u);
    LIFT_CALL(0x813700u, 0x822e2au);  // InputNames_KeyText
    fR_822e2a = (rd8(m, esp + 0x1cu) & 0xcu);
    if ((fR_822e2a == 0)) goto L_822e6c;
    ecx = rd32(m, esp + 0x18u);
    eax = rd32(m, ecx);
    fA_822e37 = eax;
    fB_822e37 = 0x48u;
    eax = fA_822e37 - fB_822e37;
    if (fA_822e37 == fB_822e37) goto L_822e64;
    fA_822e3c = eax;
    fB_822e3c = 0x1u;
    eax = fA_822e3c - fB_822e3c;
    if (fA_822e3c == fB_822e3c) goto L_822e5a;
    fA_822e41 = eax;
    fB_822e41 = 0x6u;
    eax = fA_822e41 - fB_822e41;
    if (fA_822e41 == fB_822e41) goto L_822e50;
    wr8(m, esp + 0x240u, (rd8(m, esp + 0x240u) + 0xe0u) & 0xffu);
    goto L_822e6c;
L_822e50:
    wr8(m, esp + 0x240u, 0x22u);
    goto L_822e6c;
L_822e5a:
    wr8(m, esp + 0x240u, 0x2bu);
    goto L_822e6c;
L_822e64:
    wr8(m, esp + 0x240u, 0x5fu);
L_822e6c:
    edx = rd32(m, ebp + 0x1144u);
    edx = (edx << 7);
    edx = edx + rd32(m, ebp + 0x11f8u);
    fA_822e7b = rd8(m, edx + ebp + 0x144u);
    fB_822e7b = 0x0u;
    eax = (edx + ebp);
    if (fA_822e7b != fB_822e7b) goto L_822ea0;
    ecx = (ecx & 0xffffff00u) | ((rd8(m, esp + 0x240u)) & 0xffu);
    wr8(m, eax + 0x144u, (ecx & 0xffu));
    wr32(m, ebp + 0x11f8u, (rd32(m, ebp + 0x11f8u) + 1u));
    goto L_8233e7;
L_822ea0:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, eax + 0x143u)) & 0xffu);
    wr8(m, eax + 0x143u, 0x0u);
    edx = rd32(m, ebp + 0x1144u);
    edx = (edx << 7);
    edx = edx + rd32(m, ebp + 0x11f8u);
    esp -= 4; wr32(m, esp, 0x7fu);
    eax = (edx + ebp + 0x144u);
    esp -= 4; wr32(m, esp, eax);
    ecx = (esp + 0x1c0u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(edi, 0x822ed0u);
    edx = rd32(m, ebp + 0x1144u);
    edx = (edx << 7);
    edx = edx + rd32(m, ebp + 0x11f8u);
    wr8(m, esp + 0x243u, 0x0u);
    wr8(m, edx + ebp + 0x143u, (ebx & 0xffu));
    eax = rd32(m, ebp + 0x1144u);
    ecx = (ecx & 0xffffff00u) | ((rd8(m, esp + 0x24cu)) & 0xffu);
    eax = (eax << 7);
    eax = eax + rd32(m, ebp + 0x11f8u);
    esp = esp + 0xcu;
    wr8(m, eax + ebp + 0x144u, (ecx & 0xffu));
    edx = rd32(m, ebp + 0x1144u);
    edx = (edx << 7);
    edx = edx + rd32(m, ebp + 0x11f8u);
    esp -= 4; wr32(m, esp, 0x7fu);
    wr8(m, edx + ebp + 0x145u, 0x0u);
    ecx = rd32(m, ebp + 0x1144u);
    eax = (esp + 0x1bcu);
    ecx = (ecx << 7);
    esp -= 4; wr32(m, esp, eax);
    edx = (ecx + ebp + 0x144u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(rd32(m, 0x916254u) /* 8.286586876575592e-39 */, 0x822f46u);
    esp = esp + 0xcu;
    wr32(m, ebp + 0x11f8u, (rd32(m, ebp + 0x11f8u) + 1u));
    goto L_8233e7;
L_822f54:
    fA_822f54 = esi;
    fB_822f54 = 0x64u;
    if (fA_822f54 != fB_822f54) goto L_82300c;
    ecx = rd32(m, ebp + 0x11f8u);
    fR_822f63 = (ecx & ecx);
    if (((fR_822f63 == 0) || sgn(fR_822f63, 4) != false)) goto L_8233e7;
    eax = rd32(m, ebp + 0x1144u);
    eax = (eax << 7);
    ecx = ecx + eax;
    fA_822f76 = rd8(m, ecx + ebp + 0x144u);
    fB_822f76 = 0x0u;
    edx = (ecx + ebp);
    if (fA_822f76 != fB_822f76) goto L_822fac;
    eax = (eax + ebp + 0x144u);
    esi = (eax + 0x1u);
L_822f90:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    eax = (eax + 1u);
    fR_822f93 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_822f93 == 0)) goto L_822f90;
    fA_822f97 = eax;
    fB_822f97 = esi;
    eax = fA_822f97 - fB_822f97;
    if (fA_822f97 == fB_822f97) goto L_823001;
    wr8(m, edx + 0x143u, (ecx & 0xffu));
    wr32(m, ebp + 0x11f8u, (rd32(m, ebp + 0x11f8u) - 1u));
    goto L_8233e7;
L_822fac:
    wr8(m, edx + 0x143u, 0x0u);
    eax = rd32(m, ebp + 0x1144u);
    eax = (eax << 7);
    eax = eax + rd32(m, ebp + 0x11f8u);
    esp -= 4; wr32(m, esp, 0x7fu);
    ecx = (eax + ebp + 0x144u);
    esp -= 4; wr32(m, esp, ecx);
    edx = (esp + 0x1c0u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(edi, 0x822fd6u);
    ecx = rd32(m, ebp + 0x1144u);
    esp = esp + 0xcu;
    esp -= 4; wr32(m, esp, 0x7fu);
    eax = (esp + 0x1bcu);
    ecx = (ecx << 7);
    esp -= 4; wr32(m, esp, eax);
    edx = (ecx + ebp + 0x144u);
    esp -= 4; wr32(m, esp, edx);
    wr8(m, esp + 0x243u, 0x0u);
    LIFT_CALL(ebx, 0x822ffeu);
    esp = esp + 0xcu;
L_823001:
    wr32(m, ebp + 0x11f8u, (rd32(m, ebp + 0x11f8u) - 1u));
    goto L_8233e7;
L_82300c:
    fA_82300c = esi;
    fB_82300c = 0x6eu;
    if (fA_82300c != fB_82300c) goto L_82308b;
    eax = rd32(m, ebp + 0x1144u);
    eax = (eax << 7);
    eax = eax + rd32(m, ebp + 0x11f8u);
    fA_823020 = rd8(m, eax + ebp + 0x144u);
    fB_823020 = 0x0u;
    eax = (eax + ebp + 0x144u);
    if (fA_823020 == fB_823020) goto L_8233e7;
    wr8(m, eax, 0x0u);
    ecx = rd32(m, ebp + 0x1144u);
    ecx = (ecx << 7);
    ecx = ecx + rd32(m, ebp + 0x11f8u);
    esp -= 4; wr32(m, esp, 0x7fu);
    edx = (ecx + ebp + 0x145u);
    esp -= 4; wr32(m, esp, edx);
    eax = (esp + 0x1c0u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(edi, 0x82305bu);
    edx = rd32(m, ebp + 0x1144u);
    esp = esp + 0xcu;
    esp -= 4; wr32(m, esp, 0x7fu);
    ecx = (esp + 0x1bcu);
    edx = (edx << 7);
    esp -= 4; wr32(m, esp, ecx);
    eax = (edx + ebp + 0x144u);
    esp -= 4; wr32(m, esp, eax);
    wr8(m, esp + 0x243u, 0x0u);
    LIFT_CALL(ebx, 0x823083u);
    esp = esp + 0xcu;
    goto L_8233e7;
L_82308b:
    fA_82308b = esi;
    fB_82308b = 0x65u;
    if (fA_82308b != fB_82308b) goto L_82317d;
    ecx = rd32(m, ebp + 0x1144u);
    edx = rd32(m, ebp + 0x11f8u);
    ecx = (ecx << 7);
    edx = edx + ecx;
    fA_8230a5 = rd8(m, edx + ebp + 0x144u);
    fB_8230a5 = 0x0u;
    eax = (edx + ebp);
    esp -= 4; wr32(m, esp, 0x7fu);
    if (fA_8230a5 != fB_8230a5) goto L_8230d1;
    eax = (ecx + ebp + 0x144u);
    esp -= 4; wr32(m, esp, 0x93de14u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(ebx, 0x8230c3u);
    esp = esp + 0xcu;
    wr32(m, ebp + 0x11f8u, (rd32(m, ebp + 0x11f8u) + 1u));
    goto L_8233e7;
L_8230d1:
    ebx = (ebx & 0xffffff00u) | ((rd8(m, eax + 0x143u)) & 0xffu);
    wr8(m, eax + 0x143u, 0x0u);
    ecx = rd32(m, ebp + 0x1144u);
    ecx = (ecx << 7);
    ecx = ecx + rd32(m, ebp + 0x11f8u);
    eax = (esp + 0x1bcu);
    edx = (ecx + ebp + 0x144u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(edi, 0x8230ffu);
    ecx = rd32(m, ebp + 0x1144u);
    ecx = (ecx << 7);
    ecx = ecx + rd32(m, ebp + 0x11f8u);
    wr8(m, esp + 0x243u, 0x0u);
    wr8(m, ecx + ebp + 0x143u, (ebx & 0xffu));
    edx = rd32(m, ebp + 0x1144u);
    edx = (edx << 7);
    edx = edx + rd32(m, ebp + 0x11f8u);
    esp = esp + 0xcu;
    wr8(m, edx + ebp + 0x144u, 0x20u);
    eax = rd32(m, ebp + 0x1144u);
    eax = (eax << 7);
    eax = eax + rd32(m, ebp + 0x11f8u);
    esp -= 4; wr32(m, esp, 0x7fu);
    wr8(m, eax + ebp + 0x145u, 0x0u);
    edx = rd32(m, ebp + 0x1144u);
    ecx = (esp + 0x1bcu);
    edx = (edx << 7);
    esp -= 4; wr32(m, esp, ecx);
    eax = (edx + ebp + 0x144u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x916254u) /* 8.286586876575592e-39 */, 0x82316fu);
    esp = esp + 0xcu;
    wr32(m, ebp + 0x11f8u, (rd32(m, ebp + 0x11f8u) + 1u));
    goto L_8233e7;
L_82317d:
    fA_82317d = esi;
    fB_82317d = 0x66u;
    if (fA_82317d != fB_82317d) goto L_823234;
    ecx = rd32(m, ebp + 0x1144u);
    ecx = (ecx << 7);
    fA_82318f = rd8(m, ecx + ebp + 0x144u);
    fB_82318f = 0x0u;
    eax = (ecx + ebp + 0x144u);
    if (fA_82318f == fB_82318f) goto L_823234;
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, 0x9745fcu);
    esi = 0x82u;
    edi = (esp + 0x1c0u);
    LIFT_CALL(0x78fd60u, 0x8231bbu);  // FUN_0078fd60
    esp = esp + 0x8u;
    edx = edi;
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, ebp);
    LIFT_CALL(0x8227e0u, 0x8231c7u);  // FUN_008227e0
    eax = rd32(m, ebp + 0x1144u);
    eax = (eax << 7);
    ecx = (eax + ebp + 0x144u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x813eb0u, 0x8231ddu);  // FUN_00813eb0
    wr32(m, ebp + 0x1144u, (rd32(m, ebp + 0x1144u) + 1u));
    esp = esp + 0x4u;
    fA_8231e6 = rd32(m, ebp + 0x1144u);
    fB_8231e6 = 0x20u;
    if (int32_t(sext(fA_8231e6, 4)) < int32_t(sext(fB_8231e6, 4))) goto L_8231f9;
    wr32(m, ebp + 0x1144u, 0x0u);
L_8231f9:
    eax = rd32(m, ebp + 0x1144u);
    wr32(m, ebp + 0x1148u, eax);
    esp -= 4; wr32(m, esp, 0x80u);
    eax = (eax << 7);
    edx = (eax + ebp + 0x144u);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x8fa06au, 0x82321cu);  // MSVCR90.DLL::memset
    edi = rd32(m, 0x916260u) /* 8.286626112932593e-39 */;
    esp = esp + 0xcu;
    wr32(m, ebp + 0x11f8u, 0x0u);
    goto L_8233e7;
L_823234:
    fA_823234 = esi;
    fB_823234 = 0x84u;
    if (fA_823234 != fB_823234) goto L_8232ac;
    ecx = rd32(m, ebp + 0x1148u);
    fA_823242 = ecx;
    fB_823242 = 0x1u;
    ecx = fA_823242 - fB_823242;
    if (!sgn(((fA_823242 - fB_823242)), 4)) goto L_82324c;
    ecx = 0x1fu;
L_82324c:
    eax = rd32(m, ebp + 0x1144u);
    fA_823252 = ecx;
    fB_823252 = eax;
    if (fA_823252 == fB_823252) goto L_823289;
    edx = ecx;
    edx = (edx << 7);
    fA_82325b = rd8(m, edx + ebp + 0x144u);
    fB_82325b = 0x0u;
    edx = (edx + ebp + 0x144u);
    if (fA_82325b == fB_82325b) goto L_823289;
    eax = (eax << 7);
    esp -= 4; wr32(m, esp, 0x7fu);
    esi = (eax + ebp + 0x144u);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, esi);
    wr32(m, ebp + 0x1148u, ecx);
    LIFT_CALL(edi, 0x823282u);
    esp = esp + 0xcu;
    wr8(m, esi + 0x7fu, 0x0u);
L_823289:
    eax = rd32(m, ebp + 0x1144u);
    eax = (eax << 7);
    eax = (eax + ebp + 0x144u);
    edx = (eax + 0x1u);
L_8232a0:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    eax = (eax + 1u);
    fR_8232a3 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_8232a3 == 0)) goto L_8232a0;
    goto L_8233df;
L_8232ac:
    fA_8232ac = esi;
    fB_8232ac = 0x85u;
    if (fA_8232ac != fB_8232ac) goto L_82333f;
    eax = rd32(m, ebp + 0x1148u);
    eax = (eax + 1u);
    fA_8232bf = eax;
    fB_8232bf = 0x20u;
    if (int32_t(sext(fA_8232bf, 4)) < int32_t(sext(fB_8232bf, 4))) goto L_8232cf;
    fA_8232c4 = rd32(m, ebp + 0x1144u);
    fB_8232c4 = 0x1fu;
    if (fA_8232c4 == fB_8232c4) goto L_8232cf;
    eax = 0u;
L_8232cf:
    ecx = rd32(m, ebp + 0x1144u);
    edx = (eax - 0x1u);
    fA_8232d8 = edx;
    fB_8232d8 = ecx;
    if (fA_8232d8 == fB_8232d8) goto L_823320;
    wr32(m, ebp + 0x1148u, eax);
    fA_8232e2 = eax;
    fB_8232e2 = ecx;
    if (fA_8232e2 != fB_8232e2) goto L_8232ff;
    esp -= 4; wr32(m, esp, 0x80u);
    ecx = (ecx << 7);
    eax = (ecx + ebp + 0x144u);
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x8fa06au, 0x8232fdu);  // MSVCR90.DLL::memset
    goto L_82331d;
L_8232ff:
    ecx = (ecx << 7);
    esi = (ecx + ebp + 0x144u);
    eax = (eax << 7);
    esp -= 4; wr32(m, esp, 0x7fu);
    ecx = (eax + ebp + 0x144u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(edi, 0x823319u);
    wr8(m, esi + 0x7fu, 0x0u);
L_82331d:
    esp = esp + 0xcu;
L_823320:
    edx = rd32(m, ebp + 0x1144u);
    edx = (edx << 7);
    eax = (edx + ebp + 0x144u);
    edx = (eax + 0x1u);
L_823333:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    eax = (eax + 1u);
    fR_823336 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_823336 == 0)) goto L_823333;
    goto L_8233df;
L_82333f:
    fA_82333f = esi;
    fB_82333f = 0x86u;
    if (fA_82333f != fB_82333f) goto L_823364;
    fA_823347 = rd32(m, ebp + 0x11f8u);
    fB_823347 = 0xffffffffu;
    wr32(m, ebp + 0x11f8u, fA_823347 + fB_823347);
    eax = rd32(m, ebp + 0x11f8u);
    ecx = 0x0u;
    ecx = (ecx & 0xffffff00u) | (((sgn(((fA_823347 + fB_823347)), 4)) ? 1u : 0u) & 0xffu);
    ecx = (ecx - 1u);
    eax = (eax & ecx);
    goto L_8233e1;
L_823364:
    fA_823364 = esi;
    fB_823364 = 0x87u;
    if (fA_823364 != fB_823364) goto L_8233af;
    wr32(m, ebp + 0x11f8u, (rd32(m, ebp + 0x11f8u) + 1u));
    edx = rd32(m, ebp + 0x1144u);
    esi = rd32(m, ebp + 0x11f8u);
    edx = (edx << 7);
    edx = (edx + ebp + 0x144u);
    eax = edx;
    ebx = (eax + 0x1u);
L_823390:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    eax = (eax + 1u);
    fR_823393 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_823393 == 0)) goto L_823390;
    eax = eax - ebx;
    fA_823399 = eax;
    fB_823399 = esi;
    if (int32_t(sext(fA_823399, 4)) >= int32_t(sext(fB_823399, 4))) goto L_8233ab;
    eax = edx;
    edx = (eax + 0x1u);
L_8233a2:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    eax = (eax + 1u);
    fR_8233a5 = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_8233a5 == 0)) goto L_8233a2;
    goto L_8233df;
L_8233ab:
    eax = esi;
    goto L_8233e1;
L_8233af:
    fA_8233af = esi;
    fB_8233af = 0x6fu;
    if (fA_8233af != fB_8233af) goto L_8233c0;
    wr32(m, ebp + 0x11f8u, 0x0u);
    goto L_8233e7;
L_8233c0:
    fA_8233c0 = esi;
    fB_8233c0 = 0x70u;
    if (fA_8233c0 != fB_8233c0) goto L_8233e7;
    eax = rd32(m, ebp + 0x1144u);
    eax = (eax << 7);
    eax = (eax + ebp + 0x144u);
    edx = (eax + 0x1u);
L_8233d8:
    ecx = (ecx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    eax = (eax + 1u);
    fR_8233db = ((ecx & 0xffu) & (ecx & 0xffu));
    if (!(fR_8233db == 0)) goto L_8233d8;
L_8233df:
    eax = eax - edx;
L_8233e1:
    wr32(m, ebp + 0x11f8u, eax);
L_8233e7:
    ecx = rd32(m, esp + 0x14u);
    ecx = (ecx + 1u);
    fA_8233ec = ecx;
    fB_8233ec = rd32(m, esp + 0x10u);
    wr32(m, esp + 0x14u, ecx);
    if (int32_t(sext(fA_8233ec, 4)) < int32_t(sext(fB_8233ec, 4))) goto L_822de0;
L_8233fa:
    esi = rd32(m, 0x9164b8u) /* 8.28818715942185e-39 */;
    LIFT_CALL(esi, 0x823402u);
    eax = eax - rd32(m, ebp + 0x11f4u);
    wr32(m, esp + 0x10u, eax);
    fpush(m, reflex_f80::from_int(int64_t(int32_t(sext(rd32(m, esp + 0x10u), 4)))));
    fR_823410 = (eax & eax);
    if ((sgn(fR_823410, 4) == false)) goto L_82341a;
    st(m, 0) = farith(m, 0, st(m, 0), reflex_f80::from_f64(rd64(m, 0x977eb0u) /* 4294967296.0 */));
L_82341a:
    fpush(m, reflex_f80::from_f64(rd64(m, 0x977ea8u) /* 400.0 */));
    std::swap(st(m, 0), st(m, 1));
    fC_823422 = f80cmp(st(m, 0), st(m, 1));
    fpop(m);
    st(m, 0) = st(m, 0);
    fpop(m);
    if ((fc_b(fC_823422) || fc_z(fC_823422))) goto L_82343e;
    ecx = rd32(m, ebp + 0x11f0u);
    ecx = ~ecx;
    wr32(m, ebp + 0x11f0u, ecx);
    LIFT_CALL(esi, 0x823438u);
    wr32(m, ebp + 0x11f4u, eax);
L_82343e:
    edx = rd32(m, ebp + 0x1144u);
    ecx = rd32(m, ebp + 0x11f0u);
    edx = (edx << 7);
    edx = edx + rd32(m, ebp + 0x11f8u);
    fA_823453 = ecx;
    fR_823453 = (0u - fA_823453);
    ecx = fR_823453;
    ecx = (ecx & 0xffffff00u) | ((((fA_823453 != 0)) ? 0xffu : 0u) & 0xffu);
    eax = (edx + ebp + 0x144u);
    ecx = (ecx & 0xffffff00u) | ((((ecx & 0xffu) & 0x3fu)) & 0xffu);
    ecx = (ecx & 0xffffff00u) | ((((ecx & 0xffu) + 0x20u) & 0xffu) & 0xffu);
    wr32(m, ebp + 0x11fcu, eax);
    ebx = (ebx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    wr8(m, eax, (ecx & 0xffu));
    edx = rd32(m, ebp + 0x1144u);
    edx = (edx << 7);
    ecx = (edx + ebp + 0x144u);
    fR_82347e = (ecx & ecx);
    if ((fR_82347e == 0)) goto L_82349c;
    eax = ecx;
    esi = (eax + 0x1u);
L_823487:
    edx = (edx & 0xffffff00u) | ((rd8(m, eax)) & 0xffu);
    eax = (eax + 1u);
    fR_82348a = ((edx & 0xffu) & (edx & 0xffu));
    if (!(fR_82348a == 0)) goto L_823487;
    eax = eax - esi;
    esi = rd32(m, ebp + 0x118u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x4027e0u, 0x82349cu);  // FUN_004027e0
L_82349c:
    ebp = rd32(m, ebp + 0x11fcu);
    fR_8234a2 = (ebp & ebp);
    if ((fR_8234a2 == 0)) goto L_8234a9;
    wr8(m, ebp, (ebx & 0xffu));
L_8234a9:
    ecx = rd32(m, esp + 0x2acu);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x8234bbu);  // __security_check_cookie
    esp = esp + 0x2a0u;
    LIFT_RET(4);
}

// 008234f0 DevConsole__vf19
//   original DevConsole::vf19; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x008234f0, DevConsole__vf19, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_823519 = 0;
    uint32_t fB_823519 = 0;
    uint32_t fR_823549 = 0;
    uint32_t fA_823573 = 0;
    uint32_t fB_823573 = 0;
    uint32_t fR_82358a = 0;
    uint32_t fA_823595 = 0;
    uint32_t fB_823595 = 0;
    uint32_t fR_8235ac = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff8u);
    esp = esp - 0x88u;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp + 0x84u, eax);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    esp -= 4; wr32(m, esp, 0xd6b56cu);
    edi = ecx;
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x823519u);
    fA_823519 = rd32(m, 0xd6b560u);
    fB_823519 = 0x0u;
    if (fA_823519 == fB_823519) goto L_823539;
    eax = (esp + 0x8u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x82ae10u, 0x82352cu);  // FUN_0082ae10
    ecx = (esp + 0x8u);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x813eb0u, 0x823536u);  // FUN_00813eb0
    esp = esp + 0x4u;
L_823539:
    esp -= 4; wr32(m, esp, 0xd6b56cu);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x823544u);
    eax = 0x1u;
    fR_823549 = (rd8(m, 0xd92f48u) & (eax & 0xffu));
    if (!(fR_823549 == 0)) goto L_823569;
    wr32(m, 0xd92f48u, (rd32(m, 0xd92f48u) | eax));
    LIFT_CALL(0x812170u, 0x82355cu);  // InputNames_Init
    esp -= 4; wr32(m, esp, 0x9127c0u);
    LIFT_CALL(0x8f9fbfu, 0x823566u);  // _atexit
    esp = esp + 0x4u;
L_823569:
    esi = 0xd92f50u;
    LIFT_CALL(0x813ab0u, 0x823573u);  // FUN_00813ab0
    fA_823573 = rd8(m, 0x9b716du);
    fB_823573 = 0x0u;
    if (fA_823573 != fB_823573) goto L_8235b7;
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0x7u);
    ecx = 0xd6a718u;
    LIFT_CALL(0x811420u, 0x82358au);  // KeyMap::vf1
    fR_82358a = (eax & eax);
    if ((fR_82358a == 0)) goto L_823595;
    eax = edi;
    LIFT_CALL(0x822700u, 0x823595u);  // FUN_00822700
L_823595:
    fA_823595 = rd8(m, 0x9b716du);
    fB_823595 = 0x0u;
    if (fA_823595 != fB_823595) goto L_8235b7;
    esp -= 4; wr32(m, esp, 0x0u);
    esp -= 4; wr32(m, esp, 0x7u);
    ecx = 0xd69e90u;
    LIFT_CALL(0x811420u, 0x8235acu);  // KeyMap::vf1
    fR_8235ac = (eax & eax);
    if ((fR_8235ac == 0)) goto L_8235b7;
    eax = edi;
    LIFT_CALL(0x822700u, 0x8235b7u);  // FUN_00822700
L_8235b7:
    ecx = rd32(m, 0xa95844u);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x8u);
    LIFT_CALL(eax, 0x8235c4u);
    esp -= 4; wr32(m, esp, edi);
    ecx = eax;
    LIFT_CALL(0x822d80u, 0x8235ccu);  // FUN_00822d80
    ecx = rd32(m, esp + 0x8cu);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ecx = (ecx ^ esp);
    LIFT_CALL(0x8f9efeu, 0x8235dcu);  // __security_check_cookie
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00824390 Registry__vf3
//   original Registry::vf3; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x00824390, Registry__vf3, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_824398 = 0;
    uint32_t fB_824398 = 0;
    uint32_t fA_8243a7 = 0;
    uint32_t fB_8243a7 = 0;
    edx = rd32(m, ecx + 0x3cu);
    esp -= 4; wr32(m, esp, ebx);
    ebx = 0u;
    eax = 0u;
    fA_824398 = edx;
    fB_824398 = ebx;
    if (int32_t(sext(fA_824398, 4)) <= int32_t(sext(fB_824398, 4))) goto L_8243af;
    esp -= 4; wr32(m, esp, esi);
L_8243a0:
    esi = rd32(m, ecx + 0x38u);
    esi = rd32(m, esi + eax * 4);
    eax = (eax + 1u);
    fA_8243a7 = eax;
    fB_8243a7 = edx;
    wr8(m, esi + 0x8u, (ebx & 0xffu));
    if (int32_t(sext(fA_8243a7, 4)) < int32_t(sext(fB_8243a7, 4))) goto L_8243a0;
    esi = rd32(m, esp); esp += 4;
L_8243af:
    wr32(m, ecx + 0x3cu, ebx);
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00824540 Registry__vf20
//   original Registry::vf20; register inputs: none; returns with ret 12
REFLEX_PORT_FN(0x00824540, Registry__vf20, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, esp + 0x8u);
    eax = rd32(m, esi);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x82454eu);
    eax = rd32(m, esp + 0x10u);
    ecx = rd32(m, esi);
    ecx = rd32(m, ecx + 0x18u);
    edx = rd32(m, ecx);
    edx = rd32(m, edx + 0xd8u);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, esi + 0x4u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(edx, 0x82456bu);
    eax = rd32(m, esi);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x824574u);
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(12);
}

// 00824640 Registry__vf17
//   original Registry::vf17; register inputs: none; returns with ret 12
REFLEX_PORT_FN(0x00824640, Registry__vf17, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, esp + 0x8u);
    eax = rd32(m, esi);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x82464eu);
    eax = rd32(m, esp + 0x10u);
    ecx = rd32(m, esi);
    ecx = rd32(m, ecx + 0x18u);
    edx = rd32(m, ecx);
    edx = rd32(m, edx + 0xccu);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, esp + 0x10u);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, esi + 0x4u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(edx, 0x82466bu);
    eax = rd32(m, esi);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x824674u);
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(12);
}

// 00824af0 Registry__vf30
//   original Registry::vf30; register inputs: ecx; returns with ret 12
REFLEX_PORT_FN(0x00824af0, Registry__vf30, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_824b28 = 0;
    uint32_t fB_824b28 = 0;
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = rd32(m, esp + 0xcu);
    eax = rd32(m, edi);
    esp -= 4; wr32(m, esp, eax);
    esi = ecx;
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x824b01u);
    eax = rd32(m, esp + 0x14u);
    ecx = rd32(m, edi);
    ecx = rd32(m, ecx + 0x18u);
    edx = rd32(m, ecx);
    edx = rd32(m, edx + 0x1bcu);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, edi + 0x4u);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(edx, 0x824b1eu);
    esi = esi + 0x38u;
    wr8(m, edi + 0x8u, 0x1u);
    eax = rd32(m, esi + 0x8u);
    fA_824b28 = rd32(m, esi + 0x4u);
    fB_824b28 = eax;
    if (int32_t(sext(fA_824b28, 4)) < int32_t(sext(fB_824b28, 4))) goto L_824b38;
    ecx = rd32(m, esi + 0xcu);
    ecx = ecx + eax;
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x7cff20u, 0x824b38u);  // FUN_007cff20
L_824b38:
    edx = rd32(m, esi + 0x4u);
    eax = rd32(m, esi);
    wr32(m, eax + edx * 4, edi);
    wr32(m, esi + 0x4u, (rd32(m, esi + 0x4u) + 1u));
    ecx = rd32(m, edi);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x824b4cu);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(12);
}

// 00827f70 XMLReader__vf1
//   original XMLReader::vf1; register inputs: ecx; returns with ret 0
REFLEX_PORT_FN(0x00827f70, XMLReader__vf1, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = rd32(m, ecx + 0x38u);
    wr32(m, ecx + 0x3cu, eax);
    LIFT_RET(0);
}

// 008280b0 XMLReader__vf8
//   original XMLReader::vf8; register inputs: ecx; returns with ret 12
REFLEX_PORT_FN(0x008280b0, XMLReader__vf8, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_8280e9 = 0;
    uint32_t fR_828102 = 0;
    uint32_t fR_828151 = 0;
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    edi = ecx;
    eax = rd32(m, edi + 0x2c4u);
    ebx = rd32(m, edi + eax * 4 + 0x180u);
    eax = rd32(m, esp + 0x18u);
    edx = (ebx + ebx * 2 + 0x75u);
    esi = (edi + edx * 4);
    ecx = (ebx + ebx * 2);
    wr32(m, esi, eax);
    eax = rd32(m, esp + 0x1cu);
    ebp = (edi + ecx * 4);
    wr32(m, ebp + 0x1d8u, 0xffffffffu);
    wr32(m, esp + 0x10u, esi);
    fR_8280e9 = (eax & eax);
    if ((fR_8280e9 == 0)) goto L_82813d;
    esi = rd32(m, edi + 0x10u);
    ecx = (esp + 0x1cu);
    wr32(m, esp + 0x18u, eax);
    esp -= 4; wr32(m, esp, ecx);
    eax = (esp + 0x1cu);
    LIFT_CALL(0x7fa030u, 0x828102u);  // FUN_007fa030
    fR_828102 = (eax & eax);
    if (!(fR_828102 == 0)) goto L_828127;
    edx = rd32(m, esp + 0x10u);
    wr32(m, edx, eax);
    eax = rd32(m, esp + 0x20u);
    wr32(m, eax, 0xffffffffu);
    wr32(m, edi + 0x3cu, 0x0u);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(12);
L_828127:
    ecx = rd32(m, edi + 0xcu);
    edx = rd32(m, esp + 0x1cu);
    eax = rd32(m, ecx + edx * 4);
    esi = rd32(m, esp + 0x10u);
    wr32(m, ebp + 0x1dcu, eax);
    goto L_828147;
L_82813d:
    wr32(m, ebp + 0x1dcu, 0x0u);
L_828147:
    edx = rd32(m, edi);
    eax = rd32(m, edx + 0x28u);
    esp -= 4; wr32(m, esp, ebx);
    ecx = edi;
    LIFT_CALL(eax, 0x828151u);
    fR_828151 = (eax & eax);
    if (!(fR_828151 == 0)) goto L_82816c;
    ecx = rd32(m, esp + 0x20u);
    wr32(m, esi, eax);
    wr32(m, ecx, 0xffffffffu);
    wr32(m, edi + 0x3cu, eax);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(12);
L_82816c:
    wr32(m, edi + 0x2c4u, (rd32(m, edi + 0x2c4u) - 1u));
    edx = rd32(m, esp + 0x20u);
    wr32(m, edx, ebx);
    wr32(m, edi + 0x3cu, eax);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    ecx = rd32(m, esp); esp += 4;
    LIFT_RET(12);
}

// 00828190 XMLReader__vf9
//   original XMLReader::vf9; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x00828190, XMLReader__vf9, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_828194 = 0;
    uint32_t fB_828194 = 0;
    eax = rd32(m, esp + 0x4u);
    fA_828194 = eax;
    fB_828194 = 0xffffffffu;
    if (fA_828194 == fB_828194) goto L_8281b7;
    edx = (eax + eax * 2 + 0x75u);
    wr32(m, ecx + edx * 4, 0x0u);
    edx = rd32(m, ecx + 0x2c4u);
    wr32(m, ecx + edx * 4 + 0x184u, eax);
    wr32(m, ecx + 0x2c4u, (rd32(m, ecx + 0x2c4u) + 1u));
L_8281b7:
    LIFT_RET(4);
}

// 008281c0 XMLReader__vf10
//   original XMLReader::vf10; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x008281c0, XMLReader__vf10, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_8281e0 = 0;
    uint32_t fB_8281e0 = 0;
    uint32_t fR_828200 = 0;
    uint32_t fA_82821c = 0;
    uint32_t fB_82821c = 0;
    uint32_t fA_828236 = 0;
    uint32_t fB_828236 = 0;
    uint32_t fA_82823e = 0;
    uint32_t fB_82823e = 0;
    eax = rd32(m, esp + 0x4u);
    edx = (eax + eax * 2 + 0x75u);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ecx + edx * 4);
    edx = rd32(m, ebx + 0x18u);
    eax = (eax + eax * 2);
    esp -= 4; wr32(m, esp, esi);
    esi = (ecx + eax * 4);
    eax = rd32(m, esi + 0x1d8u);
    esp -= 4; wr32(m, esp, edi);
    edi = (edx - 0x1u);
    fA_8281e0 = eax;
    fB_8281e0 = edi;
    if (int32_t(sext(fA_8281e0, 4)) < int32_t(sext(fB_8281e0, 4))) goto L_8281f9;
    edi = rd32(m, esp); esp += 4;
    wr32(m, esi + 0x1d8u, edx);
    esi = rd32(m, esp); esp += 4;
    wr32(m, ecx + 0x3cu, 0x0u);
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_8281f9:
    edi = rd32(m, esi + 0x1dcu);
    eax = (eax + 1u);
    fR_828200 = (edi & edi);
    if (!(fR_828200 == 0)) goto L_82821c;
    wr32(m, esi + 0x1d8u, eax);
    edx = (eax + eax * 8);
    eax = rd32(m, ebx + 0x14u);
    edi = rd32(m, esp); esp += 4;
    eax = (eax + edx * 4);
    esi = rd32(m, esp); esp += 4;
    wr32(m, ecx + 0x3cu, eax);
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_82821c:
    fA_82821c = eax;
    fB_82821c = edx;
    esp -= 4; wr32(m, esp, ebp);
    if (int32_t(sext(fA_82821c, 4)) >= int32_t(sext(fB_82821c, 4))) goto L_828242;
    ebp = rd32(m, ebx + 0x14u);
    edi = (eax + eax * 8);
    edi = (ebp + edi * 4);
    goto L_828230;
L_828230:
    ebp = rd32(m, esi + 0x1dcu);
    fA_828236 = ebp;
    fB_828236 = rd32(m, edi);
    if (fA_828236 == fB_828236) goto L_828258;
    eax = (eax + 1u);
    edi = edi + 0x24u;
    fA_82823e = eax;
    fB_82823e = edx;
    if (int32_t(sext(fA_82823e, 4)) < int32_t(sext(fB_82823e, 4))) goto L_828230;
L_828242:
    ebp = rd32(m, esp); esp += 4;
    wr32(m, esi + 0x1d8u, eax);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    wr32(m, ecx + 0x3cu, 0x0u);
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_828258:
    wr32(m, esi + 0x1d8u, eax);
    ebp = rd32(m, esp); esp += 4;
    edx = (eax + eax * 8);
    eax = rd32(m, ebx + 0x14u);
    edi = rd32(m, esp); esp += 4;
    eax = (eax + edx * 4);
    esi = rd32(m, esp); esp += 4;
    wr32(m, ecx + 0x3cu, eax);
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 00828ae0 XMLReader__vf58
//   original XMLReader::vf58; register inputs: ecx; returns with ret 8
REFLEX_PORT_FN(0x00828ae0, XMLReader__vf58, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_828ae6 = 0;
    uint32_t fR_828aed = 0;
    uint32_t fR_828af5 = 0;
    uint32_t fR_828b10 = 0;
    uint32_t fR_828b16 = 0;
    uint32_t fA_828b23 = 0;
    uint32_t fB_828b23 = 0;
    uint32_t fA_828b2b = 0;
    uint32_t fB_828b2b = 0;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, edi);
    fR_828ae6 = (ebx & ebx);
    if ((fR_828ae6 == 0)) goto L_828b2f;
    edi = rd32(m, ebx + 0x20u);
    fR_828aed = (edi & edi);
    if ((fR_828aed == 0)) goto L_828b2f;
    eax = rd32(m, esp + 0x10u);
    fR_828af5 = (eax & eax);
    if ((fR_828af5 == 0)) goto L_828b41;
    wr32(m, esp + 0xcu, eax);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, ecx + 0x10u);
    eax = (esp + 0x14u);
    esp -= 4; wr32(m, esp, eax);
    eax = (esp + 0x14u);
    LIFT_CALL(0x7fa030u, 0x828b0fu);  // FUN_007fa030
    esi = rd32(m, esp); esp += 4;
    fR_828b10 = (eax & eax);
    if ((fR_828b10 == 0)) goto L_828b2f;
    eax = 0u;
    fR_828b16 = (edi & edi);
    if (((fR_828b16 == 0) || sgn(fR_828b16, 4) != false)) goto L_828b2f;
    ebx = rd32(m, ebx + 0x1cu);
    edx = rd32(m, esp + 0x10u);
    ecx = ebx;
L_828b23:
    fA_828b23 = rd32(m, ecx);
    fB_828b23 = edx;
    if (fA_828b23 == fB_828b23) goto L_828b36;
    eax = (eax + 1u);
    ecx = ecx + 0xcu;
    fA_828b2b = eax;
    fB_828b2b = edi;
    if (int32_t(sext(fA_828b2b, 4)) < int32_t(sext(fB_828b2b, 4))) goto L_828b23;
L_828b2f:
    edi = rd32(m, esp); esp += 4;
    eax = 0u;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(8);
L_828b36:
    ecx = (eax + eax * 2);
    edi = rd32(m, esp); esp += 4;
    eax = (ebx + ecx * 4);
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(8);
L_828b41:
    eax = rd32(m, ebx + 0x1cu);
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 00828f90 XMLReader__vf68
//   original XMLReader::vf68; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x00828f90, XMLReader__vf68, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    eax = rd32(m, esp + 0x4u);
    edx = rd32(m, eax + 0x4u);
    eax = rd32(m, ecx + 0xcu);
    eax = rd32(m, eax + edx * 4);
    LIFT_RET(4);
}

// 00829080 XMLReader__vf77
//   original XMLReader::vf77; register inputs: ecx; returns with ret 12
REFLEX_PORT_FN(0x00829080, XMLReader__vf77, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_829088 = 0;
    uint32_t fB_829088 = 0;
    uint32_t fA_8290b3 = 0;
    uint32_t fB_8290b3 = 0;
    edx = rd32(m, esp + 0x4u);
    eax = rd16(m, edx + 0xau);
    fA_829088 = (eax & 0xffffu);
    fB_829088 = 0x3u;
    if (fA_829088 != fB_829088) goto L_8290b3;
    eax = rd32(m, esp + 0xcu);
    edx = rd32(m, edx + 0x4u);
    ecx = (eax * 4);
    eax = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x8fa070u, 0x8290a8u);  // MSVCR90.DLL::memcpy
    esp = esp + 0xcu;
    eax = 0x1u;
    LIFT_RET(12);
L_8290b3:
    fA_8290b3 = (eax & 0xffffu);
    fB_8290b3 = 0x1u;
    if (fA_8290b3 != fB_8290b3) goto L_8290dc;
    eax = rd32(m, ecx + 0xcu);
    ecx = rd32(m, esp + 0xcu);
    edx = rd32(m, edx + 0x4u);
    eax = rd32(m, eax + edx * 4);
    esp -= 4; wr32(m, esp, ecx);
    ecx = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x829560u, 0x8290d1u);  // FUN_00829560
    esp = esp + 0x8u;
    eax = 0x1u;
    LIFT_RET(12);
L_8290dc:
    eax = 0u;
    LIFT_RET(12);
}

// 00829160 XMLReader__vf76
//   original XMLReader::vf76; register inputs: ecx; returns with ret 12
REFLEX_PORT_FN(0x00829160, XMLReader__vf76, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_829168 = 0;
    uint32_t fB_829168 = 0;
    uint32_t fA_829193 = 0;
    uint32_t fB_829193 = 0;
    edx = rd32(m, esp + 0x4u);
    eax = rd16(m, edx + 0xau);
    fA_829168 = (eax & 0xffffu);
    fB_829168 = 0x5u;
    if (fA_829168 != fB_829168) goto L_829193;
    eax = rd32(m, esp + 0xcu);
    edx = rd32(m, edx + 0x4u);
    ecx = (eax * 4);
    eax = rd32(m, esp + 0x8u);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, eax);
    LIFT_CALL(0x8fa070u, 0x829188u);  // MSVCR90.DLL::memcpy
    esp = esp + 0xcu;
    eax = 0x1u;
    LIFT_RET(12);
L_829193:
    fA_829193 = (eax & 0xffffu);
    fB_829193 = 0x1u;
    if (fA_829193 != fB_829193) goto L_8291bc;
    eax = rd32(m, ecx + 0xcu);
    ecx = rd32(m, esp + 0xcu);
    edx = rd32(m, edx + 0x4u);
    eax = rd32(m, eax + edx * 4);
    esp -= 4; wr32(m, esp, ecx);
    ecx = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, ecx);
    LIFT_CALL(0x829640u, 0x8291b1u);  // FUN_00829640
    esp = esp + 0x8u;
    eax = 0x1u;
    LIFT_RET(12);
L_8291bc:
    eax = 0u;
    LIFT_RET(12);
}

// 00829460 XMLReader__vf69
//   original XMLReader::vf69; register inputs: ecx; returns with ret 12
REFLEX_PORT_FN(0x00829460, XMLReader__vf69, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_829468 = 0;
    uint32_t fB_829468 = 0;
    uint32_t fA_82948f = 0;
    uint32_t fB_82948f = 0;
    edx = rd32(m, esp + 0x4u);
    eax = rd16(m, edx + 0xau);
    fA_829468 = (eax & 0xffffu);
    fB_829468 = 0x8u;
    if (fA_829468 != fB_829468) goto L_82948f;
    eax = rd32(m, esp + 0xcu);
    ecx = rd32(m, edx + 0x4u);
    edx = rd32(m, esp + 0x8u);
    eax = (eax << 6);
    esp -= 4; wr32(m, esp, eax);
    esp -= 4; wr32(m, esp, ecx);
    esp -= 4; wr32(m, esp, edx);
    LIFT_CALL(0x8fa070u, 0x829484u);  // MSVCR90.DLL::memcpy
    esp = esp + 0xcu;
    eax = 0x1u;
    LIFT_RET(12);
L_82948f:
    fA_82948f = (eax & 0xffffu);
    fB_82948f = 0x1u;
    if (fA_82948f != fB_82948f) goto L_8294b8;
    eax = rd32(m, edx + 0x4u);
    ecx = rd32(m, ecx + 0xcu);
    edx = rd32(m, ecx + eax * 4);
    eax = rd32(m, esp + 0xcu);
    esp -= 4; wr32(m, esp, edx);
    esp -= 4; wr32(m, esp, eax);
    eax = rd32(m, esp + 0x10u);
    LIFT_CALL(0x829920u, 0x8294adu);  // FUN_00829920
    esp = esp + 0x8u;
    eax = 0x1u;
    LIFT_RET(12);
L_8294b8:
    eax = 0u;
    LIFT_RET(12);
}

// 0082af60 ActionContext__vf0
//   original ActionContext::vf0; register inputs: ecx; returns with ret 4
REFLEX_PORT_FN(0x0082af60, ActionContext__vf0, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_82af60 = 0;
    fR_82af60 = (rd8(m, esp + 0x4u) & 0x1u);
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    wr32(m, esi, 0x975e8cu);
    if ((fR_82af60 == 0)) goto L_82af79;
    esp -= 4; wr32(m, esp, esi);
    LIFT_CALL(0x78fc50u, 0x82af76u);  // ReflexAlignedFree
    esp = esp + 0x4u;
L_82af79:
    eax = esi;
    esi = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 0082b930 AnimKeys_Locate: ECX key track, xmm0 time (wrapped by +0x10): frame = trunc(time * rate +8), fraction; key pointers k / k+1 (clamped to +0x20) at +0x34 / +0x38; optional out (stack).
//   original FUN_0082b930; register inputs: ecx xmm0; returns with ret 4
REFLEX_PORT_FN(0x0082b930, AnimKeys_Locate, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    FCmp fC_82b938{};
    FCmp fC_82b944{};
    uint32_t fA_82b975 = 0;
    uint32_t fB_82b975 = 0;
    uint32_t fR_82b9a4 = 0;
    x1 = movss_m(rd32(m, ecx + 0x10u));
    esp = esp - 0x10u;
    fC_82b938 = fcmp32(x0.l(0), x1.l(0));
    if ((fc_b(fC_82b938) || fc_z(fC_82b938))) goto L_82b949;
    x2 = x1;
L_82b940:
    x0 = subss(x0, x1.l(0));
    fC_82b944 = fcmp32(x0.l(0), x2.l(0));
    if (!(fc_b(fC_82b944) || fc_z(fC_82b944))) goto L_82b940;
L_82b949:
    eax = rd32(m, ecx + 0x24u);
    x1 = cvtsi2ss(x1, rd32(m, ecx + 0x8u));
    eax = (eax + 1u);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ecx + 0x3cu);
    eax = (eax & 0xfffffffeu);
    esp -= 4; wr32(m, esp, ebp);
    ebp = rd32(m, ecx + 0x2cu);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    x1 = mulss(x1, x0.l(0));
    edx = cvttss2si(x1.l(0));
    esi = uint32_t(int32_t(esi) * int32_t(edx));
    esp -= 4; wr32(m, esp, edi);
    edi = (ebp + esi * 8);
    esi = (edx + 0x1u);
    edi = edi + ebx;
    fA_82b975 = esi;
    fB_82b975 = rd32(m, ecx + 0x20u);
    x2 = cvtsi2ss(x2, edx);
    x1 = subss(x1, x2.l(0));
    wr32(m, esp + 0x10u, x0.l(0));
    if (int32_t(sext(fA_82b975, 4)) >= int32_t(sext(fB_82b975, 4))) esi = edx;
    eax = uint32_t(int32_t(eax) * int32_t(esi));
    edx = rd32(m, esp + 0x24u);
    eax = (ebp + eax * 8);
    eax = eax + ebx;
    wr32(m, esp + 0x18u, edi);
    wr32(m, esp + 0x14u, x1.l(0));
    wr32(m, esp + 0x1cu, eax);
    fR_82b9a4 = (edx & edx);
    if ((fR_82b9a4 == 0)) goto L_82b9bd;
    x2 = movsd_m(rd64(m, esp + 0x10u));
    wr64(m, edx, x2.lo);
    x2 = movsd_m(rd64(m, esp + 0x18u));
    wr64(m, edx + 0x8u, x2.lo);
L_82b9bd:
    wr32(m, ecx + 0x34u, edi);
    edi = rd32(m, esp); esp += 4;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    wr32(m, ecx + 0x14u, x0.l(0));
    wr32(m, ecx + 0x18u, x1.l(0));
    wr32(m, ecx + 0x38u, eax);
    ebx = rd32(m, esp); esp += 4;
    esp = esp + 0x10u;
    LIFT_RET(4);
}

// 0082b9e0 AnimKeys_SampleRotation: ECX track, EDX bone: the two keys' int16x4 quaternions (x 1/32767) slerped (0042ca00) by the track fraction +4, stored at ESI.
//   original FUN_0082b9e0; register inputs: ecx edx esi; returns with ret 0
REFLEX_PORT_FN(0x0082b9e0, AnimKeys_SampleRotation, "engine", LIFT_OUTS) {
    LIFT_ENTRY();

    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x1cu;
    eax = rd32(m, ecx + 0x8u);
    x0 = movss_m(rd32(m, 0x92e054u) /* 3.0518509447574615e-05 */);
    edx = edx + edx;
    edx = edx + edx;
    edx = edx + edx;
    eax = eax + edx;
    esp -= 4; wr32(m, esp, edi);
    edi = sext(rd16(m, eax), 2);
    x1 = cvtsi2ss(x1, edi);
    edi = sext(rd16(m, eax + 0x2u), 2);
    x1 = mulss(x1, x0.l(0));
    wr32(m, esp + 0x10u, x1.l(0));
    x1 = cvtsi2ss(x1, edi);
    edi = sext(rd16(m, eax + 0x4u), 2);
    eax = sext(rd16(m, eax + 0x6u), 2);
    x1 = mulss(x1, x0.l(0));
    wr32(m, esp + 0x14u, x1.l(0));
    x2 = movss_m(rd32(m, ecx + 0x4u));
    x1 = cvtsi2ss(x1, edi);
    x1 = mulss(x1, x0.l(0));
    wr32(m, esp + 0x18u, x1.l(0));
    x1 = cvtsi2ss(x1, eax);
    eax = rd32(m, ecx + 0xcu);
    x1 = mulss(x1, x0.l(0));
    wr32(m, esp + 0x1cu, x1.l(0));
    x3 = ldx(m, esp + 0x10u);
    eax = eax + edx;
    edx = sext(rd16(m, eax), 2);
    x1 = cvtsi2ss(x1, edx);
    edx = sext(rd16(m, eax + 0x2u), 2);
    x1 = mulss(x1, x0.l(0));
    wr32(m, esp + 0x10u, x1.l(0));
    x1 = cvtsi2ss(x1, edx);
    edx = sext(rd16(m, eax + 0x4u), 2);
    eax = sext(rd16(m, eax + 0x6u), 2);
    x1 = mulss(x1, x0.l(0));
    wr32(m, esp + 0x14u, x1.l(0));
    x1 = cvtsi2ss(x1, edx);
    x1 = mulss(x1, x0.l(0));
    wr32(m, esp + 0x18u, x1.l(0));
    x1 = cvtsi2ss(x1, eax);
    x1 = mulss(x1, x0.l(0));
    wr32(m, esp + 0x1cu, x1.l(0));
    x0 = ldx(m, esp + 0x10u);
    x1 = x0;
    x2 = shufps(x2, x2, 0x0);
    x0 = x3;
    LIFT_CALL(0x42ca00u, 0x82bab0u);  // FUN_0042ca00
    stx(m, esi, x0);
    edi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0082bac0 AnimKeys_SampleTranslation: ECX bone, EDX out vec3, EDI time/fraction, stack track: the static translation, or the lerp of the two keys of the animated bone.
//   original FUN_0082bac0; register inputs: ecx edx edi; returns with ret 4
REFLEX_PORT_FN(0x0082bac0, AnimKeys_SampleTranslation, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_82bacd = 0;
    uint32_t fB_82bacd = 0;
    uint32_t fA_82bb11 = 0;
    uint32_t fB_82bb11 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0xcu;
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, ebp + 0x8u);
    fA_82bacd = ecx;
    fB_82bacd = rd32(m, esi + 0x4u);
    if (fA_82bacd == fB_82bacd) goto L_82baf4;
    eax = rd32(m, esi + 0x1cu);
    eax = eax + ecx;
    eax = (eax << 4);
    eax = eax + rd32(m, esi + 0x3cu);
    fpush(m, reflex_f80::from_f32(rd32(m, eax)));
    wr32(m, edx, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x4u)));
    wr32(m, edx + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x8u)));
    wr32(m, edx + 0x8u, fst32(m));
    fpop(m);
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_82baf4:
    x0 = cvtsi2ss(x0, rd32(m, esi + 0xcu));
    x0 = mulss(x0, rd32(m, edi));
    x1 = movss_m(rd32(m, edi + 0x4u));
    ecx = cvttss2si(x0.l(0));
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    eax = (ecx + 0x1u);
    fA_82bb11 = eax;
    fB_82bb11 = rd32(m, esi + 0x1cu);
    esi = rd32(m, esi + 0x3cu);
    if (int32_t(sext(fA_82bb11, 4)) > int32_t(sext(fB_82bb11, 4))) eax = ecx;
    ecx = (ecx << 4);
    x2 = movss_m(rd32(m, ecx + esi));
    x3 = movss_m(rd32(m, ecx + esi + 0x4u));
    x4 = movss_m(rd32(m, ecx + esi + 0x8u));
    ecx = ecx + esi;
    eax = (eax << 4);
    x5 = movss_m(rd32(m, eax + esi));
    x6 = movss_m(rd32(m, eax + esi + 0x4u));
    x7 = movss_m(rd32(m, eax + esi + 0x8u));
    eax = eax + esi;
    x0 = subss(x0, x1.l(0));
    x5 = mulss(x5, x1.l(0));
    x6 = mulss(x6, x1.l(0));
    x7 = mulss(x7, x1.l(0));
    x2 = mulss(x2, x0.l(0));
    x3 = mulss(x3, x0.l(0));
    x4 = mulss(x4, x0.l(0));
    x2 = addss(x2, x5.l(0));
    x3 = addss(x3, x6.l(0));
    x4 = addss(x4, x7.l(0));
    wr32(m, edx, x2.l(0));
    wr32(m, edx + 0x4u, x3.l(0));
    wr32(m, edx + 0x8u, x4.l(0));
    esi = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 0082e250 SkeletonPose_Reset: EAX skeleton (bone count +0x234), EDX pose: every 0x30-byte bone = identity rotation (0,0,0,1), zero translation, flags +0x20/+0x21 cleared.
//   original FUN_0082e250; register inputs: eax edx; returns with ret 0
REFLEX_PORT_FN(0x0082e250, SkeletonPose_Reset, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_82e25e = 0;
    uint32_t fB_82e25e = 0;
    uint32_t fA_82e2bc = 0;
    uint32_t fB_82e2bc = 0;
    eax = rd32(m, eax + 0x234u);
    ecx = (eax + eax * 2);
    ecx = (ecx << 4);
    ecx = ecx + edx;
    fA_82e25e = edx;
    fB_82e25e = ecx;
    if (fA_82e25e == fB_82e25e) goto L_82e2c1;
    eax = (edx + 0x18u);
    edx = (edx & 0xffffff00u) | ((0u) & 0xffu);
    esp -= 4; wr32(m, esp, esi);
    goto L_82e270;
L_82e270:
    wr8(m, eax + 0x8u, (edx & 0xffu));
    wr8(m, eax + 0x9u, (edx & 0xffu));
    x0 = movsd_m(rd64(m, 0x9cb550u));
    wr64(m, eax - 0x18u, x0.lo);
    x0 = movsd_m(rd64(m, 0x9cb558u));
    wr64(m, eax - 0x10u, x0.lo);
    x0 = movss_m(rd32(m, 0xa95d50u));
    wr32(m, eax - 0x8u, x0.l(0));
    x0 = movss_m(rd32(m, 0xa95d54u));
    wr32(m, eax - 0x4u, x0.l(0));
    x0 = movss_m(rd32(m, 0xa95d58u));
    wr32(m, eax, x0.l(0));
    eax = eax + 0x30u;
    esi = (eax - 0x18u);
    fA_82e2bc = esi;
    fB_82e2bc = ecx;
    if (fA_82e2bc != fB_82e2bc) goto L_82e270;
    esi = rd32(m, esp); esp += 4;
L_82e2c1:
    LIFT_RET(0);
}

// 0082e2d0 SkeletonPosePool_Acquire: EDI owner: under the critical section +0x208, the first free slot of +0x22c (count +0x230) is marked used; returns the reset pose (+0x228 + i * bones * 0x30).
//   original FUN_0082e2d0; register inputs: edi; returns with ret 0
REFLEX_PORT_FN(0x0082e2d0, SkeletonPosePool_Acquire, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_82e2ee = 0;
    uint32_t fA_82e300 = 0;
    uint32_t fB_82e300 = 0;
    uint32_t fA_82e309 = 0;
    uint32_t fB_82e309 = 0;
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, ebp);
    esp -= 4; wr32(m, esp, esi);
    ebx = (edi + 0x208u);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(rd32(m, 0x91600cu) /* 8.29485173491818e-39 */, 0x82e2e0u);
    eax = rd32(m, edi + 0x230u);
    ebp = rd32(m, edi + 0x228u);
    esi = 0u;
    fR_82e2ee = (eax & eax);
    if ((false || (fR_82e2ee == 0))) goto L_82e30d;
    edx = rd32(m, edi + 0x22cu);
    ecx = edx;
L_82e300:
    fA_82e300 = rd32(m, ecx);
    fB_82e300 = 0x0u;
    if (fA_82e300 == fB_82e300) goto L_82e31a;
    esi = (esi + 1u);
    ecx = ecx + 0x4u;
    fA_82e309 = esi;
    fB_82e309 = eax;
    if (fA_82e309 < fB_82e309) goto L_82e300;
L_82e30d:
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x82e314u);
    esi = rd32(m, esp); esp += 4;
    eax = ebp;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_82e31a:
    esp -= 4; wr32(m, esp, ebx);
    wr32(m, edx + esi * 4, 0x1u);
    LIFT_CALL(rd32(m, 0x9160f8u) /* 8.294818103755036e-39 */, 0x82e328u);
    eax = rd32(m, edi + 0x234u);
    eax = uint32_t(int32_t(eax) * int32_t(esi));
    esi = (eax + eax * 2);
    esi = (esi << 4);
    esi = esi + rd32(m, edi + 0x228u);
    eax = edi;
    edx = esi;
    LIFT_CALL(0x82e250u, 0x82e346u);  // FUN_0082e250
    eax = esi;
    esi = rd32(m, esp); esp += 4;
    ebp = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 0082e610 Fn_0082e610
//   original FUN_0082e610; register inputs: ecx eax edi; returns with ret 4
REFLEX_PORT_FN(0x0082e610, Fn_0082e610, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_82e65f = 0;
    uint32_t fR_82ea6d = 0;
    FCmp fC_82eef7{};
    uint32_t fR_82ef05 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    x5 = movss_m(rd32(m, 0x9774a8u) /* -0.0 */);
    esp = esp - 0x58u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, esi);
    esi = eax;
    eax = (ecx + ecx * 2);
    eax = (eax << 4);
    x0 = movsd_m(rd64(m, eax + esi));
    wr64(m, edi, x0.lo);
    x0 = movsd_m(rd64(m, eax + esi + 0x8u));
    eax = eax + esi;
    wr64(m, edi + 0x8u, x0.lo);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x10u)));
    edx = rd32(m, ebx + 0x23cu);
    wr32(m, edi + 0x10u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x14u)));
    wr32(m, edi + 0x14u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x18u)));
    wr32(m, edi + 0x18u, fst32(m));
    fpop(m);
    ecx = rd32(m, edx + ecx * 4);
    fR_82e65f = (ecx & ecx);
    if ((sgn(fR_82e65f, 4) != false)) goto L_82ea75;
    goto L_82e678;
L_82e670:
    x5 = movss_m(rd32(m, 0x9774a8u) /* -0.0 */);
L_82e678:
    x1 = x5;
    x3 = x5;
    x6 = movss_m(rd32(m, edi + 0x10u));
    x6 = cvtps2pd(x6.lo);
    eax = (ecx + ecx * 2);
    eax = (eax << 4);
    x4 = movss_m(rd32(m, eax + esi + 0x8u));
    x0 = movss_m(rd32(m, eax + esi));
    x5 = subss(x5, x4.l(0));
    x2 = movss_m(rd32(m, eax + esi + 0x4u));
    wr32(m, esp + 0x48u, x5.l(0));
    x5 = movss_m(rd32(m, eax + esi + 0xcu));
    x7 = cvtps2pd(x5.lo);
    x7 = mulsd(x7, rd64(m, 0x977548u) /* 0.0 */);
    wr32(m, esp + 0x4cu, x5.l(0));
    eax = eax + esi;
    x5 = cvtps2pd(x0.lo);
    x5 = mulsd(x5, x6.lo);
    x7 = subsd(x7, x5.lo);
    x5 = movss_m(rd32(m, edi + 0x14u));
    x5 = cvtps2pd(x5.lo);
    x6 = cvtps2pd(x2.lo);
    x6 = mulsd(x6, x5.lo);
    x5 = movss_m(rd32(m, edi + 0x18u));
    x7 = subsd(x7, x6.lo);
    x5 = cvtps2pd(x5.lo);
    x6 = cvtps2pd(x4.lo);
    x6 = mulsd(x6, x5.lo);
    x7 = subsd(x7, x6.lo);
    x5 = cvtsd2ss(x5, x7.lo);
    wr32(m, esp + 0x2cu, x5.l(0));
    x5 = movss_m(rd32(m, eax + 0xcu));
    x6 = cvtps2pd(x5.lo);
    x5 = movss_m(rd32(m, edi + 0x10u));
    x5 = cvtps2pd(x5.lo);
    x6 = mulsd(x6, x5.lo);
    ecx = rd32(m, edx + ecx * 4);
    x5 = cvtps2pd(x0.lo);
    x5 = mulsd(x5, rd64(m, 0x977548u) /* 0.0 */);
    x6 = addsd(x6, x5.lo);
    x5 = movss_m(rd32(m, edi + 0x18u));
    x5 = cvtps2pd(x5.lo);
    x7 = cvtps2pd(x2.lo);
    x7 = mulsd(x7, x5.lo);
    x5 = movss_m(rd32(m, edi + 0x14u));
    x6 = addsd(x6, x7.lo);
    x5 = cvtps2pd(x5.lo);
    x7 = cvtps2pd(x4.lo);
    x7 = mulsd(x7, x5.lo);
    x6 = subsd(x6, x7.lo);
    x5 = cvtpd2ps(x6);
    wr32(m, esp + 0x20u, x5.l(0));
    x5 = movss_m(rd32(m, eax + 0xcu));
    x6 = cvtps2pd(x5.lo);
    x5 = movss_m(rd32(m, edi + 0x14u));
    x5 = cvtps2pd(x5.lo);
    x6 = mulsd(x6, x5.lo);
    x5 = cvtps2pd(x2.lo);
    x5 = mulsd(x5, rd64(m, 0x977548u) /* 0.0 */);
    x6 = addsd(x6, x5.lo);
    x5 = movss_m(rd32(m, edi + 0x10u));
    x5 = cvtps2pd(x5.lo);
    x7 = cvtps2pd(x4.lo);
    x7 = mulsd(x7, x5.lo);
    x5 = movss_m(rd32(m, edi + 0x18u));
    x6 = addsd(x6, x7.lo);
    x5 = cvtps2pd(x5.lo);
    x7 = cvtps2pd(x0.lo);
    x7 = mulsd(x7, x5.lo);
    x5 = movss_m(rd32(m, eax + 0xcu));
    x6 = subsd(x6, x7.lo);
    x7 = cvtps2pd(x5.lo);
    x5 = movss_m(rd32(m, edi + 0x18u));
    x6 = cvtpd2ps(x6);
    x5 = cvtps2pd(x5.lo);
    x4 = cvtps2pd(x4.lo);
    x4 = mulsd(x4, rd64(m, 0x977548u) /* 0.0 */);
    x1 = subss(x1, x0.l(0));
    x3 = subss(x3, x2.l(0));
    wr32(m, esp + 0x24u, x6.l(0));
    x7 = mulsd(x7, x5.lo);
    x7 = addsd(x7, x4.lo);
    x4 = cvtps2pd(x0.lo);
    x0 = movss_m(rd32(m, edi + 0x14u));
    x0 = cvtps2pd(x0.lo);
    x4 = mulsd(x4, x0.lo);
    x0 = movss_m(rd32(m, edi + 0x10u));
    x0 = cvtps2pd(x0.lo);
    x7 = addsd(x7, x4.lo);
    x4 = movss_m(rd32(m, esp + 0x2cu));
    x2 = cvtps2pd(x2.lo);
    x2 = mulsd(x2, x0.lo);
    x7 = subsd(x7, x2.lo);
    x2 = movss_m(rd32(m, esp + 0x4cu));
    x0 = cvtsd2ss(x0, x7.lo);
    wr32(m, esp + 0x28u, x0.l(0));
    x0 = cvtss2sd(x0, rd32(m, esp + 0x20u));
    x5 = cvtps2pd(x2.lo);
    x5 = mulsd(x5, x0.lo);
    x6 = cvtss2sd(x6, x6.l(0));
    x7 = cvtss2sd(x7, x1.l(0));
    x0 = cvtps2pd(x4.lo);
    x0 = mulsd(x0, x7.lo);
    x5 = addsd(x5, x0.lo);
    x0 = movss_m(rd32(m, esp + 0x48u));
    x7 = cvtps2pd(x0.lo);
    x7 = mulsd(x7, x6.lo);
    x5 = addsd(x5, x7.lo);
    x7 = cvtss2sd(x7, rd32(m, esp + 0x28u));
    x6 = cvtss2sd(x6, x3.l(0));
    x6 = mulsd(x6, x7.lo);
    x5 = subsd(x5, x6.lo);
    x6 = cvtss2sd(x6, rd32(m, esp + 0x24u));
    x5 = cvtpd2ps(x5);
    wr32(m, esp + 0x50u, x5.l(0));
    x5 = cvtps2pd(x2.lo);
    x5 = mulsd(x5, x6.lo);
    x6 = cvtss2sd(x6, x3.l(0));
    x7 = cvtps2pd(x4.lo);
    x6 = mulsd(x6, x7.lo);
    x5 = addsd(x5, x6.lo);
    x6 = cvtss2sd(x6, rd32(m, esp + 0x28u));
    x7 = cvtss2sd(x7, x1.l(0));
    x6 = mulsd(x6, x7.lo);
    x7 = cvtss2sd(x7, rd32(m, esp + 0x20u));
    x5 = addsd(x5, x6.lo);
    x6 = cvtps2pd(x0.lo);
    x0 = cvtps2pd(x0.lo);
    x2 = cvtps2pd(x2.lo);
    x6 = mulsd(x6, x7.lo);
    x5 = subsd(x5, x6.lo);
    x6 = cvtss2sd(x6, rd32(m, esp + 0x28u));
    x2 = mulsd(x2, x6.lo);
    x4 = cvtps2pd(x4.lo);
    x0 = mulsd(x0, x4.lo);
    x2 = addsd(x2, x0.lo);
    x0 = cvtss2sd(x0, x3.l(0));
    x3 = cvtss2sd(x3, rd32(m, esp + 0x20u));
    x0 = mulsd(x0, x3.lo);
    x2 = addsd(x2, x0.lo);
    x0 = cvtss2sd(x0, rd32(m, esp + 0x24u));
    x1 = cvtss2sd(x1, x1.l(0));
    x0 = mulsd(x0, x1.lo);
    x1 = movss_m(rd32(m, eax + 0x14u));
    x2 = subsd(x2, x0.lo);
    x0 = movss_m(rd32(m, eax + 0x10u));
    x0 = addss(x0, rd32(m, esp + 0x50u));
    x3 = cvtpd2ps(x2);
    x2 = movss_m(rd32(m, eax + 0x18u));
    wr32(m, edi + 0x10u, x0.l(0));
    x5 = cvtpd2ps(x5);
    x1 = addss(x1, x5.l(0));
    wr32(m, edi + 0x14u, x1.l(0));
    x2 = addss(x2, x3.l(0));
    wr32(m, edi + 0x18u, x2.l(0));
    x5 = movss_m(rd32(m, eax + 0x8u));
    x0 = movss_m(rd32(m, eax + 0xcu));
    x1 = movss_m(rd32(m, edi + 0xcu));
    x2 = movss_m(rd32(m, eax));
    x3 = movss_m(rd32(m, edi));
    x4 = movss_m(rd32(m, eax + 0x4u));
    wr32(m, esp + 0xcu, x5.l(0));
    x5 = cvtps2pd(x0.lo);
    x6 = cvtps2pd(x1.lo);
    x6 = mulsd(x6, x5.lo);
    x7 = cvtps2pd(x2.lo);
    x5 = cvtps2pd(x3.lo);
    x5 = mulsd(x5, x7.lo);
    x6 = subsd(x6, x5.lo);
    x5 = movss_m(rd32(m, edi + 0x4u));
    x5 = cvtps2pd(x5.lo);
    x7 = cvtps2pd(x4.lo);
    x5 = mulsd(x5, x7.lo);
    x6 = subsd(x6, x5.lo);
    x5 = movss_m(rd32(m, edi + 0x8u));
    x7 = cvtss2sd(x7, rd32(m, esp + 0xcu));
    x5 = cvtps2pd(x5.lo);
    x5 = mulsd(x5, x7.lo);
    x6 = subsd(x6, x5.lo);
    x5 = cvtpd2ps(x6);
    wr32(m, esp + 0x3cu, x5.l(0));
    x5 = cvtps2pd(x0.lo);
    x6 = cvtps2pd(x3.lo);
    x6 = mulsd(x6, x5.lo);
    x5 = cvtps2pd(x1.lo);
    x7 = cvtps2pd(x2.lo);
    x5 = mulsd(x5, x7.lo);
    x6 = addsd(x6, x5.lo);
    x5 = movss_m(rd32(m, edi + 0x8u));
    x5 = cvtps2pd(x5.lo);
    x7 = cvtps2pd(x4.lo);
    x5 = mulsd(x5, x7.lo);
    x7 = cvtss2sd(x7, rd32(m, esp + 0xcu));
    x6 = addsd(x6, x5.lo);
    x5 = movss_m(rd32(m, edi + 0x4u));
    x5 = cvtps2pd(x5.lo);
    x7 = mulsd(x7, x5.lo);
    x6 = subsd(x6, x7.lo);
    x5 = cvtpd2ps(x6);
    wr32(m, esp + 0x30u, x5.l(0));
    x5 = cvtps2pd(x1.lo);
    x7 = cvtps2pd(x0.lo);
    x6 = cvtps2pd(x4.lo);
    x6 = mulsd(x6, x5.lo);
    x5 = movss_m(rd32(m, edi + 0x4u));
    x5 = cvtps2pd(x5.lo);
    x5 = mulsd(x5, x7.lo);
    x6 = addsd(x6, x5.lo);
    x5 = cvtss2sd(x5, rd32(m, esp + 0xcu));
    x7 = cvtps2pd(x3.lo);
    x5 = mulsd(x5, x7.lo);
    x6 = addsd(x6, x5.lo);
    x5 = movss_m(rd32(m, edi + 0x8u));
    x5 = cvtps2pd(x5.lo);
    x7 = cvtps2pd(x2.lo);
    x5 = mulsd(x5, x7.lo);
    x6 = subsd(x6, x5.lo);
    x1 = cvtps2pd(x1.lo);
    x0 = cvtps2pd(x0.lo);
    x5 = cvtpd2ps(x6);
    wr32(m, esp + 0x34u, x5.l(0));
    x5 = cvtss2sd(x5, rd32(m, esp + 0xcu));
    x5 = mulsd(x5, x1.lo);
    x1 = movss_m(rd32(m, edi + 0x8u));
    x1 = cvtps2pd(x1.lo);
    x1 = mulsd(x1, x0.lo);
    x0 = movss_m(rd32(m, edi + 0x4u));
    x0 = cvtps2pd(x0.lo);
    x5 = addsd(x5, x1.lo);
    x1 = cvtps2pd(x2.lo);
    x0 = mulsd(x0, x1.lo);
    x5 = addsd(x5, x0.lo);
    x0 = cvtps2pd(x4.lo);
    x1 = cvtps2pd(x3.lo);
    x0 = mulsd(x0, x1.lo);
    x5 = subsd(x5, x0.lo);
    x0 = cvtpd2ps(x5);
    wr32(m, esp + 0x38u, x0.l(0));
    x0 = movsd_m(rd64(m, esp + 0x30u));
    wr64(m, edi, x0.lo);
    x0 = movsd_m(rd64(m, esp + 0x38u));
    wr64(m, edi + 0x8u, x0.lo);
    fR_82ea6d = (ecx & ecx);
    if ((sgn(fR_82ea6d, 4) == false)) goto L_82e670;
L_82ea75:
    eax = rd32(m, ebx + 0x240u);
    eax = rd32(m, eax);
    ebx = rd8(m, eax + 0xbu);
    eax = rd32(m, eax);
    ebx = (ebx << 6);
    ebx = ebx + rd32(m, eax + 0x18u);
    esi = (esp + 0x20u);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x401740u, 0x82ea93u);  // FUN_00401740
    x0 = movss_m(rd32(m, ebx + 0x34u));
    x2 = movss_m(rd32(m, esp + 0x28u));
    x1 = movss_m(rd32(m, esp + 0x24u));
    x5 = movss_m(rd32(m, edi + 0x10u));
    x5 = cvtps2pd(x5.lo);
    wr32(m, esp + 0x58u, x0.l(0));
    x0 = movss_m(rd32(m, ebx + 0x38u));
    wr32(m, esp + 0x5cu, x0.l(0));
    x0 = movss_m(rd32(m, 0x9774a8u) /* -0.0 */);
    x3 = x0;
    x3 = subss(x3, x2.l(0));
    x4 = x0;
    wr32(m, esp + 0x38u, x3.l(0));
    x3 = movss_m(rd32(m, esp + 0x2cu));
    x0 = subss(x0, x3.l(0));
    wr32(m, esp + 0x3cu, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x30u));
    x6 = cvtps2pd(x0.lo);
    x6 = mulsd(x6, rd64(m, 0x977548u) /* 0.0 */);
    x7 = cvtps2pd(x1.lo);
    x7 = mulsd(x7, x5.lo);
    x5 = movss_m(rd32(m, edi + 0x14u));
    x6 = subsd(x6, x7.lo);
    x5 = cvtps2pd(x5.lo);
    x7 = cvtps2pd(x2.lo);
    x7 = mulsd(x7, x5.lo);
    x5 = movss_m(rd32(m, edi + 0x18u));
    x6 = subsd(x6, x7.lo);
    x5 = cvtps2pd(x5.lo);
    x7 = cvtps2pd(x3.lo);
    x7 = mulsd(x7, x5.lo);
    x6 = subsd(x6, x7.lo);
    x5 = cvtsd2ss(x5, x6.lo);
    wr32(m, esp + 0x30u, x5.l(0));
    x5 = movss_m(rd32(m, edi + 0x10u));
    x5 = cvtps2pd(x5.lo);
    x6 = cvtps2pd(x0.lo);
    x6 = mulsd(x6, x5.lo);
    x5 = cvtps2pd(x1.lo);
    x5 = mulsd(x5, rd64(m, 0x977548u) /* 0.0 */);
    x6 = addsd(x6, x5.lo);
    x5 = movss_m(rd32(m, edi + 0x18u));
    x5 = cvtps2pd(x5.lo);
    x7 = cvtps2pd(x2.lo);
    x7 = mulsd(x7, x5.lo);
    x5 = movss_m(rd32(m, edi + 0x14u));
    x6 = addsd(x6, x7.lo);
    x5 = cvtps2pd(x5.lo);
    x7 = cvtps2pd(x3.lo);
    x7 = mulsd(x7, x5.lo);
    x6 = subsd(x6, x7.lo);
    x5 = cvtsd2ss(x5, x6.lo);
    wr32(m, esp + 0x24u, x5.l(0));
    x5 = movss_m(rd32(m, edi + 0x14u));
    x5 = cvtps2pd(x5.lo);
    x6 = cvtps2pd(x0.lo);
    x6 = mulsd(x6, x5.lo);
    x5 = cvtps2pd(x2.lo);
    x5 = mulsd(x5, rd64(m, 0x977548u) /* 0.0 */);
    x6 = addsd(x6, x5.lo);
    x5 = movss_m(rd32(m, edi + 0x10u));
    x5 = cvtps2pd(x5.lo);
    x7 = cvtps2pd(x3.lo);
    x7 = mulsd(x7, x5.lo);
    x5 = movss_m(rd32(m, edi + 0x18u));
    x6 = addsd(x6, x7.lo);
    x7 = cvtps2pd(x1.lo);
    x5 = cvtps2pd(x5.lo);
    x7 = mulsd(x7, x5.lo);
    esp = esp + 0x4u;
    x4 = subss(x4, x1.l(0));
    x6 = subsd(x6, x7.lo);
    x5 = cvtsd2ss(x5, x6.lo);
    wr32(m, esp + 0x24u, x5.l(0));
    x5 = movss_m(rd32(m, edi + 0x18u));
    x5 = cvtps2pd(x5.lo);
    x6 = cvtps2pd(x0.lo);
    x6 = mulsd(x6, x5.lo);
    x5 = cvtps2pd(x3.lo);
    x5 = mulsd(x5, rd64(m, 0x977548u) /* 0.0 */);
    x6 = addsd(x6, x5.lo);
    x5 = movss_m(rd32(m, edi + 0x14u));
    x5 = cvtps2pd(x5.lo);
    x7 = cvtps2pd(x1.lo);
    x7 = mulsd(x7, x5.lo);
    x5 = movss_m(rd32(m, edi + 0x10u));
    x6 = addsd(x6, x7.lo);
    x5 = cvtps2pd(x5.lo);
    x7 = cvtps2pd(x2.lo);
    x7 = mulsd(x7, x5.lo);
    x6 = subsd(x6, x7.lo);
    x5 = cvtsd2ss(x5, x6.lo);
    x6 = cvtss2sd(x6, rd32(m, esp + 0x20u));
    wr32(m, esp + 0x28u, x5.l(0));
    x7 = cvtss2sd(x7, x4.l(0));
    x5 = cvtps2pd(x0.lo);
    x5 = mulsd(x5, x6.lo);
    x6 = cvtss2sd(x6, rd32(m, esp + 0x2cu));
    x6 = mulsd(x6, x7.lo);
    x5 = addsd(x5, x6.lo);
    x7 = cvtss2sd(x7, rd32(m, esp + 0x24u));
    x6 = cvtss2sd(x6, rd32(m, esp + 0x38u));
    x6 = mulsd(x6, x7.lo);
    x5 = addsd(x5, x6.lo);
    x7 = cvtss2sd(x7, rd32(m, esp + 0x28u));
    x6 = cvtss2sd(x6, rd32(m, esp + 0x34u));
    x6 = mulsd(x6, x7.lo);
    x5 = subsd(x5, x6.lo);
    x6 = cvtss2sd(x6, rd32(m, esp + 0x24u));
    x7 = cvtss2sd(x7, rd32(m, esp + 0x2cu));
    x5 = cvtpd2ps(x5);
    wr32(m, esp + 0x40u, x5.l(0));
    x5 = cvtps2pd(x0.lo);
    x5 = mulsd(x5, x6.lo);
    x6 = cvtss2sd(x6, rd32(m, esp + 0x34u));
    x6 = mulsd(x6, x7.lo);
    x5 = addsd(x5, x6.lo);
    x6 = cvtss2sd(x6, rd32(m, esp + 0x28u));
    x7 = cvtss2sd(x7, x4.l(0));
    x6 = mulsd(x6, x7.lo);
    x7 = cvtss2sd(x7, rd32(m, esp + 0x20u));
    x5 = addsd(x5, x6.lo);
    x6 = cvtss2sd(x6, rd32(m, esp + 0x38u));
    x6 = mulsd(x6, x7.lo);
    x7 = cvtss2sd(x7, rd32(m, esp + 0x2cu));
    x5 = subsd(x5, x6.lo);
    x6 = cvtss2sd(x6, rd32(m, esp + 0x28u));
    x5 = cvtpd2ps(x5);
    wr32(m, esp + 0x44u, x5.l(0));
    x5 = cvtps2pd(x0.lo);
    x5 = mulsd(x5, x6.lo);
    x6 = cvtss2sd(x6, rd32(m, esp + 0x38u));
    x6 = mulsd(x6, x7.lo);
    x7 = cvtss2sd(x7, rd32(m, esp + 0x20u));
    x5 = addsd(x5, x6.lo);
    x6 = cvtss2sd(x6, rd32(m, esp + 0x34u));
    x6 = mulsd(x6, x7.lo);
    x5 = addsd(x5, x6.lo);
    x6 = cvtss2sd(x6, rd32(m, esp + 0x24u));
    x4 = cvtss2sd(x4, x4.l(0));
    x6 = mulsd(x6, x4.lo);
    x4 = movss_m(rd32(m, esp + 0x40u));
    x5 = subsd(x5, x6.lo);
    x6 = cvtpd2ps(x5);
    x5 = movss_m(rd32(m, ebx + 0x30u));
    x4 = addss(x4, x5.l(0));
    x5 = movss_m(rd32(m, esp + 0x58u));
    x7 = x4;
    x4 = movss_m(rd32(m, esp + 0x54u));
    x4 = addss(x4, rd32(m, esp + 0x44u));
    x5 = addss(x5, x6.l(0));
    wr32(m, edi + 0x10u, x7.l(0));
    wr32(m, edi + 0x14u, x4.l(0));
    wr32(m, edi + 0x18u, x5.l(0));
    x4 = movss_m(rd32(m, edi + 0xcu));
    x5 = movss_m(rd32(m, edi));
    x5 = cvtps2pd(x5.lo);
    x6 = cvtps2pd(x0.lo);
    x7 = cvtps2pd(x4.lo);
    x6 = mulsd(x6, x7.lo);
    x7 = cvtps2pd(x1.lo);
    x5 = mulsd(x5, x7.lo);
    x6 = subsd(x6, x5.lo);
    x5 = movss_m(rd32(m, edi + 0x4u));
    x5 = cvtps2pd(x5.lo);
    x7 = cvtps2pd(x2.lo);
    x7 = mulsd(x7, x5.lo);
    x5 = movss_m(rd32(m, edi + 0x8u));
    x5 = cvtps2pd(x5.lo);
    x6 = subsd(x6, x7.lo);
    x7 = cvtps2pd(x3.lo);
    x7 = mulsd(x7, x5.lo);
    x6 = subsd(x6, x7.lo);
    x5 = cvtsd2ss(x5, x6.lo);
    wr32(m, esp + 0x3cu, x5.l(0));
    x5 = movss_m(rd32(m, edi));
    x5 = cvtps2pd(x5.lo);
    x6 = cvtps2pd(x0.lo);
    x6 = mulsd(x6, x5.lo);
    x5 = cvtps2pd(x1.lo);
    x7 = cvtps2pd(x4.lo);
    x5 = mulsd(x5, x7.lo);
    x6 = addsd(x6, x5.lo);
    x5 = movss_m(rd32(m, edi + 0x8u));
    x5 = cvtps2pd(x5.lo);
    x7 = cvtps2pd(x2.lo);
    x7 = mulsd(x7, x5.lo);
    x5 = movss_m(rd32(m, edi + 0x4u));
    x5 = cvtps2pd(x5.lo);
    x6 = addsd(x6, x7.lo);
    x7 = cvtps2pd(x3.lo);
    x7 = mulsd(x7, x5.lo);
    x6 = subsd(x6, x7.lo);
    x5 = cvtsd2ss(x5, x6.lo);
    wr32(m, esp + 0x30u, x5.l(0));
    x5 = movss_m(rd32(m, edi + 0x4u));
    x5 = cvtps2pd(x5.lo);
    x6 = cvtps2pd(x0.lo);
    x6 = mulsd(x6, x5.lo);
    x7 = cvtps2pd(x4.lo);
    x5 = cvtps2pd(x2.lo);
    x5 = mulsd(x5, x7.lo);
    x6 = addsd(x6, x5.lo);
    x5 = movss_m(rd32(m, edi));
    x5 = cvtps2pd(x5.lo);
    x7 = cvtps2pd(x3.lo);
    x7 = mulsd(x7, x5.lo);
    x5 = movss_m(rd32(m, edi + 0x8u));
    x5 = cvtps2pd(x5.lo);
    x6 = addsd(x6, x7.lo);
    x7 = cvtps2pd(x1.lo);
    x5 = mulsd(x5, x7.lo);
    x6 = subsd(x6, x5.lo);
    x5 = cvtsd2ss(x5, x6.lo);
    wr32(m, esp + 0x34u, x5.l(0));
    x5 = cvtps2pd(x0.lo);
    x0 = movss_m(rd32(m, edi + 0x8u));
    x0 = cvtps2pd(x0.lo);
    x5 = mulsd(x5, x0.lo);
    x0 = cvtps2pd(x3.lo);
    x3 = cvtps2pd(x4.lo);
    x0 = mulsd(x0, x3.lo);
    x5 = addsd(x5, x0.lo);
    x0 = movss_m(rd32(m, edi + 0x4u));
    x0 = cvtps2pd(x0.lo);
    x1 = cvtps2pd(x1.lo);
    x0 = mulsd(x0, x1.lo);
    x5 = addsd(x5, x0.lo);
    x0 = movss_m(rd32(m, edi));
    x0 = cvtps2pd(x0.lo);
    x1 = cvtps2pd(x2.lo);
    x1 = mulsd(x1, x0.lo);
    x5 = subsd(x5, x1.lo);
    x0 = cvtpd2ps(x5);
    wr32(m, esp + 0x38u, x0.l(0));
    x0 = movsd_m(rd64(m, esp + 0x30u));
    wr64(m, edi, x0.lo);
    x0 = movsd_m(rd64(m, esp + 0x38u));
    wr64(m, edi + 0x8u, x0.lo);
    x1 = movss_m(rd32(m, edi));
    x0 = movss_m(rd32(m, edi + 0xcu));
    x2 = movss_m(rd32(m, edi + 0x4u));
    x3 = movss_m(rd32(m, edi + 0x8u));
    wr32(m, esp + 0x10u, x0.l(0));
    x0 = cvtps2pd(x0.lo);
    wr32(m, esp + 0x14u, x1.l(0));
    x1 = cvtps2pd(x1.lo);
    x4 = x1;
    x4 = mulsd(x4, x1.lo);
    x1 = x0;
    x1 = mulsd(x1, x0.lo);
    wr32(m, esp + 0x18u, x2.l(0));
    x2 = cvtps2pd(x2.lo);
    x4 = addsd(x4, x1.lo);
    x0 = x2;
    x0 = mulsd(x0, x2.lo);
    x4 = addsd(x4, x0.lo);
    wr32(m, esp + 0x1cu, x3.l(0));
    x3 = cvtps2pd(x3.lo);
    x0 = x3;
    x0 = mulsd(x0, x3.lo);
    x4 = addsd(x4, x0.lo);
    x0 = cvtpd2ps(x4);
    fC_82eef7 = fcmp32(x0.l(0), rd32(m, 0x92e12cu) /* 0.0 */);
    eax = (eax & 0xffff00ffu) | ((fc_flags(fC_82eef7) | 2u) << 8);
    wr32(m, esp + 0xcu, x0.l(0));
    fR_82ef05 = (((eax >> 8) & 0xffu) & 0x44u);
    if (!parity8(fR_82ef05)) goto L_82ef8c;
    fpush(m, reflex_f80::from_f32(rd32(m, esp + 0xcu)));
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x42c230u, 0x82ef1bu);  // FUN_0042c230
    x2 = cvtss2sd(x2, rd32(m, esp + 0x14u));
    x1 = x0;
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    x0 = divss(x0, x1.l(0));
    x1 = cvtps2pd(x0.lo);
    x1 = mulsd(x1, x2.lo);
    x1 = cvtsd2ss(x1, x1.lo);
    wr32(m, edi + 0xcu, x1.l(0));
    x1 = cvtss2sd(x1, rd32(m, esp + 0x18u));
    x2 = cvtps2pd(x0.lo);
    x1 = mulsd(x1, x2.lo);
    x1 = cvtsd2ss(x1, x1.lo);
    wr32(m, edi, x1.l(0));
    x1 = cvtss2sd(x1, rd32(m, esp + 0x1cu));
    x2 = cvtps2pd(x0.lo);
    x1 = mulsd(x1, x2.lo);
    x1 = cvtsd2ss(x1, x1.lo);
    wr32(m, edi + 0x4u, x1.l(0));
    x1 = cvtss2sd(x1, rd32(m, esp + 0x20u));
    x0 = cvtps2pd(x0.lo);
    x1 = mulsd(x1, x0.lo);
    x0 = cvtsd2ss(x0, x1.lo);
    esp = esp + 0x4u;
    wr32(m, edi + 0x8u, x0.l(0));
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
L_82ef8c:
    x0 = X{};
    esi = rd32(m, esp); esp += 4;
    wr64(m, edi, x0.lo);
    wr64(m, edi + 0x8u, x0.lo);
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}

// 0082efb0 SkeletonPose_BoneToModel: Composes a bone's rotation / translation up its parent chain (parent indices skeleton+0x23c) into model space.
//   original FUN_0082efb0; register inputs: ecx edx edi; returns with ret 8
REFLEX_PORT_FN(0x0082efb0, SkeletonPose_BoneToModel, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fR_82f01c = 0;
    uint32_t fR_82f164 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0x88u;
    eax = rd32(m, 0x9b45e4u);
    eax = (eax ^ esp);
    wr32(m, esp + 0x84u, eax);
    eax = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0xcu);
    wr32(m, esp + 0x8u, eax);
    esp -= 4; wr32(m, esp, esi);
    esi = rd32(m, eax + 0x23cu);
    eax = (ecx + ecx * 2);
    eax = (eax << 4);
    x0 = movsd_m(rd64(m, eax + edx));
    wr64(m, ebx, x0.lo);
    x0 = movsd_m(rd64(m, eax + edx + 0x8u));
    eax = eax + edx;
    wr64(m, ebx + 0x8u, x0.lo);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x10u)));
    wr32(m, ebx + 0x10u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x14u)));
    wr32(m, ebx + 0x14u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x18u)));
    wr32(m, ebx + 0x18u, fst32(m));
    fpop(m);
    x0 = ldx(m, ebx + 0x10u);
    stx(m, edi, x0);
    eax = rd32(m, esi + ecx * 4);
    x0 = ldx(m, ebx);
    stx(m, esp + 0x20u, x0);
    fR_82f01c = (eax & eax);
    if ((sgn(fR_82f01c, 4) != false)) goto L_82f16c;
    x6 = ldx(m, 0x93a210u);
    goto L_82f030;
L_82f030:
    x2 = ldx(m, 0x92db80u);
    x1 = ldx(m, edi);
    x1 = andps(x1, x2);
    x2 = ldx(m, 0x93a220u);
    ecx = (eax + eax * 2);
    x3 = x1;
    x3 = shufps(x3, x1, 0x0);
    ecx = (ecx << 4);
    x0 = ldx(m, ecx + edx);
    x4 = x1;
    x4 = shufps(x4, x1, 0x55);
    ecx = ecx + edx;
    x7 = x1;
    x7 = shufps(x7, x1, 0xaa);
    x5 = x0;
    x5 = mulps(x5, x2);
    x2 = x5;
    x2 = shufps(x2, x5, 0x1b);
    x3 = mulps(x3, x2);
    x3 = mulps(x3, ldx(m, 0x93a1f0u));
    eax = rd32(m, esi + eax * 4);
    x2 = shufps(x2, x2, 0xb1);
    x4 = mulps(x4, x2);
    x4 = mulps(x4, ldx(m, 0x93a200u));
    x2 = shufps(x2, x2, 0x1b);
    x2 = mulps(x2, x7);
    x2 = mulps(x2, x6);
    x2 = addps(x2, x4);
    x4 = x1;
    x4 = shufps(x4, x1, 0xff);
    x4 = mulps(x4, x5);
    x4 = addps(x4, x3);
    x2 = addps(x2, x4);
    x3 = x0;
    x3 = shufps(x3, x0, 0x0);
    x7 = x0;
    x7 = shufps(x7, x0, 0xaa);
    stx(m, esp + 0x10u, x7);
    x1 = x2;
    x1 = shufps(x1, x2, 0x1b);
    x5 = x3;
    x5 = mulps(x5, x1);
    x5 = mulps(x5, ldx(m, 0x93a1f0u));
    x1 = shufps(x1, x1, 0xb1);
    x7 = x1;
    x7 = shufps(x7, x1, 0x1b);
    x7 = mulps(x7, ldx(m, esp + 0x10u));
    x7 = mulps(x7, ldx(m, 0x93a210u));
    x4 = x0;
    x4 = shufps(x4, x0, 0x55);
    x6 = x4;
    x6 = mulps(x6, x1);
    x6 = mulps(x6, ldx(m, 0x93a200u));
    x0 = shufps(x0, x0, 0xff);
    x1 = x0;
    x1 = mulps(x1, x2);
    x2 = ldx(m, esp + 0x20u);
    x1 = addps(x1, x5);
    x7 = addps(x7, x6);
    x7 = addps(x7, x1);
    x1 = ldx(m, ecx + 0x10u);
    x7 = addps(x7, x1);
    x1 = x2;
    x1 = shufps(x1, x2, 0x1b);
    x3 = mulps(x3, x1);
    x1 = shufps(x1, x1, 0xb1);
    stx(m, edi, x7);
    x6 = ldx(m, 0x93a210u);
    x3 = mulps(x3, ldx(m, 0x93a1f0u));
    x5 = x1;
    x5 = shufps(x5, x1, 0x1b);
    x5 = mulps(x5, ldx(m, esp + 0x10u));
    x4 = mulps(x4, x1);
    x4 = mulps(x4, ldx(m, 0x93a200u));
    x5 = mulps(x5, x6);
    x0 = mulps(x0, x2);
    x5 = addps(x5, x4);
    x0 = addps(x0, x3);
    x5 = addps(x5, x0);
    stx(m, esp + 0x20u, x5);
    fR_82f164 = (eax & eax);
    if ((sgn(fR_82f164, 4) == false)) goto L_82f030;
L_82f16c:
    eax = rd32(m, esp + 0xcu);
    ecx = rd32(m, eax + 0x240u);
    edx = rd32(m, ecx);
    eax = rd8(m, edx + 0xbu);
    edx = rd32(m, edx);
    eax = (eax << 6);
    eax = eax + rd32(m, edx + 0x18u);
    esi = (esp + 0x10u);
    wr32(m, esp + 0xcu, eax);
    eax = rd32(m, ecx);
    LIFT_CALL(0x7dd380u, 0x82f193u);  // FUN_007dd380
    x1 = ldx(m, esp + 0x10u);
    x2 = ldx(m, 0x92db80u);
    x0 = ldx(m, edi);
    x0 = andps(x0, x2);
    x2 = ldx(m, 0x93a220u);
    x3 = x0;
    x3 = shufps(x3, x0, 0x0);
    x4 = x0;
    x4 = shufps(x4, x0, 0x55);
    x6 = x0;
    x6 = shufps(x6, x0, 0xaa);
    x5 = x1;
    x5 = mulps(x5, x2);
    x2 = x5;
    x2 = shufps(x2, x5, 0x1b);
    x3 = mulps(x3, x2);
    x3 = mulps(x3, ldx(m, 0x93a1f0u));
    x2 = shufps(x2, x2, 0xb1);
    x4 = mulps(x4, x2);
    x4 = mulps(x4, ldx(m, 0x93a200u));
    x2 = shufps(x2, x2, 0x1b);
    x2 = mulps(x2, x6);
    x2 = mulps(x2, ldx(m, 0x93a210u));
    x2 = addps(x2, x4);
    x4 = x0;
    x4 = shufps(x4, x0, 0xff);
    x4 = mulps(x4, x5);
    x4 = addps(x4, x3);
    x2 = addps(x2, x4);
    x3 = x1;
    x3 = shufps(x3, x1, 0x0);
    x5 = x3;
    x0 = x2;
    x0 = shufps(x0, x2, 0x1b);
    x5 = mulps(x5, x0);
    x5 = mulps(x5, ldx(m, 0x93a1f0u));
    x0 = shufps(x0, x0, 0xb1);
    x4 = x0;
    stx(m, esp + 0x10u, x4);
    x0 = x1;
    x0 = shufps(x0, x1, 0x55);
    x6 = x0;
    x6 = mulps(x6, x4);
    x4 = ldx(m, esp + 0x20u);
    x6 = mulps(x6, ldx(m, 0x93a200u));
    x4 = shufps(x4, x4, 0x1b);
    x7 = x4;
    x7 = mulps(x7, x3);
    x4 = shufps(x4, x4, 0xb1);
    x3 = x4;
    x4 = ldx(m, 0x93a1f0u);
    stx(m, esp + 0x30u, x3);
    x3 = mulps(x3, x0);
    x0 = ldx(m, 0x93a200u);
    stx(m, esp + 0x50u, x3);
    x3 = ldx(m, esp + 0x10u);
    stx(m, esp + 0x40u, x0);
    x0 = ldx(m, 0x93a210u);
    stx(m, esp + 0x70u, x4);
    x4 = x1;
    stx(m, esp + 0x60u, x0);
    x4 = shufps(x4, x1, 0xaa);
    x3 = shufps(x3, x3, 0x1b);
    x1 = shufps(x1, x1, 0xff);
    x3 = mulps(x3, x4);
    x3 = mulps(x3, ldx(m, 0x93a210u));
    x0 = x1;
    x0 = mulps(x0, x2);
    x3 = addps(x3, x6);
    x0 = addps(x0, x5);
    eax = rd32(m, esp + 0xcu);
    x2 = movss_m(rd32(m, eax + 0x34u));
    x1 = mulps(x1, ldx(m, esp + 0x20u));
    x3 = addps(x3, x0);
    x0 = movss_m(rd32(m, eax + 0x38u));
    x0 = shufps(x0, x0, 0x0);
    x2 = shufps(x2, x2, 0x0);
    x2 = shufps(x2, x0, 0xee);
    x0 = movss_m(rd32(m, eax + 0x30u));
    x0 = shufps(x0, x0, 0x0);
    x2 = movss_r(x2, x0);
    x0 = x2;
    x2 = ldx(m, esp + 0x40u);
    x2 = mulps(x2, ldx(m, esp + 0x50u));
    x3 = addps(x3, x0);
    x0 = ldx(m, esp + 0x30u);
    x0 = shufps(x0, x0, 0x1b);
    x0 = mulps(x0, x4);
    x0 = mulps(x0, ldx(m, esp + 0x60u));
    x0 = addps(x0, x2);
    x2 = ldx(m, esp + 0x70u);
    ecx = rd32(m, esp + 0x8cu);
    x2 = mulps(x2, x7);
    x1 = addps(x1, x2);
    x0 = addps(x0, x1);
    x4 = x0;
    x4 = mulps(x4, x0);
    x1 = x4;
    x1 = shufps(x1, x4, 0xee);
    x2 = x1;
    x2 = addps(x2, x4);
    x2 = shufps(x2, x2, 0x40);
    x1 = shufps(x1, x2, 0xf0);
    x1 = addps(x1, x2);
    stx(m, edi, x3);
    x4 = ldx(m, 0x92dc20u);
    x1 = shufps(x1, x1, 0xaa);
    x2 = sqrtps(x2, x1);
    x1 = cmpps<4>(x1, x4);
    x4 = ldx(m, 0x92dc30u);
    stx(m, ebx + 0x10u, x3);
    x0 = divps(x0, x2);
    x3 = X{};
    x2 = cmpps<4>(x2, x3);
    x0 = andps(x0, x2);
    x0 = andps(x0, x1);
    x1 = andnps(x1, x4);
    x0 = orps(x0, x1);
    esi = rd32(m, esp); esp += 4;
    stx(m, ebx, x0);
    ebx = rd32(m, esp); esp += 4;
    ecx = (ecx ^ esp);
    eax = edi;
    LIFT_CALL(0x8f9efeu, 0x82f37du);  // __security_check_cookie
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(8);
}

// 0082ff20 Fn_0082ff20
//   original FUN_0082ff20; register inputs: ecx edi; returns with ret 0
REFLEX_PORT_FN(0x0082ff20, Fn_0082ff20, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_82ff60 = 0;
    uint32_t fB_82ff60 = 0;
    uint32_t fA_82ff7a = 0;
    uint32_t fB_82ff7a = 0;
    uint32_t fA_82ff9a = 0;
    uint32_t fB_82ff9a = 0;
    uint32_t fA_82ffa0 = 0;
    uint32_t fB_82ffa0 = 0;
    uint32_t fA_830009 = 0;
    uint32_t fB_830009 = 0;
    uint32_t fA_83001f = 0;
    uint32_t fB_83001f = 0;
    uint32_t fA_83007e = 0;
    uint32_t fB_83007e = 0;
    uint32_t fA_83008c = 0;
    uint32_t fB_83008c = 0;
    uint32_t fA_8300aa = 0;
    uint32_t fB_8300aa = 0;
    uint32_t fA_830113 = 0;
    uint32_t fB_830113 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    edx = rd32(m, 0xa95844u);
    edx = rd32(m, edx + 0x10u);
    fpush(m, reflex_f80::from_f32(rd32(m, edx + 0x18u)));
    esp = esp - 0x28u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, edi + 0x228u);
    esp -= 4; wr32(m, esp, esi);
    esi = ecx;
    ecx = rd32(m, edi + 0x224u);
    eax = rd32(m, ecx);
    eax = rd32(m, eax + 0xcu);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(eax, 0x82ff50u);
    ecx = rd32(m, edi + 0x224u);
    edx = rd32(m, ecx);
    eax = rd32(m, edx + 0x10u);
    esp -= 4; wr32(m, esp, ebx);
    esp -= 4; wr32(m, esp, esi);
    esp -= 4; wr32(m, esp, edi);
    LIFT_CALL(eax, 0x82ff60u);
    fA_82ff60 = rd8(m, edi + 0x1f4u);
    fB_82ff60 = 0x0u;
    wr8(m, edi + 0x204u, 0x0u);
    if (fA_82ff60 == fB_82ff60) goto L_83001b;
    eax = 0u;
    wr32(m, esp + 0x1cu, eax);
    fA_82ff7a = rd32(m, edi + 0x234u);
    fB_82ff7a = eax;
    if (fA_82ff7a <= fB_82ff7a) goto L_830122;
    wr32(m, esp + 0x14u, eax);
    wr32(m, esp + 0x18u, eax);
    edi = edi;
L_82ff90:
    ebx = rd32(m, edi + 0x228u);
    ebx = ebx + rd32(m, esp + 0x18u);
    fA_82ff9a = rd8(m, ebx + 0x21u);
    fB_82ff9a = 0x0u;
    if (fA_82ff9a != fB_82ff9a) goto L_82ffa6;
    fA_82ffa0 = rd8(m, ebx + 0x20u);
    fB_82ffa0 = 0x0u;
    if (fA_82ffa0 == fB_82ffa0) goto L_82fff6;
L_82ffa6:
    ecx = rd32(m, edi + 0x240u);
    esi = rd32(m, ecx + 0x8u);
    esi = esi + rd32(m, esp + 0x14u);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x7dcdf0u, 0x82ffb9u);  // FUN_007dcdf0
    eax = rd8(m, esi + 0xbu);
    x0 = ldx(m, ebx + 0x10u);
    edx = rd32(m, esi);
    eax = (eax << 6);
    eax = eax + rd32(m, edx + 0x18u);
    stx(m, esp + 0x20u, x0);
    ecx = esi;
    x0 = movss_m(rd32(m, esp + 0x20u));
    wr32(m, eax + 0x30u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x24u));
    wr32(m, eax + 0x34u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x28u));
    wr32(m, eax + 0x38u, x0.l(0));
    LIFT_CALL(0x7db7e0u, 0x82fff6u);  // FUN_007db7e0
L_82fff6:
    eax = rd32(m, esp + 0x1cu);
    wr32(m, esp + 0x18u, rd32(m, esp + 0x18u) + 0x30u);
    wr32(m, esp + 0x14u, rd32(m, esp + 0x14u) + 0x10u);
    eax = (eax + 1u);
    wr32(m, esp + 0x1cu, eax);
    fA_830009 = eax;
    fB_830009 = rd32(m, edi + 0x234u);
    if (fA_830009 < fB_830009) goto L_82ff90;
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
L_83001b:
    ebx = 0u;
    edx = 0u;
    fA_83001f = rd32(m, edi + 0x200u);
    fB_83001f = ebx;
    if (int32_t(sext(fA_83001f, 4)) <= int32_t(sext(fB_83001f, 4))) goto L_830086;
    esi = 0u;
L_830030:
    eax = rd32(m, edi + 0x1fcu);
    eax = rd32(m, eax + edx * 4);
    ecx = rd32(m, edi + 0x1f8u);
    eax = (eax + eax * 2);
    eax = (eax << 4);
    eax = eax + rd32(m, edi + 0x228u);
    ecx = (esi + ecx + 0x10u);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x10u)));
    edx = (edx + 1u);
    wr32(m, ecx, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x14u)));
    wr32(m, ecx + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x18u)));
    wr32(m, ecx + 0x8u, fst32(m));
    fpop(m);
    ecx = rd32(m, edi + 0x1f8u);
    x0 = movsd_m(rd64(m, eax));
    ecx = ecx + esi;
    wr64(m, ecx, x0.lo);
    x0 = movsd_m(rd64(m, eax + 0x8u));
    wr64(m, ecx + 0x8u, x0.lo);
    esi = esi + 0x30u;
    fA_83007e = edx;
    fB_83007e = rd32(m, edi + 0x200u);
    if (int32_t(sext(fA_83007e, 4)) < int32_t(sext(fB_83007e, 4))) goto L_830030;
L_830086:
    eax = 0u;
    wr32(m, esp + 0x1cu, eax);
    fA_83008c = rd32(m, edi + 0x234u);
    fB_83008c = ebx;
    if (fA_83008c <= fB_83008c) goto L_83011b;
    wr32(m, esp + 0x18u, ebx);
    wr32(m, esp + 0x14u, ebx);
L_8300a0:
    ebx = rd32(m, edi + 0x228u);
    ebx = ebx + rd32(m, esp + 0x14u);
    fA_8300aa = rd32(m, edi + eax * 4);
    fB_8300aa = 0x0u;
    if (fA_8300aa != fB_8300aa) goto L_830104;
    edx = rd32(m, edi + 0x240u);
    esi = rd32(m, edx + 0x8u);
    esi = esi + rd32(m, esp + 0x18u);
    esp -= 4; wr32(m, esp, ebx);
    LIFT_CALL(0x7dcdf0u, 0x8300c3u);  // FUN_007dcdf0
    eax = rd8(m, esi + 0xbu);
    ecx = rd32(m, esi);
    x0 = ldx(m, ebx + 0x10u);
    eax = (eax << 6);
    eax = eax + rd32(m, ecx + 0x18u);
    stx(m, esp + 0x20u, x0);
    ecx = esi;
    x0 = movss_m(rd32(m, esp + 0x20u));
    wr32(m, eax + 0x30u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x24u));
    wr32(m, eax + 0x34u, x0.l(0));
    x0 = movss_m(rd32(m, esp + 0x28u));
    wr32(m, eax + 0x38u, x0.l(0));
    LIFT_CALL(0x7db7e0u, 0x830100u);  // FUN_007db7e0
    eax = rd32(m, esp + 0x1cu);
L_830104:
    wr32(m, esp + 0x14u, rd32(m, esp + 0x14u) + 0x30u);
    wr32(m, esp + 0x18u, rd32(m, esp + 0x18u) + 0x10u);
    eax = (eax + 1u);
    wr32(m, esp + 0x1cu, eax);
    fA_830113 = eax;
    fB_830113 = rd32(m, edi + 0x234u);
    if (fA_830113 < fB_830113) goto L_8300a0;
L_83011b:
    wr8(m, edi + 0x204u, 0x1u);
L_830122:
    esi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(0);
}

// 00840040 Fn_00840040
//   original FUN_00840040; register inputs: ecx esi; returns with ret 4
REFLEX_PORT_FN(0x00840040, Fn_00840040, "engine", LIFT_OUTS) {
    LIFT_ENTRY();
    uint32_t fA_840063 = 0;
    uint32_t fB_840063 = 0;
    uint32_t fA_84026f = 0;
    uint32_t fB_84026f = 0;
    uint32_t fA_8402a9 = 0;
    uint32_t fB_8402a9 = 0;
    uint32_t fA_8402c2 = 0;
    uint32_t fB_8402c2 = 0;
    uint32_t fA_840301 = 0;
    uint32_t fB_840301 = 0;
    uint32_t fA_840323 = 0;
    uint32_t fB_840323 = 0;
    esp -= 4; wr32(m, esp, ebp);
    ebp = esp;
    esp = (esp & 0xfffffff0u);
    esp = esp - 0xf8u;
    esp -= 4; wr32(m, esp, ebx);
    ebx = rd32(m, ebp + 0x8u);
    esp -= 4; wr32(m, esp, edi);
    eax = (esp + 0x50u);
    edi = ecx;
    esp -= 4; wr32(m, esp, eax);
    ecx = esi;
    wr32(m, esp + 0x10u, ebx);
    LIFT_CALL(0x82b930u, 0x840063u);  // FUN_0082b930
    fA_840063 = rd32(m, esi + 0x24u);
    fB_840063 = 0x0u;
    x1 = movss_m(rd32(m, esp + 0x54u));
    wr32(m, esp + 0x8u, 0x0u);
    if (int32_t(sext(fA_840063, 4)) <= int32_t(sext(fB_840063, 4))) goto L_8402c0;
    eax = rd32(m, esp + 0x5cu);
    ecx = rd32(m, esp + 0x58u);
    ecx = ecx - eax;
    edx = 0x1u;
    edx = edx - ebx;
    x1 = shufps(x1, x1, 0x0);
    stx(m, esp + 0x60u, x1);
    wr32(m, esp + 0x10u, eax);
    wr32(m, esp + 0x30u, ecx);
    wr32(m, esp + 0x4cu, edx);
    goto L_8400a7;
L_8400a3:
    ecx = rd32(m, esp + 0x30u);
L_8400a7:
    x0 = ldx(m, ecx + eax);
    fpush(m, reflex_f80::from_int(1));
    x1 = ldx(m, eax);
    stx(m, esp + 0x70u, x0);
    ecx = sext(rd16(m, esp + 0x70u), 2);
    edx = sext(rd16(m, esp + 0x72u), 2);
    eax = sext(rd16(m, esp + 0x74u), 2);
    wr32(m, esp + 0x80u, ecx);
    ecx = sext(rd16(m, esp + 0x76u), 2);
    stx(m, esp + 0x70u, x0);
    wr32(m, esp + 0x84u, edx);
    edx = sext(rd16(m, esp + 0x78u), 2);
    wr32(m, esp + 0x88u, eax);
    eax = sext(rd16(m, esp + 0x7au), 2);
    wr32(m, esp + 0x8cu, ecx);
    ecx = sext(rd16(m, esp + 0x7cu), 2);
    x0 = ldx(m, esp + 0x80u);
    wr32(m, esp + 0xb0u, edx);
    edx = sext(rd16(m, esp + 0x7eu), 2);
    stx(m, esp + 0x70u, x1);
    wr32(m, esp + 0xb4u, eax);
    eax = sext(rd16(m, esp + 0x70u), 2);
    wr32(m, esp + 0xb8u, ecx);
    ecx = sext(rd16(m, esp + 0x72u), 2);
    wr32(m, esp + 0xbcu, edx);
    edx = sext(rd16(m, esp + 0x74u), 2);
    wr32(m, esp + 0xa0u, eax);
    eax = sext(rd16(m, esp + 0x76u), 2);
    stx(m, esp + 0x70u, x1);
    wr32(m, esp + 0xa4u, ecx);
    ecx = sext(rd16(m, esp + 0x78u), 2);
    wr32(m, esp + 0x90u, ecx);
    ecx = sext(rd16(m, esp + 0x7eu), 2);
    wr32(m, esp + 0xa8u, edx);
    edx = sext(rd16(m, esp + 0x7au), 2);
    wr32(m, esp + 0xacu, eax);
    eax = sext(rd16(m, esp + 0x7cu), 2);
    esp -= 4; wr32(m, esp, ecx);
    wr32(m, esp + 0x98u, edx);
    wr32(m, esp + 0x9cu, eax);
    wr32(m, esp, fst32(m));
    fpop(m);
    wr32(m, esp + 0xa0u, ecx);
    LIFT_CALL(0x41c580u, 0x840195u);  // FUN_0041c580
    stx(m, esp + 0xd4u, x0);
    fpush(m, reflex_f80::from_int(1));
    x0 = ldx(m, esp + 0xb4u);
    wr32(m, esp, fst32(m));
    fpop(m);
    LIFT_CALL(0x41c580u, 0x8401afu);  // FUN_0041c580
    fpush(m, reflex_f80::from_int(1));
    stx(m, esp + 0xc4u, x0);
    wr32(m, esp, fst32(m));
    fpop(m);
    x0 = ldx(m, esp + 0xa4u);
    LIFT_CALL(0x41c580u, 0x8401c9u);  // FUN_0041c580
    fpush(m, reflex_f80::from_int(1));
    stx(m, esp + 0xe4u, x0);
    wr32(m, esp, fst32(m));
    fpop(m);
    x0 = ldx(m, esp + 0x94u);
    LIFT_CALL(0x41c580u, 0x8401e3u);  // FUN_0041c580
    x3 = ldx(m, 0x92e000u);
    x2 = ldx(m, esp + 0x64u);
    x1 = x0;
    x0 = ldx(m, esp + 0xc4u);
    esp = esp + 0x4u;
    x1 = mulps(x1, x3);
    x0 = mulps(x0, x3);
    LIFT_CALL(0x42ca00u, 0x840208u);  // FUN_0042ca00
    edx = rd32(m, esp + 0x8u);
    eax = rd8(m, ebx + edx);
    x3 = ldx(m, 0x92e000u);
    x1 = ldx(m, esp + 0xe0u);
    x2 = ldx(m, esp + 0x60u);
    ebx = ebx + edx;
    eax = (eax + eax * 2);
    eax = (eax << 4);
    stx(m, esp + 0xf0u, x0);
    x0 = ldx(m, esp + 0xd0u);
    eax = eax + edi;
    wr32(m, esp + 0x2cu, ebx);
    ebx = eax;
    x1 = mulps(x1, x3);
    x0 = mulps(x0, x3);
    LIFT_CALL(0x42ca00u, 0x84024fu);  // FUN_0042ca00
    ecx = rd32(m, esp + 0x2cu);
    stx(m, ebx, x0);
    eax = rd8(m, ecx);
    ebx = rd32(m, esp + 0xcu);
    eax = (eax + eax * 2);
    eax = (eax << 4);
    edx = (edx & 0xffffff00u) | ((0x1u) & 0xffu);
    wr8(m, eax + edi + 0x20u, (edx & 0xffu));
    eax = rd32(m, esp + 0x4cu);
    eax = eax + ecx;
    fA_84026f = eax;
    fB_84026f = rd32(m, esi + 0x24u);
    ecx = rd32(m, esp + 0x8u);
    if (int32_t(sext(fA_84026f, 4)) >= int32_t(sext(fB_84026f, 4))) goto L_84029f;
    eax = rd8(m, ebx + ecx + 0x1u);
    x0 = ldx(m, esp + 0xf0u);
    eax = (eax + eax * 2);
    eax = (eax << 4);
    eax = eax + edi;
    stx(m, eax, x0);
    eax = rd8(m, ebx + ecx + 0x1u);
    eax = (eax + eax * 2);
    eax = (eax << 4);
    wr8(m, eax + edi + 0x20u, (edx & 0xffu));
L_84029f:
    eax = rd32(m, esp + 0x10u);
    ecx = ecx + 0x2u;
    eax = eax + 0x10u;
    fA_8402a9 = ecx;
    fB_8402a9 = rd32(m, esi + 0x24u);
    wr32(m, esp + 0x8u, ecx);
    wr32(m, esp + 0x10u, eax);
    if (int32_t(sext(fA_8402a9, 4)) < int32_t(sext(fB_8402a9, 4))) goto L_8400a3;
    x1 = movss_m(rd32(m, esp + 0x54u));
L_8402c0:
    ecx = 0u;
    fA_8402c2 = rd32(m, esi + 0x28u);
    fB_8402c2 = ecx;
    if (int32_t(sext(fA_8402c2, 4)) <= int32_t(sext(fB_8402c2, 4))) goto L_840306;
L_8402c7:
    eax = rd32(m, esi + 0x1cu);
    edx = rd8(m, ebx + ecx);
    eax = eax + ecx;
    eax = (eax << 4);
    eax = eax + rd32(m, esi + 0x3cu);
    edx = (edx + edx * 2);
    edx = (edx << 4);
    edx = (edx + edi + 0x10u);
    fpush(m, reflex_f80::from_f32(rd32(m, eax)));
    ecx = (ecx + 1u);
    wr32(m, edx, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x4u)));
    wr32(m, edx + 0x4u, fst32(m));
    fpop(m);
    fpush(m, reflex_f80::from_f32(rd32(m, eax + 0x8u)));
    wr32(m, edx + 0x8u, fst32(m));
    fpop(m);
    eax = rd8(m, ebx + ecx - 0x1u);
    eax = (eax + eax * 2);
    eax = (eax << 4);
    wr8(m, eax + edi + 0x21u, 0x1u);
    fA_840301 = ecx;
    fB_840301 = rd32(m, esi + 0x28u);
    if (int32_t(sext(fA_840301, 4)) < int32_t(sext(fB_840301, 4))) goto L_8402c7;
L_840306:
    x0 = cvtsi2ss(x0, rd32(m, esi + 0xcu));
    x0 = mulss(x0, rd32(m, esp + 0x50u));
    edx = rd32(m, esi + 0x3cu);
    ecx = cvttss2si(x0.l(0));
    x0 = movss_m(rd32(m, 0x931f24u) /* 1.0 */);
    eax = (ecx + 0x1u);
    fA_840323 = eax;
    fB_840323 = rd32(m, esi + 0x1cu);
    x0 = subss(x0, x1.l(0));
    if (int32_t(sext(fA_840323, 4)) > int32_t(sext(fB_840323, 4))) eax = ecx;
    ecx = (ecx << 4);
    x2 = movss_m(rd32(m, ecx + edx));
    x3 = movss_m(rd32(m, ecx + edx + 0x4u));
    x4 = movss_m(rd32(m, ecx + edx + 0x8u));
    ecx = ecx + edx;
    ecx = rd32(m, esi + 0x4u);
    eax = (eax << 4);
    x5 = movss_m(rd32(m, eax + edx));
    x6 = movss_m(rd32(m, eax + edx + 0x4u));
    x7 = movss_m(rd32(m, eax + edx + 0x8u));
    eax = eax + edx;
    eax = rd8(m, ecx + ebx);
    edx = (eax + eax * 2);
    edx = (edx << 4);
    edi = (edx + edi + 0x10u);
    x5 = mulss(x5, x1.l(0));
    x6 = mulss(x6, x1.l(0));
    x7 = mulss(x7, x1.l(0));
    x2 = mulss(x2, x0.l(0));
    x3 = mulss(x3, x0.l(0));
    x4 = mulss(x4, x0.l(0));
    x2 = addss(x2, x5.l(0));
    x3 = addss(x3, x6.l(0));
    x4 = addss(x4, x7.l(0));
    wr32(m, edi, x2.l(0));
    wr32(m, edi + 0x4u, x3.l(0));
    wr32(m, edi + 0x8u, x4.l(0));
    edi = rd32(m, esp); esp += 4;
    ebx = rd32(m, esp); esp += 4;
    esp = ebp;
    ebp = rd32(m, esp); esp += 4;
    LIFT_RET(4);
}
