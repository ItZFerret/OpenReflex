/* world_wreck_audio.c -- the AudioCollisionManager (0x0067axxx-0x0067dxxx): the collision sounds of the physics world's contact
 * events, kept in a std::map of 0x70-byte records keyed by the two objects; the per-rider sound slots (0x006817c0 / 0x00681840 /
 * 0x0068bfb0 / 0x0068c360).
 *
 * Manager: +8 / +0xc the sound tables (crowd / ambience banks), +0x10 / +0x14 "missing table" flags, +0x1c / +0x20 the strength
 * range, +0x24 / +0x28 / +0x2c the rider crash thresholds, +0x30 the map lock, +0x48 the map (+0 container proxy, +0x18 head,
 * +0x1c size), +0x60 the map's end iterator, +0x68 a stolen flag, +0x70 / +0x74 / +0x78 the free record pool (array, count,
 * capacity), +0x7c its lock, +0x98 the event system, +0xac / +0xb0 its free events.
 * Record (0x70 bytes, from a contact event): +0 / +4 the two objects, +0x10 position, +0x20 type byte, +0x24 / +0x28 strengths
 * (A / B), +0x2c output, +0x30 / +0x34 feature ids, +0x40 / +0x44 last time / frame, +0x48 / +0x4c start time / frame, +0x50
 * the roll sound id, +0x54, +0x58 / +0x5c the impact / roll events.
 * Map nodes: +0 left, +4 parent, +8 right, +0xc the record, +0x10 colour, +0x11 nil.
 * Rider sound banks: 12 banks of 0x1b10 bytes at [0xd92f00] (+0x10 the rider, +0xa70 the slot block: +0x14 / +0x18 ready,
 * slots of 0x1d4 bytes: +0x5a0 the event, +0x754 / +0x758 flags, +0x10 + 4 * 0x1d4 the position source; +0x109c slot 5 on). */
#include "phys_rigbody.h"
#include "phys_terrain.h"
#include "phys_coll_misc.h"
#include "world_wreck.h"
#include "crt.h"
#include "world_util.h"
#include "world_node.h"
#include "world_misc.h"
#include "world_veh.h"
#include "race.h"
#include "sound_state.h"

#define IAT_ENTER_CS   0x0091600cu
#define IAT_LEAVE_CS   0x009160f8u
#define IAT_TIME       0x0091601cu
#define IAT_THREAD_ID  0x0091607cu
static void crt_abort(void) { crt_invalid_parameter_noinfo(); }
static void enter_cs(rx_addr cs) { WORLD_ORIG(U32(0, IAT_ENTER_CS), RX_STDCALL, 0, cs); }
static void leave_cs(rx_addr cs) { WORLD_ORIG(U32(0, IAT_LEAVE_CS), RX_STDCALL, 0, cs); }
/* an object's type id: vfunc +0x58 */
static uint32_t type_id(rx_addr obj, world_ctx c) {
    c.ecx = obj;
    return WORLD_CTX0(U32(U32(obj, 0), 0x58), &c, 0);
}
static int node_nil(rx_addr node) { return U8(node, 0x11) != 0; }

/* the key order of two records (a < b): by the first object, then the second; two contacts on the same object of type 0x100f
 * (the terrain) also by the type byte +0x20 */
static int key_less(rx_addr a, rx_addr b, world_ctx c) {
    const rx_addr obj = U32(a, 4);
    if (obj != 0 && obj == U32(b, 4) && type_id(obj, c) == 0x100f) {
        const uint8_t ta = U8(a, 0x20), tb = U8(b, 0x20);
        if (ta != tb) return ta < tb;
    }
    if (U32(b, 0) > U32(a, 0)) return 1;
    if (U32(b, 0) < U32(a, 0)) return 0;
    return U32(a, 4) < U32(b, 4);
}

/* 0067ac80 CollisionKey_Less (ESI = a, EDI = b; RET 0; EAX = 0 / 1). */
int rx_world_collision_key_less(rx_addr a, rx_addr b) { return key_less(a, b, WCTX(.esi = a, .edi = b)); }
RX_FUNCTION(0x0067ac80, rx_world_collision_key_less)

