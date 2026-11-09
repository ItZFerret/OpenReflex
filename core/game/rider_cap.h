/* rider_cap.h (w-rc-core) -- races past the original's 12 riders, up to 32 (port addition; core/plan/RIDER_CAP.md).
 *
 * The original keeps 12 of everything per rider inline in its objects: the game's 12 MXPlayer seats (MXGame + 0x7460, 0x12a0 each)
 * with their 12-entry trailer (+0xdf80 used bytes, +0xdf8c the active-seat order list, +0xdfbc its count byte, +0xe010 the leave
 * queue), the activity's 12 standings (+0x2d0 .. +0x2ff), and so on. Riders 13..32 cannot grow those in place (fixed offsets follow).
 * The faithful design: entries 0..11 stay EXACTLY where the original has them (a race of 12 or fewer riders is byte-for-byte the
 * original: nothing below reads or writes the side storage then), entries 12..31 live in this side storage, at fixed addresses of
 * the arena outside the original's image and heap (0x0e000000 .. 0x0fffffff: free in the original's process, unmapped in the
 * emulator of the oracle build, which only runs <= 12 riders). The arena is fresh per session (rx_mem), so is this storage.
 *
 * Side storage values that mean "empty" in the original (-1 seat indices) are stored complemented, so the zero-filled arena reads
 * as -1 without writing anything first. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

#define RX_RC_ORIG 12u                         /* the original's riders (seats, standings, tables) */
#define RX_RC_MAX  32u                         /* the port's cap (rx_api.h RX_MAX_RIDERS) */
#define RX_RC_SIDE (RX_RC_MAX - RX_RC_ORIG)    /* entries in side storage: 20 */

#define RX_RC_BASE        0x0e000000u
#define RX_RC_SEATS       (RX_RC_BASE + 0x00000u)   /* seats 12..31: 20 x 0x12a0 (0x17440) MXPlayer objects */
#define RX_RC_SEAT_USED   (RX_RC_BASE + 0x18000u)   /* used bytes of seats 12..31 (the original's +0xdf80 + seat) */
#define RX_RC_SEAT_ORDER  (RX_RC_BASE + 0x18020u)   /* order list positions 12..31 (+0xdf8c + 4 i), complemented */
#define RX_RC_SEAT_LEAVE  (RX_RC_BASE + 0x18080u)   /* leave queue positions 12..31 (+0xe010 + 4 k): unique ids */
#define RX_RC_ACT         (RX_RC_BASE + 0x20000u)   /* per-object records (RX_RC_ACT_N x RX_RC_ACT_SIZE): rx_rc_act */
#define RX_RC_ACT_N       32u
#define RX_RC_ACT_SIZE    0x800u
/* an activity's record: +0 the activity, +4 standings 12..31 (+0x2d0), +0x60 race index flags 12..31 (+0x304), +0x100 start slots
 * 28..39 (+0x340, 0x30 each: rx_rc_start_slot) */
#define RX_RC_ACT_STAND   0x004u
#define RX_RC_ACT_RACEIDX 0x060u
#define RX_RC_ACT_SLOTS   0x100u

/* ---- the gate of the side tables that a vehicle or rider claims (effect sets, mud, damage, sound slots): only while the event has
 * more than 12 players (the Player List's rows), so that an extra vehicle of a 12-rider event (one the original leaves without an
 * effect set ...) stays without one, as in the original ---- */
int rx_rc_more(void);

/* the new capacity of a full pointer vector {data, count, capacity, grow} (00544cc0's push and its kind): capacity + grow, as the
 * original; a vector made with grow 0 (the vehicle list's specs (24) and made vehicles (12), the gear list's (24 / 12 / 12), the
 * spawned riders (12)) would keep its capacity and the push write past the block: with 13+ players it grows by RX_RC_MAX instead */
static inline uint32_t rx_rc_grow(uint32_t capacity, uint32_t grow) { return grow == 0 && rx_rc_more() ? capacity + RX_RC_MAX : capacity + grow; }

/* ---- the port setting FIELD SIZE (rx_api.h rx_session_set_field_size): 0 = the MaxRiders tables (the original), else 13 .. 32 ---- */
void     rx_rc_set_field_size(uint32_t riders);
uint32_t rx_rc_field_size(void);

