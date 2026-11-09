/* race_beh2_perform_stunt.c -- BehaviorPerformStunt: the AI rider that performs stunts in the air in a Free Ride
 * (2026-10-26, agent w-stunts). Its lifecycle virtuals (init / reset / activate) are race_beh2_perform_init.c; this file is the
 * update vf4 0052aac0 and its state machine.
 *
 * Layout: +0x04 enabled (motorcycle / ATV only), +0x08 the driver, +0x0c the state (0 idle, 1 plan, 2 predict the landing, 3 wait,
 * 4 perform, 5 next stunt), +0x10 the mode of the take-off test (0 test, 1 / 2 a take-off was started on the left / right, 3 the
 * vehicle's vf45 clears it), +0x20 the take-off timer, +0x24 the side drawn (1 / 2), +0x30..+0x38 the position at the start, +0x40 the
 * airborne time (the prediction's time to land, then the driver clock), +0x44 / +0x48 the two stunt machines' idle flags,
 * +0x50 the wait counter, +0x54 the minimum time to land, +0x58 the countdown, +0x5c the plan's kind (1 from the landing prediction,
 * 2 from a start on the ground), +0x90 the current stunt index, +0x94 the number of stunts planned (at most 3), +0x98 the spacing,
 * +0x9c the time the current stunt ends, +0xa0 when the next starts, +0xa4 the driver's air-leveling behaviour (disabled while a
 * stunt is flown from the ground), and the planned stunts at +0x60 + 16 * i: {+0 the stunt record, +4 its clip length, +8 its
 * hold time (006cd1c0), +0xc the performance asset's +0x28}. The player's current stunt (player +0xb7c) is the one flown.
 *
 * Every function is a hand transliteration of the original at its stack frame (address in the comment). The behaviour is
 * the ESI (or EAX) register of the state functions. */
#include "race_beh2_extern.h"
#include "race_beh1_extern.h"
#include "phys_crt.h"
#include "fx.h"
#include "rx_vcall.h"
#include "race_misc.h"

#define PS_DRIVER(b)   U32((b), 8)
#define PS_PLAYER(b)   U32(PS_DRIVER(b), 0x74)
#define PS_VEHICLE(b)  U32(PS_PLAYER(b), 0x1ac)
#define PS_CLOCK(b)    F32(PS_DRIVER(b), 0x130)

/* a float through the x87 (fld m32 / fstp m32): a signalling NaN comes out quiet */
static float ps_x87(float v) {
    uint32_t b; memcpy(&b, &v, 4);
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu)) b |= 0x00400000u;
    memcpy(&v, &b, 4);
    return v;
}
static void ps_x87_copy(rx_addr dst, rx_addr src) { SF32(dst, 0, ps_x87(F32(src, 0))); }

/* vehicle vf45 (slot 0xb4, thiscall, no arguments): nonzero while ... (see BehaviorAirLeveling) */
static uint32_t ps_vehicle_vf45(rx_addr vehicle) {
    const rx_addr fn = race_vfn(vehicle, 0xb4 / 4);
    uint32_t (*f)(rx_addr) = RX_FN(fn, uint32_t (*)(rx_addr));
    return f ? f(vehicle) : race_thiscall(fn, vehicle, 0, 0, 0, 0, 0);
}

/* ---- 0078fcf0 (EAX = haystack, EDI = needle): the first position of the needle in the haystack, comparing through the CRT's
 * lower-case table (0x9b77c8) as the C library's case-insensitive search does; 0 when absent (or the haystack is empty). The
 * haystack characters index the table signed for the first character, unsigned for the rest, as the original's movsx / movzx. ---- */
rx_addr rx_str_find_nocase(rx_addr haystack, rx_addr needle) {
    const int32_t first = (int8_t)U8(0, 0x009b77c8 + U8(needle, 0));
    rx_addr p = haystack;
    uint8_t c = U8(p, 0);
    if (c == 0) return 0;
    do {
        const uint32_t lower = U8(0, 0x009b77c8 + (uint32_t)(int32_t)(int8_t)c);
        if ((int32_t)lower == first) {
            rx_addr n = needle + 1;
            for (;;) {
                const uint8_t nc = U8(n, 0);
                if (nc == 0) return p;
                const uint8_t hc = U8(p + 1 + (n - (needle + 1)), 0);
                if (U8(0, 0x009b77c8 + hc) != U8(0, 0x009b77c8 + nc)) break;
                n++;
            }
        }
        c = U8(p, 1);
        p++;
    } while (c != 0);
    return 0;
}
RX_FUNCTION(0x0078fcf0, rx_str_find_nocase)

