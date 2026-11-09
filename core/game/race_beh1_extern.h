/* race_beh1_extern.h -- the AI behaviours' (part 1, core/game/race_beh1_*.c) calls into code they do not own: the vehicle's and
 * its sub-objects' virtual functions, the track / driver / behaviour-part-2 functions of the other rx_race clusters (called by
 * address: the core function when it is registered, else the original), engine / CRT imports. One wrapper per callee, named after
 * what it does; the register conventions are the original's (LTCG) and are given in each comment. */
#pragma once
#include "race.h"
#include "race_extern.h"
#include "race_beh1.h"
#include "loop_vehicle.h"
#include "fourwheel_scalar.h"
#include "../src/rx_x87.h"

/* ---- register calls into original code ---- */
enum { B1_EAX, B1_ECX, B1_EDX, B1_EBX, B1_ESP, B1_EBP, B1_ESI, B1_EDI };
/* A core function called at the original's stack depth (the callee's entry ESP: arg_bytes of pushes + the return address), keeping
 * its result. */
#define B1_CALL_SP(var, arg_bytes, call) do { const rx_addr sp_ = rx_sp(); rx_set_sp(sp_ - (arg_bytes) - 4u); var = (call); rx_set_sp(sp_); } while (0)
static inline rx_regs b1_regs(void) { rx_regs r; memset(&r, 0, sizeof r); return r; }
#define B1_NARGS(...) ((int)(sizeof((uint32_t[]){__VA_ARGS__}) / sizeof(uint32_t)))
#define B1_ORIG(fn, conv, ecx, ...) rx_call_original((fn), (conv), (ecx), B1_NARGS(__VA_ARGS__), (uint32_t[]){__VA_ARGS__})
#define B1_ORIG0(fn, conv, ecx) rx_call_original((fn), (conv), (ecx), 0, (const uint32_t*)0)
#define B1_EX(fn, in, out, ...) rx_call_original_ex((fn), (in), (out), B1_NARGS(__VA_ARGS__), (uint32_t[]){__VA_ARGS__})
#define B1_EX0(fn, in, out) rx_call_original_ex((fn), (in), (out), 0, (const uint32_t*)0)

/* A float result an original left in st(0) (rx_call_original_ex takes the callee's x87 results off the stack). */
static inline float b1_x87_take(const rx_regs* o) { return o->st0; }

/* ---- the driver the behaviour belongs to (behaviour +0x08), its player (+0x74) and vehicle (player +0x1ac) ---- */
static inline rx_addr b1_driver(rx_addr beh) { return U32(beh, 8); }
static inline rx_addr b1_player(rx_addr drv) { return U32(drv, 0x74); }
static inline rx_addr b1_vehicle(rx_addr drv) { return U32(U32(drv, 0x74), 0x1ac); }
/* The AI's tuning row: the per-skill tables (0x9e39c8, 0x9e3ba8, 0xa04c58 ...) are indexed by
 * player +0x8f4 + 4 * (3 * driver +0x328 + driver +0x32c) (float entries). */
static inline uint32_t b1_tuning_index(rx_addr drv) {
    return U32(U32(drv, 0x74), 0x8f4) + (U32(drv, 0x328) * 3u + U32(drv, 0x32c)) * 4u;
}
static inline float b1_tuning(rx_addr table, rx_addr drv) { return F32(table, 4u * b1_tuning_index(drv)); }

/* ---- virtual calls ---- */
/* behaviour vf6 (slot 6, 0x18): reset the behaviour's state (thiscall, no arguments) */
typedef void (*b1_void_fn)(rx_addr self);
static inline void b1_behavior_reset(rx_addr beh) {
    rx_addr fn = race_vfn(beh, 6);
    b1_void_fn f = RX_FN(fn, b1_void_fn);
    if (f) f(beh); else B1_ORIG0(fn, RX_THISCALL, beh);
}
/* vehicle vf43 (0xac): nonzero while the vehicle is airborne (thiscall, no arguments) */
typedef uint32_t (*b1_u32_fn)(rx_addr self);
static inline uint32_t b1_vehicle_airborne(rx_addr veh) {
    rx_addr fn = race_vfn(veh, 0xac / 4);
    b1_u32_fn f = RX_FN(fn, b1_u32_fn);
    return f ? f(veh) : B1_ORIG0(fn, RX_THISCALL, veh);
}
/* vehicle +0x234 sub-object vf19 (0x4c): the steering angle (radians) in st(0) (thiscall, no arguments); _x87: the st(0) value
 * itself (the main thread's x87 runs at 53-bit precision: callers that compute on it before storing need it unrounded) */
