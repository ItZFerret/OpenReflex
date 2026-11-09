/* race_activity_ui.c -- the UI property store and the UI language table, as the race / activity code reads them, and the profile's
 * collectable flags (0054d590).
 *   The UI manager's property store (ui + 0x134): a hash map keyed by case-insensitive name; the value records are { +0 the value
 *   (an int or a float), +4 the text (type 2), +0x24 the type: 1 int, 2 text, 3 float }. UIProperty_GetInt 005dc0c0 converts any
 *   type to an int. In the product there is no UI manager: rx_ui_session_get_int reads the host's event spec instead (the
 *   documented UI boundary, build_ui_props.c), the oracle runs this port over the real store.
 *   The language table (a localised-string database): a map from the djb2 hash of a string id to the entry index
 *   (005e0270 -> 005ee560), entries 0x1c bytes from +0x14 of the table, the default entry at +0x64. */
#include "race.h"
#include "race_extern.h"
#include "race_activity_ui.h"
#include "build_asset.h"
#include "build_ui_props.h"
#include "build_vehicle.h"
#include "build_tables.h"
#include "fx.h"
#include "crt.h"

/* 005c3440 (ECX = the stored key, EAX = the key) -> AL: the two names are equal ignoring case (_stricmp). Frame: push eax, push ecx. */
uint32_t rx_ui_key_equal(rx_addr stored, rx_addr key) {
    RACE_FRAME_PLAIN(F, 0, 2);
    int32_t d;
    RACE_CORE_CALL(2, d = crt_stricmp(stored, key));
    RACE_FRAME_END(F);
    return d == 0;
}
RX_FUNCTION(0x005c3440, rx_ui_key_equal)

/* 005eab30 (ECX = the property map {+0 bucket count, +0xc buckets}; stdcall &key (a char* slot), &out; RET 8) -> 1 found: the
 * bucket of the case-insensitive hash of the key (005c3400); in the chain (node +0 the name, +4 the value record, +8 the hash,
 * +0xc the next) the first node with the same hash and an equal name gives its value record. Frame: push ebp; sub 0x18. */
uint32_t rx_ui_property_map_find(rx_addr map, rx_addr key_slot, rx_addr out_slot) {
    RACE_FRAME_PLAIN(F, 0x18, 1);
    const rx_addr key = U32(key_slot, 0);
    uint32_t hash;
    RACE_CORE_CALL(0, hash = rx_string_hash_nocase(key));          /* 005c3400 (EAX = the key) */
    rx_addr node = U32(U32(map, 0xc), 4 * (hash % U32(map, 0)));
    uint32_t found = 0;
    while (node != 0) {
        if (U32(node, 8) == hash) {
            uint32_t equal;
            RACE_CORE_CALL(0, equal = rx_ui_key_equal(U32(node, 0), key));
            if ((equal & 0xff) != 0) {
                SU32(out_slot, 0, U32(node, 4));
                found = 1;
                break;
            }
        }
        node = U32(node, 0xc);
    }
    RACE_FRAME_END(F);
    return found;
}
RX_FUNCTION(0x005eab30, rx_ui_property_map_find)

/* 005dc0c0 UIProperty_GetInt (EAX = the property name; stdcall ui, &out; RET 8) -> EAX = 1 when the name exists (0: *out is left
 * alone): *out = the value as an int: type 1 the stored dword, 2 atoi of the text, 3 the float truncated (cvttss2si), else 0.
 * Frame: sub 8 (the found record at +0, the key at +4). */
uint32_t rx_ui_property_get_int(rx_addr key, rx_addr ui, rx_addr out) {
    RACE_FRAME_PLAIN(F, 8, 0);
    SU32(F, 0, 0);
    SU32(F, 4, key);
    uint32_t found;
    RACE_CORE_CALL(2, found = rx_ui_property_map_find(ui + 0x134, F + 4, F));
    if (found == 0) {
        RACE_FRAME_END(F);
        return 0;
    }
    const rx_addr rec = U32(F, 0);
    switch (U32(rec, 0x24)) {
    case 1:
        SU32(out, 0, U32(rec, 0));
        break;
    case 2: {
        int32_t v;
        RACE_CORE_CALL(1, v = crt_atoi(rec + 4));                   /* atoi (IAT 0x916220) */
        SU32(out, 0, (uint32_t)v);
        break;
    }
    case 3:
        SU32(out, 0, (uint32_t)rx_cvtt(F32(rec, 0)));               /* cvttss2si */
        break;
    default:
        SU32(out, 0, 0);
        break;
    }
    RACE_FRAME_END(F);
    return 1;
}
RX_FUNCTION(0x005dc0c0, rx_ui_property_get_int)

uint32_t rx_ui_session_get_int(rx_addr key, rx_addr out) {
#ifdef RX_ORACLE
    return rx_ui_property_get_int(key, U32(0, 0x00a93fcc), out);
#else
    return rx_ui_prop_get_int(key, out);
#endif
}

/* ---- the UI language table --------------------------------------------------------------------------------------------------- */

/* 005e1190 (cdecl string) -> EAX: the djb2 hash: h = 5381; for each character (signed) h = h * 33 + c. The original advances its
 * argument slot with the string pointer: it ends one past the terminator. */
uint32_t rx_djb2_hash(rx_addr s) {
    const rx_addr slot = rx_sp() + 4;
    uint32_t h = 0x1505;
    for (;; s++) {
        const int32_t c = (int8_t)U8(s, 0);
        if (c == 0) break;
        h = h * 0x21 + (uint32_t)c;
    }
    SU32(slot, 0, s + 1);
    return h;
}
RX_FUNCTION(0x005e1190, rx_djb2_hash)

