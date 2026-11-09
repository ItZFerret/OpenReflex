#include "replay_clip.h"
/* Hand-translated shared Waypoint finish/results transitions. */
#include "waypoint_scenario.h"
#include "build_activity_int.h"
#include "anim_scenario.h"
#include "anim_scenario_race_intro.h"
#include "race_finish_select.h"
#include "phys_body.h"
#include "fx.h"
#include "world_node.h"
#include "rider_cap.h"

/* The loops are inlined source factoring, not calls in the original. Preserve
 * the snapshot count and reload the activity/driver pointers each iteration. */
static void finish_riders_visible(rx_addr sc, uint8_t visible) {
    rx_addr podium = U32(U32(sc, 0x14), 0x234) + 0x20150;
    int32_t count = I32(U32(podium, 0x40), 0x26c);
    for (int32_t i=0; i<count; ++i) {
        rx_addr driver=U32(U32(U32(podium, 0x40), 0x268), 4u*(uint32_t)i);
        SU8(U32(U32(driver, 0x74), 0x1b4), 0xa2, visible);
    }
}
static void finish_vehicles_visible(rx_addr sc, uint8_t visible) {
    rx_addr podium = U32(U32(sc, 0x14), 0x234) + 0x20150;
    int32_t count = I32(U32(podium, 0x40), 0x26c);
    for (int32_t i=0; i<count; ++i) {
        rx_addr player=U32(U32(U32(U32(podium, 0x40), 0x268), 4u*(uint32_t)i), 0x74);
        SU8(U32(U32(player, 0x1ac), 0x1328), 0x14e, visible);
        rx_addr handle=U32(U32(player, 0x1ac), 0x17c);
        if (handle) {
            rx_addr record; ACT_CALLR(record, 0, phys_body_record(handle));
            SU32(U32(record, 0x2c), 0x14, visible ? 0 : UINT32_MAX);
        }
    }
}

/* 0074bcc0: copy the vehicle's saved scalar to each existing local cast actor. */
void rx_waypoint_podium_scalars(rx_addr podium) {
    uint32_t count=U8(U32(0, 0x00a946b8), 0x1541c);
    if (!count) return;
    ACT_FRAME(f, 0, 3);
    for (uint32_t i=0; i<count; ++i) {
        rx_addr cast=rx_cast_rec_ro(podium,i), actor=cast?U32(cast, 0):0;   /* (w-rc-core) entries 12..31 in side storage */
        if (!actor) continue;
        rx_addr vehicle=U32(U32(cast, 4), 0x1ac);
        uint32_t scalar=0;
        for (uint32_t j=0; j<12; ++j) if (vehicle==U32(0, 0x00a96dc0+0x40*j)) {
            scalar=U32(0, 0x00a96d90+0x40*j); break;
        }
        if (!scalar && rx_mud_side_of(vehicle)) scalar=U32(rx_mud_side_of(vehicle), 0);   /* (w-rc-core) */
        for (uint32_t j=0; j<4; ++j) SU32(actor, 0x3b0+4*j, scalar);
    }
    ACT_END(f);
}
RX_FUNCTION(0x0074bcc0, rx_waypoint_podium_scalars)

/* 0074e230: check the actual finish-script resource, through the asset manager. */
uint32_t rx_waypoint_finish_script_exists(rx_addr activity) {
    ACT_FRAME(f, 0x108, 1);
    SU32(f, 0x108, U32(0, 0x009b45e4) ^ (f+4));
    ACT_CALL(3, rx_scenario_intro_script_name(7, activity, f+4));
    rx_addr assets=U32(U32(0, 0x00a95844), 8);
    uint32_t exists=act_vcall(assets, 0x6c, 4, (uint32_t[]){f+4, 0x6c6d7862, 0, 0});
    ACT_END(f); return exists != 0;
}
RX_FUNCTION(0x0074e230, rx_waypoint_finish_script_exists)

