/* anim_scenario_finish.c -- the post-race presentation scenarios (agent "w-podium", 2026-10-26).
 *
 * When a race / Freestyle event reaches Post, the activity's Post update (race_finish.c rx_race_update_post 00569a90,
 * race_activity_freestyle.c rx_freestyle_update_post 00563b10) sets the level cast up for the podium (0074ada0: the podium
 * RiderCharacters, the crew chiefs, the scenario actors) and starts the finish scenario (00747810). The scenario then plays its
 * steps, one script each ("<level>_<step name>", rx_scenario_load_intro_script 00747750):
 *    7  the finish replay: the clip recorder plays the finish clip (type 9) through the replay host;
 *    8  the winner on the mound (0074f7e0);
 *    9 / 11  the winner's walk / the lose walk of the crew chiefs (0074f410);
 *   10 / 12  the podium actors at their placements (0074f620).
 * Which steps follow which is the scenario's vf3 (race_finish_select.c for the race; FMX below). The results scenario (the activity's
 * +0x129430 / +0x129420) is started after the results page.
 *
 * The scenario object (BaseScenario): +0x14 the activity, +0x151b0 started, +0x151b4 the step, +0x151b8 the step changed,
 * +0x151c0 a local human is on the podium, +0x151c4 a local human won. The level cast is the world's +0x20150 (its +0x40 the
 * activity, +0x50 the podium RiderCharacter records of 0x30 bytes: +0 the character, +4 the player). For each driver of the activity
 * (+0x268 [+0x26c]) the player (+0x74) has its rider (+0x1b4: model instance flag +0xa2 = drawn) and its vehicle (+0x1ac: the model
 * instance +0x1328, +0x14e = drawn; the physics body +0x17c, whose record +0x2c has the collision mask +0x14: 0 = collides,
 * 0xffffffff = none while hidden).
 *
 * Globals: 0x00d61048 the debug drivers table (set only by the debug menu: the scenario steps can then be stepped by hand),
 * 0x00d610a0 the debug hold step, 0x00a942b4 the clip recorder's state (2 = playing), 0x009c407c the replay's "finished" flag,
 * 0x00d323c8 the clip slots (0x35d0 bytes each, +0 the clip type).
 *
 * Replay playback (006aa8d0 / 006aa990): the product runs the core's replay playback through the replay host (replay_clip.c,
 * waypoint_playback.c; the host is bound for every event, QUEUE "w-replay"); the oracle runs the originals there, as
 * race_finish.c's Post state does.
 *
 * Adapters: core/oracle/anim_scenario_finish.cpp (group rx_anim). */
#include "anim_scenario_finish.h"
#include "anim.h"
#include "anim_scenario.h"
#include "anim_scenario_race_intro.h"
#include "build_activity_int.h"
#include "loop_world.h"
#include "phys_body.h"
#include "race_finish_select.h"
#include "replay_clip.h"
#include "rx_vcall.h"
#include "waypoint_playback.h"
#include "waypoint_post_steps.h"
#include "waypoint_scenario.h"
#include "rider_cap.h"

#define DEBUG_DRIVERS        0x00d61048u   /* the debug drivers table (debug menu only) */
#define DEBUG_HOLD_STEP      0x00d610a0u   /* the debug hold step */
#define CLIP_RECORDER_STATE  0x00a942b4u   /* 2 = a clip is playing */
#define REPLAY_FINISHED      0x009c407cu
#define CLIP_SLOTS_BEGIN     0x00d323c8u   /* the clip slots: +0 the clip type */
#define CLIP_SLOTS_END       0x00d5a988u
#define CLIP_SLOT_SIZE       0x35d0u
#define CLIP_TYPE_FINISH     9u

/* ---- small helpers (the original inlines these loops) ------------------------------------------------------------------------ */

static rx_addr scenario_cast(rx_addr sc) {
    return U32(U32(sc, 0x14), 0x234) + 0x20150;
}

