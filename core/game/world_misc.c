/* world_misc.c -- the small leaves of the physics world's list that belong to no larger share: constant vfuncs, the per-pad
 * key map selectors of the player controls (0x00548xxx), rider wreck-state entry points (0x0054exxx), the activity / audio
 * logic event handlers, the 0x50-byte record copies of the key tracks (0x004dbxxx), red-black tree helpers (0x00586xxx), the
 * name registry lookup (00593220) and a few accessors.
 *
 * Player controls: the per-pad player block is [0x9c52c4 + 4 * pad]; its key maps are KeyMap / CompoundKeyMap objects at
 * fixed offsets (+0xc90, +0x1734 ...), chosen by the control scheme +0x5130 (1, 2, other) and the "alternate" flag +0x512c.
 * Key map vfuncs (world_node_input.c): vf2 (+0x08) down, vf4 (+0x10) analog value (x87), vf8 (+0x20) code; mask 7, pad. */
#include "world_util.h"
#include "rider_cap.h"
#include "../src/rx_os.h"
#include "crt.h"
#include "crt_util.h"
#include "world_node.h"
#include "world_rider.h"
#include "world_wreck.h"
#include "world_misc.h"
#include "replay_restore.h"
#include "phys_crt.h"
#include "world_veh.h"

#define PAD_BLOCKS 0x009c52c4u
static rx_addr pad_block(uint32_t pad) { return U32(0, PAD_BLOCKS + 4u * pad); }

static void crt_abort(void) { crt_invalid_parameter_noinfo(); }   /* _invalid_parameter_noinfo */

/* ---- key map vfuncs through the vtable (core port when registered) ---- */
static uint32_t km_vcall_u(rx_addr km, uint32_t slot, uint32_t mask, uint32_t pad) {
    const rx_addr fn = U32(U32(km, 0), slot);
    uint32_t (*f)(rx_addr, uint32_t, uint32_t) = RX_FN(fn, uint32_t (*)(rx_addr, uint32_t, uint32_t));
    return f ? f(km, mask, pad) : WORLD_ORIG(fn, RX_THISCALL, km, mask, pad);
}
static uint32_t km_down(rx_addr km, uint32_t pad) { return km_vcall_u(km, 0x08, 7, pad); }
static uint32_t km_code(rx_addr km, uint32_t pad) { return km_vcall_u(km, 0x20, 7, pad); }
static float km_value(rx_addr km, uint32_t pad) {
    const rx_addr fn = U32(U32(km, 0), 0x10);
    float (*f)(rx_addr, uint32_t, uint32_t) = RX_FN(fn, float (*)(rx_addr, uint32_t, uint32_t));
    if (f) return f(km, 7, pad);
    rx_regs o;
    WORLD_CTX(fn, &WCTX(.ecx = km), &o, 7, pad);
    return o.st0;
}
/* a thiscall vfunc without arguments, EAX result */
static uint32_t vcall0(rx_addr self, uint32_t slot) {
    const rx_addr fn = U32(U32(self, 0), slot);
    uint32_t (*f)(rx_addr) = RX_FN(fn, uint32_t (*)(rx_addr));
    return f ? f(self) : WORLD_ORIG0(fn, RX_THISCALL, self);
}

/* ---- constant / empty vfuncs ---- */
uint32_t rx_world_entity_vf13(rx_addr self) { (void)self; return 5; }                  /* 0051bd60 Entity::vf13 */
RX_FUNCTION(0x0051bd60, rx_world_entity_vf13)
uint32_t rx_world_lap_pacing_vf5(rx_addr self) { (void)self; return 0x13; }            /* 00617e20 BehaviorLapPacing::vf5 */
RX_FUNCTION(0x00617e20, rx_world_lap_pacing_vf5)
uint32_t rx_world_rider_animation_vf12(rx_addr self) { (void)self; return 0x31; }      /* 00617f30 RiderAnimation::vf12 */
RX_FUNCTION(0x00617f30, rx_world_rider_animation_vf12)
/* 00617d70 RiderAnimation::vf17 (AnimationEntity's; thiscall out; RET 4) -> EAX = the node's matrix: the world position of the animation's root
 * node into out (the hierarchy +0x180, its node index byte +0xb, the matrix table [+0] + 0x18, 64 bytes a node, the translation row +0x30).
 * Copied with fld / fstp dword: a signaling NaN comes out quiet. Read off the disassembly (w-vtunported: the entity position slot 0x44 that
 * the cameras, the land animals and the free-ride discoveries call; no run had it reach a rider animation). */