/* ---- the game's player seats (A = MXGame + 0x7460) ---- */
#define RX_SEAT_SIZE 0x12a0u
static inline rx_addr rx_seat(rx_addr A, uint32_t slot) {
    return slot < RX_RC_ORIG ? A + RX_SEAT_SIZE * slot : RX_RC_SEATS + RX_SEAT_SIZE * (slot - RX_RC_ORIG);
}
/* the slot of a seat's player object, -1 when p is no seat */
static inline int32_t rx_seat_of(rx_addr A, rx_addr p) {
    if (p >= A && p < A + RX_RC_ORIG * RX_SEAT_SIZE) return (p - A) % RX_SEAT_SIZE ? -1 : (int32_t)((p - A) / RX_SEAT_SIZE);
    if (p >= RX_RC_SEATS && p < RX_RC_SEATS + RX_RC_SIDE * RX_SEAT_SIZE)
        return (p - RX_RC_SEATS) % RX_SEAT_SIZE ? -1 : (int32_t)(RX_RC_ORIG + (p - RX_RC_SEATS) / RX_SEAT_SIZE);
    return -1;
}
static inline uint32_t rx_seat_used(rx_addr A, uint32_t slot) {
    return slot < RX_RC_ORIG ? U8(A, 0xdf80 + slot) : U8(RX_RC_SEAT_USED, slot - RX_RC_ORIG);
}
static inline void rx_seat_set_used(rx_addr A, uint32_t slot, uint32_t v) {
    if (slot < RX_RC_ORIG) SU8(A, 0xdf80 + slot, v); else SU8(RX_RC_SEAT_USED, slot - RX_RC_ORIG, v);
}
/* the i-th entry of the active-seat order list (a seat index, -1 = none) */
static inline uint32_t rx_seat_order(rx_addr A, uint32_t i) {
    return i < RX_RC_ORIG ? U32(A, 0xdf8c + 4 * i) : ~U32(RX_RC_SEAT_ORDER, 4 * (i - RX_RC_ORIG));
}
static inline void rx_seat_set_order(rx_addr A, uint32_t i, uint32_t slot) {
    if (i < RX_RC_ORIG) SU32(A, 0xdf8c + 4 * i, slot); else SU32(RX_RC_SEAT_ORDER, 4 * (i - RX_RC_ORIG), ~slot);
}
/* the player of the i-th active seat */
static inline rx_addr rx_seat_active(rx_addr A, uint32_t i) { return rx_seat(A, rx_seat_order(A, i)); }
/* the number of active seats (the count byte +0xdfbc) */
static inline uint32_t rx_seat_count(rx_addr A) { return U8(A, 0xdfbc); }
/* the leave queue (unique ids of players leaving; count +0xe040) */
static inline uint32_t rx_seat_leave(rx_addr A, uint32_t k) {
    return k < RX_RC_ORIG ? U32(A, 0xe010 + 4 * k) : U32(RX_RC_SEAT_LEAVE, 4 * (k - RX_RC_ORIG));
}
static inline void rx_seat_set_leave(rx_addr A, uint32_t k, uint32_t uid) {
    if (k < RX_RC_ORIG) SU32(A, 0xe010 + 4 * k, uid); else SU32(RX_RC_SEAT_LEAVE, 4 * (k - RX_RC_ORIG), uid);
}
/* the game's seats: [0x00a946b8] + 0x7460 */
static inline rx_addr rx_seats(void) { return U32(0, 0x00a946b8) + 0x7460; }

/* ---- the original's 12-entry global tables (.data / .bss, `table + stride * k`), entries 12..31 in side storage ----
 * Each table the port extends is listed in rider_cap.c (an unlisted one aborts: no silent overlap). */
#define RX_RC_TABS        (RX_RC_BASE + 0x40000u)   /* the tables' entries 12..31, to RX_RC_BASE + 0x1000000 (rider_cap.c) */
rx_addr rx_rc_tab_side(rx_addr table, uint32_t stride, uint32_t k);
static inline rx_addr rx_rc_tab(rx_addr table, uint32_t stride, uint32_t k) {
    return k < RX_RC_ORIG ? table + stride * k : rx_rc_tab_side(table, stride, k);
}
/* the entries of a table a loop over "all of them" visits: 12 + the side entries ever taken (rx_rc_tab_mark; host bookkeeping, so a
 * race of 12 or fewer riders never touches the side entries: the original's loops, the original's register / memory state) */
