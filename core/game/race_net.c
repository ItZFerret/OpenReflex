/* race_net.c -- the NetworkDriver's side of the network player: the replicated state of a remote rider (the "network player" object
 * driver +0xef0, MXPlayer +0xb2c) and its snapshots. Only reached online (a NetworkDriver with a valid network object): no reference
 * world runs it, so these ports are transliterated from the disassembly and checked by the oracle's probe (oracle/race_net_probe.cpp).
 * The Quazal / STL library underneath (selection iterators 00703450 / 00508800 / 0088c1e0 / 0088c190, ref-counted pointer 0088bc40,
 * change marking 006dcc70 / 00884ba0 / 00884c90, std::deque 00701bf0 / 00701c50 / 0080dbe0, the snapshot
 * ring's constructor / destructor 007004a0 / 007006a0 and its ends 00700750 / 00700cb0, the session mode 006f8860, the clock 00892db0)
 * is a boundary: those calls run the original (the product does not reach them offline).
 * ONLINE-SESSION BOUNDARY (2026-10-26, wave w-net; QUEUE.md "w-net"): every function of that list is reachable only through a
 * NetworkDriver whose +0xef0 network player is non-null. That pointer is the player's +0xb2c -> +0x1d8, and +0xb2c comes from
 * 006e3030 on the session [0xda6cb0] (rx_player_from_player_list: 0 in the product's offline stand-in, 0 in the reference worlds);
 * with +0xef0 == 0 rx_network_driver_body (race_driver_update.c) takes its 'net == 0' arm and only re-reads +0xb2c. The activity
 * side (race_activity.c countdown exit, world_wreck_misc.c timer) is guarded by the session word +0x1c4c != 0 (an online session).
 * What they write is the Quazal object-duplication state (selection iterators, ref-counted handles, the change-mark queue, the
 * snapshot ring in the network player's +0x1d4 block); nothing the offline race reads.
 *
 * Network player N (driver +0xef0): +0x14 owner, +0x20 flags (bit 0 = valid), +0x50 id (low 22 bits), +0x80.. +0xbc the replicated
 * controls, +0xc0 / +0xc1 / +0xc2 colour bytes, +0xc4.. the packed pose, +0xd0 position, +0xdc, +0x10c, +0x1d0 .. +0x1e4 the update
 * timers, +0x1d4 the snapshot state (a pointer to a pointer), +0x1f0 position copy, +0x240 last packed colour.
 * Snapshot S (0xc0 bytes; the replicated record the network layer exchanges): +0x10 position, +0x20 / +0x30 / +0x40 rows, +0x50 / +0x54..
 * (core/notes/race.md "Network drivers"). The float moves in the original go through fld / fstp: a signalling NaN comes out quiet. */
#include "race.h"
#include "race_extern.h"
#include "race_misc.h"
#include "fx.h"
#include "world_node.h"
#include "world_rider.h"
#include "phys_body.h"
#include "phys_coll_grid.h"
#include "build_vehicle.h"
#include "build_alloc.h"
#include "race_driver.h"
#include "loop_player.h"
#include "race_net_session.h"

/* fld dword; fstp dword: the bits, a signalling NaN quieted */
static uint32_t fcopy(uint32_t bits) {
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x007fffffu) != 0) bits |= 0x00400000u;
    return bits;
}
#define FC(dst, doff, src, soff) SU32((dst), (doff), fcopy(U32((src), (soff))))

/* ---- small objects -------------------------------------------------------------------------------------------------------- */

/* 0089c8a0 (cdecl out; EAX = out): the thread's current Quazal station into *out: the per-thread table entry [0x9dfb94] + 4 * id
 * when the thread id (the shared 007a0760: 0) is nonzero, else the global 0x9dfb98. */
rx_addr rx_net_current_station(rx_addr out) {
    uint32_t id;
    RACE_CORE_CALL(0, id = rx_world_shared_vf_ret0());
    if (id == 0) { SU32(out, 0, U32(0, 0x9dfb98)); return out; }
    SU32(out, 0, U32(U32(0, 0x9dfb94), 4 * id));
    return out;
}
RX_FUNCTION(0x0089c8a0, rx_net_current_station)

/* 00884950 (ECX = reference; AL): is the reference's owner (+0x14) the current station (or is there none)? 0 when it has no owner.
 * EAX is what the original leaves: the out pointer of the last station query (stack slot) with AL the result. Frame: push ecx, esi. */
