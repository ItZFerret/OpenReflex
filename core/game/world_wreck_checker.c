/* world_wreck_checker.c -- the WreckChecker (0x00632aa0-0x00638150): crash detection per rider from the bike's orientation, the
 * rates of its angles and the impact forces, choosing a wreck type and handing it to the rider (0054e8f0); the ramped "wreck
 * state" values of the rider (0x006367b0 / 0x00636870 / 0x00632aa0).
 *
 * Checker: +4 the player record (+0x5bab8 the control mode: 0 / 1 = riding, +0x5b540 the scripted wreck), +8 the vehicle
 * (vfuncs: +0x54 active, +0x94 / +0x9c vectors, +0xac / +0xb0 states, +0xe0 / +0xe4 angle rates (x87 results); +0x190 the rider,
 * +0x234 the impact block: +0x14 hit, +0x18 crash), +0x10 the thresholds (+0 / +4 speed curve, +0x10 / +0x14 impact levels,
 * +0x18.. 18 angle limits), +0x14 the bail timer, +0x18 the hop flag, +0x1c a pending wreck type, +0x20 its direction,
 * +0x24 the impact force, +0x28 the impact timer. The rider: +0x94 the controller, +0x1f0 the player index, +0xb34 the current
 * wreck type, +0xb38 its stamina. */
#include "phys_body.h"
#include "phys_misc.h"
#include "race_activity.h"
#include "world_wreck.h"
#include "world_util.h"
#include "world_veh.h"
#include "world_rider.h"
#include "world_misc.h"
#include "loop_player.h"
#include "vehicle_atv_leaves.h"
#include "vehicle_atv_telemetry.h"

/* 00637ee0 WreckChecker_TypeClass (ECX = wreck type - 1, 0..0x22; RET 0; EAX): the rider's reaction class of a wreck type (the
 * original's jump table 0x637f50). */
uint32_t rx_world_wreck_checker_type_class(uint32_t index) {
    static const uint8_t cls[35] = {1, 1, 1, 1, 4, 7, 6, 5, 1, 1, 7, 4, 5, 6, 5, 6, 5, 6, 7, 4, 1, 1, 1,
                                    9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14};
    return cls[index];
}
RX_FUNCTION(0x00637ee0, rx_world_wreck_checker_type_class)

/* 00637ff0 WreckChecker_ClassAnim (ECX = class - 1, 0..0x13, EAX = {int anim, float weight} out; RET 0; xmm0 = the weight): the
 * wreck animation of a class (jump table 0x6380fc); weight 1, or 1e-5 for the first three. */
float rx_world_wreck_checker_class_anim(uint32_t index, rx_addr out) {
    static const uint8_t anim[20] = {9, 9, 9, 3, 9, 0xc, 6, 6, 0xc, 9, 6, 3, 0xb, 8, 5, 2, 0xa, 7, 4, 1};
    const float weight = index < 3 ? 9.99999975e-06f /* 0x9774a4 */ : 1.0f /* 0x931f24 */;
    SU32(out, 0, anim[index]);
    SF32(out, 4, weight);
    return weight;
}
RX_FUNCTION(0x00637ff0, rx_world_wreck_checker_class_anim)

/* 00638150 WreckChecker_Excess (stdcall a, b, c (floats); RET 0xc; xmm0): how far a measure c is past its threshold a between a
 * and b, as a wreck weight: 1e-5 when |c| > |a|, else (a - c) / (a - b) * 0.25 + 0.75. */
float rx_world_wreck_checker_excess(float a, float b, float c) {
    float abs_c, abs_a;
    WORLD_CALL_SP(0, abs_c = rx_world_float_abs(c));
    WORLD_CALL_SP(0, abs_a = rx_world_float_abs(a));
    if (abs_c > abs_a) return 9.99999975e-06f /* 0x9774a4 */;
    return phys_add(phys_mul(phys_div(phys_sub(a, c), phys_sub(a, b)), 0.25f /* 0x92ff84 */), 0.75f /* 0x935aa8 */);
}
RX_FUNCTION(0x00638150, rx_world_wreck_checker_excess)

