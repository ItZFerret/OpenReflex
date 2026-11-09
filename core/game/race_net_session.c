/* race_net_session.c -- the online session code of the race layer that sits on the Quazal library (QuazalNet-Z, the game's online
 * stack): the replicated player's snapshot ring and apply (006fe870 .. 00700e70), its event send, the session recorder (006f7810),
 * the reference-counted object handle (0088bcd0 / 0088bac0 / 00885050) and the selection iterators the replay waits on (005544c0 /
 * 00585b20 / 00508880). Hand transliterations of the disassembly (addresses in each comment).
 *
 * ONLINE BOUNDARY. Everything below is reached only with a Quazal session: a replicated player object (the driver's +0xef0), the
 * session object [0xd60d54] / handle [0x9df938], the replay recorder (the session block 0xda6cb0 +0x1c48, a flag the core's activity
 * code refuses to set: build_activity_pre.c), or a network race. The product has none of them (no Quazal, no sockets: the offline
 * stand-in is a null session). The Quazal library calls (the leaves named below: 00701bf0 the snapshot deque's operator[], 00884ba0
 * / 00884c90 the change marking, 008932c0 .. 00886b10 the message writer, 00703070 the checked reference, 0088bb50 / 0088b8f0 /
 * 00885310 the reference counts, 00884650 / 0088afe0 / 0088c150 / 0088c380 / 0088c1e0 / 0088bc40 the selection
 * iterators; 00508800, which moves the selection, is ported below with only its handle assignment 0088bc40 left as the boundary) are the boundary: the oracle runs the originals, the product stops naming the address if it is ever reached. The
 * game-memory writes (the replicated fields, the flags) are in these ports and are verified (race_wave_probe.cpp "net_*" cases with
 * the Quazal leaves replaced by logging stand-ins). 0088c1e0 / 0088c190 / 0088bc40 are ported by the net wave; 00508800 / 00508770 / 0053a170 (pure game-memory set walks) are ported here. */
#include "race.h"
#include "race_extern.h"
#include "race_net_session.h"
#include "build_activity_util.h"
#include "fx.h"
#include "loop_player.h"
#include "world_node.h"
#include "crt.h"
#include <string.h>

#ifdef RX_ORACLE
static void quazal(rx_addr fn, const rx_regs* in, rx_regs* out, int nargs, const uint32_t* args) {
    rx_regs o;
    rx_call_original_ex(fn, in, out != 0 ? out : &o, nargs, args);
}
#else
static void quazal(rx_addr fn, const rx_regs* in, rx_regs* out, int nargs, const uint32_t* args) {
    (void)in; (void)out; (void)nargs; (void)args;
    act_unreached(fn, "the Quazal network layer (the product has no online session)");
}
#endif
static rx_regs no_regs(void) { rx_regs r; memset(&r, 0, sizeof r); return r; }

/* fld dword; fstp dword: the bits, a signalling NaN quieted */
static uint32_t fcopy(uint32_t bits) {
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x007fffffu) != 0) bits |= 0x00400000u;
    return bits;
}

/* ---- the replicated player's snapshot ring ------------------------------------------------------------------------------------- */

/* 00700a70 (EAX = the network object N): the local values that the object points at (the pointers at +0xe8 .. +0x124 which are set:
 * floats copied through the x87, the integers plain) written into each of the four snapshot records [N + 0x128 + 4k] (+0x7c ..
 * +0xbc). Frame: push esi; push edi. */