/* ---- 0051dff0 / 0051e040 (EAX = the driver) -> xmm0: a tuning float of the AI tables (0x00a8a460 .., 0xc7c bytes per difficulty
 * +0xee8): the player's tuning row +0x8fc when set (-1: none), else the entry of tuning set +0x8f4 * 3 + column +0x8f8. The first
 * (a) is the weight of the stunt kinds, the second (b) whether another stunt follows. ---- */
float rx_driver_stunt_tuning_a(rx_addr driver) {
    const rx_addr player = U32(driver, 0x74);
    const uint32_t row = U32(player, 0x8fc);
    if (row != 0xffffffffu) return F32(0, 0x00a8a4c0 + U32(driver, 0xee8) * 0xc7c + row * 8);
    const uint32_t set = U32(player, 0x8f4) * 3u + U32(player, 0x8f8);
    return F32(0, 0x00a8a460 + U32(driver, 0xee8) * 0xc7c + set * 8);
}
RX_FUNCTION(0x0051dff0, rx_driver_stunt_tuning_a)
float rx_driver_stunt_tuning_b(rx_addr driver) {
    const rx_addr player = U32(driver, 0x74);
    const uint32_t row = U32(player, 0x8fc);
    if (row != 0xffffffffu) return F32(0, 0x00a8a57c + U32(driver, 0xee8) * 0xc7c + row * 8);
    const uint32_t set = U32(player, 0x8f4) * 3u + U32(player, 0x8f8);
    return F32(0, 0x00a8a51c + U32(driver, 0xee8) * 0xc7c + set * 8);
}
RX_FUNCTION(0x0051e040, rx_driver_stunt_tuning_b)

/* ---- 0052b0a0 (ESI = the behaviour; stack budget (float), flag; RET 8) -> xmm0 = the budget left: plan up to three stunts.
 * A random draw below the tuning's cap (0051dff0, at least 1) picks the stunt table (0xd5aa80 / 0xd5aef8 / 0xd5af0c for 1 / 2 / 3,
 * else 0xd5aa94); then up to 20 random draws from it, each skipped when the stunt is hidden (+0x68) or of another kind than the
 * flag (+0x64) (these two test the end of the plan first), is a ground stunt (flag 0) with +0x98 set, a "taunt", or has no
 * performance asset for the vehicle class (006cd050); one whose clip fits the budget (asset +0x24 > ...) is appended to the plan
 * (+0x60 + 16 * count: record, clip length, hold time (006cd1c0), asset +0x28) and its length taken off the budget; planning ends
 * when the budget is below 1 or three stunts are planned. Frame: sub esp, 0x10; push ebx / ebp / edi. ---- */
