/* world_veh_transfer.c -- a vehicle and its rider moved into another world: RavenVehicleMotocross::vf31 00464a00 (MXGame_EnterRealWorld: each player's vehicle from the UI
 * world to the event world) with its callees: the physics world's listener / simulation lists (00769900 / 00769110), the vehicle's terrain-track slots (TerrainTracks_FreeSlot
 * 007c3400 / AllocSlot 007c33a0), the body records moved between the physics worlds' 0xe0-byte record vectors (0042b4a0 / 0042b300 / 0042af70 / 008825f0 / 0042a0d0) and the rider
 * (0061d160). Physics world: +0x1914 listeners {data, count +0x1918}, +0x1940 simulations {data, count +0x1944, capacity +0x1948, growth +0x194c}, +0x1954 the body
 * records (+0xc first, +0x10 last, +0x18 the broadphase cursor, +0x1c the world tag), +0x1960 / +0x1964 the same vector seen from the world. */
#include "build_activity_util.h"
#include "build_alloc.h"
#include "build_rider_util.h"
#include "build_vehicle.h"
#include "anim.h"
#include "anim_extern.h"
#include "anim_scenario.h"
#include "phys_body.h"
#include "phys_coll_grid.h"
#include "phys_terrain.h"
#include "world_misc.h"
#include "world_rider.h"
#include "world_veh_transfer.h"

#define PUSHW(n, ...) do { const uint32_t pw_[] = {__VA_ARGS__}; for (int i_ = 0; i_ < (n); i_++) SU32(rx_sp(), -4 * (n) + 4 * i_, pw_[i_]); } while (0)

/* a swap-with-last removal of a pointer from the array {data +off, count +off + 4}: the first match is overwritten by the last and the count drops */
static void ptr_array_remove(rx_addr o, uint32_t off, rx_addr value) {
    const int32_t n = I32(o, off + 4);
    int32_t i = 0;
    if (n <= 0) return;
    const rx_addr data = U32(o, off);
    while (U32(data, 4u * (uint32_t)i) != value) { i++; if (i >= n) return; }
    if (i < 0) return;
    SU32(data, 4u * (uint32_t)i, U32(data, 4u * (uint32_t)(n - 1)));
    SU32(o, off + 4, U32(o, off + 4) - 1);
}
void rx_world_physics_remove_listener(rx_addr pw, rx_addr listener) { ptr_array_remove(pw, 0x1914, listener); }
RX_FUNCTION(0x00769900, rx_world_physics_remove_listener)
void rx_world_physics_remove_simulation(rx_addr pw, rx_addr sim) { ptr_array_remove(pw, 0x1940, sim); }
RX_FUNCTION(0x00769110, rx_world_physics_remove_simulation)

/* TerrainTracks_FreeSlot 007c3400 (ECX = the tracks, EAX = the slot): a slot of the ring (0 <= slot < +0xc): its strip cut (007c4560: ESI = the tracks), free again (+0x94 = 1, +0x9c =
 * +0x98 = 0), the first word of its +0xc block = 1, +0x68 = -1, +0x20 = the up vector (0x009cb4c0), +0xa0 = +0xa4 = 1, the used count (+0x28) - 1 */
void rx_tracks_free_slot(rx_addr tr, int32_t slot) {
    ACT_FRAME(L, 0, 2);
    if (slot < I32(tr, 0xc) && slot >= 0) {
        ACT_CALL(0, phys_tracks_cut_strip(tr, slot));
        const uint32_t o = 0xc0u * (uint32_t)slot;
        SU32(U32(tr, 0x24), 0x94 + o, 1);
        SU32(U32(tr, 0x24), 0x9c + o, 0);
        SU32(U32(tr, 0x24), 0x98 + o, 0);
        SU32(U32(U32(tr, 0x24), 0xc + o), 0, 1);
        SU32(U32(tr, 0x24), 0x68 + o, 0xffffffffu);
        { const rx_addr v = U32(tr, 0x24) + 0x20 + o; SU32(v, 0, U32(0, 0x009cb4c0)); SU32(v, 4, U32(0, 0x009cb4c4)); SU32(v, 8, U32(0, 0x009cb4c8)); }
        SU32(U32(tr, 0x24), 0xa0 + o, 1);
        SU32(U32(tr, 0x24), 0xa4 + o, 1);
        SU32(tr, 0x28, U32(tr, 0x28) - 1);
    }
    ACT_END(L);
}
RX_FUNCTION(0x007c3400, rx_tracks_free_slot)

