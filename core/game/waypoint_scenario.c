/* WaypointIntroScenario and its actual placement/camera dependencies.
 * All offsets and operation ordering follow the saved complete original spans. */
#include "waypoint_scenario.h"
#include "build_activity_int.h"
#include "race_activity.h"
#include "anim.h"
#include "anim_scenario.h"
#include "anim_scenario_race_intro.h"
#include "anim_scenario_misc.h"
#include "world_veh.h"
#include "world_node.h"
#include "phys_terrain.h"
#include "phys_crt.h"
#include "rider_cap.h"

/* 0074e400, ECX=player,EAX=fallback, stack index; switch table0074e468. */
rx_addr rx_waypoint_chief_clip(rx_addr player, uint32_t index, rx_addr fallback) {
    if (index >= 5) act_unreached(0x0074e404, "invalid waypoint chief clip switch index");
    if (index == 2) return fallback;
    if (index == 4) return 0x0096cba8;
    uint32_t kind = U32(U32(player, 0x1ac), 0x194);
    if (index < 2) return kind == 0 ? 0x0096cb34 : kind == 1 ? 0x0096cb54 : 0x0096cb70;
    return kind == 0 ? 0x0096cb90 : kind == 1 ? 0x0096ccdc : 0x0096cba8;
}
RX_FUNCTION(0x0074e400, rx_waypoint_chief_clip)
/* 0074e870 has the same default-register case but different clips. */
rx_addr rx_waypoint_camera_clip(rx_addr player, uint32_t index, rx_addr fallback) {
    if (index >= 5) act_unreached(0x0074e874, "invalid waypoint camera clip switch index");
    if (index == 2) return fallback;
    if (index == 4) return 0x0096ccc4;
    uint32_t kind = U32(U32(player, 0x1ac), 0x194);
    return kind == 0 ? 0x0096cbd0 : kind == 1 ? 0x0096cbe4 : 0x0096cbf8;
}
RX_FUNCTION(0x0074e870, rx_waypoint_camera_clip)

