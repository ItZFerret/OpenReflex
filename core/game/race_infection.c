/* race_infection.c -- MXActivityInfection (the Infection party mode, activity type 0xe; vtable 0x00944c74; 0xff1f0 bytes), its
 * InfectionDriver (vtable 0x0093feb4, 0x11c0 bytes), RuleInfection (vtable 0x0093ff00) and InfectionActivityAudioLogic (vtable 0x009430bc),
 * hand-transliterated from the original (the address of each function is in its comment; frames and the stack depth of each call are
 * the original's).
 *
 * The game: one rider starts infected (the round's index modulo the riders); an infected rider who comes within 12 ft (RuleInfection:
 * the distance^2 below 144) of a healthy one infects it (+0xf14) and scores 100 / (riders - 1) (InfectionDriver::vf9); a healthy rider
 * scores 100 / +0x2bc per second of the round. A circular boundary (the start zone's SnakeAreaRadius around its position) shrinks by
 * BoundaryShrinkRate / 2 per second; a human outside it for 5 s (RuleOutOfSnakeBounds, kind 9: race_snake_rules.c) is infected too.
 * The round ends when at most one rider is healthy (vf42): that rider wins the round (+0xf18); +0xff084 (4) rounds win the match; 3 s of
 * WindDown (vf27) restart the next round (vf16 / vf15). The boundary is drawn by the activity's XenonSnakeWallVisibleMesh (+0xff0c0,
 * race_snake_wall.c: the ring rebuilt at each round's start). The infected rider's smoke is a PlaceableParticleEffect of its driver.
 *
 * ONLINE (a Quazal session: [0x00da6cb0] + 0x1c4c set and the mode +0x1b0 not 1; none exists in the product): the network snake object
 * (+0xff1e0: the boundary's radius sent every 3 s), the session's start zones and infected rider, the drivers' network objects (+0xef0:
 * the score), the host's round end. Each networked branch is marked "ONLINE" and stops with act_unreached. */
#include "race_snake.h"
#include "race_activity.h"
#include "race_activity_misc.h"
#include "race_activity_ui.h"
#include "race_finish.h"
#include "race_driver.h"
#include "race_misc.h"
#include "race_extern.h"
#include "race_champ.h"
#include "race.h"
#include "race_challenge_activity.h"
#include "build_activity.h"
#include "build_activity_int.h"
#include "build_activity_teardown.h"
#include "build_activity_freeride_run.h"
#include "build_driver_reset.h"
#include "build_driver_dtor.h"
#include "build_vehicle.h"
#include "build_alloc.h"
#include "waypoint_activity.h"
#include "waypoint_driver.h"
#include "loop_vehicle.h"
#include "phys_terrain.h"
#include "phys_crt.h"
#include "phys_body.h"
#include "world_misc.h"
#include "world_wreck.h"
#include "activity_cues.h"
#include "anim.h"
#include "race_beh1_extern.h"
#include "loop_platform.h"
#include "race_tag.h"
#include "fx.h"
#include "anim_entity_dtors.h"
#include "race_net_session.h"
#include "crt.h"
#include "../src/rx_os.h"
#include <string.h>
#include "rider_cap.h"

#define VT_INFECTION        0x00944c74u
#define VT_INFECTION_AUDIO  0x009430bcu
#define VT_INFECTION_DRIVER 0x0093feb4u
#define VT_RULE_INFECTION   0x0093ff00u
#define VT_AUDIO_LOGIC      0x009429fcu
#define TD_ACTIVITY         0x009cbd24u   /* void(Activity*) */
#define TD_STRING           0x009cdb68u   /* void(const char*) */
#define TD_ROUND_STARTED    0x009cf680u   /* void(Activity*, int, int, int) */
#define STR_ROUND_STARTED   0x009421b0u
#define STR_ROUND_OVER      0x009421c0u
#define STR_MATCH_OVER      0x009421ccu
#define STR_INFECTED        0x0093efe0u   /* "PlayerInfected" */
#define STR_ELIMINATED      0x0093ef48u   /* "Player Eliminated" */
#define STR_PLAYER_WINS     0x00941ca8u
#define WALL(A)             ((A) + 0xff0c0u)

static void session_static(void) {
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
}
static int session_networked(void) {
    const rx_addr s = U32(0, 0x00da6cb0);
    return U32(s, 0x1c4c) != 0 && U32(s, 0x1b0) != 1;
}
/* fld dword / fstp dword: the bits, a signalling NaN quieted */
static float x87_copy(float v) {
    uint32_t b = rx_fb(v);
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu)) b |= 0x400000u;
    return rx_bf(b);
}
/* the terrain of a scene: the registry 0x9e30d4 (count 0x9e30dc, initialised under the guard bit 0x9e30e0) */
static rx_addr scene_terrain(rx_addr scene) {
    if (!(U8(0, 0x009e30e0) & 1)) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
    for (uint32_t i = 0; i < U32(0, 0x009e30dc); i++) {
        const rx_addr t = U32(0, 0x009e30d4 + 4 * i);
        if (t != 0 && U32(t, 0x44a8) == scene) return t;
    }
    return 0;
}
/* the driver's network object is live (+0xef0 set, its flag +0x20 bit 0, 00884950): ONLINE only */
static int net_object_live(rx_addr d) {
    const rx_addr net = U32(d, 0xef0);
    return net != 0 && (U8(net, 0x20) & 1) != 0 && (rx_net_ref_valid(net) & 0xff) != 0;
}
/* the engine registry's float (the registry [0xa95844] + 0xc: vf1 finds "MXGame\\SnakeActivity" / key, vf17 reads one float) */
static void registry_float(rx_addr key, rx_addr out) {
    const rx_addr reg = U32(U32(0, 0x00a95844), 0xc);
    const uint32_t find[3] = {0x00942184 /* "MXGame\\SnakeActivity" */, key, 1};
    const rx_addr rec = act_vcall(reg, 4, 3, find);
    if (rec != 0) { const uint32_t read[3] = {rec, out, 1}; (void)act_vcall(reg, 0x44, 3, read); }
}
static inline void os_cs(uint32_t slot, rx_addr cs) { const uint32_t a[1] = {cs}; (void)rx_os_import(slot, 1, a); }

/* ======================================================================================================================== */
/* construction / destruction                                                                                                */

/* 00572d00 (stack act; ECX = type, EAX = world; RET 4) -> act: Activity's constructor, the boundary wall (0057ebb0: red {1, 0, 0},
 * 200 segments) at +0xff0c0, the class's fields, a new InfectionActivityAudioLogic (0x98 bytes) at +0xfd8, the radius 300; the wall added
 * to the scene twice (006a0e20, then again inline: two ids). Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x14; push ebx / esi /
 * edi (the colour at F + 0x10). */
rx_addr rx_infection_construct(rx_addr act, uint32_t type, rx_addr world) {
    ACT_FRAME_EBP(F, 16, 0x14, 3);
    ACT_CALL(1, rx_activity_construct(act, type, world));
    SF32(F, 0x10, 1.0f /* 0x931f24 */);
    SF32(F, 0x14, 0.0f);
    SF32(F, 0x18, 0.0f);
    SU32(act, 0, VT_INFECTION);
    ACT_CALL(3, (void)rx_snake_wall_construct(WALL(act), F + 0x10, 200));
    SU32(act, 0x2c8, 1);
    SU32(act, 0xff080, 0);
    SU32(act, 0xff084, 4);
    SU32(act, 0xff088, 0);
    SU32(act, 0xff094, 1);
    SU32(act, 0xff070, 0);
    SF32(act, 0xff074, 0.0f);
    SF32(act, 0xff078, 0.0f);
    SU32(act, 0xff1e0, 0);
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
        SU32(logic, 0, VT_INFECTION_AUDIO);
    }
    SF32(act, 0xff098, 300.0f /* 0x935d28 */);
    SU32(act, 0xfd8, logic);
    SF32(act, 0xff0a0, 0.0f); SF32(act, 0xff0a4, 0.0f); SF32(act, 0xff0a8, 0.0f);
    SF32(act, 0xff0b4, 0.0f);
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
    ACT_END(F);
    return act;
}
RX_FUNCTION(0x00572d00, rx_infection_construct)

/* 00572e70 slot 0 (thiscall; flags; RET 4) -> act: the wall's destructor body (0057f020), Activity's destructor (005566b0), freed with
 * flags & 1. Frame: push esi / edi. */
rx_addr rx_infection_sdtor(rx_addr act, uint32_t flags) {
    ACT_FRAME(F, 0, 2);
    SU32(act, 0, VT_INFECTION);
    ACT_CALL(0, rx_snake_wall_destroy(WALL(act)));
    ACT_CALL(0, rx_waypoint_activity_base_destroy(act));
    if (flags & 1) ACT_CALL(1, rx_reflex_aligned_free(act));
    ACT_END(F);
    return act;
}
RX_FUNCTION(0x00572e70, rx_infection_sdtor)

/* ======================================================================================================================== */
/* the listeners (as race_tag.c's templates)                                                                                 */

