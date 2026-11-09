/* build_activity_freeride_state.c -- MXActivityFreeRide's Pre enter (vf37 005603c0) and the pieces it uses: vf54 005602f0 ("discovery mode"), the jump
 * tracker's collectables (00585790), the world's AAA_ / DISC_ pickups hidden (00561820) and the collision entity's body flags (00819be0).
 * Career mode (vf54 = 1: the UI property InCareerMode, which the product's host sets with rx_session_event_in_career_mode for the Race & Ride challenges
 * Free Ride): the discovery collectibles of the profile (00585520 / 0054d640, race_activation.c: the profile's PlayerCollectables table). */
#include "build_activity_int.h"
#include "build_activity_pre.h"
#include "build_activity_freeride_run.h"
#include "build_rider_util.h"
#include "race_activity.h"
#include "build_ui_props.h"
#include "race_activation.h"
#include "rider_cap.h"

static void session_static(void) {
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
}

/* 005602f0 (ECX = the activity) -> 1 in "discovery" mode: the UI property "InCareerMode" set (UI: the oracle reads the property, the product has none: 0), the
 * world the front end's (+0x20018 == 0 is the event world: the property decides there), and exactly one human among the game's local players (offline) */
uint32_t rx_free_ride_vf54(rx_addr A) {
    ACT_FRAME(L, 4, 2);
    session_static();
    const rx_addr s = U32(0, 0x00da6cb0), g = U32(0, 0x00a946b8);
    uint32_t flag = 0;
    if (U32(s, 0x1c4c) == 0 || U32(s, 0x1b0) == 1) {
        uint32_t n = U8(g, 0x1541c), humans = 0;
        for (uint32_t i = 0; i < n; i++) if (U32(rx_seat_active(g + 0x7460, i), 0x10) == 0) humans++;
        if (n != 0 && humans == 1) flag = 1;
    }
    SU32(L, 8, 0);
    {   /* UIProperty_GetInt("InCareerMode") (EAX = the name; stack the UI manager, &out): the oracle's UI; the product's property is absent (build_ui_props.c) */
#ifdef RX_ORACLE
        rx_regs in, out; memset(&in, 0, sizeof in); memset(&out, 0, sizeof out);
        in.r[0] = 0x00941f6c;
        const uint32_t a[2] = {U32(0, 0x00a93fcc), L + 8};
        act_ui_call_ex(0x005dc0c0, &in, &out, 2, a);
#else
        (void)rx_ui_prop_get_int(0x00941f6c, L + 8);
#endif
    }
    const uint32_t career = U32(L, 8);
    const uint32_t r = (flag && career != 0 && U32(U32(A, 0x234), 0x20018) == 0) ? 1 : 0;
    ACT_END(L);
    return r;
}
RX_FUNCTION(0x005602f0, rx_free_ride_vf54)

/* 00585790 (ESI = the jump tracker, stack activity; RET 4): the tracker cleared (005857f0), the "jump collectables" (type 0x106d) of the activity's area group in the
 * current world's scene queried into it (+4.., count +0), each one's +0x70 = 0, the tracker cleared again */
void rx_free_ride_collectables_hide(rx_addr t, rx_addr A) {
    ACT_FRAME(L, 0, 1);
    rx_addr w;
    ACT_CALLR(w, 0, rx_game_current_world());
    const rx_addr scene = U32(w, 0x20138);
    rx_free_ride_jump_tracker_init(t);
    uint32_t n;
    ACT_CALLR(n, 6, rx_scene_group_query(scene, 0x106d, t + 4, 0x20, A + 0x118, 0));
    SU32(t, 0, n);
    for (int32_t i = 0; i < (int32_t)n; i++) SU32(U32(t, 4 + 4 * (uint32_t)i), 0x70, 0);
    rx_free_ride_jump_tracker_init(t);
    ACT_END(L);
}
RX_FUNCTION(0x00585790, rx_free_ride_collectables_hide)

/* 00819be0 (stdcall entity; RET 4): the collision flag byte (+0x38) of the entity's body record (the handle at +0x160: {+4 the world, +8 the index}; 0x40 bytes
 * from world +0x19c0) and of each body of its list (+0x170 / +0x174: 8-byte items, the handle at +4) cleared */
static void body_flag_clear(rx_addr h) {
    const rx_addr c = U32(h, 4);
    const uint32_t idx = U32(h, 8);
    if ((uint32_t)((int32_t)(U32(c, 0x19c4) - U32(c, 0x19c0)) >> 6) <= idx) crt_invalid_parameter_noinfo();
    SU8(U32(c, 0x19c0) + idx * 0x40 + 0x38, 0, 0);
}
void rx_entity_bodies_flag_clear(rx_addr e) {
    if (U32(e, 0x160) != 0) body_flag_clear(U32(e, 0x160));
    for (uint32_t i = 0; i < (uint32_t)((int32_t)(U32(e, 0x174) - U32(e, 0x170)) >> 3); i++) {
        if ((uint32_t)((int32_t)(U32(e, 0x174) - U32(e, 0x170)) >> 3) <= i) crt_invalid_parameter_noinfo();
        const rx_addr h = U32(U32(e, 0x170) + 8 * i, 4);
        if (h != 0) body_flag_clear(h);
    }
}
RX_FUNCTION(0x00819be0, rx_entity_bodies_flag_clear)