void rx_net_snapshots_store(rx_addr net) {
    ACT_FRAME(F, 0, 2);
    static const struct { uint16_t src, dst; uint8_t flt; } map[16] = {
        {0xe8, 0x7c, 1}, {0xec, 0x80, 1}, {0xf0, 0x84, 1}, {0xf8, 0x88, 1}, {0xf4, 0x8c, 1}, {0xfc, 0x90, 0}, {0x100, 0x94, 0}, {0x104, 0x98, 1},
        {0x108, 0x9c, 1}, {0x10c, 0xa0, 1}, {0x110, 0xa4, 1}, {0x114, 0xa8, 1}, {0x118, 0xac, 1}, {0x11c, 0xb0, 0}, {0x120, 0xb8, 1}, {0x124, 0xbc, 0}};
    for (uint32_t k = 0; k < 4; k++) {
        for (uint32_t i = 0; i < 16; i++) {
            const rx_addr value = U32(net, map[i].src);
            if (value == 0) continue;
            const rx_addr snapshot = U32(net, 0x128 + 4 * k);
            SU32(snapshot, map[i].dst, map[i].flt ? fcopy(U32(value, 0)) : U32(value, 0));
        }
    }
    ACT_END(F);
}
RX_FUNCTION(0x00700a70, rx_net_snapshots_store)

/* 00700e70 (ESI = a holder whose first dword is the network object N) -> EAX = N: the ring's element 3 (the snapshot deque's
 * operator[], 00701bf0 with EAX = N, stack 3 -> an iterator whose first dword is the record) copied into N: +0x9c .. +0xdc from the
 * record's +0x7c .. +0xbc (floats through the x87, +0xb0 / +0xb4 / +0xb8 .. as listed). Frame: push ecx. */
rx_addr rx_net_snapshot_load(rx_addr holder) {
    ACT_FRAME(F, 0, 1);
    const rx_addr net = U32(holder, 0);
    rx_regs in = no_regs(), out = no_regs();
    in.r[0] = net;
    const uint32_t a[1] = {3};
    quazal(0x00701bf0, &in, &out, 1, a);                            /* the deque's operator[] -> EAX = the iterator */
    const rx_addr rec = U32(out.r[0], 0);
    const rx_addr n = U32(holder, 0);
    SU32(n, 0x9c, fcopy(U32(rec, 0x7c)));
    SU32(n, 0xa0, fcopy(U32(rec, 0x80)));
    SU32(n, 0xa4, fcopy(U32(rec, 0x84)));
    SU32(n, 0xa8, fcopy(U32(rec, 0x88)));
    SU32(n, 0xac, fcopy(U32(rec, 0x8c)));
    SU32(n, 0xb0, U32(rec, 0x90));
    SU8(n, 0xd4, U8(rec, 0xb4));
    SU32(n, 0xb4, U32(rec, 0x94));
    SU32(n, 0xb8, fcopy(U32(rec, 0x98)));
    SU32(n, 0xbc, fcopy(U32(rec, 0x9c)));
    SU32(n, 0xc0, fcopy(U32(rec, 0xa0)));
    SU32(n, 0xc4, fcopy(U32(rec, 0xa4)));
    SU32(n, 0xc8, fcopy(U32(rec, 0xa8)));
    SU32(n, 0xcc, fcopy(U32(rec, 0xac)));
    SU32(n, 0xd0, U32(rec, 0xb0));
    SU32(n, 0xd8, fcopy(U32(rec, 0xb8)));
    SU32(n, 0xdc, U32(rec, 0xbc));
    ACT_END(F);
    return U32(holder, 0);
}
RX_FUNCTION(0x00700e70, rx_net_snapshot_load)

/* 006fe870 (EDI = the player's network object O): the ring ([O + 0x1d4] -> the holder -> N) stored (00700a70 while N exists) and
 * loaded (00700e70), then N's replicated fields copied into O: +0xa8 / +0x80 / +0xb4 / +0xa4 / +0x88 / +0x9c(byte) / +0xac /
 * +0xa0 / +0x94 / +0x84 / +0x90 / +0xb8 / +0x98 / +0x9e(word) / +0x8c(byte) / +0xb0 / +0xbc(byte) from N's +0x9c .. +0xdc, +0xbd = 1, and
 * the change stamp: the clock hook 0x9df898 (0: none; 64 bit EDX:EAX) handed to 00884ba0 (ECX = O; stack O + 0x80, &stamp; RET 8).
 * Frame: push ebp; and esp, -8; sub 0xc; push esi (the stamp at +8). */