/* 0042b4a0 (ESI = dst, EDI = src; -> EAX = dst): the body record copied: +0 / +0x10 / +0x14 / +0x18 through the x87, +0x20 / +0x24, the shape handle +0x2c (reference counted: the
 * old one released (vf0(1) at the last reference), the new one taken), +0x30 / +0x34, the nine vectors +0x40 .. +0xc0, +0xd0, the byte +0xd4 */
rx_addr rx_dyn_record_assign(rx_addr d, rx_addr s) {
    anim_x87_copy_f32(d, s); anim_x87_copy_f32(d + 0x10, s + 0x10); anim_x87_copy_f32(d + 0x14, s + 0x14); anim_x87_copy_f32(d + 0x18, s + 0x18);
    SU32(d, 0x20, U32(s, 0x20)); SU32(d, 0x24, U32(s, 0x24));
    const rx_addr old = U32(d, 0x2c);
    if (U32(s, 0x2c) != old) {
        if (old != 0) {
            SU32(old, 4, U32(old, 4) - 1);
            if (U32(old, 4) == 0 && U32(d, 0x2c) != 0) { const uint32_t one = 1; act_vcall(U32(d, 0x2c), 0, 1, &one); }
        }
        SU32(d, 0x2c, U32(s, 0x2c));
        if (U32(s, 0x2c) != 0) SU32(U32(s, 0x2c), 4, U32(U32(s, 0x2c), 4) + 1);
    }
    SU32(d, 0x30, U32(s, 0x30)); SU32(d, 0x34, U32(s, 0x34));
    for (uint32_t o = 0x40; o <= 0xc0; o += 0x10) rx_st4(d + o, rx_ld4(s + o));
    SU32(d, 0xd0, U32(s, 0xd0));
    SU8(d, 0xd4, U8(s, 0xd4));
    return d;
}
RX_FUNCTION(0x0042b4a0, rx_dyn_record_assign)

/* 0042b300 (stdcall vector, index, source record; RET 0xc): the source record assigned to the record at the index (0042b4a0), its broadphase box updated (0042b000), its
 * tracking record (+0x30: +4 = the vector's world tag +0x1c, +8 = the index) set */
void rx_dyn_records_move_slot(rx_addr vec, uint32_t index, rx_addr src) {
    ACT_FRAME_EBP(L, 8, 0, 4);
    if (index >= (uint32_t)(((int32_t)(U32(vec, 0x10) - U32(vec, 0xc))) / 0xe0)) crt_invalid_parameter_noinfo();
    const rx_addr dst = index * 0xe0u + U32(vec, 0xc);
    ACT_CALL(0, (void)rx_dyn_record_assign(dst, src));
    ACT_CALL(0, phys_body_update_grid(vec, (int32_t)index));
    const rx_addr t = U32(src, 0x30);
    SU32(t, 8, index);
    SU32(t, 4, U32(vec, 0x1c));
    ACT_END(L);
}
RX_FUNCTION(0x0042b300, rx_dyn_records_move_slot)

/* 008825f0 (ECX = index, EDX = the cursor, ESI-less: the grid is EDX's own): BroadphaseGrid_Remove (ESI = the cursor's grid: the index) and the index's two boxes (cursor +0x41c /
 * +0x420, 16 bytes each) set to the empty box (0x00d61670) */
void rx_grid_slot_clear(uint32_t index, rx_addr grid, rx_addr cursor) {
    (void)cursor;
    rx_grid_slot_clear_fr(index, grid, 0, 0, 0, 0);
}
RX_FUNCTION(0x008825f0, rx_grid_slot_clear)
/* the same with the original's frame (entered at the return slot; the caller's EBP / EBX / ESI / EDI): push ebp; and esp, -16; sub
 * esp, 8; push esi, edi; push edi (EDI = the index); call 00882980 (return 00882605: BroadphaseGrid_Remove with its frame, EBX the
 * caller's) -- the vehicle's move leaves those frames as dead stack that its light's event record reads back (known-differences 9) */
