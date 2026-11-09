/* race_snake.c -- MXActivitySnake (the Snake party mode, activity type 0xa; vtable 0x00944884; 0xff220 bytes) and its
 * SnakeActivityAudioLogic (vtable 0x00942edc), hand-transliterated from the original (the address of each function is in its comment;
 * frames and the stack depth of each call are the original's).
 *
 * The game: every rider lays a trail (a wall of light: race_snake_trail.c, the driver's +0xf14 trail object) behind its vehicle; a rider
 * whose vehicle hits a trail (its own after 25 segments, or another's: the activity's spatial hash +0xff070, 0069bba0) or whose energy
 * (+0xf34: down below 40 ft/s, up above) reaches 0 is cut off (+0xf1c; "Player Eliminated", SnakeDriver::vf7: race_snake_driver.c); the
 * trail's owner scores a kill (+0xf2c). The round ends when at most one rider is left (vf42): its rounds won (+0xf24) + 1; +0xff0a0 (the
 * activity row's NumLaps) rounds win the match; 3 s of WindDown (vf27) restart the next round (vf16 / vf15). Team mode (+0xff0b4) is never
 * set offline. A circular boundary (the start zone's SnakeAreaRadius around its position) shrinks by BoundaryShrinkRate / 2 per second
 * (0056d080; a zone with UseWaterAsBoundary raises the water instead: 0056cf80); a human outside it for 5 s is cut off
 * (RuleOutOfSnakeBounds, race_snake_rules.c). The boundary is drawn by the activity's XenonSnakeWallVisibleMesh (+0xff100,
 * race_snake_wall.c).
 *
 * ONLINE (a Quazal session: [0x00da6cb0] + 0x1c4c set and the mode +0x1b0 not 1; none exists in the product): the NetSnake object
 * (+0xff0c4: the round count, the winner and the boundary for the stations), the session's start zones, the drivers' network objects
 * (+0xef0), the host's round end (+0xff0c8, "NetworkSnakeRoundComplete" 0056d1f0), the spectators' respawn. Each networked branch is
 * marked "ONLINE" and stops with act_unreached. */
#include "race_snake.h"
#include "race_snake_trail.h"
#include "race_activity.h"
#include "race_activity_misc.h"
#include "race_activity_ui.h"
#include "race_finish.h"
#include "race_extern.h"
#include "race_champ.h"
#include "race.h"
#include "race_tag.h"
#include "build_activity.h"
#include "build_activity_int.h"
#include "build_activity_teardown.h"
#include "build_activity_freeride_run.h"
#include "build_vehicle.h"
#include "build_alloc.h"
#include "waypoint_activity.h"
#include "waypoint_driver.h"
#include "waypoint_audio.h"
#include "world_rider.h"
#include "world_wreck.h"
#include "loop_vehicle.h"
#include "loop_platform.h"
#include "phys_terrain.h"
#include "phys_crt.h"
#include "race_beh1_extern.h"
#include "activity_cues.h"
#include "fx.h"
#include "anim.h"
#include "crt.h"
#include "../src/rx_d3d.h"
#include "../src/rx_os.h"
#include <string.h>
#include "rider_cap.h"

#define VT_SNAKE        0x00944884u
#define VT_SNAKE_AUDIO  0x00942edcu
#define VT_AUDIO_LOGIC  0x009429fcu
#define TD_ACTIVITY     0x009cbd24u   /* void(Activity*) */
#define TD_STRING       0x009cdb68u   /* void(const char*) */
#define TD_ROUND_STARTED 0x009cf680u  /* void(Activity*, int, int, int) */
#define STR_ROUND_STARTED 0x009421b0u
#define STR_ROUND_OVER  0x009421c0u
#define STR_MATCH_OVER  0x009421ccu
#define STR_ELIMINATED  0x0093ef48u   /* "Player Eliminated" */
#define STR_TRAIL_HIT   0x0093efa4u   /* "SnakeTrailHit" */
#define STR_PLAYER_WINS 0x00941ca8u
#define HASH(A)         ((A) + 0xff070u)
#define WALL(A)         ((A) + 0xff100u)

static void session_static(void) {
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
}
static int session_networked(void) {
    const rx_addr s = U32(0, 0x00da6cb0);
    return U32(s, 0x1c4c) != 0 && U32(s, 0x1b0) != 1;
}
static float x87_copy(float v) {
    uint32_t b = rx_fb(v);
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu)) b |= 0x400000u;
    return rx_bf(b);
}
static rx_addr scene_terrain(rx_addr scene) {
    if (!(U8(0, 0x009e30e0) & 1)) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
    for (uint32_t i = 0; i < U32(0, 0x009e30dc); i++) {
        const rx_addr t = U32(0, 0x009e30d4 + 4 * i);
        if (t != 0 && U32(t, 0x44a8) == scene) return t;
    }
    return 0;
}
static void registry_float(rx_addr key, rx_addr out) {
    const rx_addr reg = U32(U32(0, 0x00a95844), 0xc);
    const uint32_t find[3] = {0x00942184 /* "MXGame\\SnakeActivity" */, key, 1};
    const rx_addr rec = act_vcall(reg, 4, 3, find);
    if (rec != 0) { const uint32_t read[3] = {rec, out, 1}; (void)act_vcall(reg, 0x44, 3, read); }
}
static inline void os_cs(uint32_t slot, rx_addr cs) { const uint32_t a[1] = {cs}; (void)rx_os_import(slot, 1, a); }
/* the NetSnake object is live (00884950): ONLINE only (+0xff0c4 is 0 offline) */
static int netsnake_live(rx_addr act) { const rx_addr n = U32(act, 0xff0c4); return n != 0 && (rx_net_ref_valid(n) & 0xff) != 0; }

/* ======================================================================================================================== */
/* construction / destruction                                                                                                */

/* 0056b050 (stack act; ECX = type, EAX = world; RET 4) -> act: Activity's constructor, the trails' spatial hash (006a21d0) at +0xff070,
 * the boundary wall (0057ebb0: red {1, 0, 0}, 250 segments) at +0xff100, the class's fields (the radius 300, the water rise 60 s), the wall
 * added to the scene twice (006a0e20, then again inline), a new SnakeActivityAudioLogic (0x98 bytes) at +0xfd8.
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x14; push ebx / esi / edi (the colour at F + 0x10). */
rx_addr rx_snake_construct(rx_addr act, uint32_t type, rx_addr world) {
    ACT_FRAME_EBP(F, 16, 0x14, 3);
    ACT_CALL(1, rx_activity_construct(act, type, world));
    SU32(act, 0, VT_SNAKE);
    ACT_CALL(0, rx_snake_hash_init(HASH(act)));
    SF32(F, 0x10, 1.0f /* 0x931f24 */);
    SU32(HASH(act), 0x28, 0);
    SF32(F, 0x14, 0.0f);
    SF32(F, 0x18, 0.0f);
    ACT_CALL(3, (void)rx_snake_wall_construct(WALL(act), F + 0x10, 0xfa));
    SF32(act, 0xff0dc, 300.0f /* 0x935d28 */);
    SU32(act, 0x2c8, 1);
    SU32(act, 0xff0b0, 1);
    SU32(act, 0xff09c, 0);
    SU32(act, 0xff0a0, 4);
    SU32(act, 0xff0a4, 0);
    SU32(act, 0xff0b4, 0);
    SU32(act, 0xff0bc, 0);
    SU32(act, 0xff0b8, 0);
    SU32(act, 0xff0c0, 0xffffffffu);
    SU32(act, 0xff0c4, 0);
    SF32(act, 0xff0e0, 0.0f); SF32(act, 0xff0e4, 0.0f); SF32(act, 0xff0e8, 0.0f);
    SF32(act, 0xff0f4, 0.0f);
    SU32(act, 0xff0cc, 0);
    ACT_CALL(0, rx_snake_mesh_scene_add(WALL(act), U32(U32(act, 0x234), 0x20138)));
    const rx_addr scene = U32(U32(act, 0x234), 0x20138);
    rx_set_sp(F - 4);
    os_cs(0x0091600c, scene + 4);
    rx_set_sp(F);
    const uint32_t id = U32(scene, 0x19f4);
    SU32(scene, 0x19f4, id + 1);
    SU32(WALL(act), 0x14, id);
    rx_set_sp(F - 4);
    os_cs(0x009160f8, scene + 4);
    rx_set_sp(F);
    ACT_CALL(1, rx_scene_manager_add_entity(WALL(act), scene));
    SF32(act, 0xff0d4, 0.0f);
    SF32(act, 0xff0d8, 60.0f /* 0x931da0 */);
    SU32(act, 0xff0d0, 0);
    rx_addr logic;
    ACT_CALLR(logic, 1, rx_reflex_aligned_alloc(0x98));
    if (logic != 0) {
        SU32(logic, 0, VT_AUDIO_LOGIC);
        SU32(logic, 0x94, 0);
        SU8(logic, 4, 0);
        SU32(logic, 0x50, 0);
        SU32(logic, 0x54, 0);
        SU32(logic, 0x8c, 0);
        SU32(logic, 0x90, 0);
        ACT_CALL(0, rx_audio_logic_vf32(logic));
        SU32(logic, 0, VT_SNAKE_AUDIO);
    }
    SU32(act, 0xfd8, logic);
    ACT_END(F);
    return act;
}
RX_FUNCTION(0x0056b050, rx_snake_construct)

