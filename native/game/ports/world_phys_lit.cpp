// World group: the large straight-line SIMD kernels of the generic rigid bodies (contact impulses between two bodies), as literal
// register-level transliterations (one statement per original instruction, the address in the comment; generated from the
// disassembly with scratchpad tool sse2cpp.py and checked by Verify). The original's stack frame is a local dword array S,
// loaded from the guest frame at entry so that even its never-written slots read the same bytes.
#include "../port.h"
#include "world_util.h"

using namespace world;
using reflex_port::OUT_XMM_ALL;

// Locals of a literal kernel: GPRs, xmm, frame S (frame_dwords dwords at guest address SP), arguments A(off) = [ebp + off].
#define WORLD_LIT_BEGIN(frame_bytes, pushes)                                                                         \
    Guest g{m};                                                                                                     \
    const uint32_t E = m.r[4], ebp = E - 4;                                                                         \
    const uint32_t SP = ((E - 4) & ~15u) - (frame_bytes) - 4u * (pushes);                                         \
    uint32_t eax = m.r[0], ecx = m.r[1], edx = m.r[2], ebx = m.r[3], esi = m.r[6], edi = m.r[7];                  \
    (void)eax; (void)ecx; (void)edx; (void)ebx; (void)esi; (void)edi;                                              \
    X x0 = xr(g, 0), x1 = xr(g, 1), x2 = xr(g, 2), x3 = xr(g, 3), x4 = xr(g, 4), x5 = xr(g, 5), x6 = xr(g, 6), x7 = xr(g, 7); \
    uint32_t S[((frame_bytes) + 4u * (pushes)) / 4];                                                               \
    for (uint32_t i = 0; i < sizeof S / 4; i++) S[i] = g.u32(SP + 4 * i);                                          \
    auto A = [&](uint32_t off) { return g.u32(ebp + off); };                                                        \
    auto SX = [&](uint32_t off) { return X{{S[off / 4], S[off / 4 + 1], S[off / 4 + 2], S[off / 4 + 3]}}; };         \
    auto SXw = [&](uint32_t off, const X& v) { for (int i = 0; i < 4; i++) S[off / 4 + i] = v.u[i]; };
#define WORLD_LIT_END(cleanup)                                                                                      \
    L_return:                                                                                                       \
    xw(g, 0, x0); xw(g, 1, x1); xw(g, 2, x2); xw(g, 3, x3); xw(g, 4, x4); xw(g, 5, x5); xw(g, 6, x6); xw(g, 7, x7); \
    m.r[0] = eax; m.r[1] = ecx; m.r[2] = edx;                                                                       \
    return m.ret(cleanup);