uint32_t rx_rc_tab_n(rx_addr table);
void     rx_rc_tab_mark(rx_addr table, uint32_t k);

/* the mud records (fx_mud_bind.c: 0xa96d90, 12 x 0x40; owner +0x30): record k, and the records a loop visits */
#define RX_MUD 0x00a96d90u
static inline rx_addr rx_mud_rec(uint32_t k) { return rx_rc_tab(RX_MUD, 0x40, k); }
static inline uint32_t rx_mud_n(void) { return rx_rc_tab_n(RX_MUD); }
/* the side mud record owned by the vehicle (records 12..), 0 when none: after the original's loop over its 12 */
static inline rx_addr rx_mud_side_of(rx_addr vehicle) {
    for (uint32_t k = RX_RC_ORIG; k < rx_mud_n(); k++) if (U32(rx_mud_rec(k), 0x30) == vehicle && vehicle) return rx_mud_rec(k);
    return 0;
}

/* the damage behaviours (world_veh_raven.c 0047bb40: the manager 0xdb1410, 12 x 0x410 from +0x10; the vehicle +0x3fc): entry k of
 * the manager `m` (always 0xdb1410), and the entries a loop visits */
static inline rx_addr rx_dmg_entry(rx_addr m, uint32_t k) { return k < RX_RC_ORIG ? m + 0x10 + 0x410 * k : rx_rc_tab(0x00db1420u, 0x410, k); }
static inline uint32_t rx_dmg_n(void) { return rx_rc_tab_n(0x00db1420u); }

/* ---- per-object side records (an activity's, a scenario cast's ...), keyed by the object's address ---- */
/* the object's record (0 when it has none and !create); claimed by the first write past the original's entries */
rx_addr rx_rc_act(rx_addr act, int create);
void    rx_rc_act_clear(rx_addr act);                            /* the activity's record released (when it has one) */

/* ---- an activity's standings (+0x2d0, 12 driver pointers inline; +0x26c the drivers) ---- */
static inline rx_addr rx_stand(rx_addr act, uint32_t i) {
    if (i < RX_RC_ORIG) return U32(act, 0x2d0 + 4 * i);
    const rx_addr r = i < RX_RC_MAX ? rx_rc_act(act, 0) : 0;
    return r ? U32(r, RX_RC_ACT_STAND + 4 * (i - RX_RC_ORIG)) : 0;
}
static inline void rx_stand_set(rx_addr act, uint32_t i, rx_addr d) {
    if (i < RX_RC_ORIG) { SU32(act, 0x2d0 + 4 * i, d); return; }
    const rx_addr r = i < RX_RC_MAX ? rx_rc_act(act, d != 0) : 0;
    if (r) SU32(r, RX_RC_ACT_STAND + 4 * (i - RX_RC_ORIG), d);
}
/* ---- an activity's race index flags (+0x304, 12: the driver's +0x148 taken) ---- */
static inline uint32_t rx_raceidx_taken(rx_addr act, uint32_t k) {
    if (k < RX_RC_ORIG) return U32(act, 0x304 + 4 * k);
    const rx_addr r = k < RX_RC_MAX ? rx_rc_act(act, 0) : 0;
    return r ? U32(r, RX_RC_ACT_RACEIDX + 4 * (k - RX_RC_ORIG)) : 0;
}
static inline void rx_raceidx_set(rx_addr act, uint32_t k, uint32_t v) {
    if (k < RX_RC_ORIG) { SU32(act, 0x304 + 4 * k, v); return; }
    const rx_addr r = k < RX_RC_MAX ? rx_rc_act(act, v != 0) : 0;
    if (r) SU32(r, RX_RC_ACT_RACEIDX + 4 * (k - RX_RC_ORIG), v);
}

/* ---- an activity's start slots (+0x340 + 0x30 z, 28 inline to +0x880: +0 position, +0x10 direction, +0x20 section, +0x24 node,
 * +0x28 users, +0x2c placed): slots 28..31 in the activity's record (RX_RC_ACT_SLOTS), made on first use ---- */
