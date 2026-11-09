// World group, wreck share: small virtuals, the crowd / ambience audio helpers (0x00674xxx, 0x00679xxx, 0x0067abc0,
// 0x0067ecxx), the ClipRecorder (0x006aaxxx-0x006abxxx), and assorted 0x006bxxxx-0x0075xxxx functions.
#include "world_wreck_util.h"

using namespace world;
using namespace wreck;
using reflex_port::OUT_EAX;
using reflex_port::OUT_ECX;
using reflex_port::OUT_EDX;
using reflex_port::OUT_XMM_ALL;

// 00679000: a shared empty virtual (ret 4).
REFLEX_PORT_FN(0x00679000, SharedVf_Ret4, "world", OUT_EAX | OUT_ECX | OUT_EDX) { return m.ret(4); }
// 00703d50: a shared virtual returning 0x10 (type id).
REFLEX_PORT_FN(0x00703d50, SharedVf_Type10, "world", OUT_EAX | OUT_ECX | OUT_EDX) { return m.ret(0, 0x10); }
// 0065cab0 ForestSystem::vf22: the type id 0x100f.
REFLEX_PORT_FN(0x0065cab0, ForestSystem_vf22, "world", OUT_EAX | OUT_ECX | OUT_EDX) { return m.ret(0, 0x100f); }
// 006d7870 KeyMap::vf11 (thiscall, arg = key): the binding word of a key (36-byte entries from +0x28).
REFLEX_PORT_FN(0x006d7870, KeyMap_vf11, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    Guest g{m};
    const uint32_t k = m.arg(0);
    return m.ret(4, g.u32(m.r[ECX] + (k + k * 8) * 4 + 0x28));
}

// 0075c580 (EDX = track object): the active sound table (+0x5a54 when mode 0x20 is on, +0x20c8 when mode 0x1c is on), else 0.
REFLEX_PORT_FN(0x0075c580, Fn_0075c580, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    Regs r(m);
    r.eax = r.u32(0xa95844);
    const uint32_t s = r.u32(r.eax + 0x20);
    r.ecx = 1;
    if (r.u32(s + 0x34) == 1) { r.eax = r.u32(r.edx + 0x5a54); return r.ret(0); }
    r.eax = r.u32(r.eax + 0x1c);
    if (r.u32(r.eax + 0x34) == 1) { r.eax = r.u32(r.edx + 0x20c8); return r.ret(0); }
    r.eax = 0;
    return r.ret(0);
}

// 0067ec50 / 0067eca0: whether the active sound table (as 0x0075c580 on 0xa946b8) has (+0x20018 != 0) / lacks the crowd bank.
static uint32_t active_table(Regs& r) {
    r.eax = r.u32(0xa946b8);
    r.edx = 1;
    if (r.eax == 0) return 0;
    r.ecx = r.u32(0xa95844);
    const uint32_t s = r.u32(r.ecx + 0x20);
    if (r.u32(s + 0x34) == 1) return r.u32(r.eax + 0x5a54);
    r.ecx = r.u32(r.ecx + 0x1c);
    if (r.u32(r.ecx + 0x34) == 1) return r.u32(r.eax + 0x20c8);
    return 0;
}
REFLEX_PORT_FN(0x0067ec50, Fn_0067ec50, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    Regs r(m);
    r.eax = active_table(r);
    r.eax = (r.eax != 0 && r.u32(r.eax + 0x20018) != 0) ? r.edx : 0;
    return r.ret(0);
}
REFLEX_PORT_FN(0x0067eca0, Fn_0067eca0, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    Regs r(m);
    r.eax = active_table(r);
    r.eax = (r.eax != 0 && r.u32(r.eax + 0x20018) == 0) ? r.edx : 0;
    return r.ret(0);
}

// 00679420 (EAX = slot 0..7): the slot record (+0x220, 0x310 bytes) of the crowd table 0xa94234 / 0xa94238 in use, else 0.
REFLEX_PORT_FN(0x00679420, Fn_00679420, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    Regs r(m);
    const uint32_t sp = r.esp - 4;
    r.esi = r.eax;
    r.call(sp, 0x0067ec50);
    if (r.eax != 0) r.eax = r.u32(0xa94234);
    else {
        r.call(sp, 0x0067eca0);
        if (r.eax == 0) return r.ret(0);
        r.eax = r.u32(0xa94238);
    }
    if (r.eax == 0 || r.esi >= 8) { r.eax = 0; return r.ret(0); }
    r.eax = r.esi * 0x310 + r.eax + 0x220;
    return r.ret(0);
}

