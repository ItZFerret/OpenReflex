/* race_graffiti.c -- MXActivityGraffiti (the Graffiti party mode, activity type 0xb; the area subscene "<level>MiniGameGraffiti";
 * vtable 0x0094497c; 0xff090 bytes), its GraffitiDriver (vtable 0x0093fc24, 0xf30 bytes) and GraffitiActivityAudioLogic (vtable
 * 0x00942c5c, 0x9c bytes), and the activity's side of the GraffitiCapturePoint scene entities (type 0x1040), hand-transliterated from
 * the original (the address of each function is in its comment; frames and the stack depth of each call are the original's).
 *
 * The game: the area's GraffitiCapturePoint entities (collected at the start: +0xff070 an array, +0xff074 the count) are captured by
 * stunts in their linked triggers (the point's own handlers: race_graffiti_point.c); a driver's +0xf14 counts the
 * points it holds and +0xf1c their score. The round ends by the clock (vf42); the message names the standings' leader (+0x2d0). The
 * starts: 16 places spread over the start entities of type 0x1011 (StartEntityFreestyle) or, without one, 0x1014 (StartEntityFreeRide)
 * of the loaded subscenes, i / count * 10 along the start's x axis.
 *
 * Content: no installed level holds a MiniGameGraffiti subscene nor a 0x1011 / 0x1014 start entity in the subscenes a Graffiti event
 * loads (the original divides by zero at 00564151 on every level), so the session refuses a Graffiti event before it loads
 * (api/rx_session.c minigame_content_missing); vf2 stops (act_unreached, STOP_AUDIT R125) where the original would fault.
 *
 * GraffitiCapturePoint (vtable 0x00938b9c; a CollisionEntity / ModelEntity built by the scene loader from its RSData, with a 3D text
 * label and trigger / stunt listeners) is race_graffiti_point.c; the activity's calls of it are here (its label 00408220 and linked
 * trigger 00408400, the driver lookup 00564970). The label is an AnimatedTextIcon3dBillboard (+0xbd0), a render object the product never
 * builds (as the players' name plates): its text object's calls and fields are the oracle's only, the product leaves them out.
 *
 * ONLINE (a Quazal session: [0x00da6cb0] + 0x1c4c set and the mode +0x1b0 not 1; none exists in the product): the network graffiti object
 * (+0xff084: NetGraffiti, "NetworkObjectRemoved" 00564ab0 / "GraffitiPointCaptured" 00564ad0 handlers: not ported, as the other party
 * modes' network handlers), the host's round end. Each networked branch is marked "ONLINE" and stops with act_unreached
 * (STOP_AUDIT R125). */
#include "race_minigames.h"
#include "race_activity.h"
#include "race_activity_misc.h"
#include "race_activity_ui.h"
#include "race_challenge_activity.h"
#include "race_champ.h"
#include "race_finish.h"
#include "race_driver.h"
#include "race_misc.h"
#include "race_extern.h"
#include "race.h"
#include "build_activity.h"
#include "build_activity_int.h"
#include "build_activity_teardown.h"
#include "build_activity_freeride_run.h"
#include "build_driver_reset.h"
#include "build_driver_dtor.h"
#include "build_vehicle.h"
#include "build_alloc.h"
#include "waypoint_activity.h"
#include "waypoint_audio.h"
#include "world_wreck.h"
#include "phys_terrain.h"
#include "anim.h"
#include "fx.h"
#include "crt.h"
#include "../src/rx_os.h"
#include <string.h>

#define VT_GRAFFITI         0x0094497cu
#define VT_GRAFFITI_DRIVER  0x0093fc24u
#define VT_GRAFFITI_AUDIO   0x00942c5cu
#define VT_AUDIO_LOGIC      0x009429fcu
#define TD_ACTIVITY         0x009cbd24u   /* void(Activity*) */
#define STR_ACTIVITY_INIT   0x00935ea4u

void rx_waypoint_network_vf14(rx_addr driver);   /* 0050ec30 NetworkDriver::vf14 (waypoint_driver_network.c) */

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
/* a capture point not held: no holder, its colour white (+0xb70 .. +0xb7c) */
static void point_release(rx_addr p) {
    SU32(p, 0xb5c, 0); SU32(p, 0xb60, 0); SU32(p, 0xb64, 0);
    SF32(p, 0xb70, 1.0f); SF32(p, 0xb74, 1.0f); SF32(p, 0xb78, 1.0f); SF32(p, 0xb7c, 1.0f);
}

/* ======================================================================================================================== */
/* construction / destruction                                                                                                */

/* 00582450 (ESI = logic) -> logic: ActivityAudioLogic's fields and vf32, the class's vtable, vf32 again. Frame: push ecx. */
rx_addr rx_graffiti_audio_construct(rx_addr logic) {
    ACT_FRAME(F, 0, 1);
    SU32(logic, 0, VT_AUDIO_LOGIC);
    SU32(logic, 0x94, 0);
    SU8(logic, 4, 0);
    SU32(logic, 0x50, 0);
    SU32(logic, 0x54, 0);
    SU32(logic, 0x8c, 0);
    SU32(logic, 0x90, 0);
    ACT_CALL(0, rx_audio_logic_vf32(logic));
    SU32(logic, 0, VT_GRAFFITI_AUDIO);
    ACT_CALL(0, rx_audio_logic_vf32(logic));
    ACT_END(F);
    return logic;
}
RX_FUNCTION(0x00582450, rx_graffiti_audio_construct)

