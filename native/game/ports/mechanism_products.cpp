// RavenPhysics matrix inverses and block products of the mechanism group.
// Generated from the original instructions by symbolic translation (scratchpad sym2.py): guest loads and stores at their
// program points, every lane with the original operation order, stack locals at the original's own addresses (ESP0 = entry
// ESP, A = the aligned frame) and every call made with ESP and the registers the original has at that call.
#include "mechanism_kernels.h"

using namespace mech;
using namespace x86;
using reflex_port::OUT_EAX; using reflex_port::OUT_ECX; using reflex_port::OUT_EDX;

// 00859250 FUN_00859250: ECX = inverse of the 3x3 at arg (cofactors / determinant with the exe's masks 0092db80, numerator 00d61650), then ECX's slot 2 (transpose in place)
REFLEX_PORT_FN(0x00859250, Mat3_Invert, "mechanism", OUT_EAX) {
    Guest g{m}; const uint32_t ESP0 = m.r[4], ecx = m.r[1], arg0 = m.arg(0), A = (ESP0 - 0x4) & 0xfffffff0u;
    const V4 L1 = ld(g, arg0 + 0x30);
    const V4 L2 = ld(g, arg0 + 0x20);
    const V4 L3 = ld(g, 0x0092db80);
    const V4 L4 = ld(g, arg0 + 0x10);
    const V4 L5 = ld(g, 0x00d61650);
    const float t1 = flt(bits(sub(mul(L1.f[2], L2.f[1]), mul(L1.f[1], L2.f[2]))) & bits(L3.f[0]));
    const float t2 = flt(bits(sub(mul(L1.f[0], L2.f[2]), mul(L1.f[2], L2.f[0]))) & bits(L3.f[1]));
    const float t3 = flt(bits(sub(mul(L1.f[1], L2.f[0]), mul(L1.f[0], L2.f[1]))) & bits(L3.f[2]));
    const float t4 = add(add(mul(L4.f[0], t1), mul(L4.f[1], t2)), mul(L4.f[2], t3));
    const V4 v1 = div(L5, V4::splat(t4));
    const V4 v2 = mul(perm(L1, 2, 0, 1, 3), perm(L2, 1, 2, 0, 3));
    const V4 v3 = mul(perm(L1, 1, 2, 0, 3), perm(L2, 2, 0, 1, 3));
    const V4 v4 = andps(sub(v2, v3), L3);
    st(g, ecx + 0x10, mul(v1, v4));
    const V4 v5 = mul(perm(L4, 2, 0, 1, 3), perm(L1, 1, 2, 0, 3));
    const V4 v6 = mul(perm(L4, 1, 2, 0, 3), perm(L1, 2, 0, 1, 3));
    st(g, ecx + 0x20, mul(andps(sub(v5, v6), L3), v1));
    const V4 v7 = mul(perm(L2, 2, 0, 1, 3), perm(L4, 1, 2, 0, 3));
    const V4 v8 = mul(perm(L2, 1, 2, 0, 3), perm(L4, 2, 0, 1, 3));
    st(g, ecx + 0x30, mul(andps(sub(v7, v8), L3), v1));
    const uint32_t W6 = g.u32(ecx);
    const uint32_t W7 = g.u32(W6 + 0x8);
    m.r[4] = A - 0x10;
    const uint32_t R8 = callr(g, W7, {ecx}, In().EAX(W6).ECX(ecx).EDX(W7));
    m.r[0] = R8;
    m.r[4] = ESP0;
    return m.ret(0);
}

