/* phys_math.h -- the packed SSE math kernels (phys_math.c). LTCG register conventions of the originals: arguments and result
 * in xmm registers (sin / atan / floor: xmm0; atan2: xmm0 = y, xmm1 = x; slerp: xmm0 = q0, xmm1 = q1, xmm2 = t). */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
rx_v4 phys_vec4_sin(rx_v4 x);                       /* 0042c240 Vec4Sin_RangeReduced */
rx_v4 phys_vec4_atan(rx_v4 x);                      /* 0042c650 Vec4Atan */
rx_v4 phys_vec4_atan2(rx_v4 y, rx_v4 x);            /* 0042c880 Vec4Atan2 */
rx_v4 phys_quat_slerp(rx_v4 q0, rx_v4 q1, rx_v4 t); /* 0042ca00 QuatSlerp_Simd */
rx_v4 phys_vec4_floor(rx_v4 x);                     /* 0042ccd0 Vec4Floor */
void  phys_vec4_sincos(rx_v4 x, rx_addr sin_out, rx_addr cos_out);   /* 0042c390 Vec4Cos_RangeReduced (cdecl sin*, ECX cos*) */
rx_v4 phys_vec4_splat_lane(rx_v4 x, int32_t index); /* 0042caf0 Vec4_SplatLane (cdecl index) */
#if defined(RX_ORACLE) || defined(RX_FOURWHEEL_ACTIVE_STACK)
void phys_quat_slerp_dead_stack(rx_addr esp, uint32_t ebp, rx_v4 q0, rx_v4 q1, rx_v4 t);
#endif
#ifdef __cplusplus
}
#endif