/* 00563d90 (stack act; ECX = type, EAX = world; RET 4) -> act: Activity's constructor, the class's fields (no capture points),
 * +0x2c8 = 0, a new GraffitiActivityAudioLogic (0x9c bytes: 00582450) at +0xfd8. Frame: push ecx / ebx / esi / edi. */
rx_addr rx_graffiti_construct(rx_addr act, uint32_t type, rx_addr world) {
    ACT_FRAME(F, 0, 4);
    ACT_CALL(1, rx_activity_construct(act, type, world));
    SU32(act, 0, VT_GRAFFITI);
    for (uint32_t k = 0xff070; k <= 0xff080; k += 4) SU32(act, k, 0);
    SU32(act, 0x2c8, 0);
    SU32(act, 0xff084, 0);
    rx_addr logic;
    ACT_CALLR(logic, 1, rx_reflex_aligned_alloc(0x9c));
    if (logic != 0) ACT_CALLR(logic, 0, rx_graffiti_audio_construct(logic));
    SU32(act, 0xfd8, logic);
    ACT_END(F);
    return act;
}
RX_FUNCTION(0x00563d90, rx_graffiti_construct)

/* 00563e10 slot 0 (thiscall; flags; RET 4) -> act: the audio logic released (00580d30) and freed, the capture point array freed,
 * Activity's destructor (005566b0), freed with flags & 1. Frame: push esi / edi. */
rx_addr rx_graffiti_sdtor(rx_addr act, uint32_t flags) {
    ACT_FRAME(F, 0, 2);
    const rx_addr logic = U32(act, 0xfd8);
    SU32(act, 0, VT_GRAFFITI);
    if (logic != 0) {
        ACT_CALL(1, rx_activity_audio_destroy(logic));
        ACT_CALL(1, rx_reflex_aligned_free(logic));
    }
    ACT_CALL(1, rx_reflex_aligned_free(U32(act, 0xff070)));
    ACT_CALL(0, rx_waypoint_activity_base_destroy(act));
    if (flags & 1) ACT_CALL(1, rx_reflex_aligned_free(act));
    ACT_END(F);
    return act;
}
RX_FUNCTION(0x00563e10, rx_graffiti_sdtor)

/* ======================================================================================================================== */
/* the listeners (as race_infection.c's templates)                                                                          */

/* 00587ef0 (EAX = the session channel; stdcall act; RET 4): "NetworkObjectRemoved" with 00564ab0 (proxy 0x945984) */
void rx_graffiti_net_removed_listen(rx_addr channel, rx_addr act) {
    ACT_CALL(1, act_subscribe(0x00587ef0, 1, 1, 4, channel, 0x0093ee98 /* "NetworkObjectRemoved" */, 0x00945984, 0x009cdc7c, act, 0x00564ab0));
}
RX_FUNCTION(0x00587ef0, rx_graffiti_net_removed_listen)
/* 00587fb0: "NetworkGraffitiActivity" with 00564ad0 (proxy 0x9459a4: void(GraffitiCapturePoint*, RavenVehicle*, unsigned)) */
void rx_graffiti_net_captured_listen(rx_addr channel, rx_addr act) {
    ACT_CALL(1, act_subscribe(0x00587fb0, 1, 1, 4, channel, 0x00941ff4 /* "NetworkGraffitiActivity" */, 0x009459a4, 0x009cfcb0, act, 0x00564ad0));
}
RX_FUNCTION(0x00587fb0, rx_graffiti_net_captured_listen)
/* 00588070 / 005880d0: the two listeners leave (frame: push ebp; mov ebp, esp; and esp, -8; sub esp, 0x14; push esi) */
void rx_graffiti_net_removed_unlisten(rx_addr channel, rx_addr act) {
    act_unlisten(1, channel, 0x0093ee98, 0x00945984, 0x009cdc7c, act, 0x00564ab0);
}
RX_FUNCTION(0x00588070, rx_graffiti_net_removed_unlisten)
void rx_graffiti_net_captured_unlisten(rx_addr channel, rx_addr act) {
    act_unlisten(1, channel, 0x00941ff4, 0x009459a4, 0x009cfcb0, act, 0x00564ad0);
}
RX_FUNCTION(0x005880d0, rx_graffiti_net_captured_unlisten)

/* ======================================================================================================================== */
/* the capture points (the activity's side)                                                                                  */

/* 00408400 (stdcall point; RET 4) -> the area's trigger (type 0x1028, 007d2820 in the point's activity's +0x118) whose name (+0xc0)
 * equals the point's LinkedTriggerID (+0xb00), or 0. Frame: sub esp, 0x3fc; push ebp / esi / edi (+ ebx in the loop; the list F + 0x10). */
