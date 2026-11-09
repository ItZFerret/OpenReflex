/* race_snake_driver.c -- the Snake party mode's SnakeDriver (vtable 0x0093fdb4, 0xfe0 bytes: a NetworkDriver), hand-transliterated from
 * the original (each function's address is in its comment; frames and the stack depth of each call are the original's).
 *
 * SnakeDriver adds +0xf10 the activity, +0xf14 its trail (race_snake_trail.c), +0xf18 a vehicle with a trail of its own kind (1: not an MX
 * / ATV 1 / 2), +0xf1c cut off this round (byte), +0xf20 off the boundary too long, +0xf24 rounds won, +0xf28 its team (-1 free for all),
 * +0xf2c the riders it cut off, +0xf30 the trail's grow length (300 at the round's start, up to 20000), +0xf34 the energy (0 .. 100, 50 at
 * the start: up by 7.5 (+0xf38) and down by 15 (+0xf3c) per second with the speed (+0x150 of the vehicle, 0 .. 80 ft/s around 40)),
 * +0xf40 the start's grace clock (8 s: no trail shrink while it runs), +0xf44 the trail's height offset, +0xf48 a cut's bonus length,
 * +0xf50 the kill clock (0.5 s: the vehicle's state +0xf60 .. +0xf88 kept at a cut), +0xf90 the start's matrix, +0xfd0 the vehicle kind
 * read, +0xfd4 the colour index (the driver's id % 12: the 0x9ca020 table).
 *
 * ONLINE (the driver's network object +0xef0, a Quazal DuplicatedObject; never made offline): the score sync (0050bb90 / 006fea10,
 * 007006e0, 00700310 / 006fed10), the eliminations sent to the host with the NetSnake object (006fd310 / 004024c0). Each networked branch
 * stops with act_unreached. */
#include "race_snake.h"
#include "race_snake_trail.h"
#include "race_driver.h"
#include "race_misc.h"
#include "race_extern.h"
#include "race.h"
#include "race_challenge_activity.h"
#include "build_activity.h"
#include "build_activity_int.h"
#include "build_activity_freeride_run.h"
#include "build_driver_reset.h"
#include "build_driver_dtor.h"
#include "build_activity_teardown.h"
#include "build_alloc.h"
#include "world_veh.h"
#include "phys_body.h"
#include "phys_crt.h"
#include "fx.h"
#include "crt.h"
#include <string.h>

#define VT_SNAKE_DRIVER 0x0093fdb4u
#define TD_DRIVER       0x009cda44u   /* void(BaseDriver*) */
#define STR_OFF_TRACK   0x0093ef24u   /* "Off Track" */
#define STR_SHOW_ARROW  0x0093ef30u
#define STR_HIDE_ARROW  0x0093eef0u

void rx_waypoint_network_vf14(rx_addr driver);   /* 0050ec30 NetworkDriver::vf14 (waypoint_driver_network.c) */

static void session_static(void) {
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
}
static float x87_copy(float v) {
    uint32_t b = rx_fb(v);
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu)) b |= 0x400000u;
    return rx_bf(b);
}
static inline void fcopy(rx_addr d, uint32_t od, rx_addr s, uint32_t os) { SF32(d, od, x87_copy(F32(s, os))); }
static int net_object_live(rx_addr d) {
    const rx_addr net = U32(d, 0xef0);
    return net != 0 && (U8(net, 0x20) & 1) != 0 && (rx_net_ref_valid(net) & 0xff) != 0;
}

/* ---- the event raisers at their own frames (the dispatch keeps the payload's stack address: race_tag.c's note on vf40) ---- */
/* 0053ac80 "Player Eliminated" (ESI = manager, ECX = driver, EDX = next; stdcall act; RET 4): {act, driver, next} (0x9cdaa8).
 * Frame: sub esp, 0x10; push edi (the payload F + 4). */
