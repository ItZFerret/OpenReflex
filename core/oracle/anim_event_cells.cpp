// anim_event_cells.cpp -- oracle adapters for core/game/anim_event_cells.c (group rx_anim). thiscall (ECX = the cell) -> EAX.
#include "anim_oracle.h"
extern "C" {
#include "../game/anim_event_cells.h"
}
using namespace reflex_port;

ANIM_PORT(0x007d5d20, EventCellGrowUp_l38, "rx_anim", OUT_EAX) { m.r[0] = rx_event_cell_grow_up_l38(m.r[1]); return m.ret(0); }
ANIM_PORT(0x007d5d90, EventCellGrowUp_l44, "rx_anim", OUT_EAX) { m.r[0] = rx_event_cell_grow_up_l44(m.r[1]); return m.ret(0); }
ANIM_PORT(0x007d5e00, EventCellGrowUp_l20, "rx_anim", OUT_EAX) { m.r[0] = rx_event_cell_grow_up_l20(m.r[1]); return m.ret(0); }
ANIM_PORT(0x007d5e70, EventCellGrowUp_a, "rx_anim", OUT_EAX) { m.r[0] = rx_event_cell_grow_up_a(m.r[1]); return m.ret(0); }
ANIM_PORT(0x007d5ee0, EventCellGrowDown_l38, "rx_anim", OUT_EAX) { m.r[0] = rx_event_cell_grow_down_l38(m.r[1]); return m.ret(0); }
ANIM_PORT(0x007d5f90, EventCellGrowDown_l44, "rx_anim", OUT_EAX) { m.r[0] = rx_event_cell_grow_down_l44(m.r[1]); return m.ret(0); }
ANIM_PORT(0x007d6040, EventCellGrowDown_l20, "rx_anim", OUT_EAX) { m.r[0] = rx_event_cell_grow_down_l20(m.r[1]); return m.ret(0); }
ANIM_PORT(0x007d60f0, EventCellGrowDown_a, "rx_anim", OUT_EAX) { m.r[0] = rx_event_cell_grow_down_a(m.r[1]); return m.ret(0); }
