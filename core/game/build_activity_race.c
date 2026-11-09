#include "../src/fmt_modtrack.h"
/* build_activity_race.c -- construction unit 8: the activity of the event (build_activity.h, construction.md section 16).
 * MXWorld::vf3 007665f0 (on the loader thread, from FUN_00759f00): the world's particle effects (fx unit), the activity manager
 * (MXWorld + 0x21da0: 0055ee60, its "<level> Root Activity Channel"), then one activity per row of the activity table (the factory
 * 0055ef80 -> MXActivityRace 0x13e660 bytes: 00565830 / 005562c0 / 005825a0), its vf1 (the row's Duration / ActivityMode /
 * ActivityArea / Spectator / NumLaps, the activity's name) and vf5 (the activity area: its subscene, the physics world finalized, the
 * audio logic, the lap pacing, the track's AI path).
 * The event-system channels (EventSystem::Channel: 00593900 the channel registry, 005930a0 / 007ce320 / 00593520 / 00593950) are
 * here too: rx_event_channel_create is the API the startup unit uses for the global channels. */
#include "build_activity_util.h"
#include "build_activity_freestyle.h"
#include "waypoint_activity.h"
#include "race_challenge_activity.h"
#include "race_champ.h"
#include "build_rider.h"
#include "build_world.h"
#include "build_mxworld.h"
#include "world_misc.h"
#include "world_wreck.h"
#include "build_xml.h"
#include "build_asset.h"
#include "build_scene.h"
#include "race.h"
#include "race_misc.h"
#include "fx.h"
#include "race_section.h"
#include "race_tag.h"
#include "race_poker.h"
#include "race_snake.h"
#include "race_minigames.h"
#include "loop_world.h"
#include "../src/fmt_scene.h"
#include <string.h>
#include <stdarg.h>
#include "activity_cues.h"
#include "rider_cap.h"

/* fld dword / fstp dword: the bits, a signalling NaN quieted */
static uint32_t x87_copy(uint32_t b) { return ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu)) ? (b | 0x400000u) : b; }

