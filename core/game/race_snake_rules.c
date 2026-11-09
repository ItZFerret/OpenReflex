/* race_snake_rules.c -- RuleOutOfSnakeBounds (vtable 0x0093fe00, 0x60 bytes; kind 9), the boundary rule the Snake and Infection party
 * modes give every driver (their vf47), and the two activities' boundary tests it calls, hand-transliterated from the original (each
 * function's address is in its comment).
 *
 * RuleOutOfSnakeBounds: +4 the driver, +8 enabled, +0x50 inside the boundary, +0x54 the clock (the driver's +0x130) until which the
 * driver may stay out (5 s after it left). Leaving or coming back is reported to the driver's vf9 (slot 0x24) with the rule. */
#include "race_snake.h"
#include "race_extern.h"
#include "build_activity_util.h"
#include "build_activity_int.h"

#define VT_RULE_BOUNDS 0x0093fe00u

/* 00589120 (ECX = driver) -> a RuleOutOfSnakeBounds (0x60 bytes from the driver's rule storage +0x3e4, appended to the rules +0xa0 /
 * +0xee4), 0 when the driver has 18 rules */
rx_addr rx_snake_bounds_rule_create(rx_addr d) {
    if (U32(d, 0xee4) >= 0x12) return 0;
    const rx_addr r = U32(d, 0x3e4);
    SU32(d, 0x3e4, r + 0x60);
    rx_addr rule = 0;
    if (r != 0) { SU32(r, 0, VT_RULE_BOUNDS); rule = r; }
    SU32(d, 0xa0 + 4 * U32(d, 0xee4), rule);
    const rx_addr out = U32(d, 0xa0 + 4 * U32(d, 0xee4));
    SU32(d, 0xee4, U32(d, 0xee4) + 1);
    return out;
}
RX_FUNCTION(0x00589120, rx_snake_bounds_rule_create)

/* 0051bb30 slot 1 (thiscall; driver; RET 4): +4 the driver, enabled, inside, the clock 0 */
void rx_snake_bounds_rule_vf1(rx_addr rule, rx_addr driver) {
    SU32(rule, 4, driver);
    SU32(rule, 8, 1);
    SU32(rule, 0x50, 1);
    SF32(rule, 0x54, 0.0f);
}
RX_FUNCTION(0x0051bb30, rx_snake_bounds_rule_vf1)

/* 0051bb50 slot 4 (thiscall; on; RET 4): enabled = on; disabled it is inside again with the clock 0 */
void rx_snake_bounds_rule_vf4(rx_addr rule, uint32_t on) {
    SU32(rule, 8, on);
    if (on == 0) { SU32(rule, 0x50, 1); SF32(rule, 0x54, 0.0f); }
}
RX_FUNCTION(0x0051bb50, rx_snake_bounds_rule_vf4)

/* 0056bf20 (EAX = driver, ECX = MXActivitySnake) -> EAX 1 when the driver's vehicle (vf17 slot 0x44) is outside the boundary (the
 * distance^2 in x / z from +0xff0e0 / +0xff0e8 at least +0xff0dc^2; then "OutOfBounds" is raised with the driver), else 0.
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x14; push ebx / esi / edi (the position F + 0x10). */
uint32_t rx_snake_out_of_bounds(rx_addr d, rx_addr act) {
    ACT_FRAME_EBP(F, 16, 0x14, 3);
    { const uint32_t a[1] = {F + 0x10}; (void)act_vcall(U32(U32(d, 0x74), 0x1ac), 0x44, 1, a); }
    uint32_t out = 0;
    const float dz = rx_subs(F32(F, 0x18), F32(act, 0xff0e8)), r = F32(act, 0xff0dc);
    SF32(F, 0x14, 0.0f);
    const float dx = rx_subs(F32(F, 0x10), F32(act, 0xff0e0));
    const float d2 = rx_adds(rx_muls(dz, dz), rx_muls(dx, dx));
    if (d2 >= rx_muls(r, r)) {   /* comiss d2, r^2; jb: inside (NaN inside) */
        out = 1;
        SU32(F, 0xc, d);
        act_raise(U32(act, 0x290), 0x00942160 /* "OutOfBounds" */, 0x009cda44, F + 0xc, 4);
    }
    ACT_END(F);
    return out;
}
RX_FUNCTION(0x0056bf20, rx_snake_out_of_bounds)

/* 00574300 (EAX = driver, ESI = MXActivityInfection) -> EAX 1 when the driver's vehicle (vf17 slot 0x44) is outside the boundary (the
 * distance^2 in x / z from +0xff0a0 / +0xff0a8 at least +0xff098^2), else 0. Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x1c;
 * push edi (the position F + 0x10). */
uint32_t rx_infection_out_of_bounds(rx_addr d, rx_addr act) {
    ACT_FRAME_EBP(F, 16, 0x1c, 1);
    { const uint32_t a[1] = {F + 0x10}; (void)act_vcall(U32(U32(d, 0x74), 0x1ac), 0x44, 1, a); }
    const float dx = rx_subs(F32(F, 0x10), F32(act, 0xff0a0)), dz = rx_subs(F32(F, 0x18), F32(act, 0xff0a8)), r = F32(act, 0xff098);
    const float d2 = rx_adds(rx_muls(dx, dx), rx_muls(dz, dz));
    ACT_END(F);
    return d2 >= rx_muls(r, r) ? 1u : 0u;   /* comiss d2, r^2; jae */
}
RX_FUNCTION(0x00574300, rx_infection_out_of_bounds)

/* 0051bb70 slot 5 (thiscall; dt; RET 4): for an enabled rule of a driver with a vehicle in an activity: out of the boundary (Snake
 * type 0xa: 0056bf20, Infection 0xe: 00574300; any other activity reads an uninitialised slot of the frame) while inside: the clock +5 s
 * (0x935890) into +0x54, outside, and the driver's vf9 (slot 0x24) with the rule; back inside while outside: inside and vf9.
 * Frame: push ebp; mov ebp, esp; and esp, -8; sub esp, 8; push esi / edi. */
void rx_snake_bounds_rule_vf5(rx_addr rule, float dt) {
    ACT_FRAME_EBP(F, 8, 8, 2);
    (void)dt;
    const rx_addr d = U32(rule, 4), p = U32(d, 0x74);
    if (U32(p, 0x1ac) == 0 || U32(rule, 8) == 0 || U32(p, 0x94) == 0) { ACT_END(F); return; }
    const rx_addr act = U32(p, 0x94);
    uint32_t out;
    if (U32(act, 0x23c) == 0xa) ACT_CALLR(out, 0, rx_snake_out_of_bounds(d, act));
    else if (U32(act, 0x23c) == 0xe) ACT_CALLR(out, 0, rx_infection_out_of_bounds(d, act));
    else out = U32(F, 0xc);
    const uint32_t a[1] = {rule};
    if (U32(rule, 0x50) != 0) {
        if (out != 0) {
            const rx_addr dr = U32(rule, 4);
            SF32(rule, 0x54, rx_adds(F32(dr, 0x130), 5.0f /* 0x935890 */));
            SU32(rule, 0x50, 0);
            (void)act_vcall(dr, 0x24, 1, a);
        }
    } else if (out == 0) {
        SU32(rule, 0x50, 1);
        (void)act_vcall(U32(rule, 4), 0x24, 1, a);
    }
    ACT_END(F);
}
RX_FUNCTION(0x0051bb70, rx_snake_bounds_rule_vf5)
