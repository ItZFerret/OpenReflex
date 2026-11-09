/* race_tag.c -- MXActivityTag (the Tag party mode, activity type 9; vtable 0x009446cc; 0xff3b0 bytes), hand-transliterated from the
 * original (the address of each function is in its comment; frames and the stack depth of each call are the original's).
 *
 * The game: one ball (the scene's "TagBall" entity, a physics object) lies near the round's start zone; a rider who touches it holds
 * it (RuleTag, race_tag_driver.c); a rider holding the ball counts the time it held it (TagDriver::vf7); a crash or a water kick
 * drops it. Who first holds it for +0xff38c seconds (60) wins the round (vf28 / vf42); who first wins +0xff390 rounds (the activity
 * row's NumLaps: the "Rounds" duration) wins the match. Between rounds the activity winds down for 3 s (vf27) and restarts (vf16) at
 * the next start zone. Team mode (+0xff080) is never set by any offline path (the session's team rules).
 * The class derives from Activity directly (005562c0); its audio logic is a TagActivityAudioLogic (race_tag_audio.c).
 *
 * ONLINE (a Quazal session: [0x00da6cb0] + 0x1c4c set and the mode +0x1b0 not 1; none exists in the product): the network TagBall
 * object (+0xff380), its holder sync and the host's round end. Offline the activity is the whole game on one machine: it decides the
 * grabs, drops and rounds itself. Each Quazal branch is marked "ONLINE" and stops with act_unreached. */
#include "race_tag.h"
#include "race_champ.h"
#include "race_activity.h"
#include "race_activity_misc.h"
#include "race_finish.h"
#include "race_extern.h"
#include "build_activity.h"
#include "build_activity_int.h"
#include "build_activity_teardown.h"
#include "build_activity_freeride_run.h"
#include "build_vehicle.h"
#include "build_alloc.h"
#include "waypoint_activity.h"
#include "waypoint_audio.h"
#include "waypoint_driver.h"
#include "anim_entity_dtors.h"
#include "anim.h"
#include "world_veh.h"
#include "world.h"
#include "phys_terrain.h"
#include "phys_crt.h"
#include "race_beh1_extern.h"
#include "race.h"
#include "race_activity_ui.h"
#include "waypoint_activity.h"
#include "loop_platform.h"
#include "fx.h"
#include "crt.h"
#include "../src/rx_os.h"
#include <string.h>
#include "rider_cap.h"

#define VT_TAG            0x009446ccu
#define VT_TAG_AUDIO      0x00942f7cu
#define VT_AUDIO_LOGIC    0x009429fcu   /* ActivityAudioLogic::vftable */
#define TD_ACTIVITY       0x009cbd24u   /* void(Activity*) */
#define TD_STRING         0x009cdb68u   /* void(const char*) */
#define TD_ROUND_STARTED  0x009cf680u   /* void(Activity*, int, int, int) */
#define TD_ARROW_TARGET   0x009cdb00u   /* void(BaseDriver*, const Matrix4x4*, int) */
#define TD_DRIVER         0x009cda44u   /* void(BaseDriver*) */
#define STR_ACTIVITY_INIT 0x00935ea4u   /* "Activity Init" */
#define STR_ROUND_STARTED 0x009421b0u   /* "Round Started" */
#define STR_TAG_CHANGED   0x0093efc4u   /* "Tag It Changed" */
#define STR_SET_ARROW     0x0093ef90u   /* "Set Arrow Target" */
#define STR_SHOW_ARROW    0x0093ef30u   /* "Show Arrow" */
#define STR_EMPTY         0x00935d6cu   /* "" */

#define BALL(A)    U32((A), 0xff090)                               /* the TagBall scene entity */
#define BALL_PH(A) U32(BALL(A) + 0x154, 4)                         /* its physics handle (+0x158) */

/* the session's static initialiser (006de1f0 under bit 0 of 0x00dcf304) has run at startup */
static void session_static(void) {
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
}
/* a networked session: +0x1c4c set and the mode +0x1b0 not 1 */
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
/* the world matrix of an entity (its vf14, slot 0x38) */
static rx_addr entity_matrix(rx_addr e) { return act_vcall(e, 0x38, 0, 0); }
/* the terrain of the activity's scene: the registry 0x9e30d4 (count 0x9e30dc, initialised under the guard bit 0x9e30e0) */
static rx_addr scene_terrain(rx_addr scene) {
    if (!(U8(0, 0x009e30e0) & 1)) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
    for (uint32_t i = 0; i < U32(0, 0x009e30dc); i++) {
        const rx_addr t = U32(0, 0x009e30d4 + 4 * i);
        if (t != 0 && U32(t, 0x44a8) == scene) return t;
    }
    return 0;
}

/* ======================================================================================================================== */
/* construction / destruction                                                                                                */

/* 0056d550 (stack act; ECX = type, EAX = world; RET 4) -> act: Activity's constructor, the class's fields, one PlaceableParticleEffect
 * at +0xff0c0 (the vector constructor with 006964d0), a new TagActivityAudioLogic (0x98 bytes) at +0xfd8.
 * Frame: push ecx / ebx / esi / edi. */
rx_addr rx_tag_construct(rx_addr act, uint32_t type, rx_addr world) {
    ACT_FRAME(F, 0, 4);
    ACT_CALL(1, rx_activity_construct(act, type, world));
    SU32(act, 0, VT_TAG);
    SU32(act, 0xff090, 0);
    ACT_CALL(4, rx_fx_vector_ctor_iterator(act + 0xff0c0, 0x2a0, 1, 0x006964d0));
    SU32(act, 0xff37c, 0);
    SU32(act, 0xff38c, U32(0, 0x00931da0));   /* 60.0 */
    SU32(act, 0xff380, 0);
    SU32(act, 0xff394, 0);
    SU32(act, 0xff390, 4);
    SU32(act, 0xff3a0, 1);
    SU32(act, 0xff398, 0);
    SF32(act, 0xff39c, 0.0f);
    SU32(act, 0xff080, 0);
    SU32(act, 0xff088, 0);
    SU32(act, 0xff084, 0);
    SU32(act, 0xff08c, 0xffffffffu);
    SF32(act, 0xff384, 0.0f);
    SU32(act, 0x2c8, 0);
    SU32(act, 0xff07c, 0);
    SU32(act, 0xff370, 0);
    SU32(act, 0xff374, 0);
    SU32(act, 0xff3a4, 1);
    SU32(act, 0xff3a8, 0);
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
        SU32(logic, 0, VT_TAG_AUDIO);
    }
    SU32(act, 0xfd8, logic);
    ACT_END(F);
    return act;
}
RX_FUNCTION(0x0056d550, rx_tag_construct)

/* 0056d680 slot 0 (thiscall; flags; RET 4) -> this: the audio logic destroyed and freed, the particle effect's destructor (006965a0),
 * the Activity base's (005566b0), freed when flags & 1. Frame: push esi / edi. */
rx_addr rx_tag_sdtor(rx_addr act, uint32_t flags) {
    ACT_FRAME(F, 0, 2);
    const rx_addr logic = U32(act, 0xfd8);
    SU32(act, 0, VT_TAG);
    if (logic != 0) {
        ACT_CALL(1, rx_activity_audio_destroy(logic));
        ACT_CALL(1, rx_reflex_aligned_free(logic));
    }
    ACT_CALL(0, rx_particle_effect_destroy(act + 0xff0c0));
    ACT_CALL(0, rx_waypoint_activity_base_destroy(act));
    if (flags & 1) ACT_CALL(1, rx_reflex_aligned_free(act));
    ACT_END(F);
    return act;
}
RX_FUNCTION(0x0056d680, rx_tag_sdtor)

/* 0056d6e0 slot 1 (thiscall; &row ref; RET 4): the rounds to win (+0xff390) from the activity row's "NumLaps", then Activity::vf1.
 * Frame: push ebx / ebp / esi / edi. */
void rx_tag_vf1(rx_addr act, rx_addr ref) {
    ACT_FRAME(F, 0, 4);
    const uint32_t row = U32(ref, 0);
    uint32_t col;
    ACT_CALLR(col, 1, rx_tbl_column_index(ACT_ACTIVITY_TBL, 0x0093dd64 /* "NumLaps" */));
    rx_addr cell;
    ACT_CALLR(cell, 1, rx_tbl_cell_word(ACT_ACTIVITY_TBL, col, row));
    SU32(act, 0xff390, U32(cell, 0));
    ACT_CALL(1, rx_activity_vf1(act, ref));
    ACT_END(F);
}
RX_FUNCTION(0x0056d6e0, rx_tag_vf1)

