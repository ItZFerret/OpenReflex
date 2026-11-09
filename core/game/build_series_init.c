/* build_series_init.c -- the static MXSeriesManager / MXSeriesProgress (0x009e2188; guard bit 0 of 0x009e275c): the career's series state. MXSeriesManager_Construct
 * 005b2370 builds it on first use: the progress reset (005b0620), the rewards block (0x009e2200: the "RewardsUnlock" / "RewardsPurchase" tables 005b1440), the four series
 * event slots (Heat 0x009e231c, Moto 0x009e2410, Freestyle 0x009e2508, Single 0x009e25f4: their vtables and defaults) and the "SeriesStandings" table (0x009e26e8) with its
 * columns. A race that is not a career event reads only that the series is not running (0x009e2318 = 0, 0x009e2314 = 0). The UI property "SeriesHasRewards" the original
 * erases (005dc230: the UI manager's store) is the UI boundary (the oracle runs it, the product has no UI manager). */
#include "build_activity_util.h"
#include "build_tables.h"
#include "build_series_init.h"
#include "career_properties.h"
#include <string.h>

/* 005b14d0 (ESI = the rewards block 0x009e2200, EAX = the progress): +0 = EAX; both tables' rows cleared (004ecdb0); +0xf4 .. +0x104 = 0, +0xec = -1, +0xf0 = 1000; the UI
 * property "SeriesHasRewards" erased (005dc230, ECX = the UI manager: UI). 005dc230 is a THIS-CALL: ECX = [0x00a93fcc], the key pushed; with a stdcall ECX keeps the
 * previous call's residue and its prologue `lea esi, [ecx + 0x134]` makes the property-map probe divide by a garbage bucket count (or read [ecx + 0x134]) */
void rx_series_rewards_reset(rx_addr b, uint32_t progress) {
    ACT_FRAME(L, 0, 1);
    SU32(b, 0, progress);
    ACT_CALL(1, rx_tbl_table_clear_rows(b + 4));
    ACT_CALL(1, rx_tbl_table_clear_rows(b + 0x78));
    SU32(b, 0xf4, 0); SU32(b, 0xf8, 0); SU32(b, 0xfc, 0); SU32(b, 0x100, 0); SU32(b, 0x104, 0);
    SU32(b, 0xec, 0xffffffffu); SU32(b, 0xf0, 0x3e8);
#ifdef RX_ORACLE
    { const uint32_t a[1] = {0x0094d884 /* "SeriesHasRewards" */}; act_ui_call(0x005dc230, RX_THISCALL, U32(0, 0x00a93fcc), 1, a); }
#else
    rx_owned_property_erase(0x0094d884);
#endif
    ACT_END(L);
}
RX_FUNCTION(0x005b14d0, rx_series_rewards_reset)

/* 005b0620 (EAX = the progress 0x009e2188): its fields to their start values (+0x4c / +0x74 = 1, +0x20 = 0, +0x1c = -1, +0x24 = 1000, +0x28 .. +0x34 = 0, +4 = +0xc = 0,
 * +0x38 = 0x50, +0x40 = +0x48 = 0, +0x58 = +0x5c = +0x68 = +0x64 = -1, +0 = 0, +0x10 = 2, +0x14 = 0x3d, +0x18 = 0), then the rewards block (progress + 0x78) reset */
void rx_series_progress_reset(rx_addr p) {
    ACT_FRAME(L, 0, 1);
    SU32(p, 0x4c, 1); SU32(p, 0x74, 1);
    SU32(p, 0x20, 0); SU32(p, 0x1c, 0xffffffffu); SU32(p, 0x24, 0x3e8);
    SU32(p, 0x28, 0); SU32(p, 0x2c, 0); SU32(p, 0x30, 0); SU32(p, 0x34, 0);
    SU32(p, 4, 0); SU32(p, 0xc, 0);
    SU32(p, 0x38, 0x50); SU32(p, 0x40, 0); SU32(p, 0x48, 0);
    SU32(p, 0x58, 0xffffffffu); SU32(p, 0x5c, 0xffffffffu); SU32(p, 0x68, 0xffffffffu); SU32(p, 0x64, 0xffffffffu);
    SU32(p, 0, 0); SU32(p, 0x10, 2); SU32(p, 0x14, 0x3d); SU32(p, 0x18, 0);
    ACT_CALL(0, rx_series_rewards_reset(p + 0x78, p));
    ACT_END(L);
}
RX_FUNCTION(0x005b0620, rx_series_progress_reset)

/* 005b1440 (ESI = the rewards block) -> ESI: the "RewardsUnlock" (+4) and "RewardsPurchase" (+0x78) tables constructed, +0 / +0xfc / +0x100 / +0x104 = 0, the int column
 * "StoreProductId" (default -1, flag 1) on the first and "StoreAssetId" on the second */
