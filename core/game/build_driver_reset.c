/* build_driver_reset.c -- the drivers' reset: BaseDriver::vf3 00508f90 (a driver's start / restart: the vehicle's and rider's state, the player's controls and
 * stunts), NetworkDriver::vf3 0050e400 (the network player's state under the session's lock, then the base) and FreeRideDriver::vf3 0050bd80 ("Reset Hint" on the
 * activity's channel, the vehicle placed at the driver's start slot, the behaviours' and rules' vf6, the driver's pace behaviour).
 * The session's lock (Quazal 00883d80 / 00883d90, 00884950) is not core: the oracle runs the original, the product nothing (build_activity_driver.c's rule). */
#include "build_activity_util.h"
#include "rider_cap.h"
#include "build_activity_int.h"
#include "build_activity.h"
#include "build_driver_reset.h"
#include "build_rider_util.h"
#include "race.h"
#include "race_misc.h"
#include "race_activity.h"
#include "loop_player.h"
#include "loop_vehicle.h"
#include "bike_sim.h"
#include "anim_scenario.h"
#include "phys_body.h"
#include <string.h>
#include <stdio.h>

static uint32_t net_call(rx_addr fn, uint32_t ecx) {
#ifdef RX_ORACLE
    return rx_call_original(fn, RX_THISCALL, ecx, 0, NULL);
#else
    (void)fn; (void)ecx; return 0;
#endif
}
static void net_call_ex(rx_addr fn, const rx_regs* in, int n, const uint32_t* a) {
#ifdef RX_ORACLE
    rx_regs o; rx_call_original_ex(fn, in, &o, n, a);
#else
    (void)fn; (void)in; (void)n; (void)a;
#endif
}
static void session_static(void) {
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
}

/* 0051f150 (EAX = the kind) */
uint32_t rx_activity_kind_flag(uint32_t kind) {
    if (kind > 0x13) return 1;
    switch (kind) { case 0: case 2: case 6: case 0x11: case 0x13: return 0; default: return 1; }
}
RX_FUNCTION(0x0051f150, rx_activity_kind_flag)

/* 00508ad0 (EAX = the player): +0x122c / +0x1230 / +0x1224 / +0x1280 / +0x1250 .. +0x1258 / +0x1260 .. +0x1268 / +0x1270 .. +0x1278 = 0, +0x1220 = 1 */
void rx_driver_reset_timers(rx_addr p) {
    static const uint16_t z[] = {0x122c, 0x1230, 0x1224, 0x1280, 0x1250, 0x1254, 0x1258, 0x1260, 0x1264, 0x1268, 0x1270, 0x1274, 0x1278};
    for (unsigned i = 0; i < sizeof z / sizeof *z; i++) SU32(p, z[i], 0);
    SU32(p, 0x1220, 1);
}
RX_FUNCTION(0x00508ad0, rx_driver_reset_timers)

/* MudRecord_Clear 00409320 (EDX = the vehicle): the mud record of the twelve (0x00a96d90 + 0x40 k; owner +0x30) owned by the vehicle: its eight values cleared */
void rx_mud_record_clear(rx_addr v) {
    for (uint32_t k = 0; k < 0xc; k++) {
        const rx_addr r = 0x00a96d90u + 0x40u * k;
        if (U32(r, 0x30) == v) { for (uint32_t o = 0; o < 0x20; o += 4) SU32(r, o, 0); return; }
    }
    const rx_addr r = rx_mud_side_of(v);   /* (w-rc-core) */
    if (r) for (uint32_t o = 0; o < 0x20; o += 4) SU32(r, o, 0);
}
RX_FUNCTION(0x00409320, rx_mud_record_clear)