uint32_t rx_net_ref_valid(rx_addr obj) {
    RACE_FRAME_PLAIN(F, 0, 2);
    if (U32(obj, 0x14) == 0) { RACE_FRAME_END(F); return 0; }
    rx_addr p;
    RACE_CORE_CALL(1, p = rx_net_current_station(F + 4));
    if (U32(p, 0) == 0) { RACE_FRAME_END(F); return (p & ~0xffu) | 1; }
    const uint32_t owner = U32(obj, 0x14);
    RACE_CORE_CALL(1, p = rx_net_current_station(F + 4));
    const uint32_t eq = owner == U32(p, 0);
    RACE_FRAME_END(F);
    return (p & ~0xffu) | eq;
}
RX_FUNCTION(0x00884950, rx_net_ref_valid)

/* 005088c0 (ECX = object; no result): marks the object changed now: the clock hook 0x9df898 (0 when none; 64 bit EDX:EAX) is
 * the stamp, handed to 00884c90 (ECX = object; stack 0, &stamp; RET 8). Frame: push ebp; and esp, -8; sub esp, 0xc; push esi. */
void rx_net_stamp(rx_addr net) {
    RACE_FRAME_EBP(S, 8, 0xc, 1);
    const rx_addr stamp = S + 8;
    const rx_addr hook = U32(0, 0x9df898);
    SU32(stamp, 0, 0); SU32(stamp, 4, 0);
    if (hook != 0) {
        rx_regs in, o; memset(&in, 0, sizeof in); in.r[1] = net; in.r[6] = net;   /* (the registers the original has: ECX, ESI = the object) */
        rx_call_original_ex(hook, &in, &o, 0, 0);
        SU32(stamp, 0, o.r[0]); SU32(stamp, 4, o.r[2]);
    }
    const uint32_t a[2] = {0, stamp};
    /* BOUNDARY (online only): 00884c90 queues a change mark on a Quazal duplicated object (owner +0x14 must be non-null; the
     * station / session lock 00883d80, the class table 008a32a0 vf14); rx_net_stamp is only called with a non-null +0xef0. */
    rx_call_original(0x00884c90, RX_THISCALL, net, 2, a);
    RACE_FRAME_END(S);
}
RX_FUNCTION(0x005088c0, rx_net_stamp)

/* 00508b90 (EAX = player, stack slot; RET 4) -> EAX: binds a snapshot state (0x140 bytes, 007004a0) to the player in the slot:
 * the same player keeps it (0); another player's is destroyed (007006a0, free) and a new one built; 1 when (re)built (the slot
 * is 0 when the allocation fails). Frame: push ecx ebx esi edi. */
uint32_t rx_net_snapshot_bind(rx_addr player, rx_addr slot) {
    RACE_FRAME_PLAIN(F, 0, 4);
    rx_addr old = U32(slot, 0);
    if (old != 0) {
        if (player == U32(old, 0x20)) { RACE_FRAME_END(F); return 0; }
        rx_regs in; memset(&in, 0, sizeof in); in.r[0] = old;
        /* BOUNDARY (online only): 007006a0 frees a snapshot ring (4 blocks + 2 deques) built by 007004a0 for a network player. */
        rx_call_original_ex(0x007006a0, &in, 0, 0, 0);
        RACE_CORE_CALL(1, rx_reflex_aligned_free(old));
        SU32(slot, 0, 0);
    }
    rx_addr obj;
    RACE_CORE_CALL(1, obj = rx_reflex_aligned_alloc(0x140));
    if (obj != 0) {
        rx_regs in; memset(&in, 0, sizeof in); in.r[7] = obj;
        const uint32_t a = player;
        rx_regs o;
        /* BOUNDARY (online only): 007004a0 constructs the 0x140-byte snapshot ring (timeGetTime stamp, zeroed record) of a network player. */
        rx_call_original_ex(0x007004a0, &in, &o, 1, &a);
        SU32(slot, 0, o.r[0]);
        RACE_FRAME_END(F);
        return 1;
    }
    SU32(slot, 0, 0);
    RACE_FRAME_END(F);
    return 1;
}
RX_FUNCTION(0x00508b90, rx_net_snapshot_bind)

/* 0050bc70 (EAX = network object, stack player; RET 4) -> EAX = 1: binds the network object's snapshot state (+0x1d4) to the player. */
uint32_t rx_net_bind_player(rx_addr net, rx_addr player) {
    const rx_addr slot = U32(net, 0x1d4);
    RACE_CORE_CALL(1, rx_net_snapshot_bind(player, slot));
    return 1;
}
RX_FUNCTION(0x0050bc70, rx_net_bind_player)

/* ---- the snapshot record ------------------------------------------------------------------------------------------------- */

/* 00700310 (ESI = snapshot; EAX = it): a neutral snapshot: zero floats, the rotation row +0x3c = 1, +0x8c = FLT_MAX, and +0x50 the
 * session clock in seconds (the session static first; the clock through 00892db0 online, 00892170 otherwise: the record's low
 * word x 0.001). Frame: push ebp; and esp, -8; sub esp, 0x14; push ebx. */