void rx_net_player_apply(rx_addr o) {
    ACT_FRAME_EBP(F, 8, 0xc, 1);
    if (U32(U32(o, 0x1d4), 0) != 0) ACT_CALL(0, rx_net_snapshots_store(U32(U32(o, 0x1d4), 0)));
    rx_addr n;
    ACT_CALLR(n, 0, rx_net_snapshot_load(U32(o, 0x1d4)));
    SU32(o, 0xa8, fcopy(U32(n, 0x9c)));
    SU32(o, 0x80, fcopy(U32(n, 0xa0)));
    SU32(o, 0xb4, fcopy(U32(n, 0xa4)));
    SU32(o, 0xa4, fcopy(U32(n, 0xa8)));
    SU32(o, 0x88, fcopy(U32(n, 0xac)));
    SU8(o, 0x9c, U8(n, 0xb0));
    SU32(o, 0xac, U32(n, 0xb4));
    SU32(o, 0xa0, fcopy(U32(n, 0xb8)));
    SU32(o, 0x94, fcopy(U32(n, 0xbc)));
    SU32(o, 0x84, fcopy(U32(n, 0xc0)));
    SU32(o, 0x90, fcopy(U32(n, 0xc4)));
    SU32(o, 0xb8, fcopy(U32(n, 0xc8)));
    SU32(o, 0x98, fcopy(U32(n, 0xcc)));
    SU16(o, 0x9e, U16(n, 0xd0));
    SU8(o, 0x8c, U8(n, 0xd4));
    SU32(o, 0xb0, fcopy(U32(n, 0xd8)));
    SU8(o, 0xbc, U8(n, 0xdc));
    SU8(o, 0xbd, 1);
    const rx_addr stamp = F + 8;
    const rx_addr hook = U32(0, 0x9df898);
    SU32(stamp, 0, 0);
    SU32(stamp, 4, 0);
    if (hook != 0) {
        rx_regs in = no_regs(), out = no_regs();
        quazal(hook, &in, &out, 0, 0);                               /* the clock (EDX:EAX) */
        SU32(stamp, 0, out.r[0]);
        SU32(stamp, 4, out.r[2]);
    }
    rx_regs in = no_regs();
    in.r[1] = o;
    const uint32_t a[2] = {o + 0x80, stamp};
    quazal(0x00884ba0, &in, 0, 2, a);                                /* the change marking (Quazal) */
    ACT_END(F);
}
RX_FUNCTION(0x006fe870, rx_net_player_apply)

/* 006fe9c0 (EAX = O, CL = flag): the four snapshot records of N ([[O + 0x1d4]] + 0x128 .. + 0x134) get the flag byte at +0xb4 while N
 * exists, then 006fe870 (EDI = O). Frame: push edi. */
void rx_net_player_set_flag(rx_addr o, uint32_t flag) {
    ACT_FRAME(F, 0, 1);
    const rx_addr n = U32(U32(o, 0x1d4), 0);
    if (n != 0) {
        for (uint32_t k = 0; k < 4; k++) SU8(U32(n, 0x128 + 4 * k), 0xb4, flag);
    }
    ACT_CALL(0, rx_net_player_apply(o));
    ACT_END(F);
}
RX_FUNCTION(0x006fe9c0, rx_net_player_set_flag)

/* 006ff8d0 (EDI = O; stdcall a, b, c; RET 0xc): the "SendEvent" message: a string object (008932c0: ECX = a temporary at F - 4, stack
 * "SendEvent"; RET 4) whose event id (00897d40 -> AX) is kept in the local at +4, the message buffer of O (008847b0: ECX = O, stack
 * &id; RET 4) and a, b, c appended as 4-byte values (0088c5e0: ECX = buffer, stack &value, 4, 1; RET 0xc), sent (00886b10: ECX = O,
 * stack 1, buffer; RET 8). All Quazal; the registers pass from call to call as the original has them. Frame: push ecx (the id
 * local); push esi; the temporary string object is a third push the original drops with `add esp, 4`. */
