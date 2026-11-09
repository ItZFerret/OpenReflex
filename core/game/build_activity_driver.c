/* build_activity_driver.c -- construction unit 8: a player joins the activity (construction.md section 16): Activity_OnPlayerJoined
 * 0055b610 (from the world's player slot pass 00766ec0): the activity's driver (vf44: RaceDriver 0x1130 bytes: 0050ed80 / NetworkDriver
 * 0050e1b0 / BaseDriver 00508c10), the player bound to the activity (0054d9d0), its controls and stunt system reset, the AI rules and
 * behaviours (vf47 00565e00: the getters 005883e0 .. carve them from the driver's block +0x3f0 into the rule slots +0xa0 (count +0xee4)
 * and the behaviour slots +0xe8 (count +0xee0), each one's vf1 binds it to the driver), its start slot and the start pose of its
 * vehicle (the slot's quaternion from its direction), the driver lists of the activity, "PlayerJoined", the records (MXRecords_Join)
 * and the grid slot pass (0075f890). Readable C from the disassembly; the network (Quazal session) and audio parts are boundary calls
 * (oracle: the original; product: none). */
#include "career_activity.h"
#include "vehicle_audio.h"
#include "build_activity_util.h"
#include "build_activity_int.h"
#include "race.h"
#include "race_beh1.h"
#include "world_misc.h"
#include "race_activity_freestyle.h"
#include "race_poker.h"
#include "race_beh2.h"
#include <string.h>
#include <math.h>
#include "rider_cap.h"

/* the network session (Quazal; not core) and audio: oracle the original, product nothing */
static uint32_t drv_net_call(rx_addr fn, enum rx_conv conv, uint32_t ecx, int n, const uint32_t* a) {
#ifdef RX_ORACLE
    return rx_call_original(fn, conv, ecx, n, a);
#else
    (void)fn; (void)conv; (void)ecx; (void)n; (void)a; return 0;
#endif
}
static void drv_net_call_ex(rx_addr fn, const rx_regs* in, int n, const uint32_t* a) {
#ifdef RX_ORACLE
    rx_regs o; rx_call_original_ex(fn, in, &o, n, a);
#else
    (void)fn; (void)in; (void)n; (void)a;
#endif
}
static void session_static(void) {
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
}
static void series_static(void) {
    if (!(U8(0, 0x009e275c) & 1)) act_unreached(0x005b2370, "the series manager's static initialiser (MXSeriesManager_Construct)");
}

/* an event handler proxy (0x14 bytes: vtable, +4 0, +8 the signature name, +0xc the object, +0x10 the callback) appended to the
 * event's handler list when the signature names match (the inlined registrations: 006cdd50, 0053b7f0, 0053a8c0, 0053a800) */
static void drv_register(rx_addr channel, rx_addr name, rx_addr vtable, rx_addr type_desc, rx_addr obj, uint32_t cb) {
    rx_addr ev;
    ACT_CALLR(ev, 2, rx_world_registry_find(channel, name));            /* 00593220 */
    rx_addr h = rx_reflex_aligned_alloc(0x14);
    if (h) {
        SU32(h, 0xc, obj); SU32(h, 4, 0); SU32(h, 0, vtable); SU32(h, 0x10, cb);
        rx_addr sig;
        ACT_CALLR(sig, 1, crt_type_info_name(type_desc, 0x009e1a7c));
        SU32(h, 8, sig);
    }
    if (crt_strcmp(U32(ev, 4), U32(h, 8)) == 0) ACT_CALL(1, rx_event_handler_list_push_back(ev + 8, h));   /* (under +0x24: one thread) */
}

/* ======================================================================================================================== */
/* the rules and behaviours                                                                                                   */

/* the getters 005883e0 ...: the next 0x10-aligned piece of the driver's block (+0x3e4, bump) holds the object, its vtable and the
 * fields its inlined constructor sets; it is appended to the rules (+0xa0, count +0xee4) or behaviours (+0xe8, count +0xee0), at most
 * 0x12 each; 0 when full */
typedef struct { uint32_t addr, rules, size, vtable; } beh_getter;
static rx_addr beh_get(rx_addr d, const beh_getter* g) {
    const uint32_t cnt = g->rules ? 0xee4 : 0xee0, base = g->rules ? 0xa0 : 0xe8;
    if (U32(d, cnt) >= 0x12) return 0;
    rx_addr o = U32(d, 0x3e4);
    SU32(d, 0x3e4, o + g->size);
    if (o) {
        SU32(o, 0, g->vtable);
        switch (g->addr) {
        case 0x00588430: case 0x00588490: SU32(o, 8, 1); break;                      /* RuleFinishLine, RuleBackwards */
        case 0x00588380: SF32(o, 0xc, 0.0f); SU8(o, 0xd8, 0); break;                  /* RuleOfftrackTimePenalty */
        case 0x00587690: SU32(o, 0xc, 0); break;                                       /* the free ride's rule */
        case 0x005876f0: SF32(o, 0xc, 0.0f); SF32(o, 0x10, 0.0f); SU32(o, 0x18, 0); SU32(o, 0x14, 0); break;   /* BehaviorAvoidWreck */
        default: break;
        }
    }
    SU32(d, base + 4 * U32(d, cnt), o);
    const rx_addr r = U32(d, base + 4 * U32(d, cnt));
    SU32(d, cnt, U32(d, cnt) + 1);
    return r;
}
static const beh_getter G_TRACK_ADVANCE = {0x005883e0, 1, 0x20, 0x00940168}, G_FINISH_LINE = {0x00588430, 1, 0x10, 0x0093fbe4},
    G_BACKWARDS = {0x00588490, 1, 0x10, 0x0093fb4c}, G_OFFTRACK_PENALTY = {0x00588380, 1, 0xe0, 0x0093fab0},
    G_AVOID_OBJECTS = {0x00587890, 0, 0x360, 0x0093fad0}, G_AVOID_WRECK = {0x005876f0, 0, 0x20, 0x00940220},
    G_SUCK_RACING = {0x005888b0, 0, 0x30, 0x0093fcc0}, G_FIX_FORWARD = {0x005877f0, 0, 0x70, 0x0093fbbc},
    G_LAP_PACING = {0x005884f0, 0, 0x70, 0x0093f950}, G_MONITOR_CORNER = {0x00588540, 0, 0x30, 0x0093f9c8},
    G_BRAKE_TURN = {0x00588590, 0, 0x40, 0x00940050}, G_BLUE_YELLOW = {0x005885e0, 0, 0x20, 0x00940248},
    G_SEPERATE = {0x00588630, 0, 0xb0, 0x0093f9a0}, G_OFFTRACK = {0x00588680, 0, 0x40, 0x009401d0},
    G_NAVIGATE = {0x005886d0, 0, 0x50, 0x0093f8b0}, G_588720 = {0x00588720, 0, 0x70, 0x0093f928},
    G_588770 = {0x00588770, 0, 0x60, 0x009401a8}, G_SPLIT_SECTION = {0x005887c0, 0, 0x20, 0x0093fc70},
    G_588810 = {0x00588810, 0, 0x30, 0x0093f978}, G_588860 = {0x00588860, 0, 0x20, 0x0093fc98},
    G_AVOID_FLIP_BACK = {0x005878e0, 0, 0x20, 0x0093fce8}, G_587DB0 = {0x00587db0, 0, 0x20, 0x00940270},
    G_FR_RULE = {0x00587690, 1, 0x20, 0x0093fe90}, G_587750 = {0x00587750, 0, 0xb0, 0x0093f888}, G_587840 = {0x00587840, 0, 0x130, 0x0093fa68};
/* the Freestyle driver's own (MXActivityFreestyleStub::vf47): RuleInFreestyleCrowd, BehaviorGenericBrake, BehaviorRoamTrack */
static const beh_getter G_IN_CROWD = {0x00587d60, 1, 0x10, 0x0093fc04}, G_GENERIC_BRAKE = {0x00587e00, 0, 0x70, 0x009401f8},
    G_ROAM_TRACK = {0x00587e50, 0, 0xf0, 0x0093fb6c};

/* FUN_0051ff30 (ESI = node, BehaviorAirLeveling + 0x60): its slot (+0x38) of the air-leveling table 0x009e3f68 (count
 * [0x00a89d04]) released: the last one moved in */