void rx_grid_slot_clear_fr(uint32_t index, rx_addr grid, uint32_t ebp, uint32_t ebx, uint32_t esi, uint32_t edi) {
    ACT_FRAME_EBP(L, 16, 8, 2);
    SU32(L_sp, -4, ebp); SU32(L, 4, esi); SU32(L, 0, edi);
    SU32(L, -4, index); SU32(L, -8, 0x00882605);
    { const rx_addr sp = rx_sp(); rx_set_sp(L - 8); phys_grid_remove_fr(grid, index, L - 8, L_sp - 4, ebx, index); rx_set_sp(sp); }
    rx_st4(U32(grid, 0x41c) + 16u * index, rx_ld4(0x00d61670));
    rx_st4(U32(grid, 0x420) + 16u * index, rx_ld4(0x00d61670));
    ACT_END(L);
}

/* 0042af70 (ESI = the record vector; stack index; RET 4): the record at the index replaced by the last (0042b300) unless it is the last, the last one's broadphase entry removed
 * (008825f0), the last record destroyed (0042c080) and dropped */
void rx_dyn_records_remove(rx_addr vec, uint32_t index) {
    ACT_FRAME(L, 0, 2);
    const uint32_t count = (uint32_t)(((int32_t)(U32(vec, 0x10) - U32(vec, 0xc))) / 0xe0);
    const uint32_t last = count - 1;
    if (index != last) {
        if (last >= count) crt_invalid_parameter_noinfo();
        PUSHW(3, vec, index, last * 0xe0u + U32(vec, 0xc));
        ACT_CALL(3, rx_dyn_records_move_slot(vec, index, last * 0xe0u + U32(vec, 0xc)));
    }
    SU32(L, -4, 0x0042afc0);                                     /* call 008825f0 (ECX = last, EDX = the grid) */
    ACT_CALL(0, rx_grid_slot_clear_fr(last, U32(U32(vec, 0x18), 0x30), 0, index, vec, last));
    const rx_addr end = U32(vec, 0x10);
    if (((int32_t)(end - U32(vec, 0xc))) / 0xe0 != 0) {
        void (*destroy)(rx_addr, rx_addr) = RX_FN(0x0042c080, void (*)(rx_addr, rx_addr));
        if (!destroy) act_unreached(0x0042c080, "the records' destructors");
        PUSHW(2, end, index);
        ACT_CALL(2, destroy(end - 0xe0, end));
        SU32(vec, 0x10, U32(vec, 0x10) - 0xe0);
    }
    ACT_END(L);
}
RX_FUNCTION(0x0042af70, rx_dyn_records_remove)

/* one body of a source world's record vector moved into the destination's: the record (at the handle's index) pushed into the destination vector (007743a0) and inserted into its
 * broadphase (00774120: EDI = its new index), its tracking record (+0x30) re-pointed (+4 = the destination's tag +0x1c, +8 = the new index), the source's record removed (0042af70) */
static void move_body(rx_addr L, rx_addr src_world, uint32_t index, rx_addr dst_world) {
    const rx_addr svec = src_world + 0x1954, dvec = dst_world + 0x1954;
    const int32_t scount = ((int32_t)(U32(src_world, 0x1964) - U32(src_world, 0x1960))) / 0xe0;
    SU32(L, 0x1c, index);
    if (index >= (uint32_t)scount) crt_invalid_parameter_noinfo();
    const rx_addr rec = index * 0xe0u + U32(svec, 0xc);
    const uint32_t newidx = (uint32_t)(((int32_t)(U32(dvec, 0x10) - U32(dvec, 0xc))) / 0xe0);
    PUSHW(2, dvec, rec);
    SU32(L, 0x10, dvec);
    ACT_CALL(2, rx_phys_dynamic_push(dvec, rec));
    ACT_CALL(0, rx_phys_dynamic_insert(dvec, newidx));
    const rx_addr t = U32(rec, 0x30);
    SU32(t, 4, U32(dvec, 0x1c));
    SU32(t, 8, newidx);
    PUSHW(1, index);
    ACT_CALL(1, rx_dyn_records_remove(svec, index));
}