void rx_net_send_event(rx_addr o, uint32_t a, uint32_t b, uint32_t c) {
    const rx_addr args = rx_sp() + 4;
    ACT_FRAME(F, 0, 2);
    (void)a; (void)b; (void)c;
    rx_regs r = no_regs(), out = no_regs();
    r.r[1] = F - 4;
    const uint32_t name[1] = {0x00963be4 /* "SendEvent" */};
    quazal(0x008932c0, &r, &out, 1, name);
    r = out;
    quazal(0x00897d40, &r, &out, 0, 0);
    SU32(F, 4, out.r[0] & 0xffff);
    r = out;
    r.r[1] = o;
    const uint32_t pid[1] = {F + 4};
    quazal(0x008847b0, &r, &out, 1, pid);
    const rx_addr buffer = out.r[0];
    for (uint32_t k = 0; k < 3; k++) {
        r = out;
        r.r[1] = buffer;
        const uint32_t w[3] = {args + 4 * k, 4, 1};
        quazal(0x0088c5e0, &r, &out, 3, w);
    }
    r = out;
    r.r[1] = o;
    const uint32_t s[2] = {1, buffer};
    quazal(0x00886b10, &r, 0, 2, s);
    ACT_END(F);
}
RX_FUNCTION(0x006ff8d0, rx_net_send_event)

/* ---- the Quazal reference handle ------------------------------------------------------------------------------------------------ */

/* 0088bcd0 (ECX = the handle H (0xc bytes); stdcall object; RET 4) -> EAX = H: H = {+0 the referenced object (0), +4 the object
 * argument, +8 byte 1 (counts both references)}, then 0088bb50 (ECX = H) looks the object up and takes the reference. A null object
 * takes none. Frame: push esi. */
rx_addr rx_quazal_handle_init(rx_addr h, rx_addr object) {
    ACT_FRAME(F, 0, 1);
    SU32(h, 0, 0);
    SU8(h, 8, 1);
    SU32(h, 4, object);
    rx_regs in = no_regs();
    in.r[1] = h;
    quazal(0x0088bb50, &in, 0, 0, 0);
    ACT_END(F);
    return h;
}
RX_FUNCTION(0x0088bcd0, rx_quazal_handle_init)

/* 0088bac0 (ECX = the handle): a held reference (+0) is released (+8 byte: both counts 0088b8f0, else the plain count 00885310) and
 * the handle cleared. Frame: push esi. */
void rx_quazal_handle_release(rx_addr h) {
    ACT_FRAME(F, 0, 1);
    const rx_addr object = U32(h, 0);
    if (object != 0) {
        rx_regs in = no_regs();
        in.r[1] = object;
        quazal(U8(h, 8) != 0 ? 0x0088b8f0 : 0x00885310, &in, 0, 0, 0);
        SU32(h, 0, 0);
    }
    ACT_END(F);
}
RX_FUNCTION(0x0088bac0, rx_quazal_handle_release)

/* 00885050 (ECX = the handle) -> AL (EAX as the original leaves it): is the referenced object usable? None (+0 = 0): Quazal's error
 * status 0xa0030004 is set (00884390: cdecl 0, 0, status, 0) and 0 returned. Else the object's station (008a32a0 of its id >> 22)
 * must accept the reference test (its vf15 with [0x9dfd9c]) or status 0xe003000c is set; 1 when it does. */
uint32_t rx_quazal_ref_ready(rx_addr h) {
    if (U32(h, 0) == 0) {
        rx_regs in = no_regs(), out = no_regs();
        const uint32_t a[4] = {0, 0, 0xa0030004u, 0};
        quazal(0x00884390, &in, &out, 4, a);
        return out.r[0] & ~0xffu;                                   /* the call's EAX with AL cleared (xor al, al) */
    }
    rx_regs in = no_regs(), out = no_regs();
    const uint32_t id[1] = {U32(U32(h, 0), 0x50) >> 22};
    quazal(0x008a32a0, &in, &out, 1, id);
    const rx_addr station = out.r[0];
    const rx_addr fn = U32(U32(station, 0), 0x3c);
    in = no_regs();
    in.r[1] = station;
    const uint32_t arg[1] = {U32(0, 0x009dfd9c)};
    quazal(fn, &in, &out, 1, arg);
    if ((out.r[0] & 0xff) == 0) {
        rx_regs in2 = no_regs(), out2 = no_regs();
        const uint32_t a[4] = {0, 0, 0xe003000cu, 0};
        quazal(0x00884390, &in2, &out2, 4, a);
        return out2.r[0] & ~0xffu;
    }
    return (out.r[0] & ~0xffu) | 1;
}
RX_FUNCTION(0x00885050, rx_quazal_ref_ready)