float rx_perform_stunt_plan(rx_addr beh, float budget, uint32_t flag) {
    RACE_FRAME_PLAIN(F, 0x10, 3);
    int32_t cap = rx_cvtt(rx_driver_stunt_tuning_a(PS_DRIVER(beh)));
    if (cap < 1) cap = 1;
    const uint32_t kind = race_rand() % (uint32_t)cap;
    rx_addr table;
    switch (kind) {
    case 1: table = 0x00d5aa80; break;
    case 2: table = 0x00d5aef8; break;
    case 3: table = 0x00d5af0c; break;
    default: table = 0x00d5aa94; break;
    }
    for (uint32_t k = 0; k < 0x14; k++) {
        const uint32_t pick = race_rand() % U32(table, 4);
        const rx_addr stunt = U32(U32(table, 0), 4 * pick);
        if (U32(stunt, 0x68) != 0 || U32(stunt, 0x64) != flag) {
            if (1.0f > budget || U32(beh, 0x94) == 3) break;
            continue;
        }
        if (flag == 0 && U32(stunt, 0x98) != 0) continue;
        if (rx_str_find_nocase(stunt, 0x0093f5f8 /* "taunt" */) != 0) continue;
        const uint32_t klass = U32(PS_VEHICLE(beh), 0x194);
        uint32_t has;
        RACE_CORE_CALL(1, has = rx_fx_trick_perf_exists(stunt, klass));
        if (!has) continue;
        rx_addr asset;
        RACE_CORE_CALL(1, asset = rx_fx_trick_asset_handle(stunt, klass));
        SU32(asset, 0x4c, 1);
        const float length = F32(U32(asset, 0x44), 0x24);
        SU32(asset, 0x4c, 0);
        if (budget > length) {
            rx_addr asset2;
            RACE_CORE_CALL(1, asset2 = rx_fx_trick_asset_handle(stunt, klass));
            SU32(asset2, 0x4c, 1);
            const uint32_t n = U32(beh, 0x94);
            const rx_addr entry = beh + 0x60 + 16 * n;
            SU32(entry, 0, stunt);
            SF32(entry, 4, length);
            const float hold = rx_fx_trick_hold_time(stunt, klass);
            SF32(entry, 8, hold);
            SF32(entry, 0xc, ps_x87(F32(U32(asset2, 0x44), 0x28)));
            SU32(beh, 0x94, n + 1);
            budget = budget - length;
            SU32(asset2, 0x4c, 0);
        }
        if (1.0f > budget || U32(beh, 0x94) == 3) break;
    }
    RACE_FRAME_END(F);
    return budget;
}
RX_FUNCTION(0x0052b0a0, rx_perform_stunt_plan)

/* ---- 0052ae10 (ESI = the behaviour) state 0: the player's current stunt (player +0xb7c) is cleared; a wrecked vehicle (+0x280)
 * clears the wait counter (+0x50) and stops. Airborne (vf43) with the take-off machine idle (+0x44 clear): when the tuning
 * (0051e040) is positive, or the player's +0xf40 is not above 0, the wait counter restarts and the state becomes 2 (predict the
 * landing). On the ground with the other machine idle (+0x48 clear) and fewer than two ticks (+0x50): the same test, and unless
 * the player's tuning set (+0x8f4) is 3 the state becomes 1 (plan a ground stunt). Frame: push edi (in the branches). ---- */
void rx_perform_stunt_idle(rx_addr beh) {
    const rx_addr player = PS_PLAYER(beh);
    SU32(player, 0xb7c, 0);
    const rx_addr vehicle = U32(PS_PLAYER(beh), 0x1ac);
    if (U32(vehicle, 0x280) != 0) { SU32(beh, 0x50, 0); return; }
    const rx_addr entry0 = rx_sp();
    rx_set_sp(entry0 - 4);                                       /* push edi */
    if (beh2_vehicle_flag43(vehicle) != 0) {
        if (U32(beh, 0x44) == 0) {
            const rx_addr driver = PS_DRIVER(beh);
            const float tuning = rx_driver_stunt_tuning_b(driver);
            if (tuning > 0.0f || !(0.0f < F32(U32(driver, 0x74), 0xf40))) {
                SU32(beh, 0x50, 0);
                SU32(beh, 0xc, 2);
            }
        }
    } else if (U32(beh, 0x48) == 0 && U32(beh, 0x50) < 2) {
        const rx_addr driver = PS_DRIVER(beh);
        const float tuning = rx_driver_stunt_tuning_b(driver);
        if (tuning > 0.0f || !(0.0f < F32(U32(driver, 0x74), 0xf40))) {
            if (U32(U32(driver, 0x74), 0x8f4) != 3) SU32(beh, 0xc, 1);
        }
    }
    rx_set_sp(entry0);
}
RX_FUNCTION(0x0052ae10, rx_perform_stunt_idle)

/* ---- 0052aed0 (EAX = the behaviour) state 1: the plan is cleared (index, count, the spacing / times +0x98 .. +0xa0) and made
 * from the long budget (3.0, 0x92fe48) for ground stunts (flag 0, 0052b0a0); the airborne time +0x40 restarts at that budget.
 * Stunts planned: spacing 0.1 (0x931d94), state 3, kind 2; none planned: state 0 when the vehicle is airborne (vf43). The driver clock
 * is added to +0x40. Frame: push esi. ---- */
