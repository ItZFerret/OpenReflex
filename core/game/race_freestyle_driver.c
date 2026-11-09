/* race_freestyle_driver.c -- FreestyleDriver (vtable 0x0093fafc, 0xf50 bytes): the driver MXActivityFreestyleStub::vf44 makes for
 * each player of a Freestyle event. A NetworkDriver (0050e1b0) with the trick tallies it shows on the HUD and sends over the network:
 *     +0xf10  the score (int)              +0xf14  the last trick's player     +0xf18 .. +0xf2c  the trick categories / score
 *     +0xf30  the score to beat (ghost)    +0xf34  the landings counted       +0xf38  the trick count    +0xf40  airborne
 *     +0xf44  the ghost's best (-1)        +0xf48  the run's duration         +0xf4c  the ghost run's clock
 * Each function is a hand transliteration of the disassembly (address in its comment). The network object (+0xef0) exists only in an
 * online session: offline it is 0 (the session's player table is empty: core/plan/QUEUE.md "w-net"), so the branches that use it are
 * not taken; they stop with act_unreached if ever reached (online-only, the oracle's reference worlds never have one). */
#include "race_freestyle_driver.h"
#include "race.h"
#include "race_extern.h"
#include "race_driver.h"
#include "race_activity.h"
#include "build_activity.h"
#include "build_activity_util.h"
#include "build_driver_dtor.h"
#include "build_driver_reset.h"
#include "build_vehicle.h"
#include "bike_sim.h"
#include "loop_player.h"
#include "world_board_handlers.h"
#include "phys_terrain.h"
#include "phys_body.h"
#include "race_misc.h"
#include <string.h>

/* the ghost driver's "Ghost Session Completed" handler (waypoint_driver_events.c; no header declares them) */
void rx_waypoint_driver_ghost_subscribe(rx_addr channel, rx_addr driver);     /* 0053a5b0 (EAX = channel; stdcall driver) */
void rx_waypoint_driver_ghost_unsubscribe(rx_addr channel, rx_addr driver);   /* 0053a670 (EAX = channel; stdcall driver) */
void rx_waypoint_network_vf14(rx_addr driver);                                /* 0050ec30 NetworkDriver::vf14 (waypoint_driver_network.c) */

/* 00884950 (ECX = the network object) -> AL: the object is the local one */
static int net_valid(rx_addr net) {
    uint32_t r;
    RACE_CORE_CALL(0, r = rx_net_ref_valid(net));
    return (r & 0xff) != 0;
}

/* the network object of an online session (flag bit 0 of +0x20, ours): its score report (0050bc00) and publish (006fea10) */
static void net_publish_scores(rx_addr d) {
    const rx_addr net = U32(d, 0xef0);
    if (net == 0 || (U8(net, 0x20) & 1) == 0) return;
    if (!net_valid(net)) return;
    act_unreached(0x0050bc00, "a Freestyle driver's network score report (online session only)");
}

/* fld dword; fstp dword: the bits, a signalling NaN quieted */
static uint32_t x87_copy(uint32_t bits) {
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x007fffffu) != 0) bits |= 0x00400000u;
    return bits;
}

/* 0050c900 (ECX = driver, EAX = player) -> driver: the NetworkDriver (0050e1b0), the tallies cleared, vtable 0x93fafc, +0xee8 = 1
 * (the driver kind), +0xf44 = -1. Frame: push edi. */
rx_addr rx_freestyle_driver_construct(rx_addr d, rx_addr player) {
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(1, (void)rx_network_driver_construct(d, player));
    SU32(d, 0xf10, 0);
    SU32(d, 0xf14, 0);
    SU32(d, 0xf34, 0);
    SU32(d, 0xf40, 0);
    SU32(d, 0xf3c, 0);
    SU32(d, 0xf38, 0);
    SU32(d, 0, 0x0093fafc);
    SU32(d, 0xee8, 1);
    for (uint32_t o = 0xf18; o <= 0xf2c; o += 4) SF32(d, o, 0.0f);
    SU32(d, 0xf44, 0xffffffffu);
    SF32(d, 0xf48, 0.0f);
    SF32(d, 0xf4c, 0.0f);
    RACE_FRAME_END(F);
    return d;
}
RX_FUNCTION(0x0050c900, rx_freestyle_driver_construct)

/* 0050c990 FreestyleDriver::sdtor (ECX = driver; flags; RET 4) -> driver: vtable 0x93fafc, the NetworkDriver's destructor (0050e2f0),
 * freed with flag 1. Frame: push esi. */
