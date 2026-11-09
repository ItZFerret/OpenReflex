// World group: literal transliterations (world_lit.h) of small helpers whose scratch registers (EAX / ECX / EDX after the
// call) literal callers pass on: exact by construction. Readable versions: world_phys_small.cpp under WORLD_HAND_READABLE.
#include "../port.h"
#include "world_lit.h"

#ifndef WORLD_HAND_READABLE
using namespace world;
namespace {
constexpr uint32_t kLitOut = reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX | reflex_port::OUT_XMM_ALL;
}

// 00768460 DynObj_SetPose: sets a generic object's pose through its handle (stdcall handle, rot, pos, vel, angvel; ret 0x14), clears +0xb0 / +0xc0.
REFLEX_PORT_FN(0x00768460, DynObj_SetPose, "world", kLitOut) {
    LIT_BEGIN
    { const uint32_t v_ = ebp; esp -= 4; g.u32(esp, v_); } // 00768460 push ebp
    ebp = esp; // 00768461 mov ebp, esp
    { const uint32_t r_ = esp & uint32_t(0xfffffff0); esp = r_; } // 00768463 and esp, 0xfffffff0
    eax = g.u32((ebp + uint32_t(8))); // 00768466 mov eax, dword ptr [ebp + 8]
    eax = g.u32((eax + uint32_t(4))); // 00768469 mov eax, dword ptr [eax + 4]
    { CMP_A = eax; CMP_B = eax; } // 0076846c test eax, eax
    if ((CMP_A & CMP_B) == 0) goto L_007684b9; // 0076846e je 0x7684b9
    LIT_CALL(0x42bd90); // 00768470 call 0x42bd90
    ecx = g.u32((ebp + uint32_t(0xc))); // 00768475 mov ecx, dword ptr [ebp + 0xc]
    x0 = ld(g, (ecx)); // 00768478 movaps xmm0, xmmword ptr [ecx]
    edx = g.u32((ebp + uint32_t(0x10))); // 0076847b mov edx, dword ptr [ebp + 0x10]
    ecx = g.u32((ebp + uint32_t(0x14))); // 0076847e mov ecx, dword ptr [ebp + 0x14]
    st(g, (eax + uint32_t(0x40)), x0); // 00768481 movaps xmmword ptr [eax + 0x40], xmm0
    x0 = ld(g, (edx)); // 00768485 movaps xmm0, xmmword ptr [edx]
    edx = g.u32((ebp + uint32_t(0x18))); // 00768488 mov edx, dword ptr [ebp + 0x18]
    st(g, (eax + uint32_t(0x50)), x0); // 0076848b movaps xmmword ptr [eax + 0x50], xmm0
    x0 = ld(g, (ecx)); // 0076848f movaps xmm0, xmmword ptr [ecx]
    st(g, (eax + uint32_t(0x60)), x0); // 00768492 movaps xmmword ptr [eax + 0x60], xmm0
    x0 = ld(g, (edx)); // 00768496 movaps xmm0, xmmword ptr [edx]
    st(g, (eax + uint32_t(0x70)), x0); // 00768499 movaps xmmword ptr [eax + 0x70], xmm0
    x0 = ld(g, (uint32_t(0xd61670))); // 0076849d movaps xmm0, xmmword ptr [0xd61670]
    st(g, (eax + uint32_t(0xb0)), x0); // 007684a4 movaps xmmword ptr [eax + 0xb0], xmm0
    x0 = ld(g, (uint32_t(0xd61670))); // 007684ab movaps xmm0, xmmword ptr [0xd61670]
    st(g, (eax + uint32_t(0xc0)), x0); // 007684b2 movaps xmmword ptr [eax + 0xc0], xmm0
  L_007684b9:;
    esp = ebp; // 007684b9 mov esp, ebp
    { const uint32_t v_ = g.u32(esp); esp += 4; ebp = v_; } // 007684bb pop ebp
    goto L_return; // 007684bc ret 0x14

    LIT_END(0x14)
}

// 007684c0 DynObj_TakeContactResults: copies out and clears a generic object's +0x80 / +0x90 / +0xa0 (ESI, EDI = out, stdcall handle, out; ret 8).
REFLEX_PORT_FN(0x007684c0, DynObj_TakeContactResults, "world", kLitOut) {
    LIT_BEGIN
    { const uint32_t v_ = ebp; esp -= 4; g.u32(esp, v_); } // 007684c0 push ebp
    ebp = esp; // 007684c1 mov ebp, esp
    { const uint32_t r_ = esp & uint32_t(0xfffffff0); esp = r_; } // 007684c3 and esp, 0xfffffff0
    eax = g.u32((ebp + uint32_t(8))); // 007684c6 mov eax, dword ptr [ebp + 8]
    eax = g.u32((eax + uint32_t(4))); // 007684c9 mov eax, dword ptr [eax + 4]
    { const uint32_t r_ = esp - uint32_t(0xc); esp = r_; } // 007684cc sub esp, 0xc
    { const uint32_t v_ = ebx; esp -= 4; g.u32(esp, v_); } // 007684cf push ebx
    ebx = g.u32((ebp + uint32_t(0xc))); // 007684d0 mov ebx, dword ptr [ebp + 0xc]
    { CMP_A = eax; CMP_B = eax; } // 007684d3 test eax, eax
    if ((CMP_A & CMP_B) == 0) goto L_0076852b; // 007684d5 je 0x76852b
    LIT_CALL(0x42bd90); // 007684d7 call 0x42bd90
    x0 = ld(g, (eax + uint32_t(0x80))); // 007684dc movaps xmm0, xmmword ptr [eax + 0x80]
    st(g, (esi), x0); // 007684e3 movaps xmmword ptr [esi], xmm0
    x0 = ld(g, (eax + uint32_t(0x90))); // 007684e6 movaps xmm0, xmmword ptr [eax + 0x90]
    st(g, (edi), x0); // 007684ed movaps xmmword ptr [edi], xmm0
    x0 = ld(g, (eax + uint32_t(0xa0))); // 007684f0 movaps xmm0, xmmword ptr [eax + 0xa0]
    st(g, (ebx), x0); // 007684f7 movaps xmmword ptr [ebx], xmm0
    x0 = ld(g, (uint32_t(0xd61670))); // 007684fa movaps xmm0, xmmword ptr [0xd61670]
    st(g, (eax + uint32_t(0x80)), x0); // 00768501 movaps xmmword ptr [eax + 0x80], xmm0
    x0 = ld(g, (uint32_t(0xd61670))); // 00768508 movaps xmm0, xmmword ptr [0xd61670]
    st(g, (eax + uint32_t(0x90)), x0); // 0076850f movaps xmmword ptr [eax + 0x90], xmm0
    x0 = ld(g, (uint32_t(0xd61670))); // 00768516 movaps xmm0, xmmword ptr [0xd61670]
    st(g, (eax + uint32_t(0xa0)), x0); // 0076851d movaps xmmword ptr [eax + 0xa0], xmm0
    { const uint32_t v_ = g.u32(esp); esp += 4; ebx = v_; } // 00768524 pop ebx
    esp = ebp; // 00768525 mov esp, ebp
    { const uint32_t v_ = g.u32(esp); esp += 4; ebp = v_; } // 00768527 pop ebp
    goto L_return; // 00768528 ret 8
  L_0076852b:;
    x0 = ld(g, (uint32_t(0xd61670))); // 0076852b movaps xmm0, xmmword ptr [0xd61670]
    st(g, (esi), x0); // 00768532 movaps xmmword ptr [esi], xmm0
    x0 = ld(g, (uint32_t(0xd61670))); // 00768535 movaps xmm0, xmmword ptr [0xd61670]
    st(g, (edi), x0); // 0076853c movaps xmmword ptr [edi], xmm0
    x0 = ld(g, (uint32_t(0xd61670))); // 0076853f movaps xmm0, xmmword ptr [0xd61670]
    st(g, (ebx), x0); // 00768546 movaps xmmword ptr [ebx], xmm0
    { const uint32_t v_ = g.u32(esp); esp += 4; ebx = v_; } // 00768549 pop ebx
    esp = ebp; // 0076854a mov esp, ebp
    { const uint32_t v_ = g.u32(esp); esp += 4; ebp = v_; } // 0076854c pop ebp
    goto L_return; // 0076854d ret 8

    LIT_END(0x8)
}

// 00768550 PtrVector_PushIfRoom: push_back (008423d0, EDI = vector) unless full (EAX = vector, stdcall value; ret 4).
REFLEX_PORT_FN(0x00768550, PtrVector_PushIfRoom, "world", kLitOut) {
    LIT_BEGIN
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 00768550 push ecx
    { const uint32_t v_ = edi; esp -= 4; g.u32(esp, v_); } // 00768551 push edi
    edi = eax; // 00768552 mov edi, eax
    ecx = g.u32((edi + uint32_t(0xc))); // 00768554 mov ecx, dword ptr [edi + 0xc]
    { CMP_A = ecx; CMP_B = ecx; } // 00768557 test ecx, ecx
    if ((CMP_A & CMP_B) != 0) goto L_0076855f; // 00768559 jne 0x76855f
    eax = 0u; // 0076855b xor eax, eax
    goto L_00768567; // 0076855d jmp 0x768567
  L_0076855f:;
    eax = g.u32((edi + uint32_t(0x14))); // 0076855f mov eax, dword ptr [edi + 0x14]
    { const uint32_t r_ = eax - ecx; eax = r_; } // 00768562 sub eax, ecx
    { const uint32_t r_ = uint32_t(int32_t(eax) >> (uint32_t(2) & 31u)); eax = r_; } // 00768564 sar eax, 2
  L_00768567:;
    edx = g.u32((edi + uint32_t(0x10))); // 00768567 mov edx, dword ptr [edi + 0x10]
    { const uint32_t r_ = edx - ecx; edx = r_; } // 0076856a sub edx, ecx
    { const uint32_t r_ = uint32_t(int32_t(edx) >> (uint32_t(2) & 31u)); edx = r_; } // 0076856c sar edx, 2
    { CMP_A = edx; CMP_B = eax; } // 0076856f cmp edx, eax
    if (CMP_A == CMP_B) goto L_0076857d; // 00768571 je 0x76857d
    eax = (esp + uint32_t(0xc)); // 00768573 lea eax, [esp + 0xc]
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 00768577 push eax
    LIT_CALL(0x8423d0); // 00768578 call 0x8423d0
  L_0076857d:;
    { const uint32_t v_ = g.u32(esp); esp += 4; edi = v_; } // 0076857d pop edi
    { const uint32_t v_ = g.u32(esp); esp += 4; ecx = v_; } // 0076857e pop ecx
    goto L_return; // 0076857f ret 4

    LIT_END(0x4)
}

// 00768590 PtrVector_ClearValue: zeroes the entries equal to the argument (EDI = vector, stdcall value; ret 4).
REFLEX_PORT_FN(0x00768590, PtrVector_ClearValue, "world", kLitOut) {
    LIT_BEGIN
    { const uint32_t v_ = ebx; esp -= 4; g.u32(esp, v_); } // 00768590 push ebx
    ebx = g.u32((edi + uint32_t(0x10))); // 00768591 mov ebx, dword ptr [edi + 0x10]
    { const uint32_t r_ = ebx - g.u32((edi + uint32_t(0xc))); ebx = r_; } // 00768594 sub ebx, dword ptr [edi + 0xc]
    { const uint32_t v_ = ebp; esp -= 4; g.u32(esp, v_); } // 00768597 push ebp
    ebp = g.u32((esp + uint32_t(0xc))); // 00768598 mov ebp, dword ptr [esp + 0xc]
    { const uint32_t v_ = esi; esp -= 4; g.u32(esp, v_); } // 0076859c push esi
    { const uint32_t r_ = uint32_t(int32_t(ebx) >> (uint32_t(2) & 31u)); ebx = r_; } // 0076859d sar ebx, 2
    esi = 0u; // 007685a0 xor esi, esi
    { CMP_A = ebx; CMP_B = ebx; } // 007685a2 test ebx, ebx
    if (int32_t((CMP_A & CMP_B)) <= 0) goto L_007685e2; // 007685a4 jle 0x7685e2
  L_007685a6:;
    eax = g.u32((edi + uint32_t(0x10))); // 007685a6 mov eax, dword ptr [edi + 0x10]
    { const uint32_t r_ = eax - g.u32((edi + uint32_t(0xc))); eax = r_; } // 007685a9 sub eax, dword ptr [edi + 0xc]
    { const uint32_t r_ = uint32_t(int32_t(eax) >> (uint32_t(2) & 31u)); eax = r_; } // 007685ac sar eax, 2
    { CMP_A = esi; CMP_B = eax; } // 007685af cmp esi, eax
    if (CMP_A < CMP_B) goto L_007685b9; // 007685b1 jb 0x7685b9
    LIT_CALL(g.u32((uint32_t(0x916224)))); // 007685b3 call dword ptr [0x916224]
  L_007685b9:;
    eax = g.u32((edi + uint32_t(0xc))); // 007685b9 mov eax, dword ptr [edi + 0xc]
    { CMP_A = g.u32((eax + esi * 4)); CMP_B = ebp; } // 007685bc cmp dword ptr [eax + esi*4], ebp
    if (CMP_A != CMP_B) goto L_007685dd; // 007685bf jne 0x7685dd
    ecx = g.u32((edi + uint32_t(0x10))); // 007685c1 mov ecx, dword ptr [edi + 0x10]
    { const uint32_t r_ = ecx - eax; ecx = r_; } // 007685c4 sub ecx, eax
    { const uint32_t r_ = uint32_t(int32_t(ecx) >> (uint32_t(2) & 31u)); ecx = r_; } // 007685c6 sar ecx, 2
    { CMP_A = esi; CMP_B = ecx; } // 007685c9 cmp esi, ecx
    if (CMP_A < CMP_B) goto L_007685d3; // 007685cb jb 0x7685d3
    LIT_CALL(g.u32((uint32_t(0x916224)))); // 007685cd call dword ptr [0x916224]
  L_007685d3:;
    edx = g.u32((edi + uint32_t(0xc))); // 007685d3 mov edx, dword ptr [edi + 0xc]
    g.u32((edx + esi * 4), uint32_t(0)); // 007685d6 mov dword ptr [edx + esi*4], 0
  L_007685dd:;
    { const uint32_t r_ = esi + 1u; esi = r_; } // 007685dd inc esi
    { CMP_A = esi; CMP_B = ebx; } // 007685de cmp esi, ebx
    if (int32_t(CMP_A) < int32_t(CMP_B)) goto L_007685a6; // 007685e0 jl 0x7685a6
  L_007685e2:;
    { const uint32_t v_ = g.u32(esp); esp += 4; esi = v_; } // 007685e2 pop esi
    { const uint32_t v_ = g.u32(esp); esp += 4; ebp = v_; } // 007685e3 pop ebp
    { const uint32_t v_ = g.u32(esp); esp += 4; ebx = v_; } // 007685e4 pop ebx
    goto L_return; // 007685e5 ret 4

    LIT_END(0x4)
}

