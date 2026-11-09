/* race_egghunt.c -- MXActivityEggHunt (the EggHunt party mode, activity type 0xf; "Greed" in the data: the area subscene
 * "<level>MiniGameGreed", the HUD layer SinglePlayerGreed; vtable 0x00944e24; 0x100130 bytes), its GreedObjectModel eggs (vtable
 * 0x00944d6c), EggHuntDriver (vtable 0x0093ff24, 0xf20 bytes) and EggHuntActivityAudioLogic (vtable 0x0094315c), hand-transliterated
 * from the original (the address of each function is in its comment; frames and the stack depth of each call are the original's).
 *
 * The game: the activity owns ten egg models (GreedObjectModel, a ModelEntity cloning the model of the area's "GreedModel" entity).
 * At each round's start the riders are placed on a circle of 24 around the round's start zone (the area's StartEntitySnakeTag entities,
 * type 0x1017; the zone of the activity row's MiniGameStartZone, 0 without a row) and the ten eggs are spread out from the next zone
 * (00575440: along the vector [0x9cb4a0] scaled by i * (the zone's SnakeAreaRadius / 10), turned by i * [0x9b7338] (36) degrees about
 * the vertical, on the terrain + 2), spinning about the vertical (2.0944 rad/s). A rider within sqrt(10) ft of an egg collects it
 * (00574660: +0xf1c of its driver + 1, the egg hidden); 30 s after the first collection all ten respawn at the next zone. The round
 * ends by the clock (vf42: the elapsed time reaches the activity's duration); the original never decides a winner (+0x10011c stays 0:
 * every round's message is STR_TIE and the match never ends), 3 s of WindDown restart the next round (vf27 / vf16).
 *
 * Content: no installed level has a "GreedModel" entity in its MiniGameGreed subscene (the original faults at 0057492d on every level:
 * QUEUE "## w-partyresults" item 4), so the session refuses an EggHunt event before it loads (rx_minigame_content_missing, api); vf2
 * keeps the original's lookup and stops (act_unreached, STOP_AUDIT R125) where the original would fault.
 *
 * ONLINE (a Quazal session: [0x00da6cb0] + 0x1c4c set and the mode +0x1b0 not 1; none exists in the product): the network egg hunt
 * object (+0x100128: NetEggHunt, the eggs' collections sent to the host 006f4c50, "NetEggHuntEggPicked" 00575f20 / "NetworkObjectRemoved"
 * 00575f00 handlers: not ported, as the other party modes' network handlers), the session's start zone (006f7510). Each networked branch
 * is marked "ONLINE" and stops with act_unreached (STOP_AUDIT R125). */
#include "race_minigames.h"
#include "race_snake.h"
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
#include "build_scene.h"
#include "build_scene_entity.h"
#include "build_alloc.h"
#include "waypoint_activity.h"
#include "waypoint_driver.h"
#include "loop_vehicle.h"
#include "loop_platform.h"
#include "phys_terrain.h"
#include "phys_crt.h"
#include "anim.h"
#include "fx.h"
#include "race_beh1_extern.h"
#include "race_tag.h"
#include "activity_cues.h"
#include "world_misc.h"
#include "world_wreck.h"
#include "crt.h"
#include "../src/rx_os.h"
#include <string.h>
#include "rider_cap.h"

#define VT_EGGHUNT          0x00944e24u
#define VT_EGGHUNT_AUDIO    0x0094315cu
#define VT_EGGHUNT_DRIVER   0x0093ff24u
#define VT_GREED_MODEL      0x00944d6cu
#define VT_GREED_MODEL_O90  0x00944de4u
#define VT_AUDIO_LOGIC      0x009429fcu
#define TD_ACTIVITY         0x009cbd24u   /* void(Activity*) */
#define TD_STRING           0x009cdb68u   /* void(const char*) */
#define TD_ROUND_STARTED    0x009cf680u   /* void(Activity*, int, int, int) */
#define STR_GREED_MODEL     0x00942338u   /* "GreedModel" */
#define STR_GREED_OBJECT    0x00942344u   /* "GreedObject" */
#define STR_EGG_COLLECTED   0x00942364u   /* "EggCollected" */
#define STR_EGG_SPAWNED     0x00942374u   /* "EggSpawned" */
#define STR_ROUND_STARTED   0x009421b0u
#define STR_ROUND_OVER      0x009421c0u
#define STR_MATCH_OVER      0x009421ccu
#define STR_PLAYER_WINS     0x00941ca8u
#define STR_ACTIVITY_INIT   0x00935ea4u
#define EGG(A, i)           ((A) + 0xff0d0u + 0x1a0u * (uint32_t)(i))

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
static inline void os_cs(uint32_t slot, rx_addr cs) { const uint32_t a[1] = {cs}; (void)rx_os_import(slot, 1, a); }
/* a tail jump to a virtual with one stack argument (the jump keeps the caller's return address: the callee runs at the entry ESP) */
static void tail_vcall1(rx_addr obj, uint32_t slot, uint32_t a0) {
    const rx_addr fn = U32(U32(obj, 0), slot);
    void (*f)(rx_addr, uint32_t) = RX_FN(fn, void (*)(rx_addr, uint32_t));
    if (f) { f(obj, a0); return; }
    const rx_addr sp = rx_sp();
    rx_set_sp(sp + 8);
    rx_call_original(fn, RX_THISCALL, obj, 1, &a0);
    rx_set_sp(sp);
}

/* ======================================================================================================================== */
/* construction / destruction                                                                                                */

/* 00574730 (stack act; ECX = type, EAX = world; RET 4) -> act: Activity's constructor, the ten eggs (the vector constructor iterator
 * with 0055ec30), the class's fields (the rounds to win 4, the respawn clock 30, a new match, the eggs' zone position from the globals
 * [0xa95d50] ..), a new EggHuntActivityAudioLogic (0x98 bytes) at +0xfd8. Frame: push ecx / ebx / esi / edi. */
rx_addr rx_egghunt_construct(rx_addr act, uint32_t type, rx_addr world) {
    ACT_FRAME(F, 0, 4);
    ACT_CALL(1, rx_activity_construct(act, type, world));
    SU32(act, 0, VT_EGGHUNT);
    ACT_CALL(4, rx_fx_vector_ctor_iterator(EGG(act, 0), 0x1a0, 10, 0x0055ec30));
    SU32(act, 0x100110, 0);
    SU32(act, 0x100114, 4);
    SU32(act, 0x100118, 0);
    SU32(act, 0x100128, 0);
    SU32(act, 0xff0a0, 0);
    SU32(act, 0xff0a8, 0);
    SU32(act, 0xff0c0, 0);
    SF32(act, 0xff0c4, 30.0f /* 0x931d98 */);
    SU32(act, 0x2c8, 1);
    SU32(act, 0x100124, 1);
    SU32(act, 0xff0b0, U32(0, 0x00a95d50));
    SU32(act, 0xff0b4, U32(0, 0x00a95d54));
    SU32(act, 0xff0b8, U32(0, 0x00a95d58));
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
        SU32(logic, 0, VT_EGGHUNT_AUDIO);
        SU32(act, 0xfd8, logic);
    } else SU32(act, 0xfd8, 0);
    ACT_END(F);
    return act;
}
RX_FUNCTION(0x00574730, rx_egghunt_construct)

