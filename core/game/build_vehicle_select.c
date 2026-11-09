/* build_vehicle_select.c -- App.ChangeVehicle 005a0670, MXTableHelper_EquipVehicle 004e7930, MXTableHelper_GetEquippedVehicle 004e7c60
 * (build_vehicle_select.h; w-tutcore). Transliterated from the disassembly; frames at the original's stack addresses (the criteria vectors
 * and row sets put stack addresses into the heap blocks they allocate). */
#include "build_vehicle_select.h"
#include "build_activity_util.h"
#include "fx.h"
#include "loop_player.h"

#define PLAYER_LIST   0x00ae0e08u
#define VEHICLE_LIST  0x00ae05c8u
#define RIDER_LIST    0x00ae19b0u
#define DEFAULT_DB    0x00ae0910u
#define PL_CHANGES    0x00ae0e70u   /* the Player List's change counter (RequestedVehicleId written with another value) */

#define S_REQ_VEHICLE  0x0093df5cu  /* "RequestedVehicleId" */
#define S_VEHICLE_ID   0x00936cb4u  /* "VehicleId" */
#define S_REQ_RIDER    0x0093df70u  /* "RequestedRiderId" */
#define S_RIDER_ID     0x0093dec8u  /* "RiderId" */
#define S_PLAYER_TYPE  0x00936ef4u  /* "PlayerType" */
#define S_CONTROLLER   0x0093ded0u  /* "Controller" */

/* Table_ColumnIndex + 0041aaf0: an int cell of a row by column name */
static uint32_t cell(rx_addr table, rx_addr name, uint32_t row) {
    uint32_t col;
    ACT_CALLR(col, 1, rx_tbl_column_index(table, name));                /* 00402160 */
    rx_addr p;
    ACT_CALLR(p, 1, rx_tbl_cell_word(table, col, row));                 /* 0041aaf0 */
    return U32(p, 0);
}
/* Table_ColumnIndex and the column vector's checked subscript (_invalid_parameter_noinfo when out of range) */
static rx_addr column(rx_addr table, rx_addr name) {
    uint32_t c;
    ACT_CALLR(c, 1, rx_tbl_column_index(table, name));
    if (c >= (U32(table, 0x14) - U32(table, 0x10)) >> 2) crt_invalid_parameter_noinfo();
    return U32(U32(table, 0x10), 4 * c);
}
/* one int criterion built in the frame's slot (00419a70: ESI = crit, EAX = 0 the operator, ECX = &value; the name), pushed (004175e0: EDI =
 * vector) and destroyed (00415320: ESI) */
static void crit(rx_addr vec, rx_addr c, rx_addr name, rx_addr valp) {
    ACT_CALL(1, (void)rx_fx_crit_init_int(c, 0, valp, name));
    ACT_CALL(1, rx_tbl_crit_push(vec, c));
    ACT_CALL(0, rx_fx_crit_destroy(c));
}

/* ---- 004e7930 ------------------------------------------------------------------------------------------------------------------ */

/* MXTableHelper_EquipVehicle (EAX = owner; stdcall row, owner vehicle id; RET 8). The row's RequestedVehicleId before (kept at +0x10; -1 when
 * it is the row's VehicleId); the new Vehicle List row in the first argument's slot: owners 0..3 VehicleList_AddRow( owner, id ) and the
 * player's PlayerRider.LastEquippedVehicle (Table_SetCellInt row 0, &the id's slot), 4 / 8 VehicleList_AddRow( 4 / 8, id ), others -1; the
 * change counter when it differs from the row's RequestedVehicleId; RequestedVehicleId = it (004f5330); the old requested row (its VehicleId
 * index, 00418ec0) erased by a tail call of 004ece50( its table, its row ) unless -1 or the new row.
 * Frame: sub esp, 0x14; push ebx / ebp / esi / edi. */