rx_addr rx_graffiti_point_trigger(rx_addr p) {
    ACT_FRAME(F, 0x3fc, 3);
    const rx_addr act = U32(p, 0xb80);
    const rx_addr scene = U32(U32(act, 0x234), 0x20138);
    for (uint32_t k = 0; k < 0x3f8; k += 4) SU32(F + 0x14, k, 0);
    SU32(F, 0x10, 0);
    uint32_t n;
    ACT_CALLR(n, 6, rx_scene_group_query(scene, 0x1028, F + 0x10, 0xff, act + 0x118, 0));
    rx_addr found = 0;
    for (uint32_t i = 0; i < n && found == 0; i++) {
        const rx_addr t = U32(F + 0x10, 4 * i);
        if (crt_strcmp(t + 0xc0, p + 0xb00) == 0) found = t;
    }
    ACT_END(F);
    return found;
}
RX_FUNCTION(0x00408400, rx_graffiti_point_trigger)

/* 00408220 (EDI = point): a counted point (+0xb44) gets its 3D text label: the text object (+0xbe0: 007ff4b0 with 10, 1), the label's
 * colour (the static 0xdcd450 {0, 10, 0}, guard 0xdcd460) in +0xc70, the empty text (004027e0), the transform +0xb90 in +0xc74 / +0xc78,
 * the font "MorganSansCondLining28B" (007fb950 / 007fb8e0 / 007ff120), its scale 6 (+0x118) and flags of the text (+0xc48); the transform
 * identity with the linked trigger's position (00408400; vf17 slot 0x44) as its row 3; +0xc9c = 0, the label's distances +0xc80 ..
 * +0xc8c = 10, 25, 300, 325. The text renderer's calls are render-side (the product never draws: the oracle runs the originals).
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x18; push ebx / esi (the position F + 0x10). */
void rx_graffiti_point_label(rx_addr p) {
    ACT_FRAME_EBP(F, 16, 0x18, 2);
    if (U32(p, 0xb44) != 0) {
#ifdef RX_ORACLE
        { rx_regs in, out; memset(&in, 0, sizeof in); in.r[1] = p + 0xbe0; in.r[7] = p; const uint32_t a[2] = {0xa, 1}; rx_call_original_ex(0x007ff4b0, &in, &out, 2, a); }
#endif
        if (!(U8(0, 0x00dcd460) & 1)) {
            SU32(0, 0x00dcd460, U32(0, 0x00dcd460) | 1);
            SF32(0, 0x00dcd450, 0.0f); SF32(0, 0x00dcd454, 10.0f /* 0x92fd88 */); SF32(0, 0x00dcd458, 0.0f);
        }
        SU32(p, 0xc70, 0x00dcd450);
#ifdef RX_ORACLE
        ACT_CALL(1, rx_fx_text_set(U32(p, 0xc48), 0, 0x00935d6c /* "" */));
#endif
        SU32(p, 0xc74, p + 0xb90);
        SU32(p, 0xc78, p + 0xb90);
#ifdef RX_ORACLE
        {   rx_regs in, out; memset(&in, 0, sizeof in); in.r[6] = p + 0xb90; in.r[7] = p;
            rx_call_original_ex(0x007fb950, &in, &out, 0, 0);
            const rx_addr fonts = out.r[0];
            in.r[1] = fonts; const uint32_t nm[1] = {0x00935efc /* "MorganSansCondLining28B" */};
            rx_call_original_ex(0x007fb8e0, &in, &out, 1, nm);
            const uint32_t font[1] = {out.r[0] != 0 ? U32(out.r[0], 0) : 0};
            in.r[1] = U32(p, 0xc48);
            rx_call_original_ex(0x007ff120, &in, &out, 1, font); }
#endif
#ifdef RX_ORACLE   /* the label's text object (+0xc48, made by the billboard's 007ff4b0): render only, the product builds no billboard */
        const rx_addr text = U32(p, 0xc48);
        if (!(F32(text, 0x118) == 6.0f /* 0x92fe4c */)) SF32(text, 0x118, 6.0f);
        SU8(U32(p, 0xc48), 0xfa, (uint8_t)(U8(U32(p, 0xc48), 0xfa) | 2));
        SU32(U32(p, 0xc48), 0xdc, 1);
        SU32(U32(p, 0xc48), 0xe0, 1);
#endif
        rx_addr trig;
        ACT_CALLR(trig, 1, rx_graffiti_point_trigger(p));
        for (uint32_t i = 0; i < 16; i++) SF32(p + 0xb90, 4 * i, (i % 5) == 0 ? 1.0f : 0.0f);
        if (trig != 0) {
            { const uint32_t a[1] = {F + 0x10}; (void)act_vcall(trig, 0x44, 1, a); }
            SU32(p, 0xbc0, U32(F, 0x10)); SU32(p, 0xbc4, U32(F, 0x14)); SU32(p, 0xbc8, U32(F, 0x18));
        }
        SU32(p, 0xc9c, 0);
        SF32(p, 0xc80, 10.0f /* 0x92fd88 */);
        SF32(p, 0xc84, 25.0f /* 0x92e134 */);
        SF32(p, 0xc88, 300.0f /* 0x935d28 */);
        SF32(p, 0xc8c, 325.0f /* 0x935d2c */);
    }
    ACT_END(F);
}
RX_FUNCTION(0x00408220, rx_graffiti_point_label)

