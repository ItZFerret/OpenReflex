// Collision dispatch and the triangle batch test in the mechanism group's address range.
// Generated from the original instructions by symbolic translation (scratchpad sym3.py): each path of the original's
// branches and jump tables is an if / switch arm; guest loads, stores and calls keep their program order, stack locals
// live at the original's own addresses (ESP0 = entry ESP, A = the aligned frame) and calls run with the original's ESP and
// registers.
#include <stdexcept>
#include "mechanism_kernels.h"

using namespace mech;
using namespace x86;
using reflex_port::OUT_EAX; using reflex_port::OUT_ECX; using reflex_port::OUT_EDX;

// 0085ddd0 FUN_0085ddd0: contact generation between two collision shapes: checks the pair masks (+8..+14 of both), then dispatches on the two shape type flags (2 .. 0x8000) to the pair routine with its register / stack convention; returns its EAX
REFLEX_PORT_FN(0x0085ddd0, Collision_ContactDispatch, "mechanism", OUT_EAX | OUT_ECX | OUT_EDX) {
    Guest g{m}; const uint32_t ESP0 = m.r[4], ecx = m.r[1], arg0 = m.arg(0), arg1 = m.arg(1), arg2 = m.arg(2), arg3 = m.arg(3), arg4 = m.arg(4), arg5 = m.arg(5), arg6 = m.arg(6), A = (ESP0 - 0x4) & 0xfffffff8u;
    (void)ESP0;
    const uint32_t W1 = g.u32(arg0 + 0xc);
    const uint32_t W2 = g.u32(arg3 + 0x8);
    const uint32_t W3 = g.u32(arg3 + 0xc);
    const uint32_t W4 = g.u32(arg0 + 0x8);
    const uint32_t i1 = uint32_t(W3 & W4);
    const uint32_t i2 = uint32_t(uint32_t(W1 & W2) | i1);
    if (i2 != 0x0u) {
        m.r[0] = arg0;
        m.r[1] = W4;
        m.r[2] = i1;
        m.r[4] = ESP0;
        return m.ret(28);
    } else {
        const uint32_t W5 = g.u32(arg0 + 0x14);
        const uint32_t W6 = g.u32(arg3 + 0x10);
        if (uint32_t(W6 & W5) != 0x0u) {
            m.r[0] = arg0;
            m.r[1] = W4;
            m.r[2] = W5;
            m.r[4] = ESP0;
            return m.ret(28);
        } else {
            const uint32_t W7 = g.u32(arg0 + 0x10);
            const uint32_t W8 = g.u32(arg3 + 0x14);
            if (uint32_t(W8 & W7) != 0x0u) {
                m.r[0] = arg0;
                m.r[1] = W4;
                m.r[2] = W7;
                m.r[4] = ESP0;
                return m.ret(28);
            } else {
                if (int32_t(W4) > int32_t(0x40u)) {
                    if (int32_t(W4) > int32_t(0x200u)) {
                        if (W4 == 0x400u) {
                            const uint32_t W9 = g.u32(arg3 + 0x8);
                            if (int32_t(W9) > int32_t(0x40u)) {
                                if (int32_t(W9) <= int32_t(0x200u)) {
                                    m.r[0] = arg0;
                                    m.r[1] = W9;
                                    m.r[2] = W7;
                                    m.r[4] = ESP0;
                                    return m.ret(28);
                                } else {
                                    if (W9 == 0x400u) {
                                        m.r[0] = arg0;
                                        m.r[1] = W9;
                                        m.r[2] = W7;
                                        m.r[4] = ESP0;
                                        return m.ret(28);
                                    } else {
                                        if (W9 != 0x8000u) {
                                            m.r[0] = arg0;
                                            m.r[1] = W9;
                                            m.r[2] = W7;
                                            m.r[4] = ESP0;
                                            return m.ret(28);
                                        } else {
                                            m.r[4] = A - 0x10;
                                            const uint32_t R10 = callr(g, 0x00872860, {ecx, arg0, arg3, arg4, arg5, arg6}, In().EAX(arg0).ECX(arg1).EDX(arg2).EBX(i2).ESI(arg3).EDI(ecx));
                                            m.r[0] = R10;
                                            m.r[4] = ESP0;
                                            return m.ret(28);
                                        }
                                    }
                                }
                            } else {
                                if (W9 == 0x40u) {
                                    m.r[0] = arg0;
                                    m.r[1] = W9;
                                    m.r[2] = W7;
                                    m.r[4] = ESP0;
                                    return m.ret(28);
                                } else {
                                    const uint32_t i3 = uint32_t(W9 - 0x2u);
                                    if (i3 > 0x1eu) {
                                        m.r[0] = arg0;
                                        m.r[1] = i3;
                                        m.r[2] = W7;
                                        m.r[4] = ESP0;
                                        return m.ret(28);
                                    } else {
                                        switch (i3) {
                                            case 0x0: {
                                                const uint32_t W11 = g.u8(ESP0 + 0x1c);
                                                const uint32_t i4 = uint32_t(i3 & 0xffffff00u);
                                                m.r[4] = A - 0x10;
                                                const uint32_t R12 = callr(g, 0x008650c0, {ecx, arg3, arg5, arg0, arg2}, In().EAX(arg5).ECX(uint32_t(i4 | uint32_t(uint32_t(W11 == 0x0u ? 1 : 0) & 0xffu))).EDX(arg1).EBX(i2).ESI(arg3).EDI(ecx));
                                                m.r[0] = R12;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            }
                                            case 0x1: case 0x3: case 0x4: case 0x5: case 0x7: case 0x8: case 0x9: case 0xa: case 0xb: case 0xc: case 0xd: case 0xf: case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: {
                                                m.r[0] = arg0;
                                                m.r[1] = i3;
                                                m.r[2] = uint32_t(g.u8(0x0085ee4c + i3));
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            }
                                            case 0x2: {
                                                const uint32_t W13 = g.u8(ESP0 + 0x1c);
                                                const uint32_t i5 = uint32_t(i3 & 0xffffff00u);
                                                m.r[4] = A - 0x10;
                                                const uint32_t R14 = callr(g, 0x0086b7f0, {ecx, arg3, arg4, arg5, arg0, uint32_t(uint32_t(i5 | uint32_t(uint32_t(W13 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg5).ECX(arg1).EDX(arg2).EBX(i2).ESI(arg3).EDI(ecx));
                                                m.r[0] = R14;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            }
                                            case 0x6: {
                                                const uint32_t W15 = g.u8(ESP0 + 0x1c);
                                                m.r[4] = A - 0x10;
                                                const uint32_t R16 = callr(g, 0x0086f3d0, {ecx, arg3, arg5, arg0, arg1, uint32_t(uint32_t(uint32_t(uint32_t(g.u8(0x0085ee4c + i3)) & 0xffffff00u) | uint32_t(uint32_t(W15 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg5).ECX(arg2).EDX(arg4).EBX(i2).ESI(arg3).EDI(ecx));
                                                m.r[0] = R16;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            }
                                            case 0xe: {
                                                const uint32_t W17 = g.u8(ESP0 + 0x1c);
                                                const uint32_t i6 = uint32_t(i3 & 0xffffff00u);
                                                m.r[4] = A - 0x10;
                                                const uint32_t R18 = callr(g, 0x008716b0, {ecx, arg4, arg5, arg0, arg2, uint32_t(uint32_t(i6 | uint32_t(uint32_t(W17 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg4).ECX(arg1).EDX(arg3).EBX(i2).ESI(arg3).EDI(ecx));
                                                m.r[0] = R18;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            }
                                            default: break;
                                        }
                                        throw std::logic_error("0x0 switch value outside the table");
                                    }
                                }
                            }
                        } else {
                            if (W4 != 0x8000u) {
                                m.r[0] = arg0;
                                m.r[1] = W4;
                                m.r[2] = W7;
                                m.r[4] = ESP0;
                                return m.ret(28);
                            } else {
                                const uint32_t W19 = g.u32(arg3 + 0x8);
                                if (int32_t(W19) > int32_t(0x40u)) {
                                    if (int32_t(W19) > int32_t(0x200u)) {
                                        if (W19 == 0x400u) {
                                            const uint32_t W20 = g.u8(ESP0 + 0x1c);
                                            const uint32_t i7 = uint32_t(W7 & 0xffffff00u);
                                            m.r[4] = A - 0x10;
                                            const uint32_t R21 = callr(g, 0x00872860, {ecx, arg3, arg0, arg1, arg2, uint32_t(uint32_t(i7 | uint32_t(uint32_t(W20 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg0).ECX(arg4).EDX(arg5).EBX(i2).ESI(arg3).EDI(ecx));
                                            m.r[0] = R21;
                                            m.r[4] = ESP0;
                                            return m.ret(28);
                                        } else {
                                            if (W19 != 0x8000u) {
                                                m.r[0] = arg0;
                                                m.r[1] = W19;
                                                m.r[2] = W7;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            } else {
                                                m.r[4] = A - 0x10;
                                                const uint32_t R22 = callr(g, 0x008732d0, {arg0, arg1, arg2, arg3, arg4, arg5, arg6}, In().EAX(arg0).ECX(ecx).EDX(arg2).EBX(i2).ESI(arg3).EDI(ecx));
                                                m.r[0] = R22;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            }
                                        }
                                    } else {
                                        if (W19 == 0x200u) {
                                            const uint32_t W23 = g.u8(ESP0 + 0x1c);
                                            const uint32_t i8 = uint32_t(W7 & 0xffffff00u);
                                            m.r[4] = A - 0x10;
                                            const uint32_t R24 = callr(g, 0x00873010, {ecx, arg5, arg0, arg1, arg2, uint32_t(uint32_t(i8 | uint32_t(uint32_t(W23 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg0).ECX(arg3).EDX(arg4).EBX(i2).ESI(arg3).EDI(ecx));
                                            m.r[0] = R24;
                                            m.r[4] = ESP0;
                                            return m.ret(28);
                                        } else {
                                            if (W19 == 0x80u) {
                                                const uint32_t W25 = g.u8(ESP0 + 0x1c);
                                                const uint32_t i9 = uint32_t(W7 & 0xffffff00u);
                                                m.r[4] = A - 0x10;
                                                const uint32_t R26 = callr(g, 0x00872860, {ecx, arg3, arg0, arg1, arg2, uint32_t(uint32_t(i9 | uint32_t(uint32_t(W25 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg0).ECX(arg4).EDX(arg5).EBX(i2).ESI(arg3).EDI(ecx));
                                                m.r[0] = R26;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            } else {
                                                if (W19 != 0x100u) {
                                                    m.r[0] = arg0;
                                                    m.r[1] = W19;
                                                    m.r[2] = W7;
                                                    m.r[4] = ESP0;
                                                    return m.ret(28);
                                                } else {
                                                    const uint32_t W27 = g.u8(ESP0 + 0x1c);
                                                    const uint32_t i10 = uint32_t(W7 & 0xffffff00u);
                                                    m.r[4] = A - 0x10;
                                                    const uint32_t R28 = callr(g, 0x00872860, {ecx, arg3, arg0, arg1, arg2, uint32_t(uint32_t(i10 | uint32_t(uint32_t(W27 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg0).ECX(arg4).EDX(arg5).EBX(i2).ESI(arg3).EDI(ecx));
                                                    m.r[0] = R28;
                                                    m.r[4] = ESP0;
                                                    return m.ret(28);
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    if (W19 == 0x40u) {
                                        const uint32_t W29 = g.u8(ESP0 + 0x1c);
                                        const uint32_t i11 = uint32_t(W7 & 0xffffff00u);
                                        m.r[4] = A - 0x10;
                                        const uint32_t R30 = callr(g, 0x00872860, {ecx, arg3, arg0, arg1, arg2, uint32_t(uint32_t(i11 | uint32_t(uint32_t(W29 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg0).ECX(arg4).EDX(arg5).EBX(i2).ESI(arg3).EDI(ecx));
                                        m.r[0] = R30;
                                        m.r[4] = ESP0;
                                        return m.ret(28);
                                    } else {
                                        const uint32_t i12 = uint32_t(W19 - 0x2u);
                                        if (i12 > 0x1eu) {
                                            m.r[0] = arg0;
                                            m.r[1] = i12;
                                            m.r[2] = W7;
                                            m.r[4] = ESP0;
                                            return m.ret(28);
                                        } else {
                                            switch (i12) {
                                                case 0x0: {
                                                    const uint32_t W31 = g.u8(ESP0 + 0x1c);
                                                    const uint32_t i13 = uint32_t(W7 & 0xffffff00u);
                                                    m.r[4] = A - 0x10;
                                                    const uint32_t R32 = callr(g, 0x008656c0, {ecx, arg3, arg0, arg1, arg2, uint32_t(uint32_t(i13 | uint32_t(uint32_t(W31 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg0).ECX(arg4).EDX(arg5).EBX(i2).ESI(arg3).EDI(ecx));
                                                    m.r[0] = R32;
                                                    m.r[4] = ESP0;
                                                    return m.ret(28);
                                                }
                                                case 0x1: case 0x3: case 0x4: case 0x5: case 0x7: case 0x8: case 0x9: case 0xa: case 0xb: case 0xc: case 0xd: case 0xf: case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: {
                                                    m.r[0] = arg0;
                                                    m.r[1] = uint32_t(g.u8(0x0085ee14 + i12));
                                                    m.r[2] = W7;
                                                    m.r[4] = ESP0;
                                                    return m.ret(28);
                                                }
                                                case 0x2: {
                                                    const uint32_t W33 = g.u8(ESP0 + 0x1c);
                                                    const uint32_t i14 = uint32_t(W7 & 0xffffff00u);
                                                    m.r[4] = A - 0x10;
                                                    const uint32_t R34 = callr(g, 0x00872860, {ecx, arg3, arg0, arg1, arg2, uint32_t(uint32_t(i14 | uint32_t(uint32_t(W33 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg0).ECX(arg4).EDX(arg5).EBX(i2).ESI(arg3).EDI(ecx));
                                                    m.r[0] = R34;
                                                    m.r[4] = ESP0;
                                                    return m.ret(28);
                                                }
                                                case 0x6: {
                                                    const uint32_t W35 = g.u8(ESP0 + 0x1c);
                                                    const uint32_t i15 = uint32_t(W7 & 0xffffff00u);
                                                    m.r[4] = A - 0x10;
                                                    const uint32_t R36 = callr(g, 0x00872860, {ecx, arg3, arg0, arg1, arg2, uint32_t(uint32_t(i15 | uint32_t(uint32_t(W35 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg0).ECX(arg4).EDX(arg5).EBX(i2).ESI(arg3).EDI(ecx));
                                                    m.r[0] = R36;
                                                    m.r[4] = ESP0;
                                                    return m.ret(28);
                                                }
                                                case 0xe: {
                                                    const uint32_t W37 = g.u8(ESP0 + 0x1c);
                                                    const uint32_t i16 = uint32_t(W7 & 0xffffff00u);
                                                    m.r[4] = A - 0x10;
                                                    const uint32_t R38 = callr(g, 0x00872860, {ecx, arg3, arg0, arg1, arg2, uint32_t(uint32_t(i16 | uint32_t(uint32_t(W37 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg0).ECX(arg4).EDX(arg5).EBX(i2).ESI(arg3).EDI(ecx));
                                                    m.r[0] = R38;
                                                    m.r[4] = ESP0;
                                                    return m.ret(28);
                                                }
                                                case 0x1e: {
                                                    const uint32_t W39 = g.u8(ESP0 + 0x1c);
                                                    const uint32_t i17 = uint32_t(W7 & 0xffffff00u);
                                                    m.r[4] = A - 0x10;
                                                    const uint32_t R40 = callr(g, 0x00872860, {ecx, arg3, arg0, arg1, arg2, uint32_t(uint32_t(i17 | uint32_t(uint32_t(W39 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg0).ECX(arg4).EDX(arg5).EBX(i2).ESI(arg3).EDI(ecx));
                                                    m.r[0] = R40;
                                                    m.r[4] = ESP0;
                                                    return m.ret(28);
                                                }
                                                default: break;
                                            }
                                            throw std::logic_error("0x0 switch value outside the table");
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        if (W4 == 0x200u) {
                            const uint32_t W41 = g.u32(arg3 + 0x8);
                            if (int32_t(W41) > int32_t(0x40u)) {
                                if (int32_t(W41) <= int32_t(0x200u)) {
                                    m.r[0] = arg0;
                                    m.r[1] = W41;
                                    m.r[2] = W7;
                                    m.r[4] = ESP0;
                                    return m.ret(28);
                                } else {
                                    if (W41 == 0x400u) {
                                        m.r[0] = arg0;
                                        m.r[1] = W41;
                                        m.r[2] = W7;
                                        m.r[4] = ESP0;
                                        return m.ret(28);
                                    } else {
                                        if (W41 != 0x8000u) {
                                            m.r[0] = arg0;
                                            m.r[1] = W41;
                                            m.r[2] = W7;
                                            m.r[4] = ESP0;
                                            return m.ret(28);
                                        } else {
                                            m.r[4] = A - 0x10;
                                            const uint32_t R42 = callr(g, 0x00873010, {ecx, arg2, arg3, arg4, arg5, arg6}, In().EAX(arg0).ECX(arg0).EDX(arg1).EBX(i2).ESI(arg3).EDI(ecx));
                                            m.r[0] = R42;
                                            m.r[4] = ESP0;
                                            return m.ret(28);
                                        }
                                    }
                                }
                            } else {
                                if (W41 == 0x40u) {
                                    m.r[0] = arg0;
                                    m.r[1] = W41;
                                    m.r[2] = W7;
                                    m.r[4] = ESP0;
                                    return m.ret(28);
                                } else {
                                    const uint32_t i18 = uint32_t(W41 - 0x2u);
                                    if (i18 > 0x1eu) {
                                        m.r[0] = arg0;
                                        m.r[1] = i18;
                                        m.r[2] = W7;
                                        m.r[4] = ESP0;
                                        return m.ret(28);
                                    } else {
                                        switch (i18) {
                                            case 0x0: {
                                                const uint32_t W43 = g.u8(ESP0 + 0x1c);
                                                const uint32_t i19 = uint32_t(i18 & 0xffffff00u);
                                                const uint32_t i20 = uint32_t(uint32_t(i19 | uint32_t(uint32_t(W43 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu);
                                                m.r[4] = A - 0x10;
                                                const uint32_t R44 = callr(g, 0x00864ec0, {ecx, arg0, i20}, In().EAX(arg0).ECX(arg3).EDX(i20).EBX(i2).ESI(arg3).EDI(arg5));
                                                m.r[0] = R44;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            }
                                            case 0x1: case 0x3: case 0x4: case 0x5: case 0x7: case 0x8: case 0x9: case 0xa: case 0xb: case 0xc: case 0xd: case 0xe: case 0xf: case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: {
                                                m.r[0] = arg0;
                                                m.r[1] = i18;
                                                m.r[2] = uint32_t(g.u8(0x0085eddc + i18));
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            }
                                            case 0x2: {
                                                const uint32_t W45 = g.u8(ESP0 + 0x1c);
                                                const uint32_t i21 = uint32_t(i18 & 0xffffff00u);
                                                m.r[4] = A - 0x10;
                                                const uint32_t R46 = callr(g, 0x0086b4d0, {ecx, arg4, arg5, uint32_t(uint32_t(i21 | uint32_t(uint32_t(W45 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg0).ECX(arg0).EDX(arg3).EBX(i2).ESI(arg3).EDI(ecx));
                                                m.r[0] = R46;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            }
                                            case 0x6: {
                                                const uint32_t W47 = g.u8(ESP0 + 0x1c);
                                                const uint32_t i22 = uint32_t(i18 & 0xffffff00u);
                                                m.r[4] = A - 0x10;
                                                const uint32_t R48 = callr(g, 0x0086f2a0, {arg3, arg0, uint32_t(uint32_t(i22 | uint32_t(uint32_t(W47 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg0).ECX(arg5).EDX(arg4).EBX(i2).ESI(arg3).EDI(ecx));
                                                m.r[0] = R48;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            }
                                            case 0x1e: {
                                                const uint32_t W49 = g.u8(ESP0 + 0x1c);
                                                const uint32_t i23 = uint32_t(i18 & 0xffffff00u);
                                                m.r[4] = A - 0x10;
                                                const uint32_t R50 = callr(g, 0x00872c90, {ecx, arg4, arg0, uint32_t(uint32_t(i23 | uint32_t(uint32_t(W49 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg4).ECX(arg3).EDX(arg5).EBX(i2).ESI(arg3).EDI(ecx));
                                                m.r[0] = R50;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            }
                                            default: break;
                                        }
                                        throw std::logic_error("0x0 switch value outside the table");
                                    }
                                }
                            }
                        } else {
                            if (W4 == 0x80u) {
                                const uint32_t W51 = g.u32(arg3 + 0x8);
                                if (int32_t(W51) > int32_t(0x40u)) {
                                    if (int32_t(W51) <= int32_t(0x200u)) {
                                        m.r[0] = arg0;
                                        m.r[1] = W51;
                                        m.r[2] = W7;
                                        m.r[4] = ESP0;
                                        return m.ret(28);
                                    } else {
                                        if (W51 == 0x400u) {
                                            m.r[0] = arg0;
                                            m.r[1] = W51;
                                            m.r[2] = W7;
                                            m.r[4] = ESP0;
                                            return m.ret(28);
                                        } else {
                                            if (W51 != 0x8000u) {
                                                m.r[0] = arg0;
                                                m.r[1] = W51;
                                                m.r[2] = W7;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            } else {
                                                m.r[4] = A - 0x10;
                                                const uint32_t R52 = callr(g, 0x00872860, {ecx, arg0, arg3, arg4, arg5, arg6}, In().EAX(arg0).ECX(arg1).EDX(arg2).EBX(i2).ESI(arg3).EDI(ecx));
                                                m.r[0] = R52;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            }
                                        }
                                    }
                                } else {
                                    if (W51 == 0x40u) {
                                        m.r[0] = arg0;
                                        m.r[1] = W51;
                                        m.r[2] = W7;
                                        m.r[4] = ESP0;
                                        return m.ret(28);
                                    } else {
                                        const uint32_t i24 = uint32_t(W51 - 0x2u);
                                        if (i24 > 0x1eu) {
                                            m.r[0] = arg0;
                                            m.r[1] = i24;
                                            m.r[2] = W7;
                                            m.r[4] = ESP0;
                                            return m.ret(28);
                                        } else {
                                            switch (i24) {
                                                case 0x0: case 0x1: case 0x2: case 0x3: case 0x4: case 0x5: case 0x6: case 0x7: case 0x8: case 0x9: case 0xa: case 0xb: case 0xc: case 0xd: case 0xe: case 0xf: case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: {
                                                    m.r[0] = uint32_t(g.u8(0x0085eda4 + i24));
                                                    m.r[1] = i24;
                                                    m.r[2] = W7;
                                                    m.r[4] = ESP0;
                                                    return m.ret(28);
                                                }
                                                default: break;
                                            }
                                            throw std::logic_error("0x0 switch value outside the table");
                                        }
                                    }
                                }
                            } else {
                                if (W4 != 0x100u) {
                                    m.r[0] = arg0;
                                    m.r[1] = W4;
                                    m.r[2] = W7;
                                    m.r[4] = ESP0;
                                    return m.ret(28);
                                } else {
                                    const uint32_t W53 = g.u32(arg3 + 0x8);
                                    if (int32_t(W53) > int32_t(0x40u)) {
                                        if (int32_t(W53) <= int32_t(0x200u)) {
                                            m.r[0] = arg0;
                                            m.r[1] = W53;
                                            m.r[2] = W7;
                                            m.r[4] = ESP0;
                                            return m.ret(28);
                                        } else {
                                            if (W53 == 0x400u) {
                                                m.r[0] = arg0;
                                                m.r[1] = W53;
                                                m.r[2] = W7;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            } else {
                                                if (W53 != 0x8000u) {
                                                    m.r[0] = arg0;
                                                    m.r[1] = W53;
                                                    m.r[2] = W7;
                                                    m.r[4] = ESP0;
                                                    return m.ret(28);
                                                } else {
                                                    m.r[4] = A - 0x10;
                                                    const uint32_t R54 = callr(g, 0x00872860, {ecx, arg0, arg3, arg4, arg5, arg6}, In().EAX(arg0).ECX(arg1).EDX(arg2).EBX(i2).ESI(arg3).EDI(ecx));
                                                    m.r[0] = R54;
                                                    m.r[4] = ESP0;
                                                    return m.ret(28);
                                                }
                                            }
                                        }
                                    } else {
                                        if (W53 == 0x40u) {
                                            m.r[0] = arg0;
                                            m.r[1] = W53;
                                            m.r[2] = W7;
                                            m.r[4] = ESP0;
                                            return m.ret(28);
                                        } else {
                                            const uint32_t i25 = uint32_t(W53 - 0x2u);
                                            if (i25 > 0x1eu) {
                                                m.r[0] = arg0;
                                                m.r[1] = i25;
                                                m.r[2] = W7;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            } else {
                                                switch (i25) {
                                                    case 0x0: {
                                                        const uint32_t W55 = g.u8(ESP0 + 0x1c);
                                                        const uint32_t i26 = uint32_t(i25 & 0xffffff00u);
                                                        const uint32_t i27 = uint32_t(i26 | uint32_t(uint32_t(W55 == 0x0u ? 1 : 0) & 0xffu));
                                                        const uint32_t i28 = uint32_t(i27 & 0xffu);
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R56 = callr(g, 0x00864dd0, {ecx, arg0, i28}, In().EAX(arg0).ECX(i27).EDX(i28).EBX(i2).ESI(arg3).EDI(arg5));
                                                        m.r[0] = R56;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0x1: case 0x3: case 0x4: case 0x5: case 0x7: case 0x8: case 0x9: case 0xa: case 0xb: case 0xc: case 0xd: case 0xe: case 0xf: case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: {
                                                        m.r[0] = arg0;
                                                        m.r[1] = i25;
                                                        m.r[2] = uint32_t(g.u8(0x0085ed7c + i25));
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0x2: {
                                                        const uint32_t W57 = g.u8(ESP0 + 0x1c);
                                                        const uint32_t i29 = uint32_t(i25 & 0xffffff00u);
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R58 = callr(g, 0x0086b360, {ecx, arg4, arg0, uint32_t(uint32_t(i29 | uint32_t(uint32_t(W57 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg4).ECX(arg3).EDX(arg5).EBX(i2).ESI(arg3).EDI(ecx));
                                                        m.r[0] = R58;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0x6: {
                                                        const uint32_t W59 = g.u8(ESP0 + 0x1c);
                                                        const uint32_t i30 = uint32_t(i25 & 0xffffff00u);
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R60 = callr(g, 0x0086f150, {arg3, arg0, uint32_t(uint32_t(i30 | uint32_t(uint32_t(W59 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg0).ECX(arg5).EDX(arg4).EBX(i2).ESI(arg3).EDI(ecx));
                                                        m.r[0] = R60;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0x1e: {
                                                        const uint32_t W61 = g.u8(ESP0 + 0x1c);
                                                        const uint32_t i31 = uint32_t(i25 & 0xffffff00u);
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R62 = callr(g, 0x00872970, {arg4, uint32_t(uint32_t(i31 | uint32_t(uint32_t(W61 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg0).ECX(arg5).EDX(arg0).EBX(i2).ESI(arg3).EDI(ecx));
                                                        m.r[0] = R62;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    default: break;
                                                }
                                                throw std::logic_error("0x0 switch value outside the table");
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                } else {
                    if (W4 == 0x40u) {
                        const uint32_t W63 = g.u32(arg3 + 0x8);
                        if (int32_t(W63) > int32_t(0x40u)) {
                            if (int32_t(W63) <= int32_t(0x200u)) {
                                m.r[0] = arg0;
                                m.r[1] = W63;
                                m.r[2] = W7;
                                m.r[4] = ESP0;
                                return m.ret(28);
                            } else {
                                if (W63 == 0x400u) {
                                    m.r[0] = arg0;
                                    m.r[1] = W63;
                                    m.r[2] = W7;
                                    m.r[4] = ESP0;
                                    return m.ret(28);
                                } else {
                                    if (W63 != 0x8000u) {
                                        m.r[0] = arg0;
                                        m.r[1] = W63;
                                        m.r[2] = W7;
                                        m.r[4] = ESP0;
                                        return m.ret(28);
                                    } else {
                                        m.r[4] = A - 0x10;
                                        const uint32_t R64 = callr(g, 0x00872860, {ecx, arg0, arg3, arg4, arg5, arg6}, In().EAX(arg0).ECX(arg1).EDX(arg2).EBX(i2).ESI(arg3).EDI(ecx));
                                        m.r[0] = R64;
                                        m.r[4] = ESP0;
                                        return m.ret(28);
                                    }
                                }
                            }
                        } else {
                            if (W63 == 0x40u) {
                                m.r[0] = arg0;
                                m.r[1] = W63;
                                m.r[2] = W7;
                                m.r[4] = ESP0;
                                return m.ret(28);
                            } else {
                                const uint32_t i32 = uint32_t(W63 - 0x2u);
                                if (i32 > 0x1eu) {
                                    m.r[0] = arg0;
                                    m.r[1] = i32;
                                    m.r[2] = W7;
                                    m.r[4] = ESP0;
                                    return m.ret(28);
                                } else {
                                    switch (i32) {
                                        case 0x0: case 0x1: case 0x2: case 0x3: case 0x4: case 0x5: case 0x6: case 0x7: case 0x8: case 0x9: case 0xa: case 0xb: case 0xc: case 0xd: case 0xe: case 0xf: case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: {
                                            m.r[0] = arg0;
                                            m.r[1] = i32;
                                            m.r[2] = uint32_t(g.u8(0x0085ed44 + i32));
                                            m.r[4] = ESP0;
                                            return m.ret(28);
                                        }
                                        default: break;
                                    }
                                    throw std::logic_error("0x0 switch value outside the table");
                                }
                            }
                        }
                    } else {
                        const uint32_t i33 = uint32_t(W4 - 0x2u);
                        if (i33 > 0x1eu) {
                            m.r[0] = arg0;
                            m.r[1] = i33;
                            m.r[2] = W7;
                            m.r[4] = ESP0;
                            return m.ret(28);
                        } else {
                            switch (i33) {
                                case 0x0: {
                                    const uint32_t W65 = g.u32(arg3 + 0x8);
                                    if (int32_t(W65) > int32_t(0x40u)) {
                                        if (int32_t(W65) > int32_t(0x200u)) {
                                            if (W65 == 0x400u) {
                                                const uint32_t W66 = g.u8(ESP0 + 0x1c);
                                                m.r[4] = A - 0x10;
                                                const uint32_t R67 = callr(g, 0x008650c0, {ecx, arg0, arg2, arg3, arg5}, In().EAX(arg0).ECX(uint32_t(uint32_t(arg5 & 0xffffff00u) | uint32_t(W66 & 0xffu))).EDX(arg4).EBX(i2).ESI(arg3).EDI(ecx));
                                                m.r[0] = R67;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            } else {
                                                if (W65 != 0x8000u) {
                                                    m.r[0] = arg0;
                                                    m.r[1] = W65;
                                                    m.r[2] = W7;
                                                    m.r[4] = ESP0;
                                                    return m.ret(28);
                                                } else {
                                                    m.r[4] = A - 0x10;
                                                    const uint32_t R68 = callr(g, 0x008656c0, {ecx, arg0, arg3, arg4, arg5, arg6}, In().EAX(arg0).ECX(arg1).EDX(arg2).EBX(i2).ESI(arg3).EDI(ecx));
                                                    m.r[0] = R68;
                                                    m.r[4] = ESP0;
                                                    return m.ret(28);
                                                }
                                            }
                                        } else {
                                            if (W65 == 0x200u) {
                                                m.r[4] = A - 0x10;
                                                const uint32_t R69 = callr(g, 0x00864ec0, {ecx, arg3, arg6}, In().EAX(arg0).ECX(arg0).EDX(W7).EBX(i2).ESI(arg3).EDI(arg2));
                                                m.r[0] = R69;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            } else {
                                                if (W65 == 0x80u) {
                                                    m.r[0] = arg0;
                                                    m.r[1] = W65;
                                                    m.r[2] = W7;
                                                    m.r[4] = ESP0;
                                                    return m.ret(28);
                                                } else {
                                                    if (W65 != 0x100u) {
                                                        m.r[0] = arg0;
                                                        m.r[1] = W65;
                                                        m.r[2] = W7;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    } else {
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R70 = callr(g, 0x00864dd0, {ecx, arg3, arg6}, In().EAX(arg0).ECX(W65).EDX(arg6).EBX(i2).ESI(arg0).EDI(arg2));
                                                        m.r[0] = R70;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                }
                                            }
                                        }
                                    } else {
                                        if (W65 == 0x40u) {
                                            m.r[0] = arg0;
                                            m.r[1] = W65;
                                            m.r[2] = W7;
                                            m.r[4] = ESP0;
                                            return m.ret(28);
                                        } else {
                                            const uint32_t i34 = uint32_t(W65 - 0x2u);
                                            if (i34 > 0x1eu) {
                                                m.r[0] = arg0;
                                                m.r[1] = i34;
                                                m.r[2] = W7;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            } else {
                                                switch (i34) {
                                                    case 0x0: {
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R71 = callr(g, 0x00861a60, {ecx, arg3, arg5, arg6}, In().EAX(arg0).ECX(arg2).EDX(arg0).EBX(i2).ESI(arg3).EDI(ecx));
                                                        m.r[0] = R71;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0x1: case 0x3: case 0x4: case 0x5: case 0x7: case 0x8: case 0x9: case 0xa: case 0xb: case 0xc: case 0xd: case 0xf: case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: {
                                                        m.r[0] = arg0;
                                                        m.r[1] = i34;
                                                        m.r[2] = uint32_t(g.u8(0x0085ec3c + i34));
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0x2: {
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R72 = callr(g, 0x00861ba0, {ecx, arg1, arg3, arg4, arg5, arg6}, In().EAX(arg0).ECX(arg0).EDX(arg2).EBX(i2).ESI(arg3).EDI(ecx));
                                                        m.r[0] = R72;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0x6: {
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R73 = callr(g, 0x00862da0, {ecx, arg2, arg3, arg4, arg6}, In().EAX(arg0).ECX(arg5).EDX(arg0).EBX(i2).ESI(arg3).EDI(ecx));
                                                        m.r[0] = R73;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0xe: {
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R74 = callr(g, 0x00863f70, {ecx, arg2, arg4, arg6}, In().EAX(arg0).ECX(arg5).EDX(arg0).EBX(i2).ESI(arg3).EDI(arg3));
                                                        m.r[0] = R74;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0x1e: {
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R75 = callr(g, 0x00864460, {arg2, arg3, arg4, arg6}, In().EAX(arg0).ECX(arg5).EDX(arg0).EBX(i2).ESI(ecx).EDI(ecx));
                                                        m.r[0] = R75;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    default: break;
                                                }
                                                throw std::logic_error("0x0 switch value outside the table");
                                            }
                                        }
                                    }
                                }
                                case 0x1: case 0x3: case 0x4: case 0x5: case 0x7: case 0x8: case 0x9: case 0xa: case 0xb: case 0xc: case 0xd: case 0xf: case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: {
                                    m.r[0] = arg0;
                                    m.r[1] = uint32_t(g.u8(0x0085ec04 + i33));
                                    m.r[2] = W7;
                                    m.r[4] = ESP0;
                                    return m.ret(28);
                                }
                                case 0x2: {
                                    const uint32_t W76 = g.u32(arg3 + 0x8);
                                    if (int32_t(W76) > int32_t(0x40u)) {
                                        if (int32_t(W76) > int32_t(0x200u)) {
                                            if (W76 == 0x400u) {
                                                m.r[4] = A - 0x10;
                                                const uint32_t R77 = callr(g, 0x0086b7f0, {ecx, arg0, arg1, arg2, arg3, arg6}, In().EAX(arg0).ECX(arg4).EDX(arg5).EBX(i2).ESI(arg3).EDI(ecx));
                                                m.r[0] = R77;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            } else {
                                                if (W76 != 0x8000u) {
                                                    m.r[0] = arg0;
                                                    m.r[1] = W76;
                                                    m.r[2] = W7;
                                                    m.r[4] = ESP0;
                                                    return m.ret(28);
                                                } else {
                                                    m.r[4] = A - 0x10;
                                                    const uint32_t R78 = callr(g, 0x00872860, {ecx, arg0, arg3, arg4, arg5, arg6}, In().EAX(arg0).ECX(arg1).EDX(arg2).EBX(i2).ESI(arg3).EDI(ecx));
                                                    m.r[0] = R78;
                                                    m.r[4] = ESP0;
                                                    return m.ret(28);
                                                }
                                            }
                                        } else {
                                            if (W76 == 0x200u) {
                                                m.r[4] = A - 0x10;
                                                const uint32_t R79 = callr(g, 0x0086b4d0, {ecx, arg1, arg2, arg6}, In().EAX(arg0).ECX(arg3).EDX(arg0).EBX(i2).ESI(arg3).EDI(ecx));
                                                m.r[0] = R79;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            } else {
                                                if (W76 == 0x80u) {
                                                    m.r[0] = arg0;
                                                    m.r[1] = W76;
                                                    m.r[2] = W7;
                                                    m.r[4] = ESP0;
                                                    return m.ret(28);
                                                } else {
                                                    if (W76 != 0x100u) {
                                                        m.r[0] = arg0;
                                                        m.r[1] = W76;
                                                        m.r[2] = W7;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    } else {
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R80 = callr(g, 0x0086b360, {ecx, arg1, arg3, arg6}, In().EAX(arg0).ECX(arg0).EDX(arg2).EBX(i2).ESI(arg3).EDI(ecx));
                                                        m.r[0] = R80;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                }
                                            }
                                        }
                                    } else {
                                        if (W76 == 0x40u) {
                                            m.r[0] = arg0;
                                            m.r[1] = W76;
                                            m.r[2] = W7;
                                            m.r[4] = ESP0;
                                            return m.ret(28);
                                        } else {
                                            const uint32_t i35 = uint32_t(W76 - 0x2u);
                                            if (i35 > 0x1eu) {
                                                m.r[0] = arg0;
                                                m.r[1] = i35;
                                                m.r[2] = W7;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            } else {
                                                switch (i35) {
                                                    case 0x0: {
                                                        const uint32_t W81 = g.u8(ESP0 + 0x1c);
                                                        const uint32_t i36 = uint32_t(W7 & 0xffffff00u);
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R82 = callr(g, 0x00861ba0, {ecx, arg4, arg0, arg1, arg2, uint32_t(uint32_t(i36 | uint32_t(uint32_t(W81 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg0).ECX(arg3).EDX(arg5).EBX(i2).ESI(arg3).EDI(ecx));
                                                        m.r[0] = R82;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0x1: case 0x3: case 0x4: case 0x5: case 0x7: case 0x8: case 0x9: case 0xa: case 0xb: case 0xc: case 0xd: case 0xf: case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: {
                                                        m.r[0] = arg0;
                                                        m.r[1] = uint32_t(g.u8(0x0085ec74 + i35));
                                                        m.r[2] = W7;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0x2: {
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R83 = callr(g, 0x008657d0, {ecx, arg0, arg1, arg2, arg5, arg6}, In().EAX(arg0).ECX(arg3).EDX(arg4).EBX(i2).ESI(arg3).EDI(ecx));
                                                        m.r[0] = R83;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0x6: {
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R84 = callr(g, 0x00867ca0, {ecx, arg0, arg1, arg2, arg5, arg6}, In().EAX(arg0).ECX(arg4).EDX(arg3).EBX(i2).ESI(arg3).EDI(ecx));
                                                        m.r[0] = R84;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0xe: {
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R85 = callr(g, 0x00869d40, {ecx, arg1, arg2, arg3, arg5, arg6}, In().EAX(arg0).ECX(arg0).EDX(arg4).EBX(i2).ESI(arg3).EDI(ecx));
                                                        m.r[0] = R85;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0x1e: {
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R86 = callr(g, 0x0086a1e0, {arg0, arg1, arg2, arg4, arg6}, In().EAX(arg0).ECX(arg3).EDX(arg5).EBX(i2).ESI(arg3).EDI(ecx));
                                                        m.r[0] = R86;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    default: break;
                                                }
                                                throw std::logic_error("0x0 switch value outside the table");
                                            }
                                        }
                                    }
                                }
                                case 0x6: {
                                    const uint32_t W87 = g.u32(arg3 + 0x8);
                                    if (int32_t(W87) > int32_t(0x40u)) {
                                        if (int32_t(W87) > int32_t(0x200u)) {
                                            if (W87 == 0x400u) {
                                                m.r[4] = A - 0x10;
                                                const uint32_t R88 = callr(g, 0x0086f3d0, {ecx, arg0, arg2, arg3, arg4, arg6}, In().EAX(arg0).ECX(arg5).EDX(arg1).EBX(i2).ESI(arg3).EDI(ecx));
                                                m.r[0] = R88;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            } else {
                                                if (W87 != 0x8000u) {
                                                    m.r[0] = arg0;
                                                    m.r[1] = W87;
                                                    m.r[2] = W7;
                                                    m.r[4] = ESP0;
                                                    return m.ret(28);
                                                } else {
                                                    m.r[4] = A - 0x10;
                                                    const uint32_t R89 = callr(g, 0x00872860, {ecx, arg0, arg3, arg4, arg5, arg6}, In().EAX(arg0).ECX(arg1).EDX(arg2).EBX(i2).ESI(arg3).EDI(ecx));
                                                    m.r[0] = R89;
                                                    m.r[4] = ESP0;
                                                    return m.ret(28);
                                                }
                                            }
                                        } else {
                                            if (W87 == 0x200u) {
                                                m.r[4] = A - 0x10;
                                                const uint32_t R90 = callr(g, 0x0086f2a0, {arg0, arg3, arg6}, In().EAX(arg0).ECX(arg2).EDX(arg1).EBX(i2).ESI(arg3).EDI(ecx));
                                                m.r[0] = R90;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            } else {
                                                if (W87 == 0x80u) {
                                                    m.r[0] = arg0;
                                                    m.r[1] = W87;
                                                    m.r[2] = W7;
                                                    m.r[4] = ESP0;
                                                    return m.ret(28);
                                                } else {
                                                    if (W87 != 0x100u) {
                                                        m.r[0] = arg0;
                                                        m.r[1] = W87;
                                                        m.r[2] = W7;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    } else {
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R91 = callr(g, 0x0086f150, {arg0, arg3, arg6}, In().EAX(arg0).ECX(arg2).EDX(arg1).EBX(i2).ESI(arg3).EDI(ecx));
                                                        m.r[0] = R91;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                }
                                            }
                                        }
                                    } else {
                                        if (W87 == 0x40u) {
                                            m.r[0] = arg0;
                                            m.r[1] = W87;
                                            m.r[2] = W7;
                                            m.r[4] = ESP0;
                                            return m.ret(28);
                                        } else {
                                            const uint32_t i37 = uint32_t(W87 - 0x2u);
                                            if (i37 > 0x1eu) {
                                                m.r[0] = arg0;
                                                m.r[1] = i37;
                                                m.r[2] = W7;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            } else {
                                                switch (i37) {
                                                    case 0x0: {
                                                        const uint32_t W92 = g.u8(ESP0 + 0x1c);
                                                        const uint32_t i38 = uint32_t(i37 & 0xffffff00u);
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R93 = callr(g, 0x00862da0, {ecx, arg5, arg0, arg1, uint32_t(uint32_t(i38 | uint32_t(uint32_t(W92 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg0).ECX(arg2).EDX(arg3).EBX(i2).ESI(arg3).EDI(ecx));
                                                        m.r[0] = R93;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0x1: case 0x3: case 0x4: case 0x5: case 0x7: case 0x8: case 0x9: case 0xa: case 0xb: case 0xc: case 0xd: case 0xf: case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: {
                                                        m.r[0] = arg0;
                                                        m.r[1] = i37;
                                                        m.r[2] = uint32_t(g.u8(0x0085ecac + i37));
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0x2: {
                                                        const uint32_t W94 = g.u8(ESP0 + 0x1c);
                                                        const uint32_t i39 = uint32_t(i37 & 0xffffff00u);
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R95 = callr(g, 0x00867ca0, {ecx, arg3, arg4, arg5, arg2, uint32_t(uint32_t(i39 | uint32_t(uint32_t(W94 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg0).ECX(arg1).EDX(arg0).EBX(i2).ESI(arg3).EDI(ecx));
                                                        m.r[0] = R95;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0x6: {
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R96 = callr(g, 0x0086bd80, {ecx, arg0, arg1, arg2, arg3, arg6}, In().EAX(arg0).ECX(arg4).EDX(arg5).EBX(i2).ESI(arg3).EDI(ecx));
                                                        m.r[0] = R96;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0xe: {
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R97 = callr(g, 0x0086e090, {arg0, arg1, arg3, arg5, arg6}, In().EAX(arg0).ECX(arg2).EDX(arg4).EBX(i2).ESI(arg3).EDI(ecx));
                                                        m.r[0] = R97;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0x1e: {
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R98 = callr(g, 0x0086e750, {ecx, arg0, arg1, arg2, arg4, arg6}, In().EAX(arg0).ECX(arg3).EDX(arg5).EBX(i2).ESI(arg3).EDI(ecx));
                                                        m.r[0] = R98;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    default: break;
                                                }
                                                throw std::logic_error("0x0 switch value outside the table");
                                            }
                                        }
                                    }
                                }
                                case 0xe: {
                                    const uint32_t W99 = g.u32(arg3 + 0x8);
                                    if (int32_t(W99) > int32_t(0x40u)) {
                                        if (int32_t(W99) <= int32_t(0x200u)) {
                                            m.r[0] = arg0;
                                            m.r[1] = W99;
                                            m.r[2] = W7;
                                            m.r[4] = ESP0;
                                            return m.ret(28);
                                        } else {
                                            if (W99 == 0x400u) {
                                                m.r[4] = A - 0x10;
                                                const uint32_t R100 = callr(g, 0x008716b0, {ecx, arg1, arg2, arg3, arg5, arg6}, In().EAX(arg0).ECX(arg4).EDX(arg0).EBX(i2).ESI(arg3).EDI(ecx));
                                                m.r[0] = R100;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            } else {
                                                if (W99 != 0x8000u) {
                                                    m.r[0] = arg0;
                                                    m.r[1] = W99;
                                                    m.r[2] = W7;
                                                    m.r[4] = ESP0;
                                                    return m.ret(28);
                                                } else {
                                                    m.r[4] = A - 0x10;
                                                    const uint32_t R101 = callr(g, 0x00872860, {ecx, arg0, arg3, arg4, arg5, arg6}, In().EAX(arg0).ECX(arg1).EDX(arg2).EBX(i2).ESI(arg3).EDI(ecx));
                                                    m.r[0] = R101;
                                                    m.r[4] = ESP0;
                                                    return m.ret(28);
                                                }
                                            }
                                        }
                                    } else {
                                        if (W99 == 0x40u) {
                                            m.r[0] = arg0;
                                            m.r[1] = W99;
                                            m.r[2] = W7;
                                            m.r[4] = ESP0;
                                            return m.ret(28);
                                        } else {
                                            const uint32_t i40 = uint32_t(W99 - 0x2u);
                                            if (i40 > 0x1eu) {
                                                m.r[0] = arg0;
                                                m.r[1] = i40;
                                                m.r[2] = W7;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            } else {
                                                switch (i40) {
                                                    case 0x0: {
                                                        const uint32_t W102 = g.u8(ESP0 + 0x1c);
                                                        const uint32_t i41 = uint32_t(i40 & 0xffffff00u);
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R103 = callr(g, 0x00863f70, {ecx, arg5, arg1, uint32_t(uint32_t(i41 | uint32_t(uint32_t(W102 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg0).ECX(arg2).EDX(arg3).EBX(i2).ESI(arg3).EDI(arg0));
                                                        m.r[0] = R103;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0x1: case 0x3: case 0x4: case 0x5: case 0x7: case 0x8: case 0x9: case 0xa: case 0xb: case 0xc: case 0xd: case 0xe: case 0xf: case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: {
                                                        m.r[0] = arg0;
                                                        m.r[1] = i40;
                                                        m.r[2] = uint32_t(g.u8(0x0085ece4 + i40));
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0x2: {
                                                        const uint32_t W104 = g.u8(ESP0 + 0x1c);
                                                        const uint32_t i42 = uint32_t(i40 & 0xffffff00u);
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R105 = callr(g, 0x00869d40, {ecx, arg4, arg5, arg0, arg2, uint32_t(uint32_t(i42 | uint32_t(uint32_t(W104 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg4).ECX(arg3).EDX(arg1).EBX(i2).ESI(arg3).EDI(ecx));
                                                        m.r[0] = R105;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0x6: {
                                                        const uint32_t W106 = g.u8(ESP0 + 0x1c);
                                                        const uint32_t i43 = uint32_t(i40 & 0xffffff00u);
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R107 = callr(g, 0x0086e090, {arg3, arg4, arg0, arg2, uint32_t(uint32_t(i43 | uint32_t(uint32_t(W106 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg0).ECX(arg5).EDX(arg1).EBX(i2).ESI(arg3).EDI(ecx));
                                                        m.r[0] = R107;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0x1e: {
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R108 = callr(g, 0x0086fac0, {arg0, arg2, arg3, arg4, arg6}, In().EAX(arg0).ECX(arg5).EDX(arg1).EBX(i2).ESI(ecx).EDI(ecx));
                                                        m.r[0] = R108;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    default: break;
                                                }
                                                throw std::logic_error("0x0 switch value outside the table");
                                            }
                                        }
                                    }
                                }
                                case 0x1e: {
                                    const uint32_t W109 = g.u32(arg3 + 0x8);
                                    if (int32_t(W109) > int32_t(0x40u)) {
                                        if (int32_t(W109) > int32_t(0x200u)) {
                                            if (W109 == 0x400u) {
                                                m.r[0] = arg0;
                                                m.r[1] = W109;
                                                m.r[2] = W7;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            } else {
                                                if (W109 != 0x8000u) {
                                                    m.r[0] = arg0;
                                                    m.r[1] = W109;
                                                    m.r[2] = W7;
                                                    m.r[4] = ESP0;
                                                    return m.ret(28);
                                                } else {
                                                    m.r[4] = A - 0x10;
                                                    const uint32_t R110 = callr(g, 0x00872860, {ecx, arg0, arg3, arg4, arg5, arg6}, In().EAX(arg0).ECX(arg1).EDX(arg2).EBX(i2).ESI(arg3).EDI(ecx));
                                                    m.r[0] = R110;
                                                    m.r[4] = ESP0;
                                                    return m.ret(28);
                                                }
                                            }
                                        } else {
                                            if (W109 == 0x200u) {
                                                m.r[4] = A - 0x10;
                                                const uint32_t R111 = callr(g, 0x00872c90, {ecx, arg1, arg3, arg6}, In().EAX(arg0).ECX(arg0).EDX(arg2).EBX(i2).ESI(arg3).EDI(ecx));
                                                m.r[0] = R111;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            } else {
                                                if (W109 == 0x80u) {
                                                    m.r[0] = arg0;
                                                    m.r[1] = W109;
                                                    m.r[2] = W7;
                                                    m.r[4] = ESP0;
                                                    return m.ret(28);
                                                } else {
                                                    if (W109 != 0x100u) {
                                                        m.r[0] = arg0;
                                                        m.r[1] = W109;
                                                        m.r[2] = W7;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    } else {
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R112 = callr(g, 0x00872970, {arg1, arg6}, In().EAX(arg0).ECX(arg2).EDX(arg3).EBX(i2).ESI(arg0).EDI(ecx));
                                                        m.r[0] = R112;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                }
                                            }
                                        }
                                    } else {
                                        if (W109 == 0x40u) {
                                            m.r[0] = arg0;
                                            m.r[1] = W109;
                                            m.r[2] = W7;
                                            m.r[4] = ESP0;
                                            return m.ret(28);
                                        } else {
                                            const uint32_t i44 = uint32_t(W109 - 0x2u);
                                            if (i44 > 0x1eu) {
                                                m.r[0] = arg0;
                                                m.r[1] = i44;
                                                m.r[2] = W7;
                                                m.r[4] = ESP0;
                                                return m.ret(28);
                                            } else {
                                                switch (i44) {
                                                    case 0x0: {
                                                        const uint32_t W113 = g.u8(ESP0 + 0x1c);
                                                        const uint32_t i45 = uint32_t(W7 & 0xffffff00u);
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R114 = callr(g, 0x00864460, {arg5, arg0, arg1, uint32_t(uint32_t(i45 | uint32_t(uint32_t(W113 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg5).ECX(arg2).EDX(arg3).EBX(i2).ESI(ecx).EDI(ecx));
                                                        m.r[0] = R114;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0x1: case 0x3: case 0x4: case 0x5: case 0x7: case 0x8: case 0x9: case 0xa: case 0xb: case 0xc: case 0xd: case 0xf: case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: {
                                                        m.r[0] = arg0;
                                                        m.r[1] = uint32_t(g.u8(0x0085ed1c + i44));
                                                        m.r[2] = W7;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0x2: {
                                                        const uint32_t W115 = g.u8(ESP0 + 0x1c);
                                                        const uint32_t i46 = uint32_t(uint32_t(g.u8(0x0085ed1c + i44)) & 0xffffff00u);
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R116 = callr(g, 0x0086a1e0, {arg3, arg4, arg5, arg1, uint32_t(uint32_t(i46 | uint32_t(uint32_t(W115 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg0).ECX(arg0).EDX(arg2).EBX(i2).ESI(arg3).EDI(ecx));
                                                        m.r[0] = R116;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0x6: {
                                                        const uint32_t W117 = g.u8(ESP0 + 0x1c);
                                                        const uint32_t i47 = uint32_t(W7 & 0xffffff00u);
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R118 = callr(g, 0x0086e750, {ecx, arg3, arg4, arg5, arg1, uint32_t(uint32_t(i47 | uint32_t(uint32_t(W117 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg0).ECX(arg0).EDX(arg2).EBX(i2).ESI(arg3).EDI(ecx));
                                                        m.r[0] = R118;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    case 0xe: {
                                                        const uint32_t W119 = g.u8(ESP0 + 0x1c);
                                                        const uint32_t i48 = uint32_t(uint32_t(g.u8(0x0085ed1c + i44)) & 0xffffff00u);
                                                        m.r[4] = A - 0x10;
                                                        const uint32_t R120 = callr(g, 0x0086fac0, {arg3, arg5, arg0, arg1, uint32_t(uint32_t(i48 | uint32_t(uint32_t(W119 == 0x0u ? 1 : 0) & 0xffu)) & 0xffu)}, In().EAX(arg0).ECX(arg2).EDX(arg4).EBX(i2).ESI(ecx).EDI(ecx));
                                                        m.r[0] = R120;
                                                        m.r[4] = ESP0;
                                                        return m.ret(28);
                                                    }
                                                    default: break;
                                                }
                                                throw std::logic_error("0x0 switch value outside the table");
                                            }
                                        }
                                    }
                                }
                                default: break;
                            }
                            throw std::logic_error("0x0 switch value outside the table");
                        }
                    }
                }
            }
        }
    }
}

// 0085ee70 FUN_0085ee70: batched segment / triangle test of Triangles_DynamicRayContact (six vec4 rays against a triangle, masks counted with integer sums); EAX = hit
REFLEX_PORT_FN(0x0085ee70, Fn_0085ee70, "mechanism", OUT_EAX | OUT_EDX) {
    Guest g{m}; const uint32_t ESP0 = m.r[4], ecx = m.r[1], edx = m.r[2], arg0 = m.arg(0), arg1 = m.arg(1), arg2 = m.arg(2), arg3 = m.arg(3), arg4 = m.arg(4), arg5 = m.arg(5);
    (void)ESP0;
    const uint32_t W1 = g.u32(0x009777c4);
    const V4 L2 = ld(g, arg2);
    const V4 L3 = ld(g, arg2 + 0x10);
    const V4 L4 = ld(g, arg2 + 0x20);
    const V4 L5 = ld(g, arg2 + 0x30);
    const V4 L6 = ld(g, arg2 + 0x40);
    const V4 L7 = ld(g, arg2 + 0x50);
    const uint32_t W8 = g.u32(0x00977494);
    st(g, arg4, V4::splat(flt(W8)));
    st(g, arg4 + 0x10, V4::splat(flt(W8)));
    st(g, arg4 + 0x20, V4::splat(flt(W8)));
    st(g, arg4 + 0x30, V4::splat(flt(W8)));
    st(g, arg4 + 0x40, V4::splat(flt(W8)));
    st(g, arg4 + 0x50, V4::splat(flt(W8)));
    g.u32(arg3, 0x0u);
    g.u32(arg3 + 0x4, 0x0u);
    g.u32(arg3 + 0x8, 0x0u);
    g.u32(arg3 + 0xc, 0x0u);
    g.u32(arg3 + 0x10, 0x0u);
    g.u32(arg3 + 0x14, 0x0u);
    const V4 L9 = ld(g, ecx);
    const V4 L10 = ld(g, ecx + 0x10);
    const V4 L11 = ld(g, ecx + 0x20);
    const V4 L12 = ld(g, ecx + 0x30);
    const V4 L13 = ld(g, ecx + 0x40);
    const V4 L14 = ld(g, ecx + 0x50);
    const V4 L15 = ld(g, 0x0092dc20);
    const V4 L16 = ld(g, 0x0092dc30);
    const V4 L17 = ld(g, 0x0092dc20);
    const V4 L18 = ld(g, 0x0092dc20);
    const V4 L19 = ld(g, 0x0092dc20);
    const V4 L20 = ld(g, 0x0092dc20);
    const V4 L21 = ld(g, 0x0092dc20);
    const V4 L22 = ld(g, 0x0092dc30);
    const V4 L23 = ld(g, edx);
    const V4 L24 = ld(g, arg0);
    const V4 L25 = ld(g, edx + 0x10);
    const V4 L26 = ld(g, edx + 0x20);
    const V4 L27 = ld(g, arg0 + 0x10);
    const V4 L28 = ld(g, arg0 + 0x20);
    const V4 L29 = ld(g, arg0 + 0x30);
    const V4 L30 = ld(g, edx + 0x30);
    const V4 L31 = ld(g, edx + 0x40);
    const V4 L32 = ld(g, arg0 + 0x40);
    const V4 L33 = ld(g, edx + 0x50);
    const V4 L34 = ld(g, arg0 + 0x50);
    const V4 L35 = ld(g, arg1);
    const V4 L36 = ld(g, arg1 + 0x10);
    const V4 L37 = ld(g, arg1 + 0x20);
    const V4 L38 = ld(g, arg1 + 0x30);
    const V4 L39 = ld(g, arg1 + 0x40);
    const V4 L40 = ld(g, arg1 + 0x50);
    const V4 L41 = ld(g, arg2);
    const V4 L42 = ld(g, arg2 + 0x10);
    const V4 L43 = ld(g, arg2 + 0x20);
    const V4 L44 = ld(g, arg2 + 0x30);
    const V4 L45 = ld(g, arg2 + 0x40);
    const V4 L46 = ld(g, arg2 + 0x50);
    const V4 L47 = ld(g, 0x0092de10);
    const float t1 = sub(L34.f[0], L33.f[0]);
    const float t2 = sub(L14.f[0], L7.f[0]);
    const float t3 = mul(t2, t2);
    const float t4 = sub(L14.f[1], L7.f[1]);
    const float t5 = mul(t4, t4);
    const float t6 = sub(L14.f[2], L7.f[2]);
    const float t7 = mul(t6, t6);
    const float t8 = add(add(t3, t5), t7);
    const float t9 = x86::sqrt(t8);
    const float t10 = flt(cmp_pred(flt(0x00000000), t9, 4) ? 0xffffffffu : 0u);
    const float t11 = flt(cmp_pred(t8, L21.f[2], 4) ? 0xffffffffu : 0u);
    const float t12 = flt(bits(flt(bits(flt(bits(t10) & bits(div(t6, t9)))) & bits(t11))) | bits(flt(~bits(t11) & bits(L22.f[2]))));
    const float t13 = sub(L40.f[1], L33.f[1]);
    const float t14 = flt(cmp_pred(t8, L21.f[1], 4) ? 0xffffffffu : 0u);
    const float t15 = flt(bits(flt(bits(flt(bits(t10) & bits(div(t4, t9)))) & bits(t14))) | bits(flt(~bits(t14) & bits(L22.f[1]))));
    const float t16 = sub(L40.f[2], L33.f[2]);
    const float t17 = mul(sub(mul(t12, t13), mul(t15, t16)), L47.f[0]);
    const float t18 = sub(L34.f[1], L33.f[1]);
    const float t19 = flt(cmp_pred(t8, L21.f[0], 4) ? 0xffffffffu : 0u);
    const float t20 = flt(bits(flt(bits(flt(bits(t10) & bits(div(t2, t9)))) & bits(t19))) | bits(flt(~bits(t19) & bits(L22.f[0]))));
    const float t21 = sub(L40.f[0], L33.f[0]);
    const float t22 = mul(sub(mul(t20, t16), mul(t12, t21)), L47.f[1]);
    const float t23 = sub(L34.f[2], L33.f[2]);
    const float t24 = mul(sub(mul(t15, t21), mul(t20, t13)), L47.f[2]);
    const float t25 = add(add(mul(t1, t17), mul(t18, t22)), mul(t23, t24));
    const uint32_t i26 = uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(flt(W1), t25, 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(flt(W1), t25, 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(flt(W1), t25, 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu));
    const float t27 = sub(L32.f[0], L31.f[0]);
    const float t28 = sub(L13.f[0], L6.f[0]);
    const float t29 = mul(t28, t28);
    const float t30 = sub(L13.f[1], L6.f[1]);
    const float t31 = mul(t30, t30);
    const float t32 = sub(L13.f[2], L6.f[2]);
    const float t33 = mul(t32, t32);
    const float t34 = add(add(t29, t31), t33);
    const float t35 = x86::sqrt(t34);
    const float t36 = flt(cmp_pred(flt(0x00000000), t35, 4) ? 0xffffffffu : 0u);
    const float t37 = flt(cmp_pred(t34, L20.f[2], 4) ? 0xffffffffu : 0u);
    const float t38 = flt(bits(flt(bits(flt(bits(t36) & bits(div(t32, t35)))) & bits(t37))) | bits(flt(~bits(t37) & bits(L16.f[2]))));
    const float t39 = sub(L39.f[1], L31.f[1]);
    const float t40 = flt(cmp_pred(t34, L20.f[1], 4) ? 0xffffffffu : 0u);
    const float t41 = flt(bits(flt(bits(flt(bits(t36) & bits(div(t30, t35)))) & bits(t40))) | bits(flt(~bits(t40) & bits(L16.f[1]))));
    const float t42 = sub(L39.f[2], L31.f[2]);
    const float t43 = mul(sub(mul(t38, t39), mul(t41, t42)), L47.f[0]);
    const float t44 = sub(L32.f[1], L31.f[1]);
    const float t45 = flt(cmp_pred(t34, L20.f[0], 4) ? 0xffffffffu : 0u);
    const float t46 = flt(bits(flt(bits(flt(bits(t36) & bits(div(t28, t35)))) & bits(t45))) | bits(flt(~bits(t45) & bits(L16.f[0]))));
    const float t47 = sub(L39.f[0], L31.f[0]);
    const float t48 = mul(sub(mul(t46, t42), mul(t38, t47)), L47.f[1]);
    const float t49 = sub(L32.f[2], L31.f[2]);
    const float t50 = mul(sub(mul(t41, t47), mul(t46, t39)), L47.f[2]);
    const float t51 = add(add(mul(t27, t43), mul(t44, t48)), mul(t49, t50));
    const uint32_t i52 = uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(flt(W1), t51, 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(flt(W1), t51, 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(flt(W1), t51, 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu));
    const float t53 = sub(L29.f[0], L30.f[0]);
    const float t54 = sub(L12.f[0], L5.f[0]);
    const float t55 = mul(t54, t54);
    const float t56 = sub(L12.f[1], L5.f[1]);
    const float t57 = mul(t56, t56);
    const float t58 = sub(L12.f[2], L5.f[2]);
    const float t59 = mul(t58, t58);
    const float t60 = add(add(t55, t57), t59);
    const float t61 = x86::sqrt(t60);
    const float t62 = flt(cmp_pred(flt(0x00000000), t61, 4) ? 0xffffffffu : 0u);
    const float t63 = flt(cmp_pred(t60, L19.f[2], 4) ? 0xffffffffu : 0u);
    const float t64 = flt(bits(flt(bits(flt(bits(t62) & bits(div(t58, t61)))) & bits(t63))) | bits(flt(~bits(t63) & bits(L16.f[2]))));
    const float t65 = sub(L38.f[1], L30.f[1]);
    const float t66 = flt(cmp_pred(t60, L19.f[1], 4) ? 0xffffffffu : 0u);
    const float t67 = flt(bits(flt(bits(flt(bits(t62) & bits(div(t56, t61)))) & bits(t66))) | bits(flt(~bits(t66) & bits(L16.f[1]))));
    const float t68 = sub(L38.f[2], L30.f[2]);
    const float t69 = mul(sub(mul(t64, t65), mul(t67, t68)), L47.f[0]);
    const float t70 = sub(L29.f[1], L30.f[1]);
    const float t71 = flt(cmp_pred(t60, L19.f[0], 4) ? 0xffffffffu : 0u);
    const float t72 = flt(bits(flt(bits(flt(bits(t62) & bits(div(t54, t61)))) & bits(t71))) | bits(flt(~bits(t71) & bits(L16.f[0]))));
    const float t73 = sub(L38.f[0], L30.f[0]);
    const float t74 = mul(sub(mul(t72, t68), mul(t64, t73)), L47.f[1]);
    const float t75 = sub(L29.f[2], L30.f[2]);
    const float t76 = mul(sub(mul(t67, t73), mul(t72, t65)), L47.f[2]);
    const float t77 = add(add(mul(t53, t69), mul(t70, t74)), mul(t75, t76));
    const uint32_t i78 = uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(flt(W1), t77, 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(flt(W1), t77, 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(flt(W1), t77, 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu));
    const float t79 = sub(L28.f[0], L26.f[0]);
    const float t80 = sub(L11.f[0], L4.f[0]);
    const float t81 = mul(t80, t80);
    const float t82 = sub(L11.f[1], L4.f[1]);
    const float t83 = mul(t82, t82);
    const float t84 = sub(L11.f[2], L4.f[2]);
    const float t85 = mul(t84, t84);
    const float t86 = add(add(t81, t83), t85);
    const float t87 = x86::sqrt(t86);
    const float t88 = flt(cmp_pred(flt(0x00000000), t87, 4) ? 0xffffffffu : 0u);
    const float t89 = flt(cmp_pred(t86, L18.f[2], 4) ? 0xffffffffu : 0u);
    const float t90 = flt(bits(flt(bits(flt(bits(t88) & bits(div(t84, t87)))) & bits(t89))) | bits(flt(~bits(t89) & bits(L16.f[2]))));
    const float t91 = sub(L37.f[1], L26.f[1]);
    const float t92 = flt(cmp_pred(t86, L18.f[1], 4) ? 0xffffffffu : 0u);
    const float t93 = flt(bits(flt(bits(flt(bits(t88) & bits(div(t82, t87)))) & bits(t92))) | bits(flt(~bits(t92) & bits(L16.f[1]))));
    const float t94 = sub(L37.f[2], L26.f[2]);
    const float t95 = mul(sub(mul(t90, t91), mul(t93, t94)), L47.f[0]);
    const float t96 = sub(L28.f[1], L26.f[1]);
    const float t97 = flt(cmp_pred(t86, L18.f[0], 4) ? 0xffffffffu : 0u);
    const float t98 = flt(bits(flt(bits(flt(bits(t88) & bits(div(t80, t87)))) & bits(t97))) | bits(flt(~bits(t97) & bits(L16.f[0]))));
    const float t99 = sub(L37.f[0], L26.f[0]);
    const float t100 = mul(sub(mul(t98, t94), mul(t90, t99)), L47.f[1]);
    const float t101 = sub(L28.f[2], L26.f[2]);
    const float t102 = mul(sub(mul(t93, t99), mul(t98, t91)), L47.f[2]);
    const float t103 = add(add(mul(t79, t95), mul(t96, t100)), mul(t101, t102));
    const uint32_t i104 = uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(flt(W1), t103, 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(flt(W1), t103, 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(flt(W1), t103, 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu));
    const float t105 = sub(L27.f[0], L25.f[0]);
    const float t106 = sub(L10.f[0], L3.f[0]);
    const float t107 = mul(t106, t106);
    const float t108 = sub(L10.f[1], L3.f[1]);
    const float t109 = mul(t108, t108);
    const float t110 = sub(L10.f[2], L3.f[2]);
    const float t111 = mul(t110, t110);
    const float t112 = add(add(t107, t109), t111);
    const float t113 = x86::sqrt(t112);
    const float t114 = flt(cmp_pred(flt(0x00000000), t113, 4) ? 0xffffffffu : 0u);
    const float t115 = flt(cmp_pred(t112, L17.f[2], 4) ? 0xffffffffu : 0u);
    const float t116 = flt(bits(flt(bits(flt(bits(t114) & bits(div(t110, t113)))) & bits(t115))) | bits(flt(~bits(t115) & bits(L16.f[2]))));
    const float t117 = sub(L36.f[1], L25.f[1]);
    const float t118 = flt(cmp_pred(t112, L17.f[1], 4) ? 0xffffffffu : 0u);
    const float t119 = flt(bits(flt(bits(flt(bits(t114) & bits(div(t108, t113)))) & bits(t118))) | bits(flt(~bits(t118) & bits(L16.f[1]))));
    const float t120 = sub(L36.f[2], L25.f[2]);
    const float t121 = mul(sub(mul(t116, t117), mul(t119, t120)), L47.f[0]);
    const float t122 = sub(L27.f[1], L25.f[1]);
    const float t123 = flt(cmp_pred(t112, L17.f[0], 4) ? 0xffffffffu : 0u);
    const float t124 = flt(bits(flt(bits(flt(bits(t114) & bits(div(t106, t113)))) & bits(t123))) | bits(flt(~bits(t123) & bits(L16.f[0]))));
    const float t125 = sub(L36.f[0], L25.f[0]);
    const float t126 = mul(sub(mul(t124, t120), mul(t116, t125)), L47.f[1]);
    const float t127 = sub(L27.f[2], L25.f[2]);
    const float t128 = mul(sub(mul(t119, t125), mul(t124, t117)), L47.f[2]);
    const float t129 = add(add(mul(t105, t121), mul(t122, t126)), mul(t127, t128));
    const uint32_t i130 = uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(flt(W1), t129, 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(flt(W1), t129, 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(flt(W1), t129, 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu));
    const float t131 = sub(L9.f[2], L2.f[2]);
    const float t132 = sub(L9.f[0], L2.f[0]);
    const float t133 = mul(t132, t132);
    const float t134 = sub(L9.f[1], L2.f[1]);
    const float t135 = mul(t134, t134);
    const float t136 = mul(t131, t131);
    const float t137 = add(add(t133, t135), t136);
    const float t138 = x86::sqrt(t137);
    const float t139 = flt(cmp_pred(flt(0x00000000), t138, 4) ? 0xffffffffu : 0u);
    const float t140 = flt(cmp_pred(t137, L15.f[2], 4) ? 0xffffffffu : 0u);
    const float t141 = flt(bits(flt(bits(flt(bits(div(t131, t138)) & bits(t139))) & bits(t140))) | bits(flt(~bits(t140) & bits(L16.f[2]))));
    const float t142 = sub(L35.f[1], L23.f[1]);
    const float t143 = flt(cmp_pred(t137, L15.f[1], 4) ? 0xffffffffu : 0u);
    const float t144 = flt(bits(flt(bits(flt(bits(div(t134, t138)) & bits(t139))) & bits(t143))) | bits(flt(~bits(t143) & bits(L16.f[1]))));
    const float t145 = sub(L35.f[2], L23.f[2]);
    const float t146 = mul(sub(mul(t141, t142), mul(t144, t145)), L47.f[0]);
    const float t147 = sub(L24.f[0], L23.f[0]);
    const float t148 = flt(cmp_pred(t137, L15.f[0], 4) ? 0xffffffffu : 0u);
    const float t149 = flt(bits(flt(bits(flt(bits(div(t132, t138)) & bits(t139))) & bits(t148))) | bits(flt(~bits(t148) & bits(L16.f[0]))));
    const float t150 = sub(L35.f[0], L23.f[0]);
    const float t151 = mul(sub(mul(t149, t145), mul(t141, t150)), L47.f[1]);
    const float t152 = sub(L24.f[1], L23.f[1]);
    const float t153 = mul(sub(mul(t144, t150), mul(t149, t142)), L47.f[2]);
    const float t154 = sub(L24.f[2], L23.f[2]);
    const float t155 = add(add(mul(t146, t147), mul(t151, t152)), mul(t153, t154));
    const uint32_t i156 = uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(flt(W1), t155, 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(flt(W1), t155, 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(flt(W1), t155, 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu));
    const uint32_t i157 = uint32_t(uint32_t(uint32_t(uint32_t(uint32_t(i26 | i52) | i78) | i104) | i130) | i156);
    if (i157 != 0x0u) {
        const V4 L48 = ld(g, 0x00d61650);
        const V4 L49 = ld(g, 0x00d61670);
        const V4 L50 = ld(g, 0x00d61670);
        const V4 L51 = ld(g, 0x00d61670);
        const V4 L52 = ld(g, 0x00d61670);
        const V4 L53 = ld(g, 0x00d61670);
        const V4 L54 = ld(g, 0x00d61670);
        const float t158 = sub(L46.f[0], L33.f[0]);
        const float t159 = sub(L46.f[1], L33.f[1]);
        const float t160 = sub(L46.f[2], L33.f[2]);
        const float t161 = add(add(mul(t158, t17), mul(t159, t22)), mul(t160, t24));
        const float t162 = div(L48.f[2], t25);
        const float t163 = mul(t161, t162);
        const float t164 = div(L48.f[1], t25);
        const float t165 = mul(t161, t164);
        const float t166 = div(L48.f[0], t25);
        const float t167 = mul(t161, t166);
        const uint32_t i168 = uint32_t(uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(L54.f[2], t163, 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(L54.f[1], t165, 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(L54.f[0], t167, 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu)) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(t163, L48.f[2], 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(t165, L48.f[1], 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(t167, L48.f[0], 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu)));
        const float t169 = sub(L45.f[0], L31.f[0]);
        const float t170 = sub(L45.f[1], L31.f[1]);
        const float t171 = sub(L45.f[2], L31.f[2]);
        const float t172 = add(add(mul(t169, t43), mul(t170, t48)), mul(t171, t50));
        const float t173 = div(L48.f[2], t51);
        const float t174 = mul(t172, t173);
        const float t175 = div(L48.f[1], t51);
        const float t176 = mul(t172, t175);
        const float t177 = div(L48.f[0], t51);
        const float t178 = mul(t172, t177);
        const uint32_t i179 = uint32_t(uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(L53.f[2], t174, 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(L53.f[1], t176, 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(L53.f[0], t178, 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu)) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(t174, L48.f[2], 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(t176, L48.f[1], 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(t178, L48.f[0], 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu)));
        const float t180 = sub(L44.f[0], L30.f[0]);
        const float t181 = sub(L44.f[1], L30.f[1]);
        const float t182 = sub(L44.f[2], L30.f[2]);
        const float t183 = add(add(mul(t180, t69), mul(t181, t74)), mul(t182, t76));
        const float t184 = div(L48.f[2], t77);
        const float t185 = mul(t183, t184);
        const float t186 = div(L48.f[1], t77);
        const float t187 = mul(t183, t186);
        const float t188 = div(L48.f[0], t77);
        const float t189 = mul(t183, t188);
        const uint32_t i190 = uint32_t(uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(L52.f[2], t185, 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(L52.f[1], t187, 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(L52.f[0], t189, 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu)) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(t185, L48.f[2], 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(t187, L48.f[1], 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(t189, L48.f[0], 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu)));
        const float t191 = sub(L43.f[0], L26.f[0]);
        const float t192 = sub(L43.f[1], L26.f[1]);
        const float t193 = sub(L43.f[2], L26.f[2]);
        const float t194 = add(add(mul(t191, t95), mul(t192, t100)), mul(t193, t102));
        const float t195 = div(L48.f[2], t103);
        const float t196 = mul(t194, t195);
        const float t197 = div(L48.f[1], t103);
        const float t198 = mul(t194, t197);
        const float t199 = div(L48.f[0], t103);
        const float t200 = mul(t194, t199);
        const uint32_t i201 = uint32_t(uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(L51.f[2], t196, 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(L51.f[1], t198, 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(L51.f[0], t200, 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu)) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(t196, L48.f[2], 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(t198, L48.f[1], 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(t200, L48.f[0], 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu)));
        const float t202 = sub(L42.f[0], L25.f[0]);
        const float t203 = sub(L42.f[1], L25.f[1]);
        const float t204 = sub(L42.f[2], L25.f[2]);
        const float t205 = add(add(mul(t202, t121), mul(t203, t126)), mul(t204, t128));
        const float t206 = div(L48.f[2], t129);
        const float t207 = mul(t205, t206);
        const float t208 = div(L48.f[1], t129);
        const float t209 = mul(t205, t208);
        const float t210 = div(L48.f[0], t129);
        const float t211 = mul(t205, t210);
        const uint32_t i212 = uint32_t(uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(L50.f[2], t207, 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(L50.f[1], t209, 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(L50.f[0], t211, 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu)) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(t207, L48.f[2], 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(t209, L48.f[1], 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(t211, L48.f[0], 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu)));
        const float t213 = div(L48.f[2], t155);
        const float t214 = sub(L41.f[0], L23.f[0]);
        const float t215 = sub(L41.f[1], L23.f[1]);
        const float t216 = sub(L41.f[2], L23.f[2]);
        const float t217 = add(add(mul(t146, t214), mul(t151, t215)), mul(t153, t216));
        const float t218 = mul(t213, t217);
        const float t219 = div(L48.f[1], t155);
        const float t220 = mul(t219, t217);
        const float t221 = div(L48.f[0], t155);
        const float t222 = mul(t221, t217);
        const uint32_t i223 = uint32_t(uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(L49.f[2], t218, 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(L49.f[1], t220, 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(L49.f[0], t222, 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu)) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(t218, L48.f[2], 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(t220, L48.f[1], 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(t222, L48.f[0], 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu)));
        if (uint32_t(uint32_t(uint32_t(uint32_t(uint32_t(i168 | i179) | i190) | i201) | i212) | i223) == 0x0u) {
            m.r[0] = 0x0u;
            m.r[2] = i223;
            m.r[4] = ESP0;
            return m.ret(0);
        } else {
            const V4 L55 = ld(g, 0x00d61670);
            const V4 L56 = ld(g, 0x00d61650);
            const float t224 = mul(sub(mul(t160, t18), mul(t159, t23)), L47.f[0]);
            const float t225 = mul(sub(mul(t158, t23), mul(t160, t1)), L47.f[1]);
            const float t226 = mul(sub(mul(t159, t1), mul(t158, t18)), L47.f[2]);
            const float t227 = add(add(mul(t224, t20), mul(t225, t15)), mul(t226, t12));
            const float t228 = mul(t227, t162);
            const float t229 = mul(t227, t164);
            const float t230 = mul(t227, t166);
            const uint32_t i231 = uint32_t(uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(L55.f[2], t228, 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(L55.f[1], t229, 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(L55.f[0], t230, 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu)) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(add(t228, t163), L56.f[2], 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(add(t229, t165), L56.f[1], 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(add(t230, t167), L56.f[0], 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu)));
            const float t232 = mul(sub(mul(t171, t44), mul(t170, t49)), L47.f[0]);
            const float t233 = mul(sub(mul(t169, t49), mul(t171, t27)), L47.f[1]);
            const float t234 = mul(sub(mul(t170, t27), mul(t169, t44)), L47.f[2]);
            const float t235 = add(add(mul(t232, t46), mul(t233, t41)), mul(t234, t38));
            const float t236 = mul(t235, t173);
            const float t237 = mul(t235, t175);
            const float t238 = mul(t235, t177);
            const uint32_t i239 = uint32_t(uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(L55.f[2], t236, 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(L55.f[1], t237, 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(L55.f[0], t238, 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu)) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(add(t236, t174), L56.f[2], 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(add(t237, t176), L56.f[1], 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(add(t238, t178), L56.f[0], 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu)));
            const float t240 = mul(sub(mul(t182, t70), mul(t181, t75)), L47.f[0]);
            const float t241 = mul(sub(mul(t180, t75), mul(t182, t53)), L47.f[1]);
            const float t242 = mul(sub(mul(t181, t53), mul(t180, t70)), L47.f[2]);
            const float t243 = add(add(mul(t240, t72), mul(t241, t67)), mul(t242, t64));
            const float t244 = mul(t243, t184);
            const float t245 = mul(t243, t186);
            const float t246 = mul(t243, t188);
            const uint32_t i247 = uint32_t(uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(L55.f[2], t244, 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(L55.f[1], t245, 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(L55.f[0], t246, 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu)) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(add(t244, t185), L56.f[2], 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(add(t245, t187), L56.f[1], 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(add(t246, t189), L56.f[0], 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu)));
            const float t248 = mul(sub(mul(t193, t96), mul(t192, t101)), L47.f[0]);
            const float t249 = mul(sub(mul(t191, t101), mul(t193, t79)), L47.f[1]);
            const float t250 = mul(sub(mul(t192, t79), mul(t191, t96)), L47.f[2]);
            const float t251 = add(add(mul(t248, t98), mul(t249, t93)), mul(t250, t90));
            const float t252 = mul(t251, t195);
            const float t253 = mul(t251, t197);
            const float t254 = mul(t251, t199);
            const uint32_t i255 = uint32_t(uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(L55.f[2], t252, 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(L55.f[1], t253, 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(L55.f[0], t254, 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu)) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(add(t252, t196), L56.f[2], 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(add(t253, t198), L56.f[1], 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(add(t254, t200), L56.f[0], 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu)));
            const float t256 = mul(sub(mul(t204, t122), mul(t203, t127)), L47.f[0]);
            const float t257 = mul(sub(mul(t202, t127), mul(t204, t105)), L47.f[1]);
            const float t258 = mul(sub(mul(t203, t105), mul(t202, t122)), L47.f[2]);
            const float t259 = add(add(mul(t256, t124), mul(t257, t119)), mul(t258, t116));
            const float t260 = mul(t259, t206);
            const float t261 = mul(t259, t208);
            const float t262 = mul(t259, t210);
            const uint32_t i263 = uint32_t(uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(L55.f[2], t260, 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(L55.f[1], t261, 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(L55.f[0], t262, 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu)) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(add(t260, t207), L56.f[2], 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(add(t261, t209), L56.f[1], 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(add(t262, t211), L56.f[0], 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu)));
            const float t264 = mul(sub(mul(t216, t152), mul(t215, t154)), L47.f[0]);
            const float t265 = mul(sub(mul(t214, t154), mul(t216, t147)), L47.f[1]);
            const float t266 = mul(sub(mul(t215, t147), mul(t214, t152)), L47.f[2]);
            const float t267 = add(add(mul(t149, t264), mul(t144, t265)), mul(t141, t266));
            const float t268 = mul(t267, t213);
            const float t269 = mul(t267, t219);
            const float t270 = mul(t267, t221);
            const uint32_t i271 = uint32_t(uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(L55.f[2], t268, 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(L55.f[1], t269, 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(L55.f[0], t270, 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu)) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(add(t268, t218), L56.f[2], 2) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(add(t269, t220), L56.f[1], 2) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(add(t270, t222), L56.f[0], 2) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu)));
            if (uint32_t(uint32_t(uint32_t(uint32_t(uint32_t(i231 | i239) | i247) | i255) | i263) | i271) == 0x0u) {
                m.r[0] = 0x0u;
                m.r[2] = i223;
                m.r[4] = ESP0;
                return m.ret(0);
            } else {
                const float t272 = add(add(mul(t150, t264), mul(t142, t265)), mul(t145, t266));
                const float t273 = mul(t272, t213);
                const float t274 = mul(t272, t219);
                const float t275 = mul(t272, t221);
                const float t276 = x86::sqrt(add(add(t133, t136), t135));
                const uint32_t i277 = uint32_t(uint32_t(uint32_t(uint32_t(uint32_t(i223 & i156) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(t273, flt(W8), 1) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(t274, flt(W8), 1) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(t275, flt(W8), 1) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu))) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(L55.f[2], t273, 1) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(L55.f[1], t274, 1) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(L55.f[0], t275, 1) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu))) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(t273, t276, 1) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(t274, t276, 1) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(t275, t276, 1) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu))) & i271);
                g.u32(arg3, i277);
                const float t278 = add(add(mul(t256, t125), mul(t257, t117)), mul(t258, t120));
                const float t279 = mul(t278, t206);
                const float t280 = mul(t278, t208);
                const float t281 = mul(t278, t210);
                const float t282 = x86::sqrt(add(add(t107, t111), t109));
                const uint32_t i283 = uint32_t(uint32_t(uint32_t(uint32_t(uint32_t(i212 & i130) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(t279, flt(W8), 1) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(t280, flt(W8), 1) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(t281, flt(W8), 1) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu))) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(L55.f[2], t279, 1) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(L55.f[1], t280, 1) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(L55.f[0], t281, 1) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu))) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(t279, t282, 1) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(t280, t282, 1) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(t281, t282, 1) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu))) & i263);
                g.u32(arg3 + 0x4, i283);
                const float t284 = add(add(mul(t248, t99), mul(t249, t91)), mul(t250, t94));
                const float t285 = mul(t284, t195);
                const float t286 = mul(t284, t197);
                const float t287 = mul(t284, t199);
                const float t288 = x86::sqrt(add(add(t81, t85), t83));
                const uint32_t i289 = uint32_t(uint32_t(uint32_t(uint32_t(uint32_t(i201 & i104) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(t285, flt(W8), 1) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(t286, flt(W8), 1) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(t287, flt(W8), 1) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu))) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(L55.f[2], t285, 1) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(L55.f[1], t286, 1) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(L55.f[0], t287, 1) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu))) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(t285, t288, 1) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(t286, t288, 1) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(t287, t288, 1) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu))) & i255);
                g.u32(arg3 + 0x8, i289);
                const float t290 = add(add(mul(t240, t73), mul(t241, t65)), mul(t242, t68));
                const float t291 = mul(t290, t184);
                const float t292 = mul(t290, t186);
                const float t293 = mul(t290, t188);
                const float t294 = x86::sqrt(add(add(t55, t59), t57));
                const uint32_t i295 = uint32_t(uint32_t(uint32_t(uint32_t(uint32_t(i190 & i78) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(t291, flt(W8), 1) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(t292, flt(W8), 1) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(t293, flt(W8), 1) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu))) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(L55.f[2], t291, 1) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(L55.f[1], t292, 1) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(L55.f[0], t293, 1) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu))) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(t291, t294, 1) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(t292, t294, 1) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(t293, t294, 1) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu))) & i247);
                g.u32(arg3 + 0xc, i295);
                const float t296 = add(add(mul(t232, t47), mul(t233, t39)), mul(t234, t42));
                const float t297 = mul(t296, t173);
                const float t298 = mul(t296, t175);
                const float t299 = mul(t296, t177);
                const float t300 = x86::sqrt(add(add(t29, t33), t31));
                const uint32_t i301 = uint32_t(uint32_t(uint32_t(uint32_t(uint32_t(i179 & i52) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(t297, flt(W8), 1) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(t298, flt(W8), 1) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(t299, flt(W8), 1) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu))) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(L55.f[2], t297, 1) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(L55.f[1], t298, 1) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(L55.f[0], t299, 1) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu))) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(t297, t300, 1) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(t298, t300, 1) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(t299, t300, 1) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu))) & i239);
                g.u32(arg3 + 0x10, i301);
                const float t302 = add(add(mul(t224, t21), mul(t225, t13)), mul(t226, t16));
                const float t303 = mul(t302, t162);
                const float t304 = mul(t302, t164);
                const float t305 = mul(t302, t166);
                const float t306 = x86::sqrt(add(add(t3, t7), t5));
                const uint32_t i307 = uint32_t(uint32_t(uint32_t(uint32_t(uint32_t(i168 & i26) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(t303, flt(W8), 1) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(t304, flt(W8), 1) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(t305, flt(W8), 1) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu))) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(L55.f[2], t303, 1) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(L55.f[1], t304, 1) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(L55.f[0], t305, 1) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu))) & uint32_t(0x0u | uint32_t(uint32_t(uint32_t(uint32_t(bits(flt(cmp_pred(t303, t306, 1) ? 0xffffffffu : 0u)) + bits(flt(cmp_pred(t304, t306, 1) ? 0xffffffffu : 0u))) + bits(flt(cmp_pred(t305, t306, 1) ? 0xffffffffu : 0u))) == 0xfffffffdu ? 1 : 0) & 0xffu))) & i231);
                g.u32(arg3 + 0x14, i307);
                const uint32_t i308 = uint32_t(uint32_t(uint32_t(uint32_t(uint32_t(i307 | i301) | i295) | i289) | i283) | i277);
                if (i308 == 0x0u) {
                    m.r[0] = i308;
                    m.r[2] = i277;
                    m.r[4] = ESP0;
                    return m.ret(0);
                } else {
                    const V4 v1 = sub(L9, L2);
                    const V4 v2 = cmpps(V4::splat(t137), L15, 4);
                    const V4 v3 = orps(andps(andps(div(v1, V4::splat(t138)), V4::splat(t139)), v2), andnps(v2, L16));
                    const V4 v4 = div(L48, V4::splat(t155));
                    const V4 v5 = mul(V4::splat(t272), v4);
                    st(g, arg4, add(mul(v3, v5), L41));
                    const V4 v6 = div(L48, V4::splat(t129));
                    const V4 v7 = mul(V4::splat(t278), v6);
                    const V4 v8 = sub(L10, L3);
                    const V4 v9 = cmpps(V4::splat(t112), L17, 4);
                    const V4 v10 = orps(andps(andps(V4::splat(t114), div(v8, V4::splat(t113))), v9), andnps(v9, L16));
                    st(g, arg4 + 0x10, add(mul(v7, v10), L42));
                    const V4 v11 = div(L48, V4::splat(t103));
                    const V4 v12 = mul(V4::splat(t284), v11);
                    const V4 v13 = sub(L11, L4);
                    const V4 v14 = cmpps(V4::splat(t86), L18, 4);
                    const V4 v15 = orps(andps(andps(V4::splat(t88), div(v13, V4::splat(t87))), v14), andnps(v14, L16));
                    st(g, arg4 + 0x20, add(mul(v12, v15), L43));
                    const V4 v16 = div(L48, V4::splat(t77));
                    const V4 v17 = mul(V4::splat(t290), v16);
                    const V4 v18 = sub(L12, L5);
                    const V4 v19 = cmpps(V4::splat(t60), L19, 4);
                    const V4 v20 = orps(andps(andps(V4::splat(t62), div(v18, V4::splat(t61))), v19), andnps(v19, L16));
                    st(g, arg4 + 0x30, add(mul(v17, v20), L44));
                    const V4 v21 = div(L48, V4::splat(t51));
                    const V4 v22 = mul(V4::splat(t296), v21);
                    const V4 v23 = sub(L13, L6);
                    const V4 v24 = cmpps(V4::splat(t34), L20, 4);
                    const V4 v25 = orps(andps(andps(V4::splat(t36), div(v23, V4::splat(t35))), v24), andnps(v24, L16));
                    st(g, arg4 + 0x40, add(mul(v22, v25), L45));
                    const V4 v26 = div(L48, V4::splat(t25));
                    const V4 v27 = mul(V4::splat(t302), v26);
                    const V4 v28 = sub(L14, L7);
                    const V4 v29 = cmpps(V4::splat(t8), L21, 4);
                    const V4 v30 = orps(andps(andps(V4::splat(t10), div(v28, V4::splat(t9))), v29), andnps(v29, L22));
                    st(g, arg4 + 0x50, add(mul(v27, v30), L46));
                    const V4 v31 = sub(perm(L24, 2, 0, 1, 3), perm(L23, 2, 0, 1, 3));
                    const V4 v32 = sub(perm(L35, 1, 2, 0, 3), perm(L23, 1, 2, 0, 3));
                    const V4 v33 = mul(v31, v32);
                    const V4 v34 = mul(V4{{t152, t154, t147, v31.f[3]}}, V4{{t145, t150, t142, v32.f[3]}});
                    const V4 v35 = mul(sub(v33, v34), L47);
                    const float t309 = x86::sqrt(add(add(mul(v35.f[0], v35.f[0]), mul(v35.f[1], v35.f[1])), mul(v35.f[2], v35.f[2])));
                    st(g, arg5, div(v35, V4::splat(t309)));
                    const V4 v36 = sub(perm(L27, 2, 0, 1, 3), perm(L25, 2, 0, 1, 3));
                    const V4 v37 = sub(perm(L36, 1, 2, 0, 3), perm(L25, 1, 2, 0, 3));
                    const V4 v38 = mul(v36, v37);
                    const V4 v39 = mul(V4{{t122, t127, t105, v36.f[3]}}, V4{{t120, t125, t117, v37.f[3]}});
                    const V4 v40 = mul(sub(v38, v39), L47);
                    const float t310 = x86::sqrt(add(add(mul(v40.f[0], v40.f[0]), mul(v40.f[1], v40.f[1])), mul(v40.f[2], v40.f[2])));
                    st(g, arg5 + 0x10, div(v40, V4::splat(t310)));
                    const V4 v41 = sub(perm(L28, 2, 0, 1, 3), perm(L26, 2, 0, 1, 3));
                    const V4 v42 = sub(perm(L37, 1, 2, 0, 3), perm(L26, 1, 2, 0, 3));
                    const V4 v43 = mul(v41, v42);
                    const V4 v44 = mul(V4{{t96, t101, t79, v41.f[3]}}, V4{{t94, t99, t91, v42.f[3]}});
                    const V4 v45 = mul(sub(v43, v44), L47);
                    const float t311 = x86::sqrt(add(add(mul(v45.f[0], v45.f[0]), mul(v45.f[1], v45.f[1])), mul(v45.f[2], v45.f[2])));
                    st(g, arg5 + 0x20, div(v45, V4::splat(t311)));
                    const V4 v46 = sub(perm(L29, 2, 0, 1, 3), perm(L30, 2, 0, 1, 3));
                    const V4 v47 = sub(perm(L38, 1, 2, 0, 3), perm(L30, 1, 2, 0, 3));
                    const V4 v48 = mul(v46, v47);
                    const V4 v49 = mul(V4{{t70, t75, t53, v46.f[3]}}, V4{{t68, t73, t65, v47.f[3]}});
                    const V4 v50 = mul(sub(v48, v49), L47);
                    const float t312 = x86::sqrt(add(add(mul(v50.f[0], v50.f[0]), mul(v50.f[1], v50.f[1])), mul(v50.f[2], v50.f[2])));
                    st(g, arg5 + 0x30, div(v50, V4::splat(t312)));
                    const V4 v51 = sub(perm(L32, 2, 0, 1, 3), perm(L31, 2, 0, 1, 3));
                    const V4 v52 = sub(perm(L39, 1, 2, 0, 3), perm(L31, 1, 2, 0, 3));
                    const V4 v53 = mul(v51, v52);
                    const V4 v54 = mul(V4{{t44, t49, t27, v51.f[3]}}, V4{{t42, t47, t39, v52.f[3]}});
                    const V4 v55 = mul(sub(v53, v54), L47);
                    const float t313 = x86::sqrt(add(add(mul(v55.f[0], v55.f[0]), mul(v55.f[1], v55.f[1])), mul(v55.f[2], v55.f[2])));
                    st(g, arg5 + 0x40, div(v55, V4::splat(t313)));
                    const V4 v56 = sub(perm(L34, 2, 0, 1, 3), perm(L33, 2, 0, 1, 3));
                    const V4 v57 = sub(perm(L40, 1, 2, 0, 3), perm(L33, 1, 2, 0, 3));
                    const V4 v58 = mul(v56, v57);
                    const V4 v59 = mul(V4{{t18, t23, t1, v56.f[3]}}, V4{{t16, t21, t13, v57.f[3]}});
                    const V4 v60 = mul(sub(v58, v59), L47);
                    const float t314 = x86::sqrt(add(add(mul(v60.f[0], v60.f[0]), mul(v60.f[1], v60.f[1])), mul(v60.f[2], v60.f[2])));
                    st(g, arg5 + 0x50, div(v60, V4::splat(t314)));
                    m.r[0] = i308;
                    m.r[2] = i277;
                    m.r[4] = ESP0;
                    return m.ret(0);
                }
            }
        }
    } else {
        m.r[0] = 0x0u;
        m.r[2] = i157;
        m.r[4] = ESP0;
        return m.ret(0);
    }
}