rx_addr rx_net_snapshot_init(rx_addr s) {
    RACE_FRAME_EBP(S, 8, 0x14, 1);
    SF32(s, 0x04, 0.0f);
    const int guard = (U8(0, 0xdcf304) & 1) != 0;
    SU32(s, 0x00, 0);
    SF32(s, 0x10, 0.0f); SF32(s, 0x14, 0.0f); SF32(s, 0x18, 0.0f);
    SF32(s, 0x20, 0.0f); SF32(s, 0x24, 0.0f); SF32(s, 0x28, 0.0f);
    SF32(s, 0x30, 0.0f); SF32(s, 0x34, 0.0f); SF32(s, 0x38, 0.0f);
    SF32(s, 0x3c, 1.0f /* 0x931f24 */);
    SF32(s, 0x40, 0.0f); SF32(s, 0x44, 0.0f); SF32(s, 0x48, 0.0f);
    SU32(s, 0x70, 0);
    SF32(s, 0x54, 0.0f); SF32(s, 0x58, 0.0f); SF32(s, 0x5c, 0.0f); SF32(s, 0x60, 0.0f); SF32(s, 0x64, 0.0f); SF32(s, 0x68, 0.0f);
    SF32(s, 0x6c, 0.0f);
    if (!guard) race_static_dcf304();
    rx_addr rec;
    const rx_addr sess = U32(0, 0xda6cb0);
    if (U32(sess, 0x1c4c) != 0 && U32(sess, 0x1b0) != 1) {
        const uint32_t a = S + 0x10;
        /* BOUNDARY (online only): 00892db0 = the Quazal session clock (008926d0) of the thread's station; guarded by +0x1c4c != 0. */
        rx_call_original(0x00892db0, RX_CDECL, 0, 1, &a);
        rec = a;
    } else {
        RACE_CORE_CALL(2, (void)rx_world_clock_now(S + 8, 0));
        rec = S + 8;
    }
    const int32_t ms = (int32_t)phys_int64_load(rec);
    SF32(s, 0x50, rx_muls((float)ms, 0.001f /* 0x97743c */));
    SU32(s, 0x74, 0); SU32(s, 0x78, 0); SU8(s, 0xb4, 0);
    SU32(s, 0x94, 0); SU32(s, 0xb0, 0); SU32(s, 0x90, 0); SU32(s, 0xbc, 0);
    SF32(s, 0x7c, 0.0f); SF32(s, 0x80, 0.0f); SF32(s, 0x84, 0.0f); SF32(s, 0x88, 0.0f);
    SU32(s, 0x8c, 0x7f7fffffu /* 0x977494 */);
    SF32(s, 0x98, 0.0f); SF32(s, 0x9c, 0.0f); SF32(s, 0xa0, 0.0f); SF32(s, 0xa4, 0.0f); SF32(s, 0xa8, 0.0f); SF32(s, 0xac, 0.0f);
    SF32(s, 0xb8, 0.0f);
    RACE_FRAME_END(S);
    return s;
}
RX_FUNCTION(0x00700310, rx_net_snapshot_init)

/* The replicated controls (network player +0x80 .. +0xbc) into a snapshot (+0x74 .. +0xbc): colour bytes, steering / throttle /
 * lean values, flags. The byte / word fields are widened (zero / sign extended) as the original does. */
static void net_to_snapshot_controls(rx_addr s, rx_addr n) {
    SU32(s, 0x74, U8(n, 0xc2));
    SU32(s, 0x78, U8(n, 0xc1));
    FC(s, 0x7c, n, 0xa8);
    FC(s, 0x80, n, 0x80);
    FC(s, 0x84, n, 0xb4);
    FC(s, 0x88, n, 0xa4);
    FC(s, 0x8c, n, 0x88);
    SU32(s, 0x90, (uint32_t)(int32_t)(int8_t)U8(n, 0x9c));
    SU8(s, 0xb4, U8(n, 0x8c));
    SU32(s, 0x94, U32(n, 0xac));
    FC(s, 0x98, n, 0xa0);
    FC(s, 0x9c, n, 0x94);
    FC(s, 0xa0, n, 0x84);
    FC(s, 0xa4, n, 0x90);
    FC(s, 0xa8, n, 0xb8);
    FC(s, 0xac, n, 0x98);
    SU32(s, 0xb0, (uint32_t)(int32_t)(int16_t)U16(n, 0x9e));
    FC(s, 0xb8, n, 0xb0);
    SU32(s, 0xbc, (uint32_t)(int32_t)(int8_t)U8(n, 0xbc));
}

/* 006fed10 (EAX = snapshot, ECX = network player; no result): the controls part of 006feb00. */
void rx_net_snapshot_controls(rx_addr s, rx_addr n) { net_to_snapshot_controls(s, n); }
RX_FUNCTION(0x006fed10, rx_net_snapshot_controls)

