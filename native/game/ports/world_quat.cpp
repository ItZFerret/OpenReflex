// World group: rotation matrix to quaternion (0x0040xxxx).
#include "../port.h"
#include "world_util.h"

using namespace world;
using reflex_port::OUT_XMM_ALL;

// 00401210 Matrix3_ToQuaternion (cdecl, arg 0 = 3x4 matrix rows at +0, +0x10, +0x20; result xmm0). Branch-free Xenon code:
// the four candidate traces 1 +- m00 +- m11 +- m22 (signs per lane from 0x92dc80 / 0x92dc90 / 0x92dca0), their
// 0.5 * sqrt and 0.5 / sqrt (sqrt(x) computed as x * rsqrt(x) = x / sqrt(x), except where 1/sqrt(x) is 0 or inf), the largest
// trace chosen by compare masks that select byte-permute controls, and the remaining components from sums / differences of the
// off-diagonal terms. Transliterated register by register (all xmm registers end as in the original).
REFLEX_PORT_FN(0x00401210, Matrix3_ToQuaternion, "world", OUT_XMM_ALL | reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX) {
    Guest g{m};
    auto C = [&](uint32_t a) { return ld(g, a); };
    const uint32_t mat = m.arg(0);
    X x0, x1, x2, x3, x4, x5, x6, x7;
    X s00, s10, s20, s30, s40, s50, s60, s70, s80, s90, sa0;
    x0 = ld(g, mat + 0x10); x1 = ld(g, mat); x3 = C(0x92dc90); x2 = ld(g, mat + 0x20); x4 = C(0x92dc40);
    s00 = x1; s20 = x0;
    x0 = shufps(x0, x0, 0x55); x0 = mulps(x0, x3);
    x3 = shufps(x1, x1, 0); x1 = C(0x92dc80); x3 = mulps(x3, x1);
    x1 = shufps(x2, x2, 0xaa); s30 = x2;
    x2 = C(0x92dca0); x1 = mulps(x1, x2);
    x0 = addps(x0, x3);
    x3 = C(0x92dba0);
    x0 = addps(x0, x1);
    x2 = addps(x3, x0);
    x1 = sqrtps(x2);
    x3 = divps(x3, x1);
    x1 = andps(x3, x4);
    x4 = C(0x92dbb0); x1 = pcmpeqd(x1, x4);
    x4 = C(0x92dc20); x5 = andps(x3, x4); x5 = pcmpeqd(x5, x4);
    x1 = andnps(x1, x5);
    x4 = x1; x1 = andps(x1, x2);
    x5 = mulps(x3, x2);
    x4 = andnps(x4, x5); x4 = orps(x4, x1);
    x1 = C(0x92dbd0); x4 = mulps(x4, x1); x1 = mulps(x1, x3);
    s70 = x1;
    x1 = C(0x92dce0);
    s80 = x4; s90 = x0; sa0 = x0; s40 = x1;
    s10 = perm(s90, sa0, s40);
    s50 = x0; x0 = C(0x92dc10); s60 = x0; x0 = C(0x92dcf0); s40 = x0;
    s90 = perm(s50, s60, s40);
    x0 = s10; x1 = s90; x7 = C(0x92dd10);
    x1 = cmpleps(x1, x0);
    x3 = shufps(x1, x1, 0xaa);
    x5 = andnps(x3, C(0x92dd20));
    x6 = andps(x3, x7);
    x5 = orps(x5, x6);
    x7 = C(0x92dcc0);
    x6 = andps(x3, x7);
    x7 = C(0x92dd80);
    x0 = shufps(x1, x1, 0);
    x4 = andnps(x0, x5);
    x5 = andps(C(0x92dd20), x0);
    x4 = orps(x4, x5);
    x5 = andnps(x3, C(0x92dcd0)); x5 = orps(x5, x6);
    s10 = x0;
    x6 = andnps(s10, x5);
    x5 = andps(C(0x92dcd0), x0);
    x6 = orps(x6, x5);
    s40 = x6;
    x6 = andnps(x3, C(0x92dd90));
    x3 = andps(x3, x7);
    x7 = C(0x92dcb0);
    x6 = orps(x6, x3);
    x3 = andps(C(0x92dd90), x0);
    x2 = shufps(x1, x1, 0x55);
    x5 = andnps(x0, x6); x5 = orps(x5, x3);
    x1 = shufps(x1, x1, 0xff);
    x6 = andnps(x2, s40);
    x3 = andps(x2, x7);
    x6 = orps(x6, x3);
    x6 = andps(x6, x0);
    x3 = andnps(x0, s40);
    x6 = orps(x6, x3);
    s10 = x1;
    x3 = andnps(s10, x6);
    x6 = andps(C(0x92dbc0), x1);
    x3 = orps(x3, x6);
    x6 = andps(C(0x92dd70), x2);
    x7 = andnps(x2, x5);
    x6 = orps(x6, x7);
    s40 = x3;
    x6 = andps(x6, x0);
    x7 = andnps(x0, x5);
    x5 = C(0x92dda0);
    x6 = orps(x6, x7);
    x5 = andps(x5, x1);
    x3 = andnps(x1, x6); x3 = orps(x3, x5);
    x5 = s70; s50 = x5; s60 = x5;
    x6 = x2; x5 = x1;
    x6 = andnps(x6, x4);
    x7 = C(0x92dd00);
    x2 = andps(x2, x7);
    x6 = orps(x6, x2);
    x6 = andps(x6, x0);
    x0 = andnps(x0, x4);
    x6 = orps(x6, x0);
    x0 = C(0x92dd30);
    x5 = andnps(x5, x6);
    x0 = andps(x0, x1);
    x5 = orps(x5, x0);
    s70 = x5;
    s10 = perm(s50, s60, s70);
    s50 = s20; s60 = s30; s30 = C(0x92dd40);
    s20 = perm(s50, s60, s30);
    x1 = s00; x0 = s20; x2 = C(0x92dd50);
    s50 = x1; s60 = x0; s00 = x2;
    s20 = perm(s50, s60, s00);
    s60 = x0; x0 = C(0x92dd60); s50 = x1; s00 = x0;
    s30 = perm(s50, s60, s00);
    x0 = s80; x1 = s20;
    s50 = x0;
    x0 = mulps(s30, s40); x0 = addps(x0, x1);
    x1 = s10;
    x0 = mulps(x0, x1);
    s60 = x0;
    s80 = x3;
    const uint8_t last = perm_last_byte(s50, s60, s80);
    s00 = perm(s50, s60, s80);
    x0 = s00;
    (void)sa0;
    // the permute loops leave EAX = 16 and DL = the last byte copied; ECX = the cookie (__security_check_cookie's argument)
    m.r[0] = 0x10; m.r[2] = (m.r[2] & 0xffffff00u) | last; m.r[1] = g.u32(0x009b45e4);
    xw(g, 0, x0); xw(g, 1, x1); xw(g, 2, x2); xw(g, 3, x3); xw(g, 4, x4); xw(g, 5, x5); xw(g, 6, x6); xw(g, 7, x7);
    return m.ret(0);
}

// 00401740 (Matrix3_ToQuaternion into [ESI]): cdecl arg 0 = matrix, ESI = 16-byte output; xmm registers as the callee leaves them.
REFLEX_PORT_FN(0x00401740, Matrix3_ToQuaternionToEsi, "world", OUT_XMM_ALL | reflex_port::OUT_EAX | reflex_port::OUT_ECX | reflex_port::OUT_EDX) {
    Guest g{m};
    Frame f(m, 0, 0);
    f.call(0x00401210, {m.arg(0)});
    st(g, m.r[6], xr(g, 0));
    return m.ret(0);
}

// 0041ad80 (va_arg-style cursor advance): ECX = pointer to a cursor; EAX = the old cursor, the cursor += 4, EDX = the new one.
REFLEX_PORT_FN(0x0041ad80, Cursor_Advance4, "world", reflex_port::OUT_EAX | reflex_port::OUT_EDX) {
    Guest g{m};
    const uint32_t c = m.r[1], old = g.u32(c);
    g.u32(c, old + 4);
    m.r[2] = old + 4;
    return m.ret(0, old);
}