/* ======================================================================================================================== */
/* the session's network listeners (the templates as 00588b80) and their mirrors                                              */

/* 00589170 (EAX = the session channel [0xda6ca0]; stdcall activity; RET 4): "NetworkObjectRemoved" with 00570300 (proxy 0x945ae4) */
void rx_tag_net_removed_listen(rx_addr channel, rx_addr act) {
    ACT_CALL(1, act_subscribe(0x00589170, 1, 1, 2, channel, 0x0093ee98 /* "NetworkObjectRemoved" */, 0x00945ae4, 0x009cdc7c, act, 0x00570300));
}
RX_FUNCTION(0x00589170, rx_tag_net_removed_listen)
/* 00589230: "NetworkTagActivity" with 00570320 (proxy 0x945b04: void(DOHandle)) */
void rx_tag_net_activity_listen(rx_addr channel, rx_addr act) {
    ACT_CALL(1, act_subscribe(0x00589230, 1, 1, 2, channel, 0x00942230 /* "NetworkTagActivity" */, 0x00945b04, 0x009d024c, act, 0x00570320));
}
RX_FUNCTION(0x00589230, rx_tag_net_activity_listen)
/* 005892f0: "NetworkSnakeRoundComplete" with 005708d0 (proxy 0x945b24: void(DOHandle, DOHandle, int)) */
void rx_tag_net_round_listen(rx_addr channel, rx_addr act) {
    ACT_CALL(1, act_subscribe(0x005892f0, 1, 1, 2, channel, 0x00942144 /* "NetworkSnakeRoundComplete" */, 0x00945b24, 0x009d0140, act, 0x005708d0));
}
RX_FUNCTION(0x005892f0, rx_tag_net_round_listen)
/* 005893c0 (EAX = the session channel; stdcall act; RET 4): the listener of 00589230 leaves. Frame: push ebp; and esp, -8; sub esp, 0x14;
 * push esi. */
void rx_tag_net_activity_unlisten(rx_addr channel, rx_addr act) {
    act_unlisten(1, channel, 0x00942230, 0x00945b04, 0x009d024c, act, 0x00570320);
}
RX_FUNCTION(0x005893c0, rx_tag_net_activity_unlisten)
/* 00589420: the listener of 00589170 leaves */
void rx_tag_net_removed_unlisten(rx_addr channel, rx_addr act) {
    act_unlisten(1, channel, 0x0093ee98, 0x00945ae4, 0x009cdc7c, act, 0x00570300);
}
RX_FUNCTION(0x00589420, rx_tag_net_removed_unlisten)
/* 00589480: the listener of 005892f0 leaves */
void rx_tag_net_round_unlisten(rx_addr channel, rx_addr act) {
    act_unlisten(1, channel, 0x00942144, 0x00945b24, 0x009d0140, act, 0x005708d0);
}
RX_FUNCTION(0x00589480, rx_tag_net_round_unlisten)

/* 00570300 (thiscall; kind, object; RET 8): a removed network TagBall object (kind 2) unlinks from the activity. Raised only by a
 * session (ONLINE). */
void rx_tag_net_object_removed(rx_addr act, uint32_t kind, rx_addr object) {
    (void)object;
    if (kind == 2) SU32(act, 0xff380, 0);
}
RX_FUNCTION(0x00570300, rx_tag_net_object_removed)

/* 005708d0 (thiscall; the round's object handle, the holder's handle, its score; RET 0xc): the host's round end ("NetworkSnake-
 * RoundComplete", raised only by a session: ONLINE): the driver whose network object (+0xef0) has the handle (+0x50 & 0x3fffff, a null
 * handle reported through 00884390) gets the score; +0xff36c = 1 (vf42 then ends the round on this client). Frame: push ecx / ebx /
 * ebp / edi (+ push esi in the loop). */
void rx_tag_net_round_complete(rx_addr act, uint32_t handle, uint32_t holder, uint32_t score) {
    (void)handle;
    ACT_FRAME(F, 0, 4);
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        const rx_addr d = U32(U32(act, 0x268), 4u * (uint32_t)i);
        const rx_addr net = U32(d, 0xef0);
        if (net == 0) continue;
        uint32_t id = U32(net, 0x50);
        if ((id & 0x3fffffu) == 0) act_unreached(0x00570910, "ONLINE: a null network handle (00884390)");
        if (id == holder) SU32(d, 0xf1c, score);
    }
    SU32(act, 0xff36c, 1);
    ACT_END(F);
}
RX_FUNCTION(0x005708d0, rx_tag_net_round_complete)

/* ======================================================================================================================== */
/* the round's start                                                                                                         */

/* 0056e390 (stdcall act, on; RET 8): the ball's physics on / off. Off: its kinematic body deactivated (twice, the second time also the
 * body's flag +0xb0 cleared), +0xff388 = 0. On: a networked client's ball (not the host, ONLINE) stays deactivated and replays its
 * record (0041ce60); else the body's flag +0xb0 set and the body activated at rest (the zero vector 0xd61670); +0xff388 = 1.
 * Frame: push ebp / esi / edi. */
void rx_tag_ball_physics(rx_addr act, uint32_t on) {
    ACT_FRAME(F, 0, 3);
    if (on != 0) {
        session_static();
        if (session_networked()) {
            session_static();
            if (U32(U32(0, 0x00da6cb0), 0x1c48) == 0) act_unreached(0x0056e40d, "ONLINE: a client's network ball (kinematic off, 0041ce60)");
        }
        if (BALL_PH(act) != 0) {
            rx_addr body = rx_world_physics_handle_body(BALL_PH(act));
            SU8(body, 0xb0, 1);
        }
        if (BALL_PH(act) != 0) {
            const rx_addr world = U32(BALL_PH(act), 4);
            rx_addr body = rx_world_physics_handle_body(BALL_PH(act));
            ACT_CALL(2, rx_world_kinematic_activate(body, world, 0x00d61670));
        }
        SU32(act, 0xff388, 1);
    } else {
        if (BALL_PH(act) != 0) {
            const rx_addr world = U32(BALL_PH(act), 4);
            rx_addr body = rx_world_physics_handle_body(BALL_PH(act));
            ACT_CALL(1, rx_world_kinematic_deactivate(body, world));
        }
        if (BALL_PH(act) != 0) {
            const rx_addr world = U32(BALL_PH(act), 4);
            rx_addr body = rx_world_physics_handle_body(BALL_PH(act));
            ACT_CALL(1, rx_world_kinematic_deactivate(body, world));
            body = rx_world_physics_handle_body(BALL_PH(act));
            SU8(body, 0xb0, 0);
        }
        SU32(act, 0xff388, 0);
    }
    ACT_END(F);
}
RX_FUNCTION(0x0056e390, rx_tag_ball_physics)

/* 0056da60 (stdcall act; RET 4): the round's places. The scene's SnakeTagStartEntity group (type 0x1017, the activity's subscene +0x118,
 * at most 12) and the round's start zone (+0xff07c: the activity row's MiniGameStartZone, 0 without a row; ONLINE: the session's).
 * Every rider's start (+0x340 + 0x30 * i: the position, +0x350 the direction) on a circle of 24 ft around the zone, at the rider's angle
 * i * 360 / n (the inlined sine / cosine polynomials) in the plane of the zone's direction (+0x20 of its matrix, normalised) and the up
 * axis, put on the terrain (007c5950) and 2 ft up. The ball goes to the next start entity (its 0x1017 vector, +0xff360), 0x9d78dc above
 * the terrain, at the identity orientation (0x9cb550; vf29 slot 0x74 of the ball), kept as its round start (+0xff0a0 / +0xff0b0);
 * +0x880 = 12. With drivers listed (+0x2d0) "Round Started" is raised (the activity, the rounds played + 1, the most rounds a driver won,
 * the rounds to win). The vector's block is freed (_aligned_free under the allocator lock).
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0xf4; push ebx / esi / edi (F below). The zone entities F + 0xb0, the zone's
 * position F + 0x80, its direction F + 0x10, the up axis F + 0x90, the cross products F + 0xe0 / F + 0xa0, the angle step F + 0x70, the
 * rider count F + 0x6c, the terrain F + 0x28, the zone F + 0x2c, the ball's start F + 0x30, the 0x1017 vector F + 0x40, the event F + 0xe0. */