/* 00589bd0 (EAX = the session channel; stdcall act; RET 4): "NetworkObjectRemoved" with 005742e0 (proxy 0x945be4) */
void rx_infection_net_removed_listen(rx_addr channel, rx_addr act) {
    ACT_CALL(1, act_subscribe(0x00589bd0, 1, 1, 2, channel, 0x0093ee98 /* "NetworkObjectRemoved" */, 0x00945be4, 0x009cdc7c, act, 0x005742e0));
}
RX_FUNCTION(0x00589bd0, rx_infection_net_removed_listen)
/* 00589c90: "NetworkSnakeBoundaryChanged" with 00574610 (proxy 0x945c04: void(float)) */
void rx_infection_net_boundary_listen(rx_addr channel, rx_addr act) {
    ACT_CALL(1, act_subscribe(0x00589c90, 1, 1, 2, channel, 0x00942128 /* "NetworkSnakeBoundaryChanged" */, 0x00945c04, 0x009d00dc, act, 0x00574610));
}
RX_FUNCTION(0x00589c90, rx_infection_net_boundary_listen)
/* 00589d50 / 00589db0: the two listeners leave (frame: push ebp; mov ebp, esp; and esp, -8; sub esp, 0x14; push esi) */
void rx_infection_net_removed_unlisten(rx_addr channel, rx_addr act) {
    act_unlisten(1, channel, 0x0093ee98, 0x00945be4, 0x009cdc7c, act, 0x005742e0);
}
RX_FUNCTION(0x00589d50, rx_infection_net_removed_unlisten)
void rx_infection_net_boundary_unlisten(rx_addr channel, rx_addr act) {
    act_unlisten(1, channel, 0x00942128, 0x00945c04, 0x009d00dc, act, 0x00574610);
}
RX_FUNCTION(0x00589db0, rx_infection_net_boundary_unlisten)

/* 00589e10 (ESI = the activity's channel, ECX = the winner, EDX = the text; stdcall act; RET 4): "Player Wins" raised with
 * {act, winner, text, 1} (void(Activity*, BaseDriver*, const char*, int): 0x9cf708). Frame: sub esp, 0x14; push edi (the payload F + 4). */
void rx_party_player_wins(rx_addr manager, rx_addr winner, rx_addr text, rx_addr act) {
    ACT_FRAME(F, 0x14, 1);
    SU32(F, 8, winner);
    SU32(F, 4, act);
    SU32(F, 0xc, text);
    SU32(F, 0x10, 1);
    act_raise(manager, STR_PLAYER_WINS, 0x009cf708, F + 4, 0x10);
    ACT_END(F);
}
RX_FUNCTION(0x00589e10, rx_party_player_wins)

/* ======================================================================================================================== */
/* the activity's virtuals                                                                                                   */

/* 00572eb0 slot 2 (thiscall; arg; RET 4): Activity::vf2, the two network listeners, the session's snake object (ONLINE: the host's
 * 008879b0 / 00887780), the registry's "BoundaryWallPolyWidth" (+0xff0b8), the round 0, the round's places (00573450), the boundary
 * (00574370), "BoundaryShrinkRate" (+0xff0b0), "Activity Init" raised. Frame: push ebx / esi / edi. */
void rx_infection_vf2(rx_addr act, rx_addr arg) {
    ACT_FRAME(F, 0, 3);
    ACT_CALL(1, rx_activity_vf2(act, arg));
    session_static();
    ACT_CALL(1, rx_infection_net_removed_listen(U32(0, 0x00da6ca0), act));
    session_static();
    ACT_CALL(1, rx_infection_net_boundary_listen(U32(0, 0x00da6ca0), act));
    session_static();
    if (session_networked()) {
        session_static();
        if (U32(U32(0, 0x00da6cb0), 0x1c48) != 0) act_unreached(0x00572f82, "ONLINE: the host's network snake object (008879b0 / 00887780)");
    }
    registry_float(0x0094216c /* "BoundaryWallPolyWidth" */, act + 0xff0b8);
    SU32(act, 0xff080, 0);
    ACT_CALL(1, rx_infection_place_round(act));
    ACT_CALL(0, rx_infection_boundary_reset(act));
    registry_float(0x0094219c /* "BoundaryShrinkRate" */, act + 0xff0b0);
    SU32(F, 0x10, act);   /* the payload: the argument slot */
    act_raise(U32(0, 0x00a93f74), 0x00935ea4 /* "Activity Init" */, TD_ACTIVITY, F + 0x10, 4);
    ACT_END(F);
}
RX_FUNCTION(0x00572eb0, rx_infection_vf2)

/* 00573080 slot 3 (ECX = activity): every driver of the two lists leaves the activity, the two listeners leave, the snake object
 * unlinked, then a jump to Activity::vf3. Frame: push ebx / esi. */
void rx_infection_vf3(rx_addr act) {
    ACT_FRAME(F, 0, 2);
    while (I32(act, 0x26c) > 0) ACT_CALL(2, rx_activity_on_player_left(act, U32(U32(act, 0x268), 0)));
    while (I32(act, 0x280) > 0) ACT_CALL(2, rx_activity_on_player_left(act, U32(U32(act, 0x27c), 0)));
    session_static();
    ACT_CALL(1, rx_infection_net_removed_unlisten(U32(0, 0x00da6ca0), act));
    session_static();
    ACT_CALL(1, rx_infection_net_boundary_unlisten(U32(0, 0x00da6ca0), act));
    SU32(act, 0xff1e0, 0);
    ACT_END(F);
    rx_activity_vf3(act);
}
RX_FUNCTION(0x00573080, rx_infection_vf3)

/* 00573140 slot 4 (ECX = activity): the class's event types on the activity's channel (Activity::vf4 is not called): "Round Started"
 * (005925e0), "Round Over" / "Match Over" (00590c00), "PlayerInfected" (00591500). Frame: push esi. */
void rx_infection_vf4(rx_addr act) {
    ACT_FRAME(F, 0, 1);
    act_event_type(0x005925e0, U32(act, 0x290), 0);
    act_event_type(0x00590c00, U32(act, 0x290), STR_ROUND_OVER);
    act_event_type(0x00590c00, U32(act, 0x290), STR_MATCH_OVER);
    act_event_type(0x00591500, U32(act, 0x290), STR_INFECTED);
    ACT_END(F);
}
RX_FUNCTION(0x00573140, rx_infection_vf4)

/* 00573180 slot 15 (ECX = activity): the round's start. The places (00573450), Activity::vf15, the boundary (00574370); the round's first
 * infected (the driver round % riders; ONLINE: the driver whose player's network id is that); the scores per second (100 / +0x2bc) and per
 * infection (100 / (riders - 1)); the boundary's centre on the terrain (007c5950) and the wall's ring built (0057fc00: the radius, the
 * poly width); +0xff0b4 = 0. Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x14; push ebx / esi / edi (the centre F + 0x10). */
void rx_infection_vf15(rx_addr act) {
    ACT_FRAME_EBP(F, 16, 0x14, 3);
    ACT_CALL(1, rx_infection_place_round(act));
    ACT_CALL(0, rx_activity_vf15(act));
    ACT_CALL(0, rx_infection_boundary_reset(act));
    session_static();
    if (session_networked()) act_unreached(0x005731db, "ONLINE: the round's first infected by network id");
    {   const int32_t n = I32(act, 0x26c), r = I32(act, 0xff080);
        if (n == 0) act_unreached(0x0057321a, "the round's first infected of no driver (integer division by zero)");
        SU8(U32(U32(act, 0x268), 4u * (uint32_t)(r % n)), 0xf14, 1); }
    SF32(act, 0xff078, rx_divs(100.0f /* 0x93005c */, F32(act, 0x2bc)));
    SF32(act, 0xff074, rx_divs(100.0f, (float)(I32(act, 0x26c) - 1)));
    SF32(F, 0x10, F32(act, 0xff0a0));
    SF32(F, 0x14, F32(act, 0xff0a4));
    SF32(F, 0x18, F32(act, 0xff0a8));
    const rx_addr terrain = scene_terrain(U32(U32(act, 0x234), 0x20138));
    uint32_t edx;
    ACT_CALL(1, (void)phys_terrain_base_height_at(terrain, F + 0x10, &edx));
    const float radius = x87_copy(F32(act, 0xff098)), width = x87_copy(F32(act, 0xff0b8));
    rx_set_sp(F - 8);
    SF32(F - 8, 4, width);
    SF32(F - 8, 0, radius);
    ACT_CALL(0, (void)rx_snake_wall_build_ring(WALL(act), F + 0x10, radius, width));
    rx_set_sp(F);
    SF32(act, 0xff0b4, 0.0f);
    ACT_END(F);
}
RX_FUNCTION(0x00573180, rx_infection_vf15)

/* 00573330 slot 16 (ECX = activity): a new round. A new match (+0xff094) clears every driver's rounds won (+0xf18), the match state and
 * the round; Activity::vf16; every driver takes its start zone (0054cd20; ONLINE: the session's 006f7330) and its start place (+0x144 =
 * the activity's +0x340 + 0x30 * zone); +0xff07c / +0xff094 cleared. Frame: push ebp; mov ebp, esp; and esp, -8; push ebx / ebp / esi /
 * edi. */