/* the player of driver i of the cast's activity (the driver list is read again for every driver, as the original does) */
static rx_addr cast_player(rx_addr cast, int32_t i) {
    const rx_addr drivers = U32(U32(cast, 0x40), 0x268);
    return U32(U32(drivers, 4u * (uint32_t)i), 0x74);
}

/* every driver's rider model shown / hidden (+0xa2) */
static void cast_riders_visible(rx_addr cast, uint8_t visible) {
    const int32_t count = I32(U32(cast, 0x40), 0x26c);
    for (int32_t i = 0; i < count; i++) {
        const rx_addr player = cast_player(cast, i);
        SU8(U32(player, 0x1b4), 0xa2, visible);
    }
}

/* one vehicle's model shown / hidden (+0x14e) and its physics body's collision mask (0 shown, 0xffffffff hidden) */
static void vehicle_visible(rx_addr player, uint8_t visible) {
    SU8(U32(U32(player, 0x1ac), 0x1328), 0x14e, visible);
    const rx_addr body = U32(U32(player, 0x1ac), 0x17c);
    if (body != 0) {
        rx_addr record;
        ACT_CALLR(record, 0, phys_body_record(body));            /* 0042bd90 (EAX = the body) */
        SU32(U32(record, 0x2c), 0x14, visible ? 0u : 0xffffffffu);
    }
}

/* every driver's vehicle shown / hidden */
static void cast_vehicles_visible(rx_addr cast, uint8_t visible) {
    const int32_t count = I32(U32(cast, 0x40), 0x26c);
    for (int32_t i = 0; i < count; i++) vehicle_visible(cast_player(cast, i), visible);
}

/* 006aa8d0 ClipRecorder_StartPlayback (cdecl type) */
static void clip_start_playback(uint32_t type) {
#ifdef RX_ORACLE
    const uint32_t a[1] = {type};
    rx_call_original(0x006aa8d0, RX_CDECL, 0, 1, a);
#else
    ACT_CALL(1, rx_replay_start((int32_t)type));
#endif
}

/* 006aa990 ClipRecorder_StopPlayback (cdecl) */
static void clip_stop_playback(void) {
#ifdef RX_ORACLE
    rx_call_original(0x006aa990, RX_CDECL, 0, 0, 0);
#else
    ACT_CALL(0, rx_clip_recorder_stop_playback());
#endif
}

/* the replay stopped for a podium step: the riders hidden, the world's replay camera index (+0x21de0) 0 and the replay cameras
 * updated once (007678b0 with the replay's frame time 0x00977818), the replay marked finished, the playback stopped */
static void replay_stop_for_podium(rx_addr sc) {
    cast_riders_visible(scenario_cast(sc), 0);
    const rx_addr world = U32(U32(sc, 0x14), 0x234);
    SU32(world, 0x21de0, 0);
    ACT_CALL(2, rx_replay_cameras_update(world, F32(0, 0x00977818)));
    SU32(0, REPLAY_FINISHED, 1);
    clip_stop_playback();
}

/* the first clip slot holding a finish clip (type 9), or -1 */
static int32_t finish_clip_slot(void) {
    int32_t slot = 0;
    for (rx_addr p = CLIP_SLOTS_BEGIN; p < CLIP_SLOTS_END; p += CLIP_SLOT_SIZE) {
        if (U32(p, 0) == CLIP_TYPE_FINISH) return slot;
        slot++;
    }
    return -1;
}

/* the scenario's vf3 as a tail call (jmp [vtable + 0xc]: the callee returns to our caller, the stack as at our entry) */
static void tail_call_vf3(rx_addr sc) {
    const rx_addr target = U32(U32(sc, 0), 0xc);
    void* owner = rx_fn(target);
    if (owner) (void)((uint32_t (*)(rx_addr))owner)(sc);
    else (void)rx_call_original(target, RX_THISCALL, sc, 0, NULL);
}

/* ---- the podium visibility helpers ------------------------------------------------------------------------------------------- */

/* 0074c590 (EDI = cast, ESI = kind; stack visible; RET 4): the rider model of every driver whose player kind (+0x10: 0 a local
 * human) is `kind` (3 = every driver) shown / hidden. Frame: push ebx. */