static void air_slot_release(rx_addr o) {
    if (I32(o, 0x38) < 0) return;
    const uint32_t k = U32(o, 0x38), n = U32(0, 0x00a89d04);
    SU32(rx_rc_tab(0x009e3f68, 4, (uint32_t)k), 0, U32(rx_rc_tab(0x009e3f68, 4, n - 1), 0));
    SU32(0, 0x00a89d04, n - 1);
    SU32(U32(rx_rc_tab(0x009e3f68, 4, (uint32_t)k), 0), 0x38, k);
    SU32(o, 0x38, 0xffffffffu);
}
/* FUN_0051f330 (EDI = o): BehaviorAirLeveling's constructor (0xa0 bytes; its table node at +0x60: +0 the behaviour, +0x10 /
 * +0x20 two vectors, +0x30, +0x34, +0x38 the slot -1, +0x3c) */
static rx_addr air_leveling_construct(rx_addr o) {
    SU32(o, 0, 0x0093f9f0);
    SU32(o, 0xc, 0); SU32(o, 0x10, 0); SU32(o, 0x60, 0);
    for (uint32_t k = 0; k < 4; k++) SU32(o, 0x70 + 4 * k, U32(0, 0x00d61670 + 4 * k));   /* the runtime's (1, 1, 1, 1) */
    for (uint32_t k = 0; k < 4; k++) SU32(o, 0x80 + 4 * k, U32(0, 0x00d616a0 + 4 * k));   /* (0, 1, 0, 0) */
    SU32(o, 0x90, 0); SU8(o, 0x94, 0); SU32(o, 0x98, 0xffffffffu); SU8(o, 0x9c, 0);
    air_slot_release(o + 0x60);   /* (no slot yet) */
    SU32(o, 0x60, o);
    SU32(o, 4, 0); SU32(o, 0x4c, 0); SU32(o, 0x40, 0); SU32(o, 0x44, 0); SU32(o, 0x48, 0); SU32(o, 0x50, 0); SU32(o, 0x54, 0);
    SU32(o, 0x58, 0);
    return o;
}
/* 005877a0 (ESI = driver): BehaviorAirLeveling */
static rx_addr beh_get_air_leveling(rx_addr d) {
    if (U32(d, 0xee0) >= 0x12) return 0;
    rx_addr o = U32(d, 0x3e4);
    SU32(d, 0x3e4, o + 0xa0);
    const rx_addr r0 = o ? air_leveling_construct(o) : 0;
    SU32(d, 0xe8 + 4 * U32(d, 0xee0), r0);
    const rx_addr r = U32(d, 0xe8 + 4 * U32(d, 0xee0));
    SU32(d, 0xee0, U32(d, 0xee0) + 1);
    return r;
}

/* the vf1s (bind to the driver) and their resets (vf6) */
void rx_rule_track_advance_vf1(rx_addr o, rx_addr d) { SU32(o, 4, d); SU32(o, 0x10, U32(d, 0x14)); SU32(o, 0xc, 0x43e10000u); SU32(o, 8, 1); }   /* 0051d3d0 */
RX_FUNCTION(0x0051d3d0, rx_rule_track_advance_vf1)
void rx_rule_finish_line_vf1(rx_addr o, rx_addr d) { SU32(o, 4, d); SU32(o, 8, 1); }                              /* 0051a770 */
RX_FUNCTION(0x0051a770, rx_rule_finish_line_vf1)
void rx_rule_backwards_vf1(rx_addr o, rx_addr d) { SU32(o, 4, d); SU32(o, 0xc, 0); SU32(o, 8, 1); }               /* 00519950 */
RX_FUNCTION(0x00519950, rx_rule_backwards_vf1)
void rx_rule_offtrack_penalty_vf1(rx_addr o, rx_addr d) {                                                          /* 0051af90 */
    SU32(o, 4, d); SU32(o, 8, 1); SU32(o, 0x10, 1);
    static const uint16_t z[] = {0x14, 0x1c, 0x20, 0x54, 0xd0, 0xc, 0x28, 0x30, 0x50, 0x40, 0x44, 0x48, 0x80, 0x84, 0x88};
    for (unsigned k = 0; k < sizeof z / sizeof z[0]; k++) SU32(o, z[k], 0);
    SU8(o, 0xd8, 0); SU32(o, 0xd4, 0);
}
RX_FUNCTION(0x0051af90, rx_rule_offtrack_penalty_vf1)
uint32_t rx_behavior_bind_vf1(rx_addr o, rx_addr d) { SU32(o, 8, d); SU32(o, 4, 1); return o; }                   /* 00524880 (shared) */
RX_FUNCTION(0x00524880, rx_behavior_bind_vf1)
void rx_fix_forward_vf6(rx_addr o) {                                                                                /* 005234a0 */
    SU32(o, 0x18, 0); SU32(o, 0x54, 0); SU32(o, 0x58, 0); SU32(o, 0x20, 0); SU32(U32(o, 8), 0x3cc, 0);
    SU32(o, 0x14, 0); SU32(o, 0x50, 0); SU32(o, 0x1c, 0); SU32(U32(o, 8), 0x3cc, 0);
    SU32(o, 0x60, rx_behavior_fix_progress_race_type(o));   /* 00524050 (no stack arguments) */
}
RX_FUNCTION(0x005234a0, rx_fix_forward_vf6)
/* BehaviorFixForwardProgress::vf1 005233e0: "VehicleWrecked" on the activity's channel (+0x290 of the activity: 0053b7f0), the
 * stuck times 1.5 / 30 (5 / 15 for an AI with +0x32c == 2), vf6 */
uint32_t rx_fix_forward_vf1(rx_addr o, rx_addr d) {
    SU32(o, 8, d); SU32(o, 4, 1);
    ACT_CALL(1, drv_register(U32(U32(U32(d, 0x74), 0x94), 0x290), 0x0093ee38 /* "VehicleWrecked" */, 0x00940430, 0x009cdb7c, o, 0x005234f0));
    SU32(o, 0xc, 0x3fc00000u); SU32(o, 0x10, 0x41f00000u);
    if (U32(U32(o, 8), 0x32c) == 2) { SU32(o, 0xc, 0x40a00000u); SU32(o, 0x10, 0x41700000u); }
    act_vcall(o, 0x18, 0, 0);
    return 1;
}
RX_FUNCTION(0x005233e0, rx_fix_forward_vf1)
/* BehaviorLapPacing::vf1 00525450: the pacing kind of the activity type (0051f100: 0 -> 1, 3 -> 2, else 0) */
uint32_t rx_lap_pacing_vf1(rx_addr o, rx_addr d) {
    ACT_FRAME(L, 0, 1);   /* push esi */
    SU32(o, 8, d); SU32(o, 4, 1);
    uint32_t t = U32(U32(U32(d, 0x74), 0x94), 0x23c);
    if (t >= 0x1f) { series_static(); ACT_CALLR(t, 1, rx_career_current_event_type(0x009e2188)); }   /* a championship type: the series manager's base type (005b6a30) */
    SU32(o, 0x38, t == 0 ? 1 : t == 3 ? 2 : 0);
    SU32(o, 0x2c, 0);
    ACT_END(L);
    return 1;
}
RX_FUNCTION(0x00525450, rx_lap_pacing_vf1)
void rx_monitor_corner_vf6(rx_addr o) { for (uint32_t k = 0xc; k <= 0x20; k += 4) SU32(o, k, 0); }                 /* 00526950 */
RX_FUNCTION(0x00526950, rx_monitor_corner_vf6)
uint32_t rx_monitor_corner_vf1(rx_addr o, rx_addr d) { SU32(o, 8, d); SU32(o, 4, 1); act_vcall(o, 0x18, 0, 0); return 1; }   /* 00526930 */
RX_FUNCTION(0x00526930, rx_monitor_corner_vf1)
void rx_brake_turn_vf6(rx_addr o) {                                                                                 /* 00522e70 */
    SU32(o, 0x14, 0); SU32(o, 4, 0); SU32(o, 0x10, 0); SU32(o, 0x18, 0); SU32(o, 0x30, 0); SU32(o, 0x34, 0); SU32(o, 0x38, 0);
    SU32(o, 0x20, 0); SU32(o, 0x24, 0x3f800000u);
}
RX_FUNCTION(0x00522e70, rx_brake_turn_vf6)
uint32_t rx_brake_turn_vf1(rx_addr o, rx_addr d) { SU32(o, 8, d); SU32(o, 4, 0); act_vcall(o, 0x18, 0, 0); return 1; }   /* 00522e50 */
RX_FUNCTION(0x00522e50, rx_brake_turn_vf1)
uint32_t rx_blue_yellow_vf1(rx_addr o, rx_addr d) { SU32(o, 8, d); SU32(o, 0xc, 0); SU32(o, 0x14, 0); SU32(o, 0x10, 0); SU32(o, 4, 1); return 1; }   /* 00522580 */
RX_FUNCTION(0x00522580, rx_blue_yellow_vf1)
uint32_t rx_seperate_vf1(rx_addr o, rx_addr d) {                                                                    /* 005318a0 */
    SU32(o, 8, d); SU32(o, 4, 1); SU32(o, 0x84, 0); SU32(o, 0x70, 0x40e00000u); SU32(o, 0x74, 0x40a00000u);
    if (U32(d, 0x32c) == 2) SU32(o, 0x74, 0x40e00000u);
    return 1;
}
RX_FUNCTION(0x005318a0, rx_seperate_vf1)
void rx_air_leveling_vf6(rx_addr o) { for (uint32_t k = 0x40; k <= 0x58; k += 4) SU32(o, k, 0); }                 /* 0051f420 */
RX_FUNCTION(0x0051f420, rx_air_leveling_vf6)
/* BehaviorAirLeveling::vf7 0051f450 (on): +4 = on; the slot of the air-leveling table taken (0051fee0: at most 0xc) or released;
 * vf6 */