void rx_tag_place_round(rx_addr act) {
    ACT_FRAME_EBP(F, 16, 0xf4, 3);
    const rx_addr scene = U32(U32(act, 0x234), 0x20138);
    for (uint32_t k = 0; k < 0x2c; k += 4) SU32(F + 0xb4, k, 0);
    SU32(F, 0xb0, 0);
    ACT_CALL(6, rx_scene_group_query(scene, 0x1017, F + 0xb0, 0xc, act + 0x118, 0));
    int32_t row;
    ACT_CALLR(row, 1, rx_activity_find_row(ACT_ACTIVITY_TBL, 9, (int32_t)(int8_t)U8(act, 0x220)));
    SU32(act, 0xff07c, 0);
    session_static();
    uint32_t zone;
    if (session_networked()) act_unreached(0x0056db25, "ONLINE: the session's start zone (006f7510)");
    if (row == -1) zone = 0;
    else {
        uint32_t col;
        ACT_CALLR(col, 1, rx_tbl_column_index(ACT_ACTIVITY_TBL, 0x0093dfd4 /* "MiniGameStartZone" */));
        rx_addr cell;
        ACT_CALLR(cell, 1, rx_tbl_cell_word(ACT_ACTIVITY_TBL, col, (uint32_t)row));
        zone = U32(cell, 0);
    }
    const int32_t riders = I32(act, 0x280) + I32(act, 0x26c);
    SU32(act, 0xff07c, zone);
    const rx_addr start = U32(F + 0xb0, 4 * zone);
    const float step = rx_bf(0x43b40000u) /* 360.0, 0x9776c0 */ / (float)riders;
    SU32(F, 0x6c, (uint32_t)riders);
    SF32(F, 0x70, step);
    SU32(F, 0x2c, start);
    const rx_addr terrain = scene_terrain(scene);
    SU32(F, 0x28, terrain);
    for (int32_t i = 0; i < riders; i++) {
        const rx_addr out = act + 0x358 + 0x30u * (uint32_t)i;   /* ESI: the start's direction z; position at -0x18, direction at -8 */
        rx_addr m = entity_matrix(start);
        SF32(F, 0x80, F32(m, 0x30)); SF32(F, 0x84, F32(m, 0x34)); SF32(F, 0x88, F32(m, 0x38));
        m = entity_matrix(start);
        float dx = F32(m, 0x20), dy = F32(m, 0x24), dz = F32(m, 0x28);
        SF32(F, 0x10, dx); SF32(F, 0x18, dz);
        const float len2 = dx * dx + dz * dz + dy * dy;
        SF32(F, 0x14, dy);
        SF32(F, 0x74, len2);
        float one = rx_bf(0x3f800000u);   /* 0x931f24 */
        if (len2 > 0.0f) {
            float len;
            ACT_CALLR(len, 1, phys_sqrtf(x87_copy(len2)));
            const float inv = one / len;
            SF32(F, 0x10, F32(F, 0x10) * inv);
            SF32(F, 0x14, F32(F, 0x14) * inv);
            SF32(F, 0x18, F32(F, 0x18) * inv);
        }
        float angle = (float)i * step * rx_bf(0x3c8efa34u) /* 0x977678 pi/180 */;
        float sign = one;
        SF32(F, 0x90, 0.0f); SF32(F, 0x94, one); SF32(F, 0x98, 0.0f);
        if (!(angle >= 0.0f)) sign = rx_bf(0xbf800000u);   /* -1, 0x9cd9e8 (comiss; jae: NaN takes -1) */
        const float x = sign * angle;
        const float t = rx_bf(0x40490fdbu) /* pi, 0x977498 */ - (x - (float)rx_cvtt(x * rx_bf(0x3e22f983u) /* 0x9774a0 */) * rx_bf(0x40c90fdbu) /* 0x97749c */);
        const float t2 = t * t, t4 = t2 * t2;
        const float cosv = rx_bf(0x80000000u) /* -0.0, 0x9774a8 */ - b1_cos_poly(t2, t4);
        SF32(F, 0x78, cosv);
        const float sinv = b1_sin_poly(t2, t4) * t * sign;
        SF32(F, 0x7c, sinv);
        float a[3], b[3], c1[3], c2[3];
        a[0] = F32(F, 0x10); a[1] = F32(F, 0x14); a[2] = F32(F, 0x18);
        b[0] = F32(F, 0x90); b[1] = F32(F, 0x94); b[2] = F32(F, 0x98);
        b1_cross(a, b, c1);                                  /* Vec3_Cross 00401630 (EAX = the direction, ECX = up) -> F + 0xe0 */
        SF32(F, 0xe0, c1[0]); SF32(F, 0xe4, c1[1]); SF32(F, 0xe8, c1[2]);
        b1_cross(b, c1, c2);                                 /* (EAX = up, ECX = the first product) -> F + 0xa0 */
        SF32(F, 0xa0, c2[0]); SF32(F, 0xa4, c2[1]); SF32(F, 0xa8, c2[2]);
        const float k = (F32(F, 0x18) + F32(F, 0x10)) * 0.0f + F32(F, 0x14);
        const float k0 = k * 0.0f;
        SF32(F, 0x48, k0);
        SF32(F, 0x44, k);
        const float ox = c2[0] * cosv + c1[0] * sinv + k0;
        const float oy = c2[1] * cosv + c1[1] * sinv + k;
        const float oz = c2[2] * cosv + c1[2] * sinv + k0;
        SF32(out, -8, ox); SF32(out, -4, oy); SF32(out, 0, oz);
        SF32(F, 0x10, ox); SF32(F, 0x14, oy); SF32(F, 0x18, oz);
        const float r = rx_bf(0x41c00000u);   /* 24.0, 0x97796c */
        SF32(out, -0x18, ox * r + F32(F, 0x80));
        SF32(out, -0x14, oy * r + F32(F, 0x84));
        SF32(out, -0x10, oz * r + F32(F, 0x88));
        uint32_t edx;
        ACT_CALL(1, (void)phys_terrain_base_height_at(terrain, out - 0x18, &edx));
        SF32(out, -0x14, F32(out, -0x14) + rx_bf(0x40000000u) /* 2.0, 0x92e330 */);
    }
    /* the start entities again as a vector (007d26d0: ESI = the vector {data, size, capacity 4, growth 4, INT_MAX}) */
    rx_addr data;
    ACT_CALLR(data, 1, rx_reflex_aligned_alloc(4 * 4));
    SU32(F, 0x40, data);
    SU32(F, 0x48, 4);
    SU32(F, 0x4c, 4);
    SU32(F, 0x44, 0);
    SU32(F, 0x50, 0x7fffffff);
    ACT_CALL(3, rx_waypoint_scene_collect_typed(F + 0x40, scene, 0x1017, act + 0x118));
    int32_t n = I32(F, 0x44);
    if (n == 0) n = 1;
    int32_t next = I32(act, 0xff07c) + 1;
    if (next >= n) next = 0;
    const rx_addr ball_start = U32(F + 0xb0, 4u * (uint32_t)next);
    SU32(act, 0xff360, ball_start);
    {
        const rx_addr m = entity_matrix(ball_start);
        const float y = F32(m, 0x34), z = F32(m, 0x38), x = F32(m, 0x30);
        SF32(F, 0x30, x); SF32(F, 0x34, y); SF32(F, 0x38, z);
        uint32_t edx;
        ACT_CALL(1, (void)phys_terrain_base_height_at(U32(F, 0x28), F + 0x30, &edx));
        SF32(F, 0x34, F32(F, 0x34) + rx_bf(0x42480000u) /* 50.0, 0x9d78dc */);
        const uint32_t a[2] = {F + 0x30, 0x00d61640};
        (void)act_vcall(BALL(act), 0x74, 2, a);
    }
    SU32(act, 0xff0a0, U32(F, 0x30));
    SU32(act, 0xff0a4, U32(F, 0x34));
    SU32(act, 0xff0a8, U32(F, 0x38));
    for (uint32_t k = 0; k < 16; k += 4) SU32(act + 0xff0b0, k, U32(0, 0x009cb550 + k));
    SU32(act, 0x880, 0xc);
    if (U32(act, 0x2d0) != 0) {
        int32_t sum = 0, best = 0;
        for (int32_t i = 0; i < I32(act, 0x26c); i++) {
            const int32_t won = I32(rx_stand(act, (uint32_t)i), 0xf1c);
            sum += won;
            if (won > best) best = won;
        }
        SU32(F, 0xe0, act);
        SU32(F, 0xe4, (uint32_t)(sum + 1));
        SU32(F, 0xe8, (uint32_t)best);
        SU32(F, 0xec, U32(act, 0xff390));
        act_raise(U32(act, 0x290), STR_ROUND_STARTED, TD_ROUND_STARTED, F + 0xe0, 0x10);
    }
    if (U8(0, 0x00da67d8) != 0) {
        loop_enter_cs(0x00d61704);
        if (U8(0, 0x00da67d8) != 0) loop_enter_cs(0x00d61704);
    }
    ACT_CALL(1, crt_aligned_free(U32(F, 0x40)));
    if (U8(0, 0x00da67d8) != 0) {
        loop_leave_cs(0x00d61704);
        if (U8(0, 0x00da67d8) != 0) loop_leave_cs(0x00d61704);
    }
    ACT_END(F);
}
RX_FUNCTION(0x0056da60, rx_tag_place_round)

