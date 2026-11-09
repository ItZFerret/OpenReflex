#include "replay_clip.h"
#include "rider_cap.h"
/* Hand-translated runtime continuation for the separately laid-out waypoint class. */
#include "waypoint_activity.h"
#include "build_activity_int.h"
#include "build_activity_pre.h"
#include "race_activity.h"
#include "anim_scenario.h"
#include "anim_scenario_race_intro.h"
#include "anim_scenario_misc.h"
#include <string.h>

static float waypoint_float_arg(float value) {
    uint32_t b = rx_fb(value);
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu)) b |= 0x400000u;
    return rx_bf(b);
}
/* 00578f40: marker ownership for the first two local humans, then intro setup. */
void rx_waypoint_activity_vf37(rx_addr a, uint32_t previous) {
    (void)previous;
    const rx_addr entry = rx_sp();
    ACT_FRAME(f, 0, 3);
    ACT_CALL(1, rx_activity_vf37(a));
    rx_addr driver; ACT_CALLR(driver, 2, rx_activity_driver_of_kind(a, 0, 0));
    SU32(a, 0x13ec60, driver);
    uint32_t count; ACT_CALLR(count, 0, rx_activity_count_humans_of(a, 1));
    if ((int32_t)count > 1) ACT_CALLR(driver, 2, rx_activity_driver_of_kind(a, 0, 1));
    else driver = 0;
    SU32(a, 0x13ec64, driver);
    ACT_CALL(2, rx_waypoint_select_marker(a, 0));
    if (I32(a, 0x26c) > 0) {
        ACT_FRAME(t, 0, 1);
        for (int32_t i = 0; i < I32(a, 0x26c); ++i) {
            SU32(entry, 4, U32(U32(a, 0x268), 4u * (uint32_t)i));
            act_raise(U32(a, 0x290), 0x0093eef0, 0x009cda44, entry + 4, 4);
        }
        ACT_END(t);
    }
    ACT_CALL(0, rx_podium_setup(a, U32(a, 0x234) + 0x20150));
    if (!U32(U32(a, 0x234), 0x20018) && U32(0, 0x00d60ff0) && !U32(0, 0x00ae1b90) && !U32(a, 0xfe8)) {
        ACT_CALL(0, rx_local_vehicles_vf49()); SU32(a, 0x13e610, 2);
    }
    ACT_END(f);
}
RX_FUNCTION(0x00578f40, rx_waypoint_activity_vf37)

/* 005791a0: the actual pre-race frame, including its post-decrement delay. */
void rx_waypoint_activity_vf25(rx_addr a, float dt) {
    ACT_FRAME(f, 0, 1);
    uint32_t busy = U32(0, 0x00a81ad8) + U32(0, 0x00a81ad0) + U32(0, 0x00a81ac8);
    if ((int32_t)busy > 0 || U32(a, 0xff068)) { ACT_END(f); return; }
    if (!act_vcall(a, 0x2c, 0, 0)) {
        SU32(a, 0x114254, 0); SU32(a, 0xff070, 1);
        ACT_CALL(0, rx_scenario_anims_reset_cameras(U32(a, 0x234) + 0x20150));
        rx_addr entity = U32(U32(a, 0x234), 0x203e0);
        if (entity) { SU8(entity, 0xa2, 0); SU32(entity, 0x70, 0); }
        ACT_CALL(0, rx_scenario_anims_stop_actors(U32(a, 0x234) + 0x20150));
        ACT_CALL(0, rx_scenario_cast_stop_all(U32(a, 0x234) + 0x20150));
    }
    if (U32(a, 0xfe8) || U32(0, 0x00ae1b90)) SU32(a, 0xff070, 1);
    int32_t delay = I32(a, 0x13e610); SU32(a, 0x13e610, (uint32_t)delay - 1);
    ACT_FRAME(t, 0, 1);
    if (delay <= 0 && act_vcall(a + 0xff0a0, 0x1c, 0, 0) && !U32(a, 0xfe8) && !U32(0, 0x00ae1b90)) {
        uint32_t started; ACT_CALLR(started, 0, rx_scenario_start(a + 0xff0a0));
        if (started) {
            /* Presentation-only GotoEventIntroPage. Existing core UI boundary. */
            rx_regs in = {0}, out; uint32_t flag = 0;
            in.r[0] = 0x00941f7c; in.r[1] = U32(0, 0x00a93fcc);
            act_ui_call_ex(0x0059b7e0, &in, &out, 1, &flag);
        }
        SU32(a, 0xff070, 1);
    }
    if (!U32(a, 0x224)) {
        ACT_CALL(1, rx_activity_pre(a, waypoint_float_arg(dt)));
        if (!act_vcall(a + 0xff0a0, 0x1c, 0, 0)) {
            /* This owner reads the original dt argument slot. */
            rx_addr saved = rx_sp(); rx_set_sp(saved - 12);
            SU32(rx_sp(), 4, a + 0xff0a0); SF32(rx_sp(), 8, waypoint_float_arg(dt));
            rx_scenario_intro_update(a + 0xff0a0, waypoint_float_arg(dt)); rx_set_sp(saved);
        }
    }
    ACT_END(t); ACT_END(f);
}
RX_FUNCTION(0x005791a0, rx_waypoint_activity_vf25)