/* 00574860 (ECX = act): the destructor's body: the ten eggs from the last to the first get the ModelEntity base's vtables (IRenderable
 * 0x93708c at +0x90, Entity 0x95a2e4) and the two entity registries ([0xa95824], [0xa95828]: vf1 with the egg) are told; then a jump to
 * Activity's destructor 005566b0. Frame: push ebx / ebp / esi. */
void rx_egghunt_destroy(rx_addr act) {
    ACT_FRAME(F, 0, 3);
    SU32(act, 0, VT_EGGHUNT);
    for (int i = 9; i >= 0; i--) {
        const rx_addr e = EGG(act, i);
        SU32(e, 0x90, 0x0093708c);
        SU32(e, 0, 0x0095a2e4);
        for (int k = 0; k < 2; k++) {
            const rx_addr registry = U32(0, k ? 0x00a95828 : 0x00a95824);
            if (registry != 0) { const uint32_t a[1] = {e}; (void)act_vcall(registry, 4, 1, a); }
        }
    }
    ACT_END(F);
    rx_waypoint_activity_base_destroy(act);
}
RX_FUNCTION(0x00574860, rx_egghunt_destroy)

/* 00574840 slot 0 (thiscall; flags; RET 4) -> act: the body (00574860), freed with flags & 1. Frame: push esi. */
rx_addr rx_egghunt_sdtor(rx_addr act, uint32_t flags) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(0, rx_egghunt_destroy(act));
    if (flags & 1) ACT_CALL(1, rx_reflex_aligned_free(act));
    ACT_END(F);
    return act;
}
RX_FUNCTION(0x00574840, rx_egghunt_sdtor)

/* 0055ec30 (ECX = egg) -> egg: GreedObjectModel's constructor: ModelEntity's (00401db0), the class's vtables, not collected, the spin
 * angle 0, +0x70 = 1. Frame: push esi. */
rx_addr rx_greed_model_construct(rx_addr e) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(1, (void)rx_model_entity_construct(e));
    SU32(e, 0, VT_GREED_MODEL);
    SU32(e, 0x90, VT_GREED_MODEL_O90);
    SU32(e, 0x190, 0);
    SF32(e, 0x140, 0.0f);
    SU32(e, 0x70, 1);
    ACT_END(F);
    return e;
}
RX_FUNCTION(0x0055ec30, rx_greed_model_construct)

/* ======================================================================================================================== */
/* the listeners (as race_infection.c's templates)                                                                          */

/* 00589ed0 (EAX = the session channel; stdcall act; RET 4): "NetworkObjectRemoved" with 00575f00 (proxy 0x945c24) */
void rx_egghunt_net_removed_listen(rx_addr channel, rx_addr act) {
    ACT_CALL(1, act_subscribe(0x00589ed0, 1, 1, 2, channel, 0x0093ee98 /* "NetworkObjectRemoved" */, 0x00945c24, 0x009cdc7c, act, 0x00575f00));
}
RX_FUNCTION(0x00589ed0, rx_egghunt_net_removed_listen)
/* 00589f90: "NetEggHuntEggPicked" with 00575f20 (proxy 0x945c44: void(DOHandle, unsigned, int, int)) */
void rx_egghunt_net_picked_listen(rx_addr channel, rx_addr act) {
    ACT_CALL(1, act_subscribe(0x00589f90, 1, 1, 2, channel, 0x00942350 /* "NetEggHuntEggPicked" */, 0x00945c44, 0x009d0710, act, 0x00575f20));
}
RX_FUNCTION(0x00589f90, rx_egghunt_net_picked_listen)
/* 0058a050 / 0058a0b0: the two listeners leave (frame: push ebp; mov ebp, esp; and esp, -8; sub esp, 0x14; push esi) */
void rx_egghunt_net_removed_unlisten(rx_addr channel, rx_addr act) {
    act_unlisten(1, channel, 0x0093ee98, 0x00945c24, 0x009cdc7c, act, 0x00575f00);
}
RX_FUNCTION(0x0058a050, rx_egghunt_net_removed_unlisten)
void rx_egghunt_net_picked_unlisten(rx_addr channel, rx_addr act) {
    act_unlisten(1, channel, 0x00942350, 0x00945c44, 0x009d0710, act, 0x00575f20);
}
RX_FUNCTION(0x0058a0b0, rx_egghunt_net_picked_unlisten)

/* 0058a110 (ESI = the activity's channel, ECX = the winner, EDX = the text; stdcall act; RET 4): "Player Wins" raised with
 * {act, winner, text, 1} (void(Activity*, EggHuntDriver*, const char*, int): 0x9cf740). Frame: sub esp, 0x14; push edi. */
void rx_egghunt_player_wins(rx_addr manager, rx_addr winner, rx_addr text, rx_addr act) {
    ACT_FRAME(F, 0x14, 1);
    SU32(F, 8, winner);
    SU32(F, 4, act);
    SU32(F, 0xc, text);
    SU32(F, 0x10, 1);
    act_raise(manager, STR_PLAYER_WINS, 0x009cf740, F + 4, 0x10);
    ACT_END(F);
}
RX_FUNCTION(0x0058a110, rx_egghunt_player_wins)

/* ======================================================================================================================== */
/* the activity's virtuals                                                                                                   */

/* 005748f0 slot 2 (thiscall; arg; RET 4): Activity::vf2; the round 0; the area's "GreedModel" (007d2c80 in the area subscene +0x118)
 * hidden (+0xa2 = 0) and its model (+0x100) given to the ten eggs: each takes the model, its flags (+0xb0: bit 0 / 3 off, 4 on),
 * the entity description {scene, "GreedObject", 0} (0080ddf0), its matrix copied to the render matrix (+0x30 -> +0xc0), the flags
 * (bit 4 off, 3 on), vf15 (slot 0x3c) with +0x150, a scene id (+0x14, under the scene's lock), the scene (007d2f70), then +0x14 = its
 * index and visible (+0xa2 = 1); the round's places (00574dd0), the two network listeners, the host's network egg hunt object
 * (ONLINE), "Activity Init" raised. Frame: sub esp, 0x14; push ebx / ebp / esi / edi (the description F + 0x18; the egg index in the
 * argument slot F + 0x28). */
