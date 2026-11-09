/* race_tag_driver.c -- the Tag party mode's TagDriver (vtable 0x0093fe24, 0xf30 bytes: a NetworkDriver) and its RuleTag (vtable
 * 0x0093fe70: the grab / drop rule), hand-transliterated from the original (each function's address is in its comment; frames and the
 * stack depth of each call are the original's).
 *
 * TagDriver adds +0xf10 the activity, +0xf14 the time it held the ball this round (capped at the round's target), +0xf18 its total over
 * the rounds, +0xf1c the rounds won, +0xf20 its team (-1 free for all), +0xf24 a water kick pending, +0xf28 the rider's head bone.
 * RuleTag: +4 the driver, +8 enabled, +0xc this driver holds the ball, +0x10 the clock before which it cannot grab it again.
 *
 * ONLINE (the driver's network object +0xef0, a Quazal DuplicatedObject; never made offline): its score / time sync (0050bb90 with
 * 006fea10, 007006e0, 00700310 / 006fed10) and the ball request (006ffdb0) stop with act_unreached. A RuleTag of a driver with a
 * network object grabs only on the host (the session's +0x1c48) for a non-spectator. */
#include "race_tag.h"
#include "race_driver.h"
#include "race_misc.h"
#include "race_extern.h"
#include "race.h"
#include "race_challenge_activity.h"
#include "build_activity.h"
#include "build_activity_int.h"
#include "build_driver_reset.h"
#include "build_driver_dtor.h"
#include "build_rider.h"
#include "build_alloc.h"
#include "world_veh.h"
#include "phys_body.h"
#include "fx.h"
#include "loop_anim.h"
#include "race_beh1_extern.h"
#include "crt.h"
#include <string.h>

#define VT_TAG_DRIVER 0x0093fe24u
#define VT_RULE_TAG   0x0093fe70u
#define TD_STRING     0x009cdb68u   /* void(const char*) */
#define TD_DRIVER     0x009cda44u   /* void(BaseDriver*) */
#define STR_TAG_CHANGED 0x0093efc4u /* "Tag It Changed" */
#define STR_SHOW_ARROW  0x0093ef30u /* "Show Arrow" */
#define STR_EMPTY       0x00935d6cu /* "" */

void rx_waypoint_network_vf14(rx_addr driver);   /* 0050ec30 NetworkDriver::vf14 (waypoint_driver_network.c) */

static void session_static(void) {
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
}
/* fld dword / fstp dword: the bits, a signalling NaN quieted */
static float x87_copy(float v) {
    uint32_t b = rx_fb(v);
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu)) b |= 0x400000u;
    return rx_bf(b);
}
/* the driver's network object is live (+0xef0 set, its flag +0x20 bit 0, 00884950): ONLINE only */
static int net_object_live(rx_addr d) {
    const rx_addr net = U32(d, 0xef0);
    return net != 0 && (U8(net, 0x20) & 1) != 0 && (rx_net_ref_valid(net) & 0xff) != 0;
}

/* ======================================================================================================================== */
/* TagDriver                                                                                                                 */

/* 00514ce0 (ECX = driver, EAX = player; stdcall activity, team; RET 8) -> driver: NetworkDriver's constructor (0050e1b0), the class's
 * fields, kind +0xee8 = 6. Frame: push edi. */
rx_addr rx_tag_driver_construct(rx_addr d, rx_addr act, rx_addr player, uint32_t team) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(1, rx_network_driver_construct(d, player));
    SU32(d, 0xf1c, 0);
    SU32(d, 0xf24, 0);
    SU32(d, 0xf28, 0);
    SU32(d, 0, VT_TAG_DRIVER);
    SU32(d, 0xf10, act);
    SU32(d, 0xee8, 6);
    SU32(d, 0xf20, team);
    ACT_END(F);
    return d;
}
RX_FUNCTION(0x00514ce0, rx_tag_driver_construct)

/* 00514d30 slot 0 (thiscall; flags; RET 4) -> this: NetworkDriver's destructor (0050e2f0), freed with flags & 1. Frame: push esi. */
rx_addr rx_tag_driver_sdtor(rx_addr d, uint32_t flags) {
    ACT_FRAME(F, 0, 1);
    SU32(d, 0, VT_TAG_DRIVER);
    ACT_CALL(0, rx_network_driver_dtor(d));
    if (flags & 1) ACT_CALL(1, rx_reflex_aligned_free(d));
    ACT_END(F);
    return d;
}
RX_FUNCTION(0x00514d30, rx_tag_driver_sdtor)

