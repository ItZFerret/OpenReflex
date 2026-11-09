/* build_activity_setup.c -- construction unit 8: the event's table rows (build_activity.h): EnsureLocalPlayers 0059e030 and the
 * Player List row 004dd460, the viewports 005a1420, the local players cleared 0059e310 / pruned 0059e7a0, the rows nobody
 * references freed 0059e4e0. Transliterated from the disassembly; frames at the original's stack addresses (their std::vector /
 * std::set temporaries put stack addresses into the heap). */
#include "build_activity_util.h"
#include <string.h>

/* ======================================================================================================================== */
/* the event spec                                                                                                           */

static rx_event_spec g_spec;
static int g_spec_set;
static void wset(uint16_t* d, const char* s) { int i = 0; for (; s[i] && i < 67; i++) d[i] = (uint8_t)s[i]; d[i] = 0; }
void rx_event_spec_default(rx_event_spec* e) {
    memset(e, 0, sizeof *e);
    strcpy(e->event_type, "National"); e->event_type_id = 10;
    strcpy(e->location, "NAT_Concord"); e->location_id = 50; e->track_id = 0;
    e->duration = 3; strcpy(e->duration_type, "Laps");
    e->riders = 12; e->ai_difficulty = 95; e->vehicle_class = 2;
    strcpy(e->player_mode, "SinglePlayer");
    wset(e->player_name, "Player 1");
}
void rx_event_set_spec(const rx_event_spec* e) { g_spec = *e; g_spec_set = 1; }
const rx_event_spec* rx_event_spec_get(void) { if (!g_spec_set) { rx_event_spec_default(&g_spec); g_spec_set = 1; } return &g_spec; }

/* ======================================================================================================================== */
/* the profile's name (not core: the profile layer; the spec carries it)                                                    */

/* MXProfile_GetDisplayName 00616840 (stdcall profile, wide buffer; RET 8): oracle the original (it reads the profile manager and the
 * UI string table), product the spec's name with the original's wcsncpy(dst, name, 0x43), dst[0x43] = 0 */
static void act_profile_display_name(int32_t controller, rx_addr wbuf) {
#ifdef RX_ORACLE
    const rx_addr profile = controller < 0 ? 0 : U32(U32(0, 0x00d92ee0), 4 + 4 * (uint32_t)controller);
    const uint32_t a[2] = {profile, wbuf};
    rx_call_original(0x00616840, RX_STDCALL, 0, 2, a);
#else
    (void)controller;
    const uint16_t* n = rx_event_spec_get()->player_name;
    uint32_t i = 0;
    for (; i < 0x43 && n[i]; i++) SU16(wbuf, 2 * i, n[i]);
    for (; i < 0x43; i++) SU16(wbuf, 2 * i, 0);
    SU16(wbuf, 0x86, 0);
#endif
}
/* WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, n, 0, 0) (KERNEL32 through 0x009160a8): oracle the import, product the same
 * conversion (UTF-16 -> UTF-8 up to and with the terminator, at most n bytes) */
static void act_wide_to_utf8(rx_addr src, rx_addr dst, uint32_t n) {
#ifdef RX_ORACLE
    const uint32_t a[8] = {0xfde9, 0, src, 0xffffffffu, dst, n, 0, 0};
    rx_call_original(U32(0, 0x009160a8), RX_STDCALL, 0, 8, a);
#else
    uint32_t o = 0;
    for (uint32_t i = 0;; i++) {
        uint32_t c = U16(src, 2 * i);
        uint8_t b[3]; uint32_t k;
        if (c < 0x80) { b[0] = (uint8_t)c; k = 1; }
        else if (c < 0x800) { b[0] = (uint8_t)(0xc0 | c >> 6); b[1] = (uint8_t)(0x80 | (c & 0x3f)); k = 2; }
        else { b[0] = (uint8_t)(0xe0 | c >> 12); b[1] = (uint8_t)(0x80 | ((c >> 6) & 0x3f)); b[2] = (uint8_t)(0x80 | (c & 0x3f)); k = 3; }
        if (o + k > n) break;
        for (uint32_t j = 0; j < k; j++) SU8(dst, o++, b[j]);
        if (!c) break;
    }
#endif
}
void rx_profile_display_name(int32_t controller, rx_addr wide_buffer) { act_profile_display_name(controller, wide_buffer); }
void rx_wide_to_utf8(rx_addr wide, rx_addr out, uint32_t capacity) { act_wide_to_utf8(wide, out, capacity); }
/* FUN_0059df30 (stdcall controller, buffer, size; RET 0xc): the profile's name as UTF-8 into the buffer (and, UI only, the "DYN_
 * PROFILENAME_<n>" dynamic string of the UI string table: oracle the original, product only the buffer) */