/* 0056d720 slot 2 (thiscall; arg; RET 4): Activity::vf2, the HUD fade 0.25 (0x92ff84), the scene's "TagBall" entity (its body's flags
 * +0xb1 / +0xb2 cleared, the entity's +0xac = 0), the three network listeners, the session's TagBall object (ONLINE: the host's
 * 008879b0 / 00887780), the ball's physics on, the round's places (0056da60), with particles on ([0xaf650c]) the "Cloud_Fire" effect
 * added to the scene at +0xff0c0 (its id from the scene's counter +0x19f4 under the scene's lock, its vf7 slot 0x1c, its four distance
 * zones 1e6), the counters cleared and "Activity Init" raised on the event manager [0xa93f74]. Frame: push ebx / ebp / esi / edi. */
void rx_tag_vf2(rx_addr act, rx_addr arg) {
    ACT_FRAME(F, 0, 4);
    ACT_CALL(1, rx_activity_vf2(act, arg));
    SU32(act, 0xff070, U32(0, 0x0092ff84));   /* 0.25 */
    rx_addr ball;
    ACT_CALLR(ball, 2, rx_scene_manager_find_named(U32(U32(act, 0x234), 0x20138), 0x00942228 /* "TagBall" */));
    SU32(act, 0xff090, ball);
    SU8(rx_world_physics_handle_body(U32(ball, 0x158)), 0xb1, 0);
    SU8(rx_world_physics_handle_body(U32(BALL(act), 0x158)), 0xb2, 0);
    SU32(BALL(act), 0xac, 0);
    session_static();
    ACT_CALL(1, rx_tag_net_removed_listen(U32(0, 0x00da6ca0), act));
    session_static();
    ACT_CALL(1, rx_tag_net_activity_listen(U32(0, 0x00da6ca0), act));
    session_static();
    ACT_CALL(1, rx_tag_net_round_listen(U32(0, 0x00da6ca0), act));
    session_static();
    if (session_networked()) {
        session_static();
        if (U32(U32(0, 0x00da6cb0), 0x1c48) != 0) act_unreached(0x0056d8a5, "ONLINE: the host's network TagBall object (008879b0)");
    }
    ACT_CALL(2, rx_tag_ball_physics(act, 1));
    ACT_CALL(1, rx_tag_place_round(act));
    if (U32(0, 0x00af650c) != 0) {
        const rx_addr scene = U32(U32(act, 0x234), 0x20138);
        const rx_addr fx = act + 0xff0c0;
        loop_enter_cs(scene + 4);
        const uint32_t id = U32(scene, 0x19f4);
        SU32(scene, 0x19f4, id + 1);
        SU32(fx, 0x14, id);
        loop_leave_cs(scene + 4);
        ACT_CALL(1, rx_scene_manager_add_entity(fx, scene));
        SU32(act, 0xff0d0, U32(U32(act, 0x234), 0x20138));
        act_strcpy(act + 0xff250, 0x0093efd4 /* "Cloud_Fire" */);
        (void)act_vcall(fx, 0x1c, 0, 0);
        for (uint32_t k = 0; k < 16; k += 4) SU32(act, 0xff204 + k, 0x49742400u);   /* 1e6 */
    }
    SU32(act, 0xff370, 0);
    SU32(act, 0xff374, 0);
    SU32(act, 0xff088, 0);
    SU32(act, 0xff084, 0);
    SU32(act, 0xff08c, 0xffffffffu);
    SU32(F, 0x14, act);   /* the payload: the argument slot */
    act_raise(U32(0, 0x00a93f74), STR_ACTIVITY_INIT, TD_ACTIVITY, F + 0x14, 4);
    ACT_END(F);
}
RX_FUNCTION(0x0056d720, rx_tag_vf2)

/* 0056d9d0 slot 4 (ECX = activity): the class's event types on the activity's channel (Activity::vf4 is not called): "Round Started"
 * (005925e0), "Round Over" / "Match Over" (00590c00), "Tag It Changed" (00592820), "BallGrabbed" / "BallDropped" (00591500), "Set Arrow
 * Target" (00592280), "Show Arrow" / "Hide Arrow" (00590f60). Frame: push esi. */
void rx_tag_vf4(rx_addr act) {
    ACT_FRAME(F, 0, 1);
    act_event_type(0x005925e0, U32(act, 0x290), 0);
    act_event_type(0x00590c00, U32(act, 0x290), 0x009421c0 /* "Round Over" */);
    act_event_type(0x00590c00, U32(act, 0x290), 0x009421cc /* "Match Over" */);
    act_event_type(0x00592820, U32(act, 0x290), STR_TAG_CHANGED);
    act_event_type(0x00591500, U32(act, 0x290), 0x00942244 /* "BallGrabbed" */);
    act_event_type(0x00591500, U32(act, 0x290), 0x00942250 /* "BallDropped" */);
    act_event_type(0x00592280, U32(act, 0x290), 0);
    act_event_type(0x00590f60, U32(act, 0x290), STR_SHOW_ARROW);
    act_event_type(0x00590f60, U32(act, 0x290), 0x0093eef0 /* "Hide Arrow" */);
    ACT_END(F);
}
RX_FUNCTION(0x0056d9d0, rx_tag_vf4)

/* 0056e1a0 slot 3 (thiscall): the teardown. A networked session's selection loop (ONLINE), the three listeners leave, the network ball
 * unlinks, every driver of the two lists leaves the activity, the particle effect leaves the scene (with particles on), then a jump to
 * Activity::vf3. Frame: sub esp, 0x24; push ebx / esi / edi. */
void rx_tag_vf3(rx_addr act) {
    ACT_FRAME(F, 0x24, 3);
    session_static();
    if (session_networked()) act_unreached(0x0056e1e3, "ONLINE: the networked tag teardown (the selection iterator loop)");
    session_static();
    ACT_CALL(1, rx_tag_net_activity_unlisten(U32(0, 0x00da6ca0), act));
    session_static();
    ACT_CALL(1, rx_tag_net_removed_unlisten(U32(0, 0x00da6ca0), act));
    session_static();
    ACT_CALL(1, rx_tag_net_round_unlisten(U32(0, 0x00da6ca0), act));
    SU32(act, 0xff380, 0);
    while (I32(act, 0x26c) > 0) ACT_CALL(2, rx_activity_on_player_left(act, U32(U32(act, 0x268), 0)));
    while (I32(act, 0x280) > 0) ACT_CALL(2, rx_activity_on_player_left(act, U32(U32(act, 0x27c), 0)));
    if (U32(0, 0x00af650c) != 0) ACT_CALL(1, rx_scene_manager_remove_entity(act + 0xff0c0, U32(U32(act, 0x234), 0x20138)));
    ACT_END(F);
    rx_activity_vf3(act);
}
RX_FUNCTION(0x0056e1a0, rx_tag_vf3)

/* 0056fe60 slot 15 (ECX = activity): the round's places (0056da60), Activity::vf15, +0xff378 = 0 and the registry's
 * "MXGame\\Tag" "WaterKickTime" (one float into 0x9c5194: the registry [0xa95844] + 0xc, its vf1 finds the record, vf17 reads it).
 * Frame: push esi. */