/* 0056b230 (EDI = act): the destructor's body. Unless the boundary is water (+0xff0d0) the wall leaves the scene (007d3040) and its vf9
 * (slot 0x24) runs; the audio logic destroyed (00580d30) and freed; the wall's body (0057f020); the spatial hash's cells released
 * (006a26b0) and its blocks freed; Activity's destructor (005566b0, a tail jump). Frame: push esi. */
void rx_snake_destroy(rx_addr act) {
    ACT_FRAME(F, 0, 1);
    SU32(act, 0, VT_SNAKE);
    if (U32(act, 0xff0d0) == 0) {
        ACT_CALL(1, rx_scene_manager_remove_entity(WALL(act), U32(act, 0xff110)));
        (void)act_vcall(WALL(act), 0x24, 0, 0);
    }
    const rx_addr logic = U32(act, 0xfd8);
    if (logic != 0) {
        ACT_CALL(1, rx_activity_audio_destroy(logic));
        ACT_CALL(1, rx_reflex_aligned_free(logic));
    }
    ACT_CALL(0, rx_snake_wall_destroy(WALL(act)));
    ACT_CALL(0, rx_snake_hash_clear(HASH(act)));
    ACT_CALL(1, rx_reflex_aligned_free(U32(HASH(act), 0x14)));
    ACT_CALL(1, rx_reflex_aligned_free(U32(HASH(act), 0)));
    ACT_END(F);
    rx_waypoint_activity_base_destroy(act);
}
RX_FUNCTION(0x0056b230, rx_snake_destroy)

/* 0056b210 slot 0 (thiscall; flags; RET 4) -> act: 0056b230, freed with flags & 1. Frame: push edi. */
rx_addr rx_snake_sdtor(rx_addr act, uint32_t flags) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(0, rx_snake_destroy(act));
    if (flags & 1) ACT_CALL(1, rx_reflex_aligned_free(act));
    ACT_END(F);
    return act;
}
RX_FUNCTION(0x0056b210, rx_snake_sdtor)

/* ======================================================================================================================== */
/* the listeners (as race_tag.c's templates)                                                                                 */

/* 00588dc0 (EAX = the session channel; stdcall act; RET 4): "NetworkObjectRemoved" with 0056cf60 (proxy 0x945a84) */
void rx_snake_net_removed_listen(rx_addr channel, rx_addr act) {
    ACT_CALL(1, act_subscribe(0x00588dc0, 1, 1, 2, channel, 0x0093ee98 /* "NetworkObjectRemoved" */, 0x00945a84, 0x009cdc7c, act, 0x0056cf60));
}
RX_FUNCTION(0x00588dc0, rx_snake_net_removed_listen)
/* 00588e80: "NetworkSnakeBoundaryChanged" with 0056d1d0 (proxy 0x945aa4: void(float)) */
void rx_snake_net_boundary_listen(rx_addr channel, rx_addr act) {
    ACT_CALL(1, act_subscribe(0x00588e80, 1, 1, 2, channel, 0x00942128 /* "NetworkSnakeBoundaryChanged" */, 0x00945aa4, 0x009d00dc, act, 0x0056d1d0));
}
RX_FUNCTION(0x00588e80, rx_snake_net_boundary_listen)
/* 00588f40: "NetworkSnakeRoundComplete" with 0056d1f0 (proxy 0x945ac4) */
void rx_snake_net_round_listen(rx_addr channel, rx_addr act) {
    ACT_CALL(1, act_subscribe(0x00588f40, 1, 1, 2, channel, 0x00942144 /* "NetworkSnakeRoundComplete" */, 0x00945ac4, 0x009d0140, act, 0x0056d1f0));
}
RX_FUNCTION(0x00588f40, rx_snake_net_round_listen)
/* 00589000 / 00589060 / 005890c0: the three listeners leave (frame: push ebp; mov ebp, esp; and esp, -8; sub esp, 0x14; push esi) */
void rx_snake_net_removed_unlisten(rx_addr channel, rx_addr act) { act_unlisten(1, channel, 0x0093ee98, 0x00945a84, 0x009cdc7c, act, 0x0056cf60); }
RX_FUNCTION(0x00589000, rx_snake_net_removed_unlisten)
void rx_snake_net_boundary_unlisten(rx_addr channel, rx_addr act) { act_unlisten(1, channel, 0x00942128, 0x00945aa4, 0x009d00dc, act, 0x0056d1d0); }
RX_FUNCTION(0x00589060, rx_snake_net_boundary_unlisten)
void rx_snake_net_round_unlisten(rx_addr channel, rx_addr act) { act_unlisten(1, channel, 0x00942144, 0x00945ac4, 0x009d0140, act, 0x0056d1f0); }
RX_FUNCTION(0x005890c0, rx_snake_net_round_unlisten)

/* ======================================================================================================================== */
/* the activity's virtuals                                                                                                   */

/* 0056d340 slot 1 (thiscall; &row; RET 4): the rounds to win (+0xff0a0) from the activity row's NumLaps, then Activity::vf1.
 * Frame: push ebx / ebp / esi / edi. */
void rx_snake_vf1(rx_addr act, rx_addr ref) {
    ACT_FRAME(F, 0, 4);
    const uint32_t row = U32(ref, 0);
    uint32_t col;
    ACT_CALLR(col, 1, rx_tbl_column_index(ACT_ACTIVITY_TBL, 0x0093dd64 /* "NumLaps" */));
    rx_addr cell;
    ACT_CALLR(cell, 1, rx_tbl_cell_word(ACT_ACTIVITY_TBL, col, row));
    SU32(act, 0xff0a0, U32(cell, 0));
    ACT_CALL(1, rx_activity_vf1(act, ref));
    ACT_END(F);
}
RX_FUNCTION(0x0056d340, rx_snake_vf1)

/* 0056b310 slot 2 (thiscall; arg; RET 4): Activity::vf2, the three network listeners, the session's NetSnake object (ONLINE: the host's
 * 008879b0 / 00887780), the "OutOfBounds" event type (00590f60), the registry's "BoundaryWallPolyWidth" (+0xff0f8), the round 0, the
 * round's places (0056b5c0), the boundary (0056cd20), "BoundaryShrinkRate" (+0xff0f0), +0x880 = 0xc, "Activity Init" raised.
 * Frame: push ebx / esi / edi. */
void rx_snake_vf2(rx_addr act, rx_addr arg) {
    ACT_FRAME(F, 0, 3);
    ACT_CALL(1, rx_activity_vf2(act, arg));
    session_static();
    ACT_CALL(1, rx_snake_net_removed_listen(U32(0, 0x00da6ca0), act));
    session_static();
    ACT_CALL(1, rx_snake_net_boundary_listen(U32(0, 0x00da6ca0), act));
    session_static();
    ACT_CALL(1, rx_snake_net_round_listen(U32(0, 0x00da6ca0), act));
    session_static();
    if (session_networked()) {
        session_static();
        if (U32(U32(0, 0x00da6cb0), 0x1c48) != 0) act_unreached(0x0056b411, "ONLINE: the host's NetSnake object (008879b0 / 00887780)");
    }
    act_event_type(0x00590f60, U32(act, 0x290), 0x00942160 /* "OutOfBounds" */);
    registry_float(0x0094216c /* "BoundaryWallPolyWidth" */, act + 0xff0f8);
    SU32(act, 0xff09c, 0);
    ACT_CALL(1, rx_snake_place_round(act));
    ACT_CALL(0, rx_snake_boundary_reset(act));
    registry_float(0x0094219c /* "BoundaryShrinkRate" */, act + 0xff0f0);
    SU32(act, 0x880, 0xc);
    SU32(F, 0x10, act);
    act_raise(U32(0, 0x00a93f74), 0x00935ea4 /* "Activity Init" */, TD_ACTIVITY, F + 0x10, 4);
    ACT_END(F);
}
RX_FUNCTION(0x0056b310, rx_snake_vf2)

