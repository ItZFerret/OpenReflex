/* race_driver_update.c -- the drivers' per-frame updates (vf7): BaseDriver (clocks, the player update, the AI's control outputs),
 * NetworkDriver (the session's network object), RaceDriver (the race states: start sequence, AI racing, lap time, distance to the
 * line) and FreeRideDriver. AI control outputs on the driver (+0x3c0 .. +0x3e0, handed to the vehicle each frame): +0x3c0
 * throttle, +0x3c4 / +0x3c8 brakes (0..1), +0x3cc clutch (0..1), +0x3d0 (int) a button, +0x3d4 / +0x3d8 / +0x3dc / +0x3e0 steer and
 * lean axes (-1..1). (core/notes/race.md "Driver") */
#include "race.h"
#include "race_extern.h"
#include "race_driver.h"
#include "crt.h"
#include "world_veh.h"
#include "build_activity.h"
#include "bike_sim.h"
#include "race_beh1.h"
#include "race_activity.h"
#include "loop_player.h"
#include "loop_vehicle.h"
#include "loop_cam_misc.h"
#include "world_node.h"
#include "anim_scenario.h"
#include "world_rider.h"
#include "world_wreck.h"
#include "phys_crt.h"
#include "practice.h"   /* PORT ADDITION: the lap log */
#include "crt.h"

/* ---- calls into other subsystems (not in the core yet) --------------------------------------------------------------------- */
/* 00508970 DebugKeyMap::vf1 (ECX = map, args (mask, pad); RET 8) -> EAX: 0 while the debug keys are off (0x9b716d), else the
 * key map's latched value (KeyMap::vf1 00811420). Frame: push ecx. */
uint32_t rx_debug_keymap_latched(rx_addr self, uint32_t mask, uint32_t pad) {
    if (U8(0, 0x9b716d) != 0) return 0;
    RACE_FRAME_PLAIN(F, 0, 1);
    uint32_t r;
    RACE_CORE_CALL(2, r = rx_world_keymap_latched(self, mask, pad));
    RACE_FRAME_END(F);
    return r;
}
RX_FUNCTION(0x00508970, rx_debug_keymap_latched)
static uint32_t debug_key_pressed(rx_addr keymap) {        /* DebugKeyMap::vf1 00508970 (key 7, 0) */
    uint32_t r;
    RACE_CORE_CALL(2, r = rx_debug_keymap_latched(keymap, 7, 0));
    return r;
}
static uint32_t key_pressed(rx_addr keymap) {              /* KeyMap::vf2 008115f0 (7, 0) */
    uint32_t r;
    RACE_CORE_CALL(2, r = rx_world_keymap_down(keymap, 7, 0));
    return r;
}
static uint32_t controller_connected(uint32_t index) {     /* 00548880 (EAX = controller index) -> 0 / 1 */
    uint32_t r;
    RACE_CORE_CALL(0, r = rx_player_pressed_2fa0(index));
    return r;
}
static void player_update(rx_addr player, float dt) {      /* MXPlayer_Update 0054af90 (player, dt) */
    RACE_CORE_CALL(2, rx_player_update(player, dt));
}
static void vehicle_set_controls(rx_addr vehicle, rx_addr controls) {   /* RavenVehicle_SetControlInputs 0045f530 (ECX, EAX) */
    RACE_CORE_CALL(0, rx_vehicle_set_control_inputs(vehicle, controls));
}
static void rumble_stop(rx_addr pad) {                     /* 00621720 (EDI = the player's pad block, stack 0) */
    RACE_CORE_CALL(1, rx_rider_animation_set_stance(pad, 0));
}
static int stick_moved(uint32_t index) {                   /* 005485c0 (st0) / 00548620 (xmm0): the controller's axes */
    float a;
    RACE_CORE_CALL(0, a = rx_player_analog_348(index));
    if (!(a == 0.0f)) return 1;
    float b;
    RACE_CORE_CALL(0, b = rx_player_analog7_active(index));
    return !(b == 0.0f);
}
static int net_valid(rx_addr net) {                        /* 00884950 (ECX) -> AL */
    uint32_t r;
    RACE_CORE_CALL(0, r = rx_net_ref_valid(net));
    return (r & 0xff) != 0;
}

/* Vehicle vf43 (slot 43; 00466ae0 = bike +0x8d98): nonzero while in the air. */
static uint32_t vehicle_airborne_race(rx_addr v) {
    typedef uint32_t (*fn_t)(rx_addr);
    rx_addr fn = race_vfn(v, 43);
    fn_t f = RX_FN(fn, fn_t);
    return f ? f(v) : race_thiscall(fn, v, 0, 0, 0, 0, 0);
}
/* Vehicle vf55 (slot 55): set the control mode (0 local pad, 2 AI). */
static void vehicle_set_mode(rx_addr v, uint32_t mode) { race_thiscall(race_vfn(v, 55), v, 1, mode, 0, 0, 0); }
/* Behaviour vf6 (slot 6): reset. */
static void behavior_reset(rx_addr b) {
    typedef void (*fn_t)(rx_addr);
    rx_addr fn = race_vfn(b, 6);
    fn_t f = RX_FN(fn, fn_t);
    if (f) f(b); else race_thiscall(fn, b, 0, 0, 0, 0, 0);
}
/* Rule vf5 (slot 5): the update. */
static void rule_update(rx_addr r, float dt) {
    typedef void (*fn_t)(rx_addr, float);
    rx_addr fn = race_vfn(r, 5);
    fn_t f = RX_FN(fn, fn_t);
    if (f) RACE_CORE_CALL(1, f(r, dt)); else race_thiscall(fn, r, 1, rx_fb(dt), 0, 0, 0);   /* push dt; call vf5 */
}

static float clamp_axis(float x) { if (x >= 1.0f) return 1.0f; if (F32(0, 0x9cd9e8) /* -1 */ >= x) return F32(0, 0x9cd9e8); return x; }
static float clamp_unit(float x) { if (x >= 1.0f) return 1.0f; if (0.0f >= x) return 0.0f; return x; }

/* The debug keys (debug builds' cheats, the debug flag 0xae1b90; one latch 0xa89d00): toggle AI / pad control of the player,
 * cycle the camera target through the riders (and activities), cycle the player's difficulty tier. Returns 1 when the update ends. */
static int debug_keys(rx_addr d, int dbg) {
    rx_addr player = U32(d, 0x74);
    if (dbg != 0 && debug_key_pressed(0x00d607d8) != 0) {
        player = U32(d, 0x74);
        if (U32(player, 0x10) == 1) {
            SU32(player, 0x10, 0);
            SU32(U32(d, 0x74), 0x1f0, 0);
            vehicle_set_mode(U32(U32(d, 0x74), 0x1ac), 0);
            SU32(U32(U32(d, 0x74), 0x1b4), 0x5bb60, 1);
        } else if (U32(player, 0x10) == 0) {
            SU32(player, 0x10, 1);
            SU32(U32(d, 0x74), 0x1f0, 0xffffffffu);
            vehicle_set_mode(U32(U32(d, 0x74), 0x1ac), 2);
            rx_addr pad = U32(U32(d, 0x74), 0x1b4);
            RACE_CORE_CALL(1, rx_world_rider_end_pose_override(pad));
            SU32(pad, 0x5bb60, 0);
            for (uint32_t i = 0; i < U32(d, 0xee0); i++) behavior_reset(U32(d, 0xe8 + 4 * i));
        }
        return 1;
    }
    if (U32(0, 0xa89d00) == 0 && debug_key_pressed(0x00d5c770) != 0) {
        rx_addr act = U32(U32(d, 0x74), 0x94);
        const int32_t n = I32(act, 0x26c);
        SU32(0, 0xa89d00, 1);
        int32_t me = 0;
        while (me < n && U32(U32(act, 0x268), 4 * (uint32_t)me) != d) me++;
        rx_addr target;
        if (U32(0, 0xae1b90) != 0) {
            if (n == me + 1) {
                rx_addr g;
                RACE_CORE_CALL(0, g = rx_world_active_sound_table(U32(0, 0xa946b8)));
                rx_addr list = U32(g, 0x21da4);
                const int32_t c = I32(g, 0x21da8);
                int32_t k = 0;
                while (k < c && U32(list, 4 * (uint32_t)k) != act) k++;
                rx_addr a2;
                do { k = (k + 1) % c; a2 = U32(list, 4 * (uint32_t)k); } while (U32(a2, 0x26c) == 0);
                target = U32(U32(a2, 0x268), 0);
            } else {
                target = U32(U32(act, 0x268), 4 * (uint32_t)(me + 1));
            }
        } else {
            int32_t k = me;
            do { k = (k + 1) % n; target = U32(U32(act, 0x268), 4 * (uint32_t)k); } while (U32(U32(target, 0x74), 0x1e4) != 0);
        }
        rx_addr a = U32(U32(d, 0x74), 0x94);
        race_thiscall(race_vfn(a, 45), a, 2, d, target, 0, 0);
        return 1;
    }
    if (U32(0, 0xae1b90) != 0 && U32(0, 0xa89d00) == 0 && debug_key_pressed(0x00d5ccb0) != 0) {
        rx_addr p = U32(d, 0x74);
        SU32(0, 0xa89d00, 1);
        SU32(p, 0x8f4, (uint32_t)((I32(p, 0x8f4) + 1) % 4));
        return 0;
    }
    SU32(0, 0xa89d00, 0);
    return 0;
}

/* 00757f40 (stdcall player; RET 4): the debug "free camera" toggle: MXGame +0x15b64 flips; switched on, the player's name
 * (+0x14, a C string) is copied to +0x15b68 (the camera's owner); switched off, that name is cleared. */