// 00859340 FUN_00859340: ESI = inverse of the 4x4 at EDI (block form on stack matrices)
REFLEX_PORT_FN(0x00859340, Mat4_Invert, "mechanism", OUT_EAX | OUT_EDX) {
    Guest g{m}; const uint32_t ESP0 = m.r[4], ecx = m.r[1], edx = m.r[2], esi = m.r[6], edi = m.r[7], A = (ESP0 - 0x4) & 0xfffffff0u;
    m.r[4] = A - 0x250;
    callr(g, 0x0042f1d0, {}, In().EAX(A - 0x230).ECX(ecx).EDX(edx));
    m.r[4] = A - 0x250;
    callr(g, 0x0042f1d0, {}, In().EAX(A - 0x120));
    const V4 L3 = ld(g, 0x00d616a0);
    const V4 L4 = ld(g, 0x00d61670);
    const V4 L5 = ld(g, 0x00d615c0);
    st(g, A - 0x140, L3);
    const V4 L6 = ld(g, 0x00d61680);
    st(g, A - 0x130, L6);
    const V4 L7 = ld(g, edi + 0x10);
    st(g, A - 0x210, L7);
    const V4 L8 = ld(g, edi + 0x20);
    st(g, A - 0x200, L8);
    const V4 L9 = ld(g, edi + 0x30);
    st(g, A - 0x1f0, L9);
    const V4 L10 = ld(g, edi + 0x40);
    st(g, A - 0x190, L10);
    const V4 L11 = ld(g, edi + 0x10);
    const V4 L12 = ld(g, edi + 0x20);
    st(g, A - 0x1d0, V4{{L11.f[3], L4.f[1], L4.f[2], L4.f[3]}});
    const V4 L13 = ld(g, edi + 0x30);
    st(g, A - 0x180, L4);
    st(g, A - 0x170, L4);
    st(g, A - 0x1b0, V4{{L13.f[3], L4.f[1], L4.f[2], L4.f[3]}});
    const V4 L14 = ld(g, edi + 0x40);
    st(g, A - 0x1c0, V4{{L12.f[3], L4.f[1], L4.f[2], L4.f[3]}});
    st(g, A - 0x150, V4{{L14.f[3], L5.f[1], L5.f[2], L5.f[3]}});
    m.r[4] = A - 0x250;
    callr(g, 0x008597e0, {A - 0x120}, In().EAX(A - 0x120).ECX(A - 0x230));
    const V4 L16 = ld(g, A - 0x100);
    st(g, esi + 0x10, L16);
    const V4 L17 = ld(g, A - 0xf0);
    st(g, esi + 0x20, L17);
    const V4 L18 = ld(g, A - 0xe0);
    st(g, esi + 0x30, L18);
    const V4 L19 = ld(g, A - 0x80);
    st(g, esi + 0x40, L19);
    const V4 L20 = ld(g, A - 0xc0);
    const V4 L21 = ld(g, esi + 0x10);
    st(g, esi + 0x10, V4{{L21.f[0], L21.f[1], L21.f[2], L20.f[0]}});
    const V4 L22 = ld(g, A - 0xb0);
    const V4 L23 = ld(g, esi + 0x20);
    st(g, esi + 0x20, V4{{L23.f[0], L23.f[1], L23.f[2], L22.f[0]}});
    const V4 L24 = ld(g, A - 0xa0);
    const V4 L25 = ld(g, esi + 0x30);
    st(g, esi + 0x30, V4{{L25.f[0], L25.f[1], L25.f[2], L24.f[0]}});
    const V4 L26 = ld(g, A - 0x40);
    const V4 L27 = ld(g, esi + 0x40);
    st(g, esi + 0x40, V4{{L27.f[0], L27.f[1], L27.f[2], L26.f[0]}});
    m.r[0] = bits(L24.f[0]);
    m.r[2] = bits(L22.f[0]);
    m.r[4] = ESP0;
    return m.ret(0);
}

