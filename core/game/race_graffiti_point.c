/* race_graffiti_point.c -- GraffitiCapturePoint, the Graffiti party mode's scene entity (RSData GraffitiCapturePointRSData, type 0x1040;
 * 0xcb0 bytes; vtables 0x00938b9c (Entity), 0x00938c14 (IRenderable at +0x90), 0x00938c54 (IRenderEvent at +0x1a0)), hand-transliterated
 * from the original (the address of each function is in its comment; frames and the stack depth of each call are the original's).
 *
 * The entity: a ModelEntity / CollisionEntity (constructor 00818f60, no collision shape: vf5 runs only ModelInstance_Init and
 * Entity::vf5) that a rider captures with a stunt. vf5 reads LinkedTriggerID (+0xb00: the name of an MXTrigger, type 0x1028, of the
 * activity's area) and CountTowardCapPointTotal (+0xb44, 1 when absent); MXActivityGraffiti::vf2 links the point to the activity (+0xb80)
 * and gives it its label (race_graffiti.c 00408220). The point keeps the riders inside its trigger (+0xb48: 12-byte records {vehicle,
 * state, inside}, count +0xb4c, capacity +0xb50 = 12, growth +0xb54 = 5): "VehicleEntered" adds a rider (state 2 when its rider is in a
 * stunt already, else 0), "VehicleExited" clears its inside flag, vf19 moves state 0 <-> 1 with the vehicle's slot 43 (0xac) and drops
 * the records that are outside in state 0. "StuntBegins" marks the rider's record 2 (a stunt begun inside); "StuntFinalScore" of a rider
 * with a record: a score above the point's (+0xb64) captures the point (00408090: the old holder's driver loses the point and its score,
 * the new holder +0xb5c (vehicle) / +0xb60 (its +0x190) and the colour of its driver's number (0x9356ec[driver + 0x148]) at +0xb70, the
 * driver gains the point, the score and the capture clock (0050e0a0, "GraffitiPointCaptured")); then the rider's records go.
 * "StuntFailed" / "StuntKickOut" drop the rider's records; "Activity Init" (00407930) releases the point.
 *
 * Content: no installed level holds a GraffitiCapturePoint (nor a MiniGameGraffiti subscene), so the product's session refuses a Graffiti
 * event before it loads (api/rx_session.c minigame_content_missing); the scene factory builds the class (build_scene.c case 0x1040) for a
 * level that has one (the restored-content pass / a custom level). The oracle verifies every function here on test-fixture points
 * (core/oracle/partylast_probe.cpp: points built by the original's factory path on a fabricated RSData, original vs core under Verify).
 *
 * RENDER BOUNDARY (the core's precedent for render objects, as w-snake's trail mesh / the players' name plates): the label is an
 * AnimatedTextIcon3dBillboard at +0xbd0 (constructor 006b62a0, its text object +0xc48, destructor 007ffba0), the same class as the
 * players' name plates, which the product never builds (build_activity_player.c / loop_player_destroy.c): the oracle runs the original's
 * constructor / destructor / text calls, the product leaves the label out. The draw slots are not ported: IRenderable::vf2 004084c0 (the
 * label's text = the score +0xb64, its colour from +0xb70, then Model_SubmitSkinned) and IRenderEvent::vf0 00408650 (Model_RenderInstance
 * / 007f6a60): the product never draws; Unity draws a point from its game state (the model, +0xb5c holder, +0xb64 score, +0xb70 colour).
 * The shader constant override vf5 adds ("usr_diffuse_constantcolor" -> +0xb70, the ModelInstance's override list +0x1b0 / +0x5b0 /
 * +0x5f0, count +0xaf0) is game state and kept.
 *
 * ONLINE (a Quazal session: [0x00da6cb0] + 0x1c4c set and the mode +0x1b0 not 1; none exists in the product): a capture is sent to the
 * host's NetGraffiti object (00407f10: 006f0690) instead of taken (STOP_AUDIT R125); 00408110 (the network handler 00564ad0's capture of
 * a host's message) is ported but reached only from that handler. */
#include "race_minigames.h"
#include "race_driver.h"
#include "race_extern.h"
#include "build_activity_int.h"
#include "build_activity_teardown.h"
#include "build_scene_int.h"
#include "build_scene_entity.h"
#include "fourwheel_xml_packet.h"
#include "loop_player.h"
#include "loop_platform.h"
#include "race_activity_misc.h"
#include "world_misc.h"
#include "anim_entity_dtors.h"
#include "crt.h"
#include <string.h>

#define VT_POINT          0x00938b9cu
#define VT_POINT_RENDER   0x00938c14u
#define VT_POINT_EVENT    0x00938c54u
#define VT_IRENDEREVENT   0x009370ccu

/* the handlers' addresses (the proxies' member functions) */
#define H_ENTERED   0x00407af0u
#define H_EXITED    0x00407c00u
#define H_INIT      0x00407930u
#define H_SCORE     0x00408180u
#define H_BEGINS    0x004081c0u
#define H_FAILED    0x004081e0u
#define H_KICKOUT   0x00408200u

static void session_static(void) {
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
}
static int session_networked(void) {
    const rx_addr s = U32(0, 0x00da6cb0);
    return U32(s, 0x1c4c) != 0 && U32(s, 0x1b0) != 1;
}

/* 00564970 for a point: the driver of the vehicle in the point's activity (EDI = +0xb80). A point outside a Graffiti activity (+0xb80 0:
 * MXActivityGraffiti::vf2 links it) or an activity without drivers faults in the original (the reads at 00564970 / the caller's +0xef0) */
static rx_addr point_driver(rx_addr p, rx_addr vehicle, uint32_t site) {
    const rx_addr act = U32(p, 0xb80);
    if (act == 0) act_unreached(site, "a GraffitiCapturePoint outside a Graffiti activity (the original reads a null activity in 00564970)");
    rx_addr d;
    ACT_CALLR(d, 1, rx_graffiti_driver_of(act, vehicle));
    if (d == 0) act_unreached(site, "a GraffitiCapturePoint's activity without drivers (the original reads a null driver)");
    return d;
}