/* 0042a0d0 (EAX = the rider's body handle table (+0x5d0; 16 entries {handle, .} of 8 bytes from +0xc); stack the destination world; RET 4 by the caller's pushes): each present
 * handle {+4 the source physics world, +8 the index}: its body moved into the destination's physics world (+0x20134) */
void rx_ragdoll_bodies_move(rx_addr bodies, rx_addr world) {
    ACT_FRAME_EBP(L, 8, 0x14, 3);
    rx_addr p = bodies + 0xc;
    SU32(L, 0x14, p); SU32(L, 0x18, 0x10);
    for (int n = 0x10; n != 0; n--) {
        if (U32(p, 0) != 0) {
            const rx_addr h = U32(p, 0);
            move_body(L, U32(h, 4), U32(h, 8), U32(world, 0x20134));
        }
        p += 8;
        SU32(L, 0x14, p); SU32(L, 0x18, (uint32_t)(n - 1));
    }
    ACT_END(L);
}
RX_FUNCTION(0x0042a0d0, rx_ragdoll_bodies_move)

/* 0061d160 (EAX = the rider; stack the world; RET 4): the rider's physics bookkeeping (+0x5b54c: the old world's listeners lose it, +0x5b558 = the new world, the new world's
 * listener list takes it, +0x14 / +0x24 / +0x28 = 0, +0x1c = 0), its bodies moved (0042a0d0), and without a controller crash state (+0xaf08 == 0) the pose override / stunt /
 * trick / crash ended and the stunt animation reset; the stance +0x5b57c = 0xc, +0x5b580 = 3, the animation restarted (00623500); the entity moved to the world's scene (remove
 * 007d3040 / the scene's id counter +0x19f4 under its lock +4 / add 007d2f70) and the lens-swipe effect (+0x5b6dc) told the new scene's effect set */
void rx_rider_world_move(rx_addr rider, rx_addr world) {
    ACT_FRAME(L, 0, 4);
    const rx_addr rec = rider + 0x5b54c;
    ACT_CALL(1, rx_world_physics_remove_listener(U32(U32(rider, 0x5b558), 0x20134), rec));
    SU32(rec, 0xc, world);
    SU32(L, 0x14, rec);                                                 /* the argument slot (entry + 4) holds the value pushed to the list */
    ACT_CALL(1, (void)rx_world_ptr_vector_push_unique(U32(world, 0x20134) + 0x1914, L + 0x14));
    SU32(rec, 0x1c, 0); SF32(rec, 0x14, 0.0f); SF32(rec, 0x24, 0.0f); SF32(rec, 0x28, 0.0f);
    PUSHW(1, world);
    ACT_CALL(1, rx_ragdoll_bodies_move(rider + 0x5d0, world));
    if (U32(rider, 0xaf08) == 0) {
        ACT_CALL(1, rx_world_rider_end_pose_override(rider));
        ACT_CALL(0, rx_world_rider_end_stunt(rider));
        ACT_CALL(0, rx_world_rider_end_trick(rider));
        ACT_CALL(1, rx_world_rider_end_crash(rider));
        ACT_CALL(0, rx_rider_animation_reset_stunt(rider));
    }
    SU32(rider, 0x5b57c, 0xc); SU32(rider, 0x5b580, 3);
    ACT_CALL(0, rx_rider_animation_restart(rider));
    ACT_CALL(1, rx_scene_manager_remove_entity(rider, U32(rider, 0x10)));
    const rx_addr scene = U32(world, 0x20138);
    SU32(rider, 0x10, scene);
    rr_os_cs_enter(scene + 4);
    const uint32_t id = U32(scene, 0x19f4);
    SU32(scene, 0x19f4, id + 1);
    SU32(rider, 0x14, id);
    rr_os_cs_leave(scene + 4);
    ACT_CALL(1, rx_scene_manager_add_entity(rider, scene));
    {   /* the lens-swipe effect: vf1 (the scene's effect set [0x00a94264 + 4 * the scene's +0x2cc4], "Lens_Swipe_Effect") */
        const rx_addr fx = U32(rider, 0x5b6dc);
        const uint32_t a[2] = {U32(0, 0x00a94264 + 4 * U32(U32(rider, 0x10), 0x2cc4)), 0x009581d8};
        rr_vcall(fx, 4, 2, a);
    }
    ACT_END(L);
}
RX_FUNCTION(0x0061d160, rx_rider_world_move)