/* 00564970 (EDI = act; stdcall vehicle; RET 4) -> the driver whose player's vehicle (+0x1ac) is the vehicle (the last driver when none
 * is, 0 without drivers). Frame: push ebx / esi (+ ebp with drivers). */
rx_addr rx_graffiti_driver_of(rx_addr act, rx_addr vehicle) {
    rx_addr d = 0;
    int found = 0;
    for (int32_t i = 0; i < I32(act, 0x26c) && !found; i++) {
        d = U32(U32(act, 0x268), 4u * (uint32_t)i);
        if (U32(U32(d, 0x74), 0x1ac) == vehicle) found = 1;
    }
    return d;
}
RX_FUNCTION(0x00564970, rx_graffiti_driver_of)

/* ======================================================================================================================== */
/* the activity's virtuals                                                                                                   */

/* 00563e60 slot 2 (thiscall; arg; RET 4): Activity::vf2; the two network listeners; the host's network graffiti object (ONLINE); the
 * area's capture points (type 0x1040, up to 0xff: 007d2820) into the array +0xff070 (a vector: count +0xff074, capacity +0xff078, growth
 * +0xff07c = 1, +0xff080 the found count), each linked to the activity (+0xb80) and labelled (00408220); the start entities of type 0x1011
 * (or 0x1014 when none / too many: 007d2920, up to 0x10) and the 16 starts (+0x340 + 0x30 * i) spread over them: the start's position +
 * its x axis (row 0) * (i / count) * 10 on the terrain (007c5950), its direction (row 2); +0x880 = 0x10; "Activity Init" raised.
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x484; push ebx / esi / edi (the points F + 0x90, the starts F + 0x50, the
 * start's position F + 0x20). */
void rx_graffiti_vf2(rx_addr act, rx_addr arg) {
    ACT_FRAME_EBP(F, 16, 0x484, 3);
    ACT_CALL(1, rx_activity_vf2(act, arg));
    const rx_addr scene = U32(U32(act, 0x234), 0x20138);
    SU32(F, 0x1c, scene);
    session_static();
    ACT_CALL(1, rx_graffiti_net_removed_listen(U32(0, 0x00da6ca0), act));
    session_static();
    ACT_CALL(1, rx_graffiti_net_captured_listen(U32(0, 0x00da6ca0), act));
    session_static();
    if (session_networked()) {
        session_static();
        if (U32(U32(0, 0x00da6cb0), 0x1c48) != 0) act_unreached(0x00563f55, "ONLINE: the host's network graffiti object (008879b0 / 00887780)");
    }
    for (uint32_t k = 0; k < 0x3f8; k += 4) SU32(F + 0x94, k, 0);
    SU32(F, 0x90, 0);
    uint32_t n;
    ACT_CALLR(n, 6, rx_scene_group_query(scene, 0x1040, F + 0x90, 0xff, act + 0x118, 0));
    rx_addr arr;
    ACT_CALLR(arr, 1, rx_reflex_aligned_alloc(rx_mul_sat(n, 4)));
    SU32(act, 0xff070, arr);
    SU32(act, 0xff074, 0);
    SU32(act, 0xff078, n);
    SU32(act, 0xff07c, 1);
    SU32(act, 0xff080, n);
    SU32(F, 0x10, 0);
    for (uint32_t i = 0; i < n; i++) {
        if (I32(act, 0xff074) >= I32(act, 0xff078)) {   /* the vector grows by +0xff07c */
            const uint32_t cap = U32(act, 0xff07c) + U32(act, 0xff078);
            rx_addr grown;
            ACT_CALLR(grown, 1, rx_reflex_aligned_alloc(rx_mul_sat(cap, 4)));
            SU32(F, 0x18, grown);
            ACT_CALL(3, (void)crt_memcpy(grown, U32(act, 0xff070), 4 * U32(act, 0xff078)));
            ACT_CALL(1, rx_reflex_aligned_free(U32(act, 0xff070)));
            SU32(act, 0xff070, U32(F, 0x18));
            SU32(act, 0xff078, cap);
        }
        const rx_addr p = U32(F + 0x90, 4 * i);
        SU32(U32(act, 0xff070), 4 * U32(act, 0xff074), p);
        SU32(act, 0xff074, U32(act, 0xff074) + 1);
        SU32(p, 0xb80, act);
        ACT_CALL(0, rx_graffiti_point_label(p));
        SU32(F, 0x10, i + 1);
    }
    for (uint32_t k = 0; k < 0x3c; k += 4) SU32(F + 0x54, k, 0);
    SU32(F, 0x50, 0);
    SU32(F, 0x10, 0);
    uint32_t count;
    ACT_CALLR(count, 4, rx_scene_manager_collect_typed(U32(F, 0x1c), 0x1011, F + 0x50, 0x10, F + 0x10));
    SU32(F, 0x18, count);
    if (U32(F, 0x10) != 0 || count == 0) {
        ACT_CALLR(count, 4, rx_scene_manager_collect_typed(U32(F, 0x1c), 0x1014, F + 0x50, 0x10, F + 0x10));
        SU32(F, 0x18, count);
    }
    if (count == 0)   /* the original divides by zero (00564151); the session refuses such a level first */
        act_unreached(0x00564151, "Graffiti: no StartEntityFreestyle / StartEntityFreeRide entity in the loaded subscenes (the level lacks the mode's content)");
    for (uint32_t i = 0; i < 0x10; i++) {
        SU32(F, 0x10, i);
        const uint32_t q = i / count;
        const rx_addr st = U32(F + 0x50, 4 * (i % count));
        SU32(F, 0x14, q);
        rx_addr m = act_vcall(st, 0x38, 0, 0);
        SF32(F, 0x40, F32(m, 0x30)); SF32(F, 0x44, F32(m, 0x34)); SF32(F, 0x48, F32(m, 0x38));
        m = act_vcall(st, 0x38, 0, 0);
        SF32(F, 0x30, F32(m, 0x20)); SF32(F, 0x34, F32(m, 0x24)); SF32(F, 0x38, F32(m, 0x28));
        m = act_vcall(st, 0x38, 0, 0);
        const float qf = (float)q;   /* fild (+ 2^32 when negative) + fstp: exact */
        SF32(F, 0x14, qf);
        SF32(F, 0x20, rx_adds(rx_muls(rx_muls(F32(m, 0), qf), 10.0f /* 0x92fd88 */), F32(F, 0x40)));
        SF32(F, 0x24, rx_adds(rx_muls(rx_muls(F32(m, 4), qf), 10.0f), F32(F, 0x44)));
        SF32(F, 0x28, rx_adds(rx_muls(rx_muls(F32(m, 8), qf), 10.0f), F32(F, 0x48)));
        uint32_t edx;
        ACT_CALL(1, (void)phys_terrain_base_height_at(scene_terrain(U32(F, 0x1c)), F + 0x20, &edx));
        const rx_addr out = act + 0x340 + 0x30 * i;
        SF32(out, 0, F32(F, 0x20)); SF32(out, 4, F32(F, 0x24)); SF32(out, 8, F32(F, 0x28));
        SF32(out, 0x10, F32(F, 0x30)); SF32(out, 0x14, F32(F, 0x34)); SF32(out, 0x18, F32(F, 0x38));
        SU32(F, 0x10, i + 1);
    }
    SU32(act, 0x880, 0x10);
    SU32(F, 0x14, act);
    act_raise(U32(0, 0x00a93f74), STR_ACTIVITY_INIT, TD_ACTIVITY, F + 0x14, 4);
    ACT_END(F);
}
RX_FUNCTION(0x00563e60, rx_graffiti_vf2)