// 00776920 PtrRemove: std::remove over a checked pointer vector (cdecl result*, &value, first, last; EAX = result*).
REFLEX_PORT_FN(0x00776920, PtrRemove, "world", kLitOut) {
    LIT_BEGIN
    { const uint32_t r_ = esp - uint32_t(0xc); esp = r_; } // 00776920 sub esp, 0xc
    eax = g.u32((esp + uint32_t(0x1c))); // 00776923 mov eax, dword ptr [esp + 0x1c]
    edx = g.u32((esp + uint32_t(0x24))); // 00776927 mov edx, dword ptr [esp + 0x24]
    ecx = g.u32((esp + uint32_t(0x18))); // 0077692b mov ecx, dword ptr [esp + 0x18]
    { const uint32_t v_ = ebx; esp -= 4; g.u32(esp, v_); } // 0077692f push ebx
    ebx = g.u32((esp + uint32_t(0x14))); // 00776930 mov ebx, dword ptr [esp + 0x14]
    { const uint32_t v_ = ebp; esp -= 4; g.u32(esp, v_); } // 00776934 push ebp
    { const uint32_t v_ = esi; esp -= 4; g.u32(esp, v_); } // 00776935 push esi
    { const uint32_t v_ = edi; esp -= 4; g.u32(esp, v_); } // 00776936 push edi
    g.u32((esp + uint32_t(0x18)), eax); // 00776937 mov dword ptr [esp + 0x18], eax
    { CMP_A = eax; CMP_B = edx; } // 0077693b cmp eax, edx
    if (CMP_A == CMP_B) goto L_00776950; // 0077693d je 0x776950
    esi = g.u32((esp + uint32_t(0x24))); // 0077693f mov esi, dword ptr [esp + 0x24]
    esi = g.u32((esi)); // 00776943 mov esi, dword ptr [esi]
  L_00776945:;
    { CMP_A = g.u32((eax)); CMP_B = esi; } // 00776945 cmp dword ptr [eax], esi
    if (CMP_A == CMP_B) goto L_00776950; // 00776947 je 0x776950
    { const uint32_t r_ = eax + uint32_t(4); eax = r_; } // 00776949 add eax, 4
    { CMP_A = eax; CMP_B = edx; } // 0077694c cmp eax, edx
    if (CMP_A != CMP_B) goto L_00776945; // 0077694e jne 0x776945
  L_00776950:;
    ebp = g.u32((uint32_t(0x916224))); // 00776950 mov ebp, dword ptr [0x916224]
    g.u32((esp + uint32_t(0x28)), ecx); // 00776956 mov dword ptr [esp + 0x28], ecx
    g.u32((esp + uint32_t(0x2c)), eax); // 0077695a mov dword ptr [esp + 0x2c], eax
    { CMP_A = ecx; CMP_B = ecx; } // 0077695e test ecx, ecx
    if ((CMP_A & CMP_B) == 0) goto L_00776968; // 00776960 je 0x776968
    { CMP_A = ecx; CMP_B = g.u32((esp + uint32_t(0x30))); } // 00776962 cmp ecx, dword ptr [esp + 0x30]
    if (CMP_A == CMP_B) goto L_00776976; // 00776966 je 0x776976
  L_00776968:;
    LIT_CALL(ebp); // 00776968 call ebp
    edx = g.u32((esp + uint32_t(0x34))); // 0077696a mov edx, dword ptr [esp + 0x34]
    eax = g.u32((esp + uint32_t(0x2c))); // 0077696e mov eax, dword ptr [esp + 0x2c]
    ecx = g.u32((esp + uint32_t(0x28))); // 00776972 mov ecx, dword ptr [esp + 0x28]
  L_00776976:;
    { CMP_A = eax; CMP_B = edx; } // 00776976 cmp eax, edx
    if (CMP_A != CMP_B) goto L_00776989; // 00776978 jne 0x776989
    g.u32((ebx + uint32_t(4)), eax); // 0077697a mov dword ptr [ebx + 4], eax
    g.u32((ebx), ecx); // 0077697d mov dword ptr [ebx], ecx
    eax = ebx; // 0077697f mov eax, ebx
    { const uint32_t v_ = g.u32(esp); esp += 4; edi = v_; } // 00776981 pop edi
    { const uint32_t v_ = g.u32(esp); esp += 4; esi = v_; } // 00776982 pop esi
    { const uint32_t v_ = g.u32(esp); esp += 4; ebp = v_; } // 00776983 pop ebp
    { const uint32_t v_ = g.u32(esp); esp += 4; ebx = v_; } // 00776984 pop ebx
    { const uint32_t r_ = esp + uint32_t(0xc); esp = r_; } // 00776985 add esp, 0xc
    goto L_return; // 00776988 ret 
  L_00776989:;
    g.u32((esp + uint32_t(0x14)), ecx); // 00776989 mov dword ptr [esp + 0x14], ecx
    esi = eax; // 0077698d mov esi, eax
    { CMP_A = ecx; CMP_B = ecx; } // 0077698f test ecx, ecx
    if ((CMP_A & CMP_B) != 0) goto L_007769df; // 00776991 jne 0x7769df
    LIT_CALL(ebp); // 00776993 call ebp
    edx = g.u32((esp + uint32_t(0x34))); // 00776995 mov edx, dword ptr [esp + 0x34]
    eax = g.u32((esp + uint32_t(0x2c))); // 00776999 mov eax, dword ptr [esp + 0x2c]
    ecx = g.u32((esp + uint32_t(0x28))); // 0077699d mov ecx, dword ptr [esp + 0x28]
    edi = 0u; // 007769a1 xor edi, edi
  L_007769a3:;
    { CMP_A = esi; CMP_B = g.u32((edi + uint32_t(0x10))); } // 007769a3 cmp esi, dword ptr [edi + 0x10]
    if (CMP_A < CMP_B) goto L_007769b6; // 007769a6 jb 0x7769b6
    LIT_CALL(ebp); // 007769a8 call ebp
    edx = g.u32((esp + uint32_t(0x34))); // 007769aa mov edx, dword ptr [esp + 0x34]
    eax = g.u32((esp + uint32_t(0x2c))); // 007769ae mov eax, dword ptr [esp + 0x2c]
    ecx = g.u32((esp + uint32_t(0x28))); // 007769b2 mov ecx, dword ptr [esp + 0x28]
  L_007769b6:;
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 007769b6 push eax
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 007769b7 push ecx
    ecx = g.u32((esp + uint32_t(0x2c))); // 007769b8 mov ecx, dword ptr [esp + 0x2c]
    g.u8((esp + uint32_t(0x1c)), uint8_t(uint32_t(0))); // 007769bc mov byte ptr [esp + 0x1c], 0
    eax = g.u32((esp + uint32_t(0x1c))); // 007769c1 mov eax, dword ptr [esp + 0x1c]
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 007769c5 push eax
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 007769c6 push ecx
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 007769c7 push edx
    { const uint32_t r_ = esi + uint32_t(4); esi = r_; } // 007769c8 add esi, 4
    { const uint32_t v_ = esi; esp -= 4; g.u32(esp, v_); } // 007769cb push esi
    { const uint32_t v_ = ebx; esp -= 4; g.u32(esp, v_); } // 007769cc push ebx
    LIT_CALL(0x7774f0); // 007769cd call 0x7774f0
    { const uint32_t r_ = esp + uint32_t(0x1c); esp = r_; } // 007769d2 add esp, 0x1c
    { const uint32_t v_ = g.u32(esp); esp += 4; edi = v_; } // 007769d5 pop edi
    { const uint32_t v_ = g.u32(esp); esp += 4; esi = v_; } // 007769d6 pop esi
    { const uint32_t v_ = g.u32(esp); esp += 4; ebp = v_; } // 007769d7 pop ebp
    eax = ebx; // 007769d8 mov eax, ebx
    { const uint32_t v_ = g.u32(esp); esp += 4; ebx = v_; } // 007769da pop ebx
    { const uint32_t r_ = esp + uint32_t(0xc); esp = r_; } // 007769db add esp, 0xc
    goto L_return; // 007769de ret 
  L_007769df:;
    edi = g.u32((ecx)); // 007769df mov edi, dword ptr [ecx]
    goto L_007769a3; // 007769e1 jmp 0x7769a3

    LIT_END(0x0)
}

// 007774f0 PtrRemoveCopy: std::remove_copy over a checked pointer vector (cdecl result*, first, last, &value, -, dest; EAX = result*).
REFLEX_PORT_FN(0x007774f0, PtrRemoveCopy, "world", kLitOut) {
    LIT_BEGIN
    { const uint32_t v_ = ebx; esp -= 4; g.u32(esp, v_); } // 007774f0 push ebx
    ebx = g.u32((esp + uint32_t(0xc))); // 007774f1 mov ebx, dword ptr [esp + 0xc]
    { const uint32_t v_ = ebp; esp -= 4; g.u32(esp, v_); } // 007774f5 push ebp
    { const uint32_t v_ = esi; esp -= 4; g.u32(esp, v_); } // 007774f6 push esi
    { const uint32_t v_ = edi; esp -= 4; g.u32(esp, v_); } // 007774f7 push edi
    { CMP_A = ebx; CMP_B = g.u32((esp + uint32_t(0x1c))); } // 007774f8 cmp ebx, dword ptr [esp + 0x1c]
    if (CMP_A == CMP_B) goto L_00777558; // 007774fc je 0x777558
    ebp = g.u32((uint32_t(0x916224))); // 007774fe mov ebp, dword ptr [0x916224]
  L_00777504:;
    eax = g.u32((ebx)); // 00777504 mov eax, dword ptr [ebx]
    ecx = g.u32((esp + uint32_t(0x20))); // 00777506 mov ecx, dword ptr [esp + 0x20]
    { CMP_A = eax; CMP_B = g.u32((ecx)); } // 0077750a cmp eax, dword ptr [ecx]
    if (CMP_A == CMP_B) goto L_0077754f; // 0077750c je 0x77754f
    eax = g.u32((esp + uint32_t(0x28))); // 0077750e mov eax, dword ptr [esp + 0x28]
    edi = g.u32((esp + uint32_t(0x2c))); // 00777512 mov edi, dword ptr [esp + 0x2c]
    esi = eax; // 00777516 mov esi, eax
    { CMP_A = eax; CMP_B = eax; } // 00777518 test eax, eax
    if ((CMP_A & CMP_B) != 0) goto L_00777526; // 0077751a jne 0x777526
    LIT_CALL(ebp); // 0077751c call ebp
    eax = g.u32((esp + uint32_t(0x28))); // 0077751e mov eax, dword ptr [esp + 0x28]
    { CMP_A = eax; CMP_B = eax; } // 00777522 test eax, eax
    if ((CMP_A & CMP_B) == 0) goto L_0077752a; // 00777524 je 0x77752a
  L_00777526:;
    eax = g.u32((eax)); // 00777526 mov eax, dword ptr [eax]
    goto L_0077752c; // 00777528 jmp 0x77752c
  L_0077752a:;
    eax = 0u; // 0077752a xor eax, eax
  L_0077752c:;
    edx = g.u32((esp + uint32_t(0x2c))); // 0077752c mov edx, dword ptr [esp + 0x2c]
    { CMP_A = edx; CMP_B = g.u32((eax + uint32_t(0x10))); } // 00777530 cmp edx, dword ptr [eax + 0x10]
    if (CMP_A < CMP_B) goto L_00777537; // 00777533 jb 0x777537
    LIT_CALL(ebp); // 00777535 call ebp
  L_00777537:;
    { const uint32_t r_ = g.u32((esp + uint32_t(0x2c))) + uint32_t(4); g.u32((esp + uint32_t(0x2c)), r_); } // 00777537 add dword ptr [esp + 0x2c], 4
    { CMP_A = esi; CMP_B = esi; } // 0077753c test esi, esi
    if ((CMP_A & CMP_B) != 0) goto L_0077756e; // 0077753e jne 0x77756e
    LIT_CALL(ebp); // 00777540 call ebp
    eax = 0u; // 00777542 xor eax, eax
  L_00777544:;
    { CMP_A = edi; CMP_B = g.u32((eax + uint32_t(0x10))); } // 00777544 cmp edi, dword ptr [eax + 0x10]
    if (CMP_A < CMP_B) goto L_0077754b; // 00777547 jb 0x77754b
    LIT_CALL(ebp); // 00777549 call ebp
  L_0077754b:;
    eax = g.u32((ebx)); // 0077754b mov eax, dword ptr [ebx]
    g.u32((edi), eax); // 0077754d mov dword ptr [edi], eax
  L_0077754f:;
    { const uint32_t r_ = ebx + uint32_t(4); ebx = r_; } // 0077754f add ebx, 4
    { CMP_A = ebx; CMP_B = g.u32((esp + uint32_t(0x1c))); } // 00777552 cmp ebx, dword ptr [esp + 0x1c]
    if (CMP_A != CMP_B) goto L_00777504; // 00777556 jne 0x777504
  L_00777558:;
    eax = g.u32((esp + uint32_t(0x14))); // 00777558 mov eax, dword ptr [esp + 0x14]
    ecx = g.u32((esp + uint32_t(0x28))); // 0077755c mov ecx, dword ptr [esp + 0x28]
    edx = g.u32((esp + uint32_t(0x2c))); // 00777560 mov edx, dword ptr [esp + 0x2c]
    { const uint32_t v_ = g.u32(esp); esp += 4; edi = v_; } // 00777564 pop edi
    { const uint32_t v_ = g.u32(esp); esp += 4; esi = v_; } // 00777565 pop esi
    { const uint32_t v_ = g.u32(esp); esp += 4; ebp = v_; } // 00777566 pop ebp
    g.u32((eax), ecx); // 00777567 mov dword ptr [eax], ecx
    g.u32((eax + uint32_t(4)), edx); // 00777569 mov dword ptr [eax + 4], edx
    { const uint32_t v_ = g.u32(esp); esp += 4; ebx = v_; } // 0077756c pop ebx
    goto L_return; // 0077756d ret 
  L_0077756e:;
    eax = g.u32((esi)); // 0077756e mov eax, dword ptr [esi]
    goto L_00777544; // 00777570 jmp 0x777544

    LIT_END(0x0)
}

