// Vector / quaternion helpers of the 0x0042xxxx range with memory operands, transliterated instruction by instruction (the
// original's instruction in each line's comment): every xmm register and EAX / ECX / EDX end as in the original, memory is
// read and written in the original's order (inputs may alias outputs).
#include "math_util.h"

using namespace x86;
using namespace mathx;
using reflex_port::OUT_EAX;

// 0042db20 (ECX, EDX = vec4 pointers, stack: object, two vec4 pointers, a result pointer; stdcall 0x10): fills the object's
// +0x30..+0x90 and +0x120..+0x140 from a rotation (the global quaternion at 0xd615c0 composed with *arg2), a position
// difference normalized to the length at +0x10, and cross products (NaN where the length is zero or infinite).
REFLEX_PORT_FN(0x0042db20, Fn_0042db20, "math", OUT_XMM | OUT_GPR) {
    Regs r(m);
    [[maybe_unused]] X &x0 = r[0], &x1 = r[1], &x2 = r[2], &x3 = r[3], &x4 = r[4], &x5 = r[5], &x6 = r[6], &x7 = r[7];
    [[maybe_unused]] uint32_t eax = m.r[0], ecx = m.r[1], edx = m.r[2], ebx = m.r[3], esi = m.r[6], edi = m.r[7];
    [[maybe_unused]] const uint32_t E = m.r[4], ebp = E - 4;
    [[maybe_unused]] Cmp c{};
    // push ebp                                                    // 42db20 push ebp
    // mov ebp, esp                                                // 42db21 mov ebp, esp
    // frame: align                                                // 42db23 and esp, 0xfffffff0
    eax = ld32(m, ebp + 0x8);                                      // 42db26 mov eax, [ebp + 8]
    x0 = ld(m, edx);                                               // 42db29 movaps xmm0, [edx]
    st(m, eax + 0x40, x0);                                         // 42db2c movaps [eax + 0x40], xmm0
    x2 = ld(m, 0xd615c0);                                          // 42db30 movaps xmm2, [0xd615c0]
    x0 = X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000);     // 42db37 movaps xmm0, [0x92db80]
    edx = ld32(m, ebp + 0x10);                                     // 42db3e mov edx, [ebp + 0x10]
    x1 = ld(m, edx);                                               // 42db41 movaps xmm1, [edx]
    x0 = andps(x0, x2);                                            // 42db44 andps xmm0, xmm2
    x2 = X::v(0xbf800000, 0xbf800000, 0xbf800000, 0x3f800000);     // 42db47 movaps xmm2, [0x93a220]
    x7 = X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000);     // 42db4e movaps xmm7, [0x93a200]
    x3 = x0;                                                       // 42db55 movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0x00);                                     // 42db58 shufps xmm3, xmm0, 0
    x4 = x0;                                                       // 42db5c movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x55);                                     // 42db5f shufps xmm4, xmm0, 0x55
    x6 = x0;                                                       // 42db63 movaps xmm6, xmm0
    x6 = shufps(x6, x0, 0xaa);                                     // 42db66 shufps xmm6, xmm0, 0xaa
    x5 = x1;                                                       // 42db6a movaps xmm5, xmm1
    x5 = mulps(x5, x2);                                            // 42db6d mulps xmm5, xmm2
    x2 = x5;                                                       // 42db70 movaps xmm2, xmm5
    x2 = shufps(x2, x5, 0x1b);                                     // 42db73 shufps xmm2, xmm5, 0x1b
    x3 = mulps(x3, x2);                                            // 42db77 mulps xmm3, xmm2
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 42db7a mulps xmm3, [0x93a1f0]
    edx = ld32(m, ebp + 0xc);                                      // 42db81 mov edx, [ebp + 0xc]
    x2 = shufps(x2, x2, 0xb1);                                     // 42db84 shufps xmm2, xmm2, 0xb1
    x4 = mulps(x4, x2);                                            // 42db88 mulps xmm4, xmm2
    x4 = mulps(x4, x7);                                            // 42db8b mulps xmm4, xmm7
    x2 = shufps(x2, x2, 0x1b);                                     // 42db8e shufps xmm2, xmm2, 0x1b
    x2 = mulps(x2, x6);                                            // 42db92 mulps xmm2, xmm6
    x6 = X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000);     // 42db95 movaps xmm6, [0x93a210]
    x2 = mulps(x2, x6);                                            // 42db9c mulps xmm2, xmm6
    x2 = addps(x2, x4);                                            // 42db9f addps xmm2, xmm4
    x4 = x0;                                                       // 42dba2 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0xff);                                     // 42dba5 shufps xmm4, xmm0, 0xff
    x4 = mulps(x4, x5);                                            // 42dba9 mulps xmm4, xmm5
    x4 = addps(x4, x3);                                            // 42dbac addps xmm4, xmm3
    x3 = x1;                                                       // 42dbaf movaps xmm3, xmm1
    x3 = shufps(x3, x1, 0x00);                                     // 42dbb2 shufps xmm3, xmm1, 0
    x2 = addps(x2, x4);                                            // 42dbb6 addps xmm2, xmm4
    x4 = x1;                                                       // 42dbb9 movaps xmm4, xmm1
    x4 = shufps(x4, x1, 0x55);                                     // 42dbbc shufps xmm4, xmm1, 0x55
    x0 = x2;                                                       // 42dbc0 movaps xmm0, xmm2
    x0 = shufps(x0, x2, 0x1b);                                     // 42dbc3 shufps xmm0, xmm2, 0x1b
    x3 = mulps(x3, x0);                                            // 42dbc7 mulps xmm3, xmm0
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 42dbca mulps xmm3, [0x93a1f0]
    x0 = shufps(x0, x0, 0xb1);                                     // 42dbd1 shufps xmm0, xmm0, 0xb1
    x5 = x0;                                                       // 42dbd5 movaps xmm5, xmm0
    x5 = shufps(x5, x0, 0x1b);                                     // 42dbd8 shufps xmm5, xmm0, 0x1b
    x4 = mulps(x4, x0);                                            // 42dbdc mulps xmm4, xmm0
    x0 = x1;                                                       // 42dbdf movaps xmm0, xmm1
    x0 = shufps(x0, x1, 0xaa);                                     // 42dbe2 shufps xmm0, xmm1, 0xaa
    x5 = mulps(x5, x0);                                            // 42dbe6 mulps xmm5, xmm0
    x0 = x1;                                                       // 42dbe9 movaps xmm0, xmm1
    x0 = shufps(x0, x1, 0xff);                                     // 42dbec shufps xmm0, xmm1, 0xff
    x0 = mulps(x0, x2);                                            // 42dbf0 mulps xmm0, xmm2
    x0 = addps(x0, x3);                                            // 42dbf3 addps xmm0, xmm3
    x5 = mulps(x5, x6);                                            // 42dbf6 mulps xmm5, xmm6
    x4 = mulps(x4, x7);                                            // 42dbf9 mulps xmm4, xmm7
    x5 = addps(x5, x4);                                            // 42dbfc addps xmm5, xmm4
    x5 = addps(x5, x0);                                            // 42dbff addps xmm5, xmm0
    st(m, eax + 0x30, x5);                                         // 42dc02 movaps [eax + 0x30], xmm5
    x0 = ld(m, edx);                                               // 42dc06 movaps xmm0, [edx]
    st(m, eax + 0x50, x0);                                         // 42dc09 movaps [eax + 0x50], xmm0
    x0 = ld(m, ecx);                                               // 42dc0d movaps xmm0, [ecx]
    x1 = ld(m, eax + 0x50);                                        // 42dc10 movaps xmm1, [eax + 0x50]
    x1 = subps(x1, x0);                                            // 42dc14 subps xmm1, xmm0
    st(m, eax + 0x70, x1);                                         // 42dc17 movaps [eax + 0x70], xmm1
    x1 = ld(m, eax + 0x70);                                        // 42dc1b movaps xmm1, [eax + 0x70]
    x1 = mulps(x1, x1);                                            // 42dc1f mulps xmm1, xmm1
    x0 = x1;                                                       // 42dc22 movaps xmm0, xmm1
    x0 = shufps(x0, x1, 0x66);                                     // 42dc25 shufps xmm0, xmm1, 0x66
    x1 = addss(x1, x0);                                            // 42dc29 addss xmm1, xmm0
    x0 = shufps(x0, x0, 0x55);                                     // 42dc2d shufps xmm0, xmm0, 0x55
    x1 = addss(x1, x0);                                            // 42dc31 addss xmm1, xmm0
    x0 = x1;                                                       // 42dc35 movaps xmm0, xmm1
    x1 = ld(m, eax + 0x10);                                        // 42dc38 movaps xmm1, [eax + 0x10]
    x0 = shufps(x0, x0, 0x00);                                     // 42dc3c shufps xmm0, xmm0, 0
    x0 = sqrtps(x0);                                               // 42dc40 sqrtps xmm0, xmm0
    x1 = divps(x1, x0);                                            // 42dc43 divps xmm1, xmm0
    x0 = ld(m, eax + 0x70);                                        // 42dc46 movaps xmm0, [eax + 0x70]
    x1 = mulps(x1, x0);                                            // 42dc4a mulps xmm1, xmm0
    st(m, eax + 0x70, x1);                                         // 42dc4d movaps [eax + 0x70], xmm1
    x1 = ld(m, ecx);                                               // 42dc51 movaps xmm1, [ecx]
    x0 = ld(m, eax + 0x70);                                        // 42dc54 movaps xmm0, [eax + 0x70]
    x0 = addps(x0, x1);                                            // 42dc58 addps xmm0, xmm1
    st(m, eax + 0x60, x0);                                         // 42dc5b movaps [eax + 0x60], xmm0
    x0 = ld(m, eax + 0x40);                                        // 42dc5f movaps xmm0, [eax + 0x40]
    st(m, eax + 0x130, x0);                                        // 42dc63 movaps [eax + 0x130], xmm0
    x0 = ld(m, eax + 0x130);                                       // 42dc6a movaps xmm0, [eax + 0x130]
    x1 = ld(m, eax + 0x30);                                        // 42dc71 movaps xmm1, [eax + 0x30]
    x1 = shufps(x1, x1, 0xc9);                                     // 42dc75 shufps xmm1, xmm1, 0xc9
    x3 = x1;                                                       // 42dc79 movaps xmm3, xmm1
    x3 = shufps(x3, x1, 0xc9);                                     // 42dc7c shufps xmm3, xmm1, 0xc9
    x0 = shufps(x0, x0, 0xd2);                                     // 42dc80 shufps xmm0, xmm0, 0xd2
    x2 = x0;                                                       // 42dc84 movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0xd2);                                     // 42dc87 shufps xmm2, xmm0, 0xd2
    x0 = mulps(x0, x1);                                            // 42dc8b mulps xmm0, xmm1
    x1 = X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000);     // 42dc8e movaps xmm1, [0x92db80]
    x2 = mulps(x2, x3);                                            // 42dc95 mulps xmm2, xmm3
    x0 = subps(x0, x2);                                            // 42dc98 subps xmm0, xmm2
    x0 = andps(x0, x1);                                            // 42dc9b andps xmm0, xmm1
    st(m, eax + 0x120, x0);                                        // 42dc9e movaps [eax + 0x120], xmm0
    x0 = ld(m, eax + 0x120);                                       // 42dca5 movaps xmm0, [eax + 0x120]
    x3 = X::v(0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000);     // 42dcac movaps xmm3, [0x92dc20]
    x1 = x0;                                                       // 42dcb3 movaps xmm1, xmm0
    x1 = mulps(x1, x0);                                            // 42dcb6 mulps xmm1, xmm0
    x2 = x1;                                                       // 42dcb9 movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0x99);                                     // 42dcbc shufps xmm2, xmm1, 0x99
    x1 = addss(x1, x2);                                            // 42dcc0 addss xmm1, xmm2
    ecx = ld32(m, ebp + 0x14);                                     // 42dcc4 mov ecx, [ebp + 0x14]
    x2 = shufps(x2, x2, 0x55);                                     // 42dcc7 shufps xmm2, xmm2, 0x55
    x1 = addss(x1, x2);                                            // 42dccb addss xmm1, xmm2
    x1 = shufps(x1, x1, 0x00);                                     // 42dccf shufps xmm1, xmm1, 0
    x2 = sqrtps(x1);                                               // 42dcd3 sqrtps xmm2, xmm1
    x1 = cmpneqps(x1, x3);                                         // 42dcd6 cmpneqps xmm1, xmm3
    x0 = divps(x0, x2);                                            // 42dcda divps xmm0, xmm2
    x3 = X{};                                                      // 42dcdd xorps xmm3, xmm3
    x2 = cmpneqps(x2, x3);                                         // 42dce0 cmpneqps xmm2, xmm3
    x0 = andps(x0, x2);                                            // 42dce4 andps xmm0, xmm2
    x2 = X::v(0x7fc00000, 0x7fc00000, 0x7fc00000, 0x7fc00000);     // 42dce7 movaps xmm2, [0x92dc30]
    x0 = andps(x0, x1);                                            // 42dcee andps xmm0, xmm1
    x1 = andnps(x1, x2);                                           // 42dcf1 andnps xmm1, xmm2
    x0 = orps(x0, x1);                                             // 42dcf4 orps xmm0, xmm1
    st(m, eax + 0x120, x0);                                        // 42dcf7 movaps [eax + 0x120], xmm0
    x0 = ld(m, eax + 0x120);                                       // 42dcfe movaps xmm0, [eax + 0x120]
    x1 = ld(m, eax + 0x130);                                       // 42dd05 movaps xmm1, [eax + 0x130]
    x1 = shufps(x1, x1, 0xc9);                                     // 42dd0c shufps xmm1, xmm1, 0xc9
    x3 = x1;                                                       // 42dd10 movaps xmm3, xmm1
    x3 = shufps(x3, x1, 0xc9);                                     // 42dd13 shufps xmm3, xmm1, 0xc9
    x0 = shufps(x0, x0, 0xd2);                                     // 42dd17 shufps xmm0, xmm0, 0xd2
    x2 = x0;                                                       // 42dd1b movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0xd2);                                     // 42dd1e shufps xmm2, xmm0, 0xd2
    x0 = mulps(x0, x1);                                            // 42dd22 mulps xmm0, xmm1
    x1 = X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000);     // 42dd25 movaps xmm1, [0x92db80]
    x2 = mulps(x2, x3);                                            // 42dd2c mulps xmm2, xmm3
    x0 = subps(x0, x2);                                            // 42dd2f subps xmm0, xmm2
    x0 = andps(x0, x1);                                            // 42dd32 andps xmm0, xmm1
    st(m, eax + 0x140, x0);                                        // 42dd35 movaps [eax + 0x140], xmm0
    x2 = ld(m, ecx);                                               // 42dd3c movaps xmm2, [ecx]
    x1 = X::v(0xffffffff, 0xffffffff, 0xffffffff, 0x00000000);     // 42dd3f movaps xmm1, [0x92db80]
    x0 = ld(m, eax + 0x70);                                        // 42dd46 movaps xmm0, [eax + 0x70]
    x6 = X::v(0x3f800000, 0x3f800000, 0xbf800000, 0xbf800000);     // 42dd4a movaps xmm6, [0x93a200]
    x0 = andps(x0, x1);                                            // 42dd51 andps xmm0, xmm1
    x1 = x2;                                                       // 42dd54 movaps xmm1, xmm2
    x1 = shufps(x1, x2, 0x1b);                                     // 42dd57 shufps xmm1, xmm2, 0x1b
    x3 = x0;                                                       // 42dd5b movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0x00);                                     // 42dd5e shufps xmm3, xmm0, 0
    x3 = mulps(x3, x1);                                            // 42dd62 mulps xmm3, xmm1
    x1 = shufps(x1, x1, 0xb1);                                     // 42dd65 shufps xmm1, xmm1, 0xb1
    x4 = x0;                                                       // 42dd69 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x55);                                     // 42dd6c shufps xmm4, xmm0, 0x55
    x4 = mulps(x4, x1);                                            // 42dd70 mulps xmm4, xmm1
    x1 = shufps(x1, x1, 0x1b);                                     // 42dd73 shufps xmm1, xmm1, 0x1b
    x5 = x0;                                                       // 42dd77 movaps xmm5, xmm0
    x5 = shufps(x5, x0, 0xaa);                                     // 42dd7a shufps xmm5, xmm0, 0xaa
    x1 = mulps(x1, x5);                                            // 42dd7e mulps xmm1, xmm5
    x5 = X::v(0xbf800000, 0x3f800000, 0x3f800000, 0xbf800000);     // 42dd81 movaps xmm5, [0x93a210]
    x4 = mulps(x4, x6);                                            // 42dd88 mulps xmm4, xmm6
    x1 = mulps(x1, x5);                                            // 42dd8b mulps xmm1, xmm5
    x1 = addps(x1, x4);                                            // 42dd8e addps xmm1, xmm4
    x4 = x0;                                                       // 42dd91 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0xff);                                     // 42dd94 shufps xmm4, xmm0, 0xff
    x4 = mulps(x4, x2);                                            // 42dd98 mulps xmm4, xmm2
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 42dd9b mulps xmm3, [0x93a1f0]
    x0 = X::v(0xbf800000, 0xbf800000, 0xbf800000, 0x3f800000);     // 42dda2 movaps xmm0, [0x93a220]
    x4 = addps(x4, x3);                                            // 42dda9 addps xmm4, xmm3
    x2 = mulps(x2, x0);                                            // 42ddac mulps xmm2, xmm0
    x0 = x2;                                                       // 42ddaf movaps xmm0, xmm2
    x1 = addps(x1, x4);                                            // 42ddb2 addps xmm1, xmm4
    x2 = x1;                                                       // 42ddb5 movaps xmm2, xmm1
    x2 = shufps(x2, x1, 0x1b);                                     // 42ddb8 shufps xmm2, xmm1, 0x1b
    x3 = x0;                                                       // 42ddbc movaps xmm3, xmm0
    x3 = shufps(x3, x0, 0x00);                                     // 42ddbf shufps xmm3, xmm0, 0
    x3 = mulps(x3, x2);                                            // 42ddc3 mulps xmm3, xmm2
    x3 = mulps(x3, X::v(0x3f800000, 0xbf800000, 0x3f800000, 0xbf800000)); // 42ddc6 mulps xmm3, [0x93a1f0]
    x2 = shufps(x2, x2, 0xb1);                                     // 42ddcd shufps xmm2, xmm2, 0xb1
    x4 = x0;                                                       // 42ddd1 movaps xmm4, xmm0
    x4 = shufps(x4, x0, 0x55);                                     // 42ddd4 shufps xmm4, xmm0, 0x55
    x4 = mulps(x4, x2);                                            // 42ddd8 mulps xmm4, xmm2
    x2 = shufps(x2, x2, 0x1b);                                     // 42dddb shufps xmm2, xmm2, 0x1b
    x7 = x0;                                                       // 42dddf movaps xmm7, xmm0
    x7 = shufps(x7, x0, 0xaa);                                     // 42dde2 shufps xmm7, xmm0, 0xaa
    x7 = mulps(x7, x2);                                            // 42dde6 mulps xmm7, xmm2
    x2 = x0;                                                       // 42dde9 movaps xmm2, xmm0
    x2 = shufps(x2, x0, 0xff);                                     // 42ddec shufps xmm2, xmm0, 0xff
    x7 = mulps(x7, x5);                                            // 42ddf0 mulps xmm7, xmm5
    x4 = mulps(x4, x6);                                            // 42ddf3 mulps xmm4, xmm6
    x2 = mulps(x2, x1);                                            // 42ddf6 mulps xmm2, xmm1
    x7 = addps(x7, x4);                                            // 42ddf9 addps xmm7, xmm4
    x2 = addps(x2, x3);                                            // 42ddfc addps xmm2, xmm3
    x7 = addps(x7, x2);                                            // 42ddff addps xmm7, xmm2
    st(m, eax + 0x90, x7);                                         // 42de02 movaps [eax + 0x90], xmm7
    // mov esp, ebp                                                // 42de09 mov esp, ebp
    // pop ebp                                                     // 42de0b pop ebp
    // ret 0x10                                                    // 42de0c ret 0x10
    r.store(m);
    m.r[0] = eax; m.r[1] = ecx; m.r[2] = edx;
    return m.ret(0x10);
}