/* ---- the calls of the checker into the rider and the vehicle -------------------------------------------------------- */
static int scripted_mode(void) { return U32(0, 0x00a942b4) == 2; }   /* a replay / scripted session: no bails */
/* 00550650 (ESI = rider): the rider lets go (bail) */
static void rider_bail(rx_addr rider) { WORLD_CALL_SP(0, rx_player_respawn(rider)); }
/* 0054e8f0 (ECX = class, EAX = {anim, weight}, EDI = rider; stdcall wreck type; RET 4): the rider starts the wreck */
static void rider_start_wreck(rx_addr rider, uint32_t cls, rx_addr anim, uint32_t type) {
    WORLD_CALL_SP(4, rx_world_rider_start_wreck(rider, anim, cls, type));
}
/* 0054ed10 (EDI = rider, ESI = the wreck type, which it saves): the rider falls off */
static void rider_fall(rx_addr rider, uint32_t type) { WORLD_CALL_SP(0, rx_world_rider_fall(rider, type)); }
/* a vehicle vfunc returning an x87 float (thiscall, one int argument), stored with fstp by the caller.
 * BOTH slots the checker reads are st(0) returns in the original: 0xe0 = 00466b70 (the wheel value) and 0xe4 = 00466bd0
 * (the wheel grip factor). The core ports return them through C: the wheel value as a float (it also writes the original's
 * pushed argument slot as a dead store, so the call site hands it the same slot), the grip factor as an exact rx_f80, rounded
 * here exactly as the original's `fstp dword` does. Calling either through the wrong prototype is not a small inexactness: the
 * grip factor's result scales the wreck thresholds (P+0x1c / 0x24 by the front, 0x20 / 0x2c by the rear), and a garbage scale
 * (what a float-typed call of an x87/struct-returning callee delivers) makes every threshold zero -> a wreck type 0x13 / 0x14
 * for every rider at the green light. */
static float vehicle_rate(rx_addr veh, uint32_t slot, uint32_t arg, world_ctx c) {
    rx_regs o;
    c.ecx = veh;
    const rx_addr fn = U32(U32(veh, 0), slot);
#ifndef RX_ORACLE   /* the product: the owner's function with its own prototype */
    if (fn == 0x00466b70u) {                        /* the wheel value: (this, which, the argument slot) -> float */
        const rx_addr sp = rx_sp();
        rx_set_sp(sp - 8u);
        const float v = rx_world_raven_wheel_value(veh, arg, sp - 4u);
        rx_set_sp(sp);
        return v;
    }
    if (fn == 0x00466bd0u) {                        /* the grip factor: (this, wheel) -> st(0); fstp dword at the caller */
        const rx_addr sp = rx_sp();
        rx_set_sp(sp - 8u);
        const rx_f80 g = rx_world_raven_grip_factor(veh, arg);
        rx_set_sp(sp);
        return rx_f80_to_f32(g, 0);
    }
    if (fn == 0x004867b0u || fn == 0x00486850u) {
        /* ATV slots both return rx_f80, including wheel load. Preserve the
         * original argument/return frame and the caller's fstp dword rounding. */
        const rx_addr sp = rx_sp();
        rx_set_sp(sp - 8u);
        const rx_f80 value = fn == 0x004867b0u
            ? rx_atv_vehicle_wheel_load(veh, arg)
            : rx_atv_grip_factor(veh, arg);
        rx_set_sp(sp);
        return rx_f80_to_f32(value, 0);
    }
    /* Other classes retain the explicit original-call boundary: registration
     * alone cannot tell us whether a C return is float or rx_f80. */
#endif
    WORLD_CTX(fn, &c, &o, arg);
    return o.st0;
}

/* 006373e0 WreckChecker_Update (stdcall checker, dt; RET 8): the wreck check of one rider.
 * Nothing unless the checker has a player and a vehicle, the session is not scripted, and the rider's controller is active.
 * The impact timer (+0x28) counts down; while it is out a new hit adds dt to the impact force (+0x24), which otherwise decays by
 * 0.75 dt. A riding player (mode 0 / 1) hit hard (force > 0.25) gets the pending type 0x17 (stuck), else a force over 1 bails
 * the rider; either restarts the timer at 4 and clears the force. A crash flag does the same without the force test. A player
 * whose controller hops (00548b70) toggles the hop flag and, on a fresh hop, bails or marks 0x17 the same way.
 * Not riding: only a pending 0x16 bails. Riding: the vehicle's orientation (+0x9c), up vector (+0x94) and angle rates (+0xe0 /
 * +0xe4) against the 18 thresholds scaled by the speed curve (00424370) choose the wreck type and its weight (00638150): pitch
 * over / under (0xb / 0xc), roll left / right (0xf / 0x10 + the vehicle's state +0xb0), the yaw rate (mode 1: 0xd / 0xe), the
 * pitch rate while upside down (0x11..0x14, CRT rand), the roll rates (0x16 / 0x14 + ...), a scripted wreck (00548960), a stall
 * (a zero rate: 0x13). The class (00637ee0), animation and weight (00637ff0) scaled by the impact go to the rider (0054e8f0); a
 * negligible weight or no stamina left (+0xb38) starts the wreck at once, else the bail timer drains the stamina. */
