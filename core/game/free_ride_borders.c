/* free_ride_borders.c -- PORT ADDITION (not original game code): FREE RIDE WITHOUT BORDERS (free_ride_borders.h). */
#include "free_ride_borders.h"

uint32_t rx_frb_mode_value;
rx_frb_stats rx_frb_stats_value;

void rx_frb_set(uint32_t mode) { rx_frb_mode_value = mode == RX_FREE_RIDE_BORDERS_OPEN ? 1u : 0u; }

/* the player's activity (+0x94): MXActivityFreeRide is type 8 (+0x23c); mode 4 (+0x238) is the UI world's (UI_Compound: MotoSkills, the
 * garage's test ride). +0x10: 0 human, 1 computer. Read only. */
uint32_t rx_frb_free_ride_human(rx_addr p) {
    if (!p || U32(p, 0x10) != 0) return 0;
    const rx_addr a = U32(p, 0x94);
    return a && U32(a, 0x23c) == 8 && U32(a, 0x238) != 4;
}

/* the MXTrigger's entity Name (+0x18, Entity::vf5's copy) contains "water" (any case) */
uint32_t rx_frb_keep_trigger(rx_addr t) {
    const rx_addr n = t ? U32(t, 0x18) : 0;
    if (!n) return 0;
    static const char w[] = "water";
    for (uint32_t i = 0; i < 256 && U8(n, i); i++) {
        uint32_t k = 0;
        while (w[k]) {
            uint8_t c = U8(n, i + k);
            if (c >= 'A' && c <= 'Z') c = (uint8_t)(c + 32);
            if (c != (uint8_t)w[k]) break;
            k++;
        }
        if (!w[k]) return 1;
    }
    return 0;
}

/* the vehicle (position +0xc0) outside the base terrain's box (+0x44d8 / +0x44dc min x / z, +0x44e0 / +0x44e4 max x / z: the
 * heightfield's own extent, without the original's -30 ft margin) and in the air for RX_FRB_FALL_TIME (the player's airtime +0x10bc) */
uint32_t rx_frb_fallen(rx_addr p, rx_addr g, rx_addr v) {
    const float x = F32(v, 0xc0), z = F32(v, 0xc8);
    const int past = F32(g, 0x44d8) > x || x > F32(g, 0x44e0) || F32(g, 0x44dc) > z || z > F32(g, 0x44e4);
    const int fallen = past && F32(p, 0x10bc) >= RX_FRB_FALL_TIME;
    if (fallen && !(F32(p, 0x116c) > 0.0f)) rx_frb_stats_value.fall_resets++;   /* (a reset already running ignores the call) */
    return (uint32_t)fallen;
}