void rx_infection_vf16(rx_addr act) {
    ACT_FRAME_EBP(F, 8, 0, 4);
    if (U32(act, 0xff094) != 0) {
        for (int32_t i = 0; i < I32(act, 0x26c); i++) SU32(U32(U32(act, 0x268), 4u * (uint32_t)i), 0xf18, 0);
        SU32(act, 0xff088, 0);
        SU32(act, 0xff080, 0);
    }
    ACT_CALL(0, rx_activity_vf16(act));
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        const rx_addr d = U32(U32(act, 0x268), 4u * (uint32_t)i);
        session_static();
        if (session_networked()) act_unreached(0x005733d1, "ONLINE: the session's start zone of a player (006f7330)");
        ACT_CALL(1, rx_player_start_zone(U32(d, 0x74)));
        SU32(d, 0x144, act + 0x340 + 0x30u * U32(U32(d, 0x74), 0x1dc));
    }
    SU32(act, 0xff07c, 0);
    SU32(act, 0xff094, 0);
    ACT_END(F);
}
RX_FUNCTION(0x00573330, rx_infection_vf16)

/* 00573ab0 slot 18 (ECX = activity): the round's reset: the winner, every driver's infection and the last infected cleared, then a jump to
 * Activity::vf18. */
void rx_infection_vf18(rx_addr act) {
    SU32(act, 0xff08c, 0);
    for (int32_t i = 0; i < I32(act, 0x26c); i++) SU8(U32(U32(act, 0x268), 4u * (uint32_t)i), 0xf14, 0);
    SU32(act, 0xff07c, 0);
    rx_activity_vf18(act);
}
RX_FUNCTION(0x00573ab0, rx_infection_vf18)

/* 00573d90 slot 23 (thiscall; dt; RET 4): Activity::vf23, the boundary's shrink (005744c0), then under the engine's profiler scope the
 * session's snake object (ONLINE: found or made, its update 00555540 / 005088c0). Frame: push ebp; mov ebp, esp; and esp, -8;
 * sub esp, 0x24; push ebx / esi / edi; dt through x87. */
void rx_infection_vf23(rx_addr act, float dt) {
    ACT_FRAME_EBP(F, 8, 0x24, 3);
    ACT_CALL(1, rx_activity_update(act, x87_copy(dt)));
    ACT_CALL(1, rx_infection_boundary_shrink(act, x87_copy(dt)));
    session_static();
    const rx_addr scope = race_profile_begin();
    if (U32(act, 0xff1e0) == 0) {
        session_static();
        if (session_networked()) act_unreached(0x00573e72, "ONLINE: the session's network snake object (006e4fa0 / 008879b0)");
    }
    if (U32(act, 0xff1e0) != 0) act_unreached(0x00573f4e, "ONLINE: the network snake object's update (00555540 / 005088c0)");
    race_profile_end(scope);
    ACT_END(F);
}
RX_FUNCTION(0x00573d90, rx_infection_vf23)

/* 00573f90 slot 25 (MXActivityCapture::vf25, shared by Infection; thiscall dt; RET 4): Activity::vf25 (the intro), then the UI route of
 * the HUD: "GotoSpectatorSinglePlayer" for a spectator (+0xfe8) else "GotoCaptureSinglePlayer" (UIProperty_SetString "UIRoutingMessage"),
 * and the UI manager's "GotoHub" event raised on its channel. UI BOUNDARY (as race_tag.c's vf25): the product has no UI manager
 * ([0xa93fcc] = 0). Frame: push ebp; mov ebp, esp; and esp, -8; push ecx; push ebx / esi / edi (the payload F + 0xc); dt through x87. */
void rx_party_vf25(rx_addr act, float dt) {
    ACT_FRAME_EBP(F, 8, 4, 3);
    ACT_CALL(1, rx_activity_pre(act, x87_copy(dt)));
    const rx_addr ui = U32(0, 0x00a93fcc);
    rx_regs in, out;
    memset(&in, 0, sizeof in);
    in.r[6] = ui;
    in.r[7] = 0x0094cc74;   /* "UIRoutingMessage" */
    const uint32_t v[1] = {U32(act, 0xfe8) != 0 ? 0x009421d8u /* "GotoSpectatorSinglePlayer" */ : 0x00942320u /* "GotoCaptureSinglePlayer" */};
    act_ui_call_ex(0x005dbf50, &in, &out, 1, v);
    in.r[7] = 0x0094cc74;
    const uint32_t h[1] = {0x009424ec /* "GotoHub" */};
    act_ui_call_ex(0x005db500, &in, &out, 1, h);
    if (out.r[0] != 0) {
        SU32(F, 0xc, out.r[0]);
        act_raise(U32(ui, 0x98), out.r[0], TD_STRING, F + 0xc, 4);
    }
    ACT_END(F);
}
RX_FUNCTION(0x00573f90, rx_party_vf25)

/* 00574060 slot 27 (thiscall; dt; RET 4): WindDown. Unless the match is over (+0xff088: then +0xff094 = 1, the next start is a new match)
 * the wind-down timer (+0xff090) runs down and at 0 (or NaN) every driver's vehicle lets go of its controls (0045f590) and the activity's
 * vf16 (slot 0x40) restarts the round; the race clock's limit +0x2c4 = FLT_MAX; Activity::vf27. Frame: push ebx / esi (+ edi in the loop). */
void rx_infection_vf27(rx_addr act, float dt) {
    ACT_FRAME(F, 0, 2);
    if (U32(act, 0xff088) != 0) {
        SU32(act, 0xff094, 1);
        ACT_CALL(1, rx_activity_wind_down(act, x87_copy(dt)));
        ACT_END(F);
        return;
    }
    const float t = rx_subs(F32(act, 0xff090), dt);
    SU32(act, 0xff094, 0);
    SF32(act, 0xff090, t);
    if (!(0.0f < t)) {
        rx_set_sp(F - 4);   /* push edi */
        for (int32_t i = 0; i < I32(act, 0x26c); i++)
            ACT_CALL(0, rx_vehicle_release_controls(U32(U32(U32(U32(act, 0x268), 4u * (uint32_t)i), 0x74), 0x1ac)));
        rx_set_sp(F);
        (void)act_vcall(act, 0x40, 0, 0);
    }
    SU32(act, 0x2c4, 0x7f7fffffu /* FLT_MAX, 0x977494 */);
    ACT_CALL(1, rx_activity_wind_down(act, x87_copy(dt)));
    ACT_END(F);
}
RX_FUNCTION(0x00574060, rx_infection_vf27)

/* 00574110 slot 32 (thiscall; next state; RET 4): Activity::vf32 (the countdown's end), then the grid pass of the game (0075f890: type
 * 0xe, the activity's mode, assign). Frame: push esi. */
void rx_infection_vf32(rx_addr act, uint32_t next) {
    ACT_FRAME(F, 0, 1);
    (void)next;
    ACT_CALL(1, rx_activity_countdown_exit(act));
    ACT_CALL(4, rx_game_grid_pass(U32(0, 0x00a946b8), 0xe, U32(act, 0x238), 1));
    ACT_END(F);
}
RX_FUNCTION(0x00574110, rx_infection_vf32)

/* 00573b80 slot 40 (thiscall; old state; RET 4): WindDown begins. Activity::vf40; the wind-down timer 3 s; for a single-player activity
 * (mode +0x238 == 1) the message ("%s %s": the winner's name and the localized STR_WINS_THE_MATCH / STR_WINS, or "%s" STR_TIE without a
 * winner: UI text, the oracle runs the original localization chain, the product leaves an empty string, as race_tag.c's vf40) raised as
 * "Player Wins" (00589e10); then "Match Over" (the match is over, +0xff088) or "Round Over" with the activity.
 * Frame: sub esp, 0x20c; push ebp / esi / edi (F); the UTF-8 name F + 0x10, the text F + 0x110. */
