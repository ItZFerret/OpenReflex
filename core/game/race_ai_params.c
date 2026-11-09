/* race_ai_params.c -- the AI riders' tuning parameters. Each AI driver has a personality (driver +0xee8, a row of 0xc7c bytes in
 * the table at 0xa89xxx .. 0xa8axxx, .data, filled from the AI character files); each parameter is a block of (low, high) float
 * pairs: 12 pairs by difficulty (player +0x8f4 tier * 3 + player +0x8f8 step) followed by pairs chosen explicitly when the player
 * has an override (player +0x8fc != -1), 0x60 bytes further. (core/notes/race.md "AI difficulty") */
#include "race.h"
#include "race_extern.h"

/* The address of a parameter's (low, high) pair for a driver. */
static rx_addr ai_param_pair(rx_addr driver, rx_addr block, uint32_t* idx_out, int* explicit_out) {
    rx_addr player = U32(driver, 0x74);
    const int32_t sel = I32(player, 0x8fc);
    const uint32_t row = U32(driver, 0xee8) * 0xc7cu;
    if (sel != -1) { *idx_out = (uint32_t)sel; *explicit_out = 1; return row + 8u * (uint32_t)sel + block + 0x60; }
    const uint32_t idx = U32(player, 0x8f4) * 3 + U32(player, 0x8f8);
    *idx_out = idx; *explicit_out = 0;
    return row + 8u * idx + block;
}
float race_ai_param(rx_addr driver, rx_addr block, uint32_t* idx, int* expl) {
    return F32(ai_param_pair(driver, block, idx, expl), 0);
}

/* The single-value getters (EAX = driver) -> xmm0: the low value of the parameter. */
#define AI_PARAM(addr, name, block) \
    float name(rx_addr driver) { uint32_t i; int e; return race_ai_param(driver, block, &i, &e); } \
    RX_FUNCTION(addr, name)
AI_PARAM(0x0051dd20, rx_ai_param_dd20, 0x00a89dc4)
AI_PARAM(0x0051dea0, rx_ai_param_dea0, 0x00a8a0b4)
AI_PARAM(0x0051df40, rx_ai_param_df40, 0x00a8a2e8)
AI_PARAM(0x0051e090, rx_ai_param_e090, 0x00a8a694)
AI_PARAM(0x0051e0e0, rx_ai_param_e0e0, 0x00a8a750)
AI_PARAM(0x0051e130, rx_ai_param_e130, 0x00a8a80c)
AI_PARAM(0x0051e180, rx_ai_param_e180, 0x00a8a8c8)
AI_PARAM(0x0051de50, rx_ai_param_de50, 0x00a8a170)
AI_PARAM(0x0051def0, rx_ai_param_def0, 0x00a8a22c)

/* 0051dd70 (ECX = driver, arg = out (low, high); cdecl): a parameter's range (block 0xa89f3c). */
void rx_ai_param_range_dd70(rx_addr driver, rx_addr out) {
    uint32_t i; int e;
    rx_addr pair = ai_param_pair(driver, 0x00a89f3c, &i, &e);
    SF32(out, 0, F32(pair, 0));
    SF32(out, 4, F32(pair, 4));
}
RX_FUNCTION(0x0051dd70, rx_ai_param_range_dd70)

/* 0050ac80 (EAX = AI controls): all control channels off (+0x2f0 / +0x2f4 / +0x2f8 / +0x30c / +0x31c / +0x324 flags, +0x318 /
 * +0x320 values). 0050acc0: on (the flags 1, +0x324 and +0x2f8 untouched / 0). */
void rx_ai_controls_off(rx_addr c) {
    SU32(c, 0x2f0, 0); SU32(c, 0x2f4, 0); SU32(c, 0x30c, 0); SU32(c, 0x31c, 0); SU32(c, 0x324, 0);
    SF32(c, 0x318, 0.0f); SF32(c, 0x320, 0.0f); SU32(c, 0x2f8, 0);
}
RX_FUNCTION(0x0050ac80, rx_ai_controls_off)
void rx_ai_controls_on(rx_addr c) {
    SU32(c, 0x2f0, 1); SU32(c, 0x2f4, 1); SU32(c, 0x30c, 1); SU32(c, 0x31c, 1); SU32(c, 0x324, 0);
    SF32(c, 0x318, 0.0f); SF32(c, 0x320, 0.0f);
}
RX_FUNCTION(0x0050acc0, rx_ai_controls_on)