// 0067abc0 (EAX = slot, EDX = crowd table): the slot's level (+0x1dc), refreshed from its event (+0x1e0, 0x0080b160) when not
// positive (reset to 0x931f2c first). xmm0 = the level.
REFLEX_PORT_FN(0x0067abc0, Fn_0067abc0, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Regs r(m);
    X* x = r.x;
    const uint32_t E = r.esp;
    x[0] = X{};
    r.ecx = r.eax * 0x310;
    r.edi = r.ecx + r.edx;
    if (c_jb(x[0].f(0), r.ss(r.ecx + r.edx + 0x1dc).f(0))) { x[0] = r.ss(r.edi + 0x1dc); return r.ret(0); }
    x[1] = r.ss(0x931f2c);
    r.w32(r.edi + 0x1dc, x[1].u[0]);
    if (r.eax >= 8) { x[0] = r.ss(r.edi + 0x1dc); return r.ret(0); }
    r.esi = r.edi + 0x1e0;
    if (r.esi != 0) {
        r.eax = E - 8;
        r.ecx = E - 4;
        r.w32(E - 8, x[0].u[0]);
        r.call(E - 16, 0x0080b160, {r.ecx, r.eax});
        x[0] = r.ss(E - 8);
        if (c_ja(x[0].f(0), r.ss(0x92e12c).f(0))) r.w32(r.edi + 0x1dc, x[0].u[0]);
    }
    x[0] = r.ss(r.edi + 0x1dc);
    return r.ret(0);
}

// 00679540 (ESI = slot): the slot's level from the crowd table in use (tail call of 0x0067abc0), else 0 (xmm0; ECX = 0 from the
// popped local).
REFLEX_PORT_FN(0x00679540, Fn_00679540, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Regs r(m);
    const uint32_t E = r.esp, L = E - 4;
    r.x[0] = X{};
    r.w32(L, 0);
    r.call(L, 0x0067ec50);
    uint32_t t = 0;
    if (r.eax != 0) { r.edx = r.u32(0xa94234); t = r.edx; }
    else {
        r.call(L, 0x0067eca0);
        if (r.eax != 0) { r.edx = r.u32(0xa94238); t = r.edx; }
    }
    if (t != 0) { r.eax = r.esi; r.tailcall(0x0067abc0); return r.ret(0); }
    r.x[0] = r.ss(L);
    r.ecx = r.u32(L);
    return r.ret(0);
}

// 00674440 (cdecl arg = delta): the crowd excitement: target = clamp(current + delta, min 0xc023a8, max 0xc023ac); when it
// changes, starts a ramp (+0xc023c0 target, state 1, t = 0, from = current). Lazily initialised (state 4).
REFLEX_PORT_FN(0x00674440, Fn_00674440, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Regs r(m);
    X* x = r.x;
    const uint32_t E = r.esp;
    if (r.u32(0xc02400) == 0) { x[3] = r.ss(0xc023b0); r.w32(0xc023b8, x[3].u[0]); r.w32(0xc02400, 4); }
    else x[3] = r.ss(0xc023b8);
    x[2] = r.ss(0xc023ac);
    x[0] = x[3]; x[0] = addss(x[0], r.ss(E + 4));
    if (!c_jb(x[0].f(0), x[2].f(0))) x[0] = x[2];
    x[1] = r.ss(0xc023a8);
    if (!c_jb(x[1].f(0), x[0].f(0))) x[0] = x[1];
    r.eax = lahf_ucomiss(r.eax, x[0].f(0), x[3].f(0));
    if (x[0].f(0) == x[3].f(0)) return r.ret(0);
    if (c_ja(x[1].f(0), x[0].f(0))) r.w32(0xc023c0, x[1].u[0]);
    else if (c_ja(x[0].f(0), x[2].f(0))) r.w32(0xc023c0, x[2].u[0]);
    else r.w32(0xc023c0, x[0].u[0]);
    x[0] = X{};
    r.w32(0xc02400, 1);
    r.w32(0xc023cc, x[0].u[0]);
    r.w32(0xc023c4, x[3].u[0]);
    return r.ret(0);
}

