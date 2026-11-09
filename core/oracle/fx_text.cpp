// fx_text.cpp -- oracle adapters for core/game/fx_text.c (group "rx_fx").
#include "fx_oracle.h"
extern "C" {
#include "../game/fx.h"
}
using namespace reflex_port;

FX_PORT(0x00401830, Matrix4_Mul_00401830, OUT_ECX) { rx_fx_matrix4_mul(m.r[1], m.r[0], m.arg(0)); m.r[1] = 0; return m.ret(0); }
FX_PORT(0x004029a0, TextSetWide_004029a0, 0) { rx_fx_text_set_wide(m.r[6], m.arg(0), int32_t(m.arg(1))); return m.ret(8); }
FX_PORT(0x00402970, TextSetWideZ_00402970, 0) { rx_fx_text_set_wide_z(m.arg(0), m.r[2]); return m.ret(4); }
FX_PORT(0x004027e0, TextSet_004027e0, 0) { rx_fx_text_set(m.r[6], m.r[0], m.arg(0)); return m.ret(4); }
FX_PORT(0x00402420, ThreadTableGet_00402420, OUT_EAX) { m.r[0] = rx_fx_thread_table_get(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