/* 0056bc00 slot 3 (ECX = activity): the teardown. A networked session's selection loop (ONLINE), the three listeners leave, the NetSnake
 * object unlinked, every driver of the two lists leaves the activity, then a jump to Activity::vf3. Frame: sub esp, 0x24; push ebx / edi. */
void rx_snake_vf3(rx_addr act) {
    ACT_FRAME(F, 0x24, 2);
    session_static();
    if (session_networked()) act_unreached(0x0056bc42, "ONLINE: the networked snake teardown (the selection iterator loop)");
    session_static();
    ACT_CALL(1, rx_snake_net_removed_unlisten(U32(0, 0x00da6ca0), act));
    session_static();
    ACT_CALL(1, rx_snake_net_boundary_unlisten(U32(0, 0x00da6ca0), act));
    session_static();
    ACT_CALL(1, rx_snake_net_round_unlisten(U32(0, 0x00da6ca0), act));
    SU32(act, 0xff0c4, 0);
    while (I32(act, 0x26c) > 0) ACT_CALL(2, rx_activity_on_player_left(act, U32(U32(act, 0x268), 0)));
    while (I32(act, 0x280) > 0) ACT_CALL(2, rx_activity_on_player_left(act, U32(U32(act, 0x27c), 0)));
    ACT_END(F);
    rx_activity_vf3(act);
}
RX_FUNCTION(0x0056bc00, rx_snake_vf3)

/* 0056b530 slot 4 (ECX = activity): the class's event types on the activity's channel (Activity::vf4 is not called): "Round Started"
 * (005925e0), "Round Over" / "Match Over" (00590c00), "Player Eliminated" (00591e00), "SnakeTrailHit" (00592700), "Off Track"
 * (00591740), "Set Arrow Target" (00592280), "Show Arrow" / "Hide Arrow" (00590f60). Frame: push esi. */
void rx_snake_vf4(rx_addr act) {
    ACT_FRAME(F, 0, 1);
    act_event_type(0x005925e0, U32(act, 0x290), 0);
    act_event_type(0x00590c00, U32(act, 0x290), STR_ROUND_OVER);
    act_event_type(0x00590c00, U32(act, 0x290), STR_MATCH_OVER);
    act_event_type(0x00591e00, U32(act, 0x290), STR_ELIMINATED);
    act_event_type(0x00592700, U32(act, 0x290), 0);
    act_event_type(0x00591740, U32(act, 0x290), 0x0093ef24 /* "Off Track" */);
    act_event_type(0x00592280, U32(act, 0x290), 0);
    act_event_type(0x00590f60, U32(act, 0x290), 0x0093ef30 /* "Show Arrow" */);
    act_event_type(0x00590f60, U32(act, 0x290), 0x0093eef0 /* "Hide Arrow" */);
    ACT_END(F);
}
RX_FUNCTION(0x0056b530, rx_snake_vf4)

/* 0056b2b0 slot 5 (ECX = activity): Activity::vf5, then the trail meshes' shared resources (006a0cd0) and shaders (006a0c70).
 * Frame: push ebp; mov ebp, esp; and esp, -8. */
void rx_snake_vf5(rx_addr act) {
    ACT_FRAME_EBP(F, 8, 0, 0);
    ACT_CALL(0, rx_activity_vf5(act));
    ACT_CALL(0, rx_snake_mesh_statics_load());
    ACT_CALL(0, rx_snake_mesh_shaders_load());
    ACT_END(F);
}
RX_FUNCTION(0x0056b2b0, rx_snake_vf5)

/* 0056b2d0 slot 6 (thiscall; flag; RET 4): the trail meshes' shared resources released (006a0d50), the three shared D3D objects of
 * 006a0c70 released ([0xa7f120], [0xa7f118], [0xa7f11c]), then Activity::vf6. Frame: push esi. */
void rx_snake_vf6(rx_addr act, uint32_t flag) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(0, rx_snake_mesh_statics_release());
    static const rx_addr objs[3] = {0x00a7f120, 0x00a7f118, 0x00a7f11c};
    for (int k = 0; k < 3; k++) { const rx_addr o = U32(0, objs[k]); const uint32_t a[1] = {o}; ACT_CALL(1, (void)rx_d3d_call(o, 2, 1, a)); }
    ACT_CALL(1, rx_activity_vf6(act, flag));
    ACT_END(F);
}
RX_FUNCTION(0x0056b2d0, rx_snake_vf6)

/* 0056ce80 slot 15 (ECX = activity): the round's start. The places (0056b5c0), the boundary (0056cd20), its centre on the terrain
 * (007c5950) and the wall's ring built (0057fc00: the radius, the poly width), +0xff0f4 = 0, Activity::vf15.
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x18; push esi / edi (the centre F + 0x10). */
void rx_snake_vf15(rx_addr act) {
    ACT_FRAME_EBP(F, 16, 0x18, 2);
    ACT_CALL(1, rx_snake_place_round(act));
    ACT_CALL(0, rx_snake_boundary_reset(act));
    SF32(F, 0x10, F32(act, 0xff0e0));
    SF32(F, 0x14, F32(act, 0xff0e4));
    SF32(F, 0x18, F32(act, 0xff0e8));
    const rx_addr terrain = scene_terrain(U32(U32(act, 0x234), 0x20138));
    uint32_t edx;
    ACT_CALL(1, (void)phys_terrain_base_height_at(terrain, F + 0x10, &edx));
    const float width = x87_copy(F32(act, 0xff0f8)), radius = x87_copy(F32(act, 0xff0dc));
    rx_set_sp(F - 8);
    SF32(F - 8, 4, width);
    SF32(F - 8, 0, radius);
    ACT_CALL(0, (void)rx_snake_wall_build_ring(WALL(act), F + 0x10, radius, width));
    rx_set_sp(F);
    SF32(act, 0xff0f4, 0.0f);
    ACT_CALL(0, rx_activity_vf15(act));
    ACT_END(F);
}
RX_FUNCTION(0x0056ce80, rx_snake_vf15)

/* 0056c200 slot 16 (ECX = activity): a new round. A new match (+0xff0b0) clears every driver's rounds won (+0xf24), the match state and
 * the round count; Activity::vf16; every driver of the two lists takes its start zone (0054cd20; ONLINE: the session's 006f7330) and its
 * start place (+0x144 = the activity's +0x340 + 0x30 * zone); a live NetSnake object reset (ONLINE); +0xff0b0 / +0xff0c8 cleared.
 * Frame: push ebp; mov ebp, esp; and esp, -8; sub esp, 8; push ebx / ebp / esi / edi. */
void rx_snake_vf16(rx_addr act) {
    ACT_FRAME_EBP(F, 8, 8, 4);
    if (U32(act, 0xff0b0) != 0) {
        for (int32_t i = 0; i < I32(act, 0x26c); i++) SU32(U32(U32(act, 0x268), 4u * (uint32_t)i), 0xf24, 0);
        SU32(act, 0xff0a4, 0);
        SU32(act, 0xff09c, 0);
    }
    ACT_CALL(0, rx_activity_vf16(act));
    for (int list = 0; list < 2; list++) {
        const uint32_t cnt = list ? 0x280 : 0x26c, arr = list ? 0x27c : 0x268;
        for (int32_t i = 0; i < I32(act, cnt); i++) {
            const rx_addr d = U32(U32(act, arr), 4u * (uint32_t)i);
            session_static();
            if (session_networked()) act_unreached(0x0056c2af, "ONLINE: the session's start zone of a player (006f7330)");
            ACT_CALL(1, rx_player_start_zone(U32(d, 0x74)));
            SU32(d, 0x144, act + 0x340 + 0x30u * U32(U32(d, 0x74), 0x1dc));
        }
    }
    if (netsnake_live(act)) act_unreached(0x0056c3e9, "ONLINE: the NetSnake object's round reset (00884ba0)");
    SU32(act, 0xff0b0, 0);
    SU32(act, 0xff0c8, 0);
    ACT_END(F);
}
RX_FUNCTION(0x0056c200, rx_snake_vf16)

