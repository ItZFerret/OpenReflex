/* loop_cam_parts_view.h -- the Garage's parts-view camera MXUIPartsViewCamera (vtable 0x0095a6dc; the camera at MXGarage + 0x10,
 * MXGame + 0x15580; also the base of UIGateSelectionCameraEntity, whose vf19 00404020 calls it): its update vf19 00656070 and the
 * functions it calls that were not in the core (loop_cam_parts_view.c).
 *
 * Fields (00655d00 builds them, build_mxgame.c): +0x310 .. +0x34c the camera's 4x3 placement (rows x / y / z, +0x340 the
 * translation); the three blends, each a duration / elapsed pair, a goal, the start the elapsed time restarts from and the
 * current value: the look-at point (+0x3c8 / +0x3cc, goal +0x350, start +0x360, current +0x3a0), the eye offset (+0x3c0 / +0x3c4,
 * +0x370, +0x380, +0x390), the up vector (+0x480 / +0x484, +0x460, +0x470, +0x3b0); +0x3d4 the orbit distance; +0x3e0 the mode
 * (0 the free orbit by the stick, else the two springs +0x3fc (yaw) / +0x408 (pitch) {position, target, speed}, limits +0x440 /
 * +0x444 and +0x438 / +0x43c, the angles +0x3f4 / +0x3f8); +0x3ec / +0x3f0 the orbit yaw / pitch (degrees, the pitch limited to
 * +0x434 .. +0x430, the stick rates +0x424 / +0x428); +0x41c the idle auto-rotation (+0x420 the rate, +0x448 the delay, +0x44c the
 * idle time); +0x450 the pad (-1 none); +0x48c the zoom input, +0x490 / +0x494 the zoom range. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* 00656070 MXUIPartsViewCamera::vf19 (thiscall camera, dt; RET 4) */
void rx_parts_view_camera_update(rx_addr cam, float dt);

/* 00658550 (EAX = camera, ESI = out; RET 0) -> EAX = out: the eye offset blend (+0x380 toward +0x370 by +0x3c4 / +0x3c0) */
rx_addr rx_parts_view_blend_offset(rx_addr cam, rx_addr out);
/* 00658d80 (EAX = camera, ESI = out; RET 0) -> EAX = out: the look-at blend (+0x360 toward +0x350 by +0x3cc / +0x3c8) */
rx_addr rx_parts_view_blend_target(rx_addr cam, rx_addr out);
/* 006596a0 (xmm1 = t, stack duration; RET 4) -> xmm0: the quintic ease-out 1 - (1 - t / duration)^5 as the game evaluates it */
float rx_ease_out_quintic(float t, float duration);
/* 0063e160 (ESI = out, EAX = target, ECX = eye; RET 0) -> EAX = out: the look-at matrix (forward = target - eye normalized, the
 * side and up from the world up (0, 1, 0), the translation = eye) */
rx_addr rx_look_at_matrix(rx_addr out, rx_addr target, rx_addr eye);
/* 004073e0 (EAX = row, ECX = out, stack matrix; cdecl) -> EAX = the row: out = the row's x / y / z (x as an x87 copy) */
rx_addr rx_matrix_row3(rx_addr out, rx_addr matrix, uint32_t row);
/* 0063c890 (EAX = p, xmm0 = v): *p = v */
void rx_store_float(rx_addr p, float v);

#ifdef __cplusplus
}
#endif