void rx_infection_vf40(rx_addr act, uint32_t old_state) {
    ACT_FRAME(F, 0x20c, 3);
    (void)old_state;
    ACT_CALL(1, rx_activity_wind_down_enter(act));
    SF32(act, 0xff090, 3.0f /* 0x92fe48 */);
    if (U32(act, 0x238) == 1) {
        const rx_addr winner = U32(act, 0xff08c);
        const rx_addr key = winner == 0 ? 0x00942318u /* "STR_TIE" */ : U32(act, 0xff088) != 0 ? 0x00942304u /* "STR_WINS_THE_MATCH" */ : 0x00941fc4u /* "STR_WINS" */;
#ifdef RX_ORACLE
        uint32_t zero = 0;
        const uint32_t lang = rx_call_original(0x005dfc90, RX_CDECL, 0, 1, &zero);
        rx_addr entry;
        ACT_CALLR(entry, 0, rx_ui_language_lookup(key, lang));
        { uint32_t a[8] = {0xfde9, 0, U32(entry, 0x10), 0xffffffffu, F + 0x10, 0x100, 0, 0}; rx_os_import(0x009160a8, 8, a); }
        SU8(F, 0x10f, 0);
        if (winner != 0) {
            SU32(F, -12, 0x00941fd0); SU32(F, -8, U32(winner, 0x74) + 0x54); SU32(F, -4, F + 0x10);
            rx_set_sp(F - 16);
            rx_format_buffer(F + 0x110, 0x104, 0x00941fd0 /* "%s %s" */, F - 8);
        } else {
            SU32(F, -4, 0x009419e4);
            rx_set_sp(F - 8);
            rx_format_buffer(F + 0x110, 0x104, 0x009419e4 /* "%s" */, F);
        }
        rx_set_sp(F);
#else
        (void)key;
        SU8(F, 0x110, 0);
#endif
        ACT_CALL(1, rx_party_player_wins(U32(act, 0x290), U32(act, 0xff08c), F + 0x110, act));
    }
    SU32(F, 0xc, act);
    act_raise(U32(act, 0x290), U32(act, 0xff088) != 0 ? STR_MATCH_OVER : STR_ROUND_OVER, TD_ACTIVITY, F + 0xc, 4);
    ACT_END(F);
}
RX_FUNCTION(0x00573b80, rx_infection_vf40)

/* 00574140 slot 42 (ECX = activity): the round's end check. (ONLINE: a client waits for the host's round end +0xbc0.) The healthy riders
 * are counted (the last one found kept in +0xff08c); the driver with the most rounds won is the leader. At most one healthy rider (none
 * for a single rider) ends the round: the round count + 1, the survivor's rounds won + 1 (and it leads if it has more); a leader with the
 * rounds to win (+0xff084) wins the match (+0xff08c, +0xff088 = 1); then WindDown (state 3). Frame: push ecx / ebx / esi (+ ebp / edi;
 * the healthy count at F + 0x10). */
void rx_infection_vf42(rx_addr act) {
    ACT_FRAME(F, 0, 3);
    session_static();
    if (session_networked()) {
        session_static();
        if (U32(U32(0, 0x00da6cb0), 0x1c48) == 0 && U32(act, 0xbc0) == 0) { ACT_END(F); return; }
        act_unreached(0x005741bc, "ONLINE: the networked infection round end");
    }
    rx_set_sp(F - 8);   /* push ebp / edi */
    const rx_addr G = F - 8;
    int32_t best_won = -1;
    rx_addr best = 0;
    SU32(G, 0x10, 0);
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        const rx_addr d = U32(U32(act, 0x268), 4u * (uint32_t)i);
        if (U8(d, 0xf14) == 0) {
            SU32(G, 0x10, U32(G, 0x10) + 1);
            SU32(act, 0xff08c, d);
        }
        if (I32(d, 0xf18) > best_won) { best_won = I32(d, 0xf18); best = d; }
    }
    const uint32_t healthy = U32(G, 0x10);
    if (I32(act, 0x26c) > 1 ? healthy > 1 : healthy != 0) { rx_set_sp(F); ACT_END(F); return; }
    SU32(act, 0xff080, U32(act, 0xff080) + 1);
    const rx_addr surv = U32(act, 0xff08c);
    if (surv != 0) {
        SU32(surv, 0xf18, U32(surv, 0xf18) + 1);
        if (I32(U32(act, 0xff08c), 0xf18) > I32(best, 0xf18)) best = U32(act, 0xff08c);
    }
    if (best != 0 && I32(best, 0xf18) >= I32(act, 0xff084)) {
        SU32(act, 0xff08c, best);
        SU32(act, 0xff088, 1);
    }
    ACT_CALL(2, rx_activity_set_state(act, 3));
    rx_set_sp(F);
    ACT_END(F);
}
RX_FUNCTION(0x00574140, rx_infection_vf42)

/* 00573b20 slot 43 (stdcall a, b, tie; RET 0xc; ECX unused) -> the standings' order: healthy first (+0xf14), then the more rounds won
 * (+0xf18), then (with tie) the start zones (+0x1dc of the players: a >= b gives 1, else -1). */
int32_t rx_infection_vf43(rx_addr act, rx_addr a, rx_addr b, uint32_t tie) {
    (void)act;
    int32_t r = (int32_t)U8(a, 0xf14) - (int32_t)U8(b, 0xf14);
    if (r != 0) return r;
    r = I32(b, 0xf18) - I32(a, 0xf18);
    if (r != 0) return r;
    if (tie == 0) return 0;
    return I32(U32(a, 0x74), 0x1dc) >= I32(U32(b, 0x74), 0x1dc) ? 1 : -1;
}
RX_FUNCTION(0x00573b20, rx_infection_vf43)

/* 00573af0 slot 44 (thiscall; player; RET 4) -> a new InfectionDriver (0x11c0 bytes: 00515690) for the player, 0 when the allocation
 * fails. Frame: push esi. */
rx_addr rx_infection_vf44(rx_addr act, rx_addr player) {
    ACT_FRAME(F, 0, 1);
    rx_addr p;
    ACT_CALLR(p, 1, rx_reflex_aligned_alloc(0x11c0));
    rx_addr d = 0;
    if (p != 0) ACT_CALLR(d, 1, rx_infection_driver_construct(p, act, player));
    ACT_END(F);
    return d;
}
RX_FUNCTION(0x00573af0, rx_infection_vf44)

/* 00574290 slot 47 (thiscall; driver; RET 4): the driver's rules: outside a free-ride area (the world's +0x22144 != 1) the free ride
 * rule (00587690), then RuleInfection (00589e70) and RuleOutOfSnakeBounds (00589120), each initialised with the driver (vf1, slot 4).
 * Frame: push esi. */
void rx_infection_vf47(rx_addr act, rx_addr driver) {
    ACT_FRAME(F, 0, 1);
    const uint32_t a[1] = {driver};
    if (U32(U32(act, 0x234), 0x22144) != 1) {
        rx_addr rule;
        ACT_CALLR(rule, 0, rx_waypoint_get_free_ride_rule(driver));
        (void)act_vcall(rule, 4, 1, a);
    }
    rx_addr rule;
    ACT_CALLR(rule, 0, rx_infection_rule_create(driver));
    (void)act_vcall(rule, 4, 1, a);
    ACT_CALLR(rule, 0, rx_snake_bounds_rule_create(driver));
    (void)act_vcall(rule, 4, 1, a);
    ACT_END(F);
}
RX_FUNCTION(0x00574290, rx_infection_vf47)

/* ======================================================================================================================== */
/* the round                                                                                                                 */

