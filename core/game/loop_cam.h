/* loop_cam.h -- the game cameras (loop_cam_*.c): the Camera base (vtable 0095a2d8), the camera entities (MXCameraEntity,
 * MXPlayerCamera, MXThirdPersonCamera, MXFirstPersonCamera, quick / start gate / replay tower / animated / path cameras) and the
 * smoothing helpers they share. Layout facts: _re/notes/game-camera.md and the comments of each function. Notes:
 * core/notes/loop.md.
 *
 * Core signatures follow loop_util.h: `this` first, stack arguments in order, LTCG register arguments after them in the order
 * the comment names. */
#pragma once
#include "rx_game.h"
#include "../src/rx_x87.h"
#include "loop_util.h"
#include "loop_extern.h"
#include "phys_quat.h"
#include "phys_terrain.h"
#include "race_misc.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

/* ---- calls into functions other subsystems own ----
 * The owner's core function when it is registered (called at the original's stack depth: loop_push writes the stack arguments
 * and the call site's return address `ret`), else the original. */
/* 00402dc0 (EDX = registry 009e30d4 {pointers, count +8}) -> EAX = the registry entry of the current context (rx_world) */
static inline rx_addr cam_registry_entry(rx_addr registry, rx_addr ret) {
    __typeof__(&rx_world_find_by_controller) f = LOOP_CORE(0x00402dc0, rx_world_find_by_controller);
    if (f) { const rx_addr s = loop_push(0, 0, ret); const rx_addr r = f(registry); loop_pop(s); return r; }
    rx_regs in, out;
    memset(&in, 0, sizeof in);
    in.r[2] = registry;
    rx_call_original_ex(0x00402dc0, &in, &out, 0, 0);
    return out.r[0];
}
/* 007c5aa0 (stdcall world, point; ret 8): is the point inside a volume of the world (EAX != 0); may move the point (rx_phys) */
static inline uint32_t cam_volume_test(rx_addr world, rx_addr point, rx_addr ret) {
    uint32_t a[2] = {world, point};
    __typeof__(&phys_terrain_height_at) f = LOOP_CORE(0x007c5aa0, phys_terrain_height_at);
    if (f) { uint32_t edx; const rx_addr s = loop_push(2, a, ret); const uint32_t r = f(world, point, &edx); loop_pop(s); return r; }
    return rx_call_original(0x007c5aa0, RX_STDCALL, 0, 2, a);
}
/* 007c5950 (thiscall ECX = world, stack point in / out; ret 4): the water test of a point (rx_phys) */
static inline uint32_t cam_water_contains(rx_addr world, rx_addr point, rx_addr ret) {
    uint32_t a[1] = {point};
    __typeof__(&phys_terrain_base_height_at) f = LOOP_CORE(0x007c5950, phys_terrain_base_height_at);
    if (f) { uint32_t edx; const rx_addr s = loop_push(1, a, ret); const uint32_t r = f(world, point, &edx); loop_pop(s); return r; }
    return rx_call_original(0x007c5950, RX_THISCALL, world, 1, a);
}
/* 00401760 (EAX = source, EDX = destination matrix): the transpose of a 4x4 matrix (rx_world) */
static inline void cam_matrix_transpose(rx_addr src, rx_addr dst, rx_addr ret) {
    __typeof__(&rx_world_matrix4_transpose) f = LOOP_CORE(0x00401760, rx_world_matrix4_transpose);
    if (f) { const rx_addr s = loop_push(0, 0, ret); f(dst, src); loop_pop(s); return; }
    rx_regs in, out;
    memset(&in, 0, sizeof in);
    in.r[0] = src; in.r[2] = dst;
    rx_call_original_ex(0x00401760, &in, &out, 0, 0);
}
/* 007f0050 (ESI = the ray object 00d72aac; stdcall in/out point, out normal; ret 8): the ground below a point (EAX != 0 on a hit,
 * the point is replaced by the hit) (rx_race) */
static inline uint32_t cam_ground_probe(rx_addr point, rx_addr normal, rx_addr ret) {
    uint32_t a[2] = {point, normal};
    __typeof__(&rx_scene_ground_probe) f = LOOP_CORE(0x007f0050, rx_scene_ground_probe);
    if (f) { const rx_addr s = loop_push(2, a, ret); const uint32_t r = f(0x00d72aac, point, normal); loop_pop(s); return r; }
    rx_regs in, out;
    memset(&in, 0, sizeof in);
    in.r[6] = 0x00d72aac;
    rx_call_original_ex(0x007f0050, &in, &out, 2, a);
    return out.r[0];
}