void rx_game_debug_camera_toggle(rx_addr player) {
    const rx_addr g = U32(0, 0xa946b8);
    const uint32_t on = U32(g, 0x15b64) == 0;
    SU32(g, 0x15b64, on);
    if (on == 0) { SU8(g, 0x15b68, 0); return; }
    for (uint32_t i = 0;; i++) {
        const uint8_t c = U8(player, 0x14 + i);
        SU8(g, 0x15b68 + i, c);
        if (c == 0) break;
    }
}
RX_FUNCTION(0x00757f40, rx_game_debug_camera_toggle)

/* The pad's debug view keys during a session: the "free camera" key (0xd5bd98) or one of 12 camera keys (5089a0) -> activity
 * vf14 (driver, camera). */
static void camera_keys(rx_addr d) {
    static const uint32_t maps[12] = {0x00d5b468, 0x00d5c2d8, 0x00d5c578, 0x00d5f0e0, 0x00d5d730, 0x00d5f380,
                                      0x00d5b060, 0x00d5ddd8, 0x00d60538, 0x00d5bc48, 0x00d5ec48, 0x00d5ce00};
    uint32_t cam = 0;
    int hit = 0;
    if (U8(0, 0x9b716d) == 0 && key_pressed(0x00d5bd98) != 0) {
        RACE_CORE_CALL(1, rx_game_debug_camera_toggle(U32(d, 0x74)));
        cam = 0; hit = 1;
    } else {
        for (uint32_t i = 0; i < 12 && !hit; i++)
            if (rx_debug_keymap_get(maps[i], 7, 0) != 0) { cam = i; hit = 1; }
    }
    if (!hit) return;
    rx_addr a = U32(U32(d, 0x74), 0x94);
    race_thiscall(race_vfn(a, 14), a, 2, d, cam, 0, 0);
}

/* 00509c20 (EAX = driver): the AI's wreck: every control output neutral. */
void rx_ai_wreck_controls_off(rx_addr d) {
    SF32(d, 0x60, 0.0f); SF32(d, 0x64, 0.0f); SF32(d, 0x68, 0.0f);
    SF32(d, 0x3c0, 0.0f); SF32(d, 0x3cc, 0.0f); SF32(d, 0x3c4, 0.0f); SF32(d, 0x3c8, 0.0f);
    SF32(d, 0x3dc, 0.0f); SF32(d, 0x3e0, 0.0f); SF32(d, 0x3d4, 0.0f); SF32(d, 0x3d8, 0.0f);
    SU32(d, 0x3d0, 0);
}
RX_FUNCTION(0x00509c20, rx_ai_wreck_controls_off)

static void rx_base_driver_body(rx_addr d, float dt);
/* 00509370 BaseDriver::vf7 (ECX = driver, arg = dt; RET 4): the base update of every driver: debug keys (debug flag 0xae1b90,
 * or a finished session (+0xfe8) for the local player while the activity is not ending), the camera keys while a pad is
 * connected, the driver clock (+0x130) and the race clock (+0x134, state 2, not finished), the player's update (0054af90); for an
 * AI driver: a wreck hook (00509c20), the control outputs clamped (axes -1..1, pedals 0..1) and handed to the vehicle (0045f530). */
void rx_base_driver_update(rx_addr d, float dt) {
    if (U32(d, 0xeec) == 0) return;
    RACE_ORIG_FRAME_ENTER(SF, 0, 4);   /* the original's frame: the player update's callees keep stack addresses */
    rx_base_driver_body(d, dt);
    RACE_ORIG_FRAME_LEAVE(SF);
}
static void rx_base_driver_body(rx_addr d, float dt) {
    const int dbg = U32(0, 0xae1b90) != 0;
    rx_addr player = U32(d, 0x74);
    if ((dbg || U32(U32(player, 0x94), 0xfe8) != 0) && U32(player, 8) == 0 && U32(U32(player, 0x94), 0x238) != 4) {
        if (debug_keys(d, dbg)) return;
    }
    player = U32(d, 0x74);
    if (U32(U32(player, 0x94), 0x21c) != 0 && I32(player, 0x1f0) > -1 && (controller_connected(U32(player, 0x1f0)) & 0xff) != 0)
        camera_keys(d);
    SF32(d, 0x130, dt + F32(d, 0x130));
    if (U32(U32(U32(d, 0x74), 0x94), 0x21c) == 2 && U32(d, 0x70) == 0) SF32(d, 0x134, dt + F32(d, 0x134));
    if (U32(0, 0xae1b90) != 0) {
        RACE_CORE_CALL(0, rx_ai_debug_overlay(d));
    }
    player_update(U32(d, 0x74), dt);
    player = U32(d, 0x74);
    if (U32(player, 0x10) != 1) return;
    if (U32(U32(player, 0x1ac), 0x280) != 0) RACE_CORE_CALL(0, rx_ai_wreck_controls_off(d));
    SF32(d, 0x3dc, clamp_axis(F32(d, 0x3dc)));
    SF32(d, 0x3e0, clamp_axis(F32(d, 0x3e0)));
    SF32(d, 0x3d4, clamp_axis(F32(d, 0x3d4)));
    SF32(d, 0x3d8, clamp_axis(F32(d, 0x3d8)));
    SF32(d, 0x3c4, clamp_unit(F32(d, 0x3c4)));
    SF32(d, 0x3c8, clamp_unit(F32(d, 0x3c8)));
    SF32(d, 0x3c0, clamp_unit(F32(d, 0x3c0)));
    SF32(d, 0x3cc, clamp_unit(F32(d, 0x3cc)));
    if (U32(0, 0xae1b90) != 0) {
        rx_addr dbgobj = U32(U32(U32(d, 0x74), 0x1ac), 0x234);
        race_thiscall(race_vfn(dbgobj, 19), dbgobj, 0, 0, 0, 0, 0);   /* returns on the x87 stack (popped: the adapter) */
    }
    vehicle_set_controls(U32(U32(d, 0x74), 0x1ac), d + 0x3c0);
}
RX_FUNCTION(0x00509370, rx_base_driver_update)

static void rx_network_driver_body(rx_addr d, float dt);
/* 0050e510 NetworkDriver::vf7 (ECX = driver, arg = dt; RET 4): the base update, then (stats system idle: 0xda6cb0 +0x1c3c) the
 * network session object (+0xef0): its updates, results and remote wrecks; offline it only picks up the player's object. */
void rx_network_driver_update(rx_addr d, float dt) {
    RACE_ORIG_FRAME_ENTER(SF, 0xc4, 3);
    RACE_CORE_CALL(1, rx_base_driver_update(d, dt));
    rx_network_driver_body(d, dt);
    RACE_ORIG_FRAME_LEAVE(SF);
}
static void rx_network_driver_body(rx_addr d, float dt) {
    race_static_dcf304();
    if (U32(U32(0, 0xda6cb0), 0x1c3c) != 0) return;
    race_static_dcf304();
    rx_addr scope = race_profile_begin();
    rx_addr net = U32(d, 0xef0);
    if (net != 0 && U32(U32(d, 0x74), 0x1ac) != 0 && (U8(net, 0x20) & 1) != 0) {
        if (U32(0, 0xa942b4) == 2) {
            rx_addr pad = U32(U32(d, 0x74), 0x1b4);
            if (pad != 0) SU32(pad, 0xaf04, 1);
        } else if (net_valid(net)) {
            if (U32(U32(U32(d, 0xef0), 0x1d4), 0) == 0) RACE_CORE_CALL(1, rx_net_bind_player(U32(d, 0xef0), U32(d, 0x74)));
            rx_addr player = U32(d, 0x74);
            if (U32(U32(player, 0x94), 0x21c) != 5 && U32(player, 0x908) == 0) {
                RACE_CORE_CALL(2, rx_net_player_update(U32(d, 0xef0), dt));
            }
            player = U32(d, 0x74);
            rx_addr pad = U32(player, 0x1b4);
            if (pad != 0 && U32(player, 0x1e4) == 0) SU32(pad, 0xaf04, 1);
        } else {
            RACE_FRAME16_ENTER(F, 0xb0);
            uint32_t moved;
            RACE_CORE_CALL(0, (void)rx_net_snapshot_init(F));
            RACE_CORE_CALL(0, rx_net_stamp(U32(d, 0xef0)));
            RACE_CORE_CALL(0, moved = rx_net_player_snapshot(U32(d, 0xef0), F));
            if (moved != 0) {
                SU32(d, 0xf00, 1);
                if (U32(d, 0xef8) == 0 && U32(U32(U32(d, 0x74), 0x94), 0x21c) != 5) {
                    RACE_CORE_CALL(1, rx_net_player_reset(d, F));
                }
                const uint32_t wreck = U32(F, 0x74);
                const uint32_t had = U32(d, 0xef8);
                if (had == 0) {
                    rx_addr v = U32(U32(d, 0x74), 0x1ac);
                    if (wreck != 0 && U32(v, 0x280) == 0) {
                        SU32(v, 0x280, wreck);
                        SU32(U32(v, 0x234), 0x2374, 1);
                        rx_addr pad = U32(U32(d, 0x74), 0x1b4);
                        if (pad != 0) {
                            RACE_CORE_CALL(2, rx_world_rider_chains_fade_out(pad, 4, 0.25f /* 0x92ff84 */, 0));
                            RACE_CORE_CALL(2, rx_world_rider_end_wreck(pad, U32(F, 0x78)));
                        }
                        SU32(d, 0xef8, 1);
                    }
                } else if (wreck == 0 && U32(U32(U32(d, 0x74), 0x1ac), 0x280) == 0) {
                    SU32(d, 0xef8, 0);
                }
            }
            RACE_FRAME16_LEAVE(F);
        }
    } else if (net == 0) {
        rx_addr obj = U32(U32(d, 0x74), 0xb2c);
        if (obj != 0) {
            SU32(d, 0xef0, U32(obj, 0x1d8));
            if (U32(d, 0xef0) != 0 && net_valid(U32(d, 0xef0))) {
                RACE_CORE_CALL(1, rx_net_bind_player(U32(d, 0xef0), U32(d, 0x74)));
            }
        }
    }
    race_profile_end(scope);
}
RX_FUNCTION(0x0050e510, rx_network_driver_update)

