/* vehicle_saved_pose.c -- 00460070 (275 bytes): the divergence reset of a vehicle. When the simulation's state went non-finite
 * (bike_state_divergence_check 00434d00), the vehicle is put back at the pose it saved: a unit quaternion at vehicle +0x210 (x, y,
 * z, w) and a position at +0x200, both only when the saved-pose flag +0x180 is set. The quaternion becomes a forward and an up axis
 * (the standard unit-quaternion to rotation-matrix terms, rows 3 and 2), and 0045f600 (rx_vehicle_place) places the vehicle there.
 *
 * Original frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x50 (no register is saved). Locals, from the frame's ESP E:
 *   E + 0x0c  x*2x      E + 0x10  the quaternion copy (x, y, z, w)    E + 0x20  forward axis (x, y, z; lane 3 not written)
 *   E + 0x30  up axis (x, y, z; lane 3 not written)                   E + 0x40  position copy (all four lanes)
 * The two axes' fourth lanes keep whatever the stack held, which 0045f600 may copy; the frame is therefore the original's. The call is
 * `push &forward; push &position; push vehicle; ECX = &up; call 0045f600` and returns to 0x0046017f.
 * All the arithmetic is single-precision SSE scalar, in the order below (phys_* keep x86's NaN results). */
#include "vehicle_saved_pose.h"
#include "world_util.h"
#include "build_vehicle.h"

#define SAVED_POSE_FLAG        0x180u
#define SAVED_POSITION         0x200u
#define SAVED_QUATERNION       0x210u
#define PLACE_RETURN_SITE      0x0046017fu
#define CONST_TWO              0x0092e330u     /* 2.0f (writable data: read, never folded) */
#define CONST_ONE              0x00931f24u     /* 1.0f */

void rx_vehicle_restore_saved_pose(rx_addr vehicle) {
    world_frame f = world_frame_enter(0x50, 0, 1);
    const rx_addr E = f.esp;
    if (U32(vehicle, SAVED_POSE_FLAG) != 0) {
        const float two = F32(0, CONST_TWO);
        v4_st(E + 0x10, v4_ld(vehicle + SAVED_QUATERNION));
        const float x = F32(E, 0x10), y = F32(E, 0x14), z = F32(E, 0x18), w = F32(E, 0x1c);
        const float z2 = phys_mul(z, two);
        v4_st(E + 0x40, v4_ld(vehicle + SAVED_POSITION));
        const float y2 = phys_mul(y, two);
        const float x2 = phys_mul(x, two);
        const float yz = phys_mul(z2, y);
        const float xx = phys_mul(x, x2);
        SF32(E, 0x0c, xx);
        const float xy = phys_mul(x, y2);
        const float wz = phys_mul(z2, w);
        const float xy_wz = phys_sub(xy, wz);
        const float wx = phys_mul(x2, w);
        const float zz = phys_mul(z2, F32(E, 0x18));
        const float zz_xx = phys_add(zz, F32(E, 0x0c));
        SF32(E, 0x30, xy_wz);                                  /* up.x = 2(xy - wz) */
        const float one = F32(0, CONST_ONE);
        const float xz = phys_mul(x, z2);
        const float yy = phys_mul(y2, F32(E, 0x14));           /* after the first push: [esp + 0x18] is E + 0x14 */
        const float yw = phys_mul(y2, F32(E, 0x1c));
        const float yy_xx = phys_add(yy, F32(E, 0x0c));
        const float up_y = phys_sub(one, zz_xx);               /* 1 - 2(xx + zz) */
        const float up_z = phys_add(yz, wx);                   /* 2(yz + wx) */
        const float fwd_x = phys_add(xz, yw);                  /* 2(xz + yw) */
        const float fwd_y = phys_sub(yz, wx);                  /* 2(yz - wx) */
        const float fwd_z = phys_sub(one, yy_xx);              /* 1 - 2(xx + yy) */
        SF32(E, 0x34, up_y);
        SF32(E, 0x38, up_z);
        SF32(E, 0x20, fwd_x);
        SF32(E, 0x24, fwd_y);
        SF32(E, 0x28, fwd_z);
        /* push &forward; push &position; push vehicle (ECX = &up); call 0045f600 */
        WORLD_PUSHED(PLACE_RETURN_SITE, vehicle, E + 0x40, E + 0x20);
        WORLD_CALL_SP(12, rx_vehicle_place(vehicle, E + 0x40, E + 0x20, E + 0x30));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x00460070, rx_vehicle_restore_saved_pose)