// 008597e0 FUN_008597e0: arg = inverse of the 6x6 (3x3 blocks) at ECX (block form through 00853740 / 00859250)
REFLEX_PORT_FN(0x008597e0, Mat6_Invert, "mechanism", OUT_EAX) {
    Guest g{m}; const uint32_t ESP0 = m.r[4], ecx = m.r[1], edx = m.r[2], arg0 = m.arg(0), A = (ESP0 - 0x4) & 0xfffffff0u;
    g.u32(A - 0x1cc, 0x3u);
    g.u32(A - 0x1c8, 0x3u);
    g.u32(A - 0x4c, 0x3u);
    g.u32(A - 0x48, 0x3u);
    g.u32(A - 0x8c, 0x3u);
    g.u32(A - 0x88, 0x3u);
    g.u32(A - 0x10c, 0x3u);
    g.u32(A - 0x108, 0x3u);
    g.u32(A - 0x18c, 0x3u);
    g.u32(A - 0x188, 0x3u);
    g.u32(A - 0xcc, 0x3u);
    g.u32(A - 0xc8, 0x3u);
    g.u32(A - 0x14c, 0x3u);
    g.u32(A - 0x148, 0x3u);
    g.u32(A - 0x1d0, 0x93ca04u);
    g.u32(A - 0x50, 0x93ca04u);
    g.u32(A - 0x90, 0x93ca04u);
    g.u32(A - 0x110, 0x93ca04u);
    g.u32(A - 0x190, 0x93ca04u);
    g.u32(A - 0xd0, 0x93ca04u);
    g.u32(A - 0x150, 0x93ca04u);
    m.r[4] = A - 0x1e0;
    const uint32_t R1 = callr(g, 0x00859250, {ecx + 0x10}, In().EAX(ecx + 0x10).ECX(A - 0x1d0).EDX(edx).EBX(arg0).ESI(ecx));
    m.r[4] = A - 0x1e0;
    const uint32_t R2 = callr(g, 0x00853740, {}, In().EAX(R1).ECX(A - 0x1d0).EBX(arg0).ESI(A - 0x50).EDI(ecx + 0x90));
    m.r[4] = A - 0x1e0;
    const uint32_t R3 = callr(g, 0x00853740, {}, In().EAX(R2).ECX(ecx + 0x50).EBX(arg0).ESI(A - 0x90).EDI(A - 0x1d0));
    m.r[4] = A - 0x1e0;
    callr(g, 0x00853740, {}, In().EAX(R3).ECX(A - 0x90).EBX(arg0).ESI(A - 0x110).EDI(ecx + 0x90));
    const V4 L5 = ld(g, ecx + 0xe0);
    const V4 L6 = ld(g, A - 0x100);
    const V4 L7 = ld(g, ecx + 0xf0);
    st(g, A - 0x180, sub(L6, L5));
    const V4 L8 = ld(g, A - 0xf0);
    const V4 L9 = ld(g, ecx + 0x100);
    st(g, A - 0x170, sub(L8, L7));
    const V4 L10 = ld(g, A - 0xe0);
    st(g, A - 0x160, sub(L10, L9));
    m.r[4] = A - 0x1e0;
    const uint32_t R11 = callr(g, 0x00859250, {A - 0x190}, In().EAX(ecx).ECX(A - 0xd0).EBX(arg0).ESI(A - 0x110).EDI(ecx + 0x90));
    m.r[4] = A - 0x1e0;
    const uint32_t R12 = callr(g, 0x00853740, {}, In().EAX(R11).ECX(A - 0xd0).EBX(arg0).ESI(arg0 + 0x50).EDI(A - 0x90));
    m.r[4] = A - 0x1e0;
    const uint32_t R13 = callr(g, 0x00853740, {}, In().EAX(R12).ECX(A - 0x50).EBX(arg0).ESI(arg0 + 0x90).EDI(A - 0xd0));
    m.r[4] = A - 0x1e0;
    callr(g, 0x00853740, {}, In().EAX(R13).ECX(arg0 + 0x90).EBX(arg0).ESI(A - 0x150).EDI(A - 0x90));
    const V4 L15 = ld(g, A - 0x140);
    const V4 L16 = ld(g, A - 0x1c0);
    const V4 L17 = ld(g, A - 0x130);
    st(g, arg0 + 0x20, sub(L16, L15));
    const V4 L18 = ld(g, A - 0x1b0);
    const V4 L19 = ld(g, A - 0x120);
    st(g, arg0 + 0x30, sub(L18, L17));
    const V4 L20 = ld(g, A - 0x1a0);
    st(g, arg0 + 0x40, sub(L20, L19));
    const uint32_t W21 = g.u32(arg0 + 0xd0);
    const uint32_t W22 = g.u32(W21 + 0xc);
    m.r[4] = A - 0x1e0;
    callr(g, W22, {A - 0xd0}, In().EAX(A - 0xd0).ECX(arg0 + 0xd0).EDX(W22).EBX(arg0 + 0xd0).ESI(A - 0x150).EDI(A - 0x90));
    const uint32_t W24 = g.u32(arg0 + 0xd0);
    const uint32_t W25 = g.u32(W24 + 0x18);
    m.r[4] = A - 0x1e0;
    const uint32_t R26 = callr(g, W25, {}, In().EAX(W24).ECX(arg0 + 0xd0).EDX(W25).EBX(arg0 + 0xd0).ESI(A - 0x150).EDI(A - 0x90));
    m.r[0] = R26;
    m.r[4] = ESP0;
    return m.ret(0);
}