/* ---- the session recorder and the selection iterators --------------------------------------------------------------------- */

/* 006f7810 (cdecl state byte): the replay recorder notes an activity state change in the online session object [0xd60d54]: a handle
 * H to it (0088bcd0: H = F + 4, the object pointer as the argument slot F), and while the handle holds a valid object (703070:
 * EAX = H -> AL) whose reference is ready (00884950), its +0x148 byte = the state, its +0x15c byte = 1 and 00555540 (ECX = the
 * object: the change stamp) run; the handle released (0088bac0). Frame: sub 0xc; push ecx (the handle argument slot: the callee pops it).
 * Without a session (the offline stand-in [0xd60d54] = 0) the handle stays empty and the checks fail: no game memory is written. */
void rx_net_session_record_state(uint32_t state) {
    const rx_addr arg = rx_sp() + 4;
    ACT_FRAME(F, 0xc, 1);
    SU32(F, 0, U32(0, 0x00d60d54));
    const rx_addr h = F + 4;
    ACT_CALL(0, (void)rx_quazal_handle_init(h, U32(F, 0)));         /* 0088bcd0 pops the pushed slot */
    rx_set_sp(F + 4);
    rx_regs in = no_regs(), out = no_regs();
    in.r[0] = h;
    quazal(0x00703070, &in, &out, 0, 0);                            /* the checked reference: AL */
    if ((out.r[0] & 0xff) != 0) {
        in.r[0] = h;
        quazal(0x00703070, &in, &out, 0, 0);
        const rx_addr obj = (out.r[0] & 0xff) != 0 ? U32(h, 0) : 0;
        uint32_t valid;
        ACT_CALLR(valid, 0, rx_net_ref_valid(obj));                 /* 00884950 */
        if ((valid & 0xff) != 0) {
            in.r[0] = h;
            quazal(0x00703070, &in, &out, 0, 0);
            const rx_addr o1 = (out.r[0] & 0xff) != 0 ? U32(h, 0) : 0;
            SU8(o1, 0x148, state);
            in.r[0] = h;
            quazal(0x00703070, &in, &out, 0, 0);
            const rx_addr o2 = (out.r[0] & 0xff) != 0 ? U32(h, 0) : 0;
            SU8(o2, 0x15c, 1);
            in.r[0] = h;
            quazal(0x00703070, &in, &out, 0, 0);
            rx_regs q = no_regs();
            q.r[1] = (out.r[0] & 0xff) != 0 ? U32(h, 0) : 0;
            quazal(0x00555540, &q, 0, 0, 0);                        /* the change stamp (Quazal's clock hook + 00884ba0) */
        }
    }
    ACT_CALL(0, rx_quazal_handle_release(h));                       /* 0088bac0 */
    (void)arg;
    ACT_END(F);
}
RX_FUNCTION(0x006f7810, rx_net_session_record_state)

/* ---- the selection iterator's advance: 0053a170 / 00508770 / 00508800 --------------------------------------------------------- */

/* 0053a170 (ECX = a std::set<uint> S {+0 owner proxy, +0x18 head}; stdcall out, &key; RET 8): lower_bound. out = {S's proxy, the first
 * node whose key (+0xc, unsigned) is not below *key, the head when there is none}. Nodes: {+0 left, +4 parent, +8 right, +0xc key,
 * +0x15 isnil}; the head's parent (+4) is the root. */
