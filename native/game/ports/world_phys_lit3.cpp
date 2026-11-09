// World group: literal transliterations (world_lit.h) of collision passes whose contact paths the test worlds never reach, so
// a hand port of them could not be verified there. The readable hand ports (same semantics, documented) are in
// world_phys.cpp under WORLD_HAND_DEADPATH; see native/game/notes/world.md.
#include "../port.h"
#include "world_lit.h"

#ifndef WORLD_HAND_DEADPATH
using namespace world;
namespace {
constexpr uint32_t kLitOut = reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX | reflex_port::OUT_XMM_ALL;
}

// 0076c510 DynObj_CollideWater: generic dynamic objects in water (ESI = world): see the hand port in world_phys.cpp.
REFLEX_PORT_FN(0x0076c510, DynObj_CollideWater, "world", kLitOut) {
    LIT_BEGIN
    { const uint32_t v_ = ebp; esp -= 4; g.u32(esp, v_); } // 0076c510 push ebp
    ebp = esp; // 0076c511 mov ebp, esp
    { const uint32_t r_ = esp & uint32_t(0xfffffff0); esp = r_; } // 0076c513 and esp, 0xfffffff0
    { const uint32_t r_ = esp - uint32_t(0x738); esp = r_; } // 0076c516 sub esp, 0x738
    eax = g.u32((uint32_t(0x9b45e4))); // 0076c51c mov eax, dword ptr [0x9b45e4]
    { const uint32_t r_ = eax ^ esp; eax = r_; } // 0076c521 xor eax, esp
    g.u32((esp + uint32_t(0x734)), eax); // 0076c523 mov dword ptr [esp + 0x734], eax
    x0 = ld(g, (uint32_t(0xd61670))); // 0076c52a movaps xmm0, xmmword ptr [0xd61670]
    { const uint32_t v_ = ebx; esp -= 4; g.u32(esp, v_); } // 0076c531 push ebx
    st(g, (esp + uint32_t(0x6c)), x0); // 0076c532 movaps xmmword ptr [esp + 0x6c], xmm0
    x0 = ld(g, (uint32_t(0xd61640))); // 0076c537 movaps xmm0, xmmword ptr [0xd61640]
    ebx = 0u; // 0076c53e xor ebx, ebx
    { const uint32_t v_ = edi; esp -= 4; g.u32(esp, v_); } // 0076c540 push edi
    st(g, (esp + uint32_t(0x40)), x0); // 0076c541 movaps xmmword ptr [esp + 0x40], xmm0
    eax = 0u; // 0076c546 xor eax, eax
    goto L_0076c550; // 0076c548 jmp 0x76c550
    ebx = (ebx); // 0076c54a lea ebx, [ebx]
  L_0076c550:;
    { const uint32_t r_ = ecx | uint32_t(0xffffffff); ecx = r_; } // 0076c550 or ecx, 0xffffffff
    g.u32((esp + eax * 4 + uint32_t(0x630)), ecx); // 0076c553 mov dword ptr [esp + eax*4 + 0x630], ecx
    g.u32((esp + eax * 4 + uint32_t(0x6a8)), ecx); // 0076c55a mov dword ptr [esp + eax*4 + 0x6a8], ecx
    { const uint32_t r_ = eax + 1u; eax = r_; } // 0076c561 inc eax
    { CMP_A = eax; CMP_B = uint32_t(0x1e); } // 0076c562 cmp eax, 0x1e
    if (int32_t(CMP_A) < int32_t(CMP_B)) goto L_0076c550; // 0076c565 jl 0x76c550
    ecx = g.u32((esi + uint32_t(0x1964))); // 0076c567 mov ecx, dword ptr [esi + 0x1964]
    { const uint32_t r_ = ecx - g.u32((esi + uint32_t(0x1960))); ecx = r_; } // 0076c56d sub ecx, dword ptr [esi + 0x1960]
    eax = uint32_t(0x92492493); // 0076c573 mov eax, 0x92492493
    { const int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); } // 0076c578 imul ecx
    { const uint32_t r_ = edx + ecx; edx = r_; } // 0076c57a add edx, ecx
    { const uint32_t r_ = uint32_t(int32_t(edx) >> (uint32_t(7) & 31u)); edx = r_; } // 0076c57c sar edx, 7
    eax = edx; // 0076c57f mov eax, edx
    edi = 0u; // 0076c581 xor edi, edi
    { const uint32_t r_ = eax >> (uint32_t(0x1f) & 31u); eax = r_; } // 0076c583 shr eax, 0x1f
    { const uint32_t r_ = eax + edx; eax = r_; } // 0076c586 add eax, edx
    g.u32((esp + uint32_t(0x1c)), edi); // 0076c588 mov dword ptr [esp + 0x1c], edi
    if (eax == 0) goto L_0076c85f; // 0076c58c je 0x76c85f
    g.u32((esp + uint32_t(0x14)), ebx); // 0076c592 mov dword ptr [esp + 0x14], ebx
    goto L_0076c5a2; // 0076c596 jmp 0x76c5a2
    goto L_0076c5a0; // 0076c598 jmp 0x76c5a0
    ebx = (ebx); // 0076c59a lea ebx, [ebx]
  L_0076c5a0:;
    ebx = 0u; // 0076c5a0 xor ebx, ebx
  L_0076c5a2:;
    ecx = g.u32((esi + uint32_t(0x1964))); // 0076c5a2 mov ecx, dword ptr [esi + 0x1964]
    { const uint32_t r_ = ecx - g.u32((esi + uint32_t(0x1960))); ecx = r_; } // 0076c5a8 sub ecx, dword ptr [esi + 0x1960]
    eax = uint32_t(0x92492493); // 0076c5ae mov eax, 0x92492493
    { const int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); } // 0076c5b3 imul ecx
    { const uint32_t r_ = edx + ecx; edx = r_; } // 0076c5b5 add edx, ecx
    { const uint32_t r_ = uint32_t(int32_t(edx) >> (uint32_t(7) & 31u)); edx = r_; } // 0076c5b7 sar edx, 7
    ecx = edx; // 0076c5ba mov ecx, edx
    { const uint32_t r_ = ecx >> (uint32_t(0x1f) & 31u); ecx = r_; } // 0076c5bc shr ecx, 0x1f
    { const uint32_t r_ = ecx + edx; ecx = r_; } // 0076c5bf add ecx, edx
    { CMP_A = edi; CMP_B = ecx; } // 0076c5c1 cmp edi, ecx
    if (CMP_A < CMP_B) goto L_0076c5cb; // 0076c5c3 jb 0x76c5cb
    LIT_CALL(g.u32((uint32_t(0x916224)))); // 0076c5c5 call dword ptr [0x916224]
  L_0076c5cb:;
    edi = g.u32((esi + uint32_t(0x1960))); // 0076c5cb mov edi, dword ptr [esi + 0x1960]
    { const uint32_t r_ = edi + g.u32((esp + uint32_t(0x14))); edi = r_; } // 0076c5d1 add edi, dword ptr [esp + 0x14]
    { CMP_A = uint32_t(g.u8((edi + uint32_t(0xd4)))); CMP_B = uint32_t(0); } // 0076c5d5 cmp byte ptr [edi + 0xd4], 0
    if (CMP_A == CMP_B) goto L_0076c827; // 0076c5dc je 0x76c827
    x0 = ld(g, (edi + uint32_t(0x40))); // 0076c5e2 movaps xmm0, xmmword ptr [edi + 0x40]
    st(g, (esp + uint32_t(0x30)), x0); // 0076c5e6 movaps xmmword ptr [esp + 0x30], xmm0
    x0 = ld(g, (edi + uint32_t(0x50))); // 0076c5eb movaps xmm0, xmmword ptr [edi + 0x50]
    st(g, (esp + uint32_t(0x50)), x0); // 0076c5ef movaps xmmword ptr [esp + 0x50], xmm0
    g.u32((esp + uint32_t(0x720)), ebx); // 0076c5f4 mov dword ptr [esp + 0x720], ebx
    g.u32((esp + uint32_t(0x80)), ebx); // 0076c5fb mov dword ptr [esp + 0x80], ebx
    eax = 0u; // 0076c602 xor eax, eax
  L_0076c604:;
    { const uint32_t r_ = ecx | uint32_t(0xffffffff); ecx = r_; } // 0076c604 or ecx, 0xffffffff
    g.u32((esp + eax * 4 + uint32_t(0x630)), ecx); // 0076c607 mov dword ptr [esp + eax*4 + 0x630], ecx
    g.u32((esp + eax * 4 + uint32_t(0x6a8)), ecx); // 0076c60e mov dword ptr [esp + eax*4 + 0x6a8], ecx
    { const uint32_t r_ = eax + 1u; eax = r_; } // 0076c615 inc eax
    { CMP_A = eax; CMP_B = uint32_t(0x1e); } // 0076c616 cmp eax, 0x1e
    if (int32_t(CMP_A) < int32_t(CMP_B)) goto L_0076c604; // 0076c619 jl 0x76c604
    ecx = g.u32((edi + uint32_t(0x2c))); // 0076c61b mov ecx, dword ptr [edi + 0x2c]
    { const uint32_t v_ = ebx; esp -= 4; g.u32(esp, v_); } // 0076c61e push ebx
    edx = (esp + uint32_t(0x34)); // 0076c61f lea edx, [esp + 0x34]
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 0076c623 push edx
    eax = (esp + uint32_t(0x58)); // 0076c624 lea eax, [esp + 0x58]
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 0076c628 push eax
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 0076c629 push ecx
    edx = (esp + uint32_t(0x80)); // 0076c62a lea edx, [esp + 0x80]
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 0076c631 push edx
    eax = (esp + uint32_t(0x54)); // 0076c632 lea eax, [esp + 0x54]
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 0076c636 push eax
    ecx = (esi + uint32_t(0xccb0)); // 0076c637 lea ecx, [esi + 0xccb0]
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 0076c63d push ecx
    ecx = (esp + uint32_t(0x9c)); // 0076c63e lea ecx, [esp + 0x9c]
    LIT_CALL(0x85ddd0); // 0076c645 call 0x85ddd0
    { CMP_A = g.u32((esp + uint32_t(0x720))); CMP_B = ebx; } // 0076c64a cmp dword ptr [esp + 0x720], ebx
    if (CMP_A == CMP_B) goto L_0076c827; // 0076c651 je 0x76c827
    ecx = g.u32((esp + uint32_t(0x80))); // 0076c657 mov ecx, dword ptr [esp + 0x80]
    x0 = ld(g, (edi + uint32_t(0x80))); // 0076c65e movaps xmm0, xmmword ptr [edi + 0x80]
    st(g, (esp + uint32_t(0x60)), x0); // 0076c665 movaps xmmword ptr [esp + 0x60], xmm0
    { CMP_A = ecx; CMP_B = ecx; } // 0076c66a test ecx, ecx
    if (int32_t((CMP_A & CMP_B)) <= 0) goto L_0076c696; // 0076c66c jle 0x76c696
    edx = (esp + uint32_t(0x90)); // 0076c66e lea edx, [esp + 0x90]
    g.u32((esp + uint32_t(0x18)), edx); // 0076c675 mov dword ptr [esp + 0x18], edx
  L_0076c679:;
    eax = g.u32((esp + uint32_t(0x18))); // 0076c679 mov eax, dword ptr [esp + 0x18]
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 0076c67d push eax
    ecx = esi; // 0076c67e mov ecx, esi
    LIT_CALL(0x76dec0); // 0076c680 call 0x76dec0
    ecx = g.u32((esp + uint32_t(0x80))); // 0076c685 mov ecx, dword ptr [esp + 0x80]
    { const uint32_t r_ = g.u32((esp + uint32_t(0x18))) + uint32_t(0x30); g.u32((esp + uint32_t(0x18)), r_); } // 0076c68c add dword ptr [esp + 0x18], 0x30
    { const uint32_t r_ = ebx + 1u; ebx = r_; } // 0076c691 inc ebx
    { CMP_A = ebx; CMP_B = ecx; } // 0076c692 cmp ebx, ecx
    if (int32_t(CMP_A) < int32_t(CMP_B)) goto L_0076c679; // 0076c694 jl 0x76c679
  L_0076c696:;
    x1 = ld(g, (uint32_t(0xd61670))); // 0076c696 movaps xmm1, xmmword ptr [0xd61670]
    x2 = x1; // 0076c69d movaps xmm2, xmm1
    { CMP_A = ecx; CMP_B = ecx; } // 0076c6a0 test ecx, ecx
    if (int32_t((CMP_A & CMP_B)) <= 0) goto L_0076c6c8; // 0076c6a2 jle 0x76c6c8
    eax = (esp + uint32_t(0xa0)); // 0076c6a4 lea eax, [esp + 0xa0]
    goto L_0076c6b0; // 0076c6ab jmp 0x76c6b0
    ecx = (ecx); // 0076c6ad lea ecx, [ecx]
  L_0076c6b0:;
    x0 = ld(g, (eax + uint32_t(0x10))); // 0076c6b0 movaps xmm0, xmmword ptr [eax + 0x10]
    x3 = ld(g, (eax)); // 0076c6b4 movaps xmm3, xmmword ptr [eax]
    x0 = mulps(x0, x3); // 0076c6b7 mulps xmm0, xmm3
    { const uint32_t r_ = eax + uint32_t(0x30); eax = r_; } // 0076c6ba add eax, 0x30
    { const uint32_t r_ = ecx - uint32_t(1); ecx = r_; } // 0076c6bd sub ecx, 1
    x1 = maxps(x1, x0); // 0076c6c0 maxps xmm1, xmm0
    x2 = minps(x2, x0); // 0076c6c3 minps xmm2, xmm0
    if (ecx != 0) goto L_0076c6b0; // 0076c6c6 jne 0x76c6b0
  L_0076c6c8:;
    x0 = X{{g.u32((uint32_t(0x9cc8a8))), 0, 0, 0}}; // 0076c6c8 movss xmm0, dword ptr [0x9cc8a8]
    ecx = g.u32((edi + uint32_t(0x20))); // 0076c6d0 mov ecx, dword ptr [edi + 0x20]
    x0 = shufps(x0, x0, 0); // 0076c6d3 shufps xmm0, xmm0, 0
    x2 = addps(x2, x1); // 0076c6d7 addps xmm2, xmm1
    x2 = mulps(x2, x0); // 0076c6da mulps xmm2, xmm0
    x0 = x2; // 0076c6dd movaps xmm0, xmm2
    x0 = mulps(x0, x2); // 0076c6e0 mulps xmm0, xmm2
    x1 = x0; // 0076c6e3 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x66); // 0076c6e6 shufps xmm1, xmm0, 0x66
    x0.u[0] = x86::bits(x86::add(x0.f(0), x1.f(0))); // 0076c6ea addss xmm0, xmm1
    x1 = shufps(x1, x1, 0x55); // 0076c6ee shufps xmm1, xmm1, 0x55
    x0.u[0] = x86::bits(x86::add(x0.f(0), x1.f(0))); // 0076c6f2 addss xmm0, xmm1
    x0 = shufps(x0, x0, 0); // 0076c6f6 shufps xmm0, xmm0, 0
    x1 = sqrtps(x0); // 0076c6fa sqrtps xmm1, xmm0
    x0 = ld(g, (ecx + uint32_t(0xc0))); // 0076c6fd movaps xmm0, xmmword ptr [ecx + 0xc0]
    x3 = x0; // 0076c704 movaps xmm3, xmm0
    x3 = cmpltps(x3, x1); // 0076c707 cmpltps xmm3, xmm1
    st(g, (esp + uint32_t(0x20)), x3); // 0076c70b movaps xmmword ptr [esp + 0x20], xmm3
    edx = g.u32((esp + uint32_t(0x24))); // 0076c710 mov edx, dword ptr [esp + 0x24]
    eax = g.u32((esp + uint32_t(0x28))); // 0076c714 mov eax, dword ptr [esp + 0x28]
    { const uint32_t r_ = eax + edx; eax = r_; } // 0076c718 add eax, edx
    { const uint32_t r_ = eax + g.u32((esp + uint32_t(0x20))); eax = r_; } // 0076c71a add eax, dword ptr [esp + 0x20]
    { CMP_A = eax; CMP_B = uint32_t(-3); } // 0076c71e cmp eax, -3
    if (CMP_A != CMP_B) goto L_0076c72c; // 0076c721 jne 0x76c72c
    x0 = divps(x0, x1); // 0076c723 divps xmm0, xmm1
    x0 = mulps(x0, x2); // 0076c726 mulps xmm0, xmm2
    x2 = x0; // 0076c729 movaps xmm2, xmm0
  L_0076c72c:;
    x0 = ld(g, (edi + uint32_t(0xa0))); // 0076c72c movaps xmm0, xmmword ptr [edi + 0xa0]
    x2 = addps(x2, x0); // 0076c733 addps xmm2, xmm0
    st(g, (edi + uint32_t(0xa0)), x2); // 0076c736 movaps xmmword ptr [edi + 0xa0], xmm2
    x0 = ld(g, (edi + uint32_t(0x80))); // 0076c73d movaps xmm0, xmmword ptr [edi + 0x80]
    x0 = subps(x0, ld(g, (esp + uint32_t(0x60)))); // 0076c744 subps xmm0, xmmword ptr [esp + 0x60]
    eax = g.u32((esi + uint32_t(0x1900))); // 0076c749 mov eax, dword ptr [esi + 0x1900]
    x0 = mulps(x0, x0); // 0076c74f mulps xmm0, xmm0
    x1 = x0; // 0076c752 movaps xmm1, xmm0
    x0 = shufps(x0, x1, 0x66); // 0076c755 shufps xmm0, xmm1, 0x66
    x1.u[0] = x86::bits(x86::add(x1.f(0), x0.f(0))); // 0076c759 addss xmm1, xmm0
    x0 = shufps(x0, x0, 0x55); // 0076c75d shufps xmm0, xmm0, 0x55
    x1.u[0] = x86::bits(x86::add(x1.f(0), x0.f(0))); // 0076c761 addss xmm1, xmm0
    x0 = x1; // 0076c765 movaps xmm0, xmm1
    x0 = shufps(x0, x0, 0); // 0076c768 shufps xmm0, xmm0, 0
    x0 = sqrtps(x0); // 0076c76c sqrtps xmm0, xmm0
    { CMP_A = eax; CMP_B = uint32_t(0x64); } // 0076c76f cmp eax, 0x64
    if (CMP_A >= CMP_B) goto L_0076c827; // 0076c772 jae 0x76c827
    ecx = (eax + uint32_t(1)); // 0076c778 lea ecx, [eax + 1]
    g.u32((esi + uint32_t(0x1900)), ecx); // 0076c77b mov dword ptr [esi + 0x1900], ecx
    edx = g.u32((edi + uint32_t(0x34))); // 0076c781 mov edx, dword ptr [edi + 0x34]
    { const uint32_t r_ = eax << (uint32_t(6) & 31u); eax = r_; } // 0076c784 shl eax, 6
    { const uint32_t r_ = eax + esi; eax = r_; } // 0076c787 add eax, esi
    g.u32((eax), edx); // 0076c789 mov dword ptr [eax], edx
    edx = g.u32((esi + uint32_t(0xccd0))); // 0076c78b mov edx, dword ptr [esi + 0xccd0]
    ecx = 0u; // 0076c791 xor ecx, ecx
    { CMP_A = g.u32((edx + uint32_t(8))); CMP_B = ecx; } // 0076c793 cmp dword ptr [edx + 8], ecx
    if (CMP_A <= CMP_B) goto L_0076c7c6; // 0076c796 jbe 0x76c7c6
    edi = g.u32((uint32_t(0xa95844))); // 0076c798 mov edi, dword ptr [0xa95844]
  L_0076c79e:;
    { CMP_A = g.u32((edx + ecx * 4)); CMP_B = uint32_t(0); } // 0076c79e cmp dword ptr [edx + ecx*4], 0
    if (CMP_A == CMP_B) goto L_0076c7c0; // 0076c7a2 je 0x76c7c0
    ebx = g.u32((edi + uint32_t(4))); // 0076c7a4 mov ebx, dword ptr [edi + 4]
    ebx = g.u32((edi + ebx * 4 + uint32_t(0x1c))); // 0076c7a7 mov ebx, dword ptr [edi + ebx*4 + 0x1c]
    edi = g.u32((edx + ecx * 4)); // 0076c7ab mov edi, dword ptr [edx + ecx*4]
    { CMP_A = ebx; CMP_B = g.u32((edi + uint32_t(0x44a8))); } // 0076c7ae cmp ebx, dword ptr [edi + 0x44a8]
    if (CMP_A == CMP_B) goto L_0076c873; // 0076c7b4 je 0x76c873
    edi = g.u32((uint32_t(0xa95844))); // 0076c7ba mov edi, dword ptr [0xa95844]
  L_0076c7c0:;
    { const uint32_t r_ = ecx + 1u; ecx = r_; } // 0076c7c0 inc ecx
    { CMP_A = ecx; CMP_B = g.u32((edx + uint32_t(8))); } // 0076c7c1 cmp ecx, dword ptr [edx + 8]
    if (CMP_A < CMP_B) goto L_0076c79e; // 0076c7c4 jb 0x76c79e
  L_0076c7c6:;
    ecx = 0u; // 0076c7c6 xor ecx, ecx
  L_0076c7c8:;
    ecx = g.u32((ecx + uint32_t(0xe4))); // 0076c7c8 mov ecx, dword ptr [ecx + 0xe4]
    g.u32((eax + uint32_t(4)), ecx); // 0076c7ce mov dword ptr [eax + 4], ecx
    g.u32((eax + uint32_t(0x2c)), uint32_t(0)); // 0076c7d1 mov dword ptr [eax + 0x2c], 0
    g.u8((eax + uint32_t(0x20)), uint8_t(uint32_t(0))); // 0076c7d8 mov byte ptr [eax + 0x20], 0
    edx = g.u32((esp + uint32_t(0x630))); // 0076c7dc mov edx, dword ptr [esp + 0x630]
    g.u32((eax + uint32_t(0x30)), edx); // 0076c7e3 mov dword ptr [eax + 0x30], edx
    ecx = g.u32((esp + uint32_t(0x6a8))); // 0076c7e6 mov ecx, dword ptr [esp + 0x6a8]
    g.u32((eax + uint32_t(0x34)), ecx); // 0076c7ed mov dword ptr [eax + 0x34], ecx
    x1 = X{{g.u32((esp + uint32_t(0x90))), 0, 0, 0}}; // 0076c7f0 movss xmm1, dword ptr [esp + 0x90]
    g.u32((eax + uint32_t(0x10)), x1.u[0]); // 0076c7f9 movss dword ptr [eax + 0x10], xmm1
    x1 = X{{g.u32((esp + uint32_t(0x94))), 0, 0, 0}}; // 0076c7fe movss xmm1, dword ptr [esp + 0x94]
    g.u32((eax + uint32_t(0x14)), x1.u[0]); // 0076c807 movss dword ptr [eax + 0x14], xmm1
    x1 = X{{g.u32((esp + uint32_t(0x98))), 0, 0, 0}}; // 0076c80c movss xmm1, dword ptr [esp + 0x98]
    g.u32((eax + uint32_t(0x18)), x1.u[0]); // 0076c815 movss dword ptr [eax + 0x18], xmm1
    g.u32((eax + uint32_t(0x24)), x0.u[0]); // 0076c81a movss dword ptr [eax + 0x24], xmm0
    x0 = X::splat(0); // 0076c81f xorps xmm0, xmm0
    g.u32((eax + uint32_t(0x28)), x0.u[0]); // 0076c822 movss dword ptr [eax + 0x28], xmm0
  L_0076c827:;
    ecx = g.u32((esi + uint32_t(0x1964))); // 0076c827 mov ecx, dword ptr [esi + 0x1964]
    { const uint32_t r_ = ecx - g.u32((esi + uint32_t(0x1960))); ecx = r_; } // 0076c82d sub ecx, dword ptr [esi + 0x1960]
    edi = g.u32((esp + uint32_t(0x1c))); // 0076c833 mov edi, dword ptr [esp + 0x1c]
    { const uint32_t r_ = g.u32((esp + uint32_t(0x14))) + uint32_t(0xe0); g.u32((esp + uint32_t(0x14)), r_); } // 0076c837 add dword ptr [esp + 0x14], 0xe0
    eax = uint32_t(0x92492493); // 0076c83f mov eax, 0x92492493
    { const int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); } // 0076c844 imul ecx
    { const uint32_t r_ = edx + ecx; edx = r_; } // 0076c846 add edx, ecx
    { const uint32_t r_ = uint32_t(int32_t(edx) >> (uint32_t(7) & 31u)); edx = r_; } // 0076c848 sar edx, 7
    eax = edx; // 0076c84b mov eax, edx
    { const uint32_t r_ = eax >> (uint32_t(0x1f) & 31u); eax = r_; } // 0076c84d shr eax, 0x1f
    { const uint32_t r_ = edi + 1u; edi = r_; } // 0076c850 inc edi
    { const uint32_t r_ = eax + edx; eax = r_; } // 0076c851 add eax, edx
    g.u32((esp + uint32_t(0x1c)), edi); // 0076c853 mov dword ptr [esp + 0x1c], edi
    { CMP_A = edi; CMP_B = eax; } // 0076c857 cmp edi, eax
    if (CMP_A < CMP_B) goto L_0076c5a0; // 0076c859 jb 0x76c5a0
  L_0076c85f:;
    ecx = g.u32((esp + uint32_t(0x73c))); // 0076c85f mov ecx, dword ptr [esp + 0x73c]
    { const uint32_t v_ = g.u32(esp); esp += 4; edi = v_; } // 0076c866 pop edi
    { const uint32_t v_ = g.u32(esp); esp += 4; ebx = v_; } // 0076c867 pop ebx
    { const uint32_t r_ = ecx ^ esp; ecx = r_; } // 0076c868 xor ecx, esp
    LIT_CALL(0x8f9efe); // 0076c86a call 0x8f9efe
    esp = ebp; // 0076c86f mov esp, ebp
    { const uint32_t v_ = g.u32(esp); esp += 4; ebp = v_; } // 0076c871 pop ebp
    goto L_return; // 0076c872 ret 
  L_0076c873:;
    ecx = edi; // 0076c873 mov ecx, edi
    goto L_0076c7c8; // 0076c875 jmp 0x76c7c8

    LIT_END(0x0)
}

