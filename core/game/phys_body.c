/* phys_body.c -- the physics world's generic dynamic objects (0xe0-byte records in a checked vector; world +0x1954 header,
 * begin +0x1960, end +0x1964; native/game/notes/world.md) as the rider / bike bodies use them through handles.
 * Handle (8 bytes): +4 the vector header (world + 0x1954 points... ; the owner of +0xc begin / +0x10 end / +0x18 grid object),
 * +8 the record index.
 * Record: +0x2c shape (vfunc 3 = world bounds at a pose (q*, pos*, min*, max*)), +0x40 rotation, +0x50 position,
 * +0x60 / +0x70 reset pose, +0xb0 / +0xc0 contact accumulators. */
#include "phys_body.h"
#include "crt.h"
#include "phys_coll_shape.h"
#include "phys_coll_grid.h"

/* the vector's _invalid_parameter_noinfo (import slot 0x00916224): an out-of-range index */
static void invalid_parameter(void) { crt_invalid_parameter_noinfo(); }

/* the record count (end - begin) / 0xe0, signed division as the original's multiply-shift */
static int32_t body_count(rx_addr vec) { return (I32(vec, 0x10) - I32(vec, 0xc)) / 0xe0; }

/* 0042bd90 (EAX = handle): the address of the handle's record (vector header handle+4 at world +0x1954: begin at +0xc). */
rx_addr phys_body_record(rx_addr handle) {
    const rx_addr vec = U32(handle, 4) + 0x1954;
    const uint32_t index = U32(handle, 8);
    if (index >= (uint32_t)body_count(vec)) invalid_parameter();
    return index * 0xe0 + U32(vec, 0xc);
}
RX_FUNCTION(0x0042bd90, phys_body_record)

/* 0042b000 (ESI = vector header, ECX = index): moves the record's box in the broadphase grid (header +0x18; skipped without one
 * or for an index past the end): the shape's world bounds at the record's pose (+0x50 rotation, +0x40 position: vfunc 3) into
 * a box on the stack, then BroadphaseGrid_Update (EDI = index; grid, min*, max*). */
void phys_body_update_grid(rx_addr vec, int32_t index) {
    /* frame: push ebp; and esp, -16; sub esp, 0x38 (cookie +0x34, box max +8, box min +0x18); push ebx; push edi */
    const rx_addr sp0 = rx_sp(), F = ((sp0 - 4) & ~15u) - 0x38;
    if (U32(vec, 0x18) == 0) return;
    const int32_t count = body_count(vec);
    if (index >= count) return;
    if ((uint32_t)index >= (uint32_t)count) invalid_parameter();
    rx_set_sp(F - 8);
    const rx_addr rec = (uint32_t)index * 0xe0 + U32(vec, 0xc), shape = U32(rec, 0x2c);
    const rx_addr box_max = F + 0x08, box_min = F + 0x18;
    phys_shape_bounds(U32(U32(shape, 0), 0xc), shape, rec + 0x50, rec + 0x40, box_min, box_max);
    PHYS_CALL_SP(12, phys_grid_update((uint32_t)index, U32(vec, 0x18), box_min, box_max));   /* BroadphaseGrid_Update, RET 0xc */
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0042b000, phys_body_update_grid)

/* 00429910 (ECX = rider rig R): for the 16 bones with a physics body: bone index b = R+0xa764[i], body handle R+0xc + 8 i
 * (-> {+4 vector owner, +8 index}). When the handle's slot is set, the body record gets the bone's pose: +0x40 = position
 * R + 0x400 + 0x80 b, +0x50 = rotation R + 0xe80 + 0x70 b, +0x60 = R + 0x410 + 0x80 b, +0x70 = R + 0xe90 + 0x70 b, and its contact
 * accumulators +0xb0 / +0xc0 cleared (runtime zero vec4 0x00d61670); then the body's grid box is updated (0042b000). */
void phys_rig_bodies_from_bones(rx_addr rig) {
    for (int i = 0; i < 16; i++) {
        const int32_t bone = I32(rig, 0xa764 + 4 * i);
        const rx_v4 pos = v4_ld(rig + 0x400 + 0x80u * (uint32_t)bone);
        const rx_v4 pos_b = v4_ld(rig + 0x410 + 0x80u * (uint32_t)bone);
        const rx_v4 rot = v4_ld(rig + 0xe80 + 0x70u * (uint32_t)bone);
        const rx_v4 rot_b = v4_ld(rig + 0xe90 + 0x70u * (uint32_t)bone);
        const rx_addr slot = rig + 0xc + 8u * (uint32_t)i;
        if (U32(slot, 0) != 0) {
            const rx_addr rec = phys_body_record(U32(slot, 0));
            v4_st(rec + 0x40, pos);
            v4_st(rec + 0x50, rot);
            v4_st(rec + 0x60, pos_b);
            v4_st(rec + 0x70, rot_b);
            v4_st(rec + 0xb0, v4_ld(0x00d61670));
            v4_st(rec + 0xc0, v4_ld(0x00d61670));
        }
        const rx_addr handle = U32(slot, 0);
        phys_body_update_grid(U32(handle, 4) + 0x1954, I32(handle, 8));
    }
}
RX_FUNCTION(0x00429910, phys_rig_bodies_from_bones)