/* 00514d60 slot 1 (ECX = driver): under the engine's profiler scope, a live network object sends the driver's score (ONLINE), then
 * NetworkDriver::vf1. Frame: push ebp; mov ebp, esp; and esp, -8; push ebx / ebp / esi / edi. */
void rx_tag_driver_vf1(rx_addr d) {
    ACT_FRAME_EBP(F, 8, 0, 4);
    session_static();
    const rx_addr scope = race_profile_begin();
    if (net_object_live(d)) act_unreached(0x00514df1, "ONLINE: the tag driver's score sent (0050bb90 / 006fea10)");
    ACT_CALL(0, rx_network_driver_vf1(d));
    race_profile_end(scope);
    ACT_END(F);
}
RX_FUNCTION(0x00514d60, rx_tag_driver_vf1)

/* 00514e40 slot 2 (ECX = driver): under the profiler scope, a live network object's link released (ONLINE), then BaseDriver::vf2.
 * Frame: push esi / edi; the scope's leave is a tail jump. */
void rx_tag_driver_vf2(rx_addr d) {
    ACT_FRAME(F, 0, 2);
    session_static();
    const rx_addr scope = race_profile_begin();
    if (net_object_live(d) && U32(U32(U32(d, 0xef0), 0x1d4), 0) != 0) act_unreached(0x00514eda, "ONLINE: the tag driver's network link released (007006e0)");
    ACT_CALL(0, rx_base_driver_vf2(d));
    race_profile_end(scope);
    ACT_END(F);
}
RX_FUNCTION(0x00514e40, rx_tag_driver_vf2)

/* 00514f20 slot 3 (ECX = driver): the round's reset. NetworkDriver::vf3; a local player's vehicle effects at its start (00460020 with
 * the start's position and direction); +0x3c0 = 0; its RuleTag no longer holds; "Show Arrow" raised; the round's time cleared (the
 * total too for a new match); the rider's head bone (00830160 "MXRider_Head"); a live network object's score (ONLINE); a new match's
 * rounds won cleared; the behaviours' and rules' resets (005092b0 / 005092f0); a spectator's vehicle made visible again (+0x14e of
 * its model, vf49 slot 0xc4 with 0, its body record +0x14 = -1).
 * Frame: push ebp; mov ebp, esp; and esp, -8; push ecx / ebp / esi / edi (the event's payload / the scope at F + 0xc). */
void rx_tag_driver_vf3(rx_addr d) {
    ACT_FRAME_EBP(F, 8, 0, 4);
    ACT_CALL(0, rx_network_driver_vf3(d));
    const rx_addr player = U32(d, 0x74);
    if (U32(player, 4) != 0) {
        const rx_addr start = U32(d, 0x144);
        ACT_CALL(2, rx_vehicle_effect_query(U32(player, 0x1ac), start, start + 0x10));
    }
    SF32(d, 0x3c0, 0.0f);
    rx_addr rule;
    ACT_CALLR(rule, 1, rx_tag_driver_rule(d));
    SU32(rule, 0xc, 0);
    SU32(F, 0xc, d);
    act_raise(U32(U32(U32(d, 0x74), 0x94), 0x290), STR_SHOW_ARROW, TD_DRIVER, F + 0xc, 4);
    if (U32(U32(d, 0xf10), 0xff3a0) != 0) SF32(d, 0xf18, 0.0f);
    SF32(d, 0xf14, 0.0f);
    const rx_addr rider = U32(U32(d, 0x74), 0x1b4);
    if (rider != 0) {
        int32_t bone;
        ACT_CALLR(bone, 1, rx_rider_bone_index(U32(rider, 0x144), 0x0093efb4 /* "MXRider_Head" */));
        SU32(d, 0xf28, (uint32_t)bone);
    }
    session_static();
    const rx_addr scope = race_profile_begin();
    SU32(F, 0xc, scope);
    if (net_object_live(d)) act_unreached(0x0051507e, "ONLINE: the tag driver's score sent (0050bb90 / 006fea10)");
    if (U32(U32(U32(d, 0x74), 0x94), 0xff3a0) != 0) SU32(d, 0xf1c, 0);
    ACT_CALL(1, rx_driver_behaviors_vf6(d));
    ACT_CALL(1, rx_driver_rules_vf6(d));
    const rx_addr p = U32(d, 0x74);
    if (U32(p, 0x1e4) != 0) {
        const rx_addr veh = U32(p, 0x1ac);
        SU8(U32(veh, 0x1328), 0x14e, 0);
        const uint32_t a[1] = {0};
        (void)act_vcall(U32(U32(d, 0x74), 0x1ac), 0xc4, 1, a);
        const rx_addr h = U32(U32(U32(d, 0x74), 0x1ac) + 0x178, 4);
        if (h != 0) SU32(U32(phys_body_record(h), 0x2c), 0x14, 0xffffffffu);
    }
    race_profile_end(U32(F, 0xc));
    ACT_END(F);
}
RX_FUNCTION(0x00514f20, rx_tag_driver_vf3)