/* 00573450 (stdcall act; RET 4): the round's places. The scene's start entities of type 0x1017 named by the activity (+0x118: 007d2820),
 * the round's start zone (the activity row's MiniGameStartZone; ONLINE: the session's 006f7510); every driver's start (+0x340 + 0x30 * i:
 * position, direction) on a circle of 24 around the zone's entity, i * 360 / riders degrees from its direction (the game's inlined sine /
 * cosine), on the terrain (007c5950) + 2; "Round Started" ({act, round + 1, the most rounds won of the standings, the rounds to win})
 * when the standings exist (+0x2d0); the start entities again as a vector (007d26d0, freed); +0x880 = 0xc.
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0xe4; push ebx / esi / edi. */
void rx_infection_place_round(rx_addr act) {
    ACT_FRAME_EBP(F, 16, 0xe4, 3);
    const rx_addr scene = U32(U32(act, 0x234), 0x20138);
    for (uint32_t k = 0; k < 0x2c; k += 4) SU32(F + 0xa4, k, 0);
    SU32(F, 0x5c, scene);
    SU32(F, 0xa0, 0);
    ACT_CALL(6, (void)rx_scene_group_query(scene, 0x1017, F + 0xa0, 0xc, act + 0x118, 0));
    int32_t row;
    ACT_CALLR(row, 1, rx_activity_find_row(ACT_ACTIVITY_TBL, 0xe, (int32_t)(int8_t)U8(act, 0x220)));
    SU32(act, 0xff070, 0);
    session_static();
    uint32_t zone;
    if (session_networked()) act_unreached(0x0057350e, "ONLINE: the session's start zone (006f7510)");
    if (row == -1) zone = 0;
    else {
        uint32_t col;
        ACT_CALLR(col, 1, rx_tbl_column_index(ACT_ACTIVITY_TBL, 0x0093dfd4 /* "MiniGameStartZone" */));
        rx_addr cell;
        ACT_CALLR(cell, 1, rx_tbl_cell_word(ACT_ACTIVITY_TBL, col, (uint32_t)row));
        zone = U32(cell, 0);
    }
    SU32(act, 0xff070, zone);
    const int32_t riders = I32(act, 0x26c);
    const rx_addr start = U32(F + 0xa0, 4 * U32(act, 0xff070));
    const float step = rx_divs(360.0f /* 0x9776c0 */, (float)riders);
    SF32(F, 0x60, step);
    SU32(F, 0x2c, start);
    SU32(F, 0x28, 0);
    for (int32_t i = 0; i < riders; i++) {
        const rx_addr out = act + 0x358 + 0x30u * (uint32_t)i;   /* EDI: the start's direction z; position at -0x18, direction at -8 */
        rx_addr m = act_vcall(start, 0x38, 0, 0);
        SF32(F, 0x70, F32(m, 0x30)); SF32(F, 0x74, F32(m, 0x34)); SF32(F, 0x78, F32(m, 0x38));
        m = act_vcall(start, 0x38, 0, 0);
        const float dy = F32(m, 0x24), dx = F32(m, 0x20), dz = F32(m, 0x28);
        SF32(F, 0x14, dy); SF32(F, 0x18, dz); SF32(F, 0x10, dx);
        const float len2 = rx_adds(rx_adds(rx_muls(dz, dz), rx_muls(dy, dy)), rx_muls(dx, dx));
        SF32(F, 0x6c, len2);
        float one = 1.0f;   /* 0x931f24 */
        if (len2 > 0.0f) {
            float len;
            ACT_CALLR(len, 1, phys_sqrtf(x87_copy(len2)));
            const float inv = rx_divs(one, len);
            SF32(F, 0x10, rx_muls(F32(F, 0x10), inv));
            SF32(F, 0x14, rx_muls(F32(F, 0x14), inv));
            SF32(F, 0x18, rx_muls(F32(F, 0x18), inv));
        }
        const float angle = rx_muls(rx_muls((float)I32(F, 0x28), F32(F, 0x60)), rx_bf(0x3c8efa34u) /* pi / 180, 0x977678 */);
        float sign = one;
        SF32(F, 0x80, 0.0f); SF32(F, 0x84, one); SF32(F, 0x88, 0.0f);
        if (!(angle >= 0.0f)) sign = rx_bf(0xbf800000u);   /* -1, 0x9cd9e8 (comiss; jae: NaN takes -1) */
        const float x = rx_muls(sign, angle);
        const float t = b1_trig_reduce(x);
        const float t2 = rx_muls(t, t), t4 = rx_muls(t2, t2);
        const float cosv = rx_subs(rx_bf(0x80000000u) /* -0.0, 0x9774a8 */, b1_cos_poly(t2, t4));
        SF32(F, 0x68, cosv);
        const float sinv = rx_muls(rx_muls(b1_sin_poly(t2, t4), t), sign);
        SF32(F, 0x64, sinv);
        float a[3], b[3], c1[3], c2[3];
        a[0] = F32(F, 0x10); a[1] = F32(F, 0x14); a[2] = F32(F, 0x18);
        b[0] = F32(F, 0x80); b[1] = F32(F, 0x84); b[2] = F32(F, 0x88);
        b1_cross(a, b, c1);                                  /* Vec3_Cross 00401630 (EAX = the direction, ECX = up) -> F + 0xd0 */
        SF32(F, 0xd0, c1[0]); SF32(F, 0xd4, c1[1]); SF32(F, 0xd8, c1[2]);
        b1_cross(b, c1, c2);                                 /* (EAX = up, ECX = the first product) -> F + 0x90 */
        SF32(F, 0x90, c2[0]); SF32(F, 0x94, c2[1]); SF32(F, 0x98, c2[2]);
        const float k = rx_adds(rx_muls(rx_adds(F32(F, 0x18), F32(F, 0x10)), 0.0f), F32(F, 0x14));
        const float k0 = rx_muls(k, 0.0f);
        SF32(F, 0x34, k);
        SF32(F, 0x38, k0);
        const float ox = rx_adds(rx_adds(rx_muls(c2[0], cosv), rx_muls(c1[0], sinv)), k0);
        const float oy = rx_adds(rx_adds(rx_muls(c2[1], cosv), rx_muls(c1[1], sinv)), k);
        const float oz = rx_adds(rx_adds(rx_muls(c2[2], cosv), rx_muls(c1[2], sinv)), k0);
        SF32(out, -8, ox); SF32(out, -4, oy); SF32(out, 0, oz);
        SF32(F, 0x10, ox); SF32(F, 0x14, oy); SF32(F, 0x18, oz);
        const float r = 24.0f;   /* 0x97796c */
        SF32(out, -0x18, rx_adds(rx_muls(ox, r), F32(F, 0x70)));
        SF32(out, -0x14, rx_adds(rx_muls(oy, r), F32(F, 0x74)));
        SF32(out, -0x10, rx_adds(rx_muls(oz, r), F32(F, 0x78)));
        const rx_addr terrain = scene_terrain(U32(F, 0x5c));
        uint32_t edx;
        ACT_CALL(1, (void)phys_terrain_base_height_at(terrain, out - 0x18, &edx));
        SF32(out, -0x14, rx_adds(F32(out, -0x14), 2.0f /* 0x92e330 */));
        SU32(F, 0x28, (uint32_t)(i + 1));
    }
    if (U32(act, 0x2d0) != 0) {
        int32_t best = 0;
        for (int32_t i = 0; i < I32(act, 0x26c); i++) {
            const int32_t won = I32(rx_stand(act, (uint32_t)i), 0xf18);
            if (won > best) best = won;
        }
        SU32(F, 0xd0, act);
        SU32(F, 0xd4, U32(act, 0xff080) + 1);
        SU32(F, 0xd8, (uint32_t)best);
        SU32(F, 0xdc, U32(act, 0xff084));
        act_raise(U32(act, 0x290), STR_ROUND_STARTED, TD_ROUND_STARTED, F + 0xd0, 0x10);
    }
    /* the start entities again as a vector (007d26d0: ESI = the vector {data, size, capacity 4, growth 4, INT_MAX}) */
    rx_addr data;
    ACT_CALLR(data, 1, rx_reflex_aligned_alloc(4 * 4));
    SU32(F, 0x30, data);
    SU32(F, 0x38, 4);
    SU32(F, 0x3c, 4);
    SU32(F, 0x34, 0);
    SU32(F, 0x40, 0x7fffffff);
    ACT_CALL(3, rx_waypoint_scene_collect_typed(F + 0x30, U32(U32(act, 0x234), 0x20138), 0x1017, act + 0x118));
    SU32(act, 0x880, 0xc);
    if (U8(0, 0x00da67d8) != 0) {
        loop_enter_cs(0x00d61704);
        if (U8(0, 0x00da67d8) != 0) loop_enter_cs(0x00d61704);
    }
    ACT_CALL(1, crt_aligned_free(U32(F, 0x30)));
    if (U8(0, 0x00da67d8) != 0) {
        loop_leave_cs(0x00d61704);
        if (U8(0, 0x00da67d8) != 0) loop_leave_cs(0x00d61704);
    }
    ACT_END(F);
}
RX_FUNCTION(0x00573450, rx_infection_place_round)

/* 00574370 (ESI = act): the boundary of the round's start zone: its SnakeAreaRadius (+0xa0 of the zone's entity, 007d2820's list) into
 * +0xff098, its position (vf17 slot 0x44) into +0xff0a0; under the profiler scope a live network snake object gets the radius (ONLINE).
 * Frame: push ebp; mov ebp, esp; and esp, -8; sub esp, 0x38; push ebx / edi (the list F + 0x10). */
void rx_infection_boundary_reset(rx_addr act) {
    ACT_FRAME_EBP(F, 8, 0x38, 2);
    for (uint32_t k = 0; k < 0x2c; k += 4) SU32(F + 0x14, k, 0);
    SU32(F, 0x10, 0);
    ACT_CALL(6, (void)rx_scene_group_query(U32(U32(act, 0x234), 0x20138), 0x1017, F + 0x10, 0xc, act + 0x118, 0));
    const rx_addr zone = U32(F + 0x10, 4 * U32(act, 0xff070));
    SF32(act, 0xff098, x87_copy(F32(zone, 0xa0)));
    { const uint32_t a[1] = {act + 0xff0a0}; (void)act_vcall(zone, 0x44, 1, a); }
    session_static();
    const rx_addr scope = race_profile_begin();
    if (U32(act, 0xff1e0) != 0 && (rx_net_ref_valid(U32(act, 0xff1e0)) & 0xff) != 0) act_unreached(0x0057445e, "ONLINE: the boundary's radius sent (the network snake object)");
    race_profile_end(scope);
    ACT_END(F);
}
RX_FUNCTION(0x00574370, rx_infection_boundary_reset)

/* 005744c0 (EDI = act; stdcall dt; RET 4): the boundary shrinks by BoundaryShrinkRate * 0.5 * dt while above 1 (unless the debug freeze
 * [0xd92f0c]); +0xff180 the radius; under the profiler scope a live network snake object gets the radius every 3 s (ONLINE).
 * Frame: push ebx / esi. */
void rx_infection_boundary_shrink(rx_addr act, float dt) {
    ACT_FRAME(F, 0, 2);
    const float r = rx_subs(F32(act, 0xff098), rx_muls(rx_muls(F32(act, 0xff0b0), 0.5f /* 0x9322cc */), dt));
    if (r > 1.0f /* 0x931f24 */) {
        if (U32(0, 0x00d92f0c) == 0) SF32(act, 0xff098, r);
        SF32(act, 0xff180, x87_copy(F32(act, 0xff098)));
    }
    session_static();
    const rx_addr scope = race_profile_begin();
    if (U32(act, 0xff1e0) != 0 && (rx_net_ref_valid(U32(act, 0xff1e0)) & 0xff) != 0) act_unreached(0x00574589, "ONLINE: the boundary's radius sent every 3 s");
    race_profile_end(scope);
    ACT_END(F);
}
RX_FUNCTION(0x005744c0, rx_infection_boundary_shrink)