// 00777db0 BodySort_Insertion: insertion sort of body pointers by 007686b0 (cdecl first, last, comparator), memmove_s for runs.
REFLEX_PORT_FN(0x00777db0, BodySort_Insertion, "world", kLitOut) {
    LIT_BEGIN
    eax = g.u32((esp + uint32_t(8))); // 00777db0 mov eax, dword ptr [esp + 8]
    { const uint32_t r_ = esp - uint32_t(8); esp = r_; } // 00777db4 sub esp, 8
    { const uint32_t v_ = ebx; esp -= 4; g.u32(esp, v_); } // 00777db7 push ebx
    { const uint32_t v_ = ebp; esp -= 4; g.u32(esp, v_); } // 00777db8 push ebp
    { const uint32_t v_ = esi; esp -= 4; g.u32(esp, v_); } // 00777db9 push esi
    esi = g.u32((esp + uint32_t(0x18))); // 00777dba mov esi, dword ptr [esp + 0x18]
    { const uint32_t v_ = edi; esp -= 4; g.u32(esp, v_); } // 00777dbe push edi
    { CMP_A = esi; CMP_B = eax; } // 00777dbf cmp esi, eax
    if (CMP_A == CMP_B) goto L_00777e74; // 00777dc1 je 0x777e74
    ebp = (esi + uint32_t(4)); // 00777dc7 lea ebp, [esi + 4]
    { CMP_A = ebp; CMP_B = eax; } // 00777dca cmp ebp, eax
    if (CMP_A == CMP_B) goto L_00777e74; // 00777dcc je 0x777e74
    eax = uint32_t(4); // 00777dd2 mov eax, 4
    { const uint32_t r_ = eax - esi; eax = r_; } // 00777dd7 sub eax, esi
    g.u32((esp + uint32_t(0x14)), eax); // 00777dd9 mov dword ptr [esp + 0x14], eax
    ecx = (ecx); // 00777ddd lea ecx, [ecx]
  L_00777de0:;
    eax = g.u32((esi)); // 00777de0 mov eax, dword ptr [esi]
    edi = g.u32((ebp)); // 00777de2 mov edi, dword ptr [ebp]
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 00777de5 push eax
    ecx = (esp + uint32_t(0x28)); // 00777de6 lea ecx, [esp + 0x28]
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 00777dea push ecx
    edx = edi; // 00777deb mov edx, edi
    ebx = ebp; // 00777ded mov ebx, ebp
    g.u32((esp + uint32_t(0x18)), edi); // 00777def mov dword ptr [esp + 0x18], edi
    LIT_CALL(0x7686b0); // 00777df3 call 0x7686b0
    { CMP_A = ((eax >> 0) & 0xffu); CMP_B = ((eax >> 0) & 0xffu); } // 00777df8 test al, al
    if ((CMP_A & CMP_B) == 0) goto L_00777e27; // 00777dfa je 0x777e27
    edx = g.u32((esp + uint32_t(0x14))); // 00777dfc mov edx, dword ptr [esp + 0x14]
    eax = (edx + ebp - uint32_t(4)); // 00777e00 lea eax, [edx + ebp - 4]
    { const uint32_t r_ = uint32_t(int32_t(eax) >> (uint32_t(2) & 31u)); eax = r_; } // 00777e04 sar eax, 2
    { CMP_A = eax; CMP_B = eax; } // 00777e07 test eax, eax
    if (int32_t((CMP_A & CMP_B)) <= 0) goto L_00777e23; // 00777e09 jle 0x777e23
    { const uint32_t r_ = eax + eax; eax = r_; } // 00777e0b add eax, eax
    { const uint32_t r_ = eax + eax; eax = r_; } // 00777e0d add eax, eax
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 00777e0f push eax
    ecx = ebp; // 00777e10 mov ecx, ebp
    { const uint32_t v_ = esi; esp -= 4; g.u32(esp, v_); } // 00777e12 push esi
    { const uint32_t r_ = ecx - eax; ecx = r_; } // 00777e13 sub ecx, eax
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 00777e15 push eax
    { const uint32_t r_ = ecx + uint32_t(4); ecx = r_; } // 00777e16 add ecx, 4
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 00777e19 push ecx
    LIT_CALL(g.u32((uint32_t(0x916200)))); // 00777e1a call dword ptr [0x916200]
    { const uint32_t r_ = esp + uint32_t(0x10); esp = r_; } // 00777e20 add esp, 0x10
  L_00777e23:;
    g.u32((esi), edi); // 00777e23 mov dword ptr [esi], edi
    goto L_00777e67; // 00777e25 jmp 0x777e67
  L_00777e27:;
    eax = g.u32((ebp - uint32_t(4))); // 00777e27 mov eax, dword ptr [ebp - 4]
    esi = (ebp - uint32_t(4)); // 00777e2a lea esi, [ebp - 4]
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 00777e2d push eax
    edx = (esp + uint32_t(0x28)); // 00777e2e lea edx, [esp + 0x28]
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 00777e32 push edx
    edx = edi; // 00777e33 mov edx, edi
    LIT_CALL(0x7686b0); // 00777e35 call 0x7686b0
    { CMP_A = ((eax >> 0) & 0xffu); CMP_B = ((eax >> 0) & 0xffu); } // 00777e3a test al, al
    if ((CMP_A & CMP_B) == 0) goto L_00777e61; // 00777e3c je 0x777e61
    edi = g.u32((esi)); // 00777e3e mov edi, dword ptr [esi]
  L_00777e40:;
    edx = g.u32((esp + uint32_t(0x10))); // 00777e40 mov edx, dword ptr [esp + 0x10]
    g.u32((ebx), edi); // 00777e44 mov dword ptr [ebx], edi
    edi = g.u32((esi - uint32_t(4))); // 00777e46 mov edi, dword ptr [esi - 4]
    ebx = esi; // 00777e49 mov ebx, esi
    { const uint32_t r_ = esi - uint32_t(4); esi = r_; } // 00777e4b sub esi, 4
    { const uint32_t v_ = edi; esp -= 4; g.u32(esp, v_); } // 00777e4e push edi
    eax = (esp + uint32_t(0x28)); // 00777e4f lea eax, [esp + 0x28]
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 00777e53 push eax
    LIT_CALL(0x7686b0); // 00777e54 call 0x7686b0
    { CMP_A = ((eax >> 0) & 0xffu); CMP_B = ((eax >> 0) & 0xffu); } // 00777e59 test al, al
    if ((CMP_A & CMP_B) != 0) goto L_00777e40; // 00777e5b jne 0x777e40
    edi = g.u32((esp + uint32_t(0x10))); // 00777e5d mov edi, dword ptr [esp + 0x10]
  L_00777e61:;
    esi = g.u32((esp + uint32_t(0x1c))); // 00777e61 mov esi, dword ptr [esp + 0x1c]
    g.u32((ebx), edi); // 00777e65 mov dword ptr [ebx], edi
  L_00777e67:;
    { const uint32_t r_ = ebp + uint32_t(4); ebp = r_; } // 00777e67 add ebp, 4
    { CMP_A = ebp; CMP_B = g.u32((esp + uint32_t(0x20))); } // 00777e6a cmp ebp, dword ptr [esp + 0x20]
    if (CMP_A != CMP_B) goto L_00777de0; // 00777e6e jne 0x777de0
  L_00777e74:;
    { const uint32_t v_ = g.u32(esp); esp += 4; edi = v_; } // 00777e74 pop edi
    { const uint32_t v_ = g.u32(esp); esp += 4; esi = v_; } // 00777e75 pop esi
    { const uint32_t v_ = g.u32(esp); esp += 4; ebp = v_; } // 00777e76 pop ebp
    { const uint32_t v_ = g.u32(esp); esp += 4; ebx = v_; } // 00777e77 pop ebx
    { const uint32_t r_ = esp + uint32_t(8); esp = r_; } // 00777e78 add esp, 8
    goto L_return; // 00777e7b ret 

    LIT_END(0x0)
}

// 00776ef0 BodySort_Loop: the introsort loop of std::sort over body pointers (cdecl first, last, ideal, comparator).
REFLEX_PORT_FN(0x00776ef0, BodySort_Loop, "world", kLitOut) {
    LIT_BEGIN
    { const uint32_t r_ = esp - uint32_t(0xc); esp = r_; } // 00776ef0 sub esp, 0xc
    { const uint32_t v_ = ebx; esp -= 4; g.u32(esp, v_); } // 00776ef3 push ebx
    ebx = g.u32((esp + uint32_t(0x18))); // 00776ef4 mov ebx, dword ptr [esp + 0x18]
    { const uint32_t v_ = ebp; esp -= 4; g.u32(esp, v_); } // 00776ef8 push ebp
    { const uint32_t v_ = esi; esp -= 4; g.u32(esp, v_); } // 00776ef9 push esi
    { const uint32_t v_ = edi; esp -= 4; g.u32(esp, v_); } // 00776efa push edi
    edi = g.u32((esp + uint32_t(0x20))); // 00776efb mov edi, dword ptr [esp + 0x20]
    eax = ebx; // 00776eff mov eax, ebx
    { const uint32_t r_ = eax - edi; eax = r_; } // 00776f01 sub eax, edi
    { const uint32_t r_ = uint32_t(int32_t(eax) >> (uint32_t(2) & 31u)); eax = r_; } // 00776f03 sar eax, 2
    { CMP_A = eax; CMP_B = uint32_t(0x20); } // 00776f06 cmp eax, 0x20
    if (int32_t(CMP_A) <= int32_t(CMP_B)) goto L_00776f86; // 00776f09 jle 0x776f86
    esi = g.u32((esp + uint32_t(0x28))); // 00776f0b mov esi, dword ptr [esp + 0x28]
  L_00776f0f:;
    { CMP_A = esi; CMP_B = esi; } // 00776f0f test esi, esi
    if (int32_t((CMP_A & CMP_B)) <= 0) goto L_00776fa2; // 00776f11 jle 0x776fa2
    eax = g.u32((esp + uint32_t(0x2c))); // 00776f17 mov eax, dword ptr [esp + 0x2c]
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 00776f1b push eax
    { const uint32_t v_ = ebx; esp -= 4; g.u32(esp, v_); } // 00776f1c push ebx
    ecx = (esp + uint32_t(0x18)); // 00776f1d lea ecx, [esp + 0x18]
    { const uint32_t v_ = edi; esp -= 4; g.u32(esp, v_); } // 00776f21 push edi
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 00776f22 push ecx
    LIT_CALL(0x777580); // 00776f23 call 0x777580
    ebp = g.u32((esp + uint32_t(0x24))); // 00776f28 mov ebp, dword ptr [esp + 0x24]
    eax = esi; // 00776f2c mov eax, esi
    edx = int32_t(eax) < 0 ? 0xffffffffu : 0u; // 00776f2e cdq 
    { const uint32_t r_ = eax - edx; eax = r_; } // 00776f2f sub eax, edx
    { const uint32_t r_ = uint32_t(int32_t(eax) >> (uint32_t(1) & 31u)); eax = r_; } // 00776f31 sar eax, 1
    esi = eax; // 00776f33 mov esi, eax
    edx = int32_t(eax) < 0 ? 0xffffffffu : 0u; // 00776f35 cdq 
    { const uint32_t r_ = eax - edx; eax = r_; } // 00776f36 sub eax, edx
    { const uint32_t r_ = uint32_t(int32_t(eax) >> (uint32_t(1) & 31u)); eax = r_; } // 00776f38 sar eax, 1
    { const uint32_t r_ = esi + eax; esi = r_; } // 00776f3a add esi, eax
    eax = g.u32((esp + uint32_t(0x20))); // 00776f3c mov eax, dword ptr [esp + 0x20]
    edx = ebx; // 00776f40 mov edx, ebx
    ecx = eax; // 00776f42 mov ecx, eax
    { const uint32_t r_ = edx - ebp; edx = r_; } // 00776f44 sub edx, ebp
    { const uint32_t r_ = ecx - edi; ecx = r_; } // 00776f46 sub ecx, edi
    { const uint32_t r_ = edx & uint32_t(0xfffffffc); edx = r_; } // 00776f48 and edx, 0xfffffffc
    { const uint32_t r_ = ecx & uint32_t(0xfffffffc); ecx = r_; } // 00776f4b and ecx, 0xfffffffc
    { const uint32_t r_ = esp + uint32_t(0x10); esp = r_; } // 00776f4e add esp, 0x10
    { CMP_A = ecx; CMP_B = edx; } // 00776f51 cmp ecx, edx
    if (int32_t(CMP_A) >= int32_t(CMP_B)) goto L_00776f66; // 00776f53 jge 0x776f66
    edx = g.u32((esp + uint32_t(0x2c))); // 00776f55 mov edx, dword ptr [esp + 0x2c]
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 00776f59 push edx
    { const uint32_t v_ = esi; esp -= 4; g.u32(esp, v_); } // 00776f5a push esi
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 00776f5b push eax
    { const uint32_t v_ = edi; esp -= 4; g.u32(esp, v_); } // 00776f5c push edi
    LIT_CALL(0x776ef0); // 00776f5d call 0x776ef0
    edi = ebp; // 00776f62 mov edi, ebp
    goto L_00776f77; // 00776f64 jmp 0x776f77
  L_00776f66:;
    eax = g.u32((esp + uint32_t(0x2c))); // 00776f66 mov eax, dword ptr [esp + 0x2c]
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 00776f6a push eax
    { const uint32_t v_ = esi; esp -= 4; g.u32(esp, v_); } // 00776f6b push esi
    { const uint32_t v_ = ebx; esp -= 4; g.u32(esp, v_); } // 00776f6c push ebx
    { const uint32_t v_ = ebp; esp -= 4; g.u32(esp, v_); } // 00776f6d push ebp
    LIT_CALL(0x776ef0); // 00776f6e call 0x776ef0
    ebx = g.u32((esp + uint32_t(0x20))); // 00776f73 mov ebx, dword ptr [esp + 0x20]
  L_00776f77:;
    eax = ebx; // 00776f77 mov eax, ebx
    { const uint32_t r_ = eax - edi; eax = r_; } // 00776f79 sub eax, edi
    { const uint32_t r_ = uint32_t(int32_t(eax) >> (uint32_t(2) & 31u)); eax = r_; } // 00776f7b sar eax, 2
    { const uint32_t r_ = esp + uint32_t(0x10); esp = r_; } // 00776f7e add esp, 0x10
    { CMP_A = eax; CMP_B = uint32_t(0x20); } // 00776f81 cmp eax, 0x20
    if (int32_t(CMP_A) > int32_t(CMP_B)) goto L_00776f0f; // 00776f84 jg 0x776f0f
  L_00776f86:;
    { CMP_A = eax; CMP_B = uint32_t(1); } // 00776f86 cmp eax, 1
    if (int32_t(CMP_A) <= int32_t(CMP_B)) goto L_00776f9a; // 00776f89 jle 0x776f9a
    ecx = g.u32((esp + uint32_t(0x2c))); // 00776f8b mov ecx, dword ptr [esp + 0x2c]
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 00776f8f push ecx
    { const uint32_t v_ = ebx; esp -= 4; g.u32(esp, v_); } // 00776f90 push ebx
    { const uint32_t v_ = edi; esp -= 4; g.u32(esp, v_); } // 00776f91 push edi
    LIT_CALL(0x777db0); // 00776f92 call 0x777db0
    { const uint32_t r_ = esp + uint32_t(0xc); esp = r_; } // 00776f97 add esp, 0xc
  L_00776f9a:;
    { const uint32_t v_ = g.u32(esp); esp += 4; edi = v_; } // 00776f9a pop edi
    { const uint32_t v_ = g.u32(esp); esp += 4; esi = v_; } // 00776f9b pop esi
    { const uint32_t v_ = g.u32(esp); esp += 4; ebp = v_; } // 00776f9c pop ebp
    { const uint32_t v_ = g.u32(esp); esp += 4; ebx = v_; } // 00776f9d pop ebx
    { const uint32_t r_ = esp + uint32_t(0xc); esp = r_; } // 00776f9e add esp, 0xc
    goto L_return; // 00776fa1 ret 
  L_00776fa2:;
    { CMP_A = eax; CMP_B = uint32_t(0x20); } // 00776fa2 cmp eax, 0x20
    if (int32_t(CMP_A) <= int32_t(CMP_B)) goto L_00776f86; // 00776fa5 jle 0x776f86
    ecx = ebx; // 00776fa7 mov ecx, ebx
    { const uint32_t r_ = ecx - edi; ecx = r_; } // 00776fa9 sub ecx, edi
    { const uint32_t r_ = ecx & uint32_t(0xfffffffc); ecx = r_; } // 00776fab and ecx, 0xfffffffc
    { CMP_A = ecx; CMP_B = uint32_t(4); } // 00776fae cmp ecx, 4
    if (int32_t(CMP_A) <= int32_t(CMP_B)) goto L_00776fc2; // 00776fb1 jle 0x776fc2
    edx = g.u32((esp + uint32_t(0x2c))); // 00776fb3 mov edx, dword ptr [esp + 0x2c]
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 00776fb7 push edx
    eax = ebx; // 00776fb8 mov eax, ebx
    LIT_CALL(0x777d20); // 00776fba call 0x777d20
    { const uint32_t r_ = esp + uint32_t(4); esp = r_; } // 00776fbf add esp, 4
  L_00776fc2:;
    eax = g.u32((esp + uint32_t(0x2c))); // 00776fc2 mov eax, dword ptr [esp + 0x2c]
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 00776fc6 push eax
    eax = ebx; // 00776fc7 mov eax, ebx
    LIT_CALL(0x777d60); // 00776fc9 call 0x777d60
    { const uint32_t r_ = esp + uint32_t(4); esp = r_; } // 00776fce add esp, 4
    { const uint32_t v_ = g.u32(esp); esp += 4; edi = v_; } // 00776fd1 pop edi
    { const uint32_t v_ = g.u32(esp); esp += 4; esi = v_; } // 00776fd2 pop esi
    { const uint32_t v_ = g.u32(esp); esp += 4; ebp = v_; } // 00776fd3 pop ebp
    { const uint32_t v_ = g.u32(esp); esp += 4; ebx = v_; } // 00776fd4 pop ebx
    { const uint32_t r_ = esp + uint32_t(0xc); esp = r_; } // 00776fd5 add esp, 0xc
    goto L_return; // 00776fd8 ret 

    LIT_END(0x0)
}