void rx_air_leveling_vf7(rx_addr o, uint32_t on) {
    SU32(o, 4, on);
    const rx_addr n = o + 0x60;
    if (on) {
        if (I32(n, 0x38) < 0 && U32(0, 0x00a89d04) < RX_RC_MAX) {   /* (w-rc-core) 0xc: slots 12..31 in side storage */
            SU32(n, 0x38, U32(0, 0x00a89d04));
            SU32(rx_rc_tab(0x009e3f68, 4, (uint32_t)U32(0, 0x00a89d04)), 0, n);
            SU32(0, 0x00a89d04, U32(0, 0x00a89d04) + 1);
        }
    } else air_slot_release(n);
    act_vcall(o, 0x18, 0, 0);
}
RX_FUNCTION(0x0051f450, rx_air_leveling_vf7)
uint32_t rx_air_leveling_vf1(rx_addr o, rx_addr d) { SU32(o, 8, d); const uint32_t a = 1; act_vcall(o, 0x1c, 1, &a); return 1; }   /* 0051f3e0 */
RX_FUNCTION(0x0051f3e0, rx_air_leveling_vf1)
void rx_offtrack_vf6(rx_addr o) { SU32(o, 0x30, 0); SU32(o, 0x10, 0); SU32(o, 0x14, 0); SU32(o, 0x18, 0); }           /* 00529130 */
RX_FUNCTION(0x00529130, rx_offtrack_vf6)
uint32_t rx_offtrack_vf1(rx_addr o, rx_addr d) {                                                                    /* 005290d0 */
    rx_addr r;
    ACT_CALLR(r, 1, rx_driver_rule_1(d));
    SU32(o, 0x1c, r);
    ACT_CALLR(r, 1, rx_driver_offtrack_penalty_rule(d));
    SU32(o, 0x20, r);
    SU32(o, 8, d); SU32(o, 4, 0);
    act_vcall(o, 0x18, 0, 0);
    return 1;
}
RX_FUNCTION(0x005290d0, rx_offtrack_vf1)
uint32_t rx_split_section_vf1(rx_addr o, rx_addr d) { SU32(o, 8, d); SU32(o, 4, 0); return 1; }                      /* 00534420 */
RX_FUNCTION(0x00534420, rx_split_section_vf1)

static void beh_bind(rx_addr o, rx_addr d) { act_vcall(o, 4, 1, &d); }   /* vf1 (driver) */
static void beh_add(rx_addr d, const beh_getter* g) { beh_bind(beh_get(d, g), d); }

/* MXActivityRaceStub::vf47 00565e00 (ECX = activity; driver; RET 4): the rules and behaviours of a racing (mode != 0 / 2) or a
 * practice / free activity (the offtrack penalty first; its AI riders also suck-racing / fix-forward ...; mode 0 with [0x009cb860]
 * behaviour 00587db0 when 0078fcf0), BehaviorAvoidObjects for Opencross (type 2), a network-remote driver's (+0x74 +0x10 == 1) track
 * advance rule active */
void rx_race_stub_vf47(rx_addr A, rx_addr d) {
    ACT_FRAME(L, 0, 2);
    const uint32_t mode = U32(A, 0x238);
    if (mode != 0 && mode != 2) {
        beh_add(d, &G_TRACK_ADVANCE); beh_add(d, &G_FINISH_LINE); beh_add(d, &G_BACKWARDS); beh_add(d, &G_OFFTRACK_PENALTY);
        if (U32(A, 0x23c) == 2) beh_add(d, &G_AVOID_OBJECTS);
        beh_add(d, &G_AVOID_WRECK); beh_add(d, &G_SUCK_RACING); beh_add(d, &G_FIX_FORWARD); beh_add(d, &G_LAP_PACING);
        beh_add(d, &G_MONITOR_CORNER); beh_add(d, &G_BRAKE_TURN); beh_add(d, &G_BLUE_YELLOW); beh_add(d, &G_SEPERATE);
        beh_bind(beh_get_air_leveling(d), d);
        beh_add(d, &G_OFFTRACK); beh_add(d, &G_NAVIGATE); beh_add(d, &G_588720); beh_add(d, &G_588770); beh_add(d, &G_SPLIT_SECTION);
        beh_add(d, &G_588810); beh_add(d, &G_588860); beh_add(d, &G_AVOID_FLIP_BACK);
        ACT_END(L);
        return;
    }
    beh_add(d, &G_OFFTRACK_PENALTY); beh_add(d, &G_TRACK_ADVANCE); beh_add(d, &G_FINISH_LINE); beh_add(d, &G_BACKWARDS);
    /* 0078fcf0 (EAX = the level name +0x20030 of the world, EDI = "WORLD"): a case-insensitive strstr */
    if (mode == 0 && rx_str_find_nocase(U32(A, 0x234) + 0x20030, 0x0096dd28 /* "WORLD" */) != 0) beh_add(d, &G_587DB0);
    if (U32(A, 0x23c) == 2) beh_add(d, &G_AVOID_OBJECTS);
    beh_add(d, &G_AVOID_WRECK); beh_add(d, &G_FIX_FORWARD); beh_add(d, &G_LAP_PACING); beh_add(d, &G_MONITOR_CORNER);
    beh_add(d, &G_BRAKE_TURN); beh_add(d, &G_BLUE_YELLOW); beh_add(d, &G_SEPERATE);
    beh_bind(beh_get_air_leveling(d), d);
    beh_add(d, &G_OFFTRACK); beh_add(d, &G_NAVIGATE); beh_add(d, &G_588720); beh_add(d, &G_588770); beh_add(d, &G_SPLIT_SECTION);
    beh_add(d, &G_588810); beh_add(d, &G_588860); beh_add(d, &G_AVOID_FLIP_BACK);
    if (U32(U32(d, 0x74), 0x10) == 1) {
        rx_addr r;
        ACT_CALLR(r, 1, rx_driver_rule_1(d));
        SU32(r, 0x58, 1);
    }
    ACT_END(L);
}
RX_FUNCTION(0x00565e00, rx_race_stub_vf47)

/* MXActivityFreestyleStub::vf47 00563740 (ECX = activity; driver; RET 4): the Freestyle driver's rule and behaviours, in this order:
 * RuleInFreestyleCrowd, RuleCannonball (00587690), in mode 0 on a "WORLD" level (0078fcf0) BehaviorLeaveActivity, BehaviorAvoidWreck,
 * BehaviorPerformStunt (00587750), BehaviorFixForwardProgress, BehaviorAirLeveling, BehaviorGenericBrake, BehaviorRoamTrack,
 * BehaviorAvoidFlipBack (each carved from the driver's block and bound with its vf1). Frame: push esi / edi. */
void rx_freestyle_stub_vf47(rx_addr A, rx_addr d) {
    ACT_FRAME(L, 0, 2);
    beh_add(d, &G_IN_CROWD);
    beh_add(d, &G_FR_RULE);
    if (U32(A, 0x238) == 0 && rx_str_find_nocase(U32(A, 0x234) + 0x20030, 0x0096dd28 /* "WORLD" */) != 0) beh_add(d, &G_587DB0);
    beh_add(d, &G_AVOID_WRECK);
    beh_add(d, &G_587750);
    beh_add(d, &G_FIX_FORWARD);
    beh_bind(beh_get_air_leveling(d), d);
    beh_add(d, &G_GENERIC_BRAKE);
    beh_add(d, &G_ROAM_TRACK);
    beh_add(d, &G_AVOID_FLIP_BACK);
    ACT_END(L);
}
RX_FUNCTION(0x00563740, rx_freestyle_stub_vf47)

