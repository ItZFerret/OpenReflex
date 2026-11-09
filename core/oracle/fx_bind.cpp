// fx_bind.cpp -- oracle adapters for core/game/fx_bind.c (group "rx_fx").
#include "fx_oracle.h"
extern "C" {
#include "../game/fx_bind.h"
}
using namespace reflex_port;

FX_PORT(0x007e3400, EffectIndexByName_007e3400, OUT_EAX) { m.r[0] = (uint32_t)rx_fx_effect_index_by_name(m.r[1], m.arg(0)); return m.ret(4); }
FX_PORT(0x0069b0e0, EffectBind_0069b0e0, 0) { rx_fx_effect_bind(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
