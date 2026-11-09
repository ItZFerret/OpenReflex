// fx_rdb.cpp -- oracle adapters for core/game/fx_rdb.c (group "rx_fx").
#include "fx_oracle.h"
extern "C" {
#include "../game/fx.h"
}
using namespace reflex_port;

FX_PORT(0x00417da0, StrMapLowerBound_00417da0, OUT_EAX) { m.r[0] = rx_fx_strmap_lower_bound(m.r[0], m.arg(0)); return m.ret(4); }
FX_PORT(0x00416e40, StrMapFind_00416e40, OUT_EAX) { m.r[0] = rx_fx_strmap_find(m.r[6], m.arg(0), m.arg(1)); return m.ret(8); }
FX_PORT(0x00417df0, RowSetNext_00417df0, 0) { rx_fx_rowset_next(m.r[6]); return m.ret(0); }
FX_PORT(0x00418e00, CritIterAdd_00418e00, OUT_EAX) { m.r[0] = rx_fx_crit_iter_add(m.r[6], int32_t(m.arg(0))); return m.ret(4); }
FX_PORT(0x00419ea0, IntSetMapFind_00419ea0, OUT_EAX) { m.r[0] = rx_fx_intsetmap_find(m.r[7], m.arg(0), m.arg(1)); return m.ret(8); }
FX_PORT(0x00419ae0, TableRowBy_00419ae0, OUT_EAX) { m.r[0] = rx_fx_table_row_by(m.r[6], m.r[0], m.arg(0)); return m.ret(4); }
FX_PORT(0x004199a0, RowCell_004199a0, OUT_EAX) { m.r[0] = rx_fx_row_cell(m.r[0], m.arg(0)); return m.ret(4); }
FX_PORT(0x00419a70, CritInitInt_00419a70, OUT_EAX) { m.r[0] = rx_fx_crit_init_int(m.r[6], m.r[0], m.r[1], m.arg(0)); return m.ret(4); }
FX_PORT(0x004f73b0, CritInitFloat_004f73b0, OUT_EAX) { m.r[0] = rx_fx_crit_init_float(m.r[6], m.r[0], m.r[1], m.arg(0)); return m.ret(4); }
FX_PORT(0x00415320, CritDestroy_00415320, 0) { rx_fx_crit_destroy(m.r[6]); return m.ret(0); }
FX_PORT(0x00417590, CritVecDestroy_00417590, 0) { rx_fx_crit_vec_destroy(m.r[6]); return m.ret(0); }
FX_PORT(0x004021d0, TableRows_004021d0, OUT_EAX) { m.r[0] = rx_fx_table_rows(m.r[0]); return m.ret(0); }
FX_PORT(0x00401ea0, ModelEntity_vf9, 0) { rx_fx_model_entity_vf9(m.r[1]); return m.ret(0); }
FX_PORT(0x004172e0, ConVarBool_vf2, OUT_EAX) { m.r[0] = rx_fx_convar_bool_toggle(m.r[1], m.arg(0)); return m.ret(4); }
// 004f3320 (stdcall p; RET 4): the allocator table made, the block freed
FX_PORT(0x004f3320, TblFree_004f3320, 0) { rx_tbl_free(m.arg(0)); return m.ret(4); }
// 004eee20 (ESI = a char vector; plain ret) -> EAX = ESI
FX_PORT(0x004eee20, CharVectorFree_004eee20, OUT_EAX) { m.r[0] = rx_fx_char_vector_free(m.r[6]); return m.ret(0); }
// 004eedc0 (EDI = a value variant; plain ret)
FX_PORT(0x004eedc0, VariantDestroy_004eedc0, 0) { rx_fx_variant_destroy(m.r[7]); return m.ret(0); }
// 0041ac70 (cdecl begin, end, ecx; plain ret)
FX_PORT(0x0041ac70, CritRangeDestroy_0041ac70, 0) { rx_fx_crit_range_destroy(m.arg(0), m.arg(1)); return m.ret(0); }