/* the PokerRace's RuleOfftrack (00589b30: 0x70 bytes, vtable 0x0093fa90; race_poker.c) */
static const beh_getter G_OFFTRACK_RULE = {0x00589b30, 1, 0x70, 0x0093fa90};
static rx_addr get_offtrack_rule(rx_addr d) { return beh_get(d, &G_OFFTRACK_RULE); }
RX_FUNCTION(0x00589b30, get_offtrack_rule)

/* MXActivityPokerRace::vf47 005714b0 (ECX = activity; driver; RET 4): its drivers' rules only, in this order: RuleOfftrack,
 * RuleTrackAdvance, RuleFinishLine, RuleBackwards (each carved from the driver's block and bound with its vf1; no behaviours: the
 * poker race has no AI riders' driving). Frame: push esi. */
void rx_poker_race_vf47(rx_addr A, rx_addr d) {
    (void)A;
    ACT_FRAME(L, 0, 1);
    beh_add(d, &G_OFFTRACK_RULE);
    beh_add(d, &G_TRACK_ADVANCE);
    beh_add(d, &G_FINISH_LINE);
    beh_add(d, &G_BACKWARDS);
    ACT_END(L);
}
RX_FUNCTION(0x005714b0, rx_poker_race_vf47)

/* ======================================================================================================================== */
/* the drivers                                                                                                                */

/* FUN_00508c10 (ESI = driver, EAX = player): BaseDriver (vtable 0x0093f7ac): the 8 speed ranges (+0x334 .. +0x3bc: lo 0, 0.1,
 * 1, FLT_MAX, -FLT_MAX), the rule / behaviour state, the block +0x3f0 (0xaf0 bytes) and its bump pointer +0x3e4 */
rx_addr rx_base_driver_construct(rx_addr d, rx_addr player) {
    SU32(d, 0, 0x0093f7ac);
    for (uint32_t k = 0; k < 8; k++) {
        const rx_addr r = d + 0x334 + 0x14 * k;
        SF32(r, 0, 0.0f); SU32(r, 4, 0x3dcccccdu); SU32(r, 0xc, 0x7f7fffffu); SU32(r, 0x10, 0xff7fffffu); SU32(r, 8, 0x3f800000u);
    }
    for (uint32_t o = 0x3c0; o <= 0x3e0; o += 4) if (o != 0x3cc && o != 0x3d0) SF32(d, o, 0.0f);
    SU32(d, 0x3d0, 0);
    SU32(d, 0x74, player);
    SU32(d, 0x14, 0); SU32(d, 0x18, 0); SU32(d, 0x1c, 0); SU32(d, 0x24, 0); SU32(d, 0x20, 0);
    SF32(d, 0x60, 0.0f); SF32(d, 0x64, 0.0f); SF32(d, 0x68, 0.0f); SF32(d, 0x130, 0.0f); SF32(d, 0x140, 0.0f); SF32(d, 0x13c, 0.0f);
    for (uint32_t o = 0x80; o < 0xa0; o += 4) SU32(d, o, 0);
    for (uint32_t o = 0x28; o <= 0x54; o += 4) SU32(d, o, 1);
    if (U32(0, 0x00ae1b90) != 0) { SU32(U32(d, 0x74), 0x8f4, 0); SU32(U32(d, 0x74), 0x8f8, 2); }
    SF32(d, 0x3c0, 0.0f); SU32(d, 0x3d0, 0);
    for (uint32_t o = 0x3c4; o <= 0x3e0; o += 4) if (o != 0x3d0) SF32(d, o, 0.0f);
    SU32(d, 0x3e4, d + 0x3f0);
    for (uint32_t i = 0; i < 0xaf0; i++) SU8(d + 0x3f0, i, 0);
    SU32(d, 0xee0, 0); SU32(d, 0xee4, 0); SU32(d, 0xeec, 1);
    return d;
}
RX_FUNCTION(0x00508c10, rx_base_driver_construct)

/* FUN_0050e1b0 (EDI = driver; stdcall player; RET 4): NetworkDriver (vtable 0x0093f81c): the base, the session's network player of
 * the player (+0xb2c -> +0x1d8: +0xef0; its duplicated object +0x1d4 bound (00508b90) when it is the local one; the rider's network
 * blend 0.25 (006237a0) for a remote one) under the session's lock (Quazal: 00883d80 / 00883d90), +0xefc = 1 */
rx_addr rx_network_driver_construct(rx_addr d, rx_addr player) {
    ACT_FRAME_EBP(L, 8, 8, 2);
    ACT_CALL(0, rx_base_driver_construct(d, player));
    SU32(d, 0, 0x0093f81c);
    session_static();
    if (U32(0, 0x00da6d08) != 0) act_unreached(0x0050e1f9, "the networked session's lock (00402420)");
    SU32(L, 0xc, 0x00da6cf0);
    if (U8(0, 0x009df580) == 0) drv_net_call(0x00883d80, RX_THISCALL, U32(L, 0xc), 0, 0);
    const rx_addr np = U32(player, 0xb2c) ? U32(U32(player, 0xb2c), 0x1d8) : 0;
    SU32(d, 0xef0, np);
    if (np && drv_net_call(0x00884950, RX_THISCALL, np, 0, 0) & 0xff) {
        rx_regs in; memset(&in, 0, sizeof in); in.r[0] = player;
        const uint32_t a = U32(U32(d, 0xef0), 0x1d4);
        drv_net_call_ex(0x00508b90, &in, 1, &a);
    }
    SU32(d, 0xef4, 0); SU32(d, 0xef8, 0);
    if (U32(d, 0xef0) && !(drv_net_call(0x00884950, RX_THISCALL, U32(d, 0xef0), 0, 0) & 0xff) && U32(U32(d, 0x74), 0x1b4)) {
        rx_regs in; memset(&in, 0, sizeof in); in.r[0] = 4; in.r[1] = U32(U32(d, 0x74), 0x1b4);
        const uint32_t a = 0x3e800000u;
        drv_net_call_ex(0x006237a0, &in, 1, &a);
    }
    SU32(d, 0xefc, 1);
    if (U8(0, 0x009df580) == 0) drv_net_call(0x00883d90, RX_THISCALL, U32(L, 0xc), 0, 0);
    ACT_END(L);
    return d;
}
RX_FUNCTION(0x0050e1b0, rx_network_driver_construct)

/* FUN_0050ed80 (ECX = driver, EAX = player) -> driver: RaceDriver (vtable 0x0093f8dc, 0x1130 bytes) */
rx_addr rx_race_driver_construct(rx_addr d, rx_addr player) {
    ACT_FRAME(L, 0, 1);
    ACT_CALL(1, rx_network_driver_construct(d, player));
    SU32(d, 0xf1c, 0); SU8(d, 0x10d4, 0); SU32(d, 0x111c, 0);
    SU32(d, 0, 0x0093f8dc);
    SU32(d, 0xee8, 3);
    SF32(d, 0xf20, 0.0f);
    SU32(d, 0x10c4, 0x7f7fffffu); SU32(d, 0x10bc, 0x7f7fffffu);
    SF32(d, 0x10b4, 0.0f); SF32(d, 0x10b8, 0.0f); SF32(d, 0x10b0, 0.0f); SF32(d, 0x10c8, 0.0f); SF32(d, 0x10d0, 0.0f);
    SF32(d, 0x10e4, 0.0f); SF32(d, 0x10e8, 0.0f); SF32(d, 0x10ec, 0.0f); SF32(d, 0x10d8, 0.0f);
    SU32(d, 0x1124, 0x7f7fffffu); SU32(d, 0x10cc, 0x7f7fffffu); SU32(d, 0x1118, 0x7f7fffffu);
    ACT_END(L);
    return d;
}
RX_FUNCTION(0x0050ed80, rx_race_driver_construct)

/* MXActivityRaceStub::vf44 00566ea0 (ECX = activity; player; RET 4) -> a new RaceDriver */
rx_addr rx_race_stub_vf44(rx_addr A, rx_addr player) {
    (void)A;
    rx_addr d = rx_reflex_aligned_alloc(0x1130);
    if (!d) return 0;
    rx_addr r;
    ACT_CALLR(r, 0, rx_race_driver_construct(d, player));
    return r;
}
RX_FUNCTION(0x00566ea0, rx_race_stub_vf44)

