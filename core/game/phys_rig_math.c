/* phys_rig_math.c -- calls of the world math helpers the rig code uses (see phys_rig_math.h): the world unit's ports at the
 * original's call-site stack depth (`n` stack words and the return address below the caller's stack pointer). */
#include "phys_rig_math.h"
#include "world_veh.h"

#define RIG_CALL(n, call) do { const rx_addr sp_ = rx_sp(); rx_set_sp(sp_ - 4u * (n) - 4u); call; rx_set_sp(sp_); } while (0)

void rig_m3_from_quat(rx_addr out, rx_addr q) { RIG_CALL(1, rx_world_quat_to_matrix3(out, q)); }
void rig_m3_skew(rx_addr out, rx_addr v) { RIG_CALL(1, rx_world_matrix3_skew(out, v)); }
void rig_m3_mul(rx_addr out, rx_addr A, rx_addr B) { RIG_CALL(0, rx_world_matrix3_mul(out, A, B)); }
void rig_m3_mul_vec(rx_addr out, rx_addr v, rx_addr M) { RIG_CALL(1, rx_world_matrix3_mul_vec3(out, v, M)); }
void rig_m3_transpose(rx_addr dst, rx_addr src) { RIG_CALL(1, rx_world_matrix3_transpose(dst, src)); }
void rig_m3_inverse(rx_addr dst, rx_addr src) { RIG_CALL(1, rx_world_matrix3_inverse(dst, src)); }
void rig_block_inverse(rx_addr A, rx_addr B, rx_addr C, rx_addr D, rx_addr o11, rx_addr o12, rx_addr o21, rx_addr o22) {
    RIG_CALL(8, rx_world_block_inverse(A, B, C, D, o11, o12, o21, o22));
}