/* 0057a4b0: finish presentation is available only for an offline activity. */
uint32_t rx_waypoint_activity_vf7(rx_addr activity) {
    uint32_t exists; ACT_CALLR(exists, 1, rx_waypoint_finish_script_exists(activity));
    if (!exists) return 0;
    if (!(U32(0, 0x00dcf304)&1)) act_unreached(0x006de1f0, "session static initializer has not run");
    rx_addr session=U32(0, 0x00da6cb0);
    return !U32(session, 0x1c4c) || U32(session, 0x1b0)==1;
}
RX_FUNCTION(0x0057a4b0, rx_waypoint_activity_vf7)

/* 0074eb80: Waypoint's finish selection uses its own state, not RaceStub mode8. */
uint32_t rx_waypoint_finish_vf3(rx_addr sc) {
    ACT_FRAME(f, 0, 3);
    uint32_t step=U32(sc, 0x151b4), result=1;
    if (step==U32(0, 0x00d610a0)) { ACT_END(f); return 1; }
    if (U32(0, 0x00d61048)) {
        result=(int32_t)step<12; SU32(sc, 0x151b4, result ? step+1 : 0);
    } else {
        if (!(U32(0, 0x009e275c)&1)) act_unreached(0x005b2370, "series static initializer has not run");
        uint32_t is_series=U32(0, 0x009e2318)==2;
        rx_addr manager;
        if (!is_series) { ACT_CALLR(manager, 0, rx_fx_series_manager()); is_series=U32(manager, 0x190)==1; }
        if (is_series) {
            ACT_CALLR(manager, 0, rx_fx_series_manager());
            rx_addr event=U32(manager, 0x18c);
            uint32_t kind=U32(event, 0xc); if (kind==2) kind=U32(event, 0x10);
            ACT_CALLR(result, 0, rx_finish_select_advance(sc, kind));
        } else {
            step=U32(sc, 0x151b4);
            if (step==7 && U32(sc, 0x151c4)) { SU32(sc, 0x151b8, 1); SU32(sc, 0x151b4, 8); }
            else if (step==7 || step==8) { result=0; SU32(sc, 0x151b4, 0); }
        }
    }
    ACT_END(f); return result;
}
RX_FUNCTION(0x0074eb80, rx_waypoint_finish_vf3)

/* 0074ec60: select the finish clip, hide the live scene, pause it, then stage7. */
void rx_waypoint_finish_vf0(rx_addr sc) {
    ACT_FRAME(f, 0, 1);
    ACT_CALL(0, rx_waypoint_podium_scalars(U32(U32(sc, 0x14), 0x234)+0x20150));
    uint32_t available; ACT_CALLR(available, 1, rx_waypoint_finish_script_exists(U32(sc, 0x14)));
    if (!available) { ACT_END(f); return; }
    ACT_FRAME(t, 0, 3);
    ACT_CALL(0, rx_finish_select_local_flags(sc));
    finish_riders_visible(sc, 0); finish_vehicles_visible(sc, 0);
    ACT_CALL(0, rx_scenario_sim_flag(sc));
    ACT_CALL(0, rx_scenario_set_riders_paused(sc, 1));
    SU32(sc, 0x151b4, 7); SU32(sc, 0x151b8, 1);
    ACT_END(t);
    uint32_t has_clip=0;
    for (rx_addr p=0x00d323c8; p<0x00d5a988; p+=0x35d0) if (U32(p,0)==9) { has_clip=1; break; }
    ACT_END(f);
    /* The no-clip branch is an actual tail call, so the entry stack is retained. */
    if (!has_clip) {
        rx_addr fn=U32(U32(sc,0),0xc);
        void *owner=rx_fn(fn);
        if (!owner) act_unreached(fn, "unported Waypoint finish selection tail target");
        (void)((uint32_t (*)(rx_addr))owner)(sc);
    }
}
RX_FUNCTION(0x0074ec60, rx_waypoint_finish_vf0)

/* 0074f890: results begin at stage9 for a local podium, otherwise stage11. */
void rx_waypoint_results_vf0(rx_addr sc) {
    ACT_FRAME(f, 0, 4);
    ACT_CALL(0, rx_finish_select_local_flags(sc));
    finish_riders_visible(sc, 0); finish_vehicles_visible(sc, 0);
    ACT_CALL(0, rx_scenario_sim_flag(sc));
    ACT_CALL(0, rx_scenario_set_riders_paused(sc, 1));
    SU32(sc, 0x151b4, U32(sc, 0x151c0) ? 9 : 11);
    ACT_END(f);
}
RX_FUNCTION(0x0074f890, rx_waypoint_results_vf0)

