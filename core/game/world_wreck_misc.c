/* world_wreck_misc.c -- small virtuals, the crowd / ambience audio helpers (0x00674xxx, 0x00679xxx, 0x0067abc0, 0x0067ecxx,
 * 0x0075c580), the ClipRecorder (0x006aaxxx-0x006abxxx: the replay director's clip candidates and event ring), and assorted
 * 0x006bxxxx-0x0070xxxx functions (an IK fade reset, a vector getter, the timer in seconds, a string-keyed map lookup, the
 * keyframe blend selection).
 *
 * Crowd tables (0xa94234 the crowd bank, 0xa94238 the ambience bank): 8 slots of 0x310 bytes from +0x220 (+0x1dc the level, +0x1e0
 * its event). The crowd excitement: 0xc023a8 / 0xc023ac min / max, 0xc023b0 the start value, 0xc023b8 current, 0xc023c0 target,
 * 0xc023c4 from, 0xc023cc t, 0xc02400 state. Crowd emitters: 0xc02470 list, 0xc02474 count (reaction events +0x234 / +0x3d8 /
 * +0x57c). The clip ring: 100 entries of 128 bytes at 0xd2f160 (+0 time, +4 type, +8 rider id, +0x60 value), cursor 0xa942c8. */
#include "phys_anim_sample.h"
#include "world_wreck.h"
#include "clip_damage_copy.h"
#include "world_misc.h"
#include "world_node.h"
#include "world_util.h"
#include "world_veh.h"
#include "phys_coll_grid.h"
#include "crt.h"
#include "crt_util.h"
#include "sound_state.h"
#include "rider_cap.h"

static void crt_abort(void) { crt_invalid_parameter_noinfo(); }

/* 00679000 SharedVf_Ret4: an empty virtual (thiscall; one argument; RET 4). */
void rx_world_shared_vf_ret4(rx_addr self, uint32_t arg) { (void)self; (void)arg; }
RX_FUNCTION(0x00679000, rx_world_shared_vf_ret4)
/* 00703d50 SharedVf_Type10: a shared type-id virtual, 0x10. */
uint32_t rx_world_shared_vf_type10(rx_addr self) { (void)self; return 0x10; }
RX_FUNCTION(0x00703d50, rx_world_shared_vf_type10)
/* 0065cab0 ForestSystem::vf22: the type id 0x100f. */
uint32_t rx_world_forest_system_vf22(rx_addr self) { (void)self; return 0x100f; }
RX_FUNCTION(0x0065cab0, rx_world_forest_system_vf22)
/* 006d7870 KeyMap::vf11 (thiscall map; key; RET 4): the binding word of a key (36-byte entries from +0x28). */
uint32_t rx_world_key_map_vf11(rx_addr map, uint32_t key) { return U32(map, key * 36 + 0x28); }
RX_FUNCTION(0x006d7870, rx_world_key_map_vf11)

/* The active sound table of a track object: +0x5a54 when the game mode 0x20 is on, +0x20c8 when 0x1c is ([0xa95844] +0x20 /
 * +0x1c records, +0x34 on), else 0. */
static rx_addr active_sound_table(rx_addr track) {
    const rx_addr modes = U32(0, 0x00a95844);
    if (U32(U32(modes, 0x20), 0x34) == 1) return U32(track, 0x5a54);
    if (U32(U32(modes, 0x1c), 0x34) == 1) return U32(track, 0x20c8);
    return 0;
}
/* 0075c580 (EDX = track object; RET 0): the active sound table. */
rx_addr rx_world_active_sound_table(rx_addr track) { return active_sound_table(track); }
RX_FUNCTION(0x0075c580, rx_world_active_sound_table)

/* 0067ec50 / 0067eca0 (RET 0; EAX): whether the current track's (0xa946b8) active sound table has (+0x20018 != 0) / lacks the
 * crowd bank. */