/* ======================================================================================================================== */
/* the listener templates (the proxy {+0 vtable, +4 0, +8 the signature's type name, +0xc the point, +0x10 the member function, +0x14 its
 * adjustor}, 0x18 bytes)                                                                                                      */

/* the subscription templates 00419220 (ECX = the event name) / 004192f0 / 004193c0 / 00419490 / 00419560 / 00419630 (stdcall point,
 * member, adjustor; RET 0xc): the channel's event (00593220), a new proxy, appended to the event's handler list (005b9ff0, under the
 * event's lock +0x24) when the signatures' names match (the proxy's vf1 008ad240 returns +8). Frame: push ecx / ebx / esi / edi; the
 * proxy kept in the first argument's slot. */
static void point_listen(rx_addr channel, rx_addr name, rx_addr vtable, rx_addr td, rx_addr p, rx_addr method, rx_addr adj) {
    const rx_addr E = rx_sp();
    ACT_FRAME(F, 0, 4);
    rx_addr ev;
    ACT_CALLR(ev, 2, rx_world_registry_find(channel, name));            /* 00593220 */
    rx_addr h;
    ACT_CALLR(h, 1, rx_reflex_aligned_alloc(0x18));
    if (h == 0) act_unreached(0x0041927b, "the handler proxy's allocation failed (the original reads a null proxy)");
    SU32(h, 4, 0); SU32(h, 0, vtable); SU32(h, 0xc, p); SU32(h, 0x10, method); SU32(h, 0x14, adj);
    SU32(h, 8, crt_type_info_name(td, 0x009e1a7c));                     /* typeid(signature).name() */
    if (crt_strcmp(U32(ev, 4), U32(h, 8)) == 0) {
        SU32(E, 4, h);
        loop_enter_cs(ev + 0x24);
        ACT_CALL(1, rx_event_handler_list_push_back(ev + 8, h));       /* 005b9ff0 (EAX = &proxy) */
        loop_leave_cs(ev + 0x24);
    }
    ACT_END(F);
}
void rx_graffiti_point_listen_trigger(rx_addr name, rx_addr p, rx_addr method, rx_addr adj) {          /* 00419220 */
    point_listen(U32(0, 0x00a89bc8), name, 0x00939eac, 0x009cbb7c, p, method, adj);
}
RX_FUNCTION(0x00419220, rx_graffiti_point_listen_trigger)
void rx_graffiti_point_listen_init(rx_addr p, rx_addr method, rx_addr adj) {                           /* 004192f0 */
    point_listen(U32(0, 0x00a93f74), 0x00935ea4 /* "Activity Init" */, 0x00939ecc, 0x009cbd24, p, method, adj);
}
RX_FUNCTION(0x004192f0, rx_graffiti_point_listen_init)
void rx_graffiti_point_listen_score(rx_addr p, rx_addr method, rx_addr adj) {                          /* 004193c0 */
    point_listen(U32(0, 0x00a94318), 0x0092fc60 /* "StuntFinalScore" */, 0x00939eec, 0x009cbda8, p, method, adj);
}
RX_FUNCTION(0x004193c0, rx_graffiti_point_listen_score)
void rx_graffiti_point_listen_begins(rx_addr p, rx_addr method, rx_addr adj) {                         /* 00419490 */
    point_listen(U32(0, 0x00a94318), 0x0092fd00 /* "StuntBegins" */, 0x00939f0c, 0x009cbe2c, p, method, adj);
}
RX_FUNCTION(0x00419490, rx_graffiti_point_listen_begins)
void rx_graffiti_point_listen_failed(rx_addr p, rx_addr method, rx_addr adj) {                         /* 00419560 */
    point_listen(U32(0, 0x00a94318), 0x00960764 /* "StuntFailed" */, 0x00939f2c, 0x009cbeb4, p, method, adj);
}
RX_FUNCTION(0x00419560, rx_graffiti_point_listen_failed)
void rx_graffiti_point_listen_kickout(rx_addr p, rx_addr method, rx_addr adj) {                        /* 00419630 */
    point_listen(U32(0, 0x00a94304), 0x00960770 /* "StuntKickOut" */, 0x00939f4c, 0x009cbf38, p, method, adj);
}
RX_FUNCTION(0x00419630, rx_graffiti_point_listen_kickout)

/* the unsubscription templates 00418f80 (ECX = the event name) / 00418ff0 / 00419060 / 004190d0 / 00419140 / 004191b0 (stdcall point,
 * member, adjustor; RET 0xc): the key {vtable, 0, the signature's type name, point, member, adjustor} on the frame (ESP + 8 after
 * `sub esp, 0x1c; push esi`), the channel's event (00593220), the listener that accepts the key removed (00592ef0) */
