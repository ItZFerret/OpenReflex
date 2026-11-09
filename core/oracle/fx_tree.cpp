// fx_tree.cpp -- oracle adapters for core/game/fx_tree.c (group "rx_fx").
#include "fx_oracle.h"
extern "C" {
#include "../game/fx.h"
}
using namespace reflex_port;

FX_PORT(0x00608390, MapSize_00608390, OUT_EAX) { m.r[0] = rx_fx_map_size(m.r[1]); return m.ret(0); }
FX_PORT(0x00593dc0, TreeIterator_Increment, 0) { rx_fx_tree_next(m.r[6]); return m.ret(0); }
FX_PORT(0x0063bf50, TreeIterator_Decrement_0063bf50, 0) { rx_fx_tree_prev(m.r[6]); return m.ret(0); }
FX_PORT(0x004fd0c0, TreeIterator_Equal_004fd0c0, OUT_EAX) { m.r[0] = rx_fx_tree_iter_eq(m.r[7], m.r[6]); return m.ret(0); }
FX_PORT(0x00851f50, Tree_Lrotate_00851f50, 0) { rx_fx_tree_lrotate(m.r[1], m.arg(0)); return m.ret(4); }
FX_PORT(0x005bb590, Tree_Rrotate_005bb590, 0) { rx_fx_tree_rrotate(m.r[1], m.arg(0)); return m.ret(4); }
FX_PORT(0x005bac00, Tree_NodeNew_005bac00, OUT_EAX) { m.r[0] = rx_fx_tree_node_new(m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4)); return m.ret(0x14); }
FX_PORT(0x005ba570, Tree_InsertAt_005ba570, OUT_EAX) { m.r[0] = rx_fx_tree_insert_at(m.r[7], m.r[0], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
FX_PORT(0x005ba210, Tree_InsertUnique_005ba210, OUT_EAX) { m.r[0] = rx_fx_tree_insert_unique(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
FX_PORT(0x006cd4a0, Map_InsertHint_006cd4a0, OUT_EAX) { m.r[0] = rx_fx_map_insert_hint(m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4)); return m.ret(0x14); }