/* These two bodies differ only in their original scenario-name literal. */
static void finish_scenario_exit(rx_addr sc, rx_addr name) {
    ACT_FRAME(f, 0xc, 4);
    SU32(f, 0x18, U32(0, 0x009b45e4) ^ (f+0x10));
    finish_riders_visible(sc, 1); finish_vehicles_visible(sc, 1);
    ACT_CALL(0, rx_scenario_set_riders_paused(sc, 0));
    ACT_CALL(0, rx_scenario_anims_stop_actors(U32(U32(sc, 0x14), 0x234)+0x20150));
    ACT_CALL(0, rx_scenario_anims_reset_cameras(U32(U32(sc, 0x14), 0x234)+0x20150));
    rx_addr a=U32(sc, 0x14);
    SU32(f, 0x10, a); SU32(f, 0x14, name);
    act_raise(U32(a, 0x290), 0x00941c1c, 0x009cf660, f+0x10, 8);
    ACT_END(f);
}
void rx_waypoint_finish_vf6(rx_addr sc) { finish_scenario_exit(sc, 0x00941eac); }
RX_FUNCTION(0x0074ef90, rx_waypoint_finish_vf6)
void rx_waypoint_results_vf6(rx_addr sc) { finish_scenario_exit(sc, 0x00941fd8); }
RX_FUNCTION(0x0074f980, rx_waypoint_results_vf6)
/* 0074a630 actual results next-stage callback. */
uint32_t rx_waypoint_results_vf3(rx_addr sc) {
    if (U32(sc, 0x151b4)==U32(0, 0x00d610a0)) return 1;
    SU32(sc, 0x151b4, 0); return 0;
}
RX_FUNCTION(0x0074a630, rx_waypoint_results_vf3)
/* 0074f0e0 is an actual jump to the shared empty virtual. */
void rx_waypoint_post_vf12(void) { rx_world_shared_vf_empty(); }
RX_FUNCTION(0x0074f0e0, rx_waypoint_post_vf12)

#include "waypoint_post_steps.h"
#include "waypoint_playback.h"
#include "loop_world.h"
/*0074eda0: activate one real finish step. Step7's playback start still requires
 * the original clip pose/tick closure; it is explicitly unavailable, not skipped. */
void rx_waypoint_finish_vf1(rx_addr sc,uint32_t step) {
    ACT_FRAME(f,0,4);
    finish_vehicles_visible(sc,0);
    ACT_CALL(1,rx_post_step_cameras(sc,step));
    if (U32(0,0x00d61048)) ACT_CALL(0,rx_post_debug_standings(sc,(int32_t)step));
    if (step==7) {
        finish_riders_visible(sc,1);finish_vehicles_visible(sc,1);
        if (!U32(0,0x00d61048)) ACT_CALL(1,rx_replay_start(9));
    } else {
        if (U32(0,0x00a942b4)==2) {
            finish_riders_visible(sc,0);
            rx_addr world=U32(U32(sc,0x14),0x234);
            SU32(world,0x21de0,0);
            ACT_CALL(2,rx_replay_cameras_update(world,rx_f80_to_f32(rx_f80_from_f32(F32(0,0x00977818)),0)));
            SU32(0,0x009c407c,1);
            ACT_CALL(0,rx_clip_recorder_stop_playback());
        }
        ACT_CALL(0,rx_post_step_entities(sc,step));
    }
    ACT_END(f);
}
RX_FUNCTION(0x0074eda0,rx_waypoint_finish_vf1)

/*0074fbe0: hide the current vehicles and select the authored results camera. */
void rx_waypoint_results_vf1(rx_addr sc,uint32_t step) {
    ACT_FRAME(f,4,3);SU32(f,0xc,sc);
    finish_vehicles_visible(sc,0);
    ACT_CALL(1,rx_post_step_cameras(U32(f,0xc),step));
    ACT_END(f);
}
RX_FUNCTION(0x0074fbe0,rx_waypoint_results_vf1)