/* the terrain-track tables' scene search: guard bit 0 of 0x009e30e0 (the count 0x009e30dc = 0 on first use), then the first table [0x009e30d4 + 4 k] of the scene (+0x44a8); the index is
 * not used afterwards (reads only) */
static void tracks_table_search(rx_addr scene) {
    if ((U8(0, 0x009e30e0) & 1) == 0) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
    const uint32_t n = U32(0, 0x009e30dc);
    for (uint32_t k = 0; k < n; k++) {
        const rx_addr t = U32(0, 0x009e30d4 + 4 * k);
        if (t != 0 && scene == U32(t, 0x44a8)) break;
    }
}

/* the vehicle's terrain-track slot word (+0x1578 .. +0x1584): bit 0x1000 the first tracks object [0x00a95774], 0x2000 the second [0x00a95778], else (0x4000) the third [0x00a9577c] */
static void free_slot_word(uint32_t w, rx_addr t1, rx_addr t2, rx_addr t3) {
    if (w & 0x1000) { ACT_CALL(0, rx_tracks_free_slot(t1, (int32_t)(w ^ 0x1000))); }
    else if (w & 0x2000) { ACT_CALL(0, rx_tracks_free_slot(t2, (int32_t)(w ^ 0x2000))); }
    else { ACT_CALL(0, rx_tracks_free_slot(t3, (int32_t)(w ^ 0x4000))); }
}

/* RavenVehicleMotocross::vf31 00464a00 (ECX = the vehicle; stack the destination world; RET 4): the vehicle (+0x90 listener, +0x16d4 simulation, +0x188 its world) leaves its world:
 * out of the physics world's listener (00769900) and simulation (00769110) lists and the scene (007d3040), its four terrain-track slots freed and the 25 contact slots' (+0x11b0 + 4 k
 * and +0x310) strips cut; into the destination: its listener (00544cc0) and simulation appended to the physics world's lists (the simulation array grown by its growth step), its
 * body moved (+0x17c) and the scene's entity id taken (+0x19f4, +0x14), the entity added (007d2f70), the rider (+0x1a0) moved (0061d160) and four new track slots allocated */