// 0076d9a0 Kinematic_CollideTerrain: active kinematic bodies against the terrain (ESI = world): see the hand port in world_phys.cpp.
REFLEX_PORT_FN(0x0076d9a0, Kinematic_CollideTerrain, "world", kLitOut) {
    LIT_BEGIN
    { const uint32_t v_ = ebp; esp -= 4; g.u32(esp, v_); } // 0076d9a0 push ebp
    ebp = esp; // 0076d9a1 mov ebp, esp
    { const uint32_t r_ = esp & uint32_t(0xfffffff0); esp = r_; } // 0076d9a3 and esp, 0xfffffff0
    { const uint32_t r_ = esp - uint32_t(0xdb8); esp = r_; } // 0076d9a6 sub esp, 0xdb8
    eax = g.u32((uint32_t(0x9b45e4))); // 0076d9ac mov eax, dword ptr [0x9b45e4]
    { const uint32_t r_ = eax ^ esp; eax = r_; } // 0076d9b1 xor eax, esp
    g.u32((esp + uint32_t(0xdb4)), eax); // 0076d9b3 mov dword ptr [esp + 0xdb4], eax
    { const uint32_t v_ = ebx; esp -= 4; g.u32(esp, v_); } // 0076d9ba push ebx
    ebx = 0u; // 0076d9bb xor ebx, ebx
    { const uint32_t v_ = edi; esp -= 4; g.u32(esp, v_); } // 0076d9bd push edi
    { CMP_A = g.u32((esi + uint32_t(0xcc9c))); CMP_B = ebx; } // 0076d9be cmp dword ptr [esi + 0xcc9c], ebx
    if (CMP_A == CMP_B) goto L_0076dd3c; // 0076d9c4 je 0x76dd3c
    eax = 0u; // 0076d9ca xor eax, eax
    esp = (esp); // 0076d9cc lea esp, [esp]
  L_0076d9d0:;
    { const uint32_t r_ = ecx | uint32_t(0xffffffff); ecx = r_; } // 0076d9d0 or ecx, 0xffffffff
    g.u32((esp + eax * 4 + uint32_t(0x600)), ecx); // 0076d9d3 mov dword ptr [esp + eax*4 + 0x600], ecx
    g.u32((esp + eax * 4 + uint32_t(0x678)), ecx); // 0076d9da mov dword ptr [esp + eax*4 + 0x678], ecx
    { const uint32_t r_ = eax + 1u; eax = r_; } // 0076d9e1 inc eax
    { CMP_A = eax; CMP_B = uint32_t(0x1e); } // 0076d9e2 cmp eax, 0x1e
    if (int32_t(CMP_A) < int32_t(CMP_B)) goto L_0076d9d0; // 0076d9e5 jl 0x76d9d0
    eax = 0u; // 0076d9e7 xor eax, eax
    esp = (esp); // 0076d9e9 lea esp, [esp]
  L_0076d9f0:;
    { const uint32_t r_ = ecx | uint32_t(0xffffffff); ecx = r_; } // 0076d9f0 or ecx, 0xffffffff
    g.u32((esp + eax * 4 + uint32_t(0xcb0)), ecx); // 0076d9f3 mov dword ptr [esp + eax*4 + 0xcb0], ecx
    g.u32((esp + eax * 4 + uint32_t(0xd28)), ecx); // 0076d9fa mov dword ptr [esp + eax*4 + 0xd28], ecx
    { const uint32_t r_ = eax + 1u; eax = r_; } // 0076da01 inc eax
    { CMP_A = eax; CMP_B = uint32_t(0x1e); } // 0076da02 cmp eax, 0x1e
    if (int32_t(CMP_A) < int32_t(CMP_B)) goto L_0076d9f0; // 0076da05 jl 0x76d9f0
    { CMP_A = g.u32((esi + uint32_t(0x19d4))); CMP_B = ebx; } // 0076da07 cmp dword ptr [esi + 0x19d4], ebx
    g.u32((esp + uint32_t(0x24)), ebx); // 0076da0d mov dword ptr [esp + 0x24], ebx
    if (int32_t(CMP_A) <= int32_t(CMP_B)) goto L_0076dd3c; // 0076da11 jle 0x76dd3c
    eax = (esi + uint32_t(0x1d10)); // 0076da17 lea eax, [esi + 0x1d10]
    ecx = (esi + uint32_t(0x19d8)); // 0076da1d lea ecx, [esi + 0x19d8]
    g.u32((esp + uint32_t(0x10)), eax); // 0076da23 mov dword ptr [esp + 0x10], eax
    g.u32((esp + uint32_t(0x20)), ecx); // 0076da27 mov dword ptr [esp + 0x20], ecx
    goto L_0076da30; // 0076da2b jmp 0x76da30
    ecx = (ecx); // 0076da2d lea ecx, [ecx]
  L_0076da30:;
    edx = g.u32((esp + uint32_t(0x20))); // 0076da30 mov edx, dword ptr [esp + 0x20]
    edi = g.u32((edx)); // 0076da34 mov edi, dword ptr [edx]
    { CMP_A = uint32_t(g.u8((edi + uint32_t(0x84)))); CMP_B = uint32_t(1); } // 0076da36 test byte ptr [edi + 0x84], 1
    if ((CMP_A & CMP_B) == 0) goto L_0076dd1d; // 0076da3d je 0x76dd1d
    g.u32((esp + uint32_t(0x6f0)), ebx); // 0076da43 mov dword ptr [esp + 0x6f0], ebx
    g.u32((esp + uint32_t(0x50)), ebx); // 0076da4a mov dword ptr [esp + 0x50], ebx
    eax = 0u; // 0076da4e xor eax, eax
  L_0076da50:;
    { const uint32_t r_ = ecx | uint32_t(0xffffffff); ecx = r_; } // 0076da50 or ecx, 0xffffffff
    g.u32((esp + eax * 4 + uint32_t(0x600)), ecx); // 0076da53 mov dword ptr [esp + eax*4 + 0x600], ecx
    g.u32((esp + eax * 4 + uint32_t(0x678)), ecx); // 0076da5a mov dword ptr [esp + eax*4 + 0x678], ecx
    { const uint32_t r_ = eax + 1u; eax = r_; } // 0076da61 inc eax
    { CMP_A = eax; CMP_B = uint32_t(0x1e); } // 0076da62 cmp eax, 0x1e
    if (int32_t(CMP_A) < int32_t(CMP_B)) goto L_0076da50; // 0076da65 jl 0x76da50
    g.u32((esp + uint32_t(0xda0)), ebx); // 0076da67 mov dword ptr [esp + 0xda0], ebx
    g.u32((esp + uint32_t(0x700)), ebx); // 0076da6e mov dword ptr [esp + 0x700], ebx
    eax = 0u; // 0076da75 xor eax, eax
    goto L_0076da80; // 0076da77 jmp 0x76da80
    esp = (esp); // 0076da79 lea esp, [esp]
  L_0076da80:;
    { const uint32_t r_ = ecx | uint32_t(0xffffffff); ecx = r_; } // 0076da80 or ecx, 0xffffffff
    g.u32((esp + eax * 4 + uint32_t(0xcb0)), ecx); // 0076da83 mov dword ptr [esp + eax*4 + 0xcb0], ecx
    g.u32((esp + eax * 4 + uint32_t(0xd28)), ecx); // 0076da8a mov dword ptr [esp + eax*4 + 0xd28], ecx
    { const uint32_t r_ = eax + 1u; eax = r_; } // 0076da91 inc eax
    { CMP_A = eax; CMP_B = uint32_t(0x1e); } // 0076da92 cmp eax, 0x1e
    if (int32_t(CMP_A) < int32_t(CMP_B)) goto L_0076da80; // 0076da95 jl 0x76da80
    ecx = g.u32((edi + uint32_t(0x2c))); // 0076da97 mov ecx, dword ptr [edi + 0x2c]
    eax = g.u32((ecx + uint32_t(0x14))); // 0076da9a mov eax, dword ptr [ecx + 0x14]
    { const uint32_t r_ = eax >> (uint32_t(5) & 31u); eax = r_; } // 0076da9d shr eax, 5
    g.u32((esp + uint32_t(0x28)), ebx); // 0076daa0 mov dword ptr [esp + 0x28], ebx
    g.u32((esp + uint32_t(0xc)), ebx); // 0076daa4 mov dword ptr [esp + 0xc], ebx
    { CMP_A = ((eax >> 0) & 0xffu); CMP_B = uint32_t(1); } // 0076daa8 test al, 1
    if ((CMP_A & CMP_B) != 0) goto L_0076dadc; // 0076daaa jne 0x76dadc
    edx = (esp + uint32_t(0xc)); // 0076daac lea edx, [esp + 0xc]
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 0076dab0 push edx
    eax = (esp + uint32_t(0x704)); // 0076dab1 lea eax, [esp + 0x704]
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 0076dab8 push eax
    edx = (esp + uint32_t(0x30)); // 0076dab9 lea edx, [esp + 0x30]
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 0076dabd push edx
    eax = (edi + uint32_t(0x50)); // 0076dabe lea eax, [edi + 0x50]
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 0076dac1 push eax
    eax = g.u32((esi + uint32_t(0xcc9c))); // 0076dac2 mov eax, dword ptr [esi + 0xcc9c]
    edx = (edi + uint32_t(0x40)); // 0076dac8 lea edx, [edi + 0x40]
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 0076dacb push edx
    edx = g.u32((eax + uint32_t(0x31ec))); // 0076dacc mov edx, dword ptr [eax + 0x31ec]
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 0076dad2 push edx
    edx = (esp + uint32_t(0x68)); // 0076dad3 lea edx, [esp + 0x68]
    LIT_CALL(0x667e40); // 0076dad7 call 0x667e40
  L_0076dadc:;
    { CMP_A = g.u32((esp + uint32_t(0xda0))); CMP_B = ebx; } // 0076dadc cmp dword ptr [esp + 0xda0], ebx
    if (CMP_A == CMP_B) goto L_0076db93; // 0076dae3 je 0x76db93
    eax = g.u32((esp + uint32_t(0x10))); // 0076dae9 mov eax, dword ptr [esp + 0x10]
    x0 = X::splat(0); // 0076daed xorps xmm0, xmm0
    x1 = ld(g, (eax)); // 0076daf0 movaps xmm1, xmmword ptr [eax]
    eax = g.u32((esi + uint32_t(0x1900))); // 0076daf3 mov eax, dword ptr [esi + 0x1900]
    g.u32((esp + uint32_t(0x2c)), x0.u[0]); // 0076daf9 movss dword ptr [esp + 0x2c], xmm0
    x1 = mulps(x1, x1); // 0076daff mulps xmm1, xmm1
    x0 = x1; // 0076db02 movaps xmm0, xmm1
    x0 = shufps(x0, x1, 0x66); // 0076db05 shufps xmm0, xmm1, 0x66
    x1.u[0] = x86::bits(x86::add(x1.f(0), x0.f(0))); // 0076db09 addss xmm1, xmm0
    x0 = shufps(x0, x0, 0x55); // 0076db0d shufps xmm0, xmm0, 0x55
    x1.u[0] = x86::bits(x86::add(x1.f(0), x0.f(0))); // 0076db11 addss xmm1, xmm0
    x0 = x1; // 0076db15 movaps xmm0, xmm1
    x0 = shufps(x0, x0, 0); // 0076db18 shufps xmm0, xmm0, 0
    x0 = sqrtps(x0); // 0076db1c sqrtps xmm0, xmm0
    { CMP_A = eax; CMP_B = uint32_t(0x64); } // 0076db1f cmp eax, 0x64
    if (CMP_A >= CMP_B) goto L_0076db93; // 0076db22 jae 0x76db93
    ecx = (eax + uint32_t(1)); // 0076db24 lea ecx, [eax + 1]
    g.u32((esi + uint32_t(0x1900)), ecx); // 0076db27 mov dword ptr [esi + 0x1900], ecx
    edx = g.u32((edi + uint32_t(0x34))); // 0076db2d mov edx, dword ptr [edi + 0x34]
    { const uint32_t r_ = eax << (uint32_t(6) & 31u); eax = r_; } // 0076db30 shl eax, 6
    { const uint32_t r_ = eax + esi; eax = r_; } // 0076db33 add eax, esi
    g.u32((eax), edx); // 0076db35 mov dword ptr [eax], edx
    ecx = g.u32((esi + uint32_t(0xcc9c))); // 0076db37 mov ecx, dword ptr [esi + 0xcc9c]
    edx = g.u32((esp + uint32_t(0xc))); // 0076db3d mov edx, dword ptr [esp + 0xc]
    g.u32((eax + uint32_t(4)), ecx); // 0076db41 mov dword ptr [eax + 4], ecx
    g.u32((eax + uint32_t(0x2c)), edx); // 0076db44 mov dword ptr [eax + 0x2c], edx
    g.u8((eax + uint32_t(0x20)), uint8_t(uint32_t(1))); // 0076db47 mov byte ptr [eax + 0x20], 1
    ecx = g.u32((esp + uint32_t(0xcb0))); // 0076db4b mov ecx, dword ptr [esp + 0xcb0]
    g.u32((eax + uint32_t(0x30)), ecx); // 0076db52 mov dword ptr [eax + 0x30], ecx
    edx = g.u32((esp + uint32_t(0xd28))); // 0076db55 mov edx, dword ptr [esp + 0xd28]
    g.u32((eax + uint32_t(0x34)), edx); // 0076db5c mov dword ptr [eax + 0x34], edx
    x1 = X{{g.u32((esp + uint32_t(0x710))), 0, 0, 0}}; // 0076db5f movss xmm1, dword ptr [esp + 0x710]
    g.u32((eax + uint32_t(0x10)), x1.u[0]); // 0076db68 movss dword ptr [eax + 0x10], xmm1
    x1 = X{{g.u32((esp + uint32_t(0x714))), 0, 0, 0}}; // 0076db6d movss xmm1, dword ptr [esp + 0x714]
    g.u32((eax + uint32_t(0x14)), x1.u[0]); // 0076db76 movss dword ptr [eax + 0x14], xmm1
    x1 = X{{g.u32((esp + uint32_t(0x718))), 0, 0, 0}}; // 0076db7b movss xmm1, dword ptr [esp + 0x718]
    g.u32((eax + uint32_t(0x18)), x1.u[0]); // 0076db84 movss dword ptr [eax + 0x18], xmm1
    g.u32((eax + uint32_t(0x24)), x0.u[0]); // 0076db89 movss dword ptr [eax + 0x24], xmm0
    g.u32((eax + uint32_t(0x28)), x0.u[0]); // 0076db8e movss dword ptr [eax + 0x28], xmm0
  L_0076db93:;
    { CMP_A = g.u32((esp + uint32_t(0x6f0))); CMP_B = ebx; } // 0076db93 cmp dword ptr [esp + 0x6f0], ebx
    if (CMP_A == CMP_B) goto L_0076dd1d; // 0076db9a je 0x76dd1d
    eax = g.u32((esp + uint32_t(0x10))); // 0076dba0 mov eax, dword ptr [esp + 0x10]
    ecx = g.u32((esp + uint32_t(0x50))); // 0076dba4 mov ecx, dword ptr [esp + 0x50]
    x0 = ld(g, (eax)); // 0076dba8 movaps xmm0, xmmword ptr [eax]
    st(g, (esp + uint32_t(0x40)), x0); // 0076dbab movaps xmmword ptr [esp + 0x40], xmm0
    { CMP_A = ecx; CMP_B = ecx; } // 0076dbb0 test ecx, ecx
    if (int32_t((CMP_A & CMP_B)) <= 0) goto L_0076dbd7; // 0076dbb2 jle 0x76dbd7
    ecx = (esp + uint32_t(0x60)); // 0076dbb4 lea ecx, [esp + 0x60]
    g.u32((esp + uint32_t(0xc)), ecx); // 0076dbb8 mov dword ptr [esp + 0xc], ecx
  L_0076dbbc:;
    edx = g.u32((esp + uint32_t(0xc))); // 0076dbbc mov edx, dword ptr [esp + 0xc]
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 0076dbc0 push edx
    { const uint32_t v_ = esi; esp -= 4; g.u32(esp, v_); } // 0076dbc1 push esi
    ecx = edi; // 0076dbc2 mov ecx, edi
    LIT_CALL(0x76f2f0); // 0076dbc4 call 0x76f2f0
    ecx = g.u32((esp + uint32_t(0x50))); // 0076dbc9 mov ecx, dword ptr [esp + 0x50]
    { const uint32_t r_ = g.u32((esp + uint32_t(0xc))) + uint32_t(0x30); g.u32((esp + uint32_t(0xc)), r_); } // 0076dbcd add dword ptr [esp + 0xc], 0x30
    { const uint32_t r_ = ebx + 1u; ebx = r_; } // 0076dbd2 inc ebx
    { CMP_A = ebx; CMP_B = ecx; } // 0076dbd3 cmp ebx, ecx
    if (int32_t(CMP_A) < int32_t(CMP_B)) goto L_0076dbbc; // 0076dbd5 jl 0x76dbbc
  L_0076dbd7:;
    x3 = X{{g.u32((uint32_t(0x9cc8a8))), 0, 0, 0}}; // 0076dbd7 movss xmm3, dword ptr [0x9cc8a8]
    x0 = ld(g, (uint32_t(0xd61670))); // 0076dbdf movaps xmm0, xmmword ptr [0xd61670]
    x3 = shufps(x3, x3, 0); // 0076dbe6 shufps xmm3, xmm3, 0
    x2 = x0; // 0076dbea movaps xmm2, xmm0
    { CMP_A = ecx; CMP_B = ecx; } // 0076dbed test ecx, ecx
    if (int32_t((CMP_A & CMP_B)) <= 0) goto L_0076dc0d; // 0076dbef jle 0x76dc0d
    eax = (esp + uint32_t(0x70)); // 0076dbf1 lea eax, [esp + 0x70]
  L_0076dbf5:;
    x1 = ld(g, (eax + uint32_t(0x10))); // 0076dbf5 movaps xmm1, xmmword ptr [eax + 0x10]
    x4 = ld(g, (eax)); // 0076dbf9 movaps xmm4, xmmword ptr [eax]
    x1 = mulps(x1, x4); // 0076dbfc mulps xmm1, xmm4
    { const uint32_t r_ = eax + uint32_t(0x30); eax = r_; } // 0076dbff add eax, 0x30
    { const uint32_t r_ = ecx - uint32_t(1); ecx = r_; } // 0076dc02 sub ecx, 1
    x2 = maxps(x2, x1); // 0076dc05 maxps xmm2, xmm1
    x0 = minps(x0, x1); // 0076dc08 minps xmm0, xmm1
    if (ecx != 0) goto L_0076dbf5; // 0076dc0b jne 0x76dbf5
  L_0076dc0d:;
    eax = g.u32((edi + uint32_t(0x20))); // 0076dc0d mov eax, dword ptr [edi + 0x20]
    x0 = addps(x0, x2); // 0076dc10 addps xmm0, xmm2
    x1 = X::splat(0); // 0076dc13 xorps xmm1, xmm1
    x1 = subps(x1, x3); // 0076dc16 subps xmm1, xmm3
    x0 = mulps(x0, x1); // 0076dc19 mulps xmm0, xmm1
    x2 = x0; // 0076dc1c movaps xmm2, xmm0
    x0 = mulps(x0, x2); // 0076dc1f mulps xmm0, xmm2
    x1 = x0; // 0076dc22 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x66); // 0076dc25 shufps xmm1, xmm0, 0x66
    x0.u[0] = x86::bits(x86::add(x0.f(0), x1.f(0))); // 0076dc29 addss xmm0, xmm1
    x1 = shufps(x1, x1, 0x55); // 0076dc2d shufps xmm1, xmm1, 0x55
    x0.u[0] = x86::bits(x86::add(x0.f(0), x1.f(0))); // 0076dc31 addss xmm0, xmm1
    x0 = shufps(x0, x0, 0); // 0076dc35 shufps xmm0, xmm0, 0
    x1 = sqrtps(x0); // 0076dc39 sqrtps xmm1, xmm0
    x0 = ld(g, (eax + uint32_t(0xc0))); // 0076dc3c movaps xmm0, xmmword ptr [eax + 0xc0]
    x3 = x0; // 0076dc43 movaps xmm3, xmm0
    x3 = cmpltps(x3, x1); // 0076dc46 cmpltps xmm3, xmm1
    st(g, (esp + uint32_t(0x30)), x3); // 0076dc4a movaps xmmword ptr [esp + 0x30], xmm3
    ecx = g.u32((esp + uint32_t(0x34))); // 0076dc4f mov ecx, dword ptr [esp + 0x34]
    edx = g.u32((esp + uint32_t(0x38))); // 0076dc53 mov edx, dword ptr [esp + 0x38]
    { const uint32_t r_ = edx + ecx; edx = r_; } // 0076dc57 add edx, ecx
    { const uint32_t r_ = edx + g.u32((esp + uint32_t(0x30))); edx = r_; } // 0076dc59 add edx, dword ptr [esp + 0x30]
    { CMP_A = edx; CMP_B = uint32_t(-3); } // 0076dc5d cmp edx, -3
    if (CMP_A != CMP_B) goto L_0076dc6b; // 0076dc60 jne 0x76dc6b
    x0 = divps(x0, x1); // 0076dc62 divps xmm0, xmm1
    x0 = mulps(x0, x2); // 0076dc65 mulps xmm0, xmm2
    x2 = x0; // 0076dc68 movaps xmm2, xmm0
  L_0076dc6b:;
    eax = g.u32((esp + uint32_t(0x10))); // 0076dc6b mov eax, dword ptr [esp + 0x10]
    x0 = ld(g, (eax - uint32_t(0x10))); // 0076dc6f movaps xmm0, xmmword ptr [eax - 0x10]
    x0 = addps(x0, x2); // 0076dc73 addps xmm0, xmm2
    st(g, (eax - uint32_t(0x10)), x0); // 0076dc76 movaps xmmword ptr [eax - 0x10], xmm0
    x0 = ld(g, (eax - uint32_t(0x10))); // 0076dc7a movaps xmm0, xmmword ptr [eax - 0x10]
    st(g, (edi + uint32_t(0x40)), x0); // 0076dc7e movaps xmmword ptr [edi + 0x40], xmm0
    x0 = ld(g, (eax)); // 0076dc82 movaps xmm0, xmmword ptr [eax]
    x0 = subps(x0, ld(g, (esp + uint32_t(0x40)))); // 0076dc85 subps xmm0, xmmword ptr [esp + 0x40]
    eax = g.u32((esi + uint32_t(0x1900))); // 0076dc8a mov eax, dword ptr [esi + 0x1900]
    x0 = mulps(x0, x0); // 0076dc90 mulps xmm0, xmm0
    x1 = x0; // 0076dc93 movaps xmm1, xmm0
    x0 = shufps(x0, x1, 0x66); // 0076dc96 shufps xmm0, xmm1, 0x66
    x1.u[0] = x86::bits(x86::add(x1.f(0), x0.f(0))); // 0076dc9a addss xmm1, xmm0
    x0 = shufps(x0, x0, 0x55); // 0076dc9e shufps xmm0, xmm0, 0x55
    x1.u[0] = x86::bits(x86::add(x1.f(0), x0.f(0))); // 0076dca2 addss xmm1, xmm0
    x0 = x1; // 0076dca6 movaps xmm0, xmm1
    x0 = shufps(x0, x0, 0); // 0076dca9 shufps xmm0, xmm0, 0
    x0 = sqrtps(x0); // 0076dcad sqrtps xmm0, xmm0
    { CMP_A = eax; CMP_B = uint32_t(0x64); } // 0076dcb0 cmp eax, 0x64
    if (CMP_A >= CMP_B) goto L_0076dd1b; // 0076dcb3 jae 0x76dd1b
    ecx = (eax + uint32_t(1)); // 0076dcb5 lea ecx, [eax + 1]
    g.u32((esi + uint32_t(0x1900)), ecx); // 0076dcb8 mov dword ptr [esi + 0x1900], ecx
    edx = g.u32((edi + uint32_t(0x34))); // 0076dcbe mov edx, dword ptr [edi + 0x34]
    { const uint32_t r_ = eax << (uint32_t(6) & 31u); eax = r_; } // 0076dcc1 shl eax, 6
    { const uint32_t r_ = eax + esi; eax = r_; } // 0076dcc4 add eax, esi
    g.u32((eax), edx); // 0076dcc6 mov dword ptr [eax], edx
    ecx = g.u32((esi + uint32_t(0xcc9c))); // 0076dcc8 mov ecx, dword ptr [esi + 0xcc9c]
    edx = g.u32((esp + uint32_t(0x28))); // 0076dcce mov edx, dword ptr [esp + 0x28]
    g.u32((eax + uint32_t(4)), ecx); // 0076dcd2 mov dword ptr [eax + 4], ecx
    g.u32((eax + uint32_t(0x2c)), edx); // 0076dcd5 mov dword ptr [eax + 0x2c], edx
    g.u8((eax + uint32_t(0x20)), uint8_t(uint32_t(0))); // 0076dcd8 mov byte ptr [eax + 0x20], 0
    ecx = g.u32((esp + uint32_t(0x600))); // 0076dcdc mov ecx, dword ptr [esp + 0x600]
    g.u32((eax + uint32_t(0x30)), ecx); // 0076dce3 mov dword ptr [eax + 0x30], ecx
    edx = g.u32((esp + uint32_t(0x678))); // 0076dce6 mov edx, dword ptr [esp + 0x678]
    g.u32((eax + uint32_t(0x34)), edx); // 0076dced mov dword ptr [eax + 0x34], edx
    x1 = X{{g.u32((esp + uint32_t(0x60))), 0, 0, 0}}; // 0076dcf0 movss xmm1, dword ptr [esp + 0x60]
    g.u32((eax + uint32_t(0x10)), x1.u[0]); // 0076dcf6 movss dword ptr [eax + 0x10], xmm1
    x1 = X{{g.u32((esp + uint32_t(0x64))), 0, 0, 0}}; // 0076dcfb movss xmm1, dword ptr [esp + 0x64]
    g.u32((eax + uint32_t(0x14)), x1.u[0]); // 0076dd01 movss dword ptr [eax + 0x14], xmm1
    x1 = X{{g.u32((esp + uint32_t(0x68))), 0, 0, 0}}; // 0076dd06 movss xmm1, dword ptr [esp + 0x68]
    g.u32((eax + uint32_t(0x18)), x1.u[0]); // 0076dd0c movss dword ptr [eax + 0x18], xmm1
    g.u32((eax + uint32_t(0x24)), x0.u[0]); // 0076dd11 movss dword ptr [eax + 0x24], xmm0
    g.u32((eax + uint32_t(0x28)), x0.u[0]); // 0076dd16 movss dword ptr [eax + 0x28], xmm0
  L_0076dd1b:;
    ebx = 0u; // 0076dd1b xor ebx, ebx
  L_0076dd1d:;
    eax = g.u32((esp + uint32_t(0x24))); // 0076dd1d mov eax, dword ptr [esp + 0x24]
    { const uint32_t r_ = g.u32((esp + uint32_t(0x20))) + uint32_t(4); g.u32((esp + uint32_t(0x20)), r_); } // 0076dd21 add dword ptr [esp + 0x20], 4
    { const uint32_t r_ = g.u32((esp + uint32_t(0x10))) + uint32_t(0x50); g.u32((esp + uint32_t(0x10)), r_); } // 0076dd26 add dword ptr [esp + 0x10], 0x50
    { const uint32_t r_ = eax + 1u; eax = r_; } // 0076dd2b inc eax
    { CMP_A = eax; CMP_B = g.u32((esi + uint32_t(0x19d4))); } // 0076dd2c cmp eax, dword ptr [esi + 0x19d4]
    g.u32((esp + uint32_t(0x24)), eax); // 0076dd32 mov dword ptr [esp + 0x24], eax
    if (int32_t(CMP_A) < int32_t(CMP_B)) goto L_0076da30; // 0076dd36 jl 0x76da30
  L_0076dd3c:;
    ecx = g.u32((esp + uint32_t(0xdbc))); // 0076dd3c mov ecx, dword ptr [esp + 0xdbc]
    { const uint32_t v_ = g.u32(esp); esp += 4; edi = v_; } // 0076dd43 pop edi
    { const uint32_t v_ = g.u32(esp); esp += 4; ebx = v_; } // 0076dd44 pop ebx
    { const uint32_t r_ = ecx ^ esp; ecx = r_; } // 0076dd45 xor ecx, esp
    LIT_CALL(0x8f9efe); // 0076dd47 call 0x8f9efe
    esp = ebp; // 0076dd4c mov esp, ebp
    { const uint32_t v_ = g.u32(esp); esp += 4; ebp = v_; } // 0076dd4e pop ebp
    goto L_return; // 0076dd4f ret 

    LIT_END(0x0)
}