static uint32_t x87_copy_bits(uint32_t b) { return ((b & 0x7fffffffu) > 0x7f800000u) ? (b | 0x400000u) : b; }
uint32_t rx_world_rider_animation_position(rx_addr self, rx_addr out) {
    const rx_addr h = U32(self, 0x180);
    const rx_addr m = U32(U32(h, 0), 0x18) + ((uint32_t)U8(h, 0xb) << 6);
    for (uint32_t k = 0; k < 3; k++) SU32(out, 4 * k, x87_copy_bits(U32(m, 0x30 + 4 * k)));
    return m;
}
RX_FUNCTION(0x00617d70, rx_world_rider_animation_position)
void rx_world_shared_vf_ret8(rx_addr self, uint32_t a, uint32_t b) { (void)self; (void)a; (void)b; }   /* 005a4150 (RET 8) */
RX_FUNCTION(0x005a4150, rx_world_shared_vf_ret8)
void rx_world_shared_vf_ret16(rx_addr self, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {          /* 005de600 (RET 0x10) */
    (void)self; (void)a; (void)b; (void)c; (void)d;
}
RX_FUNCTION(0x005de600, rx_world_shared_vf_ret16)

/* 00592b20 MXSeriesEvent_OnAdvancePostEvent (thiscall a, b; an event handler): forwards to the object's vf13 */
uint32_t rx_world_series_event_on_advance_post(rx_addr self, uint32_t a, uint32_t b) {
    const rx_addr fn = U32(U32(self, 0), 0x34);
    uint32_t (*f)(rx_addr, uint32_t, uint32_t) = RX_FN(fn, uint32_t (*)(rx_addr, uint32_t, uint32_t));
    return f ? f(self, a, b) : WORLD_ORIG(fn, RX_THISCALL, self, a, b);
}
RX_FUNCTION(0x00592b20, rx_world_series_event_on_advance_post)

/* 0055eb10 BaseScenario::vf8: no scenario object at +0x151b4 */
uint32_t rx_world_base_scenario_vf8(rx_addr self) { return U32(self, 0x151b4) == 0; }
RX_FUNCTION(0x0055eb10, rx_world_base_scenario_vf8)

/* 0055ebd0 MXActivityFreeRide::vf8: 0 while the session's +0x20018 is set, else !(the sub-activity at +0xff200)->vf8() */
uint32_t rx_world_free_ride_vf8(rx_addr self) {
    if (U32(U32(self, 0x234), 0x20018) != 0) return 0;
    return vcall0(self + 0xff200, 0x20) == 0;
}
RX_FUNCTION(0x0055ebd0, rx_world_free_ride_vf8)

/* 0055e960 Activity::vf21: state +0x21c 2 or 3 -> 1; 4 -> (+0x23c == 0x16); else 0 */
uint32_t rx_world_activity_vf21(rx_addr self) {
    const int32_t s = I32(self, 0x21c);
    if (s < 2) return 0;
    if (s <= 3) return 1;
    if (s == 4) return U32(self, 0x23c) == 0x16;
    return 0;
}
RX_FUNCTION(0x0055e960, rx_world_activity_vf21)

/* 0056a020 MXActivityRaceStub::vf8: by activity state +0x21c:
 *   0: 0 when +0x1068 is set and +0x2cc clear; else (the race +0xff090)->vf11() || ->vf7()
 *   4: by +0x13e640: 1 -> (+0x114260)->vf11(), 2 -> 0, 3 -> (+0x129430)->vf11() (tail calls), other -> 0
 *   other: 0 */
uint32_t rx_world_race_stub_vf8(rx_addr self) {
    const uint32_t s = U32(self, 0x21c);
    if (s == 0) {
        if (U32(self, 0x1068) != 0 && U32(self, 0x2cc) == 0) return 0;
        const rx_addr race = self + 0xff090;
        if (vcall0(race, 0x2c) != 0) return 1;
        return vcall0(race, 0x1c) != 0;
    }
    if (s != 4) return 0;
    switch (U32(self, 0x13e640)) {
    case 1: return vcall0(self + 0x114260, 0x2c);
    case 3: return vcall0(self + 0x129430, 0x2c);
    default: return 0;
    }
}
RX_FUNCTION(0x0056a020, rx_world_race_stub_vf8)

/* 00617d20 AnimationEntity::vf14: the node's world matrix slot (node +0x180: set +0 (matrices +0x18), index byte +0xb) */
rx_addr rx_world_anim_entity_matrix(rx_addr self) {
    const rx_addr node = U32(self, 0x180);
    return ((uint32_t)U8(node, 0xb) << 6) + U32(U32(node, 0), 0x18);
}
RX_FUNCTION(0x00617d20, rx_world_anim_entity_matrix)

/* 00617dc0 AnimationEntity::vf16 (thiscall matrix; RET 4): copy the 4x4 matrix into the node's slot and invalidate the node
 * (frame: push ebp; and esp, -16). caller_*: the caller's EBX / ESI / EDI (Node_Invalidate pushes them). */
void rx_world_anim_entity_set_matrix_regs(rx_addr self, rx_addr m, uint32_t caller_ebx, uint32_t caller_esi, uint32_t caller_edi) {
    world_frame f = world_frame_enter(0, 0, 1);
    const rx_addr node = U32(self, 0x180);
    const rx_addr dst = ((uint32_t)U8(node, 0xb) << 6) + U32(U32(node, 0), 0x18);
    uint32_t tmp[16];
    for (int i = 0; i < 16; i++) tmp[i] = U32(m, 4 * i);
    for (int i = 0; i < 16; i++) SU32(dst, 4 * i, tmp[i]);
    WORLD_CALL_SP(0, rx_world_node_invalidate(node, caller_ebx, caller_esi, caller_edi));
    world_frame_leave(&f);
}
void rx_world_anim_entity_set_matrix(rx_addr self, rx_addr m) { rx_world_anim_entity_set_matrix_regs(self, m, 0, 0, 0); }
RX_FUNCTION(0x00617dc0, rx_world_anim_entity_set_matrix)

/* 00617f00 / 00545340 (ECX = rider animation): the pose state u16 +0x5a8 (resp. +0x5a0) equals the current pose id
 * (u16 at [+0x580], 0 without one). EAX = EDX = the result. */
uint32_t rx_world_rider_pose_is_current(rx_addr ra) {
    const rx_addr p = U32(ra, 0x580);
    return U16(ra, 0x5a8) == (p ? U16(p, 0) : 0);
}
RX_FUNCTION(0x00617f00, rx_world_rider_pose_is_current)
uint32_t rx_world_rider_pose_is_current_5a0(rx_addr ra) {
    const rx_addr p = U32(ra, 0x580);
    return U16(ra, 0x5a0) == (p ? U16(p, 0) : 0);
}
RX_FUNCTION(0x00545340, rx_world_rider_pose_is_current_5a0)

/* 006298f0 / 00629900 (EAX = blend, xmm0 = value): when the blend is active (+0xc), its +0x18 (resp. +0x1c) = value */
void rx_world_blend_set_18(rx_addr b, float v) { if (U32(b, 0xc) != 0) SF32(b, 0x18, v); }
RX_FUNCTION(0x006298f0, rx_world_blend_set_18)
void rx_world_blend_set_1c(rx_addr b, float v) { if (U32(b, 0xc) != 0) SF32(b, 0x1c, v); }
RX_FUNCTION(0x00629900, rx_world_blend_set_1c)

/* ---- 0x50-byte key records (vector at +0xc begin, +0x14 end) ---- */

/* 004db570 (ECX = vector): the record count (0 without storage) */
uint32_t rx_world_record_vector_size(rx_addr v) {
    const rx_addr b = U32(v, 0xc);
    if (b == 0) return 0;
    return (uint32_t)((int32_t)(U32(v, 0x14) - b) / 80);
}
RX_FUNCTION(0x004db570, rx_world_record_vector_size)

static void record_copy(rx_addr dst, rx_addr src) { for (uint32_t i = 0; i < 0x48; i += 4) SU32(dst, i, U32(src, i)); }

/* 004dbfa0 (EAX = destination or 0, ECX = source): copy one record (its first 0x48 bytes) */
void rx_world_record_construct(rx_addr dst, rx_addr src) { if (dst != 0) record_copy(dst, src); }
RX_FUNCTION(0x004dbfa0, rx_world_record_construct)

/* 004dbed0 (ECX = first, EDX = last; cdecl destination, 3 tag arguments; EAX = past the last written): copy [first, last) */
rx_addr rx_world_record_copy_range(rx_addr first, rx_addr last, rx_addr dst) {
    for (; first != last; first += 0x50, dst += 0x50) record_copy(dst, first);
    return dst;
}
RX_FUNCTION(0x004dbed0, rx_world_record_copy_range)

/* 004dbe80 (EDI = destination, EAX = last; cdecl first; EAX = destination + (last - first)): the uninitialized copy wrapper
 * (it zeroes the low byte of its argument slot for a tag argument; the oracle adapter repeats that). */
rx_addr rx_world_record_uninitialized_copy(rx_addr first, rx_addr last, rx_addr dst) {
    WORLD_CALL_SP(0x10, rx_world_record_copy_range(first, last, dst));
    return (uint32_t)((int32_t)(last - first) / 80) * 80u + dst;
}
RX_FUNCTION(0x004dbe80, rx_world_record_uninitialized_copy)

/* 00544cc0 (ESI = vector {data, count, capacity, grow}; stdcall const value*; RET 4; EAX): push the value unless already there
 * (0), growing by `grow` (aligned alloc of the new capacity * 4, -1 on overflow; memcpy; free); 1 when pushed. */
uint32_t rx_world_ptr_vector_push_unique(rx_addr v, rx_addr value) {
    const int32_t count = I32(v, 4);
    for (int32_t i = 0; i < count; i++)
        if (U32(U32(v, 0) + 4u * (uint32_t)i, 0) == U32(value, 0)) return 0;
    if (count >= I32(v, 8)) {
        const uint32_t cap = rx_rc_grow(U32(v, 8), U32(v, 0xc));   /* (w-rc-core) a zero growth with 13+ players: rider_cap.h */
        const uint64_t bytes = (uint64_t)cap * 4u;
        const rx_addr data = rx_world_aligned_alloc(bytes > 0xffffffffu ? 0xffffffffu : (uint32_t)bytes);
        crt_memcpy(data, U32(v, 0), U32(v, 8) * 4u);   /* memcpy (thunk 0x8fa070) */
        rx_world_aligned_free(U32(v, 0));
        SU32(v, 0, data);
        SU32(v, 8, cap);
    }
    SU32(U32(v, 0) + 4u * U32(v, 4), 0, U32(value, 0));
    SU32(v, 4, U32(v, 4) + 1);
    return 1;
}
RX_FUNCTION(0x00544cc0, rx_world_ptr_vector_push_unique)
/* The same push with the original's frames (e = its return slot, r = the caller's EBX EBP ESI EDI): push ebp, edi; when it grows push
 * ebx; push the size; call ReflexAlignedAlloc (return 00544d05: push esi; [the allocator lock when 0xda67d8 is set]; push 0x10,
 * size; call [_aligned_malloc], return 0078fc14); push the old size, old, new; call memcpy (return 00544d18); push old; call
 * ReflexAlignedFree (return 00544d20: push esi; push p; call [_aligned_free], return 0078fc82). In the oracle the heap calls run the
 * originals at the original's ESP: a later event record copies these words (dead stack). */
uint32_t rx_world_ptr_vector_push_unique_fr(rx_addr e, crt_regs4 r, rx_addr v, rx_addr value) {
    SU32(e, -4, r.ebp); SU32(e, -8, r.edi);
    const int32_t count = I32(v, 4);
    for (int32_t i = 0; i < count; i++)
        if (U32(U32(v, 0) + 4u * (uint32_t)i, 0) == U32(value, 0)) return 0;
    if (count >= I32(v, 8)) {
        const int lock = U8(0, 0x00da67d8) != 0;
        const rx_addr sp = rx_sp();
        const uint32_t cap = rx_rc_grow(U32(v, 8), U32(v, 0xc));   /* (w-rc-core) a zero growth with 13+ players: rider_cap.h */
        const uint64_t bytes = (uint64_t)cap * 4u;
        const uint32_t size = bytes > 0xffffffffu ? 0xffffffffu : (uint32_t)bytes;
        SU32(e, -0xc, r.ebx); SU32(e, -0x10, size); SU32(e, -0x14, 0x00544d05u);
        const rx_addr ra = e - 0x14;
        SU32(ra, -4, v);
        if (lock) { SU32(ra, -8, 0x00d61704u); SU32(ra, -0xc, 0x0078fbf7u); SU32(ra, -8, 0x00d61704u); SU32(ra, -0xc, 0x0078fc07u); }
        SU32(ra, -8, 0x10); SU32(ra, -0xc, size);
        rx_set_sp(ra - 4);
        const rx_addr data = rx_world_aligned_alloc(size);
        SU32(ra, -0x10, 0x0078fc14u);
        if (lock) { SU32(ra, -8, cap); SU32(ra, -0xc, 0x00d61704u); SU32(ra, -0x10, 0x0078fc30u); SU32(ra, -0x10, 0x0078fc40u); }
        const rx_addr old = U32(v, 0);
        const uint32_t n = U32(v, 8) * 4u;
        SU32(e, -0x14, n); SU32(e, -0x18, old); SU32(e, -0x1c, data); SU32(e, -0x20, 0x00544d18u);
        rx_set_sp(e - 0x10);
        CRT_WITH_REGS(data, value, v, cap, crt_memcpy(data, old, n));   /* memcpy (thunk 0x8fa070): EBX = new, EBP = &value, ESI = v, EDI = cap */
        SU32(e, -0x20, old); SU32(e, -0x24, 0x00544d20u);
        const rx_addr F = e - 0x24;
        SU32(F, -4, v);
        if (lock) { SU32(F, -8, 0x00d61704u); SU32(F, -0xc, 0x0078fc67u); SU32(F, -8, 0x00d61704u); SU32(F, -0xc, 0x0078fc77u); }
        SU32(F, -8, old);
        rx_set_sp(F - 4);
        rx_world_aligned_free(old);
        SU32(F, -0xc, 0x0078fc82u);
        if (lock) { SU32(F, -8, 0x00d61704u); SU32(F, -0xc, 0x0078fc9bu); SU32(F, -0xc, 0x0078fcabu); }
        rx_set_sp(sp);
        SU32(v, 0, data);
        SU32(v, 8, cap);
    }
    SU32(U32(v, 0) + 4u * U32(v, 4), 0, U32(value, 0));
    SU32(v, 4, U32(v, 4) + 1);
    return 1;
}

/* ---- per-pad controls ---- */

/* 00548660 (ESI = pad; xmm0): 1.0 when the pad's "look back" button is down, else 0. The button's map is +0xc90, except when
 * its code is 0x11: then +0x1dc4 (alternate) or +0x2524. */
float rx_world_pad_look_back(uint32_t pad) {
    rx_addr km = pad_block(pad) + 0xc90;
    if (km_code(km, pad) == 0x11) km = pad_block(pad) + (U32(pad_block(pad), 0x512c) == 1 ? 0x1dc4u : 0x2524u);
    else km = pad_block(pad) + 0xc90;
    return km_down(km, pad) ? 1.0f /* 0x931f24 */ : 0.0f;
}
RX_FUNCTION(0x00548660, rx_world_pad_look_back)

/* 00548b00 (cdecl pad, motor, speed): rumble the pad motor (input manager vf46 (1, pad, motor, speed, 0.2)); the floats pass
 * through the x87 (fld / fstp). EAX = vf46's. */
uint32_t rx_world_pad_rumble(uint32_t pad, uint32_t motor, uint32_t speed_bits) {
    const rx_addr g = U32(0, 0x00a95844);
    const rx_addr dev = vcall0(g, 0x08);
    const rx_addr fn = U32(U32(dev, 0), 0xb8);
    const uint32_t high = world_x87_pass(speed_bits), low = 0x3e4ccccdu;   /* 0.2 (0x92e388) */
    void (*f)(rx_addr, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) =
        RX_FN(fn, void (*)(rx_addr, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t));
    if (f) { f(dev, 1, pad, motor, high, low); return dev + pad * 0x2dcu; }
    return WORLD_ORIG(fn, RX_THISCALL, dev, 1, pad, motor, high, low);
}
RX_FUNCTION(0x00548b00, rx_world_pad_rumble)

/* 00548b70 (ESI = pad): all four buttons of the reset combination down (maps +0x3258, +0x32fc, +0x2680, +0x2724, in order) */
uint32_t rx_world_pad_reset_combo(uint32_t pad) {
    static const uint32_t maps[4] = {0x3258, 0x32fc, 0x2680, 0x2724};
    for (int i = 0; i < 4; i++)
        if (km_down(pad_block(pad) + maps[i], pad) == 0) return 0;
    return 1;
}
RX_FUNCTION(0x00548b70, rx_world_pad_reset_combo)

/* the scheme switch of the steer / lean maps: scheme 1, 2, other x alternate (+0x512c == 1) */
static rx_addr pad_map(uint32_t pad, uint32_t s1_alt, uint32_t s1, uint32_t s2_alt, uint32_t s2, uint32_t o_alt, uint32_t o) {
    const rx_addr b = pad_block(pad);
    const uint32_t scheme = U32(b, 0x5130), alt = U32(b, 0x512c) == 1;
    if (scheme == 1) return b + (alt ? s1_alt : s1);
    if (scheme == 2) return b + (alt ? s2_alt : s2);
    return b + (alt ? o_alt : o);
}
/* 00548f20 (EAX = pad): the steer map */
rx_addr rx_world_pad_steer_map(uint32_t pad) { return pad_map(pad, 0x17d8, 0x1f20, 0x19c4, 0x17d8, 0x19c4, 0x1f20); }
RX_FUNCTION(0x00548f20, rx_world_pad_steer_map)
/* 00548f70 (EAX = pad): the lean map */
rx_addr rx_world_pad_lean_map(uint32_t pad) { return pad_map(pad, 0x1734, 0x21d0, 0x1bc4, 0x1734, 0x1bc4, 0x21d0); }
RX_FUNCTION(0x00548f70, rx_world_pad_lean_map)

/* the first local player's vehicle speed value (+0x150) into the pad block's cumulative map (at `off`) */
static void pad_cumulative_from_player(uint32_t pad, uint32_t off) {
    const rx_addr g = U32(0, 0x00a946b8);
    if (g == 0 || g + 0x7460 == 0) return;
    rx_addr r;
    { rx_addr sp_ = rx_sp(); rx_set_sp(sp_ - 0xc); r = rx_world_players_find(g + 0x7460, 0, 3); rx_set_sp(sp_); }
    if (r == 0) return;
    const rx_addr veh = U32(r, 0x1ac);
    if (veh == 0) return;
    SU32(pad_block(pad), off, world_x87_pass(U32(veh, 0x150)));
}
/* 00548fc0 (ESI = pad): the wreck steer map (scheme 2: +0x17d8 / +0x19c4; 1: +0x1f20 / +0x17d8; other: the cumulative map
 * +0x2074 (fed with the player's +0x150, stored at +0x2070) / +0x19c4) */
rx_addr rx_world_pad_wreck_steer_map(uint32_t pad) {
    pad_cumulative_from_player(pad, 0x2070);
    return pad_map(pad, 0x1f20, 0x17d8, 0x17d8, 0x19c4, 0x2074, 0x19c4);
}
RX_FUNCTION(0x00548fc0, rx_world_pad_wreck_steer_map)
/* 00549050 (ESI = pad): the wreck lean map (likewise with +0x21d0 / +0x1734 / +0x1bc4 and the cumulative map +0x2324) */
rx_addr rx_world_pad_wreck_lean_map(uint32_t pad) {
    pad_cumulative_from_player(pad, 0x2320);
    return pad_map(pad, 0x21d0, 0x1734, 0x1734, 0x1bc4, 0x2324, 0x1bc4);
}
RX_FUNCTION(0x00549050, rx_world_pad_wreck_lean_map)

/* ---- rider wreck state (rider r: +0xb30 class, +0xb34 wreck type, +0xb38 / +0xb3c weight, +0xb40, +0xb44 kind,
 *      +0xb48 steer / lean input, +0xb50 / +0xb54 fixed input, +0x1f0 pad, +0x1ac vehicle, +0x1b4 animation, +0x94 group) ---- */

/* 0054ee80 (EAX = rider; stdcall out float[2]; RET 4): the wreck steer / lean input: the pad's wreck maps' values for a player
 * rider (+0x10 == 0), else the fixed +0xb50 / +0xb54 */
void rx_world_rider_wreck_input(rx_addr r, rx_addr out) {
    if (U32(r, 0x10) != 0) {
        SU32(out, 0, world_x87_pass(U32(r, 0xb50)));
        SU32(out, 4, world_x87_pass(U32(r, 0xb54)));
        return;
    }
    world_frame f = world_frame_flat(4, 0);   /* push ebp, ebx, esi, edi */
    const uint32_t pad = U32(r, 0x1f0);
    const rx_addr steer = rx_world_pad_wreck_steer_map(pad);
    const rx_addr lean = rx_world_pad_wreck_lean_map(pad);
    SF32(out, 0, km_value(steer, pad));
    SF32(out, 4, km_value(lean, pad));
    world_frame_leave(&f);
}
RX_FUNCTION(0x0054ee80, rx_world_rider_wreck_input)

/* 00554da0 (ECX = sender, EDX = event name, ESI = the dispatcher; stdcall arg; RET 4; frame 0xc + cookie): raise the event
 * with the 8-byte payload {arg, sender} (004020c0 EAX = dispatcher, EDI = name; stdcall payload, 8) */
void rx_world_raise_event(rx_addr sender, rx_addr name, rx_addr dispatcher, uint32_t arg) {
    world_frame f = world_frame_flat(0, 0xc);
    const rx_addr F = f.esp;
    SU32(F, 8, U32(0, 0x009b45e4) ^ F);
    SU32(F, 4, sender);
    SU32(F, 0, arg);
    rx_set_sp(F - 4);   /* push edi */
    crt_type_info_name(0x009cdb7c, 0x009e1a7c);   /* type_info::_name_internal_method (IAT 0x916388) */
    WORLD_CALL_SP(8, rx_world_event_dispatch(dispatcher, name, F, 8));
    rx_set_sp(F);
    phys_security_check_cookie(U32(F, 8) ^ F);
    world_frame_leave(&f);
}
RX_FUNCTION(0x00554da0, rx_world_raise_event)

/* 0054e330 (EDX = wreck checker): during a race (group state +0x21c == 2) the checker's +0xc9c grows by 0.1 / 0.25 for the
 * rider animation's wreck mode 1 / 2 (0 otherwise) */
void rx_world_wreck_checker_add_severity(rx_addr chk) {
    const rx_addr c = U32(chk, 0xd98);
    const uint32_t mode = U32(U32(U32(c, 0x1ac), 0x1a0), 0x5b580);
    const float x = mode == 1 ? 0.1f /* 0x931d94 */ : mode == 2 ? 0.25f /* 0x92ff84 */ : 0.0f;
    if (U32(U32(c, 0x94), 0x21c) == 2) SF32(chk, 0xc9c, phys_add(x, F32(chk, 0xc9c)));
}
RX_FUNCTION(0x0054e330, rx_world_wreck_checker_add_severity)

/* 0054e890 (EAX = rider): clear the wreck state; with a group raise its "wreck end" event (0x9416e8) */
void rx_world_rider_clear_wreck(rx_addr r) {
    SU32(r, 0xb30, 0); SU32(r, 0xb34, 0); SU32(r, 0xb44, 0);
    SF32(r, 0xb3c, 0.0f); SF32(r, 0xb38, 0.0f); SF32(r, 0xb50, 0.0f); SF32(r, 0xb54, 0.0f);
    const rx_addr g = U32(r, 0x94);
    if (g == 0) return;
    world_frame f = world_frame_flat(1, 0);   /* push esi */
    WORLD_CALL_SP(4, rx_world_raise_event(U32(r, 0x1ac), 0x009416e8, U32(g, 0x290), g));
    world_frame_leave(&f);
}
RX_FUNCTION(0x0054e890, rx_world_rider_clear_wreck)

/* 0054e8f0 (EDI = rider, EAX = {kind, weight}, ECX = class; stdcall wreck type; RET 4): start a wreck (rider and vehicle
 * present): the state, the input (0054ee80 into +0xb48), +0xb40 = 0; with an animation its scripted mode by kind
 * (table 0x54e9f8; 8 otherwise) and the clip recorder's stunt log; with a group the "wreck" event (0x941278) and the
 * checker severity (0054e330 EDX = rider). */
void rx_world_rider_start_wreck(rx_addr r, rx_addr anim, uint32_t cls, uint32_t type) {
    static const uint32_t mode_of_kind[12] = {1, 5, 9, 0, 4, 8, 3, 7, 0xb, 2, 6, 0xa};
    world_frame f = world_frame_flat(2, 0);   /* push ecx; push esi */
    if (U32(r, 4) != 0 && U32(r, 0x1ac) != 0) {
        const uint32_t w = U32(anim, 4);
        SU32(r, 0xb30, cls);
        SU32(r, 0xb44, U32(anim, 0));
        SU32(r, 0xb34, type);
        SU32(r, 0xb38, w);
        SU32(r, 0xb3c, w);
        WORLD_CALL_SP(4, rx_world_rider_wreck_input(r, r + 0xb48));
        SF32(r, 0xb40, 0.0f);
        const rx_addr ra = U32(r, 0x1b4);
        if (ra != 0) {
            const uint32_t k = U32(r, 0xb44) - 1;
            const uint32_t mode = k <= 0xb ? mode_of_kind[k] : 8;
            WORLD_CALL_SP(0, rx_world_rider_start_scripted_mode(ra, mode));
            WORLD_CALL_SP(4, rx_world_clip_recorder_log_stunt(r, mode));
        }
        const rx_addr g = U32(r, 0x94);
        if (g != 0) {
            WORLD_CALL_SP(4, rx_world_raise_event(U32(r, 0x1ac), 0x00941278, U32(g, 0x290), g));
            WORLD_CALL_SP(0, rx_world_wreck_checker_add_severity(r));
        }
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0054e8f0, rx_world_rider_start_wreck)

/* 0054ed10 (EDI = rider; ESI = the caller's, passed on): the rider falls off: the vehicle's +0x280 = wreck type, its sim
 * +0x2374 = 1; the animation's wreck end (00623310 stdcall animation, class); clear the wreck state; the clip recorder's wreck log; the "fell" event
 * (0x9416fc); +0x11d4 counts falls, +0x1168 = the respawn delay (0x9ca1e0 in a scripted session 0xaf63b0, else 0x9c5430). */
void rx_world_rider_fall(rx_addr r, uint32_t caller_esi) {
    world_frame f = world_frame_flat(1, 0);   /* push esi */
    SU32(f.esp, 0, caller_esi);
    const rx_addr veh = U32(r, 0x1ac);
    SU32(veh, 0x280, U32(r, 0xb34));
    SU32(U32(veh, 0x234), 0x2374, 1);
    WORLD_CALL_SP(8, rx_world_rider_end_wreck(U32(r, 0x1b4), U32(r, 0xb30)));
    WORLD_CALL_SP(0, rx_world_rider_clear_wreck(r));
    WORLD_CALL_SP(4, rx_world_clip_recorder_log_wreck(r, U32(r, 0xb34)));
    if (U32(r, 0x1ac) != 0) {
        const rx_addr g = U32(r, 0x94);
        WORLD_CALL_SP(4, rx_world_raise_event(U32(r, 0x1ac), 0x009416fc, U32(g, 0x290), g));
    }
    SU32(r, 0x11d4, U32(r, 0x11d4) + 1);
    SU32(r, 0x1168, U32(0, U32(0, 0x00af63b0) != 0 ? 0x009ca1e0 : 0x009c5430));
    world_frame_leave(&f);
}
RX_FUNCTION(0x0054ed10, rx_world_rider_fall)

/* 0055a050 (ECX = group, ESI = rider; EAX): the rider's index in the group's list (+0x27c / +0x280 for a rider whose
 * +0x908 is set, else +0x268 / +0x26c; entries' +0x74 is the rider), -1 when absent */
int32_t rx_world_group_rider_index(rx_addr g, rx_addr rider) {
    const int use2 = rider != 0 && U32(rider, 0x908) != 0;
    const rx_addr list = U32(g, use2 ? 0x27c : 0x268);
    const int32_t n = I32(g, use2 ? 0x280 : 0x26c);
    for (int32_t i = 0; i < n; i++)
        if (U32(U32(list, 4u * (uint32_t)i), 0x74) == rider) return i;
    return -1;
}
RX_FUNCTION(0x0055a050, rx_world_group_rider_index)

/* ---- ActivityAudioLogic (thiscall player, rider; RET 8): the activity's own player (+0x50) with a live wreck checker
 *      (rider +0x190: +4 set, +0x10 clear) ---- */
static int audio_logic_ours(rx_addr self, uint32_t player, rx_addr rider) {
    if (player != U32(self, 0x50) || rider == 0) return 0;
    const rx_addr w = U32(rider, 0x190);
    return w != 0 && U32(w, 4) != 0 && U32(w, 0x10) == 0;
}
/* 00581c30 ActivityAudioLogic::vf13: the crowd's wreck reaction (00674540 kind 9) and, for wreck kinds other than 4 / 7 / 10
 * in 2..12, the rider's sound slot 2 (006817c0) */
void rx_world_activity_audio_vf13(rx_addr self, uint32_t player, rx_addr rider) {
    if (!audio_logic_ours(self, player, rider)) return;
    world_frame f = world_frame_flat(1, 0);   /* push esi */
    WORLD_CALL_SP(0, rx_world_crowd_event(9));
    const uint32_t k = U32(U32(rider, 0x190), 0xb44) - 2;
    if (k <= 0xa && k != 2 && k != 5 && k != 8)   /* byte table 0x581c9c */
        WORLD_CALL_SP(4, rx_world_rider_sound_start(rider, 2));
    world_frame_leave(&f);
}
RX_FUNCTION(0x00581c30, rx_world_activity_audio_vf13)
/* 00581cb0 ActivityAudioLogic::vf14 (EBP = the caller's, passed on): stop the rider's sound slot 5 (00681840) */
void rx_world_activity_audio_vf14(rx_addr self, uint32_t player, rx_addr rider, uint32_t ebp) {
    if (!audio_logic_ours(self, player, rider)) return;
    world_frame f = world_frame_flat(1, 0);
    WORLD_CALL_SP(4, rx_world_rider_sound_stop(rider, 5, ebp));
    world_frame_leave(&f);
}
RX_FUNCTION(0x00581cb0, rx_world_activity_audio_vf14)

/* ---- red-black tree (std::map nodes: +0 left, +4 parent, +8 right, +0x11 is-nil; tree +0x18 head, head +4 root) ---- */

/* 00586660 (ECX = node; stdcall tree; RET 4; EAX = the new subtree root): rotate left */
rx_addr rx_world_tree_rotate_left(rx_addr x, rx_addr tree) {
    const rx_addr y = U32(x, 8);
    SU32(x, 8, U32(y, 0));
    if (U8(U32(y, 0), 0x11) == 0) SU32(U32(y, 0), 4, x);
    SU32(y, 4, U32(x, 4));
    const rx_addr head = U32(tree, 0x18);
    if (x == U32(head, 4)) SU32(head, 4, y);
    else if (x == U32(U32(x, 4), 0)) SU32(U32(x, 4), 0, y);
    else SU32(U32(x, 4), 8, y);
    SU32(y, 0, x);
    SU32(x, 4, y);
    return y;
}
RX_FUNCTION(0x00586660, rx_world_tree_rotate_left)

/* 005866b0 (ECX = node; stdcall tree; RET 4; EAX = the new subtree root): rotate right */
rx_addr rx_world_tree_rotate_right(rx_addr x, rx_addr tree) {
    const rx_addr y = U32(x, 0);
    SU32(x, 0, U32(y, 8));
    if (U8(U32(y, 8), 0x11) == 0) SU32(U32(y, 8), 4, x);
    SU32(y, 4, U32(x, 4));
    const rx_addr head = U32(tree, 0x18);
    if (x == U32(head, 4)) SU32(head, 4, y);
    else if (x == U32(U32(x, 4), 8)) SU32(U32(x, 4), 8, y);
    else SU32(U32(x, 4), 0, y);
    SU32(y, 8, x);
    SU32(x, 4, y);
    return y;
}
RX_FUNCTION(0x005866b0, rx_world_tree_rotate_right)

/* 00586c10 (ESI = checked iterator {container, node}): --it (the debug checks call the CRT's invalid-parameter handler:
 * no container, or decrementing begin()) */
void rx_world_tree_iterator_decrement(rx_addr it) {
    if (U32(it, 0) == 0) crt_abort();
    const rx_addr node = U32(it, 4);
    if (U8(node, 0x11) != 0) {          /* end(): the rightmost node */
        const rx_addr last = U32(node, 8);
        SU32(it, 4, last);
        if (U8(last, 0x11) != 0) crt_abort();
        return;
    }
    rx_addr l = U32(node, 0);
    if (U8(l, 0x11) == 0) {             /* the rightmost node of the left subtree */
        for (rx_addr n = U32(l, 8); U8(n, 0x11) == 0; n = U32(l, 8)) l = n;
        SU32(it, 4, l);
        return;
    }
    rx_addr p = U32(node, 4);           /* climb while we are a left child */
    while (U8(p, 0x11) == 0 && U32(it, 4) == U32(p, 0)) {
        SU32(it, 4, p);
        p = U32(p, 4);
    }
    if (U8(U32(it, 4), 0x11) != 0) { crt_abort(); return; }
    SU32(it, 4, p);
}
RX_FUNCTION(0x00586c10, rx_world_tree_iterator_decrement)

/* 00593220 (stdcall registry, name; RET 8; EAX): under the registry lock (0xaf667c) the value (+0x28) registered under the
 * name (std::string 0xaf6660 = name; StringMap_Find on registry + 8), 0 when absent. Frame: push ebp; and esp, -8; sub 0x14. */
uint32_t rx_world_registry_find(rx_addr registry, rx_addr name) {
    world_frame f = world_frame_enter(0x14, 3, 0);
    const rx_addr A = f.ebp & ~7u, it = A - 8;
    rx_set_sp(A - 0x20);
    RX_OS(0x0091600c, 0x00af667c);            /* EnterCriticalSection */
    crt_str_operator_assign(0x00af6660, name);   /* std::string::operator=(const char*) (IAT 0x916190) */
    const rx_addr map = registry + 8;
    WORLD_CALL_SP(8, rx_world_string_map_find(map, it, 0x00af6660));
    const rx_addr cont = U32(it, 0), node = U32(it, 4);
    const rx_addr head = U32(map, 0x18);
    SU32(A - 0xc, 0, head);
    if (cont == 0 || cont != U32(map, 0)) crt_abort();
    uint32_t r = 0;
    if (node != head) {
        if (cont == 0) crt_abort();
        if (node == U32(U32(cont, 0), 0x18)) crt_abort();
        r = U32(node, 0x28);
    }
    RX_OS(0x009160f8, 0x00af667c);            /* LeaveCriticalSection */
    world_frame_leave(&f);
    return r;
}
RX_FUNCTION(0x00593220, rx_world_registry_find)

/* 005c3380 (EDX = string; EAX): the ELF hash of a C string (signed chars) */
uint32_t rx_world_elf_hash(rx_addr s) {
    uint32_t h = 0;
    for (uint32_t i = 0; U8(s, i) != 0; i++) {
        h = (h << 4) + (uint32_t)(int32_t)(int8_t)U8(s, i);
        const uint32_t hi = h & 0xf0000000u;
        if (hi) h ^= (hi >> 24) ^ hi;
    }
    return h;
}
RX_FUNCTION(0x005c3380, rx_world_elf_hash)

/* 00613420 (cdecl a, b): swap two dwords (unless the same address) */
void rx_world_swap_dwords(rx_addr a, rx_addr b) {
    if (a == b) return;
    const uint32_t t = U32(a, 0);
    SU32(a, 0, U32(b, 0));
    SU32(b, 0, t);
}
RX_FUNCTION(0x00613420, rx_world_swap_dwords)

/* 005cf8f0 UIContainer::vf14: mark the container dirty (+0xb8: bit 0 set, bit 1 clear, unless either was set) and, when it
 * is visible (+0xb4 & 0x10), run vf14 of every visible child (+0xcc .. +0xd0, bounds-checked) */
void rx_world_ui_container_vf14(rx_addr self) {
    const uint32_t fl = U32(self, 0xb8);
    if ((fl & 3) == 0) SU32(self, 0xb8, (fl & ~2u) | 1u);
    if ((U8(self, 0xb4) & 0x10) == 0) return;
    world_frame f = world_frame_flat(4, 0);   /* push esi, ebp, edi, ebx */
    const int32_t n = (int32_t)(U32(self, 0xd0) - U32(self, 0xcc)) >> 2;
    for (int32_t i = 0; i < n; i++) {
        world_bounds_check((uint32_t)i, (U32(self, 0xd0) - U32(self, 0xcc)) >> 2);
        const rx_addr c = U32(U32(self, 0xcc) + 4u * (uint32_t)i, 0);
        if (c != 0 && (U8(c, 0xb4) & 0x10)) {
            const rx_addr fn = U32(U32(c, 0), 0x38);
            void (*fp)(rx_addr) = RX_FN(fn, void (*)(rx_addr));
            if (fp) WORLD_CALL_SP(0, fp(c)); else WORLD_ORIG0(fn, RX_THISCALL, c);
        }
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x005cf8f0, rx_world_ui_container_vf14)

/* 005d57e0 (EAX = color vec4, ECX = UI element): set the element's color (+0xe0, through the x87) and run its vf14 (tail) */
void rx_world_ui_set_color(rx_addr elem, rx_addr color) {
    for (uint32_t i = 0; i < 16; i += 4) SU32(elem, 0xe0 + i, world_x87_pass(U32(color, i)));
    const rx_addr fn = U32(U32(elem, 0), 0x38);
    void (*fp)(rx_addr) = RX_FN(fn, void (*)(rx_addr));
    if (fp) fp(elem); else WORLD_ORIG0(fn, RX_THISCALL, elem);
}
RX_FUNCTION(0x005d57e0, rx_world_ui_set_color)

/* 00592d80 (EAX = handler list {+8 container, +0x1c list head, +0x24 lock, +0x3c busy count}; stdcall a, b; RET 8; frame
 * 0x1a4): call every live handler (vf6 () == false) with the event {a, b} (vf2 (&event)): the live ones are collected under
 * the lock (array at [esp + 0x20]), called outside it with the busy count raised; when the count drops back to 0 the
 * collected handlers that died meanwhile (vf6) are deleted (vf0 (1)). The list iterator's debug checks call the CRT handler. */
static uint32_t handler_dead(rx_addr h) {
    const rx_addr fn = U32(U32(h, 0), 0x18);
    uint32_t (*f)(rx_addr) = RX_FN(fn, uint32_t (*)(rx_addr));
    return (f ? f(h) : WORLD_ORIG0(fn, RX_THISCALL, h)) & 0xff;
}
void rx_world_handler_list_call(rx_addr s, uint32_t a, uint32_t b) {
    world_frame f = world_frame_flat(4, 0x1a4);
    const rx_addr sp = f.esp, event = sp + 0x10, handlers = sp + 0x20;
    const rx_addr cs = s + 0x24, busy = s + 0x3c;
    SU32(sp, 0x1c, cs);
    SU32(sp, 0x18, busy);
    RX_OS(0x0091600c, cs);         /* EnterCriticalSection */
    RX_OS(0x00916080, busy);       /* InterlockedIncrement */
    const rx_addr cont = U32(s, 8), head = U32(s, 0x1c);
    SU32(sp, 0x14, head);
    SU32(sp, 0x10, cont);
    uint32_t n = 0;
    for (rx_addr node = U32(head, 0);;) {
        if (cont == 0) crt_abort();
        if (node == head) break;
        if (cont == 0) crt_abort();
        if (node == U32(U32(cont, 0), 0x14)) crt_abort();
        if (handler_dead(U32(node, 8)) == 0) {
            if (node == U32(U32(cont, 0), 0x14)) crt_abort();
            SU32(handlers, 4 * n, U32(node, 8));
            n++;
        }
        if (node == U32(U32(cont, 0), 0x14)) crt_abort();
        node = U32(node, 0);
    }
    RX_OS(0x009160f8, cs);         /* LeaveCriticalSection */
    for (uint32_t i = 0; i < n; i++) {
        SU32(event, 0, a);
        SU32(event, 4, b);
        const rx_addr h = U32(handlers, 4 * i);
        const rx_addr fn = U32(U32(h, 0), 0x08);
        void (*fp)(rx_addr, rx_addr) = RX_FN(fn, void (*)(rx_addr, rx_addr));
        if (fp) WORLD_CALL_SP(4, fp(h, event)); else WORLD_ORIG(fn, RX_THISCALL, h, event);
    }
    RX_OS(0x0091600c, cs);
    if (RX_OS(0x00916084, busy) == 0) {   /* InterlockedDecrement */
        for (uint32_t i = 0; i < n; i++) {
            const rx_addr h = U32(handlers, 4 * i);
            if (handler_dead(h) != 0 && h != 0) {
                const rx_addr fn = U32(U32(h, 0), 0);
                WORLD_ORIG(fn, RX_THISCALL, h, 1);   /* scalar deleting destructor */
            }
        }
    }
    RX_OS(0x009160f8, cs);
    world_frame_leave(&f);
}
RX_FUNCTION(0x00592d80, rx_world_handler_list_call)

/* 004db5b0 Vector16_EraseRange (ESI = vector of 0x50-byte records {+0 container proxy, +0xc first, +0x10 last}; stdcall out
 * iterator, first {container, ptr}, last {container, ptr}; RET 0x14; EAX = out): erase [first, last): the tail moves down
 * (004dbe80) and the end pointer follows; out = {proxy, first}. The checked iterators' range checks call the CRT handler. */
rx_addr rx_world_record_vector_erase(rx_addr v, rx_addr out, uint32_t first_cont, rx_addr first, uint32_t last_cont, rx_addr last) {
    (void)first_cont; (void)last_cont;
    world_frame f = world_frame_flat(4, 0);   /* push ecx, ebx, ebp, edi */
    SU32(out, 0, 0);
    if (v == 0 || U32(v, 0xc) > first || first > U32(v, 0x10)) crt_abort();
    SU32(out, 0, U32(v, 0));
    SU32(out, 4, first);
    if (U32(v, 0xc) > last || last > U32(v, 0x10)) crt_abort();
    if (U32(out, 0) == 0 || U32(out, 0) != U32(v, 0)) crt_abort();
    const rx_addr dst = U32(out, 4);
    if (dst != last) {
        rx_addr end;
        WORLD_CALL_SP(0x10, end = rx_world_record_uninitialized_copy(last, U32(v, 0x10), dst));
        SU32(v, 0x10, end);
    }
    world_frame_leave(&f);
    return out;
}
RX_FUNCTION(0x004db5b0, rx_world_record_vector_erase)

/* 007fa030 (EAX = const char** key, ESI = string hash table {+0 bucket count, +0xc buckets; entries {name, value, hash,
 * next}}; stdcall out value; RET 4; EAX): look the name up (ELF hash 005c3380): 1 and *out = the value when found, else 0 */
uint32_t rx_world_string_table_find(rx_addr table, rx_addr key, rx_addr out) {
    const rx_addr name = U32(key, 0);
    const uint32_t h = rx_world_elf_hash(name);
    for (rx_addr e = U32(U32(table, 0xc), 4u * (h % U32(table, 0))); e != 0; e = U32(e, 0xc)) {
        if (U32(e, 8) != h) continue;
        if (crt_strcmp(U32(e, 0), name) == 0) { SU32(out, 0, U32(e, 4)); return 1; }
    }
    return 0;
}
RX_FUNCTION(0x007fa030, rx_world_string_table_find)

/* 005860e0 (EAX = the parent "where", EDI = tree {+0 container proxy, +0x18 head, +0x1c size}; stdcall out iterator,
 * add-left flag (byte), const value*; RET 0xc; EAX = out): std::map insert at a known position: the new node (0080da40),
 * linked as the root / left / right child (head +0 leftmost / +8 rightmost follow), then the red-black fix-up (+0x10 color:
 * 0 red, 1 black; rotations 00586660 / 005866b0). (The size check's length_error throw at 0x3ffffffe nodes is not
 * reproduced.) */
rx_addr rx_world_tree_insert_at(rx_addr where, rx_addr tree, rx_addr out, uint32_t add_left, rx_addr value) {
    world_frame f = world_frame_flat(3, 0x44);
    const rx_addr head = U32(tree, 0x18);
    rx_addr node;
    WORLD_CALL_SP(0x10, node = rx_world_map_node_new(head, where, head, value));
    SU32(tree, 0x1c, U32(tree, 0x1c) + 1);
    if (where == U32(tree, 0x18)) {
        SU32(U32(tree, 0x18), 4, node);
        SU32(U32(tree, 0x18), 0, node);
        SU32(U32(tree, 0x18), 8, node);
    } else if ((add_left & 0xff) != 0) {
        SU32(where, 0, node);
        if (where == U32(U32(tree, 0x18), 0)) SU32(U32(tree, 0x18), 0, node);
    } else {
        SU32(where, 8, node);
        if (where == U32(U32(tree, 0x18), 8)) SU32(U32(tree, 0x18), 8, node);
    }
#define PARENT(n) U32((n), 4)
#define COLOR(n) U8((n), 0x10)
    rx_addr x = node;
    while (COLOR(PARENT(x)) == 0) {
        const rx_addr p = PARENT(x), g = PARENT(p);
        if (p == U32(g, 0)) {
            const rx_addr y = U32(g, 8);
            if (COLOR(y) == 0) {
                SU8(p, 0x10, 1); SU8(y, 0x10, 1); SU8(PARENT(PARENT(x)), 0x10, 0);
                x = PARENT(PARENT(x));
                continue;
            }
            if (x == U32(p, 8)) { x = p; WORLD_CALL_SP(4, rx_world_tree_rotate_left(x, tree)); }
            SU8(PARENT(x), 0x10, 1);
            SU8(PARENT(PARENT(x)), 0x10, 0);
            WORLD_CALL_SP(4, rx_world_tree_rotate_right(PARENT(PARENT(x)), tree));
        } else {
            const rx_addr y = U32(g, 0);
            if (COLOR(y) == 0) {
                SU8(p, 0x10, 1); SU8(y, 0x10, 1); SU8(PARENT(PARENT(x)), 0x10, 0);
                x = PARENT(PARENT(x));
                continue;
            }
            if (x == U32(p, 0)) { x = p; WORLD_CALL_SP(4, rx_world_tree_rotate_right(x, tree)); }
            SU8(PARENT(x), 0x10, 1);
            SU8(PARENT(PARENT(x)), 0x10, 0);
            WORLD_CALL_SP(4, rx_world_tree_rotate_left(PARENT(PARENT(x)), tree));
        }
    }
#undef PARENT
#undef COLOR
    SU8(U32(U32(tree, 0x18), 4), 0x10, 1);
    SU32(out, 4, node);
    SU32(out, 0, U32(tree, 0));
    world_frame_leave(&f);
    return out;
}
RX_FUNCTION(0x005860e0, rx_world_tree_insert_at)

/* 00646e80 QuickCamera_SetMode (EAX = mode, ESI = camera entity; plain ret; frame sub 0x54): the camera's mode +0x378 and, for
 * the three quick cameras, their name (+0x2d8: 0 "QuickLeft" 0x958a60, 1 "QuickRight" 0x958a6c, 2 "QuickBack" 0x958a78, built
 * as a temporary string, assigned, the camera's vf32 (+0x80) run, the temporary destroyed) and the spring tuning
 * (+0x350 / +0x358 / +0x360 / +0x354 / +0x364 / +0x368: left -10 (runtime global 0x9d78b8), right 10, back 0 with the
 * -1 of 0x9cd9e8 in +0x368; the speeds are 6.0 for +0x354 and +0x364). Then in every case vf32 again and vf19 (+0x4c) with
 * the 1/30 s step 0x977470 (through x87: fld / fstp). The temporaries sit at entry esp - 0x1c / - 0x38 / - 0x54. */
void rx_world_quick_camera_set_mode(rx_addr cam, uint32_t mode) {
    static const uint32_t names[3] = {0x00958a60, 0x00958a6c, 0x00958a78};
    const rx_addr entry = rx_sp();
    rx_set_sp(entry - 0x54);
    SU32(cam, 0x378, mode);
    if (mode <= 2) {
        const rx_addr str = entry - (mode == 0 ? 0x1cu : mode == 1 ? 0x38u : 0x54u);
        crt_str_ctor_cstr(str, names[mode]);
        crt_str_assign_sub(cam + 0x2d8, str, 0, 0xffffffffu);
        WORLD_VCALL0(cam, 0x80);
        crt_str_dtor(str);
        if (mode == 2) {
            SF32(cam, 0x350, 0.0f);
            SF32(cam, 0x358, 0.0f);
            SF32(cam, 0x360, 0.0f);
            SU32(cam, 0x368, U32(0, 0x009cd9e8));
        } else {
            SU32(cam, 0x350, mode == 0 ? U32(0, 0x009d78b8) : rx_fb(10.0f /* 0x92fd88 */));
            SF32(cam, 0x360, 0.0f);
            SF32(cam, 0x358, 0.0f);
            SF32(cam, 0x368, 0.0f);
        }
        SF32(cam, 0x354, 6.0f /* 0x92fe4c */);
        SF32(cam, 0x364, 6.0f /* 0x92fe4c */);
    }
    WORLD_VCALL0(cam, 0x80);
    world_vcall_f(cam, 0x4c, rx_bf(world_x87_pass(0x3d088889u /* 0x977470 */)));
    rx_set_sp(entry);
}
RX_FUNCTION(0x00646e80, rx_world_quick_camera_set_mode)

/* 0054c020 Player_ActiveCameraEntity (EDI = player; EAX): the entity the camera follows: an override (+0x21c -> +0x220);
 * the replay entity +0x99c while it plays (+0x2f4) outside the scripted session state 2 (0xa942b4); in that state the
 * group's (+0x94) vf8 decides for +0x980; the group's session +0x234 (+0x218f0) when its vf8 holds; the spectator +0x9a4;
 * else by camera mode +0x988 (5: the vehicle's reset 007676f0 first, flag 0xa9434f): with a crashed vehicle (+0x1ac +0x280)
 * the mode's entity (+0x974 + 4 mode), with a valid target index +0x994 the target +0x998 (00646e80), else the mode's
 * entity. */
rx_addr rx_world_player_active_camera_entity(rx_addr p) {
    world_frame f = world_frame_flat(1, 0);   /* push esi */
    rx_addr r;
    if (U8(p, 0x21c) != 0) { r = U32(p, 0x220); goto out; }
    {
        const rx_addr a = U32(p, 0x99c);
        const int scripted = U32(0, 0x00a942b4) == 2;
        if (a != 0 && U32(a, 0x2f4) != 0 && !scripted) { r = a; goto out; }
        if (scripted && U32(p, 0x94) != 0 && vcall0(U32(p, 0x94), 0x20) == 0) { r = U32(p, 0x980); goto out; }
    }
    {
        const rx_addr g = U32(p, 0x94);
        if (g != 0 && U32(g, 0x234) != 0 && vcall0(g, 0x20) != 0) { r = U32(U32(p, 0x94), 0x234) + 0x218f0; goto out; }
    }
    r = U32(p, 0x9a4);
    if (r != 0) goto out;
    if (U32(p, 0x988) == 5) {
        const rx_addr g = U32(p, 0x94);
        if (g != 0 && U32(g, 0x234) != 0) {
            WORLD_CALL_SP(0, rx_replay_select_camera(U32(p, 0x1ac), U32(g, 0x234)));   /* 007676f0 (EAX = the vehicle, ESI = the world) */
            SU8(0, 0x00a9434f, 1);
        }
    }
    {
        const rx_addr v = U32(p, 0x1ac);
        if (v != 0 && U32(v, 0x280) != 0) {
            const uint32_t k = U32(p, 0x988);
            r = k == 4 ? U32(p, 0x974) : U32(p, 0x974 + 4 * k);
            goto out;
        }
    }
    if (I32(p, 0x994) >= 0 && U32(p, 0x998) != 0) {
        WORLD_CALL_SP(0, rx_world_quick_camera_set_mode(U32(p, 0x998), U32(p, 0x994)));
        r = U32(p, 0x998);
        goto out;
    }
    r = U32(p, 0x974 + 4 * U32(p, 0x988));
out:
    world_frame_leave(&f);
    return r;
}
RX_FUNCTION(0x0054c020, rx_world_player_active_camera_entity)
