/* bike_remote.h -- a remote (type 1) simulation following the received pose records: 00432be0 (the record applied, called from
 * RavenVehicle::vf0_o90 0045d510 for S+4 == 1 with a pending record L+0x1184), 004331d0 (the jump straight to a record), and the
 * cubic track filters they use (bike_remote.c).
 *
 * The pose record (00433120 Simulation_GetReplayState, 0x64 bytes): +0x00 position, +0x10 orientation, +0x20 velocity, +0x30 angular
 * velocity (vec4s), +0x40 time, +0x44 flags (0x10000 S+48 set, 0x20000 reverse input), +0x48 .. +0x60 the seven input axes.
 * The simulation keeps the 20 last records at S+0x2390 (stride 0x70; S+0x23d0 = the newest one's time), the last three applied at
 * S+0x2c50 / S+0x2cc0 / S+0x2d30 (S+0x2c90 = the newest time), and the corrections 00432680 smooths in at S+0x2da0 (position),
 * S+0x2db0 (velocity), S+0x2dc0 (orientation, a quaternion) and S+0x2dd0 (angular velocity).
 *
 * A vector track (0x90 bytes; 0042e660 / 0042e6d0 / 0042e720 / 0042e840): +0x00 / +0x10 / +0x20 the last three keys (newest first),
 * +0x30 / +0x34 / +0x38 their times, +0x3c the filter time (the output moves toward the curve by min(5 / it, 1)), +0x40 .. +0x70 the
 * cubic's coefficients c0 .. c3 in (t - +0x34), +0x80 the filtered output. A scalar track (0x30 bytes; 0042ee50 / 0042ee90): +0 /
 * +4 / +8 the keys, +0xc / +0x10 / +0x14 the times, +0x18 the filter time, +0x1c .. +0x28 c0 .. c3, +0x2c the output.
 * An orientation track (0x100 bytes; 0042e8e0 / 0042e9f0 / 0042eb20 / 0042ec50): +0x00 the axis as a vector track, +0x90 the w
 * component and +0xc0 the axis length as scalar tracks, +0xf0 the last quaternion. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* 00432be0 (ECX = the received record, stack S; RET 4) */
void bike_remote_apply_record(rx_addr record, rx_addr S);
/* 004331d0 (EDI = the record, stack S; RET 4): the simulation set to the record and the record ring refilled from it */
void bike_remote_jump_to_record(rx_addr record, rx_addr S);

/* vector tracks */
rx_addr bike_track_reset(rx_addr track);                               /* 0042e660 (stdcall track; RET 4) -> EAX = track */
rx_addr bike_track_reset_to(rx_addr value, rx_addr track);             /* 0042e6d0 (ECX = value, stack track; RET 4) -> EAX = track */
void bike_track_add_key(rx_addr key, float time, rx_addr track);       /* 0042e720 (ECX = key, xmm2 = time, stack track; RET 4) */
void bike_track_evaluate(rx_addr out, float time, rx_addr track);      /* 0042e840 (ECX = out, xmm1 = time, stack track; RET 4) */
/* scalar tracks */
void bike_scalar_track_reset_to(rx_addr track, float value);           /* 0042ee50 (EAX = track, xmm0 = value) */
void bike_scalar_track_add_key(rx_addr track, float time, float value); /* 0042ee90 (EAX = track, xmm0 = time, xmm3 = value) */
/* orientation tracks */
rx_addr bike_orient_track_init(rx_addr track);                         /* 0042e8e0 (ECX = track) -> EAX = track */
void bike_orient_track_reset(rx_addr track);                           /* 0042e9f0 (EDX = track): to the identity [0xd61640] */
void bike_orient_track_add_key(rx_addr track, rx_addr key, float time); /* 0042eb20 (EDX = track, stack key, time; RET 8) */
void bike_orient_track_evaluate(rx_addr track, float time, rx_addr out); /* 0042ec50 (EDX = track, xmm5 = time, ESI = out) */

#ifdef __cplusplus
}
#endif
