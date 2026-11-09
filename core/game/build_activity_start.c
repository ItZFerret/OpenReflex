/* build_activity_start.c -- construction unit 11: the activity's start (core/notes/construction.md section 16).
 * Game_EnterRealWorldAudio's 007668e0 calls every activity's vf2 ("PostEventOverlay"): MXActivityRace::vf2 00564b20 ->
 * MXActivityRaceStub::vf2 00566170 -> Activity::vf2 00556ec0: the activity's event channel (a child of the world's root activity
 * channel) and its ~45 event types, the race states' names ("Pre" .. "Inactive", 005585e0: "<state> Entered / Exited"), the
 * activity's handlers on the session / vehicle / UI channels, the class's own event types (vf4), the audio logic's start (vf29),
 * the start board and the world's typed entity list (00767ba0); then the race's handlers (lap completed, stunts), the OUT_ markers,
 * "Activity Init" and the mode's start grid (005666d0 / 00564c90 / 00564bc0).
 * The event-system helpers (act_event_type, act_subscribe, act_raise) are shared by the unit's files (build_activity_int.h). */
#include "career_activity.h"
#include "build_activity_int.h"
#include "gateless_race_start.h"
#include "build_rider.h"
#include "world_misc.h"
#include "world_veh.h"
#include "anim.h"
#include "build_vehicle_tree.h"
#include "loop_platform.h"
#include "race_activity_misc.h"
#include "race_activity.h"
#include "race_activity_stub.h"
#include "practice.h"   /* PORT ADDITION: PRACTICE */
#include <string.h>
#include "rider_cap.h"

/* ======================================================================================================================== */
/* the event system: event types, handler subscriptions, raisers                                                             */

typedef struct { uint32_t fn, name, td; int aligned; int32_t sp; } act_event_inst;
static const act_event_inst k_event_types[] = {
#include "build_activity_events.inc"
};

void act_event_type(rx_addr fn, rx_addr ch, rx_addr name) {
#ifdef RX_ORACLE   /* diagnostics: RX_B11_ORIG_TYPES=1 runs the original instantiations (bisecting a Verify difference) */
    static int orig = -1;
    if (orig < 0) orig = getenv("RX_B11_ORIG_TYPES") != NULL;
    if (orig) {
        rx_regs in, out; memset(&in, 0, sizeof in); in.r[0] = ch;
        for (size_t i = 0; i < sizeof k_event_types / sizeof k_event_types[0]; i++)
            if (k_event_types[i].fn == fn) { rx_call_original_ex(fn, &in, &out, k_event_types[i].name ? 0 : 1, &name); return; }
    }
#endif
    for (size_t i = 0; i < sizeof k_event_types / sizeof k_event_types[0]; i++) {
        const act_event_inst* t = &k_event_types[i];
        if (t->fn != fn) continue;
        if (t->name) ACT_CALL(0, rx_event_type_register_at(ch, t->name, t->td, t->aligned, t->sp));
        else ACT_CALL(1, rx_event_type_register_at(ch, name, t->td, t->aligned, t->sp));
        return;
    }
    act_unreached(fn, "an event-type instantiation not in build_activity_events.inc");
}