/* 0056bd60 slot 18 (ECX = activity): the round's reset: the winner, the team wins, the last team (-1), the water rise and the host's
 * round flag cleared, then a jump to Activity::vf18. */
void rx_snake_vf18(rx_addr act) {
    SU32(act, 0xff0a8, 0);
    SU32(act, 0xff0bc, 0);
    SU32(act, 0xff0b8, 0);
    SU32(act, 0xff0c0, 0xffffffffu);
    SF32(act, 0xff0d4, 0.0f);
    SU32(act, 0xff0c8, 0);
    rx_activity_vf18(act);
}
RX_FUNCTION(0x0056bd60, rx_snake_vf18)

/* 0056bfd0 slot 23 (thiscall; dt; RET 4): Activity::vf23, the boundary (the water's rise 0056cf80 for a water boundary +0xff0d0, else the
 * shrink 0056d080), the trail-hit counter [0xa942a0] = 0, then under the engine's profiler scope the session's NetSnake object (ONLINE:
 * found or made, its update 00555540 / 005088c0). Frame: push ebp; mov ebp, esp; and esp, -8; sub esp, 0x24; push ebx / esi / edi. */
void rx_snake_vf23(rx_addr act, float dt) {
    ACT_FRAME_EBP(F, 8, 0x24, 3);
    ACT_CALL(1, rx_activity_update(act, x87_copy(dt)));
    if (U32(act, 0xff0d0) == 0) ACT_CALL(1, rx_snake_boundary_shrink(act, x87_copy(dt)));
    else ACT_CALL(1, rx_snake_water_rise(act, x87_copy(dt)));
    SU32(0, 0x00a942a0, 0);
    session_static();
    const rx_addr scope = race_profile_begin();
    if (U32(act, 0xff0c4) == 0) {
        session_static();
        if (session_networked()) act_unreached(0x0056c0cc, "ONLINE: the session's NetSnake object (006e4fa0 / 008879b0)");
    }
    if (U32(act, 0xff0c4) != 0) act_unreached(0x0056c1c7, "ONLINE: the NetSnake object's update (00555540 / 005088c0)");
    race_profile_end(scope);
    ACT_END(F);
}
RX_FUNCTION(0x0056bfd0, rx_snake_vf23)

/* 0056c460 slot 25 (thiscall; dt; RET 4): Activity::vf25 (the intro), the UI route of the HUD ("GotoSpectatorSinglePlayer" for a spectator
 * +0xfe8, else "GotoSnakeSinglePlayer"; the UI manager's "GotoHub" event: UI BOUNDARY as race_tag.c's vf25), the standings sorted
 * (00559eb0). Frame: push ebp; mov ebp, esp; and esp, -8; sub esp, 0xc; push ebx / esi / edi (the payload F + 0x10, act F + 0x14). */
void rx_snake_vf25(rx_addr act, float dt) {
    ACT_FRAME_EBP(F, 8, 0xc, 3);
    SU32(F, 0x14, act);
    ACT_CALL(1, rx_activity_pre(act, x87_copy(dt)));
    const rx_addr ui = U32(0, 0x00a93fcc);
    rx_regs in, out;
    memset(&in, 0, sizeof in);
    in.r[6] = ui;
    in.r[7] = 0x0094cc74;   /* "UIRoutingMessage" */
    const uint32_t v[1] = {U32(act, 0xfe8) != 0 ? 0x009421d8u /* "GotoSpectatorSinglePlayer" */ : 0x009421f4u /* "GotoSnakeSinglePlayer" */};
    act_ui_call_ex(0x005dbf50, &in, &out, 1, v);
    in.r[7] = 0x0094cc74;
    const uint32_t h[1] = {0x009424ec /* "GotoHub" */};
    act_ui_call_ex(0x005db500, &in, &out, 1, h);
    if (out.r[0] != 0) {
        SU32(F, 0x10, out.r[0]);
        act_raise(U32(ui, 0x98), out.r[0], TD_STRING, F + 0x10, 4);
    }
    ACT_CALL(0, rx_activity_sort_standings(U32(F, 0x14)));
    ACT_END(F);
}
RX_FUNCTION(0x0056c460, rx_snake_vf25)

/* 0056c530 slot 26 (thiscall; dt; RET 4): Activity::vf26 (the countdown), then the standings sorted (00559eb0). Frame: push esi. */
void rx_snake_vf26(rx_addr act, float dt) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(1, rx_activity_countdown(act, x87_copy(dt)));
    ACT_CALL(0, rx_activity_sort_standings(act));
    ACT_END(F);
}
RX_FUNCTION(0x0056c530, rx_snake_vf26)

/* 0056c700 slot 27 (thiscall; dt; RET 4): WindDown. Unless the match is over (+0xff0a4: then +0xff0b0 = 1, the next start is a new match)
 * the wind-down timer (+0xff0ac) runs down and at 0 (or NaN): with spectators (+0xfe8) the first local player with a session player
 * (+0xb2c) that is a spectator (+0x908) is brought back into the race (ONLINE: only a networked session has spectators), every driver's
 * vehicle lets go of its controls (0045f590) and its trail's grow clock +0xf48 is cleared, and the activity's vf16 (slot 0x40) restarts the
 * round; the race clock's limit +0x2c4 = FLT_MAX; +0xff0c8 = 0; Activity::vf27. Frame: push ebp; mov ebp, esp; and esp, -8;
 * sub esp, 0x34; push ebx / esi / edi (the player count F + 0xc, the activity F + 0x10). */
void rx_snake_vf27(rx_addr act, float dt) {
    ACT_FRAME_EBP(F, 8, 0x34, 3);
    SU32(F, 0x10, act);
    if (U32(act, 0xff0a4) != 0) SU32(act, 0xff0b0, 1);
    else {
        const float t = rx_subs(F32(act, 0xff0ac), dt);
        SU32(act, 0xff0b0, 0);
        SF32(act, 0xff0ac, t);
        if (!(0.0f < t)) {
            if (U32(act, 0xfe8) != 0) {
                const uint32_t n = U8(U32(0, 0x00a946b8), 0x1541c);
                SU32(F, 0xc, n);
                for (int32_t i = 0; i < (int32_t)U32(F, 0xc); i++) {
                    rx_addr P;
                    ACT_CALLR(P, 2, rx_world_players_find(U32(0, 0x00a946b8) + 0x7460, (uint32_t)i, 3));
                    if (U32(P, 4) == 0 || U32(P, 0x10) != 0 || U32(P, 0xb2c) == 0) continue;
                    if (U32(P, 0x908) != 0) {
                        session_static();
                        const rx_addr np = U32(P, 0xb2c);
                        if (session_networked() && np != 0 && U32(np, 0x1d8) != 0)
                            act_unreached(0x0056c811, "ONLINE: a spectator brought back into the race (0055c800 / 006fe110 / 006f3400)");
                    }
                    break;
                }
            }
            for (int32_t i = 0; i < I32(act, 0x26c); i++) {
                const rx_addr d = U32(U32(act, 0x268), 4u * (uint32_t)i);
                ACT_CALL(0, rx_vehicle_release_controls(U32(U32(d, 0x74), 0x1ac)));
                SF32(d, 0xf48, 0.0f);
            }
            (void)act_vcall(act, 0x40, 0, 0);
        }
        SU32(act, 0x2c4, 0x7f7fffffu /* FLT_MAX, 0x977494 */);
    }
    SU32(act, 0xff0c8, 0);
    ACT_CALL(1, rx_activity_wind_down(act, x87_copy(dt)));
    ACT_END(F);
}
RX_FUNCTION(0x0056c700, rx_snake_vf27)

/* 0056bec0 slot 32 (thiscall; next state; RET 4): Activity::vf32 (the countdown's end), every driver back in (+0xf1c = 0) with its trail's
 * pause cleared (+0xac of the trail), then the grid pass of the game (0075f890: type 0xa, the activity's mode, assign). Frame: push esi. */
void rx_snake_vf32(rx_addr act, uint32_t next) {
    ACT_FRAME(F, 0, 1);
    (void)next;
    ACT_CALL(1, rx_activity_countdown_exit(act));
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        const rx_addr d = U32(U32(act, 0x268), 4u * (uint32_t)i);
        SU8(d, 0xf1c, 0);
        SU32(U32(d, 0xf14), 0xac, 0);
    }
    ACT_CALL(4, rx_game_grid_pass(U32(0, 0x00a946b8), 0xa, U32(act, 0x238), 1));
    ACT_END(F);
}
RX_FUNCTION(0x0056bec0, rx_snake_vf32)

