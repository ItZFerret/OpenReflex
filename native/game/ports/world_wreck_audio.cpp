// World group, wreck share: the AudioCollisionManager (0x0067axxx-0x0067dxxx, its std::map of collision sounds 0x0068exxx /
// 0x0068fxxx, the per-rider sound slots 0x006817c0 / 0x00681840 / 0x0068bfb0 / 0x0068c360). Written in the register-file style
// (Regs: m.r in place) because nearly every callee takes inputs in registers the original sets long before the call.
#include "world_wreck_util.h"

using namespace world;
using namespace wreck;
using reflex_port::OUT_EAX;
using reflex_port::OUT_ECX;
using reflex_port::OUT_EDX;
using reflex_port::OUT_XMM_ALL;

namespace {

constexpr uint32_t ENTER_CS = 0x91600c, LEAVE_CS = 0x9160f8, TIME = 0x91601c, THREAD_ID = 0x91607c, ABORT = 0x916224;

// the inline strlen loop (lea edx, [eax + 1]; mov cl, [eax]; inc eax; test cl, cl; jne; sub eax, edx): EAX = length, EDX = p + 1,
// CL = 0.
void strlen_regs(Regs& r, uint32_t p, uint32_t& er) {
    uint32_t e = p;
    uint8_t c;
    do { c = r.u8(e); e++; } while (c);
    r.ecx = (r.ecx & 0xffffff00u);
    er = e - (p + 1);
}
// the inline strcmp loop (byte pairs, DL): EAX = 0 / -1 / 1, ECX and the pointer advanced, DL = the last byte.
uint32_t strcmp_regs(Regs& r, uint32_t a, uint32_t b) {
    uint32_t pa = a, pb = b;
    for (;;) {
        uint8_t d = r.u8(pa);
        setlo8(r.edx, d);
        if (d != r.u8(pb)) { r.eax = pa; r.ecx = pb; return d < r.u8(pb) ? 0xffffffffu : 1u; }
        if (d == 0) break;
        d = r.u8(pa + 1);
        setlo8(r.edx, d);
        if (d != r.u8(pb + 1)) { r.eax = pa; r.ecx = pb; return d < r.u8(pb + 1) ? 0xffffffffu : 1u; }
        pa += 2; pb += 2;
        if (d == 0) break;
    }
    r.eax = pa; r.ecx = pb;
    return 0;
}

}  // namespace

// 0067ac80 (ESI = a, EDI = b collision keys {obj0, obj1, ...}): a < b: by obj0, then obj1; for two contacts of the same object of
// type 0x100f, by the byte +0x20. AL / EAX = the result.
REFLEX_PORT_FN(0x0067ac80, CollisionKey_Less, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    Regs r(m);
    const uint32_t E = r.esp;
    r.eax = r.u32(r.esi + 4);
    if (r.eax != 0 && r.eax == r.u32(r.edi + 4)) {
        r.ecx = r.eax;
        r.vcall(E, r.ecx, 0x58, r.eax, r.edx);
        if (r.eax == 0x100f) {
            r.eax = r.u8(r.esi + 0x20);
            const uint8_t cl = r.u8(r.edi + 0x20);
            setlo8(r.ecx, cl);
            if (uint8_t(r.eax) != cl) { r.eax = uint8_t(r.eax) < cl ? 1u : 0u; return r.ret(0); }
        }
    }
    r.eax = r.u32(r.esi); r.ecx = r.u32(r.edi);
    if (r.ecx > r.eax) { r.eax = 1; return r.ret(0); }
    if (r.ecx < r.eax) { r.eax = 0; return r.ret(0); }
    r.eax = r.u32(r.esi + 4);
    r.eax = r.eax < r.u32(r.edi + 4) ? 1u : 0u;
    return r.ret(0);
}

// 0068faf0 (EAX = map, stack = key*; ret 4): lower_bound (the head when none); the candidate lives in the argument slot.
REFLEX_PORT_FN(0x0068faf0, CollisionMap_LowerBound, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    Regs r(m);
    const uint32_t E = r.esp, slot = E + 4;
    r.eax = r.u32(r.eax + 0x18);
    r.ebx = r.u32(r.eax + 4);
    r.ebp = r.u32(slot);
    r.w32(slot, r.eax);
    if (r.u8(r.ebx + 0x11) != 0) return r.ret(4);
    const uint32_t sp = E - 16;
    do {
        r.esi = r.u32(r.ebx + 0xc);
        r.eax = r.u32(r.esi + 4);
        r.edi = r.u32(r.ebp);
        bool less_set = false, less = false;
        if (r.eax != 0 && r.eax == r.u32(r.edi + 4)) {
            r.ecx = r.eax;
            r.vcall(sp, r.ecx, 0x58, r.edx, r.eax);
            if (r.eax == 0x100f) {
                const uint8_t al = r.u8(r.esi + 0x20), cl = r.u8(r.edi + 0x20);
                setlo8(r.eax, al); setlo8(r.ecx, cl);
                if (al != cl) { less = al < cl; setlo8(r.eax, less ? 1 : 0); less_set = true; }
            }
        }
        if (!less_set) {
            r.eax = r.u32(r.esi); r.ecx = r.u32(r.edi);
            if (r.ecx > r.eax) less = true;
            else if (r.ecx < r.eax) less = false;
            else { r.ecx = r.u32(r.esi + 4); less = r.ecx < r.u32(r.edi + 4); }
        }
        if (less) r.ebx = r.u32(r.ebx + 8);
        else { r.w32(slot, r.ebx); r.ebx = r.u32(r.ebx); }
    } while (r.u8(r.ebx + 0x11) == 0);
    r.eax = r.u32(slot);
    return r.ret(4);
}

// 0068fc80 (ESI = iterator {container, node}): ++iterator (in-order successor), checked.
REFLEX_PORT_FN(0x0068fc80, CollisionMap_IterNext, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    Regs r(m);
    const uint32_t E = r.esp, sp = E - 4;
    if (r.u32(r.esi) == 0) r.icall(sp, ABORT);
    r.eax = r.u32(r.esi + 4);
    if (r.u8(r.eax + 0x11) != 0) { r.icall(sp, ABORT); return r.ret(0); }
    r.ecx = r.u32(r.eax + 8);
    if (r.u8(r.ecx + 0x11) == 0) {
        r.eax = r.u32(r.ecx);
        while (r.u8(r.eax + 0x11) == 0) { r.ecx = r.eax; r.eax = r.u32(r.ecx); }
        r.w32(r.esi + 4, r.ecx);
        return r.ret(0);
    }
    r.eax = r.u32(r.eax + 4);
    while (r.u8(r.eax + 0x11) == 0) {
        r.ecx = r.u32(r.esi + 4);
        if (r.ecx != r.u32(r.eax + 8)) break;
        r.w32(r.esi + 4, r.eax);
        r.edx = r.eax;
        r.eax = r.u32(r.edx + 4);
    }
    r.w32(r.esi + 4, r.eax);
    return r.ret(0);
}

// 0068e510 (stdcall 12: map, out iterator, key*): find (end when absent).
REFLEX_PORT_FN(0x0068e510, CollisionMap_Find, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    Regs r(m);
    const uint32_t E = r.esp, P = E - 0x20, out = m.arg(1);
    r.ebx = m.arg(0); r.ebp = m.arg(2);
    r.eax = r.ebx;
    r.call(P, 0x0068faf0, {r.ebp});
    r.edi = r.eax;
    const uint32_t node = r.edi;
    if (r.ebx == 0) r.icall(P, ABORT);
    r.eax = r.u32(r.ebx);
    r.esi = r.u32(r.ebx + 0x18);
    const uint32_t cont = r.eax;
    if (r.eax == 0) r.icall(P, ABORT);
    uint32_t it0, it1;
    bool found = false;
    if (r.edi != r.esi) {
        r.edi = r.u32(r.edi + 0xc);
        r.esi = r.u32(r.ebp);
        r.call(P, 0x0067ac80);
        if ((r.eax & 0xff) == 0) found = true;
    }
    if (found) { it0 = cont; it1 = node; }
    else { it1 = r.u32(r.ebx + 0x18); it0 = r.u32(r.ebx); r.eax = it1; r.ecx = it0; }
    r.edx = it0;
    r.ecx = out;
    r.eax = it1;
    r.w32(out + 4, it1); r.w32(out, it0);
    r.eax = out;
    return r.ret(0xc);
}