static void point_unlisten(rx_addr channel, rx_addr name, rx_addr vtable, rx_addr td, rx_addr p, rx_addr method, rx_addr adj) {
    ACT_FRAME(F, 0x1c, 1);
    SU32(F, -4, name); SU32(F, -8, channel);
    rx_addr ev;
    ACT_CALLR(ev, 2, rx_world_registry_find(channel, name));            /* 00593220 */
    const rx_addr key = F + 8;
    SU32(key, 0x14, adj);
    SU32(key, 4, 0);
    SU32(key, 0, vtable);
    SU32(key, 0xc, p);
    SU32(key, 0x10, method);
    SU32(key, 8, crt_type_info_name(td, 0x009e1a7c));                   /* push 0x9e1a7c; call [0x916388] */
    SU32(F, -4, key);
    ACT_CALL(1, rx_channel_remove_listener(ev, key));                  /* 00592ef0 (EAX = the event) */
    ACT_END(F);
}
void rx_graffiti_point_unlisten_trigger(rx_addr name, rx_addr p, rx_addr method, rx_addr adj) {        /* 00418f80 */
    point_unlisten(U32(0, 0x00a89bc8), name, 0x00939eac, 0x009cbb7c, p, method, adj);
}
RX_FUNCTION(0x00418f80, rx_graffiti_point_unlisten_trigger)
void rx_graffiti_point_unlisten_init(rx_addr p, rx_addr method, rx_addr adj) {                         /* 00418ff0 */
    point_unlisten(U32(0, 0x00a93f74), 0x00935ea4 /* "Activity Init" */, 0x00939ecc, 0x009cbd24, p, method, adj);
}
RX_FUNCTION(0x00418ff0, rx_graffiti_point_unlisten_init)
void rx_graffiti_point_unlisten_score(rx_addr p, rx_addr method, rx_addr adj) {                        /* 00419060 */
    point_unlisten(U32(0, 0x00a94318), 0x0092fc60 /* "StuntFinalScore" */, 0x00939eec, 0x009cbda8, p, method, adj);
}
RX_FUNCTION(0x00419060, rx_graffiti_point_unlisten_score)
void rx_graffiti_point_unlisten_begins(rx_addr p, rx_addr method, rx_addr adj) {                       /* 004190d0 */
    point_unlisten(U32(0, 0x00a94318), 0x0092fd00 /* "StuntBegins" */, 0x00939f0c, 0x009cbe2c, p, method, adj);
}
RX_FUNCTION(0x004190d0, rx_graffiti_point_unlisten_begins)
void rx_graffiti_point_unlisten_failed(rx_addr p, rx_addr method, rx_addr adj) {                       /* 00419140 */
    point_unlisten(U32(0, 0x00a94318), 0x0092fd20 /* "StuntFailed" */, 0x00939f2c, 0x009cbeb4, p, method, adj);
}
RX_FUNCTION(0x00419140, rx_graffiti_point_unlisten_failed)
void rx_graffiti_point_unlisten_kickout(rx_addr p, rx_addr method, rx_addr adj) {                      /* 004191b0 */
    point_unlisten(U32(0, 0x00a94304), 0x00960770 /* "StuntKickOut" */, 0x00939f4c, 0x009cbf38, p, method, adj);
}
RX_FUNCTION(0x004191b0, rx_graffiti_point_unlisten_kickout)

/* the proxies' calls (slot 2) for the two signatures no other class had ported: 0041a0d0 void (X::*)(MXPlayer*, float) (the payload
 * {player, score}: the float through the x87, a signalling NaN comes out quiet) and 006c57d0 void (X::*)(MXPlayer*, int, int) (push esi
 * around the call); the object +0xc + the adjustor +0x14, the member +0x10 (thiscall proxy, stack payload pointer; RET 4) */
void rx_event_proxy_call_member_player_float(rx_addr proxy, rx_addr payload) {
    const rx_addr d = U32(payload, 0);
    uint32_t fb = U32(d, 4);
    if ((fb & 0x7f800000u) == 0x7f800000u && (fb & 0x7fffffu)) fb |= 0x400000u;
    const rx_addr fn = U32(proxy, 0x10), obj = U32(proxy, 0x14) + U32(proxy, 0xc);
    void (*f)(rx_addr, rx_addr, float) = RX_FN(fn, void (*)(rx_addr, rx_addr, float));
    if (f) { ACT_CALL(2, f(obj, U32(d, 0), rx_bf(fb))); return; }
    const uint32_t a[2] = {U32(d, 0), fb};
    rx_call_original(fn, RX_THISCALL, obj, 2, a);
}
RX_FUNCTION(0x0041a0d0, rx_event_proxy_call_member_player_float)
void rx_event_proxy_call_member_player_int2(rx_addr proxy, rx_addr payload) {
    ACT_FRAME(F, 0, 1);
    const rx_addr d = U32(payload, 0);
    const uint32_t a[3] = {U32(d, 0), U32(d, 4), U32(d, 8)};
    (void)act_call_fn(U32(proxy, 0x10), RX_THISCALL, U32(proxy, 0x14) + U32(proxy, 0xc), 3, a);
    ACT_END(F);
}
RX_FUNCTION(0x006c57d0, rx_event_proxy_call_member_player_int2)

/* ======================================================================================================================== */
/* construction / destruction                                                                                                 */

/* 004075c0 (EDI = point) -> point: CollisionEntity's constructor (00818f60), IRenderEvent at +0x1a0, the class's vtables, the override
 * count +0xaf0 / +0xaf1 0, no rider records (+0xb48 .. +0xb58), the label (006b62a0 on +0xbd0: RENDER BOUNDARY, the oracle's only),
 * +0x70 1, no activity, the colour white, counted (+0xb44 1). Frame: push ecx / ebx / esi. */
rx_addr rx_graffiti_point_construct(rx_addr p) {
    ACT_FRAME(F, 0, 3);
    ACT_CALL(0, (void)rx_collision_entity_construct(p));
    SU32(p, 0x1a0, VT_IRENDEREVENT);
    SU32(p, 0, VT_POINT);
    SU32(p, 0x90, VT_POINT_RENDER);
    SU32(p, 0x1a0, VT_POINT_EVENT);
    SU8(p, 0xaf0, 0);
    SU8(p, 0xaf1, 0);
    SU32(p, 0xb48, 0); SU32(p, 0xb4c, 0); SU32(p, 0xb50, 0); SU32(p, 0xb54, 0); SU32(p, 0xb58, 0);
#ifdef RX_ORACLE
    {   /* 006b62a0 (ESI = point + 0xbd0): the label's AnimatedTextIcon3dBillboard (render only: the product never builds it) */
        rx_regs in, o; memset(&in, 0, sizeof in); in.r[6] = p + 0xbd0; in.r[7] = p;
        rx_call_original_ex(0x006b62a0, &in, &o, 0, 0);
    }
#endif
    SU32(p, 0x70, 1);
    SU32(p, 0xb80, 0);
    SF32(p, 0xb70, 1.0f /* 0x931f24 */); SF32(p, 0xb74, 1.0f); SF32(p, 0xb78, 1.0f); SF32(p, 0xb7c, 1.0f);
    SU32(p, 0xb44, 1);
    ACT_END(F);
    return p;
}
RX_FUNCTION(0x004075c0, rx_graffiti_point_construct)
/* the factory's case 0x1040 (004046a0: FUN_00401f90(0xcb0), then 004075c0 when the allocation succeeded) */
rx_addr rx_graffiti_point_create(void) {
    rx_addr p;
    ACT_CALLR(p, 1, rx_tuning_buffer_alloc(0xcb0));
    if (p == 0) return 0;
    return rx_graffiti_point_construct(p);
}