int rx_world_crowd_bank_in_use(void) {
    const rx_addr track = U32(0, 0x00a946b8);
    if (track == 0) return 0;
    const rx_addr t = active_sound_table(track);
    return t != 0 && U32(t, 0x20018) != 0;
}
RX_FUNCTION(0x0067ec50, rx_world_crowd_bank_in_use)
int rx_world_ambience_bank_in_use(void) {
    const rx_addr track = U32(0, 0x00a946b8);
    if (track == 0) return 0;
    const rx_addr t = active_sound_table(track);
    return t != 0 && U32(t, 0x20018) == 0;
}
RX_FUNCTION(0x0067eca0, rx_world_ambience_bank_in_use)
/* the crowd table in use (0xa94234 with the crowd bank, 0xa94238 with the ambience bank), else 0 */
static rx_addr crowd_table(void) {
    int crowd, ambience;
    WORLD_CALL_SP(0, crowd = rx_world_crowd_bank_in_use());
    if (crowd) return U32(0, 0x00a94234);
    WORLD_CALL_SP(0, ambience = rx_world_ambience_bank_in_use());
    return ambience ? U32(0, 0x00a94238) : 0;
}

/* 00679420 (EAX = slot 0..7; RET 0): the slot record (+0x220, 0x310 bytes) of the crowd table in use, else 0. */
rx_addr rx_world_crowd_slot(uint32_t slot) {
    world_frame f = world_frame_flat(1, 0);
    const rx_addr t = crowd_table();
    world_frame_leave(&f);
    if (t == 0 || slot >= 8) return 0;
    return slot * 0x310 + t + 0x220;
}
RX_FUNCTION(0x00679420, rx_world_crowd_slot)

/* 0067abc0 (EAX = slot, EDX = crowd table; RET 0; xmm0): the slot's level (+0x1dc); a level not above 0 is reset to 70 and, for a
 * valid slot, refreshed from its event (+0x1e0: 0080b160 into a local) when that is positive. */
float rx_world_crowd_slot_level(uint32_t slot, rx_addr table) {
    const rx_addr rec = table + slot * 0x310;
    if (!(rx_bf(0) >= F32(rec, 0x1dc))) return F32(rec, 0x1dc);
    SF32(rec, 0x1dc, 70.0f /* 0x931f2c */);
    if (slot >= 8) return F32(rec, 0x1dc);
    const rx_addr event = rec + 0x1e0;
    if (event != 0) {
        world_frame f = world_frame_flat(2, 8);   /* sub esp, 8; push edi; push esi: the level local at [esp + 8] */
        const rx_addr level_slot = f.esp + 8, out2 = f.esp + 0xc;
        SF32(level_slot, 0, rx_bf(0));
        WORLD_CALL_SP(8, (void)rx_world_event_parameter_range(event, out2, level_slot));   /* 0080b160 */
        const float level = F32(level_slot, 0);
        if (level > rx_bf(0) /* 0x92e12c */) SF32(rec, 0x1dc, level);
        world_frame_leave(&f);
    }
    return F32(rec, 0x1dc);
}
RX_FUNCTION(0x0067abc0, rx_world_crowd_slot_level)

/* 00679540 (ESI = slot; RET 0; xmm0): the slot's level from the crowd table in use (a tail call of 0067abc0), else 0. */
float rx_world_crowd_level(uint32_t slot) {
    const rx_addr entry = rx_sp();
    world_frame f = world_frame_flat(1, 0);   /* push ecx: a zero local */
    SU32(f.esp, 0, 0);
    const rx_addr t = crowd_table();
    world_frame_leave(&f);
    if (t == 0) return rx_bf(0);
    rx_set_sp(entry);   /* the tail call runs at the entry stack pointer */
    return rx_world_crowd_slot_level(slot, t);
}
RX_FUNCTION(0x00679540, rx_world_crowd_level)

/* 00674440 (cdecl delta): the crowd excitement: target = clamp(current + delta, min, max); when it changes, a ramp starts from
 * the current value (state 1, t = 0). Lazily initialised (state 4) from the start value. */
