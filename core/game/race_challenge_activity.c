/* race_challenge_activity.c -- the Free Ride challenge activities.
 *
 * A challenge is an activity of its own (the factory 0055ef80 builds it when the event's EventTypeActivation names a challenge:
 * MXUIManager::SetupEvent takes the activity type from it, build_activity_event.c). Every class derives from MXActivityChallenge
 * (vtable 0x009450cc), whose constructor 0057aa30 runs Activity's (005562c0) and adds:
 *     +0xff070  the challenge kind (0 BigAir, 1 HillClimb, 3 Pathfinder, 4 StepUp, 6 Trailblazer, 8 TargetPractice: the name table
 *               0x009cf540, the start entity's type 0057ac50, the medal criteria index)
 *     +0xff074 / +0xff078 / +0xff07c  the medals won (vf64 fills them at the Post enter: gold / silver / bronze flags)
 *     +0xff080  the challenge's track block (00536ca0; the .tdf07 "<level>_<kind name>", kind 0xd, loaded by 0057b1a0)
 *     +0xff3a0  the block's loaded flag (block + 0x320)
 *     +0xfd8    the ChallengeActivityAudioLogic (00584e50, vtable 0x009431fc)
 *   HillClimb / TrailBlazer: +0xff3b0 the best height / the clock, +0xff3b4 the start height;
 *   BigAir: +0xff3b0 the jump state (0 on the ground, 1 in the air);
 *   TargetPractice: +0xff3b0 the number of targets, +0xff3b4 .. +0xff430 the targets (MXTargetPracticeCollectable), +0xff434 the hits.
 * The medal criteria are the level's MXChallengesSpecEntity (world + 0x22140: the floats of [kind][medal][area], at index
 * 36 + 12 * kind + 3 * medal + area; the time limit at 0x43 + 3 * kind + area); 10.0 without the entity.
 *
 * The run: Activity's state machine (race_activity.c) calls the class's state methods; the Post enter (vf41) asks vf64 which medals
 * the value (vf62) reached and awards them (0054e670, race_challenge.c), raising "ChallengeAward" on the activity's channel.
 * The challenge's driver is a ChallengeDriver (vf44): a NetworkDriver whose update runs its rules before the network update; its rule
 * RuleChallengeBoundary (vf47) ends the challenge when the rider leaves the challenge track's corridor for a second.
 *
 * UI boundary: vf2 raises the UI events "ShowPreChallengePopup" and "GotoHub" on the UI manager's channel (the UI manager's event
 * list 005db500, then its +0x98 channel). The product has no UI manager ([0x00a93fcc] = 0): act_ui_call there; the pages they show
 * write only UI state (the oracle runs the original lookup).
 *
 * Every function is a hand transliteration of the disassembly; frames and the stack depth of each call are the original's. */
#include "race_challenge_activity.h"
#include "race_challenge.h"
#include "race_activity.h"
#include "race_activity_misc.h"
#include "race_finish.h"
#include "build_activity_int.h"
#include "build_activity_pre.h"
#include "build_activity_freeride_run.h"
#include "build_activity_clear.h"
#include "build_activity_teardown.h"
#include "build_vehicle.h"
#include "build_rider.h"
#include "waypoint_activity.h"
#include "waypoint_audio.h"
#include "waypoint_in_progress.h"
#include "world_misc.h"
#include "world_rider.h"
#include "world_board_handlers.h"
#include "phys_terrain.h"
#include "loop_player.h"
#include "loop_world.h"
#include "race_driver.h"
#include "fx.h"
#include "crt.h"
#include "crt_atexit.h"
#include "race_champ.h"
#include "build_rider_util.h"
#include "anim.h"
#include "waypoint_driver.h"
#include "build_driver_dtor.h"
#include "build_driver_reset.h"
#include "race_misc.h"
#include "race.h"
#include "bike_sim.h"
#include <string.h>

void rx_waypoint_track_debug_default(rx_addr track);   /* 00539d20 (stdcall track; RET 4; waypoint_in_progress.c) */

/* ---- the classes' vtables ---- */
#define VT_CHALLENGE        0x009450ccu
#define VT_HILL_CLIMB       0x009451dcu
#define VT_BIG_AIR          0x009452ecu
#define VT_PATHFINDER       0x009453fcu
#define VT_STEP_UP          0x00945514u
#define VT_TRAIL_BLAZER     0x00945624u
#define VT_TARGET_PRACTICE  0x00945734u
#define VT_CHALLENGE_DRIVER 0x009400a4u
#define VT_BOUNDARY_RULE    0x00940188u

/* ---- the strings the original passes ---- */
#define STR_ACTIVITY_INIT      0x00935ea4u   /* "Activity Init" */
#define STR_SHOW_PRE_CHALLENGE 0x009424d4u   /* "ShowPreChallengePopup" */
#define STR_GOTO_HUB           0x009424ecu   /* "GotoHub" */
#define STR_CHALLENGE_AWARD    0x009424f4u   /* "ChallengeAward" */
#define STR_TARGET_FOUND       0x009419b0u   /* "TargetPracticeFound" */
#define STR_VEHICLE_ENTERED    0x00935e84u   /* "VehicleEntered" */
#define STR_TRAILBLAZER_FINISH 0x009425fcu   /* "TrailblazerFinish" */
#define STR_POST_COMPLETE      0x00941c48u   /* "Post State Complete" */
#define STR_RESTART            0x00941ba0u   /* "Restart" */
#define STR_NAME_FORMAT        0x00942504u   /* "%s_%s" */
#define STR_COLLECTIBLES       0x00942640u   /* "MXGame\\Collectibles" */

/* ---- the signatures' type descriptors (typeid(...).name()) ---- */
#define TD_ACTIVITY            0x009cbd24u   /* void(Activity*) */
#define TD_STRING              0x009cdb68u   /* void(const char*) */
#define TD_AWARD               0x009cf774u   /* void(Activity*, MXPlayer*, int, int, int) */
#define TD_TARGET_FOUND        0x009cf7a4u
#define TD_VEHICLE_TRIGGER     0x009cbb7cu   /* void(RavenVehicle*, const MXTrigger*) */
#define TYPE_INFO_NODE         0x009e1a7cu

#define SPEC_ENTITY(A)         U32(U32((A), 0x234), 0x22140)   /* the level's MXChallengesSpecEntity (or 0) */
#define FIRST_PLAYER(A)        U32(U32(U32((A), 0x268), 0), 0x74)   /* the first driver's player */

/* fld dword; fstp dword: the bits, a signalling NaN quieted */
static uint32_t fcopy(uint32_t bits) {
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x007fffffu) != 0) bits |= 0x00400000u;
    return bits;
}
static float fcopy_f(float f) { return rx_bf(fcopy(rx_fb(f))); }

/* a virtual of the activity with no arguments (thiscall) */
static void vcall0(rx_addr obj, uint32_t slot) { (void)act_vcall(obj, slot, 0, 0); }

/* the activity's value (vf62 +0xf8, an x87 return): the IEEE bits as the caller's fstp stores them */
static uint32_t challenge_value_bits(rx_addr A) {
    const rx_addr fn = U32(U32(A, 0), 0xf8);
    float (*f)(rx_addr) = RX_FN(fn, float (*)(rx_addr));
    if (f) {
        float v;
        ACT_CALLR(v, 0, f(A));
        return fcopy(rx_fb(v));
    }
    rx_regs in, out;
    memset(&in, 0, sizeof in);
    in.r[1] = A;
    rx_call_original_ex(fn, &in, &out, 0, 0);
    return fcopy(rx_fb(out.st0));
}

/* the criteria float at an index of the spec entity (10.0 without the entity) */
static float spec_float(rx_addr A, uint32_t index) {
    const rx_addr spec = SPEC_ENTITY(A);
    if (spec == 0) return 10.0f;   /* 0x92fd88 */
    return F32(spec, 4 * index);
}
/* the three medal criteria of the activity's kind and area (vf64's reads): medal 0 at index 36 + 12 kind + area, 1 at +3, 2 at +6 */
static float medal_criterion(rx_addr A, uint32_t medal) {
    return spec_float(A, 36 + 12 * U32(A, 0xff070) + 3 * medal + U32(A, 0x220));
}

/* ======================================================================================================================== */
/* construction                                                                                                              */

/* 00536ca0 (ESI = block): the track block (the activity's +0x890 and +0xff080, the world's +0x21e10): [0x009e3104] guards an
 * InitializeCriticalSection(0x00ae1cf0) (the lock shared by every block; an OS object, not built by the core), the fields, 0x1e0
 * bytes from +0x10 zeroed */
void rx_track_block_construct(rx_addr block) {
    if (U32(0, 0x009e3104) == 0) {
        rr_os_cs_init(0x00ae1cf0);
        SU32(0, 0x009e3104, 1);
    }
    SU32(block, 0x320, 0);
    SU32(block, 8, 0xf);
    SU32(block, 0x2f4, 0);
    SU32(block, 0x2f8, 0);
    SU32(block, 0x2fc, 0);
    SU32(block, 4, 0);
    SU32(block, 0, 0);
    for (uint32_t i = 0; i < 0x1e0; i++) SU8(block + 0x10, i, 0);
}
RX_FUNCTION(0x00536ca0, rx_track_block_construct)

/* 00584e50 (ESI = logic): ChallengeActivityAudioLogic: the ActivityAudioLogic base (vtable 0x009429fc, the cue map +0x8c, +0x50 /
 * +0x54 / +0x90 / +0x94 cleared), its vf32 (00581030), then the class's vtable 0x009431fc */
rx_addr rx_challenge_audio_logic_construct(rx_addr l) {
    ACT_FRAME(L, 0, 1);   /* push ecx */
    SU32(l, 0, 0x009429fc);
    SU32(l, 0x94, 0);
    SU8(l, 4, 0);
    SU32(l, 0x50, 0);
    SU32(l, 0x54, 0);
    SU32(l, 0x8c, 0);
    SU32(l, 0x90, 0);
    ACT_CALL(0, rx_audio_logic_vf32(l));
    SU32(l, 0, 0x009431fc);
    ACT_END(L);
    return l;
}
RX_FUNCTION(0x00584e50, rx_challenge_audio_logic_construct)