// 00770e80 Vehicle_CollectNearbyStatics: the static objects near a vehicle into its 0x50-byte record list (EDI = list, ECX = query box, stdcall world, box; ret 8); the vehicle vf62 callers read EAX / EDX afterwards.
REFLEX_PORT_FN(0x00770e80, Vehicle_CollectNearbyStatics, "world", kLitOut) {
    LIT_BEGIN
    { const uint32_t v_ = ebp; esp -= 4; g.u32(esp, v_); } // 00770e80 push ebp
    ebp = esp; // 00770e81 mov ebp, esp
    { const uint32_t r_ = esp & uint32_t(0xfffffff0); esp = r_; } // 00770e83 and esp, 0xfffffff0
    { const uint32_t r_ = esp - uint32_t(0x78); esp = r_; } // 00770e86 sub esp, 0x78
    eax = g.u32((uint32_t(0x9b45e4))); // 00770e89 mov eax, dword ptr [0x9b45e4]
    { const uint32_t r_ = eax ^ esp; eax = r_; } // 00770e8e xor eax, esp
    g.u32((esp + uint32_t(0x74)), eax); // 00770e90 mov dword ptr [esp + 0x74], eax
    eax = g.u32((ebp + uint32_t(0xc))); // 00770e94 mov eax, dword ptr [ebp + 0xc]
    { const uint32_t v_ = ebx; esp -= 4; g.u32(esp, v_); } // 00770e97 push ebx
    ebx = g.u32((ebp + uint32_t(8))); // 00770e98 mov ebx, dword ptr [ebp + 8]
    { const uint32_t v_ = esi; esp -= 4; g.u32(esp, v_); } // 00770e9b push esi
    esi = g.u32((edi + uint32_t(0x10))); // 00770e9c mov esi, dword ptr [edi + 0x10]
    g.u32((esp + uint32_t(0x10)), eax); // 00770e9f mov dword ptr [esp + 0x10], eax
    g.u32((esp + uint32_t(0x14)), ecx); // 00770ea3 mov dword ptr [esp + 0x14], ecx
    { CMP_A = g.u32((edi + uint32_t(0xc))); CMP_B = esi; } // 00770ea7 cmp dword ptr [edi + 0xc], esi
    if (CMP_A <= CMP_B) goto L_00770eb2; // 00770eaa jbe 0x770eb2
    LIT_CALL(g.u32((uint32_t(0x916224)))); // 00770eac call dword ptr [0x916224]
  L_00770eb2:;
    ecx = g.u32((edi + uint32_t(0xc))); // 00770eb2 mov ecx, dword ptr [edi + 0xc]
    edx = g.u32((edi)); // 00770eb5 mov edx, dword ptr [edi]
    g.u32((esp + uint32_t(0x18)), edx); // 00770eb7 mov dword ptr [esp + 0x18], edx
    g.u32((esp + uint32_t(0xc)), ecx); // 00770ebb mov dword ptr [esp + 0xc], ecx
    { CMP_A = ecx; CMP_B = g.u32((edi + uint32_t(0x10))); } // 00770ebf cmp ecx, dword ptr [edi + 0x10]
    if (CMP_A <= CMP_B) goto L_00770ece; // 00770ec2 jbe 0x770ece
    LIT_CALL(g.u32((uint32_t(0x916224)))); // 00770ec4 call dword ptr [0x916224]
    ecx = g.u32((esp + uint32_t(0xc))); // 00770eca mov ecx, dword ptr [esp + 0xc]
  L_00770ece:;
    edx = g.u32((esp + uint32_t(0x18))); // 00770ece mov edx, dword ptr [esp + 0x18]
    eax = g.u32((edi)); // 00770ed2 mov eax, dword ptr [edi]
    { const uint32_t v_ = esi; esp -= 4; g.u32(esp, v_); } // 00770ed4 push esi
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 00770ed5 push edx
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 00770ed6 push ecx
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 00770ed7 push eax
    eax = (esp + uint32_t(0x28)); // 00770ed8 lea eax, [esp + 0x28]
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 00770edc push eax
    esi = edi; // 00770edd mov esi, edi
    LIT_CALL(0x4db5b0); // 00770edf call 0x4db5b0
    eax = g.u32((ebx + uint32_t(0x19cc))); // 00770ee4 mov eax, dword ptr [ebx + 0x19cc]
    ecx = g.u32((esp + uint32_t(0x14))); // 00770eea mov ecx, dword ptr [esp + 0x14]
    edx = g.u32((esp + uint32_t(0x10))); // 00770eee mov edx, dword ptr [esp + 0x10]
    esi = g.u32((eax + uint32_t(0x30))); // 00770ef2 mov esi, dword ptr [eax + 0x30]
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 00770ef5 push ecx
    g.u32((esp + uint32_t(0x10)), eax); // 00770ef6 mov dword ptr [esp + 0x10], eax
    LIT_CALL(0x883020); // 00770efa call 0x883020
    edx = g.u32((esp + uint32_t(0xc))); // 00770eff mov edx, dword ptr [esp + 0xc]
    g.u32((edx), uint32_t(0)); // 00770f03 mov dword ptr [edx], 0
    esp = (esp); // 00770f09 lea esp, [esp]
  L_00770f10:;
    ecx = g.u32((ebx + uint32_t(0x19cc))); // 00770f10 mov ecx, dword ptr [ebx + 0x19cc]
    edx = g.u32((ecx + uint32_t(0x30))); // 00770f16 mov edx, dword ptr [ecx + 0x30]
    eax = g.u32((ecx)); // 00770f19 mov eax, dword ptr [ecx]
    { CMP_A = eax; CMP_B = g.u32((edx + uint32_t(0x400))); } // 00770f1b cmp eax, dword ptr [edx + 0x400]
    if (int32_t(CMP_A) >= int32_t(CMP_B)) goto L_00770ff1; // 00770f21 jge 0x770ff1
    { const uint32_t r_ = eax + 1u; eax = r_; } // 00770f27 inc eax
    g.u32((ecx), eax); // 00770f28 mov dword ptr [ecx], eax
    esi = g.u32((edx + eax * 4 - uint32_t(4))); // 00770f2a mov esi, dword ptr [edx + eax*4 - 4]
    { CMP_A = esi; CMP_B = uint32_t(-1); } // 00770f2e cmp esi, -1
    if (CMP_A == CMP_B) goto L_00770ff1; // 00770f31 je 0x770ff1
    eax = g.u32((ebx + uint32_t(0x19c4))); // 00770f37 mov eax, dword ptr [ebx + 0x19c4]
    { const uint32_t r_ = eax - g.u32((ebx + uint32_t(0x19c0))); eax = r_; } // 00770f3d sub eax, dword ptr [ebx + 0x19c0]
    { const uint32_t r_ = uint32_t(int32_t(eax) >> (uint32_t(6) & 31u)); eax = r_; } // 00770f43 sar eax, 6
    { CMP_A = esi; CMP_B = eax; } // 00770f46 cmp esi, eax
    if (CMP_A < CMP_B) goto L_00770f50; // 00770f48 jb 0x770f50
    LIT_CALL(g.u32((uint32_t(0x916224)))); // 00770f4a call dword ptr [0x916224]
  L_00770f50:;
    eax = esi; // 00770f50 mov eax, esi
    { const uint32_t r_ = eax << (uint32_t(6) & 31u); eax = r_; } // 00770f52 shl eax, 6
    { const uint32_t r_ = eax + g.u32((ebx + uint32_t(0x19c0))); eax = r_; } // 00770f55 add eax, dword ptr [ebx + 0x19c0]
    { CMP_A = uint32_t(g.u8((eax + uint32_t(0x38)))); CMP_B = uint32_t(0); } // 00770f5b cmp byte ptr [eax + 0x38], 0
    if (CMP_A == CMP_B) goto L_00770f10; // 00770f5f je 0x770f10
    ecx = g.u32((eax + uint32_t(0x28))); // 00770f61 mov ecx, dword ptr [eax + 0x28]
    edx = g.u32((ecx + uint32_t(0x10))); // 00770f64 mov edx, dword ptr [ecx + 0x10]
    { const uint32_t r_ = edx >> (uint32_t(2) & 31u); edx = r_; } // 00770f67 shr edx, 2
    { CMP_A = ((edx >> 0) & 0xffu); CMP_B = uint32_t(1); } // 00770f6a test dl, 1
    if ((CMP_A & CMP_B) == 0) goto L_00770f10; // 00770f6d je 0x770f10
    x0 = ld(g, (eax)); // 00770f6f movaps xmm0, xmmword ptr [eax]
    ecx = g.u32((ebx + uint32_t(0x19cc))); // 00770f72 mov ecx, dword ptr [ebx + 0x19cc]
    ecx = g.u32((ecx + uint32_t(0x30))); // 00770f78 mov ecx, dword ptr [ecx + 0x30]
    edx = g.u32((ecx + uint32_t(0x41c))); // 00770f7b mov edx, dword ptr [ecx + 0x41c]
    ecx = g.u32((ecx + uint32_t(0x420))); // 00770f81 mov ecx, dword ptr [ecx + 0x420]
    st(g, (esp + uint32_t(0x20)), x0); // 00770f87 movaps xmmword ptr [esp + 0x20], xmm0
    x0 = ld(g, (eax + uint32_t(0x10))); // 00770f8c movaps xmm0, xmmword ptr [eax + 0x10]
    st(g, (esp + uint32_t(0x30)), x0); // 00770f90 movaps xmmword ptr [esp + 0x30], xmm0
    { const uint32_t r_ = esi << (uint32_t(4) & 31u); esi = r_; } // 00770f95 shl esi, 4
    x0 = ld(g, (edx + esi)); // 00770f98 movaps xmm0, xmmword ptr [edx + esi]
    edx = g.u32((eax + uint32_t(0x30))); // 00770f9c mov edx, dword ptr [eax + 0x30]
    st(g, (esp + uint32_t(0x40)), x0); // 00770f9f movaps xmmword ptr [esp + 0x40], xmm0
    x0 = ld(g, (ecx + esi)); // 00770fa4 movaps xmm0, xmmword ptr [ecx + esi]
    { const uint32_t r_ = eax + uint32_t(0x24); eax = r_; } // 00770fa8 add eax, 0x24
    ecx = edi; // 00770fab mov ecx, edi
    st(g, (esp + uint32_t(0x50)), x0); // 00770fad movaps xmmword ptr [esp + 0x50], xmm0
    g.u32((esp + uint32_t(0x60)), edx); // 00770fb2 mov dword ptr [esp + 0x60], edx
    g.u32((esp + uint32_t(0x64)), eax); // 00770fb6 mov dword ptr [esp + 0x64], eax
    LIT_CALL(0x4db570); // 00770fba call 0x4db570
    ecx = g.u32((edi + uint32_t(0x10))); // 00770fbf mov ecx, dword ptr [edi + 0x10]
    { const uint32_t r_ = ecx - g.u32((edi + uint32_t(0xc))); ecx = r_; } // 00770fc2 sub ecx, dword ptr [edi + 0xc]
    esi = eax; // 00770fc5 mov esi, eax
    eax = uint32_t(0x66666667); // 00770fc7 mov eax, 0x66666667
    { const int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); } // 00770fcc imul ecx
    { const uint32_t r_ = uint32_t(int32_t(edx) >> (uint32_t(5) & 31u)); edx = r_; } // 00770fce sar edx, 5
    eax = edx; // 00770fd1 mov eax, edx
    { const uint32_t r_ = eax >> (uint32_t(0x1f) & 31u); eax = r_; } // 00770fd3 shr eax, 0x1f
    { const uint32_t r_ = eax + edx; eax = r_; } // 00770fd6 add eax, edx
    { CMP_A = eax; CMP_B = esi; } // 00770fd8 cmp eax, esi
    if (CMP_A >= CMP_B) goto L_00770f10; // 00770fda jae 0x770f10
    ecx = (esp + uint32_t(0x20)); // 00770fe0 lea ecx, [esp + 0x20]
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 00770fe4 push ecx
    eax = edi; // 00770fe5 mov eax, edi
    LIT_CALL(0x773d10); // 00770fe7 call 0x773d10
    goto L_00770f10; // 00770fec jmp 0x770f10
  L_00770ff1:;
    ecx = g.u32((esp + uint32_t(0x7c))); // 00770ff1 mov ecx, dword ptr [esp + 0x7c]
    { const uint32_t v_ = g.u32(esp); esp += 4; esi = v_; } // 00770ff5 pop esi
    { const uint32_t v_ = g.u32(esp); esp += 4; ebx = v_; } // 00770ff6 pop ebx
    { const uint32_t r_ = ecx ^ esp; ecx = r_; } // 00770ff7 xor ecx, esp
    LIT_CALL(0x8f9efe); // 00770ff9 call 0x8f9efe
    esp = ebp; // 00770ffe mov esp, ebp
    { const uint32_t v_ = g.u32(esp); esp += 4; ebp = v_; } // 00771000 pop ebp
    goto L_return; // 00771001 ret 8

    LIT_END(0x8)
}