void rx_world_wreck_checker_update(rx_addr checker, float dt) {
    world_frame f = world_frame_enter(0xc4, 3, 1);
    const rx_addr P = f.esp, dt_slot = f.entry + 8;
    (void)dt_slot;
    if (U32(checker, 4) == 0) goto done;
    rx_addr veh = U32(checker, 8);
    if (veh == 0 || scripted_mode()) goto done;
    {
        const rx_addr rider = U32(veh, 0x190);
        if (U32(rider, 0x94) == 0 || U32(rider, 4) == 0) goto done;
        const rx_addr ctl = U32(rider, 0x94);
        if (WORLD_VCALL0(ctl, 0x54) == 0)
            goto done;
    }
    /* the impact timer and force */
    float force_zero = rx_bf(0);
    {
        const uint32_t hit = U32(U32(veh, 0x234), 0x14);
        float t = phys_sub(F32(checker, 0x28), dt);
        SF32(checker, 0x28, t);
        if (rx_bf(0) > t) t = rx_bf(0);
        SF32(checker, 0x28, t);
        float force;
        if (hit != 0 && rx_bf(0) >= t) force = phys_add(dt, F32(checker, 0x24));
        else {
            force = phys_sub(F32(checker, 0x24), phys_mul(dt, 0.75f /* 0x935aa8 */));
            SF32(checker, 0x24, force);
            if (rx_bf(0) > force) force = rx_bf(0);
        }
        SF32(checker, 0x24, force);
        if (hit != 0) {
            const uint32_t mode = U32(U32(checker, 4), 0x5bab8);
            int restart = 0;
            if ((mode == 1 || mode == 0) && F32(checker, 0x24) > 0.25f /* 0x92ff84 */) {
                if (U32(checker, 0x1c) == 0) SU32(checker, 0x1c, 0x17);
                restart = 1;
            }
            if (!restart && F32(checker, 0x24) > 1.0f /* 0x931f24 */) {
                if (!scripted_mode()) rider_bail(U32(veh, 0x190));
                restart = 1;
            }
            if (restart) { SF32(checker, 0x28, 4.0f /* 0x9322c0 */); SF32(checker, 0x24, force_zero); }
        }
    }
    veh = U32(checker, 8);
    if (U32(U32(veh, 0x234), 0x18) != 0) {
        const uint32_t mode = U32(U32(checker, 4), 0x5bab8);
        if (mode == 1 || mode == 0) { if (U32(checker, 0x1c) == 0) SU32(checker, 0x1c, 0x17); }
        else if (!scripted_mode()) rider_bail(U32(veh, 0x190));
    }
    {
        const rx_addr rider = U32(U32(checker, 8), 0x190);
        const uint32_t player = U32(rider, 0x1f0);
        if ((int32_t)player >= 0) {
            const rx_addr ctl = U32(rider, 0x94);
            if (ctl != 0 && U32(ctl, 0x21c) == 2) {
                uint32_t hop;
                WORLD_CALL_SP(0, hop = rx_world_pad_reset_combo(player) & 0xff);   /* 00548b70 */
                if (hop) {
                    if (U32(checker, 0x18) != 0) SU32(checker, 0x18, 0);
                    else {
                        const rx_addr game = U32(0, 0x00a93fcc);
                        SU32(checker, 0x18, 1);
                        if (U32(game, 0x298) == 0) {
                            const rx_addr v = U32(checker, 8), r = U32(v, 0x190);
                            if (U32(U32(r, 0x94), 0xfe8) == 0 && U32(game, 0x288) == 0) {
                                const uint32_t state = U32(v, 0x194);
                                if (state == 0 || state == 1) { if (U32(checker, 0x1c) == 0) SU32(checker, 0x1c, 0x17); }
                                else if (!scripted_mode()) rider_bail(r);
                            }
                        }
                    }
                } else if (U32(checker, 0x18) != 0) SU32(checker, 0x18, 0);
            }
        }
    }
    {
        const uint32_t mode = U32(U32(checker, 4), 0x5bab8);
        if (mode != 1 && mode != 0) {
            if (U32(checker, 0x1c) == 0x16 && !scripted_mode()) rider_bail(U32(U32(checker, 8), 0x190));
            goto done;
        }
    }
    /* the orientation, up vector and angle rates; the thresholds scaled by the speed curve */
    veh = U32(checker, 8);
    WORLD_VCALL(veh, 0x9c, P + 0xa0);
    WORLD_VCALL(veh, 0x94, P + 0xb0);
    SU32(P, 0x34, world_x87_pass(U32(P, 0xb4)));     /* the speed */
    SU32(P, 0x80, U32(P, 0xa0)); SU32(P, 0x84, U32(P, 0xa4)); SU32(P, 0x88, U32(P, 0xa8));   /* pitch, roll, yaw */
    const world_ctx at = WCTX(.ebx = checker, .edi = 0x17);
    SF32(P, 0x48, vehicle_rate(veh, 0xe0, 2, WCTX(.edx = U32(veh, 0), .eax = U32(U32(veh, 0), 0xe0), .ebx = checker, .edi = 0x17)));
    SF32(P, 0x28, vehicle_rate(veh, 0xe0, 0, WCTX(.edx = U32(veh, 0), .eax = U32(U32(veh, 0), 0xe0), .ebx = checker, .edi = 0x17)));
    SF32(P, 0x58, vehicle_rate(veh, 0xe0, 1, WCTX(.edx = U32(veh, 0), .eax = U32(U32(veh, 0), 0xe0), .ebx = checker, .edi = 0x17)));
    (void)at;
    WORLD_VCALL(veh, 0x94, P + 0x90);
    const rx_addr th = U32(checker, 0x10);
    float speed_scale;
    WORLD_CALL_SP(12, speed_scale = phys_remap_clamped(rx_bf(U32(th, 0)), rx_bf(U32(th, 4)), rx_bf(world_x87_pass(U32(P, 0x34))),
                                                       rx_bf(world_x87_pass(U32(th, 0xc))), rx_bf(world_x87_pass(U32(th, 8)))));
    {
        static const uint8_t dst[18] = {0x54, 0x64, 0x50, 0x6c, 0x60, 0x70, 0x4c, 0x7c, 0x5c, 0x78, 0x44, 0x68, 0x40, 0x30, 0x1c, 0x24, 0x20, 0x2c};
        for (uint32_t i = 0; i < 18; i++) SF32(P, dst[i], phys_mul(F32(th, 0x18 + 4 * i), speed_scale));
    }
    veh = U32(checker, 8);
    SF32(P, 0x18, vehicle_rate(veh, 0xe4, 0, WCTX(.eax = U32(veh, 0), .edx = U32(U32(veh, 0), 0xe4), .ebx = checker, .esi = th, .edi = 0x17)));
    SF32(P, 0x74, vehicle_rate(veh, 0xe4, 1, WCTX(.eax = U32(veh, 0), .edx = U32(U32(veh, 0), 0xe4), .ebx = checker, .esi = th, .edi = 0x17)));
    {
        const float r0 = F32(P, 0x18), r1 = F32(P, 0x74);
        SF32(P, 0x24, phys_mul(r0, F32(P, 0x24)));
        SF32(P, 0x1c, phys_mul(r0, F32(P, 0x1c)));
        SF32(P, 0x20, phys_mul(r1, F32(P, 0x20)));
        SF32(P, 0x2c, phys_mul(r1, F32(P, 0x2c)));
        SF32(P, 0x18, 1.0f /* 0x931f24 */);
    }
    /* choose the wreck type and its impact scale */
    uint32_t type = 0;
    float impact = 1.0f;
    const uint32_t pending = U32(checker, 0x1c);
#define EXCESS(a, b, c) WORLD_CALL_SP(0xc, impact = rx_world_wreck_checker_excess(F32(P, a), F32(P, b), F32(P, c)))
    /* the up vector's y below 5 (all three lanes of the splat): the bike lies on its side / back */
#define UPSIDE_DOWN() (v4_st(P + 0x80, v4_cmplt(v4_lane(v4_ld(P + 0x90), 1), v4_splat(5.0f /* 0x935890 */))), \
                       v4_all3(v4_ld(P + 0x80)))
    if (pending != 0) {
        type = pending;
        const int32_t dir = I32(checker, 0x20);
        SU32(checker, 0x1c, 0);
        if (dir < 0) impact = 0.01f /* 0x9322ac */;
        else if (dir == 0) impact = phys_add(F32(th, 0x10), 0.001f /* 0x97743c */);
        else if (dir == 1) impact = phys_add(F32(th, 0x14), 0.001f);
        else impact = phys_sub(F32(th, 0x14), 0.001f);
        SU32(checker, 0x20, 0xffffffffu);
        goto chosen;
    }
    {
        const float pitch = F32(P, 0x80);
        if (pitch > F32(P, 0x50)) { type = 0xb; EXCESS(0x6c, 0x50, 0x80); goto chosen; }
        if (F32(P, 0x54) > pitch) { type = 0xc; EXCESS(0x64, 0x54, 0x80); goto chosen; }
        const float roll = F32(P, 0x84);
        const world_ctx vc = WCTX(.ebx = checker, .esi = th, .edi = 0x17);
        if (F32(P, 0x60) > roll) {
            world_ctx c = vc; c.ecx = veh; c.eax = U32(veh, 0); c.edx = U32(U32(veh, 0), 0xb0);
            const uint32_t s = WORLD_VCALL0(veh, 0xb0);
            if (s != 0) goto rated;
            type = s + 0xf; EXCESS(0x70, 0x60, 0x84); goto chosen;
        }
        if (roll > F32(P, 0x4c)) {
            world_ctx c = vc; c.ecx = veh; c.eax = U32(veh, 0); c.edx = U32(U32(veh, 0), 0xb0);
            const uint32_t s = WORLD_VCALL0(veh, 0xb0);
            if (s != 0) goto rated;
            type = s + 0x10; EXCESS(0x7c, 0x4c, 0x84); goto chosen;
        }
        if (U32(U32(checker, 4), 0x5bab8) == 1) {
            if (F32(P, 0x5c) > F32(P, 0x88)) { type = 0xd; EXCESS(0x78, 0x5c, 0x88); goto chosen; }
            if (F32(P, 0x88) > F32(P, 0x44)) { type = 0xe; EXCESS(0x68, 0x44, 0x88); goto chosen; }
        }
        if (F32(P, 0x48) > F32(P, 0x40)) {
            world_ctx c = vc; c.ecx = veh; c.edx = U32(veh, 0); c.eax = U32(U32(veh, 0), 0xac);
            if (WORLD_VCALL0(veh, 0xac) == 0 && UPSIDE_DOWN()) {
                /* CRT rand(): 0x9cb860 seed, 0x9cb864 call count */
                uint32_t seed = U32(0, 0x009cb860);
                SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
                seed = seed * 0x41c64e6du + 0x3039u;
                SU32(0, 0x009cb860, seed);
                const uint32_t pick = (((seed >> 16) & 0x7fff) & 0x80000003u) - 1;
                if (pick <= 3) type = 0x11 + pick;
                EXCESS(0x30, 0x40, 0x48);
                goto chosen;
            }
        }
        if (F32(P, 0x28) > F32(P, 0x1c)) {
            world_ctx c = vc; c.ecx = veh; c.eax = U32(veh, 0); c.edx = U32(U32(veh, 0), 0xac);
            const uint32_t s = WORLD_VCALL0(veh, 0xac);
            if (s == 0 && UPSIDE_DOWN()) { type = 0x13; EXCESS(0x24, 0x1c, 0x28); goto chosen;   /* the original adds 0x16 to the mask sum -3 */ }
        }
        if (F32(P, 0x58) > F32(P, 0x20)) {
            world_ctx c = vc; c.ecx = veh; c.edx = U32(veh, 0); c.eax = U32(U32(veh, 0), 0xac);
            const uint32_t s = WORLD_VCALL0(veh, 0xac);
            if (s == 0 && UPSIDE_DOWN()) { type = s + 0x14; EXCESS(0x2c, 0x20, 0x58); goto chosen; }
        }
        if (U32(0, 0x00af6c00) == 0) goto chosen;
        const int32_t scripted = I32(U32(checker, 4), 0x5b540);
        if (scripted <= -1) goto chosen;
        WORLD_CALL_SP(0, type = rx_player_scripted_wreck_type((uint32_t)scripted));   /* 00548960 (ESI = the slot) */
    }