/* 00515330 (ESI = driver; stdcall dt; RET 4): a computer rider's drive. The steering / throttle / brake (+0x60 .. +0x68) cleared, every
 * rule's vf5 (slot 0x14) with 0, the behaviours' vf4 (slot 0x10) with dt until one takes the drive (non-zero); the throttle kept
 * (+0x3dc); unless the vehicle says otherwise (its vf43, slot 0xac) the speed limit +0x3d4 = the throttle times the AI ability's speed
 * percentage (+0x18 + 0x468: [class * 4 + skill], x 0.01) or x 0.5 without one; +0x3c0 = 1. Frame: push ebx / edi (popped before the
 * tail). */
void rx_tag_driver_ai(rx_addr d, float dt) {
    ACT_FRAME(F, 0, 2);
    SF32(d, 0x60, 0.0f); SF32(d, 0x64, 0.0f); SF32(d, 0x68, 0.0f);
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
    SU32(d, 0x3dc, rx_fb(x87_copy(F32(d, 0x60))));
    const uint32_t own = act_vcall(U32(U32(d, 0x74), 0x1ac), 0xac, 0, 0);
    ACT_END(F);
    if (own == 0) {
        const rx_addr ab = U32(d, 0x18);
        float v;
        if (ab != 0) {
            const rx_addr veh = U32(U32(d, 0x74), 0x1ac);
            const float pct = F32(ab, 0x468 + 4u * (U32(veh, 0x198) + U32(veh, 0x194) * 4u));
            if (!(pct == 0.0f)) v = pct * rx_bf(0x3c23d70au) /* 0.01, 0x9322ac */ * F32(d, 0x3dc);
            else v = F32(d, 0x3dc) * rx_bf(0x3f000000u) /* 0.5, 0x9322cc */;
        } else v = F32(d, 0x3dc) * rx_bf(0x3f000000u) /* 0.5, 0x9322cc */;
        SF32(d, 0x3d4, v);
    }
    SF32(d, 0x3c0, 1.0f);
}
RX_FUNCTION(0x00515330, rx_tag_driver_ai)

/* 00515440 (ESI = driver): a pending water kick (+0xf24) ends once the vehicle (its vf17 position, slot 0x44) is out of every cannonball
 * zone (00519fa0). Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x1c; push edi; the position at F + 0x14 (F after the push). */
void rx_tag_driver_water_kick(rx_addr d) {
    ACT_FRAME_EBP(F, 16, 0x1c, 1);
    if (U32(d, 0xf24) != 0) {
        const uint32_t a[1] = {F + 0x14};
        (void)act_vcall(U32(U32(d, 0x74), 0x1ac), 0x44, 1, a);
        int inside;
        ACT_CALLR(inside, 0, rx_cannonball_inside(F + 0x14, 0));
        if (inside == 0) SU32(d, 0xf24, 0);
    }
    ACT_END(F);
}
RX_FUNCTION(0x00515440, rx_tag_driver_water_kick)