/* 00579300: completion is based on each driver's actual lap count. Online
 * observers mark finish flags without advancing the authoritative state. */
void rx_waypoint_activity_vf42(rx_addr a) {
    ACT_FRAME(f, 0x14, 1);
    int32_t mode = I32(a, 0x238);
    if (mode >= 2 && mode <= 3) { ACT_END(f); return; }
    ACT_FRAME(t, 0, 3);
    if (!(U32(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "session static initializer has not run");
    rx_addr session = U32(0, 0x00da6cb0);
    int32_t count = I32(a, 0x26c);
    if (U32(session, 0x1c4c) && U32(session, 0x1b0) != 1 && !U32(session, 0x1c48) && !U32(a, 0xbc0)) {
        for (int32_t i = 0; i < count; ++i) {
            uint32_t index = (uint32_t)i;
            if (act_vcall(a, 0xf4, 1, &index)) SU32(U32(U32(a, 0x268), 4u * index), 0x70, 1);
        }
        ACT_END(t); ACT_END(f); return;
    }
    int32_t humans = 0, ai = 0, finished_humans = 0, finished_others = 0, disconnected = 0;
    for (int32_t i = 0; i < count; ++i) {
        rx_addr player = U32(U32(U32(a, 0x268), 4u * (uint32_t)i), 0x74);
        if (U32(player, 0x10) == 0) ++humans;
    }
    for (int32_t i = 0; i < count; ++i) {
        rx_addr player = U32(U32(U32(a, 0x268), 4u * (uint32_t)i), 0x74);
        if (U32(player, 0x10) == 1) ++ai;
    }
    for (int32_t i = 0; i < count; ++i) {
        uint32_t index = (uint32_t)i;
        rx_addr driver = U32(U32(a, 0x268), 4u * index);
        if (act_vcall(a, 0xf4, 1, &index)) {
            if (U32(U32(driver, 0x74), 0x10) == 0) ++finished_humans; else ++finished_others;
            SU32(driver, 0x70, 1);
        }
        if (!(U32(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "session static initializer has not run");
        session = U32(0, 0x00da6cb0);
        rx_addr player = U32(driver, 0x74);
        if (U32(session, 0x1c4c) && U32(session, 0x1b0) != 1 && !U32(player, 0x10) && U32(player, 0x1e4) && !U32(driver, 0x70)) ++disconnected;
    }
    int advance = 0;
    if (humans <= 0) advance = ai > 0 && ai == finished_others;
    else if (humans == disconnected + finished_humans || humans == finished_humans) {
        int32_t last_human = 0;
        count = I32(a, 0x26c);
        for (int32_t i = 0; i < count; ++i)
            if (!U32(U32(rx_stand(a, (uint32_t)i), 0x74), 0x10)) last_human = i;
        float leader_time = F32(U32(a, 0x2d0), 0xf14 + 4u * U32(a, 0xff090));
        for (int32_t i = last_human; i < count; ++i) {
            rx_addr driver = rx_stand(a, (uint32_t)i);
            if (U32(U32(driver, 0x74), 0x10) != 1) continue;
            if (F32(driver, 0x10b0) == F32(0, 0x00977494)) {
                if (U32(a, 0xff090) == 1) SU32(driver, 0x10b0, U32(0, 0x00977494));
                else if (U32(a, 0xff090) == 2) SF32(driver, 0x10b0, rx_adds(F32(driver, 0x10a4), leader_time));
            }
            SU32(driver, 0xf10, U32(a, 0xff090));
        }
        advance = 1;
    } else if (finished_humans > 0 || (U32(0, 0x00ae1b98) && ai > 0 && ai == finished_others)) {
        float timer = F32(a, 0x2a4);
        if (timer < 0) SU32(a, 0x2a4, 0);
        else if (timer > F32(a, 0x2ac)) advance = 1;
    }
    if (advance) ACT_CALL(2, rx_activity_set_state(a, 3));
    ACT_END(t); ACT_END(f);
}
RX_FUNCTION(0x00579300, rx_waypoint_activity_vf42)

/* 0057eb80: signed lap-completion check on the selected driver's own state. */
uint32_t rx_waypoint_activity_vf61(rx_addr a, uint32_t index) {
    rx_addr driver = U32(U32(a, 0x268), index * 4);
    return I32(driver, 0xf10) >= I32(a, 0xff090);
}
RX_FUNCTION(0x0057eb80, rx_waypoint_activity_vf61)

#include "waypoint_lap_support.h"
#include "loop_player.h"
#include "world_wreck.h"
#include "fx.h"
#include "race.h"

/* 0057a050: Waypoint's own lap event handler. Its finish count is not clamped
 * as RaceStub's is, and it neither addresses RaceDriver fields nor starts the
 * RaceStub finish script. Localization and presence use existing UI helpers. */
void rx_waypoint_lap_completed(rx_addr a, rx_addr driver, int32_t lap, uint32_t flag, int32_t laps) {
    (void)flag;
    const rx_addr entry = rx_sp();
    ACT_FRAME(f, 0x214, 3);
    if (U32(a, 0x238) != 1) { ACT_END(f); return; }
    ACT_FRAME(t, 0, 1);
    uint32_t finished = 0;
    if (U32(driver, 0xf10) == U32(a, 0xff090)) {
        ACT_CALL(0, rx_activity_sort_standings(a));
        SU32(a, 0x300, U32(a, 0x300) + 1);
        rx_addr manager; ACT_CALLR(manager, 0, rx_fx_session_manager());
        rx_addr session = U32(manager, 0x10);
        int online = U32(session, 0x1c4c) && U32(session, 0x1b0) != 1;
        if (U32(a, 0x300) == 1) {
            rx_addr text = rx_waypoint_lap_message(entry, a, driver, 1, online);
            ACT_CALL(2, rx_event_player_wins(U32(a, 0x290), a, driver, text, 0));
            ACT_CALLR(manager, 0, rx_fx_session_manager()); session = U32(manager, 0x10);
            if (U32(session, 0x1c4c) && U32(session, 0x1b0) != 1 && F32(a, 0x2a4) < 0) SU32(a, 0x2a4, 0);
            ACT_CALL(0, rx_player_set_fade_hold(U32(driver, 0x74)));
        } else {
            rx_addr text = rx_waypoint_lap_message(entry, a, driver, 0, online);
            ACT_CALL(2, rx_event_player_wins(U32(a, 0x290), U32(U32(driver, 0x74), 0x94), driver, text, 0));
        }
        ACT_CALL(0, rx_player_set_fade_hold(U32(driver, 0x74))); finished = 1;
    }
    rx_addr player = U32(driver, 0x74);
    if (U32(0, 0x00a942b4) == 1 && U32(player, 4) && !U32(player, 0x10))
        ACT_CALL(0, rx_world_clip_recorder_candidate_of_type(finished ? 9 : 7, driver));
    player = U32(driver, 0x74);
    if (U32(player, 4) && !U32(player, 0x10)) {
        int32_t next = (int32_t)((uint32_t)lap + 1);
        next = next < 1 ? 1 : next > laps ? laps : next;
        rx_waypoint_lap_presence(U32(player, 0x1f0), 0x10, (uint32_t)next);
        rx_waypoint_lap_presence(U32(U32(driver, 0x74), 0x1f0), 0x11, (uint32_t)laps);
    }
    ACT_END(t); ACT_END(f);
}
RX_FUNCTION(0x0057a050, rx_waypoint_lap_completed)

#include "race_finish.h"
/* 00559a40: the original results-page event with a four-byte activity payload. */
void rx_waypoint_show_results(rx_addr a) {
    ACT_FRAME(f, 4, 2);
    SU32(f, 8, a);
    act_raise(U32(a, 0x290), 0x00941c30, 0x009cbd24, f+8, 4);
    ACT_END(f);
}
RX_FUNCTION(0x00559a40, rx_waypoint_show_results)

/* 0057a500, complete97-byte owner: state2 ends Finish, starts replay type8,
 * then emits the original typed Show Activity Results event in that order. */
void rx_waypoint_post_state(rx_addr a, uint32_t state) {
    ACT_FRAME(f, 4, 2);
    SU32(a, 0x13e618, state);
    if (state==2) {
        ACT_CALL(1, rx_event_scenario(0x00941eac, 0x00941c1c, U32(a, 0x290), a));
        ACT_CALL(1,rx_replay_start(8));
        /* Original57a52d..57a558: activity payload occupies the saved ECX
         * local at entryESP-4 (f+8). Reload the channel after replay setup. */
        const rx_addr channel=U32(a,0x290);
        SU32(f,8,a);
        act_raise(channel,0x00941c30,0x009cbd24,f+8,4);
    }
    ACT_END(f);
}
RX_FUNCTION(0x0057a500, rx_waypoint_post_state)

/* 0057a570: actual Waypoint post state machine; both scenarios retain their own
 * offsets and virtual completion checks. No synthetic scenario success. */
void rx_waypoint_update_post(rx_addr a, float dt) {
    ACT_FRAME_EBP(f, 8, 0, 2);
    uint32_t state=U32(a, 0x13e618);
    if (state==0) {
        uint32_t started=0;
        if (act_vcall(a, 0x30, 0, 0)) {
            ACT_CALL(0, rx_podium_setup(a, U32(a, 0x234)+0x20150));
            ACT_CALLR(started, 0, rx_scenario_start(a+0x114270));
        }
        if (started) {
            ACT_CALL(1, rx_event_scenario(0x00941eac, 0x00941c04, U32(a, 0x290), a));
            SU32(a, 0x13e618, 1);
        } else ACT_CALL(0, rx_waypoint_post_state(a, 2));
    } else if (state==1 || state==3) {
        rx_addr sc=a+(state==1 ? 0x114270 : 0x129440);
        /* The canonical update reads its dt from the guest stack. */
        SF32(rx_sp()-4, 0, waypoint_float_arg(dt));
        SU32(rx_sp()-8, 0, sc);
        ACT_CALL(2, rx_scenario_intro_update(sc, waypoint_float_arg(dt)));
        if (act_vcall(sc, 0x20, 0, 0)) {
            if (state==1) ACT_CALL(0, rx_waypoint_post_state(a, 2));
            else {
                ACT_CALL(1, rx_event_scenario(0x00941fd8, 0x00941c1c, U32(a, 0x290), a));
                ACT_CALL(1, rx_podium_release(U32(a, 0x234)+0x20150, 1));
                ACT_CALL(1, rx_event_activity(U32(a, 0x290), 0x00941c48, a));
            }
        }
    } else if (state==2) ACT_CALL(0, rx_activity_result_flags(a, 1));
    ACT_END(f);
}
RX_FUNCTION(0x0057a570, rx_waypoint_update_post)

/* 00579940: first results update event and SSE clock, then the post state. */
void rx_waypoint_activity_vf29(rx_addr a, float dt) {
    ACT_FRAME(f, 0, 1);
    if (F32(a, 0xff064)<=0.0f && act_vcall(a, 0xe4, 0, 0))
        ACT_CALL(0, rx_waypoint_show_results(a));
    SF32(a, 0xff064, rx_adds(dt, F32(a, 0xff064)));
    SF32(rx_sp()-4, 0, waypoint_float_arg(dt));
    ACT_CALL(1, rx_waypoint_update_post(a, waypoint_float_arg(dt)));
    ACT_END(f);
}
RX_FUNCTION(0x00579940, rx_waypoint_activity_vf29)

#include "waypoint_playback.h"
#include "loop_world.h"
/* 00579b20: post exit, shared with Freestyle. Unlike RaceStub no podium release. */
void rx_waypoint_activity_vf35(rx_addr a, uint32_t next) {
    (void)next;
    ACT_FRAME(f, 0, 2);
    ACT_CALL(1, rx_activity_enable_controls(1, a));
    rx_addr world=U32(a, 0x234); SU32(world, 0x21de0, 0);
    ACT_CALL(2, rx_replay_cameras_update(world, rx_bf(0x3d072b02)));
    SU32(0, 0x00a941b8, 0);
    ACT_CALL(0, rx_clip_recorder_stop_playback());
    SU32(0, 0x00a942b8, 0);
    ACT_CALL(0, rx_activity_result_flags(a, 0));
    ACT_END(f);
}
RX_FUNCTION(0x00579b20, rx_waypoint_activity_vf35)

/* 0057a6b0: close the replay and enter the actual ResultsScenario, or complete. */
void rx_waypoint_activity_vf58(rx_addr a) {
    ACT_FRAME(f, 4, 2);
    ACT_CALL(0, rx_clip_recorder_stop_playback());
    uint32_t started=0;
    if (act_vcall(a, 0x34, 0, 0)) ACT_CALLR(started, 0, rx_scenario_start(a+0x129440));
    if (started) {
        ACT_CALL(1, rx_event_scenario(0x00941fd8, 0x00941c04, U32(a, 0x290), a));
        SU32(a, 0x13e618, 3);
    } else {
        ACT_CALL(1, rx_podium_release(U32(a, 0x234)+0x20150, 1));
        SU32(f, 8, a);
        act_raise(U32(a, 0x290), 0x00941c48, 0x009cbd24, f+8, 4);
    }
    ACT_END(f);
}
RX_FUNCTION(0x0057a6b0, rx_waypoint_activity_vf58)

/* 00579990: actual Waypoint records use total driver time+134, best lap+10b0,
 * and lap count+ff090. Local records-slot -1 still reaches the guarded placing,
 * score and best-lap owners, exactly as in the original. */
void rx_waypoint_activity_vf41(rx_addr a, uint32_t previous) {
    (void)previous;
    const rx_addr entry=rx_sp();
    ACT_FRAME(f, 0x28, 2);
    ACT_CALL(1, rx_activity_post_enter(a));
    SU32(f, 8, 0);
    if (I32(a, 0x300)>0) {
        ACT_FRAME(t, 0, 2);
        SU32(entry, 4, a+0x2d0);
        for (int32_t i=0; i<I32(a, 0x300); ++i) {
            rx_addr driver=U32(U32(entry, 4), 0), player=U32(driver, 0x74);
            if (U32(player, 4) && !U32(player, 0x10)) {
                uint32_t slot=U32(player, 0x1f0);
                rx_addr records=U32(0, 0x00a946c0), rowref=records+slot*0x18u;
                if (slot!=UINT32_MAX && U32(rowref,4)!=UINT32_MAX) {
                    SU32(t, 0x14, U32(rowref,4)); SU32(t, 0x18, 0); SU32(t, 0x1c, U32(rowref,0));
                    ACT_CALL(3, rx_records_add_int(t+0x14, 1, 0x0096d99c));
                }
                ACT_CALL(1, (void)rx_records_placing(U32(0, 0x00a946c0), slot, i+1));
                if (slot!=UINT32_MAX) {
                    rowref=U32(0, 0x00a946c0)+slot*0x18u;
                    if (U32(rowref,4)!=UINT32_MAX) {
                        SU32(t, 0x20, U32(rowref,4)); SU32(t, 0x24, 0); SU32(t, 0x28, U32(rowref,0));
                        ACT_CALL(3, rx_records_add_int(t+0x20, I32(a,0xff090), 0x0096d990));
                    }
                    rowref=U32(0, 0x00a946c0)+slot*0x18u;
                    if (U32(rowref,4)!=UINT32_MAX) {
                        SU32(t, 0x2c, U32(rowref,4)); SU32(t, 0x30, 0); SU32(t, 0x34, U32(rowref,0));
                        ACT_CALL(2, rx_records_add_total_time(t+0x2c, waypoint_float_arg(F32(driver,0x134))));
                    }
                }
                ACT_CALL(1, (void)rx_records_score(U32(0, 0x00a946c0), slot, waypoint_float_arg(F32(U32(driver,0x74),0xcec))));
                ACT_CALL(1, (void)rx_records_best_lap(U32(0, 0x00a946c0), slot, waypoint_float_arg(F32(driver,0x10b0))));
            }
            SU32(entry, 4, U32(entry,4)+4); SU32(t, 0x10, (uint32_t)i+1);
        }
        ACT_END(t);
    }
    /* The existing boundary owns its call frame; do not add another one. */
    rx_waypoint_finish_highlights(a);
    SU32(a, 0x13e618, 0);
    ACT_END(f);
}
RX_FUNCTION(0x00579990, rx_waypoint_activity_vf41)