void rx_podium_riders_set_visible(rx_addr cast, uint8_t visible, uint32_t kind) {
    ACT_FRAME(f, 0, 1);
    const int32_t count = I32(U32(cast, 0x40), 0x26c);
    for (int32_t i = 0; i < count; i++) {
        const rx_addr player = cast_player(cast, i);
        if (U32(player, 0x10) == kind || kind == 3) SU8(U32(player, 0x1b4), 0xa2, visible);
    }
    ACT_END(f);
}
RX_FUNCTION(0x0074c590, rx_podium_riders_set_visible)

/* 0074c620 (stdcall cast, visible, kind; RET 0xc): the vehicle of every driver whose player kind is `kind` (3 = every driver)
 * shown / hidden, with its body's collision mask. Frame: push ebx / ebp / esi / edi. */
void rx_podium_vehicles_set_visible(rx_addr cast, uint32_t visible, uint32_t kind) {
    ACT_FRAME(f, 0, 4);
    const int32_t count = I32(U32(cast, 0x40), 0x26c);
    for (int32_t i = 0; i < count; i++) {
        const rx_addr player = cast_player(cast, i);
        if (U32(player, 0x10) == kind || kind == 3) vehicle_visible(player, visible != 0 ? 1 : 0);
    }
    ACT_END(f);
}
RX_FUNCTION(0x0074c620, rx_podium_vehicles_set_visible)

/* ---- RaceFinishScenario (vtable 0x009433d4) ---------------------------------------------------------------------------------- */

/* 0074a1e0 RaceFinishScenario::vf0 (ECX = scenario): the start. The podium riders take their players' mud (0074bcc0); when the level
 * has a finish script (0074e230): the local podium flags (0074f1a0), every rider and vehicle hidden, the simulations flagged (00749550)
 * and the riders paused (00749590 with 1); the first step is 0 (none) for an activity of kind 8 without a local winner, else 7 (the
 * finish replay). Without a finish clip in the recorder the scenario's vf3 is tail-called at once (the next step).
 * Frame: push edi; push esi after the script test; push ebx / ebp around the vehicle loop. */
void rx_race_finish_vf0(rx_addr sc) {
    ACT_FRAME(f, 0, 1);
    ACT_CALL(0, rx_waypoint_podium_scalars(scenario_cast(sc)));          /* 0074bcc0 PodiumRiders_CopyMud (EDX = cast) */
    uint32_t has_script;
    ACT_CALLR(has_script, 1, rx_waypoint_finish_script_exists(U32(sc, 0x14)));   /* 0074e230 (stdcall activity) */
    if (has_script == 0) {
        ACT_END(f);
        return;
    }
    {
        ACT_FRAME(g, 0, 1);                                                /* push esi */
        ACT_CALL(0, rx_finish_select_local_flags(sc));                     /* 0074f1a0 (EAX = scenario) */
        cast_riders_visible(scenario_cast(sc), 0);
        {
            ACT_FRAME(h, 0, 2);                                            /* push ebx; push ebp */
            cast_vehicles_visible(scenario_cast(sc), 0);
            ACT_CALL(0, rx_scenario_sim_flag(sc));                         /* 00749550 (EDX = scenario) */
            ACT_CALL(0, rx_scenario_set_riders_paused(sc, 1));             /* 00749590 (EDI = scenario, EDX = 1) */
            ACT_END(h);
        }
        if (U32(U32(sc, 0x14), 0x238) == 8 && U32(sc, 0x151c4) == 0) {
            SU32(sc, 0x151b4, 0);
        } else {
            SU32(sc, 0x151b8, 1);
            SU32(sc, 0x151b4, 7);
        }
        ACT_END(g);
    }
    const int32_t slot = finish_clip_slot();
    ACT_END(f);
    if (slot != -1) return;
    tail_call_vf3(sc);
}
RX_FUNCTION(0x0074a1e0, rx_race_finish_vf0)

