/* anim_rider_stunt.h -- the rider's stunt animation start (anim_rider_stunt.c): the player's "StuntBegin" listener and the two
 * rider-animation entries it calls. See the file header. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

void     rx_rider_stunt_name(rx_addr dst, uint32_t size, uint32_t klass, rx_addr src);   /* 006ccfc0 (EAX = class, EDI = dst, ESI = size) */
rx_addr  rx_rider_stunt_map_u16(rx_addr map, rx_addr key);                               /* 0063b6e0 (EAX = key, EDI = map; RET 4) */
rx_addr  rx_rider_stunt_map_u32(rx_addr map, rx_addr key);                               /* 00850cd0 (EAX = key, EDI = map; RET 4) */
void     rx_rider_stunt_listener_disable(rx_addr vec, uint32_t id);                      /* 0063b2b0 (RET 8) */
void     rx_rider_stunt_listener_select(rx_addr vec, uint32_t id);                       /* 0063b1e0 (RET 8) */
void     rx_rider_stunt_controls(rx_addr P, rx_addr trick);                              /* 0054d3b0 (EAX = player; RET 4) */
void     rx_rider_stunt_ground(rx_addr ra, uint32_t airborne, uint32_t clip);            /* 00622790 (EDI = rider; RET 8) */
void     rx_rider_stunt_clip(rx_addr ra, rx_addr player, rx_addr trick);                 /* 006226c0 (RET 0xc) */
void     rx_rider_stunt_air(rx_addr ra, rx_addr player, rx_addr trick);                  /* 00622bc0 (ECX = trick; RET 8) */
void     rx_rider_stunt_begin_evt(rx_addr P, rx_addr player, rx_addr trick);             /* 0054d120 "StuntBegin" (ECX; RET 8) */
void     rx_rider_stunt_completed_evt(rx_addr P, rx_addr player, rx_addr trick);         /* 0054d220 "StuntCompleted" (ECX; RET 8) */
void     rx_rider_listeners_broadcast(rx_addr out, rx_addr value, rx_addr proxy, rx_addr begin, rx_addr proxy2, rx_addr end);   /* 0063c190 (RET 0x18) */
void     rx_rider_listeners_broadcast_vec(rx_addr vec, rx_addr value);                   /* 0063b7d0 (EAX = vec; RET 4) */
void     rx_rider_clamped_float_add(rx_addr obj, rx_addr value);                         /* 0063b4b0 (EAX = obj; RET 4) */

#ifdef __cplusplus
}
#endif