/* 00515140 slot 7 (thiscall; dt; RET 4): the driver's update. A spectator (+0x908) runs NetworkDriver::vf7 only. A human runs its rules
 * (00516190), a local computer rider its drive (00515330); while its RuleTag holds the ball the round's time grows by dt (capped at the
 * activity's target +0xff38c); NetworkDriver::vf7; outside a replay ([0xda6cb0] + 0x1c3c) a live network object takes the host's time
 * and score in InProgress / WindDown (ONLINE); the water kick (00515440).
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0xc4; push ebx / esi / edi; dt through x87. */
void rx_tag_driver_vf7(rx_addr d, float dt) {
    ACT_FRAME_EBP(F, 16, 0xc4, 3);
    const rx_addr p = U32(d, 0x74);
    if (U32(p, 0x908) != 0) {
        ACT_CALL(1, rx_network_driver_update(d, x87_copy(dt)));
        ACT_END(F);
        return;
    }
    if (U32(p, 0x10) == 0) ACT_CALL(1, rx_driver_rules_tick(d));
    else if (U32(p, 0x10) == 1 && U32(p, 4) != 0) ACT_CALL(1, rx_tag_driver_ai(d, x87_copy(dt)));
    rx_addr rule;
    ACT_CALLR(rule, 1, rx_tag_driver_rule(d));
    if (U32(rule, 0xc) != 0) {
        const float t = F32(d, 0xf14) + dt;
        const float cap = F32(U32(U32(d, 0x74), 0x94), 0xff38c);
        SF32(d, 0xf14, cap > t ? t : cap);   /* comiss xmm1, xmm0; ja: keep the sum (NaN takes the cap) */
    }
    ACT_CALL(1, rx_network_driver_update(d, x87_copy(dt)));
    session_static();
    if (U32(U32(0, 0x00da6cb0), 0x1c3c) == 0) {
        session_static();
        const rx_addr scope = race_profile_begin();
        const rx_addr net = U32(d, 0xef0);
        if (net != 0 && (U8(net, 0x20) & 1) != 0 && (rx_net_ref_valid(net) & 0xff) == 0) {
            const uint32_t st = U32(U32(U32(d, 0x74), 0x94), 0x21c);
            if (st == 2 || st == 3) act_unreached(0x005152b7, "ONLINE: the host's tag time and score (00700310 / 006fed10)");
        }
        race_profile_end(scope);
    }
    ACT_CALL(0, rx_tag_driver_water_kick(d));
    ACT_END(F);
}
RX_FUNCTION(0x00515140, rx_tag_driver_vf7)

/* 00515490 slot 9 (thiscall; event; RET 4): a vehicle event of kind 5 (a crash) or 7 (a cannonball zone: +0xf24, the water kick)
 * raises "Tag It Changed" with the holder's name (+0x54 of its player) or "" when nobody holds the ball; a live network object then asks
 * the session for the ball (ONLINE: 0053a210 / 00508880 / 006ffdb0). Frame: sub esp, 0x24; push ebx / esi / edi; the payload is the
 * argument slot. */
void rx_tag_driver_vf9(rx_addr d, rx_addr event) {
    ACT_FRAME(F, 0x24, 3);
    const uint32_t kind = act_vcall(event, 0xc, 0, 0);
    if (kind != 5) {
        if (kind != 7) { ACT_END(F); return; }
        SU32(d, 0xf24, 1);
    }
    const rx_addr act = U32(U32(d, 0x74), 0x94);
    const rx_addr slot = F + 0x34;   /* the argument slot (entry ESP + 4) */
    if (U32(act, 0xff078) != 0) {
        SU32(slot, 0, U32(d, 0x74) + 0x54);
        act_raise(U32(act, 0x290), STR_TAG_CHANGED, TD_STRING, slot, 4);
        if (U32(U32(act, 0xff078), 0xef0) != 0) act_unreached(0x00515511, "ONLINE: the ball's network request for the holder (006ffdb0)");
    } else {
        SU32(slot, 0, STR_EMPTY);
        act_raise(U32(act, 0x290), STR_TAG_CHANGED, TD_STRING, slot, 4);
        if (U32(d, 0xef0) != 0) act_unreached(0x0051556c, "ONLINE: the ball's network request (006ffdb0)");
    }
    ACT_END(F);
}
RX_FUNCTION(0x00515490, rx_tag_driver_vf9)

/* 005155d0 slot 16 (thiscall; kind, object; RET 8): NetworkDriver::vf16 (the network player's kind change), then for kind 1 with its own
 * live network object the score sent and the vehicle wrecked (+0x280 = 0x17, the simulation's +0x2374 = 1): ONLINE. Frame: push ebx /
 * esi / edi. */
void rx_tag_driver_vf16(rx_addr d, uint32_t kind, rx_addr object) {
    ACT_FRAME(F, 0, 3);
    ACT_CALL(2, rx_network_driver_vf16(d, kind, object));
    if (kind == 1 && U32(d, 0xef0) == object && (rx_net_ref_valid(object) & 0xff) != 0)
        act_unreached(0x005155fc, "ONLINE: the tag driver's network kind change (0050bb90)");
    ACT_END(F);
}
RX_FUNCTION(0x005155d0, rx_tag_driver_vf16)