/* 006feb00 (EAX = network player, ECX = snapshot) -> EAX: the player's pose and controls into a snapshot, and the position
 * (+0xd0 .. +0xd8) also into +0x1f0 .. +0x1f8; 1 when the position (its bits) differs from the last recorded one (0xa95d50 ..). */
uint32_t rx_net_player_snapshot(rx_addr n, rx_addr s) {
    const uint32_t x = U32(n, 0xd0), y = U32(n, 0xd4), z = U32(n, 0xd8);
    SU32(n, 0x1f0, x); SU32(n, 0x1f4, y); SU32(n, 0x1f8, z);
    SU32(s, 0x10, x); SU32(s, 0x14, y); SU32(s, 0x18, z);
    FC(s, 0x30, n, 0xfc); FC(s, 0x34, n, 0x100); FC(s, 0x38, n, 0x104); FC(s, 0x3c, n, 0x108);
    FC(s, 0x20, n, 0x110); FC(s, 0x24, n, 0x114); FC(s, 0x28, n, 0x118);
    FC(s, 0x40, n, 0xc4); FC(s, 0x44, n, 0xc8); FC(s, 0x48, n, 0xcc);
    FC(s, 0x54, n, 0xe0); FC(s, 0x58, n, 0xe4); FC(s, 0x5c, n, 0xe8); FC(s, 0x60, n, 0xec);
    FC(s, 0x64, n, 0xf0); FC(s, 0x68, n, 0xf4); FC(s, 0x6c, n, 0xf8);
    SU32(s, 0x70, U32(n, 0xdc));
    FC(s, 0x50, n, 0x10c);
    net_to_snapshot_controls(s, n);
    return ((z - U32(0, 0xa95d58)) | (y - U32(0, 0xa95d54)) | (x - U32(0, 0xa95d50))) != 0;
}
RX_FUNCTION(0x006feb00, rx_net_player_snapshot)

/* 00431cf0 (ECX = source, stack destination; RET 4): copies a pose record (0x19 words): the four vec4s +0x00 .. +0x30 (movaps)
 * and the flags word +0x44 as they are, the floats +0x40 and +0x48 .. +0x60 through fld / fstp (a signaling NaN comes out quiet). */
static uint32_t net_x87_copy32(uint32_t b) { return ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu)) ? (b | 0x400000u) : b; }
void rx_net_pose_copy(rx_addr src, rx_addr dst) {
    for (uint32_t i = 0; i < 0x10; i++) SU32(dst, 4 * i, U32(src, 4 * i));
    SU32(dst, 0x40, net_x87_copy32(U32(src, 0x40)));
    SU32(dst, 0x44, U32(src, 0x44));
    for (uint32_t off = 0x48; off <= 0x60; off += 4) SU32(dst, off, net_x87_copy32(U32(src, off)));
}
RX_FUNCTION(0x00431cf0, rx_net_pose_copy)

/* ---- small functions of the reset ----------------------------------------------------------------------------------------- */

/* 0045d4d0 (EAX = vehicle): the vehicle's body record (+0x17c handle): its +0x2c record's field +0x14 = 0. */
void rx_vehicle_body_link_clear(rx_addr v) {
    rx_addr h = U32(v, 0x17c);
    if (h == 0) return;
    rx_addr rec;
    RACE_CORE_CALL(0, rec = phys_body_record(h));
    SU32(U32(rec, 0x2c), 0x14, 0);
}
RX_FUNCTION(0x0045d4d0, rx_vehicle_body_link_clear)

/* 005084d0 (ECX = vehicle): the vehicle's controls block (+0x1328) +0x148 = 0xff, then vf32 (1). */
void rx_vehicle_net_controls_reset(rx_addr v) {
    SU8(U32(v, 0x1328), 0x148, 0xff);
    race_thiscall(race_vfn(v, 32), v, 1, 1, 0, 0, 0);
}
RX_FUNCTION(0x005084d0, rx_vehicle_net_controls_reset)

/* 0061c6b0 (EAX = pad block): the pad block's held state: +0x5bb5c = 1, +0xa2 = 1, +0x5bb64 = 1, +0x5b6d0 = 0. */
void rx_pad_net_hold(rx_addr pad) {
    SU32(pad, 0x5bb5c, 1); SU8(pad, 0xa2, 1); SU8(pad, 0x5bb64, 1); SU32(pad, 0x5b6d0, 0);
}
RX_FUNCTION(0x0061c6b0, rx_pad_net_hold)

/* 0054e110 (ESI = player; no result): +0x9ac = the UI's "pause for this player" flag: only in the network player modes (2 and 3),
 * from the UI settings (0xd92efc): the player's pad entry (+0x1f0 or the UI's current one +0x16c), or the default +0x130, == 0. */
