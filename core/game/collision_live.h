#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Same exact entered-frame APIs; optional live value is actual packedXMM0.
 * Dispatcher rejected/no-pair paths retain incoming bits. Pair roles outside
 * the ten typed closures run their core generator and keep the incoming bits
 * (their XMM0 has no reader: phys_contact_pair_observed). */
void phys_contact_dispatch_observed_live(rx_addr M,rx_addr A,rx_addr QA,rx_addr PA,rx_addr B,rx_addr QB,rx_addr PB,uint32_t flag,rx_v4*live_xmm0);
void phys_pair_triangles_segments_observed_live(rx_addr PS,rx_addr QT,rx_addr M,rx_addr T,rx_addr PT,rx_addr S,rx_addr QS,uint32_t flag,rx_v4*live_xmm0);
void rx_collision_dispatch_pair_observed_live(rx_addr target,rx_addr ra,unsigned nargs,rx_addr expected,uint32_t compare_left,uint32_t compare_right,unsigned compare_width,rx_v4*live_xmm0);
#ifdef __cplusplus
}
#endif
