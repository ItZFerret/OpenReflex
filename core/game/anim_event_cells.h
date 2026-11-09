/* anim_event_cells.h -- the growers of an event cell's four lists (anim_event_cells.c). */
#pragma once
#include "rx_game.h"
/* thiscall (ECX = the cell) -> 1 when the list's window grew, else 0. up: 007d5d20 (list 0x38), 007d5d90 (0x44), 007d5e00 (0x20), 007d5e70 (0x2c);
 * down: 007d5ee0, 007d5f90, 007d6040, 007d60f0. */
uint32_t rx_event_cell_grow_up_l38(rx_addr cell);
uint32_t rx_event_cell_grow_up_l44(rx_addr cell);
uint32_t rx_event_cell_grow_up_l20(rx_addr cell);
uint32_t rx_event_cell_grow_up_a(rx_addr cell);
uint32_t rx_event_cell_grow_down_l38(rx_addr cell);
uint32_t rx_event_cell_grow_down_l44(rx_addr cell);
uint32_t rx_event_cell_grow_down_l20(rx_addr cell);
uint32_t rx_event_cell_grow_down_a(rx_addr cell);