// 006745d0 (EAX = kind 1..3): plays the crowd reaction event (+0x234 / +0x3d8 / +0x57c) of every crowd emitter (0xc02470 list).
REFLEX_PORT_FN(0x006745d0, Fn_006745d0, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    Regs r(m);
    const uint32_t sp = r.esp - 12, ecx0 = r.ecx;
    auto done = [&] { r.ecx = ecx0; return r.ret(0); };
    uint32_t off;
    r.eax -= 1;
    if (r.eax == 0) off = 0x234;
    else { r.eax -= 1; if (r.eax == 0) off = 0x3d8; else { r.eax -= 1; if (r.eax != 0) return done(); off = 0x57c; } }
    r.edi = 0;
    if (int32_t(r.u32(0xc02474)) <= 0) return done();
    do {
        uint32_t list = r.u32(0xc02470);
        if (off == 0x234) r.edx = list; else if (off == 0x3d8) r.ecx = list; else r.eax = list;
        r.esi = r.u32(list + r.edi * 4) + off;
        r.call(sp, 0x0080abd0, {0, 0, 0});
        r.edi++;
    } while (int32_t(r.edi) < int32_t(r.u32(0xc02474)));
    return done();
}

// 00674540 (ECX = event kind): adds the kind's excitement (0x9c47dc) and, with the kind's probability (0x9c4804, rand / 32767),
// tail-calls the reaction (0x006745d0): 3 for kinds 0 / 1, else 2, 1 when the excitement was negative.
REFLEX_PORT_FN(0x00674540, Fn_00674540, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Regs r(m);
    X* x = r.x;
    const uint32_t E = r.esp;
    const uint32_t v = fcopy32(m, r.u32(r.ecx * 4 + 0x9c47dc));
    r.w32(E - 4, v);
    r.call(E - 4, 0x00674440, {fcopy32(m, v)});
    r.eax = r.u32(0x9cb860);
    r.w32(0x9cb864, r.u32(0x9cb864) + 1);
    r.eax = r.eax * 0x41c64e6du;
    x[1] = r.ss(r.ecx * 4 + 0x9c4804);
    r.eax += 0x3039;
    r.w32(0x9cb860, r.eax);
    r.eax = (r.eax >> 16) & 0x7fff;
    x[0] = cvtsi2ss(x[0], int32_t(r.eax));
    double d = double(x[0].f(0));
    uint64_t db; std::memcpy(&db, &d, 8);
    uint64_t kb = uint64_t(r.u32(0x9776a0)) | uint64_t(r.u32(0x9776a4)) << 32;
    double k; std::memcpy(&k, &kb, 8);
    d = d * k;
    std::memcpy(&db, &d, 8);
    x[0].u[1] = uint32_t(db >> 32);
    x[0].setf(0, float(d));
    if (!c_ja(x[1].f(0), x[0].f(0))) { r.ecx = r.u32(E - 4); return r.ret(0); }
    r.eax = 2;
    if (int32_t(r.ecx) >= 0 && int32_t(r.ecx) <= 1) r.eax = 3;
    x[0] = X{};
    if (c_ja(x[0].f(0), r.ss(E - 4).f(0))) r.eax = 1;
    r.tailcall(0x006745d0);
    return r.ret(0);
}

// 006b0480 (ESI = object with 128-byte entries (+0x64, count +0x60)): every flagged entry (+8) restarts its fade: +0x70 = k,
// +0x40 = k - +0x40 (0 when it reached k); flags cleared.
REFLEX_PORT_FN(0x006b0480, Fn_006b0480, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Regs r(m);
    X* x = r.x;
    r.ecx = 0;
    if (r.u32(r.esi + 0x60) <= r.ecx) return r.ret(0);
    x[2] = r.ss(0x92ff84);
    x[3] = X{};
    r.edx = 0;
    do {
        r.eax = r.u32(r.esi + 0x64) + r.edx;
        if (r.u32(r.eax + 8) != 0) {
            x[1] = r.ss(r.eax + 0x40);
            const bool below = c_jb(x[1].f(0), x[2].f(0));
            r.w32(r.eax + 0x70, x[2].u[0]);
            if (!below) x[0] = x[3];
            else { x[0] = x[2]; x[0] = subss(x[0], x[1]); }
            r.w32(r.eax + 0x40, x[0].u[0]);
        }
        r.ecx++;
        r.w32(r.eax + 8, 0);
        r.edx += 0x80;
    } while (r.ecx < r.u32(r.esi + 0x60));
    return r.ret(0);
}