/* 005c3340 (ECX = x) -> EAX: Thomas Wang's 32-bit integer hash with the arithmetic shifts (sar) of the signed original. */
uint32_t rx_int_hash_signed(uint32_t x) {
    x = x + ~(x << 15);
    x = x ^ (uint32_t)((int32_t)x >> 10);
    x = x * 9;
    x = x ^ (uint32_t)((int32_t)x >> 6);
    x = x + ~(x << 11);
    return x ^ (uint32_t)((int32_t)x >> 16);
}
RX_FUNCTION(0x005c3340, rx_int_hash_signed)

/* 005c3370 (ECX = a; cdecl b) -> AL: a == b. */
uint32_t rx_int_key_equal(uint32_t stored, uint32_t key) { return stored == key; }
RX_FUNCTION(0x005c3370, rx_int_key_equal)

/* 005ee560 (ECX = the map {+0 bucket count, +0xc buckets}; stdcall &key (an int slot), &out; RET 8) -> 1 found: the same chained
 * lookup as 005eab30 with the integer hash 005c3340 of the key and node +0 compared by value. Frame: push ebp; sub 0xc. */
uint32_t rx_int_map_find(rx_addr map, rx_addr key_slot, rx_addr out_slot) {
    RACE_FRAME_PLAIN(F, 0xc, 1);
    const uint32_t key = U32(key_slot, 0);
    const uint32_t hash = rx_int_hash_signed(key);
    rx_addr node = U32(U32(map, 0xc), 4 * (hash % U32(map, 0)));
    uint32_t found = 0;
    while (node != 0) {
        if (U32(node, 8) == hash && (rx_int_key_equal(U32(node, 0), key) & 0xff) != 0) {
            SU32(out_slot, 0, U32(node, 4));
            found = 1;
            break;
        }
        node = U32(node, 0xc);
    }
    RACE_FRAME_END(F);
    return found;
}
RX_FUNCTION(0x005ee560, rx_int_map_find)

/* 005e0270 (EAX = the string id; EDI = the language holder, whose first dword is the table) -> EAX: the localised entry (0x1c
 * bytes) of the id in the table's map (+0x44 -> +0x18) while the table's busy count (+0x4c) is raised, the table's default entry
 * (+0x64) when the id is unknown, 0 without a table. Frame: sub 8; push esi. */
rx_addr rx_ui_language_lookup(rx_addr key, rx_addr holder) {
    RACE_FRAME_PLAIN(F, 8, 1);
    uint32_t hash;
    RACE_CORE_CALL(1, hash = rx_djb2_hash(key));                    /* 005e1190 */
    SU32(F, 4, hash);
    const rx_addr table = U32(holder, 0);
    if (table == 0) {
        RACE_FRAME_END(F);
        return 0;
    }
    const rx_addr data = U32(table, 0x44);
    SU32(table, 0x4c, 1);
    if (data == 0) {
        RACE_FRAME_END(F);
        return 0;
    }
    SU32(F, 0, 0);
    uint32_t found;
    RACE_CORE_CALL(2, found = rx_int_map_find(U32(data, 0x18), F + 4, F));   /* 005ee560 */
    rx_addr entry;
    if (found != 0) entry = U32(data, 0x14) + 0x1c * U32(F, 0);
    else entry = data + 0x64;
    SU32(U32(holder, 0), 0x4c, 0);
    RACE_FRAME_END(F);
    return entry;
}
RX_FUNCTION(0x005e0270, rx_ui_language_lookup)

/* ---- the profile's collectable flags ---------------------------------------------------------------------------------------- */

/* 0054d590 (stdcall player, index; RET 8): a free ride discovery recorded in the player's profile: with the player's profile
 * number (+0x1f0) set and the session's EventLocationId (UIProperty_GetInt, -1 when absent), the profile database
 * (0xae10f0 + 0x1f4 * number) table "PlayerCollectables"'s row of that location gets "HitDiscoveryCollectables" |= 1 << index
 * (Table_SetCellInt). The original builds the new value in its first argument slot. Frame: sub 8; push esi. */
void rx_free_ride_collectable_found(rx_addr player, uint32_t index) {
    const rx_addr args = rx_sp() + 4;
    RACE_FRAME_PLAIN(F, 8, 1);
    SU32(F + 4, 0, 0xffffffffu);                                    /* the location id */
    RACE_CORE_CALL(2, (void)rx_ui_session_get_int(0x00936e20 /* "EventLocationId" */, F + 4));
    if (I32(player, 0x1f0) >= 0 && I32(F + 4, 0) != -1) {
        rx_addr table;
        RACE_CORE_CALL(1, table = rx_tbl_find(0x00ae10f0u + U32(player, 0x1f0) * 0x1f4u, 0x009416c0 /* "PlayerCollectables" */));   /* 00402220 */
        if (table != 0) {
            rx_addr row;
            RACE_CORE_CALL(1, row = rx_fx_table_row_by(table, 0x00936e20, F + 4));   /* 00419ae0 (ESI = the table, EAX = the column) */
            if (U32(row, 8) != 0 && I32(row, 0) != -1) {
                rx_addr cell;
                RACE_CORE_CALL(1, cell = rx_fx_row_cell(row, 0x00941870 /* "HitDiscoveryCollectables" */));   /* 004199a0 */
                SU32(args, 0, (1u << (index & 31)) | U32(cell, 0));
                RACE_CORE_CALL(2, rx_tbl_set_cell_int(U32(row, 8), 0x00941870, U32(row, 0), args));   /* 004f6e60 */
            }
        }
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0054d590, rx_free_ride_collectable_found)