// 00854f00 FUN_00854f00: ESI (6x6 of 3x3 blocks) = ECX * arg^T, block by block through 00854650 and the blocks' slot 5
REFLEX_PORT_FN(0x00854f00, Mat6_MulABt, "mechanism", OUT_EAX) {
    Guest g{m}; const uint32_t ESP0 = m.r[4], ecx = m.r[1], esi = m.r[6], arg0 = m.arg(0), A = (ESP0 - 0x4) & 0xfffffff0u;
    g.u32(A - 0x4c, 0x3u);
    g.u32(A - 0x48, 0x3u);
    g.u32(A - 0x50, 0x93ca04u);
    m.r[4] = A - 0x70;
    callr(g, 0x00854650, {arg0 + 0x10}, In().EAX(arg0 + 0x10).ECX(A - 0x50).EDX(ecx + 0x10).EBX(arg0).EDI(ecx));
    m.r[4] = A - 0x70;
    callr(g, 0x00854650, {arg0 + 0x50}, In().EAX(arg0 + 0x50).ECX(esi + 0x10).EDX(ecx + 0x50).EBX(arg0).EDI(ecx));
    const uint32_t W3 = g.u32(esi + 0x10);
    const uint32_t W4 = g.u32(W3 + 0x14);
    m.r[4] = A - 0x70;
    callr(g, W4, {esi + 0x10, A - 0x50}, In().EAX(W4).ECX(esi + 0x10).EDX(A - 0x50).EBX(arg0).EDI(ecx));
    m.r[4] = A - 0x70;
    const uint32_t R6 = callr(g, 0x00854650, {arg0 + 0x90}, In().EAX(arg0 + 0x90).ECX(A - 0x50).EDX(ecx + 0x10).EBX(arg0).EDI(ecx));
    m.r[4] = A - 0x70;
    callr(g, 0x00854650, {arg0 + 0xd0}, In().EAX(R6).ECX(esi + 0x50).EDX(ecx + 0x50).EBX(arg0 + 0xd0).EDI(ecx));
    const uint32_t W8 = g.u32(esi + 0x50);
    const uint32_t W9 = g.u32(W8 + 0x14);
    m.r[4] = A - 0x70;
    callr(g, W9, {esi + 0x50, A - 0x50}, In().EAX(A - 0x50).ECX(esi + 0x50).EDX(W9).EBX(arg0 + 0xd0).EDI(ecx));
    m.r[4] = A - 0x70;
    const uint32_t R11 = callr(g, 0x00854650, {arg0 + 0x10}, In().EAX(arg0 + 0x10).ECX(A - 0x50).EDX(ecx + 0x90).EBX(arg0 + 0xd0).EDI(ecx));
    m.r[4] = A - 0x70;
    callr(g, 0x00854650, {arg0 + 0x50}, In().EAX(R11).ECX(esi + 0x90).EDX(ecx + 0xd0).EBX(arg0 + 0xd0).EDI(ecx + 0xd0));
    const uint32_t W13 = g.u32(esi + 0x90);
    const uint32_t W14 = g.u32(W13 + 0x14);
    m.r[4] = A - 0x70;
    const uint32_t R15 = callr(g, W14, {esi + 0x90, A - 0x50}, In().EAX(W14).ECX(esi + 0x90).EDX(A - 0x50).EBX(arg0 + 0xd0).EDI(ecx + 0xd0));
    m.r[4] = A - 0x70;
    const uint32_t R16 = callr(g, 0x00854650, {arg0 + 0x90}, In().EAX(R15).ECX(A - 0x50).EDX(ecx + 0x90).EBX(arg0 + 0xd0).EDI(ecx + 0xd0));
    m.r[4] = A - 0x70;
    callr(g, 0x00854650, {arg0 + 0xd0}, In().EAX(R16).ECX(esi + 0xd0).EDX(ecx + 0xd0).EBX(arg0 + 0xd0).EDI(ecx + 0xd0));
    const uint32_t W18 = g.u32(esi + 0xd0);
    const uint32_t W19 = g.u32(W18 + 0x14);
    m.r[4] = A - 0x70;
    const uint32_t R20 = callr(g, W19, {esi + 0xd0, A - 0x50}, In().EAX(A - 0x50).ECX(esi + 0xd0).EDX(W19).EBX(arg0 + 0xd0).EDI(ecx + 0xd0));
    m.r[0] = R20;
    m.r[4] = ESP0;
    return m.ret(0);
}