rx_addr rx_freestyle_driver_sdtor(rx_addr d, uint32_t flags) {
    RACE_FRAME_PLAIN(F, 0, 1);
    SU32(d, 0, 0x0093fafc);
    RACE_CORE_CALL(0, rx_network_driver_dtor(d));
    if (flags & 1) RACE_CORE_CALL(1, rx_reflex_aligned_free(d));
    RACE_FRAME_END(F);
    return d;
}
RX_FUNCTION(0x0050c990, rx_freestyle_driver_sdtor)

/* 0050c9c0 FreestyleDriver::vf1 (ECX = driver): the start: a ghost driver (player +0x10 == 2) listens for "Ghost Session Completed"
 * (0053a5b0); under the engine's profiler scope the network object's score report; NetworkDriver::vf1. Frame: push ebp; and esp,
 * -8; push ecx / ebx / esi / edi. */
void rx_freestyle_driver_vf1(rx_addr d) {
    RACE_FRAME_EBP(F, 8, 0, 4);
    const rx_addr p = U32(d, 0x74);
    if (U32(p, 0x10) == 2) RACE_CORE_CALL(1, rx_waypoint_driver_ghost_subscribe(U32(U32(p, 0x94), 0x290), d));
    race_static_dcf304();
    const rx_addr scope = race_profile_begin();
    net_publish_scores(d);
    RACE_CORE_CALL(0, rx_network_driver_vf1(d));
    race_profile_end(scope);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0050c9c0, rx_freestyle_driver_vf1)

/* 0050cae0 FreestyleDriver::vf2 (ECX = driver): the end: a ghost driver's "Ghost Session Completed" handler leaves (0053a670); under
 * the profiler scope the network object's player state cleared (007006e0) and published (online only); BaseDriver::vf2. Frame:
 * push ebp; and esp, -8; push ecx / ebx / esi / edi. */
void rx_freestyle_driver_vf2(rx_addr d) {
    RACE_FRAME_EBP(F, 8, 0, 4);
    const rx_addr p = U32(d, 0x74);
    if (U32(p, 0x10) == 2) RACE_CORE_CALL(1, rx_waypoint_driver_ghost_unsubscribe(U32(U32(p, 0x94), 0x290), d));
    race_static_dcf304();
    const rx_addr scope = race_profile_begin();
    const rx_addr net = U32(d, 0xef0);
    if (net != 0 && (U8(net, 0x20) & 1) != 0 && net_valid(net))
        act_unreached(0x007006e0, "a Freestyle driver's network player state (online session only)");
    RACE_CORE_CALL(0, rx_base_driver_vf2(d));
    race_profile_end(scope);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0050cae0, rx_freestyle_driver_vf2)

/* 0050cbd0 FreestyleDriver::vf3 (ECX = driver): the reset: NetworkDriver::vf3; then the vehicle back to the start: mode 0 (practice:
 * unless the vehicle kind is 8) on the ground below the start pose (+0x144: the terrain of the activity's scene 0042d960 / 007c5950,
 * placed upright with 0045f600); other modes the simulation's pose override ended (00434ef0) and the start pose's effects (00460020);
 * the throttle +0x3c0 = 0; the tallies cleared; a remote / AI player's vehicle hidden (model +0x14e = 0, vf49 (0), the chassis link
 * unset, the rider's hold released); a ghost (+0x10 == 2) gets the activity's human driver as its target (+0x24, 0055a000 (0, 0)),
 * its rider hidden, +0xf44 = -1, +0x20 = 0; the others the run's duration (+0xf48 = the activity's +0x2bc, +0xf4c = 0); the network
 * object's score report (online); the behaviours' and rules' resets (005092b0 / 005092f0). Frame: push ebp; and esp, -16; sub 0x24;
 * push ebx / esi / edi (the point at +0x10, the up axis at +0x20). */