// 0068e130 (stdcall 12: map, out {iterator, inserted}, key*): insert-or-find of a new record (0x005860e0 inserts at a hint,
// 0x00586c10 decrements an iterator). The byte "go left" flag lives at [P+0x10] (its dword goes to 0x005860e0 as is).
REFLEX_PORT_FN(0x0068e130, CollisionMap_Insert, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    Regs r(m);
    const uint32_t E = r.esp, P = (((E - 4) & ~7u) - 0x14) - 12;
    const uint32_t map = m.arg(0), out = m.arg(1), key = m.arg(2);
    r.ecx = map;
    r.eax = r.u32(r.ecx + 0x18);
    r.ebx = r.u32(r.eax + 4);
    r.esi = r.eax;
    setlo8(r.eax, 1);
    r.w32(P + 0x14, r.esi);
    r.w8(P + 0x10, 1);
    if (r.u8(r.ebx + 0x11) == 0) {
        do {
            r.eax = key;
            r.esi = r.u32(r.eax);
            r.eax = r.u32(r.esi + 4);
            r.edi = r.u32(r.ebx + 0xc);
            r.w32(P + 0x14, r.ebx);
            bool decided = false, left = false;
            if (r.eax != 0 && r.eax == r.u32(r.edi + 4)) {
                r.ecx = r.eax;
                r.vcall(P, r.ecx, 0x58, r.edx, r.eax);
                if (r.eax == 0x100f) {
                    const uint8_t al = r.u8(r.esi + 0x20), cl = r.u8(r.edi + 0x20);
                    setlo8(r.eax, al); setlo8(r.ecx, cl);
                    if (al != cl) { left = al < cl; decided = true; }
                }
            }
            if (!decided) {
                r.eax = r.u32(r.esi); r.ecx = r.u32(r.edi);
                if (r.ecx > r.eax) left = true;
                else if (r.ecx < r.eax) left = false;
                else { r.ecx = r.u32(r.esi + 4); left = r.ecx < r.u32(r.edi + 4); }
            }
            setlo8(r.eax, left ? 1 : 0);
            r.w8(P + 0x10, left ? 1 : 0);
            r.ebx = left ? r.u32(r.ebx) : r.u32(r.ebx + 8);
        } while (r.u8(r.ebx + 0x11) == 0);
        r.esi = r.u32(P + 0x14);
        r.ecx = map;
    }
    r.ebx = r.u32(r.ecx);
    r.w32(P + 0x1c, r.esi);
    r.w32(P + 0x18, r.ebx);
    auto insert_at = [&](uint32_t flag) {   // registers as each call site leaves them
        r.edi = map;
        r.call(P, 0x005860e0, {P + 0x18, flag, key});
        r.edx = r.u32(r.eax);
        r.ecx = out;
        r.eax = r.u32(r.eax + 4);
        r.w32(r.ecx + 4, r.eax); r.w32(r.ecx, r.edx); r.w8(r.ecx + 8, 1);
        r.eax = r.ecx;
        return r.ret(0xc);
    };
    if ((r.eax & 0xff) != 0) {
        r.edx = r.u32(r.ecx + 0x18);
        r.edi = r.u32(r.edx);
        if (r.ebx == 0) r.icall(P, ABORT);
        if (r.esi == r.edi) { r.ecx = P + 0x18; r.eax = r.esi; return insert_at(1); }
        r.esi = P + 0x18;
        r.call(P, 0x00586c10);
        r.ebx = r.u32(P + 0x18);
    }
    r.ecx = key;
    r.edx = r.u32(P + 0x1c);
    r.edi = r.u32(r.ecx);
    r.esi = r.u32(r.edx + 0xc);
    r.call(P, 0x0067ac80);
    if ((r.eax & 0xff) != 0) {
        r.ecx = r.u32(P + 0x10);
        r.eax = r.u32(P + 0x14);
        r.edx = P + 0x18;
        return insert_at(r.ecx);
    }
    r.eax = out;
    r.ecx = r.u32(P + 0x1c);
    r.w32(r.eax, r.ebx); r.w32(r.eax + 4, r.ecx); r.w8(r.eax + 8, 0);
    return r.ret(0xc);
}

// 00681840 (cdecl arg = slot, ESI = rider): stops the rider's collision sound slot (0x0068c360) when the rider has a sound bank
// entry in 0xd92f00 (12 banks of 0x1b10 bytes).
REFLEX_PORT_FN(0x00681840, Fn_00681840, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    Regs r(m);
    const uint32_t E = r.esp;
    r.edx = r.u32(0xd92f00);
    r.eax = 0;
    r.ecx = r.edx + 0x10;
    for (;;) {
        if (r.esi == r.u32(r.ecx)) break;
        r.eax++; r.ecx += 0x1b10;
        if (int32_t(r.eax) >= 12) return r.ret(0);
    }
    r.ecx = r.u32(E + 4);
    r.eax = r.eax * 0x1b10;
    r.edx = r.eax + r.edx + 0xa70;
    r.call(E, 0x0068c360, {r.edx, r.ecx});
    return r.ret(0);
}

// 006817c0 (cdecl arg = slot, ESI = rider): starts the slot: flag +0x758, the sound source of 0x0068bfb0 into +0x10 of the slot
// record (0x1d4 bytes each), slot 5 also sets +0x109c.
REFLEX_PORT_FN(0x006817c0, Fn_006817c0, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    Regs r(m);
    const uint32_t E = r.esp;
    r.edx = r.u32(0xd92f00);
    r.ebx = r.u32(E + 4);
    r.eax = 0;
    r.ecx = r.edx + 0x10;
    for (;;) {
        if (r.esi == r.u32(r.ecx)) break;
        r.eax++; r.ecx += 0x1b10;
        if (int32_t(r.eax) >= 12) return r.ret(0);
    }
    r.eax = r.eax * 0x1b10;
    if (r.u32(r.eax + r.edx + 0xa84) == 0) return r.ret(0);
    r.edi = r.eax + r.edx + 0xa70;
    if (r.u32(r.edi + 0x18) == 0) return r.ret(0);
    r.eax = r.ebx * 0x1d4;
    r.w32(r.eax + r.edi + 0x758, 1);
    r.call(E - 8, 0x0068bfb0);
    r.ecx = (r.ebx + 4) * 0x1d4;
    r.w32(r.ecx + r.edi, r.eax);
    if (r.ebx == 5) r.w32(r.edi + 0x109c, 1);
    return r.ret(0);
}

// 0068bfb0 (EDI = rider sound bank): the position source: the bike's active wheel contact (+0x60 of a 0x1002 object with +0x2f4),
// else the rider's +0x200 (with a player) or +0x1c0.
REFLEX_PORT_FN(0x0068bfb0, Fn_0068bfb0, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    Regs r(m);
    const uint32_t E = r.esp;
    r.eax = r.u32(r.edi + 0x14);
    r.eax = r.u32(r.eax + 0x190);
    if (r.eax != 0) {
        r.ecx = r.u32(r.eax + 0x988);
        r.esi = r.u32(r.eax + r.ecx * 4 + 0x974);
        if (r.esi != 0) {
            r.vcall(E - 4, r.esi, 0x58, r.edx, r.eax);
            if (r.eax == 0x1002 && r.u32(r.esi + 0x2f4) != 0) {
                r.eax = r.esi + 0x60;
                if (r.eax != 0) return r.ret(0);
            }
        }
    }
    r.ecx = r.u32(r.edi + 0x18);
    if (r.u32(r.ecx + 0x1f0) != 0) {
        r.eax = r.ecx + 0x200;
        if (r.eax != 0) return r.ret(0);
    }
    r.eax = r.ecx + 0x1c0;
    return r.ret(0);
}