void rx_egghunt_vf2(rx_addr act, rx_addr arg) {
    ACT_FRAME(F, 0x14, 4);
    SU32(F, 0x10, act);
    ACT_CALL(1, rx_activity_vf2(act, arg));
    /* the dead stack Activity::vf2 leaves at F - 0xd0: msvcr90's _aligned_offset_malloc pushes malloc's size there (78583c48:
     * 0xc + 0xf + 4 = 0x1f for the last 0xc-byte ReflexAlignedAlloc of its event setup, 005ba9c2); nothing writes the word again
     * before the eggs join the scene, whose event grid copies it as word +0xc of each egg's cell records (anim_scene.c grid_add) */
    SU32(F, -0xd0, 0x1f);
    SU32(act, 0x100110, 0);
    rx_addr model_entity;
    ACT_CALLR(model_entity, 3, rx_scene_manager_find_in_subscene(U32(U32(act, 0x234), 0x20138), STR_GREED_MODEL, act + 0x118));
    if (model_entity == 0)   /* the original writes +0xa2 of 0 (0057492d); the session refuses such a level first (rx_minigame_content_missing) */
        act_unreached(0x0057492d, "EggHunt: no \"GreedModel\" entity in the area subscene (the level lacks the mode's content)");
    SU8(model_entity, 0xa2, 0);
    const uint32_t model = U32(model_entity, 0x100);
    SU32(F, 0x14, model);
    SU32(F, 0x28, 0);
    SU32(F, 0x20, 0);
    SU32(F, 0x1c, STR_GREED_OBJECT);
    for (uint32_t i = 0; i < 10; i++) {
        const rx_addr e = EGG(act, i);
        SU32(F, 0x18, U32(U32(act, 0x234), 0x20138));
        SU32(e, 0x100, model);
        SU16(e, 0xb0, (uint16_t)((U16(e, 0xb0) & 0xfff6u) | 0x10u));
        ACT_CALL(2, rx_entity_describe(e, F + 0x18));
        for (uint32_t k = 0; k < 0x40; k += 4) SU32(e + 0xc0, k, U32(e + 0x30, k));
        SU16(e, 0xb0, (uint16_t)((U16(e, 0xb0) & 0xffefu) | 8u));
        { const uint32_t a[1] = {e + 0x150}; (void)act_vcall(e, 0x3c, 1, a); }
        const rx_addr scene = U32(U32(U32(F, 0x10), 0x234), 0x20138);
        rx_set_sp(F - 4);
        os_cs(0x0091600c, scene + 4);
        rx_set_sp(F);
        const uint32_t id = U32(scene, 0x19f4);
        SU32(scene, 0x19f4, id + 1);
        SU32(e, 0x14, id);
        rx_set_sp(F - 4);
        os_cs(0x009160f8, scene + 4);
        rx_set_sp(F);
        ACT_CALL(1, rx_scene_manager_add_entity(e, scene));
        SU32(e, 0x14, U32(F, 0x28));
        SU32(F, 0x28, i + 1);
        SU8(e, 0xa2, 1);
    }
    ACT_CALL(1, rx_egghunt_place_round(act));
    session_static();
    ACT_CALL(1, rx_egghunt_net_removed_listen(U32(0, 0x00da6ca0), act));
    session_static();
    ACT_CALL(1, rx_egghunt_net_picked_listen(U32(0, 0x00da6ca0), act));
    session_static();
    if (session_networked()) {
        session_static();
        if (U32(U32(0, 0x00da6cb0), 0x1c48) != 0) act_unreached(0x00574b02, "ONLINE: the host's network egg hunt object (008879b0 / 00887780)");
    }
    SU32(F, 0x28, act);   /* the payload: the argument slot */
    act_raise(U32(0, 0x00a93f74), STR_ACTIVITY_INIT, TD_ACTIVITY, F + 0x28, 4);
    ACT_END(F);
}
RX_FUNCTION(0x005748f0, rx_egghunt_vf2)

/* 00574b90 slot 3 (ECX = activity): (ONLINE: the session's NetEggHunt objects unlinked, 00585e10 / 00885d80 / 0088c1e0 / 0088c190.)
 * Every driver of the two lists leaves the activity, the ten eggs' vf9 (slot 0x24, released), the two listeners leave, then a jump to
 * Activity::vf3. Frame: sub esp, 0x24; push edi; then push ebx / esi around the drivers and eggs. */
void rx_egghunt_vf3(rx_addr act) {
    ACT_FRAME(F, 0x24, 1);
    session_static();
    rx_set_sp(F - 8);   /* push ebx / esi */
    if (session_networked()) act_unreached(0x00574bd1, "ONLINE: the session's NetEggHunt objects unlinked (00585e10)");
    while (I32(act, 0x26c) > 0) ACT_CALL(2, rx_activity_on_player_left(act, U32(U32(act, 0x268), 0)));
    while (I32(act, 0x280) > 0) ACT_CALL(2, rx_activity_on_player_left(act, U32(U32(act, 0x27c), 0)));
    for (uint32_t i = 0; i < 10; i++) (void)act_vcall(EGG(act, i), 0x24, 0, 0);
    rx_set_sp(F);
    session_static();
    ACT_CALL(1, rx_egghunt_net_removed_unlisten(U32(0, 0x00da6ca0), act));
    session_static();
    ACT_CALL(1, rx_egghunt_net_picked_unlisten(U32(0, 0x00da6ca0), act));
    ACT_END(F);
    rx_activity_vf3(act);
}
RX_FUNCTION(0x00574b90, rx_egghunt_vf3)

/* 00574cd0 slot 4 (ECX = activity): the class's event types on the activity's channel (Activity::vf4 is not called): "Round Started"
 * (005925e0), "Round Over" / "Match Over" (00590c00), "EggCollected" (00591500), "EggSpawned" (00590c00). Frame: push esi. */
void rx_egghunt_vf4(rx_addr act) {
    ACT_FRAME(F, 0, 1);
    act_event_type(0x005925e0, U32(act, 0x290), 0);
    act_event_type(0x00590c00, U32(act, 0x290), STR_ROUND_OVER);
    act_event_type(0x00590c00, U32(act, 0x290), STR_MATCH_OVER);
    act_event_type(0x00591500, U32(act, 0x290), STR_EGG_COLLECTED);
    act_event_type(0x00590c00, U32(act, 0x290), STR_EGG_SPAWNED);
    ACT_END(F);
}
RX_FUNCTION(0x00574cd0, rx_egghunt_vf4)

/* 00574d20 slot 15 (ECX = activity): the round's start: the places (00574dd0), then a jump to Activity::vf15. Frame: push esi. */
void rx_egghunt_vf15(rx_addr act) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(1, rx_egghunt_place_round(act));
    ACT_END(F);
    rx_activity_vf15(act);
}
RX_FUNCTION(0x00574d20, rx_egghunt_vf15)

/* 00574d40 slot 16 (ECX = activity): a new round. A new match (+0x100124) clears every driver's +0xf14, the match state and the round;
 * Activity::vf16; every driver takes its start zone (0054cd20); +0x100124 = 0. Frame: push ebx / esi / edi. */
void rx_egghunt_vf16(rx_addr act) {
    ACT_FRAME(F, 0, 3);
    if (U32(act, 0x100124) != 0) {
        for (int32_t i = 0; i < I32(act, 0x26c); i++) SU32(U32(U32(act, 0x268), 4u * (uint32_t)i), 0xf14, 0);
        SU32(act, 0x100118, 0);
        SU32(act, 0x100110, 0);
    }
    ACT_CALL(0, rx_activity_vf16(act));
    for (int32_t i = 0; i < I32(act, 0x26c); i++)
        ACT_CALL(1, rx_player_start_zone(U32(U32(U32(act, 0x268), 4u * (uint32_t)i), 0x74)));
    SU32(act, 0x100124, 0);
    ACT_END(F);
}
RX_FUNCTION(0x00574d40, rx_egghunt_vf16)

/* 005757b0 slot 18 (ECX = activity): the round's reset: no winner, the ten eggs not collected and visible, then a jump to
 * Activity::vf18. */
void rx_egghunt_vf18(rx_addr act) {
    SU32(act, 0x10011c, 0);
    for (uint32_t i = 0; i < 10; i++) {
        SU32(EGG(act, i), 0x190, 0);
        SU8(EGG(act, i), 0xa2, 1);
    }
    rx_activity_vf18(act);
}
RX_FUNCTION(0x005757b0, rx_egghunt_vf18)