/* 00564320 slot 3 (ECX = activity): (ONLINE: the session's NetGraffiti objects unlinked, 00585bb0); the two listeners leave; the network
 * object 0; every driver of the two lists leaves the activity; then a jump to Activity::vf3. Frame: sub esp, 0x24; push ebx / edi. */
void rx_graffiti_vf3(rx_addr act) {
    ACT_FRAME(F, 0x24, 2);
    session_static();
    if (session_networked()) act_unreached(0x00564362, "ONLINE: the session's NetGraffiti objects unlinked (00585bb0)");
    session_static();
    ACT_CALL(1, rx_graffiti_net_removed_unlisten(U32(0, 0x00da6ca0), act));
    session_static();
    ACT_CALL(1, rx_graffiti_net_captured_unlisten(U32(0, 0x00da6ca0), act));
    SU32(act, 0xff084, 0);
    while (I32(act, 0x26c) > 0) ACT_CALL(2, rx_activity_on_player_left(act, U32(U32(act, 0x268), 0)));
    while (I32(act, 0x280) > 0) ACT_CALL(2, rx_activity_on_player_left(act, U32(U32(act, 0x27c), 0)));
    ACT_END(F);
    rx_activity_vf3(act);
}
RX_FUNCTION(0x00564320, rx_graffiti_vf3)

/* 00564960 slot 15 (ECX = activity): a jump to Activity::vf15 */
void rx_graffiti_vf15(rx_addr act) { rx_activity_vf15(act); }
RX_FUNCTION(0x00564960, rx_graffiti_vf15)

/* 00564450 slot 18 (ECX = activity): the round's reset: every capture point released, then a jump to Activity::vf18. Frame: push esi. */
void rx_graffiti_vf18(rx_addr act) {
    ACT_FRAME(F, 0, 1);
    for (int32_t i = 0; i < I32(act, 0xff074); i++) point_release(U32(U32(act, 0xff070), 4u * (uint32_t)i));
    ACT_END(F);
    rx_activity_vf18(act);
}
RX_FUNCTION(0x00564450, rx_graffiti_vf18)

/* 005646c0 slot 23 (thiscall; dt; RET 4): Activity::vf23, then under the engine's profiler scope (ONLINE: the network graffiti object
 * found or made). Frame: push ebp; mov ebp, esp; and esp, -8; sub esp, 0x2c; push ebx / esi / edi; dt through x87. */
void rx_graffiti_vf23(rx_addr act, float dt) {
    ACT_FRAME_EBP(F, 8, 0x2c, 3);
    ACT_CALL(1, rx_activity_update(act, x87_copy(dt)));
    session_static();
    const rx_addr scope = race_profile_begin();
    if (U32(act, 0xff084) == 0) {
        session_static();
        if (session_networked()) act_unreached(0x005647a1, "ONLINE: the network graffiti object found or made (006e4fa0 / 008879b0)");
    }
    race_profile_end(scope);
    ACT_END(F);
}
RX_FUNCTION(0x005646c0, rx_graffiti_vf23)