/* FUN_00508f50 (stdcall driver; RET 4): every behaviour's vf3 and vf6 */
void rx_driver_behaviors_reset(rx_addr d) {
    ACT_FRAME(L, 0, 3);
    for (uint32_t i = 0; i < U32(d, 0xee0); i++) {
        const rx_addr b = U32(d, 0xe8 + 4 * i);
        act_vcall(b, 0xc, 0, 0);
        act_vcall(b, 0x18, 0, 0);
    }
    ACT_END(L);
}
RX_FUNCTION(0x00508f50, rx_driver_behaviors_reset)

/* ======================================================================================================================== */
/* the player's side                                                                                                          */

/* FUN_0054e890 (EAX = player): the wreck recovery fields cleared; "WreckRecoveryReset" (activity, vehicle) raised on its activity's
 * channel (+0x290; 00554da0: ESI = channel, EDX = name, ECX = vehicle; activity) */
static void player_race_reset(rx_addr P) {
    SU32(P, 0xb30, 0); SU32(P, 0xb34, 0); SU32(P, 0xb44, 0); SF32(P, 0xb3c, 0.0f); SF32(P, 0xb38, 0.0f); SF32(P, 0xb50, 0.0f); SF32(P, 0xb54, 0.0f);
    const rx_addr A = U32(P, 0x94);
    if (A != 0) {
        const uint32_t payload[2] = {A, U32(P, 0x1ac)};
        race_raise_event(U32(A, 0x290), 0x009416e8 /* "WreckRecoveryReset" */, 0x009cdb7c, payload, 2);
    }
}

/* FUN_006cdd50 (stdcall scoring): the stunt scoring system (P + 0xc70) listens to "VehicleEntered" on the vehicle trigger channel
 * [0x00a89bc8] */
void rx_stunt_scoring_listen(rx_addr s) {
    ACT_FRAME(L, 0, 4);
    ACT_CALL(1, drv_register(U32(0, 0x00a89bc8), 0x00935e84 /* "VehicleEntered" */, 0x00960b64, 0x009cbb7c, s, 0x006cc230));
    ACT_END(L);
}
RX_FUNCTION(0x006cdd50, rx_stunt_scoring_listen)

/* FUN_0054d9d0 (ESI = player, EAX = activity): the player's activity (+0x94); its vehicle known to the vehicle audio manager
 * [0x00d92f00] (12 entries of 0x1b10; audio, not core) unless the activity is inactive (5); for a local human: the stunt scoring on
 * when the session's local player (+0xd98) is a local human too (006cdd50); the respawn / camera / flag fields */
void rx_player_join_activity(rx_addr P, rx_addr A) {
    ACT_FRAME(L, 0, 3);
    SU32(P, 0x94, A);
    if (U32(P, 0x1ac) != 0 && A != 0) {
        uint32_t t = U32(A, 0x23c);
        if (t >= 0x1f) { series_static(); ACT_CALLR(t, 1, rx_career_current_event_type(0x009e2188)); }
        if (t != 5) {
            const rx_addr m = U32(0, 0x00d92f00);
            int found = 0;
            for (uint32_t k = 0; k < 0xc; k++) if (U32(m, 0x10 + 0x1b10 * k) == U32(P, 0x1ac)) { found = 1; break; }
#ifdef RX_ORACLE
            if (!found) { const uint32_t a[2] = {m, U32(P, 0x1ac)}; rx_call_original(0x00680b10, RX_CDECL, 0, 2, a); }
#else
            if (!found) rx_vehicle_audio_register(U32(P, 0x1ac));   /* w-sfx: 00680b10's slot choice (game/vehicle_audio.c) */
#endif
        }
    }
    if (U32(P, 4) != 0 && U32(P, 0x10) == 0) {
        const rx_addr lp = U32(P, 0xd98);
        const uint32_t on = U32(lp, 4) != 0 && U32(lp, 0x10) == 0;
        SU32(P, 0xc70, on);
        if (on) ACT_CALL(1, rx_stunt_scoring_listen(P + 0xc70));
    }
    const rx_addr a = U32(P, 0x94);
    SU32(P, 0x10ec, 0xffffffffu);
    if (U32(a, 0x106c) != 0) {
        SU32(P, 0x10ec, 0); SF32(P, 0x10f0, 0.0f); SU32(P, 0x10f4, 0x3f800000u); SF32(P, 0x10f8, 0.0f); SF32(P, 0x10fc, 0.0f);
        SF32(P, 0x1100, 0.0f);
    }
    SU32(P, 0x1160, 1); SU32(P, 0x1130, 0); SU32(P, 0x1134, 1); SF32(P, 0x1164, 0.0f);
    if (a && U32(a, 0x23c) == 8) SU8(a, 0x114482, 0);
    player_race_reset(P);
    SU32(P, 0x11d8, 0); SF32(P, 0x1168, 0.0f); SF32(P, 0x116c, 0.0f);
    ACT_END(L);
}
RX_FUNCTION(0x0054d9d0, rx_player_join_activity)

/* FUN_0054cd20 (stdcall player; RET 4): its Player List row by name (+0x14 vs "PlayerName"): StartZone -> +0x1dc, Team -> +0x1fc */
void rx_player_start_zone(rx_addr P) {
    ACT_FRAME_EBP(L, 8, 4, 3);
    const rx_addr t = ACT_PLAYER_LIST;
    for (uint32_t r = 0;; r++) {
        if (((U32(t, 0x14) - U32(t, 0x10)) >> 2) == 0) break;
        if (r >= rx_tbl_column_rows(U32(U32(t, 0x10), 0))) break;
        const rx_addr cells = U32(act_column(t, 0x00936f00), 0x40);   /* "PlayerName" (std::string cells, 0x1c each) */
        if (r >= (U32(cells, 0x10) - U32(cells, 0xc)) / 0x1c) act_unreached(0x0054cdf0, "row index (_invalid_parameter_noinfo)");
        const rx_addr s = U32(cells, 0xc) + 0x1c * r;
        const rx_addr str = U32(s, 0x18) < 0x10 ? s + 4 : U32(s, 4);
        if (crt_strcmp(P + 0x14, str) != 0) continue;
        SU32(P, 0x1dc, act_cell(t, 0x0093df00, r));   /* "StartZone" */
        SU32(P, 0x1fc, act_cell(t, 0x0093df18, r));   /* "Team" */
        break;
    }
    ACT_END(L);
}
RX_FUNCTION(0x0054cd20, rx_player_start_zone)

/* the controls reset 006c9100 (EAX = controls) and the stunt system reset 006ca750 are build_activity_player.c's */
void rx_controls_reset(rx_addr c);

/* ======================================================================================================================== */
/* Activity_OnPlayerJoined 0055b610 (ECX = player; activity; RET 4) -> the driver                                             */

static float sqrtf_(float x) { return sqrtf(x); }   /* SqrtF 0042c230 (sqrtss) */

/* a driver appended to one of the activity's arrays {data, count, capacity, 0, grow} (A + 0x268 the racing drivers, + 0x27c the
 * waiting / spectators): grown by the grow step (alloc + copy + free) when full */
static void driver_array_push(rx_addr arr, rx_addr d) {
    if (I32(arr, 4) >= I32(arr, 8)) {
        uint32_t n = U32(arr, 0xc) + U32(arr, 8);
        /* PORT ADDITION (w-rc-core): the activity's arrays are made {12, step 0} (build_activity_race.c), so the original's growth keeps
         * 12 and the 13th driver is written past the block (0055bf0e: the original never has 13); grown to the rider cap instead. Never
         * reached with 12 or fewer drivers. */
        if (n <= U32(arr, 8)) n = U32(arr, 8) + 20u;
        const rx_addr p = rx_reflex_aligned_alloc(rx_mul_sat(n, 4));
        for (uint32_t i = 0; i < U32(arr, 8) * 4; i++) SU8(p, i, U8(U32(arr, 0), i));
        rx_reflex_aligned_free(U32(arr, 0));
        SU32(arr, 0, p);
        SU32(arr, 8, n);
    }
    SU32(U32(arr, 0), 4 * U32(arr, 4), d);
    SU32(arr, 4, U32(arr, 4) + 1);
}