/* 00515650 slot 17 (ECX = driver): the driver leaves mid-game: its score and times cleared, +0x3c0 = 0, then a jump to NetworkDriver::vf14. */
void rx_tag_driver_vf17(rx_addr d) {
    SU32(d, 0xf1c, 0);
    SF32(d, 0xf18, 0.0f);
    SF32(d, 0xf14, 0.0f);
    SF32(d, 0x3c0, 0.0f);
    rx_waypoint_network_vf14(d);
}
RX_FUNCTION(0x00515650, rx_tag_driver_vf17)

/* 0053b120 (stdcall driver; RET 4) -> the driver's first rule whose kind (its vf3, slot 0xc) is 5 (RuleTag), else 0.
 * Frame: push ebx / esi / edi. */
rx_addr rx_tag_driver_rule(rx_addr d) {
    ACT_FRAME(F, 0, 3);
    for (uint32_t i = 0; i < U32(d, 0xee4); i++) {
        const rx_addr r = U32(d, 0xa0 + 4 * i);
        if (r == 0) continue;
        if (act_vcall(r, 0xc, 0, 0) == 5) { ACT_END(F); return U32(d, 0xa0 + 4 * i); }
    }
    ACT_END(F);
    return 0;
}
RX_FUNCTION(0x0053b120, rx_tag_driver_rule)

/* ======================================================================================================================== */
/* RuleTag                                                                                                                   */

/* 005894e0 (ECX = driver) -> a new RuleTag from the driver's rule pool (+0x3e4, 0x20 bytes), appended to its rules (+0xa0, count
 * +0xee4; 18 at most: then 0): vtable 0x93fe70, +8 = 1. */
rx_addr rx_tag_rule_create(rx_addr d) {
    if (U32(d, 0xee4) >= 0x12) return 0;
    const rx_addr o = U32(d, 0x3e4);
    SU32(d, 0x3e4, o + 0x20);
    rx_addr r = 0;
    if (o != 0) { SU32(o, 0, VT_RULE_TAG); SU32(o, 8, 1); r = o; }
    SU32(d, 0xa0 + 4 * U32(d, 0xee4), r);
    const rx_addr out = U32(d, 0xa0 + 4 * U32(d, 0xee4));
    SU32(d, 0xee4, U32(d, 0xee4) + 1);
    return out;
}
RX_FUNCTION(0x005894e0, rx_tag_rule_create)

/* 0051bd30 slot 1 (thiscall; driver; RET 4): bound to the driver, enabled, not holding, the grab clock 0 */
void rx_tag_rule_vf1(rx_addr rule, rx_addr driver) {
    SU32(rule, 4, driver);
    SU32(rule, 8, 1);
    SU32(rule, 0xc, 0);
    SF32(rule, 0x10, 0.0f);
}
RX_FUNCTION(0x0051bd30, rx_tag_rule_vf1)

/* ---- the packed quaternion product as inlined in RuleTag::vf5 (sign vectors 0x93a1f0 / 0x93a200 / 0x93a210; the same lanes as
 * bike_math.h's bike_quat_mul): a * b = (A2 + A1) + (A3 + A0) with A0 = (a.x * b.wzyx) * (1,-1,1,-1), A1 = (a.y * b.zwxy) * (1,1,-1,-1),
 * A2 = (b.yxwz * a.z) * (-1,1,1,-1), A3 = a.w * b ---- */
typedef struct { float v[4]; } tag_q;
static tag_q tag_qmul(tag_q a, tag_q b) {
    static const float s0[4] = {1.0f, -1.0f, 1.0f, -1.0f}, s1[4] = {1.0f, 1.0f, -1.0f, -1.0f}, s2[4] = {-1.0f, 1.0f, 1.0f, -1.0f};
    static const int wzyx[4] = {3, 2, 1, 0}, zwxy[4] = {2, 3, 0, 1}, yxwz[4] = {1, 0, 3, 2};
    tag_q r;
    for (int i = 0; i < 4; i++) {
        const float A0 = (a.v[0] * b.v[wzyx[i]]) * s0[i];
        const float A1 = (a.v[1] * b.v[zwxy[i]]) * s1[i];
        const float A2 = (b.v[yxwz[i]] * a.v[2]) * s2[i];
        const float A3 = a.v[3] * b.v[i];
        r.v[i] = (A2 + A1) + (A3 + A0);
    }
    return r;
}
static tag_q tag_qld(rx_addr p) { tag_q q; for (int i = 0; i < 4; i++) q.v[i] = F32(p, 4u * (uint32_t)i); return q; }
static void tag_qst(rx_addr p, tag_q q) { for (int i = 0; i < 4; i++) SF32(p, 4u * (uint32_t)i, q.v[i]); }

