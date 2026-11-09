/* world_extern.h -- the world code's calls into functions it does not own (collision / broad phase / math = rx_phys): each wrapper
 * calls the owner's core function (sweep 2026-10-19; they all ran the original before), with the original's argument pushes and
 * return slot on the game stack (WORLD_CALL_SP). `c` is the register context the original holds at the call: kept in the
 * signatures for the call sites, unused now that no original runs here. */
#pragma once
#include "world_util.h"
#include "phys_body.h"
#include "phys_terrain.h"
#include "phys_coll_shape.h"
#include "phys_coll_misc.h"
#include "phys_mech.h"
#include "phys_contact.h"
#include "phys_coll_grid.h"

/* 0085ddd0 Collision_ContactDispatch (thiscall ECX = contact set; shape A, pos A, rot A, shape B, pos B, rot B, flag; RET 0x1c):
 * the narrow phase of a shape pair into the contact set (count +0, records of 0x30 bytes from +0x10, feature ids +0x5b0 /
 * +0x628, touching flag +0x6a0). */
static inline void world_contact_dispatch(rx_addr set, rx_addr shape_a, rx_addr pos_a, rx_addr rot_a, rx_addr shape_b,
                                          rx_addr pos_b, rx_addr rot_b, uint32_t flag, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(0x1c, phys_contact_dispatch(set, shape_a, pos_a, rot_a, shape_b, pos_b, rot_b, flag));
}
/* 0085dcf0 Contacts_Average (ESI = contact set; stack out; RET 4): the set's mean point / normal into out[2]. */
static inline void world_contacts_average(rx_addr set, rx_addr out, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(4, phys_contacts_average(set, out));
}
/* 00882d90 BroadphaseGrid_QueryCells (ESI = grid, ECX = max corner; stack min corner; RET 4) */
static inline void world_broadphase_query_cells(rx_addr grid, rx_addr box_min, rx_addr box_max, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(4, phys_grid_query_cells(grid, box_min, box_max));
}
/* 00883020 BroadphaseGrid_QueryBoxes (ESI = grid, EDX = min corner; stack max corner; RET 4) */
static inline void world_broadphase_query_boxes(rx_addr grid, rx_addr box_min, rx_addr box_max, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(4, phys_grid_query_boxes(grid, box_min, box_max));
}
/* 0084fe00 (thiscall ECX = a body's water state; stack vector in / out; RET 4): the water drag of a push. */
static inline void world_water_filter(rx_addr water_state, rx_addr v, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(4, phys_plane_clip(water_state, v));
}
/* 0042b000 (ECX = keyed-object index, ESI = the keyed vector header world +0x1994): the keyed object's broad-phase refresh. */
static inline void world_keyed_refresh(uint32_t index, rx_addr header, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(0, phys_body_update_grid(header, (int32_t)index));
}
/* 0042bd90 (EAX = handle body {+4 world, +8 index}) -> the generic object world +0x1960 + 0xe0 * index */
static inline rx_addr world_handle_object(rx_addr body, world_ctx c) {
    (void)c;
    rx_addr r;
    WORLD_CALL_SP(0, r = phys_body_record(body));
    return r;
}
/* 007c5950 (thiscall water; stack point in / out): is the point inside the water body (EAX != 0) */
static inline uint32_t world_water_contains(rx_addr water, rx_addr point, world_ctx c) {
    (void)c;
    uint32_t r, edx;
    WORLD_CALL_SP(4, r = phys_terrain_base_height_at(water, point, &edx));
    return r;
}
/* 008454c0 (EAX = shape): the shape's radius in xmm0 lane 0 */
static inline float world_shape_radius(rx_addr shape, world_ctx c) {
    (void)c;
    rx_v4 v;
    WORLD_CALL_SP(0, v = phys_shape_vec20(shape));
    return v.v[0];
}