void rx_player_net_pause_flag(rx_addr p) {
    uint32_t mode;
    RACE_CORE_CALL(0, mode = rx_game_player_mode());
    const uint32_t flag = mode == 2 || mode == 3;
    SU32(p, 0x9ac, flag);
    if (flag == 0) return;
    const rx_addr ui = U32(0, 0xd92efc);
    const int32_t pad = I32(p, 0x1f0), cur = I32(ui, 0x16c);
    if (pad == cur) {
        if (cur != -1) { SU32(p, 0x9ac, U32(ui, 0x10 + 0x48 * (uint32_t)pad) == 0); return; }
    } else if (cur != -1) {
        SU32(p, 0x9ac, U32(ui, 0x10 + 0x48 * (uint32_t)cur) == 0);
        return;
    }
    SU32(p, 0x9ac, U32(ui, 0x130) == 0);
}
RX_FUNCTION(0x0054e110, rx_player_net_pause_flag)

/* ---- NetworkDriver ---------------------------------------------------------------------------------------------------------- */

typedef void (*race_vf49_fn)(rx_addr, uint32_t);
static void vehicle_set_net_mode(rx_addr v, uint32_t on) {          /* vehicle vf49 (+0xc4) */
    const rx_addr fn = race_vfn(v, 49);
    race_vf49_fn f = RX_FN(fn, race_vf49_fn);
    if (f) RACE_CORE_CALL(1, f(v, on)); else race_thiscall(fn, v, 1, on, 0, 0, 0);
}

/* 0050e7e0 (EDI = driver, stack snapshot; RET 4; no result): the remote rider's state from a snapshot. While the network player
 * is for this track view (0075c5f0 == flag +0x129 bit 0, not +0xc8): a resume (the driver's +0xefc and +0xf00 set, the vehicle
 * there, no reset delay, and for a mode-5 session the activity's flag) turns the vehicle's network mode on (vf49), clears the
 * body link, resets the pad hold and the pause flag; either way the snapshot's pose (+0x10 .. +0x70) goes through a local record
 * (initialised by 0042e560) into the vehicle's remote target (+0x1220, +0x1214 = 1). Otherwise the driver waits (+0xefc = 1,
 * +0xf00 = 0) and the vehicle's network mode goes off. Frame: push ebp; and esp, -16; sub esp, 0x88; push ebx, esi. */