/* BaseDriver::vf3 00508f90 (ECX = the driver) */
void rx_base_driver_vf3(rx_addr d) {
    ACT_FRAME(L, 0, 5);
    SU32(d, 0x334, 0); SU32(d, 0x348, 0); SU32(d, 0x35c, 0); SU32(d, 0x370, 0); SU32(d, 0x384, 0); SU32(d, 0x398, 0); SU32(d, 0x3ac, 0);
    SU32(d, 0x2dc, 0); SU32(d, 0x2e0, 0); SU32(d, 0x2e4, 0); SU32(d, 0x2e8, 0);
    crt_memset(d + 0x14c, 0, 0xc8);
    rx_addr P = U32(d, 0x74);
    SU32(d, 0x134, 0); SU32(d, 0x70, 0); SU32(d, 0x78, 0); SU32(d, 0x7c, 0); SU32(d, 0x140, 0); SU32(d, 0xeec, 1);
    const rx_addr veh = U32(P, 0x1ac);
    if (veh == 0) { ACT_END(L); return; }
    int branch_a = 0;
    if (U32(P, 0x1e4) == 0) {
        const rx_addr np = U32(P, 0xb2c);
        if (np == 0 || U8(np, 0xc8) == 0) branch_a = 1;
        else {
            uint32_t view;
            ACT_CALLR(view, 0, rx_game_is_view_track(U32(0, 0x00a946b8)));
            branch_a = view == 0;
        }
    }
    rx_addr rider;
    if (branch_a) {
        SU8(U32(veh, 0x1328), 0x14e, 1);
        { const uint32_t a[1] = {1}; act_vcall(U32(P, 0x1ac), 0xc4, 1, a); }
        if (U32(U32(P, 0x1ac), 0x17c) != 0) {
            rx_addr e;
            ACT_CALLR(e, 0, phys_body_record(U32(U32(P, 0x1ac), 0x17c)));
            SU32(U32(e, 0x2c), 0x14, 0);
        }
        const rx_addr sim = U32(U32(P, 0x1ac), 0x234);
        ACT_CALL(0, bike_end_pose_override(sim));
        rider = U32(P, 0x1b4);
        if (rider) { SU32(rider, 0x5bb5c, 1); SU8(rider, 0xa2, 1); SU8(rider, 0x5bb64, 1); SU32(rider, 0x5b6d0, 0); }
    } else {
        if (U32(veh, 0x17c) != 0) {
            rx_addr e;
            ACT_CALLR(e, 0, phys_body_record(U32(veh, 0x17c)));
            SU32(U32(e, 0x2c), 0x14, 0xffffffffu);
        }
        { const uint32_t a[1] = {0}; act_vcall(U32(P, 0x1ac), 0xc4, 1, a); }
        SU8(U32(U32(P, 0x1ac), 0x1328), 0x14e, 0);
        rider = U32(P, 0x1b4);
        if (rider) { SU32(rider, 0x5bb5c, 0); SU8(rider, 0xa2, 0); SU8(rider, 0x5bb64, 0); SU32(rider, 0x5b6d0, 0); }
    }
    P = U32(d, 0x74);
    ACT_CALL(0, rx_player_set_fade_clear(P));                           /* 0054b7f0 (ESI = the player) */
    uint32_t kind;
    ACT_CALLR(kind, 0, rx_activity_ui_kind(U32(P, 0x94)));              /* 00556280 */
    SU32(d, 0x138, 2);
    SU32(d, 0x328, rx_activity_kind_flag(kind));
    P = U32(d, 0x74);
    { const uint32_t t = U32(U32(P, 0x1ac), 0x194); SU32(d, 0x32c, t == 0 ? 1 : t == 1 ? 0 : 2); }
    ACT_CALL(0, rx_vehicle_release_controls(U32(P, 0x1ac)));            /* 0045f590 (EDI = the vehicle) */
    SU32(U32(U32(P, 0x1ac), 0x234), 0x54, 1);
    ACT_CALL(0, rx_controls_reset(U32(d, 0x74) + 0xb58));               /* 006c9100 */
    ACT_CALL(0, rx_stunt_system_reset(U32(d, 0x74) + 0xc70));           /* 006ca750 */
    ACT_CALL(0, rx_driver_reset_timers(U32(d, 0x74)));                  /* 00508ad0 */
    P = U32(d, 0x74);
    if (U32(P, 0x1ac) != 0) ACT_CALL(0, rx_mud_record_clear(U32(P, 0x1ac)));
    const rx_addr ra = U32(U32(U32(d, 0x74), 0x1ac), 0x1a0);
    if (ra != 0) ACT_CALL(0, rx_rider_animation_reset_pose(ra));        /* 00619050 (EDI) */
    SU32(d, 0x2f0, 0); SU32(d, 0x2f4, 0); SU32(d, 0x30c, 0); SU32(d, 0x31c, 0); SU32(d, 0x324, 0); SU32(d, 0x318, 0); SU32(d, 0x320, 0);
    SU32(d, 0x2f8, 0); SU32(d, 0x300, 0); SU32(d, 0x304, 0);
    if (U32(d, 0x32c) == 2) {
        SU32(d, 0x370, 0); SU32(d, 0x374, U32(0, 0x009776a8)); SU32(d, 0x378, U32(0, 0x00931f24));
        SU32(d, 0x398, 0); SU32(d, 0x39c, U32(0, 0x009776a8)); SU32(d, 0x3a0, U32(0, 0x00931f24));
    }
    ACT_END(L);
}
RX_FUNCTION(0x00508f90, rx_base_driver_vf3)

/* NetworkDriver::vf3 0050e400 (ECX = the driver): under the session's lock the network player's state cleared (+0xef4 / +0xef8), a remote one's rider blended 0.25
 * (006237a0), +0xefc = 1, +0xf00 = 0, the base's vf3 */
void rx_network_driver_vf3(rx_addr d) {
    ACT_FRAME_EBP(L, 8, 4, 3);
    session_static();
    if (U32(0, 0x00da6d08) != 0) act_unreached(0x0050e437, "the networked session's lock (00402420)");
    const rx_addr lock = 0x00da6cf0;
    if (U8(0, 0x009df580) == 0) net_call(0x00883d80, lock);
    const rx_addr np = U32(d, 0xef0);
    SU32(d, 0xef4, 0); SU32(d, 0xef8, 0);
    if (np && !(net_call(0x00884950, np) & 0xff) && U32(U32(d, 0x74), 0x1b4)) {
        rx_regs in; memset(&in, 0, sizeof in); in.r[0] = 4; in.r[1] = U32(U32(d, 0x74), 0x1b4);
        const uint32_t a = 0x3e800000u;
        net_call_ex(0x006237a0, &in, 1, &a);
    }
    SU32(d, 0xefc, 1); SU32(d, 0xf00, 0);
    ACT_CALL(0, rx_base_driver_vf3(d));
    if (U8(0, 0x009df580) == 0) net_call(0x00883d90, lock);
    ACT_END(L);
}
RX_FUNCTION(0x0050e400, rx_network_driver_vf3)