/* 0056caa0 slot 34 (thiscall; arg; RET 4): the race's end: every driver's trail paused (+0xac of the trail = 1) */
void rx_snake_vf34(rx_addr act, uint32_t arg) {
    (void)arg;
    for (int32_t i = 0; i < I32(act, 0x26c); i++) SU32(U32(U32(U32(act, 0x268), 4u * (uint32_t)i), 0xf14), 0xac, 1);
}
RX_FUNCTION(0x0056caa0, rx_snake_vf34)

/* 0056c550 slot 39 (thiscall; old state; RET 4): InProgress begins. With spectators (+0xfe8) and a NetSnake object (ONLINE) every driver
 * whose station is out (the object's +0x80 + the player's id) is cut off and the round count is the rounds won summed; then a jump to
 * Activity::vf39. */
void rx_snake_vf39(rx_addr act, uint32_t old_state) {
    (void)old_state;
    if (U32(act, 0xfe8) != 0 && U32(act, 0xff0c4) != 0) {
        uint32_t sum = 0;
        for (int32_t i = 0; i < I32(act, 0x26c); i++) {
            const rx_addr d = U32(U32(act, 0x268), 4u * (uint32_t)i);
            const rx_addr p = U32(d, 0x74);
            if (p != 0 && U8(U32(p, 0xc) + 0x80 + U32(act, 0xff0c4), 0) != 0) SU8(d, 0xf1c, 1);
            sum += U32(d, 0xf24);
        }
        SU32(act, 0xff09c, sum);
    }
    rx_activity_in_progress_enter(act);
}
RX_FUNCTION(0x0056c550, rx_snake_vf39)

/* 00587d00's frame ("Player Wins": {activity, driver, text, value}, void(Activity*, BaseDriver*, const char*, int) 0x9cf630), as race_tag.c */
static void snake_player_wins(rx_addr manager, rx_addr activity, rx_addr driver, rx_addr text, uint32_t value) {
    ACT_FRAME(F, 0x18, 1);
    SU32(F, 4, activity); SU32(F, 8, driver); SU32(F, 0xc, text); SU32(F, 0x10, value);
    act_raise(manager, STR_PLAYER_WINS, 0x009cf630, F + 4, 0x10);
    ACT_END(F);
}

/* 0056c5c0 slot 40 (thiscall; old state; RET 4): WindDown begins. Activity::vf40; the wind-down timer 3 s; for a single-player activity
 * (mode +0x238 == 1) the round winner's message ("%s %s": the winner's name and the localized STR_WINS; no text in team mode: UI text, the
 * oracle runs the original localization chain, the product leaves an empty string, as race_tag.c's vf40) raised as "Player Wins"
 * (00587d00, value 1); then "Match Over" (+0xff0a4) or "Round Over" with the activity. Frame: sub esp, 0x20c; push ebx / esi / edi (F);
 * the UTF-8 name F + 0x10, the text F + 0x110. */
void rx_snake_vf40(rx_addr act, uint32_t old_state) {
    ACT_FRAME(F, 0x20c, 3);
    (void)old_state;
    ACT_CALL(1, rx_activity_wind_down_enter(act));
    SF32(act, 0xff0ac, 3.0f /* 0x92fe48 */);
    if (U32(act, 0x238) == 1) {
        if (U32(act, 0xff0b4) == 0) {
#ifdef RX_ORACLE
            uint32_t zero = 0;
            const uint32_t lang = rx_call_original(0x005dfc90, RX_CDECL, 0, 1, &zero);
            rx_addr entry;
            ACT_CALLR(entry, 0, rx_ui_language_lookup(0x00941fc4 /* "STR_WINS" */, lang));
            { uint32_t a[8] = {0xfde9, 0, U32(entry, 0x10), 0xffffffffu, F + 0x10, 0x100, 0, 0}; rx_os_import(0x009160a8, 8, a); }
            SU8(F, 0x10f, 0);
            SU32(F, -12, 0x00941fd0); SU32(F, -8, U32(U32(act, 0xff0a8), 0x74) + 0x54); SU32(F, -4, F + 0x10);
            rx_set_sp(F - 16);
            rx_format_buffer(F + 0x110, 0x104, 0x00941fd0 /* "%s %s" */, F - 8);
            rx_set_sp(F);
#else
            SU8(F, 0x110, 0);
#endif
        }
        ACT_CALL(2, snake_player_wins(U32(act, 0x290), act, U32(act, 0xff0a8), F + 0x110, 1));
    }
    SU32(F, 0xc, act);
    act_raise(U32(act, 0x290), U32(act, 0xff0a4) != 0 ? STR_MATCH_OVER : STR_ROUND_OVER, TD_ACTIVITY, F + 0xc, 4);
    ACT_END(F);
}
RX_FUNCTION(0x0056c5c0, rx_snake_vf40)

/* 0056cae0 slot 42 (ECX = activity): the round's end check. (ONLINE: a client waits for the host's round end +0xff0c8 / +0xbc0.) The
 * riders still in (+0xf1c == 0) are counted (by team: +0xf28), the last one found kept in +0xff0a8 with its team in +0xff0c0; the driver
 * with the most rounds won (+0xf24) is the leader. Free for all: at most one rider left (and more than one rider) ends the round: the round
 * count + 1, the survivor's rounds won + 1 (it leads if it has more), the leader with the rounds to win (+0xff0a0) wins the match
 * (+0xff0a8, +0xff0a4 = 1). Teams: one team left ends it (its round wins +0xff0b8 / +0xff0bc). A NetSnake object reports it (ONLINE); then
 * WindDown (state 3). Frame: sub esp, 0xc; push ebp / esi (+ ebx / edi; the counts at F + 0x10 .. F + 0x18). */
void rx_snake_vf42(rx_addr act) {
    ACT_FRAME(F, 0xc, 2);
    session_static();
    if (session_networked()) {
        session_static();
        if (U32(U32(0, 0x00da6cb0), 0x1c48) == 0 && U32(act, 0xff0c8) == 0 && U32(act, 0xbc0) == 0) { ACT_END(F); return; }
        act_unreached(0x0056cb68, "ONLINE: the networked snake round end");
    }
    rx_set_sp(F - 8);   /* push ebx / edi */
    const rx_addr G = F - 8;
    uint32_t alive = 0;
    int32_t best_won = -1;
    SU32(G, 0x18, 0); SU32(G, 0x14, 0); SU32(G, 0x10, 0);
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        const rx_addr d = U32(U32(act, 0x268), 4u * (uint32_t)i);
        if (U8(d, 0xf1c) == 0) {
            alive++;
            if (U32(d, 0xf28) != 0) SU32(G, 0x14, U32(G, 0x14) + 1);
            else SU32(G, 0x18, U32(G, 0x18) + 1);
            SU32(act, 0xff0a8, d);
            SU32(act, 0xff0c0, U32(d, 0xf28));
        }
        if (I32(d, 0xf24) > best_won) { best_won = I32(d, 0xf24); SU32(G, 0x10, d); }
    }
    const int over = U32(act, 0xff0b4) == 0 ? !(alive > 1) : !(U32(G, 0x14) != 0 && U32(G, 0x18) != 0);
    if (!over || I32(act, 0x26c) <= 1) { rx_set_sp(F); ACT_END(F); return; }
    if (U32(act, 0xff0c4) == 0 || netsnake_live(act)) SU32(act, 0xff09c, U32(act, 0xff09c) + 1);
    if (U32(act, 0xff0b4) == 0) {
        rx_addr lead;
        if (U32(act, 0xff0a8) != 0) {
            if ((U32(act, 0xff0c4) == 0 || netsnake_live(act))) SU32(U32(act, 0xff0a8), 0xf24, U32(U32(act, 0xff0a8), 0xf24) + 1);
            const rx_addr w = U32(act, 0xff0a8);
            lead = U32(G, 0x10);
            if (I32(w, 0xf24) > I32(lead, 0xf24)) lead = w;
        } else lead = U32(G, 0x10);
        if (lead != 0 && I32(lead, 0xf24) >= I32(act, 0xff0a0)) { SU32(act, 0xff0a8, lead); SU32(act, 0xff0a4, 1); }
    } else {
        if (I32(act, 0xff0c0) != -1) {
            if (U32(act, 0xff0c0) != 0) SU32(act, 0xff0b8, U32(act, 0xff0b8) + 1);
            else SU32(act, 0xff0bc, U32(act, 0xff0bc) + 1);
        }
        if (I32(act, 0xff0b8) >= I32(act, 0xff0a0) || I32(act, 0xff0bc) >= I32(act, 0xff0a0)) SU32(act, 0xff0a4, 1);
    }
    if (netsnake_live(act) && U32(act, 0xff0a8) != 0) act_unreached(0x0056ccde, "ONLINE: the round end sent with the NetSnake object (006fda80)");
    ACT_CALL(2, rx_activity_set_state(act, 3));
    rx_set_sp(F);
    ACT_END(F);
}
RX_FUNCTION(0x0056cae0, rx_snake_vf42)