/* 0042ce40 QuatSlerp_Packed (EAX = key a, ESI = key b, EDI = out quaternion, cdecl t) (rx_phys); `ret` = the call site's return */
static inline void cam_quat_slerp_packed(rx_addr a, rx_addr b, rx_addr out, float t, rx_addr ret) {
    const uint32_t v[1] = {rx_fb(t)};
    __typeof__(&phys_quat_slerp_packed) f = LOOP_CORE(0x0042ce40, phys_quat_slerp_packed);
    if (f) { const rx_addr s = loop_push(1, v, ret); f(a, b, out, t); loop_pop(s); return; }
    rx_regs in = loop_regs(), o;
    in.r[LOOP_EAX] = a; in.r[LOOP_ESI] = b; in.r[LOOP_EDI] = out;
    rx_call_original_ex(0x0042ce40, &in, &o, 1, v);
}
/* 004dbce0 KeyTrack_FindSegment (EAX = keys of 16 bytes, EDX = count, xmm0 = t; cdecl prev out, next out, fraction out) -> EAX
 * nonzero when t is inside the track (rx_world) */
static inline uint32_t cam_keytrack_find(rx_addr keys, uint32_t n, float t, rx_addr prev, rx_addr next, rx_addr frac, rx_addr ret) {
    const uint32_t v[3] = {prev, next, frac};
    __typeof__(&rx_world_keytrack_find_segment) f = LOOP_CORE(0x004dbce0, rx_world_keytrack_find_segment);
    if (f) { const rx_addr s = loop_push(3, v, ret); const uint32_t r = f(keys, n, t, prev, next, frac); loop_pop(s); return r; }
    rx_regs in = loop_regs(), o;
    in.r[LOOP_EAX] = keys; in.r[LOOP_EDX] = n; in.xmm[0].v[0] = t;
    rx_call_original_ex(0x004dbce0, &in, &o, 3, v);
    return o.r[LOOP_EAX];
}
/* 00839f40 FloatTrack_Sample (ECX = track, EDI = out float; stdcall t) (rx_anim) */
static inline void cam_float_track(rx_addr track, rx_addr out, float t, rx_addr ret) {
    const uint32_t v[1] = {rx_fb(t)};
    __typeof__(&rx_float_track_sample) f = LOOP_CORE(0x00839f40, rx_float_track_sample);
    if (f) { const rx_addr s = loop_push(1, v, ret); f(track, out, t); loop_pop(s); return; }
    rx_regs in = loop_regs(), o;
    in.r[LOOP_ECX] = track; in.r[LOOP_EDI] = out;
    rx_call_original_ex(0x00839f40, &in, &o, 1, v);
}

/* ---- x87 float copies (`fld m32; fstp m32`) ----
 * Numbers (denormals too) pass unchanged. A NaN keeps its payload with the quiet bit set (an SNaN comes out quiet), as the x87
 * hardware and rx_x87 / the oracle's f80 do (seen in an unused field of the render camera on Alpine). */
static inline uint32_t cam_x87_bits(uint32_t b) {
    return ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu)) ? (b | 0x00400000u) : b;
}
static inline float cam_x87_f32(rx_addr a) { return rx_bf(cam_x87_bits(U32(a, 0))); }
static inline void cam_x87_copy(rx_addr dst, rx_addr src) { SU32(dst, 0, cam_x87_bits(U32(src, 0))); }
static inline void cam_x87_copy3(rx_addr dst, rx_addr src) {
    cam_x87_copy(dst, src); cam_x87_copy(dst + 4, src + 4); cam_x87_copy(dst + 8, src + 8);
}

/* ---- inline math the camera code shares ---- */
/* The x87 precision of the thread that runs the cameras: the main thread, control word 0x027f = 53 bits (measured in the
 * MXThirdPersonCamera::vf19 adapter). */
#define CAM_X87_BITS 53
/* x87: st = copysign(0.5, v) (msvcr90 _copysign, a double), st += v at CAM_X87_BITS, then _ftol2_sse (fstp m64 ; cvttsd2si):
 * round half away from zero. */