// 00854910 FUN_00854910: 6x6 x 4x6 block products through 00854ac0 and slot 5
REFLEX_PORT_FN(0x00854910, Fn_00854910, "mechanism", OUT_EAX) {
    Guest g{m}; const uint32_t ESP0 = m.r[4], ecx = m.r[1], edx = m.r[2], arg0 = m.arg(0), A = (ESP0 - 0x4) & 0xfffffff0u;
    g.u32(A - 0x15c, 0x4u);
    g.u32(A - 0x10c, 0x4u);
    g.u32(A - 0xbc, 0x4u);
    g.u32(A - 0xac, 0x4u);
    g.u32(A - 0x5c, 0x4u);
    g.u32(A - 0x158, 0x3u);
    g.u32(A - 0x108, 0x3u);
    g.u32(A - 0xa8, 0x3u);
    g.u32(A - 0x58, 0x3u);
    g.u32(A - 0x160, 0x93ca4cu);
    g.u32(A - 0x110, 0x93ca4cu);
    g.u32(A - 0xb0, 0x93ca4cu);
    g.u32(A - 0x60, 0x93ca4cu);
    g.u32(A - 0xb8, 0x6u);
    g.u32(A - 0xc0, 0x93cab8u);
    m.r[4] = A - 0x180;
    const uint32_t R1 = callr(g, 0x00854ac0, {edx + 0x10}, In().EAX(edx + 0x10).ECX(A - 0x160).EDX(arg0 + 0x10).EBX(arg0).ESI(edx).EDI(ecx));
    m.r[4] = A - 0x180;
    callr(g, 0x00854ac0, {edx + 0xd0}, In().EAX(R1).ECX(A - 0x110).EDX(arg0 + 0x60).EBX(arg0).ESI(edx).EDI(ecx));
    m.r[4] = A - 0x180;
    const uint32_t R3 = callr(g, 0x00854ac0, {edx + 0x50}, In().EAX(edx + 0x50).ECX(A - 0xb0).EDX(arg0 + 0x60).EBX(arg0).ESI(edx).EDI(ecx));
    m.r[4] = A - 0x180;
    callr(g, 0x00854ac0, {edx + 0x90}, In().EAX(R3).ECX(A - 0x60).EDX(arg0 + 0x10).EBX(arg0).ESI(edx + 0x90).EDI(ecx));
    const uint32_t W5 = g.u32(A - 0x160);
    const uint32_t W6 = g.u32(W5 + 0x14);
    m.r[4] = A - 0x180;
    callr(g, W6, {ecx + 0x10, A - 0xb0}, In().EAX(W5).ECX(A - 0x160).EDX(W6).EBX(arg0).ESI(edx + 0x90).EDI(ecx));
    const uint32_t W8 = g.u32(A - 0x110);
    const uint32_t W9 = g.u32(W8 + 0x14);
    m.r[4] = A - 0x180;
    const uint32_t R10 = callr(g, W9, {ecx + 0x60, A - 0x60}, In().EAX(W9).ECX(A - 0x110).EDX(W8).EBX(arg0).ESI(edx + 0x90).EDI(ecx + 0x60));
    m.r[0] = R10;
    m.r[4] = ESP0;
    return m.ret(0);
}