/* 0057aa30 (EAX = world, ECX = type; stdcall A; RET 4) -> A: MXActivityChallenge's constructor: Activity (005562c0), the vtable, the
 * kind 0, the track block (+0xff080), the audio logic (+0xfd8, 0x98 bytes) */
rx_addr rx_challenge_construct(rx_addr A, uint32_t type, rx_addr world) {
    ACT_FRAME(L, 0, 2);   /* push esi; push edi */
    ACT_CALL(1, rx_activity_construct(A, type, world));
    SU32(A, 0, VT_CHALLENGE);
    SU32(A, 0xff070, 0);
    rx_track_block_construct(A + 0xff080);
    rx_addr l;
    ACT_CALLR(l, 1, rx_reflex_aligned_alloc(0x98));
    if (l != 0) {
        ACT_CALL(0, rx_challenge_audio_logic_construct(l));
        SU32(A, 0xfd8, l);
    } else {
        SU32(A, 0xfd8, 0);
    }
    ACT_END(L);
    return A;
}
RX_FUNCTION(0x0057aa30, rx_challenge_construct)

/* 0057b740 (ESI = A, EAX = world, ECX = type) -> A: MXActivityPathfinderChallenge: the base, the vtable, the kind 3, +0xff3b0 = 1,
 * the mode fields +0x22c / +0x2c8 = 0, +0xff3d0 = 0.0, and its vector of 4-byte entries (+0xff3b8 storage of 4 * 4 bytes, +0xff3bc
 * size 0, +0xff3c0 / +0xff3c4 capacity 4, +0xff3c8 the limit 0x7fffffff). Frame: push edi; push esi (A, the base's argument). */
rx_addr rx_pathfinder_construct(rx_addr A, uint32_t type, rx_addr world) {
    ACT_FRAME(L, 0, 1);   /* push edi */
    ACT_CALL(1, rx_challenge_construct(A, type, world));
    SU32(A, 0, VT_PATHFINDER);
    SU32(A, 0xff3b8, 0);
    SU32(A, 0xff3bc, 0);
    SU32(A, 0xff3c0, 0);
    SU32(A, 0xff3c4, 0);
    SU32(A, 0xff3c8, 0);
    SU32(A, 0xff070, 3);
    SU32(A, 0xff3b0, 1);
    SU32(A, 0x22c, 0);
    SU32(A, 0x2c8, 0);
    SF32(A, 0xff3d0, 0.0f);
    rx_addr storage;
    ACT_CALLR(storage, 1, rx_reflex_aligned_alloc(0x10));   /* 4 * 4 (the multiplication's overflow check: -1 on overflow) */
    SU32(A, 0xff3b8, storage);
    SU32(A, 0xff3bc, 0);
    SU32(A, 0xff3c0, 4);
    SU32(A, 0xff3c4, 4);
    SU32(A, 0xff3c8, 0x7fffffff);
    ACT_END(L);
    return A;
}
RX_FUNCTION(0x0057b740, rx_pathfinder_construct)

/* 0057c3e0 (ESI = A, EAX = world, ECX = type) -> A: MXActivityStepUpChallenge: the base, its fields +0xff3b0 / +0xff3b4 / +0xff3b8 /
 * +0xff3ec .. +0xff3fc cleared, the vtable, the kind 4. Frame: push ecx; push esi. */
rx_addr rx_step_up_construct(rx_addr A, uint32_t type, rx_addr world) {
    ACT_FRAME(L, 0, 1);   /* push ecx */
    ACT_CALL(1, rx_challenge_construct(A, type, world));
    SU32(A, 0xff3f0, 0);
    SU32(A, 0xff3f8, 0);
    SU32(A, 0xff3fc, 0);
    SU32(A, 0xff3f4, 0);
    SU32(A, 0xff3b0, 0);
    SU32(A, 0xff3b4, 0);
    SU32(A, 0xff3b8, 0);
    SU32(A, 0xff3ec, 0);
    SU32(A, 0, VT_STEP_UP);
    SU32(A, 0xff070, 4);
    ACT_END(L);
    return A;
}
RX_FUNCTION(0x0057c3e0, rx_step_up_construct)

/* 0058a7b0 (stdcall A; RET 4): TrailBlazer's "VehicleEntered" listener on the vehicle-trigger registry [0x00a89bc8]: the proxy
 * (0x14 bytes, vtable 0x00945ce4, the method 0057d3f0, signature void(RavenVehicle*, const MXTrigger*)) joins the event's handler
 * list when the signature names match. Frame: push ecx / ebx / esi / edi before the registry lookup. */
void rx_trail_blazer_listen(rx_addr A) {
    act_subscribe(0x0058a7b0, 0, 1, 4, U32(0, 0x00a89bc8), STR_VEHICLE_ENTERED, 0x00945ce4, TD_VEHICLE_TRIGGER, A, 0x0057d3f0);
}
RX_FUNCTION(0x0058a7b0, rx_trail_blazer_listen)

/* 0058a870 (stdcall A; RET 4): the listener of 0058a7b0 leaves (the aligned 0x14-byte key frame) */
void rx_trail_blazer_unlisten(rx_addr A) {
    act_unlisten(1, U32(0, 0x00a89bc8), STR_VEHICLE_ENTERED, 0x00945ce4, TD_VEHICLE_TRIGGER, A, 0x0057d3f0);
}
RX_FUNCTION(0x0058a870, rx_trail_blazer_unlisten)

/* ======================================================================================================================== */
/* MXActivityChallenge                                                                                                       */

/* 0057aab0 (ECX = A): the destructor's body: the vtable back to MXActivityChallenge, the audio logic destroyed (00580d30) and freed,
 * the track block reset (00538090), Activity's destructor (005566b0) */
void rx_challenge_destroy(rx_addr A) {
    ACT_FRAME(L, 0, 3);   /* push ecx; push esi; push edi */
    const rx_addr l = U32(A, 0xfd8);
    SU32(A, 0, VT_CHALLENGE);
    if (l != 0) {
        ACT_CALL(1, rx_activity_audio_destroy(l));
        ACT_CALL(1, rx_reflex_aligned_free(l));
        SU32(A, 0xfd8, 0);
    }
    ACT_CALL(0, rx_track_block_reset(A + 0xff080));
    ACT_CALL(0, rx_waypoint_activity_base_destroy(A));
    ACT_END(L);
}
RX_FUNCTION(0x0057aab0, rx_challenge_destroy)

/* the scalar deleting destructors: the class's vtable, the body, the block freed with flag 1 (TrailBlazer's listener leaves first) */
static rx_addr challenge_sdtor(rx_addr A, uint32_t flags, rx_addr vtable) {
    ACT_FRAME(L, 0, 1);   /* push esi */
    if (vtable != 0) SU32(A, 0, vtable);
    ACT_CALL(0, rx_challenge_destroy(A));
    if (flags & 1) ACT_CALL(1, rx_reflex_aligned_free(A));
    ACT_END(L);
    return A;
}
/* 0057aa90 MXActivityChallenge::sdtor */
rx_addr rx_challenge_sdtor(rx_addr A, uint32_t flags) { return challenge_sdtor(A, flags, 0); }
RX_FUNCTION(0x0057aa90, rx_challenge_sdtor)
/* 0057b240 MXActivityHillClimbChallenge::sdtor */
rx_addr rx_hill_climb_sdtor(rx_addr A, uint32_t flags) { return challenge_sdtor(A, flags, VT_HILL_CLIMB); }
RX_FUNCTION(0x0057b240, rx_hill_climb_sdtor)
/* 0055eca0 MXActivityBigAirChallenge::sdtor */
rx_addr rx_big_air_sdtor(rx_addr A, uint32_t flags) { return challenge_sdtor(A, flags, VT_BIG_AIR); }
RX_FUNCTION(0x0055eca0, rx_big_air_sdtor)
/* 0057d500 MXActivityTargetPracticeChallenge::sdtor */
rx_addr rx_target_practice_sdtor(rx_addr A, uint32_t flags) { return challenge_sdtor(A, flags, VT_TARGET_PRACTICE); }
RX_FUNCTION(0x0057d500, rx_target_practice_sdtor)

/* 0057d370 MXActivityTrailBlazerChallenge::sdtor: the vtable, the "VehicleEntered" listener leaves (0058a870), then the body */
rx_addr rx_trail_blazer_sdtor(rx_addr A, uint32_t flags) {
    ACT_FRAME(L, 0, 1);   /* push esi */
    SU32(A, 0, VT_TRAIL_BLAZER);
    ACT_CALL(1, rx_trail_blazer_unlisten(A));
    ACT_CALL(0, rx_challenge_destroy(A));
    if (flags & 1) ACT_CALL(1, rx_reflex_aligned_free(A));
    ACT_END(L);
    return A;
}
RX_FUNCTION(0x0057d370, rx_trail_blazer_sdtor)

/* 0057b1a0 (stdcall A; RET 4): the challenge's track: block +0 = the world; the name "<level>_<kind name>" (0078fd60 into a 0x104-byte
 * buffer); when the database has the .tdf07 of kind 0xd (the "chl" suffix) for it, area 0, it is loaded into the block (00536ef0).
 * Frame: push ebp; mov ebp, esp; and esp, -8; sub esp, 0x114 (the cookie at +0x110); push ebx / esi / edi; the buffer at +0x10. */