rx_addr rx_activity_on_player_joined(rx_addr P, rx_addr A) {
    ACT_FRAME_EBP(L, 16, 0x64, 3);
    SU32(L, 0x14, P);
    SU32(P, 0xa0, 0);
    for (uint32_t i = 0; i < 0x104; i++) SU8(P + 0xa4, i, 0);
    SU32(P, 0x1a8, 0);
    rx_addr d = act_vcall(A, 0xb0, 1, (const uint32_t[]){P});           /* vf44: the driver */
    SU32(L, 0x1c, d);
    if (U32(A, 0x106c) != 0 && U32(P, 0x10) == 0 && U32(P, 0x1f0) == 0) SU32(U32(A, 0x106c), 0xa8, P);
    SU32(d, 0x10, A + 0x890);
    session_static();
    const rx_addr s = U32(0, 0x00da6cb0);
    const int online = U32(s, 0x1c4c) != 0 && U32(s, 0x1b0) != 1;
    if (!online) rx_stand_set(A, U32(A, 0x26c), d);                     /* (w-rc-core) standings 12..31 in side storage */
    else if (U32(P, 0x908) == 0 && (U32(A, 0x21c) == 5 || U32(A, 0x23c) == 8)) rx_stand_set(A, U32(A, 0x26c), d);
    ACT_CALL(0, rx_player_join_activity(P, A));                         /* 0054d9d0 */
    SU32(d, 0x74, P);
    ACT_CALL(0, rx_controls_reset(P + 0xb58));                          /* 006c9100 */
    ACT_CALL(0, rx_stunt_system_reset(P + 0xc70));                      /* 006ca750 */
    act_vcall(A, 0xbc, 1, (const uint32_t[]){d});                       /* vf47: the rules and behaviours */
    ACT_CALL(1, rx_driver_behaviors_reset(d));                          /* 00508f50 */
    ACT_CALL(1, rx_player_start_zone(U32(d, 0x74)));                    /* 0054cd20 */
    /* the start slot (A + 0x340, 0x30 each: position, direction, +0x28 users; the count +0x880): the first free one past the 16 grid
     * slots (free activities: mode 0), else the player's start zone, the first free one when it has none */
    if (U32(A, 0x238) == 0) {
        for (int32_t k = 0x10; k < I32(A, 0x880); k++) {   /* (w-rc-core) slots 28..31 in side storage */
            if (U32(rx_start_slot(A, (uint32_t)k), 0x28) == 0) {
                SU32(U32(d, 0x74), 0x1dc, (uint32_t)k);
                SU32(d, 0x144, rx_start_slot(A, (uint32_t)k));
                break;
            }
        }
    } else {
        const rx_addr p = U32(d, 0x74);
        int32_t k = I32(p, 0x1dc);
        if (k == -1) {
            k = 0;
            for (; k < I32(A, 0x880); k++) {
                if (U32(rx_start_slot(A, (uint32_t)k), 0x28) == 0) {
                    SU32(p, 0x1dc, (uint32_t)k);
                    SU32(d, 0x144, rx_start_slot(A, (uint32_t)k));
                    break;
                }
            }
            if (k == I32(A, 0x880)) { SU32(U32(d, 0x74), 0x1dc, 0); SU32(d, 0x144, A + 0x340); }
        } else SU32(d, 0x144, rx_start_slot(A, (uint32_t)k));
    }
    const rx_addr slot = U32(d, 0x144);
    SU32(slot, 0x28, U32(slot, 0x28) + 1);
    /* the start pose: the slot's position, the quaternion of the frame (right = dir x up, normalized; dir) */
    for (uint32_t k = 0; k < 4; k++) SU32(L, 0x40 + 4 * k, U32(slot, 4 * k));
    const rx_addr dir = slot + 0x10;
    SF32(L, 0x50, 0.0f); SF32(L, 0x54, 1.0f); SF32(L, 0x58, 0.0f);
    {   /* Vec3_Cross 00401630 (EAX = dir, ECX = up; out L + 0x20) */
        const float ax = F32(dir, 0), ay = F32(dir, 4), az = F32(dir, 8), bx = F32(L, 0x50), by = F32(L, 0x54), bz = F32(L, 0x58);
        SF32(L, 0x20, az * by - ay * bz);
        SF32(L, 0x24, ax * bz - bx * az);
        SF32(L, 0x28, bx * ay - ax * by);
    }
    float r0 = F32(L, 0x20);
    {
        const float x = F32(L, 0x20), y = F32(L, 0x24), z = F32(L, 0x28);
        const float len2 = y * y + z * z + x * x;
        SF32(L, 0x18, len2);
        if (len2 > 0.0f) {
            const float inv = 1.0f / sqrtf_(len2);
            r0 = inv * x;
            SF32(L, 0x24, inv * y);
            SF32(L, 0x28, inv * z);
        }
    }
    const float d0 = F32(dir, 0), d1 = F32(dir, 4), d2 = F32(dir, 8), r1 = F32(L, 0x24), r2 = F32(L, 0x28);
    const float W = ((d2 + r0) + 2.0f) * 0.25f;
    const float Aq = (((r0 + 1.0f) - 1.0f) - d2) * 0.25f;
    const float Bq = (((1.0f - r0) + 1.0f) - d2) * 0.25f;
    const float Cq = (((1.0f - r0) - 1.0f) + d2) * 0.25f;
    SF32(L, 0x18, W); SF32(L, 0x3c, Aq); SF32(L, 0x50, Bq); SF32(L, 0x38, Cq);
    float m = Aq > Bq ? Aq : Bq;
    if (!(m > Cq)) m = Cq;
    if (!(m > W)) m = W;
    if (m == W) {
        const float sq = sqrtf_(W), f = 0.25f / sq;
        SF32(L, 0x5c, sq);
        SF32(L, 0x50, (-0.0f - d1) * f);
        SF32(L, 0x54, (d0 - r2) * f);
        SF32(L, 0x58, r1 * f);
    } else {
        float m2 = Bq > Cq ? Bq : Cq;
        if (!(m2 > Aq)) m2 = Aq;
        if (m2 == Aq) {
            const float sq = sqrtf_(Aq), f = 0.25f / sq;
            SF32(L, 0x50, sq);
            SF32(L, 0x5c, (-0.0f - d1) * f);
            SF32(L, 0x54, r1 * f);
            SF32(L, 0x58, (d0 + r2) * f);
        } else if (Bq > Cq) {
            const float sq = sqrtf_(Bq), f = 0.25f / sq;
            SF32(L, 0x54, sq);
            SF32(L, 0x5c, (d0 - r2) * f);
            SF32(L, 0x50, r1 * f);
            SF32(L, 0x58, d1 * f);
        } else {
            const float sq = sqrtf_(Cq), f = 0.25f / sq;
            SF32(L, 0x58, sq);
            SF32(L, 0x5c, r1 * f);
            SF32(L, 0x50, (d0 + r2) * f);
            SF32(L, 0x54, d1 * f);
        }
    }
    {
        const rx_addr v = U32(U32(d, 0x74), 0x1ac);   /* the vehicle: its start pose */
        for (uint32_t k = 0; k < 4; k++) SU32(v, 0x200 + 4 * k, U32(L, 0x40 + 4 * k));
        for (uint32_t k = 0; k < 4; k++) SU32(v, 0x210 + 4 * k, U32(L, 0x50 + 4 * k));
        SU32(v, 0x180, 1);
    }
    /* the player's race index (+0x148): its unique id, else the first free one of the activity's 12 (+0x304) */
    if (I32(U32(L, 0x14), 0xc) >= 0) SU32(d, 0x148, U32(U32(L, 0x14), 0xc));
    else {
        uint32_t k = 0;
        for (; k < RX_RC_MAX; k++) if (rx_raceidx_taken(A, k) == 0) break;   /* (w-rc-core) 0xc in the original: 12..31 in side storage */
        if (k == RX_RC_MAX) act_unreached(0x0055bb9a, "no free race index (32 taken)");
        rx_raceidx_set(A, k, 1);
        SU32(d, 0x148, k);
    }
    if (U32(U32(d, 0x74), 8) == 0) SU32(U32(d, 0x74), 0x1cc, 1);
    act_vcall(d, 4, 0, 0);                                              /* vf1 */
    session_static();
    if (!online) driver_array_push(A + 0x268, d);
    else act_unreached(0x0055bbfe, "a player joining an online activity");
    {   /* "PlayerJoined" on the activity channel [0x00a93f74] with (activity, driver) */
        const uint32_t payload[2] = {A, d};
        race_raise_event(U32(0, 0x00a93f74), 0x00941ba8, 0x009cda18, payload, 2);
    }
    {   /* the first local human driver: vf50 (the activity's start for the player) */
        uint32_t humans = 0;
        for (uint32_t i = 0; i < U32(A, 0x26c); i++) if (U32(U32(U32(U32(A, 0x268), 4 * i), 0x74), 0x10) == 0) humans++;
        if (humans == 1 && U32(U32(d, 0x74), 4) != 0 && U32(U32(d, 0x74), 0x10) == 0) act_vcall(A, 0xc8, 0, 0);
    }
    const rx_addr p = U32(d, 0x74);
    if (U32(p, 4) != 0 && U32(p, 0x10) == 0 && U32(A, 0x238) != 4) {   /* the local human's records */
        uint32_t type = U32(A, 0x23c);
        series_static();
        if (U32(0, 0x009e2318) == 2 || U32(0, 0x009e2318) == 1) ACT_CALLR(type, 1, rx_career_current_event_type(0x009e2188));
        ACT_CALL(5, rx_records_join(U32(p, 0x1f0), U32(0, 0x00a946c0), U32(p, 0x1b0), type, U32(A, 0x220), U32(A, 0x234) + 0x20030));   /* 00764170 MXRecords_Join (EAX = slot) */
    }
    if (U32(A, 0x21c) == 5) {   /* an inactive activity (the front end's free ride): the vehicle placed at the slot at once */
        const rx_addr sl = U32(d, 0x144), v = U32(U32(d, 0x74), 0x1ac), body = U32(v, 0x234);
        const uint32_t a[5] = {sl, sl + 0x10, L + 0x20, L + 0x40, L + 0x50};
        act_vcall(body, 0x50, 5, a);                                    /* the start frame of the slot */
        ACT_CALL(3, rx_vehicle_place(v, L + 0x20, L + 0x40, L + 0x50));   /* 0045f600: ECX = L + 0x50, stack v, position, axis a */
        const rx_addr pp = U32(d, 0x74);
        if (U32(pp, 0x1ac)) act_vcall(U32(pp, 0x1ac), 0x80, 1, (const uint32_t[]){0});
        const rx_addr rd = U32(pp, 0x1b4);
        if (rd) { SU8(rd, 0x5bb64, 0); SU32(rd, 0x5b6d0, 0); SU32(rd, 0x5b568, 0); SF32(rd, 0x5b570, 0.0f); }
        SU32(pp, 0x11a0, 2);
    } else {
        act_vcall(d, 0xc, 0, 0);                                        /* vf3 */
        SU32(U32(d, 0x74), 0x214, 1);
        /* FUN_0075f890 (game, type, mode, 1): the grid slots */
        const uint32_t a[4] = {U32(0, 0x00a946b8), U32(A, 0x23c), U32(A, 0x238), 1};
        act_call_fn(0x0075f890, RX_STDCALL, 0, 4, a);
    }
    ACT_END(L);
    return d;
}
RX_FUNCTION(0x0055b610, rx_activity_on_player_joined)