/* 005092b0 / 005092f0 (stdcall driver; RET 4): the behaviours' (+0xe8, count +0xee0) / the rules' (+0xa0, count +0xee4) vf6 (+0x18) */
void rx_driver_behaviors_vf6(rx_addr d) {
    for (uint32_t i = 0; i < U32(d, 0xee0); i++) act_vcall(U32(d, 0xe8 + 4 * i), 0x18, 0, 0);
}
RX_FUNCTION(0x005092b0, rx_driver_behaviors_vf6)
void rx_driver_rules_vf6(rx_addr d) {
    for (uint32_t i = 0; i < U32(d, 0xee4); i++) act_vcall(U32(d, 0xa0 + 4 * i), 0x18, 0, 0);
}
RX_FUNCTION(0x005092f0, rx_driver_rules_vf6)

/* 0053a4c0 (stdcall driver; RET 4): the first behaviour whose vf5 (+0x14) is 0xf, else 0 */
rx_addr rx_driver_behavior_f(rx_addr d) {
    for (uint32_t i = 0; i < U32(d, 0xee0); i++)
        if (act_vcall(U32(d, 0xe8 + 4 * i), 0x14, 0, 0) == 0xf) return U32(d, 0xe8 + 4 * i);
    return 0;
}
RX_FUNCTION(0x0053a4c0, rx_driver_behavior_f)

/* FreeRideDriver::vf3 0050bd80 (ECX = the driver): NetworkDriver::vf3; "Reset Hint" (void(BaseDriver*, int) {driver, 0}) on the player's activity's channel;
 * the vehicle placed (00460020: the sound / effect component's query, then the vehicle placed) by the player's activity kind (+0x9c): 8 / 0x3e: the driver's
 * start slot (+0x144) and slot + 0x10 for a player with a vehicle; 0x16 / 0x1b / 0x1d: the player's +0x10c0 / +0x10d0; any other: for a human (+0x10 == 0) with
 * a vehicle the vehicle's vf17 (+0x44) writes a record, placed with the vehicle's +0x130; then the behaviours' and the rules' vf6 (005092b0 / 005092f0), +0xf10 =
 * the driver's pace behaviour (kind 0xf, 0053a4c0), +0xf14 = 0.0 */
void rx_free_ride_driver_vf3(rx_addr d) {
    ACT_FRAME_EBP(L, 16, 0x24, 3);
    SU32(L, 0x2c, U32(0, 0x009b45e4) ^ (L + 12));                       /* the /GS cookie */
    ACT_CALL(0, rx_network_driver_vf3(d));
    rx_addr P = U32(d, 0x74);
    const rx_addr ch = U32(U32(P, 0x94), 0x290);
    SU32(L, 0x10, d); SU32(L, 0x14, 0);
    { const uint32_t payload[2] = {d, 0}; race_raise_event(ch, 0x0093ee5c /* "Reset Hint" */, 0x009cd9f8, payload, 2); }
    P = U32(d, 0x74);
    const uint32_t type = U32(P, 0x9c);
    rx_addr a = 0, b = 0, veh = 0;
    int place = 0;
    if (type == 8 || type == 0x3e) {
        if (U32(P, 0x1ac) != 0) { a = U32(d, 0x144); b = a + 0x10; veh = U32(P, 0x1ac); place = 1; }
    } else if (type == 0x16 || type == 0x1b || type == 0x1d) {
        veh = U32(P, 0x1ac); a = P + 0x10c0; b = P + 0x10d0; place = 1;
    } else if (U32(P, 0x10) == 0 && U32(P, 0x1ac) != 0) {
        const rx_addr buf = L + 0x10;
        const uint32_t arg[1] = {buf};
        act_vcall(U32(P, 0x1ac), 0x44, 1, arg);
        veh = U32(U32(d, 0x74), 0x1ac); a = buf; b = veh + 0x130; place = 1;
    }
    if (place) ACT_CALL(2, rx_vehicle_effect_query(veh, a, b));         /* 00460020 (ESI = the vehicle) */
    ACT_CALL(1, rx_driver_behaviors_vf6(d));
    ACT_CALL(1, rx_driver_rules_vf6(d));
    rx_addr pace;
    ACT_CALLR(pace, 1, rx_driver_behavior_f(d));
    SU32(d, 0xf10, pace); SU32(d, 0xf14, 0);
    ACT_END(L);
}
RX_FUNCTION(0x0050bd80, rx_free_ride_driver_vf3)
