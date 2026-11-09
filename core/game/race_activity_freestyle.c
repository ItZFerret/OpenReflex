/* race_activity_freestyle.c -- the Freestyle event mode (FMX): MXActivityFreestyleStub (vtable 0x009441cc), MXActivityFreestyle
 * (0x009442cc) and the Freestyle championship classes (0x009443cc intro, 0x009444cc practice, 0x009445cc main event).
 *
 * A Freestyle event is a timed run (the row's Duration, e.g. 60 s) in an FMX arena: the rider does tricks, the stunt scorer
 * (fx_stunt.c, player +0xc70) adds them to the score (player +0xcec), the crowd meter (+0x13e600 / +0x13e604 / +0x13e608, the
 * stunt handlers below) rises with each trick and decays, and when the time is up (vf42) the activity winds down and posts the
 * results (vf41: the records, TotalEvents / TotalTime / Score). The activity's own layout (the stub constructor 00562280,
 * build_activity_freestyle.c):
 *     +0xff064  the Post clock            +0xff070  the event was decided by vf14     +0xff074  vf48 (the intro is over)
 *     +0xff080  FMXIntroScenario          +0x114250 FMXFinishScenario                 +0x129420 FMXResultsScenario
 *     +0x13e5f0 the Pre countdown         +0x13e5f8 the Post sub-state
 *   MXActivityFreestyle only (0x13e610 bytes): +0x13e600 the crowd timer, +0x13e604 its peak (100), +0x13e608 the level (5 .. 100,
 *   mirrored in the crowd audio's global 0x009c3edc).
 * Each function is a hand transliteration of the disassembly (address in its comment), at the original's stack depth for its
 * calls (callees that keep stack addresses see the original's). Calls the product cannot make are boundaries, documented at the
 * call: the UI (the hub routing 0059b860, the intro page 005dbf50 / 005db670), the network session (online only) and the clip
 * recorder's playback (the existing replay boundary). */
#include "race_activity_freestyle.h"
#include "anim_entity_dtors.h"
#include "build_activity_teardown.h"
#include "race_champ.h"
#include "race_beh2.h"
#include "race.h"
#include "race_extern.h"
#include "race_activity.h"
#include "race_activity_misc.h"
#include "race_activity_ui.h"
#include "race_finish.h"
#include "build_activity.h"
#include "build_activity_int.h"
#include "build_activity_pre.h"
#include "build_activity_teardown.h"
#include "build_activity_freeride_run.h"
#include "build_activity_clear.h"
#include "build_alloc.h"
#include "build_tables.h"
#include "build_vehicle.h"
#include "anim.h"
#include "anim_scenario.h"
#include "anim_scenario_misc.h"
#include "anim_scenario_race_intro.h"
#include "waypoint_activity.h"
#include "waypoint_in_progress.h"
#include "waypoint_playback.h"
#include "replay_clip.h"
#include "world_misc.h"
#include "world_wreck.h"
#include "highlight_progress.h"
#include "race_freestyle_driver.h"
#include "fx.h"
#include "phys_body.h"
#include "world_rider.h"
#include "bike_sim.h"
#include "loop_player.h"
#include "build_mxworld.h"
#include "build_scene_entity.h"
#include "loop_cam_entity.h"
#include "loop_world.h"
#include "waypoint_audio.h"
#include "anim_rider_dtors.h"
#include "crt.h"
#include "../src/rx_os.h"
#include <string.h>
#include "rider_cap.h"

/* ---- strings and globals the functions use ---- */
#define STR_STUNT_BEGIN          0x0095ba28u   /* "StuntBegin" */
#define STR_STUNT_FAILED         0x00960764u   /* "StuntFailed" */
#define STR_PHYSICS_STUNT_BEGIN  0x009607a0u   /* "PhysicsStuntBegin" */
#define STR_GHOST_COMPLETED      0x0093ee68u   /* "Ghost Session Completed" */
#define STR_ACTIVITY_INIT        0x00935ea4u   /* "Activity Init" */
#define STR_BASICS_LOADED        0x00936038u   /* "Activity Basic Essentials Loaded" */
#define STR_FREESTYLE_END        0x00941c8cu   /* "FreestyleEnd" */
#define STR_SHOW_RESULTS         0x00941c30u   /* "Show Activity Results" */
#define STR_POST_START           0x00941c04u   /* "Post Scenarios Start" */
#define STR_POST_END             0x00941c1cu   /* "Post Scenarios End" */
#define STR_POST_COMPLETE        0x00941c48u   /* "Post State Complete" */
#define STR_FINISH               0x00941eacu   /* "Finish" */
#define STR_RESULTS              0x00941fd8u   /* "Results" */
#define STR_START_LINE_SCRIPT    0x00941fe0u   /* "StartLineScript_FMX" */
#define STR_TOTAL_EVENTS         0x0096d99cu   /* "TotalEvents" */
#define STR_TOTAL_TIME           0x00957c1cu   /* "TotalTime" */
#define STR_SERIES_OVERLAY       0x0094cc98u   /* "SeriesOverlay" (the championship classes' vf2 argument) */
#define TYPEDESC_ACTIVITY        0x009cbd24u   /* typeid(void (*)(Activity*)) */
#define ACTIVITY_MANAGER_CHANNEL 0x00a93f74u   /* the activity manager's event channel */
#define STUNTS_CHANNEL           0x00a94304u   /* the stunt events' channel */
#define CROWD_LEVEL              0x009c3edcu   /* the crowd audio's level (int) */
#define CROWD_MIX                0x009c3ed8u   /* the crowd audio's mix (float) */
#define CROWD_MODE               0x009c47bcu   /* the crowd audio's current mode */
#define CROWD_MODE_NEXT          0x009c47c0u   /* the crowd audio's requested mode */
#define ENTITY_START_FREESTYLE   0x1011u       /* the start entity type of a Freestyle arena (the scene's 0x1011 entities) */

/* the session block's static initialiser (006de1f0 under bit 0 of 0x00dcf304) ran at startup (MXGame::Startup) */
static void session_static(void) {
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
}

/* fld dword; fstp dword: the bits, a signalling NaN quieted */
static uint32_t x87_copy(uint32_t bits) {
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x007fffffu) != 0) bits |= 0x00400000u;
    return bits;
}

/* a virtual call (thiscall) at the current stack depth */
static uint32_t vcall0(rx_addr obj, uint32_t slot) { return race_thiscall(race_vfn(obj, slot), obj, 0, 0, 0, 0, 0); }
static uint32_t vcall1(rx_addr obj, uint32_t slot, uint32_t a) { return race_thiscall(race_vfn(obj, slot), obj, 1, a, 0, 0, 0); }
/* a tail jump (`jmp [vtable + slot]`, no arguments): the core function registered at the target is called directly, so its entry
 * ESP is the caller's as in the original; an unregistered target (oracle only) is called one slot deeper */
static uint32_t vjump0(rx_addr obj, uint32_t slot) {
    typedef uint32_t (*fn_t)(rx_addr);
    fn_t f = RX_FN(race_vfn(obj, slot), fn_t);
    if (f) return f(obj);
    return vcall0(obj, slot);
}

/* an event with a 4-byte payload raised by the inlined raiser (the payload in the caller's frame at `payload`); the current stack
 * pointer is the raiser's ESP before the `push type node` of typeid().name() */
static void raise_activity(rx_addr channel, rx_addr name, rx_addr payload, rx_addr act) {
    SU32(payload, 0, act);
    act_raise(channel, name, TYPEDESC_ACTIVITY, payload, 4);
}

/* the clip recorder's playback (006aa8d0, cdecl type): the existing replay boundary (race_finish.c: oracle the original, product the
 * core's replay start) */
static void clip_start_playback(uint32_t type) {
#ifdef RX_ORACLE
    const uint32_t a[1] = {type};
    rx_call_original(0x006aa8d0, RX_CDECL, 0, 1, a);
#else
    RACE_CORE_CALL(1, rx_replay_start((int32_t)type));
#endif
}
static void clip_stop_playback(void) {
#ifdef RX_ORACLE
    rx_call_original(0x006aa990, RX_CDECL, 0, 0, 0);
#else
    RACE_CORE_CALL(0, rx_clip_recorder_stop_playback());
#endif
}

/* UI BOUNDARY: 0059b860 (EAX = the UI manager [0xa93fcc]) sets the hub routing's UI properties and raises "GotoHub" on the UI. It
 * writes the UI manager's property store and its event queue only (race_activity_stub.c, QUEUE.md "chain"); the product has no UI
 * manager. Oracle: the original; product: nothing. */
static void ui_goto_hub(void) {
#ifdef RX_ORACLE
    rx_regs in;
    memset(&in, 0, sizeof in);
    in.r[0] = U32(0, 0x00a93fcc);
    rx_call_original_ex(0x0059b860, &in, 0, 0, 0);
#endif
}

/* UI BOUNDARY: the event intro page: UIProperty_SetString("UIRoutingMessage", "GotoEventIntroPage") (005dbf50: EDI = key, ESI = UI)
 * and UIManager_QueueUIEvent("GotoHub", 1) (005db670): the UI's property store and event queue only (as rx_race_stub_vf25). */
static void ui_goto_event_intro(void) {
#ifdef RX_ORACLE
    const rx_addr ui = U32(0, 0x00a93fcc);
    const uint32_t value[1] = {0x00941f7c};                       /* "GotoEventIntroPage" */
    rx_regs in;
    memset(&in, 0, sizeof in);
    in.r[7] = 0x0094cc74;                                         /* "UIRoutingMessage" */
    in.r[6] = ui;
    rx_call_original_ex(0x005dbf50, &in, 0, 1, value);
    const uint32_t ev[2] = {0x009424ec /* "GotoHub" */, 1};
    rx_call_original(0x005db670, RX_THISCALL, ui, 2, ev);
#endif
}

/* FUN_0078fd60 (cdecl buffer, size, format, ...) with the variadic arguments pushed as the original's call */
static void format_text(rx_addr buf, uint32_t size, rx_addr fmt, int n, const uint32_t* va) {
    const rx_addr sp = rx_sp();
    const rx_addr args = sp - 4u * (uint32_t)(n + 1);
    SU32(args, 0, fmt);
    for (int i = 0; i < n; i++) SU32(args, 4u * (uint32_t)(i + 1), va[i]);
    rx_set_sp(args);
    rx_format_buffer(buf, size, fmt, args + 4);
    rx_set_sp(sp);
}

/* the first local human player of the game (the player slots [0xa946b8] + 0x7460, the order list +0xdf8c, count byte +0xdfbc):
 * the slot whose +0x10 is 0; 0 when the list is empty (-1 first) or has none */
static rx_addr first_local_human(void) {
    const rx_addr base = U32(0, 0x00a946b8) + 0x7460;
    if (I32(base, 0xdf8c) == -1) return 0;
    const uint32_t n = U8(base, 0xdfbc);
    for (uint32_t i = 0; i < n; i++) {
        const rx_addr slot = rx_seat_active(base, i);
        if (U32(slot, 0x10) == 0) return slot;
    }
    return 0;
}

/* the driver of that player in the activity (0055a050: its index in its list; the replay list +0x27c when the player is a
 * replay's (+0x908), else the drivers +0x268) */
static rx_addr local_human_driver(rx_addr A, rx_addr player) {
    int32_t index;
    RACE_CORE_CALL(0, index = rx_world_group_rider_index(A, player));
    const uint32_t list = U32(player, 0x908) != 0 ? U32(A, 0x27c) : U32(A, 0x268);
    return U32(list, 4 * (uint32_t)index);
}

/* ======================================================================================================================== */
/* 0078fcf0: a case-insensitive strstr                                                                                       */


/* ======================================================================================================================== */
/* the start                                                                                                                  */

/* 00562560 MXActivityFreestyleStub::vf2 (ECX = activity; arg; RET 4): Activity::vf2; an active activity (mode != 0): +0xff074 = 0,
 * the start position from the arena's start entities: the scene's entities of type 0x1011 (at most 12, 007d2920) -- when there are
 * none, the scene's groups 0x1014 then 0x1015 named after the activity (+0x118, 007d2820) -- the first one's transform (vf15 into a
 * 0x40-byte buffer, its translation at +0x30) or (0, 0, 0); the start slots from that count on (+0x340 + 0x30 i, i < 12) get that
 * position, x two metres further each. Frame: push ebp; and esp, -16; sub 0xc4; push ebx / esi / edi (the transform at +0x20, the
 * list at +0x68, 0x64 bytes cleared). */