// 0076c880 Rider_CollideKeyed: rider bodies against the keyed objects (EDI = world): see the hand port in world_phys.cpp.
REFLEX_PORT_FN(0x0076c880, Rider_CollideKeyed, "world", kLitOut) {
    LIT_BEGIN
    { const uint32_t v_ = ebp; esp -= 4; g.u32(esp, v_); } // 0076c880 push ebp
    ebp = esp; // 0076c881 mov ebp, esp
    { const uint32_t r_ = esp & uint32_t(0xfffffff0); esp = r_; } // 0076c883 and esp, 0xfffffff0
    { const uint32_t r_ = esp - uint32_t(0x778); esp = r_; } // 0076c886 sub esp, 0x778
    eax = g.u32((uint32_t(0x9b45e4))); // 0076c88c mov eax, dword ptr [0x9b45e4]
    { const uint32_t r_ = eax ^ esp; eax = r_; } // 0076c891 xor eax, esp
    g.u32((esp + uint32_t(0x774)), eax); // 0076c893 mov dword ptr [esp + 0x774], eax
    x0 = X{{g.u32((uint32_t(0x9322cc))), 0, 0, 0}}; // 0076c89a movss xmm0, dword ptr [0x9322cc]
    { const uint32_t v_ = ebx; esp -= 4; g.u32(esp, v_); } // 0076c8a2 push ebx
    x0 = shufps(x0, x0, 0); // 0076c8a3 shufps xmm0, xmm0, 0
    edx = 0u; // 0076c8a7 xor edx, edx
    { const uint32_t v_ = esi; esp -= 4; g.u32(esp, v_); } // 0076c8a9 push esi
    st(g, (esp + uint32_t(0x20)), x0); // 0076c8aa movaps xmmword ptr [esp + 0x20], xmm0
    eax = 0u; // 0076c8af xor eax, eax
  L_0076c8b1:;
    { const uint32_t r_ = ecx | uint32_t(0xffffffff); ecx = r_; } // 0076c8b1 or ecx, 0xffffffff
    g.u32((esp + eax * 4 + uint32_t(0x670)), ecx); // 0076c8b4 mov dword ptr [esp + eax*4 + 0x670], ecx
    g.u32((esp + eax * 4 + uint32_t(0x6e8)), ecx); // 0076c8bb mov dword ptr [esp + eax*4 + 0x6e8], ecx
    { const uint32_t r_ = eax + 1u; eax = r_; } // 0076c8c2 inc eax
    { CMP_A = eax; CMP_B = uint32_t(0x1e); } // 0076c8c3 cmp eax, 0x1e
    if (int32_t(CMP_A) < int32_t(CMP_B)) goto L_0076c8b1; // 0076c8c6 jl 0x76c8b1
    eax = g.u32((edi + uint32_t(0x1984))); // 0076c8c8 mov eax, dword ptr [edi + 0x1984]
    { const uint32_t r_ = eax - g.u32((edi + uint32_t(0x1980))); eax = r_; } // 0076c8ce sub eax, dword ptr [edi + 0x1980]
    g.u32((esp + uint32_t(0x10)), edx); // 0076c8d4 mov dword ptr [esp + 0x10], edx
    { const uint32_t r_ = uint32_t(int32_t(eax) >> (uint32_t(7) & 31u)); eax = r_; } // 0076c8d8 sar eax, 7
    g.u32((esp + uint32_t(0x1c)), eax); // 0076c8db mov dword ptr [esp + 0x1c], eax
    { CMP_A = eax; CMP_B = edx; } // 0076c8df cmp eax, edx
    if (CMP_A <= CMP_B) goto L_0076ccea; // 0076c8e1 jbe 0x76ccea
    g.u32((esp + uint32_t(0xc)), edx); // 0076c8e7 mov dword ptr [esp + 0xc], edx
  L_0076c8eb:;
    eax = g.u32((edi + uint32_t(0x1984))); // 0076c8eb mov eax, dword ptr [edi + 0x1984]
    { const uint32_t r_ = eax - g.u32((edi + uint32_t(0x1980))); eax = r_; } // 0076c8f1 sub eax, dword ptr [edi + 0x1980]
    { const uint32_t r_ = uint32_t(int32_t(eax) >> (uint32_t(7) & 31u)); eax = r_; } // 0076c8f7 sar eax, 7
    { CMP_A = g.u32((esp + uint32_t(0x10))); CMP_B = eax; } // 0076c8fa cmp dword ptr [esp + 0x10], eax
    if (CMP_A < CMP_B) goto L_0076c906; // 0076c8fe jb 0x76c906
    LIT_CALL(g.u32((uint32_t(0x916224)))); // 0076c900 call dword ptr [0x916224]
  L_0076c906:;
    eax = g.u32((edi + uint32_t(0x1980))); // 0076c906 mov eax, dword ptr [edi + 0x1980]
    { const uint32_t r_ = eax + g.u32((esp + uint32_t(0xc))); eax = r_; } // 0076c90c add eax, dword ptr [esp + 0xc]
    x0 = ld(g, (eax + uint32_t(0x40))); // 0076c910 movaps xmm0, xmmword ptr [eax + 0x40]
    st(g, (esp + uint32_t(0x60)), x0); // 0076c914 movaps xmmword ptr [esp + 0x60], xmm0
    x0 = ld(g, (eax + uint32_t(0x50))); // 0076c919 movaps xmm0, xmmword ptr [eax + 0x50]
    g.u32((esp + uint32_t(0x14)), eax); // 0076c91d mov dword ptr [esp + 0x14], eax
    st(g, (esp + uint32_t(0x40)), x0); // 0076c921 movaps xmmword ptr [esp + 0x40], xmm0
    ecx = g.u32((eax + uint32_t(0x2c))); // 0076c926 mov ecx, dword ptr [eax + 0x2c]
    edx = g.u32((ecx)); // 0076c929 mov edx, dword ptr [ecx]
    edx = g.u32((edx + uint32_t(0xc))); // 0076c92b mov edx, dword ptr [edx + 0xc]
    eax = (esp + uint32_t(0xa0)); // 0076c92e lea eax, [esp + 0xa0]
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 0076c935 push eax
    eax = (esp + uint32_t(0x84)); // 0076c936 lea eax, [esp + 0x84]
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 0076c93d push eax
    eax = (esp + uint32_t(0x68)); // 0076c93e lea eax, [esp + 0x68]
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 0076c942 push eax
    eax = (esp + uint32_t(0x4c)); // 0076c943 lea eax, [esp + 0x4c]
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 0076c947 push eax
    LIT_CALL(edx); // 0076c948 call edx
    ebx = g.u32((edi + uint32_t(0x19ac))); // 0076c94a mov ebx, dword ptr [edi + 0x19ac]
    esi = g.u32((ebx + uint32_t(0x30))); // 0076c950 mov esi, dword ptr [ebx + 0x30]
    eax = (esp + uint32_t(0xa0)); // 0076c953 lea eax, [esp + 0xa0]
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 0076c95a push eax
    edx = (esp + uint32_t(0x84)); // 0076c95b lea edx, [esp + 0x84]
    LIT_CALL(0x883020); // 0076c962 call 0x883020
    g.u32((ebx), uint32_t(0)); // 0076c967 mov dword ptr [ebx], 0
    eax = g.u32((edi + uint32_t(0x19ac))); // 0076c96d mov eax, dword ptr [edi + 0x19ac]
    ecx = g.u32((eax + uint32_t(0x30))); // 0076c973 mov ecx, dword ptr [eax + 0x30]
    edx = g.u32((eax)); // 0076c976 mov edx, dword ptr [eax]
    { CMP_A = edx; CMP_B = g.u32((ecx + uint32_t(0x400))); } // 0076c978 cmp edx, dword ptr [ecx + 0x400]
    if (int32_t(CMP_A) >= int32_t(CMP_B)) goto L_0076c989; // 0076c97e jge 0x76c989
    { const uint32_t r_ = edx + 1u; edx = r_; } // 0076c980 inc edx
    g.u32((eax), edx); // 0076c981 mov dword ptr [eax], edx
    ecx = g.u32((ecx + edx * 4 - uint32_t(4))); // 0076c983 mov ecx, dword ptr [ecx + edx*4 - 4]
    goto L_0076c98c; // 0076c987 jmp 0x76c98c
  L_0076c989:;
    { const uint32_t r_ = ecx | uint32_t(0xffffffff); ecx = r_; } // 0076c989 or ecx, 0xffffffff
  L_0076c98c:;
    esi = ecx; // 0076c98c mov esi, ecx
    { CMP_A = ecx; CMP_B = uint32_t(-1); } // 0076c98e cmp ecx, -1
    if (CMP_A == CMP_B) goto L_0076ccd2; // 0076c991 je 0x76ccd2
    goto L_0076c9a0; // 0076c997 jmp 0x76c9a0
    esp = (esp); // 0076c999 lea esp, [esp]
  L_0076c9a0:;
    ecx = g.u32((edi + uint32_t(0x19a4))); // 0076c9a0 mov ecx, dword ptr [edi + 0x19a4]
    { const uint32_t r_ = ecx - g.u32((edi + uint32_t(0x19a0))); ecx = r_; } // 0076c9a6 sub ecx, dword ptr [edi + 0x19a0]
    eax = uint32_t(0x92492493); // 0076c9ac mov eax, 0x92492493
    { const int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); } // 0076c9b1 imul ecx
    { const uint32_t r_ = edx + ecx; edx = r_; } // 0076c9b3 add edx, ecx
    { const uint32_t r_ = uint32_t(int32_t(edx) >> (uint32_t(7) & 31u)); edx = r_; } // 0076c9b5 sar edx, 7
    ecx = edx; // 0076c9b8 mov ecx, edx
    { const uint32_t r_ = ecx >> (uint32_t(0x1f) & 31u); ecx = r_; } // 0076c9ba shr ecx, 0x1f
    { const uint32_t r_ = ecx + edx; ecx = r_; } // 0076c9bd add ecx, edx
    { CMP_A = esi; CMP_B = ecx; } // 0076c9bf cmp esi, ecx
    if (CMP_A < CMP_B) goto L_0076c9c9; // 0076c9c1 jb 0x76c9c9
    LIT_CALL(g.u32((uint32_t(0x916224)))); // 0076c9c3 call dword ptr [0x916224]
  L_0076c9c9:;
    esi = uint32_t(int32_t(esi) * int32_t(uint32_t(0xe0))); // 0076c9c9 imul esi, esi, 0xe0
    { const uint32_t r_ = esi + g.u32((edi + uint32_t(0x19a0))); esi = r_; } // 0076c9cf add esi, dword ptr [edi + 0x19a0]
    { CMP_A = uint32_t(g.u8((esi + uint32_t(0xb0)))); CMP_B = uint32_t(0); } // 0076c9d5 cmp byte ptr [esi + 0xb0], 0
    if (CMP_A == CMP_B) goto L_0076cca8; // 0076c9dc je 0x76cca8
    x0 = ld(g, (esi + uint32_t(0x40))); // 0076c9e2 movaps xmm0, xmmword ptr [esi + 0x40]
    ebx = 0u; // 0076c9e6 xor ebx, ebx
    st(g, (esp + uint32_t(0xb0)), x0); // 0076c9e8 movaps xmmword ptr [esp + 0xb0], xmm0
    x0 = ld(g, (esi + uint32_t(0x50))); // 0076c9f0 movaps xmm0, xmmword ptr [esi + 0x50]
    st(g, (esp + uint32_t(0x90)), x0); // 0076c9f4 movaps xmmword ptr [esp + 0x90], xmm0
    g.u32((esp + uint32_t(0x760)), ebx); // 0076c9fc mov dword ptr [esp + 0x760], ebx
    g.u32((esp + uint32_t(0xc0)), ebx); // 0076ca03 mov dword ptr [esp + 0xc0], ebx
    eax = 0u; // 0076ca0a xor eax, eax
    esp = (esp); // 0076ca0c lea esp, [esp]
  L_0076ca10:;
    { const uint32_t r_ = ecx | uint32_t(0xffffffff); ecx = r_; } // 0076ca10 or ecx, 0xffffffff
    g.u32((esp + eax * 4 + uint32_t(0x670)), ecx); // 0076ca13 mov dword ptr [esp + eax*4 + 0x670], ecx
    g.u32((esp + eax * 4 + uint32_t(0x6e8)), ecx); // 0076ca1a mov dword ptr [esp + eax*4 + 0x6e8], ecx
    { const uint32_t r_ = eax + 1u; eax = r_; } // 0076ca21 inc eax
    { CMP_A = eax; CMP_B = uint32_t(0x1e); } // 0076ca22 cmp eax, 0x1e
    if (int32_t(CMP_A) < int32_t(CMP_B)) goto L_0076ca10; // 0076ca25 jl 0x76ca10
    ecx = g.u32((esi + uint32_t(0x2c))); // 0076ca27 mov ecx, dword ptr [esi + 0x2c]
    { const uint32_t v_ = ebx; esp -= 4; g.u32(esp, v_); } // 0076ca2a push ebx
    edx = (esp + uint32_t(0xb4)); // 0076ca2b lea edx, [esp + 0xb4]
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 0076ca32 push edx
    eax = (esp + uint32_t(0x98)); // 0076ca33 lea eax, [esp + 0x98]
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 0076ca3a push eax
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 0076ca3b push ecx
    ecx = g.u32((esp + uint32_t(0x24))); // 0076ca3c mov ecx, dword ptr [esp + 0x24]
    edx = (esp + uint32_t(0x70)); // 0076ca40 lea edx, [esp + 0x70]
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 0076ca44 push edx
    edx = g.u32((ecx + uint32_t(0x2c))); // 0076ca45 mov edx, dword ptr [ecx + 0x2c]
    eax = (esp + uint32_t(0x54)); // 0076ca48 lea eax, [esp + 0x54]
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 0076ca4c push eax
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 0076ca4d push edx
    ecx = (esp + uint32_t(0xdc)); // 0076ca4e lea ecx, [esp + 0xdc]
    LIT_CALL(0x85ddd0); // 0076ca55 call 0x85ddd0
    { CMP_A = g.u32((esp + uint32_t(0x760))); CMP_B = ebx; } // 0076ca5a cmp dword ptr [esp + 0x760], ebx
    if (CMP_A == CMP_B) goto L_0076cca8; // 0076ca61 je 0x76cca8
    { CMP_A = uint32_t(g.u8((esi + uint32_t(0x84)))); CMP_B = uint32_t(0x20); } // 0076ca67 test byte ptr [esi + 0x84], 0x20
    x0 = ld(g, (uint32_t(0xd61670))); // 0076ca6e movaps xmm0, xmmword ptr [0xd61670]
    st(g, (esp + uint32_t(0x50)), x0); // 0076ca75 movaps xmmword ptr [esp + 0x50], xmm0
    if ((CMP_A & CMP_B) == 0) goto L_0076ca94; // 0076ca7a je 0x76ca94
    eax = g.u32((esi + uint32_t(0x80))); // 0076ca7c mov eax, dword ptr [esi + 0x80]
    { const uint32_t r_ = eax + uint32_t(0x5d); eax = r_; } // 0076ca82 add eax, 0x5d
    eax = (eax + eax * 4); // 0076ca85 lea eax, [eax + eax*4]
    { const uint32_t r_ = eax << (uint32_t(4) & 31u); eax = r_; } // 0076ca88 shl eax, 4
    x1 = ld(g, (eax + edi)); // 0076ca8b movaps xmm1, xmmword ptr [eax + edi]
    st(g, (esp + uint32_t(0x50)), x1); // 0076ca8f movaps xmmword ptr [esp + 0x50], xmm1
  L_0076ca94:;
    ecx = g.u32((esp + uint32_t(0xc0))); // 0076ca94 mov ecx, dword ptr [esp + 0xc0]
    { CMP_A = ecx; CMP_B = ecx; } // 0076ca9b test ecx, ecx
    if (int32_t((CMP_A & CMP_B)) <= 0) goto L_0076cad3; // 0076ca9d jle 0x76cad3
    ecx = (esp + uint32_t(0xd0)); // 0076ca9f lea ecx, [esp + 0xd0]
    g.u32((esp + uint32_t(0x18)), ecx); // 0076caa6 mov dword ptr [esp + 0x18], ecx
  L_0076caaa:;
    edx = g.u32((esp + uint32_t(0x14))); // 0076caaa mov edx, dword ptr [esp + 0x14]
    ecx = g.u32((esp + uint32_t(0x18))); // 0076caae mov ecx, dword ptr [esp + 0x18]
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 0076cab2 push edx
    { const uint32_t v_ = edi; esp -= 4; g.u32(esp, v_); } // 0076cab3 push edi
    edx = esi; // 0076cab4 mov edx, esi
    LIT_CALL(0x76e6c0); // 0076cab6 call 0x76e6c0
    ecx = g.u32((esp + uint32_t(0xc0))); // 0076cabb mov ecx, dword ptr [esp + 0xc0]
    { const uint32_t r_ = g.u32((esp + uint32_t(0x18))) + uint32_t(0x30); g.u32((esp + uint32_t(0x18)), r_); } // 0076cac2 add dword ptr [esp + 0x18], 0x30
    { const uint32_t r_ = ebx + 1u; ebx = r_; } // 0076cac7 inc ebx
    { CMP_A = ebx; CMP_B = ecx; } // 0076cac8 cmp ebx, ecx
    if (int32_t(CMP_A) < int32_t(CMP_B)) goto L_0076caaa; // 0076caca jl 0x76caaa
    x0 = ld(g, (uint32_t(0xd61670))); // 0076cacc movaps xmm0, xmmword ptr [0xd61670]
  L_0076cad3:;
    { CMP_A = uint32_t(g.u8((esi + uint32_t(0x84)))); CMP_B = uint32_t(0x20); } // 0076cad3 test byte ptr [esi + 0x84], 0x20
    if ((CMP_A & CMP_B) == 0) goto L_0076cbe6; // 0076cada je 0x76cbe6
    x3 = X{{g.u32((uint32_t(0x9cc8a8))), 0, 0, 0}}; // 0076cae0 movss xmm3, dword ptr [0x9cc8a8]
    x3 = shufps(x3, x3, 0); // 0076cae8 shufps xmm3, xmm3, 0
    x2 = x0; // 0076caec movaps xmm2, xmm0
    { CMP_A = ecx; CMP_B = ecx; } // 0076caef test ecx, ecx
    if (int32_t((CMP_A & CMP_B)) <= 0) goto L_0076cb18; // 0076caf1 jle 0x76cb18
    eax = (esp + uint32_t(0xe0)); // 0076caf3 lea eax, [esp + 0xe0]
    ebx = (ebx); // 0076cafa lea ebx, [ebx]
  L_0076cb00:;
    x1 = ld(g, (eax + uint32_t(0x10))); // 0076cb00 movaps xmm1, xmmword ptr [eax + 0x10]
    x4 = ld(g, (eax)); // 0076cb04 movaps xmm4, xmmword ptr [eax]
    x1 = mulps(x1, x4); // 0076cb07 mulps xmm1, xmm4
    { const uint32_t r_ = eax + uint32_t(0x30); eax = r_; } // 0076cb0a add eax, 0x30
    { const uint32_t r_ = ecx - uint32_t(1); ecx = r_; } // 0076cb0d sub ecx, 1
    x2 = maxps(x2, x1); // 0076cb10 maxps xmm2, xmm1
    x0 = minps(x0, x1); // 0076cb13 minps xmm0, xmm1
    if (ecx != 0) goto L_0076cb00; // 0076cb16 jne 0x76cb00
  L_0076cb18:;
    { CMP_A = g.u32((uint32_t(0xda67d0))); CMP_B = uint32_t(1); } // 0076cb18 cmp dword ptr [0xda67d0], 1
    x0 = addps(x0, x2); // 0076cb1f addps xmm0, xmm2
    x0 = mulps(x0, x3); // 0076cb22 mulps xmm0, xmm3
    x0 = mulps(x0, ld(g, (esp + uint32_t(0x20)))); // 0076cb25 mulps xmm0, xmmword ptr [esp + 0x20]
    x2 = x0; // 0076cb2a movaps xmm2, xmm0
    st(g, (esp + uint32_t(0x70)), x2); // 0076cb2d movaps xmmword ptr [esp + 0x70], xmm2
    if (CMP_A != CMP_B) goto L_0076cb54; // 0076cb32 jne 0x76cb54
    ecx = g.u32((esi + uint32_t(0x80))); // 0076cb34 mov ecx, dword ptr [esi + 0x80]
    { const uint32_t r_ = ecx + uint32_t(0x4d0); ecx = r_; } // 0076cb3a add ecx, 0x4d0
    eax = (esp + uint32_t(0x70)); // 0076cb40 lea eax, [esp + 0x70]
    { const uint32_t r_ = ecx << (uint32_t(5) & 31u); ecx = r_; } // 0076cb44 shl ecx, 5
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 0076cb47 push eax
    { const uint32_t r_ = ecx + edi; ecx = r_; } // 0076cb48 add ecx, edi
    LIT_CALL(0x84fe00); // 0076cb4a call 0x84fe00
    x2 = ld(g, (esp + uint32_t(0x70))); // 0076cb4f movaps xmm2, xmmword ptr [esp + 0x70]
  L_0076cb54:;
    ecx = g.u32((esi + uint32_t(0x20))); // 0076cb54 mov ecx, dword ptr [esi + 0x20]
    x0 = x2; // 0076cb57 movaps xmm0, xmm2
    x0 = mulps(x0, x2); // 0076cb5a mulps xmm0, xmm2
    x1 = x0; // 0076cb5d movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x66); // 0076cb60 shufps xmm1, xmm0, 0x66
    x0.u[0] = x86::bits(x86::add(x0.f(0), x1.f(0))); // 0076cb64 addss xmm0, xmm1
    x1 = shufps(x1, x1, 0x55); // 0076cb68 shufps xmm1, xmm1, 0x55
    x0.u[0] = x86::bits(x86::add(x0.f(0), x1.f(0))); // 0076cb6c addss xmm0, xmm1
    x0 = shufps(x0, x0, 0); // 0076cb70 shufps xmm0, xmm0, 0
    x1 = sqrtps(x0); // 0076cb74 sqrtps xmm1, xmm0
    x0 = ld(g, (ecx + uint32_t(0xc0))); // 0076cb77 movaps xmm0, xmmword ptr [ecx + 0xc0]
    x3 = x0; // 0076cb7e movaps xmm3, xmm0
    x3 = cmpltps(x3, x1); // 0076cb81 cmpltps xmm3, xmm1
    st(g, (esp + uint32_t(0x30)), x3); // 0076cb85 movaps xmmword ptr [esp + 0x30], xmm3
    edx = g.u32((esp + uint32_t(0x34))); // 0076cb8a mov edx, dword ptr [esp + 0x34]
    eax = g.u32((esp + uint32_t(0x38))); // 0076cb8e mov eax, dword ptr [esp + 0x38]
    { const uint32_t r_ = eax + edx; eax = r_; } // 0076cb92 add eax, edx
    { const uint32_t r_ = eax + g.u32((esp + uint32_t(0x30))); eax = r_; } // 0076cb94 add eax, dword ptr [esp + 0x30]
    { CMP_A = eax; CMP_B = uint32_t(-3); } // 0076cb98 cmp eax, -3
    if (CMP_A != CMP_B) goto L_0076cba6; // 0076cb9b jne 0x76cba6
    x0 = divps(x0, x1); // 0076cb9d divps xmm0, xmm1
    x0 = mulps(x0, x2); // 0076cba0 mulps xmm0, xmm2
    x2 = x0; // 0076cba3 movaps xmm2, xmm0
  L_0076cba6:;
    eax = g.u32((esi + uint32_t(0x80))); // 0076cba6 mov eax, dword ptr [esi + 0x80]
    ecx = (eax + eax * 4); // 0076cbac lea ecx, [eax + eax*4]
    { const uint32_t r_ = ecx << (uint32_t(4) & 31u); ecx = r_; } // 0076cbaf shl ecx, 4
    x0 = ld(g, (ecx + edi + uint32_t(0x1d00))); // 0076cbb2 movaps xmm0, xmmword ptr [ecx + edi + 0x1d00]
    eax = (ecx + edi + uint32_t(0x1d00)); // 0076cbba lea eax, [ecx + edi + 0x1d00]
    x0 = addps(x0, x2); // 0076cbc1 addps xmm0, xmm2
    st(g, (eax), x0); // 0076cbc4 movaps xmmword ptr [eax], xmm0
    eax = g.u32((esi + uint32_t(0x80))); // 0076cbc7 mov eax, dword ptr [esi + 0x80]
    edx = (eax + eax * 4); // 0076cbcd lea edx, [eax + eax*4]
    { const uint32_t r_ = edx << (uint32_t(4) & 31u); edx = r_; } // 0076cbd0 shl edx, 4
    x0 = ld(g, (edx + edi + uint32_t(0x1d00))); // 0076cbd3 movaps xmm0, xmmword ptr [edx + edi + 0x1d00]
    st(g, (esi + uint32_t(0x40)), x0); // 0076cbdb movaps xmmword ptr [esi + 0x40], xmm0
    x0 = ld(g, (uint32_t(0xd61670))); // 0076cbdf movaps xmm0, xmmword ptr [0xd61670]
  L_0076cbe6:;
    { CMP_A = uint32_t(g.u8((esi + uint32_t(0x84)))); CMP_B = uint32_t(0x20); } // 0076cbe6 test byte ptr [esi + 0x84], 0x20
    x1 = x0; // 0076cbed movaps xmm1, xmm0
    if ((CMP_A & CMP_B) == 0) goto L_0076cc05; // 0076cbf0 je 0x76cc05
    eax = g.u32((esi + uint32_t(0x80))); // 0076cbf2 mov eax, dword ptr [esi + 0x80]
    { const uint32_t r_ = eax + uint32_t(0x5d); eax = r_; } // 0076cbf8 add eax, 0x5d
    eax = (eax + eax * 4); // 0076cbfb lea eax, [eax + eax*4]
    { const uint32_t r_ = eax << (uint32_t(4) & 31u); eax = r_; } // 0076cbfe shl eax, 4
    x1 = ld(g, (eax + edi)); // 0076cc01 movaps xmm1, xmmword ptr [eax + edi]
  L_0076cc05:;
    x1 = subps(x1, ld(g, (esp + uint32_t(0x50)))); // 0076cc05 subps xmm1, xmmword ptr [esp + 0x50]
    eax = g.u32((edi + uint32_t(0x1900))); // 0076cc0a mov eax, dword ptr [edi + 0x1900]
    x1 = mulps(x1, x1); // 0076cc10 mulps xmm1, xmm1
    x0 = x1; // 0076cc13 movaps xmm0, xmm1
    x0 = shufps(x0, x1, 0x66); // 0076cc16 shufps xmm0, xmm1, 0x66
    x1.u[0] = x86::bits(x86::add(x1.f(0), x0.f(0))); // 0076cc1a addss xmm1, xmm0
    x0 = shufps(x0, x0, 0x55); // 0076cc1e shufps xmm0, xmm0, 0x55
    x1.u[0] = x86::bits(x86::add(x1.f(0), x0.f(0))); // 0076cc22 addss xmm1, xmm0
    x0 = x1; // 0076cc26 movaps xmm0, xmm1
    x0 = shufps(x0, x0, 0); // 0076cc29 shufps xmm0, xmm0, 0
    x0 = sqrtps(x0); // 0076cc2d sqrtps xmm0, xmm0
    { CMP_A = eax; CMP_B = uint32_t(0x64); } // 0076cc30 cmp eax, 0x64
    if (CMP_A >= CMP_B) goto L_0076cca8; // 0076cc33 jae 0x76cca8
    edx = g.u32((esp + uint32_t(0x14))); // 0076cc35 mov edx, dword ptr [esp + 0x14]
    ecx = (eax + uint32_t(1)); // 0076cc39 lea ecx, [eax + 1]
    g.u32((edi + uint32_t(0x1900)), ecx); // 0076cc3c mov dword ptr [edi + 0x1900], ecx
    ecx = g.u32((edx + uint32_t(0x34))); // 0076cc42 mov ecx, dword ptr [edx + 0x34]
    { const uint32_t r_ = eax << (uint32_t(6) & 31u); eax = r_; } // 0076cc45 shl eax, 6
    { const uint32_t r_ = eax + edi; eax = r_; } // 0076cc48 add eax, edi
    g.u32((eax), ecx); // 0076cc4a mov dword ptr [eax], ecx
    edx = g.u32((esi + uint32_t(0x34))); // 0076cc4c mov edx, dword ptr [esi + 0x34]
    g.u32((eax + uint32_t(4)), edx); // 0076cc4f mov dword ptr [eax + 4], edx
    g.u32((eax + uint32_t(0x2c)), uint32_t(0)); // 0076cc52 mov dword ptr [eax + 0x2c], 0
    g.u8((eax + uint32_t(0x20)), uint8_t(uint32_t(0))); // 0076cc59 mov byte ptr [eax + 0x20], 0
    ecx = g.u32((esp + uint32_t(0x670))); // 0076cc5d mov ecx, dword ptr [esp + 0x670]
    g.u32((eax + uint32_t(0x30)), ecx); // 0076cc64 mov dword ptr [eax + 0x30], ecx
    edx = g.u32((esp + uint32_t(0x6e8))); // 0076cc67 mov edx, dword ptr [esp + 0x6e8]
    g.u32((eax + uint32_t(0x34)), edx); // 0076cc6e mov dword ptr [eax + 0x34], edx
    x1 = X{{g.u32((esp + uint32_t(0xd0))), 0, 0, 0}}; // 0076cc71 movss xmm1, dword ptr [esp + 0xd0]
    g.u32((eax + uint32_t(0x10)), x1.u[0]); // 0076cc7a movss dword ptr [eax + 0x10], xmm1
    x1 = X{{g.u32((esp + uint32_t(0xd4))), 0, 0, 0}}; // 0076cc7f movss xmm1, dword ptr [esp + 0xd4]
    g.u32((eax + uint32_t(0x14)), x1.u[0]); // 0076cc88 movss dword ptr [eax + 0x14], xmm1
    x1 = X{{g.u32((esp + uint32_t(0xd8))), 0, 0, 0}}; // 0076cc8d movss xmm1, dword ptr [esp + 0xd8]
    g.u32((eax + uint32_t(0x18)), x1.u[0]); // 0076cc96 movss dword ptr [eax + 0x18], xmm1
    x1 = X::splat(0); // 0076cc9b xorps xmm1, xmm1
    g.u32((eax + uint32_t(0x24)), x1.u[0]); // 0076cc9e movss dword ptr [eax + 0x24], xmm1
    g.u32((eax + uint32_t(0x28)), x0.u[0]); // 0076cca3 movss dword ptr [eax + 0x28], xmm0
  L_0076cca8:;
    eax = g.u32((edi + uint32_t(0x19ac))); // 0076cca8 mov eax, dword ptr [edi + 0x19ac]
    edx = g.u32((eax + uint32_t(0x30))); // 0076ccae mov edx, dword ptr [eax + 0x30]
    ecx = g.u32((eax)); // 0076ccb1 mov ecx, dword ptr [eax]
    { CMP_A = ecx; CMP_B = g.u32((edx + uint32_t(0x400))); } // 0076ccb3 cmp ecx, dword ptr [edx + 0x400]
    if (int32_t(CMP_A) >= int32_t(CMP_B)) goto L_0076ccc4; // 0076ccb9 jge 0x76ccc4
    { const uint32_t r_ = ecx + 1u; ecx = r_; } // 0076ccbb inc ecx
    g.u32((eax), ecx); // 0076ccbc mov dword ptr [eax], ecx
    ecx = g.u32((edx + ecx * 4 - uint32_t(4))); // 0076ccbe mov ecx, dword ptr [edx + ecx*4 - 4]
    goto L_0076ccc7; // 0076ccc2 jmp 0x76ccc7
  L_0076ccc4:;
    { const uint32_t r_ = ecx | uint32_t(0xffffffff); ecx = r_; } // 0076ccc4 or ecx, 0xffffffff
  L_0076ccc7:;
    esi = ecx; // 0076ccc7 mov esi, ecx
    { CMP_A = ecx; CMP_B = uint32_t(-1); } // 0076ccc9 cmp ecx, -1
    if (CMP_A != CMP_B) goto L_0076c9a0; // 0076cccc jne 0x76c9a0
  L_0076ccd2:;
    eax = g.u32((esp + uint32_t(0x10))); // 0076ccd2 mov eax, dword ptr [esp + 0x10]
    { const uint32_t r_ = g.u32((esp + uint32_t(0xc))) - uint32_t(-0x80); g.u32((esp + uint32_t(0xc)), r_); } // 0076ccd6 sub dword ptr [esp + 0xc], -0x80
    { const uint32_t r_ = eax + 1u; eax = r_; } // 0076ccdb inc eax
    g.u32((esp + uint32_t(0x10)), eax); // 0076ccdc mov dword ptr [esp + 0x10], eax
    { CMP_A = eax; CMP_B = g.u32((esp + uint32_t(0x1c))); } // 0076cce0 cmp eax, dword ptr [esp + 0x1c]
    if (CMP_A < CMP_B) goto L_0076c8eb; // 0076cce4 jb 0x76c8eb
  L_0076ccea:;
    ecx = g.u32((esp + uint32_t(0x77c))); // 0076ccea mov ecx, dword ptr [esp + 0x77c]
    { const uint32_t v_ = g.u32(esp); esp += 4; esi = v_; } // 0076ccf1 pop esi
    { const uint32_t v_ = g.u32(esp); esp += 4; ebx = v_; } // 0076ccf2 pop ebx
    { const uint32_t r_ = ecx ^ esp; ecx = r_; } // 0076ccf3 xor ecx, esp
    LIT_CALL(0x8f9efe); // 0076ccf5 call 0x8f9efe
    esp = ebp; // 0076ccfa mov esp, ebp
    { const uint32_t v_ = g.u32(esp); esp += 4; ebp = v_; } // 0076ccfc pop ebp
    goto L_return; // 0076ccfd ret 

    LIT_END(0x0)
}