static void snake_eliminated(rx_addr manager, rx_addr act, rx_addr driver, rx_addr next) {
    ACT_FRAME(F, 0x10, 1);
    SU32(F, 8, driver); SU32(F, 4, act); SU32(F, 0xc, next);
    act_raise(manager, 0x0093ef48 /* "Player Eliminated" */, 0x009cdaa8, F + 4, 0xc);
    ACT_END(F);
}
/* 0053b070 "SnakeTrailHit" (ESI = manager, ECX = driver, EDX = position; stdcall act; RET 4): {act, driver, position} (0x9cdb30). Same frame. */
static void snake_trail_hit_event(rx_addr manager, rx_addr act, rx_addr driver, rx_addr pos) {
    ACT_FRAME(F, 0x10, 1);
    SU32(F, 8, driver); SU32(F, 4, act); SU32(F, 0xc, pos);
    act_raise(manager, 0x0093efa4 /* "SnakeTrailHit" */, 0x009cdb30, F + 4, 0xc);
    ACT_END(F);
}
void rx_snake_trail_hit_raise(rx_addr manager, rx_addr driver, rx_addr pos, rx_addr act) { snake_trail_hit_event(manager, act, driver, pos); }
RX_FUNCTION(0x0053b070, rx_snake_trail_hit_raise)
/* 0053b010 "Set Arrow Target" (ESI = manager, ECX = matrix, EDX = flag; stdcall driver; RET 4): {driver, matrix, flag} (0x9cdb00). Same frame. */
static void snake_arrow_target(rx_addr manager, rx_addr driver, rx_addr matrix, uint32_t flag) {
    ACT_FRAME(F, 0x10, 1);
    SU32(F, 8, matrix); SU32(F, 4, driver); SU32(F, 0xc, flag);
    act_raise(manager, 0x0093ef90 /* "Set Arrow Target" */, 0x009cdb00, F + 4, 0xc);
    ACT_END(F);
}
/* 0053a470 (ESI = manager, EDX = name, ECX = value; stdcall driver; RET 4): {driver, value} (0x9cd9f8). Frame: sub esp, 0xc; push edi. */
static void snake_driver_int(rx_addr manager, rx_addr name, rx_addr driver, uint32_t value) {
    ACT_FRAME(F, 0xc, 1);
    SU32(F, 8, value); SU32(F, 4, driver);
    act_raise(manager, name, 0x009cd9f8, F + 4, 8);
    ACT_END(F);
}
/* 0053a770 (EAX = driver, EDI = name; stdcall manager; RET 4): {driver} (0x9cda44). Frame: push ecx (the payload F). */
static void snake_driver_event(rx_addr manager, rx_addr name, rx_addr driver) {
    ACT_FRAME(F, 0, 1);
    SU32(F, 0, driver);
    act_raise(manager, name, TD_DRIVER, F, 4);
    ACT_END(F);
}

/* ======================================================================================================================== */
/* SnakeDriver                                                                                                               */

/* 00513910 (ECX = driver, EAX = player; stdcall activity, team; RET 8) -> driver: NetworkDriver's constructor (0050e1b0), the class's
 * fields, kind +0xee8 = 5, the colour index 8. Frame: push edi. */
rx_addr rx_snake_driver_construct(rx_addr d, rx_addr act, rx_addr player, uint32_t team) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(1, rx_network_driver_construct(d, player));
    SU32(d, 0xf18, 0);
    SU32(d, 0xf2c, 0);
    SU32(d, 0, VT_SNAKE_DRIVER);
    SU32(d, 0xf10, act);
    SF32(d, 0xf44, 0.0f); SF32(d, 0xf48, 0.0f); SF32(d, 0xf4c, 0.0f);
    SU8(d, 0xfd0, 0);
    SU32(d, 0xf24, 0);
    SU32(d, 0xee8, 5);
    SU32(d, 0xfd4, 8);
    SU32(d, 0xf28, team);
    ACT_END(F);
    return d;
}
RX_FUNCTION(0x00513910, rx_snake_driver_construct)

/* 0050df60 NetworkDriver::sdtor slot 0 of SnakeDriver (thiscall; flags; RET 4) -> driver: NetworkDriver's destructor (0050e2f0), freed
 * with flags & 1. Frame: push esi. */
rx_addr rx_network_driver_sdtor(rx_addr d, uint32_t flags) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(0, rx_network_driver_dtor(d));
    if (flags & 1) ACT_CALL(1, rx_reflex_aligned_free(d));
    ACT_END(F);
    return d;
}
RX_FUNCTION(0x0050df60, rx_network_driver_sdtor)

/* 0053aef0 (EAX = the session channel; stdcall driver; RET 4): "NetworkSnakeActivity" with 00514a60 (proxy 0x940370) */
void rx_snake_driver_listen(rx_addr channel, rx_addr d) {
    ACT_CALL(1, act_subscribe(0x0053aef0, 1, 1, 4, channel, 0x0093ef78 /* "NetworkSnakeActivity" */, 0x00940370, 0x009cde24, d, 0x00514a60));
}
RX_FUNCTION(0x0053aef0, rx_snake_driver_listen)
/* 0053afb0: the listener leaves (frame: sub esp, 0x14; push esi) */
void rx_snake_driver_unlisten(rx_addr channel, rx_addr d) {
    act_unlisten(0, channel, 0x0093ef78, 0x00940370, 0x009cde24, d, 0x00514a60);
}
RX_FUNCTION(0x0053afb0, rx_snake_driver_unlisten)