// 007729f0 World_QueryShapeContacts: what a shape at a position touches (ECX = types[10], EDX = shape, stdcall world, pos, results, ids; ret 0x10; EAX = count): see the hand port in world_phys.cpp.
REFLEX_PORT_FN(0x007729f0, World_QueryShapeContacts, "world", kLitOut) {
    LIT_BEGIN
    { const uint32_t v_ = ebp; esp -= 4; g.u32(esp, v_); } // 007729f0 push ebp
    ebp = esp; // 007729f1 mov ebp, esp
    { const uint32_t r_ = esp & uint32_t(0xfffffff0); esp = r_; } // 007729f3 and esp, 0xfffffff0
    { const uint32_t r_ = esp - uint32_t(0x724); esp = r_; } // 007729f6 sub esp, 0x724
    eax = g.u32((uint32_t(0x9b45e4))); // 007729fc mov eax, dword ptr [0x9b45e4]
    { const uint32_t r_ = eax ^ esp; eax = r_; } // 00772a01 xor eax, esp
    g.u32((esp + uint32_t(0x720)), eax); // 00772a03 mov dword ptr [esp + 0x720], eax
    eax = g.u32((ebp + uint32_t(0x10))); // 00772a0a mov eax, dword ptr [ebp + 0x10]
    x0 = X{{g.u32((uint32_t(0x977b88))), 0, 0, 0}}; // 00772a0d movss xmm0, dword ptr [0x977b88]
    x1 = X{{g.u32((uint32_t(0x977d60))), 0, 0, 0}}; // 00772a15 movss xmm1, dword ptr [0x977d60]
    { const uint32_t v_ = ebx; esp -= 4; g.u32(esp, v_); } // 00772a1d push ebx
    ebx = g.u32((ebp + uint32_t(8))); // 00772a1e mov ebx, dword ptr [ebp + 8]
    { const uint32_t v_ = esi; esp -= 4; g.u32(esp, v_); } // 00772a21 push esi
    esi = g.u32((ebp + uint32_t(0xc))); // 00772a22 mov esi, dword ptr [ebp + 0xc]
    g.u32((esp + uint32_t(0x18)), ecx); // 00772a25 mov dword ptr [esp + 0x18], ecx
    ecx = g.u32((ebp + uint32_t(0x14))); // 00772a29 mov ecx, dword ptr [ebp + 0x14]
    { const uint32_t v_ = edi; esp -= 4; g.u32(esp, v_); } // 00772a2c push edi
    edi = 0u; // 00772a2d xor edi, edi
    g.u32((esp + uint32_t(0x28)), eax); // 00772a2f mov dword ptr [esp + 0x28], eax
    g.u32((esp + uint32_t(0x18)), ecx); // 00772a33 mov dword ptr [esp + 0x18], ecx
    g.u32((esp + uint32_t(0x24)), ebx); // 00772a37 mov dword ptr [esp + 0x24], ebx
    g.u32((esp + uint32_t(0x2c)), esi); // 00772a3b mov dword ptr [esp + 0x2c], esi
    g.u32((esp + uint32_t(0x20)), edx); // 00772a3f mov dword ptr [esp + 0x20], edx
    x0 = shufps(x0, x0, 0); // 00772a43 shufps xmm0, xmm0, 0
    x1 = shufps(x1, x1, 0); // 00772a47 shufps xmm1, xmm1, 0
    { const uint32_t r_ = eax + uint32_t(0x20); eax = r_; } // 00772a4b add eax, 0x20
    ecx = (edi + uint32_t(0xa)); // 00772a4e lea ecx, [edi + 0xa]
  L_00772a51:;
    st(g, (eax - uint32_t(0x20)), x0); // 00772a51 movaps xmmword ptr [eax - 0x20], xmm0
    x2 = ld(g, (uint32_t(0xd61670))); // 00772a55 movaps xmm2, xmmword ptr [0xd61670]
    st(g, (eax - uint32_t(0x10)), x2); // 00772a5c movaps xmmword ptr [eax - 0x10], xmm2
    st(g, (eax), x1); // 00772a60 movaps xmmword ptr [eax], xmm1
    { const uint32_t r_ = eax + uint32_t(0x30); eax = r_; } // 00772a63 add eax, 0x30
    { const uint32_t r_ = ecx - uint32_t(1); ecx = r_; } // 00772a66 sub ecx, 1
    if (ecx != 0) goto L_00772a51; // 00772a69 jne 0x772a51
    eax = 0u; // 00772a6b xor eax, eax
    ecx = (ecx); // 00772a6d lea ecx, [ecx]
  L_00772a70:;
    { const uint32_t r_ = ecx | uint32_t(0xffffffff); ecx = r_; } // 00772a70 or ecx, 0xffffffff
    g.u32((esp + eax * 4 + uint32_t(0x620)), ecx); // 00772a73 mov dword ptr [esp + eax*4 + 0x620], ecx
    g.u32((esp + eax * 4 + uint32_t(0x698)), ecx); // 00772a7a mov dword ptr [esp + eax*4 + 0x698], ecx
    { const uint32_t r_ = eax + 1u; eax = r_; } // 00772a81 inc eax
    { CMP_A = eax; CMP_B = uint32_t(0x1e); } // 00772a82 cmp eax, 0x1e
    if (int32_t(CMP_A) < int32_t(CMP_B)) goto L_00772a70; // 00772a85 jl 0x772a70
    eax = g.u32((edx)); // 00772a87 mov eax, dword ptr [edx]
    ecx = (esp + uint32_t(0x60)); // 00772a89 lea ecx, [esp + 0x60]
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 00772a8d push ecx
    ecx = (esp + uint32_t(0x54)); // 00772a8e lea ecx, [esp + 0x54]
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 00772a92 push ecx
    { const uint32_t v_ = esi; esp -= 4; g.u32(esp, v_); } // 00772a93 push esi
    ecx = edx; // 00772a94 mov ecx, edx
    edx = g.u32((eax + uint32_t(8))); // 00772a96 mov edx, dword ptr [eax + 8]
    { const uint32_t v_ = uint32_t(0xd61640); esp -= 4; g.u32(esp, v_); } // 00772a99 push 0xd61640
    LIT_CALL(edx); // 00772a9e call edx
    { CMP_A = uint32_t(g.u8((uint32_t(0x9e30e0)))); CMP_B = uint32_t(1); } // 00772aa0 test byte ptr [0x9e30e0], 1
    if ((CMP_A & CMP_B) != 0) goto L_00772ab6; // 00772aa7 jne 0x772ab6
    { const uint32_t r_ = g.u32((uint32_t(0x9e30e0))) | uint32_t(1); g.u32((uint32_t(0x9e30e0)), r_); } // 00772aa9 or dword ptr [0x9e30e0], 1
    g.u32((uint32_t(0x9e30dc)), edi); // 00772ab0 mov dword ptr [0x9e30dc], edi
  L_00772ab6:;
    edx = uint32_t(0x9e30d4); // 00772ab6 mov edx, 0x9e30d4
    LIT_CALL(0x402dc0); // 00772abb call 0x402dc0
    x0 = X{{g.u32((esi)), 0, 0, 0}}; // 00772ac0 movss xmm0, dword ptr [esi]
    g.u32((esp + uint32_t(0x30)), x0.u[0]); // 00772ac4 movss dword ptr [esp + 0x30], xmm0
    x0 = X{{g.u32((esi + uint32_t(4))), 0, 0, 0}}; // 00772aca movss xmm0, dword ptr [esi + 4]
    g.u32((esp + uint32_t(0x34)), x0.u[0]); // 00772acf movss dword ptr [esp + 0x34], xmm0
    x0 = X{{g.u32((esi + uint32_t(8))), 0, 0, 0}}; // 00772ad5 movss xmm0, dword ptr [esi + 8]
    g.u32((esp + uint32_t(0x10)), eax); // 00772ada mov dword ptr [esp + 0x10], eax
    g.u32((esp + uint32_t(0x38)), x0.u[0]); // 00772ade movss dword ptr [esp + 0x38], xmm0
    { CMP_A = eax; CMP_B = eax; } // 00772ae4 test eax, eax
    if ((CMP_A & CMP_B) == 0) goto L_00772b82; // 00772ae6 je 0x772b82
    ecx = (esp + uint32_t(0x30)); // 00772aec lea ecx, [esp + 0x30]
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 00772af0 push ecx
    ecx = eax; // 00772af1 mov ecx, eax
    LIT_CALL(0x7c5950); // 00772af3 call 0x7c5950
    { CMP_A = eax; CMP_B = eax; } // 00772af8 test eax, eax
    if ((CMP_A & CMP_B) == 0) goto L_00772b82; // 00772afa je 0x772b82
    x1 = ld(g, (uint32_t(0xd616a0))); // 00772b00 movaps xmm1, xmmword ptr [0xd616a0]
    x0 = ld(g, (esi)); // 00772b07 movaps xmm0, xmmword ptr [esi]
    eax = g.u32((esp + uint32_t(0x20))); // 00772b0a mov eax, dword ptr [esp + 0x20]
    x0 = mulps(x0, x1); // 00772b0e mulps xmm0, xmm1
    x1 = x0; // 00772b11 movaps xmm1, xmm0
    x1 = shufps(x1, x0, 0x99); // 00772b14 shufps xmm1, xmm0, 0x99
    x2 = x1; // 00772b18 movaps xmm2, xmm1
    x0.u[0] = x86::bits(x86::add(x0.f(0), x1.f(0))); // 00772b1b addss xmm0, xmm1
    x2 = shufps(x2, x1, 0x55); // 00772b1f shufps xmm2, xmm1, 0x55
    x0.u[0] = x86::bits(x86::add(x0.f(0), x2.f(0))); // 00772b23 addss xmm0, xmm2
    x1 = x0; // 00772b27 movaps xmm1, xmm0
    LIT_CALL(0x8454c0); // 00772b2a call 0x8454c0
    x2 = x1; // 00772b2f movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0); // 00772b32 shufps xmm2, xmm1, 0
    x0 = shufps(x0, x0, 0); // 00772b36 shufps xmm0, xmm0, 0
    x2 = subps(x2, x0); // 00772b3a subps xmm2, xmm0
    x0 = X{{g.u32((esp + uint32_t(0x34))), 0, 0, 0}}; // 00772b3d movss xmm0, dword ptr [esp + 0x34]
    x0 = shufps(x0, x0, 0); // 00772b43 shufps xmm0, xmm0, 0
    x0 = cmpltps(x0, x2); // 00772b47 cmpltps xmm0, xmm2
    st(g, (esp + uint32_t(0x30)), x0); // 00772b4b movaps xmmword ptr [esp + 0x30], xmm0
    edx = g.u32((esp + uint32_t(0x34))); // 00772b50 mov edx, dword ptr [esp + 0x34]
    eax = g.u32((esp + uint32_t(0x38))); // 00772b54 mov eax, dword ptr [esp + 0x38]
    { const uint32_t r_ = eax + edx; eax = r_; } // 00772b58 add eax, edx
    { const uint32_t r_ = eax + g.u32((esp + uint32_t(0x30))); eax = r_; } // 00772b5a add eax, dword ptr [esp + 0x30]
    { CMP_A = eax; CMP_B = uint32_t(-3); } // 00772b5e cmp eax, -3
    if (CMP_A == CMP_B) goto L_00772b82; // 00772b61 je 0x772b82
    ecx = g.u32((esp + uint32_t(0x1c))); // 00772b63 mov ecx, dword ptr [esp + 0x1c]
    edx = g.u32((esp + uint32_t(0x10))); // 00772b67 mov edx, dword ptr [esp + 0x10]
    g.u32((ecx), uint32_t(0x10)); // 00772b6b mov dword ptr [ecx], 0x10
    eax = g.u32((edx + uint32_t(0xe4))); // 00772b71 mov eax, dword ptr [edx + 0xe4]
    ecx = g.u32((esp + uint32_t(0x18))); // 00772b77 mov ecx, dword ptr [esp + 0x18]
    g.u32((ecx), eax); // 00772b7b mov dword ptr [ecx], eax
    edi = uint32_t(1); // 00772b7d mov edi, 1
  L_00772b82:;
    eax = g.u32((ebx + uint32_t(0x19cc))); // 00772b82 mov eax, dword ptr [ebx + 0x19cc]
    esi = g.u32((eax + uint32_t(0x30))); // 00772b88 mov esi, dword ptr [eax + 0x30]
    edx = (esp + uint32_t(0x60)); // 00772b8b lea edx, [esp + 0x60]
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 00772b8f push edx
    edx = (esp + uint32_t(0x54)); // 00772b90 lea edx, [esp + 0x54]
    g.u32((esp + uint32_t(0x14)), eax); // 00772b94 mov dword ptr [esp + 0x14], eax
    LIT_CALL(0x883020); // 00772b98 call 0x883020
    eax = g.u32((esp + uint32_t(0x10))); // 00772b9d mov eax, dword ptr [esp + 0x10]
    g.u32((eax), uint32_t(0)); // 00772ba1 mov dword ptr [eax], 0
  L_00772ba7:;
    ecx = g.u32((ebx + uint32_t(0x19cc))); // 00772ba7 mov ecx, dword ptr [ebx + 0x19cc]
    edx = g.u32((ecx + uint32_t(0x30))); // 00772bad mov edx, dword ptr [ecx + 0x30]
    eax = g.u32((ecx)); // 00772bb0 mov eax, dword ptr [ecx]
    { CMP_A = eax; CMP_B = g.u32((edx + uint32_t(0x400))); } // 00772bb2 cmp eax, dword ptr [edx + 0x400]
    if (int32_t(CMP_A) >= int32_t(CMP_B)) goto L_00772ce7; // 00772bb8 jge 0x772ce7
    { const uint32_t r_ = eax + 1u; eax = r_; } // 00772bbe inc eax
    g.u32((ecx), eax); // 00772bbf mov dword ptr [ecx], eax
    esi = g.u32((edx + eax * 4 - uint32_t(4))); // 00772bc1 mov esi, dword ptr [edx + eax*4 - 4]
    { CMP_A = esi; CMP_B = uint32_t(-1); } // 00772bc5 cmp esi, -1
    if (CMP_A == CMP_B) goto L_00772ce7; // 00772bc8 je 0x772ce7
    ecx = g.u32((ebx + uint32_t(0x19c4))); // 00772bce mov ecx, dword ptr [ebx + 0x19c4]
    { const uint32_t r_ = ecx - g.u32((ebx + uint32_t(0x19c0))); ecx = r_; } // 00772bd4 sub ecx, dword ptr [ebx + 0x19c0]
    { const uint32_t r_ = uint32_t(int32_t(ecx) >> (uint32_t(6) & 31u)); ecx = r_; } // 00772bda sar ecx, 6
    { CMP_A = esi; CMP_B = ecx; } // 00772bdd cmp esi, ecx
    if (CMP_A < CMP_B) goto L_00772be7; // 00772bdf jb 0x772be7
    LIT_CALL(g.u32((uint32_t(0x916224)))); // 00772be1 call dword ptr [0x916224]
  L_00772be7:;
    { const uint32_t r_ = esi << (uint32_t(6) & 31u); esi = r_; } // 00772be7 shl esi, 6
    { const uint32_t r_ = esi + g.u32((ebx + uint32_t(0x19c0))); esi = r_; } // 00772bea add esi, dword ptr [ebx + 0x19c0]
    { CMP_A = uint32_t(g.u8((esi + uint32_t(0x38)))); CMP_B = uint32_t(0); } // 00772bf0 cmp byte ptr [esi + 0x38], 0
    g.u32((esp + uint32_t(0x14)), esi); // 00772bf4 mov dword ptr [esp + 0x14], esi
    if (CMP_A == CMP_B) goto L_00772ba7; // 00772bf8 je 0x772ba7
    x0 = ld(g, (esi)); // 00772bfa movaps xmm0, xmmword ptr [esi]
    edx = 0u; // 00772bfd xor edx, edx
    st(g, (esp + uint32_t(0x30)), x0); // 00772bff movaps xmmword ptr [esp + 0x30], xmm0
    x0 = ld(g, (esi + uint32_t(0x10))); // 00772c04 movaps xmm0, xmmword ptr [esi + 0x10]
    st(g, (esp + uint32_t(0x40)), x0); // 00772c08 movaps xmmword ptr [esp + 0x40], xmm0
    g.u32((esp + uint32_t(0x710)), edx); // 00772c0d mov dword ptr [esp + 0x710], edx
    g.u32((esp + uint32_t(0x70)), edx); // 00772c14 mov dword ptr [esp + 0x70], edx
    eax = 0u; // 00772c18 xor eax, eax
    ebx = (ebx); // 00772c1a lea ebx, [ebx]
  L_00772c20:;
    { const uint32_t r_ = ecx | uint32_t(0xffffffff); ecx = r_; } // 00772c20 or ecx, 0xffffffff
    g.u32((esp + eax * 4 + uint32_t(0x620)), ecx); // 00772c23 mov dword ptr [esp + eax*4 + 0x620], ecx
    g.u32((esp + eax * 4 + uint32_t(0x698)), ecx); // 00772c2a mov dword ptr [esp + eax*4 + 0x698], ecx
    { const uint32_t r_ = eax + 1u; eax = r_; } // 00772c31 inc eax
    { CMP_A = eax; CMP_B = uint32_t(0x1e); } // 00772c32 cmp eax, 0x1e
    if (int32_t(CMP_A) < int32_t(CMP_B)) goto L_00772c20; // 00772c35 jl 0x772c20
    ecx = g.u32((esi + uint32_t(0x28))); // 00772c37 mov ecx, dword ptr [esi + 0x28]
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 00772c3a push edx
    edx = (esp + uint32_t(0x34)); // 00772c3b lea edx, [esp + 0x34]
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 00772c3f push edx
    edx = g.u32((esp + uint32_t(0x34))); // 00772c40 mov edx, dword ptr [esp + 0x34]
    eax = (esp + uint32_t(0x48)); // 00772c44 lea eax, [esp + 0x48]
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 00772c48 push eax
    eax = g.u32((esp + uint32_t(0x2c))); // 00772c49 mov eax, dword ptr [esp + 0x2c]
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 00772c4d push ecx
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 00772c4e push edx
    { const uint32_t v_ = uint32_t(0xd61640); esp -= 4; g.u32(esp, v_); } // 00772c4f push 0xd61640
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 00772c54 push eax
    ecx = (esp + uint32_t(0x8c)); // 00772c55 lea ecx, [esp + 0x8c]
    LIT_CALL(0x85ddd0); // 00772c5c call 0x85ddd0
    { CMP_A = g.u32((esp + uint32_t(0x710))); CMP_B = uint32_t(0); } // 00772c61 cmp dword ptr [esp + 0x710], 0
    if (CMP_A == CMP_B) goto L_00772ba7; // 00772c69 je 0x772ba7
    eax = g.u32((esp + uint32_t(0x70))); // 00772c6f mov eax, dword ptr [esp + 0x70]
    { CMP_A = eax; CMP_B = eax; } // 00772c73 test eax, eax
    if (int32_t((CMP_A & CMP_B)) <= 0) goto L_00772ba7; // 00772c75 jle 0x772ba7
    ecx = g.u32((esp + uint32_t(0x18))); // 00772c7b mov ecx, dword ptr [esp + 0x18]
    ebx = g.u32((esp + uint32_t(0x1c))); // 00772c7f mov ebx, dword ptr [esp + 0x1c]
    edx = (edi + edi * 2); // 00772c83 lea edx, [edi + edi*2]
    { const uint32_t r_ = edx << (uint32_t(4) & 31u); edx = r_; } // 00772c86 shl edx, 4
    { const uint32_t r_ = edx + g.u32((esp + uint32_t(0x28))); edx = r_; } // 00772c89 add edx, dword ptr [esp + 0x28]
    { const uint32_t r_ = ebx - g.u32((esp + uint32_t(0x18))); ebx = r_; } // 00772c8d sub ebx, dword ptr [esp + 0x18]
    esi = (ecx + edi * 4); // 00772c91 lea esi, [ecx + edi*4]
    ecx = (esp + uint32_t(0xa0)); // 00772c94 lea ecx, [esp + 0xa0]
    g.u32((esp + uint32_t(0x10)), eax); // 00772c9b mov dword ptr [esp + 0x10], eax
  L_00772ca0:;
    { CMP_A = edi; CMP_B = uint32_t(0xa); } // 00772ca0 cmp edi, 0xa
    if (int32_t(CMP_A) >= int32_t(CMP_B)) goto L_00772cd4; // 00772ca3 jge 0x772cd4
    eax = g.u32((esp + uint32_t(0x14))); // 00772ca5 mov eax, dword ptr [esp + 0x14]
    x0 = ld(g, (ecx - uint32_t(0x20))); // 00772ca9 movaps xmm0, xmmword ptr [ecx - 0x20]
    g.u32((ebx + esi), uint32_t(1)); // 00772cad mov dword ptr [ebx + esi], 1
    eax = g.u32((eax + uint32_t(0x30))); // 00772cb4 mov eax, dword ptr [eax + 0x30]
    g.u32((esi), eax); // 00772cb7 mov dword ptr [esi], eax
    eax = edx; // 00772cb9 mov eax, edx
    st(g, (eax), x0); // 00772cbb movaps xmmword ptr [eax], xmm0
    x0 = ld(g, (ecx - uint32_t(0x10))); // 00772cbe movaps xmm0, xmmword ptr [ecx - 0x10]
    st(g, (eax + uint32_t(0x10)), x0); // 00772cc2 movaps xmmword ptr [eax + 0x10], xmm0
    x0 = ld(g, (ecx)); // 00772cc6 movaps xmm0, xmmword ptr [ecx]
    { const uint32_t r_ = edi + 1u; edi = r_; } // 00772cc9 inc edi
    { const uint32_t r_ = esi + uint32_t(4); esi = r_; } // 00772cca add esi, 4
    { const uint32_t r_ = edx + uint32_t(0x30); edx = r_; } // 00772ccd add edx, 0x30
    st(g, (eax + uint32_t(0x20)), x0); // 00772cd0 movaps xmmword ptr [eax + 0x20], xmm0
  L_00772cd4:;
    { const uint32_t r_ = ecx + uint32_t(0x30); ecx = r_; } // 00772cd4 add ecx, 0x30
    { const uint32_t r_ = g.u32((esp + uint32_t(0x10))) - uint32_t(1); g.u32((esp + uint32_t(0x10)), r_); } // 00772cd7 sub dword ptr [esp + 0x10], 1
    if (g.u32((esp + uint32_t(0x10))) != 0) goto L_00772ca0; // 00772cdc jne 0x772ca0
    ebx = g.u32((esp + uint32_t(0x24))); // 00772cde mov ebx, dword ptr [esp + 0x24]
    goto L_00772ba7; // 00772ce2 jmp 0x772ba7
  L_00772ce7:;
    { CMP_A = edi; CMP_B = uint32_t(0xa); } // 00772ce7 cmp edi, 0xa
    if (CMP_A != CMP_B) goto L_00772d08; // 00772cea jne 0x772d08
  L_00772cec:;
    eax = uint32_t(0xa); // 00772cec mov eax, 0xa
    { const uint32_t v_ = g.u32(esp); esp += 4; edi = v_; } // 00772cf1 pop edi
    { const uint32_t v_ = g.u32(esp); esp += 4; esi = v_; } // 00772cf2 pop esi
    { const uint32_t v_ = g.u32(esp); esp += 4; ebx = v_; } // 00772cf3 pop ebx
    ecx = g.u32((esp + uint32_t(0x720))); // 00772cf4 mov ecx, dword ptr [esp + 0x720]
    { const uint32_t r_ = ecx ^ esp; ecx = r_; } // 00772cfb xor ecx, esp
    LIT_CALL(0x8f9efe); // 00772cfd call 0x8f9efe
    esp = ebp; // 00772d02 mov esp, ebp
    { const uint32_t v_ = g.u32(esp); esp += 4; ebp = v_; } // 00772d04 pop ebp
    goto L_return; // 00772d05 ret 0x10
  L_00772d08:;
    eax = g.u32((ebx + uint32_t(0x19ac))); // 00772d08 mov eax, dword ptr [ebx + 0x19ac]
    esi = g.u32((eax + uint32_t(0x30))); // 00772d0e mov esi, dword ptr [eax + 0x30]
    ecx = (esp + uint32_t(0x60)); // 00772d11 lea ecx, [esp + 0x60]
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 00772d15 push ecx
    edx = (esp + uint32_t(0x54)); // 00772d16 lea edx, [esp + 0x54]
    g.u32((esp + uint32_t(0x18)), eax); // 00772d1a mov dword ptr [esp + 0x18], eax
    LIT_CALL(0x883020); // 00772d1e call 0x883020
    edx = g.u32((esp + uint32_t(0x14))); // 00772d23 mov edx, dword ptr [esp + 0x14]
    g.u32((edx), uint32_t(0)); // 00772d27 mov dword ptr [edx], 0
    ecx = (ecx); // 00772d2d lea ecx, [ecx]
  L_00772d30:;
    ecx = g.u32((ebx + uint32_t(0x19ac))); // 00772d30 mov ecx, dword ptr [ebx + 0x19ac]
    edx = g.u32((ecx + uint32_t(0x30))); // 00772d36 mov edx, dword ptr [ecx + 0x30]
    eax = g.u32((ecx)); // 00772d39 mov eax, dword ptr [ecx]
    { CMP_A = eax; CMP_B = g.u32((edx + uint32_t(0x400))); } // 00772d3b cmp eax, dword ptr [edx + 0x400]
    if (int32_t(CMP_A) >= int32_t(CMP_B)) goto L_00772e87; // 00772d41 jge 0x772e87
    { const uint32_t r_ = eax + 1u; eax = r_; } // 00772d47 inc eax
    g.u32((ecx), eax); // 00772d48 mov dword ptr [ecx], eax
    esi = g.u32((edx + eax * 4 - uint32_t(4))); // 00772d4a mov esi, dword ptr [edx + eax*4 - 4]
    { CMP_A = esi; CMP_B = uint32_t(-1); } // 00772d4e cmp esi, -1
    if (CMP_A == CMP_B) goto L_00772e87; // 00772d51 je 0x772e87
    ecx = g.u32((ebx + uint32_t(0x19a4))); // 00772d57 mov ecx, dword ptr [ebx + 0x19a4]
    { const uint32_t r_ = ecx - g.u32((ebx + uint32_t(0x19a0))); ecx = r_; } // 00772d5d sub ecx, dword ptr [ebx + 0x19a0]
    eax = uint32_t(0x92492493); // 00772d63 mov eax, 0x92492493
    { const int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); } // 00772d68 imul ecx
    { const uint32_t r_ = edx + ecx; edx = r_; } // 00772d6a add edx, ecx
    { const uint32_t r_ = uint32_t(int32_t(edx) >> (uint32_t(7) & 31u)); edx = r_; } // 00772d6c sar edx, 7
    eax = edx; // 00772d6f mov eax, edx
    { const uint32_t r_ = eax >> (uint32_t(0x1f) & 31u); eax = r_; } // 00772d71 shr eax, 0x1f
    { const uint32_t r_ = eax + edx; eax = r_; } // 00772d74 add eax, edx
    { CMP_A = esi; CMP_B = eax; } // 00772d76 cmp esi, eax
    if (CMP_A < CMP_B) goto L_00772d80; // 00772d78 jb 0x772d80
    LIT_CALL(g.u32((uint32_t(0x916224)))); // 00772d7a call dword ptr [0x916224]
  L_00772d80:;
    esi = uint32_t(int32_t(esi) * int32_t(uint32_t(0xe0))); // 00772d80 imul esi, esi, 0xe0
    { const uint32_t r_ = esi + g.u32((ebx + uint32_t(0x19a0))); esi = r_; } // 00772d86 add esi, dword ptr [ebx + 0x19a0]
    ecx = esi; // 00772d8c mov ecx, esi
    { CMP_A = uint32_t(g.u8((ecx + uint32_t(0xb0)))); CMP_B = uint32_t(0); } // 00772d8e cmp byte ptr [ecx + 0xb0], 0
    g.u32((esp + uint32_t(0x14)), ecx); // 00772d95 mov dword ptr [esp + 0x14], ecx
    if (CMP_A == CMP_B) goto L_00772d30; // 00772d99 je 0x772d30
    x0 = ld(g, (ecx + uint32_t(0x40))); // 00772d9b movaps xmm0, xmmword ptr [ecx + 0x40]
    esi = 0u; // 00772d9f xor esi, esi
    st(g, (esp + uint32_t(0x40)), x0); // 00772da1 movaps xmmword ptr [esp + 0x40], xmm0
    x0 = ld(g, (ecx + uint32_t(0x50))); // 00772da6 movaps xmm0, xmmword ptr [ecx + 0x50]
    st(g, (esp + uint32_t(0x30)), x0); // 00772daa movaps xmmword ptr [esp + 0x30], xmm0
    g.u32((esp + uint32_t(0x710)), esi); // 00772daf mov dword ptr [esp + 0x710], esi
    g.u32((esp + uint32_t(0x70)), esi); // 00772db6 mov dword ptr [esp + 0x70], esi
    eax = 0u; // 00772dba xor eax, eax
    esp = (esp); // 00772dbc lea esp, [esp]
  L_00772dc0:;
    { const uint32_t r_ = edx | uint32_t(0xffffffff); edx = r_; } // 00772dc0 or edx, 0xffffffff
    g.u32((esp + eax * 4 + uint32_t(0x620)), edx); // 00772dc3 mov dword ptr [esp + eax*4 + 0x620], edx
    g.u32((esp + eax * 4 + uint32_t(0x698)), edx); // 00772dca mov dword ptr [esp + eax*4 + 0x698], edx
    { const uint32_t r_ = eax + 1u; eax = r_; } // 00772dd1 inc eax
    { CMP_A = eax; CMP_B = uint32_t(0x1e); } // 00772dd2 cmp eax, 0x1e
    if (int32_t(CMP_A) < int32_t(CMP_B)) goto L_00772dc0; // 00772dd5 jl 0x772dc0
    ecx = g.u32((ecx + uint32_t(0x2c))); // 00772dd7 mov ecx, dword ptr [ecx + 0x2c]
    { const uint32_t v_ = esi; esp -= 4; g.u32(esp, v_); } // 00772dda push esi
    edx = (esp + uint32_t(0x44)); // 00772ddb lea edx, [esp + 0x44]
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 00772ddf push edx
    edx = g.u32((esp + uint32_t(0x34))); // 00772de0 mov edx, dword ptr [esp + 0x34]
    eax = (esp + uint32_t(0x38)); // 00772de4 lea eax, [esp + 0x38]
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 00772de8 push eax
    eax = g.u32((esp + uint32_t(0x2c))); // 00772de9 mov eax, dword ptr [esp + 0x2c]
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 00772ded push ecx
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 00772dee push edx
    { const uint32_t v_ = uint32_t(0xd61640); esp -= 4; g.u32(esp, v_); } // 00772def push 0xd61640
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 00772df4 push eax
    ecx = (esp + uint32_t(0x8c)); // 00772df5 lea ecx, [esp + 0x8c]
    LIT_CALL(0x85ddd0); // 00772dfc call 0x85ddd0
    { CMP_A = g.u32((esp + uint32_t(0x710))); CMP_B = esi; } // 00772e01 cmp dword ptr [esp + 0x710], esi
    if (CMP_A == CMP_B) goto L_00772d30; // 00772e08 je 0x772d30
    eax = g.u32((esp + uint32_t(0x70))); // 00772e0e mov eax, dword ptr [esp + 0x70]
    { CMP_A = eax; CMP_B = eax; } // 00772e12 test eax, eax
    if (int32_t((CMP_A & CMP_B)) <= 0) goto L_00772d30; // 00772e14 jle 0x772d30
    ecx = g.u32((esp + uint32_t(0x18))); // 00772e1a mov ecx, dword ptr [esp + 0x18]
    ebx = g.u32((esp + uint32_t(0x1c))); // 00772e1e mov ebx, dword ptr [esp + 0x1c]
    edx = (edi + edi * 2); // 00772e22 lea edx, [edi + edi*2]
    { const uint32_t r_ = edx << (uint32_t(4) & 31u); edx = r_; } // 00772e25 shl edx, 4
    { const uint32_t r_ = edx + g.u32((esp + uint32_t(0x28))); edx = r_; } // 00772e28 add edx, dword ptr [esp + 0x28]
    { const uint32_t r_ = ebx - g.u32((esp + uint32_t(0x18))); ebx = r_; } // 00772e2c sub ebx, dword ptr [esp + 0x18]
    esi = (ecx + edi * 4); // 00772e30 lea esi, [ecx + edi*4]
    ecx = (esp + uint32_t(0xa0)); // 00772e33 lea ecx, [esp + 0xa0]
    g.u32((esp + uint32_t(0x10)), eax); // 00772e3a mov dword ptr [esp + 0x10], eax
    edi = edi; // 00772e3e mov edi, edi
  L_00772e40:;
    { CMP_A = edi; CMP_B = uint32_t(0xa); } // 00772e40 cmp edi, 0xa
    if (int32_t(CMP_A) >= int32_t(CMP_B)) goto L_00772e74; // 00772e43 jge 0x772e74
    eax = g.u32((esp + uint32_t(0x14))); // 00772e45 mov eax, dword ptr [esp + 0x14]
    x0 = ld(g, (ecx - uint32_t(0x20))); // 00772e49 movaps xmm0, xmmword ptr [ecx - 0x20]
    g.u32((esi + ebx), uint32_t(2)); // 00772e4d mov dword ptr [esi + ebx], 2
    eax = g.u32((eax + uint32_t(0x34))); // 00772e54 mov eax, dword ptr [eax + 0x34]
    g.u32((esi), eax); // 00772e57 mov dword ptr [esi], eax
    eax = edx; // 00772e59 mov eax, edx
    st(g, (eax), x0); // 00772e5b movaps xmmword ptr [eax], xmm0
    x0 = ld(g, (ecx - uint32_t(0x10))); // 00772e5e movaps xmm0, xmmword ptr [ecx - 0x10]
    st(g, (eax + uint32_t(0x10)), x0); // 00772e62 movaps xmmword ptr [eax + 0x10], xmm0
    x0 = ld(g, (ecx)); // 00772e66 movaps xmm0, xmmword ptr [ecx]
    { const uint32_t r_ = edi + 1u; edi = r_; } // 00772e69 inc edi
    { const uint32_t r_ = esi + uint32_t(4); esi = r_; } // 00772e6a add esi, 4
    { const uint32_t r_ = edx + uint32_t(0x30); edx = r_; } // 00772e6d add edx, 0x30
    st(g, (eax + uint32_t(0x20)), x0); // 00772e70 movaps xmmword ptr [eax + 0x20], xmm0
  L_00772e74:;
    { const uint32_t r_ = ecx + uint32_t(0x30); ecx = r_; } // 00772e74 add ecx, 0x30
    { const uint32_t r_ = g.u32((esp + uint32_t(0x10))) - uint32_t(1); g.u32((esp + uint32_t(0x10)), r_); } // 00772e77 sub dword ptr [esp + 0x10], 1
    if (g.u32((esp + uint32_t(0x10))) != 0) goto L_00772e40; // 00772e7c jne 0x772e40
    ebx = g.u32((esp + uint32_t(0x24))); // 00772e7e mov ebx, dword ptr [esp + 0x24]
    goto L_00772d30; // 00772e82 jmp 0x772d30
  L_00772e87:;
    { CMP_A = edi; CMP_B = uint32_t(0xa); } // 00772e87 cmp edi, 0xa
    if (CMP_A == CMP_B) goto L_00772cec; // 00772e8a je 0x772cec
    eax = g.u32((ebx + uint32_t(0x196c))); // 00772e90 mov eax, dword ptr [ebx + 0x196c]
    esi = g.u32((eax + uint32_t(0x30))); // 00772e96 mov esi, dword ptr [eax + 0x30]
    ecx = (esp + uint32_t(0x60)); // 00772e99 lea ecx, [esp + 0x60]
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 00772e9d push ecx
    edx = (esp + uint32_t(0x54)); // 00772e9e lea edx, [esp + 0x54]
    g.u32((esp + uint32_t(0x18)), eax); // 00772ea2 mov dword ptr [esp + 0x18], eax
    LIT_CALL(0x883020); // 00772ea6 call 0x883020
    edx = g.u32((esp + uint32_t(0x14))); // 00772eab mov edx, dword ptr [esp + 0x14]
    g.u32((edx), uint32_t(0)); // 00772eaf mov dword ptr [edx], 0
  L_00772eb5:;
    ecx = g.u32((ebx + uint32_t(0x196c))); // 00772eb5 mov ecx, dword ptr [ebx + 0x196c]
    edx = g.u32((ecx + uint32_t(0x30))); // 00772ebb mov edx, dword ptr [ecx + 0x30]
    eax = g.u32((ecx)); // 00772ebe mov eax, dword ptr [ecx]
    { CMP_A = eax; CMP_B = g.u32((edx + uint32_t(0x400))); } // 00772ec0 cmp eax, dword ptr [edx + 0x400]
    if (int32_t(CMP_A) >= int32_t(CMP_B)) goto L_00773007; // 00772ec6 jge 0x773007
    { const uint32_t r_ = eax + 1u; eax = r_; } // 00772ecc inc eax
    g.u32((ecx), eax); // 00772ecd mov dword ptr [ecx], eax
    esi = g.u32((edx + eax * 4 - uint32_t(4))); // 00772ecf mov esi, dword ptr [edx + eax*4 - 4]
    { const uint32_t r_ = ecx | uint32_t(0xffffffff); ecx = r_; } // 00772ed3 or ecx, 0xffffffff
    { CMP_A = esi; CMP_B = ecx; } // 00772ed6 cmp esi, ecx
    if (CMP_A == CMP_B) goto L_0077300a; // 00772ed8 je 0x77300a
    ecx = g.u32((ebx + uint32_t(0x1964))); // 00772ede mov ecx, dword ptr [ebx + 0x1964]
    { const uint32_t r_ = ecx - g.u32((ebx + uint32_t(0x1960))); ecx = r_; } // 00772ee4 sub ecx, dword ptr [ebx + 0x1960]
    eax = uint32_t(0x92492493); // 00772eea mov eax, 0x92492493
    { const int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(ecx)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); } // 00772eef imul ecx
    { const uint32_t r_ = edx + ecx; edx = r_; } // 00772ef1 add edx, ecx
    { const uint32_t r_ = uint32_t(int32_t(edx) >> (uint32_t(7) & 31u)); edx = r_; } // 00772ef3 sar edx, 7
    eax = edx; // 00772ef6 mov eax, edx
    { const uint32_t r_ = eax >> (uint32_t(0x1f) & 31u); eax = r_; } // 00772ef8 shr eax, 0x1f
    { const uint32_t r_ = eax + edx; eax = r_; } // 00772efb add eax, edx
    { CMP_A = esi; CMP_B = eax; } // 00772efd cmp esi, eax
    if (CMP_A < CMP_B) goto L_00772f07; // 00772eff jb 0x772f07
    LIT_CALL(g.u32((uint32_t(0x916224)))); // 00772f01 call dword ptr [0x916224]
  L_00772f07:;
    esi = uint32_t(int32_t(esi) * int32_t(uint32_t(0xe0))); // 00772f07 imul esi, esi, 0xe0
    { const uint32_t r_ = esi + g.u32((ebx + uint32_t(0x1960))); esi = r_; } // 00772f0d add esi, dword ptr [ebx + 0x1960]
    edx = 0u; // 00772f13 xor edx, edx
    x0 = ld(g, (esi + uint32_t(0x40))); // 00772f15 movaps xmm0, xmmword ptr [esi + 0x40]
    st(g, (esp + uint32_t(0x40)), x0); // 00772f19 movaps xmmword ptr [esp + 0x40], xmm0
    x0 = ld(g, (esi + uint32_t(0x50))); // 00772f1e movaps xmm0, xmmword ptr [esi + 0x50]
    g.u32((esp + uint32_t(0x14)), esi); // 00772f22 mov dword ptr [esp + 0x14], esi
    st(g, (esp + uint32_t(0x30)), x0); // 00772f26 movaps xmmword ptr [esp + 0x30], xmm0
    g.u32((esp + uint32_t(0x710)), edx); // 00772f2b mov dword ptr [esp + 0x710], edx
    g.u32((esp + uint32_t(0x70)), edx); // 00772f32 mov dword ptr [esp + 0x70], edx
    eax = 0u; // 00772f36 xor eax, eax
    goto L_00772f40; // 00772f38 jmp 0x772f40
    ebx = (ebx); // 00772f3a lea ebx, [ebx]
  L_00772f40:;
    { const uint32_t r_ = ecx | uint32_t(0xffffffff); ecx = r_; } // 00772f40 or ecx, 0xffffffff
    g.u32((esp + eax * 4 + uint32_t(0x620)), ecx); // 00772f43 mov dword ptr [esp + eax*4 + 0x620], ecx
    g.u32((esp + eax * 4 + uint32_t(0x698)), ecx); // 00772f4a mov dword ptr [esp + eax*4 + 0x698], ecx
    { const uint32_t r_ = eax + 1u; eax = r_; } // 00772f51 inc eax
    { CMP_A = eax; CMP_B = uint32_t(0x1e); } // 00772f52 cmp eax, 0x1e
    if (int32_t(CMP_A) < int32_t(CMP_B)) goto L_00772f40; // 00772f55 jl 0x772f40
    eax = g.u32((esi + uint32_t(0x2c))); // 00772f57 mov eax, dword ptr [esi + 0x2c]
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 00772f5a push edx
    ecx = (esp + uint32_t(0x44)); // 00772f5b lea ecx, [esp + 0x44]
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 00772f5f push ecx
    ecx = g.u32((esp + uint32_t(0x34))); // 00772f60 mov ecx, dword ptr [esp + 0x34]
    edx = (esp + uint32_t(0x38)); // 00772f64 lea edx, [esp + 0x38]
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 00772f68 push edx
    edx = g.u32((esp + uint32_t(0x2c))); // 00772f69 mov edx, dword ptr [esp + 0x2c]
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 00772f6d push eax
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 00772f6e push ecx
    { const uint32_t v_ = uint32_t(0xd61640); esp -= 4; g.u32(esp, v_); } // 00772f6f push 0xd61640
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 00772f74 push edx
    ecx = (esp + uint32_t(0x8c)); // 00772f75 lea ecx, [esp + 0x8c]
    LIT_CALL(0x85ddd0); // 00772f7c call 0x85ddd0
    { CMP_A = g.u32((esp + uint32_t(0x710))); CMP_B = uint32_t(0); } // 00772f81 cmp dword ptr [esp + 0x710], 0
    if (CMP_A == CMP_B) goto L_00772eb5; // 00772f89 je 0x772eb5
    eax = g.u32((esp + uint32_t(0x70))); // 00772f8f mov eax, dword ptr [esp + 0x70]
    { CMP_A = eax; CMP_B = eax; } // 00772f93 test eax, eax
    if (int32_t((CMP_A & CMP_B)) <= 0) goto L_00772eb5; // 00772f95 jle 0x772eb5
    ecx = g.u32((esp + uint32_t(0x18))); // 00772f9b mov ecx, dword ptr [esp + 0x18]
    ebx = g.u32((esp + uint32_t(0x1c))); // 00772f9f mov ebx, dword ptr [esp + 0x1c]
    edx = (edi + edi * 2); // 00772fa3 lea edx, [edi + edi*2]
    { const uint32_t r_ = edx << (uint32_t(4) & 31u); edx = r_; } // 00772fa6 shl edx, 4
    { const uint32_t r_ = edx + g.u32((esp + uint32_t(0x28))); edx = r_; } // 00772fa9 add edx, dword ptr [esp + 0x28]
    { const uint32_t r_ = ebx - g.u32((esp + uint32_t(0x18))); ebx = r_; } // 00772fad sub ebx, dword ptr [esp + 0x18]
    esi = (ecx + edi * 4); // 00772fb1 lea esi, [ecx + edi*4]
    ecx = (esp + uint32_t(0xa0)); // 00772fb4 lea ecx, [esp + 0xa0]
    g.u32((esp + uint32_t(0x10)), eax); // 00772fbb mov dword ptr [esp + 0x10], eax
  L_00772fc0:;
    { CMP_A = edi; CMP_B = uint32_t(0xa); } // 00772fc0 cmp edi, 0xa
    if (int32_t(CMP_A) >= int32_t(CMP_B)) goto L_00772ff4; // 00772fc3 jge 0x772ff4
    eax = g.u32((esp + uint32_t(0x14))); // 00772fc5 mov eax, dword ptr [esp + 0x14]
    x0 = ld(g, (ecx - uint32_t(0x20))); // 00772fc9 movaps xmm0, xmmword ptr [ecx - 0x20]
    g.u32((esi + ebx), uint32_t(4)); // 00772fcd mov dword ptr [esi + ebx], 4
    eax = g.u32((eax + uint32_t(0x34))); // 00772fd4 mov eax, dword ptr [eax + 0x34]
    g.u32((esi), eax); // 00772fd7 mov dword ptr [esi], eax
    eax = edx; // 00772fd9 mov eax, edx
    st(g, (eax), x0); // 00772fdb movaps xmmword ptr [eax], xmm0
    x0 = ld(g, (ecx - uint32_t(0x10))); // 00772fde movaps xmm0, xmmword ptr [ecx - 0x10]
    st(g, (eax + uint32_t(0x10)), x0); // 00772fe2 movaps xmmword ptr [eax + 0x10], xmm0
    x0 = ld(g, (ecx)); // 00772fe6 movaps xmm0, xmmword ptr [ecx]
    { const uint32_t r_ = edi + 1u; edi = r_; } // 00772fe9 inc edi
    { const uint32_t r_ = esi + uint32_t(4); esi = r_; } // 00772fea add esi, 4
    { const uint32_t r_ = edx + uint32_t(0x30); edx = r_; } // 00772fed add edx, 0x30
    st(g, (eax + uint32_t(0x20)), x0); // 00772ff0 movaps xmmword ptr [eax + 0x20], xmm0
  L_00772ff4:;
    { const uint32_t r_ = ecx + uint32_t(0x30); ecx = r_; } // 00772ff4 add ecx, 0x30
    { const uint32_t r_ = g.u32((esp + uint32_t(0x10))) - uint32_t(1); g.u32((esp + uint32_t(0x10)), r_); } // 00772ff7 sub dword ptr [esp + 0x10], 1
    if (g.u32((esp + uint32_t(0x10))) != 0) goto L_00772fc0; // 00772ffc jne 0x772fc0
    ebx = g.u32((esp + uint32_t(0x24))); // 00772ffe mov ebx, dword ptr [esp + 0x24]
    goto L_00772eb5; // 00773002 jmp 0x772eb5
  L_00773007:;
    { const uint32_t r_ = ecx | uint32_t(0xffffffff); ecx = r_; } // 00773007 or ecx, 0xffffffff
  L_0077300a:;
    { CMP_A = edi; CMP_B = uint32_t(0xa); } // 0077300a cmp edi, 0xa
    if (CMP_A == CMP_B) goto L_00772cec; // 0077300d je 0x772cec
    ebx = g.u32((ebx + uint32_t(0xcc9c))); // 00773013 mov ebx, dword ptr [ebx + 0xcc9c]
    edx = 0u; // 00773019 xor edx, edx
    { CMP_A = ebx; CMP_B = edx; } // 0077301b cmp ebx, edx
    if (CMP_A == CMP_B) goto L_007730f7; // 0077301d je 0x7730f7
    { CMP_A = g.u32((ebx + uint32_t(0x31ec))); CMP_B = edx; } // 00773023 cmp dword ptr [ebx + 0x31ec], edx
    if (CMP_A == CMP_B) goto L_007730f7; // 00773029 je 0x7730f7
    g.u32((esp + uint32_t(0x710)), edx); // 0077302f mov dword ptr [esp + 0x710], edx
    g.u32((esp + uint32_t(0x70)), edx); // 00773036 mov dword ptr [esp + 0x70], edx
    eax = 0u; // 0077303a xor eax, eax
    esp = (esp); // 0077303c lea esp, [esp]
  L_00773040:;
    g.u32((esp + eax * 4 + uint32_t(0x620)), ecx); // 00773040 mov dword ptr [esp + eax*4 + 0x620], ecx
    g.u32((esp + eax * 4 + uint32_t(0x698)), ecx); // 00773047 mov dword ptr [esp + eax*4 + 0x698], ecx
    { const uint32_t r_ = eax + 1u; eax = r_; } // 0077304e inc eax
    { CMP_A = eax; CMP_B = uint32_t(0x1e); } // 0077304f cmp eax, 0x1e
    if (int32_t(CMP_A) < int32_t(CMP_B)) goto L_00773040; // 00773052 jl 0x773040
    eax = g.u32((esp + uint32_t(0x2c))); // 00773054 mov eax, dword ptr [esp + 0x2c]
    ecx = (esp + uint32_t(0x14)); // 00773058 lea ecx, [esp + 0x14]
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 0077305c push ecx
    ecx = g.u32((ebx + uint32_t(0x31ec))); // 0077305d mov ecx, dword ptr [ebx + 0x31ec]
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 00773063 push edx
    edx = (esp + uint32_t(0x18)); // 00773064 lea edx, [esp + 0x18]
    { const uint32_t v_ = edx; esp -= 4; g.u32(esp, v_); } // 00773068 push edx
    { const uint32_t v_ = uint32_t(0xd61640); esp -= 4; g.u32(esp, v_); } // 00773069 push 0xd61640
    { const uint32_t v_ = eax; esp -= 4; g.u32(esp, v_); } // 0077306e push eax
    { const uint32_t v_ = ecx; esp -= 4; g.u32(esp, v_); } // 0077306f push ecx
    ecx = g.u32((esp + uint32_t(0x38))); // 00773070 mov ecx, dword ptr [esp + 0x38]
    edx = (esp + uint32_t(0x88)); // 00773074 lea edx, [esp + 0x88]
    LIT_CALL(0x667e40); // 0077307b call 0x667e40
    { CMP_A = g.u32((esp + uint32_t(0x710))); CMP_B = uint32_t(0); } // 00773080 cmp dword ptr [esp + 0x710], 0
    if (CMP_A == CMP_B) goto L_007730f7; // 00773088 je 0x7730f7
    eax = g.u32((esp + uint32_t(0x70))); // 0077308a mov eax, dword ptr [esp + 0x70]
    { CMP_A = eax; CMP_B = eax; } // 0077308e test eax, eax
    if (int32_t((CMP_A & CMP_B)) <= 0) goto L_007730f7; // 00773090 jle 0x7730f7
    ecx = g.u32((esp + uint32_t(0x18))); // 00773092 mov ecx, dword ptr [esp + 0x18]
    ebx = g.u32((esp + uint32_t(0x1c))); // 00773096 mov ebx, dword ptr [esp + 0x1c]
    edx = (edi + edi * 2); // 0077309a lea edx, [edi + edi*2]
    { const uint32_t r_ = edx << (uint32_t(4) & 31u); edx = r_; } // 0077309d shl edx, 4
    { const uint32_t r_ = edx + g.u32((esp + uint32_t(0x28))); edx = r_; } // 007730a0 add edx, dword ptr [esp + 0x28]
    { const uint32_t r_ = ebx - g.u32((esp + uint32_t(0x18))); ebx = r_; } // 007730a4 sub ebx, dword ptr [esp + 0x18]
    esi = (ecx + edi * 4); // 007730a8 lea esi, [ecx + edi*4]
    ecx = (esp + uint32_t(0xa0)); // 007730ab lea ecx, [esp + 0xa0]
    g.u32((esp + uint32_t(0x10)), eax); // 007730b2 mov dword ptr [esp + 0x10], eax
  L_007730b6:;
    { CMP_A = edi; CMP_B = uint32_t(0xa); } // 007730b6 cmp edi, 0xa
    if (int32_t(CMP_A) >= int32_t(CMP_B)) goto L_007730ed; // 007730b9 jge 0x7730ed
    eax = g.u32((esp + uint32_t(0x24))); // 007730bb mov eax, dword ptr [esp + 0x24]
    x0 = ld(g, (ecx - uint32_t(0x20))); // 007730bf movaps xmm0, xmmword ptr [ecx - 0x20]
    g.u32((esi + ebx), uint32_t(8)); // 007730c3 mov dword ptr [esi + ebx], 8
    eax = g.u32((eax + uint32_t(0xcc9c))); // 007730ca mov eax, dword ptr [eax + 0xcc9c]
    g.u32((esi), eax); // 007730d0 mov dword ptr [esi], eax
    eax = edx; // 007730d2 mov eax, edx
    st(g, (eax), x0); // 007730d4 movaps xmmword ptr [eax], xmm0
    x0 = ld(g, (ecx - uint32_t(0x10))); // 007730d7 movaps xmm0, xmmword ptr [ecx - 0x10]
    st(g, (eax + uint32_t(0x10)), x0); // 007730db movaps xmmword ptr [eax + 0x10], xmm0
    x0 = ld(g, (ecx)); // 007730df movaps xmm0, xmmword ptr [ecx]
    { const uint32_t r_ = edi + 1u; edi = r_; } // 007730e2 inc edi
    { const uint32_t r_ = esi + uint32_t(4); esi = r_; } // 007730e3 add esi, 4
    { const uint32_t r_ = edx + uint32_t(0x30); edx = r_; } // 007730e6 add edx, 0x30
    st(g, (eax + uint32_t(0x20)), x0); // 007730e9 movaps xmmword ptr [eax + 0x20], xmm0
  L_007730ed:;
    { const uint32_t r_ = ecx + uint32_t(0x30); ecx = r_; } // 007730ed add ecx, 0x30
    { const uint32_t r_ = g.u32((esp + uint32_t(0x10))) - uint32_t(1); g.u32((esp + uint32_t(0x10)), r_); } // 007730f0 sub dword ptr [esp + 0x10], 1
    if (g.u32((esp + uint32_t(0x10))) != 0) goto L_007730b6; // 007730f5 jne 0x7730b6
  L_007730f7:;
    ecx = g.u32((esp + uint32_t(0x72c))); // 007730f7 mov ecx, dword ptr [esp + 0x72c]
    eax = edi; // 007730fe mov eax, edi
    { const uint32_t v_ = g.u32(esp); esp += 4; edi = v_; } // 00773100 pop edi
    { const uint32_t v_ = g.u32(esp); esp += 4; esi = v_; } // 00773101 pop esi
    { const uint32_t v_ = g.u32(esp); esp += 4; ebx = v_; } // 00773102 pop ebx
    { const uint32_t r_ = ecx ^ esp; ecx = r_; } // 00773103 xor ecx, esp
    LIT_CALL(0x8f9efe); // 00773105 call 0x8f9efe
    esp = ebp; // 0077310a mov esp, ebp
    { const uint32_t v_ = g.u32(esp); esp += 4; ebp = v_; } // 0077310c pop ebp
    goto L_return; // 0077310d ret 0x10

    LIT_END(0x10)
}

#endif