// 0068c360 (stdcall 8: bank, slot): clears the slot's flags and stops its sound (0x0080ae10 on the audio thread, else the queued
// 0x0080c280); slot 5 also clears +0x109c.
REFLEX_PORT_FN(0x0068c360, Fn_0068c360, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    Regs r(m);
    const uint32_t E = r.esp, sp = E - 12;
    r.ebx = m.arg(0);
    const uint32_t k = m.arg(1);
    if (r.u32(r.ebx + 0x14) == 0 || r.u32(r.ebx + 0x18) == 0) return r.ret(8);
    r.eax = k * 0x1d4 + r.ebx;
    r.w32(r.eax + 0x758, 0); r.w32(r.eax + 0x754, 0);
    r.edi = r.u32(0xd67d1c);
    r.esi = r.eax + 0x5a0;
    r.icall(sp, THREAD_ID);
    if (r.edi == r.eax) r.call(sp, 0x0080ae10);
    else r.call(sp, 0x0080c280, {1, r.esi, 0, 0});
    if (k == 5) r.w32(r.ebx + 0x109c, 0);
    return r.ret(8);
}

// 0067d310 (ECX = sound table, EAX = id): the sound name of an id from the table's map (+0x90, 0x00690e00), else of the default
// id 0x931eac; 0 when neither.
REFLEX_PORT_FN(0x0067d310, AudioTable_Name, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    Regs r(m);
    const uint32_t E = r.esp, P = E - 12;
    r.esi = r.ecx + 0x90;
    r.ecx = P + 4;
    r.w32(P + 8, r.eax);
    r.eax = P + 8;
    r.w32(P + 4, 0);
    r.call(P, 0x00690e00, {r.ecx});
    if (r.eax != 0) { r.edx = r.u32(P + 4); r.eax = r.u32(r.edx); return r.ret(0); }
    r.eax = P + 4;
    const uint32_t a = r.eax;
    r.eax = 0x931eac;
    r.call(P, 0x00690e00, {a});
    if (r.eax != 0) { r.ecx = r.u32(P + 4); r.eax = r.u32(r.ecx); return r.ret(0); }
    r.eax = 0;
    return r.ret(0);
}

// 0067b890 (ESI = manager, EAX = sound id): the sound name from the current track's table (0xa946b8 +0x5a54 / +0x20c8 by the
// active mode, then the manager's +8 or +0xc table: tail call of 0x0067d310); none: sets +0x10 / +0x14 when 0xa9423c, EAX = 0.
REFLEX_PORT_FN(0x0067b890, AudioCollision_SoundName, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    Regs r(m);
    r.ecx = r.u32(0xa946b8);
    r.edi = 1;
    bool have = false;
    if (r.ecx != 0) {
        r.edx = r.u32(0xa95844);
        const uint32_t b = r.u32(r.edx + 0x20);
        if (r.u32(b + 0x34) == 1) { r.ecx = r.u32(r.ecx + 0x5a54); have = true; }
        else {
            r.edx = r.u32(r.edx + 0x1c);
            if (r.u32(r.edx + 0x34) == 1) { r.ecx = r.u32(r.ecx + 0x20c8); have = true; }
        }
    }
    if (!have) r.ecx = 0;
    uint32_t tbl, flag;
    if (r.ecx != 0 && r.u32(r.ecx + 0x20018) != 0) { tbl = 8; flag = 0x10; }
    else { tbl = 0xc; flag = 0x14; }
    r.ecx = r.u32(r.esi + tbl);
    if (r.ecx != 0) {
        r.edi = r.saved[3];
        r.tailcall(0x0067d310);
        return r.ret(0);
    }
    if (r.u32(0xa9423c) != 0) r.w32(r.esi + flag, r.edi);
    r.eax = 0;
    return r.ret(0);
}

// 0067c930 (EAX = collision record): 1 when its playing event (+0x58) has a name and its bank is 0x95b800 ("" or the collision
// bank), else 0.
REFLEX_PORT_FN(0x0067c930, AudioCollision_EventIsOurs, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    Regs r(m);
    const uint32_t ecx0 = r.ecx;
    r.edx = r.u32(r.eax + 0x58);
    uint32_t len;
    strlen_regs(r, r.edx + 0x18, len);
    r.eax = len;
    if (len == 0) { r.eax = 0; r.ecx = ecx0; return r.ret(0); }
    const uint32_t c = strcmp_regs(r, r.u32(r.edx + 0x134), 0x95b800);
    r.eax = c == 0 ? 1u : 0u;
    r.ecx = ecx0;
    return r.ret(0);
}