/* 0056be60 slot 43 (stdcall a, b, tie; RET 0xc; ECX unused) -> the standings' order: riders still in first (+0xf1c), then the more rounds
 * won (+0xf24), then (with tie) the start zones (+0x1dc of the players: a >= b gives 1, else -1). */
int32_t rx_snake_vf43(rx_addr act, rx_addr a, rx_addr b, uint32_t tie) {
    (void)act;
    int32_t r = (int32_t)U8(a, 0xf1c) - (int32_t)U8(b, 0xf1c);
    if (r != 0) return r;
    r = I32(b, 0xf24) - I32(a, 0xf24);
    if (r != 0) return r;
    if (tie == 0) return 0;
    return I32(U32(a, 0x74), 0x1dc) >= I32(U32(b, 0x74), 0x1dc) ? 1 : -1;
}
RX_FUNCTION(0x0056be60, rx_snake_vf43)

/* 0056bda0 slot 44 (thiscall; player; RET 4) -> a new SnakeDriver (0xfe0 bytes: 00513910) for the player: team -1 free for all, in team
 * mode (+0xff0b4) the drivers' count's parity; 0 when the allocation fails. Frame: push esi. */
rx_addr rx_snake_vf44(rx_addr act, rx_addr player) {
    ACT_FRAME(F, 0, 1);
    rx_addr p;
    ACT_CALLR(p, 1, rx_reflex_aligned_alloc(0xfe0));
    rx_addr d = 0;
    if (p != 0) {
        uint32_t team = 0xffffffffu;
        if (U32(act, 0xff0b4) != 0) { const int32_t n = I32(act, 0x26c); team = (uint32_t)(n % 2); }
        ACT_CALLR(d, 2, rx_snake_driver_construct(p, act, player, team));
    }
    ACT_END(F);
    return d;
}
RX_FUNCTION(0x0056bda0, rx_snake_vf44)

/* 0056be10 slot 47 (thiscall; driver; RET 4): the driver's rules: unless the boundary is water (+0xff0d0) RuleOutOfSnakeBounds (00589120),
 * then the free ride rule (00587690), each initialised with the driver (vf1, slot 4). Frame: push esi. */
void rx_snake_vf47(rx_addr act, rx_addr driver) {
    ACT_FRAME(F, 0, 1);
    const uint32_t a[1] = {driver};
    rx_addr rule;
    if (U32(act, 0xff0d0) == 0) {
        ACT_CALLR(rule, 0, rx_snake_bounds_rule_create(driver));
        (void)act_vcall(rule, 4, 1, a);
    }
    ACT_CALLR(rule, 0, rx_waypoint_get_free_ride_rule(driver));
    (void)act_vcall(rule, 4, 1, a);
    ACT_END(F);
}
RX_FUNCTION(0x0056be10, rx_snake_vf47)

/* ======================================================================================================================== */
/* the round                                                                                                                 */

/* 0056b5c0 (stdcall act; RET 4): the round's places. The scene's start entities of type 0x1017 named by the activity (+0x118: 007d2820),
 * the round's start zone (the activity row's MiniGameStartZone; ONLINE: the session's 006f7510); the activity's 28 starts reset to the
 * default place ([0xa95d50]) and direction ([0x9cb4a0]); the riders (both lists, rounded up to even; 12 when none) placed on a circle of 24
 * around the zone's entity, i * 360 / n degrees from its direction (the game's inlined sine / cosine), on the terrain (007c5950) + 2;
 * "Round Started" ({act, round + 1, the most rounds won of the standings, the rounds to win}) when the standings exist (+0x2d0); the team
 * wins cleared and the last team -1. Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0xd4; push ebx / esi / edi. */
void rx_snake_place_round(rx_addr act) {
    ACT_FRAME_EBP(F, 16, 0xd4, 3);
    const rx_addr scene = U32(U32(act, 0x234), 0x20138);
    for (uint32_t k = 0; k < 0x2c; k += 4) SU32(F + 0x94, k, 0);
    SU32(F, 0x3c, scene);
    SU32(F, 0x90, 0);
    ACT_CALL(6, (void)rx_scene_group_query(scene, 0x1017, F + 0x90, 0xc, act + 0x118, 0));
    int32_t row;
    ACT_CALLR(row, 1, rx_activity_find_row(ACT_ACTIVITY_TBL, 0xa, (int32_t)(int8_t)U8(act, 0x220)));
    SU32(act, 0xff0cc, 0);
    session_static();
    uint32_t zone;
    if (session_networked()) act_unreached(0x0056b67e, "ONLINE: the session's start zone (006f7510)");
    if (row == -1) zone = 0;
    else {
        uint32_t col;
        ACT_CALLR(col, 1, rx_tbl_column_index(ACT_ACTIVITY_TBL, 0x0093dfd4 /* "MiniGameStartZone" */));
        rx_addr cell;
        ACT_CALLR(cell, 1, rx_tbl_cell_word(ACT_ACTIVITY_TBL, col, (uint32_t)row));
        zone = U32(cell, 0);
    }
    SU32(act, 0xff0cc, zone);
    int32_t n = I32(act, 0x280) + I32(act, 0x26c);
    if (n % 2 != 0) n = n + 1;
    if (!(n > 0)) n = 0xc;
    SU32(F, 0x18, (uint32_t)n);
    const float step = rx_divs(360.0f /* 0x9776c0 */, (float)n);
    SF32(F, 0x4c, step);
    for (uint32_t k = 0; k < 0x1c; k++) {
        const rx_addr s = act + 0x340 + 0x30u * k;
        SF32(s, 0, F32(0, 0x00a95d50)); SF32(s, 4, F32(0, 0x00a95d54)); SF32(s, 8, F32(0, 0x00a95d58));
        SF32(s, 0x10, F32(0, 0x009cb4a0)); SF32(s, 0x14, F32(0, 0x009cb4a4)); SF32(s, 0x18, F32(0, 0x009cb4a8));
        SU32(s, 0x20, 0); SU32(s, 0x24, 0); SU32(s, 0x28, 0);
    }
    SU32(F, 0x1c, 0);
    for (int32_t i = 0; i < n; i++) {
        const rx_addr out = act + 0x358 + 0x30u * (uint32_t)i;   /* ESI: the start's direction z; position at -0x18, direction at -8 */
        const rx_addr start = U32(F + 0x90, 4 * U32(act, 0xff0cc));
        rx_addr m = act_vcall(start, 0x38, 0, 0);
        SF32(F, 0x50, F32(m, 0x30)); SF32(F, 0x54, F32(m, 0x34)); SF32(F, 0x58, F32(m, 0x38));
        m = act_vcall(start, 0x38, 0, 0);
        const float dy = F32(m, 0x24), dx = F32(m, 0x20), dz = F32(m, 0x28);
        SF32(F, 0x24, dy); SF32(F, 0x28, dz); SF32(F, 0x20, dx);
        const float len2 = rx_adds(rx_adds(rx_muls(dz, dz), rx_muls(dy, dy)), rx_muls(dx, dx));
        SF32(F, 0x44, len2);
        float one = 1.0f;   /* 0x931f24 */
        if (len2 > 0.0f) {
            float len;
            ACT_CALLR(len, 1, phys_sqrtf(x87_copy(len2)));
            const float inv = rx_divs(one, len);
            SF32(F, 0x20, rx_muls(F32(F, 0x20), inv));
            SF32(F, 0x24, rx_muls(F32(F, 0x24), inv));
            SF32(F, 0x28, rx_muls(F32(F, 0x28), inv));
        }
        const float angle = rx_muls(rx_muls((float)I32(F, 0x1c), F32(F, 0x4c)), rx_bf(0x3c8efa34u) /* pi / 180, 0x977678 */);
        float sign = one;
        SF32(F, 0x60, 0.0f); SF32(F, 0x64, one); SF32(F, 0x68, 0.0f);
        if (!(angle >= 0.0f)) sign = rx_bf(0xbf800000u);   /* -1, 0x9cd9e8 (comiss; jae: NaN takes -1) */
        const float x = rx_muls(sign, angle);
        const float t = b1_trig_reduce(x);
        const float t2 = rx_muls(t, t), t4 = rx_muls(t2, t2);
        const float cosv = rx_subs(rx_bf(0x80000000u) /* -0.0, 0x9774a8 */, b1_cos_poly(t2, t4));
        SF32(F, 0x40, cosv);
        const float sinv = rx_muls(rx_muls(b1_sin_poly(t2, t4), t), sign);
        SF32(F, 0x48, sinv);
        float a[3], b[3], c1[3], c2[3];
        a[0] = F32(F, 0x20); a[1] = F32(F, 0x24); a[2] = F32(F, 0x28);
        b[0] = F32(F, 0x60); b[1] = F32(F, 0x64); b[2] = F32(F, 0x68);
        b1_cross(a, b, c1);                                  /* Vec3_Cross 00401630 (EAX = the direction, ECX = up) -> F + 0xc0 */
        SF32(F, 0xc0, c1[0]); SF32(F, 0xc4, c1[1]); SF32(F, 0xc8, c1[2]);
        b1_cross(b, c1, c2);                                 /* (EAX = up, ECX = the first product) -> F + 0x70 */
        SF32(F, 0x70, c2[0]); SF32(F, 0x74, c2[1]); SF32(F, 0x78, c2[2]);
        const float k = rx_adds(rx_muls(rx_adds(F32(F, 0x28), F32(F, 0x20)), 0.0f), F32(F, 0x24));
        const float k0 = rx_muls(k, 0.0f);
        SF32(F, 0x84, k);
        SF32(F, 0x88, k0);
        const float ox = rx_adds(rx_adds(rx_muls(c2[0], cosv), rx_muls(c1[0], sinv)), k0);
        const float oy = rx_adds(rx_adds(rx_muls(c2[1], cosv), rx_muls(c1[1], sinv)), k);
        const float oz = rx_adds(rx_adds(rx_muls(c2[2], cosv), rx_muls(c1[2], sinv)), k0);
        SF32(out, -8, ox); SF32(out, -4, oy); SF32(out, 0, oz);
        SF32(F, 0x20, ox); SF32(F, 0x24, oy); SF32(F, 0x28, oz);
        const float r = 24.0f;   /* 0x97796c */
        SF32(out, -0x18, rx_adds(rx_muls(ox, r), F32(F, 0x50)));
        SF32(out, -0x14, rx_adds(rx_muls(oy, r), F32(F, 0x54)));
        SF32(out, -0x10, rx_adds(rx_muls(oz, r), F32(F, 0x58)));
        const rx_addr terrain = scene_terrain(U32(F, 0x3c));
        uint32_t edx;
        ACT_CALL(1, (void)phys_terrain_base_height_at(terrain, out - 0x18, &edx));
        SF32(out, -0x14, rx_adds(F32(out, -0x14), 2.0f /* 0x92e330 */));
        SU32(F, 0x1c, (uint32_t)(i + 1));
    }
    if (U32(act, 0x2d0) != 0) {
        int32_t best = 0;
        for (int32_t i = 0; i < I32(act, 0x26c); i++) {
            const int32_t won = I32(rx_stand(act, (uint32_t)i), 0xf24);
            if (won > best) best = won;
        }
        SU32(F, 0xc8, (uint32_t)best);
        SU32(F, 0xc4, U32(act, 0xff09c) + 1);
        SU32(F, 0xc0, act);
        SU32(F, 0xcc, U32(act, 0xff0a0));
        act_raise(U32(act, 0x290), STR_ROUND_STARTED, TD_ROUND_STARTED, F + 0xc0, 0x10);
    }
    SU32(act, 0xff0bc, 0);
    SU32(act, 0xff0b8, 0);
    SU32(act, 0xff0c0, 0xffffffffu);
    ACT_END(F);
}
RX_FUNCTION(0x0056b5c0, rx_snake_place_round)