/* ======================================================================================================================== */
/* InfectionDriver                                                                                                           */

/* 00515690 (ECX = driver, EAX = player; stdcall act; RET 4) -> driver: NetworkDriver's constructor (0050e1b0), the activity, the
 * PlaceableParticleEffect at +0xf20 (006964d0), healthy, no rounds, score 0. Frame: push edi. */
rx_addr rx_infection_driver_construct(rx_addr d, rx_addr act, rx_addr player) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(1, rx_network_driver_construct(d, player));
    SU32(d, 0xf10, act);
    SU32(d, 0, VT_INFECTION_DRIVER);
    ACT_CALL(0, (void)rx_placeable_effect_construct(d + 0xf20));
    SU8(d, 0xf14, 0);
    SU32(d, 0xf18, 0);
    SF32(d, 0xf1c, 0.0f);
    ACT_END(F);
    return d;
}
RX_FUNCTION(0x00515690, rx_infection_driver_construct)

/* 005156e0 slot 0 (thiscall; flags; RET 4) -> driver: the effect's destructor (006965a0), NetworkDriver's (0050e2f0), freed with flags & 1.
 * Frame: push esi. */
rx_addr rx_infection_driver_sdtor(rx_addr d, uint32_t flags) {
    ACT_FRAME(F, 0, 1);
    SU32(d, 0, VT_INFECTION_DRIVER);
    ACT_CALL(0, rx_particle_effect_destroy(d + 0xf20));
    ACT_CALL(0, rx_network_driver_dtor(d));
    if (flags & 1) ACT_CALL(1, rx_reflex_aligned_free(d));
    ACT_END(F);
    return d;
}
RX_FUNCTION(0x005156e0, rx_infection_driver_sdtor)

/* 00515720 slot 1 (ECX = driver): NetworkDriver::vf1; the effect added to the activity's scene (its id from the scene's counter under the
 * lock, 007d2f70), its scene (+0xf30), "Cloud_Fire" as its name (+0x10b0), its vf7 (slot 0x1c), its four distances 1e6; under the profiler
 * scope the driver's "NetworkSnakeActivity" listener (0053b170) and a live network object's score (ONLINE). Frame: push ebp; mov ebp, esp;
 * and esp, -8; push ebx / ebp / esi / edi. */
void rx_infection_driver_vf1(rx_addr d) {
    ACT_FRAME_EBP(F, 8, 0, 4);
    ACT_CALL(0, rx_network_driver_vf1(d));
    const rx_addr scene = U32(U32(U32(d, 0xf10), 0x234), 0x20138);
    const rx_addr fx = d + 0xf20;
    rx_set_sp(F - 4);
    os_cs(0x0091600c, scene + 4);
    rx_set_sp(F);
    const uint32_t id = U32(scene, 0x19f4);
    SU32(scene, 0x19f4, id + 1);
    SU32(fx, 0x14, id);
    rx_set_sp(F - 4);
    os_cs(0x009160f8, scene + 4);
    rx_set_sp(F);
    ACT_CALL(1, rx_scene_manager_add_entity(fx, scene));
    SU32(d, 0xf30, U32(U32(U32(d, 0xf10), 0x234), 0x20138));
    act_strcpy(d + 0x10b0, 0x0093efd4 /* "Cloud_Fire" */);
    (void)act_vcall(fx, 0x1c, 0, 0);
    for (uint32_t k = 0; k < 16; k += 4) SU32(d, 0x1064 + k, 0x49742400u);   /* 1e6, 0x977658 */
    session_static();
    const rx_addr scope = race_profile_begin();
    session_static();
    ACT_CALL(1, rx_infection_driver_listen(U32(0, 0x00da6ca0), d));
    if (net_object_live(d)) act_unreached(0x00515884, "ONLINE: the infection driver's score sent (0050bbd0 / 006fea10)");
    race_profile_end(scope);
    ACT_END(F);
}
RX_FUNCTION(0x00515720, rx_infection_driver_vf1)

/* 005158c0 slot 2 (ECX = driver): BaseDriver::vf2, the effect leaves the scene (007d3040), under the profiler scope the listener leaves
 * (0053b230) and a live network object's link is released (ONLINE). Frame: push ebp; mov ebp, esp; and esp, -8; push ecx / ebx / esi /
 * edi. */
void rx_infection_driver_vf2(rx_addr d) {
    ACT_FRAME_EBP(F, 8, 4, 3);
    ACT_CALL(0, rx_base_driver_vf2(d));
    ACT_CALL(1, rx_scene_manager_remove_entity(d + 0xf20, U32(U32(U32(d, 0xf10), 0x234), 0x20138)));
    session_static();
    const rx_addr scope = race_profile_begin();
    session_static();
    ACT_CALL(1, rx_infection_driver_unlisten(U32(0, 0x00da6ca0), d));
    if (net_object_live(d) && U32(U32(U32(d, 0xef0), 0x1d4), 0) != 0) act_unreached(0x005159b2, "ONLINE: the infection driver's network link released (007006e0)");
    race_profile_end(scope);
    ACT_END(F);
}
RX_FUNCTION(0x005158c0, rx_infection_driver_vf2)

/* 005159d0 slot 3 (ECX = driver): the round's reset. NetworkDriver::vf3; a local player's vehicle effects at its start (00460020);
 * healthy, score 0, +0x3c0 = 0, a new match's rounds won cleared; under the profiler scope a live network object's score (ONLINE), a
 * spectator's vehicle made visible again (+0x14e of its model, vf49 slot 0xc4 with 0, its body record +0x14 = -1), the behaviours' and
 * rules' resets (005092b0 / 005092f0). Frame: push ebp; mov ebp, esp; and esp, -8; push ecx / ebp / esi / edi (the scope at F + 0xc). */