// 0068e270 (stdcall 16: map, out iterator, container, node): std::map::erase (red-black rebalance: 0x00586660 / 0x005866b0 rotate,
// 0x0080d320 the rightmost below, 0x0078fc50 frees the node). The iterator argument is advanced in place (0x0068fc80) and the
// node is kept in the map argument slot, as the original does. Erasing the head throws: left to the original (never happens).
REFLEX_PORT_FN(0x0068e270, CollisionMap_Erase, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    Regs r(m);
    const uint32_t E = r.esp, P = E - 0x5c;
    if (r.u8(m.arg(3) + 0x11) != 0) return reflex_vm::RUN_ORIGINAL;
    r.eax = m.arg(3);
    r.ebp = m.arg(0);
    r.ebx = r.eax;
    r.w32(E + 4, r.ebx);
    r.esi = E + 0xc;
    r.call(P, 0x0068fc80);
    auto head = [&] { return r.u32(r.ebp + 0x18); };
    r.ecx = r.u32(r.ebx);
    bool two = false;
    if (r.u8(r.ecx + 0x11) != 0) r.edi = r.u32(r.ebx + 8);
    else {
        r.edx = r.u32(r.ebx + 8);
        if (r.u8(r.edx + 0x11) != 0) r.edi = r.ecx;
        else {
            r.eax = r.u32(E + 0x10);
            r.edi = r.u32(r.eax + 8);
            r.edx = r.eax + 8;
            two = r.eax != r.ebx;
        }
    }
    if (!two) {
        r.esi = r.u32(r.ebx + 4);
        if (r.u8(r.edi + 0x11) == 0) r.w32(r.edi + 4, r.esi);
        r.eax = head();
        if (r.u32(r.eax + 4) == r.ebx) r.w32(r.eax + 4, r.edi);
        else if (r.u32(r.esi) == r.ebx) r.w32(r.esi, r.edi);
        else r.w32(r.esi + 8, r.edi);
        r.edx = head();
        if (r.u32(r.edx) == r.ebx) {
            if (r.u8(r.edi + 0x11) != 0) r.ecx = r.esi;
            else {
                r.eax = r.u32(r.edi);
                r.ecx = r.edi;
                while (r.u8(r.eax + 0x11) == 0) { r.ecx = r.eax; r.eax = r.u32(r.ecx); }
            }
            r.w32(r.edx, r.ecx);
        }
        r.ebx = head();
        r.eax = r.u32(E + 4);
        if (r.u32(r.ebx + 8) == r.eax) {
            if (r.u8(r.edi + 0x11) != 0) { r.eax = r.esi; r.w32(r.ebx + 8, r.eax); }
            else { r.eax = r.edi; r.call(P, 0x0080d320); r.w32(r.ebx + 8, r.eax); }
        }
    } else {
        r.w32(r.ecx + 4, r.eax);
        r.ecx = r.u32(r.ebx);
        r.w32(r.eax, r.ecx);
        if (r.eax == r.u32(r.ebx + 8)) r.esi = r.eax;
        else {
            r.esi = r.u32(r.eax + 4);
            if (r.u8(r.edi + 0x11) == 0) r.w32(r.edi + 4, r.esi);
            r.w32(r.esi, r.edi);
            r.ecx = r.u32(r.ebx + 8);
            r.w32(r.edx, r.ecx);
            r.edx = r.u32(r.ebx + 8);
            r.w32(r.edx + 4, r.eax);
        }
        r.ecx = head();
        if (r.u32(r.ecx + 4) == r.ebx) r.w32(r.ecx + 4, r.eax);
        else {
            r.ecx = r.u32(r.ebx + 4);
            if (r.u32(r.ecx) == r.ebx) r.w32(r.ecx, r.eax); else r.w32(r.ecx + 8, r.eax);
        }
        r.ecx = r.u32(r.ebx + 4);
        r.w32(r.eax + 4, r.ecx);
        r.ecx = r.ebx + 0x10;
        r.eax += 0x10;
        if (r.eax != r.ecx) {
            const uint8_t bl = r.u8(r.ecx), dl = r.u8(r.eax);
            setlo8(r.ebx, bl); setlo8(r.edx, dl);
            r.w8(r.eax, bl); r.w8(r.ecx, dl);
        }
    }
    // rebalance when the erased node was black
    r.edx = r.u32(E + 4);
    setlo8(r.ebx, 1);
    if (r.u8(r.edx + 0x10) == 1) {
        r.eax = head();
        if (r.edi != r.u32(r.eax + 4)) {
            auto rot = [&](uint32_t fn) { r.call(P, fn, {r.ebp}); };
            const uint32_t L = 0x00586660, R = 0x005866b0;
            while (r.u8(r.edi + 0x10) == 1) {
                r.ecx = r.u32(r.esi);
                bool stop = false;
                if (r.edi == r.ecx) {
                    r.ecx = r.u32(r.esi + 8);
                    if (r.u8(r.ecx + 0x10) == 0) { r.w8(r.ecx + 0x10, 1); r.ecx = r.esi; r.w8(r.esi + 0x10, 0); rot(L); r.ecx = r.u32(r.esi + 8); }
                    if (r.u8(r.ecx + 0x11) == 0) {
                        r.edx = r.u32(r.ecx);
                        bool both = false;
                        if (r.u8(r.edx + 0x10) == 1) { r.eax = r.u32(r.ecx + 8); both = r.u8(r.eax + 0x10) == 1; }
                        if (both) r.w8(r.ecx + 0x10, 0);
                        else {
                            r.edx = r.u32(r.ecx + 8);
                            if (r.u8(r.edx + 0x10) == 1) { r.eax = r.u32(r.ecx); r.w8(r.eax + 0x10, 1); r.w8(r.ecx + 0x10, 0); rot(R); r.ecx = r.u32(r.esi + 8); }
                            const uint8_t dl = r.u8(r.esi + 0x10); setlo8(r.edx, dl);
                            r.w8(r.ecx + 0x10, dl); r.w8(r.esi + 0x10, 1);
                            r.eax = r.u32(r.ecx + 8);
                            r.ecx = r.esi; r.w8(r.eax + 0x10, 1);
                            rot(L);
                            stop = true;
                        }
                    }
                } else {
                    if (r.u8(r.ecx + 0x10) == 0) { r.w8(r.ecx + 0x10, 1); r.ecx = r.esi; r.w8(r.esi + 0x10, 0); rot(R); r.ecx = r.u32(r.esi); }
                    if (r.u8(r.ecx + 0x11) == 0) {
                        r.edx = r.u32(r.ecx + 8);
                        bool both = false;
                        if (r.u8(r.edx + 0x10) == 1) { r.eax = r.u32(r.ecx); both = r.u8(r.eax + 0x10) == 1; }
                        if (both) r.w8(r.ecx + 0x10, 0);
                        else {
                            r.edx = r.u32(r.ecx);
                            if (r.u8(r.edx + 0x10) == 1) { r.eax = r.u32(r.ecx + 8); r.w8(r.eax + 0x10, 1); r.w8(r.ecx + 0x10, 0); rot(L); r.ecx = r.u32(r.esi); }
                            const uint8_t dl = r.u8(r.esi + 0x10); setlo8(r.edx, dl);
                            r.w8(r.ecx + 0x10, dl); r.w8(r.esi + 0x10, 1);
                            r.eax = r.u32(r.ecx);
                            r.ecx = r.esi; r.w8(r.eax + 0x10, 1);
                            rot(R);
                            stop = true;
                        }
                    }
                }
                if (stop) break;
                r.ecx = head();
                r.edi = r.esi;
                r.esi = r.u32(r.esi + 4);
                if (r.edi == r.u32(r.ecx + 4)) break;
            }
        }
        r.w8(r.edi + 0x10, 1);
    }
    r.ecx = r.u32(E + 4);
    r.call(P, 0x0078fc50, {r.ecx});
    r.eax = r.u32(r.ebp + 0x1c);
    if (r.eax != 0) { r.eax--; r.w32(r.ebp + 0x1c, r.eax); }
    r.edx = r.u32(E + 8);
    r.eax = r.u32(E + 0x10);
    r.ecx = r.u32(r.ebp);
    r.w32(r.edx + 4, r.eax); r.w32(r.edx, r.ecx);
    r.eax = r.edx;
    return r.ret(0x10);
}