void rx_net_set_lower_bound(rx_addr set, rx_addr out, rx_addr key) {
    rx_addr best = U32(set, 0x18);
    rx_addr node = U32(best, 4);
    if (U8(node, 0x15) == 0) {
        const uint32_t k = U32(key, 0);
        do {
            if (U32(node, 0xc) < k) {
                node = U32(node, 8);
            } else {
                best = node;
                node = U32(node, 0);
            }
        } while (U8(node, 0x15) == 0);
    }
    SU32(out, 4, best);
    SU32(out, 0, U32(set, 0));
}
RX_FUNCTION(0x0053a170, rx_net_set_lower_bound)

/* 00508770 (ECX = the iterator I; stdcall out, key; RET 8): the element following `key` in I's set (I + 0: the holder whose +4 is
 * the std::set<uint>): *out = the smallest element >= key + 1, 0 when there is none. The iterator returned by 0053a170 is checked
 * like a checked MSVC iterator (owner proxy, not the end: _invalid_parameter_noinfo). Frame: sub 0x14; push ebx / ebp / esi / edi
 * (key + 1 at +0x14, the iterator {proxy, node} at +0x18). */
void rx_net_selection_successor(rx_addr it, rx_addr out, uint32_t key) {
    ACT_FRAME(F, 0x14, 4);
    const rx_addr set = U32(it, 0) + 4;
    SU32(F, 0x14, key + 1);
    ACT_CALL(2, rx_net_set_lower_bound(set, F + 0x18, F + 0x14));
    const rx_addr proxy = U32(F, 0x18), node = U32(F, 0x1c);
    if (proxy == 0 || proxy != U32(set, 0)) crt_invalid_parameter_noinfo();
    if (node != U32(set, 0x18)) {
        if (proxy == 0) crt_invalid_parameter_noinfo();
        if (node == U32(U32(proxy, 0), 0x18)) crt_invalid_parameter_noinfo();
        SU32(out, 0, U32(node, 0xc));
    } else {
        SU32(out, 0, 0);
    }
    ACT_END(F);
}
RX_FUNCTION(0x00508770, rx_net_selection_successor)

/* 00508800 (ECX = the iterator I; plain ret): I moves to the element after its current key (I + 0x14), under the engine's stats
 * lock: the lock of the profile scope of stat 3 (402420 on 0x9df584; its +0xc -> +8, + 0x3c; 0x3c without the scope) is entered and left
 * through 00883d80 / 00883d90 unless locking is off (byte 0x9df580). The next element (00508770 into a local) is assigned to the
 * iterator's reference-counted handle (I + 4) by 0088bc40, which is Quazal's: BOUNDARY (online only) below. Frame: push ecx; push esi;
 * push edi (the local at +8). The last call is a tail jump (00883d90 returns to our caller). */
void rx_net_selection_advance(rx_addr it) {
    ACT_FRAME(F, 0, 3);
    uint32_t shared;
    ACT_CALLR(shared, 0, rx_world_shared_vf_ret0());                       /* 007a0760 */
    rx_addr stat;
    ACT_CALLR(stat, 2, rx_fx_thread_table_get(0x009df584, 3, shared));     /* 00402420 (ECX = 0x9df584; 3, shared) */
    rx_addr scope = 0;
    if (stat != 0 && U32(stat, 0xc) != 0) scope = U32(U32(stat, 0xc), 8);
    const rx_addr lock = scope + 0x3c;                                      /* EDI */
    if (U8(0, 0x009df580) == 0) ACT_CALL(0, rx_lock_enter(lock));          /* 00883d80 */
    const rx_addr local = F + 8;
    SU32(F, -4, U32(it, 0x14));                                             /* push ecx; mov [esp], the current key */
    SU32(F, -8, local);                                                     /* push edx: the out pointer */
    ACT_CALL(2, rx_net_selection_successor(it, local, U32(it, 0x14)));      /* 00508770 */
    {
        /* BOUNDARY (online only): Quazal ref-counted handle assignment (0088bc40); the iterator exists only in a network session. */
        uint32_t a = local;
        rx_call_original(0x0088bc40, RX_THISCALL, it + 4, 1, &a);
    }
    const int locked = U8(0, 0x009df580) == 0;
    ACT_END(F);
    if (locked) rx_lock_leave(lock);                                        /* jmp 00883d90 */
}
RX_FUNCTION(0x00508800, rx_net_selection_advance)