/* 004076c0 (EAX = point): the class's vtables, the seven listeners leave (00418f80 x 2 .. 004191b0), the label's destructor (007ffba0 on
 * +0xbd0: RENDER BOUNDARY, the oracle's only), the rider records freed, CollisionEntity's destructor (00819050). Frame: push ebp; mov ebp,
 * esp; and esp, -8; push ecx; push edi. */
void rx_graffiti_point_destroy(rx_addr p) {
    ACT_FRAME_EBP(F, 8, 4, 1);
    SU32(p, 0, VT_POINT);
    SU32(p, 0x90, VT_POINT_RENDER);
    SU32(p, 0x1a0, VT_POINT_EVENT);
    ACT_CALL(3, rx_graffiti_point_unlisten_trigger(0x00935e84 /* "VehicleEntered" */, p, H_ENTERED, 0));
    ACT_CALL(3, rx_graffiti_point_unlisten_trigger(0x00935e94 /* "VehicleExited" */, p, H_EXITED, 0));
    ACT_CALL(3, rx_graffiti_point_unlisten_init(p, H_INIT, 0));
    ACT_CALL(3, rx_graffiti_point_unlisten_score(p, H_SCORE, 0));
    ACT_CALL(3, rx_graffiti_point_unlisten_begins(p, H_BEGINS, 0));
    ACT_CALL(3, rx_graffiti_point_unlisten_failed(p, H_FAILED, 0));
    ACT_CALL(3, rx_graffiti_point_unlisten_kickout(p, H_KICKOUT, 0));
#ifdef RX_ORACLE
    /* 007ffba0 (ECX = point + 0xbd0): TextIcon3dBillboard's destructor, the label (render only: the product never builds it) */
    rx_call_original(0x007ffba0, RX_THISCALL, p + 0xbd0, 0, 0);
#endif
    ACT_CALL(1, rx_reflex_aligned_free(U32(p, 0xb48)));
    ACT_CALL(0, rx_collision_entity_destroy(p));
    ACT_END(F);
}
RX_FUNCTION(0x004076c0, rx_graffiti_point_destroy)

/* 00407670 slot 0 (thiscall; flags; RET 4) -> point: the destructor, then (flags bit 0) _aligned_free under the allocator lock (critical
 * section 0x00d61704 while [0x00da67d8]). Frame: push esi. */
rx_addr rx_graffiti_point_sdtor(rx_addr p, uint32_t flags) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(0, rx_graffiti_point_destroy(p));
    if (flags & 1) {
        if (U8(0, 0x00da67d8) != 0) loop_enter_cs(0x00d61704);
        rx_set_sp(F - 4);
        SU32(F, -4, p);
        crt_aligned_free(p);                                           /* [0x00916350] _aligned_free */
        rx_set_sp(F);
        if (U8(0, 0x00da67d8) != 0) loop_leave_cs(0x00d61704);
    }
    ACT_END(F);
    return p;
}
RX_FUNCTION(0x00407670, rx_graffiti_point_sdtor)
/* 0041b170 IRenderable slot 0 (ECX = point + 0x90): `sub ecx, 0x90; jmp 00407670` */
rx_addr rx_graffiti_point_sdtor_render(rx_addr r, uint32_t flags) { return rx_graffiti_point_sdtor(r - 0x90, flags); }
RX_FUNCTION(0x0041b170, rx_graffiti_point_sdtor_render)

/* ---- the RSData reads of vf5: the product's reader (rx_scene_rs(), build_scene.c) or, in the oracle where the original's scene
 * loader made the call, the original's reader [0x00a9583c] (0080e320 / 0080e180 / 0080e540) ---- */
static void point_rs_linked(rx_addr dst) {
#ifdef RX_ORACLE
    if (rx_scene_rs() == NULL) {
        const uint32_t a[1] = {0x00935eb4 /* "LinkedTriggerID" */};
        const rx_addr s = rx_call_original(0x0080e320, RX_CDECL, 0, 1, a);
        crt_strncpy(dst, s, 0x3f);
        return;
    }
#endif
    const char* s = rs_str("LinkedTriggerID");
    uint32_t i = 0;
    for (; i < 0x3f && s[i]; i++) SU8(dst, i, (uint8_t)s[i]);   /* strncpy(.., 0x3f): zero padded */
    for (; i < 0x3f; i++) SU8(dst, i, 0);
}
static int point_rs_has_counted(void) {
#ifdef RX_ORACLE
    if (rx_scene_rs() == NULL) {
        const uint32_t a[2] = {0x00935ec4 /* "CountTowardCapPointTotal" */, RS_BOOL};
        return (rx_call_original(0x0080e180, RX_CDECL, 0, 2, a) & 0xff) != 0;
    }
#endif
    return rs_has("CountTowardCapPointTotal", RS_BOOL);
}
static uint32_t point_rs_counted(rx_addr out) {
#ifdef RX_ORACLE
    if (rx_scene_rs() == NULL) {   /* 0080e540 (ESI = the result's address) */
        rx_regs in, o; memset(&in, 0, sizeof in); in.r[6] = out;
        const uint32_t a[1] = {0x00935ec4};
        rx_call_original_ex(0x0080e540, &in, &o, 1, a);
        return U32(out, 0);
    }
#endif
    SU32(out, 0, rs_bool("CountTowardCapPointTotal"));
    return U32(out, 0);
}