// 0067bb10 (EAX = manager, stack = collision event (64 bytes); ret 4): takes a 0x70-byte record from the pool (+0x70 / +0x74,
// lock +0x7c), fills it from the event, inserts it into the map (+0x48, lock +0x30): a new key plays it (0x0067bd30); an existing
// one merges position / strengths into the stored record (same frame: the stronger; else replaces), returns the record to the
// pool and updates the stored one (0x0067c2c0). The record pointer lives in the argument slot (its address is the key).
REFLEX_PORT_FN(0x0067bb10, AudioCollision_Add, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Regs r(m);
    const uint32_t E = r.esp, P = E - 0x28, slot = E + 4;
    r.ebp = r.u32(slot);
    r.edi = r.eax;
    r.esi = r.edi + 0x7c;
    r.ebx = 0;
    r.icall(P, ENTER_CS, {r.esi});
    r.eax = r.u32(r.edi + 0x74);
    if (r.eax != 0) { r.ecx = r.u32(r.edi + 0x70); r.eax--; r.w32(r.edi + 0x74, r.eax); r.ebx = r.u32(r.ecx + r.eax * 4); }
    r.icall(P, LEAVE_CS, {r.esi});
    r.w32(slot, r.ebx);
    if (r.ebx == 0) return r.ret(4);
    r.call(P, 0x008fa06a, {r.ebx, 0, 0x70});
    r.edx = r.u32(r.ebp); r.w32(r.ebx, r.edx);
    r.eax = r.u32(r.ebp + 4); r.w32(r.ebx + 4, r.eax);
    for (uint32_t o : {0x10u, 0x14u, 0x18u}) r.w32(r.ebx + o, fcopy32(m, r.u32(r.ebp + o)));
    setlo8(r.ecx, r.u8(r.ebp + 0x20)); r.w8(r.ebx + 0x20, uint8_t(r.ecx));
    for (uint32_t o : {0x24u, 0x28u}) r.w32(r.ebx + o, fcopy32(m, r.u32(r.ebp + o)));
    r.edx = r.u32(r.ebp + 0x2c); r.w32(r.ebx + 0x2c, r.edx);
    r.eax = r.u32(r.ebp + 0x30); r.w32(r.ebx + 0x30, r.eax);
    r.ecx = r.u32(r.ebp + 0x34); r.w32(r.ebx + 0x34, r.ecx);
    r.icall(P, TIME);
    r.edx = r.u32(0xa95844);
    r.w32(r.ebx + 0x48, r.eax);
    r.eax = r.u32(r.edx + 0x10);
    r.ecx = r.u32(r.eax + 0x58);
    r.eax = 0xffffffffu;
    r.w32(r.ebx + 0x4c, r.ecx);
    r.esi = r.edi + 0x30;
    r.w32(r.ebx + 0x50, r.eax); r.w32(r.ebx + 0x54, r.eax);
    r.icall(P, ENTER_CS, {r.esi});
    r.edx = slot; r.eax = P + 0x1c; r.ecx = r.edi + 0x48;
    r.call(P, 0x0068e130, {r.ecx, r.eax, r.edx});
    r.edx = r.u32(r.eax + 8);
    r.x[0] = X{{r.u32(r.eax), r.u32(r.eax + 4), 0, 0}};
    const uint32_t it0 = r.x[0].u[0], it1 = r.x[0].u[1], inserted = r.edx;
    r.w32(P + 0x10, it0); r.w32(P + 0x14, it1); r.w32(P + 0x18, inserted);
    r.icall(P, LEAVE_CS, {r.esi});
    r.eax = r.u32(P + 0x10);
    if (r.eax != 0) r.eax = r.u32(r.eax);
    else { r.icall(P, ABORT); r.eax = 0; }
    r.esi = r.u32(P + 0x14);
    if (r.esi == r.u32(r.eax + 0x18)) r.icall(P, ABORT);
    r.esi = r.u32(r.esi + 0xc);
    r.icall(P, TIME);
    r.w32(r.esi + 0x40, r.eax);
    r.eax = r.u32(0xa95844);
    r.ecx = r.u32(r.eax + 0x10);
    r.eax = r.u32(r.ecx + 0x58);
    r.w32(r.esi + 0x44, r.eax);
    X& x0 = r.x[0]; X& x1 = r.x[1]; X& x2 = r.x[2];
    auto copy_pos = [&] { for (uint32_t o : {0x10u, 0x14u, 0x18u}) r.w32(r.esi + o, fcopy32(m, r.u32(r.ebp + o))); };
    if (r.eax == r.u32(r.esi + 0x4c)) {
        x1 = r.ss(r.ebp + 0x24);
        const bool a = c_ja(x1.f(0), r.ss(r.esi + 0x24).f(0));
        x2 = X{};
        bool cp = a;
        if (!a) {
            x0 = r.ss(r.ebp + 0x28);
            if (!c_ja(x0.f(0), x2.f(0))) x0 = x1;
            cp = c_ja(x0.f(0), r.ss(r.esi + 0x28).f(0));
        }
        if (cp) copy_pos();
        x0 = r.ss(r.esi + 0x24); x1 = r.ss(r.ebp + 0x24);
        if (!c_ja(x0.f(0), x1.f(0))) x0 = x1;
        r.w32(r.esi + 0x24, x0.u[0]);
        x0 = r.ss(r.ebp + 0x28);
        if (!c_ja(x0.f(0), x2.f(0))) x0 = r.ss(r.ebp + 0x24);
        x1 = r.ss(r.esi + 0x28);
        if (c_ja(x1.f(0), x0.f(0))) x0 = x1;
        r.w32(r.esi + 0x28, x0.u[0]);
    } else {
        r.w32(r.esi + 0x24, fcopy32(m, r.u32(r.ebp + 0x24)));
        x0 = r.ss(r.ebp + 0x28);
        if (!c_ja(x0.f(0), r.ss(0x92e12c).f(0))) x0 = r.ss(r.ebp + 0x24);
        r.w32(r.esi + 0x28, x0.u[0]);
        copy_pos();
    }
    if ((inserted & 0xff) != 0) {
        r.call(P, 0x0067bd30, {r.edi, r.esi});
        return r.ret(4);
    }
    r.edx = r.u32(r.edi + 0x74);
    if (int32_t(r.edx) < int32_t(r.u32(r.edi + 0x78))) {
        r.ebp = r.edi + 0x7c;
        r.icall(P, ENTER_CS, {r.ebp});
        r.eax = r.u32(r.edi + 0x74);
        r.ecx = r.u32(r.edi + 0x70);
        r.w32(r.ecx + r.eax * 4, r.ebx);
        r.w32(r.edi + 0x74, r.u32(r.edi + 0x74) + 1);
        r.icall(P, LEAVE_CS, {r.ebp});
    }
    r.call(P, 0x0067c2c0);
    return r.ret(4);
}

// 0067bac0 AudioCollisionManager::vf0 (thiscall, arg = event list {64-byte events, count +0x1900}; ret 4): adds every event
// (0x0067bb10) under the map lock (+0x30). ECX ends as the lock address (the original's saved-ECX slot holds it).
REFLEX_PORT_FN(0x0067bac0, AudioCollisionManager_vf0, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Regs r(m);
    const uint32_t E = r.esp, P = E - 20;
    r.ebx = r.ecx;
    r.edi = r.ebx + 0x30;
    const uint32_t cs = r.edi;
    r.w32(P + 0x10, cs);
    r.icall(P, ENTER_CS, {r.edi});
    r.ebp = r.u32(E + 4);
    r.esi = 0;
    if (r.u32(r.ebp + 0x1900) > r.esi) {
        r.edi = r.ebp;
        do {
            r.eax = r.ebx;
            r.call(P, 0x0067bb10, {r.edi});
            r.esi++;
            r.edi += 0x40;
        } while (r.esi < r.u32(r.ebp + 0x1900));
        r.edi = r.u32(P + 0x10);
    }
    r.icall(P, LEAVE_CS, {r.edi});
    r.ecx = r.u32(P + 0x10);
    return r.ret(4);
}

// 0067c2c0 (ESI = collision record, EDI = manager): restarts a finished 0x100f contact sound (0x0067bd30), updates a 0x10 sound
// (0x0067c350) or a type-5 (with +0x19c) one (0x0067c670).
REFLEX_PORT_FN(0x0067c2c0, AudioCollision_Update, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Regs r(m);
    const uint32_t E = r.esp, P = (E - 4) & ~7u;
    if (r.u8(r.esi + 0x20) != 0) {
        bool go = r.u32(r.esi + 0x58) == 0;
        if (!go) { r.eax = r.esi; r.call(P, 0x0067c930); go = r.eax == 0; }
        if (go) {
            r.ecx = r.u32(r.esi + 4);
            if (r.ecx != 0) {
                r.vcall(P, r.ecx, 0x58, r.eax, r.edx);
                if (r.eax == 0x100f) {
                    r.w32(r.esi + 0x58, 0);
                    r.call(P, 0x0067bd30, {r.edi, r.esi});
                    return r.ret(0);
                }
            }
        }
    }
    r.ecx = r.u32(r.esi + 4);
    if (r.ecx != 0) {
        r.vcall(P, r.ecx, 0x58, r.eax, r.edx);
        if (r.eax == 0x10) { r.ecx = r.edi; r.call(P, 0x0067c350, {r.esi}); return r.ret(0); }
    }
    r.ecx = r.u32(r.esi + 4);
    if (r.ecx != 0) {
        r.vcall(P, r.ecx, 0x58, r.eax, r.edx);
        if (r.eax == 5) {
            r.eax = r.u32(r.esi + 4);
            if (r.u8(r.eax + 0x19c) != 0) { r.ecx = r.edi; r.call(P, 0x0067c670, {r.esi}); }
        }
    }
    return r.ret(0);
}