static inline rx_f80 b1_vehicle_steer_angle_x87(rx_addr veh) {
    rx_addr obj = U32(veh, 0x234), fn = race_vfn(obj, 0x4c / 4);
    if (fn == 0x004392e0u) return rx_f80_from_f32(rx_sim_vf19(obj));   /* the core port (x87 return: not an int-signature bridge call) */
    /* Four-wheel FLD m32 leaves use native float ABI, like the MX case.
     * Convert that exact m32 value to x87 before the caller's double-precision
     * arithmetic; do not send these known native floats through B1_EX0. */
    if (fn == 0x0044f680u) {
        float value; B1_CALL_SP(value, 0, rx_fourwheel_scalar_truck(obj));
        return rx_f80_from_f32(value);
    }
    if (fn == 0x004a2510u) {
        float value; B1_CALL_SP(value, 0, rx_fourwheel_scalar_buggy(obj));
        return rx_f80_from_f32(value);
    }
    if (fn == 0x0047c460u) {   /* ATV S19 returns an rx_f80 struct, not a C float. */
        rx_f80 (*f)(rx_addr) = RX_FN(fn, rx_f80 (*)(rx_addr));
        if (f) { rx_f80 r; B1_CALL_SP(r, 0, f(obj)); return r; }
    }
    rx_regs in = b1_regs(), o;
    in.r[B1_ECX] = obj;
    B1_EX0(fn, &in, &o);
    return o.st[0];
}
static inline float b1_vehicle_steer_angle(rx_addr veh) { return rx_f80_to_f32(b1_vehicle_steer_angle_x87(veh), 0); }

/* ---- the track cluster (race_track.c / race_rules.c / race_math.c, lead): called directly through race.h ---- */
/* rx_driver_rule_1 0053ae00 (RuleTrackAdvance, type 1), rx_driver_offtrack_penalty_rule 0053aaa0 (type 0xb),
 * rx_race_segments_cross_xz 0050b990, rx_track_find_segment 005387c0, rx_track_nearest_node 005381f0, rx_track_project_xz 00538b30 */

/* ---- the inlined sine / cosine (the same source routine expanded at each site) ---- */
/* .data constants (writable game globals at >= 0x9a6000) are read from memory, .rdata ones are C literals */
static inline float b1_data(rx_addr a) { return F32(0, a); }
/* comiss / fcomi x, 0; jb: -1 (the .data -1.0 at 0x9cd9e8) when x < 0 or unordered, else 1 (0x931f24) */
static inline float b1_sign(float x) { return x >= 0.0f ? 1.0f : F32(0, 0x009cd9e8); }
/* range reduction: pi - (a - trunc(a / 2pi) * 2pi) */
static inline float b1_trig_reduce(float a) {
    float turns = (float)rx_cvtt(a * rx_bf(0x3e22f983u) /* 0x9774a0 1/2pi */);
    return rx_bf(0x40490fdbu) /* 0x977498 pi */ - (a - turns * rx_bf(0x40c90fdbu) /* 0x97749c 2pi */);
}
/* cos of the reduced angle (t2 = t * t, t4 = t2 * t2) */
static inline float b1_cos_poly(float t2, float t4) {
    float p = t2 * rx_bf(0xb46beed0u) /* 0x935d50 */ + rx_bf(0x37cb076bu) /* 0x935d48 */;
    p = p * t4;
    p = p + (t2 * rx_bf(0xbab5a662u) /* 0x935d44 */ + rx_bf(0x3d2aa372u) /* 0x935d4c */);
    p = p * t4;
    return p + (t2 * rx_bf(0xbeffff3du) /* 0x935d40 */ + rx_bf(0x3f7ffff3u) /* 0x935d3c */);
}
/* sin(t) / t of the reduced angle */
static inline float b1_sin_poly(float t2, float t4) {
    float p = t2 * rx_bf(0xb2aee90au) /* 0x935d68 */ + rx_bf(0x36352cc0u) /* 0x935d64 */;
    float q = t2 * rx_bf(0xb94fb563u) /* 0x935d60 */ + rx_bf(0x3c0884a2u) /* 0x935d5c */;
    float r = t2 * rx_bf(0xbe2aaa5cu) /* 0x935d58 */ + rx_bf(0x3f7ffff9u) /* 0x935d54 */;
    p = p * t4;
    p = p + q;
    p = p * t4;
    return p + r;
}
/* Vec3_Cross 00401630 (EAX = a, ECX = b, stack out): out = b x a (written after all reads) */
static inline void b1_cross(const float a[3], const float b[3], float out[3]) {
    float x = a[2] * b[1] - a[1] * b[2];
    float y = a[0] * b[2] - b[0] * a[2];
    float z = b[0] * a[1] - a[0] * b[1];
    out[0] = x; out[1] = y; out[2] = z;
}