/* ---- RaceDriver --------------------------------------------------------------------------------------------------------------- */

/* 00511760 (EAX = driver): the AI's controls on the start line (driver +0xf10 start stage): all outputs neutral, brakes held;
 * stage 2 blips: after +0x1104 it toggles +0x1108 at random intervals (0..1 s, +0x110c) and drops the clutch while off; stages 3
 * and 4 launch once the clock passes +0x10f8 (throttle 1, brakes released, button +0x3d0); stage 5 goes to 6. */
void rx_ai_start_line_controls(rx_addr d) {
    SF32(d, 0x60, 0.0f); SF32(d, 0x64, 0.0f); SF32(d, 0x68, 0.0f);
    SF32(d, 0x3c0, 0.0f); SF32(d, 0x3cc, 0.0f); SF32(d, 0x3dc, 0.0f); SF32(d, 0x3e0, 0.0f); SF32(d, 0x3d4, 0.0f); SF32(d, 0x3d8, 0.0f);
    SU32(d, 0x3d0, 0);
    SF32(d, 0x3c4, 1.0f /* 0x931f24 */); SF32(d, 0x3c8, 1.0f);
    switch (U32(d, 0xf10)) {
    case 2: {
        const float clock = F32(d, 0x130);
        if (!(clock > F32(d, 0x1104))) return;
        if (F32(d, 0x110c) > clock) {
            if (U32(d, 0x1108) != 0) return;
            break;   /* launch */
        }
        SF32(d, 0x110c, (float)race_rand() * 3.0518509e-05f /* 0x92e054: 1 / 32767 */ + F32(d, 0x130));
        SU32(d, 0x1108, U32(d, 0x1108) == 0);
        return;
    }
    case 3: case 4:
        if (!(F32(d, 0x130) > F32(d, 0x10f8))) return;
        break;
    case 5:
        SU32(d, 0xf10, 6);
        return;
    default:
        return;
    }
    SU32(d, 0x3d0, 1);
    SF32(d, 0x3c0, 1.0f);
    SF32(d, 0x3c4, 0.0f);
    SF32(d, 0x3c8, 0.0f);
}
RX_FUNCTION(0x00511760, rx_ai_start_line_controls)

/* 0050fef0 (ESI = driver): the AI's start (driver +0x138 mode 0): once the clock passes the reaction time (+0x13c) full throttle,
 * the button (+0x3d0: launch / holeshot) with the probability of AI parameter dd20 (in percent, the game's LCG), for bikes and ATVs
 * (class 0 / 1 with a +0x198) a random hold time in the parameter range dd70 (+0x1100, CRT rand), then racing (mode 2, +0xf10 0);
 * before it, the start line controls (00511760). */
void rx_ai_race_start(rx_addr d) {
    if (!(F32(d, 0x130) > F32(d, 0x13c))) { rx_ai_start_line_controls(d); return; }
    SF32(d, 0x3c0, 1.0f /* 0x931f24 */);
    const float roll = (float)((int32_t)race_rand() % 100);
    const float chance = rx_ai_param_dd20(d);
    SU32(d, 0x3d0, (chance >= roll && U32(U32(U32(d, 0x74), 0x1ac), 0x198) != 0) ? 1 : 0);
    SF32(d, 0x1100, 0.0f);
    rx_addr v = U32(U32(d, 0x74), 0x1ac);
    const uint32_t cls = U32(v, 0x194);
    if ((cls == 1 || cls == 0) && U32(v, 0x198) != 0) {
        RX_FRAME_ENTER(F, 8);
        rx_ai_param_range_dd70(d, F);
        const int32_t r = race_crt_rand();
        const float lo = F32(F, 0), hi = F32(F, 4);
        RX_FRAME_LEAVE(F, 8);
        SF32(d, 0x1100, ((float)r * 3.0518509e-05f /* 0x92e054 */ * (hi - lo) + F32(d, 0x130)) + lo);
    }
    SU32(d, 0x138, 2);
    SU32(d, 0xf10, 0);
}
RX_FUNCTION(0x0050fef0, rx_ai_race_start)

/* 0050fd30 RaceDriver::vf6 (ECX = driver): entering the countdown (session state 1 / 6 / 7 / 8): an AI driver's outputs go neutral
 * and it draws its reaction time for the start (+0x13c = clock + delay: delay 0..1 s at difficulty tiers 2 / 3, 0..0.75 s in kind
 * 0x13, else 0; for activity kinds 2..6 / 8..0x12 none at all) and tells its lap pacing (00525d60 with 1 - delay); start mode
 * +0x138 = 1 for state 6 (else 0); other states: racing (2). */
void rx_race_driver_countdown(rx_addr d) {
    const uint32_t st = U32(U32(U32(d, 0x74), 0x94), 0x238);
    if (st != 1 && st != 6 && st != 7 && st != 8) { SU32(d, 0x138, 2); return; }
    rx_addr player = U32(d, 0x74);
    if (U32(player, 0x10) != 1) return;
    SF32(d, 0x60, 0.0f); SF32(d, 0x64, 0.0f); SF32(d, 0x68, 0.0f);
    SF32(d, 0x3c0, 0.0f); SF32(d, 0x3cc, 0.0f); SF32(d, 0x3c4, 0.0f); SF32(d, 0x3c8, 0.0f);
    SF32(d, 0x3dc, 0.0f); SF32(d, 0x3e0, 0.0f); SF32(d, 0x3d4, 0.0f); SF32(d, 0x3d8, 0.0f);
    SU32(d, 0x3d0, 0);
    const uint32_t kind = rx_activity_ui_kind(U32(U32(d, 0x74), 0x94));
    float start = 0.0f;
    static const uint8_t has_delay[0x14] = {1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};   /* table 0x50fedc */
    if (kind <= 0x13 && has_delay[kind]) {
        const uint32_t tier = U32(U32(d, 0x74), 0x8f4);
        float delay;
        if (tier == 3 || tier == 2) delay = (float)race_rand() * 3.0518509e-05f /* 0x92e054 */;
        else if (kind == 0x13) delay = (float)race_rand() * 3.0518509e-05f * 0.75f /* 0x935aa8 */;
        else delay = 0.0f;
        SF32(d, 0x13c, delay);
        rx_addr lp = rx_driver_lap_pacing_behavior(d);
        if (lp != 0) {
            RACE_CORE_CALL(1, rx_behavior_lap_pacing_start(lp, 1.0f - F32(d, 0x13c)));
        }
        start = F32(d, 0x130) + F32(d, 0x13c);
    }
    SF32(d, 0x13c, start);
    SU32(d, 0x138, U32(U32(U32(d, 0x74), 0x94), 0x238) == 6);
}
RX_FUNCTION(0x0050fd30, rx_race_driver_countdown)

/* The driver's distance to the finish line along its progress node (the inlined 00512620). */
static void update_distance_to_line(rx_addr d) { SF32(d, 0x10cc, rx_driver_distance_to_line(d)); }

/* 0050fac0 (ESI = driver): the AI's delayed start ends: the vehicle's effect object's vf22, its +0x54 cleared, racing mode (+0x138
 * = 2). */
void rx_ai_delayed_start(rx_addr d) {
    rx_addr c = U32(U32(U32(d, 0x74), 0x1ac), 0x234);
    typedef void (*fn_t)(rx_addr);
    rx_addr fn = race_vfn(c, 22);
    fn_t f = RX_FN(fn, fn_t);
    if (f) RACE_CORE_CALL(0, f(c)); else race_thiscall(fn, c, 0, 0, 0, 0, 0);
    c = U32(U32(U32(d, 0x74), 0x1ac), 0x234);
    SU32(c, 0x54, 0);
    SU32(d, 0x138, 2);
}
RX_FUNCTION(0x0050fac0, rx_ai_delayed_start)
static void ai_race(rx_addr d, float dt) {
    RACE_CORE_CALL(2, rx_ai_race_update(d, dt));
}

/* 0050f620 RaceDriver::vf7 (ECX = driver, arg = dt; RET 4): by the race state: Pre / Post: no distance; Countdown: the AI's start
 * line controls and the off-track rule reset; WindDown: the AI keeps racing; InProgress (and others): the lap time (+0xf20, until
 * finished or +0x10f4), the distance to the line and, for the human, 00511430 (the rider's on-track help), for an AI the start
 * mode (+0x138: 0 reaction start, 1 delayed start, 2 racing); the pad rumble stops while nothing drives it. Then the network
 * driver's update and the network race results. Remote players (+0x908) only get the network update. */