void rx_freestyle_stub_vf2(rx_addr A, rx_addr arg) {
    RACE_ORIG_FRAME_ENTER(F, 0xc4, 3);
    RACE_CORE_CALL(1, rx_activity_vf2(A, arg));
    if (U32(A, 0x238) == 0) {
        RACE_ORIG_FRAME_LEAVE(F);
        return;
    }
    SU32(A, 0xff074, 0);
    const rx_addr scene = U32(U32(A, 0x234), 0x20138);
    const rx_addr list = F + 0x68;
    SU32(list, 0, 0);
    for (uint32_t i = 0; i < 0x60; i++) SU8(list + 4, i, 0);
    uint32_t count;
    RACE_CORE_CALL(4, count = rx_scene_manager_collect_typed(scene, ENTITY_START_FREESTYLE, list, 0xc, 0));
    if (count >= 0xc) {
        RACE_ORIG_FRAME_LEAVE(F);
        return;
    }
    SF32(F, 0x10, 0.0f);
    SF32(F, 0x14, 0.0f);
    SF32(F, 0x18, 0.0f);
    if (count == 0) {
        RACE_CORE_CALL(6, count = rx_scene_group_query(scene, 0x1014, list, 0xc, A + 0x118, 0));
        if (count == 0) RACE_CORE_CALL(6, count = rx_scene_group_query(scene, 0x1015, list, 0xc, A + 0x118, count));
    }
    float x, y, z;
    if (count != 0) {
        const rx_addr transform = F + 0x20;
        vcall1(U32(list, 0), 15, transform);                      /* the entity's world transform */
        x = F32(transform, 0x30);
        y = F32(transform, 0x34);
        z = F32(transform, 0x38);
    } else {
        x = F32(F, 0x10);
        y = F32(F, 0x14);
        z = F32(F, 0x18);
    }
    if (count < 0xc) {
        const float zero = 0.0f * 2.0f;                           /* xorps; mulss by 2.0 (0x92e330) */
        for (uint32_t i = count; i < 0xc; i++) {
            const rx_addr slot = A + 0x340 + 0x30 * i;
            SF32(slot, 4, y);
            const float next_y = zero + y;
            SF32(slot, 0, x);
            SF32(slot, 8, z);
            y = next_y;
            z = zero + z;
            x = x + 2.0f;
        }
    }
    RACE_ORIG_FRAME_LEAVE(F);
}
RX_FUNCTION(0x00562560, rx_freestyle_stub_vf2)

/* 00561a70 (ECX = activity): mode 0 (free practice): the 28 start slots in a line along the first start node's (+0x9c0 -> [0] ->
 * [0]) direction (+0x50: d = (+0x58, +0x54, -+0x50)): slot 0 at its position (+0x10) + 5 d, then 20 d further each; each slot's
 * direction (+0x10 of the slot) = the node's +0x50 .. +0x58 (x87 copies); +0x880 = 28. Frame: push esi. */
void rx_freestyle_start_line(rx_addr A) {
    const rx_addr node0 = U32(U32(U32(A, 0x9c0), 0), 0);
    const float dx = F32(node0, 0x58);
    const float dy = F32(node0, 0x54);
    const float dz = -0.0f - F32(node0, 0x50);
    float x = dx * 5.0f + F32(node0, 0x10);
    float y = dy * 5.0f + F32(node0, 0x14);
    float z = dz * 5.0f + F32(node0, 0x18);
    const float sx = dx * 20.0f;
    const float sy = dy * 20.0f;
    const float sz = dz * 20.0f;
    for (uint32_t i = 0; i < 0x1c; i++) {
        const rx_addr slot = A + 0x340 + 0x30 * i;
        SF32(slot, 0, x);
        SF32(slot, 4, y);
        SF32(slot, 8, z);
        const rx_addr node = U32(U32(U32(A, 0x9c0), 0), 0);
        SU32(slot, 0x10, x87_copy(U32(node, 0x50)));
        SU32(slot, 0x14, x87_copy(U32(node, 0x54)));
        SU32(slot, 0x18, x87_copy(U32(node, 0x58)));
        x = sx + x;
        y = sy + y;
        z = sz + z;
    }
    SU32(A, 0x880, 0x1c);
}
RX_FUNCTION(0x00561a70, rx_freestyle_start_line)

/* 00562800 (EDI = activity): modes 1 .. 3: the 12 start slots. The base: the start gate's frame when the activity has one (byte
 * +0x8c4: position +0x8a0, forward +0x8b0 .. +0x8b8, length +0x8c0: base = position - length * (+0x8b8, +0x8b4, -+0x8b0), step 5 m
 * along that direction), else (0, 200, 0) and the step (0, 1000, 0). Slot i: the start entity (type 0x1011, at most 25) whose index
 * (+0x90) is i + 1 gives the position (its transform +0x30) and the direction (+0x20), +0x36c = 1; without one the base + (i + 2)
 * steps and the gate's direction (x87 copies), +0x36c = 0. +0x880 = the number of start entities. Frame: push ebp; and esp, -16;
 * sub 0xe8; push ebx / esi (the transform at +0x40, the list at +0x88, 0x64 bytes cleared). */
void rx_freestyle_start_slots(rx_addr A) {
    RACE_ORIG_FRAME_ENTER(F, 0xe8, 2);
    float dx = 0.0f, dy = 200.0f, dz = 0.0f;                      /* 0x9c4750 */
    float bx = 0.0f, by = 200.0f, bz = 0.0f;
    if (U8(A, 0x8c4) != 0) {
        const float len = -0.0f - F32(A, 0x8c0);
        dx = F32(A, 0x8b8);
        dy = F32(A, 0x8b4);
        dz = -0.0f - F32(A, 0x8b0);
        bx = F32(A, 0x8a0) + dx * len;
        by = F32(A, 0x8a4) + dy * len;
        bz = F32(A, 0x8a8) + dz * len;
    }
    const float sx = dx * 5.0f;
    const float sy = dy * 5.0f;
    const float sz = dz * 5.0f;
    SF32(F, 0x28, sx);
    SF32(F, 0x2c, sy);
    SF32(F, 0xc, sz);
    float px = sx + bx;
    float py = sy + by;
    float pz = sz + bz;
    SF32(F, 0x10, px);
    SF32(F, 0x14, py);
    SF32(F, 0x18, pz);
    const rx_addr list = F + 0x88;
    SU32(list, 0, 0);
    for (uint32_t i = 0; i < 0x60; i++) SU8(list + 4, i, 0);
    const rx_addr scene = U32(U32(A, 0x234), 0x20138);
    uint32_t count;
    RACE_CORE_CALL(4, count = rx_scene_manager_collect_typed(scene, ENTITY_START_FREESTYLE, list, 0x19, 0));
    SF32(F, 0x30, sx);
    SF32(F, 0x34, sy);
    SF32(F, 0x38, sz);
    SU32(F, 8, count);
    for (uint32_t i = 0; i < 0xc; i++) {
        rx_addr entity = 0;
        int found = 0;
        for (uint32_t k = 0; k < count && !found; k++) {
            entity = U32(list, 4 * k);
            if (U32(entity, 0x90) == i + 1) found = 1;
        }
        px = F32(F, 0x30) + px;
        py = F32(F, 0x34) + py;
        pz = F32(F, 0x38) + pz;
        SF32(F, 0x10, px);
        SF32(F, 0x14, py);
        SF32(F, 0x18, pz);
        const rx_addr slot = A + 0x340 + 0x30 * i;
        if (found) {
            const rx_addr transform = F + 0x40;
            vcall1(entity, 15, transform);
            SU32(slot, 0, U32(transform, 0x30));
            SU32(slot, 4, U32(transform, 0x34));
            SU32(slot, 8, U32(transform, 0x38));
            SU32(slot, 0x10, U32(transform, 0x20));
            SU32(slot, 0x14, U32(transform, 0x24));
            SU32(slot, 0x18, U32(transform, 0x28));
            SU32(slot, 0x2c, 1);
        } else {
            SF32(slot, 0, px);
            SF32(slot, 4, py);
            SF32(slot, 8, pz);
            SU32(slot, 0x10, x87_copy(U32(A, 0x8b0)));
            SU32(slot, 0x14, x87_copy(U32(A, 0x8b4)));
            SU32(slot, 0x18, x87_copy(U32(A, 0x8b8)));
            SU32(slot, 0x2c, 0);
        }
    }
    SU32(A, 0x880, count);
    RACE_ORIG_FRAME_LEAVE(F);
}
RX_FUNCTION(0x00562800, rx_freestyle_start_slots)

/* 00561990 MXActivityFreestyle::vf2 (ECX = activity; arg; RET 4): the stub's start; +0x224 = 0; the crowd handlers on the stunts
 * channel ("StuntBegin" 00562220 / "StuntFailed" 00562250: the template 00587930, proxy 0x945924; "PhysicsStuntBegin" 005621f0:
 * 005879f0, proxy 0x945944); mode 3: +0x224 = 1; "Ghost Session Completed" on the activity's channel -> 00561f80 (00587ab0, proxy
 * 0x945964); the start slots (mode 0: 00561a70, modes 1 .. 3: 00562800); an active activity raises "Activity Init" on the activity
 * manager's channel. Frame: push ebp; and esp, -8; sub 8; push esi / edi (the payload at +0xc). */
void rx_freestyle_vf2(rx_addr A, rx_addr arg) {
    RACE_FRAME_EBP(F, 8, 8, 2);
    RACE_CORE_CALL(1, rx_freestyle_stub_vf2(A, arg));
    SU32(A, 0x224, 0);
    ACT_CALL(2, act_subscribe(0x00587930, 8, 2, 3, U32(0, STUNTS_CHANNEL), STR_STUNT_BEGIN, 0x00945924, 0x009cbf38, A, 0x00562220));
    ACT_CALL(2, act_subscribe(0x00587930, 8, 2, 3, U32(0, STUNTS_CHANNEL), STR_STUNT_FAILED, 0x00945924, 0x009cbf38, A, 0x00562250));
    ACT_CALL(1, act_subscribe(0x005879f0, 0, 1, 4, U32(0, STUNTS_CHANNEL), STR_PHYSICS_STUNT_BEGIN, 0x00945944, 0x009ce768, A, 0x005621f0));
    if (U32(A, 0x238) == 3) SU32(A, 0x224, 1);
    ACT_CALL(1, act_subscribe(0x00587ab0, 1, 1, 4, U32(A, 0x290), STR_GHOST_COMPLETED, 0x00945964, 0x009cda18, A, 0x00561f80));
    switch (U32(A, 0x238)) {
    case 0:
        RACE_CORE_CALL(0, rx_freestyle_start_line(A));
        break;
    case 1:
    case 2:
    case 3:
        RACE_CORE_CALL(0, rx_freestyle_start_slots(A));
        break;
    default:
        break;
    }
    if (U32(A, 0x238) != 0) raise_activity(U32(0, ACTIVITY_MANAGER_CHANNEL), STR_ACTIVITY_INIT, F + 0xc, A);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00561990, rx_freestyle_vf2)

/* 00562440 MXActivityFreestyleStub::vf5 (ECX = activity): Activity::vf5; the Freestyle types (5, 0x14, 0x3a .. 0x3c; 0x20 counts as 5)
 * load the track data of kind 6 into the activity's block +0x890 (00536ef0); an active activity of a non-championship type (mode not
 * 0 / 2, type < 0x22) gets its track name (+0xed4, 0055a5d0 with the type) and the world's track (007677e0); "Activity Basic
 * Essentials Loaded" on the activity manager's channel. Frame: push ebp; and esp, -8; push ecx / ebx / esi / edi. */
void rx_freestyle_stub_vf5(rx_addr A) {
    RACE_FRAME_EBP(F, 8, 0, 4);
    RACE_CORE_CALL(0, rx_activity_vf5(A));
    uint32_t type = U32(A, 0x23c);
    if (type == 0x20) type = 5;
    if (type == 5 || type == 0x14 || type == 0x3a || type == 0x3b || type == 0x3c) {
        const rx_addr w = U32(A, 0x234);
        RACE_CORE_CALL(3, (void)rx_track_load(w + 0x20030, A + 0x890, 6, 0));
    }
    const uint32_t mode = U32(A, 0x238);
    const uint32_t t = U32(A, 0x23c);
    if (mode != 2 && mode != 0 && (t < 0x1f || t <= 0x21)) {
        RACE_CORE_CALL(1, rx_activity_track_name(A + 0xed4, type, A));
        RACE_CORE_CALL(2, rx_mxworld_set_track(U32(A, 0x234), A + 0xed4, U32(A, 0xbb0) != 0 ? A + 0x890 : 0));
    }
    raise_activity(U32(0, ACTIVITY_MANAGER_CHANNEL), STR_BASICS_LOADED, F + 0xc, A);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00562440, rx_freestyle_stub_vf5)

/* 005626c0 MXActivityFreestyleStub::vf18 (ECX = activity): the finish scenario's state (+0x114234) = 0xf, the results scenario's
 * (+0x129404) = 0; a jump to Activity::vf18 */
void rx_freestyle_stub_vf18(rx_addr A) {
    SU32(A, 0x114234, 0xf);
    SU32(A, 0x129404, 0);
    rx_activity_vf18(A);
}
RX_FUNCTION(0x005626c0, rx_freestyle_stub_vf18)

/* 00562ac0 MXActivityFreestyleStub::vf44 (ECX = activity; player; RET 4) -> a new FreestyleDriver (0xf50 bytes, 0050c900) or 0 */
rx_addr rx_freestyle_stub_vf44(rx_addr A, rx_addr player) {
    (void)A;
    rx_addr d;
    RACE_CORE_CALL(1, d = rx_reflex_aligned_alloc(0xf50));
    if (d == 0) return 0;
    rx_addr r;
    RACE_CORE_CALL(0, r = rx_freestyle_driver_construct(d, player));
    return r;
}
RX_FUNCTION(0x00562ac0, rx_freestyle_stub_vf44)

/* 00562d40 MXActivityFreestyleStub::vf37 (ECX = activity; the old state; RET 4): Pre enter: Activity::vf37; each driver: an AI's
 * vehicle reset (0050abe0, unless 0xae1b90), a remote / AI one's vehicle audio slot released (00680d10 with [0xd92f00]); the
 * podium setup (0074ada0); with no scene lock (world +0x20018 = 0), 0xd60ff0 set, 0xae1b90 clear and no area flag (+0xfe8): the Pre
 * countdown +0x13e5f0 = 2. Frame: push ebx / edi (esi inside the loop). */