/* ======================================================================================================================== */
/* the drivers' vf1 (handlers) and the behaviours' vf3 (links to the other behaviours) / vf6 (reset)                          */

/* NetworkDriver::vf1 0050e380 (ECX = driver): "NetworkObjectRemoved" / "NetworkObjectMigrated" on the session channel [0x00da6ca0]
 * (0053a800: EAX = name, ECX = channel; driver, callback) */
void rx_network_driver_vf1(rx_addr d) {
    ACT_FRAME(L, 0, 3);
    session_static();
    ACT_CALL(1, drv_register(U32(0, 0x00da6ca0), 0x0093ee98, 0x00940310, 0x009cdc7c, d, 0x0050eaa0));
    session_static();
    ACT_CALL(1, drv_register(U32(0, 0x00da6ca0), 0x0093eeb0, 0x00940310, 0x009cdc7c, d, 0x00592ac0));
    ACT_END(L);
}
RX_FUNCTION(0x0050e380, rx_network_driver_vf1)

/* RaceDriver::vf1 0050ee40 (ECX = driver): a ghost (+0x74 +0x10 == 2) listens to "Ghost Session Completed" on its activity's
 * channel (0053a5b0); under the session's lock: the network player's state sent when it is the local one (0050bad0 / 006fea10:
 * network); "NetworkActivityEvent" on the session channel (0053a8c0); NetworkDriver::vf1 */
void rx_race_driver_vf1(rx_addr d) {
    ACT_FRAME_EBP(L, 8, 0, 4);
    const rx_addr p = U32(d, 0x74);
    if (U32(p, 0x10) == 2)
        ACT_CALL(1, drv_register(U32(U32(p, 0x94), 0x290), 0x0093ee68 /* "Ghost Session Completed" */, 0x009402f0, 0x009cda18, d, 0x005a4150));
    session_static();
    if (U32(0, 0x00da6d08) != 0) act_unreached(0x0050ee92, "the networked session's lock (00402420)");
    const rx_addr lock = 0x00da6cf0;
    if (U8(0, 0x009df580) == 0) drv_net_call(0x00883d80, RX_THISCALL, lock, 0, 0);
    const rx_addr np = U32(d, 0xef0);
    if (np && (U8(np, 0x20) & 1) && (drv_net_call(0x00884950, RX_THISCALL, np, 0, 0) & 0xff)) {
        const uint32_t a[7] = {U32(d, 0x74), d + 0x10d8, d + 0x10c4, d + 0x10b4, d + 0x10c8, d + 0x10cc, d + 0xf1c};
        drv_net_call(0x0050bad0, RX_CDECL, 0, 7, a);
        rx_regs in; memset(&in, 0, sizeof in); in.r[6] = U32(d, 0xef0);
        drv_net_call_ex(0x006fea10, &in, 0, 0);
    }
    session_static();
    ACT_CALL(1, drv_register(U32(0, 0x00da6ca0), 0x0093eed8 /* "NetworkActivityEvent" */, 0x00940330, 0x009cdd3c, d, 0x00512990));
    ACT_CALL(0, rx_network_driver_vf1(d));
    if (U8(0, 0x009df580) == 0) drv_net_call(0x00883d90, RX_THISCALL, lock, 0, 0);
    ACT_END(L);
}
RX_FUNCTION(0x0050ee40, rx_race_driver_vf1)

/* the driver's behaviour of a kind (vf5 (+0x14) of each, in slot order), or 0: 0053b450 (0x14), 0053b960 (0x12), 0053ba00 (0x10),
 * 0053b9b0 (6), 0053ba50 (4), 0053b910 (0xb) */
static rx_addr beh_find(rx_addr d, uint32_t kind) {
    for (uint32_t i = 0; i < U32(d, 0xee0); i++) if (act_vcall(U32(d, 0xe8 + 4 * i), 0x14, 0, 0) == kind) return U32(d, 0xe8 + 4 * i);
    return 0;
}
rx_addr rx_driver_behavior_14(rx_addr d) { return beh_find(d, 0x14); }   /* 0053b450 */
RX_FUNCTION(0x0053b450, rx_driver_behavior_14)
rx_addr rx_driver_behavior_12(rx_addr d) { return beh_find(d, 0x12); }   /* 0053b960 */
RX_FUNCTION(0x0053b960, rx_driver_behavior_12)
rx_addr rx_driver_behavior_10(rx_addr d) { return beh_find(d, 0x10); }   /* 0053ba00 */
RX_FUNCTION(0x0053ba00, rx_driver_behavior_10)
rx_addr rx_driver_behavior_6(rx_addr d) { return beh_find(d, 6); }       /* 0053b9b0 */
RX_FUNCTION(0x0053b9b0, rx_driver_behavior_6)
rx_addr rx_driver_behavior_4(rx_addr d) { return beh_find(d, 4); }       /* 0053ba50 */
RX_FUNCTION(0x0053ba50, rx_driver_behavior_4)
rx_addr rx_driver_behavior_b(rx_addr d) { return beh_find(d, 0xb); }     /* 0053b910 */
RX_FUNCTION(0x0053b910, rx_driver_behavior_b)