void rx_tag_vf15(rx_addr act) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(1, rx_tag_place_round(act));
    ACT_CALL(0, rx_activity_vf15(act));
    const rx_addr reg = U32(U32(0, 0x00a95844), 0xc);
    SU32(act, 0xff378, 0);
    const uint32_t find[3] = {0x0093f050 /* "MXGame\\Tag" */, 0x00942270 /* "WaterKickTime" */, 1};
    const rx_addr rec = act_vcall(reg, 4, 3, find);
    if (rec != 0) {
        const uint32_t read[3] = {rec, 0x009c5194, 1};
        (void)act_vcall(reg, 0x44, 3, read);
    }
    ACT_END(F);
}
RX_FUNCTION(0x0056fe60, rx_tag_vf15)

/* 0056feb0 slot 16 (ECX = activity): a new round. A new match (+0xff3a0) clears the match state (+0xff394) and every driver's rounds
 * won (+0xf1c); every driver of the two lists takes its start zone (0054cd20; ONLINE: the session's 006f7330) and its start place
 * (+0x144 = the activity's start +0x340 + 0x30 * zone); +0xff3a0 / +0xff36c cleared, Activity::vf16, +0xff378 = 0.
 * Frame: push ebp; mov ebp, esp; and esp, -8; push ebx / ebp / esi / edi. */
void rx_tag_vf16(rx_addr act) {
    ACT_FRAME_EBP(F, 8, 0, 4);
    if (U32(act, 0xff3a0) != 0) {
        SU32(act, 0xff394, 0);
        for (int32_t i = 0; i < I32(act, 0x26c); i++) SU32(U32(U32(act, 0x268), 4u * (uint32_t)i), 0xf1c, 0);
    }
    for (int list = 0; list < 2; list++) {
        const uint32_t cnt = list ? 0x280 : 0x26c, arr = list ? 0x27c : 0x268;
        for (int32_t i = 0; i < I32(act, cnt); i++) {
            const rx_addr d = U32(U32(act, arr), 4u * (uint32_t)i);
            session_static();
            if (session_networked()) act_unreached(0x0056ff4f, "ONLINE: the session's start zone of a player (006f7330)");
            ACT_CALL(1, rx_player_start_zone(U32(d, 0x74)));
            SU32(d, 0x144, act + 0x340 + 0x30u * U32(U32(d, 0x74), 0x1dc));
        }
    }
    SU32(act, 0xff3a0, 0);
    SU32(act, 0xff36c, 0);
    ACT_CALL(0, rx_activity_vf16(act));
    SU32(act, 0xff378, 0);
    ACT_END(F);
}
RX_FUNCTION(0x0056feb0, rx_tag_vf16)

/* 0056e510 slot 18 (ECX = activity): the round's reset. Activity::vf18, the round's state cleared (the winner, the holder, the water
 * timer / flags / kicks, the resets, the team scores, the host flag, the send timer), the ball back to its round start (its vf29 slot
 * 0x74), its physics on, "Tag It Changed" raised with "" (nobody), and for every driver whose player has a team slot (+8 > -1) "Set
 * Arrow Target" (the driver, the ball's matrix (vf14), 0). A network ball (ONLINE) is reset too.
 * Frame: sub esp, 0x14 (the cookie at F + 0x20); push ebx / ebp / esi / edi; the event payload F + 0x10 (the name), F + 0x14. */
void rx_tag_vf18(rx_addr act) {
    ACT_FRAME(F, 0x14, 4);
    ACT_CALL(0, rx_activity_vf18(act));
    SU32(act, 0xff398, 0);
    SU32(act, 0xff078, 0);
    SF32(act, 0xff364, 0.0f);
    SU32(act, 0xff368, 0);
    SU32(act, 0xff370, 0);
    SU32(act, 0xff374, 0);
    SU32(act, 0xff088, 0);
    SU32(act, 0xff084, 0);
    SU32(act, 0xff08c, 0xffffffffu);
    SU32(act, 0xff36c, 0);
    SF32(act, 0xff384, 0.0f);
    {
        const uint32_t a[2] = {act + 0xff0a0, act + 0xff0b0};
        (void)act_vcall(BALL(act), 0x74, 2, a);
    }
    ACT_CALL(2, rx_tag_ball_physics(act, 1));
    SU32(F, 0x10, STR_EMPTY);
    act_raise(U32(act, 0x290), STR_TAG_CHANGED, TD_STRING, F + 0x10, 4);
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        SU32(F, 0x10, (uint32_t)i);
        const rx_addr d = U32(U32(act, 0x268), 4u * (uint32_t)i);
        if (I32(U32(d, 0x74), 8) <= -1) continue;
        const rx_addr m = act_vcall(BALL(act), 0x38, 0, 0);
        SU32(F, 0x14, d);
        SU32(F, 0x18, m);
        SU32(F, 0x1c, 0);
        rx_set_sp(F);
        act_raise(U32(act, 0x290), STR_SET_ARROW, TD_ARROW_TARGET, F + 0x14, 0xc);
    }
    if (U32(act, 0xff380) != 0) act_unreached(0x0056e64f, "ONLINE: the network TagBall object's reset");
    ACT_END(F);
}
RX_FUNCTION(0x0056e510, rx_tag_vf18)

/* 0056e800 slot 25 (thiscall; dt; RET 4): Activity::vf25 (the intro), then the UI route of the HUD: "GotoSpectatorSinglePlayer" for a
 * spectator (+0xfe8) else "GotoTagSinglePlayer" (UIProperty_SetString "UIRoutingMessage"), and the UI manager's "GotoHub" event raised on
 * its channel; then the standings sorted (00559eb0). UI BOUNDARY (as the challenges' vf2): the product has no UI manager ([0xa93fcc] = 0);
 * the pages only write UI state. Frame: push ebp; mov ebp, esp; and esp, -8; sub esp, 0xc; push ebx / esi / edi; dt through x87. */
void rx_tag_vf25(rx_addr act, float dt) {
    ACT_FRAME_EBP(F, 8, 0xc, 3);
    SU32(F, 0x14, act);
    ACT_CALL(1, rx_activity_pre(act, x87_copy(dt)));
    const rx_addr ui = U32(0, 0x00a93fcc);
    {
        rx_regs in, out;
        memset(&in, 0, sizeof in);
        in.r[6] = ui;
        in.r[7] = 0x0094cc74;   /* "UIRoutingMessage" */
        const uint32_t v[1] = {U32(act, 0xfe8) != 0 ? 0x009421d8u /* "GotoSpectatorSinglePlayer" */ : 0x0094225cu /* "GotoTagSinglePlayer" */};
        act_ui_call_ex(0x005dbf50, &in, &out, 1, v);
        in.r[7] = 0x0094cc74;
        const uint32_t h[1] = {0x009424ec /* "GotoHub" */};
        act_ui_call_ex(0x005db500, &in, &out, 1, h);
        if (out.r[0] != 0) {
            SU32(F, 0x10, out.r[0]);
            act_raise(U32(ui, 0x98), out.r[0], TD_STRING, F + 0x10, 4);
        }
    }
    ACT_CALL(0, rx_activity_sort_standings(U32(F, 0x14)));
    ACT_END(F);
}
RX_FUNCTION(0x0056e800, rx_tag_vf25)

/* 0056fe30 slot 32 (thiscall; next state; RET 4): Activity::vf32 (the countdown's end), then the grid pass of the game (0075f890: type 9,
 * the activity's mode, assign). Frame: push esi. */
void rx_tag_vf32(rx_addr act, uint32_t next) {
    ACT_FRAME(F, 0, 1);
    (void)next;
    ACT_CALL(1, rx_activity_countdown_exit(act));
    ACT_CALL(4, rx_game_grid_pass(U32(0, 0x00a946b8), 9, U32(act, 0x238), 1));
    ACT_END(F);
}
RX_FUNCTION(0x0056fe30, rx_tag_vf32)

/* 0056e8d0 slot 39 (thiscall; old state; RET 4): Activity::vf39 (InProgress begins), "Show Arrow" raised for every driver, the ball's
 * physics on. Frame: push ebx / esi (+ push ebp / edi around the loop); the payload is the argument slot. */
