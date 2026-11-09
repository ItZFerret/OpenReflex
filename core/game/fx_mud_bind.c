/* fx_mud_bind.c -- MudAndNumberCallback::vf0 0054a2e0 (the callback the player's rider spawn runs once the rider is inserted: the rider model
 * handle goes to the player's +0x1b4, the number plate textures are bound to the rider's texture override map and a mud record is bound to
 * the vehicle), MXPlayer_BindNamePlates 0054bfd0, Model_OverrideTexture 005510a0, MudRecord_Alloc 00408fb0 and MudRecord_Bind 004089d0.
 * Mud records: 12 of 0x40 bytes at 0xa96d90 {+0 amount (x4), +0x10 wetness (x4), +0x20 colour, +0x30 the owner vehicle}; the owner's
 * (and its rider's) shader-constant override lists get three entries (name 0x40 bytes, kind 1, pointer to the record's field).
 * core/notes/fx.md. */
#include "fx.h"
#include "rider_cap.h"
#include "fx_mud_bind.h"
#include "crt.h"
#include "build_vehicle.h"
#include "build_rider_util.h"

/* one override entry on a list: names at base + 0x40 n, kinds at base + 0x400 + 4 n, pointers at base + 0x440 + 4 n, the count byte at
 * base + 0x940 (base = vehicle + 0x6c0 or rider + 0x5bb90) */
static void add_override(rx_addr base, rx_addr src_name, rx_addr value) {
    const uint32_t n = U8(base, 0x940);
    const rx_addr nm = base + (n << 6);
    crt_strncpy(nm, src_name, 0x3f);
    SU8(nm, 0x3f, 0);
    SU32(base, 0x440 + 4u * U8(base, 0x940), value);
    SU32(base, 0x400 + 4u * U8(base, 0x940), 1);
    SU8(base, 0x940, (uint8_t)(U8(base, 0x940) + 1));
}

/* 004089d0 (EDI = the record, EAX = the vehicle V): owner = V; MUD_COLOR -> record + 0x20, MUD_Amount -> record, MUD_Wetness -> record + 0x10
 * on V's list and on V's rider's (V + 0x1a0); then amount and wetness cleared and the colour (0.2559, 0.1706, 0.1176, 1) */
void rx_mud_record_bind(rx_addr rec, rx_addr veh) {
    SU32(rec, 0x30, veh);
    if (veh != 0) {
        add_override(veh + 0x6c0, 0x009c6180, rec + 0x20);
        add_override(U32(rec, 0x30) + 0x6c0, 0x009c6078, rec);
        add_override(U32(rec, 0x30) + 0x6c0, 0x009c6288, rec + 0x10);
        rx_addr rider = U32(U32(rec, 0x30), 0x1a0);
        if (rider != 0) {
            add_override(rider + 0x5bb90, 0x009c6180, rec + 0x20);
            add_override(U32(U32(rec, 0x30), 0x1a0) + 0x5bb90, 0x009c6078, rec);
            add_override(U32(U32(rec, 0x30), 0x1a0) + 0x5bb90, 0x009c6288, rec + 0x10);
        }
    }
    for (uint32_t k = 0; k < 8; k++) SF32(rec, 4 * k, 0.0f);
    SU32(rec, 0x20, 0x3e830165u /* 0x9777c0 */); SU32(rec, 0x24, 0x3e2eac86u /* 0x9777bc */); SU32(rec, 0x28, 0x3df0ed3eu /* 0x9777b8 */);
    SU32(rec, 0x2c, 0x3f800000u /* 0x931f24 */);
}
RX_FUNCTION(0x004089d0, rx_mud_record_bind)

/* 00408fb0 (stack: the vehicle): the first of the 12 records whose owner (+0x30) is clear gets the vehicle */
void rx_mud_record_alloc(rx_addr veh) {
    for (rx_addr owner = 0x00a96dc0u; owner < 0x00a970c0u; owner += 0x40u) {
        if (U32(owner, 0) == 0) { rx_mud_record_bind(owner - 0x30u, veh); return; }
    }
    for (uint32_t k = RX_RC_ORIG; k < (rx_rc_more() ? RX_RC_MAX : 0u); k++) {   /* (w-rc-core) the 12 taken (13+ players): records 12..31 (rider_cap.h) */
        if (U32(rx_mud_rec(k), 0x30) == 0) { rx_rc_tab_mark(RX_MUD, k); rx_mud_record_bind(rx_mud_rec(k), veh); return; }
    }
}
RX_FUNCTION(0x00408fb0, rx_mud_record_alloc)

/* 005510a0 (cdecl: the texture override map, the texture name, the texture asset): the asset's handle by name ("texture", 0, 0: Database vf27),
 * its loading mark raised, then map[the asset's key (the key array +0x10 at index +0x18)] = tex and the mark cleared. The frame: ebp,
 * and esp -8, push ecx (the key slot), push esi */
void rx_model_override_texture(rx_addr map, rx_addr name, rx_addr tex) {
    const rx_addr E = rx_sp(), A = (E - 4u) & ~7u, key = A - 4u;
    rx_set_sp(A - 8u - 16u - 4u);
    uint32_t a[4] = {name, 0x74786574u, 0x657275u, 0};
    const rx_addr h = rr_vcall(rr_database(), 0x6c, 4, a);
    if (h != 0) {
        const rx_addr asset = U32(h, 0x44);
        SU32(h, 0x4c, 1);
        if (asset != 0) {
            SU32(key, 0, U32(U32(asset, 0x10), 4u * U32(asset, 0x18)));
            rx_set_sp(A - 16u - 4u);
            const rx_addr p = rx_uint_map_index(map, key);
            SU32(p, 0, tex);
            SU32(h, 0x4c, 0);
        }
    }
    rx_set_sp(E);
}
RX_FUNCTION(0x005510a0, rx_model_override_texture)

/* 0054bfd0 (EDI = the player): the rider model's override map ([P + 0x1b4] + 0x5bb6c): "Rider_NamNum_Plate" <- P + 0xb24,
 * "Rider_CP_NamNum_Plate" <- P + 0xb28 */
void rx_player_bind_name_plates(rx_addr P) {
    if (U32(P, 0x1b4) != 0) {
        const rx_addr E = rx_sp(), A = (E - 4u) & ~7u, map = U32(P, 0x1b4) + 0x5bb6c;
        rx_set_sp(A - 8u - 12u - 4u);
        rx_model_override_texture(map, 0x00941724, U32(P, 0xb24));
        rx_model_override_texture(map, 0x00941738, U32(P, 0xb28));
        rx_set_sp(E);
    }
}
RX_FUNCTION(0x0054bfd0, rx_player_bind_name_plates)

/* 0054a2e0 MudAndNumberCallback::vf0 (ECX = the callback {+4 the player}; stack: the rider model): when a player is set: P + 0x1b4 = the rider,
 * the plates, a mud record for P + 0x1ac (the vehicle) */
void rx_mud_and_number_callback(rx_addr self, uint32_t rider) {
    if (U32(self, 4) == 0) return;
    const rx_addr E = rx_sp(), P = U32(self, 4);
    SU32(P, 0x1b4, rider);
    rx_set_sp(E - 8u - 4u);                    /* push esi, push edi; the call */
    rx_player_bind_name_plates(P);
    rx_set_sp(E - 8u - 4u - 4u);               /* push eax; the call */
    rx_mud_record_alloc(U32(U32(self, 4), 0x1ac));
    rx_set_sp(E);
}
RX_FUNCTION(0x0054a2e0, rx_mud_and_number_callback)