void rx_freestyle_stub_vf37(rx_addr A, uint32_t old) {
    (void)old;
    RACE_FRAME_PLAIN(F, 0, 2);
    RACE_CORE_CALL(1, rx_activity_vf37(A));
    if (I32(A, 0x26c) > 0) {
        rx_set_sp(F - 4);                                         /* push esi */
        for (int32_t i = 0; i < I32(A, 0x26c); i++) {
            const rx_addr d = U32(U32(A, 0x268), 4 * (uint32_t)i);
            if (U32(0, 0x00ae1b90) == 0 && U32(U32(d, 0x74), 0x10) == 1) RACE_CORE_CALL(0, rx_driver_reset_vehicle(d));
            const rx_addr p = U32(d, 0x74);
            if (U32(p, 4) == 0 || U32(p, 0x10) != 0) RACE_CORE_CALL(1, rx_vehicle_audio_release(U32(p, 0x1ac), U32(0, 0x00d92f00)));
        }
        rx_set_sp(F);
    }
    RACE_CORE_CALL(0, rx_podium_setup(A, U32(A, 0x234) + 0x20150));
    if (U32(U32(A, 0x234), 0x20018) == 0 && U32(0, 0x00d60ff0) != 0 && U32(0, 0x00ae1b90) == 0 && U32(A, 0xfe8) == 0)
        SU32(A, 0x13e5f0, 2);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00562d40, rx_freestyle_stub_vf37)

/* 005620e0 MXActivityFreestyle::vf52 (ECX = activity): the crowd audio by mode: 1: mix 1.0, level 5, mode 2; 2: mix 0.5, level 0,
 * mode 1 (only requested when the current mode is not 1); 3: mix 0, level 0, mode -1. The requested mode (0x9c47c0) is written only
 * when it differs from the current one (0x9c47bc). */
void rx_freestyle_vf52(rx_addr A) {
    uint32_t mode_next;
    switch (U32(A, 0x238)) {
    case 1:
        SU32(A, 0x13e608, 5);
        SU32(0, CROWD_LEVEL, 5);
        SF32(0, CROWD_MIX, 1.0f);
        mode_next = 2;
        break;
    case 2:
        SU32(A, 0x13e608, 0);
        SU32(0, CROWD_LEVEL, 0);
        SF32(0, CROWD_MIX, 0.5f);
        if (U32(0, CROWD_MODE) != 1) SU32(0, CROWD_MODE_NEXT, 1);
        return;
    case 3:
        SU32(A, 0x13e608, 0);
        SU32(0, CROWD_LEVEL, 0);
        SF32(0, CROWD_MIX, 0.0f);
        mode_next = 0xffffffffu;
        break;
    default:
        return;
    }
    if (U32(0, CROWD_MODE) != mode_next) SU32(0, CROWD_MODE_NEXT, mode_next);
}
RX_FUNCTION(0x005620e0, rx_freestyle_vf52)

/* ======================================================================================================================== */
/* the crowd: the stunt handlers                                                                                              */

/* 00562220 (ECX = activity; player, stunt; RET 8): "StuntBegin": a local player's (+0x10 == 0) trick fills the crowd meter: the level
 * (+0x13e608 and 0x9c3edc) = the peak (+0x13e604), the timer +0x13e600 = 4 s */
void rx_freestyle_stunt_begin(rx_addr A, rx_addr player, rx_addr stunt) {
    (void)stunt;
    if (U32(player, 0x10) != 0) return;
    const uint32_t peak = U32(A, 0x13e604);
    SU32(A, 0x13e608, peak);
    SU32(0, CROWD_LEVEL, peak);
    SF32(A, 0x13e600, 4.0f);                                      /* 0x9322c0 */
}
RX_FUNCTION(0x00562220, rx_freestyle_stunt_begin)

/* 00562250 (ECX = activity; player, stunt; RET 8): "StuntFailed": a local player's failed trick: level 50, timer 2 s */
void rx_freestyle_stunt_failed(rx_addr A, rx_addr player, rx_addr stunt) {
    (void)stunt;
    if (U32(player, 0x10) != 0) return;
    SU32(A, 0x13e608, 0x32);
    SU32(0, CROWD_LEVEL, 0x32);
    SF32(A, 0x13e600, 2.0f);                                      /* 0x92e330 */
}
RX_FUNCTION(0x00562250, rx_freestyle_stunt_failed)

/* 005621f0 (ECX = activity; player, stunt, type; RET 0xc): "PhysicsStuntBegin": as "StuntBegin" */
void rx_freestyle_physics_stunt_begin(rx_addr A, rx_addr player, rx_addr stunt, uint32_t type) {
    (void)stunt;
    (void)type;
    if (U32(player, 0x10) != 0) return;
    const uint32_t peak = U32(A, 0x13e604);
    SU32(A, 0x13e608, peak);
    SU32(0, CROWD_LEVEL, peak);
    SF32(A, 0x13e600, 4.0f);
}
RX_FUNCTION(0x005621f0, rx_freestyle_physics_stunt_begin)

/* 00561e30 MXActivityFreestyle::vf28 (ECX = activity; dt; RET 4): InProgress: Activity::vf28; the track debug (005399e0 when +0xb8c);
 * while the crowd timer +0x13e600 >= 0 the level decays with it: level = (int)((peak - 5) * (timer * 0.25) + 5) (+0x13e608 and
 * 0x9c3edc), the timer -= dt. Frame: push ebp; and esp, -8; push ecx; push esi. */