// 0076f440: the impulse of one contact between two rigid bodies (ECX = body A linear velocity (out), stdcall 15 arguments:
// both bodies' inverse mass, orientation, centre, angular velocity (in / out), inverse inertia, the contact point, normal,
// restitution and friction; ret 0x3c). As 007705b0 for two bodies: relative point velocity along the normal (separating:
// only the tangential path), the normal impulse over both bodies' effective masses, friction above a threshold, and the
// velocity changes of both. Three exits as in the original.
REFLEX_PORT_FN(0x0076f440, RigidBody_ContactImpulsePair, "world", OUT_XMM_ALL) {
    WORLD_LIT_BEGIN(0xb4, 3)
    eax = A(0x18); // 0076f44c mov eax, dword ptr [ebp + 0x18]
    x2 = ld(g, 0x92db80); // 0076f44f movaps xmm2, xmmword ptr [0x92db80]
    // 0076f456 push ebx  (prologue / epilogue: skipped)
    // 0076f457 push esi  (prologue / epilogue: skipped)
    // 0076f458 push edi  (prologue / epilogue: skipped)
    edi = A(0x3c); // 0076f459 mov edi, dword ptr [ebp + 0x3c]
    x0 = ld(g, edi + 0x0); // 0076f45c movaps xmm0, xmmword ptr [edi]
    edi = A(0x14); // 0076f45f mov edi, dword ptr [ebp + 0x14]
    x1 = ld(g, edi + 0x0); // 0076f462 movaps xmm1, xmmword ptr [edi]
    edi = A(0x10); // 0076f465 mov edi, dword ptr [ebp + 0x10]
    SXw(0xa0, x0); // 0076f468 movaps xmmword ptr [esp + 0xa0], xmm0
    x0 = subps(x0, x1); // 0076f470 subps xmm0, xmm1
    x1 = ld(g, eax + 0x0); // 0076f473 movaps xmm1, xmmword ptr [eax]
    x1 = andps(x1, x2); // 0076f476 andps xmm1, xmm2
    x5 = x1; // 0076f479 movaps xmm5, xmm1
    x5 = shufps(x5, x1, 0); // 0076f47c shufps xmm5, xmm1, 0
    x6 = x1; // 0076f480 movaps xmm6, xmm1
    x6 = shufps(x6, x1, 0x55); // 0076f483 shufps xmm6, xmm1, 0x55
    SXw(0x10, x0); // 0076f487 movaps xmmword ptr [esp + 0x10], xmm0
    x0 = ld(g, edi + 0x0); // 0076f48c movaps xmm0, xmmword ptr [edi]
    x3 = x0; // 0076f48f movaps xmm3, xmm0
    x3 = mulps(x3, ld(g, 0x93a220)); // 0076f492 mulps xmm3, xmmword ptr [0x93a220]
    SXw(0x90, x3); // 0076f499 movaps xmmword ptr [esp + 0x90], xmm3
    x3 = shufps(x3, x3, 0x1b); // 0076f4a1 shufps xmm3, xmm3, 0x1b
    x5 = mulps(x5, x3); // 0076f4a5 mulps xmm5, xmm3
    x5 = mulps(x5, ld(g, 0x93a1f0)); // 0076f4a8 mulps xmm5, xmmword ptr [0x93a1f0]
    x3 = shufps(x3, x3, 0xb1); // 0076f4af shufps xmm3, xmm3, 0xb1
    x6 = mulps(x6, x3); // 0076f4b3 mulps xmm6, xmm3
    x6 = mulps(x6, ld(g, 0x93a200)); // 0076f4b6 mulps xmm6, xmmword ptr [0x93a200]
    x4 = x3; // 0076f4bd movaps xmm4, xmm3
    x4 = shufps(x4, x3, 0x1b); // 0076f4c0 shufps xmm4, xmm3, 0x1b
    x3 = x1; // 0076f4c4 movaps xmm3, xmm1
    x3 = shufps(x3, x1, 0xaa); // 0076f4c7 shufps xmm3, xmm1, 0xaa
    edi = A(0x28); // 0076f4cb mov edi, dword ptr [ebp + 0x28]
    x4 = mulps(x4, x3); // 0076f4ce mulps xmm4, xmm3
    x3 = ld(g, 0x93a210); // 0076f4d1 movaps xmm3, xmmword ptr [0x93a210]
    x4 = mulps(x4, x3); // 0076f4d8 mulps xmm4, xmm3
    x4 = addps(x4, x6); // 0076f4db addps xmm4, xmm6
    x6 = x1; // 0076f4de movaps xmm6, xmm1
    x6 = shufps(x6, x1, 0xff); // 0076f4e1 shufps xmm6, xmm1, 0xff
    x6 = mulps(x6, SX(0x90)); // 0076f4e5 mulps xmm6, xmmword ptr [esp + 0x90]
    x6 = addps(x6, x5); // 0076f4ed addps xmm6, xmm5
    x4 = addps(x4, x6); // 0076f4f0 addps xmm4, xmm6
    edx = A(0x30); // 0076f4f3 mov edx, dword ptr [ebp + 0x30]
    x1 = x4; // 0076f4f6 movaps xmm1, xmm4
    x1 = shufps(x1, x4, 0x1b); // 0076f4f9 shufps xmm1, xmm4, 0x1b
    x5 = x0; // 0076f4fd movaps xmm5, xmm0
    x5 = shufps(x5, x0, 0); // 0076f500 shufps xmm5, xmm0, 0
    x5 = mulps(x5, x1); // 0076f504 mulps xmm5, xmm1
    x5 = mulps(x5, ld(g, 0x93a1f0)); // 0076f507 mulps xmm5, xmmword ptr [0x93a1f0]
    x1 = shufps(x1, x1, 0xb1); // 0076f50e shufps xmm1, xmm1, 0xb1
    x7 = x1; // 0076f512 movaps xmm7, xmm1
    x6 = x0; // 0076f515 movaps xmm6, xmm0
    x6 = shufps(x6, x0, 0x55); // 0076f518 shufps xmm6, xmm0, 0x55
    x6 = mulps(x6, x7); // 0076f51c mulps xmm6, xmm7
    x6 = mulps(x6, ld(g, 0x93a200)); // 0076f51f mulps xmm6, xmmword ptr [0x93a200]
    x1 = x0; // 0076f526 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0xaa); // 0076f529 shufps xmm1, xmm0, 0xaa
    x7 = shufps(x7, x7, 0x1b); // 0076f52d shufps xmm7, xmm7, 0x1b
    x1 = mulps(x1, x7); // 0076f531 mulps xmm1, xmm7
    x1 = mulps(x1, x3); // 0076f534 mulps xmm1, xmm3
    x1 = addps(x1, x6); // 0076f537 addps xmm1, xmm6
    x6 = x0; // 0076f53a movaps xmm6, xmm0
    x6 = shufps(x6, x0, 0xff); // 0076f53d shufps xmm6, xmm0, 0xff
    x6 = mulps(x6, x4); // 0076f541 mulps xmm6, xmm4
    x4 = ld(g, edi + 0x0); // 0076f544 movaps xmm4, xmmword ptr [edi]
    edi = A(0x24); // 0076f547 mov edi, dword ptr [ebp + 0x24]
    x6 = addps(x6, x5); // 0076f54a addps xmm6, xmm5
    x1 = addps(x1, x6); // 0076f54d addps xmm1, xmm6
    x1 = shufps(x1, x1, 0xc9); // 0076f550 shufps xmm1, xmm1, 0xc9
    SXw(0x70, x1); // 0076f554 movaps xmmword ptr [esp + 0x70], xmm1
    x1 = SX(0x10); // 0076f559 movaps xmm1, xmmword ptr [esp + 0x10]
    x1 = shufps(x1, x1, 0xd2); // 0076f55e shufps xmm1, xmm1, 0xd2
    SXw(0x30, x1); // 0076f562 movaps xmmword ptr [esp + 0x30], xmm1
    x1 = SX(0xa0); // 0076f567 movaps xmm1, xmmword ptr [esp + 0xa0]
    x1 = subps(x1, x4); // 0076f56f subps xmm1, xmm4
    esi = ecx; // 0076f572 mov esi, ecx
    ecx = A(0x2c); // 0076f574 mov ecx, dword ptr [ebp + 0x2c]
    x4 = ld(g, ecx + 0x0); // 0076f577 movaps xmm4, xmmword ptr [ecx]
    SXw(0x80, x1); // 0076f57a movaps xmmword ptr [esp + 0x80], xmm1
    x1 = ld(g, edi + 0x0); // 0076f582 movaps xmm1, xmmword ptr [edi]
    x4 = andps(x4, x2); // 0076f585 andps xmm4, xmm2
    x6 = x4; // 0076f588 movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0); // 0076f58b shufps xmm6, xmm4, 0
    x7 = x4; // 0076f58f movaps xmm7, xmm4
    x7 = shufps(x7, x4, 0xaa); // 0076f592 shufps xmm7, xmm4, 0xaa
    x5 = x1; // 0076f596 movaps xmm5, xmm1
    x5 = mulps(x5, ld(g, 0x93a220)); // 0076f599 mulps xmm5, xmmword ptr [0x93a220]
    SXw(0x90, x5); // 0076f5a0 movaps xmmword ptr [esp + 0x90], xmm5
    x5 = shufps(x5, x5, 0x1b); // 0076f5a8 shufps xmm5, xmm5, 0x1b
    x6 = mulps(x6, x5); // 0076f5ac mulps xmm6, xmm5
    SXw(0xa0, x6); // 0076f5af movaps xmmword ptr [esp + 0xa0], xmm6
    x6 = x4; // 0076f5b7 movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0x55); // 0076f5ba shufps xmm6, xmm4, 0x55
    x5 = shufps(x5, x5, 0xb1); // 0076f5be shufps xmm5, xmm5, 0xb1
    x6 = mulps(x6, x5); // 0076f5c2 mulps xmm6, xmm5
    x6 = mulps(x6, ld(g, 0x93a200)); // 0076f5c5 mulps xmm6, xmmword ptr [0x93a200]
    x5 = shufps(x5, x5, 0x1b); // 0076f5cc shufps xmm5, xmm5, 0x1b
    x5 = mulps(x5, x7); // 0076f5d0 mulps xmm5, xmm7
    x5 = mulps(x5, x3); // 0076f5d3 mulps xmm5, xmm3
    x5 = addps(x5, x6); // 0076f5d6 addps xmm5, xmm6
    x6 = x4; // 0076f5d9 movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0xff); // 0076f5dc shufps xmm6, xmm4, 0xff
    x6 = mulps(x6, SX(0x90)); // 0076f5e0 mulps xmm6, xmmword ptr [esp + 0x90]
    x4 = SX(0xa0); // 0076f5e8 movaps xmm4, xmmword ptr [esp + 0xa0]
    x4 = mulps(x4, ld(g, 0x93a1f0)); // 0076f5f0 mulps xmm4, xmmword ptr [0x93a1f0]
    x6 = addps(x6, x4); // 0076f5f7 addps xmm6, xmm4
    x5 = addps(x5, x6); // 0076f5fa addps xmm5, xmm6
    x4 = x5; // 0076f5fd movaps xmm4, xmm5
    x4 = shufps(x4, x5, 0x1b); // 0076f600 shufps xmm4, xmm5, 0x1b
    x6 = x1; // 0076f604 movaps xmm6, xmm1
    x6 = shufps(x6, x1, 0); // 0076f607 shufps xmm6, xmm1, 0
    x6 = mulps(x6, x4); // 0076f60b mulps xmm6, xmm4
    SXw(0x90, x6); // 0076f60e movaps xmmword ptr [esp + 0x90], xmm6
    x4 = shufps(x4, x4, 0xb1); // 0076f616 shufps xmm4, xmm4, 0xb1
    x6 = x1; // 0076f61a movaps xmm6, xmm1
    x6 = shufps(x6, x1, 0x55); // 0076f61d shufps xmm6, xmm1, 0x55
    x6 = mulps(x6, x4); // 0076f621 mulps xmm6, xmm4
    x6 = mulps(x6, ld(g, 0x93a200)); // 0076f624 mulps xmm6, xmmword ptr [0x93a200]
    x4 = shufps(x4, x4, 0x1b); // 0076f62b shufps xmm4, xmm4, 0x1b
    x7 = x1; // 0076f62f movaps xmm7, xmm1
    x7 = shufps(x7, x1, 0xaa); // 0076f632 shufps xmm7, xmm1, 0xaa
    x4 = mulps(x4, x7); // 0076f636 mulps xmm4, xmm7
    x4 = mulps(x4, x3); // 0076f639 mulps xmm4, xmm3
    x4 = addps(x4, x6); // 0076f63c addps xmm4, xmm6
    x6 = x1; // 0076f63f movaps xmm6, xmm1
    x6 = shufps(x6, x1, 0xff); // 0076f642 shufps xmm6, xmm1, 0xff
    x6 = mulps(x6, x5); // 0076f646 mulps xmm6, xmm5
    x5 = SX(0x90); // 0076f649 movaps xmm5, xmmword ptr [esp + 0x90]
    x5 = mulps(x5, ld(g, 0x93a1f0)); // 0076f651 mulps xmm5, xmmword ptr [0x93a1f0]
    x6 = addps(x6, x5); // 0076f658 addps xmm6, xmm5
    x5 = SX(0x80); // 0076f65b movaps xmm5, xmmword ptr [esp + 0x80]
    x4 = addps(x4, x6); // 0076f663 addps xmm4, xmm6
    x4 = shufps(x4, x4, 0xc9); // 0076f666 shufps xmm4, xmm4, 0xc9
    SXw(0x60, x4); // 0076f66a movaps xmmword ptr [esp + 0x60], xmm4
    x4 = ld(g, esi + 0x0); // 0076f66f movaps xmm4, xmmword ptr [esi]
    SXw(0x90, x4); // 0076f672 movaps xmmword ptr [esp + 0x90], xmm4
    x4 = SX(0x30); // 0076f67a movaps xmm4, xmmword ptr [esp + 0x30]
    x6 = x4; // 0076f67f movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0xd2); // 0076f682 shufps xmm6, xmm4, 0xd2
    x4 = SX(0x70); // 0076f686 movaps xmm4, xmmword ptr [esp + 0x70]
    x5 = shufps(x5, x5, 0xd2); // 0076f68b shufps xmm5, xmm5, 0xd2
    x7 = x4; // 0076f68f movaps xmm7, xmm4
    x7 = shufps(x7, x4, 0xc9); // 0076f692 shufps xmm7, xmm4, 0xc9
    x6 = mulps(x6, x7); // 0076f696 mulps xmm6, xmm7
    x7 = SX(0x30); // 0076f699 movaps xmm7, xmmword ptr [esp + 0x30]
    x7 = mulps(x7, x4); // 0076f69e mulps xmm7, xmm4
    x4 = SX(0x60); // 0076f6a1 movaps xmm4, xmmword ptr [esp + 0x60]
    x7 = subps(x7, x6); // 0076f6a6 subps xmm7, xmm6
    x6 = x5; // 0076f6a9 movaps xmm6, xmm5
    x6 = shufps(x6, x5, 0xd2); // 0076f6ac shufps xmm6, xmm5, 0xd2
    SXw(0xa0, x5); // 0076f6b0 movaps xmmword ptr [esp + 0xa0], xmm5
    x5 = x4; // 0076f6b8 movaps xmm5, xmm4
    x5 = shufps(x5, x4, 0xc9); // 0076f6bb shufps xmm5, xmm4, 0xc9
    x6 = mulps(x6, x5); // 0076f6bf mulps xmm6, xmm5
    x5 = SX(0xa0); // 0076f6c2 movaps xmm5, xmmword ptr [esp + 0xa0]
    x5 = mulps(x5, x4); // 0076f6ca mulps xmm5, xmm4
    x4 = ld(g, edx + 0x0); // 0076f6cd movaps xmm4, xmmword ptr [edx]
    x7 = andps(x7, x2); // 0076f6d0 andps xmm7, xmm2
    x7 = addps(x7, SX(0x90)); // 0076f6d3 addps xmm7, xmmword ptr [esp + 0x90]
    x5 = subps(x5, x6); // 0076f6db subps xmm5, xmm6
    x5 = andps(x5, x2); // 0076f6de andps xmm5, xmm2
    edi = A(0x40); // 0076f6e1 mov edi, dword ptr [ebp + 0x40]
    x5 = addps(x5, x4); // 0076f6e4 addps xmm5, xmm4
    x4 = ld(g, edi + 0x0); // 0076f6e7 movaps xmm4, xmmword ptr [edi]
    x5 = subps(x5, x7); // 0076f6ea subps xmm5, xmm7
    x6 = x4; // 0076f6ed movaps xmm6, xmm4
    x6 = mulps(x6, x5); // 0076f6f0 mulps xmm6, xmm5
    x7 = x6; // 0076f6f3 movaps xmm7, xmm6
    x7 = shufps(x7, x6, 0x99); // 0076f6f6 shufps xmm7, xmm6, 0x99
    x6.u[0] = x86::bits(x86::add(x6.f(0), (x7).f(0))); // 0076f6fa addss xmm6, xmm7
    x7 = shufps(x7, x7, 0x55); // 0076f6fe shufps xmm7, xmm7, 0x55
    x6.u[0] = x86::bits(x86::add(x6.f(0), (x7).f(0))); // 0076f702 addss xmm6, xmm7
    x6 = shufps(x6, x6, 0); // 0076f706 shufps xmm6, xmm6, 0
    SXw(0x70, x6); // 0076f70a movaps xmmword ptr [esp + 0x70], xmm6
    x6 = cmpltps(x6, ld(g, 0xd61670)); // 0076f70f cmpltps xmm6, xmmword ptr [0xd61670]
    SXw(0x60, x6); // 0076f717 movaps xmmword ptr [esp + 0x60], xmm6
    edi = S[0x64 / 4]; // 0076f71c mov edi, dword ptr [esp + 0x64]
    ebx = S[0x68 / 4]; // 0076f720 mov ebx, dword ptr [esp + 0x68]
    ebx = ebx + edi; // 0076f724 add ebx, edi
    ebx = ebx + S[0x60 / 4]; // 0076f726 add ebx, dword ptr [esp + 0x60]
    SXw(0x50, x5); // 0076f72a movaps xmmword ptr [esp + 0x50], xmm5
    SXw(0x20, x4); // 0076f72f movaps xmmword ptr [esp + 0x20], xmm4
    // 0076f734 cmp ebx, -3
    if (int32_t(ebx) != int32_t(uint32_t(-3))) goto L_0076fe88; // 0076f737 jne 0x76fe88
    x5 = SX(0x10); // 0076f73d movaps xmm5, xmmword ptr [esp + 0x10]
    x5 = shufps(x5, x5, 0xc9); // 0076f742 shufps xmm5, xmm5, 0xc9
    x7 = x5; // 0076f746 movaps xmm7, xmm5
    x7 = shufps(x7, x5, 0xc9); // 0076f749 shufps xmm7, xmm5, 0xc9
    x4 = shufps(x4, x4, 0xd2); // 0076f74d shufps xmm4, xmm4, 0xd2
    x6 = x4; // 0076f751 movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0xd2); // 0076f754 shufps xmm6, xmm4, 0xd2
    x4 = mulps(x4, x5); // 0076f758 mulps xmm4, xmm5
    x6 = mulps(x6, x7); // 0076f75b mulps xmm6, xmm7
    x4 = subps(x4, x6); // 0076f75e subps xmm4, xmm6
    x4 = andps(x4, x2); // 0076f761 andps xmm4, xmm2
    x4 = andps(x4, x2); // 0076f764 andps xmm4, xmm2
    x6 = x4; // 0076f767 movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0); // 0076f76a shufps xmm6, xmm4, 0
    x7 = x4; // 0076f76e movaps xmm7, xmm4
    x7 = shufps(x7, x4, 0xaa); // 0076f771 shufps xmm7, xmm4, 0xaa
    x5 = x0; // 0076f775 movaps xmm5, xmm0
    x5 = shufps(x5, x0, 0x1b); // 0076f778 shufps xmm5, xmm0, 0x1b
    x6 = mulps(x6, x5); // 0076f77c mulps xmm6, xmm5
    SXw(0x60, x6); // 0076f77f movaps xmmword ptr [esp + 0x60], xmm6
    x5 = shufps(x5, x5, 0xb1); // 0076f784 shufps xmm5, xmm5, 0xb1
    x6 = x5; // 0076f788 movaps xmm6, xmm5
    x5 = x4; // 0076f78b movaps xmm5, xmm4
    x5 = shufps(x5, x4, 0x55); // 0076f78e shufps xmm5, xmm4, 0x55
    x5 = mulps(x5, x6); // 0076f792 mulps xmm5, xmm6
    x5 = mulps(x5, ld(g, 0x93a200)); // 0076f795 mulps xmm5, xmmword ptr [0x93a200]
    x6 = shufps(x6, x6, 0x1b); // 0076f79c shufps xmm6, xmm6, 0x1b
    x6 = mulps(x6, x7); // 0076f7a0 mulps xmm6, xmm7
    x6 = mulps(x6, x3); // 0076f7a3 mulps xmm6, xmm3
    x6 = addps(x6, x5); // 0076f7a6 addps xmm6, xmm5
    x5 = x4; // 0076f7a9 movaps xmm5, xmm4
    x5 = shufps(x5, x4, 0xff); // 0076f7ac shufps xmm5, xmm4, 0xff
    x4 = SX(0x60); // 0076f7b0 movaps xmm4, xmmword ptr [esp + 0x60]
    x4 = mulps(x4, ld(g, 0x93a1f0)); // 0076f7b5 mulps xmm4, xmmword ptr [0x93a1f0]
    edi = A(0xc); // 0076f7bc mov edi, dword ptr [ebp + 0xc]
    x5 = mulps(x5, x0); // 0076f7bf mulps xmm5, xmm0
    x5 = addps(x5, x4); // 0076f7c2 addps xmm5, xmm4
    x6 = addps(x6, x5); // 0076f7c5 addps xmm6, xmm5
    SXw(0x60, x6); // 0076f7c8 movaps xmmword ptr [esp + 0x60], xmm6
    x6 = shufps(x6, x6, 0x1b); // 0076f7cd shufps xmm6, xmm6, 0x1b
    x5 = x0; // 0076f7d1 movaps xmm5, xmm0
    x5 = mulps(x5, ld(g, 0x93a220)); // 0076f7d4 mulps xmm5, xmmword ptr [0x93a220]
    x4 = x5; // 0076f7db movaps xmm4, xmm5
    x4 = shufps(x4, x5, 0); // 0076f7de shufps xmm4, xmm5, 0
    x4 = mulps(x4, x6); // 0076f7e2 mulps xmm4, xmm6
    x6 = shufps(x6, x6, 0xb1); // 0076f7e5 shufps xmm6, xmm6, 0xb1
    x7 = x6; // 0076f7e9 movaps xmm7, xmm6
    SXw(0x40, x4); // 0076f7ec movaps xmmword ptr [esp + 0x40], xmm4
    x6 = x5; // 0076f7f1 movaps xmm6, xmm5
    x6 = shufps(x6, x5, 0x55); // 0076f7f4 shufps xmm6, xmm5, 0x55
    x6 = mulps(x6, x7); // 0076f7f8 mulps xmm6, xmm7
    x6 = mulps(x6, ld(g, 0x93a200)); // 0076f7fb mulps xmm6, xmmword ptr [0x93a200]
    x4 = x5; // 0076f802 movaps xmm4, xmm5
    x4 = shufps(x4, x5, 0xaa); // 0076f805 shufps xmm4, xmm5, 0xaa
    x7 = shufps(x7, x7, 0x1b); // 0076f809 shufps xmm7, xmm7, 0x1b
    x4 = mulps(x4, x7); // 0076f80d mulps xmm4, xmm7
    x4 = mulps(x4, x3); // 0076f810 mulps xmm4, xmm3
    x4 = addps(x4, x6); // 0076f813 addps xmm4, xmm6
    x6 = x5; // 0076f816 movaps xmm6, xmm5
    x6 = shufps(x6, x5, 0xff); // 0076f819 shufps xmm6, xmm5, 0xff
    x6 = mulps(x6, SX(0x60)); // 0076f81d mulps xmm6, xmmword ptr [esp + 0x60]
    x5 = SX(0x40); // 0076f822 movaps xmm5, xmmword ptr [esp + 0x40]
    x5 = mulps(x5, ld(g, 0x93a1f0)); // 0076f827 mulps xmm5, xmmword ptr [0x93a1f0]
    x6 = addps(x6, x5); // 0076f82e addps xmm6, xmm5
    x5 = ld(g, edi + 0x0); // 0076f831 movaps xmm5, xmmword ptr [edi]
    x4 = addps(x4, x6); // 0076f834 addps xmm4, xmm6
    x4 = mulps(x4, x5); // 0076f837 mulps xmm4, xmm5
    x4 = andps(x4, x2); // 0076f83a andps xmm4, xmm2
    x5 = x0; // 0076f83d movaps xmm5, xmm0
    x5 = mulps(x5, ld(g, 0x93a220)); // 0076f840 mulps xmm5, xmmword ptr [0x93a220]
    SXw(0x40, x5); // 0076f847 movaps xmmword ptr [esp + 0x40], xmm5
    x5 = shufps(x5, x5, 0x1b); // 0076f84c shufps xmm5, xmm5, 0x1b
    x6 = x4; // 0076f850 movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0); // 0076f853 shufps xmm6, xmm4, 0
    x6 = mulps(x6, x5); // 0076f857 mulps xmm6, xmm5
    SXw(0x60, x6); // 0076f85a movaps xmmword ptr [esp + 0x60], xmm6
    x5 = shufps(x5, x5, 0xb1); // 0076f85f shufps xmm5, xmm5, 0xb1
    x6 = x4; // 0076f863 movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0x55); // 0076f866 shufps xmm6, xmm4, 0x55
    x6 = mulps(x6, x5); // 0076f86a mulps xmm6, xmm5
    x5 = shufps(x5, x5, 0x1b); // 0076f86d shufps xmm5, xmm5, 0x1b
    x6 = mulps(x6, ld(g, 0x93a200)); // 0076f871 mulps xmm6, xmmword ptr [0x93a200]
    x7 = x4; // 0076f878 movaps xmm7, xmm4
    x7 = shufps(x7, x4, 0xaa); // 0076f87b shufps xmm7, xmm4, 0xaa
    x5 = mulps(x5, x7); // 0076f87f mulps xmm5, xmm7
    x5 = mulps(x5, x3); // 0076f882 mulps xmm5, xmm3
    x5 = addps(x5, x6); // 0076f885 addps xmm5, xmm6
    x6 = x4; // 0076f888 movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0xff); // 0076f88b shufps xmm6, xmm4, 0xff
    x6 = mulps(x6, SX(0x40)); // 0076f88f mulps xmm6, xmmword ptr [esp + 0x40]
    x4 = SX(0x60); // 0076f894 movaps xmm4, xmmword ptr [esp + 0x60]
    x4 = mulps(x4, ld(g, 0x93a1f0)); // 0076f899 mulps xmm4, xmmword ptr [0x93a1f0]
    x6 = addps(x6, x4); // 0076f8a0 addps xmm6, xmm4
    x5 = addps(x5, x6); // 0076f8a3 addps xmm5, xmm6
    x4 = x5; // 0076f8a6 movaps xmm4, xmm5
    x4 = shufps(x4, x5, 0x1b); // 0076f8a9 shufps xmm4, xmm5, 0x1b
    x6 = x0; // 0076f8ad movaps xmm6, xmm0
    x6 = shufps(x6, x0, 0); // 0076f8b0 shufps xmm6, xmm0, 0
    x6 = mulps(x6, x4); // 0076f8b4 mulps xmm6, xmm4
    SXw(0x40, x6); // 0076f8b7 movaps xmmword ptr [esp + 0x40], xmm6
    x4 = shufps(x4, x4, 0xb1); // 0076f8bc shufps xmm4, xmm4, 0xb1
    x6 = x0; // 0076f8c0 movaps xmm6, xmm0
    x6 = shufps(x6, x0, 0x55); // 0076f8c3 shufps xmm6, xmm0, 0x55
    x6 = mulps(x6, x4); // 0076f8c7 mulps xmm6, xmm4
    x6 = mulps(x6, ld(g, 0x93a200)); // 0076f8ca mulps xmm6, xmmword ptr [0x93a200]
    x4 = shufps(x4, x4, 0x1b); // 0076f8d1 shufps xmm4, xmm4, 0x1b
    x7 = x0; // 0076f8d5 movaps xmm7, xmm0
    x7 = shufps(x7, x0, 0xaa); // 0076f8d8 shufps xmm7, xmm0, 0xaa
    x4 = mulps(x4, x7); // 0076f8dc mulps xmm4, xmm7
    x4 = mulps(x4, x3); // 0076f8df mulps xmm4, xmm3
    x4 = addps(x4, x6); // 0076f8e2 addps xmm4, xmm6
    x6 = x0; // 0076f8e5 movaps xmm6, xmm0
    x6 = shufps(x6, x0, 0xff); // 0076f8e8 shufps xmm6, xmm0, 0xff
    x6 = mulps(x6, x5); // 0076f8ec mulps xmm6, xmm5
    x5 = SX(0x40); // 0076f8ef movaps xmm5, xmmword ptr [esp + 0x40]
    x5 = mulps(x5, ld(g, 0x93a1f0)); // 0076f8f4 mulps xmm5, xmmword ptr [0x93a1f0]
    x6 = addps(x6, x5); // 0076f8fb addps xmm6, xmm5
    x5 = SX(0x80); // 0076f8fe movaps xmm5, xmmword ptr [esp + 0x80]
    x4 = addps(x4, x6); // 0076f906 addps xmm4, xmm6
    x5 = shufps(x5, x5, 0xc9); // 0076f909 shufps xmm5, xmm5, 0xc9
    x7 = x5; // 0076f90d movaps xmm7, xmm5
    x7 = shufps(x7, x5, 0xc9); // 0076f910 shufps xmm7, xmm5, 0xc9
    x4 = shufps(x4, x4, 0xc9); // 0076f914 shufps xmm4, xmm4, 0xc9
    SXw(0x60, x4); // 0076f918 movaps xmmword ptr [esp + 0x60], xmm4
    x4 = SX(0x20); // 0076f91d movaps xmm4, xmmword ptr [esp + 0x20]
    x4 = shufps(x4, x4, 0xd2); // 0076f922 shufps xmm4, xmm4, 0xd2
    x6 = x4; // 0076f926 movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0xd2); // 0076f929 shufps xmm6, xmm4, 0xd2
    x4 = mulps(x4, x5); // 0076f92d mulps xmm4, xmm5
    x6 = mulps(x6, x7); // 0076f930 mulps xmm6, xmm7
    x4 = subps(x4, x6); // 0076f933 subps xmm4, xmm6
    x4 = andps(x4, x2); // 0076f936 andps xmm4, xmm2
    x4 = andps(x4, x2); // 0076f939 andps xmm4, xmm2
    x6 = x4; // 0076f93c movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0); // 0076f93f shufps xmm6, xmm4, 0
    x5 = x1; // 0076f943 movaps xmm5, xmm1
    x5 = shufps(x5, x1, 0x1b); // 0076f946 shufps xmm5, xmm1, 0x1b
    x6 = mulps(x6, x5); // 0076f94a mulps xmm6, xmm5
    x5 = shufps(x5, x5, 0xb1); // 0076f94d shufps xmm5, xmm5, 0xb1
    SXw(0x40, x6); // 0076f951 movaps xmmword ptr [esp + 0x40], xmm6
    x6 = x5; // 0076f956 movaps xmm6, xmm5
    x5 = x4; // 0076f959 movaps xmm5, xmm4
    x5 = shufps(x5, x4, 0x55); // 0076f95c shufps xmm5, xmm4, 0x55
    x5 = mulps(x5, x6); // 0076f960 mulps xmm5, xmm6
    x5 = mulps(x5, ld(g, 0x93a200)); // 0076f963 mulps xmm5, xmmword ptr [0x93a200]
    x6 = shufps(x6, x6, 0x1b); // 0076f96a shufps xmm6, xmm6, 0x1b
    x7 = x4; // 0076f96e movaps xmm7, xmm4
    x7 = shufps(x7, x4, 0xaa); // 0076f971 shufps xmm7, xmm4, 0xaa
    x6 = mulps(x6, x7); // 0076f975 mulps xmm6, xmm7
    x6 = mulps(x6, x3); // 0076f978 mulps xmm6, xmm3
    x6 = addps(x6, x5); // 0076f97b addps xmm6, xmm5
    x5 = x4; // 0076f97e movaps xmm5, xmm4
    x5 = shufps(x5, x4, 0xff); // 0076f981 shufps xmm5, xmm4, 0xff
    x4 = SX(0x40); // 0076f985 movaps xmm4, xmmword ptr [esp + 0x40]
    x4 = mulps(x4, ld(g, 0x93a1f0)); // 0076f98a mulps xmm4, xmmword ptr [0x93a1f0]
    x5 = mulps(x5, x1); // 0076f991 mulps xmm5, xmm1
    x5 = addps(x5, x4); // 0076f994 addps xmm5, xmm4
    x6 = addps(x6, x5); // 0076f997 addps xmm6, xmm5
    SXw(0x40, x6); // 0076f99a movaps xmmword ptr [esp + 0x40], xmm6
    x5 = x1; // 0076f99f movaps xmm5, xmm1
    x5 = mulps(x5, ld(g, 0x93a220)); // 0076f9a2 mulps xmm5, xmmword ptr [0x93a220]
    x6 = shufps(x6, x6, 0x1b); // 0076f9a9 shufps xmm6, xmm6, 0x1b
    edi = A(0x20); // 0076f9ad mov edi, dword ptr [ebp + 0x20]
    x4 = x5; // 0076f9b0 movaps xmm4, xmm5
    x4 = shufps(x4, x5, 0); // 0076f9b3 shufps xmm4, xmm5, 0
    x4 = mulps(x4, x6); // 0076f9b7 mulps xmm4, xmm6
    x6 = shufps(x6, x6, 0xb1); // 0076f9ba shufps xmm6, xmm6, 0xb1
    SXw(0x80, x4); // 0076f9be movaps xmmword ptr [esp + 0x80], xmm4
    x4 = x6; // 0076f9c6 movaps xmm4, xmm6
    x6 = x5; // 0076f9c9 movaps xmm6, xmm5
    x6 = shufps(x6, x5, 0x55); // 0076f9cc shufps xmm6, xmm5, 0x55
    x6 = mulps(x6, x4); // 0076f9d0 mulps xmm6, xmm4
    x6 = mulps(x6, ld(g, 0x93a200)); // 0076f9d3 mulps xmm6, xmmword ptr [0x93a200]
    x4 = shufps(x4, x4, 0x1b); // 0076f9da shufps xmm4, xmm4, 0x1b
    x7 = x5; // 0076f9de movaps xmm7, xmm5
    x7 = shufps(x7, x5, 0xaa); // 0076f9e1 shufps xmm7, xmm5, 0xaa
    x4 = mulps(x4, x7); // 0076f9e5 mulps xmm4, xmm7
    x4 = mulps(x4, x3); // 0076f9e8 mulps xmm4, xmm3
    x4 = addps(x4, x6); // 0076f9eb addps xmm4, xmm6
    x6 = x5; // 0076f9ee movaps xmm6, xmm5
    x6 = shufps(x6, x5, 0xff); // 0076f9f1 shufps xmm6, xmm5, 0xff
    x6 = mulps(x6, SX(0x40)); // 0076f9f5 mulps xmm6, xmmword ptr [esp + 0x40]
    x5 = SX(0x80); // 0076f9fa movaps xmm5, xmmword ptr [esp + 0x80]
    x5 = mulps(x5, ld(g, 0x93a1f0)); // 0076fa02 mulps xmm5, xmmword ptr [0x93a1f0]
    x6 = addps(x6, x5); // 0076fa09 addps xmm6, xmm5
    x4 = addps(x4, x6); // 0076fa0c addps xmm4, xmm6
    x5 = ld(g, edi + 0x0); // 0076fa0f movaps xmm5, xmmword ptr [edi]
    x4 = mulps(x4, x5); // 0076fa12 mulps xmm4, xmm5
    x4 = andps(x4, x2); // 0076fa15 andps xmm4, xmm2
    x6 = x4; // 0076fa18 movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0); // 0076fa1b shufps xmm6, xmm4, 0
    x7 = x4; // 0076fa1f movaps xmm7, xmm4
    x7 = shufps(x7, x4, 0xaa); // 0076fa22 shufps xmm7, xmm4, 0xaa
    x5 = x1; // 0076fa26 movaps xmm5, xmm1
    x5 = mulps(x5, ld(g, 0x93a220)); // 0076fa29 mulps xmm5, xmmword ptr [0x93a220]
    SXw(0x40, x5); // 0076fa30 movaps xmmword ptr [esp + 0x40], xmm5
    x5 = shufps(x5, x5, 0x1b); // 0076fa35 shufps xmm5, xmm5, 0x1b
    x6 = mulps(x6, x5); // 0076fa39 mulps xmm6, xmm5
    SXw(0x80, x6); // 0076fa3c movaps xmmword ptr [esp + 0x80], xmm6
    x6 = x4; // 0076fa44 movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0x55); // 0076fa47 shufps xmm6, xmm4, 0x55
    x5 = shufps(x5, x5, 0xb1); // 0076fa4b shufps xmm5, xmm5, 0xb1
    x6 = mulps(x6, x5); // 0076fa4f mulps xmm6, xmm5
    x6 = mulps(x6, ld(g, 0x93a200)); // 0076fa52 mulps xmm6, xmmword ptr [0x93a200]
    x5 = shufps(x5, x5, 0x1b); // 0076fa59 shufps xmm5, xmm5, 0x1b
    x5 = mulps(x5, x7); // 0076fa5d mulps xmm5, xmm7
    x5 = mulps(x5, x3); // 0076fa60 mulps xmm5, xmm3
    x5 = addps(x5, x6); // 0076fa63 addps xmm5, xmm6
    x6 = x4; // 0076fa66 movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0xff); // 0076fa69 shufps xmm6, xmm4, 0xff
    x6 = mulps(x6, SX(0x40)); // 0076fa6d mulps xmm6, xmmword ptr [esp + 0x40]
    x4 = SX(0x80); // 0076fa72 movaps xmm4, xmmword ptr [esp + 0x80]
    x4 = mulps(x4, ld(g, 0x93a1f0)); // 0076fa7a mulps xmm4, xmmword ptr [0x93a1f0]
    x6 = addps(x6, x4); // 0076fa81 addps xmm6, xmm4
    x5 = addps(x5, x6); // 0076fa84 addps xmm5, xmm6
    x6 = x1; // 0076fa87 movaps xmm6, xmm1
    x6 = shufps(x6, x1, 0); // 0076fa8a shufps xmm6, xmm1, 0
    x4 = x5; // 0076fa8e movaps xmm4, xmm5
    x4 = shufps(x4, x5, 0x1b); // 0076fa91 shufps xmm4, xmm5, 0x1b
    x6 = mulps(x6, x4); // 0076fa95 mulps xmm6, xmm4
    SXw(0x40, x6); // 0076fa98 movaps xmmword ptr [esp + 0x40], xmm6
    x4 = shufps(x4, x4, 0xb1); // 0076fa9d shufps xmm4, xmm4, 0xb1
    x7 = x4; // 0076faa1 movaps xmm7, xmm4
    x6 = x1; // 0076faa4 movaps xmm6, xmm1
    x6 = shufps(x6, x1, 0x55); // 0076faa7 shufps xmm6, xmm1, 0x55
    x6 = mulps(x6, x7); // 0076faab mulps xmm6, xmm7
    x6 = mulps(x6, ld(g, 0x93a200)); // 0076faae mulps xmm6, xmmword ptr [0x93a200]
    x4 = x1; // 0076fab5 movaps xmm4, xmm1
    x4 = shufps(x4, x1, 0xaa); // 0076fab8 shufps xmm4, xmm1, 0xaa
    x7 = shufps(x7, x7, 0x1b); // 0076fabc shufps xmm7, xmm7, 0x1b
    x4 = mulps(x4, x7); // 0076fac0 mulps xmm4, xmm7
    x4 = mulps(x4, x3); // 0076fac3 mulps xmm4, xmm3
    x4 = addps(x4, x6); // 0076fac6 addps xmm4, xmm6
    x6 = x1; // 0076fac9 movaps xmm6, xmm1
    x6 = shufps(x6, x1, 0xff); // 0076facc shufps xmm6, xmm1, 0xff
    x1 = SX(0x40); // 0076fad0 movaps xmm1, xmmword ptr [esp + 0x40]
    x1 = mulps(x1, ld(g, 0x93a1f0)); // 0076fad5 mulps xmm1, xmmword ptr [0x93a1f0]
    x6 = mulps(x6, x5); // 0076fadc mulps xmm6, xmm5
    x6 = addps(x6, x1); // 0076fadf addps xmm6, xmm1
    x1 = SX(0x30); // 0076fae2 movaps xmm1, xmmword ptr [esp + 0x30]
    x4 = addps(x4, x6); // 0076fae7 addps xmm4, xmm6
    x4 = shufps(x4, x4, 0xc9); // 0076faea shufps xmm4, xmm4, 0xc9
    x6 = x1; // 0076faee movaps xmm6, xmm1
    x5 = SX(0x60); // 0076faf1 movaps xmm5, xmmword ptr [esp + 0x60]
    x6 = shufps(x6, x1, 0xd2); // 0076faf6 shufps xmm6, xmm1, 0xd2
    x1 = mulps(x1, x5); // 0076fafa mulps xmm1, xmm5
    edi = A(0x8); // 0076fafd mov edi, dword ptr [ebp + 8]
    x7 = x5; // 0076fb00 movaps xmm7, xmm5
    x7 = shufps(x7, x5, 0xc9); // 0076fb03 shufps xmm7, xmm5, 0xc9
    x5 = SX(0xa0); // 0076fb07 movaps xmm5, xmmword ptr [esp + 0xa0]
    x6 = mulps(x6, x7); // 0076fb0f mulps xmm6, xmm7
    x1 = subps(x1, x6); // 0076fb12 subps xmm1, xmm6
    x1 = andps(x1, x2); // 0076fb15 andps xmm1, xmm2
    x7 = x4; // 0076fb18 movaps xmm7, xmm4
    x7 = shufps(x7, x4, 0xc9); // 0076fb1b shufps xmm7, xmm4, 0xc9
    x6 = x5; // 0076fb1f movaps xmm6, xmm5
    x6 = shufps(x6, x5, 0xd2); // 0076fb22 shufps xmm6, xmm5, 0xd2
    x5 = mulps(x5, x4); // 0076fb26 mulps xmm5, xmm4
    x6 = mulps(x6, x7); // 0076fb29 mulps xmm6, xmm7
    x5 = subps(x5, x6); // 0076fb2c subps xmm5, xmm6
    x5 = andps(x5, x2); // 0076fb2f andps xmm5, xmm2
    x1 = addps(x1, x5); // 0076fb32 addps xmm1, xmm5
    x5 = SX(0x20); // 0076fb35 movaps xmm5, xmmword ptr [esp + 0x20]
    x1 = mulps(x1, x5); // 0076fb3a mulps xmm1, xmm5
    x4 = x1; // 0076fb3d movaps xmm4, xmm1
    x4 = shufps(x4, x1, 0x99); // 0076fb40 shufps xmm4, xmm1, 0x99
    x1.u[0] = x86::bits(x86::add(x1.f(0), (x4).f(0))); // 0076fb44 addss xmm1, xmm4
    x4 = shufps(x4, x4, 0x55); // 0076fb48 shufps xmm4, xmm4, 0x55
    x1.u[0] = x86::bits(x86::add(x1.f(0), (x4).f(0))); // 0076fb4c addss xmm1, xmm4
    x4 = ld(g, edi + 0x0); // 0076fb50 movaps xmm4, xmmword ptr [edi]
    edi = A(0x1c); // 0076fb53 mov edi, dword ptr [ebp + 0x1c]
    x6 = x1; // 0076fb56 movaps xmm6, xmm1
    x6 = shufps(x6, x1, 0); // 0076fb59 shufps xmm6, xmm1, 0
    x1 = x4; // 0076fb5d movaps xmm1, xmm4
    SXw(0xa0, x4); // 0076fb60 movaps xmmword ptr [esp + 0xa0], xmm4
    x4 = ld(g, edi + 0x0); // 0076fb68 movaps xmm4, xmmword ptr [edi]
    edi = A(0x34); // 0076fb6b mov edi, dword ptr [ebp + 0x34]
    x1 = addps(x1, x4); // 0076fb6e addps xmm1, xmm4
    x4 = ld(g, 0xd61650); // 0076fb71 movaps xmm4, xmmword ptr [0xd61650]
    x6 = addps(x6, x1); // 0076fb78 addps xmm6, xmm1
    x1 = ld(g, edi + 0x0); // 0076fb7b movaps xmm1, xmmword ptr [edi]
    x1 = addps(x1, x4); // 0076fb7e addps xmm1, xmm4
    x7 = X::splat(0); // 0076fb81 xorps xmm7, xmm7
    x4 = x7; // 0076fb84 movaps xmm4, xmm7
    x4 = subps(x4, x1); // 0076fb87 subps xmm4, xmm1
    x4 = mulps(x4, SX(0x70)); // 0076fb8a mulps xmm4, xmmword ptr [esp + 0x70]
    x4 = divps(x4, x6); // 0076fb8f divps xmm4, xmm6
    x1 = x7; // 0076fb92 movaps xmm1, xmm7
    x1 = subps(x1, x4); // 0076fb95 subps xmm1, xmm4
    x1 = mulps(x1, x5); // 0076fb98 mulps xmm1, xmm5
    x5 = mulps(x5, SX(0x70)); // 0076fb9b mulps xmm5, xmmword ptr [esp + 0x70]
    SXw(0x40, x1); // 0076fba0 movaps xmmword ptr [esp + 0x40], xmm1
    x1 = SX(0x50); // 0076fba5 movaps xmm1, xmmword ptr [esp + 0x50]
    x1 = subps(x1, x5); // 0076fbaa subps xmm1, xmm5
    x5 = x1; // 0076fbad movaps xmm5, xmm1
    x1 = mulps(x1, x5); // 0076fbb0 mulps xmm1, xmm5
    x4 = x1; // 0076fbb3 movaps xmm4, xmm1
    x4 = shufps(x4, x1, 0x66); // 0076fbb6 shufps xmm4, xmm1, 0x66
    x1.u[0] = x86::bits(x86::add(x1.f(0), (x4).f(0))); // 0076fbba addss xmm1, xmm4
    x4 = shufps(x4, x4, 0x55); // 0076fbbe shufps xmm4, xmm4, 0x55
    x1.u[0] = x86::bits(x86::add(x1.f(0), (x4).f(0))); // 0076fbc2 addss xmm1, xmm4
    x1 = shufps(x1, x1, 0); // 0076fbc6 shufps xmm1, xmm1, 0
    x4 = sqrtps(x1); // 0076fbca sqrtps xmm4, xmm1
    x1 = X{{g.u32(0x9322ac), 0, 0, 0}}; // 0076fbcd movss xmm1, dword ptr [0x9322ac]
    x1 = shufps(x1, x1, 0); // 0076fbd5 shufps xmm1, xmm1, 0
    x1 = cmpltps(x1, x4); // 0076fbd9 cmpltps xmm1, xmm4
    SXw(0x60, x1); // 0076fbdd movaps xmmword ptr [esp + 0x60], xmm1
    edi = S[0x64 / 4]; // 0076fbe2 mov edi, dword ptr [esp + 0x64]
    ebx = S[0x68 / 4]; // 0076fbe6 mov ebx, dword ptr [esp + 0x68]
    ebx = ebx + edi; // 0076fbea add ebx, edi
    ebx = ebx + S[0x60 / 4]; // 0076fbec add ebx, dword ptr [esp + 0x60]
    // 0076fbf0 cmp ebx, -3
    if (int32_t(ebx) != int32_t(uint32_t(-3))) goto L_0076fc09; // 0076fbf3 jne 0x76fc09
    edi = A(0x38); // 0076fbf5 mov edi, dword ptr [ebp + 0x38]
    x1 = ld(g, edi + 0x0); // 0076fbf8 movaps xmm1, xmmword ptr [edi]
    x1 = mulps(x1, x4); // 0076fbfb mulps xmm1, xmm4
    x1 = divps(x1, x6); // 0076fbfe divps xmm1, xmm6
    x5 = divps(x5, x4); // 0076fc01 divps xmm5, xmm4
    x1 = mulps(x1, x5); // 0076fc04 mulps xmm1, xmm5
    goto L_0076fc10; // 0076fc07 jmp 0x76fc10
  L_0076fc09:
    x1 = ld(g, 0xd61670); // 0076fc09 movaps xmm1, xmmword ptr [0xd61670]
  L_0076fc10:
    x1 = addps(x1, SX(0x40)); // 0076fc10 addps xmm1, xmmword ptr [esp + 0x40]
    x4 = SX(0x10); // 0076fc15 movaps xmm4, xmmword ptr [esp + 0x10]
    x6 = x1; // 0076fc1a movaps xmm6, xmm1
    x1 = shufps(x1, x6, 0xd2); // 0076fc1d shufps xmm1, xmm6, 0xd2
    x7 = subps(x7, x6); // 0076fc21 subps xmm7, xmm6
    x5 = x1; // 0076fc24 movaps xmm5, xmm1
    x5 = shufps(x5, x1, 0xd2); // 0076fc27 shufps xmm5, xmm1, 0xd2
    x4 = shufps(x4, x4, 0xc9); // 0076fc2b shufps xmm4, xmm4, 0xc9
    x1 = mulps(x1, x4); // 0076fc2f mulps xmm1, xmm4
    SXw(0x80, x7); // 0076fc32 movaps xmmword ptr [esp + 0x80], xmm7
    x7 = x4; // 0076fc3a movaps xmm7, xmm4
    x7 = shufps(x7, x4, 0xc9); // 0076fc3d shufps xmm7, xmm4, 0xc9
    x5 = mulps(x5, x7); // 0076fc41 mulps xmm5, xmm7
    x1 = subps(x1, x5); // 0076fc44 subps xmm1, xmm5
    x1 = andps(x1, x2); // 0076fc47 andps xmm1, xmm2
    x1 = andps(x1, x2); // 0076fc4a andps xmm1, xmm2
    x4 = x1; // 0076fc4d movaps xmm4, xmm1
    x4 = shufps(x4, x1, 0); // 0076fc50 shufps xmm4, xmm1, 0
    x5 = x1; // 0076fc54 movaps xmm5, xmm1
    x5 = shufps(x5, x1, 0x55); // 0076fc57 shufps xmm5, xmm1, 0x55
    x7 = x1; // 0076fc5b movaps xmm7, xmm1
    x7 = shufps(x7, x1, 0xaa); // 0076fc5e shufps xmm7, xmm1, 0xaa
    x2 = x0; // 0076fc62 movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0x1b); // 0076fc65 shufps xmm2, xmm0, 0x1b
    x4 = mulps(x4, x2); // 0076fc69 mulps xmm4, xmm2
    x2 = shufps(x2, x2, 0xb1); // 0076fc6c shufps xmm2, xmm2, 0xb1
    x5 = mulps(x5, x2); // 0076fc70 mulps xmm5, xmm2
    x2 = shufps(x2, x2, 0x1b); // 0076fc73 shufps xmm2, xmm2, 0x1b
    x2 = mulps(x2, x7); // 0076fc77 mulps xmm2, xmm7
    x7 = ld(g, 0x93a200); // 0076fc7a movaps xmm7, xmmword ptr [0x93a200]
    x5 = mulps(x5, x7); // 0076fc81 mulps xmm5, xmm7
    x2 = mulps(x2, x3); // 0076fc84 mulps xmm2, xmm3
    x2 = addps(x2, x5); // 0076fc87 addps xmm2, xmm5
    x5 = x1; // 0076fc8a movaps xmm5, xmm1
    x5 = shufps(x5, x1, 0xff); // 0076fc8d shufps xmm5, xmm1, 0xff
    x5 = mulps(x5, x0); // 0076fc91 mulps xmm5, xmm0
    x0 = mulps(x0, ld(g, 0x93a220)); // 0076fc94 mulps xmm0, xmmword ptr [0x93a220]
    SXw(0x50, x5); // 0076fc9b movaps xmmword ptr [esp + 0x50], xmm5
    x5 = ld(g, 0x93a1f0); // 0076fca0 movaps xmm5, xmmword ptr [0x93a1f0]
    x1 = SX(0x50); // 0076fca7 movaps xmm1, xmmword ptr [esp + 0x50]
    x4 = mulps(x4, x5); // 0076fcac mulps xmm4, xmm5
    x1 = addps(x1, x4); // 0076fcaf addps xmm1, xmm4
    x2 = addps(x2, x1); // 0076fcb2 addps xmm2, xmm1
    x4 = x0; // 0076fcb5 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0); // 0076fcb8 shufps xmm4, xmm0, 0
    SXw(0x50, x7); // 0076fcbc movaps xmmword ptr [esp + 0x50], xmm7
    x7 = SX(0xa0); // 0076fcc1 movaps xmm7, xmmword ptr [esp + 0xa0]
    x7 = mulps(x7, x6); // 0076fcc9 mulps xmm7, xmm6
    x7 = addps(x7, SX(0x90)); // 0076fccc addps xmm7, xmmword ptr [esp + 0x90]
    SXw(0x40, x5); // 0076fcd4 movaps xmmword ptr [esp + 0x40], xmm5
    x5 = x0; // 0076fcd9 movaps xmm5, xmm0
    x1 = x2; // 0076fcdc movaps xmm1, xmm2
    x1 = shufps(x1, x2, 0x1b); // 0076fcdf shufps xmm1, xmm2, 0x1b
    x4 = mulps(x4, x1); // 0076fce3 mulps xmm4, xmm1
    x1 = shufps(x1, x1, 0xb1); // 0076fce6 shufps xmm1, xmm1, 0xb1
    x6 = x1; // 0076fcea movaps xmm6, xmm1
    x6 = shufps(x6, x1, 0x1b); // 0076fced shufps xmm6, xmm1, 0x1b
    x5 = shufps(x5, x0, 0x55); // 0076fcf1 shufps xmm5, xmm0, 0x55
    x5 = mulps(x5, x1); // 0076fcf5 mulps xmm5, xmm1
    x1 = x0; // 0076fcf8 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0xaa); // 0076fcfb shufps xmm1, xmm0, 0xaa
    x6 = mulps(x6, x1); // 0076fcff mulps xmm6, xmm1
    x1 = SX(0x50); // 0076fd02 movaps xmm1, xmmword ptr [esp + 0x50]
    x1 = mulps(x1, x5); // 0076fd07 mulps xmm1, xmm5
    x6 = mulps(x6, x3); // 0076fd0a mulps xmm6, xmm3
    x6 = addps(x6, x1); // 0076fd0d addps xmm6, xmm1
    x1 = x0; // 0076fd10 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0xff); // 0076fd13 shufps xmm1, xmm0, 0xff
    x0 = SX(0x40); // 0076fd17 movaps xmm0, xmmword ptr [esp + 0x40]
    st(g, esi + 0x0, x7); // 0076fd1c movaps xmmword ptr [esi], xmm7
    esi = A(0xc); // 0076fd1f mov esi, dword ptr [ebp + 0xc]
    x0 = mulps(x0, x4); // 0076fd22 mulps xmm0, xmm4
    x1 = mulps(x1, x2); // 0076fd25 mulps xmm1, xmm2
    x1 = addps(x1, x0); // 0076fd28 addps xmm1, xmm0
    x0 = ld(g, esi + 0x0); // 0076fd2b movaps xmm0, xmmword ptr [esi]
    x6 = addps(x6, x1); // 0076fd2e addps xmm6, xmm1
    x6 = mulps(x6, x0); // 0076fd31 mulps xmm6, xmm0
    x0 = ld(g, eax + 0x0); // 0076fd34 movaps xmm0, xmmword ptr [eax]
    x6 = addps(x6, x0); // 0076fd37 addps xmm6, xmm0
    st(g, eax + 0x0, x6); // 0076fd3a movaps xmmword ptr [eax], xmm6
    eax = A(0x3c); // 0076fd3d mov eax, dword ptr [ebp + 0x3c]
    x0 = ld(g, eax + 0x0); // 0076fd40 movaps xmm0, xmmword ptr [eax]
    eax = A(0x28); // 0076fd43 mov eax, dword ptr [ebp + 0x28]
    x1 = ld(g, eax + 0x0); // 0076fd46 movaps xmm1, xmmword ptr [eax]
    x6 = SX(0x80); // 0076fd49 movaps xmm6, xmmword ptr [esp + 0x80]
    eax = A(0x24); // 0076fd51 mov eax, dword ptr [ebp + 0x24]
    x2 = ld(g, eax + 0x0); // 0076fd54 movaps xmm2, xmmword ptr [eax]
    x0 = subps(x0, x1); // 0076fd57 subps xmm0, xmm1
    x7 = ld(g, 0x93a200); // 0076fd5a movaps xmm7, xmmword ptr [0x93a200]
    eax = A(0x1c); // 0076fd61 mov eax, dword ptr [ebp + 0x1c]
    x0 = shufps(x0, x0, 0xc9); // 0076fd64 shufps xmm0, xmm0, 0xc9
    x1 = x0; // 0076fd68 movaps xmm1, xmm0
    x4 = x1; // 0076fd6b movaps xmm4, xmm1
    x4 = shufps(x4, x1, 0xc9); // 0076fd6e shufps xmm4, xmm1, 0xc9
    x0 = x6; // 0076fd72 movaps xmm0, xmm6
    x0 = shufps(x0, x6, 0xd2); // 0076fd75 shufps xmm0, xmm6, 0xd2
    x3 = x0; // 0076fd79 movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0xd2); // 0076fd7c shufps xmm3, xmm0, 0xd2
    x3 = mulps(x3, x4); // 0076fd80 mulps xmm3, xmm4
    x0 = mulps(x0, x1); // 0076fd83 mulps xmm0, xmm1
    x1 = ld(g, 0x92db80); // 0076fd86 movaps xmm1, xmmword ptr [0x92db80]
    x0 = subps(x0, x3); // 0076fd8d subps xmm0, xmm3
    x0 = andps(x0, x1); // 0076fd90 andps xmm0, xmm1
    x0 = andps(x0, x1); // 0076fd93 andps xmm0, xmm1
    x1 = x2; // 0076fd96 movaps xmm1, xmm2
    x1 = shufps(x1, x2, 0x1b); // 0076fd99 shufps xmm1, xmm2, 0x1b
    x3 = x0; // 0076fd9d movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0); // 0076fda0 shufps xmm3, xmm0, 0
    x3 = mulps(x3, x1); // 0076fda4 mulps xmm3, xmm1
    x1 = shufps(x1, x1, 0xb1); // 0076fda7 shufps xmm1, xmm1, 0xb1
    x5 = x1; // 0076fdab movaps xmm5, xmm1
    x4 = x0; // 0076fdae movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x55); // 0076fdb1 shufps xmm4, xmm0, 0x55
    x4 = mulps(x4, x5); // 0076fdb5 mulps xmm4, xmm5
    x4 = mulps(x4, x7); // 0076fdb8 mulps xmm4, xmm7
    x1 = x0; // 0076fdbb movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0xaa); // 0076fdbe shufps xmm1, xmm0, 0xaa
    x5 = shufps(x5, x5, 0x1b); // 0076fdc2 shufps xmm5, xmm5, 0x1b
    x1 = mulps(x1, x5); // 0076fdc6 mulps xmm1, xmm5
    x1 = mulps(x1, ld(g, 0x93a210)); // 0076fdc9 mulps xmm1, xmmword ptr [0x93a210]
    x5 = ld(g, 0x93a1f0); // 0076fdd0 movaps xmm5, xmmword ptr [0x93a1f0]
    x1 = addps(x1, x4); // 0076fdd7 addps xmm1, xmm4
    x4 = x0; // 0076fdda movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0xff); // 0076fddd shufps xmm4, xmm0, 0xff
    x0 = ld(g, 0x93a220); // 0076fde1 movaps xmm0, xmmword ptr [0x93a220]
    x4 = mulps(x4, x2); // 0076fde8 mulps xmm4, xmm2
    x2 = mulps(x2, x0); // 0076fdeb mulps xmm2, xmm0
    x0 = x2; // 0076fdee movaps xmm0, xmm2
    x3 = mulps(x3, x5); // 0076fdf1 mulps xmm3, xmm5
    x4 = addps(x4, x3); // 0076fdf4 addps xmm4, xmm3
    SXw(0x40, x7); // 0076fdf7 movaps xmmword ptr [esp + 0x40], xmm7
    x7 = ld(g, 0x93a210); // 0076fdfc movaps xmm7, xmmword ptr [0x93a210]
    x1 = addps(x1, x4); // 0076fe03 addps xmm1, xmm4
    SXw(0x50, x7); // 0076fe06 movaps xmmword ptr [esp + 0x50], xmm7
    x7 = ld(g, eax + 0x0); // 0076fe0b movaps xmm7, xmmword ptr [eax]
    x7 = mulps(x7, x6); // 0076fe0e mulps xmm7, xmm6
    x6 = ld(g, edx + 0x0); // 0076fe11 movaps xmm6, xmmword ptr [edx]
    x7 = addps(x7, x6); // 0076fe14 addps xmm7, xmm6
    x2 = x1; // 0076fe17 movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0x1b); // 0076fe1a shufps xmm2, xmm1, 0x1b
    x3 = x0; // 0076fe1e movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0); // 0076fe21 shufps xmm3, xmm0, 0
    x3 = mulps(x3, x2); // 0076fe25 mulps xmm3, xmm2
    x2 = shufps(x2, x2, 0xb1); // 0076fe28 shufps xmm2, xmm2, 0xb1
    x6 = x2; // 0076fe2c movaps xmm6, xmm2
    x6 = shufps(x6, x2, 0x1b); // 0076fe2f shufps xmm6, xmm2, 0x1b
    x4 = x0; // 0076fe33 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x55); // 0076fe36 shufps xmm4, xmm0, 0x55
    x4 = mulps(x4, x2); // 0076fe3a mulps xmm4, xmm2
    x2 = x0; // 0076fe3d movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0xaa); // 0076fe40 shufps xmm2, xmm0, 0xaa
    x6 = mulps(x6, x2); // 0076fe44 mulps xmm6, xmm2
    x2 = SX(0x40); // 0076fe47 movaps xmm2, xmmword ptr [esp + 0x40]
    x6 = mulps(x6, SX(0x50)); // 0076fe4c mulps xmm6, xmmword ptr [esp + 0x50]
    x2 = mulps(x2, x4); // 0076fe51 mulps xmm2, xmm4
    x6 = addps(x6, x2); // 0076fe54 addps xmm6, xmm2
    x2 = x0; // 0076fe57 movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0xff); // 0076fe5a shufps xmm2, xmm0, 0xff
    x2 = mulps(x2, x1); // 0076fe5e mulps xmm2, xmm1
    x5 = mulps(x5, x3); // 0076fe61 mulps xmm5, xmm3
    st(g, edx + 0x0, x7); // 0076fe64 movaps xmmword ptr [edx], xmm7
    x2 = addps(x2, x5); // 0076fe67 addps xmm2, xmm5
    edx = A(0x20); // 0076fe6a mov edx, dword ptr [ebp + 0x20]
    x0 = ld(g, edx + 0x0); // 0076fe6d movaps xmm0, xmmword ptr [edx]
    x6 = addps(x6, x2); // 0076fe70 addps xmm6, xmm2
    x6 = mulps(x6, x0); // 0076fe73 mulps xmm6, xmm0
    x0 = ld(g, ecx + 0x0); // 0076fe76 movaps xmm0, xmmword ptr [ecx]
    x6 = addps(x6, x0); // 0076fe79 addps xmm6, xmm0
    st(g, ecx + 0x0, x6); // 0076fe7c movaps xmmword ptr [ecx], xmm6
    // 0076fe7f pop edi  (prologue / epilogue: skipped)
    // 0076fe80 pop esi  (prologue / epilogue: skipped)
    // 0076fe81 pop ebx  (prologue / epilogue: skipped)
    // 0076fe82 mov esp, ebp  (epilogue: skipped)
    // 0076fe84 pop ebp  (prologue / epilogue: skipped)
    goto L_return; // 0076fe85 ret 0x3c
  L_0076fe88:
    x6 = x4; // 0076fe88 movaps xmm6, xmm4
    x6 = mulps(x6, SX(0x70)); // 0076fe8b mulps xmm6, xmmword ptr [esp + 0x70]
    x5 = subps(x5, x6); // 0076fe90 subps xmm5, xmm6
    SXw(0xb0, x5); // 0076fe93 movaps xmmword ptr [esp + 0xb0], xmm5
    x5 = mulps(x5, x5); // 0076fe9b mulps xmm5, xmm5
    x6 = x5; // 0076fe9e movaps xmm6, xmm5
    x6 = shufps(x6, x5, 0x66); // 0076fea1 shufps xmm6, xmm5, 0x66
    x5.u[0] = x86::bits(x86::add(x5.f(0), (x6).f(0))); // 0076fea5 addss xmm5, xmm6
    x6 = shufps(x6, x6, 0x55); // 0076fea9 shufps xmm6, xmm6, 0x55
    x5.u[0] = x86::bits(x86::add(x5.f(0), (x6).f(0))); // 0076fead addss xmm5, xmm6
    x5 = shufps(x5, x5, 0); // 0076feb1 shufps xmm5, xmm5, 0
    x6 = sqrtps(x5); // 0076feb5 sqrtps xmm6, xmm5
    x5 = X{{g.u32(0x9322ac), 0, 0, 0}}; // 0076feb8 movss xmm5, dword ptr [0x9322ac]
    x5 = shufps(x5, x5, 0); // 0076fec0 shufps xmm5, xmm5, 0
    x5 = cmpltps(x5, x6); // 0076fec4 cmpltps xmm5, xmm6
    SXw(0x70, x5); // 0076fec8 movaps xmmword ptr [esp + 0x70], xmm5
    edi = S[0x74 / 4]; // 0076fecd mov edi, dword ptr [esp + 0x74]
    ebx = S[0x78 / 4]; // 0076fed1 mov ebx, dword ptr [esp + 0x78]
    x5 = SX(0x10); // 0076fed5 movaps xmm5, xmmword ptr [esp + 0x10]
    ebx = ebx + edi; // 0076feda add ebx, edi
    ebx = ebx + S[0x70 / 4]; // 0076fedc add ebx, dword ptr [esp + 0x70]
    SXw(0x60, x6); // 0076fee0 movaps xmmword ptr [esp + 0x60], xmm6
    // 0076fee5 cmp ebx, -3
    if (int32_t(ebx) != int32_t(uint32_t(-3))) goto L_00770326; // 0076fee8 jne 0x770326
    x5 = shufps(x5, x5, 0xc9); // 0076feee shufps xmm5, xmm5, 0xc9
    x7 = x5; // 0076fef2 movaps xmm7, xmm5
    x7 = shufps(x7, x5, 0xc9); // 0076fef5 shufps xmm7, xmm5, 0xc9
    x4 = shufps(x4, x4, 0xd2); // 0076fef9 shufps xmm4, xmm4, 0xd2
    x6 = x4; // 0076fefd movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0xd2); // 0076ff00 shufps xmm6, xmm4, 0xd2
    x4 = mulps(x4, x5); // 0076ff04 mulps xmm4, xmm5
    x6 = mulps(x6, x7); // 0076ff07 mulps xmm6, xmm7
    x4 = subps(x4, x6); // 0076ff0a subps xmm4, xmm6
    x4 = andps(x4, x2); // 0076ff0d andps xmm4, xmm2
    x4 = andps(x4, x2); // 0076ff10 andps xmm4, xmm2
    x6 = x4; // 0076ff13 movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0); // 0076ff16 shufps xmm6, xmm4, 0
    x7 = x4; // 0076ff1a movaps xmm7, xmm4
    x7 = shufps(x7, x4, 0xaa); // 0076ff1d shufps xmm7, xmm4, 0xaa
    x5 = x0; // 0076ff21 movaps xmm5, xmm0
    x5 = shufps(x5, x0, 0x1b); // 0076ff24 shufps xmm5, xmm0, 0x1b
    x6 = mulps(x6, x5); // 0076ff28 mulps xmm6, xmm5
    SXw(0x50, x6); // 0076ff2b movaps xmmword ptr [esp + 0x50], xmm6
    x5 = shufps(x5, x5, 0xb1); // 0076ff30 shufps xmm5, xmm5, 0xb1
    x6 = x5; // 0076ff34 movaps xmm6, xmm5
    x5 = x4; // 0076ff37 movaps xmm5, xmm4
    x5 = shufps(x5, x4, 0x55); // 0076ff3a shufps xmm5, xmm4, 0x55
    x5 = mulps(x5, x6); // 0076ff3e mulps xmm5, xmm6
    x5 = mulps(x5, ld(g, 0x93a200)); // 0076ff41 mulps xmm5, xmmword ptr [0x93a200]
    x6 = shufps(x6, x6, 0x1b); // 0076ff48 shufps xmm6, xmm6, 0x1b
    x6 = mulps(x6, x7); // 0076ff4c mulps xmm6, xmm7
    x6 = mulps(x6, x3); // 0076ff4f mulps xmm6, xmm3
    x6 = addps(x6, x5); // 0076ff52 addps xmm6, xmm5
    x5 = x4; // 0076ff55 movaps xmm5, xmm4
    x5 = shufps(x5, x4, 0xff); // 0076ff58 shufps xmm5, xmm4, 0xff
    x4 = SX(0x50); // 0076ff5c movaps xmm4, xmmword ptr [esp + 0x50]
    x4 = mulps(x4, ld(g, 0x93a1f0)); // 0076ff61 mulps xmm4, xmmword ptr [0x93a1f0]
    edi = A(0xc); // 0076ff68 mov edi, dword ptr [ebp + 0xc]
    x5 = mulps(x5, x0); // 0076ff6b mulps xmm5, xmm0
    x5 = addps(x5, x4); // 0076ff6e addps xmm5, xmm4
    x6 = addps(x6, x5); // 0076ff71 addps xmm6, xmm5
    SXw(0x50, x6); // 0076ff74 movaps xmmword ptr [esp + 0x50], xmm6
    x6 = shufps(x6, x6, 0x1b); // 0076ff79 shufps xmm6, xmm6, 0x1b
    x5 = x0; // 0076ff7d movaps xmm5, xmm0
    x5 = mulps(x5, ld(g, 0x93a220)); // 0076ff80 mulps xmm5, xmmword ptr [0x93a220]
    x4 = x5; // 0076ff87 movaps xmm4, xmm5
    x4 = shufps(x4, x5, 0); // 0076ff8a shufps xmm4, xmm5, 0
    x4 = mulps(x4, x6); // 0076ff8e mulps xmm4, xmm6
    x6 = shufps(x6, x6, 0xb1); // 0076ff91 shufps xmm6, xmm6, 0xb1
    SXw(0x40, x4); // 0076ff95 movaps xmmword ptr [esp + 0x40], xmm4
    x4 = x6; // 0076ff9a movaps xmm4, xmm6
    x6 = x5; // 0076ff9d movaps xmm6, xmm5
    x6 = shufps(x6, x5, 0x55); // 0076ffa0 shufps xmm6, xmm5, 0x55
    x6 = mulps(x6, x4); // 0076ffa4 mulps xmm6, xmm4
    x6 = mulps(x6, ld(g, 0x93a200)); // 0076ffa7 mulps xmm6, xmmword ptr [0x93a200]
    x7 = x5; // 0076ffae movaps xmm7, xmm5
    x7 = shufps(x7, x5, 0xaa); // 0076ffb1 shufps xmm7, xmm5, 0xaa
    x4 = shufps(x4, x4, 0x1b); // 0076ffb5 shufps xmm4, xmm4, 0x1b
    x4 = mulps(x4, x7); // 0076ffb9 mulps xmm4, xmm7
    x4 = mulps(x4, x3); // 0076ffbc mulps xmm4, xmm3
    x4 = addps(x4, x6); // 0076ffbf addps xmm4, xmm6
    x6 = x5; // 0076ffc2 movaps xmm6, xmm5
    x6 = shufps(x6, x5, 0xff); // 0076ffc5 shufps xmm6, xmm5, 0xff
    x6 = mulps(x6, SX(0x50)); // 0076ffc9 mulps xmm6, xmmword ptr [esp + 0x50]
    x5 = SX(0x40); // 0076ffce movaps xmm5, xmmword ptr [esp + 0x40]
    x5 = mulps(x5, ld(g, 0x93a1f0)); // 0076ffd3 mulps xmm5, xmmword ptr [0x93a1f0]
    x6 = addps(x6, x5); // 0076ffda addps xmm6, xmm5
    x5 = ld(g, edi + 0x0); // 0076ffdd movaps xmm5, xmmword ptr [edi]
    x4 = addps(x4, x6); // 0076ffe0 addps xmm4, xmm6
    x4 = mulps(x4, x5); // 0076ffe3 mulps xmm4, xmm5
    x4 = andps(x4, x2); // 0076ffe6 andps xmm4, xmm2
    x5 = x0; // 0076ffe9 movaps xmm5, xmm0
    x5 = mulps(x5, ld(g, 0x93a220)); // 0076ffec mulps xmm5, xmmword ptr [0x93a220]
    SXw(0x50, x5); // 0076fff3 movaps xmmword ptr [esp + 0x50], xmm5
    x5 = shufps(x5, x5, 0x1b); // 0076fff8 shufps xmm5, xmm5, 0x1b
    x6 = x4; // 0076fffc movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0); // 0076ffff shufps xmm6, xmm4, 0
    x6 = mulps(x6, x5); // 00770003 mulps xmm6, xmm5
    SXw(0x40, x6); // 00770006 movaps xmmword ptr [esp + 0x40], xmm6
    x5 = shufps(x5, x5, 0xb1); // 0077000b shufps xmm5, xmm5, 0xb1
    x6 = x4; // 0077000f movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0x55); // 00770012 shufps xmm6, xmm4, 0x55
    x6 = mulps(x6, x5); // 00770016 mulps xmm6, xmm5
    x5 = shufps(x5, x5, 0x1b); // 00770019 shufps xmm5, xmm5, 0x1b
    x7 = x4; // 0077001d movaps xmm7, xmm4
    x6 = mulps(x6, ld(g, 0x93a200)); // 00770020 mulps xmm6, xmmword ptr [0x93a200]
    x7 = shufps(x7, x4, 0xaa); // 00770027 shufps xmm7, xmm4, 0xaa
    x5 = mulps(x5, x7); // 0077002b mulps xmm5, xmm7
    x5 = mulps(x5, x3); // 0077002e mulps xmm5, xmm3
    x5 = addps(x5, x6); // 00770031 addps xmm5, xmm6
    x6 = x4; // 00770034 movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0xff); // 00770037 shufps xmm6, xmm4, 0xff
    x6 = mulps(x6, SX(0x50)); // 0077003b mulps xmm6, xmmword ptr [esp + 0x50]
    x4 = SX(0x40); // 00770040 movaps xmm4, xmmword ptr [esp + 0x40]
    x4 = mulps(x4, ld(g, 0x93a1f0)); // 00770045 mulps xmm4, xmmword ptr [0x93a1f0]
    x6 = addps(x6, x4); // 0077004c addps xmm6, xmm4
    x5 = addps(x5, x6); // 0077004f addps xmm5, xmm6
    x4 = x5; // 00770052 movaps xmm4, xmm5
    x4 = shufps(x4, x5, 0x1b); // 00770055 shufps xmm4, xmm5, 0x1b
    x6 = x0; // 00770059 movaps xmm6, xmm0
    x6 = shufps(x6, x0, 0); // 0077005c shufps xmm6, xmm0, 0
    x6 = mulps(x6, x4); // 00770060 mulps xmm6, xmm4
    SXw(0x50, x6); // 00770063 movaps xmmword ptr [esp + 0x50], xmm6
    x4 = shufps(x4, x4, 0xb1); // 00770068 shufps xmm4, xmm4, 0xb1
    x6 = x0; // 0077006c movaps xmm6, xmm0
    x6 = shufps(x6, x0, 0x55); // 0077006f shufps xmm6, xmm0, 0x55
    x6 = mulps(x6, x4); // 00770073 mulps xmm6, xmm4
    x6 = mulps(x6, ld(g, 0x93a200)); // 00770076 mulps xmm6, xmmword ptr [0x93a200]
    x4 = shufps(x4, x4, 0x1b); // 0077007d shufps xmm4, xmm4, 0x1b
    x7 = x0; // 00770081 movaps xmm7, xmm0
    x7 = shufps(x7, x0, 0xaa); // 00770084 shufps xmm7, xmm0, 0xaa
    x4 = mulps(x4, x7); // 00770088 mulps xmm4, xmm7
    x4 = mulps(x4, x3); // 0077008b mulps xmm4, xmm3
    x4 = addps(x4, x6); // 0077008e addps xmm4, xmm6
    x6 = x0; // 00770091 movaps xmm6, xmm0
    x6 = shufps(x6, x0, 0xff); // 00770094 shufps xmm6, xmm0, 0xff
    x6 = mulps(x6, x5); // 00770098 mulps xmm6, xmm5
    x5 = SX(0x50); // 0077009b movaps xmm5, xmmword ptr [esp + 0x50]
    x5 = mulps(x5, ld(g, 0x93a1f0)); // 007700a0 mulps xmm5, xmmword ptr [0x93a1f0]
    x6 = addps(x6, x5); // 007700a7 addps xmm6, xmm5
    x5 = SX(0x80); // 007700aa movaps xmm5, xmmword ptr [esp + 0x80]
    x4 = addps(x4, x6); // 007700b2 addps xmm4, xmm6
    x5 = shufps(x5, x5, 0xc9); // 007700b5 shufps xmm5, xmm5, 0xc9
    x4 = shufps(x4, x4, 0xc9); // 007700b9 shufps xmm4, xmm4, 0xc9
    x7 = x5; // 007700bd movaps xmm7, xmm5
    x7 = shufps(x7, x5, 0xc9); // 007700c0 shufps xmm7, xmm5, 0xc9
    SXw(0x70, x4); // 007700c4 movaps xmmword ptr [esp + 0x70], xmm4
    x4 = SX(0x20); // 007700c9 movaps xmm4, xmmword ptr [esp + 0x20]
    x4 = shufps(x4, x4, 0xd2); // 007700ce shufps xmm4, xmm4, 0xd2
    x6 = x4; // 007700d2 movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0xd2); // 007700d5 shufps xmm6, xmm4, 0xd2
    x4 = mulps(x4, x5); // 007700d9 mulps xmm4, xmm5
    x6 = mulps(x6, x7); // 007700dc mulps xmm6, xmm7
    x4 = subps(x4, x6); // 007700df subps xmm4, xmm6
    x4 = andps(x4, x2); // 007700e2 andps xmm4, xmm2
    x4 = andps(x4, x2); // 007700e5 andps xmm4, xmm2
    x6 = x4; // 007700e8 movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0); // 007700eb shufps xmm6, xmm4, 0
    x5 = x1; // 007700ef movaps xmm5, xmm1
    x5 = shufps(x5, x1, 0x1b); // 007700f2 shufps xmm5, xmm1, 0x1b
    x6 = mulps(x6, x5); // 007700f6 mulps xmm6, xmm5
    x5 = shufps(x5, x5, 0xb1); // 007700f9 shufps xmm5, xmm5, 0xb1
    SXw(0x50, x6); // 007700fd movaps xmmword ptr [esp + 0x50], xmm6
    x6 = x5; // 00770102 movaps xmm6, xmm5
    x5 = x4; // 00770105 movaps xmm5, xmm4
    x5 = shufps(x5, x4, 0x55); // 00770108 shufps xmm5, xmm4, 0x55
    x5 = mulps(x5, x6); // 0077010c mulps xmm5, xmm6
    x5 = mulps(x5, ld(g, 0x93a200)); // 0077010f mulps xmm5, xmmword ptr [0x93a200]
    x6 = shufps(x6, x6, 0x1b); // 00770116 shufps xmm6, xmm6, 0x1b
    x7 = x4; // 0077011a movaps xmm7, xmm4
    x7 = shufps(x7, x4, 0xaa); // 0077011d shufps xmm7, xmm4, 0xaa
    x6 = mulps(x6, x7); // 00770121 mulps xmm6, xmm7
    x6 = mulps(x6, x3); // 00770124 mulps xmm6, xmm3
    x6 = addps(x6, x5); // 00770127 addps xmm6, xmm5
    x5 = x4; // 0077012a movaps xmm5, xmm4
    x5 = shufps(x5, x4, 0xff); // 0077012d shufps xmm5, xmm4, 0xff
    x4 = SX(0x50); // 00770131 movaps xmm4, xmmword ptr [esp + 0x50]
    x4 = mulps(x4, ld(g, 0x93a1f0)); // 00770136 mulps xmm4, xmmword ptr [0x93a1f0]
    x5 = mulps(x5, x1); // 0077013d mulps xmm5, xmm1
    x5 = addps(x5, x4); // 00770140 addps xmm5, xmm4
    x6 = addps(x6, x5); // 00770143 addps xmm6, xmm5
    x5 = x1; // 00770146 movaps xmm5, xmm1
    x5 = mulps(x5, ld(g, 0x93a220)); // 00770149 mulps xmm5, xmmword ptr [0x93a220]
    SXw(0x50, x6); // 00770150 movaps xmmword ptr [esp + 0x50], xmm6
    x6 = shufps(x6, x6, 0x1b); // 00770155 shufps xmm6, xmm6, 0x1b
    x4 = x5; // 00770159 movaps xmm4, xmm5
    x4 = shufps(x4, x5, 0); // 0077015c shufps xmm4, xmm5, 0
    x4 = mulps(x4, x6); // 00770160 mulps xmm4, xmm6
    x6 = shufps(x6, x6, 0xb1); // 00770163 shufps xmm6, xmm6, 0xb1
    SXw(0x40, x4); // 00770167 movaps xmmword ptr [esp + 0x40], xmm4
    x4 = x6; // 0077016c movaps xmm4, xmm6
    edi = A(0x20); // 0077016f mov edi, dword ptr [ebp + 0x20]
    x6 = x5; // 00770172 movaps xmm6, xmm5
    x6 = shufps(x6, x5, 0x55); // 00770175 shufps xmm6, xmm5, 0x55
    x6 = mulps(x6, x4); // 00770179 mulps xmm6, xmm4
    x6 = mulps(x6, ld(g, 0x93a200)); // 0077017c mulps xmm6, xmmword ptr [0x93a200]
    x4 = shufps(x4, x4, 0x1b); // 00770183 shufps xmm4, xmm4, 0x1b
    x7 = x5; // 00770187 movaps xmm7, xmm5
    x7 = shufps(x7, x5, 0xaa); // 0077018a shufps xmm7, xmm5, 0xaa
    x4 = mulps(x4, x7); // 0077018e mulps xmm4, xmm7
    x4 = mulps(x4, x3); // 00770191 mulps xmm4, xmm3
    x4 = addps(x4, x6); // 00770194 addps xmm4, xmm6
    x6 = x5; // 00770197 movaps xmm6, xmm5
    x6 = shufps(x6, x5, 0xff); // 0077019a shufps xmm6, xmm5, 0xff
    x6 = mulps(x6, SX(0x50)); // 0077019e mulps xmm6, xmmword ptr [esp + 0x50]
    x5 = SX(0x40); // 007701a3 movaps xmm5, xmmword ptr [esp + 0x40]
    x5 = mulps(x5, ld(g, 0x93a1f0)); // 007701a8 mulps xmm5, xmmword ptr [0x93a1f0]
    x6 = addps(x6, x5); // 007701af addps xmm6, xmm5
    x4 = addps(x4, x6); // 007701b2 addps xmm4, xmm6
    x5 = ld(g, edi + 0x0); // 007701b5 movaps xmm5, xmmword ptr [edi]
    x4 = mulps(x4, x5); // 007701b8 mulps xmm4, xmm5
    x4 = andps(x4, x2); // 007701bb andps xmm4, xmm2
    x6 = x4; // 007701be movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0); // 007701c1 shufps xmm6, xmm4, 0
    x5 = x1; // 007701c5 movaps xmm5, xmm1
    x5 = mulps(x5, ld(g, 0x93a220)); // 007701c8 mulps xmm5, xmmword ptr [0x93a220]
    SXw(0x50, x5); // 007701cf movaps xmmword ptr [esp + 0x50], xmm5
    x5 = shufps(x5, x5, 0x1b); // 007701d4 shufps xmm5, xmm5, 0x1b
    x6 = mulps(x6, x5); // 007701d8 mulps xmm6, xmm5
    SXw(0x40, x6); // 007701db movaps xmmword ptr [esp + 0x40], xmm6
    x6 = x4; // 007701e0 movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0x55); // 007701e3 shufps xmm6, xmm4, 0x55
    x7 = x4; // 007701e7 movaps xmm7, xmm4
    x7 = shufps(x7, x4, 0xaa); // 007701ea shufps xmm7, xmm4, 0xaa
    x5 = shufps(x5, x5, 0xb1); // 007701ee shufps xmm5, xmm5, 0xb1
    x6 = mulps(x6, x5); // 007701f2 mulps xmm6, xmm5
    x6 = mulps(x6, ld(g, 0x93a200)); // 007701f5 mulps xmm6, xmmword ptr [0x93a200]
    x5 = shufps(x5, x5, 0x1b); // 007701fc shufps xmm5, xmm5, 0x1b
    x5 = mulps(x5, x7); // 00770200 mulps xmm5, xmm7
    x5 = mulps(x5, x3); // 00770203 mulps xmm5, xmm3
    x5 = addps(x5, x6); // 00770206 addps xmm5, xmm6
    x6 = x4; // 00770209 movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0xff); // 0077020c shufps xmm6, xmm4, 0xff
    x6 = mulps(x6, SX(0x50)); // 00770210 mulps xmm6, xmmword ptr [esp + 0x50]
    x4 = SX(0x40); // 00770215 movaps xmm4, xmmword ptr [esp + 0x40]
    x4 = mulps(x4, ld(g, 0x93a1f0)); // 0077021a mulps xmm4, xmmword ptr [0x93a1f0]
    x6 = addps(x6, x4); // 00770221 addps xmm6, xmm4
    x5 = addps(x5, x6); // 00770224 addps xmm5, xmm6
    x6 = x1; // 00770227 movaps xmm6, xmm1
    x6 = shufps(x6, x1, 0); // 0077022a shufps xmm6, xmm1, 0
    x4 = x5; // 0077022e movaps xmm4, xmm5
    x4 = shufps(x4, x5, 0x1b); // 00770231 shufps xmm4, xmm5, 0x1b
    x6 = mulps(x6, x4); // 00770235 mulps xmm6, xmm4
    SXw(0x50, x6); // 00770238 movaps xmmword ptr [esp + 0x50], xmm6
    x4 = shufps(x4, x4, 0xb1); // 0077023d shufps xmm4, xmm4, 0xb1
    x6 = x1; // 00770241 movaps xmm6, xmm1
    x6 = shufps(x6, x1, 0x55); // 00770244 shufps xmm6, xmm1, 0x55
    x6 = mulps(x6, x4); // 00770248 mulps xmm6, xmm4
    x6 = mulps(x6, ld(g, 0x93a200)); // 0077024b mulps xmm6, xmmword ptr [0x93a200]
    x4 = shufps(x4, x4, 0x1b); // 00770252 shufps xmm4, xmm4, 0x1b
    x7 = x1; // 00770256 movaps xmm7, xmm1
    x7 = shufps(x7, x1, 0xaa); // 00770259 shufps xmm7, xmm1, 0xaa
    x4 = mulps(x4, x7); // 0077025d mulps xmm4, xmm7
    x4 = mulps(x4, x3); // 00770260 mulps xmm4, xmm3
    x4 = addps(x4, x6); // 00770263 addps xmm4, xmm6
    x6 = x1; // 00770266 movaps xmm6, xmm1
    x6 = shufps(x6, x1, 0xff); // 00770269 shufps xmm6, xmm1, 0xff
    x1 = SX(0x50); // 0077026d movaps xmm1, xmmword ptr [esp + 0x50]
    x1 = mulps(x1, ld(g, 0x93a1f0)); // 00770272 mulps xmm1, xmmword ptr [0x93a1f0]
    x6 = mulps(x6, x5); // 00770279 mulps xmm6, xmm5
    x6 = addps(x6, x1); // 0077027c addps xmm6, xmm1
    x1 = SX(0xa0); // 0077027f movaps xmm1, xmmword ptr [esp + 0xa0]
    x4 = addps(x4, x6); // 00770287 addps xmm4, xmm6
    x4 = shufps(x4, x4, 0xc9); // 0077028a shufps xmm4, xmm4, 0xc9
    x5 = x1; // 0077028e movaps xmm5, xmm1
    x5 = shufps(x5, x1, 0xd2); // 00770291 shufps xmm5, xmm1, 0xd2
    x6 = x4; // 00770295 movaps xmm6, xmm4
    x1 = mulps(x1, x4); // 00770298 mulps xmm1, xmm4
    edi = A(0x1c); // 0077029b mov edi, dword ptr [ebp + 0x1c]
    x6 = shufps(x6, x4, 0xc9); // 0077029e shufps xmm6, xmm4, 0xc9
    x4 = SX(0x30); // 007702a2 movaps xmm4, xmmword ptr [esp + 0x30]
    x5 = mulps(x5, x6); // 007702a7 mulps xmm5, xmm6
    x1 = subps(x1, x5); // 007702aa subps xmm1, xmm5
    x5 = SX(0x70); // 007702ad movaps xmm5, xmmword ptr [esp + 0x70]
    x6 = x4; // 007702b2 movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0xd2); // 007702b5 shufps xmm6, xmm4, 0xd2
    x4 = mulps(x4, x5); // 007702b9 mulps xmm4, xmm5
    x1 = andps(x1, x2); // 007702bc andps xmm1, xmm2
    x7 = x5; // 007702bf movaps xmm7, xmm5
    x7 = shufps(x7, x5, 0xc9); // 007702c2 shufps xmm7, xmm5, 0xc9
    x6 = mulps(x6, x7); // 007702c6 mulps xmm6, xmm7
    x4 = subps(x4, x6); // 007702c9 subps xmm4, xmm6
    x4 = andps(x4, x2); // 007702cc andps xmm4, xmm2
    x1 = addps(x1, x4); // 007702cf addps xmm1, xmm4
    x1 = mulps(x1, SX(0x20)); // 007702d2 mulps xmm1, xmmword ptr [esp + 0x20]
    x4 = x1; // 007702d7 movaps xmm4, xmm1
    x4 = shufps(x4, x1, 0x99); // 007702da shufps xmm4, xmm1, 0x99
    x1.u[0] = x86::bits(x86::add(x1.f(0), (x4).f(0))); // 007702de addss xmm1, xmm4
    x4 = shufps(x4, x4, 0x55); // 007702e2 shufps xmm4, xmm4, 0x55
    x1.u[0] = x86::bits(x86::add(x1.f(0), (x4).f(0))); // 007702e6 addss xmm1, xmm4
    x4 = x1; // 007702ea movaps xmm4, xmm1
    x4 = shufps(x4, x1, 0); // 007702ed shufps xmm4, xmm1, 0
    x1 = ld(g, edi + 0x0); // 007702f1 movaps xmm1, xmmword ptr [edi]
    edi = A(0x8); // 007702f4 mov edi, dword ptr [ebp + 8]
    x5 = ld(g, edi + 0x0); // 007702f7 movaps xmm5, xmmword ptr [edi]
    edi = A(0x38); // 007702fa mov edi, dword ptr [ebp + 0x38]
    x1 = addps(x1, x5); // 007702fd addps xmm1, xmm5
    x5 = SX(0x60); // 00770300 movaps xmm5, xmmword ptr [esp + 0x60]
    x4 = addps(x4, x1); // 00770305 addps xmm4, xmm1
    x1 = ld(g, edi + 0x0); // 00770308 movaps xmm1, xmmword ptr [edi]
    x1 = mulps(x1, x5); // 0077030b mulps xmm1, xmm5
    x1 = divps(x1, x4); // 0077030e divps xmm1, xmm4
    x4 = SX(0xb0); // 00770311 movaps xmm4, xmmword ptr [esp + 0xb0]
    x4 = divps(x4, x5); // 00770319 divps xmm4, xmm5
    x5 = SX(0x10); // 0077031c movaps xmm5, xmmword ptr [esp + 0x10]
    x1 = mulps(x1, x4); // 00770321 mulps xmm1, xmm4
    goto L_0077032d; // 00770324 jmp 0x77032d
  L_00770326:
    x1 = ld(g, 0xd61670); // 00770326 movaps xmm1, xmmword ptr [0xd61670]
  L_0077032d:
    x4 = X::splat(0); // 0077032d xorps xmm4, xmm4
    x4 = subps(x4, x1); // 00770330 subps xmm4, xmm1
    SXw(0x40, x4); // 00770333 movaps xmmword ptr [esp + 0x40], xmm4
    x4 = x1; // 00770338 movaps xmm4, xmm1
    x4 = shufps(x4, x1, 0xd2); // 0077033b shufps xmm4, xmm1, 0xd2
    x6 = x4; // 0077033f movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0xd2); // 00770342 shufps xmm6, xmm4, 0xd2
    x5 = shufps(x5, x5, 0xc9); // 00770346 shufps xmm5, xmm5, 0xc9
    x4 = mulps(x4, x5); // 0077034a mulps xmm4, xmm5
    x7 = x5; // 0077034d movaps xmm7, xmm5
    x7 = shufps(x7, x5, 0xc9); // 00770350 shufps xmm7, xmm5, 0xc9
    x6 = mulps(x6, x7); // 00770354 mulps xmm6, xmm7
    x4 = subps(x4, x6); // 00770357 subps xmm4, xmm6
    x4 = andps(x4, x2); // 0077035a andps xmm4, xmm2
    x4 = andps(x4, x2); // 0077035d andps xmm4, xmm2
    edi = A(0x8); // 00770360 mov edi, dword ptr [ebp + 8]
    x5 = x4; // 00770363 movaps xmm5, xmm4
    x5 = shufps(x5, x4, 0); // 00770366 shufps xmm5, xmm4, 0
    x2 = x0; // 0077036a movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0x1b); // 0077036d shufps xmm2, xmm0, 0x1b
    x5 = mulps(x5, x2); // 00770371 mulps xmm5, xmm2
    x6 = x4; // 00770374 movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0x55); // 00770377 shufps xmm6, xmm4, 0x55
    x7 = x4; // 0077037b movaps xmm7, xmm4
    x7 = shufps(x7, x4, 0xaa); // 0077037e shufps xmm7, xmm4, 0xaa
    x2 = shufps(x2, x2, 0xb1); // 00770382 shufps xmm2, xmm2, 0xb1
    x6 = mulps(x6, x2); // 00770386 mulps xmm6, xmm2
    x2 = shufps(x2, x2, 0x1b); // 00770389 shufps xmm2, xmm2, 0x1b
    x2 = mulps(x2, x7); // 0077038d mulps xmm2, xmm7
    x7 = ld(g, 0x93a200); // 00770390 movaps xmm7, xmmword ptr [0x93a200]
    x2 = mulps(x2, x3); // 00770397 mulps xmm2, xmm3
    x6 = mulps(x6, x7); // 0077039a mulps xmm6, xmm7
    x2 = addps(x2, x6); // 0077039d addps xmm2, xmm6
    x6 = x4; // 007703a0 movaps xmm6, xmm4
    x6 = shufps(x6, x4, 0xff); // 007703a3 shufps xmm6, xmm4, 0xff
    x6 = mulps(x6, x0); // 007703a7 mulps xmm6, xmm0
    SXw(0xb0, x6); // 007703aa movaps xmmword ptr [esp + 0xb0], xmm6
    x6 = ld(g, 0x93a1f0); // 007703b2 movaps xmm6, xmmword ptr [0x93a1f0]
    x4 = SX(0xb0); // 007703b9 movaps xmm4, xmmword ptr [esp + 0xb0]
    x5 = mulps(x5, x6); // 007703c1 mulps xmm5, xmm6
    x4 = addps(x4, x5); // 007703c4 addps xmm4, xmm5
    x2 = addps(x2, x4); // 007703c7 addps xmm2, xmm4
    x4 = ld(g, 0x93a220); // 007703ca movaps xmm4, xmmword ptr [0x93a220]
    x4 = mulps(x4, x0); // 007703d1 mulps xmm4, xmm0
    x0 = x4; // 007703d4 movaps xmm0, xmm4
    SXw(0xb0, x3); // 007703d7 movaps xmmword ptr [esp + 0xb0], xmm3
    x3 = ld(g, edi + 0x0); // 007703df movaps xmm3, xmmword ptr [edi]
    x1 = mulps(x1, x3); // 007703e2 mulps xmm1, xmm3
    x1 = addps(x1, SX(0x90)); // 007703e5 addps xmm1, xmmword ptr [esp + 0x90]
    st(g, esi + 0x0, x1); // 007703ed movaps xmmword ptr [esi], xmm1
    esi = A(0xc); // 007703f0 mov esi, dword ptr [ebp + 0xc]
    x5 = x0; // 007703f3 movaps xmm5, xmm0
    x5 = shufps(x5, x0, 0); // 007703f6 shufps xmm5, xmm0, 0
    x4 = x2; // 007703fa movaps xmm4, xmm2
    x4 = shufps(x4, x2, 0x1b); // 007703fd shufps xmm4, xmm2, 0x1b
    x3 = x0; // 00770401 movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0xaa); // 00770404 shufps xmm3, xmm0, 0xaa
    x5 = mulps(x5, x4); // 00770408 mulps xmm5, xmm4
    x4 = shufps(x4, x4, 0xb1); // 0077040b shufps xmm4, xmm4, 0xb1
    SXw(0x50, x6); // 0077040f movaps xmmword ptr [esp + 0x50], xmm6
    x6 = x0; // 00770414 movaps xmm6, xmm0
    x6 = shufps(x6, x0, 0x55); // 00770417 shufps xmm6, xmm0, 0x55
    x1 = x4; // 0077041b movaps xmm1, xmm4
    x1 = shufps(x1, x4, 0x1b); // 0077041e shufps xmm1, xmm4, 0x1b
    x1 = mulps(x1, x3); // 00770422 mulps xmm1, xmm3
    x1 = mulps(x1, SX(0xb0)); // 00770425 mulps xmm1, xmmword ptr [esp + 0xb0]
    x3 = x0; // 0077042d movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0xff); // 00770430 shufps xmm3, xmm0, 0xff
    x0 = SX(0x50); // 00770434 movaps xmm0, xmmword ptr [esp + 0x50]
    x0 = mulps(x0, x5); // 00770439 mulps xmm0, xmm5
    x6 = mulps(x6, x4); // 0077043c mulps xmm6, xmm4
    x7 = mulps(x7, x6); // 0077043f mulps xmm7, xmm6
    x1 = addps(x1, x7); // 00770442 addps xmm1, xmm7
    x3 = mulps(x3, x2); // 00770445 mulps xmm3, xmm2
    x3 = addps(x3, x0); // 00770448 addps xmm3, xmm0
    x0 = ld(g, esi + 0x0); // 0077044b movaps xmm0, xmmword ptr [esi]
    x1 = addps(x1, x3); // 0077044e addps xmm1, xmm3
    x1 = mulps(x1, x0); // 00770451 mulps xmm1, xmm0
    x0 = ld(g, eax + 0x0); // 00770454 movaps xmm0, xmmword ptr [eax]
    x1 = addps(x1, x0); // 00770457 addps xmm1, xmm0
    st(g, eax + 0x0, x1); // 0077045a movaps xmmword ptr [eax], xmm1
    eax = A(0x3c); // 0077045d mov eax, dword ptr [ebp + 0x3c]
    x0 = ld(g, eax + 0x0); // 00770460 movaps xmm0, xmmword ptr [eax]
    eax = A(0x28); // 00770463 mov eax, dword ptr [ebp + 0x28]
    x1 = ld(g, eax + 0x0); // 00770466 movaps xmm1, xmmword ptr [eax]
    x5 = SX(0x40); // 00770469 movaps xmm5, xmmword ptr [esp + 0x40]
    eax = A(0x24); // 0077046e mov eax, dword ptr [ebp + 0x24]
    x2 = ld(g, eax + 0x0); // 00770471 movaps xmm2, xmmword ptr [eax]
    x0 = subps(x0, x1); // 00770474 subps xmm0, xmm1
    x7 = ld(g, 0x93a200); // 00770477 movaps xmm7, xmmword ptr [0x93a200]
    eax = A(0x1c); // 0077047e mov eax, dword ptr [ebp + 0x1c]
    x0 = shufps(x0, x0, 0xc9); // 00770481 shufps xmm0, xmm0, 0xc9
    x1 = x0; // 00770485 movaps xmm1, xmm0
    x0 = x5; // 00770488 movaps xmm0, xmm5
    x0 = shufps(x0, x5, 0xd2); // 0077048b shufps xmm0, xmm5, 0xd2
    x4 = x1; // 0077048f movaps xmm4, xmm1
    x4 = shufps(x4, x1, 0xc9); // 00770492 shufps xmm4, xmm1, 0xc9
    x3 = x0; // 00770496 movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0xd2); // 00770499 shufps xmm3, xmm0, 0xd2
    x3 = mulps(x3, x4); // 0077049d mulps xmm3, xmm4
    x0 = mulps(x0, x1); // 007704a0 mulps xmm0, xmm1
    x1 = ld(g, 0x92db80); // 007704a3 movaps xmm1, xmmword ptr [0x92db80]
    x0 = subps(x0, x3); // 007704aa subps xmm0, xmm3
    x0 = andps(x0, x1); // 007704ad andps xmm0, xmm1
    x0 = andps(x0, x1); // 007704b0 andps xmm0, xmm1
    x1 = x2; // 007704b3 movaps xmm1, xmm2
    x1 = shufps(x1, x2, 0x1b); // 007704b6 shufps xmm1, xmm2, 0x1b
    x3 = x0; // 007704ba movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0); // 007704bd shufps xmm3, xmm0, 0
    x3 = mulps(x3, x1); // 007704c1 mulps xmm3, xmm1
    x1 = shufps(x1, x1, 0xb1); // 007704c4 shufps xmm1, xmm1, 0xb1
    x4 = x0; // 007704c8 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x55); // 007704cb shufps xmm4, xmm0, 0x55
    x4 = mulps(x4, x1); // 007704cf mulps xmm4, xmm1
    x4 = mulps(x4, x7); // 007704d2 mulps xmm4, xmm7
    x1 = shufps(x1, x1, 0x1b); // 007704d5 shufps xmm1, xmm1, 0x1b
    x6 = x0; // 007704d9 movaps xmm6, xmm0
    x6 = shufps(x6, x0, 0xaa); // 007704dc shufps xmm6, xmm0, 0xaa
    x1 = mulps(x1, x6); // 007704e0 mulps xmm1, xmm6
    x1 = mulps(x1, ld(g, 0x93a210)); // 007704e3 mulps xmm1, xmmword ptr [0x93a210]
    x6 = ld(g, 0x93a1f0); // 007704ea movaps xmm6, xmmword ptr [0x93a1f0]
    x1 = addps(x1, x4); // 007704f1 addps xmm1, xmm4
    x4 = x0; // 007704f4 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0xff); // 007704f7 shufps xmm4, xmm0, 0xff
    x0 = ld(g, 0x93a220); // 007704fb movaps xmm0, xmmword ptr [0x93a220]
    x4 = mulps(x4, x2); // 00770502 mulps xmm4, xmm2
    x2 = mulps(x2, x0); // 00770505 mulps xmm2, xmm0
    x0 = x2; // 00770508 movaps xmm0, xmm2
    x3 = mulps(x3, x6); // 0077050b mulps xmm3, xmm6
    x4 = addps(x4, x3); // 0077050e addps xmm4, xmm3
    SXw(0x50, x7); // 00770511 movaps xmmword ptr [esp + 0x50], xmm7
    x7 = ld(g, 0x93a210); // 00770516 movaps xmm7, xmmword ptr [0x93a210]
    x1 = addps(x1, x4); // 0077051d addps xmm1, xmm4
    SXw(0xb0, x7); // 00770520 movaps xmmword ptr [esp + 0xb0], xmm7
    x7 = ld(g, eax + 0x0); // 00770528 movaps xmm7, xmmword ptr [eax]
    x5 = mulps(x5, x7); // 0077052b mulps xmm5, xmm7
    x7 = ld(g, edx + 0x0); // 0077052e movaps xmm7, xmmword ptr [edx]
    x5 = addps(x5, x7); // 00770531 addps xmm5, xmm7
    st(g, edx + 0x0, x5); // 00770534 movaps xmmword ptr [edx], xmm5
    x2 = x1; // 00770537 movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0x1b); // 0077053a shufps xmm2, xmm1, 0x1b
    x3 = x0; // 0077053e movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0); // 00770541 shufps xmm3, xmm0, 0
    x3 = mulps(x3, x2); // 00770545 mulps xmm3, xmm2
    x2 = shufps(x2, x2, 0xb1); // 00770548 shufps xmm2, xmm2, 0xb1
    x5 = x2; // 0077054c movaps xmm5, xmm2
    x5 = shufps(x5, x2, 0x1b); // 0077054f shufps xmm5, xmm2, 0x1b
    x4 = x0; // 00770553 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x55); // 00770556 shufps xmm4, xmm0, 0x55
    x4 = mulps(x4, x2); // 0077055a mulps xmm4, xmm2
    x2 = x0; // 0077055d movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0xaa); // 00770560 shufps xmm2, xmm0, 0xaa
    x5 = mulps(x5, x2); // 00770564 mulps xmm5, xmm2
    x2 = SX(0x50); // 00770567 movaps xmm2, xmmword ptr [esp + 0x50]
    x5 = mulps(x5, SX(0xb0)); // 0077056c mulps xmm5, xmmword ptr [esp + 0xb0]
    x2 = mulps(x2, x4); // 00770574 mulps xmm2, xmm4
    x5 = addps(x5, x2); // 00770577 addps xmm5, xmm2
    x2 = x0; // 0077057a movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0xff); // 0077057d shufps xmm2, xmm0, 0xff
    x2 = mulps(x2, x1); // 00770581 mulps xmm2, xmm1
    x6 = mulps(x6, x3); // 00770584 mulps xmm6, xmm3
    x2 = addps(x2, x6); // 00770587 addps xmm2, xmm6
    edx = A(0x20); // 0077058a mov edx, dword ptr [ebp + 0x20]
    x0 = ld(g, edx + 0x0); // 0077058d movaps xmm0, xmmword ptr [edx]
    x5 = addps(x5, x2); // 00770590 addps xmm5, xmm2
    // 00770593 pop edi  (prologue / epilogue: skipped)
    x5 = mulps(x5, x0); // 00770594 mulps xmm5, xmm0
    x0 = ld(g, ecx + 0x0); // 00770597 movaps xmm0, xmmword ptr [ecx]
    // 0077059a pop esi  (prologue / epilogue: skipped)
    x5 = addps(x5, x0); // 0077059b addps xmm5, xmm0
    st(g, ecx + 0x0, x5); // 0077059e movaps xmmword ptr [ecx], xmm5
    // 007705a1 pop ebx  (prologue / epilogue: skipped)
    // 007705a2 mov esp, ebp  (epilogue: skipped)
    // 007705a4 pop ebp  (prologue / epilogue: skipped)
    goto L_return; // 007705a5 ret 0x3c
    WORLD_LIT_END(0x3c)
}