void act_subscribe(rx_addr tmpl, uint32_t conv, int nargs, uint32_t pushes, rx_addr ch, rx_addr name, rx_addr vtable, rx_addr td, rx_addr obj, rx_addr method) {
    const rx_addr entry = rx_sp();   /* the template's entry ESP (its return address) */
#ifdef RX_ORACLE   /* diagnostics: RX_B11_ORIG_SUBS=1 runs the original templates (bisecting a Verify difference) */
    static int orig = -1;
    if (orig < 0) orig = getenv("RX_B11_ORIG_SUBS") != NULL;
    if (orig) {
        rx_regs in, out; memset(&in, 0, sizeof in);
        if (conv & 1) in.r[0] = ch;
        if (conv & 2) in.r[1] = ch;
        if (conv & 4) in.r[0] = name;
        if (conv & 8) in.r[1] = name;
        const uint32_t a[2] = {obj, method};
        rx_set_sp(entry + 4 + 4u * (uint32_t)nargs);
        rx_call_original_ex(tmpl, &in, &out, nargs, a);
        rx_set_sp(entry);
        return;
    }
#else
    (void)tmpl; (void)conv; (void)nargs;
#endif
    rx_set_sp(entry - 4 * pushes);
    rx_addr ev;
    ACT_CALLR(ev, 2, rx_world_registry_find(ch, name));                 /* 00593220 */
    rx_addr h;
    ACT_CALLR(h, 1, rx_reflex_aligned_alloc(0x14));
    if (h) {
        SU32(h, 4, 0); SU32(h, 0, vtable); SU32(h, 0xc, obj); SU32(h, 0x10, method);
        rx_addr nm;
#ifdef RX_ORACLE
        if (getenv("RX_B11_LOG") && !U32(td, 4)) fprintf(stderr, "b11 first name %08x at %08x (subscribe %08x)\n", td, rx_sp(), tmpl);
#endif
        nm = crt_type_info_name(td, 0x009e1a7c);                       /* typeid(signature).name() (from the ESP before "push node") */
        SU32(h, 8, nm);
    }
    if (crt_strcmp(U32(ev, 4), U32(h, 8)) == 0) {                      /* (vf1 008ad240 returns +8) */
        SU32(entry, 4, h);                                             /* the proxy kept in the first argument's slot */
        loop_enter_cs(ev + 0x24);
        ACT_CALL(1, rx_event_handler_list_push_back(ev + 8, h));       /* 005b9ff0 (EAX = &proxy) */
        loop_leave_cs(ev + 0x24);
    }
    rx_set_sp(entry);
}

void act_raise(rx_addr manager, rx_addr name, rx_addr td, rx_addr payload, uint32_t size) {
#ifdef RX_ORACLE
    if (getenv("RX_B11_LOG")) fprintf(stderr, "b11 raise %08x td %08x named %08x\n", name, td, U32(td, 4));
#endif
    crt_type_info_name(td, 0x009e1a7c);
    ACT_CALL(2, rx_world_event_dispatch(manager, name, payload, size)); /* 004020c0 (EAX = manager, EDI = name) */
}

/* FUN_0078fd60 with one variadic argument, pushed as the original's cdecl call */
static void act_format1(rx_addr buf, uint32_t size, rx_addr fmt, uint32_t arg) {
    const rx_addr sp = rx_sp();
    SU32(sp, -8, fmt); SU32(sp, -4, arg);
    rx_set_sp(sp - 12);
    rx_format_buffer(buf, size, fmt, sp - 4);
    rx_set_sp(sp);
}

/* the session's static initialiser (006de1f0 under bit 0 of 0x00dcf304) has run at startup */
static void session_static(void) {
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
}
static void series_static(void) {
    if (!(U8(0, 0x009e275c) & 1)) act_unreached(0x005b2370, "the series manager's static initialiser (MXSeriesManager_Construct)");
}

/* ======================================================================================================================== */
/* the state names (std::map<int, std::string> at +0x240)                                                                    */

/* 005585e0 (stdcall activity, state, std::string name by value; RET 0x24): the state's name in +0x240 (operator[] =), the event
 * types "<name> Entered" / "<name> Exited" (005911a0: void(Activity*, int)) on the activity's channel, the string destroyed */
void rx_activity_name_state(rx_addr A, uint32_t state, rx_addr name) {
    const rx_addr entry = rx_sp();
    ACT_FRAME(L, 0x114, 3);
    SU32(entry, -4, U32(0, 0x009b45e4) ^ (entry - 0x114));             /* the /GS cookie */
    (void)state;
    rx_addr slot;
    ACT_CALLR(slot, 3, rx_activity_state_name(A + 0x240, entry + 8));      /* 00585860 (stdcall map, &state; the third pushed word is the name for operator=) */
    crt_str_assign_sub(slot, name, 0, 0xffffffffu);                     /* operator=(const string&) */
    const rx_addr buf = L + 0x10;
    const rx_addr cs = U32(name, 0x18) < 0x10 ? name + 4 : U32(name, 4);
    act_format1(buf, 0x104, 0x00941db4 /* "%s Entered" */, cs);
    act_event_type(0x005911a0, U32(A, 0x290), buf);
    const rx_addr cs2 = U32(name, 0x18) < 0x10 ? name + 4 : U32(name, 4);
    act_format1(buf, 0x104, 0x00941dc0 /* "%s Exited" */, cs2);
    act_event_type(0x005911a0, U32(A, 0x290), buf);
    crt_str_dtor(name);
    ACT_END(L);
}
RX_FUNCTION(0x005585e0, rx_activity_name_state)