/* A network selection iterator I (0x24 bytes): built by 0088c380 (ECX = I; stack all, ready), then a DO filter (an IsAKindOfDOFilter
 * of 0x14 bytes: 00884650 allocates (0x14, "…/ObjDup/SelectionIteratorTemplate.h", line 0x7b), 0088afe0 builds it with the filter
 * class [0xa94348]) installed (0088c150: ECX = I; stack filter) and released once (its vf2); an empty iterator (+0 = 0) is reset
 * (0088bc40 on I + 4 with a zero) else positioned on its first match (00508800 ported below, then 0088c1e0 with 1). Frame: sub 8; push edi. */
static rx_addr selection_begin(rx_addr it, uint32_t all, uint32_t ready) {
    ACT_FRAME(F, 8, 1);
    rx_regs r = no_regs(), out = no_regs();
    r.r[1] = it;
    const uint32_t a0[2] = {all, ready};
    quazal(0x0088c380, &r, &out, 2, a0);
    r = out;
    const uint32_t a1[3] = {0x14, 0x0093f730, 0x7b};
    quazal(0x00884650, &r, &out, 3, a1);
    rx_addr filter = 0;
    if (out.r[0] != 0) {
        r = out;
        r.r[1] = out.r[0];
        const uint32_t a2[1] = {U32(0, 0x00a94348)};
        quazal(0x0088afe0, &r, &out, 1, a2);
        filter = out.r[0];
    }
    r = out;
    r.r[1] = it;
    const uint32_t a3[1] = {filter};
    quazal(0x0088c150, &r, &out, 1, a3);
    r = out;
    r.r[1] = filter;
    quazal(U32(U32(filter, 0), 8), &r, &out, 0, 0);                 /* the filter's vf2 */
    if (U32(it, 0) == 0) {
        SU32(F, 8, 0);
        r = out;
        r.r[1] = it + 4;
        const uint32_t a4[1] = {F + 8};
        quazal(0x0088bc40, &r, 0, 1, a4);
    } else {
        ACT_CALL(0, rx_net_selection_advance(it));                  /* 00508800 (ECX = the iterator) */
        r = out;
        r.r[1] = it;
        const uint32_t a5[1] = {1};
        quazal(0x0088c1e0, &r, 0, 1, a5);
    }
    ACT_END(F);
    return it;
}
/* 005544c0 (ESI = the iterator) -> EAX = it: 0088c380 (1, 1) */
rx_addr rx_net_selection_begin_all(rx_addr it) { return selection_begin(it, 1, 1); }
RX_FUNCTION(0x005544c0, rx_net_selection_begin_all)
/* 00585b20 (ESI = the iterator) -> EAX = it: 0088c380 (0, 1) */
rx_addr rx_net_selection_begin_ready(rx_addr it) { return selection_begin(it, 0, 1); }
RX_FUNCTION(0x00585b20, rx_net_selection_begin_ready)

/* 00508880 (ECX = the iterator): the iterator advanced: an empty one (+0 = 0) is reset (0088bc40 on it + 4 with a zero), else
 * 00508800 then 0088c1e0 (1). Frame: sub 8; push esi. */
void rx_net_selection_next(rx_addr it) {
    ACT_FRAME(F, 8, 1);
    rx_regs r = no_regs(), out = no_regs();
    if (U32(it, 0) == 0) {
        SU32(F, 8, 0);
        r.r[1] = it + 4;
        const uint32_t a[1] = {F + 8};
        quazal(0x0088bc40, &r, 0, 1, a);
    } else {
        ACT_CALL(0, rx_net_selection_advance(it));                  /* 00508800 (ECX = the iterator) */
        r = out;
        r.r[1] = it;
        const uint32_t a[1] = {1};
        quazal(0x0088c1e0, &r, 0, 1, a);
    }
    ACT_END(F);
}
RX_FUNCTION(0x00508880, rx_net_selection_next)