/* 0074a340 RaceFinishScenario::vf1 (ECX = scenario; stack step; RET 4): begin a step. Every vehicle hidden; the step's camera
 * (0074f510); with the debug table the debug standings (0074f260). Step 7 (the finish replay): every rider and vehicle shown and,
 * without the debug table, the finish clip played (006aa8d0 with 9). Any other step: a playing replay is stopped (the riders hidden,
 * the replay cameras reset, 006aa990), the step's entities placed (0074f590) and the step's camera again (0074f510).
 * Frame: push ebx / ebp / esi / edi. */
void rx_race_finish_vf1(rx_addr sc, uint32_t step) {
    ACT_FRAME(f, 0, 4);
    cast_vehicles_visible(scenario_cast(sc), 0);
    ACT_CALL(1, rx_post_step_cameras(sc, step));                           /* 0074f510 (EDI = scenario; stack step) */
    if (U32(0, DEBUG_DRIVERS) != 0) ACT_CALL(0, rx_post_debug_standings(sc, (int32_t)step));   /* 0074f260 (EAX = step) */
    if (step == 7) {
        cast_riders_visible(scenario_cast(sc), 1);
        cast_vehicles_visible(scenario_cast(sc), 1);
        if (U32(0, DEBUG_DRIVERS) == 0) clip_start_playback(CLIP_TYPE_FINISH);
    } else {
        if (U32(0, CLIP_RECORDER_STATE) == 2) replay_stop_for_podium(sc);
        ACT_CALL(0, rx_post_step_entities(sc, step));                      /* 0074f590 (EAX = step, ESI = scenario) */
        ACT_CALL(1, rx_post_step_cameras(sc, step));
    }
    ACT_END(f);
}
RX_FUNCTION(0x0074a340, rx_race_finish_vf1)

/* 0074a540 RaceFinishScenario::vf6 (ECX = scenario): the end. Every rider and vehicle shown again, the riders unpaused (00749590
 * with 0), the cast's actors stopped (0074b450) and its cameras reset (0074b480, a tail call). Frame: push ebx / ebp / esi / edi. */
void rx_race_finish_vf6(rx_addr sc) {
    ACT_FRAME(f, 0, 4);
    cast_riders_visible(scenario_cast(sc), 1);
    cast_vehicles_visible(scenario_cast(sc), 1);
    ACT_CALL(0, rx_scenario_set_riders_paused(sc, 0));                     /* 00749590 (EDI = scenario, EDX = 0) */
    ACT_CALL(0, rx_scenario_anims_stop_actors(scenario_cast(sc)));         /* 0074b450 (EDX = cast) */
    const rx_addr cast = scenario_cast(sc);
    ACT_END(f);
    rx_scenario_anims_reset_cameras(cast);                                 /* 0074b480 (EAX = cast): jmp */
}
RX_FUNCTION(0x0074a540, rx_race_finish_vf6)

/* ---- FMXFinishScenario (vtable 0x0094414c) ----------------------------------------------------------------------------------- */

/* 0074dd40 (EAX = step, EDI = scenario): with the debug table only, the activity's podium order (+0x2d0 .. +0x2dc) rearranged so the
 * step can be previewed: steps 7 .. 10 put the first local human (player kind +0x10 == 0) in first place when the winner is not one
 * (the original writes that driver into BOTH slots); step 11 (the lose walk) puts the first local human of places 1 .. 3 into place 4
 * (+0x2dc) when no local human finished below the podium. Frame: push esi. */