// 0067bd30 (stdcall 8: manager, collision record): plays a new collision: classifies both objects (type 0x1082 bikes, 0x100f,
// 0x13 / 0x31 riders, 0x10, 0xf), picks each side's sound id (virtual +0xd8 by surface index, else +0x30) and material (+0x34),
// scales the strengths (+0x24 / +0x28) by the class factors, maps them to 0..0x92fd88 between the manager's thresholds, kicks
// the riders' crash slots (0x006817c0) and starts up to two events (0x0080c760 / 0x0080a9f0 / 0x0080afc0 / 0x0080abd0).
REFLEX_PORT_FN(0x0067bd30, AudioCollision_Play, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Regs r(m);
    const uint32_t E = r.esp, P = (((E - 4) & ~7u) - 0x3c) - 12;
    const uint32_t mgr = m.arg(0);
    X& x0 = r.x[0]; X& x1 = r.x[1]; X& x2 = r.x[2]; X& x3 = r.x[3]; X& x4 = r.x[4];
    auto L = [&](uint32_t o) { return r.u32(P + o); };
    auto S = [&](uint32_t o, uint32_t v) { r.w32(P + o, v); };
    r.ebx = m.arg(1);
    r.ecx = r.u32(r.ebx);
    r.edi = 0;
    // kind flags: [P+0x28] / [P+0x2c] = 0x1082 (side 0 / 1)
    if (r.ecx != 0) { r.vcall(P, r.ecx, 0x58, r.eax, r.edx); r.ecx = r.eax == 0x1082 ? 1 : 0; S(0x28, r.ecx); } else S(0x28, 0);
    r.ecx = r.u32(r.ebx + 4);
    if (r.ecx != 0) { r.vcall(P, r.ecx, 0x58, r.edx, r.eax); r.ecx = r.eax == 0x1082 ? 1 : 0; S(0x2c, r.ecx); } else S(0x2c, 0);
    r.ecx = r.u32(r.ebx);
    if (r.ecx != 0) r.vcall(P, r.ecx, 0x58, r.edx, r.eax);
    r.ecx = r.u32(r.ebx + 4);
    if (r.ecx != 0) { r.vcall(P, r.ecx, 0x58, r.edx, r.eax); r.ecx = r.eax == 0x100f ? 1 : 0; S(0x3c, r.ecx); } else S(0x3c, 0);
    // rider flags [P+0x30] / [P+0x34]: type 0x13 with +0x30 = 0x31
    for (int side = 0; side < 2; side++) {
        r.ecx = r.u32(r.ebx + 4 * side);
        if (side == 0) r.esi = 1;
        bool rider = false;
        if (r.ecx != 0) {
            r.vcall(P, r.ecx, 0x58, r.edx, r.eax);
            if (r.eax == 0x13) {
                r.ecx = r.u32(r.ebx + 4 * side);
                r.vcall(P, r.ecx, 0x30, r.edx, r.eax);
                S(0x30 + 4 * side, r.esi);
                rider = r.eax == 0x31;
            }
        }
        if (!rider) S(0x30 + 4 * side, r.edi);
    }
    // type 0x10: ESI (side 0), EDI (side 1)
    r.ecx = r.u32(r.ebx);
    if (r.ecx != 0) { r.vcall(P, r.ecx, 0x58, r.edx, r.eax); r.ecx = r.eax == 0x10 ? 1 : 0; r.esi = r.ecx; } else r.esi = 0;
    r.ecx = r.u32(r.ebx + 4);
    if (r.ecx != 0) { r.vcall(P, r.ecx, 0x58, r.edx, r.eax); r.ecx = r.eax == 0x10 ? 1 : 0; r.edi = r.ecx; } else r.edi = 0;
    // type 0xf: [P+0x40] / [P+0x44]
    for (int side = 0; side < 2; side++) {
        r.ecx = r.u32(r.ebx + 4 * side);
        if (r.ecx != 0) { r.vcall(P, r.ecx, 0x58, r.edx, r.eax); r.ecx = r.eax == 0xf ? 1 : 0; S(0x40 + 4 * side, r.ecx); }
        else S(0x40 + 4 * side, 0);
    }
    // sound ids [P+0x18] / [P+0x1c] and materials [P+0x20] / [P+0x24]
    for (int side = 0; side < 2; side++) {
        r.edx = 0;
        const uint32_t id = 0x18 + 4 * side;
        bool by_index = L(0x28 + 4 * side) != 0 && (r.eax = r.u32(r.ebx + 0x30 + 4 * side), int32_t(r.eax) >= 0);
        r.ecx = r.u32(r.ebx + 4 * side);
        if (r.ecx == 0) S(id, r.edx);
        else if (by_index) { r.edx = r.u32(r.ecx); const uint32_t a = r.eax; r.eax = r.u32(r.edx + 0xd8); r.call(P, r.eax, {a}); S(id, r.eax); }
        else { r.vcall(P, r.ecx, 0x30, r.edx, r.eax); S(id, r.eax); }
        r.ecx = r.u32(r.ebx + 4 * side);
        if (r.ecx != 0) { r.vcall(P, r.ecx, 0x34, r.edx, r.eax); S(0x20 + 4 * side, r.eax); } else S(0x20 + 4 * side, 5);
    }
    r.eax = r.u32(r.ebx + 4);
    if (r.eax == 0 || L(0x3c) != 0) { r.ecx = r.u32(r.ebx + 0x2c); S(0x1c, r.ecx); }
    auto scale = [&](uint32_t o, uint32_t c) { x0 = r.ss(c); x0 = mulss(x0, r.ss(r.ebx + o)); r.w32(r.ebx + o, x0.u[0]); };
    if (r.u8(r.ebx + 0x20) != 0 && L(0x3c) != 0) { scale(0x24, 0x9ca5d0); scale(0x28, 0x9ca5d0); }
    if (L(0x30) != 0) { scale(0x24, 0x9ca5fc); scale(0x28, 0x9ca5fc); }
    r.edx = L(0x28);
    if (r.edx != 0 && r.edi != 0) scale(0x24, 0x9ca654);
    r.ecx = L(0x2c);
    if (r.ecx != 0 && r.esi != 0) scale(0x28, 0x9ca654);
    if (r.edx != 0 && r.ecx != 0) { scale(0x24, 0x9ca6ac); scale(0x28, 0x9ca6ac); }
    auto is_1029_or_5 = [&](uint32_t side) {
        r.ecx = r.u32(r.ebx + 4 * side); r.vcall(P, r.ecx, 0x58, r.edx, r.eax);
        if (r.eax == 0x1029) return true;
        r.ecx = r.u32(r.ebx + 4 * side); r.vcall(P, r.ecx, 0x58, r.edx, r.eax);
        return r.eax == 5;
    };
    if (r.eax != 0) {
        if (is_1029_or_5(0) && is_1029_or_5(1)) {
            x0 = r.ss(r.ebx + 0x24); x1 = r.ss(r.ebx + 0x28);
            if (!c_ja(x0.f(0), x1.f(0))) x0 = x1;
            r.w32(r.ebx + 0x24, x0.u[0]); r.w32(r.ebx + 0x28, x0.u[0]);
        }
        if ((L(0x30) != 0 || L(0x28) != 0) && is_1029_or_5(1)) {
            x0 = r.ss(r.ebx + 0x24); x1 = r.ss(r.ebx + 0x28);
            if (!c_ja(x0.f(0), x1.f(0))) x0 = x1;
            r.w32(r.ebx + 0x28, x0.u[0]);
        }
    }
    // normalized strengths [P+0x38] / [P+0x3c]
    r.eax = mgr;
    x1 = r.ss(r.eax + 0x1c); x2 = r.ss(r.eax + 0x20); x0 = r.ss(r.ebx + 0x24);
    x4 = X{};
    x0 = subss(x0, x1);
    x3 = x2; x3 = subss(x3, x1);
    x1 = r.ss(0x92fd88);
    x0 = divss(x0, x3); x0 = mulss(x0, x1);
    if (!c_jb(x0.f(0), x1.f(0))) { x0 = x1; S(0x38, x0.u[0]); }
    else if (c_jb(x4.f(0), x0.f(0))) S(0x38, x0.u[0]);
    else S(0x38, x4.u[0]);
    x3 = r.ss(r.eax + 0x1c); x0 = r.ss(r.ebx + 0x28);
    x0 = subss(x0, x3); x2 = subss(x2, x3); x0 = divss(x0, x2); x0 = mulss(x0, x1);
    if (!c_jb(x0.f(0), x1.f(0))) S(0x3c, x1.u[0]);
    else {
        const bool pos = c_jb(x4.f(0), x0.f(0));
        x1 = x0;
        S(0x3c, pos ? x1.u[0] : x4.u[0]);
    }
    // the two sides
    r.edi = 0;
    S(0x10, 0);
    for (;;) {
        bool skip;
        if (r.u8(r.ebx + 0x20) != 0) skip = r.edi == 0 || (r.edi == 1 && L(0x20) == 0);
        else skip = r.edi == 0 ? L(0x24) == 0 : (r.edi == 1 && L(0x20) == 0);
        if (!skip) {
            const uint32_t k = r.edi;
            if (L(0x28 + 4 * k) != 0) {
                r.esi = k == 0 ? r.u32(r.ebx) : r.u32(r.ebx + 4);
                if (r.u32(r.esi + 0x194) <= 1) {
                    r.eax = mgr;
                    x0 = r.ss(P + 0x38 + 4 * k);
                    if (!c_jb(x0.f(0), r.ss(r.eax + 0x28).f(0))) r.call(P, 0x006817c0, {4});
                    else if (!c_jb(x0.f(0), r.ss(r.eax + 0x24).f(0))) r.call(P, 0x006817c0, {3});
                }
            }
            if (L(0x30 + 4 * k) != 0) {
                r.eax = k == 0 ? r.u32(r.ebx) : r.u32(r.ebx + 4);
                r.ecx = k == 0 ? 1 : 0;
                if (L(0x40 + 4 * r.ecx) == 0 && r.u32(r.eax + 0xae70) == 0) {
                    uint32_t kind = 0;
                    if (r.u32(r.eax + 0x5bb68) != 0) {
                        r.edx = mgr;
                        x0 = r.ss(P + 0x38 + 4 * k);
                        if (!c_jb(x0.f(0), r.ss(r.edx + 0x2c).f(0))) kind = 4;
                    }
                    if (kind == 0) {
                        r.ecx = mgr;
                        x0 = r.ss(P + 0x38 + 4 * k);
                        if (!c_jb(x0.f(0), r.ss(r.ecx + 0x2c).f(0))) kind = 3;
                    }
                    if (kind != 0) {
                        r.esi = r.u32(r.eax + 0xaf1c);
                        r.call(P, 0x006817c0, {kind});
                    }
                }
            }
            r.eax = L(0x18 + 4 * k);
            r.esi = mgr;
            r.call(P, 0x0067b890);
            r.esi = r.eax;
            if (r.esi != 0) {
                const uint32_t name = r.eax;
                uint32_t len;
                strlen_regs(r, name, len);
                r.edx = name + 1;
                r.eax = len;
                if (len != 0) {
                    r.eax = mgr;
                    r.edx = r.u32(r.eax + 0xb0) - r.u32(r.eax + 0xac);
                    if ((r.edx & 0xfffffffcu) == 0) r.call(P, 0x0067c990, {r.eax});
                    r.eax = mgr + 0x98;
                    r.call(P, 0x0080c760, {r.esi, 0x95b800});
                    r.w32(r.ebx + 0x58, r.eax);
                    if (r.eax != 0) {
                        r.w32(r.eax + 0x10, r.ebx);
                        r.esi = r.u32(r.ebx + 0x58);
                        r.call(P, 0x0080a9f0);
                        r.edi = r.u32(r.ebx + 0x58);
                        r.ecx = L(0x10);
                        const uint32_t v = fcopy32(m, r.u32(P + 0x38 + 4 * r.ecx));
                        r.call(P, 0x0080afc0, {0x95c398, v});
                        r.esi = r.u32(r.ebx + 0x58);
                        r.edx = r.ebx + 0x10;
                        r.call(P, 0x0080abd0, {r.edx, 0, 0});
                        r.edi = L(0x10);
                    }
                }
            }
            r.eax = L(0x1c);
            if (L(0x18) == r.eax) break;
        }
        r.edi++;
        S(0x10, r.edi);
        if (int32_t(r.edi) >= 2) break;
    }
    return r.ret(8);
}