rated:
    impact = F32(P, 0x18);
chosen:
#undef UPSIDE_DOWN
    if (F32(P, 0x1c) == 0.0f) {   /* a stalled pitch rate */
        type = 0x13;
        EXCESS(0x24, 0x1c, 0x28);
    } else if (type == 0) goto done;
#undef EXCESS
    {
        uint32_t cls = 1;
        if (type - 1 <= 0x22) cls = rx_world_wreck_checker_type_class(type - 1);
        if (cls - 1 <= 0x13) rx_world_wreck_checker_class_anim(cls - 1, P + 0x30);
        float weight = F32(P, 0x34);
        if (U32(0, 0x00af6c00) == 0) {
            weight = phys_mul(weight, impact);
            SF32(P, 0x34, weight);
            if (impact > F32(th, 0x10)) SU32(P, 0x30, U32(P, 0x30) - 2);
            else if (impact > F32(th, 0x14)) SU32(P, 0x30, U32(P, 0x30) - 1);
        }
        veh = U32(checker, 8);
        if (9.99999975e-06f /* 0x9774a4 */ >= weight) {
            rider_start_wreck(U32(veh, 0x190), cls, P + 0x30, type);
            rider_fall(U32(U32(checker, 8), 0x190), type);
            goto done;
        }
        const rx_addr rider = U32(veh, 0x190);
        if (rx_bf(0) >= F32(rider, 0xb38)) { rider_start_wreck(rider, cls, P + 0x30, type); goto done; }
        const float timer = phys_add(F32(checker, 0x14), dt);
        SF32(checker, 0x14, timer);
        if (type == U32(U32(veh, 0x190), 0xb34) || U32(0, 0x00af6c00) != 0) goto done;
        if (!(timer > 0.15f /* 0x9322c8 */)) goto done;
        SF32(checker, 0x14, rx_bf(0));
        const rx_addr r = U32(veh, 0x190);
        const float stamina = phys_sub(F32(r, 0xb38), weight);
        SF32(r, 0xb38, stamina);
        if (rx_bf(0) > stamina) rider_fall(r, type);
    }
