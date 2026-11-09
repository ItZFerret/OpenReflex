/* anim_rider_play.h -- the rider's trick / crash animation playback (anim_rider_play.c): the clip-range predicates the
 * performance animator uses (0083e0e0 / 0083e170 / 0083e1d0 / 0083e230), the crash animation step and clip update
 * (00621f40 / 00620a60), the two crash-clip starts (00622160 / 00622460), the first looping block's end (006cd240) and
 * ClampedFloat_Sub (0063b5f0). See the file header. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

uint32_t rx_rider_clip_looping_at(rx_addr anim, float t);   /* 0083e170 (EAX = animator, xmm0 = t) -> EAX */
float    rx_rider_clip_end_at(rx_addr anim, float t);       /* 0083e230 (EAX = animator, xmm1 = t) -> xmm0 */
float    rx_rider_clip_start_at(rx_addr anim, float t);     /* 0083e1d0 (EAX = animator, xmm1 = t) -> xmm0 */
void     rx_rider_clip_block_at(rx_addr anim);              /* 0083e0e0 (ECX = animator) */
void     rx_rider_clamped_float_sub(rx_addr cf, rx_addr value); /* 0063b5f0 (ESI = clamped float; RET 4) */
float    rx_rider_perf_loop_end(rx_addr trick, uint32_t klass); /* 006cd240 (ECX = trick, EAX = class) -> xmm0 */
void     rx_rider_crash_step(rx_addr ra);                   /* 00621f40 (ESI = ra) */
void     rx_rider_crash_update(rx_addr ra, float dt);       /* 00620a60 (stdcall ra, dt; RET 8) */
void     rx_rider_crash_begin(rx_addr player, rx_addr ra, rx_addr trick);   /* 00622160 (ECX = player; stack ra, trick; RET 8) */
void     rx_rider_crash_resume(rx_addr player, rx_addr ra, rx_addr trick);  /* 00622460 (ECX = player; stack ra, trick; RET 8) */
void     rx_rider_crash_request_begin(rx_addr ra, rx_addr player, rx_addr trick);   /* 00621fe0 (ESI = ra, EDI = player, ECX = trick) */
void     rx_rider_stunt_end_outputs(rx_addr e);                                       /* 00623060 (EAX = the rider controller; plain RET) */
void     rx_rider_crash_request_resume(rx_addr ra, rx_addr player, rx_addr trick);  /* 006223e0 (ESI = ra, EDI = player, ECX = trick) */

#ifdef __cplusplus
}
#endif