/* ======================================================================================================================== */
/* Activity::vf2 00556ec0                                                                                                    */

/* the inlined EventSystem::Channel constructor with a parent (0x5c bytes; 005930a0's body inline: vtable 0x00946250, the event map,
 * the critical section, the parent; then vtable 0x00946258 and the std::list +0x40 / +0x54 / +0x58) */
static rx_addr channel_new_child(rx_addr parent) {
    rx_addr ch;
    ACT_CALLR(ch, 1, rx_reflex_aligned_alloc(0x5c));
    if (!ch) return 0;
    ACT_CALL(1, rx_event_base_channel_construct(ch, parent));
    SU32(ch, 0, 0x00946258);
    rx_addr p;
    ACT_CALLR(p, 1, rx_reflex_aligned_alloc(4));
    if (p) SU32(p, 0, ch + 0x40);
    SU32(ch, 0x40, p);
    rx_addr h;
    ACT_CALLR(h, 0, rx_event_list_head_alloc());
    SU32(ch, 0x54, h);
    SU32(ch, 0x58, 0);
    return ch;
}

/* the event types of 00556ec0, in its order (instantiation, the pushed name or 0 for a fixed one) */
static const uint32_t k_activity_types[][2] = {
    {0x00590c00, 0x00941b94 /* "Start" */}, {0x00590c00, 0x00941b9c /* "End" */}, {0x00590c00, 0x00941ba0 /* "Restart" */},
    {0x00590d20, 0}, {0x00590e40, 0},
    {0x00590f60, 0x00941bc4 /* "Flooding" */}, {0x00591080, 0x00941bd0 /* "PlayerReset" */},
    {0x005911a0, 0x00941bdc /* "Countdown Step" */}, {0x005911a0, 0x00941bec /* "FinishCountdown Step" */},
    {0x005912c0, 0x0093ee38 /* "VehicleWrecked" */}, {0x005912c0, 0x0093ee48 /* "VehicleStopWreck" */},
    {0x005913e0, 0x00941c04 /* "Post Scenarios Start" */}, {0x005913e0, 0x00941c1c /* "Post Scenarios End" */},
    {0x00590c00, 0x00941c30 /* "Show Activity Results" */}, {0x00590c00, 0x00941c48 /* "Post State Complete" */},
    {0x005912c0, 0x00941278 /* "WreckRecoveryBegin" */}, {0x005912c0, 0x00941260 /* "WreckRecoverySuccess" */},
    {0x005912c0, 0x009416fc /* "WreckRecoveryFailed" */}, {0x005912c0, 0x009416e8 /* "WreckRecoveryReset" */},
    {0x005912c0, 0x00941c5c /* "VehicleGettingLapped" */},
    {0x00591500, 0x00941c74 /* "TwoLapsLeft" */}, {0x00591500, 0x00941c80 /* "OneLapLeft" */},
    {0x00590c00, 0x00941c8c /* "FreestyleEnd" */}, {0x00591500, 0x00941c9c /* "EndOfRace" */},
    {0x00591620, 0}, {0x00591740, 0x00941cb4 /* "UIUpdateScore" */}, {0x00590f60, 0x0093ee80 /* "GraffitiPointCaptured" */},
    {0x00591740, 0x0093ee5c /* "Reset Hint" */}, {0x00591860, 0}, {0x00591980, 0x00942898 /* "HitDiscoveryFound" */},
    {0x00591aa0, 0}, {0x00591bc0, 0}, {0x00591ce0, 0},
    {0x00591500, 0x0093ee68 /* "Ghost Session Completed" */}, {0x00590c00, 0x00941cc4 /* "Reset Ghost Overlays" */},
    {0x00591e00, 0x00941cdc /* "Ghost Was Beat" */},
};
/* the state names of 00556ec0 ("Pre" .. "Inactive") */
static const uint32_t k_state_strings[6] = {0x00941cec, 0x00941cf0, 0x009419e8, 0x00941cfc, 0x00941d08, 0x00941d10};