/* 00587d00 "Player Wins" (ESI = manager, ECX = driver, EDX = text; args: activity, value; RET 8), at the original's frame (sub esp,
 * 0x18; push edi: the payload F + 4), as race_tag.c's (the dispatch's handlers keep the payload's stack address) */
static void graffiti_player_wins(rx_addr manager, rx_addr activity, rx_addr driver, rx_addr text, uint32_t value) {
    ACT_FRAME(F, 0x18, 1);
    SU32(F, 4, activity); SU32(F, 8, driver); SU32(F, 0xc, text); SU32(F, 0x10, value);
    act_raise(manager, 0x00941ca8 /* "Player Wins" */, 0x009cf630, F + 4, 0x10);
    ACT_END(F);
}

/* 00564560 slot 40 (thiscall; old state; RET 4): WindDown begins. Activity::vf40; the message "%s %s" (the standings' leader +0x2d0 and
 * the localized STR_WINS; ONLINE: "%s" of 0x941fb4): UI text, the oracle runs the original localization chain, the product leaves an
 * empty string (as race_infection.c's vf40); "Player Wins" (00587d00) with the leader, the text, 1. Frame: sub esp, 0x208; push ebp /
 * esi / edi (F); the UTF-8 name F + 0xc, the text F + 0x10c. */
void rx_graffiti_vf40(rx_addr act, uint32_t old_state) {
    ACT_FRAME(F, 0x208, 3);
    (void)old_state;
    ACT_CALL(1, rx_activity_wind_down_enter(act));
    session_static();
    if (session_networked()) act_unreached(0x005645bf, "ONLINE: the session's round message");
#ifdef RX_ORACLE
    {
        uint32_t zero = 0;
        const uint32_t lang = rx_call_original(0x005dfc90, RX_CDECL, 0, 1, &zero);
        rx_addr entry;
        ACT_CALLR(entry, 0, rx_ui_language_lookup(0x00941fc4 /* "STR_WINS" */, lang));
        { uint32_t a[8] = {0xfde9, 0, U32(entry, 0x10), 0xffffffffu, F + 0xc, 0x100, 0, 0}; rx_os_import(0x009160a8, 8, a); }
        SU8(F, 0x10b, 0);
        SU32(F, -12, 0x00941fd0); SU32(F, -8, U32(U32(act, 0x2d0), 0x74) + 0x54); SU32(F, -4, F + 0xc);
        rx_set_sp(F - 16);
        rx_format_buffer(F + 0x10c, 0x104, 0x00941fd0 /* "%s %s" */, F - 8);
        rx_set_sp(F);
    }
#else
    SU8(F, 0x10c, 0);
#endif
    ACT_CALL(2, graffiti_player_wins(U32(act, 0x290), act, U32(act, 0x2d0), F + 0x10c, 1));
    ACT_END(F);
}
RX_FUNCTION(0x00564560, rx_graffiti_vf40)

/* 005648c0 slot 42 (ECX = activity): the round's end by the clock (+0x294 >= +0x2bc) -> WindDown (state 3); (ONLINE: a client waits for
 * the host's round end +0xbc0). Frame: push ebx / esi. */
void rx_graffiti_vf42(rx_addr act) {
    ACT_FRAME(F, 0, 2);
    session_static();
    if (session_networked()) {
        session_static();
        if (U32(U32(0, 0x00da6cb0), 0x1c48) == 0 && U32(act, 0xbc0) == 0) { ACT_END(F); return; }
        act_unreached(0x00564936, "ONLINE: the networked graffiti round end");
    }
    if (F32(act, 0x294) >= F32(act, 0x2bc)) ACT_CALL(2, rx_activity_set_state(act, 3));
    ACT_END(F);
}
RX_FUNCTION(0x005648c0, rx_graffiti_vf42)

/* 005644f0 slot 43 (stdcall a, b, tie; RET 0xc; ECX unused) -> the standings' order: the more points held (+0xf14), then the more score
 * (+0xf1c), then (with tie) the start zones (+0x1dc of the players: a >= b gives 1, else -1). */
int32_t rx_graffiti_vf43(rx_addr act, rx_addr a, rx_addr b, uint32_t tie) {
    (void)act;
    if (I32(b, 0xf14) < I32(a, 0xf14)) return -1;
    if (I32(b, 0xf14) > I32(a, 0xf14)) return 1;
    if (I32(b, 0xf1c) < I32(a, 0xf1c)) return -1;
    if (I32(b, 0xf1c) > I32(a, 0xf1c)) return 1;
    if (tie == 0) return 0;
    return I32(U32(a, 0x74), 0x1dc) >= I32(U32(b, 0x74), 0x1dc) ? 1 : -1;
}
RX_FUNCTION(0x005644f0, rx_graffiti_vf43)

/* 0050df10 (ECX = driver, EAX = player; stdcall act; RET 4) -> driver: NetworkDriver's constructor (0050e1b0), +0xf14 .. +0xf20 = 0, the
 * class's vtable, the activity, +0xf24 = the .data -1 (0x9cd9e8). Frame: push edi. */
