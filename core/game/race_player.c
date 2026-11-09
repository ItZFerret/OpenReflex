/* race_player.c -- the race layer's MXPlayer helpers: resets to the track, the reset fade. MXPlayer: +0x04 local, +0x08 local
 * index, +0x10 kind (0 local human, 1 AI, 2 remote), +0x94 activity, +0x1ac vehicle, +0x116c reset timer (> 0 while a reset runs),
 * +0x1170 reset reason, +0x1180 / +0x1190 the reset position / orientation. (core/notes/race.md "Player") */
#include "race.h"
#include "race_extern.h"
#include "loop_player.h"
#include "loop_vehicle.h"
#include "world_misc.h"

/* 0054b720 (ESI = player), 0054fff0 (EAX = player, arg; RET 4), 0054d7c0 (EAX = player) -> EAX: the player's own reset steps
 * (rx_loop). */
static void player_begin_reset(rx_addr player) {
    RACE_CORE_CALL(0, rx_player_set_fade_hold(player));
}
static void player_finish_reset(rx_addr player, uint32_t arg) {
    RACE_CORE_CALL(1, rx_player_reset(player, arg));
}
static uint32_t player_has_viewport(rx_addr player) {
    uint32_t r = 0;
    RACE_CORE_CALL(0, r = rx_player_can_reset(player));
    return r;
}

/* 00550280 (ESI = player): for a local human player, starts the reset fade on its viewport (engine context 0xa95844: the current
 * view set's viewport by local index; fade record = viewport +0x70: state 0, then +0x10.. kind 1 / 3, colour 0x9ca220 (white),
 * alpha 1, duration = the reset time 0x9c5424). */
void rx_player_reset_fade(rx_addr player) {
    if (U32(player, 4) == 0 || U32(player, 0x10) != 0 || F32(0, 0x9c5424) == 0.0f) return;
    if (player_has_viewport(player) == 0) return;
    rx_addr ctx = U32(0, 0xa95844);
    rx_addr view = U32(U32(ctx, 0x1c + 4 * U32(ctx, 4)), 0x19b8 + 4 * U32(player, 8));
    if (view == 0) return;
    rx_addr fade = U32(view, 0x70);
    SU32(fade, 0, 0);
    rx_addr f = fade + 0x10;
    if (f == 0) return;
    SU32(f, 0x30, 1);
    SF32(f, 0x20, F32(0, 0x9ca220)); SF32(f, 0x24, F32(0, 0x9ca224)); SF32(f, 0x28, F32(0, 0x9ca228));
    SF32(f, 0x2c, 1.0f /* 0x931f24 */);
    SF32(f, 0xc, F32(0, 0x9c5424));
    SU32(f, 4, 1);
    SF32(f, 0x10, 0.0f);
    SU32(f, 8, 3);
    SF32(f, 0x18, 0.0f);
}
RX_FUNCTION(0x00550280, rx_player_reset_fade)

/* 005501f0 (EDX = position, ECX = orientation, arg = player; RET 4): place the player at a reset point while the race runs
 * (activity state 2 / 3): store the point, start the reset (timer = 0x9c5424); local humans get the fade, others finish at once. */
void rx_player_reset_to(rx_addr player, rx_addr pos, rx_addr orient) {
    rx_addr act = U32(player, 0x94);
    if (act == 0) return;
    uint32_t st = U32(act, 0x21c);
    if (st != 2 && st != 3) return;
    for (uint32_t i = 0; i < 12; i += 4) SF32(player, 0x1180 + i, F32(pos, i));
    for (uint32_t i = 0; i < 12; i += 4) SF32(player, 0x1190 + i, F32(orient, i));
    player_begin_reset(player);
    SF32(player, 0x116c, F32(0, 0x9c5424));
    if (U32(player, 4) != 0 && U32(player, 0x10) == 0) rx_player_reset_fade(player);
    else player_finish_reset(player, 0);
}
RX_FUNCTION(0x005501f0, rx_player_reset_to)