done:
    world_frame_leave(&f);
}
RX_FUNCTION(0x006373e0, rx_world_wreck_checker_update)

/* 00637320 WreckChecker::vf0 (thiscall checker; stdcall dt; RET 4): the wreck check unless the rider is already down (vehicle
 * +0x280), then the bike's 00429910 on the player record +0x5d0. */
void rx_world_wreck_checker_vf0(rx_addr checker, float dt) {
    const rx_addr sp0 = rx_sp();
    const rx_addr sp = ((sp0 - 4) & ~7u) - 8;   /* push ebp; and esp, -8; push ecx; push esi */
    rx_set_sp(sp);
    const rx_addr veh = U32(checker, 8);
    if (veh != 0 && U32(veh, 0x280) == 0) WORLD_CALL_SP(8, rx_world_wreck_checker_update(checker, rx_bf(world_x87_pass(rx_fb(dt)))));
    const rx_addr player = U32(checker, 4);
    if (player != 0) WORLD_CALL_SP(0, phys_rig_bodies_from_bones(player + 0x5d0));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00637320, rx_world_wreck_checker_vf0)

/* ---- the rider's ramped wreck-state value (state: +0 mode (0 down, 1 up, 2 decay), +4 the previous mode, +0x14 decaying,
 * +0x18 value, +0x1c delta, +0x20 t, +0x24 duration, +0x28 the rider) ---------------------------------------------------- */