/* 00561820 (stdcall activity; RET 4): the scene's entities named "AAA_..." (up to 0x32): a type 5 one's bodies' flags cleared (00819be0), all hidden (+0xa2 = 0, +0x70 = 0);
 * the "DISC_..." ones (up to 0x14) hidden (+0xa2 = 0, +0x70 = 0) */
void rx_free_ride_pickups_hide(rx_addr A) {
    ACT_FRAME(L, 0x120, 3);
    const rx_addr list1 = L + 0x64, list2 = L + 0x14, flag = L + 0xc;
    SU32(list1, 0, 0);
    crt_memset(L + 0x68, 0, 0xc4);
    const rx_addr scene = U32(U32(A, 0x234), 0x20138);
    SU32(flag, 0, 0);
    uint32_t n;
    ACT_CALLR(n, 4, rx_scene_collect_prefixed(scene, 0x00941fa4 /* "AAA_" */, list1, 0x32, flag));
    for (uint32_t i = 0; n != 0 && i < n; i++) {
        const rx_addr e = U32(list1, 4 * i);
        if (act_vcall(e, 0x58, 0, 0) == 5) ACT_CALL(1, rx_entity_bodies_flag_clear(e));
        SU8(e, 0xa2, 0); SU32(e, 0x70, 0);
    }
    SU32(list2, 0, 0);
    crt_memset(L + 0x18, 0, 0x4c);
    ACT_CALLR(n, 4, rx_scene_collect_prefixed(scene, 0x00941fac /* "DISC_" */, list2, 0x14, flag));
    for (uint32_t i = 0; n != 0 && i < n; i++) { const rx_addr e = U32(list2, 4 * i); SU8(e, 0xa2, 0); SU32(e, 0x70, 0); }
    ACT_END(L);
}
RX_FUNCTION(0x00561820, rx_free_ride_pickups_hide)

/* MXActivityFreeRide::vf37 005603c0 (ECX = the activity; stack the old state; RET 4): Activity::vf37; in the event world (+0x20018 == 0): with no session result
 * and the UI's intro flags set ([0x00d60ff0], the game's +0x15b4c, none of [0x00ae1b90]) the intro counter (+0x1143d0) = 2, else the intro done (+0xff1f0 = 1,
 * +0x1143b4 = 0); then the pickups hidden (vf54 = 0: 00585790 / 00561820, +0xff180 = 0; discovery mode: 00585520) and +0xff1f0 = 1; the activity area's
 * "pickup" entities (0x1059, up to 0xff): +0x794 = 1, the object at +0x790's vf4 result's +0x1c = 0, hidden (+0xa2 = 0) */
void rx_free_ride_vf37(rx_addr A, uint32_t old) {
    ACT_FRAME(L, 0x3fc, 4);
    ACT_CALL(1, rx_activity_vf37(A));                                   /* Activity::vf37 (the old state is its argument) */
    (void)old;
    const rx_addr w = U32(A, 0x234);
    if (U32(w, 0x20018) == 0) {
        if (U32(A, 0xfe8) == 0 && U32(0, 0x00d60ff0) != 0 && U32(U32(0, 0x00a946b8), 0x15b4c) != 0 && U32(0, 0x00ae1b90) == 0)
            SU32(A, 0x1143d0, 2);
        else { SU32(A, 0xff1f0, 1); SU32(A, 0x1143b4, 0); }
        if (U32(w, 0x20018) == 0) {
            const uint32_t disc = act_vcall(A, 0xd8, 0, 0);             /* vf54 */
            if (disc != 0) {
                /* discovery mode (the career's Free Ride): the profile's found collectables (00585520, race_activation.c); the "AAA_" area
                 * markers and the "DISC_" ones stay shown (00561820 is not called) */
                ACT_CALL(2, rx_free_ride_discoveries_setup(A + 0xff080, A));
                SU32(A, 0xff180, 1);
            } else {
                ACT_CALL(1, rx_free_ride_collectables_hide(A + 0xff080, A));
                SU32(A, 0xff180, 0);
                ACT_CALL(1, rx_free_ride_pickups_hide(A));
            }
            SU32(A, 0xff1f0, 1);
        }
    }
    const rx_addr list = L + 0x10;
    uint32_t n;
    ACT_CALLR(n, 6, rx_scene_group_query(U32(U32(A, 0x234), 0x20138), 0x1059, list, 0xff, A + 0x118, 0));
    for (uint32_t i = 0; i < n; i++) {
        const rx_addr e = U32(list, 4 * i);
        const rx_addr o = U32(e, 0x790);
        SU32(e, 0x794, 1);
        if (o != 0) { const rx_addr r = act_vcall(o, 0x10, 0, 0); if (r != 0) SU32(r, 0x1c, 0); }
        SU8(e, 0xa2, 0);
    }
    ACT_END(L);
}
RX_FUNCTION(0x005603c0, rx_free_ride_vf37)

/* 0055ebc0 MXActivityFreeRide::vf48 (ECX = activity): intro completion flag. */
uint32_t rx_free_ride_intro_done(rx_addr activity) { return U32(activity,0xff1f0); }
RX_FUNCTION(0x0055ebc0, rx_free_ride_intro_done)