static inline int32_t cam_round_half_away(double v) {
    rx_f80 s = rx_f80_add(rx_f80_from_f64(copysign(0.5 /* 0x9774e8 */, v)), rx_f80_from_f64(v), CAM_X87_BITS, 0);
    return rx_cvtt(rx_f80_to_f64(s, 0));
}
/* The game's inline sine / cosine: the argument is reduced to y = pi - (|x| - trunc(|x| / 2pi) * 2pi) and a polynomial in y^2
 * (0x935d50 .. 0x935d3c) gives about -cos(x); sin(x) = trig(x + pi/2), cos(x) = -0.0 - trig(x). */
static inline float cam_trig_reduce(float r) {   /* r = |x|: y */
    return 3.1415927f /* 0x977498 */ - (r - ((float)rx_cvtt(r * 0.15915494f /* 0x9774a0 */) * 6.2831855f /* 0x97749c */));
}
static inline float cam_trig_poly(float y) {
    float y2 = y * y, y4 = y2 * y2;
    return ((((y2 * -2.197296e-07f /* 0x935d50 */ + 2.420294e-05f /* 0x935d48 */) * y4) +
             (y2 * -0.001385879f /* 0x935d44 */ + 0.04165978f /* 0x935d4c */)) * y4) +
           (y2 * -0.4999942f /* 0x935d40 */ + 0.9999992f /* 0x935d3c */);
}
static inline float cam_trig(float x) { return cam_trig_poly(cam_trig_reduce(fabsf(x))); }
static inline float cam_sin(float x) { return cam_trig(x + 1.5707964f /* 0x977508 */); }
static inline float cam_cos(float x) { return -0.0f /* 0x9774a8 */ - cam_trig(x); }

/* ---- shared smoothing / math helpers (loop_cam_util.c) ---- */
float rx_cam_wrap_angle(float x);                                              /* 0063c6a0 (x on the stack, result xmm0) */
void  rx_cam_angle_spring(rx_addr s, float dt, float accel, float max_speed, float k);   /* 0063c7c0 (ESI = {pos, target, vel}) */
void  rx_cam_spring(rx_addr s, float dt, float accel, float max_speed, float k);         /* 0063c8c0 (ESI = {pos, target, vel}) */
void  rx_cam_vec3_spring(rx_addr s, float dt, float accel, float max_speed, float k);    /* 0063c9b0 (ESI = {pos, target, vel} x3) */
float rx_cam_rotate_matrix(rx_addr m, float angle);                            /* 0063cf00 (+ 0063cbb0): m = R(angle) * m; returns the scratch value the original leaves in the angle's stack slot */
float rx_cam_spring_target(rx_addr s);                                         /* 0063c7b0 (EAX = s) -> xmm0 */
void  rx_cam_spring_snap(rx_addr s);                                           /* 0063c8a0 (EAX = s) */
float rx_cam_spring_speed(rx_addr s);                                          /* 0063c8b0 (EAX = s) -> xmm0 */
void  rx_cam_set_float_254(rx_addr obj, float v);                              /* 0063d920 (EAX = obj, xmm0 = v) */
void  rx_cam_copy_vec3_10(rx_addr dst, rx_addr src);                           /* 0063c990 (EAX = src, ECX = dst) */

/* ---- MXThirdPersonCamera (loop_cam_third.c) ---- */
void rx_cam_water_ahead(rx_addr cam, rx_addr target, rx_addr dir);            /* 0064f240 (EAX = cam; ret 8) */
void rx_cam_ground_clearance(rx_addr pos);                                     /* 0063e400 (EDI = position vec3) */
void rx_cam_lift_above_water(rx_addr cam, rx_addr pos, rx_addr out, float margin);   /* 00650c60 (ECX = cam, EAX = pos; ret 8) */
void rx_cam_lift_common(rx_addr cam, rx_addr pos, rx_addr out, int fixed, float margin, rx_addr ret_reg, rx_addr ret_vol);   /* (loop_cam_misc.c) */
void rx_cam_orbit_pose(rx_addr cam, rx_addr dir, uint32_t side, rx_addr target, rx_addr out_pos, rx_addr out_look);   /* 006502e0 (ESI = cam, EAX = dir; ret 0x10) */
void rx_cam_orbit_probe(rx_addr cam, rx_addr dir, rx_addr target);             /* 00650250 (EAX = cam, EDI = dir; ret 4) */

#ifdef __cplusplus
}
#endif