void rx_world_crowd_excite(float delta) {
    float cur;
    if (U32(0, 0x00c02400) == 0) { cur = F32(0, 0x00c023b0); SF32(0, 0x00c023b8, cur); SU32(0, 0x00c02400, 4); }
    else cur = F32(0, 0x00c023b8);
    const float hi = F32(0, 0x00c023ac), lo = F32(0, 0x00c023a8);
    float v = phys_add(cur, delta);
    if (v >= hi) v = hi;
    if (lo >= v) v = lo;
    if (v == cur) return;
    SF32(0, 0x00c023c0, lo > v ? lo : (v > hi ? hi : v));
    SU32(0, 0x00c02400, 1);
    SF32(0, 0x00c023cc, rx_bf(0));
    SF32(0, 0x00c023c4, cur);
}
RX_FUNCTION(0x00674440, rx_world_crowd_excite)

/* 006745d0 (EAX = reaction 1..3; RET 0): every crowd emitter plays its reaction event (+0x234 / +0x3d8 / +0x57c; 0080abd0 with
 * ESI = the event). */
void rx_world_crowd_react(uint32_t kind) {
#ifndef RX_ORACLE
    rx_sound_crowd_reaction(kind);   /* w-sfx: the reaction for the host's crowd emitters (game/sound_state.c) */
#endif
    uint32_t off;
    if (kind == 1) off = 0x234;
    else if (kind == 2) off = 0x3d8;
    else if (kind == 3) off = 0x57c;
    else return;
    world_frame f = world_frame_flat(3, 0);
    for (int32_t i = 0; i < I32(0, 0x00c02474); i++) {
        const rx_addr list = U32(0, 0x00c02470);
        world_ctx c = WCTX(.esi = U32(list, 4u * (uint32_t)i) + off, .edi = (uint32_t)i);
        if (kind == 1) c.edx = list; else if (kind == 2) c.ecx = list; else c.eax = list;
        WORLD_CALL_SP(0xc, rx_world_event_play(c.esi, 0, 0, 0));   /* AudioEvent_Play 0080abd0 */
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x006745d0, rx_world_crowd_react)

/* 00674540 (ECX = event kind; RET 0): the kind's excitement (0x9c47dc) is added, and with the kind's probability (0x9c4804
 * against CRT rand() / 32768 in double precision) the crowd reacts: 3 for kinds 0 / 1, else 2, 1 when the excitement is
 * negative (a tail call of 006745d0). */
void rx_world_crowd_event(int32_t kind) {
    const rx_addr entry = rx_sp();
    world_frame f = world_frame_flat(1, 0);   /* push ecx: the excitement local */
    const uint32_t excitement = world_x87_pass(U32(0, 0x009c47dc + 4u * (uint32_t)kind));
    SU32(f.esp, 0, excitement);
    WORLD_CALL_SP(4, rx_world_crowd_excite(rx_bf(world_x87_pass(excitement))));
    uint32_t seed = U32(0, 0x009cb860);
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    seed = seed * 0x41c64e6du + 0x3039u;
    SU32(0, 0x009cb860, seed);
    const float roll = (float)((double)(float)(int32_t)((seed >> 16) & 0x7fff) * 3.0517578125e-05 /* 0x9776a0 */);
    const float chance = F32(0, 0x009c4804 + 4u * (uint32_t)kind);
    world_frame_leave(&f);
    if (!(chance > roll)) return;
    uint32_t reaction = (kind >= 0 && kind <= 1) ? 3 : 2;
    if (rx_bf(0) > rx_bf(excitement)) reaction = 1;
    rx_set_sp(entry);
    rx_world_crowd_react(reaction);
}
RX_FUNCTION(0x00674540, rx_world_crowd_event)

/* 006b0480 (ESI = IK object with 128-byte chain entries at +0x64, count +0x60; RET 0): every flagged entry (+8) restarts its fade:
 * +0x70 = 0.25, +0x40 = 0.25 - +0x40 (0 when it reached 0.25); all flags cleared. */
void rx_world_ik_restart_fades(rx_addr obj) {
    for (uint32_t i = 0; i < U32(obj, 0x60); i++) {
        const rx_addr e = U32(obj, 0x64) + 0x80 * i;
        if (U32(e, 8) != 0) {
            const float v = F32(e, 0x40);
            SF32(e, 0x70, 0.25f /* 0x92ff84 */);
            SF32(e, 0x40, !(v >= 0.25f) ? phys_sub(0.25f, v) : rx_bf(0));
        }
        SU32(e, 8, 0);
    }
}
RX_FUNCTION(0x006b0480, rx_world_ik_restart_fades)

/* 006b7560 (thiscall object; out (48 bytes); RET 4; EAX = out): its three vectors +0x450 / +0x460 / +0x490. */
void rx_world_get_three_vectors(rx_addr obj, rx_addr out) {
    v4_st(out, v4_ld(obj + 0x450));
    v4_st(out + 0x10, v4_ld(obj + 0x460));
    v4_st(out + 0x20, v4_ld(obj + 0x490));
}
RX_FUNCTION(0x006b7560, rx_world_get_three_vectors)

/* 006def20 (EAX = object; RET 0; xmm0): its timer in seconds: the 64-bit tick count (0088bd90) of the session clock (00892db0
 * when +0x1c4c is set and the mode +0x1b0 is not 1, else 00892170) times 0.001. */
float rx_world_timer_seconds(rx_addr obj) {
    const rx_addr sp0 = rx_sp();
    const rx_addr F = ((sp0 - 4) & ~7u) - 8;   /* push ebp; and esp, -8; sub esp, 8 */
    rx_set_sp(F);
    const rx_addr o = U32(obj, 0x10);
    rx_addr clock;
    /* BOUNDARY (online only): 00892db0 is the Quazal session clock; +0x1c4c == 0 offline, so 00892170 is the offline path. */
    if (U32(o, 0x1c4c) != 0 && U32(o, 0x1b0) != 1) clock = WORLD_CTX(0x00892db0, &WCTX(.ecx = F, .eax = o), 0, F);
    else { WORLD_CALL_SP(8, (void)rx_world_clock_now(F, 0)); clock = F; }   /* 00892170 */
    const uint64_t ticks = phys_int64_load(clock);   /* 0088bd90 Int64_Load (registered port) */
    SU32(F, 4, (uint32_t)(ticks >> 32));
    rx_set_sp(sp0);
    return phys_mul((float)(int32_t)(uint32_t)ticks, 0.001f /* 0x97743c */);
}
RX_FUNCTION(0x006def20, rx_world_timer_seconds)

/* 00702ad0 StringMap_Find (ESI = map {+0 container, +0x18 head}; stdcall out iterator, key; RET 8; EAX = out): find in a
 * string-keyed map: lower_bound (008519a0), then the key compare (IAT 0x9161bc: key < node key): found -> {container, node},
 * else end() = {container, head}. */
rx_addr rx_world_string_map_find(rx_addr map, rx_addr out, rx_addr key) {
    world_frame f = world_frame_flat(4, 0x14);
    rx_addr node;
    WORLD_CALL_SP(4, node = rx_world_string_map_lower_bound(map, key));   /* 008519a0 */
    if (map == 0) crt_abort();
    const rx_addr container = U32(map, 0), head = U32(map, 0x18);
    if (container == 0) crt_abort();
    int found = 0;
    if (node != head) {
        uint32_t lt;
        CRT_WITH_REGS(node + 0xc, key, map, head, lt = crt_str_less(key, node + 0xc));   /* 78495979 std::operator< (IAT 0x9161bc) */
        found = (lt & 0xff) == 0;
    }
    if (found) { SU32(out, 4, node); SU32(out, 0, container); }
    else { SU32(out, 4, U32(map, 0x18)); SU32(out, 0, U32(map, 0)); }
    world_frame_leave(&f);
    return out;
}
RX_FUNCTION(0x00702ad0, rx_world_string_map_find)

/* 006b6d80 (EAX = keyframe blender; RET 0): the keys around the current time (+0xdb4) among the +0xda8 tracks (112-byte keys at
 * [[blender] +0x18]: +0x40 start, +0x4c / +0x50 lead-in / lead-out, +0x64 / +0x68 flags; 216-byte states from +0xd8: +0 end) and
 * the blend toward them: a key whose span holds the time (and is not flagged) blends alone (00840440 with its fraction); else the
 * nearest key ending before the time and the nearest starting after it, alone or the two together (00840b70 with the
 * cross-fade weights; +0xdb8 = the fraction between them). */
void rx_world_keyframe_blend_select(rx_addr b) {
    world_frame f = world_frame_flat(4, 0xc);
    const rx_addr P = f.esp;
    const int32_t tracks = I32(b, 0xda8);
    int32_t inside = -1;
    SF32(P, 0x10, rx_bf(0));
    if (tracks > 0) {
        const float now = F32(b, 0xdb4);
        rx_addr key = U32(U32(b, 0), 0x18) + 0x50, state = b + 0xd8;
        for (int32_t i = 0; i < tracks; i++, key += 0x70, state += 0xd8) {
            if (!(now >= phys_add(F32(key, -4), F32(key, -0x10)))) continue;
            const float end = F32(state, 0);
            if (!(phys_sub(end, F32(key, 0)) >= now) || U32(key, 0x14) != 0 || U32(key, 0x18) != 0) continue;
            const float start = F32(key, -0x10);
            SF32(P, 0x10, phys_div(phys_sub(now, start), phys_sub(end, start)));
            inside = i;
        }
        if (inside != -1) {
            WORLD_CALL_SP(12, (void)phys_pose_sample((uint32_t)inside * 0xd8 + b + 4, rx_bf(world_x87_pass(U32(P, 0x10))), b + 0x440));
            world_frame_leave(&f);
            return;
        }
    }
    if (I32(b, 0xda8) <= 0) { world_frame_leave(&f); return; }
    float before_best = -3.40282347e+38f /* 0x977550 */, after_best = 3.40282347e+38f /* 0x977494 */;
    int32_t before = -1, after = -1;
    const rx_addr keys = U32(U32(b, 0), 0x18);
    SU32(P, 0x10, keys);
    {
        rx_addr key = keys + 0x68, state = b + 0xd8;
        for (int32_t i = 0; i < I32(b, 0xda8); i++, key += 0x70, state += 0xd8) {
            if (U32(key, -4) != 0 || U32(key, 0) != 0) continue;
            const float end = phys_sub(F32(state, 0), F32(key, -0x18));
            const float now = F32(b, 0xdb4);
            const float start = phys_add(F32(key, -0x28), F32(key, -0x1c));
            if (now >= end && end >= before_best) { before_best = end; before = i; }
            if (start >= now && after_best >= start) { after_best = start; after = i; }
        }
    }
    if (before == -1) {
        if (after == -1) { world_frame_leave(&f); return; }
        const rx_addr key = (uint32_t)after * 0x70 + U32(P, 0x10);
        const float span = phys_sub(F32(b, (uint32_t)after * 0xd8 + 0xd8), F32(key, 0x40));
        WORLD_CALL_SP(12, (void)phys_pose_sample((uint32_t)after * 0xd8 + b + 4, phys_div(F32(key, 0x4c), span), b + 0x440));
        world_frame_leave(&f);
        return;
    }
    if (after == -1) {
        const rx_addr key = (uint32_t)before * 0x70 + U32(P, 0x10);
        const float span = phys_sub(F32(b, (uint32_t)before * 0xd8 + 0xd8), F32(key, 0x40));
        WORLD_CALL_SP(12, (void)phys_pose_sample((uint32_t)before * 0xd8 + b + 4, phys_div(phys_sub(span, F32(key, 0x50)), span), b + 0x440));
        world_frame_leave(&f);
        return;
    }
    {
        const rx_addr keys2 = U32(U32(b, 0), 0x18);
        const rx_addr kb = (uint32_t)before * 0x70 + keys2, ka = (uint32_t)after * 0x70 + keys2;
        const float end_b = F32(b, (uint32_t)before * 0xd8 + 0xd8), start_b = F32(kb, 0x40);
        const float fade_b = phys_sub(end_b, F32(kb, 0x50));
        const float w_b = phys_div(phys_sub(fade_b, start_b), phys_sub(end_b, start_b));
        const float lead_a = F32(ka, 0x4c), start_a = F32(ka, 0x40);
        const float w_a = phys_div(lead_a, phys_sub(F32(b, (uint32_t)after * 0xd8 + 0xd8), start_a));
        const float mix = phys_div(phys_sub(F32(b, 0xdb4), fade_b), phys_sub(phys_add(lead_a, start_a), fade_b));
        SF32(P, 0x18, w_b);
        SF32(b, 0xdb8, mix);
        SF32(P, 0x10, mix);
        SF32(P, 0x14, w_a);
        WORLD_CALL_SP(0x18, (void)phys_pose_sample_blend((uint32_t)before * 0xd8 + b + 4, (uint32_t)after * 0xd8 + b + 4,
                                                          rx_bf(world_x87_pass(U32(P, 0x18))), rx_bf(world_x87_pass(U32(P, 0x14))),
                                                          rx_bf(world_x87_pass(U32(P, 0x10))), b + 0x440));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x006b6d80, rx_world_keyframe_blend_select)

/* ---- the ClipRecorder ------------------------------------------------------------------------------------------------- */

/* 006aa460 ClipRecorder_FillEntry (EDI = bike; stdcall entry (76 bytes), rider; RET 8): the bike, its damage poses (classes 3 / 5
 * / 7: 0047bb40 / 0047bd80 / 0047be30 into +8 / +0x28), its race time (the 0xa96dc0 table, 64-byte rows) and, in mode 5, whether
 * the rider has a lap (+0x1120 >= 0). */
void rx_world_clip_recorder_fill_entry(rx_addr bike, rx_addr entry, rx_addr rider) {
    world_frame f = world_frame_flat(3, 0);
    SU32(entry, 0, bike);
    SU32(entry, 0x48, 0);
    if (bike != 0) {
        rx_addr mgr;
        WORLD_CALL_SP(0, mgr = rx_world_damage_manager_instance());
        const uint32_t cls = U32(bike, 0x194);
        if (cls == 3 || cls == 5 || cls == 7) {
            WORLD_CALL_SP(0, mgr = rx_world_damage_manager_instance());
            WORLD_CALL_SP(0, rx_clip_damage_copy_positions(bike, mgr, entry + 8));
            WORLD_CALL_SP(0, mgr = rx_world_damage_manager_instance());
            WORLD_CALL_SP(0, rx_clip_damage_copy_states(bike, mgr, entry + 0x28));
        }
        float t = rx_bf(0);
        for (uint32_t row = 0, a = 0x00a96dc0; a < 0x00a970c0; a += 0x40, row++)
            if (bike == U32(a, 0)) { t = F32(0, (row << 6) + 0x00a96d90); break; }
        if (rx_mud_side_of(bike)) t = F32(rx_mud_side_of(bike), 0);   /* (w-rc-core) records 12..31 */
        SF32(entry, 4, t);
        if (U32(U32(U32(rider, 0x74), 0x94), 0x238) == 5) SU32(entry, 0x48, I32(rider, 0x1120) >= 0 ? 1 : 0);
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x006aa460, rx_world_clip_recorder_fill_entry)

/* 006abbc0 ClipRecorder_FillEntries (cdecl 12 entries): one entry per rider of the race (0xa942b8: +0x26c riders, +0x2d0 their
 * records), the rest cleared. */
void rx_world_clip_recorder_fill_entries(rx_addr entries) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp((((sp0 - 4) & ~7u) - 4) - 12);   /* push ebp; and esp, -8; push ecx; 3 pushes */
    const rx_addr P = rx_sp();
    uint32_t i = 0;
    if (I32(U32(0, 0x00a942b8), 0x26c) > 0) {
        SU32(P, 0xc, entries);
        for (; (int32_t)i < I32(U32(0, 0x00a942b8), 0x26c); i++) {
            const rx_addr rider = rx_stand(U32(0, 0x00a942b8), i);
            const rx_addr bike = U32(U32(rider, 0x74), 0x1ac);
            if (i >= RX_RC_ORIG) SU32(P, 0xc, rx_clip_entry(entries, i, 1));   /* (w-rc-core) entries 12..31 in side storage */
            WORLD_CALL_SP(8, rx_world_clip_recorder_fill_entry(bike, U32(P, 0xc), rider));
            SU32(P, 0xc, U32(P, 0xc) + 0x4c);
        }
        if (i >= 0xc) { for (uint32_t k = i; k < RX_RC_MAX; k++) { const rx_addr e = rx_clip_entry(entries, k, 0); if (e) { SU32(e, 0, 0); SU32(e, 0x48, 0); } } rx_set_sp(sp0); return; }
    }
    for (rx_addr e = i * 0x4c + entries; i < 0xc; i++, e += 0x4c) { SU32(e, 0, 0); SU32(e, 0x48, 0); }
    for (uint32_t k = RX_RC_ORIG; k < RX_RC_MAX; k++) { const rx_addr e = rx_clip_entry(entries, k, 0); if (e) { SU32(e, 0, 0); SU32(e, 0x48, 0); } }   /* (w-rc-core) */
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006abbc0, rx_world_clip_recorder_fill_entries)

/* 006aacf0 ClipRecorder_Candidate (ECX = clip type, EDX = rider; cdecl start, end): with recording on (0xd5a9a4) and a rider
 * with a player (+0x74): takes the candidate when there is none (0xa959a0 start, 0xa959a4 end, 0xa959a8 type, 0xa959ac rider;
 * the field snapshot 006abbc0 into 0xa959b0), extends the current one when they overlap within 9.5 s and the type is not 9 (a
 * higher type takes it over), or replaces it with a higher type. */
void rx_world_clip_recorder_candidate(uint32_t type, rx_addr rider, float start, float end) {
    if (U32(0, 0x00d5a9a4) == 0 || U32(rider, 0x74) == 0) return;
    const int32_t cur = I32(0, 0x00a959a8);
    if (cur == 0) {
        SF32(0, 0x00a959a0, start); SF32(0, 0x00a959a4, end);
        SU32(0, 0x00a959a8, type); SU32(0, 0x00a959ac, rider);
        WORLD_CALL_SP(4, rx_world_clip_recorder_fill_entries(0x00a959b0));
        return;
    }
    const float span = phys_add(phys_sub(F32(0, 0x00a959a4), F32(0, 0x00a959a0)), phys_sub(end, start));
    if (9.5f /* 0x977a84 */ > span && cur != 9) {
        SF32(0, 0x00a959a4, end);
        if (cur > (int32_t)type) return;
        SU32(0, 0x00a959a8, type);
        return;
    }
    if ((int32_t)type <= cur) return;
    SF32(0, 0x00a959a0, start); SF32(0, 0x00a959a4, end);
    SU32(0, 0x00a959a8, type); SU32(0, 0x00a959ac, rider);
    WORLD_CALL_SP(4, rx_world_clip_recorder_fill_entries(0x00a959b0));
}
RX_FUNCTION(0x006aacf0, rx_world_clip_recorder_candidate)

/* 006aaca0 ClipRecorder_CandidateOfType (ECX = clip type, EDX = rider): a candidate around the race time (0xa942b8 +0x294) with
 * the type's lead / trail (0x9c40a8 pairs). */
void rx_world_clip_recorder_candidate_of_type(uint32_t type, rx_addr rider) {
    if (U32(0, 0x00d5a9a4) == 0) return;
    const float now = F32(U32(0, 0x00a942b8), 0x294);
    const float end = phys_add(F32(0, type * 8 + 0x009c40ac), now), start = phys_sub(now, F32(0, type * 8 + 0x009c40a8));
    WORLD_CALL_SP(8, rx_world_clip_recorder_candidate(type, rider, start, end));
}
RX_FUNCTION(0x006aaca0, rx_world_clip_recorder_candidate_of_type)

/* one event into the clip ring: time, type, rider id, value; the cursor wraps at 100 */
static void clip_ring_push(uint32_t type, uint32_t rider_id, uint32_t value) {
    const uint32_t i = U32(0, 0x00a942c8);
    const rx_addr e = 0x00d2f160 + (i << 7);
    SU32(e, 4, type);
    SU32(e, 0, world_x87_pass(U32(U32(0, 0x00a942b8), 0x294)));
    SU32(e, 8, rider_id);
    SU32(e, 0x60, value);
    SU32(0, 0x00a942c8, (uint32_t)((int32_t)(i + 1) % 100));
}
/* the rider index in the race (0055a050, ECX = the race) */
static uint32_t race_rider_index(rx_addr rider, rx_addr race) {
    int32_t r;
    WORLD_CALL_SP(0, r = rx_world_group_rider_index(race, rider));
    return (uint32_t)r;
}

/* 006ab8a0 ClipRecorder_LogWreck (EAX = rider; cdecl value): in a live race (0xa942b4 == 1) for a non-replay rider (+0x908): a
 * wreck event (type 4) into the ring and, for an active rider (+4 set, +0x10 clear), a wreck candidate (type 6, a tail call). */
void rx_world_clip_recorder_log_wreck(rx_addr rider, uint32_t value) {
    if (U32(rider, 0x908) != 0 || U32(0, 0x00a942b4) != 1) return;
    const rx_addr race = U32(0, 0x00a942b8);
    world_frame f = world_frame_flat(2, 0);
    const uint32_t idx = race_rider_index(rider, race);
    world_frame_leave(&f);
    const uint32_t id = U32(U32(race, 0x268), idx * 4);
    clip_ring_push(4, id, value);
    if (U32(rider, 4) != 0 && U32(rider, 0x10) == 0) rx_world_clip_recorder_candidate_of_type(6, id);
}
RX_FUNCTION(0x006ab8a0, rx_world_clip_recorder_log_wreck)

/* 006ab930 ClipRecorder_LogStunt (EAX = rider; cdecl value): in a live race, a stunt event (type 6) into the ring (a replay
 * rider's id from the race +0x27c, else +0x268). */
void rx_world_clip_recorder_log_stunt(rx_addr rider, uint32_t value) {
    if (U32(0, 0x00a942b4) != 1) return;
    const rx_addr race = U32(0, 0x00a942b8);
    world_frame f = world_frame_flat(2, 0);
    const uint32_t idx = race_rider_index(rider, race);
    world_frame_leave(&f);
    const uint32_t id = U32(rider, 0x908) != 0 ? U32(U32(race, 0x27c), idx * 4) : U32(U32(race, 0x268), idx * 4);
    clip_ring_push(6, id, value);
}
RX_FUNCTION(0x006ab930, rx_world_clip_recorder_log_stunt)

/* 006ab9b0 ClipRecorder_LogRecover (EAX = the player; RET 0): in a live race (0xa942b4 == 1): a recovery event (type 7) into the
 * ring (time from the race +0x294, the rider's id from the racing list +0x268, or the waiting list +0x27c for +0x908 != 0; no
 * value word) and, for an active local pad (+4 set, +0x10 clear) with recording on (0xd5a9a4), a recovery candidate (type 5,
 * ClipRecorder_Candidate with the race time - 0x9c40d0 .. + 0x9c40d4).
 * Frame: push ebx / esi / edi (0xc). */
void rx_world_clip_recorder_log_recover(rx_addr rider) {
    if (U32(0, 0x00a942b4) != 1) return;
    world_frame f = world_frame_flat(3, 0);
    const rx_addr race = U32(0, 0x00a942b8);
    const uint32_t idx = race_rider_index(rider, race);
    const rx_addr id = U32(rider, 0x908) != 0 ? U32(U32(race, 0x27c), idx * 4) : U32(U32(race, 0x268), idx * 4);
    const uint32_t i = U32(0, 0x00a942c8);
    const rx_addr e = 0x00d2f160 + (i << 7);
    SU32(e, 4, 7);
    SU32(e, 0, world_x87_pass(U32(race, 0x294)));
    SU32(e, 8, id);
    SU32(0, 0x00a942c8, (uint32_t)((int32_t)(i + 1) % 100));
    if (U32(rider, 4) != 0 && U32(rider, 0x10) == 0 && U32(0, 0x00d5a9a4) != 0) {
        const float now = F32(race, 0x294);
        const float end = phys_add(now, F32(0, 0x009c40d4)), start = phys_sub(now, F32(0, 0x009c40d0));
        WORLD_CALL_SP(8, rx_world_clip_recorder_candidate(5, id, start, end));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x006ab9b0, rx_world_clip_recorder_log_recover)