// 006b7560 (thiscall, arg = out 48 bytes): the object's three vectors +0x450 / +0x460 / +0x490.
REFLEX_PORT_FN(0x006b7560, Fn_006b7560, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Regs r(m);
    r.eax = m.arg(0);
    r.st(r.eax, r.ld(r.ecx + 0x450));
    r.st(r.eax + 0x10, r.ld(r.ecx + 0x460));
    r.x[0] = r.ld(r.ecx + 0x490);
    r.st(r.eax + 0x20, r.x[0]);
    return r.ret(4);
}

// 006def20 (EAX = object): its timer in seconds: ticks of 0x0088bd90 on the clock of 0x00892db0 (when +0x1c4c and mode != 1) or
// 0x00892170, times 0x97743c (xmm0).
REFLEX_PORT_FN(0x006def20, Fn_006def20, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Regs r(m);
    const uint32_t E = r.esp, F = ((E - 4) & ~7u) - 8;
    r.eax = r.u32(r.eax + 0x10);
    if (r.u32(r.eax + 0x1c4c) != 0 && r.u32(r.eax + 0x1b0) != 1) {
        r.ecx = F;
        r.call(F, 0x00892db0, {r.ecx});
        r.ecx = r.eax;
    } else {
        r.edx = F;
        r.call(F, 0x00892170, {r.edx, 0});
        r.ecx = F;
    }
    r.call(F, 0x0088bd90);
    r.x[0] = cvtsi2ss(r.x[0], int32_t(r.eax));
    r.x[0] = mulss(r.x[0], r.ss(0x97743c));
    r.w32(F + 4, r.edx);
    return r.ret(0);
}

// 00702ad0 (ESI = map, stdcall 8: out iterator, key*): find in a string-keyed map (lower_bound 0x008519a0, compare 0x009161bc).
REFLEX_PORT_FN(0x00702ad0, StringMap_Find, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    Regs r(m);
    const uint32_t E = r.esp, P = E - 0x24, out = m.arg(0);
    r.ebp = m.arg(1);
    r.eax = r.esi;
    r.call(P, 0x008519a0, {r.ebp});
    r.ebx = r.eax;
    const uint32_t node = r.ebx;
    if (r.esi == 0) r.icall(P, 0x916224);
    r.eax = r.u32(r.esi);
    r.edi = r.u32(r.esi + 0x18);
    const uint32_t cont = r.eax;
    if (r.eax == 0) r.icall(P, 0x916224);
    bool found = false;
    if (r.ebx != r.edi) {
        r.ebx += 0xc;
        r.icall(P, 0x9161bc, {r.ebp, r.ebx});
        found = (r.eax & 0xff) == 0;
    }
    uint32_t i0, i1;
    if (found) { i0 = cont; i1 = node; }
    else { i1 = r.u32(r.esi + 0x18); i0 = r.u32(r.esi); r.eax = i1; r.ecx = i0; }
    r.edx = i0;
    r.ecx = out;
    r.eax = i1;
    r.w32(out + 4, i1); r.w32(out, i0);
    r.eax = out;
    return r.ret(8);
}