void rx_tag_vf39(rx_addr act, uint32_t old_state) {
    ACT_FRAME(F, 0, 2);
    ACT_CALL(1, rx_activity_in_progress_enter(act));
    (void)old_state;
    if (I32(act, 0x26c) > 0) {
        rx_set_sp(F - 8);
        for (int32_t i = 0; i < I32(act, 0x26c); i++) {
            SU32(F, 0xc, U32(U32(act, 0x268), 4u * (uint32_t)i));   /* the argument slot */
            act_raise(U32(act, 0x290), STR_SHOW_ARROW, TD_DRIVER, F + 0xc, 4);
        }
        rx_set_sp(F);
    }
    ACT_CALL(2, rx_tag_ball_physics(act, 1));
    ACT_END(F);
}
RX_FUNCTION(0x0056e8d0, rx_tag_vf39)

/* 0056e790 slot 43 (arguments driver a, driver b, tie; RET 0xc; the activity is not used) -> the standings order: more rounds won
 * (+0xf1c) first, then the longer total time with the ball (+0xf18), then (with `tie`) the lower start zone ... b's zone >= a's: 1. */
int32_t rx_tag_vf43(rx_addr act, rx_addr a, rx_addr b, uint32_t tie) {
    (void)act;
    int32_t r = I32(b, 0xf1c) - I32(a, 0xf1c);
    if (r != 0) return r;
    const float ta = F32(a, 0xf18), tb = F32(b, 0xf18);
    if (ta > tb) return -1;
    if (tb > ta) return 1;
    if (tie == 0) return 0;
    return I32(U32(a, 0x74), 0x1dc) >= I32(U32(b, 0x74), 0x1dc) ? 1 : -1;
}
RX_FUNCTION(0x0056e790, rx_tag_vf43)

/* 0056e6e0 slot 44 (thiscall; player; RET 4) -> a new TagDriver (0xf30 bytes; team -1, or in team mode the driver count's parity),
 * 0 when the allocation fails. Frame: push esi. */
rx_addr rx_tag_vf44(rx_addr act, rx_addr player) {
    ACT_FRAME(F, 0, 1);
    rx_addr d;
    ACT_CALLR(d, 1, rx_reflex_aligned_alloc(0xf30));
    if (d != 0) {
        uint32_t team = 0xffffffffu;
        if (U32(act, 0xff080) != 0) {
            int32_t n = I32(act, 0x26c);
            team = (uint32_t)(n % 2);   /* and 0x80000001; jns; dec; or -2; inc: the signed remainder */
        }
        ACT_CALLR(d, 2, rx_tag_driver_construct(d, act, player, team));
    }
    ACT_END(F);
    return d;
}
RX_FUNCTION(0x0056e6e0, rx_tag_vf44)

/* 0056e750 slot 47 (thiscall; driver; RET 4): the driver's rules. Outside the free-ride mode (world +0x22144 != 1) the free ride's rule
 * (00587690) binds to it (its vf1), then a RuleTag (005894e0) binds (its vf1). Frame: push esi. */
void rx_tag_vf47(rx_addr act, rx_addr driver) {
    ACT_FRAME(F, 0, 1);
    if (U32(U32(act, 0x234), 0x22144) != 1) {
        rx_addr rule;
        ACT_CALLR(rule, 0, rx_waypoint_get_free_ride_rule(driver));
        const uint32_t a[1] = {driver};
        (void)act_vcall(rule, 4, 1, a);
    }
    rx_addr rule;
    ACT_CALLR(rule, 0, rx_tag_rule_create(driver));
    const uint32_t a[1] = {driver};
    (void)act_vcall(rule, 4, 1, a);
    ACT_END(F);
}
RX_FUNCTION(0x0056e750, rx_tag_vf47)

/* ======================================================================================================================== */
/* the round                                                                                                                 */

/* 0056e940 slot 28 (thiscall; dt; RET 4): InProgress. Activity::vf28; with a holder, the announcer's countdown (+0x2a0 stepped down once
 * the holder's remaining time (+0xff38c - its +0xf14) is within it: "FinishCountdown Step" 005875e0, or reset to +0x2c0 while the time
 * is beyond +0x2c0 + 1); the ball's visibility (0056f210); with particles on the fire effect follows the ball (+0xff120, 2.5 ft up while
 * held); the free ball's reset clock [0x9c5198] (30 s: after 3 resets the ball goes back to its round start; else it is kicked toward
 * the level's origin 1000 ft up: the direction normalised x 10000, the body's velocity cleared, 0056e320); the HUD fade +0xff070 (to -1
 * while held, toward 0.25 while free); a ball outside the terrain's bounds (+0x44d8 .. +0x44e4 less the margin +0x44f0) is put back at
 * (50, 10, 50) 50 ft above the terrain; the debug flag [0xaf6430] puts it at the human's (00559e60); under the profiler scope the water
 * (0056f290), the cannonball zones (0056fa50), the ground (0056fcf0); the network ball (ONLINE).
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x54; push ebx / esi / edi (F); dt pushed through x87 for Activity::vf28; the
 * ball's / probe position F + 0x10, the kick F + 0x20, the fire's lift F + 0x24, the probe F + 0x30, the ball's position F + 0x40, the
 * scope F + 0xc. */