/* 006367b0 (ECX = state, xmm2 = target; RET 0): unless decaying, and when the rider's controller id (+0x580 -> word) matches
 * +0x5a0: delta = |target - value|; a non-zero delta starts a ramp down (value > target: mode 0) or up (mode 1) over
 * delta * 0.15, remembering the previous mode. */
void rx_world_wreck_state_ramp_to(rx_addr state, float target) {
    const uint32_t mode = U32(state, 0);
    if (mode == 2) return;
    const rx_addr r = U32(U32(state, 0x28), 0x1b4);
    const rx_addr idp = U32(r, 0x580);
    const uint16_t id = idp ? U16(idp, 0) : 0;
    if (U16(r, 0x5a0) != id) return;
    const float value = F32(state, 0x18);
    const float delta = rx_bf(rx_fb(phys_sub(target, value)) & 0x7fffffffu);   /* andps with 0x977570 */
    SF32(state, 0x1c, delta);
    if (delta == 0.0f) return;
    if (value > target) {
        SF32(state, 0x24, phys_mul(delta, 0.15f /* 0x9322c8 */)); SU32(state, 0x20, 0);
        if (mode != 0) SU32(state, 4, mode);
        SU32(state, 0, 0);
    } else if (target > value) {
        SF32(state, 0x24, phys_mul(delta, 0.15f)); SU32(state, 0x20, 0);
        if (mode != 1) SU32(state, 4, mode);
        SU32(state, 0, 1);
    }
}
RX_FUNCTION(0x006367b0, rx_world_wreck_state_ramp_to)