// 006b6d80 (EAX = animation blender): picks the keyframes around the current time (+0xdb4) among +0xda8 tracks (112-byte
// keys at [[obj]+0x18], 216-byte states from +0xd8) and starts the blend (0x00840440 single, 0x00840b70 between two).
REFLEX_PORT_FN(0x006b6d80, Fn_006b6d80, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Regs r(m);
    X* x = r.x;
    const uint32_t E = r.esp, P = E - 0x1c;
    r.ecx = r.u32(r.eax + 0xda8);
    x[0] = X{};
    r.esi = 0xffffffffu;
    r.edi = r.esi;
    r.edx = 0;
    r.w32(P + 0x10, x[0].u[0]);
    if (int32_t(r.ecx) > 0) {
        x[2] = r.ss(r.eax + 0xdb4);
        r.ebp = r.ecx;
        r.ecx = r.u32(r.u32(r.eax) + 0x18);
        r.ebx = r.eax + 0xd8;
        r.ecx += 0x50;
        do {
            x[0] = r.ss(r.ecx - 4);
            x[0] = addss(x[0], r.ss(r.ecx - 0x10));
            if (!c_jb(x[2].f(0), x[0].f(0))) {
                x[0] = r.ss(r.ebx);
                x[1] = x[0]; x[1] = subss(x[1], r.ss(r.ecx));
                if (!c_jb(x[1].f(0), x[2].f(0)) && r.u32(r.ecx + 0x14) == 0 && r.u32(r.ecx + 0x18) == 0) {
                    x[1] = r.ss(r.ecx - 0x10);
                    x[3] = x[2]; x[3] = subss(x[3], x[1]);
                    x[0] = subss(x[0], x[1]);
                    x[3] = divss(x[3], x[0]);
                    r.esi = r.edx;
                    r.w32(P + 0x10, x[3].u[0]);
                }
            }
            r.edx++; r.ecx += 0x70; r.ebx += 0xd8;
        } while (int32_t(r.edx) < int32_t(r.ebp));
        if (r.esi != 0xffffffffu) {
            const uint32_t f = fcopy32(m, r.u32(P + 0x10));
            r.esi = r.esi * 0xd8;
            r.edx = r.eax + 0x440;
            r.eax = r.esi + r.eax + 4;
            r.call(P, 0x00840440, {r.eax, f, r.edx});
            return r.ret(0);
        }
    }
    x[4] = r.ss(0x977550);
    x[3] = r.ss(0x977494);
    r.edx = 0;
    if (int32_t(r.u32(r.eax + 0xda8)) <= 0) return r.ret(0);
    r.ecx = r.u32(r.u32(r.eax) + 0x18);
    r.ebp = r.u32(r.eax + 0xda8);
    r.w32(P + 0x10, r.ecx);
    r.ebx = r.eax + 0xd8;
    r.ecx += 0x68;
    do {
        if (r.u32(r.ecx - 4) == 0 && r.u32(r.ecx) == 0) {
            x[0] = r.ss(r.ebx);
            x[0] = subss(x[0], r.ss(r.ecx - 0x18));
            x[2] = r.ss(r.eax + 0xdb4);
            const bool before = c_jb(x[2].f(0), x[0].f(0));
            x[1] = r.ss(r.ecx - 0x28);
            x[1] = addss(x[1], r.ss(r.ecx - 0x1c));
            if (!before && !c_jb(x[0].f(0), x[4].f(0))) { x[4] = x[0]; r.esi = r.edx; }
            if (!c_jb(x[1].f(0), x[2].f(0)) && !c_jb(x[3].f(0), x[1].f(0))) { x[3] = x[1]; r.edi = r.edx; }
        }
        r.edx++; r.ecx += 0x70; r.ebx += 0xd8;
    } while (int32_t(r.edx) < int32_t(r.ebp));
    if (r.esi == 0xffffffffu) {
        if (r.edi == r.esi) return r.ret(0);
        r.ecx = r.edi;
        r.edi = r.edi * 0xd8;
        x[0] = r.ss(r.edi + r.eax + 0xd8);
        r.ecx = r.ecx * 0x70;
        r.ecx += r.u32(P + 0x10);
        r.edx = r.edi + r.eax;
        x[0] = subss(x[0], r.ss(r.ecx + 0x40));
        x[1] = r.ss(r.ecx + 0x4c);
        r.eax += 0x440;
        x[1] = divss(x[1], x[0]);
        r.edx += 4;
        r.call(P, 0x00840440, {r.edx, x[1].u[0], r.eax});
        return r.ret(0);
    }
    if (r.edi == 0xffffffffu) {
        r.ecx = r.esi;
        r.esi = r.esi * 0xd8;
        x[0] = r.ss(r.esi + r.eax + 0xd8);
        r.ecx = r.ecx * 0x70;
        r.ecx += r.u32(P + 0x10);
        r.edx = r.esi + r.eax;
        x[0] = subss(x[0], r.ss(r.ecx + 0x40));
        r.eax += 0x440;
        x[1] = x[0]; x[1] = subss(x[1], r.ss(r.ecx + 0x50));
        x[1] = divss(x[1], x[0]);
        r.edx += 4;
        r.call(P, 0x00840440, {r.edx, x[1].u[0], r.eax});
        return r.ret(0);
    }
    r.ecx = r.u32(r.eax);
    r.ebx = r.u32(r.ecx + 0x18);
    r.ecx = r.esi;
    r.esi = r.esi * 0xd8;
    x[1] = r.ss(r.esi + r.eax + 0xd8);
    r.ecx = r.ecx * 0x70;
    x[2] = r.ss(r.ecx + r.ebx + 0x40);
    r.edx = r.edi;
    r.edi = r.edi * 0xd8;
    r.edx = r.edx * 0x70;
    r.esi += r.eax;
    r.ecx += r.ebx;
    x[0] = x[1]; x[0] = subss(x[0], r.ss(r.ecx + 0x50));
    x[1] = subss(x[1], x[2]);
    x[3] = x[0]; x[3] = subss(x[3], x[2]);
    x[2] = r.ss(r.edi + r.eax + 0xd8);
    x[3] = divss(x[3], x[1]);
    x[1] = r.ss(r.edx + r.ebx + 0x4c);
    r.ecx = r.edi + r.eax;
    r.edx += r.ebx;
    r.w32(P + 0x18, x[3].u[0]);
    x[3] = r.ss(r.edx + 0x40);
    x[2] = subss(x[2], x[3]);
    x[4] = x[1]; x[4] = divss(x[4], x[2]);
    x[2] = r.ss(r.eax + 0xdb4);
    x[1] = addss(x[1], x[3]);
    x[2] = subss(x[2], x[0]);
    x[1] = subss(x[1], x[0]);
    x[2] = divss(x[2], x[1]);
    r.w32(r.eax + 0xdb8, x[2].u[0]);
    r.eax += 0x440;
    r.w32(P + 0x10, x[2].u[0]);
    r.w32(P + 0x14, x[4].u[0]);
    const uint32_t a2 = fcopy32(m, r.u32(P + 0x10)), a1 = fcopy32(m, r.u32(P + 0x14)), a0 = fcopy32(m, r.u32(P + 0x18));
    r.ecx += 4;
    r.esi += 4;
    r.call(P, 0x00840b70, {r.esi, r.ecx, a0, a1, a2, r.eax});
    return r.ret(0);
}