/* Activity::vf2 00556ec0 (thiscall activity; arg ("PostEventOverlay"); RET 4) */
void rx_activity_vf2(rx_addr A, rx_addr arg) {
    (void)arg;
    ACT_FRAME(L, 8, 4);
    const rx_addr core = U32(0, 0x00a95844);
    if (U32(U32(0, 0x00a946b8), 0x15b4c) != 0 && U32(A, 0x238) != 0) SU32(A, 0xfe4, 1);
    ACT_CALL(0, rx_engine_frame_mark(U32(core, 0x10)));               /* 0084ea00 (EDI = the engine's frame object) */
    for (uint32_t i = 0; i < 0x1c; i++) {   /* the 28 start slots: the default position / direction, no vehicle */
        const rx_addr s = A + 0x340 + 0x30 * i;
        SU32(s, 0, U32(0, 0x00a95d50)); SU32(s, 4, U32(0, 0x00a95d54)); SU32(s, 8, U32(0, 0x00a95d58));
        SU32(s, 0x10, U32(0, 0x009cb4a0)); SU32(s, 0x14, U32(0, 0x009cb4a4)); SU32(s, 0x18, U32(0, 0x009cb4a8));
        SU32(s, 0x20, 0); SU32(s, 0x24, 0); SU32(s, 0x28, 0);
    }
    for (uint32_t o = 0x304; o < 0x334; o += 4) SU32(A, o, 0);         /* the race indices taken */
    rx_raceidx_clear_side(A);                                          /* (w-rc-core) indices 12..31 in side storage */
    if (!(U32(0, 0x00dce780) & 1)) ACT_CALL(0, rx_event_channel_registry());   /* (inlined: 00593900's static initialiser) */
    const rx_addr ch = channel_new_child(U32(U32(A, 0x234), 0x21db8));
    ACT_CALL(2, rx_event_channel_register(0x00dcbeb4, A + 0x14, ch));   /* 00593520 (EAX = the registry) */
    SU32(A, 0x290, ch);
    for (size_t i = 0; i < sizeof k_activity_types / sizeof k_activity_types[0]; i++)
        act_event_type(k_activity_types[i][0], U32(A, 0x290), k_activity_types[i][1]);
    /* 00586dc0 (EAX = the channel; activity): "Reset Ghost Overlays" -> 006137b0 */
    ACT_CALL(1, act_subscribe(0x00586dc0, 1, 1, 4, U32(A, 0x290), 0x00941cc4, 0x00945844, 0x009cbd24, A, 0x006137b0));
    for (uint32_t k = 0; k < 6; k++) {   /* 005585e0 (activity, state, std::string by value) */
        const rx_addr sp = rx_sp(), s = sp - 0x1c;
        rx_set_sp(s);
        crt_str_ctor_cstr(s, k_state_strings[k]);
        SU32(s, -4, k); SU32(s, -8, A);                                 /* push state; push activity */
        rx_set_sp(s - 8 - 4);
#ifdef RX_ORACLE   /* diagnostics: RX_B11_ORIG_NAMES=1 runs the original 005585e0 */
        if (getenv("RX_B11_ORIG_NAMES")) { const uint32_t a[2] = {A, k}; rx_set_sp(s); rx_call_original(0x005585e0, RX_STDCALL, 0, 2, a); rx_set_sp(sp); continue; }
#endif
        rx_activity_name_state(A, k, s);
        rx_set_sp(sp);
    }
    if (U32(A, 0x238) != 4) {   /* the session channel [0x00da6ca0]: 00586e80 / 00586f40 / 00587000 (EAX = channel; activity) */
        session_static();
        ACT_CALL(1, act_subscribe(0x00586e80, 1, 1, 4, U32(0, 0x00da6ca0), 0x00941d1c /* "NetworkTimerEvents" */, 0x00945864, 0x009cf81c, A, 0x0055cda0));
        session_static();
        ACT_CALL(1, act_subscribe(0x00586f40, 1, 1, 4, U32(0, 0x00da6ca0), 0x00941d30 /* "NetworkSessionState" */, 0x00945884, 0x009cf8a8, A, 0x0055cdf0));
        session_static();
        ACT_CALL(1, act_subscribe(0x00587000, 1, 1, 4, U32(0, 0x00da6ca0), 0x00941d44 /* "NetworkPositionListUpdate" */, 0x009458a4, 0x009cf940, A, 0x0055ce10));
    }
    session_static();
    ACT_CALL(1, act_subscribe(0x005870c0, 1, 1, 4, U32(0, 0x00da6ca0), 0x00941d60 /* "NetworkPlayerModeChanged" */, 0x009458c4, 0x009cf9a4, A, 0x0055c970));
    session_static();
    if (U32(U32(0, 0x00da6cb0), 0x1c4c) != 0) act_unreached(0x005574a3, "an online activity's session handlers (006def80)");
    {   /* 007557c0 (ECX = the channel, EAX = name; records object, method; RET 8): MXRecords' "Start" / "End" handlers */
        const rx_addr rec = U32(0, 0x00d92f10);
        ACT_CALL(2, act_subscribe(0x007557c0, 6, 2, 3, U32(A, 0x290), 0x00941b94 /* "Start" */, 0x0096d4b0, 0x009cbd24, rec, 0x00753760));
        ACT_CALL(2, act_subscribe(0x007557c0, 6, 2, 3, U32(A, 0x290), 0x00941b9c /* "End" */, 0x0096d4b0, 0x009cbd24, rec, 0x007538c0));
    }
    act_vcall(A, 0x10, 0, 0);                                          /* vf4: the class's own event types */
    if (U32(A, 0xfd8) != 0) act_vcall(U32(A, 0xfd8), 0x74, 1, (const uint32_t[]){A});   /* the audio logic's vf29 */
    if (U32(A, 0x238) != 0) {   /* the start board (type 0x103a, at most 2) and the world's typed entity list */
        SU32(A, 0x2cc, 0);
        SU32(L, 0x10, 0); SU32(L, 0x14, 0);
        const rx_addr scene = U32(U32(A, 0x234), 0x20138);
        uint32_t n_;
        {
            const rx_addr sp_ = rx_sp(); rx_set_sp(sp_ - 0x14);
            n_ = rx_scene_manager_collect_typed(scene, 0x103a, L + 0x10, 2, 0);   /* 007d2920 (EAX = scene) */
            rx_set_sp(sp_);
        }
        (void)n_;
        SU32(A, 0x2cc, U32(L, 0x10));
        ACT_CALL(0, rx_world_link_lights(U32(A, 0x234)));                 /* 00767ba0 (ESI = world) */
    }
    /* 00587180 (activity): "VehicleEntered" on the vehicle channel [0x00a89bc8] -> 0055e8b0 (race_activity.c
     * rx_activity_vehicle_entered: a "SCR_<sequence>" trigger starts that scripted sequence, any other goes to vf59) */
    ACT_CALL(1, act_subscribe(0x00587180, 0, 1, 4, U32(0, 0x00a89bc8), 0x00935e84, 0x009458e4, 0x009cbb7c, A, 0x0055e8b0));
    /* 00587240 (EAX = the UI manager's channel [0x00a93fcc] + 0x98; activity): "PageWipeTransition" -> 0055e9e0 */
    /* PRODUCT BOUNDARY: without the UI manager ([0x00a93fcc] = 0: the product has no UI) there is no UI event channel (+0x98) and nothing raises
     * "PageWipeTransition": the subscription is not made (the oracle always has the UI manager) */
    if (U32(0, 0x00a93fcc) != 0)
        ACT_CALL(1, act_subscribe(0x00587240, 1, 1, 4, U32(U32(0, 0x00a93fcc), 0x98), 0x00941d7c, 0x00945904, 0x009cdb68, A, 0x0055e9e0));
    SU32(A, 0xec4, 1);
    ACT_END(L);
}
RX_FUNCTION(0x00556ec0, rx_activity_vf2)

