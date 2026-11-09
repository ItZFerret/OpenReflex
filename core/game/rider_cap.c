/* rider_cap.c (w-rc-core) -- the side storage of riders 13..32 (rider_cap.h; core/plan/RIDER_CAP.md). Port code, no original function. */
#include "rider_cap.h"
#include "build_tables.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static void rx_fatal(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    fprintf(stderr, "rxcore: "); vfprintf(stderr, fmt, ap); fprintf(stderr, "\n");
    va_end(ap);
    abort();
}

int rx_rc_more(void) { return rx_tbl_row_count(0x00ae0e08u /* the Player List */) > RX_RC_ORIG; }

static uint32_t g_field_size;   /* host static: restored (0) when the session ends (rx_reset.h) */
void rx_rc_set_field_size(uint32_t riders) { g_field_size = riders > RX_RC_ORIG && riders <= RX_RC_MAX ? riders : 0; }
uint32_t rx_rc_field_size(void) { return g_field_size; }

/* the per-object records: RX_RC_ACT_N records keyed by the activity's address; claimed by the first write past the original's
 * entries, released when the activity's own tables are cleared (its construction) */
rx_addr rx_rc_act(rx_addr act, int create) {
    if (!act) return 0;
    for (uint32_t k = 0; k < RX_RC_ACT_N; k++) if (U32(RX_RC_ACT + RX_RC_ACT_SIZE * k, 0) == act) return RX_RC_ACT + RX_RC_ACT_SIZE * k;
    if (!create) return 0;
    rx_addr r = 0;
    for (uint32_t k = 0; k < RX_RC_ACT_N && !r; k++) if (U32(RX_RC_ACT + RX_RC_ACT_SIZE * k, 0) == 0) r = RX_RC_ACT + RX_RC_ACT_SIZE * k;
    if (!r) {   /* every record taken (activities that ended without clearing theirs): the oldest goes (round robin) */
        static uint32_t next;
        r = RX_RC_ACT + RX_RC_ACT_SIZE * (next++ % RX_RC_ACT_N);
        for (uint32_t o = 0; o < RX_RC_ACT_SIZE; o += 4) SU32(r, o, 0);
    }
    SU32(r, 0, act);
    return r;
}
void rx_rc_act_clear(rx_addr act) {
    const rx_addr r = rx_rc_act(act, 0);
    if (r) for (uint32_t o = 0; o < RX_RC_ACT_SIZE; o += 4) SU32(r, o, 0);
}

/* the extended global tables (rider_cap.h rx_rc_tab): {the table, its entry stride}; each gets 20 entries of side storage, one after
 * another from RX_RC_TABS in this order (0x100-aligned) */
static const struct { uint32_t table, stride; } k_tabs[] = {
    /* the rider animation workers (build_rider_init.c 00619490, anim_scenario_rider.c, world_rider_62.c, loop_anim.c) */
    {0x00a9408cu, 4}, {0x00a940bcu, 4}, {0x00a940ecu, 4}, {0x00a9411cu, 4}, {0x00af6c08u, 4}, {0x00af6b80u, 4}, {0x00af6b50u, 4}, {0x00af6c38u, 8},
    /* the mud records (fx_mud_bind.c 00408fb0: 12 x 0x40, owner +0x30) */
    {0x00a96d90u, 0x40},
    /* the damage behaviours of the manager 0x00db1410 (world_veh_raven.c 0047bb40: 12 x 0x410 from +0x10) */
    {0x00db1420u, 0x410},
    /* the highlight recorder's places of the riders (anim_scenario_clip.c 006ab530: .data 0xa7f3a8, 12; the sample sections follow) */
    {0x00a7f3a8u, 4},
    /* the AI's air-leveling table (BehaviorAirLeveling 0051f450: .data 0x9e3f68, 12; count 0xa89d04) */
    {0x009e3f68u, 4},
    /* the vehicle particle effect sets (fx_worldparticles.c 00691cc0: 0xc11e90, 12 x 0x17c30) */
    {0x00c11e90u, 0x17c30},
};
#define RX_RC_TABS_END (RX_RC_BASE + 0x1000000u)
#define NTABS (sizeof k_tabs / sizeof k_tabs[0])
static uint32_t g_tab_hw[NTABS];   /* the side entries taken per table (high water) */
rx_addr rx_rc_tab_side(rx_addr table, uint32_t stride, uint32_t k) {
    if (k < RX_RC_ORIG) return table + stride * k;
    if (k >= RX_RC_MAX) rx_fatal("rider_cap: entry %u of the table 0x%08x past the cap", k, table);
    rx_addr at = RX_RC_TABS;
    for (uint32_t i = 0; i < sizeof k_tabs / sizeof k_tabs[0]; i++) {
        if (k_tabs[i].table == table) {
            if (k_tabs[i].stride != stride) rx_fatal("rider_cap: the table 0x%08x has stride 0x%x, not 0x%x", table, k_tabs[i].stride, stride);
            return at + stride * (k - RX_RC_ORIG);
        }
        at += (k_tabs[i].stride * RX_RC_SIDE + 0xffu) & ~0xffu;
        if (at > RX_RC_TABS_END) rx_fatal("rider_cap: the side tables overflow their region");
    }
    rx_fatal("rider_cap: the table 0x%08x has no side storage (rider_cap.c k_tabs)", table);
    return 0;
}
static uint32_t tab_index(rx_addr table) {
    for (uint32_t i = 0; i < NTABS; i++) if (k_tabs[i].table == table) return i;
    rx_fatal("rider_cap: the table 0x%08x has no side storage (rider_cap.c k_tabs)", table);
    return 0;
}
uint32_t rx_rc_tab_n(rx_addr table) { return RX_RC_ORIG + g_tab_hw[tab_index(table)]; }
void rx_rc_tab_mark(rx_addr table, uint32_t k) {
    const uint32_t i = tab_index(table);
    if (k >= RX_RC_ORIG && k < RX_RC_MAX && k - RX_RC_ORIG + 1 > g_tab_hw[i]) g_tab_hw[i] = k - RX_RC_ORIG + 1;
}