// 006aa460 (stdcall 8: clip entry (76 bytes), rider; EDI = bike): records the bike, its pose (0x0047bb40 / 0x0047bd80 /
// 0x0047be30 for bike classes 3 / 5 / 7), its race time (0xa96dc0 table) and, in mode 5, whether the rider has a lap (+0x1120).
REFLEX_PORT_FN(0x006aa460, ClipRecorder_FillEntry, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Regs r(m);
    const uint32_t E = r.esp, sp = E - 12;
    r.ebx = m.arg(0);
    r.ebp = m.arg(1);
    r.w32(r.ebx, r.edi);
    r.w32(r.ebx + 0x48, 0);
    if (r.edi == 0) return r.ret(8);
    r.call(sp, 0x0047bb40);
    r.eax = r.u32(r.edi + 0x194);
    if (r.eax == 3 || r.eax == 5 || r.eax == 7) {
        r.eax = 0;
        r.esi = r.ebx + 8;
        r.call(sp, 0x0047bb40);
        r.edx = r.eax; r.eax = r.edi;
        r.call(sp, 0x0047bd80);
        r.esi = r.ebx + 0x28;
        r.call(sp, 0x0047bb40);
        r.edx = r.eax; r.eax = r.edi;
        r.call(sp, 0x0047be30);
    } else r.eax = r.eax - 7;
    r.x[0] = X{};
    r.ecx = 0;
    r.eax = 0xa96dc0;
    for (;;) {
        if (r.edi == r.u32(r.eax)) { r.ecx <<= 6; r.x[0] = r.ss(r.ecx + 0xa96d90); break; }
        r.eax += 0x40; r.ecx++;
        if (!(r.eax < 0xa970c0)) break;
    }
    r.w32(r.ebx + 4, r.x[0].u[0]);
    r.eax = r.u32(r.ebp + 0x74);
    r.ecx = r.u32(r.eax + 0x94);
    if (r.u32(r.ecx + 0x238) == 5) {
        r.edx = int32_t(r.u32(r.ebp + 0x1120)) >= 0 ? 1 : 0;
        r.w32(r.ebx + 0x48, r.edx);
    }
    return r.ret(8);
}