/* 00767ba0 (ESI = world): the scene's entities of the types 0x1046 .. 0x1049 (at most 0x28 each, 007d2920) appended in that order
 * to the world's doubly linked list (+0x21dd8 first, +0x21ddc last; entity +0x310 next, +0x314 prev) */
void rx_world_link_lights(rx_addr w) {
    ACT_FRAME(L, 0x288, 3);
    static const uint32_t types[4] = {0x1046, 0x1047, 0x1048, 0x1049};
    static const uint32_t outs[4] = {0x14, 0x154, 0xb4, 0x1f4};   /* the four 0xa0-byte arrays in the frame */
    int32_t n[4];
    for (int k = 0; k < 4; k++) {
        const rx_addr b = L + outs[k];
        for (uint32_t i = 0; i < 0x9c; i++) SU8(b + 4, i, 0);
        SU32(b, 0, 0);
        const rx_addr sp_ = rx_sp(); rx_set_sp(sp_ - 0x14);
        n[k] = (int32_t)rx_scene_manager_collect_typed(U32(w, 0x20138), types[k], b, 0x28, 0);
        rx_set_sp(sp_);
    }
    for (int k = 0; k < 4; k++) {
        const rx_addr b = L + outs[k];
        for (int32_t i = 0; i < n[k]; i++) {
            const rx_addr e = U32(b, 4u * (uint32_t)i);
            if (!e) continue;
            if (U32(w, 0x21ddc) == 0) { SU32(w, 0x21dd8, e); SU32(w, 0x21ddc, e); SU32(e, 0x314, 0); }
            else { SU32(U32(w, 0x21ddc), 0x310, e); SU32(e, 0x314, U32(w, 0x21ddc)); SU32(w, 0x21ddc, e); }
            SU32(e, 0x310, 0);
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x00767ba0, rx_world_link_lights)

/* ======================================================================================================================== */
/* MXActivityRaceStub / MXActivityRace                                                                                       */

/* MXActivityRaceStub::vf4 00566310 (ECX = activity): the race's event types on its channel */
void rx_race_stub_vf4(rx_addr A) {
    ACT_FRAME(L, 0, 1);
    act_event_type(0x00592040, U32(A, 0x290), 0x0093eefc /* "Lap Completed" */);
    act_event_type(0x00591e00, U32(A, 0x290), 0x0093ef48 /* "Player Eliminated" */);
    act_event_type(0x00591740, U32(A, 0x290), 0x0093ef24 /* "Off Track" */);
    act_event_type(0x00591740, U32(A, 0x290), 0x0093ef3c /* "Backwards" */);
    act_event_type(0x00592160, U32(A, 0x290), 0);
    act_event_type(0x00592280, U32(A, 0x290), 0);
    act_event_type(0x00590f60, U32(A, 0x290), 0x0093ef30 /* "Show Arrow" */);
    act_event_type(0x00590f60, U32(A, 0x290), 0x0093eef0 /* "Hide Arrow" */);
    act_event_type(0x00591500, U32(A, 0x290), 0x00942044 /* "Holeshot" */);
    const rx_addr ch = U32(A, 0x290);
    ACT_END(L);
    {   /* a tail jump: the instantiation enters with this function's return address */
        const rx_addr sp = rx_sp();
        rx_set_sp(sp + 4);
        act_event_type(0x005923a0, ch, 0);
        rx_set_sp(sp);
    }
}
RX_FUNCTION(0x00566310, rx_race_stub_vf4)

/* 007d29e0 (EAX = scene; stdcall prefix, out, capacity, overflow*; RET 0x10): the entities of the enabled buckets whose name
 * (+0x18) starts with the prefix (strncmp over the prefix's length), up to the capacity (then *overflow = 1) */
uint32_t rx_scene_collect_prefixed(rx_addr s, rx_addr prefix, rx_addr out, int32_t cap, rx_addr overflow) {
    ACT_FRAME(L, 8, 4);
    loop_enter_cs(s + 4);
    if (overflow) SU32(overflow, 0, 0);
    const uint32_t len = crt_strlen(prefix);
    int32_t n = 0;
    int full = 0;
    for (uint32_t b = 0; b < 0x18 && !full; b++) {
        const rx_addr bucket = s + 0x140 + 0x110 * b;
        if (U32(bucket, 4) == 0) continue;
        for (int32_t i = 0; i < I32(bucket, 0); i++) {
            if (n >= cap) { full = 1; break; }
            const rx_addr nm = U32(U32(U32(bucket, -4), 4u * (uint32_t)i), 0x18);
            if (nm && U8(nm, 0) == U8(prefix, 0) && crt_strncmp(prefix, nm, len) == 0)
                SU32(out, 4u * (uint32_t)n++, U32(U32(bucket, -4), 4u * (uint32_t)i));
        }
        if (n >= cap) full = 1;
    }
    if (full && overflow) SU32(overflow, 0, 1);
    loop_leave_cs(s + 4);
    ACT_END(L);
    return (uint32_t)n;
}
RX_FUNCTION(0x007d29e0, rx_scene_collect_prefixed)

/* 00566260 (EAX = activity; stdcall on; RET 4): the starting-light entities (type 0x107b, at most 50: +0xff08c = their count / 2)
 * and the scene's "OUT_" entities: +0x70 and +0xa2 = on */
static void race_out_markers(rx_addr A, uint32_t on) {
    ACT_FRAME(L, 0xcc, 2);
    const rx_addr arr = L + 0xc, ov = L + 8;
    for (uint32_t i = 0; i < 0xc8; i++) SU8(arr, i, 0);
    SU32(ov, 0, 0);
    int32_t n;
    {
        const rx_addr sp_ = rx_sp(); rx_set_sp(sp_ - 0x14);
        n = (int32_t)rx_scene_manager_collect_typed(U32(U32(A, 0x234), 0x20138), 0x107b, arr, 0x32, ov);
        rx_set_sp(sp_);
    }
    SU32(A, 0xff08c, (uint32_t)(n / 2));
    ACT_CALLR(n, 4, (int32_t)rx_scene_collect_prefixed(U32(U32(A, 0x234), 0x20138), 0x0094203c /* "OUT_" */, arr, 0x32, ov));
    for (int32_t i = 0; i < n; i++) {
        const rx_addr e = U32(arr, 4u * (uint32_t)i);
        SU32(e, 0x70, on);
        SU8(e, 0xa2, (uint8_t)on);
    }
    ACT_END(L);
}

/* MXActivityRaceStub::vf2 00566170 (thiscall activity; arg; RET 4) */
void rx_race_stub_vf2(rx_addr A, rx_addr arg) {
    const rx_addr entry = rx_sp();
    ACT_FRAME(L, 0, 2);
    ACT_CALL(1, rx_activity_vf2(A, arg));
    if (U32(A, 0x23c) >= 0x1f) { series_static(); ACT_CALL(1, (void)rx_career_current_event_type(0x009e2188)); }
    /* 00588900 (EAX = the channel; activity, method; RET 8): "Lap Completed" -> 00567660 (mode 5) / 005676e0 */
    ACT_CALL(2, act_subscribe(0x00588900, 1, 2, 2, U32(A, 0x290), 0x0093eefc, 0x00945a04, 0x009cda64, A, U32(A, 0x238) == 5 ? 0x00567660 : 0x005676e0));
    uint32_t on = 0;
    if (U32(A, 0x238) == 8) {   /* 005889c0 (activity): "EnteredTimedSection" ([0x009c617c]) on the vehicle channel -> 00569550 */
        ACT_CALL(1, act_subscribe(0x005889c0, 0, 1, 2, U32(0, 0x00a89bc8), U32(0, 0x009c617c), 0x00945a24, 0x009cbc38, A, 0x00569550));
        on = 1;
    }
    ACT_CALL(1, race_out_markers(A, on));
    if (U32(A, 0x238) != 0) {   /* "Activity Init" (void(Activity*)) on the activity manager channel [0x00a93f74]: the payload in the
                                 * argument's slot */
        ACT_FRAME(R, 0, 1);
        SU32(entry, 4, A);
        act_raise(U32(0, 0x00a93f74), 0x00935ea4, 0x009cbd24, entry + 4, 4);
        ACT_END(R);
    }
    /* PORT ADDITION (game/practice.h): PRACTICE takes the original's Practice rule for an exhibition lap race (one host static when off) */
    if (U32(A, 0x238) == 2 || rx_practice_applies(A)) SU32(A, 0xff070, 0x7fffffff);
    SU32(A, 0xff084, 0);
    {   /* 00566da0 (ESI = activity, EDI = 1): the section / gate types' start message */
        uint32_t t = U32(A, 0x23c);
        if (t == 0xc) ACT_CALLR(t, 1, rx_race_track_kind(U32(A, 0x234) + 0x20030));   /* the section race: its level's track kind (0056a120) */
        const uint32_t m = U32(A, 0x238);
        const int listed = t == 2 || t == 3 || t == 0x11 || t == 0x12 || t == 0x2e || t == 0x2f || t == 0x32 || t == 0x34 || t == 0x35;
        if ((listed || t == 0x13) && m != 0 && m != 2 && U32(A, 0x884) == 0) {
            ACT_CALL(4, rx_gateless_race_start(A, A + 0x8a0, t == 0x13 ? 12 : 5, 1));
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x00566170, rx_race_stub_vf2)

/* 00564bc0 (EAX = activity): a free start: no time limit (+0x13e650 = FLT_MAX), the start state (+0x228 0, +0x224 1), every start
 * slot's vehicle reference (+0x360 + 0x30 i) = +0x8f8 */
static void race_free_start(rx_addr A) {
    SU32(A, 0x13e650, U32(0, 0x00977494));
    SU32(A, 0x228, 0);
    SU32(A, 0x224, 1);
    for (uint32_t i = 0; i < 0x1c; i++) SU32(A, 0x360 + 0x30 * i, U32(A, 0x8f8));
}

void rx_race_grid_from_gate(rx_addr A);   /* 005666d0 (build_activity_grid.c) */
void rx_race_grid_from_track(rx_addr A);  /* 00564c90 (build_activity_grid.c) */

/* MXActivityRace::vf2 00564b20 (thiscall activity; arg; RET 4): the stub's start, the stunt handlers ("StuntBegin" 00565770 /
 * "StuntFailed" 005657d0 / "PhysicsStuntBegin" 00565710 on the stunts channel [0x00a94304]), the mode's start grid */
void rx_race_vf2(rx_addr A, rx_addr arg) {
    ACT_FRAME(L, 0, 1);
    ACT_CALL(1, rx_race_stub_vf2(A, arg));
    /* 00588130 (ECX = name; activity, method; RET 8) */
    ACT_CALL(2, act_subscribe(0x00588130, 8, 2, 2, U32(0, 0x00a94304), 0x0095ba28 /* "StuntBegin" */, 0x009459c4, 0x009cbf38, A, 0x00565770));
    ACT_CALL(2, act_subscribe(0x00588130, 8, 2, 2, U32(0, 0x00a94304), 0x00960764 /* "StuntFailed" */, 0x009459c4, 0x009cbf38, A, 0x005657d0));
    /* 005881f0 (activity; RET 4) */
    ACT_CALL(1, act_subscribe(0x005881f0, 0, 1, 2, U32(0, 0x00a94304), 0x009607a0 /* "PhysicsStuntBegin" */, 0x009459e4, 0x009ce768, A, 0x00565710));
    switch (U32(A, 0x238)) {
    case 3: race_free_start(A); break;                                  /* 00564bc0 (EAX = activity) */
    case 1: case 5: case 6: case 7: case 8:
        if (U32(A, 0x238) == 1) SU32(A, 0x224, 0);
        ACT_CALL(1, rx_race_grid_from_gate(A));                         /* 005666d0 (stdcall activity) */
        break;
    case 0: case 2:
        SU32(A, 0x224, 0);
        ACT_CALL(0, rx_race_grid_from_track(A));                        /* 00564c90 (EDI = activity) */
        break;
    default: break;
    }
#ifdef RX_ORACLE
    if (getenv("RX_B11_LOG")) fprintf(stderr, "b11 race vf2 end: 785bc0fc = %08x\n", U32(0, 0x785bc0fc));
#endif
    ACT_END(L);
}
RX_FUNCTION(0x00564b20, rx_race_vf2)