/* 00513990 slot 1 (ECX = driver): NetworkDriver::vf1; the colour index (+0xfd4: the driver's id % 12) and its trail (0x150 bytes:
 * 0069e3a0 with the activity's scene, the colour 0x9ca020 + 16 * index, the id, the activity's spatial hash); under the profiler scope the
 * driver's "NetworkSnakeActivity" listener (0053aef0) and a live network object's score (ONLINE); the energy 50 (+0xf34, [0x9d78dc]), its
 * rates 7.5 / 15 (+0xf38 / +0xf3c), +0xf20 = 0. Frame: push ebp; mov ebp, esp; and esp, -8; push ebx / ebp / esi / edi. */
void rx_snake_driver_vf1(rx_addr d) {
    ACT_FRAME_EBP(F, 8, 0, 4);
    ACT_CALL(0, rx_network_driver_vf1(d));
    const uint32_t index = U32(d, 0x148) % 0xc;
    SU32(d, 0xfd4, index);
    const rx_addr color = 0x009ca020 + (index << 4);
    rx_addr p;
    ACT_CALLR(p, 1, rx_reflex_aligned_alloc(0x150));
    rx_addr trail = 0;
    if (p != 0) {
        const rx_addr act = U32(d, 0xf10);
        ACT_CALLR(trail, 3, rx_snake_trail_construct(p, U32(U32(act, 0x234), 0x20138), color, U32(d, 0x148), act + 0xff070));
    }
    SU32(d, 0xf14, trail);
    session_static();
    const rx_addr scope = race_profile_begin();
    session_static();
    ACT_CALL(1, rx_snake_driver_listen(U32(0, 0x00da6ca0), d));
    if (net_object_live(d)) act_unreached(0x00513ab6, "ONLINE: the snake driver's score sent (0050bb90 / 006fea10)");
    SF32(d, 0xf34, F32(0, 0x009d78dc));
    SF32(d, 0xf38, 7.5f /* 0x97745c */);
    SU32(d, 0xf20, 0);
    SF32(d, 0xf3c, 15.0f /* 0x9322d0 */);
    race_profile_end(scope);
    ACT_END(F);
}
RX_FUNCTION(0x00513990, rx_snake_driver_vf1)

/* 00513b30 slot 2 (ECX = driver): the engine frame's mark (0084ea00), under the profiler scope the listener leaves (0053afb0) and a live
 * network object's link is released (ONLINE), BaseDriver::vf2, the trail destroyed (0069e460) and freed. Frame: push ebp; mov ebp, esp;
 * and esp, -8; push ecx / ebx / esi / edi. */
void rx_snake_driver_vf2(rx_addr d) {
    ACT_FRAME_EBP(F, 8, 4, 3);
    ACT_CALL(0, rx_engine_frame_mark(U32(U32(0, 0x00a95844), 0x10)));
    session_static();
    const rx_addr scope = race_profile_begin();
    session_static();
    ACT_CALL(1, rx_snake_driver_unlisten(U32(0, 0x00da6ca0), d));
    if (net_object_live(d) && U32(U32(U32(d, 0xef0), 0x1d4), 0) != 0) act_unreached(0x00513bfb, "ONLINE: the snake driver's network link released (007006e0)");
    ACT_CALL(0, rx_base_driver_vf2(d));
    const rx_addr trail = U32(d, 0xf14);
    if (trail != 0) {
        ACT_CALL(0, rx_snake_trail_destroy(trail));
        ACT_CALL(1, rx_reflex_aligned_free(trail));
    }
    race_profile_end(scope);
    ACT_END(F);
}
RX_FUNCTION(0x00513b30, rx_snake_driver_vf2)

/* 00513c50 slot 3 (ECX = driver): the round's reset. NetworkDriver::vf3; under the profiler scope: a local player's vehicle effects at its
 * start (00460020); the start's matrix (+0xf90: the side axis = up x the start's direction (Vec3_Cross), up [0x9cb4b0], the direction, the
 * position) and "Set Arrow Target" with it (0053b010, flag 0); +0x3c0 = 0; the trail reset (0069ec90); the energy 50, cut off until the
 * countdown's end (+0xf1c), the grow length 300 (+0xf30); a live network object's score (ONLINE); the grace clock 8 s (+0xf40), the kill
 * clock 0; once, the vehicle's kind (+0x194: 1 / 2 give +0xf18 = 0, else 1); a new match's rounds won and kills cleared; a spectator's
 * vehicle made visible again; the behaviours' and rules' resets (005092b0 / 005092f0). Frame: push ebp; mov ebp, esp; and esp, -16;
 * sub esp, 0x14; push ebx / esi / edi (the scope at F + 0xc, the side axis at F + 0x10). */