// 006abbc0 (cdecl arg = 12 clip entries): fills one entry per rider (0x006aa460), clears the rest.
REFLEX_PORT_FN(0x006abbc0, ClipRecorder_FillEntries, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Regs r(m);
    const uint32_t E = r.esp, P = (((E - 4) & ~7u) - 4) - 12;
    const uint32_t buf = m.arg(0);
    r.eax = r.u32(0xa942b8);
    r.esi = 0;
    if (int32_t(r.u32(r.eax + 0x26c)) > 0) {
        r.ecx = buf;
        r.w32(P + 0xc, r.ecx);
        r.ebx = 0x2d0;
        do {
            r.eax = r.u32(r.ebx + r.eax);
            r.edx = r.u32(r.eax + 0x74);
            r.edi = r.u32(r.edx + 0x1ac);
            const uint32_t rider = r.eax;
            r.eax = r.u32(P + 0xc);
            r.call(P, 0x006aa460, {r.eax, rider});
            r.eax = r.u32(0xa942b8);
            r.w32(P + 0xc, r.u32(P + 0xc) + 0x4c);
            r.esi++;
            r.ebx += 4;
        } while (int32_t(r.esi) < int32_t(r.u32(r.eax + 0x26c)));
        if (r.esi >= 0xc) return r.ret(0);
    }
    r.eax = r.esi * 0x4c + buf;
    r.ecx = 0xc - r.esi;
    r.edx = 0;
    do { r.w32(r.eax, 0); r.w32(r.eax + 0x48, 0); r.eax += 0x4c; r.ecx--; } while (r.ecx != 0);
    return r.ret(0);
}

// 006aacf0 (ECX = clip type, EDX = rider, cdecl: start, end): the clip candidate: takes it when none, extends the current one
// when they overlap closely (0x977a84) and the type is not 9, or replaces it with a higher type; snapshots the field
// (0x006abbc0 into 0xa959b0) for a new one.
REFLEX_PORT_FN(0x006aacf0, ClipRecorder_Candidate, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Regs r(m);
    X* x = r.x;
    const uint32_t E = r.esp;
    if (r.u32(0xd5a9a4) == 0 || r.u32(r.edx + 0x74) == 0) return r.ret(0);
    r.eax = r.u32(0xa959a8);
    if (r.eax == 0) {
        x[0] = r.ss(E + 4); r.w32(0xa959a0, x[0].u[0]);
        x[0] = r.ss(E + 8); r.w32(0xa959a4, x[0].u[0]);
        r.w32(0xa959a8, r.ecx); r.w32(0xa959ac, r.edx);
        r.call(E, 0x006abbc0, {0xa959b0});
        return r.ret(0);
    }
    x[0] = r.ss(E + 8); x[2] = r.ss(0xa959a4); x[1] = r.ss(E + 4);
    x[2] = subss(x[2], r.ss(0xa959a0));
    x[3] = x[0]; x[3] = subss(x[3], x[1]);
    x[2] = addss(x[2], x[3]);
    x[3] = r.ss(0x977a84);
    if (c_ja(x[3].f(0), x[2].f(0)) && r.eax != 9) {
        r.w32(0xa959a4, x[0].u[0]);
        if (int32_t(r.eax) > int32_t(r.ecx)) return r.ret(0);
        r.w32(0xa959a8, r.ecx);
        return r.ret(0);
    }
    if (int32_t(r.ecx) <= int32_t(r.eax)) return r.ret(0);
    r.w32(0xa959a0, x[1].u[0]); r.w32(0xa959a4, x[0].u[0]);
    r.w32(0xa959a8, r.ecx); r.w32(0xa959ac, r.edx);
    r.call(E, 0x006abbc0, {0xa959b0});
    r.ecx = 0xa959b0;
    return r.ret(0);
}