void rx_net_player_reset(rx_addr d, rx_addr snap) {
    RACE_FRAME_EBP(S, 16, 0x88, 2);
    rx_addr player = U32(d, 0x74);
    if (U32(player, 0xb2c) == 0 || U32(player, 0x1ac) == 0) { RACE_FRAME_END(S); return; }
    uint32_t view;
    RACE_CORE_CALL(0, view = rx_game_is_view_track(U32(0, 0xa946b8)));
    rx_addr rec = U32(player, 0xb2c);
    if (view != (U8(rec, 0x129) & 1u) || U8(rec, 0xc8) != 0) {
        SU32(d, 0xefc, 1); SU32(d, 0xf00, 0);
        rx_addr v = U32(player, 0x1ac);
        if (U32(v, 0x17c) != 0) {
            rx_addr b;
            RACE_CORE_CALL(0, b = phys_body_record(U32(v, 0x17c)));
            SU32(U32(b, 0x2c), 0x14, 0xffffffffu);
        }
        vehicle_set_net_mode(U32(U32(d, 0x74), 0x1ac), 0);
        SU8(U32(U32(U32(d, 0x74), 0x1ac), 0x1328), 0x14e, 0);
        SU32(U32(d, 0x74), 0x9ac, 0);
        rx_addr pad = U32(U32(d, 0x74), 0x1b4);
        if (pad != 0) { SU32(pad, 0x5bb5c, 0); SU8(pad, 0xa2, 0); SU8(pad, 0x5bb64, 0); SU32(pad, 0x5b6d0, 0); }
        RACE_FRAME_END(S);
        return;
    }
    uint32_t mode;
    /* BOUNDARY (online only): 006f8860 reads the session mode byte (+0x85) of the Quazal session object [0xd60d54] under its lock. */
    RACE_CORE_CALL(0, mode = rx_call_original(0x006f8860, RX_CDECL, 0, 0, 0));
    if (U32(d, 0xefc) != 0 && U32(d, 0xf00) != 0 && U32(U32(d, 0x74), 0x1ac) != 0 && U32(U32(d, 0x74), 0x1e4) == 0 &&
        (mode != 5 || U32(U32(U32(U32(d, 0x74), 0x94), 0x234), 0x20018) != 0)) {
        SU32(d, 0xefc, 0);
        rx_addr sess;
        RACE_CORE_CALL(0, sess = rx_fx_session_manager());
        if (U32(sess, 8) != 0 && mode == 8) {
            RACE_CORE_CALL(0, sess = rx_fx_session_manager());
            const rx_addr root = U32(sess, 8);
            RACE_CORE_CALL(0, sess = rx_fx_session_manager());
            RACE_CORE_CALL(1, rx_fx_session_clock(sess, F32(root, 0x294)));
        }
        SU8(U32(U32(U32(d, 0x74), 0x1ac), 0x1328), 0x14e, 1);
        vehicle_set_net_mode(U32(U32(d, 0x74), 0x1ac), 1);
        RACE_CORE_CALL(0, rx_vehicle_body_link_clear(U32(U32(d, 0x74), 0x1ac)));
        RACE_CORE_CALL(0, rx_vehicle_net_controls_reset(U32(U32(d, 0x74), 0x1ac)));
        rx_addr pad = U32(U32(d, 0x74), 0x1b4);
        if (pad != 0) {
            RACE_CORE_CALL(0, rx_pad_net_hold(pad));
            RACE_CORE_CALL(1, rx_world_rider_chains_fade_in(pad, 4, 0, 0.25f /* 0x92ff84 */));
        }
        RACE_CORE_CALL(0, rx_player_net_pause_flag(U32(d, 0x74)));
    }
    const rx_addr L = S + 0x10;
    RACE_CORE_CALL(0, rx_veh_body_rec_init(L));
    rx_st4(L + 0x00, rx_ld4(snap + 0x10));
    rx_st4(L + 0x10, rx_ld4(snap + 0x30));
    rx_st4(L + 0x20, rx_ld4(snap + 0x20));
    rx_st4(L + 0x30, rx_ld4(snap + 0x40));
    SU32(L, 0x48, U32(snap, 0x54)); SU32(L, 0x4c, U32(snap, 0x58));
    SU32(L, 0x50, U32(snap, 0x5c)); SU32(L, 0x54, U32(snap, 0x60));
    SU32(L, 0x58, U32(snap, 0x64)); SU32(L, 0x5c, U32(snap, 0x68));
    SU32(L, 0x60, U32(snap, 0x6c));
    SU32(L, 0x44, U32(snap, 0x70));
    SU32(L, 0x40, U32(snap, 0x50));
    rx_addr v = U32(U32(d, 0x74), 0x1ac);
    RACE_CORE_CALL(1, rx_net_pose_copy(L, v + 0x1220));
    SU32(v, 0x1214, 1);
    RACE_FRAME_END(S);
}
RX_FUNCTION(0x0050e7e0, rx_net_player_reset)

/* 006fe210 (stdcall network player N, dt; RET 8) -> EAX: one frame of the network player's replicated state (the snapshot state
 * N+0x1d4 -> S0, its newest snapshot R (00700750 pushes the vehicle's state, 00700cb0 reads the record)): the distance of every
 * other network player in the selection iterator (those with another id, +0x50 low 22 bits) from the player: sum, minimum and
 * count give the mean distance, which picks the update intervals; the snapshot's replicated fields are copied into N; the change
 * markers (006dcc70) are raised when the colour changed, the position timer ran out or the send timer (+0x1d0) passed its
 * interval (by the player's distance class, the UI mode and the session). 0 when there is no state / vehicle, else 1.
 * Frame: push ebp; and esp, -16; sub esp, 0x54; push ebx esi edi; locals: +0x14 the distance sum (then the mean), +0x18 a
 * scratch float (the iterator's reference), +0x1c the minimum, +0x20 the count, +0x24 N's id, +0x28 the iterated player's flag,
 * +0x2c .. +0x34 the selection iterator, +0x50 / +0x54 the iterated player's position x / y. */