void rx_tag_vf28(rx_addr act, float dt) {
    ACT_FRAME_EBP(F, 16, 0x54, 3);
    SU32(F, 0xc, 0);
    ACT_CALL(1, rx_activity_in_progress(act, x87_copy(dt)));
    const rx_addr holder = U32(act, 0xff078);
    if (holder != 0) {
        const float left = F32(act, 0xff38c) - F32(holder, 0xf14);
        const float lead = (float)I32(act, 0x2c0) + 1.0f;
        if (lead >= left) {
            const int32_t n = I32(act, 0x2a0);
            if ((float)n >= left) {
                SU32(act, 0x2a0, (uint32_t)(n - 1));
                ACT_CALL(1, rx_event_activity_int(U32(act, 0x290), 0x00941bec /* "FinishCountdown Step" */, act, (uint32_t)n));
            }
        } else if (left > lead) SU32(act, 0x2a0, U32(act, 0x2c0));
    }
    ACT_CALL(0, rx_tag_ball_visible(act));
    SF32(F, 0x24, 0.0f);
    if (U32(act, 0xff078) != 0) SF32(F, 0x24, 2.5f /* 0x9d789c */);
    const rx_addr terrain = scene_terrain(U32(U32(act, 0x234), 0x20138));
    if (U32(0, 0x00af650c) != 0) {
        const uint32_t a[1] = {F + 0x10};
        (void)act_vcall(BALL(act), 0x44, 1, a);
        SF32(act, 0xff120, F32(F, 0x10));
        SF32(act, 0xff124, F32(F, 0x14) + F32(F, 0x24));
        SF32(act, 0xff128, F32(F, 0x18));
    }
    if (U32(act, 0xff078) != 0) {
        SU32(0, 0x009c5198, 0x41f00000u);   /* 30.0, 0x931d98 */
        SU32(act, 0xff370, 0);
        SU32(act, 0xff374, 0);
    } else {
        const float clock = F32(0, 0x009c5198) - dt;
        SF32(0, 0x009c5198, clock);
        if (0.0f > clock) {
            if (U32(act, 0xff374) == 3) {
                SU32(act, 0xff374, 0);
                ACT_CALL(2, rx_tag_ball_physics(act, 0));
                { const uint32_t a[2] = {act + 0xff0a0, act + 0xff0b0}; (void)act_vcall(BALL(act), 0x70, 2, a); }
                ACT_CALL(2, rx_tag_ball_physics(act, 1));
                SU32(0, 0x009c5198, 0x41f00000u);
            } else {
                SU32(act, 0xff374, U32(act, 0xff374) + 1);
                SU32(F, 0x10, U32(0, 0x00a95d50)); SU32(F, 0x14, U32(0, 0x00a95d54)); SU32(F, 0x18, U32(0, 0x00a95d58));
                uint32_t edx;
                ACT_CALL(1, (void)phys_terrain_base_height_at(terrain, F + 0x10, &edx));
                { const uint32_t a[1] = {F + 0x40}; (void)act_vcall(BALL(act), 0x44, 1, a); }
                float x = F32(F, 0x10) - F32(F, 0x40);
                float z = F32(F, 0x18) - F32(F, 0x48);
                float y = F32(F, 0x14) + 1000.0f /* 0x9cf5b0 */;
                const float l2 = z * z + x * x + y * y;
                SF32(F, 0x20, x); SF32(F, 0x28, z); SF32(F, 0x24, y); SF32(F, 0xc, l2);
                if (l2 > 0.0f) {
                    float len;
                    ACT_CALLR(len, 1, phys_sqrtf(x87_copy(l2)));
                    const float inv = 1.0f / len;
                    y = inv * F32(F, 0x24);
                    x = F32(F, 0x20) * inv;
                    z = F32(F, 0x28) * inv;
                }
                const float k = 10000.0f;   /* 0x931e98 */
                SF32(F, 0x20, x * k); SF32(F, 0x24, y * k); SF32(F, 0x28, z * k);
                if (BALL_PH(act) != 0) {
                    const rx_addr world = U32(BALL_PH(act), 4);
                    ACT_CALL(1, rx_world_body_velocity_clear(world, rx_world_physics_handle_body(BALL_PH(act))));
                }
                ACT_CALL(2, rx_tag_ball_impulse(act, F + 0x20));
            }
        }
    }
    /* the HUD fade */
    {
        float f = F32(act, 0xff070);
        const float hidden = rx_bf(0xbf800000u);   /* -1, 0x9cd9e8 */
        if (U32(act, 0xff078) != 0) {
            if (!(f == hidden)) {
                f = f - dt;
                SF32(act, 0xff070, f);
                if (f <= 0.0f) SF32(act, 0xff070, hidden);   /* comiss 0, f; jb: kept above 0 (or NaN) */
            }
        } else {
            if (f == hidden) SF32(act, 0xff070, 0.0f);
            f = F32(act, 0xff070);
            const float top = 0.25f;   /* 0x92ff84 */
            if (top > f) {
                f = f + dt;
                SF32(act, 0xff070, f);
                if (f >= top) SF32(act, 0xff070, top);        /* comiss f, top; jb: kept below (or NaN) */
            }
        }
    }
    /* the terrain's bounds */
    {
        const uint32_t a[1] = {F + 0x30};
        (void)act_vcall(BALL(act), 0x44, 1, a);
        const float margin = F32(terrain, 0x44f0), x = F32(F, 0x30), z = F32(F, 0x38);
        if (F32(terrain, 0x44d8) > margin + x || x - margin > F32(terrain, 0x44e0) ||
            F32(terrain, 0x44dc) > z + margin || z - margin > F32(terrain, 0x44e4)) {
            SF32(F, 0x30, 50.0f /* 0x9d78dc */); SF32(F, 0x34, 10.0f /* 0x92fd88 */); SF32(F, 0x38, 50.0f);
            uint32_t edx;
            ACT_CALL(1, (void)phys_terrain_base_height_at(terrain, F + 0x30, &edx));
            SF32(F, 0x34, F32(F, 0x34) + 50.0f);
            ACT_CALL(2, rx_tag_ball_physics(act, 0));
            { const uint32_t b[2] = {F + 0x30, 0x00d61640}; (void)act_vcall(BALL(act), 0x70, 2, b); }
            ACT_CALL(2, rx_tag_ball_physics(act, 1));
        }
    }
    if (U32(0, 0x00af6430) != 0) {
        SU32(0, 0x00af6430, 0);
        rx_addr human;
        ACT_CALLR(human, 1, rx_activity_human_driver(act, 0));
        rx_addr m = act_vcall(U32(U32(human, 0x74), 0x220), 0x38, 0, 0);
        SF32(F, 0x10, F32(m, 0x30)); SF32(F, 0x14, F32(m, 0x34)); SF32(F, 0x18, F32(m, 0x38));
        (void)act_vcall(U32(U32(human, 0x74), 0x220), 0x38, 0, 0);
        ACT_CALL(2, rx_tag_ball_physics(act, 0));
        { const uint32_t b[2] = {F + 0x10, 0x00d61640}; (void)act_vcall(BALL(act), 0x70, 2, b); }
        ACT_CALL(2, rx_tag_ball_physics(act, 1));
    }
    session_static();
    const rx_addr scope = race_profile_begin();
    SU32(F, 0xc, scope);
    ACT_CALL(2, rx_tag_ball_water(act, x87_copy(dt)));
    ACT_CALL(0, rx_tag_ball_cannonball(act));
    ACT_CALL(0, rx_tag_ball_ground(act));
    if (U32(act, 0xff380) == 0) {
        session_static();
        if (session_networked()) act_unreached(0x0056ef0d, "ONLINE: the network TagBall object found (host 008879b0 / client 006e4fa0)");
    }
    if (U32(act, 0xff380) != 0) act_unreached(0x0056f037, "ONLINE: the network TagBall's holder and pose sync");
    race_profile_end(U32(F, 0xc));
    ACT_END(F);
}
RX_FUNCTION(0x0056e940, rx_tag_vf28)

/* 00570f00 slot 29 (thiscall; dt; RET 4): Post. The HUD fade (as vf28's); the Post clock +0xff064: at (or below) 0 the results are shown
 * (00559a40) when the class allows it (vf57, slot 0xe4); the clock grows by dt. Frame: push ebp; mov ebp, esp; and esp, -16; sub esp,
 * 0xc; push esi. */
void rx_tag_vf29(rx_addr act, float dt) {
    ACT_FRAME_EBP(F, 16, 0xc, 1);
    float f = F32(act, 0xff070);
    const float hidden = rx_bf(0xbf800000u);   /* -1, 0x9cd9e8 */
    if (U32(act, 0xff078) != 0) {
        if (!(f == hidden)) {
            f = f - dt;
            SF32(act, 0xff070, f);
            if (f <= 0.0f) SF32(act, 0xff070, hidden);
        }
    } else {
        if (f == hidden) SF32(act, 0xff070, 0.0f);
        f = F32(act, 0xff070);
        if (0.25f /* 0x92ff84 */ > f) {
            f = f + dt;
            SF32(act, 0xff070, f);
            if (f >= 0.25f) SF32(act, 0xff070, 0.25f);
        }
    }
    if (F32(act, 0xff064) <= 0.0f) {   /* comiss 0, +0xff064; jb: skipped above 0 (or NaN) */
        if (act_vcall(act, 0xe4, 0, 0) != 0) ACT_CALL(0, rx_waypoint_show_results(act));
    }
    SF32(act, 0xff064, F32(act, 0xff064) + dt);
    ACT_END(F);
}
RX_FUNCTION(0x00570f00, rx_tag_vf29)

/* 00570b30 slot 27 (thiscall; dt; RET 4): WindDown. The HUD fade; unless the match is over (+0xff394: +0xff3a0 = 1, a new match next) the
 * wind-down timer +0xff39c (3 s) runs down (+0xff3a0 = 0), then a spectator (+0xfe8) rejoins (ONLINE: the networked spectator's re-entry)
 * and the round restarts (vf16, slot 0x40); the activity's limit +0x2c4 = FLT_MAX; +0xff36c = 0 and Activity::vf27.
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x34; push ebx / esi / edi (F); the activity at F + 0x18, the count F + 0xc. */
void rx_tag_vf27(rx_addr act, float dt) {
    ACT_FRAME_EBP(F, 16, 0x34, 3);
    SU32(F, 0x18, act);
    {
        float f = F32(act, 0xff070);
        const float hidden = rx_bf(0xbf800000u);
        if (U32(act, 0xff078) != 0) {
            if (!(f == hidden)) {
                f = f - dt;
                SF32(act, 0xff070, f);
                if (f <= 0.0f) SF32(act, 0xff070, hidden);
            }
        } else {
            if (f == hidden) SF32(act, 0xff070, 0.0f);
            f = F32(act, 0xff070);
            if (0.25f > f) {
                f = f + dt;
                SF32(act, 0xff070, f);
                if (f >= 0.25f) SF32(act, 0xff070, 0.25f);
            }
        }
    }
    if (U32(act, 0xff394) != 0) SU32(act, 0xff3a0, 1);
    else {
        const float t = F32(act, 0xff39c) - dt;
        SU32(act, 0xff3a0, 0);
        SF32(act, 0xff39c, t);
        if (!(0.0f < t)) {   /* comiss 0, t; jb: running while above 0 (or NaN) */
            if (U32(act, 0xfe8) != 0) {
                const uint32_t n = U8(U32(0, 0x00a946b8), 0x1541c);
                SU32(F, 0xc, n);
                for (uint32_t i = 0; i < n; i++) {
                    rx_addr p;
                    ACT_CALLR(p, 2, rx_player_active(U32(0, 0x00a946b8) + 0x7460, i, 3));
                    if (U32(p, 4) == 0 || U32(p, 0x10) != 0 || U32(p, 0xb2c) == 0) continue;
                    if (U32(p, 0x908) != 0) {
                        session_static();
                        if (session_networked() && U32(p, 0xb2c) != 0 && U32(U32(p, 0xb2c), 0x1d8) != 0)
                            act_unreached(0x00570cb1, "ONLINE: a networked spectator rejoins the tag match");
                    }
                    break;
                }
            }
            (void)act_vcall(act, 0x40, 0, 0);
        }
        SU32(act, 0x2c4, 0x7f7fffffu);   /* FLT_MAX, 0x977494: also while the timer runs */
    }
    SU32(act, 0xff36c, 0);
    ACT_CALL(1, rx_activity_wind_down(act, x87_copy(dt)));
    ACT_END(F);
}
RX_FUNCTION(0x00570b30, rx_tag_vf27)