/* 00575af0 slot 25 (thiscall; dt; RET 4): Activity::vf25, then the UI's routing "GotoCaptureSinglePlayer" (UIRoutingMessage; UI
 * boundary) and the UI event "GotoHub" raised on the UI's channel when it exists (as 00573f90 without its spectator branch).
 * Frame: push ebp; mov ebp, esp; and esp, -8; push ecx; push ebx / esi / edi; dt through x87. */
void rx_egghunt_vf25(rx_addr act, float dt) {
    ACT_FRAME_EBP(F, 8, 4, 3);
    ACT_CALL(1, rx_activity_pre(act, x87_copy(dt)));
    const rx_addr ui = U32(0, 0x00a93fcc);
    rx_regs in, out;
    memset(&in, 0, sizeof in);
    in.r[6] = ui;
    in.r[3] = ui;
    in.r[7] = 0x0094cc74;   /* "UIRoutingMessage" */
    const uint32_t v[1] = {0x00942320u /* "GotoCaptureSinglePlayer" */};
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
RX_FUNCTION(0x00575af0, rx_egghunt_vf25)

/* 00575e00 slot 27 (thiscall; dt; RET 4): WindDown. When the match is over (+0x100118) a new match follows (+0x100124 = 1) and
 * Activity::vf27 runs. Else the wind-down timer (+0x100120) runs down and at 0 every driver's vehicle lets go of its controls
 * (0045f590) and the activity's vf16 (slot 0x40) starts the next round; the race clock's limit +0x2c4 = FLT_MAX; Activity::vf27.
 * Frame: push ebx / esi (+ edi in the loop); dt through x87. */
void rx_egghunt_vf27(rx_addr act, float dt) {
    ACT_FRAME(F, 0, 2);
    if (U32(act, 0x100118) != 0) {
        SU32(act, 0x100124, 1);
        ACT_CALL(1, rx_activity_wind_down(act, x87_copy(dt)));
        ACT_END(F);
        return;
    }
    const float t = rx_subs(F32(act, 0x100120), dt);
    SU32(act, 0x100124, 0);
    SF32(act, 0x100120, t);
    if (t <= 0.0f) {   /* comiss 0, t; jb: the release when not 0 < t and ordered */
        if (I32(act, 0x26c) > 0) {
            rx_set_sp(F - 4);   /* push edi */
            for (int32_t i = 0; i < I32(act, 0x26c); i++)
                ACT_CALL(0, rx_vehicle_release_controls(U32(U32(U32(U32(act, 0x268), 4u * (uint32_t)i), 0x74), 0x1ac)));
            rx_set_sp(F);
        }
        (void)act_vcall(act, 0x40, 0, 0);
    }
    SU32(act, 0x2c4, 0x7f7fffffu /* FLT_MAX, 0x977494 */);
    ACT_CALL(1, rx_activity_wind_down(act, x87_copy(dt)));
    ACT_END(F);
}
RX_FUNCTION(0x00575e00, rx_egghunt_vf27)

/* 00575b60 slot 28 (thiscall; dt; RET 4): InProgress. After an egg was collected (+0xff0c0) the respawn clock (+0xff0c4) runs down and
 * at 0 the ten eggs respawn (00575440); Activity::vf28; (ONLINE: the network egg hunt object found or made: the host's 008879b0 /
 * 00887780, a client's 006e4fa0 / 00585c30); the eggs' zone's position (vf17 slot 0x44) into F + 0x20; then (unless a client of a
 * live network object) each egg against each driver's vehicle (00574660; ONLINE: a collection sent to the host, 006f4c50).
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x44; push ebx / esi / edi (F + 0x14 the egg, F + 0x18 the driver index, F + 0x1c
 * the eggs left). */
void rx_egghunt_vf28(rx_addr act, float dt) {
    ACT_FRAME_EBP(F, 16, 0x44, 3);
    if (U32(act, 0xff0c0) != 0) {
        const float t = rx_subs(F32(act, 0xff0c4), dt);
        SF32(act, 0xff0c4, t);
        if (t <= 0.0f) ACT_CALL(0, rx_egghunt_spawn_eggs(act));
    }
    ACT_CALL(1, rx_activity_in_progress(act, x87_copy(dt)));
    if (U32(act, 0x100128) == 0) {
        session_static();
        if (session_networked()) {
            session_static();
            act_unreached(0x00575c2b, "ONLINE: the network egg hunt object found or made (008879b0 / 006e4fa0)");
        }
    }
    { const uint32_t a[1] = {F + 0x20}; (void)act_vcall(U32(act, 0x10012c), 0x44, 1, a); }
    if (U32(act, 0x100128) != 0) {
        session_static();
        if (U32(U32(0, 0x00da6cb0), 0x1c48) == 0) { ACT_END(F); return; }
    }
    SU32(F, 0x14, act);
    SU32(F, 0x1c, 10);
    for (uint32_t k = 0; k < 10; k++) {
        SU32(F, 0x18, 0);
        if (I32(act, 0x26c) > 0) {
            const rx_addr e = U32(F, 0x14) + 0xff0d0;
            for (int32_t i = 0; i < I32(act, 0x26c); i++) {
                const rx_addr d = U32(U32(act, 0x268), 4u * (uint32_t)i);
                uint32_t got;
                ACT_CALLR(got, 1, rx_egghunt_collect(e, d, U32(d, 0x74)));
                if (got != 0 && U32(act, 0x100128) != 0 && U32(d, 0xef0) != 0)
                    act_unreached(0x00575d68, "ONLINE: an egg's collection sent to the host (006f4c50)");
                SU32(F, 0x18, (uint32_t)(i + 1));
            }
        }
        SU32(F, 0x14, U32(F, 0x14) + 0x1a0);
        SU32(F, 0x1c, 9 - k);
    }
    ACT_END(F);
}
RX_FUNCTION(0x00575b60, rx_egghunt_vf28)

/* 005758e0 slot 40 (thiscall; old state; RET 4): WindDown begins. Activity::vf40; the wind-down timer 3 s; for a single-player activity
 * (mode +0x238 == 1) the message ("%s %s": the winner's name and the localized STR_WINS_THE_MATCH / STR_WINS, or "%s" STR_TIE without a
 * winner (always: the original sets no winner): UI text, the oracle runs the original localization chain, the product leaves an empty
 * string, as race_infection.c's vf40) raised as "Player Wins" (0058a110); then "Match Over" (+0x100118) or "Round Over" with the
 * activity. Frame: sub esp, 0x20c; push ebp / esi / edi (F); the UTF-8 name F + 0x10, the text F + 0x110. */
void rx_egghunt_vf40(rx_addr act, uint32_t old_state) {
    ACT_FRAME(F, 0x20c, 3);
    (void)old_state;
    ACT_CALL(1, rx_activity_wind_down_enter(act));
    SF32(act, 0x100120, 3.0f /* 0x92fe48 */);
    if (U32(act, 0x238) == 1) {
        const rx_addr winner = U32(act, 0x10011c);
        const rx_addr key = winner == 0 ? 0x00942318u /* "STR_TIE" */ : U32(act, 0x100118) != 0 ? 0x00942304u /* "STR_WINS_THE_MATCH" */ : 0x00941fc4u /* "STR_WINS" */;
#ifdef RX_ORACLE
        uint32_t zero = 0;
        const uint32_t lang = rx_call_original(0x005dfc90, RX_CDECL, 0, 1, &zero);
        rx_addr entry;
        ACT_CALLR(entry, 0, rx_ui_language_lookup(key, lang));
        { uint32_t a[8] = {0xfde9, 0, U32(entry, 0x10), 0xffffffffu, F + 0x10, 0x100, 0, 0}; rx_os_import(0x009160a8, 8, a); }
        SU8(F, 0x10f, 0);
        if (winner != 0) {   /* push &name; push the winner's name; push "%s %s"; call 0078fd60 (EDI = the text, ESI = 0x104) */
            SU32(F, -12, 0x00941fd0); SU32(F, -8, U32(winner, 0x74) + 0x54); SU32(F, -4, F + 0x10);
            rx_set_sp(F - 16);
            rx_format_buffer(F + 0x110, 0x104, 0x00941fd0 /* "%s %s" */, F - 8);
        } else {             /* push &name; push "%s"; call 0078fd60 */
            SU32(F, -8, 0x009419e4); SU32(F, -4, F + 0x10);
            rx_set_sp(F - 12);
            rx_format_buffer(F + 0x110, 0x104, 0x009419e4 /* "%s" */, F - 4);
        }
        rx_set_sp(F);
#else
        (void)key;
        SU8(F, 0x110, 0);
#endif
        ACT_CALL(1, rx_egghunt_player_wins(U32(act, 0x290), U32(act, 0x10011c), F + 0x110, act));
    }
    SU32(F, 0xc, act);
    act_raise(U32(act, 0x290), U32(act, 0x100118) != 0 ? STR_MATCH_OVER : STR_ROUND_OVER, TD_ACTIVITY, F + 0xc, 4);
    ACT_END(F);
}
RX_FUNCTION(0x005758e0, rx_egghunt_vf40)

/* 00575eb0 slot 42 (ECX = activity): the round's end: the elapsed time (+0x294) reached the duration (+0x2bc) -> WindDown (state 3) */
void rx_egghunt_vf42(rx_addr act) {
    if (F32(act, 0x294) >= F32(act, 0x2bc)) ACT_CALL(2, rx_activity_set_state(act, 3));
}
RX_FUNCTION(0x00575eb0, rx_egghunt_vf42)

/* 005758a0 slot 43 (stdcall a, b, tie; RET 0xc; ECX unused) -> the standings' order: the more eggs (+0xf1c), then (with tie) the start
 * zones (+0x1dc of the players: a >= b gives 1, else -1). */
int32_t rx_egghunt_vf43(rx_addr act, rx_addr a, rx_addr b, uint32_t tie) {
    (void)act;
    const int32_t r = I32(b, 0xf1c) - I32(a, 0xf1c);
    if (r != 0) return r;
    if (tie == 0) return 0;
    return I32(U32(a, 0x74), 0x1dc) >= I32(U32(b, 0x74), 0x1dc) ? 1 : -1;
}
RX_FUNCTION(0x005758a0, rx_egghunt_vf43)

/* 00575840 slot 44 (thiscall; player; RET 4) -> a new EggHuntDriver (0xf20 bytes: NetworkDriver's constructor 0050e1b0, the activity,
 * the class's vtable, +0xf14 / +0xf18 / +0xf1c = 0) for the player, 0 when the allocation fails. Frame: push esi / edi. */
rx_addr rx_egghunt_vf44(rx_addr act, rx_addr player) {
    ACT_FRAME(F, 0, 2);
    rx_addr d;
    ACT_CALLR(d, 1, rx_reflex_aligned_alloc(0xf20));
    if (d != 0) {
        ACT_CALL(1, (void)rx_network_driver_construct(d, player));
        SU32(d, 0xf10, act);
        SU32(d, 0, VT_EGGHUNT_DRIVER);
        SU32(d, 0xf14, 0);
        SU32(d, 0xf18, 0);
        SU32(d, 0xf1c, 0);
    }
    ACT_END(F);
    return d;
}
RX_FUNCTION(0x00575840, rx_egghunt_vf44)

/* 00575ed0 slot 47 (thiscall; driver; RET 4): the driver's rules: outside a free-ride area (the world's +0x22144 != 1) the free ride
 * rule (00587690), initialised with the driver (vf1, slot 4). Frame: push esi (inside the branch). */
void rx_egghunt_vf47(rx_addr act, rx_addr driver) {
    if (U32(U32(act, 0x234), 0x22144) == 1) return;
    ACT_FRAME(F, 0, 1);
    rx_addr rule;
    ACT_CALLR(rule, 0, rx_waypoint_get_free_ride_rule(driver));
    const uint32_t a[1] = {driver};
    (void)act_vcall(rule, 4, 1, a);
    ACT_END(F);
}
RX_FUNCTION(0x00575ed0, rx_egghunt_vf47)

/* ======================================================================================================================== */
/* the round and the eggs                                                                                                    */

/* 00574dd0 (stdcall act; RET 4): the round's places (as Infection's 00573450): the scene's start entities of type 0x1017 named by the
 * activity (+0x118: 007d2820), the round's start zone (the activity row's MiniGameStartZone for type 0xf; ONLINE: the session's
 * 006f7510) into +0xff0a0; every driver's start (+0x340 + 0x30 * i: position, direction) on a circle of 24 around the zone's entity,
 * i * 360 / riders degrees from its direction (the game's inlined sine / cosine), on the terrain (007c5950) + 2; "Round Started"
 * ({act, round + 1, the most eggs of the standings' +0xf14, the rounds to win}) when the standings exist (+0x2d0); the start entities
 * again as a vector (007d26d0); +0x880 = 0xc, the eggs' zone = the round's zone and the eggs spawned (00575440); the vector freed.
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0xe4; push ebx / esi / edi. */
void rx_egghunt_place_round(rx_addr act) {
    ACT_FRAME_EBP(F, 16, 0xe4, 3);
    const rx_addr scene = U32(U32(act, 0x234), 0x20138);
    for (uint32_t k = 0; k < 0x2c; k += 4) SU32(F + 0xa4, k, 0);
    SU32(F, 0x5c, scene);
    SU32(F, 0xa0, 0);
    ACT_CALL(6, (void)rx_scene_group_query(scene, 0x1017, F + 0xa0, 0xc, act + 0x118, 0));
    int32_t row;
    ACT_CALLR(row, 1, rx_activity_find_row(ACT_ACTIVITY_TBL, 0xf, (int32_t)(int8_t)U8(act, 0x220)));
    SU32(act, 0xff0a0, 0);
    session_static();
    uint32_t zone;
    if (session_networked()) act_unreached(0x00574e8e, "ONLINE: the session's start zone (006f7510)");
    if (row == -1) zone = 0;
    else {
        uint32_t col;
        ACT_CALLR(col, 1, rx_tbl_column_index(ACT_ACTIVITY_TBL, 0x0093dfd4 /* "MiniGameStartZone" */));
        rx_addr cell;
        ACT_CALLR(cell, 1, rx_tbl_cell_word(ACT_ACTIVITY_TBL, col, (uint32_t)row));
        zone = U32(cell, 0);
    }
    SU32(act, 0xff0a0, zone);
    const int32_t riders = I32(act, 0x26c);
    const rx_addr start = U32(F + 0xa0, 4 * U32(act, 0xff0a0));
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
            const int32_t eggs = I32(rx_stand(act, (uint32_t)i), 0xf14);
            if (eggs > best) best = eggs;
        }
        SU32(F, 0xdc, U32(act, 0x100114));
        SU32(F, 0xd0, act);
        SU32(F, 0xd4, U32(act, 0x100110) + 1);
        SU32(F, 0xd8, (uint32_t)best);
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
    SU32(act, 0xff0a8, U32(act, 0xff0a0));
    ACT_CALL(0, rx_egghunt_spawn_eggs(act));
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
RX_FUNCTION(0x00574dd0, rx_egghunt_place_round)

/* 00575440 (EDI = act): the eggs spawn at the next zone: the area's start entities (0x1017, 007d2820), the eggs' zone (+0xff0a8) the
 * next one (back to 0 past the last), its entity into +0x10012c, r = its SnakeAreaRadius (+0xa0) / 10; its position (vf17 slot 0x44)
 * into +0xff0b0; egg i at the zone's position + the vector v = [0x9cb4a0] (.data) scaled (v * i * r + v) and turned by i * [0x9b7338]
 * (.data, 36) degrees about the vertical (x' = cos * x + sin * z, z' = cos * z - sin * x; the inlined sine / cosine), on the terrain
 * (007c5950) + 2.0 (in double), placed (vf28 slot 0x70), not collected, visible; the respawn off, its clock 30.
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x68; push ebx / esi (the list F + 0x40, the position F + 0x20). */
void rx_egghunt_spawn_eggs(rx_addr act) {
    ACT_FRAME_EBP(F, 16, 0x68, 2);
    const rx_addr scene = U32(U32(act, 0x234), 0x20138);
    for (uint32_t k = 0; k < 0x2c; k += 4) SU32(F + 0x44, k, 0);
    SU32(F, 0x40, 0);
    uint32_t n;
    ACT_CALLR(n, 6, rx_scene_group_query(scene, 0x1017, F + 0x40, 0xc, act + 0x118, 0));
    SU32(act, 0xff0a8, U32(act, 0xff0a8) + 1);
    if (U32(act, 0xff0a8) >= n) SU32(act, 0xff0a8, 0);
    const rx_addr zone = U32(F + 0x40, 4 * U32(act, 0xff0a8));
    SU32(act, 0x10012c, zone);
    SF32(F, 0x18, rx_divs(F32(zone, 0xa0), 10.0f /* 0x92fd88 */));
    SU32(F, 0x14, scene_terrain(scene));
    { const uint32_t a[1] = {act + 0xff0b0}; (void)act_vcall(U32(act, 0x10012c), 0x44, 1, a); }
    for (int32_t i = 0; i < 10; i++) {
        const rx_addr e = EGG(act, i);
        const float vx = F32(0, 0x009cb4a0), vy = F32(0, 0x009cb4a4), vz = F32(0, 0x009cb4a8);
        const float fi = (float)i;
        const float s = rx_muls(fi, F32(F, 0x18));
        const float angle = rx_muls(rx_muls(fi, F32(0, 0x009b7338)), rx_bf(0x3c8efa34u) /* pi / 180, 0x977678 */);
        const float px = rx_adds(rx_muls(vx, s), vx);
        const float py = rx_adds(rx_muls(vy, s), vy);
        const float pz = rx_adds(rx_muls(vz, s), vz);
        SF32(F, 0x34, py);
        const float sign = angle >= 0.0f ? 1.0f /* 0x931f24 */ : rx_bf(0xbf800000u);   /* comiss angle, 0; jb: -1 (0x9cd9e8) */
        SF32(F, 0x10, sign);
        const float x = rx_muls(sign, angle);
        const float t = b1_trig_reduce(x);
        const float t2 = rx_muls(t, t), t4 = rx_muls(t2, t2);
        const float cosv = rx_subs(rx_bf(0x80000000u) /* -0.0, 0x9774a8 */, b1_cos_poly(t2, t4));
        const float sinv = rx_muls(rx_muls(b1_sin_poly(t2, t4), t), sign);
        const float nz = rx_subs(rx_muls(cosv, pz), rx_muls(sinv, px));
        const float nx = rx_adds(rx_muls(cosv, px), rx_muls(sinv, pz));
        SF32(F, 0x1c, nz);
        SF32(F, 0x30, nx);
        { const uint32_t a[1] = {F + 0x20}; (void)act_vcall(U32(act, 0x10012c), 0x44, 1, a); }
        SF32(F, 0x20, rx_adds(F32(F, 0x20), F32(F, 0x30)));
        SF32(F, 0x24, rx_adds(F32(F, 0x34), F32(F, 0x24)));
        SF32(F, 0x28, rx_adds(F32(F, 0x1c), F32(F, 0x28)));
        uint32_t edx;
        ACT_CALL(1, (void)phys_terrain_base_height_at(U32(F, 0x14), F + 0x20, &edx));
        SF32(F, 0x24, (float)((double)F32(F, 0x24) + 2.0 /* 0x9774f8 */));
        { const uint32_t a[1] = {F + 0x20}; (void)act_vcall(e, 0x70, 1, a); }
        SU32(e, 0x190, 0);
        SU8(e, 0xa2, 1);
    }
    SU32(act, 0xff0c0, 0);
    SF32(act, 0xff0c4, 30.0f /* 0x931d98 */);
    ACT_END(F);
}
RX_FUNCTION(0x00575440, rx_egghunt_spawn_eggs)

/* 00574660 (ESI = the egg, EDI = the driver; stdcall player; RET 4) -> 1 when the player collects the egg: not collected yet, the player
 * has a vehicle, the egg's position (vf17 slot 0x44 -> F + 0x20) and the vehicle's (-> F + 0x10) closer than sqrt(10) (the squared
 * distance below 10.0): the activity's respawn starts (+0xff0c0), the driver's eggs + 1, the egg collected and hidden.
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x2c; push ebx. */
uint32_t rx_egghunt_collect(rx_addr e, rx_addr d, rx_addr player) {
    ACT_FRAME_EBP(F, 16, 0x2c, 1);
    uint32_t got = 0;
    if (U32(e, 0x190) == 0 && U32(player, 0x1ac) != 0) {
        { const uint32_t a[1] = {F + 0x20}; (void)act_vcall(e, 0x44, 1, a); }
        { const uint32_t a[1] = {F + 0x10}; (void)act_vcall(U32(player, 0x1ac), 0x44, 1, a); }
        const float dz = rx_subs(F32(F, 0x18), F32(F, 0x28));
        const float dy = rx_subs(F32(F, 0x14), F32(F, 0x24));
        const float dx = rx_subs(F32(F, 0x10), F32(F, 0x20));
        const float d2 = rx_adds(rx_adds(rx_muls(dz, dz), rx_muls(dy, dy)), rx_muls(dx, dx));
        if (10.0f /* 0x92fd88 */ > d2) {
            SU32(U32(d, 0xf10), 0xff0c0, 1);
            SU32(d, 0xf1c, U32(d, 0xf1c) + 1);
            SU32(e, 0x190, 1);
            SU8(e, 0xa2, 0);
            got = 1;
        }
    }
    ACT_END(F);
    return got;
}
RX_FUNCTION(0x00574660, rx_egghunt_collect)

/* ======================================================================================================================== */
/* GreedObjectModel (the eggs)                                                                                               */

/* 00574630 slot 28 (thiscall; position; RET 4): the position (+0x60, through x87) and a jump to vf15 (slot 0x3c) with the base matrix
 * +0x150 in the argument slot */
void rx_greed_model_vf28(rx_addr e, rx_addr p) {
    SF32(e, 0x60, x87_copy(F32(p, 0)));
    SU32(rx_sp(), 4, e + 0x150);
    SF32(e, 0x64, x87_copy(F32(p, 4)));
    SF32(e, 0x68, x87_copy(F32(p, 8)));
    tail_vcall1(e, 0x3c, e + 0x150);
}
RX_FUNCTION(0x00574630, rx_greed_model_vf28)

/* 00576040 slot 16 (thiscall; matrix; RET 4): the matrix (16 words) into +0x30 and a jump to vf15 (slot 0x3c) with +0x150 */
void rx_greed_model_vf16(rx_addr e, rx_addr m) {
    for (uint32_t k = 0; k < 0x40; k += 4) SU32(e + 0x30, k, U32(m, k));
    SU32(rx_sp(), 4, e + 0x150);
    tail_vcall1(e, 0x3c, e + 0x150);
}
RX_FUNCTION(0x00576040, rx_greed_model_vf16)

/* 00576070 slot 19 (thiscall; dt; RET 4): the egg spins: unless the engine is paused ([0xa95844] + 0x10 -> +0xc) the angle +0x140 +=
 * dt * 2.0944 (120 degrees per second); the matrix {cos, 0, -sin, 0 / 0, 1, 0, 0 / sin, 0, cos, 0 / 0, 0, 0, 1} (the game's inlined
 * sine / cosine) times the base matrix +0x150 (00401830) into +0x30. Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x48;
 * push esi / edi (the matrix F + 0x10, the sign F + 0xc). */
void rx_greed_model_vf19(rx_addr e, float dt) {
    ACT_FRAME_EBP(F, 16, 0x48, 2);
    if (U32(U32(U32(0, 0x00a95844), 0x10), 0xc) == 0) {
        const float a = rx_adds(rx_muls(dt, rx_bf(0x40060a91u) /* 2.0943949, 0x977820 */), F32(e, 0x140));
        SF32(e, 0x140, a);
        const float sign = a >= 0.0f ? 1.0f /* 0x931f24 */ : rx_bf(0xbf800000u);   /* comiss a, 0; jb: -1 (0x9cd9e8) */
        SF32(F, 0xc, sign);
        const float x = rx_muls(sign, a);
        const float t = b1_trig_reduce(x);
        const float t2 = rx_muls(t, t), t4 = rx_muls(t2, t2);
        const float cosv = rx_subs(rx_bf(0x80000000u) /* -0.0, 0x9774a8 */, b1_cos_poly(t2, t4));
        const float sinv = rx_muls(rx_muls(b1_sin_poly(t2, t4), t), sign);
        const float nsin = rx_subs(rx_bf(0x80000000u), sinv);
        const float m[16] = {cosv, 0.0f, nsin, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, sinv, 0.0f, cosv, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
        for (int k = 0; k < 16; k++) SF32(F + 0x10, 4u * (uint32_t)k, m[k]);
        ACT_CALL(1, rx_fx_matrix4_mul(F + 0x10, e + 0x150, F + 0x10));
        for (uint32_t k = 0; k < 0x40; k += 4) SU32(e + 0x30, k, U32(F + 0x10, k));
    }
    ACT_END(F);
}
RX_FUNCTION(0x00576070, rx_greed_model_vf19)

/* ======================================================================================================================== */
/* EggHuntDriver                                                                                                             */

/* 00515ff0 slot 0 (thiscall; flags; RET 4) -> driver: the class's vtable, NetworkDriver's destructor body (0050e2f0), freed with
 * flags & 1. Frame: push esi. */
rx_addr rx_egghunt_driver_sdtor(rx_addr d, uint32_t flags) {
    ACT_FRAME(F, 0, 1);
    SU32(d, 0, VT_EGGHUNT_DRIVER);
    ACT_CALL(0, rx_network_driver_dtor(d));
    if (flags & 1) ACT_CALL(1, rx_reflex_aligned_free(d));
    ACT_END(F);
    return d;
}
RX_FUNCTION(0x00515ff0, rx_egghunt_driver_sdtor)

/* 00516030 slot 3 (ECX = driver): the reset. NetworkDriver::vf3; a player with a vehicle in the world (+4) gets its start place's
 * effect query (00460020: ESI = the vehicle; the place +0x144 and +0x144 + 0x10); +0xf18 / +0xf1c / +0x3c0 cleared, +0xf14 too for a new
 * match (the activity's +0x100124); the behaviours' and rules' vf6 (005092b0 / 005092f0). Frame: push ebp; mov ebp, esp; and esp, -8;
 * push esi / edi. */
void rx_egghunt_driver_vf3(rx_addr d) {
    ACT_FRAME_EBP(F, 8, 0, 2);
    ACT_CALL(0, rx_network_driver_vf3(d));
    if (U32(U32(d, 0x74), 4) != 0) {
        const rx_addr place = U32(d, 0x144);
        ACT_CALL(2, rx_vehicle_effect_query(U32(U32(d, 0x74), 0x1ac), place, place + 0x10));
    }
    SU32(d, 0xf18, 0);
    SU32(d, 0xf1c, 0);
    SF32(d, 0x3c0, 0.0f);
    if (U32(U32(d, 0xf10), 0x100124) != 0) SU32(d, 0xf14, 0);
    ACT_CALL(1, rx_driver_behaviors_vf6(d));
    ACT_CALL(1, rx_driver_rules_vf6(d));
    ACT_END(F);
}
RX_FUNCTION(0x00516030, rx_egghunt_driver_vf3)

/* 005160b0 slot 7 (thiscall; dt; RET 4): the driver's update: a spectator (+0x908) runs NetworkDriver::vf7 only; else a human its rules
 * (00516190), a local computer rider its drive (00516120, Infection's), then NetworkDriver::vf7. Frame: push ebp; mov ebp, esp;
 * and esp, -8; push ecx; push esi; dt through x87. */
void rx_egghunt_driver_vf7(rx_addr d, float dt) {
    ACT_FRAME_EBP(F, 8, 4, 1);
    const rx_addr p = U32(d, 0x74);
    if (U32(p, 0x908) != 0) {
        ACT_CALL(1, rx_network_driver_update(d, x87_copy(dt)));
        ACT_END(F);
        return;
    }
    if (U32(p, 0x10) == 0) ACT_CALL(1, rx_driver_rules_tick(d));
    else if (U32(p, 0x10) == 1 && U32(p, 4) != 0) ACT_CALL(1, rx_infection_driver_ai(d, x87_copy(dt)));
    ACT_CALL(1, rx_network_driver_update(d, x87_copy(dt)));
    ACT_END(F);
}
RX_FUNCTION(0x005160b0, rx_egghunt_driver_vf7)

/* ======================================================================================================================== */
/* EggHuntActivityAudioLogic (as race_infection.c's)                                                                         */

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
/* the listener templates: 0058cff0 "Round Started" (00584df0, proxy 0x946144), 0058d0b0 (ECX = channel, EAX = name; stdcall logic,
 * method: void(Activity*), proxy 0x946164), 0058d170 "EggCollected" (00584e10, proxy 0x946184: void(Activity*, BaseDriver*)), 0058d230
 * "Player Wins" (00583fb0, proxy 0x9461a4), and their leaves 0058d2f0 / 0058d350 / 0058d3b0 / 0058d410 */
void rx_egghunt_audio_listen_round_started(rx_addr channel, rx_addr logic) {
    act_subscribe(0x0058cff0, 1, 1, 4, channel, STR_ROUND_STARTED, 0x00946144, TD_ROUND_STARTED, logic, 0x00584df0);
}
RX_FUNCTION(0x0058cff0, rx_egghunt_audio_listen_round_started)
void rx_egghunt_audio_listen_activity(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method) {
    act_subscribe(0x0058d0b0, 2 | 4, 2, 3, channel, name, 0x00946164, TD_ACTIVITY, logic, method);
}
RX_FUNCTION(0x0058d0b0, rx_egghunt_audio_listen_activity)
void rx_egghunt_audio_listen_collected(rx_addr channel, rx_addr logic) {
    act_subscribe(0x0058d170, 1, 1, 4, channel, STR_EGG_COLLECTED, 0x00946184, 0x009cda18, logic, 0x00584e10);
}
RX_FUNCTION(0x0058d170, rx_egghunt_audio_listen_collected)
void rx_egghunt_audio_listen_player_wins(rx_addr channel, rx_addr logic) {
    act_subscribe(0x0058d230, 1, 1, 4, channel, STR_PLAYER_WINS, 0x009461a4, 0x009cf630, logic, 0x00583fb0);
}
RX_FUNCTION(0x0058d230, rx_egghunt_audio_listen_player_wins)
void rx_egghunt_audio_unlisten_round_started(rx_addr channel, rx_addr logic) {
    act_unlisten(0, channel, STR_ROUND_STARTED, 0x00946144, TD_ROUND_STARTED, logic, 0x00584df0);
}
RX_FUNCTION(0x0058d2f0, rx_egghunt_audio_unlisten_round_started)
void rx_egghunt_audio_unlisten_activity(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method) {
    act_unlisten(0, channel, name, 0x00946164, TD_ACTIVITY, logic, method);
}
RX_FUNCTION(0x0058d350, rx_egghunt_audio_unlisten_activity)
void rx_egghunt_audio_unlisten_collected(rx_addr channel, rx_addr logic) {
    act_unlisten(0, channel, STR_EGG_COLLECTED, 0x00946184, 0x009cda18, logic, 0x00584e10);
}
RX_FUNCTION(0x0058d3b0, rx_egghunt_audio_unlisten_collected)
void rx_egghunt_audio_unlisten_player_wins(rx_addr channel, rx_addr logic) {
    act_unlisten(0, channel, STR_PLAYER_WINS, 0x009461a4, 0x009cf630, logic, 0x00583fb0);
}
RX_FUNCTION(0x0058d410, rx_egghunt_audio_unlisten_player_wins)

/* 00584e10 (thiscall logic; activity, driver; RET 8): the "EggCollected" cue for the driver's player */
void rx_egghunt_audio_collected(rx_addr logic, rx_addr act, rx_addr driver) { (void)act; RACE_CORE_CALL(4, rx_audio_cue(logic, STR_EGG_COLLECTED, U32(driver, 0x74), 0)); }
RX_FUNCTION(0x00584e10, rx_egghunt_audio_collected)
/* 00584e30 (thiscall logic; activity; RET 4): the "EggSpawned" cue */
void rx_egghunt_audio_spawned(rx_addr logic, rx_addr act) { (void)act; RACE_CORE_CALL(4, rx_audio_cue(logic, STR_EGG_SPAWNED, 0, 0)); }
RX_FUNCTION(0x00584e30, rx_egghunt_audio_spawned)

/* 005848f0 slot 3 (ECX = logic): the cue handlers listen ("Round Started", "Round Over", "Match Over", "EggCollected", "EggSpawned" (on
 * the activity template with 00584e30), "Player Wins") when the level's cue map holds the cue. Frame: push ebp; mov ebp, esp; and esp, -8;
 * sub esp, 0x28; push ebx / ebp / esi / edi. */
void rx_egghunt_audio_vf3(rx_addr logic) {
    ACT_FRAME_EBP(L, 8, 0x28, 4);
    if (U32(logic, 0x8c) != 0) {
        if (cue_present(logic, L, STR_ROUND_STARTED)) ACT_CALL(1, rx_egghunt_audio_listen_round_started(ACH(logic), logic));
        if (cue_present(logic, L, STR_ROUND_OVER)) ACT_CALL(2, rx_egghunt_audio_listen_activity(ACH(logic), STR_ROUND_OVER, logic, 0x00583a30));
        if (cue_present(logic, L, STR_MATCH_OVER)) ACT_CALL(2, rx_egghunt_audio_listen_activity(ACH(logic), STR_MATCH_OVER, logic, 0x00583a50));
        if (cue_present(logic, L, STR_EGG_COLLECTED)) ACT_CALL(1, rx_egghunt_audio_listen_collected(ACH(logic), logic));
        if (cue_present(logic, L, STR_EGG_SPAWNED)) ACT_CALL(2, rx_egghunt_audio_listen_activity(ACH(logic), STR_EGG_SPAWNED, logic, 0x00584e30));
        if (cue_present(logic, L, STR_PLAYER_WINS)) ACT_CALL(1, rx_egghunt_audio_listen_player_wins(ACH(logic), logic));
    }
    ACT_END(L);
}
RX_FUNCTION(0x005848f0, rx_egghunt_audio_vf3)

/* 00584b70 slot 4 (ECX = logic): the cue handlers leave. Same frame as vf3. */
void rx_egghunt_audio_vf4(rx_addr logic) {
    ACT_FRAME_EBP(L, 8, 0x28, 4);
    if (U32(logic, 0x8c) != 0) {
        if (cue_present(logic, L, STR_ROUND_STARTED)) ACT_CALL(1, rx_egghunt_audio_unlisten_round_started(ACH(logic), logic));
        if (cue_present(logic, L, STR_ROUND_OVER)) ACT_CALL(2, rx_egghunt_audio_unlisten_activity(ACH(logic), STR_ROUND_OVER, logic, 0x00583a30));
        if (cue_present(logic, L, STR_MATCH_OVER)) ACT_CALL(2, rx_egghunt_audio_unlisten_activity(ACH(logic), STR_MATCH_OVER, logic, 0x00583a50));
        if (cue_present(logic, L, STR_EGG_COLLECTED)) ACT_CALL(1, rx_egghunt_audio_unlisten_collected(ACH(logic), logic));
        if (cue_present(logic, L, STR_EGG_SPAWNED)) ACT_CALL(2, rx_egghunt_audio_unlisten_activity(ACH(logic), STR_EGG_SPAWNED, logic, 0x00584e30));
        if (cue_present(logic, L, STR_PLAYER_WINS)) ACT_CALL(1, rx_egghunt_audio_unlisten_player_wins(ACH(logic), logic));
    }
    ACT_END(L);
}
RX_FUNCTION(0x00584b70, rx_egghunt_audio_vf4)