void rx_freestyle_driver_vf3(rx_addr d) {
    RACE_ORIG_FRAME_ENTER(F, 0x24, 3);
    RACE_CORE_CALL(0, rx_network_driver_vf3(d));
    const rx_addr p = U32(d, 0x74);
    const rx_addr act = U32(p, 0x94);
    int clear_throttle = 1;
    if (U32(act, 0x238) != 0) {
        RACE_CORE_CALL(0, bike_end_pose_override(U32(U32(p, 0x1ac), 0x234)));
        const rx_addr pose = U32(d, 0x144);
        RACE_CORE_CALL(2, rx_vehicle_effect_query(U32(U32(d, 0x74), 0x1ac), pose, pose + 0x10));
    } else if (U32(p, 0x9c) == 8) {
        clear_throttle = 0;
    } else {
        const rx_addr pose = U32(d, 0x144);
        SU32(F, 0x10, U32(pose, 0));
        SU32(F, 0x14, U32(pose, 4));
        SU32(F, 0x18, U32(pose, 8));
        const rx_addr scene = U32(U32(act, 0x234), 0x20138);
        if (!(U8(0, 0x009e30e0) & 1)) {
            SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1);
            SU32(0, 0x009e30dc, 0);
        }
        rx_set_sp(F - 4);                                         /* push the point (007c5950's argument) */
        SU32(F, -4, F + 0x10);
        rx_addr terrain;
        RACE_CORE_CALL(0, terrain = rx_terrain_for_scene(0x009e30d4, scene));
        uint32_t edx = 0;
        RACE_CORE_CALL(0, (void)phys_terrain_base_height_at(terrain, F + 0x10, &edx));
        rx_set_sp(F);
        SF32(F, 0x20, 0.0f);
        SF32(F, 0x24, 1.0f);
        SF32(F, 0x28, 0.0f);
        RACE_CORE_CALL(3, rx_vehicle_place(U32(U32(d, 0x74), 0x1ac), F + 0x10, U32(d, 0x144) + 0x10, F + 0x20));
    }
    if (clear_throttle) SF32(d, 0x3c0, 0.0f);
    SU32(d, 0xf10, 0);
    SU32(d, 0xf14, 0);
    for (uint32_t o = 0xf18; o <= 0xf2c; o += 4) SF32(d, o, 0.0f);
    SU32(d, 0xf38, 0);
    SU32(d, 0xf40, 0);
    SU32(d, 0xf3c, 0);
    const rx_addr pl = U32(d, 0x74);
    if (U32(pl, 4) == 0 || U32(pl, 0x10) != 0) {
        SU8(U32(U32(pl, 0x1ac), 0x1328), 0x14e, 0);
        const rx_addr v = U32(U32(d, 0x74), 0x1ac);
        race_thiscall(race_vfn(v, 49), v, 1, 0, 0, 0, 0);
        const rx_addr handle = U32(U32(U32(d, 0x74), 0x1ac) + 0x178, 4);
        if (handle != 0) {
            rx_addr rec;
            RACE_CORE_CALL(0, rec = phys_body_record(handle));
            SU32(U32(rec, 0x2c), 0x14, 0xffffffffu);
        }
        const rx_addr ra = U32(U32(d, 0x74), 0x1b4);
        if (ra != 0) {
            SU32(ra, 0x5bb5c, 0);
            SU8(ra, 0xa2, 0);
            SU8(ra, 0x5bb64, 0);
            SU32(ra, 0x5b6d0, 0);
        }
    }
    const rx_addr pp = U32(d, 0x74);
    if (U32(pp, 0x10) == 2) {
        rx_addr human;
        RACE_CORE_CALL(2, human = rx_activity_driver_of_kind(U32(pp, 0x94), 0, 0));
        const rx_addr ra = U32(pp, 0x1b4);
        if (ra != 0) SU8(ra, 0xa2, 0);
        SU32(d, 0xf44, 0xffffffffu);
        SU32(d, 0x20, 0);
        SU32(d, 0x24, human);
    } else {
        SF32(d, 0xf4c, 0.0f);
        SU32(d, 0xf48, x87_copy(U32(U32(pp, 0x94), 0x2bc)));
    }
    race_static_dcf304();
    const rx_addr scope = race_profile_begin();
    net_publish_scores(d);
    RACE_CORE_CALL(1, rx_driver_behaviors_vf6(d));
    RACE_CORE_CALL(1, rx_driver_rules_vf6(d));
    race_profile_end(scope);
    RACE_ORIG_FRAME_LEAVE(F);
}
RX_FUNCTION(0x0050cbd0, rx_freestyle_driver_vf3)

/* 0050d1c0 FreestyleDriver::vf6 (ECX = driver): the countdown: in a non-event mode (activity mode != 1) the start state +0x138 = 2;
 * an AI driver of mode 1 draws its reaction time: +0x13c = rand * (1 / 32767) * 2 + the clock +0x130 (the game's LCG 0x9cb860,
 * count 0x9cb864), +0x138 = 0 */
