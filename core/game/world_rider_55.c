/* world_rider_55.c -- players, activities and scenarios as the physics frame reaches them (0x0054xxxx .. 0x005dxxxx).
 *
 * The player table (game +0x7460, `players`): 0x12a0-byte player records from +0 (+0x10 kind: 3 = any), +0xdf8c the slot ->
 * record index map (int each, -1 = empty), +0xdfbc the number of slots (byte). core/notes/world.md (rider). */
#include "world_rider.h"
#include "world_util.h"
#include "world_rider_extern.h"
#include "rider_cap.h"


/* 00552b90 Players_Find (EDX = players; stack index, kind; RET 8): kind 3 = the record in slot `index` (0 when the slot is
 * empty); any other kind = the index-th slot whose record has that kind (0 if there are fewer). The empty-slot test reads slot
 * `index` first in both cases. */
rx_addr rx_world_players_find(rx_addr players, uint32_t index, uint32_t kind) {
    const int32_t first = (int32_t)rx_seat_order(players, index);
    if (first == -1) return 0;
    if (kind == 3) return rx_seat(players, (uint32_t)first);
    const uint32_t slots = U8(players, 0xdfbc);
    uint32_t seen = 0;
    for (uint32_t s = 0; s < slots; s++) {
        const uint32_t rec = rx_seat_order(players, s);
        if (kind == U32(rx_seat(players, rec), 0x10)) {
            if (seen == index) return rx_seat(players, rec);
            seen++;
        }
    }
    return 0;
}
RX_FUNCTION(0x00552b90, rx_world_players_find)