// 0067c350 (thiscall manager, arg = collision record; ret 4): the rolling / scraping sound of a 0x10 contact: its strength from
// the rider's speed (0x13 / 0x31: |0x00422350 velocity| scaled), the 0x1029 object's 0x0041cf30, a bike's +0x150, or the record's
// +0x24, mapped to 0..1 between thresholds; switches the event (+0x5c) when the sound id (0x007c5120 of the surface) changes,
// else updates its parameter 0x95c3a0.
REFLEX_PORT_FN(0x0067c350, AudioCollision_UpdateRoll, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Regs r(m);
    const uint32_t E = r.esp, P = (((E - 4) & ~15u) - 0x34) - 12;
    X& x0 = r.x[0]; X& x1 = r.x[1]; X& x2 = r.x[2];
    auto finish = [&] { r.ecx = r.u32(0x9b45e4); return r.ret(4); };
    if ((r.u8(0x9e30e0) & 1) == 0) { r.w32(0x9e30e0, r.u32(0x9e30e0) | 1); r.w32(0x9e30dc, 0); }
    r.ebx = m.arg(0);
    r.esi = r.ecx;
    r.w32(P + 0x18, r.esi);
    r.edx = 0x9e30d4;
    r.call(P, 0x00402dc0);
    r.edi = r.eax;
    r.edx = r.ebx + 0x10;
    r.eax = 3;
    r.call(P, 0x007c5120);
    r.ecx = r.u32(r.ebx);
    r.edi = r.eax;
    r.w32(P + 0x1c, r.edi);
    bool have = false, rate = false;   // rate: [P+0x14] already set (the rider branch)
    if (r.ecx != 0) {
        r.vcall(P, r.ecx, 0x58, r.eax, r.edx);
        bool rider = false;
        if (r.eax == 0x13) {
            r.ecx = r.u32(r.ebx);
            r.vcall(P, r.ecx, 0x30, r.eax, r.edx);
            rider = r.eax == 0x31;
        }
        if (rider) {
            r.esi = r.u32(r.ebx) + 0x950;
            r.ecx = P + 0x20;
            r.call(P, 0x00422350);
            x1 = r.ld(P + 0x20);
            x2 = r.ss(0x9ca75c);
            x1 = mulps(x1, x1);
            x0 = shufps(x1, x1, 0x66);
            x1 = addss(x1, x0);
            x0 = shufps(x0, x0, 0x55);
            x1 = addss(x1, x0);
            x0 = shufps(x1, x1, 0);
            x1 = sqrtps(x0);
            x0 = r.ss(0x9ca628);
            x0 = mulss(x0, r.ss(0x97748c));
            x0 = mulss(x0, x1);
            x1 = r.ss(0x9ca730);
            x0 = subss(x0, x1);
            x2 = subss(x2, x1);
            x1 = r.ss(0x931f24);
            x0 = divss(x0, x2);
            if (!c_jb(x0.f(0), x1.f(0))) { r.esi = r.u32(P + 0x18); r.w32(P + 0x14, x1.u[0]); }
            else {
                x2 = X{};
                const bool pos = c_jb(x2.f(0), x0.f(0));
                x1 = x0;
                r.esi = r.u32(P + 0x18);
                r.w32(P + 0x14, pos ? x1.u[0] : x2.u[0]);
            }
            rate = true;
        } else {
            r.ecx = r.u32(r.ebx);
            if (r.ecx != 0) {
                r.vcall(P, r.ecx, 0x58, r.eax, r.edx);
                r.ecx = r.u32(r.ebx);
                if (r.eax == 0x1029) {
                    r.eax = P + 0x14;
                    r.call(P, 0x0041cf30, {r.eax});
                    x0 = r.ss(P + 0x14); x0 = mulss(x0, r.ss(0x97748c));
                    x1 = r.ss(0x9ca788); x2 = r.ss(0x9ca7b4);
                    have = true;
                } else if (r.ecx != 0) {
                    r.vcall(P, r.ecx, 0x58, r.edx, r.eax);
                    if (r.eax == 0x1082) {
                        r.ecx = r.u32(r.ebx);
                        x0 = r.ss(r.ecx + 0x150); x0 = mulss(x0, r.ss(0x97748c));
                        x1 = r.ss(0x9ca6d8); x2 = r.ss(0x9ca704);
                        have = true;
                    }
                }
            }
        }
    }
    if (!rate) {
        if (!have) { x1 = r.ss(r.esi + 0x1c); x0 = r.ss(r.ebx + 0x24); x2 = r.ss(r.esi + 0x20); }
        x0 = subss(x0, x1); x2 = subss(x2, x1);
        x1 = r.ss(0x931f24);
        x0 = divss(x0, x2);
        if (!c_jb(x0.f(0), x1.f(0))) { x0 = x1; r.w32(P + 0x14, x0.u[0]); }
        else {
            x1 = X{};
            if (c_jb(x1.f(0), x0.f(0))) r.w32(P + 0x14, x0.u[0]);
            else r.w32(P + 0x14, x1.u[0]);
        }
    }
    if (r.edi != r.u32(r.ebx + 0x50)) {
        r.esi = r.u32(r.ebx + 0x5c);
        if (r.esi != 0) {
            r.edi = r.u32(0xd67d1c);
            r.icall(P, THREAD_ID);
            if (r.edi == r.eax) r.call(P, 0x0080ae10);
            else r.call(P, 0x0080c280, {1, r.esi, 0, 0});
            r.edi = r.u32(P + 0x1c);
            r.w32(r.ebx + 0x5c, 0);
        }
        r.esi = r.u32(P + 0x18);
    }
    r.eax = r.u32(r.ebx + 0x5c);
    auto set_param = [&] {
        const uint32_t v = fcopy32(m, r.u32(P + 0x14));
        r.edi = r.u32(r.ebx + 0x5c);
        r.call(P, 0x0080afc0, {0x95c3a0, v});
    };
    if (r.eax == 0) {
        r.eax = r.edi;
        r.w32(r.ebx + 0x50, r.edi);
        r.call(P, 0x00679420);
        r.edi = r.eax;
        if (r.edi == 0) return finish();
        uint32_t len;
        strlen_regs(r, r.eax, len);
        r.edx = r.edi + 1;
        r.eax = len;
        if (len == 0) return finish();
        r.edx = r.u32(r.esi + 0xb0) - r.u32(r.esi + 0xac);
        if ((r.edx & 0xfffffffcu) == 0) r.call(P, 0x0067c990, {r.esi});
        r.eax = r.esi + 0x98;
        r.call(P, 0x0080c760, {r.edi, 0x95b800});
        r.w32(r.ebx + 0x5c, r.eax);
        if (r.eax == 0) return finish();
        r.w32(r.eax + 0x10, r.ebx);
        r.esi = r.u32(r.ebx + 0x5c);
        r.call(P, 0x0080a9f0);
        set_param();
        r.esi = r.u32(r.ebx + 0x5c);
        r.eax = r.ebx + 0x10;
        r.call(P, 0x0080abd0, {r.eax, 0, 0});
        return finish();
    }
    if (r.u32(r.eax + 0x10) != r.ebx) { r.w32(r.ebx + 0x5c, 0); return finish(); }
    r.edi = r.ebx + 0x10;
    r.call(P, 0x0080ae80, {r.eax});
    set_param();
    return finish();
}