/* 00587d00 "Player Wins" (ESI = manager, ECX = driver, EDX = text; args: activity, value; RET 8), at the original's frame (sub esp, 0x18;
 * push edi: the payload F + 4) because the dispatch's handlers keep the payload's stack address (the world's heap at 0x30000000); the
 * registered race_event.c port raises it from a generic frame. */
static void tag_player_wins(rx_addr manager, rx_addr activity, rx_addr driver, rx_addr text, uint32_t value) {
    ACT_FRAME(F, 0x18, 1);
    SU32(F, 4, activity); SU32(F, 8, driver); SU32(F, 0xc, text); SU32(F, 0x10, value);
    act_raise(manager, 0x00941ca8 /* "Player Wins" */, 0x009cf630, F + 4, 0x10);
    ACT_END(F);
}

/* 00570970 slot 40 (thiscall; old state; RET 4): WindDown begins. Activity::vf40; the wind-down timer 3 s; for a single-player activity
 * (mode +0x238 == 1) the round winner's message ("%s %s": the winner's name, the localized STR_YOU_WIN (0x941fc4): UI text, the oracle
 * runs the original localization chain, the product leaves an empty string, as race_section.c's vf42) raised as "Player Wins" (00587d00,
 * value 1); "Hide Arrow" for every driver and its round's time added to its total (+0xf18 += +0xf14); then "Match Over" (the match is
 * over, +0xff394) or "Round Over" with the activity.
 * Frame: sub esp, 0x210; push ebx / esi / edi (F); the UTF-8 name F + 0x14, the text F + 0x114; + push ebp in the loop. */
void rx_tag_vf40(rx_addr act, uint32_t old_state) {
    ACT_FRAME(F, 0x210, 3);
    ACT_CALL(1, rx_activity_wind_down_enter(act));
    (void)old_state;
    SF32(act, 0xff39c, 3.0f /* 0x92fe48 */);
    if (U32(act, 0x238) == 1) {
        if (U32(act, 0xff080) == 0) {
#ifdef RX_ORACLE
            uint32_t zero = 0;
            const uint32_t lang = rx_call_original(0x005dfc90, RX_CDECL, 0, 1, &zero);
            rx_addr entry;
            ACT_CALLR(entry, 0, rx_ui_language_lookup(0x00941fc4, lang));
            { uint32_t a[8] = {0xfde9, 0, U32(entry, 0x10), 0xffffffffu, F + 0x14, 0x100, 0, 0}; rx_os_import(0x009160a8, 8, a); }
            SU8(F, 0x113, 0);
            SU32(F, -12, 0x00941fd0); SU32(F, -8, U32(U32(act, 0xff398), 0x74) + 0x54); SU32(F, -4, F + 0x14);
            rx_set_sp(F - 16);
            rx_format_buffer(F + 0x114, 0x104, 0x00941fd0 /* "%s %s" */, F - 8);
            rx_set_sp(F);
#else
            SU8(F, 0x114, 0);
#endif
        }
        ACT_CALL(2, tag_player_wins(U32(act, 0x290), act, U32(act, 0xff398), F + 0x114, 1));
    }
    rx_set_sp(F - 4);   /* push ebp */
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        const rx_addr d = U32(U32(act, 0x268), 4u * (uint32_t)i);
        SU32(F, 0xc, U32(act, 0x290));
        SU32(F, 4, d);
        act_raise(U32(act, 0x290), 0x0093eef0 /* "Hide Arrow" */, 0x009cda44, F + 4, 4);
        SF32(d, 0xf18, F32(d, 0xf14) + F32(d, 0xf18));
    }
    rx_set_sp(F);
    SF32(act, 0xff39c, 3.0f);
    SU32(F, 0xc, act);
    act_raise(U32(act, 0x290), U32(act, 0xff394) != 0 ? 0x009421ccu /* "Match Over" */ : 0x009421c0u /* "Round Over" */, 0x009cbd24, F + 0xc, 4);
    ACT_END(F);
}
RX_FUNCTION(0x00570970, rx_tag_vf40)

/* 005700a0 slot 42 (ECX = activity): the round's end check. (ONLINE: a client waits for the host's round end +0xff36c.) Free for all: a
 * driver whose round time reached the target (+0xf14 >= +0xff38c) wins the round (+0xf1c + 1; a network ball only on its owner) and is
 * the round's winner; the driver with the most rounds won is the leader. Teams: the team times summed (+0xf20). A round won (a team's
 * time at the target: +0xff084 / +0xff088 + 1, +0xff08c the team) ends it: the leader (or a team) with the rounds to win (+0xff390) wins
 * the match (+0xff398, +0xff394 = 1); a network ball reports it (ONLINE: 007001a0); then WindDown (state 3).
 * Frame: sub esp, 8; push ebx / ebp / esi / edi; the team times F + 0x10 / F + 0x14. */
void rx_tag_vf42(rx_addr act) {
    ACT_FRAME(F, 8, 4);
    session_static();
    if (session_networked()) {
        session_static();
        if (U32(U32(0, 0x00da6cb0), 0x1c48) == 0 && U32(act, 0xff36c) == 0 && U32(act, 0xbc0) == 0) { ACT_END(F); return; }
        act_unreached(0x0057012a, "ONLINE: the networked tag round end");
    }
    rx_addr best = 0;
    uint32_t won = 0;
    float team1 = 0.0f, team0 = 0.0f;
    SF32(F, 0x14, team0);
    SF32(F, 0x10, team1);
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        const rx_addr d = U32(U32(act, 0x268), 4u * (uint32_t)i);
        const float f = F32(d, 0xf14);
        if (U32(act, 0xff080) == 0) {
            if (f >= F32(act, 0xff38c)) {
                if (U32(act, 0xff380) == 0 || (U32(act, 0xff380) != 0 && (rx_net_ref_valid(U32(act, 0xff380)) & 0xff) != 0)) SU32(d, 0xf1c, U32(d, 0xf1c) + 1);
                team1 = F32(F, 0x10);
                team0 = F32(F, 0x14);
                won = 1;
                SU32(act, 0xff398, d);
            }
            if (best == 0 || I32(d, 0xf1c) > I32(best, 0xf1c)) best = d;
        } else if (U32(d, 0xf20) != 0) {
            team1 = f + team1;
            SF32(F, 0x10, team1);
        } else {
            team0 = f + team0;
            SF32(F, 0x14, team0);
        }
    }
    const float target = F32(act, 0xff38c);
    if (team1 >= target) {
        SU32(act, 0xff084, U32(act, 0xff084) + 1);
        SU32(act, 0xff08c, 1);
    } else if (team0 >= target) {
        SU32(act, 0xff088, U32(act, 0xff088) + 1);
        SU32(act, 0xff08c, 0);
    } else if (won == 0) { ACT_END(F); return; }
    if (U32(act, 0xff080) == 0) {
        if (I32(best, 0xf1c) >= I32(act, 0xff390)) { SU32(act, 0xff398, best); SU32(act, 0xff394, 1); }
    } else {
        const int32_t rounds = I32(act, 0xff390);
        if (I32(act, 0xff084) >= rounds) { SU32(act, 0xff08c, 1); SU32(act, 0xff394, 1); }
        else if (I32(act, 0xff088) >= rounds) { SU32(act, 0xff08c, 0); SU32(act, 0xff394, 1); }
    }
    if (U32(act, 0xff380) != 0 && (rx_net_ref_valid(U32(act, 0xff380)) & 0xff) != 0) act_unreached(0x0057029a, "ONLINE: the round's end sent (007001a0)");
    ACT_CALL(2, rx_activity_set_state(act, 3));
    ACT_END(F);
}
RX_FUNCTION(0x005700a0, rx_tag_vf42)