/* 0056cd20 (ESI = act): the boundary of the round's start zone: its SnakeAreaRadius (+0xa0) into +0xff0dc, its UseWaterAsBoundary (+0xa4)
 * into +0xff0d0 (and its negation +0xff19c), its position (vf17 slot 0x44) into +0xff0e0; under the profiler scope a live NetSnake object
 * gets the radius (ONLINE). Frame: push ebp; mov ebp, esp; and esp, -8; sub esp, 0x38; push ebx / edi (the list F + 0x10). */
void rx_snake_boundary_reset(rx_addr act) {
    ACT_FRAME_EBP(F, 8, 0x38, 2);
    for (uint32_t k = 0; k < 0x2c; k += 4) SU32(F + 0x14, k, 0);
    SU32(F, 0x10, 0);
    ACT_CALL(6, (void)rx_scene_group_query(U32(U32(act, 0x234), 0x20138), 0x1017, F + 0x10, 0xc, act + 0x118, 0));
    const rx_addr zone = U32(F + 0x10, 4 * U32(act, 0xff0cc));
    SF32(act, 0xff0dc, x87_copy(F32(zone, 0xa0)));
    const uint32_t water = U32(zone, 0xa4);
    SU32(act, 0xff0d0, water);
    SU32(act, 0xff19c, water == 0 ? 1 : 0);
    { const uint32_t a[1] = {act + 0xff0e0}; (void)act_vcall(zone, 0x44, 1, a); }
    session_static();
    const rx_addr scope = race_profile_begin();
    if (netsnake_live(act)) act_unreached(0x0056ce27, "ONLINE: the boundary's radius sent (the NetSnake object)");
    race_profile_end(scope);
    ACT_END(F);
}
RX_FUNCTION(0x0056cd20, rx_snake_boundary_reset)

/* 0056d080 (EDI = act; stdcall dt; RET 4): the boundary shrinks by BoundaryShrinkRate * 0.5 * dt while above 1 (unless the debug freeze
 * [0xd92f08]); +0xff1c0 the radius; under the profiler scope a live NetSnake object gets the radius every 3 s (ONLINE). Frame: push ebx /
 * esi. */
void rx_snake_boundary_shrink(rx_addr act, float dt) {
    ACT_FRAME(F, 0, 2);
    const float r = rx_subs(F32(act, 0xff0dc), rx_muls(rx_muls(F32(act, 0xff0f0), 0.5f /* 0x9322cc */), dt));
    if (r > 1.0f /* 0x931f24 */) {
        if (U32(0, 0x00d92f08) == 0) SF32(act, 0xff0dc, r);
        SF32(act, 0xff1c0, x87_copy(F32(act, 0xff0dc)));
    }
    session_static();
    const rx_addr scope = race_profile_begin();
    if (netsnake_live(act)) act_unreached(0x0056d149, "ONLINE: the boundary's radius sent every 3 s");
    race_profile_end(scope);
    ACT_END(F);
}
RX_FUNCTION(0x0056d080, rx_snake_boundary_shrink)

/* 0056cf80 (EDI = act; stdcall dt; RET 4): a water boundary (the start zone's UseWaterAsBoundary) rises: the rise clock +0xff0d4 grows by
 * dt up to +0xff0d8 (60 s) and the scene's water entities take the fraction (clamped to 0 .. 1: 007f0200 on the water set 0xd72aac). Offline
 * the round's zone is always the activity row's MiniGameStartZone (0056b5c0), one per Snake level (EventTypeLocations: FR_Alpine /
 * FR_Cheyenne / FR_Gibson, track 0); none of the three has the flag (party_snake_test host and stop_audit_run fuzz on each level run
 * the shrink, never this), so the water's level setter 007f0200 / 007ee7b0 is not ported. Frame: push ebx / esi. */
void rx_snake_water_rise(rx_addr act, float dt) {
    (void)act; (void)dt;
    act_unreached(0x0056cf80, "a Snake start zone with UseWaterAsBoundary (the water's rise 007f0200; no shipped zone has it)");
}
RX_FUNCTION(0x0056cf80, rx_snake_water_rise)

/* ======================================================================================================================== */
/* SnakeActivityAudioLogic                                                                                                   */