void rx_race_driver_update(rx_addr d, float dt) {
    RACE_ORIG_FRAME_ENTER(SF, 0xc4, 3);   /* the racing AI's callees keep stack addresses (AirLeveling's landing prediction) */
    race_static_dcf304();
    rx_addr scope = race_profile_begin();
    rx_addr player = U32(d, 0x74);
    if (U32(player, 0x908) != 0) {
        RACE_CORE_CALL(1, rx_network_driver_update(d, dt));
        race_profile_end(scope);
        RACE_ORIG_FRAME_LEAVE(SF);
        return;
    }
    const uint32_t kind = U32(player, 0x10);
    switch (U32(U32(player, 0x94), 0x21c)) {
    case 0: case 4:
        SF32(d, 0x10cc, 0.0f);
        break;
    case 3:
        if (kind == 1) ai_race(d, dt);
        SF32(d, 0x10cc, 0.0f);
        break;
    case 1:
        if (kind == 1) rx_ai_start_line_controls(d);
        { rx_addr pen = rx_driver_offtrack_penalty_rule(d); if (pen != 0) rx_rule_offtrack_reset_state(pen); }
        break;
    case 2:
        if (U32(d, 0x70) == 0 && U32(d, 0x10f4) == 0) {
            SF32(d, 0xf20, dt + F32(d, 0xf20));
            rx_lap_log_clock(d, dt);   /* PORT ADDITION (game/practice.h): the lap log's frame step (a host static) */
        }
        /* fall through */
    default:
        if (kind == 0) {
            update_distance_to_line(d);
            /* the original's frame of 00511430 below this one (for its uninitialized-stack read) */
            RACE_CORE_CALL(2, rx_human_driver_rules(d, dt, (((SF - 12) - 4) & ~15u) - 0x34 - 0xc));
            player = U32(d, 0x74);
            if (U32(player, 4) != 0 && U32(player, 0x10) == 0) {
                if (U32(U32(player, 0x1b4), 0xaee0) == 1 && stick_moved(U32(U32(d, 0x74), 0x1f0)))
                    rumble_stop(U32(U32(d, 0x74), 0x1b4));
            } else if (U32(U32(player, 0x1b4), 0xaee0) == 1) {
                rumble_stop(U32(player, 0x1b4));
            }
        } else if (kind == 1) {
            update_distance_to_line(d);
            if (U32(player, 4) == 0) {
                if (U32(U32(player, 0x1b4), 0xaee0) == 1) rumble_stop(U32(player, 0x1b4));
            } else {
                switch (U32(d, 0x138)) {
                case 0: rx_ai_race_start(d); break;
                case 1: if (F32(d, 0x130) > F32(d, 0x13c)) RACE_CORE_CALL(0, rx_ai_delayed_start(d)); break;
                case 2:
                    ai_race(d, dt);
                    if (U32(U32(U32(d, 0x74), 0x1b4), 0xaee0) == 1 && !(F32(d, 0x3c0) == 0.0f)) rumble_stop(U32(U32(d, 0x74), 0x1b4));
                    break;
                default: break;
                }
            }
        }
        break;
    }
    RACE_CORE_CALL(1, rx_network_driver_update(d, dt));
    race_static_dcf304();
    rx_addr net = U32(d, 0xef0);
    if (U32(U32(0, 0xda6cb0), 0x1c3c) == 0 && net != 0 && (U8(net, 0x20) & 1) != 0 && !net_valid(net)) {
        RACE_FRAME16_ENTER(F, 0xa0);
        RACE_CORE_CALL(0, (void)rx_net_snapshot_init(F));
        RACE_CORE_CALL(0, rx_net_snapshot_controls(F, U32(d, 0xef0)));
        if (!(F32(F, 0x80) == 0.0f)) SF32(d, 0x10c4, F32(F, 0x80));
        SF32(d, 0x10b4, F32(F, 0x84));
        SF32(d, 0x10cc, F32(F, 0x8c));
        SF32(d, 0x10c8, F32(F, 0x88));
        SU32(d, 0xf1c, U32(F, 0x90));
        SF32(d, 0x10d8, F32(F, 0x7c));
        RACE_FRAME16_LEAVE(F);
    }
    race_profile_end(scope);
    RACE_ORIG_FRAME_LEAVE(SF);
}
RX_FUNCTION(0x0050f620, rx_race_driver_update)

/* ---- FreeRideDriver ----------------------------------------------------------------------------------------------------------- */

/* 0050bfa0 (arg = driver; RET 4): the free ride human's rules, updated with dt 0 (while the network object, if any, is valid). */
void rx_free_ride_rules_update(rx_addr d) {
    RACE_FRAME_PLAIN(F, 0, 4);   /* push ebx / ebp / esi / edi (0050bfa0) */
    race_static_dcf304();
    rx_addr scope = race_profile_begin();
    if (U32(d, 0xef0) == 0 || net_valid(U32(d, 0xef0)))
        for (uint32_t i = 0; i < U32(d, 0xee4); i++) rule_update(U32(d, 0xa0 + 4 * i), 0.0f);
    race_profile_end(scope);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0050bfa0, rx_free_ride_rules_update)

/* the behaviour vf4 (dt) / vf8 (debug draw) dispatches (the ports are the behaviours' registered functions). */
static uint32_t ai_behavior_update(rx_addr b, float dt) {
    typedef uint32_t (*fn_t)(rx_addr, float);
    rx_addr fn = race_vfn(b, 4);
    fn_t f = RX_FN(fn, fn_t);
    uint32_t r;
    if (f) RACE_CORE_CALL(1, r = f(b, dt)); else r = race_thiscall(fn, b, 1, rx_fb(dt), 0, 0, 0);
    return r;
}
static void ai_behavior_draw(rx_addr b) { race_thiscall(race_vfn(b, 8), b, 0, 0, 0, 0, 0); }
/* rule vf5 with the original's stack depth (the rules' ports keep stack addresses). */
static void ai_rule_update(rx_addr r, float dt) {
    typedef void (*fn_t)(rx_addr, float);
    rx_addr fn = race_vfn(r, 5);
    fn_t f = RX_FN(fn, fn_t);
    if (f) RACE_CORE_CALL(1, f(r, dt)); else race_thiscall(fn, r, 1, rx_fb(dt), 0, 0, 0);
}

/* 0050c070 (EAX = driver, arg = dt; RET 4): the FreeRide AI's frame. With a network object, only while it is valid: the outputs
 * neutral, every rule vf5(0), then the behaviours in priority order (vf4; the first that claims the frame stops the run; the ones
 * after it are only given their debug vf8), the steering sum clamped to [-1, 1] (+0x3dc), the throttle scale (+0x3d4) from the
 * node's per-class speed (+0x468) or a half, the lean bias +0x318 handed to +0x3e0 / +0x3d8 when slow enough for the class
 * (2 / 4), and the air leveling override (0053a510: its steering / lean, its reset flag, the skill table 0x9e4478's lean gain);
 * then 0050c7f0 (the free-roam timer) and the debug 0050ab00. */
void rx_free_ride_ai_update(rx_addr d, float dt) {
    race_static_dcf304();
    RACE_FRAME_EBP(F, 8, 0xc, 3);
    rx_addr scope = race_profile_begin();
    if (U32(d, 0xef0) == 0 || net_valid(U32(d, 0xef0))) {
        SF32(d, 0x60, 0.0f); SF32(d, 0x64, 0.0f); SF32(d, 0x68, 0.0f);
        SF32(d, 0x3c0, 1.0f /* 0x931f24 */); SF32(d, 0x3c4, 0.0f); SF32(d, 0x3c8, 0.0f);
        SF32(d, 0x3e0, 0.0f); SF32(d, 0x3d8, 0.0f);
        SU32(d, 0x3d0, 0);
        for (uint32_t i = 0; i < U32(d, 0xee4); i++) ai_rule_update(U32(d, 0xa0 + 4 * i), 0.0f);
        const int dbg = U32(U32(d, 0x74), 0x1cc) != 0 && U32(0, 0xae1b90) != 0;
        if (dbg) RACE_CORE_CALL(0, rx_world_shared_vf_empty());
        uint32_t i = 0;
        for (; i < U32(d, 0xee0); i++) {
            rx_addr b = U32(d, 0xe8 + 4 * i);
            const uint32_t claimed = ai_behavior_update(b, dt);
            if (dbg) ai_behavior_draw(b);
            if (claimed != 0) { i++; break; }
        }
        for (; i < U32(d, 0xee0); i++) if (dbg) ai_behavior_draw(U32(d, 0xe8 + 4 * i));
        SF32(d, 0x3dc, clamp_axis(F32(d, 0x60)));
        if (vehicle_airborne_race(U32(U32(d, 0x74), 0x1ac)) == 0) {
            const rx_addr node = U32(d, 0x18);
            float t = 0.0f;
            if (node != 0) {
                const rx_addr v = U32(U32(d, 0x74), 0x1ac);
                t = F32(node, 0x468 + 4 * (U32(v, 0x198) + 4 * U32(v, 0x194)));
            }
            SF32(d, 0x3d4, t != 0.0f ? rx_muls(rx_muls(t, 0.01f /* 0x9322ac */), F32(d, 0x3dc))
                                     : rx_muls(F32(d, 0x3dc), 0.5f /* 0x9322cc */));
        }
        {   /* the lean bias +0x318 */
            const rx_addr v = U32(U32(d, 0x74), 0x1ac);
            const float lean = F32(d, 0x318);
            if (lean != 0.0f) {
                const float a = fabsf(F32(d, 0x3dc));
                if (0.1f /* 0x931d94 */ > a || (U32(v, 0x194) != 2 && U32(v, 0x194) != 4)) {
                    if (F32(v, 0x150) > F32(0, 0x9cd954) && U32(v, 0x280) == 0) { SF32(d, 0x3e0, lean); SF32(d, 0x3d8, lean); }
                } else { SF32(d, 0x3e0, 0.0f); SF32(d, 0x3d8, 0.0f); }
            }
        }
        {   /* the air leveling behaviour's override */
            const rx_addr b = rx_driver_air_leveling_behavior(d);
            float store = 0.0f;
            int have = 0;
            if (b != 0 && F32(d, 0x318) == 0.0f) {
                if (F32(b, 0x58) > F32(d, 0x130)) {
                    SF32(d, 0x3e0, F32(0, 0x9cd9e8) /* -1 */);
                    SF32(d, 0x3c4, 0.0f); SF32(d, 0x3c8, 0.0f);
                    store = F32(0, 0x9cd9e8); have = 1;
                } else if (F32(d, 0x318) == 0.0f) {
                    SF32(d, 0x3e0, rx_adds(F32(b, 0x40), F32(d, 0x3e0)));
                    SF32(d, 0x3dc, rx_adds(F32(b, 0x44), F32(d, 0x3dc)));
                    if (U32(b, 0x4c) != 0) { SF32(d, 0x3dc, 0.0f); SF32(d, 0x3d4, 0.0f); }
                    if (vehicle_airborne_race(U32(U32(d, 0x74), 0x1ac)) == 0) {
                        float a = fabsf(F32(d, 0x3dc));
                        if (!(1.0f /* 0x931f24 */ > a)) a = 1.0f;
                        const uint32_t idx = U32(U32(d, 0x74), 0x8f4) + 4 * (3 * U32(d, 0x328) + U32(d, 0x32c));
                        const float g = F32(0, 0x9e4478 + 4 * idx);
                        SF32(d, 0x3e0, rx_adds(rx_muls(g, a), F32(d, 0x3e0)));
                        store = rx_adds(rx_muls(g, a), F32(d, 0x3d8));
                        have = 1;
                    }
                }
            }
            if (have) SF32(d, 0x3d8, store);
        }
        RACE_CORE_CALL(1, rx_free_ride_steer(d));       /* the free-roam steering helper */
        RACE_CORE_CALL(0, rx_free_ride_timer(d));       /* the free-roam hint timer */
        if (dbg) RACE_CORE_CALL(0, rx_free_ride_debug_draw(d));
    }
    race_profile_end(scope);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0050c070, rx_free_ride_ai_update)