// 00855190 FUN_00855190: 4x6 block product through 00855240, added into ESI
REFLEX_PORT_FN(0x00855190, Fn_00855190, "mechanism", OUT_EAX) {
    Guest g{m}; const uint32_t ESP0 = m.r[4], esi = m.r[6], edi = m.r[7], arg0 = m.arg(0), A = (ESP0 - 0x4) & 0xfffffff0u;
    g.u32(A - 0x5c, 0x4u);
    g.u32(A - 0x58, 0x4u);
    g.u32(A - 0x60, 0x93ca70u);
    m.r[4] = A - 0x70;
    const uint32_t R1 = callr(g, 0x00855240, {arg0 + 0x10}, In().EAX(arg0 + 0x10).ECX(esi).EDX(edi + 0x10).EBX(arg0));
    m.r[4] = A - 0x70;
    const uint32_t R2 = callr(g, 0x00855240, {arg0 + 0x60}, In().EAX(R1).ECX(A - 0x60).EDX(edi + 0x60).EBX(arg0 + 0x60));
    const V4 L3 = ld(g, esi + 0x10);
    const V4 L4 = ld(g, A - 0x50);
    const V4 L6 = ld(g, A - 0x40);
    st(g, esi + 0x10, add(L3, L4));
    const V4 L7 = ld(g, esi + 0x20);
    const V4 L8 = ld(g, A - 0x30);
    st(g, esi + 0x20, add(L7, L6));
    const V4 L9 = ld(g, esi + 0x30);
    const V4 L10 = ld(g, A - 0x20);
    st(g, esi + 0x30, add(L9, L8));
    const V4 L11 = ld(g, esi + 0x40);
    st(g, esi + 0x40, add(L11, L10));
    m.r[0] = R2;
    m.r[4] = ESP0;
    return m.ret(0);
}

// 00855090 FUN_00855090: four 3x4 block products through 00855590 (ESI blocks, arg0 / arg1)
REFLEX_PORT_FN(0x00855090, Fn_00855090, "mechanism", OUT_EAX | OUT_ECX) {
    Guest g{m}; const uint32_t ESP0 = m.r[4], eax = m.r[0], esi = m.r[6], arg0 = m.arg(0), arg1 = m.arg(1);
    m.r[4] = ESP0 - 0xc;
    const uint32_t R1 = callr(g, 0x00855590, {arg1 + 0x10}, In().EAX(eax).ECX(esi + 0x10).EDX(arg0 + 0x10).EBX(arg0 + 0x10).EDI(arg1 + 0x10));
    m.r[4] = ESP0 - 0xc;
    const uint32_t R2 = callr(g, 0x00855590, {arg1 + 0x50}, In().EAX(R1).ECX(esi + 0x50).EDX(arg0 + 0x10).EBX(arg0 + 0x10).EDI(arg1 + 0x10));
    m.r[4] = ESP0 - 0xc;
    const uint32_t R3 = callr(g, 0x00855590, {arg1 + 0x10}, In().EAX(R2).ECX(esi + 0x90).EDX(arg0 + 0x50).EBX(arg0 + 0x50).EDI(arg1 + 0x10));
    m.r[4] = ESP0 - 0xc;
    const uint32_t R4 = callr(g, 0x00855590, {arg1 + 0x50}, In().EAX(R3).ECX(esi + 0xd0).EDX(arg0 + 0x50).EBX(arg0 + 0x50).EDI(arg1 + 0x10));
    m.r[0] = R4;
    m.r[1] = esi + 0xd0;
    m.r[4] = ESP0;
    return m.ret(0);
}

// 00854a90 FUN_00854a90: two 3x4 x 4x4 block products through 00854cb0 (EDI, ESI, arg)
REFLEX_PORT_FN(0x00854a90, Fn_00854a90, "mechanism", OUT_EAX | OUT_ECX) {
    Guest g{m}; const uint32_t ESP0 = m.r[4], eax = m.r[0], esi = m.r[6], edi = m.r[7], arg0 = m.arg(0);
    m.r[4] = ESP0 - 0x8;
    const uint32_t R1 = callr(g, 0x00854cb0, {esi}, In().EAX(eax).ECX(arg0 + 0x10).EDX(edi + 0x10).EBX(arg0));
    m.r[4] = ESP0 - 0x8;
    const uint32_t R2 = callr(g, 0x00854cb0, {esi}, In().EAX(R1).ECX(arg0 + 0x50).EDX(edi + 0x50).EBX(arg0));
    m.r[0] = R2;
    m.r[4] = ESP0;
    return m.ret(0);
}