/* 0051bd70 slot 5 (thiscall; dt; RET 4): the grab / hold / drop rule, under the engine's profiler scope.
 *   Not holding (+0xc == 0): once the driver's clock (+0x130) is past the rule's (+0x10) (the vehicle simulation's +0x2f00 += 0), and
 *   offline (no network object; ONLINE: only the host for a non-spectator), a vehicle within the class's grab radius (0x9e38e8[class],
 *   squared) of the ball (its vf17 position) takes it: the old holder's rule lets go for 5 s (its clock + 5) and shows its arrow, the
 *   holder's ball is shown (a local human: in 3 frames), "Hide Arrow" for the new holder, +0xff078 = the driver, the ball's physics off,
 *   the new local human holder hides the ball (+0x230: first person), and the driver's vf9 (slot 0x24) gets the rule as its event
 *   (kind 5: "Tag It Changed").
 *   Holding: the ball rides on the rider's head: the head bone's model transform (0082e610), its local rotation turned by the constant
 *   angle pi/2 about z (the inlined sine / cosine polynomials, Matrix3_ToQuaternionToEsi), the head offset 1.55 ft up, the ball placed there
 *   (its vf28, slot 0x70), its physics off (the simulation's +0x2f00 += 3); a crash (the vehicle's +0x280) or a water kick (+0xf24) drops
 *   it: the holder's rule lets go for 5 s, "Show Arrow" for the old holder, the ball put 7 ft above the vehicle with the physics on and the
 *   vehicle's velocity x 500 + its angular velocity x 2000 as the impulse (0056e320), +0xff078 = 0, the driver's vf9 with the rule.
 *   With the debug flag [0xae1cbc] the vehicle's position (vf17) is read once more (unused).
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0xd4; push ebx / esi / edi (F); the rule F + 0x14, the scope F + 0x18, the network
 * object F + 0x1c, the impulse / the debug position F + 0x20, the quaternion F + 0x20, the ball's position F + 0x30, the rotation matrix
 * F + 0x40, the head's model transform F + 0x80 (rotation, F + 0x90 position, F + 0xa0 flags), the ball's orientation F + 0xb0 and
 * position F + 0xc0. */