void rx_infection_driver_vf3(rx_addr d) {
    ACT_FRAME_EBP(F, 8, 0, 4);
    ACT_CALL(0, rx_network_driver_vf3(d));
    const rx_addr player = U32(d, 0x74);
    if (U32(player, 4) != 0) {
        const rx_addr start = U32(d, 0x144);
        ACT_CALL(2, rx_vehicle_effect_query(U32(player, 0x1ac), start, start + 0x10));
    }
    SU8(d, 0xf14, 0);
    SF32(d, 0xf1c, 0.0f);
    SF32(d, 0x3c0, 0.0f);
    if (U32(U32(d, 0xf10), 0xff094) != 0) SU32(d, 0xf18, 0);
    session_static();
    const rx_addr scope = race_profile_begin();
    SU32(F, 0xc, scope);
    if (net_object_live(d)) act_unreached(0x00515ac9, "ONLINE: the infection driver's score sent (0050bbd0 / 006fea10)");
    const rx_addr p = U32(d, 0x74);
    if (U32(p, 0x1e4) != 0) {
        const rx_addr veh = U32(p, 0x1ac);
        SU8(U32(veh, 0x1328), 0x14e, 0);
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
RX_FUNCTION(0x005159d0, rx_infection_driver_vf3)

/* 00515d70 (EAX = driver): a human's rules: every rule's vf5 (slot 0x14) with 0; a rider out of the boundary for 5 s (its
 * RuleOutOfSnakeBounds, kind 9: +0x50 outside, +0x54 the clock it may stay out until) is infected and "PlayerInfected" raised with it
 * (0053a6d0). Frame: push ebx / esi / edi. */
void rx_infection_driver_human(rx_addr d) {
    ACT_FRAME(F, 0, 3);
    for (uint32_t i = 0; i < U32(d, 0xee4); i++) act_vcall_f(U32(d, 0xa0 + 4 * i), 0x14, 0.0f);
    rx_addr rule;
    ACT_CALLR(rule, 1, rx_driver_rule_kind9(d));
    if (rule != 0 && U32(rule, 0x50) == 0 && 0.0f > rx_subs(F32(rule, 0x54), F32(U32(rule, 4), 0x130))) {
        SU8(d, 0xf14, 1);
        const rx_addr act = U32(U32(d, 0x74), 0x94);
        ACT_CALL(1, rx_event_activity_driver(U32(act, 0x290), STR_INFECTED, act, d));
    }
    ACT_END(F);
}
RX_FUNCTION(0x00515d70, rx_infection_driver_human)

/* 00516120 (ESI = driver; stdcall dt; RET 4): a computer rider's drive: every rule's vf5 (slot 0x14) with 0, then the behaviours' vf4
 * (slot 0x10) with dt until one takes the drive (non-zero). Frame: push ebx / edi. */
void rx_infection_driver_ai(rx_addr d, float dt) {
    ACT_FRAME(F, 0, 2);
    for (uint32_t i = 0; i < U32(d, 0xee4); i++) act_vcall_f(U32(d, 0xa0 + 4 * i), 0x14, 0.0f);
    for (uint32_t i = 0; i < U32(d, 0xee0); i++) {
        const rx_addr b = U32(d, 0xe8 + 4 * i);
        const rx_addr fn = U32(U32(b, 0), 0x10);
        uint32_t (*f)(rx_addr, float) = RX_FN(fn, uint32_t (*)(rx_addr, float));
        uint32_t took;
        if (f) ACT_CALLR(took, 1, f(b, x87_copy(dt)));
        else { const uint32_t a = rx_fb(x87_copy(dt)); took = rx_call_original(fn, RX_THISCALL, b, 1, &a); }
        if (took != 0) break;
    }
    ACT_END(F);
}
RX_FUNCTION(0x00516120, rx_infection_driver_ai)

/* 00515b60 slot 7 (thiscall; dt; RET 4): the driver's update. A spectator (+0x908) runs NetworkDriver::vf7 only. The vehicle's position
 * (vf17 slot 0x44) into +0xf80; the infected's smoke on / off (the effect's IRenderable +0xfb0, slot 5); a healthy rider's score is the
 * round's time (+0x294) times the activity's rate (+0xff078); a human runs its rules (00515d70), a local computer rider its drive
 * (00516120); NetworkDriver::vf7; outside a replay ([0xda6cb0] + 0x1c3c) a live network object takes the host's score in InProgress
 * (ONLINE). Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0xd4; push ebx / esi / edi (the position F + 0x10); dt through x87. */
void rx_infection_driver_vf7(rx_addr d, float dt) {
    ACT_FRAME_EBP(F, 16, 0xd4, 3);
    const rx_addr p = U32(d, 0x74);
    if (U32(p, 0x908) != 0) {
        ACT_CALL(1, rx_network_driver_update(d, x87_copy(dt)));
        ACT_END(F);
        return;
    }
    { const uint32_t a[1] = {F + 0x10}; (void)act_vcall(U32(p, 0x1ac), 0x44, 1, a); }
    SF32(d, 0xf80, F32(F, 0x10));
    SF32(d, 0xf84, F32(F, 0x14));
    SF32(d, 0xf88, F32(F, 0x18));
    if (U8(d, 0xf14) != 0) {
        const uint32_t a[1] = {1};
        (void)act_vcall(d + 0xfb0, 0x14, 1, a);
    } else {
        const uint32_t a[1] = {0};
        (void)act_vcall(d + 0xfb0, 0x14, 1, a);
        const rx_addr act = U32(d, 0xf10);
        SF32(d, 0xf1c, rx_muls(F32(act, 0x294), F32(act, 0xff078)));
    }
    const rx_addr pl = U32(d, 0x74);
    if (U32(pl, 0x10) == 0) ACT_CALL(0, rx_infection_driver_human(d));
    else if (U32(pl, 0x10) == 1 && U32(pl, 4) != 0) ACT_CALL(1, rx_infection_driver_ai(d, x87_copy(dt)));
    ACT_CALL(1, rx_network_driver_update(d, x87_copy(dt)));
    session_static();
    if (U32(U32(0, 0x00da6cb0), 0x1c3c) == 0) {
        session_static();
        const rx_addr scope = race_profile_begin();
        const rx_addr net = U32(d, 0xef0);
        if (net != 0 && (U8(net, 0x20) & 1) != 0 && (rx_net_ref_valid(net) & 0xff) == 0 && U32(U32(U32(d, 0x74), 0x94), 0x21c) == 2)
            act_unreached(0x00515d14, "ONLINE: the host's infection score (00700310 / 006fed10)");
        race_profile_end(scope);
    }
    ACT_END(F);
}
RX_FUNCTION(0x00515b60, rx_infection_driver_vf7)

/* 00515df0 slot 9 (thiscall; event; RET 4): a rule's event. RuleInfection's (kind 0xc: this driver infected another) scores the infection
 * (+0xf1c += the activity's +0xff074), then the session's stations are selected (0053a290: a Quazal selection iterator) and, for a
 * station, the infection is sent with both drivers' network handles (ONLINE: 004024c0, 006fd970); the iterator is released (0088c190).
 * ONLINE BOUNDARY: the selection is the Quazal object store's (the session's duplicated stations); the product has no Quazal store and the
 * original's offline selection is empty (no station: [0xda6cb0] + 0x1c4c == 0, N1), so the product selects nothing; the oracle runs the
 * original iterator (its Quazal heap churn is the library's, read by no game code). Frame: sub esp, 0x2c; push esi / edi (the iterator
 * F + 0x10, its current entry F + 0x18). */
void rx_infection_driver_vf9(rx_addr d, rx_addr event) {
    ACT_FRAME(F, 0x2c, 2);
    SU32(F, 8, 0);
    const uint32_t kind = act_vcall(event, 0xc, 0, 0);
    if (kind == 9 || kind != 0xc) { ACT_END(F); return; }
    SF32(d, 0xf1c, rx_adds(F32(U32(d, 0xf10), 0xff074), F32(d, 0xf1c)));
#ifdef RX_ORACLE
    {   rx_regs in, out; memset(&in, 0, sizeof in); in.r[6] = F + 0x10;
        ACT_CALL(0, rx_call_original_ex(0x0053a290, &in, &out, 0, 0));
        ACT_CALL(0, rx_net_selection_next(F + 0x10)); }
#else
    SU32(F, 0x18, 0);
#endif
    if (U32(F, 0x18) != 0) act_unreached(0x00515e5a, "ONLINE: the infection sent to the session's stations (006fd970)");
#ifdef RX_ORACLE
    ACT_CALL(0, (void)rx_call_original(0x0088c190, RX_THISCALL, F + 0x10, 0, 0));
#endif
    ACT_END(F);
}
RX_FUNCTION(0x00515df0, rx_infection_driver_vf9)

/* 0053b0d0 (stdcall driver; RET 4) -> the driver's first rule whose kind (its vf3, slot 0xc) is 9 (RuleOutOfSnakeBounds), else 0.
 * Frame: push ebx / esi / edi. */
rx_addr rx_driver_rule_kind9(rx_addr d) {
    ACT_FRAME(F, 0, 3);
    for (uint32_t i = 0; i < U32(d, 0xee4); i++) {
        const rx_addr r = U32(d, 0xa0 + 4 * i);
        if (r != 0 && act_vcall(r, 0xc, 0, 0) == 9) { ACT_END(F); return U32(d, 0xa0 + 4 * i); }
    }
    ACT_END(F);
    return 0;
}
RX_FUNCTION(0x0053b0d0, rx_driver_rule_kind9)

/* 0053b170 (EAX = the session channel; stdcall driver; RET 4): "NetworkSnakeActivity" with 00515f00 (proxy 0x940390) */
void rx_infection_driver_listen(rx_addr channel, rx_addr d) {
    ACT_CALL(1, act_subscribe(0x0053b170, 1, 1, 4, channel, 0x0093ef78 /* "NetworkSnakeActivity" */, 0x00940390, 0x009cde24, d, 0x00515f00));
}
RX_FUNCTION(0x0053b170, rx_infection_driver_listen)
/* 0053b230: the listener leaves (frame: sub esp, 0x14; push esi) */
void rx_infection_driver_unlisten(rx_addr channel, rx_addr d) {
    act_unlisten(0, channel, 0x0093ef78, 0x00940390, 0x009cde24, d, 0x00515f00);
}
RX_FUNCTION(0x0053b230, rx_infection_driver_unlisten)

/* ======================================================================================================================== */
/* RuleInfection                                                                                                             */

/* 00589e70 (ECX = driver) -> a RuleInfection (0x10 bytes from the driver's rule storage +0x3e4, appended to the rules +0xa0 / +0xee4;
 * radius^2 0), 0 when the driver has 18 rules */
rx_addr rx_infection_rule_create(rx_addr d) {
    if (U32(d, 0xee4) >= 0x12) return 0;
    const rx_addr r = U32(d, 0x3e4);
    SU32(d, 0x3e4, r + 0x10);
    rx_addr rule = 0;
    if (r != 0) { SU32(r, 0, VT_RULE_INFECTION); SF32(r, 0xc, 0.0f); rule = r; }
    SU32(d, 0xa0 + 4 * U32(d, 0xee4), rule);
    const rx_addr out = U32(d, 0xa0 + 4 * U32(d, 0xee4));
    SU32(d, 0xee4, U32(d, 0xee4) + 1);
    return out;
}
RX_FUNCTION(0x00589e70, rx_infection_rule_create)

/* 0051c880 slot 1 (thiscall; driver; RET 4): +4 the driver, +8 enabled, +0xc 144 (12 ft squared, 0x9358b4) */
void rx_infection_rule_vf1(rx_addr rule, rx_addr driver) {
    SU32(rule, 4, driver);
    SU32(rule, 8, 1);
    SF32(rule, 0xc, 144.0f);
}
RX_FUNCTION(0x0051c880, rx_infection_rule_vf1)

/* 0051c8a0 slot 5 (thiscall; dt; RET 4) -> EAX (left over): under the profiler scope, an infected driver (without a network object, or
 * only on the host for a non-spectator) infects every healthy driver of the activity whose vehicle (+0xc0) is within the radius (the
 * distance^2 below +0xc: z, y, x summed in that order): +0xf14 = 1, the activity's last infected (+0xff07c), and its own vf9 (slot 0x24)
 * with the rule (the score). Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x14; push ebx / esi / edi (the driver F + 0x1c, the scope F + 0x18). */
uint32_t rx_infection_rule_vf5(rx_addr rule, float dt) {
    ACT_FRAME_EBP(F, 16, 0x14, 3);
    (void)dt;
    const rx_addr me = U32(rule, 4);
    const rx_addr act = U32(U32(me, 0x74), 0x94);
    SU32(F, 0x1c, me);
    session_static();
    const rx_addr scope = race_profile_begin();
    SU32(F, 0x18, scope);
    int run = 1;
    if (U32(U32(rule, 4), 0xef0) != 0) {
        session_static();
        if (U32(U32(0, 0x00da6cb0), 0x1c48) == 0 || U32(U32(U32(rule, 4), 0x74), 0x1e4) != 0) run = 0;
    }
    if (run && U8(me, 0xf14) != 0) {
        for (int32_t i = 0; i < I32(act, 0x26c); i++) {
            const rx_addr o = U32(U32(act, 0x268), 4u * (uint32_t)i);
            if (U8(o, 0xf14) != 0) continue;
            const rx_addr ov = U32(U32(o, 0x74), 0x1ac), mv = U32(U32(U32(rule, 4), 0x74), 0x1ac);
            const float dz = rx_subs(F32(ov, 0xc8), F32(mv, 0xc8)), dy = rx_subs(F32(ov, 0xc4), F32(mv, 0xc4)), dx = rx_subs(F32(ov, 0xc0), F32(mv, 0xc0));
            const float d2 = rx_adds(rx_adds(rx_muls(dz, dz), rx_muls(dy, dy)), rx_muls(dx, dx));
            if (F32(rule, 0xc) > d2) {
                SU8(o, 0xf14, 1);
                SU32(act, 0xff07c, o);
                const uint32_t a[1] = {rule};
                (void)act_vcall(U32(rule, 4), 0x24, 1, a);
            }
        }
    }
    race_profile_end(U32(F, 0x18));
    ACT_END(F);
    return 0;
}
RX_FUNCTION(0x0051c8a0, rx_infection_rule_vf5)

/* ======================================================================================================================== */
/* InfectionActivityAudioLogic                                                                                               */

#define ACH(l) U32(U32((l), 0x50), 0x290)   /* the logic's activity's channel */
/* the cue map (+0x8c) holds a cue of the name (as race_tag_audio.c) */
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
/* the listener templates: 0058cb70 "Round Started" (00584df0, proxy 0x9460c4), 0058cc30 (ECX = channel, EAX = name; stdcall logic, method:
 * void(Activity*), proxy 0x9460e4), 0058ccf0 "Player Eliminated" (00584880, proxy 0x946104: void(Activity*, BaseDriver*)), 0058cdb0
 * "Player Wins" (00583fb0, proxy 0x946124), and their leaves 0058ce70 / 0058ced0 / 0058cf30 / 0058cf90 */
void rx_infection_audio_listen_round_started(rx_addr channel, rx_addr logic) {
    act_subscribe(0x0058cb70, 1, 1, 4, channel, STR_ROUND_STARTED, 0x009460c4, TD_ROUND_STARTED, logic, 0x00584df0);
}
RX_FUNCTION(0x0058cb70, rx_infection_audio_listen_round_started)
void rx_infection_audio_listen_activity(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method) {
    act_subscribe(0x0058cc30, 2 | 4, 2, 3, channel, name, 0x009460e4, TD_ACTIVITY, logic, method);
}
RX_FUNCTION(0x0058cc30, rx_infection_audio_listen_activity)
void rx_infection_audio_listen_eliminated(rx_addr channel, rx_addr logic) {
    act_subscribe(0x0058ccf0, 1, 1, 4, channel, STR_ELIMINATED, 0x00946104, 0x009cda18, logic, 0x00584880);
}
RX_FUNCTION(0x0058ccf0, rx_infection_audio_listen_eliminated)
void rx_infection_audio_listen_player_wins(rx_addr channel, rx_addr logic) {
    act_subscribe(0x0058cdb0, 1, 1, 4, channel, STR_PLAYER_WINS, 0x00946124, 0x009cf630, logic, 0x00583fb0);
}
RX_FUNCTION(0x0058cdb0, rx_infection_audio_listen_player_wins)
void rx_infection_audio_unlisten_round_started(rx_addr channel, rx_addr logic) {
    act_unlisten(0, channel, STR_ROUND_STARTED, 0x009460c4, TD_ROUND_STARTED, logic, 0x00584df0);
}
RX_FUNCTION(0x0058ce70, rx_infection_audio_unlisten_round_started)
void rx_infection_audio_unlisten_activity(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method) {
    act_unlisten(0, channel, name, 0x009460e4, TD_ACTIVITY, logic, method);
}
RX_FUNCTION(0x0058ced0, rx_infection_audio_unlisten_activity)
void rx_infection_audio_unlisten_eliminated(rx_addr channel, rx_addr logic) {
    act_unlisten(0, channel, STR_ELIMINATED, 0x00946104, 0x009cda18, logic, 0x00584880);
}
RX_FUNCTION(0x0058cf30, rx_infection_audio_unlisten_eliminated)
void rx_infection_audio_unlisten_player_wins(rx_addr channel, rx_addr logic) {
    act_unlisten(0, channel, STR_PLAYER_WINS, 0x00946124, 0x009cf630, logic, 0x00583fb0);
}
RX_FUNCTION(0x0058cf90, rx_infection_audio_unlisten_player_wins)

/* 00584880 (thiscall logic; activity, driver; RET 8): the "PlayerInfected" cue for the driver's player */
void rx_infection_audio_infected(rx_addr logic, rx_addr act, rx_addr driver) { (void)act; RACE_CORE_CALL(4, rx_audio_cue(logic, STR_INFECTED, U32(driver, 0x74), 0)); }
RX_FUNCTION(0x00584880, rx_infection_audio_infected)

/* 00584460 slot 3 (ECX = logic): the cue handlers listen ("Round Started", "Round Over", "Match Over", the "PlayerInfected" cue on
 * "Player Eliminated", "Player Wins") when the level's cue map holds the cue. Frame: push ebp; mov ebp, esp; and esp, -8; sub esp, 0x28;
 * push ebx / ebp / esi / edi. */
void rx_infection_audio_vf3(rx_addr logic) {
    ACT_FRAME_EBP(L, 8, 0x28, 4);
    if (U32(logic, 0x8c) != 0) {
        if (cue_present(logic, L, STR_ROUND_STARTED)) ACT_CALL(1, rx_infection_audio_listen_round_started(ACH(logic), logic));
        if (cue_present(logic, L, STR_ROUND_OVER)) ACT_CALL(2, rx_infection_audio_listen_activity(ACH(logic), STR_ROUND_OVER, logic, 0x00583a30));
        if (cue_present(logic, L, STR_MATCH_OVER)) ACT_CALL(2, rx_infection_audio_listen_activity(ACH(logic), STR_MATCH_OVER, logic, 0x00583a50));
        if (cue_present(logic, L, STR_INFECTED)) ACT_CALL(1, rx_infection_audio_listen_eliminated(ACH(logic), logic));
        if (cue_present(logic, L, STR_PLAYER_WINS)) ACT_CALL(1, rx_infection_audio_listen_player_wins(ACH(logic), logic));
    }
    ACT_END(L);
}
RX_FUNCTION(0x00584460, rx_infection_audio_vf3)

/* 00584670 slot 4 (ECX = logic): the cue handlers leave. Same frame as vf3. */
void rx_infection_audio_vf4(rx_addr logic) {
    ACT_FRAME_EBP(L, 8, 0x28, 4);
    if (U32(logic, 0x8c) != 0) {
        if (cue_present(logic, L, STR_ROUND_STARTED)) ACT_CALL(1, rx_infection_audio_unlisten_round_started(ACH(logic), logic));
        if (cue_present(logic, L, STR_ROUND_OVER)) ACT_CALL(2, rx_infection_audio_unlisten_activity(ACH(logic), STR_ROUND_OVER, logic, 0x00583a30));
        if (cue_present(logic, L, STR_MATCH_OVER)) ACT_CALL(2, rx_infection_audio_unlisten_activity(ACH(logic), STR_MATCH_OVER, logic, 0x00583a50));
        if (cue_present(logic, L, STR_INFECTED)) ACT_CALL(1, rx_infection_audio_unlisten_eliminated(ACH(logic), logic));
        if (cue_present(logic, L, STR_PLAYER_WINS)) ACT_CALL(1, rx_infection_audio_unlisten_player_wins(ACH(logic), logic));
    }
    ACT_END(L);
}
RX_FUNCTION(0x00584670, rx_infection_audio_vf4)
