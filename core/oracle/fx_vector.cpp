// fx_vector.cpp -- oracle adapters for core/game/fx_vector.c (group "rx_fx").
#include "fx_oracle.h"
extern "C" {
#include "../game/fx.h"
}
using namespace reflex_port;

FX_PORT(0x0042bf30, VectorAlloc_0042bf30, OUT_EAX) { m.r[0] = rx_fx_vector_alloc(m.r[1]); return m.ret(0); }
FX_PORT(0x00776d20, VectorUmove_00776d20, OUT_EAX) { m.r[0] = rx_fx_vector_umove(m.r[0], m.r[7], m.r[2]); return m.ret(0); }   // ECX: memmove_s's residue (not an output)
FX_PORT(0x007cfd80, VectorFill_007cfd80, OUT_EAX) { m.r[0] = rx_fx_vector_fill(m.r[2], m.r[1], m.arg(0)); return m.ret(0); }
FX_PORT(0x006c8260, VectorInsertN_006c8260, 0) { rx_fx_vector_insert_n(m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4)); return m.ret(0x14); }