void rx_fmx_finish_debug_standings(uint32_t step, rx_addr sc) {
    if (U32(0, DEBUG_DRIVERS) == 0 || (int32_t)step < 7) return;
    ACT_FRAME(f, 0, 1);
    rx_addr act = U32(sc, 0x14);
    if ((int32_t)step <= 10) {
        if (U32(U32(U32(act, 0x2d0), 0x74), 0x10) != 0) {
            const int32_t count = I32(act, 0x26c);
            for (int32_t i = 1; i < count; i++) {
                if (U32(U32(rx_stand(act, (uint32_t)i), 0x74), 0x10) == 0) {
                    const uint32_t driver = rx_stand(U32(sc, 0x14), (uint32_t)i);
                    SU32(U32(sc, 0x14), 0x2d0, driver);
                    rx_stand_set(U32(sc, 0x14), (uint32_t)i, driver);
                    break;
                }
            }
        }
    } else if (step == 11) {
        /* the last place held by a local human: when it is below the podium (index > 2), nothing to do */
        for (int32_t i = I32(act, 0x26c) - 1; i >= 0; i--) {
            const rx_addr player = U32(rx_stand(act, (uint32_t)i), 0x74);
            if (U32(player, 4) != 0 && U32(player, 0x10) == 0) {
                if (i > 2) {
                    ACT_END(f);
                    return;
                }
                break;
            }
        }
        for (uint32_t i = 0; i < 3; i++) {
            const rx_addr player = U32(rx_stand(act, i), 0x74);
            if (U32(player, 4) != 0 && U32(player, 0x10) == 0) {
                const uint32_t fourth = U32(act, 0x2dc);
                SU32(act, 0x2dc, rx_stand(act, i));
                if (fourth != 0) rx_stand_set(U32(sc, 0x14), i, fourth);
                break;
            }
        }
    }
    ACT_END(f);
}
RX_FUNCTION(0x0074dd40, rx_fmx_finish_debug_standings)

/* 0074df30 FMXFinishScenario::vf0 (ECX = scenario): the start: every rider and vehicle hidden, the podium riders' mud (0074bcc0),
 * the simulations flagged (00749550), the riders paused (00749590 with 1), the local podium flags (0074f1a0); the first step is 8
 * (the winner's mound) when a local human won, else 0 (none). There is no finish replay in a Freestyle event.
 * Frame: push esi / edi; push ebx / ebp after the rider loop. */
void rx_fmx_finish_vf0(rx_addr sc) {
    ACT_FRAME(f, 0, 2);
    cast_riders_visible(scenario_cast(sc), 0);
    {
        ACT_FRAME(g, 0, 2);
        cast_vehicles_visible(scenario_cast(sc), 0);
        ACT_CALL(0, rx_waypoint_podium_scalars(scenario_cast(sc)));      /* 0074bcc0 PodiumRiders_CopyMud (EDX = cast) */
        ACT_CALL(0, rx_scenario_sim_flag(sc));
        ACT_CALL(0, rx_scenario_set_riders_paused(sc, 1));
        ACT_CALL(0, rx_finish_select_local_flags(sc));
        ACT_END(g);
    }
    SU32(sc, 0x151b8, 1);
    SU32(sc, 0x151b4, U32(sc, 0x151c4) != 0 ? 8u : 0u);
    ACT_END(f);
}
RX_FUNCTION(0x0074df30, rx_fmx_finish_vf0)

/* 0074e050 FMXFinishScenario::vf1 (ECX = scenario; stack step; RET 4): begin a step: the local humans' vehicles hidden (0074c620);
 * with the debug table the debug standings (0074dd40). Step 7: the local riders and vehicles shown and, without the debug table, the
 * finish clip played (006aa8d0 with 9). Any other step: a playing replay stopped (the local riders hidden, the replay cameras reset,
 * 006aa990) and the step's entities placed (0074f590). Frame: push ebx / ebp / esi / edi. */
void rx_fmx_finish_vf1(rx_addr sc, uint32_t step) {
    ACT_FRAME(f, 0, 4);
    ACT_CALL(3, rx_podium_vehicles_set_visible(scenario_cast(sc), 0, 0));
    if (U32(0, DEBUG_DRIVERS) != 0) ACT_CALL(0, rx_fmx_finish_debug_standings(step, sc));
    if (step == 7) {
        ACT_CALL(1, rx_podium_riders_set_visible(scenario_cast(sc), 1, 0));
        ACT_CALL(3, rx_podium_vehicles_set_visible(scenario_cast(sc), 1, 0));
        if (U32(0, DEBUG_DRIVERS) == 0) clip_start_playback(CLIP_TYPE_FINISH);
    } else {
        if (U32(0, CLIP_RECORDER_STATE) == 2) {
            ACT_CALL(1, rx_podium_riders_set_visible(scenario_cast(sc), 0, 0));
            const rx_addr world = U32(U32(sc, 0x14), 0x234);
            SU32(world, 0x21de0, 0);
            ACT_CALL(2, rx_replay_cameras_update(world, F32(0, 0x00977818)));
            SU32(0, REPLAY_FINISHED, 1);
            clip_stop_playback();
        }
        ACT_CALL(0, rx_post_step_entities(sc, step));
    }
    ACT_END(f);
}
RX_FUNCTION(0x0074e050, rx_fmx_finish_vf1)