/* 00407780 slot 5 (thiscall; the reader; RET 4): ModelInstance_Init (+0xa0), Entity::vf5, the matrix copied to +0xc0; LinkedTriggerID
 * (+0xb00, 63 characters); CountTowardCapPointTotal when present (+0xb44); "VehicleEntered" / "VehicleExited" listened (00419220 with
 * 00407af0 / 00407c00); 12 rider records (+0xb48, 0x90 bytes), none in use, growth 5, the limit 0x7fffffff; no holder, no score; "Activity
 * Init", "StuntFinalScore", "StuntBegins", "StuntFailed", "StuntKickOut" listened; the ModelInstance's shader constant override
 * "usr_diffuse_constantcolor" -> the colour +0xb70 (kind 1). Frame: push ebp; mov ebp, esp; and esp, -8; push ecx; push ebx / esi / edi
 * (the bool at F + 0xc). */
uint32_t rx_graffiti_point_vf5(rx_addr p, uint32_t reader) {
    ACT_FRAME_EBP(F, 8, 4, 3);
#ifdef RX_ORACLE
    /* the oracle's call comes from the original's scene loader (or a probe) with the original's reader [0x00a9583c]: the base parts
     * (ModelInstance_Init, Entity::vf5) through the original's functions (the core's read the product's reader) */
    if (rx_scene_rs() == NULL) {
        const uint32_t i[1] = {p + 0xa0}, a[1] = {reader};
        rx_call_original(0x007f12c0, RX_STDCALL, 0, 1, i);
        rx_call_original(0x0080ded0, RX_THISCALL, p, 1, a);
    } else
#endif
    {
        ACT_CALL(1, rx_model_instance_init(p + 0xa0));
        ACT_CALL(1, rx_entity_vf5(p));
    }
    (void)reader;
    for (uint32_t k = 0; k < 0x40; k += 4) SU32(p, 0xc0 + k, U32(p, 0x30 + k));
    point_rs_linked(p + 0xb00);                                        /* (the reader's calls at the original's ESP) */
    SU8(p, 0xb3f, 0);
    if (point_rs_has_counted()) SU32(p, 0xb44, point_rs_counted(F + 0xc));
    ACT_CALL(3, rx_graffiti_point_listen_trigger(0x00935e84 /* "VehicleEntered" */, p, H_ENTERED, 0));
    ACT_CALL(3, rx_graffiti_point_listen_trigger(0x00935e94 /* "VehicleExited" */, p, H_EXITED, 0));
    rx_addr recs;
    ACT_CALLR(recs, 1, rx_reflex_aligned_alloc(0x90));                 /* 12 * 12 (the multiplication's overflow saturates) */
    SU32(p, 0xb48, recs);
    SU32(p, 0xb4c, 0);
    SU32(p, 0xb50, 0xc);
    SU32(p, 0xb54, 5);
    SU32(p, 0xb58, 0x7fffffff);
    SU32(p, 0xb5c, 0);
    SU32(p, 0xb64, 0);
    ACT_CALL(3, rx_graffiti_point_listen_init(p, H_INIT, 0));
    ACT_CALL(3, rx_graffiti_point_listen_score(p, H_SCORE, 0));
    ACT_CALL(3, rx_graffiti_point_listen_begins(p, H_BEGINS, 0));
    ACT_CALL(3, rx_graffiti_point_listen_failed(p, H_FAILED, 0));
    ACT_CALL(3, rx_graffiti_point_listen_kickout(p, H_KICKOUT, 0));
    const rx_addr ov = p + 0x1b0;                                      /* the ModelInstance's override list */
    const uint32_t n = U8(p, 0xaf0);
    ACT_CALL(3, (void)crt_strncpy(ov + 0x40 * n, 0x00935ee0 /* "usr_diffuse_constantcolor" */, 0x3f));
    SU8(ov + 0x40 * n, 0x3f, 0);
    SU32(ov, 0x440 + 4 * (uint32_t)U8(ov, 0x940), p + 0xb70);
    SU32(ov, 0x400 + 4 * (uint32_t)U8(ov, 0x940), 1);
    SU8(ov, 0x940, (uint8_t)(U8(ov, 0x940) + 1));
    ACT_END(F);
    return 0;
}
RX_FUNCTION(0x00407780, rx_graffiti_point_vf5)

/* ======================================================================================================================== */
/* the rider records and the capture                                                                                          */

/* 00407930 "Activity Init" (thiscall; activity; RET 4): the point released (no holder, no score, white) */
void rx_graffiti_point_on_init(rx_addr p, rx_addr act) {
    (void)act;
    SU32(p, 0xb5c, 0); SU32(p, 0xb60, 0); SU32(p, 0xb64, 0);
    SF32(p, 0xb70, 1.0f); SF32(p, 0xb74, 1.0f); SF32(p, 0xb78, 1.0f); SF32(p, 0xb7c, 1.0f);
}
RX_FUNCTION(0x00407930, rx_graffiti_point_on_init)

/* 00407970 (ESI = point, EAX = vehicle): the holder. A vehicle: +0xb60 its +0x190, the colour of its driver's number (0x9356ec[driver +
 * 0x148]: r, g, b bytes x 1/255 (0x9775a0) through the x87: an exact product, one rounding; the alpha +0xb7c kept); none: released (no
 * score, white). Frame: push ecx (+ push edi with a vehicle). */