void rx_challenge_load_track(rx_addr A) {
    const rx_addr entry = rx_sp();
    const rx_addr S = ((entry - 4) & ~(rx_addr)7) - 0x114;   /* the frame after the sub */
    SU32(S, 0x110, U32(0, 0x009b45e4) ^ S);                    /* __security_cookie ^ esp */
    rx_set_sp(S - 12);                                          /* push ebx; push esi; push edi */
    const rx_addr world = U32(A, 0x234);
    const rx_addr block = A + 0xff080;
    const rx_addr buf = S + 0x10;
    SU32(block, 0, world);
    {   /* 0078fd60 (EDI = buf, ESI = 0x104; cdecl "%s_%s", level, kind name) */
        const rx_addr sp = rx_sp();
        SU32(sp, -12, STR_NAME_FORMAT);
        SU32(sp, -8, world + 0x20030);
        SU32(sp, -4, U32(0, 0x009cf540 + 4 * U32(A, 0xff070)));
        rx_set_sp(sp - 16);
        rx_format_buffer(buf, 0x104, STR_NAME_FORMAT, sp - 8);
        rx_set_sp(sp);
    }
    uint32_t has;
    ACT_CALLR(has, 1, rx_track_has_asset(0xd, buf, 0));         /* 00536e80 (ECX = kind, EDX = name; area) */
    if (has != 0) ACT_CALL(3, (void)rx_track_load(buf, block, 0xd, 0));   /* 00536ef0 (ECX = name; block, kind, area) */
    rx_set_sp(entry);
}
RX_FUNCTION(0x0057b1a0, rx_challenge_load_track)

/* 0057ab00 MXActivityChallenge::vf2 (ECX = A; arg; RET 4): Activity::vf2, the class's start entity pose (vf61), the challenge's track
 * (0057b1a0), "Activity Init" (the activity) on the game's channel [0x00a93f74], then the UI events "ShowPreChallengePopup" and
 * "GotoHub" (each found in the UI manager's event list, 005db500, and raised on its channel +0x98 with the event as the payload).
 * The payloads live in the argument slot. Frame: push ebx / ebp / esi / edi. */