void rx_snake_driver_vf3(rx_addr d) {
    ACT_FRAME_EBP(F, 16, 0x14, 3);
    ACT_CALL(0, rx_network_driver_vf3(d));
    session_static();
    const rx_addr scope = race_profile_begin();
    SU32(F, 0xc, scope);
    const rx_addr player = U32(d, 0x74);
    if (U32(player, 4) != 0) {
        const rx_addr start = U32(d, 0x144);
        ACT_CALL(2, rx_vehicle_effect_query(U32(player, 0x1ac), start, start + 0x10));
    }
    const rx_addr st = U32(d, 0x144);
    ACT_CALL(1, rx_world_vec3_cross(F + 0x10, 0x009cb4b0, st + 0x10));
    const rx_addr m = d + 0xf90;
    SF32(m, 0, F32(F, 0x10)); SF32(m, 4, F32(F, 0x14)); SF32(m, 8, F32(F, 0x18));
    SF32(m, 0xc, 0.0f);
    SF32(m, 0x10, F32(0, 0x009cb4b0)); SF32(m, 0x14, F32(0, 0x009cb4b4));
    SF32(m, 0x1c, 0.0f);
    SF32(m, 0x18, F32(0, 0x009cb4b8));
    fcopy(m, 0x20, st, 0x10); fcopy(m, 0x24, st, 0x14);
    SF32(m, 0x2c, 0.0f);
    fcopy(m, 0x28, st, 0x18);
    fcopy(m, 0x30, st, 0); fcopy(m, 0x34, st, 4);
    SF32(m, 0x3c, 1.0f);
    fcopy(m, 0x38, st, 8);
    ACT_CALL(1, snake_arrow_target(U32(U32(d, 0xf10), 0x290), d, m, 0));
    SF32(d, 0x3c0, 0.0f);
    ACT_CALL(0, rx_snake_trail_reset(U32(d, 0xf14)));
    SF32(d, 0xf34, F32(0, 0x009d78dc));
    SU8(d, 0xf1c, 1);
    SU32(d, 0xf20, 0);
    SF32(d, 0xf30, 300.0f /* 0x935d28 */);
    if (net_object_live(d)) act_unreached(0x00513e1d, "ONLINE: the snake driver's score sent (0050bb90 / 006fea10)");
    SF32(d, 0xf40, 8.0f /* 0x9322b8 */);
    SF32(d, 0xf50, 0.0f);
    if (U8(d, 0xfd0) == 0) {
        const uint32_t kind = U32(U32(U32(d, 0x74), 0x1ac), 0x194);
        SU32(d, 0xf18, (kind == 1 || kind == 2) ? 0u : 1u);
        SU8(d, 0xfd0, 1);
    }
    if (U32(U32(d, 0xf10), 0xff0b0) != 0) { SU32(d, 0xf24, 0); SU32(d, 0xf2c, 0); }
    const rx_addr p = U32(d, 0x74);
    if (U32(p, 0x1e4) != 0) {
        SU8(U32(U32(p, 0x1ac), 0x1328), 0x14e, 0);
        const uint32_t a[1] = {0};
        (void)act_vcall(U32(U32(d, 0x74), 0x1ac), 0xc4, 1, a);
        const rx_addr h = U32(U32(U32(d, 0x74), 0x1ac) + 0x178, 4);
        if (h != 0) SU32(U32(phys_body_record(h), 0x2c), 0x14, 0xffffffffu);
    }
    ACT_CALL(1, rx_driver_behaviors_vf6(d));
    ACT_CALL(1, rx_driver_rules_vf6(d));
    race_profile_end(U32(F, 0xc));
    ACT_END(F);
}
RX_FUNCTION(0x00513c50, rx_snake_driver_vf3)

/* 00514790 (EAX = driver): a human's rules (unless its network object is someone else's: ONLINE): every rule's vf5 (slot 0x14) with 0; out
 * of the boundary for 5 s (its RuleOutOfSnakeBounds, kind 9: +0x50 outside, +0x54 the clock it may stay out until) it is off the boundary
 * (+0xf20 = 1: the energy check cuts it off), "Off Track" (1) and "Hide Arrow" raised. Frame: push ecx / ebx / ebp / esi / edi (the payload
 * F + 0x10). */