static void act_profile_name_utf8(int32_t controller, rx_addr buf, int32_t size) {
#ifdef RX_ORACLE
    const uint32_t a[3] = {(uint32_t)controller, buf, (uint32_t)size};
    rx_call_original(0x0059df30, RX_STDCALL, 0, 3, a);
#else
    ACT_FRAME(L, 0xd0, 4);                                             /* sub esp, 0xd0; push ebx / ebp / esi / edi: the wide name at +0x10 */
    const rx_addr F = L + 0x10;
    SU16(F, 0, 0); crt_memset(F + 2, 0, 0x86);
    act_profile_display_name(controller, F);
    if (buf && size > 0) {
        if (size > 0x43) size = 0x44;
        act_wide_to_utf8(F, buf, (uint32_t)size);
        SU8(buf, (uint32_t)size - 1, 0);
    }
    ACT_END(L);
#endif
}

/* ======================================================================================================================== */
/* Player List rows                                                                                                         */

/* one criterion built, pushed and destroyed (the pattern of every table helper: 00419a70 / 004f6c60 ..., 004175e0, the inline
 * destructor) */
static void crit_int(rx_addr vec, rx_addr crit, rx_addr name, uint32_t value) {
    rx_tbl_crit_init_int(crit, name, 0, value);                       /* 00419a70 (ESI = crit, EAX = op, ECX = &value; name) */
    rx_tbl_crit_push(vec, crit);                                      /* 004175e0 */
    rx_tbl_crit_destroy(crit);
}
static void crit_str(rx_addr vec, rx_addr crit, rx_addr name, rx_addr value) {
    ACT_CALL(2, rx_tbl_crit_init_string(crit, name, 0, value));       /* 004f6c60 (EAX = op, ECX = &value; crit, name; RET 8) */
    rx_tbl_crit_push(vec, crit);
    rx_tbl_crit_destroy(crit);
}
/* FUN_004dd460 (stdcall &player; RET 4): a "Player List" row from a player record (the 0x44-byte struct the callers build on their
 * stack: +0 PlayerType, +4 PlayerLocal, +8 / +0xc / +0x10 PlayerName / DisplayName / MXProfileName (C strings; MXProfileName "" when
 * 0), +0x14 RequestedActivityType, +0x18 RequestedActivityArea (signed char), +0x1c RequestedVehicleId, +0x20 RequestedRiderId,
 * +0x24 Controller, +0x28 Viewport, +0x2c StartZone, +0x30 Active, +0x34 UniqueId, +0x38 Team, +0x3c Spectator, +0x40 AIDifficulty);
 * ActivityType 0x3d, ActivityArea / VehicleId / RiderId -1, Disconnected / Ready / Loaded 0. Returns the new row. */