/* 0050c500 (stdcall driver; RET 4): the free-roam steering helper. The roam behaviour (0053a560, type 0xe): with an override node
 * (+0x60) its per-class speed limit (+0xe8[k]) drives throttle / brake: below it throttle +0.5; above it the distance from the
 * segment (005389c0) over the node's length times the over-speed into +0x3c4 / +0x3c8 and throttle -0.5. Without one: the node's
 * speed limit, and when over it while on the ground, throttle 0 with full brake. Then the override node's flags: 0x20 the button
 * (+0x3d0), a positive +0x168 sets the AI channel +0x2f0, a positive +0x2e8 starts a lean recovery (0050b590), flag 8 the steering
 * bias +0x318 from table 0x9e43b8. */
void rx_free_ride_steer(rx_addr d) {
    RACE_ORIG_FRAME_ENTER(F, 0x24, 3);
    rx_addr beh;
    RACE_CORE_CALL(1, beh = rx_driver_roam_behavior(d));
    const rx_addr node = U32(beh, 0x34);
    if (node != 0) {
        rx_addr player = U32(d, 0x74);
        rx_addr v = U32(player, 0x1ac);
        const uint32_t k = U32(v, 0x198) + 4 * U32(v, 0x194);
        rx_addr over = U32(node, 0x60);
        if (over != 0) {
            const float lim = F32(over, 0xe8 + 4 * k);
            if (lim != F32(0, 0x9cd9e8) /* -1 */) {
                const float speed = F32(v, 0x150);
                if (lim > speed) {
                    SF32(d, 0x3c0, rx_adds(F32(d, 0x3c0), 0.5f /* 0x9322cc */));
                } else if (speed > lim) {
                    const float ov = rx_subs(speed, lim);
                    RACE_CORE_CALL(1, rx_track_project_clamped_xz(over, U32(over, 0x64), v + 0xc0, F + 0x20));
                    const float dx = rx_subs(F32(F, 0x20), F32(v, 0xc0));
                    const float dy = rx_subs(F32(F, 0x24), F32(v, 0xc4));
                    const float dz = rx_subs(F32(F, 0x28), F32(v, 0xc8));
                    const float sum = rx_adds(rx_adds(rx_muls(dz, dz), rx_muls(dy, dy)), rx_muls(dx, dx));
                    float len;
                    RACE_CORE_CALL(1, len = phys_sqrtf(sum));
                    const float r = rx_divs(len, F32(over, 0x4f0));
                    const float b = rx_adds(rx_divs(rx_muls(r, ov), ov), F32(d, 0x3c4));
                    SF32(d, 0x3c4, b);
                    SF32(d, 0x3c8, b);
                    SF32(d, 0x3c0, rx_subs(F32(d, 0x3c0), 0.5f));
                }
            }
        } else {
            const float lim = F32(node, 0xe8 + 4 * k);
            if (lim != F32(0, 0x9cd9e8) /* -1 */ && F32(v, 0x150) > lim) {
                if (vehicle_airborne_race(v) == 0) {
                    SF32(d, 0x3c0, 0.0f);
                    SF32(d, 0x3c8, 1.0f /* 0x931f24 */);
                    SF32(d, 0x3c4, 1.0f);
                }
            }
        }
        over = U32(node, 0x60);
        if (over != 0) {
            v = U32(U32(d, 0x74), 0x1ac);
            const uint32_t k2 = U32(v, 0x198) + 4 * U32(v, 0x194);
            if ((U8(over, 0x68 + 4 * k2) & 0x20) != 0 && U32(d, 0x30c) == 0) {
                SU32(d, 0x3d0, 1);
                SU32(d, 0x30c, 1);
            }
            if (F32(over, 0x168 + 4 * k2) > 0.0f && U32(d, 0x2f0) == 0) SU32(d, 0x2f0, 1);
            if (F32(over, 0x2e8 + 4 * k2) > 0.0f && U32(d, 0x2f4) == 0) {
                RACE_CORE_CALL(0, rx_ai_start_lean(d, over + 0x2e8));
                SU32(d, 0x2f4, 1);
                RACE_FRAME_END(F);
                return;
            }
            if ((U8(over, 0x68 + 4 * k2) & 8) != 0 && U32(d, 0x31c) == 0) {
                const uint32_t idx = U32(U32(d, 0x74), 0x8f4) + 4 * (3 * U32(d, 0x328) + U32(d, 0x32c));
                SF32(d, 0x318, rx_subs(-0.0f /* 0x9774a8 */, F32(0, 0x9e43b8 + 4 * idx)));
                SU32(d, 0x31c, 1);
            }
        }
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0050c500, rx_free_ride_steer)

/* 0051de00 (EAX = driver) -> xmm0: the AI parameter at block 0xa89ff8 (the lean trigger's chance), by personality / difficulty. */
float rx_ai_param_de00(rx_addr driver) {
    uint32_t i; int e;
    return race_ai_param(driver, 0x00a89ff8, &i, &e);
}
RX_FUNCTION(0x0051de00, rx_ai_param_de00)

/* 0050ad00 (ESI = driver): start the lean recovery: the vehicle's flat heading (the first row of its world matrix, x / z; its own
 * frame's uninitialized slots at +0x10 / +0x14 / +0x18 when that length is zero) against the vehicle's axis at +0x140; the dot's
 * sign is the side (+0x2fc) and the initial lean (+0x3d4 = +1 / -1), state +0x2f8 = 1. */
void rx_ai_lean_start(rx_addr d, rx_addr orig_frame) {
    RACE_ORIG_FRAME_ENTER(F, 0x60, 0);
    rx_addr v = U32(U32(d, 0x74), 0x1ac);
    typedef void (*fn_t)(rx_addr, rx_addr);
    rx_addr fn = race_vfn(v, 15);
    fn_t f = RX_FN(fn, fn_t);
    if (f) RACE_CORE_CALL(1, f(v, F + 0x20)); else race_thiscall(fn, v, 1, F + 0x20, 0, 0, 0);
    const float mx = F32(F, 0x20), mz = F32(F, 0x28);
    float hx, hy, hz;
    const float l2 = rx_adds(rx_muls(mx, mx), rx_muls(mz, mz));
    if (l2 > 0.0f) {
        float s;
        RACE_CORE_CALL(1, s = phys_sqrtf(l2));
        const float inv = rx_divs(1.0f /* 0x931f24 */, s);
        hx = rx_muls(inv, mx);
        hy = rx_muls(inv, 0.0f);
        hz = rx_muls(inv, mz);
    } else {
        hz = F32(orig_frame, 0x18); hy = F32(orig_frame, 0x14); hx = F32(orig_frame, 0x10);
    }
    v = U32(U32(d, 0x74), 0x1ac);
    const float dot = rx_adds(rx_adds(rx_muls(F32(v, 0x148), hz), rx_muls(F32(v, 0x144), hy)), rx_muls(F32(v, 0x140), hx));
    if (dot > 0.0f) {
        SU32(d, 0x2fc, 1); SU32(d, 0x2f8, 1); SF32(d, 0x3d4, 1.0f /* 0x931f24 */);
    } else {
        SU32(d, 0x2fc, 0); SU32(d, 0x2f8, 1); SF32(d, 0x3d4, F32(0, 0x9cd9e8) /* -1 */);
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0050ad00, rx_ai_lean_start)

/* 0050b590 (EAX = driver, EDI = node + 0x2e8; RET 0) -> EAX: start a lean-recovery record (driver +0x214 + 0x14 * +0x2e4:
 * the record pointer, flags, the speed (one cell's threshold above it becomes speed + 1.46666), the node's +0x20 and +0x40
 * per-class values). Flags: 8 when the game random draw (0..99) is above the AI parameter; 0x10 when the hint timer +0x304 is
 * still ahead; 4 while airborne; 1 when under the node's +0x20 speed, 2 when over its +0x40 speed (non-zero). With no flags the
 * lean recovery starts (0050ad00) and the hint timer is clock + 0.4. The ring index +0x2e4 wraps at 10, +0x2e8 saturates at 10.
 * Returns 1 when the record started with no flags. */
uint32_t rx_ai_start_lean(rx_addr d, rx_addr rec) {
    RACE_FRAME_PLAIN(F, 0, 3);   /* push ecx; push ebx; push esi */
    rx_addr v = U32(U32(d, 0x74), 0x1ac);
    float sp = F32(v, 0x150);
    const uint32_t iv = U32(v, 0x198) + 0x20 + 4 * U32(v, 0x194);
    if (sp < F32(rec, 4 * iv)) sp = rx_adds(sp, 1.46666f /* 0x977a94 */);
    const uint32_t idx = U32(d, 0x2e4);
    SU32(d, 0x214 + idx * 0x14, rec);
    SU32(d, 0x218 + idx * 0x14, 0);
    SF32(d, (idx + 0x1b) * 0x14, sp);
    v = U32(U32(d, 0x74), 0x1ac);
    const uint32_t iv2 = U32(v, 0x198) + 0x20 + 4 * U32(v, 0x194);
    SU32(d, 0x220 + idx * 0x14, U32(rec, 4 * iv2));
    v = U32(U32(d, 0x74), 0x1ac);
    const uint32_t iv3 = U32(v, 0x198) + 0x40 + 4 * U32(v, 0x194);
    SU32(d, 0x224 + idx * 0x14, U32(rec, 4 * iv3));
    uint32_t flags = 0;
    const float rnd = (float)((int32_t)race_rand() % 100);
    if (rnd > rx_ai_param_de00(d)) flags |= 8;
    if (F32(d, 0x304) > F32(d, 0x130)) flags |= 0x10;
    v = U32(U32(d, 0x74), 0x1ac);
    if (vehicle_airborne_race(v) != 0) flags |= 4;
    v = U32(U32(d, 0x74), 0x1ac);
    const uint32_t iv20 = U32(v, 0x198) + 0x20 + 4 * U32(v, 0x194);
    const uint32_t iv40 = U32(v, 0x198) + 0x40 + 4 * U32(v, 0x194);
    const float a = F32(rec, 4 * iv20), b = F32(rec, 4 * iv40);
    if (sp < a) flags |= 1;
    else if (sp > b && b != 0.0f) flags |= 2;
    SU32(d, 0x218 + idx * 0x14, flags);
    int started = 0;
    if (flags == 0) {
        const rx_addr orig = ((rx_sp() - 8) & ~15u) - 0x60;
        RACE_CORE_CALL(0, rx_ai_lean_start(d, orig));
        SF32(d, 0x304, rx_adds(F32(d, 0x130), 0.4f /* 0x935860 */));
        started = 1;
    }
    SU32(d, 0x2e4, (U32(d, 0x2e4) + 1) % 10);
    uint32_t e8 = U32(d, 0x2e8) + 1;
    if (e8 >= 10) e8 = 10;
    SU32(d, 0x2e8, e8);
    RACE_FRAME_END(F);
    return (uint32_t)started;
}
RX_FUNCTION(0x0050b590, rx_ai_start_lean)

/* 0050c7f0 (ECX = driver): every 30 s (clock +0x130 past +0xf14, when the debug flag is off), if the timer object (+0xf10) is
 * waiting (+0x44) and the game random draw is above 4 in 10: clear it and start the 1.5 s hint (+0x54, state +0x58 = 2). */
void rx_free_ride_timer(rx_addr d) {
    rx_addr t = U32(d, 0xf10);
    if (t == 0) return;
    if (U32(0, 0xae1b90) != 0) return;
    const float clock = F32(d, 0x130);
    if (!(clock > F32(d, 0xf14))) return;
    SF32(d, 0xf14, rx_adds(clock, 30.0f /* 0x931d98 */));
    if (U32(t, 0x44) == 0) return;
    if ((int32_t)race_rand() % 10 <= 4) return;
    t = U32(d, 0xf10);
    SU32(t, 0x44, 0);
    SF32(t, 0x54, 1.5f /* 0x9322a4 */);
    SU32(t, 0x58, 2);
}
RX_FUNCTION(0x0050c7f0, rx_free_ride_timer)

/* 0050ab00 (ESI = driver): the debug view of the free-roam AI's control outputs: nine calls to the empty print hook (007e3be0)
 * with +0x3dc / +0x3e0 / +0x3d4 / +0x3d8 / +0x3c0 / +0x3cc / +0x3d0 / +0x3c4 / +0x3c8. */
void rx_free_ride_debug_draw(rx_addr d) {
    static const uint32_t offs[9] = {0x3dc, 0x3e0, 0x3d4, 0x3d8, 0x3c0, 0x3cc, 0x3d0, 0x3c4, 0x3c8};
    for (int i = 0; i < 9; i++) {
        (void)U32(d, offs[i]);   /* the original loads each value for the (empty) print hook */
        RACE_CORE_CALL(0, rx_world_shared_vf_empty());
    }
}
RX_FUNCTION(0x0050ab00, rx_free_ride_debug_draw)

/* 0050bf30 FreeRideDriver::vf7 (ECX = driver, arg = dt; RET 4): the human's rules (0050bfa0) or the AI's free roaming (0050c070,
 * EAX = driver), then the network driver's update. */
void rx_free_ride_driver_update(rx_addr d, float dt) {
    RACE_FRAME_EBP(F, 8, 0, 2);   /* push ebp; and esp, -8; push ecx; push esi (0050bf30) */
    rx_addr player = U32(d, 0x74);
    if (U32(player, 0x908) == 0) {
        if (U32(player, 0x10) == 0) RACE_CORE_CALL(1, rx_free_ride_rules_update(d));
        else if (U32(player, 0x10) == 1) RACE_CORE_CALL(1, rx_free_ride_ai_update(d, dt));
    }
    RACE_CORE_CALL(1, rx_network_driver_update(d, dt));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0050bf30, rx_free_ride_driver_update)

/* ---- human driver helpers ----------------------------------------------------------------------------------------------------- */

/* 00511430 (args: driver, dt; RET 8): the human driver's rules (each vf5(dt)), its lap penalty copy (+0x10c8 = the off-track
 * rule's +0x1c) and its "last position" (+0x80): moved forward only while on track, not wrecked, not being reset, riding along
 * the segment (velocity . segment > 0) and ahead of the last position. Off track (or wrecked / reset): the arrow returns after its
 * time (+0x10d0; the rule's +0xd8) and a pending arrow (+0x10d4) raises "Off Track" (driver, on track) and "Show Arrow" (driver).
 * With a zero velocity the original reads its own uninitialized stack for the direction (orig_frame + 0x20 / +0x24 / +0x28). */
static void human_driver_rules(rx_addr d, float dt, rx_addr orig_frame);
void rx_human_driver_rules(rx_addr d, float dt, rx_addr orig_frame) {
    /* the callees (the rules' updates, the events' handlers) run below the original's frame */
    const rx_addr sp_in = rx_sp();
    rx_set_sp(orig_frame);
    human_driver_rules(d, dt, orig_frame);
    rx_set_sp(sp_in);
}
static void human_driver_rules(rx_addr d, float dt, rx_addr orig_frame) {
    if (U32(d, 0xef0) != 0 && !net_valid(U32(d, 0xef0))) return;
    if (U32(U32(d, 0x74), 0x1ac) == 0) return;
    for (uint32_t i = 0; i < U32(d, 0xee4); i++) rule_update(U32(d, 0xa0 + 4 * i), dt);
    SF32(d, 0x60, 0.0f);
    uint32_t on_track = 1;
    rx_addr pen = rx_driver_offtrack_penalty_rule(d);
    int keep = 1;
    if (pen != 0) {
        on_track = U32(pen, 0x10);
        SF32(d, 0x10c8, F32(pen, 0x1c));
        if (on_track == 0) keep = 0;
    }
    rx_addr player = U32(d, 0x74), v = U32(player, 0x1ac);
    if (keep && (U32(v, 0x280) != 0 || F32(player, 0x116c) > 0.0f)) keep = 0;
    if (keep) {
        float nx, ny, nz;
        const float vx = F32(v, 0x160), vy = F32(v, 0x164), vz = F32(v, 0x168);
        const float l2 = (vx * vx + vy * vy) + vz * vz;
        if (l2 > 0.0f) {
            const float inv = 1.0f /* 0x931f24 */ / sqrtf(l2);
            nx = vx * inv; ny = inv * F32(v, 0x164); nz = inv * F32(v, 0x168);
        } else {
            nx = F32(orig_frame, 0x20); ny = F32(orig_frame, 0x24); nz = F32(orig_frame, 0x28);
        }
        rx_addr a = U32(d, 0x18), b = U32(d, 0x1c);
        float sx = F32(b, 0) - F32(a, 0), sy = F32(b, 4) - F32(a, 4), sz = F32(b, 8) - F32(a, 8);
        const float s2 = (sz * sz + sy * sy) + sx * sx;
        if (s2 > 0.0f) {
            const float inv = 1.0f / sqrtf(s2);
            sx = sx * inv; sy = sy * inv; sz = sz * inv;
        }
        if (!(((sz * nz + sy * ny) + sx * nx) > 0.0f)) return;
        if (((F32(v, 0xc8) - F32(d, 0x88)) * sz + (F32(v, 0xc4) - F32(d, 0x84)) * sy) + (F32(v, 0xc0) - F32(d, 0x80)) * sx > 0.0f) {
            SF32(d, 0x80, F32(v, 0xc0)); SF32(d, 0x84, F32(v, 0xc4)); SF32(d, 0x88, F32(v, 0xc8));
        }
        return;
    }
    if (pen != 0 && F32(d, 0x130) > F32(d, 0x10d0)) SU8(pen, 0xd8, 0);
    if (U8(d, 0x10d4) == 0 || U32(U32(U32(d, 0x74), 0x1ac), 0x280) != 0) return;
    RACE_CORE_CALL(1, rx_event_driver_int(U32(U32(U32(d, 0x74), 0x94), 0x290), 0x0093ef24 /* "Off Track" */, d, on_track));
    {   /* "Show Arrow" (driver): the raiser is inlined here (0051172x): the payload at frame + 0x1c, dispatch (EAX = manager,
         * EDI = name; size 4, payload) entered at frame - 12 */
        const rx_addr manager = U32(U32(U32(d, 0x74), 0x94), 0x290);
        SU32(orig_frame, 0x1c, d);
        crt_type_info_name(0x009cda44, 0x009e1a7c);
        RACE_CORE_CALL(2, rx_world_event_dispatch(manager, 0x0093ef30, orig_frame + 0x1c, 4));
    }
    SU8(d, 0x10d4, 0);
}
RX_FUNCTION(0x00511430, rx_human_driver_rules)

/* 00511f00 (EAX = driver): the rider's highlight icon (+0x1114: +0x44 shown, +0x54 time, +0x58 -1): the leader (standings
 * +0x2d0[0]) gets 3 s, the others are shown (not in debug mode or activity kind 7). */
void rx_driver_update_highlight(rx_addr d) {
    rx_addr hl = U32(d, 0x1114);
    if (hl == 0 || U32(0, 0xae1b90) != 0) return;
    if (rx_activity_ui_kind(U32(U32(d, 0x74), 0x94)) == 7) return;
    hl = U32(d, 0x1114);
    if (U32(U32(U32(d, 0x74), 0x94), 0x2d0) == d) { SU32(hl, 0x44, 0); SF32(hl, 0x54, 3.0f /* 0x92fe48 */); }
    else { SU32(hl, 0x44, 1); SF32(hl, 0x54, 0.0f); }
    SU32(hl, 0x58, 0xffffffffu);
}
RX_FUNCTION(0x00511f00, rx_driver_update_highlight)

/* 00511f80 (arg = driver, ESI = out point; RET 4) -> EAX: where the vehicle will be in a second (position + velocity, written to
 * out) and on which side of the track's line there: -1 left / 1 right of the segment found (005387c0 within 100 units of the
 * driver's segment, following sections), 0 off the track. With a zero velocity the original reads its own uninitialized stack
 * (orig_frame + 0x10 / +0x14 / +0x18). */
int rx_driver_predict_side(rx_addr d, rx_addr out, rx_addr orig_frame) {
    rx_addr v = U32(U32(d, 0x74), 0x1ac);
    const float vx = F32(v, 0x160), vy = F32(v, 0x164), vz = F32(v, 0x168);
    const float l2 = (vx * vx + vy * vy) + vz * vz;
    float dx, dy, dz;
    if (l2 > 0.0f) {
        const float inv = 1.0f /* 0x931f24 */ / sqrtf(l2);
        dx = vx * inv; dy = F32(v, 0x164) * inv; dz = F32(v, 0x168) * inv;
    } else {
        dz = F32(orig_frame, 0x18); dy = F32(orig_frame, 0x14); dx = F32(orig_frame, 0x10);
    }
    v = U32(U32(d, 0x74), 0x1ac);
    const float speed = F32(v, 0x150);
    SF32(out, 0, F32(v, 0xc0) + dx * speed);
    SF32(out, 4, F32(v, 0xc4) + dy * speed);
    SF32(out, 8, F32(v, 0xc8) + dz * speed);
    race_track_query q = {out, U32(d, 0x14), U32(d, 0x18), 0, 100.0f /* 0x93005c */, 100.0f, 0, 1};
    rx_addr sec, node;
    if (race_track_find(&q, 0, &sec, &node) == 0) return 0;
    const float side = F32(node, 0x50) * (F32(out, 8) - F32(node, 8)) - F32(node, 0x58) * (F32(out, 0) - F32(node, 0));
    return side > 0.0f ? -1 : 1;
}
RX_FUNCTION(0x00511f80, rx_driver_predict_side)

/* ---- the AI's reactions to the track's node flags ----------------------------------------------------------------------------- */

/* 0050b590 (EAX = driver, EDI = the node's threshold record) -> EAX: start a lean recovery. */
static uint32_t ai_start_lean(rx_addr d, rx_addr rec) {
    uint32_t r;
    RACE_CORE_CALL(0, r = rx_ai_start_lean(d, rec));
    return r;
}

/* 005118c0 (args: driver, dt; RET 8): the AI rider reacting to its track node (behaviour 5's override node, else the segment's
 * first node), per vehicle kind k = vehicle +0x198 + 4 * class +0x194:
 *  - speed limit node +0xe8[k] (-1 none; 10 lower at the top tier in kinds 2 / 0x11): below it throttle +0.5, above it brake in
 *    proportion to the distance into the segment (skill 2 tolerates 15 % over);
 *  - flag 0x20: the button (+0x3d0, jumps / holeshot) once per node (+0x30c), by chance (AI parameter def0, percent) unless the
 *    sub tier +0x8f8 is 2 (always);
 *  - threshold +0x2e8[k] > 0: a lean recovery (0050b590, then 0050ae20 while it runs);
 *  - flag 0x8000: a throttle scale +0x320 (skill table 0x9e3ff8 x parameter de50 %), flag 8: a steering bias +0x318 (table 0x9e43b8
 *    x parameter dea0 %, negated), flag 0x800: full left bias (-1); each once per node (+0x324 / +0x31c);
 *  - flag 0x40: keep to a side (+0x310: the side the node lies on in the vehicle's frame, flipped near the edges: 0050b9e30 > 0.75
 *    / < 0.25);
 *  - flags 0x100 / 0x80 (unless track +0x1e8): force the lane behaviour (3) to the left / right (+0x314, enable it), none: release
 *    it; a lane ends by chance (parameter df40) (+0x330). */
void rx_ai_track_node_reactions(rx_addr d, float dt) {
    RACE_ORIG_FRAME_ENTER(SF, 0x24, 3);   /* locals at SF + 0x20 (a vector), as the original's [esp + 0x20] */
    rx_addr node;
    rx_addr b5 = rx_driver_behavior_5(d);
    if (b5 != 0 && U32(b5, 0x54) != 0) node = U32(b5, 0x3c);
    else node = U32(d, 0x18);
#define KIDX() ({ rx_addr v_ = U32(U32(d, 0x74), 0x1ac); U32(v_, 0x198) + 4 * U32(v_, 0x194); })
    const float limit = F32(node, 0xe8 + 4 * KIDX());
    rx_addr player = U32(d, 0x74);
    if (!(limit == F32(0, 0x9cd9e8)) && vehicle_airborne_race(U32(player, 0x1ac)) == 0) {
        const uint32_t kind = rx_activity_ui_kind(U32(U32(d, 0x74), 0x94));
        float lim = limit;
        if ((kind == 2 || kind == 0x11) && U32(U32(d, 0x74), 0x8f4) == 3) lim = limit - 10.0f /* 0x92fd88 */;
        rx_addr v = U32(U32(d, 0x74), 0x1ac);
        const float speed = F32(v, 0x150);
        if (lim > speed) {
            SF32(d, 0x3c0, F32(d, 0x3c0) + 0.5f /* 0x9322cc */);
        } else if (speed > lim) {
            const float over = speed - lim;
            int brake = 1;
            if (U32(d, 0x32c) == 2) {
                const float base = lim > 1.0f /* 0x931f24 */ ? lim : 1.0f;
                if (!(0.15f /* 0x9322c8 */ < over / base) && !((over / base) != (over / base))) { SF32(d, 0x3c0, 0.0f); brake = 0; }
            }
            if (brake) {
                rx_track_project_clamped_xz(node, U32(d, 0x1c), v + 0xc0, SF + 0x20);
                v = U32(U32(d, 0x74), 0x1ac);
                const float px = F32(SF, 0x20) - F32(v, 0xc0), py = F32(SF, 0x24), pz = F32(SF, 0x28) - F32(v, 0xc8);
                float r = sqrtf((pz * pz + py * py) + px * px) / F32(node, 0x4f0);
                r = (r * over) / over;
                const float b = r + F32(d, 0x3c8);
                SF32(d, 0x3c8, b);
                SF32(d, 0x3c4, b + F32(d, 0x3c4));
                SF32(d, 0x3c0, F32(d, 0x3c0) - 1.0f);
            }
        }
    }
    uint32_t flags = U32(node, 0x68 + 4 * KIDX());
    if ((flags & 0x20) != 0 && U32(d, 0x30c) == 0) {
        if (vehicle_airborne_race(U32(U32(d, 0x74), 0x1ac)) == 0) {
            if (U32(U32(d, 0x74), 0x8f8) == 2) SU32(d, 0x3d0, 1);
            else {
                const float roll = (float)((int32_t)race_rand() % 100);
                if (rx_ai_param_def0(d) >= roll) SU32(d, 0x3d0, 1);
            }
        }
        SU32(d, 0x30c, 1);
    } else {
        const float thr = F32(node, 0x2e8 + 4 * KIDX());
        if (thr > 0.0f || U32(d, 0x2f4) != 0) {
            if (U32(d, 0x2f4) != 0) {
                SU32(d, 0x2f4, rx_race_lean_recover(d, dt, (((SF - 8) - 4) & ~15u) - 0x60));
            } else {
                SU32(d, 0x2f4, ai_start_lean(d, node + 0x2e8));
            }
        } else {
            rx_addr p = U32(d, 0x74);
            const uint32_t idx = U32(p, 0x8f4) + 4 * (3 * U32(d, 0x328) + U32(d, 0x32c));
            if ((U32(node, 0x68 + 4 * KIDX()) & 0x8000) != 0 && U32(d, 0x324) == 0) {
                const float t = F32(0, 0x9e3ff8 + 4 * idx);
                SF32(d, 0x320, t);
                SF32(d, 0x320, rx_ai_param_de50(d) * t * 0.01f /* 0x9322ac */);
                SU32(d, 0x324, 1);
            } else if ((U32(node, 0x68 + 4 * KIDX()) & 8) != 0 && U32(d, 0x31c) == 0) {
                const float t = -0.0f /* 0x9774a8 */ - F32(0, 0x9e43b8 + 4 * idx);
                SF32(d, 0x318, t);
                SF32(d, 0x318, rx_ai_param_dea0(d) * t * 0.01f);
                SU32(d, 0x31c, 1);
            } else if ((U32(node, 0x68 + 4 * KIDX()) & 0x800) != 0 && U32(d, 0x31c) == 0) {
                SF32(d, 0x318, F32(0, 0x9cd9e8) /* -1 */);
                SU32(d, 0x31c, 1);
            }
        }
    }
    /* keep to a side */
    flags = U32(node, 0x68 + 4 * KIDX());
    if ((flags & 0x40) == 0) {
        SU32(d, 0x310, 0);
    } else {
        const int32_t side = I32(d, 0x310);
        if (side == 0) {
            rx_addr v = U32(U32(d, 0x74), 0x1ac);
            SF32(SF, 0x20, F32(node, 0) - F32(v, 0xc0));
            SF32(SF, 0x24, F32(node, 4) - F32(v, 0xc4));
            SF32(SF, 0x28, F32(node, 8) - F32(v, 0xc8));
            rx_race_to_vehicle_frame(v, SF + 0x20);
            SU32(d, 0x310, 0.0f > F32(SF, 0x20) ? 1 : 0xffffffffu);
        } else {
            const float f = rx_race_track_lateral(d, U32(U32(d, 0x74), 0x1ac) + 0xc0);
            if (f > 0.75f /* 0x935aa8 */ && side == -1) SU32(d, 0x310, 1);
            else if (0.25f /* 0x92ff84 */ > f && side == 1) SU32(d, 0x310, 0xffffffffu);
        }
    }
    /* forced lanes */
    if (U32(U32(d, 0x10), 0x1e8) == 0) {
        const int32_t lane = I32(d, 0x314);
        flags = U32(node, 0x68 + 4 * KIDX());
        if ((flags & 0x100) != 0) {
            rx_addr b3;
            if (lane != -1 && U32(d, 0x330) == 0 && (b3 = rx_driver_behavior_3(d)) != 0) {
                SU32(d, 0x314, 0xffffffffu);
                race_thiscall(race_vfn(b3, 7), b3, 1, 1, 0, 0, 0);
            }
        } else if ((flags & 0x80) != 0) {
            rx_addr b3;
            if (lane != 1 && U32(d, 0x330) == 0 && (b3 = rx_driver_behavior_3(d)) != 0) {
                SU32(d, 0x314, 1);
                race_thiscall(race_vfn(b3, 7), b3, 1, 1, 0, 0, 0);
            }
        } else {
            SU32(d, 0x330, 0);
            if (lane != 0) {
                rx_addr b3 = rx_driver_behavior_3(d);
                if (b3 != 0) {
                    SU32(d, 0x314, 0);
                    SU32(d, 0x330, 0);
                    race_thiscall(race_vfn(b3, 7), b3, 1, 0, 0, 0, 0);
                }
            }
        }
        if (lane == 0 && U32(d, 0x314) != 0) {
            const float roll = (float)((int32_t)race_rand() % 100);
            if (roll > rx_ai_param_df40(d)) { SU32(d, 0x330, 1); SU32(d, 0x314, 0); }
        }
    }
#undef KIDX
    RACE_ORIG_FRAME_LEAVE(SF);
}
RX_FUNCTION(0x005118c0, rx_ai_track_node_reactions)

/* 0051dcd0 (EAX = the driver) -> xmm0: the start reaction chance of the driver's rider: the table 0xa89d68 (row = the vehicle data
 * +0xee8, indexed by the rider's +0x8fc) when that is set, else 0xa89d08 (row +0xee8, index (+0x8f4 * 3 + +0x8f8)). Frame: push
 * ecx. */
float rx_driver_start_chance(rx_addr d) {
    const rx_addr entry0 = rx_sp(), E = entry0 - 4u;           /* push ecx */
    rx_set_sp(E);
    const rx_addr p = U32(d, 0x74);
    const int32_t idx = I32(p, 0x8fc);
    float r;
    if (idx != -1) r = F32(0, 0x00a89d68 + I32(d, 0xee8) * 0xc7c + idx * 8);
    else r = F32(0, 0x00a89d08 + I32(d, 0xee8) * 0xc7c + (I32(p, 0x8f4) * 3 + I32(p, 0x8f8)) * 8);
    rx_set_sp(entry0);
    return r;
}
RX_FUNCTION(0x0051dcd0, rx_driver_start_chance)

/* 0050fb00 RaceDriver::vf5 (ECX = the driver; no RET): the countdown's start reaction: with the session state 1 only; an AI driver
 * (player kind +0x10 != 0: the original's `if (*(int *)(player + 0x10) != 0)`, so a human draws nothing here), with the start board
 * (+0x2cc of the activity): the board's countdown time +0x4f4 and the game RNG's percentage against the
 * rider's chance (0051dcd0): chance < percentage -> state 4 (+0x10f8 = rand * (1/32768) * (t - 3) + the driver's +0x130 + (t - 3));
 * else state 5 (+0x10fc / +0x10f8 = t - rand * (1/32768) * (t - 3), then - rand * (1/32768) + +0x130). Without a board: the RNG's
 * percentage below 0x32 -> state 1, else state 2 (+0x1108 = 0, +0x1104 = rand * (1/32768) + +0x130). Otherwise state 0. */
void rx_driver_vf5(rx_addr d) {
    const rx_addr entry0 = rx_sp(), E = entry0 - 8u;           /* push ecx; push esi; push edi */
    rx_set_sp(E);
    const rx_addr p = U32(d, 0x74), act = U32(p, 0x94);
    if (I32(act, 0x238) != 1) { SU32(d, 0xf10, 0); rx_set_sp(entry0); return; }
    if (U32(p, 0x10) != 0) {
        const rx_addr board = U32(act, 0x2cc);
        if (board != 0) {
            const float t = F32(board, 0x4f4);
            SU32(0, 0x009cb860, U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u);
            const uint32_t rnd = (U32(0, 0x009cb860) >> 16) & 0x7fffu;
            SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1u);
            const float pct = (float)(int32_t)(rnd % 100u);
            const float chance = rx_driver_start_chance(d);
            if (chance < pct) {
                SU32(d, 0xf10, 4);
                const float u = (float)crt_rand();
                const float tm = phys_sub(t, 3.0f /* 0x92fe48 */);
                SF32(d, 0x10f8, phys_add(phys_add(phys_mul(phys_mul(u, 3.0518509447574615e-05f /* 0x92e054 */), tm), F32(d, 0x130)), tm));
            } else {
                SU32(d, 0x10fc, rx_fb(t));
                const float u = (float)crt_rand();
                const float f = phys_sub(t, phys_mul(phys_mul(u, 3.0518509447574615e-05f), phys_sub(t, 3.0f)));
                SF32(d, 0x10fc, f);
                SF32(d, 0x10f8, f);
                const float base = F32(d, 0x130);
                const float u2 = (float)crt_rand();
                SF32(d, 0x10f8, phys_add(phys_sub(F32(d, 0x10f8), phys_mul(u2, 3.0518509447574615e-05f)), base));
                SF32(d, 0x10fc, phys_add(F32(d, 0x10fc), base));
                SU32(d, 0xf10, 5);
            }
            rx_set_sp(entry0);
            return;
        }
        SU32(0, 0x009cb860, U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u);
        const uint32_t rnd = (U32(0, 0x009cb860) >> 16) & 0x7fffu;
        SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1u);
        if ((int32_t)(rnd % 100u) < 0x32) {
            SU32(d, 0xf10, 1);
            SF32(d, 0x10f8, 0.0f);
            rx_set_sp(entry0);
            return;
        }
        SU32(d, 0xf10, 2);
        SU32(d, 0x1108, 0);
        SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1u);
        SU32(0, 0x009cb860, U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u);
        const uint32_t r2 = (U32(0, 0x009cb860) >> 16) & 0x7fffu;
        SF32(d, 0x1104, phys_add(phys_mul((float)(int32_t)r2, 3.0518509447574615e-05f), F32(d, 0x130)));
        SF32(d, 0x10f8, 0.0f);
    }
    rx_set_sp(entry0);
}
RX_FUNCTION(0x0050fb00, rx_driver_vf5)

/* 0050f5d0 RaceDriver::vf4 (ECX = driver): the player's controls reset (006c9100 on P + 0xb58), the player's finish reset
 * (0054e280), the vehicle simulation's pose override ended (00434ef0 on [[P+0x1ac]+0x234]), the driver's +0x3c0 / +0x3cc = 0.
 * Frame: push ebx, edi. */
void rx_race_driver_vf4(rx_addr d) {
    RACE_FRAME_PLAIN(F, 0, 2);
    const rx_addr p = U32(d, 0x74);
    if (p != 0) {
        RACE_CORE_CALL(0, rx_controls_reset(p + 0xb58));
        RACE_CORE_CALL(0, rx_player_finish_reset(p));
    }
    const rx_addr p2 = U32(d, 0x74);
    RACE_CORE_CALL(0, bike_end_pose_override(U32(U32(p2, 0x1ac), 0x234)));
    SF32(d, 0x3c0, 0.0f);
    SF32(d, 0x3cc, 0.0f);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0050f5d0, rx_race_driver_vf4)