void rx_snake_driver_human(rx_addr d) {
    ACT_FRAME(F, 0, 5);
    session_static();
    const rx_addr scope = race_profile_begin();
    const rx_addr net = U32(d, 0xef0);
    if (net == 0 || (rx_net_ref_valid(net) & 0xff) != 0) {
        for (uint32_t i = 0; i < U32(d, 0xee4); i++) act_vcall_f(U32(d, 0xa0 + 4 * i), 0x14, 0.0f);
        rx_addr rule;
        ACT_CALLR(rule, 1, rx_driver_rule_kind9(d));
        if (rule != 0 && U32(rule, 0x50) == 0 && 0.0f > rx_subs(F32(rule, 0x54), F32(U32(rule, 4), 0x130))) {
            SU32(d, 0xf20, 1);
            ACT_CALL(1, snake_driver_int(U32(U32(d, 0xf10), 0x290), STR_OFF_TRACK, d, 1));
            SU32(F, 0x10, d);
            act_raise(U32(U32(d, 0xf10), 0x290), STR_HIDE_ARROW, TD_DRIVER, F + 0x10, 4);
        }
    }
    race_profile_end(scope);
    ACT_END(F);
}
RX_FUNCTION(0x00514790, rx_snake_driver_human)

/* 005149b0 (ECX = driver): a cut: unless the kill clock (+0xf50) runs, the vehicle's position (+0xc0), its rows +0xe0 .. +0xec and its
 * velocity (+0x160) kept at +0xf60 / +0xf70 / +0xf80; the kill clock 0.5 s. */
void rx_snake_driver_keep_state(rx_addr d) {
    if (F32(d, 0xf50) == 0.0f) {
        const rx_addr v = U32(U32(d, 0x74), 0x1ac);
        fcopy(d, 0xf60, v, 0xc0); fcopy(d, 0xf64, v, 0xc4); fcopy(d, 0xf68, v, 0xc8);
        const rx_addr v2 = U32(U32(d, 0x74), 0x1ac);
        for (uint32_t k = 0; k < 0x10; k += 4) SU32(d, 0xf70 + k, U32(v2, 0xe0 + k));
        fcopy(d, 0xf80, v2, 0x160); fcopy(d, 0xf84, v2, 0x164); fcopy(d, 0xf88, v2, 0x168);
    }
    SF32(d, 0xf50, 0.5f /* 0x9322cc */);
}
RX_FUNCTION(0x005149b0, rx_snake_driver_keep_state)