/* 00636870 (ESI = state; stdcall dt; RET 4; EAX): advances the ramp. Modes 0 / 1: t += dt (up to the duration), the value
 * moves by delta * t / duration (down to 0, or up to 1, ending when the rider's 00545340 says so: previous = 1, mode 0); mode 2
 * decays the value by 6 dt (when +0x14) and ends at 0 (previous = 2, mode 0). Nothing while the rider has no player
 * (+0x1f0 = -1). */
void rx_world_wreck_state_advance(rx_addr state, float dt) {
    const rx_addr rider = U32(state, 0x28);
    if (U32(rider, 0x1f0) == 0xffffffffu) return;
    const uint32_t mode = U32(state, 0);
    if (mode == 0 || mode == 1) {
        float t = phys_add(F32(state, 0x20), dt);
        const float duration = F32(state, 0x24);
        if (!(duration > t)) t = duration;
        SF32(state, 0x20, t);
        const float step = phys_mul(phys_div(t, duration), F32(state, 0x1c));
        if (mode == 0) {
            const float v = phys_sub(F32(state, 0x18), step);
            SF32(state, 0x18, v > rx_bf(0) ? v : rx_bf(0));
            return;
        }
        const float v = phys_add(F32(state, 0x18), step);
        SF32(state, 0x18, 1.0f /* 0x931f24 */ > v ? v : 1.0f);
        uint32_t cur;
        WORLD_CALL_SP(0, cur = rx_world_rider_pose_is_current_5a0(U32(rider, 0x1b4)));   /* 00545340 */
        if (cur == 0) { SU32(state, 4, 1); SU32(state, 0, 0); }
        return;
    }
    if (mode != 2) return;
    if (U32(state, 0x14) != 0) {
        float v = phys_sub(F32(state, 0x18), phys_mul(dt, 6.0f /* 0x92fe4c */));
        SF32(state, 0x18, v);
        if (!(v > rx_bf(0))) v = rx_bf(0);
        SF32(state, 0x18, v);
    }
    if (F32(state, 0x18) == 0.0f) { SU32(state, 4, 2); SU32(state, 0, 0); }
}
RX_FUNCTION(0x00636870, rx_world_wreck_state_advance)

/* 00632aa0 (ECX = state, ESI = rider; RET 0): in the ramp modes 1 / 2, the rider's IK chain group 3 (previous mode 1) or 2 fades
 * out over 0.25 s (006235e0) when it is enabled (00623960). Callers read no result. */
void rx_world_wreck_state_react(rx_addr state, rx_addr rider) {
    const uint32_t m1 = U32(state, 0) - 1;
    if (m1 > 1) return;
    const uint32_t k = U32(state, 4) == 1 ? 3 : 2;
    const uint32_t ok = rx_world_rider_chain_group(rider, k);   /* is chain group k there */
    if (ok == 0) return;
    WORLD_CALL_SP(8, rx_world_rider_chains_fade_out(rider, k, 0.25f /* 0x92ff84 */, 0));
}
RX_FUNCTION(0x00632aa0, rx_world_wreck_state_react)
