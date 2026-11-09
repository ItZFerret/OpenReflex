/* loop_rider.h -- the procedural rider limb helpers (loop_rider.c). Register arguments first (named in each comment). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void     rx_rider_bone_target_dir(rx_addr out, rx_addr st, rx_addr pose, uint32_t bone);      /* 00632b00 (ESI = out; stdcall 3) */
uint32_t rx_rider_reach_probe(rx_addr st, rx_addr pose, rx_addr hit, rx_addr normal);       /* 00632dc0 (EAX = st; stdcall 3) */
void     rx_rider_stage_twist(rx_addr stage, float value, rx_addr pose);                    /* 00636990 (EDI = pose; stdcall stage, value) */
/* the same with the original's frame (its saved EBX / EBP / ESI: the caller's), entered at the return slot */
void     rx_rider_stage_twist_fr(rx_addr stage, float value, rx_addr pose, uint32_t ebx, uint32_t ebp, uint32_t esi);
void     rx_rider_limb_release(rx_addr st, rx_addr pose);                                 /* 00635e10 (stdcall 2) */
void     rx_rider_limb_reach_contact(rx_addr st, rx_addr pose, rx_addr contact, rx_addr normal); /* 00633010 (stdcall 4) */
#ifdef __cplusplus
}
#endif