/* Activity vf19 (slot 19): the reset point for a player (player, out position, out orientation, out extra) -> nonzero if one. */
typedef uint32_t (*race_activity_reset_point_fn)(rx_addr activity, rx_addr player, rx_addr pos, rx_addr orient, rx_addr extra);
static uint32_t activity_reset_point(rx_addr act, rx_addr player, rx_addr pos, rx_addr orient, rx_addr extra) {
    rx_addr fn = race_vfn(act, 19);
    race_activity_reset_point_fn f = RX_FN(fn, race_activity_reset_point_fn);
    if (f) return f(act, player, pos, orient, extra);
    uint32_t a[4] = {player, pos, orient, extra};
    return rx_call_original(fn, RX_THISCALL, act, 4, a);
}

/* Reset the player to the track unless a reset already runs: the activity gives the point; +0x1170 = the reason. */
static void reset_to_track(rx_addr player, uint32_t reason) {
    if (F32(player, 0x116c) > 0.0f /* 0x92e12c */) return;
    RACE_FRAME16_ENTER(F, 0x30);
    if (activity_reset_point(U32(player, 0x94), player, F + 0x10, F, F + 0x20) != 0) rx_player_reset_to(player, F + 0x10, F);
    RACE_FRAME16_LEAVE(F);
    SU32(player, 0x1170, reason);
}
/* 005507a0 (ESI = player): reset to the track for riding off it (reason 3). */
void rx_player_reset_offtrack(rx_addr player) { reset_to_track(player, 3); }
RX_FUNCTION(0x005507a0, rx_player_reset_offtrack)
/* 00550800 (ESI = player): reset to the track for riding the wrong way (reason 4). */
void rx_player_reset_to_track(rx_addr player) { reset_to_track(player, 4); }
RX_FUNCTION(0x00550800, rx_player_reset_to_track)

/* 00550920 (ESI = player; no result): reset the player where its vehicle is, unless a reset already runs (+0x116c > 0): the
 * vehicle's position (+0x200) and the forward direction of its orientation quaternion (+0x210; minus the matrix's third column:
 * -0 - (2(xz + yw), 2(yz - xw), 1 - 2(x^2 + y^2)), the constants 2.0 0x92e330 and -0.0 0x9774a8) go to the reset (005501f0), the
 * reason becomes 5 and the player's reset runs (0054fff0, flag 0). Frame: push ebp; and esp, -16; sub esp, 0x20; the position copy
 * at +0x10, the quaternion copy (overwritten by the direction) at +0. */
void rx_player_reset_in_place(rx_addr player) {
    if (F32(player, 0x116c) > 0.0f /* 0x92e12c */) return;
    RACE_FRAME_EBP(S, 16, 0x20, 0);
    rx_addr veh = U32(player, 0x1ac);
    rx_st4(S + 0x10, rx_ld4(veh + 0x200));
    rx_st4(S, rx_ld4(veh + 0x210));
    const float x = F32(S, 0), y = F32(S, 4), z = F32(S, 8), w = F32(S, 0xc);
    const float two = 2.0f /* 0x92e330 */, k = -0.0f /* 0x9774a8 */;
    const float z2 = z * two, x2 = x * two, y2 = y * two;
    const float a = x * z2 + y2 * w;
    const float b = x * x2 + y2 * y;
    const float r = 1.0f /* 0x931f24 */ - b;
    const float e = z2 * y - x2 * w;
    const float vx = k - a, vy = k - e, vz = k - r;
    SF32(S, 0, vx); SF32(S, 4, vy); SF32(S, 8, vz);
    RACE_CORE_CALL(1, rx_player_reset_to(player, S + 0x10, S));
    SU32(player, 0x1170, 5);
    RACE_CORE_CALL(1, rx_player_reset(player, 0));
    RACE_FRAME_END(S);
}
RX_FUNCTION(0x00550920, rx_player_reset_in_place)