void rx_table_equip_vehicle(uint32_t owner, uint32_t row, uint32_t owner_vehicle_id) {
    const rx_addr entry = rx_sp(), args = entry + 4;
    ACT_FRAME(L, 0x14, 4);
    SU32(args, 0, row); SU32(args, 4, owner_vehicle_id);
    uint32_t old = cell(PLAYER_LIST, S_REQ_VEHICLE, row);
    SU32(L, 0x10, old);
    if (old == cell(PLAYER_LIST, S_VEHICLE_ID, row)) { old = 0xffffffffu; SU32(L, 0x10, old); }
    SU32(args, 0, 0xffffffffu);                                         /* the new row (the first argument's slot) */
    switch (owner) {
    case 0: case 1: case 2: case 3: {
        uint32_t r;
        ACT_CALLR(r, 2, rx_tbl_vehicle_list_add_row(owner, U32(args, 4)));   /* 004de810 */
        SU32(args, 0, r);
        rx_addr t;
        ACT_CALLR(t, 1, rx_tbl_find(0x00ae10f0u + owner * 0x1f4u, 0x0093e53c /* "PlayerRider" */));   /* 00402220 */
        ACT_CALL(2, rx_tbl_set_cell_int(t, 0x0093e548 /* "LastEquippedVehicle" */, 0, args + 4));    /* 004f6e60 */
        break;
    }
    case 4: case 8: {
        uint32_t r;
        ACT_CALLR(r, 2, rx_tbl_vehicle_list_add_row(owner, U32(args, 4)));
        SU32(args, 0, r);
        break;
    }
    default: break;
    }
    rx_addr col;
    {
        uint32_t c;
        ACT_CALLR(c, 1, rx_tbl_column_index(PLAYER_LIST, S_REQ_VEHICLE));
        rx_addr p;
        ACT_CALLR(p, 1, rx_tbl_cell_word(PLAYER_LIST, c, row));
        const uint32_t now = U32(args, 0);
        if (now != U32(p, 0)) SU32(0, PL_CHANGES, U32(0, PL_CHANGES) + 1);
        if (c >= (U32(PLAYER_LIST, 0x14) - U32(PLAYER_LIST, 0x10)) >> 2) crt_invalid_parameter_noinfo();
        col = U32(U32(PLAYER_LIST, 0x10), 4 * c);
    }
    ACT_CALL(3, rx_tbl_set_int(col, row, args));                        /* 004f5330 (column, row, &the new row) */
    const uint32_t now = U32(args, 0);
    if (old == 0xffffffffu || old == now) { ACT_END(L); return; }
    const rx_addr vcol = column(VEHICLE_LIST, S_VEHICLE_ID);
    rx_addr ref;
    ACT_CALLR(ref, 1, rx_tbl_row(vcol, (int32_t)old));                  /* 00418ec0 (EAX = &the old id at +0x10) */
    const rx_addr table = U32(ref, 8);
    SU32(L, 0x14, U32(ref, 0)); SU32(L, 0x18, U32(ref, 4));             /* movq [esp + 0x14] */
    const uint32_t erow = U32(L, 0x14);
    ACT_END(L);
    SU32(args, 4, erow); SU32(args, 0, table);                          /* the tail call's arguments in this call's slots */
    rx_tbl_erase_row(table, erow);                                      /* jmp 004ece50 (RET 8 returns to our caller) */
}
RX_FUNCTION(0x004e7930, rx_table_equip_vehicle)

/* ---- 004e7c60 ------------------------------------------------------------------------------------------------------------------ */

/* MXTableHelper_GetEquippedVehicle (EAX = row; stdcall &owner vehicle id, &owner; RET 8) -> 1. For a row (not -1): its RequestedVehicleId, -1
 * -> its VehicleId (kept in the push ecx slot); that Vehicle List row (the VehicleId index, 00418ec0): *owner = its VehicleOwner, *owner
 * vehicle id = its OwnerVehicleId. Frame: push ecx; push esi; (row) push ebx / ebp / edi. */
uint32_t rx_table_get_equipped_vehicle(uint32_t row, rx_addr owner_vehicle_id, rx_addr owner) {
    const rx_addr entry = rx_sp();
    const rx_addr slot = entry - 4;                                     /* the push ecx slot: the vehicle id */
    if (row == 0xffffffffu) return 1;
    ACT_FRAME(L, 8, 3);
    uint32_t id = cell(PLAYER_LIST, S_REQ_VEHICLE, row);
    SU32(slot, 0, id);
    if (id == 0xffffffffu) { id = cell(PLAYER_LIST, S_VEHICLE_ID, row); SU32(slot, 0, id); }
    const rx_addr vcol = column(VEHICLE_LIST, S_VEHICLE_ID);
    rx_addr ref;
    ACT_CALLR(ref, 1, rx_tbl_row(vcol, (int32_t)U32(slot, 0)));
    SU32(owner, 0, cell(U32(ref, 8), 0x0093df84 /* "VehicleOwner" */, U32(ref, 0)));
    SU32(owner_vehicle_id, 0, cell(U32(ref, 8), 0x0093df94 /* "OwnerVehicleId" */, U32(ref, 0)));
    ACT_END(L);
    return 1;
}
RX_FUNCTION(0x004e7c60, rx_table_get_equipped_vehicle)