void rx_tag_rule_vf5(rx_addr rule, float dt) {
    (void)dt;
    ACT_FRAME_EBP(F, 16, 0xd4, 3);
    const rx_addr act = U32(U32(U32(rule, 4), 0x74), 0x94);
    SU32(F, 0x14, rule);
    session_static();
    const rx_addr scope = race_profile_begin();
    SU32(F, 0x18, scope);
    const rx_addr driver = U32(rule, 4);
    const rx_addr net = U32(driver, 0xef0);
    SU32(F, 0x1c, net);
    if (U32(rule, 0xc) == 0) {
        if (F32(driver, 0x130) > F32(rule, 0x10)) {
            const rx_addr sim = U32(U32(U32(driver, 0x74), 0x1ac), 0x234);
            SF32(sim, 0x2f00, F32(sim, 0x2f00) + 0.0f);
            int may = net == 0;
            if (!may) {
                const rx_addr mgr = rx_fx_session_manager();
                may = U32(U32(mgr, 0x10), 0x1c48) != 0 && U32(U32(driver, 0x74), 0x1e4) == 0;
                if (may) act_unreached(0x0051be93, "ONLINE: the host's grab check for a networked driver");
            }
            if (may) {
                { const uint32_t a[1] = {F + 0x30}; (void)act_vcall(U32(act, 0xff090), 0x44, 1, a); }
                const rx_addr veh = U32(U32(U32(rule, 4), 0x74), 0x1ac);
                const float dy = F32(F, 0x34) - F32(veh, 0xc4);
                const float dx = F32(F, 0x30) - F32(veh, 0xc0);
                const float dz = F32(F, 0x38) - F32(veh, 0xc8);
                const float d2 = dy * dy + dx * dx + dz * dz;
                if (F32(0, 0x009e38e8 + 4 * U32(veh, 0x194)) > d2) {
                    SU32(rule, 0xc, 1);
                    const rx_addr old = U32(act, 0xff078);
                    if (old != 0) {
                        const float t = F32(old, 0x130) + 5.0f /* 0x935890 */;
                        SF32(F, 0x14, t);
                        rx_addr r;
                        ACT_CALLR(r, 1, rx_tag_driver_rule(old));
                        SF32(r, 0x10, F32(F, 0x14));
                        ACT_CALLR(r, 1, rx_tag_driver_rule(U32(act, 0xff078)));
                        SU32(r, 0xc, 0);
                        ACT_CALL(1, rx_event_driver(U32(act, 0x290), STR_SHOW_ARROW, U32(act, 0xff078)));
                        const rx_addr p = U32(U32(act, 0xff078), 0x74);
                        int local = 0;
                        if (p != 0) ACT_CALLR(local, 0, rx_player_is_local_human(p));
                        if (local != 0) { SU32(act, 0xff3a4, 1); SU32(act, 0xff3a8, 3); }
                    }
                    ACT_CALL(1, rx_event_driver(U32(act, 0x290), 0x0093eef0 /* "Hide Arrow" */, U32(rule, 4)));
                    SU32(act, 0xff078, U32(rule, 4));
                    ACT_CALL(2, rx_tag_ball_physics(act, 0));
                    const rx_addr p = U32(U32(rule, 4), 0x74);
                    int local = 0;
                    if (p != 0) ACT_CALLR(local, 0, rx_player_is_local_human(p));
                    if (local != 0 && U32(p, 0x230) != 0) { SU32(act, 0xff3a4, 0); SU32(act, 0xff3a8, 0); }
                    const uint32_t a[1] = {rule};
                    (void)act_vcall(U32(rule, 4), 0x24, 1, a);
                }
            }
        }
    } else {
        const rx_addr ch = U32(U32(U32(driver, 0x74), 0x1b4), 0x144);
        const uint32_t bone = U32(driver, 0xf28);
        for (uint32_t k = 0; k < 16; k += 4) SU32(F + 0x80, k, U32(0, 0x009cb550 + k));
        SU32(F, 0x90, U32(0, 0x00a95d50)); SU32(F, 0x94, U32(0, 0x00a95d54)); SU32(F, 0x98, U32(0, 0x00a95d58));
        SU8(F, 0xa0, 0); SU8(F, 0xa1, 0);
        ACT_CALL(1, rx_skeleton_bone_to_model_scalar(ch, bone, U32(ch, 0x228), F + 0x80));
        const rx_addr d2 = U32(U32(F, 0x14), 4);
        const rx_addr sim = U32(U32(U32(d2, 0x74), 0x1ac), 0x234);
        const rx_addr head = U32(ch, 0x228) + 0x30u * U32(d2, 0xf28);
        SF32(sim, 0x2f00, F32(sim, 0x2f00) + 3.0f /* 0x92fe48 */);
        /* the angle 0 * 2pi reduced from pi/2 (0x977980): t = pi - (1.5707961 - 0) */
        const float t = rx_bf(0x40490fdbu) /* pi, 0x977498 */ - (rx_bf(0x3fc90fd9u) /* 0x977980 */ - 0.0f * rx_bf(0x40c90fdbu) /* 0x97749c */);
        const float t2 = t * t, t4 = t2 * t2;
        const float c = rx_bf(0x80000000u) /* -0.0, 0x9774a8 */ - b1_cos_poly(t2, t4);
        const float s = b1_sin_poly(t2, t4) * t * rx_bf(0xbf800000u) /* -1, 0x9cd9e8 */;
        const float ns = rx_bf(0x80000000u) - s;
        const rx_addr M = F + 0x40;
        static const float zero = 0.0f;
        SF32(M, 0x00, c); SF32(M, 0x04, s); SF32(M, 0x08, zero); SF32(M, 0x0c, zero);
        SF32(M, 0x10, ns); SF32(M, 0x14, c); SF32(M, 0x18, zero); SF32(M, 0x1c, zero);
        SF32(M, 0x20, zero); SF32(M, 0x24, zero); SF32(M, 0x28, 1.0f); SF32(M, 0x2c, zero);
        SF32(M, 0x30, zero); SF32(M, 0x34, zero); SF32(M, 0x38, zero); SF32(M, 0x3c, 1.0f);
        ACT_CALL(1, rx_world_matrix3_to_quaternion_to(F + 0x20, M));
        const tag_q r1 = tag_qmul(tag_qld(head), tag_qld(F + 0x20));
        const tag_q r2 = tag_qmul(tag_qld(F + 0x80), r1);
        tag_qst(F + 0xb0, r2);
        tag_q v = {{F32(head, 0x10), F32(head, 0x14) + rx_bf(0x3fc66666u) /* 1.55, 0x9cd98c */, F32(head, 0x18), 0.0f}};
        tag_q cj;
        for (int i = 0; i < 4; i++) cj.v[i] = (i < 3 ? -1.0f : 1.0f) * r2.v[i];   /* 0x93a220 * r2 */
        const tag_q pos = tag_qmul(r2, tag_qmul(v, cj));
        for (int i = 0; i < 3; i++) SF32(F + 0xc0, 4u * (uint32_t)i, pos.v[i] + F32(F + 0x90, 4u * (uint32_t)i));
        SF32(F + 0xc0, 0xc, pos.v[3] + F32(F + 0x90, 0xc));
        { const uint32_t a[2] = {F + 0xc0, F + 0xb0}; (void)act_vcall(U32(act, 0xff090), 0x70, 2, a); }
        ACT_CALL(2, rx_tag_ball_physics(act, 0));
        const rx_addr nobj = U32(F, 0x1c);
        int go = 1;
        if (nobj != 0 && (rx_net_ref_valid(nobj) & 0xff) == 0) go = 0;
        if (go) {
            const rx_addr r = U32(F, 0x14);
            const rx_addr d = U32(r, 4);
            const float wrecked = U32(U32(U32(d, 0x74), 0x1ac), 0x280) != 0 ? 1.0f : 0.0f;
            if (U32(d, 0xf24) != 0 || !(wrecked == 0.0f)) {
                const rx_addr old = U32(act, 0xff078);
                if (old != 0) {
                    SF32(F, 0x1c, F32(old, 0x130) + 5.0f /* 0x935890 */);
                    rx_addr rr;
                    ACT_CALLR(rr, 1, rx_tag_driver_rule(old));
                    SF32(rr, 0x10, F32(F, 0x1c));
                    ACT_CALLR(rr, 1, rx_tag_driver_rule(U32(act, 0xff078)));
                    SU32(rr, 0xc, 0);
                    if (nobj == 0 || U32(act, 0xff078) == U32(r, 4))
                        ACT_CALL(1, rx_event_driver(U32(act, 0x290), STR_SHOW_ARROW, U32(act, 0xff078)));
                }
                const rx_addr veh = U32(U32(U32(r, 4), 0x74), 0x1ac);
                const float k0 = 2000.0f /* 0x977e10 */, k6 = 500.0f /* 0x9778ec */;
                SF32(F, 0x20, F32(veh, 0x130) * k6 + F32(veh, 0x140) * k0);
                SF32(F, 0x30, F32(veh, 0xc0));
                const float y = F32(veh, 0xc4);
                SF32(F, 0x34, y);
                SF32(F, 0x24, F32(veh, 0x134) * k6 + F32(veh, 0x144) * k0);
                SF32(F, 0x38, F32(veh, 0xc8));
                SF32(F, 0x34, y + 7.0f /* 0x930004 */);
                SF32(F, 0x28, F32(veh, 0x138) * k6 + F32(veh, 0x148) * k0);
                { const uint32_t a[2] = {F + 0x30, 0x00d61640}; (void)act_vcall(U32(act, 0xff090), 0x70, 2, a); }
                ACT_CALL(2, rx_tag_ball_physics(act, 1));
                ACT_CALL(2, rx_tag_ball_impulse(act, F + 0x20));
                SU32(act, 0xff078, 0);
                const uint32_t a[1] = {r};
                (void)act_vcall(U32(r, 4), 0x24, 1, a);
            }
        }
    }
    if (U32(0, 0x00ae1cbc) != 0) {
        const rx_addr r = rule;   /* ESI: the rule (the grab path overwrote its frame copy F + 0x14) */
        SF32(F, 0x20, 0.0f); SF32(F, 0x24, 0.0f); SF32(F, 0x28, 0.0f);
        const uint32_t a[1] = {F + 0x20};
        (void)act_vcall(U32(U32(U32(r, 4), 0x74), 0x1ac), 0x44, 1, a);
    }
    race_profile_end(U32(F, 0x18));
    ACT_END(F);
}
RX_FUNCTION(0x0051bd70, rx_tag_rule_vf5)