void rx_freestyle_driver_vf6(rx_addr d) {
    const rx_addr p = U32(d, 0x74);
    if (U32(U32(p, 0x94), 0x238) != 1) {
        SU32(d, 0x138, 2);
        return;
    }
    if (U32(p, 0x10) != 1) return;
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    const uint32_t seed = U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u;
    SU32(0, 0x009cb860, seed);
    const int32_t r = (int32_t)((seed >> 16) & 0x7fff);
    SF32(d, 0x13c, (float)r * F32(0, 0x0092e054) * 2.0f + F32(d, 0x130));
    SU32(d, 0x138, 0);
}
RX_FUNCTION(0x0050d1c0, rx_freestyle_driver_vf6)

/* 0050d610 (EDI = driver; stdcall dt; RET 4): the human's Freestyle frame, under the engine's profiler scope and only while the
 * network object (if any) is ours: every rule's vf5 (0.0); in InProgress the tallies from the player's stunt scorer (+0x10a0 the last
 * trick's player, +0xc84 / +0xc80 - +0xca4 / +0xc74 - +0xca8 / +0xc78 / +0xc7c - +0xca0 / +0xcec / +0x1088) and the landings: the
 * vehicle's vf43 (airborne) sets +0xf40, its end counts +0xf34; a ghost run (mode 3): when the clock +0xf4c reaches the duration +0xf48
 * "Ghost Session Completed" (0053a6d0: the activity, the driver) and the clock restarts, in InProgress the clock += dt. Frame: push
 * ecx / ebx / ebp / esi (the scope in the ecx slot). */
void rx_freestyle_driver_human(rx_addr d, float dt) {
    RACE_FRAME_PLAIN(F, 0, 4);
    race_static_dcf304();
    const rx_addr scope = race_profile_begin();
    SU32(F, 0xc, scope);
    const rx_addr net = U32(d, 0xef0);
    if (net != 0 && !net_valid(net)) {
        race_profile_end(scope);
        RACE_FRAME_END(F);
        return;
    }
    for (uint32_t i = 0; i < U32(d, 0xee4); i++) {
        const rx_addr rule = U32(d, 0xa0 + 4 * i);
        typedef void (*fn_t)(rx_addr, float);
        const rx_addr fn = race_vfn(rule, 5);
        fn_t f = RX_FN(fn, fn_t);
        if (f) RACE_CORE_CALL(1, f(rule, 0.0f));
        else race_thiscall(fn, rule, 1, 0, 0, 0, 0);
    }
    const rx_addr p = U32(d, 0x74);
    if (U32(U32(p, 0x94), 0x21c) == 2) {
        const rx_addr last = U32(p, 0x10a0);
        SU32(d, 0xf14, last != 0 ? U32(last, 0x74) : 0);
        SU32(d, 0xf18, x87_copy(U32(p, 0xc84)));
        SF32(d, 0xf1c, F32(p, 0xc80) - F32(p, 0xca4));
        SF32(d, 0xf20, F32(p, 0xc74) - F32(p, 0xca8));
        SU32(d, 0xf24, x87_copy(U32(p, 0xc78)));
        SF32(d, 0xf28, F32(p, 0xc7c) - F32(p, 0xca0));
        SU32(d, 0xf2c, x87_copy(U32(p, 0xcec)));
        SU32(d, 0xf38, U32(p, 0x1088));
        const rx_addr v = U32(p, 0x1ac);
        typedef uint32_t (*air_t)(rx_addr);
        const rx_addr afn = race_vfn(v, 43);
        air_t af = RX_FN(afn, air_t);
        uint32_t air;
        if (af) RACE_CORE_CALL(0, air = af(v));
        else air = race_thiscall(afn, v, 0, 0, 0, 0, 0);
        if (air != 0) {
            SU32(d, 0xf40, 1);
        } else if (U32(d, 0xf40) != 0) {
            SU32(d, 0xf34, U32(d, 0xf34) + 1);
            SU32(d, 0xf40, 0);
        }
    }
    const rx_addr act = U32(U32(d, 0x74), 0x94);
    if (U32(act, 0x238) == 3) {
        if (F32(d, 0xf4c) >= F32(d, 0xf48)) {
            RACE_CORE_CALL(1, rx_event_activity_driver(U32(act, 0x290), STR_GHOST_SESSION_COMPLETED, act, d));
            SF32(d, 0xf4c, 0.0f);
        }
        if (U32(U32(U32(d, 0x74), 0x94), 0x21c) == 2) SF32(d, 0xf4c, F32(d, 0xf4c) + dt);
    }
    race_profile_end(scope);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0050d610, rx_freestyle_driver_human)