void rx_freestyle_vf28(rx_addr A, float dt) {
    RACE_FRAME_EBP(F, 8, 0, 2);
    RACE_CORE_CALL(1, rx_activity_in_progress(A, dt));
    if (U32(A, 0xb8c) != 0) RACE_CORE_CALL(0, rx_waypoint_track_debug(A + 0x890));
    const float timer = F32(A, 0x13e600);
    if (timer >= 0.0f) {
        const float scaled = timer * 0.25f;                       /* 0x92ff84 */
        const float rest = rx_subs(timer, dt);
        const float level = (float)(I32(A, 0x13e604) - 5) * scaled + 5.0f;
        const int32_t level_int = rx_cvtt(level);
        SU32(A, 0x13e608, (uint32_t)level_int);
        SU32(0, CROWD_LEVEL, (uint32_t)level_int);
        SF32(A, 0x13e600, rest);
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00561e30, rx_freestyle_vf28)

/* 00561dc0 MXActivityFreestyle::vf38 (ECX = activity; the old state; RET 4): Countdown enter: Activity::vf38; the countdown's length
 * +0x29c: 0 for modes 0 / 2, 5 s for mode 3 */
void rx_freestyle_vf38(rx_addr A, uint32_t old) {
    (void)old;
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(1, rx_activity_countdown_enter(A));
    const uint32_t mode = U32(A, 0x238);
    if (mode == 0 || mode == 2) SF32(A, 0x29c, 0.0f);
    else if (mode == 3) SF32(A, 0x29c, 5.0f);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00561dc0, rx_freestyle_vf38)

/* 005620d0 MXActivityFreestyle::vf49 (ECX = activity; arg; RET 4): the activity clock +0x294 = 0 */
void rx_freestyle_vf49(rx_addr A, uint32_t arg) {
    (void)arg;
    SF32(A, 0x294, 0.0f);
}
RX_FUNCTION(0x005620d0, rx_freestyle_vf49)

/* 00562170 MXActivityFreestyle::vf46 (ECX = activity; kind, on, value; RET 0xc): mode 3 only: with `on` the start state +0x228 = 1 and
 * the ghost driver (player +0x10 == 2) gets the value as its score to beat (+0xf44 = (int)value) and +0x20 = 1; +0x224 = 0 */
void rx_freestyle_vf46(rx_addr A, uint32_t kind, uint32_t on, float value) {
    (void)kind;
    if (U32(A, 0x238) != 3) return;
    if (on != 0) {
        SU32(A, 0x228, 1);
        for (int32_t i = 0; i < I32(A, 0x26c); i++) {
            const rx_addr d = U32(U32(A, 0x268), 4 * (uint32_t)i);
            if (U32(U32(d, 0x74), 0x10) == 2) {
                SU32(d, 0xf44, (uint32_t)rx_cvtt(value));
                SU32(d, 0x20, 1);
                break;
            }
        }
    }
    SU32(A, 0x224, 0);
}
RX_FUNCTION(0x00562170, rx_freestyle_vf46)

/* ======================================================================================================================== */
/* the frame                                                                                                                  */

/* 00562ce0 MXActivityFreestyleStub::vf23 (ECX = activity; dt; RET 4): Activity::vf23; the intro and the finish scenarios (+0xff080,
 * +0x114250) each get their vf14 (0x38) while their vf8 (0x20, done) is 0. Frame: push esi / edi (the dt copy pushed for the call). */
void rx_freestyle_stub_vf23(rx_addr A, float dt) {
    RACE_FRAME_PLAIN(F, 0, 2);
    RACE_CORE_CALL(1, rx_activity_update(A, dt));
    const rx_addr intro = A + 0xff080;
    if (vcall0(intro, 8) == 0) vcall0(intro, 14);
    const rx_addr finish = A + 0x114250;
    if (vcall0(finish, 8) == 0) vcall0(finish, 14);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00562ce0, rx_freestyle_stub_vf23)

/* 00562e00 MXActivityFreestyleStub::vf25 (ECX = activity; dt; RET 4): Pre. +0xff074 (vf48: the intro is over) = 1 with the area flag
 * (+0xfe8), the AI debug flag 0xae1b90 or when the activity cannot start yet (vf11 == 0); Activity::vf25; then, unless the three
 * load counters (0xa81ad0 / 0xa81ad8 / 0xa81ac8) add up above 0 or the rules popup holds (+0xff068): when the activity cannot start
 * (vf11 == 0): the finish scenario's state (+0x114234) = 0, +0xff074 = 1, the level cast's cameras reset (0074b480), the world's
 * +0x203e0 entity hidden, the cast stopped (0074b450 / 0074b4f0); the Pre countdown +0x13e5f0 decremented and when it ran out and
 * the intro scenario is done (vf7) without the area / AI flags: the intro page (00747810 started: UI) and +0xff074 = 1; while the
 * intro scenario is not done, its update (007473f0). Frame: push ebp; and esp, -8; push ecx / ebx / esi / edi. */
void rx_freestyle_stub_vf25(rx_addr A, float dt) {
    RACE_FRAME_EBP(F, 8, 0, 4);
    if (U32(A, 0xfe8) != 0 || U32(0, 0x00ae1b90) != 0 || vcall0(A, 11) == 0) SU32(A, 0xff074, 1);
    RACE_CORE_CALL(1, rx_activity_pre(A, dt));
    const int32_t busy = I32(0, 0x00a81ad8) + I32(0, 0x00a81ad0) + I32(0, 0x00a81ac8);
    if (busy > 0 || U32(A, 0xff068) != 0) {
        RACE_FRAME_END(F);
        return;
    }
    if (vcall0(A, 11) == 0) {
        SU32(A, 0x114234, 0);
        SU32(A, 0xff074, 1);
        RACE_CORE_CALL(0, rx_scenario_anims_reset_cameras(U32(A, 0x234) + 0x20150));
        const rx_addr hidden = U32(U32(A, 0x234), 0x203e0);
        if (hidden != 0) {
            SU8(hidden, 0xa2, 0);
            SU32(hidden, 0x70, 0);
        }
        RACE_CORE_CALL(0, rx_scenario_anims_stop_actors(U32(A, 0x234) + 0x20150));
        RACE_CORE_CALL(0, rx_scenario_cast_stop_all(U32(A, 0x234) + 0x20150));
    }
    const rx_addr intro = A + 0xff080;
    const int32_t countdown = I32(A, 0x13e5f0);
    SU32(A, 0x13e5f0, (uint32_t)(countdown - 1));
    if (countdown <= 0 && vcall0(intro, 7) != 0 && U32(A, 0xfe8) == 0 && U32(0, 0x00ae1b90) == 0) {
        uint32_t started;
        RACE_CORE_CALL(0, started = rx_scenario_start(intro));
        if (started != 0) ui_goto_event_intro();
        SU32(A, 0xff074, 1);
    }
    if (vcall0(intro, 7) == 0) RACE_CORE_CALL(2, rx_scenario_intro_update(intro, dt));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00562e00, rx_freestyle_stub_vf25)

/* 00561e10 MXActivityFreestyle::vf25 (ECX = activity; dt; RET 4): the stub's Pre unless the start waits (+0x224) */
void rx_freestyle_vf25(rx_addr A, float dt) {
    if (U32(A, 0x224) == 0) RACE_CORE_CALL(1, rx_freestyle_stub_vf25(A, dt));
}
RX_FUNCTION(0x00561e10, rx_freestyle_vf25)

/* 00563d00 MXActivityFreestyleStub::vf8 (ECX = activity) -> EAX: whether the current scenario may be skipped: in Pre (state 0) 1 when
 * the intro scenario is skippable (vf11) or done (vf7) and the UI flag +0x1068 is clear; in Post (state 4) the finish scenario's vf11
 * (sub-state 1, a tail jump), 0 (sub-state 2) or the results scenario's vf11 (sub-state 3); else 0. Frame: push esi (edi in Pre). */
uint32_t rx_freestyle_stub_vf8(rx_addr A) {
    const rx_addr entry = rx_sp();
    const uint32_t state = U32(A, 0x21c);
    if (state == 0) {
        rx_set_sp(entry - 8);                                     /* push esi; push edi */
        const rx_addr intro = A + 0xff080;
        uint32_t r = 0;
        if ((vcall0(intro, 11) != 0 || vcall0(intro, 7) != 0) && U32(A, 0x1068) == 0) r = 1;
        rx_set_sp(entry);
        return r;
    }
    if (state != 4) return 0;
    switch (U32(A, 0x13e5f8)) {
    case 1: return vjump0(A + 0x114250, 11);
    case 2: return 0;
    case 3: return vjump0(A + 0x129420, 11);
    default: return 0;
    }
}
RX_FUNCTION(0x00563d00, rx_freestyle_stub_vf8)

/* 0055ec10 MXActivityFreestyleStub::vf9 (ECX = activity) -> EAX: a tail jump to the finish scenario's vf9 (0x24) */
uint32_t rx_freestyle_stub_vf9(rx_addr A) { return vjump0(A + 0x114250, 9); }
RX_FUNCTION(0x0055ec10, rx_freestyle_stub_vf9)

/* 0055ec00 MXActivityFreestyleStub::vf48 (ECX = activity) -> EAX: +0xff074 (the intro is over) */
uint32_t rx_freestyle_stub_vf48(rx_addr A) { return U32(A, 0xff074); }
RX_FUNCTION(0x0055ec00, rx_freestyle_stub_vf48)

/* 00562bd0 MXActivityFreestyleStub::vf42 (ECX = activity): the time limit of an active non-championship-free mode (mode 1, or above 3):
 * 5 s before the Duration (+0x2bc) "FreestyleEnd" is raised on the activity's channel each frame; once the clock (+0x294) reaches the
 * Duration -- offline, or online with the session's result given (0x1c48) or the activity's result flag (+0xbc0) -- the activity
 * winds down (SetState 3) and every driver is done (+0x70 = 1). Frame: push ecx / esi (ebx; ebp / edi around the raise). */
void rx_freestyle_stub_vf42(rx_addr A) {
    RACE_FRAME_PLAIN(F, 0, 2);
    const uint32_t mode = U32(A, 0x238);
    if (mode == 0 || (mode >= 2 && mode <= 3)) {
        RACE_FRAME_END(F);
        return;
    }
    rx_set_sp(F - 4);                                             /* push ebx */
    const float warn = F32(A, 0x2bc) - 5.0f;
    if (F32(A, 0x294) >= warn) {
        rx_set_sp(F - 12);                                        /* push ebp; push edi */
        raise_activity(U32(A, 0x290), STR_FREESTYLE_END, F + 4, A);
        rx_set_sp(F - 4);
    }
    session_static();
    const rx_addr session = U32(0, 0x00da6cb0);
    int offline_or_decided = 1;
    if (session != 0 && U32(session, 0x1c4c) != 0 && U32(session, 0x1b0) != 1) {
        /* an online session: 006de1c0's session (+0x10) must have the result (+0x1c48), or the activity its result flag */
        act_unreached(0x00562c79, "the Freestyle time limit of an online session (006de1c0)");
        offline_or_decided = U32(A, 0xbc0) != 0;
    }
    if (offline_or_decided && F32(A, 0x294) >= F32(A, 0x2bc)) {
        RACE_CORE_CALL(2, rx_activity_set_state(A, 3));
        for (int32_t i = 0; i < I32(A, 0x26c); i++) SU32(U32(U32(A, 0x268), 4 * (uint32_t)i), 0x70, 1);
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00562bd0, rx_freestyle_stub_vf42)

/* 00562af0 MXActivityFreestyleStub::vf43 (ECX = activity; a, b, tie; RET 0xc) -> EAX: the standings' order of two drivers: by score
 * (InProgress: the sum of the four trick categories +0xc74 .. +0xc80 of the player; else the player's score +0xcec), higher first
 * (+1 when b's is higher or unordered, -1 when lower); equal scores by +0x1088 (b - a); then, with `tie`, +0x1dc (a >= b: +1, else -1) */
int32_t rx_freestyle_stub_vf43(rx_addr A, rx_addr a, rx_addr b, uint32_t tie) {
    const uint32_t state = U32(A, 0x21c);
    const rx_addr pa = U32(a, 0x74);
    const rx_addr pb = U32(b, 0x74);
    float sa, sb;
    if (state == 2) sa = ((F32(pa, 0xc80) + F32(pa, 0xc7c)) + F32(pa, 0xc78)) + F32(pa, 0xc74);
    else sa = F32(pa, 0xcec);
    if (state == 2) sb = ((F32(pb, 0xc80) + F32(pb, 0xc7c)) + F32(pb, 0xc78)) + F32(pb, 0xc74);
    else sb = F32(pb, 0xcec);
    const float diff = sb - sa;
    if (!(diff == 0.0f)) return diff < 0.0f ? -1 : 1;
    const int32_t ka = I32(pa, 0x1088);
    const int32_t kb = I32(pb, 0x1088);
    if (ka != kb) return kb - ka;
    if (tie == 0) return 0;
    return I32(pa, 0x1dc) >= I32(pb, 0x1dc) ? 1 : -1;
}
RX_FUNCTION(0x00562af0, rx_freestyle_stub_vf43)

/* 005626e0 MXActivityFreestyleStub::vf14 (ECX = activity; driver, place; RET 8): InProgress only: the driver is moved to the place
 * (at most drivers - 1) in the standings +0x2d0 (swapped with the one there); the drivers' scores (player +0xcec and +0x10a8) set to
 * 100, 95, 90 .. in standings order; the standings sorted (00559eb0); +0xff070 = 1 (decided); WindDown (SetState 3). Frame: push esi
 * (edi for the search). */
void rx_freestyle_stub_vf14(rx_addr A, rx_addr driver, int32_t place) {
    RACE_FRAME_PLAIN(F, 0, 1);
    if (U32(A, 0x21c) != 2) {
        RACE_FRAME_END(F);
        return;
    }
    const int32_t n = I32(A, 0x26c);
    if (place >= n - 1) place = n - 1;
    int32_t at = 0;
    while (at < n && rx_stand(A, (uint32_t)at) != driver) at++;
    const uint32_t there = rx_stand(A, (uint32_t)place);
    rx_stand_set(A, (uint32_t)place, driver);
    rx_stand_set(A, (uint32_t)at, there);
    float score = 100.0f;                                         /* 0x93005c */
    for (int32_t i = 0; i < I32(A, 0x26c); i++) {
        const rx_addr scorer = U32(rx_stand(A, (uint32_t)i), 0x74) + 0xc70;
        SF32(scorer, 0x7c, score);
        SF32(scorer, 0x438, score);
        score = score - 5.0f;
    }
    RACE_CORE_CALL(0, rx_activity_sort_standings(A));
    SU32(A, 0xff070, 1);
    RACE_CORE_CALL(2, rx_activity_set_state(A, 3));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x005626e0, rx_freestyle_stub_vf14)

/* 00563820 MXActivityFreestyleStub::vf19 (ECX = activity; player, position, out position, out up; RET 0x10) -> EAX: the respawn point:
 * the start entity (type 0x1011, at most 25) nearest to the player's vehicle (+0x1ac +0xc0) -- its position into `position` (vf17),
 * its transform's translation (vf14 -> +0x20) into `out position`, (0, 1, 0) into `out up`, 1; 0 without a vehicle or an entity.
 * Frame: push ebp; and esp, -16; sub 0x94; push ebx / esi / edi (the vehicle's position at +0x10, the best distance at +0xc, the
 * entity's position at +0x20, the list at +0x38, 0x64 bytes cleared). */
uint32_t rx_freestyle_stub_vf19(rx_addr A, rx_addr player, rx_addr pos, rx_addr out_pos, rx_addr out_up) {
    RACE_ORIG_FRAME_ENTER(F, 0x94, 3);
    const rx_addr vehicle = U32(player, 0x1ac);
    if (vehicle == 0) {
        RACE_ORIG_FRAME_LEAVE(F);
        return 0;
    }
    SU32(F, 0x10, U32(vehicle, 0xc0));
    SU32(F, 0x14, U32(vehicle, 0xc4));
    SU32(F, 0x18, U32(vehicle, 0xc8));
    SF32(F, 0xc, 3.4028234663852886e+38f);                        /* FLT_MAX (0x977494) */
    const rx_addr list = F + 0x38;
    SU32(list, 0, 0);
    for (uint32_t i = 0; i < 0x60; i++) SU8(list + 4, i, 0);
    const rx_addr scene = U32(U32(A, 0x234), 0x20138);
    uint32_t count;
    RACE_CORE_CALL(4, count = rx_scene_manager_collect_typed(scene, ENTITY_START_FREESTYLE, list, 0x19, 0));
    if (count == 0 || (int32_t)count <= 0) {
        RACE_ORIG_FRAME_LEAVE(F);
        return 0;
    }
    rx_addr best = 0;
    for (int32_t i = 0; i < (int32_t)count; i++) {
        const rx_addr e = U32(list, 4 * (uint32_t)i);
        vcall1(e, 17, F + 0x20);                                  /* the entity's position */
        const float dy = F32(F, 0x14) - F32(F, 0x24);
        const float dx = F32(F, 0x10) - F32(F, 0x20);
        const float dz = F32(F, 0x18) - F32(F, 0x28);
        const float d = (dz * dz + dy * dy) + dx * dx;
        if (F32(F, 0xc) > d) {
            best = e;
            SF32(F, 0xc, d);
        }
    }
    if (best == 0) {
        RACE_ORIG_FRAME_LEAVE(F);
        return 0;
    }
    vcall1(best, 17, pos);
    const rx_addr t = vcall0(best, 14);                           /* the entity's transform */
    SU32(out_pos, 4, U32(t, 0x24));
    SU32(out_pos, 0, x87_copy(U32(t, 0x20)));
    SU32(out_pos, 8, U32(t, 0x28));
    SF32(out_up, 0, 0.0f);
    SF32(out_up, 4, 1.0f);
    SF32(out_up, 8, 0.0f);
    RACE_ORIG_FRAME_LEAVE(F);
    return 1;
}
RX_FUNCTION(0x00563820, rx_freestyle_stub_vf19)

/* 00563380 MXActivityFreestyleStub::vf61 (ECX = activity) -> EAX: 0 while the clip recorder plays the clip kind 9
 * ([0xa942b4] == 1 and [0xa959a8] == 9), else 1 */
uint32_t rx_freestyle_stub_vf61(rx_addr A) {
    (void)A;
    if (U32(0, 0x00a942b4) == 1 && U32(0, 0x00a959a8) == 9) return 0;
    return 1;
}
RX_FUNCTION(0x00563380, rx_freestyle_stub_vf61)

/* 00563ce0 MXActivityFreestyleStub::vf53 (ECX = activity; which; RET 4): which == 0 starts the scripted sequence
 * "StartLineScript_FMX" (0055e100) */
void rx_freestyle_stub_vf53(rx_addr A, uint32_t which) {
    if (which != 0) return;
    RACE_CORE_CALL(3, (void)rx_activity_start_sequence(A, STR_START_LINE_SCRIPT, 0));
}
RX_FUNCTION(0x00563ce0, rx_freestyle_stub_vf53)

/* ======================================================================================================================== */
/* the states                                                                                                                 */

/* 00562f70 MXActivityFreestyleStub::vf39 (ECX = activity; the old state; RET 4): InProgress enter: +0xff070 = 0; Activity::vf39; the
 * clip recorder starts recording this activity (0xd5a9a4); five HUD flags (0xa7f384 0, 0xa7f390 1, 0xa7f398 0, 0xa7f3a0 1, 0xa7f3a4
 * 1); the first local human's driver offered to the clip recorder (kind 8, 0 .. 5 s); without the area flag the start-line script
 * (vf53 (0)). Frame: push ebx (esi around the player search). */
void rx_freestyle_stub_vf39(rx_addr A, uint32_t old) {
    RACE_FRAME_PLAIN(F, 0, 1);
    SU32(A, 0xff070, 0);
    RACE_CORE_CALL(1, rx_activity_in_progress_enter(A));
    (void)old;
    if (U32(0, 0x00d5a9a4) != 0) RACE_CORE_CALL(1, rx_clip_recorder_start_recording(A));
    SU32(0, 0x00a7f384, 0);
    SU32(0, 0x00a7f390, 1);
    SU32(0, 0x00a7f398, 0);
    SU32(0, 0x00a7f3a0, 1);
    SU32(0, 0x00a7f3a4, 1);
    rx_set_sp(F - 4);                                             /* push esi */
    const rx_addr player = first_local_human();
    const rx_addr d = local_human_driver(A, player);
    rx_set_sp(F);
    RACE_CORE_CALL(2, rx_world_clip_recorder_candidate(8, d, 0.0f, 5.0f));
    if (U32(A, 0xfe8) == 0) vcall1(A, 53, 0);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00562f70, rx_freestyle_stub_vf39)

/* 005633a0 MXActivityFreestyleStub::vf40 (ECX = activity; the old state; RET 4): WindDown enter: Activity::vf40; mode 1: the
 * wind-down's length +0x2c4 = 5 s and the finish message raised ("Player Wins" 00587d00 with the text): offline "<rider> STR_WINS"
 * ("%s %s": the leader's player +0x54 and the localized STR_WINS), online the localized STR_FINISHED; the first local human's
 * driver offered to the clip recorder (kind 9, the recorded activity's clock - 3 s .. + 2 s; 0x9c40f0 / 0x9c40f4). The text goes
 * to the HUD only (the event's subscribers are UI components): as rx_race_stub_check_finish it is made with the localization the
 * core has. Frame: sub 0x208 (the security cookie at +0x204, the UTF-8 text at +0x10, the message at +0x110); push ebx / ebp / esi
 * / edi. */
void rx_freestyle_stub_vf40(rx_addr A, uint32_t old) {
    const rx_addr entry = rx_sp();
    RACE_FRAME_PLAIN(F, 0x208, 4);
    SU32(entry, -4, U32(0, 0x009b45e4) ^ (entry - 0x208));        /* the /GS cookie */
    (void)old;
    RACE_CORE_CALL(1, rx_activity_wind_down_enter(A));
    if (U32(A, 0x238) == 1) {
        SF32(A, 0x2c4, 5.0f);
        session_static();
        const rx_addr session = U32(0, 0x00da6cb0);
        const int online = session != 0 && U32(session, 0x1c4c) != 0 && U32(session, 0x1b0) != 1;
        const rx_addr utf8 = F + 0x10;
        const rx_addr text = F + 0x110;
#ifdef RX_ORACLE
        rx_addr language;
        RACE_CORE_CALL(1, language = rx_ui_language_find(0));      /* 005dfc90 */
        rx_addr entry_text;
        RACE_CORE_CALL(0, entry_text = rx_ui_language_lookup(online ? 0x00941fb4 /* "STR_FINISHED" */ : 0x00941fc4 /* "STR_WINS" */, language));
        const uint32_t w[8] = {0xfde9 /* CP_UTF8 */, 0, U32(entry_text, 0x10), 0xffffffffu, utf8, 0x100, 0, 0};
        rx_os_import(0x009160a8, 8, w);                           /* WideCharToMultiByte */
#else
        /* UI boundary (the product has no UI string table). 005dfc90 only searches the UI language registry: the language
         * handles at 0xaf6888 and the suffix strings at 0xaf6950, indexed by [0xa94074]. The UI system fills both, and the
         * product never does, so the original's index check would terminate (_invalid_parameter_noinfo). 005e0270 only reads
         * that language's string table. The localized word is used only in this HUD text. The product leaves the word empty
         * and still raises the event. This is the same stand-in as build_activity_setup.c's 005dfc90 call. */
        SU8(utf8, 0, 0);
#endif
        SU8(F, 0x10f, 0);
        if (online) {
            const uint32_t va[1] = {utf8};
            format_text(text, 0x104, 0x009419e4 /* "%s" */, 1, va);
        } else {
            const uint32_t va[2] = {U32(U32(A, 0x2d0), 0x74) + 0x54, utf8};
            format_text(text, 0x104, 0x00941fd0 /* "%s %s" */, 2, va);
        }
        RACE_CORE_CALL(2, rx_event_player_wins(U32(A, 0x290), A, 0, text, 0));
    }
    const rx_addr player = first_local_human();
    const rx_addr d = local_human_driver(A, player);
    rx_set_sp(F + 0x10);                                          /* pop edi / esi / ebp / ebx */
    if (U32(0, 0x00d5a9a4) != 0) {
        const float t = F32(U32(0, 0x00a942b8), 0x294);
        const float end = t + F32(0, 0x009c40f4);
        const float start = t - F32(0, 0x009c40f0);
        RACE_CORE_CALL(2, rx_world_clip_recorder_candidate(9, d, start, end));
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x005633a0, rx_freestyle_stub_vf40)

/* 006d85e0 (stdcall tracking, pad, player, remote; RET 0x10): a career event's (the series manager's mode 0x9e2318 is 1 or 2) local
 * finish with no trick repeated (006cb9b0 on the player's stunt scorer +0xc70) counts the highlight 0x23 (MXHighlightTracking_AddProgress
 * 006d7980: the profile's persistence, oracle the original as in fx_stunt.c / race_challenge.c). Frame: push ebp; and esp, -8; push
 * ebx / edi. */
void rx_freestyle_highlight(rx_addr tracking, uint32_t pad, rx_addr player, uint32_t remote) {
    RACE_FRAME_EBP(F, 8, 0, 2);
    if (remote == 0) {
        if (!(U8(0, 0x009e275c) & 1)) act_unreached(0x005b2370, "the series manager's static initialiser (MXSeriesManager_Construct)");
        const uint32_t mode = U32(0, 0x009e2318);
        if (mode == 1 || mode == 2) {
            uint32_t unique;
            RACE_CORE_CALL(0, unique = rx_fx_tricks_unique(player + 0xc70));
            if (unique != 0) {
                const uint32_t a[3] = {tracking, pad, 0x23};
#ifdef RX_ORACLE
                rx_call_original(0x006d7980, RX_STDCALL, 0, 3, a);
#else
                RACE_CORE_CALL(3, (void)rx_highlight_add_progress(a[0], a[1], a[2]));
#endif
            }
        }
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x006d85e0, rx_freestyle_highlight)

/* 006ccc70 (cdecl spread; xmm0 out): a random trick deviation: the game's LCG (0x9cb860, advanced twice, the count 0x9cb864 + 2)
 * gives a sign (-1 when the first draw / 32768 is above 0.5) and a magnitude (the second draw / 32768) times spread * 10 */
float rx_fx_ai_trick_deviation(float spread) {
    const uint32_t s1 = U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u;
    const float u1 = (float)((double)(float)(int32_t)((s1 >> 16) & 0x7fff) * 3.0517578125e-05);   /* 0x9776a0 */
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    const float sign = u1 > 0.5f ? -1.0f : 1.0f;                  /* 0x9cd9e8 / 0x931f24 */
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    const uint32_t s2 = s1 * 0x41c64e6du + 0x3039u;
    SU32(0, 0x009cb860, s2);
    const float u2 = (float)((double)(float)(int32_t)((s2 >> 16) & 0x7fff) * 3.0517578125e-05);
    const float scale = spread * 10.0f;                           /* 0x92fd88 */
    return (u2 * scale) * sign;
}
RX_FUNCTION(0x006ccc70, rx_fx_ai_trick_deviation)

/* 006c8640 (EDI = the trick index) -> EAX: the trick record (the stunt table [0xd5aad4], 0xc0 bytes each, count [0xd5aad8]) whose
 * +0x74 is the index, or 0 */
rx_addr rx_fx_trick_record(uint32_t index) {
    const int32_t n = I32(0, 0x00d5aad8);
    rx_addr found = 0;
    rx_addr rec = U32(0, 0x00d5aad4);
    for (int32_t i = 0; i < n && found == 0; i++, rec += 0xc0)
        if (U32(rec, 0x74) == index) found = rec;
    return found;
}
RX_FUNCTION(0x006c8640, rx_fx_trick_record)

/* clamp to [0, hi]: hi when x >= hi, x when 0 < x, else 0 (a NaN gives 0) */
static float clamp0(float x, float hi) {
    if (x >= hi) return hi;
    if (0.0f < x) return x;
    return 0.0f;
}

/* 006ccd10 (ESI = an AI player's stunt scorer, xmm3 = the trick chance; stdcall the spread; RET 4): the AI's freestyle result: the
 * four trick categories (+0x18 .. +0x24) = chance * 10 + a deviation (006ccc70), each clamped to [0, 10]; the run's style (+0x14) =
 * the mean of the three best (in the original's compare order: the larger of +0x18 / +0x1c, the larger of the smaller one and
 * +0x24, the larger of the rest and +0x20), clamped to [0, 10]; the score (+0x7c, the player's +0xcec) = (style + the four) * 2,
 * clamped to [0, 100]; a random trick (index = draw % 53, 006c8640) as the last trick +0x430. Frame: push ecx (the spread). */
void rx_fx_ai_trick_scores(rx_addr scorer, float chance, float spread) {
    RACE_FRAME_PLAIN(F, 0, 1);
    const float base = chance * 10.0f;
    float v;
    RACE_CORE_CALL(1, v = rx_fx_ai_trick_deviation(spread));
    SF32(scorer, 0x18, v + base);
    RACE_CORE_CALL(1, v = rx_fx_ai_trick_deviation(spread));
    SF32(scorer, 0x1c, v + base);
    RACE_CORE_CALL(1, v = rx_fx_ai_trick_deviation(spread));
    SF32(scorer, 0x20, v + base);
    RACE_CORE_CALL(1, v = rx_fx_ai_trick_deviation(spread));
    const float c4raw = v + base;
    SF32(scorer, 0x24, c4raw);
    const float c1 = clamp0(F32(scorer, 0x18), 10.0f);
    SF32(scorer, 0x18, c1);
    const float c2 = clamp0(F32(scorer, 0x1c), 10.0f);
    SF32(scorer, 0x1c, c2);
    const float c3 = clamp0(F32(scorer, 0x20), 10.0f);
    SF32(scorer, 0x20, c3);
    const float c4 = clamp0(c4raw, 10.0f);
    SF32(scorer, 0x24, c4);
    float hi = c1, lo = c2;                                      /* xmm2 / xmm1 */
    if (c2 > c1) { hi = c2; lo = c1; }
    float mid = c4;                                              /* xmm0 */
    if (c4 > lo) { mid = lo; lo = c4; }
    if (c3 > mid) mid = c3;
    const float style0 = ((mid + lo) + hi) * 0.33333334f;        /* 0x92e214 */
    SF32(scorer, 0x14, style0);
    const float style = clamp0(style0, 10.0f);
    SF32(scorer, 0x14, style);
    const float total = ((((style + c4) + c3) + c2) + c1) * 2.0f;
    SF32(scorer, 0x7c, total);
    SF32(scorer, 0x7c, clamp0(total, 100.0f));
    const uint32_t seed = U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u;
    SU32(0, 0x009cb860, seed);
    int32_t index = (int32_t)((seed >> 16) & 0x7fff) % 0x35;
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    if (index < 0) index = 0;
    else if (index > 0x34) index = 0x34;
    rx_addr trick;
    RACE_CORE_CALL(0, trick = rx_fx_trick_record((uint32_t)index));
    SU32(scorer, 0x430, trick);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x006ccd10, rx_fx_ai_trick_scores)

/* 005639a0 (stdcall activity; RET 4): each AI driver's stunt scorer (+0xc70) gets its freestyle result by the AI's skill (+0x8f4:
 * the trick chance / spread 0 = 0.9 / 0.07, 1 = 0.8 / 0.11, 2 = 0.72 / 0.18, 3 = 0.6 / 0.18; another skill keeps the previous
 * AI's values): 006ccd10. Frame: push ecx / ebx / edi (esi in the loop; the chance at +8, the spread at +0x10). */
void rx_freestyle_ai_scores(rx_addr A) {
    RACE_FRAME_PLAIN(F, 0, 3);
    SF32(F, 8, 0.6f);                                             /* 0x931da8 */
    SF32(F, 0x10, 0.18f);                                         /* 0x977830 */
    if (I32(A, 0x26c) > 0) {
        rx_set_sp(F - 4);                                         /* push esi */
        for (int32_t i = 0; i < I32(A, 0x26c); i++) {
            const rx_addr p = U32(U32(U32(A, 0x268), 4 * (uint32_t)i), 0x74);
            if (U32(p, 4) == 0 || U32(p, 0x10) != 1) continue;
            /* another skill keeps the slots as the previous AI left them */
            switch (U32(p, 0x8f4)) {
            case 0: SF32(F, 8, 0.9f); SF32(F, 0x10, 0.07f); break;    /* (0x9cd970 / 0x977824) */
            case 1: SF32(F, 8, 0.8f); SF32(F, 0x10, 0.11f); break;    /* (0x9cc8a8 / 0x977828) */
            case 2: SF32(F, 8, 0.72f); SF32(F, 0x10, 0.18f); break;   /* (0x97782c / the reset 0x977830) */
            case 3: SF32(F, 8, 0.6f); SF32(F, 0x10, 0.18f); break;    /* (0x931da8 / the reset 0x977830) */
            default: break;
            }
            RACE_CORE_CALL(1, rx_fx_ai_trick_scores(p + 0xc70, F32(F, 8), F32(F, 0x10)));
        }
        rx_set_sp(F);
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x005639a0, rx_freestyle_ai_scores)

/* 00563090 MXActivityFreestyleStub::vf41 (ECX = activity; the old state; RET 4): Post enter: Activity::vf41; unless the event was
 * decided by vf14 (+0xff070): the AI trick chances (005639a0), every AI driver's vf18 (0x48), the standings sorted (00559eb0); then
 * for each driver in standings order that is a local human with a profile (player +0x1f0): its records (MXRecords [0xa946c0]: the
 * slot's table / row) TotalEvents + 1 (a saturating add; the table's revision +0x68 counts a change), MXRecords_Placing(place),
 * TotalTime + the driver's time +0x134 (saturating at FLT_MAX), MXRecords_Score(the player's +0xcec) and, for mode 1, the career
 * highlight (006d85e0); +0x13e5f8 (the Post sub-state) = 0. Frame: sub 0x18; push ebx / ebp / esi / edi. */
void rx_freestyle_stub_vf41(rx_addr A, uint32_t old) {
    RACE_FRAME_PLAIN(F, 0x18, 4);
    SU32(F, 0x1c, A);
    (void)old;
    RACE_CORE_CALL(1, rx_activity_post_enter(A));
    if (U32(A, 0xff070) == 0) {
        RACE_CORE_CALL(1, rx_freestyle_ai_scores(A));
        for (int32_t i = 0; i < I32(A, 0x26c); i++) {
            const rx_addr d = rx_stand(A, (uint32_t)i);
            const rx_addr p = U32(d, 0x74);
            if (U32(p, 4) != 0 && U32(p, 0x10) == 1) vcall0(d, 18);
        }
        RACE_CORE_CALL(0, rx_activity_sort_standings(A));
    }
    for (int32_t i = 0; i < I32(A, 0x26c); i++) {
        const rx_addr d = rx_stand(A, (uint32_t)i);
        const rx_addr p = U32(d, 0x74);
        SU32(F, 0x20, d);
        if (U32(p, 4) == 0 || U32(p, 0x10) != 0) continue;
        const int32_t pad = I32(p, 0x1f0);
        if (pad != -1) {
            const rx_addr rec = U32(0, 0x00a946c0) + 0x18 * (uint32_t)pad;
            const uint32_t row = U32(rec, 4);
            if (row != 0xffffffffu) {
                const rx_addr table = U32(rec, 0);
                uint32_t col;
                rx_addr cell;
                RACE_CORE_CALL(1, col = rx_tbl_column_index(table, STR_TOTAL_EVENTS));
                RACE_CORE_CALL(1, cell = rx_tbl_cell_word(table, col, row));
                const int32_t have = I32(cell, 0);
                int32_t add = 0x7fffffff - have;
                if (add > 1) add = 1;
                SU32(F, 0x14, (uint32_t)(add + have));
                RACE_CORE_CALL(1, col = rx_tbl_column_index(table, STR_TOTAL_EVENTS));
                RACE_CORE_CALL(1, cell = rx_tbl_cell_word(table, col, row));
                if (U32(F, 0x14) != U32(cell, 0)) SU32(table, 0x68, U32(table, 0x68) + 1);
                if (col >= (U32(table, 0x14) - U32(table, 0x10)) >> 2) crt_invalid_parameter_noinfo();
                RACE_CORE_CALL(3, rx_tbl_set_int(U32(U32(table, 0x10), 4 * col), row, F + 0x14));
            }
        }
        uint32_t placed;
        RACE_CORE_CALL(1, placed = rx_records_placing(U32(0, 0x00a946c0), (uint32_t)pad, i + 1));
        (void)placed;
        const float time = F32(d, 0x134);
        SF32(F, 0x24, time);
        if (pad != -1) {
            const rx_addr rec = U32(0, 0x00a946c0) + 0x18 * (uint32_t)pad;
            const uint32_t row = U32(rec, 4);
            if (row != 0xffffffffu) {
                const rx_addr table = U32(rec, 0);
                uint32_t col;
                rx_addr cell;
                RACE_CORE_CALL(1, col = rx_tbl_column_index(table, STR_TOTAL_TIME));
                RACE_CORE_CALL(1, cell = rx_tbl_cell_word(table, col, row));
                const float have = F32(cell, 0);
                float add = 3.4028234663852886e+38f - have;
                if (add > time) add = time;
                SF32(F, 0x18, add + have);
                RACE_CORE_CALL(1, col = rx_tbl_column_index(table, STR_TOTAL_TIME));
                RACE_CORE_CALL(1, cell = rx_tbl_cell_word(table, col, row));
                if (!(F32(F, 0x18) == F32(cell, 0))) SU32(table, 0x68, U32(table, 0x68) + 1);
                if (col >= (U32(table, 0x14) - U32(table, 0x10)) >> 2) crt_invalid_parameter_noinfo();
                RACE_CORE_CALL(3, rx_tbl_set_float(U32(U32(table, 0x10), 4 * col), row, F + 0x18));
            }
        }
        uint32_t scored;
        RACE_CORE_CALL(1, scored = rx_records_score(U32(0, 0x00a946c0), (uint32_t)pad, F32(U32(d, 0x74), 0xcec)));
        (void)scored;
        if (U32(A, 0x238) == 1 && pad >= 0)
            RACE_CORE_CALL(4, rx_freestyle_highlight(U32(0, 0x00d92edc), (uint32_t)pad, U32(d, 0x74), U32(d, 0x78)));
    }
    SU32(A, 0x13e5f8, 0);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00563090, rx_freestyle_stub_vf41)

/* 00561ef0 MXActivityFreestyle::vf41 (ECX = activity; the old state; RET 4): the stub's Post enter; the crowd level +0x13e608 = 5 */
void rx_freestyle_vf41(rx_addr A, uint32_t old) {
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(1, rx_freestyle_stub_vf41(A, old));
    SU32(A, 0x13e608, 5);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00561ef0, rx_freestyle_vf41)

/* 00563aa0 (ECX = activity, EAX = the Post sub-state): +0x13e5f8 = the state; state 2 (the results): "Post Scenarios End" ({"Finish",
 * activity}), the clip recorder's playback of kind 8 (the replay boundary) and "Show Activity Results" (the activity) on the
 * activity's channel. Frame: push ecx / esi / edi (the payload in the ecx slot). */
void rx_freestyle_post_state(rx_addr A, uint32_t state) {
    RACE_FRAME_PLAIN(F, 0, 3);
    SU32(A, 0x13e5f8, state);
    if (state == 2) {
        RACE_CORE_CALL(1, rx_event_scenario(STR_FINISH, STR_POST_END, U32(A, 0x290), A));
        clip_start_playback(8);
        raise_activity(U32(A, 0x290), STR_SHOW_RESULTS, F + 8, A);
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00563aa0, rx_freestyle_post_state)

/* 00563b10 (EAX = activity; stdcall dt; RET 4): the Post sub-state +0x13e5f8:
 *  0: when the finish scenario is allowed (vf12) the podium setup (0074ada0) and the finish scenario started (00747810): "Post
 *     Scenarios Start" ({"Finish", activity}) and sub-state 1; otherwise sub-state 2;
 *  1: the finish scenario updated (007473f0) until done (vf8), then sub-state 2;
 *  2: the result flags (0055a470 with 1);
 *  3: the results scenario updated until done: "Post Scenarios End" ({"Results", activity}), the podium released (0074b180 with 1),
 *     "Post State Complete".
 * Frame: push ebp; and esp, -8; push esi / edi. */
void rx_freestyle_update_post(rx_addr A, float dt) {
    RACE_FRAME_EBP(F, 8, 0, 2);
    switch (U32(A, 0x13e5f8)) {
    case 0: {
        if (vcall0(A, 12) != 0) {
            RACE_CORE_CALL(0, rx_podium_setup(A, U32(A, 0x234) + 0x20150));
            uint32_t started;
            RACE_CORE_CALL(0, started = rx_scenario_start(A + 0x114250));
            if (started != 0) {
                RACE_CORE_CALL(1, rx_event_scenario(STR_FINISH, STR_POST_START, U32(A, 0x290), A));
                SU32(A, 0x13e5f8, 1);
                break;
            }
        }
        RACE_CORE_CALL(0, rx_freestyle_post_state(A, 2));
        break;
    }
    case 1: {
        const rx_addr finish = A + 0x114250;
        RACE_CORE_CALL(2, rx_scenario_intro_update(finish, dt));
        if (vcall0(finish, 8) == 0) break;
        RACE_CORE_CALL(0, rx_freestyle_post_state(A, 2));
        break;
    }
    case 2:
        RACE_CORE_CALL(0, rx_activity_result_flags(A, 1));
        break;
    case 3: {
        const rx_addr results = A + 0x129420;
        RACE_CORE_CALL(2, rx_scenario_intro_update(results, dt));
        if (vcall0(results, 8) == 0) break;
        RACE_CORE_CALL(1, rx_event_scenario(STR_RESULTS, STR_POST_END, U32(A, 0x290), A));
        RACE_CORE_CALL(1, rx_podium_release(U32(A, 0x234) + 0x20150, 1));
        RACE_CORE_CALL(1, rx_event_activity(U32(A, 0x290), STR_POST_COMPLETE, A));
        break;
    }
    default:
        break;
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00563b10, rx_freestyle_update_post)

/* 00563330 MXActivityFreestyleStub::vf29 (ECX = activity; dt; RET 4): Post: while the Post clock +0xff064 <= 0, when the activity's
 * vf57 says so the results are shown (00559a40); the clock += dt; the Post sub-states (00563b10). Frame: push esi. */
void rx_freestyle_stub_vf29(rx_addr A, float dt) {
    RACE_FRAME_PLAIN(F, 0, 1);
    if (F32(A, 0xff064) <= 0.0f && vcall0(A, 57) != 0) RACE_CORE_CALL(0, rx_waypoint_show_results(A));
    SF32(A, 0xff064, rx_adds(dt, F32(A, 0xff064)));
    RACE_CORE_CALL(1, rx_freestyle_update_post(A, dt));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00563330, rx_freestyle_stub_vf29)

/* 00561f10 MXActivityFreestyle::vf29 (ECX = activity; dt; RET 4): the stub's Post; mode 1 mirrors the crowd level into 0x9c3edc */
void rx_freestyle_vf29(rx_addr A, float dt) {
    RACE_FRAME_PLAIN(F, 0, 1);
    if (F32(A, 0xff064) <= 0.0f && vcall0(A, 57) != 0) RACE_CORE_CALL(0, rx_waypoint_show_results(A));
    SF32(A, 0xff064, rx_adds(dt, F32(A, 0xff064)));
    RACE_CORE_CALL(1, rx_freestyle_update_post(A, dt));
    if (U32(A, 0x238) == 1) SU32(0, CROWD_LEVEL, U32(A, 0x13e608));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00561f10, rx_freestyle_vf29)

/* 00563c50 MXActivityFreestyleStub::vf58 (ECX = activity): the results reel: the clip recorder's playback stopped (006aa990); when
 * the results scenario is allowed (vf13) and starts (00747810): "Post Scenarios Start" ({"Results", activity}) and sub-state 3;
 * otherwise the podium released (0074b180 with 1) and "Post State Complete" on the activity's channel. Frame: push ecx / esi / edi. */
void rx_freestyle_stub_vf58(rx_addr A) {
    RACE_FRAME_PLAIN(F, 0, 3);
    clip_stop_playback();
    if (vcall0(A, 13) != 0) {
        uint32_t started;
        RACE_CORE_CALL(0, started = rx_scenario_start(A + 0x129420));
        if (started != 0) {
            RACE_CORE_CALL(1, rx_event_scenario(STR_RESULTS, STR_POST_START, U32(A, 0x290), A));
            SU32(A, 0x13e5f8, 3);
            RACE_FRAME_END(F);
            return;
        }
    }
    RACE_CORE_CALL(1, rx_podium_release(U32(A, 0x234) + 0x20150, 1));
    raise_activity(U32(A, 0x290), STR_POST_COMPLETE, F + 8, A);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00563c50, rx_freestyle_stub_vf58)

/* 005635f0 MXActivityFreestyleStub::vf31 (ECX = activity; arg; RET 4): back to the hub: the engine's frame marked (0084ea00), the level
 * cast's current scripted sequences finished (0055e2b0), the cast released (0074bd50) and its +0x44 cleared, the UI's hub routing
 * (0059b860: UI BOUNDARY). Frame: push esi / edi. */
void rx_freestyle_stub_vf31(rx_addr A, uint32_t arg) {
    (void)arg;
    RACE_FRAME_PLAIN(F, 0, 2);
    const rx_addr cast = U32(A, 0x234) + 0x20150;
    RACE_CORE_CALL(0, rx_engine_frame_mark(U32(U32(0, 0x00a95844), 0x10)));
    const rx_addr current = U32(cast, 0x40);
    if (current != 0) RACE_CORE_CALL(1, rx_activity_finish_sequences(current));
    RACE_CORE_CALL(1, rx_scenario_cast_release(cast));
    SU32(cast, 0x44, 0);
    ui_goto_hub();
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x005635f0, rx_freestyle_stub_vf31)

/* ======================================================================================================================== */
/* the players                                                                                                                */

/* 0045d4f0 (EAX = vehicle): the vehicle's chassis body record (+0x17c, 0042bd90) loses its link (+0x2c -> +0x14 = -1); the twin of
 * 0045d4d0 (race_net.c) */
void rx_vehicle_body_link_unset(rx_addr v) {
    const rx_addr handle = U32(v, 0x17c);
    if (handle == 0) return;
    rx_addr rec;
    RACE_CORE_CALL(0, rec = phys_body_record(handle));
    SU32(U32(rec, 0x2c), 0x14, 0xffffffffu);
}
RX_FUNCTION(0x0045d4f0, rx_vehicle_body_link_unset)

/* 0061c690 (EAX = the rider animation): its network hold released: +0x5bb5c = 0, +0xa2 = 0 (byte), +0x5bb64 = 0 (byte), +0x5b6d0 = 0;
 * the twin of 0061c6b0 (race_net.c) */
void rx_pad_net_release(rx_addr ra) {
    SU32(ra, 0x5bb5c, 0);
    SU8(ra, 0xa2, 0);
    SU8(ra, 0x5bb64, 0);
    SU32(ra, 0x5b6d0, 0);
}
RX_FUNCTION(0x0061c690, rx_pad_net_release)


/* 00563640 MXActivityFreestyleStub::vf45 (ECX = activity; from, to; RET 8): Activity::vf45; with the area flag (+0xfe8) and both
 * drivers: `from`'s vehicle hidden (model +0x14e = 0, vf49 (0)), its chassis link unset (0045d4f0), its rider's hold released
 * (0061c690); `to`'s vehicle shown (+0x14e = 1, vf49 (1)), its link cleared (0045d4d0), its simulation's pose override ended
 * (00434ef0), its rider held (0061c6b0). Frame: push ebx / esi / edi. */
void rx_freestyle_stub_vf45(rx_addr A, rx_addr from, rx_addr to) {
    RACE_FRAME_PLAIN(F, 0, 3);
    RACE_CORE_CALL(2, rx_activity_vf45(A, from, to));
    if (U32(A, 0xfe8) != 0 && from != 0 && to != 0) {
        if (U32(U32(from, 0x74), 0x1ac) != 0) {
            SU8(U32(U32(U32(from, 0x74), 0x1ac), 0x1328), 0x14e, 0);
            const rx_addr v = U32(U32(from, 0x74), 0x1ac);
            vcall1(v, 49, 0);
            RACE_CORE_CALL(0, rx_vehicle_body_link_unset(U32(U32(from, 0x74), 0x1ac)));
            const rx_addr ra = U32(U32(from, 0x74), 0x1b4);
            if (ra != 0) RACE_CORE_CALL(0, rx_pad_net_release(ra));
        }
        if (U32(U32(to, 0x74), 0x1ac) != 0) {
            SU8(U32(U32(U32(to, 0x74), 0x1ac), 0x1328), 0x14e, 1);
            const rx_addr v = U32(U32(to, 0x74), 0x1ac);
            vcall1(v, 49, 1);
            RACE_CORE_CALL(0, rx_vehicle_body_link_clear(U32(U32(to, 0x74), 0x1ac)));
            RACE_CORE_CALL(0, bike_end_pose_override(U32(U32(U32(to, 0x74), 0x1ac), 0x234)));
            const rx_addr ra = U32(U32(to, 0x74), 0x1b4);
            if (ra != 0) RACE_CORE_CALL(0, rx_pad_net_hold(ra));
        }
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00563640, rx_freestyle_stub_vf45)


/* 00561f80 (ECX = activity; the event's activity, driver; RET 8): "Ghost Session Completed" (mode 3, the ghost run): the activity
 * back to Pre (SetState 0); each driver of the event's activity (the count is this activity's): its score kept as the score to beat
 * (+0xf30 = +0xf10), +0xf10 / +0xf34 / +0xf40 = 0, the stunt system reset (006ca750), "StuntFinalScore" ({player, 0}) on the stunt
 * scores' channel [0xa94318], the controls reset (006c9100), its vehicle back to the driver's start pose (the simulation's vf20,
 * 0045f600), the rider's pose reset (00619050). Frame: push ebp; and esp, -16; sub 0x54 (the security cookie at +0x50); push ebx /
 * esi / edi (the counter at +0x14, this at +0x18, the event's activity at +0x1c, the ground results at +0x20 / +0x30 / +0x40, the
 * payload at +0x54). */
void rx_freestyle_ghost_completed(rx_addr A, rx_addr activity, rx_addr driver) {
    (void)driver;
    const rx_addr entry = rx_sp();
    RACE_ORIG_FRAME_ENTER(F, 0x54, 3);
    SU32(F, 0x5c, U32(0, 0x009b45e4) ^ (F + 0xc));                /* the /GS cookie (esp + 0x50 before the pushes) */
    (void)entry;
    SU32(F, 0x18, A);
    SU32(F, 0x1c, activity);
    RACE_CORE_CALL(2, rx_activity_set_state(A, 0));
    SU32(F, 0x14, 0);
    for (int32_t i = 0; i < I32(A, 0x26c); i++) {
        SU32(F, 0x14, (uint32_t)i);
        const rx_addr d = U32(U32(activity, 0x268), 4 * (uint32_t)i);
        SU32(d, 0xf30, U32(d, 0xf10));
        SU32(d, 0xf10, 0);
        SU32(d, 0xf34, 0);
        SU32(d, 0xf40, 0);
        RACE_CORE_CALL(0, rx_stunt_system_reset(U32(d, 0x74) + 0xc70));
        SU32(F, 0x54, U32(d, 0x74));
        SU32(F, 0x58, 0);
        act_raise(U32(0, 0x00a94318), 0x0092fc60 /* "StuntFinalScore" */, 0x009cf610, F + 0x54, 8);
        RACE_CORE_CALL(0, rx_controls_reset(U32(d, 0x74) + 0xb58));
        const rx_addr pose = U32(d, 0x144);
        const rx_addr vehicle = U32(U32(d, 0x74), 0x1ac);
        const rx_addr sim = U32(vehicle, 0x234);
        const uint32_t ground[5] = {pose, pose + 0x10, F + 0x40, F + 0x20, F + 0x30};
        (void)act_call_fn(race_vfn(sim, 20), RX_THISCALL, sim, 5, ground);
        RACE_CORE_CALL(3, rx_vehicle_place(vehicle, F + 0x40, F + 0x20, F + 0x30));
        const rx_addr ra = U32(U32(d, 0x74), 0x1b4);
        if (ra != 0) RACE_CORE_CALL(0, rx_rider_animation_reset_pose(ra));
    }
    RACE_ORIG_FRAME_LEAVE(F);
}
RX_FUNCTION(0x00561f80, rx_freestyle_ghost_completed)



/* ======================================================================================================================== */
/* the end                                                                                                                    */

/* 005627b0 MXActivityFreestyleStub::vf3 (ECX = activity): the track block (+0x890) reset (00538090); the clip recorder released
 * when it records this activity ([0xa942b8] / [0xa942b4] = 0); the world's podium released (0074b180 with 1); a jump to
 * Activity::vf3. Frame: push esi (popped before the jump). */
void rx_freestyle_stub_vf3(rx_addr A) {
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(0, rx_track_block_reset(A + 0x890));
    if (A != 0 && A == U32(0, 0x00a942b8)) {
        SU32(0, 0x00a942b4, 0);
        SU32(0, 0x00a942b8, 0);
    }
    if (U32(A, 0x234) != 0) RACE_CORE_CALL(1, rx_podium_release(U32(A, 0x234) + 0x20150, 1));
    RACE_FRAME_END(F);
    rx_activity_vf3(A);
}
RX_FUNCTION(0x005627b0, rx_freestyle_stub_vf3)

/* the unsubscription templates of MXActivityFreestyle::vf3 (act_unlisten: `push ebp; mov ebp, esp; and esp, -8; sub esp, 0x14;
 * push esi`): 00587b70 (EAX = the activity's channel; stdcall activity; RET 4) "Ghost Session Completed" -> 00561f80; 00587bd0 (ECX =
 * name; stdcall activity, method; RET 8) the crowd's stunt handlers; 00587c30 (stdcall activity; RET 4) "PhysicsStuntBegin" */
void rx_freestyle_unlisten_ghost(rx_addr channel, rx_addr A) {
    act_unlisten(1, channel, STR_GHOST_COMPLETED, 0x00945964, 0x009cda18, A, 0x00561f80);
}
RX_FUNCTION(0x00587b70, rx_freestyle_unlisten_ghost)
void rx_freestyle_unlisten_stunt(rx_addr name, rx_addr A, rx_addr method) {
    act_unlisten(1, U32(0, STUNTS_CHANNEL), name, 0x00945924, 0x009cbf38, A, method);
}
RX_FUNCTION(0x00587bd0, rx_freestyle_unlisten_stunt)
void rx_freestyle_unlisten_physics_stunt(rx_addr A) {
    act_unlisten(1, U32(0, STUNTS_CHANNEL), STR_PHYSICS_STUNT_BEGIN, 0x00945944, 0x009ce768, A, 0x005621f0);
}
RX_FUNCTION(0x00587c30, rx_freestyle_unlisten_physics_stunt)

/* the drivers of both lists leave (Activity_OnPlayerLeft 0055c1d0 with the list's first driver each time: the call removes it) */
static void drivers_leave(rx_addr F, rx_addr A) {
    while (I32(A, 0x26c) > 0) {
        const rx_addr d = U32(U32(A, 0x268), 0);
        SU32(F, -4, d);
        SU32(F, -8, A);
        RACE_CORE_CALL(2, rx_activity_on_player_left(A, d));
    }
    while (I32(A, 0x280) > 0) {
        const rx_addr d = U32(U32(A, 0x27c), 0);
        SU32(F, -4, d);
        SU32(F, -8, A);
        RACE_CORE_CALL(2, rx_activity_on_player_left(A, d));
    }
}

/* 00561d30 MXActivityFreestyle::vf3 (ECX = activity): the drivers leave; the handlers vf2 made leave (00587b70 / 00587bd0 x2 /
 * 00587c30); a jump to MXActivityFreestyleStub::vf3. Frame: push esi (popped before the jump). */
void rx_freestyle_vf3(rx_addr A) {
    RACE_FRAME_PLAIN(F, 0, 1);
    drivers_leave(F, A);
    SU32(F, -4, A);
    RACE_CORE_CALL(1, rx_freestyle_unlisten_ghost(U32(A, 0x290), A));
    SU32(F, -4, 0x00562220);
    SU32(F, -8, A);
    RACE_CORE_CALL(2, rx_freestyle_unlisten_stunt(STR_STUNT_BEGIN, A, 0x00562220));
    SU32(F, -4, 0x00562250);
    SU32(F, -8, A);
    RACE_CORE_CALL(2, rx_freestyle_unlisten_stunt(STR_STUNT_FAILED, A, 0x00562250));
    SU32(F, -4, A);
    RACE_CORE_CALL(1, rx_freestyle_unlisten_physics_stunt(A));
    RACE_FRAME_END(F);
    rx_freestyle_stub_vf3(A);
}
RX_FUNCTION(0x00561d30, rx_freestyle_vf3)

/* 0057dc90 MXActivityFreestyleChampIntro::vf3 (also the practice / main event's; ECX = activity): the drivers leave; a jump to
 * MXActivityFreestyleStub::vf3. Frame: push esi. */
void rx_freestyle_champ_intro_vf3(rx_addr A) {
    RACE_FRAME_PLAIN(F, 0, 1);
    drivers_leave(F, A);
    RACE_FRAME_END(F);
    rx_freestyle_stub_vf3(A);
}
RX_FUNCTION(0x0057dc90, rx_freestyle_champ_intro_vf3)

/* 005623f0 (ECX = activity): the stub's destructor body: vtable 0x9441cc; the audio logic (+0xfd8) destroyed (00580d30) and freed;
 * the three scenarios back to BaseScenario (0x96cea4); the Activity's destructor body (005566b0). Frame: push ecx / esi / edi. */
void rx_freestyle_destroy(rx_addr A) {
    RACE_FRAME_PLAIN(F, 0, 3);
    const rx_addr audio = U32(A, 0xfd8);
    SU32(A, 0, 0x009441cc);
    if (audio != 0) {
        RACE_CORE_CALL(1, rx_activity_audio_destroy(audio));
        RACE_CORE_CALL(1, rx_reflex_aligned_free(audio));
    }
    SU32(A, 0x129420, 0x0096cea4);
    SU32(A, 0x114250, 0x0096cea4);
    SU32(A, 0xff080, 0x0096cea4);
    RACE_CORE_CALL(0, rx_waypoint_activity_base_destroy(A));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x005623f0, rx_freestyle_destroy)

/* the deleting destructors: the class's vtable, the body, freed with flag 1 (`push esi`; ReflexAlignedFree is cdecl) */
static rx_addr freestyle_delete(rx_addr A, uint32_t flags) {
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(0, rx_freestyle_destroy(A));
    if (flags & 1) RACE_CORE_CALL(1, rx_reflex_aligned_free(A));
    RACE_FRAME_END(F);
    return A;
}

/* 00562390 MXActivityFreestyleStub::sdtor (ECX = activity; flags; RET 4) -> activity */
rx_addr rx_freestyle_stub_sdtor(rx_addr A, uint32_t flags) { return freestyle_delete(A, flags); }
RX_FUNCTION(0x00562390, rx_freestyle_stub_sdtor)

/* 00561960 MXActivityFreestyle::sdtor (ECX = activity; flags; RET 4) -> activity: vtable 0x9442cc first */
rx_addr rx_freestyle_sdtor(rx_addr A, uint32_t flags) {
    SU32(A, 0, 0x009442cc);
    return freestyle_delete(A, flags);
}
RX_FUNCTION(0x00561960, rx_freestyle_sdtor)

/* the championship classes' deleting destructors (0057dab0 / 0057dbf0 / 0057de90): the class's vtable, the activity's start block
 * +0x890 (0x330 bytes) cleared, then as the stub's */
static rx_addr champ_delete(rx_addr A, uint32_t flags, uint32_t vtable) {
    RACE_FRAME_PLAIN(F, 0, 1);
    SU32(A, 0, vtable);
    for (uint32_t i = 0; i < 0x330; i++) SU8(A + 0x890, i, 0);
    RACE_CORE_CALL(0, rx_freestyle_destroy(A));
    if (flags & 1) RACE_CORE_CALL(1, rx_reflex_aligned_free(A));
    RACE_FRAME_END(F);
    return A;
}
rx_addr rx_freestyle_champ_intro_sdtor(rx_addr A, uint32_t flags) { return champ_delete(A, flags, 0x009443cc); }
RX_FUNCTION(0x0057dab0, rx_freestyle_champ_intro_sdtor)
rx_addr rx_freestyle_champ_main_sdtor(rx_addr A, uint32_t flags) { return champ_delete(A, flags, 0x009445cc); }
RX_FUNCTION(0x0057dbf0, rx_freestyle_champ_main_sdtor)
rx_addr rx_freestyle_champ_practice_sdtor(rx_addr A, uint32_t flags) { return champ_delete(A, flags, 0x009444cc); }
RX_FUNCTION(0x0057de90, rx_freestyle_champ_practice_sdtor)

/* ======================================================================================================================== */
/* the Freestyle championship classes                                                                                          */

/* 0057daf0 MXActivityFreestyleChampIntro::vf2 (ECX = activity; arg; RET 4): the stub's start with the argument, the start slots
 * (00562800), "Activity Init" on the activity manager's channel. Frame: push ebp; and esp, -8; sub 8; push esi / edi. */
void rx_freestyle_champ_intro_vf2(rx_addr A, rx_addr arg) {
    RACE_FRAME_EBP(F, 8, 8, 2);
    RACE_CORE_CALL(1, rx_freestyle_stub_vf2(A, arg));
    RACE_CORE_CALL(0, rx_freestyle_start_slots(A));
    raise_activity(U32(0, ACTIVITY_MANAGER_CHANNEL), STR_ACTIVITY_INIT, F + 0xc, A);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0057daf0, rx_freestyle_champ_intro_vf2)

/* 0057dc30 MXActivityFreestyleChampMainEvent::vf2 (also the practice's; ECX = activity; arg (unused); RET 4): as the intro's, the
 * stub's start with "SeriesOverlay" (0x9426b4) */
void rx_freestyle_champ_main_vf2(rx_addr A, rx_addr arg) {
    (void)arg;
    RACE_FRAME_EBP(F, 8, 8, 2);
    RACE_CORE_CALL(1, rx_freestyle_stub_vf2(A, 0x009426b4 /* "SeriesOverlay" */));
    RACE_CORE_CALL(0, rx_freestyle_start_slots(A));
    raise_activity(U32(0, ACTIVITY_MANAGER_CHANNEL), STR_ACTIVITY_INIT, F + 0xc, A);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0057dc30, rx_freestyle_champ_main_vf2)

/* 0057db40 MXActivityFreestyleChampIntro::vf5 (ECX = activity): Activity::vf5; an active activity of the race / intro types (mode not
 * 0 / 2, type < 0x22) gets its track name (0055a5d0 with the type, 0x20 as 5) and the world's track (007677e0); "Activity Basic
 * Essentials Loaded". Frame: push ecx / esi / edi (the payload in the ecx slot). */
void rx_freestyle_champ_intro_vf5(rx_addr A) {
    RACE_FRAME_PLAIN(F, 0, 3);
    RACE_CORE_CALL(0, rx_activity_vf5(A));
    uint32_t type = U32(A, 0x23c);
    if (type == 0x20) type = 5;
    const uint32_t mode = U32(A, 0x238);
    const uint32_t t = U32(A, 0x23c);
    if (mode != 2 && mode != 0 && (t < 0x1f || t <= 0x21)) {
        RACE_CORE_CALL(1, rx_activity_track_name(A + 0xed4, type, A));
        RACE_CORE_CALL(2, rx_mxworld_set_track(U32(A, 0x234), A + 0xed4, U32(A, 0xbb0) != 0 ? A + 0x890 : 0));
    }
    raise_activity(U32(0, ACTIVITY_MANAGER_CHANNEL), STR_BASICS_LOADED, F + 8, A);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0057db40, rx_freestyle_champ_intro_vf5)

/* 0057ded0 MXActivityFreestyleChampMainEvent::vf5 (also the practice's; ECX = activity): Activity::vf5 and "Activity Basic Essentials
 * Loaded". Frame: push ecx / ebx / esi / edi. */
void rx_freestyle_champ_main_vf5(rx_addr A) {
    RACE_FRAME_PLAIN(F, 0, 4);
    RACE_CORE_CALL(0, rx_activity_vf5(A));
    raise_activity(U32(0, ACTIVITY_MANAGER_CHANNEL), STR_BASICS_LOADED, F + 0xc, A);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0057ded0, rx_freestyle_champ_main_vf5)

/* 0057dbe0 MXActivityFreestyleChampIntro::vf25 (ECX = activity; dt; RET 4): the stub's Pre */
void rx_freestyle_champ_intro_vf25(rx_addr A, float dt) { RACE_CORE_CALL(1, rx_freestyle_stub_vf25(A, dt)); }
RX_FUNCTION(0x0057dbe0, rx_freestyle_champ_intro_vf25)

/* 0057dd00 MXActivityFreestyleChampMainEvent::vf28 (also the practice's; ECX = activity; dt; RET 4): InProgress: Activity::vf28 and
 * the track debug (005399e0 when +0xb8c). Frame: push ebp; and esp, -8; push ecx; push esi. */
void rx_freestyle_champ_main_vf28(rx_addr A, float dt) {
    RACE_FRAME_EBP(F, 8, 0, 2);
    RACE_CORE_CALL(1, rx_activity_in_progress(A, dt));
    if (U32(A, 0xb8c) != 0) RACE_CORE_CALL(0, rx_waypoint_track_debug(A + 0x890));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0057dd00, rx_freestyle_champ_main_vf28)


/* 00767760 (EAX = world) -> EAX: the world's camera path named "Freestyle_Intro_Cam" (the list +0x21de8, next +0x90, name +0x9c;
 * _stricmp) or 0 */
rx_addr rx_world_freestyle_intro_cam(rx_addr w) {
    RACE_FRAME_PLAIN(F, 0, 2);
    rx_addr path = U32(w, 0x21de8);
    while (path != 0) {
        const rx_addr name = path + 0x9c != 0 ? path + 0x9c : 0x00935d6c;
        int32_t c;
        RACE_CORE_CALL(2, c = crt_stricmp(0x009426a0 /* "Freestyle_Intro_Cam" */, name));
        if (c == 0) break;
        path = U32(path, 0x90);
    }
    RACE_FRAME_END(F);
    return path;
}
RX_FUNCTION(0x00767760, rx_world_freestyle_intro_cam)

/* 0063d580 (EAX = the new path, EDX = the template): Entity's copy (vtable 0x95a2e4): +0x10 .. +0x1c, +0x20 / +0x24 (x87 copies),
 * +0x28 / +0x2c, the matrix +0x30 (0x40 bytes), +0x70 / +0x74, +0x78 (x87), +0x7c, the byte +0x80 */
void rx_entity_copy_construct(rx_addr e, rx_addr src) {
    SU32(e, 0, 0x0095a2e4);
    for (uint32_t o = 0x10; o <= 0x1c; o += 4) SU32(e, o, U32(src, o));
    SU32(e, 0x20, x87_copy(U32(src, 0x20)));
    SU32(e, 0x24, x87_copy(U32(src, 0x24)));
    SU32(e, 0x28, U32(src, 0x28));
    SU32(e, 0x2c, U32(src, 0x2c));
    for (uint32_t o = 0; o < 0x40; o += 4) SU32(e + 0x30, o, U32(src + 0x30, o));
    SU32(e, 0x70, U32(src, 0x70));
    SU32(e, 0x74, U32(src, 0x74));
    SU32(e, 0x78, x87_copy(U32(src, 0x78)));
    SU32(e, 0x7c, U32(src, 0x7c));
    SU8(e, 0x80, U8(src, 0x80));
}
RX_FUNCTION(0x0063d580, rx_entity_copy_construct)

/* 0063d4d0 (stdcall path, template; RET 8) -> path: MXCameraPath's copy (vtable 0x95a3f4): the Entity part (0063d580), the list links
 * +0x90 / +0x94 / +0x98, the two 0x40-byte blocks +0x9c / +0xdc, +0x11c, +0x120 (x87) */
rx_addr rx_camera_path_copy_construct(rx_addr path, rx_addr src) {
    RACE_FRAME_PLAIN(F, 0, 4);
    RACE_CORE_CALL(0, rx_entity_copy_construct(path, src));
    SU32(path, 0, 0x0095a3f4);
    SU32(path, 0x90, U32(src, 0x90));
    SU32(path, 0x94, U32(src, 0x94));
    SU32(path, 0x98, U32(src, 0x98));
    for (uint32_t o = 0; o < 0x40; o += 4) SU32(path + 0x9c, o, U32(src + 0x9c, o));
    for (uint32_t o = 0; o < 0x40; o += 4) SU32(path + 0xdc, o, U32(src + 0xdc, o));
    SU32(path, 0x11c, U32(src, 0x11c));
    SU32(path, 0x120, x87_copy(U32(src, 0x120)));
    RACE_FRAME_END(F);
    return path;
}
RX_FUNCTION(0x0063d4d0, rx_camera_path_copy_construct)

/* 0063d3e0 (ESI = the new camera; stdcall the path template; RET 4) -> EAX: MXAnimatedCamera (vtable 0x95a46c): the camera entity
 * base (0063d930), its path +0x310 copied from the template (0063d4d0), +0x448 = 1, +0x44c = 0, +0x450 = 0.033 s, +0x70 = 1,
 * +0x444 / +0x4a0 = 0, the identity matrix at +0x460. Frame: push esi (the base's stack argument). */
rx_addr rx_animated_camera_construct(rx_addr cam, rx_addr path) {
    RACE_CORE_CALL(1, (void)rx_camera_entity_construct(cam));
    RACE_CORE_CALL(2, (void)rx_camera_path_copy_construct(cam + 0x310, path));
    SU32(cam, 0, 0x0095a46c);
    SF32(cam, 0x448, 1.0f);
    SF32(cam, 0x44c, 0.0f);
    SF32(cam, 0x450, F32(0, 0x00977818));
    SU32(cam, 0x70, 1);
    SU32(cam, 0x444, 0);
    SU32(cam, 0x4a0, 0);
    for (uint32_t i = 0; i < 16; i++) SF32(cam + 0x460, 4 * i, (i % 5) == 0 ? 1.0f : 0.0f);
    return cam;
}
RX_FUNCTION(0x0063d3e0, rx_animated_camera_construct)


/* 0063d3b0 MXAnimatedCamera::sdtor (ECX = camera; flags; RET 4) -> camera: the path's destructor (0063e6a0), the camera entity's
 * (0063da30), freed with flag 1. Frame: push esi / edi. */
rx_addr rx_animated_camera_sdtor(rx_addr cam, uint32_t flags) {
    RACE_FRAME_PLAIN(F, 0, 2);
    RACE_CORE_CALL(0, rx_camera_path_destroy(cam + 0x310));
    RACE_CORE_CALL(0, rx_camera_entity_destroy(cam));              /* 0063da30 (ESI = the camera entity; anim_rider_dtors.c) */
    if (flags & 1) RACE_CORE_CALL(1, rx_reflex_aligned_free(cam));
    RACE_FRAME_END(F);
    return cam;
}
RX_FUNCTION(0x0063d3b0, rx_animated_camera_sdtor)

/* the championship's freestyle intro camera (the WindDown exit / Post exit of the championship classes): a new MXAnimatedCamera
 * (0x4b0 bytes, 0063d3e0) from the world's "Freestyle_Intro_Cam" path, added to the scene (0063dae0), updated once (vf19 (0)),
 * shown in the scene's first view (0063dba0), removed from the scene (007d3040), released (vf9) and deleted (vf0 (1)). The game
 * world is [0xa946b8] + 0x5a54. */
static void champ_intro_camera(void) {
    const rx_addr w = U32(U32(0, 0x00a946b8), 0x5a54);
    rx_addr path;
    RACE_CORE_CALL(0, path = rx_world_freestyle_intro_cam(w));
    rx_addr mem;
    RACE_CORE_CALL(1, mem = rx_reflex_aligned_alloc(0x4b0));
    rx_addr cam = 0;
    if (mem != 0) RACE_CORE_CALL(1, cam = rx_animated_camera_construct(mem, path));
    RACE_CORE_CALL(0, rx_camera_entity_add_to_scene(cam, U32(w, 0x20138)));
    act_vcall_f(cam, 0x4c, 0.0f);                                 /* vf19 (0.0f): the camera's update */
    RACE_CORE_CALL(2, rx_cam_entity_to_view(cam, U32(U32(w, 0x20138), 0x19b8)));
    RACE_CORE_CALL(1, rx_scene_manager_remove_entity(cam, U32(cam, 0x10)));
    vcall0(cam, 9);
    vcall1(cam, 0, 1);
}

/* 0057dd40 MXActivityFreestyleChampMainEvent::vf34 (ECX = activity; the new state; RET 4): WindDown exit: the intro camera. Frame:
 * push ebx / esi / edi. */
void rx_freestyle_champ_main_vf34(rx_addr A, uint32_t state) {
    (void)A;
    (void)state;
    RACE_FRAME_PLAIN(F, 0, 3);
    champ_intro_camera();
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0057dd40, rx_freestyle_champ_main_vf34)

/* 0057ddd0 MXActivityFreestyleChampMainEvent::vf35 (ECX = activity; the new state; RET 4): Post exit: the controls on (0055a1e0
 * with 1), the world's +0x21de0 = 0, the replay cameras updated (0.033 s), 0xa941b8 = 0, the intro camera. Frame: push ebx / esi /
 * edi. */
void rx_freestyle_champ_main_vf35(rx_addr A, uint32_t state) {
    (void)state;
    RACE_FRAME_PLAIN(F, 0, 3);
    RACE_CORE_CALL(1, rx_activity_enable_controls(1, A));
    const rx_addr w = U32(A, 0x234);
    SU32(w, 0x21de0, 0);
    RACE_CORE_CALL(2, rx_replay_cameras_update(w, F32(0, 0x00977818)));
    SU32(0, 0x00a941b8, 0);
    champ_intro_camera();
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0057ddd0, rx_freestyle_champ_main_vf35)

/* 0057df10 MXActivityFreestyleChampPractice::vf35 (ECX = activity; the new state; RET 4): the stub's Post exit (00579b20) and the
 * intro camera. Frame: push ebx / esi / edi. */
void rx_freestyle_champ_practice_vf35(rx_addr A, uint32_t state) {
    RACE_FRAME_PLAIN(F, 0, 3);
    RACE_CORE_CALL(1, rx_waypoint_activity_vf35(A, state));
    champ_intro_camera();
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0057df10, rx_freestyle_champ_practice_vf35)


/* ======================================================================================================================== */
/* the activity audio logic's shared slots the Freestyle logic (0x00942bbc) reaches                                            */


/* 0057dcf0 (slot 38 of MXActivityFreestyleChampMainEvent, MXActivityEggHunt, MXActivityInfection, MXActivityTag; ECX = activity;
 * the old state; RET 4): `jmp Activity::vf38` */
void rx_champ_vf38(rx_addr A, uint32_t old) {
    (void)old;
    rx_activity_countdown_enter(A);
}
RX_FUNCTION(0x0057dcf0, rx_champ_vf38)