uint32_t rx_net_player_update(rx_addr n, float dt) {
    RACE_FRAME_EBP(S, 16, 0x54, 3);
    const rx_addr it = S + 0x2c;
    SF32(S, 0x14, 0.0f);
    SU32(S, 0x1c, 0x7f7fffffu /* 0x977494 */);
    SU32(S, 0x20, 0);
    const rx_addr st = U32(n, 0x1d4);
    const rx_addr s0 = U32(st, 0);
    /* BOUNDARY (online only): 00700750 pushes the vehicle's state into the snapshot ring (std::deque 0080dbe0), 00700cb0 reads the
     * newest record; 00703450 / 0088c1e0 / 0088c190 / 0088bc40 are the Quazal selection iterator over the network
     * players; 006dcc70 (-> 00884ba0) raises the change marks. rx_net_player_update needs a non-null +0xef0 (see the file header). */
    if (s0 == 0 || U32(U32(s0, 0x20), 0x1ac) == 0 || rx_call_original(0x00700750, RX_THISCALL, s0, 0, 0) == 0) {
        RACE_FRAME_END(S);
        return 0;
    }
    rx_addr r;
    {
        rx_regs in, o; memset(&in, 0, sizeof in); in.r[7] = U32(n, 0x1d4);
        rx_call_original_ex(0x00700cb0, &in, &o, 0, 0);
        r = o.r[0];
    }
    SU32(S, 0x24, U32(n, 0x50) & 0x3fffffu);
    {
        rx_regs in; memset(&in, 0, sizeof in); in.r[6] = it;
        rx_call_original_ex(0x00703450, &in, 0, 0, 0);
    }
    if (U32(it, 0) == 0) {
        SU32(S, 0x18, 0);
        const uint32_t a = S + 0x18;
        rx_call_original(0x0088bc40, RX_THISCALL, it + 4, 1, &a);
    } else {
        RACE_CORE_CALL(0, rx_net_selection_advance(it));                  /* 00508800 (race_net_session.c) */
        const uint32_t one = 1;
        rx_call_original(0x0088c1e0, RX_THISCALL, it, 1, &one);
    }
    while (U32(it, 8) != 0) {
        rx_addr cur = U32(it, 4);
        SU32(S, 0x28, (uint32_t)(int32_t)(int8_t)U8(cur, 0x168));
        if (U32(it, 8) == 0) {
            RACE_CORE_CALL(4, rx_thread_note(0, 0, 0xa0000007u, 0));   /* 00884390 (loop_player.c) */
            cur = 0;
        }
        if ((U32(cur, 0x50) & 0x3fffffu) != U32(S, 0x24)) {
            cur = U32(it, 4);
            if (U32(it, 8) == 0) {
                RACE_CORE_CALL(4, rx_thread_note(0, 0, 0xa0000007u, 0));
                cur = 0;
            }
            SU32(S, 0x50, U32(cur, 0xd0));
            SU32(S, 0x54, U32(cur, 0xd4));
            float sum = (float)race_pow((double)rx_subs(F32(r, 0x38), F32(cur, 0xd8)), 2.0 /* 0x9774f8 */);
            SF32(S, 0x18, sum);
            float q = (float)race_pow((double)rx_subs(F32(r, 0x34), F32(S, 0x54)), 2.0);
            sum = rx_adds(F32(S, 0x18), q);
            SF32(S, 0x18, sum);
            q = (float)race_pow((double)rx_subs(F32(r, 0x30), F32(S, 0x50)), 2.0);
            const float dist = rx_sqrts(rx_adds(sum, q));
            if (F32(S, 0x1c) > dist) SF32(S, 0x1c, dist);
            SF32(S, 0x14, rx_adds(dist, F32(S, 0x14)));
            SU32(S, 0x20, U32(S, 0x20) + 1);
        }
        const uint32_t zero = 0;
        rx_call_original(0x0088c1e0, RX_THISCALL, it, 1, &zero);
    }
    const int32_t count = I32(S, 0x20);
    float avg = rx_divs(F32(S, 0x14), (float)count);
    SF32(S, 0x14, avg);
    rx_addr ctx = U32(0, 0xa95844), track = 0;
    if (U32(U32(ctx, 0x20), 0x34) == 1) track = U32(U32(0, 0xa946b8), 0x5a54);
    else if (U32(U32(ctx, 0x1c), 0x34) == 1) track = U32(U32(0, 0xa946b8), 0x20c8);
    if (U32(track, 0x20018) != 0 && (uint32_t)count != U32(n, 0x1e4)) {
        SU32(n, 0x1dc, fcopy(U32(n, 0x1d8)));
        SU32(n, 0x1e4, (uint32_t)count);
    }
    SU8(n, 0xc2, U8(r, 0x94));
    SU8(n, 0xc1, U8(r, 0x98));
    const rx_addr n80 = n + 0x80;
    FC(n, 0xa8, r, 0x9c);
    const float t0 = F32(n, 0x1dc);
    FC(n80, 0, r, 0xa0);
    FC(n, 0xb4, r, 0xa4);
    FC(n, 0xa4, r, 0xa8);
    FC(n, 0x88, r, 0xac);
    SU8(n, 0x9c, U8(r, 0xb0));
    SU32(n, 0xac, U32(r, 0xb4));
    FC(n, 0xa0, r, 0xb8);
    FC(n, 0x94, r, 0xbc);
    FC(n, 0x84, r, 0xc0);
    FC(n, 0x90, r, 0xc4);
    FC(n, 0xb8, r, 0xc8);
    FC(n, 0x98, r, 0xcc);
    SU16(n, 0x9e, U16(r, 0xd0));
    SU8(n, 0x8c, U8(r, 0xd4));
    FC(n, 0xb0, r, 0xd8);
    SU8(n, 0xbc, U8(r, 0xdc));
    if (!(t0 < 0.0f) && t0 == t0) {      /* comiss: jb is also taken for a NaN */
        if (((U32(r, 0x38) - U32(0, 0xa95d58)) | (U32(r, 0x34) - U32(0, 0xa95d54)) | (U32(r, 0x30) - U32(0, 0xa95d50))) != 0)
            SF32(n, 0x1dc, rx_subs(t0, dt));
    }
    FC(n, 0x10c, r, 0x70);
    FC(n, 0xd0, r, 0x30); FC(n, 0xd4, r, 0x34); FC(n, 0xd8, r, 0x38);
    FC(n, 0xfc, r, 0x50); FC(n, 0x100, r, 0x54); FC(n, 0x104, r, 0x58); FC(n, 0x108, r, 0x5c);
    FC(n, 0x110, r, 0x40); FC(n, 0x114, r, 0x44); FC(n, 0x118, r, 0x48);
    FC(n, 0xc4, r, 0x60); FC(n, 0xc8, r, 0x60); FC(n, 0xcc, r, 0x60);
    FC(n, 0xe0, r, 0x74); FC(n, 0xe4, r, 0x78); FC(n, 0xe8, r, 0x7c); FC(n, 0xec, r, 0x80);
    FC(n, 0xf0, r, 0x84); FC(n, 0xf4, r, 0x88); FC(n, 0xf8, r, 0x8c);
    SU32(n, 0xdc, U32(r, 0x90));
    const uint32_t packed = ((uint32_t)(int32_t)(int8_t)U8(n, 0xc0) << 16) + (U8(n, 0xc1) << 8) + U8(n, 0xc2);
    const float t1 = F32(n, 0x1dc);
    if (0.0f > t1) {
        if (packed != U32(n, 0x240)) {
            SU8(n, 0xc3, 1);
            SU32(n, 0x240, packed);
            const uint32_t a = n + 0xc0;
            rx_call_original(0x006dcc70, RX_THISCALL, n, 1, &a);
        }
    }
    if (0.0f > F32(n, 0x1dc)) {
        const rx_addr p = U32(n, 0x1d4);
        float lim = 0.0f;
        uint32_t i = 0;
        for (; i < 3; i++) if (avg > F32(p, 0x38 + 4 * i)) break;
        lim = i < 3 ? F32(p, 0x68 + 4 * i) : F32(p, 0x70);
        const float acc = rx_adds(F32(p, 0x10), dt);
        SF32(p, 0x10, acc);
        if (acc > lim) {
            SF32(p, 0x10, rx_subs(acc, lim));
            SU8(n, 0xbd, 1);
            const uint32_t a = n + 0x80;
            rx_call_original(0x006dcc70, RX_THISCALL, n, 1, &a);
        }
    }
    const float t = F32(n, 0x1dc), period = F32(n, 0x1d8);
    if (period > t) {
        float mn;
        if (U32(S, 0x28) != 0) { avg = 0.0f; mn = 0.0f; }
        else mn = F32(S, 0x1c);
        const rx_addr q = U32(n, 0x1d4) + 0x14;
        float iv;
        if (50.0f /* 0x9d78dc */ > mn) iv = 83.0f /* 0x9cd9c8 */;
        else {
            uint32_t k = 0;
            for (; k < 3; k++) if (avg > F32(q, 4 * k)) break;
            iv = k < 3 ? F32(q, 0x30 + 4 * k) : F32(q, 0x38);
        }
        iv = rx_muls(iv, 0.001f /* 0x97743c */);
        if (U32(U32(U32(0, 0xa95844), 0x10), 0xc) != 0) iv = 1.0f;
        else if (t > 0.0f) {
            if (U32(n, 0x1e0) != 0) iv = t > rx_muls(period, 0.5f /* 0x9322cc */) ? 10.0f /* 0x92fd88 */ : 0.75f /* 0x935aa8 */;
            else {
                rx_addr sm;
                RACE_CORE_CALL(0, sm = rx_fx_session_manager());
                iv = U32(U32(sm, 0x10), 0x1c48) != 0 ? 0.5f : 0.333f /* 0x977478 */;
            }
        } else SU32(n, 0x1e0, 0);
        const float acc = rx_adds(F32(n, 0x1d0), dt);
        SF32(n, 0x1d0, acc);
        if (acc > iv) {
            SF32(n, 0x1d0, 0.0f);
            SU8(n, 0x11c, 1);
            const uint32_t a = n + 0xc4;
            rx_call_original(0x006dcc70, RX_THISCALL, n, 1, &a);
        }
    }
    rx_call_original(0x0088c190, RX_THISCALL, it, 0, 0);
    RACE_FRAME_END(S);
    return 1;
}
RX_FUNCTION(0x006fe210, rx_net_player_update)