/* 0074de60 FMXFinishScenario::vf3 (ECX = scenario) -> EAX: the next step. The debug hold step stays (1). Without the debug table, or
 * from step 12 on, the scenario ends (step 0, returns 0). With it, unless the scenario's vf8 says it is done, the steps go
 * 7 -> 8 -> 9 -> 11 -> 10 -> 12 (returns 1). Frame: push esi. */
uint32_t rx_fmx_finish_vf3(rx_addr sc) {
    ACT_FRAME(f, 0, 1);
    const uint32_t step = U32(sc, 0x151b4);
    if (step == U32(0, DEBUG_HOLD_STEP)) {
        ACT_END(f);
        return 1;
    }
    if (U32(0, DEBUG_DRIVERS) == 0 || (int32_t)step >= 12) {
        SU32(sc, 0x151b4, 0);
        ACT_END(f);
        return 0;
    }
    if (rx_vcall(sc, 0x20, 0, NULL) == 0) {                                /* vf8: the scenario is done */
        switch (U32(sc, 0x151b4)) {
        case 7:  SU32(sc, 0x151b4, 8);    break;
        case 8:  SU32(sc, 0x151b4, 9);    break;
        case 9:  SU32(sc, 0x151b4, 0xb);  break;
        case 10: SU32(sc, 0x151b4, 0xc);  break;
        case 11: SU32(sc, 0x151b4, 0xa);  break;
        default: break;
        }
    }
    ACT_END(f);
    return 1;
}
RX_FUNCTION(0x0074de60, rx_fmx_finish_vf3)

/* 0074e140 FMXFinishScenario::vf6 (ECX = scenario): the end: the local humans' riders and vehicles shown, the riders unpaused
 * (00749590 with 0), the cast's actors stopped (0074b450) and cameras reset (0074b480), then "Post Scenarios End" ({ activity,
 * "Finish" }) on the activity's channel. Frame: sub esp, 0xc (the payload at +8, the cookie at +0x10 after the pushes); push esi /
 * edi. */
void rx_fmx_finish_vf6(rx_addr sc) {
    ACT_FRAME(f, 0xc, 2);
    SU32(f, 0x10, U32(0, 0x009b45e4) ^ (f + 8));                            /* the security cookie */
    const rx_addr cast = scenario_cast(sc);
    const int32_t count = I32(U32(cast, 0x40), 0x26c);
    for (int32_t i = 0; i < count; i++) {
        const rx_addr player = cast_player(cast, i);
        if (U32(player, 0x10) == 0) SU8(U32(player, 0x1b4), 0xa2, 1);
    }
    ACT_CALL(3, rx_podium_vehicles_set_visible(scenario_cast(sc), 1, 0));
    ACT_CALL(0, rx_scenario_set_riders_paused(sc, 0));
    ACT_CALL(0, rx_scenario_anims_stop_actors(scenario_cast(sc)));
    ACT_CALL(0, rx_scenario_anims_reset_cameras(scenario_cast(sc)));
    const rx_addr act = U32(sc, 0x14);
    SU32(f, 8, act);
    SU32(f, 0xc, 0x00941eac);                                               /* "Finish" */
    act_raise(U32(act, 0x290), 0x00941c1c /* "Post Scenarios End" */, 0x009cf660 /* void(Activity*, char const*) */, f + 8, 8);
    ACT_END(f);
}
RX_FUNCTION(0x0074e140, rx_fmx_finish_vf6)