rx_addr rx_series_rewards_construct(rx_addr b) {
    ACT_FRAME(L, 0, 3);
    const rx_addr def = L + 8;
    ACT_CALL(1, (void)rx_tbl_table_construct_named(b + 4, 0x0094d864 /* "RewardsUnlock" */));
    ACT_CALL(1, (void)rx_tbl_table_construct_named(b + 0x78, 0x0094d874 /* "RewardsPurchase" */));
    SU32(b, 0, 0); SU32(b, 0xfc, 0); SU32(b, 0x100, 0); SU32(b, 0x104, 0);
    SU32(def, 0, 0xffffffffu);
    ACT_CALL(7, rx_tbl_add_int_column(b + 4, 0x00936ed4 /* "StoreProductId" */, def, 1, 0, 0x00935d6c, 0));
    SU32(def, 0, 0xffffffffu);
    ACT_CALL(7, rx_tbl_add_int_column(b + 0x78, 0x0093e63c /* "StoreAssetId" */, def, 1, 0, 0x00935d6c, 0));
    ACT_END(L);
    return b;
}
RX_FUNCTION(0x005b1440, rx_series_rewards_construct)

/* MXSeriesManager_Construct 005b2370 (cdecl) -> 0x009e2188 */
rx_addr rx_series_manager_construct(void) {
    ACT_FRAME(L, 0x24, 4);
    const rx_addr tab = 0x009e26e8, def = L + 0x14;
    ACT_CALL(0, (void)rx_series_rewards_construct(0x009e2200));
    ACT_CALL(0, rx_series_progress_reset(0x009e2188));
    SU32(0, 0x009e2348, 0); SU32(0, 0x009e234c, 0); SU32(0, 0x009e2324, 10); SU32(0, 0x009e233c, 1); SU32(0, 0x009e231c, 0x0094e28c);   /* Heat */
    SU32(0, 0x009e2408, 0); SU32(0, 0x009e240c, 0); SU32(0, 0x009e23cc, 0); SU32(0, 0x009e23d0, 3);
    SU32(0, 0x009e243c, 0); SU32(0, 0x009e2440, 0); SU32(0, 0x009e2418, 10); SU32(0, 0x009e2430, 1); SU32(0, 0x009e2410, 0x0094e2fc);   /* Moto */
    SU32(0, 0x009e24fc, 0); SU32(0, 0x009e2500, 0); SU32(0, 0x009e24c0, 0); SU32(0, 0x009e24c4, 3);
    SU32(0, 0x009e2534, 0); SU32(0, 0x009e2538, 0); SU32(0, 0x009e2510, 10); SU32(0, 0x009e2528, 1); SU32(0, 0x009e2508, 0x0094e3dc);   /* Freestyle */
    SU32(0, 0x009e25b8, 1); SU32(0, 0x009e25bc, 3);
    SU32(0, 0x009e2620, 0); SU32(0, 0x009e2624, 0); SU32(0, 0x009e25fc, 10); SU32(0, 0x009e2614, 1); SU32(0, 0x009e25f4, 0x0094e36c);   /* Single */
    SU32(0, 0x009e26e0, 0); SU32(0, 0x009e26e4, 0); SU32(0, 0x009e26a4, 0); SU32(0, 0x009e26a8, 3);
    ACT_CALL(1, (void)rx_tbl_table_construct_named(tab, 0x0094d900 /* "SeriesStandings" */));
    SU32(0, 0x009e2308, 0); SU32(0, 0x009e2318, 0); SU32(0, 0x009e230c, 0); SU32(0, 0x009e2310, 0); SU32(0, 0x009e2314, 0);
    static const struct { uint32_t name, flags; } cols[7] = {
        {0x0094c990, 1}, {0x0094d6b8, 1}, {0x0094c150, 0}, {0x0094c144, 0}, {0x0094c138, 0}, {0x0094d910, 0}, {0x0093e170, 1}};
    for (int k = 0; k < 7; k++) {
        SU32(def, 0, 0xffffffffu);
        ACT_CALL(7, rx_tbl_add_int_column(tab, cols[k].name, def, cols[k].flags, 0, 0x00935d6c, 0));
    }
    {   /* the "PlayerName" string column: a local empty std::string (capacity 0xf) as its default */
        const rx_addr str = L + 0x18;
        SU32(str, 0x18, 0xf); SU32(str, 0x14, 0); SU8(str, 4, 0);
        ACT_CALL(6, rx_tbl_add_string_column(tab, 0x00936f00 /* "PlayerName" */, str, 1, 0, 0x00935d6c));
    }
    SU32(def, 0, 0);
    ACT_CALL(7, rx_tbl_add_int_column(tab, 0x00936ef4 /* "PlayerType" */, def, 1, 0, 0x00935d6c, 0x005964a0));
    ACT_END(L);
    return 0x009e2188;
}
RX_FUNCTION(0x005b2370, rx_series_manager_construct)