/* 0050cf00 FreestyleDriver::vf7 (ECX = driver; dt; RET 4): the frame: a remote player's (+0x908) only NetworkDriver::vf7; in
 * InProgress / WindDown (2, 3) the human's frame (0050d610), an AI's (with the AI debug override 0xae1b90 only: the reaction start
 * +0x138 0 -> 2 once the clock +0x130 passes +0x13c, then 0050d240); NetworkDriver::vf7; when the stats system is idle (session
 * +0x1c3c) under the profiler scope a remote network object's tallies are taken over (online only). Frame: push ebp; and esp, -16;
 * sub 0xc4; push ebx / esi / edi (the network snapshot at +0x10). */
void rx_freestyle_driver_vf7(rx_addr d, float dt) {
    RACE_ORIG_FRAME_ENTER(F, 0xc4, 3);
    const rx_addr p = U32(d, 0x74);
    if (U32(p, 0x908) != 0) {
        RACE_CORE_CALL(1, rx_network_driver_update(d, dt));
        RACE_ORIG_FRAME_LEAVE(F);
        return;
    }
    if (U32(U32(p, 0x94), 0x21c) - 2u <= 1u) {
        const uint32_t kind = U32(p, 0x10);
        if (kind == 0) {
            RACE_CORE_CALL(1, rx_freestyle_driver_human(d, dt));
        } else if (kind == 1 && U32(p, 4) != 0 && U32(0, 0x00ae1b90) != 0) {
            const uint32_t start = U32(d, 0x138);
            if (start == 0) {
                if (F32(d, 0x130) > F32(d, 0x13c)) SU32(d, 0x138, 2);
            } else if (start == 2) {
                /* DEBUG BOUNDARY: the AI's freestyle riding (0050d240, 962 bytes) runs only with the AI debug override 0xae1b90 (the
                 * registry's AI benchmark switch, 0 in the game and the product): a Freestyle event has no AI riders otherwise */
                act_unreached(0x0050d240, "a Freestyle AI rider (the AI debug override 0xae1b90)");
            }
        }
    }
    RACE_CORE_CALL(1, rx_network_driver_update(d, dt));
    race_static_dcf304();
    if (U32(U32(0, 0x00da6cb0), 0x1c3c) == 0) {
        race_static_dcf304();
        const rx_addr scope = race_profile_begin();
        SU32(F, 0xc, scope);
        const rx_addr net = U32(d, 0xef0);
        if (net != 0 && (U8(net, 0x20) & 1) != 0 && !net_valid(net))
            act_unreached(0x00700310, "a remote Freestyle driver's network tallies (online session only)");
        race_profile_end(scope);
    }
    RACE_ORIG_FRAME_LEAVE(F);
}
RX_FUNCTION(0x0050cf00, rx_freestyle_driver_vf7)

/* 0050db40 FreestyleDriver::vf9 (ECX = driver; rule; RET 4): a rule's event: kind 10 (the rule's vf3; RuleInFreestyleCrowd's) with the
 * vehicle not wrecking (+0x280) and the player's reset clock +0x116c not running (<= 0): the vehicle back at the driver's start pose
 * (0045f600 with the default up axis 0x9cb4b0) and the player's fade (0054b780). Frame: push esi. */