/* 0074e2a0 FMXFinishScenario::vf13 (ECX = scenario) -> EAX: the previous step (the debug menu's back): unless the scenario's vf8
 * says it is done, from step 8 on: 8 -> 7, 9 -> 8, 10 -> 12, 11 -> 9, 12 -> 11 (returns 1); otherwise 0. Frame: push esi / edi. */
uint32_t rx_fmx_finish_vf13(rx_addr sc) {
    ACT_FRAME(f, 0, 2);
    uint32_t result = 0;
    if (rx_vcall(sc, 0x20, 0, NULL) == 0 && I32(sc, 0x151b4) > 7) {
        switch (U32(sc, 0x151b4)) {
        case 8:  SU32(sc, 0x151b4, 7);    result = 1; break;
        case 9:  SU32(sc, 0x151b4, 8);    result = 1; break;
        case 10: SU32(sc, 0x151b4, 0xc);  result = 1; break;
        case 11: SU32(sc, 0x151b4, 9);    result = 1; break;
        case 12: SU32(sc, 0x151b4, 0xb);  result = 1; break;
        default: break;
        }
    }
    ACT_END(f);
    return result;
}
RX_FUNCTION(0x0074e2a0, rx_fmx_finish_vf13)

/* ---- FMXResultsScenario (vtable 0x0094418c) ---------------------------------------------------------------------------------- */

/* 0074fc70 (EAX = scenario): the CardGirl's event cards (the nodes "EventCard_Moto1" .. "EventCard_MainEvent", names at 0x009dd26c)
 * hidden (+0xc bit 4) in her hierarchy (+0x210), when the level cast has her. Frame: push esi / edi; push ebp in the loop. */
void rx_fmx_results_hide_cards(rx_addr sc) {
    ACT_FRAME(f, 0, 2);
    rx_addr girl;
    ACT_CALLR(girl, 0, rx_scenario_anims_find_entity(scenario_cast(sc), 0x0096c5f8 /* "CardGirl" */));   /* 0074b970 */
    if (girl != 0) {
        ACT_FRAME(g, 0, 1);
        const rx_addr hierarchy = girl + 0x210;
        for (rx_addr p = 0x009dd26c; p < 0x009dd284; p += 4) {
            const rx_addr name = U32(p, 0);
            uint32_t has;
            ACT_CALLR(has, 1, rx_hierarchy_has_node(hierarchy, name));     /* 007de980 (ESI = hierarchy) */
            if (has != 0) {
                rx_addr node;
                ACT_CALLR(node, 1, rx_hierarchy_find_node(hierarchy, name));   /* 007de9e0 (EDI = hierarchy) */
                SU8(node, 0xc, (uint8_t)(U8(node, 0xc) | 0x10));
            }
        }
        ACT_END(g);
    }
    ACT_END(f);
}
RX_FUNCTION(0x0074fc70, rx_fmx_results_hide_cards)

/* 0074fae0 FMXResultsScenario::vf0 (ECX = scenario): the start: the local podium flags (0074f1a0), every rider and vehicle hidden,
 * the simulations flagged (00749550), the riders paused (00749590 with 1); the first step 9 when a local human is on the podium,
 * else 11; the CardGirl's event cards hidden (0074fc70); +0x151b8 = 1. Frame: push ebx / ebp / esi / edi. */
void rx_fmx_results_vf0(rx_addr sc) {
    ACT_FRAME(f, 0, 4);
    ACT_CALL(0, rx_finish_select_local_flags(sc));
    cast_riders_visible(scenario_cast(sc), 0);
    cast_vehicles_visible(scenario_cast(sc), 0);
    ACT_CALL(0, rx_scenario_sim_flag(sc));
    ACT_CALL(0, rx_scenario_set_riders_paused(sc, 1));
    SU32(sc, 0x151b4, U32(sc, 0x151c0) == 0 ? 11u : 9u);
    ACT_CALL(0, rx_fmx_results_hide_cards(sc));
    SU32(sc, 0x151b8, 1);
    ACT_END(f);
}
RX_FUNCTION(0x0074fae0, rx_fmx_results_vf0)