// 0076cd00 Kinematic_CollideKeyed: active kinematic bodies against the keyed objects (EDI = world): see the hand port in world_phys.cpp.
REFLEX_PORT_FN(0x0076cd00, Kinematic_CollideKeyed, "world", kLitOut) {
    LIT_BEGIN
    { const uint32_t v_ = ebp; esp -= 4; g.u32(esp, v_); } // 0076cd00 push ebp
    ebp = esp; // 0076cd01 mov ebp, esp
    { const uint32_t r_ = esp & uint32_t(0xfffffff0); esp = r_; } // 0076cd03 and esp, 0xfffffff0
    { const uint32_t r_ = esp - uint32_t(0x7a8); esp = r_; } // 0076cd06 sub esp, 0x7a8
    eax = g.u32((uint32_t(0x9b45e4))); // 0076cd0c mov eax, dword ptr [0x9b45e4]
    { const uint32_t r_ = eax ^ esp; eax = r_; } // 0076cd11 xor eax, esp
    g.u32((esp + uint32_t(0x7a4)), eax); // 0076cd13 mov dword ptr [esp + 0x7a4], eax
    x0 = X{{g.u32((uint32_t(0x9cc8a8))), 0, 0, 0}}; // 0076cd1a movss xmm0, dword ptr [0x9cc8a8]
    { const uint32_t v_ = ebx; esp -= 4; g.u32(esp, v_); } // 0076cd22 push ebx
    x0 = shufps(x0, x0, 0); // 0076cd23 shufps xmm0, xmm0, 0
    { const uint32_t v_ = esi; esp -= 4; g.u32(esp, v_); } // 0076cd27 push esi
    st(g, (esp + uint32_t(0x60)), x0); // 0076cd28 movaps xmmword ptr [esp + 0x60], xmm0
    eax = 0u; // 0076cd2d xor eax, eax
  L_0076cd30:;
    { const uint32_t r_ = ecx | uint32_t(0xffffffff); ecx = r_; } // 0076cd30 or ecx, 0xffffffff
    g.u32((esp + eax * 4 + uint32_t(0x6a0)), ecx); // 0076cd33 mov dword ptr [esp + eax*4 + 0x6a0], ecx
    g.u32((esp + eax * 4 + uint32_t(0x718)), ecx); // 0076cd3a mov dword ptr [esp + eax*4 + 0x718], ecx
    { const uint32_t r_ = eax + 1u; eax = r_; } // 0076cd41 inc eax
    { CMP_A = eax; CMP_B = uint32_t(0x1e); } // 0076cd42 cmp eax, 0x1e
    if (int32_t(CMP_A) < int32_t(CMP_B)) goto L_0076cd30; // 0076cd45 jl 0x76cd30
    { CMP_A = g.u32((edi + uint32_t(0x19d4))); CMP_B = uint32_t(0); } // 0076cd47 cmp dword ptr [edi + 0x19d4], 0
    g.u32((esp + uint32_t(0x3c)), uint32_t(0)); // 0076cd4e mov dword ptr [esp + 0x3c], 0
    if (int32_t(CMP_A) <= int32_t(CMP_B)) goto L_0076d3ab; // 0076cd56 jle 0x76d3ab
    eax = (edi + uint32_t(0x1d00)); // 0076cd5c lea eax, [edi + 0x1d00]
    ecx = (edi + uint32_t(0x9a00)); // 0076cd62 lea ecx, [edi + 0x9a00]
    edx = (edi + uint32_t(0x19d8)); // 0076cd68 lea edx, [edi + 0x19d8]
    g.u32((esp + uint32_t(0x10)), eax); // 0076cd6e mov dword ptr [esp + 0x10], eax
    g.u32((esp + uint32_t(0x38)), ecx); // 0076cd72 mov dword ptr [esp + 0x38], ecx
    g.u32((esp + uint32_t(0x30)), edx); // 0076cd76 mov dword ptr [esp + 0x30], edx
    ebx = (ebx); // 0076cd7a lea ebx, [ebx]
  L_0076cd80:;
    eax = g.u32((esp + uint32_t(0x30))); // 0076cd80 mov eax, dword ptr [esp + 0x30]
    esi = g.u32((eax)); // 0076cd84 mov esi, dword ptr [eax]
    x0 = ld(g, (esi + uint32_t(0x40))); // 0076cd86 movaps xmm0, xmmword ptr [esi + 0x40]
    st(g, (esp + uint32_t(0x90)), x0); // 0076cd8a movaps xmmword ptr [esp + 0x90], xmm0
    x0 = ld(g, (esi + uint32_t(0x50))); // 0076cd92 movaps xmm0, xmmword ptr [esi + 0x50]
    st(g, (esp + uint32_t(0xb0)), x0); // 0076cd96 movaps xmmword ptr [esp + 0xb0], xmm0
    { CMP_A = uint32_t(g.u8((esi + uint32_t(0x84)))); CMP_B = uint32_t(1); } // 0076cd9e test byte ptr [esi + 0x84], 1
    g.u32((esp + uint32_t(0x34)), esi); // 0076cda5 mov dword ptr [esp + 0x34], esi
    if ((CMP_A & CMP_B) == 0) goto L_0076d387; // 0076cda9 je 0x76d387
    ecx = g.u32((edi + uint32_t(0x19a4))); // 0076cdaf mov ecx, dword ptr [edi + 0x19a4]
    { const uint32_t r_ = ecx - g.u32((edi + uint32_t(0x19a0))); ecx = r_; } // 0076cdb5 sub ecx, dword ptr [edi + 0x19a0]
    eax = uint32_t(0x92492493); // 0076cdbb mov eax, 0x92492493
    { const int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); } // 0076cdc0 imul ecx
    { const uint32_t r_ = edx + ecx; edx = r_; } // 0076cdc2 add edx, ecx
    { const uint32_t r_ = uint32_t(int32_t(edx) >> (uint32_t(7) & 31u)); edx = r_; } // 0076cdc4 sar edx, 7
    ecx = edx; // 0076cdc7 mov ecx, edx
    { const uint32_t r_ = ecx >> (uint32_t(0x1f) & 31u); ecx = r_; } // 0076cdc9 shr ecx, 0x1f
    { const uint32_t r_ = ecx + edx; ecx = r_; } // 0076cdcc add ecx, edx
    if (ecx != 0) goto L_0076cdd6; // 0076cdce jne 0x76cdd6
    LIT_CALL(g.u32((uint32_t(0x916224)))); // 0076cdd0 call dword ptr [0x916224]
  L_0076cdd6:;
    ebx = g.u32((edi + uint32_t(0x19ac))); // 0076cdd6 mov ebx, dword ptr [edi + 0x19ac]
    ecx = esi; // 0076cddc mov ecx, esi
    { const uint32_t r_ = ecx - g.u32((edi + uint32_t(0x19a0))); ecx = r_; } // 0076cdde sub ecx, dword ptr [edi + 0x19a0]
    esi = g.u32((ebx + uint32_t(0x30))); // 0076cde4 mov esi, dword ptr [ebx + 0x30]
    eax = uint32_t(0x92492493); // 0076cde7 mov eax, 0x92492493
    { const int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); } // 0076cdec imul ecx
    { const uint32_t r_ = edx + ecx; edx = r_; } // 0076cdee add edx, ecx
    ecx = g.u32((ebx + uint32_t(0x30))); // 0076cdf0 mov ecx, dword ptr [ebx + 0x30]
    { const uint32_t r_ = uint32_t(int32_t(edx) >> (uint32_t(7) & 31u)); edx = r_; } // 0076cdf3 sar edx, 7
    eax = edx; // 0076cdf6 mov eax, edx
    { const uint32_t r_ = eax >> (uint32_t(0x1f) & 31u); eax = r_; } // 0076cdf8 shr eax, 0x1f
    { const uint32_t r_ = eax + edx; eax = r_; } // 0076cdfb add eax, edx
    edx = g.u32((ecx + uint32_t(0x41c))); // 0076cdfd mov edx, dword ptr [ecx + 0x41c]
    ecx = g.u32((ecx + uint32_t(0x420))); // 0076ce03 mov ecx, dword ptr [ecx + 0x420]
    { const uint32_t r_ = eax << (uint32_t(4) & 31u); eax = r_; } // 0076ce09 shl eax, 4
    x0 = ld(g, (edx + eax)); // 0076ce0c movaps xmm0, xmmword ptr [edx + eax]
    edx = (esp + uint32_t(0xc0)); // 0076ce10 lea edx, [esp + 0xc0]
    st(g, (esp + uint32_t(0xa0)), x0); // 0076ce17 movaps xmmword ptr [esp + 0xa0], xmm0
    x0 = ld(g, (ecx + eax)); // 0076ce1f movaps xmm0, xmmword ptr [ecx + eax]
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 0076ce23 push edx
    edx = (esp + uint32_t(0xa4)); // 0076ce24 lea edx, [esp + 0xa4]
    st(g, (esp + uint32_t(0xc4)), x0); // 0076ce2b movaps xmmword ptr [esp + 0xc4], xmm0
    LIT_CALL(0x883020); // 0076ce33 call 0x883020
    g.u32((ebx), uint32_t(0)); // 0076ce38 mov dword ptr [ebx], 0
    eax = g.u32((edi + uint32_t(0x19ac))); // 0076ce3e mov eax, dword ptr [edi + 0x19ac]
    ecx = g.u32((eax + uint32_t(0x30))); // 0076ce44 mov ecx, dword ptr [eax + 0x30]
    edx = g.u32((eax)); // 0076ce47 mov edx, dword ptr [eax]
    { CMP_A = edx; CMP_B = g.u32((ecx + uint32_t(0x400))); } // 0076ce49 cmp edx, dword ptr [ecx + 0x400]
    if (int32_t(CMP_A) >= int32_t(CMP_B)) goto L_0076ce5a; // 0076ce4f jge 0x76ce5a
    { const uint32_t r_ = edx + 1u; edx = r_; } // 0076ce51 inc edx
    g.u32((eax), edx); // 0076ce52 mov dword ptr [eax], edx
    ecx = g.u32((ecx + edx * 4 - uint32_t(4))); // 0076ce54 mov ecx, dword ptr [ecx + edx*4 - 4]
    goto L_0076ce5d; // 0076ce58 jmp 0x76ce5d
  L_0076ce5a:;
    { const uint32_t r_ = ecx | uint32_t(0xffffffff); ecx = r_; } // 0076ce5a or ecx, 0xffffffff
  L_0076ce5d:;
    esi = ecx; // 0076ce5d mov esi, ecx
    { CMP_A = ecx; CMP_B = uint32_t(-1); } // 0076ce5f cmp ecx, -1
    if (CMP_A == CMP_B) goto L_0076d387; // 0076ce62 je 0x76d387
    goto L_0076ce70; // 0076ce68 jmp 0x76ce70
    ebx = (ebx); // 0076ce6a lea ebx, [ebx]
  L_0076ce70:;
    ecx = g.u32((edi + uint32_t(0x19a4))); // 0076ce70 mov ecx, dword ptr [edi + 0x19a4]
    { const uint32_t r_ = ecx - g.u32((edi + uint32_t(0x19a0))); ecx = r_; } // 0076ce76 sub ecx, dword ptr [edi + 0x19a0]
    eax = uint32_t(0x92492493); // 0076ce7c mov eax, 0x92492493
    { const int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); } // 0076ce81 imul ecx
    { const uint32_t r_ = edx + ecx; edx = r_; } // 0076ce83 add edx, ecx
    { const uint32_t r_ = uint32_t(int32_t(edx) >> (uint32_t(7) & 31u)); edx = r_; } // 0076ce85 sar edx, 7
    eax = edx; // 0076ce88 mov eax, edx
    { const uint32_t r_ = eax >> (uint32_t(0x1f) & 31u); eax = r_; } // 0076ce8a shr eax, 0x1f
    { const uint32_t r_ = eax + edx; eax = r_; } // 0076ce8d add eax, edx
    if (eax != 0) goto L_0076ce97; // 0076ce8f jne 0x76ce97
    LIT_CALL(g.u32((uint32_t(0x916224)))); // 0076ce91 call dword ptr [0x916224]
  L_0076ce97:;
    ebx = g.u32((esp + uint32_t(0x34))); // 0076ce97 mov ebx, dword ptr [esp + 0x34]
    ecx = ebx; // 0076ce9b mov ecx, ebx
    { const uint32_t r_ = ecx - g.u32((edi + uint32_t(0x19a0))); ecx = r_; } // 0076ce9d sub ecx, dword ptr [edi + 0x19a0]
    eax = uint32_t(0x92492493); // 0076cea3 mov eax, 0x92492493
    { const int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); } // 0076cea8 imul ecx
    { const uint32_t r_ = edx + ecx; edx = r_; } // 0076ceaa add edx, ecx
    { const uint32_t r_ = uint32_t(int32_t(edx) >> (uint32_t(7) & 31u)); edx = r_; } // 0076ceac sar edx, 7
    ecx = edx; // 0076ceaf mov ecx, edx
    { const uint32_t r_ = ecx >> (uint32_t(0x1f) & 31u); ecx = r_; } // 0076ceb1 shr ecx, 0x1f
    { const uint32_t r_ = ecx + edx; ecx = r_; } // 0076ceb4 add ecx, edx
    { CMP_A = esi; CMP_B = ecx; } // 0076ceb6 cmp esi, ecx
    if (CMP_A == CMP_B) goto L_0076d35d; // 0076ceb8 je 0x76d35d
    ecx = g.u32((edi + uint32_t(0x19a4))); // 0076cebe mov ecx, dword ptr [edi + 0x19a4]
    { const uint32_t r_ = ecx - g.u32((edi + uint32_t(0x19a0))); ecx = r_; } // 0076cec4 sub ecx, dword ptr [edi + 0x19a0]
    eax = uint32_t(0x92492493); // 0076ceca mov eax, 0x92492493
    { const int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); } // 0076cecf imul ecx
    { const uint32_t r_ = edx + ecx; edx = r_; } // 0076ced1 add edx, ecx
    { const uint32_t r_ = uint32_t(int32_t(edx) >> (uint32_t(7) & 31u)); edx = r_; } // 0076ced3 sar edx, 7
    eax = edx; // 0076ced6 mov eax, edx
    { const uint32_t r_ = eax >> (uint32_t(0x1f) & 31u); eax = r_; } // 0076ced8 shr eax, 0x1f
    { const uint32_t r_ = eax + edx; eax = r_; } // 0076cedb add eax, edx
    { CMP_A = esi; CMP_B = eax; } // 0076cedd cmp esi, eax
    if (CMP_A < CMP_B) goto L_0076cee7; // 0076cedf jb 0x76cee7
    LIT_CALL(g.u32((uint32_t(0x916224)))); // 0076cee1 call dword ptr [0x916224]
  L_0076cee7:;
    esi = uint32_t(int32_t(esi) * int32_t(uint32_t(0xe0))); // 0076cee7 imul esi, esi, 0xe0
    { const uint32_t r_ = esi + g.u32((edi + uint32_t(0x19a0))); esi = r_; } // 0076ceed add esi, dword ptr [edi + 0x19a0]
    { CMP_A = uint32_t(g.u8((esi + uint32_t(0xb0)))); CMP_B = uint32_t(0); } // 0076cef3 cmp byte ptr [esi + 0xb0], 0
    if (CMP_A == CMP_B) goto L_0076d35d; // 0076cefa je 0x76d35d
    x0 = ld(g, (esi + uint32_t(0x40))); // 0076cf00 movaps xmm0, xmmword ptr [esi + 0x40]
    edx = 0u; // 0076cf04 xor edx, edx
    st(g, (esp + uint32_t(0xe0)), x0); // 0076cf06 movaps xmmword ptr [esp + 0xe0], xmm0
    x0 = ld(g, (esi + uint32_t(0x50))); // 0076cf0e movaps xmm0, xmmword ptr [esi + 0x50]
    st(g, (esp + uint32_t(0x80)), x0); // 0076cf12 movaps xmmword ptr [esp + 0x80], xmm0
    g.u32((esp + uint32_t(0x790)), edx); // 0076cf1a mov dword ptr [esp + 0x790], edx
    g.u32((esp + uint32_t(0xf0)), edx); // 0076cf21 mov dword ptr [esp + 0xf0], edx
    eax = 0u; // 0076cf28 xor eax, eax
    ebx = (ebx); // 0076cf2a lea ebx, [ebx]
  L_0076cf30:;
    { const uint32_t r_ = ecx | uint32_t(0xffffffff); ecx = r_; } // 0076cf30 or ecx, 0xffffffff
    g.u32((esp + eax * 4 + uint32_t(0x6a0)), ecx); // 0076cf33 mov dword ptr [esp + eax*4 + 0x6a0], ecx
    g.u32((esp + eax * 4 + uint32_t(0x718)), ecx); // 0076cf3a mov dword ptr [esp + eax*4 + 0x718], ecx
    { const uint32_t r_ = eax + 1u; eax = r_; } // 0076cf41 inc eax
    { CMP_A = eax; CMP_B = uint32_t(0x1e); } // 0076cf42 cmp eax, 0x1e
    if (int32_t(CMP_A) < int32_t(CMP_B)) goto L_0076cf30; // 0076cf45 jl 0x76cf30
    eax = g.u32((esi + uint32_t(0x2c))); // 0076cf47 mov eax, dword ptr [esi + 0x2c]
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 0076cf4a push edx
    ecx = (esp + uint32_t(0xe4)); // 0076cf4b lea ecx, [esp + 0xe4]
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 0076cf52 push ecx
    edx = (esp + uint32_t(0x88)); // 0076cf53 lea edx, [esp + 0x88]
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 0076cf5a push edx
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 0076cf5b push eax
    eax = g.u32((ebx + uint32_t(0x2c))); // 0076cf5c mov eax, dword ptr [ebx + 0x2c]
    ecx = (esp + uint32_t(0xa0)); // 0076cf5f lea ecx, [esp + 0xa0]
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 0076cf66 push ecx
    edx = (esp + uint32_t(0xc4)); // 0076cf67 lea edx, [esp + 0xc4]
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 0076cf6e push edx
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 0076cf6f push eax
    ecx = (esp + uint32_t(0x10c)); // 0076cf70 lea ecx, [esp + 0x10c]
    LIT_CALL(0x85ddd0); // 0076cf77 call 0x85ddd0
    { CMP_A = g.u32((esp + uint32_t(0x790))); CMP_B = uint32_t(0); } // 0076cf7c cmp dword ptr [esp + 0x790], 0
    if (CMP_A == CMP_B) goto L_0076d35d; // 0076cf84 je 0x76d35d
    ecx = g.u32((esp + uint32_t(0x10))); // 0076cf8a mov ecx, dword ptr [esp + 0x10]
    eax = g.u32((esi + uint32_t(0x80))); // 0076cf8e mov eax, dword ptr [esi + 0x80]
    x0 = ld(g, (ecx + uint32_t(0x10))); // 0076cf94 movaps xmm0, xmmword ptr [ecx + 0x10]
    x2 = ld(g, (uint32_t(0xd61670))); // 0076cf98 movaps xmm2, xmmword ptr [0xd61670]
    st(g, (esp + uint32_t(0xd0)), x0); // 0076cf9f movaps xmmword ptr [esp + 0xd0], xmm0
    st(g, (esp + uint32_t(0x70)), x2); // 0076cfa7 movaps xmmword ptr [esp + 0x70], xmm2
    { CMP_A = eax; CMP_B = eax; } // 0076cfac test eax, eax
    if (int32_t((CMP_A & CMP_B)) < 0) goto L_0076cfc3; // 0076cfae jl 0x76cfc3
    edx = (eax + eax * 4 + uint32_t(0x1d1)); // 0076cfb0 lea edx, [eax + eax*4 + 0x1d1]
    { const uint32_t r_ = edx << (uint32_t(4) & 31u); edx = r_; } // 0076cfb7 shl edx, 4
    x0 = ld(g, (edx + edi)); // 0076cfba movaps xmm0, xmmword ptr [edx + edi]
    st(g, (esp + uint32_t(0x70)), x0); // 0076cfbe movaps xmmword ptr [esp + 0x70], xmm0
  L_0076cfc3:;
    ecx = g.u32((esp + uint32_t(0xf0))); // 0076cfc3 mov ecx, dword ptr [esp + 0xf0]
    ebx = 0u; // 0076cfca xor ebx, ebx
    { CMP_A = ecx; CMP_B = ecx; } // 0076cfcc test ecx, ecx
    if (int32_t((CMP_A & CMP_B)) <= 0) goto L_0076d004; // 0076cfce jle 0x76d004
    eax = (esp + uint32_t(0x100)); // 0076cfd0 lea eax, [esp + 0x100]
    g.u32((esp + uint32_t(0x2c)), eax); // 0076cfd7 mov dword ptr [esp + 0x2c], eax
  L_0076cfdb:;
    ecx = g.u32((esp + uint32_t(0x34))); // 0076cfdb mov ecx, dword ptr [esp + 0x34]
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 0076cfdf push ecx
    ecx = g.u32((esp + uint32_t(0x30))); // 0076cfe0 mov ecx, dword ptr [esp + 0x30]
    { const uint32_t v_ = edi; esp -= 4; g.u32(esp, v_); } // 0076cfe4 push edi
    edx = esi; // 0076cfe5 mov edx, esi
    LIT_CALL(0x76ea00); // 0076cfe7 call 0x76ea00
    ecx = g.u32((esp + uint32_t(0xf0))); // 0076cfec mov ecx, dword ptr [esp + 0xf0]
    { const uint32_t r_ = g.u32((esp + uint32_t(0x2c))) + uint32_t(0x30); g.u32((esp + uint32_t(0x2c)), r_); } // 0076cff3 add dword ptr [esp + 0x2c], 0x30
    { const uint32_t r_ = ebx + 1u; ebx = r_; } // 0076cff8 inc ebx
    { CMP_A = ebx; CMP_B = ecx; } // 0076cff9 cmp ebx, ecx
    if (int32_t(CMP_A) < int32_t(CMP_B)) goto L_0076cfdb; // 0076cffb jl 0x76cfdb
    x2 = ld(g, (uint32_t(0xd61670))); // 0076cffd movaps xmm2, xmmword ptr [0xd61670]
  L_0076d004:;
    x1 = x2; // 0076d004 movaps xmm1, xmm2
    x3 = x2; // 0076d007 movaps xmm3, xmm2
    { CMP_A = ecx; CMP_B = ecx; } // 0076d00a test ecx, ecx
    if (int32_t((CMP_A & CMP_B)) <= 0) goto L_0076d02d; // 0076d00c jle 0x76d02d
    eax = (esp + uint32_t(0x110)); // 0076d00e lea eax, [esp + 0x110]
  L_0076d015:;
    x0 = ld(g, (eax + uint32_t(0x10))); // 0076d015 movaps xmm0, xmmword ptr [eax + 0x10]
    x4 = ld(g, (eax)); // 0076d019 movaps xmm4, xmmword ptr [eax]
    x0 = mulps(x0, x4); // 0076d01c mulps xmm0, xmm4
    { const uint32_t r_ = eax + uint32_t(0x30); eax = r_; } // 0076d01f add eax, 0x30
    { const uint32_t r_ = ecx - uint32_t(1); ecx = r_; } // 0076d022 sub ecx, 1
    x1 = maxps(x1, x0); // 0076d025 maxps xmm1, xmm0
    x3 = minps(x3, x0); // 0076d028 minps xmm3, xmm0
    if (ecx != 0) goto L_0076d015; // 0076d02b jne 0x76d015
  L_0076d02d:;
    eax = g.u32((uint32_t(0xda67d4))); // 0076d02d mov eax, dword ptr [0xda67d4]
    { const uint32_t r_ = eax - uint32_t(1); eax = r_; } // 0076d032 sub eax, 1
    x0 = X::splat(0); // 0076d035 xorps xmm0, xmm0
    x0 = subps(x0, ld(g, (esp + uint32_t(0x60)))); // 0076d038 subps xmm0, xmmword ptr [esp + 0x60]
    x3 = addps(x3, x1); // 0076d03d addps xmm3, xmm1
    x3 = mulps(x3, x0); // 0076d040 mulps xmm3, xmm0
    st(g, (esp + uint32_t(0x40)), x3); // 0076d043 movaps xmmword ptr [esp + 0x40], xmm3
    if (eax != 0) goto L_0076d064; // 0076d048 jne 0x76d064
    ecx = g.u32((esp + uint32_t(0x38))); // 0076d04a mov ecx, dword ptr [esp + 0x38]
    edx = (esp + uint32_t(0x40)); // 0076d04e lea edx, [esp + 0x40]
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 0076d052 push edx
    LIT_CALL(0x84fe00); // 0076d053 call 0x84fe00
    x3 = ld(g, (esp + uint32_t(0x40))); // 0076d058 movaps xmm3, xmmword ptr [esp + 0x40]
    x2 = ld(g, (uint32_t(0xd61670))); // 0076d05d movaps xmm2, xmmword ptr [0xd61670]
  L_0076d064:;
    ebx = g.u32((esp + uint32_t(0x34))); // 0076d064 mov ebx, dword ptr [esp + 0x34]
    eax = g.u32((ebx + uint32_t(0x20))); // 0076d068 mov eax, dword ptr [ebx + 0x20]
    x0 = x3; // 0076d06b movaps xmm0, xmm3
    x0 = mulps(x0, x3); // 0076d06e mulps xmm0, xmm3
    x1 = x0; // 0076d071 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x66); // 0076d074 shufps xmm1, xmm0, 0x66
    x0.u[0] = x86::bits(x86::add(x0.f(0), x1.f(0))); // 0076d078 addss xmm0, xmm1
    x1 = shufps(x1, x1, 0x55); // 0076d07c shufps xmm1, xmm1, 0x55
    x0.u[0] = x86::bits(x86::add(x0.f(0), x1.f(0))); // 0076d080 addss xmm0, xmm1
    x0 = shufps(x0, x0, 0); // 0076d084 shufps xmm0, xmm0, 0
    x1 = sqrtps(x0); // 0076d088 sqrtps xmm1, xmm0
    x0 = ld(g, (eax + uint32_t(0xc0))); // 0076d08b movaps xmm0, xmmword ptr [eax + 0xc0]
    x4 = x0; // 0076d092 movaps xmm4, xmm0
    x4 = cmpltps(x4, x1); // 0076d095 cmpltps xmm4, xmm1
    st(g, (esp + uint32_t(0x50)), x4); // 0076d099 movaps xmmword ptr [esp + 0x50], xmm4
    ecx = g.u32((esp + uint32_t(0x54))); // 0076d09e mov ecx, dword ptr [esp + 0x54]
    edx = g.u32((esp + uint32_t(0x58))); // 0076d0a2 mov edx, dword ptr [esp + 0x58]
    { const uint32_t r_ = edx + ecx; edx = r_; } // 0076d0a6 add edx, ecx
    { const uint32_t r_ = edx + g.u32((esp + uint32_t(0x50))); edx = r_; } // 0076d0a8 add edx, dword ptr [esp + 0x50]
    { CMP_A = edx; CMP_B = uint32_t(-3); } // 0076d0ac cmp edx, -3
    if (CMP_A != CMP_B) goto L_0076d0c1; // 0076d0af jne 0x76d0c1
    x0 = divps(x0, x1); // 0076d0b1 divps xmm0, xmm1
    x3 = mulps(x3, x0); // 0076d0b4 mulps xmm3, xmm0
    st(g, (esp + uint32_t(0x60)), x0); // 0076d0b7 movaps xmmword ptr [esp + 0x60], xmm0
    st(g, (esp + uint32_t(0x40)), x3); // 0076d0bc movaps xmmword ptr [esp + 0x40], xmm3
  L_0076d0c1:;
    { CMP_A = g.u32((ebx + uint32_t(0x80))); CMP_B = uint32_t(0); } // 0076d0c1 cmp dword ptr [ebx + 0x80], 0
    ecx = (ecx & ~(0xffu << 0)) | (((((int32_t(CMP_A) >= int32_t(CMP_B)) ? 1u : 0u)) & 0xffu) << 0); // 0076d0c8 setge cl
    { CMP_A = g.u32((esi + uint32_t(0x80))); CMP_B = uint32_t(0); } // 0076d0cb cmp dword ptr [esi + 0x80], 0
    g.u8((esp + uint32_t(0xf)), uint8_t(((ecx >> 0) & 0xffu))); // 0076d0d2 mov byte ptr [esp + 0xf], cl
    if (int32_t(CMP_A) < int32_t(CMP_B)) goto L_0076d0e5; // 0076d0d6 jl 0x76d0e5
    { CMP_A = uint32_t(g.u8((esi + uint32_t(0x84)))); CMP_B = uint32_t(1); } // 0076d0d8 test byte ptr [esi + 0x84], 1
    if ((CMP_A & CMP_B) == 0) goto L_0076d0e5; // 0076d0df je 0x76d0e5
    eax = (eax & ~(0xffu << 0)) | (((uint32_t(1)) & 0xffu) << 0); // 0076d0e1 mov al, 1
    goto L_0076d0e7; // 0076d0e3 jmp 0x76d0e7
  L_0076d0e5:;
    eax = (eax & ~(0xffu << 0)) | (((0u) & 0xffu) << 0); // 0076d0e5 xor al, al
  L_0076d0e7:;
    { CMP_A = ((ecx >> 0) & 0xffu); CMP_B = ((ecx >> 0) & 0xffu); } // 0076d0e7 test cl, cl
    if ((CMP_A & CMP_B) == 0) goto L_0076d1fe; // 0076d0e9 je 0x76d1fe
    { CMP_A = ((eax >> 0) & 0xffu); CMP_B = ((eax >> 0) & 0xffu); } // 0076d0ef test al, al
    eax = g.u32((ebx + uint32_t(0xb4))); // 0076d0f1 mov eax, dword ptr [ebx + 0xb4]
    if ((CMP_A & CMP_B) == 0) goto L_0076d1c2; // 0076d0f7 je 0x76d1c2
    x0 = X{{g.u32((uint32_t(0x9322cc))), 0, 0, 0}}; // 0076d0fd movss xmm0, dword ptr [0x9322cc]
    x0 = shufps(x0, x0, 0); // 0076d105 shufps xmm0, xmm0, 0
    x0 = mulps(x0, x3); // 0076d109 mulps xmm0, xmm3
    x1 = x0; // 0076d10c movaps xmm1, xmm0
    { CMP_A = eax; CMP_B = eax; } // 0076d10f test eax, eax
    if ((CMP_A & CMP_B) == 0) goto L_0076d139; // 0076d111 je 0x76d139
    { CMP_A = ((eax >> 0) & 0xffu); CMP_B = uint32_t(1); } // 0076d113 test al, 1
    if ((CMP_A & CMP_B) == 0) goto L_0076d11f; // 0076d115 je 0x76d11f
    x3.u[0] = x2.u[0]; // 0076d117 movss xmm3, xmm2
    x1.u[0] = x3.u[0]; // 0076d11b movss xmm1, xmm3
  L_0076d11f:;
    { CMP_A = ((eax >> 0) & 0xffu); CMP_B = uint32_t(2); } // 0076d11f test al, 2
    if ((CMP_A & CMP_B) == 0) goto L_0076d131; // 0076d121 je 0x76d131
    x3 = x2; // 0076d123 movaps xmm3, xmm2
    x3 = shufps(x3, x1, 0xe5); // 0076d126 shufps xmm3, xmm1, 0xe5
    x3.u[0] = x1.u[0]; // 0076d12a movss xmm3, xmm1
    x1 = x3; // 0076d12e movaps xmm1, xmm3
  L_0076d131:;
    { CMP_A = ((eax >> 0) & 0xffu); CMP_B = uint32_t(4); } // 0076d131 test al, 4
    if ((CMP_A & CMP_B) == 0) goto L_0076d139; // 0076d133 je 0x76d139
    x1 = shufps(x1, x2, 0xa4); // 0076d135 shufps xmm1, xmm2, 0xa4
  L_0076d139:;
    eax = g.u32((esp + uint32_t(0x10))); // 0076d139 mov eax, dword ptr [esp + 0x10]
    x2 = ld(g, (eax)); // 0076d13d movaps xmm2, xmmword ptr [eax]
    x2 = addps(x2, x1); // 0076d140 addps xmm2, xmm1
    st(g, (eax), x2); // 0076d143 movaps xmmword ptr [eax], xmm2
    x1 = ld(g, (eax)); // 0076d146 movaps xmm1, xmmword ptr [eax]
    st(g, (ebx + uint32_t(0x40)), x1); // 0076d149 movaps xmmword ptr [ebx + 0x40], xmm1
    eax = g.u32((esi + uint32_t(0xb4))); // 0076d14d mov eax, dword ptr [esi + 0xb4]
    { CMP_A = eax; CMP_B = eax; } // 0076d153 test eax, eax
    if ((CMP_A & CMP_B) == 0) goto L_0076d184; // 0076d155 je 0x76d184
    x1 = ld(g, (uint32_t(0xd61670))); // 0076d157 movaps xmm1, xmmword ptr [0xd61670]
    { CMP_A = ((eax >> 0) & 0xffu); CMP_B = uint32_t(1); } // 0076d15e test al, 1
    if ((CMP_A & CMP_B) == 0) goto L_0076d16a; // 0076d160 je 0x76d16a
    x2.u[0] = x1.u[0]; // 0076d162 movss xmm2, xmm1
    x0.u[0] = x2.u[0]; // 0076d166 movss xmm0, xmm2
  L_0076d16a:;
    { CMP_A = ((eax >> 0) & 0xffu); CMP_B = uint32_t(2); } // 0076d16a test al, 2
    if ((CMP_A & CMP_B) == 0) goto L_0076d17c; // 0076d16c je 0x76d17c
    x2 = x1; // 0076d16e movaps xmm2, xmm1
    x2 = shufps(x2, x0, 0xe5); // 0076d171 shufps xmm2, xmm0, 0xe5
    x2.u[0] = x0.u[0]; // 0076d175 movss xmm2, xmm0
    x0 = x2; // 0076d179 movaps xmm0, xmm2
  L_0076d17c:;
    { CMP_A = ((eax >> 0) & 0xffu); CMP_B = uint32_t(4); } // 0076d17c test al, 4
    if ((CMP_A & CMP_B) == 0) goto L_0076d184; // 0076d17e je 0x76d184
    x0 = shufps(x0, x1, 0xa4); // 0076d180 shufps xmm0, xmm1, 0xa4
  L_0076d184:;
    eax = g.u32((esi + uint32_t(0x80))); // 0076d184 mov eax, dword ptr [esi + 0x80]
    eax = (eax + eax * 4); // 0076d18a lea eax, [eax + eax*4]
    { const uint32_t r_ = eax << (uint32_t(4) & 31u); eax = r_; } // 0076d18d shl eax, 4
    x1 = ld(g, (eax + edi + uint32_t(0x1d00))); // 0076d190 movaps xmm1, xmmword ptr [eax + edi + 0x1d00]
    eax = (eax + edi + uint32_t(0x1d00)); // 0076d198 lea eax, [eax + edi + 0x1d00]
    x1 = subps(x1, x0); // 0076d19f subps xmm1, xmm0
    st(g, (eax), x1); // 0076d1a2 movaps xmmword ptr [eax], xmm1
    eax = g.u32((esi + uint32_t(0x80))); // 0076d1a5 mov eax, dword ptr [esi + 0x80]
    edx = (eax + eax * 4); // 0076d1ab lea edx, [eax + eax*4]
    { const uint32_t r_ = edx << (uint32_t(4) & 31u); edx = r_; } // 0076d1ae shl edx, 4
    x0 = ld(g, (edx + edi + uint32_t(0x1d00))); // 0076d1b1 movaps xmm0, xmmword ptr [edx + edi + 0x1d00]
    st(g, (esi + uint32_t(0x40)), x0); // 0076d1b9 movaps xmmword ptr [esi + 0x40], xmm0
    goto L_0076d24f; // 0076d1bd jmp 0x76d24f
  L_0076d1c2:;
    { CMP_A = eax; CMP_B = eax; } // 0076d1c2 test eax, eax
    if ((CMP_A & CMP_B) == 0) goto L_0076d1ec; // 0076d1c4 je 0x76d1ec
    { CMP_A = ((eax >> 0) & 0xffu); CMP_B = uint32_t(1); } // 0076d1c6 test al, 1
    if ((CMP_A & CMP_B) == 0) goto L_0076d1d2; // 0076d1c8 je 0x76d1d2
    x0.u[0] = x2.u[0]; // 0076d1ca movss xmm0, xmm2
    x3.u[0] = x0.u[0]; // 0076d1ce movss xmm3, xmm0
  L_0076d1d2:;
    { CMP_A = ((eax >> 0) & 0xffu); CMP_B = uint32_t(2); } // 0076d1d2 test al, 2
    if ((CMP_A & CMP_B) == 0) goto L_0076d1e4; // 0076d1d4 je 0x76d1e4
    x0 = x2; // 0076d1d6 movaps xmm0, xmm2
    x0 = shufps(x0, x3, 0xe5); // 0076d1d9 shufps xmm0, xmm3, 0xe5
    x0.u[0] = x3.u[0]; // 0076d1dd movss xmm0, xmm3
    x3 = x0; // 0076d1e1 movaps xmm3, xmm0
  L_0076d1e4:;
    { CMP_A = ((eax >> 0) & 0xffu); CMP_B = uint32_t(4); } // 0076d1e4 test al, 4
    if ((CMP_A & CMP_B) == 0) goto L_0076d1ec; // 0076d1e6 je 0x76d1ec
    x3 = shufps(x3, x2, 0xa4); // 0076d1e8 shufps xmm3, xmm2, 0xa4
  L_0076d1ec:;
    eax = g.u32((esp + uint32_t(0x10))); // 0076d1ec mov eax, dword ptr [esp + 0x10]
    x0 = ld(g, (eax)); // 0076d1f0 movaps xmm0, xmmword ptr [eax]
    x0 = addps(x0, x3); // 0076d1f3 addps xmm0, xmm3
    st(g, (eax), x0); // 0076d1f6 movaps xmmword ptr [eax], xmm0
    x0 = ld(g, (eax)); // 0076d1f9 movaps xmm0, xmmword ptr [eax]
    goto L_0076d24b; // 0076d1fc jmp 0x76d24b
  L_0076d1fe:;
    { CMP_A = ((eax >> 0) & 0xffu); CMP_B = ((eax >> 0) & 0xffu); } // 0076d1fe test al, al
    if ((CMP_A & CMP_B) == 0) goto L_0076d256; // 0076d200 je 0x76d256
    ecx = (esp + uint32_t(0x40)); // 0076d202 lea ecx, [esp + 0x40]
    edx = esi; // 0076d206 mov edx, esi
    LIT_CALL(0x769f40); // 0076d208 call 0x769f40
    eax = g.u32((esi + uint32_t(0x80))); // 0076d20d mov eax, dword ptr [esi + 0x80]
    x0 = ld(g, (esp + uint32_t(0x40))); // 0076d213 movaps xmm0, xmmword ptr [esp + 0x40]
    eax = (eax + eax * 4); // 0076d218 lea eax, [eax + eax*4]
    { const uint32_t r_ = eax << (uint32_t(4) & 31u); eax = r_; } // 0076d21b shl eax, 4
    x1 = ld(g, (eax + edi + uint32_t(0x1d00))); // 0076d21e movaps xmm1, xmmword ptr [eax + edi + 0x1d00]
    eax = (eax + edi + uint32_t(0x1d00)); // 0076d226 lea eax, [eax + edi + 0x1d00]
    x1 = subps(x1, x0); // 0076d22d subps xmm1, xmm0
    st(g, (eax), x1); // 0076d230 movaps xmmword ptr [eax], xmm1
    eax = g.u32((esi + uint32_t(0x80))); // 0076d233 mov eax, dword ptr [esi + 0x80]
    ecx = (eax + eax * 4); // 0076d239 lea ecx, [eax + eax*4]
    { const uint32_t r_ = ecx << (uint32_t(4) & 31u); ecx = r_; } // 0076d23c shl ecx, 4
    x0 = ld(g, (ecx + edi + uint32_t(0x1d00))); // 0076d23f movaps xmm0, xmmword ptr [ecx + edi + 0x1d00]
    ecx = (ecx & ~(0xffu << 0)) | (((uint32_t(g.u8((esp + uint32_t(0xf))))) & 0xffu) << 0); // 0076d247 mov cl, byte ptr [esp + 0xf]
  L_0076d24b:;
    st(g, (ebx + uint32_t(0x40)), x0); // 0076d24b movaps xmmword ptr [ebx + 0x40], xmm0
  L_0076d24f:;
    x2 = ld(g, (uint32_t(0xd61670))); // 0076d24f movaps xmm2, xmmword ptr [0xd61670]
  L_0076d256:;
    eax = g.u32((esi + uint32_t(0x80))); // 0076d256 mov eax, dword ptr [esi + 0x80]
    x1 = x2; // 0076d25c movaps xmm1, xmm2
    { CMP_A = eax; CMP_B = eax; } // 0076d25f test eax, eax
    if (int32_t((CMP_A & CMP_B)) < 0) goto L_0076d283; // 0076d261 jl 0x76d283
    edx = (eax + eax * 4 + uint32_t(0x1d1)); // 0076d263 lea edx, [eax + eax*4 + 0x1d1]
    { const uint32_t r_ = edx << (uint32_t(4) & 31u); edx = r_; } // 0076d26a shl edx, 4
    x1 = ld(g, (edx + edi)); // 0076d26d movaps xmm1, xmmword ptr [edx + edi]
    { CMP_A = eax; CMP_B = eax; } // 0076d271 test eax, eax
    if (int32_t((CMP_A & CMP_B)) < 0) goto L_0076d283; // 0076d273 jl 0x76d283
    eax = (eax + eax * 4 + uint32_t(0x1d1)); // 0076d275 lea eax, [eax + eax*4 + 0x1d1]
    { const uint32_t r_ = eax << (uint32_t(4) & 31u); eax = r_; } // 0076d27c shl eax, 4
    x2 = ld(g, (eax + edi)); // 0076d27f movaps xmm2, xmmword ptr [eax + edi]
  L_0076d283:;
    x1 = subps(x1, ld(g, (esp + uint32_t(0xd0)))); // 0076d283 subps xmm1, xmmword ptr [esp + 0xd0]
    x0 = X::splat(0); // 0076d28b xorps xmm0, xmm0
    x2 = subps(x2, ld(g, (esp + uint32_t(0x70)))); // 0076d28e subps xmm2, xmmword ptr [esp + 0x70]
    eax = g.u32((edi + uint32_t(0x1900))); // 0076d293 mov eax, dword ptr [edi + 0x1900]
    g.u32((esp + uint32_t(0x2c)), x0.u[0]); // 0076d299 movss dword ptr [esp + 0x2c], xmm0
    g.u32((esp + uint32_t(0x2c)), x0.u[0]); // 0076d29f movss dword ptr [esp + 0x2c], xmm0
    x1 = mulps(x1, x1); // 0076d2a5 mulps xmm1, xmm1
    x0 = x1; // 0076d2a8 movaps xmm0, xmm1
    x0 = shufps(x0, x1, 0x66); // 0076d2ab shufps xmm0, xmm1, 0x66
    x1.u[0] = x86::bits(x86::add(x1.f(0), x0.f(0))); // 0076d2af addss xmm1, xmm0
    x0 = shufps(x0, x0, 0x55); // 0076d2b3 shufps xmm0, xmm0, 0x55
    x1.u[0] = x86::bits(x86::add(x1.f(0), x0.f(0))); // 0076d2b7 addss xmm1, xmm0
    x0 = x1; // 0076d2bb movaps xmm0, xmm1
    x0 = shufps(x0, x0, 0); // 0076d2be shufps xmm0, xmm0, 0
    x3 = sqrtps(x0); // 0076d2c2 sqrtps xmm3, xmm0
    x1 = x2; // 0076d2c5 movaps xmm1, xmm2
    x1 = mulps(x1, x1); // 0076d2c8 mulps xmm1, xmm1
    x0 = x1; // 0076d2cb movaps xmm0, xmm1
    x0 = shufps(x0, x1, 0x66); // 0076d2ce shufps xmm0, xmm1, 0x66
    x1.u[0] = x86::bits(x86::add(x1.f(0), x0.f(0))); // 0076d2d2 addss xmm1, xmm0
    x0 = shufps(x0, x0, 0x55); // 0076d2d6 shufps xmm0, xmm0, 0x55
    x1.u[0] = x86::bits(x86::add(x1.f(0), x0.f(0))); // 0076d2da addss xmm1, xmm0
    x0 = x1; // 0076d2de movaps xmm0, xmm1
    x0 = shufps(x0, x0, 0); // 0076d2e1 shufps xmm0, xmm0, 0
    x0 = sqrtps(x0); // 0076d2e5 sqrtps xmm0, xmm0
    { CMP_A = eax; CMP_B = uint32_t(0x64); } // 0076d2e8 cmp eax, 0x64
    if (CMP_A >= CMP_B) goto L_0076d359; // 0076d2eb jae 0x76d359
    edx = (eax + uint32_t(1)); // 0076d2ed lea edx, [eax + 1]
    g.u32((edi + uint32_t(0x1900)), edx); // 0076d2f0 mov dword ptr [edi + 0x1900], edx
    edx = g.u32((ebx + uint32_t(0x34))); // 0076d2f6 mov edx, dword ptr [ebx + 0x34]
    { const uint32_t r_ = eax << (uint32_t(6) & 31u); eax = r_; } // 0076d2f9 shl eax, 6
    { const uint32_t r_ = eax + edi; eax = r_; } // 0076d2fc add eax, edi
    g.u32((eax), edx); // 0076d2fe mov dword ptr [eax], edx
    edx = g.u32((esi + uint32_t(0x34))); // 0076d300 mov edx, dword ptr [esi + 0x34]
    g.u32((eax + uint32_t(4)), edx); // 0076d303 mov dword ptr [eax + 4], edx
    g.u32((eax + uint32_t(0x2c)), uint32_t(0)); // 0076d306 mov dword ptr [eax + 0x2c], 0
    g.u8((eax + uint32_t(0x20)), uint8_t(uint32_t(0))); // 0076d30d mov byte ptr [eax + 0x20], 0
    edx = g.u32((esp + uint32_t(0x6a0))); // 0076d311 mov edx, dword ptr [esp + 0x6a0]
    g.u32((eax + uint32_t(0x30)), edx); // 0076d318 mov dword ptr [eax + 0x30], edx
    edx = g.u32((esp + uint32_t(0x718))); // 0076d31b mov edx, dword ptr [esp + 0x718]
    g.u32((eax + uint32_t(0x34)), edx); // 0076d322 mov dword ptr [eax + 0x34], edx
    x1 = X{{g.u32((esp + uint32_t(0x100))), 0, 0, 0}}; // 0076d325 movss xmm1, dword ptr [esp + 0x100]
    g.u32((eax + uint32_t(0x10)), x1.u[0]); // 0076d32e movss dword ptr [eax + 0x10], xmm1
    x1 = X{{g.u32((esp + uint32_t(0x104))), 0, 0, 0}}; // 0076d333 movss xmm1, dword ptr [esp + 0x104]
    g.u32((eax + uint32_t(0x14)), x1.u[0]); // 0076d33c movss dword ptr [eax + 0x14], xmm1
    x1 = X{{g.u32((esp + uint32_t(0x108))), 0, 0, 0}}; // 0076d341 movss xmm1, dword ptr [esp + 0x108]
    g.u32((eax + uint32_t(0x18)), x1.u[0]); // 0076d34a movss dword ptr [eax + 0x18], xmm1
    g.u32((eax + uint32_t(0x24)), x3.u[0]); // 0076d34f movss dword ptr [eax + 0x24], xmm3
    g.u32((eax + uint32_t(0x28)), x0.u[0]); // 0076d354 movss dword ptr [eax + 0x28], xmm0
  L_0076d359:;
    { CMP_A = ((ecx >> 0) & 0xffu); CMP_B = ((ecx >> 0) & 0xffu); } // 0076d359 test cl, cl
    if ((CMP_A & CMP_B) == 0) goto L_0076d387; // 0076d35b je 0x76d387
  L_0076d35d:;
    eax = g.u32((edi + uint32_t(0x19ac))); // 0076d35d mov eax, dword ptr [edi + 0x19ac]
    edx = g.u32((eax + uint32_t(0x30))); // 0076d363 mov edx, dword ptr [eax + 0x30]
    ecx = g.u32((eax)); // 0076d366 mov ecx, dword ptr [eax]
    { CMP_A = ecx; CMP_B = g.u32((edx + uint32_t(0x400))); } // 0076d368 cmp ecx, dword ptr [edx + 0x400]
    if (int32_t(CMP_A) >= int32_t(CMP_B)) goto L_0076d379; // 0076d36e jge 0x76d379
    { const uint32_t r_ = ecx + 1u; ecx = r_; } // 0076d370 inc ecx
    g.u32((eax), ecx); // 0076d371 mov dword ptr [eax], ecx
    ecx = g.u32((edx + ecx * 4 - uint32_t(4))); // 0076d373 mov ecx, dword ptr [edx + ecx*4 - 4]
    goto L_0076d37c; // 0076d377 jmp 0x76d37c
  L_0076d379:;
    { const uint32_t r_ = ecx | uint32_t(0xffffffff); ecx = r_; } // 0076d379 or ecx, 0xffffffff
  L_0076d37c:;
    esi = ecx; // 0076d37c mov esi, ecx
    { CMP_A = ecx; CMP_B = uint32_t(-1); } // 0076d37e cmp ecx, -1
    if (CMP_A != CMP_B) goto L_0076ce70; // 0076d381 jne 0x76ce70
  L_0076d387:;
    eax = g.u32((esp + uint32_t(0x3c))); // 0076d387 mov eax, dword ptr [esp + 0x3c]
    { const uint32_t r_ = g.u32((esp + uint32_t(0x30))) + uint32_t(4); g.u32((esp + uint32_t(0x30)), r_); } // 0076d38b add dword ptr [esp + 0x30], 4
    { const uint32_t r_ = g.u32((esp + uint32_t(0x38))) + uint32_t(0x20); g.u32((esp + uint32_t(0x38)), r_); } // 0076d390 add dword ptr [esp + 0x38], 0x20
    { const uint32_t r_ = g.u32((esp + uint32_t(0x10))) + uint32_t(0x50); g.u32((esp + uint32_t(0x10)), r_); } // 0076d395 add dword ptr [esp + 0x10], 0x50
    { const uint32_t r_ = eax + 1u; eax = r_; } // 0076d39a inc eax
    { CMP_A = eax; CMP_B = g.u32((edi + uint32_t(0x19d4))); } // 0076d39b cmp eax, dword ptr [edi + 0x19d4]
    g.u32((esp + uint32_t(0x3c)), eax); // 0076d3a1 mov dword ptr [esp + 0x3c], eax
    if (int32_t(CMP_A) < int32_t(CMP_B)) goto L_0076cd80; // 0076d3a5 jl 0x76cd80
  L_0076d3ab:;
    ecx = g.u32((esp + uint32_t(0x7ac))); // 0076d3ab mov ecx, dword ptr [esp + 0x7ac]
    { const uint32_t v_ = g.u32(esp); esp += 4; esi = v_; } // 0076d3b2 pop esi
    { const uint32_t v_ = g.u32(esp); esp += 4; ebx = v_; } // 0076d3b3 pop ebx
    { const uint32_t r_ = ecx ^ esp; ecx = r_; } // 0076d3b4 xor ecx, esp
    LIT_CALL(0x8f9efe); // 0076d3b6 call 0x8f9efe
    esp = ebp; // 0076d3bb mov esp, ebp
    { const uint32_t v_ = g.u32(esp); esp += 4; ebp = v_; } // 0076d3bd pop ebp
    goto L_return; // 0076d3be ret 

    LIT_END(0x0)
}

#endif