void rx_graffiti_point_set_holder(rx_addr p, rx_addr vehicle) {
    ACT_FRAME(F, 0, 1);
    SU32(p, 0xb5c, vehicle);
    if (vehicle != 0) {
        ACT_FRAME(G, 0, 1);
        SU32(p, 0xb60, U32(vehicle, 0x190));
        rx_addr d;
        ACT_CALLR(d, 1, rx_graffiti_driver_of(U32(p, 0xb80), vehicle));
        const uint32_t c = U32(0, 0x009356ec + 4 * U32(d, 0x148));
        const float k = F32(0, 0x009775a0);
        SF32(p, 0xb70, rx_muls((float)(int32_t)((c >> 16) & 0xff), k));
        SF32(p, 0xb74, rx_muls((float)(int32_t)((c >> 8) & 0xff), k));
        SF32(p, 0xb78, rx_muls((float)(int32_t)(c & 0xff), k));
        ACT_END(G);
    } else {
        SU32(p, 0xb60, 0);
        SU32(p, 0xb64, 0);
        SF32(p, 0xb70, 1.0f); SF32(p, 0xb74, 1.0f); SF32(p, 0xb78, 1.0f); SF32(p, 0xb7c, 1.0f);
    }
    ACT_END(F);
}
RX_FUNCTION(0x00407970, rx_graffiti_point_set_holder)

/* 00407a30 (stdcall point, vehicle; RET 8) -> the vehicle's record, or 0 */
rx_addr rx_graffiti_point_record(rx_addr p, rx_addr vehicle) {
    rx_addr r = 0;
    for (int32_t i = 0; i < I32(p, 0xb4c) && r == 0; i++) {
        const rx_addr e = U32(p, 0xb48) + 0xc * (uint32_t)i;
        if (U32(e, 0) == vehicle) r = e;
    }
    return r;
}
RX_FUNCTION(0x00407a30, rx_graffiti_point_record)

/* 0041a530 (ESI = the record vector +0xb48 {data, count, capacity, growth}; stdcall capacity; RET 4): a new block (capacity * 12, the
 * multiplication's overflow saturates), the old capacity's records copied, the old block freed. Frame: push ebp / edi; the allocation's,
 * the copy's and the free's arguments stay pushed (add esp, 0x14 at the end). */
void rx_graffiti_point_records_grow(rx_addr v, uint32_t cap) {
    ACT_FRAME(F, 0, 2);
    const uint64_t bytes = (uint64_t)cap * 0xc;
    rx_addr b;
    SU32(F, -4, bytes >> 32 ? 0xffffffffu : (uint32_t)bytes);
    rx_set_sp(F - 4);
    ACT_CALLR(b, 0, rx_reflex_aligned_alloc(bytes >> 32 ? 0xffffffffu : (uint32_t)bytes));
    SU32(F, -8, 0xc * U32(v, 8)); SU32(F, -0xc, U32(v, 0)); SU32(F, -0x10, b);
    rx_set_sp(F - 0x10);
    ACT_CALL(0, (void)crt_memcpy(b, U32(v, 0), 0xc * U32(v, 8)));
    SU32(F, -0x14, U32(v, 0));
    rx_set_sp(F - 0x14);
    ACT_CALL(0, rx_reflex_aligned_free(U32(v, 0)));
    rx_set_sp(F);
    SU32(v, 0, b);
    SU32(v, 8, cap);
    ACT_END(F);
}
RX_FUNCTION(0x0041a530, rx_graffiti_point_records_grow)

/* 00407a70 (EAX = vehicle; stdcall point; RET 4): a rider enters: outside the vehicle's slot 43 (0xac: nonzero, no record), a record
 * {vehicle, 2 when its rider (+0x1a0) is in a stunt (+0xaf10) else 0, inside 1} appended (the vector grown by its growth when full).
 * Frame: sub esp, 0xc; push esi (+ push edi); the record's first two words built at F + 4. */
void rx_graffiti_point_add_rider(rx_addr vehicle, rx_addr p) {
    ACT_FRAME(F, 0xc, 1);
    SU32(F, 4, vehicle);
    uint32_t r;
    ACT_CALLR(r, 0, act_vcall(vehicle, 0xac, 0, 0));
    if (r == 0) {
        ACT_FRAME(G, 0, 1);
        const uint32_t flag = U32(U32(vehicle, 0x1a0), 0xaf10) != 0 ? 2u : 0u;
        const rx_addr v = p + 0xb48;
        SU32(F, 8, flag);
        if (I32(v, 4) >= I32(p, 0xb50)) ACT_CALL(1, rx_graffiti_point_records_grow(v, U32(v, 0xc) + U32(p, 0xb50)));
        const rx_addr e = U32(v, 0) + 0xc * U32(v, 4);
        SU32(e, 0, U32(F, 4)); SU32(e, 4, U32(F, 8));
        SU32(e, 8, 1);
        SU32(v, 4, U32(v, 4) + 1);
        ACT_END(G);
    }
    ACT_END(F);
}
RX_FUNCTION(0x00407a70, rx_graffiti_point_add_rider)

/* the two trigger handlers' common start: the session static, the engine's profiler scope; the vehicle's driver in the point's activity;
 * a networked driver (+0xef0) whose reference is not valid (00884950) is ignored; the trigger is the linked one (its name +0xc0) */
static int point_trigger_event(rx_addr p, rx_addr vehicle, rx_addr trigger, uint32_t site) {
    const rx_addr d = point_driver(p, vehicle, site);
    if (U32(d, 0xef0) != 0) {
        uint32_t valid;
        ACT_CALLR(valid, 0, rx_net_ref_valid(U32(d, 0xef0)));
        if ((valid & 0xff) == 0) return 0;
    }
    return crt_strcmp(trigger + 0xc0, p + 0xb00) == 0;
}

/* 00407af0 "VehicleEntered" (thiscall; vehicle, trigger; RET 8): of the linked trigger, a vehicle without a record gets one (00407a70).
 * Frame: push ebx / ebp / esi / edi. */