void rx_perform_stunt_plan_ground(rx_addr beh) {
    RACE_FRAME_PLAIN(F, 0, 1);
    SF32(beh, 0x98, 0.0f);
    SF32(beh, 0x9c, 0.0f);
    SF32(beh, 0xa0, 0.0f);
    const float budget = ps_x87(3.0f /* 0x92fe48 */);
    SU32(beh, 0x94, 0);
    SU32(beh, 0x90, 0);
    SF32(beh, 0x40, 3.0f);
    SU32(F, -4, 0);
    SU32(F, -8, rx_fb(budget));
    RACE_CORE_CALL(2, (void)rx_perform_stunt_plan(beh, budget, 0));
    if (U32(beh, 0x94) != 0) {
        SF32(beh, 0x98, 0.1f /* 0x931d94 */);
        SU32(beh, 0xc, 3);
        SU32(beh, 0x5c, 2);
    } else if (beh2_vehicle_flag43(PS_VEHICLE(beh)) != 0) {
        SU32(beh, 0xc, 0);
    }
    SF32(beh, 0x40, PS_CLOCK(beh) + F32(beh, 0x40));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0052aed0, rx_perform_stunt_plan_ground)

/* ---- 0052af90 (EAX = the behaviour) state 2: the landing prediction (00461980: time to land into +0x40, with a point and a
 * normal on the frame); once the time to land reaches the minimum (+0x54, not above it) a plan is made from the air (flag 1,
 * budget = 0.75 (0x935aa8) of the time to land): stunts planned: the spacing +0x98 = the budget left / the count (x87), state 3,
 * kind 1; none: state 0 when the vehicle is not airborne (vf43); and the driver clock is added to +0x40. Frame: push ebp; and esp,
 * -16; sub esp, 0x28; push esi / edi (the prediction's normal at +0x10, point at +0x20). ---- */