#define BEH_LINK(o, off, call) do { rx_addr r_; ACT_CALLR(r_, 1, call(U32((o), 8))); SU32((o), (off), r_); } while (0)
void rx_fix_forward_vf3(rx_addr o) { BEH_LINK(o, 0x5c, rx_driver_behavior_14); }                                     /* 00523480 */
RX_FUNCTION(0x00523480, rx_fix_forward_vf3)
void rx_lap_pacing_vf3(rx_addr o) { BEH_LINK(o, 0x24, rx_driver_behavior_5); }                                       /* 005254c0 */
RX_FUNCTION(0x005254c0, rx_lap_pacing_vf3)
void rx_blue_yellow_vf6(rx_addr o) { SU32(o, 0xc, 0); SU32(o, 0x14, 0); SU32(o, 0x10, 0); }                         /* 005225b0 */
RX_FUNCTION(0x005225b0, rx_blue_yellow_vf6)
void rx_seperate_vf3(rx_addr o) { BEH_LINK(o, 0x9c, rx_driver_behavior_3); BEH_LINK(o, 0xa0, rx_driver_behavior_12); }   /* 00531910 */
RX_FUNCTION(0x00531910, rx_seperate_vf3)
void rx_air_leveling_vf3(rx_addr o) { BEH_LINK(o, 0xc, rx_driver_navigate_behavior); BEH_LINK(o, 0x10, rx_driver_behavior_5); }   /* 0051f400 */
RX_FUNCTION(0x0051f400, rx_air_leveling_vf3)
void rx_offtrack_vf3(rx_addr o) { BEH_LINK(o, 0xc, rx_driver_behavior_10); }                                         /* 00529110 */
RX_FUNCTION(0x00529110, rx_offtrack_vf3)
void rx_navigate_track_vf3(rx_addr o) {                                                                              /* 00527280 */
    BEH_LINK(o, 0x2c, rx_driver_behavior_3); BEH_LINK(o, 0x30, rx_driver_behavior_6); BEH_LINK(o, 0x34, rx_driver_behavior_12);
    BEH_LINK(o, 0x38, rx_driver_air_leveling_behavior);
}
RX_FUNCTION(0x00527280, rx_navigate_track_vf3)
void rx_navigate_track_vf6(rx_addr o) {                                                                              /* 005272c0 */
    SU32(o, 0xc, 0xffffffffu); SU32(o, 0x10, 0); SU32(o, 0x14, 0xffffffffu); SU32(o, 0x18, 0); SU32(o, 0x3c, 1);
    SU32(o, 0x40, 0x3f800000u); SU32(o, 0x44, 0x3f800000u); SU32(o, 0x48, 0x3f800000u); SU32(o, 0x28, 0); SU32(o, 0x4c, 0);
}
RX_FUNCTION(0x005272c0, rx_navigate_track_vf6)
void rx_override_spline_vf3(rx_addr o) {                                                                             /* 00529b70 */
    BEH_LINK(o, 0x5c, rx_driver_navigate_behavior); BEH_LINK(o, 0x60, rx_driver_behavior_4); BEH_LINK(o, 0x64, rx_driver_behavior_3);
}
RX_FUNCTION(0x00529b70, rx_override_spline_vf3)
void rx_handle_braking_vf3(rx_addr o) {                                                                              /* 005248a0 */
    BEH_LINK(o, 0x48, rx_driver_behavior_b); BEH_LINK(o, 0x4c, rx_driver_behavior_12); BEH_LINK(o, 0x50, rx_driver_navigate_behavior);
}
RX_FUNCTION(0x005248a0, rx_handle_braking_vf3)
void rx_split_section_vf3(rx_addr o) { BEH_LINK(o, 0xc, rx_driver_behavior_1); }                                     /* 00533950 */
RX_FUNCTION(0x00533950, rx_split_section_vf3)
void rx_take_line_vf3(rx_addr o) { BEH_LINK(o, 0xc, rx_driver_behavior_1); BEH_LINK(o, 0x10, rx_driver_behavior_6); }  /* 00534440 */
RX_FUNCTION(0x00534440, rx_take_line_vf3)
void rx_take_line_vf6(rx_addr o) { SU32(o, 0x14, 0); SU32(o, 0x1c, 0); }                                             /* 00534460 */
RX_FUNCTION(0x00534460, rx_take_line_vf6)
void rx_avoid_track_boundaries_vf6(rx_addr o) { SU32(o, 0x10, 0); SU32(o, 0xc, 0x1b); }                               /* 00521b30 */
RX_FUNCTION(0x00521b30, rx_avoid_track_boundaries_vf6)

/* MXActivityFreeRide::vf47 0x00560c20 (ECX = activity; driver; RET 4): the free ride's rules and behaviours: its rule (unless the world's +0x22144 is 1), and when
 * the mode (+0x238) is not 4: avoid wreck, 00587750, air leveling, fix forward, 00587840, avoid objects, avoid flip back (each created in the driver's block
 * and bound to the driver) */
void rx_free_ride_vf47(rx_addr A, rx_addr d) {
    ACT_FRAME(L, 0, 2);
    if (U32(U32(A, 0x234), 0x22144) != 1) beh_add(d, &G_FR_RULE);
    if (U32(A, 0x238) != 4) {
        beh_add(d, &G_AVOID_WRECK); beh_add(d, &G_587750);
        beh_bind(beh_get_air_leveling(d), d);
        beh_add(d, &G_FIX_FORWARD); beh_add(d, &G_587840); beh_add(d, &G_AVOID_OBJECTS); beh_add(d, &G_AVOID_FLIP_BACK);
    }
    ACT_END(L);
}
RX_FUNCTION(0x00560c20, rx_free_ride_vf47)

/* FreeRideDriver::vf1 0050bce0 (ECX = driver): "VehicleWrecked" / "VehicleStopWreck" on the player's activity's channel (0053a360: EAX = name, ECX = channel;
 * driver, callback: proxy vtable 0x009402d0, signature 0x009cdb7c) -> 0050c8a0 / 0050c8d0, then NetworkDriver::vf1 (a tail jump) */
void rx_free_ride_driver_vf1(rx_addr d) {
    ACT_FRAME(L, 0, 1);
    ACT_CALL(2, drv_register(U32(U32(U32(d, 0x74), 0x94), 0x290), 0x0093ee38 /* "VehicleWrecked" */, 0x009402d0, 0x009cdb7c, d, 0x0050c8a0));
    ACT_CALL(2, drv_register(U32(U32(U32(d, 0x74), 0x94), 0x290), 0x0093ee48 /* "VehicleStopWreck" */, 0x009402d0, 0x009cdb7c, d, 0x0050c8d0));
    ACT_END(L);
    rx_network_driver_vf1(d);
}
RX_FUNCTION(0x0050bce0, rx_free_ride_driver_vf1)

/* Finite actual Waypoint constructors; reuse this unique canonical kernel. */
static const beh_getter G_WP_ADVANCE={0x58a480,1,0x10,0x940030},G_WP_LINES={0x587e00,0,0x70,0x9401f8},G_WP_NAVIGATE={0x58a4d0,0,0x1c0,0x93fb94},G_WP_CORNER={0x58a520,0,0x40,0x940078};
rx_addr rx_waypoint_get_free_ride_rule(rx_addr driver){return beh_get(driver,&G_FR_RULE);}
RX_FUNCTION(0x00587690,rx_waypoint_get_free_ride_rule)
rx_addr rx_waypoint_get_advance_rule(rx_addr driver){return beh_get(driver,&G_WP_ADVANCE);}
RX_FUNCTION(0x0058a480,rx_waypoint_get_advance_rule)
rx_addr rx_waypoint_get_avoid_wreck(rx_addr driver){return beh_get(driver,&G_AVOID_WRECK);}
RX_FUNCTION(0x005876f0,rx_waypoint_get_avoid_wreck)
rx_addr rx_waypoint_get_suck_racing(rx_addr driver){return beh_get(driver,&G_SUCK_RACING);}
RX_FUNCTION(0x005888b0,rx_waypoint_get_suck_racing)
rx_addr rx_waypoint_get_air_leveling(rx_addr driver){return beh_get_air_leveling(driver);}
RX_FUNCTION(0x005877a0,rx_waypoint_get_air_leveling)
rx_addr rx_waypoint_get_fix_forward(rx_addr driver){return beh_get(driver,&G_FIX_FORWARD);}
RX_FUNCTION(0x005877f0,rx_waypoint_get_fix_forward)
rx_addr rx_waypoint_get_brake_turn(rx_addr driver){return beh_get(driver,&G_BRAKE_TURN);}
RX_FUNCTION(0x00588590,rx_waypoint_get_brake_turn)
rx_addr rx_waypoint_get_racing_lines(rx_addr driver){return beh_get(driver,&G_WP_LINES);}
RX_FUNCTION(0x00587e00,rx_waypoint_get_racing_lines)
rx_addr rx_waypoint_get_waypoint_navigation(rx_addr driver){return beh_get(driver,&G_WP_NAVIGATE);}
RX_FUNCTION(0x0058a4d0,rx_waypoint_get_waypoint_navigation)
rx_addr rx_waypoint_get_avoid_objects(rx_addr driver){return beh_get(driver,&G_AVOID_OBJECTS);}
RX_FUNCTION(0x00587890,rx_waypoint_get_avoid_objects)
rx_addr rx_waypoint_get_cornering(rx_addr driver){return beh_get(driver,&G_WP_CORNER);}
RX_FUNCTION(0x0058a520,rx_waypoint_get_cornering)
rx_addr rx_waypoint_get_avoid_flip_back(rx_addr driver){return beh_get(driver,&G_AVOID_FLIP_BACK);}
RX_FUNCTION(0x005878e0,rx_waypoint_get_avoid_flip_back)