/* ---- 005a0670 ------------------------------------------------------------------------------------------------------------------ */

/* App.ChangeVehicle's native (stdcall row, vehicle type; RET 8; Lua binding 0072e690). The row's RequestedVehicleId / VehicleId are read (and
 * dropped), its rider (RequestedRiderId, else RiderId) kept at +0x10; owner 9 / id -1 at +0x20 / +0x24; the default database's AIRiderVehicle
 * at +0x28. By the row's PlayerType:
 *   1 (AI): the rider's Rider List row (its RiderId index) -> OwnerRiderId (+0x10); the first AIRiderVehicle with VehicleTypeId = the type
 *     and AIRiderId = that rider: owner 4, its AIVehicleId.
 *   0 (human): the row's Controller; the first row of that player's PlayerVehicle with Favorite = 1 and VehicleModelId.VehicleTypeId = the
 *     type: owner = the controller, its PlayerVehicleId; none: the PresetVehicle rows of VehicleModelId.VehicleTypeId = the type, the first
 *     one: owner 8, its PresetVehicleId (no row: owner 9, -1).
 * Then MXTableHelper_EquipVehicle( owner, row, id ). Frame: sub esp, 0x114; push ebx / ebp / esi / edi: the found row reference at +0x14,
 * the criteria vector at +0x30, the row set at +0x48, the criterion at +0x70, the arguments at +0x128 / +0x12c. */
