// Game helpers of the 0x0042xxxx range (rider / bike skeleton bookkeeping, std::vector-style accessors, clocks), transliterated
// instruction by instruction (tools: the math agent's tr2, scratchpad). Each line is one original instruction (in its comment);
// registers are the Cpu's locals, memory (the stack included) is guest memory, calls run the original (or its port) with every
// register synced, jumps are gotos. Exact by construction; to be lifted into readable C++ function by function.
#include "math_util.h"

using namespace x86;
using namespace mathx;

// 00420160 FUN_00420160: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00420160, Fn_00420160, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 420160 push ebp
    ebp = esp;                                                     // 420161 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 420163 and esp, 0xfffffff0
    esp = esp - 0x5cu;                                             // 420166 sub esp, 0x5c
    k.push(ebx);                                                   // 420169 push ebx
    eax = ld32(m, ebp + 0x8);                                      // 42016a mov eax, [ebp + 8]
    x3 = ld(m, eax);                                               // 42016d movaps xmm3, [eax]
    x1 = ld(m, 0xd615c0);                                          // 420170 movaps xmm1, [0xd615c0]
    x0 = X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000);     // 420177 movaps xmm0, [0x92db80]
    x6 = X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000);     // 42017e movaps xmm6, [0x93a210]
    x0 = andps(x0, x1);                                            // 420185 andps xmm0, xmm1
    x7 = ld(m, 0xd61660);                                          // 420188 movaps xmm7, [0xd61660]
    x2 = x0;                                                       // 42018f movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0x00);                                     // 420192 shufps xmm2, xmm0, 0
    x4 = x0;                                                       // 420196 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x55);                                     // 420199 shufps xmm4, xmm0, 0x55
    x1 = x3;                                                       // 42019d movaps xmm1, xmm3
    x1 = shufps(x1, x3, 0x1b);                                     // 4201a0 shufps xmm1, xmm3, 0x1b
    x2 = mulps(x2, x1);                                            // 4201a4 mulps xmm2, xmm1
    x2 = mulps(x2, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 4201a7 mulps xmm2, [0x93a1f0]
    x1 = shufps(x1, x1, 0xb1);                                     // 4201ae shufps xmm1, xmm1, 0xb1
    x4 = mulps(x4, x1);                                            // 4201b2 mulps xmm4, xmm1
    x4 = mulps(x4, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 4201b5 mulps xmm4, [0x93a200]
    x1 = shufps(x1, x1, 0x1b);                                     // 4201bc shufps xmm1, xmm1, 0x1b
    x5 = x0;                                                       // 4201c0 movaps xmm5, xmm0
    x5 = shufps(x5, x0, 0xaa);                                     // 4201c3 shufps xmm5, xmm0, 0xaa
    x1 = mulps(x1, x5);                                            // 4201c7 mulps xmm1, xmm5
    x1 = mulps(x1, x6);                                            // 4201ca mulps xmm1, xmm6
    x1 = addps(x1, x4);                                            // 4201cd addps xmm1, xmm4
    x4 = x0;                                                       // 4201d0 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0xff);                                     // 4201d3 shufps xmm4, xmm0, 0xff
    x4 = mulps(x4, x3);                                            // 4201d7 mulps xmm4, xmm3
    x4 = addps(x4, x2);                                            // 4201da addps xmm4, xmm2
    x2 = X::v(0xbf800000, 0xbf800000, 0xbf800000, 0x3f800000);     // 4201dd movaps xmm2, [0x93a220]
    x1 = addps(x1, x4);                                            // 4201e4 addps xmm1, xmm4
    st(m, esp + 0x30, x7);                                         // 4201e7 movaps [esp + 0x30], xmm7
    eax = ld32(m, esp + 0x3c);                                     // 4201ec mov eax, [esp + 0x3c]
    x0 = x3;                                                       // 4201f0 movaps xmm0, xmm3
    x0 = mulps(x0, x2);                                            // 4201f3 mulps xmm0, xmm2
    x2 = x1;                                                       // 4201f6 movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0x1b);                                     // 4201f9 shufps xmm2, xmm1, 0x1b
    x4 = x0;                                                       // 4201fd movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x00);                                     // 420200 shufps xmm4, xmm0, 0
    x4 = mulps(x4, x2);                                            // 420204 mulps xmm4, xmm2
    x4 = mulps(x4, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 420207 mulps xmm4, [0x93a1f0]
    x2 = shufps(x2, x2, 0xb1);                                     // 42020e shufps xmm2, xmm2, 0xb1
    x7 = x2;                                                       // 420212 movaps xmm7, xmm2
    x7 = shufps(x7, x2, 0x1b);                                     // 420215 shufps xmm7, xmm2, 0x1b
    x5 = x0;                                                       // 420219 movaps xmm5, xmm0
    x5 = shufps(x5, x0, 0x55);                                     // 42021c shufps xmm5, xmm0, 0x55
    x5 = mulps(x5, x2);                                            // 420220 mulps xmm5, xmm2
    x5 = mulps(x5, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 420223 mulps xmm5, [0x93a200]
    x2 = x0;                                                       // 42022a movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0xaa);                                     // 42022d shufps xmm2, xmm0, 0xaa
    x7 = mulps(x7, x2);                                            // 420231 mulps xmm7, xmm2
    x7 = mulps(x7, x6);                                            // 420234 mulps xmm7, xmm6
    x2 = x0;                                                       // 420237 movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0xff);                                     // 42023a shufps xmm2, xmm0, 0xff
    x2 = mulps(x2, x1);                                            // 42023e mulps xmm2, xmm1
    x2 = addps(x2, x4);                                            // 420241 addps xmm2, xmm4
    x4 = ld(m, 0xd61690);                                          // 420244 movaps xmm4, [0xd61690]
    x7 = addps(x7, x5);                                            // 42024b addps xmm7, xmm5
    x7 = addps(x7, x2);                                            // 42024e addps xmm7, xmm2
    st(m, esp + 0x10, x7);                                         // 420251 movaps [esp + 0x10], xmm7
    st32(m, esp + 0x1c, eax);                                      // 420256 mov [esp + 0x1c], eax
    x1 = ld(m, esp + 0x10);                                        // 42025a movaps xmm1, [esp + 0x10]
    x2 = x1;                                                       // 42025f movaps xmm2, xmm1
    x2 = mulps(x2, x1);                                            // 420262 mulps xmm2, xmm1
    x0 = X{};                                                      // 420265 xorps xmm0, xmm0
    st(m, esp + 0x40, x1);                                         // 420268 movaps [esp + 0x40], xmm1
    x5 = x0;                                                       // 42026d movaps xmm5, xmm0
    x5 = subps(x5, x1);                                            // 420270 subps xmm5, xmm1
    x5 = maxps(x5, x1);                                            // 420273 maxps xmm5, xmm1
    x1 = x4;                                                       // 420276 movaps xmm1, xmm4
    x1 = subps(x1, x2);                                            // 420279 subps xmm1, xmm2
    x2 = X::ss(0x3f3504e6u);                                       // 42027c movss xmm2, [0x977780]
    x2 = shufps(x2, x2, 0x00);                                     // 420284 shufps xmm2, xmm2, 0
    x6 = x0;                                                       // 420288 movaps xmm6, xmm0
    x6 = subps(x6, x1);                                            // 42028b subps xmm6, xmm1
    x6 = maxps(x6, x1);                                            // 42028e maxps xmm6, xmm1
    st(m, esp + 0x20, x2);                                         // 420291 movaps [esp + 0x20], xmm2
    x2 = cmpleps(x2, x5);                                          // 420296 cmpleps xmm2, xmm5
    x1 = sqrtps(x6);                                               // 42029a sqrtps xmm1, xmm6
    x1 = andps(x1, x2);                                            // 42029d andps xmm1, xmm2
    x6 = x2;                                                       // 4202a0 movaps xmm6, xmm2
    x6 = andnps(x6, x5);                                           // 4202a3 andnps xmm6, xmm5
    x1 = orps(x1, x6);                                             // 4202a6 orps xmm1, xmm6
    st(m, esp + 0x50, x1);                                         // 4202a9 movaps [esp + 0x50], xmm1
    x1 = mulps(x1, x1);                                            // 4202ae mulps xmm1, xmm1
    x6 = x1;                                                       // 4202b1 movaps xmm6, xmm1
    x6 = mulps(x6, X::v(0xbd4c81d6, 0xbd4c81d6, 0xbd4c81d6, 0xbd4c81d6)); // 4202b4 mulps xmm6, [0x935bc0]
    x6 = addps(x6, X::v(0x3f109758, 0x3f109758, 0x3f109758, 0x3f109758)); // 4202bb addps xmm6, [0x935bb0]
    x5 = x1;                                                       // 4202c2 movaps xmm5, xmm1
    x5 = mulps(x5, x1);                                            // 4202c5 mulps xmm5, xmm1
    x6 = mulps(x6, x5);                                            // 4202c8 mulps xmm6, xmm5
    st(m, esp + 0x30, x5);                                         // 4202cb movaps [esp + 0x30], xmm5
    x5 = x1;                                                       // 4202d0 movaps xmm5, xmm1
    x5 = mulps(x5, X::v(0xbfb8b563, 0xbfb8b563, 0xbfb8b563, 0xbfb8b563)); // 4202d3 mulps xmm5, [0x935ba0]
    x5 = addps(x5, x4);                                            // 4202da addps xmm5, xmm4
    x6 = addps(x6, x5);                                            // 4202dd addps xmm6, xmm5
    x5 = X::v(0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000);     // 4202e0 movaps xmm5, [0x92dba0]
    x5 = divps(x5, x6);                                            // 4202e7 divps xmm5, xmm6
    x6 = x1;                                                       // 4202ea movaps xmm6, xmm1
    x1 = mulps(x1, X::v(0xbfa3600d, 0xbfa3600d, 0xbfa3600d, 0xbfa3600d)); // 4202ed mulps xmm1, [0x935b90]
    x6 = mulps(x6, X::v(0xbc9ebf80, 0xbc9ebf80, 0xbc9ebf80, 0xbc9ebf80)); // 4202f4 mulps xmm6, [0x935b70]
    x6 = addps(x6, X::v(0x3ecc717f, 0x3ecc717f, 0x3ecc717f, 0x3ecc717f)); // 4202fb addps xmm6, [0x935b80]
    x6 = mulps(x6, ld(m, esp + 0x30));                             // 420302 mulps xmm6, [esp + 0x30]
    x1 = addps(x1, x4);                                            // 420307 addps xmm1, xmm4
    x6 = addps(x6, x1);                                            // 42030a addps xmm6, xmm1
    x6 = mulps(x6, ld(m, esp + 0x50));                             // 42030d mulps xmm6, [esp + 0x50]
    x5 = mulps(x5, x6);                                            // 420312 mulps xmm5, xmm6
    x6 = X::v(0x3fc90fdb, 0x3fc90fdb, 0x3fc90fdb, 0x3fc90fdb);     // 420315 movaps xmm6, [0x935b00]
    x1 = x6;                                                       // 42031c movaps xmm1, xmm6
    x1 = subps(x1, x5);                                            // 42031f subps xmm1, xmm5
    x1 = andps(x1, x2);                                            // 420322 andps xmm1, xmm2
    x2 = andnps(x2, x5);                                           // 420325 andnps xmm2, xmm5
    x1 = orps(x1, x2);                                             // 420328 orps xmm1, xmm2
    x2 = ld(m, 0xd61660);                                          // 42032b movaps xmm2, [0xd61660]
    x2 = cmpleps(x2, ld(m, esp + 0x10));                           // 420332 cmpleps xmm2, [esp + 0x10]
    x5 = x2;                                                       // 420338 movaps xmm5, xmm2
    x7 = x0;                                                       // 42033b movaps xmm7, xmm0
    x7 = subps(x7, x1);                                            // 42033e subps xmm7, xmm1
    x1 = andps(x1, x2);                                            // 420341 andps xmm1, xmm2
    x5 = andnps(x5, x7);                                           // 420344 andnps xmm5, xmm7
    x5 = orps(x5, x1);                                             // 420347 orps xmm5, xmm1
    x1 = X::ss(0x3f000000u);                                       // 42034a movss xmm1, [0x9322cc]
    x1 = shufps(x1, x1, 0x00);                                     // 420352 shufps xmm1, xmm1, 0
    x2 = x6;                                                       // 420356 movaps xmm2, xmm6
    x2 = subps(x2, x5);                                            // 420359 subps xmm2, xmm5
    x1 = mulps(x1, x2);                                            // 42035c mulps xmm1, xmm2
    st(m, esp + 0x30, x2);                                         // 42035f movaps [esp + 0x30], xmm2
    x2 = x0;                                                       // 420364 movaps xmm2, xmm0
    x2 = subps(x2, x1);                                            // 420367 subps xmm2, xmm1
    x2 = maxps(x2, x1);                                            // 42036a maxps xmm2, xmm1
    x1 = x2;                                                       // 42036d movaps xmm1, xmm2
    x1 = mulps(x1, X::v(0x3e22f983, 0x3e22f983, 0x3e22f983, 0x3e22f983)); // 420370 mulps xmm1, [0x935ad0]
    st(m, esp + 0x10, x1);                                         // 420377 movaps [esp + 0x10], xmm1
    eax = uint32_t(cvtt(flt(ld32(m, esp + 0x10))));                // 42037c cvttss2si eax, [esp + 0x10]
    x1 = cvtsi2ss(x1, int32_t(eax));                               // 420382 cvtsi2ss xmm1, eax
    eax = uint32_t(cvtt(flt(ld32(m, esp + 0x14))));                // 420386 cvttss2si eax, [esp + 0x14]
    st32(m, esp + 0x10, x1.u[0]);                                  // 42038c movss [esp + 0x10], xmm1
    x1 = cvtsi2ss(x1, int32_t(eax));                               // 420392 cvtsi2ss xmm1, eax
    eax = uint32_t(cvtt(flt(ld32(m, esp + 0x18))));                // 420396 cvttss2si eax, [esp + 0x18]
    st32(m, esp + 0x14, x1.u[0]);                                  // 42039c movss [esp + 0x14], xmm1
    x1 = cvtsi2ss(x1, int32_t(eax));                               // 4203a2 cvtsi2ss xmm1, eax
    eax = uint32_t(cvtt(flt(ld32(m, esp + 0x1c))));                // 4203a6 cvttss2si eax, [esp + 0x1c]
    st32(m, esp + 0x18, x1.u[0]);                                  // 4203ac movss [esp + 0x18], xmm1
    x1 = cvtsi2ss(x1, int32_t(eax));                               // 4203b2 cvtsi2ss xmm1, eax
    st32(m, esp + 0x1c, x1.u[0]);                                  // 4203b6 movss [esp + 0x1c], xmm1
    x1 = ld(m, esp + 0x10);                                        // 4203bc movaps xmm1, [esp + 0x10]
    x1 = mulps(x1, X::v(0x40c90fdb, 0x40c90fdb, 0x40c90fdb, 0x40c90fdb)); // 4203c1 mulps xmm1, [0x935ae0]
    x2 = subps(x2, x1);                                            // 4203c8 subps xmm2, xmm1
    x1 = X::v(0x40490fdb, 0x40490fdb, 0x40490fdb, 0x40490fdb);     // 4203cb movaps xmm1, [0x935af0]
    x1 = subps(x1, x2);                                            // 4203d2 subps xmm1, xmm2
    x1 = mulps(x1, x1);                                            // 4203d5 mulps xmm1, xmm1
    x5 = x1;                                                       // 4203d8 movaps xmm5, xmm1
    x5 = mulps(x5, X::v(0xb46beed0, 0xb46beed0, 0xb46beed0, 0xb46beed0)); // 4203db mulps xmm5, [0x935b60]
    x5 = addps(x5, X::v(0x37cb076b, 0x37cb076b, 0x37cb076b, 0x37cb076b)); // 4203e2 addps xmm5, [0x935b50]
    x2 = x1;                                                       // 4203e9 movaps xmm2, xmm1
    x2 = mulps(x2, x1);                                            // 4203ec mulps xmm2, xmm1
    x6 = x1;                                                       // 4203ef movaps xmm6, xmm1
    x6 = mulps(x6, X::v(0xbab5a662, 0xbab5a662, 0xbab5a662, 0xbab5a662)); // 4203f2 mulps xmm6, [0x935b40]
    x6 = addps(x6, X::v(0x3d2aa372, 0x3d2aa372, 0x3d2aa372, 0x3d2aa372)); // 4203f9 addps xmm6, [0x935b30]
    x5 = mulps(x5, x2);                                            // 420400 mulps xmm5, xmm2
    x5 = addps(x5, x6);                                            // 420403 addps xmm5, xmm6
    x5 = mulps(x5, x2);                                            // 420406 mulps xmm5, xmm2
    x1 = mulps(x1, X::v(0xbeffff3d, 0xbeffff3d, 0xbeffff3d, 0xbeffff3d)); // 420409 mulps xmm1, [0x935b20]
    x1 = addps(x1, X::v(0x3f7ffff3, 0x3f7ffff3, 0x3f7ffff3, 0x3f7ffff3)); // 420410 addps xmm1, [0x935b10]
    x5 = addps(x5, x1);                                            // 420417 addps xmm5, xmm1
    x1 = x0;                                                       // 42041a movaps xmm1, xmm0
    x1 = subps(x1, x5);                                            // 42041d subps xmm1, xmm5
    x3 = divps(x3, x1);                                            // 420420 divps xmm3, xmm1
    st(m, esp + 0x10, x3);                                         // 420423 movaps [esp + 0x10], xmm3
    x1 = X::ss(ld32(m, esp + 0x10));                               // 420428 movss xmm1, [esp + 0x10]
    x1 = shufps(x1, x1, 0x00);                                     // 42042e shufps xmm1, xmm1, 0
    x1 = minps(x1, x4);                                            // 420432 minps xmm1, xmm4
    x2 = x0;                                                       // 420435 movaps xmm2, xmm0
    x2 = subps(x2, x4);                                            // 420438 subps xmm2, xmm4
    x1 = maxps(x1, x2);                                            // 42043b maxps xmm1, xmm2
    st(m, esp + 0x10, x1);                                         // 42043e movaps [esp + 0x10], xmm1
    x2 = x1;                                                       // 420443 movaps xmm2, xmm1
    x2 = mulps(x2, x1);                                            // 420446 mulps xmm2, xmm1
    x5 = x0;                                                       // 420449 movaps xmm5, xmm0
    x5 = subps(x5, x1);                                            // 42044c subps xmm5, xmm1
    x5 = maxps(x5, x1);                                            // 42044f maxps xmm5, xmm1
    x1 = x4;                                                       // 420452 movaps xmm1, xmm4
    x1 = subps(x1, x2);                                            // 420455 subps xmm1, xmm2
    x2 = ld(m, esp + 0x20);                                        // 420458 movaps xmm2, [esp + 0x20]
    x2 = cmpleps(x2, x5);                                          // 42045d cmpleps xmm2, xmm5
    x3 = x0;                                                       // 420461 movaps xmm3, xmm0
    x3 = subps(x3, x1);                                            // 420464 subps xmm3, xmm1
    x3 = maxps(x3, x1);                                            // 420467 maxps xmm3, xmm1
    x1 = x2;                                                       // 42046a movaps xmm1, xmm2
    x1 = andnps(x1, x5);                                           // 42046d andnps xmm1, xmm5
    x3 = sqrtps(x3);                                               // 420470 sqrtps xmm3, xmm3
    x3 = andps(x3, x2);                                            // 420473 andps xmm3, xmm2
    x3 = orps(x3, x1);                                             // 420476 orps xmm3, xmm1
    x1 = x3;                                                       // 420479 movaps xmm1, xmm3
    x1 = mulps(x1, x3);                                            // 42047c mulps xmm1, xmm3
    x6 = x1;                                                       // 42047f movaps xmm6, xmm1
    x6 = mulps(x6, X::v(0xbd4c81d6, 0xbd4c81d6, 0xbd4c81d6, 0xbd4c81d6)); // 420482 mulps xmm6, [0x935bc0]
    x6 = addps(x6, X::v(0x3f109758, 0x3f109758, 0x3f109758, 0x3f109758)); // 420489 addps xmm6, [0x935bb0]
    x5 = x1;                                                       // 420490 movaps xmm5, xmm1
    x5 = mulps(x5, x1);                                            // 420493 mulps xmm5, xmm1
    x6 = mulps(x6, x5);                                            // 420496 mulps xmm6, xmm5
    st(m, esp + 0x20, x5);                                         // 420499 movaps [esp + 0x20], xmm5
    x5 = x1;                                                       // 42049e movaps xmm5, xmm1
    x5 = mulps(x5, X::v(0xbfb8b563, 0xbfb8b563, 0xbfb8b563, 0xbfb8b563)); // 4204a1 mulps xmm5, [0x935ba0]
    x5 = addps(x5, x4);                                            // 4204a8 addps xmm5, xmm4
    x6 = addps(x6, x5);                                            // 4204ab addps xmm6, xmm5
    x5 = X::v(0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000);     // 4204ae movaps xmm5, [0x92dba0]
    x5 = divps(x5, x6);                                            // 4204b5 divps xmm5, xmm6
    x6 = x1;                                                       // 4204b8 movaps xmm6, xmm1
    x1 = mulps(x1, X::v(0xbfa3600d, 0xbfa3600d, 0xbfa3600d, 0xbfa3600d)); // 4204bb mulps xmm1, [0x935b90]
    x6 = mulps(x6, X::v(0xbc9ebf80, 0xbc9ebf80, 0xbc9ebf80, 0xbc9ebf80)); // 4204c2 mulps xmm6, [0x935b70]
    x6 = addps(x6, X::v(0x3ecc717f, 0x3ecc717f, 0x3ecc717f, 0x3ecc717f)); // 4204c9 addps xmm6, [0x935b80]
    x6 = mulps(x6, ld(m, esp + 0x20));                             // 4204d0 mulps xmm6, [esp + 0x20]
    x1 = addps(x1, x4);                                            // 4204d5 addps xmm1, xmm4
    x6 = addps(x6, x1);                                            // 4204d8 addps xmm6, xmm1
    x1 = X::v(0x3fc90fdb, 0x3fc90fdb, 0x3fc90fdb, 0x3fc90fdb);     // 4204db movaps xmm1, [0x935b00]
    x6 = mulps(x6, x3);                                            // 4204e2 mulps xmm6, xmm3
    x5 = mulps(x5, x6);                                            // 4204e5 mulps xmm5, xmm6
    x1 = subps(x1, x5);                                            // 4204e8 subps xmm1, xmm5
    x1 = andps(x1, x2);                                            // 4204eb andps xmm1, xmm2
    x2 = andnps(x2, x5);                                           // 4204ee andnps xmm2, xmm5
    x1 = orps(x1, x2);                                             // 4204f1 orps xmm1, xmm2
    x2 = ld(m, 0xd61660);                                          // 4204f4 movaps xmm2, [0xd61660]
    x2 = cmpleps(x2, ld(m, esp + 0x10));                           // 4204fb cmpleps xmm2, [esp + 0x10]
    x3 = x2;                                                       // 420501 movaps xmm3, xmm2
    x4 = x0;                                                       // 420504 movaps xmm4, xmm0
    x4 = subps(x4, x1);                                            // 420507 subps xmm4, xmm1
    x1 = andps(x1, x2);                                            // 42050a andps xmm1, xmm2
    x3 = andnps(x3, x4);                                           // 42050d andnps xmm3, xmm4
    x3 = orps(x3, x1);                                             // 420510 orps xmm3, xmm1
    x1 = X::ss(0x40000000u);                                       // 420513 movss xmm1, [0x92e330]
    x1 = shufps(x1, x1, 0x00);                                     // 42051b shufps xmm1, xmm1, 0
    x3 = mulps(x3, x1);                                            // 42051f mulps xmm3, xmm1
    x1 = ld(m, esp + 0x30);                                        // 420522 movaps xmm1, [esp + 0x30]
    st(m, esp + 0x20, x1);                                         // 420527 movaps [esp + 0x20], xmm1
    eax = ld32(m, esp + 0x20);                                     // 42052c mov eax, [esp + 0x20]
    st(m, esp + 0x10, x3);                                         // 420530 movaps [esp + 0x10], xmm3
    st32(m, esp + 0x1c, eax);                                      // 420535 mov [esp + 0x1c], eax
    x4 = ld(m, esp + 0x10);                                        // 420539 movaps xmm4, [esp + 0x10]
    x2 = x0;                                                       // 42053e movaps xmm2, xmm0
    x2 = subps(x2, x4);                                            // 420541 subps xmm2, xmm4
    x2 = maxps(x2, x4);                                            // 420544 maxps xmm2, xmm4
    x1 = x2;                                                       // 420547 movaps xmm1, xmm2
    x1 = mulps(x1, X::v(0x3e22f983, 0x3e22f983, 0x3e22f983, 0x3e22f983)); // 42054a mulps xmm1, [0x935ad0]
    st(m, esp + 0x10, x1);                                         // 420551 movaps [esp + 0x10], xmm1
    eax = uint32_t(cvtt(flt(ld32(m, esp + 0x10))));                // 420556 cvttss2si eax, [esp + 0x10]
    x6 = X::v(0x3d2aa372, 0x3d2aa372, 0x3d2aa372, 0x3d2aa372);     // 42055c movaps xmm6, [0x935b30]
    x4 = addps(x4, X::v(0x3fc90fdb, 0x3fc90fdb, 0x3fc90fdb, 0x3fc90fdb)); // 420563 addps xmm4, [0x935b00]
    x1 = cvtsi2ss(x1, int32_t(eax));                               // 42056a cvtsi2ss xmm1, eax
    eax = uint32_t(cvtt(flt(ld32(m, esp + 0x14))));                // 42056e cvttss2si eax, [esp + 0x14]
    st32(m, esp + 0x10, x1.u[0]);                                  // 420574 movss [esp + 0x10], xmm1
    x1 = cvtsi2ss(x1, int32_t(eax));                               // 42057a cvtsi2ss xmm1, eax
    eax = uint32_t(cvtt(flt(ld32(m, esp + 0x18))));                // 42057e cvttss2si eax, [esp + 0x18]
    st32(m, esp + 0x14, x1.u[0]);                                  // 420584 movss [esp + 0x14], xmm1
    x1 = cvtsi2ss(x1, int32_t(eax));                               // 42058a cvtsi2ss xmm1, eax
    eax = uint32_t(cvtt(flt(ld32(m, esp + 0x1c))));                // 42058e cvttss2si eax, [esp + 0x1c]
    st32(m, esp + 0x18, x1.u[0]);                                  // 420594 movss [esp + 0x18], xmm1
    x1 = cvtsi2ss(x1, int32_t(eax));                               // 42059a cvtsi2ss xmm1, eax
    st32(m, esp + 0x1c, x1.u[0]);                                  // 42059e movss [esp + 0x1c], xmm1
    x1 = ld(m, esp + 0x10);                                        // 4205a4 movaps xmm1, [esp + 0x10]
    x1 = mulps(x1, X::v(0x40c90fdb, 0x40c90fdb, 0x40c90fdb, 0x40c90fdb)); // 4205a9 mulps xmm1, [0x935ae0]
    x2 = subps(x2, x1);                                            // 4205b0 subps xmm2, xmm1
    x1 = X::v(0x40490fdb, 0x40490fdb, 0x40490fdb, 0x40490fdb);     // 4205b3 movaps xmm1, [0x935af0]
    x1 = subps(x1, x2);                                            // 4205ba subps xmm1, xmm2
    x1 = mulps(x1, x1);                                            // 4205bd mulps xmm1, xmm1
    x7 = x1;                                                       // 4205c0 movaps xmm7, xmm1
    x7 = mulps(x7, X::v(0xb46beed0, 0xb46beed0, 0xb46beed0, 0xb46beed0)); // 4205c3 mulps xmm7, [0x935b60]
    x7 = addps(x7, X::v(0x37cb076b, 0x37cb076b, 0x37cb076b, 0x37cb076b)); // 4205ca addps xmm7, [0x935b50]
    x2 = x1;                                                       // 4205d1 movaps xmm2, xmm1
    x2 = mulps(x2, x1);                                            // 4205d4 mulps xmm2, xmm1
    x7 = mulps(x7, x2);                                            // 4205d7 mulps xmm7, xmm2
    x5 = x1;                                                       // 4205da movaps xmm5, xmm1
    x1 = mulps(x1, X::v(0xbeffff3d, 0xbeffff3d, 0xbeffff3d, 0xbeffff3d)); // 4205dd mulps xmm1, [0x935b20]
    x1 = addps(x1, X::v(0x3f7ffff3, 0x3f7ffff3, 0x3f7ffff3, 0x3f7ffff3)); // 4205e4 addps xmm1, [0x935b10]
    x5 = mulps(x5, X::v(0xbab5a662, 0xbab5a662, 0xbab5a662, 0xbab5a662)); // 4205eb mulps xmm5, [0x935b40]
    x5 = addps(x5, x6);                                            // 4205f2 addps xmm5, xmm6
    x7 = addps(x7, x5);                                            // 4205f5 addps xmm7, xmm5
    x7 = mulps(x7, x2);                                            // 4205f8 mulps xmm7, xmm2
    x7 = addps(x7, x1);                                            // 4205fb addps xmm7, xmm1
    x1 = x0;                                                       // 4205fe movaps xmm1, xmm0
    x1 = subps(x1, x4);                                            // 420601 subps xmm1, xmm4
    x1 = maxps(x1, x4);                                            // 420604 maxps xmm1, xmm4
    x2 = x1;                                                       // 420607 movaps xmm2, xmm1
    x2 = mulps(x2, X::v(0x3e22f983, 0x3e22f983, 0x3e22f983, 0x3e22f983)); // 42060a mulps xmm2, [0x935ad0]
    st(m, esp + 0x10, x2);                                         // 420611 movaps [esp + 0x10], xmm2
    eax = uint32_t(cvtt(flt(ld32(m, esp + 0x10))));                // 420616 cvttss2si eax, [esp + 0x10]
    x2 = cvtsi2ss(x2, int32_t(eax));                               // 42061c cvtsi2ss xmm2, eax
    eax = uint32_t(cvtt(flt(ld32(m, esp + 0x14))));                // 420620 cvttss2si eax, [esp + 0x14]
    st32(m, esp + 0x10, x2.u[0]);                                  // 420626 movss [esp + 0x10], xmm2
    x2 = cvtsi2ss(x2, int32_t(eax));                               // 42062c cvtsi2ss xmm2, eax
    eax = uint32_t(cvtt(flt(ld32(m, esp + 0x18))));                // 420630 cvttss2si eax, [esp + 0x18]
    st32(m, esp + 0x14, x2.u[0]);                                  // 420636 movss [esp + 0x14], xmm2
    x2 = cvtsi2ss(x2, int32_t(eax));                               // 42063c cvtsi2ss xmm2, eax
    eax = uint32_t(cvtt(flt(ld32(m, esp + 0x1c))));                // 420640 cvttss2si eax, [esp + 0x1c]
    st32(m, esp + 0x18, x2.u[0]);                                  // 420646 movss [esp + 0x18], xmm2
    x2 = cvtsi2ss(x2, int32_t(eax));                               // 42064c cvtsi2ss xmm2, eax
    st32(m, esp + 0x1c, x2.u[0]);                                  // 420650 movss [esp + 0x1c], xmm2
    x2 = ld(m, esp + 0x10);                                        // 420656 movaps xmm2, [esp + 0x10]
    x2 = mulps(x2, X::v(0x40c90fdb, 0x40c90fdb, 0x40c90fdb, 0x40c90fdb)); // 42065b mulps xmm2, [0x935ae0]
    x1 = subps(x1, x2);                                            // 420662 subps xmm1, xmm2
    x2 = X::v(0x40490fdb, 0x40490fdb, 0x40490fdb, 0x40490fdb);     // 420665 movaps xmm2, [0x935af0]
    x2 = subps(x2, x1);                                            // 42066c subps xmm2, xmm1
    x1 = x2;                                                       // 42066f movaps xmm1, xmm2
    x1 = mulps(x1, x1);                                            // 420672 mulps xmm1, xmm1
    x2 = x1;                                                       // 420675 movaps xmm2, xmm1
    x2 = mulps(x2, X::v(0xb46beed0, 0xb46beed0, 0xb46beed0, 0xb46beed0)); // 420678 mulps xmm2, [0x935b60]
    x2 = addps(x2, X::v(0x37cb076b, 0x37cb076b, 0x37cb076b, 0x37cb076b)); // 42067f addps xmm2, [0x935b50]
    x4 = x1;                                                       // 420686 movaps xmm4, xmm1
    x4 = mulps(x4, x1);                                            // 420689 mulps xmm4, xmm1
    x5 = x0;                                                       // 42068c movaps xmm5, xmm0
    x5 = subps(x5, x7);                                            // 42068f subps xmm5, xmm7
    x7 = x1;                                                       // 420692 movaps xmm7, xmm1
    x7 = mulps(x7, X::v(0xbab5a662, 0xbab5a662, 0xbab5a662, 0xbab5a662)); // 420695 mulps xmm7, [0x935b40]
    x1 = mulps(x1, X::v(0xbeffff3d, 0xbeffff3d, 0xbeffff3d, 0xbeffff3d)); // 42069c mulps xmm1, [0x935b20]
    x1 = addps(x1, X::v(0x3f7ffff3, 0x3f7ffff3, 0x3f7ffff3, 0x3f7ffff3)); // 4206a3 addps xmm1, [0x935b10]
    x2 = mulps(x2, x4);                                            // 4206aa mulps xmm2, xmm4
    x7 = addps(x7, x6);                                            // 4206ad addps xmm7, xmm6
    x2 = addps(x2, x7);                                            // 4206b0 addps xmm2, xmm7
    x2 = mulps(x2, x4);                                            // 4206b3 mulps xmm2, xmm4
    x4 = X::ss(0x38d1b717u);                                       // 4206b6 movss xmm4, [0x9775a4]
    x2 = addps(x2, x1);                                            // 4206be addps xmm2, xmm1
    st(m, esp + 0x20, x2);                                         // 4206c1 movaps [esp + 0x20], xmm2
    x1 = X::ss(ld32(m, esp + 0x2c));                               // 4206c6 movss xmm1, [esp + 0x2c]
    x1 = shufps(x1, x1, 0x00);                                     // 4206cc shufps xmm1, xmm1, 0
    x4 = shufps(x4, x4, 0x00);                                     // 4206d0 shufps xmm4, xmm4, 0
    x6 = x1;                                                       // 4206d4 movaps xmm6, xmm1
    x6 = cmpltps(x6, x4);                                          // 4206d7 cmpltps xmm6, xmm4
    st(m, esp + 0x10, x6);                                         // 4206db movaps [esp + 0x10], xmm6
    eax = ld32(m, esp + 0x18);                                     // 4206e0 mov eax, [esp + 0x18]
    ebx = ld32(m, esp + 0x1c);                                     // 4206e4 mov ebx, [esp + 0x1c]
    ebx = ebx + eax;                                               // 4206e8 add ebx, eax
    ebx = ebx + ld32(m, esp + 0x14);                               // 4206ea add ebx, [esp + 0x14]
    ebx = ebx + ld32(m, esp + 0x10);                               // 4206ee add ebx, [esp + 0x10]
    fl = f_cmp(ebx, 0xfffffffcu);                                  // 4206f2 cmp ebx, -4
    if (fl.ne()) goto L_00420708;                                  // 4206f5 jne 0x420708
    x0 = X::ss(0x3f800000u);                                       // 4206f7 movss xmm0, [0x931f24]
    st32(m, ecx, x0.u[0]);                                         // 4206ff movss [ecx], xmm0
    x0 = X{};                                                      // 420703 xorps xmm0, xmm0
    goto L_0042074e;                                               // 420706 jmp 0x42074e
L_00420708:
    x6 = X::ss(ld32(m, esp + 0x48));                               // 420708 movss xmm6, [esp + 0x48]
    x4 = X::ss(ld32(m, esp + 0x44));                               // 42070e movss xmm4, [esp + 0x44]
    x6 = shufps(x6, x6, 0x00);                                     // 420714 shufps xmm6, xmm6, 0
    st(m, esp + 0x20, x6);                                         // 420718 movaps [esp + 0x20], xmm6
    x6 = mulps(x6, x2);                                            // 42071d mulps xmm6, xmm2
    x4 = shufps(x4, x4, 0x00);                                     // 420720 shufps xmm4, xmm4, 0
    x7 = x4;                                                       // 420724 movaps xmm7, xmm4
    x4 = mulps(x4, x2);                                            // 420727 mulps xmm4, xmm2
    x2 = ld(m, esp + 0x20);                                        // 42072a movaps xmm2, [esp + 0x20]
    x7 = mulps(x7, x5);                                            // 42072f mulps xmm7, xmm5
    x7 = subps(x7, x6);                                            // 420732 subps xmm7, xmm6
    x6 = x0;                                                       // 420735 movaps xmm6, xmm0
    x2 = mulps(x2, x5);                                            // 420738 mulps xmm2, xmm5
    x6 = subps(x6, x7);                                            // 42073b subps xmm6, xmm7
    x4 = addps(x4, x2);                                            // 42073e addps xmm4, xmm2
    x6 = divps(x6, x1);                                            // 420741 divps xmm6, xmm1
    x0 = subps(x0, x4);                                            // 420744 subps xmm0, xmm4
    st32(m, ecx, x6.u[0]);                                         // 420747 movss [ecx], xmm6
    x0 = divps(x0, x1);                                            // 42074b divps xmm0, xmm1
L_0042074e:
    st32(m, edx, x0.u[0]);                                         // 42074e movss [edx], xmm0
    x0 = X::ss(ld32(m, esp + 0x30));                               // 420752 movss xmm0, [esp + 0x30]
    st32(m, esi, x0.u[0]);                                         // 420758 movss [esi], xmm0
    st32(m, edi, x3.u[0]);                                         // 42075c movss [edi], xmm3
    ebx = k.pop();                                                 // 420760 pop ebx
    esp = ebp;                                                     // 420761 mov esp, ebp
    ebp = k.pop();                                                 // 420763 pop ebp
    return k.ret(4);                                               // 420764 ret 4
}

// 00420770 FUN_00420770: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00420770, Fn_00420770, "math", 0) {  // outputs: what callers read (it calls other groups' code)
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 420770 push ebp
    ebp = esp;                                                     // 420771 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 420773 and esp, 0xfffffff0
    esp = esp - 0x194u;                                            // 420776 sub esp, 0x194
    eax = ld32(m, 0x9b45e4);                                       // 42077c mov eax, [0x9b45e4]
    eax = eax ^ esp;                                               // 420781 xor eax, esp
    st32(m, esp + 0x190, eax);                                     // 420783 mov [esp + 0x190], eax
    x0 = X::ss(0x3f000000u);                                       // 42078a movss xmm0, [0x9322cc]
    k.push(ebx);                                                   // 420792 push ebx
    ebx = ld32(m, ebp + 0x8);                                      // 420793 mov ebx, [ebp + 8]
    fl = f_cmp(ld32(m, ebx + 0x1b20), 0x0u);                       // 420796 cmp [ebx + 0x1b20], 0
    k.push(esi);                                                   // 42079d push esi
    x0 = shufps(x0, x0, 0x00);                                     // 42079e shufps xmm0, xmm0, 0
    k.push(edi);                                                   // 4207a2 push edi
    st(m, esp + 0x90, x0);                                         // 4207a3 movaps [esp + 0x90], xmm0
    st32(m, esp + 0x1c, 0x0u);                                     // 4207ab mov [esp + 0x1c], 0
    if (fl.le()) goto L_00420f9f;                                  // 4207b3 jle 0x420f9f
    eax = ebx + 0x37f0;                                            // 4207b9 lea eax, [ebx + 0x37f0]
    x0 = X{};                                                      // 4207bf xorps xmm0, xmm0
    st(m, esp + 0x110, x0);                                        // 4207c2 movaps [esp + 0x110], xmm0
    st32(m, esp + 0x18, eax);                                      // 4207ca mov [esp + 0x18], eax
L_004207ce:
    ecx = ld32(m, esp + 0x18);                                     // 4207ce mov ecx, [esp + 0x18]
    eax = ld32(m, ecx);                                            // 4207d2 mov eax, [ecx]
    edx = eax;                                                     // 4207d4 mov edx, eax
    edx = edx * 0x170u;                                            // 4207d6 imul edx, edx, 0x170
    ecx = esp + eax * 4 + 0xa0;                                    // 4207dc lea ecx, [esp + eax*4 + 0xa0]
    st32(m, esp + 0x64, ecx);                                      // 4207e3 mov [esp + 0x64], ecx
    st32(m, ecx, 0x0u);                                            // 4207e7 mov [ecx], 0
    ecx = edx + ebx;                                               // 4207ed lea ecx, [edx + ebx]
    edx = ld32(m, ecx + 0x1b30);                                   // 4207f0 mov edx, [ecx + 0x1b30]
    edx = edx * 0x70u;                                             // 4207f6 imul edx, edx, 0x70
    x0 = ld(m, edx + ebx + 0xb00);                                 // 4207f9 movaps xmm0, [edx + ebx + 0xb00]
    x6 = X::v(0xbf800000, 0xbf800000, 0xbf800000, 0x3f800000);     // 420801 movaps xmm6, [0x93a220]
    edx = eax + 0x13;                                              // 420808 lea edx, [eax + 0x13]
    edx = edx * 0x170u;                                            // 42080b imul edx, edx, 0x170
    x7 = X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000);     // 420811 movaps xmm7, [0x93a200]
    edx = edx + ebx;                                               // 420818 add edx, ebx
    x0 = mulps(x0, x6);                                            // 42081a mulps xmm0, xmm6
    st32(m, esp + 0x6c, edx);                                      // 42081d mov [esp + 0x6c], edx
    edx = ld32(m, edx);                                            // 420821 mov edx, [edx]
    edx = edx * 0x70u;                                             // 420823 imul edx, edx, 0x70
    x5 = ld(m, edx + ebx + 0xb00);                                 // 420826 movaps xmm5, [edx + ebx + 0xb00]
    x3 = x0;                                                       // 42082e movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0x00);                                     // 420831 shufps xmm3, xmm0, 0
    x4 = x0;                                                       // 420835 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x55);                                     // 420838 shufps xmm4, xmm0, 0x55
    x1 = x5;                                                       // 42083c movaps xmm1, xmm5
    x1 = shufps(x1, x5, 0x1b);                                     // 42083f shufps xmm1, xmm5, 0x1b
    x3 = mulps(x3, x1);                                            // 420843 mulps xmm3, xmm1
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 420846 mulps xmm3, [0x93a1f0]
    x1 = shufps(x1, x1, 0xb1);                                     // 42084d shufps xmm1, xmm1, 0xb1
    x2 = x1;                                                       // 420851 movaps xmm2, xmm1
    x1 = ld(m, ecx + 0x1be0);                                      // 420854 movaps xmm1, [ecx + 0x1be0]
    x4 = mulps(x4, x2);                                            // 42085b mulps xmm4, xmm2
    x1 = mulps(x1, x6);                                            // 42085e mulps xmm1, xmm6
    x6 = x0;                                                       // 420861 movaps xmm6, xmm0
    x6 = shufps(x6, x0, 0xaa);                                     // 420864 shufps xmm6, xmm0, 0xaa
    x4 = mulps(x4, x7);                                            // 420868 mulps xmm4, xmm7
    x2 = shufps(x2, x2, 0x1b);                                     // 42086b shufps xmm2, xmm2, 0x1b
    x2 = mulps(x2, x6);                                            // 42086f mulps xmm2, xmm6
    x6 = X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000);     // 420872 movaps xmm6, [0x93a210]
    x2 = mulps(x2, x6);                                            // 420879 mulps xmm2, xmm6
    x2 = addps(x2, x4);                                            // 42087c addps xmm2, xmm4
    x4 = x0;                                                       // 42087f movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0xff);                                     // 420882 shufps xmm4, xmm0, 0xff
    x4 = mulps(x4, x5);                                            // 420886 mulps xmm4, xmm5
    x4 = addps(x4, x3);                                            // 420889 addps xmm4, xmm3
    x2 = addps(x2, x4);                                            // 42088c addps xmm2, xmm4
    x3 = x1;                                                       // 42088f movaps xmm3, xmm1
    x3 = shufps(x3, x1, 0x00);                                     // 420892 shufps xmm3, xmm1, 0
    x0 = x2;                                                       // 420896 movaps xmm0, xmm2
    x0 = shufps(x0, x2, 0x1b);                                     // 420899 shufps xmm0, xmm2, 0x1b
    x3 = mulps(x3, x0);                                            // 42089d mulps xmm3, xmm0
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 4208a0 mulps xmm3, [0x93a1f0]
    x4 = x1;                                                       // 4208a7 movaps xmm4, xmm1
    x4 = shufps(x4, x1, 0x55);                                     // 4208aa shufps xmm4, xmm1, 0x55
    x0 = shufps(x0, x0, 0xb1);                                     // 4208ae shufps xmm0, xmm0, 0xb1
    x4 = mulps(x4, x0);                                            // 4208b2 mulps xmm4, xmm0
    x5 = x1;                                                       // 4208b5 movaps xmm5, xmm1
    x5 = shufps(x5, x1, 0xaa);                                     // 4208b8 shufps xmm5, xmm1, 0xaa
    x0 = shufps(x0, x0, 0x1b);                                     // 4208bc shufps xmm0, xmm0, 0x1b
    x4 = mulps(x4, x7);                                            // 4208c0 mulps xmm4, xmm7
    x0 = mulps(x0, x5);                                            // 4208c3 mulps xmm0, xmm5
    x0 = mulps(x0, x6);                                            // 4208c6 mulps xmm0, xmm6
    x0 = addps(x0, x4);                                            // 4208c9 addps xmm0, xmm4
    x4 = x1;                                                       // 4208cc movaps xmm4, xmm1
    x4 = shufps(x4, x1, 0xff);                                     // 4208cf shufps xmm4, xmm1, 0xff
    x4 = mulps(x4, x2);                                            // 4208d3 mulps xmm4, xmm2
    x4 = addps(x4, x3);                                            // 4208d6 addps xmm4, xmm3
    x0 = addps(x0, x4);                                            // 4208d9 addps xmm0, xmm4
    x3 = x0;                                                       // 4208dc movaps xmm3, xmm0
    x3 = mulps(x3, x0);                                            // 4208df mulps xmm3, xmm0
    x1 = x3;                                                       // 4208e2 movaps xmm1, xmm3
    x1 = shufps(x1, x3, 0xee);                                     // 4208e5 shufps xmm1, xmm3, 0xee
    x2 = x1;                                                       // 4208e9 movaps xmm2, xmm1
    x2 = addps(x2, x3);                                            // 4208ec addps xmm2, xmm3
    x2 = shufps(x2, x2, 0x40);                                     // 4208ef shufps xmm2, xmm2, 0x40
    x1 = shufps(x1, x2, 0xf0);                                     // 4208f3 shufps xmm1, xmm2, 0xf0
    x1 = addps(x1, x2);                                            // 4208f7 addps xmm1, xmm2
    st32(m, esp + 0x54, eax);                                      // 4208fa mov [esp + 0x54], eax
    st32(m, esp + 0x30, ecx);                                      // 4208fe mov [esp + 0x30], ecx
    x1 = shufps(x1, x1, 0xaa);                                     // 420902 shufps xmm1, xmm1, 0xaa
    x3 = X::v(0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000);     // 420906 movaps xmm3, [0x92dc20]
    x2 = sqrtps(x1);                                               // 42090d sqrtps xmm2, xmm1
    x1 = cmpneqps(x1, x3);                                         // 420910 cmpneqps xmm1, xmm3
    x3 = ld(m, esp + 0x110);                                       // 420914 movaps xmm3, [esp + 0x110]
    x3 = cmpneqps(x3, x2);                                         // 42091c cmpneqps xmm3, xmm2
    x0 = divps(x0, x2);                                            // 420920 divps xmm0, xmm2
    x3 = andps(x3, x0);                                            // 420923 andps xmm3, xmm0
    x0 = X::v(0x7fc00000, 0x7fc00000, 0x7fc00000, 0x7fc00000);     // 420926 movaps xmm0, [0x92dc30]
    x3 = andps(x3, x1);                                            // 42092d andps xmm3, xmm1
    edx = ebx + eax * 4 + 0x3980;                                  // 420930 lea edx, [ebx + eax*4 + 0x3980]
    ecx = esp + 0x140;                                             // 420937 lea ecx, [esp + 0x140]
    eax = ebx + eax * 4 + 0x3930;                                  // 42093e lea eax, [ebx + eax*4 + 0x3930]
    x1 = andnps(x1, x0);                                           // 420945 andnps xmm1, xmm0
    k.push(ecx);                                                   // 420948 push ecx
    x3 = orps(x3, x1);                                             // 420949 orps xmm3, xmm1
    ecx = eax;                                                     // 42094c mov ecx, eax
    edi = esp + 0x2c;                                              // 42094e lea edi, [esp + 0x2c]
    esi = esp + 0x28;                                              // 420952 lea esi, [esp + 0x28]
    st(m, esp + 0x144, x3);                                        // 420956 movaps [esp + 0x144], xmm3
    k.call(0x00420160);  // FUN_00420160                           // 42095e call 0x420160
    eax = ld32(m, esp + 0x54);                                     // 420963 mov eax, [esp + 0x54]
    fpush(m, reflex_f80::from_f32(ld32(m, ebx + eax * 4 + 0x3980))); // 420967 fld [ebx + eax*4 + 0x3980]
    x0 = X::ss(ld32(m, esp + 0x24));                               // 42096e movss xmm0, [esp + 0x24]
    fpush(m, reflex_f80::from_f32(ld32(m, ebx + eax * 4 + 0x3930))); // 420974 fld [ebx + eax*4 + 0x3930]
    edi = ebx + eax * 4 + 0x3980;                                  // 42097b lea edi, [ebx + eax*4 + 0x3980]
    eax = ebx + eax * 4 + 0x3930;                                  // 420982 lea eax, [ebx + eax*4 + 0x3930]
    st32(m, esp + 0x4c, x0.u[0]);                                  // 420989 movss [esp + 0x4c], xmm0
    st32(m, esp + 0x50, eax);                                      // 42098f mov [esp + 0x50], eax
    k.call(0x009009c0);  // MSVCR90.DLL::_CIatan2                  // 420993 call 0x9009c0
    st32(m, esp + 0x20, reflex_f80::to_f32(fst_(m, 0), frc(m))); m.fpu.pop_back(); // 420998 fstp [esp + 0x20]
    fpush(m, reflex_f80::from_f32(ld32(m, esp + 0x20)));           // 42099c fld [esp + 0x20]
    esi = ld32(m, esp + 0x30);                                     // 4209a0 mov esi, [esp + 0x30]
    edx = esp + 0x4c;                                              // 4209a4 lea edx, [esp + 0x4c]
    st32(m, esp + 0x20, reflex_f80::to_f32(fst_(m, 0), frc(m))); m.fpu.pop_back(); // 4209a8 fstp [esp + 0x20]
    k.push(edx);                                                   // 4209ac push edx
    eax = esp + 0x24;                                              // 4209ad lea eax, [esp + 0x24]
    k.push(eax);                                                   // 4209b1 push eax
    esi = esi + 0x1b30u;                                           // 4209b2 add esi, 0x1b30
    k.call(0x0041e080);  // FUN_0041e080                           // 4209b8 call 0x41e080
    ecx = ld32(m, esp + 0x30);                                     // 4209bd mov ecx, [esp + 0x30]
    x0 = X::ss(ld32(m, ecx + 0x1c10));                             // 4209c1 movss xmm0, [ecx + 0x1c10]
    esi = ld32(m, esp + 0x54);                                     // 4209c9 mov esi, [esp + 0x54]
    x1 = X::ss(ld32(m, esp + 0x28));                               // 4209cd movss xmm1, [esp + 0x28]
    eax = 0u; fl = f_logic(0u);                                    // 4209d3 xor eax, eax
    x2 = cvtps2pd(x1);                                             // 4209d5 cvtps2pd xmm2, xmm1
    x3 = cvtps2pd(x0);                                             // 4209d8 cvtps2pd xmm3, xmm0
    fl = f_comisd(x2.q(0), x3.q(0));                               // 4209db comisd xmm2, xmm3
    st32(m, ebx + esi * 4 + 0x3840, eax);                          // 4209df mov [ebx + esi*4 + 0x3840], eax
    st32(m, ebx + esi * 4 + 0x3890, eax);                          // 4209e6 mov [ebx + esi*4 + 0x3890], eax
    st32(m, ebx + esi * 4 + 0x38e0, eax);                          // 4209ed mov [ebx + esi*4 + 0x38e0], eax
    if (fl.be()) goto L_00420a19;                                  // 4209f4 jbe 0x420a19
    x1 = X::ss(ld32(m, 0xd72ac8));                                 // 4209f6 movss xmm1, [0xd72ac8]
    x0 = cvtps2pd(x0);                                             // 4209fe cvtps2pd xmm0, xmm0
    x1 = cvtps2pd(x1);                                             // 420a01 cvtps2pd xmm1, xmm1
    x0 = subsd(x0, x1);                                            // 420a04 subsd xmm0, xmm1
    x0 = cvtpd2ps(x0);                                             // 420a08 cvtpd2ps xmm0, xmm0
    st32(m, ebx + esi * 4 + 0x3840, 0x1u);                         // 420a0c mov [ebx + esi*4 + 0x3840], 1
    goto L_00420a52;                                               // 420a17 jmp 0x420a52
L_00420a19:
    x2 = X::ss(0x80000000u);                                       // 420a19 movss xmm2, [0x9774a8]
    x2 = subss(x2, x0);                                            // 420a21 subss xmm2, xmm0
    x1 = cvtps2pd(x1);                                             // 420a25 cvtps2pd xmm1, xmm1
    x2 = cvtps2pd(x2);                                             // 420a28 cvtps2pd xmm2, xmm2
    fl = f_comisd(x2.q(0), x1.q(0));                               // 420a2b comisd xmm2, xmm1
    if (fl.be()) goto L_00420a58;                                  // 420a2f jbe 0x420a58
    x1 = X::ss(ld32(m, 0xd72ac8));                                 // 420a31 movss xmm1, [0xd72ac8]
    x0 = cvtps2pd(x0);                                             // 420a39 cvtps2pd xmm0, xmm0
    x1 = cvtps2pd(x1);                                             // 420a3c cvtps2pd xmm1, xmm1
    x1 = subsd(x1, x0);                                            // 420a3f subsd xmm1, xmm0
    x0 = cvtpd2ps(x1);                                             // 420a43 cvtpd2ps xmm0, xmm1
    st32(m, ebx + esi * 4 + 0x3890, 0x1u);                         // 420a47 mov [ebx + esi*4 + 0x3890], 1
L_00420a52:
    st32(m, esp + 0x28, x0.u[0]);                                  // 420a52 movss [esp + 0x28], xmm0
L_00420a58:
    x0 = X::ss(ld32(m, esp + 0x4c));                               // 420a58 movss xmm0, [esp + 0x4c]
    x1 = cvtss2sd(x1, X::ss(ld32(m, esp + 0x24)));                 // 420a5e cvtss2sd xmm1, [esp + 0x24]
    x2 = cvtps2pd(x0);                                             // 420a64 cvtps2pd xmm2, xmm0
    fl = f_comisd(x1.q(0), x2.q(0));                               // 420a67 comisd xmm1, xmm2
    if (fl.be()) goto L_00420bce;                                  // 420a6b jbe 0x420bce
    x1 = X::ss(ld32(m, 0xd72acc));                                 // 420a71 movss xmm1, [0xd72acc]
    x0 = cvtps2pd(x0);                                             // 420a79 cvtps2pd xmm0, xmm0
    x1 = cvtps2pd(x1);                                             // 420a7c cvtps2pd xmm1, xmm1
    x0 = subsd(x0, x1);                                            // 420a7f subsd xmm0, xmm1
    x0 = cvtpd2ps(x0);                                             // 420a83 cvtpd2ps xmm0, xmm0
    st32(m, esp + 0x24, x0.u[0]);                                  // 420a87 movss [esp + 0x24], xmm0
    x0 = X::ss(ld32(m, esp + 0x20));                               // 420a8d movss xmm0, [esp + 0x20]
    k.call(0x00401000);  // FUN_00401000                           // 420a93 call 0x401000
    eax = ld32(m, esp + 0x50);                                     // 420a98 mov eax, [esp + 0x50]
    x1 = x0;                                                       // 420a9c movaps xmm1, xmm0
    x1 = mulss(x1, X::ss(0x3e22f983u));                            // 420a9f mulss xmm1, [0x9774a0]
    edx = uint32_t(cvtt(x1.f(0)));                                 // 420aa7 cvttss2si edx, xmm1
    x1 = cvtsi2ss(x1, int32_t(edx));                               // 420aab cvtsi2ss xmm1, edx
    x1 = mulss(x1, X::ss(0x40c90fdbu));                            // 420aaf mulss xmm1, [0x97749c]
    x0 = subss(x0, x1);                                            // 420ab7 subss xmm0, xmm1
    x1 = X::ss(0x40490fdbu);                                       // 420abb movss xmm1, [0x977498]
    x1 = subss(x1, x0);                                            // 420ac3 subss xmm1, xmm0
    x0 = x1;                                                       // 420ac7 movaps xmm0, xmm1
    x0 = mulss(x0, x1);                                            // 420aca mulss xmm0, xmm1
    x2 = x0;                                                       // 420ace movaps xmm2, xmm0
    x2 = mulss(x2, X::ss(0xb46beed0u));                            // 420ad1 mulss xmm2, [0x935d50]
    x2 = addss(x2, X::ss(0x37cb076bu));                            // 420ad9 addss xmm2, [0x935d48]
    x1 = x0;                                                       // 420ae1 movaps xmm1, xmm0
    x1 = mulss(x1, x0);                                            // 420ae4 mulss xmm1, xmm0
    x3 = x0;                                                       // 420ae8 movaps xmm3, xmm0
    x0 = mulss(x0, X::ss(0xbeffff3du));                            // 420aeb mulss xmm0, [0x935d40]
    x0 = addss(x0, X::ss(0x3f7ffff3u));                            // 420af3 addss xmm0, [0x935d3c]
    x3 = mulss(x3, X::ss(0xbab5a662u));                            // 420afb mulss xmm3, [0x935d44]
    x3 = addss(x3, X::ss(0x3d2aa372u));                            // 420b03 addss xmm3, [0x935d4c]
    x2 = mulss(x2, x1);                                            // 420b0b mulss xmm2, xmm1
    x2 = addss(x2, x3);                                            // 420b0f addss xmm2, xmm3
    x2 = mulss(x2, x1);                                            // 420b13 mulss xmm2, xmm1
    x2 = addss(x2, x0);                                            // 420b17 addss xmm2, xmm0
    x0 = X::ss(0x80000000u);                                       // 420b1b movss xmm0, [0x9774a8]
    x0 = subss(x0, x2);                                            // 420b23 subss xmm0, xmm2
    st32(m, eax, x0.u[0]);                                         // 420b27 movss [eax], xmm0
    x0 = X::ss(ld32(m, esp + 0x20));                               // 420b2b movss xmm0, [esp + 0x20]
    x0 = addss(x0, X::ss(0x3fc90fdbu));                            // 420b31 addss xmm0, [0x977508]
    k.call(0x00401000);  // FUN_00401000                           // 420b39 call 0x401000
    x1 = x0;                                                       // 420b3e movaps xmm1, xmm0
    x1 = mulss(x1, X::ss(0x3e22f983u));                            // 420b41 mulss xmm1, [0x9774a0]
    ecx = uint32_t(cvtt(x1.f(0)));                                 // 420b49 cvttss2si ecx, xmm1
    x1 = cvtsi2ss(x1, int32_t(ecx));                               // 420b4d cvtsi2ss xmm1, ecx
    x1 = mulss(x1, X::ss(0x40c90fdbu));                            // 420b51 mulss xmm1, [0x97749c]
    x0 = subss(x0, x1);                                            // 420b59 subss xmm0, xmm1
    x1 = X::ss(0x40490fdbu);                                       // 420b5d movss xmm1, [0x977498]
    x1 = subss(x1, x0);                                            // 420b65 subss xmm1, xmm0
    x0 = x1;                                                       // 420b69 movaps xmm0, xmm1
    x0 = mulss(x0, x1);                                            // 420b6c mulss xmm0, xmm1
    x2 = x0;                                                       // 420b70 movaps xmm2, xmm0
    x2 = mulss(x2, X::ss(0xb46beed0u));                            // 420b73 mulss xmm2, [0x935d50]
    x2 = addss(x2, X::ss(0x37cb076bu));                            // 420b7b addss xmm2, [0x935d48]
    x1 = x0;                                                       // 420b83 movaps xmm1, xmm0
    x1 = mulss(x1, x0);                                            // 420b86 mulss xmm1, xmm0
    x3 = x0;                                                       // 420b8a movaps xmm3, xmm0
    x3 = mulss(x3, X::ss(0xbab5a662u));                            // 420b8d mulss xmm3, [0x935d44]
    x3 = addss(x3, X::ss(0x3d2aa372u));                            // 420b95 addss xmm3, [0x935d4c]
    x0 = mulss(x0, X::ss(0xbeffff3du));                            // 420b9d mulss xmm0, [0x935d40]
    x0 = addss(x0, X::ss(0x3f7ffff3u));                            // 420ba5 addss xmm0, [0x935d3c]
    x2 = mulss(x2, x1);                                            // 420bad mulss xmm2, xmm1
    x2 = addss(x2, x3);                                            // 420bb1 addss xmm2, xmm3
    x2 = mulss(x2, x1);                                            // 420bb5 mulss xmm2, xmm1
    x2 = addss(x2, x0);                                            // 420bb9 addss xmm2, xmm0
    st32(m, edi, x2.u[0]);                                         // 420bbd movss [edi], xmm2
    st32(m, ebx + esi * 4 + 0x38e0, 0x1u);                         // 420bc1 mov [ebx + esi*4 + 0x38e0], 1
    eax = 0u;                                                      // 420bcc xor eax, eax
L_00420bce:
    fl = f_cmp(ld32(m, ebx + esi * 4 + 0x38e0), eax);              // 420bce cmp [ebx + esi*4 + 0x38e0], eax
    if (fl.ne()) goto L_00420bed;                                  // 420bd5 jne 0x420bed
    fl = f_cmp(ld32(m, ebx + esi * 4 + 0x3840), eax);              // 420bd7 cmp [ebx + esi*4 + 0x3840], eax
    if (fl.ne()) goto L_00420bed;                                  // 420bde jne 0x420bed
    fl = f_cmp(ld32(m, ebx + esi * 4 + 0x3890), eax);              // 420be0 cmp [ebx + esi*4 + 0x3890], eax
    if (fl.e()) goto L_00420f6b;                                   // 420be7 je 0x420f6b
L_00420bed:
    x0 = cvtss2sd(x0, X::ss(ld32(m, esp + 0x24)));                 // 420bed cvtss2sd xmm0, [esp + 0x24]
    x0 = mulsd(x0, sd(0x3fe0000000000000ull));                     // 420bf3 mulsd xmm0, [0x9774e8]
    edx = ld32(m, esp + 0x64);                                     // 420bfb mov edx, [esp + 0x64]
    x0 = cvtsd2ss(x0, x0);                                         // 420bff cvtsd2ss xmm0, xmm0
    st32(m, esp + 0x68, x0.u[0]);                                  // 420c03 movss [esp + 0x68], xmm0
    x0 = addss(x0, X::ss(0x3fc90fdbu));                            // 420c09 addss xmm0, [0x977508]
    st32(m, edx, 0x1u);                                            // 420c11 mov [edx], 1
    k.call(0x00401000);  // FUN_00401000                           // 420c17 call 0x401000
    x1 = x0;                                                       // 420c1c movaps xmm1, xmm0
    x1 = mulss(x1, X::ss(0x3e22f983u));                            // 420c1f mulss xmm1, [0x9774a0]
    eax = uint32_t(cvtt(x1.f(0)));                                 // 420c27 cvttss2si eax, xmm1
    x1 = cvtsi2ss(x1, int32_t(eax));                               // 420c2b cvtsi2ss xmm1, eax
    x1 = mulss(x1, X::ss(0x40c90fdbu));                            // 420c2f mulss xmm1, [0x97749c]
    x0 = subss(x0, x1);                                            // 420c37 subss xmm0, xmm1
    x1 = X::ss(0x40490fdbu);                                       // 420c3b movss xmm1, [0x977498]
    x1 = subss(x1, x0);                                            // 420c43 subss xmm1, xmm0
    x0 = x1;                                                       // 420c47 movaps xmm0, xmm1
    x0 = mulss(x0, x1);                                            // 420c4a mulss xmm0, xmm1
    x1 = x0;                                                       // 420c4e movaps xmm1, xmm0
    x1 = mulss(x1, X::ss(0xb46beed0u));                            // 420c51 mulss xmm1, [0x935d50]
    x1 = addss(x1, X::ss(0x37cb076bu));                            // 420c59 addss xmm1, [0x935d48]
    x2 = x0;                                                       // 420c61 movaps xmm2, xmm0
    x2 = mulss(x2, x0);                                            // 420c64 mulss xmm2, xmm0
    x3 = x0;                                                       // 420c68 movaps xmm3, xmm0
    x3 = mulss(x3, X::ss(0xbab5a662u));                            // 420c6b mulss xmm3, [0x935d44]
    x3 = addss(x3, X::ss(0x3d2aa372u));                            // 420c73 addss xmm3, [0x935d4c]
    x0 = mulss(x0, X::ss(0xbeffff3du));                            // 420c7b mulss xmm0, [0x935d40]
    x0 = addss(x0, X::ss(0x3f7ffff3u));                            // 420c83 addss xmm0, [0x935d3c]
    x1 = mulss(x1, x2);                                            // 420c8b mulss xmm1, xmm2
    x1 = addss(x1, x3);                                            // 420c8f addss xmm1, xmm3
    x1 = mulss(x1, x2);                                            // 420c93 mulss xmm1, xmm2
    x1 = addss(x1, x0);                                            // 420c97 addss xmm1, xmm0
    x0 = X::ss(ld32(m, esp + 0x68));                               // 420c9b movss xmm0, [esp + 0x68]
    st32(m, esp + 0x5c, x1.u[0]);                                  // 420ca1 movss [esp + 0x5c], xmm1
    k.call(0x00401000);  // FUN_00401000                           // 420ca7 call 0x401000
    x1 = x0;                                                       // 420cac movaps xmm1, xmm0
    x1 = mulss(x1, X::ss(0x3e22f983u));                            // 420caf mulss xmm1, [0x9774a0]
    ecx = uint32_t(cvtt(x1.f(0)));                                 // 420cb7 cvttss2si ecx, xmm1
    x1 = cvtsi2ss(x1, int32_t(ecx));                               // 420cbb cvtsi2ss xmm1, ecx
    x1 = mulss(x1, X::ss(0x40c90fdbu));                            // 420cbf mulss xmm1, [0x97749c]
    x0 = subss(x0, x1);                                            // 420cc7 subss xmm0, xmm1
    x1 = X::ss(0x40490fdbu);                                       // 420ccb movss xmm1, [0x977498]
    x1 = subss(x1, x0);                                            // 420cd3 subss xmm1, xmm0
    x0 = x1;                                                       // 420cd7 movaps xmm0, xmm1
    x0 = mulss(x0, x1);                                            // 420cda mulss xmm0, xmm1
    x2 = x0;                                                       // 420cde movaps xmm2, xmm0
    x2 = mulss(x2, X::ss(0xb46beed0u));                            // 420ce1 mulss xmm2, [0x935d50]
    x2 = addss(x2, X::ss(0x37cb076bu));                            // 420ce9 addss xmm2, [0x935d48]
    x1 = x0;                                                       // 420cf1 movaps xmm1, xmm0
    x1 = mulss(x1, x0);                                            // 420cf4 mulss xmm1, xmm0
    x3 = x0;                                                       // 420cf8 movaps xmm3, xmm0
    x0 = mulss(x0, X::ss(0xbeffff3du));                            // 420cfb mulss xmm0, [0x935d40]
    x0 = addss(x0, X::ss(0x3f7ffff3u));                            // 420d03 addss xmm0, [0x935d3c]
    x3 = mulss(x3, X::ss(0xbab5a662u));                            // 420d0b mulss xmm3, [0x935d44]
    x3 = addss(x3, X::ss(0x3d2aa372u));                            // 420d13 addss xmm3, [0x935d4c]
    x2 = mulss(x2, x1);                                            // 420d1b mulss xmm2, xmm1
    x2 = addss(x2, x3);                                            // 420d1f addss xmm2, xmm3
    x2 = mulss(x2, x1);                                            // 420d23 mulss xmm2, xmm1
    x2 = addss(x2, x0);                                            // 420d27 addss xmm2, xmm0
    x0 = X::ss(0x80000000u);                                       // 420d2b movss xmm0, [0x9774a8]
    x0 = subss(x0, x2);                                            // 420d33 subss xmm0, xmm2
    st32(m, esp + 0x58, x0.u[0]);                                  // 420d37 movss [esp + 0x58], xmm0
    x0 = cvtss2sd(x0, X::ss(ld32(m, esp + 0x28)));                 // 420d3d cvtss2sd xmm0, [esp + 0x28]
    x0 = mulsd(x0, sd(0x3fe0000000000000ull));                     // 420d43 mulsd xmm0, [0x9774e8]
    x0 = cvtpd2ps(x0);                                             // 420d4b cvtpd2ps xmm0, xmm0
    st32(m, esp + 0x60, x0.u[0]);                                  // 420d4f movss [esp + 0x60], xmm0
    x0 = addss(x0, X::ss(0x3fc90fdbu));                            // 420d55 addss xmm0, [0x977508]
    k.call(0x00401000);  // FUN_00401000                           // 420d5d call 0x401000
    x1 = x0;                                                       // 420d62 movaps xmm1, xmm0
    x1 = mulss(x1, X::ss(0x3e22f983u));                            // 420d65 mulss xmm1, [0x9774a0]
    edx = uint32_t(cvtt(x1.f(0)));                                 // 420d6d cvttss2si edx, xmm1
    x1 = cvtsi2ss(x1, int32_t(edx));                               // 420d71 cvtsi2ss xmm1, edx
    x1 = mulss(x1, X::ss(0x40c90fdbu));                            // 420d75 mulss xmm1, [0x97749c]
    x0 = subss(x0, x1);                                            // 420d7d subss xmm0, xmm1
    x1 = X::ss(0x40490fdbu);                                       // 420d81 movss xmm1, [0x977498]
    x1 = subss(x1, x0);                                            // 420d89 subss xmm1, xmm0
    x0 = x1;                                                       // 420d8d movaps xmm0, xmm1
    x0 = mulss(x0, x1);                                            // 420d90 mulss xmm0, xmm1
    x2 = x0;                                                       // 420d94 movaps xmm2, xmm0
    x2 = mulss(x2, x0);                                            // 420d97 mulss xmm2, xmm0
    x1 = x0;                                                       // 420d9b movaps xmm1, xmm0
    x1 = mulss(x1, X::ss(0xb46beed0u));                            // 420d9e mulss xmm1, [0x935d50]
    x1 = addss(x1, X::ss(0x37cb076bu));                            // 420da6 addss xmm1, [0x935d48]
    x3 = x0;                                                       // 420dae movaps xmm3, xmm0
    x3 = mulss(x3, X::ss(0xbab5a662u));                            // 420db1 mulss xmm3, [0x935d44]
    x3 = addss(x3, X::ss(0x3d2aa372u));                            // 420db9 addss xmm3, [0x935d4c]
    x0 = mulss(x0, X::ss(0xbeffff3du));                            // 420dc1 mulss xmm0, [0x935d40]
    x0 = addss(x0, X::ss(0x3f7ffff3u));                            // 420dc9 addss xmm0, [0x935d3c]
    x1 = mulss(x1, x2);                                            // 420dd1 mulss xmm1, xmm2
    x1 = addss(x1, x3);                                            // 420dd5 addss xmm1, xmm3
    x1 = mulss(x1, x2);                                            // 420dd9 mulss xmm1, xmm2
    x1 = addss(x1, x0);                                            // 420ddd addss xmm1, xmm0
    x0 = X::ss(ld32(m, esp + 0x60));                               // 420de1 movss xmm0, [esp + 0x60]
    st32(m, esp + 0x2c, x1.u[0]);                                  // 420de7 movss [esp + 0x2c], xmm1
    k.call(0x00401000);  // FUN_00401000                           // 420ded call 0x401000
    x1 = x0;                                                       // 420df2 movaps xmm1, xmm0
    x1 = mulss(x1, X::ss(0x3e22f983u));                            // 420df5 mulss xmm1, [0x9774a0]
    x4 = cvtss2sd(x4, X::ss(ld32(m, esp + 0x58)));                 // 420dfd cvtss2sd xmm4, [esp + 0x58]
    ecx = ld32(m, esp + 0x50);                                     // 420e03 mov ecx, [esp + 0x50]
    eax = uint32_t(cvtt(x1.f(0)));                                 // 420e07 cvttss2si eax, xmm1
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x2c)));                 // 420e0b cvtss2sd xmm7, [esp + 0x2c]
    x1 = cvtsi2ss(x1, int32_t(eax));                               // 420e11 cvtsi2ss xmm1, eax
    x1 = mulss(x1, X::ss(0x40c90fdbu));                            // 420e15 mulss xmm1, [0x97749c]
    x0 = subss(x0, x1);                                            // 420e1d subss xmm0, xmm1
    x1 = X::ss(0x40490fdbu);                                       // 420e21 movss xmm1, [0x977498]
    x1 = subss(x1, x0);                                            // 420e29 subss xmm1, xmm0
    x0 = x1;                                                       // 420e2d movaps xmm0, xmm1
    x0 = mulss(x0, x1);                                            // 420e30 mulss xmm0, xmm1
    x1 = x0;                                                       // 420e34 movaps xmm1, xmm0
    x1 = mulss(x1, x0);                                            // 420e37 mulss xmm1, xmm0
    x2 = x0;                                                       // 420e3b movaps xmm2, xmm0
    x2 = mulss(x2, X::ss(0xb46beed0u));                            // 420e3e mulss xmm2, [0x935d50]
    x2 = addss(x2, X::ss(0x37cb076bu));                            // 420e46 addss xmm2, [0x935d48]
    x3 = x0;                                                       // 420e4e movaps xmm3, xmm0
    x0 = mulss(x0, X::ss(0xbeffff3du));                            // 420e51 mulss xmm0, [0x935d40]
    x0 = addss(x0, X::ss(0x3f7ffff3u));                            // 420e59 addss xmm0, [0x935d3c]
    x3 = mulss(x3, X::ss(0xbab5a662u));                            // 420e61 mulss xmm3, [0x935d44]
    x3 = addss(x3, X::ss(0x3d2aa372u));                            // 420e69 addss xmm3, [0x935d4c]
    x2 = mulss(x2, x1);                                            // 420e71 mulss xmm2, xmm1
    x2 = addss(x2, x3);                                            // 420e75 addss xmm2, xmm3
    x3 = X::ss(ld32(m, edi));                                      // 420e79 movss xmm3, [edi]
    x2 = mulss(x2, x1);                                            // 420e7d mulss xmm2, xmm1
    x2 = addss(x2, x0);                                            // 420e81 addss xmm2, xmm0
    x0 = X::ss(0x80000000u);                                       // 420e85 movss xmm0, [0x9774a8]
    x0 = subss(x0, x2);                                            // 420e8d subss xmm0, xmm2
    x2 = X::ss(ld32(m, ecx));                                      // 420e91 movss xmm2, [ecx]
    x1 = cvtps2pd(x0);                                             // 420e95 cvtps2pd xmm1, xmm0
    x5 = cvtps2pd(x0);                                             // 420e98 cvtps2pd xmm5, xmm0
    x1 = mulsd(x1, x4);                                            // 420e9b mulsd xmm1, xmm4
    x4 = cvtpd2ps(x1);                                             // 420e9f cvtpd2ps xmm4, xmm1
    x1 = X::ss(ld32(m, esp + 0x5c));                               // 420ea3 movss xmm1, [esp + 0x5c]
    x6 = cvtps2pd(x1);                                             // 420ea9 cvtps2pd xmm6, xmm1
    x5 = mulsd(x5, x6);                                            // 420eac mulsd xmm5, xmm6
    x6 = cvtps2pd(x2);                                             // 420eb0 cvtps2pd xmm6, xmm2
    x5 = mulsd(x5, x6);                                            // 420eb3 mulsd xmm5, xmm6
    x6 = cvtps2pd(x3);                                             // 420eb7 cvtps2pd xmm6, xmm3
    x6 = mulsd(x6, x7);                                            // 420eba mulsd xmm6, xmm7
    x7 = cvtps2pd(x1);                                             // 420ebe cvtps2pd xmm7, xmm1
    x6 = mulsd(x6, x7);                                            // 420ec1 mulsd xmm6, xmm7
    x5 = addsd(x5, x6);                                            // 420ec5 addsd xmm5, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x2c)));                 // 420ec9 cvtss2sd xmm6, [esp + 0x2c]
    x2 = cvtps2pd(x2);                                             // 420ecf cvtps2pd xmm2, xmm2
    x0 = cvtps2pd(x0);                                             // 420ed2 cvtps2pd xmm0, xmm0
    x7 = cvtps2pd(x1);                                             // 420ed5 cvtps2pd xmm7, xmm1
    x6 = mulsd(x6, x7);                                            // 420ed8 mulsd xmm6, xmm7
    x6 = mulsd(x6, x2);                                            // 420edc mulsd xmm6, xmm2
    x2 = cvtps2pd(x3);                                             // 420ee0 cvtps2pd xmm2, xmm3
    x2 = mulsd(x2, x0);                                            // 420ee3 mulsd xmm2, xmm0
    x0 = cvtps2pd(x1);                                             // 420ee7 cvtps2pd xmm0, xmm1
    x2 = mulsd(x2, x0);                                            // 420eea mulsd xmm2, xmm0
    x0 = cvtss2sd(x0, X::ss(ld32(m, esp + 0x2c)));                 // 420eee cvtss2sd xmm0, [esp + 0x2c]
    x6 = subsd(x6, x2);                                            // 420ef4 subsd xmm6, xmm2
    x2 = cvtss2sd(x2, X::ss(ld32(m, esp + 0x58)));                 // 420ef8 cvtss2sd xmm2, [esp + 0x58]
    x0 = mulsd(x0, x2);                                            // 420efe mulsd xmm0, xmm2
    x0 = cvtpd2ps(x0);                                             // 420f02 cvtpd2ps xmm0, xmm0
    x5 = cvtsd2ss(x5, x5);                                         // 420f06 cvtsd2ss xmm5, xmm5
    x1 = cvtsd2ss(x1, x6);                                         // 420f0a cvtsd2ss xmm1, xmm6
    x0 = unpcklps(x0, x5);                                         // 420f0e unpcklps xmm0, xmm5
    x1 = unpcklps(x1, x4);                                         // 420f11 unpcklps xmm1, xmm4
    x0 = unpcklps(x0, x1);                                         // 420f14 unpcklps xmm0, xmm1
    x3 = x0;                                                       // 420f17 movaps xmm3, xmm0
    x3 = mulps(x3, x0);                                            // 420f1a mulps xmm3, xmm0
    x1 = x3;                                                       // 420f1d movaps xmm1, xmm3
    x1 = shufps(x1, x3, 0xee);                                     // 420f20 shufps xmm1, xmm3, 0xee
    x2 = x1;                                                       // 420f24 movaps xmm2, xmm1
    x2 = addps(x2, x3);                                            // 420f27 addps xmm2, xmm3
    x3 = X::v(0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000);     // 420f2a movaps xmm3, [0x92dc20]
    x2 = shufps(x2, x2, 0x40);                                     // 420f31 shufps xmm2, xmm2, 0x40
    x1 = shufps(x1, x2, 0xf0);                                     // 420f35 shufps xmm1, xmm2, 0xf0
    x1 = addps(x1, x2);                                            // 420f39 addps xmm1, xmm2
    x1 = shufps(x1, x1, 0xaa);                                     // 420f3c shufps xmm1, xmm1, 0xaa
    x2 = sqrtps(x1);                                               // 420f40 sqrtps xmm2, xmm1
    x1 = cmpneqps(x1, x3);                                         // 420f43 cmpneqps xmm1, xmm3
    x3 = ld(m, esp + 0x110);                                       // 420f47 movaps xmm3, [esp + 0x110]
    x0 = divps(x0, x2);                                            // 420f4f divps xmm0, xmm2
    x3 = cmpneqps(x3, x2);                                         // 420f52 cmpneqps xmm3, xmm2
    x2 = X::v(0x7fc00000, 0x7fc00000, 0x7fc00000, 0x7fc00000);     // 420f56 movaps xmm2, [0x92dc30]
    x0 = andps(x0, x3);                                            // 420f5d andps xmm0, xmm3
    x0 = andps(x0, x1);                                            // 420f60 andps xmm0, xmm1
    x1 = andnps(x1, x2);                                           // 420f63 andnps xmm1, xmm2
    x0 = orps(x0, x1);                                             // 420f66 orps xmm0, xmm1
    goto L_00420f73;                                               // 420f69 jmp 0x420f73
L_00420f6b:
    x0 = ld(m, esp + 0x140);                                       // 420f6b movaps xmm0, [esp + 0x140]
L_00420f73:
    edx = ld32(m, esp + 0x6c);                                     // 420f73 mov edx, [esp + 0x6c]
    eax = ld32(m, edx);                                            // 420f77 mov eax, [edx]
    st32(m, esp + 0x18, ld32(m, esp + 0x18) + 0x4u);               // 420f79 add [esp + 0x18], 4
    eax = eax + 0x19du;                                            // 420f7e add eax, 0x19d
    eax = eax << 4;                                                // 420f83 shl eax, 4
    st(m, eax + ebx, x0);                                          // 420f86 movaps [eax + ebx], xmm0
    eax = ld32(m, esp + 0x1c);                                     // 420f8a mov eax, [esp + 0x1c]
    eax = eax + 1u;                                                // 420f8e inc eax
    fl = f_cmp(eax, ld32(m, ebx + 0x1b20));                        // 420f8f cmp eax, [ebx + 0x1b20]
    st32(m, esp + 0x1c, eax);                                      // 420f95 mov [esp + 0x1c], eax
    if (fl.l()) goto L_004207ce;                                   // 420f99 jl 0x4207ce
L_00420f9f:
    eax = ld32(m, ebx + 0x1b20);                                   // 420f9f mov eax, [ebx + 0x1b20]
    eax = eax - 1u;                                                // 420fa5 dec eax
    fl = f_cmp(eax, 0xffffffffu);                                  // 420fa6 cmp eax, -1
    if (fl.le()) goto L_0042111a;                                  // 420fa9 jle 0x42111a
    edx = ebx + eax * 4 + 0x37f0;                                  // 420faf lea edx, [ebx + eax*4 + 0x37f0]
    esi = eax + 0x1;                                               // 420fb6 lea esi, [eax + 1]
    esp = esp;                                                     // 420fb9 lea esp, [esp]
L_00420fc0:
    eax = ld32(m, edx);                                            // 420fc0 mov eax, [edx]
    fl = f_cmp(ld32(m, esp + eax * 4 + 0xa0), 0x0u);               // 420fc2 cmp [esp + eax*4 + 0xa0], 0
    if (fl.e()) goto L_0042110e;                                   // 420fca je 0x42110e
    ecx = eax + 0x13;                                              // 420fd0 lea ecx, [eax + 0x13]
    eax = eax * 0x170u;                                            // 420fd3 imul eax, eax, 0x170
    x0 = ld(m, eax + ebx + 0x1be0);                                // 420fd9 movaps xmm0, [eax + ebx + 0x1be0]
    ecx = ecx * 0x170u;                                            // 420fe1 imul ecx, ecx, 0x170
    edi = ld32(m, ecx + ebx);                                      // 420fe7 mov edi, [ecx + ebx]
    x7 = X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000);     // 420fea movaps xmm7, [0x93a200]
    x3 = x0;                                                       // 420ff1 movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0x00);                                     // 420ff4 shufps xmm3, xmm0, 0
    ecx = ecx + ebx;                                               // 420ff8 add ecx, ebx
    x4 = x0;                                                       // 420ffa movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x55);                                     // 420ffd shufps xmm4, xmm0, 0x55
    x6 = x0;                                                       // 421001 movaps xmm6, xmm0
    x6 = shufps(x6, x0, 0xaa);                                     // 421004 shufps xmm6, xmm0, 0xaa
    edi = edi + 0x19du;                                            // 421008 add edi, 0x19d
    eax = eax + ebx;                                               // 42100e add eax, ebx
    eax = ld32(m, eax + 0x1b30);                                   // 421010 mov eax, [eax + 0x1b30]
    edi = edi << 4;                                                // 421016 shl edi, 4
    eax = eax * 0x70u;                                             // 421019 imul eax, eax, 0x70
    x5 = ld(m, edi + ebx);                                         // 42101c movaps xmm5, [edi + ebx]
    x2 = ld(m, eax + ebx + 0xb00);                                 // 421020 movaps xmm2, [eax + ebx + 0xb00]
    x1 = x5;                                                       // 421028 movaps xmm1, xmm5
    x1 = shufps(x1, x5, 0x1b);                                     // 42102b shufps xmm1, xmm5, 0x1b
    x3 = mulps(x3, x1);                                            // 42102f mulps xmm3, xmm1
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 421032 mulps xmm3, [0x93a1f0]
    x1 = shufps(x1, x1, 0xb1);                                     // 421039 shufps xmm1, xmm1, 0xb1
    x4 = mulps(x4, x1);                                            // 42103d mulps xmm4, xmm1
    x1 = shufps(x1, x1, 0x1b);                                     // 421040 shufps xmm1, xmm1, 0x1b
    x1 = mulps(x1, x6);                                            // 421044 mulps xmm1, xmm6
    x6 = X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000);     // 421047 movaps xmm6, [0x93a210]
    x4 = mulps(x4, x7);                                            // 42104e mulps xmm4, xmm7
    x1 = mulps(x1, x6);                                            // 421051 mulps xmm1, xmm6
    x1 = addps(x1, x4);                                            // 421054 addps xmm1, xmm4
    x4 = x0;                                                       // 421057 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0xff);                                     // 42105a shufps xmm4, xmm0, 0xff
    x4 = mulps(x4, x5);                                            // 42105e mulps xmm4, xmm5
    x4 = addps(x4, x3);                                            // 421061 addps xmm4, xmm3
    x1 = addps(x1, x4);                                            // 421064 addps xmm1, xmm4
    x0 = x1;                                                       // 421067 movaps xmm0, xmm1
    x0 = shufps(x0, x1, 0x1b);                                     // 42106a shufps xmm0, xmm1, 0x1b
    x3 = x2;                                                       // 42106e movaps xmm3, xmm2
    x3 = shufps(x3, x2, 0x00);                                     // 421071 shufps xmm3, xmm2, 0
    x3 = mulps(x3, x0);                                            // 421075 mulps xmm3, xmm0
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 421078 mulps xmm3, [0x93a1f0]
    x0 = shufps(x0, x0, 0xb1);                                     // 42107f shufps xmm0, xmm0, 0xb1
    x4 = x2;                                                       // 421083 movaps xmm4, xmm2
    x4 = shufps(x4, x2, 0x55);                                     // 421086 shufps xmm4, xmm2, 0x55
    x4 = mulps(x4, x0);                                            // 42108a mulps xmm4, xmm0
    x0 = shufps(x0, x0, 0x1b);                                     // 42108d shufps xmm0, xmm0, 0x1b
    x5 = x2;                                                       // 421091 movaps xmm5, xmm2
    x5 = shufps(x5, x2, 0xaa);                                     // 421094 shufps xmm5, xmm2, 0xaa
    ecx = ld32(m, ecx);                                            // 421098 mov ecx, [ecx]
    x0 = mulps(x0, x5);                                            // 42109a mulps xmm0, xmm5
    x0 = mulps(x0, x6);                                            // 42109d mulps xmm0, xmm6
    x4 = mulps(x4, x7);                                            // 4210a0 mulps xmm4, xmm7
    x0 = addps(x0, x4);                                            // 4210a3 addps xmm0, xmm4
    x4 = x2;                                                       // 4210a6 movaps xmm4, xmm2
    x4 = shufps(x4, x2, 0xff);                                     // 4210a9 shufps xmm4, xmm2, 0xff
    x4 = mulps(x4, x1);                                            // 4210ad mulps xmm4, xmm1
    x4 = addps(x4, x3);                                            // 4210b0 addps xmm4, xmm3
    x0 = addps(x0, x4);                                            // 4210b3 addps xmm0, xmm4
    x3 = x0;                                                       // 4210b6 movaps xmm3, xmm0
    x3 = mulps(x3, x0);                                            // 4210b9 mulps xmm3, xmm0
    x1 = x3;                                                       // 4210bc movaps xmm1, xmm3
    x1 = shufps(x1, x3, 0xee);                                     // 4210bf shufps xmm1, xmm3, 0xee
    x2 = x1;                                                       // 4210c3 movaps xmm2, xmm1
    x2 = addps(x2, x3);                                            // 4210c6 addps xmm2, xmm3
    x3 = X::v(0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000);     // 4210c9 movaps xmm3, [0x92dc20]
    x2 = shufps(x2, x2, 0x40);                                     // 4210d0 shufps xmm2, xmm2, 0x40
    x1 = shufps(x1, x2, 0xf0);                                     // 4210d4 shufps xmm1, xmm2, 0xf0
    x1 = addps(x1, x2);                                            // 4210d8 addps xmm1, xmm2
    x1 = shufps(x1, x1, 0xaa);                                     // 4210db shufps xmm1, xmm1, 0xaa
    x2 = sqrtps(x1);                                               // 4210df sqrtps xmm2, xmm1
    x0 = divps(x0, x2);                                            // 4210e2 divps xmm0, xmm2
    x1 = cmpneqps(x1, x3);                                         // 4210e5 cmpneqps xmm1, xmm3
    x3 = X{};                                                      // 4210e9 xorps xmm3, xmm3
    x2 = cmpneqps(x2, x3);                                         // 4210ec cmpneqps xmm2, xmm3
    x0 = andps(x0, x2);                                            // 4210f0 andps xmm0, xmm2
    x2 = X::v(0x7fc00000, 0x7fc00000, 0x7fc00000, 0x7fc00000);     // 4210f3 movaps xmm2, [0x92dc30]
    x0 = andps(x0, x1);                                            // 4210fa andps xmm0, xmm1
    x1 = andnps(x1, x2);                                           // 4210fd andnps xmm1, xmm2
    x0 = orps(x0, x1);                                             // 421100 orps xmm0, xmm1
    ecx = ecx * 0x70u;                                             // 421103 imul ecx, ecx, 0x70
    st(m, ecx + ebx + 0xb00, x0);                                  // 421106 movaps [ecx + ebx + 0xb00], xmm0
L_0042110e:
    edx = edx - 0x4u;                                              // 42110e sub edx, 4
    { uint32_t a_ = esi, b_ = 0x1u, r_ = a_ - b_; fl = f_sub(a_, b_, r_); esi = r_; } // 421111 sub esi, 1
    if (fl.ne()) goto L_00420fc0;                                  // 421114 jne 0x420fc0
L_0042111a:
    fl = f_cmp(ld32(m, ebx + 0x1b20), 0x0u);                       // 42111a cmp [ebx + 0x1b20], 0
    st32(m, esp + 0x1c, 0x0u);                                     // 421121 mov [esp + 0x1c], 0
    if (fl.le()) goto L_00421f85;                                  // 421129 jle 0x421f85
    edx = ebx + 0x37f0;                                            // 42112f lea edx, [ebx + 0x37f0]
    st32(m, esp + 0x18, edx);                                      // 421135 mov [esp + 0x18], edx
L_00421139:
    eax = ld32(m, esp + 0x18);                                     // 421139 mov eax, [esp + 0x18]
    eax = ld32(m, eax);                                            // 42113d mov eax, [eax]
    fl = f_cmp(ld32(m, esp + eax * 4 + 0xa0), 0x0u);               // 42113f cmp [esp + eax*4 + 0xa0], 0
    if (fl.e()) goto L_00421f6b;                                   // 421147 je 0x421f6b
    ecx = eax;                                                     // 42114d mov ecx, eax
    ecx = ecx * 0x170u;                                            // 42114f imul ecx, ecx, 0x170
    edx = ld32(m, ecx + ebx + 0x1b30);                             // 421155 mov edx, [ecx + ebx + 0x1b30]
    ecx = ecx + ebx;                                               // 42115c add ecx, ebx
    edx = edx * 0x70u;                                             // 42115e imul edx, edx, 0x70
    x0 = ld(m, edx + ebx + 0xb00);                                 // 421161 movaps xmm0, [edx + ebx + 0xb00]
    x2 = ld(m, ecx + 0x1be0);                                      // 421169 movaps xmm2, [ecx + 0x1be0]
    edx = eax + 0x13;                                              // 421170 lea edx, [eax + 0x13]
    edx = edx * 0x170u;                                            // 421173 imul edx, edx, 0x170
    esi = ld32(m, edx + ebx);                                      // 421179 mov esi, [edx + ebx]
    edx = edx + ebx;                                               // 42117c add edx, ebx
    edi = esi;                                                     // 42117e mov edi, esi
    edi = edi * 0x70u;                                             // 421180 imul edi, edi, 0x70
    x1 = ld(m, edi + ebx + 0xb00);                                 // 421183 movaps xmm1, [edi + ebx + 0xb00]
    esi = esi + 0x19du;                                            // 42118b add esi, 0x19d
    esi = esi << 4;                                                // 421191 shl esi, 4
    fl = f_cmp(ld32(m, ebx + eax * 4 + 0x38e0), 0x0u);             // 421194 cmp [ebx + eax*4 + 0x38e0], 0
    x3 = ld(m, esi + ebx);                                         // 42119c movaps xmm3, [esi + ebx]
    st(m, esp + 0x140, x3);                                        // 4211a0 movaps [esp + 0x140], xmm3
    if (fl.e()) goto L_004218b5;                                   // 4211a8 je 0x4218b5
    x3 = X::ss(0x80000000u);                                       // 4211ae movss xmm3, [0x9774a8]
    x3 = subss(x3, X::ss(ld32(m, ebx + eax * 4 + 0x3980)));        // 4211b6 subss xmm3, [ebx + eax*4 + 0x3980]
    x4 = X::ss(ld32(m, ebx + eax * 4 + 0x3930));                   // 4211bf movss xmm4, [ebx + eax*4 + 0x3930]
    x4 = shufps(x4, x4, 0x00);                                     // 4211c8 shufps xmm4, xmm4, 0
    x6 = X{};                                                      // 4211cc xorps xmm6, xmm6
    st(m, esp + 0x110, x6);                                        // 4211cf movaps [esp + 0x110], xmm6
    x3 = shufps(x3, x3, 0x00);                                     // 4211d7 shufps xmm3, xmm3, 0
    x3 = shufps(x3, x4, 0xee);                                     // 4211db shufps xmm3, xmm4, 0xee
    x4 = X::ss(0x00000000u);                                       // 4211df movss xmm4, [0x92e12c]
    x4 = shufps(x4, x4, 0x00);                                     // 4211e7 shufps xmm4, xmm4, 0
    x3 = movss(x3, x4);                                            // 4211eb movss xmm3, xmm4
    x4 = x3;                                                       // 4211ef movaps xmm4, xmm3
    x4 = mulps(x4, x3);                                            // 4211f2 mulps xmm4, xmm3
    x5 = x4;                                                       // 4211f5 movaps xmm5, xmm4
    x5 = shufps(x5, x4, 0x99);                                     // 4211f8 shufps xmm5, xmm4, 0x99
    x4 = addss(x4, x5);                                            // 4211fc addss xmm4, xmm5
    x5 = shufps(x5, x5, 0x55);                                     // 421200 shufps xmm5, xmm5, 0x55
    x4 = addss(x4, x5);                                            // 421204 addss xmm4, xmm5
    x4 = shufps(x4, x4, 0x00);                                     // 421208 shufps xmm4, xmm4, 0
    x5 = sqrtps(x4);                                               // 42120c sqrtps xmm5, xmm4
    x4 = cmpneqps(x4, X::v(0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000)); // 42120f cmpneqps xmm4, [0x92dc20]
    x3 = divps(x3, x5);                                            // 421217 divps xmm3, xmm5
    x5 = cmpneqps(x5, x6);                                         // 42121a cmpneqps xmm5, xmm6
    x3 = andps(x3, x5);                                            // 42121e andps xmm3, xmm5
    x3 = andps(x3, x4);                                            // 421221 andps xmm3, xmm4
    x4 = andnps(x4, X::v(0x7fc00000, 0x7fc00000, 0x7fc00000, 0x7fc00000)); // 421224 andnps xmm4, [0x92dc30]
    x3 = orps(x3, x4);                                             // 42122b orps xmm3, xmm4
    x3 = andps(x3, X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000)); // 42122e andps xmm3, [0x92db80]
    x5 = x3;                                                       // 421235 movaps xmm5, xmm3
    x5 = shufps(x5, x3, 0x00);                                     // 421238 shufps xmm5, xmm3, 0
    x6 = x3;                                                       // 42123c movaps xmm6, xmm3
    x6 = shufps(x6, x3, 0x55);                                     // 42123f shufps xmm6, xmm3, 0x55
    x7 = x3;                                                       // 421243 movaps xmm7, xmm3
    x7 = shufps(x7, x3, 0xaa);                                     // 421246 shufps xmm7, xmm3, 0xaa
    x4 = x2;                                                       // 42124a movaps xmm4, xmm2
    x4 = mulps(x4, X::v(0xbf800000, 0xbf800000, 0xbf800000, 0x3f800000)); // 42124d mulps xmm4, [0x93a220]
    st(m, esp + 0x120, x4);                                        // 421254 movaps [esp + 0x120], xmm4
    x4 = shufps(x4, x4, 0x1b);                                     // 42125c shufps xmm4, xmm4, 0x1b
    x5 = mulps(x5, x4);                                            // 421260 mulps xmm5, xmm4
    x5 = mulps(x5, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 421263 mulps xmm5, [0x93a1f0]
    x4 = shufps(x4, x4, 0xb1);                                     // 42126a shufps xmm4, xmm4, 0xb1
    x6 = mulps(x6, x4);                                            // 42126e mulps xmm6, xmm4
    x6 = mulps(x6, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 421271 mulps xmm6, [0x93a200]
    x4 = shufps(x4, x4, 0x1b);                                     // 421278 shufps xmm4, xmm4, 0x1b
    x4 = mulps(x4, x7);                                            // 42127c mulps xmm4, xmm7
    x4 = mulps(x4, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 42127f mulps xmm4, [0x93a210]
    x4 = addps(x4, x6);                                            // 421286 addps xmm4, xmm6
    x6 = x3;                                                       // 421289 movaps xmm6, xmm3
    x6 = shufps(x6, x3, 0xff);                                     // 42128c shufps xmm6, xmm3, 0xff
    x6 = mulps(x6, ld(m, esp + 0x120));                            // 421290 mulps xmm6, [esp + 0x120]
    x6 = addps(x6, x5);                                            // 421298 addps xmm6, xmm5
    x4 = addps(x4, x6);                                            // 42129b addps xmm4, xmm6
    x3 = x4;                                                       // 42129e movaps xmm3, xmm4
    x3 = shufps(x3, x4, 0x1b);                                     // 4212a1 shufps xmm3, xmm4, 0x1b
    x5 = x2;                                                       // 4212a5 movaps xmm5, xmm2
    x5 = shufps(x5, x2, 0x00);                                     // 4212a8 shufps xmm5, xmm2, 0
    x5 = mulps(x5, x3);                                            // 4212ac mulps xmm5, xmm3
    x5 = mulps(x5, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 4212af mulps xmm5, [0x93a1f0]
    x3 = shufps(x3, x3, 0xb1);                                     // 4212b6 shufps xmm3, xmm3, 0xb1
    x6 = x2;                                                       // 4212ba movaps xmm6, xmm2
    x6 = shufps(x6, x2, 0x55);                                     // 4212bd shufps xmm6, xmm2, 0x55
    x6 = mulps(x6, x3);                                            // 4212c1 mulps xmm6, xmm3
    x6 = mulps(x6, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 4212c4 mulps xmm6, [0x93a200]
    x3 = shufps(x3, x3, 0x1b);                                     // 4212cb shufps xmm3, xmm3, 0x1b
    x7 = x2;                                                       // 4212cf movaps xmm7, xmm2
    x7 = shufps(x7, x2, 0xaa);                                     // 4212d2 shufps xmm7, xmm2, 0xaa
    x3 = mulps(x3, x7);                                            // 4212d6 mulps xmm3, xmm7
    x3 = mulps(x3, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 4212d9 mulps xmm3, [0x93a210]
    x3 = addps(x3, x6);                                            // 4212e0 addps xmm3, xmm6
    x6 = x2;                                                       // 4212e3 movaps xmm6, xmm2
    x6 = shufps(x6, x2, 0xff);                                     // 4212e6 shufps xmm6, xmm2, 0xff
    x6 = mulps(x6, x4);                                            // 4212ea mulps xmm6, xmm4
    x6 = addps(x6, x5);                                            // 4212ed addps xmm6, xmm5
    x3 = addps(x3, x6);                                            // 4212f0 addps xmm3, xmm6
    x4 = x3;                                                       // 4212f3 movaps xmm4, xmm3
    x4 = mulps(x4, x3);                                            // 4212f6 mulps xmm4, xmm3
    x5 = x4;                                                       // 4212f9 movaps xmm5, xmm4
    x5 = shufps(x5, x4, 0x99);                                     // 4212fc shufps xmm5, xmm4, 0x99
    x4 = addss(x4, x5);                                            // 421300 addss xmm4, xmm5
    x5 = shufps(x5, x5, 0x55);                                     // 421304 shufps xmm5, xmm5, 0x55
    x4 = addss(x4, x5);                                            // 421308 addss xmm4, xmm5
    x4 = shufps(x4, x4, 0x00);                                     // 42130c shufps xmm4, xmm4, 0
    x5 = sqrtps(x4);                                               // 421310 sqrtps xmm5, xmm4
    x4 = cmpneqps(x4, X::v(0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000)); // 421313 cmpneqps xmm4, [0x92dc20]
    x3 = divps(x3, x5);                                            // 42131b divps xmm3, xmm5
    x5 = cmpneqps(x5, ld(m, esp + 0x110));                         // 42131e cmpneqps xmm5, [esp + 0x110]
    x3 = andps(x3, x5);                                            // 421327 andps xmm3, xmm5
    x3 = andps(x3, x4);                                            // 42132a andps xmm3, xmm4
    x4 = andnps(x4, X::v(0x7fc00000, 0x7fc00000, 0x7fc00000, 0x7fc00000)); // 42132d andnps xmm4, [0x92dc30]
    x3 = orps(x3, x4);                                             // 421334 orps xmm3, xmm4
    x3 = andps(x3, X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000)); // 421337 andps xmm3, [0x92db80]
    x5 = x3;                                                       // 42133e movaps xmm5, xmm3
    x5 = shufps(x5, x3, 0x00);                                     // 421341 shufps xmm5, xmm3, 0
    x6 = x3;                                                       // 421345 movaps xmm6, xmm3
    x6 = shufps(x6, x3, 0x55);                                     // 421348 shufps xmm6, xmm3, 0x55
    x7 = x3;                                                       // 42134c movaps xmm7, xmm3
    x7 = shufps(x7, x3, 0xaa);                                     // 42134f shufps xmm7, xmm3, 0xaa
    x4 = x0;                                                       // 421353 movaps xmm4, xmm0
    x4 = mulps(x4, X::v(0xbf800000, 0xbf800000, 0xbf800000, 0x3f800000)); // 421356 mulps xmm4, [0x93a220]
    st(m, esp + 0x120, x4);                                        // 42135d movaps [esp + 0x120], xmm4
    x4 = shufps(x4, x4, 0x1b);                                     // 421365 shufps xmm4, xmm4, 0x1b
    x5 = mulps(x5, x4);                                            // 421369 mulps xmm5, xmm4
    x5 = mulps(x5, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 42136c mulps xmm5, [0x93a1f0]
    x4 = shufps(x4, x4, 0xb1);                                     // 421373 shufps xmm4, xmm4, 0xb1
    x6 = mulps(x6, x4);                                            // 421377 mulps xmm6, xmm4
    x6 = mulps(x6, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 42137a mulps xmm6, [0x93a200]
    x4 = shufps(x4, x4, 0x1b);                                     // 421381 shufps xmm4, xmm4, 0x1b
    x4 = mulps(x4, x7);                                            // 421385 mulps xmm4, xmm7
    x4 = mulps(x4, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 421388 mulps xmm4, [0x93a210]
    x4 = addps(x4, x6);                                            // 42138f addps xmm4, xmm6
    x6 = x3;                                                       // 421392 movaps xmm6, xmm3
    x6 = shufps(x6, x3, 0xff);                                     // 421395 shufps xmm6, xmm3, 0xff
    x6 = mulps(x6, ld(m, esp + 0x120));                            // 421399 mulps xmm6, [esp + 0x120]
    x6 = addps(x6, x5);                                            // 4213a1 addps xmm6, xmm5
    x4 = addps(x4, x6);                                            // 4213a4 addps xmm4, xmm6
    x3 = x4;                                                       // 4213a7 movaps xmm3, xmm4
    x3 = shufps(x3, x4, 0x1b);                                     // 4213aa shufps xmm3, xmm4, 0x1b
    x6 = x0;                                                       // 4213ae movaps xmm6, xmm0
    x6 = shufps(x6, x0, 0x00);                                     // 4213b1 shufps xmm6, xmm0, 0
    x5 = x6;                                                       // 4213b5 movaps xmm5, xmm6
    x5 = mulps(x5, x3);                                            // 4213b8 mulps xmm5, xmm3
    st(m, esp + 0x130, x5);                                        // 4213bb movaps [esp + 0x130], xmm5
    x3 = shufps(x3, x3, 0xb1);                                     // 4213c3 shufps xmm3, xmm3, 0xb1
    x5 = x0;                                                       // 4213c7 movaps xmm5, xmm0
    x5 = shufps(x5, x0, 0x55);                                     // 4213ca shufps xmm5, xmm0, 0x55
    st(m, esp + 0x120, x5);                                        // 4213ce movaps [esp + 0x120], xmm5
    x5 = mulps(x5, x3);                                            // 4213d6 mulps xmm5, xmm3
    x5 = mulps(x5, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 4213d9 mulps xmm5, [0x93a200]
    x3 = shufps(x3, x3, 0x1b);                                     // 4213e0 shufps xmm3, xmm3, 0x1b
    x7 = x0;                                                       // 4213e4 movaps xmm7, xmm0
    x7 = shufps(x7, x0, 0xaa);                                     // 4213e7 shufps xmm7, xmm0, 0xaa
    st(m, esp + 0x70, x7);                                         // 4213eb movaps [esp + 0x70], xmm7
    x3 = mulps(x3, ld(m, esp + 0x70));                             // 4213f0 mulps xmm3, [esp + 0x70]
    x3 = mulps(x3, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 4213f5 mulps xmm3, [0x93a210]
    x3 = addps(x3, x5);                                            // 4213fc addps xmm3, xmm5
    x7 = x0;                                                       // 4213ff movaps xmm7, xmm0
    x7 = shufps(x7, x0, 0xff);                                     // 421402 shufps xmm7, xmm0, 0xff
    st(m, esp + 0x80, x7);                                         // 421406 movaps [esp + 0x80], xmm7
    x5 = ld(m, esp + 0x80);                                        // 42140e movaps xmm5, [esp + 0x80]
    x5 = mulps(x5, x4);                                            // 421416 mulps xmm5, xmm4
    x4 = ld(m, esp + 0x130);                                       // 421419 movaps xmm4, [esp + 0x130]
    x4 = mulps(x4, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 421421 mulps xmm4, [0x93a1f0]
    x5 = addps(x5, x4);                                            // 421428 addps xmm5, xmm4
    x3 = addps(x3, x5);                                            // 42142b addps xmm3, xmm5
    x4 = x3;                                                       // 42142e movaps xmm4, xmm3
    x4 = mulps(x4, x3);                                            // 421431 mulps xmm4, xmm3
    x5 = x4;                                                       // 421434 movaps xmm5, xmm4
    x5 = shufps(x5, x4, 0x99);                                     // 421437 shufps xmm5, xmm4, 0x99
    x4 = addss(x4, x5);                                            // 42143b addss xmm4, xmm5
    x5 = shufps(x5, x5, 0x55);                                     // 42143f shufps xmm5, xmm5, 0x55
    x4 = addss(x4, x5);                                            // 421443 addss xmm4, xmm5
    x5 = ld(m, esp + 0x110);                                       // 421447 movaps xmm5, [esp + 0x110]
    x4 = shufps(x4, x4, 0x00);                                     // 42144f shufps xmm4, xmm4, 0
    x7 = sqrtps(x4);                                               // 421453 sqrtps xmm7, xmm4
    x4 = cmpneqps(x4, X::v(0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000)); // 421456 cmpneqps xmm4, [0x92dc20]
    x3 = divps(x3, x7);                                            // 42145e divps xmm3, xmm7
    x5 = cmpneqps(x5, x7);                                         // 421461 cmpneqps xmm5, xmm7
    x3 = andps(x3, x5);                                            // 421465 andps xmm3, xmm5
    x3 = andps(x3, x4);                                            // 421468 andps xmm3, xmm4
    x4 = andnps(x4, X::v(0x7fc00000, 0x7fc00000, 0x7fc00000, 0x7fc00000)); // 42146b andnps xmm4, [0x92dc30]
    x3 = orps(x3, x4);                                             // 421472 orps xmm3, xmm4
    st(m, esp + 0x180, x3);                                        // 421475 movaps [esp + 0x180], xmm3
    esi = ld32(m, ecx + 0x1b30);                                   // 42147d mov esi, [ecx + 0x1b30]
    esi = esi * 0x70u;                                             // 421483 imul esi, esi, 0x70
    x3 = ld(m, esi + ebx + 0xb10);                                 // 421486 movaps xmm3, [esi + ebx + 0xb10]
    x3 = andps(x3, X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000)); // 42148e andps xmm3, [0x92db80]
    esi = ld32(m, edx);                                            // 421495 mov esi, [edx]
    x5 = x3;                                                       // 421497 movaps xmm5, xmm3
    esi = esi * 0x70u;                                             // 42149a imul esi, esi, 0x70
    x5 = shufps(x5, x3, 0x00);                                     // 42149d shufps xmm5, xmm3, 0
    x7 = x3;                                                       // 4214a1 movaps xmm7, xmm3
    x7 = shufps(x7, x3, 0xaa);                                     // 4214a4 shufps xmm7, xmm3, 0xaa
    x4 = x0;                                                       // 4214a8 movaps xmm4, xmm0
    x4 = mulps(x4, X::v(0xbf800000, 0xbf800000, 0xbf800000, 0x3f800000)); // 4214ab mulps xmm4, [0x93a220]
    st(m, esp + 0x130, x4);                                        // 4214b2 movaps [esp + 0x130], xmm4
    x4 = shufps(x4, x4, 0x1b);                                     // 4214ba shufps xmm4, xmm4, 0x1b
    x5 = mulps(x5, x4);                                            // 4214be mulps xmm5, xmm4
    st(m, esp + 0x110, x5);                                        // 4214c1 movaps [esp + 0x110], xmm5
    x5 = x3;                                                       // 4214c9 movaps xmm5, xmm3
    x5 = shufps(x5, x3, 0x55);                                     // 4214cc shufps xmm5, xmm3, 0x55
    x4 = shufps(x4, x4, 0xb1);                                     // 4214d0 shufps xmm4, xmm4, 0xb1
    x5 = mulps(x5, x4);                                            // 4214d4 mulps xmm5, xmm4
    x5 = mulps(x5, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 4214d7 mulps xmm5, [0x93a200]
    x4 = shufps(x4, x4, 0x1b);                                     // 4214de shufps xmm4, xmm4, 0x1b
    x4 = mulps(x4, x7);                                            // 4214e2 mulps xmm4, xmm7
    x4 = mulps(x4, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 4214e5 mulps xmm4, [0x93a210]
    x4 = addps(x4, x5);                                            // 4214ec addps xmm4, xmm5
    x5 = x3;                                                       // 4214ef movaps xmm5, xmm3
    x5 = shufps(x5, x3, 0xff);                                     // 4214f2 shufps xmm5, xmm3, 0xff
    x5 = mulps(x5, ld(m, esp + 0x130));                            // 4214f6 mulps xmm5, [esp + 0x130]
    x3 = ld(m, esp + 0x110);                                       // 4214fe movaps xmm3, [esp + 0x110]
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 421506 mulps xmm3, [0x93a1f0]
    x5 = addps(x5, x3);                                            // 42150d addps xmm5, xmm3
    x3 = ld(m, esp + 0x120);                                       // 421510 movaps xmm3, [esp + 0x120]
    x4 = addps(x4, x5);                                            // 421518 addps xmm4, xmm5
    st(m, esp + 0x100, x4);                                        // 42151b movaps [esp + 0x100], xmm4
    x4 = shufps(x4, x4, 0x1b);                                     // 421523 shufps xmm4, xmm4, 0x1b
    x6 = mulps(x6, x4);                                            // 421527 mulps xmm6, xmm4
    st(m, esp + 0xf0, x6);                                         // 42152a movaps [esp + 0xf0], xmm6
    x4 = shufps(x4, x4, 0xb1);                                     // 421532 shufps xmm4, xmm4, 0xb1
    x3 = mulps(x3, x4);                                            // 421536 mulps xmm3, xmm4
    st(m, esp + 0x150, x3);                                        // 421539 movaps [esp + 0x150], xmm3
    x3 = ld(m, esi + ebx + 0xb10);                                 // 421541 movaps xmm3, [esi + ebx + 0xb10]
    st(m, esp + 0x120, x3);                                        // 421549 movaps [esp + 0x120], xmm3
    x3 = andps(x3, X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000)); // 421551 andps xmm3, [0x92db80]
    esi = esi + ebx + 0xb10;                                       // 421558 lea esi, [esi + ebx + 0xb10]
    x6 = x3;                                                       // 42155f movaps xmm6, xmm3
    x6 = shufps(x6, x3, 0x00);                                     // 421562 shufps xmm6, xmm3, 0
    st(m, esp + 0x110, x4);                                        // 421566 movaps [esp + 0x110], xmm4
    x4 = x1;                                                       // 42156e movaps xmm4, xmm1
    x4 = mulps(x4, X::v(0xbf800000, 0xbf800000, 0xbf800000, 0x3f800000)); // 421571 mulps xmm4, [0x93a220]
    x7 = x3;                                                       // 421578 movaps xmm7, xmm3
    x7 = shufps(x7, x3, 0x55);                                     // 42157b shufps xmm7, xmm3, 0x55
    x5 = x4;                                                       // 42157f movaps xmm5, xmm4
    x5 = shufps(x5, x4, 0x1b);                                     // 421582 shufps xmm5, xmm4, 0x1b
    x6 = mulps(x6, x5);                                            // 421586 mulps xmm6, xmm5
    x5 = shufps(x5, x5, 0xb1);                                     // 421589 shufps xmm5, xmm5, 0xb1
    x7 = mulps(x7, x5);                                            // 42158d mulps xmm7, xmm5
    x7 = mulps(x7, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 421590 mulps xmm7, [0x93a200]
    st(m, esp + 0x130, x6);                                        // 421597 movaps [esp + 0x130], xmm6
    x6 = x5;                                                       // 42159f movaps xmm6, xmm5
    x6 = shufps(x6, x5, 0x1b);                                     // 4215a2 shufps xmm6, xmm5, 0x1b
    x5 = x3;                                                       // 4215a6 movaps xmm5, xmm3
    x5 = shufps(x5, x3, 0xaa);                                     // 4215a9 shufps xmm5, xmm3, 0xaa
    x6 = mulps(x6, x5);                                            // 4215ad mulps xmm6, xmm5
    x6 = mulps(x6, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 4215b0 mulps xmm6, [0x93a210]
    x5 = x3;                                                       // 4215b7 movaps xmm5, xmm3
    x5 = shufps(x5, x3, 0xff);                                     // 4215ba shufps xmm5, xmm3, 0xff
    x3 = ld(m, esp + 0x130);                                       // 4215be movaps xmm3, [esp + 0x130]
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 4215c6 mulps xmm3, [0x93a1f0]
    x5 = mulps(x5, x4);                                            // 4215cd mulps xmm5, xmm4
    x5 = addps(x5, x3);                                            // 4215d0 addps xmm5, xmm3
    x6 = addps(x6, x7);                                            // 4215d3 addps xmm6, xmm7
    x6 = addps(x6, x5);                                            // 4215d6 addps xmm6, xmm5
    x3 = x6;                                                       // 4215d9 movaps xmm3, xmm6
    x3 = shufps(x3, x6, 0x1b);                                     // 4215dc shufps xmm3, xmm6, 0x1b
    x5 = x1;                                                       // 4215e0 movaps xmm5, xmm1
    x5 = shufps(x5, x1, 0x00);                                     // 4215e3 shufps xmm5, xmm1, 0
    x5 = mulps(x5, x3);                                            // 4215e7 mulps xmm5, xmm3
    x7 = x1;                                                       // 4215ea movaps xmm7, xmm1
    st(m, esp + 0x130, x5);                                        // 4215ed movaps [esp + 0x130], xmm5
    x3 = shufps(x3, x3, 0xb1);                                     // 4215f5 shufps xmm3, xmm3, 0xb1
    x7 = shufps(x7, x1, 0x55);                                     // 4215f9 shufps xmm7, xmm1, 0x55
    x7 = mulps(x7, x3);                                            // 4215fd mulps xmm7, xmm3
    x7 = mulps(x7, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 421600 mulps xmm7, [0x93a200]
    x5 = x3;                                                       // 421607 movaps xmm5, xmm3
    x5 = shufps(x5, x3, 0x1b);                                     // 42160a shufps xmm5, xmm3, 0x1b
    x3 = x1;                                                       // 42160e movaps xmm3, xmm1
    x3 = shufps(x3, x1, 0xaa);                                     // 421611 shufps xmm3, xmm1, 0xaa
    x5 = mulps(x5, x3);                                            // 421615 mulps xmm5, xmm3
    x5 = mulps(x5, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 421618 mulps xmm5, [0x93a210]
    x3 = ld(m, esp + 0x130);                                       // 42161f movaps xmm3, [esp + 0x130]
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 421627 mulps xmm3, [0x93a1f0]
    x5 = addps(x5, x7);                                            // 42162e addps xmm5, xmm7
    x7 = x1;                                                       // 421631 movaps xmm7, xmm1
    x7 = shufps(x7, x1, 0xff);                                     // 421634 shufps xmm7, xmm1, 0xff
    x7 = mulps(x7, x6);                                            // 421638 mulps xmm7, xmm6
    x7 = addps(x7, x3);                                            // 42163b addps xmm7, xmm3
    x3 = ld(m, esp + 0x110);                                       // 42163e movaps xmm3, [esp + 0x110]
    x5 = addps(x5, x7);                                            // 421646 addps xmm5, xmm7
    x7 = x3;                                                       // 421649 movaps xmm7, xmm3
    x7 = shufps(x7, x3, 0x1b);                                     // 42164c shufps xmm7, xmm3, 0x1b
    x7 = mulps(x7, ld(m, esp + 0x70));                             // 421650 mulps xmm7, [esp + 0x70]
    x7 = mulps(x7, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 421655 mulps xmm7, [0x93a210]
    x3 = ld(m, esp + 0x150);                                       // 42165c movaps xmm3, [esp + 0x150]
    x3 = mulps(x3, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 421664 mulps xmm3, [0x93a200]
    x7 = addps(x7, x3);                                            // 42166b addps xmm7, xmm3
    x3 = ld(m, esp + 0x80);                                        // 42166e movaps xmm3, [esp + 0x80]
    x3 = mulps(x3, ld(m, esp + 0x100));                            // 421676 mulps xmm3, [esp + 0x100]
    st(m, esp + 0x100, x3);                                        // 42167e movaps [esp + 0x100], xmm3
    x3 = ld(m, esp + 0xf0);                                        // 421686 movaps xmm3, [esp + 0xf0]
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 42168e mulps xmm3, [0x93a1f0]
    x6 = ld(m, esp + 0x100);                                       // 421695 movaps xmm6, [esp + 0x100]
    x6 = addps(x6, x3);                                            // 42169d addps xmm6, xmm3
    x7 = addps(x7, x6);                                            // 4216a0 addps xmm7, xmm6
    x6 = ld(m, esp + 0x180);                                       // 4216a3 movaps xmm6, [esp + 0x180]
    x5 = subps(x5, x7);                                            // 4216ab subps xmm5, xmm7
    x5 = mulps(x5, x6);                                            // 4216ae mulps xmm5, xmm6
    x3 = x5;                                                       // 4216b1 movaps xmm3, xmm5
    x3 = shufps(x3, x5, 0x99);                                     // 4216b4 shufps xmm3, xmm5, 0x99
    x5 = addss(x5, x3);                                            // 4216b8 addss xmm5, xmm3
    x3 = shufps(x3, x3, 0x55);                                     // 4216bc shufps xmm3, xmm3, 0x55
    x5 = addss(x5, x3);                                            // 4216c0 addss xmm5, xmm3
    x3 = x5;                                                       // 4216c4 movaps xmm3, xmm5
    x3 = shufps(x3, x3, 0x00);                                     // 4216c7 shufps xmm3, xmm3, 0
    x3 = mulps(x3, ld(m, esp + 0x90));                             // 4216cb mulps xmm3, [esp + 0x90]
    x3 = mulps(x3, x6);                                            // 4216d3 mulps xmm3, xmm6
    st(m, esp + 0x170, x3);                                        // 4216d6 movaps [esp + 0x170], xmm3
    x3 = andps(x3, X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000)); // 4216de andps xmm3, [0x92db80]
    x6 = x3;                                                       // 4216e5 movaps xmm6, xmm3
    x6 = shufps(x6, x3, 0x00);                                     // 4216e8 shufps xmm6, xmm3, 0
    x5 = x1;                                                       // 4216ec movaps xmm5, xmm1
    x5 = shufps(x5, x1, 0x1b);                                     // 4216ef shufps xmm5, xmm1, 0x1b
    x6 = mulps(x6, x5);                                            // 4216f3 mulps xmm6, xmm5
    st(m, esp + 0xf0, x6);                                         // 4216f6 movaps [esp + 0xf0], xmm6
    x6 = x3;                                                       // 4216fe movaps xmm6, xmm3
    x6 = shufps(x6, x3, 0x55);                                     // 421701 shufps xmm6, xmm3, 0x55
    x5 = shufps(x5, x5, 0xb1);                                     // 421705 shufps xmm5, xmm5, 0xb1
    x6 = mulps(x6, x5);                                            // 421709 mulps xmm6, xmm5
    x6 = mulps(x6, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 42170c mulps xmm6, [0x93a200]
    x7 = x3;                                                       // 421713 movaps xmm7, xmm3
    x7 = shufps(x7, x3, 0xaa);                                     // 421716 shufps xmm7, xmm3, 0xaa
    x5 = shufps(x5, x5, 0x1b);                                     // 42171a shufps xmm5, xmm5, 0x1b
    x5 = mulps(x5, x7);                                            // 42171e mulps xmm5, xmm7
    x5 = mulps(x5, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 421721 mulps xmm5, [0x93a210]
    x5 = addps(x5, x6);                                            // 421728 addps xmm5, xmm6
    x6 = x3;                                                       // 42172b movaps xmm6, xmm3
    x6 = shufps(x6, x3, 0xff);                                     // 42172e shufps xmm6, xmm3, 0xff
    x3 = ld(m, esp + 0xf0);                                        // 421732 movaps xmm3, [esp + 0xf0]
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 42173a mulps xmm3, [0x93a1f0]
    x6 = mulps(x6, x1);                                            // 421741 mulps xmm6, xmm1
    x6 = addps(x6, x3);                                            // 421744 addps xmm6, xmm3
    x5 = addps(x5, x6);                                            // 421747 addps xmm5, xmm6
    x3 = x5;                                                       // 42174a movaps xmm3, xmm5
    x3 = shufps(x3, x5, 0x1b);                                     // 42174d shufps xmm3, xmm5, 0x1b
    x6 = x4;                                                       // 421751 movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0x00);                                     // 421754 shufps xmm6, xmm4, 0
    x6 = mulps(x6, x3);                                            // 421758 mulps xmm6, xmm3
    st(m, esp + 0xf0, x6);                                         // 42175b movaps [esp + 0xf0], xmm6
    x3 = shufps(x3, x3, 0xb1);                                     // 421763 shufps xmm3, xmm3, 0xb1
    x6 = x4;                                                       // 421767 movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0x55);                                     // 42176a shufps xmm6, xmm4, 0x55
    x6 = mulps(x6, x3);                                            // 42176e mulps xmm6, xmm3
    x7 = x3;                                                       // 421771 movaps xmm7, xmm3
    x6 = mulps(x6, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 421774 mulps xmm6, [0x93a200]
    x7 = shufps(x7, x3, 0x1b);                                     // 42177b shufps xmm7, xmm3, 0x1b
    x3 = x4;                                                       // 42177f movaps xmm3, xmm4
    x3 = shufps(x3, x4, 0xaa);                                     // 421782 shufps xmm3, xmm4, 0xaa
    x7 = mulps(x7, x3);                                            // 421786 mulps xmm7, xmm3
    x7 = mulps(x7, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 421789 mulps xmm7, [0x93a210]
    x3 = ld(m, esp + 0xf0);                                        // 421790 movaps xmm3, [esp + 0xf0]
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 421798 mulps xmm3, [0x93a1f0]
    x7 = addps(x7, x6);                                            // 42179f addps xmm7, xmm6
    x6 = x4;                                                       // 4217a2 movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0xff);                                     // 4217a5 shufps xmm6, xmm4, 0xff
    x6 = mulps(x6, x5);                                            // 4217a9 mulps xmm6, xmm5
    x6 = addps(x6, x3);                                            // 4217ac addps xmm6, xmm3
    x3 = ld(m, esp + 0x120);                                       // 4217af movaps xmm3, [esp + 0x120]
    x7 = addps(x7, x6);                                            // 4217b7 addps xmm7, xmm6
    x3 = subps(x3, x7);                                            // 4217ba subps xmm3, xmm7
    st(m, esi, x3);                                                // 4217bd movaps [esi], xmm3
    x4 = X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000);     // 4217c0 movaps xmm4, [0x92db80]
    x3 = ld(m, esp + 0x170);                                       // 4217c7 movaps xmm3, [esp + 0x170]
    x3 = andps(x3, x4);                                            // 4217cf andps xmm3, xmm4
    esi = ld32(m, ecx + 0x1b30);                                   // 4217d2 mov esi, [ecx + 0x1b30]
    x5 = x3;                                                       // 4217d8 movaps xmm5, xmm3
    esi = esi * 0x70u;                                             // 4217db imul esi, esi, 0x70
    x5 = shufps(x5, x3, 0x00);                                     // 4217de shufps xmm5, xmm3, 0
    x6 = x3;                                                       // 4217e2 movaps xmm6, xmm3
    x6 = shufps(x6, x3, 0x55);                                     // 4217e5 shufps xmm6, xmm3, 0x55
    x7 = x3;                                                       // 4217e9 movaps xmm7, xmm3
    x7 = shufps(x7, x3, 0xaa);                                     // 4217ec shufps xmm7, xmm3, 0xaa
    x4 = x0;                                                       // 4217f0 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x1b);                                     // 4217f3 shufps xmm4, xmm0, 0x1b
    x5 = mulps(x5, x4);                                            // 4217f7 mulps xmm5, xmm4
    x5 = mulps(x5, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 4217fa mulps xmm5, [0x93a1f0]
    x4 = shufps(x4, x4, 0xb1);                                     // 421801 shufps xmm4, xmm4, 0xb1
    x6 = mulps(x6, x4);                                            // 421805 mulps xmm6, xmm4
    x6 = mulps(x6, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 421808 mulps xmm6, [0x93a200]
    x4 = shufps(x4, x4, 0x1b);                                     // 42180f shufps xmm4, xmm4, 0x1b
    x4 = mulps(x4, x7);                                            // 421813 mulps xmm4, xmm7
    x4 = mulps(x4, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 421816 mulps xmm4, [0x93a210]
    x4 = addps(x4, x6);                                            // 42181d addps xmm4, xmm6
    x6 = x3;                                                       // 421820 movaps xmm6, xmm3
    x6 = shufps(x6, x3, 0xff);                                     // 421823 shufps xmm6, xmm3, 0xff
    x6 = mulps(x6, x0);                                            // 421827 mulps xmm6, xmm0
    x6 = addps(x6, x5);                                            // 42182a addps xmm6, xmm5
    x5 = X::v(0xbf800000, 0xbf800000, 0xbf800000, 0x3f800000);     // 42182d movaps xmm5, [0x93a220]
    x4 = addps(x4, x6);                                            // 421834 addps xmm4, xmm6
    x3 = x0;                                                       // 421837 movaps xmm3, xmm0
    x3 = mulps(x3, x5);                                            // 42183a mulps xmm3, xmm5
    x6 = x3;                                                       // 42183d movaps xmm6, xmm3
    x6 = shufps(x6, x3, 0x00);                                     // 421840 shufps xmm6, xmm3, 0
    x5 = x4;                                                       // 421844 movaps xmm5, xmm4
    x5 = shufps(x5, x4, 0x1b);                                     // 421847 shufps xmm5, xmm4, 0x1b
    x6 = mulps(x6, x5);                                            // 42184b mulps xmm6, xmm5
    x5 = shufps(x5, x5, 0xb1);                                     // 42184e shufps xmm5, xmm5, 0xb1
    st(m, esp + 0xf0, x6);                                         // 421852 movaps [esp + 0xf0], xmm6
    x7 = x5;                                                       // 42185a movaps xmm7, xmm5
    x7 = shufps(x7, x5, 0x1b);                                     // 42185d shufps xmm7, xmm5, 0x1b
    x6 = x3;                                                       // 421861 movaps xmm6, xmm3
    x6 = shufps(x6, x3, 0x55);                                     // 421864 shufps xmm6, xmm3, 0x55
    x6 = mulps(x6, x5);                                            // 421868 mulps xmm6, xmm5
    x6 = mulps(x6, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 42186b mulps xmm6, [0x93a200]
    x5 = x3;                                                       // 421872 movaps xmm5, xmm3
    x5 = shufps(x5, x3, 0xaa);                                     // 421875 shufps xmm5, xmm3, 0xaa
    x7 = mulps(x7, x5);                                            // 421879 mulps xmm7, xmm5
    x7 = mulps(x7, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 42187c mulps xmm7, [0x93a210]
    x5 = x3;                                                       // 421883 movaps xmm5, xmm3
    x5 = shufps(x5, x3, 0xff);                                     // 421886 shufps xmm5, xmm3, 0xff
    x3 = ld(m, esp + 0xf0);                                        // 42188a movaps xmm3, [esp + 0xf0]
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 421892 mulps xmm3, [0x93a1f0]
    esi = esi + ebx + 0xb10;                                       // 421899 lea esi, [esi + ebx + 0xb10]
    x7 = addps(x7, x6);                                            // 4218a0 addps xmm7, xmm6
    x5 = mulps(x5, x4);                                            // 4218a3 mulps xmm5, xmm4
    x5 = addps(x5, x3);                                            // 4218a6 addps xmm5, xmm3
    x3 = ld(m, esi);                                               // 4218a9 movaps xmm3, [esi]
    x7 = addps(x7, x5);                                            // 4218ac addps xmm7, xmm5
    x7 = addps(x7, x3);                                            // 4218af addps xmm7, xmm3
    st(m, esi, x7);                                                // 4218b2 movaps [esi], xmm7
L_004218b5:
    fl = f_cmp(ld32(m, ebx + eax * 4 + 0x3840), 0x0u);             // 4218b5 cmp [ebx + eax*4 + 0x3840], 0
    if (fl.ne()) goto L_004218cd;                                  // 4218bd jne 0x4218cd
    fl = f_cmp(ld32(m, ebx + eax * 4 + 0x3890), 0x0u);             // 4218bf cmp [ebx + eax*4 + 0x3890], 0
    if (fl.e()) goto L_00421f6b;                                   // 4218c7 je 0x421f6b
L_004218cd:
    x3 = ld(m, 0xd615c0);                                          // 4218cd movaps xmm3, [0xd615c0]
    x3 = andps(x3, X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000)); // 4218d4 andps xmm3, [0x92db80]
    x4 = ld(m, esp + 0x140);                                       // 4218db movaps xmm4, [esp + 0x140]
    x4 = mulps(x4, X::v(0xbf800000, 0xbf800000, 0xbf800000, 0x3f800000)); // 4218e3 mulps xmm4, [0x93a220]
    x5 = x3;                                                       // 4218ea movaps xmm5, xmm3
    x5 = shufps(x5, x3, 0x00);                                     // 4218ed shufps xmm5, xmm3, 0
    st(m, esp + 0xf0, x4);                                         // 4218f1 movaps [esp + 0xf0], xmm4
    x6 = X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000);     // 4218f9 movaps xmm6, [0x93a210]
    x4 = shufps(x4, x4, 0x1b);                                     // 421900 shufps xmm4, xmm4, 0x1b
    x5 = mulps(x5, x4);                                            // 421904 mulps xmm5, xmm4
    x7 = x3;                                                       // 421907 movaps xmm7, xmm3
    x7 = shufps(x7, x3, 0x55);                                     // 42190a shufps xmm7, xmm3, 0x55
    x4 = shufps(x4, x4, 0xb1);                                     // 42190e shufps xmm4, xmm4, 0xb1
    x7 = mulps(x7, x4);                                            // 421912 mulps xmm7, xmm4
    x7 = mulps(x7, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 421915 mulps xmm7, [0x93a200]
    st(m, esp + 0x100, x5);                                        // 42191c movaps [esp + 0x100], xmm5
    x5 = x4;                                                       // 421924 movaps xmm5, xmm4
    x5 = shufps(x5, x4, 0x1b);                                     // 421927 shufps xmm5, xmm4, 0x1b
    x4 = x3;                                                       // 42192b movaps xmm4, xmm3
    x4 = shufps(x4, x3, 0xaa);                                     // 42192e shufps xmm4, xmm3, 0xaa
    x5 = mulps(x5, x4);                                            // 421932 mulps xmm5, xmm4
    x4 = x3;                                                       // 421935 movaps xmm4, xmm3
    x4 = shufps(x4, x3, 0xff);                                     // 421938 shufps xmm4, xmm3, 0xff
    x4 = mulps(x4, ld(m, esp + 0xf0));                             // 42193c mulps xmm4, [esp + 0xf0]
    x3 = ld(m, esp + 0x100);                                       // 421944 movaps xmm3, [esp + 0x100]
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 42194c mulps xmm3, [0x93a1f0]
    x4 = addps(x4, x3);                                            // 421953 addps xmm4, xmm3
    x5 = mulps(x5, x6);                                            // 421956 mulps xmm5, xmm6
    x5 = addps(x5, x7);                                            // 421959 addps xmm5, xmm7
    x7 = ld(m, esp + 0x140);                                       // 42195c movaps xmm7, [esp + 0x140]
    x5 = addps(x5, x4);                                            // 421964 addps xmm5, xmm4
    x3 = x5;                                                       // 421967 movaps xmm3, xmm5
    x3 = shufps(x3, x5, 0x1b);                                     // 42196a shufps xmm3, xmm5, 0x1b
    x4 = x7;                                                       // 42196e movaps xmm4, xmm7
    x4 = shufps(x4, x7, 0x00);                                     // 421971 shufps xmm4, xmm7, 0
    x4 = mulps(x4, x3);                                            // 421975 mulps xmm4, xmm3
    x3 = shufps(x3, x3, 0xb1);                                     // 421978 shufps xmm3, xmm3, 0xb1
    st(m, esp + 0x100, x4);                                        // 42197c movaps [esp + 0x100], xmm4
    x4 = x7;                                                       // 421984 movaps xmm4, xmm7
    x4 = shufps(x4, x7, 0x55);                                     // 421987 shufps xmm4, xmm7, 0x55
    x4 = mulps(x4, x3);                                            // 42198b mulps xmm4, xmm3
    st(m, esp + 0xf0, x4);                                         // 42198e movaps [esp + 0xf0], xmm4
    x4 = x3;                                                       // 421996 movaps xmm4, xmm3
    x4 = shufps(x4, x3, 0x1b);                                     // 421999 shufps xmm4, xmm3, 0x1b
    x3 = x7;                                                       // 42199d movaps xmm3, xmm7
    x3 = shufps(x3, x7, 0xaa);                                     // 4219a0 shufps xmm3, xmm7, 0xaa
    x4 = mulps(x4, x3);                                            // 4219a4 mulps xmm4, xmm3
    x3 = ld(m, esp + 0xf0);                                        // 4219a7 movaps xmm3, [esp + 0xf0]
    x3 = mulps(x3, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 4219af mulps xmm3, [0x93a200]
    x4 = mulps(x4, x6);                                            // 4219b6 mulps xmm4, xmm6
    x4 = addps(x4, x3);                                            // 4219b9 addps xmm4, xmm3
    x3 = ld(m, esp + 0x140);                                       // 4219bc movaps xmm3, [esp + 0x140]
    x7 = x3;                                                       // 4219c4 movaps xmm7, xmm3
    x7 = shufps(x7, x3, 0xff);                                     // 4219c7 shufps xmm7, xmm3, 0xff
    x3 = ld(m, esp + 0x100);                                       // 4219cb movaps xmm3, [esp + 0x100]
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 4219d3 mulps xmm3, [0x93a1f0]
    x7 = mulps(x7, x5);                                            // 4219da mulps xmm7, xmm5
    x7 = addps(x7, x3);                                            // 4219dd addps xmm7, xmm3
    x4 = addps(x4, x7);                                            // 4219e0 addps xmm4, xmm7
    x3 = x4;                                                       // 4219e3 movaps xmm3, xmm4
    x3 = mulps(x3, x4);                                            // 4219e6 mulps xmm3, xmm4
    x5 = x3;                                                       // 4219e9 movaps xmm5, xmm3
    x5 = shufps(x5, x3, 0x99);                                     // 4219ec shufps xmm5, xmm3, 0x99
    x3 = addss(x3, x5);                                            // 4219f0 addss xmm3, xmm5
    x5 = shufps(x5, x5, 0x55);                                     // 4219f4 shufps xmm5, xmm5, 0x55
    x3 = addss(x3, x5);                                            // 4219f8 addss xmm3, xmm5
    x3 = shufps(x3, x3, 0x00);                                     // 4219fc shufps xmm3, xmm3, 0
    x7 = sqrtps(x3);                                               // 421a00 sqrtps xmm7, xmm3
    x3 = cmpneqps(x3, X::v(0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000)); // 421a03 cmpneqps xmm3, [0x92dc20]
    x5 = x3;                                                       // 421a0b movaps xmm5, xmm3
    x3 = X{};                                                      // 421a0e xorps xmm3, xmm3
    st(m, esp + 0x110, x3);                                        // 421a11 movaps [esp + 0x110], xmm3
    x3 = cmpneqps(x3, x7);                                         // 421a19 cmpneqps xmm3, xmm7
    x4 = divps(x4, x7);                                            // 421a1d divps xmm4, xmm7
    x3 = andps(x3, x4);                                            // 421a20 andps xmm3, xmm4
    x3 = andps(x3, x5);                                            // 421a23 andps xmm3, xmm5
    x5 = andnps(x5, X::v(0x7fc00000, 0x7fc00000, 0x7fc00000, 0x7fc00000)); // 421a26 andnps xmm5, [0x92dc30]
    x3 = orps(x3, x5);                                             // 421a2d orps xmm3, xmm5
    x3 = andps(x3, X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000)); // 421a30 andps xmm3, [0x92db80]
    x4 = x2;                                                       // 421a37 movaps xmm4, xmm2
    x4 = mulps(x4, X::v(0xbf800000, 0xbf800000, 0xbf800000, 0x3f800000)); // 421a3a mulps xmm4, [0x93a220]
    st(m, esp + 0xf0, x4);                                         // 421a41 movaps [esp + 0xf0], xmm4
    x5 = x3;                                                       // 421a49 movaps xmm5, xmm3
    x5 = shufps(x5, x3, 0x00);                                     // 421a4c shufps xmm5, xmm3, 0
    x4 = shufps(x4, x4, 0x1b);                                     // 421a50 shufps xmm4, xmm4, 0x1b
    x5 = mulps(x5, x4);                                            // 421a54 mulps xmm5, xmm4
    st(m, esp + 0x100, x5);                                        // 421a57 movaps [esp + 0x100], xmm5
    x5 = x3;                                                       // 421a5f movaps xmm5, xmm3
    x5 = shufps(x5, x3, 0x55);                                     // 421a62 shufps xmm5, xmm3, 0x55
    x7 = x3;                                                       // 421a66 movaps xmm7, xmm3
    x7 = shufps(x7, x3, 0xaa);                                     // 421a69 shufps xmm7, xmm3, 0xaa
    x4 = shufps(x4, x4, 0xb1);                                     // 421a6d shufps xmm4, xmm4, 0xb1
    x5 = mulps(x5, x4);                                            // 421a71 mulps xmm5, xmm4
    x5 = mulps(x5, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 421a74 mulps xmm5, [0x93a200]
    x4 = shufps(x4, x4, 0x1b);                                     // 421a7b shufps xmm4, xmm4, 0x1b
    x4 = mulps(x4, x7);                                            // 421a7f mulps xmm4, xmm7
    x4 = mulps(x4, x6);                                            // 421a82 mulps xmm4, xmm6
    x4 = addps(x4, x5);                                            // 421a85 addps xmm4, xmm5
    x5 = x3;                                                       // 421a88 movaps xmm5, xmm3
    x5 = shufps(x5, x3, 0xff);                                     // 421a8b shufps xmm5, xmm3, 0xff
    x5 = mulps(x5, ld(m, esp + 0xf0));                             // 421a8f mulps xmm5, [esp + 0xf0]
    x3 = ld(m, esp + 0x100);                                       // 421a97 movaps xmm3, [esp + 0x100]
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 421a9f mulps xmm3, [0x93a1f0]
    x5 = addps(x5, x3);                                            // 421aa6 addps xmm5, xmm3
    x4 = addps(x4, x5);                                            // 421aa9 addps xmm4, xmm5
    x5 = x2;                                                       // 421aac movaps xmm5, xmm2
    x5 = shufps(x5, x2, 0x00);                                     // 421aaf shufps xmm5, xmm2, 0
    x7 = x2;                                                       // 421ab3 movaps xmm7, xmm2
    x7 = shufps(x7, x2, 0xaa);                                     // 421ab6 shufps xmm7, xmm2, 0xaa
    x3 = x4;                                                       // 421aba movaps xmm3, xmm4
    x3 = shufps(x3, x4, 0x1b);                                     // 421abd shufps xmm3, xmm4, 0x1b
    x5 = mulps(x5, x3);                                            // 421ac1 mulps xmm5, xmm3
    st(m, esp + 0xf0, x5);                                         // 421ac4 movaps [esp + 0xf0], xmm5
    x5 = x2;                                                       // 421acc movaps xmm5, xmm2
    x5 = shufps(x5, x2, 0x55);                                     // 421acf shufps xmm5, xmm2, 0x55
    x3 = shufps(x3, x3, 0xb1);                                     // 421ad3 shufps xmm3, xmm3, 0xb1
    x5 = mulps(x5, x3);                                            // 421ad7 mulps xmm5, xmm3
    x5 = mulps(x5, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 421ada mulps xmm5, [0x93a200]
    x3 = shufps(x3, x3, 0x1b);                                     // 421ae1 shufps xmm3, xmm3, 0x1b
    x3 = mulps(x3, x7);                                            // 421ae5 mulps xmm3, xmm7
    x3 = mulps(x3, x6);                                            // 421ae8 mulps xmm3, xmm6
    x3 = addps(x3, x5);                                            // 421aeb addps xmm3, xmm5
    x5 = x2;                                                       // 421aee movaps xmm5, xmm2
    x5 = shufps(x5, x2, 0xff);                                     // 421af1 shufps xmm5, xmm2, 0xff
    x2 = ld(m, esp + 0xf0);                                        // 421af5 movaps xmm2, [esp + 0xf0]
    x2 = mulps(x2, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 421afd mulps xmm2, [0x93a1f0]
    x5 = mulps(x5, x4);                                            // 421b04 mulps xmm5, xmm4
    x5 = addps(x5, x2);                                            // 421b07 addps xmm5, xmm2
    x3 = addps(x3, x5);                                            // 421b0a addps xmm3, xmm5
    x2 = x3;                                                       // 421b0d movaps xmm2, xmm3
    x2 = mulps(x2, x3);                                            // 421b10 mulps xmm2, xmm3
    x4 = x2;                                                       // 421b13 movaps xmm4, xmm2
    x4 = shufps(x4, x2, 0x99);                                     // 421b16 shufps xmm4, xmm2, 0x99
    x2 = addss(x2, x4);                                            // 421b1a addss xmm2, xmm4
    x4 = shufps(x4, x4, 0x55);                                     // 421b1e shufps xmm4, xmm4, 0x55
    x2 = addss(x2, x4);                                            // 421b22 addss xmm2, xmm4
    x2 = shufps(x2, x2, 0x00);                                     // 421b26 shufps xmm2, xmm2, 0
    x5 = sqrtps(x2);                                               // 421b2a sqrtps xmm5, xmm2
    x2 = cmpneqps(x2, X::v(0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000)); // 421b2d cmpneqps xmm2, [0x92dc20]
    x3 = divps(x3, x5);                                            // 421b35 divps xmm3, xmm5
    x4 = x2;                                                       // 421b38 movaps xmm4, xmm2
    x2 = ld(m, esp + 0x110);                                       // 421b3b movaps xmm2, [esp + 0x110]
    x2 = cmpneqps(x2, x5);                                         // 421b43 cmpneqps xmm2, xmm5
    x2 = andps(x2, x3);                                            // 421b47 andps xmm2, xmm3
    x2 = andps(x2, x4);                                            // 421b4a andps xmm2, xmm4
    x4 = andnps(x4, X::v(0x7fc00000, 0x7fc00000, 0x7fc00000, 0x7fc00000)); // 421b4d andnps xmm4, [0x92dc30]
    x2 = orps(x2, x4);                                             // 421b54 orps xmm2, xmm4
    x2 = andps(x2, X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000)); // 421b57 andps xmm2, [0x92db80]
    x4 = x0;                                                       // 421b5e movaps xmm4, xmm0
    x4 = mulps(x4, X::v(0xbf800000, 0xbf800000, 0xbf800000, 0x3f800000)); // 421b61 mulps xmm4, [0x93a220]
    x3 = x4;                                                       // 421b68 movaps xmm3, xmm4
    x3 = shufps(x3, x4, 0x1b);                                     // 421b6b shufps xmm3, xmm4, 0x1b
    x5 = x2;                                                       // 421b6f movaps xmm5, xmm2
    x5 = shufps(x5, x2, 0x00);                                     // 421b72 shufps xmm5, xmm2, 0
    x5 = mulps(x5, x3);                                            // 421b76 mulps xmm5, xmm3
    st(m, esp + 0xf0, x5);                                         // 421b79 movaps [esp + 0xf0], xmm5
    x3 = shufps(x3, x3, 0xb1);                                     // 421b81 shufps xmm3, xmm3, 0xb1
    x5 = x2;                                                       // 421b85 movaps xmm5, xmm2
    x5 = shufps(x5, x2, 0x55);                                     // 421b88 shufps xmm5, xmm2, 0x55
    x5 = mulps(x5, x3);                                            // 421b8c mulps xmm5, xmm3
    x3 = shufps(x3, x3, 0x1b);                                     // 421b8f shufps xmm3, xmm3, 0x1b
    x7 = x2;                                                       // 421b93 movaps xmm7, xmm2
    x5 = mulps(x5, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 421b96 mulps xmm5, [0x93a200]
    x7 = shufps(x7, x2, 0xaa);                                     // 421b9d shufps xmm7, xmm2, 0xaa
    x3 = mulps(x3, x7);                                            // 421ba1 mulps xmm3, xmm7
    x3 = mulps(x3, x6);                                            // 421ba4 mulps xmm3, xmm6
    x3 = addps(x3, x5);                                            // 421ba7 addps xmm3, xmm5
    x5 = x2;                                                       // 421baa movaps xmm5, xmm2
    x5 = shufps(x5, x2, 0xff);                                     // 421bad shufps xmm5, xmm2, 0xff
    x2 = ld(m, esp + 0xf0);                                        // 421bb1 movaps xmm2, [esp + 0xf0]
    x2 = mulps(x2, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 421bb9 mulps xmm2, [0x93a1f0]
    x5 = mulps(x5, x4);                                            // 421bc0 mulps xmm5, xmm4
    x5 = addps(x5, x2);                                            // 421bc3 addps xmm5, xmm2
    x3 = addps(x3, x5);                                            // 421bc6 addps xmm3, xmm5
    x2 = x3;                                                       // 421bc9 movaps xmm2, xmm3
    x2 = shufps(x2, x3, 0x1b);                                     // 421bcc shufps xmm2, xmm3, 0x1b
    x5 = x2;                                                       // 421bd0 movaps xmm5, xmm2
    x2 = shufps(x2, x2, 0xb1);                                     // 421bd3 shufps xmm2, xmm2, 0xb1
    x7 = x0;                                                       // 421bd7 movaps xmm7, xmm0
    eax = ld32(m, ecx + 0x1b30);                                   // 421bda mov eax, [ecx + 0x1b30]
    x7 = shufps(x7, x0, 0x00);                                     // 421be0 shufps xmm7, xmm0, 0
    eax = eax * 0x70u;                                             // 421be4 imul eax, eax, 0x70
    x5 = mulps(x5, x7);                                            // 421be7 mulps xmm5, xmm7
    st(m, esp + 0x100, x5);                                        // 421bea movaps [esp + 0x100], xmm5
    x5 = x2;                                                       // 421bf2 movaps xmm5, xmm2
    x2 = x0;                                                       // 421bf5 movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0x55);                                     // 421bf8 shufps xmm2, xmm0, 0x55
    st(m, esp + 0x120, x2);                                        // 421bfc movaps [esp + 0x120], xmm2
    st(m, esp + 0xf0, x5);                                         // 421c04 movaps [esp + 0xf0], xmm5
    x5 = mulps(x5, x2);                                            // 421c0c mulps xmm5, xmm2
    x5 = mulps(x5, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 421c0f mulps xmm5, [0x93a200]
    x2 = x0;                                                       // 421c16 movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0xaa);                                     // 421c19 shufps xmm2, xmm0, 0xaa
    st(m, esp + 0x70, x2);                                         // 421c1d movaps [esp + 0x70], xmm2
    x2 = ld(m, esp + 0xf0);                                        // 421c22 movaps xmm2, [esp + 0xf0]
    x0 = shufps(x0, x0, 0xff);                                     // 421c2a shufps xmm0, xmm0, 0xff
    st(m, esp + 0x80, x0);                                         // 421c2e movaps [esp + 0x80], xmm0
    x3 = mulps(x3, ld(m, esp + 0x80));                             // 421c36 mulps xmm3, [esp + 0x80]
    x0 = ld(m, esp + 0x100);                                       // 421c3e movaps xmm0, [esp + 0x100]
    x0 = mulps(x0, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 421c46 mulps xmm0, [0x93a1f0]
    x0 = addps(x0, x3);                                            // 421c4d addps xmm0, xmm3
    x2 = shufps(x2, x2, 0x1b);                                     // 421c50 shufps xmm2, xmm2, 0x1b
    x2 = mulps(x2, ld(m, esp + 0x70));                             // 421c54 mulps xmm2, [esp + 0x70]
    x2 = mulps(x2, x6);                                            // 421c59 mulps xmm2, xmm6
    x2 = addps(x2, x5);                                            // 421c5c addps xmm2, xmm5
    x2 = addps(x2, x0);                                            // 421c5f addps xmm2, xmm0
    x0 = x2;                                                       // 421c62 movaps xmm0, xmm2
    x0 = mulps(x0, x2);                                            // 421c65 mulps xmm0, xmm2
    x3 = x0;                                                       // 421c68 movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0x99);                                     // 421c6b shufps xmm3, xmm0, 0x99
    x0 = addss(x0, x3);                                            // 421c6f addss xmm0, xmm3
    x3 = shufps(x3, x3, 0x55);                                     // 421c73 shufps xmm3, xmm3, 0x55
    x0 = addss(x0, x3);                                            // 421c77 addss xmm0, xmm3
    x0 = shufps(x0, x0, 0x00);                                     // 421c7b shufps xmm0, xmm0, 0
    x3 = sqrtps(x0);                                               // 421c7f sqrtps xmm3, xmm0
    x0 = cmpneqps(x0, X::v(0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000)); // 421c82 cmpneqps xmm0, [0x92dc20]
    x2 = divps(x2, x3);                                            // 421c8a divps xmm2, xmm3
    x3 = cmpneqps(x3, ld(m, esp + 0x110));                         // 421c8d cmpneqps xmm3, [esp + 0x110]
    x2 = andps(x2, x3);                                            // 421c96 andps xmm2, xmm3
    x2 = andps(x2, x0);                                            // 421c99 andps xmm2, xmm0
    x0 = andnps(x0, X::v(0x7fc00000, 0x7fc00000, 0x7fc00000, 0x7fc00000)); // 421c9c andnps xmm0, [0x92dc30]
    x2 = orps(x2, x0);                                             // 421ca3 orps xmm2, xmm0
    x0 = ld(m, eax + ebx + 0xb10);                                 // 421ca6 movaps xmm0, [eax + ebx + 0xb10]
    x0 = andps(x0, X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000)); // 421cae andps xmm0, [0x92db80]
    st(m, esp + 0x160, x2);                                        // 421cb5 movaps [esp + 0x160], xmm2
    x2 = x4;                                                       // 421cbd movaps xmm2, xmm4
    x2 = shufps(x2, x4, 0x1b);                                     // 421cc0 shufps xmm2, xmm4, 0x1b
    x3 = x0;                                                       // 421cc4 movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0x00);                                     // 421cc7 shufps xmm3, xmm0, 0
    x3 = mulps(x3, x2);                                            // 421ccb mulps xmm3, xmm2
    st(m, esp + 0xf0, x3);                                         // 421cce movaps [esp + 0xf0], xmm3
    x2 = shufps(x2, x2, 0xb1);                                     // 421cd6 shufps xmm2, xmm2, 0xb1
    x3 = x0;                                                       // 421cda movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0x55);                                     // 421cdd shufps xmm3, xmm0, 0x55
    x3 = mulps(x3, x2);                                            // 421ce1 mulps xmm3, xmm2
    x3 = mulps(x3, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 421ce4 mulps xmm3, [0x93a200]
    x5 = x0;                                                       // 421ceb movaps xmm5, xmm0
    x5 = shufps(x5, x0, 0xaa);                                     // 421cee shufps xmm5, xmm0, 0xaa
    x2 = shufps(x2, x2, 0x1b);                                     // 421cf2 shufps xmm2, xmm2, 0x1b
    x5 = mulps(x5, x2);                                            // 421cf6 mulps xmm5, xmm2
    x5 = mulps(x5, x6);                                            // 421cf9 mulps xmm5, xmm6
    x5 = addps(x5, x3);                                            // 421cfc addps xmm5, xmm3
    x2 = x0;                                                       // 421cff movaps xmm2, xmm0
    ecx = ld32(m, edx);                                            // 421d02 mov ecx, [edx]
    x2 = shufps(x2, x0, 0xff);                                     // 421d04 shufps xmm2, xmm0, 0xff
    ecx = ecx * 0x70u;                                             // 421d08 imul ecx, ecx, 0x70
    x2 = mulps(x2, x4);                                            // 421d0b mulps xmm2, xmm4
    x0 = ld(m, esp + 0xf0);                                        // 421d0e movaps xmm0, [esp + 0xf0]
    x0 = mulps(x0, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 421d16 mulps xmm0, [0x93a1f0]
    x2 = addps(x2, x0);                                            // 421d1d addps xmm2, xmm0
    x5 = addps(x5, x2);                                            // 421d20 addps xmm5, xmm2
    x2 = ld(m, esp + 0x120);                                       // 421d23 movaps xmm2, [esp + 0x120]
    x0 = x5;                                                       // 421d2b movaps xmm0, xmm5
    x0 = shufps(x0, x5, 0x1b);                                     // 421d2e shufps xmm0, xmm5, 0x1b
    x7 = mulps(x7, x0);                                            // 421d32 mulps xmm7, xmm0
    x0 = shufps(x0, x0, 0xb1);                                     // 421d35 shufps xmm0, xmm0, 0xb1
    x2 = mulps(x2, x0);                                            // 421d39 mulps xmm2, xmm0
    st(m, esp + 0x100, x0);                                        // 421d3c movaps [esp + 0x100], xmm0
    x0 = ld(m, ecx + ebx + 0xb10);                                 // 421d44 movaps xmm0, [ecx + ebx + 0xb10]
    st(m, esp + 0x150, x2);                                        // 421d4c movaps [esp + 0x150], xmm2
    st(m, esp + 0x120, x0);                                        // 421d54 movaps [esp + 0x120], xmm0
    x0 = andps(x0, X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000)); // 421d5c andps xmm0, [0x92db80]
    x4 = x0;                                                       // 421d63 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x00);                                     // 421d66 shufps xmm4, xmm0, 0
    eax = ecx + ebx + 0xb10;                                       // 421d6a lea eax, [ecx + ebx + 0xb10]
    st(m, esp + 0x130, x7);                                        // 421d71 movaps [esp + 0x130], xmm7
    x7 = x0;                                                       // 421d79 movaps xmm7, xmm0
    x7 = shufps(x7, x0, 0xaa);                                     // 421d7c shufps xmm7, xmm0, 0xaa
    x3 = x1;                                                       // 421d80 movaps xmm3, xmm1
    x3 = mulps(x3, X::v(0xbf800000, 0xbf800000, 0xbf800000, 0x3f800000)); // 421d83 mulps xmm3, [0x93a220]
    x2 = x3;                                                       // 421d8a movaps xmm2, xmm3
    x2 = shufps(x2, x3, 0x1b);                                     // 421d8d shufps xmm2, xmm3, 0x1b
    x4 = mulps(x4, x2);                                            // 421d91 mulps xmm4, xmm2
    st(m, esp + 0xf0, x4);                                         // 421d94 movaps [esp + 0xf0], xmm4
    x4 = x0;                                                       // 421d9c movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x55);                                     // 421d9f shufps xmm4, xmm0, 0x55
    x2 = shufps(x2, x2, 0xb1);                                     // 421da3 shufps xmm2, xmm2, 0xb1
    x4 = mulps(x4, x2);                                            // 421da7 mulps xmm4, xmm2
    x4 = mulps(x4, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 421daa mulps xmm4, [0x93a200]
    x2 = shufps(x2, x2, 0x1b);                                     // 421db1 shufps xmm2, xmm2, 0x1b
    x2 = mulps(x2, x7);                                            // 421db5 mulps xmm2, xmm7
    x2 = mulps(x2, x6);                                            // 421db8 mulps xmm2, xmm6
    x2 = addps(x2, x4);                                            // 421dbb addps xmm2, xmm4
    x4 = x0;                                                       // 421dbe movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0xff);                                     // 421dc1 shufps xmm4, xmm0, 0xff
    x0 = ld(m, esp + 0xf0);                                        // 421dc5 movaps xmm0, [esp + 0xf0]
    x0 = mulps(x0, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 421dcd mulps xmm0, [0x93a1f0]
    x4 = mulps(x4, x3);                                            // 421dd4 mulps xmm4, xmm3
    x4 = addps(x4, x0);                                            // 421dd7 addps xmm4, xmm0
    x2 = addps(x2, x4);                                            // 421dda addps xmm2, xmm4
    x5 = mulps(x5, ld(m, esp + 0x80));                             // 421ddd mulps xmm5, [esp + 0x80]
    x0 = x2;                                                       // 421de5 movaps xmm0, xmm2
    x0 = shufps(x0, x2, 0x1b);                                     // 421de8 shufps xmm0, xmm2, 0x1b
    x4 = x1;                                                       // 421dec movaps xmm4, xmm1
    x4 = shufps(x4, x1, 0x00);                                     // 421def shufps xmm4, xmm1, 0
    x4 = mulps(x4, x0);                                            // 421df3 mulps xmm4, xmm0
    x0 = shufps(x0, x0, 0xb1);                                     // 421df6 shufps xmm0, xmm0, 0xb1
    st(m, esp + 0xf0, x4);                                         // 421dfa movaps [esp + 0xf0], xmm4
    x4 = x0;                                                       // 421e02 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x1b);                                     // 421e05 shufps xmm4, xmm0, 0x1b
    x7 = x1;                                                       // 421e09 movaps xmm7, xmm1
    x7 = shufps(x7, x1, 0x55);                                     // 421e0c shufps xmm7, xmm1, 0x55
    x7 = mulps(x7, x0);                                            // 421e10 mulps xmm7, xmm0
    x7 = mulps(x7, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 421e13 mulps xmm7, [0x93a200]
    x0 = x1;                                                       // 421e1a movaps xmm0, xmm1
    x0 = shufps(x0, x1, 0xaa);                                     // 421e1d shufps xmm0, xmm1, 0xaa
    x4 = mulps(x4, x0);                                            // 421e21 mulps xmm4, xmm0
    x0 = ld(m, esp + 0xf0);                                        // 421e24 movaps xmm0, [esp + 0xf0]
    x0 = mulps(x0, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 421e2c mulps xmm0, [0x93a1f0]
    x4 = mulps(x4, x6);                                            // 421e33 mulps xmm4, xmm6
    x4 = addps(x4, x7);                                            // 421e36 addps xmm4, xmm7
    x7 = x1;                                                       // 421e39 movaps xmm7, xmm1
    x7 = shufps(x7, x1, 0xff);                                     // 421e3c shufps xmm7, xmm1, 0xff
    x7 = mulps(x7, x2);                                            // 421e40 mulps xmm7, xmm2
    x7 = addps(x7, x0);                                            // 421e43 addps xmm7, xmm0
    x0 = ld(m, esp + 0x100);                                       // 421e46 movaps xmm0, [esp + 0x100]
    x2 = x0;                                                       // 421e4e movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0x1b);                                     // 421e51 shufps xmm2, xmm0, 0x1b
    x2 = mulps(x2, ld(m, esp + 0x70));                             // 421e55 mulps xmm2, [esp + 0x70]
    x0 = X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000);     // 421e5a movaps xmm0, [0x93a200]
    x0 = mulps(x0, ld(m, esp + 0x150));                            // 421e61 mulps xmm0, [esp + 0x150]
    x2 = mulps(x2, x6);                                            // 421e69 mulps xmm2, xmm6
    x4 = addps(x4, x7);                                            // 421e6c addps xmm4, xmm7
    x2 = addps(x2, x0);                                            // 421e6f addps xmm2, xmm0
    x0 = X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000);     // 421e72 movaps xmm0, [0x93a1f0]
    x0 = mulps(x0, ld(m, esp + 0x130));                            // 421e79 mulps xmm0, [esp + 0x130]
    x5 = addps(x5, x0);                                            // 421e81 addps xmm5, xmm0
    x2 = addps(x2, x5);                                            // 421e84 addps xmm2, xmm5
    x4 = subps(x4, x2);                                            // 421e87 subps xmm4, xmm2
    x2 = ld(m, esp + 0x160);                                       // 421e8a movaps xmm2, [esp + 0x160]
    x4 = mulps(x4, x2);                                            // 421e92 mulps xmm4, xmm2
    x0 = x4;                                                       // 421e95 movaps xmm0, xmm4
    x0 = shufps(x0, x4, 0x99);                                     // 421e98 shufps xmm0, xmm4, 0x99
    x4 = addss(x4, x0);                                            // 421e9c addss xmm4, xmm0
    x0 = shufps(x0, x0, 0x55);                                     // 421ea0 shufps xmm0, xmm0, 0x55
    x4 = addss(x4, x0);                                            // 421ea4 addss xmm4, xmm0
    x0 = x4;                                                       // 421ea8 movaps xmm0, xmm4
    x0 = shufps(x0, x0, 0x00);                                     // 421eab shufps xmm0, xmm0, 0
    x0 = mulps(x0, x2);                                            // 421eaf mulps xmm0, xmm2
    x0 = andps(x0, X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000)); // 421eb2 andps xmm0, [0x92db80]
    x4 = x0;                                                       // 421eb9 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x00);                                     // 421ebc shufps xmm4, xmm0, 0
    x5 = x0;                                                       // 421ec0 movaps xmm5, xmm0
    x5 = shufps(x5, x0, 0x55);                                     // 421ec3 shufps xmm5, xmm0, 0x55
    x7 = x0;                                                       // 421ec7 movaps xmm7, xmm0
    x7 = shufps(x7, x0, 0xaa);                                     // 421eca shufps xmm7, xmm0, 0xaa
    x2 = x1;                                                       // 421ece movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0x1b);                                     // 421ed1 shufps xmm2, xmm1, 0x1b
    x4 = mulps(x4, x2);                                            // 421ed5 mulps xmm4, xmm2
    x4 = mulps(x4, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 421ed8 mulps xmm4, [0x93a1f0]
    x2 = shufps(x2, x2, 0xb1);                                     // 421edf shufps xmm2, xmm2, 0xb1
    x5 = mulps(x5, x2);                                            // 421ee3 mulps xmm5, xmm2
    x2 = shufps(x2, x2, 0x1b);                                     // 421ee6 shufps xmm2, xmm2, 0x1b
    x2 = mulps(x2, x7);                                            // 421eea mulps xmm2, xmm7
    x7 = X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000);     // 421eed movaps xmm7, [0x93a200]
    x5 = mulps(x5, x7);                                            // 421ef4 mulps xmm5, xmm7
    x2 = mulps(x2, x6);                                            // 421ef7 mulps xmm2, xmm6
    x2 = addps(x2, x5);                                            // 421efa addps xmm2, xmm5
    x5 = x0;                                                       // 421efd movaps xmm5, xmm0
    x5 = shufps(x5, x0, 0xff);                                     // 421f00 shufps xmm5, xmm0, 0xff
    x5 = mulps(x5, x1);                                            // 421f04 mulps xmm5, xmm1
    x5 = addps(x5, x4);                                            // 421f07 addps xmm5, xmm4
    x2 = addps(x2, x5);                                            // 421f0a addps xmm2, xmm5
    x0 = x2;                                                       // 421f0d movaps xmm0, xmm2
    x0 = shufps(x0, x2, 0x1b);                                     // 421f10 shufps xmm0, xmm2, 0x1b
    x1 = x3;                                                       // 421f14 movaps xmm1, xmm3
    x1 = shufps(x1, x3, 0x00);                                     // 421f17 shufps xmm1, xmm3, 0
    x1 = mulps(x1, x0);                                            // 421f1b mulps xmm1, xmm0
    x1 = mulps(x1, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 421f1e mulps xmm1, [0x93a1f0]
    x0 = shufps(x0, x0, 0xb1);                                     // 421f25 shufps xmm0, xmm0, 0xb1
    x5 = x0;                                                       // 421f29 movaps xmm5, xmm0
    x5 = shufps(x5, x0, 0x1b);                                     // 421f2c shufps xmm5, xmm0, 0x1b
    x4 = x3;                                                       // 421f30 movaps xmm4, xmm3
    x4 = shufps(x4, x3, 0x55);                                     // 421f33 shufps xmm4, xmm3, 0x55
    x4 = mulps(x4, x0);                                            // 421f37 mulps xmm4, xmm0
    x0 = x3;                                                       // 421f3a movaps xmm0, xmm3
    x0 = shufps(x0, x3, 0xaa);                                     // 421f3d shufps xmm0, xmm3, 0xaa
    x5 = mulps(x5, x0);                                            // 421f41 mulps xmm5, xmm0
    x0 = x3;                                                       // 421f44 movaps xmm0, xmm3
    x0 = shufps(x0, x3, 0xff);                                     // 421f47 shufps xmm0, xmm3, 0xff
    x0 = mulps(x0, x2);                                            // 421f4b mulps xmm0, xmm2
    x5 = mulps(x5, x6);                                            // 421f4e mulps xmm5, xmm6
    x0 = addps(x0, x1);                                            // 421f51 addps xmm0, xmm1
    x4 = mulps(x4, x7);                                            // 421f54 mulps xmm4, xmm7
    x5 = addps(x5, x4);                                            // 421f57 addps xmm5, xmm4
    x5 = addps(x5, x0);                                            // 421f5a addps xmm5, xmm0
    x0 = ld(m, esp + 0x120);                                       // 421f5d movaps xmm0, [esp + 0x120]
    x0 = subps(x0, x5);                                            // 421f65 subps xmm0, xmm5
    st(m, eax, x0);                                                // 421f68 movaps [eax], xmm0
L_00421f6b:
    eax = ld32(m, esp + 0x1c);                                     // 421f6b mov eax, [esp + 0x1c]
    st32(m, esp + 0x18, ld32(m, esp + 0x18) + 0x4u);               // 421f6f add [esp + 0x18], 4
    eax = eax + 1u;                                                // 421f74 inc eax
    fl = f_cmp(eax, ld32(m, ebx + 0x1b20));                        // 421f75 cmp eax, [ebx + 0x1b20]
    st32(m, esp + 0x1c, eax);                                      // 421f7b mov [esp + 0x1c], eax
    if (fl.l()) goto L_00421139;                                   // 421f7f jl 0x421139
L_00421f85:
    ecx = ld32(m, esp + 0x19c);                                    // 421f85 mov ecx, [esp + 0x19c]
    edi = k.pop();                                                 // 421f8c pop edi
    esi = k.pop();                                                 // 421f8d pop esi
    ebx = k.pop();                                                 // 421f8e pop ebx
    ecx = ecx ^ esp;                                               // 421f8f xor ecx, esp
    k.call(0x008f9efe);  // __security_check_cookie                // 421f91 call 0x8f9efe
    esp = ebp;                                                     // 421f96 mov esp, ebp
    ebp = k.pop();                                                 // 421f98 pop ebp
    return k.ret(4);                                               // 421f99 ret 4
}

// 00421fa0 FUN_00421fa0: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00421fa0, Fn_00421fa0, "math", 0) {  // outputs: what callers read (it calls other groups' code)
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 421fa0 push ebp
    ebp = esp;                                                     // 421fa1 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 421fa3 and esp, 0xfffffff0
    esp = esp - 0x184u;                                            // 421fa6 sub esp, 0x184
    eax = ld32(m, 0x9b45e4);                                       // 421fac mov eax, [0x9b45e4]
    eax = eax ^ esp;                                               // 421fb1 xor eax, esp
    st32(m, esp + 0x180, eax);                                     // 421fb3 mov [esp + 0x180], eax
    k.push(ebx);                                                   // 421fba push ebx
    ebx = ld32(m, ebp + 0x8);                                      // 421fbb mov ebx, [ebp + 8]
    fl = f_cmp(ld32(m, ebx + 0x1b20), 0x0u);                       // 421fbe cmp [ebx + 0x1b20], 0
    k.push(esi);                                                   // 421fc5 push esi
    k.push(edi);                                                   // 421fc6 push edi
    st32(m, esp + 0xc, 0x0u);                                      // 421fc7 mov [esp + 0xc], 0
    if (fl.le()) goto L_00422169;                                  // 421fcf jle 0x422169
    x0 = X{};                                                      // 421fd5 xorps xmm0, xmm0
    st(m, esp + 0x10, x0);                                         // 421fd8 movaps [esp + 0x10], xmm0
    edi = ebx + 0x1b90;                                            // 421fdd lea edi, [ebx + 0x1b90]
L_00421fe3:
    eax = ld32(m, edi - 0x60);                                     // 421fe3 mov eax, [edi - 0x60]
    eax = eax * 0x70u;                                             // 421fe6 imul eax, eax, 0x70
    ecx = eax + ebx + 0xb00;                                       // 421fe9 lea ecx, [eax + ebx + 0xb00]
    k.push(ecx);                                                   // 421ff0 push ecx
    ecx = esp + 0xc4;                                              // 421ff1 lea ecx, [esp + 0xc4]
    k.call(0x0041f160);  // FUN_0041f160                           // 421ff8 call 0x41f160
    edx = ld32(m, edi - 0x40);                                     // 421ffd mov edx, [edi - 0x40]
    edx = edx * 0x70u;                                             // 422000 imul edx, edx, 0x70
    eax = edx + ebx + 0xb00;                                       // 422003 lea eax, [edx + ebx + 0xb00]
    k.push(eax);                                                   // 42200a push eax
    ecx = esp + 0x94;                                              // 42200b lea ecx, [esp + 0x94]
    k.call(0x0041f160);  // FUN_0041f160                           // 422012 call 0x41f160
    edx = esp + 0x120;                                             // 422017 lea edx, [esp + 0x120]
    ecx = edi - 0x50;                                              // 42201e lea ecx, [edi - 0x50]
    k.push(edx);                                                   // 422021 push edx
    k.call(0x0041f390);  // FUN_0041f390                           // 422022 call 0x41f390
    eax = esp + 0x150;                                             // 422027 lea eax, [esp + 0x150]
    ecx = edi - 0x30;                                              // 42202e lea ecx, [edi - 0x30]
    k.push(eax);                                                   // 422031 push eax
    k.call(0x0041f390);  // FUN_0041f390                           // 422032 call 0x41f390
    edx = edi - 0x20;                                              // 422037 lea edx, [edi - 0x20]
    ecx = esp + 0x120;                                             // 42203a lea ecx, [esp + 0x120]
    esi = esp + 0xc0;                                              // 422041 lea esi, [esp + 0xc0]
    k.call(0x0041f430);  // FUN_0041f430                           // 422048 call 0x41f430
    edx = edi + 0x10;                                              // 42204d lea edx, [edi + 0x10]
    ecx = esp + 0x150;                                             // 422050 lea ecx, [esp + 0x150]
    esi = esp + 0x90;                                              // 422057 lea esi, [esp + 0x90]
    k.call(0x0041f430);  // FUN_0041f430                           // 42205e call 0x41f430
    x1 = ld(m, edi - 0x20);                                        // 422063 movaps xmm1, [edi - 0x20]
    x0 = ld(m, esp + 0x10);                                        // 422067 movaps xmm0, [esp + 0x10]
    x2 = x0;                                                       // 42206c movaps xmm2, xmm0
    x2 = subps(x2, x1);                                            // 42206f subps xmm2, xmm1
    x1 = ld(m, edi - 0x10);                                        // 422072 movaps xmm1, [edi - 0x10]
    st(m, edi - 0x20, x2);                                         // 422076 movaps [edi - 0x20], xmm2
    x2 = x0;                                                       // 42207a movaps xmm2, xmm0
    x2 = subps(x2, x1);                                            // 42207d subps xmm2, xmm1
    st(m, edi - 0x10, x2);                                         // 422080 movaps [edi - 0x10], xmm2
    x1 = ld(m, edi);                                               // 422084 movaps xmm1, [edi]
    x0 = subps(x0, x1);                                            // 422087 subps xmm0, xmm1
    st(m, edi, x0);                                                // 42208a movaps [edi], xmm0
    ecx = ld32(m, edi - 0x60);                                     // 42208d mov ecx, [edi - 0x60]
    ecx = ecx * 0x70u;                                             // 422090 imul ecx, ecx, 0x70
    edx = esp + 0xf0;                                              // 422093 lea edx, [esp + 0xf0]
    ecx = ecx + ebx + 0xb10;                                       // 42209a lea ecx, [ecx + ebx + 0xb10]
    k.push(edx);                                                   // 4220a1 push edx
    k.call(0x0041f390);  // FUN_0041f390                           // 4220a2 call 0x41f390
    eax = ld32(m, edi - 0x40);                                     // 4220a7 mov eax, [edi - 0x40]
    eax = eax * 0x70u;                                             // 4220aa imul eax, eax, 0x70
    edx = esp + 0x60;                                              // 4220ad lea edx, [esp + 0x60]
    ecx = eax + ebx + 0xb10;                                       // 4220b1 lea ecx, [eax + ebx + 0xb10]
    k.push(edx);                                                   // 4220b8 push edx
    k.call(0x0041f390);  // FUN_0041f390                           // 4220b9 call 0x41f390
    eax = esp + 0xf0;                                              // 4220be lea eax, [esp + 0xf0]
    k.push(eax);                                                   // 4220c5 push eax
    edx = edi - 0x50;                                              // 4220c6 lea edx, [edi - 0x50]
    ecx = esp + 0x24;                                              // 4220c9 lea ecx, [esp + 0x24]
    k.call(0x0041f5c0);  // FUN_0041f5c0                           // 4220cd call 0x41f5c0
    ecx = esp + 0xf4;                                              // 4220d2 lea ecx, [esp + 0xf4]
    esp = esp + 0x4u;                                              // 4220d9 add esp, 4
    k.push(ecx);                                                   // 4220dc push ecx
    edx = esp + 0x24;                                              // 4220dd lea edx, [esp + 0x24]
    ecx = esp + 0x34;                                              // 4220e1 lea ecx, [esp + 0x34]
    k.call(0x0041f5c0);  // FUN_0041f5c0                           // 4220e5 call 0x41f5c0
    edx = esp + 0xc4;                                              // 4220ea lea edx, [esp + 0xc4]
    esp = esp + 0x4u;                                              // 4220f1 add esp, 4
    k.push(edx);                                                   // 4220f4 push edx
    edx = ecx;                                                     // 4220f5 mov edx, ecx
    ecx = esp + 0x54;                                              // 4220f7 lea ecx, [esp + 0x54]
    k.call(0x0041f5c0);  // FUN_0041f5c0                           // 4220fb call 0x41f5c0
    eax = esp + 0x64;                                              // 422100 lea eax, [esp + 0x64]
    esp = esp + 0x4u;                                              // 422104 add esp, 4
    k.push(eax);                                                   // 422107 push eax
    edx = edi - 0x30;                                              // 422108 lea edx, [edi - 0x30]
    ecx = esp + 0x24;                                              // 42210b lea ecx, [esp + 0x24]
    k.call(0x0041f5c0);  // FUN_0041f5c0                           // 42210f call 0x41f5c0
    esp = esp + 0x4u;                                              // 422114 add esp, 4
    ecx = esp + 0x60;                                              // 422117 lea ecx, [esp + 0x60]
    k.push(ecx);                                                   // 42211b push ecx
    edx = esp + 0x24;                                              // 42211c lea edx, [esp + 0x24]
    ecx = esp + 0x34;                                              // 422120 lea ecx, [esp + 0x34]
    k.call(0x0041f5c0);  // FUN_0041f5c0                           // 422124 call 0x41f5c0
    edx = esi;                                                     // 422129 mov edx, esi
    esp = esp + 0x4u;                                              // 42212b add esp, 4
    k.push(edx);                                                   // 42212e push edx
    edx = ecx;                                                     // 42212f mov edx, ecx
    ecx = esp + 0x44;                                              // 422131 lea ecx, [esp + 0x44]
    k.call(0x0041f5c0);  // FUN_0041f5c0                           // 422135 call 0x41f5c0
    eax = ld32(m, esp + 0x10);                                     // 42213a mov eax, [esp + 0x10]
    x0 = ld(m, esp + 0x54);                                        // 42213e movaps xmm0, [esp + 0x54]
    x1 = ld(m, esp + 0x44);                                        // 422143 movaps xmm1, [esp + 0x44]
    x0 = subps(x0, x1);                                            // 422148 subps xmm0, xmm1
    st(m, edi + 0x40, x0);                                         // 42214b movaps [edi + 0x40], xmm0
    eax = eax + 1u;                                                // 42214f inc eax
    esp = esp + 0x4u;                                              // 422150 add esp, 4
    edi = edi + 0x170u;                                            // 422153 add edi, 0x170
    fl = f_cmp(eax, ld32(m, ebx + 0x1b20));                        // 422159 cmp eax, [ebx + 0x1b20]
    st32(m, esp + 0xc, eax);                                       // 42215f mov [esp + 0xc], eax
    if (fl.l()) goto L_00421fe3;                                   // 422163 jl 0x421fe3
L_00422169:
    ecx = ld32(m, esp + 0x18c);                                    // 422169 mov ecx, [esp + 0x18c]
    edi = k.pop();                                                 // 422170 pop edi
    esi = k.pop();                                                 // 422171 pop esi
    ebx = k.pop();                                                 // 422172 pop ebx
    ecx = ecx ^ esp;                                               // 422173 xor ecx, esp
    k.call(0x008f9efe);  // __security_check_cookie                // 422175 call 0x8f9efe
    esp = ebp;                                                     // 42217a mov esp, ebp
    ebp = k.pop();                                                 // 42217c pop ebp
    return k.ret(4);                                               // 42217d ret 4
}

// 00422180 FUN_00422180: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00422180, Fn_00422180, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 422180 push ebp
    ebp = esp;                                                     // 422181 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 422183 and esp, 0xfffffff0
    esp = esp - 0xcu;                                              // 422186 sub esp, 0xc
    k.push(ebx);                                                   // 422189 push ebx
    x1 = ld(m, edi);                                               // 42218a movaps xmm1, [edi]
    ecx = esi;                                                     // 42218d mov ecx, esi
    ecx = ecx * 0x70u;                                             // 42218f imul ecx, ecx, 0x70
    eax = esi;                                                     // 422192 mov eax, esi
    eax = eax << 7;                                                // 422194 shl eax, 7
    x0 = ld(m, eax + edx + 0xb0);                                  // 422197 movaps xmm0, [eax + edx + 0xb0]
    x0 = mulps(x0, x1);                                            // 42219f mulps xmm0, xmm1
    x1 = ld(m, eax + edx + 0x90);                                  // 4221a2 movaps xmm1, [eax + edx + 0x90]
    x0 = addps(x0, x1);                                            // 4221aa addps xmm0, xmm1
    st(m, eax + edx + 0x90, x0);                                   // 4221ad movaps [eax + edx + 0x90], xmm0
    x2 = ld(m, ecx + edx + 0xb00);                                 // 4221b5 movaps xmm2, [ecx + edx + 0xb00]
    x5 = X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000);     // 4221bd movaps xmm5, [0x92db80]
    x0 = ld(m, edi);                                               // 4221c4 movaps xmm0, [edi]
    x0 = andps(x0, x5);                                            // 4221c7 andps xmm0, xmm5
    x3 = x0;                                                       // 4221ca movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0x00);                                     // 4221cd shufps xmm3, xmm0, 0
    eax = eax + edx;                                               // 4221d1 add eax, edx
    x4 = x0;                                                       // 4221d3 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x55);                                     // 4221d6 shufps xmm4, xmm0, 0x55
    x6 = x0;                                                       // 4221da movaps xmm6, xmm0
    x6 = shufps(x6, x0, 0xaa);                                     // 4221dd shufps xmm6, xmm0, 0xaa
    ecx = ecx + edx;                                               // 4221e1 add ecx, edx
    x1 = x2;                                                       // 4221e3 movaps xmm1, xmm2
    x1 = shufps(x1, x2, 0x1b);                                     // 4221e6 shufps xmm1, xmm2, 0x1b
    x3 = mulps(x3, x1);                                            // 4221ea mulps xmm3, xmm1
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 4221ed mulps xmm3, [0x93a1f0]
    x1 = shufps(x1, x1, 0xb1);                                     // 4221f4 shufps xmm1, xmm1, 0xb1
    x4 = mulps(x4, x1);                                            // 4221f8 mulps xmm4, xmm1
    x4 = mulps(x4, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 4221fb mulps xmm4, [0x93a200]
    x1 = shufps(x1, x1, 0x1b);                                     // 422202 shufps xmm1, xmm1, 0x1b
    x1 = mulps(x1, x6);                                            // 422206 mulps xmm1, xmm6
    x6 = X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000);     // 422209 movaps xmm6, [0x93a210]
    x1 = mulps(x1, x6);                                            // 422210 mulps xmm1, xmm6
    x1 = addps(x1, x4);                                            // 422213 addps xmm1, xmm4
    x4 = x0;                                                       // 422216 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0xff);                                     // 422219 shufps xmm4, xmm0, 0xff
    x0 = X::v(0xbf800000, 0xbf800000, 0xbf800000, 0x3f800000);     // 42221d movaps xmm0, [0x93a220]
    x4 = mulps(x4, x2);                                            // 422224 mulps xmm4, xmm2
    x2 = mulps(x2, x0);                                            // 422227 mulps xmm2, xmm0
    x4 = addps(x4, x3);                                            // 42222a addps xmm4, xmm3
    x1 = addps(x1, x4);                                            // 42222d addps xmm1, xmm4
    x0 = x1;                                                       // 422230 movaps xmm0, xmm1
    x0 = shufps(x0, x1, 0x1b);                                     // 422233 shufps xmm0, xmm1, 0x1b
    x3 = x2;                                                       // 422237 movaps xmm3, xmm2
    x3 = shufps(x3, x2, 0x00);                                     // 42223a shufps xmm3, xmm2, 0
    x3 = mulps(x3, x0);                                            // 42223e mulps xmm3, xmm0
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 422241 mulps xmm3, [0x93a1f0]
    x0 = shufps(x0, x0, 0xb1);                                     // 422248 shufps xmm0, xmm0, 0xb1
    x4 = x2;                                                       // 42224c movaps xmm4, xmm2
    x4 = shufps(x4, x2, 0x55);                                     // 42224f shufps xmm4, xmm2, 0x55
    x4 = mulps(x4, x0);                                            // 422253 mulps xmm4, xmm0
    x4 = mulps(x4, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 422256 mulps xmm4, [0x93a200]
    x0 = shufps(x0, x0, 0x1b);                                     // 42225d shufps xmm0, xmm0, 0x1b
    x7 = x2;                                                       // 422261 movaps xmm7, xmm2
    x7 = shufps(x7, x2, 0xaa);                                     // 422264 shufps xmm7, xmm2, 0xaa
    x0 = mulps(x0, x7);                                            // 422268 mulps xmm0, xmm7
    x0 = mulps(x0, x6);                                            // 42226b mulps xmm0, xmm6
    x0 = addps(x0, x4);                                            // 42226e addps xmm0, xmm4
    x4 = x2;                                                       // 422271 movaps xmm4, xmm2
    x4 = shufps(x4, x2, 0xff);                                     // 422274 shufps xmm4, xmm2, 0xff
    x4 = mulps(x4, x1);                                            // 422278 mulps xmm4, xmm1
    ebx = ld32(m, ebp + 0x8);                                      // 42227b mov ebx, [ebp + 8]
    x1 = ld(m, ebx);                                               // 42227e movaps xmm1, [ebx]
    x1 = shufps(x1, x1, 0xc9);                                     // 422281 shufps xmm1, xmm1, 0xc9
    x4 = addps(x4, x3);                                            // 422285 addps xmm4, xmm3
    x0 = addps(x0, x4);                                            // 422288 addps xmm0, xmm4
    x0 = shufps(x0, x0, 0xd2);                                     // 42228b shufps xmm0, xmm0, 0xd2
    x2 = x0;                                                       // 42228f movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0xd2);                                     // 422292 shufps xmm2, xmm0, 0xd2
    x3 = x1;                                                       // 422296 movaps xmm3, xmm1
    x3 = shufps(x3, x1, 0xc9);                                     // 422299 shufps xmm3, xmm1, 0xc9
    x0 = mulps(x0, x1);                                            // 42229d mulps xmm0, xmm1
    x1 = ld(m, ecx + 0xb30);                                       // 4222a0 movaps xmm1, [ecx + 0xb30]
    x2 = mulps(x2, x3);                                            // 4222a7 mulps xmm2, xmm3
    x0 = subps(x0, x2);                                            // 4222aa subps xmm0, xmm2
    x2 = ld(m, ecx + 0xb10);                                       // 4222ad movaps xmm2, [ecx + 0xb10]
    x0 = andps(x0, x5);                                            // 4222b4 andps xmm0, xmm5
    x1 = mulps(x1, x0);                                            // 4222b7 mulps xmm1, xmm0
    x1 = addps(x1, x2);                                            // 4222ba addps xmm1, xmm2
    st(m, ecx + 0xb10, x1);                                        // 4222bd movaps [ecx + 0xb10], xmm1
    x2 = ld(m, eax + 0xe0);                                        // 4222c4 movaps xmm2, [eax + 0xe0]
    x1 = ld(m, edi);                                               // 4222cb movaps xmm1, [edi]
    x1 = addps(x1, x2);                                            // 4222ce addps xmm1, xmm2
    st(m, eax + 0xe0, x1);                                         // 4222d1 movaps [eax + 0xe0], xmm1
    eax = esi + 0x1a;                                              // 4222d8 lea eax, [esi + 0x1a]
    eax = eax * 0x70u;                                             // 4222db imul eax, eax, 0x70
    x1 = ld(m, eax + edx);                                         // 4222de movaps xmm1, [eax + edx]
    eax = eax + edx;                                               // 4222e2 add eax, edx
    x1 = addps(x1, x0);                                            // 4222e4 addps xmm1, xmm0
    st(m, eax, x1);                                                // 4222e7 movaps [eax], xmm1
    ebx = k.pop();                                                 // 4222ea pop ebx
    esp = ebp;                                                     // 4222eb mov esp, ebp
    ebp = k.pop();                                                 // 4222ed pop ebp
    return k.ret(4);                                               // 4222ee ret 4
}

// 00422300 FUN_00422300: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00422300, Fn_00422300, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    x0 = ld(m, 0xd61670);                                          // 422300 movaps xmm0, [0xd61670]
    st(m, ecx, x0);                                                // 422307 movaps [ecx], xmm0
    x0 = ld(m, 0xd61670);                                          // 42230a movaps xmm0, [0xd61670]
    edx = 0u;                                                      // 422311 xor edx, edx
    fl = f_cmp(ld32(m, esi + 0x7c), edx);                          // 422313 cmp [esi + 0x7c], edx
    if (fl.le()) goto L_00422342;                                  // 422316 jle 0x422342
    eax = esi + 0xa0;                                              // 422318 lea eax, [esi + 0xa0]
    edi = edi;                                                     // 42231e mov edi, edi
L_00422320:
    x2 = ld(m, eax);                                               // 422320 movaps xmm2, [eax]
    x1 = ld(m, eax - 0x20);                                        // 422323 movaps xmm1, [eax - 0x20]
    x1 = mulps(x1, x2);                                            // 422327 mulps xmm1, xmm2
    x2 = ld(m, ecx);                                               // 42232a movaps xmm2, [ecx]
    x1 = addps(x1, x2);                                            // 42232d addps xmm1, xmm2
    st(m, ecx, x1);                                                // 422330 movaps [ecx], xmm1
    x1 = ld(m, eax);                                               // 422333 movaps xmm1, [eax]
    edx = edx + 1u;                                                // 422336 inc edx
    eax = eax - 0xffffff80u;                                       // 422337 sub eax, -0x80
    fl = f_cmp(edx, ld32(m, esi + 0x7c));                          // 42233a cmp edx, [esi + 0x7c]
    x0 = addps(x0, x1);                                            // 42233d addps xmm0, xmm1
    if (fl.l()) goto L_00422320;                                   // 422340 jl 0x422320
L_00422342:
    x1 = ld(m, ecx);                                               // 422342 movaps xmm1, [ecx]
    x1 = divps(x1, x0);                                            // 422345 divps xmm1, xmm0
    st(m, ecx, x1);                                                // 422348 movaps [ecx], xmm1
    return k.ret(0);                                               // 42234b ret 
}

// 00422350 FUN_00422350: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00422350, Fn_00422350, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    x0 = ld(m, 0xd61670);                                          // 422350 movaps xmm0, [0xd61670]
    st(m, ecx, x0);                                                // 422357 movaps [ecx], xmm0
    x0 = ld(m, 0xd61670);                                          // 42235a movaps xmm0, [0xd61670]
    edx = 0u;                                                      // 422361 xor edx, edx
    fl = f_cmp(ld32(m, esi + 0x7c), edx);                          // 422363 cmp [esi + 0x7c], edx
    if (fl.le()) goto L_00422392;                                  // 422366 jle 0x422392
    eax = esi + 0xa0;                                              // 422368 lea eax, [esi + 0xa0]
    edi = edi;                                                     // 42236e mov edi, edi
L_00422370:
    x2 = ld(m, eax);                                               // 422370 movaps xmm2, [eax]
    x1 = ld(m, eax - 0x10);                                        // 422373 movaps xmm1, [eax - 0x10]
    x1 = mulps(x1, x2);                                            // 422377 mulps xmm1, xmm2
    x2 = ld(m, ecx);                                               // 42237a movaps xmm2, [ecx]
    x1 = addps(x1, x2);                                            // 42237d addps xmm1, xmm2
    st(m, ecx, x1);                                                // 422380 movaps [ecx], xmm1
    x1 = ld(m, eax);                                               // 422383 movaps xmm1, [eax]
    edx = edx + 1u;                                                // 422386 inc edx
    eax = eax - 0xffffff80u;                                       // 422387 sub eax, -0x80
    fl = f_cmp(edx, ld32(m, esi + 0x7c));                          // 42238a cmp edx, [esi + 0x7c]
    x0 = addps(x0, x1);                                            // 42238d addps xmm0, xmm1
    if (fl.l()) goto L_00422370;                                   // 422390 jl 0x422370
L_00422392:
    x1 = ld(m, ecx);                                               // 422392 movaps xmm1, [ecx]
    x1 = divps(x1, x0);                                            // 422395 divps xmm1, xmm0
    st(m, ecx, x1);                                                // 422398 movaps [ecx], xmm1
    return k.ret(0);                                               // 42239b ret 
}

// 004223a0 FUN_004223a0: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x004223a0, Fn_004223a0, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 4223a0 push ebp
    ebp = esp;                                                     // 4223a1 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 4223a3 and esp, 0xfffffff0
    eax = ld32(m, ebp + 0x8);                                      // 4223a6 mov eax, [ebp + 8]
    x0 = ld(m, 0xd61670);                                          // 4223a9 movaps xmm0, [0xd61670]
    esp = esp - 0xcu;                                              // 4223b0 sub esp, 0xc
    edx = 0u;                                                      // 4223b3 xor edx, edx
    st(m, eax, x0);                                                // 4223b5 movaps [eax], xmm0
    fl = f_cmp(ld32(m, esi + 0x7c), edx);                          // 4223b8 cmp [esi + 0x7c], edx
    k.push(ebx);                                                   // 4223bb push ebx
    ebx = ld32(m, ebp + 0xc);                                      // 4223bc mov ebx, [ebp + 0xc]
    if (fl.le()) goto L_00422441;                                  // 4223bf jle 0x422441
    ecx = esi + 0xa0;                                              // 4223c5 lea ecx, [esi + 0xa0]
    goto L_004223d0;                                               // 4223cb jmp 0x4223d0
    ecx = ecx;                                                     // 4223cd lea ecx, [ecx]
L_004223d0:
    x2 = ld(m, ecx - 0x20);                                        // 4223d0 movaps xmm2, [ecx - 0x20]
    x0 = ld(m, ebx);                                               // 4223d4 movaps xmm0, [ebx]
    x3 = ld(m, edi);                                               // 4223d7 movaps xmm3, [edi]
    x2 = subps(x2, x0);                                            // 4223da subps xmm2, xmm0
    x0 = x3;                                                       // 4223dd movaps xmm0, xmm3
    x0 = mulps(x0, x2);                                            // 4223e0 mulps xmm0, xmm2
    x1 = x0;                                                       // 4223e3 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x99);                                     // 4223e6 shufps xmm1, xmm0, 0x99
    x0 = addss(x0, x1);                                            // 4223ea addss xmm0, xmm1
    x1 = shufps(x1, x1, 0x55);                                     // 4223ee shufps xmm1, xmm1, 0x55
    x0 = addss(x0, x1);                                            // 4223f2 addss xmm0, xmm1
    x1 = x0;                                                       // 4223f6 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x00);                                     // 4223f9 shufps xmm1, xmm0, 0
    x1 = mulps(x1, x3);                                            // 4223fd mulps xmm1, xmm3
    x2 = subps(x2, x1);                                            // 422400 subps xmm2, xmm1
    x1 = x2;                                                       // 422403 movaps xmm1, xmm2
    x1 = mulps(x1, x1);                                            // 422406 mulps xmm1, xmm1
    x0 = x1;                                                       // 422409 movaps xmm0, xmm1
    x0 = shufps(x0, x1, 0x66);                                     // 42240c shufps xmm0, xmm1, 0x66
    x1 = addss(x1, x0);                                            // 422410 addss xmm1, xmm0
    x0 = shufps(x0, x0, 0x55);                                     // 422414 shufps xmm0, xmm0, 0x55
    x1 = addss(x1, x0);                                            // 422418 addss xmm1, xmm0
    x0 = x1;                                                       // 42241c movaps xmm0, xmm1
    x1 = ld(m, ecx);                                               // 42241f movaps xmm1, [ecx]
    x0 = shufps(x0, x0, 0x00);                                     // 422422 shufps xmm0, xmm0, 0
    x0 = sqrtps(x0);                                               // 422426 sqrtps xmm0, xmm0
    x1 = mulps(x1, x0);                                            // 422429 mulps xmm1, xmm0
    x1 = mulps(x1, x0);                                            // 42242c mulps xmm1, xmm0
    x0 = ld(m, eax);                                               // 42242f movaps xmm0, [eax]
    x1 = addps(x1, x0);                                            // 422432 addps xmm1, xmm0
    edx = edx + 1u;                                                // 422435 inc edx
    st(m, eax, x1);                                                // 422436 movaps [eax], xmm1
    ecx = ecx - 0xffffff80u;                                       // 422439 sub ecx, -0x80
    fl = f_cmp(edx, ld32(m, esi + 0x7c));                          // 42243c cmp edx, [esi + 0x7c]
    if (fl.l()) goto L_004223d0;                                   // 42243f jl 0x4223d0
L_00422441:
    ebx = k.pop();                                                 // 422441 pop ebx
    esp = ebp;                                                     // 422442 mov esp, ebp
    ebp = k.pop();                                                 // 422444 pop ebp
    return k.ret(8);                                               // 422445 ret 8
}

// 00422450 FUN_00422450: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00422450, Fn_00422450, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    ecx = 0u;                                                      // 422450 xor ecx, ecx
    fl = f_cmp(ld32(m, edx + 0x7c), ecx);                          // 422452 cmp [edx + 0x7c], ecx
    if (fl.le()) goto L_00422543;                                  // 422455 jle 0x422543
    eax = edx + 0xb10;                                             // 42245b lea eax, [edx + 0xb10]
L_00422461:
    x2 = ld(m, eax - 0x10);                                        // 422461 movaps xmm2, [eax - 0x10]
    x1 = X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000);     // 422465 movaps xmm1, [0x92db80]
    x0 = ld(m, esi);                                               // 42246c movaps xmm0, [esi]
    x6 = X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000);     // 42246f movaps xmm6, [0x93a200]
    x0 = andps(x0, x1);                                            // 422476 andps xmm0, xmm1
    x1 = x2;                                                       // 422479 movaps xmm1, xmm2
    x1 = shufps(x1, x2, 0x1b);                                     // 42247c shufps xmm1, xmm2, 0x1b
    x3 = x0;                                                       // 422480 movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0x00);                                     // 422483 shufps xmm3, xmm0, 0
    x3 = mulps(x3, x1);                                            // 422487 mulps xmm3, xmm1
    x4 = x0;                                                       // 42248a movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x55);                                     // 42248d shufps xmm4, xmm0, 0x55
    x1 = shufps(x1, x1, 0xb1);                                     // 422491 shufps xmm1, xmm1, 0xb1
    x4 = mulps(x4, x1);                                            // 422495 mulps xmm4, xmm1
    x5 = x0;                                                       // 422498 movaps xmm5, xmm0
    x5 = shufps(x5, x0, 0xaa);                                     // 42249b shufps xmm5, xmm0, 0xaa
    x1 = shufps(x1, x1, 0x1b);                                     // 42249f shufps xmm1, xmm1, 0x1b
    x7 = x6;                                                       // 4224a3 movaps xmm7, xmm6
    x7 = mulps(x7, x4);                                            // 4224a6 mulps xmm7, xmm4
    x4 = x0;                                                       // 4224a9 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0xff);                                     // 4224ac shufps xmm4, xmm0, 0xff
    x0 = X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000);     // 4224b0 movaps xmm0, [0x93a1f0]
    x4 = mulps(x4, x2);                                            // 4224b7 mulps xmm4, xmm2
    x0 = mulps(x0, x3);                                            // 4224ba mulps xmm0, xmm3
    x4 = addps(x4, x0);                                            // 4224bd addps xmm4, xmm0
    x0 = X::v(0xbf800000, 0xbf800000, 0xbf800000, 0x3f800000);     // 4224c0 movaps xmm0, [0x93a220]
    x0 = mulps(x0, x2);                                            // 4224c7 mulps xmm0, xmm2
    x1 = mulps(x1, x5);                                            // 4224ca mulps xmm1, xmm5
    x5 = X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000);     // 4224cd movaps xmm5, [0x93a210]
    x1 = mulps(x1, x5);                                            // 4224d4 mulps xmm1, xmm5
    x1 = addps(x1, x7);                                            // 4224d7 addps xmm1, xmm7
    x1 = addps(x1, x4);                                            // 4224da addps xmm1, xmm4
    x3 = x0;                                                       // 4224dd movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0x00);                                     // 4224e0 shufps xmm3, xmm0, 0
    x2 = x1;                                                       // 4224e4 movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0x1b);                                     // 4224e7 shufps xmm2, xmm1, 0x1b
    x3 = mulps(x3, x2);                                            // 4224eb mulps xmm3, xmm2
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 4224ee mulps xmm3, [0x93a1f0]
    x2 = shufps(x2, x2, 0xb1);                                     // 4224f5 shufps xmm2, xmm2, 0xb1
    x7 = x2;                                                       // 4224f9 movaps xmm7, xmm2
    x7 = shufps(x7, x2, 0x1b);                                     // 4224fc shufps xmm7, xmm2, 0x1b
    x4 = x0;                                                       // 422500 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x55);                                     // 422503 shufps xmm4, xmm0, 0x55
    x4 = mulps(x4, x2);                                            // 422507 mulps xmm4, xmm2
    x2 = x0;                                                       // 42250a movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0xaa);                                     // 42250d shufps xmm2, xmm0, 0xaa
    x7 = mulps(x7, x2);                                            // 422511 mulps xmm7, xmm2
    x2 = x0;                                                       // 422514 movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0xff);                                     // 422517 shufps xmm2, xmm0, 0xff
    x0 = ld(m, eax);                                               // 42251b movaps xmm0, [eax]
    x7 = mulps(x7, x5);                                            // 42251e mulps xmm7, xmm5
    x4 = mulps(x4, x6);                                            // 422521 mulps xmm4, xmm6
    x7 = addps(x7, x4);                                            // 422524 addps xmm7, xmm4
    x2 = mulps(x2, x1);                                            // 422527 mulps xmm2, xmm1
    x2 = addps(x2, x3);                                            // 42252a addps xmm2, xmm3
    x7 = addps(x7, x2);                                            // 42252d addps xmm7, xmm2
    x7 = addps(x7, x0);                                            // 422530 addps xmm7, xmm0
    st(m, eax, x7);                                                // 422533 movaps [eax], xmm7
    ecx = ecx + 1u;                                                // 422536 inc ecx
    eax = eax + 0x70u;                                             // 422537 add eax, 0x70
    fl = f_cmp(ecx, ld32(m, edx + 0x7c));                          // 42253a cmp ecx, [edx + 0x7c]
    if (fl.l()) goto L_00422461;                                   // 42253d jl 0x422461
L_00422543:
    return k.ret(0);                                               // 422543 ret 
}

// 00422550 FUN_00422550: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00422550, Fn_00422550, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    k.push(esi);                                                   // 422550 push esi
    esi = 0u;                                                      // 422551 xor esi, esi
    fl = f_cmp(ld32(m, edi + 0x7c), esi);                          // 422553 cmp [edi + 0x7c], esi
    if (fl.le()) goto L_0042258c;                                  // 422556 jle 0x42258c
    eax = edi + 0x1460;                                            // 422558 lea eax, [edi + 0x1460]
    edx = edi + 0xb10;                                             // 42255e lea edx, [edi + 0xb10]
    ecx = edi + 0x90;                                              // 422564 lea ecx, [edi + 0x90]
    ebx = ebx;                                                     // 42256a lea ebx, [ebx]
L_00422570:
    x0 = ld(m, ecx);                                               // 422570 movaps xmm0, [ecx]
    st(m, eax - 0x20, x0);                                         // 422573 movaps [eax - 0x20], xmm0
    x0 = ld(m, edx);                                               // 422577 movaps xmm0, [edx]
    st(m, eax, x0);                                                // 42257a movaps [eax], xmm0
    esi = esi + 1u;                                                // 42257d inc esi
    ecx = ecx - 0xffffff80u;                                       // 42257e sub ecx, -0x80
    edx = edx + 0x70u;                                             // 422581 add edx, 0x70
    eax = eax + 0x40u;                                             // 422584 add eax, 0x40
    fl = f_cmp(esi, ld32(m, edi + 0x7c));                          // 422587 cmp esi, [edi + 0x7c]
    if (fl.l()) goto L_00422570;                                   // 42258a jl 0x422570
L_0042258c:
    esi = k.pop();                                                 // 42258c pop esi
    return k.ret(0);                                               // 42258d ret 
}

// 00422590 FUN_00422590: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00422590, Fn_00422590, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    k.push(esi);                                                   // 422590 push esi
    esi = 0u;                                                      // 422591 xor esi, esi
    fl = f_cmp(ld32(m, edi + 0x7c), esi);                          // 422593 cmp [edi + 0x7c], esi
    if (fl.le()) goto L_004225cc;                                  // 422596 jle 0x4225cc
    edx = edi + 0xb10;                                             // 422598 lea edx, [edi + 0xb10]
    eax = edi + 0x1460;                                            // 42259e lea eax, [edi + 0x1460]
    ecx = edi + 0x90;                                              // 4225a4 lea ecx, [edi + 0x90]
    ebx = ebx;                                                     // 4225aa lea ebx, [ebx]
L_004225b0:
    x0 = ld(m, eax - 0x20);                                        // 4225b0 movaps xmm0, [eax - 0x20]
    st(m, ecx, x0);                                                // 4225b4 movaps [ecx], xmm0
    x0 = ld(m, eax);                                               // 4225b7 movaps xmm0, [eax]
    st(m, edx, x0);                                                // 4225ba movaps [edx], xmm0
    esi = esi + 1u;                                                // 4225bd inc esi
    ecx = ecx - 0xffffff80u;                                       // 4225be sub ecx, -0x80
    eax = eax + 0x40u;                                             // 4225c1 add eax, 0x40
    edx = edx + 0x70u;                                             // 4225c4 add edx, 0x70
    fl = f_cmp(esi, ld32(m, edi + 0x7c));                          // 4225c7 cmp esi, [edi + 0x7c]
    if (fl.l()) goto L_004225b0;                                   // 4225ca jl 0x4225b0
L_004225cc:
    esi = k.pop();                                                 // 4225cc pop esi
    return k.ret(0);                                               // 4225cd ret 
}

// 004225d0 FUN_004225d0: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x004225d0, Fn_004225d0, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 4225d0 push ebp
    ebp = esp;                                                     // 4225d1 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 4225d3 and esp, 0xfffffff0
    eax = ld32(m, ebp + 0x8);                                      // 4225d6 mov eax, [ebp + 8]
    ecx = ld32(m, eax + 0x1970);                                   // 4225d9 mov ecx, [eax + 0x1970]
    ecx = ecx * 0x230u;                                            // 4225df imul ecx, ecx, 0x230
    ecx = ld32(m, ecx + eax + 0x39ec);                             // 4225e5 mov ecx, [ecx + eax + 0x39ec]
    fl = f_cmp(ecx, 0xfffffffbu);                                  // 4225ec cmp ecx, -5
    if (fl.e()) goto L_004226e9;                                   // 4225ef je 0x4226e9
    x1 = ld(m, eax + 0xb00);                                       // 4225f5 movaps xmm1, [eax + 0xb00]
    ecx = ecx * 0x230u;                                            // 4225fc imul ecx, ecx, 0x230
    ecx = ld32(m, ecx + eax + 0x39e4);                             // 422602 mov ecx, [ecx + eax + 0x39e4]
    x2 = X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000);     // 422609 movaps xmm2, [0x92db80]
    ecx = ecx * 0x170u;                                            // 422610 imul ecx, ecx, 0x170
    x0 = ld(m, ecx + eax + 0x1b60);                                // 422616 movaps xmm0, [ecx + eax + 0x1b60]
    x5 = X::v(0xbf800000, 0xbf800000, 0xbf800000, 0x3f800000);     // 42261e movaps xmm5, [0x93a220]
    x7 = X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000);     // 422625 movaps xmm7, [0x93a200]
    x0 = andps(x0, x2);                                            // 42262c andps xmm0, xmm2
    x3 = x0;                                                       // 42262f movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0x00);                                     // 422632 shufps xmm3, xmm0, 0
    x5 = mulps(x5, x1);                                            // 422636 mulps xmm5, xmm1
    x4 = x0;                                                       // 422639 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x55);                                     // 42263c shufps xmm4, xmm0, 0x55
    x6 = x0;                                                       // 422640 movaps xmm6, xmm0
    x6 = shufps(x6, x0, 0xaa);                                     // 422643 shufps xmm6, xmm0, 0xaa
    x2 = x5;                                                       // 422647 movaps xmm2, xmm5
    x2 = shufps(x2, x5, 0x1b);                                     // 42264a shufps xmm2, xmm5, 0x1b
    x3 = mulps(x3, x2);                                            // 42264e mulps xmm3, xmm2
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 422651 mulps xmm3, [0x93a1f0]
    x2 = shufps(x2, x2, 0xb1);                                     // 422658 shufps xmm2, xmm2, 0xb1
    x4 = mulps(x4, x2);                                            // 42265c mulps xmm4, xmm2
    x4 = mulps(x4, x7);                                            // 42265f mulps xmm4, xmm7
    x2 = shufps(x2, x2, 0x1b);                                     // 422662 shufps xmm2, xmm2, 0x1b
    x2 = mulps(x2, x6);                                            // 422666 mulps xmm2, xmm6
    x6 = X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000);     // 422669 movaps xmm6, [0x93a210]
    x2 = mulps(x2, x6);                                            // 422670 mulps xmm2, xmm6
    x2 = addps(x2, x4);                                            // 422673 addps xmm2, xmm4
    x4 = x0;                                                       // 422676 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0xff);                                     // 422679 shufps xmm4, xmm0, 0xff
    x4 = mulps(x4, x5);                                            // 42267d mulps xmm4, xmm5
    x4 = addps(x4, x3);                                            // 422680 addps xmm4, xmm3
    x2 = addps(x2, x4);                                            // 422683 addps xmm2, xmm4
    x3 = x1;                                                       // 422686 movaps xmm3, xmm1
    x3 = shufps(x3, x1, 0x00);                                     // 422689 shufps xmm3, xmm1, 0
    x0 = x2;                                                       // 42268d movaps xmm0, xmm2
    x0 = shufps(x0, x2, 0x1b);                                     // 422690 shufps xmm0, xmm2, 0x1b
    x3 = mulps(x3, x0);                                            // 422694 mulps xmm3, xmm0
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 422697 mulps xmm3, [0x93a1f0]
    x0 = shufps(x0, x0, 0xb1);                                     // 42269e shufps xmm0, xmm0, 0xb1
    x4 = x1;                                                       // 4226a2 movaps xmm4, xmm1
    x4 = shufps(x4, x1, 0x55);                                     // 4226a5 shufps xmm4, xmm1, 0x55
    x4 = mulps(x4, x0);                                            // 4226a9 mulps xmm4, xmm0
    x0 = shufps(x0, x0, 0x1b);                                     // 4226ac shufps xmm0, xmm0, 0x1b
    x5 = x1;                                                       // 4226b0 movaps xmm5, xmm1
    x5 = shufps(x5, x1, 0xaa);                                     // 4226b3 shufps xmm5, xmm1, 0xaa
    x4 = mulps(x4, x7);                                            // 4226b7 mulps xmm4, xmm7
    x0 = mulps(x0, x5);                                            // 4226ba mulps xmm0, xmm5
    x0 = mulps(x0, x6);                                            // 4226bd mulps xmm0, xmm6
    x0 = addps(x0, x4);                                            // 4226c0 addps xmm0, xmm4
    x4 = x1;                                                       // 4226c3 movaps xmm4, xmm1
    x4 = shufps(x4, x1, 0xff);                                     // 4226c6 shufps xmm4, xmm1, 0xff
    x4 = mulps(x4, x2);                                            // 4226ca mulps xmm4, xmm2
    x4 = addps(x4, x3);                                            // 4226cd addps xmm4, xmm3
    x0 = addps(x0, x4);                                            // 4226d0 addps xmm0, xmm4
    st(m, edx, x0);                                                // 4226d3 movaps [edx], xmm0
    x1 = ld(m, eax + 0x80);                                        // 4226d6 movaps xmm1, [eax + 0x80]
    x1 = addps(x1, x0);                                            // 4226dd addps xmm1, xmm0
    st(m, edx, x1);                                                // 4226e0 movaps [edx], xmm1
    esp = ebp;                                                     // 4226e3 mov esp, ebp
    ebp = k.pop();                                                 // 4226e5 pop ebp
    return k.ret(4);                                               // 4226e6 ret 4
L_004226e9:
    x0 = ld(m, eax + 0x80);                                        // 4226e9 movaps xmm0, [eax + 0x80]
    st(m, edx, x0);                                                // 4226f0 movaps [edx], xmm0
    esp = ebp;                                                     // 4226f3 mov esp, ebp
    ebp = k.pop();                                                 // 4226f5 pop ebp
    return k.ret(4);                                               // 4226f6 ret 4
}

// 00422700 FUN_00422700: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00422700, Fn_00422700, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 422700 push ebp
    ebp = esp;                                                     // 422701 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 422703 and esp, 0xfffffff0
    eax = ld32(m, ebp + 0x8);                                      // 422706 mov eax, [ebp + 8]
    x1 = X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000);     // 422709 movaps xmm1, [0x92db80]
    eax = eax * 0x70u;                                             // 422710 imul eax, eax, 0x70
    x2 = ld(m, eax + ecx + 0xb00);                                 // 422713 movaps xmm2, [eax + ecx + 0xb00]
    x6 = X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000);     // 42271b movaps xmm6, [0x93a200]
    eax = eax + ecx;                                               // 422722 add eax, ecx
    ecx = ld32(m, ebp + 0xc);                                      // 422724 mov ecx, [ebp + 0xc]
    x0 = ld(m, ecx);                                               // 422727 movaps xmm0, [ecx]
    x0 = andps(x0, x1);                                            // 42272a andps xmm0, xmm1
    x1 = x2;                                                       // 42272d movaps xmm1, xmm2
    x1 = shufps(x1, x2, 0x1b);                                     // 422730 shufps xmm1, xmm2, 0x1b
    x3 = x0;                                                       // 422734 movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0x00);                                     // 422737 shufps xmm3, xmm0, 0
    x3 = mulps(x3, x1);                                            // 42273b mulps xmm3, xmm1
    x4 = x0;                                                       // 42273e movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x55);                                     // 422741 shufps xmm4, xmm0, 0x55
    x1 = shufps(x1, x1, 0xb1);                                     // 422745 shufps xmm1, xmm1, 0xb1
    x4 = mulps(x4, x1);                                            // 422749 mulps xmm4, xmm1
    x5 = x0;                                                       // 42274c movaps xmm5, xmm0
    x5 = shufps(x5, x0, 0xaa);                                     // 42274f shufps xmm5, xmm0, 0xaa
    x1 = shufps(x1, x1, 0x1b);                                     // 422753 shufps xmm1, xmm1, 0x1b
    x7 = x6;                                                       // 422757 movaps xmm7, xmm6
    x7 = mulps(x7, x4);                                            // 42275a mulps xmm7, xmm4
    x4 = x0;                                                       // 42275d movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0xff);                                     // 422760 shufps xmm4, xmm0, 0xff
    x0 = X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000);     // 422764 movaps xmm0, [0x93a1f0]
    x4 = mulps(x4, x2);                                            // 42276b mulps xmm4, xmm2
    x0 = mulps(x0, x3);                                            // 42276e mulps xmm0, xmm3
    x4 = addps(x4, x0);                                            // 422771 addps xmm4, xmm0
    x0 = X::v(0xbf800000, 0xbf800000, 0xbf800000, 0x3f800000);     // 422774 movaps xmm0, [0x93a220]
    x0 = mulps(x0, x2);                                            // 42277b mulps xmm0, xmm2
    x1 = mulps(x1, x5);                                            // 42277e mulps xmm1, xmm5
    x5 = X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000);     // 422781 movaps xmm5, [0x93a210]
    x1 = mulps(x1, x5);                                            // 422788 mulps xmm1, xmm5
    x1 = addps(x1, x7);                                            // 42278b addps xmm1, xmm7
    x1 = addps(x1, x4);                                            // 42278e addps xmm1, xmm4
    x3 = x0;                                                       // 422791 movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0x00);                                     // 422794 shufps xmm3, xmm0, 0
    x2 = x1;                                                       // 422798 movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0x1b);                                     // 42279b shufps xmm2, xmm1, 0x1b
    x3 = mulps(x3, x2);                                            // 42279f mulps xmm3, xmm2
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 4227a2 mulps xmm3, [0x93a1f0]
    x2 = shufps(x2, x2, 0xb1);                                     // 4227a9 shufps xmm2, xmm2, 0xb1
    x7 = x2;                                                       // 4227ad movaps xmm7, xmm2
    x7 = shufps(x7, x2, 0x1b);                                     // 4227b0 shufps xmm7, xmm2, 0x1b
    x4 = x0;                                                       // 4227b4 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x55);                                     // 4227b7 shufps xmm4, xmm0, 0x55
    x4 = mulps(x4, x2);                                            // 4227bb mulps xmm4, xmm2
    x2 = x0;                                                       // 4227be movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0xaa);                                     // 4227c1 shufps xmm2, xmm0, 0xaa
    x7 = mulps(x7, x2);                                            // 4227c5 mulps xmm7, xmm2
    x2 = x0;                                                       // 4227c8 movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0xff);                                     // 4227cb shufps xmm2, xmm0, 0xff
    x0 = ld(m, eax + 0xb50);                                       // 4227cf movaps xmm0, [eax + 0xb50]
    x7 = mulps(x7, x5);                                            // 4227d6 mulps xmm7, xmm5
    x4 = mulps(x4, x6);                                            // 4227d9 mulps xmm4, xmm6
    x7 = addps(x7, x4);                                            // 4227dc addps xmm7, xmm4
    x2 = mulps(x2, x1);                                            // 4227df mulps xmm2, xmm1
    x2 = addps(x2, x3);                                            // 4227e2 addps xmm2, xmm3
    x7 = addps(x7, x2);                                            // 4227e5 addps xmm7, xmm2
    x7 = addps(x7, x0);                                            // 4227e8 addps xmm7, xmm0
    st(m, eax + 0xb50, x7);                                        // 4227eb movaps [eax + 0xb50], xmm7
    esp = ebp;                                                     // 4227f2 mov esp, ebp
    ebp = k.pop();                                                 // 4227f4 pop ebp
    return k.ret(8);                                               // 4227f5 ret 8
}

// 00422800 FUN_00422800: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00422800, Fn_00422800, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    edx = 0u;                                                      // 422800 xor edx, edx
    fl = f_cmp(ld32(m, esi + 0x7c), edx);                          // 422802 cmp [esi + 0x7c], edx
    if (fl.le()) goto L_00422849;                                  // 422805 jle 0x422849
    ecx = esi + 0xb50;                                             // 422807 lea ecx, [esi + 0xb50]
    eax = esi + 0xd0;                                              // 42280d lea eax, [esi + 0xd0]
L_00422813:
    x0 = ld(m, 0xd61670);                                          // 422813 movaps xmm0, [0xd61670]
    st(m, eax + 0x10, x0);                                         // 42281a movaps [eax + 0x10], xmm0
    x0 = ld(m, 0xd61670);                                          // 42281e movaps xmm0, [0xd61670]
    st(m, eax, x0);                                                // 422825 movaps [eax], xmm0
    x0 = ld(m, 0xd61670);                                          // 422828 movaps xmm0, [0xd61670]
    st(m, ecx + 0x10, x0);                                         // 42282f movaps [ecx + 0x10], xmm0
    x0 = ld(m, 0xd61670);                                          // 422833 movaps xmm0, [0xd61670]
    st(m, ecx, x0);                                                // 42283a movaps [ecx], xmm0
    edx = edx + 1u;                                                // 42283d inc edx
    eax = eax - 0xffffff80u;                                       // 42283e sub eax, -0x80
    ecx = ecx + 0x70u;                                             // 422841 add ecx, 0x70
    fl = f_cmp(edx, ld32(m, esi + 0x7c));                          // 422844 cmp edx, [esi + 0x7c]
    if (fl.l()) goto L_00422813;                                   // 422847 jl 0x422813
L_00422849:
    return k.ret(0);                                               // 422849 ret 
}

// 00422850 FUN_00422850: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00422850, Fn_00422850, "math", 0) {  // outputs: what callers read (it calls other groups' code)
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 422850 push ebp
    ebp = esp;                                                     // 422851 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 422853 and esp, 0xfffffff0
    esp = esp - 0x68u;                                             // 422856 sub esp, 0x68
    eax = ld32(m, 0x9b45e4);                                       // 422859 mov eax, [0x9b45e4]
    eax = eax ^ esp;                                               // 42285e xor eax, esp
    st32(m, esp + 0x64, eax);                                      // 422860 mov [esp + 0x64], eax
    fl = f_cmp(ld32(m, edi + 0x7c), 0x0u);                         // 422864 cmp [edi + 0x7c], 0
    x1 = sd(0x3ff0000000000000ull);                                // 422868 movsd xmm1, [0x977638]
    x0 = cvtss2sd(x0, x0);                                         // 422870 cvtss2sd xmm0, xmm0
    x1 = divsd(x1, x0);                                            // 422874 divsd xmm1, xmm0
    x5 = cvtpd2ps(x1);                                             // 422878 cvtpd2ps xmm5, xmm1
    k.push(ebx);                                                   // 42287c push ebx
    k.push(esi);                                                   // 42287d push esi
    st32(m, esp + 0xc, 0x0u);                                      // 42287e mov [esp + 0xc], 0
    x5 = shufps(x5, x5, 0x00);                                     // 422886 shufps xmm5, xmm5, 0
    if (fl.le()) goto L_00422910;                                  // 42288a jle 0x422910
    esi = edi + 0xb10;                                             // 422890 lea esi, [edi + 0xb10]
    ebx = edi + 0xd0;                                              // 422896 lea ebx, [edi + 0xd0]
    esp = esp;                                                     // 42289c lea esp, [esp]
L_004228a0:
    x1 = ld(m, edi);                                               // 4228a0 movaps xmm1, [edi]
    x0 = ld(m, ebx - 0x30);                                        // 4228a3 movaps xmm0, [ebx - 0x30]
    x0 = mulps(x0, x1);                                            // 4228a7 mulps xmm0, xmm1
    x1 = ld(m, ebx);                                               // 4228aa movaps xmm1, [ebx]
    x0 = addps(x0, x1);                                            // 4228ad addps xmm0, xmm1
    st(m, ebx - 0x10, x0);                                         // 4228b0 movaps [ebx - 0x10], xmm0
    x0 = ld(m, esi + 0x40);                                        // 4228b4 movaps xmm0, [esi + 0x40]
    st(m, esi + 0x30, x0);                                         // 4228b8 movaps [esi + 0x30], xmm0
    x0 = ld(m, esi + 0x10);                                        // 4228bc movaps xmm0, [esi + 0x10]
    x1 = ld(m, esi);                                               // 4228c0 movaps xmm1, [esi]
    eax = esp + 0x30;                                              // 4228c3 lea eax, [esp + 0x30]
    x0 = mulps(x0, x1);                                            // 4228c7 mulps xmm0, xmm1
    k.push(eax);                                                   // 4228ca push eax
    ecx = esi;                                                     // 4228cb mov ecx, esi
    st(m, esp + 0x14, x0);                                         // 4228cd movaps [esp + 0x14], xmm0
    k.call(0x0041f390);  // FUN_0041f390                           // 4228d2 call 0x41f390
    ecx = esp + 0x30;                                              // 4228d7 lea ecx, [esp + 0x30]
    k.push(ecx);                                                   // 4228db push ecx
    edx = esp + 0x14;                                              // 4228dc lea edx, [esp + 0x14]
    ecx = esp + 0x24;                                              // 4228e0 lea ecx, [esp + 0x24]
    k.call(0x0041f5c0);  // FUN_0041f5c0                           // 4228e4 call 0x41f5c0
    x0 = ld(m, esi + 0x30);                                        // 4228e9 movaps xmm0, [esi + 0x30]
    eax = ld32(m, esp + 0x10);                                     // 4228ed mov eax, [esp + 0x10]
    x1 = ld(m, esp + 0x24);                                        // 4228f1 movaps xmm1, [esp + 0x24]
    x0 = subps(x0, x1);                                            // 4228f6 subps xmm0, xmm1
    st(m, esi + 0x30, x0);                                         // 4228f9 movaps [esi + 0x30], xmm0
    eax = eax + 1u;                                                // 4228fd inc eax
    esp = esp + 0x4u;                                              // 4228fe add esp, 4
    ebx = ebx - 0xffffff80u;                                       // 422901 sub ebx, -0x80
    esi = esi + 0x70u;                                             // 422904 add esi, 0x70
    fl = f_cmp(eax, ld32(m, edi + 0x7c));                          // 422907 cmp eax, [edi + 0x7c]
    st32(m, esp + 0xc, eax);                                       // 42290a mov [esp + 0xc], eax
    if (fl.l()) goto L_004228a0;                                   // 42290e jl 0x4228a0
L_00422910:
    edx = 0u;                                                      // 422910 xor edx, edx
    fl = f_cmp(ld32(m, edi + 0x7c), edx);                          // 422912 cmp [edi + 0x7c], edx
    if (fl.le()) goto L_0042294f;                                  // 422915 jle 0x42294f
    ecx = edi + 0xb40;                                             // 422917 lea ecx, [edi + 0xb40]
    eax = edi + 0xc0;                                              // 42291d lea eax, [edi + 0xc0]
L_00422923:
    x0 = ld(m, eax + 0x20);                                        // 422923 movaps xmm0, [eax + 0x20]
    x1 = ld(m, eax);                                               // 422927 movaps xmm1, [eax]
    x0 = mulps(x0, x5);                                            // 42292a mulps xmm0, xmm5
    x0 = addps(x0, x1);                                            // 42292d addps xmm0, xmm1
    st(m, eax, x0);                                                // 422930 movaps [eax], xmm0
    x0 = ld(m, ecx + 0x20);                                        // 422933 movaps xmm0, [ecx + 0x20]
    x1 = ld(m, ecx);                                               // 422937 movaps xmm1, [ecx]
    x0 = mulps(x0, x5);                                            // 42293a mulps xmm0, xmm5
    x0 = addps(x0, x1);                                            // 42293d addps xmm0, xmm1
    st(m, ecx, x0);                                                // 422940 movaps [ecx], xmm0
    edx = edx + 1u;                                                // 422943 inc edx
    eax = eax - 0xffffff80u;                                       // 422944 sub eax, -0x80
    ecx = ecx + 0x70u;                                             // 422947 add ecx, 0x70
    fl = f_cmp(edx, ld32(m, edi + 0x7c));                          // 42294a cmp edx, [edi + 0x7c]
    if (fl.l()) goto L_00422923;                                   // 42294d jl 0x422923
L_0042294f:
    ecx = ld32(m, esp + 0x6c);                                     // 42294f mov ecx, [esp + 0x6c]
    esi = k.pop();                                                 // 422953 pop esi
    ebx = k.pop();                                                 // 422954 pop ebx
    ecx = ecx ^ esp;                                               // 422955 xor ecx, esp
    k.call(0x008f9efe);  // __security_check_cookie                // 422957 call 0x8f9efe
    esp = ebp;                                                     // 42295c mov esp, ebp
    ebp = k.pop();                                                 // 42295e pop ebp
    return k.ret(0);                                               // 42295f ret 
}

// 00422960 FUN_00422960: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00422960, Fn_00422960, "math", 0) {  // outputs: what callers read (it calls other groups' code)
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 422960 push ebp
    ebp = esp;                                                     // 422961 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 422963 and esp, 0xfffffff0
    esp = esp - 0x204u;                                            // 422966 sub esp, 0x204
    eax = ld32(m, 0x9b45e4);                                       // 42296c mov eax, [0x9b45e4]
    eax = eax ^ esp;                                               // 422971 xor eax, esp
    st32(m, esp + 0x200, eax);                                     // 422973 mov [esp + 0x200], eax
    k.push(ebx);                                                   // 42297a push ebx
    ebx = ld32(m, ebp + 0x8);                                      // 42297b mov ebx, [ebp + 8]
    k.push(esi);                                                   // 42297e push esi
    esi = 0u;                                                      // 42297f xor esi, esi
    fl = f_cmp(ld32(m, ebx + 0x9390), esi);                        // 422981 cmp [ebx + 0x9390], esi
    k.push(edi);                                                   // 422987 push edi
    st32(m, esp + 0x18, ebx);                                      // 422988 mov [esp + 0x18], ebx
    if (fl.le()) goto L_00422c0e;                                  // 42298c jle 0x422c0e
    edi = ebx + 0x9394;                                            // 422992 lea edi, [ebx + 0x9394]
L_00422998:
    eax = ld32(m, edi);                                            // 422998 mov eax, [edi]
    x0 = ld(m, 0xd61670);                                          // 42299a movaps xmm0, [0xd61670]
    eax = eax * 0x230u;                                            // 4229a1 imul eax, eax, 0x230
    fl = f_cmp(ld32(m, eax + ebx + 0x39e0), 0x0u);                 // 4229a7 cmp [eax + ebx + 0x39e0], 0
    edx = eax + ebx + 0x39e0;                                      // 4229af lea edx, [eax + ebx + 0x39e0]
    st(m, edx + 0x40, x0);                                         // 4229b6 movaps [edx + 0x40], xmm0
    x0 = ld(m, 0xd61670);                                          // 4229ba movaps xmm0, [0xd61670]
    st(m, edx + 0x50, x0);                                         // 4229c1 movaps [edx + 0x50], xmm0
    x0 = ld(m, 0xd61670);                                          // 4229c5 movaps xmm0, [0xd61670]
    st(m, edx + 0x60, x0);                                         // 4229cc movaps [edx + 0x60], xmm0
    if (fl.ne()) goto L_00422b02;                                  // 4229d0 jne 0x422b02
    x0 = ld(m, 0xd61670);                                          // 4229d6 movaps xmm0, [0xd61670]
    st(m, edx + 0x70, x0);                                         // 4229dd movaps [edx + 0x70], xmm0
    x0 = ld(m, 0xd61670);                                          // 4229e1 movaps xmm0, [0xd61670]
    st(m, edx + 0x80, x0);                                         // 4229e8 movaps [edx + 0x80], xmm0
    x0 = ld(m, 0xd61670);                                          // 4229ef movaps xmm0, [0xd61670]
    st(m, edx + 0x90, x0);                                         // 4229f6 movaps [edx + 0x90], xmm0
    x0 = ld(m, 0xd61670);                                          // 4229fd movaps xmm0, [0xd61670]
    st(m, edx + 0xa0, x0);                                         // 422a04 movaps [edx + 0xa0], xmm0
    x0 = ld(m, 0xd61670);                                          // 422a0b movaps xmm0, [0xd61670]
    st(m, edx + 0xb0, x0);                                         // 422a12 movaps [edx + 0xb0], xmm0
    x0 = ld(m, 0xd61670);                                          // 422a19 movaps xmm0, [0xd61670]
    st(m, edx + 0xc0, x0);                                         // 422a20 movaps [edx + 0xc0], xmm0
    x0 = ld(m, 0xd61670);                                          // 422a27 movaps xmm0, [0xd61670]
    st(m, edx + 0xd0, x0);                                         // 422a2e movaps [edx + 0xd0], xmm0
    x0 = ld(m, 0xd61670);                                          // 422a35 movaps xmm0, [0xd61670]
    st(m, edx + 0xe0, x0);                                         // 422a3c movaps [edx + 0xe0], xmm0
    x0 = ld(m, 0xd61670);                                          // 422a43 movaps xmm0, [0xd61670]
    st(m, edx + 0xf0, x0);                                         // 422a4a movaps [edx + 0xf0], xmm0
    ecx = ld32(m, edx + 0x4);                                      // 422a51 mov ecx, [edx + 4]
    x1 = ld(m, 0xd615c0);                                          // 422a54 movaps xmm1, [0xd615c0]
    ecx = ecx << 7;                                                // 422a5b shl ecx, 7
    x0 = ld(m, ecx + ebx + 0xa0);                                  // 422a5e movaps xmm0, [ecx + ebx + 0xa0]
    x0 = mulps(x0, x1);                                            // 422a66 mulps xmm0, xmm1
    st(m, edx + 0x40, x0);                                         // 422a69 movaps [edx + 0x40], xmm0
    eax = ld32(m, edx + 0x4);                                      // 422a6d mov eax, [edx + 4]
    x1 = ld(m, 0xd616a0);                                          // 422a70 movaps xmm1, [0xd616a0]
    eax = eax << 7;                                                // 422a77 shl eax, 7
    x0 = ld(m, eax + ebx + 0xa0);                                  // 422a7a movaps xmm0, [eax + ebx + 0xa0]
    x0 = mulps(x0, x1);                                            // 422a82 mulps xmm0, xmm1
    st(m, edx + 0x50, x0);                                         // 422a85 movaps [edx + 0x50], xmm0
    ecx = ld32(m, edx + 0x4);                                      // 422a89 mov ecx, [edx + 4]
    x1 = ld(m, 0xd61680);                                          // 422a8c movaps xmm1, [0xd61680]
    ecx = ecx << 7;                                                // 422a93 shl ecx, 7
    x0 = ld(m, ecx + ebx + 0xa0);                                  // 422a96 movaps xmm0, [ecx + ebx + 0xa0]
    x0 = mulps(x0, x1);                                            // 422a9e mulps xmm0, xmm1
    st(m, edx + 0x60, x0);                                         // 422aa1 movaps [edx + 0x60], xmm0
    eax = ld32(m, edx + 0x4);                                      // 422aa5 mov eax, [edx + 4]
    x1 = ld(m, 0xd615c0);                                          // 422aa8 movaps xmm1, [0xd615c0]
    eax = eax * 0x70u;                                             // 422aaf imul eax, eax, 0x70
    x0 = ld(m, eax + ebx + 0xb20);                                 // 422ab2 movaps xmm0, [eax + ebx + 0xb20]
    x0 = mulps(x0, x1);                                            // 422aba mulps xmm0, xmm1
    st(m, edx + 0xd0, x0);                                         // 422abd movaps [edx + 0xd0], xmm0
    ecx = ld32(m, edx + 0x4);                                      // 422ac4 mov ecx, [edx + 4]
    x1 = ld(m, 0xd616a0);                                          // 422ac7 movaps xmm1, [0xd616a0]
    ecx = ecx * 0x70u;                                             // 422ace imul ecx, ecx, 0x70
    x0 = ld(m, ecx + ebx + 0xb20);                                 // 422ad1 movaps xmm0, [ecx + ebx + 0xb20]
    x0 = mulps(x0, x1);                                            // 422ad9 mulps xmm0, xmm1
    st(m, edx + 0xe0, x0);                                         // 422adc movaps [edx + 0xe0], xmm0
    eax = ld32(m, edx + 0x4);                                      // 422ae3 mov eax, [edx + 4]
    x1 = ld(m, 0xd61680);                                          // 422ae6 movaps xmm1, [0xd61680]
    eax = eax * 0x70u;                                             // 422aed imul eax, eax, 0x70
    x0 = ld(m, eax + ebx + 0xb20);                                 // 422af0 movaps xmm0, [eax + ebx + 0xb20]
    x0 = mulps(x0, x1);                                            // 422af8 mulps xmm0, xmm1
    st(m, edx + 0xf0, x0);                                         // 422afb movaps [edx + 0xf0], xmm0
L_00422b02:
    eax = ld32(m, edx + 0xc);                                      // 422b02 mov eax, [edx + 0xc]
    fl = f_cmp(eax, 0xfffffffbu);                                  // 422b05 cmp eax, -5
    if (fl.e()) goto L_00422bfe;                                   // 422b08 je 0x422bfe
    fl = f_cmp(ld32(m, edx), 0x0u);                                // 422b0e cmp [edx], 0
    x0 = ld(m, 0xd615c0);                                          // 422b11 movaps xmm0, [0xd615c0]
    if (fl.e()) goto L_00422b52;                                   // 422b18 je 0x422b52
    st(m, edx + 0x1c0, x0);                                        // 422b1a movaps [edx + 0x1c0], xmm0
    x0 = ld(m, 0xd616a0);                                          // 422b21 movaps xmm0, [0xd616a0]
    st(m, edx + 0x1d0, x0);                                        // 422b28 movaps [edx + 0x1d0], xmm0
    x0 = ld(m, 0xd61680);                                          // 422b2f movaps xmm0, [0xd61680]
    st(m, edx + 0x1e0, x0);                                        // 422b36 movaps [edx + 0x1e0], xmm0
    ecx = ld32(m, edx + 0x4);                                      // 422b3d mov ecx, [edx + 4]
    ecx = ecx * 0x170u;                                            // 422b40 imul ecx, ecx, 0x170
    eax = ecx + ebx + 0x1b70;                                      // 422b46 lea eax, [ecx + ebx + 0x1b70]
    goto L_00422bde;                                               // 422b4d jmp 0x422bde
L_00422b52:
    eax = eax * 0x230u;                                            // 422b52 imul eax, eax, 0x230
    eax = ld32(m, eax + ebx + 0x39e4);                             // 422b58 mov eax, [eax + ebx + 0x39e4]
    st(m, edx + 0x1c0, x0);                                        // 422b5f movaps [edx + 0x1c0], xmm0
    eax = eax * 0x170u;                                            // 422b66 imul eax, eax, 0x170
    x0 = ld(m, 0xd616a0);                                          // 422b6c movaps xmm0, [0xd616a0]
    st(m, edx + 0x1d0, x0);                                        // 422b73 movaps [edx + 0x1d0], xmm0
    x0 = ld(m, 0xd61680);                                          // 422b7a movaps xmm0, [0xd61680]
    st(m, edx + 0x1e0, x0);                                        // 422b81 movaps [edx + 0x1e0], xmm0
    x1 = ld(m, edx + 0x1c0);                                       // 422b88 movaps xmm1, [edx + 0x1c0]
    x0 = X{};                                                      // 422b8f xorps xmm0, xmm0
    x2 = x0;                                                       // 422b92 movaps xmm2, xmm0
    x2 = subps(x2, x1);                                            // 422b95 subps xmm2, xmm1
    st(m, edx + 0x1c0, x2);                                        // 422b98 movaps [edx + 0x1c0], xmm2
    x2 = ld(m, edx + 0x1d0);                                       // 422b9f movaps xmm2, [edx + 0x1d0]
    eax = eax + ebx + 0x1b30;                                      // 422ba6 lea eax, [eax + ebx + 0x1b30]
    x1 = x0;                                                       // 422bad movaps xmm1, xmm0
    x1 = subps(x1, x2);                                            // 422bb0 subps xmm1, xmm2
    st(m, edx + 0x1d0, x1);                                        // 422bb3 movaps [edx + 0x1d0], xmm1
    x1 = ld(m, edx + 0x1e0);                                       // 422bba movaps xmm1, [edx + 0x1e0]
    ecx = eax + 0x70;                                              // 422bc1 lea ecx, [eax + 0x70]
    eax = esp + 0x170;                                             // 422bc4 lea eax, [esp + 0x170]
    x0 = subps(x0, x1);                                            // 422bcb subps xmm0, xmm1
    k.push(eax);                                                   // 422bce push eax
    st(m, edx + 0x1e0, x0);                                        // 422bcf movaps [edx + 0x1e0], xmm0
    k.call(0x0041f650);  // FUN_0041f650                           // 422bd6 call 0x41f650
    esp = esp + 0x4u;                                              // 422bdb add esp, 4
L_00422bde:
    x0 = ld(m, eax);                                               // 422bde movaps xmm0, [eax]
    st(m, edx + 0x1f0, x0);                                        // 422be1 movaps [edx + 0x1f0], xmm0
    x0 = ld(m, eax + 0x10);                                        // 422be8 movaps xmm0, [eax + 0x10]
    st(m, edx + 0x200, x0);                                        // 422bec movaps [edx + 0x200], xmm0
    x0 = ld(m, eax + 0x20);                                        // 422bf3 movaps xmm0, [eax + 0x20]
    st(m, edx + 0x210, x0);                                        // 422bf7 movaps [edx + 0x210], xmm0
L_00422bfe:
    esi = esi + 1u;                                                // 422bfe inc esi
    edi = edi + 0x4u;                                              // 422bff add edi, 4
    fl = f_cmp(esi, ld32(m, ebx + 0x9390));                        // 422c02 cmp esi, [ebx + 0x9390]
    if (fl.l()) goto L_00422998;                                   // 422c08 jl 0x422998
L_00422c0e:
    fl = f_cmp(ld32(m, ebx + 0x9390), 0x0u);                       // 422c0e cmp [ebx + 0x9390], 0
    st32(m, esp + 0x2c, 0x0u);                                     // 422c15 mov [esp + 0x2c], 0
    if (fl.le()) goto L_0042328d;                                  // 422c1d jle 0x42328d
    ecx = ebx + 0x9394;                                            // 422c23 lea ecx, [ebx + 0x9394]
    st32(m, esp + 0x24, ecx);                                      // 422c29 mov [esp + 0x24], ecx
L_00422c2d:
    edx = ld32(m, esp + 0x24);                                     // 422c2d mov edx, [esp + 0x24]
    eax = ld32(m, edx);                                            // 422c31 mov eax, [edx]
    eax = eax * 0x230u;                                            // 422c33 imul eax, eax, 0x230
    fl = f_cmp(ld32(m, eax + ebx + 0x39f0), 0x0u);                 // 422c39 cmp [eax + ebx + 0x39f0], 0
    edi = eax + ebx + 0x39e0;                                      // 422c41 lea edi, [eax + ebx + 0x39e0]
    st32(m, esp + 0x1c, 0x0u);                                     // 422c48 mov [esp + 0x1c], 0
    if (fl.le()) goto L_00423041;                                  // 422c50 jle 0x423041
    ecx = edi + 0x14;                                              // 422c56 lea ecx, [edi + 0x14]
    st32(m, esp + 0x28, ecx);                                      // 422c59 mov [esp + 0x28], ecx
    ecx = ecx;                                                     // 422c5d lea ecx, [ecx]
L_00422c60:
    edx = ld32(m, esp + 0x28);                                     // 422c60 mov edx, [esp + 0x28]
    eax = ld32(m, edx);                                            // 422c64 mov eax, [edx]
    eax = eax * 0x230u;                                            // 422c66 imul eax, eax, 0x230
    fl = f_cmp(ld32(m, edi), 0x0u);                                // 422c6c cmp [edi], 0
    ebx = eax + ebx + 0x39e0;                                      // 422c6f lea ebx, [eax + ebx + 0x39e0]
    ecx = esp + 0x1d0;                                             // 422c76 lea ecx, [esp + 0x1d0]
    edx = ebx + 0x1c0;                                             // 422c7d lea edx, [ebx + 0x1c0]
    k.push(ecx);                                                   // 422c83 push ecx
    ecx = edx;                                                     // 422c84 mov ecx, edx
    if (fl.e()) goto L_00422e5f;                                   // 422c86 je 0x422e5f
    k.call(0x0041f650);  // FUN_0041f650                           // 422c8c call 0x41f650
    x0 = ld(m, eax);                                               // 422c91 movaps xmm0, [eax]
    st(m, esp + 0x144, x0);                                        // 422c94 movaps [esp + 0x144], xmm0
    x0 = ld(m, eax + 0x10);                                        // 422c9c movaps xmm0, [eax + 0x10]
    st(m, esp + 0x154, x0);                                        // 422ca0 movaps [esp + 0x154], xmm0
    x0 = ld(m, eax + 0x20);                                        // 422ca8 movaps xmm0, [eax + 0x20]
    eax = esp + 0x1d4;                                             // 422cac lea eax, [esp + 0x1d4]
    esp = esp + 0x4u;                                              // 422cb3 add esp, 4
    ecx = ebx + 0x1f0;                                             // 422cb6 lea ecx, [ebx + 0x1f0]
    k.push(eax);                                                   // 422cbc push eax
    st(m, esp + 0x164, x0);                                        // 422cbd movaps [esp + 0x164], xmm0
    st32(m, esp + 0x24, ecx);                                      // 422cc5 mov [esp + 0x24], ecx
    k.call(0x0041f650);  // FUN_0041f650                           // 422cc9 call 0x41f650
    x0 = ld(m, eax);                                               // 422cce movaps xmm0, [eax]
    st(m, esp + 0x1a4, x0);                                        // 422cd1 movaps [esp + 0x1a4], xmm0
    x0 = ld(m, eax + 0x10);                                        // 422cd9 movaps xmm0, [eax + 0x10]
    st(m, esp + 0x1b4, x0);                                        // 422cdd movaps [esp + 0x1b4], xmm0
    x0 = ld(m, eax + 0x20);                                        // 422ce5 movaps xmm0, [eax + 0x20]
    ecx = edx;                                                     // 422ce9 mov ecx, edx
    esp = esp + 0x4u;                                              // 422ceb add esp, 4
    esi = ebx + 0x40;                                              // 422cee lea esi, [ebx + 0x40]
    edx = esp + 0x50;                                              // 422cf1 lea edx, [esp + 0x50]
    st(m, esp + 0x1c0, x0);                                        // 422cf5 movaps [esp + 0x1c0], xmm0
    k.call(0x0041f430);  // FUN_0041f430                           // 422cfd call 0x41f430
    ecx = ld32(m, esp + 0x20);                                     // 422d02 mov ecx, [esp + 0x20]
    esi = ebx + 0x70;                                              // 422d06 lea esi, [ebx + 0x70]
    edx = esp + 0xb0;                                              // 422d09 lea edx, [esp + 0xb0]
    k.call(0x0041f430);  // FUN_0041f430                           // 422d10 call 0x41f430
    x0 = ld(m, esp + 0x50);                                        // 422d15 movaps xmm0, [esp + 0x50]
    x1 = ld(m, esp + 0xb0);                                        // 422d1a movaps xmm1, [esp + 0xb0]
    x0 = addps(x0, x1);                                            // 422d22 addps xmm0, xmm1
    x1 = ld(m, esp + 0xc0);                                        // 422d25 movaps xmm1, [esp + 0xc0]
    st(m, esp + 0x80, x0);                                         // 422d2d movaps [esp + 0x80], xmm0
    x0 = ld(m, esp + 0x60);                                        // 422d35 movaps xmm0, [esp + 0x60]
    x0 = addps(x0, x1);                                            // 422d3a addps xmm0, xmm1
    x1 = ld(m, esp + 0x70);                                        // 422d3d movaps xmm1, [esp + 0x70]
    st(m, esp + 0x90, x0);                                         // 422d42 movaps [esp + 0x90], xmm0
    x0 = ld(m, esp + 0xd0);                                        // 422d4a movaps xmm0, [esp + 0xd0]
    x0 = addps(x0, x1);                                            // 422d52 addps xmm0, xmm1
    ecx = esp + 0x80;                                              // 422d55 lea ecx, [esp + 0x80]
    esi = esp + 0x140;                                             // 422d5c lea esi, [esp + 0x140]
    edx = esp + 0x50;                                              // 422d63 lea edx, [esp + 0x50]
    st(m, esp + 0xa0, x0);                                         // 422d67 movaps [esp + 0xa0], xmm0
    k.call(0x0041f430);  // FUN_0041f430                           // 422d6f call 0x41f430
    x0 = ld(m, edi + 0x40);                                        // 422d74 movaps xmm0, [edi + 0x40]
    x1 = ld(m, esp + 0x50);                                        // 422d78 movaps xmm1, [esp + 0x50]
    x0 = subps(x0, x1);                                            // 422d7d subps xmm0, xmm1
    x1 = ld(m, esp + 0x60);                                        // 422d80 movaps xmm1, [esp + 0x60]
    st(m, edi + 0x40, x0);                                         // 422d85 movaps [edi + 0x40], xmm0
    x0 = ld(m, edi + 0x50);                                        // 422d89 movaps xmm0, [edi + 0x50]
    x0 = subps(x0, x1);                                            // 422d8d subps xmm0, xmm1
    x1 = ld(m, esp + 0x70);                                        // 422d90 movaps xmm1, [esp + 0x70]
    st(m, edi + 0x50, x0);                                         // 422d95 movaps [edi + 0x50], xmm0
    x0 = ld(m, edi + 0x60);                                        // 422d99 movaps xmm0, [edi + 0x60]
    x0 = subps(x0, x1);                                            // 422d9d subps xmm0, xmm1
    esi = ebx + 0xa0;                                              // 422da0 lea esi, [ebx + 0xa0]
    ecx = ebx + 0x1c0;                                             // 422da6 lea ecx, [ebx + 0x1c0]
    st(m, edi + 0x60, x0);                                         // 422dac movaps [edi + 0x60], xmm0
    k.call(0x0041f430);  // FUN_0041f430                           // 422db0 call 0x41f430
    ecx = ld32(m, esp + 0x20);                                     // 422db5 mov ecx, [esp + 0x20]
    esi = ebx + 0xd0;                                              // 422db9 lea esi, [ebx + 0xd0]
    edx = esp + 0xb0;                                              // 422dbf lea edx, [esp + 0xb0]
    k.call(0x0041f430);  // FUN_0041f430                           // 422dc6 call 0x41f430
    x0 = ld(m, esp + 0x50);                                        // 422dcb movaps xmm0, [esp + 0x50]
    x1 = ld(m, esp + 0xb0);                                        // 422dd0 movaps xmm1, [esp + 0xb0]
    x0 = addps(x0, x1);                                            // 422dd8 addps xmm0, xmm1
    x1 = ld(m, esp + 0xc0);                                        // 422ddb movaps xmm1, [esp + 0xc0]
    st(m, esp + 0x80, x0);                                         // 422de3 movaps [esp + 0x80], xmm0
    x0 = ld(m, esp + 0x60);                                        // 422deb movaps xmm0, [esp + 0x60]
    x0 = addps(x0, x1);                                            // 422df0 addps xmm0, xmm1
    x1 = ld(m, esp + 0x70);                                        // 422df3 movaps xmm1, [esp + 0x70]
    st(m, esp + 0x90, x0);                                         // 422df8 movaps [esp + 0x90], xmm0
    x0 = ld(m, esp + 0xd0);                                        // 422e00 movaps xmm0, [esp + 0xd0]
    x0 = addps(x0, x1);                                            // 422e08 addps xmm0, xmm1
    ecx = esp + 0x80;                                              // 422e0b lea ecx, [esp + 0x80]
    esi = esp + 0x1a0;                                             // 422e12 lea esi, [esp + 0x1a0]
    edx = esp + 0x50;                                              // 422e19 lea edx, [esp + 0x50]
    st(m, esp + 0xa0, x0);                                         // 422e1d movaps [esp + 0xa0], xmm0
    k.call(0x0041f430);  // FUN_0041f430                           // 422e25 call 0x41f430
    x0 = ld(m, esp + 0x50);                                        // 422e2a movaps xmm0, [esp + 0x50]
    x1 = ld(m, edi + 0x40);                                        // 422e2f movaps xmm1, [edi + 0x40]
    x1 = subps(x1, x0);                                            // 422e33 subps xmm1, xmm0
    st(m, edi + 0x40, x1);                                         // 422e36 movaps [edi + 0x40], xmm1
    x0 = ld(m, edi + 0x50);                                        // 422e3a movaps xmm0, [edi + 0x50]
    x1 = ld(m, esp + 0x60);                                        // 422e3e movaps xmm1, [esp + 0x60]
    x0 = subps(x0, x1);                                            // 422e43 subps xmm0, xmm1
    x1 = ld(m, esp + 0x70);                                        // 422e46 movaps xmm1, [esp + 0x70]
    st(m, edi + 0x50, x0);                                         // 422e4b movaps [edi + 0x50], xmm0
    x0 = ld(m, edi + 0x60);                                        // 422e4f movaps xmm0, [edi + 0x60]
    x0 = subps(x0, x1);                                            // 422e53 subps xmm0, xmm1
    st(m, edi + 0x60, x0);                                         // 422e56 movaps [edi + 0x60], xmm0
    goto L_00423026;                                               // 422e5a jmp 0x423026
L_00422e5f:
    k.call(0x0041f650);  // FUN_0041f650                           // 422e5f call 0x41f650
    x0 = ld(m, eax);                                               // 422e64 movaps xmm0, [eax]
    st(m, esp + 0xb4, x0);                                         // 422e67 movaps [esp + 0xb4], xmm0
    x0 = ld(m, eax + 0x10);                                        // 422e6f movaps xmm0, [eax + 0x10]
    st(m, esp + 0xc4, x0);                                         // 422e73 movaps [esp + 0xc4], xmm0
    x0 = ld(m, eax + 0x20);                                        // 422e7b movaps xmm0, [eax + 0x20]
    eax = esp + 0x1d4;                                             // 422e7f lea eax, [esp + 0x1d4]
    esp = esp + 0x4u;                                              // 422e86 add esp, 4
    ecx = ebx + 0x1f0;                                             // 422e89 lea ecx, [ebx + 0x1f0]
    k.push(eax);                                                   // 422e8f push eax
    st(m, esp + 0xd4, x0);                                         // 422e90 movaps [esp + 0xd4], xmm0
    st32(m, esp + 0x24, ecx);                                      // 422e98 mov [esp + 0x24], ecx
    k.call(0x0041f650);  // FUN_0041f650                           // 422e9c call 0x41f650
    x0 = ld(m, eax);                                               // 422ea1 movaps xmm0, [eax]
    st(m, esp + 0x144, x0);                                        // 422ea4 movaps [esp + 0x144], xmm0
    x0 = ld(m, eax + 0x10);                                        // 422eac movaps xmm0, [eax + 0x10]
    st(m, esp + 0x154, x0);                                        // 422eb0 movaps [esp + 0x154], xmm0
    x0 = ld(m, eax + 0x20);                                        // 422eb8 movaps xmm0, [eax + 0x20]
    ebx = ebx + 0x40u;                                             // 422ebc add ebx, 0x40
    ecx = edx;                                                     // 422ebf mov ecx, edx
    esp = esp + 0x4u;                                              // 422ec1 add esp, 4
    esi = ebx;                                                     // 422ec4 mov esi, ebx
    edx = esp + 0x80;                                              // 422ec6 lea edx, [esp + 0x80]
    st(m, esp + 0x160, x0);                                        // 422ecd movaps [esp + 0x160], xmm0
    k.call(0x0041f430);  // FUN_0041f430                           // 422ed5 call 0x41f430
    ecx = edx;                                                     // 422eda mov ecx, edx
    esi = esp + 0xb0;                                              // 422edc lea esi, [esp + 0xb0]
    edx = esp + 0x50;                                              // 422ee3 lea edx, [esp + 0x50]
    k.call(0x0041f430);  // FUN_0041f430                           // 422ee7 call 0x41f430
    x0 = ld(m, edi + 0x40);                                        // 422eec movaps xmm0, [edi + 0x40]
    x1 = ld(m, esp + 0x50);                                        // 422ef0 movaps xmm1, [esp + 0x50]
    x0 = subps(x0, x1);                                            // 422ef5 subps xmm0, xmm1
    x1 = ld(m, esp + 0x60);                                        // 422ef8 movaps xmm1, [esp + 0x60]
    st(m, edi + 0x40, x0);                                         // 422efd movaps [edi + 0x40], xmm0
    x0 = ld(m, edi + 0x50);                                        // 422f01 movaps xmm0, [edi + 0x50]
    x0 = subps(x0, x1);                                            // 422f05 subps xmm0, xmm1
    x1 = ld(m, esp + 0x70);                                        // 422f08 movaps xmm1, [esp + 0x70]
    st(m, edi + 0x50, x0);                                         // 422f0d movaps [edi + 0x50], xmm0
    x0 = ld(m, edi + 0x60);                                        // 422f11 movaps xmm0, [edi + 0x60]
    x0 = subps(x0, x1);                                            // 422f15 subps xmm0, xmm1
    ecx = esp + 0x80;                                              // 422f18 lea ecx, [esp + 0x80]
    esi = esp + 0x140;                                             // 422f1f lea esi, [esp + 0x140]
    st(m, edi + 0x60, x0);                                         // 422f26 movaps [edi + 0x60], xmm0
    k.call(0x0041f430);  // FUN_0041f430                           // 422f2a call 0x41f430
    x1 = ld(m, edi + 0xa0);                                        // 422f2f movaps xmm1, [edi + 0xa0]
    x0 = ld(m, esp + 0x50);                                        // 422f36 movaps xmm0, [esp + 0x50]
    ecx = ld32(m, esp + 0x20);                                     // 422f3b mov ecx, [esp + 0x20]
    x1 = subps(x1, x0);                                            // 422f3f subps xmm1, xmm0
    st(m, edi + 0xa0, x1);                                         // 422f42 movaps [edi + 0xa0], xmm1
    x0 = ld(m, edi + 0xb0);                                        // 422f49 movaps xmm0, [edi + 0xb0]
    x1 = ld(m, esp + 0x60);                                        // 422f50 movaps xmm1, [esp + 0x60]
    x0 = subps(x0, x1);                                            // 422f55 subps xmm0, xmm1
    x1 = ld(m, esp + 0x70);                                        // 422f58 movaps xmm1, [esp + 0x70]
    st(m, edi + 0xb0, x0);                                         // 422f5d movaps [edi + 0xb0], xmm0
    x0 = ld(m, edi + 0xc0);                                        // 422f64 movaps xmm0, [edi + 0xc0]
    x0 = subps(x0, x1);                                            // 422f6b subps xmm0, xmm1
    esi = ebx;                                                     // 422f6e mov esi, ebx
    edx = esp + 0x80;                                              // 422f70 lea edx, [esp + 0x80]
    st(m, edi + 0xc0, x0);                                         // 422f77 movaps [edi + 0xc0], xmm0
    k.call(0x0041f430);  // FUN_0041f430                           // 422f7e call 0x41f430
    ecx = edx;                                                     // 422f83 mov ecx, edx
    esi = esp + 0xb0;                                              // 422f85 lea esi, [esp + 0xb0]
    edx = esp + 0x50;                                              // 422f8c lea edx, [esp + 0x50]
    k.call(0x0041f430);  // FUN_0041f430                           // 422f90 call 0x41f430
    x0 = ld(m, edi + 0x70);                                        // 422f95 movaps xmm0, [edi + 0x70]
    x1 = ld(m, esp + 0x50);                                        // 422f99 movaps xmm1, [esp + 0x50]
    x0 = subps(x0, x1);                                            // 422f9e subps xmm0, xmm1
    x1 = ld(m, esp + 0x60);                                        // 422fa1 movaps xmm1, [esp + 0x60]
    st(m, edi + 0x70, x0);                                         // 422fa6 movaps [edi + 0x70], xmm0
    x0 = ld(m, edi + 0x80);                                        // 422faa movaps xmm0, [edi + 0x80]
    x0 = subps(x0, x1);                                            // 422fb1 subps xmm0, xmm1
    x1 = ld(m, esp + 0x70);                                        // 422fb4 movaps xmm1, [esp + 0x70]
    st(m, edi + 0x80, x0);                                         // 422fb9 movaps [edi + 0x80], xmm0
    x0 = ld(m, edi + 0x90);                                        // 422fc0 movaps xmm0, [edi + 0x90]
    x0 = subps(x0, x1);                                            // 422fc7 subps xmm0, xmm1
    ecx = esp + 0x80;                                              // 422fca lea ecx, [esp + 0x80]
    esi = esp + 0x140;                                             // 422fd1 lea esi, [esp + 0x140]
    st(m, edi + 0x90, x0);                                         // 422fd8 movaps [edi + 0x90], xmm0
    k.call(0x0041f430);  // FUN_0041f430                           // 422fdf call 0x41f430
    x0 = ld(m, esp + 0x50);                                        // 422fe4 movaps xmm0, [esp + 0x50]
    x1 = ld(m, edi + 0xd0);                                        // 422fe9 movaps xmm1, [edi + 0xd0]
    x1 = subps(x1, x0);                                            // 422ff0 subps xmm1, xmm0
    st(m, edi + 0xd0, x1);                                         // 422ff3 movaps [edi + 0xd0], xmm1
    x0 = ld(m, edi + 0xe0);                                        // 422ffa movaps xmm0, [edi + 0xe0]
    x1 = ld(m, esp + 0x60);                                        // 423001 movaps xmm1, [esp + 0x60]
    x0 = subps(x0, x1);                                            // 423006 subps xmm0, xmm1
    x1 = ld(m, esp + 0x70);                                        // 423009 movaps xmm1, [esp + 0x70]
    st(m, edi + 0xe0, x0);                                         // 42300e movaps [edi + 0xe0], xmm0
    x0 = ld(m, edi + 0xf0);                                        // 423015 movaps xmm0, [edi + 0xf0]
    x0 = subps(x0, x1);                                            // 42301c subps xmm0, xmm1
    st(m, edi + 0xf0, x0);                                         // 42301f movaps [edi + 0xf0], xmm0
L_00423026:
    eax = ld32(m, esp + 0x1c);                                     // 423026 mov eax, [esp + 0x1c]
    st32(m, esp + 0x28, ld32(m, esp + 0x28) + 0x4u);               // 42302a add [esp + 0x28], 4
    ebx = ld32(m, esp + 0x18);                                     // 42302f mov ebx, [esp + 0x18]
    eax = eax + 1u;                                                // 423033 inc eax
    fl = f_cmp(eax, ld32(m, edi + 0x10));                          // 423034 cmp eax, [edi + 0x10]
    st32(m, esp + 0x1c, eax);                                      // 423037 mov [esp + 0x1c], eax
    if (fl.l()) goto L_00422c60;                                   // 42303b jl 0x422c60
L_00423041:
    fl = f_cmp(ld32(m, edi), 0x0u);                                // 423041 cmp [edi], 0
    esi = edi + 0x100;                                             // 423044 lea esi, [edi + 0x100]
    if (fl.e()) goto L_0042305f;                                   // 42304a je 0x42305f
    ecx = edi + 0x40;                                              // 42304c lea ecx, [edi + 0x40]
    k.push(ecx);                                                   // 42304f push ecx
    ecx = esi;                                                     // 423050 mov ecx, esi
    k.call(0x0041f6e0);  // FUN_0041f6e0                           // 423052 call 0x41f6e0
    esp = esp + 0x4u;                                              // 423057 add esp, 4
    goto L_004230df;                                               // 42305a jmp 0x4230df
L_0042305f:
    edx = edi + 0x40;                                              // 42305f lea edx, [edi + 0x40]
    st32(m, esp + 0x40, edx);                                      // 423062 mov [esp + 0x40], edx
    eax = edi + 0x70;                                              // 423066 lea eax, [edi + 0x70]
    st32(m, esp + 0x44, eax);                                      // 423069 mov [esp + 0x44], eax
    ecx = edi + 0xa0;                                              // 42306d lea ecx, [edi + 0xa0]
    edx = edi + 0xd0;                                              // 423073 lea edx, [edi + 0xd0]
    eax = edi + 0x130;                                             // 423079 lea eax, [edi + 0x130]
    st32(m, esp + 0x34, eax);                                      // 42307f mov [esp + 0x34], eax
    st32(m, esp + 0x48, ecx);                                      // 423083 mov [esp + 0x48], ecx
    st32(m, esp + 0x4c, edx);                                      // 423087 mov [esp + 0x4c], edx
    esp = esp - 0x10u;                                             // 42308b sub esp, 0x10
    eax = esp;                                                     // 42308e mov eax, esp
    ecx = edi + 0x160;                                             // 423090 lea ecx, [edi + 0x160]
    edx = edi + 0x190;                                             // 423096 lea edx, [edi + 0x190]
    st32(m, esp + 0x40, esi);                                      // 42309c mov [esp + 0x40], esi
    x0 = sd(ld64(m, esp + 0x40));                                  // 4230a0 movq xmm0, [esp + 0x40]
    st64(m, eax, x0.q(0));                                         // 4230a6 movq [eax], xmm0
    st32(m, esp + 0x48, ecx);                                      // 4230aa mov [esp + 0x48], ecx
    st32(m, esp + 0x4c, edx);                                      // 4230ae mov [esp + 0x4c], edx
    x0 = sd(ld64(m, esp + 0x48));                                  // 4230b2 movq xmm0, [esp + 0x48]
    st64(m, eax + 0x8, x0.q(0));                                   // 4230b8 movq [eax + 8], xmm0
    x0 = sd(ld64(m, esp + 0x50));                                  // 4230bd movq xmm0, [esp + 0x50]
    esp = esp - 0x10u;                                             // 4230c3 sub esp, 0x10
    eax = esp;                                                     // 4230c6 mov eax, esp
    st64(m, eax, x0.q(0));                                         // 4230c8 movq [eax], xmm0
    x0 = sd(ld64(m, esp + 0x68));                                  // 4230cc movq xmm0, [esp + 0x68]
    st64(m, eax + 0x8, x0.q(0));                                   // 4230d2 movq [eax + 8], xmm0
    k.call(0x0041f800);  // FUN_0041f800                           // 4230d7 call 0x41f800
    esp = esp + 0x20u;                                             // 4230dc add esp, 0x20
L_004230df:
    fl = f_cmp(ld32(m, edi), 0x0u);                                // 4230df cmp [edi], 0
    if (fl.e()) goto L_00423161;                                   // 4230e2 je 0x423161
    fl = f_cmp(ld32(m, edi + 0xc), 0xfffffffbu);                   // 4230e4 cmp [edi + 0xc], -5
    if (fl.e()) goto L_00423273;                                   // 4230e8 je 0x423273
    ebx = edi + 0x1c0;                                             // 4230ee lea ebx, [edi + 0x1c0]
    ecx = ebx;                                                     // 4230f4 mov ecx, ebx
    edx = esp + 0xe0;                                              // 4230f6 lea edx, [esp + 0xe0]
    k.call(0x0041f430);  // FUN_0041f430                           // 4230fd call 0x41f430
    edi = edi + 0x1f0u;                                            // 423102 add edi, 0x1f0
    ecx = edi;                                                     // 423108 mov ecx, edi
    edx = esp + 0x110;                                             // 42310a lea edx, [esp + 0x110]
    k.call(0x0041f430);  // FUN_0041f430                           // 423111 call 0x41f430
    x0 = ld(m, esp + 0xe0);                                        // 423116 movaps xmm0, [esp + 0xe0]
    st(m, ebx, x0);                                                // 42311e movaps [ebx], xmm0
    x0 = ld(m, esp + 0xf0);                                        // 423121 movaps xmm0, [esp + 0xf0]
    st(m, ebx + 0x10, x0);                                         // 423129 movaps [ebx + 0x10], xmm0
    x0 = ld(m, esp + 0x100);                                       // 42312d movaps xmm0, [esp + 0x100]
    st(m, ebx + 0x20, x0);                                         // 423135 movaps [ebx + 0x20], xmm0
    x0 = ld(m, esp + 0x110);                                       // 423139 movaps xmm0, [esp + 0x110]
    st(m, edi, x0);                                                // 423141 movaps [edi], xmm0
    x0 = ld(m, esp + 0x120);                                       // 423144 movaps xmm0, [esp + 0x120]
    st(m, edi + 0x10, x0);                                         // 42314c movaps [edi + 0x10], xmm0
    x0 = ld(m, esp + 0x130);                                       // 423150 movaps xmm0, [esp + 0x130]
    st(m, edi + 0x20, x0);                                         // 423158 movaps [edi + 0x20], xmm0
    goto L_0042326f;                                               // 42315c jmp 0x42326f
L_00423161:
    fl = f_cmp(ld32(m, edi + 0xc), 0xfffffffbu);                   // 423161 cmp [edi + 0xc], -5
    if (fl.e()) goto L_00423273;                                   // 423165 je 0x423273
    ebx = edi + 0x1c0;                                             // 42316b lea ebx, [edi + 0x1c0]
    ecx = ebx;                                                     // 423171 mov ecx, ebx
    edx = esp + 0xe0;                                              // 423173 lea edx, [esp + 0xe0]
    k.call(0x0041f430);  // FUN_0041f430                           // 42317a call 0x41f430
    ecx = edi + 0x1f0;                                             // 42317f lea ecx, [edi + 0x1f0]
    esi = edi + 0x130;                                             // 423185 lea esi, [edi + 0x130]
    edx = esp + 0x110;                                             // 42318b lea edx, [esp + 0x110]
    st32(m, esp + 0x1c, ecx);                                      // 423192 mov [esp + 0x1c], ecx
    k.call(0x0041f430);  // FUN_0041f430                           // 423196 call 0x41f430
    x0 = ld(m, esp + 0xe0);                                        // 42319b movaps xmm0, [esp + 0xe0]
    x7 = ld(m, esp + 0x110);                                       // 4231a3 movaps xmm7, [esp + 0x110]
    x1 = ld(m, esp + 0xf0);                                        // 4231ab movaps xmm1, [esp + 0xf0]
    x7 = addps(x7, x0);                                            // 4231b3 addps xmm7, xmm0
    x0 = ld(m, esp + 0x120);                                       // 4231b6 movaps xmm0, [esp + 0x120]
    x0 = addps(x0, x1);                                            // 4231be addps xmm0, xmm1
    x1 = ld(m, esp + 0x100);                                       // 4231c1 movaps xmm1, [esp + 0x100]
    st(m, esp + 0x180, x0);                                        // 4231c9 movaps [esp + 0x180], xmm0
    x0 = ld(m, esp + 0x130);                                       // 4231d1 movaps xmm0, [esp + 0x130]
    x0 = addps(x0, x1);                                            // 4231d9 addps xmm0, xmm1
    esi = edi + 0x160;                                             // 4231dc lea esi, [edi + 0x160]
    ecx = ebx;                                                     // 4231e2 mov ecx, ebx
    edx = esp + 0xe0;                                              // 4231e4 lea edx, [esp + 0xe0]
    st(m, esp + 0x190, x0);                                        // 4231eb movaps [esp + 0x190], xmm0
    k.call(0x0041f430);  // FUN_0041f430                           // 4231f3 call 0x41f430
    esi = edi + 0x190;                                             // 4231f8 lea esi, [edi + 0x190]
    edi = ld32(m, esp + 0x1c);                                     // 4231fe mov edi, [esp + 0x1c]
    ecx = edi;                                                     // 423202 mov ecx, edi
    edx = esp + 0x110;                                             // 423204 lea edx, [esp + 0x110]
    k.call(0x0041f430);  // FUN_0041f430                           // 42320b call 0x41f430
    x1 = ld(m, esp + 0xe0);                                        // 423210 movaps xmm1, [esp + 0xe0]
    x0 = ld(m, esp + 0x110);                                       // 423218 movaps xmm0, [esp + 0x110]
    x0 = addps(x0, x1);                                            // 423220 addps xmm0, xmm1
    x1 = ld(m, esp + 0xf0);                                        // 423223 movaps xmm1, [esp + 0xf0]
    st(m, edi, x0);                                                // 42322b movaps [edi], xmm0
    x0 = ld(m, esp + 0x120);                                       // 42322e movaps xmm0, [esp + 0x120]
    x0 = addps(x0, x1);                                            // 423236 addps xmm0, xmm1
    x1 = ld(m, esp + 0x100);                                       // 423239 movaps xmm1, [esp + 0x100]
    st(m, edi + 0x10, x0);                                         // 423241 movaps [edi + 0x10], xmm0
    x0 = ld(m, esp + 0x130);                                       // 423245 movaps xmm0, [esp + 0x130]
    x0 = addps(x0, x1);                                            // 42324d addps xmm0, xmm1
    st(m, edi + 0x20, x0);                                         // 423250 movaps [edi + 0x20], xmm0
    x0 = ld(m, esp + 0x180);                                       // 423254 movaps xmm0, [esp + 0x180]
    st(m, ebx, x7);                                                // 42325c movaps [ebx], xmm7
    st(m, ebx + 0x10, x0);                                         // 42325f movaps [ebx + 0x10], xmm0
    x0 = ld(m, esp + 0x190);                                       // 423263 movaps xmm0, [esp + 0x190]
    st(m, ebx + 0x20, x0);                                         // 42326b movaps [ebx + 0x20], xmm0
L_0042326f:
    ebx = ld32(m, esp + 0x18);                                     // 42326f mov ebx, [esp + 0x18]
L_00423273:
    eax = ld32(m, esp + 0x2c);                                     // 423273 mov eax, [esp + 0x2c]
    st32(m, esp + 0x24, ld32(m, esp + 0x24) + 0x4u);               // 423277 add [esp + 0x24], 4
    eax = eax + 1u;                                                // 42327c inc eax
    fl = f_cmp(eax, ld32(m, ebx + 0x9390));                        // 42327d cmp eax, [ebx + 0x9390]
    st32(m, esp + 0x2c, eax);                                      // 423283 mov [esp + 0x2c], eax
    if (fl.l()) goto L_00422c2d;                                   // 423287 jl 0x422c2d
L_0042328d:
    ecx = ld32(m, esp + 0x20c);                                    // 42328d mov ecx, [esp + 0x20c]
    edi = k.pop();                                                 // 423294 pop edi
    esi = k.pop();                                                 // 423295 pop esi
    ebx = k.pop();                                                 // 423296 pop ebx
    ecx = ecx ^ esp;                                               // 423297 xor ecx, esp
    k.call(0x008f9efe);  // __security_check_cookie                // 423299 call 0x8f9efe
    esp = ebp;                                                     // 42329e mov esp, ebp
    ebp = k.pop();                                                 // 4232a0 pop ebp
    return k.ret(4);                                               // 4232a1 ret 4
}

// 004232b0 FUN_004232b0: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x004232b0, Fn_004232b0, "math", 0) {  // outputs: what callers read (it calls other groups' code)
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 4232b0 push ebp
    ebp = esp;                                                     // 4232b1 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 4232b3 and esp, 0xfffffff0
    esp = esp - 0xf8u;                                             // 4232b6 sub esp, 0xf8
    eax = ld32(m, 0x9b45e4);                                       // 4232bc mov eax, [0x9b45e4]
    eax = eax ^ esp;                                               // 4232c1 xor eax, esp
    st32(m, esp + 0xf4, eax);                                      // 4232c3 mov [esp + 0xf4], eax
    fl = f_cmp(ld32(m, esi + 0x9390), 0x0u);                       // 4232ca cmp [esi + 0x9390], 0
    k.push(ebx);                                                   // 4232d1 push ebx
    k.push(edi);                                                   // 4232d2 push edi
    st32(m, esp + 0x2c, 0x0u);                                     // 4232d3 mov [esp + 0x2c], 0
    if (fl.le()) goto L_004235b7;                                  // 4232db jle 0x4235b7
    eax = esi + 0x9394;                                            // 4232e1 lea eax, [esi + 0x9394]
    st32(m, esp + 0x10, eax);                                      // 4232e7 mov [esp + 0x10], eax
L_004232eb:
    ecx = ld32(m, esp + 0x10);                                     // 4232eb mov ecx, [esp + 0x10]
    edx = ld32(m, ecx);                                            // 4232ef mov edx, [ecx]
    edx = edx * 0x230u;                                            // 4232f1 imul edx, edx, 0x230
    fl = f_cmp(ld32(m, edx + esi + 0x39e0), 0x0u);                 // 4232f7 cmp [edx + esi + 0x39e0], 0
    edi = edx + esi + 0x39e0;                                      // 4232ff lea edi, [edx + esi + 0x39e0]
    if (fl.e()) goto L_004233e4;                                   // 423306 je 0x4233e4
    eax = ld32(m, edi + 0x4);                                      // 42330c mov eax, [edi + 4]
    eax = eax * 0x170u;                                            // 42330f imul eax, eax, 0x170
    ecx = ld32(m, eax + esi + 0x1b50);                             // 423315 mov ecx, [eax + esi + 0x1b50]
    ebx = eax + esi + 0x1b30;                                      // 42331c lea ebx, [eax + esi + 0x1b30]
    eax = ld32(m, ebx);                                            // 423323 mov eax, [ebx]
    edx = eax;                                                     // 423325 mov edx, eax
    eax = eax * 0x70u;                                             // 423327 imul eax, eax, 0x70
    st32(m, esp + 0xc, ecx);                                       // 42332a mov [esp + 0xc], ecx
    edx = edx << 7;                                                // 42332e shl edx, 7
    x0 = ld(m, edx + esi + 0xb0);                                  // 423331 movaps xmm0, [edx + esi + 0xb0]
    x5 = ld(m, edx + esi + 0xc0);                                  // 423339 movaps xmm5, [edx + esi + 0xc0]
    edx = edx + esi;                                               // 423341 add edx, esi
    x5 = mulps(x5, x0);                                            // 423343 mulps xmm5, xmm0
    ecx = ecx << 7;                                                // 423346 shl ecx, 7
    x1 = ld(m, ecx + esi + 0xb0);                                  // 423349 movaps xmm1, [ecx + esi + 0xb0]
    x0 = ld(m, ecx + esi + 0xc0);                                  // 423351 movaps xmm0, [ecx + esi + 0xc0]
    ecx = ecx + esi;                                               // 423359 add ecx, esi
    x0 = mulps(x0, x1);                                            // 42335b mulps xmm0, xmm1
    x1 = ld(m, eax + esi + 0xb30);                                 // 42335e movaps xmm1, [eax + esi + 0xb30]
    eax = eax + esi;                                               // 423366 add eax, esi
    x5 = subps(x5, x0);                                            // 423368 subps xmm5, xmm0
    x0 = ld(m, eax + 0xb40);                                       // 42336b movaps xmm0, [eax + 0xb40]
    eax = ebx + 0x40;                                              // 423372 lea eax, [ebx + 0x40]
    x0 = mulps(x0, x1);                                            // 423375 mulps xmm0, xmm1
    k.push(eax);                                                   // 423378 push eax
    edx = esp + 0x44;                                              // 423379 lea edx, [esp + 0x44]
    ecx = esp + 0x54;                                              // 42337d lea ecx, [esp + 0x54]
    st(m, esp + 0x44, x0);                                         // 423381 movaps [esp + 0x44], xmm0
    k.call(0x0041f5c0);  // FUN_0041f5c0                           // 423386 call 0x41f5c0
    eax = ld32(m, esp + 0x10);                                     // 42338b mov eax, [esp + 0x10]
    x6 = ld(m, esp + 0x54);                                        // 42338f movaps xmm6, [esp + 0x54]
    eax = eax * 0x70u;                                             // 423394 imul eax, eax, 0x70
    x0 = ld(m, eax + esi + 0xb40);                                 // 423397 movaps xmm0, [eax + esi + 0xb40]
    x1 = ld(m, eax + esi + 0xb30);                                 // 42339f movaps xmm1, [eax + esi + 0xb30]
    eax = eax + esi;                                               // 4233a7 add eax, esi
    ecx = ebx + 0x70;                                              // 4233a9 lea ecx, [ebx + 0x70]
    esp = esp + 0x4u;                                              // 4233ac add esp, 4
    k.push(ecx);                                                   // 4233af push ecx
    x0 = mulps(x0, x1);                                            // 4233b0 mulps xmm0, xmm1
    x6 = addps(x6, x5);                                            // 4233b3 addps xmm6, xmm5
    ecx = esp + 0x54;                                              // 4233b6 lea ecx, [esp + 0x54]
    st(m, esp + 0x34, x6);                                         // 4233ba movaps [esp + 0x34], xmm6
    st(m, esp + 0x44, x0);                                         // 4233bf movaps [esp + 0x44], xmm0
    k.call(0x0041f5c0);  // FUN_0041f5c0                           // 4233c4 call 0x41f5c0
    x0 = ld(m, esp + 0x54);                                        // 4233c9 movaps xmm0, [esp + 0x54]
    x1 = ld(m, ebx + 0xa0);                                        // 4233ce movaps xmm1, [ebx + 0xa0]
    x0 = addps(x0, x6);                                            // 4233d5 addps xmm0, xmm6
    x0 = addps(x0, x1);                                            // 4233d8 addps xmm0, xmm1
    esp = esp + 0x4u;                                              // 4233db add esp, 4
    st(m, edi + 0x20, x0);                                         // 4233de movaps [edi + 0x20], xmm0
    goto L_004233fa;                                               // 4233e2 jmp 0x4233fa
L_004233e4:
    x0 = ld(m, 0xd61670);                                          // 4233e4 movaps xmm0, [0xd61670]
    st(m, edi + 0x20, x0);                                         // 4233eb movaps [edi + 0x20], xmm0
    x0 = ld(m, 0xd61670);                                          // 4233ef movaps xmm0, [0xd61670]
    st(m, edi + 0x30, x0);                                         // 4233f6 movaps [edi + 0x30], xmm0
L_004233fa:
    fl = f_cmp(ld32(m, edi + 0x10), 0x0u);                         // 4233fa cmp [edi + 0x10], 0
    st32(m, esp + 0xc, 0x0u);                                      // 4233fe mov [esp + 0xc], 0
    if (fl.le()) goto L_0042359d;                                  // 423406 jle 0x42359d
    edx = edi + 0x14;                                              // 42340c lea edx, [edi + 0x14]
    st32(m, esp + 0x28, edx);                                      // 42340f mov [esp + 0x28], edx
L_00423413:
    eax = ld32(m, esp + 0x28);                                     // 423413 mov eax, [esp + 0x28]
    ecx = ld32(m, eax);                                            // 423417 mov ecx, [eax]
    ecx = ecx * 0x230u;                                            // 423419 imul ecx, ecx, 0x230
    fl = f_cmp(ld32(m, edi), 0x0u);                                // 42341f cmp [edi], 0
    ebx = ecx + esi + 0x39e0;                                      // 423422 lea ebx, [ecx + esi + 0x39e0]
    edx = esp + 0xc0;                                              // 423429 lea edx, [esp + 0xc0]
    ecx = ebx + 0x1c0;                                             // 423430 lea ecx, [ebx + 0x1c0]
    k.push(edx);                                                   // 423436 push edx
    if (fl.e()) goto L_004234e0;                                   // 423437 je 0x4234e0
    k.call(0x0041f650);  // FUN_0041f650                           // 42343d call 0x41f650
    x0 = ld(m, eax);                                               // 423442 movaps xmm0, [eax]
    st(m, esp + 0x94, x0);                                         // 423445 movaps [esp + 0x94], xmm0
    x0 = ld(m, eax + 0x10);                                        // 42344d movaps xmm0, [eax + 0x10]
    st(m, esp + 0xa4, x0);                                         // 423451 movaps [esp + 0xa4], xmm0
    x0 = ld(m, eax + 0x20);                                        // 423459 movaps xmm0, [eax + 0x20]
    eax = esp + 0xc4;                                              // 42345d lea eax, [esp + 0xc4]
    esp = esp + 0x4u;                                              // 423464 add esp, 4
    ecx = ebx + 0x1f0;                                             // 423467 lea ecx, [ebx + 0x1f0]
    k.push(eax);                                                   // 42346d push eax
    st(m, esp + 0xb4, x0);                                         // 42346e movaps [esp + 0xb4], xmm0
    k.call(0x0041f650);  // FUN_0041f650                           // 423476 call 0x41f650
    x0 = ld(m, eax);                                               // 42347b movaps xmm0, [eax]
    st(m, esp + 0x64, x0);                                         // 42347e movaps [esp + 0x64], xmm0
    x0 = ld(m, eax + 0x10);                                        // 423483 movaps xmm0, [eax + 0x10]
    ecx = esp + 0x94;                                              // 423487 lea ecx, [esp + 0x94]
    esp = esp + 0x4u;                                              // 42348e add esp, 4
    st(m, esp + 0x70, x0);                                         // 423491 movaps [esp + 0x70], xmm0
    x0 = ld(m, eax + 0x20);                                        // 423496 movaps xmm0, [eax + 0x20]
    k.push(ecx);                                                   // 42349a push ecx
    edx = ebx + 0x20;                                              // 42349b lea edx, [ebx + 0x20]
    ecx = esp + 0x34;                                              // 42349e lea ecx, [esp + 0x34]
    st(m, esp + 0x84, x0);                                         // 4234a2 movaps [esp + 0x84], xmm0
    k.call(0x0041f5c0);  // FUN_0041f5c0                           // 4234aa call 0x41f5c0
    esp = esp + 0x4u;                                              // 4234af add esp, 4
    eax = esp + 0x60;                                              // 4234b2 lea eax, [esp + 0x60]
    edx = ebx + 0x30;                                              // 4234b6 lea edx, [ebx + 0x30]
    k.push(eax);                                                   // 4234b9 push eax
    ecx = esp + 0x44;                                              // 4234ba lea ecx, [esp + 0x44]
    k.call(0x0041f5c0);  // FUN_0041f5c0                           // 4234be call 0x41f5c0
    x1 = ld(m, esp + 0x34);                                        // 4234c3 movaps xmm1, [esp + 0x34]
    x0 = ld(m, esp + 0x44);                                        // 4234c8 movaps xmm0, [esp + 0x44]
    x0 = addps(x0, x1);                                            // 4234cd addps xmm0, xmm1
    x1 = ld(m, edi + 0x20);                                        // 4234d0 movaps xmm1, [edi + 0x20]
    x1 = subps(x1, x0);                                            // 4234d4 subps xmm1, xmm0
    st(m, edi + 0x20, x1);                                         // 4234d7 movaps [edi + 0x20], xmm1
    goto L_00423583;                                               // 4234db jmp 0x423583
L_004234e0:
    k.call(0x0041f650);  // FUN_0041f650                           // 4234e0 call 0x41f650
    x0 = ld(m, eax);                                               // 4234e5 movaps xmm0, [eax]
    st(m, esp + 0x94, x0);                                         // 4234e8 movaps [esp + 0x94], xmm0
    x0 = ld(m, eax + 0x10);                                        // 4234f0 movaps xmm0, [eax + 0x10]
    st(m, esp + 0xa4, x0);                                         // 4234f4 movaps [esp + 0xa4], xmm0
    x0 = ld(m, eax + 0x20);                                        // 4234fc movaps xmm0, [eax + 0x20]
    eax = esp + 0xc4;                                              // 423500 lea eax, [esp + 0xc4]
    esp = esp + 0x4u;                                              // 423507 add esp, 4
    ecx = ebx + 0x1f0;                                             // 42350a lea ecx, [ebx + 0x1f0]
    k.push(eax);                                                   // 423510 push eax
    st(m, esp + 0xb4, x0);                                         // 423511 movaps [esp + 0xb4], xmm0
    k.call(0x0041f650);  // FUN_0041f650                           // 423519 call 0x41f650
    x0 = ld(m, eax);                                               // 42351e movaps xmm0, [eax]
    st(m, esp + 0x64, x0);                                         // 423521 movaps [esp + 0x64], xmm0
    x0 = ld(m, eax + 0x10);                                        // 423526 movaps xmm0, [eax + 0x10]
    ecx = esp + 0x94;                                              // 42352a lea ecx, [esp + 0x94]
    esp = esp + 0x4u;                                              // 423531 add esp, 4
    st(m, esp + 0x70, x0);                                         // 423534 movaps [esp + 0x70], xmm0
    x0 = ld(m, eax + 0x20);                                        // 423539 movaps xmm0, [eax + 0x20]
    k.push(ecx);                                                   // 42353d push ecx
    edx = ebx + 0x20;                                              // 42353e lea edx, [ebx + 0x20]
    ecx = esp + 0x34;                                              // 423541 lea ecx, [esp + 0x34]
    st(m, esp + 0x84, x0);                                         // 423545 movaps [esp + 0x84], xmm0
    k.call(0x0041f5c0);  // FUN_0041f5c0                           // 42354d call 0x41f5c0
    esp = esp + 0x4u;                                              // 423552 add esp, 4
    eax = esp + 0x60;                                              // 423555 lea eax, [esp + 0x60]
    k.push(eax);                                                   // 423559 push eax
    ecx = esp + 0x44;                                              // 42355a lea ecx, [esp + 0x44]
    k.call(0x0041f5c0);  // FUN_0041f5c0                           // 42355e call 0x41f5c0
    x0 = ld(m, edi + 0x20);                                        // 423563 movaps xmm0, [edi + 0x20]
    x1 = ld(m, esp + 0x34);                                        // 423567 movaps xmm1, [esp + 0x34]
    x0 = subps(x0, x1);                                            // 42356c subps xmm0, xmm1
    st(m, edi + 0x20, x0);                                         // 42356f movaps [edi + 0x20], xmm0
    x1 = ld(m, edi + 0x30);                                        // 423573 movaps xmm1, [edi + 0x30]
    x0 = ld(m, esp + 0x44);                                        // 423577 movaps xmm0, [esp + 0x44]
    x1 = subps(x1, x0);                                            // 42357c subps xmm1, xmm0
    st(m, edi + 0x30, x1);                                         // 42357f movaps [edi + 0x30], xmm1
L_00423583:
    eax = ld32(m, esp + 0x10);                                     // 423583 mov eax, [esp + 0x10]
    st32(m, esp + 0x2c, ld32(m, esp + 0x2c) + 0x4u);               // 423587 add [esp + 0x2c], 4
    eax = eax + 1u;                                                // 42358c inc eax
    esp = esp + 0x4u;                                              // 42358d add esp, 4
    fl = f_cmp(eax, ld32(m, edi + 0x10));                          // 423590 cmp eax, [edi + 0x10]
    st32(m, esp + 0xc, eax);                                       // 423593 mov [esp + 0xc], eax
    if (fl.l()) goto L_00423413;                                   // 423597 jl 0x423413
L_0042359d:
    eax = ld32(m, esp + 0x2c);                                     // 42359d mov eax, [esp + 0x2c]
    st32(m, esp + 0x10, ld32(m, esp + 0x10) + 0x4u);               // 4235a1 add [esp + 0x10], 4
    eax = eax + 1u;                                                // 4235a6 inc eax
    fl = f_cmp(eax, ld32(m, esi + 0x9390));                        // 4235a7 cmp eax, [esi + 0x9390]
    st32(m, esp + 0x2c, eax);                                      // 4235ad mov [esp + 0x2c], eax
    if (fl.l()) goto L_004232eb;                                   // 4235b1 jl 0x4232eb
L_004235b7:
    eax = ld32(m, esi + 0x9390);                                   // 4235b7 mov eax, [esi + 0x9390]
    eax = eax - 1u;                                                // 4235bd dec eax
    fl = f_cmp(eax, 0xffffffffu);                                  // 4235be cmp eax, -1
    if (fl.le()) goto L_0042387f;                                  // 4235c1 jle 0x42387f
    ecx = esi + eax * 4 + 0x9394;                                  // 4235c7 lea ecx, [esi + eax*4 + 0x9394]
    eax = eax + 1u;                                                // 4235ce inc eax
    st32(m, esp + 0x28, ecx);                                      // 4235cf mov [esp + 0x28], ecx
    st32(m, esp + 0x2c, eax);                                      // 4235d3 mov [esp + 0x2c], eax
L_004235d7:
    edx = ld32(m, esp + 0x28);                                     // 4235d7 mov edx, [esp + 0x28]
    eax = ld32(m, edx);                                            // 4235db mov eax, [edx]
    eax = eax * 0x230u;                                            // 4235dd imul eax, eax, 0x230
    fl = f_cmp(ld32(m, eax + esi + 0x39e0), 0x0u);                 // 4235e3 cmp [eax + esi + 0x39e0], 0
    edi = eax + esi + 0x39e0;                                      // 4235eb lea edi, [eax + esi + 0x39e0]
    ebx = edi + 0x20;                                              // 4235f2 lea ebx, [edi + 0x20]
    st32(m, esp + 0x10, ebx);                                      // 4235f5 mov [esp + 0x10], ebx
    if (fl.e()) goto L_00423617;                                   // 4235f9 je 0x423617
    ecx = edi + 0x100;                                             // 4235fb lea ecx, [edi + 0x100]
    k.push(ecx);                                                   // 423601 push ecx
    edx = ebx;                                                     // 423602 mov edx, ebx
    ecx = esp + 0x34;                                              // 423604 lea ecx, [esp + 0x34]
    k.call(0x0041f5c0);  // FUN_0041f5c0                           // 423608 call 0x41f5c0
    x0 = ld(m, esp + 0x34);                                        // 42360d movaps xmm0, [esp + 0x34]
    st(m, ebx, x0);                                                // 423612 movaps [ebx], xmm0
    goto L_0042368f;                                               // 423615 jmp 0x42368f
L_00423617:
    edx = edi + 0x100;                                             // 423617 lea edx, [edi + 0x100]
    k.push(edx);                                                   // 42361d push edx
    edx = ebx;                                                     // 42361e mov edx, ebx
    ecx = esp + 0x34;                                              // 423620 lea ecx, [esp + 0x34]
    k.call(0x0041f5c0);  // FUN_0041f5c0                           // 423624 call 0x41f5c0
    eax = edi + 0x130;                                             // 423629 lea eax, [edi + 0x130]
    esp = esp + 0x4u;                                              // 42362f add esp, 4
    edx = edi + 0x30;                                              // 423632 lea edx, [edi + 0x30]
    k.push(eax);                                                   // 423635 push eax
    ecx = esp + 0x44;                                              // 423636 lea ecx, [esp + 0x44]
    k.call(0x0041f5c0);  // FUN_0041f5c0                           // 42363a call 0x41f5c0
    x0 = ld(m, esp + 0x34);                                        // 42363f movaps xmm0, [esp + 0x34]
    x5 = ld(m, esp + 0x44);                                        // 423644 movaps xmm5, [esp + 0x44]
    ecx = edi + 0x160;                                             // 423649 lea ecx, [edi + 0x160]
    esp = esp + 0x4u;                                              // 42364f add esp, 4
    k.push(ecx);                                                   // 423652 push ecx
    edx = ebx;                                                     // 423653 mov edx, ebx
    ecx = esp + 0x34;                                              // 423655 lea ecx, [esp + 0x34]
    x5 = addps(x5, x0);                                            // 423659 addps xmm5, xmm0
    k.call(0x0041f5c0);  // FUN_0041f5c0                           // 42365c call 0x41f5c0
    esp = esp + 0x4u;                                              // 423661 add esp, 4
    edx = edi + 0x190;                                             // 423664 lea edx, [edi + 0x190]
    k.push(edx);                                                   // 42366a push edx
    edx = edi + 0x30;                                              // 42366b lea edx, [edi + 0x30]
    ecx = esp + 0x44;                                              // 42366e lea ecx, [esp + 0x44]
    k.call(0x0041f5c0);  // FUN_0041f5c0                           // 423672 call 0x41f5c0
    x0 = ld(m, esp + 0x44);                                        // 423677 movaps xmm0, [esp + 0x44]
    x1 = ld(m, esp + 0x34);                                        // 42367c movaps xmm1, [esp + 0x34]
    st(m, ebx, x5);                                                // 423681 movaps [ebx], xmm5
    ebx = ld32(m, esp + 0x14);                                     // 423684 mov ebx, [esp + 0x14]
    x0 = addps(x0, x1);                                            // 423688 addps xmm0, xmm1
    st(m, edi + 0x30, x0);                                         // 42368b movaps [edi + 0x30], xmm0
L_0042368f:
    eax = ld32(m, edi + 0xc);                                      // 42368f mov eax, [edi + 0xc]
    esp = esp + 0x4u;                                              // 423692 add esp, 4
    fl = f_cmp(eax, 0xfffffffbu);                                  // 423695 cmp eax, -5
    if (fl.e()) goto L_00423739;                                   // 423698 je 0x423739
    eax = eax * 0x230u;                                            // 42369e imul eax, eax, 0x230
    fl = f_cmp(ld32(m, edi), 0x0u);                                // 4236a4 cmp [edi], 0
    eax = eax + esi + 0x39e0;                                      // 4236a7 lea eax, [eax + esi + 0x39e0]
    ecx = esp + 0x30;                                              // 4236ae lea ecx, [esp + 0x30]
    edx = eax + 0x20;                                              // 4236b2 lea edx, [eax + 0x20]
    if (fl.e()) goto L_004236f9;                                   // 4236b5 je 0x4236f9
    st32(m, esp + 0xc, eax);                                       // 4236b7 mov [esp + 0xc], eax
    eax = edi + 0x1c0;                                             // 4236bb lea eax, [edi + 0x1c0]
    k.push(eax);                                                   // 4236c1 push eax
    k.call(0x0041f5c0);  // FUN_0041f5c0                           // 4236c2 call 0x41f5c0
    edx = ld32(m, esp + 0x10);                                     // 4236c7 mov edx, [esp + 0x10]
    esp = esp + 0x4u;                                              // 4236cb add esp, 4
    ecx = edi + 0x1f0;                                             // 4236ce lea ecx, [edi + 0x1f0]
    k.push(ecx);                                                   // 4236d4 push ecx
    edx = edx + 0x30u;                                             // 4236d5 add edx, 0x30
    ecx = esp + 0x44;                                              // 4236d8 lea ecx, [esp + 0x44]
    k.call(0x0041f5c0);  // FUN_0041f5c0                           // 4236dc call 0x41f5c0
    x1 = ld(m, esp + 0x34);                                        // 4236e1 movaps xmm1, [esp + 0x34]
    x0 = ld(m, esp + 0x44);                                        // 4236e6 movaps xmm0, [esp + 0x44]
    x0 = addps(x0, x1);                                            // 4236eb addps xmm0, xmm1
    x1 = ld(m, ebx);                                               // 4236ee movaps xmm1, [ebx]
    x1 = subps(x1, x0);                                            // 4236f1 subps xmm1, xmm0
    st(m, ebx, x1);                                                // 4236f4 movaps [ebx], xmm1
    goto L_00423736;                                               // 4236f7 jmp 0x423736
L_004236f9:
    eax = edi + 0x1c0;                                             // 4236f9 lea eax, [edi + 0x1c0]
    k.push(eax);                                                   // 4236ff push eax
    k.call(0x0041f5c0);  // FUN_0041f5c0                           // 423700 call 0x41f5c0
    esp = esp + 0x4u;                                              // 423705 add esp, 4
    ecx = edi + 0x1f0;                                             // 423708 lea ecx, [edi + 0x1f0]
    k.push(ecx);                                                   // 42370e push ecx
    ecx = esp + 0x44;                                              // 42370f lea ecx, [esp + 0x44]
    k.call(0x0041f5c0);  // FUN_0041f5c0                           // 423713 call 0x41f5c0
    x0 = ld(m, ebx);                                               // 423718 movaps xmm0, [ebx]
    x1 = ld(m, esp + 0x34);                                        // 42371b movaps xmm1, [esp + 0x34]
    x0 = subps(x0, x1);                                            // 423720 subps xmm0, xmm1
    st(m, ebx, x0);                                                // 423723 movaps [ebx], xmm0
    x1 = ld(m, edi + 0x30);                                        // 423726 movaps xmm1, [edi + 0x30]
    x0 = ld(m, esp + 0x44);                                        // 42372a movaps xmm0, [esp + 0x44]
    x1 = subps(x1, x0);                                            // 42372f subps xmm1, xmm0
    st(m, edi + 0x30, x1);                                         // 423732 movaps [edi + 0x30], xmm1
L_00423736:
    esp = esp + 0x4u;                                              // 423736 add esp, 4
L_00423739:
    fl = f_cmp(ld32(m, edi), 0x0u);                                // 423739 cmp [edi], 0
    if (fl.e()) goto L_0042386f;                                   // 42373c je 0x42386f
    edx = ld32(m, edi + 0x4);                                      // 423742 mov edx, [edi + 4]
    ecx = ld32(m, esp + 0x10);                                     // 423745 mov ecx, [esp + 0x10]
    edx = edx * 0x170u;                                            // 423749 imul edx, edx, 0x170
    x5 = ld(m, ecx);                                               // 42374f movaps xmm5, [ecx]
    eax = ld32(m, edx + esi + 0x1b50);                             // 423752 mov eax, [edx + esi + 0x1b50]
    ebx = ld32(m, edx + esi + 0x1b30);                             // 423759 mov ebx, [edx + esi + 0x1b30]
    edi = edx + esi + 0x1b30;                                      // 423760 lea edi, [edx + esi + 0x1b30]
    edx = esp + 0xc0;                                              // 423767 lea edx, [esp + 0xc0]
    ecx = edi + 0x40;                                              // 42376e lea ecx, [edi + 0x40]
    k.push(edx);                                                   // 423771 push edx
    st32(m, esp + 0x10, eax);                                      // 423772 mov [esp + 0x10], eax
    st(m, esp + 0x54, x5);                                         // 423776 movaps [esp + 0x54], xmm5
    k.call(0x0041f650);  // FUN_0041f650                           // 42377b call 0x41f650
    x0 = ld(m, eax);                                               // 423780 movaps xmm0, [eax]
    st(m, esp + 0x64, x0);                                         // 423783 movaps [esp + 0x64], xmm0
    x0 = ld(m, eax + 0x10);                                        // 423788 movaps xmm0, [eax + 0x10]
    st(m, esp + 0x74, x0);                                         // 42378c movaps [esp + 0x74], xmm0
    x0 = ld(m, eax + 0x20);                                        // 423791 movaps xmm0, [eax + 0x20]
    eax = esp + 0x64;                                              // 423795 lea eax, [esp + 0x64]
    esp = esp + 0x4u;                                              // 423799 add esp, 4
    k.push(eax);                                                   // 42379c push eax
    edx = esp + 0x54;                                              // 42379d lea edx, [esp + 0x54]
    ecx = esp + 0x34;                                              // 4237a1 lea ecx, [esp + 0x34]
    st(m, esp + 0x84, x0);                                         // 4237a5 movaps [esp + 0x84], xmm0
    k.call(0x0041f5c0);  // FUN_0041f5c0                           // 4237ad call 0x41f5c0
    ecx = ebx;                                                     // 4237b2 mov ecx, ebx
    ebx = ebx * 0x70u;                                             // 4237b4 imul ebx, ebx, 0x70
    ecx = ecx << 7;                                                // 4237b7 shl ecx, 7
    x1 = ld(m, ecx + esi + 0xc0);                                  // 4237ba movaps xmm1, [ecx + esi + 0xc0]
    eax = ecx + esi + 0xc0;                                        // 4237c2 lea eax, [ecx + esi + 0xc0]
    x0 = x5;                                                       // 4237c9 movaps xmm0, xmm5
    x0 = addps(x0, x1);                                            // 4237cc addps xmm0, xmm1
    st(m, eax, x0);                                                // 4237cf movaps [eax], xmm0
    x1 = ld(m, ebx + esi + 0xb40);                                 // 4237d2 movaps xmm1, [ebx + esi + 0xb40]
    x0 = ld(m, esp + 0x34);                                        // 4237da movaps xmm0, [esp + 0x34]
    ebx = ebx + esi + 0xb40;                                       // 4237df lea ebx, [ebx + esi + 0xb40]
    esp = esp + 0x4u;                                              // 4237e6 add esp, 4
    edx = esp + 0xc0;                                              // 4237e9 lea edx, [esp + 0xc0]
    x0 = addps(x0, x1);                                            // 4237f0 addps xmm0, xmm1
    ecx = edi + 0x70;                                              // 4237f3 lea ecx, [edi + 0x70]
    k.push(edx);                                                   // 4237f6 push edx
    st(m, ebx, x0);                                                // 4237f7 movaps [ebx], xmm0
    k.call(0x0041f650);  // FUN_0041f650                           // 4237fa call 0x41f650
    x0 = ld(m, eax);                                               // 4237ff movaps xmm0, [eax]
    st(m, esp + 0x64, x0);                                         // 423802 movaps [esp + 0x64], xmm0
    x0 = ld(m, eax + 0x10);                                        // 423807 movaps xmm0, [eax + 0x10]
    st(m, esp + 0x74, x0);                                         // 42380b movaps [esp + 0x74], xmm0
    x0 = ld(m, eax + 0x20);                                        // 423810 movaps xmm0, [eax + 0x20]
    eax = esp + 0x64;                                              // 423814 lea eax, [esp + 0x64]
    esp = esp + 0x4u;                                              // 423818 add esp, 4
    k.push(eax);                                                   // 42381b push eax
    edx = esp + 0x54;                                              // 42381c lea edx, [esp + 0x54]
    ecx = esp + 0x34;                                              // 423820 lea ecx, [esp + 0x34]
    st(m, esp + 0x84, x0);                                         // 423824 movaps [esp + 0x84], xmm0
    k.call(0x0041f5c0);  // FUN_0041f5c0                           // 42382c call 0x41f5c0
    eax = ld32(m, esp + 0x10);                                     // 423831 mov eax, [esp + 0x10]
    x1 = ld(m, esp + 0x34);                                        // 423835 movaps xmm1, [esp + 0x34]
    ecx = eax;                                                     // 42383a mov ecx, eax
    ecx = ecx << 7;                                                // 42383c shl ecx, 7
    x0 = ld(m, ecx + esi + 0xc0);                                  // 42383f movaps xmm0, [ecx + esi + 0xc0]
    esp = esp + 0x4u;                                              // 423847 add esp, 4
    eax = eax * 0x70u;                                             // 42384a imul eax, eax, 0x70
    ecx = ecx + esi + 0xc0;                                        // 42384d lea ecx, [ecx + esi + 0xc0]
    x0 = subps(x0, x5);                                            // 423854 subps xmm0, xmm5
    st(m, ecx, x0);                                                // 423857 movaps [ecx], xmm0
    x0 = ld(m, eax + esi + 0xb40);                                 // 42385a movaps xmm0, [eax + esi + 0xb40]
    eax = eax + esi + 0xb40;                                       // 423862 lea eax, [eax + esi + 0xb40]
    x0 = addps(x0, x1);                                            // 423869 addps xmm0, xmm1
    st(m, eax, x0);                                                // 42386c movaps [eax], xmm0
L_0042386f:
    st32(m, esp + 0x28, ld32(m, esp + 0x28) - 0x4u);               // 42386f sub [esp + 0x28], 4
    { uint32_t a_ = ld32(m, esp + 0x2c), b_ = 0x1u, r_ = a_ - b_; fl = f_sub(a_, b_, r_); st32(m, esp + 0x2c, r_); } // 423874 sub [esp + 0x2c], 1
    if (fl.ne()) goto L_004235d7;                                  // 423879 jne 0x4235d7
L_0042387f:
    ecx = ld32(m, esp + 0xfc);                                     // 42387f mov ecx, [esp + 0xfc]
    edi = k.pop();                                                 // 423886 pop edi
    ebx = k.pop();                                                 // 423887 pop ebx
    ecx = ecx ^ esp;                                               // 423888 xor ecx, esp
    k.call(0x008f9efe);  // __security_check_cookie                // 42388a call 0x8f9efe
    esp = ebp;                                                     // 42388f mov esp, ebp
    ebp = k.pop();                                                 // 423891 pop ebp
    return k.ret(0);                                               // 423892 ret 
}

// 004238a0 FUN_004238a0: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x004238a0, Fn_004238a0, "math", 0) {  // outputs: what callers read (it calls other groups' code)
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 4238a0 push ebp
    ebp = esp;                                                     // 4238a1 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 4238a3 and esp, 0xfffffff0
    k.push(ecx);                                                   // 4238a6 push ecx
    k.push(ebx);                                                   // 4238a7 push ebx
    k.push(esi);                                                   // 4238a8 push esi
    esi = ecx;                                                     // 4238a9 mov esi, ecx
    eax = ld32(m, esi + 0x10);                                     // 4238ab mov eax, [esi + 0x10]
    ebx = 0u;                                                      // 4238ae xor ebx, ebx
    fl = f_cmp(eax, ebx);                                          // 4238b0 cmp eax, ebx
    x0 = cvtss2sd(x0, x0);                                         // 4238b2 cvtss2sd xmm0, xmm0
    x1 = cvtsi2ss(x1, int32_t(eax));                               // 4238b6 cvtsi2ss xmm1, eax
    x1 = cvtss2sd(x1, x1);                                         // 4238ba cvtss2sd xmm1, xmm1
    x0 = divsd(x0, x1);                                            // 4238be divsd xmm0, xmm1
    x0 = cvtsd2ss(x0, x0);                                         // 4238c2 cvtsd2ss xmm0, xmm0
    k.push(edi);                                                   // 4238c6 push edi
    st32(m, esi + 0x943c, ebx);                                    // 4238c7 mov [esi + 0x943c], ebx
    st32(m, esp + 0xc, x0.u[0]);                                   // 4238cd movss [esp + 0xc], xmm0
    if (fl.le()) goto L_00423904;                                  // 4238d3 jle 0x423904
L_004238d5:
    x0 = X::ss(ld32(m, esp + 0xc));                                // 4238d5 movss xmm0, [esp + 0xc]
    edi = esi;                                                     // 4238db mov edi, esi
    k.call(0x00422850);  // FUN_00422850                           // 4238dd call 0x422850
    k.push(esi);                                                   // 4238e2 push esi
    k.call(0x00421fa0);  // FUN_00421fa0                           // 4238e3 call 0x421fa0
    k.push(esi);                                                   // 4238e8 push esi
    k.call(0x00422960);  // FUN_00422960                           // 4238e9 call 0x422960
    k.call(0x004232b0);  // FUN_004232b0                           // 4238ee call 0x4232b0
    x0 = X::ss(ld32(m, esp + 0xc));                                // 4238f3 movss xmm0, [esp + 0xc]
    k.call(0x00423a50);  // FUN_00423a50                           // 4238f9 call 0x423a50
    ebx = ebx + 1u;                                                // 4238fe inc ebx
    fl = f_cmp(ebx, ld32(m, esi + 0x10));                          // 4238ff cmp ebx, [esi + 0x10]
    if (fl.l()) goto L_004238d5;                                   // 423902 jl 0x4238d5
L_00423904:
    ecx = 0u;                                                      // 423904 xor ecx, ecx
    fl = f_cmp(ld32(m, esi + 0x7c), ecx);                          // 423906 cmp [esi + 0x7c], ecx
    if (fl.le()) goto L_00423976;                                  // 423909 jle 0x423976
    x0 = X{};                                                      // 42390b xorps xmm0, xmm0
    x4 = x0;                                                       // 42390e movaps xmm4, xmm0
    eax = esi + 0xb00;                                             // 423911 lea eax, [esi + 0xb00]
L_00423917:
    x2 = ld(m, eax);                                               // 423917 movaps xmm2, [eax]
    x3 = x2;                                                       // 42391a movaps xmm3, xmm2
    x3 = mulps(x3, x2);                                            // 42391d mulps xmm3, xmm2
    x0 = x3;                                                       // 423920 movaps xmm0, xmm3
    x0 = shufps(x0, x3, 0xee);                                     // 423923 shufps xmm0, xmm3, 0xee
    x1 = x0;                                                       // 423927 movaps xmm1, xmm0
    x1 = addps(x1, x3);                                            // 42392a addps xmm1, xmm3
    x1 = shufps(x1, x1, 0x40);                                     // 42392d shufps xmm1, xmm1, 0x40
    x0 = shufps(x0, x1, 0xf0);                                     // 423931 shufps xmm0, xmm1, 0xf0
    x0 = addps(x0, x1);                                            // 423935 addps xmm0, xmm1
    x0 = shufps(x0, x0, 0xaa);                                     // 423938 shufps xmm0, xmm0, 0xaa
    x1 = x0;                                                       // 42393c movaps xmm1, xmm0
    x0 = X::v(0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000);     // 42393f movaps xmm0, [0x92dc20]
    x3 = sqrtps(x1);                                               // 423946 sqrtps xmm3, xmm1
    x0 = cmpneqps(x0, x1);                                         // 423949 cmpneqps xmm0, xmm1
    x1 = x4;                                                       // 42394d movaps xmm1, xmm4
    x1 = cmpneqps(x1, x3);                                         // 423950 cmpneqps xmm1, xmm3
    x2 = divps(x2, x3);                                            // 423954 divps xmm2, xmm3
    x1 = andps(x1, x2);                                            // 423957 andps xmm1, xmm2
    x2 = X::v(0x7fc00000, 0x7fc00000, 0x7fc00000, 0x7fc00000);     // 42395a movaps xmm2, [0x92dc30]
    x1 = andps(x1, x0);                                            // 423961 andps xmm1, xmm0
    x0 = andnps(x0, x2);                                           // 423964 andnps xmm0, xmm2
    x1 = orps(x1, x0);                                             // 423967 orps xmm1, xmm0
    st(m, eax, x1);                                                // 42396a movaps [eax], xmm1
    ecx = ecx + 1u;                                                // 42396d inc ecx
    eax = eax + 0x70u;                                             // 42396e add eax, 0x70
    fl = f_cmp(ecx, ld32(m, esi + 0x7c));                          // 423971 cmp ecx, [esi + 0x7c]
    if (fl.l()) goto L_00423917;                                   // 423974 jl 0x423917
L_00423976:
    edi = k.pop();                                                 // 423976 pop edi
    esi = k.pop();                                                 // 423977 pop esi
    ebx = k.pop();                                                 // 423978 pop ebx
    esp = ebp;                                                     // 423979 mov esp, ebp
    ebp = k.pop();                                                 // 42397b pop ebp
    return k.ret(0);                                               // 42397c ret 
}

// 00423980 FUN_00423980: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00423980, Fn_00423980, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 423980 push ebp
    ebp = esp;                                                     // 423981 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 423983 and esp, 0xfffffff0
    eax = ld32(m, ebp + 0x8);                                      // 423986 mov eax, [ebp + 8]
    esp = esp - 0x8u;                                              // 423989 sub esp, 8
    k.push(esi);                                                   // 42398c push esi
    k.push(edi);                                                   // 42398d push edi
    edi = 0u;                                                      // 42398e xor edi, edi
    fl = f_cmp(ld32(m, eax + 0x7c), edi);                          // 423990 cmp [eax + 0x7c], edi
    x0 = cvtss2sd(x0, x0);                                         // 423993 cvtss2sd xmm0, xmm0
    x0 = mulsd(x0, sd(0x4049000000000000ull));                     // 423997 mulsd xmm0, [0x977778]
    x1 = cvtsd2ss(x1, x0);                                         // 42399f cvtsd2ss xmm1, xmm0
    if (fl.le()) goto L_00423a3b;                                  // 4239a3 jle 0x423a3b
    x2 = sd(0x3ff0000000000000ull);                                // 4239a9 movsd xmm2, [0x977638]
    ecx = eax + 0x18;                                              // 4239b1 lea ecx, [eax + 0x18]
    esi = eax + 0xb10;                                             // 4239b4 lea esi, [eax + 0xb10]
    edx = eax + 0x90;                                              // 4239ba lea edx, [eax + 0x90]
L_004239c0:
    x0 = X::ss(ld32(m, ecx));                                      // 4239c0 movss xmm0, [ecx]
    x3 = X::ss(ld32(m, eax + 0x14));                               // 4239c4 movss xmm3, [eax + 0x14]
    x0 = cvtps2pd(x0);                                             // 4239c9 cvtps2pd xmm0, xmm0
    x3 = cvtps2pd(x3);                                             // 4239cc cvtps2pd xmm3, xmm3
    x0 = addsd(x0, x3);                                            // 4239cf addsd xmm0, xmm3
    x3 = cvtss2sd(x3, x1);                                         // 4239d3 cvtss2sd xmm3, xmm1
    x0 = mulsd(x0, x3);                                            // 4239d7 mulsd xmm0, xmm3
    x3 = x2;                                                       // 4239db movapd xmm3, xmm2
    x3 = subsd(x3, x0);                                            // 4239df subsd xmm3, xmm0
    x0 = cvtpd2ps(x3);                                             // 4239e3 cvtpd2ps xmm0, xmm3
    x3 = ld(m, edx);                                               // 4239e7 movaps xmm3, [edx]
    edi = edi + 1u;                                                // 4239ea inc edi
    ecx = ecx + 0x4u;                                              // 4239eb add ecx, 4
    edx = edx - 0xffffff80u;                                       // 4239ee sub edx, -0x80
    esi = esi + 0x70u;                                             // 4239f1 add esi, 0x70
    x0 = shufps(x0, x0, 0x00);                                     // 4239f4 shufps xmm0, xmm0, 0
    x0 = mulps(x0, x3);                                            // 4239f8 mulps xmm0, xmm3
    st(m, edx - 0x80, x0);                                         // 4239fb movaps [edx - 0x80], xmm0
    x0 = X::ss(ld32(m, ecx - 0x4));                                // 4239ff movss xmm0, [ecx - 4]
    x3 = X::ss(ld32(m, eax + 0x6c));                               // 423a04 movss xmm3, [eax + 0x6c]
    x0 = cvtps2pd(x0);                                             // 423a09 cvtps2pd xmm0, xmm0
    x3 = cvtps2pd(x3);                                             // 423a0c cvtps2pd xmm3, xmm3
    x0 = addsd(x0, x3);                                            // 423a0f addsd xmm0, xmm3
    x3 = cvtss2sd(x3, x1);                                         // 423a13 cvtss2sd xmm3, xmm1
    x0 = mulsd(x0, x3);                                            // 423a17 mulsd xmm0, xmm3
    x3 = x2;                                                       // 423a1b movapd xmm3, xmm2
    x3 = subsd(x3, x0);                                            // 423a1f subsd xmm3, xmm0
    x0 = cvtpd2ps(x3);                                             // 423a23 cvtpd2ps xmm0, xmm3
    x3 = ld(m, esi - 0x70);                                        // 423a27 movaps xmm3, [esi - 0x70]
    x0 = shufps(x0, x0, 0x00);                                     // 423a2b shufps xmm0, xmm0, 0
    x0 = mulps(x0, x3);                                            // 423a2f mulps xmm0, xmm3
    st(m, esi - 0x70, x0);                                         // 423a32 movaps [esi - 0x70], xmm0
    fl = f_cmp(edi, ld32(m, eax + 0x7c));                          // 423a36 cmp edi, [eax + 0x7c]
    if (fl.l()) goto L_004239c0;                                   // 423a39 jl 0x4239c0
L_00423a3b:
    edi = k.pop();                                                 // 423a3b pop edi
    esi = k.pop();                                                 // 423a3c pop esi
    esp = ebp;                                                     // 423a3d mov esp, ebp
    ebp = k.pop();                                                 // 423a3f pop ebp
    return k.ret(4);                                               // 423a40 ret 4
}

// 00423a50 FUN_00423a50: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00423a50, Fn_00423a50, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 423a50 push ebp
    ebp = esp;                                                     // 423a51 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 423a53 and esp, 0xfffffff0
    x4 = movss(x4, x0);                                            // 423a56 movss xmm4, xmm0
    x0 = cvtss2sd(x0, x0);                                         // 423a5a cvtss2sd xmm0, xmm0
    x0 = mulsd(x0, sd(0x3fe0000000000000ull));                     // 423a5e mulsd xmm0, [0x9774e8]
    x6 = cvtsd2ss(x6, x0);                                         // 423a66 cvtsd2ss xmm6, xmm0
    esp = esp - 0x2cu;                                             // 423a6a sub esp, 0x2c
    edx = 0u;                                                      // 423a6d xor edx, edx
    fl = f_cmp(ld32(m, esi + 0x7c), edx);                          // 423a6f cmp [esi + 0x7c], edx
    k.push(edi);                                                   // 423a72 push edi
    x4 = shufps(x4, x4, 0x00);                                     // 423a73 shufps xmm4, xmm4, 0
    x6 = shufps(x6, x6, 0x00);                                     // 423a77 shufps xmm6, xmm6, 0
    if (fl.le()) goto L_00423b70;                                  // 423a7b jle 0x423b70
    ecx = esi + 0xb10;                                             // 423a81 lea ecx, [esi + 0xb10]
    eax = esi + 0x80;                                              // 423a87 lea eax, [esi + 0x80]
    ecx = ecx;                                                     // 423a8d lea ecx, [ecx]
L_00423a90:
    x1 = ld(m, eax + 0x30);                                        // 423a90 movaps xmm1, [eax + 0x30]
    x0 = ld(m, eax + 0x40);                                        // 423a94 movaps xmm0, [eax + 0x40]
    x0 = mulps(x0, x1);                                            // 423a98 mulps xmm0, xmm1
    x1 = ld(m, eax + 0x10);                                        // 423a9b movaps xmm1, [eax + 0x10]
    x0 = mulps(x0, x4);                                            // 423a9f mulps xmm0, xmm4
    x0 = addps(x0, x1);                                            // 423aa2 addps xmm0, xmm1
    st(m, eax + 0x10, x0);                                         // 423aa5 movaps [eax + 0x10], xmm0
    x1 = ld(m, ecx + 0x20);                                        // 423aa9 movaps xmm1, [ecx + 0x20]
    x0 = ld(m, ecx + 0x30);                                        // 423aad movaps xmm0, [ecx + 0x30]
    x0 = mulps(x0, x1);                                            // 423ab1 mulps xmm0, xmm1
    x1 = ld(m, ecx);                                               // 423ab4 movaps xmm1, [ecx]
    x0 = mulps(x0, x4);                                            // 423ab7 mulps xmm0, xmm4
    x0 = addps(x0, x1);                                            // 423aba addps xmm0, xmm1
    st(m, ecx, x0);                                                // 423abd movaps [ecx], xmm0
    x0 = ld(m, eax + 0x10);                                        // 423ac0 movaps xmm0, [eax + 0x10]
    x1 = ld(m, eax);                                               // 423ac4 movaps xmm1, [eax]
    x0 = mulps(x0, x4);                                            // 423ac7 mulps xmm0, xmm4
    x0 = addps(x0, x1);                                            // 423aca addps xmm0, xmm1
    st(m, eax, x0);                                                // 423acd movaps [eax], xmm0
    x0 = ld(m, 0xd61660);                                          // 423ad0 movaps xmm0, [0xd61660]
    st(m, esp + 0x20, x0);                                         // 423ad7 movaps [esp + 0x20], xmm0
    x0 = ld(m, ecx);                                               // 423adc movaps xmm0, [ecx]
    edi = ld32(m, esp + 0x2c);                                     // 423adf mov edi, [esp + 0x2c]
    st(m, esp + 0x10, x0);                                         // 423ae3 movaps [esp + 0x10], xmm0
    x0 = ld(m, ecx - 0x10);                                        // 423ae8 movaps xmm0, [ecx - 0x10]
    st32(m, esp + 0x1c, edi);                                      // 423aec mov [esp + 0x1c], edi
    x5 = ld(m, esp + 0x10);                                        // 423af0 movaps xmm5, [esp + 0x10]
    x1 = x5;                                                       // 423af5 movaps xmm1, xmm5
    x1 = shufps(x1, x5, 0x1b);                                     // 423af8 shufps xmm1, xmm5, 0x1b
    x2 = x0;                                                       // 423afc movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0x00);                                     // 423aff shufps xmm2, xmm0, 0
    x2 = mulps(x2, x1);                                            // 423b03 mulps xmm2, xmm1
    x1 = shufps(x1, x1, 0xb1);                                     // 423b06 shufps xmm1, xmm1, 0xb1
    x7 = x1;                                                       // 423b0a movaps xmm7, xmm1
    x7 = shufps(x7, x1, 0x1b);                                     // 423b0d shufps xmm7, xmm1, 0x1b
    x3 = x0;                                                       // 423b11 movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0x55);                                     // 423b14 shufps xmm3, xmm0, 0x55
    x3 = mulps(x3, x1);                                            // 423b18 mulps xmm3, xmm1
    x1 = x0;                                                       // 423b1b movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0xaa);                                     // 423b1e shufps xmm1, xmm0, 0xaa
    x7 = mulps(x7, x1);                                            // 423b22 mulps xmm7, xmm1
    x1 = X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000);     // 423b25 movaps xmm1, [0x93a210]
    x7 = mulps(x7, x1);                                            // 423b2c mulps xmm7, xmm1
    x1 = X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000);     // 423b2f movaps xmm1, [0x93a200]
    x3 = mulps(x3, x1);                                            // 423b36 mulps xmm3, xmm1
    x7 = addps(x7, x3);                                            // 423b39 addps xmm7, xmm3
    x3 = X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000);     // 423b3c movaps xmm3, [0x93a1f0]
    x1 = x0;                                                       // 423b43 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0xff);                                     // 423b46 shufps xmm1, xmm0, 0xff
    x1 = mulps(x1, x5);                                            // 423b4a mulps xmm1, xmm5
    x2 = mulps(x2, x3);                                            // 423b4d mulps xmm2, xmm3
    x1 = addps(x1, x2);                                            // 423b50 addps xmm1, xmm2
    x7 = addps(x7, x1);                                            // 423b53 addps xmm7, xmm1
    x7 = mulps(x7, x6);                                            // 423b56 mulps xmm7, xmm6
    x7 = addps(x7, x0);                                            // 423b59 addps xmm7, xmm0
    st(m, ecx - 0x10, x7);                                         // 423b5c movaps [ecx - 0x10], xmm7
    edx = edx + 1u;                                                // 423b60 inc edx
    eax = eax - 0xffffff80u;                                       // 423b61 sub eax, -0x80
    ecx = ecx + 0x70u;                                             // 423b64 add ecx, 0x70
    fl = f_cmp(edx, ld32(m, esi + 0x7c));                          // 423b67 cmp edx, [esi + 0x7c]
    if (fl.l()) goto L_00423a90;                                   // 423b6a jl 0x423a90
L_00423b70:
    edi = k.pop();                                                 // 423b70 pop edi
    esp = ebp;                                                     // 423b71 mov esp, ebp
    ebp = k.pop();                                                 // 423b73 pop ebp
    return k.ret(0);                                               // 423b74 ret 
}

// 00423b80 FUN_00423b80: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00423b80, Fn_00423b80, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 423b80 push ebp
    ebp = esp;                                                     // 423b81 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 423b83 and esp, 0xfffffff0
    eax = ld32(m, ebp + 0x8);                                      // 423b86 mov eax, [ebp + 8]
    ecx = ld32(m, eax + 0x9390);                                   // 423b89 mov ecx, [eax + 0x9390]
    esp = esp - 0x54u;                                             // 423b8f sub esp, 0x54
    k.push(ebx);                                                   // 423b92 push ebx
    ecx = ecx - 1u;                                                // 423b93 dec ecx
    fl = f_cmp(ecx, 0xffffffffu);                                  // 423b94 cmp ecx, -1
    k.push(esi);                                                   // 423b97 push esi
    k.push(edi);                                                   // 423b98 push edi
    if (fl.le()) goto L_00423de5;                                  // 423b99 jle 0x423de5
    edi = eax + ecx * 4 + 0x9394;                                  // 423b9f lea edi, [eax + ecx*4 + 0x9394]
    ecx = ecx + 1u;                                                // 423ba6 inc ecx
    st32(m, esp + 0xc, ecx);                                       // 423ba7 mov [esp + 0xc], ecx
    goto L_00423bb0;                                               // 423bab jmp 0x423bb0
    ecx = ecx;                                                     // 423bad lea ecx, [ecx]
L_00423bb0:
    ecx = ld32(m, edi);                                            // 423bb0 mov ecx, [edi]
    ecx = ecx * 0x230u;                                            // 423bb2 imul ecx, ecx, 0x230
    fl = f_cmp(ld32(m, ecx + eax + 0x39e0), 0x0u);                 // 423bb8 cmp [ecx + eax + 0x39e0], 0
    ecx = ecx + eax + 0x39e0;                                      // 423bc0 lea ecx, [ecx + eax + 0x39e0]
    if (fl.e()) goto L_00423dd7;                                   // 423bc7 je 0x423dd7
    edx = ld32(m, ecx + 0x4);                                      // 423bcd mov edx, [ecx + 4]
    x6 = X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000);     // 423bd0 movaps xmm6, [0x92db80]
    edx = edx * 0x170u;                                            // 423bd7 imul edx, edx, 0x170
    x0 = ld(m, edx + eax + 0x1b40);                                // 423bdd movaps xmm0, [edx + eax + 0x1b40]
    x5 = X::v(0xbf800000, 0xbf800000, 0xbf800000, 0x3f800000);     // 423be5 movaps xmm5, [0x93a220]
    x0 = andps(x0, x6);                                            // 423bec andps xmm0, xmm6
    ecx = edx + eax + 0x1b30;                                      // 423bef lea ecx, [edx + eax + 0x1b30]
    edx = ld32(m, ecx);                                            // 423bf6 mov edx, [ecx]
    esi = edx;                                                     // 423bf8 mov esi, edx
    esi = esi * 0x70u;                                             // 423bfa imul esi, esi, 0x70
    x1 = ld(m, esi + eax + 0xb00);                                 // 423bfd movaps xmm1, [esi + eax + 0xb00]
    esi = ld32(m, ecx + 0x20);                                     // 423c05 mov esi, [ecx + 0x20]
    x3 = x0;                                                       // 423c08 movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0x00);                                     // 423c0b shufps xmm3, xmm0, 0
    x4 = x0;                                                       // 423c0f movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x55);                                     // 423c12 shufps xmm4, xmm0, 0x55
    x5 = mulps(x5, x1);                                            // 423c16 mulps xmm5, xmm1
    x7 = x0;                                                       // 423c19 movaps xmm7, xmm0
    x7 = shufps(x7, x0, 0xaa);                                     // 423c1c shufps xmm7, xmm0, 0xaa
    x2 = x5;                                                       // 423c20 movaps xmm2, xmm5
    x2 = shufps(x2, x5, 0x1b);                                     // 423c23 shufps xmm2, xmm5, 0x1b
    x3 = mulps(x3, x2);                                            // 423c27 mulps xmm3, xmm2
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 423c2a mulps xmm3, [0x93a1f0]
    x2 = shufps(x2, x2, 0xb1);                                     // 423c31 shufps xmm2, xmm2, 0xb1
    x4 = mulps(x4, x2);                                            // 423c35 mulps xmm4, xmm2
    x4 = mulps(x4, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 423c38 mulps xmm4, [0x93a200]
    x2 = shufps(x2, x2, 0x1b);                                     // 423c3f shufps xmm2, xmm2, 0x1b
    x2 = mulps(x2, x7);                                            // 423c43 mulps xmm2, xmm7
    x2 = mulps(x2, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 423c46 mulps xmm2, [0x93a210]
    x2 = addps(x2, x4);                                            // 423c4d addps xmm2, xmm4
    x4 = x0;                                                       // 423c50 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0xff);                                     // 423c53 shufps xmm4, xmm0, 0xff
    x4 = mulps(x4, x5);                                            // 423c57 mulps xmm4, xmm5
    x4 = addps(x4, x3);                                            // 423c5a addps xmm4, xmm3
    x2 = addps(x2, x4);                                            // 423c5d addps xmm2, xmm4
    ebx = esi;                                                     // 423c60 mov ebx, esi
    ebx = ebx * 0x70u;                                             // 423c62 imul ebx, ebx, 0x70
    x0 = x2;                                                       // 423c65 movaps xmm0, xmm2
    x0 = shufps(x0, x2, 0x1b);                                     // 423c68 shufps xmm0, xmm2, 0x1b
    x5 = x1;                                                       // 423c6c movaps xmm5, xmm1
    x5 = shufps(x5, x1, 0x00);                                     // 423c6f shufps xmm5, xmm1, 0
    x5 = mulps(x5, x0);                                            // 423c73 mulps xmm5, xmm0
    x0 = shufps(x0, x0, 0xb1);                                     // 423c76 shufps xmm0, xmm0, 0xb1
    st(m, esp + 0x30, x0);                                         // 423c7a movaps [esp + 0x30], xmm0
    x3 = x1;                                                       // 423c7f movaps xmm3, xmm1
    x3 = shufps(x3, x1, 0x55);                                     // 423c82 shufps xmm3, xmm1, 0x55
    x3 = mulps(x3, x0);                                            // 423c86 mulps xmm3, xmm0
    x0 = ld(m, ecx + 0x30);                                        // 423c89 movaps xmm0, [ecx + 0x30]
    x0 = andps(x0, x6);                                            // 423c8d andps xmm0, xmm6
    st(m, esp + 0x40, x3);                                         // 423c90 movaps [esp + 0x40], xmm3
    x3 = ld(m, ebx + eax + 0xb00);                                 // 423c95 movaps xmm3, [ebx + eax + 0xb00]
    x6 = x0;                                                       // 423c9d movaps xmm6, xmm0
    x6 = shufps(x6, x0, 0x00);                                     // 423ca0 shufps xmm6, xmm0, 0
    x4 = x3;                                                       // 423ca4 movaps xmm4, xmm3
    x4 = mulps(x4, X::v(0xbf800000, 0xbf800000, 0xbf800000, 0x3f800000)); // 423ca7 mulps xmm4, [0x93a220]
    st(m, esp + 0x20, x4);                                         // 423cae movaps [esp + 0x20], xmm4
    x4 = shufps(x4, x4, 0x1b);                                     // 423cb3 shufps xmm4, xmm4, 0x1b
    x6 = mulps(x6, x4);                                            // 423cb7 mulps xmm6, xmm4
    st(m, esp + 0x10, x6);                                         // 423cba movaps [esp + 0x10], xmm6
    x4 = shufps(x4, x4, 0xb1);                                     // 423cbf shufps xmm4, xmm4, 0xb1
    x6 = x0;                                                       // 423cc3 movaps xmm6, xmm0
    x6 = shufps(x6, x0, 0x55);                                     // 423cc6 shufps xmm6, xmm0, 0x55
    x6 = mulps(x6, x4);                                            // 423cca mulps xmm6, xmm4
    x6 = mulps(x6, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 423ccd mulps xmm6, [0x93a200]
    x7 = x0;                                                       // 423cd4 movaps xmm7, xmm0
    x7 = shufps(x7, x0, 0xaa);                                     // 423cd7 shufps xmm7, xmm0, 0xaa
    x4 = shufps(x4, x4, 0x1b);                                     // 423cdb shufps xmm4, xmm4, 0x1b
    x4 = mulps(x4, x7);                                            // 423cdf mulps xmm4, xmm7
    x4 = mulps(x4, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 423ce2 mulps xmm4, [0x93a210]
    x4 = addps(x4, x6);                                            // 423ce9 addps xmm4, xmm6
    x6 = x0;                                                       // 423cec movaps xmm6, xmm0
    x6 = shufps(x6, x0, 0xff);                                     // 423cef shufps xmm6, xmm0, 0xff
    x6 = mulps(x6, ld(m, esp + 0x20));                             // 423cf3 mulps xmm6, [esp + 0x20]
    x0 = ld(m, esp + 0x10);                                        // 423cf8 movaps xmm0, [esp + 0x10]
    x0 = mulps(x0, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 423cfd mulps xmm0, [0x93a1f0]
    x6 = addps(x6, x0);                                            // 423d04 addps xmm6, xmm0
    x4 = addps(x4, x6);                                            // 423d07 addps xmm4, xmm6
    x0 = x4;                                                       // 423d0a movaps xmm0, xmm4
    x6 = x3;                                                       // 423d0d movaps xmm6, xmm3
    x0 = shufps(x0, x4, 0x1b);                                     // 423d10 shufps xmm0, xmm4, 0x1b
    x6 = shufps(x6, x3, 0x00);                                     // 423d14 shufps xmm6, xmm3, 0
    x6 = mulps(x6, x0);                                            // 423d18 mulps xmm6, xmm0
    x0 = shufps(x0, x0, 0xb1);                                     // 423d1b shufps xmm0, xmm0, 0xb1
    st(m, esp + 0x10, x0);                                         // 423d1f movaps [esp + 0x10], xmm0
    st(m, esp + 0x50, x6);                                         // 423d24 movaps [esp + 0x50], xmm6
    x6 = x3;                                                       // 423d29 movaps xmm6, xmm3
    x6 = shufps(x6, x3, 0x55);                                     // 423d2c shufps xmm6, xmm3, 0x55
    x6 = mulps(x6, x0);                                            // 423d30 mulps xmm6, xmm0
    x0 = ld(m, esp + 0x30);                                        // 423d33 movaps xmm0, [esp + 0x30]
    x7 = x0;                                                       // 423d38 movaps xmm7, xmm0
    x7 = shufps(x7, x0, 0x1b);                                     // 423d3b shufps xmm7, xmm0, 0x1b
    x0 = x1;                                                       // 423d3f movaps xmm0, xmm1
    x0 = shufps(x0, x1, 0xaa);                                     // 423d42 shufps xmm0, xmm1, 0xaa
    x7 = mulps(x7, x0);                                            // 423d46 mulps xmm7, xmm0
    x7 = mulps(x7, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 423d49 mulps xmm7, [0x93a210]
    x0 = ld(m, esp + 0x40);                                        // 423d50 movaps xmm0, [esp + 0x40]
    st(m, esp + 0x20, x6);                                         // 423d55 movaps [esp + 0x20], xmm6
    x6 = X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000);     // 423d5a movaps xmm6, [0x93a200]
    x0 = mulps(x0, x6);                                            // 423d61 mulps xmm0, xmm6
    x7 = addps(x7, x0);                                            // 423d64 addps xmm7, xmm0
    x0 = x1;                                                       // 423d67 movaps xmm0, xmm1
    x0 = shufps(x0, x1, 0xff);                                     // 423d6a shufps xmm0, xmm1, 0xff
    x1 = X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000);     // 423d6e movaps xmm1, [0x93a1f0]
    x0 = mulps(x0, x2);                                            // 423d75 mulps xmm0, xmm2
    x5 = mulps(x5, x1);                                            // 423d78 mulps xmm5, xmm1
    x0 = addps(x0, x5);                                            // 423d7b addps xmm0, xmm5
    x7 = addps(x7, x0);                                            // 423d7e addps xmm7, xmm0
    edx = edx + 1u;                                                // 423d81 inc edx
    edx = edx << 7;                                                // 423d82 shl edx, 7
    x0 = ld(m, edx + eax);                                         // 423d85 movaps xmm0, [edx + eax]
    x7 = addps(x7, x0);                                            // 423d89 addps xmm7, xmm0
    x0 = ld(m, esp + 0x10);                                        // 423d8c movaps xmm0, [esp + 0x10]
    x2 = x0;                                                       // 423d91 movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0x1b);                                     // 423d94 shufps xmm2, xmm0, 0x1b
    x0 = x3;                                                       // 423d98 movaps xmm0, xmm3
    x0 = shufps(x0, x3, 0xaa);                                     // 423d9b shufps xmm0, xmm3, 0xaa
    x2 = mulps(x2, x0);                                            // 423d9f mulps xmm2, xmm0
    x2 = mulps(x2, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 423da2 mulps xmm2, [0x93a210]
    x0 = ld(m, esp + 0x20);                                        // 423da9 movaps xmm0, [esp + 0x20]
    x0 = mulps(x0, x6);                                            // 423dae mulps xmm0, xmm6
    x2 = addps(x2, x0);                                            // 423db1 addps xmm2, xmm0
    x0 = ld(m, esp + 0x50);                                        // 423db4 movaps xmm0, [esp + 0x50]
    x5 = x3;                                                       // 423db9 movaps xmm5, xmm3
    x5 = shufps(x5, x3, 0xff);                                     // 423dbc shufps xmm5, xmm3, 0xff
    x5 = mulps(x5, x4);                                            // 423dc0 mulps xmm5, xmm4
    x0 = mulps(x0, x1);                                            // 423dc3 mulps xmm0, xmm1
    esi = esi + 1u;                                                // 423dc6 inc esi
    x5 = addps(x5, x0);                                            // 423dc7 addps xmm5, xmm0
    x2 = addps(x2, x5);                                            // 423dca addps xmm2, xmm5
    esi = esi << 7;                                                // 423dcd shl esi, 7
    x7 = subps(x7, x2);                                            // 423dd0 subps xmm7, xmm2
    st(m, esi + eax, x7);                                          // 423dd3 movaps [esi + eax], xmm7
L_00423dd7:
    edi = edi - 0x4u;                                              // 423dd7 sub edi, 4
    { uint32_t a_ = ld32(m, esp + 0xc), b_ = 0x1u, r_ = a_ - b_; fl = f_sub(a_, b_, r_); st32(m, esp + 0xc, r_); } // 423dda sub [esp + 0xc], 1
    if (fl.ne()) goto L_00423bb0;                                  // 423ddf jne 0x423bb0
L_00423de5:
    edi = k.pop();                                                 // 423de5 pop edi
    esi = k.pop();                                                 // 423de6 pop esi
    ebx = k.pop();                                                 // 423de7 pop ebx
    esp = ebp;                                                     // 423de8 mov esp, ebp
    ebp = k.pop();                                                 // 423dea pop ebp
    return k.ret(4);                                               // 423deb ret 4
}

// 00423df0 FUN_00423df0: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00423df0, Fn_00423df0, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 423df0 push ebp
    ebp = esp;                                                     // 423df1 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 423df3 and esp, 0xfffffff0
    eax = ld32(m, ebp + 0x8);                                      // 423df6 mov eax, [ebp + 8]
    ecx = ld32(m, eax + 0x9390);                                   // 423df9 mov ecx, [eax + 0x9390]
    esp = esp - 0x54u;                                             // 423dff sub esp, 0x54
    k.push(ebx);                                                   // 423e02 push ebx
    ecx = ecx - 1u;                                                // 423e03 dec ecx
    fl = f_cmp(ecx, 0xffffffffu);                                  // 423e04 cmp ecx, -1
    k.push(esi);                                                   // 423e07 push esi
    k.push(edi);                                                   // 423e08 push edi
    if (fl.le()) goto L_004240cf;                                  // 423e09 jle 0x4240cf
    ebx = eax + ecx * 4 + 0x9394;                                  // 423e0f lea ebx, [eax + ecx*4 + 0x9394]
    ecx = ecx + 1u;                                                // 423e16 inc ecx
    st32(m, esp + 0xc, ecx);                                       // 423e17 mov [esp + 0xc], ecx
    goto L_00423e20;                                               // 423e1b jmp 0x423e20
    ecx = ecx;                                                     // 423e1d lea ecx, [ecx]
L_00423e20:
    ecx = ld32(m, ebx);                                            // 423e20 mov ecx, [ebx]
    ecx = ecx * 0x230u;                                            // 423e22 imul ecx, ecx, 0x230
    fl = f_cmp(ld32(m, ecx + eax + 0x39e0), 0x0u);                 // 423e28 cmp [ecx + eax + 0x39e0], 0
    ecx = ecx + eax + 0x39e0;                                      // 423e30 lea ecx, [ecx + eax + 0x39e0]
    if (fl.e()) goto L_004240c1;                                   // 423e37 je 0x4240c1
    edx = ld32(m, ecx + 0x4);                                      // 423e3d mov edx, [ecx + 4]
    edx = edx * 0x170u;                                            // 423e40 imul edx, edx, 0x170
    x1 = ld(m, edx + eax + 0x1b40);                                // 423e46 movaps xmm1, [edx + eax + 0x1b40]
    x3 = ld(m, edx + eax + 0x1b60);                                // 423e4e movaps xmm3, [edx + eax + 0x1b60]
    ecx = edx + eax + 0x1b30;                                      // 423e56 lea ecx, [edx + eax + 0x1b30]
    edx = ld32(m, ecx);                                            // 423e5d mov edx, [ecx]
    ecx = ld32(m, ecx + 0x20);                                     // 423e5f mov ecx, [ecx + 0x20]
    esi = edx;                                                     // 423e62 mov esi, edx
    esi = esi * 0x70u;                                             // 423e64 imul esi, esi, 0x70
    x0 = ld(m, esi + eax + 0xb10);                                 // 423e67 movaps xmm0, [esi + eax + 0xb10]
    x1 = shufps(x1, x1, 0xd2);                                     // 423e6f shufps xmm1, xmm1, 0xd2
    edi = ecx;                                                     // 423e73 mov edi, ecx
    edi = edi * 0x70u;                                             // 423e75 imul edi, edi, 0x70
    x4 = x1;                                                       // 423e78 movaps xmm4, xmm1
    x4 = shufps(x4, x1, 0xd2);                                     // 423e7b shufps xmm4, xmm1, 0xd2
    esi = esi + eax;                                               // 423e7f add esi, eax
    x0 = shufps(x0, x0, 0xc9);                                     // 423e81 shufps xmm0, xmm0, 0xc9
    x2 = x0;                                                       // 423e85 movaps xmm2, xmm0
    x0 = ld(m, edi + eax + 0xb10);                                 // 423e88 movaps xmm0, [edi + eax + 0xb10]
    x1 = mulps(x1, x2);                                            // 423e90 mulps xmm1, xmm2
    x5 = x2;                                                       // 423e93 movaps xmm5, xmm2
    x5 = shufps(x5, x2, 0xc9);                                     // 423e96 shufps xmm5, xmm2, 0xc9
    x2 = X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000);     // 423e9a movaps xmm2, [0x92db80]
    x4 = mulps(x4, x5);                                            // 423ea1 mulps xmm4, xmm5
    x1 = subps(x1, x4);                                            // 423ea4 subps xmm1, xmm4
    x1 = andps(x1, x2);                                            // 423ea7 andps xmm1, xmm2
    x1 = andps(x1, x2);                                            // 423eaa andps xmm1, xmm2
    x2 = x1;                                                       // 423ead movaps xmm2, xmm1
    edi = edi + eax;                                               // 423eb0 add edi, eax
    x0 = shufps(x0, x0, 0xc9);                                     // 423eb2 shufps xmm0, xmm0, 0xc9
    st(m, esp + 0x10, x0);                                         // 423eb6 movaps [esp + 0x10], xmm0
    x0 = ld(m, esi + 0xb00);                                       // 423ebb movaps xmm0, [esi + 0xb00]
    x4 = x2;                                                       // 423ec2 movaps xmm4, xmm2
    x4 = shufps(x4, x2, 0x00);                                     // 423ec5 shufps xmm4, xmm2, 0
    x6 = x0;                                                       // 423ec9 movaps xmm6, xmm0
    x6 = mulps(x6, X::v(0xbf800000, 0xbf800000, 0xbf800000, 0x3f800000)); // 423ecc mulps xmm6, [0x93a220]
    x1 = x6;                                                       // 423ed3 movaps xmm1, xmm6
    x1 = shufps(x1, x6, 0x1b);                                     // 423ed6 shufps xmm1, xmm6, 0x1b
    x4 = mulps(x4, x1);                                            // 423eda mulps xmm4, xmm1
    x4 = mulps(x4, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 423edd mulps xmm4, [0x93a1f0]
    x1 = shufps(x1, x1, 0xb1);                                     // 423ee4 shufps xmm1, xmm1, 0xb1
    x5 = x2;                                                       // 423ee8 movaps xmm5, xmm2
    x5 = shufps(x5, x2, 0x55);                                     // 423eeb shufps xmm5, xmm2, 0x55
    x5 = mulps(x5, x1);                                            // 423eef mulps xmm5, xmm1
    x5 = mulps(x5, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 423ef2 mulps xmm5, [0x93a200]
    x7 = x2;                                                       // 423ef9 movaps xmm7, xmm2
    x7 = shufps(x7, x2, 0xaa);                                     // 423efc shufps xmm7, xmm2, 0xaa
    x1 = shufps(x1, x1, 0x1b);                                     // 423f00 shufps xmm1, xmm1, 0x1b
    x1 = mulps(x1, x7);                                            // 423f04 mulps xmm1, xmm7
    x1 = mulps(x1, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 423f07 mulps xmm1, [0x93a210]
    x1 = addps(x1, x5);                                            // 423f0e addps xmm1, xmm5
    x5 = x2;                                                       // 423f11 movaps xmm5, xmm2
    x5 = shufps(x5, x2, 0xff);                                     // 423f14 shufps xmm5, xmm2, 0xff
    x5 = mulps(x5, x6);                                            // 423f18 mulps xmm5, xmm6
    x5 = addps(x5, x4);                                            // 423f1b addps xmm5, xmm4
    x1 = addps(x1, x5);                                            // 423f1e addps xmm1, xmm5
    x2 = x1;                                                       // 423f21 movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0x1b);                                     // 423f24 shufps xmm2, xmm1, 0x1b
    x5 = x0;                                                       // 423f28 movaps xmm5, xmm0
    x3 = shufps(x3, x3, 0xd2);                                     // 423f2b shufps xmm3, xmm3, 0xd2
    x5 = shufps(x5, x0, 0x00);                                     // 423f2f shufps xmm5, xmm0, 0
    x5 = mulps(x5, x2);                                            // 423f33 mulps xmm5, xmm2
    x2 = shufps(x2, x2, 0xb1);                                     // 423f36 shufps xmm2, xmm2, 0xb1
    x4 = x0;                                                       // 423f3a movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x55);                                     // 423f3d shufps xmm4, xmm0, 0x55
    x4 = mulps(x4, x2);                                            // 423f41 mulps xmm4, xmm2
    st(m, esp + 0x30, x4);                                         // 423f44 movaps [esp + 0x30], xmm4
    x4 = ld(m, esp + 0x10);                                        // 423f49 movaps xmm4, [esp + 0x10]
    x6 = x3;                                                       // 423f4e movaps xmm6, xmm3
    x6 = shufps(x6, x3, 0xd2);                                     // 423f51 shufps xmm6, xmm3, 0xd2
    x7 = x4;                                                       // 423f55 movaps xmm7, xmm4
    x3 = mulps(x3, x4);                                            // 423f58 mulps xmm3, xmm4
    x7 = shufps(x7, x4, 0xc9);                                     // 423f5b shufps xmm7, xmm4, 0xc9
    x4 = X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000);     // 423f5f movaps xmm4, [0x92db80]
    x6 = mulps(x6, x7);                                            // 423f66 mulps xmm6, xmm7
    x3 = subps(x3, x6);                                            // 423f69 subps xmm3, xmm6
    x3 = andps(x3, x4);                                            // 423f6c andps xmm3, xmm4
    st(m, esp + 0x40, x2);                                         // 423f6f movaps [esp + 0x40], xmm2
    x2 = ld(m, edi + 0xb00);                                       // 423f74 movaps xmm2, [edi + 0xb00]
    x3 = andps(x3, x4);                                            // 423f7b andps xmm3, xmm4
    x4 = x3;                                                       // 423f7e movaps xmm4, xmm3
    x6 = x4;                                                       // 423f81 movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0x00);                                     // 423f84 shufps xmm6, xmm4, 0
    x7 = x4;                                                       // 423f88 movaps xmm7, xmm4
    x7 = shufps(x7, x4, 0xaa);                                     // 423f8b shufps xmm7, xmm4, 0xaa
    x3 = x2;                                                       // 423f8f movaps xmm3, xmm2
    x3 = mulps(x3, X::v(0xbf800000, 0xbf800000, 0xbf800000, 0x3f800000)); // 423f92 mulps xmm3, [0x93a220]
    st(m, esp + 0x10, x3);                                         // 423f99 movaps [esp + 0x10], xmm3
    x3 = shufps(x3, x3, 0x1b);                                     // 423f9e shufps xmm3, xmm3, 0x1b
    x6 = mulps(x6, x3);                                            // 423fa2 mulps xmm6, xmm3
    st(m, esp + 0x20, x6);                                         // 423fa5 movaps [esp + 0x20], xmm6
    x6 = x4;                                                       // 423faa movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0x55);                                     // 423fad shufps xmm6, xmm4, 0x55
    x3 = shufps(x3, x3, 0xb1);                                     // 423fb1 shufps xmm3, xmm3, 0xb1
    x6 = mulps(x6, x3);                                            // 423fb5 mulps xmm6, xmm3
    x6 = mulps(x6, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 423fb8 mulps xmm6, [0x93a200]
    x3 = shufps(x3, x3, 0x1b);                                     // 423fbf shufps xmm3, xmm3, 0x1b
    x3 = mulps(x3, x7);                                            // 423fc3 mulps xmm3, xmm7
    x3 = mulps(x3, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 423fc6 mulps xmm3, [0x93a210]
    x3 = addps(x3, x6);                                            // 423fcd addps xmm3, xmm6
    x6 = x4;                                                       // 423fd0 movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0xff);                                     // 423fd3 shufps xmm6, xmm4, 0xff
    x6 = mulps(x6, ld(m, esp + 0x10));                             // 423fd7 mulps xmm6, [esp + 0x10]
    x4 = ld(m, esp + 0x20);                                        // 423fdc movaps xmm4, [esp + 0x20]
    x4 = mulps(x4, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 423fe1 mulps xmm4, [0x93a1f0]
    x6 = addps(x6, x4);                                            // 423fe8 addps xmm6, xmm4
    x3 = addps(x3, x6);                                            // 423feb addps xmm3, xmm6
    x4 = x3;                                                       // 423fee movaps xmm4, xmm3
    x4 = shufps(x4, x3, 0x1b);                                     // 423ff1 shufps xmm4, xmm3, 0x1b
    x6 = x2;                                                       // 423ff5 movaps xmm6, xmm2
    x6 = shufps(x6, x2, 0x00);                                     // 423ff8 shufps xmm6, xmm2, 0
    x6 = mulps(x6, x4);                                            // 423ffc mulps xmm6, xmm4
    x4 = shufps(x4, x4, 0xb1);                                     // 423fff shufps xmm4, xmm4, 0xb1
    st(m, esp + 0x20, x4);                                         // 424003 movaps [esp + 0x20], xmm4
    st(m, esp + 0x50, x6);                                         // 424008 movaps [esp + 0x50], xmm6
    x6 = x2;                                                       // 42400d movaps xmm6, xmm2
    x6 = shufps(x6, x2, 0x55);                                     // 424010 shufps xmm6, xmm2, 0x55
    x6 = mulps(x6, x4);                                            // 424014 mulps xmm6, xmm4
    x4 = ld(m, esp + 0x40);                                        // 424017 movaps xmm4, [esp + 0x40]
    x7 = x4;                                                       // 42401c movaps xmm7, xmm4
    x7 = shufps(x7, x4, 0x1b);                                     // 42401f shufps xmm7, xmm4, 0x1b
    x4 = x0;                                                       // 424023 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0xaa);                                     // 424026 shufps xmm4, xmm0, 0xaa
    x7 = mulps(x7, x4);                                            // 42402a mulps xmm7, xmm4
    x7 = mulps(x7, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 42402d mulps xmm7, [0x93a210]
    x4 = ld(m, esp + 0x30);                                        // 424034 movaps xmm4, [esp + 0x30]
    st(m, esp + 0x10, x6);                                         // 424039 movaps [esp + 0x10], xmm6
    x6 = X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000);     // 42403e movaps xmm6, [0x93a200]
    x4 = mulps(x4, x6);                                            // 424045 mulps xmm4, xmm6
    x7 = addps(x7, x4);                                            // 424048 addps xmm7, xmm4
    x4 = x0;                                                       // 42404b movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0xff);                                     // 42404e shufps xmm4, xmm0, 0xff
    x4 = mulps(x4, x1);                                            // 424052 mulps xmm4, xmm1
    x1 = X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000);     // 424055 movaps xmm1, [0x93a1f0]
    x5 = mulps(x5, x1);                                            // 42405c mulps xmm5, xmm1
    x4 = addps(x4, x5);                                            // 42405f addps xmm4, xmm5
    x7 = addps(x7, x4);                                            // 424062 addps xmm7, xmm4
    edx = edx << 7;                                                // 424065 shl edx, 7
    x0 = ld(m, edx + eax + 0x90);                                  // 424068 movaps xmm0, [edx + eax + 0x90]
    x7 = addps(x7, x0);                                            // 424070 addps xmm7, xmm0
    x0 = ld(m, esp + 0x20);                                        // 424073 movaps xmm0, [esp + 0x20]
    x4 = x0;                                                       // 424078 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x1b);                                     // 42407b shufps xmm4, xmm0, 0x1b
    x0 = x2;                                                       // 42407f movaps xmm0, xmm2
    x0 = shufps(x0, x2, 0xaa);                                     // 424082 shufps xmm0, xmm2, 0xaa
    x4 = mulps(x4, x0);                                            // 424086 mulps xmm4, xmm0
    x4 = mulps(x4, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 424089 mulps xmm4, [0x93a210]
    x0 = ld(m, esp + 0x10);                                        // 424090 movaps xmm0, [esp + 0x10]
    x0 = mulps(x0, x6);                                            // 424095 mulps xmm0, xmm6
    x4 = addps(x4, x0);                                            // 424098 addps xmm4, xmm0
    x0 = ld(m, esp + 0x50);                                        // 42409b movaps xmm0, [esp + 0x50]
    x5 = x2;                                                       // 4240a0 movaps xmm5, xmm2
    x5 = shufps(x5, x2, 0xff);                                     // 4240a3 shufps xmm5, xmm2, 0xff
    x5 = mulps(x5, x3);                                            // 4240a7 mulps xmm5, xmm3
    x0 = mulps(x0, x1);                                            // 4240aa mulps xmm0, xmm1
    x5 = addps(x5, x0);                                            // 4240ad addps xmm5, xmm0
    x4 = addps(x4, x5);                                            // 4240b0 addps xmm4, xmm5
    ecx = ecx << 7;                                                // 4240b3 shl ecx, 7
    x7 = subps(x7, x4);                                            // 4240b6 subps xmm7, xmm4
    st(m, ecx + eax + 0x90, x7);                                   // 4240b9 movaps [ecx + eax + 0x90], xmm7
L_004240c1:
    ebx = ebx - 0x4u;                                              // 4240c1 sub ebx, 4
    { uint32_t a_ = ld32(m, esp + 0xc), b_ = 0x1u, r_ = a_ - b_; fl = f_sub(a_, b_, r_); st32(m, esp + 0xc, r_); } // 4240c4 sub [esp + 0xc], 1
    if (fl.ne()) goto L_00423e20;                                  // 4240c9 jne 0x423e20
L_004240cf:
    edi = k.pop();                                                 // 4240cf pop edi
    esi = k.pop();                                                 // 4240d0 pop esi
    ebx = k.pop();                                                 // 4240d1 pop ebx
    esp = ebp;                                                     // 4240d2 mov esp, ebp
    ebp = k.pop();                                                 // 4240d4 pop ebp
    return k.ret(4);                                               // 4240d5 ret 4
}

// 004240e0 FUN_004240e0: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x004240e0, Fn_004240e0, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 4240e0 push ebp
    ebp = esp;                                                     // 4240e1 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 4240e3 and esp, 0xfffffff0
    eax = ld32(m, 0x9cb860);                                       // 4240e6 mov eax, [0x9cb860]
    ecx = ecx * 0x70u;                                             // 4240eb imul ecx, ecx, 0x70
    eax = eax * 0x41c64e6du;                                       // 4240ee imul eax, eax, 0x41c64e6d
    x2 = sd(0x3f00000000000000ull);                                // 4240f4 movsd xmm2, [0x9776a0]
    x3 = sd(0x3fe0000000000000ull);                                // 4240fc movsd xmm3, [0x9774e8]
    edx = ld32(m, 0x9cb864);                                       // 424104 mov edx, [0x9cb864]
    eax = eax + 0x3039u;                                           // 42410a add eax, 0x3039
    esp = esp - 0xcu;                                              // 42410f sub esp, 0xc
    k.push(edi);                                                   // 424112 push edi
    edi = eax;                                                     // 424113 mov edi, eax
    eax = eax * 0x41c64e6du;                                       // 424115 imul eax, eax, 0x41c64e6d
    edi = edi >> 16;                                               // 42411b shr edi, 0x10
    edi = edi & 0x7fffu;                                           // 42411e and edi, 0x7fff
    x0 = cvtsi2ss(x0, int32_t(edi));                               // 424124 cvtsi2ss xmm0, edi
    x0 = cvtss2sd(x0, x0);                                         // 424128 cvtss2sd xmm0, xmm0
    x0 = mulsd(x0, x2);                                            // 42412c mulsd xmm0, xmm2
    x1 = cvtsd2ss(x1, x0);                                         // 424130 cvtsd2ss xmm1, xmm0
    eax = eax + 0x3039u;                                           // 424134 add eax, 0x3039
    edi = eax;                                                     // 424139 mov edi, eax
    eax = eax * 0x41c64e6du;                                       // 42413b imul eax, eax, 0x41c64e6d
    x1 = cvtss2sd(x1, x1);                                         // 424141 cvtss2sd xmm1, xmm1
    x1 = subsd(x1, x3);                                            // 424145 subsd xmm1, xmm3
    edi = edi >> 16;                                               // 424149 shr edi, 0x10
    x1 = cvtsd2ss(x1, x1);                                         // 42414c cvtsd2ss xmm1, xmm1
    edi = edi & 0x7fffu;                                           // 424150 and edi, 0x7fff
    x0 = cvtsi2ss(x0, int32_t(edi));                               // 424156 cvtsi2ss xmm0, edi
    x0 = cvtss2sd(x0, x0);                                         // 42415a cvtss2sd xmm0, xmm0
    x0 = mulsd(x0, x2);                                            // 42415e mulsd xmm0, xmm2
    x0 = cvtsd2ss(x0, x0);                                         // 424162 cvtsd2ss xmm0, xmm0
    x0 = cvtss2sd(x0, x0);                                         // 424166 cvtss2sd xmm0, xmm0
    x0 = subsd(x0, x3);                                            // 42416a subsd xmm0, xmm3
    x0 = cvtsd2ss(x0, x0);                                         // 42416e cvtsd2ss xmm0, xmm0
    x1 = shufps(x1, x1, 0x00);                                     // 424172 shufps xmm1, xmm1, 0
    edx = edx + 1u;                                                // 424176 inc edx
    eax = eax + 0x3039u;                                           // 424177 add eax, 0x3039
    st32(m, 0x9cb860, eax);                                        // 42417c mov [0x9cb860], eax
    edx = edx + 1u;                                                // 424181 inc edx
    x0 = shufps(x0, x0, 0x00);                                     // 424182 shufps xmm0, xmm0, 0
    x0 = shufps(x0, x1, 0xee);                                     // 424186 shufps xmm0, xmm1, 0xee
    edx = edx + 1u;                                                // 42418a inc edx
    eax = eax >> 16;                                               // 42418b shr eax, 0x10
    eax = eax & 0x7fffu;                                           // 42418e and eax, 0x7fff
    x1 = cvtsi2ss(x1, int32_t(eax));                               // 424193 cvtsi2ss xmm1, eax
    x1 = cvtss2sd(x1, x1);                                         // 424197 cvtss2sd xmm1, xmm1
    x1 = mulsd(x1, x2);                                            // 42419b mulsd xmm1, xmm2
    x1 = cvtsd2ss(x1, x1);                                         // 42419f cvtsd2ss xmm1, xmm1
    x1 = cvtss2sd(x1, x1);                                         // 4241a3 cvtss2sd xmm1, xmm1
    x1 = subsd(x1, x3);                                            // 4241a7 subsd xmm1, xmm3
    x1 = cvtsd2ss(x1, x1);                                         // 4241ab cvtsd2ss xmm1, xmm1
    ecx = ecx + esi + 0xb10;                                       // 4241af lea ecx, [ecx + esi + 0xb10]
    st32(m, 0x9cb864, edx);                                        // 4241b6 mov [0x9cb864], edx
    edi = k.pop();                                                 // 4241bc pop edi
    x1 = shufps(x1, x1, 0x00);                                     // 4241bd shufps xmm1, xmm1, 0
    x0 = movss(x0, x1);                                            // 4241c1 movss xmm0, xmm1
    x1 = x0;                                                       // 4241c5 movaps xmm1, xmm0
    x0 = X::ss(ld32(m, ebp + 0x8));                                // 4241c8 movss xmm0, [ebp + 8]
    x0 = shufps(x0, x0, 0x00);                                     // 4241cd shufps xmm0, xmm0, 0
    x1 = mulps(x1, x0);                                            // 4241d1 mulps xmm1, xmm0
    x0 = ld(m, ecx);                                               // 4241d4 movaps xmm0, [ecx]
    x1 = addps(x1, x0);                                            // 4241d7 addps xmm1, xmm0
    st(m, ecx, x1);                                                // 4241da movaps [ecx], xmm1
    esp = ebp;                                                     // 4241dd mov esp, ebp
    ebp = k.pop();                                                 // 4241df pop ebp
    return k.ret(4);                                               // 4241e0 ret 4
}

// 004241f0 FUN_004241f0: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x004241f0, Fn_004241f0, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 4241f0 push ebp
    ebp = esp;                                                     // 4241f1 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 4241f3 and esp, 0xfffffff0
    eax = ld32(m, ebp + 0x8);                                      // 4241f6 mov eax, [ebp + 8]
    x1 = X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000);     // 4241f9 movaps xmm1, [0x92db80]
    eax = eax * 0x70u;                                             // 424200 imul eax, eax, 0x70
    x3 = ld(m, eax + edx + 0xb00);                                 // 424203 movaps xmm3, [eax + edx + 0xb00]
    x0 = ld(m, ecx);                                               // 42420b movaps xmm0, [ecx]
    x0 = andps(x0, x1);                                            // 42420e andps xmm0, xmm1
    x7 = X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000);     // 424211 movaps xmm7, [0x93a200]
    x6 = X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000);     // 424218 movaps xmm6, [0x93a210]
    eax = eax + edx;                                               // 42421f add eax, edx
    x4 = x0;                                                       // 424221 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x00);                                     // 424224 shufps xmm4, xmm0, 0
    x1 = x3;                                                       // 424228 movaps xmm1, xmm3
    x1 = shufps(x1, x3, 0x1b);                                     // 42422b shufps xmm1, xmm3, 0x1b
    x4 = mulps(x4, x1);                                            // 42422f mulps xmm4, xmm1
    x1 = shufps(x1, x1, 0xb1);                                     // 424232 shufps xmm1, xmm1, 0xb1
    x2 = x1;                                                       // 424236 movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0x1b);                                     // 424239 shufps xmm2, xmm1, 0x1b
    x5 = x0;                                                       // 42423d movaps xmm5, xmm0
    x5 = shufps(x5, x0, 0x55);                                     // 424240 shufps xmm5, xmm0, 0x55
    x5 = mulps(x5, x1);                                            // 424244 mulps xmm5, xmm1
    x1 = x0;                                                       // 424247 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0xaa);                                     // 42424a shufps xmm1, xmm0, 0xaa
    x2 = mulps(x2, x1);                                            // 42424e mulps xmm2, xmm1
    x2 = mulps(x2, x6);                                            // 424251 mulps xmm2, xmm6
    x1 = x7;                                                       // 424254 movaps xmm1, xmm7
    x1 = mulps(x1, x5);                                            // 424257 mulps xmm1, xmm5
    x2 = addps(x2, x1);                                            // 42425a addps xmm2, xmm1
    x1 = x0;                                                       // 42425d movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0xff);                                     // 424260 shufps xmm1, xmm0, 0xff
    x0 = X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000);     // 424264 movaps xmm0, [0x93a1f0]
    x1 = mulps(x1, x3);                                            // 42426b mulps xmm1, xmm3
    x0 = mulps(x0, x4);                                            // 42426e mulps xmm0, xmm4
    x1 = addps(x1, x0);                                            // 424271 addps xmm1, xmm0
    x0 = X::v(0xbf800000, 0xbf800000, 0xbf800000, 0x3f800000);     // 424274 movaps xmm0, [0x93a220]
    x0 = mulps(x0, x3);                                            // 42427b mulps xmm0, xmm3
    x2 = addps(x2, x1);                                            // 42427e addps xmm2, xmm1
    x3 = x0;                                                       // 424281 movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0x00);                                     // 424284 shufps xmm3, xmm0, 0
    x1 = x2;                                                       // 424288 movaps xmm1, xmm2
    x1 = shufps(x1, x2, 0x1b);                                     // 42428b shufps xmm1, xmm2, 0x1b
    x3 = mulps(x3, x1);                                            // 42428f mulps xmm3, xmm1
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 424292 mulps xmm3, [0x93a1f0]
    x1 = shufps(x1, x1, 0xb1);                                     // 424299 shufps xmm1, xmm1, 0xb1
    x4 = x0;                                                       // 42429d movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x55);                                     // 4242a0 shufps xmm4, xmm0, 0x55
    x4 = mulps(x4, x1);                                            // 4242a4 mulps xmm4, xmm1
    x1 = shufps(x1, x1, 0x1b);                                     // 4242a7 shufps xmm1, xmm1, 0x1b
    x5 = x0;                                                       // 4242ab movaps xmm5, xmm0
    x4 = mulps(x4, x7);                                            // 4242ae mulps xmm4, xmm7
    x5 = shufps(x5, x0, 0xaa);                                     // 4242b1 shufps xmm5, xmm0, 0xaa
    x1 = mulps(x1, x5);                                            // 4242b5 mulps xmm1, xmm5
    x1 = mulps(x1, x6);                                            // 4242b8 mulps xmm1, xmm6
    x1 = addps(x1, x4);                                            // 4242bb addps xmm1, xmm4
    x4 = x0;                                                       // 4242be movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0xff);                                     // 4242c1 shufps xmm4, xmm0, 0xff
    x4 = mulps(x4, x2);                                            // 4242c5 mulps xmm4, xmm2
    x4 = addps(x4, x3);                                            // 4242c8 addps xmm4, xmm3
    x1 = addps(x1, x4);                                            // 4242cb addps xmm1, xmm4
    st(m, ecx, x1);                                                // 4242ce movaps [ecx], xmm1
    x0 = ld(m, eax + 0xb10);                                       // 4242d1 movaps xmm0, [eax + 0xb10]
    x1 = addps(x1, x0);                                            // 4242d8 addps xmm1, xmm0
    st(m, eax + 0xb10, x1);                                        // 4242db movaps [eax + 0xb10], xmm1
    esp = ebp;                                                     // 4242e2 mov esp, ebp
    ebp = k.pop();                                                 // 4242e4 pop ebp
    return k.ret(4);                                               // 4242e5 ret 4
}

// 00424340 FUN_00424340: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00424340, Fn_00424340, "math", reflex_port::OUT_EAX) {  // outputs: what callers read (it calls other groups' code)
    Cpu k(m);
    CPU_REFS(k);
    k.push(ecx);                                                   // 424340 push ecx
    eax = 0x1u;                                                    // 424341 mov eax, 1
    fl = f_test8(ld8(m, 0xd72aa8), (eax & 0xff));                  // 424346 test byte ptr [0xd72aa8], al
    if (fl.ne()) goto L_00424366;                                  // 42434c jne 0x424366
    st32(m, 0xd72aa8, ld32(m, 0xd72aa8) | eax);                    // 42434e or [0xd72aa8], eax
    k.call(0x004242f0);  // FUN_004242f0                           // 424354 call 0x4242f0
    k.push(0x911e90u);                                             // 424359 push 0x911e90
    k.call(0x008f9fbf);  // _atexit                                // 42435e call 0x8f9fbf
    esp = esp + 0x4u;                                              // 424363 add esp, 4
L_00424366:
    eax = 0xd72aacu;                                               // 424366 mov eax, 0xd72aac
    ecx = k.pop();                                                 // 42436b pop ecx
    return k.ret(0);                                               // 42436c ret 
}

// 00424370 FUN_00424370: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00424370, Fn_00424370, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    x2 = cvtss2sd(x2, x0);                                         // 424370 cvtss2sd xmm2, xmm0
    x3 = cvtss2sd(x3, x1);                                         // 424374 cvtss2sd xmm3, xmm1
    fl = f_comisd(x2.q(0), x3.q(0));                               // 424378 ucomisd xmm2, xmm3
    eax = (eax & 0xffff00ffu) | (fl.lahf() << 8);                  // 42437c lahf 
    fl = f_test8(((eax >> 8) & 0xff), 0x44u);                      // 42437d test ah, 0x44
    if (fl.np()) goto L_004243fb;                                  // 424380 jnp 0x4243fb
    x2 = X::ss(ld32(m, esp + 0x4));                                // 424382 movss xmm2, [esp + 4]
    x3 = cvtss2sd(x3, x0);                                         // 424388 cvtss2sd xmm3, xmm0
    x2 = cvtps2pd(x2);                                             // 42438c cvtps2pd xmm2, xmm2
    x1 = cvtss2sd(x1, x1);                                         // 42438f cvtss2sd xmm1, xmm1
    x0 = cvtss2sd(x0, x0);                                         // 424393 cvtss2sd xmm0, xmm0
    x1 = subsd(x1, x0);                                            // 424397 subsd xmm1, xmm0
    x0 = X::ss(ld32(m, esp + 0x8));                                // 42439b movss xmm0, [esp + 8]
    x2 = subsd(x2, x3);                                            // 4243a1 subsd xmm2, xmm3
    x2 = divsd(x2, x1);                                            // 4243a5 divsd xmm2, xmm1
    x1 = X::ss(ld32(m, esp + 0xc));                                // 4243a9 movss xmm1, [esp + 0xc]
    fl = f_comiss(x1.f(0), x0.f(0));                               // 4243af comiss xmm1, xmm0
    x3 = cvtpd2ps(x2);                                             // 4243b2 cvtpd2ps xmm3, xmm2
    if (fl.be()) goto L_004243bd;                                  // 4243b6 jbe 0x4243bd
    x2 = x0;                                                       // 4243b8 movaps xmm2, xmm0
    goto L_004243c0;                                               // 4243bb jmp 0x4243c0
L_004243bd:
    x2 = x1;                                                       // 4243bd movaps xmm2, xmm1
L_004243c0:
    fl = f_comiss(x0.f(0), x1.f(0));                               // 4243c0 comiss xmm0, xmm1
    if (fl.be()) goto L_004243ca;                                  // 4243c3 jbe 0x4243ca
    x4 = x0;                                                       // 4243c5 movaps xmm4, xmm0
    goto L_004243cd;                                               // 4243c8 jmp 0x4243cd
L_004243ca:
    x4 = x1;                                                       // 4243ca movaps xmm4, xmm1
L_004243cd:
    x1 = cvtps2pd(x1);                                             // 4243cd cvtps2pd xmm1, xmm1
    x5 = cvtps2pd(x0);                                             // 4243d0 cvtps2pd xmm5, xmm0
    x1 = subsd(x1, x5);                                            // 4243d3 subsd xmm1, xmm5
    x3 = cvtss2sd(x3, x3);                                         // 4243d7 cvtss2sd xmm3, xmm3
    x0 = cvtps2pd(x0);                                             // 4243db cvtps2pd xmm0, xmm0
    x1 = mulsd(x1, x3);                                            // 4243de mulsd xmm1, xmm3
    x1 = addsd(x1, x0);                                            // 4243e2 addsd xmm1, xmm0
    x0 = cvtpd2ps(x1);                                             // 4243e6 cvtpd2ps xmm0, xmm1
    fl = f_comiss(x0.f(0), x2.f(0));                               // 4243ea comiss xmm0, xmm2
    if (fl.a()) goto L_004243f2;                                   // 4243ed ja 0x4243f2
    x0 = x2;                                                       // 4243ef movaps xmm0, xmm2
L_004243f2:
    fl = f_comiss(x4.f(0), x0.f(0));                               // 4243f2 comiss xmm4, xmm0
    if (fl.a()) goto L_00424401;                                   // 4243f5 ja 0x424401
    x0 = x4;                                                       // 4243f7 movaps xmm0, xmm4
    return k.ret(0);                                               // 4243fa ret 
L_004243fb:
    x0 = X::ss(ld32(m, esp + 0x8));                                // 4243fb movss xmm0, [esp + 8]
L_00424401:
    return k.ret(0);                                               // 424401 ret 
}

// 004252a0 FUN_004252a0: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x004252a0, Fn_004252a0, "math", 0) {  // outputs: what callers read (it calls other groups' code)
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 4252a0 push ebp
    ebp = esp;                                                     // 4252a1 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 4252a3 and esp, 0xfffffff0
    esp = esp - 0x14u;                                             // 4252a6 sub esp, 0x14
    k.push(ebx);                                                   // 4252a9 push ebx
    ebx = ld32(m, ebp + 0x8);                                      // 4252aa mov ebx, [ebp + 8]
    fl = f_cmp(ld32(m, ebx + 0xa8a0), 0x0u);                       // 4252ad cmp [ebx + 0xa8a0], 0
    k.push(esi);                                                   // 4252b4 push esi
    k.push(edi);                                                   // 4252b5 push edi
    if (fl.e()) goto L_004252bf;                                   // 4252b6 je 0x4252bf
    edi = 0x6u;                                                    // 4252b8 mov edi, 6
    goto L_004252e2;                                               // 4252bd jmp 0x4252e2
L_004252bf:
    fl = f_cmp(ld32(m, ebx + 0xa834), 0x0u);                       // 4252bf cmp [ebx + 0xa834], 0
    if (fl.e()) goto L_0042531d;                                   // 4252c6 je 0x42531d
    eax = (eax & 0xffffff00) | (uint32_t(ld8(m, ebx + 0x2fc)) & 0xff); // 4252c8 mov al, byte ptr [ebx + 0x2fc]
    fl = f_cmp8((eax & 0xff), 0x2u);                               // 4252ce cmp al, 2
    if (fl.ne()) goto L_004252d6;                                  // 4252d0 jne 0x4252d6
    edi = 0u;                                                      // 4252d2 xor edi, edi
    goto L_004252e2;                                               // 4252d4 jmp 0x4252e2
L_004252d6:
    edi = uint32_t(int32_t(uint8_t((eax & 0xff))));                // 4252d6 movzx edi, al
    fl = f_cmp(edi, 0xffffffffu);                                  // 4252d9 cmp edi, -1
    if (fl.e()) goto L_004255e8;                                   // 4252dc je 0x4255e8
L_004252e2:
    ecx = ld32(m, ebx + 0x2f4);                                    // 4252e2 mov ecx, [ebx + 0x2f4]
    fl = f_cmp(edi, ecx);                                          // 4252e8 cmp edi, ecx
    if (fl.e()) goto L_00425438;                                   // 4252ea je 0x425438
    eax = ld32(m, ebx + 0x2f0);                                    // 4252f0 mov eax, [ebx + 0x2f0]
    fl = f_test(eax, eax);                                         // 4252f6 test eax, eax
    if (fl.e()) goto L_00425366;                                   // 4252f8 je 0x425366
    fl = f_cmp(ecx, 0x5u);                                         // 4252fa cmp ecx, 5
    if (fl.e()) goto L_0042534d;                                   // 4252fd je 0x42534d
    fl = f_cmp(ecx, 0x6u);                                         // 4252ff cmp ecx, 6
    if (fl.e()) goto L_0042534d;                                   // 425302 je 0x42534d
    fl = f_cmp(ecx, 0x7u);                                         // 425304 cmp ecx, 7
    if (fl.e()) goto L_0042534d;                                   // 425307 je 0x42534d
    esi = eax;                                                     // 425309 mov esi, eax
    k.call(0x00807890);  // FUN_00807890                           // 42530b call 0x807890
    esi = ld32(m, ebx + 0x2f0);                                    // 425310 mov esi, [ebx + 0x2f0]
    k.call(0x00804e40);  // FUN_00804e40                           // 425316 call 0x804e40
    goto L_0042535c;                                               // 42531b jmp 0x42535c
L_0042531d:
    esi = ld32(m, ebx + 0x2f0);                                    // 42531d mov esi, [ebx + 0x2f0]
    fl = f_test(esi, esi);                                         // 425323 test esi, esi
    if (fl.e()) goto L_004255e8;                                   // 425325 je 0x4255e8
    k.call(0x00804e40);  // FUN_00804e40                           // 42532b call 0x804e40
    st32(m, ebx + 0x2f4, 0xffffffffu);                             // 425330 mov [ebx + 0x2f4], 0xffffffff
    st32(m, ebx + 0x2f0, 0x0u);                                    // 42533a mov [ebx + 0x2f0], 0
    edi = k.pop();                                                 // 425344 pop edi
    esi = k.pop();                                                 // 425345 pop esi
    ebx = k.pop();                                                 // 425346 pop ebx
    esp = ebp;                                                     // 425347 mov esp, ebp
    ebp = k.pop();                                                 // 425349 pop ebp
    return k.ret(4);                                               // 42534a ret 4
L_0042534d:
    fpush(m, reflex_f80::from_f32(0x3f000000u));                   // 42534d fld [0x9322cc]
    k.push(ecx);                                                   // 425353 push ecx
    st32(m, esp, reflex_f80::to_f32(fst_(m, 0), frc(m))); m.fpu.pop_back(); // 425354 fstp [esp]
    k.call(0x00807970);  // FUN_00807970                           // 425357 call 0x807970
L_0042535c:
    st32(m, ebx + 0x2f0, 0x0u);                                    // 42535c mov [ebx + 0x2f0], 0
L_00425366:
    st32(m, ebx + 0x2f4, edi);                                     // 425366 mov [ebx + 0x2f4], edi
    esi = uint32_t(int32_t(uint8_t(ld8(m, ebx + 0x2f4))));         // 42536c movzx esi, byte ptr [ebx + 0x2f4]
    st32(m, ebx + 0x2f0, 0x0u);                                    // 425373 mov [ebx + 0x2f0], 0
    x1 = ld(m, ebx + 0x410);                                       // 42537d movaps xmm1, [ebx + 0x410]
    x1 = mulps(x1, x1);                                            // 425384 mulps xmm1, xmm1
    x0 = x1;                                                       // 425387 movaps xmm0, xmm1
    x0 = shufps(x0, x1, 0x66);                                     // 42538a shufps xmm0, xmm1, 0x66
    x1 = addss(x1, x0);                                            // 42538e addss xmm1, xmm0
    x0 = shufps(x0, x0, 0x55);                                     // 425392 shufps xmm0, xmm0, 0x55
    x1 = addss(x1, x0);                                            // 425396 addss xmm1, xmm0
    x0 = x1;                                                       // 42539a movaps xmm0, xmm1
    x0 = shufps(x0, x0, 0x00);                                     // 42539d shufps xmm0, xmm0, 0
    x0 = sqrtps(x0);                                               // 4253a1 sqrtps xmm0, xmm0
    st32(m, esp + 0x14, x0.u[0]);                                  // 4253a4 movss [esp + 0x14], xmm0
    x0 = cvtss2sd(x0, x0);                                         // 4253aa cvtss2sd xmm0, xmm0
    st64(m, esp + 0x18, x0.q(0));                                  // 4253ae movsd [esp + 0x18], xmm0
    k.call(0x00679540);  // FUN_00679540                           // 4253b4 call 0x679540
    x1 = sd(ld64(m, esp + 0x18));                                  // 4253b9 movsd xmm1, [esp + 0x18]
    x0 = cvtss2sd(x0, x0);                                         // 4253bf cvtss2sd xmm0, xmm0
    x1 = divsd(x1, x0);                                            // 4253c3 divsd xmm1, xmm0
    x0 = sd(0x3ff0000000000000ull);                                // 4253c7 movsd xmm0, [0x977638]
    fl = f_comisd(x1.q(0), x0.q(0));                               // 4253cf comisd xmm1, xmm0
    if (fl.a()) goto L_004253fb;                                   // 4253d3 ja 0x4253fb
    esi = uint32_t(int32_t(uint8_t(ld8(m, ebx + 0x2f4))));         // 4253d5 movzx esi, byte ptr [ebx + 0x2f4]
    x0 = cvtss2sd(x0, X::ss(ld32(m, esp + 0x14)));                 // 4253dc cvtss2sd xmm0, [esp + 0x14]
    st64(m, esp + 0x18, x0.q(0));                                  // 4253e2 movsd [esp + 0x18], xmm0
    k.call(0x00679540);  // FUN_00679540                           // 4253e8 call 0x679540
    x1 = cvtss2sd(x1, x0);                                         // 4253ed cvtss2sd xmm1, xmm0
    x0 = sd(ld64(m, esp + 0x18));                                  // 4253f1 movsd xmm0, [esp + 0x18]
    x0 = divsd(x0, x1);                                            // 4253f7 divsd xmm0, xmm1
L_004253fb:
    esi = ld32(m, ebx + 0x2f0);                                    // 4253fb mov esi, [ebx + 0x2f0]
    x0 = cvtpd2ps(x0);                                             // 425401 cvtpd2ps xmm0, xmm0
    x0 = cvtss2sd(x0, x0);                                         // 425405 cvtss2sd xmm0, xmm0
    x0 = mulsd(x0, sd(0x4008000000000000ull));                     // 425409 mulsd xmm0, [0x977500]
    x0 = cvtpd2ps(x0);                                             // 425411 cvtpd2ps xmm0, xmm0
    st32(m, ebx + 0x2f8, x0.u[0]);                                 // 425415 movss [ebx + 0x2f8], xmm0
    fl = f_test(esi, esi);                                         // 42541d test esi, esi
    if (fl.e()) goto L_00425438;                                   // 42541f je 0x425438
    fpush(m, reflex_f80::from_f32(ld32(m, 0x9cd9e8)));             // 425421 fld [0x9cd9e8]
    k.push(ecx);                                                   // 425427 push ecx
    st32(m, esp, reflex_f80::to_f32(fst_(m, 0), frc(m))); m.fpu.pop_back(); // 425428 fstp [esp]
    eax = ebx + 0xa5a0;                                            // 42542b lea eax, [ebx + 0xa5a0]
    k.push(0x2u);                                                  // 425431 push 2
    k.call(0x008077c0);  // FUN_008077c0                           // 425433 call 0x8077c0
L_00425438:
    fl = f_cmp(ld32(m, ebx + 0x2f0), 0x0u);                        // 425438 cmp [ebx + 0x2f0], 0
    if (fl.e()) goto L_004255e8;                                   // 42543f je 0x4255e8
    x1 = ld(m, ebx + 0x410);                                       // 425445 movaps xmm1, [ebx + 0x410]
    esi = uint32_t(int32_t(uint8_t(ld8(m, ebx + 0x2f4))));         // 42544c movzx esi, byte ptr [ebx + 0x2f4]
    x1 = mulps(x1, x1);                                            // 425453 mulps xmm1, xmm1
    x0 = x1;                                                       // 425456 movaps xmm0, xmm1
    x0 = shufps(x0, x1, 0x66);                                     // 425459 shufps xmm0, xmm1, 0x66
    x1 = addss(x1, x0);                                            // 42545d addss xmm1, xmm0
    x0 = shufps(x0, x0, 0x55);                                     // 425461 shufps xmm0, xmm0, 0x55
    x1 = addss(x1, x0);                                            // 425465 addss xmm1, xmm0
    x0 = x1;                                                       // 425469 movaps xmm0, xmm1
    x0 = shufps(x0, x0, 0x00);                                     // 42546c shufps xmm0, xmm0, 0
    x0 = sqrtps(x0);                                               // 425470 sqrtps xmm0, xmm0
    st32(m, esp + 0x14, x0.u[0]);                                  // 425473 movss [esp + 0x14], xmm0
    x0 = cvtss2sd(x0, x0);                                         // 425479 cvtss2sd xmm0, xmm0
    st64(m, esp + 0x18, x0.q(0));                                  // 42547d movsd [esp + 0x18], xmm0
    k.call(0x00679540);  // FUN_00679540                           // 425483 call 0x679540
    x1 = sd(ld64(m, esp + 0x18));                                  // 425488 movsd xmm1, [esp + 0x18]
    x0 = cvtss2sd(x0, x0);                                         // 42548e cvtss2sd xmm0, xmm0
    x1 = divsd(x1, x0);                                            // 425492 divsd xmm1, xmm0
    x0 = sd(0x3ff0000000000000ull);                                // 425496 movsd xmm0, [0x977638]
    fl = f_comisd(x1.q(0), x0.q(0));                               // 42549e comisd xmm1, xmm0
    if (fl.a()) goto L_004254ca;                                   // 4254a2 ja 0x4254ca
    esi = uint32_t(int32_t(uint8_t(ld8(m, ebx + 0x2f4))));         // 4254a4 movzx esi, byte ptr [ebx + 0x2f4]
    x0 = cvtss2sd(x0, X::ss(ld32(m, esp + 0x14)));                 // 4254ab cvtss2sd xmm0, [esp + 0x14]
    st64(m, esp + 0x18, x0.q(0));                                  // 4254b1 movsd [esp + 0x18], xmm0
    k.call(0x00679540);  // FUN_00679540                           // 4254b7 call 0x679540
    x1 = cvtss2sd(x1, x0);                                         // 4254bc cvtss2sd xmm1, xmm0
    x0 = sd(ld64(m, esp + 0x18));                                  // 4254c0 movsd xmm0, [esp + 0x18]
    x0 = divsd(x0, x1);                                            // 4254c6 divsd xmm0, xmm1
L_004254ca:
    x0 = cvtpd2ps(x0);                                             // 4254ca cvtpd2ps xmm0, xmm0
    x0 = cvtss2sd(x0, x0);                                         // 4254ce cvtss2sd xmm0, xmm0
    x0 = mulsd(x0, sd(0x4008000000000000ull));                     // 4254d2 mulsd xmm0, [0x977500]
    x1 = cvtpd2ps(x0);                                             // 4254da cvtpd2ps xmm1, xmm0
    eax = 0xd67b98u;                                               // 4254de mov eax, 0xd67b98
    st32(m, ebx + 0x2f8, x1.u[0]);                                 // 4254e3 movss [ebx + 0x2f8], xmm1
    k.call(0x008070a0);  // FUN_008070a0                           // 4254eb call 0x8070a0
    esi = ld32(m, ebx + 0x2f0);                                    // 4254f0 mov esi, [ebx + 0x2f0]
    fl = f_cmp(ld32(m, esi), 0x1u);                                // 4254f6 cmp [esi], 1
    x1 = cvtss2sd(x1, x1);                                         // 4254f9 cvtss2sd xmm1, xmm1
    x0 = cvtss2sd(x0, x0);                                         // 4254fd cvtss2sd xmm0, xmm0
    x0 = mulsd(x0, x1);                                            // 425501 mulsd xmm0, xmm1
    x1 = cvtsd2ss(x1, x0);                                         // 425505 cvtsd2ss xmm1, xmm0
    st32(m, esp + 0x14, x1.u[0]);                                  // 425509 movss [esp + 0x14], xmm1
    if (fl.ne()) goto L_00425587;                                  // 42550f jne 0x425587
    x0 = X::ss(0x3a83126fu);                                       // 425511 movss xmm0, [0x97743c]
    fl = f_comiss(x0.f(0), x1.f(0));                               // 425519 comiss xmm0, xmm1
    x0 = X{};                                                      // 42551c xorps xmm0, xmm0
    if (fl.be()) goto L_0042553d;                                  // 42551f jbe 0x42553d
    x2 = X::ss(ld32(m, esi + 0x1c));                               // 425521 movss xmm2, [esi + 0x1c]
    fl = f_comiss(x2.f(0), x0.f(0));                               // 425526 ucomiss xmm2, xmm0
    eax = (eax & 0xffff00ffu) | (fl.lahf() << 8);                  // 425529 lahf 
    fl = f_test8(((eax >> 8) & 0xff), 0x44u);                      // 42552a test ah, 0x44
    if (fl.p()) goto L_0042553d;                                   // 42552d jp 0x42553d
    k.call(0x00807890);  // FUN_00807890                           // 42552f call 0x807890
    edi = k.pop();                                                 // 425534 pop edi
    esi = k.pop();                                                 // 425535 pop esi
    ebx = k.pop();                                                 // 425536 pop ebx
    esp = ebp;                                                     // 425537 mov esp, ebp
    ebp = k.pop();                                                 // 425539 pop ebp
    return k.ret(4);                                               // 42553a ret 4
L_0042553d:
    x2 = X::ss(ld32(m, esi + 0x1c));                               // 42553d movss xmm2, [esi + 0x1c]
    fl = f_comiss(x2.f(0), x0.f(0));                               // 425542 ucomiss xmm2, xmm0
    eax = (eax & 0xffff00ffu) | (fl.lahf() << 8);                  // 425545 lahf 
    fl = f_test8(((eax >> 8) & 0xff), 0x44u);                      // 425546 test ah, 0x44
    if (fl.np()) goto L_00425563;                                  // 425549 jnp 0x425563
    eax = esi + 0x8;                                               // 42554b lea eax, [esi + 8]
    k.call(0x008070a0);  // FUN_008070a0                           // 42554e call 0x8070a0
    x1 = cvtss2sd(x1, x1);                                         // 425553 cvtss2sd xmm1, xmm1
    x0 = cvtss2sd(x0, x0);                                         // 425557 cvtss2sd xmm0, xmm0
    x0 = mulsd(x0, x1);                                            // 42555b mulsd xmm0, xmm1
    x1 = cvtsd2ss(x1, x0);                                         // 42555f cvtsd2ss xmm1, xmm0
L_00425563:
    edi = esi;                                                     // 425563 mov edi, esi
    x0 = x1;                                                       // 425565 movaps xmm0, xmm1
    k.call(0x008078d0);  // FUN_008078d0                           // 425568 call 0x8078d0
    eax = ld32(m, ebx + 0x2f0);                                    // 42556d mov eax, [ebx + 0x2f0]
    ecx = ebx + 0xa5a0;                                            // 425573 lea ecx, [ebx + 0xa5a0]
    k.call(0x00807c30);  // FUN_00807c30                           // 425579 call 0x807c30
    edi = k.pop();                                                 // 42557e pop edi
    esi = k.pop();                                                 // 42557f pop esi
    ebx = k.pop();                                                 // 425580 pop ebx
    esp = ebp;                                                     // 425581 mov esp, ebp
    ebp = k.pop();                                                 // 425583 pop ebp
    return k.ret(4);                                               // 425584 ret 4
L_00425587:
    fl = f_comiss(x1.f(0), flt(0x3a83126fu));                      // 425587 comiss xmm1, [0x97743c]
    if (fl.be()) goto L_004255e8;                                  // 42558e jbe 0x4255e8
    x0 = X::ss(ld32(m, esi + 0x1c));                               // 425590 movss xmm0, [esi + 0x1c]
    fl = f_comiss(x0.f(0), flt(0x00000000u));                      // 425595 ucomiss xmm0, [0x92e12c]
    eax = (eax & 0xffff00ffu) | (fl.lahf() << 8);                  // 42559c lahf 
    fl = f_test8(((eax >> 8) & 0xff), 0x44u);                      // 42559d test ah, 0x44
    if (fl.np()) goto L_004255c0;                                  // 4255a0 jnp 0x4255c0
    eax = esi + 0x8;                                               // 4255a2 lea eax, [esi + 8]
    k.call(0x008070a0);  // FUN_008070a0                           // 4255a5 call 0x8070a0
    x0 = cvtss2sd(x0, x0);                                         // 4255aa cvtss2sd xmm0, xmm0
    x1 = cvtss2sd(x1, x1);                                         // 4255ae cvtss2sd xmm1, xmm1
    x0 = mulsd(x0, x1);                                            // 4255b2 mulsd xmm0, xmm1
    x0 = cvtsd2ss(x0, x0);                                         // 4255b6 cvtsd2ss xmm0, xmm0
    st32(m, esp + 0x14, x0.u[0]);                                  // 4255ba movss [esp + 0x14], xmm0
L_004255c0:
    fpush(m, reflex_f80::from_f32(ld32(m, 0x9cd9e8)));             // 4255c0 fld [0x9cd9e8]
    k.push(ecx);                                                   // 4255c6 push ecx
    st32(m, esp, reflex_f80::to_f32(fst_(m, 0), frc(m))); m.fpu.pop_back(); // 4255c7 fstp [esp]
    eax = ebx + 0xa5a0;                                            // 4255ca lea eax, [ebx + 0xa5a0]
    k.push(0x2u);                                                  // 4255d0 push 2
    k.call(0x008077c0);  // FUN_008077c0                           // 4255d2 call 0x8077c0
    edi = ld32(m, ebx + 0x2f0);                                    // 4255d7 mov edi, [ebx + 0x2f0]
    x0 = X::ss(ld32(m, esp + 0x14));                               // 4255dd movss xmm0, [esp + 0x14]
    k.call(0x008078d0);  // FUN_008078d0                           // 4255e3 call 0x8078d0
L_004255e8:
    edi = k.pop();                                                 // 4255e8 pop edi
    esi = k.pop();                                                 // 4255e9 pop esi
    ebx = k.pop();                                                 // 4255ea pop ebx
    esp = ebp;                                                     // 4255eb mov esp, ebp
    ebp = k.pop();                                                 // 4255ed pop ebp
    return k.ret(4);                                               // 4255ee ret 4
}

// 00425600 FUN_00425600: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00425600, Fn_00425600, "math", 0) {  // outputs: what callers read (it calls other groups' code)
    Cpu k(m);
    CPU_REFS(k);
    x0 = X::ss(ld32(m, esp + 0x4));                                // 425600 movss xmm0, [esp + 4]
    x0 = cvtps2pd(x0);                                             // 425606 cvtps2pd xmm0, xmm0
    fl = f_comisd(x0.q(0), 0x3f9eb851e0000000ull);                 // 425609 comisd xmm0, [0x977dd8]
    if (fl.be()) goto L_00425621;                                  // 425611 jbe 0x425621
    x0 = X::ss(ld32(m, 0x9cf5a8));                                 // 425613 movss xmm0, [0x9cf5a8]
    st32(m, esp + 0x4, x0.u[0]);                                   // 42561b movss [esp + 4], xmm0
L_00425621:
    fpush(m, reflex_f80::from_f32(ld32(m, esp + 0x4)));            // 425621 fld [esp + 4]
    k.push(ecx);                                                   // 425625 push ecx
    st32(m, esp, reflex_f80::to_f32(fst_(m, 0), frc(m))); m.fpu.pop_back(); // 425626 fstp [esp]
    k.push(esi);                                                   // 425629 push esi
    k.call(0x00425640);  // FUN_00425640                           // 42562a call 0x425640
    st32(m, esp + 0x4, esi);                                       // 42562f mov [esp + 4], esi
    return k.tail(0x004252a0);  // tail jump FUN_004252a0          // 425633 jmp 0x4252a0
}

// 00425640 FUN_00425640: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00425640, Fn_00425640, "math", 0) {  // outputs: what callers read (it calls other groups' code)
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 425640 push ebp
    ebp = esp;                                                     // 425641 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 425643 and esp, 0xfffffff0
    k.push(ecx);                                                   // 425646 push ecx
    k.push(ebx);                                                   // 425647 push ebx
    ebx = ld32(m, ebp + 0x8);                                      // 425648 mov ebx, [ebp + 8]
    fl = f_cmp(ld32(m, ebx + 0xa830), 0x0u);                       // 42564b cmp [ebx + 0xa830], 0
    k.push(esi);                                                   // 425652 push esi
    k.push(edi);                                                   // 425653 push edi
    if (fl.e()) goto L_0042567f;                                   // 425654 je 0x42567f
    esi = ebx + 0xa810;                                            // 425656 lea esi, [ebx + 0xa810]
    edx = ebx + 0x380;                                             // 42565c lea edx, [ebx + 0x380]
    k.call(0x00422450);  // FUN_00422450                           // 425662 call 0x422450
    x0 = ld(m, ebx + 0xa820);                                      // 425667 movaps xmm0, [ebx + 0xa820]
    x1 = ld(m, ebx + 0x410);                                       // 42566e movaps xmm1, [ebx + 0x410]
    x0 = addps(x0, x1);                                            // 425675 addps xmm0, xmm1
    st(m, ebx + 0x410, x0);                                        // 425678 movaps [ebx + 0x410], xmm0
L_0042567f:
    fl = f_cmp(ld32(m, ebx + 0x374), 0x0u);                        // 42567f cmp [ebx + 0x374], 0
    if (fl.ne()) goto L_0042570d;                                  // 425686 jne 0x42570d
    fl = f_cmp8(ld8(m, ebx + 0x88), 0x0u);                         // 42568c cmp byte ptr [ebx + 0x88], 0
    if (fl.ne()) goto L_0042570d;                                  // 425693 jne 0x42570d
    fpush(m, reflex_f80::from_int(1));                             // 425695 fld1 
    k.push(ecx);                                                   // 425697 push ecx
    esi = ebx + 0x380;                                             // 425698 lea esi, [ebx + 0x380]
    st32(m, esp, reflex_f80::to_f32(fst_(m, 0), frc(m)));          // 42569e fst [esp]
    ecx = 0x7u;                                                    // 4256a1 mov ecx, 7
    k.call(0x004240e0);  // FUN_004240e0                           // 4256a6 call 0x4240e0
    k.push(ecx);                                                   // 4256ab push ecx
    ecx = 0x8u;                                                    // 4256ac mov ecx, 8
    st32(m, esp, reflex_f80::to_f32(fst_(m, 0), frc(m)));          // 4256b1 fst [esp]
    k.call(0x004240e0);  // FUN_004240e0                           // 4256b4 call 0x4240e0
    k.push(ecx);                                                   // 4256b9 push ecx
    ecx = 0xau;                                                    // 4256ba mov ecx, 0xa
    st32(m, esp, reflex_f80::to_f32(fst_(m, 0), frc(m)));          // 4256bf fst [esp]
    k.call(0x004240e0);  // FUN_004240e0                           // 4256c2 call 0x4240e0
    k.push(ecx);                                                   // 4256c7 push ecx
    ecx = 0xbu;                                                    // 4256c8 mov ecx, 0xb
    st32(m, esp, reflex_f80::to_f32(fst_(m, 0), frc(m)));          // 4256cd fst [esp]
    k.call(0x004240e0);  // FUN_004240e0                           // 4256d0 call 0x4240e0
    k.push(ecx);                                                   // 4256d5 push ecx
    ecx = 0x12u;                                                   // 4256d6 mov ecx, 0x12
    st32(m, esp, reflex_f80::to_f32(fst_(m, 0), frc(m)));          // 4256db fst [esp]
    k.call(0x004240e0);  // FUN_004240e0                           // 4256de call 0x4240e0
    k.push(ecx);                                                   // 4256e3 push ecx
    ecx = 0x13u;                                                   // 4256e4 mov ecx, 0x13
    st32(m, esp, reflex_f80::to_f32(fst_(m, 0), frc(m)));          // 4256e9 fst [esp]
    k.call(0x004240e0);  // FUN_004240e0                           // 4256ec call 0x4240e0
    k.push(ecx);                                                   // 4256f1 push ecx
    ecx = 0xeu;                                                    // 4256f2 mov ecx, 0xe
    st32(m, esp, reflex_f80::to_f32(fst_(m, 0), frc(m)));          // 4256f7 fst [esp]
    k.call(0x004240e0);  // FUN_004240e0                           // 4256fa call 0x4240e0
    k.push(ecx);                                                   // 4256ff push ecx
    ecx = 0xfu;                                                    // 425700 mov ecx, 0xf
    st32(m, esp, reflex_f80::to_f32(fst_(m, 0), frc(m))); m.fpu.pop_back(); // 425705 fstp [esp]
    k.call(0x004240e0);  // FUN_004240e0                           // 425708 call 0x4240e0
L_0042570d:
    fl = f_cmp8(ld8(m, ebx + 0x94), 0x0u);                         // 42570d cmp byte ptr [ebx + 0x94], 0
    if (fl.e()) goto L_0042571d;                                   // 425714 je 0x42571d
    ecx = ebx;                                                     // 425716 mov ecx, ebx
    k.call(0x0042a310);  // FUN_0042a310                           // 425718 call 0x42a310
L_0042571d:
    edi = ebx + 0x380;                                             // 42571d lea edi, [ebx + 0x380]
    k.push(edi);                                                   // 425723 push edi
    k.call(0x00420770);  // FUN_00420770                           // 425724 call 0x420770
    k.push(edi);                                                   // 425729 push edi
    k.call(0x00423b80);  // FUN_00423b80                           // 42572a call 0x423b80
    k.push(edi);                                                   // 42572f push edi
    k.call(0x00423df0);  // FUN_00423df0                           // 425730 call 0x423df0
    eax = ld32(m, ebx + 0x8c);                                     // 425735 mov eax, [ebx + 0x8c]
    st32(m, ebx + 0x90, eax);                                      // 42573b mov [ebx + 0x90], eax
    k.call(0x00422550);  // FUN_00422550                           // 425741 call 0x422550
    esi = edi;                                                     // 425746 mov esi, edi
    k.call(0x00422800);  // FUN_00422800                           // 425748 call 0x422800
    eax = 0u;                                                      // 42574d xor eax, eax
    fl = f_cmp(ld32(m, edi + 0x7c), eax);                          // 42574f cmp [edi + 0x7c], eax
    if (fl.le()) goto L_0042576d;                                  // 425752 jle 0x42576d
    x0 = X{};                                                      // 425754 xorps xmm0, xmm0
    ecx = edi + 0x18;                                              // 425757 lea ecx, [edi + 0x18]
    ebx = ebx;                                                     // 42575a lea ebx, [ebx]
L_00425760:
    st32(m, ecx, x0.u[0]);                                         // 425760 movss [ecx], xmm0
    eax = eax + 1u;                                                // 425764 inc eax
    ecx = ecx + 0x4u;                                              // 425765 add ecx, 4
    fl = f_cmp(eax, ld32(m, edi + 0x7c));                          // 425768 cmp eax, [edi + 0x7c]
    if (fl.l()) goto L_00425760;                                   // 42576b jl 0x425760
L_0042576d:
    fpush(m, reflex_f80::from_f32(ld32(m, ebp + 0xc)));            // 42576d fld [ebp + 0xc]
    k.push(ecx);                                                   // 425770 push ecx
    st32(m, esp, reflex_f80::to_f32(fst_(m, 0), frc(m))); m.fpu.pop_back(); // 425771 fstp [esp]
    k.push(ebx);                                                   // 425774 push ebx
    k.call(0x00425b20);  // FUN_00425b20                           // 425775 call 0x425b20
    k.call(0x00422590);  // FUN_00422590                           // 42577a call 0x422590
    fpush(m, reflex_f80::from_f32(ld32(m, ebp + 0xc)));            // 42577f fld [ebp + 0xc]
    k.push(ecx);                                                   // 425782 push ecx
    st32(m, esp, reflex_f80::to_f32(fst_(m, 0), frc(m))); m.fpu.pop_back(); // 425783 fstp [esp]
    k.push(ebx);                                                   // 425786 push ebx
    k.call(0x00426d00);  // FUN_00426d00                           // 425787 call 0x426d00
    fpush(m, reflex_f80::from_f32(ld32(m, ebp + 0xc)));            // 42578c fld [ebp + 0xc]
    k.push(ecx);                                                   // 42578f push ecx
    st32(m, esp, reflex_f80::to_f32(fst_(m, 0), frc(m))); m.fpu.pop_back(); // 425790 fstp [esp]
    k.push(ebx);                                                   // 425793 push ebx
    k.call(0x00429a40);  // FUN_00429a40                           // 425794 call 0x429a40
    k.push(ebx);                                                   // 425799 push ebx
    k.call(0x0042a6c0);  // FUN_0042a6c0                           // 42579a call 0x42a6c0
    x0 = X::ss(ld32(m, ebp + 0xc));                                // 42579f movss xmm0, [ebp + 0xc]
    ecx = edi;                                                     // 4257a4 mov ecx, edi
    k.call(0x004238a0);  // FUN_004238a0                           // 4257a6 call 0x4238a0
    x0 = X::ss(ld32(m, ebp + 0xc));                                // 4257ab movss xmm0, [ebp + 0xc]
    k.push(edi);                                                   // 4257b0 push edi
    k.call(0x00423980);  // FUN_00423980                           // 4257b1 call 0x423980
    k.push(ebx);                                                   // 4257b6 push ebx
    k.call(0x00425920);  // FUN_00425920                           // 4257b7 call 0x425920
    fl = f_cmp(ld32(m, ebx + 0x374), 0x0u);                        // 4257bc cmp [ebx + 0x374], 0
    if (fl.ne()) goto L_004257d2;                                  // 4257c3 jne 0x4257d2
    fl = f_cmp8(ld8(m, ebx + 0x88), 0x0u);                         // 4257c5 cmp byte ptr [ebx + 0x88], 0
    if (fl.ne()) goto L_004257d2;                                  // 4257cc jne 0x4257d2
    eax = 0u;                                                      // 4257ce xor eax, eax
    goto L_004257d7;                                               // 4257d0 jmp 0x4257d7
L_004257d2:
    eax = 0x1u;                                                    // 4257d2 mov eax, 1
L_004257d7:
    fl = f_cmp(ld32(m, ebx + 0x90), 0x0u);                         // 4257d7 cmp [ebx + 0x90], 0
    st32(m, ebx + 0x8c, eax);                                      // 4257de mov [ebx + 0x8c], eax
    if (fl.ne()) goto L_004257f0;                                  // 4257e4 jne 0x4257f0
    fl = f_test(eax, eax);                                         // 4257e6 test eax, eax
    if (fl.e()) goto L_004257f0;                                   // 4257e8 je 0x4257f0
    k.push(ebx);                                                   // 4257ea push ebx
    k.call(0x0042ade0);  // FUN_0042ade0                           // 4257eb call 0x42ade0
L_004257f0:
    edi = k.pop();                                                 // 4257f0 pop edi
    esi = k.pop();                                                 // 4257f1 pop esi
    ebx = k.pop();                                                 // 4257f2 pop ebx
    esp = ebp;                                                     // 4257f3 mov esp, ebp
    ebp = k.pop();                                                 // 4257f5 pop ebp
    return k.ret(8);                                               // 4257f6 ret 8
}

// 00425800 FUN_00425800: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00425800, Fn_00425800, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 425800 push ebp
    ebp = esp;                                                     // 425801 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 425803 and esp, 0xfffffff0
    esp = esp - 0x1cu;                                             // 425806 sub esp, 0x1c
    k.push(esi);                                                   // 425809 push esi
    eax = ld32(m, ebp + 0x8);                                      // 42580a mov eax, [ebp + 8]
    x1 = ld(m, eax + 0x410);                                       // 42580d movaps xmm1, [eax + 0x410]
    x2 = ld(m, eax + 0xe90);                                       // 425814 movaps xmm2, [eax + 0xe90]
    x1 = mulps(x1, x1);                                            // 42581b mulps xmm1, xmm1
    x0 = x1;                                                       // 42581e movaps xmm0, xmm1
    x0 = shufps(x0, x1, 0x99);                                     // 425821 shufps xmm0, xmm1, 0x99
    x1 = addss(x1, x0);                                            // 425825 addss xmm1, xmm0
    x0 = shufps(x0, x0, 0x55);                                     // 425829 shufps xmm0, xmm0, 0x55
    x1 = addss(x1, x0);                                            // 42582d addss xmm1, xmm0
    x3 = x1;                                                       // 425831 movaps xmm3, xmm1
    x2 = mulps(x2, x2);                                            // 425834 mulps xmm2, xmm2
    x1 = x2;                                                       // 425837 movaps xmm1, xmm2
    x2 = ld(m, eax + 0x330);                                       // 42583a movaps xmm2, [eax + 0x330]
    x0 = x1;                                                       // 425841 movaps xmm0, xmm1
    x0 = shufps(x0, x1, 0x99);                                     // 425844 shufps xmm0, xmm1, 0x99
    x1 = addss(x1, x0);                                            // 425848 addss xmm1, xmm0
    x0 = shufps(x0, x0, 0x55);                                     // 42584c shufps xmm0, xmm0, 0x55
    x1 = addss(x1, x0);                                            // 425850 addss xmm1, xmm0
    x0 = x3;                                                       // 425854 movaps xmm0, xmm3
    x0 = shufps(x0, x3, 0x00);                                     // 425857 shufps xmm0, xmm3, 0
    x0 = cmpltps(x0, x2);                                          // 42585b cmpltps xmm0, xmm2
    st(m, esp + 0x10, x0);                                         // 42585f movaps [esp + 0x10], xmm0
    ecx = ld32(m, esp + 0x14);                                     // 425864 mov ecx, [esp + 0x14]
    edx = ld32(m, esp + 0x18);                                     // 425868 mov edx, [esp + 0x18]
    edx = edx + ecx;                                               // 42586c add edx, ecx
    edx = edx + ld32(m, esp + 0x10);                               // 42586e add edx, [esp + 0x10]
    ecx = 0x1u;                                                    // 425872 mov ecx, 1
    fl = f_cmp(edx, 0xfffffffdu);                                  // 425877 cmp edx, -3
    if (fl.ne()) goto L_004258fb;                                  // 42587a jne 0x4258fb
    x0 = x1;                                                       // 42587c movaps xmm0, xmm1
    x0 = shufps(x0, x1, 0x00);                                     // 42587f shufps xmm0, xmm1, 0
    x1 = ld(m, eax + 0x340);                                       // 425883 movaps xmm1, [eax + 0x340]
    x0 = cmpltps(x0, x1);                                          // 42588a cmpltps xmm0, xmm1
    st(m, esp + 0x10, x0);                                         // 42588e movaps [esp + 0x10], xmm0
    edx = ld32(m, esp + 0x14);                                     // 425893 mov edx, [esp + 0x14]
    esi = ld32(m, esp + 0x18);                                     // 425897 mov esi, [esp + 0x18]
    esi = esi + edx;                                               // 42589b add esi, edx
    esi = esi + ld32(m, esp + 0x10);                               // 42589d add esi, [esp + 0x10]
    fl = f_cmp(esi, 0xfffffffdu);                                  // 4258a1 cmp esi, -3
    if (fl.ne()) goto L_004258fb;                                  // 4258a4 jne 0x4258fb
    fl = f_cmp(ld32(m, eax + 0x374), 0x0u);                        // 4258a6 cmp [eax + 0x374], 0
    if (fl.e()) goto L_004258fb;                                   // 4258ad je 0x4258fb
    x1 = ld(m, eax + 0x310);                                       // 4258af movaps xmm1, [eax + 0x310]
    x0 = X::ss(ld32(m, ebp + 0xc));                                // 4258b6 movss xmm0, [ebp + 0xc]
    x0 = shufps(x0, x0, 0x00);                                     // 4258bb shufps xmm0, xmm0, 0
    x0 = addps(x0, x1);                                            // 4258bf addps xmm0, xmm1
    st(m, eax + 0x310, x0);                                        // 4258c2 movaps [eax + 0x310], xmm0
    x0 = ld(m, eax + 0x320);                                       // 4258c9 movaps xmm0, [eax + 0x320]
    x1 = ld(m, eax + 0x310);                                       // 4258d0 movaps xmm1, [eax + 0x310]
    x0 = cmpltps(x0, x1);                                          // 4258d7 cmpltps xmm0, xmm1
    st(m, esp + 0x10, x0);                                         // 4258db movaps [esp + 0x10], xmm0
    edx = ld32(m, esp + 0x14);                                     // 4258e0 mov edx, [esp + 0x14]
    esi = ld32(m, esp + 0x18);                                     // 4258e4 mov esi, [esp + 0x18]
    esi = esi + edx;                                               // 4258e8 add esi, edx
    esi = esi + ld32(m, esp + 0x10);                               // 4258ea add esi, [esp + 0x10]
    fl = f_cmp(esi, 0xfffffffdu);                                  // 4258ee cmp esi, -3
    if (fl.ne()) goto L_00425909;                                  // 4258f1 jne 0x425909
    st32(m, eax + 0x370, ecx);                                     // 4258f3 mov [eax + 0x370], ecx
    goto L_00425909;                                               // 4258f9 jmp 0x425909
L_004258fb:
    x0 = ld(m, 0xd61670);                                          // 4258fb movaps xmm0, [0xd61670]
    st(m, eax + 0x310, x0);                                        // 425902 movaps [eax + 0x310], xmm0
L_00425909:
    fl = f_cmp(ld32(m, eax + 0x378), 0x0u);                        // 425909 cmp [eax + 0x378], 0
    if (fl.e()) goto L_00425918;                                   // 425910 je 0x425918
    st32(m, eax + 0x370, ecx);                                     // 425912 mov [eax + 0x370], ecx
L_00425918:
    esi = k.pop();                                                 // 425918 pop esi
    esp = ebp;                                                     // 425919 mov esp, ebp
    ebp = k.pop();                                                 // 42591b pop ebp
    return k.ret(8);                                               // 42591c ret 8
}

// 00425920 FUN_00425920: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00425920, Fn_00425920, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 425920 push ebp
    ebp = esp;                                                     // 425921 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 425923 and esp, 0xfffffff0
    esp = esp - 0x34u;                                             // 425926 sub esp, 0x34
    eax = ld32(m, ebp + 0x8);                                      // 425929 mov eax, [ebp + 8]
    x4 = ld(m, 0xd61670);                                          // 42592c movaps xmm4, [0xd61670]
    k.push(ebx);                                                   // 425933 push ebx
    ebx = ld32(m, eax + 0x3fc);                                    // 425934 mov ebx, [eax + 0x3fc]
    ecx = 0u;                                                      // 42593a xor ecx, ecx
    fl = f_cmp(ebx, ecx);                                          // 42593c cmp ebx, ecx
    k.push(esi);                                                   // 42593e push esi
    k.push(edi);                                                   // 42593f push edi
    st32(m, esp + 0x1c, ecx);                                      // 425940 mov [esp + 0x1c], ecx
    st32(m, esp + 0x18, ecx);                                      // 425944 mov [esp + 0x18], ecx
    x6 = x4;                                                       // 425948 movaps xmm6, xmm4
    if (fl.le()) goto L_00425b08;                                  // 42594b jle 0x425b08
    x0 = ld(m, eax + 0x360);                                       // 425951 movaps xmm0, [eax + 0x360]
    x7 = ld(m, eax + 0x350);                                       // 425958 movaps xmm7, [eax + 0x350]
    st(m, esp + 0x20, x0);                                         // 42595f movaps [esp + 0x20], xmm0
    edi = eax + 0xe90;                                             // 425964 lea edi, [eax + 0xe90]
    ecx = eax + 0x410;                                             // 42596a lea ecx, [eax + 0x410]
L_00425970:
    x2 = ld(m, ecx);                                               // 425970 movaps xmm2, [ecx]
    x3 = ld(m, edi);                                               // 425973 movaps xmm3, [edi]
    x0 = x2;                                                       // 425976 movaps xmm0, xmm2
    x0 = mulps(x0, x2);                                            // 425979 mulps xmm0, xmm2
    x1 = x0;                                                       // 42597c movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x99);                                     // 42597f shufps xmm1, xmm0, 0x99
    x0 = addss(x0, x1);                                            // 425983 addss xmm0, xmm1
    x1 = shufps(x1, x1, 0x55);                                     // 425987 shufps xmm1, xmm1, 0x55
    x0 = addss(x0, x1);                                            // 42598b addss xmm0, xmm1
    x0 = shufps(x0, x0, 0x00);                                     // 42598f shufps xmm0, xmm0, 0
    x5 = x0;                                                       // 425993 movaps xmm5, xmm0
    x3 = mulps(x3, x3);                                            // 425996 mulps xmm3, xmm3
    x1 = x3;                                                       // 425999 movaps xmm1, xmm3
    x0 = x1;                                                       // 42599c movaps xmm0, xmm1
    x0 = shufps(x0, x1, 0x99);                                     // 42599f shufps xmm0, xmm1, 0x99
    x1 = addss(x1, x0);                                            // 4259a3 addss xmm1, xmm0
    x0 = shufps(x0, x0, 0x55);                                     // 4259a7 shufps xmm0, xmm0, 0x55
    x1 = addss(x1, x0);                                            // 4259ab addss xmm1, xmm0
    x0 = x1;                                                       // 4259af movaps xmm0, xmm1
    x1 = x5;                                                       // 4259b2 movaps xmm1, xmm5
    x1 = addps(x1, x6);                                            // 4259b5 addps xmm1, xmm6
    x6 = x1;                                                       // 4259b8 movaps xmm6, xmm1
    x1 = x7;                                                       // 4259bb movaps xmm1, xmm7
    x1 = cmpltps(x1, x5);                                          // 4259be cmpltps xmm1, xmm5
    st(m, esp + 0x30, x1);                                         // 4259c2 movaps [esp + 0x30], xmm1
    edx = ld32(m, esp + 0x34);                                     // 4259c7 mov edx, [esp + 0x34]
    esi = ld32(m, esp + 0x38);                                     // 4259cb mov esi, [esp + 0x38]
    esi = esi + edx;                                               // 4259cf add esi, edx
    esi = esi + ld32(m, esp + 0x30);                               // 4259d1 add esi, [esp + 0x30]
    x2 = addps(x2, x4);                                            // 4259d5 addps xmm2, xmm4
    x0 = shufps(x0, x0, 0x00);                                     // 4259d8 shufps xmm0, xmm0, 0
    x4 = x2;                                                       // 4259dc movaps xmm4, xmm2
    fl = f_cmp(esi, 0xfffffffdu);                                  // 4259df cmp esi, -3
    if (fl.ne()) goto L_004259ec;                                  // 4259e2 jne 0x4259ec
    st32(m, esp + 0x1c, 0x1u);                                     // 4259e4 mov [esp + 0x1c], 1
L_004259ec:
    x1 = ld(m, esp + 0x20);                                        // 4259ec movaps xmm1, [esp + 0x20]
    x1 = cmpltps(x1, x0);                                          // 4259f1 cmpltps xmm1, xmm0
    st(m, esp + 0x30, x1);                                         // 4259f5 movaps [esp + 0x30], xmm1
    edx = ld32(m, esp + 0x34);                                     // 4259fa mov edx, [esp + 0x34]
    esi = ld32(m, esp + 0x38);                                     // 4259fe mov esi, [esp + 0x38]
    esi = esi + edx;                                               // 425a02 add esi, edx
    esi = esi + ld32(m, esp + 0x30);                               // 425a04 add esi, [esp + 0x30]
    fl = f_cmp(esi, 0xfffffffdu);                                  // 425a08 cmp esi, -3
    if (fl.ne()) goto L_00425a15;                                  // 425a0b jne 0x425a15
    st32(m, esp + 0x18, 0x1u);                                     // 425a0d mov [esp + 0x18], 1
L_00425a15:
    ecx = ecx - 0xffffff80u;                                       // 425a15 sub ecx, -0x80
    edi = edi + 0x70u;                                             // 425a18 add edi, 0x70
    { uint32_t a_ = ebx, b_ = 0x1u, r_ = a_ - b_; fl = f_sub(a_, b_, r_); ebx = r_; } // 425a1b sub ebx, 1
    if (fl.ne()) goto L_00425970;                                  // 425a1e jne 0x425970
    fl = f_cmp(ld32(m, esp + 0x1c), ebx);                          // 425a24 cmp [esp + 0x1c], ebx
    if (fl.ne()) goto L_00425a34;                                  // 425a28 jne 0x425a34
    fl = f_cmp(ld32(m, esp + 0x18), ebx);                          // 425a2a cmp [esp + 0x18], ebx
    if (fl.e()) goto L_00425b08;                                   // 425a2e je 0x425b08
L_00425a34:
    edx = ld32(m, eax + 0x3fc);                                    // 425a34 mov edx, [eax + 0x3fc]
    x0 = cvtsi2ss(x0, int32_t(edx));                               // 425a3a cvtsi2ss xmm0, edx
    x0 = shufps(x0, x0, 0x00);                                     // 425a3e shufps xmm0, xmm0, 0
    x6 = divps(x6, x0);                                            // 425a42 divps xmm6, xmm0
    x0 = x7;                                                       // 425a45 movaps xmm0, xmm7
    x0 = cmpltps(x0, x6);                                          // 425a48 cmpltps xmm0, xmm6
    st(m, esp + 0x30, x0);                                         // 425a4c movaps [esp + 0x30], xmm0
    ecx = ld32(m, esp + 0x34);                                     // 425a51 mov ecx, [esp + 0x34]
    esi = ld32(m, esp + 0x38);                                     // 425a55 mov esi, [esp + 0x38]
    esi = esi + ecx;                                               // 425a59 add esi, ecx
    esi = esi + ld32(m, esp + 0x30);                               // 425a5b add esi, [esp + 0x30]
    fl = f_cmp(esi, 0xfffffffdu);                                  // 425a5f cmp esi, -3
    if (fl.ne()) goto L_00425a67;                                  // 425a62 jne 0x425a67
    x6 = x7;                                                       // 425a64 movaps xmm6, xmm7
L_00425a67:
    x2 = X::v(0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000);     // 425a67 movaps xmm2, [0x92dc20]
    x0 = x4;                                                       // 425a6e movaps xmm0, xmm4
    x0 = mulps(x0, x4);                                            // 425a71 mulps xmm0, xmm4
    x1 = x0;                                                       // 425a74 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x99);                                     // 425a77 shufps xmm1, xmm0, 0x99
    x0 = addss(x0, x1);                                            // 425a7b addss xmm0, xmm1
    x1 = shufps(x1, x1, 0x55);                                     // 425a7f shufps xmm1, xmm1, 0x55
    x0 = addss(x0, x1);                                            // 425a83 addss xmm0, xmm1
    x0 = shufps(x0, x0, 0x00);                                     // 425a87 shufps xmm0, xmm0, 0
    x1 = sqrtps(x0);                                               // 425a8b sqrtps xmm1, xmm0
    x4 = divps(x4, x1);                                            // 425a8e divps xmm4, xmm1
    x0 = cmpneqps(x0, x2);                                         // 425a91 cmpneqps xmm0, xmm2
    x2 = X{};                                                      // 425a95 xorps xmm2, xmm2
    x1 = cmpneqps(x1, x2);                                         // 425a98 cmpneqps xmm1, xmm2
    x4 = andps(x4, x1);                                            // 425a9c andps xmm4, xmm1
    x1 = X::v(0x7fc00000, 0x7fc00000, 0x7fc00000, 0x7fc00000);     // 425a9f movaps xmm1, [0x92dc30]
    x4 = andps(x4, x0);                                            // 425aa6 andps xmm4, xmm0
    x0 = andnps(x0, x1);                                           // 425aa9 andnps xmm0, xmm1
    x4 = orps(x4, x0);                                             // 425aac orps xmm4, xmm0
    ecx = 0u;                                                      // 425aaf xor ecx, ecx
    x0 = sqrtps(x6);                                               // 425ab1 sqrtps xmm0, xmm6
    x4 = mulps(x4, x0);                                            // 425ab4 mulps xmm4, xmm0
    fl = f_test(edx, edx);                                         // 425ab7 test edx, edx
    if (fl.le()) goto L_00425ad0;                                  // 425ab9 jle 0x425ad0
    esi = eax + 0x410;                                             // 425abb lea esi, [eax + 0x410]
L_00425ac1:
    st(m, esi, x4);                                                // 425ac1 movaps [esi], xmm4
    ecx = ecx + 1u;                                                // 425ac4 inc ecx
    esi = esi - 0xffffff80u;                                       // 425ac5 sub esi, -0x80
    fl = f_cmp(ecx, ld32(m, eax + 0x3fc));                         // 425ac8 cmp ecx, [eax + 0x3fc]
    if (fl.l()) goto L_00425ac1;                                   // 425ace jl 0x425ac1
L_00425ad0:
    ecx = 0u;                                                      // 425ad0 xor ecx, ecx
    fl = f_cmp(ld32(m, eax + 0x3fc), ecx);                         // 425ad2 cmp [eax + 0x3fc], ecx
    if (fl.le()) goto L_00425af6;                                  // 425ad8 jle 0x425af6
    edx = eax + 0xe90;                                             // 425ada lea edx, [eax + 0xe90]
L_00425ae0:
    x0 = ld(m, 0xd61670);                                          // 425ae0 movaps xmm0, [0xd61670]
    st(m, edx, x0);                                                // 425ae7 movaps [edx], xmm0
    ecx = ecx + 1u;                                                // 425aea inc ecx
    edx = edx + 0x70u;                                             // 425aeb add edx, 0x70
    fl = f_cmp(ecx, ld32(m, eax + 0x3fc));                         // 425aee cmp ecx, [eax + 0x3fc]
    if (fl.l()) goto L_00425ae0;                                   // 425af4 jl 0x425ae0
L_00425af6:
    esi = eax + 0x380;                                             // 425af6 lea esi, [eax + 0x380]
    k.push(esi);                                                   // 425afc push esi
    k.call(0x00423b80);  // FUN_00423b80                           // 425afd call 0x423b80
    k.push(esi);                                                   // 425b02 push esi
    k.call(0x00423df0);  // FUN_00423df0                           // 425b03 call 0x423df0
L_00425b08:
    edi = k.pop();                                                 // 425b08 pop edi
    esi = k.pop();                                                 // 425b09 pop esi
    ebx = k.pop();                                                 // 425b0a pop ebx
    esp = ebp;                                                     // 425b0b mov esp, ebp
    ebp = k.pop();                                                 // 425b0d pop ebp
    return k.ret(4);                                               // 425b0e ret 4
}

// 00425b20 FUN_00425b20: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00425b20, Fn_00425b20, "math", 0) {  // outputs: what callers read (it calls other groups' code)
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 425b20 push ebp
    ebp = esp;                                                     // 425b21 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 425b23 and esp, 0xfffffff0
    esp = esp - 0x1d4u;                                            // 425b26 sub esp, 0x1d4
    eax = ld32(m, 0x9b45e4);                                       // 425b2c mov eax, [0x9b45e4]
    eax = eax ^ esp;                                               // 425b31 xor eax, esp
    st32(m, esp + 0x1d0, eax);                                     // 425b33 mov [esp + 0x1d0], eax
    k.push(ebx);                                                   // 425b3a push ebx
    ebx = ld32(m, ebp + 0x8);                                      // 425b3b mov ebx, [ebp + 8]
    k.push(esi);                                                   // 425b3e push esi
    esi = ebx + 0x380;                                             // 425b3f lea esi, [ebx + 0x380]
    k.push(edi);                                                   // 425b45 push edi
    ecx = ebx + 0xa5a0;                                            // 425b46 lea ecx, [ebx + 0xa5a0]
    st32(m, esp + 0x30, esi);                                      // 425b4c mov [esp + 0x30], esi
    k.call(0x00422300);  // FUN_00422300                           // 425b50 call 0x422300
    x0 = X{};                                                      // 425b55 xorps xmm0, xmm0
    eax = 0u;                                                      // 425b58 xor eax, eax
    st32(m, ebx + 0x374, eax);                                     // 425b5a mov [ebx + 0x374], eax
    st32(m, ebx + 0x378, eax);                                     // 425b60 mov [ebx + 0x378], eax
    st32(m, ebx + 0x304, x0.u[0]);                                 // 425b66 movss [ebx + 0x304], xmm0
    st32(m, ebx + 0x300, x0.u[0]);                                 // 425b6e movss [ebx + 0x300], xmm0
    st32(m, ebx + 0xa834, eax);                                    // 425b76 mov [ebx + 0xa834], eax
    st32(m, ebx + 0xa880, x0.u[0]);                                // 425b7c movss [ebx + 0xa880], xmm0
    x0 = ld(m, 0xd61670);                                          // 425b84 movaps xmm0, [0xd61670]
    ecx = esp + 0xc0;                                              // 425b8b lea ecx, [esp + 0xc0]
    st(m, ebx + 0xa850, x0);                                       // 425b92 movaps [ebx + 0xa850], xmm0
    k.call(0x00422350);  // FUN_00422350                           // 425b99 call 0x422350
    x0 = X::ss(ld32(m, ebp + 0xc));                                // 425b9e movss xmm0, [ebp + 0xc]
    x0 = shufps(x0, x0, 0x00);                                     // 425ba3 shufps xmm0, xmm0, 0
    st(m, esp + 0x90, x0);                                         // 425ba7 movaps [esp + 0x90], xmm0
    x0 = ld(m, ebx + 0xa840);                                      // 425baf movaps xmm0, [ebx + 0xa840]
    eax = 0u;                                                      // 425bb6 xor eax, eax
    ecx = 0u;                                                      // 425bb8 xor ecx, ecx
    edx = 0u;                                                      // 425bba xor edx, edx
    st16(m, esp + 0x1d4, (eax & 0xffff));                          // 425bbc mov word ptr [esp + 0x1d4], ax
    st16(m, esp + 0x1d6, (ecx & 0xffff));                          // 425bc4 mov word ptr [esp + 0x1d6], cx
    st16(m, esp + 0x1d8, (edx & 0xffff));                          // 425bcc mov word ptr [esp + 0x1d8], dx
    st16(m, esp + 0x1da, (eax & 0xffff));                          // 425bd4 mov word ptr [esp + 0x1da], ax
    x0 = shufps(x0, x0, 0x55);                                     // 425bdc shufps xmm0, xmm0, 0x55
    k.call(0x00401000);  // FUN_00401000                           // 425be0 call 0x401000
    ecx = ebx + 0xa4;                                              // 425be5 lea ecx, [ebx + 0xa4]
    edx = ebx + 0x1f0;                                             // 425beb lea edx, [ebx + 0x1f0]
    st32(m, esp + 0x7c, x0.u[0]);                                  // 425bf1 movss [esp + 0x7c], xmm0
    st32(m, esp + 0x58, ecx);                                      // 425bf7 mov [esp + 0x58], ecx
    st32(m, esp + 0x5c, edx);                                      // 425bfb mov [esp + 0x5c], edx
    edi = ebx + 0xa768;                                            // 425bff lea edi, [ebx + 0xa768]
    st32(m, esp + 0x18, 0x4u);                                     // 425c05 mov [esp + 0x18], 4
L_00425c0d:
    eax = ld32(m, edi - 0x4);                                      // 425c0d mov eax, [edi - 4]
    x0 = X::ss(ld32(m, edi + 0x5c));                               // 425c10 movss xmm0, [edi + 0x5c]
    x2 = X::ss(ld32(m, edi + 0x50));                               // 425c15 movss xmm2, [edi + 0x50]
    ecx = ld32(m, edi);                                            // 425c1a mov ecx, [edi]
    x4 = X::ss(ld32(m, edi + 0x54));                               // 425c1c movss xmm4, [edi + 0x54]
    edx = ld32(m, edi + 0x4);                                      // 425c21 mov edx, [edi + 4]
    esi = ld32(m, edi + 0x8);                                      // 425c24 mov esi, [edi + 8]
    x6 = X::ss(ld32(m, edi + 0x58));                               // 425c27 movss xmm6, [edi + 0x58]
    st32(m, esp + 0x80, eax);                                      // 425c2c mov [esp + 0x80], eax
    eax = eax + 0x8u;                                              // 425c33 add eax, 8
    x0 = shufps(x0, x0, 0x00);                                     // 425c36 shufps xmm0, xmm0, 0
    st(m, esp + 0x60, x0);                                         // 425c3a movaps [esp + 0x60], xmm0
    x0 = ld(m, ebx + 0xa840);                                      // 425c3f movaps xmm0, [ebx + 0xa840]
    eax = eax << 7;                                                // 425c46 shl eax, 7
    x2 = shufps(x2, x2, 0x00);                                     // 425c49 shufps xmm2, xmm2, 0
    st32(m, esp + 0x84, ecx);                                      // 425c4d mov [esp + 0x84], ecx
    x1 = x0;                                                       // 425c54 movaps xmm1, xmm0
    x1 = mulps(x1, x2);                                            // 425c57 mulps xmm1, xmm2
    x2 = ld(m, eax + ebx);                                         // 425c5a movaps xmm2, [eax + ebx]
    ecx = ecx + 0x8u;                                              // 425c5e add ecx, 8
    x3 = x2;                                                       // 425c61 movaps xmm3, xmm2
    x3 = addps(x3, x1);                                            // 425c64 addps xmm3, xmm1
    ecx = ecx << 7;                                                // 425c67 shl ecx, 7
    x4 = shufps(x4, x4, 0x00);                                     // 425c6a shufps xmm4, xmm4, 0
    st(m, esp + 0xd0, x3);                                         // 425c6e movaps [esp + 0xd0], xmm3
    x3 = x0;                                                       // 425c76 movaps xmm3, xmm0
    x3 = mulps(x3, x4);                                            // 425c79 mulps xmm3, xmm4
    x4 = ld(m, ecx + ebx);                                         // 425c7c movaps xmm4, [ecx + ebx]
    st32(m, esp + 0x88, edx);                                      // 425c80 mov [esp + 0x88], edx
    x5 = x4;                                                       // 425c87 movaps xmm5, xmm4
    x5 = addps(x5, x3);                                            // 425c8a addps xmm5, xmm3
    edx = edx + 0x8u;                                              // 425c8d add edx, 8
    x6 = shufps(x6, x6, 0x00);                                     // 425c90 shufps xmm6, xmm6, 0
    st(m, esp + 0xf0, x5);                                         // 425c94 movaps [esp + 0xf0], xmm5
    edx = edx << 7;                                                // 425c9c shl edx, 7
    x5 = x0;                                                       // 425c9f movaps xmm5, xmm0
    x0 = mulps(x0, ld(m, esp + 0x60));                             // 425ca2 mulps xmm0, [esp + 0x60]
    x5 = mulps(x5, x6);                                            // 425ca7 mulps xmm5, xmm6
    x6 = ld(m, edx + ebx);                                         // 425caa movaps xmm6, [edx + ebx]
    st32(m, esp + 0x8c, esi);                                      // 425cae mov [esp + 0x8c], esi
    esi = esi + 0x8u;                                              // 425cb5 add esi, 8
    eax = esp + 0xe0;                                              // 425cb8 lea eax, [esp + 0xe0]
    x7 = x6;                                                       // 425cbf movaps xmm7, xmm6
    x7 = addps(x7, x5);                                            // 425cc2 addps xmm7, xmm5
    ecx = esp + 0x1d4;                                             // 425cc5 lea ecx, [esp + 0x1d4]
    esi = esi << 7;                                                // 425ccc shl esi, 7
    st32(m, esp + 0x20, eax);                                      // 425ccf mov [esp + 0x20], eax
    eax = ld32(m, esp + 0x5c);                                     // 425cd3 mov eax, [esp + 0x5c]
    st(m, esp + 0x110, x7);                                        // 425cd7 movaps [esp + 0x110], xmm7
    x7 = ld(m, esi + ebx);                                         // 425cdf movaps xmm7, [esi + ebx]
    st32(m, esp + 0x38, ecx);                                      // 425ce3 mov [esp + 0x38], ecx
    ecx = ld32(m, esp + 0x58);                                     // 425ce7 mov ecx, [esp + 0x58]
    st(m, esp + 0x60, x7);                                         // 425ceb movaps [esp + 0x60], xmm7
    x2 = subps(x2, x1);                                            // 425cf0 subps xmm2, xmm1
    x1 = ld(m, esp + 0x60);                                        // 425cf3 movaps xmm1, [esp + 0x60]
    st32(m, esp + 0x40, eax);                                      // 425cf8 mov [esp + 0x40], eax
    eax = eax + 0x40u;                                             // 425cfc add eax, 0x40
    st32(m, esp + 0x3c, ecx);                                      // 425cff mov [esp + 0x3c], ecx
    edx = esp + 0x80;                                              // 425d03 lea edx, [esp + 0x80]
    ecx = ecx + 0x10u;                                             // 425d0a add ecx, 0x10
    x7 = addps(x7, x0);                                            // 425d0d addps xmm7, xmm0
    x4 = subps(x4, x3);                                            // 425d10 subps xmm4, xmm3
    x6 = subps(x6, x5);                                            // 425d13 subps xmm6, xmm5
    x1 = subps(x1, x0);                                            // 425d16 subps xmm1, xmm0
    st(m, esp + 0x130, x7);                                        // 425d19 movaps [esp + 0x130], xmm7
    st(m, esp + 0xe0, x2);                                         // 425d21 movaps [esp + 0xe0], xmm2
    st(m, esp + 0x100, x4);                                        // 425d29 movaps [esp + 0x100], xmm4
    st(m, esp + 0x120, x6);                                        // 425d31 movaps [esp + 0x120], xmm6
    st(m, esp + 0x140, x1);                                        // 425d39 movaps [esp + 0x140], xmm1
    st32(m, esp + 0x34, edx);                                      // 425d41 mov [esp + 0x34], edx
    st32(m, esp + 0x1c, 0x0u);                                     // 425d45 mov [esp + 0x1c], 0
    st32(m, esp + 0x5c, eax);                                      // 425d4d mov [esp + 0x5c], eax
    st32(m, esp + 0x58, ecx);                                      // 425d51 mov [esp + 0x58], ecx
L_00425d55:
    fl = f_test8(ld8(m, 0x9e30e0), 0x1u);                          // 425d55 test byte ptr [0x9e30e0], 1
    esi = ld32(m, esp + 0x1c);                                     // 425d5c mov esi, [esp + 0x1c]
    eax = ld32(m, esp + 0x20);                                     // 425d60 mov eax, [esp + 0x20]
    x0 = ld(m, eax);                                               // 425d64 movaps xmm0, [eax]
    esi = esp + esi + 0x190;                                       // 425d67 lea esi, [esp + esi + 0x190]
    st(m, esp + 0x60, x0);                                         // 425d6e movaps [esp + 0x60], xmm0
    st(m, esi, x0);                                                // 425d73 movaps [esi], xmm0
    if (fl.ne()) goto L_00425d89;                                  // 425d76 jne 0x425d89
    st32(m, 0x9e30e0, ld32(m, 0x9e30e0) | 0x1u);                   // 425d78 or [0x9e30e0], 1
    st32(m, 0x9e30dc, 0x0u);                                       // 425d7f mov [0x9e30dc], 0
L_00425d89:
    eax = 0u;                                                      // 425d89 xor eax, eax
    fl = f_cmp(ld32(m, 0x9e30dc), eax);                            // 425d8b cmp [0x9e30dc], eax
    if (fl.be()) goto L_00425dcc;                                  // 425d91 jbe 0x425dcc
    ecx = ld32(m, 0xa95844);                                       // 425d93 mov ecx, [0xa95844]
L_00425d99:
    fl = f_cmp(ld32(m, eax * 4 + 0x9e30d4), 0x0u);                 // 425d99 cmp [eax*4 + 0x9e30d4], 0
    if (fl.e()) goto L_00425dc3;                                   // 425da1 je 0x425dc3
    edx = ld32(m, ecx + 0x4);                                      // 425da3 mov edx, [ecx + 4]
    edx = ld32(m, ecx + edx * 4 + 0x1c);                           // 425da6 mov edx, [ecx + edx*4 + 0x1c]
    ecx = ld32(m, eax * 4 + 0x9e30d4);                             // 425daa mov ecx, [eax*4 + 0x9e30d4]
    fl = f_cmp(edx, ld32(m, ecx + 0x44a8));                        // 425db1 cmp edx, [ecx + 0x44a8]
    if (fl.e()) goto L_00425f34;                                   // 425db7 je 0x425f34
    ecx = ld32(m, 0xa95844);                                       // 425dbd mov ecx, [0xa95844]
L_00425dc3:
    eax = eax + 1u;                                                // 425dc3 inc eax
    fl = f_cmp(eax, ld32(m, 0x9e30dc));                            // 425dc4 cmp eax, [0x9e30dc]
    if (fl.b()) goto L_00425d99;                                   // 425dca jb 0x425d99
L_00425dcc:
    eax = 0u;                                                      // 425dcc xor eax, eax
L_00425dce:
    ecx = ld32(m, esp + 0x1c);                                     // 425dce mov ecx, [esp + 0x1c]
    k.push(esi);                                                   // 425dd2 push esi
    ecx = esp + ecx + 0x154;                                       // 425dd3 lea ecx, [esp + ecx + 0x154]
    k.push(eax);                                                   // 425dda push eax
    k.call(0x007c5e80);  // FUN_007c5e80                           // 425ddb call 0x7c5e80
    fl = f_test(eax, eax);                                         // 425de0 test eax, eax
    if (fl.e()) goto L_00425f6c;                                   // 425de2 je 0x425f6c
    eax = ld32(m, esp + 0x20);                                     // 425de8 mov eax, [esp + 0x20]
    x1 = ld(m, eax);                                               // 425dec movaps xmm1, [eax]
    x0 = ld(m, esi);                                               // 425def movaps xmm0, [esi]
    x2 = x1;                                                       // 425df2 movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0x55);                                     // 425df5 shufps xmm2, xmm1, 0x55
    x0 = shufps(x0, x0, 0x55);                                     // 425df9 shufps xmm0, xmm0, 0x55
    x2 = cmpltps(x2, x0);                                          // 425dfd cmpltps xmm2, xmm0
    st(m, esp + 0xa0, x2);                                         // 425e01 movaps [esp + 0xa0], xmm2
    ecx = ld32(m, esp + 0xa4);                                     // 425e09 mov ecx, [esp + 0xa4]
    edx = ld32(m, esp + 0xa8);                                     // 425e10 mov edx, [esp + 0xa8]
    edx = edx + ecx;                                               // 425e17 add edx, ecx
    edx = edx + ld32(m, esp + 0xa0);                               // 425e19 add edx, [esp + 0xa0]
    fl = f_cmp(edx, 0xfffffffdu);                                  // 425e20 cmp edx, -3
    if (fl.ne()) goto L_00425f6c;                                  // 425e23 jne 0x425f6c
    eax = ld32(m, esp + 0x34);                                     // 425e29 mov eax, [esp + 0x34]
    ecx = ld32(m, eax);                                            // 425e2d mov ecx, [eax]
    eax = ld32(m, esp + 0x1c);                                     // 425e2f mov eax, [esp + 0x1c]
    edx = ecx;                                                     // 425e33 mov edx, ecx
    edx = edx << 7;                                                // 425e35 shl edx, 7
    x0 = ld(m, edx + ebx + 0x410);                                 // 425e38 movaps xmm0, [edx + ebx + 0x410]
    x0 = mulps(x0, x0);                                            // 425e40 mulps xmm0, xmm0
    x1 = x0;                                                       // 425e43 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x66);                                     // 425e46 shufps xmm1, xmm0, 0x66
    x0 = addss(x0, x1);                                            // 425e4a addss xmm0, xmm1
    x2 = x1;                                                       // 425e4e movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0x55);                                     // 425e51 shufps xmm2, xmm1, 0x55
    x1 = ld(m, esp + eax + 0x150);                                 // 425e55 movaps xmm1, [esp + eax + 0x150]
    x0 = addss(x0, x2);                                            // 425e5d addss xmm0, xmm2
    x0 = shufps(x0, x0, 0x00);                                     // 425e61 shufps xmm0, xmm0, 0
    x0 = sqrtps(x0);                                               // 425e65 sqrtps xmm0, xmm0
    st32(m, esp + 0x78, x0.u[0]);                                  // 425e68 movss [esp + 0x78], xmm0
    x0 = ld(m, esi);                                               // 425e6e movaps xmm0, [esi]
    x0 = subps(x0, ld(m, esp + 0x60));                             // 425e71 subps xmm0, [esp + 0x60]
    eax = esp + eax + 0x150;                                       // 425e76 lea eax, [esp + eax + 0x150]
    x0 = mulps(x0, x1);                                            // 425e7d mulps xmm0, xmm1
    st(m, esp + 0xa0, x1);                                         // 425e80 movaps [esp + 0xa0], xmm1
    x1 = x0;                                                       // 425e88 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x99);                                     // 425e8b shufps xmm1, xmm0, 0x99
    edx = esp + 0xb0;                                              // 425e8f lea edx, [esp + 0xb0]
    k.push(edx);                                                   // 425e96 push edx
    x2 = x1;                                                       // 425e97 movaps xmm2, xmm1
    x0 = addss(x0, x1);                                            // 425e9a addss xmm0, xmm1
    x2 = shufps(x2, x1, 0x55);                                     // 425e9e shufps xmm2, xmm1, 0x55
    k.push(eax);                                                   // 425ea2 push eax
    x0 = addss(x0, x2);                                            // 425ea3 addss xmm0, xmm2
    x0 = shufps(x0, x0, 0x00);                                     // 425ea7 shufps xmm0, xmm0, 0
    k.push(ebx);                                                   // 425eab push ebx
    edx = esi;                                                     // 425eac mov edx, esi
    st(m, esp + 0xbc, x0);                                         // 425eae movaps [esp + 0xbc], xmm0
    k.call(0x004263a0);  // FUN_004263a0                           // 425eb6 call 0x4263a0
    x1 = ld(m, ebx + 0xa850);                                      // 425ebb movaps xmm1, [ebx + 0xa850]
    x0 = ld(m, esp + 0xa0);                                        // 425ec2 movaps xmm0, [esp + 0xa0]
    x0 = addps(x0, x1);                                            // 425eca addps xmm0, xmm1
    x1 = X::ss(ld32(m, esp + 0x78));                               // 425ecd movss xmm1, [esp + 0x78]
    st32(m, ebx + 0xa834, 0x1u);                                   // 425ed3 mov [ebx + 0xa834], 1
    st(m, ebx + 0xa850, x0);                                       // 425edd movaps [ebx + 0xa850], xmm0
    x0 = X::ss(0x41a00000u);                                       // 425ee4 movss xmm0, [0x935aac]
    fl = f_comiss(x0.f(0), x1.f(0));                               // 425eec comiss xmm0, xmm1
    if (fl.be()) goto L_00425f3b;                                  // 425eef jbe 0x425f3b
    x0 = cvtss2sd(x0, X::ss(ld32(m, esp + 0x7c)));                 // 425ef1 cvtss2sd xmm0, [esp + 0x7c]
    x0 = mulsd(x0, sd(0x3fc70a3d78000000ull));                     // 425ef7 mulsd xmm0, [0x977d48]
    x2 = sd(0x3f947ae140000000ull);                                // 425eff movsd xmm2, [0x977d40]
    x0 = addsd(x0, x2);                                            // 425f07 addsd xmm0, xmm2
    x0 = cvtpd2ps(x0);                                             // 425f0b cvtpd2ps xmm0, xmm0
    x3 = cvtss2sd(x3, x0);                                         // 425f0f cvtss2sd xmm3, xmm0
    x2 = subsd(x2, x3);                                            // 425f13 subsd xmm2, xmm3
    x1 = cvtps2pd(x1);                                             // 425f17 cvtps2pd xmm1, xmm1
    x2 = mulsd(x2, x1);                                            // 425f1a mulsd xmm2, xmm1
    x2 = divsd(x2, sd(0x4034000000000000ull));                     // 425f1e divsd xmm2, [0x9779a0]
    x0 = cvtss2sd(x0, x0);                                         // 425f26 cvtss2sd xmm0, xmm0
    x2 = addsd(x2, x0);                                            // 425f2a addsd xmm2, xmm0
    x0 = cvtpd2ps(x2);                                             // 425f2e cvtpd2ps xmm0, xmm2
    goto L_00425f43;                                               // 425f32 jmp 0x425f43
L_00425f34:
    eax = ecx;                                                     // 425f34 mov eax, ecx
    goto L_00425dce;                                               // 425f36 jmp 0x425dce
L_00425f3b:
    x0 = X::ss(0x3ca3d70au);                                       // 425f3b movss xmm0, [0x977408]
L_00425f43:
    eax = ld32(m, esp + 0x34);                                     // 425f43 mov eax, [esp + 0x34]
    ecx = ld32(m, esp + 0x38);                                     // 425f47 mov ecx, [esp + 0x38]
    eax = ld32(m, eax);                                            // 425f4b mov eax, [eax]
    edx = (edx & 0xffffff00) | (uint32_t(ld8(m, ecx + 0x1)) & 0xff); // 425f4d mov dl, byte ptr [ecx + 1]
    st32(m, ebx + eax * 4 + 0x398, x0.u[0]);                       // 425f50 movss [ebx + eax*4 + 0x398], xmm0
    eax = ld32(m, esp + 0x3c);                                     // 425f59 mov eax, [esp + 0x3c]
    edx = (edx & 0xffffff00) | (uint32_t((edx & 0xff) & 0xfu) & 0xff); // 425f5d and dl, 0xf
    st8(m, ebx + 0x2fc, (edx & 0xff));                             // 425f60 mov byte ptr [ebx + 0x2fc], dl
    st32(m, eax, 0x1u);                                            // 425f66 mov [eax], 1
L_00425f6c:
    eax = ld32(m, esp + 0x40);                                     // 425f6c mov eax, [esp + 0x40]
    x0 = ld(m, esi);                                               // 425f70 movaps xmm0, [esi]
    st32(m, esp + 0x38, ld32(m, esp + 0x38) + 0x2u);               // 425f73 add [esp + 0x38], 2
    st32(m, esp + 0x20, ld32(m, esp + 0x20) + 0x20u);              // 425f78 add [esp + 0x20], 0x20
    st(m, eax - 0x100, x0);                                        // 425f7d movaps [eax - 0x100], xmm0
    x0 = subps(x0, ld(m, esp + 0x60));                             // 425f84 subps xmm0, [esp + 0x60]
    st(m, eax, x0);                                                // 425f89 movaps [eax], xmm0
    eax = eax + 0x10u;                                             // 425f8c add eax, 0x10
    st32(m, esp + 0x40, eax);                                      // 425f8f mov [esp + 0x40], eax
    eax = ld32(m, esp + 0x1c);                                     // 425f93 mov eax, [esp + 0x1c]
    ecx = 0x4u;                                                    // 425f97 mov ecx, 4
    st32(m, esp + 0x3c, ld32(m, esp + 0x3c) + ecx);                // 425f9c add [esp + 0x3c], ecx
    st32(m, esp + 0x34, ld32(m, esp + 0x34) + ecx);                // 425fa0 add [esp + 0x34], ecx
    eax = eax + 0x10u;                                             // 425fa4 add eax, 0x10
    fl = f_cmp(eax, 0x40u);                                        // 425fa7 cmp eax, 0x40
    st32(m, esp + 0x1c, eax);                                      // 425faa mov [esp + 0x1c], eax
    if (fl.l()) goto L_00425d55;                                   // 425fae jl 0x425d55
    edi = edi + 0x10u;                                             // 425fb4 add edi, 0x10
    { uint32_t a_ = ld32(m, esp + 0x18), b_ = 0x1u, r_ = a_ - b_; fl = f_sub(a_, b_, r_); st32(m, esp + 0x18, r_); } // 425fb7 sub [esp + 0x18], 1
    if (fl.ne()) goto L_00425c0d;                                  // 425fbc jne 0x425c0d
    esi = ld32(m, esp + 0x30);                                     // 425fc2 mov esi, [esp + 0x30]
    ecx = esp + 0xb0;                                              // 425fc6 lea ecx, [esp + 0xb0]
    k.call(0x00422350);  // FUN_00422350                           // 425fcd call 0x422350
    ecx = 0u;                                                      // 425fd2 xor ecx, ecx
    fl = f_cmp(ld32(m, ebx + 0xa834), ecx);                        // 425fd4 cmp [ebx + 0xa834], ecx
    if (fl.e()) goto L_00426386;                                   // 425fda je 0x426386
    x0 = ld(m, esp + 0xc0);                                        // 425fe0 movaps xmm0, [esp + 0xc0]
    x7 = X::ss(0x3f800000u);                                       // 425fe8 movss xmm7, [0x931f24]
    x6 = X::ss(ld32(m, 0x9cd9e4));                                 // 425ff0 movss xmm6, [0x9cd9e4]
    x0 = mulps(x0, x0);                                            // 425ff8 mulps xmm0, xmm0
    x1 = x0;                                                       // 425ffb movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x66);                                     // 425ffe shufps xmm1, xmm0, 0x66
    x0 = addss(x0, x1);                                            // 426002 addss xmm0, xmm1
    x2 = x1;                                                       // 426006 movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0x55);                                     // 426009 shufps xmm2, xmm1, 0x55
    x0 = addss(x0, x2);                                            // 42600d addss xmm0, xmm2
    x0 = shufps(x0, x0, 0x00);                                     // 426011 shufps xmm0, xmm0, 0
    x7 = shufps(x7, x7, 0x00);                                     // 426015 shufps xmm7, xmm7, 0
    x0 = sqrtps(x0);                                               // 426019 sqrtps xmm0, xmm0
    x6 = shufps(x6, x6, 0x00);                                     // 42601c shufps xmm6, xmm6, 0
    st(m, esp + 0x60, x7);                                         // 426020 movaps [esp + 0x60], xmm7
    st32(m, ebx + 0x374, 0x1u);                                    // 426025 mov [ebx + 0x374], 1
    st32(m, esp + 0x18, x0.u[0]);                                  // 42602f movss [esp + 0x18], xmm0
    fl = f_cmp(ld32(m, ebx + 0xa87c), ecx);                        // 426035 cmp [ebx + 0xa87c], ecx
    if (fl.e()) goto L_00426069;                                   // 42603b je 0x426069
    fl = f_cmp(ld32(m, esi + 0x7c), ecx);                          // 42603d cmp [esi + 0x7c], ecx
    if (fl.le()) goto L_00426069;                                  // 426040 jle 0x426069
    eax = esi + 0x80;                                              // 426042 lea eax, [esi + 0x80]
    goto L_00426050;                                               // 426048 jmp 0x426050
    ebx = ebx;                                                     // 42604a lea ebx, [ebx]
L_00426050:
    x1 = ld(m, eax);                                               // 426050 movaps xmm1, [eax]
    x0 = ld(m, ebx + 0xa890);                                      // 426053 movaps xmm0, [ebx + 0xa890]
    x0 = addps(x0, x1);                                            // 42605a addps xmm0, xmm1
    st(m, eax, x0);                                                // 42605d movaps [eax], xmm0
    ecx = ecx + 1u;                                                // 426060 inc ecx
    eax = eax - 0xffffff80u;                                       // 426061 sub eax, -0x80
    fl = f_cmp(ecx, ld32(m, esi + 0x7c));                          // 426064 cmp ecx, [esi + 0x7c]
    if (fl.l()) goto L_00426050;                                   // 426067 jl 0x426050
L_00426069:
    x0 = ld(m, ebx + 0xa850);                                      // 426069 movaps xmm0, [ebx + 0xa850]
    x1 = x0;                                                       // 426070 movaps xmm1, xmm0
    x1 = mulps(x1, x0);                                            // 426073 mulps xmm1, xmm0
    x2 = x1;                                                       // 426076 movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0x99);                                     // 426079 shufps xmm2, xmm1, 0x99
    x1 = addss(x1, x2);                                            // 42607d addss xmm1, xmm2
    x3 = x2;                                                       // 426081 movaps xmm3, xmm2
    x3 = shufps(x3, x2, 0x55);                                     // 426084 shufps xmm3, xmm2, 0x55
    x1 = addss(x1, x3);                                            // 426088 addss xmm1, xmm3
    x3 = X::v(0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000);     // 42608c movaps xmm3, [0x92dc20]
    x1 = shufps(x1, x1, 0x00);                                     // 426093 shufps xmm1, xmm1, 0
    x2 = sqrtps(x1);                                               // 426097 sqrtps xmm2, xmm1
    x0 = divps(x0, x2);                                            // 42609a divps xmm0, xmm2
    x1 = cmpneqps(x1, x3);                                         // 42609d cmpneqps xmm1, xmm3
    x5 = X{};                                                      // 4260a1 xorps xmm5, xmm5
    x2 = cmpneqps(x2, x5);                                         // 4260a4 cmpneqps xmm2, xmm5
    x0 = andps(x0, x2);                                            // 4260a8 andps xmm0, xmm2
    x2 = X::v(0x7fc00000, 0x7fc00000, 0x7fc00000, 0x7fc00000);     // 4260ab movaps xmm2, [0x92dc30]
    x0 = andps(x0, x1);                                            // 4260b2 andps xmm0, xmm1
    x1 = andnps(x1, x2);                                           // 4260b5 andnps xmm1, xmm2
    x2 = ld(m, esp + 0xb0);                                        // 4260b8 movaps xmm2, [esp + 0xb0]
    x0 = orps(x0, x1);                                             // 4260c0 orps xmm0, xmm1
    st(m, ebx + 0xa840, x0);                                       // 4260c3 movaps [ebx + 0xa840], xmm0
    x0 = ld(m, ebx + 0xa840);                                      // 4260ca movaps xmm0, [ebx + 0xa840]
    x0 = shufps(x0, x0, 0xc9);                                     // 4260d1 shufps xmm0, xmm0, 0xc9
    x1 = x0;                                                       // 4260d5 movaps xmm1, xmm0
    x4 = x1;                                                       // 4260d8 movaps xmm4, xmm1
    x4 = shufps(x4, x1, 0xc9);                                     // 4260db shufps xmm4, xmm1, 0xc9
    x0 = x2;                                                       // 4260df movaps xmm0, xmm2
    x0 = shufps(x0, x2, 0xd2);                                     // 4260e2 shufps xmm0, xmm2, 0xd2
    x3 = x0;                                                       // 4260e6 movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0xd2);                                     // 4260e9 shufps xmm3, xmm0, 0xd2
    x0 = mulps(x0, x1);                                            // 4260ed mulps xmm0, xmm1
    x1 = X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000);     // 4260f0 movaps xmm1, [0x92db80]
    x3 = mulps(x3, x4);                                            // 4260f7 mulps xmm3, xmm4
    x0 = subps(x0, x3);                                            // 4260fa subps xmm0, xmm3
    x0 = andps(x0, x1);                                            // 4260fd andps xmm0, xmm1
    x1 = x0;                                                       // 426100 movaps xmm1, xmm0
    x1 = mulps(x1, x0);                                            // 426103 mulps xmm1, xmm0
    x3 = x1;                                                       // 426106 movaps xmm3, xmm1
    x3 = shufps(x3, x1, 0x99);                                     // 426109 shufps xmm3, xmm1, 0x99
    x1 = addss(x1, x3);                                            // 42610d addss xmm1, xmm3
    x4 = x3;                                                       // 426111 movaps xmm4, xmm3
    x4 = shufps(x4, x3, 0x55);                                     // 426114 shufps xmm4, xmm3, 0x55
    x1 = addss(x1, x4);                                            // 426118 addss xmm1, xmm4
    x4 = X::v(0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000);     // 42611c movaps xmm4, [0x92dc20]
    x1 = shufps(x1, x1, 0x00);                                     // 426123 shufps xmm1, xmm1, 0
    x3 = sqrtps(x1);                                               // 426127 sqrtps xmm3, xmm1
    x1 = cmpneqps(x1, x4);                                         // 42612a cmpneqps xmm1, xmm4
    x0 = divps(x0, x3);                                            // 42612e divps xmm0, xmm3
    x4 = x3;                                                       // 426131 movaps xmm4, xmm3
    x4 = cmpneqps(x4, x5);                                         // 426134 cmpneqps xmm4, xmm5
    x4 = andps(x4, x0);                                            // 426138 andps xmm4, xmm0
    x0 = X::v(0x7fc00000, 0x7fc00000, 0x7fc00000, 0x7fc00000);     // 42613b movaps xmm0, [0x92dc30]
    x4 = andps(x4, x1);                                            // 426142 andps xmm4, xmm1
    x1 = andnps(x1, x0);                                           // 426145 andnps xmm1, xmm0
    x2 = mulps(x2, x2);                                            // 426148 mulps xmm2, xmm2
    x0 = x2;                                                       // 42614b movaps xmm0, xmm2
    x4 = orps(x4, x1);                                             // 42614e orps xmm4, xmm1
    x1 = x0;                                                       // 426151 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x66);                                     // 426154 shufps xmm1, xmm0, 0x66
    eax = ebx + 0xa5a0;                                            // 426158 lea eax, [ebx + 0xa5a0]
    x2 = x1;                                                       // 42615e movaps xmm2, xmm1
    k.push(eax);                                                   // 426161 push eax
    ecx = esp + 0xb4;                                              // 426162 lea ecx, [esp + 0xb4]
    x2 = shufps(x2, x1, 0x55);                                     // 426169 shufps xmm2, xmm1, 0x55
    x0 = addss(x0, x1);                                            // 42616d addss xmm0, xmm1
    x0 = addss(x0, x2);                                            // 426171 addss xmm0, xmm2
    k.push(ecx);                                                   // 426175 push ecx
    edi = esp + 0xc8;                                              // 426176 lea edi, [esp + 0xc8]
    st(m, esp + 0xc8, x4);                                         // 42617d movaps [esp + 0xc8], xmm4
    x5 = x0;                                                       // 426185 movaps xmm5, xmm0
    k.call(0x004223a0);  // FUN_004223a0                           // 426188 call 0x4223a0
    x1 = ld(m, eax);                                               // 42618d movaps xmm1, [eax]
    x5 = shufps(x5, x5, 0x00);                                     // 426190 shufps xmm5, xmm5, 0
    x0 = sqrtps(x5);                                               // 426194 sqrtps xmm0, xmm5
    x0 = mulps(x0, x6);                                            // 426197 mulps xmm0, xmm6
    x0 = mulps(x0, x4);                                            // 42619a mulps xmm0, xmm4
    x0 = mulps(x0, ld(m, esp + 0x90));                             // 42619d mulps xmm0, [esp + 0x90]
    x0 = divps(x0, x1);                                            // 4261a5 divps xmm0, xmm1
    st(m, ebx + 0xa810, x0);                                       // 4261a8 movaps [ebx + 0xa810], xmm0
    x0 = ld(m, ebx + 0x400);                                       // 4261af movaps xmm0, [ebx + 0x400]
    x1 = ld(m, ebx + 0xa5a0);                                      // 4261b6 movaps xmm1, [ebx + 0xa5a0]
    eax = ld32(m, 0xa95844);                                       // 4261bd mov eax, [0xa95844]
    st(m, esp + 0xa0, x0);                                         // 4261c2 movaps [esp + 0xa0], xmm0
    x0 = subps(x0, x1);                                            // 4261ca subps xmm0, xmm1
    x1 = ld(m, ebx + 0xa810);                                      // 4261cd movaps xmm1, [ebx + 0xa810]
    x0 = shufps(x0, x0, 0xd2);                                     // 4261d4 shufps xmm0, xmm0, 0xd2
    x1 = shufps(x1, x1, 0xc9);                                     // 4261d8 shufps xmm1, xmm1, 0xc9
    x2 = x0;                                                       // 4261dc movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0xd2);                                     // 4261df shufps xmm2, xmm0, 0xd2
    x3 = x1;                                                       // 4261e3 movaps xmm3, xmm1
    x3 = shufps(x3, x1, 0xc9);                                     // 4261e6 shufps xmm3, xmm1, 0xc9
    x0 = mulps(x0, x1);                                            // 4261ea mulps xmm0, xmm1
    x1 = X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000);     // 4261ed movaps xmm1, [0x92db80]
    x2 = mulps(x2, x3);                                            // 4261f4 mulps xmm2, xmm3
    x0 = subps(x0, x2);                                            // 4261f7 subps xmm0, xmm2
    edi = 0x1u;                                                    // 4261fa mov edi, 1
    x0 = andps(x0, x1);                                            // 4261ff andps xmm0, xmm1
    st(m, ebx + 0xa820, x0);                                       // 426202 movaps [ebx + 0xa820], xmm0
    st32(m, ebx + 0xa830, edi);                                    // 426209 mov [ebx + 0xa830], edi
    edx = ld32(m, eax + 0x20);                                     // 42620f mov edx, [eax + 0x20]
    fl = f_cmp(ld32(m, edx + 0x34), edi);                          // 426212 cmp [edx + 0x34], edi
    if (fl.ne()) goto L_00426224;                                  // 426215 jne 0x426224
    eax = ld32(m, 0xa946b8);                                       // 426217 mov eax, [0xa946b8]
    eax = ld32(m, eax + 0x5a54);                                   // 42621c mov eax, [eax + 0x5a54]
    goto L_0042623c;                                               // 426222 jmp 0x42623c
L_00426224:
    ecx = ld32(m, eax + 0x1c);                                     // 426224 mov ecx, [eax + 0x1c]
    fl = f_cmp(ld32(m, ecx + 0x34), edi);                          // 426227 cmp [ecx + 0x34], edi
    if (fl.ne()) goto L_0042623a;                                  // 42622a jne 0x42623a
    edx = ld32(m, 0xa946b8);                                       // 42622c mov edx, [0xa946b8]
    eax = ld32(m, edx + 0x20c8);                                   // 426232 mov eax, [edx + 0x20c8]
    goto L_0042623c;                                               // 426238 jmp 0x42623c
L_0042623a:
    eax = 0u;                                                      // 42623a xor eax, eax
L_0042623c:
    eax = ld32(m, eax + 0x20134);                                  // 42623c mov eax, [eax + 0x20134]
    esi = ld32(m, eax + 0x1900);                                   // 426242 mov esi, [eax + 0x1900]
    fl = f_cmp(esi, 0x64u);                                        // 426248 cmp esi, 0x64
    if (fl.ae()) goto L_004262ee;                                  // 42624b jae 0x4262ee
    ecx = esi + 0x1;                                               // 426251 lea ecx, [esi + 1]
    esi = esi << 6;                                                // 426254 shl esi, 6
    st32(m, eax + 0x1900, ecx);                                    // 426257 mov [eax + 0x1900], ecx
    esi = esi + eax;                                               // 42625d add esi, eax
    eax = ld32(m, ebx + 0xc);                                      // 42625f mov eax, [ebx + 0xc]
    k.call(0x0042bd90);  // FUN_0042bd90                           // 426262 call 0x42bd90
    fl = f_test8(ld8(m, 0x9e30e0), 0x1u);                          // 426267 test byte ptr [0x9e30e0], 1
    edx = ld32(m, eax + 0x34);                                     // 42626e mov edx, [eax + 0x34]
    st32(m, esi, edx);                                             // 426271 mov [esi], edx
    if (fl.ne()) goto L_00426285;                                  // 426273 jne 0x426285
    st32(m, 0x9e30e0, ld32(m, 0x9e30e0) | edi);                    // 426275 or [0x9e30e0], edi
    st32(m, 0x9e30dc, 0x0u);                                       // 42627b mov [0x9e30dc], 0
L_00426285:
    edx = 0x9e30d4u;                                               // 426285 mov edx, 0x9e30d4
    k.call(0x00402dc0);  // FUN_00402dc0                           // 42628a call 0x402dc0
    eax = ld32(m, eax + 0xe4);                                     // 42628f mov eax, [eax + 0xe4]
    x0 = X::ss(ld32(m, esp + 0xa0));                               // 426295 movss xmm0, [esp + 0xa0]
    x7 = ld(m, esp + 0x60);                                        // 42629e movaps xmm7, [esp + 0x60]
    st32(m, esi + 0x4, eax);                                       // 4262a3 mov [esi + 4], eax
    eax = eax | 0xffffffffu;                                       // 4262a6 or eax, 0xffffffff
    st32(m, esi + 0x2c, 0x0u);                                     // 4262a9 mov [esi + 0x2c], 0
    st8(m, esi + 0x20, 0x0u);                                      // 4262b0 mov byte ptr [esi + 0x20], 0
    st32(m, esi + 0x30, eax);                                      // 4262b4 mov [esi + 0x30], eax
    st32(m, esi + 0x34, eax);                                      // 4262b7 mov [esi + 0x34], eax
    st32(m, esi + 0x10, x0.u[0]);                                  // 4262ba movss [esi + 0x10], xmm0
    x0 = X::ss(ld32(m, esp + 0xa4));                               // 4262bf movss xmm0, [esp + 0xa4]
    st32(m, esi + 0x14, x0.u[0]);                                  // 4262c8 movss [esi + 0x14], xmm0
    x0 = X::ss(ld32(m, esp + 0xa8));                               // 4262cd movss xmm0, [esp + 0xa8]
    st32(m, esi + 0x18, x0.u[0]);                                  // 4262d6 movss [esi + 0x18], xmm0
    x0 = X::ss(ld32(m, esp + 0x18));                               // 4262db movss xmm0, [esp + 0x18]
    st32(m, esi + 0x24, x0.u[0]);                                  // 4262e1 movss [esi + 0x24], xmm0
    x0 = X{};                                                      // 4262e6 xorps xmm0, xmm0
    st32(m, esi + 0x28, x0.u[0]);                                  // 4262e9 movss [esi + 0x28], xmm0
L_004262ee:
    x0 = ld(m, ebx + 0xa810);                                      // 4262ee movaps xmm0, [ebx + 0xa810]
    x0 = mulps(x0, x0);                                            // 4262f5 mulps xmm0, xmm0
    x1 = x0;                                                       // 4262f8 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x66);                                     // 4262fb shufps xmm1, xmm0, 0x66
    x0 = addss(x0, x1);                                            // 4262ff addss xmm0, xmm1
    x2 = x1;                                                       // 426303 movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0x55);                                     // 426306 shufps xmm2, xmm1, 0x55
    x0 = addss(x0, x2);                                            // 42630a addss xmm0, xmm2
    x0 = shufps(x0, x0, 0x00);                                     // 42630e shufps xmm0, xmm0, 0
    x0 = sqrtps(x0);                                               // 426312 sqrtps xmm0, xmm0
    x7 = cmpltps(x7, x0);                                          // 426315 cmpltps xmm7, xmm0
    st(m, esp + 0xa0, x7);                                         // 426319 movaps [esp + 0xa0], xmm7
    ecx = ld32(m, esp + 0xa4);                                     // 426321 mov ecx, [esp + 0xa4]
    edx = ld32(m, esp + 0xa8);                                     // 426328 mov edx, [esp + 0xa8]
    edx = edx + ecx;                                               // 42632f add edx, ecx
    edx = edx + ld32(m, esp + 0xa0);                               // 426331 add edx, [esp + 0xa0]
    fl = f_cmp(edx, 0xfffffffdu);                                  // 426338 cmp edx, -3
    if (fl.e()) goto L_0042634d;                                   // 42633b je 0x42634d
    x0 = cvtss2sd(x0, X::ss(ld32(m, esp + 0x18)));                 // 42633d cvtss2sd xmm0, [esp + 0x18]
    fl = f_comisd(x0.q(0), 0x404e000000000000ull);                 // 426343 comisd xmm0, [0x977d38]
    if (fl.be()) goto L_00426365;                                  // 42634b jbe 0x426365
L_0042634d:
    x0 = X::ss(0x3f800000u);                                       // 42634d movss xmm0, [0x931f24]
    st32(m, ebx + 0x300, x0.u[0]);                                 // 426355 movss [ebx + 0x300], xmm0
    st32(m, ebx + 0x304, x0.u[0]);                                 // 42635d movss [ebx + 0x304], xmm0
L_00426365:
    esi = ld32(m, esp + 0x30);                                     // 426365 mov esi, [esp + 0x30]
L_00426369:
    k.push(esi);                                                   // 426369 push esi
    k.call(0x00423df0);  // FUN_00423df0                           // 42636a call 0x423df0
    ecx = ld32(m, esp + 0x1dc);                                    // 42636f mov ecx, [esp + 0x1dc]
    edi = k.pop();                                                 // 426376 pop edi
    esi = k.pop();                                                 // 426377 pop esi
    ebx = k.pop();                                                 // 426378 pop ebx
    ecx = ecx ^ esp;                                               // 426379 xor ecx, esp
    k.call(0x008f9efe);  // __security_check_cookie                // 42637b call 0x8f9efe
    esp = ebp;                                                     // 426380 mov esp, ebp
    ebp = k.pop();                                                 // 426382 pop ebp
    return k.ret(8);                                               // 426383 ret 8
L_00426386:
    st32(m, ebx + 0xa830, ecx);                                    // 426386 mov [ebx + 0xa830], ecx
    x0 = ld(m, 0xd616a0);                                          // 42638c movaps xmm0, [0xd616a0]
    st(m, ebx + 0xa840, x0);                                       // 426393 movaps [ebx + 0xa840], xmm0
    goto L_00426369;                                               // 42639a jmp 0x426369
}

// 004263a0 FUN_004263a0: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x004263a0, Fn_004263a0, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 4263a0 push ebp
    ebp = esp;                                                     // 4263a1 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 4263a3 and esp, 0xfffffff0
    esp = esp - 0x94u;                                             // 4263a6 sub esp, 0x94
    eax = ld32(m, 0x9b45e4);                                       // 4263ac mov eax, [0x9b45e4]
    eax = eax ^ esp;                                               // 4263b1 xor eax, esp
    st32(m, esp + 0x90, eax);                                      // 4263b3 mov [esp + 0x90], eax
    x0 = X::ss(ld32(m, 0x9c4750));                                 // 4263ba movss xmm0, [0x9c4750]
    k.push(ebx);                                                   // 4263c2 push ebx
    ebx = ld32(m, ebp + 0x8);                                      // 4263c3 mov ebx, [ebp + 8]
    k.push(esi);                                                   // 4263c6 push esi
    esi = ecx;                                                     // 4263c7 mov esi, ecx
    st32(m, ebx + 0xa870, x0.u[0]);                                // 4263c9 movss [ebx + 0xa870], xmm0
    x5 = ld(m, edx);                                               // 4263d1 movaps xmm5, [edx]
    x1 = X::v(0xbf800000, 0xbf800000, 0xbf800000, 0x3f800000);     // 4263d4 movaps xmm1, [0x93a220]
    eax = esi;                                                     // 4263db mov eax, esi
    eax = eax * 0x70u;                                             // 4263dd imul eax, eax, 0x70
    x2 = ld(m, eax + ebx + 0xe80);                                 // 4263e0 movaps xmm2, [eax + ebx + 0xe80]
    x3 = ld(m, eax + ebx + 0xe90);                                 // 4263e8 movaps xmm3, [eax + ebx + 0xe90]
    x3 = andps(x3, X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000)); // 4263f0 andps xmm3, [0x92db80]
    x4 = x3;                                                       // 4263f7 movaps xmm4, xmm3
    x4 = shufps(x4, x3, 0x00);                                     // 4263fa shufps xmm4, xmm3, 0
    x6 = x3;                                                       // 4263fe movaps xmm6, xmm3
    x6 = shufps(x6, x3, 0x55);                                     // 426401 shufps xmm6, xmm3, 0x55
    ecx = esi + 0x8;                                               // 426405 lea ecx, [esi + 8]
    ecx = ecx << 7;                                                // 426408 shl ecx, 7
    x0 = ld(m, ecx + ebx);                                         // 42640b movaps xmm0, [ecx + ebx]
    eax = eax + ebx;                                               // 42640f add eax, ebx
    x5 = subps(x5, x0);                                            // 426411 subps xmm5, xmm0
    x7 = x3;                                                       // 426414 movaps xmm7, xmm3
    x7 = shufps(x7, x3, 0xaa);                                     // 426417 shufps xmm7, xmm3, 0xaa
    x0 = x2;                                                       // 42641b movaps xmm0, xmm2
    x0 = mulps(x0, x1);                                            // 42641e mulps xmm0, xmm1
    x1 = x0;                                                       // 426421 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x1b);                                     // 426424 shufps xmm1, xmm0, 0x1b
    x4 = mulps(x4, x1);                                            // 426428 mulps xmm4, xmm1
    x4 = mulps(x4, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 42642b mulps xmm4, [0x93a1f0]
    x1 = shufps(x1, x1, 0xb1);                                     // 426432 shufps xmm1, xmm1, 0xb1
    x6 = mulps(x6, x1);                                            // 426436 mulps xmm6, xmm1
    x6 = mulps(x6, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 426439 mulps xmm6, [0x93a200]
    x1 = shufps(x1, x1, 0x1b);                                     // 426440 shufps xmm1, xmm1, 0x1b
    x1 = mulps(x1, x7);                                            // 426444 mulps xmm1, xmm7
    x1 = mulps(x1, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 426447 mulps xmm1, [0x93a210]
    x1 = addps(x1, x6);                                            // 42644e addps xmm1, xmm6
    x6 = x3;                                                       // 426451 movaps xmm6, xmm3
    x6 = shufps(x6, x3, 0xff);                                     // 426454 shufps xmm6, xmm3, 0xff
    edx = ld32(m, ebp + 0xc);                                      // 426458 mov edx, [ebp + 0xc]
    x6 = mulps(x6, x0);                                            // 42645b mulps xmm6, xmm0
    x6 = addps(x6, x4);                                            // 42645e addps xmm6, xmm4
    x1 = addps(x1, x6);                                            // 426461 addps xmm1, xmm6
    x3 = x1;                                                       // 426464 movaps xmm3, xmm1
    x3 = shufps(x3, x1, 0x1b);                                     // 426467 shufps xmm3, xmm1, 0x1b
    x4 = x3;                                                       // 42646b movaps xmm4, xmm3
    x3 = shufps(x3, x3, 0xb1);                                     // 42646e shufps xmm3, xmm3, 0xb1
    x6 = x2;                                                       // 426472 movaps xmm6, xmm2
    x6 = shufps(x6, x2, 0x00);                                     // 426475 shufps xmm6, xmm2, 0
    x4 = mulps(x4, x6);                                            // 426479 mulps xmm4, xmm6
    x4 = mulps(x4, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 42647c mulps xmm4, [0x93a1f0]
    st(m, esp + 0x6c, x6);                                         // 426483 movaps [esp + 0x6c], xmm6
    x6 = x3;                                                       // 426488 movaps xmm6, xmm3
    x7 = x2;                                                       // 42648b movaps xmm7, xmm2
    x7 = shufps(x7, x2, 0x55);                                     // 42648e shufps xmm7, xmm2, 0x55
    x6 = mulps(x6, x7);                                            // 426492 mulps xmm6, xmm7
    x6 = mulps(x6, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 426495 mulps xmm6, [0x93a200]
    st(m, esp + 0x3c, x7);                                         // 42649c movaps [esp + 0x3c], xmm7
    x3 = shufps(x3, x3, 0x1b);                                     // 4264a1 shufps xmm3, xmm3, 0x1b
    x7 = x2;                                                       // 4264a5 movaps xmm7, xmm2
    x7 = shufps(x7, x2, 0xaa);                                     // 4264a8 shufps xmm7, xmm2, 0xaa
    st(m, esp + 0x1c, x7);                                         // 4264ac movaps [esp + 0x1c], xmm7
    x3 = mulps(x3, ld(m, esp + 0x1c));                             // 4264b1 mulps xmm3, [esp + 0x1c]
    x3 = mulps(x3, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 4264b6 mulps xmm3, [0x93a210]
    x7 = x2;                                                       // 4264bd movaps xmm7, xmm2
    x7 = shufps(x7, x2, 0xff);                                     // 4264c0 shufps xmm7, xmm2, 0xff
    st(m, esp + 0x2c, x7);                                         // 4264c4 movaps [esp + 0x2c], xmm7
    x1 = mulps(x1, ld(m, esp + 0x2c));                             // 4264c9 mulps xmm1, [esp + 0x2c]
    x3 = addps(x3, x6);                                            // 4264ce addps xmm3, xmm6
    x4 = addps(x4, x1);                                            // 4264d1 addps xmm4, xmm1
    x3 = addps(x3, x4);                                            // 4264d4 addps xmm3, xmm4
    ecx = esi;                                                     // 4264d7 mov ecx, esi
    x3 = shufps(x3, x3, 0xc9);                                     // 4264d9 shufps xmm3, xmm3, 0xc9
    ecx = ecx << 7;                                                // 4264dd shl ecx, 7
    x4 = x3;                                                       // 4264e0 movaps xmm4, xmm3
    ecx = ecx + ebx;                                               // 4264e3 add ecx, ebx
    x3 = x5;                                                       // 4264e5 movaps xmm3, xmm5
    k.push(edi);                                                   // 4264e8 push edi
    st(m, esp + 0x80, x5);                                         // 4264e9 movaps [esp + 0x80], xmm5
    x3 = shufps(x3, x5, 0xd2);                                     // 4264f1 shufps xmm3, xmm5, 0xd2
    st32(m, esp + 0x10, ecx);                                      // 4264f5 mov [esp + 0x10], ecx
    x6 = ld(m, edx);                                               // 4264f9 movaps xmm6, [edx]
    x1 = x4;                                                       // 4264fc movaps xmm1, xmm4
    x1 = shufps(x1, x4, 0xc9);                                     // 4264ff shufps xmm1, xmm4, 0xc9
    x7 = x3;                                                       // 426503 movaps xmm7, xmm3
    x7 = shufps(x7, x3, 0xd2);                                     // 426506 shufps xmm7, xmm3, 0xd2
    x7 = mulps(x7, x1);                                            // 42650a mulps xmm7, xmm1
    x1 = x3;                                                       // 42650d movaps xmm1, xmm3
    x1 = mulps(x1, x4);                                            // 426510 mulps xmm1, xmm4
    x4 = ld(m, ecx + 0x410);                                       // 426513 movaps xmm4, [ecx + 0x410]
    x1 = subps(x1, x7);                                            // 42651a subps xmm1, xmm7
    x7 = X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000);     // 42651d movaps xmm7, [0x92db80]
    x1 = andps(x1, x7);                                            // 426524 andps xmm1, xmm7
    x1 = addps(x1, x4);                                            // 426527 addps xmm1, xmm4
    x1 = mulps(x1, x6);                                            // 42652a mulps xmm1, xmm6
    x4 = x1;                                                       // 42652d movaps xmm4, xmm1
    x4 = shufps(x4, x1, 0x99);                                     // 426530 shufps xmm4, xmm1, 0x99
    x1 = addss(x1, x4);                                            // 426534 addss xmm1, xmm4
    x4 = shufps(x4, x4, 0x55);                                     // 426538 shufps xmm4, xmm4, 0x55
    x1 = addss(x1, x4);                                            // 42653c addss xmm1, xmm4
    st(m, esp + 0x60, x1);                                         // 426540 movaps [esp + 0x60], xmm1
    x1 = x6;                                                       // 426545 movaps xmm1, xmm6
    x1 = shufps(x1, x6, 0xd2);                                     // 426548 shufps xmm1, xmm6, 0xd2
    x4 = x1;                                                       // 42654c movaps xmm4, xmm1
    x4 = shufps(x4, x1, 0xd2);                                     // 42654f shufps xmm4, xmm1, 0xd2
    x5 = shufps(x5, x5, 0xc9);                                     // 426553 shufps xmm5, xmm5, 0xc9
    x1 = mulps(x1, x5);                                            // 426557 mulps xmm1, xmm5
    x6 = x5;                                                       // 42655a movaps xmm6, xmm5
    x6 = shufps(x6, x5, 0xc9);                                     // 42655d shufps xmm6, xmm5, 0xc9
    x4 = mulps(x4, x6);                                            // 426561 mulps xmm4, xmm6
    x1 = subps(x1, x4);                                            // 426564 subps xmm1, xmm4
    x1 = andps(x1, x7);                                            // 426567 andps xmm1, xmm7
    x1 = andps(x1, x7);                                            // 42656a andps xmm1, xmm7
    x4 = x1;                                                       // 42656d movaps xmm4, xmm1
    x4 = shufps(x4, x1, 0x00);                                     // 426570 shufps xmm4, xmm1, 0
    x6 = x1;                                                       // 426574 movaps xmm6, xmm1
    x6 = shufps(x6, x1, 0x55);                                     // 426577 shufps xmm6, xmm1, 0x55
    x7 = x1;                                                       // 42657b movaps xmm7, xmm1
    x7 = shufps(x7, x1, 0xaa);                                     // 42657e shufps xmm7, xmm1, 0xaa
    x5 = x2;                                                       // 426582 movaps xmm5, xmm2
    x5 = shufps(x5, x2, 0x1b);                                     // 426585 shufps xmm5, xmm2, 0x1b
    x4 = mulps(x4, x5);                                            // 426589 mulps xmm4, xmm5
    st(m, esp + 0x50, x4);                                         // 42658c movaps [esp + 0x50], xmm4
    x4 = x5;                                                       // 426591 movaps xmm4, xmm5
    x4 = shufps(x4, x5, 0xb1);                                     // 426594 shufps xmm4, xmm5, 0xb1
    x6 = mulps(x6, x4);                                            // 426598 mulps xmm6, xmm4
    x6 = mulps(x6, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 42659b mulps xmm6, [0x93a200]
    x4 = shufps(x4, x4, 0x1b);                                     // 4265a2 shufps xmm4, xmm4, 0x1b
    x4 = mulps(x4, x7);                                            // 4265a6 mulps xmm4, xmm7
    x4 = mulps(x4, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 4265a9 mulps xmm4, [0x93a210]
    x4 = addps(x4, x6);                                            // 4265b0 addps xmm4, xmm6
    x6 = x1;                                                       // 4265b3 movaps xmm6, xmm1
    x6 = shufps(x6, x1, 0xff);                                     // 4265b6 shufps xmm6, xmm1, 0xff
    x1 = ld(m, esp + 0x50);                                        // 4265ba movaps xmm1, [esp + 0x50]
    x1 = mulps(x1, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 4265bf mulps xmm1, [0x93a1f0]
    x6 = mulps(x6, x2);                                            // 4265c6 mulps xmm6, xmm2
    x6 = addps(x6, x1);                                            // 4265c9 addps xmm6, xmm1
    x4 = addps(x4, x6);                                            // 4265cc addps xmm4, xmm6
    x1 = x4;                                                       // 4265cf movaps xmm1, xmm4
    x1 = shufps(x1, x4, 0x1b);                                     // 4265d2 shufps xmm1, xmm4, 0x1b
    x6 = x0;                                                       // 4265d6 movaps xmm6, xmm0
    x6 = shufps(x6, x0, 0x00);                                     // 4265d9 shufps xmm6, xmm0, 0
    x6 = mulps(x6, x1);                                            // 4265dd mulps xmm6, xmm1
    st(m, esp + 0x50, x6);                                         // 4265e0 movaps [esp + 0x50], xmm6
    x1 = shufps(x1, x1, 0xb1);                                     // 4265e5 shufps xmm1, xmm1, 0xb1
    x6 = x0;                                                       // 4265e9 movaps xmm6, xmm0
    x6 = shufps(x6, x0, 0x55);                                     // 4265ec shufps xmm6, xmm0, 0x55
    x6 = mulps(x6, x1);                                            // 4265f0 mulps xmm6, xmm1
    x6 = mulps(x6, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 4265f3 mulps xmm6, [0x93a200]
    x1 = shufps(x1, x1, 0x1b);                                     // 4265fa shufps xmm1, xmm1, 0x1b
    x7 = x0;                                                       // 4265fe movaps xmm7, xmm0
    x7 = shufps(x7, x0, 0xaa);                                     // 426601 shufps xmm7, xmm0, 0xaa
    x1 = mulps(x1, x7);                                            // 426605 mulps xmm1, xmm7
    x1 = mulps(x1, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 426608 mulps xmm1, [0x93a210]
    x1 = addps(x1, x6);                                            // 42660f addps xmm1, xmm6
    x6 = x0;                                                       // 426612 movaps xmm6, xmm0
    x6 = shufps(x6, x0, 0xff);                                     // 426615 shufps xmm6, xmm0, 0xff
    x6 = mulps(x6, x4);                                            // 426619 mulps xmm6, xmm4
    x4 = ld(m, esp + 0x50);                                        // 42661c movaps xmm4, [esp + 0x50]
    x4 = mulps(x4, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 426621 mulps xmm4, [0x93a1f0]
    x6 = addps(x6, x4);                                            // 426628 addps xmm6, xmm4
    x1 = addps(x1, x6);                                            // 42662b addps xmm1, xmm6
    x4 = ld(m, eax + 0xeb0);                                       // 42662e movaps xmm4, [eax + 0xeb0]
    x1 = mulps(x1, x4);                                            // 426635 mulps xmm1, xmm4
    x1 = andps(x1, X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000)); // 426638 andps xmm1, [0x92db80]
    x6 = x1;                                                       // 42663f movaps xmm6, xmm1
    x6 = shufps(x6, x1, 0x00);                                     // 426642 shufps xmm6, xmm1, 0
    x7 = x1;                                                       // 426646 movaps xmm7, xmm1
    x7 = shufps(x7, x1, 0xaa);                                     // 426649 shufps xmm7, xmm1, 0xaa
    x4 = x0;                                                       // 42664d movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x1b);                                     // 426650 shufps xmm4, xmm0, 0x1b
    x6 = mulps(x6, x4);                                            // 426654 mulps xmm6, xmm4
    st(m, esp + 0x50, x6);                                         // 426657 movaps [esp + 0x50], xmm6
    x6 = x1;                                                       // 42665c movaps xmm6, xmm1
    x6 = shufps(x6, x1, 0x55);                                     // 42665f shufps xmm6, xmm1, 0x55
    x4 = shufps(x4, x4, 0xb1);                                     // 426663 shufps xmm4, xmm4, 0xb1
    x6 = mulps(x6, x4);                                            // 426667 mulps xmm6, xmm4
    x6 = mulps(x6, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 42666a mulps xmm6, [0x93a200]
    x4 = shufps(x4, x4, 0x1b);                                     // 426671 shufps xmm4, xmm4, 0x1b
    x4 = mulps(x4, x7);                                            // 426675 mulps xmm4, xmm7
    x4 = mulps(x4, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 426678 mulps xmm4, [0x93a210]
    x4 = addps(x4, x6);                                            // 42667f addps xmm4, xmm6
    x6 = x1;                                                       // 426682 movaps xmm6, xmm1
    x6 = shufps(x6, x1, 0xff);                                     // 426685 shufps xmm6, xmm1, 0xff
    x1 = ld(m, esp + 0x50);                                        // 426689 movaps xmm1, [esp + 0x50]
    x1 = mulps(x1, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 42668e mulps xmm1, [0x93a1f0]
    x7 = X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000);     // 426695 movaps xmm7, [0x93a1f0]
    x6 = mulps(x6, x0);                                            // 42669c mulps xmm6, xmm0
    x6 = addps(x6, x1);                                            // 42669f addps xmm6, xmm1
    x4 = addps(x4, x6);                                            // 4266a2 addps xmm4, xmm6
    x1 = x4;                                                       // 4266a5 movaps xmm1, xmm4
    x1 = shufps(x1, x4, 0x1b);                                     // 4266a8 shufps xmm1, xmm4, 0x1b
    x4 = mulps(x4, ld(m, esp + 0x30));                             // 4266ac mulps xmm4, [esp + 0x30]
    x6 = x1;                                                       // 4266b1 movaps xmm6, xmm1
    x6 = mulps(x6, ld(m, esp + 0x70));                             // 4266b4 mulps xmm6, [esp + 0x70]
    x1 = shufps(x1, x1, 0xb1);                                     // 4266b9 shufps xmm1, xmm1, 0xb1
    st(m, esp + 0x70, x6);                                         // 4266bd movaps [esp + 0x70], xmm6
    x6 = x1;                                                       // 4266c2 movaps xmm6, xmm1
    x6 = mulps(x6, ld(m, esp + 0x40));                             // 4266c5 mulps xmm6, [esp + 0x40]
    x6 = mulps(x6, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 4266ca mulps xmm6, [0x93a200]
    x1 = shufps(x1, x1, 0x1b);                                     // 4266d1 shufps xmm1, xmm1, 0x1b
    x1 = mulps(x1, ld(m, esp + 0x20));                             // 4266d5 mulps xmm1, [esp + 0x20]
    x1 = mulps(x1, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 4266da mulps xmm1, [0x93a210]
    x1 = addps(x1, x6);                                            // 4266e1 addps xmm1, xmm6
    x6 = ld(m, esp + 0x70);                                        // 4266e4 movaps xmm6, [esp + 0x70]
    x6 = mulps(x6, x7);                                            // 4266e9 mulps xmm6, xmm7
    x4 = addps(x4, x6);                                            // 4266ec addps xmm4, xmm6
    x1 = addps(x1, x4);                                            // 4266ef addps xmm1, xmm4
    x1 = shufps(x1, x1, 0xc9);                                     // 4266f2 shufps xmm1, xmm1, 0xc9
    x6 = x1;                                                       // 4266f6 movaps xmm6, xmm1
    x6 = shufps(x6, x1, 0xc9);                                     // 4266f9 shufps xmm6, xmm1, 0xc9
    x4 = x3;                                                       // 4266fd movaps xmm4, xmm3
    x4 = shufps(x4, x3, 0xd2);                                     // 426700 shufps xmm4, xmm3, 0xd2
    x3 = mulps(x3, x1);                                            // 426704 mulps xmm3, xmm1
    x4 = mulps(x4, x6);                                            // 426707 mulps xmm4, xmm6
    x6 = ld(m, 0xd61650);                                          // 42670a movaps xmm6, [0xd61650]
    x3 = subps(x3, x4);                                            // 426711 subps xmm3, xmm4
    x3 = andps(x3, X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000)); // 426714 andps xmm3, [0x92db80]
    x4 = ld(m, edx);                                               // 42671b movaps xmm4, [edx]
    x3 = mulps(x3, x4);                                            // 42671e mulps xmm3, xmm4
    x1 = x3;                                                       // 426721 movaps xmm1, xmm3
    x1 = shufps(x1, x3, 0x99);                                     // 426724 shufps xmm1, xmm3, 0x99
    x3 = addss(x3, x1);                                            // 426728 addss xmm3, xmm1
    x1 = shufps(x1, x1, 0x55);                                     // 42672c shufps xmm1, xmm1, 0x55
    x3 = addss(x3, x1);                                            // 426730 addss xmm3, xmm1
    x1 = X::ss(ld32(m, ebx + 0xa868));                             // 426734 movss xmm1, [ebx + 0xa868]
    x1 = shufps(x1, x1, 0x00);                                     // 42673c shufps xmm1, xmm1, 0
    x1 = addps(x1, x6);                                            // 426740 addps xmm1, xmm6
    x6 = X{};                                                      // 426743 xorps xmm6, xmm6
    x6 = subps(x6, x1);                                            // 426746 subps xmm6, xmm1
    x1 = ld(m, esp + 0x60);                                        // 426749 movaps xmm1, [esp + 0x60]
    x1 = shufps(x1, x1, 0x00);                                     // 42674e shufps xmm1, xmm1, 0
    x6 = mulps(x6, x1);                                            // 426752 mulps xmm6, xmm1
    x1 = x3;                                                       // 426755 movaps xmm1, xmm3
    x1 = shufps(x1, x3, 0x00);                                     // 426758 shufps xmm1, xmm3, 0
    x3 = ld(m, ecx + 0x430);                                       // 42675c movaps xmm3, [ecx + 0x430]
    x1 = addps(x1, x3);                                            // 426763 addps xmm1, xmm3
    x3 = ld(m, esp + 0x80);                                        // 426766 movaps xmm3, [esp + 0x80]
    x3 = andps(x3, X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000)); // 42676e andps xmm3, [0x92db80]
    x6 = divps(x6, x1);                                            // 426775 divps xmm6, xmm1
    x6 = mulps(x6, x4);                                            // 426778 mulps xmm6, xmm4
    st(m, esp + 0x60, x6);                                         // 42677b movaps [esp + 0x60], xmm6
    x4 = x3;                                                       // 426780 movaps xmm4, xmm3
    x4 = shufps(x4, x3, 0x00);                                     // 426783 shufps xmm4, xmm3, 0
    x4 = mulps(x4, x5);                                            // 426787 mulps xmm4, xmm5
    x5 = shufps(x5, x5, 0xb1);                                     // 42678a shufps xmm5, xmm5, 0xb1
    x6 = x3;                                                       // 42678e movaps xmm6, xmm3
    x6 = shufps(x6, x3, 0x55);                                     // 426791 shufps xmm6, xmm3, 0x55
    x6 = mulps(x6, x5);                                            // 426795 mulps xmm6, xmm5
    x1 = x5;                                                       // 426798 movaps xmm1, xmm5
    x1 = shufps(x1, x5, 0x1b);                                     // 42679b shufps xmm1, xmm5, 0x1b
    x5 = x3;                                                       // 42679f movaps xmm5, xmm3
    x5 = shufps(x5, x3, 0xaa);                                     // 4267a2 shufps xmm5, xmm3, 0xaa
    x1 = mulps(x1, x5);                                            // 4267a6 mulps xmm1, xmm5
    x5 = X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000);     // 4267a9 movaps xmm5, [0x93a200]
    x1 = mulps(x1, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 4267b0 mulps xmm1, [0x93a210]
    x6 = mulps(x6, x5);                                            // 4267b7 mulps xmm6, xmm5
    x1 = addps(x1, x6);                                            // 4267ba addps xmm1, xmm6
    x6 = x3;                                                       // 4267bd movaps xmm6, xmm3
    x6 = shufps(x6, x3, 0xff);                                     // 4267c0 shufps xmm6, xmm3, 0xff
    x6 = mulps(x6, x2);                                            // 4267c4 mulps xmm6, xmm2
    x4 = mulps(x4, x7);                                            // 4267c7 mulps xmm4, xmm7
    x6 = addps(x6, x4);                                            // 4267ca addps xmm6, xmm4
    x1 = addps(x1, x6);                                            // 4267cd addps xmm1, xmm6
    x2 = x1;                                                       // 4267d0 movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0x1b);                                     // 4267d3 shufps xmm2, xmm1, 0x1b
    x3 = x0;                                                       // 4267d7 movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0x00);                                     // 4267da shufps xmm3, xmm0, 0
    x3 = mulps(x3, x2);                                            // 4267de mulps xmm3, xmm2
    x2 = shufps(x2, x2, 0xb1);                                     // 4267e1 shufps xmm2, xmm2, 0xb1
    x4 = x0;                                                       // 4267e5 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x55);                                     // 4267e8 shufps xmm4, xmm0, 0x55
    x4 = mulps(x4, x2);                                            // 4267ec mulps xmm4, xmm2
    x2 = shufps(x2, x2, 0x1b);                                     // 4267ef shufps xmm2, xmm2, 0x1b
    x6 = x0;                                                       // 4267f3 movaps xmm6, xmm0
    x6 = shufps(x6, x0, 0xaa);                                     // 4267f6 shufps xmm6, xmm0, 0xaa
    x6 = mulps(x6, x2);                                            // 4267fa mulps xmm6, xmm2
    x6 = mulps(x6, X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000)); // 4267fd mulps xmm6, [0x93a210]
    x2 = x0;                                                       // 426804 movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0xff);                                     // 426807 shufps xmm2, xmm0, 0xff
    edx = esp + 0x80;                                              // 42680b lea edx, [esp + 0x80]
    x4 = mulps(x4, x5);                                            // 426812 mulps xmm4, xmm5
    x2 = mulps(x2, x1);                                            // 426815 mulps xmm2, xmm1
    x3 = mulps(x3, x7);                                            // 426818 mulps xmm3, xmm7
    x6 = addps(x6, x4);                                            // 42681b addps xmm6, xmm4
    x2 = addps(x2, x3);                                            // 42681e addps xmm2, xmm3
    k.push(edx);                                                   // 426821 push edx
    x6 = addps(x6, x2);                                            // 426822 addps xmm6, xmm2
    edx = ebx + 0x380;                                             // 426825 lea edx, [ebx + 0x380]
    edi = esp + 0x64;                                              // 42682b lea edi, [esp + 0x64]
    st(m, esp + 0x84, x6);                                         // 42682f movaps [esp + 0x84], xmm6
    k.call(0x00422180);  // FUN_00422180                           // 426837 call 0x422180
    eax = ld32(m, esp + 0x10);                                     // 42683c mov eax, [esp + 0x10]
    ecx = ld32(m, ebp + 0xc);                                      // 426840 mov ecx, [ebp + 0xc]
    x3 = ld(m, ecx);                                               // 426843 movaps xmm3, [ecx]
    x2 = ld(m, eax + 0x410);                                       // 426846 movaps xmm2, [eax + 0x410]
    x0 = x3;                                                       // 42684d movaps xmm0, xmm3
    x0 = mulps(x0, x2);                                            // 426850 mulps xmm0, xmm2
    x1 = x0;                                                       // 426853 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x99);                                     // 426856 shufps xmm1, xmm0, 0x99
    x0 = addss(x0, x1);                                            // 42685a addss xmm0, xmm1
    x1 = shufps(x1, x1, 0x55);                                     // 42685e shufps xmm1, xmm1, 0x55
    x0 = addss(x0, x1);                                            // 426862 addss xmm0, xmm1
    x1 = x0;                                                       // 426866 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x00);                                     // 426869 shufps xmm1, xmm0, 0
    x0 = X::ss(ld32(m, 0x9cc378));                                 // 42686d movss xmm0, [0x9cc378]
    x1 = mulps(x1, x3);                                            // 426875 mulps xmm1, xmm3
    x2 = subps(x2, x1);                                            // 426878 subps xmm2, xmm1
    x0 = shufps(x0, x0, 0x00);                                     // 42687b shufps xmm0, xmm0, 0
    x2 = mulps(x2, x0);                                            // 42687f mulps xmm2, xmm0
    x0 = ld(m, eax + 0x450);                                       // 426882 movaps xmm0, [eax + 0x450]
    x2 = addps(x2, x0);                                            // 426889 addps xmm2, xmm0
    st(m, eax + 0x450, x2);                                        // 42688c movaps [eax + 0x450], xmm2
    fl = f_cmp(ld32(m, ebx + 0xa878), 0x0u);                       // 426893 cmp [ebx + 0xa878], 0
    if (fl.e()) goto L_00426915;                                   // 42689a je 0x426915
    edx = ld32(m, ebp + 0x10);                                     // 42689c mov edx, [ebp + 0x10]
    x1 = ld(m, edx);                                               // 42689f movaps xmm1, [edx]
    x0 = X::ss(ld32(m, 0x9c4750));                                 // 4268a2 movss xmm0, [0x9c4750]
    x0 = shufps(x0, x0, 0x00);                                     // 4268aa shufps xmm0, xmm0, 0
    x0 = mulps(x0, x1);                                            // 4268ae mulps xmm0, xmm1
    x1 = ld(m, ecx);                                               // 4268b1 movaps xmm1, [ecx]
    x0 = mulps(x0, x1);                                            // 4268b4 mulps xmm0, xmm1
    x1 = ld(m, eax + 0x450);                                       // 4268b7 movaps xmm1, [eax + 0x450]
    x0 = addps(x0, x1);                                            // 4268be addps xmm0, xmm1
    st(m, eax + 0x450, x0);                                        // 4268c1 movaps [eax + 0x450], xmm0
    x2 = ld(m, ecx);                                               // 4268c8 movaps xmm2, [ecx]
    x1 = ld(m, eax + 0x410);                                       // 4268cb movaps xmm1, [eax + 0x410]
    x0 = x2;                                                       // 4268d2 movaps xmm0, xmm2
    x0 = mulps(x0, x1);                                            // 4268d5 mulps xmm0, xmm1
    x1 = x0;                                                       // 4268d8 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x99);                                     // 4268db shufps xmm1, xmm0, 0x99
    x0 = addss(x0, x1);                                            // 4268df addss xmm0, xmm1
    x1 = shufps(x1, x1, 0x55);                                     // 4268e3 shufps xmm1, xmm1, 0x55
    x0 = addss(x0, x1);                                            // 4268e7 addss xmm0, xmm1
    x1 = x0;                                                       // 4268eb movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x00);                                     // 4268ee shufps xmm1, xmm0, 0
    x0 = X::ss(ld32(m, 0x9cc374));                                 // 4268f2 movss xmm0, [0x9cc374]
    x1 = mulps(x1, x2);                                            // 4268fa mulps xmm1, xmm2
    x0 = shufps(x0, x0, 0x00);                                     // 4268fd shufps xmm0, xmm0, 0
    x1 = mulps(x1, x0);                                            // 426901 mulps xmm1, xmm0
    x0 = ld(m, eax + 0x450);                                       // 426904 movaps xmm0, [eax + 0x450]
    x1 = addps(x1, x0);                                            // 42690b addps xmm1, xmm0
    st(m, eax + 0x450, x1);                                        // 42690e movaps [eax + 0x450], xmm1
L_00426915:
    ecx = ld32(m, esp + 0x9c);                                     // 426915 mov ecx, [esp + 0x9c]
    edi = k.pop();                                                 // 42691c pop edi
    esi = k.pop();                                                 // 42691d pop esi
    ebx = k.pop();                                                 // 42691e pop ebx
    ecx = ecx ^ esp;                                               // 42691f xor ecx, esp
    k.call(0x008f9efe);  // __security_check_cookie                // 426921 call 0x8f9efe
    esp = ebp;                                                     // 426926 mov esp, ebp
    ebp = k.pop();                                                 // 426928 pop ebp
    return k.ret(12);                                              // 426929 ret 0xc
}

// 00426930 FUN_00426930: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00426930, Fn_00426930, "math", 0) {  // outputs: what callers read (it calls other groups' code)
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 426930 push ebp
    ebp = esp;                                                     // 426931 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 426933 and esp, 0xfffffff0
    esp = esp - 0x54u;                                             // 426936 sub esp, 0x54
    eax = ld32(m, 0x9b45e4);                                       // 426939 mov eax, [0x9b45e4]
    eax = eax ^ esp;                                               // 42693e xor eax, esp
    st32(m, esp + 0x50, eax);                                      // 426940 mov [esp + 0x50], eax
    k.push(ebx);                                                   // 426944 push ebx
    k.push(esi);                                                   // 426945 push esi
    k.push(edi);                                                   // 426946 push edi
    edi = ecx;                                                     // 426947 mov edi, ecx
    eax = ld32(m, edi + 0x37c);                                    // 426949 mov eax, [edi + 0x37c]
    esi = ld32(m, edi + 0xa540);                                   // 42694f mov esi, [edi + 0xa540]
    ecx = ld32(m, eax + 0x240);                                    // 426955 mov ecx, [eax + 0x240]
    x0 = ld(m, edi + 0x400);                                       // 42695b movaps xmm0, [edi + 0x400]
    esi = esi << 4;                                                // 426962 shl esi, 4
    esi = esi + ld32(m, ecx + 0x8);                                // 426965 add esi, [ecx + 8]
    edx = esp + 0x30;                                              // 426968 lea edx, [esp + 0x30]
    st(m, esp + 0x40, x0);                                         // 42696c movaps [esp + 0x40], xmm0
    x0 = ld(m, edi + 0xe80);                                       // 426971 movaps xmm0, [edi + 0xe80]
    k.push(edx);                                                   // 426978 push edx
    ecx = esi;                                                     // 426979 mov ecx, esi
    st32(m, esp + 0x30, edi);                                      // 42697b mov [esp + 0x30], edi
    st(m, esp + 0x34, x0);                                         // 42697f movaps [esp + 0x34], xmm0
    k.call(0x007dcfb0);  // FUN_007dcfb0                           // 426984 call 0x7dcfb0
    eax = esp + 0x40;                                              // 426989 lea eax, [esp + 0x40]
    k.push(eax);                                                   // 42698d push eax
    k.push(esi);                                                   // 42698e push esi
    k.call(0x007dd1b0);  // FUN_007dd1b0                           // 42698f call 0x7dd1b0
    eax = 0x1u;                                                    // 426994 mov eax, 1
    fl = f_cmp(ld32(m, edi + 0x3fc), eax);                         // 426999 cmp [edi + 0x3fc], eax
    st32(m, esp + 0x28, eax);                                      // 42699f mov [esp + 0x28], eax
    if (fl.le()) goto L_00426a38;                                  // 4269a3 jle 0x426a38
    ecx = edi + 0xef0;                                             // 4269a9 lea ecx, [edi + 0xef0]
    edx = edi + 0xa544;                                            // 4269af lea edx, [edi + 0xa544]
    esi = edi + 0x380;                                             // 4269b5 lea esi, [edi + 0x380]
    st32(m, esp + 0x10, ecx);                                      // 4269bb mov [esp + 0x10], ecx
    st32(m, esp + 0x24, edx);                                      // 4269bf mov [esp + 0x24], edx
    goto L_004269d2;                                               // 4269c3 jmp 0x4269d2
    goto L_004269d0;                                               // 4269c5 jmp 0x4269d0
    esp = esp;                                                     // 4269c7 lea esp, [esp]
    edi = edi;                                                     // 4269ce mov edi, edi
L_004269d0:
    edi = ecx;                                                     // 4269d0 mov edi, ecx
L_004269d2:
    edx = ld32(m, esp + 0x24);                                     // 4269d2 mov edx, [esp + 0x24]
    eax = ld32(m, edi + 0x37c);                                    // 4269d6 mov eax, [edi + 0x37c]
    ecx = ld32(m, eax + 0x240);                                    // 4269dc mov ecx, [eax + 0x240]
    ebx = ld32(m, edx);                                            // 4269e2 mov ebx, [edx]
    eax = ld32(m, esp + 0x10);                                     // 4269e4 mov eax, [esp + 0x10]
    x0 = ld(m, eax);                                               // 4269e8 movaps xmm0, [eax]
    ebx = ebx << 4;                                                // 4269eb shl ebx, 4
    ebx = ebx + ld32(m, ecx + 0x8);                                // 4269ee add ebx, [ecx + 8]
    ecx = esp + 0x30;                                              // 4269f1 lea ecx, [esp + 0x30]
    k.push(ecx);                                                   // 4269f5 push ecx
    ecx = ebx;                                                     // 4269f6 mov ecx, ebx
    st(m, esp + 0x34, x0);                                         // 4269f8 movaps [esp + 0x34], xmm0
    k.call(0x007dcfb0);  // FUN_007dcfb0                           // 4269fd call 0x7dcfb0
    edx = ld32(m, esp + 0x28);                                     // 426a02 mov edx, [esp + 0x28]
    edx = edx - 1u;                                                // 426a06 dec edx
    edi = esp + 0x40;                                              // 426a07 lea edi, [esp + 0x40]
    k.call(0x0041feb0);  // FUN_0041feb0                           // 426a0b call 0x41feb0
    edx = edi;                                                     // 426a10 mov edx, edi
    k.push(edx);                                                   // 426a12 push edx
    k.push(ebx);                                                   // 426a13 push ebx
    k.call(0x007dd1b0);  // FUN_007dd1b0                           // 426a14 call 0x7dd1b0
    eax = ld32(m, esp + 0x28);                                     // 426a19 mov eax, [esp + 0x28]
    ecx = ld32(m, esp + 0x2c);                                     // 426a1d mov ecx, [esp + 0x2c]
    st32(m, esp + 0x24, ld32(m, esp + 0x24) + 0x4u);               // 426a21 add [esp + 0x24], 4
    st32(m, esp + 0x10, ld32(m, esp + 0x10) + 0x70u);              // 426a26 add [esp + 0x10], 0x70
    eax = eax + 1u;                                                // 426a2b inc eax
    fl = f_cmp(eax, ld32(m, ecx + 0x3fc));                         // 426a2c cmp eax, [ecx + 0x3fc]
    st32(m, esp + 0x28, eax);                                      // 426a32 mov [esp + 0x28], eax
    if (fl.l()) goto L_004269d0;                                   // 426a36 jl 0x4269d0
L_00426a38:
    ecx = ld32(m, esp + 0x5c);                                     // 426a38 mov ecx, [esp + 0x5c]
    edi = k.pop();                                                 // 426a3c pop edi
    esi = k.pop();                                                 // 426a3d pop esi
    ebx = k.pop();                                                 // 426a3e pop ebx
    ecx = ecx ^ esp;                                               // 426a3f xor ecx, esp
    k.call(0x008f9efe);  // __security_check_cookie                // 426a41 call 0x8f9efe
    esp = ebp;                                                     // 426a46 mov esp, ebp
    ebp = k.pop();                                                 // 426a48 pop ebp
    return k.ret(0);                                               // 426a49 ret 
}

// 00426d00 FUN_00426d00: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00426d00, Fn_00426d00, "math", 0) {  // outputs: what callers read (it calls other groups' code)
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 426d00 push ebp
    ebp = esp;                                                     // 426d01 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 426d03 and esp, 0xfffffff0
    esp = esp - 0x4d4u;                                            // 426d06 sub esp, 0x4d4
    eax = ld32(m, 0x9b45e4);                                       // 426d0c mov eax, [0x9b45e4]
    eax = eax ^ esp;                                               // 426d11 xor eax, esp
    st32(m, esp + 0x4d0, eax);                                     // 426d13 mov [esp + 0x4d0], eax
    k.push(ebx);                                                   // 426d1a push ebx
    ebx = ld32(m, ebp + 0x8);                                      // 426d1b mov ebx, [ebp + 8]
    fl = f_cmp(ld32(m, ebx + 0xa8c0), 0x0u);                       // 426d1e cmp [ebx + 0xa8c0], 0
    k.push(esi);                                                   // 426d25 push esi
    k.push(edi);                                                   // 426d26 push edi
    if (fl.e()) goto L_004296b5;                                   // 426d27 je 0x4296b5
    eax = ld32(m, ebx + 0x37c);                                    // 426d2d mov eax, [ebx + 0x37c]
    fl = f_cmp8(ld8(m, eax + 0x204), 0x0u);                        // 426d33 cmp byte ptr [eax + 0x204], 0
    if (fl.e()) goto L_004296b5;                                   // 426d3a je 0x4296b5
    fl = f_cmp(ld32(m, ebx + 0x374), 0x0u);                        // 426d40 cmp [ebx + 0x374], 0
    if (fl.ne()) goto L_00426d52;                                  // 426d47 jne 0x426d52
    fl = f_cmp8(ld8(m, ebx + 0x88), 0x0u);                         // 426d49 cmp byte ptr [ebx + 0x88], 0
    if (fl.e()) goto L_00426d5f;                                   // 426d50 je 0x426d5f
L_00426d52:
    fl = f_cmp(ld32(m, ebx + 0x98), 0x0u);                         // 426d52 cmp [ebx + 0x98], 0
    if (fl.e()) goto L_004296b5;                                   // 426d59 je 0x4296b5
L_00426d5f:
    fl = f_cmp(ld32(m, ebx + 0xa8a0), 0x0u);                       // 426d5f cmp [ebx + 0xa8a0], 0
    if (fl.ne()) goto L_004296b5;                                  // 426d66 jne 0x4296b5
    esi = ld32(m, eax + 0x1f8);                                    // 426d6c mov esi, [eax + 0x1f8]
    edi = ebx + 0x380;                                             // 426d72 lea edi, [ebx + 0x380]
    k.push(edi);                                                   // 426d78 push edi
    edx = esp + 0x4b4;                                             // 426d79 lea edx, [esp + 0x4b4]
    st32(m, esp + 0xa0, esi);                                      // 426d80 mov [esp + 0xa0], esi
    k.call(0x004225d0);  // FUN_004225d0                           // 426d87 call 0x4225d0
    fl = f_cmp8(ld8(m, 0xd72ad0), 0x0u);                           // 426d8c cmp byte ptr [0xd72ad0], 0
    x0 = X::ss(ld32(m, esp + 0x4b0));                              // 426d93 movss xmm0, [esp + 0x4b0]
    st32(m, esp + 0x230, x0.u[0]);                                 // 426d9c movss [esp + 0x230], xmm0
    x0 = X::ss(ld32(m, esp + 0x4b4));                              // 426da5 movss xmm0, [esp + 0x4b4]
    st32(m, esp + 0x234, x0.u[0]);                                 // 426dae movss [esp + 0x234], xmm0
    x0 = X::ss(ld32(m, esp + 0x4b8));                              // 426db7 movss xmm0, [esp + 0x4b8]
    st32(m, esp + 0x238, x0.u[0]);                                 // 426dc0 movss [esp + 0x238], xmm0
    if (fl.e()) goto L_00426dfc;                                   // 426dc9 je 0x426dfc
    x0 = X::ss(0x00000000u);                                       // 426dcb movss xmm0, [0x92e12c]
    x0 = shufps(x0, x0, 0x00);                                     // 426dd3 shufps xmm0, xmm0, 0
    x1 = x0;                                                       // 426dd7 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0xee);                                     // 426dda shufps xmm1, xmm0, 0xee
    x1 = movss(x1, x0);                                            // 426dde movss xmm1, xmm0
    x0 = x1;                                                       // 426de2 movaps xmm0, xmm1
    st(m, ebx + 0x410, x0);                                        // 426de5 movaps [ebx + 0x410], xmm0
    st(m, ebx + 0xe90, x0);                                        // 426dec movaps [ebx + 0xe90], xmm0
    x0 = ld(m, 0xd61670);                                          // 426df3 movaps xmm0, [0xd61670]
    goto L_00426e1c;                                               // 426dfa jmp 0x426e1c
L_00426dfc:
    x1 = X::ss(0x00000000u);                                       // 426dfc movss xmm1, [0x92e12c]
    x0 = X::ss(0xc200cccdu);                                       // 426e04 movss xmm0, [0x977688]
    x1 = shufps(x1, x1, 0x00);                                     // 426e0c shufps xmm1, xmm1, 0
    x0 = shufps(x0, x0, 0x00);                                     // 426e10 shufps xmm0, xmm0, 0
    x0 = shufps(x0, x1, 0xee);                                     // 426e14 shufps xmm0, xmm1, 0xee
    x0 = movss(x0, x1);                                            // 426e18 movss xmm0, xmm1
L_00426e1c:
    x4 = X::ss(0x80000000u);                                       // 426e1c movss xmm4, [0x9774a8]
    x3 = X{};                                                      // 426e24 xorps xmm3, xmm3
    st(m, edi, x0);                                                // 426e27 movaps [edi], xmm0
    x0 = ld(m, ebx + 0xe80);                                       // 426e2a movaps xmm0, [ebx + 0xe80]
    eax = ld32(m, ebx + 0x3fc);                                    // 426e31 mov eax, [ebx + 0x3fc]
    fl = f_cmp(eax, 0x1u);                                         // 426e37 cmp eax, 1
    st(m, esp + 0x4b0, x0);                                        // 426e3a movaps [esp + 0x4b0], xmm0
    x0 = X::ss(ld32(m, esp + 0x4b0));                              // 426e42 movss xmm0, [esp + 0x4b0]
    st32(m, esp + 0xc0, x0.u[0]);                                  // 426e4b movss [esp + 0xc0], xmm0
    x0 = X::ss(ld32(m, esp + 0x4b4));                              // 426e54 movss xmm0, [esp + 0x4b4]
    st32(m, esp + 0xc4, x0.u[0]);                                  // 426e5d movss [esp + 0xc4], xmm0
    x0 = X::ss(ld32(m, esp + 0x4b8));                              // 426e66 movss xmm0, [esp + 0x4b8]
    st32(m, esp + 0xc8, x0.u[0]);                                  // 426e6f movss [esp + 0xc8], xmm0
    x0 = X::ss(ld32(m, esp + 0x4bc));                              // 426e78 movss xmm0, [esp + 0x4bc]
    st32(m, esp + 0xcc, x0.u[0]);                                  // 426e81 movss [esp + 0xcc], xmm0
    x0 = ld(m, esp + 0xc0);                                        // 426e8a movdqa xmm0, [esp + 0xc0]
    st(m, esp + 0x370, x0);                                        // 426e93 movdqa [esp + 0x370], xmm0
    if (fl.le()) goto L_004274e9;                                  // 426e9c jle 0x4274e9
    eax = ld32(m, ebx + 0x37c);                                    // 426ea2 mov eax, [ebx + 0x37c]
    edx = ld32(m, eax + 0x240);                                    // 426ea8 mov edx, [eax + 0x240]
    ecx = ld32(m, edx + 0x8);                                      // 426eae mov ecx, [edx + 8]
    eax = ebx + 0xa544;                                            // 426eb1 lea eax, [ebx + 0xa544]
    st32(m, esp + 0x1c, eax);                                      // 426eb7 mov [esp + 0x1c], eax
    eax = ld32(m, ebx + 0x3fc);                                    // 426ebb mov eax, [ebx + 0x3fc]
    eax = eax - 1u;                                                // 426ec1 dec eax
    st32(m, esp + 0x5c, edx);                                      // 426ec2 mov [esp + 0x5c], edx
    st32(m, esp + 0x18, ecx);                                      // 426ec6 mov [esp + 0x18], ecx
    edi = esi + 0x3c;                                              // 426eca lea edi, [esi + 0x3c]
    st32(m, esp + 0x14, 0x0u);                                     // 426ecd mov [esp + 0x14], 0
    st32(m, esp + 0x58, eax);                                      // 426ed5 mov [esp + 0x58], eax
    goto L_00426ee4;                                               // 426ed9 jmp 0x426ee4
    goto L_00426ee0;                                               // 426edb jmp 0x426ee0
    ecx = ecx;                                                     // 426edd lea ecx, [ecx]
L_00426ee0:
    edx = ld32(m, esp + 0x5c);                                     // 426ee0 mov edx, [esp + 0x5c]
L_00426ee4:
    ecx = ld32(m, esp + 0x1c);                                     // 426ee4 mov ecx, [esp + 0x1c]
    ecx = ld32(m, ecx);                                            // 426ee8 mov ecx, [ecx]
    ecx = ecx << 4;                                                // 426eea shl ecx, 4
    ecx = ecx + ld32(m, esp + 0x18);                               // 426eed add ecx, [esp + 0x18]
    eax = uint32_t(int32_t(uint8_t(ld8(m, ecx + 0xf))));           // 426ef1 movzx eax, byte ptr [ecx + 0xf]
    ecx = ld32(m, ecx);                                            // 426ef5 mov ecx, [ecx]
    eax = eax << 4;                                                // 426ef7 shl eax, 4
    { uint32_t a_ = eax, b_ = ld32(m, ecx + 0x8), r_ = a_ + b_; fl = f_add(a_, b_, r_); eax = r_; } // 426efa add eax, [ecx + 8]
    if (fl.e()) goto L_00426f07;                                   // 426efd je 0x426f07
    eax = eax - ld32(m, edx + 0x8);                                // 426eff sub eax, [edx + 8]
    eax = uint32_t(int32_t(eax) >> 4);                             // 426f02 sar eax, 4
    goto L_00426f0a;                                               // 426f05 jmp 0x426f0a
L_00426f07:
    eax = eax | 0xffffffffu;                                       // 426f07 or eax, 0xffffffff
L_00426f0a:
    esi = eax;                                                     // 426f0a mov esi, eax
    edx = ebx;                                                     // 426f0c mov edx, ebx
    k.call(0x004296d0);  // FUN_004296d0                           // 426f0e call 0x4296d0
    fpush(m, reflex_f80::from_f32(ld32(m, edi + 0x4)));            // 426f13 fld [edi + 4]
    esi = ld32(m, esp + 0x14);                                     // 426f16 mov esi, [esp + 0x14]
    st32(m, esp + esi + 0x240, reflex_f80::to_f32(fst_(m, 0), frc(m))); m.fpu.pop_back(); // 426f1a fstp [esp + esi + 0x240]
    eax = eax << 4;                                                // 426f21 shl eax, 4
    fpush(m, reflex_f80::from_f32(ld32(m, edi + 0x8)));            // 426f24 fld [edi + 8]
    ecx = eax;                                                     // 426f27 mov ecx, eax
    x2 = X::ss(ld32(m, esp + ecx + 0x374));                        // 426f29 movss xmm2, [esp + ecx + 0x374]
    st32(m, esp + esi + 0x244, reflex_f80::to_f32(fst_(m, 0), frc(m))); m.fpu.pop_back(); // 426f32 fstp [esp + esi + 0x244]
    x7 = X::ss(ld32(m, esp + ecx + 0x378));                        // 426f39 movss xmm7, [esp + ecx + 0x378]
    fpush(m, reflex_f80::from_f32(ld32(m, edi + 0xc)));            // 426f42 fld [edi + 0xc]
    x0 = X::ss(ld32(m, esp + ecx + 0x370));                        // 426f45 movss xmm0, [esp + ecx + 0x370]
    st32(m, esp + esi + 0x248, reflex_f80::to_f32(fst_(m, 0), frc(m))); m.fpu.pop_back(); // 426f4e fstp [esp + esi + 0x248]
    x6 = X::ss(ld32(m, esp + esi + 0x240));                        // 426f55 movss xmm6, [esp + esi + 0x240]
    eax = esp + ecx + 0x370;                                       // 426f5e lea eax, [esp + ecx + 0x370]
    x6 = cvtps2pd(x6);                                             // 426f65 cvtps2pd xmm6, xmm6
    x5 = x4;                                                       // 426f68 movaps xmm5, xmm4
    x5 = subss(x5, x2);                                            // 426f6b subss xmm5, xmm2
    st32(m, esp + 0x24, x5.u[0]);                                  // 426f6f movss [esp + 0x24], xmm5
    x5 = x4;                                                       // 426f75 movaps xmm5, xmm4
    x5 = subss(x5, x7);                                            // 426f78 subss xmm5, xmm7
    st32(m, esp + 0x28, x5.u[0]);                                  // 426f7c movss [esp + 0x28], xmm5
    x5 = X::ss(ld32(m, eax + 0xc));                                // 426f82 movss xmm5, [eax + 0xc]
    x7 = cvtps2pd(x5);                                             // 426f87 cvtps2pd xmm7, xmm5
    st32(m, esp + 0x2c, x5.u[0]);                                  // 426f8a movss [esp + 0x2c], xmm5
    x7 = mulsd(x7, x3);                                            // 426f90 mulsd xmm7, xmm3
    x5 = cvtps2pd(x0);                                             // 426f94 cvtps2pd xmm5, xmm0
    x5 = mulsd(x5, x6);                                            // 426f97 mulsd xmm5, xmm6
    x7 = subsd(x7, x5);                                            // 426f9b subsd xmm7, xmm5
    x5 = X::ss(ld32(m, esp + esi + 0x244));                        // 426f9f movss xmm5, [esp + esi + 0x244]
    x5 = cvtps2pd(x5);                                             // 426fa8 cvtps2pd xmm5, xmm5
    x6 = cvtps2pd(x2);                                             // 426fab cvtps2pd xmm6, xmm2
    x6 = mulsd(x6, x5);                                            // 426fae mulsd xmm6, xmm5
    x5 = X::ss(ld32(m, eax + 0x8));                                // 426fb2 movss xmm5, [eax + 8]
    x7 = subsd(x7, x6);                                            // 426fb7 subsd xmm7, xmm6
    x6 = cvtps2pd(x5);                                             // 426fbb cvtps2pd xmm6, xmm5
    x5 = X::ss(ld32(m, esp + esi + 0x248));                        // 426fbe movss xmm5, [esp + esi + 0x248]
    x5 = cvtps2pd(x5);                                             // 426fc7 cvtps2pd xmm5, xmm5
    x6 = mulsd(x6, x5);                                            // 426fca mulsd xmm6, xmm5
    x7 = subsd(x7, x6);                                            // 426fce subsd xmm7, xmm6
    x5 = cvtpd2ps(x7);                                             // 426fd2 cvtpd2ps xmm5, xmm7
    st32(m, esp + 0x4c, x5.u[0]);                                  // 426fd6 movss [esp + 0x4c], xmm5
    x5 = X::ss(ld32(m, eax + 0xc));                                // 426fdc movss xmm5, [eax + 0xc]
    x6 = cvtps2pd(x5);                                             // 426fe1 cvtps2pd xmm6, xmm5
    x5 = X::ss(ld32(m, esp + esi + 0x240));                        // 426fe4 movss xmm5, [esp + esi + 0x240]
    x5 = cvtps2pd(x5);                                             // 426fed cvtps2pd xmm5, xmm5
    x6 = mulsd(x6, x5);                                            // 426ff0 mulsd xmm6, xmm5
    x5 = cvtps2pd(x0);                                             // 426ff4 cvtps2pd xmm5, xmm0
    x5 = mulsd(x5, x3);                                            // 426ff7 mulsd xmm5, xmm3
    x6 = addsd(x6, x5);                                            // 426ffb addsd xmm6, xmm5
    x5 = X::ss(ld32(m, esp + esi + 0x248));                        // 426fff movss xmm5, [esp + esi + 0x248]
    x5 = cvtps2pd(x5);                                             // 427008 cvtps2pd xmm5, xmm5
    x7 = cvtps2pd(x2);                                             // 42700b cvtps2pd xmm7, xmm2
    x7 = mulsd(x7, x5);                                            // 42700e mulsd xmm7, xmm5
    x5 = X::ss(ld32(m, eax + 0x8));                                // 427012 movss xmm5, [eax + 8]
    x6 = addsd(x6, x7);                                            // 427017 addsd xmm6, xmm7
    x7 = cvtps2pd(x5);                                             // 42701b cvtps2pd xmm7, xmm5
    x5 = X::ss(ld32(m, esp + esi + 0x244));                        // 42701e movss xmm5, [esp + esi + 0x244]
    x5 = cvtps2pd(x5);                                             // 427027 cvtps2pd xmm5, xmm5
    x7 = mulsd(x7, x5);                                            // 42702a mulsd xmm7, xmm5
    x6 = subsd(x6, x7);                                            // 42702e subsd xmm6, xmm7
    x5 = cvtpd2ps(x6);                                             // 427032 cvtpd2ps xmm5, xmm6
    st32(m, esp + 0x40, x5.u[0]);                                  // 427036 movss [esp + 0x40], xmm5
    x5 = X::ss(ld32(m, eax + 0xc));                                // 42703c movss xmm5, [eax + 0xc]
    x6 = cvtps2pd(x5);                                             // 427041 cvtps2pd xmm6, xmm5
    x5 = X::ss(ld32(m, esp + esi + 0x244));                        // 427044 movss xmm5, [esp + esi + 0x244]
    x5 = cvtps2pd(x5);                                             // 42704d cvtps2pd xmm5, xmm5
    x6 = mulsd(x6, x5);                                            // 427050 mulsd xmm6, xmm5
    x5 = cvtps2pd(x2);                                             // 427054 cvtps2pd xmm5, xmm2
    x5 = mulsd(x5, x3);                                            // 427057 mulsd xmm5, xmm3
    x6 = addsd(x6, x5);                                            // 42705b addsd xmm6, xmm5
    x5 = X::ss(ld32(m, eax + 0x8));                                // 42705f movss xmm5, [eax + 8]
    x7 = cvtps2pd(x5);                                             // 427064 cvtps2pd xmm7, xmm5
    x5 = X::ss(ld32(m, esp + esi + 0x240));                        // 427067 movss xmm5, [esp + esi + 0x240]
    x5 = cvtps2pd(x5);                                             // 427070 cvtps2pd xmm5, xmm5
    x1 = x4;                                                       // 427073 movaps xmm1, xmm4
    x7 = mulsd(x7, x5);                                            // 427076 mulsd xmm7, xmm5
    x1 = subss(x1, x0);                                            // 42707a subss xmm1, xmm0
    x6 = addsd(x6, x7);                                            // 42707e addsd xmm6, xmm7
    x5 = X::ss(ld32(m, esp + esi + 0x248));                        // 427082 movss xmm5, [esp + esi + 0x248]
    x7 = cvtps2pd(x0);                                             // 42708b cvtps2pd xmm7, xmm0
    x5 = cvtps2pd(x5);                                             // 42708e cvtps2pd xmm5, xmm5
    x7 = mulsd(x7, x5);                                            // 427091 mulsd xmm7, xmm5
    x6 = subsd(x6, x7);                                            // 427095 subsd xmm6, xmm7
    x5 = cvtpd2ps(x6);                                             // 427099 cvtpd2ps xmm5, xmm6
    x6 = X::ss(ld32(m, eax + 0xc));                                // 42709d movss xmm6, [eax + 0xc]
    x7 = cvtps2pd(x6);                                             // 4270a2 cvtps2pd xmm7, xmm6
    x6 = X::ss(ld32(m, esp + esi + 0x248));                        // 4270a5 movss xmm6, [esp + esi + 0x248]
    x6 = cvtps2pd(x6);                                             // 4270ae cvtps2pd xmm6, xmm6
    x7 = mulsd(x7, x6);                                            // 4270b1 mulsd xmm7, xmm6
    x6 = X::ss(ld32(m, eax + 0x8));                                // 4270b5 movss xmm6, [eax + 8]
    x6 = cvtps2pd(x6);                                             // 4270ba cvtps2pd xmm6, xmm6
    x6 = mulsd(x6, x3);                                            // 4270bd mulsd xmm6, xmm3
    x7 = addsd(x7, x6);                                            // 4270c1 addsd xmm7, xmm6
    x6 = cvtps2pd(x0);                                             // 4270c5 cvtps2pd xmm6, xmm0
    x0 = X::ss(ld32(m, esp + esi + 0x244));                        // 4270c8 movss xmm0, [esp + esi + 0x244]
    x0 = cvtps2pd(x0);                                             // 4270d1 cvtps2pd xmm0, xmm0
    x6 = mulsd(x6, x0);                                            // 4270d4 mulsd xmm6, xmm0
    x0 = X::ss(ld32(m, esp + esi + 0x240));                        // 4270d8 movss xmm0, [esp + esi + 0x240]
    x7 = addsd(x7, x6);                                            // 4270e1 addsd xmm7, xmm6
    x0 = cvtps2pd(x0);                                             // 4270e5 cvtps2pd xmm0, xmm0
    x2 = cvtps2pd(x2);                                             // 4270e8 cvtps2pd xmm2, xmm2
    x2 = mulsd(x2, x0);                                            // 4270eb mulsd xmm2, xmm0
    x0 = cvtss2sd(x0, X::ss(ld32(m, esp + 0x40)));                 // 4270ef cvtss2sd xmm0, [esp + 0x40]
    x7 = subsd(x7, x2);                                            // 4270f5 subsd xmm7, xmm2
    x6 = cvtpd2ps(x7);                                             // 4270f9 cvtpd2ps xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x2c)));                 // 4270fd cvtss2sd xmm7, [esp + 0x2c]
    x7 = mulsd(x7, x0);                                            // 427103 mulsd xmm7, xmm0
    x0 = cvtss2sd(x0, X::ss(ld32(m, esp + 0x4c)));                 // 427107 cvtss2sd xmm0, [esp + 0x4c]
    st32(m, esp + 0x44, x5.u[0]);                                  // 42710d movss [esp + 0x44], xmm5
    st32(m, esp + 0x48, x6.u[0]);                                  // 427113 movss [esp + 0x48], xmm6
    x5 = cvtss2sd(x5, x5);                                         // 427119 cvtss2sd xmm5, xmm5
    x6 = cvtss2sd(x6, x6);                                         // 42711d cvtss2sd xmm6, xmm6
    x2 = cvtss2sd(x2, x1);                                         // 427121 cvtss2sd xmm2, xmm1
    x0 = mulsd(x0, x2);                                            // 427125 mulsd xmm0, xmm2
    x2 = X::ss(ld32(m, esp + 0x28));                               // 427129 movss xmm2, [esp + 0x28]
    x7 = addsd(x7, x0);                                            // 42712f addsd xmm7, xmm0
    x0 = cvtps2pd(x2);                                             // 427133 cvtps2pd xmm0, xmm2
    x0 = mulsd(x0, x5);                                            // 427136 mulsd xmm0, xmm5
    x7 = addsd(x7, x0);                                            // 42713a addsd xmm7, xmm0
    x0 = X::ss(ld32(m, esp + 0x24));                               // 42713e movss xmm0, [esp + 0x24]
    x5 = cvtps2pd(x0);                                             // 427144 cvtps2pd xmm5, xmm0
    x5 = mulsd(x5, x6);                                            // 427147 mulsd xmm5, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x44)));                 // 42714b cvtss2sd xmm6, [esp + 0x44]
    x7 = subsd(x7, x5);                                            // 427151 subsd xmm7, xmm5
    x5 = cvtpd2ps(x7);                                             // 427155 cvtpd2ps xmm5, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x4c)));                 // 427159 cvtss2sd xmm7, [esp + 0x4c]
    st32(m, esp + 0x4c0, x5.u[0]);                                 // 42715f movss [esp + 0x4c0], xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0x2c)));                 // 427168 cvtss2sd xmm5, [esp + 0x2c]
    x5 = mulsd(x5, x6);                                            // 42716e mulsd xmm5, xmm6
    x6 = cvtps2pd(x0);                                             // 427172 cvtps2pd xmm6, xmm0
    x6 = mulsd(x6, x7);                                            // 427175 mulsd xmm6, xmm7
    x5 = addsd(x5, x6);                                            // 427179 addsd xmm5, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x48)));                 // 42717d cvtss2sd xmm6, [esp + 0x48]
    x7 = cvtss2sd(x7, x1);                                         // 427183 cvtss2sd xmm7, xmm1
    x6 = mulsd(x6, x7);                                            // 427187 mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x40)));                 // 42718b cvtss2sd xmm7, [esp + 0x40]
    x5 = addsd(x5, x6);                                            // 427191 addsd xmm5, xmm6
    x6 = cvtps2pd(x2);                                             // 427195 cvtps2pd xmm6, xmm2
    x6 = mulsd(x6, x7);                                            // 427198 mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x48)));                 // 42719c cvtss2sd xmm7, [esp + 0x48]
    x5 = subsd(x5, x6);                                            // 4271a2 subsd xmm5, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x2c)));                 // 4271a6 cvtss2sd xmm6, [esp + 0x2c]
    x6 = mulsd(x6, x7);                                            // 4271ac mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x4c)));                 // 4271b0 cvtss2sd xmm7, [esp + 0x4c]
    x2 = cvtps2pd(x2);                                             // 4271b6 cvtps2pd xmm2, xmm2
    x2 = mulsd(x2, x7);                                            // 4271b9 mulsd xmm2, xmm7
    x6 = addsd(x6, x2);                                            // 4271bd addsd xmm6, xmm2
    x2 = cvtss2sd(x2, X::ss(ld32(m, esp + 0x40)));                 // 4271c1 cvtss2sd xmm2, [esp + 0x40]
    x0 = cvtps2pd(x0);                                             // 4271c7 cvtps2pd xmm0, xmm0
    x0 = mulsd(x0, x2);                                            // 4271ca mulsd xmm0, xmm2
    x2 = X::ss(ld32(m, eax));                                      // 4271ce movss xmm2, [eax]
    x6 = addsd(x6, x0);                                            // 4271d2 addsd xmm6, xmm0
    x0 = cvtss2sd(x0, X::ss(ld32(m, esp + 0x44)));                 // 4271d6 cvtss2sd xmm0, [esp + 0x44]
    x1 = cvtss2sd(x1, x1);                                         // 4271dc cvtss2sd xmm1, xmm1
    x0 = mulsd(x0, x1);                                            // 4271e0 mulsd xmm0, xmm1
    x1 = X::ss(ld32(m, edi));                                      // 4271e4 movss xmm1, [edi]
    x6 = subsd(x6, x0);                                            // 4271e8 subsd xmm6, xmm0
    x0 = X::ss(ld32(m, eax + 0xc));                                // 4271ec movss xmm0, [eax + 0xc]
    ecx = esp + ecx + 0x230;                                       // 4271f1 lea ecx, [esp + ecx + 0x230]
    x6 = X::ss(ld32(m, eax + 0x4));                                // 4271f8 movss xmm6, [eax + 4]
    x5 = X::ss(ld32(m, edi - 0xc));                                // 4271fd movss xmm5, [edi - 0xc]
    x5 = cvtps2pd(x5);                                             // 427202 cvtps2pd xmm5, xmm5
    st32(m, esp + 0xc, x6.u[0]);                                   // 427205 movss [esp + 0xc], xmm6
    x6 = X::ss(ld32(m, eax + 0x8));                                // 42720b movss xmm6, [eax + 8]
    st32(m, esp + 0x10, x6.u[0]);                                  // 427210 movss [esp + 0x10], xmm6
    x6 = cvtps2pd(x0);                                             // 427216 cvtps2pd xmm6, xmm0
    x7 = cvtps2pd(x1);                                             // 427219 cvtps2pd xmm7, xmm1
    x6 = mulsd(x6, x7);                                            // 42721c mulsd xmm6, xmm7
    x7 = cvtps2pd(x2);                                             // 427220 cvtps2pd xmm7, xmm2
    x7 = mulsd(x7, x5);                                            // 427223 mulsd xmm7, xmm5
    x5 = X::ss(ld32(m, edi - 0x8));                                // 427227 movss xmm5, [edi - 8]
    x6 = subsd(x6, x7);                                            // 42722c subsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0xc)));                  // 427230 cvtss2sd xmm7, [esp + 0xc]
    x5 = cvtps2pd(x5);                                             // 427236 cvtps2pd xmm5, xmm5
    x7 = mulsd(x7, x5);                                            // 427239 mulsd xmm7, xmm5
    x5 = X::ss(ld32(m, edi - 0x4));                                // 42723d movss xmm5, [edi - 4]
    x6 = subsd(x6, x7);                                            // 427242 subsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x10)));                 // 427246 cvtss2sd xmm7, [esp + 0x10]
    x5 = cvtps2pd(x5);                                             // 42724c cvtps2pd xmm5, xmm5
    x7 = mulsd(x7, x5);                                            // 42724f mulsd xmm7, xmm5
    x6 = subsd(x6, x7);                                            // 427253 subsd xmm6, xmm7
    x5 = cvtpd2ps(x6);                                             // 427257 cvtpd2ps xmm5, xmm6
    st32(m, esp + 0x11c, x5.u[0]);                                 // 42725b movss [esp + 0x11c], xmm5
    x5 = X::ss(ld32(m, edi - 0xc));                                // 427264 movss xmm5, [edi - 0xc]
    x5 = cvtps2pd(x5);                                             // 427269 cvtps2pd xmm5, xmm5
    x6 = cvtps2pd(x0);                                             // 42726c cvtps2pd xmm6, xmm0
    x6 = mulsd(x6, x5);                                            // 42726f mulsd xmm6, xmm5
    x7 = cvtps2pd(x1);                                             // 427273 cvtps2pd xmm7, xmm1
    x5 = cvtps2pd(x2);                                             // 427276 cvtps2pd xmm5, xmm2
    x5 = mulsd(x5, x7);                                            // 427279 mulsd xmm5, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0xc)));                  // 42727d cvtss2sd xmm7, [esp + 0xc]
    x6 = addsd(x6, x5);                                            // 427283 addsd xmm6, xmm5
    x5 = X::ss(ld32(m, edi - 0x4));                                // 427287 movss xmm5, [edi - 4]
    x5 = cvtps2pd(x5);                                             // 42728c cvtps2pd xmm5, xmm5
    x7 = mulsd(x7, x5);                                            // 42728f mulsd xmm7, xmm5
    x5 = X::ss(ld32(m, edi - 0x8));                                // 427293 movss xmm5, [edi - 8]
    x5 = cvtps2pd(x5);                                             // 427298 cvtps2pd xmm5, xmm5
    x6 = addsd(x6, x7);                                            // 42729b addsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x10)));                 // 42729f cvtss2sd xmm7, [esp + 0x10]
    x7 = mulsd(x7, x5);                                            // 4272a5 mulsd xmm7, xmm5
    x6 = subsd(x6, x7);                                            // 4272a9 subsd xmm6, xmm7
    x5 = cvtpd2ps(x6);                                             // 4272ad cvtpd2ps xmm5, xmm6
    st32(m, esp + 0x110, x5.u[0]);                                 // 4272b1 movss [esp + 0x110], xmm5
    x5 = X::ss(ld32(m, edi - 0x8));                                // 4272ba movss xmm5, [edi - 8]
    x5 = cvtps2pd(x5);                                             // 4272bf cvtps2pd xmm5, xmm5
    x6 = cvtps2pd(x0);                                             // 4272c2 cvtps2pd xmm6, xmm0
    x6 = mulsd(x6, x5);                                            // 4272c5 mulsd xmm6, xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0xc)));                  // 4272c9 cvtss2sd xmm5, [esp + 0xc]
    x7 = cvtps2pd(x1);                                             // 4272cf cvtps2pd xmm7, xmm1
    x5 = mulsd(x5, x7);                                            // 4272d2 mulsd xmm5, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x10)));                 // 4272d6 cvtss2sd xmm7, [esp + 0x10]
    x6 = addsd(x6, x5);                                            // 4272dc addsd xmm6, xmm5
    x5 = X::ss(ld32(m, edi - 0xc));                                // 4272e0 movss xmm5, [edi - 0xc]
    x5 = cvtps2pd(x5);                                             // 4272e5 cvtps2pd xmm5, xmm5
    x7 = mulsd(x7, x5);                                            // 4272e8 mulsd xmm7, xmm5
    x5 = X::ss(ld32(m, edi - 0x4));                                // 4272ec movss xmm5, [edi - 4]
    x5 = cvtps2pd(x5);                                             // 4272f1 cvtps2pd xmm5, xmm5
    x6 = addsd(x6, x7);                                            // 4272f4 addsd xmm6, xmm7
    x7 = cvtps2pd(x2);                                             // 4272f8 cvtps2pd xmm7, xmm2
    x7 = mulsd(x7, x5);                                            // 4272fb mulsd xmm7, xmm5
    x6 = subsd(x6, x7);                                            // 4272ff subsd xmm6, xmm7
    x5 = cvtpd2ps(x6);                                             // 427303 cvtpd2ps xmm5, xmm6
    st32(m, esp + 0x114, x5.u[0]);                                 // 427307 movss [esp + 0x114], xmm5
    x5 = cvtps2pd(x0);                                             // 427310 cvtps2pd xmm5, xmm0
    x0 = X::ss(ld32(m, edi - 0x4));                                // 427313 movss xmm0, [edi - 4]
    x0 = cvtps2pd(x0);                                             // 427318 cvtps2pd xmm0, xmm0
    x5 = mulsd(x5, x0);                                            // 42731b mulsd xmm5, xmm0
    x0 = cvtss2sd(x0, X::ss(ld32(m, esp + 0x10)));                 // 42731f cvtss2sd xmm0, [esp + 0x10]
    x1 = cvtps2pd(x1);                                             // 427325 cvtps2pd xmm1, xmm1
    x0 = mulsd(x0, x1);                                            // 427328 mulsd xmm0, xmm1
    x5 = addsd(x5, x0);                                            // 42732c addsd xmm5, xmm0
    x0 = X::ss(ld32(m, edi - 0x8));                                // 427330 movss xmm0, [edi - 8]
    x0 = cvtps2pd(x0);                                             // 427335 cvtps2pd xmm0, xmm0
    x1 = cvtps2pd(x2);                                             // 427338 cvtps2pd xmm1, xmm2
    x1 = mulsd(x1, x0);                                            // 42733b mulsd xmm1, xmm0
    x0 = X::ss(ld32(m, edi - 0xc));                                // 42733f movss xmm0, [edi - 0xc]
    x5 = addsd(x5, x1);                                            // 427344 addsd xmm5, xmm1
    x1 = cvtss2sd(x1, X::ss(ld32(m, esp + 0xc)));                  // 427348 cvtss2sd xmm1, [esp + 0xc]
    x0 = cvtps2pd(x0);                                             // 42734e cvtps2pd xmm0, xmm0
    x1 = mulsd(x1, x0);                                            // 427351 mulsd xmm1, xmm0
    x5 = subsd(x5, x1);                                            // 427355 subsd xmm5, xmm1
    x0 = cvtpd2ps(x5);                                             // 427359 cvtpd2ps xmm0, xmm5
    st32(m, esp + 0x118, x0.u[0]);                                 // 42735d movss [esp + 0x118], xmm0
    x0 = sd(ld64(m, esp + 0x110));                                 // 427366 movq xmm0, [esp + 0x110]
    st64(m, esp + esi + 0x380, x0.q(0));                           // 42736f movq [esp + esi + 0x380], xmm0
    x0 = sd(ld64(m, esp + 0x118));                                 // 427378 movq xmm0, [esp + 0x118]
    st64(m, esp + esi + 0x388, x0.q(0));                           // 427381 movq [esp + esi + 0x388], xmm0
    x0 = X::ss(ld32(m, esp + esi + 0x38c));                        // 42738a movss xmm0, [esp + esi + 0x38c]
    x1 = X::ss(ld32(m, esp + esi + 0x380));                        // 427393 movss xmm1, [esp + esi + 0x380]
    x2 = X::ss(ld32(m, esp + esi + 0x384));                        // 42739c movss xmm2, [esp + esi + 0x384]
    x5 = X::ss(ld32(m, esp + esi + 0x388));                        // 4273a5 movss xmm5, [esp + esi + 0x388]
    st32(m, esp + 0x94, x0.u[0]);                                  // 4273ae movss [esp + 0x94], xmm0
    x0 = cvtps2pd(x0);                                             // 4273b7 cvtps2pd xmm0, xmm0
    x6 = x0;                                                       // 4273ba movapd xmm6, xmm0
    x6 = mulsd(x6, x0);                                            // 4273be mulsd xmm6, xmm0
    x1 = cvtps2pd(x1);                                             // 4273c2 cvtps2pd xmm1, xmm1
    x0 = x1;                                                       // 4273c5 movapd xmm0, xmm1
    x0 = mulsd(x0, x1);                                            // 4273c9 mulsd xmm0, xmm1
    x6 = addsd(x6, x0);                                            // 4273cd addsd xmm6, xmm0
    x2 = cvtps2pd(x2);                                             // 4273d1 cvtps2pd xmm2, xmm2
    x0 = x2;                                                       // 4273d4 movapd xmm0, xmm2
    x0 = mulsd(x0, x2);                                            // 4273d8 mulsd xmm0, xmm2
    x6 = addsd(x6, x0);                                            // 4273dc addsd xmm6, xmm0
    x5 = cvtps2pd(x5);                                             // 4273e0 cvtps2pd xmm5, xmm5
    x0 = x5;                                                       // 4273e3 movapd xmm0, xmm5
    x0 = mulsd(x0, x5);                                            // 4273e7 mulsd xmm0, xmm5
    x6 = addsd(x6, x0);                                            // 4273eb addsd xmm6, xmm0
    x0 = cvtpd2ps(x6);                                             // 4273ef cvtpd2ps xmm0, xmm6
    fl = f_comiss(x0.f(0), flt(0x00000000u));                      // 4273f3 ucomiss xmm0, [0x92e12c]
    eax = (eax & 0xffff00ffu) | (fl.lahf() << 8);                  // 4273fa lahf 
    st32(m, esp + 0x98, x0.u[0]);                                  // 4273fb movss [esp + 0x98], xmm0
    fl = f_test8(((eax >> 8) & 0xff), 0x44u);                      // 427404 test ah, 0x44
    if (fl.np()) goto L_004274b9;                                  // 427407 jnp 0x4274b9
    fpush(m, reflex_f80::from_f32(ld32(m, esp + 0x98)));           // 42740d fld [esp + 0x98]
    k.push(ecx);                                                   // 427414 push ecx
    st32(m, esp, reflex_f80::to_f32(fst_(m, 0), frc(m))); m.fpu.pop_back(); // 427415 fstp [esp]
    k.call(0x0042c230);  // FUN_0042c230                           // 427418 call 0x42c230
    x2 = cvtss2sd(x2, X::ss(ld32(m, esp + 0x98)));                 // 42741d cvtss2sd xmm2, [esp + 0x98]
    x3 = X{};                                                      // 427426 xorps xmm3, xmm3
    x4 = X::ss(0x80000000u);                                       // 427429 movss xmm4, [0x9774a8]
    x1 = x0;                                                       // 427431 movaps xmm1, xmm0
    x0 = X::ss(0x3f800000u);                                       // 427434 movss xmm0, [0x931f24]
    x0 = divss(x0, x1);                                            // 42743c divss xmm0, xmm1
    x1 = cvtps2pd(x0);                                             // 427440 cvtps2pd xmm1, xmm0
    x1 = mulsd(x1, x2);                                            // 427443 mulsd xmm1, xmm2
    x1 = cvtsd2ss(x1, x1);                                         // 427447 cvtsd2ss xmm1, xmm1
    st32(m, esp + esi + 0x390, x1.u[0]);                           // 42744b movss [esp + esi + 0x390], xmm1
    x2 = X::ss(ld32(m, esp + esi + 0x384));                        // 427454 movss xmm2, [esp + esi + 0x384]
    x1 = cvtps2pd(x0);                                             // 42745d cvtps2pd xmm1, xmm0
    x2 = cvtps2pd(x2);                                             // 427460 cvtps2pd xmm2, xmm2
    x1 = mulsd(x1, x2);                                            // 427463 mulsd xmm1, xmm2
    x1 = cvtsd2ss(x1, x1);                                         // 427467 cvtsd2ss xmm1, xmm1
    st32(m, esp + esi + 0x384, x1.u[0]);                           // 42746b movss [esp + esi + 0x384], xmm1
    x2 = X::ss(ld32(m, esp + esi + 0x388));                        // 427474 movss xmm2, [esp + esi + 0x388]
    x1 = cvtps2pd(x0);                                             // 42747d cvtps2pd xmm1, xmm0
    x2 = cvtps2pd(x2);                                             // 427480 cvtps2pd xmm2, xmm2
    x1 = mulsd(x1, x2);                                            // 427483 mulsd xmm1, xmm2
    x1 = cvtsd2ss(x1, x1);                                         // 427487 cvtsd2ss xmm1, xmm1
    st32(m, esp + esi + 0x388, x1.u[0]);                           // 42748b movss [esp + esi + 0x388], xmm1
    x1 = X::ss(ld32(m, esp + esi + 0x38c));                        // 427494 movss xmm1, [esp + esi + 0x38c]
    x0 = cvtps2pd(x0);                                             // 42749d cvtps2pd xmm0, xmm0
    x1 = cvtps2pd(x1);                                             // 4274a0 cvtps2pd xmm1, xmm1
    x0 = mulsd(x0, x1);                                            // 4274a3 mulsd xmm0, xmm1
    x0 = cvtpd2ps(x0);                                             // 4274a7 cvtpd2ps xmm0, xmm0
    esp = esp + 0x4u;                                              // 4274ab add esp, 4
    st32(m, esp + esi + 0x388, x0.u[0]);                           // 4274ae movss [esp + esi + 0x388], xmm0
    goto L_004274cf;                                               // 4274b7 jmp 0x4274cf
L_004274b9:
    x0 = X{};                                                      // 4274b9 pxor xmm0, xmm0
    st64(m, esp + esi + 0x380, x0.q(0));                           // 4274bd movq [esp + esi + 0x380], xmm0
    st64(m, esp + esi + 0x388, x0.q(0));                           // 4274c6 movq [esp + esi + 0x388], xmm0
L_004274cf:
    st32(m, esp + 0x1c, ld32(m, esp + 0x1c) + 0x4u);               // 4274cf add [esp + 0x1c], 4
    esi = esi + 0x10u;                                             // 4274d4 add esi, 0x10
    edi = edi + 0x30u;                                             // 4274d7 add edi, 0x30
    { uint32_t a_ = ld32(m, esp + 0x58), b_ = 0x1u, r_ = a_ - b_; fl = f_sub(a_, b_, r_); st32(m, esp + 0x58, r_); } // 4274da sub [esp + 0x58], 1
    st32(m, esp + 0x14, esi);                                      // 4274df mov [esp + 0x14], esi
    if (fl.ne()) goto L_00426ee0;                                  // 4274e3 jne 0x426ee0
L_004274e9:
    fl = f_cmp8(ld8(m, 0xd72ad1), 0x0u);                           // 4274e9 cmp byte ptr [0xd72ad1], 0
    if (fl.e()) goto L_0042757a;                                   // 4274f0 je 0x42757a
    fl = f_cmp(ld32(m, ebx + 0x3fc), 0x1u);                        // 4274f6 cmp [ebx + 0x3fc], 1
    if (fl.le()) goto L_0042757a;                                  // 4274fd jle 0x42757a
    edx = ld32(m, ebx + 0x37c);                                    // 4274ff mov edx, [ebx + 0x37c]
    edx = ld32(m, edx + 0x240);                                    // 427505 mov edx, [edx + 0x240]
    edi = ld32(m, ebx + 0x3fc);                                    // 42750b mov edi, [ebx + 0x3fc]
    eax = ld32(m, edx + 0x8);                                      // 427511 mov eax, [edx + 8]
    ecx = edi - 0x1;                                               // 427514 lea ecx, [edi - 1]
    st32(m, esp + 0x18, edx);                                      // 427517 mov [esp + 0x18], edx
    st32(m, esp + 0x5c, eax);                                      // 42751b mov [esp + 0x5c], eax
    esi = ebx + 0xa544;                                            // 42751f lea esi, [ebx + 0xa544]
    st32(m, esp + 0x14, ecx);                                      // 427525 mov [esp + 0x14], ecx
L_00427529:
    ecx = ld32(m, esi);                                            // 427529 mov ecx, [esi]
    ecx = ecx << 4;                                                // 42752b shl ecx, 4
    ecx = ecx + ld32(m, esp + 0x5c);                               // 42752e add ecx, [esp + 0x5c]
    eax = uint32_t(int32_t(uint8_t(ld8(m, ecx + 0xf))));           // 427532 movzx eax, byte ptr [ecx + 0xf]
    ecx = ld32(m, ecx);                                            // 427536 mov ecx, [ecx]
    eax = eax << 4;                                                // 427538 shl eax, 4
    { uint32_t a_ = eax, b_ = ld32(m, ecx + 0x8), r_ = a_ + b_; fl = f_add(a_, b_, r_); eax = r_; } // 42753b add eax, [ecx + 8]
    if (fl.e()) goto L_00427548;                                   // 42753e je 0x427548
    eax = eax - ld32(m, edx + 0x8);                                // 427540 sub eax, [edx + 8]
    eax = uint32_t(int32_t(eax) >> 4);                             // 427543 sar eax, 4
    goto L_0042754b;                                               // 427546 jmp 0x42754b
L_00427548:
    eax = eax | 0xffffffffu;                                       // 427548 or eax, 0xffffffff
L_0042754b:
    ecx = 0u;                                                      // 42754b xor ecx, ecx
    fl = f_cmp(ld32(m, ebx + 0x3fc), ecx);                         // 42754d cmp [ebx + 0x3fc], ecx
    if (fl.le()) goto L_00427570;                                  // 427553 jle 0x427570
    edx = ebx + 0xa540;                                            // 427555 lea edx, [ebx + 0xa540]
    goto L_00427560;                                               // 42755b jmp 0x427560
    ecx = ecx;                                                     // 42755d lea ecx, [ecx]
L_00427560:
    fl = f_cmp(ld32(m, edx), eax);                                 // 427560 cmp [edx], eax
    if (fl.e()) goto L_0042756c;                                   // 427562 je 0x42756c
    ecx = ecx + 1u;                                                // 427564 inc ecx
    edx = edx + 0x4u;                                              // 427565 add edx, 4
    fl = f_cmp(ecx, edi);                                          // 427568 cmp ecx, edi
    if (fl.l()) goto L_00427560;                                   // 42756a jl 0x427560
L_0042756c:
    edx = ld32(m, esp + 0x18);                                     // 42756c mov edx, [esp + 0x18]
L_00427570:
    esi = esi + 0x4u;                                              // 427570 add esi, 4
    { uint32_t a_ = ld32(m, esp + 0x14), b_ = 0x1u, r_ = a_ - b_; fl = f_sub(a_, b_, r_); st32(m, esp + 0x14, r_); } // 427573 sub [esp + 0x14], 1
    if (fl.ne()) goto L_00427529;                                  // 427578 jne 0x427529
L_0042757a:
    edi = 0x1u;                                                    // 42757a mov edi, 1
    fl = f_cmp(ld32(m, ebx + 0x3fc), edi);                         // 42757f cmp [ebx + 0x3fc], edi
    if (fl.le()) goto L_0042901d;                                  // 427585 jle 0x42901d
    x0 = sd(0x3ff921fb60000000ull);                                // 42758b movsd xmm0, [0x977510]
    eax = ld32(m, esp + 0x9c);                                     // 427593 mov eax, [esp + 0x9c]
    edx = ld32(m, ebx + 0x3fc);                                    // 42759a mov edx, [ebx + 0x3fc]
    x0 = subsd(x0, x3);                                            // 4275a0 subsd xmm0, xmm3
    st64(m, esp + 0x1b8, x0.q(0));                                 // 4275a4 movsd [esp + 0x1b8], xmm0
    x0 = sd(0x4018000000000000ull);                                // 4275ad movsd xmm0, [0x977668]
    x0 = subsd(x0, x3);                                            // 4275b5 subsd xmm0, xmm3
    st64(m, esp + 0x1c0, x0.q(0));                                 // 4275b9 movsd [esp + 0x1c0], xmm0
    x0 = X::ss(ld32(m, 0x9cd970));                                 // 4275c2 movss xmm0, [0x9cd970]
    eax = eax + 0x38u;                                             // 4275ca add eax, 0x38
    ecx = ebx + 0xa544;                                            // 4275cd lea ecx, [ebx + 0xa544]
    x0 = shufps(x0, x0, 0x00);                                     // 4275d3 shufps xmm0, xmm0, 0
    st(m, esp + 0x200, x0);                                        // 4275d7 movaps [esp + 0x200], xmm0
    st32(m, esp + 0x14, eax);                                      // 4275df mov [esp + 0x14], eax
    st32(m, esp + 0x58, ecx);                                      // 4275e3 mov [esp + 0x58], ecx
    esi = ebx + 0xef8;                                             // 4275e7 lea esi, [ebx + 0xef8]
    ecx = ecx;                                                     // 4275ed lea ecx, [ecx]
L_004275f0:
    ecx = ld32(m, esp + 0x58);                                     // 4275f0 mov ecx, [esp + 0x58]
    ecx = ld32(m, ecx);                                            // 4275f4 mov ecx, [ecx]
    eax = ld32(m, ebx + 0x37c);                                    // 4275f6 mov eax, [ebx + 0x37c]
    eax = ld32(m, eax + 0x240);                                    // 4275fc mov eax, [eax + 0x240]
    ecx = ecx << 4;                                                // 427602 shl ecx, 4
    ecx = ecx + ld32(m, eax + 0x8);                                // 427605 add ecx, [eax + 8]
    st32(m, esp + 0x18, eax);                                      // 427608 mov [esp + 0x18], eax
    eax = uint32_t(int32_t(uint8_t(ld8(m, ecx + 0xf))));           // 42760c movzx eax, byte ptr [ecx + 0xf]
    ecx = ld32(m, ecx);                                            // 427610 mov ecx, [ecx]
    eax = eax << 4;                                                // 427612 shl eax, 4
    { uint32_t a_ = eax, b_ = ld32(m, ecx + 0x8), r_ = a_ + b_; fl = f_add(a_, b_, r_); eax = r_; } // 427615 add eax, [ecx + 8]
    if (fl.e()) goto L_0042762a;                                   // 427618 je 0x42762a
    ecx = ld32(m, esp + 0x18);                                     // 42761a mov ecx, [esp + 0x18]
    eax = eax - ld32(m, ecx + 0x8);                                // 42761e sub eax, [ecx + 8]
    eax = uint32_t(int32_t(eax) >> 4);                             // 427621 sar eax, 4
    st32(m, esp + 0x18, eax);                                      // 427624 mov [esp + 0x18], eax
    goto L_00427632;                                               // 427628 jmp 0x427632
L_0042762a:
    st32(m, esp + 0x18, 0xffffffffu);                              // 42762a mov [esp + 0x18], 0xffffffff
L_00427632:
    fl = f_cmp(ld32(m, ebx + 0x3fc), 0x0u);                        // 427632 cmp [ebx + 0x3fc], 0
    st32(m, esp + 0xc, 0x0u);                                      // 427639 mov [esp + 0xc], 0
    if (fl.le()) goto L_00427662;                                  // 427641 jle 0x427662
    ecx = ebx + 0xa540;                                            // 427643 lea ecx, [ebx + 0xa540]
L_00427649:
    eax = ld32(m, esp + 0x18);                                     // 427649 mov eax, [esp + 0x18]
    fl = f_cmp(ld32(m, ecx), eax);                                 // 42764d cmp [ecx], eax
    if (fl.e()) goto L_00428adc;                                   // 42764f je 0x428adc
    st32(m, esp + 0xc, ld32(m, esp + 0xc) + 1u);                   // 427655 inc [esp + 0xc]
    ecx = ecx + 0x4u;                                              // 427659 add ecx, 4
    fl = f_cmp(ld32(m, esp + 0xc), edx);                           // 42765c cmp [esp + 0xc], edx
    if (fl.l()) goto L_00427649;                                   // 427660 jl 0x427649
L_00427662:
    eax = eax | 0xffffffffu;                                       // 427662 or eax, 0xffffffff
L_00427665:
    x6 = X::ss(ld32(m, esi + 0x4));                                // 427665 movss xmm6, [esi + 4]
    eax = eax * 0x70u;                                             // 42766a imul eax, eax, 0x70
    x7 = X::ss(ld32(m, eax + ebx + 0xe80));                        // 42766d movss xmm7, [eax + ebx + 0xe80]
    x5 = X::ss(ld32(m, eax + ebx + 0xe84));                        // 427676 movss xmm5, [eax + ebx + 0xe84]
    x0 = X::ss(ld32(m, eax + ebx + 0xe88));                        // 42767f movss xmm0, [eax + ebx + 0xe88]
    x2 = X::ss(ld32(m, esi - 0x8));                                // 427688 movss xmm2, [esi - 8]
    st32(m, esp + 0xa4, x5.u[0]);                                  // 42768d movss [esp + 0xa4], xmm5
    st32(m, esp + 0xa0, x7.u[0]);                                  // 427696 movss [esp + 0xa0], xmm7
    eax = eax + ebx + 0xe80;                                       // 42769f lea eax, [eax + ebx + 0xe80]
    x1 = x4;                                                       // 4276a6 movaps xmm1, xmm4
    x1 = subss(x1, x7);                                            // 4276a9 subss xmm1, xmm7
    x7 = x1;                                                       // 4276ad movaps xmm7, xmm1
    st32(m, esp + 0x10, x1.u[0]);                                  // 4276b0 movss [esp + 0x10], xmm1
    st32(m, esp + 0x120, x7.u[0]);                                 // 4276b6 movss [esp + 0x120], xmm7
    x1 = x4;                                                       // 4276bf movaps xmm1, xmm4
    x1 = subss(x1, x5);                                            // 4276c2 subss xmm1, xmm5
    st32(m, esp + 0x124, x1.u[0]);                                 // 4276c6 movss [esp + 0x124], xmm1
    st32(m, esp + 0x1c, x1.u[0]);                                  // 4276cf movss [esp + 0x1c], xmm1
    x1 = cvtps2pd(x6);                                             // 4276d5 cvtps2pd xmm1, xmm6
    st32(m, esp + 0xa8, x0.u[0]);                                  // 4276d8 movss [esp + 0xa8], xmm0
    x0 = X::ss(ld32(m, eax + 0xc));                                // 4276e1 movss xmm0, [eax + 0xc]
    x5 = x4;                                                       // 4276e6 movaps xmm5, xmm4
    x5 = subss(x5, X::ss(ld32(m, esp + 0xa8)));                    // 4276e9 subss xmm5, [esp + 0xa8]
    st32(m, esp + 0x128, x5.u[0]);                                 // 4276f2 movss [esp + 0x128], xmm5
    st32(m, esp + 0xb8, x5.u[0]);                                  // 4276fb movss [esp + 0xb8], xmm5
    x5 = cvtps2pd(x0);                                             // 427704 cvtps2pd xmm5, xmm0
    x5 = mulsd(x5, x1);                                            // 427707 mulsd xmm5, xmm1
    x1 = cvtps2pd(x7);                                             // 42770b cvtps2pd xmm1, xmm7
    x7 = cvtps2pd(x2);                                             // 42770e cvtps2pd xmm7, xmm2
    x1 = mulsd(x1, x7);                                            // 427711 mulsd xmm1, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x124)));                // 427715 cvtss2sd xmm7, [esp + 0x124]
    x5 = subsd(x5, x1);                                            // 42771e subsd xmm5, xmm1
    x1 = X::ss(ld32(m, esi - 0x4));                                // 427722 movss xmm1, [esi - 4]
    x1 = cvtps2pd(x1);                                             // 427727 cvtps2pd xmm1, xmm1
    x7 = mulsd(x7, x1);                                            // 42772a mulsd xmm7, xmm1
    x1 = X::ss(ld32(m, esi));                                      // 42772e movss xmm1, [esi]
    x5 = subsd(x5, x7);                                            // 427732 subsd xmm5, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x128)));                // 427736 cvtss2sd xmm7, [esp + 0x128]
    x6 = cvtps2pd(x6);                                             // 42773f cvtps2pd xmm6, xmm6
    x1 = cvtps2pd(x1);                                             // 427742 cvtps2pd xmm1, xmm1
    x7 = mulsd(x7, x1);                                            // 427745 mulsd xmm7, xmm1
    x5 = subsd(x5, x7);                                            // 427749 subsd xmm5, xmm7
    x1 = cvtpd2ps(x5);                                             // 42774d cvtpd2ps xmm1, xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0x120)));                // 427751 cvtss2sd xmm5, [esp + 0x120]
    x6 = mulsd(x6, x5);                                            // 42775a mulsd xmm6, xmm5
    x7 = cvtps2pd(x2);                                             // 42775e cvtps2pd xmm7, xmm2
    x5 = cvtps2pd(x0);                                             // 427761 cvtps2pd xmm5, xmm0
    x5 = mulsd(x5, x7);                                            // 427764 mulsd xmm5, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x124)));                // 427768 cvtss2sd xmm7, [esp + 0x124]
    x6 = addsd(x6, x5);                                            // 427771 addsd xmm6, xmm5
    x5 = X::ss(ld32(m, esi));                                      // 427775 movss xmm5, [esi]
    x5 = cvtps2pd(x5);                                             // 427779 cvtps2pd xmm5, xmm5
    x7 = mulsd(x7, x5);                                            // 42777c mulsd xmm7, xmm5
    x5 = X::ss(ld32(m, esi - 0x4));                                // 427780 movss xmm5, [esi - 4]
    x6 = addsd(x6, x7);                                            // 427785 addsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x128)));                // 427789 cvtss2sd xmm7, [esp + 0x128]
    x5 = cvtps2pd(x5);                                             // 427792 cvtps2pd xmm5, xmm5
    x7 = mulsd(x7, x5);                                            // 427795 mulsd xmm7, xmm5
    x6 = subsd(x6, x7);                                            // 427799 subsd xmm6, xmm7
    x5 = cvtpd2ps(x6);                                             // 42779d cvtpd2ps xmm5, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x124)));                // 4277a1 cvtss2sd xmm6, [esp + 0x124]
    st32(m, esp + 0x80, x5.u[0]);                                  // 4277aa movss [esp + 0x80], xmm5
    x5 = X::ss(ld32(m, esi + 0x4));                                // 4277b3 movss xmm5, [esi + 4]
    x5 = cvtps2pd(x5);                                             // 4277b8 cvtps2pd xmm5, xmm5
    x6 = mulsd(x6, x5);                                            // 4277bb mulsd xmm6, xmm5
    x5 = X::ss(ld32(m, esi - 0x4));                                // 4277bf movss xmm5, [esi - 4]
    x5 = cvtps2pd(x5);                                             // 4277c4 cvtps2pd xmm5, xmm5
    x7 = cvtps2pd(x0);                                             // 4277c7 cvtps2pd xmm7, xmm0
    x7 = mulsd(x7, x5);                                            // 4277ca mulsd xmm7, xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0x128)));                // 4277ce cvtss2sd xmm5, [esp + 0x128]
    x6 = addsd(x6, x7);                                            // 4277d7 addsd xmm6, xmm7
    x7 = cvtps2pd(x2);                                             // 4277db cvtps2pd xmm7, xmm2
    x5 = mulsd(x5, x7);                                            // 4277de mulsd xmm5, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x120)));                // 4277e2 cvtss2sd xmm7, [esp + 0x120]
    x6 = addsd(x6, x5);                                            // 4277eb addsd xmm6, xmm5
    x5 = X::ss(ld32(m, esi));                                      // 4277ef movss xmm5, [esi]
    x5 = cvtps2pd(x5);                                             // 4277f3 cvtps2pd xmm5, xmm5
    x5 = mulsd(x5, x7);                                            // 4277f6 mulsd xmm5, xmm7
    x6 = subsd(x6, x5);                                            // 4277fa subsd xmm6, xmm5
    x5 = cvtpd2ps(x6);                                             // 4277fe cvtpd2ps xmm5, xmm6
    st32(m, esp + 0x84, x5.u[0]);                                  // 427802 movss [esp + 0x84], xmm5
    x5 = X::ss(ld32(m, esi + 0x4));                                // 42780b movss xmm5, [esi + 4]
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x128)));                // 427810 cvtss2sd xmm6, [esp + 0x128]
    x5 = cvtps2pd(x5);                                             // 427819 cvtps2pd xmm5, xmm5
    x6 = mulsd(x6, x5);                                            // 42781c mulsd xmm6, xmm5
    x5 = X::ss(ld32(m, esi));                                      // 427820 movss xmm5, [esi]
    x5 = cvtps2pd(x5);                                             // 427824 cvtps2pd xmm5, xmm5
    x7 = cvtps2pd(x0);                                             // 427827 cvtps2pd xmm7, xmm0
    x7 = mulsd(x7, x5);                                            // 42782a mulsd xmm7, xmm5
    x5 = X::ss(ld32(m, esi - 0x4));                                // 42782e movss xmm5, [esi - 4]
    x5 = cvtps2pd(x5);                                             // 427833 cvtps2pd xmm5, xmm5
    x6 = addsd(x6, x7);                                            // 427836 addsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x120)));                // 42783a cvtss2sd xmm7, [esp + 0x120]
    x5 = mulsd(x5, x7);                                            // 427843 mulsd xmm5, xmm7
    x6 = addsd(x6, x5);                                            // 427847 addsd xmm6, xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0x124)));                // 42784b cvtss2sd xmm5, [esp + 0x124]
    eax = ld32(m, esp + 0x14);                                     // 427854 mov eax, [esp + 0x14]
    x2 = cvtps2pd(x2);                                             // 427858 cvtps2pd xmm2, xmm2
    x5 = mulsd(x5, x2);                                            // 42785b mulsd xmm5, xmm2
    x6 = subsd(x6, x5);                                            // 42785f subsd xmm6, xmm5
    x7 = cvtpd2ps(x6);                                             // 427863 cvtpd2ps xmm7, xmm6
    x6 = X::ss(ld32(m, esp + 0x80));                               // 427867 movss xmm6, [esp + 0x80]
    x2 = X::ss(ld32(m, eax - 0x8));                                // 427870 movss xmm2, [eax - 8]
    x5 = x4;                                                       // 427875 movaps xmm5, xmm4
    x5 = subss(x5, x6);                                            // 427878 subss xmm5, xmm6
    st32(m, esp + 0x94, x5.u[0]);                                  // 42787c movss [esp + 0x94], xmm5
    st32(m, esp + 0x130, x5.u[0]);                                 // 427885 movss [esp + 0x130], xmm5
    x6 = cvtps2pd(x6);                                             // 42788e cvtps2pd xmm6, xmm6
    st32(m, esp + 0x88, x7.u[0]);                                  // 427891 movss [esp + 0x88], xmm7
    x5 = x4;                                                       // 42789a movaps xmm5, xmm4
    x5 = subss(x5, X::ss(ld32(m, esp + 0x84)));                    // 42789d subss xmm5, [esp + 0x84]
    st32(m, esp + 0x9c, x5.u[0]);                                  // 4278a6 movss [esp + 0x9c], xmm5
    st32(m, esp + 0x134, x5.u[0]);                                 // 4278af movss [esp + 0x134], xmm5
    x5 = x4;                                                       // 4278b8 movaps xmm5, xmm4
    x5 = subss(x5, x7);                                            // 4278bb subss xmm5, xmm7
    x7 = X::ss(ld32(m, 0x9cb4a0));                                 // 4278bf movss xmm7, [0x9cb4a0]
    st32(m, esp + 0xbc, x5.u[0]);                                  // 4278c7 movss [esp + 0xbc], xmm5
    st32(m, esp + 0x138, x5.u[0]);                                 // 4278d0 movss [esp + 0x138], xmm5
    x7 = cvtps2pd(x7);                                             // 4278d9 cvtps2pd xmm7, xmm7
    x7 = mulsd(x7, x6);                                            // 4278dc mulsd xmm7, xmm6
    x5 = cvtss2sd(x5, x1);                                         // 4278e0 cvtss2sd xmm5, xmm1
    x5 = mulsd(x5, x3);                                            // 4278e4 mulsd xmm5, xmm3
    x6 = x5;                                                       // 4278e8 movapd xmm6, xmm5
    st64(m, esp + 0x218, x5.q(0));                                 // 4278ec movsd [esp + 0x218], xmm5
    x5 = X::ss(ld32(m, 0x9cb4a4));                                 // 4278f5 movss xmm5, [0x9cb4a4]
    x6 = subsd(x6, x7);                                            // 4278fd subsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x84)));                 // 427901 cvtss2sd xmm7, [esp + 0x84]
    x5 = cvtps2pd(x5);                                             // 42790a cvtps2pd xmm5, xmm5
    x5 = mulsd(x5, x7);                                            // 42790d mulsd xmm5, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x88)));                 // 427911 cvtss2sd xmm7, [esp + 0x88]
    x6 = subsd(x6, x5);                                            // 42791a subsd xmm6, xmm5
    x5 = X::ss(ld32(m, 0x9cb4a8));                                 // 42791e movss xmm5, [0x9cb4a8]
    x5 = cvtps2pd(x5);                                             // 427926 cvtps2pd xmm5, xmm5
    x5 = mulsd(x5, x7);                                            // 427929 mulsd xmm5, xmm7
    x6 = subsd(x6, x5);                                            // 42792d subsd xmm6, xmm5
    x5 = cvtpd2ps(x6);                                             // 427931 cvtpd2ps xmm5, xmm6
    x6 = X::ss(ld32(m, 0x9cb4a0));                                 // 427935 movss xmm6, [0x9cb4a0]
    st32(m, esp + 0x10c, x5.u[0]);                                 // 42793d movss [esp + 0x10c], xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0x80)));                 // 427946 cvtss2sd xmm5, [esp + 0x80]
    x5 = mulsd(x5, x3);                                            // 42794f mulsd xmm5, xmm3
    st64(m, esp + 0x1f8, x5.q(0));                                 // 427953 movsd [esp + 0x1f8], xmm5
    x6 = cvtps2pd(x6);                                             // 42795c cvtps2pd xmm6, xmm6
    x7 = cvtss2sd(x7, x1);                                         // 42795f cvtss2sd xmm7, xmm1
    x6 = mulsd(x6, x7);                                            // 427963 mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x84)));                 // 427967 cvtss2sd xmm7, [esp + 0x84]
    x6 = addsd(x6, x5);                                            // 427970 addsd xmm6, xmm5
    x5 = X::ss(ld32(m, 0x9cb4a8));                                 // 427974 movss xmm5, [0x9cb4a8]
    x5 = cvtps2pd(x5);                                             // 42797c cvtps2pd xmm5, xmm5
    x5 = mulsd(x5, x7);                                            // 42797f mulsd xmm5, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x88)));                 // 427983 cvtss2sd xmm7, [esp + 0x88]
    x6 = addsd(x6, x5);                                            // 42798c addsd xmm6, xmm5
    x5 = X::ss(ld32(m, 0x9cb4a4));                                 // 427990 movss xmm5, [0x9cb4a4]
    st32(m, esp + 0x70, x2.u[0]);                                  // 427998 movss [esp + 0x70], xmm2
    x2 = X::ss(ld32(m, eax - 0x4));                                // 42799e movss xmm2, [eax - 4]
    st32(m, esp + 0x74, x2.u[0]);                                  // 4279a3 movss [esp + 0x74], xmm2
    x2 = X::ss(ld32(m, eax));                                      // 4279a9 movss xmm2, [eax]
    x5 = cvtps2pd(x5);                                             // 4279ad cvtps2pd xmm5, xmm5
    x5 = mulsd(x5, x7);                                            // 4279b0 mulsd xmm5, xmm7
    st32(m, esp + 0x78, x2.u[0]);                                  // 4279b4 movss [esp + 0x78], xmm2
    x2 = X::ss(ld32(m, eax + 0x4));                                // 4279ba movss xmm2, [eax + 4]
    x6 = subsd(x6, x5);                                            // 4279bf subsd xmm6, xmm5
    x5 = cvtpd2ps(x6);                                             // 4279c3 cvtpd2ps xmm5, xmm6
    st32(m, esp + 0x100, x5.u[0]);                                 // 4279c7 movss [esp + 0x100], xmm5
    x6 = X::ss(ld32(m, 0x9cb4a4));                                 // 4279d0 movss xmm6, [0x9cb4a4]
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0x84)));                 // 4279d8 cvtss2sd xmm5, [esp + 0x84]
    x5 = mulsd(x5, x3);                                            // 4279e1 mulsd xmm5, xmm3
    st64(m, esp + 0x1d0, x5.q(0));                                 // 4279e5 movsd [esp + 0x1d0], xmm5
    x6 = cvtps2pd(x6);                                             // 4279ee cvtps2pd xmm6, xmm6
    x7 = cvtss2sd(x7, x1);                                         // 4279f1 cvtss2sd xmm7, xmm1
    x6 = mulsd(x6, x7);                                            // 4279f5 mulsd xmm6, xmm7
    x6 = addsd(x6, x5);                                            // 4279f9 addsd xmm6, xmm5
    x5 = X::ss(ld32(m, 0x9cb4a0));                                 // 4279fd movss xmm5, [0x9cb4a0]
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x88)));                 // 427a05 cvtss2sd xmm7, [esp + 0x88]
    x5 = cvtps2pd(x5);                                             // 427a0e cvtps2pd xmm5, xmm5
    x5 = mulsd(x5, x7);                                            // 427a11 mulsd xmm5, xmm7
    x6 = addsd(x6, x5);                                            // 427a15 addsd xmm6, xmm5
    x5 = X::ss(ld32(m, 0x9cb4a8));                                 // 427a19 movss xmm5, [0x9cb4a8]
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x80)));                 // 427a21 cvtss2sd xmm7, [esp + 0x80]
    x5 = cvtps2pd(x5);                                             // 427a2a cvtps2pd xmm5, xmm5
    x5 = mulsd(x5, x7);                                            // 427a2d mulsd xmm5, xmm7
    x6 = subsd(x6, x5);                                            // 427a31 subsd xmm6, xmm5
    x5 = cvtpd2ps(x6);                                             // 427a35 cvtpd2ps xmm5, xmm6
    x6 = X::ss(ld32(m, 0x9cb4a8));                                 // 427a39 movss xmm6, [0x9cb4a8]
    st32(m, esp + 0x104, x5.u[0]);                                 // 427a41 movss [esp + 0x104], xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0x88)));                 // 427a4a cvtss2sd xmm5, [esp + 0x88]
    x5 = mulsd(x5, x3);                                            // 427a53 mulsd xmm5, xmm3
    x6 = cvtps2pd(x6);                                             // 427a57 cvtps2pd xmm6, xmm6
    st64(m, esp + 0x1d8, x5.q(0));                                 // 427a5a movsd [esp + 0x1d8], xmm5
    x7 = cvtss2sd(x7, x1);                                         // 427a63 cvtss2sd xmm7, xmm1
    x6 = mulsd(x6, x7);                                            // 427a67 mulsd xmm6, xmm7
    x6 = addsd(x6, x5);                                            // 427a6b addsd xmm6, xmm5
    x5 = X::ss(ld32(m, 0x9cb4a4));                                 // 427a6f movss xmm5, [0x9cb4a4]
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x80)));                 // 427a77 cvtss2sd xmm7, [esp + 0x80]
    x5 = cvtps2pd(x5);                                             // 427a80 cvtps2pd xmm5, xmm5
    x5 = mulsd(x5, x7);                                            // 427a83 mulsd xmm5, xmm7
    x6 = addsd(x6, x5);                                            // 427a87 addsd xmm6, xmm5
    x5 = X::ss(ld32(m, 0x9cb4a0));                                 // 427a8b movss xmm5, [0x9cb4a0]
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x84)));                 // 427a93 cvtss2sd xmm7, [esp + 0x84]
    x5 = cvtps2pd(x5);                                             // 427a9c cvtps2pd xmm5, xmm5
    x5 = mulsd(x5, x7);                                            // 427a9f mulsd xmm5, xmm7
    x6 = subsd(x6, x5);                                            // 427aa3 subsd xmm6, xmm5
    x5 = cvtpd2ps(x6);                                             // 427aa7 cvtpd2ps xmm5, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x130)));                // 427aab cvtss2sd xmm6, [esp + 0x130]
    st32(m, esp + 0x108, x5.u[0]);                                 // 427ab4 movss [esp + 0x108], xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0x10c)));                // 427abd cvtss2sd xmm5, [esp + 0x10c]
    x5 = mulsd(x5, x6);                                            // 427ac6 mulsd xmm5, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x100)));                // 427aca cvtss2sd xmm6, [esp + 0x100]
    x7 = cvtss2sd(x7, x1);                                         // 427ad3 cvtss2sd xmm7, xmm1
    x6 = mulsd(x6, x7);                                            // 427ad7 mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x104)));                // 427adb cvtss2sd xmm7, [esp + 0x104]
    x5 = addsd(x5, x6);                                            // 427ae4 addsd xmm5, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x138)));                // 427ae8 cvtss2sd xmm6, [esp + 0x138]
    x6 = mulsd(x6, x7);                                            // 427af1 mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x108)));                // 427af5 cvtss2sd xmm7, [esp + 0x108]
    x5 = addsd(x5, x6);                                            // 427afe addsd xmm5, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x134)));                // 427b02 cvtss2sd xmm6, [esp + 0x134]
    x6 = mulsd(x6, x7);                                            // 427b0b mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x10c)));                // 427b0f cvtss2sd xmm7, [esp + 0x10c]
    x5 = subsd(x5, x6);                                            // 427b18 subsd xmm5, xmm6
    x5 = cvtpd2ps(x5);                                             // 427b1c cvtpd2ps xmm5, xmm5
    st32(m, esp + 0x20, x5.u[0]);                                  // 427b20 movss [esp + 0x20], xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0x104)));                // 427b26 cvtss2sd xmm5, [esp + 0x104]
    x6 = cvtss2sd(x6, x1);                                         // 427b2f cvtss2sd xmm6, xmm1
    x5 = mulsd(x5, x6);                                            // 427b33 mulsd xmm5, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x134)));                // 427b37 cvtss2sd xmm6, [esp + 0x134]
    x6 = mulsd(x6, x7);                                            // 427b40 mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x130)));                // 427b44 cvtss2sd xmm7, [esp + 0x130]
    x5 = addsd(x5, x6);                                            // 427b4d addsd xmm5, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x108)));                // 427b51 cvtss2sd xmm6, [esp + 0x108]
    x6 = mulsd(x6, x7);                                            // 427b5a mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x100)));                // 427b5e cvtss2sd xmm7, [esp + 0x100]
    x5 = addsd(x5, x6);                                            // 427b67 addsd xmm5, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x138)));                // 427b6b cvtss2sd xmm6, [esp + 0x138]
    x6 = mulsd(x6, x7);                                            // 427b74 mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x10c)));                // 427b78 cvtss2sd xmm7, [esp + 0x10c]
    x5 = subsd(x5, x6);                                            // 427b81 subsd xmm5, xmm6
    x5 = cvtpd2ps(x5);                                             // 427b85 cvtpd2ps xmm5, xmm5
    st32(m, esp + 0x24, x5.u[0]);                                  // 427b89 movss [esp + 0x24], xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0x108)));                // 427b8f cvtss2sd xmm5, [esp + 0x108]
    x6 = cvtss2sd(x6, x1);                                         // 427b98 cvtss2sd xmm6, xmm1
    x5 = mulsd(x5, x6);                                            // 427b9c mulsd xmm5, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x138)));                // 427ba0 cvtss2sd xmm6, [esp + 0x138]
    x6 = mulsd(x6, x7);                                            // 427ba9 mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x100)));                // 427bad cvtss2sd xmm7, [esp + 0x100]
    x5 = addsd(x5, x6);                                            // 427bb6 addsd xmm5, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x134)));                // 427bba cvtss2sd xmm6, [esp + 0x134]
    x6 = mulsd(x6, x7);                                            // 427bc3 mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x130)));                // 427bc7 cvtss2sd xmm7, [esp + 0x130]
    x5 = addsd(x5, x6);                                            // 427bd0 addsd xmm5, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x104)));                // 427bd4 cvtss2sd xmm6, [esp + 0x104]
    x6 = mulsd(x6, x7);                                            // 427bdd mulsd xmm6, xmm7
    x7 = X::ss(ld32(m, esp + 0x74));                               // 427be1 movss xmm7, [esp + 0x74]
    x5 = subsd(x5, x6);                                            // 427be7 subsd xmm5, xmm6
    x6 = X::ss(ld32(m, esp + 0x70));                               // 427beb movss xmm6, [esp + 0x70]
    x5 = cvtpd2ps(x5);                                             // 427bf1 cvtpd2ps xmm5, xmm5
    st32(m, esp + 0x28, x5.u[0]);                                  // 427bf5 movss [esp + 0x28], xmm5
    x5 = x4;                                                       // 427bfb movaps xmm5, xmm4
    x5 = subss(x5, x6);                                            // 427bfe subss xmm5, xmm6
    x6 = cvtps2pd(x6);                                             // 427c02 cvtps2pd xmm6, xmm6
    st32(m, esp + 0x198, x5.u[0]);                                 // 427c05 movss [esp + 0x198], xmm5
    st32(m, esp + 0x140, x5.u[0]);                                 // 427c0e movss [esp + 0x140], xmm5
    x5 = x4;                                                       // 427c17 movaps xmm5, xmm4
    x4 = subss(x4, X::ss(ld32(m, esp + 0x78)));                    // 427c1a subss xmm4, [esp + 0x78]
    st32(m, esp + 0x98, x4.u[0]);                                  // 427c20 movss [esp + 0x98], xmm4
    st32(m, esp + 0x148, x4.u[0]);                                 // 427c29 movss [esp + 0x148], xmm4
    x4 = X::ss(ld32(m, 0x9cb4a0));                                 // 427c32 movss xmm4, [0x9cb4a0]
    x4 = cvtps2pd(x4);                                             // 427c3a cvtps2pd xmm4, xmm4
    x4 = mulsd(x4, x6);                                            // 427c3d mulsd xmm4, xmm6
    x5 = subss(x5, x7);                                            // 427c41 subss xmm5, xmm7
    x7 = cvtps2pd(x7);                                             // 427c45 cvtps2pd xmm7, xmm7
    st32(m, esp + 0x190, x5.u[0]);                                 // 427c48 movss [esp + 0x190], xmm5
    st32(m, esp + 0x144, x5.u[0]);                                 // 427c51 movss [esp + 0x144], xmm5
    x5 = cvtps2pd(x2);                                             // 427c5a cvtps2pd xmm5, xmm2
    x5 = mulsd(x5, x3);                                            // 427c5d mulsd xmm5, xmm3
    x6 = x5;                                                       // 427c61 movapd xmm6, xmm5
    x6 = subsd(x6, x4);                                            // 427c65 subsd xmm6, xmm4
    x4 = X::ss(ld32(m, 0x9cb4a4));                                 // 427c69 movss xmm4, [0x9cb4a4]
    x4 = cvtps2pd(x4);                                             // 427c71 cvtps2pd xmm4, xmm4
    x7 = mulsd(x7, x4);                                            // 427c74 mulsd xmm7, xmm4
    x4 = X::ss(ld32(m, 0x9cb4a8));                                 // 427c78 movss xmm4, [0x9cb4a8]
    x6 = subsd(x6, x7);                                            // 427c80 subsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x78)));                 // 427c84 cvtss2sd xmm7, [esp + 0x78]
    x4 = cvtps2pd(x4);                                             // 427c8a cvtps2pd xmm4, xmm4
    x7 = mulsd(x7, x4);                                            // 427c8d mulsd xmm7, xmm4
    x6 = subsd(x6, x7);                                            // 427c91 subsd xmm6, xmm7
    x4 = cvtpd2ps(x6);                                             // 427c95 cvtpd2ps xmm4, xmm6
    x6 = X::ss(ld32(m, 0x9cb4a0));                                 // 427c99 movss xmm6, [0x9cb4a0]
    st32(m, esp + 0xdc, x4.u[0]);                                  // 427ca1 movss [esp + 0xdc], xmm4
    x4 = cvtss2sd(x4, X::ss(ld32(m, esp + 0x70)));                 // 427caa cvtss2sd xmm4, [esp + 0x70]
    x4 = mulsd(x4, x3);                                            // 427cb0 mulsd xmm4, xmm3
    st64(m, esp + 0x1c8, x4.q(0));                                 // 427cb4 movsd [esp + 0x1c8], xmm4
    x6 = cvtps2pd(x6);                                             // 427cbd cvtps2pd xmm6, xmm6
    x7 = cvtps2pd(x2);                                             // 427cc0 cvtps2pd xmm7, xmm2
    x7 = mulsd(x7, x6);                                            // 427cc3 mulsd xmm7, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x74)));                 // 427cc7 cvtss2sd xmm6, [esp + 0x74]
    x7 = addsd(x7, x4);                                            // 427ccd addsd xmm7, xmm4
    x4 = X::ss(ld32(m, 0x9cb4a8));                                 // 427cd1 movss xmm4, [0x9cb4a8]
    x4 = cvtps2pd(x4);                                             // 427cd9 cvtps2pd xmm4, xmm4
    x6 = mulsd(x6, x4);                                            // 427cdc mulsd xmm6, xmm4
    x4 = X::ss(ld32(m, 0x9cb4a4));                                 // 427ce0 movss xmm4, [0x9cb4a4]
    x7 = addsd(x7, x6);                                            // 427ce8 addsd xmm7, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x78)));                 // 427cec cvtss2sd xmm6, [esp + 0x78]
    x4 = cvtps2pd(x4);                                             // 427cf2 cvtps2pd xmm4, xmm4
    x6 = mulsd(x6, x4);                                            // 427cf5 mulsd xmm6, xmm4
    x7 = subsd(x7, x6);                                            // 427cf9 subsd xmm7, xmm6
    x6 = X::ss(ld32(m, 0x9cb4a4));                                 // 427cfd movss xmm6, [0x9cb4a4]
    x4 = cvtpd2ps(x7);                                             // 427d05 cvtpd2ps xmm4, xmm7
    st32(m, esp + 0xd0, x4.u[0]);                                  // 427d09 movss [esp + 0xd0], xmm4
    x4 = cvtss2sd(x4, X::ss(ld32(m, esp + 0x74)));                 // 427d12 cvtss2sd xmm4, [esp + 0x74]
    x4 = mulsd(x4, x3);                                            // 427d18 mulsd xmm4, xmm3
    x6 = cvtps2pd(x6);                                             // 427d1c cvtps2pd xmm6, xmm6
    st64(m, esp + 0x1e0, x4.q(0));                                 // 427d1f movsd [esp + 0x1e0], xmm4
    x7 = cvtps2pd(x2);                                             // 427d28 cvtps2pd xmm7, xmm2
    x7 = mulsd(x7, x6);                                            // 427d2b mulsd xmm7, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x78)));                 // 427d2f cvtss2sd xmm6, [esp + 0x78]
    x7 = addsd(x7, x4);                                            // 427d35 addsd xmm7, xmm4
    x4 = X::ss(ld32(m, 0x9cb4a0));                                 // 427d39 movss xmm4, [0x9cb4a0]
    x4 = cvtps2pd(x4);                                             // 427d41 cvtps2pd xmm4, xmm4
    x6 = mulsd(x6, x4);                                            // 427d44 mulsd xmm6, xmm4
    x4 = X::ss(ld32(m, 0x9cb4a8));                                 // 427d48 movss xmm4, [0x9cb4a8]
    x7 = addsd(x7, x6);                                            // 427d50 addsd xmm7, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x70)));                 // 427d54 cvtss2sd xmm6, [esp + 0x70]
    x4 = cvtps2pd(x4);                                             // 427d5a cvtps2pd xmm4, xmm4
    x4 = mulsd(x4, x6);                                            // 427d5d mulsd xmm4, xmm6
    x7 = subsd(x7, x4);                                            // 427d61 subsd xmm7, xmm4
    x6 = X::ss(ld32(m, 0x9cb4a8));                                 // 427d65 movss xmm6, [0x9cb4a8]
    x6 = cvtps2pd(x6);                                             // 427d6d cvtps2pd xmm6, xmm6
    x4 = cvtpd2ps(x7);                                             // 427d70 cvtpd2ps xmm4, xmm7
    st32(m, esp + 0xd4, x4.u[0]);                                  // 427d74 movss [esp + 0xd4], xmm4
    x4 = cvtss2sd(x4, X::ss(ld32(m, esp + 0x78)));                 // 427d7d cvtss2sd xmm4, [esp + 0x78]
    x4 = mulsd(x4, x3);                                            // 427d83 mulsd xmm4, xmm3
    st64(m, esp + 0x1e8, x4.q(0));                                 // 427d87 movsd [esp + 0x1e8], xmm4
    x7 = cvtps2pd(x2);                                             // 427d90 cvtps2pd xmm7, xmm2
    x7 = mulsd(x7, x6);                                            // 427d93 mulsd xmm7, xmm6
    x7 = addsd(x7, x4);                                            // 427d97 addsd xmm7, xmm4
    x4 = X::ss(ld32(m, 0x9cb4a4));                                 // 427d9b movss xmm4, [0x9cb4a4]
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x70)));                 // 427da3 cvtss2sd xmm6, [esp + 0x70]
    x4 = cvtps2pd(x4);                                             // 427da9 cvtps2pd xmm4, xmm4
    x4 = mulsd(x4, x6);                                            // 427dac mulsd xmm4, xmm6
    x7 = addsd(x7, x4);                                            // 427db0 addsd xmm7, xmm4
    x4 = X::ss(ld32(m, 0x9cb4a0));                                 // 427db4 movss xmm4, [0x9cb4a0]
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x74)));                 // 427dbc cvtss2sd xmm6, [esp + 0x74]
    x4 = cvtps2pd(x4);                                             // 427dc2 cvtps2pd xmm4, xmm4
    x6 = mulsd(x6, x4);                                            // 427dc5 mulsd xmm6, xmm4
    x7 = subsd(x7, x6);                                            // 427dc9 subsd xmm7, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0xd0)));                 // 427dcd cvtss2sd xmm6, [esp + 0xd0]
    x4 = cvtpd2ps(x7);                                             // 427dd6 cvtpd2ps xmm4, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x140)));                // 427dda cvtss2sd xmm7, [esp + 0x140]
    st32(m, esp + 0xd8, x4.u[0]);                                  // 427de3 movss [esp + 0xd8], xmm4
    x4 = cvtps2pd(x2);                                             // 427dec cvtps2pd xmm4, xmm2
    x4 = mulsd(x4, x6);                                            // 427def mulsd xmm4, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0xdc)));                 // 427df3 cvtss2sd xmm6, [esp + 0xdc]
    x6 = mulsd(x6, x7);                                            // 427dfc mulsd xmm6, xmm7
    x4 = addsd(x4, x6);                                            // 427e00 addsd xmm4, xmm6
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0xd4)));                 // 427e04 cvtss2sd xmm7, [esp + 0xd4]
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x148)));                // 427e0d cvtss2sd xmm6, [esp + 0x148]
    x6 = mulsd(x6, x7);                                            // 427e16 mulsd xmm6, xmm7
    x4 = addsd(x4, x6);                                            // 427e1a addsd xmm4, xmm6
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x144)));                // 427e1e cvtss2sd xmm7, [esp + 0x144]
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0xd8)));                 // 427e27 cvtss2sd xmm6, [esp + 0xd8]
    x6 = mulsd(x6, x7);                                            // 427e30 mulsd xmm6, xmm7
    x4 = subsd(x4, x6);                                            // 427e34 subsd xmm4, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0xdc)));                 // 427e38 cvtss2sd xmm6, [esp + 0xdc]
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0xd4)));                 // 427e41 cvtss2sd xmm7, [esp + 0xd4]
    x4 = cvtpd2ps(x4);                                             // 427e4a cvtpd2ps xmm4, xmm4
    st32(m, esp + 0x110, x4.u[0]);                                 // 427e4e movss [esp + 0x110], xmm4
    x4 = cvtss2sd(x4, X::ss(ld32(m, esp + 0x144)));                // 427e57 cvtss2sd xmm4, [esp + 0x144]
    x4 = mulsd(x4, x6);                                            // 427e60 mulsd xmm4, xmm6
    x6 = cvtps2pd(x2);                                             // 427e64 cvtps2pd xmm6, xmm2
    x6 = mulsd(x6, x7);                                            // 427e67 mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x140)));                // 427e6b cvtss2sd xmm7, [esp + 0x140]
    x4 = addsd(x4, x6);                                            // 427e74 addsd xmm4, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0xd8)));                 // 427e78 cvtss2sd xmm6, [esp + 0xd8]
    x6 = mulsd(x6, x7);                                            // 427e81 mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0xd0)));                 // 427e85 cvtss2sd xmm7, [esp + 0xd0]
    x4 = addsd(x4, x6);                                            // 427e8e addsd xmm4, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x148)));                // 427e92 cvtss2sd xmm6, [esp + 0x148]
    x6 = mulsd(x6, x7);                                            // 427e9b mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0xdc)));                 // 427e9f cvtss2sd xmm7, [esp + 0xdc]
    x4 = subsd(x4, x6);                                            // 427ea8 subsd xmm4, xmm6
    x4 = cvtpd2ps(x4);                                             // 427eac cvtpd2ps xmm4, xmm4
    st32(m, esp + 0x114, x4.u[0]);                                 // 427eb0 movss [esp + 0x114], xmm4
    x4 = cvtss2sd(x4, X::ss(ld32(m, esp + 0xd8)));                 // 427eb9 cvtss2sd xmm4, [esp + 0xd8]
    x6 = cvtps2pd(x2);                                             // 427ec2 cvtps2pd xmm6, xmm2
    x4 = mulsd(x4, x6);                                            // 427ec5 mulsd xmm4, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x148)));                // 427ec9 cvtss2sd xmm6, [esp + 0x148]
    x6 = mulsd(x6, x7);                                            // 427ed2 mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0xd0)));                 // 427ed6 cvtss2sd xmm7, [esp + 0xd0]
    x4 = addsd(x4, x6);                                            // 427edf addsd xmm4, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x144)));                // 427ee3 cvtss2sd xmm6, [esp + 0x144]
    x6 = mulsd(x6, x7);                                            // 427eec mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x140)));                // 427ef0 cvtss2sd xmm7, [esp + 0x140]
    x4 = addsd(x4, x6);                                            // 427ef9 addsd xmm4, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0xd4)));                 // 427efd cvtss2sd xmm6, [esp + 0xd4]
    x6 = mulsd(x6, x7);                                            // 427f06 mulsd xmm6, xmm7
    x4 = subsd(x4, x6);                                            // 427f0a subsd xmm4, xmm6
    x6 = X::ss(ld32(m, 0x9cb4c4));                                 // 427f0e movss xmm6, [0x9cb4c4]
    x4 = cvtpd2ps(x4);                                             // 427f16 cvtpd2ps xmm4, xmm4
    st32(m, esp + 0x118, x4.u[0]);                                 // 427f1a movss [esp + 0x118], xmm4
    x4 = X::ss(ld32(m, 0x9cb4c0));                                 // 427f23 movss xmm4, [0x9cb4c0]
    x7 = cvtps2pd(x4);                                             // 427f2b cvtps2pd xmm7, xmm4
    x4 = cvtss2sd(x4, X::ss(ld32(m, esp + 0x80)));                 // 427f2e cvtss2sd xmm4, [esp + 0x80]
    x7 = mulsd(x7, x4);                                            // 427f37 mulsd xmm7, xmm4
    x4 = sd(ld64(m, esp + 0x218));                                 // 427f3b movsd xmm4, [esp + 0x218]
    x4 = subsd(x4, x7);                                            // 427f44 subsd xmm4, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x84)));                 // 427f48 cvtss2sd xmm7, [esp + 0x84]
    x6 = cvtps2pd(x6);                                             // 427f51 cvtps2pd xmm6, xmm6
    x6 = mulsd(x6, x7);                                            // 427f54 mulsd xmm6, xmm7
    x4 = subsd(x4, x6);                                            // 427f58 subsd xmm4, xmm6
    x6 = X::ss(ld32(m, 0x9cb4c8));                                 // 427f5c movss xmm6, [0x9cb4c8]
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x88)));                 // 427f64 cvtss2sd xmm7, [esp + 0x88]
    x6 = cvtps2pd(x6);                                             // 427f6d cvtps2pd xmm6, xmm6
    x6 = mulsd(x6, x7);                                            // 427f70 mulsd xmm6, xmm7
    x4 = subsd(x4, x6);                                            // 427f74 subsd xmm4, xmm6
    x4 = cvtpd2ps(x4);                                             // 427f78 cvtpd2ps xmm4, xmm4
    st32(m, esp + 0xfc, x4.u[0]);                                  // 427f7c movss [esp + 0xfc], xmm4
    x4 = X::ss(ld32(m, 0x9cb4c0));                                 // 427f85 movss xmm4, [0x9cb4c0]
    x6 = cvtps2pd(x4);                                             // 427f8d cvtps2pd xmm6, xmm4
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x84)));                 // 427f90 cvtss2sd xmm7, [esp + 0x84]
    x4 = cvtss2sd(x4, x1);                                         // 427f99 cvtss2sd xmm4, xmm1
    x6 = mulsd(x6, x4);                                            // 427f9d mulsd xmm6, xmm4
    x6 = addsd(x6, sd(ld64(m, esp + 0x1f8)));                      // 427fa1 addsd xmm6, [esp + 0x1f8]
    x4 = X::ss(ld32(m, 0x9cb4c8));                                 // 427faa movss xmm4, [0x9cb4c8]
    x4 = cvtps2pd(x4);                                             // 427fb2 cvtps2pd xmm4, xmm4
    x4 = mulsd(x4, x7);                                            // 427fb5 mulsd xmm4, xmm7
    x6 = addsd(x6, x4);                                            // 427fb9 addsd xmm6, xmm4
    x4 = X::ss(ld32(m, 0x9cb4c4));                                 // 427fbd movss xmm4, [0x9cb4c4]
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x88)));                 // 427fc5 cvtss2sd xmm7, [esp + 0x88]
    x4 = cvtps2pd(x4);                                             // 427fce cvtps2pd xmm4, xmm4
    x4 = mulsd(x4, x7);                                            // 427fd1 mulsd xmm4, xmm7
    x6 = subsd(x6, x4);                                            // 427fd5 subsd xmm6, xmm4
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x88)));                 // 427fd9 cvtss2sd xmm7, [esp + 0x88]
    x4 = cvtpd2ps(x6);                                             // 427fe2 cvtpd2ps xmm4, xmm6
    st32(m, esp + 0xf0, x4.u[0]);                                  // 427fe6 movss [esp + 0xf0], xmm4
    x4 = X::ss(ld32(m, 0x9cb4c4));                                 // 427fef movss xmm4, [0x9cb4c4]
    x6 = cvtps2pd(x4);                                             // 427ff7 cvtps2pd xmm6, xmm4
    x4 = cvtss2sd(x4, x1);                                         // 427ffa cvtss2sd xmm4, xmm1
    x6 = mulsd(x6, x4);                                            // 427ffe mulsd xmm6, xmm4
    x6 = addsd(x6, sd(ld64(m, esp + 0x1d0)));                      // 428002 addsd xmm6, [esp + 0x1d0]
    x4 = X::ss(ld32(m, 0x9cb4c0));                                 // 42800b movss xmm4, [0x9cb4c0]
    x4 = cvtps2pd(x4);                                             // 428013 cvtps2pd xmm4, xmm4
    x4 = mulsd(x4, x7);                                            // 428016 mulsd xmm4, xmm7
    x6 = addsd(x6, x4);                                            // 42801a addsd xmm6, xmm4
    x4 = X::ss(ld32(m, 0x9cb4c8));                                 // 42801e movss xmm4, [0x9cb4c8]
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x80)));                 // 428026 cvtss2sd xmm7, [esp + 0x80]
    x4 = cvtps2pd(x4);                                             // 42802f cvtps2pd xmm4, xmm4
    x4 = mulsd(x4, x7);                                            // 428032 mulsd xmm4, xmm7
    x6 = subsd(x6, x4);                                            // 428036 subsd xmm6, xmm4
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x80)));                 // 42803a cvtss2sd xmm7, [esp + 0x80]
    x4 = cvtpd2ps(x6);                                             // 428043 cvtpd2ps xmm4, xmm6
    st32(m, esp + 0xf4, x4.u[0]);                                  // 428047 movss [esp + 0xf4], xmm4
    x4 = X::ss(ld32(m, 0x9cb4c8));                                 // 428050 movss xmm4, [0x9cb4c8]
    x6 = cvtps2pd(x4);                                             // 428058 cvtps2pd xmm6, xmm4
    x4 = cvtss2sd(x4, x1);                                         // 42805b cvtss2sd xmm4, xmm1
    x6 = mulsd(x6, x4);                                            // 42805f mulsd xmm6, xmm4
    x6 = addsd(x6, sd(ld64(m, esp + 0x1d8)));                      // 428063 addsd xmm6, [esp + 0x1d8]
    x4 = X::ss(ld32(m, 0x9cb4c4));                                 // 42806c movss xmm4, [0x9cb4c4]
    x4 = cvtps2pd(x4);                                             // 428074 cvtps2pd xmm4, xmm4
    x4 = mulsd(x4, x7);                                            // 428077 mulsd xmm4, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x84)));                 // 42807b cvtss2sd xmm7, [esp + 0x84]
    x6 = addsd(x6, x4);                                            // 428084 addsd xmm6, xmm4
    x4 = X::ss(ld32(m, 0x9cb4c0));                                 // 428088 movss xmm4, [0x9cb4c0]
    x4 = cvtps2pd(x4);                                             // 428090 cvtps2pd xmm4, xmm4
    x4 = mulsd(x4, x7);                                            // 428093 mulsd xmm4, xmm7
    x6 = subsd(x6, x4);                                            // 428097 subsd xmm6, xmm4
    x4 = cvtpd2ps(x6);                                             // 42809b cvtpd2ps xmm4, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0xfc)));                 // 42809f cvtss2sd xmm6, [esp + 0xfc]
    st32(m, esp + 0xf8, x4.u[0]);                                  // 4280a8 movss [esp + 0xf8], xmm4
    x4 = X::ss(ld32(m, esp + 0x94));                               // 4280b1 movss xmm4, [esp + 0x94]
    x4 = cvtps2pd(x4);                                             // 4280ba cvtps2pd xmm4, xmm4
    x6 = mulsd(x6, x4);                                            // 4280bd mulsd xmm6, xmm4
    x4 = cvtss2sd(x4, X::ss(ld32(m, esp + 0xf0)));                 // 4280c1 cvtss2sd xmm4, [esp + 0xf0]
    x7 = cvtss2sd(x7, x1);                                         // 4280ca cvtss2sd xmm7, xmm1
    x4 = mulsd(x4, x7);                                            // 4280ce mulsd xmm4, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0xf4)));                 // 4280d2 cvtss2sd xmm7, [esp + 0xf4]
    x6 = addsd(x6, x4);                                            // 4280db addsd xmm6, xmm4
    x4 = X::ss(ld32(m, esp + 0xbc));                               // 4280df movss xmm4, [esp + 0xbc]
    x4 = cvtps2pd(x4);                                             // 4280e8 cvtps2pd xmm4, xmm4
    x7 = mulsd(x7, x4);                                            // 4280eb mulsd xmm7, xmm4
    x4 = X::ss(ld32(m, esp + 0x9c));                               // 4280ef movss xmm4, [esp + 0x9c]
    x6 = addsd(x6, x7);                                            // 4280f8 addsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0xf8)));                 // 4280fc cvtss2sd xmm7, [esp + 0xf8]
    x4 = cvtps2pd(x4);                                             // 428105 cvtps2pd xmm4, xmm4
    x7 = mulsd(x7, x4);                                            // 428108 mulsd xmm7, xmm4
    x6 = subsd(x6, x7);                                            // 42810c subsd xmm6, xmm7
    x4 = cvtpd2ps(x6);                                             // 428110 cvtpd2ps xmm4, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0xfc)));                 // 428114 cvtss2sd xmm6, [esp + 0xfc]
    st32(m, esp + 0x30, x4.u[0]);                                  // 42811d movss [esp + 0x30], xmm4
    x4 = X::ss(ld32(m, esp + 0x9c));                               // 428123 movss xmm4, [esp + 0x9c]
    x4 = cvtps2pd(x4);                                             // 42812c cvtps2pd xmm4, xmm4
    x6 = mulsd(x6, x4);                                            // 42812f mulsd xmm6, xmm4
    x4 = cvtss2sd(x4, X::ss(ld32(m, esp + 0xf4)));                 // 428133 cvtss2sd xmm4, [esp + 0xf4]
    x7 = cvtss2sd(x7, x1);                                         // 42813c cvtss2sd xmm7, xmm1
    x4 = mulsd(x4, x7);                                            // 428140 mulsd xmm4, xmm7
    x6 = addsd(x6, x4);                                            // 428144 addsd xmm6, xmm4
    x4 = X::ss(ld32(m, esp + 0x94));                               // 428148 movss xmm4, [esp + 0x94]
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0xf8)));                 // 428151 cvtss2sd xmm7, [esp + 0xf8]
    x4 = cvtps2pd(x4);                                             // 42815a cvtps2pd xmm4, xmm4
    x7 = mulsd(x7, x4);                                            // 42815d mulsd xmm7, xmm4
    x4 = X::ss(ld32(m, esp + 0xbc));                               // 428161 movss xmm4, [esp + 0xbc]
    x6 = addsd(x6, x7);                                            // 42816a addsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0xf0)));                 // 42816e cvtss2sd xmm7, [esp + 0xf0]
    x4 = cvtps2pd(x4);                                             // 428177 cvtps2pd xmm4, xmm4
    x7 = mulsd(x7, x4);                                            // 42817a mulsd xmm7, xmm4
    x6 = subsd(x6, x7);                                            // 42817e subsd xmm6, xmm7
    x4 = cvtpd2ps(x6);                                             // 428182 cvtpd2ps xmm4, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0xfc)));                 // 428186 cvtss2sd xmm6, [esp + 0xfc]
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x70)));                 // 42818f cvtss2sd xmm7, [esp + 0x70]
    st32(m, esp + 0x34, x4.u[0]);                                  // 428195 movss [esp + 0x34], xmm4
    x4 = X::ss(ld32(m, esp + 0xbc));                               // 42819b movss xmm4, [esp + 0xbc]
    x4 = cvtps2pd(x4);                                             // 4281a4 cvtps2pd xmm4, xmm4
    x6 = mulsd(x6, x4);                                            // 4281a7 mulsd xmm6, xmm4
    x4 = cvtss2sd(x4, X::ss(ld32(m, esp + 0xf8)));                 // 4281ab cvtss2sd xmm4, [esp + 0xf8]
    x1 = cvtss2sd(x1, x1);                                         // 4281b4 cvtss2sd xmm1, xmm1
    x4 = mulsd(x4, x1);                                            // 4281b8 mulsd xmm4, xmm1
    x1 = X::ss(ld32(m, esp + 0x9c));                               // 4281bc movss xmm1, [esp + 0x9c]
    x6 = addsd(x6, x4);                                            // 4281c5 addsd xmm6, xmm4
    x4 = cvtss2sd(x4, X::ss(ld32(m, esp + 0xf0)));                 // 4281c9 cvtss2sd xmm4, [esp + 0xf0]
    x1 = cvtps2pd(x1);                                             // 4281d2 cvtps2pd xmm1, xmm1
    x4 = mulsd(x4, x1);                                            // 4281d5 mulsd xmm4, xmm1
    x1 = X::ss(ld32(m, esp + 0x94));                               // 4281d9 movss xmm1, [esp + 0x94]
    x6 = addsd(x6, x4);                                            // 4281e2 addsd xmm6, xmm4
    x4 = cvtss2sd(x4, X::ss(ld32(m, esp + 0xf4)));                 // 4281e6 cvtss2sd xmm4, [esp + 0xf4]
    x1 = cvtps2pd(x1);                                             // 4281ef cvtps2pd xmm1, xmm1
    x4 = mulsd(x4, x1);                                            // 4281f2 mulsd xmm4, xmm1
    x6 = subsd(x6, x4);                                            // 4281f6 subsd xmm6, xmm4
    x4 = X::ss(ld32(m, 0x9cb4c4));                                 // 4281fa movss xmm4, [0x9cb4c4]
    x1 = cvtpd2ps(x6);                                             // 428202 cvtpd2ps xmm1, xmm6
    st32(m, esp + 0x38, x1.u[0]);                                  // 428206 movss [esp + 0x38], xmm1
    x1 = X::ss(ld32(m, 0x9cb4c0));                                 // 42820c movss xmm1, [0x9cb4c0]
    x6 = cvtps2pd(x1);                                             // 428214 cvtps2pd xmm6, xmm1
    x6 = mulsd(x6, x7);                                            // 428217 mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x74)));                 // 42821b cvtss2sd xmm7, [esp + 0x74]
    x5 = subsd(x5, x6);                                            // 428221 subsd xmm5, xmm6
    x6 = cvtps2pd(x4);                                             // 428225 cvtps2pd xmm6, xmm4
    x6 = mulsd(x6, x7);                                            // 428228 mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x78)));                 // 42822c cvtss2sd xmm7, [esp + 0x78]
    x5 = subsd(x5, x6);                                            // 428232 subsd xmm5, xmm6
    x6 = X::ss(ld32(m, 0x9cb4c8));                                 // 428236 movss xmm6, [0x9cb4c8]
    x6 = cvtps2pd(x6);                                             // 42823e cvtps2pd xmm6, xmm6
    x6 = mulsd(x6, x7);                                            // 428241 mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x74)));                 // 428245 cvtss2sd xmm7, [esp + 0x74]
    x5 = subsd(x5, x6);                                            // 42824b subsd xmm5, xmm6
    x5 = cvtpd2ps(x5);                                             // 42824f cvtpd2ps xmm5, xmm5
    st32(m, esp + 0x4c, x5.u[0]);                                  // 428253 movss [esp + 0x4c], xmm5
    x5 = cvtps2pd(x2);                                             // 428259 cvtps2pd xmm5, xmm2
    x6 = cvtps2pd(x1);                                             // 42825c cvtps2pd xmm6, xmm1
    x6 = mulsd(x6, x5);                                            // 42825f mulsd xmm6, xmm5
    x6 = addsd(x6, sd(ld64(m, esp + 0x1c8)));                      // 428263 addsd xmm6, [esp + 0x1c8]
    x5 = X::ss(ld32(m, 0x9cb4c8));                                 // 42826c movss xmm5, [0x9cb4c8]
    x5 = cvtps2pd(x5);                                             // 428274 cvtps2pd xmm5, xmm5
    x5 = mulsd(x5, x7);                                            // 428277 mulsd xmm5, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x78)));                 // 42827b cvtss2sd xmm7, [esp + 0x78]
    x6 = addsd(x6, x5);                                            // 428281 addsd xmm6, xmm5
    x5 = cvtps2pd(x4);                                             // 428285 cvtps2pd xmm5, xmm4
    x5 = mulsd(x5, x7);                                            // 428288 mulsd xmm5, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x78)));                 // 42828c cvtss2sd xmm7, [esp + 0x78]
    x6 = subsd(x6, x5);                                            // 428292 subsd xmm6, xmm5
    x5 = cvtpd2ps(x6);                                             // 428296 cvtpd2ps xmm5, xmm6
    st32(m, esp + 0x40, x5.u[0]);                                  // 42829a movss [esp + 0x40], xmm5
    x5 = cvtps2pd(x2);                                             // 4282a0 cvtps2pd xmm5, xmm2
    x6 = cvtps2pd(x4);                                             // 4282a3 cvtps2pd xmm6, xmm4
    x6 = mulsd(x6, x5);                                            // 4282a6 mulsd xmm6, xmm5
    x6 = addsd(x6, sd(ld64(m, esp + 0x1e0)));                      // 4282aa addsd xmm6, [esp + 0x1e0]
    x5 = cvtps2pd(x1);                                             // 4282b3 cvtps2pd xmm5, xmm1
    x5 = mulsd(x5, x7);                                            // 4282b6 mulsd xmm5, xmm7
    x6 = addsd(x6, x5);                                            // 4282ba addsd xmm6, xmm5
    x5 = X::ss(ld32(m, 0x9cb4c8));                                 // 4282be movss xmm5, [0x9cb4c8]
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x70)));                 // 4282c6 cvtss2sd xmm7, [esp + 0x70]
    x5 = cvtps2pd(x5);                                             // 4282cc cvtps2pd xmm5, xmm5
    x5 = mulsd(x5, x7);                                            // 4282cf mulsd xmm5, xmm7
    x6 = subsd(x6, x5);                                            // 4282d3 subsd xmm6, xmm5
    x5 = X::ss(ld32(m, 0x9cb4c8));                                 // 4282d7 movss xmm5, [0x9cb4c8]
    x5 = cvtps2pd(x5);                                             // 4282df cvtps2pd xmm5, xmm5
    x4 = cvtps2pd(x4);                                             // 4282e2 cvtps2pd xmm4, xmm4
    x7 = cvtps2pd(x2);                                             // 4282e5 cvtps2pd xmm7, xmm2
    x5 = mulsd(x5, x7);                                            // 4282e8 mulsd xmm5, xmm7
    x5 = addsd(x5, sd(ld64(m, esp + 0x1e8)));                      // 4282ec addsd xmm5, [esp + 0x1e8]
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x70)));                 // 4282f5 cvtss2sd xmm7, [esp + 0x70]
    x4 = mulsd(x4, x7);                                            // 4282fb mulsd xmm4, xmm7
    x5 = addsd(x5, x4);                                            // 4282ff addsd xmm5, xmm4
    x4 = cvtss2sd(x4, X::ss(ld32(m, esp + 0x74)));                 // 428303 cvtss2sd xmm4, [esp + 0x74]
    x6 = cvtpd2ps(x6);                                             // 428309 cvtpd2ps xmm6, xmm6
    st32(m, esp + 0x44, x6.u[0]);                                  // 42830d movss [esp + 0x44], xmm6
    x1 = cvtps2pd(x1);                                             // 428313 cvtps2pd xmm1, xmm1
    x1 = mulsd(x1, x4);                                            // 428316 mulsd xmm1, xmm4
    x5 = subsd(x5, x1);                                            // 42831a subsd xmm5, xmm1
    x1 = cvtpd2ps(x5);                                             // 42831e cvtpd2ps xmm1, xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0x4c)));                 // 428322 cvtss2sd xmm5, [esp + 0x4c]
    st32(m, esp + 0x48, x1.u[0]);                                  // 428328 movss [esp + 0x48], xmm1
    x1 = X::ss(ld32(m, esp + 0x198));                              // 42832e movss xmm1, [esp + 0x198]
    x6 = cvtss2sd(x6, x6);                                         // 428337 cvtss2sd xmm6, xmm6
    x7 = cvtps2pd(x2);                                             // 42833b cvtps2pd xmm7, xmm2
    x4 = cvtps2pd(x1);                                             // 42833e cvtps2pd xmm4, xmm1
    x5 = mulsd(x5, x4);                                            // 428341 mulsd xmm5, xmm4
    x4 = cvtss2sd(x4, X::ss(ld32(m, esp + 0x40)));                 // 428345 cvtss2sd xmm4, [esp + 0x40]
    x4 = mulsd(x4, x7);                                            // 42834b mulsd xmm4, xmm7
    x5 = addsd(x5, x4);                                            // 42834f addsd xmm5, xmm4
    x4 = X::ss(ld32(m, esp + 0x98));                               // 428353 movss xmm4, [esp + 0x98]
    x4 = cvtps2pd(x4);                                             // 42835c cvtps2pd xmm4, xmm4
    x6 = mulsd(x6, x4);                                            // 42835f mulsd xmm6, xmm4
    x4 = X::ss(ld32(m, esp + 0x190));                              // 428363 movss xmm4, [esp + 0x190]
    x5 = addsd(x5, x6);                                            // 42836c addsd xmm5, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x48)));                 // 428370 cvtss2sd xmm6, [esp + 0x48]
    x7 = cvtps2pd(x4);                                             // 428376 cvtps2pd xmm7, xmm4
    x6 = mulsd(x6, x7);                                            // 428379 mulsd xmm6, xmm7
    x5 = subsd(x5, x6);                                            // 42837d subsd xmm5, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x4c)));                 // 428381 cvtss2sd xmm6, [esp + 0x4c]
    x5 = cvtpd2ps(x5);                                             // 428387 cvtpd2ps xmm5, xmm5
    st32(m, esp + 0x160, x5.u[0]);                                 // 42838b movss [esp + 0x160], xmm5
    x7 = cvtps2pd(x2);                                             // 428394 cvtps2pd xmm7, xmm2
    x5 = cvtps2pd(x4);                                             // 428397 cvtps2pd xmm5, xmm4
    x6 = mulsd(x6, x5);                                            // 42839a mulsd xmm6, xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0x44)));                 // 42839e cvtss2sd xmm5, [esp + 0x44]
    x5 = mulsd(x5, x7);                                            // 4283a4 mulsd xmm5, xmm7
    x6 = addsd(x6, x5);                                            // 4283a8 addsd xmm6, xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0x48)));                 // 4283ac cvtss2sd xmm5, [esp + 0x48]
    x7 = cvtps2pd(x1);                                             // 4283b2 cvtps2pd xmm7, xmm1
    x5 = mulsd(x5, x7);                                            // 4283b5 mulsd xmm5, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x40)));                 // 4283b9 cvtss2sd xmm7, [esp + 0x40]
    x6 = addsd(x6, x5);                                            // 4283bf addsd xmm6, xmm5
    x5 = X::ss(ld32(m, esp + 0x98));                               // 4283c3 movss xmm5, [esp + 0x98]
    x5 = cvtps2pd(x5);                                             // 4283cc cvtps2pd xmm5, xmm5
    x7 = mulsd(x7, x5);                                            // 4283cf mulsd xmm7, xmm5
    x6 = subsd(x6, x7);                                            // 4283d3 subsd xmm6, xmm7
    x7 = X::ss(ld32(m, esp + 0x24));                               // 4283d7 movss xmm7, [esp + 0x24]
    x5 = cvtpd2ps(x6);                                             // 4283dd cvtpd2ps xmm5, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x4c)));                 // 4283e1 cvtss2sd xmm6, [esp + 0x4c]
    st32(m, esp + 0x164, x5.u[0]);                                 // 4283e7 movss [esp + 0x164], xmm5
    x5 = X::ss(ld32(m, esp + 0x98));                               // 4283f0 movss xmm5, [esp + 0x98]
    x5 = cvtps2pd(x5);                                             // 4283f9 cvtps2pd xmm5, xmm5
    x6 = mulsd(x6, x5);                                            // 4283fc mulsd xmm6, xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0x48)));                 // 428400 cvtss2sd xmm5, [esp + 0x48]
    x2 = cvtps2pd(x2);                                             // 428406 cvtps2pd xmm2, xmm2
    x5 = mulsd(x5, x2);                                            // 428409 mulsd xmm5, xmm2
    x2 = cvtss2sd(x2, X::ss(ld32(m, esp + 0x40)));                 // 42840d cvtss2sd xmm2, [esp + 0x40]
    x6 = addsd(x6, x5);                                            // 428413 addsd xmm6, xmm5
    x4 = cvtps2pd(x4);                                             // 428417 cvtps2pd xmm4, xmm4
    x2 = mulsd(x2, x4);                                            // 42841a mulsd xmm2, xmm4
    x6 = addsd(x6, x2);                                            // 42841e addsd xmm6, xmm2
    x2 = cvtss2sd(x2, X::ss(ld32(m, esp + 0x44)));                 // 428422 cvtss2sd xmm2, [esp + 0x44]
    x1 = cvtps2pd(x1);                                             // 428428 cvtps2pd xmm1, xmm1
    x2 = mulsd(x2, x1);                                            // 42842b mulsd xmm2, xmm1
    x6 = subsd(x6, x2);                                            // 42842f subsd xmm6, xmm2
    x1 = cvtpd2ps(x6);                                             // 428433 cvtpd2ps xmm1, xmm6
    x6 = X::ss(ld32(m, esp + 0x118));                              // 428437 movss xmm6, [esp + 0x118]
    st32(m, esp + 0x168, x1.u[0]);                                 // 428440 movss [esp + 0x168], xmm1
    x4 = x6;                                                       // 428449 movaps xmm4, xmm6
    x1 = X::ss(ld32(m, esp + 0x28));                               // 42844c movss xmm1, [esp + 0x28]
    x2 = X::ss(ld32(m, esp + 0x114));                              // 428452 movss xmm2, [esp + 0x114]
    x4 = mulss(x4, x7);                                            // 42845b mulss xmm4, xmm7
    x5 = x2;                                                       // 42845f movaps xmm5, xmm2
    x5 = mulss(x5, x1);                                            // 428462 mulss xmm5, xmm1
    x4 = subss(x4, x5);                                            // 428466 subss xmm4, xmm5
    x5 = X::ss(ld32(m, esp + 0x110));                              // 42846a movss xmm5, [esp + 0x110]
    x5 = mulss(x5, x1);                                            // 428473 mulss xmm5, xmm1
    x1 = x6;                                                       // 428477 movaps xmm1, xmm6
    x6 = X::ss(ld32(m, esp + 0x20));                               // 42847a movss xmm6, [esp + 0x20]
    x1 = mulss(x1, x6);                                            // 428480 mulss xmm1, xmm6
    x5 = subss(x5, x1);                                            // 428484 subss xmm5, xmm1
    x1 = x2;                                                       // 428488 movaps xmm1, xmm2
    x1 = mulss(x1, x6);                                            // 42848b mulss xmm1, xmm6
    x6 = X::ss(ld32(m, esp + 0x110));                              // 42848f movss xmm6, [esp + 0x110]
    x6 = mulss(x6, x7);                                            // 428498 mulss xmm6, xmm7
    x7 = X::ss(ld32(m, esp + 0x164));                              // 42849c movss xmm7, [esp + 0x164]
    x1 = subss(x1, x6);                                            // 4284a5 subss xmm1, xmm6
    x6 = X::ss(ld32(m, esp + 0x38));                               // 4284a9 movss xmm6, [esp + 0x38]
    x7 = mulss(x7, x6);                                            // 4284af mulss xmm7, xmm6
    st32(m, esp + 0x178, x1.u[0]);                                 // 4284b3 movss [esp + 0x178], xmm1
    x1 = X::ss(ld32(m, esp + 0x168));                              // 4284bc movss xmm1, [esp + 0x168]
    x1 = mulss(x1, X::ss(ld32(m, esp + 0x34)));                    // 4284c5 mulss xmm1, [esp + 0x34]
    x1 = subss(x1, x7);                                            // 4284cb subss xmm1, xmm7
    x7 = X::ss(ld32(m, esp + 0x168));                              // 4284cf movss xmm7, [esp + 0x168]
    st32(m, esp + 0x150, x1.u[0]);                                 // 4284d8 movss [esp + 0x150], xmm1
    x1 = X::ss(ld32(m, esp + 0x160));                              // 4284e1 movss xmm1, [esp + 0x160]
    x1 = mulss(x1, x6);                                            // 4284ea mulss xmm1, xmm6
    x6 = X::ss(ld32(m, esp + 0x30));                               // 4284ee movss xmm6, [esp + 0x30]
    x7 = mulss(x7, x6);                                            // 4284f4 mulss xmm7, xmm6
    x1 = subss(x1, x7);                                            // 4284f8 subss xmm1, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0xa0)));                 // 4284fc cvtss2sd xmm7, [esp + 0xa0]
    st32(m, esp + 0x174, x5.u[0]);                                 // 428505 movss [esp + 0x174], xmm5
    st32(m, esp + 0x154, x1.u[0]);                                 // 42850e movss [esp + 0x154], xmm1
    x1 = X::ss(ld32(m, esp + 0x164));                              // 428517 movss xmm1, [esp + 0x164]
    x1 = mulss(x1, x6);                                            // 428520 mulss xmm1, xmm6
    x6 = X::ss(ld32(m, esp + 0x160));                              // 428524 movss xmm6, [esp + 0x160]
    x6 = mulss(x6, X::ss(ld32(m, esp + 0x34)));                    // 42852d mulss xmm6, [esp + 0x34]
    x1 = subss(x1, x6);                                            // 428533 subss xmm1, xmm6
    x5 = cvtps2pd(x5);                                             // 428537 cvtps2pd xmm5, xmm5
    st32(m, esp + 0x158, x1.u[0]);                                 // 42853a movss [esp + 0x158], xmm1
    x1 = cvtps2pd(x0);                                             // 428543 cvtps2pd xmm1, xmm0
    x6 = cvtss2sd(x6, x4);                                         // 428546 cvtss2sd xmm6, xmm4
    x6 = mulsd(x6, x7);                                            // 42854a mulsd xmm6, xmm7
    x1 = mulsd(x1, x3);                                            // 42854e mulsd xmm1, xmm3
    x7 = x1;                                                       // 428552 movapd xmm7, xmm1
    x7 = subsd(x7, x6);                                            // 428556 subsd xmm7, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0xa4)));                 // 42855a cvtss2sd xmm6, [esp + 0xa4]
    x5 = mulsd(x5, x6);                                            // 428563 mulsd xmm5, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x178)));                // 428567 cvtss2sd xmm6, [esp + 0x178]
    x7 = subsd(x7, x5);                                            // 428570 subsd xmm7, xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0xa8)));                 // 428574 cvtss2sd xmm5, [esp + 0xa8]
    x5 = mulsd(x5, x6);                                            // 42857d mulsd xmm5, xmm6
    x7 = subsd(x7, x5);                                            // 428581 subsd xmm7, xmm5
    x5 = cvtpd2ps(x7);                                             // 428585 cvtpd2ps xmm5, xmm7
    st32(m, esp + 0xec, x5.u[0]);                                  // 428589 movss [esp + 0xec], xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0xa0)));                 // 428592 cvtss2sd xmm5, [esp + 0xa0]
    x5 = mulsd(x5, x3);                                            // 42859b mulsd xmm5, xmm3
    st64(m, esp + 0x210, x5.q(0));                                 // 42859f movsd [esp + 0x210], xmm5
    x7 = cvtps2pd(x0);                                             // 4285a8 cvtps2pd xmm7, xmm0
    x6 = cvtss2sd(x6, x4);                                         // 4285ab cvtss2sd xmm6, xmm4
    x6 = mulsd(x6, x7);                                            // 4285af mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x178)));                // 4285b3 cvtss2sd xmm7, [esp + 0x178]
    x6 = addsd(x6, x5);                                            // 4285bc addsd xmm6, xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0xa4)));                 // 4285c0 cvtss2sd xmm5, [esp + 0xa4]
    x5 = mulsd(x5, x7);                                            // 4285c9 mulsd xmm5, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0xa8)));                 // 4285cd cvtss2sd xmm7, [esp + 0xa8]
    x6 = addsd(x6, x5);                                            // 4285d6 addsd xmm6, xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0x174)));                // 4285da cvtss2sd xmm5, [esp + 0x174]
    x5 = mulsd(x5, x7);                                            // 4285e3 mulsd xmm5, xmm7
    x6 = subsd(x6, x5);                                            // 4285e7 subsd xmm6, xmm5
    x5 = cvtpd2ps(x6);                                             // 4285eb cvtpd2ps xmm5, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x174)));                // 4285ef cvtss2sd xmm6, [esp + 0x174]
    st32(m, esp + 0xe0, x5.u[0]);                                  // 4285f8 movss [esp + 0xe0], xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0xa4)));                 // 428601 cvtss2sd xmm5, [esp + 0xa4]
    x5 = mulsd(x5, x3);                                            // 42860a mulsd xmm5, xmm3
    x7 = cvtps2pd(x0);                                             // 42860e cvtps2pd xmm7, xmm0
    x6 = mulsd(x6, x7);                                            // 428611 mulsd xmm6, xmm7
    st64(m, esp + 0x1f0, x5.q(0));                                 // 428615 movsd [esp + 0x1f0], xmm5
    x6 = addsd(x6, x5);                                            // 42861e addsd xmm6, xmm5
    x5 = cvtss2sd(x5, x4);                                         // 428622 cvtss2sd xmm5, xmm4
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0xa8)));                 // 428626 cvtss2sd xmm7, [esp + 0xa8]
    x5 = mulsd(x5, x7);                                            // 42862f mulsd xmm5, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x178)));                // 428633 cvtss2sd xmm7, [esp + 0x178]
    x6 = addsd(x6, x5);                                            // 42863c addsd xmm6, xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0xa0)));                 // 428640 cvtss2sd xmm5, [esp + 0xa0]
    x4 = cvtss2sd(x4, x4);                                         // 428649 cvtss2sd xmm4, xmm4
    x5 = mulsd(x5, x7);                                            // 42864d mulsd xmm5, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0xa0)));                 // 428651 cvtss2sd xmm7, [esp + 0xa0]
    x6 = subsd(x6, x5);                                            // 42865a subsd xmm6, xmm5
    x5 = cvtpd2ps(x6);                                             // 42865e cvtpd2ps xmm5, xmm6
    st32(m, esp + 0xe4, x5.u[0]);                                  // 428662 movss [esp + 0xe4], xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0xa8)));                 // 42866b cvtss2sd xmm5, [esp + 0xa8]
    x5 = mulsd(x5, x3);                                            // 428674 mulsd xmm5, xmm3
    x3 = cvtss2sd(x3, X::ss(ld32(m, esp + 0x178)));                // 428678 cvtss2sd xmm3, [esp + 0x178]
    x6 = cvtps2pd(x0);                                             // 428681 cvtps2pd xmm6, xmm0
    x6 = mulsd(x6, x3);                                            // 428684 mulsd xmm6, xmm3
    x3 = cvtss2sd(x3, X::ss(ld32(m, esp + 0x174)));                // 428688 cvtss2sd xmm3, [esp + 0x174]
    x6 = addsd(x6, x5);                                            // 428691 addsd xmm6, xmm5
    x3 = mulsd(x3, x7);                                            // 428695 mulsd xmm3, xmm7
    x6 = addsd(x6, x3);                                            // 428699 addsd xmm6, xmm3
    x3 = X::ss(ld32(m, esp + 0xa4));                               // 42869d movss xmm3, [esp + 0xa4]
    x7 = cvtps2pd(x3);                                             // 4286a6 cvtps2pd xmm7, xmm3
    x4 = mulsd(x4, x7);                                            // 4286a9 mulsd xmm4, xmm7
    x6 = subsd(x6, x4);                                            // 4286ad subsd xmm6, xmm4
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0xec)));                 // 4286b1 cvtss2sd xmm7, [esp + 0xec]
    x4 = cvtpd2ps(x6);                                             // 4286ba cvtpd2ps xmm4, xmm6
    st32(m, esp + 0xe8, x4.u[0]);                                  // 4286be movss [esp + 0xe8], xmm4
    x4 = cvtss2sd(x4, X::ss(ld32(m, esp + 0xe0)));                 // 4286c7 cvtss2sd xmm4, [esp + 0xe0]
    x6 = cvtps2pd(x0);                                             // 4286d0 cvtps2pd xmm6, xmm0
    x6 = mulsd(x6, x4);                                            // 4286d3 mulsd xmm6, xmm4
    x4 = X::ss(ld32(m, esp + 0x10));                               // 4286d7 movss xmm4, [esp + 0x10]
    x4 = cvtps2pd(x4);                                             // 4286dd cvtps2pd xmm4, xmm4
    x7 = mulsd(x7, x4);                                            // 4286e0 mulsd xmm7, xmm4
    x4 = X::ss(ld32(m, esp + 0xb8));                               // 4286e4 movss xmm4, [esp + 0xb8]
    x4 = cvtps2pd(x4);                                             // 4286ed cvtps2pd xmm4, xmm4
    x6 = addsd(x6, x7);                                            // 4286f0 addsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0xe4)));                 // 4286f4 cvtss2sd xmm7, [esp + 0xe4]
    x4 = mulsd(x4, x7);                                            // 4286fd mulsd xmm4, xmm7
    x6 = addsd(x6, x4);                                            // 428701 addsd xmm6, xmm4
    x4 = X::ss(ld32(m, esp + 0x1c));                               // 428705 movss xmm4, [esp + 0x1c]
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0xe8)));                 // 42870b cvtss2sd xmm7, [esp + 0xe8]
    x4 = cvtps2pd(x4);                                             // 428714 cvtps2pd xmm4, xmm4
    x4 = mulsd(x4, x7);                                            // 428717 mulsd xmm4, xmm7
    x6 = subsd(x6, x4);                                            // 42871b subsd xmm6, xmm4
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0xe4)));                 // 42871f cvtss2sd xmm7, [esp + 0xe4]
    x4 = cvtpd2ps(x6);                                             // 428728 cvtpd2ps xmm4, xmm6
    st32(m, esp + 0xc0, x4.u[0]);                                  // 42872c movss [esp + 0xc0], xmm4
    x4 = X::ss(ld32(m, esp + 0x1c));                               // 428735 movss xmm4, [esp + 0x1c]
    x6 = cvtps2pd(x4);                                             // 42873b cvtps2pd xmm6, xmm4
    x4 = cvtss2sd(x4, X::ss(ld32(m, esp + 0xec)));                 // 42873e cvtss2sd xmm4, [esp + 0xec]
    x6 = mulsd(x6, x4);                                            // 428747 mulsd xmm6, xmm4
    x4 = cvtps2pd(x0);                                             // 42874b cvtps2pd xmm4, xmm0
    x4 = mulsd(x4, x7);                                            // 42874e mulsd xmm4, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0xe8)));                 // 428752 cvtss2sd xmm7, [esp + 0xe8]
    x6 = addsd(x6, x4);                                            // 42875b addsd xmm6, xmm4
    x4 = X::ss(ld32(m, esp + 0x10));                               // 42875f movss xmm4, [esp + 0x10]
    x4 = cvtps2pd(x4);                                             // 428765 cvtps2pd xmm4, xmm4
    x7 = mulsd(x7, x4);                                            // 428768 mulsd xmm7, xmm4
    x4 = X::ss(ld32(m, esp + 0xb8));                               // 42876c movss xmm4, [esp + 0xb8]
    x6 = addsd(x6, x7);                                            // 428775 addsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0xe0)));                 // 428779 cvtss2sd xmm7, [esp + 0xe0]
    x4 = cvtps2pd(x4);                                             // 428782 cvtps2pd xmm4, xmm4
    x4 = mulsd(x4, x7);                                            // 428785 mulsd xmm4, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0xe8)));                 // 428789 cvtss2sd xmm7, [esp + 0xe8]
    x6 = subsd(x6, x4);                                            // 428792 subsd xmm6, xmm4
    x4 = cvtpd2ps(x6);                                             // 428796 cvtpd2ps xmm4, xmm6
    st32(m, esp + 0xc4, x4.u[0]);                                  // 42879a movss [esp + 0xc4], xmm4
    x4 = X::ss(ld32(m, esp + 0xb8));                               // 4287a3 movss xmm4, [esp + 0xb8]
    x6 = cvtps2pd(x4);                                             // 4287ac cvtps2pd xmm6, xmm4
    x4 = cvtss2sd(x4, X::ss(ld32(m, esp + 0xec)));                 // 4287af cvtss2sd xmm4, [esp + 0xec]
    x6 = mulsd(x6, x4);                                            // 4287b8 mulsd xmm6, xmm4
    x4 = cvtps2pd(x0);                                             // 4287bc cvtps2pd xmm4, xmm0
    x4 = mulsd(x4, x7);                                            // 4287bf mulsd xmm4, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0xe0)));                 // 4287c3 cvtss2sd xmm7, [esp + 0xe0]
    x6 = addsd(x6, x4);                                            // 4287cc addsd xmm6, xmm4
    x4 = X::ss(ld32(m, esp + 0x1c));                               // 4287d0 movss xmm4, [esp + 0x1c]
    x4 = cvtps2pd(x4);                                             // 4287d6 cvtps2pd xmm4, xmm4
    x4 = mulsd(x4, x7);                                            // 4287d9 mulsd xmm4, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0xe4)));                 // 4287dd cvtss2sd xmm7, [esp + 0xe4]
    x6 = addsd(x6, x4);                                            // 4287e6 addsd xmm6, xmm4
    x4 = X::ss(ld32(m, esp + 0x10));                               // 4287ea movss xmm4, [esp + 0x10]
    x4 = cvtps2pd(x4);                                             // 4287f0 cvtps2pd xmm4, xmm4
    x7 = mulsd(x7, x4);                                            // 4287f3 mulsd xmm7, xmm4
    x6 = subsd(x6, x7);                                            // 4287f7 subsd xmm6, xmm7
    x4 = cvtpd2ps(x6);                                             // 4287fb cvtpd2ps xmm4, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0xa0)));                 // 4287ff cvtss2sd xmm6, [esp + 0xa0]
    st32(m, esp + 0xc8, x4.u[0]);                                  // 428808 movss [esp + 0xc8], xmm4
    x4 = X::ss(ld32(m, esp + 0x150));                              // 428811 movss xmm4, [esp + 0x150]
    x7 = cvtps2pd(x4);                                             // 42881a cvtps2pd xmm7, xmm4
    x6 = mulsd(x6, x7);                                            // 42881d mulsd xmm6, xmm7
    x1 = subsd(x1, x6);                                            // 428821 subsd xmm1, xmm6
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x154)));                // 428825 cvtss2sd xmm7, [esp + 0x154]
    x6 = cvtps2pd(x3);                                             // 42882e cvtps2pd xmm6, xmm3
    x6 = mulsd(x6, x7);                                            // 428831 mulsd xmm6, xmm7
    x1 = subsd(x1, x6);                                            // 428835 subsd xmm1, xmm6
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x158)));                // 428839 cvtss2sd xmm7, [esp + 0x158]
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0xa8)));                 // 428842 cvtss2sd xmm6, [esp + 0xa8]
    x6 = mulsd(x6, x7);                                            // 42884b mulsd xmm6, xmm7
    x1 = subsd(x1, x6);                                            // 42884f subsd xmm1, xmm6
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x158)));                // 428853 cvtss2sd xmm7, [esp + 0x158]
    x1 = cvtpd2ps(x1);                                             // 42885c cvtpd2ps xmm1, xmm1
    st32(m, esp + 0x6c, x1.u[0]);                                  // 428860 movss [esp + 0x6c], xmm1
    x6 = cvtps2pd(x4);                                             // 428866 cvtps2pd xmm6, xmm4
    x1 = cvtps2pd(x0);                                             // 428869 cvtps2pd xmm1, xmm0
    x1 = mulsd(x1, x6);                                            // 42886c mulsd xmm1, xmm6
    x1 = addsd(x1, sd(ld64(m, esp + 0x210)));                      // 428870 addsd xmm1, [esp + 0x210]
    x6 = cvtps2pd(x3);                                             // 428879 cvtps2pd xmm6, xmm3
    x6 = mulsd(x6, x7);                                            // 42887c mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x154)));                // 428880 cvtss2sd xmm7, [esp + 0x154]
    x1 = addsd(x1, x6);                                            // 428889 addsd xmm1, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0xa8)));                 // 42888d cvtss2sd xmm6, [esp + 0xa8]
    x6 = mulsd(x6, x7);                                            // 428896 mulsd xmm6, xmm7
    x1 = subsd(x1, x6);                                            // 42889a subsd xmm1, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x154)));                // 42889e cvtss2sd xmm6, [esp + 0x154]
    x1 = cvtpd2ps(x1);                                             // 4288a7 cvtpd2ps xmm1, xmm1
    st32(m, esp + 0x60, x1.u[0]);                                  // 4288ab movss [esp + 0x60], xmm1
    x7 = cvtps2pd(x4);                                             // 4288b1 cvtps2pd xmm7, xmm4
    x1 = cvtps2pd(x0);                                             // 4288b4 cvtps2pd xmm1, xmm0
    x1 = mulsd(x1, x6);                                            // 4288b7 mulsd xmm1, xmm6
    x1 = addsd(x1, sd(ld64(m, esp + 0x1f0)));                      // 4288bb addsd xmm1, [esp + 0x1f0]
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0xa8)));                 // 4288c4 cvtss2sd xmm6, [esp + 0xa8]
    x6 = mulsd(x6, x7);                                            // 4288cd mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x158)));                // 4288d1 cvtss2sd xmm7, [esp + 0x158]
    x1 = addsd(x1, x6);                                            // 4288da addsd xmm1, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0xa0)));                 // 4288de cvtss2sd xmm6, [esp + 0xa0]
    x6 = mulsd(x6, x7);                                            // 4288e7 mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x158)));                // 4288eb cvtss2sd xmm7, [esp + 0x158]
    x1 = subsd(x1, x6);                                            // 4288f4 subsd xmm1, xmm6
    x6 = cvtpd2ps(x1);                                             // 4288f8 cvtpd2ps xmm6, xmm1
    x3 = cvtps2pd(x3);                                             // 4288fc cvtps2pd xmm3, xmm3
    x1 = cvtps2pd(x0);                                             // 4288ff cvtps2pd xmm1, xmm0
    x1 = mulsd(x1, x7);                                            // 428902 mulsd xmm1, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x154)));                // 428906 cvtss2sd xmm7, [esp + 0x154]
    x1 = addsd(x1, x5);                                            // 42890f addsd xmm1, xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0xa0)));                 // 428913 cvtss2sd xmm5, [esp + 0xa0]
    x5 = mulsd(x5, x7);                                            // 42891c mulsd xmm5, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x6c)));                 // 428920 cvtss2sd xmm7, [esp + 0x6c]
    x1 = addsd(x1, x5);                                            // 428926 addsd xmm1, xmm5
    x4 = cvtps2pd(x4);                                             // 42892a cvtps2pd xmm4, xmm4
    x3 = mulsd(x3, x4);                                            // 42892d mulsd xmm3, xmm4
    x4 = cvtss2sd(x4, X::ss(ld32(m, esp + 0x60)));                 // 428931 cvtss2sd xmm4, [esp + 0x60]
    x1 = subsd(x1, x3);                                            // 428937 subsd xmm1, xmm3
    x5 = cvtpd2ps(x1);                                             // 42893b cvtpd2ps xmm5, xmm1
    x1 = X::ss(ld32(m, esp + 0x10));                               // 42893f movss xmm1, [esp + 0x10]
    x3 = cvtps2pd(x1);                                             // 428945 cvtps2pd xmm3, xmm1
    x7 = mulsd(x7, x3);                                            // 428948 mulsd xmm7, xmm3
    st32(m, esp + 0x64, x6.u[0]);                                  // 42894c movss [esp + 0x64], xmm6
    x3 = cvtps2pd(x0);                                             // 428952 cvtps2pd xmm3, xmm0
    x3 = mulsd(x3, x4);                                            // 428955 mulsd xmm3, xmm4
    x4 = X::ss(ld32(m, esp + 0xb8));                               // 428959 movss xmm4, [esp + 0xb8]
    x7 = addsd(x7, x3);                                            // 428962 addsd xmm7, xmm3
    x6 = cvtss2sd(x6, x6);                                         // 428966 cvtss2sd xmm6, xmm6
    x3 = cvtps2pd(x4);                                             // 42896a cvtps2pd xmm3, xmm4
    x3 = mulsd(x3, x6);                                            // 42896d mulsd xmm3, xmm6
    x7 = addsd(x7, x3);                                            // 428971 addsd xmm7, xmm3
    x3 = X::ss(ld32(m, esp + 0x1c));                               // 428975 movss xmm3, [esp + 0x1c]
    st32(m, esp + 0x68, x5.u[0]);                                  // 42897b movss [esp + 0x68], xmm5
    x5 = cvtss2sd(x5, x5);                                         // 428981 cvtss2sd xmm5, xmm5
    x6 = cvtps2pd(x3);                                             // 428985 cvtps2pd xmm6, xmm3
    x6 = mulsd(x6, x5);                                            // 428988 mulsd xmm6, xmm5
    x7 = subsd(x7, x6);                                            // 42898c subsd xmm7, xmm6
    x5 = cvtpd2ps(x7);                                             // 428990 cvtpd2ps xmm5, xmm7
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x6c)));                 // 428994 cvtss2sd xmm6, [esp + 0x6c]
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x64)));                 // 42899a cvtss2sd xmm7, [esp + 0x64]
    st32(m, esp + 0x4b0, x5.u[0]);                                 // 4289a0 movss [esp + 0x4b0], xmm5
    x2 = mulss(x2, X::ss(ld32(m, esp + 0x24)));                    // 4289a9 mulss xmm2, [esp + 0x24]
    x5 = cvtps2pd(x3);                                             // 4289af cvtps2pd xmm5, xmm3
    x5 = mulsd(x5, x6);                                            // 4289b2 mulsd xmm5, xmm6
    x6 = cvtps2pd(x0);                                             // 4289b6 cvtps2pd xmm6, xmm0
    x6 = mulsd(x6, x7);                                            // 4289b9 mulsd xmm6, xmm7
    x5 = addsd(x5, x6);                                            // 4289bd addsd xmm5, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x68)));                 // 4289c1 cvtss2sd xmm6, [esp + 0x68]
    x7 = cvtps2pd(x1);                                             // 4289c7 cvtps2pd xmm7, xmm1
    x6 = mulsd(x6, x7);                                            // 4289ca mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x60)));                 // 4289ce cvtss2sd xmm7, [esp + 0x60]
    x5 = addsd(x5, x6);                                            // 4289d4 addsd xmm5, xmm6
    x6 = cvtps2pd(x4);                                             // 4289d8 cvtps2pd xmm6, xmm4
    x0 = cvtps2pd(x0);                                             // 4289db cvtps2pd xmm0, xmm0
    x6 = mulsd(x6, x7);                                            // 4289de mulsd xmm6, xmm7
    x5 = subsd(x5, x6);                                            // 4289e2 subsd xmm5, xmm6
    x5 = cvtpd2ps(x5);                                             // 4289e6 cvtpd2ps xmm5, xmm5
    x4 = cvtps2pd(x4);                                             // 4289ea cvtps2pd xmm4, xmm4
    st32(m, esp + 0x4b4, x5.u[0]);                                 // 4289ed movss [esp + 0x4b4], xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0x6c)));                 // 4289f6 cvtss2sd xmm5, [esp + 0x6c]
    x4 = mulsd(x4, x5);                                            // 4289fc mulsd xmm4, xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0x68)));                 // 428a00 cvtss2sd xmm5, [esp + 0x68]
    x0 = mulsd(x0, x5);                                            // 428a06 mulsd xmm0, xmm5
    x4 = addsd(x4, x0);                                            // 428a0a addsd xmm4, xmm0
    x0 = cvtps2pd(x3);                                             // 428a0e cvtps2pd xmm0, xmm3
    x3 = cvtss2sd(x3, X::ss(ld32(m, esp + 0x60)));                 // 428a11 cvtss2sd xmm3, [esp + 0x60]
    x0 = mulsd(x0, x3);                                            // 428a17 mulsd xmm0, xmm3
    x4 = addsd(x4, x0);                                            // 428a1b addsd xmm4, xmm0
    x0 = cvtss2sd(x0, X::ss(ld32(m, esp + 0x64)));                 // 428a1f cvtss2sd xmm0, [esp + 0x64]
    x1 = cvtps2pd(x1);                                             // 428a25 cvtps2pd xmm1, xmm1
    x0 = mulsd(x0, x1);                                            // 428a28 mulsd xmm0, xmm1
    x4 = subsd(x4, x0);                                            // 428a2c subsd xmm4, xmm0
    x0 = cvtpd2ps(x4);                                             // 428a30 cvtpd2ps xmm0, xmm4
    st32(m, esp + 0x4b8, x0.u[0]);                                 // 428a34 movss [esp + 0x4b8], xmm0
    x0 = X::ss(ld32(m, esp + 0x110));                              // 428a3d movss xmm0, [esp + 0x110]
    x0 = mulss(x0, X::ss(ld32(m, esp + 0x20)));                    // 428a46 mulss xmm0, [esp + 0x20]
    x2 = addss(x2, x0);                                            // 428a4c addss xmm2, xmm0
    x0 = X::ss(ld32(m, esp + 0x118));                              // 428a50 movss xmm0, [esp + 0x118]
    x0 = mulss(x0, X::ss(ld32(m, esp + 0x28)));                    // 428a59 mulss xmm0, [esp + 0x28]
    x2 = addss(x2, x0);                                            // 428a5f addss xmm2, xmm0
    st32(m, esp + 0x194, x2.u[0]);                                 // 428a63 movss [esp + 0x194], xmm2
    x0 = x2;                                                       // 428a6c movaps xmm0, xmm2
    k.call(0x00401040);  // FUN_00401040                           // 428a6f call 0x401040
    fl = f_comiss(x0.f(0), flt(ld32(m, 0x9ddfc8)));                // 428a74 comiss xmm0, [0x9ddfc8]
    x1 = X::ss(ld32(m, esp + 0x194));                              // 428a7b movss xmm1, [esp + 0x194]
    st32(m, esp + 0x19c, x0.u[0]);                                 // 428a84 movss [esp + 0x19c], xmm0
    st32(m, esp + 0x1a4, x1.u[0]);                                 // 428a8d movss [esp + 0x1a4], xmm1
    if (fl.b()) goto L_00428ae5;                                   // 428a96 jb 0x428ae5
    x0 = cvtps2pd(x1);                                             // 428a98 cvtps2pd xmm0, xmm1
    x1 = x0;                                                       // 428a9b movapd xmm1, xmm0
    x1 = mulsd(x1, x0);                                            // 428a9f mulsd xmm1, xmm0
    x0 = cvtpd2ps(x1);                                             // 428aa3 cvtpd2ps xmm0, xmm1
    x1 = sd(0x3ff0000000000000ull);                                // 428aa7 movsd xmm1, [0x977638]
    x0 = cvtss2sd(x0, x0);                                         // 428aaf cvtss2sd xmm0, xmm0
    x1 = subsd(x1, x0);                                            // 428ab3 subsd xmm1, xmm0
    x0 = cvtpd2ps(x1);                                             // 428ab7 cvtpd2ps xmm0, xmm1
    k.call(0x00401040);  // FUN_00401040                           // 428abb call 0x401040
    k.push(ecx);                                                   // 428ac0 push ecx
    st32(m, esp, x0.u[0]);                                         // 428ac1 movss [esp], xmm0
    k.call(0x0042c230);  // FUN_0042c230                           // 428ac6 call 0x42c230
    x3 = x0;                                                       // 428acb movaps xmm3, xmm0
    x0 = X::ss(ld32(m, esp + 0x1a0));                              // 428ace movss xmm0, [esp + 0x1a0]
    esp = esp + 0x4u;                                              // 428ad7 add esp, 4
    goto L_00428ae8;                                               // 428ada jmp 0x428ae8
L_00428adc:
    eax = ld32(m, esp + 0xc);                                      // 428adc mov eax, [esp + 0xc]
    goto L_00427665;                                               // 428ae0 jmp 0x427665
L_00428ae5:
    x3 = x0;                                                       // 428ae5 movaps xmm3, xmm0
L_00428ae8:
    fl = f_comiss(x0.f(0), flt(ld32(m, 0x9ddfc8)));                // 428ae8 comiss xmm0, [0x9ddfc8]
    x5 = sd(0x3fe212eb00000000ull);                                // 428aef movsd xmm5, [0x9777b0]
    x1 = cvtss2sd(x1, x3);                                         // 428af7 cvtss2sd xmm1, xmm3
    x2 = x1;                                                       // 428afb movapd xmm2, xmm1
    x2 = mulsd(x2, x1);                                            // 428aff mulsd xmm2, xmm1
    x1 = cvtsd2ss(x1, x2);                                         // 428b03 cvtsd2ss xmm1, xmm2
    x2 = cvtss2sd(x2, x1);                                         // 428b07 cvtss2sd xmm2, xmm1
    x4 = x2;                                                       // 428b0b movapd xmm4, xmm2
    x4 = mulsd(x4, x2);                                            // 428b0f mulsd xmm4, xmm2
    x2 = cvtsd2ss(x2, x4);                                         // 428b13 cvtsd2ss xmm2, xmm4
    x4 = cvtss2sd(x4, x1);                                         // 428b17 cvtss2sd xmm4, xmm1
    x4 = mulsd(x4, sd(0x3fa9903ac0000000ull));                     // 428b1b mulsd xmm4, [0x9777a8]
    x5 = subsd(x5, x4);                                            // 428b23 subsd xmm5, xmm4
    x4 = cvtpd2ps(x5);                                             // 428b27 cvtpd2ps xmm4, xmm5
    x6 = cvtss2sd(x6, x1);                                         // 428b2b cvtss2sd xmm6, xmm1
    x6 = mulsd(x6, sd(0x3ff716ac60000000ull));                     // 428b2f mulsd xmm6, [0x9777a0]
    x5 = cvtss2sd(x5, x4);                                         // 428b37 cvtss2sd xmm5, xmm4
    x4 = cvtss2sd(x4, x2);                                         // 428b3b cvtss2sd xmm4, xmm2
    x5 = mulsd(x5, x4);                                            // 428b3f mulsd xmm5, xmm4
    x4 = sd(0x3ff0000000000000ull);                                // 428b43 movsd xmm4, [0x977638]
    x7 = x4;                                                       // 428b4b movapd xmm7, xmm4
    x7 = subsd(x7, x6);                                            // 428b4f subsd xmm7, xmm6
    x6 = cvtsd2ss(x6, x7);                                         // 428b53 cvtsd2ss xmm6, xmm7
    x7 = sd(0x3fd98e2fe0000000ull);                                // 428b57 movsd xmm7, [0x977798]
    x6 = cvtss2sd(x6, x6);                                         // 428b5f cvtss2sd xmm6, xmm6
    x5 = addsd(x5, x6);                                            // 428b63 addsd xmm5, xmm6
    x6 = x4;                                                       // 428b67 movapd xmm6, xmm4
    x6 = divsd(x6, x5);                                            // 428b6b divsd xmm6, xmm5
    x5 = cvtsd2ss(x5, x6);                                         // 428b6f cvtsd2ss xmm5, xmm6
    x6 = cvtss2sd(x6, x1);                                         // 428b73 cvtss2sd xmm6, xmm1
    x6 = mulsd(x6, sd(0x3f93d7f000000000ull));                     // 428b77 mulsd xmm6, [0x977790]
    x7 = subsd(x7, x6);                                            // 428b7f subsd xmm7, xmm6
    x1 = cvtss2sd(x1, x1);                                         // 428b83 cvtss2sd xmm1, xmm1
    x1 = mulsd(x1, sd(0x3ff46c01a0000000ull));                     // 428b87 mulsd xmm1, [0x977788]
    x2 = cvtss2sd(x2, x2);                                         // 428b8f cvtss2sd xmm2, xmm2
    x6 = cvtpd2ps(x7);                                             // 428b93 cvtpd2ps xmm6, xmm7
    x6 = cvtss2sd(x6, x6);                                         // 428b97 cvtss2sd xmm6, xmm6
    x6 = mulsd(x6, x2);                                            // 428b9b mulsd xmm6, xmm2
    x2 = x4;                                                       // 428b9f movapd xmm2, xmm4
    x2 = subsd(x2, x1);                                            // 428ba3 subsd xmm2, xmm1
    x1 = cvtsd2ss(x1, x2);                                         // 428ba7 cvtsd2ss xmm1, xmm2
    x1 = cvtss2sd(x1, x1);                                         // 428bab cvtss2sd xmm1, xmm1
    x6 = addsd(x6, x1);                                            // 428baf addsd xmm6, xmm1
    x1 = cvtss2sd(x1, x3);                                         // 428bb3 cvtss2sd xmm1, xmm3
    x6 = mulsd(x6, x1);                                            // 428bb7 mulsd xmm6, xmm1
    x1 = cvtsd2ss(x1, x6);                                         // 428bbb cvtsd2ss xmm1, xmm6
    x1 = cvtss2sd(x1, x1);                                         // 428bbf cvtss2sd xmm1, xmm1
    x5 = cvtss2sd(x5, x5);                                         // 428bc3 cvtss2sd xmm5, xmm5
    x5 = mulsd(x5, x1);                                            // 428bc7 mulsd xmm5, xmm1
    x1 = cvtpd2ps(x5);                                             // 428bcb cvtpd2ps xmm1, xmm5
    if (fl.b()) goto L_00428be5;                                   // 428bcf jb 0x428be5
    x0 = cvtss2sd(x0, x1);                                         // 428bd1 cvtss2sd xmm0, xmm1
    x1 = sd(0x3ff921fb60000000ull);                                // 428bd5 movsd xmm1, [0x977510]
    x1 = subsd(x1, x0);                                            // 428bdd subsd xmm1, xmm0
    x1 = cvtpd2ps(x1);                                             // 428be1 cvtpd2ps xmm1, xmm1
L_00428be5:
    x0 = X::ss(ld32(m, esp + 0x1a4));                              // 428be5 movss xmm0, [esp + 0x1a4]
    fl = f_comiss(x0.f(0), flt(0x00000000u));                      // 428bee comiss xmm0, [0x92e12c]
    if (fl.b()) goto L_00428bfc;                                   // 428bf5 jb 0x428bfc
    x0 = x1;                                                       // 428bf7 movaps xmm0, xmm1
    goto L_00428c08;                                               // 428bfa jmp 0x428c08
L_00428bfc:
    x0 = X::ss(0x80000000u);                                       // 428bfc movss xmm0, [0x9774a8]
    x0 = subss(x0, x1);                                            // 428c04 subss xmm0, xmm1
L_00428c08:
    x1 = sd(0x3ff921fb60000000ull);                                // 428c08 movsd xmm1, [0x977510]
    x0 = cvtps2pd(x0);                                             // 428c10 cvtps2pd xmm0, xmm0
    x1 = subsd(x1, x0);                                            // 428c13 subsd xmm1, xmm0
    x0 = cvtpd2ps(x1);                                             // 428c17 cvtpd2ps xmm0, xmm1
    x1 = X::ss(ld32(m, esp + 0x160));                              // 428c1b movss xmm1, [esp + 0x160]
    x1 = mulss(x1, X::ss(ld32(m, esp + 0x30)));                    // 428c24 mulss xmm1, [esp + 0x30]
    st32(m, esp + 0x1a8, x0.u[0]);                                 // 428c2a movss [esp + 0x1a8], xmm0
    x0 = X::ss(ld32(m, esp + 0x164));                              // 428c33 movss xmm0, [esp + 0x164]
    x0 = mulss(x0, X::ss(ld32(m, esp + 0x34)));                    // 428c3c mulss xmm0, [esp + 0x34]
    x0 = addss(x0, x1);                                            // 428c42 addss xmm0, xmm1
    x1 = X::ss(ld32(m, esp + 0x168));                              // 428c46 movss xmm1, [esp + 0x168]
    x1 = mulss(x1, X::ss(ld32(m, esp + 0x38)));                    // 428c4f mulss xmm1, [esp + 0x38]
    x0 = addss(x0, x1);                                            // 428c55 addss xmm0, xmm1
    st32(m, esp + 0x1ac, x0.u[0]);                                 // 428c59 movss [esp + 0x1ac], xmm0
    k.call(0x00401040);  // FUN_00401040                           // 428c62 call 0x401040
    x3 = x0;                                                       // 428c67 movaps xmm3, xmm0
    fl = f_comiss(x3.f(0), flt(ld32(m, 0x9ddfc8)));                // 428c6a comiss xmm3, [0x9ddfc8]
    x0 = X::ss(ld32(m, esp + 0x1ac));                              // 428c71 movss xmm0, [esp + 0x1ac]
    st32(m, esp + 0x1b4, x3.u[0]);                                 // 428c7a movss [esp + 0x1b4], xmm3
    st32(m, esp + 0x1b0, x0.u[0]);                                 // 428c83 movss [esp + 0x1b0], xmm0
    if (fl.b()) goto L_00428ccf;                                   // 428c8c jb 0x428ccf
    x0 = cvtps2pd(x0);                                             // 428c8e cvtps2pd xmm0, xmm0
    x1 = x0;                                                       // 428c91 movapd xmm1, xmm0
    x1 = mulsd(x1, x0);                                            // 428c95 mulsd xmm1, xmm0
    x0 = cvtsd2ss(x0, x1);                                         // 428c99 cvtsd2ss xmm0, xmm1
    x1 = sd(0x3ff0000000000000ull);                                // 428c9d movsd xmm1, [0x977638]
    x0 = cvtss2sd(x0, x0);                                         // 428ca5 cvtss2sd xmm0, xmm0
    x1 = subsd(x1, x0);                                            // 428ca9 subsd xmm1, xmm0
    x0 = cvtpd2ps(x1);                                             // 428cad cvtpd2ps xmm0, xmm1
    k.call(0x00401040);  // FUN_00401040                           // 428cb1 call 0x401040
    k.push(ecx);                                                   // 428cb6 push ecx
    st32(m, esp, x0.u[0]);                                         // 428cb7 movss [esp], xmm0
    k.call(0x0042c230);  // FUN_0042c230                           // 428cbc call 0x42c230
    x3 = X::ss(ld32(m, esp + 0x1b8));                              // 428cc1 movss xmm3, [esp + 0x1b8]
    esp = esp + 0x4u;                                              // 428cca add esp, 4
    goto L_00428cd2;                                               // 428ccd jmp 0x428cd2
L_00428ccf:
    x0 = x3;                                                       // 428ccf movaps xmm0, xmm3
L_00428cd2:
    fl = f_comiss(x3.f(0), flt(ld32(m, 0x9ddfc8)));                // 428cd2 comiss xmm3, [0x9ddfc8]
    x5 = sd(0x3fe212eb00000000ull);                                // 428cd9 movsd xmm5, [0x9777b0]
    x1 = cvtss2sd(x1, x0);                                         // 428ce1 cvtss2sd xmm1, xmm0
    x2 = x1;                                                       // 428ce5 movapd xmm2, xmm1
    x2 = mulsd(x2, x1);                                            // 428ce9 mulsd xmm2, xmm1
    x1 = cvtsd2ss(x1, x2);                                         // 428ced cvtsd2ss xmm1, xmm2
    x2 = cvtss2sd(x2, x1);                                         // 428cf1 cvtss2sd xmm2, xmm1
    x4 = x2;                                                       // 428cf5 movapd xmm4, xmm2
    x4 = mulsd(x4, x2);                                            // 428cf9 mulsd xmm4, xmm2
    x2 = cvtsd2ss(x2, x4);                                         // 428cfd cvtsd2ss xmm2, xmm4
    x4 = cvtss2sd(x4, x1);                                         // 428d01 cvtss2sd xmm4, xmm1
    x4 = mulsd(x4, sd(0x3fa9903ac0000000ull));                     // 428d05 mulsd xmm4, [0x9777a8]
    x5 = subsd(x5, x4);                                            // 428d0d subsd xmm5, xmm4
    x4 = cvtpd2ps(x5);                                             // 428d11 cvtpd2ps xmm4, xmm5
    x5 = cvtss2sd(x5, x4);                                         // 428d15 cvtss2sd xmm5, xmm4
    x4 = cvtss2sd(x4, x2);                                         // 428d19 cvtss2sd xmm4, xmm2
    x5 = mulsd(x5, x4);                                            // 428d1d mulsd xmm5, xmm4
    x4 = sd(0x3ff0000000000000ull);                                // 428d21 movsd xmm4, [0x977638]
    x6 = cvtss2sd(x6, x1);                                         // 428d29 cvtss2sd xmm6, xmm1
    x6 = mulsd(x6, sd(0x3ff716ac60000000ull));                     // 428d2d mulsd xmm6, [0x9777a0]
    x7 = x4;                                                       // 428d35 movapd xmm7, xmm4
    x7 = subsd(x7, x6);                                            // 428d39 subsd xmm7, xmm6
    x6 = cvtsd2ss(x6, x7);                                         // 428d3d cvtsd2ss xmm6, xmm7
    x7 = sd(0x3fd98e2fe0000000ull);                                // 428d41 movsd xmm7, [0x977798]
    x6 = cvtss2sd(x6, x6);                                         // 428d49 cvtss2sd xmm6, xmm6
    x5 = addsd(x5, x6);                                            // 428d4d addsd xmm5, xmm6
    x6 = x4;                                                       // 428d51 movapd xmm6, xmm4
    x6 = divsd(x6, x5);                                            // 428d55 divsd xmm6, xmm5
    x5 = cvtsd2ss(x5, x6);                                         // 428d59 cvtsd2ss xmm5, xmm6
    x6 = cvtss2sd(x6, x1);                                         // 428d5d cvtss2sd xmm6, xmm1
    x6 = mulsd(x6, sd(0x3f93d7f000000000ull));                     // 428d61 mulsd xmm6, [0x977790]
    x7 = subsd(x7, x6);                                            // 428d69 subsd xmm7, xmm6
    x2 = cvtss2sd(x2, x2);                                         // 428d6d cvtss2sd xmm2, xmm2
    x6 = cvtpd2ps(x7);                                             // 428d71 cvtpd2ps xmm6, xmm7
    x6 = cvtss2sd(x6, x6);                                         // 428d75 cvtss2sd xmm6, xmm6
    x6 = mulsd(x6, x2);                                            // 428d79 mulsd xmm6, xmm2
    x1 = cvtss2sd(x1, x1);                                         // 428d7d cvtss2sd xmm1, xmm1
    x1 = mulsd(x1, sd(0x3ff46c01a0000000ull));                     // 428d81 mulsd xmm1, [0x977788]
    x2 = x4;                                                       // 428d89 movapd xmm2, xmm4
    x2 = subsd(x2, x1);                                            // 428d8d subsd xmm2, xmm1
    x0 = cvtss2sd(x0, x0);                                         // 428d91 cvtss2sd xmm0, xmm0
    x1 = cvtsd2ss(x1, x2);                                         // 428d95 cvtsd2ss xmm1, xmm2
    x1 = cvtss2sd(x1, x1);                                         // 428d99 cvtss2sd xmm1, xmm1
    x6 = addsd(x6, x1);                                            // 428d9d addsd xmm6, xmm1
    x6 = mulsd(x6, x0);                                            // 428da1 mulsd xmm6, xmm0
    x0 = cvtsd2ss(x0, x6);                                         // 428da5 cvtsd2ss xmm0, xmm6
    x0 = cvtss2sd(x0, x0);                                         // 428da9 cvtss2sd xmm0, xmm0
    x5 = cvtss2sd(x5, x5);                                         // 428dad cvtss2sd xmm5, xmm5
    x5 = mulsd(x5, x0);                                            // 428db1 mulsd xmm5, xmm0
    x0 = cvtpd2ps(x5);                                             // 428db5 cvtpd2ps xmm0, xmm5
    if (fl.b()) goto L_00428dcf;                                   // 428db9 jb 0x428dcf
    x1 = sd(0x3ff921fb60000000ull);                                // 428dbb movsd xmm1, [0x977510]
    x0 = cvtss2sd(x0, x0);                                         // 428dc3 cvtss2sd xmm0, xmm0
    x1 = subsd(x1, x0);                                            // 428dc7 subsd xmm1, xmm0
    x0 = cvtpd2ps(x1);                                             // 428dcb cvtpd2ps xmm0, xmm1
L_00428dcf:
    x1 = X::ss(ld32(m, esp + 0x1b0));                              // 428dcf movss xmm1, [esp + 0x1b0]
    fl = f_comiss(x1.f(0), flt(0x00000000u));                      // 428dd8 comiss xmm1, [0x92e12c]
    if (fl.b()) goto L_00428de6;                                   // 428ddf jb 0x428de6
    x1 = x0;                                                       // 428de1 movaps xmm1, xmm0
    goto L_00428df2;                                               // 428de4 jmp 0x428df2
L_00428de6:
    x1 = X::ss(0x80000000u);                                       // 428de6 movss xmm1, [0x9774a8]
    x1 = subss(x1, x0);                                            // 428dee subss xmm1, xmm0
L_00428df2:
    x0 = cvtps2pd(x1);                                             // 428df2 cvtps2pd xmm0, xmm1
    x1 = sd(0x3ff921fb60000000ull);                                // 428df5 movsd xmm1, [0x977510]
    x1 = subsd(x1, x0);                                            // 428dfd subsd xmm1, xmm0
    x0 = cvtpd2ps(x1);                                             // 428e01 cvtpd2ps xmm0, xmm1
    st32(m, esp + 0x18c, x0.u[0]);                                 // 428e05 movss [esp + 0x18c], xmm0
    x0 = X::ss(ld32(m, esp + 0x1a8));                              // 428e0e movss xmm0, [esp + 0x1a8]
    k.call(0x00401000);  // FUN_00401000                           // 428e17 call 0x401000
    x1 = cvtss2sd(x1, x0);                                         // 428e1c cvtss2sd xmm1, xmm0
    x0 = X{};                                                      // 428e20 xorps xmm0, xmm0
    x1 = subsd(x1, x0);                                            // 428e23 subsd xmm1, xmm0
    x1 = divsd(x1, sd(ld64(m, esp + 0x1b8)));                      // 428e27 divsd xmm1, [esp + 0x1b8]
    x1 = cvtsd2ss(x1, x1);                                         // 428e30 cvtsd2ss xmm1, xmm1
    x1 = cvtss2sd(x1, x1);                                         // 428e34 cvtss2sd xmm1, xmm1
    x1 = mulsd(x1, sd(ld64(m, esp + 0x1c0)));                      // 428e38 mulsd xmm1, [esp + 0x1c0]
    x1 = addsd(x1, x0);                                            // 428e41 addsd xmm1, xmm0
    x0 = cvtsd2ss(x0, x1);                                         // 428e45 cvtsd2ss xmm0, xmm1
    x1 = X{};                                                      // 428e49 xorps xmm1, xmm1
    fl = f_comiss(x0.f(0), x1.f(0));                               // 428e4c comiss xmm0, xmm1
    if (fl.be()) goto L_00428e5e;                                  // 428e4f jbe 0x428e5e
    x1 = X::ss(0x40c00000u);                                       // 428e51 movss xmm1, [0x92fe4c]
    fl = f_comiss(x1.f(0), x0.f(0));                               // 428e59 comiss xmm1, xmm0
    if (fl.a()) goto L_00428e61;                                   // 428e5c ja 0x428e61
L_00428e5e:
    x0 = x1;                                                       // 428e5e movaps xmm0, xmm1
L_00428e61:
    st32(m, esp + 0x5c, x0.u[0]);                                  // 428e61 movss [esp + 0x5c], xmm0
    st32(m, esp + 0x1a0, x0.u[0]);                                 // 428e67 movss [esp + 0x1a0], xmm0
    x0 = X::ss(ld32(m, esp + 0x18c));                              // 428e70 movss xmm0, [esp + 0x18c]
    k.call(0x00401000);  // FUN_00401000                           // 428e79 call 0x401000
    x1 = cvtss2sd(x1, x0);                                         // 428e7e cvtss2sd xmm1, xmm0
    x0 = X{};                                                      // 428e82 xorps xmm0, xmm0
    x1 = subsd(x1, x0);                                            // 428e85 subsd xmm1, xmm0
    x1 = divsd(x1, sd(ld64(m, esp + 0x1b8)));                      // 428e89 divsd xmm1, [esp + 0x1b8]
    x1 = cvtsd2ss(x1, x1);                                         // 428e92 cvtsd2ss xmm1, xmm1
    x1 = cvtss2sd(x1, x1);                                         // 428e96 cvtss2sd xmm1, xmm1
    x1 = mulsd(x1, sd(ld64(m, esp + 0x1c0)));                      // 428e9a mulsd xmm1, [esp + 0x1c0]
    x1 = addsd(x1, x0);                                            // 428ea3 addsd xmm1, xmm0
    x0 = cvtsd2ss(x0, x1);                                         // 428ea7 cvtsd2ss xmm0, xmm1
    x1 = X{};                                                      // 428eab xorps xmm1, xmm1
    fl = f_comiss(x0.f(0), x1.f(0));                               // 428eae comiss xmm0, xmm1
    if (fl.be()) goto L_00428ec3;                                  // 428eb1 jbe 0x428ec3
    x7 = X::ss(0x40c00000u);                                       // 428eb3 movss xmm7, [0x92fe4c]
    fl = f_comiss(x7.f(0), x0.f(0));                               // 428ebb comiss xmm7, xmm0
    x1 = x0;                                                       // 428ebe movaps xmm1, xmm0
    if (fl.be()) goto L_00428ec6;                                  // 428ec1 jbe 0x428ec6
L_00428ec3:
    x7 = x1;                                                       // 428ec3 movaps xmm7, xmm1
L_00428ec6:
    fl = f_cmp(ld32(m, ebx + 0xa834), 0x0u);                       // 428ec6 cmp [ebx + 0xa834], 0
    if (fl.ne()) goto L_00428ed8;                                  // 428ecd jne 0x428ed8
    fl = f_cmp8(ld8(m, ebx + 0x88), 0x0u);                         // 428ecf cmp byte ptr [ebx + 0x88], 0
    if (fl.e()) goto L_00428efc;                                   // 428ed6 je 0x428efc
L_00428ed8:
    x0 = sd(0x3fe8000000000000ull);                                // 428ed8 movsd xmm0, [0x977b78]
    x1 = cvtss2sd(x1, X::ss(ld32(m, esp + 0x5c)));                 // 428ee0 cvtss2sd xmm1, [esp + 0x5c]
    x1 = mulsd(x1, x0);                                            // 428ee6 mulsd xmm1, xmm0
    x2 = cvtpd2ps(x1);                                             // 428eea cvtpd2ps xmm2, xmm1
    x1 = cvtss2sd(x1, x7);                                         // 428eee cvtss2sd xmm1, xmm7
    x1 = mulsd(x1, x0);                                            // 428ef2 mulsd xmm1, xmm0
    x7 = cvtpd2ps(x1);                                             // 428ef6 cvtpd2ps xmm7, xmm1
    goto L_00428f05;                                               // 428efa jmp 0x428f05
L_00428efc:
    x2 = X::ss(ld32(m, esp + 0x1a0));                              // 428efc movss xmm2, [esp + 0x1a0]
L_00428f05:
    x4 = X::ss(ld32(m, esp + 0x4b0));                              // 428f05 movss xmm4, [esp + 0x4b0]
    x5 = X::ss(ld32(m, esp + 0x4b4));                              // 428f0e movss xmm5, [esp + 0x4b4]
    x6 = X::ss(ld32(m, esp + 0x4b8));                              // 428f17 movss xmm6, [esp + 0x4b8]
    x0 = x2;                                                       // 428f20 movaps xmm0, xmm2
    x0 = mulss(x0, X::ss(ld32(m, esp + 0xc0)));                    // 428f23 mulss xmm0, [esp + 0xc0]
    x1 = x2;                                                       // 428f2c movaps xmm1, xmm2
    x2 = mulss(x2, X::ss(ld32(m, esp + 0xc8)));                    // 428f2f mulss xmm2, [esp + 0xc8]
    x1 = mulss(x1, X::ss(ld32(m, esp + 0xc4)));                    // 428f38 mulss xmm1, [esp + 0xc4]
    x3 = x2;                                                       // 428f41 movaps xmm3, xmm2
    x2 = X::ss(ld32(m, ebp + 0xc));                                // 428f44 movss xmm2, [ebp + 0xc]
    x4 = mulss(x4, x7);                                            // 428f49 mulss xmm4, xmm7
    x5 = mulss(x5, x7);                                            // 428f4d mulss xmm5, xmm7
    x6 = mulss(x6, x7);                                            // 428f51 mulss xmm6, xmm7
    x3 = mulss(x3, x2);                                            // 428f55 mulss xmm3, xmm2
    x7 = x3;                                                       // 428f59 movaps xmm7, xmm3
    x3 = X::ss(0x41f00000u);                                       // 428f5c movss xmm3, [0x931d98]
    x0 = mulss(x0, x2);                                            // 428f64 mulss xmm0, xmm2
    x1 = mulss(x1, x2);                                            // 428f68 mulss xmm1, xmm2
    x4 = mulss(x4, x2);                                            // 428f6c mulss xmm4, xmm2
    x5 = mulss(x5, x2);                                            // 428f70 mulss xmm5, xmm2
    x6 = mulss(x6, x2);                                            // 428f74 mulss xmm6, xmm2
    x0 = mulss(x0, x3);                                            // 428f78 mulss xmm0, xmm3
    x1 = mulss(x1, x3);                                            // 428f7c mulss xmm1, xmm3
    x7 = mulss(x7, x3);                                            // 428f80 mulss xmm7, xmm3
    x4 = mulss(x4, x3);                                            // 428f84 mulss xmm4, xmm3
    x5 = mulss(x5, x3);                                            // 428f88 mulss xmm5, xmm3
    x6 = mulss(x6, x3);                                            // 428f8c mulss xmm6, xmm3
    k.push(edi);                                                   // 428f90 push edi
    ecx = esp + 0x224;                                             // 428f91 lea ecx, [esp + 0x224]
    edx = ebx + 0x380;                                             // 428f98 lea edx, [ebx + 0x380]
    st32(m, esp + 0x224, x0.u[0]);                                 // 428f9e movss [esp + 0x224], xmm0
    st32(m, esp + 0x228, x1.u[0]);                                 // 428fa7 movss [esp + 0x228], xmm1
    st32(m, esp + 0x22c, x7.u[0]);                                 // 428fb0 movss [esp + 0x22c], xmm7
    st32(m, esp + 0x4c4, x4.u[0]);                                 // 428fb9 movss [esp + 0x4c4], xmm4
    st32(m, esp + 0x4c8, x5.u[0]);                                 // 428fc2 movss [esp + 0x4c8], xmm5
    st32(m, esp + 0x4cc, x6.u[0]);                                 // 428fcb movss [esp + 0x4cc], xmm6
    k.call(0x004241f0);  // FUN_004241f0                           // 428fd4 call 0x4241f0
    k.push(edi);                                                   // 428fd9 push edi
    ecx = esp + 0x4c4;                                             // 428fda lea ecx, [esp + 0x4c4]
    k.call(0x004241f0);  // FUN_004241f0                           // 428fe1 call 0x4241f0
    x0 = ld(m, esi + 0x8);                                         // 428fe6 movaps xmm0, [esi + 8]
    x0 = mulps(x0, ld(m, esp + 0x200));                            // 428fea mulps xmm0, [esp + 0x200]
    st32(m, esp + 0x58, ld32(m, esp + 0x58) + 0x4u);               // 428ff2 add [esp + 0x58], 4
    st32(m, esp + 0x14, ld32(m, esp + 0x14) + 0x30u);              // 428ff7 add [esp + 0x14], 0x30
    x3 = X{};                                                      // 428ffc xorps xmm3, xmm3
    x4 = X::ss(0x80000000u);                                       // 428fff movss xmm4, [0x9774a8]
    st(m, esi + 0x8, x0);                                          // 429007 movaps [esi + 8], xmm0
    edx = ld32(m, ebx + 0x3fc);                                    // 42900b mov edx, [ebx + 0x3fc]
    edi = edi + 1u;                                                // 429011 inc edi
    esi = esi + 0x70u;                                             // 429012 add esi, 0x70
    fl = f_cmp(edi, edx);                                          // 429015 cmp edi, edx
    if (fl.l()) goto L_004275f0;                                   // 429017 jl 0x4275f0
L_0042901d:
    fl = f_cmp(ld32(m, ebx + 0x374), 0x0u);                        // 42901d cmp [ebx + 0x374], 0
    if (fl.ne()) goto L_00429033;                                  // 429024 jne 0x429033
    fl = f_cmp8(ld8(m, ebx + 0x88), 0x0u);                         // 429026 cmp byte ptr [ebx + 0x88], 0
    if (fl.e()) goto L_004296b5;                                   // 42902d je 0x4296b5
L_00429033:
    fl = f_cmp(ld32(m, ebx + 0x98), 0x0u);                         // 429033 cmp [ebx + 0x98], 0
    if (fl.ne()) goto L_004296b5;                                  // 42903a jne 0x4296b5
    x2 = X::ss(ld32(m, ebx + 0xe84));                              // 429040 movss xmm2, [ebx + 0xe84]
    x7 = X::ss(ld32(m, ebx + 0xe88));                              // 429048 movss xmm7, [ebx + 0xe88]
    x0 = X::ss(ld32(m, ebx + 0xe80));                              // 429050 movss xmm0, [ebx + 0xe80]
    x6 = X::ss(ld32(m, ebx + 0xe90));                              // 429058 movss xmm6, [ebx + 0xe90]
    x6 = cvtps2pd(x6);                                             // 429060 cvtps2pd xmm6, xmm6
    x5 = x4;                                                       // 429063 movaps xmm5, xmm4
    x5 = subss(x5, x2);                                            // 429066 subss xmm5, xmm2
    st32(m, esp + 0x24, x5.u[0]);                                  // 42906a movss [esp + 0x24], xmm5
    x5 = x4;                                                       // 429070 movaps xmm5, xmm4
    x5 = subss(x5, x7);                                            // 429073 subss xmm5, xmm7
    st32(m, esp + 0x28, x5.u[0]);                                  // 429077 movss [esp + 0x28], xmm5
    x5 = X::ss(ld32(m, ebx + 0xe8c));                              // 42907d movss xmm5, [ebx + 0xe8c]
    x7 = cvtps2pd(x5);                                             // 429085 cvtps2pd xmm7, xmm5
    st32(m, esp + 0x2c, x5.u[0]);                                  // 429088 movss [esp + 0x2c], xmm5
    x7 = mulsd(x7, x3);                                            // 42908e mulsd xmm7, xmm3
    x5 = cvtps2pd(x0);                                             // 429092 cvtps2pd xmm5, xmm0
    x5 = mulsd(x5, x6);                                            // 429095 mulsd xmm5, xmm6
    x7 = subsd(x7, x5);                                            // 429099 subsd xmm7, xmm5
    x5 = X::ss(ld32(m, ebx + 0xe94));                              // 42909d movss xmm5, [ebx + 0xe94]
    x5 = cvtps2pd(x5);                                             // 4290a5 cvtps2pd xmm5, xmm5
    x6 = cvtps2pd(x2);                                             // 4290a8 cvtps2pd xmm6, xmm2
    x6 = mulsd(x6, x5);                                            // 4290ab mulsd xmm6, xmm5
    x5 = X::ss(ld32(m, ebx + 0xe98));                              // 4290af movss xmm5, [ebx + 0xe98]
    x7 = subsd(x7, x6);                                            // 4290b7 subsd xmm7, xmm6
    x6 = cvtps2pd(x5);                                             // 4290bb cvtps2pd xmm6, xmm5
    x5 = X::ss(ld32(m, ebx + 0xe88));                              // 4290be movss xmm5, [ebx + 0xe88]
    x5 = cvtps2pd(x5);                                             // 4290c6 cvtps2pd xmm5, xmm5
    x6 = mulsd(x6, x5);                                            // 4290c9 mulsd xmm6, xmm5
    x7 = subsd(x7, x6);                                            // 4290cd subsd xmm7, xmm6
    x5 = cvtpd2ps(x7);                                             // 4290d1 cvtpd2ps xmm5, xmm7
    st32(m, esp + 0x6c, x5.u[0]);                                  // 4290d5 movss [esp + 0x6c], xmm5
    x5 = X::ss(ld32(m, ebx + 0xe8c));                              // 4290db movss xmm5, [ebx + 0xe8c]
    x6 = cvtps2pd(x5);                                             // 4290e3 cvtps2pd xmm6, xmm5
    x5 = X::ss(ld32(m, ebx + 0xe90));                              // 4290e6 movss xmm5, [ebx + 0xe90]
    x5 = cvtps2pd(x5);                                             // 4290ee cvtps2pd xmm5, xmm5
    x6 = mulsd(x6, x5);                                            // 4290f1 mulsd xmm6, xmm5
    x5 = cvtps2pd(x0);                                             // 4290f5 cvtps2pd xmm5, xmm0
    x5 = mulsd(x5, x3);                                            // 4290f8 mulsd xmm5, xmm3
    x6 = addsd(x6, x5);                                            // 4290fc addsd xmm6, xmm5
    x5 = X::ss(ld32(m, ebx + 0xe98));                              // 429100 movss xmm5, [ebx + 0xe98]
    x5 = cvtps2pd(x5);                                             // 429108 cvtps2pd xmm5, xmm5
    x7 = cvtps2pd(x2);                                             // 42910b cvtps2pd xmm7, xmm2
    x5 = mulsd(x5, x7);                                            // 42910e mulsd xmm5, xmm7
    x6 = addsd(x6, x5);                                            // 429112 addsd xmm6, xmm5
    x5 = X::ss(ld32(m, ebx + 0xe88));                              // 429116 movss xmm5, [ebx + 0xe88]
    x7 = cvtps2pd(x5);                                             // 42911e cvtps2pd xmm7, xmm5
    x5 = X::ss(ld32(m, ebx + 0xe94));                              // 429121 movss xmm5, [ebx + 0xe94]
    x5 = cvtps2pd(x5);                                             // 429129 cvtps2pd xmm5, xmm5
    x7 = mulsd(x7, x5);                                            // 42912c mulsd xmm7, xmm5
    x6 = subsd(x6, x7);                                            // 429130 subsd xmm6, xmm7
    x5 = cvtpd2ps(x6);                                             // 429134 cvtpd2ps xmm5, xmm6
    st32(m, esp + 0x60, x5.u[0]);                                  // 429138 movss [esp + 0x60], xmm5
    x5 = X::ss(ld32(m, ebx + 0xe8c));                              // 42913e movss xmm5, [ebx + 0xe8c]
    x6 = cvtps2pd(x5);                                             // 429146 cvtps2pd xmm6, xmm5
    x5 = X::ss(ld32(m, ebx + 0xe94));                              // 429149 movss xmm5, [ebx + 0xe94]
    x5 = cvtps2pd(x5);                                             // 429151 cvtps2pd xmm5, xmm5
    x6 = mulsd(x6, x5);                                            // 429154 mulsd xmm6, xmm5
    x5 = cvtps2pd(x2);                                             // 429158 cvtps2pd xmm5, xmm2
    x5 = mulsd(x5, x3);                                            // 42915b mulsd xmm5, xmm3
    x6 = addsd(x6, x5);                                            // 42915f addsd xmm6, xmm5
    x5 = X::ss(ld32(m, ebx + 0xe88));                              // 429163 movss xmm5, [ebx + 0xe88]
    x7 = cvtps2pd(x5);                                             // 42916b cvtps2pd xmm7, xmm5
    x5 = X::ss(ld32(m, ebx + 0xe90));                              // 42916e movss xmm5, [ebx + 0xe90]
    x5 = cvtps2pd(x5);                                             // 429176 cvtps2pd xmm5, xmm5
    x7 = mulsd(x7, x5);                                            // 429179 mulsd xmm7, xmm5
    x5 = X::ss(ld32(m, ebx + 0xe98));                              // 42917d movss xmm5, [ebx + 0xe98]
    x6 = addsd(x6, x7);                                            // 429185 addsd xmm6, xmm7
    x5 = cvtps2pd(x5);                                             // 429189 cvtps2pd xmm5, xmm5
    x7 = cvtps2pd(x0);                                             // 42918c cvtps2pd xmm7, xmm0
    x5 = mulsd(x5, x7);                                            // 42918f mulsd xmm5, xmm7
    x6 = subsd(x6, x5);                                            // 429193 subsd xmm6, xmm5
    x5 = X::ss(ld32(m, ebx + 0xe98));                              // 429197 movss xmm5, [ebx + 0xe98]
    x7 = cvtps2pd(x5);                                             // 42919f cvtps2pd xmm7, xmm5
    x5 = X::ss(ld32(m, ebx + 0xe8c));                              // 4291a2 movss xmm5, [ebx + 0xe8c]
    x5 = cvtps2pd(x5);                                             // 4291aa cvtps2pd xmm5, xmm5
    x1 = x4;                                                       // 4291ad movaps xmm1, xmm4
    x6 = cvtpd2ps(x6);                                             // 4291b0 cvtpd2ps xmm6, xmm6
    x7 = mulsd(x7, x5);                                            // 4291b4 mulsd xmm7, xmm5
    x5 = X::ss(ld32(m, ebx + 0xe88));                              // 4291b8 movss xmm5, [ebx + 0xe88]
    x1 = subss(x1, x0);                                            // 4291c0 subss xmm1, xmm0
    st32(m, esp + 0x64, x6.u[0]);                                  // 4291c4 movss [esp + 0x64], xmm6
    fl = f_cmp(ld32(m, ebx + 0xa834), 0x0u);                       // 4291ca cmp [ebx + 0xa834], 0
    x5 = cvtps2pd(x5);                                             // 4291d1 cvtps2pd xmm5, xmm5
    x5 = mulsd(x5, x3);                                            // 4291d4 mulsd xmm5, xmm3
    x7 = addsd(x7, x5);                                            // 4291d8 addsd xmm7, xmm5
    x5 = cvtps2pd(x0);                                             // 4291dc cvtps2pd xmm5, xmm0
    x0 = X::ss(ld32(m, ebx + 0xe94));                              // 4291df movss xmm0, [ebx + 0xe94]
    x0 = cvtps2pd(x0);                                             // 4291e7 cvtps2pd xmm0, xmm0
    x5 = mulsd(x5, x0);                                            // 4291ea mulsd xmm5, xmm0
    x0 = X::ss(ld32(m, ebx + 0xe90));                              // 4291ee movss xmm0, [ebx + 0xe90]
    x7 = addsd(x7, x5);                                            // 4291f6 addsd xmm7, xmm5
    x0 = cvtps2pd(x0);                                             // 4291fa cvtps2pd xmm0, xmm0
    x2 = cvtps2pd(x2);                                             // 4291fd cvtps2pd xmm2, xmm2
    x2 = mulsd(x2, x0);                                            // 429200 mulsd xmm2, xmm0
    x0 = cvtss2sd(x0, X::ss(ld32(m, esp + 0x60)));                 // 429204 cvtss2sd xmm0, [esp + 0x60]
    x7 = subsd(x7, x2);                                            // 42920a subsd xmm7, xmm2
    x5 = cvtpd2ps(x7);                                             // 42920e cvtpd2ps xmm5, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x2c)));                 // 429212 cvtss2sd xmm7, [esp + 0x2c]
    x7 = mulsd(x7, x0);                                            // 429218 mulsd xmm7, xmm0
    x0 = cvtss2sd(x0, X::ss(ld32(m, esp + 0x6c)));                 // 42921c cvtss2sd xmm0, [esp + 0x6c]
    st32(m, esp + 0x68, x5.u[0]);                                  // 429222 movss [esp + 0x68], xmm5
    x6 = cvtss2sd(x6, x6);                                         // 429228 cvtss2sd xmm6, xmm6
    x5 = cvtss2sd(x5, x5);                                         // 42922c cvtss2sd xmm5, xmm5
    x2 = cvtss2sd(x2, x1);                                         // 429230 cvtss2sd xmm2, xmm1
    x0 = mulsd(x0, x2);                                            // 429234 mulsd xmm0, xmm2
    x2 = X::ss(ld32(m, esp + 0x28));                               // 429238 movss xmm2, [esp + 0x28]
    x7 = addsd(x7, x0);                                            // 42923e addsd xmm7, xmm0
    x0 = cvtps2pd(x2);                                             // 429242 cvtps2pd xmm0, xmm2
    x0 = mulsd(x0, x6);                                            // 429245 mulsd xmm0, xmm6
    x7 = addsd(x7, x0);                                            // 429249 addsd xmm7, xmm0
    x0 = X::ss(ld32(m, esp + 0x24));                               // 42924d movss xmm0, [esp + 0x24]
    x6 = cvtps2pd(x0);                                             // 429253 cvtps2pd xmm6, xmm0
    x6 = mulsd(x6, x5);                                            // 429256 mulsd xmm6, xmm5
    x7 = subsd(x7, x6);                                            // 42925a subsd xmm7, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x64)));                 // 42925e cvtss2sd xmm6, [esp + 0x64]
    x5 = cvtpd2ps(x7);                                             // 429264 cvtpd2ps xmm5, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x6c)));                 // 429268 cvtss2sd xmm7, [esp + 0x6c]
    st32(m, esp + 0x30, x5.u[0]);                                  // 42926e movss [esp + 0x30], xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0x2c)));                 // 429274 cvtss2sd xmm5, [esp + 0x2c]
    x5 = mulsd(x5, x6);                                            // 42927a mulsd xmm5, xmm6
    x6 = cvtps2pd(x0);                                             // 42927e cvtps2pd xmm6, xmm0
    x6 = mulsd(x6, x7);                                            // 429281 mulsd xmm6, xmm7
    x5 = addsd(x5, x6);                                            // 429285 addsd xmm5, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x68)));                 // 429289 cvtss2sd xmm6, [esp + 0x68]
    x7 = cvtss2sd(x7, x1);                                         // 42928f cvtss2sd xmm7, xmm1
    x6 = mulsd(x6, x7);                                            // 429293 mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x60)));                 // 429297 cvtss2sd xmm7, [esp + 0x60]
    x5 = addsd(x5, x6);                                            // 42929d addsd xmm5, xmm6
    x6 = cvtps2pd(x2);                                             // 4292a1 cvtps2pd xmm6, xmm2
    x6 = mulsd(x6, x7);                                            // 4292a4 mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x68)));                 // 4292a8 cvtss2sd xmm7, [esp + 0x68]
    x5 = subsd(x5, x6);                                            // 4292ae subsd xmm5, xmm6
    x6 = cvtpd2ps(x5);                                             // 4292b2 cvtpd2ps xmm6, xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0x2c)));                 // 4292b6 cvtss2sd xmm5, [esp + 0x2c]
    x5 = mulsd(x5, x7);                                            // 4292bc mulsd xmm5, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x6c)));                 // 4292c0 cvtss2sd xmm7, [esp + 0x6c]
    x2 = cvtps2pd(x2);                                             // 4292c6 cvtps2pd xmm2, xmm2
    x2 = mulsd(x2, x7);                                            // 4292c9 mulsd xmm2, xmm7
    x5 = addsd(x5, x2);                                            // 4292cd addsd xmm5, xmm2
    x2 = cvtss2sd(x2, X::ss(ld32(m, esp + 0x60)));                 // 4292d1 cvtss2sd xmm2, [esp + 0x60]
    x0 = cvtps2pd(x0);                                             // 4292d7 cvtps2pd xmm0, xmm0
    x0 = mulsd(x0, x2);                                            // 4292da mulsd xmm0, xmm2
    x5 = addsd(x5, x0);                                            // 4292de addsd xmm5, xmm0
    x0 = cvtss2sd(x0, X::ss(ld32(m, esp + 0x64)));                 // 4292e2 cvtss2sd xmm0, [esp + 0x64]
    x1 = cvtss2sd(x1, x1);                                         // 4292e8 cvtss2sd xmm1, xmm1
    x0 = mulsd(x0, x1);                                            // 4292ec mulsd xmm0, xmm1
    x5 = subsd(x5, x0);                                            // 4292f0 subsd xmm5, xmm0
    x0 = ld(m, ebx + 0xa840);                                      // 4292f4 movaps xmm0, [ebx + 0xa840]
    x5 = cvtpd2ps(x5);                                             // 4292fb cvtpd2ps xmm5, xmm5
    x2 = x6;                                                       // 4292ff movaps xmm2, xmm6
    x0 = shufps(x0, x0, 0x55);                                     // 429302 shufps xmm0, xmm0, 0x55
    st32(m, esp + 0x34, x6.u[0]);                                  // 429306 movss [esp + 0x34], xmm6
    st32(m, esp + 0x38, x5.u[0]);                                  // 42930c movss [esp + 0x38], xmm5
    st32(m, esp + 0x4b4, x2.u[0]);                                 // 429312 movss [esp + 0x4b4], xmm2
    st(m, esp + 0x4c0, x0);                                        // 42931b movaps [esp + 0x4c0], xmm0
    if (fl.e()) goto L_0042936f;                                   // 429323 je 0x42936f
    x0 = X::ss(ld32(m, esp + 0x4c4));                              // 429325 movss xmm0, [esp + 0x4c4]
    k.call(0x00401000);  // FUN_00401000                           // 42932e call 0x401000
    x5 = X::ss(ld32(m, esp + 0x38));                               // 429333 movss xmm5, [esp + 0x38]
    x3 = X{};                                                      // 429339 xorps xmm3, xmm3
    x4 = X::ss(0x80000000u);                                       // 42933c movss xmm4, [0x9774a8]
    x0 = cvtss2sd(x0, x0);                                         // 429344 cvtss2sd xmm0, xmm0
    fl = f_comisd(x0.q(0), 0x3feb333340000000ull);                 // 429348 comisd xmm0, [0x977b70]
    if (fl.be()) goto L_00429366;                                  // 429350 jbe 0x429366
    x0 = cvtss2sd(x0, X::ss(ld32(m, esp + 0x34)));                 // 429352 cvtss2sd xmm0, [esp + 0x34]
    x0 = mulsd(x0, sd(0x3fd0000000000000ull));                     // 429358 mulsd xmm0, [0x977868]
    x2 = cvtsd2ss(x2, x0);                                         // 429360 cvtsd2ss xmm2, xmm0
    goto L_0042936f;                                               // 429364 jmp 0x42936f
L_00429366:
    x2 = X::ss(ld32(m, esp + 0x4b4));                              // 429366 movss xmm2, [esp + 0x4b4]
L_0042936f:
    x1 = X::ss(ld32(m, ebx + 0xe80));                              // 42936f movss xmm1, [ebx + 0xe80]
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x30)));                 // 429377 cvtss2sd xmm7, [esp + 0x30]
    x0 = x4;                                                       // 42937d movaps xmm0, xmm4
    x0 = subss(x0, x1);                                            // 429380 subss xmm0, xmm1
    x1 = x4;                                                       // 429384 movaps xmm1, xmm4
    x4 = subss(x4, X::ss(ld32(m, ebx + 0xe88)));                   // 429387 subss xmm4, [ebx + 0xe88]
    x1 = subss(x1, X::ss(ld32(m, ebx + 0xe84)));                   // 42938f subss xmm1, [ebx + 0xe84]
    st32(m, esp + 0xc8, x4.u[0]);                                  // 429397 movss [esp + 0xc8], xmm4
    x4 = X::ss(ld32(m, ebx + 0xe8c));                              // 4293a0 movss xmm4, [ebx + 0xe8c]
    x4 = cvtps2pd(x4);                                             // 4293a8 cvtps2pd xmm4, xmm4
    x4 = mulsd(x4, x3);                                            // 4293ab mulsd xmm4, xmm3
    x6 = cvtss2sd(x6, x0);                                         // 4293af cvtss2sd xmm6, xmm0
    x6 = mulsd(x6, x7);                                            // 4293b3 mulsd xmm6, xmm7
    x4 = subsd(x4, x6);                                            // 4293b7 subsd xmm4, xmm6
    x7 = cvtss2sd(x7, x2);                                         // 4293bb cvtss2sd xmm7, xmm2
    x6 = cvtps2pd(x1);                                             // 4293bf cvtps2pd xmm6, xmm1
    x6 = mulsd(x6, x7);                                            // 4293c2 mulsd xmm6, xmm7
    x4 = subsd(x4, x6);                                            // 4293c6 subsd xmm4, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0xc8)));                 // 4293ca cvtss2sd xmm6, [esp + 0xc8]
    x7 = cvtss2sd(x7, x5);                                         // 4293d3 cvtss2sd xmm7, xmm5
    x6 = mulsd(x6, x7);                                            // 4293d7 mulsd xmm6, xmm7
    x4 = subsd(x4, x6);                                            // 4293db subsd xmm4, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x30)));                 // 4293df cvtss2sd xmm6, [esp + 0x30]
    x4 = cvtpd2ps(x4);                                             // 4293e5 cvtpd2ps xmm4, xmm4
    st32(m, esp + 0x2c, x4.u[0]);                                  // 4293e9 movss [esp + 0x2c], xmm4
    x4 = X::ss(ld32(m, ebx + 0xe8c));                              // 4293ef movss xmm4, [ebx + 0xe8c]
    x4 = cvtps2pd(x4);                                             // 4293f7 cvtps2pd xmm4, xmm4
    x4 = mulsd(x4, x6);                                            // 4293fa mulsd xmm4, xmm6
    x6 = cvtss2sd(x6, x0);                                         // 4293fe cvtss2sd xmm6, xmm0
    x6 = mulsd(x6, x3);                                            // 429402 mulsd xmm6, xmm3
    x4 = addsd(x4, x6);                                            // 429406 addsd xmm4, xmm6
    x7 = cvtss2sd(x7, x5);                                         // 42940a cvtss2sd xmm7, xmm5
    x6 = cvtps2pd(x1);                                             // 42940e cvtps2pd xmm6, xmm1
    x6 = mulsd(x6, x7);                                            // 429411 mulsd xmm6, xmm7
    x4 = addsd(x4, x6);                                            // 429415 addsd xmm4, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0xc8)));                 // 429419 cvtss2sd xmm6, [esp + 0xc8]
    x7 = cvtss2sd(x7, x2);                                         // 429422 cvtss2sd xmm7, xmm2
    x6 = mulsd(x6, x7);                                            // 429426 mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x30)));                 // 42942a cvtss2sd xmm7, [esp + 0x30]
    x4 = subsd(x4, x6);                                            // 429430 subsd xmm4, xmm6
    x4 = cvtpd2ps(x4);                                             // 429434 cvtpd2ps xmm4, xmm4
    st32(m, esp + 0x20, x4.u[0]);                                  // 429438 movss [esp + 0x20], xmm4
    x4 = X::ss(ld32(m, ebx + 0xe8c));                              // 42943e movss xmm4, [ebx + 0xe8c]
    x4 = cvtps2pd(x4);                                             // 429446 cvtps2pd xmm4, xmm4
    x6 = cvtss2sd(x6, x2);                                         // 429449 cvtss2sd xmm6, xmm2
    x4 = mulsd(x4, x6);                                            // 42944d mulsd xmm4, xmm6
    x6 = cvtps2pd(x1);                                             // 429451 cvtps2pd xmm6, xmm1
    x6 = mulsd(x6, x3);                                            // 429454 mulsd xmm6, xmm3
    x4 = addsd(x4, x6);                                            // 429458 addsd xmm4, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0xc8)));                 // 42945c cvtss2sd xmm6, [esp + 0xc8]
    x6 = mulsd(x6, x7);                                            // 429465 mulsd xmm6, xmm7
    x4 = addsd(x4, x6);                                            // 429469 addsd xmm4, xmm6
    x6 = cvtss2sd(x6, x0);                                         // 42946d cvtss2sd xmm6, xmm0
    x7 = cvtss2sd(x7, x5);                                         // 429471 cvtss2sd xmm7, xmm5
    x6 = mulsd(x6, x7);                                            // 429475 mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x2c)));                 // 429479 cvtss2sd xmm7, [esp + 0x2c]
    x4 = subsd(x4, x6);                                            // 42947f subsd xmm4, xmm6
    x6 = X::ss(ld32(m, ebx + 0xe8c));                              // 429483 movss xmm6, [ebx + 0xe8c]
    x6 = cvtps2pd(x6);                                             // 42948b cvtps2pd xmm6, xmm6
    x5 = cvtss2sd(x5, x5);                                         // 42948e cvtss2sd xmm5, xmm5
    x6 = mulsd(x6, x5);                                            // 429492 mulsd xmm6, xmm5
    x5 = cvtss2sd(x5, X::ss(ld32(m, esp + 0xc8)));                 // 429496 cvtss2sd xmm5, [esp + 0xc8]
    x0 = cvtss2sd(x0, x0);                                         // 42949f cvtss2sd xmm0, xmm0
    x5 = mulsd(x5, x3);                                            // 4294a3 mulsd xmm5, xmm3
    x3 = X::ss(ld32(m, ebx + 0xe88));                              // 4294a7 movss xmm3, [ebx + 0xe88]
    x6 = addsd(x6, x5);                                            // 4294af addsd xmm6, xmm5
    x2 = cvtss2sd(x2, x2);                                         // 4294b3 cvtss2sd xmm2, xmm2
    x0 = mulsd(x0, x2);                                            // 4294b7 mulsd xmm0, xmm2
    x2 = X::ss(ld32(m, ebx + 0xe8c));                              // 4294bb movss xmm2, [ebx + 0xe8c]
    x6 = addsd(x6, x0);                                            // 4294c3 addsd xmm6, xmm0
    x0 = cvtps2pd(x1);                                             // 4294c7 cvtps2pd xmm0, xmm1
    x1 = cvtss2sd(x1, X::ss(ld32(m, esp + 0x30)));                 // 4294ca cvtss2sd xmm1, [esp + 0x30]
    x0 = mulsd(x0, x1);                                            // 4294d0 mulsd xmm0, xmm1
    x6 = subsd(x6, x0);                                            // 4294d4 subsd xmm6, xmm0
    x0 = cvtss2sd(x0, X::ss(ld32(m, esp + 0x20)));                 // 4294d8 cvtss2sd xmm0, [esp + 0x20]
    x1 = cvtpd2ps(x6);                                             // 4294de cvtpd2ps xmm1, xmm6
    x6 = cvtps2pd(x2);                                             // 4294e2 cvtps2pd xmm6, xmm2
    x6 = mulsd(x6, x0);                                            // 4294e5 mulsd xmm6, xmm0
    x0 = X::ss(ld32(m, ebx + 0xe80));                              // 4294e9 movss xmm0, [ebx + 0xe80]
    x4 = cvtpd2ps(x4);                                             // 4294f1 cvtpd2ps xmm4, xmm4
    x5 = cvtps2pd(x0);                                             // 4294f5 cvtps2pd xmm5, xmm0
    x5 = mulsd(x5, x7);                                            // 4294f8 mulsd xmm5, xmm7
    x6 = addsd(x6, x5);                                            // 4294fc addsd xmm6, xmm5
    x5 = cvtps2pd(x3);                                             // 429500 cvtps2pd xmm5, xmm3
    x7 = cvtss2sd(x7, x4);                                         // 429503 cvtss2sd xmm7, xmm4
    x5 = mulsd(x5, x7);                                            // 429507 mulsd xmm5, xmm7
    x6 = addsd(x6, x5);                                            // 42950b addsd xmm6, xmm5
    x5 = X::ss(ld32(m, ebx + 0xe84));                              // 42950f movss xmm5, [ebx + 0xe84]
    x5 = cvtps2pd(x5);                                             // 429517 cvtps2pd xmm5, xmm5
    x7 = cvtss2sd(x7, x1);                                         // 42951a cvtss2sd xmm7, xmm1
    x5 = mulsd(x5, x7);                                            // 42951e mulsd xmm5, xmm7
    x6 = subsd(x6, x5);                                            // 429522 subsd xmm6, xmm5
    x5 = cvtpd2ps(x6);                                             // 429526 cvtpd2ps xmm5, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x2c)));                 // 42952a cvtss2sd xmm6, [esp + 0x2c]
    st32(m, esp + 0x200, x5.u[0]);                                 // 429530 movss [esp + 0x200], xmm5
    x5 = X::ss(ld32(m, ebx + 0xe84));                              // 429539 movss xmm5, [ebx + 0xe84]
    x5 = cvtps2pd(x5);                                             // 429541 cvtps2pd xmm5, xmm5
    x5 = mulsd(x5, x6);                                            // 429544 mulsd xmm5, xmm6
    x6 = cvtps2pd(x2);                                             // 429548 cvtps2pd xmm6, xmm2
    x7 = cvtss2sd(x7, x4);                                         // 42954b cvtss2sd xmm7, xmm4
    x6 = mulsd(x6, x7);                                            // 42954f mulsd xmm6, xmm7
    x5 = addsd(x5, x6);                                            // 429553 addsd xmm5, xmm6
    x6 = cvtps2pd(x0);                                             // 429557 cvtps2pd xmm6, xmm0
    x7 = cvtss2sd(x7, x1);                                         // 42955a cvtss2sd xmm7, xmm1
    x6 = mulsd(x6, x7);                                            // 42955e mulsd xmm6, xmm7
    x7 = cvtss2sd(x7, X::ss(ld32(m, esp + 0x20)));                 // 429562 cvtss2sd xmm7, [esp + 0x20]
    x5 = addsd(x5, x6);                                            // 429568 addsd xmm5, xmm6
    x6 = cvtps2pd(x3);                                             // 42956c cvtps2pd xmm6, xmm3
    x1 = cvtss2sd(x1, x1);                                         // 42956f cvtss2sd xmm1, xmm1
    x6 = mulsd(x6, x7);                                            // 429573 mulsd xmm6, xmm7
    x3 = cvtps2pd(x3);                                             // 429577 cvtps2pd xmm3, xmm3
    x5 = subsd(x5, x6);                                            // 42957a subsd xmm5, xmm6
    x6 = cvtss2sd(x6, X::ss(ld32(m, esp + 0x2c)));                 // 42957e cvtss2sd xmm6, [esp + 0x2c]
    x3 = mulsd(x3, x6);                                            // 429584 mulsd xmm3, xmm6
    x2 = cvtps2pd(x2);                                             // 429588 cvtps2pd xmm2, xmm2
    x2 = mulsd(x2, x1);                                            // 42958b mulsd xmm2, xmm1
    x1 = X::ss(ld32(m, ebx + 0xe84));                              // 42958f movss xmm1, [ebx + 0xe84]
    x3 = addsd(x3, x2);                                            // 429597 addsd xmm3, xmm2
    x2 = cvtss2sd(x2, X::ss(ld32(m, esp + 0x20)));                 // 42959b cvtss2sd xmm2, [esp + 0x20]
    x1 = cvtps2pd(x1);                                             // 4295a1 cvtps2pd xmm1, xmm1
    x0 = cvtps2pd(x0);                                             // 4295a4 cvtps2pd xmm0, xmm0
    x1 = mulsd(x1, x2);                                            // 4295a7 mulsd xmm1, xmm2
    x3 = addsd(x3, x1);                                            // 4295ab addsd xmm3, xmm1
    x1 = cvtss2sd(x1, x4);                                         // 4295af cvtss2sd xmm1, xmm4
    x0 = mulsd(x0, x1);                                            // 4295b3 mulsd xmm0, xmm1
    x1 = X::ss(ld32(m, esp + 0x200));                              // 4295b7 movss xmm1, [esp + 0x200]
    x3 = subsd(x3, x0);                                            // 4295c0 subsd xmm3, xmm0
    x0 = cvtpd2ps(x3);                                             // 4295c4 cvtpd2ps xmm0, xmm3
    st32(m, esp + 0x4b8, x0.u[0]);                                 // 4295c8 movss [esp + 0x4b8], xmm0
    x5 = cvtpd2ps(x5);                                             // 4295d1 cvtpd2ps xmm5, xmm5
    st32(m, esp + 0x4b0, x1.u[0]);                                 // 4295d5 movss [esp + 0x4b0], xmm1
    st32(m, esp + 0x4b4, x5.u[0]);                                 // 4295de movss [esp + 0x4b4], xmm5
    x0 = ld(m, esp + 0x4b0);                                       // 4295e7 movaps xmm0, [esp + 0x4b0]
    st(m, ebx + 0xe90, x0);                                        // 4295ef movaps [ebx + 0xe90], xmm0
    fl = f_cmp(ld32(m, ebx + 0xa834), 0x0u);                       // 4295f6 cmp [ebx + 0xa834], 0
    x0 = X::ss(ld32(m, ebx + 0x410));                              // 4295fd movss xmm0, [ebx + 0x410]
    st32(m, esp + 0x30, x0.u[0]);                                  // 429605 movss [esp + 0x30], xmm0
    x0 = X::ss(ld32(m, ebx + 0x414));                              // 42960b movss xmm0, [ebx + 0x414]
    st32(m, esp + 0x34, x0.u[0]);                                  // 429613 movss [esp + 0x34], xmm0
    x0 = X::ss(ld32(m, ebx + 0x418));                              // 429619 movss xmm0, [ebx + 0x418]
    st32(m, esp + 0x38, x0.u[0]);                                  // 429621 movss [esp + 0x38], xmm0
    if (fl.e()) goto L_004296a9;                                   // 429627 je 0x4296a9
    x0 = X::ss(ld32(m, esp + 0x4c4));                              // 42962d movss xmm0, [esp + 0x4c4]
    k.call(0x00401000);  // FUN_00401000                           // 429636 call 0x401000
    x0 = cvtss2sd(x0, x0);                                         // 42963b cvtss2sd xmm0, xmm0
    fl = f_comisd(x0.q(0), 0x3fecccccc0000000ull);                 // 42963f comisd xmm0, [0x977b68]
    if (fl.be()) goto L_004296a9;                                  // 429647 jbe 0x4296a9
    x0 = X::ss(ld32(m, esp + 0x30));                               // 429649 movss xmm0, [esp + 0x30]
    k.call(0x00401000);  // FUN_00401000                           // 42964f call 0x401000
    x1 = X::ss(0x40000000u);                                       // 429654 movss xmm1, [0x92e330]
    fl = f_comiss(x1.f(0), x0.f(0));                               // 42965c comiss xmm1, xmm0
    if (fl.be()) goto L_004296a9;                                  // 42965f jbe 0x4296a9
    x0 = X::ss(ld32(m, esp + 0x38));                               // 429661 movss xmm0, [esp + 0x38]
    k.call(0x00401000);  // FUN_00401000                           // 429667 call 0x401000
    x1 = X::ss(0x40000000u);                                       // 42966c movss xmm1, [0x92e330]
    fl = f_comiss(x1.f(0), x0.f(0));                               // 429674 comiss xmm1, xmm0
    if (fl.be()) goto L_004296a9;                                  // 429677 jbe 0x4296a9
    x0 = sd(0x3fd99999a0000000ull);                                // 429679 movsd xmm0, [0x977b60]
    x1 = cvtss2sd(x1, X::ss(ld32(m, esp + 0x30)));                 // 429681 cvtss2sd xmm1, [esp + 0x30]
    x1 = mulsd(x1, x0);                                            // 429687 mulsd xmm1, xmm0
    x1 = cvtpd2ps(x1);                                             // 42968b cvtpd2ps xmm1, xmm1
    st32(m, esp + 0x30, x1.u[0]);                                  // 42968f movss [esp + 0x30], xmm1
    x1 = cvtss2sd(x1, X::ss(ld32(m, esp + 0x38)));                 // 429695 cvtss2sd xmm1, [esp + 0x38]
    x1 = mulsd(x1, x0);                                            // 42969b mulsd xmm1, xmm0
    x0 = cvtpd2ps(x1);                                             // 42969f cvtpd2ps xmm0, xmm1
    st32(m, esp + 0x38, x0.u[0]);                                  // 4296a3 movss [esp + 0x38], xmm0
L_004296a9:
    x0 = ld(m, esp + 0x30);                                        // 4296a9 movaps xmm0, [esp + 0x30]
    st(m, ebx + 0x410, x0);                                        // 4296ae movaps [ebx + 0x410], xmm0
L_004296b5:
    ecx = ld32(m, esp + 0x4dc);                                    // 4296b5 mov ecx, [esp + 0x4dc]
    edi = k.pop();                                                 // 4296bc pop edi
    esi = k.pop();                                                 // 4296bd pop esi
    ebx = k.pop();                                                 // 4296be pop ebx
    ecx = ecx ^ esp;                                               // 4296bf xor ecx, esp
    k.call(0x008f9efe);  // __security_check_cookie                // 4296c1 call 0x8f9efe
    esp = ebp;                                                     // 4296c6 mov esp, ebp
    ebp = k.pop();                                                 // 4296c8 pop ebp
    return k.ret(8);                                               // 4296c9 ret 8
}

// 004296d0 FUN_004296d0: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x004296d0, Fn_004296d0, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    ecx = ld32(m, edx + 0x3fc);                                    // 4296d0 mov ecx, [edx + 0x3fc]
    eax = 0u;                                                      // 4296d6 xor eax, eax
    fl = f_test(ecx, ecx);                                         // 4296d8 test ecx, ecx
    if (fl.le()) goto L_004296ee;                                  // 4296da jle 0x4296ee
    edx = edx + 0xa540u;                                           // 4296dc add edx, 0xa540
L_004296e2:
    fl = f_cmp(ld32(m, edx), esi);                                 // 4296e2 cmp [edx], esi
    if (fl.e()) goto L_004296f1;                                   // 4296e4 je 0x4296f1
    eax = eax + 1u;                                                // 4296e6 inc eax
    edx = edx + 0x4u;                                              // 4296e7 add edx, 4
    fl = f_cmp(eax, ecx);                                          // 4296ea cmp eax, ecx
    if (fl.l()) goto L_004296e2;                                   // 4296ec jl 0x4296e2
L_004296ee:
    eax = eax | 0xffffffffu;                                       // 4296ee or eax, 0xffffffff
L_004296f1:
    return k.ret(0);                                               // 4296f1 ret 
}

// 00429910 FUN_00429910: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00429910, Fn_00429910, "math", 0) {  // outputs: what callers read (it calls other groups' code)
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 429910 push ebp
    ebp = esp;                                                     // 429911 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 429913 and esp, 0xfffffff0
    esp = esp - 0x54u;                                             // 429916 sub esp, 0x54
    k.push(ebx);                                                   // 429919 push ebx
    k.push(esi);                                                   // 42991a push esi
    k.push(edi);                                                   // 42991b push edi
    edi = ecx;                                                     // 42991c mov edi, ecx
    eax = edi + 0xa764;                                            // 42991e lea eax, [edi + 0xa764]
    st32(m, esp + 0x1c, edi);                                      // 429924 mov [esp + 0x1c], edi
    st32(m, esp + 0x14, eax);                                      // 429928 mov [esp + 0x14], eax
    ebx = edi + 0xc;                                               // 42992c lea ebx, [edi + 0xc]
    st32(m, esp + 0x18, 0x10u);                                    // 42992f mov [esp + 0x18], 0x10
L_00429937:
    ecx = ld32(m, esp + 0x14);                                     // 429937 mov ecx, [esp + 0x14]
    eax = ld32(m, ecx);                                            // 42993b mov eax, [ecx]
    edx = eax + 0x8;                                               // 42993d lea edx, [eax + 8]
    ecx = eax;                                                     // 429940 mov ecx, eax
    eax = eax * 0x70u;                                             // 429942 imul eax, eax, 0x70
    edx = edx << 7;                                                // 429945 shl edx, 7
    x0 = ld(m, edx + edi);                                         // 429948 movaps xmm0, [edx + edi]
    st(m, esp + 0x20, x0);                                         // 42994c movaps [esp + 0x20], xmm0
    ecx = ecx << 7;                                                // 429951 shl ecx, 7
    x0 = ld(m, ecx + edi + 0x410);                                 // 429954 movaps xmm0, [ecx + edi + 0x410]
    eax = eax + edi;                                               // 42995c add eax, edi
    st(m, esp + 0x30, x0);                                         // 42995e movaps [esp + 0x30], xmm0
    x0 = ld(m, eax + 0xe80);                                       // 429963 movaps xmm0, [eax + 0xe80]
    st(m, esp + 0x40, x0);                                         // 42996a movaps [esp + 0x40], xmm0
    x0 = ld(m, eax + 0xe90);                                       // 42996f movaps xmm0, [eax + 0xe90]
    eax = ld32(m, ebx);                                            // 429976 mov eax, [ebx]
    st(m, esp + 0x50, x0);                                         // 429978 movaps [esp + 0x50], xmm0
    fl = f_test(eax, eax);                                         // 42997d test eax, eax
    if (fl.e()) goto L_00429a07;                                   // 42997f je 0x429a07
    esi = ld32(m, eax + 0x4);                                      // 429985 mov esi, [eax + 4]
    edi = ld32(m, eax + 0x8);                                      // 429988 mov edi, [eax + 8]
    ecx = ld32(m, esi + 0x1964);                                   // 42998b mov ecx, [esi + 0x1964]
    ecx = ecx - ld32(m, esi + 0x1960);                             // 429991 sub ecx, [esi + 0x1960]
    esi = esi + 0x1954u;                                           // 429997 add esi, 0x1954
    eax = 0x92492493u;                                             // 42999d mov eax, 0x92492493
    { int64_t p_ = int64_t(int32_t(eax)) * int32_t(ecx); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); } // 4299a2 imul ecx
    edx = edx + ecx;                                               // 4299a4 add edx, ecx
    edx = uint32_t(int32_t(edx) >> 7);                             // 4299a6 sar edx, 7
    eax = edx;                                                     // 4299a9 mov eax, edx
    eax = eax >> 31;                                               // 4299ab shr eax, 0x1f
    eax = eax + edx;                                               // 4299ae add eax, edx
    fl = f_cmp(edi, eax);                                          // 4299b0 cmp edi, eax
    if (fl.b()) goto L_004299ba;                                   // 4299b2 jb 0x4299ba
    k.call(ld32(m, 0x916224));                                     // 4299b4 call [0x916224]
L_004299ba:
    x0 = ld(m, esp + 0x20);                                        // 4299ba movaps xmm0, [esp + 0x20]
    edi = edi * 0xe0u;                                             // 4299bf imul edi, edi, 0xe0
    edi = edi + ld32(m, esi + 0xc);                                // 4299c5 add edi, [esi + 0xc]
    st(m, edi + 0x40, x0);                                         // 4299c8 movaps [edi + 0x40], xmm0
    x0 = ld(m, esp + 0x40);                                        // 4299cc movaps xmm0, [esp + 0x40]
    st(m, edi + 0x50, x0);                                         // 4299d1 movaps [edi + 0x50], xmm0
    x0 = ld(m, esp + 0x30);                                        // 4299d5 movaps xmm0, [esp + 0x30]
    st(m, edi + 0x60, x0);                                         // 4299da movaps [edi + 0x60], xmm0
    x0 = ld(m, esp + 0x50);                                        // 4299de movaps xmm0, [esp + 0x50]
    st(m, edi + 0x70, x0);                                         // 4299e3 movaps [edi + 0x70], xmm0
    x0 = ld(m, 0xd61670);                                          // 4299e7 movaps xmm0, [0xd61670]
    st(m, edi + 0xb0, x0);                                         // 4299ee movaps [edi + 0xb0], xmm0
    x0 = ld(m, 0xd61670);                                          // 4299f5 movaps xmm0, [0xd61670]
    st(m, edi + 0xc0, x0);                                         // 4299fc movaps [edi + 0xc0], xmm0
    edi = ld32(m, esp + 0x1c);                                     // 429a03 mov edi, [esp + 0x1c]
L_00429a07:
    eax = ld32(m, ebx);                                            // 429a07 mov eax, [ebx]
    esi = ld32(m, eax + 0x4);                                      // 429a09 mov esi, [eax + 4]
    ecx = ld32(m, eax + 0x8);                                      // 429a0c mov ecx, [eax + 8]
    esi = esi + 0x1954u;                                           // 429a0f add esi, 0x1954
    k.call(0x0042b000);  // FUN_0042b000                           // 429a15 call 0x42b000
    st32(m, esp + 0x14, ld32(m, esp + 0x14) + 0x4u);               // 429a1a add [esp + 0x14], 4
    ebx = ebx + 0x8u;                                              // 429a1f add ebx, 8
    { uint32_t a_ = ld32(m, esp + 0x18), b_ = 0x1u, r_ = a_ - b_; fl = f_sub(a_, b_, r_); st32(m, esp + 0x18, r_); } // 429a22 sub [esp + 0x18], 1
    if (fl.ne()) goto L_00429937;                                  // 429a27 jne 0x429937
    edi = k.pop();                                                 // 429a2d pop edi
    esi = k.pop();                                                 // 429a2e pop esi
    ebx = k.pop();                                                 // 429a2f pop ebx
    esp = ebp;                                                     // 429a30 mov esp, ebp
    ebp = k.pop();                                                 // 429a32 pop ebp
    return k.ret(0);                                               // 429a33 ret 
}

// 00429a40 FUN_00429a40: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x00429a40, Fn_00429a40, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 429a40 push ebp
    ebp = esp;                                                     // 429a41 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 429a43 and esp, 0xfffffff0
    esp = esp - 0xe4u;                                             // 429a46 sub esp, 0xe4
    eax = ld32(m, 0x9b45e4);                                       // 429a4c mov eax, [0x9b45e4]
    eax = eax ^ esp;                                               // 429a51 xor eax, esp
    st32(m, esp + 0xe0, eax);                                      // 429a53 mov [esp + 0xe0], eax
    eax = ld32(m, ebp + 0x8);                                      // 429a5a mov eax, [ebp + 8]
    x4 = ld(m, esp + 0x44);                                        // 429a5d movaps xmm4, [esp + 0x44]
    k.push(ebx);                                                   // 429a62 push ebx
    st32(m, esp + 0xc, eax);                                       // 429a63 mov [esp + 0xc], eax
    st8(m, eax + 0x88, 0x0u);                                      // 429a67 mov byte ptr [eax + 0x88], 0
    ebx = 0u;                                                      // 429a6e xor ebx, ebx
    k.push(esi);                                                   // 429a70 push esi
    eax = eax + 0xcu;                                              // 429a71 add eax, 0xc
    k.push(edi);                                                   // 429a74 push edi
    st32(m, esp + 0x10, eax);                                      // 429a75 mov [esp + 0x10], eax
L_00429a79:
    eax = ld32(m, eax);                                            // 429a79 mov eax, [eax]
    st8(m, esp + ebx + 0xdc, 0x0u);                                // 429a7b mov byte ptr [esp + ebx + 0xdc], 0
    fl = f_test(eax, eax);                                         // 429a83 test eax, eax
    if (fl.e()) goto L_00429acd;                                   // 429a85 je 0x429acd
    edi = ld32(m, eax + 0x4);                                      // 429a87 mov edi, [eax + 4]
    esi = ld32(m, eax + 0x8);                                      // 429a8a mov esi, [eax + 8]
    ecx = ld32(m, edi + 0x1964);                                   // 429a8d mov ecx, [edi + 0x1964]
    ecx = ecx - ld32(m, edi + 0x1960);                             // 429a93 sub ecx, [edi + 0x1960]
    edi = edi + 0x1954u;                                           // 429a99 add edi, 0x1954
    eax = 0x92492493u;                                             // 429a9f mov eax, 0x92492493
    { int64_t p_ = int64_t(int32_t(eax)) * int32_t(ecx); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); } // 429aa4 imul ecx
    edx = edx + ecx;                                               // 429aa6 add edx, ecx
    edx = uint32_t(int32_t(edx) >> 7);                             // 429aa8 sar edx, 7
    eax = edx;                                                     // 429aab mov eax, edx
    eax = eax >> 31;                                               // 429aad shr eax, 0x1f
    eax = eax + edx;                                               // 429ab0 add eax, edx
    fl = f_cmp(esi, eax);                                          // 429ab2 cmp esi, eax
    if (fl.b()) goto L_00429abc;                                   // 429ab4 jb 0x429abc
    k.call(ld32(m, 0x916224));                                     // 429ab6 call [0x916224]
L_00429abc:
    ecx = ld32(m, edi + 0xc);                                      // 429abc mov ecx, [edi + 0xc]
    esi = esi * 0xe0u;                                             // 429abf imul esi, esi, 0xe0
    x4 = ld(m, esi + ecx + 0xc0);                                  // 429ac5 movaps xmm4, [esi + ecx + 0xc0]
L_00429acd:
    x0 = x4;                                                       // 429acd movaps xmm0, xmm4
    x0 = mulps(x0, x4);                                            // 429ad0 mulps xmm0, xmm4
    x1 = x0;                                                       // 429ad3 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x66);                                     // 429ad6 shufps xmm1, xmm0, 0x66
    x0 = addss(x0, x1);                                            // 429ada addss xmm0, xmm1
    x1 = shufps(x1, x1, 0x55);                                     // 429ade shufps xmm1, xmm1, 0x55
    x0 = addss(x0, x1);                                            // 429ae2 addss xmm0, xmm1
    x1 = ld(m, 0xd61670);                                          // 429ae6 movaps xmm1, [0xd61670]
    x0 = shufps(x0, x0, 0x00);                                     // 429aed shufps xmm0, xmm0, 0
    x0 = sqrtps(x0);                                               // 429af1 sqrtps xmm0, xmm0
    x1 = cmpltps(x1, x0);                                          // 429af4 cmpltps xmm1, xmm0
    st(m, esp + 0x50, x1);                                         // 429af8 movaps [esp + 0x50], xmm1
    edx = ld32(m, esp + 0x54);                                     // 429afd mov edx, [esp + 0x54]
    eax = ld32(m, esp + 0x58);                                     // 429b01 mov eax, [esp + 0x58]
    eax = eax + edx;                                               // 429b05 add eax, edx
    eax = eax + ld32(m, esp + 0x50);                               // 429b07 add eax, [esp + 0x50]
    fl = f_cmp(eax, 0xfffffffdu);                                  // 429b0b cmp eax, -3
    if (fl.ne()) goto L_00429b23;                                  // 429b0e jne 0x429b23
    ecx = ld32(m, esp + 0x14);                                     // 429b10 mov ecx, [esp + 0x14]
    st8(m, ecx + 0x88, 0x1u);                                      // 429b14 mov byte ptr [ecx + 0x88], 1
    st8(m, esp + ebx + 0xdc, 0x1u);                                // 429b1b mov byte ptr [esp + ebx + 0xdc], 1
L_00429b23:
    eax = ld32(m, esp + 0x10);                                     // 429b23 mov eax, [esp + 0x10]
    ebx = ebx + 1u;                                                // 429b27 inc ebx
    eax = eax + 0x8u;                                              // 429b28 add eax, 8
    fl = f_cmp(ebx, 0x10u);                                        // 429b2b cmp ebx, 0x10
    st32(m, esp + 0x10, eax);                                      // 429b2e mov [esp + 0x10], eax
    if (fl.l()) goto L_00429a79;                                   // 429b32 jl 0x429a79
    eax = ld32(m, esp + 0x14);                                     // 429b38 mov eax, [esp + 0x14]
    ecx = 0u;                                                      // 429b3c xor ecx, ecx
    edx = eax + 0xa7b8;                                            // 429b3e lea edx, [eax + 0xa7b8]
    eax = eax + 0xcu;                                              // 429b44 add eax, 0xc
    st(m, esp + 0x50, x4);                                         // 429b47 movaps [esp + 0x50], xmm4
    st32(m, esp + 0x1c, ecx);                                      // 429b4c mov [esp + 0x1c], ecx
    st32(m, esp + 0x18, edx);                                      // 429b50 mov [esp + 0x18], edx
    st32(m, esp + 0x10, eax);                                      // 429b54 mov [esp + 0x10], eax
L_00429b58:
    fl = f_cmp8(ld8(m, esp + ecx + 0xdc), 0x0u);                   // 429b58 cmp byte ptr [esp + ecx + 0xdc], 0
    if (fl.e()) goto L_0042a090;                                   // 429b60 je 0x42a090
    eax = ld32(m, eax);                                            // 429b66 mov eax, [eax]
    ecx = ld32(m, esp + 0x18);                                     // 429b68 mov ecx, [esp + 0x18]
    esi = ld32(m, ecx - 0x54);                                     // 429b6c mov esi, [ecx - 0x54]
    fl = f_test(eax, eax);                                         // 429b6f test eax, eax
    if (fl.e()) goto L_00429c1c;                                   // 429b71 je 0x429c1c
    edi = ld32(m, eax + 0x4);                                      // 429b77 mov edi, [eax + 4]
    ebx = ld32(m, eax + 0x8);                                      // 429b7a mov ebx, [eax + 8]
    ecx = ld32(m, edi + 0x1964);                                   // 429b7d mov ecx, [edi + 0x1964]
    ecx = ecx - ld32(m, edi + 0x1960);                             // 429b83 sub ecx, [edi + 0x1960]
    edi = edi + 0x1954u;                                           // 429b89 add edi, 0x1954
    eax = 0x92492493u;                                             // 429b8f mov eax, 0x92492493
    { int64_t p_ = int64_t(int32_t(eax)) * int32_t(ecx); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); } // 429b94 imul ecx
    edx = edx + ecx;                                               // 429b96 add edx, ecx
    edx = uint32_t(int32_t(edx) >> 7);                             // 429b98 sar edx, 7
    eax = edx;                                                     // 429b9b mov eax, edx
    eax = eax >> 31;                                               // 429b9d shr eax, 0x1f
    eax = eax + edx;                                               // 429ba0 add eax, edx
    fl = f_cmp(ebx, eax);                                          // 429ba2 cmp ebx, eax
    if (fl.b()) goto L_00429bac;                                   // 429ba4 jb 0x429bac
    k.call(ld32(m, 0x916224));                                     // 429ba6 call [0x916224]
L_00429bac:
    edx = ld32(m, esp + 0x10);                                     // 429bac mov edx, [esp + 0x10]
    ebx = ebx * 0xe0u;                                             // 429bb0 imul ebx, ebx, 0xe0
    ecx = ld32(m, edi + 0xc);                                      // 429bb6 mov ecx, [edi + 0xc]
    eax = ld32(m, edx);                                            // 429bb9 mov eax, [edx]
    x4 = ld(m, ebx + ecx + 0xc0);                                  // 429bbb movaps xmm4, [ebx + ecx + 0xc0]
    st(m, esp + 0x50, x4);                                         // 429bc3 movaps [esp + 0x50], xmm4
    fl = f_test(eax, eax);                                         // 429bc8 test eax, eax
    if (fl.e()) goto L_00429c1c;                                   // 429bca je 0x429c1c
    edi = ld32(m, eax + 0x4);                                      // 429bcc mov edi, [eax + 4]
    ebx = ld32(m, eax + 0x8);                                      // 429bcf mov ebx, [eax + 8]
    ecx = ld32(m, edi + 0x1964);                                   // 429bd2 mov ecx, [edi + 0x1964]
    ecx = ecx - ld32(m, edi + 0x1960);                             // 429bd8 sub ecx, [edi + 0x1960]
    edi = edi + 0x1954u;                                           // 429bde add edi, 0x1954
    eax = 0x92492493u;                                             // 429be4 mov eax, 0x92492493
    { int64_t p_ = int64_t(int32_t(eax)) * int32_t(ecx); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); } // 429be9 imul ecx
    edx = edx + ecx;                                               // 429beb add edx, ecx
    edx = uint32_t(int32_t(edx) >> 7);                             // 429bed sar edx, 7
    eax = edx;                                                     // 429bf0 mov eax, edx
    eax = eax >> 31;                                               // 429bf2 shr eax, 0x1f
    eax = eax + edx;                                               // 429bf5 add eax, edx
    fl = f_cmp(ebx, eax);                                          // 429bf7 cmp ebx, eax
    if (fl.b()) goto L_00429c06;                                   // 429bf9 jb 0x429c06
    k.call(ld32(m, 0x916224));                                     // 429bfb call [0x916224]
    x4 = ld(m, esp + 0x50);                                        // 429c01 movaps xmm4, [esp + 0x50]
L_00429c06:
    ecx = ld32(m, edi + 0xc);                                      // 429c06 mov ecx, [edi + 0xc]
    ebx = ebx * 0xe0u;                                             // 429c09 imul ebx, ebx, 0xe0
    x0 = ld(m, ebx + ecx + 0xb0);                                  // 429c0f movaps xmm0, [ebx + ecx + 0xb0]
    st(m, esp + 0x60, x0);                                         // 429c17 movaps [esp + 0x60], xmm0
L_00429c1c:
    eax = ld32(m, esp + 0x14);                                     // 429c1c mov eax, [esp + 0x14]
    edx = esi;                                                     // 429c20 mov edx, esi
    edx = edx << 7;                                                // 429c22 shl edx, 7
    x1 = ld(m, edx + eax + 0x410);                                 // 429c25 movaps xmm1, [edx + eax + 0x410]
    ebx = edx + eax;                                               // 429c2d lea ebx, [edx + eax]
    x0 = x1;                                                       // 429c30 movaps xmm0, xmm1
    x0 = mulps(x0, x1);                                            // 429c33 mulps xmm0, xmm1
    x2 = x0;                                                       // 429c36 movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0x99);                                     // 429c39 shufps xmm2, xmm0, 0x99
    x0 = addss(x0, x2);                                            // 429c3d addss xmm0, xmm2
    x2 = shufps(x2, x2, 0x55);                                     // 429c41 shufps xmm2, xmm2, 0x55
    x0 = addss(x0, x2);                                            // 429c45 addss xmm0, xmm2
    x2 = X::v(0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000);     // 429c49 movaps xmm2, [0x92dc20]
    x0 = shufps(x0, x0, 0x00);                                     // 429c50 shufps xmm0, xmm0, 0
    x7 = sqrtps(x0);                                               // 429c54 sqrtps xmm7, xmm0
    x0 = cmpneqps(x0, x2);                                         // 429c57 cmpneqps xmm0, xmm2
    x5 = x0;                                                       // 429c5b movaps xmm5, xmm0
    x2 = x1;                                                       // 429c5e movaps xmm2, xmm1
    x2 = mulps(x2, x4);                                            // 429c61 mulps xmm2, xmm4
    x0 = x2;                                                       // 429c64 movaps xmm0, xmm2
    x0 = shufps(x0, x2, 0x99);                                     // 429c67 shufps xmm0, xmm2, 0x99
    x3 = x2;                                                       // 429c6b movaps xmm3, xmm2
    x3 = addss(x3, x0);                                            // 429c6e addss xmm3, xmm0
    x0 = shufps(x0, x0, 0x55);                                     // 429c72 shufps xmm0, xmm0, 0x55
    x3 = addss(x3, x0);                                            // 429c76 addss xmm3, xmm0
    x3 = shufps(x3, x3, 0x00);                                     // 429c7a shufps xmm3, xmm3, 0
    st(m, esp + 0x70, x3);                                         // 429c7e movaps [esp + 0x70], xmm3
    x6 = X{};                                                      // 429c83 xorps xmm6, xmm6
    x0 = x1;                                                       // 429c86 movaps xmm0, xmm1
    x0 = divps(x0, x7);                                            // 429c89 divps xmm0, xmm7
    x3 = x6;                                                       // 429c8c movaps xmm3, xmm6
    x3 = cmpneqps(x3, x7);                                         // 429c8f cmpneqps xmm3, xmm7
    x0 = andps(x0, x3);                                            // 429c93 andps xmm0, xmm3
    x3 = X::v(0x7fc00000, 0x7fc00000, 0x7fc00000, 0x7fc00000);     // 429c96 movaps xmm3, [0x92dc30]
    x0 = andps(x0, x5);                                            // 429c9d andps xmm0, xmm5
    x5 = andnps(x5, x3);                                           // 429ca0 andnps xmm5, xmm3
    x0 = orps(x0, x5);                                             // 429ca3 orps xmm0, xmm5
    x0 = mulps(x0, x4);                                            // 429ca6 mulps xmm0, xmm4
    x3 = x0;                                                       // 429ca9 movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0x99);                                     // 429cac shufps xmm3, xmm0, 0x99
    x0 = addss(x0, x3);                                            // 429cb0 addss xmm0, xmm3
    x3 = shufps(x3, x3, 0x55);                                     // 429cb4 shufps xmm3, xmm3, 0x55
    x0 = addss(x0, x3);                                            // 429cb8 addss xmm0, xmm3
    x3 = x0;                                                       // 429cbc movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0x00);                                     // 429cbf shufps xmm3, xmm0, 0
    x3 = cmpltps(x3, ld(m, 0xd61670));                             // 429cc3 cmpltps xmm3, [0xd61670]
    st(m, esp + 0x40, x3);                                         // 429ccb movaps [esp + 0x40], xmm3
    ecx = ld32(m, esp + 0x44);                                     // 429cd0 mov ecx, [esp + 0x44]
    edx = ld32(m, esp + 0x48);                                     // 429cd4 mov edx, [esp + 0x48]
    edx = edx + ecx;                                               // 429cd8 add edx, ecx
    edx = edx + ld32(m, esp + 0x40);                               // 429cda add edx, [esp + 0x40]
    st(m, esp + 0xa0, x1);                                         // 429cde movaps [esp + 0xa0], xmm1
    st(m, esp + 0x20, x2);                                         // 429ce6 movaps [esp + 0x20], xmm2
    st(m, esp + 0x30, x6);                                         // 429ceb movaps [esp + 0x30], xmm6
    fl = f_cmp(edx, 0xfffffffdu);                                  // 429cf0 cmp edx, -3
    if (fl.ne()) goto L_00429e29;                                  // 429cf3 jne 0x429e29
    x6 = subps(x6, ld(m, esp + 0x70));                             // 429cf9 subps xmm6, [esp + 0x70]
    x0 = ld(m, ebx + 0x430);                                       // 429cfe movaps xmm0, [ebx + 0x430]
    x6 = mulps(x6, x4);                                            // 429d05 mulps xmm6, xmm4
    ecx = esi;                                                     // 429d08 mov ecx, esi
    ecx = ecx * 0x70u;                                             // 429d0a imul ecx, ecx, 0x70
    x2 = ld(m, ecx + eax + 0xe80);                                 // 429d0d movaps xmm2, [ecx + eax + 0xe80]
    x6 = divps(x6, x0);                                            // 429d15 divps xmm6, xmm0
    x0 = ld(m, esp + 0x60);                                        // 429d18 movaps xmm0, [esp + 0x60]
    edx = esi + 0x8;                                               // 429d1d lea edx, [esi + 8]
    edx = edx << 7;                                                // 429d20 shl edx, 7
    x1 = ld(m, edx + eax);                                         // 429d23 movaps xmm1, [edx + eax]
    x0 = subps(x0, x1);                                            // 429d27 subps xmm0, xmm1
    x1 = X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000);     // 429d2a movaps xmm1, [0x92db80]
    x0 = andps(x0, x1);                                            // 429d31 andps xmm0, xmm1
    x1 = x2;                                                       // 429d34 movaps xmm1, xmm2
    x1 = shufps(x1, x2, 0x1b);                                     // 429d37 shufps xmm1, xmm2, 0x1b
    x3 = x0;                                                       // 429d3b movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0x00);                                     // 429d3e shufps xmm3, xmm0, 0
    x3 = mulps(x3, x1);                                            // 429d42 mulps xmm3, xmm1
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 429d45 mulps xmm3, [0x93a1f0]
    x1 = shufps(x1, x1, 0xb1);                                     // 429d4c shufps xmm1, xmm1, 0xb1
    x4 = x0;                                                       // 429d50 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x55);                                     // 429d53 shufps xmm4, xmm0, 0x55
    x4 = mulps(x4, x1);                                            // 429d57 mulps xmm4, xmm1
    x1 = shufps(x1, x1, 0x1b);                                     // 429d5a shufps xmm1, xmm1, 0x1b
    x5 = x0;                                                       // 429d5e movaps xmm5, xmm0
    x5 = shufps(x5, x0, 0xaa);                                     // 429d61 shufps xmm5, xmm0, 0xaa
    x1 = mulps(x1, x5);                                            // 429d65 mulps xmm1, xmm5
    x5 = X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000);     // 429d68 movaps xmm5, [0x93a210]
    st(m, esp + 0x70, x6);                                         // 429d6f movaps [esp + 0x70], xmm6
    x6 = X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000);     // 429d74 movaps xmm6, [0x93a200]
    x4 = mulps(x4, x6);                                            // 429d7b mulps xmm4, xmm6
    x1 = mulps(x1, x5);                                            // 429d7e mulps xmm1, xmm5
    x1 = addps(x1, x4);                                            // 429d81 addps xmm1, xmm4
    x4 = x0;                                                       // 429d84 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0xff);                                     // 429d87 shufps xmm4, xmm0, 0xff
    x0 = X::v(0xbf800000, 0xbf800000, 0xbf800000, 0x3f800000);     // 429d8b movaps xmm0, [0x93a220]
    x4 = mulps(x4, x2);                                            // 429d92 mulps xmm4, xmm2
    x2 = mulps(x2, x0);                                            // 429d95 mulps xmm2, xmm0
    x0 = x2;                                                       // 429d98 movaps xmm0, xmm2
    x4 = addps(x4, x3);                                            // 429d9b addps xmm4, xmm3
    x1 = addps(x1, x4);                                            // 429d9e addps xmm1, xmm4
    x2 = x1;                                                       // 429da1 movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0x1b);                                     // 429da4 shufps xmm2, xmm1, 0x1b
    x3 = x0;                                                       // 429da8 movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0x00);                                     // 429dab shufps xmm3, xmm0, 0
    x3 = mulps(x3, x2);                                            // 429daf mulps xmm3, xmm2
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 429db2 mulps xmm3, [0x93a1f0]
    x2 = shufps(x2, x2, 0xb1);                                     // 429db9 shufps xmm2, xmm2, 0xb1
    x7 = x2;                                                       // 429dbd movaps xmm7, xmm2
    x7 = shufps(x7, x2, 0x1b);                                     // 429dc0 shufps xmm7, xmm2, 0x1b
    x4 = x0;                                                       // 429dc4 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x55);                                     // 429dc7 shufps xmm4, xmm0, 0x55
    x4 = mulps(x4, x2);                                            // 429dcb mulps xmm4, xmm2
    x2 = x0;                                                       // 429dce movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0xaa);                                     // 429dd1 shufps xmm2, xmm0, 0xaa
    x7 = mulps(x7, x2);                                            // 429dd5 mulps xmm7, xmm2
    x2 = x0;                                                       // 429dd8 movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0xff);                                     // 429ddb shufps xmm2, xmm0, 0xff
    x7 = mulps(x7, x5);                                            // 429ddf mulps xmm7, xmm5
    x4 = mulps(x4, x6);                                            // 429de2 mulps xmm4, xmm6
    x2 = mulps(x2, x1);                                            // 429de5 mulps xmm2, xmm1
    ecx = esp + 0xc0;                                              // 429de8 lea ecx, [esp + 0xc0]
    x7 = addps(x7, x4);                                            // 429def addps xmm7, xmm4
    x2 = addps(x2, x3);                                            // 429df2 addps xmm2, xmm3
    x7 = addps(x7, x2);                                            // 429df5 addps xmm7, xmm2
    k.push(ecx);                                                   // 429df8 push ecx
    edx = eax + 0x380;                                             // 429df9 lea edx, [eax + 0x380]
    edi = esp + 0x74;                                              // 429dff lea edi, [esp + 0x74]
    st8(m, esp + 0x13, 0x0u);                                      // 429e03 mov byte ptr [esp + 0x13], 0
    st(m, esp + 0xc4, x7);                                         // 429e08 movaps [esp + 0xc4], xmm7
    k.call(0x00422180);  // FUN_00422180                           // 429e10 call 0x422180
    x4 = ld(m, esp + 0x50);                                        // 429e15 movaps xmm4, [esp + 0x50]
    x2 = ld(m, esp + 0x20);                                        // 429e1a movaps xmm2, [esp + 0x20]
    x1 = ld(m, esp + 0xa0);                                        // 429e1f movaps xmm1, [esp + 0xa0]
    goto L_00429e2e;                                               // 429e27 jmp 0x429e2e
L_00429e29:
    st8(m, esp + 0xf, 0x1u);                                       // 429e29 mov byte ptr [esp + 0xf], 1
L_00429e2e:
    eax = ld32(m, esp + 0x14);                                     // 429e2e mov eax, [esp + 0x14]
    x5 = ld(m, esp + 0x60);                                        // 429e32 movaps xmm5, [esp + 0x60]
    x0 = x2;                                                       // 429e37 movaps xmm0, xmm2
    x0 = shufps(x0, x2, 0x99);                                     // 429e3a shufps xmm0, xmm2, 0x99
    x2 = addss(x2, x0);                                            // 429e3e addss xmm2, xmm0
    x0 = shufps(x0, x0, 0x55);                                     // 429e42 shufps xmm0, xmm0, 0x55
    x2 = addss(x2, x0);                                            // 429e46 addss xmm2, xmm0
    edx = ld32(m, esp + 0x18);                                     // 429e4a mov edx, [esp + 0x18]
    x0 = x2;                                                       // 429e4e movaps xmm0, xmm2
    x0 = shufps(x0, x2, 0x00);                                     // 429e51 shufps xmm0, xmm2, 0
    x0 = mulps(x0, x4);                                            // 429e55 mulps xmm0, xmm4
    x1 = subps(x1, x0);                                            // 429e58 subps xmm1, xmm0
    x0 = cvtss2sd(x0, X::ss(ld32(m, ebp + 0xc)));                  // 429e5b cvtss2sd xmm0, [ebp + 0xc]
    x0 = mulsd(x0, sd(0xc008000000000000ull));                     // 429e60 mulsd xmm0, [0x977a38]
    x0 = cvtsd2ss(x0, x0);                                         // 429e68 cvtsd2ss xmm0, xmm0
    ecx = esi + 0x8;                                               // 429e6c lea ecx, [esi + 8]
    ecx = ecx << 7;                                                // 429e6f shl ecx, 7
    edi = ecx + eax;                                               // 429e72 lea edi, [ecx + eax]
    x0 = shufps(x0, x0, 0x00);                                     // 429e75 shufps xmm0, xmm0, 0
    x1 = mulps(x1, x0);                                            // 429e79 mulps xmm1, xmm0
    x0 = ld(m, ebx + 0x410);                                       // 429e7c movaps xmm0, [ebx + 0x410]
    x1 = addps(x1, x0);                                            // 429e83 addps xmm1, xmm0
    st(m, ebx + 0x410, x1);                                        // 429e86 movaps [ebx + 0x410], xmm1
    x1 = ld(m, edi);                                               // 429e8d movaps xmm1, [edi]
    x3 = X::ss(ld32(m, edx));                                      // 429e90 movss xmm3, [edx]
    x0 = x5;                                                       // 429e94 movaps xmm0, xmm5
    x0 = subps(x0, x1);                                            // 429e97 subps xmm0, xmm1
    x1 = x0;                                                       // 429e9a movaps xmm1, xmm0
    x1 = mulps(x1, x0);                                            // 429e9d mulps xmm1, xmm0
    x2 = x1;                                                       // 429ea0 movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0x66);                                     // 429ea3 shufps xmm2, xmm1, 0x66
    x1 = addss(x1, x2);                                            // 429ea7 addss xmm1, xmm2
    x2 = shufps(x2, x2, 0x55);                                     // 429eab shufps xmm2, xmm2, 0x55
    x1 = addss(x1, x2);                                            // 429eaf addss xmm1, xmm2
    x1 = shufps(x1, x1, 0x00);                                     // 429eb3 shufps xmm1, xmm1, 0
    x1 = sqrtps(x1);                                               // 429eb7 sqrtps xmm1, xmm1
    x0 = mulps(x0, x4);                                            // 429eba mulps xmm0, xmm4
    x3 = shufps(x3, x3, 0x00);                                     // 429ebd shufps xmm3, xmm3, 0
    x2 = x3;                                                       // 429ec1 movaps xmm2, xmm3
    x2 = subps(x2, x1);                                            // 429ec4 subps xmm2, xmm1
    x1 = x0;                                                       // 429ec7 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x99);                                     // 429eca shufps xmm1, xmm0, 0x99
    x0 = addss(x0, x1);                                            // 429ece addss xmm0, xmm1
    x1 = shufps(x1, x1, 0x55);                                     // 429ed2 shufps xmm1, xmm1, 0x55
    x0 = addss(x0, x1);                                            // 429ed6 addss xmm0, xmm1
    x1 = ld(m, 0xd61670);                                          // 429eda movaps xmm1, [0xd61670]
    x0 = shufps(x0, x0, 0x00);                                     // 429ee1 shufps xmm0, xmm0, 0
    x1 = cmpltps(x1, x0);                                          // 429ee5 cmpltps xmm1, xmm0
    st(m, esp + 0x40, x1);                                         // 429ee9 movaps [esp + 0x40], xmm1
    edx = ld32(m, esp + 0x44);                                     // 429eee mov edx, [esp + 0x44]
    ecx = ld32(m, esp + 0x48);                                     // 429ef2 mov ecx, [esp + 0x48]
    ecx = ecx + edx;                                               // 429ef6 add ecx, edx
    ecx = ecx + ld32(m, esp + 0x40);                               // 429ef8 add ecx, [esp + 0x40]
    fl = f_cmp(ecx, 0xfffffffdu);                                  // 429efc cmp ecx, -3
    if (fl.ne()) goto L_00429f0a;                                  // 429eff jne 0x429f0a
    x0 = x2;                                                       // 429f01 movaps xmm0, xmm2
    x0 = addps(x0, x3);                                            // 429f04 addps xmm0, xmm3
    x2 = x0;                                                       // 429f07 movaps xmm2, xmm0
L_00429f0a:
    x1 = ld(m, ebx + 0x430);                                       // 429f0a movaps xmm1, [ebx + 0x430]
    x0 = X::ss(0x43960000u);                                       // 429f11 movss xmm0, [0x935d28]
    x0 = shufps(x0, x0, 0x00);                                     // 429f19 shufps xmm0, xmm0, 0
    x0 = divps(x0, x1);                                            // 429f1d divps xmm0, xmm1
    x1 = ld(m, ebx + 0x450);                                       // 429f20 movaps xmm1, [ebx + 0x450]
    x2 = mulps(x2, x4);                                            // 429f27 mulps xmm2, xmm4
    x0 = mulps(x0, x2);                                            // 429f2a mulps xmm0, xmm2
    x1 = addps(x1, x0);                                            // 429f2d addps xmm1, xmm0
    st(m, ebx + 0x450, x1);                                        // 429f30 movaps [ebx + 0x450], xmm1
    x2 = ld(m, edi);                                               // 429f37 movaps xmm2, [edi]
    st(m, esp + 0x90, x0);                                         // 429f3a movaps [esp + 0x90], xmm0
    x0 = shufps(x0, x0, 0xd2);                                     // 429f42 shufps xmm0, xmm0, 0xd2
    x1 = x5;                                                       // 429f46 movaps xmm1, xmm5
    x1 = subps(x1, x2);                                            // 429f49 subps xmm1, xmm2
    x1 = shufps(x1, x1, 0xc9);                                     // 429f4c shufps xmm1, xmm1, 0xc9
    x2 = x0;                                                       // 429f50 movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0xd2);                                     // 429f53 shufps xmm2, xmm0, 0xd2
    edx = eax + 0x380;                                             // 429f57 lea edx, [eax + 0x380]
    x3 = x1;                                                       // 429f5d movaps xmm3, xmm1
    x3 = shufps(x3, x1, 0xc9);                                     // 429f60 shufps xmm3, xmm1, 0xc9
    x0 = mulps(x0, x1);                                            // 429f64 mulps xmm0, xmm1
    x1 = X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000);     // 429f67 movaps xmm1, [0x92db80]
    eax = esp + 0x80;                                              // 429f6e lea eax, [esp + 0x80]
    x2 = mulps(x2, x3);                                            // 429f75 mulps xmm2, xmm3
    k.push(eax);                                                   // 429f78 push eax
    x0 = subps(x0, x2);                                            // 429f79 subps xmm0, xmm2
    x0 = andps(x0, x1);                                            // 429f7c andps xmm0, xmm1
    k.push(esi);                                                   // 429f7f push esi
    ecx = edx;                                                     // 429f80 mov ecx, edx
    st(m, esp + 0x88, x0);                                         // 429f82 movaps [esp + 0x88], xmm0
    k.call(0x00422700);  // FUN_00422700                           // 429f8a call 0x422700
    fl = f_cmp8(ld8(m, esp + 0xf), 0x0u);                          // 429f8f cmp byte ptr [esp + 0xf], 0
    x1 = ld(m, esp + 0x20);                                        // 429f94 movaps xmm1, [esp + 0x20]
    x4 = ld(m, esp + 0x50);                                        // 429f99 movaps xmm4, [esp + 0x50]
    x0 = x1;                                                       // 429f9e movaps xmm0, xmm1
    x0 = shufps(x0, x1, 0x99);                                     // 429fa1 shufps xmm0, xmm1, 0x99
    x1 = addss(x1, x0);                                            // 429fa5 addss xmm1, xmm0
    x0 = shufps(x0, x0, 0x55);                                     // 429fa9 shufps xmm0, xmm0, 0x55
    x1 = addss(x1, x0);                                            // 429fad addss xmm1, xmm0
    x0 = x1;                                                       // 429fb1 movaps xmm0, xmm1
    x0 = shufps(x0, x0, 0x00);                                     // 429fb4 shufps xmm0, xmm0, 0
    x0 = mulps(x0, x4);                                            // 429fb8 mulps xmm0, xmm4
    if (fl.e()) goto L_0042a090;                                   // 429fbb je 0x42a090
    x1 = x0;                                                       // 429fc1 movaps xmm1, xmm0
    x1 = mulps(x1, ld(m, esp + 0x90));                             // 429fc4 mulps xmm1, [esp + 0x90]
    x2 = x1;                                                       // 429fcc movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0x99);                                     // 429fcf shufps xmm2, xmm1, 0x99
    x1 = addss(x1, x2);                                            // 429fd3 addss xmm1, xmm2
    x2 = shufps(x2, x2, 0x55);                                     // 429fd7 shufps xmm2, xmm2, 0x55
    x1 = addss(x1, x2);                                            // 429fdb addss xmm1, xmm2
    x2 = ld(m, 0xd61670);                                          // 429fdf movaps xmm2, [0xd61670]
    x1 = shufps(x1, x1, 0x00);                                     // 429fe6 shufps xmm1, xmm1, 0
    x2 = cmpltps(x2, x1);                                          // 429fea cmpltps xmm2, xmm1
    st(m, esp + 0x40, x2);                                         // 429fee movaps [esp + 0x40], xmm2
    ecx = ld32(m, esp + 0x44);                                     // 429ff3 mov ecx, [esp + 0x44]
    eax = ld32(m, esp + 0x48);                                     // 429ff7 mov eax, [esp + 0x48]
    eax = eax + ecx;                                               // 429ffb add eax, ecx
    eax = eax + ld32(m, esp + 0x40);                               // 429ffd add eax, [esp + 0x40]
    fl = f_cmp(eax, 0xfffffffdu);                                  // 42a001 cmp eax, -3
    if (fl.ne()) goto L_0042a090;                                  // 42a004 jne 0x42a090
    x2 = ld(m, ebx + 0x430);                                       // 42a00a movaps xmm2, [ebx + 0x430]
    x1 = X::ss(ld32(m, 0x9cc37c));                                 // 42a011 movss xmm1, [0x9cc37c]
    x1 = shufps(x1, x1, 0x00);                                     // 42a019 shufps xmm1, xmm1, 0
    x1 = divps(x1, x2);                                            // 42a01d divps xmm1, xmm2
    x2 = ld(m, esp + 0x30);                                        // 42a020 movaps xmm2, [esp + 0x30]
    x2 = subps(x2, x1);                                            // 42a025 subps xmm2, xmm1
    x1 = ld(m, ebx + 0x450);                                       // 42a028 movaps xmm1, [ebx + 0x450]
    x2 = mulps(x2, x0);                                            // 42a02f mulps xmm2, xmm0
    x0 = x2;                                                       // 42a032 movaps xmm0, xmm2
    x1 = addps(x1, x0);                                            // 42a035 addps xmm1, xmm0
    st(m, ebx + 0x450, x1);                                        // 42a038 movaps [ebx + 0x450], xmm1
    x2 = ld(m, edi);                                               // 42a03f movaps xmm2, [edi]
    x1 = ld(m, esp + 0x60);                                        // 42a042 movaps xmm1, [esp + 0x60]
    x1 = subps(x1, x2);                                            // 42a047 subps xmm1, xmm2
    x0 = shufps(x0, x0, 0xd2);                                     // 42a04a shufps xmm0, xmm0, 0xd2
    x1 = shufps(x1, x1, 0xc9);                                     // 42a04e shufps xmm1, xmm1, 0xc9
    x2 = x0;                                                       // 42a052 movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0xd2);                                     // 42a055 shufps xmm2, xmm0, 0xd2
    x3 = x1;                                                       // 42a059 movaps xmm3, xmm1
    x3 = shufps(x3, x1, 0xc9);                                     // 42a05c shufps xmm3, xmm1, 0xc9
    x0 = mulps(x0, x1);                                            // 42a060 mulps xmm0, xmm1
    x1 = X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000);     // 42a063 movaps xmm1, [0x92db80]
    ecx = esp + 0xb0;                                              // 42a06a lea ecx, [esp + 0xb0]
    k.push(ecx);                                                   // 42a071 push ecx
    x2 = mulps(x2, x3);                                            // 42a072 mulps xmm2, xmm3
    x0 = subps(x0, x2);                                            // 42a075 subps xmm0, xmm2
    x0 = andps(x0, x1);                                            // 42a078 andps xmm0, xmm1
    k.push(esi);                                                   // 42a07b push esi
    ecx = edx;                                                     // 42a07c mov ecx, edx
    st(m, esp + 0xb8, x0);                                         // 42a07e movaps [esp + 0xb8], xmm0
    k.call(0x00422700);  // FUN_00422700                           // 42a086 call 0x422700
    x4 = ld(m, esp + 0x50);                                        // 42a08b movaps xmm4, [esp + 0x50]
L_0042a090:
    ecx = ld32(m, esp + 0x1c);                                     // 42a090 mov ecx, [esp + 0x1c]
    eax = ld32(m, esp + 0x10);                                     // 42a094 mov eax, [esp + 0x10]
    st32(m, esp + 0x18, ld32(m, esp + 0x18) + 0x4u);               // 42a098 add [esp + 0x18], 4
    ecx = ecx + 1u;                                                // 42a09d inc ecx
    eax = eax + 0x8u;                                              // 42a09e add eax, 8
    fl = f_cmp(ecx, 0x10u);                                        // 42a0a1 cmp ecx, 0x10
    st32(m, esp + 0x1c, ecx);                                      // 42a0a4 mov [esp + 0x1c], ecx
    st32(m, esp + 0x10, eax);                                      // 42a0a8 mov [esp + 0x10], eax
    if (fl.l()) goto L_00429b58;                                   // 42a0ac jl 0x429b58
    ecx = ld32(m, esp + 0xec);                                     // 42a0b2 mov ecx, [esp + 0xec]
    edi = k.pop();                                                 // 42a0b9 pop edi
    esi = k.pop();                                                 // 42a0ba pop esi
    ebx = k.pop();                                                 // 42a0bb pop ebx
    ecx = ecx ^ esp;                                               // 42a0bc xor ecx, esp
    k.call(0x008f9efe);  // __security_check_cookie                // 42a0be call 0x8f9efe
    esp = ebp;                                                     // 42a0c3 mov esp, ebp
    ebp = k.pop();                                                 // 42a0c5 pop ebp
    return k.ret(8);                                               // 42a0c6 ret 8
}

// 0042a240 FUN_0042a240: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x0042a240, Fn_0042a240, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebx);                                                   // 42a240 push ebx
    ebx = ld32(m, esp + 0x8);                                      // 42a241 mov ebx, [esp + 8]
    k.push(esi);                                                   // 42a245 push esi
    k.push(edi);                                                   // 42a246 push edi
    ebx = ebx + 0xcu;                                              // 42a247 add ebx, 0xc
    st32(m, esp + 0x10, 0x10u);                                    // 42a24a mov [esp + 0x10], 0x10
L_0042a252:
    eax = ld32(m, ebx);                                            // 42a252 mov eax, [ebx]
    fl = f_test(eax, eax);                                         // 42a254 test eax, eax
    if (fl.e()) goto L_0042a2f3;                                   // 42a256 je 0x42a2f3
    esi = ld32(m, eax + 0x4);                                      // 42a25c mov esi, [eax + 4]
    edi = ld32(m, eax + 0x8);                                      // 42a25f mov edi, [eax + 8]
    ecx = ld32(m, esi + 0x1964);                                   // 42a262 mov ecx, [esi + 0x1964]
    ecx = ecx - ld32(m, esi + 0x1960);                             // 42a268 sub ecx, [esi + 0x1960]
    esi = esi + 0x1954u;                                           // 42a26e add esi, 0x1954
    eax = 0x92492493u;                                             // 42a274 mov eax, 0x92492493
    { int64_t p_ = int64_t(int32_t(eax)) * int32_t(ecx); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); } // 42a279 imul ecx
    edx = edx + ecx;                                               // 42a27b add edx, ecx
    edx = uint32_t(int32_t(edx) >> 7);                             // 42a27d sar edx, 7
    eax = edx;                                                     // 42a280 mov eax, edx
    eax = eax >> 31;                                               // 42a282 shr eax, 0x1f
    eax = eax + edx;                                               // 42a285 add eax, edx
    fl = f_cmp(edi, eax);                                          // 42a287 cmp edi, eax
    if (fl.b()) goto L_0042a291;                                   // 42a289 jb 0x42a291
    k.call(ld32(m, 0x916224));                                     // 42a28b call [0x916224]
L_0042a291:
    ecx = ld32(m, esi + 0xc);                                      // 42a291 mov ecx, [esi + 0xc]
    edi = edi * 0xe0u;                                             // 42a294 imul edi, edi, 0xe0
    edx = ld32(m, edi + ecx + 0x2c);                               // 42a29a mov edx, [edi + ecx + 0x2c]
    st32(m, edx + 0x14, 0x0u);                                     // 42a29e mov [edx + 0x14], 0
    eax = ld32(m, ebx);                                            // 42a2a5 mov eax, [ebx]
    fl = f_test(eax, eax);                                         // 42a2a7 test eax, eax
    if (fl.e()) goto L_0042a2f3;                                   // 42a2a9 je 0x42a2f3
    esi = ld32(m, eax + 0x4);                                      // 42a2ab mov esi, [eax + 4]
    edi = ld32(m, eax + 0x8);                                      // 42a2ae mov edi, [eax + 8]
    ecx = ld32(m, esi + 0x1964);                                   // 42a2b1 mov ecx, [esi + 0x1964]
    ecx = ecx - ld32(m, esi + 0x1960);                             // 42a2b7 sub ecx, [esi + 0x1960]
    esi = esi + 0x1954u;                                           // 42a2bd add esi, 0x1954
    eax = 0x92492493u;                                             // 42a2c3 mov eax, 0x92492493
    { int64_t p_ = int64_t(int32_t(eax)) * int32_t(ecx); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); } // 42a2c8 imul ecx
    edx = edx + ecx;                                               // 42a2ca add edx, ecx
    edx = uint32_t(int32_t(edx) >> 7);                             // 42a2cc sar edx, 7
    eax = edx;                                                     // 42a2cf mov eax, edx
    eax = eax >> 31;                                               // 42a2d1 shr eax, 0x1f
    eax = eax + edx;                                               // 42a2d4 add eax, edx
    fl = f_cmp(edi, eax);                                          // 42a2d6 cmp edi, eax
    if (fl.b()) goto L_0042a2e0;                                   // 42a2d8 jb 0x42a2e0
    k.call(ld32(m, 0x916224));                                     // 42a2da call [0x916224]
L_0042a2e0:
    ecx = ld32(m, esi + 0xc);                                      // 42a2e0 mov ecx, [esi + 0xc]
    edi = edi * 0xe0u;                                             // 42a2e3 imul edi, edi, 0xe0
    edi = ld32(m, edi + ecx + 0x2c);                               // 42a2e9 mov edi, [edi + ecx + 0x2c]
    edi = edi + 0x14u;                                             // 42a2ed add edi, 0x14
    st32(m, edi, ld32(m, edi) | 0x8u);                             // 42a2f0 or [edi], 8
L_0042a2f3:
    ebx = ebx + 0x8u;                                              // 42a2f3 add ebx, 8
    { uint32_t a_ = ld32(m, esp + 0x10), b_ = 0x1u, r_ = a_ - b_; fl = f_sub(a_, b_, r_); st32(m, esp + 0x10, r_); } // 42a2f6 sub [esp + 0x10], 1
    if (fl.ne()) goto L_0042a252;                                  // 42a2fb jne 0x42a252
    edi = k.pop();                                                 // 42a301 pop edi
    esi = k.pop();                                                 // 42a302 pop esi
    ebx = k.pop();                                                 // 42a303 pop ebx
    return k.ret(4);                                               // 42a304 ret 4
}

// 0042a310 FUN_0042a310: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x0042a310, Fn_0042a310, "math", 0) {  // outputs: what callers read (it calls other groups' code)
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 42a310 push ebp
    ebp = esp;                                                     // 42a311 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 42a313 and esp, 0xfffffff0
    esp = esp - 0x34u;                                             // 42a316 sub esp, 0x34
    eax = ld32(m, 0x9b45e4);                                       // 42a319 mov eax, [0x9b45e4]
    eax = eax ^ esp;                                               // 42a31e xor eax, esp
    st32(m, esp + 0x30, eax);                                      // 42a320 mov [esp + 0x30], eax
    x7 = ld(m, 0xd616a0);                                          // 42a324 movaps xmm7, [0xd616a0]
    k.push(ebx);                                                   // 42a32b push ebx
    k.push(esi);                                                   // 42a32c push esi
    esi = ecx;                                                     // 42a32d mov esi, ecx
    x0 = X::ss(ld32(m, esi + 0xa7b8));                             // 42a32f movss xmm0, [esi + 0xa7b8]
    x5 = ld(m, esi + 0x400);                                       // 42a337 movaps xmm5, [esi + 0x400]
    x0 = shufps(x0, x0, 0x00);                                     // 42a33e shufps xmm0, xmm0, 0
    k.push(edi);                                                   // 42a342 push edi
    x6 = x5;                                                       // 42a343 movaps xmm6, xmm5
    st(m, esp + 0x20, x0);                                         // 42a346 movaps [esp + 0x20], xmm0
    eax = esi + 0xa768;                                            // 42a34b lea eax, [esi + 0xa768]
    ecx = 0xfu;                                                    // 42a351 mov ecx, 0xf
L_0042a356:
    edx = ld32(m, eax);                                            // 42a356 mov edx, [eax]
    x0 = x7;                                                       // 42a358 movaps xmm0, xmm7
    x0 = mulps(x0, x5);                                            // 42a35b mulps xmm0, xmm5
    x1 = x0;                                                       // 42a35e movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x99);                                     // 42a361 shufps xmm1, xmm0, 0x99
    x0 = addss(x0, x1);                                            // 42a365 addss xmm0, xmm1
    x1 = shufps(x1, x1, 0x55);                                     // 42a369 shufps xmm1, xmm1, 0x55
    x0 = addss(x0, x1);                                            // 42a36d addss xmm0, xmm1
    edx = edx + 0x8u;                                              // 42a371 add edx, 8
    edx = edx << 7;                                                // 42a374 shl edx, 7
    x3 = ld(m, edx + esi);                                         // 42a377 movaps xmm3, [edx + esi]
    x4 = x0;                                                       // 42a37b movaps xmm4, xmm0
    x0 = x3;                                                       // 42a37e movaps xmm0, xmm3
    x0 = mulps(x0, x7);                                            // 42a381 mulps xmm0, xmm7
    x1 = x0;                                                       // 42a384 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x99);                                     // 42a387 shufps xmm1, xmm0, 0x99
    x2 = x0;                                                       // 42a38b movaps xmm2, xmm0
    x2 = addss(x2, x1);                                            // 42a38e addss xmm2, xmm1
    x1 = shufps(x1, x1, 0x55);                                     // 42a392 shufps xmm1, xmm1, 0x55
    x2 = addss(x2, x1);                                            // 42a396 addss xmm2, xmm1
    x1 = x4;                                                       // 42a39a movaps xmm1, xmm4
    x2 = shufps(x2, x2, 0x00);                                     // 42a39d shufps xmm2, xmm2, 0
    x1 = shufps(x1, x4, 0x00);                                     // 42a3a1 shufps xmm1, xmm4, 0
    x1 = cmpltps(x1, x2);                                          // 42a3a5 cmpltps xmm1, xmm2
    st(m, esp + 0x10, x1);                                         // 42a3a9 movaps [esp + 0x10], xmm1
    edx = ld32(m, esp + 0x14);                                     // 42a3ae mov edx, [esp + 0x14]
    edi = ld32(m, esp + 0x18);                                     // 42a3b2 mov edi, [esp + 0x18]
    edi = edi + edx;                                               // 42a3b6 add edi, edx
    edi = edi + ld32(m, esp + 0x10);                               // 42a3b8 add edi, [esp + 0x10]
    fl = f_cmp(edi, 0xfffffffdu);                                  // 42a3bc cmp edi, -3
    if (fl.ne()) goto L_0042a3cf;                                  // 42a3bf jne 0x42a3cf
    x1 = x3;                                                       // 42a3c1 movaps xmm1, xmm3
    x1 = shufps(x1, x5, 0xe5);                                     // 42a3c4 shufps xmm1, xmm5, 0xe5
    x1 = movss(x1, x5);                                            // 42a3c8 movss xmm1, xmm5
    x5 = x1;                                                       // 42a3cc movaps xmm5, xmm1
L_0042a3cf:
    x1 = x7;                                                       // 42a3cf movaps xmm1, xmm7
    x1 = mulps(x1, x6);                                            // 42a3d2 mulps xmm1, xmm6
    x2 = x1;                                                       // 42a3d5 movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0x99);                                     // 42a3d8 shufps xmm2, xmm1, 0x99
    x1 = addss(x1, x2);                                            // 42a3dc addss xmm1, xmm2
    x2 = shufps(x2, x2, 0x55);                                     // 42a3e0 shufps xmm2, xmm2, 0x55
    x1 = addss(x1, x2);                                            // 42a3e4 addss xmm1, xmm2
    x2 = x1;                                                       // 42a3e8 movaps xmm2, xmm1
    x1 = x0;                                                       // 42a3eb movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x99);                                     // 42a3ee shufps xmm1, xmm0, 0x99
    x0 = addss(x0, x1);                                            // 42a3f2 addss xmm0, xmm1
    x1 = shufps(x1, x1, 0x55);                                     // 42a3f6 shufps xmm1, xmm1, 0x55
    x0 = addss(x0, x1);                                            // 42a3fa addss xmm0, xmm1
    x1 = x0;                                                       // 42a3fe movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x00);                                     // 42a401 shufps xmm1, xmm0, 0
    x2 = shufps(x2, x2, 0x00);                                     // 42a405 shufps xmm2, xmm2, 0
    x1 = cmpltps(x1, x2);                                          // 42a409 cmpltps xmm1, xmm2
    st(m, esp + 0x10, x1);                                         // 42a40d movaps [esp + 0x10], xmm1
    edx = ld32(m, esp + 0x14);                                     // 42a412 mov edx, [esp + 0x14]
    edi = ld32(m, esp + 0x18);                                     // 42a416 mov edi, [esp + 0x18]
    edi = edi + edx;                                               // 42a41a add edi, edx
    edi = edi + ld32(m, esp + 0x10);                               // 42a41c add edi, [esp + 0x10]
    fl = f_cmp(edi, 0xfffffffdu);                                  // 42a420 cmp edi, -3
    if (fl.ne()) goto L_0042a433;                                  // 42a423 jne 0x42a433
    x0 = x3;                                                       // 42a425 movaps xmm0, xmm3
    x0 = shufps(x0, x6, 0xe5);                                     // 42a428 shufps xmm0, xmm6, 0xe5
    x0 = movss(x0, x6);                                            // 42a42c movss xmm0, xmm6
    x6 = x0;                                                       // 42a430 movaps xmm6, xmm0
L_0042a433:
    x2 = ld(m, 0xd615c0);                                          // 42a433 movaps xmm2, [0xd615c0]
    x0 = x2;                                                       // 42a43a movaps xmm0, xmm2
    x0 = mulps(x0, x5);                                            // 42a43d mulps xmm0, xmm5
    x1 = x0;                                                       // 42a440 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x99);                                     // 42a443 shufps xmm1, xmm0, 0x99
    x0 = addss(x0, x1);                                            // 42a447 addss xmm0, xmm1
    x1 = shufps(x1, x1, 0x55);                                     // 42a44b shufps xmm1, xmm1, 0x55
    x0 = addss(x0, x1);                                            // 42a44f addss xmm0, xmm1
    x4 = x0;                                                       // 42a453 movaps xmm4, xmm0
    x0 = x3;                                                       // 42a456 movaps xmm0, xmm3
    x0 = mulps(x0, x2);                                            // 42a459 mulps xmm0, xmm2
    x1 = x0;                                                       // 42a45c movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x99);                                     // 42a45f shufps xmm1, xmm0, 0x99
    x2 = x0;                                                       // 42a463 movaps xmm2, xmm0
    x2 = addss(x2, x1);                                            // 42a466 addss xmm2, xmm1
    x1 = shufps(x1, x1, 0x55);                                     // 42a46a shufps xmm1, xmm1, 0x55
    x2 = addss(x2, x1);                                            // 42a46e addss xmm2, xmm1
    x1 = x4;                                                       // 42a472 movaps xmm1, xmm4
    x2 = shufps(x2, x2, 0x00);                                     // 42a475 shufps xmm2, xmm2, 0
    x1 = shufps(x1, x4, 0x00);                                     // 42a479 shufps xmm1, xmm4, 0
    x1 = cmpltps(x1, x2);                                          // 42a47d cmpltps xmm1, xmm2
    st(m, esp + 0x10, x1);                                         // 42a481 movaps [esp + 0x10], xmm1
    edx = ld32(m, esp + 0x14);                                     // 42a486 mov edx, [esp + 0x14]
    edi = ld32(m, esp + 0x18);                                     // 42a48a mov edi, [esp + 0x18]
    edi = edi + edx;                                               // 42a48e add edi, edx
    edi = edi + ld32(m, esp + 0x10);                               // 42a490 add edi, [esp + 0x10]
    fl = f_cmp(edi, 0xfffffffdu);                                  // 42a494 cmp edi, -3
    if (fl.ne()) goto L_0042a4a1;                                  // 42a497 jne 0x42a4a1
    x1 = movss(x1, x3);                                            // 42a499 movss xmm1, xmm3
    x5 = movss(x5, x1);                                            // 42a49d movss xmm5, xmm1
L_0042a4a1:
    x1 = ld(m, 0xd615c0);                                          // 42a4a1 movaps xmm1, [0xd615c0]
    x1 = mulps(x1, x6);                                            // 42a4a8 mulps xmm1, xmm6
    x2 = x1;                                                       // 42a4ab movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0x99);                                     // 42a4ae shufps xmm2, xmm1, 0x99
    x1 = addss(x1, x2);                                            // 42a4b2 addss xmm1, xmm2
    x2 = shufps(x2, x2, 0x55);                                     // 42a4b6 shufps xmm2, xmm2, 0x55
    x1 = addss(x1, x2);                                            // 42a4ba addss xmm1, xmm2
    x2 = x1;                                                       // 42a4be movaps xmm2, xmm1
    x1 = x0;                                                       // 42a4c1 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x99);                                     // 42a4c4 shufps xmm1, xmm0, 0x99
    x0 = addss(x0, x1);                                            // 42a4c8 addss xmm0, xmm1
    x1 = shufps(x1, x1, 0x55);                                     // 42a4cc shufps xmm1, xmm1, 0x55
    x0 = addss(x0, x1);                                            // 42a4d0 addss xmm0, xmm1
    x1 = x0;                                                       // 42a4d4 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x00);                                     // 42a4d7 shufps xmm1, xmm0, 0
    x2 = shufps(x2, x2, 0x00);                                     // 42a4db shufps xmm2, xmm2, 0
    x1 = cmpltps(x1, x2);                                          // 42a4df cmpltps xmm1, xmm2
    st(m, esp + 0x10, x1);                                         // 42a4e3 movaps [esp + 0x10], xmm1
    edx = ld32(m, esp + 0x14);                                     // 42a4e8 mov edx, [esp + 0x14]
    edi = ld32(m, esp + 0x18);                                     // 42a4ec mov edi, [esp + 0x18]
    edi = edi + edx;                                               // 42a4f0 add edi, edx
    edi = edi + ld32(m, esp + 0x10);                               // 42a4f2 add edi, [esp + 0x10]
    fl = f_cmp(edi, 0xfffffffdu);                                  // 42a4f6 cmp edi, -3
    if (fl.ne()) goto L_0042a503;                                  // 42a4f9 jne 0x42a503
    x0 = movss(x0, x3);                                            // 42a4fb movss xmm0, xmm3
    x6 = movss(x6, x0);                                            // 42a4ff movss xmm6, xmm0
L_0042a503:
    x2 = ld(m, 0xd61680);                                          // 42a503 movaps xmm2, [0xd61680]
    x0 = x2;                                                       // 42a50a movaps xmm0, xmm2
    x0 = mulps(x0, x5);                                            // 42a50d mulps xmm0, xmm5
    x1 = x0;                                                       // 42a510 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x99);                                     // 42a513 shufps xmm1, xmm0, 0x99
    x0 = addss(x0, x1);                                            // 42a517 addss xmm0, xmm1
    x1 = shufps(x1, x1, 0x55);                                     // 42a51b shufps xmm1, xmm1, 0x55
    x0 = addss(x0, x1);                                            // 42a51f addss xmm0, xmm1
    x4 = x0;                                                       // 42a523 movaps xmm4, xmm0
    x0 = x3;                                                       // 42a526 movaps xmm0, xmm3
    x0 = mulps(x0, x2);                                            // 42a529 mulps xmm0, xmm2
    x1 = x0;                                                       // 42a52c movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x99);                                     // 42a52f shufps xmm1, xmm0, 0x99
    x2 = x0;                                                       // 42a533 movaps xmm2, xmm0
    x2 = addss(x2, x1);                                            // 42a536 addss xmm2, xmm1
    x1 = shufps(x1, x1, 0x55);                                     // 42a53a shufps xmm1, xmm1, 0x55
    x2 = addss(x2, x1);                                            // 42a53e addss xmm2, xmm1
    x1 = x4;                                                       // 42a542 movaps xmm1, xmm4
    x2 = shufps(x2, x2, 0x00);                                     // 42a545 shufps xmm2, xmm2, 0
    x1 = shufps(x1, x4, 0x00);                                     // 42a549 shufps xmm1, xmm4, 0
    x1 = cmpltps(x1, x2);                                          // 42a54d cmpltps xmm1, xmm2
    st(m, esp + 0x10, x1);                                         // 42a551 movaps [esp + 0x10], xmm1
    edx = ld32(m, esp + 0x14);                                     // 42a556 mov edx, [esp + 0x14]
    edi = ld32(m, esp + 0x18);                                     // 42a55a mov edi, [esp + 0x18]
    edi = edi + edx;                                               // 42a55e add edi, edx
    edi = edi + ld32(m, esp + 0x10);                               // 42a560 add edi, [esp + 0x10]
    fl = f_cmp(edi, 0xfffffffdu);                                  // 42a564 cmp edi, -3
    if (fl.ne()) goto L_0042a56d;                                  // 42a567 jne 0x42a56d
    x5 = shufps(x5, x3, 0xa4);                                     // 42a569 shufps xmm5, xmm3, 0xa4
L_0042a56d:
    x1 = ld(m, 0xd61680);                                          // 42a56d movaps xmm1, [0xd61680]
    x1 = mulps(x1, x6);                                            // 42a574 mulps xmm1, xmm6
    x2 = x1;                                                       // 42a577 movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0x99);                                     // 42a57a shufps xmm2, xmm1, 0x99
    x1 = addss(x1, x2);                                            // 42a57e addss xmm1, xmm2
    x2 = shufps(x2, x2, 0x55);                                     // 42a582 shufps xmm2, xmm2, 0x55
    x1 = addss(x1, x2);                                            // 42a586 addss xmm1, xmm2
    x2 = x1;                                                       // 42a58a movaps xmm2, xmm1
    x1 = x0;                                                       // 42a58d movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x99);                                     // 42a590 shufps xmm1, xmm0, 0x99
    x0 = addss(x0, x1);                                            // 42a594 addss xmm0, xmm1
    x1 = shufps(x1, x1, 0x55);                                     // 42a598 shufps xmm1, xmm1, 0x55
    x0 = addss(x0, x1);                                            // 42a59c addss xmm0, xmm1
    x1 = x0;                                                       // 42a5a0 movaps xmm1, xmm0
    x2 = shufps(x2, x2, 0x00);                                     // 42a5a3 shufps xmm2, xmm2, 0
    x1 = shufps(x1, x0, 0x00);                                     // 42a5a7 shufps xmm1, xmm0, 0
    x1 = cmpltps(x1, x2);                                          // 42a5ab cmpltps xmm1, xmm2
    st(m, esp + 0x10, x1);                                         // 42a5af movaps [esp + 0x10], xmm1
    edx = ld32(m, esp + 0x14);                                     // 42a5b4 mov edx, [esp + 0x14]
    edi = ld32(m, esp + 0x18);                                     // 42a5b8 mov edi, [esp + 0x18]
    edi = edi + edx;                                               // 42a5bc add edi, edx
    edi = edi + ld32(m, esp + 0x10);                               // 42a5be add edi, [esp + 0x10]
    fl = f_cmp(edi, 0xfffffffdu);                                  // 42a5c2 cmp edi, -3
    if (fl.ne()) goto L_0042a5cb;                                  // 42a5c5 jne 0x42a5cb
    x6 = shufps(x6, x3, 0xa4);                                     // 42a5c7 shufps xmm6, xmm3, 0xa4
L_0042a5cb:
    eax = eax + 0x4u;                                              // 42a5cb add eax, 4
    { uint32_t a_ = ecx, b_ = 0x1u, r_ = a_ - b_; fl = f_sub(a_, b_, r_); ecx = r_; } // 42a5ce sub ecx, 1
    if (fl.ne()) goto L_0042a356;                                  // 42a5d1 jne 0x42a356
    x0 = ld(m, esp + 0x20);                                        // 42a5d7 movaps xmm0, [esp + 0x20]
    eax = ld32(m, 0xa95844);                                       // 42a5dc mov eax, [0xa95844]
    edx = ld32(m, eax + 0x20);                                     // 42a5e1 mov edx, [eax + 0x20]
    ecx = 0x1u;                                                    // 42a5e4 mov ecx, 1
    x1 = x0;                                                       // 42a5e9 movaps xmm1, xmm0
    x1 = addps(x1, x5);                                            // 42a5ec addps xmm1, xmm5
    x6 = subps(x6, x0);                                            // 42a5ef subps xmm6, xmm0
    st(m, esp + 0x10, x1);                                         // 42a5f2 movaps [esp + 0x10], xmm1
    st(m, esp + 0x20, x6);                                         // 42a5f7 movaps [esp + 0x20], xmm6
    fl = f_cmp(ld32(m, edx + 0x34), ecx);                          // 42a5fc cmp [edx + 0x34], ecx
    if (fl.ne()) goto L_0042a60e;                                  // 42a5ff jne 0x42a60e
    eax = ld32(m, 0xa946b8);                                       // 42a601 mov eax, [0xa946b8]
    eax = ld32(m, eax + 0x5a54);                                   // 42a606 mov eax, [eax + 0x5a54]
    goto L_0042a625;                                               // 42a60c jmp 0x42a625
L_0042a60e:
    edx = ld32(m, eax + 0x1c);                                     // 42a60e mov edx, [eax + 0x1c]
    fl = f_cmp(ld32(m, edx + 0x34), ecx);                          // 42a611 cmp [edx + 0x34], ecx
    if (fl.ne()) goto L_0042a623;                                  // 42a614 jne 0x42a623
    eax = ld32(m, 0xa946b8);                                       // 42a616 mov eax, [0xa946b8]
    eax = ld32(m, eax + 0x20c8);                                   // 42a61b mov eax, [eax + 0x20c8]
    goto L_0042a625;                                               // 42a621 jmp 0x42a625
L_0042a623:
    eax = 0u;                                                      // 42a623 xor eax, eax
L_0042a625:
    edi = ld32(m, eax + 0x20134);                                  // 42a625 mov edi, [eax + 0x20134]
    edx = esp + 0x20;                                              // 42a62b lea edx, [esp + 0x20]
    eax = esp + 0x10;                                              // 42a62f lea eax, [esp + 0x10]
    k.call(0x00773110);  // FUN_00773110                           // 42a633 call 0x773110
    fl = f_test8((eax & 0xff), (eax & 0xff));                      // 42a638 test al, al
    if (fl.e()) goto L_0042a6a8;                                   // 42a63a je 0x42a6a8
    st8(m, esi + 0x94, 0x0u);                                      // 42a63c mov byte ptr [esi + 0x94], 0
    ebx = esi + 0xc;                                               // 42a643 lea ebx, [esi + 0xc]
    st32(m, esp + 0xc, 0x10u);                                     // 42a646 mov [esp + 0xc], 0x10
    edi = edi;                                                     // 42a64e mov edi, edi
L_0042a650:
    eax = ld32(m, ebx);                                            // 42a650 mov eax, [ebx]
    fl = f_test(eax, eax);                                         // 42a652 test eax, eax
    if (fl.e()) goto L_0042a69e;                                   // 42a654 je 0x42a69e
    esi = ld32(m, eax + 0x4);                                      // 42a656 mov esi, [eax + 4]
    edi = ld32(m, eax + 0x8);                                      // 42a659 mov edi, [eax + 8]
    ecx = ld32(m, esi + 0x1964);                                   // 42a65c mov ecx, [esi + 0x1964]
    ecx = ecx - ld32(m, esi + 0x1960);                             // 42a662 sub ecx, [esi + 0x1960]
    esi = esi + 0x1954u;                                           // 42a668 add esi, 0x1954
    eax = 0x92492493u;                                             // 42a66e mov eax, 0x92492493
    { int64_t p_ = int64_t(int32_t(eax)) * int32_t(ecx); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); } // 42a673 imul ecx
    edx = edx + ecx;                                               // 42a675 add edx, ecx
    edx = uint32_t(int32_t(edx) >> 7);                             // 42a677 sar edx, 7
    ecx = edx;                                                     // 42a67a mov ecx, edx
    ecx = ecx >> 31;                                               // 42a67c shr ecx, 0x1f
    ecx = ecx + edx;                                               // 42a67f add ecx, edx
    fl = f_cmp(edi, ecx);                                          // 42a681 cmp edi, ecx
    if (fl.b()) goto L_0042a68b;                                   // 42a683 jb 0x42a68b
    k.call(ld32(m, 0x916224));                                     // 42a685 call [0x916224]
L_0042a68b:
    edx = ld32(m, esi + 0xc);                                      // 42a68b mov edx, [esi + 0xc]
    edi = edi * 0xe0u;                                             // 42a68e imul edi, edi, 0xe0
    edi = ld32(m, edi + edx + 0x2c);                               // 42a694 mov edi, [edi + edx + 0x2c]
    edi = edi + 0x14u;                                             // 42a698 add edi, 0x14
    st32(m, edi, ld32(m, edi) & 0xfffffff7u);                      // 42a69b and [edi], 0xfffffff7
L_0042a69e:
    ebx = ebx + 0x8u;                                              // 42a69e add ebx, 8
    { uint32_t a_ = ld32(m, esp + 0xc), b_ = 0x1u, r_ = a_ - b_; fl = f_sub(a_, b_, r_); st32(m, esp + 0xc, r_); } // 42a6a1 sub [esp + 0xc], 1
    if (fl.ne()) goto L_0042a650;                                  // 42a6a6 jne 0x42a650
L_0042a6a8:
    ecx = ld32(m, esp + 0x3c);                                     // 42a6a8 mov ecx, [esp + 0x3c]
    edi = k.pop();                                                 // 42a6ac pop edi
    esi = k.pop();                                                 // 42a6ad pop esi
    ebx = k.pop();                                                 // 42a6ae pop ebx
    ecx = ecx ^ esp;                                               // 42a6af xor ecx, esp
    k.call(0x008f9efe);  // __security_check_cookie                // 42a6b1 call 0x8f9efe
    esp = ebp;                                                     // 42a6b6 mov esp, ebp
    ebp = k.pop();                                                 // 42a6b8 pop ebp
    return k.ret(0);                                               // 42a6b9 ret 
}

// 0042a6c0 FUN_0042a6c0: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x0042a6c0, Fn_0042a6c0, "math", 0) {  // outputs: what callers read (it calls other groups' code)
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 42a6c0 push ebp
    ebp = esp;                                                     // 42a6c1 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 42a6c3 and esp, 0xfffffff0
    esp = esp - 0x104u;                                            // 42a6c6 sub esp, 0x104
    eax = ld32(m, 0x9b45e4);                                       // 42a6cc mov eax, [0x9b45e4]
    eax = eax ^ esp;                                               // 42a6d1 xor eax, esp
    st32(m, esp + 0x100, eax);                                     // 42a6d3 mov [esp + 0x100], eax
    k.push(ebx);                                                   // 42a6da push ebx
    k.push(esi);                                                   // 42a6db push esi
    k.push(edi);                                                   // 42a6dc push edi
    x0 = X::ss(0x3faaaaabu);                                       // 42a6dd movss xmm0, [0x9776dc]
    x1 = X::ss(0x4200cccdu);                                       // 42a6e5 movss xmm1, [0x977878]
    x0 = shufps(x0, x0, 0x00);                                     // 42a6ed shufps xmm0, xmm0, 0
    st(m, esp + 0x80, x0);                                         // 42a6f1 movaps [esp + 0x80], xmm0
    x0 = X::ss(0x3f2aaaabu);                                       // 42a6f9 movss xmm0, [0x977714]
    x0 = shufps(x0, x0, 0x00);                                     // 42a701 shufps xmm0, xmm0, 0
    st(m, esp + 0xa0, x0);                                         // 42a705 movaps [esp + 0xa0], xmm0
    x0 = X::ss(0x3eaaaaabu);                                       // 42a70d movss xmm0, [0x92e214]
    x0 = shufps(x0, x0, 0x00);                                     // 42a715 shufps xmm0, xmm0, 0
    st(m, esp + 0x90, x0);                                         // 42a719 movaps [esp + 0x90], xmm0
    x0 = X::ss(ld32(m, 0x9cd958));                                 // 42a721 movss xmm0, [0x9cd958]
    x0 = shufps(x0, x0, 0x00);                                     // 42a729 shufps xmm0, xmm0, 0
    st(m, esp + 0x50, x0);                                         // 42a72d movaps [esp + 0x50], xmm0
    x0 = X::ss(ld32(m, 0x9cd970));                                 // 42a732 movss xmm0, [0x9cd970]
    x0 = shufps(x0, x0, 0x00);                                     // 42a73a shufps xmm0, xmm0, 0
    ebx = ld32(m, ebp + 0x8);                                      // 42a73e mov ebx, [ebp + 8]
    st(m, esp + 0x60, x0);                                         // 42a741 movaps [esp + 0x60], xmm0
    x0 = X::ss(0x40490fdbu);                                       // 42a746 movss xmm0, [0x977498]
    x1 = shufps(x1, x1, 0x00);                                     // 42a74e shufps xmm1, xmm1, 0
    x0 = shufps(x0, x0, 0x00);                                     // 42a752 shufps xmm0, xmm0, 0
    x0 = mulps(x0, x1);                                            // 42a756 mulps xmm0, xmm1
    x1 = X::ss(0x3ecccccdu);                                       // 42a759 movss xmm1, [0x935860]
    eax = ebx + 0xa7b8;                                            // 42a761 lea eax, [ebx + 0xa7b8]
    st32(m, esp + 0x10, eax);                                      // 42a767 mov [esp + 0x10], eax
    x1 = shufps(x1, x1, 0x00);                                     // 42a76b shufps xmm1, xmm1, 0
    eax = ebx + 0xc;                                               // 42a76f lea eax, [ebx + 0xc]
    x0 = mulps(x0, x1);                                            // 42a772 mulps xmm0, xmm1
    st32(m, esp + 0x3c, ebx);                                      // 42a775 mov [esp + 0x3c], ebx
    st(m, esp + 0xe0, x0);                                         // 42a779 movaps [esp + 0xe0], xmm0
    st32(m, esp + 0x14, eax);                                      // 42a781 mov [esp + 0x14], eax
    st32(m, esp + 0x1c, 0x10u);                                    // 42a785 mov [esp + 0x1c], 0x10
L_0042a78d:
    edx = ld32(m, esp + 0x14);                                     // 42a78d mov edx, [esp + 0x14]
    ecx = ld32(m, esp + 0x10);                                     // 42a791 mov ecx, [esp + 0x10]
    eax = ld32(m, edx);                                            // 42a795 mov eax, [edx]
    edi = ld32(m, ecx - 0x54);                                     // 42a797 mov edi, [ecx - 0x54]
    st32(m, esp + 0x18, edi);                                      // 42a79a mov [esp + 0x18], edi
    fl = f_test(eax, eax);                                         // 42a79e test eax, eax
    if (fl.e()) goto L_0042a7f1;                                   // 42a7a0 je 0x42a7f1
    esi = ld32(m, eax + 0x4);                                      // 42a7a2 mov esi, [eax + 4]
    edi = ld32(m, eax + 0x8);                                      // 42a7a5 mov edi, [eax + 8]
    ecx = ld32(m, esi + 0x1964);                                   // 42a7a8 mov ecx, [esi + 0x1964]
    ecx = ecx - ld32(m, esi + 0x1960);                             // 42a7ae sub ecx, [esi + 0x1960]
    esi = esi + 0x1954u;                                           // 42a7b4 add esi, 0x1954
    eax = 0x92492493u;                                             // 42a7ba mov eax, 0x92492493
    { int64_t p_ = int64_t(int32_t(eax)) * int32_t(ecx); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); } // 42a7bf imul ecx
    edx = edx + ecx;                                               // 42a7c1 add edx, ecx
    edx = uint32_t(int32_t(edx) >> 7);                             // 42a7c3 sar edx, 7
    eax = edx;                                                     // 42a7c6 mov eax, edx
    eax = eax >> 31;                                               // 42a7c8 shr eax, 0x1f
    eax = eax + edx;                                               // 42a7cb add eax, edx
    fl = f_cmp(edi, eax);                                          // 42a7cd cmp edi, eax
    if (fl.b()) goto L_0042a7d7;                                   // 42a7cf jb 0x42a7d7
    k.call(ld32(m, 0x916224));                                     // 42a7d1 call [0x916224]
L_0042a7d7:
    ecx = ld32(m, esi + 0xc);                                      // 42a7d7 mov ecx, [esi + 0xc]
    edi = edi * 0xe0u;                                             // 42a7da imul edi, edi, 0xe0
    x0 = ld(m, edi + ecx + 0x40);                                  // 42a7e0 movaps xmm0, [edi + ecx + 0x40]
    edi = ld32(m, esp + 0x18);                                     // 42a7e5 mov edi, [esp + 0x18]
    st(m, esp + 0xd0, x0);                                         // 42a7e9 movaps [esp + 0xd0], xmm0
L_0042a7f1:
    fl = f_test8(ld8(m, 0xd72aa8), 0x1u);                          // 42a7f1 test byte ptr [0xd72aa8], 1
    edx = ld32(m, esp + 0x10);                                     // 42a7f8 mov edx, [esp + 0x10]
    x0 = X::ss(ld32(m, edx));                                      // 42a7fc movss xmm0, [edx]
    x0 = shufps(x0, x0, 0x00);                                     // 42a800 shufps xmm0, xmm0, 0
    st(m, esp + 0xc0, x0);                                         // 42a804 movaps [esp + 0xc0], xmm0
    x0 = X::ss(ld32(m, esp + 0xd0));                               // 42a80c movss xmm0, [esp + 0xd0]
    st32(m, esp + 0x20, x0.u[0]);                                  // 42a815 movss [esp + 0x20], xmm0
    x0 = X::ss(ld32(m, esp + 0xd4));                               // 42a81b movss xmm0, [esp + 0xd4]
    st32(m, esp + 0x24, x0.u[0]);                                  // 42a824 movss [esp + 0x24], xmm0
    x0 = X::ss(ld32(m, esp + 0xd8));                               // 42a82a movss xmm0, [esp + 0xd8]
    st32(m, esp + 0x28, x0.u[0]);                                  // 42a833 movss [esp + 0x28], xmm0
    if (fl.ne()) goto L_0042a8df;                                  // 42a839 jne 0x42a8df
    st32(m, 0xd72aa8, ld32(m, 0xd72aa8) | 0x1u);                   // 42a83f or [0xd72aa8], 1
    k.push(0x4u);                                                  // 42a846 push 4
    k.call(0x0078fbe0);  // ReflexAlignedAlloc                     // 42a848 call 0x78fbe0
    esp = esp + 0x4u;                                              // 42a84d add esp, 4
    fl = f_test(eax, eax);                                         // 42a850 test eax, eax
    if (fl.e()) goto L_0042a85c;                                   // 42a852 je 0x42a85c
    st32(m, eax, 0xd72ab0u);                                       // 42a854 mov [eax], 0xd72ab0
    goto L_0042a85e;                                               // 42a85a jmp 0x42a85e
L_0042a85c:
    eax = 0u;                                                      // 42a85c xor eax, eax
L_0042a85e:
    st32(m, 0xd72ab0, eax);                                        // 42a85e mov [0xd72ab0], eax
    eax = esp + 0x38;                                              // 42a863 lea eax, [esp + 0x38]
    k.push(eax);                                                   // 42a867 push eax
    k.push(0xd72ab0u);                                             // 42a868 push 0xd72ab0
    st32(m, esp + 0x40, 0x0u);                                     // 42a86d mov [esp + 0x40], 0
    k.call(0x0042b410);  // FUN_0042b410                           // 42a875 call 0x42b410
    eax = ld32(m, 0xd72ac0);                                       // 42a87a mov eax, [0xd72ac0]
    ecx = ld32(m, 0xd72abc);                                       // 42a87f mov ecx, [0xd72abc]
    ebx = eax;                                                     // 42a885 mov ebx, eax
    fl = f_cmp(ecx, eax);                                          // 42a887 cmp ecx, eax
    if (fl.be()) goto L_0042a89c;                                  // 42a889 jbe 0x42a89c
    k.call(ld32(m, 0x916224));                                     // 42a88b call [0x916224]
    eax = ld32(m, 0xd72ac0);                                       // 42a891 mov eax, [0xd72ac0]
    ecx = ld32(m, 0xd72abc);                                       // 42a896 mov ecx, [0xd72abc]
L_0042a89c:
    esi = ld32(m, 0xd72ab0);                                       // 42a89c mov esi, [0xd72ab0]
    edi = ecx;                                                     // 42a8a2 mov edi, ecx
    fl = f_cmp(ecx, eax);                                          // 42a8a4 cmp ecx, eax
    if (fl.be()) goto L_0042a8ae;                                  // 42a8a6 jbe 0x42a8ae
    k.call(ld32(m, 0x916224));                                     // 42a8a8 call [0x916224]
L_0042a8ae:
    ecx = ld32(m, 0xd72ab0);                                       // 42a8ae mov ecx, [0xd72ab0]
    k.push(ebx);                                                   // 42a8b4 push ebx
    k.push(esi);                                                   // 42a8b5 push esi
    k.push(edi);                                                   // 42a8b6 push edi
    k.push(ecx);                                                   // 42a8b7 push ecx
    edx = esp + 0x88;                                              // 42a8b8 lea edx, [esp + 0x88]
    k.push(edx);                                                   // 42a8bf push edx
    esi = 0xd72ab0u;                                               // 42a8c0 mov esi, 0xd72ab0
    k.call(0x007cf450);  // FUN_007cf450                           // 42a8c5 call 0x7cf450
    k.push(0x911e90u);                                             // 42a8ca push 0x911e90
    k.call(0x008f9fbf);  // _atexit                                // 42a8cf call 0x8f9fbf
    ebx = ld32(m, esp + 0x40);                                     // 42a8d4 mov ebx, [esp + 0x40]
    edi = ld32(m, esp + 0x1c);                                     // 42a8d8 mov edi, [esp + 0x1c]
    esp = esp + 0x4u;                                              // 42a8dc add esp, 4
L_0042a8df:
    eax = esp + 0x20;                                              // 42a8df lea eax, [esp + 0x20]
    k.push(eax);                                                   // 42a8e3 push eax
    esi = 0xd72aacu;                                               // 42a8e4 mov esi, 0xd72aac
    k.call(0x007efef0);  // FUN_007efef0                           // 42a8e9 call 0x7efef0
    fl = f_test(eax, eax);                                         // 42a8ee test eax, eax
    if (fl.e()) goto L_0042ac6c;                                   // 42a8f0 je 0x42ac6c
    x0 = ld(m, esp + 0xd0);                                        // 42a8f6 movaps xmm0, [esp + 0xd0]
    x4 = ld(m, esp + 0xc0);                                        // 42a8fe movaps xmm4, [esp + 0xc0]
    x5 = ld(m, 0xd616a0);                                          // 42a906 movaps xmm5, [0xd616a0]
    x3 = X::ss(ld32(m, esp + 0x24));                               // 42a90d movss xmm3, [esp + 0x24]
    x1 = x0;                                                       // 42a913 movaps xmm1, xmm0
    x1 = subps(x1, x4);                                            // 42a916 subps xmm1, xmm4
    x1 = shufps(x1, x0, 0xe5);                                     // 42a919 shufps xmm1, xmm0, 0xe5
    x2 = movss(x2, x0);                                            // 42a91d movss xmm2, xmm0
    x1 = movss(x1, x2);                                            // 42a921 movss xmm1, xmm2
    x1 = mulps(x1, x5);                                            // 42a925 mulps xmm1, xmm5
    x2 = x1;                                                       // 42a928 movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0x99);                                     // 42a92b shufps xmm2, xmm1, 0x99
    x1 = addss(x1, x2);                                            // 42a92f addss xmm1, xmm2
    x2 = shufps(x2, x2, 0x55);                                     // 42a933 shufps xmm2, xmm2, 0x55
    x1 = addss(x1, x2);                                            // 42a937 addss xmm1, xmm2
    x3 = shufps(x3, x3, 0x00);                                     // 42a93b shufps xmm3, xmm3, 0
    x1 = shufps(x1, x1, 0x00);                                     // 42a93f shufps xmm1, xmm1, 0
    x2 = x3;                                                       // 42a943 movaps xmm2, xmm3
    x2 = cmpleps(x2, x1);                                          // 42a946 cmpleps xmm2, xmm1
    st(m, esp + 0xb0, x2);                                         // 42a94a movaps [esp + 0xb0], xmm2
    ecx = ld32(m, esp + 0xb4);                                     // 42a952 mov ecx, [esp + 0xb4]
    edx = ld32(m, esp + 0xb8);                                     // 42a959 mov edx, [esp + 0xb8]
    edx = edx + ecx;                                               // 42a960 add edx, ecx
    edx = edx + ld32(m, esp + 0xb0);                               // 42a962 add edx, [esp + 0xb0]
    st(m, esp + 0x40, x3);                                         // 42a969 movaps [esp + 0x40], xmm3
    fl = f_cmp(edx, 0xfffffffdu);                                  // 42a96e cmp edx, -3
    if (fl.e()) goto L_0042ac6c;                                   // 42a971 je 0x42ac6c
    x1 = x4;                                                       // 42a977 movaps xmm1, xmm4
    x1 = addps(x1, x0);                                            // 42a97a addps xmm1, xmm0
    x1 = shufps(x1, x0, 0xe5);                                     // 42a97d shufps xmm1, xmm0, 0xe5
    x2 = movss(x2, x0);                                            // 42a981 movss xmm2, xmm0
    x1 = movss(x1, x2);                                            // 42a985 movss xmm1, xmm2
    x1 = mulps(x1, x5);                                            // 42a989 mulps xmm1, xmm5
    x2 = x1;                                                       // 42a98c movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0x99);                                     // 42a98f shufps xmm2, xmm1, 0x99
    x1 = addss(x1, x2);                                            // 42a993 addss xmm1, xmm2
    x2 = shufps(x2, x2, 0x55);                                     // 42a997 shufps xmm2, xmm2, 0x55
    x1 = addss(x1, x2);                                            // 42a99b addss xmm1, xmm2
    x2 = x1;                                                       // 42a99f movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0x00);                                     // 42a9a2 shufps xmm2, xmm1, 0
    x2 = cmpleps(x2, x3);                                          // 42a9a6 cmpleps xmm2, xmm3
    st(m, esp + 0xb0, x2);                                         // 42a9aa movaps [esp + 0xb0], xmm2
    eax = ld32(m, esp + 0xb4);                                     // 42a9b2 mov eax, [esp + 0xb4]
    ecx = ld32(m, esp + 0xb8);                                     // 42a9b9 mov ecx, [esp + 0xb8]
    ecx = ecx + eax;                                               // 42a9c0 add ecx, eax
    ecx = ecx + ld32(m, esp + 0xb0);                               // 42a9c2 add ecx, [esp + 0xb0]
    fl = f_cmp(ecx, 0xfffffffdu);                                  // 42a9c9 cmp ecx, -3
    if (fl.ne()) goto L_0042aa09;                                  // 42a9cc jne 0x42aa09
    x0 = ld(m, esp + 0xe0);                                        // 42a9ce movaps xmm0, [esp + 0xe0]
    x0 = mulps(x0, ld(m, esp + 0x80));                             // 42a9d6 mulps xmm0, [esp + 0x80]
    edx = edi;                                                     // 42a9de mov edx, edi
    x0 = mulps(x0, x4);                                            // 42a9e0 mulps xmm0, xmm4
    edx = edx << 7;                                                // 42a9e3 shl edx, 7
    x1 = ld(m, edx + ebx + 0x450);                                 // 42a9e6 movaps xmm1, [edx + ebx + 0x450]
    x0 = mulps(x0, x4);                                            // 42a9ee mulps xmm0, xmm4
    eax = edx + ebx;                                               // 42a9f1 lea eax, [edx + ebx]
    x0 = mulps(x0, x4);                                            // 42a9f4 mulps xmm0, xmm4
    x0 = mulps(x0, x5);                                            // 42a9f7 mulps xmm0, xmm5
    x0 = addps(x0, x1);                                            // 42a9fa addps xmm0, xmm1
    st(m, eax + 0x450, x0);                                        // 42a9fd movaps [eax + 0x450], xmm0
    goto L_0042ac3c;                                               // 42aa04 jmp 0x42ac3c
L_0042aa09:
    fl = f_cmp(ld32(m, ebx + 0xa8a0), 0x0u);                       // 42aa09 cmp [ebx + 0xa8a0], 0
    if (fl.ne()) goto L_0042ab77;                                  // 42aa10 jne 0x42ab77
    fl = f_test8(ld8(m, 0xd72aa8), 0x1u);                          // 42aa16 test byte ptr [0xd72aa8], 1
    if (fl.ne()) goto L_0042aa38;                                  // 42aa1d jne 0x42aa38
    st32(m, 0xd72aa8, ld32(m, 0xd72aa8) | 0x1u);                   // 42aa1f or [0xd72aa8], 1
    k.call(0x004242f0);  // FUN_004242f0                           // 42aa26 call 0x4242f0
    k.push(0x911e90u);                                             // 42aa2b push 0x911e90
    k.call(0x008f9fbf);  // _atexit                                // 42aa30 call 0x8f9fbf
    esp = esp + 0x4u;                                              // 42aa35 add esp, 4
L_0042aa38:
    eax = ld32(m, esp + 0x10);                                     // 42aa38 mov eax, [esp + 0x10]
    fpush(m, reflex_f80::from_int(1));                             // 42aa3c fld1 
    x0 = X::ss(ld32(m, eax));                                      // 42aa3e movss xmm0, [eax]
    k.push(ecx);                                                   // 42aa42 push ecx
    ecx = esp + 0x24;                                              // 42aa43 lea ecx, [esp + 0x24]
    st32(m, esp, reflex_f80::to_f32(fst_(m, 0), frc(m))); m.fpu.pop_back(); // 42aa47 fstp [esp]
    eax = 0xd72aacu;                                               // 42aa4a mov eax, 0xd72aac
    k.call(0x007efed0);  // FUN_007efed0                           // 42aa4f call 0x7efed0
    esi = ebx + 0x380;                                             // 42aa54 lea esi, [ebx + 0x380]
    ecx = esp + 0xf0;                                              // 42aa5a lea ecx, [esp + 0xf0]
    k.call(0x00422350);  // FUN_00422350                           // 42aa61 call 0x422350
    eax = ld32(m, 0xa95844);                                       // 42aa66 mov eax, [0xa95844]
    edx = ld32(m, eax + 0x20);                                     // 42aa6b mov edx, [eax + 0x20]
    ecx = 0x1u;                                                    // 42aa6e mov ecx, 1
    fl = f_cmp(ld32(m, edx + 0x34), ecx);                          // 42aa73 cmp [edx + 0x34], ecx
    if (fl.ne()) goto L_0042aa85;                                  // 42aa76 jne 0x42aa85
    eax = ld32(m, 0xa946b8);                                       // 42aa78 mov eax, [0xa946b8]
    eax = ld32(m, eax + 0x5a54);                                   // 42aa7d mov eax, [eax + 0x5a54]
    goto L_0042aa9c;                                               // 42aa83 jmp 0x42aa9c
L_0042aa85:
    edx = ld32(m, eax + 0x1c);                                     // 42aa85 mov edx, [eax + 0x1c]
    fl = f_cmp(ld32(m, edx + 0x34), ecx);                          // 42aa88 cmp [edx + 0x34], ecx
    if (fl.ne()) goto L_0042aa9a;                                  // 42aa8b jne 0x42aa9a
    eax = ld32(m, 0xa946b8);                                       // 42aa8d mov eax, [0xa946b8]
    eax = ld32(m, eax + 0x20c8);                                   // 42aa92 mov eax, [eax + 0x20c8]
    goto L_0042aa9c;                                               // 42aa98 jmp 0x42aa9c
L_0042aa9a:
    eax = 0u;                                                      // 42aa9a xor eax, eax
L_0042aa9c:
    eax = ld32(m, eax + 0x20134);                                  // 42aa9c mov eax, [eax + 0x20134]
    esi = ld32(m, eax + 0x1900);                                   // 42aaa2 mov esi, [eax + 0x1900]
    fl = f_cmp(esi, 0x64u);                                        // 42aaa8 cmp esi, 0x64
    if (fl.ae()) goto L_0042ab62;                                  // 42aaab jae 0x42ab62
    ecx = esi + 0x1;                                               // 42aab1 lea ecx, [esi + 1]
    esi = esi << 6;                                                // 42aab4 shl esi, 6
    st32(m, eax + 0x1900, ecx);                                    // 42aab7 mov [eax + 0x1900], ecx
    esi = esi + eax;                                               // 42aabd add esi, eax
    eax = ld32(m, ebx + 0xc);                                      // 42aabf mov eax, [ebx + 0xc]
    k.call(0x0042bd90);  // FUN_0042bd90                           // 42aac2 call 0x42bd90
    edx = ld32(m, eax + 0x34);                                     // 42aac7 mov edx, [eax + 0x34]
    st32(m, esi, edx);                                             // 42aaca mov [esi], edx
    k.call(0x00424340);  // FUN_00424340                           // 42aacc call 0x424340
    edi = eax;                                                     // 42aad1 mov edi, eax
    eax = ld32(m, edi + 0x14);                                     // 42aad3 mov eax, [edi + 0x14]
    eax = eax - ld32(m, edi + 0x10);                               // 42aad6 sub eax, [edi + 0x10]
    edi = edi + 0x4u;                                              // 42aad9 add edi, 4
    eax = uint32_t(int32_t(eax) >> 2);                             // 42aadc sar eax, 2
    fl = f_test(eax, eax);                                         // 42aadf test eax, eax
    if (fl.a()) goto L_0042aae9;                                   // 42aae1 ja 0x42aae9
    k.call(ld32(m, 0x916224));                                     // 42aae3 call [0x916224]
L_0042aae9:
    ecx = ld32(m, edi + 0xc);                                      // 42aae9 mov ecx, [edi + 0xc]
    edx = ld32(m, ecx);                                            // 42aaec mov edx, [ecx]
    eax = ld32(m, edx + 0x50);                                     // 42aaee mov eax, [edx + 0x50]
    x0 = X::ss(ld32(m, esp + 0x20));                               // 42aaf1 movss xmm0, [esp + 0x20]
    x1 = ld(m, esp + 0xf0);                                        // 42aaf7 movaps xmm1, [esp + 0xf0]
    edi = ld32(m, esp + 0x18);                                     // 42aaff mov edi, [esp + 0x18]
    st32(m, esi + 0x4, eax);                                       // 42ab03 mov [esi + 4], eax
    st32(m, esi + 0x2c, 0x0u);                                     // 42ab06 mov [esi + 0x2c], 0
    st8(m, esi + 0x20, 0x0u);                                      // 42ab0d mov byte ptr [esi + 0x20], 0
    eax = eax | 0xffffffffu;                                       // 42ab11 or eax, 0xffffffff
    st32(m, esi + 0x30, eax);                                      // 42ab14 mov [esi + 0x30], eax
    st32(m, esi + 0x34, eax);                                      // 42ab17 mov [esi + 0x34], eax
    st32(m, esi + 0x10, x0.u[0]);                                  // 42ab1a movss [esi + 0x10], xmm0
    x0 = X::ss(ld32(m, esp + 0x24));                               // 42ab1f movss xmm0, [esp + 0x24]
    st32(m, esi + 0x14, x0.u[0]);                                  // 42ab25 movss [esi + 0x14], xmm0
    x0 = X::ss(ld32(m, esp + 0x28));                               // 42ab2a movss xmm0, [esp + 0x28]
    st32(m, esi + 0x18, x0.u[0]);                                  // 42ab30 movss [esi + 0x18], xmm0
    x1 = mulps(x1, x1);                                            // 42ab35 mulps xmm1, xmm1
    x0 = x1;                                                       // 42ab38 movaps xmm0, xmm1
    x0 = shufps(x0, x1, 0x66);                                     // 42ab3b shufps xmm0, xmm1, 0x66
    x1 = addss(x1, x0);                                            // 42ab3f addss xmm1, xmm0
    x0 = shufps(x0, x0, 0x55);                                     // 42ab43 shufps xmm0, xmm0, 0x55
    x1 = addss(x1, x0);                                            // 42ab47 addss xmm1, xmm0
    x0 = x1;                                                       // 42ab4b movaps xmm0, xmm1
    x0 = shufps(x0, x0, 0x00);                                     // 42ab4e shufps xmm0, xmm0, 0
    x0 = sqrtps(x0);                                               // 42ab52 sqrtps xmm0, xmm0
    st32(m, esi + 0x24, x0.u[0]);                                  // 42ab55 movss [esi + 0x24], xmm0
    x0 = X{};                                                      // 42ab5a xorps xmm0, xmm0
    st32(m, esi + 0x28, x0.u[0]);                                  // 42ab5d movss [esi + 0x28], xmm0
L_0042ab62:
    x4 = ld(m, esp + 0xc0);                                        // 42ab62 movaps xmm4, [esp + 0xc0]
    x3 = ld(m, esp + 0x40);                                        // 42ab6a movaps xmm3, [esp + 0x40]
    x0 = ld(m, esp + 0xd0);                                        // 42ab6f movaps xmm0, [esp + 0xd0]
L_0042ab77:
    x5 = ld(m, esp + 0xe0);                                        // 42ab77 movaps xmm5, [esp + 0xe0]
    st32(m, ebx + 0xa8a0, 0x1u);                                   // 42ab7f mov [ebx + 0xa8a0], 1
    x2 = ld(m, 0xd616a0);                                          // 42ab89 movaps xmm2, [0xd616a0]
    x0 = mulps(x0, x2);                                            // 42ab90 mulps xmm0, xmm2
    x1 = x0;                                                       // 42ab93 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x99);                                     // 42ab96 shufps xmm1, xmm0, 0x99
    x0 = addss(x0, x1);                                            // 42ab9a addss xmm0, xmm1
    x1 = shufps(x1, x1, 0x55);                                     // 42ab9e shufps xmm1, xmm1, 0x55
    x0 = addss(x0, x1);                                            // 42aba2 addss xmm0, xmm1
    x0 = shufps(x0, x0, 0x00);                                     // 42aba6 shufps xmm0, xmm0, 0
    x1 = x0;                                                       // 42abaa movaps xmm1, xmm0
    x6 = x1;                                                       // 42abad movaps xmm6, xmm1
    x6 = cmpleps(x6, x3);                                          // 42abb0 cmpleps xmm6, xmm3
    st(m, esp + 0xc0, x6);                                         // 42abb4 movaps [esp + 0xc0], xmm6
    ecx = ld32(m, esp + 0xc4);                                     // 42abbc mov ecx, [esp + 0xc4]
    edx = ld32(m, esp + 0xc8);                                     // 42abc3 mov edx, [esp + 0xc8]
    edx = edx + ecx;                                               // 42abca add edx, ecx
    edx = edx + ld32(m, esp + 0xc0);                               // 42abcc add edx, [esp + 0xc0]
    x0 = x5;                                                       // 42abd3 movaps xmm0, xmm5
    x0 = mulps(x0, ld(m, esp + 0xa0));                             // 42abd6 mulps xmm0, [esp + 0xa0]
    x0 = mulps(x0, x4);                                            // 42abde mulps xmm0, xmm4
    x0 = mulps(x0, x4);                                            // 42abe1 mulps xmm0, xmm4
    x0 = mulps(x0, x4);                                            // 42abe4 mulps xmm0, xmm4
    x0 = mulps(x0, x2);                                            // 42abe7 mulps xmm0, xmm2
    fl = f_cmp(edx, 0xfffffffdu);                                  // 42abea cmp edx, -3
    if (fl.ne()) goto L_0042abf4;                                  // 42abed jne 0x42abf4
    x3 = subps(x3, x1);                                            // 42abef subps xmm3, xmm1
    goto L_0042ac01;                                               // 42abf2 jmp 0x42ac01
L_0042abf4:
    x0 = ld(m, 0xd61670);                                          // 42abf4 movaps xmm0, [0xd61670]
    x1 = subps(x1, x3);                                            // 42abfb subps xmm1, xmm3
    x3 = x1;                                                       // 42abfe movaps xmm3, xmm1
L_0042ac01:
    x1 = x3;                                                       // 42ac01 movaps xmm1, xmm3
    x1 = mulps(x1, x3);                                            // 42ac04 mulps xmm1, xmm3
    x1 = mulps(x1, ld(m, esp + 0x90));                             // 42ac07 mulps xmm1, [esp + 0x90]
    eax = edi;                                                     // 42ac0f mov eax, edi
    x6 = x4;                                                       // 42ac11 movaps xmm6, xmm4
    x6 = mulps(x6, x4);                                            // 42ac14 mulps xmm6, xmm4
    x6 = subps(x6, x1);                                            // 42ac17 subps xmm6, xmm1
    eax = eax << 7;                                                // 42ac1a shl eax, 7
    x3 = mulps(x3, x5);                                            // 42ac1d mulps xmm3, xmm5
    x6 = mulps(x6, x3);                                            // 42ac20 mulps xmm6, xmm3
    eax = eax + ebx;                                               // 42ac23 add eax, ebx
    x6 = mulps(x6, x2);                                            // 42ac25 mulps xmm6, xmm2
    x6 = addps(x6, x0);                                            // 42ac28 addps xmm6, xmm0
    x0 = ld(m, eax + 0x450);                                       // 42ac2b movaps xmm0, [eax + 0x450]
    x6 = addps(x6, x0);                                            // 42ac32 addps xmm6, xmm0
    st(m, eax + 0x450, x6);                                        // 42ac35 movaps [eax + 0x450], xmm6
L_0042ac3c:
    x1 = ld(m, eax + 0x410);                                       // 42ac3c movaps xmm1, [eax + 0x410]
    edi = edi * 0x70u;                                             // 42ac43 imul edi, edi, 0x70
    x0 = ld(m, esp + 0x60);                                        // 42ac46 movaps xmm0, [esp + 0x60]
    x0 = mulps(x0, x1);                                            // 42ac4b mulps xmm0, xmm1
    st(m, eax + 0x410, x0);                                        // 42ac4e movaps [eax + 0x410], xmm0
    x0 = ld(m, edi + ebx + 0xe90);                                 // 42ac55 movaps xmm0, [edi + ebx + 0xe90]
    x0 = mulps(x0, ld(m, esp + 0x50));                             // 42ac5d mulps xmm0, [esp + 0x50]
    eax = edi + ebx + 0xe90;                                       // 42ac62 lea eax, [edi + ebx + 0xe90]
    st(m, eax, x0);                                                // 42ac69 movaps [eax], xmm0
L_0042ac6c:
    st32(m, esp + 0x10, ld32(m, esp + 0x10) + 0x4u);               // 42ac6c add [esp + 0x10], 4
    st32(m, esp + 0x14, ld32(m, esp + 0x14) + 0x8u);               // 42ac71 add [esp + 0x14], 8
    { uint32_t a_ = ld32(m, esp + 0x1c), b_ = 0x1u, r_ = a_ - b_; fl = f_sub(a_, b_, r_); st32(m, esp + 0x1c, r_); } // 42ac76 sub [esp + 0x1c], 1
    if (fl.ne()) goto L_0042a78d;                                  // 42ac7b jne 0x42a78d
    ecx = ld32(m, esp + 0x10c);                                    // 42ac81 mov ecx, [esp + 0x10c]
    edi = k.pop();                                                 // 42ac88 pop edi
    esi = k.pop();                                                 // 42ac89 pop esi
    ebx = k.pop();                                                 // 42ac8a pop ebx
    ecx = ecx ^ esp;                                               // 42ac8b xor ecx, esp
    k.call(0x008f9efe);  // __security_check_cookie                // 42ac8d call 0x8f9efe
    esp = ebp;                                                     // 42ac92 mov esp, ebp
    ebp = k.pop();                                                 // 42ac94 pop ebp
    return k.ret(4);                                               // 42ac95 ret 4
}

// 0042aca0 FUN_0042aca0: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x0042aca0, Fn_0042aca0, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 42aca0 push ebp
    ebp = esp;                                                     // 42aca1 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 42aca3 and esp, 0xfffffff0
    eax = ld32(m, ebp + 0x8);                                      // 42aca6 mov eax, [ebp + 8]
    esp = esp - 0x10u;                                             // 42aca9 sub esp, 0x10
    fl = f_cmp(ld32(m, eax + 0x370), 0x0u);                        // 42acac cmp [eax + 0x370], 0
    if (fl.ne()) goto L_0042acbd;                                  // 42acb3 jne 0x42acbd
    eax = 0u;                                                      // 42acb5 xor eax, eax
    esp = ebp;                                                     // 42acb7 mov esp, ebp
    ebp = k.pop();                                                 // 42acb9 pop ebp
    return k.ret(4);                                               // 42acba ret 4
L_0042acbd:
    x1 = ld(m, eax + 0xe80);                                       // 42acbd movaps xmm1, [eax + 0xe80]
    x2 = ld(m, 0xd61680);                                          // 42acc4 movaps xmm2, [0xd61680]
    x0 = X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000);     // 42accb movaps xmm0, [0x92db80]
    x5 = X::v(0xbf800000, 0xbf800000, 0xbf800000, 0x3f800000);     // 42acd2 movaps xmm5, [0x93a220]
    x0 = andps(x0, x2);                                            // 42acd9 andps xmm0, xmm2
    x7 = X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000);     // 42acdc movaps xmm7, [0x93a200]
    x3 = x0;                                                       // 42ace3 movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0x00);                                     // 42ace6 shufps xmm3, xmm0, 0
    x5 = mulps(x5, x1);                                            // 42acea mulps xmm5, xmm1
    x4 = x0;                                                       // 42aced movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x55);                                     // 42acf0 shufps xmm4, xmm0, 0x55
    x6 = x0;                                                       // 42acf4 movaps xmm6, xmm0
    x6 = shufps(x6, x0, 0xaa);                                     // 42acf7 shufps xmm6, xmm0, 0xaa
    x2 = x5;                                                       // 42acfb movaps xmm2, xmm5
    x2 = shufps(x2, x5, 0x1b);                                     // 42acfe shufps xmm2, xmm5, 0x1b
    x3 = mulps(x3, x2);                                            // 42ad02 mulps xmm3, xmm2
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 42ad05 mulps xmm3, [0x93a1f0]
    x2 = shufps(x2, x2, 0xb1);                                     // 42ad0c shufps xmm2, xmm2, 0xb1
    x4 = mulps(x4, x2);                                            // 42ad10 mulps xmm4, xmm2
    x4 = mulps(x4, x7);                                            // 42ad13 mulps xmm4, xmm7
    x2 = shufps(x2, x2, 0x1b);                                     // 42ad16 shufps xmm2, xmm2, 0x1b
    x2 = mulps(x2, x6);                                            // 42ad1a mulps xmm2, xmm6
    x6 = X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000);     // 42ad1d movaps xmm6, [0x93a210]
    x2 = mulps(x2, x6);                                            // 42ad24 mulps xmm2, xmm6
    x2 = addps(x2, x4);                                            // 42ad27 addps xmm2, xmm4
    x4 = x0;                                                       // 42ad2a movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0xff);                                     // 42ad2d shufps xmm4, xmm0, 0xff
    x4 = mulps(x4, x5);                                            // 42ad31 mulps xmm4, xmm5
    x4 = addps(x4, x3);                                            // 42ad34 addps xmm4, xmm3
    x2 = addps(x2, x4);                                            // 42ad37 addps xmm2, xmm4
    x3 = x1;                                                       // 42ad3a movaps xmm3, xmm1
    x3 = shufps(x3, x1, 0x00);                                     // 42ad3d shufps xmm3, xmm1, 0
    x4 = x1;                                                       // 42ad41 movaps xmm4, xmm1
    x4 = shufps(x4, x1, 0x55);                                     // 42ad44 shufps xmm4, xmm1, 0x55
    x5 = x1;                                                       // 42ad48 movaps xmm5, xmm1
    x5 = shufps(x5, x1, 0xaa);                                     // 42ad4b shufps xmm5, xmm1, 0xaa
    x0 = x2;                                                       // 42ad4f movaps xmm0, xmm2
    x0 = shufps(x0, x2, 0x1b);                                     // 42ad52 shufps xmm0, xmm2, 0x1b
    x3 = mulps(x3, x0);                                            // 42ad56 mulps xmm3, xmm0
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 42ad59 mulps xmm3, [0x93a1f0]
    x0 = shufps(x0, x0, 0xb1);                                     // 42ad60 shufps xmm0, xmm0, 0xb1
    x4 = mulps(x4, x0);                                            // 42ad64 mulps xmm4, xmm0
    x0 = shufps(x0, x0, 0x1b);                                     // 42ad67 shufps xmm0, xmm0, 0x1b
    x0 = mulps(x0, x5);                                            // 42ad6b mulps xmm0, xmm5
    x0 = mulps(x0, x6);                                            // 42ad6e mulps xmm0, xmm6
    x4 = mulps(x4, x7);                                            // 42ad71 mulps xmm4, xmm7
    x0 = addps(x0, x4);                                            // 42ad74 addps xmm0, xmm4
    x4 = x1;                                                       // 42ad77 movaps xmm4, xmm1
    x4 = shufps(x4, x1, 0xff);                                     // 42ad7a shufps xmm4, xmm1, 0xff
    x1 = ld(m, eax + 0xa840);                                      // 42ad7e movaps xmm1, [eax + 0xa840]
    x4 = mulps(x4, x2);                                            // 42ad85 mulps xmm4, xmm2
    x4 = addps(x4, x3);                                            // 42ad88 addps xmm4, xmm3
    x0 = addps(x0, x4);                                            // 42ad8b addps xmm0, xmm4
    x0 = mulps(x0, x1);                                            // 42ad8e mulps xmm0, xmm1
    x1 = x0;                                                       // 42ad91 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x99);                                     // 42ad94 shufps xmm1, xmm0, 0x99
    x0 = addss(x0, x1);                                            // 42ad98 addss xmm0, xmm1
    x1 = shufps(x1, x1, 0x55);                                     // 42ad9c shufps xmm1, xmm1, 0x55
    x0 = addss(x0, x1);                                            // 42ada0 addss xmm0, xmm1
    x1 = ld(m, 0xd61670);                                          // 42ada4 movaps xmm1, [0xd61670]
    x0 = shufps(x0, x0, 0x00);                                     // 42adab shufps xmm0, xmm0, 0
    x1 = cmpleps(x1, x0);                                          // 42adaf cmpleps xmm1, xmm0
    st(m, esp, x1);                                                // 42adb3 movaps [esp], xmm1
    eax = ld32(m, esp + 0x4);                                      // 42adb7 mov eax, [esp + 4]
    ecx = ld32(m, esp + 0x8);                                      // 42adbb mov ecx, [esp + 8]
    ecx = ecx + eax;                                               // 42adbf add ecx, eax
    ecx = ecx + ld32(m, esp);                                      // 42adc1 add ecx, [esp]
    eax = 0u;                                                      // 42adc4 xor eax, eax
    fl = f_cmp(ecx, 0xfffffffdu);                                  // 42adc6 cmp ecx, -3
    eax = (eax & 0xffffff00) | (uint32_t(fl.e() ? 1u : 0u) & 0xff); // 42adc9 sete al
    esp = ebp;                                                     // 42adcc mov esp, ebp
    ebp = k.pop();                                                 // 42adce pop ebp
    return k.ret(4);                                               // 42adcf ret 4
}

// 0042ade0 FUN_0042ade0: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x0042ade0, Fn_0042ade0, "math", 0) {  // outputs: what callers read (it calls other groups' code)
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 42ade0 push ebp
    ebp = esp;                                                     // 42ade1 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 42ade3 and esp, 0xfffffff0
    esp = esp - 0x2cu;                                             // 42ade6 sub esp, 0x2c
    eax = ld32(m, 0x9b45e4);                                       // 42ade9 mov eax, [0x9b45e4]
    eax = eax ^ esp;                                               // 42adee xor eax, esp
    st32(m, esp + 0x28, eax);                                      // 42adf0 mov [esp + 0x28], eax
    eax = ld32(m, ebp + 0x8);                                      // 42adf4 mov eax, [ebp + 8]
    k.push(edi);                                                   // 42adf7 push edi
    edi = ld32(m, eax + 0x9c);                                     // 42adf8 mov edi, [eax + 0x9c]
    fl = f_cmp(ld32(m, edi + 0x10), 0x0u);                         // 42adfe cmp [edi + 0x10], 0
    if (fl.ne()) goto L_0042ae51;                                  // 42ae02 jne 0x42ae51
    x1 = ld(m, eax + 0x410);                                       // 42ae04 movaps xmm1, [eax + 0x410]
    x0 = ld(m, eax + 0xa840);                                      // 42ae0b movaps xmm0, [eax + 0xa840]
    x0 = mulps(x0, x1);                                            // 42ae12 mulps xmm0, xmm1
    x1 = x0;                                                       // 42ae15 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x99);                                     // 42ae18 shufps xmm1, xmm0, 0x99
    x0 = addss(x0, x1);                                            // 42ae1c addss xmm0, xmm1
    x1 = shufps(x1, x1, 0x55);                                     // 42ae20 shufps xmm1, xmm1, 0x55
    x0 = addss(x0, x1);                                            // 42ae24 addss xmm0, xmm1
    x0 = shufps(x0, x0, 0x00);                                     // 42ae28 shufps xmm0, xmm0, 0
    st(m, esp + 0x10, x0);                                         // 42ae2c movaps [esp + 0x10], xmm0
    k.call(0x0054c020);  // Player_ActiveCameraEntity              // 42ae31 call 0x54c020
    fpush(m, reflex_f80::from_f32(0x3e800000u));                   // 42ae36 fld [0x92ff84]
    edx = ld32(m, eax);                                            // 42ae3c mov edx, [eax]
    edx = ld32(m, edx + 0x88);                                     // 42ae3e mov edx, [edx + 0x88]
    k.push(ecx);                                                   // 42ae44 push ecx
    ecx = esp + 0x14;                                              // 42ae45 lea ecx, [esp + 0x14]
    st32(m, esp, reflex_f80::to_f32(fst_(m, 0), frc(m))); m.fpu.pop_back(); // 42ae49 fstp [esp]
    k.push(ecx);                                                   // 42ae4c push ecx
    ecx = eax;                                                     // 42ae4d mov ecx, eax
    k.call(edx);                                                   // 42ae4f call edx
L_0042ae51:
    ecx = ld32(m, esp + 0x2c);                                     // 42ae51 mov ecx, [esp + 0x2c]
    edi = k.pop();                                                 // 42ae55 pop edi
    ecx = ecx ^ esp;                                               // 42ae56 xor ecx, esp
    k.call(0x008f9efe);  // __security_check_cookie                // 42ae58 call 0x8f9efe
    esp = ebp;                                                     // 42ae5d mov esp, ebp
    ebp = k.pop();                                                 // 42ae5f pop ebp
    return k.ret(4);                                               // 42ae60 ret 4
}

// 0042b000 FUN_0042b000: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x0042b000, Fn_0042b000, "math", 0) {  // outputs: what callers read (it calls other groups' code)
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 42b000 push ebp
    ebp = esp;                                                     // 42b001 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 42b003 and esp, 0xfffffff0
    esp = esp - 0x38u;                                             // 42b006 sub esp, 0x38
    eax = ld32(m, 0x9b45e4);                                       // 42b009 mov eax, [0x9b45e4]
    eax = eax ^ esp;                                               // 42b00e xor eax, esp
    st32(m, esp + 0x34, eax);                                      // 42b010 mov [esp + 0x34], eax
    fl = f_cmp(ld32(m, esi + 0x18), 0x0u);                         // 42b014 cmp [esi + 0x18], 0
    k.push(ebx);                                                   // 42b018 push ebx
    k.push(edi);                                                   // 42b019 push edi
    edi = ecx;                                                     // 42b01a mov edi, ecx
    if (fl.e()) goto L_0042b07d;                                   // 42b01c je 0x42b07d
    ecx = ld32(m, esi + 0x10);                                     // 42b01e mov ecx, [esi + 0x10]
    ecx = ecx - ld32(m, esi + 0xc);                                // 42b021 sub ecx, [esi + 0xc]
    eax = 0x92492493u;                                             // 42b024 mov eax, 0x92492493
    { int64_t p_ = int64_t(int32_t(eax)) * int32_t(ecx); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); } // 42b029 imul ecx
    edx = edx + ecx;                                               // 42b02b add edx, ecx
    edx = uint32_t(int32_t(edx) >> 7);                             // 42b02d sar edx, 7
    eax = edx;                                                     // 42b030 mov eax, edx
    eax = eax >> 31;                                               // 42b032 shr eax, 0x1f
    eax = eax + edx;                                               // 42b035 add eax, edx
    fl = f_cmp(edi, eax);                                          // 42b037 cmp edi, eax
    if (fl.ge()) goto L_0042b07d;                                  // 42b039 jge 0x42b07d
    if (fl.b()) goto L_0042b043;                                   // 42b03b jb 0x42b043
    k.call(ld32(m, 0x916224));                                     // 42b03d call [0x916224]
L_0042b043:
    eax = edi;                                                     // 42b043 mov eax, edi
    eax = eax * 0xe0u;                                             // 42b045 imul eax, eax, 0xe0
    eax = eax + ld32(m, esi + 0xc);                                // 42b04b add eax, [esi + 0xc]
    ebx = esp + 0x10;                                              // 42b04e lea ebx, [esp + 0x10]
    ecx = ld32(m, eax + 0x2c);                                     // 42b052 mov ecx, [eax + 0x2c]
    edx = ld32(m, ecx);                                            // 42b055 mov edx, [ecx]
    k.push(ebx);                                                   // 42b057 push ebx
    ebx = esp + 0x24;                                              // 42b058 lea ebx, [esp + 0x24]
    k.push(ebx);                                                   // 42b05c push ebx
    ebx = eax + 0x40;                                              // 42b05d lea ebx, [eax + 0x40]
    eax = eax + 0x50u;                                             // 42b060 add eax, 0x50
    k.push(ebx);                                                   // 42b063 push ebx
    k.push(eax);                                                   // 42b064 push eax
    eax = ld32(m, edx + 0xc);                                      // 42b065 mov eax, [edx + 0xc]
    k.call(eax);                                                   // 42b068 call eax
    eax = ld32(m, esi + 0x18);                                     // 42b06a mov eax, [esi + 0x18]
    ecx = esp + 0x10;                                              // 42b06d lea ecx, [esp + 0x10]
    k.push(ecx);                                                   // 42b071 push ecx
    edx = esp + 0x24;                                              // 42b072 lea edx, [esp + 0x24]
    k.push(edx);                                                   // 42b076 push edx
    k.push(eax);                                                   // 42b077 push eax
    k.call(0x0084a1a0);  // FUN_0084a1a0                           // 42b078 call 0x84a1a0
L_0042b07d:
    ecx = ld32(m, esp + 0x3c);                                     // 42b07d mov ecx, [esp + 0x3c]
    edi = k.pop();                                                 // 42b081 pop edi
    ebx = k.pop();                                                 // 42b082 pop ebx
    ecx = ecx ^ esp;                                               // 42b083 xor ecx, esp
    k.call(0x008f9efe);  // __security_check_cookie                // 42b085 call 0x8f9efe
    esp = ebp;                                                     // 42b08a mov esp, ebp
    ebp = k.pop();                                                 // 42b08c pop ebp
    return k.ret(0);                                               // 42b08d ret 
}

// 0042bd90 FUN_0042bd90: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x0042bd90, Fn_0042bd90, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    k.push(esi);                                                   // 42bd90 push esi
    esi = ld32(m, eax + 0x4);                                      // 42bd91 mov esi, [eax + 4]
    ecx = ld32(m, esi + 0x1964);                                   // 42bd94 mov ecx, [esi + 0x1964]
    ecx = ecx - ld32(m, esi + 0x1960);                             // 42bd9a sub ecx, [esi + 0x1960]
    esi = esi + 0x1954u;                                           // 42bda0 add esi, 0x1954
    k.push(edi);                                                   // 42bda6 push edi
    edi = ld32(m, eax + 0x8);                                      // 42bda7 mov edi, [eax + 8]
    eax = 0x92492493u;                                             // 42bdaa mov eax, 0x92492493
    { int64_t p_ = int64_t(int32_t(eax)) * int32_t(ecx); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); } // 42bdaf imul ecx
    edx = edx + ecx;                                               // 42bdb1 add edx, ecx
    edx = uint32_t(int32_t(edx) >> 7);                             // 42bdb3 sar edx, 7
    eax = edx;                                                     // 42bdb6 mov eax, edx
    eax = eax >> 31;                                               // 42bdb8 shr eax, 0x1f
    eax = eax + edx;                                               // 42bdbb add eax, edx
    fl = f_cmp(edi, eax);                                          // 42bdbd cmp edi, eax
    if (fl.b()) goto L_0042bdc7;                                   // 42bdbf jb 0x42bdc7
    k.call(ld32(m, 0x916224));                                     // 42bdc1 call [0x916224]
L_0042bdc7:
    eax = edi;                                                     // 42bdc7 mov eax, edi
    eax = eax * 0xe0u;                                             // 42bdc9 imul eax, eax, 0xe0
    eax = eax + ld32(m, esi + 0xc);                                // 42bdcf add eax, [esi + 0xc]
    edi = k.pop();                                                 // 42bdd2 pop edi
    esi = k.pop();                                                 // 42bdd3 pop esi
    return k.ret(0);                                               // 42bdd4 ret 
}

// 0042c390 FUN_0042c390: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x0042c390, Vec4Cos_RangeReduced, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 42c390 push ebp
    ebp = esp;                                                     // 42c391 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 42c393 and esp, 0xfffffff0
    esp = esp - 0x50u;                                             // 42c396 sub esp, 0x50
    x3 = X::v(0x3e22f983, 0x3e22f983, 0x3e22f983, 0x3e22f983);     // 42c399 movaps xmm3, [0x92dc00]
    x1 = X::v(0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff);     // 42c3a0 movdqa xmm1, [0x92dc50]
    x3 = mulps(x3, x0);                                            // 42c3a8 mulps xmm3, xmm0
    x2 = x3;                                                       // 42c3ab movdqa xmm2, xmm3
    x2 = andps(x2, x1);                                            // 42c3af pand xmm2, xmm1
    x1 = X::v(0x4b000000, 0x4b000000, 0x4b000000, 0x4b000000);     // 42c3b3 movdqa xmm1, [0x92dc70]
    x1 = pcmpgtd(x1, x2);                                          // 42c3bb pcmpgtd xmm1, xmm2
    x2 = cvtps2dq(x3);                                             // 42c3bf cvtps2dq xmm2, xmm3
    x2 = cvtdq2ps(x2);                                             // 42c3c3 cvtdq2ps xmm2, xmm2
    x2 = andps(x2, x1);                                            // 42c3c6 andps xmm2, xmm1
    x1 = andnps(x1, x3);                                           // 42c3c9 pandn xmm1, xmm3
    x2 = orps(x2, x1);                                             // 42c3cd orps xmm2, xmm1
    x1 = X::v(0x40c90fdb, 0x40c90fdb, 0x40c90fdb, 0x40c90fdb);     // 42c3d0 movaps xmm1, [0x92dbf0]
    x2 = mulps(x2, x1);                                            // 42c3d7 mulps xmm2, xmm1
    x0 = subps(x0, x2);                                            // 42c3da subps xmm0, xmm2
    x1 = x0;                                                       // 42c3dd movaps xmm1, xmm0
    x1 = mulps(x1, x0);                                            // 42c3e0 mulps xmm1, xmm0
    x2 = x1;                                                       // 42c3e3 movaps xmm2, xmm1
    x2 = mulps(x2, x0);                                            // 42c3e6 mulps xmm2, xmm0
    x3 = x1;                                                       // 42c3e9 movaps xmm3, xmm1
    x3 = mulps(x3, x1);                                            // 42c3ec mulps xmm3, xmm1
    st(m, esp, x0);                                                // 42c3ef movaps [esp], xmm0
    x4 = x2;                                                       // 42c3f3 movaps xmm4, xmm2
    x4 = mulps(x4, x1);                                            // 42c3f6 mulps xmm4, xmm1
    x0 = x4;                                                       // 42c3f9 movaps xmm0, xmm4
    x0 = mulps(x0, x3);                                            // 42c3fc mulps xmm0, xmm3
    st(m, esp + 0x40, x0);                                         // 42c3ff movaps [esp + 0x40], xmm0
    x0 = x4;                                                       // 42c404 movaps xmm0, xmm4
    x0 = mulps(x0, x4);                                            // 42c407 mulps xmm0, xmm4
    st(m, esp + 0x10, x0);                                         // 42c40a movaps [esp + 0x10], xmm0
    x5 = x2;                                                       // 42c40f movaps xmm5, xmm2
    x5 = mulps(x5, x2);                                            // 42c412 mulps xmm5, xmm2
    x0 = x5;                                                       // 42c415 movaps xmm0, xmm5
    x0 = mulps(x0, x4);                                            // 42c418 mulps xmm0, xmm4
    st(m, esp + 0x20, x0);                                         // 42c41b movaps [esp + 0x20], xmm0
    x0 = x5;                                                       // 42c420 movaps xmm0, xmm5
    x0 = mulps(x0, x5);                                            // 42c423 mulps xmm0, xmm5
    st(m, esp + 0x30, x0);                                         // 42c426 movaps [esp + 0x30], xmm0
    x0 = X::ss(0xbf000000u);                                       // 42c42b movss xmm0, [0x92db24]
    x0 = shufps(x0, x0, 0x00);                                     // 42c433 shufps xmm0, xmm0, 0
    x0 = mulps(x0, x1);                                            // 42c437 mulps xmm0, xmm1
    x1 = X::v(0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000);     // 42c43a movaps xmm1, [0x92dba0]
    x0 = addps(x0, x1);                                            // 42c441 addps xmm0, xmm1
    x1 = X::ss(0x3d2aaaabu);                                       // 42c444 movss xmm1, [0x92db28]
    x1 = shufps(x1, x1, 0x00);                                     // 42c44c shufps xmm1, xmm1, 0
    x1 = mulps(x1, x3);                                            // 42c450 mulps xmm1, xmm3
    x0 = addps(x0, x1);                                            // 42c453 addps xmm0, xmm1
    x1 = X::ss(0xbab60b61u);                                       // 42c456 movss xmm1, [0x92db2c]
    x1 = shufps(x1, x1, 0x00);                                     // 42c45e shufps xmm1, xmm1, 0
    x1 = mulps(x1, x5);                                            // 42c462 mulps xmm1, xmm5
    x0 = addps(x0, x1);                                            // 42c465 addps xmm0, xmm1
    x1 = X::ss(0x37d00d01u);                                       // 42c468 movss xmm1, [0x92db30]
    eax = ld32(m, ebp + 0x8);                                      // 42c470 mov eax, [ebp + 8]
    x1 = shufps(x1, x1, 0x00);                                     // 42c473 shufps xmm1, xmm1, 0
    x7 = x3;                                                       // 42c477 movaps xmm7, xmm3
    x7 = mulps(x7, x3);                                            // 42c47a mulps xmm7, xmm3
    x1 = mulps(x1, x7);                                            // 42c47d mulps xmm1, xmm7
    x0 = addps(x0, x1);                                            // 42c480 addps xmm0, xmm1
    x1 = X::ss(0xb493f27eu);                                       // 42c483 movss xmm1, [0x92db34]
    x1 = shufps(x1, x1, 0x00);                                     // 42c48b shufps xmm1, xmm1, 0
    x1 = mulps(x1, ld(m, esp + 0x10));                             // 42c48f mulps xmm1, [esp + 0x10]
    x0 = addps(x0, x1);                                            // 42c494 addps xmm0, xmm1
    x1 = X::ss(0x310f76c8u);                                       // 42c497 movss xmm1, [0x92db38]
    x6 = x3;                                                       // 42c49f movaps xmm6, xmm3
    x1 = shufps(x1, x1, 0x00);                                     // 42c4a2 shufps xmm1, xmm1, 0
    x1 = mulps(x1, ld(m, esp + 0x30));                             // 42c4a6 mulps xmm1, [esp + 0x30]
    x0 = addps(x0, x1);                                            // 42c4ab addps xmm0, xmm1
    x1 = X::ss(0xad49cba5u);                                       // 42c4ae movss xmm1, [0x92db3c]
    x6 = mulps(x6, x2);                                            // 42c4b6 mulps xmm6, xmm2
    x1 = shufps(x1, x1, 0x00);                                     // 42c4b9 shufps xmm1, xmm1, 0
    x3 = x6;                                                       // 42c4bd movaps xmm3, xmm6
    x3 = mulps(x3, x6);                                            // 42c4c0 mulps xmm3, xmm6
    x3 = mulps(x3, x1);                                            // 42c4c3 mulps xmm3, xmm1
    x1 = X::ss(0x29573f9fu);                                       // 42c4c6 movss xmm1, [0x92db40]
    x0 = addps(x0, x3);                                            // 42c4ce addps xmm0, xmm3
    x3 = x7;                                                       // 42c4d1 movaps xmm3, xmm7
    x1 = shufps(x1, x1, 0x00);                                     // 42c4d4 shufps xmm1, xmm1, 0
    x3 = mulps(x3, x7);                                            // 42c4d8 mulps xmm3, xmm7
    x3 = mulps(x3, x1);                                            // 42c4db mulps xmm3, xmm1
    x1 = ld(m, esp + 0x40);                                        // 42c4de movaps xmm1, [esp + 0x40]
    x0 = addps(x0, x3);                                            // 42c4e3 addps xmm0, xmm3
    x3 = x1;                                                       // 42c4e6 movaps xmm3, xmm1
    x3 = mulps(x3, x1);                                            // 42c4e9 mulps xmm3, xmm1
    x1 = X::ss(0xa53413c3u);                                       // 42c4ec movss xmm1, [0x92db44]
    x1 = shufps(x1, x1, 0x00);                                     // 42c4f4 shufps xmm1, xmm1, 0
    x3 = mulps(x3, x1);                                            // 42c4f8 mulps xmm3, xmm1
    x1 = ld(m, esp + 0x10);                                        // 42c4fb movaps xmm1, [esp + 0x10]
    x0 = addps(x0, x3);                                            // 42c500 addps xmm0, xmm3
    x3 = x1;                                                       // 42c503 movaps xmm3, xmm1
    x3 = mulps(x3, x1);                                            // 42c506 mulps xmm3, xmm1
    x1 = X::ss(0x20f2a15du);                                       // 42c509 movss xmm1, [0x92db48]
    x1 = shufps(x1, x1, 0x00);                                     // 42c511 shufps xmm1, xmm1, 0
    x3 = mulps(x3, x1);                                            // 42c515 mulps xmm3, xmm1
    x1 = ld(m, esp + 0x20);                                        // 42c518 movaps xmm1, [esp + 0x20]
    x0 = addps(x0, x3);                                            // 42c51d addps xmm0, xmm3
    x3 = x1;                                                       // 42c520 movaps xmm3, xmm1
    x3 = mulps(x3, x1);                                            // 42c523 mulps xmm3, xmm1
    x1 = X::ss(0x9c8671cbu);                                       // 42c526 movss xmm1, [0x92db4c]
    x1 = shufps(x1, x1, 0x00);                                     // 42c52e shufps xmm1, xmm1, 0
    x3 = mulps(x3, x1);                                            // 42c532 mulps xmm3, xmm1
    x1 = X::ss(0xbe2aaaabu);                                       // 42c535 movss xmm1, [0x92daf4]
    x1 = shufps(x1, x1, 0x00);                                     // 42c53d shufps xmm1, xmm1, 0
    x1 = mulps(x1, x2);                                            // 42c541 mulps xmm1, xmm2
    x1 = addps(x1, ld(m, esp));                                    // 42c544 addps xmm1, [esp]
    x2 = X::ss(0x3c088889u);                                       // 42c548 movss xmm2, [0x92daf8]
    x2 = shufps(x2, x2, 0x00);                                     // 42c550 shufps xmm2, xmm2, 0
    x2 = mulps(x2, x4);                                            // 42c554 mulps xmm2, xmm4
    x1 = addps(x1, x2);                                            // 42c557 addps xmm1, xmm2
    x2 = X::ss(0xb9500d01u);                                       // 42c55a movss xmm2, [0x92dafc]
    x4 = ld(m, esp + 0x20);                                        // 42c562 movaps xmm4, [esp + 0x20]
    x2 = shufps(x2, x2, 0x00);                                     // 42c567 shufps xmm2, xmm2, 0
    x2 = mulps(x2, x6);                                            // 42c56b mulps xmm2, xmm6
    x1 = addps(x1, x2);                                            // 42c56e addps xmm1, xmm2
    x2 = X::ss(0x3638ef1du);                                       // 42c571 movss xmm2, [0x92db00]
    x2 = shufps(x2, x2, 0x00);                                     // 42c579 shufps xmm2, xmm2, 0
    x0 = addps(x0, x3);                                            // 42c57d addps xmm0, xmm3
    x3 = ld(m, esp + 0x40);                                        // 42c580 movaps xmm3, [esp + 0x40]
    x2 = mulps(x2, x3);                                            // 42c585 mulps xmm2, xmm3
    x1 = addps(x1, x2);                                            // 42c588 addps xmm1, xmm2
    x2 = X::ss(0xb2d7322bu);                                       // 42c58b movss xmm2, [0x92db04]
    x2 = shufps(x2, x2, 0x00);                                     // 42c593 shufps xmm2, xmm2, 0
    x2 = mulps(x2, x4);                                            // 42c597 mulps xmm2, xmm4
    x1 = addps(x1, x2);                                            // 42c59a addps xmm1, xmm2
    x2 = x6;                                                       // 42c59d movaps xmm2, xmm6
    x2 = mulps(x2, x5);                                            // 42c5a0 mulps xmm2, xmm5
    st(m, esp, x2);                                                // 42c5a3 movaps [esp], xmm2
    x2 = X::ss(0x2f309231u);                                       // 42c5a7 movss xmm2, [0x92db08]
    x5 = ld(m, esp);                                               // 42c5af movaps xmm5, [esp]
    x2 = shufps(x2, x2, 0x00);                                     // 42c5b3 shufps xmm2, xmm2, 0
    x5 = mulps(x5, x2);                                            // 42c5b7 mulps xmm5, xmm2
    x2 = X::ss(0xab573f9fu);                                       // 42c5ba movss xmm2, [0x92db0c]
    x1 = addps(x1, x5);                                            // 42c5c2 addps xmm1, xmm5
    x2 = shufps(x2, x2, 0x00);                                     // 42c5c5 shufps xmm2, xmm2, 0
    x5 = x7;                                                       // 42c5c9 movaps xmm5, xmm7
    x5 = mulps(x5, x6);                                            // 42c5cc mulps xmm5, xmm6
    x5 = mulps(x5, x2);                                            // 42c5cf mulps xmm5, xmm2
    x2 = X::ss(0x274a963cu);                                       // 42c5d2 movss xmm2, [0x92db10]
    x1 = addps(x1, x5);                                            // 42c5da addps xmm1, xmm5
    x2 = shufps(x2, x2, 0x00);                                     // 42c5dd shufps xmm2, xmm2, 0
    x5 = x3;                                                       // 42c5e1 movaps xmm5, xmm3
    x5 = mulps(x5, x7);                                            // 42c5e4 mulps xmm5, xmm7
    x5 = mulps(x5, x2);                                            // 42c5e7 mulps xmm5, xmm2
    x2 = X::ss(0xa317a4dau);                                       // 42c5ea movss xmm2, [0x92db14]
    x1 = addps(x1, x5);                                            // 42c5f2 addps xmm1, xmm5
    x5 = ld(m, esp + 0x10);                                        // 42c5f5 movaps xmm5, [esp + 0x10]
    x2 = shufps(x2, x2, 0x00);                                     // 42c5fa shufps xmm2, xmm2, 0
    x6 = x5;                                                       // 42c5fe movaps xmm6, xmm5
    x6 = mulps(x6, x3);                                            // 42c601 mulps xmm6, xmm3
    x6 = mulps(x6, x2);                                            // 42c604 mulps xmm6, xmm2
    x2 = X::ss(0x1eb8dc78u);                                       // 42c607 movss xmm2, [0x92db18]
    x2 = shufps(x2, x2, 0x00);                                     // 42c60f shufps xmm2, xmm2, 0
    x3 = x4;                                                       // 42c613 movaps xmm3, xmm4
    x3 = mulps(x3, x5);                                            // 42c616 mulps xmm3, xmm5
    x3 = mulps(x3, x2);                                            // 42c619 mulps xmm3, xmm2
    x2 = ld(m, esp + 0x30);                                        // 42c61c movaps xmm2, [esp + 0x30]
    x1 = addps(x1, x6);                                            // 42c621 addps xmm1, xmm6
    x1 = addps(x1, x3);                                            // 42c624 addps xmm1, xmm3
    x3 = X::ss(0x9a3b0da1u);                                       // 42c627 movss xmm3, [0x92db1c]
    x2 = mulps(x2, x4);                                            // 42c62f mulps xmm2, xmm4
    x3 = shufps(x3, x3, 0x00);                                     // 42c632 shufps xmm3, xmm3, 0
    x2 = mulps(x2, x3);                                            // 42c636 mulps xmm2, xmm3
    x1 = addps(x1, x2);                                            // 42c639 addps xmm1, xmm2
    st(m, eax, x1);                                                // 42c63c movaps [eax], xmm1
    st(m, ecx, x0);                                                // 42c63f movaps [ecx], xmm0
    esp = ebp;                                                     // 42c642 mov esp, ebp
    ebp = k.pop();                                                 // 42c644 pop ebp
    return k.ret(0);                                               // 42c645 ret 
}

// 0042caf0 FUN_0042caf0: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x0042caf0, Vec4_SplatLane, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 42caf0 push ebp
    ebp = esp;                                                     // 42caf1 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 42caf3 and esp, 0xfffffff0
    esp = esp - 0x30u;                                             // 42caf6 sub esp, 0x30
    eax = ld32(m, 0x9b45e4);                                       // 42caf9 mov eax, [0x9b45e4]
    eax = eax ^ esp;                                               // 42cafe xor eax, esp
    st32(m, esp + 0x2c, eax);                                      // 42cb00 mov [esp + 0x2c], eax
    st(m, esp + 0x10, x0);                                         // 42cb04 movaps [esp + 0x10], xmm0
    eax = ld32(m, ebp + 0x8);                                      // 42cb09 mov eax, [ebp + 8]
    ecx = ld32(m, esp + 0x2c);                                     // 42cb0c mov ecx, [esp + 0x2c]
    eax = ld32(m, esp + eax * 4 + 0x10);                           // 42cb10 mov eax, [esp + eax*4 + 0x10]
    st32(m, esp, eax);                                             // 42cb14 mov [esp], eax
    st32(m, esp + 0x4, eax);                                       // 42cb17 mov [esp + 4], eax
    st32(m, esp + 0x8, eax);                                       // 42cb1b mov [esp + 8], eax
    st32(m, esp + 0xc, eax);                                       // 42cb1f mov [esp + 0xc], eax
    x0 = ld(m, esp);                                               // 42cb23 movaps xmm0, [esp]
    ecx = ecx ^ esp;                                               // 42cb27 xor ecx, esp
    k.call(0x008f9efe);  // __security_check_cookie                // 42cb29 call 0x8f9efe
    esp = ebp;                                                     // 42cb2e mov esp, ebp
    ebp = k.pop();                                                 // 42cb30 pop ebp
    return k.ret(0);                                               // 42cb31 ret 
}

// 0042d670 FUN_0042d670: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x0042d670, Fn_0042d670, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    x0 = X::ss(0x7f7fffffu);                                       // 42d670 movss xmm0, [0x977494]
    fl = f_comiss(x1.f(0), x0.f(0));                               // 42d678 ucomiss xmm1, xmm0
    eax = (eax & 0xffff00ffu) | (fl.lahf() << 8);                  // 42d67b lahf 
    fl = f_test8(((eax >> 8) & 0xff), 0x44u);                      // 42d67c test ah, 0x44
    if (fl.np()) goto L_0042d685;                                  // 42d67f jnp 0x42d685
    st32(m, ecx, x1.u[0]);                                         // 42d681 movss [ecx], xmm1
L_0042d685:
    x1 = X::ss(ld32(m, esp + 0x4));                                // 42d685 movss xmm1, [esp + 4]
    fl = f_comiss(x1.f(0), x0.f(0));                               // 42d68b ucomiss xmm1, xmm0
    eax = (eax & 0xffff00ffu) | (fl.lahf() << 8);                  // 42d68e lahf 
    fl = f_test8(((eax >> 8) & 0xff), 0x44u);                      // 42d68f test ah, 0x44
    if (fl.np()) goto L_0042d6a6;                                  // 42d692 jnp 0x42d6a6
    x0 = X::ss(0x3f800000u);                                       // 42d694 movss xmm0, [0x931f24]
    st32(m, ecx + 0x4, x1.u[0]);                                   // 42d69c movss [ecx + 4], xmm1
    st32(m, ecx + 0x8, x0.u[0]);                                   // 42d6a1 movss [ecx + 8], xmm0
L_0042d6a6:
    return k.ret(4);                                               // 42d6a6 ret 4
}

// 0042d6b0 FUN_0042d6b0: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x0042d6b0, Fn_0042d6b0, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    x1 = X::ss(ld32(m, eax + 0x4));                                // 42d6b0 movss xmm1, [eax + 4]
    x0 = x1;                                                       // 42d6b5 movaps xmm0, xmm1
    fl = f_comiss(x0.f(0), x2.f(0));                               // 42d6b8 comiss xmm0, xmm2
    if (fl.be()) goto L_0042d6c0;                                  // 42d6bb jbe 0x42d6c0
    x0 = x2;                                                       // 42d6bd movaps xmm0, xmm2
L_0042d6c0:
    x2 = X::ss(0x3f800000u);                                       // 42d6c0 movss xmm2, [0x931f24]
    x2 = divss(x2, x1);                                            // 42d6c8 divss xmm2, xmm1
    x0 = mulss(x0, x2);                                            // 42d6cc mulss xmm0, xmm2
    x2 = X::ss(ld32(m, eax));                                      // 42d6d0 movss xmm2, [eax]
    x1 = x0;                                                       // 42d6d4 movaps xmm1, xmm0
    x0 = X::ss(ld32(m, esp + 0x4));                                // 42d6d7 movss xmm0, [esp + 4]
    x0 = subss(x0, x2);                                            // 42d6dd subss xmm0, xmm2
    x0 = mulss(x0, x1);                                            // 42d6e1 mulss xmm0, xmm1
    x0 = addss(x0, x2);                                            // 42d6e5 addss xmm0, xmm2
    st32(m, eax + 0x8, x1.u[0]);                                   // 42d6e9 movss [eax + 8], xmm1
    st32(m, eax, x0.u[0]);                                         // 42d6ee movss [eax], xmm0
    return k.ret(4);                                               // 42d6f2 ret 4
}

// 0042de70 FUN_0042de70: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x0042de70, Fn_0042de70, "math", reflex_port::OUT_EAX) {  // outputs: what callers read (it calls other groups' code)
    Cpu k(m);
    CPU_REFS(k);
    k.push(ebp);                                                   // 42de70 push ebp
    ebp = esp;                                                     // 42de71 mov ebp, esp
    esp = esp & 0xfffffff0u;                                       // 42de73 and esp, 0xfffffff0
    esp = esp - 0x6cu;                                             // 42de76 sub esp, 0x6c
    k.push(edi);                                                   // 42de79 push edi
    x4 = ld(m, esi);                                               // 42de7a movaps xmm4, [esi]
    eax = ld32(m, ebp + 0x8);                                      // 42de7d mov eax, [ebp + 8]
    x0 = ld(m, eax);                                               // 42de80 movaps xmm0, [eax]
    x6 = X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000);     // 42de83 movaps xmm6, [0x93a210]
    x7 = X::v(0xbf800000, 0xbf800000, 0xbf800000, 0x3f800000);     // 42de8a movaps xmm7, [0x93a220]
    x1 = x4;                                                       // 42de91 movaps xmm1, xmm4
    x1 = shufps(x1, x4, 0x1b);                                     // 42de94 shufps xmm1, xmm4, 0x1b
    x2 = x0;                                                       // 42de98 movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0x00);                                     // 42de9b shufps xmm2, xmm0, 0
    x2 = mulps(x2, x1);                                            // 42de9f mulps xmm2, xmm1
    x2 = mulps(x2, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 42dea2 mulps xmm2, [0x93a1f0]
    x1 = shufps(x1, x1, 0xb1);                                     // 42dea9 shufps xmm1, xmm1, 0xb1
    x3 = x0;                                                       // 42dead movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0x55);                                     // 42deb0 shufps xmm3, xmm0, 0x55
    x3 = mulps(x3, x1);                                            // 42deb4 mulps xmm3, xmm1
    x3 = mulps(x3, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 42deb7 mulps xmm3, [0x93a200]
    x5 = x0;                                                       // 42debe movaps xmm5, xmm0
    x5 = shufps(x5, x0, 0xaa);                                     // 42dec1 shufps xmm5, xmm0, 0xaa
    x1 = shufps(x1, x1, 0x1b);                                     // 42dec5 shufps xmm1, xmm1, 0x1b
    x1 = mulps(x1, x5);                                            // 42dec9 mulps xmm1, xmm5
    x1 = mulps(x1, x6);                                            // 42decc mulps xmm1, xmm6
    x1 = addps(x1, x3);                                            // 42decf addps xmm1, xmm3
    x3 = x0;                                                       // 42ded2 movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0xff);                                     // 42ded5 shufps xmm3, xmm0, 0xff
    x3 = mulps(x3, x4);                                            // 42ded9 mulps xmm3, xmm4
    x4 = ld(m, ecx);                                               // 42dedc movaps xmm4, [ecx]
    x3 = addps(x3, x2);                                            // 42dedf addps xmm3, xmm2
    x1 = addps(x1, x3);                                            // 42dee2 addps xmm1, xmm3
    x1 = mulps(x1, x7);                                            // 42dee5 mulps xmm1, xmm7
    x0 = x4;                                                       // 42dee8 movaps xmm0, xmm4
    x0 = shufps(x0, x4, 0x1b);                                     // 42deeb shufps xmm0, xmm4, 0x1b
    x2 = x1;                                                       // 42deef movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0x00);                                     // 42def2 shufps xmm2, xmm1, 0
    x2 = mulps(x2, x0);                                            // 42def6 mulps xmm2, xmm0
    x2 = mulps(x2, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 42def9 mulps xmm2, [0x93a1f0]
    x0 = shufps(x0, x0, 0xb1);                                     // 42df00 shufps xmm0, xmm0, 0xb1
    x3 = x1;                                                       // 42df04 movaps xmm3, xmm1
    x3 = shufps(x3, x1, 0x55);                                     // 42df07 shufps xmm3, xmm1, 0x55
    x3 = mulps(x3, x0);                                            // 42df0b mulps xmm3, xmm0
    x3 = mulps(x3, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 42df0e mulps xmm3, [0x93a200]
    x5 = x1;                                                       // 42df15 movaps xmm5, xmm1
    x5 = shufps(x5, x1, 0xaa);                                     // 42df18 shufps xmm5, xmm1, 0xaa
    x0 = shufps(x0, x0, 0x1b);                                     // 42df1c shufps xmm0, xmm0, 0x1b
    x0 = mulps(x0, x5);                                            // 42df20 mulps xmm0, xmm5
    x0 = mulps(x0, x6);                                            // 42df23 mulps xmm0, xmm6
    x0 = addps(x0, x3);                                            // 42df26 addps xmm0, xmm3
    x3 = x1;                                                       // 42df29 movaps xmm3, xmm1
    x3 = shufps(x3, x1, 0xff);                                     // 42df2c shufps xmm3, xmm1, 0xff
    x1 = X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000);     // 42df30 movaps xmm1, [0x92db80]
    x3 = mulps(x3, x4);                                            // 42df37 mulps xmm3, xmm4
    x3 = addps(x3, x2);                                            // 42df3a addps xmm3, xmm2
    x2 = ld(m, 0xd61680);                                          // 42df3d movaps xmm2, [0xd61680]
    x1 = andps(x1, x2);                                            // 42df44 andps xmm1, xmm2
    x0 = addps(x0, x3);                                            // 42df47 addps xmm0, xmm3
    x5 = x0;                                                       // 42df4a movaps xmm5, xmm0
    x5 = mulps(x5, x7);                                            // 42df4d mulps xmm5, xmm7
    x3 = x1;                                                       // 42df50 movaps xmm3, xmm1
    x3 = shufps(x3, x1, 0x00);                                     // 42df53 shufps xmm3, xmm1, 0
    x2 = x5;                                                       // 42df57 movaps xmm2, xmm5
    x2 = shufps(x2, x5, 0x1b);                                     // 42df5a shufps xmm2, xmm5, 0x1b
    x3 = mulps(x3, x2);                                            // 42df5e mulps xmm3, xmm2
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 42df61 mulps xmm3, [0x93a1f0]
    x2 = shufps(x2, x2, 0xb1);                                     // 42df68 shufps xmm2, xmm2, 0xb1
    x4 = x1;                                                       // 42df6c movaps xmm4, xmm1
    x4 = shufps(x4, x1, 0x55);                                     // 42df6f shufps xmm4, xmm1, 0x55
    x4 = mulps(x4, x2);                                            // 42df73 mulps xmm4, xmm2
    x4 = mulps(x4, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 42df76 mulps xmm4, [0x93a200]
    x2 = shufps(x2, x2, 0x1b);                                     // 42df7d shufps xmm2, xmm2, 0x1b
    x7 = x1;                                                       // 42df81 movaps xmm7, xmm1
    x7 = shufps(x7, x1, 0xaa);                                     // 42df84 shufps xmm7, xmm1, 0xaa
    x2 = mulps(x2, x7);                                            // 42df88 mulps xmm2, xmm7
    x2 = mulps(x2, x6);                                            // 42df8b mulps xmm2, xmm6
    x2 = addps(x2, x4);                                            // 42df8e addps xmm2, xmm4
    x4 = x1;                                                       // 42df91 movaps xmm4, xmm1
    x4 = shufps(x4, x1, 0xff);                                     // 42df94 shufps xmm4, xmm1, 0xff
    x4 = mulps(x4, x5);                                            // 42df98 mulps xmm4, xmm5
    x4 = addps(x4, x3);                                            // 42df9b addps xmm4, xmm3
    x2 = addps(x2, x4);                                            // 42df9e addps xmm2, xmm4
    st(m, esp + 0x20, x5);                                         // 42dfa1 movaps [esp + 0x20], xmm5
    x1 = x2;                                                       // 42dfa6 movaps xmm1, xmm2
    x3 = x0;                                                       // 42dfa9 movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0x00);                                     // 42dfac shufps xmm3, xmm0, 0
    st(m, esp + 0x50, x3);                                         // 42dfb0 movaps [esp + 0x50], xmm3
    x1 = shufps(x1, x2, 0x1b);                                     // 42dfb5 shufps xmm1, xmm2, 0x1b
    x3 = mulps(x3, x1);                                            // 42dfb9 mulps xmm3, xmm1
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 42dfbc mulps xmm3, [0x93a1f0]
    x1 = shufps(x1, x1, 0xb1);                                     // 42dfc3 shufps xmm1, xmm1, 0xb1
    x4 = x0;                                                       // 42dfc7 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x55);                                     // 42dfca shufps xmm4, xmm0, 0x55
    x5 = x0;                                                       // 42dfce movaps xmm5, xmm0
    x5 = shufps(x5, x0, 0xaa);                                     // 42dfd1 shufps xmm5, xmm0, 0xaa
    st(m, esp + 0x30, x4);                                         // 42dfd5 movaps [esp + 0x30], xmm4
    x0 = shufps(x0, x0, 0xff);                                     // 42dfda shufps xmm0, xmm0, 0xff
    x7 = x1;                                                       // 42dfde movaps xmm7, xmm1
    x7 = shufps(x7, x1, 0x1b);                                     // 42dfe1 shufps xmm7, xmm1, 0x1b
    x7 = mulps(x7, x5);                                            // 42dfe5 mulps xmm7, xmm5
    x4 = mulps(x4, x1);                                            // 42dfe8 mulps xmm4, xmm1
    x4 = mulps(x4, X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000)); // 42dfeb mulps xmm4, [0x93a200]
    st(m, esp + 0x60, x0);                                         // 42dff2 movaps [esp + 0x60], xmm0
    x7 = mulps(x7, x6);                                            // 42dff7 mulps xmm7, xmm6
    x0 = mulps(x0, x2);                                            // 42dffa mulps xmm0, xmm2
    x7 = addps(x7, x4);                                            // 42dffd addps xmm7, xmm4
    x3 = addps(x3, x0);                                            // 42e000 addps xmm3, xmm0
    x7 = addps(x7, x3);                                            // 42e003 addps xmm7, xmm3
    st(m, esp + 0x10, x7);                                         // 42e006 movaps [esp + 0x10], xmm7
    fpush(m, reflex_f80::from_f32(ld32(m, esp + 0x10)));           // 42e00b fld [esp + 0x10]
    fpush(m, reflex_f80::from_f32(ld32(m, esp + 0x14)));           // 42e00f fld [esp + 0x14]
    st(m, esp + 0x40, x5);                                         // 42e013 movaps [esp + 0x40], xmm5
    k.call(0x009009c0);  // MSVCR90.DLL::_CIatan2                  // 42e018 call 0x9009c0
    st32(m, esp + 0x8, reflex_f80::to_f32(fst_(m, 0), frc(m))); m.fpu.pop_back(); // 42e01d fstp [esp + 8]
    fpush(m, reflex_f80::from_f32(ld32(m, esp + 0x8)));            // 42e021 fld [esp + 8]
    x2 = X::ss(0x3fc90fdbu);                                       // 42e025 movss xmm2, [0x977508]
    st32(m, esp + 0x8, reflex_f80::to_f32(fst_(m, 0), frc(m))); m.fpu.pop_back(); // 42e02d fstp [esp + 8]
    fpush(m, reflex_f80::from_int(0));                             // 42e031 fldz 
    x0 = X::ss(ld32(m, esp + 0x8));                                // 42e033 movss xmm0, [esp + 8]
    fpush(m, reflex_f80::from_f32(ld32(m, esp + 0x8)));            // 42e039 fld [esp + 8]
    fl = f_fcomi(fst_(m, 0), fst_(m, 1)); m.fpu.pop_back();        // 42e03d fcompi st(1)
    fst_(m, 0) = fst_(m, 0); m.fpu.pop_back();                     // 42e03f fstp st(0)
    if (fl.b()) goto L_0042e048;                                   // 42e041 jb 0x42e048
    fl = f_comiss(x2.f(0), x0.f(0));                               // 42e043 comiss xmm2, xmm0
    if (fl.ae()) goto L_0042e086;                                  // 42e046 jae 0x42e086
L_0042e048:
    fl = f_comiss(x0.f(0), x2.f(0));                               // 42e048 comiss xmm0, xmm2
    if (fl.be()) goto L_0042e05d;                                  // 42e04b jbe 0x42e05d
    x3 = X::ss(ld32(m, esi + 0x14));                               // 42e04d movss xmm3, [esi + 0x14]
    x1 = X::ss(ld32(m, esi + 0x10));                               // 42e052 movss xmm1, [esi + 0x10]
    x0 = subss(x0, x2);                                            // 42e057 subss xmm0, xmm2
    goto L_0042e090;                                               // 42e05b jmp 0x42e090
L_0042e05d:
    fl = f_comiss(x0.f(0), flt(0xbfc90fdbu));                      // 42e05d comiss xmm0, [0x977520]
    if (fl.b()) goto L_0042e07e;                                   // 42e064 jb 0x42e07e
    x1 = X{};                                                      // 42e066 xorps xmm1, xmm1
    fl = f_comiss(x1.f(0), x0.f(0));                               // 42e069 comiss xmm1, xmm0
    if (fl.b()) goto L_0042e07e;                                   // 42e06c jb 0x42e07e
    x3 = X::ss(ld32(m, esi + 0x14));                               // 42e06e movss xmm3, [esi + 0x14]
    x1 = X::ss(ld32(m, esi + 0x10));                               // 42e073 movss xmm1, [esi + 0x10]
    x0 = addss(x0, x2);                                            // 42e078 addss xmm0, xmm2
    goto L_0042e090;                                               // 42e07c jmp 0x42e090
L_0042e07e:
    x0 = addss(x0, X::ss(0x40490fdbu));                            // 42e07e addss xmm0, [0x977498]
L_0042e086:
    x3 = X::ss(ld32(m, esi + 0x10));                               // 42e086 movss xmm3, [esi + 0x10]
    x1 = X::ss(ld32(m, esi + 0x14));                               // 42e08b movss xmm1, [esi + 0x14]
L_0042e090:
    x1 = subss(x1, x3);                                            // 42e090 subss xmm1, xmm3
    x1 = mulss(x1, x0);                                            // 42e094 mulss xmm1, xmm0
    x0 = X::ss(ld32(m, esp + 0x18));                               // 42e098 movss xmm0, [esp + 0x18]
    x1 = divss(x1, x2);                                            // 42e09e divss xmm1, xmm2
    x1 = addss(x1, x3);                                            // 42e0a2 addss xmm1, xmm3
    st32(m, esp + 0xc, x1.u[0]);                                   // 42e0a6 movss [esp + 0xc], xmm1
    k.call(0x00401040);  // FUN_00401040                           // 42e0ac call 0x401040
    x2 = sd(0x3ff0000000000000ull);                                // 42e0b1 movsd xmm2, [0x977638]
    x4 = x0;                                                       // 42e0b9 movaps xmm4, xmm0
    fl = f_comiss(x4.f(0), flt(ld32(m, 0x9ddfc8)));                // 42e0bc comiss xmm4, [0x9ddfc8]
    st32(m, esp + 0x8, x4.u[0]);                                   // 42e0c3 movss [esp + 8], xmm4
    if (fl.b()) goto L_0042e110;                                   // 42e0c9 jb 0x42e110
    x0 = cvtss2sd(x0, X::ss(ld32(m, esp + 0x18)));                 // 42e0cb cvtss2sd xmm0, [esp + 0x18]
    x1 = x0;                                                       // 42e0d1 movapd xmm1, xmm0
    x1 = mulsd(x1, x0);                                            // 42e0d5 mulsd xmm1, xmm0
    x0 = cvtsd2ss(x0, x1);                                         // 42e0d9 cvtsd2ss xmm0, xmm1
    x0 = cvtss2sd(x0, x0);                                         // 42e0dd cvtss2sd xmm0, xmm0
    x1 = x2;                                                       // 42e0e1 movapd xmm1, xmm2
    x1 = subsd(x1, x0);                                            // 42e0e5 subsd xmm1, xmm0
    x0 = cvtsd2ss(x0, x1);                                         // 42e0e9 cvtsd2ss xmm0, xmm1
    k.call(0x00401040);  // FUN_00401040                           // 42e0ed call 0x401040
    k.push(ecx);                                                   // 42e0f2 push ecx
    st32(m, esp, x0.u[0]);                                         // 42e0f3 movss [esp], xmm0
    k.call(0x0042c230);  // FUN_0042c230                           // 42e0f8 call 0x42c230
    x4 = X::ss(ld32(m, esp + 0xc));                                // 42e0fd movss xmm4, [esp + 0xc]
    x2 = sd(0x3ff0000000000000ull);                                // 42e103 movsd xmm2, [0x977638]
    esp = esp + 0x4u;                                              // 42e10b add esp, 4
    goto L_0042e113;                                               // 42e10e jmp 0x42e113
L_0042e110:
    x0 = x4;                                                       // 42e110 movaps xmm0, xmm4
L_0042e113:
    fl = f_comiss(x4.f(0), flt(ld32(m, 0x9ddfc8)));                // 42e113 comiss xmm4, [0x9ddfc8]
    x6 = sd(0x3fe212eb00000000ull);                                // 42e11a movsd xmm6, [0x9777b0]
    x1 = cvtss2sd(x1, x0);                                         // 42e122 cvtss2sd xmm1, xmm0
    x3 = x1;                                                       // 42e126 movapd xmm3, xmm1
    x3 = mulsd(x3, x1);                                            // 42e12a mulsd xmm3, xmm1
    x1 = cvtsd2ss(x1, x3);                                         // 42e12e cvtsd2ss xmm1, xmm3
    x3 = cvtss2sd(x3, x1);                                         // 42e132 cvtss2sd xmm3, xmm1
    x5 = x3;                                                       // 42e136 movapd xmm5, xmm3
    x5 = mulsd(x5, x3);                                            // 42e13a mulsd xmm5, xmm3
    x3 = cvtsd2ss(x3, x5);                                         // 42e13e cvtsd2ss xmm3, xmm5
    x5 = cvtss2sd(x5, x1);                                         // 42e142 cvtss2sd xmm5, xmm1
    x5 = mulsd(x5, sd(0x3fa9903ac0000000ull));                     // 42e146 mulsd xmm5, [0x9777a8]
    x6 = subsd(x6, x5);                                            // 42e14e subsd xmm6, xmm5
    x5 = cvtpd2ps(x6);                                             // 42e152 cvtpd2ps xmm5, xmm6
    x5 = cvtss2sd(x5, x5);                                         // 42e156 cvtss2sd xmm5, xmm5
    x6 = cvtss2sd(x6, x3);                                         // 42e15a cvtss2sd xmm6, xmm3
    x5 = mulsd(x5, x6);                                            // 42e15e mulsd xmm5, xmm6
    x6 = cvtss2sd(x6, x1);                                         // 42e162 cvtss2sd xmm6, xmm1
    x6 = mulsd(x6, sd(0x3ff716ac60000000ull));                     // 42e166 mulsd xmm6, [0x9777a0]
    x7 = x2;                                                       // 42e16e movapd xmm7, xmm2
    x7 = subsd(x7, x6);                                            // 42e172 subsd xmm7, xmm6
    x6 = cvtsd2ss(x6, x7);                                         // 42e176 cvtsd2ss xmm6, xmm7
    x7 = sd(0x3fd98e2fe0000000ull);                                // 42e17a movsd xmm7, [0x977798]
    x6 = cvtss2sd(x6, x6);                                         // 42e182 cvtss2sd xmm6, xmm6
    x5 = addsd(x5, x6);                                            // 42e186 addsd xmm5, xmm6
    x6 = x2;                                                       // 42e18a movapd xmm6, xmm2
    x6 = divsd(x6, x5);                                            // 42e18e divsd xmm6, xmm5
    x5 = cvtpd2ps(x6);                                             // 42e192 cvtpd2ps xmm5, xmm6
    x6 = cvtss2sd(x6, x1);                                         // 42e196 cvtss2sd xmm6, xmm1
    x6 = mulsd(x6, sd(0x3f93d7f000000000ull));                     // 42e19a mulsd xmm6, [0x977790]
    x7 = subsd(x7, x6);                                            // 42e1a2 subsd xmm7, xmm6
    x1 = cvtss2sd(x1, x1);                                         // 42e1a6 cvtss2sd xmm1, xmm1
    x1 = mulsd(x1, sd(0x3ff46c01a0000000ull));                     // 42e1aa mulsd xmm1, [0x977788]
    x2 = subsd(x2, x1);                                            // 42e1b2 subsd xmm2, xmm1
    x6 = cvtpd2ps(x7);                                             // 42e1b6 cvtpd2ps xmm6, xmm7
    x6 = cvtss2sd(x6, x6);                                         // 42e1ba cvtss2sd xmm6, xmm6
    x1 = cvtpd2ps(x2);                                             // 42e1be cvtpd2ps xmm1, xmm2
    x2 = sd(0x3ff921fb60000000ull);                                // 42e1c2 movsd xmm2, [0x977510]
    x0 = cvtss2sd(x0, x0);                                         // 42e1ca cvtss2sd xmm0, xmm0
    x3 = cvtss2sd(x3, x3);                                         // 42e1ce cvtss2sd xmm3, xmm3
    x6 = mulsd(x6, x3);                                            // 42e1d2 mulsd xmm6, xmm3
    x1 = cvtss2sd(x1, x1);                                         // 42e1d6 cvtss2sd xmm1, xmm1
    x6 = addsd(x6, x1);                                            // 42e1da addsd xmm6, xmm1
    x6 = mulsd(x6, x0);                                            // 42e1de mulsd xmm6, xmm0
    x0 = cvtpd2ps(x6);                                             // 42e1e2 cvtpd2ps xmm0, xmm6
    x0 = cvtss2sd(x0, x0);                                         // 42e1e6 cvtss2sd xmm0, xmm0
    x5 = cvtss2sd(x5, x5);                                         // 42e1ea cvtss2sd xmm5, xmm5
    x5 = mulsd(x5, x0);                                            // 42e1ee mulsd xmm5, xmm0
    x0 = cvtsd2ss(x0, x5);                                         // 42e1f2 cvtsd2ss xmm0, xmm5
    if (fl.b()) goto L_0042e208;                                   // 42e1f6 jb 0x42e208
    x0 = cvtss2sd(x0, x0);                                         // 42e1f8 cvtss2sd xmm0, xmm0
    x1 = x2;                                                       // 42e1fc movapd xmm1, xmm2
    x1 = subsd(x1, x0);                                            // 42e200 subsd xmm1, xmm0
    x0 = cvtsd2ss(x0, x1);                                         // 42e204 cvtsd2ss xmm0, xmm1
L_0042e208:
    x1 = X::ss(ld32(m, esp + 0x18));                               // 42e208 movss xmm1, [esp + 0x18]
    fl = f_comiss(x1.f(0), flt(0x00000000u));                      // 42e20e comiss xmm1, [0x92e12c]
    if (fl.b()) goto L_0042e21c;                                   // 42e215 jb 0x42e21c
    x1 = x0;                                                       // 42e217 movaps xmm1, xmm0
    goto L_0042e228;                                               // 42e21a jmp 0x42e228
L_0042e21c:
    x1 = X::ss(0x80000000u);                                       // 42e21c movss xmm1, [0x9774a8]
    x1 = subss(x1, x0);                                            // 42e224 subss xmm1, xmm0
L_0042e228:
    x0 = cvtps2pd(x1);                                             // 42e228 cvtps2pd xmm0, xmm1
    x1 = x2;                                                       // 42e22b movapd xmm1, xmm2
    x1 = subsd(x1, x0);                                            // 42e22f subsd xmm1, xmm0
    x0 = cvtpd2ps(x1);                                             // 42e233 cvtpd2ps xmm0, xmm1
    x1 = X::ss(ld32(m, esp + 0xc));                                // 42e237 movss xmm1, [esp + 0xc]
    fl = f_comiss(x1.f(0), x0.f(0));                               // 42e23d comiss xmm1, xmm0
    if (fl.be()) goto L_0042e249;                                  // 42e240 jbe 0x42e249
    edi = 0x1u;                                                    // 42e242 mov edi, 1
    goto L_0042e24b;                                               // 42e247 jmp 0x42e24b
L_0042e249:
    edi = 0u;                                                      // 42e249 xor edi, edi
L_0042e24b:
    x4 = ld(m, esp + 0x20);                                        // 42e24b movaps xmm4, [esp + 0x20]
    x0 = ld(m, 0xd616a0);                                          // 42e250 movaps xmm0, [0xd616a0]
    x0 = andps(x0, X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000)); // 42e257 andps xmm0, [0x92db80]
    x6 = X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000);     // 42e25e movaps xmm6, [0x93a200]
    x2 = x0;                                                       // 42e265 movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0x00);                                     // 42e268 shufps xmm2, xmm0, 0
    x3 = x0;                                                       // 42e26c movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0x55);                                     // 42e26f shufps xmm3, xmm0, 0x55
    x1 = x4;                                                       // 42e273 movaps xmm1, xmm4
    x1 = shufps(x1, x4, 0x1b);                                     // 42e276 shufps xmm1, xmm4, 0x1b
    x2 = mulps(x2, x1);                                            // 42e27a mulps xmm2, xmm1
    x1 = shufps(x1, x1, 0xb1);                                     // 42e27d shufps xmm1, xmm1, 0xb1
    x3 = mulps(x3, x1);                                            // 42e281 mulps xmm3, xmm1
    x5 = x0;                                                       // 42e284 movaps xmm5, xmm0
    x5 = shufps(x5, x0, 0xaa);                                     // 42e287 shufps xmm5, xmm0, 0xaa
    x3 = mulps(x3, x6);                                            // 42e28b mulps xmm3, xmm6
    x1 = shufps(x1, x1, 0x1b);                                     // 42e28e shufps xmm1, xmm1, 0x1b
    x1 = mulps(x1, x5);                                            // 42e292 mulps xmm1, xmm5
    x5 = X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000);     // 42e295 movaps xmm5, [0x93a210]
    x1 = mulps(x1, x5);                                            // 42e29c mulps xmm1, xmm5
    x1 = addps(x1, x3);                                            // 42e29f addps xmm1, xmm3
    x3 = x0;                                                       // 42e2a2 movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0xff);                                     // 42e2a5 shufps xmm3, xmm0, 0xff
    x3 = mulps(x3, x4);                                            // 42e2a9 mulps xmm3, xmm4
    x4 = X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000);     // 42e2ac movaps xmm4, [0x93a1f0]
    x2 = mulps(x2, x4);                                            // 42e2b3 mulps xmm2, xmm4
    x3 = addps(x3, x2);                                            // 42e2b6 addps xmm3, xmm2
    x2 = ld(m, esp + 0x50);                                        // 42e2b9 movaps xmm2, [esp + 0x50]
    x1 = addps(x1, x3);                                            // 42e2be addps xmm1, xmm3
    x3 = ld(m, esp + 0x30);                                        // 42e2c1 movaps xmm3, [esp + 0x30]
    x0 = x1;                                                       // 42e2c6 movaps xmm0, xmm1
    x0 = shufps(x0, x1, 0x1b);                                     // 42e2c9 shufps xmm0, xmm1, 0x1b
    x2 = mulps(x2, x0);                                            // 42e2cd mulps xmm2, xmm0
    x0 = shufps(x0, x0, 0xb1);                                     // 42e2d0 shufps xmm0, xmm0, 0xb1
    x7 = x0;                                                       // 42e2d4 movaps xmm7, xmm0
    x7 = shufps(x7, x0, 0x1b);                                     // 42e2d7 shufps xmm7, xmm0, 0x1b
    x7 = mulps(x7, ld(m, esp + 0x40));                             // 42e2db mulps xmm7, [esp + 0x40]
    x3 = mulps(x3, x0);                                            // 42e2e0 mulps xmm3, xmm0
    x0 = ld(m, esp + 0x60);                                        // 42e2e3 movaps xmm0, [esp + 0x60]
    x7 = mulps(x7, x5);                                            // 42e2e8 mulps xmm7, xmm5
    x3 = mulps(x3, x6);                                            // 42e2eb mulps xmm3, xmm6
    x2 = mulps(x2, x4);                                            // 42e2ee mulps xmm2, xmm4
    x0 = mulps(x0, x1);                                            // 42e2f1 mulps xmm0, xmm1
    x7 = addps(x7, x3);                                            // 42e2f4 addps xmm7, xmm3
    x2 = addps(x2, x0);                                            // 42e2f7 addps xmm2, xmm0
    x7 = addps(x7, x2);                                            // 42e2fa addps xmm7, xmm2
    st(m, esp + 0x10, x7);                                         // 42e2fd movaps [esp + 0x10], xmm7
    fpush(m, reflex_f80::from_f32(ld32(m, esp + 0x18)));           // 42e302 fld [esp + 0x18]
    fpush(m, reflex_f80::from_f32(ld32(m, esp + 0x10)));           // 42e306 fld [esp + 0x10]
    k.call(0x009009c0);  // MSVCR90.DLL::_CIatan2                  // 42e30a call 0x9009c0
    st32(m, esp + 0x8, reflex_f80::to_f32(fst_(m, 0), frc(m))); m.fpu.pop_back(); // 42e30f fstp [esp + 8]
    fpush(m, reflex_f80::from_f32(ld32(m, esp + 0x8)));            // 42e313 fld [esp + 8]
    x1 = X::ss(0x3fc90fdbu);                                       // 42e317 movss xmm1, [0x977508]
    st32(m, esp + 0x8, reflex_f80::to_f32(fst_(m, 0), frc(m))); m.fpu.pop_back(); // 42e31f fstp [esp + 8]
    fpush(m, reflex_f80::from_int(0));                             // 42e323 fldz 
    x0 = X::ss(ld32(m, esp + 0x8));                                // 42e325 movss xmm0, [esp + 8]
    fpush(m, reflex_f80::from_f32(ld32(m, esp + 0x8)));            // 42e32b fld [esp + 8]
    fl = f_fcomi(fst_(m, 0), fst_(m, 1)); m.fpu.pop_back();        // 42e32f fcompi st(1)
    fst_(m, 0) = fst_(m, 0); m.fpu.pop_back();                     // 42e331 fstp st(0)
    if (fl.b()) goto L_0042e33a;                                   // 42e333 jb 0x42e33a
    fl = f_comiss(x1.f(0), x0.f(0));                               // 42e335 comiss xmm1, xmm0
    if (fl.ae()) goto L_0042e378;                                  // 42e338 jae 0x42e378
L_0042e33a:
    fl = f_comiss(x0.f(0), x1.f(0));                               // 42e33a comiss xmm0, xmm1
    if (fl.be()) goto L_0042e34f;                                  // 42e33d jbe 0x42e34f
    x3 = X::ss(ld32(m, esi + 0x10));                               // 42e33f movss xmm3, [esi + 0x10]
    x2 = X::ss(ld32(m, esi + 0x18));                               // 42e344 movss xmm2, [esi + 0x18]
    x0 = subss(x0, x1);                                            // 42e349 subss xmm0, xmm1
    goto L_0042e382;                                               // 42e34d jmp 0x42e382
L_0042e34f:
    fl = f_comiss(x0.f(0), flt(0xbfc90fdbu));                      // 42e34f comiss xmm0, [0x977520]
    if (fl.b()) goto L_0042e370;                                   // 42e356 jb 0x42e370
    x2 = X{};                                                      // 42e358 xorps xmm2, xmm2
    fl = f_comiss(x2.f(0), x0.f(0));                               // 42e35b comiss xmm2, xmm0
    if (fl.b()) goto L_0042e370;                                   // 42e35e jb 0x42e370
    x3 = X::ss(ld32(m, esi + 0x10));                               // 42e360 movss xmm3, [esi + 0x10]
    x2 = X::ss(ld32(m, esi + 0x18));                               // 42e365 movss xmm2, [esi + 0x18]
    x0 = addss(x0, x1);                                            // 42e36a addss xmm0, xmm1
    goto L_0042e382;                                               // 42e36e jmp 0x42e382
L_0042e370:
    x0 = addss(x0, X::ss(0x40490fdbu));                            // 42e370 addss xmm0, [0x977498]
L_0042e378:
    x3 = X::ss(ld32(m, esi + 0x18));                               // 42e378 movss xmm3, [esi + 0x18]
    x2 = X::ss(ld32(m, esi + 0x10));                               // 42e37d movss xmm2, [esi + 0x10]
L_0042e382:
    x2 = subss(x2, x3);                                            // 42e382 subss xmm2, xmm3
    x2 = mulss(x2, x0);                                            // 42e386 mulss xmm2, xmm0
    x0 = X::ss(ld32(m, esp + 0x14));                               // 42e38a movss xmm0, [esp + 0x14]
    x2 = divss(x2, x1);                                            // 42e390 divss xmm2, xmm1
    x2 = addss(x2, x3);                                            // 42e394 addss xmm2, xmm3
    st32(m, esp + 0xc, x2.u[0]);                                   // 42e398 movss [esp + 0xc], xmm2
    k.call(0x00401040);  // FUN_00401040                           // 42e39e call 0x401040
    x5 = x0;                                                       // 42e3a3 movaps xmm5, xmm0
    fl = f_comiss(x5.f(0), flt(ld32(m, 0x9ddfc8)));                // 42e3a6 comiss xmm5, [0x9ddfc8]
    st32(m, esp + 0x8, x5.u[0]);                                   // 42e3ad movss [esp + 8], xmm5
    if (fl.b()) goto L_0042e3f6;                                   // 42e3b3 jb 0x42e3f6
    x0 = cvtss2sd(x0, X::ss(ld32(m, esp + 0x14)));                 // 42e3b5 cvtss2sd xmm0, [esp + 0x14]
    x1 = x0;                                                       // 42e3bb movapd xmm1, xmm0
    x1 = mulsd(x1, x0);                                            // 42e3bf mulsd xmm1, xmm0
    x0 = cvtsd2ss(x0, x1);                                         // 42e3c3 cvtsd2ss xmm0, xmm1
    x1 = sd(0x3ff0000000000000ull);                                // 42e3c7 movsd xmm1, [0x977638]
    x0 = cvtss2sd(x0, x0);                                         // 42e3cf cvtss2sd xmm0, xmm0
    x1 = subsd(x1, x0);                                            // 42e3d3 subsd xmm1, xmm0
    x0 = cvtpd2ps(x1);                                             // 42e3d7 cvtpd2ps xmm0, xmm1
    k.call(0x00401040);  // FUN_00401040                           // 42e3db call 0x401040
    k.push(ecx);                                                   // 42e3e0 push ecx
    st32(m, esp, x0.u[0]);                                         // 42e3e1 movss [esp], xmm0
    k.call(0x0042c230);  // FUN_0042c230                           // 42e3e6 call 0x42c230
    x5 = X::ss(ld32(m, esp + 0xc));                                // 42e3eb movss xmm5, [esp + 0xc]
    esp = esp + 0x4u;                                              // 42e3f1 add esp, 4
    goto L_0042e3f9;                                               // 42e3f4 jmp 0x42e3f9
L_0042e3f6:
    x0 = x5;                                                       // 42e3f6 movaps xmm0, xmm5
L_0042e3f9:
    fl = f_comiss(x5.f(0), flt(ld32(m, 0x9ddfc8)));                // 42e3f9 comiss xmm5, [0x9ddfc8]
    x1 = cvtss2sd(x1, x0);                                         // 42e400 cvtss2sd xmm1, xmm0
    x2 = x1;                                                       // 42e404 movapd xmm2, xmm1
    x2 = mulsd(x2, x1);                                            // 42e408 mulsd xmm2, xmm1
    x1 = cvtsd2ss(x1, x2);                                         // 42e40c cvtsd2ss xmm1, xmm2
    x2 = cvtss2sd(x2, x1);                                         // 42e410 cvtss2sd xmm2, xmm1
    x3 = x2;                                                       // 42e414 movapd xmm3, xmm2
    x3 = mulsd(x3, x2);                                            // 42e418 mulsd xmm3, xmm2
    x2 = sd(0x3fe212eb00000000ull);                                // 42e41c movsd xmm2, [0x9777b0]
    x4 = cvtss2sd(x4, x1);                                         // 42e424 cvtss2sd xmm4, xmm1
    x4 = mulsd(x4, sd(0x3fa9903ac0000000ull));                     // 42e428 mulsd xmm4, [0x9777a8]
    x2 = subsd(x2, x4);                                            // 42e430 subsd xmm2, xmm4
    x2 = cvtpd2ps(x2);                                             // 42e434 cvtpd2ps xmm2, xmm2
    x4 = cvtss2sd(x4, x2);                                         // 42e438 cvtss2sd xmm4, xmm2
    x3 = cvtsd2ss(x3, x3);                                         // 42e43c cvtsd2ss xmm3, xmm3
    x2 = cvtss2sd(x2, x3);                                         // 42e440 cvtss2sd xmm2, xmm3
    x4 = mulsd(x4, x2);                                            // 42e444 mulsd xmm4, xmm2
    x2 = sd(0x3ff0000000000000ull);                                // 42e448 movsd xmm2, [0x977638]
    x6 = cvtss2sd(x6, x1);                                         // 42e450 cvtss2sd xmm6, xmm1
    x6 = mulsd(x6, sd(0x3ff716ac60000000ull));                     // 42e454 mulsd xmm6, [0x9777a0]
    x7 = x2;                                                       // 42e45c movapd xmm7, xmm2
    x7 = subsd(x7, x6);                                            // 42e460 subsd xmm7, xmm6
    x6 = cvtsd2ss(x6, x7);                                         // 42e464 cvtsd2ss xmm6, xmm7
    x6 = cvtss2sd(x6, x6);                                         // 42e468 cvtss2sd xmm6, xmm6
    x4 = addsd(x4, x6);                                            // 42e46c addsd xmm4, xmm6
    x6 = x2;                                                       // 42e470 movapd xmm6, xmm2
    x6 = divsd(x6, x4);                                            // 42e474 divsd xmm6, xmm4
    x7 = cvtss2sd(x7, x1);                                         // 42e478 cvtss2sd xmm7, xmm1
    x7 = mulsd(x7, sd(0x3f93d7f000000000ull));                     // 42e47c mulsd xmm7, [0x977790]
    x4 = cvtsd2ss(x4, x6);                                         // 42e484 cvtsd2ss xmm4, xmm6
    x6 = cvtss2sd(x6, x4);                                         // 42e488 cvtss2sd xmm6, xmm4
    x4 = sd(0x3fd98e2fe0000000ull);                                // 42e48c movsd xmm4, [0x977798]
    x4 = subsd(x4, x7);                                            // 42e494 subsd xmm4, xmm7
    x1 = cvtss2sd(x1, x1);                                         // 42e498 cvtss2sd xmm1, xmm1
    x1 = mulsd(x1, sd(0x3ff46c01a0000000ull));                     // 42e49c mulsd xmm1, [0x977788]
    x4 = cvtpd2ps(x4);                                             // 42e4a4 cvtpd2ps xmm4, xmm4
    x2 = subsd(x2, x1);                                            // 42e4a8 subsd xmm2, xmm1
    x4 = cvtss2sd(x4, x4);                                         // 42e4ac cvtss2sd xmm4, xmm4
    x1 = cvtpd2ps(x2);                                             // 42e4b0 cvtpd2ps xmm1, xmm2
    x2 = sd(0x3ff921fb60000000ull);                                // 42e4b4 movsd xmm2, [0x977510]
    x0 = cvtss2sd(x0, x0);                                         // 42e4bc cvtss2sd xmm0, xmm0
    x3 = cvtss2sd(x3, x3);                                         // 42e4c0 cvtss2sd xmm3, xmm3
    x4 = mulsd(x4, x3);                                            // 42e4c4 mulsd xmm4, xmm3
    x1 = cvtss2sd(x1, x1);                                         // 42e4c8 cvtss2sd xmm1, xmm1
    x4 = addsd(x4, x1);                                            // 42e4cc addsd xmm4, xmm1
    x4 = mulsd(x4, x0);                                            // 42e4d0 mulsd xmm4, xmm0
    x0 = cvtpd2ps(x4);                                             // 42e4d4 cvtpd2ps xmm0, xmm4
    x0 = cvtss2sd(x0, x0);                                         // 42e4d8 cvtss2sd xmm0, xmm0
    x6 = mulsd(x6, x0);                                            // 42e4dc mulsd xmm6, xmm0
    x0 = cvtsd2ss(x0, x6);                                         // 42e4e0 cvtsd2ss xmm0, xmm6
    if (fl.b()) goto L_0042e4f6;                                   // 42e4e4 jb 0x42e4f6
    x0 = cvtss2sd(x0, x0);                                         // 42e4e6 cvtss2sd xmm0, xmm0
    x1 = x2;                                                       // 42e4ea movapd xmm1, xmm2
    x1 = subsd(x1, x0);                                            // 42e4ee subsd xmm1, xmm0
    x0 = cvtsd2ss(x0, x1);                                         // 42e4f2 cvtsd2ss xmm0, xmm1
L_0042e4f6:
    x1 = X::ss(ld32(m, esp + 0x14));                               // 42e4f6 movss xmm1, [esp + 0x14]
    fl = f_comiss(x1.f(0), flt(0x00000000u));                      // 42e4fc comiss xmm1, [0x92e12c]
    if (fl.ae()) goto L_0042e514;                                  // 42e503 jae 0x42e514
    x1 = X::ss(0x80000000u);                                       // 42e505 movss xmm1, [0x9774a8]
    x1 = subss(x1, x0);                                            // 42e50d subss xmm1, xmm0
    x0 = x1;                                                       // 42e511 movaps xmm0, xmm1
L_0042e514:
    x1 = X::ss(ld32(m, esp + 0xc));                                // 42e514 movss xmm1, [esp + 0xc]
    x0 = cvtss2sd(x0, x0);                                         // 42e51a cvtss2sd xmm0, xmm0
    x2 = subsd(x2, x0);                                            // 42e51e subsd xmm2, xmm0
    x0 = cvtpd2ps(x2);                                             // 42e522 cvtpd2ps xmm0, xmm2
    fl = f_comiss(x1.f(0), x0.f(0));                               // 42e526 comiss xmm1, xmm0
    if (fl.be()) goto L_0042e532;                                  // 42e529 jbe 0x42e532
    eax = 0x1u;                                                    // 42e52b mov eax, 1
    goto L_0042e534;                                               // 42e530 jmp 0x42e534
L_0042e532:
    eax = 0u;                                                      // 42e532 xor eax, eax
L_0042e534:
    fl = f_test(edi, edi);                                         // 42e534 test edi, edi
    if (fl.e()) goto L_0042e548;                                   // 42e536 je 0x42e548
    fl = f_test(eax, eax);                                         // 42e538 test eax, eax
    if (fl.e()) goto L_0042e548;                                   // 42e53a je 0x42e548
    eax = 0x1u;                                                    // 42e53c mov eax, 1
    edi = k.pop();                                                 // 42e541 pop edi
    esp = ebp;                                                     // 42e542 mov esp, ebp
    ebp = k.pop();                                                 // 42e544 pop ebp
    return k.ret(4);                                               // 42e545 ret 4
L_0042e548:
    eax = 0u;                                                      // 42e548 xor eax, eax
    edi = k.pop();                                                 // 42e54a pop edi
    esp = ebp;                                                     // 42e54b mov esp, ebp
    ebp = k.pop();                                                 // 42e54d pop ebp
    return k.ret(4);                                               // 42e54e ret 4
}

// 0042e5f0 FUN_0042e5f0: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x0042e5f0, Fn_0042e5f0, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    x0 = ld(m, 0xd61670);                                          // 42e5f0 movaps xmm0, [0xd61670]
    eax = ecx;                                                     // 42e5f7 mov eax, ecx
    st(m, eax, x0);                                                // 42e5f9 movaps [eax], xmm0
    x0 = ld(m, 0xd61640);                                          // 42e5fc movaps xmm0, [0xd61640]
    st(m, eax + 0x10, x0);                                         // 42e603 movaps [eax + 0x10], xmm0
    x0 = ld(m, 0xd61640);                                          // 42e607 movaps xmm0, [0xd61640]
    st(m, eax + 0x20, x0);                                         // 42e60e movaps [eax + 0x20], xmm0
    return k.ret(0);                                               // 42e612 ret 
}

// 0042ef70 SimulationClock_Advance: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x0042ef70, SimulationClock_Advance, "math", 0) {  // outputs: what callers read (it calls other groups' code)
    Cpu k(m);
    CPU_REFS(k);
    ecx = ld32(m, 0xa95844);                                       // 42ef70 mov ecx, [0xa95844]
    eax = ld32(m, ecx);                                            // 42ef76 mov eax, [ecx]
    edx = ld32(m, eax + 0x8);                                      // 42ef78 mov edx, [eax + 8]
    esp = esp - 0xcu;                                              // 42ef7b sub esp, 0xc
    k.push(ebx);                                                   // 42ef7e push ebx
    k.call(edx);                                                   // 42ef7f call edx
    edx = ld32(m, eax);                                            // 42ef81 mov edx, [eax]
    k.push(0x0u);                                                  // 42ef83 push 0
    ecx = eax;                                                     // 42ef85 mov ecx, eax
    eax = ld32(m, edx + 0x7c);                                     // 42ef87 mov eax, [edx + 0x7c]
    k.push(0x2cu);                                                 // 42ef8a push 0x2c
    k.call(eax);                                                   // 42ef8c call eax
    ebx = 0x1u;                                                    // 42ef8e mov ebx, 1
    fl = f_test(eax, eax);                                         // 42ef93 test eax, eax
    if (fl.e()) goto L_0042efe8;                                   // 42ef95 je 0x42efe8
    fl = f_test8(ld8(m, 0xdcf304), (ebx & 0xff));                  // 42ef97 test byte ptr [0xdcf304], bl
    if (fl.ne()) goto L_0042efb7;                                  // 42ef9d jne 0x42efb7
    st32(m, 0xdcf304, ld32(m, 0xdcf304) | ebx);                    // 42ef9f or [0xdcf304], ebx
    k.call(0x006de1f0);  // FUN_006de1f0                           // 42efa5 call 0x6de1f0
    k.push(0x913e00u);                                             // 42efaa push 0x913e00
    k.call(0x008f9fbf);  // _atexit                                // 42efaf call 0x8f9fbf
    esp = esp + 0x4u;                                              // 42efb4 add esp, 4
L_0042efb7:
    eax = 0xda6ca0u;                                               // 42efb7 mov eax, 0xda6ca0
    k.call(0x006def20);  // FUN_006def20                           // 42efbc call 0x6def20
    st32(m, 0xa89be4, x0.u[0]);                                    // 42efc1 movss [0xa89be4], xmm0
    x0 = X{};                                                      // 42efc9 xorps xmm0, xmm0
    st32(m, 0xa89be8, x0.u[0]);                                    // 42efcc movss [0xa89be8], xmm0
    st32(m, 0xa89bec, x0.u[0]);                                    // 42efd4 movss [0xa89bec], xmm0
    st32(m, 0xa89bf0, 0x0u);                                       // 42efdc mov [0xa89bf0], 0
    goto L_0042eff0;                                               // 42efe6 jmp 0x42eff0
L_0042efe8:
    x0 = X::ss(ld32(m, 0xa89bec));                                 // 42efe8 movss xmm0, [0xa89bec]
L_0042eff0:
    x0 = addss(x0, X::ss(ld32(m, 0xa89be8)));                      // 42eff0 addss xmm0, [0xa89be8]
    x0 = addss(x0, X::ss(ld32(m, 0xa89be4)));                      // 42eff8 addss xmm0, [0xa89be4]
    st32(m, esp + 0x8, x0.u[0]);                                   // 42f000 movss [esp + 8], xmm0
    fl = f_test8(ld8(m, 0xdcf304), (ebx & 0xff));                  // 42f006 test byte ptr [0xdcf304], bl
    if (fl.ne()) goto L_0042f026;                                  // 42f00c jne 0x42f026
    st32(m, 0xdcf304, ld32(m, 0xdcf304) | ebx);                    // 42f00e or [0xdcf304], ebx
    k.call(0x006de1f0);  // FUN_006de1f0                           // 42f014 call 0x6de1f0
    k.push(0x913e00u);                                             // 42f019 push 0x913e00
    k.call(0x008f9fbf);  // _atexit                                // 42f01e call 0x8f9fbf
    esp = esp + 0x4u;                                              // 42f023 add esp, 4
L_0042f026:
    eax = 0xda6ca0u;                                               // 42f026 mov eax, 0xda6ca0
    k.call(0x006def20);  // FUN_006def20                           // 42f02b call 0x6def20
    x1 = x0;                                                       // 42f030 movaps xmm1, xmm0
    x0 = X::ss(ld32(m, esp + 0x8));                                // 42f033 movss xmm0, [esp + 8]
    st32(m, esp + 0xc, x1.u[0]);                                   // 42f039 movss [esp + 0xc], xmm1
    x0 = subss(x0, x1);                                            // 42f03f subss xmm0, xmm1
    k.call(0x00401000);  // FUN_00401000                           // 42f043 call 0x401000
    fl = f_comiss(x0.f(0), flt(ld32(m, 0x9cf5a8)));                // 42f048 comiss xmm0, [0x9cf5a8]
    if (fl.be()) goto L_0042f06e;                                  // 42f04f jbe 0x42f06e
    fl = f_cmp(ld32(m, 0xa89bf0), 0x0u);                           // 42f051 cmp [0xa89bf0], 0
    if (fl.e()) goto L_0042f093;                                   // 42f058 je 0x42f093
    x1 = X::ss(ld32(m, 0xa89bec));                                 // 42f05a movss xmm1, [0xa89bec]
    x1 = addss(x1, x0);                                            // 42f062 addss xmm1, xmm0
    st32(m, 0xa89bec, x1.u[0]);                                    // 42f066 movss [0xa89bec], xmm1
L_0042f06e:
    x0 = X::ss(ld32(m, 0xa89be8));                                 // 42f06e movss xmm0, [0xa89be8]
L_0042f076:
    x0 = addss(x0, X::ss(ld32(m, esp + 0x14)));                    // 42f076 addss xmm0, [esp + 0x14]
    st32(m, 0xa89be8, x0.u[0]);                                    // 42f07c movss [0xa89be8], xmm0
    st32(m, 0xa89bf0, 0x0u);                                       // 42f084 mov [0xa89bf0], 0
    ebx = k.pop();                                                 // 42f08e pop ebx
    esp = esp + 0xcu;                                              // 42f08f add esp, 0xc
    return k.ret(0);                                               // 42f092 ret 
L_0042f093:
    x0 = X::ss(ld32(m, esp + 0xc));                                // 42f093 movss xmm0, [esp + 0xc]
    st32(m, 0xa89be4, x0.u[0]);                                    // 42f099 movss [0xa89be4], xmm0
    x0 = X{};                                                      // 42f0a1 xorps xmm0, xmm0
    st32(m, 0xa89bec, x0.u[0]);                                    // 42f0a4 movss [0xa89bec], xmm0
    goto L_0042f076;                                               // 42f0ac jmp 0x42f076
}

// 0042f1d0 FUN_0042f1d0: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x0042f1d0, Fn_0042f1d0, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    st32(m, eax, 0x93ca94u);                                       // 42f1d0 mov [eax], 0x93ca94
    ecx = 0x6u;                                                    // 42f1d6 mov ecx, 6
    st32(m, eax + 0x4, ecx);                                       // 42f1db mov [eax + 4], ecx
    st32(m, eax + 0x8, ecx);                                       // 42f1de mov [eax + 8], ecx
    ecx = 0x3u;                                                    // 42f1e1 mov ecx, 3
    st32(m, eax + 0x14, ecx);                                      // 42f1e6 mov [eax + 0x14], ecx
    st32(m, eax + 0x18, ecx);                                      // 42f1e9 mov [eax + 0x18], ecx
    edx = 0x93ca04u;                                               // 42f1ec mov edx, 0x93ca04
    st32(m, eax + 0x10, edx);                                      // 42f1f1 mov [eax + 0x10], edx
    st32(m, eax + 0x54, ecx);                                      // 42f1f4 mov [eax + 0x54], ecx
    st32(m, eax + 0x58, ecx);                                      // 42f1f7 mov [eax + 0x58], ecx
    st32(m, eax + 0x50, edx);                                      // 42f1fa mov [eax + 0x50], edx
    st32(m, eax + 0x94, ecx);                                      // 42f1fd mov [eax + 0x94], ecx
    st32(m, eax + 0x98, ecx);                                      // 42f203 mov [eax + 0x98], ecx
    st32(m, eax + 0x90, edx);                                      // 42f209 mov [eax + 0x90], edx
    st32(m, eax + 0xd4, ecx);                                      // 42f20f mov [eax + 0xd4], ecx
    st32(m, eax + 0xd8, ecx);                                      // 42f215 mov [eax + 0xd8], ecx
    st32(m, eax + 0xd0, edx);                                      // 42f21b mov [eax + 0xd0], edx
    return k.ret(0);                                               // 42f221 ret 
}

// 0042f230 FUN_0042f230: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x0042f230, Fn_0042f230, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    st32(m, eax + 0x8, 0x6u);                                      // 42f230 mov [eax + 8], 6
    st32(m, eax, 0x93cab8u);                                       // 42f237 mov [eax], 0x93cab8
    ecx = 0x4u;                                                    // 42f23d mov ecx, 4
    st32(m, eax + 0x4, ecx);                                       // 42f242 mov [eax + 4], ecx
    edx = 0x93ca4cu;                                               // 42f245 mov edx, 0x93ca4c
    st32(m, eax + 0x14, ecx);                                      // 42f24a mov [eax + 0x14], ecx
    st32(m, eax + 0x18, 0x3u);                                     // 42f24d mov [eax + 0x18], 3
    st32(m, eax + 0x10, edx);                                      // 42f254 mov [eax + 0x10], edx
    st32(m, eax + 0x64, ecx);                                      // 42f257 mov [eax + 0x64], ecx
    st32(m, eax + 0x68, 0x3u);                                     // 42f25a mov [eax + 0x68], 3
    st32(m, eax + 0x60, edx);                                      // 42f261 mov [eax + 0x60], edx
    return k.ret(0);                                               // 42f264 ret 
}

// 0042f2a0 FUN_0042f2a0: instruction-level transliteration (tr2)
REFLEX_PORT_FN(0x0042f2a0, Fn_0042f2a0, "math", OUT_XMM | OUT_GPR) {
    Cpu k(m);
    CPU_REFS(k);
    st32(m, eax + 0x4, 0x6u);                                      // 42f2a0 mov [eax + 4], 6
    st32(m, eax, 0x93cadcu);                                       // 42f2a7 mov [eax], 0x93cadc
    ecx = 0x4u;                                                    // 42f2ad mov ecx, 4
    st32(m, eax + 0x8, ecx);                                       // 42f2b2 mov [eax + 8], ecx
    edx = 0x93ca28u;                                               // 42f2b5 mov edx, 0x93ca28
    st32(m, eax + 0x14, 0x3u);                                     // 42f2ba mov [eax + 0x14], 3
    st32(m, eax + 0x18, ecx);                                      // 42f2c1 mov [eax + 0x18], ecx
    st32(m, eax + 0x10, edx);                                      // 42f2c4 mov [eax + 0x10], edx
    st32(m, eax + 0x54, 0x3u);                                     // 42f2c7 mov [eax + 0x54], 3
    st32(m, eax + 0x58, ecx);                                      // 42f2ce mov [eax + 0x58], ecx
    st32(m, eax + 0x50, edx);                                      // 42f2d1 mov [eax + 0x50], edx
    return k.ret(0);                                               // 42f2d4 ret 
}