void rx_freestyle_driver_vf9(rx_addr d, rx_addr rule) {
    RACE_FRAME_PLAIN(F, 0, 1);
    if (race_thiscall(race_vfn(rule, 3), rule, 0, 0, 0, 0, 0) == 10) {
        const rx_addr p = U32(d, 0x74);
        const rx_addr v = U32(p, 0x1ac);
        if (U32(v, 0x280) == 0 && !(F32(p, 0x116c) > 0.0f)) {
            const rx_addr pose = U32(d, 0x144);
            RACE_CORE_CALL(3, rx_vehicle_place(v, pose, pose + 0x10, 0x009cb4b0));
            RACE_CORE_CALL(0, rx_player_set_fade_out(U32(d, 0x74)));
        }
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0050db40, rx_freestyle_driver_vf9)

/* 0050dba0 FreestyleDriver::vf12 (ECX = driver) -> EAX: a ghost's new best: the target's (+0x24) score to beat +0xf30 above the best
 * +0xf44 becomes it (1), else 0 */
uint32_t rx_freestyle_driver_vf12(rx_addr d) {
    const int32_t v = I32(U32(d, 0x24), 0xf30);
    if (v <= I32(d, 0xf44)) return 0;
    SU32(d, 0xf44, (uint32_t)v);
    return 1;
}
RX_FUNCTION(0x0050dba0, rx_freestyle_driver_vf12)

/* 0050dbc0 FreestyleDriver::vf13 (ECX = driver) -> st0: the score shown: a ghost's best +0xf44, else the score +0xf10 (as float) */
float rx_freestyle_driver_vf13(rx_addr d) {
    if (U32(U32(d, 0x74), 0x10) == 2) return (float)I32(d, 0xf44);
    return (float)I32(d, 0xf10);
}
RX_FUNCTION(0x0050dbc0, rx_freestyle_driver_vf13)

/* 0050dc70 FreestyleDriver::vf14 (ECX = driver): the network object's tallies published (online only); a jump to NetworkDriver::vf14 */
void rx_freestyle_driver_vf14(rx_addr d) {
    const rx_addr net = U32(d, 0xef0);
    if (net != 0) {
        RACE_FRAME_PLAIN(F, 0, 1);
        const int valid = net_valid(net);
        RACE_FRAME_END(F);
        if (valid) act_unreached(0x006fe210, "a Freestyle driver's network tallies (online session only)");
    }
    rx_waypoint_network_vf14(d);
}
RX_FUNCTION(0x0050dc70, rx_freestyle_driver_vf14)

/* 0050dbf0 FreestyleDriver::vf16 (ECX = driver; kind, object; RET 8): a network object arrived: NetworkDriver::vf16 (0050eb50) and,
 * for the driver's own object (kind 1), its score report. ONLINE BOUNDARY: the network layer calls it for duplicated objects only
 * (an online session); 0050eb50 compares the object with +0xef0 (0 offline). The oracle runs the original NetworkDriver::vf16. */
void rx_freestyle_driver_vf16(rx_addr d, uint32_t kind, rx_addr object) {
    RACE_FRAME_PLAIN(F, 0, 3);
    const uint32_t a[2] = {kind, object};
    (void)act_call_fn(0x0050eb50, RX_THISCALL, d, 2, a);
    if (kind == 1 && U32(d, 0xef0) == object && net_valid(object))
        act_unreached(0x0050bc00, "a Freestyle driver's network score report (online session only)");
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0050dbf0, rx_freestyle_driver_vf16)

/* 0050dda0 FreestyleDriver::vf17 (ECX = driver): a network player that left (+0xb2c: flag 8 of +0x128, or not connected +0x138) has its
 * tallies cleared; a jump to NetworkDriver::vf14 */
void rx_freestyle_driver_vf17(rx_addr d) {
    const rx_addr np = U32(U32(d, 0x74), 0xb2c);
    if (np != 0 && ((U8(np, 0x128) & 8) != 0 || U8(np, 0x138) == 0)) {
        SU32(d, 0xf38, 0);
        SU32(d, 0xf14, 0);
        for (uint32_t o = 0xf18; o <= 0xf2c; o += 4) SF32(d, o, 0.0f);
    }
    rx_waypoint_network_vf14(d);
}
RX_FUNCTION(0x0050dda0, rx_freestyle_driver_vf17)

/* 0050dd20 FreestyleDriver::vf18 (ECX = driver): the tallies taken from the player (+0x10a0 the last trick's player, +0xc84 / +0xc94 /
 * +0xc88 / +0xc8c / +0xc90 / +0xcec (x87 copies), +0x1088) */
void rx_freestyle_driver_vf18(rx_addr d) {
    const rx_addr p = U32(d, 0x74);
    if (p == 0) return;
    const rx_addr last = U32(p, 0x10a0);
    SU32(d, 0xf14, last != 0 ? U32(last, 0x74) : 0);
    SU32(d, 0xf18, x87_copy(U32(p, 0xc84)));
    SU32(d, 0xf1c, x87_copy(U32(p, 0xc94)));
    SU32(d, 0xf20, x87_copy(U32(p, 0xc88)));
    SU32(d, 0xf24, x87_copy(U32(p, 0xc8c)));
    SU32(d, 0xf28, x87_copy(U32(p, 0xc90)));
    SU32(d, 0xf2c, x87_copy(U32(p, 0xcec)));
    SU32(d, 0xf38, U32(p, 0x1088));
}
RX_FUNCTION(0x0050dd20, rx_freestyle_driver_vf18)