uint32_t rx_player_list_add_row(rx_addr p) {
    ACT_FRAME(L, 0xe0, 4);
    const rx_addr vec = L + 0x18, crit = L + 0x30, out = L + 0xe4;
    rx_tbl_crit_vec_init(vec);
    crit_int(vec, crit, 0x00936ef4, U32(p, 0));                       /* "PlayerType" (004f6bf0) */
    crit_int(vec, crit, 0x0093de90, U32(p, 4));                       /* "PlayerLocal" */
    crit_str(vec, crit, 0x00936f00, U32(p, 8));                       /* "PlayerName" */
    crit_str(vec, crit, 0x0093deac, U32(p, 0xc));                     /* "DisplayName" */
    if (!U32(p, 0x10)) SU32(p, 0x10, 0x00935d6c);                     /* "" */
    crit_str(vec, crit, 0x0093deb8, U32(p, 0x10));                    /* "MXProfileName" */
    crit_int(vec, crit, 0x0093ded0, U32(p, 0x24));                    /* "Controller" */
    crit_int(vec, crit, 0x0093dedc, U32(p, 0x28));                    /* "Viewport" */
    crit_int(vec, crit, 0x0093df00, U32(p, 0x2c));                    /* "StartZone" */
    crit_int(vec, crit, 0x0093def8, U32(p, 0x30));                    /* "Active" */
    crit_int(vec, crit, 0x0093df0c, U32(p, 0x34));                    /* "UniqueId" */
    crit_int(vec, crit, 0x0093dee8, 0);                               /* "Disconnected" */
    crit_int(vec, crit, 0x0093de38, 0);                               /* "Ready" */
    crit_int(vec, crit, 0x0093de28, 0);                               /* "Loaded" */
    crit_int(vec, crit, 0x0093df18, U32(p, 0x38));                    /* "Team" */
    crit_int(vec, crit, 0x0093df20, U32(p, 0x3c));                    /* "Spectator" */
    crit_int(vec, crit, 0x00936e48, U32(p, 0x40));                    /* "AIDifficulty" (004f6d10) */
    crit_int(vec, crit, 0x0093dd90, 0x3d);                            /* "ActivityType" (004f6d80) */
    crit_int(vec, crit, 0x0093dda0, 0xffffffffu);                     /* "ActivityArea" */
    crit_int(vec, crit, 0x00936cb4, 0xffffffffu);                     /* "VehicleId" */
    crit_int(vec, crit, 0x0093dec8, 0xffffffffu);                     /* "RiderId" */
    crit_int(vec, crit, 0x0093df2c, U32(p, 0x14));                    /* "RequestedActivityType" */
    crit_int(vec, crit, 0x0093df44, (uint32_t)(int32_t)(int8_t)U8(p, 0x18));   /* "RequestedActivityArea" (004f6df0: movsx) */
    crit_int(vec, crit, 0x0093df5c, U32(p, 0x1c));                    /* "RequestedVehicleId" */
    crit_int(vec, crit, 0x0093df70, U32(p, 0x20));                    /* "RequestedRiderId" */
    ACT_CALL(2, rx_tbl_add_row(ACT_PLAYER_LIST, out, vec));           /* 004ec950 */
    const uint32_t row = rx_tbl_row_count(ACT_PLAYER_LIST) - 1;       /* 004021d0 */
    rx_tbl_crit_vec_destroy(vec);
    ACT_END(L);
    return row;
}
RX_FUNCTION(0x004dd460, rx_player_list_add_row)

/* FUN_005a1420: the viewports of the Player List rows: every row with a viewport (up to 4 of them, in row order) remembered, every
 * viewport set to -1, then with fewer than 3 such rows the n-th gets viewport n, with 3 or 4 each gets its controller; each cell
 * that changes counts in the table's change counter (+0x68 = 0x00ae0e70) */