/* 00513f30 slot 7 (thiscall; dt; RET 4): the driver's update. A spectator (+0x908) runs NetworkDriver::vf7 only. NetworkDriver::vf7; a
 * human's rules (00514790); a new trail frame id ([0x9cd974]) refreshes the trail (0069e360); the cuts are decided here only when the
 * game is not paused for them ([0xd92eec]), the network object is ours and the session is not replaying (the flag F + 0xf); the kill
 * clock runs down. The trail: cut off it is cleared (0069ec90; a spectator is cut off), else the vehicle's matrix (vf14 slot 0x38) adds a
 * point (0069e4c0: 6 ft back and wide, 12 for an ATV-kind 2 vehicle, the height + 0xf44). In InProgress the grace clock (+0xf40) runs
 * down. The grow length (+0xf30) changes with the speed's excess over [0x9cc37c] (40): x 2 per second above, x 4 below (at least -50, 0
 * while the grace runs); a remote's host values (ONLINE). In InProgress the energy (+0xf34) moves with the vehicle's +0x150 (0 .. 80:
 * (s / 80 - 0.5) x 2 at the rates +0xf38 / +0xf3c) within 0 .. 100; empty (or off the boundary) it cuts the rider off ("Player
 * Eliminated" with itself, its rider's state 0x15); the grow length at most 20000; then the vehicle (+0xc0) against the trails (0069bba0:
 * half the width): a hit cuts it off, its trail's owner (the first driver for its own trail) scores a kill and the bonus length
 * (+0xf48: both bonuses + 3, at most 20) moves to the owner ("SnakeTrailHit", "Player Eliminated" with the owner); a wrecked vehicle
 * (+0x280) without a hit is cut off too. The trail's glow (+0x144) fades by 2 per second, its intensity (+0xc0) = glow^3 x 4.
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0xd4; push ebx / esi / edi; dt through x87. */
void rx_snake_driver_vf7(rx_addr d, float dt) {
    ACT_FRAME_EBP(F, 16, 0xd4, 3);
    SU32(F, 0x18, 0);
    const rx_addr p = U32(d, 0x74);
    if (U32(p, 0x908) != 0) {
        ACT_CALL(1, rx_network_driver_update(d, x87_copy(dt)));
        ACT_END(F);
        return;
    }
    ACT_CALL(1, rx_network_driver_update(d, x87_copy(dt)));
    if (U32(U32(d, 0x74), 0x10) == 0) ACT_CALL(0, rx_snake_driver_human(d));
    {   const rx_addr trail = U32(d, 0xf14);
        if (U32(trail, 0x100) != U32(0, 0x009cd974)) {
            SU32(trail, 0x100, U32(0, 0x009cd974));
            ACT_CALL(0, rx_snake_trail_refresh(trail));
        } }
    session_static();
    {   const rx_addr scope = race_profile_begin();
        uint8_t may = 0;
        const rx_addr net = U32(d, 0xef0);
        if (U32(0, 0x00d92eec) == 0 && (net == 0 || (rx_net_ref_valid(net) & 0xff) != 0)) {
            rx_addr sm;
            ACT_CALLR(sm, 0, rx_fx_session_manager());
            if (U32(U32(sm, 0x10), 0x1c3c) == 0) may = 1;
        }
        SU8(F, 0xf, may);
        race_profile_end(scope); }
    const rx_addr veh = U32(U32(d, 0x74), 0x1ac);
    SU32(F, 0x14, veh);
    {   const float k = F32(d, 0xf50);
        if (k > 0.0f) {
            const float t = rx_subs(k, dt);
            SF32(d, 0xf50, t);
            if (0.0f > t) SF32(d, 0xf50, 0.0f);
        } }
    const rx_addr v = U32(U32(d, 0x74), 0x1ac);
    const float vx = F32(v, 0x160), vy = F32(v, 0x164), vz = F32(v, 0x168);
    const float sp2 = rx_adds(rx_adds(rx_muls(vx, vx), rx_muls(vy, vy)), rx_muls(vz, vz));
    SF32(F, 0x1c, sp2);
    float speed;
    ACT_CALLR(speed, 1, phys_sqrtf(x87_copy(sp2)));
    SF32(F, 0x10, speed);
    SF32(F, 0x1c, U32(veh, 0x194) == 2 ? 12.0f /* 0x97785c */ : 6.0f /* 0x92fe4c */);
    int add = 1;
    if (U8(d, 0xf1c) != 0) { add = 0; ACT_CALL(0, rx_snake_trail_reset(U32(d, 0xf14))); }
    if (U32(U32(d, 0x74), 0x1e4) != 0) {
        SU8(d, 0xf1c, 1);
        ACT_CALL(0, rx_snake_trail_reset(U32(d, 0xf14)));
    } else if (add) {
        const float w = F32(F, 0x1c);
        const float h = rx_adds(F32(d, 0xf44), w);
        rx_set_sp(F - 0xc);
        SF32(F - 0xc, 8, h); SF32(F - 0xc, 4, x87_copy(w)); SF32(F - 0xc, 0, x87_copy(w));
        const rx_addr m = act_vcall(veh, 0x38, 0, 0);
        rx_set_sp(F - 0x10);
        SU32(F - 0x10, 0, U32(d, 0xf14));
        ACT_CALL(0, rx_snake_trail_update(m, U32(d, 0xf14), x87_copy(w), x87_copy(w), h));
        rx_set_sp(F);
    }
    const rx_addr act = U32(d, 0xf10);
    if (U32(act, 0x21c) == 2) {
        const float g = rx_subs(F32(d, 0xf40), dt);
        SF32(d, 0xf40, g);
        if (0.0f > g) SF32(d, 0xf40, 0.0f);
    }
    {   float x = rx_subs(F32(F, 0x10), F32(0, 0x009cc37c));
        if (x > 0.0f) x = rx_muls(rx_muls(x, dt), 2.0f /* 0x92e330 */);
        else {
            if (rx_bf(0xc2480000u) /* -50, 0x977f20 */ > x) x = rx_bf(0xc2480000u);
            if (F32(d, 0xf40) > 0.0f) x = 0.0f;
            x = rx_muls(rx_muls(x, dt), 4.0f /* 0x9322c0 */);
        }
        SF32(d, 0xf30, rx_adds(F32(d, 0xf30), x)); }
    session_static();
    if (U32(U32(0, 0x00da6cb0), 0x1c3c) == 0) {
        session_static();
        const rx_addr scope = race_profile_begin();
        const rx_addr net = U32(d, 0xef0);
        if (net != 0 && (rx_net_ref_valid(net) & 0xff) == 0) act_unreached(0x005142c9, "ONLINE: the host's snake values (00700310 / 006fed10)");
        race_profile_end(scope);
    }
    if (U32(U32(U32(d, 0x74), 0x94), 0x21c) != 2) goto glow;
    {   float s = F32(veh, 0x150);
        if (s > 80.0f /* 0x977354 */) s = 80.0f;
        else if (0.0f > s) s = 0.0f;
        const float r = rx_muls(rx_subs(rx_muls(s, rx_bf(0x3c4ccccdu) /* 0.0125, 0x977f1c */), 0.5f /* 0x9322cc */), 2.0f);
        const float rate = r > 0.0f ? F32(d, 0xf38) : F32(d, 0xf3c);
        float e = rx_adds(F32(d, 0xf34), rx_muls(rx_muls(rate, r), dt));
        SF32(d, 0xf34, e);
        if (e > 100.0f /* 0x93005c */) e = 100.0f;
        else if (0.0f > e) e = 0.0f;
        SF32(d, 0xf34, e);
        const uint8_t may = U8(F, 0xf);
        if (may != 0 && (e == 0.0f || U32(d, 0xf20) != 0) && U8(d, 0xf1c) == 0) {
            if (U32(d, 0xef0) != 0) act_unreached(0x00514416, "ONLINE: the driver's network handle (004024c0)");
            SU32(F, 0x10, 0);
            if (U32(act, 0xff0c4) != 0) act_unreached(0x00514474, "ONLINE: the cut sent with the NetSnake object (006fd310)");
            SU32(U32(U32(d, 0x74), 0x1b4), 0x5b568, 0x15);
            SU8(d, 0xf1c, 1);
            ACT_CALL(1, snake_eliminated(U32(U32(d, 0xf10), 0x290), U32(d, 0xf10), d, d));
        }
        if (F32(d, 0xf30) > 20000.0f /* 0x9358a4 */) SF32(d, 0xf30, 20000.0f);
        if (may == 0 || U8(d, 0xf1c) != 0) goto glow;
        const rx_addr vv = U32(F, 0x14);
        const uint32_t age = U32(U32(d, 0xf14), 0x140);
        const float radius = rx_muls(F32(F, 0x1c), 0.5f);
        SU32(F, 0x18, vv + 0xc0);
        rx_addr hit;
        rx_set_sp(F - 0x14);
        SU32(F - 0x14, 0, act + 0xff070); SU32(F - 0x14, 4, vv + 0xc0); SF32(F - 0x14, 8, radius); SU32(F - 0x14, 0xc, U32(d, 0x148)); SU32(F - 0x14, 0x10, age);
        ACT_CALLR(hit, 0, rx_snake_hash_hit(act + 0xff070, vv + 0xc0, radius, U32(d, 0x148), age));
        rx_set_sp(F);
        if (hit == 0) {
            if (U32(vv, 0x280) == 0) goto glow;
            if (U32(d, 0xef0) != 0) act_unreached(0x005146c8, "ONLINE: the driver's network handle (004024c0)");
            SU32(F, 0x18, 0);
            if (U32(U32(d, 0xf10), 0xff0c4) != 0) act_unreached(0x00514701, "ONLINE: the cut sent with the NetSnake object (006fd310)");
            SU8(d, 0xf1c, 1);
            ACT_CALL(1, snake_eliminated(U32(U32(d, 0xf10), 0x290), U32(d, 0xf10), d, d));
            goto glow;
        }
        const uint32_t owner_id = U32(hit, 0);
        uint32_t idx = 0;
        if (U32(d, 0x148) != owner_id) {
            const rx_addr a = U32(d, 0xf10);
            for (int32_t i = 0; i < I32(a, 0x26c); i++) if (U32(U32(U32(a, 0x268), 4u * (uint32_t)i), 0x148) == owner_id) { idx = (uint32_t)i; break; }
        }
        const rx_addr owner = U32(U32(U32(d, 0xf10), 0x268), 4 * idx);
        if (U32(d, 0xef0) != 0) act_unreached(0x0051457b, "ONLINE: the driver's network handle (004024c0)");
        SU32(F, 0x10, 0);
        float bonus = rx_adds(rx_adds(F32(d, 0xf48), F32(owner, 0xf48)), 3.0f /* 0x92fe48 */);
        SF32(F, 0x14, bonus);
        if (bonus > 20.0f /* 0x935aac */) SF32(F, 0x14, 20.0f);
        if (U32(U32(d, 0xf10), 0xff0c4) != 0) act_unreached(0x0051467c, "ONLINE: the cut sent with the NetSnake object (006fd310)");
        ACT_CALL(0, rx_snake_driver_keep_state(d));
        rx_addr sm;
        ACT_CALLR(sm, 0, rx_fx_session_manager());
        uint32_t networked;
        ACT_CALLR(networked, 0, rx_session_is_networked(U32(sm, 0x10)));
        if (networked != 0) goto glow;
        ACT_CALL(1, snake_trail_hit_event(U32(U32(d, 0xf10), 0x290), U32(d, 0xf10), d, U32(F, 0x18)));
        SF32(U32(owner, 0xf14), 0x144, 1.0f);
        if (d != owner) SU32(owner, 0xf2c, U32(owner, 0xf2c) + 1);
        SU32(U32(U32(d, 0x74), 0x1b4), 0x5b568, 0x15);
        SU8(d, 0xf1c, 1);
        SF32(owner, 0xf48, F32(F, 0x14));
        SF32(d, 0xf48, 0.0f);
        ACT_CALL(1, snake_eliminated(U32(U32(d, 0xf10), 0x290), U32(d, 0xf10), d, owner));
    }
glow:
    {   const rx_addr trail = U32(d, 0xf14);
        float g = rx_subs(F32(trail, 0x144), rx_muls(dt, 2.0f));
        SF32(trail, 0x144, g);
        if (0.0f > g) SF32(trail, 0x144, 0.0f);
        g = F32(trail, 0x144);
        SF32(trail, 0xc0, rx_muls(rx_muls(rx_muls(g, g), g), 4.0f)); }
    ACT_END(F);
}
RX_FUNCTION(0x00513f30, rx_snake_driver_vf7)