/* FUN_0078fd60 (EDI = buffer, ESI = size; cdecl format, ...): _vsnprintf, the terminator written when it fits, else the last byte */
static void act_format(rx_addr dst, uint32_t size, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
static void act_format(rx_addr dst, uint32_t size, const char* fmt, ...) {
    char buf[1024];
    va_list ap; va_start(ap, fmt); int n = vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    if (n < 0) n = 0;
    uint32_t len = (uint32_t)n < size ? (uint32_t)n : size;   /* _vsnprintf writes min(n, size) characters */
    for (uint32_t i = 0; i < len; i++) SU8(dst, i, (uint8_t)buf[i]);
    if (n > 0 && (uint32_t)n < size) SU8(dst, (uint32_t)n, 0);
    else SU8(dst, size - 1, 0);
}
/* a game C string copied to a host buffer (one of four rotating buffers) */
static const char* host_str(rx_addr s) {
    static char b[4][512]; static int k;
    char* d = b[k++ & 3];
    uint32_t i = 0;
    for (; i < 511; i++) { d[i] = (char)U8(s, i); if (!d[i]) break; }
    d[i] = 0;
    return d;
}

/* InitializeCriticalSection as the reference boot leaves it: {0, -1, 0, 0, 0, 0} (the OS object is not built) */
static void cs_init(rx_addr cs) {
    SU32(cs, 0, 0); SU32(cs, 4, 0xffffffffu); SU32(cs, 8, 0); SU32(cs, 0xc, 0); SU32(cs, 0x10, 0); SU32(cs, 0x14, 0);
}

/* a call of another unit's function through the registry, the original while it is not ported (oracle only) */
static void vcall1(rx_addr obj, uint32_t slot, uint32_t a0) {
    const rx_addr fn = U32(U32(obj, 0), slot);
    void (*f)(rx_addr, uint32_t) = RX_FN(fn, void (*)(rx_addr, uint32_t));
    if (f) ACT_CALL(1, f(obj, a0));
    else rx_call_original(fn, RX_THISCALL, obj, 1, &a0);
}
static void vcall0(rx_addr obj, uint32_t slot) {
    const rx_addr fn = U32(U32(obj, 0), slot);
    void (*f)(rx_addr) = RX_FN(fn, void (*)(rx_addr));
    if (f) ACT_CALL(0, f(obj));
    else rx_call_original(fn, RX_THISCALL, obj, 0, 0);
}
/* a register-argument constructor of another unit (reg = 0 EAX / 6 ESI / 7 EDI) */
static rx_addr ctor_reg(rx_addr fn, int reg, rx_addr obj) {
    rx_addr (*f)(rx_addr) = RX_FN(fn, rx_addr (*)(rx_addr));
    if (f) { rx_addr r; ACT_CALLR(r, 0, f(obj)); return r; }
    rx_regs in, out; memset(&in, 0, sizeof in); in.r[reg] = obj;
    rx_call_original_ex(fn, &in, &out, 0, 0);
    return out.r[0];
}

/* ======================================================================================================================== */
/* the event-system channels                                                                                                */

/* std::map<std::string, T*> (MSVC9 _Tree: +0 the proxy, +0x18 the head, +0x1c the size; node 0x30: left, parent, right, +0xc the
 * key string, +0x28 the value, +0x2c color, +0x2d isnil) */
static int isnil(rx_addr n) { return U8(n, 0x2d) != 0; }
static void tree_lrotate(rx_addr map, rx_addr w) {   /* _Lrotate */
    rx_addr p = U32(w, 8);
    SU32(w, 8, U32(p, 0));
    if (!isnil(U32(p, 0))) SU32(U32(p, 0), 4, w);
    SU32(p, 4, U32(w, 4));
    rx_addr head = U32(map, 0x18);
    if (w == U32(head, 4)) SU32(head, 4, p);
    else if (w == U32(U32(w, 4), 0)) SU32(U32(w, 4), 0, p);
    else SU32(U32(w, 4), 8, p);
    SU32(p, 0, w);
    SU32(w, 4, p);
}
static void tree_rrotate(rx_addr map, rx_addr w) {   /* _Rrotate */
    rx_addr p = U32(w, 0);
    SU32(w, 0, U32(p, 8));
    if (!isnil(U32(p, 8))) SU32(U32(p, 8), 4, w);
    SU32(p, 4, U32(w, 4));
    rx_addr head = U32(map, 0x18);
    if (w == U32(head, 4)) SU32(head, 4, p);
    else if (w == U32(U32(w, 4), 8)) SU32(U32(w, 4), 8, p);
    else SU32(U32(w, 4), 0, p);
    SU32(p, 8, w);
    SU32(w, 4, p);
}
/* 00586700 _Insert(addleft, where, pair) (EDI = map, EAX = where): the node 00851b80 (ReflexAlignedAlloc(0x30), the key copied by
 * basic_string's copy constructor, the value), linked, the red-black fix-up */
static rx_addr tree_insert_at(rx_addr map, int addleft, rx_addr where, rx_addr pair) {
    if (U32(map, 0x1c) >= 0x7fffffeu) act_unreached(0x00586711, "map/set<T> too long");
    rx_addr head = U32(map, 0x18);
    rx_addr n = rx_reflex_aligned_alloc(0x30);
    if (n) {
        SU32(n, 0, head); SU32(n, 4, where); SU32(n, 8, head);
        crt_str_ctor_copy(n + 0xc, pair);
        SU32(n, 0x28, U32(pair, 0x1c));
        SU8(n, 0x2c, 0); SU8(n, 0x2d, 0);
    }
    SU32(map, 0x1c, U32(map, 0x1c) + 1);
    if (where == head) { SU32(head, 4, n); SU32(U32(map, 0x18), 0, n); SU32(U32(map, 0x18), 8, n); }
    else if (addleft) { SU32(where, 0, n); if (where == U32(U32(map, 0x18), 0)) SU32(U32(map, 0x18), 0, n); }
    else { SU32(where, 8, n); if (where == U32(U32(map, 0x18), 8)) SU32(U32(map, 0x18), 8, n); }
    for (rx_addr p = n; U8(U32(p, 4), 0x2c) == 0;) {
        rx_addr par = U32(p, 4), gp = U32(par, 4);
        if (par == U32(gp, 0)) {
            rx_addr u = U32(gp, 8);
            if (U8(u, 0x2c) == 0) { SU8(par, 0x2c, 1); SU8(u, 0x2c, 1); SU8(U32(U32(p, 4), 4), 0x2c, 0); p = U32(U32(p, 4), 4); }
            else {
                if (p == U32(par, 8)) { p = par; tree_lrotate(map, p); }
                SU8(U32(p, 4), 0x2c, 1); SU8(U32(U32(p, 4), 4), 0x2c, 0); tree_rrotate(map, U32(U32(p, 4), 4));
            }
        } else {
            rx_addr u = U32(gp, 0);
            if (U8(u, 0x2c) == 0) { SU8(par, 0x2c, 1); SU8(u, 0x2c, 1); SU8(U32(U32(p, 4), 4), 0x2c, 0); p = U32(U32(p, 4), 4); }
            else {
                if (p == U32(par, 0)) { p = par; tree_rrotate(map, p); }
                SU8(U32(p, 4), 0x2c, 1); SU8(U32(U32(p, 4), 4), 0x2c, 0); tree_lrotate(map, U32(U32(p, 4), 4));
            }
        }
    }
    SU8(U32(U32(map, 0x18), 4), 0x2c, 1);
    return n;
}
/* 0063bf50 --it */
static rx_addr tree_prev(rx_addr it) {
    if (isnil(it)) return U32(it, 8);
    if (!isnil(U32(it, 0))) { it = U32(it, 0); while (!isnil(U32(it, 8))) it = U32(it, 8); return it; }
    rx_addr p;
    while (!isnil(p = U32(it, 4)) && it == U32(p, 0)) it = p;
    return isnil(it) ? it : p;
}
/* TreeIterator_Increment 00593dc0 */
static rx_addr tree_next(rx_addr it) {
    if (!isnil(U32(it, 8))) { it = U32(it, 8); while (!isnil(U32(it, 0))) it = U32(it, 0); return it; }
    rx_addr p;
    while (!isnil(p = U32(it, 4)) && it == U32(p, 8)) it = p;
    return p;
}
/* 00593950 / 00419d60 insert(pair) (unique keys; stdcall map, &out {iterator (the map's proxy, node), bool}, &pair; RET 0xc): the
 * descent by operator<(string, string) (IAT 0x9161bc), the predecessor test, the node */
rx_addr rx_event_map_insert(rx_addr map, rx_addr out, rx_addr pair) {
    rx_addr head = U32(map, 0x18), t = U32(head, 4), where = head;
    int left = 1;
    while (!isnil(t)) { where = t; left = (int)(crt_str_less(pair, t + 0xc) & 0xff); t = left ? U32(t, 0) : U32(t, 8); }
    rx_addr it = where;
    if (left) {
        if (it == U32(head, 0)) {
            rx_addr n = tree_insert_at(map, 1, where, pair);
            SU32(out, 0, U32(map, 0)); SU32(out, 4, n); SU8(out, 8, 1);
            return out;
        }
        it = tree_prev(it);
    }
    if (crt_str_less(it + 0xc, pair) & 0xff) {
        rx_addr n = tree_insert_at(map, left, where, pair);
        SU32(out, 0, U32(map, 0)); SU32(out, 4, n); SU8(out, 8, 1);
        return out;
    }
    SU32(out, 0, U32(map, 0)); SU32(out, 4, it); SU8(out, 8, 0);
    return out;
}
RX_FUNCTION(0x00593950, rx_event_map_insert)

/* 007ce320: a std::list head node (ReflexAlignedAlloc(0xc), next = prev = itself) */
rx_addr rx_event_list_head_alloc(void) {
    rx_addr n = rx_reflex_aligned_alloc(0xc);
    if (n) SU32(n, 0, n);
    if (n + 4) SU32(n, 4, n);
    return n;
}
RX_FUNCTION(0x007ce320, rx_event_list_head_alloc)

/* 00593900: the channel registry 0x00dcbeb4 {+0 the map<string, Channel*> (proxy, +0x18 head, +0x1c size), +0x20 its critical
 * section}; its static initializer (bit 0 of 0x00dce780: 00593e40, the critical section, the name string 0x00af6660 resized to 0x200,
 * the atexit destructor (not registered: the process exit is not core)) */
rx_addr rx_event_channel_registry(void) {
    if (!(U32(0, 0x00dce780) & 1)) {
        SU32(0, 0x00dce780, U32(0, 0x00dce780) | 1);
        rx_addr p = rx_reflex_aligned_alloc(4);           /* 00593e40 */
        if (p) SU32(p, 0, 0x00dcbeb4);
        SU32(0, 0x00dcbeb4, p);
        rx_addr h = rx_rider_map30_head_alloc();
        SU32(0, 0x00dcbecc, h);
        SU8(h, 0x2d, 1);
        SU32(U32(0, 0x00dcbecc), 4, U32(0, 0x00dcbecc)); SU32(U32(0, 0x00dcbecc), 0, U32(0, 0x00dcbecc));
        SU32(U32(0, 0x00dcbecc), 8, U32(0, 0x00dcbecc));
        SU32(0, 0x00dcbed0, 0);
        cs_init(0x00dcbed4);
        /* std::string::resize(0x200) of 0x00af6660 (append 0x200 - size zero characters) */
        const uint32_t size = U32(0x00af6660, 0x14);
        if (size < 0x200) {
            if (crt_str_grow(0x00af6660, 0x200, 0)) {
                const rx_addr d = U32(0x00af6660, 0x18) < 0x10 ? 0x00af6664 : U32(0x00af6660, 4);   /* the buffer (+4) or its pointer */
                for (uint32_t i = size; i < 0x200; i++) SU8(d, i, 0);
                crt_str_eos(0x00af6660, 0x200);
            }
        } else crt_str_eos(0x00af6660, 0x200);
#ifdef RX_ORACLE   /* _atexit(0x00912750): the process exit is not core (as build_globals_int.h's RX_ATEXIT) */
        { uint32_t a_[1] = {0x00912750}; rx_call_original(0x008f9fbf, RX_CDECL, 0, 1, a_); }
#endif
    }
    return 0x00dcbeb4;
}
RX_FUNCTION(0x00593900, rx_event_channel_registry)

/* 005930a0 (EDI = the channel; stdcall parent; RET 4): EventSystem::BaseChannel: vtable 0x00946250, +4 the parent channel, +8 the
 * map<string, Event*> of its events, +0x28 a critical section */
rx_addr rx_event_base_channel_construct(rx_addr ch, rx_addr parent) {
    SU32(ch, 0, 0x00946250);
    rx_rider_map30_init(ch + 8);
    cs_init(ch + 0x28);
    SU32(ch, 4, parent);
    return ch;
}
RX_FUNCTION(0x005930a0, rx_event_base_channel_construct)

/* 00593520 (EAX = registry; stdcall name, channel; RET 8): under the registry's locks, the channel registered under the name (the
 * name string 0x00af6660 = name; insert (name copy, channel)); with a parent channel, a copy of each of the parent's events (Event
 * 0x40 bytes: vtable 0x00946248, +4 the event's signature name, +8 its handler list (proxy, +0x14 head, +0x18 size), +0x24 a
 * critical section, +0x3c 0) inserted into the channel's own event map (00419d60) */
void rx_event_channel_register(rx_addr registry, rx_addr name, rx_addr ch) {
    ACT_FRAME_EBP(L, 8, 0xac, 3);
    crt_str_operator_assign(0x00af6660, name);
    const rx_addr s1 = L + 0x20, pair = L + 0x80, out = L + 0xac;
    crt_str_ctor_copy(s1, 0x00af6660);
    SU32(L, 0x3c, ch);
    crt_str_ctor_copy(pair, s1);
    SU32(pair, 0x1c, ch);
    ACT_CALL(3, rx_event_map_insert(registry, out, pair));
    crt_str_dtor(pair);
    crt_str_dtor(s1);
    const rx_addr parent = U32(ch, 4);
    if (parent) {
        const rx_addr phead = U32(parent, 0x20);
        const rx_addr s2 = L + 0x40, pair2 = L + 0x60;
        for (rx_addr it = U32(phead, 0); it != phead; it = tree_next(it)) {
            const rx_addr src = U32(it, 0x28);
            rx_addr ev = rx_reflex_aligned_alloc(0x40);
            if (ev) {
                SU32(ev, 0, 0x00946248);
                SU32(ev, 4, U32(src, 4));
                rx_addr p = rx_reflex_aligned_alloc(4);
                if (p) SU32(p, 0, ev + 8);
                SU32(ev, 8, p);
                SU32(ev, 0x1c, rx_event_list_head_alloc());
                SU32(ev, 0x20, 0);
                cs_init(ev + 0x24);
                SU32(ev, 0x3c, 0);
            }
            crt_str_ctor_copy(s2, it + 0xc);
            SU32(L, 0x5c, ev);
            crt_str_ctor_copy(pair2, s2);
            SU32(pair2, 0x1c, ev);
            ACT_CALL(3, rx_event_map_insert(ch + 8, L + 0xa0, pair2));
            crt_str_dtor(pair2);
            crt_str_dtor(s2);
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x00593520, rx_event_channel_register)

/* 00592bd0 (ESI = event, EAX = the signature's type name): EventSystem::Event (0x40 bytes: vtable 0x00946248, +4 the signature name,
 * +8 the handler list (proxy, +0x1c head, +0x20 size), +0x24 a critical section, +0x3c 0) */
rx_addr rx_event_construct(rx_addr ev, rx_addr signature) {
    SU32(ev, 0, 0x00946248);
    SU32(ev, 4, signature);
    rx_addr p = rx_reflex_aligned_alloc(4);
    if (p) SU32(p, 0, ev + 8);
    SU32(ev, 8, p);
    SU32(ev, 0x1c, rx_event_list_head_alloc());
    SU32(ev, 0x20, 0);
    cs_init(ev + 0x24);
    SU32(ev, 0x3c, 0);
    return ev;
}
RX_FUNCTION(0x00592bd0, rx_event_construct)

/* An event type registered on a channel: the instantiations FUN_00590c00 ... (EAX = channel; stdcall name; RET 4), one per handler
 * signature (the RTTI type descriptor of the signature, e.g. 0x009cbd24 = void (*)(Activity*)): under the channel's lock, when the
 * channel's event map (+8) has no event of that name (StringMap_Find 00702ad0), a new Event with the signature's type name
 * (type_info::_name_internal_method on the type list 0x009e1a7c) is inserted under the name (00419d60). */
void rx_event_type_register_at(rx_addr ch, rx_addr name, rx_addr signature_type, int aligned, int32_t call_sp) {
    const rx_addr entry = rx_sp();
    /* the type name's undecoration (msvcr90) keeps a stack address in the CRT's data: called at the instantiation's own depth */
    const rx_addr tsp = aligned ? ((entry - 4) & ~7u) + (uint32_t)call_sp : entry + (uint32_t)call_sp;
    ACT_FRAME_EBP(L, 8, 0x74, 3);
    const rx_addr s1 = L + 0x24, it = L + 0x14, s2 = L + 0x40, pair = L + 0x60, out = L + 0x18;
    crt_str_ctor_cstr(s1, name);
    ACT_CALL(2, rx_world_string_map_find(ch + 8, it, s1));
    const rx_addr cont = U32(it, 0), node = U32(it, 4);
    crt_str_dtor(s1);
    if (cont == 0 || cont != U32(ch, 8)) act_unreached(0x00590c5e, "StringMap_Find iterator (_invalid_parameter_noinfo)");
    if (node == U32(ch + 8, 0x18)) {
        rx_addr ev = rx_reflex_aligned_alloc(0x40);
        if (ev) {
            const rx_addr sp_ = rx_sp();
            rx_set_sp(tsp + 8);   /* the caller's ESP before "push node; call" (crt_type_info_name models 7857bfaf from there) */
#ifdef RX_ORACLE
            if (getenv("RX_B11_LOG") && !U32(signature_type, 4)) fprintf(stderr, "b11 first name %08x at %08x (register_at entry %08x)\n", signature_type, tsp, entry);
#endif
            const rx_addr sig = crt_type_info_name(signature_type, 0x009e1a7c);

            rx_set_sp(sp_);
            rx_event_construct(ev, sig);
        }
        crt_str_ctor_cstr(s1, name);
        crt_str_ctor_copy(s2, s1);
        SU32(L, 0x5c, ev);
        crt_str_ctor_copy(pair, s2);
        SU32(pair, 0x1c, ev);
        ACT_CALL(3, rx_event_map_insert(ch + 8, out, pair));
        crt_str_dtor(pair);
        crt_str_dtor(s2);
        crt_str_dtor(s1);
    }
    ACT_END(L);
}
void rx_event_type_register(rx_addr ch, rx_addr name, rx_addr signature_type) { rx_event_type_register_at(ch, name, signature_type, 1, -136); }

/* the inlined EventSystem::Channel constructor (0x5c bytes): the base channel (no parent), vtable 0x00946258, +0x40 a std::list
 * (proxy, +0x54 head, +0x58 size) */
static rx_addr channel_new(void) {
    rx_addr ch = rx_reflex_aligned_alloc(0x5c);
    if (!ch) return 0;
    ACT_CALL(1, rx_event_base_channel_construct(ch, 0));
    SU32(ch, 0, 0x00946258);
    rx_addr p = rx_reflex_aligned_alloc(4);
    if (p) SU32(p, 0, ch + 0x40);
    SU32(ch, 0x40, p);
    SU32(ch, 0x54, rx_event_list_head_alloc());
    SU32(ch, 0x58, 0);
    return ch;
}
/* a new EventSystem::Channel registered under the name (the sequence of 0055ee60) */
rx_addr rx_event_channel_create(rx_addr name) {
    const rx_addr reg = rx_event_channel_registry();
    const rx_addr ch = channel_new();
    ACT_CALL(2, rx_event_channel_register(reg, name, ch));
    return ch;
}

/* ======================================================================================================================== */
/* the activity manager (MXWorld + 0x21da0: +0 the world, +4 the activity array {data, count, capacity, 0, grow}, +0x18 the root
 * channel, +0x1c the debug-selected activity)                                                                              */

/* 0055ee60 (stdcall manager, world, level name; RET 0xc) */
void rx_activity_manager_init(rx_addr mgr, rx_addr world, rx_addr level) {
    ACT_FRAME(L, 0x110, 4);
    const rx_addr name = L + 0x10;
    act_format(name, 0x104, "%s Root Activity Channel", host_str(level));
    rx_addr reg;
    ACT_CALLR(reg, 0, rx_event_channel_registry());
    const rx_addr ch = channel_new();
    ACT_CALL(2, rx_event_channel_register(reg, name, ch));
    SU32(mgr, 0x18, ch);
    SU32(mgr, 0, world);
    SU32(mgr, 0x1c, 0xffffffffu);
    ACT_END(L);
}
RX_FUNCTION(0x0055ee60, rx_activity_manager_init)

/* ---- MXActivityRace (0x13e660 bytes) ---- */

/* 00536ca0 (ESI = block): the activity's audio / track block (+0x890; the world's +0x21e10 is the same class): [0x009e3104] guards
 * an InitializeCriticalSection(0x00ae1cf0), the fields, 0x1e0 bytes zero */
static void track_block_construct(rx_addr b) {
    if (U32(0, 0x009e3104) == 0) { cs_init(0x00ae1cf0); SU32(0, 0x009e3104, 1); }
    SU32(b, 0x320, 0); SU32(b, 8, 0xf); SU32(b, 0x2f4, 0); SU32(b, 0x2f8, 0); SU32(b, 0x2fc, 0); SU32(b, 4, 0); SU32(b, 0, 0);
    for (uint32_t i = 0; i < 0x1e0; i++) SU8(b + 0x10, i, 0);
}
/* 005562c0 (EDI = activity, ECX = type; stdcall world; RET 4): Activity's constructor */
rx_addr rx_activity_construct(rx_addr A, uint32_t type, rx_addr world) {
    SU32(A, 0, 0x00942904);   /* Activity::vftable */
    SU32(A, 0x10, 0);
    SU32(A, 0x21c, 5);        /* Inactive */
    SU32(A, 0x220, 0);
    SU32(A, 0x22c, 0);
    SU32(A, 0x234, world);
    SU32(A, 0x238, 0);
    SU32(A, 0x23c, type);
    rx_rider_map30_init(A + 0x240);   /* (inlined: proxy, head 00690910, isnil, links, size) */
    SU32(A, 0x260, 1);
    SU32(A, 0x264, 0);
    SU32(A, 0x268, rx_reflex_aligned_alloc(12 * 4));
    SU32(A, 0x26c, 0); SU32(A, 0x270, 12); SU32(A, 0x274, 0); SU32(A, 0x278, 12);
    SU32(A, 0x27c, rx_reflex_aligned_alloc(12 * 4));
    SU32(A, 0x280, 0); SU32(A, 0x284, 12); SU32(A, 0x288, 0); SU32(A, 0x28c, 12);
    SU32(A, 0x2a4, 0xff7fffffu);   /* -FLT_MAX (0x977550) */
    SU32(A, 0x2ac, 0x42700000u);   /* 60 (0x931da0) */
    SU32(A, 0x2b0, 0x40a00000u); SU32(A, 0x2b4, 0x40a00000u);   /* 5 (0x935890) */
    SU32(A, 0x290, 0); SF32(A, 0x294, 0.0f); SF32(A, 0x298, 0.0f); SF32(A, 0x29c, 0.0f); SU32(A, 0x2a0, 0); SF32(A, 0x2a8, 0.0f);
    SU32(A, 0x2b8, 3);
    SU32(A, 0x2bc, 0x7f7fffffu);   /* FLT_MAX (0x977494): the duration */
    SU32(A, 0x2c0, 10);
    SF32(A, 0x2c4, 0.0f);
    SU32(A, 0x2c8, 0); SU32(A, 0x2cc, 0);
    SU32(A, 0x880, 0x10);
    track_block_construct(A + 0x890);
    const uint32_t m2 = U32(0, 0x009d7b30), m1 = U32(0, 0x009cd9e8);   /* -2, -1 (.data) */
    SU32(A, 0xbc0, 0); SU32(A, 0xec4, 0); SU32(A, 0xec8, 0); SU32(A, 0xed0, 0); SU32(A, 0xfd8, 0); SU32(A, 0xfe4, 0);
    SF32(A, 0xfec, 0.0f); SF32(A, 0xff0, 0.0f);
    SU32(A, 0x100c, m2); SU32(A, 0x1010, 0x3f800000u);
    SU32(A, 0x1028, 0x40000000u); SU32(A, 0x1030, 0x40000000u);
    SU32(A, 0x1038, 0x40400000u);
    SU32(A, 0x1014, m2);
    SU32(A, 0x1044, 0x41200000u);
    SU32(A, 0x1018, 0x3f800000u); SU32(A, 0x101c, m1); SU32(A, 0x1020, 0x3f800000u); SU32(A, 0x1024, m1); SU32(A, 0x102c, m1);
    SF32(A, 0x1034, 0.0f);
    SU8(A, 0x103c, 0);
    SU32(A, 0x1040, 0);
    SF32(A, 0x1050, 0.0f); SU32(A, 0x1054, 0x40a00000u);
    SU32(A, 0x1048, m1); SU32(A, 0x104c, 0x3f800000u); SU32(A, 0x1058, m1); SU32(A, 0x105c, 0x3f800000u);
    SF32(A, 0x1060, 0.0f); SU32(A, 0x1064, 0x40a00000u);
    SU32(A, 0x1068, 0); SU32(A, 0x106c, 0);
    for (uint32_t i = 0; i < 12; i++) {   /* `vector constructor iterator' (12 x 0x15190, Fn_0074c7b0) */
        const rx_addr e = A + 0x1070 + 0x15190u * i;
        SU32(e, 0x15180, 0); SF32(e, 0x15184, 0.0f); SF32(e, 0x15188, 0.0f); SU32(e, 0x1518c, 0);
    }
    SU32(A, 0x890, world);
    SU32(A, 0x894, A);
    for (uint32_t i = 0; i < 0x30; i++) SU8(A + 0x2d0, i, 0);
    rx_rc_act_clear(A);   /* (w-rc-core) a record of an earlier activity at this address (its standings / race indices past 12) */
    for (uint32_t i = 0; i < 9; i++) SU8(A + 0xfef60, i, (uint8_t)"GamePlay"[i]);
    SU32(A, 0x884, 0);
    for (uint32_t i = 0; i < 0x280; i++) SU8(A + 0xbc4, i, 0);
    for (uint32_t i = 0; i < 0x80; i++) SU8(A + 0xe44, i, 0);
    for (uint32_t i = 0; i < 0x104; i++) SU8(A + 0xed4, i, 0);
    return A;
}
RX_FUNCTION(0x005562c0, rx_activity_construct)

/* ActivityAudioLogic::vf32 00581030 (ECX = logic): the cue slots +0x5c .. +0x88 = -1; the cue map's (+0x8c) cues played again: the map walked from
 * its first node to the head node (+0x18), each cue's played flag (+4) cleared (the restart of the event, RaceActivityAudioLogic's "Restart" 00582d70,
 * runs it with the map filled). Frame: push ebp; and esp, -8; sub esp, 8; push ebx / ebp / esi / edi (the iterator at +0x10: the map, the node). */
void rx_audio_logic_vf32(rx_addr l) {
    for (uint32_t o = 0x5c; o <= 0x88; o += 4) SU32(l, o, 0xffffffffu);
#ifndef RX_ORACLE
    rx_activity_cues_restart(l);   /* w-sfx: the level's cues' played flags (activity_cues.c) */
#endif
    const rx_addr map = U32(l, 0x8c);
    if (map) {
        ACT_FRAME_EBP(L, 8, 8, 4);
        const rx_addr it = L + 0x10, head = U32(map, 0x18);
        SU32(it, 0, U32(map, 0)); SU32(it, 4, U32(head, 0));
        while (U32(it, 4) != head) {
            SU32(U32(U32(it, 4), 0x28), 4, 0);
            ACT_CALL(0, rx_fx_tree_next(it));   /* TreeIterator_Increment (ESI = the iterator) */
        }
        ACT_END(L);
    }
}
RX_FUNCTION(0x00581030, rx_audio_logic_vf32)
/* 005825a0 (ESI = logic, 0xa4 bytes): RaceActivityAudioLogic (ActivityAudioLogic 0x009429fc, then 0x00942cfc) */
rx_addr rx_race_audio_logic_construct(rx_addr l) {
    SU32(l, 0, 0x009429fc);
    SU32(l, 0x94, 0); SU8(l, 4, 0); SU32(l, 0x50, 0); SU32(l, 0x54, 0); SU32(l, 0x8c, 0); SU32(l, 0x90, 0);
    rx_audio_logic_vf32(l);
    SU32(l, 0, 0x00942cfc);
    rx_audio_logic_vf32(l);
    SU32(l, 0x98, 0);
    return l;
}
RX_FUNCTION(0x005825a0, rx_race_audio_logic_construct)

/* 00565830 (ECX = type, EAX = world; stdcall activity, audio logic (0 = a new RaceActivityAudioLogic); RET 8): MXActivityRaceStub:
 * the Activity, vtable 0x00943454, the three scenarios (RaceIntroScenario +0xff090, RaceFinishScenario +0x114260,
 * RaceResultsScenario +0x129430, each pointing back at the activity), laps (+0xff070: 1 for type 8, else the default 005560f0 for
 * types 6 .. 0x31, else 3), the lap / pace settings */
rx_addr rx_race_stub_construct(rx_addr A, uint32_t type, rx_addr world, rx_addr audio) {
    ACT_CALL(1, rx_activity_construct(A, type, world));
    SU32(A, 0, 0x00943454);
    SF32(A, 0x114234, 0.0f); SF32(A, 0x114238, 0.0f);
    SU32(A, 0x114230, 0); SU32(A, 0x11423c, 0); SU32(A, 0xff0a4, 0);
    SU32(A, 0xff090, 0x00943394);
    SU32(A, 0x114244, 0xf);
    SU32(A, 0x129400, 0); SF32(A, 0x129404, 0.0f); SF32(A, 0x129408, 0.0f); SU32(A, 0x12940c, 0);
    SU32(A, 0x114274, 0);
    SU32(A, 0x129414, 0xf);
    SU32(A, 0x114260, 0x009433d4);
    SU32(A, 0x13e5d0, 0); SF32(A, 0x13e5d4, 0.0f); SF32(A, 0x13e5d8, 0.0f); SU32(A, 0x13e5dc, 0);
    SU32(A, 0x129444, 0);
    SU32(A, 0x13e5e4, 0xf);
    SU32(A, 0x129430, 0x00943414);
    SF32(A, 0xff078, 0.0f);
    SU32(A, 0xff07c, 100);
    SU32(A, 0xff080, 5);
    uint32_t laps;
    if (U32(A, 0x238) == 8) laps = 1;
    else if (type - 6u <= 0x2bu) laps = (type == 6 || type == 0x21 || type == 0x30 || type == 0x31) ? 1 : 3;   /* 005560f0 */
    else laps = 3;
    SU32(A, 0xff070, laps);
    SU32(A, 0xff074, U32(0, 0x009d78dc));   /* 50 (.data) */
    SU32(A, 0xff08c, 0);
    SU32(A, 0x2c8, 1);
    SU32(A, 0x22c, 1);
    if (audio) SU32(A, 0xfd8, audio);
    else {
        rx_addr l = rx_reflex_aligned_alloc(0xa4);
        SU32(A, 0xfd8, l ? rx_race_audio_logic_construct(l) : 0);
    }
    SU32(A, 0x13e600, 0); SU32(A, 0x13e644, 0); SU32(A, 0x13e648, 0);
    SU32(A, 0xff0a4, A); SU32(A, 0x114274, A); SU32(A, 0x129444, A);
    SF32(A, 0x13e604, 0.0f);
    return A;
}
RX_FUNCTION(0x00565830, rx_race_stub_construct)

/* 0055ef80 (stdcall manager, type; RET 8): the factory of the activity classes (the race types: MXActivityRace 0x13e660, vtable
 * 0x00943554); the activity appended to the manager's array */
rx_addr rx_activity_create(rx_addr mgr, uint32_t type) {
    ACT_FRAME(L, 0, 5);
    rx_addr a = 0;
    switch (type) {
    case 0: case 1: case 2: case 3: case 4: case 7: case 0x11: case 0x12: case 0x13: {
        rx_addr p = rx_reflex_aligned_alloc(0x13e660);
        if (p) { ACT_CALL(2, rx_race_stub_construct(p, type, U32(mgr, 0), 0)); SU32(p, 0, 0x00943554); a = p; }
        break;
    }
    case 6: {   /* 00578260: the separate MXActivityWaypointRace class. */
        rx_addr p = rx_reflex_aligned_alloc(0x13ec70);
        if (p) ACT_CALL(1, a = rx_waypoint_activity_construct(p, type, U32(mgr, 0)));
        break;
    }
    case 8: {   /* MXActivityFreeRide (the front end's world): 0055fdb0 (EAX = world, ECX = 8; stdcall the activity) */
        rx_addr p = rx_reflex_aligned_alloc(0x114530);
        if (p) { ACT_CALL(1, a = rx_free_ride_construct(p, 8, U32(mgr, 0))); }
        break;
    }
    /* the Free Ride challenges (race_challenge_activity.c): MXActivityChallenge's constructor 0057aa30 (EAX = world, ECX = type;
     * stdcall the block), then the class's vtable and fields */
    /* 0x15 BigAir, 0x18 Pathfinder, 0x19 StepUp: cut on PC (no EventTypeLocations rows, no level stages them); built as the original
     * does for an EventTypeActivation that names them (probe-verified, race_challenge_pathfinder.c) */
    case 0x15: {   /* MXActivityBigAirChallenge */
        rx_addr p = rx_reflex_aligned_alloc(0xff3c0);
        if (p) {
            ACT_CALL(1, rx_challenge_construct(p, type, U32(mgr, 0)));
            SU32(p, 0, 0x009452ec);
            SU32(p, 0xff070, 0);
            a = p;
        }
        break;
    }
    case 0x16: {   /* MXActivityHillClimbChallenge */
        rx_addr p = rx_reflex_aligned_alloc(0xff3c0);
        if (p) {
            ACT_CALL(1, rx_challenge_construct(p, type, U32(mgr, 0)));
            SU32(p, 0, 0x009451dc);
            SU32(p, 0xff070, 1);
            SF32(p, 0xff3b4, 0.0f);
            SF32(p, 0xff3b0, 0.0f);
            SU32(p, 0x2c8, 1);
            a = p;
        }
        break;
    }
    case 0x18: {   /* MXActivityPathfinderChallenge (0057b740: ESI = the block) */
        rx_addr p = rx_reflex_aligned_alloc(0xff3e0);
        if (p) ACT_CALL(0, a = rx_pathfinder_construct(p, type, U32(mgr, 0)));
        break;
    }
    case 0x19: {   /* MXActivityStepUpChallenge (0057c3e0: ESI = the block) */
        rx_addr p = rx_reflex_aligned_alloc(0xff400);
        if (p) ACT_CALL(0, a = rx_step_up_construct(p, type, U32(mgr, 0)));
        break;
    }
    case 0x1b: {   /* MXActivityTrailBlazerChallenge: its "VehicleEntered" listener (0058a7b0) */
        rx_addr p = rx_reflex_aligned_alloc(0xff3c0);
        if (p) {
            ACT_CALL(1, rx_challenge_construct(p, type, U32(mgr, 0)));
            SU32(p, 0, 0x00945624);
            SU32(p, 0xff070, 6);
            SU32(p, 0x2c8, 1);
            ACT_CALL(1, rx_trail_blazer_listen(p));
            a = p;
        }
        break;
    }
    case 0x1d: {   /* MXActivityTargetPracticeChallenge */
        rx_addr p = rx_reflex_aligned_alloc(0xff440);
        if (p) {
            ACT_CALL(1, rx_challenge_construct(p, type, U32(mgr, 0)));
            SU32(p, 0, 0x00945734);
            SU32(p, 0xff070, 8);
            SU32(p, 0xff434, 0);
            a = p;
        }
        break;
    }
    case 0x17: case 0x1a: case 0x1c: case 0x1e:   /* Jumper / Stoppie / HitDiscovery / Wheelie: no class (the activity is 0) */
        break;
    case 9: {   /* MXActivityTag (race_tag.c): 0056d550 (stack the activity; ECX = type, EAX = world; RET 4) */
        rx_addr p = rx_reflex_aligned_alloc(0xff3b0);
        if (p) ACT_CALLR(a, 1, rx_tag_construct(p, type, U32(mgr, 0)));
        break;
    }
    case 0xa: {   /* MXActivitySnake (race_snake.c): 0056b050 (stack the activity; ECX = type, EAX = world; RET 4) */
        rx_addr p = rx_reflex_aligned_alloc(0xff220);
        if (p) ACT_CALLR(a, 1, rx_snake_construct(p, type, U32(mgr, 0)));
        break;
    }
    case 0xe: {   /* MXActivityInfection (race_infection.c): 00572d00 (stack the activity; ECX = type, EAX = world; RET 4) */
        rx_addr p = rx_reflex_aligned_alloc(0xff1f0);
        if (p) ACT_CALLR(a, 1, rx_infection_construct(p, type, U32(mgr, 0)));
        break;
    }
    case 0xf: {   /* MXActivityEggHunt (race_egghunt.c): 00574730 (stack the activity; ECX = type, EAX = world; RET 4) */
        rx_addr p = rx_reflex_aligned_alloc(0x100130);
        if (p) ACT_CALLR(a, 1, rx_egghunt_construct(p, type, U32(mgr, 0)));
        break;
    }
    case 0x10: {   /* MXActivityCapture (race_capture.c): 00576290 (stack the activity; ECX = type, EAX = world; RET 4) */
        rx_addr p = rx_reflex_aligned_alloc(0x1014b0);
        if (p) ACT_CALLR(a, 1, rx_capture_construct(p, type, U32(mgr, 0)));
        break;
    }
    case 0xb: {   /* MXActivityGraffiti (race_graffiti.c): 00563d90 (stack the activity; ECX = type, EAX = world; RET 4) */
        rx_addr p = rx_reflex_aligned_alloc(0xff090);
        if (p) ACT_CALLR(a, 1, rx_graffiti_construct(p, type, U32(mgr, 0)));
        break;
    }
    case 0xc: {   /* MXActivitySectionRace (race_section.c): 0056a210 (EDI = the activity; stdcall type, world) */
        rx_addr p = rx_reflex_aligned_alloc(0x13e680);
        if (p) ACT_CALLR(a, 2, rx_section_race_construct(p, 0xc, U32(mgr, 0)));
        break;
    }
    case 0xd: {   /* MXActivityPokerRace (race_poker.c): 00571000 (EDI = the activity; stdcall type, world) */
        rx_addr p = rx_reflex_aligned_alloc(0x13e8e0);
        if (p) ACT_CALLR(a, 2, rx_poker_race_construct(p, 0xd, U32(mgr, 0)));
        break;
    }
    case 5:
    case 0x14: {   /* MXActivityFreestyle (0x13e610 bytes): 00561910 (ESI = the activity, ECX = type, EAX = world) */
        rx_addr p = rx_reflex_aligned_alloc(0x13e610);
        if (p) ACT_CALL(0, a = rx_freestyle_construct(p, type, U32(mgr, 0)));
        break;
    }
    case 0x20: {   /* MXActivityFreestyleChampIntro: the stub 00562280 (ECX = 0x20, EAX = world; stdcall the activity), vtable 0x9443cc */
        rx_addr p = rx_reflex_aligned_alloc(0x13e600);
        if (p) {
            ACT_CALL(1, rx_freestyle_stub_construct(p, 0x20, U32(mgr, 0)));
            SU32(p, 0, 0x009443cc);
            a = p;
        }
        break;
    }
    case 0x3a:
    case 0x3b:
    case 0x3c: {   /* MXActivityFreestyleChampPractice (0x3a, vtable 0x9444cc) / ...ChampMainEvent (0x3b, 0x3c: 0x9445cc): the stub with the
                    * type (0x3a for the practice), then +0x2c8 = 0, +0x22c = 1 */
        rx_addr p = rx_reflex_aligned_alloc(0x13e600);
        if (p) {
            ACT_CALL(1, rx_freestyle_stub_construct(p, type, U32(mgr, 0)));
            SU32(p, 0, type == 0x3a ? 0x009444cc : 0x009445cc);
            SU32(p, 0x2c8, 0);
            SU32(p, 0x22c, 1);
            a = p;
        }
        break;
    }
    case 0x1f: {   /* MXActivityRaceChampIntro (0x13e650 bytes): the race stub's constructor, vtable 0x00943654 */
        rx_addr p = rx_reflex_aligned_alloc(0x13e650);
        if (p) { ACT_CALL(2, rx_race_stub_construct(p, type, U32(mgr, 0), 0)); SU32(p, 0, 0x00943654); a = p; }
        break;
    }
    case 0x22: case 0x26: case 0x2a: case 0x2e: case 0x32: case 0x36: {   /* MXActivityRaceChampPractice: vtable 0x00943754, +0x22c = 0 */
        rx_addr p = rx_reflex_aligned_alloc(0x13e650);
        if (p) { ACT_CALL(2, rx_race_stub_construct(p, type, U32(mgr, 0), 0)); SU32(p, 0, 0x00943754); SU32(p, 0x22c, 0); a = p; }
        break;
    }
    case 0x23: case 0x27: case 0x2b: case 0x33: case 0x37: {   /* MXActivityRaceChampHeat: vtable 0x00943854 */
        rx_addr p = rx_reflex_aligned_alloc(0x13e650);
        if (p) { ACT_CALL(2, rx_race_stub_construct(p, type, U32(mgr, 0), 0)); SU32(p, 0, 0x00943854); a = p; }
        break;
    }
    case 0x24: case 0x25: case 0x28: case 0x2c: case 0x2f: case 0x34: case 0x35: case 0x38: {   /* MXActivityRaceChampMainEvent: vtable 0x00943954 */
        rx_addr p = rx_reflex_aligned_alloc(0x13e650);
        if (p) { ACT_CALL(2, rx_race_stub_construct(p, type, U32(mgr, 0), 0)); SU32(p, 0, 0x00943954); a = p; }
        break;
    }
    case 0x29: case 0x2d: case 0x39: {   /* MXActivityRaceChampLCQ: vtable 0x00943a54 */
        rx_addr p = rx_reflex_aligned_alloc(0x13e650);
        if (p) { ACT_CALL(2, rx_race_stub_construct(p, type, U32(mgr, 0), 0)); SU32(p, 0, 0x00943a54); a = p; }
        break;
    }
    case 0x21: {   /* MXActivityWaypointRaceChampIntro (0x13ec70 bytes): the waypoint race's constructor 00578260, vtable 0x00943f14 */
        rx_addr p = rx_reflex_aligned_alloc(0x13ec70);
        if (p) { ACT_CALL(1, (void)rx_waypoint_activity_construct(p, type, U32(mgr, 0))); SU32(p, 0, 0x00943f14); a = p; }
        break;
    }
    case 0x30: {   /* MXActivityWaypointRaceChampPractice (0x13ec90 bytes): 0057ea60 */
        rx_addr p = rx_reflex_aligned_alloc(0x13ec90);
        if (p) ACT_CALL(0, a = rx_champ_wp_practice_construct(p, type, U32(mgr, 0)));
        break;
    }
    case 0x31: {   /* MXActivityWaypointRaceChampMainEvent (0x13ec90 bytes): 0057e920 */
        rx_addr p = rx_reflex_aligned_alloc(0x13ec90);
        if (p) ACT_CALL(0, a = rx_champ_wp_main_construct(p, type, U32(mgr, 0)));
        break;
    }
    default:
        if (type <= 0x3c) { fprintf(stderr, "activity type %u\n", type); act_unreached(0x0055ef98, "an activity class other than MXActivityRace"); }
        break;
    }
    if (I32(mgr, 8) >= I32(mgr, 0xc)) act_unreached(0x0055f4f6, "the activity array grown (007cff20)");
    SU32(U32(mgr, 4), 4 * U32(mgr, 8), a);
    SU32(mgr, 8, U32(mgr, 8) + 1);
    ACT_END(L);
    return a;
}
RX_FUNCTION(0x0055ef80, rx_activity_create)

/* Activity::vf22 0056be50 (ECX = activity; mode; RET 4): +0x238 the ActivityMode */
void rx_activity_set_mode(rx_addr A, uint32_t mode) { SU32(A, 0x238, mode); }
RX_FUNCTION(0x0056be50, rx_activity_set_mode)

/* Activity::vf1 00556d90 (ECX = activity; &row ref {row, 0, table}; RET 4): the row's Duration (or 10800 with the registry's AI
 * override [0x00ae1b90]; the debug override [0x009ca268] when positive), ActivityMode (vf22), ActivityArea (+0x220), Spectator
 * (+0xfe8); the activity's name +0x14 "<type>-<area>" ("UI ..." in the front end) */
void rx_activity_vf1(rx_addr A, rx_addr ref) {
    ACT_FRAME(L, 0, 4);
    const uint32_t row = U32(ref, 0);
    const rx_addr t = U32(ref, 8);
    if (U32(0, 0x00ae1b90) != 0) SU32(A, 0x2bc, 0x4628c000u);   /* 10800 (0x977ae0) */
    else SU32(A, 0x2bc, x87_copy(act_cell(t, 0x0093dd58, row)));   /* "Duration" */
    const float dbg = F32(0, 0x009ca268);
    if (dbg > 0.0f) SF32(A, 0x2bc, dbg);
    const uint32_t mode = act_cell(t, 0x00936e30, row);              /* "ActivityMode" */
    {   /* vf22 */
        const rx_addr fn = U32(U32(A, 0), 0x58);
        void (*f)(rx_addr, uint32_t) = RX_FN(fn, void (*)(rx_addr, uint32_t));
        if (f) ACT_CALL(1, f(A, mode));
        else { uint32_t v = mode; rx_call_original(fn, RX_THISCALL, A, 1, &v); }
    }
    SU32(A, 0x220, act_cell(t, 0x0093dda0, row));                    /* "ActivityArea" */
    SU32(A, 0xfe8, act_cell(t, 0x0093df20, row));                    /* "Spectator" */
    const char* tn = host_str(U32(0, 0x009cf448 + 4 * U32(A, 0x23c)));
    if (U32(U32(A, 0x234), 0x20018) != 0) act_format(A + 0x14, 0x104, "UI %s-%d", tn, I32(A, 0x220));
    else act_format(A + 0x14, 0x104, "%s-%d", tn, I32(A, 0x220));
    ACT_END(L);
}
RX_FUNCTION(0x00556d90, rx_activity_vf1)

/* MXActivityRaceStub::vf1 00568050 (ECX = activity; &row ref; RET 4): Activity::vf1, the row's NumLaps (+0xff070; 99 with the
 * registry's AI override, the debug override [0x00af6564] when positive) */
void rx_race_stub_vf1(rx_addr A, rx_addr ref) {
    ACT_FRAME(L, 0, 2);
    ACT_CALL(1, rx_activity_vf1(A, ref));
    if (U32(0, 0x00ae1b90) != 0) { SU32(A, 0xff070, 99); ACT_END(L); return; }
    SU32(A, 0xff070, act_cell(U32(ref, 8), 0x0093dd64, U32(ref, 0)));   /* "NumLaps" */
    if (I32(0, 0x00af6564) > 0) SU32(A, 0xff070, U32(0, 0x00af6564));
    ACT_END(L);
}
RX_FUNCTION(0x00568050, rx_race_stub_vf1)

/* ======================================================================================================================== */
/* Activity::vf5 / MXActivityRaceStub::vf5: the activity area                                                               */

/* the level the event's subscenes are read from (the product's fmt reader; the original reads the level's .scene file) */
static fmt_level* event_level;
void rx_event_set_level(struct fmt_level* lv) { event_level = lv; }
struct fmt_level* rx_event_level(void) { return event_level; }

/* 00555e60 (ECX = level name, EAX = type; cdecl buffer, area): the activity's subscene name "<level>Exhibition<Type>[<area>]" */
void rx_activity_area_name(rx_addr level, uint32_t type, rx_addr buf, uint32_t area) {
    if (type >= 0x1f) {   /* a championship type stands in for the series manager's current event type (005b6a30) */
        ACT_FRAME(L, 0, 3);
        type = rx_series_current_type();
        ACT_END(L);
    }
    const char* lv = host_str(level);
    switch (type) {
    case 0: case 1: case 2: case 3: case 4: case 5: case 7: case 0x11: case 0x12: case 0x13: case 0x14: {
        const char* tn = host_str(U32(0, 0x009cf448 + 4 * type));
        if (area) act_format(buf, 0x104, "%sExhibition%s%d", lv, tn, (int)area);
        else act_format(buf, 0x104, "%sExhibition%s", lv, tn);
        return;
    }
    case 6: case 0xc: {   /* WaypointRace / SectionRace */
        const char* tn = host_str(U32(0, 0x009cf448 + 4 * type));
        if (area) act_format(buf, 0x104, "%s%s%d", lv, tn, (int)area); else act_format(buf, 0x104, "%s%s", lv, tn);
        return;
    }
    case 8: act_format(buf, 0x104, "%s%s", lv, host_str(U32(0, 0x009cf468))); return;   /* FreeRide */
    case 9: case 10: case 0xb:
        act_format(buf, 0x104, "%sMiniGame%s", lv, host_str(U32(0, 0x009cf448 + 4 * type))); return;
    case 0xe: act_format(buf, 0x104, "%sMiniGame%s", lv, host_str(U32(0, 0x009cf46c))); return;   /* (the name of type 9, "Tag") */
    case 0xd: {
        const char* tn = host_str(U32(0, 0x009cf47c));   /* PokerRace */
        if (area) act_format(buf, 0x104, "%sMiniGame%s%d", lv, tn, (int)area); else act_format(buf, 0x104, "%sMiniGame%s", lv, tn);
        return;
    }
    case 0xf: if (area) act_format(buf, 0x104, "%sMiniGameGreed%d", lv, (int)area); else act_format(buf, 0x104, "%sMiniGameGreed", lv); return;
    case 0x10: if (area) act_format(buf, 0x104, "%sMiniGameCTF%d", lv, (int)area); else act_format(buf, 0x104, "%sMiniGameCTF", lv); return;
    case 0x15: case 0x16: case 0x18: case 0x1b:
        act_format(buf, 0x104, "%sChallenge%s%c", lv, host_str(U32(0, 0x009cf448 + 4 * type)), (char)(area + 0x41)); return;
    case 0x17: case 0x19: case 0x1a: case 0x1c: case 0x1d: case 0x1e:
        act_format(buf, 0x104, "%sChallenge%s", lv, host_str(U32(0, 0x009cf448 + 4 * type))); return;
    default: return;
    }
}
RX_FUNCTION(0x00555e60, rx_activity_area_name)

/* the stream buffer of 007564f0 (00823600, ESI = obj, 0x14 bytes: vtable 0x009753c4, +4 / +8 an _aligned_malloc(0x19000, 16) block
 * (under the critical section 0x00d61704 when [0x00da67d8]), +0xc its size, +0x10 0) and its deleting destructor (00823660 /
 * 00823680: the block freed, vtable 0x0097539c) */
#ifndef RX_ORACLE
static rx_addr stream_buffer_construct(rx_addr o) {
    SU32(o, 0, 0x009753c4);
    rx_addr b = crt_aligned_malloc(0x19000, 0x10);
    SU32(o, 4, b); SU32(o, 8, b); SU32(o, 0xc, 0x19000); SU32(o, 0x10, 0);
    return o;
}
static void stream_buffer_delete(rx_addr o) {
    SU32(o, 0, 0x009753c4);
    crt_aligned_free(U32(o, 4));
    SU32(o, 0, 0x0097539c);
    rx_reflex_aligned_free(o);
}
#endif

/* 007564f0 (ECX = the scene file path; stdcall config, subscene name, scene slot (1 = the event's scene); RET 0xc): a subscene of the
 * level loaded into the scene of the slot ([0x00a95844] + 0x1c + 4 * slot) when the level config lists it (+0x830: names of 0x104,
 * count +0x2090). The original opens the level's .scene file (a buffered CRT file stream, the subscene's offset from the config's hash
 * +0x396c) and reads it with an XMLReader; the product reads the same subscene with fmt_scene (rx_scene_load_subscene) and keeps the
 * game-heap side of the original (the stream buffer, the reader). The oracle runs the original: its file I/O is not core. */
void rx_scene_request_subscene(rx_addr path, rx_addr config, rx_addr name, uint32_t slot) {
#ifdef RX_ORACLE
    uint32_t a[3] = {config, name, slot};
    rx_call_original(0x007564f0, RX_THISCALL, path, 3, a);
#else
    (void)path;
    rx_addr sb = rx_reflex_aligned_alloc(0x14);
    if (sb) stream_buffer_construct(sb);
    rx_addr rd = rx_reflex_aligned_alloc(0x460);
    if (rd) rx_xml_reader_construct(rd);
    for (uint32_t i = 0; i < U32(config, 0x2090); i++) {
        if (crt_stricmp(config + 0x830 + 0x104 * i, name) != 0) continue;
        if (!event_level) act_unreached(0x00756618, "a subscene request without the event's level (rx_event_set_level)");
        fmt_scene* sc = fmt_level_scene(event_level);
        const char* n = host_str(name);
        const int sub = sc ? fmt_scene_find_subscene(sc, n) : -1;
        if (sub < 0) act_unreached(0x00756663, "a listed subscene missing from the level's scene");
        rx_scene_load_subscene(U32(U32(0, 0x00a95844), 0x1c + 4 * slot), sc, sub, n);
        break;
    }
    if (rd) { rx_xml_reader_destroy(rd); rx_reflex_aligned_free(rd); }
    if (sb) stream_buffer_delete(sb);
#endif
}

/* 00766250 (ECX = world; stdcall subscene name; RET 4): the scene file's path "<dir><level>.dx9.scene" (MXGame + 0x3ac4 / + 0x39c0;
 * the front end's +0x138 / +0x34) and the subscene requested into the event's scene (slot 1; the front end's 0) */
void rx_mxworld_load_subscene(rx_addr w, rx_addr name) {
    ACT_FRAME_EBP(L, 8, 0x114, 3);
    const rx_addr game = U32(0, 0x00a946b8), path = L + 0x10;
    for (uint32_t i = 0; i < 0x104; i++) SU8(path, i, 0);
    uint32_t slot; rx_addr config;
    if (U32(w, 0x20018) != 0) {
        act_format(path, 0x104, "%s%s.%s.scene", host_str(game + 0x138), host_str(game + 0x34), "dx9");
        slot = 0; config = game + 0x34;
    } else {
        act_format(path, 0x104, "%s%s.%s.scene", host_str(game + 0x3ac4), host_str(game + 0x39c0), "dx9");
        slot = 1; config = game + 0x39c0;
    }
    ACT_CALL(3, rx_scene_request_subscene(path, config, name, slot));
    ACT_END(L);
}
RX_FUNCTION(0x00766250, rx_mxworld_load_subscene)

/* Activity::vf5 00556c30 (ECX = activity): the area's subscene name (+0x118); for an active activity (mode != 0) of a race / intro
 * type, once (+0xec8): its subscene loaded (00766250; on the main thread the original hands that to the loader thread through the
 * engine's "LoadSubsceneCallback" job and waits: the same call), the physics world finalized (00769030: the forest's trunks, then the
 * broadphase grids) */
void rx_activity_vf5(rx_addr A) {
    ACT_FRAME_EBP(L, 8, 0x114, 3);
    const rx_addr w = U32(A, 0x234);
    ACT_CALL(2, rx_activity_area_name(w + 0x20030, U32(A, 0x23c), A + 0x118, U32(A, 0x220)));
    const uint32_t type = U32(A, 0x23c);
    if (U32(A, 0x238) != 0 && (type < 0x1f || type <= 0x21) && U32(A, 0xec8) == 0) {
        ACT_CALL(1, rx_mxworld_load_subscene(w, A + 0x118));
        const rx_addr pw = U32(U32(A, 0x234), 0x20134);
        ACT_CALL(1, rx_physics_world_install_forest(pw));   /* (00769030's first step, FUN_00772940) */
        ACT_CALL(1, rx_physics_world_finalize(pw, 0));
        SU32(A, 0xec8, 1);
    }
    ACT_END(L);
}
RX_FUNCTION(0x00556c30, rx_activity_vf5)

/* the "%s" / strncpy copies of 0055a5d0: the string and its terminator */
static void put_str(rx_addr d, const char* s) { uint32_t i = 0; do SU8(d, i, (uint8_t)s[i]); while (s[i++]); }
/* 0055a5d0 (EAX = buffer, ECX = type; stdcall activity; RET 4): the track name of the activity's type ("National", "National<area>") */
void rx_activity_track_name(rx_addr buf, uint32_t type, rx_addr A) {
    char t[32];
    switch (type) {
    case 0: case 0x22: case 0x23: case 0x24: case 0x25:
        if (U32(A, 0x220)) { snprintf(t, sizeof t, "National%d", I32(A, 0x220)); put_str(buf, t); } else put_str(buf, "National");
        return;
    case 1: case 0x26: case 0x27: case 0x28: case 0x29: put_str(buf, "Supercross"); return;
    case 2: case 0x2e: case 0x2f:
        if (U32(A, 0x220)) { snprintf(t, sizeof t, "Opencross%d", I32(A, 0x220)); put_str(buf, t); } else put_str(buf, "Opencross");
        return;
    case 3: case 0x32: case 0x33: case 0x34: case 0x35: put_str(buf, "Supermoto"); return;
    case 4: case 0x36: case 0x37: case 0x38: case 0x39: put_str(buf, "Minimoto"); return;
    case 5: case 0x14: case 0x3a: case 0x3b: case 0x3c: put_str(buf, "Freestyle"); return;
    case 7: case 0x2a: case 0x2b: case 0x2c: case 0x2d: put_str(buf, "Endurocross"); return;
    case 8: put_str(buf, "FreeRide"); return;
    case 0xc: put_str(buf, "SectionRace"); return;
    case 0x11: put_str(buf, "Terracross"); return;
    case 0x12: put_str(buf, "UTVMoto"); return;
    case 0x13: put_str(buf, "Powercross"); return;
    default: return;
    }
}
RX_FUNCTION(0x0055a5d0, rx_activity_track_name)

/* 007677e0 (ESI = world; stdcall track name, audio block or 0; RET 8): the world's racing line of the track: the previous one
 * released (00767850), the track entity named so in the world's list (+0x21df4, name +0x180, next +0x90; 007677a0) with a path
 * (+0x98) gets a new racing line (0x132a0 bytes, 006a2980 / 006a2c10 / 006a2c80) */
void rx_mxworld_set_track(rx_addr w, rx_addr name, rx_addr audio) {
    (void)audio;
    if (U32(w, 0x21df0) != 0) act_unreached(0x00767850, "a racing line released (00767850)");
    rx_addr t = U32(w, 0x21df4);
    while (t && crt_stricmp(name, t + 0x180) != 0) t = U32(t, 0x90);
    if (t && U32(t, 0x98) != 0) act_unreached(0x00767802, "the track's racing line (006a2980)");
}
RX_FUNCTION(0x007677e0, rx_mxworld_set_track)

static float act_atof(rx_addr s) { return (float)strtod(host_str(s), NULL); }

/* 00556720 (ECX = level name; stdcall activity, pacing kind, area, scene-from-file flag; RET 0x10): the AI lap pacing of the activity
 * from the level's "<level>-lappacing" BXML asset: under the node "<Type><area>" of the root, per vehicle class node (005554e0:
 * 0 .. 7, 8 unknown) and engine size node (its name from the 15th character: "50cc" / "125cc" / "250cc" / "500cc", 4 unknown), the
 * NormalTime (+0xe44 + 4 * (size + 4 * class)) and the Pro / Hard / Normal / Easy times (+0xbc4 + 0x14 * (size + 4 * class)). */
void rx_activity_lap_pacing(rx_addr level, rx_addr A, uint32_t kind, uint32_t area, uint32_t from_file) {
    (void)from_file;
    if(fmt_modtrack_racing())return; /* Donor lap-pacing targets are unrelated to this course. */
    ACT_FRAME(L, 0xcdc, 4);
    const rx_addr rd_local = L + 0x148, aname = L + 0x5a8, ttl = L + 0xbd8, root_name = L + 0x7b8, node_name = L + 0x6b0;
    const rx_addr cls_name = L + 0xad0, size_name = L + 0x9c8, tbuf = L + 0x8c0, abuf = L + 0x28;
    SU32(L, 0x24, A);
    rx_xml_reader_construct(rd_local);
    SU8(aname, 0, 0);
    for (uint32_t i = 1; i < 0x104; i++) SU8(aname, i, 0);
    SU32(L, 0x10, 0);
    rx_addr r = 0;
    if (U8(level, 0) == 0) act_unreached(0x005567d9, "the lap pacing from temp-lappacing.bxml (no level)");
    act_format(aname, 0x400, "%s-lappacing", host_str(level));   /* sprintf */
    const rx_addr db = U32(U32(0, 0x00a95844), 8);
    rx_addr h;
    ACT_CALLR(h, 4, rx_database_vf_handle(db, aname, 0x6c6d7862 /* 'bxml' */, 0, 0));
    SU32(L, 0x10, h);
    if (h == 0) goto done;
    SU32(h, 0x4c, 1);
    if (U32(h, 0x44)) r = U32(h, 0x44) - 0x460;
    if (!r) act_unreached(0x005567c8, "a lap pacing asset without its reader");
    act_format(ttl, 0x104, "%s%d", host_str(U32(0, 0x009cf448 + 4 * kind)), (int)area);
    SU8(root_name, 0, 0);
    for (uint32_t i = 1; i < 0x104; i++) SU8(root_name, i, 0);
    {   /* the XMLReader virtuals (the asset's reader: vtable 0x009753dc) */
        rx_addr root;
        ACT_CALLR(root, 0, rx_xml_vf_root(r));
        ACT_CALL(3, rx_xml_vf_node_name(r, root, root_name, 0x104));
        rx_addr n;
        ACT_CALLR(n, 3, rx_xml_vf_first(r, root, 0, L + 0x14));
        while (n) {
            ACT_CALL(3, rx_xml_vf_node_name(r, n, node_name, 0x104));
            if (crt_strcmp(ttl, node_name) == 0) break;
            ACT_CALLR(n, 1, rx_xml_vf_next(r, U32(L, 0x14)));
        }
        if (n) {
            rx_addr c;
            ACT_CALLR(c, 3, rx_xml_vf_first(r, n, 0, L + 0x1c));
            while (c) {
                ACT_CALL(3, rx_xml_vf_node_name(r, c, cls_name, 0x104));
                uint32_t cls = 8;   /* 005554e0 */
                for (uint32_t k = 0; k < 8; k++) if (crt_stricmp(cls_name, U32(0, 0x009cf400 + 4 * k)) == 0) { cls = k; break; }
                SU32(L, 0x20, cls);
                rx_addr e;
                ACT_CALLR(e, 3, rx_xml_vf_first(r, c, 0, L + 0x18));
                while (e) {
                    SU8(node_name, 0, 0);
                    for (uint32_t i = 1; i < 0x104; i++) SU8(node_name, i, 0);
                    ACT_CALL(3, rx_xml_vf_node_name(r, e, node_name, 0x104));
                    for (uint32_t i = 0;; i++) { uint8_t ch = U8(node_name + 0xe, i); SU8(size_name, i, ch); if (!ch) break; }
                    uint32_t size = 4;
                    for (uint32_t k = 0; k < 4; k++) if (crt_stricmp(size_name, U32(0, 0x009cf420 + 4 * k)) == 0) { size = k; break; }
                    rx_addr t;
                    ACT_CALLR(t, 2, rx_xml_vf_child(r, e, 0x00941b5c));   /* "NormalTime" */
                    ACT_CALL(3, rx_xml_vf_node_string(r, t, tbuf, 0x104));
                    SF32(A, 4 * (size + 4 * cls + 0x391), act_atof(tbuf));
                    const rx_addr dst = A + 0xbc4 + 0x14 * (size + 4 * cls);
                    for (uint32_t k = 0; k < 4; k++) {   /* "Pro", "Hard", "Normal", "Easy" (0x009cf430) */
                        ACT_CALLR(t, 2, rx_xml_vf_child(r, e, U32(0, 0x009cf430 + 4 * k)));
                        ACT_CALL(3, rx_xml_vf_node_string(r, t, abuf, 0x104));
                        SF32(dst, 4 * k, act_atof(abuf));
                    }
                    ACT_CALLR(e, 1, rx_xml_vf_next(r, U32(L, 0x18)));
                }
                ACT_CALL(1, rx_xml_vf_release(r, U32(L, 0x18)));
                ACT_CALLR(c, 1, rx_xml_vf_next(r, U32(L, 0x1c)));
            }
            ACT_CALL(1, rx_xml_vf_release(r, U32(L, 0x1c)));
        }
        ACT_CALL(1, rx_xml_vf_release(r, U32(L, 0x14)));
    }
    if (U32(L, 0x10)) SU32(U32(L, 0x10), 0x4c, 0);
done:
    rx_xml_reader_destroy(rd_local);
    ACT_END(L);
}
RX_FUNCTION(0x00556720, rx_activity_lap_pacing)

/* MXActivityRaceStub::vf5 00565a30 (ECX = activity): Activity::vf5, the audio logic of the type (00536ef0 on the +0x890 block: the
 * audio mode of the type, the area; audio, not core), the lap pacing (00556720: the pacing kind of the type); for an active activity
 * of a race / intro type its track name (+0xed4, 0055a5d0) and racing line (007677e0); "Activity Basic Essentials Loaded" raised on
 * the activity channel [0x00a93f74] with the activity */
void rx_race_stub_vf5(rx_addr A) {
    ACT_FRAME_EBP(L, 8, 8, 4);
    ACT_CALL(0, rx_activity_vf5(A));
    uint32_t type = U32(A, 0x23c);
    if (type == 0x1f) act_unreached(0x00565a4f, "the championship intro (the series manager)");
    int audio = -1, pace = -1;
    switch (type) {
    case 0: case 0x22: case 0x23: case 0x24: case 0x25: audio = 0; pace = 0; break;
    case 1: case 0x26: case 0x27: case 0x28: case 0x29: audio = 1; pace = 1; break;
    case 2: case 0x2e: case 0x2f: audio = 3; pace = 2; break;
    case 3: case 0x32: case 0x33: case 0x34: case 0x35: audio = 2; pace = 3; break;
    case 4: case 0x36: case 0x37: case 0x38: case 0x39: audio = 5; pace = 4; break;
    case 7: case 0x2a: case 0x2b: case 0x2c: case 0x2d: audio = 9; pace = 7; break;
    case 0x11: audio = 0xa; pace = 0x11; break;
    case 0x12: audio = 0xb; pace = 0x12; break;
    case 0x13: audio = 0xc; pace = 0x13; break;
    case 0xc: {   /* the section race: the AI track asset kind of the level (0056aed0), loaded as the audio mode; no lap pacing */
        const rx_addr level = U32(A, 0x234) + 0x20030;
        uint32_t kind;
        ACT_CALLR(kind, 0, rx_section_track_audio_kind(level, U32(A, 0x220)));
        ACT_CALL(3, rx_track_load(U32(A, 0x234) + 0x20030, A + 0x890, kind, U32(A, 0x220)));
        break;
    }
    case 0xd: {   /* the poker race: its AI track asset kind (00571c20), loaded as the audio mode; no lap pacing */
        const rx_addr level = U32(A, 0x234) + 0x20030;
        uint32_t kind;
        ACT_CALLR(kind, 0, rx_poker_track_audio_kind(level, U32(A, 0x220)));
        ACT_CALL(3, rx_track_load(U32(A, 0x234) + 0x20030, A + 0x890, kind, U32(A, 0x220)));
        break;
    }
    default: break;
    }
    if (audio >= 0) {
        const rx_addr w0 = U32(A, 0x234);
        ACT_CALL(3, rx_track_load(w0 + 0x20030, A + 0x890, (uint32_t)audio, U32(A, 0x220)));
        const rx_addr w = U32(A, 0x234);
        ACT_CALL(4, rx_activity_lap_pacing(w + 0x20030, A, (uint32_t)pace, U32(A, 0x220), U32(w, 0x10)));
    }
    const uint32_t mode = U32(A, 0x238);
    type = U32(A, 0x23c);
    if (mode != 2 && mode != 0 && (type < 0x1f || type <= 0x21)) {
        ACT_CALL(1, rx_activity_track_name(A + 0xed4, type, A));
        ACT_CALL(2, rx_mxworld_set_track(U32(A, 0x234), A + 0xed4, U32(A, 0xbb0) ? A + 0x890 : 0));
    }
    const uint32_t payload[1] = {A};
    race_raise_event(U32(0, 0x00a93f74), 0x00936038 /* "Activity Basic Essentials Loaded" */, 0x009cbd24, payload, 1);
    ACT_END(L);
}
RX_FUNCTION(0x00565a30, rx_race_stub_vf5)

/* ======================================================================================================================== */
/* the activity area                                                                                                         */

/* a column's cell vector entry (+0x40: begin +0xc / end +0x10), read under the table lock 0x00ae1c44 (one thread) */
static uint32_t col_cell(rx_addr table, rx_addr name, uint32_t row, uint32_t site) {
    const rx_addr cells = U32(act_column(table, name), 0x40);
    if (row >= (U32(cells, 0x10) - U32(cells, 0xc)) >> 2) act_unreached(site, "row index (_invalid_parameter_noinfo)");
    return U32(U32(cells, 0xc), 4 * row);
}
static uint32_t table_rows(rx_addr t) {
    if (((U32(t, 0x14) - U32(t, 0x10)) >> 2) == 0) return 0;
    return rx_tbl_column_rows(U32(U32(t, 0x10), 0));
}

/* 007669f0 (stdcall world, activity table; RET 8) -> changed: when the table changed since the world's last pass (+0x68 vs
 * world + 0x2013c): every activity whose type has no row is ended (vf17; vf3 and vf6 (0) when started, +0xec4) and removed (the
 * array's last moved into its slot) and deleted (vf0 (1)); every row without its activity (type and area) gets a new one (0055ef80,
 * vf1 (the row), vf5, vf2 ("PostEventOverlay"); a mode-2 activity (+0x238) is marked +0x10 = 2) */
uint32_t rx_activity_area_update(rx_addr w, rx_addr t) {
    ACT_FRAME_EBP(L, 8, 0x1c, 3);                                      /* push ebp; and esp, -8; sub esp, 0x1c; push ebx / esi / edi */
    if (U32(t, 0x68) == U32(w, 0x2013c)) { ACT_END(L); return 0; }
    for (int32_t i = I32(w, 0x21da8) - 1; i >= 0; i--) {
        const rx_addr a = U32(U32(w, 0x21da4), 4 * (uint32_t)i);
        uint32_t r = 0;
        const uint32_t n = table_rows(t);
        for (; r < n; r++) if (col_cell(t, 0x0093dd90, r, 0x00766a80) == U32(a, 0x23c)) break;   /* "ActivityType" */
        if (r != table_rows(t)) continue;
        vcall0(a, 0x44);
        if (U32(a, 0xec4) != 0) { vcall0(a, 0xc); vcall1(a, 0x18, 0); }
        for (int32_t k = 0; k < I32(w, 0x21da8); k++) {
            const rx_addr arr = U32(w, 0x21da4);
            if (U32(arr, 4 * (uint32_t)k) == a) {
                SU32(arr, 4 * (uint32_t)k, U32(arr, 4 * (U32(w, 0x21da8) - 1)));
                SU32(w, 0x21da8, U32(w, 0x21da8) - 1);
                break;
            }
        }
        vcall1(a, 0, 1);
    }
    for (uint32_t r = 0; r < table_rows(t); r++) {
        SU32(L, 0x24, 0); SU32(L, 0x20, r); SU32(L, 0x28, t);              /* the row reference {row, 0, table} at L + 0x20 */
        const uint32_t area = col_cell(t, 0x0093dda0, r, 0x00766c46);   /* "ActivityArea" */
        const uint32_t type = col_cell(t, 0x0093dd90, r, 0x00766c8e);   /* "ActivityType" */
        int found = 0;
        for (int32_t k = 0; k < I32(w, 0x21da8); k++) {
            const rx_addr a = U32(U32(w, 0x21da4), 4 * (uint32_t)k);
            if (type == U32(a, 0x23c) && area == U32(a, 0x220)) { (void)col_cell(t, 0x00936e30, r, 0x00766d0f); found = 1; break; }   /* "ActivityMode" */
        }
        if (found) continue;
        rx_addr a;
        ACT_CALLR(a, 2, rx_activity_create(w + 0x21da0, type));
        vcall1(a, 4, L + 0x20);
        vcall0(a, 0x14);
        vcall1(a, 8, 0x0096dd14 /* "PostEventOverlay" */);
        if (U32(a, 0x238) == 2) SU32(a, 0x10, 2);
    }
    SU32(w, 0x2013c, U32(t, 0x68));
    ACT_END(L);
    return 1;
}
RX_FUNCTION(0x007669f0, rx_activity_area_update)

/* Game_SetupActivityArea 0075c430 (ESI = game): once the world is ready (+0x15b54): the player slots from the Player List (0075ba20),
 * the activity area of the current world (the event world + 0x5a54 with the activity table, else the front end's + 0x20c8 with its
 * table 0x00adffe0: 007669f0) and its player slots (00766ec0). (The original tests the UI manager's +0x284 and the session's online
 * state first; both paths do the same.) */
void rx_game_setup_activity_area(rx_addr g) {
    ACT_FRAME(L, 0, 0);
    const rx_addr ui = U32(g, 0x745c);
    if (ui && U32(ui, 0x284) == 0 && !(U8(0, 0x00dcf304) & 1)) act_unreached(0x0075c44c, "the session's static initialiser has not run");
    if (U32(g, 0x15b54) == 0) { ACT_END(L); return; }
    ACT_CALL(1, rx_players_sync(g));
    uint32_t view;
    ACT_CALLR(view, 0, rx_game_is_view_track(g));
    const rx_addr w = view ? U32(g, 0x5a54) : U32(g, 0x20c8);
    ACT_CALL(2, rx_activity_area_update(w, view ? ACT_ACTIVITY_TBL : 0x00adffe0));
    ACT_CALL(1, rx_world_update_players(view ? U32(g, 0x5a54) : U32(g, 0x20c8)));
    ACT_END(L);
}
RX_FUNCTION(0x0075c430, rx_game_setup_activity_area)

/* ======================================================================================================================== */
/* MXWorld::vf3 007665f0 (ECX = world; stdcall the activity table; RET 4)                                                   */

void rx_mxworld_vf3(rx_addr w, rx_addr table) {
    ACT_FRAME_EBP(L, 8, 0x14, 3);
    SU32(w, 0x20140, 0);
    const rx_addr fx = U32(0, 0x00a94264 + 4 * U32(U32(w, 0x20138), 0x2cc4));
    SU32(L, 0x10, fx);
    if (fx) {   /* the world's particle effects (fx unit; the original's constructors while unported) */
        rx_addr p = rx_reflex_aligned_alloc(0x100);
        rx_addr e = p ? ctor_reg(0x00696480, 6, p) : 0;
        SU32(w, 0x21dc4, e);
        vcall1(e, 0x18, fx);
        for (uint32_t i = 0; i < 3; i++) {
            rx_addr q = rx_reflex_aligned_alloc(0x110);
            SU32(w, 0x21dc8 + 4 * i, q ? ctor_reg(0x006963b0, 6, q) : 0);
        }
        rx_addr s = rx_reflex_aligned_alloc(0x100);
        if (s) {   /* PhysicsObjectPoofParticles (inlined) */
            SU32(s, 4, 0);
            SU32(s, 0, 0x009588ec);
            ctor_reg(0x00694110, 0, s + 0x10);
            SU32(s, 0xd0, 0x0095df34);
            SU32(s, 0xe4, 0x3f800000u);
            SU32(s, 0xc, 0);
            SU32(s, 0, 0x0095e1e8);
        }
        SU32(w, 0x21dd4, s);
        vcall1(s, 0x18, U32(L, 0x10));
    }
    const rx_addr mgr = w + 0x21da0;
    SU32(L, 0xc, mgr);
    ACT_CALL(3, rx_activity_manager_init(mgr, w, w + 0x20030));
    for (uint32_t i = 0;; i++) {
        const uint32_t ncol = (U32(table, 0x14) - U32(table, 0x10)) >> 2;
        if (ncol == 0) break;
        if (i >= rx_tbl_column_rows(U32(U32(table, 0x10), 0))) break;
        SU32(L, 0x14, i); SU32(L, 0x18, 0); SU32(L, 0x1c, table);
        /* "ActivityType": the column's cell vector (+0x40: begin +0xc, end +0x10) read under the table lock 0x00ae1c44 (one thread) */
        const rx_addr col = act_column(table, 0x0093dd90);
        const rx_addr cells = U32(col, 0x40);
        if (i >= (U32(cells, 0x10) - U32(cells, 0xc)) >> 2) act_unreached(0x007667a5, "row index (_invalid_parameter_noinfo)");
        const uint32_t type = U32(U32(cells, 0xc), 4 * i);
        rx_addr a;
        ACT_CALLR(a, 2, rx_activity_create(mgr, type));
        vcall1(a, 4, L + 0x14);
        vcall0(a, 0x14);
    }
    ACT_END(L);
}
RX_FUNCTION(0x007665f0, rx_mxworld_vf3)