// 0067c990 (stdcall 4: manager): when the event pool is exhausted: under the map lock, finds the loudest collision record whose
// event is ours (0x0067c930's test) by its nearest-listener distance (0x00805420), stops its events (0x0080c810), erases it
// (0x0068e270), returns it to the record pool and flags +0x68.
REFLEX_PORT_FN(0x0067c990, AudioCollision_StealLoudest, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Regs r(m);
    const uint32_t E = r.esp, P = E - 0x30;
    const uint32_t mgr = m.arg(0);
    X& x0 = r.x[0];
    x0 = X{};
    r.edi = mgr;
    r.eax = r.edi + 0x30;
    r.w32(P + 0x18, x0.u[0]);
    r.w32(P + 0x14, 0);
    r.w32(P + 0x20, r.eax);
    r.icall(P, ENTER_CS, {r.eax});
    r.eax = r.u32(r.edi + 0x60);
    r.ebp = r.u32(r.eax);
    r.esi = r.u32(r.edi + 0x48);
    r.w32(P + 0x2c, r.ebp);
    r.w32(P + 0x28, r.esi);
    for (;;) {
        r.eax = r.edi + 0x48;
        r.edi = r.u32(r.eax + 0x18);
        r.eax = r.u32(r.eax);
        r.ebx = r.u32(ABORT);
        if (r.esi == 0 || r.esi != r.eax) r.call(P, r.ebx);
        if (r.ebp == r.edi) break;
        if (r.esi == 0) r.call(P, r.ebx); else r.esi = r.u32(r.esi);
        if (r.ebp == r.u32(r.esi + 0x18)) r.call(P, r.ebx);
        r.ebp = r.u32(r.ebp + 0xc);
        r.edx = r.u32(r.ebp + 0x58);
        bool cand = false;
        if (r.edx != 0) {
            uint32_t len;
            const uint32_t s = r.edx + 0x18;
            uint32_t e = s; uint8_t c;
            do { c = r.u8(e); e++; } while (c);
            setlo8(r.ecx, 0);
            r.esi = s + 1;
            len = e - (s + 1);
            r.eax = len;
            if (len != 0) { const uint32_t c = strcmp_regs(r, r.u32(r.edx + 0x134), 0x95b800); r.eax = c; cand = c == 0; }
        }
        if (cand) {
            r.ecx = P + 0x1c;
            r.edx = r.ebp + 0x10;
            r.call(P, 0x00805420, {r.edx, r.ecx});
            if (r.eax != 0) {
                x0 = r.ss(P + 0x1c);
                if (c_ja(x0.f(0), r.ss(P + 0x18).f(0))) { r.w32(P + 0x14, r.ebp); r.w32(P + 0x18, x0.u[0]); }
            }
        }
        r.esi = P + 0x28;
        r.call(P, 0x0068fc80);
        r.ebp = r.u32(P + 0x2c);
        r.esi = r.u32(P + 0x28);
        r.edi = mgr;
    }
    r.ebx = r.u32(LEAVE_CS);
    if (r.u32(P + 0x14) != 0) {
        r.edi = mgr;
        r.eax = P + 0x14;
        r.ecx = P + 0x28;
        r.edi += 0x48;
        r.call(P, 0x0068e510, {r.edi, r.ecx, r.eax});
        r.esi = r.u32(r.edi + 0x18);
        r.eax = r.u32(r.edi);
        r.edi = r.u32(P + 0x28);
        if (r.edi == 0 || r.edi != r.eax) r.icall(P, ABORT);
        r.ebp = r.u32(P + 0x2c);
        if (r.ebp != r.esi) {
            if (r.edi == 0) { r.icall(P, ABORT); r.eax = 0; } else r.eax = r.u32(r.edi);
            if (r.ebp == r.u32(r.eax + 0x18)) r.icall(P, ABORT);
            r.esi = r.u32(r.ebp + 0xc);
            r.eax = r.u32(r.esi + 0x58);
            if (r.eax != 0) { r.edx = mgr + 0x98; r.call(P, 0x0080c810, {r.edx}); r.w32(r.esi + 0x58, 0); }
            r.eax = r.u32(r.esi + 0x5c);
            if (r.eax != 0) { r.ecx = mgr + 0x98; r.call(P, 0x0080c810, {r.ecx}); r.w32(r.esi + 0x5c, 0); }
            r.eax = mgr;
            r.edx = P + 0x28;
            r.eax += 0x48;
            r.call(P, 0x0068e270, {r.eax, r.edx, r.edi, r.ebp});
            r.edi = mgr;
            r.eax = r.u32(r.edi + 0x74);
            r.ebp = 1;
            r.w32(r.edi + 0x68, r.ebp);
            if (int32_t(r.eax) < int32_t(r.u32(r.edi + 0x78))) {
                r.edi += 0x7c;
                r.icall(P, ENTER_CS, {r.edi});
                r.eax = mgr;
                r.ecx = r.u32(r.eax + 0x74);
                r.edx = r.u32(r.eax + 0x70);
                r.w32(r.edx + r.ecx * 4, r.esi);
                r.w32(r.eax + 0x74, r.u32(r.eax + 0x74) + r.ebp);
                r.call(P, r.ebx, {r.edi});
            }
        }
    }
    r.eax = r.u32(P + 0x20);
    r.call(P, r.ebx, {r.eax});
    return r.ret(4);
}