void rx_vehicle_world_move(rx_addr v, rx_addr world) {
    ACT_FRAME_EBP(L, 8, 0xc, 3);
    SU32(L, 0x14, v);
    const rx_addr oldw = U32(v, 0x188);
    PUSHW(1, v ? v + 0x90 : 0);
    ACT_CALL(1, rx_world_physics_remove_listener(U32(oldw, 0x20134), v ? v + 0x90 : 0));
    PUSHW(1, U32(v, 0x16d4));
    ACT_CALL(1, rx_world_physics_remove_simulation(U32(U32(v, 0x188), 0x20134), U32(v, 0x16d4)));
    PUSHW(1, U32(U32(v, 0x188), 0x20138));
    ACT_CALL(1, rx_scene_manager_remove_entity(v, U32(U32(v, 0x188), 0x20138)));
    tracks_table_search(U32(U32(v, 0x188), 0x20138));
    const rx_addr t1 = U32(0, 0x00a95774), t3 = U32(0, 0x00a9577c);
    {
        const rx_addr t2 = U32(0, 0x00a95778);
        free_slot_word(U32(v, 0x1578), t1, t2, t3);
        free_slot_word(U32(v, 0x157c), t1, t2, t3);
        free_slot_word(U32(v, 0x1580), t1, t2, t3);
        free_slot_word(U32(v, 0x1584), t1, t2, t3);
    }
    {
        rx_addr p = v + 0x11b0;
        SU32(L, 0xc, p); SU32(L, 0x10, 0x19);
        for (int n = 0x19; n != 0; n--) {
            for (int k = 0; k < 2; k++) {
                int32_t w = (int32_t)U32(p, k == 0 ? 0x310 : 0);
                if (w >= 0) {
                    w ^= 0x4000;
                    if (w < I32(t3, 0xc) && w >= 0) {
                        ACT_CALL(0, phys_tracks_cut_strip(t3, w));
                        SU32(U32(t3, 0x24), 0x98 + 0xc0u * (uint32_t)w, 1);
                        SU32(t3, 0x28, U32(t3, 0x28) - 1);
                    }
                }
            }
            p += 4;
            SU32(L, 0xc, p); SU32(L, 0x10, (uint32_t)(n - 1));
        }
    }
    SU32(v, 0x188, world);
    const rx_addr pw = U32(world, 0x20134);
    SU32(L, 0x10, v + 0x90);
    PUSHW(1, L + 0x10);
    ACT_CALL(1, (void)rx_world_ptr_vector_push_unique(pw + 0x1914, L + 0x10));
    {   /* the simulation appended to the simulation array {data, count, capacity, growth} at +0x1940 */
        const rx_addr arr = pw + 0x1940, sim = U32(v, 0x16d4);
        SU32(L, 0xc, sim);
        if (I32(arr, 4) >= I32(arr, 8)) {
            const uint32_t ncap = U32(arr, 0xc) + U32(arr, 8);
            const uint64_t bytes = (uint64_t)ncap * 4u;
            rx_addr blk;
            PUSHW(1, (bytes >> 32) ? 0xffffffffu : (uint32_t)bytes);
            ACT_CALLR(blk, 1, rx_reflex_aligned_alloc((bytes >> 32) ? 0xffffffffu : (uint32_t)bytes));
            PUSHW(3, blk, U32(arr, 0), 4u * U32(arr, 8));
            ACT_CALL(3, (void)crt_memcpy(blk, U32(arr, 0), 4u * U32(arr, 8)));
            SU32(L, 0x14, blk);
            PUSHW(1, U32(arr, 0));
            ACT_CALL(1, rx_reflex_aligned_free(U32(arr, 0)));
            SU32(arr, 8, ncap);
            SU32(arr, 0, blk);
        }
        SU32(U32(arr, 0), 4u * U32(arr, 4), sim);
        SU32(arr, 4, U32(arr, 4) + 1);
    }
    if (U32(v, 0x17c) != 0) {
        const rx_addr h = U32(v, 0x17c);
        move_body(L, U32(h, 4), U32(h, 8), pw);
    }
    {
        const rx_addr scene = U32(U32(v, 0x188), 0x20138);
        rr_os_cs_enter(scene + 4);
        const uint32_t id = U32(scene, 0x19f4);
        SU32(scene, 0x19f4, id + 1);
        rr_os_cs_leave(scene + 4);
        SU32(v, 0x14, id);
        PUSHW(1, scene);
        ACT_CALL(1, rx_scene_manager_add_entity(v, scene));
    }
    if (U32(v, 0x1a0) != 0) {
        PUSHW(1, world);
        ACT_CALL(1, rx_rider_world_move(U32(v, 0x1a0), world));
    }
    tracks_table_search(U32(world, 0x20138));
    {
        const rx_addr ta = U32(0, 0x00a95774), tb = U32(0, 0x00a95778);
        int32_t s;
        PUSHW(1, 1); ACT_CALLR(s, 1, rx_tracks_alloc_slot(ta, 1)); SU32(v, 0x1578, (uint32_t)s | 0x1000);
        PUSHW(1, 1); ACT_CALLR(s, 1, rx_tracks_alloc_slot(ta, 1)); SU32(v, 0x157c, (uint32_t)s | 0x1000);
        PUSHW(1, 0); ACT_CALLR(s, 1, rx_tracks_alloc_slot(tb, 0)); SU32(v, 0x1580, (uint32_t)s | 0x2000);
        PUSHW(1, 0); ACT_CALLR(s, 1, rx_tracks_alloc_slot(tb, 0)); SU32(v, 0x1584, (uint32_t)s | 0x2000);
    }
    SU32(v, 0x180, 0);
    ACT_END(L);
}
RX_FUNCTION(0x00464a00, rx_vehicle_world_move)