/* The respawn body of 00550b70 (the same as 00550650 / 00550cc0 in race_activity.c): unless a reset already runs (+0x116c above 0; comiss / ja,
 * so a NaN runs it), the activity's respawn point (vf19 (player, position, direction, up)); without one, where its vehicle is
 * (+0x1ac: the position +0x200 and the axes of the orientation quaternion +0x210: forward (2xz + 2wy, 2yz - 2wx, 1 - 2xx - 2yy),
 * up (2xy - 2wz, 1 - 2xx - 2zz, 2yz + 2wx), the constant 2.0 0x92e330); then the reset (005501f0, EDX = position, ECX =
 * orientation) and the reason +0x1170 = `reason`.
 * Frame: push ebp; and esp, -16; sub esp, 0x50 (the position at +0x40, the direction at +0x20, the up axis at +0x30, the
 * quaternion at +0x10), no pushes. */
static void respawn_at_point(rx_addr player, uint32_t reason) {
    if (F32(player, 0x116c) > 0.0f /* 0x92e12c */) return;
    RACE_FRAME_EBP(F, 16, 0x50, 0);
    const rx_addr act = U32(player, 0x94);
    const uint32_t ok = race_thiscall(race_vfn(act, 19), act, 4, player, F + 0x40, F + 0x20, F + 0x30);
    if (ok == 0) {
        const rx_addr v = U32(player, 0x1ac);
        for (uint32_t i = 0; i < 16; i += 4) { SU32(F + 0x40, i, U32(v, 0x200 + i)); SU32(F + 0x10, i, U32(v, 0x210 + i)); }
        const float qx = F32(F, 0x10), qy = F32(F, 0x14), qz = F32(F, 0x18), qw = F32(F, 0x1c);
        const float y2 = qy * 2.0f, z2 = qz * 2.0f, x2 = qx * 2.0f;
        const float yz2 = z2 * qy, xx2 = qx * x2;
        SF32(F, 0xc, xx2);
        const float xy2_wz2 = qx * y2 - z2 * qw;
        const float wx2 = x2 * qw;
        const float zz_xx = z2 * qz + xx2;
        const float xz2 = qx * z2, yy_xx = y2 * qy + xx2, wy2 = y2 * qw;
        SF32(F, 0x30, xy2_wz2);
        SF32(F, 0x34, 1.0f - zz_xx);
        SF32(F, 0x38, yz2 + wx2);
        SF32(F, 0x20, xz2 + wy2);
        SF32(F, 0x24, yz2 - wx2);
        SF32(F, 0x28, 1.0f - yy_xx);
    }
    RACE_CORE_CALL(1, rx_player_reset_to(player, F + 0x40, F + 0x20));
    SU32(player, 0x1170, reason);
    RACE_FRAME_END(F);
}

/* 00550b70 (ESI = player): the reset over a "reset" terrain material (0054f3e0, terrain material 7): respawn_at_point, reason 6. */
void rx_player_reset_terrain(rx_addr player) { respawn_at_point(player, 6); }
RX_FUNCTION(0x00550b70, rx_player_reset_terrain)

/* 0054e280 (ESI = player): the wreck recovery cleared (0054e890), the reset clocks +0x1168 / +0x116c = 0; with a wrecked vehicle
 * (+0x280) and an activity of another kind than 0x16 its controls released (0045f590, EDI = the vehicle); then, when the player's
 * +0x99c object has its +0x2f4 set, that object's vf29 (the original's tail jump to slot 0x74). Frame: push edi. */
void rx_player_finish_reset(rx_addr p) {
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(0, rx_world_rider_clear_wreck(p));                    /* 0054e890 (EAX = the player) */
    const rx_addr veh = U32(p, 0x1ac);
    SF32(p, 0x1168, 0.0f);
    SF32(p, 0x116c, 0.0f);
    if (U32(veh, 0x280) != 0 && U32(U32(p, 0x94), 0x23c) != 0x16) RACE_CORE_CALL(0, rx_vehicle_release_controls(veh));
    const rx_addr o = U32(p, 0x99c);
    if (U32(o, 0x2f4) != 0) race_thiscall(race_vfn(o, 29), o, 0, 0, 0, 0, 0);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0054e280, rx_player_finish_reset)