rx_addr rx_graffiti_driver_construct(rx_addr d, rx_addr player, rx_addr act) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(1, (void)rx_network_driver_construct(d, player));
    SU32(d, 0xf14, 0); SU32(d, 0xf18, 0); SU32(d, 0xf1c, 0); SU32(d, 0xf20, 0);
    SU32(d, 0, VT_GRAFFITI_DRIVER);
    SU32(d, 0xf10, act);
    SU32(d, 0xf24, U32(0, 0x009cd9e8));
    ACT_END(F);
    return d;
}
RX_FUNCTION(0x0050df10, rx_graffiti_driver_construct)

/* 005644c0 slot 44 (thiscall; player; RET 4) -> a new GraffitiDriver (0xf30 bytes: 0050df10), 0 when the allocation fails. Frame: push
 * esi. */
rx_addr rx_graffiti_vf44(rx_addr act, rx_addr player) {
    ACT_FRAME(F, 0, 1);
    rx_addr p;
    ACT_CALLR(p, 1, rx_reflex_aligned_alloc(0xf30));
    rx_addr d = 0;
    if (p != 0) ACT_CALLR(d, 1, rx_graffiti_driver_construct(p, player, act));
    ACT_END(F);
    return d;
}
RX_FUNCTION(0x005644c0, rx_graffiti_vf44)

/* ======================================================================================================================== */
/* GraffitiDriver                                                                                                            */

/* 0050df80 slot 1 (ECX = driver): NetworkDriver::vf1, +0xee8 = 2. Frame: push esi. */
void rx_graffiti_driver_vf1(rx_addr d) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(0, rx_network_driver_vf1(d));
    SU32(d, 0xee8, 2);
    ACT_END(F);
}
RX_FUNCTION(0x0050df80, rx_graffiti_driver_vf1)

/* 0050dfa0 slot 3 (ECX = driver): the reset. NetworkDriver::vf3; the start place (+0x144: position, direction +0x10) put on the vehicle's
 * world (its vf20 slot 0x50 -> the place, its axes F + 0x20 / F + 0x10 / F + 0x30) and the vehicle placed there (0045f600); +0x3c0 = 0,
 * no points, no score; the behaviours' and rules' vf6 (005092b0 / 005092f0). Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x34;
 * push ebx / esi / edi. */
void rx_graffiti_driver_vf3(rx_addr d) {
    ACT_FRAME_EBP(F, 16, 0x34, 3);
    ACT_CALL(0, rx_network_driver_vf3(d));
    const rx_addr veh = U32(U32(d, 0x74), 0x1ac);
    const rx_addr place = U32(d, 0x144);
    { const uint32_t a[5] = {place, place + 0x10, F + 0x20, F + 0x10, F + 0x30}; (void)act_vcall(U32(veh, 0x234), 0x50, 5, a); }
    ACT_CALL(3, rx_vehicle_place(veh, F + 0x20, F + 0x10, F + 0x30));
    SF32(d, 0x3c0, 0.0f);
    SU32(d, 0xf14, 0); SU32(d, 0xf18, 0); SU32(d, 0xf1c, 0);
    ACT_CALL(1, rx_driver_behaviors_vf6(d));
    ACT_CALL(1, rx_driver_rules_vf6(d));
    ACT_END(F);
}
RX_FUNCTION(0x0050dfa0, rx_graffiti_driver_vf3)

/* 0050e030 slot 7 (thiscall; dt; RET 4): NetworkDriver::vf7; outside a spectator (+0x908) the capture clock (+0xf24) runs down while
 * positive and at 0 the capture in progress (+0xf20) ends. Frame: push ebp; mov ebp, esp; and esp, -8; push ecx; push esi; dt through x87. */
void rx_graffiti_driver_vf7(rx_addr d, float dt) {
    ACT_FRAME_EBP(F, 8, 4, 1);
    const int spectator = U32(U32(d, 0x74), 0x908) != 0;
    ACT_CALL(1, rx_network_driver_update(d, x87_copy(dt)));
    if (!spectator) {
        const float t = F32(d, 0xf24);
        if (t > 0.0f) {
            const float r = rx_subs(t, dt);
            SF32(d, 0xf24, r);
            if (r <= 0.0f) SU32(d, 0xf20, 0);
        }
    }
    ACT_END(F);
}
RX_FUNCTION(0x0050e030, rx_graffiti_driver_vf7)

/* 0050e120 slot 17 (ECX = driver): the driver leaves: every capture point its vehicle holds (+0xb5c) released; no points, no score; then
 * a jump to NetworkDriver::vf14. Frame: push esi / edi (+ ebx / ebp in the loop). */
void rx_graffiti_driver_vf17(rx_addr d) {
    ACT_FRAME(F, 0, 2);
    const rx_addr act = U32(U32(d, 0x74), 0x94);
    for (int32_t i = 0; i < I32(act, 0xff074); i++) {
        const rx_addr p = U32(U32(act, 0xff070), 4u * (uint32_t)i);
        if (U32(p, 0xb5c) == U32(U32(d, 0x74), 0x1ac)) point_release(p);
    }
    SU32(d, 0xf14, 0); SU32(d, 0xf18, 0); SU32(d, 0xf1c, 0);
    ACT_END(F);
    rx_waypoint_network_vf14(d);
}
RX_FUNCTION(0x0050e120, rx_graffiti_driver_vf17)