void rx_graffiti_point_on_entered(rx_addr p, rx_addr vehicle, rx_addr trigger) {
    ACT_FRAME(F, 0, 4);
    session_static();
    const rx_addr scope = race_profile_begin();
    if (point_trigger_event(p, vehicle, trigger, 0x00407b70)) {
        rx_addr r;
        ACT_CALLR(r, 2, rx_graffiti_point_record(p, vehicle));
        if (r == 0) ACT_CALL(1, rx_graffiti_point_add_rider(vehicle, p));
    }
    race_profile_end(scope);
    ACT_END(F);
}
RX_FUNCTION(0x00407af0, rx_graffiti_point_on_entered)

/* 00407c00 "VehicleExited" (thiscall; vehicle, trigger; RET 8): of the linked trigger, the vehicle's record is no longer inside */
void rx_graffiti_point_on_exited(rx_addr p, rx_addr vehicle, rx_addr trigger) {
    ACT_FRAME(F, 0, 4);
    session_static();
    const rx_addr scope = race_profile_begin();
    if (point_trigger_event(p, vehicle, trigger, 0x00407c80)) {
        rx_addr r;
        ACT_CALLR(r, 2, rx_graffiti_point_record(p, vehicle));
        if (r != 0) SU32(r, 8, 0);
    }
    race_profile_end(scope);
    ACT_END(F);
}
RX_FUNCTION(0x00407c00, rx_graffiti_point_on_exited)

/* 00407d10 (ECX = point, ESI = record): the record's state: 0 -> 1 when the vehicle's slot 43 (0xac) is nonzero, 1 -> 0 when it is zero
 * (2, a stunt begun inside, stays); a networked driver whose reference is not valid is skipped. Under the session static and the
 * profiler scope. Frame: push ebx / edi. */
void rx_graffiti_point_record_update(rx_addr p, rx_addr r) {
    ACT_FRAME(F, 0, 2);
    const rx_addr d = point_driver(p, U32(r, 0), 0x00407d1b);
    session_static();
    const rx_addr scope = race_profile_begin();
    int skip = 0;
    if (U32(d, 0xef0) != 0) {
        uint32_t valid;
        ACT_CALLR(valid, 0, rx_net_ref_valid(U32(d, 0xef0)));
        skip = (valid & 0xff) == 0;
    }
    if (!skip) {
        if (U32(r, 4) == 0) {
            uint32_t v;
            ACT_CALLR(v, 0, act_vcall(U32(r, 0), 0xac, 0, 0));
            if (v != 0) SU32(r, 4, 1);
        } else if (U32(r, 4) == 1) {
            uint32_t v;
            ACT_CALLR(v, 0, act_vcall(U32(r, 0), 0xac, 0, 0));
            if (v == 0) SU32(r, 4, 0);
        }
    }
    race_profile_end(scope);
    ACT_END(F);
}
RX_FUNCTION(0x00407d10, rx_graffiti_point_record_update)

/* the record vector's removal (the last record moved into the removed one's place, walked from the end) */
static void records_remove_at(rx_addr p, uint32_t i) {
    const rx_addr data = U32(p, 0xb48);
    const rx_addr last = data + 0xc * (U32(p, 0xb4c) - 1), e = data + 0xc * i;
    SU32(e, 0, U32(last, 0)); SU32(e, 4, U32(last, 4));
    SU32(e, 8, U32(last, 8));
    SU32(p, 0xb4c, U32(p, 0xb4c) - 1);
}
/* 00407e00 (EAX = point; stdcall vehicle; RET 4): the vehicle's records go. Frame: push ebp / edi (+ esi). */
void rx_graffiti_point_remove_rider(rx_addr p, rx_addr vehicle) {
    for (int32_t i = I32(p, 0xb4c) - 1; i >= 0; i--)
        if (U32(U32(p, 0xb48) + 0xc * (uint32_t)i, 0) == vehicle) records_remove_at(p, (uint32_t)i);
}
RX_FUNCTION(0x00407e00, rx_graffiti_point_remove_rider)
/* 00407e60 (EAX = point): the records outside (+8 0) in state 0 go */
void rx_graffiti_point_prune(rx_addr p) {
    for (int32_t i = I32(p, 0xb4c) - 1; i >= 0; i--) {
        const rx_addr e = U32(p, 0xb48) + 0xc * (uint32_t)i;
        if (U32(e, 8) == 0 && U32(e, 4) == 0) records_remove_at(p, (uint32_t)i);
    }
}
RX_FUNCTION(0x00407e60, rx_graffiti_point_prune)

/* 00407ec0 slot 19 (thiscall; dt; RET 4): every record's state (00407d10), then the records outside in state 0 go (00407e60). Frame:
 * push ebp; mov ebp, esp; and esp, -8; push ebx / ebp / esi / edi. */
void rx_graffiti_point_vf19(rx_addr p, float dt) {
    (void)dt;
    ACT_FRAME_EBP(F, 8, 0, 4);
    for (int32_t i = 0; i < I32(p, 0xb4c); i++) ACT_CALL(0, rx_graffiti_point_record_update(p, U32(p, 0xb48) + 0xc * (uint32_t)i));
    ACT_CALL(0, rx_graffiti_point_prune(p));
    ACT_END(F);
}
RX_FUNCTION(0x00407ec0, rx_graffiti_point_vf19)

/* 0050e0a0 (EAX = driver; stdcall score; RET 4): the driver captured a point: +0xf14 points + 1, +0xf18 + 1, +0xf1c score + score, the
 * capture in progress (+0xf20 2) and its clock (+0xf24 = [0x92e330]); "GraffitiPointCaptured" raised on its activity's channel with the
 * driver (the payload in the argument's slot). Frame: push esi / edi. */