void rx_perform_stunt_plan_air(rx_addr beh) {
    RACE_FRAME_EBP(F, 16, 0x28, 2);
    const rx_addr time_out = beh + 0x40;
    SU32(beh, 0x94, 0);
    SU32(beh, 0x90, 0);
    SF32(beh, 0x98, 0.0f);
    SF32(beh, 0x9c, 0.0f);
    SF32(beh, 0xa0, 0.0f);
    SF32(beh, 0x40, 0.0f);
    uint32_t landing;
    RACE_CORE_CALL(2, landing = rx_landing_predict(time_out, PS_VEHICLE(beh), F + 0x20, F + 0x10));
    if (landing != 0) {
        float t = F32(beh, 0x40);
        if (!(F32(beh, 0x54) > t)) {
            const float budget = t * 0.75f /* 0x935aa8 */;
            SF32(beh, 0x40, budget);
            float left;
            RACE_CORE_CALL(2, left = rx_perform_stunt_plan(beh, ps_x87(budget), 1));
            const uint32_t count = U32(beh, 0x94);
            if (count != 0) {
                SU32(beh, 0xc, 3);
                SU32(beh, 0x5c, 1);
                SF32(beh, 0x98, (float)((double)left / (double)count));
            } else if (beh2_vehicle_flag43(PS_VEHICLE(beh)) == 0) {
                SU32(beh, 0xc, 0);
            }
            SF32(beh, 0x40, PS_CLOCK(beh) + F32(beh, 0x40));
        }
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0052af90, rx_perform_stunt_plan_air)

/* ---- 0052b2f0 (EAX = the behaviour) state 3: the take-off wait. Kind 2 (a ground start): after two ticks (+0x50) back to state 0,
 * else the tick counts; otherwise the countdown (+0x58, when not -1) steps down (clamped at 0) and at 0 the machines are re-armed
 * (+0x44 = 1, +0x54 = 0, +0x58 = -1). The player's current stunt (+0xb7c) becomes the planned stunt of the index (+0x90); the
 * time it ends (+0x9c) = its start offset (+0x68) + the driver clock + the spacing (+0x98); state 4. ---- */
void rx_perform_stunt_wait(rx_addr beh) {
    if (U32(beh, 0x5c) == 2) {
        const uint32_t ticks = U32(beh, 0x50);
        if (ticks >= 2) { SU32(beh, 0xc, 0); return; }
        SU32(beh, 0x50, ticks + 1);
    } else {
        int32_t count = I32(beh, 0x58);
        if (count > -1) {
            count--;
            if (count < 0) count = 0;
            SU32(beh, 0x58, (uint32_t)count);
            if (count == 0) {
                SU32(beh, 0x44, 1);
                SF32(beh, 0x54, 0.0f);
                SU32(beh, 0x58, 0xffffffffu);
            }
        }
    }
    const uint32_t index = U32(beh, 0x90);
    SU32(PS_PLAYER(beh), 0xb7c, U32(beh, 0x60 + 16 * index));
    SF32(beh, 0x9c, (F32(beh, 0x68 + 16 * index) + PS_CLOCK(beh)) + F32(beh, 0x98));
    SU32(beh, 0xc, 4);
}
RX_FUNCTION(0x0052b2f0, rx_perform_stunt_wait)

/* ---- 0052b390 (ESI = the behaviour) state 4: a wrecked vehicle (+0x280) ends the plan (state 0); for kind 1 (from the air) so does
 * a vehicle that is no longer airborne (vf43 clear); once the clock passes the end time (+0x9c) the player's stunt is cleared and
 * the next starts at the clock + the stunt's hold time (the plan entry +0x6c) (+0xa0), state 5. ---- */
void rx_perform_stunt_run(rx_addr beh) {
    if (U32(PS_VEHICLE(beh), 0x280) != 0) SU32(beh, 0xc, 0);
    if (U32(beh, 0x5c) == 1) {
        if (beh2_vehicle_flag43(PS_VEHICLE(beh)) == 0) SU32(beh, 0xc, 0);
    }
    const float clock = PS_CLOCK(beh);
    if (clock > F32(beh, 0x9c)) {
        SU32(PS_PLAYER(beh), 0xb7c, 0);
        SF32(beh, 0xa0, F32(beh, 0x6c + 16 * U32(beh, 0x90)) + clock);
        SU32(beh, 0xc, 5);
    }
}
RX_FUNCTION(0x0052b390, rx_perform_stunt_run)

/* ---- 0052b410 (ESI = the behaviour) state 5: once the clock passes the next-start time (+0xa0) the stunt index advances; another
 * stunt follows (state 3) when the index is below the count and the tuning (0051e040) is positive, else back to state 0. ---- */
void rx_perform_stunt_next(rx_addr beh) {
    if (PS_CLOCK(beh) > F32(beh, 0xa0)) {
        SU32(beh, 0x90, U32(beh, 0x90) + 1);
        const uint32_t index = U32(beh, 0x90);
        const uint32_t more = rx_driver_stunt_tuning_b(PS_DRIVER(beh)) > 0.0f;
        if (index < U32(beh, 0x94) && more) SU32(beh, 0xc, 3);
        else SU32(beh, 0xc, 0);
    }
}
RX_FUNCTION(0x0052b410, rx_perform_stunt_next)

/* ---- 0052ab80 (ESI = the behaviour) mode 0: the take-off test of a rider on the ground (while either stunt machine is idle):
 * the vehicle's matrix row (vf14: x, y, z) must be level (|(z + x) * 0 + y| not above 0.1), the vehicle not in vf45 / vf43 states;
 * a coin flip (the game's LCG, bit 16) picks the side (+0x24 = 1 / 2): side 2 (a hard push): speed above 83 and |vy| above 22:
 * the air-leveling behaviour is disabled, the timer (+0x20) = the clock + 1, the position (+0x30..) saved, mode 2, and the driver's
 * +0x60 = FLT_MAX; side 1 (a ramp): speed above 77 and |vy| above 10: the air-leveling behaviour disabled, the timer 0 (the clock
 * + 3 when |vy| is above 24.95), mode 1. Frame: push ebp; and esp, -16; sub esp, 0xc; push edi. ---- */
void rx_perform_stunt_takeoff_test(rx_addr beh) {
    RACE_FRAME_EBP(F, 16, 0xc, 1);
    if (U32(beh, 0x44) != 0 && U32(beh, 0x48) != 0) goto done;
    {
        rx_addr veh = PS_VEHICLE(beh);
        const rx_addr row = beh2_vehicle_matrix(veh);
        const float sum = ((F32(row, 8) + F32(row, 0)) * 0.0f /* 0x92e12c */) + F32(row, 4);
        if (fabsf(sum) > 0.1f /* 0x931d94 */) goto done;
        veh = PS_VEHICLE(beh);
        if (ps_vehicle_vf45(veh) != 0) goto done;
        if (beh2_vehicle_flag43(PS_VEHICLE(beh)) != 0) goto done;
        /* the game's LCG (seed 0x9cb860, count 0x9cb864); the coin is bit 16 of the new seed itself */
        SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
        const uint32_t seed = U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u;
        SU32(0, 0x009cb860, seed);
        const uint32_t side = (seed & 0x10000) ? 1u : 2u;
        SU32(beh, 0x24, side);
        veh = PS_VEHICLE(beh);
        const float vx = F32(veh, 0x160), vy = F32(veh, 0x164), vz = F32(veh, 0x168);
        float speed;
        RACE_CORE_CALL(1, speed = phys_sqrtf((vx * vx + vy * vy) + vz * vz));
        if (side == 2) {
            if (!(speed > 83.0f /* 0x9cd9c8 */)) goto done;
            if (!(fabsf(vy) > 22.0f /* 0x9cd9cc */)) goto done;
            if (U32(beh, 0xa4) != 0) beh2_behavior_enable(U32(beh, 0xa4), 0);
            SF32(beh, 0x20, PS_CLOCK(beh) + 1.0f /* 0x931f24 */);
            ps_x87_copy(beh + 0x30, veh + 0x130);
            ps_x87_copy(beh + 0x34, veh + 0x134);
            ps_x87_copy(beh + 0x38, veh + 0x138);
            SU32(beh, 0x10, 2);
            SF32(PS_DRIVER(beh), 0x60, 3.4028235e38f /* 0x977494 */);
        } else {
            if (!(speed > 77.0f /* 0x9cd9c0 */)) goto done;
            if (!(fabsf(vy) > 10.0f /* 0x92fd88 */)) goto done;
            if (U32(beh, 0xa4) != 0) beh2_behavior_enable(U32(beh, 0xa4), 0);
            SF32(beh, 0x20, 0.0f);
            if (fabsf(vy) > 24.95f /* 0x9cd9c4 */) SF32(beh, 0x20, PS_CLOCK(beh) + 3.0f /* 0x92fe48 */);
            SU32(beh, 0x10, 1);
        }
    }
done:
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0052ab80, rx_perform_stunt_takeoff_test)

/* ---- 0052aac0 BehaviorPerformStunt::vf4 (ECX = the behaviour; RET 4, the argument is unused) -> EAX 0: when enabled the state
 * function of the state (+0x0c: 0 idle, 1 plan on the ground, 2 plan from the air, 3 wait, 4 run, 5 next) runs, then the
 * take-off test of the mode (+0x10): 0 the test, 3 the vehicle's vf45 clearing the mode. Frame: push ebp; and esp, -8; push ecx / esi. ---- */
uint32_t rx_behavior_perform_stunt_update(rx_addr beh, float dt) {
    (void)dt;
    RACE_FRAME_EBP(F, 8, 0, 2);
    if (U32(beh, 4) != 0) {
        switch (U32(beh, 0xc)) {
        case 0: RACE_CORE_CALL(0, rx_perform_stunt_idle(beh)); break;
        case 1: RACE_CORE_CALL(0, rx_perform_stunt_plan_ground(beh)); break;
        case 2: RACE_CORE_CALL(0, rx_perform_stunt_plan_air(beh)); break;
        case 3: RACE_CORE_CALL(0, rx_perform_stunt_wait(beh)); break;
        case 4: RACE_CORE_CALL(0, rx_perform_stunt_run(beh)); break;
        case 5: RACE_CORE_CALL(0, rx_perform_stunt_next(beh)); break;
        default: break;
        }
        switch (U32(beh, 0x10)) {
        case 0: RACE_CORE_CALL(0, rx_perform_stunt_takeoff_test(beh)); break;
        case 3:
            if (ps_vehicle_vf45(PS_VEHICLE(beh)) != 0) SU32(beh, 0x10, 0);
            break;
        default: break;
        }
    }
    RACE_FRAME_END(F);
    return 0;
}
RX_FUNCTION(0x0052aac0, rx_behavior_perform_stunt_update)