void rx_player_viewports_assign(void) {
    ACT_FRAME(L, 0x1c, 4);
    const rx_addr rows = L + 0x1c;                                     /* int[4] */
    uint32_t n = 0;
    for (uint32_t r = 0;; r++) {
        uint32_t count = U32(ACT_PLAYER_LIST, 0x14) == U32(ACT_PLAYER_LIST, 0x10) ? 0
                       : rx_tbl_column_rows(U32(U32(ACT_PLAYER_LIST, 0x10), 0));   /* 004eb830 */
        if ((int32_t)r >= (int32_t)count) break;
        rx_addr vp = act_column(ACT_PLAYER_LIST, 0x0093dedc);          /* "Viewport" */
        if (I32(U32(U32(vp, 0x40), 0xc), 4 * r) != -1 && (int32_t)n < 4) { SU32(rows, 4 * n, r); n++; }
        SU32(L, 0x14, 0xffffffffu);
        if (act_cell(ACT_PLAYER_LIST, 0x0093dedc, r) != 0xffffffffu) SU32(0, 0x00ae0e70, U32(0, 0x00ae0e70) + 1);
        ACT_CALL(3, rx_tbl_set_int(act_column(ACT_PLAYER_LIST, 0x0093dedc), r, L + 0x14));   /* 004f5330 */
    }
    SU32(L, 0x14, n);
    if ((int32_t)n < 3) {
        for (uint32_t i = 0; (int32_t)i < (int32_t)n; i++) {
            SU32(L, 0x10, i);
            uint32_t r = U32(rows, 4 * i);
            if (act_cell(ACT_PLAYER_LIST, 0x0093dedc, r) != i) SU32(0, 0x00ae0e70, U32(0, 0x00ae0e70) + 1);
            ACT_CALL(3, rx_tbl_set_int(act_column(ACT_PLAYER_LIST, 0x0093dedc), r, L + 0x10));
        }
    } else {
        for (uint32_t i = 0; (int32_t)i < (int32_t)n; i++) {
            SU32(L, 0x10, i);
            uint32_t r = U32(rows, 4 * i);
            uint32_t ctl = act_cell(ACT_PLAYER_LIST, 0x0093ded0, r);   /* "Controller" */
            SU32(L, 0x18, ctl);
            if (act_cell(ACT_PLAYER_LIST, 0x0093dedc, r) != ctl) SU32(0, 0x00ae0e70, U32(0, 0x00ae0e70) + 1);
            ACT_CALL(3, rx_tbl_set_int(act_column(ACT_PLAYER_LIST, 0x0093dedc), r, L + 0x18));
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x005a1420, rx_player_viewports_assign)

/* EnsureLocalPlayers 0059e030 (stdcall controller, activity type; RET 8): without a Player List row for the controller: the rider
 * (the profile's PlayerRider row 0: LastRiderId < 1 -> Rider List (controller, 0, -1), else (4, LastRiderId, LastRiderGearSetId)),
 * the vehicle (Vehicle List (the rider's owner, LastEquippedVehicle)) and the player's row (human, local, the profile's name,
 * requested activity type = the argument, viewport = controller, team = rows & 1); with a row: its RequestedActivityType /
 * RequestedActivityArea set (unless the type is 0x3d), then the viewports. */
void rx_ensure_local_players(int32_t controller, uint32_t activity) {
    ACT_FRAME(L, 0x12c, 4);
    const rx_addr wname = L + 0x18, rec = L + 0x18, mbname = L + 0xa0, name = L + 0xe8;
    {   /* FUN_005dfc90(0): the UI string table (UI); the original fills the profile name only when it exists */
        const uint32_t a[1] = {0};
        uint32_t st = 1;
#ifdef RX_ORACLE
        st = rx_call_original(0x005dfc90, RX_CDECL, 0, 1, a);
#else
        (void)a;
#endif
        if (st) ACT_CALL(3, act_profile_name_utf8(controller, name, 0x44));   /* 0059df30 */
    }
    const rx_addr rider_t = rx_tbl_find(0x00ae10f0u + 0x1f4u * (uint32_t)controller, 0x0093e53c);   /* "PlayerRider" */
    rx_addr ctl = act_column(ACT_PLAYER_LIST, 0x0093ded0);             /* "Controller" */
    rx_addr set = rx_tbl_rows(ctl, controller);                        /* 00418e50 */
    if (U32(set, 0x1c) == 0) {
        uint32_t rider, vehicle;
        if ((int32_t)act_cell(rider_t, 0x0093e55c, 0) < 1) {           /* "LastRiderId" */
            ACT_CALLR(rider, 3, rx_tbl_rider_list_add_row((uint32_t)controller, 0, 0xffffffffu));
            uint32_t lev = act_cell(rider_t, 0x0093e548, 0);           /* "LastEquippedVehicle" */
            ACT_CALLR(vehicle, 2, rx_tbl_vehicle_list_add_row((uint32_t)controller, lev));
        } else {
            uint32_t gear = act_cell(rider_t, 0x0094cf4c, 0);          /* "LastRiderGearSetId" */
            uint32_t rid = act_cell(rider_t, 0x0093e55c, 0);
            ACT_CALLR(rider, 3, rx_tbl_rider_list_add_row(4, rid, gear));
            uint32_t lev = act_cell(rider_t, 0x0093e548, 0);
            ACT_CALLR(vehicle, 2, rx_tbl_vehicle_list_add_row(4, lev));
        }
        uint32_t team = rx_tbl_row_count(ACT_PLAYER_LIST) & 1;         /* 004021d0 */
        SU8(mbname, 0, 0); crt_memset(mbname + 1, 0, 0x43);
        SU16(wname, 0, 0); crt_memset(wname + 2, 0, 0x86);
        ACT_CALL(2, act_profile_display_name(controller, wname));      /* MXProfile_GetDisplayName 00616840 */
        ACT_CALL(8, act_wide_to_utf8(wname, mbname, 0x44));            /* WideCharToMultiByte */
        SU32(rec, 8, name); SU32(rec, 0xc, name); SU32(rec, 0x10, mbname);
        SU32(rec, 0, 0); SU8(rec, 0x18, 0); SU32(rec, 0x3c, 0); SU32(rec, 0x40, 0);
        SU32(rec, 0x14, activity);
        SU8(mbname, 0x43, 0);
        SU32(rec, 4, 1);
        SU32(rec, 0x1c, vehicle); SU32(rec, 0x20, rider);
        SU32(rec, 0x24, (uint32_t)controller); SU32(rec, 0x28, (uint32_t)controller);
        SU32(rec, 0x2c, 0xffffffffu); SU32(rec, 0x30, 1); SU32(rec, 0x34, 0xffffffffu);
        SU32(rec, 0x38, team);
        ACT_CALL(1, rx_player_list_add_row(rec));                      /* 004dd460 */
    } else if (activity != 0x3d) {
        const uint32_t row = U32(LEFT_OF(set), 0xc);
        SU32(L, 0x14, activity);
        ACT_CALL(2, rx_tbl_set_cell_int(ACT_PLAYER_LIST, 0x0093df2c, row, L + 0x14));   /* "RequestedActivityType" (004f6e60) */
        SU32(L, 0x14, 0);
        ACT_CALL(2, rx_tbl_set_cell_int(ACT_PLAYER_LIST, 0x0093df44, row, L + 0x14));   /* "RequestedActivityArea" */
    }
    ACT_CALL(0, rx_player_viewports_assign());                         /* 005a1420 */
    ACT_END(L);
}
RX_FUNCTION(0x0059e030, rx_ensure_local_players)

/* FUN_0059e310 (stdcall controller; RET 4): the controller's Player List row erased with its requested Vehicle / Rider List rows,
 * then the viewports. The original keeps the requested vehicle id in its argument slot. */
void rx_player_remove_local(int32_t controller) {
    ACT_FRAME(L, 0x10, 4);
    rx_addr set = rx_tbl_rows(act_column(ACT_PLAYER_LIST, 0x0093ded0), controller);   /* "Controller" */
    if (U32(set, 0x1c)) {
        const uint32_t row = U32(LEFT_OF(set), 0xc);
        const uint32_t veh = act_cell(ACT_PLAYER_LIST, 0x0093df5c, row);   /* "RequestedVehicleId" */
        SU32(L, 0x24, veh);                                            /* the argument slot */
        const uint32_t rid = act_cell(ACT_PLAYER_LIST, 0x0093df70, row);   /* "RequestedRiderId" */
        SU32(L, 0x10, rid);
        ACT_CALL(2, rx_tbl_erase_row(ACT_PLAYER_LIST, row));           /* 004ece50 */
        if (veh != 0xffffffffu) {
            rx_addr ref = rx_tbl_row(act_column(ACT_VEHICLE_LIST, 0x00936cb4), (int32_t)veh);   /* "VehicleId" (00418ec0) */
            SU32(L, 0x14, U32(ref, 0)); SU32(L, 0x18, U32(ref, 4));
            ACT_CALL(2, rx_tbl_erase_row(U32(ref, 8), U32(ref, 0)));
        }
        if (rid != 0xffffffffu) {
            rx_addr ref = rx_tbl_row(act_column(ACT_RIDER_LIST, 0x0093dec8), (int32_t)rid);     /* "RiderId" */
            SU32(L, 0x14, U32(ref, 0)); SU32(L, 0x18, U32(ref, 4));
            ACT_CALL(2, rx_tbl_erase_row(U32(ref, 8), U32(ref, 0)));
        }
    }
    ACT_CALL(0, rx_player_viewports_assign());                         /* 005a1420 */
    ACT_END(L);
}
RX_FUNCTION(0x0059e310, rx_player_remove_local)

/* PlayerList_FreeUnreferencedRows 0059e4e0: Vehicle List rows (last first) whose VehicleId no Player List row has as VehicleId or
 * RequestedVehicleId are erased, then the same for Rider List rows (RiderId / RequestedRiderId) */
void rx_player_list_free_unreferenced(void) {
    ACT_FRAME(L, 0x14, 4);
    const uint32_t c_vid = rx_tbl_column_index(ACT_PLAYER_LIST, 0x00936cb4), c_rvid = rx_tbl_column_index(ACT_PLAYER_LIST, 0x0093df5c);
    const uint32_t c_rid = rx_tbl_column_index(ACT_PLAYER_LIST, 0x0093dec8), c_rrid = rx_tbl_column_index(ACT_PLAYER_LIST, 0x0093df70);
    const uint32_t v_vid = rx_tbl_column_index(ACT_VEHICLE_LIST, 0x00936cb4), r_rid = rx_tbl_column_index(ACT_RIDER_LIST, 0x0093dec8);
    SU32(L, 0x10, c_rvid); SU32(L, 0x18, c_rid); SU32(L, 0x1c, c_rrid); SU32(L, 0x20, r_rid);
    for (int32_t r = (int32_t)rx_tbl_row_count(ACT_VEHICLE_LIST) - 1; r >= 0; r--) {
        uint32_t id = U32(rx_tbl_cell_word(ACT_VEHICLE_LIST, v_vid, (uint32_t)r), 0);
        SU32(L, 0x14, id);
        rx_addr a = rx_tbl_rows(U32(U32(ACT_PLAYER_LIST, 0x10), 4 * c_vid), (int32_t)id);
        rx_addr b = rx_tbl_rows(U32(U32(ACT_PLAYER_LIST, 0x10), 4 * c_rvid), (int32_t)id);
        if (U32(a, 0x1c) == 0 && U32(b, 0x1c) == 0) ACT_CALL(2, rx_tbl_erase_row(ACT_VEHICLE_LIST, (uint32_t)r));   /* 004ece50 */
    }
    uint32_t n = U32(ACT_RIDER_LIST, 0x14) == U32(ACT_RIDER_LIST, 0x10) ? 0 : rx_tbl_column_rows(U32(U32(ACT_RIDER_LIST, 0x10), 0));
    for (int32_t r = (int32_t)n - 1; r >= 0; r--) {
        uint32_t id = U32(rx_tbl_cell_word(ACT_RIDER_LIST, r_rid, (uint32_t)r), 0);
        SU32(L, 0x14, id);
        rx_addr a = rx_tbl_rows(U32(U32(ACT_PLAYER_LIST, 0x10), 4 * c_rid), (int32_t)id);
        rx_addr b = rx_tbl_rows(U32(U32(ACT_PLAYER_LIST, 0x10), 4 * c_rrid), (int32_t)id);
        if (U32(a, 0x1c) == 0 && U32(b, 0x1c) == 0) ACT_CALL(2, rx_tbl_erase_row(ACT_RIDER_LIST, (uint32_t)r));
    }
    ACT_END(L);
}
RX_FUNCTION(0x0059e4e0, rx_player_list_free_unreferenced)

/* FUN_0059e7a0: the AI players (PlayerType 1) and the non-local players (PlayerLocal 0) deleted from the Player List, then the rows
 * nobody references */
void rx_player_list_prune(void) {
    ACT_FRAME(L, 0xd4, 4);
    const rx_addr vec = L + 0x10, crit = L + 0x30;
    rx_tbl_crit_vec_init(vec);
    SU32(L, 0x28, 1);
    crit_int(vec, crit, 0x00936ef4, 1);                                /* "PlayerType" == 1 */
    ACT_CALL(2, rx_tbl_delete_rows(ACT_PLAYER_LIST, vec));             /* 004ecf00 */
    rx_tbl_crit_vec_clear(vec);                                        /* 004f2c40 */
    SU32(L, 0x28, 0);
    crit_int(vec, crit, 0x0093de90, 0);                                /* "PlayerLocal" == 0 */
    ACT_CALL(2, rx_tbl_delete_rows(ACT_PLAYER_LIST, vec));
    ACT_CALL(0, rx_player_list_free_unreferenced());                   /* 0059e4e0 */
    rx_tbl_crit_vec_destroy(vec);
    ACT_END(L);
}
RX_FUNCTION(0x0059e7a0, rx_player_list_prune)