void rx_graffiti_driver_captured(rx_addr d, uint32_t score) {
    const rx_addr E = rx_sp();
    ACT_FRAME(F, 0, 2);
    SU32(d, 0xf14, U32(d, 0xf14) + 1);
    SU32(d, 0xf18, U32(d, 0xf18) + 1);
    SU32(d, 0xf1c, U32(d, 0xf1c) + score);
    SU32(d, 0xf20, 2);
    SF32(d, 0xf24, F32(0, 0x0092e330));
    const rx_addr ch = U32(U32(U32(d, 0x74), 0x94), 0x290);
    SU32(E, 4, d);
    act_raise(ch, 0x0093ee80 /* "GraffitiPointCaptured" */, 0x009cda44, E + 4, 4);
    ACT_END(F);
}
RX_FUNCTION(0x0050e0a0, rx_graffiti_driver_captured)

/* 00408090 (EAX = point; stdcall record, score; RET 8): the capture by the record's vehicle: a counted point's old holder's driver loses
 * the point and its score; the new holder (00407970); the score; a counted point's new driver gains it (0050e0a0). Frame: push ebx / esi /
 * edi. 00408110 is the same with the vehicle itself (the network handler 00564ad0's call). */
static void point_capture(rx_addr p, rx_addr vehicle, uint32_t score, uint32_t site) {
    ACT_FRAME(F, 0, 3);
    if (U32(p, 0xb5c) != 0) {
        const rx_addr d = point_driver(p, U32(p, 0xb5c), site);
        if (U32(p, 0xb44) != 0) {
            SU32(d, 0xf14, U32(d, 0xf14) - 1);
            SU32(d, 0xf1c, U32(d, 0xf1c) - U32(p, 0xb64));
        }
    }
    ACT_CALL(0, rx_graffiti_point_set_holder(p, vehicle));
    SU32(p, 0xb64, score);
    if (U32(p, 0xb44) != 0) {
        SU32(F, -4, score);                                            /* push ebx (the score, 0050e0a0's argument); push eax; call 00564970 */
        rx_set_sp(F - 4);
        const rx_addr d = point_driver(p, U32(p, 0xb5c), site);
        rx_set_sp(F);
        ACT_CALL(1, rx_graffiti_driver_captured(d, score));
    }
    ACT_END(F);
}
void rx_graffiti_point_capture(rx_addr p, rx_addr record, uint32_t score) { point_capture(p, U32(record, 0), score, 0x004080aa); }
RX_FUNCTION(0x00408090, rx_graffiti_point_capture)
void rx_graffiti_point_capture_vehicle(rx_addr p, rx_addr vehicle, uint32_t score) { point_capture(p, vehicle, score, 0x0040812a); }
RX_FUNCTION(0x00408110, rx_graffiti_point_capture_vehicle)

/* 00407f10 (EAX = point; stdcall record, score; RET 8): a score above the point's: offline the capture (00408090); ONLINE (a networked
 * session) the capture goes to the host's NetGraffiti object (006f0690 with the point's index) when the driver's network reference is
 * valid. Frame: push ebp; mov ebp, esp; and esp, -8; push ecx; push ebx / esi / edi. */
void rx_graffiti_point_score(rx_addr p, rx_addr record, int32_t score) {
    ACT_FRAME_EBP(F, 8, 4, 3);
    if (score > I32(p, 0xb64)) {
        session_static();
        if (session_networked()) act_unreached(0x00407f6e, "ONLINE: a capture sent to the host's NetGraffiti object (006f0690)");
        ACT_CALL(2, rx_graffiti_point_capture(p, record, (uint32_t)score));
    }
    ACT_END(F);
}
RX_FUNCTION(0x00407f10, rx_graffiti_point_score)

/* 00408180 "StuntFinalScore" (thiscall; player, score; RET 8): a rider with a record: the score (cvttss2si) offered (00407f10), then the
 * rider's records go (00407e00). Frame: push esi / edi. */
void rx_graffiti_point_on_score(rx_addr p, rx_addr player, float score) {
    ACT_FRAME(F, 0, 2);
    rx_addr r;
    ACT_CALLR(r, 2, rx_graffiti_point_record(p, U32(player, 0x1ac)));
    if (r != 0) {
        ACT_CALL(2, rx_graffiti_point_score(p, r, rx_cvtt((double)score)));
        ACT_CALL(1, rx_graffiti_point_remove_rider(p, U32(player, 0x1ac)));
    }
    ACT_END(F);
}
RX_FUNCTION(0x00408180, rx_graffiti_point_on_score)

/* 004081c0 "StuntBegins" (thiscall; player, two ints; RET 0xc): the rider's record, when it has one, is in state 2 (a stunt begun
 * inside) */
void rx_graffiti_point_on_begins(rx_addr p, rx_addr player, uint32_t a, uint32_t b) {
    (void)a; (void)b;
    rx_addr r;
    ACT_CALLR(r, 2, rx_graffiti_point_record(p, U32(player, 0x1ac)));
    if (r != 0) SU32(r, 4, 2);
}
RX_FUNCTION(0x004081c0, rx_graffiti_point_on_begins)

/* 004081e0 "StuntFailed" (thiscall; player; RET 4): the rider's records go (a tail jump to 00407e00 with the vehicle in the argument's
 * slot) */
void rx_graffiti_point_on_failed(rx_addr p, rx_addr player) {
    const rx_addr E = rx_sp();
    SU32(E, 4, U32(player, 0x1ac));
    rx_graffiti_point_remove_rider(p, U32(player, 0x1ac));
}
RX_FUNCTION(0x004081e0, rx_graffiti_point_on_failed)

/* 00408200 "StuntKickOut" (thiscall; player, stunt; RET 8): the rider's records go (00407e00) */
void rx_graffiti_point_on_kickout(rx_addr p, rx_addr player, rx_addr stunt) {
    (void)stunt;
    ACT_CALL(1, rx_graffiti_point_remove_rider(p, U32(player, 0x1ac)));
}
RX_FUNCTION(0x00408200, rx_graffiti_point_on_kickout)