/* ======================================================================================================================== */
/* GraffitiActivityAudioLogic                                                                                                */

/* 0058b190 (EAX = the activity's channel; stdcall logic; RET 4): "GraffitiPointCaptured" with 00582520 (proxy 0x945de4:
 * void(BaseDriver*)); 0058b250 its leave */
void rx_graffiti_audio_listen_captured(rx_addr channel, rx_addr logic) {
    ACT_CALL(1, act_subscribe(0x0058b190, 1, 1, 4, channel, 0x0093ee80 /* "GraffitiPointCaptured" */, 0x00945de4, 0x009cda44, logic, 0x00582520));
}
RX_FUNCTION(0x0058b190, rx_graffiti_audio_listen_captured)
void rx_graffiti_audio_unlisten_captured(rx_addr channel, rx_addr logic) {
    act_unlisten(1, channel, 0x0093ee80, 0x00945de4, 0x009cda44, logic, 0x00582520);
}
RX_FUNCTION(0x0058b250, rx_graffiti_audio_unlisten_captured)

/* 00582490 slot 29 (thiscall; activity; RET 4): ActivityAudioLogic::vf29; the sound "ramp_capture_01" (the database's vf27 slot 0x6c,
 * type 'soundatd') into +0x98; "GraffitiPointCaptured" listened (0058b190). Frame: push ebp; mov ebp, esp; and esp, -8; push ecx; push
 * esi. */
void rx_graffiti_audio_vf29(rx_addr logic, rx_addr act) {
    ACT_FRAME_EBP(F, 8, 4, 1);
    ACT_CALL(1, rx_audio_logic_vf29(logic, act));
    SU32(logic, 0x98, 0);
    { const uint32_t a[4] = {0x00942888 /* "ramp_capture_01" */, 0x6e756f73, 0x74616464, 1};
      SU32(logic, 0x98, act_vcall(U32(U32(0, 0x00a95844), 8), 0x6c, 4, a)); }
    ACT_CALL(1, rx_graffiti_audio_listen_captured(U32(U32(logic, 0x50), 0x290), logic));
    ACT_END(F);
}
RX_FUNCTION(0x00582490, rx_graffiti_audio_vf29)

/* 005824f0 slot 30 (ECX = logic): ActivityAudioLogic::vf30; with an activity (+0x50) "GraffitiPointCaptured" left. Frame: push esi. */
void rx_graffiti_audio_vf30(rx_addr logic) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(0, rx_audio_logic_vf30(logic));
    if (U32(logic, 0x50) != 0) ACT_CALL(1, rx_graffiti_audio_unlisten_captured(U32(U32(logic, 0x50), 0x290), logic));
    ACT_END(F);
}
RX_FUNCTION(0x005824f0, rx_graffiti_audio_vf30)

/* 00582510 slot 32 (ECX = logic): a jump to ActivityAudioLogic::vf32 */
void rx_graffiti_audio_vf32(rx_addr logic) { rx_audio_logic_vf32(logic); }
RX_FUNCTION(0x00582510, rx_graffiti_audio_vf32)

/* 00582010 ActivityAudioLogic::vf19 (thiscall; two arguments; RET 8): the cue "InProgress Entered", +0x58 = 0. Frame: push esi. */
void rx_audio_logic_vf19(rx_addr logic, uint32_t a, uint32_t b) {
    (void)a; (void)b;
    ACT_FRAME(F, 0, 1);
    ACT_CALL(4, rx_audio_cue(logic, 0x009427e4 /* "InProgress Entered" */, 0, 0));
    SU32(logic, 0x58, 0);
    ACT_END(F);
}
RX_FUNCTION(0x00582010, rx_audio_logic_vf19)

/* 00582520 (thiscall logic; driver; RET 4): a point captured: the crowd excited by [0x9c47f4] (00674440), and with the game's LCG
 * ([0x9cb860] * 0x41c64e6d + 0x3039, the draw count [0x9cb864] + 1; (seed >> 16) & 0x7fff * [0x9776a0] in double) below [0x9c481c] the
 * crowd reacts (006745d0: 1 when [0x9c47f4] is negative, else 2). */
void rx_graffiti_audio_captured(rx_addr logic, rx_addr driver) {
    (void)logic; (void)driver;
    ACT_CALL(1, rx_world_crowd_excite(x87_copy(F32(0, 0x009c47f4))));
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    const uint32_t seed = U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u;
    SU32(0, 0x009cb860, seed);
    const float x = (float)((double)(float)(int32_t)((seed >> 16) & 0x7fffu) * F64(0, 0x009776a0));
    if (F32(0, 0x009c481c) > x) {
        const uint32_t kind = 0.0f > F32(0, 0x009c47f4) ? 1u : 2u;
        ACT_CALL(0, rx_world_crowd_react(kind));
    }
}
RX_FUNCTION(0x00582520, rx_graffiti_audio_captured)