// 006aaca0 (ECX = clip type, EDX = rider): a candidate around the current race time with the type's lead / trail (0x9c40a8).
REFLEX_PORT_FN(0x006aaca0, ClipRecorder_CandidateOfType, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Regs r(m);
    X* x = r.x;
    const uint32_t E = r.esp, ecx0 = r.ecx;
    if (r.u32(0xd5a9a4) != 0) {
        r.eax = r.u32(0xa942b8);
        x[0] = r.ss(r.eax + 0x294);
        x[1] = r.ss(r.ecx * 8 + 0x9c40ac);
        x[1] = addss(x[1], x[0]);
        x[0] = subss(x[0], r.ss(r.ecx * 8 + 0x9c40a8));
        r.call(E - 4, 0x006aacf0, {x[0].u[0], x[1].u[0]});
    }
    r.ecx = ecx0;
    return r.ret(0);
}

// 006ab8a0 (EAX = rider, cdecl arg = value): logs a wreck clip event (type 4) into the 100-entry ring 0xd2f160 (128 bytes each)
// and, for an active non-replay rider, a wreck candidate (type 6, tail call of 0x006aaca0).
REFLEX_PORT_FN(0x006ab8a0, ClipRecorder_LogWreck, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Regs r(m);
    const uint32_t E = r.esp;
    r.esi = r.eax;
    if (r.u32(r.esi + 0x908) != 0 || r.u32(0xa942b4) != 1) return r.ret(0);
    r.edi = r.u32(0xa942b8);
    r.ecx = r.edi;
    r.call(E - 8, 0x0055a050);
    r.ecx = r.u32(r.u32(r.edi + 0x268) + r.eax * 4);
    r.edx = r.u32(0xa942c8);
    r.eax = r.edx << 7;
    r.w32(r.eax + 0xd2f164, 4);
    const uint32_t t = fcopy32(m, r.u32(r.edi + 0x294));
    r.edi = r.u32(E + 4);
    r.w32(r.eax + 0xd2f160, t);
    r.w32(r.eax + 0xd2f168, r.ecx);
    r.w32(r.eax + 0xd2f1c0, r.edi);
    {
        const int32_t n = int32_t(r.edx + 1);
        r.edi = 0x64;
        r.eax = uint32_t(n / 100);
        r.edx = uint32_t(n % 100);
    }
    r.w32(0xa942c8, r.edx);
    if (r.u32(r.esi + 4) != 0 && r.u32(r.esi + 0x10) == 0) {
        r.edx = r.ecx;
        r.ecx = r.edi - 0x5e;
        r.edi = r.saved[3]; r.esi = r.saved[2];
        r.tailcall(0x006aaca0);
    }
    return r.ret(0);
}

// 006ab930 (EAX = rider, cdecl arg = value): logs a stunt clip event (type 6) into the ring (the replay rider's id from +0x27c).
REFLEX_PORT_FN(0x006ab930, ClipRecorder_LogStunt, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Regs r(m);
    const uint32_t E = r.esp;
    r.esi = r.eax;
    if (r.u32(0xa942b4) != 1) return r.ret(0);
    r.edi = r.u32(0xa942b8);
    r.ecx = r.edi;
    r.call(E - 8, 0x0055a050);
    if (r.u32(r.esi + 0x908) != 0) r.ecx = r.u32(r.u32(r.edi + 0x27c) + r.eax * 4);
    else { r.edx = r.u32(r.edi + 0x268); r.ecx = r.u32(r.edx + r.eax * 4); }
    r.edx = r.u32(0xa942c8);
    r.eax = r.edx << 7;
    r.w32(r.eax + 0xd2f164, 6);
    r.w32(r.eax + 0xd2f160, fcopy32(m, r.u32(r.edi + 0x294)));
    r.w32(r.eax + 0xd2f168, r.ecx);
    r.ecx = r.u32(E + 4);
    r.w32(r.eax + 0xd2f1c0, r.ecx);
    const int32_t n = int32_t(r.edx + 1);
    r.ecx = 0x64;
    r.eax = uint32_t(n / 100);
    r.edx = uint32_t(n % 100);
    r.w32(0xa942c8, r.edx);
    return r.ret(0);
}