#define ACH(l) U32(U32((l), 0x50), 0x290)   /* the logic's activity's channel */
static int cue_present(rx_addr logic, rx_addr L, rx_addr name) {
    const rx_addr s = L + 0x1c, it = L + 0x14;
    crt_str_ctor_cstr(s, name);
    const rx_addr map = U32(logic, 0x8c);
    const rx_addr end = U32(map, 0x18), proxy = U32(map, 0);
    rx_addr r;
    SU32(L, -4, s);
    SU32(L, -8, it);
    ACT_CALLR(r, 2, rx_world_string_map_find(map, it, s));
    if (U32(r, 0) == 0 || U32(r, 0) != proxy) crt_invalid_parameter_noinfo();
    int found = U32(r, 4) != end;
    crt_str_dtor(s);
#ifndef RX_ORACLE
    if (!found) found = rx_activity_cues_present(logic, name);   /* w-sfx: the level's cues (activity_cues.c) */
#endif
    return found;
}
/* the listener templates: 0058bf10 "Round Started" (00584df0, proxy 0x945f64), 0058bfd0 (ECX = channel, EAX = name; stdcall logic, method:
 * void(Activity*), proxy 0x945f84), 0058c090 "Player Eliminated" (00583a70, proxy 0x945fa4: void(Activity*, BaseDriver*, BaseDriver*)),
 * 0058c150 "Player Wins" (00583fb0, proxy 0x945fc4), 0058c210 "SnakeTrailHit" (00583a90, proxy 0x945fe4: void(Activity*, BaseDriver*,
 * const Vector3&)), and their leaves 0058c2d0 / 0058c330 / 0058c390 / 0058c3f0 / 0058c450 */
void rx_snake_audio_listen_round_started(rx_addr channel, rx_addr logic) { act_subscribe(0x0058bf10, 1, 1, 4, channel, STR_ROUND_STARTED, 0x00945f64, TD_ROUND_STARTED, logic, 0x00584df0); }
RX_FUNCTION(0x0058bf10, rx_snake_audio_listen_round_started)
void rx_snake_audio_listen_activity(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method) { act_subscribe(0x0058bfd0, 2 | 4, 2, 3, channel, name, 0x00945f84, TD_ACTIVITY, logic, method); }
RX_FUNCTION(0x0058bfd0, rx_snake_audio_listen_activity)
void rx_snake_audio_listen_eliminated(rx_addr channel, rx_addr logic) { act_subscribe(0x0058c090, 1, 1, 4, channel, STR_ELIMINATED, 0x00945fa4, 0x009cdaa8, logic, 0x00583a70); }
RX_FUNCTION(0x0058c090, rx_snake_audio_listen_eliminated)
void rx_snake_audio_listen_player_wins(rx_addr channel, rx_addr logic) { act_subscribe(0x0058c150, 1, 1, 4, channel, STR_PLAYER_WINS, 0x00945fc4, 0x009cf630, logic, 0x00583fb0); }
RX_FUNCTION(0x0058c150, rx_snake_audio_listen_player_wins)
void rx_snake_audio_listen_trail_hit(rx_addr channel, rx_addr logic) { act_subscribe(0x0058c210, 1, 1, 4, channel, STR_TRAIL_HIT, 0x00945fe4, 0x009cdb30, logic, 0x00583a90); }
RX_FUNCTION(0x0058c210, rx_snake_audio_listen_trail_hit)
void rx_snake_audio_unlisten_round_started(rx_addr channel, rx_addr logic) { act_unlisten(0, channel, STR_ROUND_STARTED, 0x00945f64, TD_ROUND_STARTED, logic, 0x00584df0); }
RX_FUNCTION(0x0058c2d0, rx_snake_audio_unlisten_round_started)
void rx_snake_audio_unlisten_activity(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method) { act_unlisten(0, channel, name, 0x00945f84, TD_ACTIVITY, logic, method); }
RX_FUNCTION(0x0058c330, rx_snake_audio_unlisten_activity)
void rx_snake_audio_unlisten_eliminated(rx_addr channel, rx_addr logic) { act_unlisten(0, channel, STR_ELIMINATED, 0x00945fa4, 0x009cdaa8, logic, 0x00583a70); }
RX_FUNCTION(0x0058c390, rx_snake_audio_unlisten_eliminated)
void rx_snake_audio_unlisten_player_wins(rx_addr channel, rx_addr logic) { act_unlisten(0, channel, STR_PLAYER_WINS, 0x00945fc4, 0x009cf630, logic, 0x00583fb0); }
RX_FUNCTION(0x0058c3f0, rx_snake_audio_unlisten_player_wins)
void rx_snake_audio_unlisten_trail_hit(rx_addr channel, rx_addr logic) { act_unlisten(0, channel, STR_TRAIL_HIT, 0x00945fe4, 0x009cdb30, logic, 0x00583a90); }
RX_FUNCTION(0x0058c450, rx_snake_audio_unlisten_trail_hit)

/* 00583a70 ("Player Eliminated": the RaceActivityAudioLogic handler, shared) is rx_race_audio_eliminated (race_activity_audio.c). */
/* 00583a90 (thiscall logic; activity, driver, position; RET 0xc): the "SnakeTrailHit" cue with the driver and the position */
void rx_snake_audio_trail_hit(rx_addr logic, rx_addr act, rx_addr driver, rx_addr pos) { (void)act; RACE_CORE_CALL(4, rx_audio_cue(logic, STR_TRAIL_HIT, driver, pos)); }
RX_FUNCTION(0x00583a90, rx_snake_audio_trail_hit)

/* 00583550 slot 3 (ECX = logic): the cue handlers listen ("Round Started", "Round Over", "Match Over", "Player Eliminated", "Player Wins",
 * "SnakeTrailHit") when the level's cue map holds the cue. Frame: push ebp; mov ebp, esp; and esp, -8; sub esp, 0x28; push ebx / ebp /
 * esi / edi. */
void rx_snake_audio_vf3(rx_addr logic) {
    ACT_FRAME_EBP(L, 8, 0x28, 4);
    if (U32(logic, 0x8c) != 0) {
        if (cue_present(logic, L, STR_ROUND_STARTED)) ACT_CALL(1, rx_snake_audio_listen_round_started(ACH(logic), logic));
        if (cue_present(logic, L, STR_ROUND_OVER)) ACT_CALL(2, rx_snake_audio_listen_activity(ACH(logic), STR_ROUND_OVER, logic, 0x00583a30));
        if (cue_present(logic, L, STR_MATCH_OVER)) ACT_CALL(2, rx_snake_audio_listen_activity(ACH(logic), STR_MATCH_OVER, logic, 0x00583a50));
        if (cue_present(logic, L, STR_ELIMINATED)) ACT_CALL(1, rx_snake_audio_listen_eliminated(ACH(logic), logic));
        if (cue_present(logic, L, STR_PLAYER_WINS)) ACT_CALL(1, rx_snake_audio_listen_player_wins(ACH(logic), logic));
        if (cue_present(logic, L, STR_TRAIL_HIT)) ACT_CALL(1, rx_snake_audio_listen_trail_hit(ACH(logic), logic));
    }
    ACT_END(L);
}
RX_FUNCTION(0x00583550, rx_snake_audio_vf3)

/* 005837c0 slot 4 (ECX = logic): the cue handlers leave. Same frame as vf3. */
void rx_snake_audio_vf4(rx_addr logic) {
    ACT_FRAME_EBP(L, 8, 0x28, 4);
    if (U32(logic, 0x8c) != 0) {
        if (cue_present(logic, L, STR_ROUND_STARTED)) ACT_CALL(1, rx_snake_audio_unlisten_round_started(ACH(logic), logic));
        if (cue_present(logic, L, STR_ROUND_OVER)) ACT_CALL(2, rx_snake_audio_unlisten_activity(ACH(logic), STR_ROUND_OVER, logic, 0x00583a30));
        if (cue_present(logic, L, STR_MATCH_OVER)) ACT_CALL(2, rx_snake_audio_unlisten_activity(ACH(logic), STR_MATCH_OVER, logic, 0x00583a50));
        if (cue_present(logic, L, STR_ELIMINATED)) ACT_CALL(1, rx_snake_audio_unlisten_eliminated(ACH(logic), logic));
        if (cue_present(logic, L, STR_PLAYER_WINS)) ACT_CALL(1, rx_snake_audio_unlisten_player_wins(ACH(logic), logic));
        if (cue_present(logic, L, STR_TRAIL_HIT)) ACT_CALL(1, rx_snake_audio_unlisten_trail_hit(ACH(logic), logic));
    }
    ACT_END(L);
}
RX_FUNCTION(0x005837c0, rx_snake_audio_vf4)