#define RX_RC_START_ORIG 28u
static inline rx_addr rx_start_slot(rx_addr act, uint32_t z) {
    if (z < RX_RC_START_ORIG) return act + 0x340 + 0x30 * z;
    const rx_addr r = z < RX_RC_MAX ? rx_rc_act(act, 1) : 0;
    return r ? r + RX_RC_ACT_SLOTS + 0x30 * (z - RX_RC_START_ORIG) : act + 0x340;
}
/* the race index flags 12..31 cleared (Activity::vf2's +0x304 .. +0x334 loop), without making a record */
static inline void rx_raceidx_clear_side(rx_addr act) { for (uint32_t k = RX_RC_ORIG; k < RX_RC_MAX; k++) rx_raceidx_set(act, k, 0); }

/* ---- a scenario cast's entries (the level cast / podium, world + 0x20150: +0x50 + 0x30 i, 12; +0 the entity, +4 the player) ----
 * entries 12..31 in the cast's own record (RX_RC_CAST); an entry that does not exist reads as zero */
#define RX_RC_CAST 0x100u
static inline rx_addr rx_cast_rec(rx_addr cast, uint32_t i) {
    if (i < RX_RC_ORIG) return cast + 0x50 + 0x30 * i;
    const rx_addr r = i < RX_RC_MAX ? rx_rc_act(cast, 1) : 0;
    return r ? r + RX_RC_CAST + 0x30 * (i - RX_RC_ORIG) : 0;
}
/* the same, without making a record (0 when entry i >= 12 does not exist: read as an empty entry) */
static inline rx_addr rx_cast_rec_ro(rx_addr cast, uint32_t i) {
    if (i < RX_RC_ORIG) return cast + 0x50 + 0x30 * i;
    const rx_addr r = i < RX_RC_MAX ? rx_rc_act(cast, 0) : 0;
    return r ? r + RX_RC_CAST + 0x30 * (i - RX_RC_ORIG) : 0;
}
static inline uint32_t rx_cast_u32(rx_addr cast, uint32_t i, uint32_t off) { const rx_addr r = rx_cast_rec_ro(cast, i); return r ? U32(r, off) : 0; }

/* ---- the highlight clip recorder (anim_scenario_clip.c, build_rider_clip.c) ----
 * The sample buffer [0x00a7f124]: 0x5b68 records of 0x50 = 13 sections ([0x00a942d8] = 0x708 records: 12 riders x 150 samples
 * [0x00a942dc]); section 0 the ring, 1..12 the clip slots'. A record index (as the replay follow keeps it, sim +0x2e84 ..) below 0x5b68
 * is the original's; riders 12..31 get indices from 0x5b68 up: the side buffer RX_RC_CLIP of 13 sections x 20 riders x 150 samples.
 * The field snapshots (12 rider entries of 0x4c: the candidate's at 0xa959b0, a clip slot's at +0xc) keep entries 12..31 in their own
 * record (keyed by the entries' address, at RX_RC_CLIPENT). */
#define RX_RC_CLIP      (RX_RC_BASE + 0x1000000u)   /* 13 x 20 x 150 x 0x50 = 0x2f9b80 bytes */
#define RX_RC_CLIP_REC0 0x5b68u
#define RX_RC_CLIP_SAMPLES 150u
static inline uint32_t rx_clip_index(uint32_t section, uint32_t rider, uint32_t sample) {
    if (rider < RX_RC_ORIG) return U32(0, 0x00a942d8) * section + U32(0, 0x00a942dc) * rider + sample;
    return RX_RC_CLIP_REC0 + RX_RC_CLIP_SAMPLES * RX_RC_SIDE * section + RX_RC_CLIP_SAMPLES * (rider - RX_RC_ORIG) + sample;
}
static inline rx_addr rx_clip_record(uint32_t index) {
    return (int32_t)index < (int32_t)RX_RC_CLIP_REC0 ? U32(0, 0x00a7f124) + 0x50u * index : RX_RC_CLIP + 0x50u * (index - RX_RC_CLIP_REC0);   /* (signed: a -1 index reads before the buffer, as the original) */
}
#define RX_RC_CLIPENT 0x100u
static inline rx_addr rx_clip_entry(rx_addr entries, uint32_t i, int create) {
    if (i < RX_RC_ORIG) return entries + 0x4c * i;
    const rx_addr r = i < RX_RC_MAX ? rx_rc_act(entries, create) : 0;
    return r ? r + RX_RC_CLIPENT + 0x4c * (i - RX_RC_ORIG) : 0;
}

#ifdef __cplusplus
}
#endif