void rx_change_vehicle(uint32_t row, uint32_t type) {
    const rx_addr args = rx_sp() + 4;
    ACT_FRAME(L, 0x114, 4);
    SU32(args, 0, row); SU32(args, 4, type);                            /* the criteria read the type in its slot (+0x12c) */
    const rx_addr out = L + 0x14, vec = L + 0x30, set = L + 0x48, c = L + 0x70;
    (void)cell(PLAYER_LIST, S_REQ_VEHICLE, row);
    (void)cell(PLAYER_LIST, S_VEHICLE_ID, row);
    SU32(L, 0x10, cell(PLAYER_LIST, S_REQ_RIDER, row));
    if (U32(L, 0x10) == 0xffffffffu) SU32(L, 0x10, cell(PLAYER_LIST, S_RIDER_ID, row));
    SU32(L, 0x20, 9);                                                   /* the owner: none */
    SU32(L, 0x24, 0xffffffffu);
    {
        rx_addr arv;
        ACT_CALLR(arv, 1, rx_tbl_find(DEFAULT_DB, 0x0094cf60 /* "AIRiderVehicle" */));
        SU32(L, 0x28, arv);
    }
    const uint32_t player_type = cell(PLAYER_LIST, S_PLAYER_TYPE, row);
    if (player_type == 1) {
        rx_addr airider;
        ACT_CALLR(airider, 1, rx_tbl_find(DEFAULT_DB, 0x0094063c /* "AIRider" */));
        const rx_addr rcol = column(RIDER_LIST, S_RIDER_ID);
        rx_addr ref;
        ACT_CALLR(ref, 1, rx_tbl_row(rcol, I32(L, 0x10)));               /* 00418ec0 (EAX = &the rider at +0x10) */
        const rx_addr rt = U32(ref, 8);
        SU32(L, 0x14, U32(ref, 0)); SU32(L, 0x18, U32(ref, 4));         /* movq [esp + 0x14] */
        SU32(L, 0x10, cell(rt, 0x0093dfb0 /* "OwnerRiderId" */, U32(L, 0x14)));
        const rx_addr acol = column(airider, 0x0094cf70 /* "AIRiderId" */);
        ACT_CALL(1, (void)rx_tbl_row(acol, I32(L, 0x10)));              /* (the reference is dropped) */
        {   /* the vector's proxy block (ReflexAlignedAlloc( 4 ), cdecl) */
            rx_addr px;
            ACT_CALLR(px, 1, rx_reflex_aligned_alloc(4));
            if (px) { SU32(px, 0, vec); SU32(vec, 0, px); } else SU32(vec, 0, 0);
        }
        SU32(vec, 0xc, 0); SU32(vec, 0x10, 0); SU32(vec, 0x14, 0);
        crit(vec, c, 0x00936ce4 /* "VehicleTypeId" */, args + 4);
        crit(vec, c, 0x0094cf70 /* "AIRiderId" */, L + 0x10);
        SU32(out, 0, 0xffffffffu); SU32(out, 8, 0);
        ACT_CALL(3, rx_tbl_find_first(U32(L, 0x28), vec, out));         /* 004ed410 */
        SU32(L, 0x24, cell(U32(out, 8), 0x0094cf7c /* "AIVehicleId" */, U32(out, 0)));
        SU32(L, 0x20, 4);
        ACT_CALL(0, rx_fx_crit_vec_destroy(vec));                       /* 00417590 */
    } else if (player_type == 0) {
        const uint32_t controller = cell(PLAYER_LIST, S_CONTROLLER, row);
        rx_addr pv;
        ACT_CALLR(pv, 1, rx_tbl_find(0x00ae10f0u + controller * 0x1f4u, 0x0093e568 /* "PlayerVehicle" */));
        SU32(L, 0x10, pv);
        ACT_CALL(0, (void)rx_list_init(vec));                           /* 005442c0 (ESI = the vector) */
        SU32(out, 0, 0xffffffffu); SU32(out, 8, 0);
        SU32(L, 0x28, 1);                                               /* Favorite = 1 (the value's slot) */
        crit(vec, c, 0x0093e5a8 /* "Favorite" */, L + 0x28);
        crit(vec, c, 0x0093e588 /* "VehicleModelId.VehicleTypeId" */, args + 4);
        ACT_CALL(3, rx_tbl_find_first(pv, vec, out));
        if (U32(out, 8) != 0 && U32(out, 0) != 0xffffffffu) {
            SU32(L, 0x24, cell(U32(out, 8), 0x0093e578 /* "PlayerVehicleId" */, U32(out, 0)));
            SU32(L, 0x20, controller);
        } else {
            rx_addr preset;
            ACT_CALLR(preset, 1, rx_tbl_find(DEFAULT_DB, 0x00940734 /* "PresetVehicle" */));
            ACT_CALL(0, rx_tbl_crit_vec_clear(vec));                    /* 004f1790 (EAX = the vector) */
            crit(vec, c, 0x0093e588 /* "VehicleModelId.VehicleTypeId" */, args + 4);
            ACT_CALL(1, rx_tbl_rowset_init(set, 0));                    /* 004dc570 (ESI = the set; 0) */
            ACT_CALL(1, rx_tbl_select(preset, set, vec));               /* 004ed2f0 (ECX = table, EAX = the set; &criteria) */
            if (U32(set, 0x1c) != 0) {
                const rx_addr it = L + 0x28;                            /* the iterator {container, node} */
                SU32(it, 4, U32(U32(set, 0x18), 0));
                SU32(it, 0, U32(set, 0));
                {   /* 004fc5c0 (ESI = the iterator): the checked dereference -> &the node's value */
                    if (U32(it, 0) == 0) crt_invalid_parameter_noinfo();
                    const rx_addr owner_set = U32(it, 0) ? U32(U32(it, 0), 0) : 0;
                    if (U32(it, 4) == U32(owner_set, 0x18)) crt_invalid_parameter_noinfo();
                }
                SU32(out, 0, U32(U32(it, 4), 0xc));                     /* {the row, 0, the PresetVehicle table} */
                SU32(out, 4, 0);
                SU32(out, 8, preset);
                rx_addr p;
                ACT_CALLR(p, 1, rx_fx_row_cell(out, 0x0094cfb4 /* "PresetVehicleId" */));   /* 004199a0 (EAX = the reference) */
                SU32(L, 0x24, U32(p, 0));
                SU32(L, 0x20, 8);
            }
            rx_tbl_rowset_destroy(set);                                 /* 004ebe10 (sorted), the table word, 004f2220, 004dc0f0, the proxy freed */
        }
        ACT_CALL(0, rx_fx_crit_vec_destroy(vec));
    }
    ACT_CALL(2, rx_table_equip_vehicle(U32(L, 0x20), row, U32(L, 0x24)));
    ACT_END(L);
}
RX_FUNCTION(0x005a0670, rx_change_vehicle)