/* 0068fc80 CollisionMap_IterNext (ESI = iterator {container, node}; RET 0): ++iterator (the in-order successor), checked. */
void rx_world_collision_map_iter_next(rx_addr it) {
    world_frame f = world_frame_flat(1, 0);
    if (U32(it, 0) == 0) crt_abort();
    const rx_addr node = U32(it, 4);
    if (node_nil(node)) { crt_abort(); world_frame_leave(&f); return; }
    rx_addr r = U32(node, 8);
    if (!node_nil(r)) {
        for (rx_addr l = U32(r, 0); !node_nil(l); l = U32(r, 0)) r = l;
        SU32(it, 4, r);
    } else {
        rx_addr p = U32(node, 4);
        while (!node_nil(p) && U32(it, 4) == U32(p, 8)) { SU32(it, 4, p); p = U32(p, 4); }
        SU32(it, 4, p);
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0068fc80, rx_world_collision_map_iter_next)

/* 0068e130 CollisionMap_Insert (stdcall map, out {iterator, inserted}, key (the address of a record pointer); RET 0xc): insert-or-
 * find. The tree walk keeps the last node and the "go left" flag in its frame; the flag's dword (the byte and 3 stale bytes) is
 * what 005860e0 (insert at a hint, EDI = map) receives, as in the original. 00586c10 steps an iterator back. */
void rx_world_collision_map_insert(rx_addr map, rx_addr out, rx_addr key) {
    const rx_addr sp0 = rx_sp();
    const rx_addr P = (((sp0 - 4) & ~7u) - 0x14) - 12;   /* push ebp; and esp, -8; sub esp, 0x14; 3 pushes */
    rx_set_sp(P);
    const rx_addr head = U32(map, 0x18);
    rx_addr x = U32(head, 4), y = head;
    int left = 1;
    SU32(P, 0x14, head);
    SU8(P, 0x10, 1);
    while (!node_nil(x)) {
        const rx_addr rec = U32(key, 0), other = U32(x, 0xc);
        SU32(P, 0x14, x);
        left = key_less(rec, other, WCTX(.esi = rec, .edi = other, .ebx = x));
        SU8(P, 0x10, (uint8_t)left);
        x = left ? U32(x, 0) : U32(x, 8);
    }
    y = U32(P, 0x14);
    const rx_addr container = U32(map, 0);
    SU32(P, 0x1c, y);
    SU32(P, 0x18, container);
    rx_addr hint_container = container;
    int insert = 0;
    uint32_t flag = 1;
    if (left) {
        const rx_addr leftmost = U32(U32(map, 0x18), 0);
        if (container == 0) crt_abort();
        if (y == leftmost) insert = 1;
        else {
            WORLD_CALL_SP(0, rx_world_tree_iterator_decrement(P + 0x18));   /* --iterator 00586c10 */
            hint_container = U32(P, 0x18);
        }
    }
    if (!insert) {
        const rx_addr rec = U32(key, 0), prev = U32(U32(P, 0x1c), 0xc);
        if (key_less(prev, rec, WCTX(.esi = prev, .edi = rec, .ecx = key, .edx = U32(P, 0x1c)))) {
            insert = 1;
            flag = U32(P, 0x10);
        }
    }
    if (insert) {
        rx_addr it;
        WORLD_CALL_SP(0xc, it = rx_world_tree_insert_at(y, map, P + 0x18, flag, key));   /* 005860e0 */
        const uint32_t c = U32(it, 0), n = U32(it, 4);
        SU32(out, 4, n); SU32(out, 0, c); SU8(out, 8, 1);
    } else {
        SU32(out, 0, hint_container); SU32(out, 4, U32(P, 0x1c)); SU8(out, 8, 0);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0068e130, rx_world_collision_map_insert)

/* the rider's sound bank (0..11), or -1 */
static int32_t rider_bank(rx_addr rider) {
    const rx_addr banks = U32(0, 0x00d92f00);
    for (int32_t i = 0; i < 12; i++)
        if (rider == U32(banks, 0x10 + 0x1b10u * (uint32_t)i)) return i;
    return -1;
}

/* 0068c360 (stdcall slot block, slot; EBP = the caller's, passed on; RET 8): clears the slot's flags and stops its event (0080ae10 on the audio thread
 * 0xd67d1c, else the queued stop 0080c280); slot 5 also clears +0x109c. */
void rx_world_rider_sound_slot_stop(rx_addr block, uint32_t slot, uint32_t ebp) {
    if (U32(block, 0x14) == 0 || U32(block, 0x18) == 0) return;
    world_frame f = world_frame_flat(3, 0);
    const rx_addr s = slot * 0x1d4 + block;
    SU32(s, 0x758, 0); SU32(s, 0x754, 0);
    const rx_addr event = s + 0x5a0;
    const uint32_t audio_thread = U32(0, 0x00d67d1c);
    if (audio_thread == WORLD_CTX0(U32(0, IAT_THREAD_ID), &WCTX(.ebx = block, .esi = event, .edi = audio_thread), 0))
        WORLD_CALL_SP(0, rx_sound_stop_now(event));   /* 0080ae10 (ESI = the event) */
    else WORLD_CALL_SP(0x10, rx_world_event_queue(1, event, 0, 0, ebp));   /* 0080c280 */
    if (slot == 5) SU32(block, 0x109c, 0);
    world_frame_leave(&f);
}
RX_FUNCTION(0x0068c360, rx_world_rider_sound_slot_stop)

/* 00681840 (ESI = rider, EBP = the caller's, passed on; cdecl slot): stops the rider's sound slot when the rider has a bank. */
void rx_world_rider_sound_stop(rx_addr rider, uint32_t slot, uint32_t ebp) {
    const int32_t i = rider_bank(rider);
    if (i < 0) return;
    WORLD_CALL_SP(8, rx_world_rider_sound_slot_stop(U32(0, 0x00d92f00) + 0x1b10u * (uint32_t)i + 0xa70, slot, ebp));
}
RX_FUNCTION(0x00681840, rx_world_rider_sound_stop)

/* 0068bfb0 (EDI = slot block; RET 0; EAX): the position a rider's sounds follow: the bike's active wheel contact (+0x60 of the
 * current part, a 0x1002 object with +0x2f4), else the rider's +0x200 (with a player) or +0x1c0. */
rx_addr rx_world_rider_sound_source(rx_addr block) {
    world_frame f = world_frame_flat(1, 0);
    const rx_addr bike = U32(U32(block, 0x14), 0x190);
    if (bike != 0) {
        const rx_addr part = U32(bike, U32(bike, 0x988) * 4 + 0x974);
        if (part != 0 && type_id(part, WCTX(.esi = part, .edi = block)) == 0x1002 && U32(part, 0x2f4) != 0 && part + 0x60 != 0) {
            world_frame_leave(&f);
            return part + 0x60;
        }
    }
    world_frame_leave(&f);
    const rx_addr rider = U32(block, 0x18);
    if (U32(rider, 0x1f0) != 0 && rider + 0x200 != 0) return rider + 0x200;
    return rider + 0x1c0;
}
RX_FUNCTION(0x0068bfb0, rx_world_rider_sound_source)

/* 006817c0 (ESI = rider; cdecl slot): starts the rider's sound slot: flag +0x758, the position source, slot 5 also +0x109c. */
void rx_world_rider_sound_start(rx_addr rider, uint32_t slot) {
#ifndef RX_ORACLE
    rx_sound_rider_sound(rider, slot);   /* w-sfx: the slot for the host's foley emitter (game/sound_state.c; the product has no rider banks) */
#endif
    const int32_t i = rider_bank(rider);
    if (i < 0) return;
    const rx_addr bank = U32(0, 0x00d92f00) + 0x1b10u * (uint32_t)i;
    if (U32(bank, 0xa84) == 0) return;
    const rx_addr block = bank + 0xa70;
    if (U32(block, 0x18) == 0) return;
    SU32(block, slot * 0x1d4 + 0x758, 1);
    world_frame f = world_frame_flat(2, 0);
    rx_addr src;
    WORLD_CALL_SP(0, src = rx_world_rider_sound_source(block));
    world_frame_leave(&f);
    SU32(block, (slot + 4) * 0x1d4, src);
    if (slot == 5) SU32(block, 0x109c, 1);
}
RX_FUNCTION(0x006817c0, rx_world_rider_sound_start)

/* 0067d310 AudioTable_Name (ECX = sound table, EAX = id; RET 0; EAX): the sound name of an id in the table's map (+0x90,
 * 00690e00 into a local), else of the default id (0x931eac); 0 when neither. */
rx_addr rx_world_audio_table_name(rx_addr table, uint32_t id) {
    world_frame f = world_frame_flat(3, 0);
    const rx_addr P = f.esp;
    SU32(P, 8, id);
    SU32(P, 4, 0);
    rx_addr name = 0;
    uint32_t found;
    WORLD_CALL_SP(4, found = phys_hash_lookup(table + 0x90, P + 8, P + 4));
    if (found != 0) name = U32(U32(P, 4), 0);
    else {
        WORLD_CALL_SP(4, found = phys_hash_lookup(table + 0x90, 0x00931eac, P + 4));
        if (found != 0) name = U32(U32(P, 4), 0);
    }
    world_frame_leave(&f);
    return name;
}
RX_FUNCTION(0x0067d310, rx_world_audio_table_name)

/* 0067b890 AudioCollision_SoundName (ESI = manager, EAX = sound id; RET 0; EAX): the sound name from the manager's table for the
 * current bank (+8 when the track's active table has the crowd bank, else +0xc; a tail call of 0067d310); without that table: 0,
 * and the matching "missing" flag (+0x10 / +0x14) is set when 0xa9423c. */
rx_addr rx_world_audio_collision_sound_name(rx_addr mgr, uint32_t id) {
    const rx_addr track = U32(0, 0x00a946b8);
    rx_addr t = 0;
    if (track != 0) {
        const rx_addr modes = U32(0, 0x00a95844);
        if (U32(U32(modes, 0x20), 0x34) == 1) t = U32(track, 0x5a54);
        else if (U32(U32(modes, 0x1c), 0x34) == 1) t = U32(track, 0x20c8);
    }
    const int crowd = t != 0 && U32(t, 0x20018) != 0;
    const rx_addr table = U32(mgr, crowd ? 8 : 0xc);
    if (table != 0) return rx_world_audio_table_name(table, id);   /* a tail call: same stack pointer */
    if (U32(0, 0x00a9423c) != 0) SU32(mgr, crowd ? 0x10 : 0x14, 1);
    return 0;
}
RX_FUNCTION(0x0067b890, rx_world_audio_collision_sound_name)

/* the length of a C string */
static uint32_t str_len(rx_addr s) { uint32_t n = 0; while (U8(s, n) != 0) n++; return n; }
static int str_equal(rx_addr a, rx_addr b) {
    for (uint32_t i = 0;; i++) {
        if (U8(a, i) != U8(b, i)) return 0;
        if (U8(a, i) == 0) return 1;
    }
}

/* 0067c930 AudioCollision_EventIsOurs (EAX = record; RET 0; EAX = 0 / 1): its impact event (+0x58) has a name (+0x18) and its
 * bank name (+0x134) is the collision bank (0x95b800, the empty string). */
int rx_world_audio_collision_event_is_ours(rx_addr rec) {
    const rx_addr ev = U32(rec, 0x58);
    if (str_len(ev + 0x18) == 0) return 0;
    return str_equal(U32(ev, 0x134), 0x0095b800);
}
RX_FUNCTION(0x0067c930, rx_world_audio_collision_event_is_ours)

/* x87 pass-through copy of a float field */
static void copy_f32(rx_addr dst, rx_addr src) { SU32(dst, 0, world_x87_pass(U32(src, 0))); }

/* 0067bb10 AudioCollision_Add (EAX = manager; stdcall contact event (64 bytes); RET 4): a record from the free pool (+0x70, lock
 * +0x7c) is filled from the event (objects, position, type, strengths, output, feature ids; start time / frame; no sounds), then
 * inserted into the map (lock +0x30; the key is the address of the original's argument slot, which holds the record). Its stored
 * record gets the time / frame and the strengths: within the same frame the stronger contact's position and the larger
 * strengths, else replaced. A new key plays it (0067bd30); an existing one returns the record to the pool and updates the stored
 * one (0067c2c0). */
void rx_world_audio_collision_add(rx_addr mgr, rx_addr event) {
    const rx_addr slot = rx_sp() + 4;
    world_frame f = world_frame_flat(4, 0x18);
    const rx_addr P = f.esp;
    const rx_addr pool_cs = mgr + 0x7c;
    rx_addr rec = 0;
    enter_cs(pool_cs);
    if (U32(mgr, 0x74) != 0) {
        const uint32_t n = U32(mgr, 0x74) - 1;
        SU32(mgr, 0x74, n);
        rec = U32(U32(mgr, 0x70), n * 4);
    }
    leave_cs(pool_cs);
    SU32(slot, 0, rec);
    if (rec == 0) { world_frame_leave(&f); return; }
    crt_memset(rec, 0, 0x70);   /* memset (thunk 0x8fa06a) */
    SU32(rec, 0, U32(event, 0));
    SU32(rec, 4, U32(event, 4));
    copy_f32(rec + 0x10, event + 0x10); copy_f32(rec + 0x14, event + 0x14); copy_f32(rec + 0x18, event + 0x18);
    SU8(rec, 0x20, U8(event, 0x20));
    copy_f32(rec + 0x24, event + 0x24); copy_f32(rec + 0x28, event + 0x28);
    SU32(rec, 0x2c, U32(event, 0x2c));
    SU32(rec, 0x30, U32(event, 0x30));
    SU32(rec, 0x34, U32(event, 0x34));
    SU32(rec, 0x48, WORLD_CTX0(U32(0, IAT_TIME), &WCTX(.ebx = rec, .ebp = event, .esi = pool_cs, .edi = mgr), 0));
    SU32(rec, 0x4c, U32(U32(U32(0, 0x00a95844), 0x10), 0x58));
    SU32(rec, 0x50, 0xffffffffu); SU32(rec, 0x54, 0xffffffffu);
    const rx_addr map_cs = mgr + 0x30;
    enter_cs(map_cs);
    WORLD_CALL_SP(0xc, rx_world_collision_map_insert(mgr + 0x48, P + 0x1c, slot));
    const uint32_t it0 = U32(P, 0x1c), it1 = U32(P, 0x20), inserted = U32(P, 0x24);
    SU32(P, 0x10, it0); SU32(P, 0x14, it1); SU32(P, 0x18, inserted);
    leave_cs(map_cs);
    rx_addr container = 0;
    if (U32(P, 0x10) != 0) container = U32(U32(P, 0x10), 0);
    else crt_abort();
    if (U32(P, 0x14) == U32(container, 0x18)) crt_abort();
    const rx_addr stored = U32(U32(P, 0x14), 0xc);
    SU32(stored, 0x40, WORLD_CTX0(U32(0, IAT_TIME), &WCTX(.ebx = rec, .ebp = event, .esi = stored, .edi = mgr), 0));
    const uint32_t frame = U32(U32(U32(0, 0x00a95844), 0x10), 0x58);
    SU32(stored, 0x44, frame);
    if (frame == U32(stored, 0x4c)) {
        const float ea = F32(event, 0x24);
        int take_pos = ea > F32(stored, 0x24);
        if (!take_pos) {
            float eb = F32(event, 0x28);
            if (!(eb > rx_bf(0))) eb = ea;
            take_pos = eb > F32(stored, 0x28);
        }
        if (take_pos) { copy_f32(stored + 0x10, event + 0x10); copy_f32(stored + 0x14, event + 0x14); copy_f32(stored + 0x18, event + 0x18); }
        float a = F32(stored, 0x24);
        if (!(a > F32(event, 0x24))) a = F32(event, 0x24);
        SF32(stored, 0x24, a);
        float b = F32(event, 0x28);
        if (!(b > rx_bf(0))) b = F32(event, 0x24);
        if (F32(stored, 0x28) > b) b = F32(stored, 0x28);
        SF32(stored, 0x28, b);
    } else {
        copy_f32(stored + 0x24, event + 0x24);
        float b = F32(event, 0x28);
        if (!(b > rx_bf(0) /* 0x92e12c */)) b = F32(event, 0x24);
        SF32(stored, 0x28, b);
        copy_f32(stored + 0x10, event + 0x10); copy_f32(stored + 0x14, event + 0x14); copy_f32(stored + 0x18, event + 0x18);
    }
    if (inserted & 0xff) {
        WORLD_CALL_SP(8, rx_world_audio_collision_play(mgr, stored));
        world_frame_leave(&f);
        return;
    }
    if (I32(mgr, 0x74) < I32(mgr, 0x78)) {
        enter_cs(pool_cs);
        SU32(U32(mgr, 0x70), U32(mgr, 0x74) * 4, rec);
        SU32(mgr, 0x74, U32(mgr, 0x74) + 1);
        leave_cs(pool_cs);
    }
    WORLD_CALL_SP(0, rx_world_audio_collision_update(stored, mgr));
    world_frame_leave(&f);
}
RX_FUNCTION(0x0067bb10, rx_world_audio_collision_add)

/* 0067bac0 AudioCollisionManager::vf0 (thiscall manager; stdcall the world's contact events (64 bytes each, count +0x1900);
 * RET 4): every event added (0067bb10) under the map lock. */
void rx_world_audio_collision_manager_vf0(rx_addr mgr, rx_addr events) {
    world_frame f = world_frame_flat(4, 4);
    const rx_addr cs = mgr + 0x30;
    SU32(f.esp, 0x10, cs);
    enter_cs(cs);
    for (uint32_t i = 0; i < U32(events, 0x1900); i++) WORLD_CALL_SP(4, rx_world_audio_collision_add(mgr, events + 0x40 * i));
    leave_cs(U32(f.esp, 0x10));
    world_frame_leave(&f);
}
RX_FUNCTION(0x0067bac0, rx_world_audio_collision_manager_vf0)

/* 0067c2c0 AudioCollision_Update (ESI = record, EDI = manager; RET 0): a terrain contact (type byte +0x20) whose impact sound has
 * ended (no event, or not ours) is replayed on a 0x100f object (0067bd30); a 0x10 object updates its roll sound (0067c350); a
 * type 5 object with +0x19c its own (0067c670). */
void rx_world_audio_collision_update(rx_addr rec, rx_addr mgr) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp((sp0 - 4) & ~7u);   /* push ebp; mov ebp, esp; and esp, -8 */
    const world_ctx c = WCTX(.esi = rec, .edi = mgr);
    if (U8(rec, 0x20) != 0) {
        int ended = U32(rec, 0x58) == 0;
        if (!ended) { int ours; WORLD_CALL_SP(0, ours = rx_world_audio_collision_event_is_ours(rec)); ended = !ours; }
        if (ended && U32(rec, 4) != 0 && type_id(U32(rec, 4), c) == 0x100f) {
            SU32(rec, 0x58, 0);
            WORLD_CALL_SP(8, rx_world_audio_collision_play(mgr, rec));
            rx_set_sp(sp0);
            return;
        }
    }
    if (U32(rec, 4) != 0 && type_id(U32(rec, 4), c) == 0x10) {
        WORLD_CALL_SP(4, rx_world_audio_collision_update_roll(mgr, rec));
        rx_set_sp(sp0);
        return;
    }
    if (U32(rec, 4) != 0 && type_id(U32(rec, 4), c) == 5 && U8(U32(rec, 4), 0x19c) != 0)
        AUDIO_BOUNDARY_CTX(0x0067c670, &WCTX(.ecx = mgr, .esi = rec, .edi = mgr), 0, rec);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0067c2c0, rx_world_audio_collision_update)

/* an impact strength mapped into 0..10 between the manager's range (+0x1c .. +0x20) */
static float strength_level(float s, float lo, float hi) {
    const float v = phys_mul(phys_div(phys_sub(s, lo), phys_sub(hi, lo)), 10.0f /* 0x92fd88 */);
    if (v >= 10.0f) return 10.0f;
    return !(rx_bf(0) >= v) ? v : rx_bf(0);   /* a NaN passes */
}
/* clamped to 0..1 as the original's comparisons leave it (a NaN passes) */
static float clamp01(float t) {
    if (t >= 1.0f) return 1.0f;
    return !(rx_bf(0) >= t) ? t : rx_bf(0);
}
/* a 0x1029 (physics entity) or type 5 object */
static int heavy(rx_addr rec, uint32_t side, world_ctx c) {
    if (type_id(U32(rec, 4 * side), c) == 0x1029) return 1;
    return type_id(U32(rec, 4 * side), c) == 5;
}

/* 0067bd30 AudioCollision_Play (stdcall manager, record; RET 8): plays a new collision. Both objects are classified (0x1082
 * bikes, 0x100f terrain, 0x13 / 0x31 riders, 0x10 rolling, 0xf), each side's sound id (a bike's vfunc +0xd8 by its surface index
 * +0x30 / +0x34, else vfunc +0x30) and material (+0x34) chosen; the strengths scaled by the class factors (0x9ca5d0 / 0x9ca5fc /
 * 0x9ca654 / 0x9ca6ac), equalized between two 0x1029 / 5 objects, and mapped to 0..10; the riders' crash slots (006817c0, kind 3
 * / 4 by the manager's thresholds) kicked, and the events started: per side its name (0067b890), a free event (0067c990 steals
 * one when none), 0080c760 / 0080a9f0, the strength parameter 0x95c398 (0080afc0) and play (0080abd0) at the position. */
void rx_world_audio_collision_play(rx_addr mgr, rx_addr rec) {
    const rx_addr sp0 = rx_sp();
    const rx_addr P = (((sp0 - 4) & ~7u) - 0x3c) - 12;   /* push ebp; and esp, -8; sub esp, 0x3c; 3 pushes */
    rx_set_sp(P);
    const world_ctx c = WCTX(.ebx = rec);
    const rx_addr a = U32(rec, 0), b = U32(rec, 4);
    const int bike_a = a != 0 && type_id(a, c) == 0x1082, bike_b = b != 0 && type_id(b, c) == 0x1082;
    SU32(P, 0x28, (uint32_t)bike_a); SU32(P, 0x2c, (uint32_t)bike_b);
    if (a != 0) type_id(a, c);
    const int terrain_b = b != 0 && type_id(b, c) == 0x100f;
    SU32(P, 0x3c, (uint32_t)terrain_b);
    int rider[2];
    for (int side = 0; side < 2; side++) {
        const rx_addr o = U32(rec, 4u * (uint32_t)side);
        rider[side] = 0;
        if (o != 0 && type_id(o, c) == 0x13) {
            world_ctx cc = c; cc.ecx = o;
            rider[side] = WORLD_CTX0(U32(U32(o, 0), 0x30), &cc, 0) == 0x31;
        }
        SU32(P, 0x30 + 4u * (uint32_t)side, (uint32_t)rider[side]);
    }
    const int roll_a = U32(rec, 0) != 0 && type_id(U32(rec, 0), c) == 0x10;
    const int roll_b = U32(rec, 4) != 0 && type_id(U32(rec, 4), c) == 0x10;
    for (int side = 0; side < 2; side++) {
        const rx_addr o = U32(rec, 4u * (uint32_t)side);
        SU32(P, 0x40 + 4u * (uint32_t)side, (uint32_t)(o != 0 && type_id(o, c) == 0xf));
    }
    for (int side = 0; side < 2; side++) {
        const uint32_t surface = U32(rec, 0x30 + 4u * (uint32_t)side);
        const int by_index = U32(P, 0x28 + 4u * (uint32_t)side) != 0 && (int32_t)surface >= 0;
        const rx_addr o = U32(rec, 4u * (uint32_t)side);
        world_ctx cc = c; cc.ecx = o;
        uint32_t id = 0;
        if (o != 0) id = by_index ? WORLD_CTX(U32(U32(o, 0), 0xd8), &cc, 0, surface) : WORLD_CTX0(U32(U32(o, 0), 0x30), &cc, 0);
        SU32(P, 0x18 + 4u * (uint32_t)side, id);
        const rx_addr o2 = U32(rec, 4u * (uint32_t)side);
        cc.ecx = o2;
        SU32(P, 0x20 + 4u * (uint32_t)side, o2 != 0 ? WORLD_CTX0(U32(U32(o2, 0), 0x34), &cc, 0) : 5);
    }
    if (U32(rec, 4) == 0 || U32(P, 0x3c) != 0) SU32(P, 0x1c, U32(rec, 0x2c));
#define SCALE(o, k) SF32(rec, o, phys_mul(F32(0, k), F32(rec, o)))
    if (U8(rec, 0x20) != 0 && U32(P, 0x3c) != 0) { SCALE(0x24, 0x009ca5d0); SCALE(0x28, 0x009ca5d0); }
    if (U32(P, 0x30) != 0) { SCALE(0x24, 0x009ca5fc); SCALE(0x28, 0x009ca5fc); }
    if (U32(P, 0x28) != 0 && roll_b) SCALE(0x24, 0x009ca654);
    if (U32(P, 0x2c) != 0 && roll_a) SCALE(0x28, 0x009ca654);
    if (U32(P, 0x28) != 0 && U32(P, 0x2c) != 0) { SCALE(0x24, 0x009ca6ac); SCALE(0x28, 0x009ca6ac); }
#undef SCALE
    if (U32(rec, 4) != 0) {
        if (heavy(rec, 0, c) && heavy(rec, 1, c)) {
            float s = F32(rec, 0x24);
            if (!(s > F32(rec, 0x28))) s = F32(rec, 0x28);
            SF32(rec, 0x24, s); SF32(rec, 0x28, s);
        }
        if ((U32(P, 0x30) != 0 || U32(P, 0x28) != 0) && heavy(rec, 1, c)) {
            float s = F32(rec, 0x24);
            if (!(s > F32(rec, 0x28))) s = F32(rec, 0x28);
            SF32(rec, 0x28, s);
        }
    }
    SF32(P, 0x38, strength_level(F32(rec, 0x24), F32(mgr, 0x1c), F32(mgr, 0x20)));
    SF32(P, 0x3c, strength_level(F32(rec, 0x28), F32(mgr, 0x1c), F32(mgr, 0x20)));
    SU32(P, 0x10, 0);
    for (uint32_t k = 0; k < 2; k++) {
        SU32(P, 0x10, k);
        int skip;
        if (U8(rec, 0x20) != 0) skip = k == 0 || (k == 1 && U32(P, 0x20) == 0);
        else skip = k == 0 ? U32(P, 0x24) == 0 : (k == 1 && U32(P, 0x20) == 0);
        if (skip) continue;
        const float level = F32(P, 0x38 + 4 * k);
        if (U32(P, 0x28 + 4 * k) != 0) {
            const rx_addr bike = U32(rec, 4 * k);
            if (U32(bike, 0x194) <= 1) {
                if (level >= F32(mgr, 0x28)) WORLD_CALL_SP(4, rx_world_rider_sound_start(bike, 4));
                else if (level >= F32(mgr, 0x24)) WORLD_CALL_SP(4, rx_world_rider_sound_start(bike, 3));
            }
        }
        if (U32(P, 0x30 + 4 * k) != 0) {
            const rx_addr r = U32(rec, 4 * k);
            if (U32(P, 0x40 + 4 * (k == 0 ? 1u : 0u)) == 0 && U32(r, 0xae70) == 0) {
                uint32_t kind = 0;
                if (U32(r, 0x5bb68) != 0 && level >= F32(mgr, 0x2c)) kind = 4;
                if (kind == 0 && level >= F32(mgr, 0x2c)) kind = 3;
                if (kind != 0) WORLD_CALL_SP(4, rx_world_rider_sound_start(U32(r, 0xaf1c), kind));
            }
        }
        rx_addr name;
        WORLD_CALL_SP(0, name = rx_world_audio_collision_sound_name(mgr, U32(P, 0x18 + 4 * k)));
        if (name != 0 && str_len(name) != 0) {
            if (((U32(mgr, 0xb0) - U32(mgr, 0xac)) & 0xfffffffcu) == 0) AUDIO_BOUNDARY_CTX(0x0067c990, &WCTX(.ebx = rec, .esi = name), 0, mgr);
            rx_addr ev;
            WORLD_CALL_SP(8, ev = rx_world_event_create(mgr + 0x98, name, 0x0095b800));   /* 0080c760 */
            SU32(rec, 0x58, ev);
            if (ev != 0) {
                SU32(ev, 0x10, rec);
                WORLD_CALL_SP(0, rx_world_event_start(U32(rec, 0x58), sp0 - 4));   /* EBP: this frame */
                WORLD_CALL_SP(8, rx_world_event_set_parameter(U32(rec, 0x58), 0x0095c398,
                                                              rx_bf(world_x87_pass(U32(P, 0x38 + 4 * U32(P, 0x10))))));
                WORLD_CALL_SP(0xc, rx_world_event_play(U32(rec, 0x58), rec + 0x10, 0, 0));
            }
        }
        if (U32(P, 0x18) == U32(P, 0x1c)) break;
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0067bd30, rx_world_audio_collision_play)

/* 0067c350 AudioCollision_UpdateRoll (thiscall manager; stdcall record; RET 4): the rolling / scraping sound of a 0x10 contact.
 * Its surface sound id (007c5120 at the position, on the player's water body 00402dc0) and its strength: a rider's speed
 * (|00422350 velocity| * 0x9ca628 * 0x97748c between 0x9ca730 and 0x9ca75c), a 0x1029 object's speed (0041cf30) or a bike's
 * +0x150 (* 0x97748c, their ranges), else the record's +0x24 in the manager's range; clamped to 0..1. A changed sound id stops
 * the old event (+0x5c); then a new event is started (name 00679420, 0080c760 / 0080a9f0, parameter 0x95c3a0, play), or the
 * running one's position (0080ae80) and parameter updated. */
void rx_world_audio_collision_update_roll(rx_addr mgr, rx_addr rec) {
    const rx_addr sp0 = rx_sp();
    const rx_addr P = (((sp0 - 4) & ~15u) - 0x34) - 12;   /* push ebp; and esp, -16; sub esp, 0x34; 3 pushes */
    rx_set_sp(P);
    if (!(U8(0, 0x009e30e0) & 1)) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
    SU32(P, 0x18, mgr);
    rx_addr water;
    WORLD_CALL_SP(0, water = rx_world_find_by_controller(0x009e30d4));
    uint32_t sound;
    { const rx_regs caller = world_ctx_regs(&WCTX(.ebx = rec, .esi = mgr)); WORLD_CALL_SP(0, sound = phys_terrain_material_at(water, rec + 0x10, 3, &caller, NULL)); }
    SU32(P, 0x1c, sound);
    const world_ctx c = WCTX(.ebx = rec, .esi = mgr, .edi = sound);
    const rx_addr obj = U32(rec, 0);
    int done = 0;
    float s = 0, lo = 0, hi = 0;
    int have = 0;
    if (obj != 0) {
        int is_rider = 0;
        if (type_id(obj, c) == 0x13) { world_ctx cc = c; cc.ecx = U32(rec, 0); is_rider = WORLD_CTX0(U32(U32(U32(rec, 0), 0), 0x30), &cc, 0) == 0x31; }
        if (is_rider) {
            WORLD_CALL_SP(0, phys_rigbody_mean_velocity(U32(rec, 0) + 0x950, P + 0x20));
            const float speed = v4_sqrt(v4_splat(v4_len2_xzy(v4_ld(P + 0x20)))).v[0];
            const float lo_r = F32(0, 0x009ca730), hi_r = F32(0, 0x009ca75c);
            const float v = phys_mul(phys_mul(F32(0, 0x009ca628), F32(0, 0x0097748c)), speed);
            const float t = phys_div(phys_sub(v, lo_r), phys_sub(hi_r, lo_r));
            SF32(P, 0x14, clamp01(t));
            done = 1;
        } else if (U32(rec, 0) != 0) {
            const rx_addr o = U32(rec, 0);
            if (type_id(o, c) == 0x1029) {
                WORLD_CALL_SP(4, rx_world_physics_entity_speed(o, P + 0x14));
                s = phys_mul(F32(P, 0x14), F32(0, 0x0097748c));
                lo = F32(0, 0x009ca788); hi = F32(0, 0x009ca7b4);
                have = 1;
            } else if (type_id(o, c) == 0x1082) {
                s = phys_mul(F32(U32(rec, 0), 0x150), F32(0, 0x0097748c));
                lo = F32(0, 0x009ca6d8); hi = F32(0, 0x009ca704);
                have = 1;
            }
        }
    }
    if (!done) {
        if (!have) { lo = F32(mgr, 0x1c); s = F32(rec, 0x24); hi = F32(mgr, 0x20); }
        SF32(P, 0x14, clamp01(phys_div(phys_sub(s, lo), phys_sub(hi, lo))));
    }
    if (sound != U32(rec, 0x50) && U32(rec, 0x5c) != 0) {
        const rx_addr ev = U32(rec, 0x5c);
        const uint32_t audio_thread = U32(0, 0x00d67d1c);
        if (audio_thread == WORLD_CTX0(U32(0, IAT_THREAD_ID), &WCTX(.ebx = rec, .esi = ev, .edi = audio_thread), 0))
            WORLD_CALL_SP(0, rx_sound_stop_now(ev));   /* 0080ae10 (ESI = the event) */
        else WORLD_CALL_SP(0x10, rx_world_event_queue(1, ev, 0, 0, sp0 - 4));   /* 0080c280 (EBP: this frame) */
        SU32(rec, 0x5c, 0);
    }
    const rx_addr ev = U32(rec, 0x5c);
    if (ev == 0) {
        SU32(rec, 0x50, sound);
        rx_addr name;
        WORLD_CALL_SP(0, name = rx_world_crowd_slot(sound));
        if (name != 0 && str_len(name) != 0) {
            if (((U32(mgr, 0xb0) - U32(mgr, 0xac)) & 0xfffffffcu) == 0) AUDIO_BOUNDARY_CTX(0x0067c990, &WCTX(.ebx = rec, .esi = mgr, .edi = name), 0, mgr);
            rx_addr nev;
            WORLD_CALL_SP(8, nev = rx_world_event_create(mgr + 0x98, name, 0x0095b800));   /* 0080c760 */
            SU32(rec, 0x5c, nev);
            if (nev != 0) {
                SU32(nev, 0x10, rec);
                WORLD_CALL_SP(0, rx_world_event_start(nev, sp0 - 4));
                WORLD_CALL_SP(8, rx_world_event_set_parameter(U32(rec, 0x5c), 0x0095c3a0, rx_bf(world_x87_pass(U32(P, 0x14)))));
                WORLD_CALL_SP(0xc, rx_world_event_play(U32(rec, 0x5c), rec + 0x10, 0, 0));
            }
        }
    } else if (U32(ev, 0x10) != rec) SU32(rec, 0x5c, 0);
    else {
        WORLD_CALL_SP(4, rx_world_event_set_position(rec + 0x10, ev, sp0 - 4));
        WORLD_CALL_SP(8, rx_world_event_set_parameter(U32(rec, 0x5c), 0x0095c3a0, rx_bf(world_x87_pass(U32(P, 0x14)))));
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0067c350, rx_world_audio_collision_update_roll)