/* 005148f0 slot 9 (thiscall; event; RET 4): a rule's event. RuleOutOfSnakeBounds's (kind 9): "Off Track" with the rule's inside flag (1 when
 * cut off); a rider still in that left the boundary in InProgress gets "Show Arrow", else "Hide Arrow". Frame: push esi / edi (the payload:
 * the argument slot). */
void rx_snake_driver_vf9(rx_addr d, rx_addr event) {
    ACT_FRAME(F, 0, 2);
    if (act_vcall(event, 0xc, 0, 0) != 9) { ACT_END(F); return; }
    const uint32_t inside = U32(event, 0x50);
    const rx_addr act = U32(d, 0xf10);
    rx_set_sp(F - 4);   /* push ebx */
    ACT_CALL(1, snake_driver_int(U32(act, 0x290), STR_OFF_TRACK, d, U8(d, 0xf1c) != 0 ? 1 : inside));
    rx_set_sp(F);
    if (inside == 0 && U8(d, 0xf1c) == 0 && U32(U32(d, 0xf10), 0x21c) == 2) {
        ACT_CALL(1, snake_driver_event(U32(U32(d, 0xf10), 0x290), STR_SHOW_ARROW, d));
        ACT_END(F);
        return;
    }
    SU32(F, 0xc, d);
    act_raise(U32(U32(d, 0xf10), 0x290), STR_HIDE_ARROW, TD_DRIVER, F + 0xc, 4);
    ACT_END(F);
}
RX_FUNCTION(0x005148f0, rx_snake_driver_vf9)

/* 00514c70 slot 16 (thiscall; kind, object; RET 8): NetworkDriver::vf16, then for kind 1 with its own live network object the score sent
 * (ONLINE). Frame: push ebx / esi / edi. */
void rx_snake_driver_vf16(rx_addr d, uint32_t kind, rx_addr object) {
    ACT_FRAME(F, 0, 3);
    ACT_CALL(2, rx_network_driver_vf16(d, kind, object));
    if (kind == 1 && U32(d, 0xef0) == object && (rx_net_ref_valid(object) & 0xff) != 0) act_unreached(0x00514c9c, "ONLINE: the snake driver's network kind change (0050bb90)");
    ACT_END(F);
}
RX_FUNCTION(0x00514c70, rx_snake_driver_vf16)

/* 00514cc0 slot 17 (ECX = driver): the driver leaves mid-game: its rounds won cleared, then a jump to NetworkDriver::vf14. */
void rx_snake_driver_vf17(rx_addr d) {
    SU32(d, 0xf24, 0);
    rx_waypoint_network_vf14(d);
}
RX_FUNCTION(0x00514cc0, rx_snake_driver_vf17)