void rx_challenge_vf2(rx_addr A, rx_addr arg) {
    ACT_FRAME(L, 0, 4);
    const rx_addr slot = L + 0x14;   /* the argument's slot (entry ESP + 4) */
    ACT_CALL(1, rx_activity_vf2(A, arg));
    vcall0(A, 0xf4);                 /* vf61 */
    ACT_CALL(1, rx_challenge_load_track(A));
    SU32(slot, 0, A);
    act_raise(U32(0, 0x00a93f74), STR_ACTIVITY_INIT, TD_ACTIVITY, slot, 4);
    /* UI BOUNDARY: the UI manager's events (005db500: ESI = the UI manager; the name; RET 4 -> the event or 0). The product has no UI
     * manager and no UI event channel: act_ui_call_ex returns 0 there and nothing is raised. The pages these events open only write UI
     * state; the challenge runs without them (the reference boots reach InProgress with the popup shown). */
    static const rx_addr names[2] = {STR_SHOW_PRE_CHALLENGE, STR_GOTO_HUB};
    for (int k = 0; k < 2; k++) {
        const rx_addr ui = U32(0, 0x00a93fcc);
        rx_regs in, out;
        memset(&in, 0, sizeof in);
        in.r[6] = ui;
        const uint32_t a[1] = {names[k]};
        act_ui_call_ex(0x005db500, &in, &out, 1, a);
        const rx_addr ev = out.r[0];
        if (ev != 0) {
            SU32(slot, 0, ev);
            act_raise(U32(ui, 0x98), ev, TD_STRING, slot, 4);   /* EDI = the UI's event name, also the payload */
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x0057ab00, rx_challenge_vf2)

/* 0057abd0 MXActivityChallenge::vf4 (ECX = A): the "ChallengeAward" event type on the activity's channel (the template 00592940, a
 * tail jump: it enters with this function's return address) */
void rx_challenge_vf4(rx_addr A) {
    const rx_addr sp = rx_sp();
    rx_set_sp(sp + 4);   /* the instantiation's entry ESP is this function's (act_event_type pushes a return address) */
    act_event_type(0x00592940, U32(A, 0x290), 0);
    rx_set_sp(sp);
}
RX_FUNCTION(0x0057abd0, rx_challenge_vf4)

/* 0057ac50 (EAX = kind) -> the type of the kind's start entity (HillClimbStartEntity 0x101c ...), 0 for kind 7 and above 9 */
uint32_t rx_challenge_start_type(uint32_t kind) {
    switch (kind) {
    case 0: return 0x101b;
    case 1: return 0x101c;
    case 2: return 0x101d;
    case 3: return 0x101e;
    case 4: return 0x1023;
    case 5: return 0x101f;
    case 6: return 0x1021;
    case 8: return 0x1020;
    case 9: return 0x1022;
    default: return 0;
    }
}
RX_FUNCTION(0x0057ac50, rx_challenge_start_type)

/* 0057acc0 MXActivityChallenge::vf61 (ECX = A): outside Pathfinder (kind 3) the start entity of the kind in the activity's group
 * (+0x118) of the scene (world + 0x20138; else the first entity of type 0x1014 of the whole scene): the position (its matrix +0x30)
 * snapped to the terrain (0042d960, 007c5950) goes to +0x340, the direction (+0x20) to +0x350.
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x74; push ebx / esi / edi. Locals (from the ESP after the pushes): the
 * entity list (16 entries) +0x40, the overflow flag +0xc, the position +0x10, the matrix copies +0x20 / +0x30. */
void rx_challenge_vf61(rx_addr A) {
    ACT_FRAME_EBP(L, 16, 0x74, 3);
    if (U32(A, 0xff070) == 3) { ACT_END(L); return; }
    const rx_addr scene = U32(U32(A, 0x234), 0x20138);
    const rx_addr list = L + 0x40;
    SU32(list, 0, 0);
    for (uint32_t i = 4; i < 0x40; i++) SU8(list, i, 0);       /* memset(list + 4, 0, 0x3c) */
    const uint32_t type = rx_challenge_start_type(U32(A, 0xff070));
    uint32_t n;
    ACT_CALLR(n, 6, rx_scene_group_query(scene, type, list, 0x10, A + 0x118, 0));   /* 007d2820 */
    if (n == 0) {
        ACT_CALLR(n, 4, rx_scene_manager_collect_typed(scene, 0x1014, list, 0x10, L + 0xc));   /* 007d2920 */
        if (n == 0) { ACT_END(L); return; }
    }
    const rx_addr e = U32(list, 0);
    rx_addr m = act_vcall(e, 0x38, 0, 0);                      /* vf14: the entity's matrix */
    SU32(L, 0x20, U32(m, 0x30)); SU32(L, 0x24, U32(m, 0x34)); SU32(L, 0x28, U32(m, 0x38));
    m = act_vcall(e, 0x38, 0, 0);
    SU32(L, 0x30, U32(m, 0x20)); SU32(L, 0x34, U32(m, 0x24)); SU32(L, 0x38, U32(m, 0x28));
    (void)act_vcall(e, 0x38, 0, 0);
    SU32(L, 0x10, U32(L, 0x20)); SU32(L, 0x14, U32(L, 0x24)); SU32(L, 0x18, U32(L, 0x28));
    if ((U8(0, 0x009e30e0) & 1) == 0) {                        /* the terrain list's static guard (00402d90 inline) */
        SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1);
        SU32(0, 0x009e30dc, 0);
    }
    {   /* push &position; 0042d960 (EDX = 0x009e30d4, EDI = scene); 007c5950 (ECX = the terrain; the pushed position) */
        const rx_addr sp = rx_sp();
        SU32(sp, -4, L + 0x10);
        rx_set_sp(sp - 4);
        rx_addr terrain;
        ACT_CALLR(terrain, 0, rx_terrain_for_scene(0x009e30d4, scene));
        uint32_t edx = L + 0x10;
        rx_set_sp(sp - 8);
        (void)phys_terrain_base_height_at(terrain, L + 0x10, &edx);
        rx_set_sp(sp);
    }
    SU32(A, 0x340, U32(L, 0x10)); SU32(A, 0x344, U32(L, 0x14)); SU32(A, 0x348, U32(L, 0x18));
    SU32(A, 0x350, U32(L, 0x30)); SU32(A, 0x354, U32(L, 0x34)); SU32(A, 0x358, U32(L, 0x38));
    ACT_END(L);
}
RX_FUNCTION(0x0057acc0, rx_challenge_vf61)

/* 0057ae60 MXActivityChallenge::vf3 (ECX = A): every driver leaves (0055c1d0 with the first driver until none is left), then
 * Activity::vf3 (a tail jump) */
void rx_challenge_vf3(rx_addr A) {
    ACT_FRAME(L, 0, 1);   /* push esi */
    while (I32(A, 0x26c) > 0) ACT_CALL(2, rx_activity_on_player_left(A, U32(U32(A, 0x268), 0)));
    ACT_END(L);
    rx_activity_vf3(A);
}
RX_FUNCTION(0x0057ae60, rx_challenge_vf3)

/* 0057ae90 MXActivityChallenge::vf18 (ECX = A; also TargetPractice's): the medals cleared, Activity::vf18 (a tail jump) */
void rx_challenge_vf18(rx_addr A) {
    SU32(A, 0xff074, 0);
    SU32(A, 0xff078, 0);
    SU32(A, 0xff07c, 0);
    rx_activity_vf18(A);
}
RX_FUNCTION(0x0057ae90, rx_challenge_vf18)

/* 0057aeb0 MXActivityChallenge::vf44 (ECX = A; player; RET 4) -> a ChallengeDriver: 0xf10 bytes (ReflexAlignedAlloc), NetworkDriver's
 * constructor (0050e1b0, EDI = the block), the vtable 0x009400a4, +0xee8 = 12 */
rx_addr rx_challenge_vf44(rx_addr A, rx_addr player) {
    (void)A;
    ACT_FRAME(L, 0, 1);   /* push edi */
    rx_addr d;
    ACT_CALLR(d, 1, rx_reflex_aligned_alloc(0xf10));
    if (d == 0) { ACT_END(L); return 0; }
    ACT_CALL(1, (void)rx_network_driver_construct(d, player));
    SU32(d, 0, VT_CHALLENGE_DRIVER);
    SU32(d, 0xee8, 0xc);
    ACT_END(L);
    return d;
}
RX_FUNCTION(0x0057aeb0, rx_challenge_vf44)

/* 0058a570 (ECX = driver) -> a RuleChallengeBoundary (0x20 bytes from the driver's block +0x3e4, vtable 0x00940188, +0xc 0, +0x10 0.0,
 * +0x18 0) appended to the rules (+0xa0, count +0xee4, at most 0x12; 0 when full) */
rx_addr rx_challenge_rule_get(rx_addr d) {
    if (U32(d, 0xee4) >= 0x12) return 0;
    const rx_addr o = U32(d, 0x3e4);
    SU32(d, 0x3e4, o + 0x20);
    rx_addr r = 0;
    if (o != 0) {
        SU32(o, 0, VT_BOUNDARY_RULE);
        SU32(o, 0xc, 0);
        SF32(o, 0x10, 0.0f);
        SU32(o, 0x18, 0);
        r = o;
    }
    SU32(d, 0xa0 + 4 * U32(d, 0xee4), r);
    r = U32(d, 0xa0 + 4 * U32(d, 0xee4));
    SU32(d, 0xee4, U32(d, 0xee4) + 1);
    return r;
}
RX_FUNCTION(0x0058a570, rx_challenge_rule_get)

/* 0057aef0 MXActivityChallenge::vf47 (ECX = A; driver; RET 4): the driver's boundary rule (0058a570), bound to the driver (vf1) */
void rx_challenge_vf47(rx_addr A, rx_addr d) {
    (void)A;
    ACT_FRAME(L, 0, 1);   /* push esi */
    rx_addr rule;
    ACT_CALLR(rule, 0, rx_challenge_rule_get(d));
    (void)act_vcall(rule, 4, 1, &d);
    ACT_END(L);
}
RX_FUNCTION(0x0057aef0, rx_challenge_vf47)

/* 0057af10 MXActivityChallenge::vf38 (Countdown enter; ECX = A; old state; RET 4): the countdown 3.0 s (+0x2b0) of 3 steps (+0x2b8),
 * Activity::vf38 (a tail jump) */
void rx_challenge_vf38(rx_addr A, uint32_t state) {
    (void)state;
    SF32(A, 0x2b0, 3.0f);   /* 0x92fe48 */
    SU32(A, 0x2b8, 3);
    rx_activity_countdown_enter(A);
}
RX_FUNCTION(0x0057af10, rx_challenge_vf38)

/* 0057af30 MXActivityChallenge::vf39 (InProgress enter): Activity::vf39 */
void rx_challenge_vf39(rx_addr A, uint32_t state) { (void)state; rx_activity_in_progress_enter(A); }
RX_FUNCTION(0x0057af30, rx_challenge_vf39)

/* 0057b020 (vf40, WindDown enter): Activity::vf40 */
void rx_challenge_vf40(rx_addr A, uint32_t state) { (void)state; rx_activity_wind_down_enter(A); }
RX_FUNCTION(0x0057b020, rx_challenge_vf40)

/* 0057af40 MXActivityChallenge::vf23 (ECX = A; dt; RET 4): Activity::vf23, then with the challenge track loaded (+0xff3a0) its debug
 * traversals: 005399e0 when the block's +0x2fc is set, 00539d20 when the debug switch [0x00af6404] is.
 * Frame: push ebp; mov ebp, esp; and esp, -8; push ecx; push esi. */
void rx_challenge_vf23(rx_addr A, float dt) {
    ACT_FRAME_EBP(L, 8, 4, 1);
    ACT_CALL(1, rx_activity_update(A, fcopy_f(dt)));
    if (U32(A, 0xff3a0) != 0) {
        const rx_addr block = A + 0xff080;
        if (U32(block, 0x2fc) != 0) ACT_CALL(0, rx_waypoint_track_debug(block));
        if (U32(0, 0x00af6404) != 0) ACT_CALL(1, rx_waypoint_track_debug_default(block));
    }
    ACT_END(L);
}
RX_FUNCTION(0x0057af40, rx_challenge_vf23)

/* 0057d750 (vf23 of the subclasses; ECX = A; dt; RET 4): MXActivityChallenge::vf23, then vf65 and vf66 */
void rx_challenge_update_tail(rx_addr A, float dt) {
    ACT_FRAME(L, 0, 1);   /* push esi */
    ACT_CALL(1, rx_challenge_vf23(A, fcopy_f(dt)));
    vcall0(A, 0x104);
    vcall0(A, 0x108);
    ACT_END(L);
}
RX_FUNCTION(0x0057d750, rx_challenge_update_tail)

/* 0057af90 MXActivityChallenge::vf33 (InProgress exit; ECX = A; old state; RET 4): Activity::vf33, then the first player's respawn
 * pose saved (00550e10: a tail jump, the argument slot replaced by the player) */
void rx_challenge_vf33(rx_addr A, uint32_t state) {
    const rx_addr entry = rx_sp();
    ACT_FRAME(L, 0, 1);   /* push esi */
    ACT_CALL(1, rx_activity_in_progress_exit(A, state));
    ACT_END(L);
    const rx_addr player = FIRST_PLAYER(A);
    SU32(entry, 4, player);
    rx_player_save_respawn_pose(player);
}
RX_FUNCTION(0x0057af90, rx_challenge_vf33)

/* 0057afc0 MXActivityChallenge::vf15 (ECX = A): vf61, then Activity::vf15 (a tail jump) */
void rx_challenge_vf15(rx_addr A) {
    ACT_FRAME(L, 0, 1);   /* push esi */
    vcall0(A, 0xf4);
    ACT_END(L);
    rx_activity_vf15(A);
}
RX_FUNCTION(0x0057afc0, rx_challenge_vf15)

/* 0057afe0 MXActivityChallenge::vf16 (restart; ECX = A): each driver's player to its start zone (0054cd20), then Activity::vf16 */
void rx_challenge_vf16(rx_addr A) {
    ACT_FRAME(L, 0, 2);   /* push esi; push edi */
    for (int32_t i = 0; i < I32(A, 0x26c); i++)
        ACT_CALL(1, rx_player_start_zone(U32(U32(U32(A, 0x268), 4u * (uint32_t)i), 0x74)));
    ACT_END(L);
    rx_activity_vf16(A);
}
RX_FUNCTION(0x0057afe0, rx_challenge_vf16)

/* 0057b030 MXActivityChallenge::vf27 (WindDown update; ECX = A; dt; RET 4): vf66, then Activity::vf27 */
void rx_challenge_vf27(rx_addr A, float dt) {
    ACT_FRAME(L, 0, 1);   /* push esi */
    vcall0(A, 0x108);
    ACT_CALL(1, rx_activity_wind_down(A, fcopy_f(dt)));
    ACT_END(L);
}
RX_FUNCTION(0x0057b030, rx_challenge_vf27)

/* 0058a5d0 (ECX = driver, EDX = medal 2, ESI = channel; stdcall A, medal 1, medal 0; RET 0xc): "ChallengeAward" with the payload
 * {A, driver, medal 2, medal 1, medal 0} (0x14 bytes at the frame). Frame: sub esp, 0x18 (the cookie at +0x14); push edi. */
void rx_challenge_award_raise(rx_addr channel, rx_addr driver, uint32_t m2, rx_addr A, uint32_t m1, uint32_t m0) {
    const rx_addr entry = rx_sp();
    const rx_addr S = entry - 0x18;
    SU32(S, 0x14, U32(0, 0x009b45e4) ^ S);
    SU32(S, 4, driver);
    rx_set_sp(S - 4);                   /* push edi */
    SU32(S, 0, A);
    SU32(S, 0xc, m1);
    SU32(S, 8, m2);
    SU32(S, 0x10, m0);
    act_raise(channel, STR_CHALLENGE_AWARD, TD_AWARD, S, 0x14);
    rx_set_sp(entry);
}
RX_FUNCTION(0x0058a5d0, rx_challenge_award_event)

/* 0057b050 MXActivityChallenge::vf41 (Post enter; ECX = A; old state; RET 4): Activity::vf41; vf64 fills the medal flags (+0xff07c,
 * +0xff078, +0xff074); each medal won is awarded to the first player with the value (vf62; 0054e670: medal 0 / 1 / 2); with any medal
 * "ChallengeAward" is raised (0058a5d0). Frame: push ebx / ebp / esi / edi; the player kept in the argument slot. */
void rx_challenge_vf41(rx_addr A, uint32_t state) {
    ACT_FRAME(L, 0, 4);
    const rx_addr slot = L + 0x14;
    ACT_CALL(1, rx_activity_post_enter(A));
    (void)state;
    {   /* vf64 (+0x100): (&+0xff07c, &+0xff078, &+0xff074) */
        const uint32_t a[3] = {A + 0xff07c, A + 0xff078, A + 0xff074};
        (void)act_vcall(A, 0x100, 3, a);
    }
    SU32(slot, 0, FIRST_PLAYER(A));
    static const uint32_t flags[3] = {0xff07c, 0xff078, 0xff074};
    for (uint32_t medal = 0; medal < 3; medal++) {
        if (U32(A, flags[medal]) == 0) continue;
        const uint32_t value = challenge_value_bits(A);
        ACT_CALL(4, rx_challenge_award(U32(slot, 0), U32(A, 0xff070), medal, value));
    }
    const uint32_t m0 = U32(A, 0xff07c);
    if (m0 != 0 || U32(A, 0xff078) != 0 || U32(A, 0xff074) != 0)
        ACT_CALL(3, rx_challenge_award_raise(U32(A, 0x290), U32(U32(A, 0x268), 0), U32(A, 0xff074), A, U32(A, 0xff078), m0));
    ACT_END(L);
}
RX_FUNCTION(0x0057b050, rx_challenge_vf41)

/* 0057b140 MXActivityChallenge::vf52: [0x009c47c0] = -1 unless [0x009c47bc] is -1 */
void rx_challenge_vf52(rx_addr A) {
    (void)A;
    if (U32(0, 0x009c47bc) != 0xffffffffu) SU32(0, 0x009c47c0, 0xffffffffu);
}
RX_FUNCTION(0x0057b140, rx_challenge_vf52)

/* 0057c2f0 (vf32, Countdown exit; ECX = A; old state; RET 4): Activity::vf32, then the game's grid pass (0075f890: type 9, the
 * activity's mode, 0) */
void rx_challenge_vf32(rx_addr A, uint32_t state) {
    (void)state;
    ACT_FRAME(L, 0, 1);   /* push esi */
    ACT_CALL(1, rx_activity_countdown_exit(A));
    ACT_CALL(4, rx_game_grid_pass(U32(0, 0x00a946b8), 9, U32(A, 0x238), 0));
    ACT_END(L);
}
RX_FUNCTION(0x0057c2f0, rx_challenge_vf32)

/* 0057c220 (vf1): a jump to Activity::vf1 */
void rx_challenge_vf1(rx_addr A, rx_addr ref) { rx_activity_vf1(A, ref); }
RX_FUNCTION(0x0057c220, rx_challenge_vf1)

/* 005189f0 (fldz; the value of the challenges without one, also the drivers' vf10 / vf13): 0.0 */
float rx_challenge_value_zero(rx_addr A) { (void)A; return 0.0f; }
RX_FUNCTION(0x005189f0, rx_challenge_value_zero)

/* 0057b160 (ECX = A, ESI = medal) -> xmm0: the criteria float of the medal (index 3 * (medal + 4 kind + 12) + area; 10.0 without the
 * spec entity) */
float rx_challenge_medal_value(rx_addr A, uint32_t medal) {
    return spec_float(A, 3 * (medal + 4 * U32(A, 0xff070) + 12) + U32(A, 0x220));
}
RX_FUNCTION(0x0057b160, rx_challenge_medal_value)

/* ======================================================================================================================== */
/* the shared Activity methods                                                                                               */

/* 00794f60 Activity::vf19 (the respawn point): 0 (RET 0x10) */
uint32_t rx_activity_vf19(rx_addr A, rx_addr player, rx_addr pos, rx_addr dir, rx_addr up) {
    (void)A; (void)player; (void)pos; (void)dir; (void)up;
    return 0;
}
RX_FUNCTION(0x00794f60, rx_activity_vf19)

/* 00559a80 Activity::vf58 (ECX = A): "Post State Complete" (the activity, in the push ecx slot) on the activity's channel.
 * Frame: push ecx; push esi; push edi. */
void rx_activity_vf58(rx_addr A) {
    ACT_FRAME(L, 0, 3);
    SU32(L, 8, A);
    act_raise(U32(A, 0x290), STR_POST_COMPLETE, TD_ACTIVITY, L + 8, 4);
    ACT_END(L);
}
RX_FUNCTION(0x00559a80, rx_activity_vf58)

/* ======================================================================================================================== */
/* MXActivityHillClimbChallenge (0x16): the height climbed from the start (+0xff3b4) is the value; the time limit ends it         */

/* 0057b510 MXActivityHillClimbChallenge::vf1 (ECX = A; &row ref; RET 4): Activity::vf1, then the duration (+0x2bc) from the spec
 * entity's time limit (index 3 * (kind + 0x43) + area) */
void rx_hill_climb_vf1(rx_addr A, rx_addr ref) {
    ACT_FRAME(L, 0, 1);   /* push esi */
    ACT_CALL(1, rx_activity_vf1(A, ref));
    const uint32_t index = 3 * (U32(A, 0xff070) + 0x43) + U32(A, 0x220);
    SU32(A, 0x2bc, fcopy(U32(SPEC_ENTITY(A), 4 * index)));
    ACT_END(L);
}
RX_FUNCTION(0x0057b510, rx_hill_climb_vf1)

/* 0057b320 MXActivityHillClimbChallenge::vf18 (ECX = A): the medals cleared, Activity::vf18; +0xff3b4 = 0, the best (+0xff3b0) 0,
 * the start height +0xff3b4 = the first player's vehicle height (+0xc4) */
void rx_hill_climb_vf18(rx_addr A) {
    ACT_FRAME(L, 0, 1);   /* push esi */
    SU32(A, 0xff074, 0);
    SU32(A, 0xff078, 0);
    SU32(A, 0xff07c, 0);
    ACT_CALL(0, rx_activity_vf18(A));
    SF32(A, 0xff3b4, 0.0f);
    const uint32_t h = U32(U32(FIRST_PLAYER(A), 0x1ac), 0xc4);
    SF32(A, 0xff3b0, 0.0f);
    SU32(A, 0xff3b4, fcopy(h));
    ACT_END(L);
}
RX_FUNCTION(0x0057b320, rx_hill_climb_vf18)

/* 0057b270 MXActivityHillClimbChallenge::vf28 (InProgress update; ECX = A; dt; RET 4): Activity::vf28; the best height (+0xff3b0) =
 * max(best, height - start); the player's wreck (+0x116c > 0 with the wreck kind +0x1170 == 2) ends it (state 4) */
void rx_hill_climb_vf28(rx_addr A, float dt) {
    ACT_FRAME(L, 0, 1);   /* push esi */
    ACT_CALL(1, rx_activity_in_progress(A, fcopy_f(dt)));
    const rx_addr player = FIRST_PLAYER(A);
    const float climbed = F32(U32(player, 0x1ac), 0xc4) - F32(A, 0xff3b4);
    const float best = F32(A, 0xff3b0);
    SF32(A, 0xff3b0, best > climbed ? best : climbed);
    const rx_addr p = FIRST_PLAYER(A);
    if (F32(p, 0x116c) > 0.0f && U32(p, 0x1170) == 2) ACT_CALL(2, rx_activity_set_state(A, 4));
    ACT_END(L);
}
RX_FUNCTION(0x0057b270, rx_hill_climb_vf28)

/* 0057b470 MXActivityHillClimbChallenge::vf42 (the finish check; ECX = A): in the modes 1 and outside 0..3: the clock (+0x294) at the
 * duration (+0x2bc) ends it (state 4); else with the best at the gold criterion (medal 2's +0xa8 entry) the best is set to the
 * criterion (0057b160, medal 2) and it ends */
void rx_hill_climb_vf42(rx_addr A) {
    const uint32_t mode = U32(A, 0x238);
    if (mode == 0) return;
    if ((int32_t)mode > 1 && (int32_t)mode <= 3) return;
    if (F32(A, 0x294) >= F32(A, 0x2bc)) {
        ACT_CALL(2, rx_activity_set_state(A, 4));
        return;
    }
    ACT_FRAME(L, 0, 1);   /* push esi */
    const float criterion = medal_criterion(A, 2);
    if (F32(A, 0xff3b0) >= criterion) {
        float v;
        ACT_CALLR(v, 0, rx_challenge_medal_value(A, 2));
        SF32(A, 0xff3b0, v);
        ACT_CALL(2, rx_activity_set_state(A, 4));
    }
    ACT_END(L);
}
RX_FUNCTION(0x0057b470, rx_hill_climb_vf42)

/* 0055ed00 MXActivityHillClimbChallenge::vf62 (also TrailBlazer's): the value +0xff3b0 */
float rx_hill_climb_value(rx_addr A) { return F32(A, 0xff3b0); }
RX_FUNCTION(0x0055ed00, rx_hill_climb_value)

/* 0057b370 MXActivityHillClimbChallenge::vf64 (ECX = A; three medal flags; RET 0xc): m2 = best >= criterion 2, m1 = best > criterion 1,
 * m0 = best > criterion 0 */
void rx_hill_climb_vf64(rx_addr A, rx_addr m0, rx_addr m1, rx_addr m2) {
    const float c2 = medal_criterion(A, 2);
    SU32(m2, 0, F32(A, 0xff3b0) >= c2 ? 1 : 0);
    const float c1 = medal_criterion(A, 1);
    SU32(m1, 0, F32(A, 0xff3b0) > c1 ? 1 : 0);
    const float c0 = medal_criterion(A, 0);
    SU32(m0, 0, F32(A, 0xff3b0) > c0 ? 1 : 0);
}
RX_FUNCTION(0x0057b370, rx_hill_climb_vf64)

/* ======================================================================================================================== */
/* MXActivityBigAirChallenge (0x15): one jump; the stunt system's height is the value                                         */

/* 0057b5e0 MXActivityBigAirChallenge::vf18 (ECX = A): the medals cleared, Activity::vf18, the jump state 0, the first player's stunt
 * system reset (006ca750, player + 0xc70) */
void rx_big_air_vf18(rx_addr A) {
    ACT_FRAME(L, 0, 2);   /* push esi; push edi */
    SU32(A, 0xff074, 0);
    SU32(A, 0xff078, 0);
    SU32(A, 0xff07c, 0);
    ACT_CALL(0, rx_activity_vf18(A));
    SU32(A, 0xff3b0, 0);
    ACT_CALL(0, rx_stunt_system_reset(FIRST_PLAYER(A) + 0xc70));
    ACT_END(L);
}
RX_FUNCTION(0x0057b5e0, rx_big_air_vf18)

/* 0057b550 MXActivityBigAirChallenge::vf28 (ECX = A; dt; RET 4): Activity::vf28; state 0: the controls' airborne flags (player +0xb58:
 * +0x68 or +0x1c) start the jump (state 1); state 1: landed (+0xbc0 and +0xb74 clear) and the vehicle's vf43 false ends it (state 3) */
void rx_big_air_vf28(rx_addr A, float dt) {
    ACT_FRAME(L, 0, 1);   /* push esi */
    ACT_CALL(1, rx_activity_in_progress(A, fcopy_f(dt)));
    const uint32_t state = U32(A, 0xff3b0);
    if (state == 0) {
        const rx_addr c = FIRST_PLAYER(A) + 0xb58;
        if (U32(c, 0x68) != 0 || U32(c, 0x1c) != 0) SU32(A, 0xff3b0, 1);
    } else if (state == 1) {
        const rx_addr p = FIRST_PLAYER(A);
        if (U32(p, 0xbc0) == 0 && U32(p, 0xb74) == 0) {
            if (act_vcall(U32(p, 0x1ac), 0xac, 0, 0) == 0) ACT_CALL(2, rx_activity_set_state(A, 3));
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x0057b550, rx_big_air_vf28)

/* 0057b620 MXActivityBigAirChallenge::vf64 (ECX = A; three medal flags; RET 0xc): the jump's value 3.3 * the stunt system's three
 * heights (player +0xc8c, +0xc88, +0xc84; 100.0 from 99.0 up); each medal when the value reaches its criterion */
void rx_big_air_vf64(rx_addr A, rx_addr m0, rx_addr m1, rx_addr m2) {
    const float c2 = medal_criterion(A, 2);
    const float c1 = medal_criterion(A, 1);
    const float c0 = medal_criterion(A, 0);
    const rx_addr p = FIRST_PLAYER(A);
    const float k = 3.3f;   /* 0x977760 */
    float v = F32(p, 0xc8c) * k + F32(p, 0xc88) * k;
    v = v + F32(p, 0xc84) * k;
    if (v >= 99.0f) v = 100.0f;   /* 0x97775c, 0x93005c */
    SU32(m2, 0, v >= c2 ? 1 : 0);
    SU32(m1, 0, v >= c1 ? 1 : 0);
    SU32(m0, 0, v >= c0 ? 1 : 0);
}
RX_FUNCTION(0x0057b620, rx_big_air_vf64)

/* ======================================================================================================================== */
/* MXActivityTrailBlazerChallenge (0x1b): the clock to the "TrailblazerFinish" trigger is the value (lower is better)        */

/* 0057d3c0 MXActivityTrailBlazerChallenge::vf18 (ECX = A): the medals cleared, Activity::vf18, the time 0 */
void rx_trail_blazer_vf18(rx_addr A) {
    ACT_FRAME(L, 0, 1);   /* push esi */
    SU32(A, 0xff074, 0);
    SU32(A, 0xff078, 0);
    SU32(A, 0xff07c, 0);
    ACT_CALL(0, rx_activity_vf18(A));
    SF32(A, 0xff3b0, 0.0f);
    ACT_END(L);
}
RX_FUNCTION(0x0057d3c0, rx_trail_blazer_vf18)

/* 0057d3a0 MXActivityTrailBlazerChallenge::vf28 (ECX = A; dt; RET 4): Activity::vf28, the time = the clock (+0x294) */
void rx_trail_blazer_vf28(rx_addr A, float dt) {
    ACT_FRAME(L, 0, 1);   /* push esi */
    ACT_CALL(1, rx_activity_in_progress(A, fcopy_f(dt)));
    SU32(A, 0xff3b0, fcopy(U32(A, 0x294)));
    ACT_END(L);
}
RX_FUNCTION(0x0057d3a0, rx_trail_blazer_vf28)

/* 0057d3f0 (the "VehicleEntered" handler; ECX = A; vehicle, trigger; RET 8): the trigger named "TrailblazerFinish" (+0xc0) ends the
 * challenge: a tail jump to 005586c0 (A, 4) through the two argument slots */
void rx_trail_blazer_vehicle_entered(rx_addr A, rx_addr vehicle, rx_addr trigger) {
    (void)vehicle;
    const rx_addr entry = rx_sp();
    ACT_FRAME(L, 0, 1);   /* push ebx */
    const int differ = crt_strcmp(trigger + 0xc0, STR_TRAILBLAZER_FINISH) != 0;
    ACT_END(L);
    if (differ) return;
    SU32(entry, 8, 4);
    SU32(entry, 4, A);
    rx_activity_set_state(A, 4);
}
RX_FUNCTION(0x0057d3f0, rx_trail_blazer_vehicle_entered)

/* 0057d440 MXActivityTrailBlazerChallenge::vf64 (ECX = A; three medal flags; RET 0xc): each flag set (never cleared) when the time is
 * under the medal's criterion */
void rx_trail_blazer_vf64(rx_addr A, rx_addr m0, rx_addr m1, rx_addr m2) {
    const float c2 = medal_criterion(A, 2);
    const float c1 = medal_criterion(A, 1);
    const float c0 = medal_criterion(A, 0);
    if (c2 > F32(A, 0xff3b0)) SU32(m2, 0, 1);
    if (c1 > F32(A, 0xff3b0)) SU32(m1, 0, 1);
    if (c0 > F32(A, 0xff3b0)) SU32(m0, 0, 1);
}
RX_FUNCTION(0x0057d440, rx_trail_blazer_vf64)

/* ======================================================================================================================== */
/* MXActivityTargetPracticeChallenge (0x1d): the targets hit are the value; all of them or the time limit end it             */

/* 0057d5b0 MXActivityTargetPracticeChallenge::vf1 (ECX = A; &row ref; RET 4): Activity::vf1, the duration from the spec entity, then
 * the collectables' tuning ("MXGame\\Collectibles"): CollectibleCollisionRadius ([0x009ca324], squared), CollectibleCollisionRider-
 * HeadRadius ([0x009ca328], squared), CollectibleRiderHeadCollisionYOffset ([0x009c5244]) through the registry ([0x00a95844] +0xc:
 * vf1 finds the record, vf17 reads one float) */
void rx_target_practice_vf1(rx_addr A, rx_addr ref) {
    ACT_FRAME(L, 0, 1);   /* push esi */
    ACT_CALL(1, rx_activity_vf1(A, ref));
    const uint32_t index = 3 * (U32(A, 0xff070) + 0x43) + U32(A, 0x220);
    SU32(A, 0x2bc, fcopy(U32(SPEC_ENTITY(A), 4 * index)));
    const rx_addr reg = U32(U32(0, 0x00a95844), 0xc);
    static const uint32_t keys[3][3] = {{0x00942624, 0x009ca324, 1}, {0x00942654, 0x009ca328, 1}, {0x00942678, 0x009c5244, 0}};
    for (int k = 0; k < 3; k++) {
        const uint32_t find[3] = {STR_COLLECTIBLES, keys[k][0], 1};
        const rx_addr rec = act_vcall(reg, 4, 3, find);
        if (rec == 0) continue;
        const uint32_t read[3] = {rec, keys[k][1], 1};
        (void)act_vcall(reg, 0x44, 3, read);
        if (keys[k][2]) {
            const float r = F32(0, keys[k][1]);
            SF32(0, keys[k][1], r * r);
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x0057d5b0, rx_target_practice_vf1)

/* 0057d530 MXActivityTargetPracticeChallenge::vf2: MXActivityChallenge::vf2 */
void rx_target_practice_vf2(rx_addr A, rx_addr arg) { rx_challenge_vf2(A, arg); }
RX_FUNCTION(0x0057d530, rx_target_practice_vf2)

/* 0057d7b0 (EAX = A): the targets (+0xff3b4 .. +0xff430) and their count cleared */
void rx_target_practice_clear(rx_addr A) {
    for (uint32_t o = 0xff3b4; o <= 0xff430; o += 4) SU32(A, o, 0);
    SU32(A, 0xff3b0, 0);
}
RX_FUNCTION(0x0057d7b0, rx_target_practice_clear)

/* 0057d6d0 (ESI = A; stdcall group name; RET 4): the targets: the current world's (0075c530) scene entities of type 0x106e
 * (MXTargetPracticeCollectable) in the group, up to 0x20 (007d2820), each shown (+0xa2 = 1, enabled +0x70 = 1) and not hit (+0x140 = 0);
 * the hits 0 */
void rx_target_practice_collect(rx_addr A, rx_addr name) {
    ACT_FRAME(L, 0, 1);   /* push edi */
    rx_addr world;
    ACT_CALLR(world, 0, rx_game_current_world());
    const rx_addr scene = U32(world, 0x20138);
    rx_target_practice_clear(A);
    int32_t n;
    ACT_CALLR(n, 6, (int32_t)rx_scene_group_query(scene, 0x106e, A + 0xff3b4, 0x20, name, 0));
    SU32(A, 0xff3b0, (uint32_t)n);
    SU32(A, 0xff434, 0);
    for (int32_t i = 0; i < I32(A, 0xff3b0); i++) {
        const rx_addr t = U32(A, 0xff3b4 + 4u * (uint32_t)i);
        SU32(t, 0x140, 0);
        SU8(t, 0xa2, 1);
        SU32(t, 0x70, 1);
    }
    ACT_END(L);
}
RX_FUNCTION(0x0057d6d0, rx_target_practice_collect)

/* 0057d590 MXActivityTargetPracticeChallenge::vf37 (ECX = A; old state; RET 4): Activity::vf37, then the targets of the activity's
 * group (+0x118) */
void rx_target_practice_vf37(rx_addr A, uint32_t state) {
    (void)state;
    ACT_FRAME(L, 0, 1);   /* push esi */
    ACT_CALL(1, rx_activity_vf37(A));
    ACT_CALL(1, rx_target_practice_collect(A, A + 0x118));
    ACT_END(L);
}
RX_FUNCTION(0x0057d590, rx_target_practice_vf37)

/* 0057d540 MXActivityTargetPracticeChallenge::vf42 (ECX = A): in the modes 1 and outside 0..3: the clock at the duration ends it
 * (state 3), and so do all targets hit */
void rx_target_practice_vf42(rx_addr A) {
    ACT_FRAME(L, 0, 1);   /* push esi */
    const uint32_t mode = U32(A, 0x238);
    if (mode == 0 || ((int32_t)mode > 1 && (int32_t)mode <= 3)) { ACT_END(L); return; }
    if (F32(A, 0x294) >= F32(A, 0x2bc)) ACT_CALL(2, rx_activity_set_state(A, 3));
    if (I32(A, 0xff434) >= I32(A, 0xff3b0)) ACT_CALL(2, rx_activity_set_state(A, 3));
    ACT_END(L);
}
RX_FUNCTION(0x0057d540, rx_target_practice_vf42)

/* 0058a8e0 (ESI = channel, ECX = hits, EDX = total; stdcall player, &position; RET 8 -- the position is the caller's local):
 * "TargetPracticeFound" with the payload {player, hits, total, &position} (0x10 bytes). Frame: sub esp, 0x18 (cookie +0x10); push edi. */
void rx_target_hit_event(rx_addr channel, uint32_t hits, uint32_t total, rx_addr player, rx_addr pos) {
    const rx_addr entry = rx_sp();
    const rx_addr S = entry - 0x18;
    SU32(S, 0x10, U32(0, 0x009b45e4) ^ S);
    rx_set_sp(S - 4);   /* push edi */
    SU32(S, 0, player);
    SU32(S, 4, hits);
    SU32(S, 8, total);
    SU32(S, 0xc, pos);
    act_raise(channel, STR_TARGET_FOUND, TD_TARGET_FOUND, S, 0x10);
    rx_set_sp(entry);
}
RX_FUNCTION(0x0058a8e0, rx_target_hit_event)

/* 0057d880 (EDI = A; stdcall player; RET 4): with a vehicle, each target not hit yet (+0x140) is hit when the vehicle's position
 * (vf17 +0x44) is within the collision radius (squared [0x009ca324]) of the target's (vf17), or the rider's head (the vehicle position
 * raised by [0x009c5244]) within the head radius (squared [0x009ca328]): +0x140 = 1, hidden (+0xa2 = 0, +0x70 = 0), the hits + 1 and
 * "TargetPracticeFound" (0058a8e0) with the target's position. Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x28; push ebx /
 * esi. Locals: the target's position +0x10, the vehicle's +0x20, the cursor +0xc. */
void rx_target_practice_hits(rx_addr A, rx_addr player) {
    ACT_FRAME_EBP(L, 16, 0x28, 2);
    if (U32(player, 0x1ac) == 0) { ACT_END(L); return; }
    {
        const uint32_t out = L + 0x20;
        (void)act_vcall(U32(player, 0x1ac), 0x44, 1, &out);
    }
    if (I32(A, 0xff3b0) <= 0) { ACT_END(L); return; }
    SU32(L, 0xc, A + 0xff3b4);
    for (int32_t i = 0; i < I32(A, 0xff3b0); i++, SU32(L, 0xc, U32(L, 0xc) + 4)) {
        const rx_addr t = U32(U32(L, 0xc), 0);
        if (U32(t, 0x140) != 0) continue;
        {
            const uint32_t out = L + 0x10;
            (void)act_vcall(t, 0x44, 1, &out);
        }
        const float vx = F32(L, 0x20), vy = F32(L, 0x24), vz = F32(L, 0x28);
        const float tx = F32(L, 0x10), ty = F32(L, 0x14), tz = F32(L, 0x18);
        const float dz = vz - tz;
        const float dx = vx - tx;
        const float dy = vy - ty;
        const float head_y = (F32(0, 0x009c5244) + vy) - ty;
        float d2 = dy * dy;
        d2 = d2 + dz * dz;
        d2 = d2 + dx * dx;
        int hit = F32(0, 0x009ca324) > d2;
        if (!hit) {
            float h2 = dz * dz;
            h2 = h2 + head_y * head_y;
            h2 = h2 + dx * dx;
            hit = F32(0, 0x009ca328) > h2;
        }
        if (!hit) continue;
        SU32(t, 0x140, 1);
        SU8(t, 0xa2, 0);
        SU32(t, 0x70, 0);
        SU32(A, 0xff434, U32(A, 0xff434) + 1);
        ACT_CALL(2, rx_target_hit_event(U32(A, 0x290), U32(A, 0xff434), U32(A, 0xff3b0), player, L + 0x10));
    }
    ACT_END(L);
}
RX_FUNCTION(0x0057d880, rx_target_practice_hits)

/* 0057d780 MXActivityTargetPracticeChallenge::vf28 (ECX = A; dt; RET 4): Activity::vf28, then the first player's hits (0057d880) */
void rx_target_practice_vf28(rx_addr A, float dt) {
    ACT_FRAME(L, 0, 1);   /* push edi */
    ACT_CALL(1, rx_activity_in_progress(A, fcopy_f(dt)));
    ACT_CALL(1, rx_target_practice_hits(A, FIRST_PLAYER(A)));
    ACT_END(L);
}
RX_FUNCTION(0x0057d780, rx_target_practice_vf28)

/* 0057d6a0 MXActivityTargetPracticeChallenge::vf47 (ECX = A; driver; RET 4): the boundary rule and the free ride's rule (00587690),
 * each bound to the driver */
void rx_target_practice_vf47(rx_addr A, rx_addr d) {
    (void)A;
    ACT_FRAME(L, 0, 1);   /* push esi */
    rx_addr rule;
    ACT_CALLR(rule, 0, rx_challenge_rule_get(d));
    (void)act_vcall(rule, 4, 1, &d);
    ACT_CALLR(rule, 0, rx_waypoint_get_free_ride_rule(d));
    (void)act_vcall(rule, 4, 1, &d);
    ACT_END(L);
}
RX_FUNCTION(0x0057d6a0, rx_target_practice_vf47)

/* 0055ed10 MXActivityTargetPracticeChallenge::vf62: the hits (fild) */
float rx_target_practice_value(rx_addr A) { return (float)I32(A, 0xff434); }
RX_FUNCTION(0x0055ed10, rx_target_practice_value)

/* 0057d9e0 MXActivityTargetPracticeChallenge::vf64 (ECX = A; three medal flags; RET 0xc): each flag set (never cleared) when the hits
 * reach the medal's criterion */
void rx_target_practice_vf64(rx_addr A, rx_addr m0, rx_addr m1, rx_addr m2) {
    const float c2 = medal_criterion(A, 2);
    const float c1 = medal_criterion(A, 1);
    const float c0 = medal_criterion(A, 0);
    if ((float)I32(A, 0xff434) >= c2) SU32(m2, 0, 1);
    if ((float)I32(A, 0xff434) >= c1) SU32(m1, 0, 1);
    if ((float)I32(A, 0xff434) >= c0) SU32(m0, 0, 1);
}
RX_FUNCTION(0x0057d9e0, rx_target_practice_vf64)

/* ======================================================================================================================== */
/* ChallengeDriver (vtable 0x009400a4: a NetworkDriver) and RuleChallengeBoundary (vtable 0x00940188)                            */

/* 005188c0 ChallengeDriver::sdtor: the vtable, NetworkDriver's destructor (0050e2f0), freed with flag 1 */
rx_addr rx_challenge_driver_sdtor(rx_addr d, uint32_t flags) {
    ACT_FRAME(L, 0, 1);   /* push esi */
    SU32(d, 0, VT_CHALLENGE_DRIVER);
    ACT_CALL(0, rx_network_driver_dtor(d));
    if (flags & 1) ACT_CALL(1, rx_reflex_aligned_free(d));
    ACT_END(L);
    return d;
}
RX_FUNCTION(0x005188c0, rx_challenge_driver_sdtor)

/* 00516020 (vf1 of ChallengeDriver and CaptureDriver): a jump to NetworkDriver::vf1 */
void rx_challenge_driver_vf1(rx_addr d) { rx_network_driver_vf1(d); }
RX_FUNCTION(0x00516020, rx_challenge_driver_vf1)

/* 005188f0 NetworkDriver::vf2 (ChallengeDriver's vf2): a jump to BaseDriver::vf2 */
void rx_network_driver_vf2(rx_addr d) { rx_base_driver_vf2(d); }
RX_FUNCTION(0x005188f0, rx_network_driver_vf2)

/* 0050eb50 NetworkDriver::vf16 (ECX = d; kind, ref; RET 8; ChallengeDriver's and FreeRideDriver's): for kind 1 on the driver's own
 * network object (+0xef0 == ref): a valid reference (00884950) binds the player's snapshot slot (00508b90 with the object's +0x1d4);
 * the driver's vf17; then in a spectator activity (+0xfe8) for a driving player (+0x908 clear) with a records slot (+0x1f0 > -1) the
 * control goes to the first other driver whose player has no +0x1e4 (the activity's vf45 (d, that driver)), else the session
 * manager's block (+0x10) is told +0x1bc = 1, +0x1b8 = 3. Frame: push esi (push ebp / edi for the spectator part). */
void rx_network_driver_vf16(rx_addr d, uint32_t kind, rx_addr ref) {
    if (kind != 1) return;
    ACT_FRAME(L, 0, 1);
    if (U32(d, 0xef0) != ref) { ACT_END(L); return; }
    uint32_t valid;
    ACT_CALLR(valid, 0, rx_net_ref_valid(ref));
    if ((valid & 0xff) != 0) ACT_CALL(1, (void)rx_net_snapshot_bind(U32(d, 0x74), U32(U32(d, 0xef0), 0x1d4)));
    vcall0(d, 0x44);
    const rx_addr p = U32(d, 0x74);
    const rx_addr act = U32(p, 0x94);
    if (U32(act, 0xfe8) == 0 || U32(p, 0x908) != 0 || I32(p, 0x1f0) <= -1) { ACT_END(L); return; }
    ACT_FRAME(L2, 0, 2);   /* push ebp; push edi */
    const int32_t n = I32(act, 0x26c);
    for (int32_t i = 0; i < n; i++) {
        const rx_addr other = U32(U32(act, 0x268), 4u * (uint32_t)i);
        if (other == d) continue;
        if (U32(U32(other, 0x74), 0x1e4) == 0) {
            const uint32_t a[2] = {d, other};
            (void)act_vcall(U32(U32(d, 0x74), 0x94), 0xb4, 2, a);
            ACT_END(L2);
            ACT_END(L);
            return;
        }
    }
    rx_addr mgr;
    ACT_CALLR(mgr, 0, rx_fx_session_manager());
    const rx_addr blk = U32(mgr, 0x10);
    SU32(blk, 0x1bc, 1);
    SU32(blk, 0x1b8, 3);
    ACT_END(L2);
    ACT_END(L);
}
RX_FUNCTION(0x0050eb50, rx_network_driver_vf16)

/* 00518900 ChallengeDriver::vf3 (reset; ECX = d): NetworkDriver::vf3; a local player (+4) is placed at the activity's start (+0x340, the
 * direction +0x350; 00460020 with the player's vehicle); the respawn pose saved (00550e10); +0x3c0 = 0; the behaviours' and rules' vf6
 * (005092b0, 005092f0). Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x28; push esi / edi; the position +0x20, the direction
 * +0x10. */
void rx_challenge_driver_vf3(rx_addr d) {
    ACT_FRAME_EBP(L, 16, 0x28, 2);
    ACT_CALL(0, rx_network_driver_vf3(d));
    const rx_addr p = U32(d, 0x74);
    if (U32(p, 4) != 0) {
        const rx_addr a = U32(p, 0x94);
        SU32(L, 0x20, U32(a, 0x340)); SU32(L, 0x24, U32(a, 0x344)); SU32(L, 0x28, U32(a, 0x348));
        SU32(L, 0x10, U32(a, 0x350)); SU32(L, 0x14, U32(a, 0x354)); SU32(L, 0x18, U32(a, 0x358));
        ACT_CALL(2, rx_vehicle_effect_query(U32(p, 0x1ac), L + 0x20, L + 0x10));
    }
    ACT_CALL(1, rx_player_save_respawn_pose(U32(d, 0x74)));
    SF32(d, 0x3c0, 0.0f);
    ACT_CALL(1, rx_driver_behaviors_vf6(d));
    ACT_CALL(1, rx_driver_rules_vf6(d));
    ACT_END(L);
}
RX_FUNCTION(0x00518900, rx_challenge_driver_vf3)

/* 0050bee0 (vf4 of ChallengeDriver and FreeRideDriver; ECX = d): with a player its controls reset (006c9100, player + 0xb58) and the
 * finish reset (0054e280); the vehicle's simulation pose override ended (00434ef0, EDI = simulation); +0x3c0 = 0 */
void rx_challenge_driver_vf4(rx_addr d) {
    ACT_FRAME(L, 0, 2);   /* push ebx; push edi */
    if (U32(d, 0x74) != 0) {
        const rx_addr sp = rx_sp();
        rx_set_sp(sp - 4);   /* push esi */
        ACT_CALL(0, rx_controls_reset(U32(d, 0x74) + 0xb58));
        ACT_CALL(0, rx_player_finish_reset(U32(d, 0x74)));
        rx_set_sp(sp);
    }
    ACT_CALL(0, bike_end_pose_override(U32(U32(U32(d, 0x74), 0x1ac), 0x234)));
    SF32(d, 0x3c0, 0.0f);
    ACT_END(L);
}
RX_FUNCTION(0x0050bee0, rx_challenge_driver_vf4)

/* 00516190 (stdcall driver; RET 4): each rule's vf5 (+0x14) with dt 0.0 (the rules +0xa0, count +0xee4) */
void rx_driver_rules_tick(rx_addr d) {
    ACT_FRAME(L, 0, 3);   /* push ebx; push esi; push edi */
    for (uint32_t i = 0; i < U32(d, 0xee4); i++) act_vcall_f(U32(d, 0xa0 + 4 * i), 0x14, 0.0f);
    ACT_END(L);
}
RX_FUNCTION(0x00516190, rx_driver_rules_tick)

/* 005189c0 ChallengeDriver::vf7 (update; ECX = d; dt; RET 4): a local player's driver (+0x10 == 0) ticks its rules (00516190), then
 * NetworkDriver::vf7 */
void rx_challenge_driver_vf7(rx_addr d, float dt) {
    ACT_FRAME(L, 0, 1);   /* push esi */
    if (U32(U32(d, 0x74), 0x10) == 0) ACT_CALL(1, rx_driver_rules_tick(d));
    ACT_CALL(1, rx_network_driver_update(d, fcopy_f(dt)));
    ACT_END(L);
}
RX_FUNCTION(0x005189c0, rx_challenge_driver_vf7)

/* 0051daa0 RuleChallengeBoundary::vf1 (ECX = rule; driver; RET 4) -> 1: the driver (+4), the activity's challenge track block (+0xc:
 * activity + 0xff080), active (+8) when the block is loaded (+0x320), the state +0x18 = 0 */
uint32_t rx_challenge_boundary_vf1(rx_addr rule, rx_addr d) {
    SU32(rule, 4, d);
    const rx_addr block = U32(U32(d, 0x74), 0x94) + 0xff080;
    SU32(rule, 0xc, block);
    SU32(rule, 0x18, 0);
    SU32(rule, 8, U32(block, 0x320) != 0 ? 1 : 0);
    return 1;
}
RX_FUNCTION(0x0051daa0, rx_challenge_boundary_vf1)

/* 0051dae0 (EAX = rule, ECX = position) -> 1 when the position is off the challenge track's side: the nearest node of the
 * block (00538440); the segment to the next node (+0x64, or the first node of the next section) or, behind the node (the projection
 * parameter < 0), from the previous node (+0x60, or the last node of the previous section); the 2-D cross product of the segment and
 * the position > 0. Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x18; push esi / edi; the result {section, node, d2} +0x14. */
uint32_t rx_challenge_boundary_outside(rx_addr rule, rx_addr pos) {
    ACT_FRAME_EBP(L, 16, 0x18, 2);
    const rx_addr out = L + 0x14;
    {   /* cdecl (track, 0, 0) with EDX = out, ESI = position */
        const rx_addr sp = rx_sp();
        SU32(sp, -4, 0); SU32(sp, -8, 0); SU32(sp, -12, U32(rule, 0xc));
        rx_set_sp(sp - 12);
        ACT_CALL(0, rx_track_nearest_node_all(U32(rule, 0xc), 0, 0, pos, out));
        rx_set_sp(sp);
    }
    const rx_addr node = U32(out, 4);
    if (node == 0) { ACT_END(L); return 0; }
    const rx_addr section = U32(out, 0);
    rx_addr next = U32(node, 0x64);
    if (next == 0) next = U32(U32(U32(U32(section, 8), 0), 0), 0x64);
    const float nx = F32(node, 0), nz = F32(node, 8);
    const float px = F32(pos, 0), pz = F32(pos, 8);
    const float sz = F32(next, 8) - nz;
    const float sx = F32(next, 0) - nx;
    const float t = (sz * (pz - nz) + (px - nx) * sx) / (sz * sz + sx * sx);
    /* the segment a -> b: node -> next, or behind the node previous -> node */
    rx_addr a = node, b = next;
    if (0.0f > t) {
        a = U32(node, 0x60);
        b = node;
        if (a == 0) a = U32(U32(U32(U32(section, 8), 0), 4), 0x60);
    }
    const float ax = F32(a, 0), az = F32(a, 8);
    const float cross = (F32(b, 8) - az) * (px - ax) - (pz - az) * (F32(b, 0) - ax);
    ACT_END(L);
    return cross > 0.0f ? 1 : 0;
}
RX_FUNCTION(0x0051dae0, rx_challenge_boundary_outside)

/* 0051dbe0 RuleChallengeBoundary::vf5 (ECX = rule; dt; RET 4): an active rule (+8) of a player with a vehicle that is not resetting
 * (vehicle +0x280): state 0: off the track (0051dae0) -> the time it left (+0x14 = the driver's clock +0x130, +0x10 = 0), state 1;
 * state 1: back on the track -> state 0; still off for a second (+0x10 = clock - left >= 1.0) ends the challenge (state 4).
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x1c; push esi; the position +0x10. */
void rx_challenge_boundary_vf5(rx_addr rule, float dt) {
    (void)dt;
    ACT_FRAME_EBP(L, 16, 0x1c, 1);
    const rx_addr p = U32(U32(rule, 4), 0x74);
    if (U32(p, 0x1ac) == 0 || U32(rule, 8) == 0) { ACT_END(L); return; }
    {
        const uint32_t out = L + 0x10;
        (void)act_vcall(U32(p, 0x1ac), 0x44, 1, &out);
    }
    if (U32(U32(U32(U32(rule, 4), 0x74), 0x1ac), 0x280) != 0) { ACT_END(L); return; }
    const uint32_t state = U32(rule, 0x18);
    if (state == 1) {
        uint32_t off;
        ACT_CALLR(off, 0, rx_challenge_boundary_outside(rule, L + 0x10));
        if (off == 0) {
            SU32(rule, 0x18, 0);
            ACT_END(L);
            return;
        }
        const rx_addr d = U32(rule, 4);
        const float out_time = F32(d, 0x130) - F32(rule, 0x14);
        SF32(rule, 0x10, out_time);
        if (out_time >= 1.0f) ACT_CALL(2, rx_activity_set_state(U32(U32(d, 0x74), 0x94), 4));
    } else if (state == 0) {
        uint32_t off;
        ACT_CALLR(off, 0, rx_challenge_boundary_outside(rule, L + 0x10));
        if (off != 0) {
            SF32(rule, 0x10, 0.0f);
            SU32(rule, 0x14, fcopy(U32(U32(rule, 4), 0x130)));
            SU32(rule, 0x18, 1);
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x0051dbe0, rx_challenge_boundary_vf5)