/* 0074e8e0: hide riders, clear mud, pause vehicles and choose first step. */
void rx_waypoint_intro_vf0(rx_addr sc) {
    ACT_FRAME(f, 0, 3);
    SU32(sc, 0x151c4, 0);
    rx_addr world = U32(U32(sc, 0x14), 0x234);
    int32_t count = I32(U32(world, 0x20190), 0x26c);
    for (int32_t i = 0; i < count; ++i) {
        rx_addr a = U32(world, 0x20190);
        rx_addr player = U32(U32(U32(a, 0x268), 4u * (uint32_t)i), 0x74);
        SU8(U32(player, 0x1b4), 0xa2, 0);
    }
    SU32(sc, 0x151c0, 0);
    ACT_CALL(1, rx_podium_riders_clear_mud(U32(U32(sc, 0x14), 0x234) + 0x20150));
    ACT_CALL(0, rx_scenario_set_riders_paused(sc, 1));
    if (!(U32(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "session static initializer has not run");
    rx_addr session = U32(0, 0x00da6cb0);
    SU32(sc, 0x151b4, ((!U32(session, 0x1c4c) || U32(session, 0x1b0) == 1) && !U32(U32(sc, 0x14), 0x1068)) ? 1 : 0);
    ACT_END(f);
}
RX_FUNCTION(0x0074e8e0, rx_waypoint_intro_vf0)

/* 0074e350 includes its switch tail through0074e3df. */
uint32_t rx_waypoint_intro_vf3(rx_addr sc) {
    ACT_FRAME(f, 0, 1);
    uint32_t step = U32(sc, 0x151b4), result = 1;
    if (step == U32(0, 0x00d610a0)) { ACT_END(f); return 1; }
    ACT_FRAME(t, 0, 1);
    if (step == 1) SU32(sc, 0x151b4, 2);
    else if (step == 2) {
        rx_addr driver; ACT_CALLR(driver, 1, rx_activity_human_driver(U32(sc, 0x14), 0));
        SU32(sc, 0x151b4, 4 + (U32(U32(U32(driver, 0x74), 0x1ac), 0x194) > 1));
    } else if (step == 4 || step == 5) {
        ACT_FRAME(g, 0, 1);
        uint32_t count; ACT_CALLR(count, 0, rx_activity_count_humans_of(U32(sc, 0x14), 1));
        SU32(sc, 0x151c0, U32(sc, 0x151c0) + 1);
        ACT_END(g);
        if (I32(sc, 0x151c0) >= (int32_t)count) { result = 0; SU32(sc, 0x151b4, 0); }
    }
    ACT_END(t); ACT_END(f); return result;
}
RX_FUNCTION(0x0074e350, rx_waypoint_intro_vf3)

/* 0074eb40: shared FMX/Waypoint previous-step override. */
uint32_t rx_waypoint_intro_vf13(rx_addr sc) {
    SU32(sc, 0x151c0, 0);
    if (I32(sc, 0x151b4) > 1 && U32(sc, 0x151b4) - 4u < 2) { SU32(sc, 0x151b4, 1); return 1; }
    return 0;
}
RX_FUNCTION(0x0074eb40, rx_waypoint_intro_vf13)

/* 0074e480: place each chief on the vehicle's horizontal heading, including
 * the selected human's special chief and the original terrain adjustment. */
void rx_waypoint_intro_place_chiefs(rx_addr sc, uint32_t step) {
    ACT_FRAME_EBP(f, 16, 0xa4, 3);
    rx_addr activity = U32(sc, 0x14), sm = U32(activity, 0x234) + 0x20150;
    int32_t count = I32(activity, 0x26c);
    SU32(f, 0x3c, sm); SU32(f, 0x1c, (uint32_t)count);
    if ((int32_t)step <= 0 || (int32_t)step > 6) { ACT_END(f); return; }
    ACT_CALL(0, rx_scenario_anims_reset_cameras(sm));
    if (I32(sm, 0x172c) < count) { ACT_END(f); return; }
    rx_addr selected = 0;
    if (step == 4 || step == 5) ACT_CALLR(selected, 0, rx_race_intro_human_player(sc));
    SU32(f, 0x18, selected); SU32(f, 0x14, sm + 0x16ec);
    for (int32_t i = 0; i < I32(f, 0x1c); ++i) {
        rx_addr player = U32(U32(U32(U32(sc, 0x14), 0x268), 4u * (uint32_t)i), 0x74);
        rx_addr chief = player == U32(f, 0x18) ? U32(U32(f, 0x3c), 0x1730) : U32(U32(f, 0x14), 0);
        if (chief) {
            uint32_t matrix = f + 0x50;
            act_vcall(U32(player, 0x1ac), 0x3c, 1, &matrix);
            for (uint32_t j = 0; j < 3; ++j) SU32(f, 0x40 + 4*j, U32(f, 0x80 + 4*j));
            if (!(U32(0, 0x009e30e0) & 1)) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
            rx_addr terrain; ACT_CALLR(terrain, 0, rx_world_find_by_controller(0x009e30d4));
            if (terrain) { uint32_t edx; ACT_CALL(1, phys_terrain_base_height_at(terrain, f + 0x40, &edx)); }
            float x = F32(f, 0x70), z = F32(f, 0x78);
            float squared = rx_adds(rx_muls(x, x), rx_muls(z, z));
            SF32(f, 0x20, x); SF32(f, 0x24, 0); SF32(f, 0x28, z); SF32(f, 0x38, squared);
            if (squared > 0) {
                float length; ACT_CALLR(length, 1, phys_sqrtf(squared));
                float reciprocal = rx_divs(1.0f, length);
                SF32(f, 0x20, rx_muls(reciprocal, F32(f, 0x70)));
                SF32(f, 0x24, rx_muls(reciprocal, 0.0f));
                SF32(f, 0x28, rx_muls(reciprocal, F32(f, 0x78)));
            }
            SU32(f, 0x90, 0); SU32(f, 0x94, 0x3f800000); SU32(f, 0x98, 0);
            ACT_CALL(1, rx_world_vec3_cross(f + 0xa0, f + 0x90, f + 0x20));
            for (uint32_t j = 0; j < 3; ++j) SU32(f, 0x50 + 4*j, U32(f, 0xa0 + 4*j));
            SU32(f, 0x5c, 0); SU32(f, 0x60, 0); SU32(f, 0x64, 0x3f800000); SU32(f, 0x68, 0); SU32(f, 0x6c, 0);
            for (uint32_t j = 0; j < 3; ++j) SU32(f, 0x70 + 4*j, U32(f, 0x20 + 4*j));
            SU32(f, 0x7c, 0); SU32(f, 0x80, U32(f, 0x40));
            SF32(f, 0x84, rx_adds(F32(f, 0x44), F32(0, 0x00931da4)));
            SU32(f, 0x88, U32(f, 0x48)); SU32(f, 0x8c, 0x3f800000);
            act_vcall(chief, 0x40, 1, &matrix);
            rx_addr clip = 0;
            if (step - 1u < 5) ACT_CALLR(clip, 1, rx_waypoint_chief_clip(player, step - 1u, 0));
            ACT_CALL(2, rx_activity_setup_object(clip, U32(sc, 0x14), chief));
            ACT_CALL(1, rx_crew_chief_set_shirt(U32(player, 0x1ac), chief));
        }
        SU32(f, 0x14, U32(f, 0x14) + 4);
    }
    ACT_END(f);
}
RX_FUNCTION(0x0074e480, rx_waypoint_intro_place_chiefs)

/* 0074e7a0: launch the existing camera cast for each local player. */
void rx_waypoint_intro_cameras(rx_addr sc, uint32_t step) {
    ACT_FRAME(f, 4, 4);
    if (step == 1 || step == 2 || step == 4 || step == 5) {
        for (int32_t i = 0; i < (int32_t)U8(U32(0, 0x00a946b8), 0x1541c); ++i) {
            rx_addr player; ACT_CALLR(player, 2, rx_player_active(U32(0, 0x00a946b8) + 0x7460, (uint32_t)i, 3));
            rx_addr clip; ACT_CALLR(clip, 1, rx_waypoint_camera_clip(player, 1, 0)); SU32(f, 0x10, clip);
            rx_addr sm = U32(U32(sc, 0x14), 0x234) + 0x20150, cast = 0;
            for (uint32_t j = 0; j < RX_RC_MAX; ++j) {   /* (w-rc-core) entries 12..31 in side storage */
                rx_addr owner = rx_cast_u32(sm, j, 4);
                if (!owner) break;
                if (owner == player) { cast = rx_cast_rec_ro(sm, j); break; }
            }
            ACT_CALL(4, rx_scenario_cast_place_entity(sm, 0, U32(cast, 0), 0, U32(f, 0x10), 0));
        }
    }
    ACT_END(f);
}
RX_FUNCTION(0x0074e7a0, rx_waypoint_intro_cameras)

/* 0074e9b0: enter step, then anchor the showcase transform at rider chest. */
void rx_waypoint_intro_vf1(rx_addr sc, uint32_t step) {
    ACT_FRAME_EBP(f, 16, 0x54, 3);
    SU32(f, 0x18, sc);
    ACT_CALL(1, rx_race_intro_card_flags(step, sc));
    ACT_CALL(2, rx_waypoint_intro_place_chiefs(sc, step));
    ACT_CALL(1, rx_waypoint_intro_cameras(sc, step));
    if ((int32_t)step >= 4 && (int32_t)step <= 5) {
        rx_addr a = U32(sc, 0x14), driver, cast;
        ACT_CALLR(driver, 1, rx_activity_human_driver(a, I32(sc, 0x151c0)));
        ACT_CALLR(cast, 1, rx_scenario_cast_find(U32(a, 0x234) + 0x20150, U32(driver, 0x74)));
        rx_addr actor = U32(cast, 0);
        rx_addr matrix = act_vcall(actor, 0x38, 0, 0);
        rx_addr world = U32(U32(sc, 0x14), 0x234);
        for (uint32_t i = 0; i < 64; i += 4) SU32(world, 0x20150 + i, U32(matrix, i));
        SU32(f, 0x1c, matrix);
        rx_addr hierarchy = act_vcall(actor, 0x70, 0, 0);
        uint32_t exists; ACT_CALLR(exists, 1, rx_hierarchy_has_node(hierarchy, 0x009581fc));
        if (exists) {
            rx_addr node; ACT_CALLR(node, 1, rx_hierarchy_find_node(hierarchy, 0x009581fc)); SU32(f, 0x1c, node);
            matrix = act_vcall(actor, 0x38, 0, 0);
            for (uint32_t i = 0; i < 64; i += 4) SU32(f, 0x20 + i, U32(matrix, i));
            ACT_CALL(1, rx_world_node_evaluate_world(node, 0));
            rx_addr node_world = U32(U32(node, 0), 0x1c) + 64u * U8(node, 0xb);
            for (uint32_t i = 0; i < 3; ++i) SU32(f, 0x50 + 4*i, U32(node_world, 0x30 + 4*i));
            matrix = f + 0x20;
        } else matrix = U32(f, 0x1c);
        world = U32(U32(U32(f, 0x18), 0x14), 0x234);
        for (uint32_t i = 0; i < 64; i += 4) SU32(world, 0x21d50 + i, U32(matrix, i));
    }
    ACT_END(f);
}
RX_FUNCTION(0x0074e9b0, rx_waypoint_intro_vf1)
