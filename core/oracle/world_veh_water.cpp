// world_veh_water.cpp -- oracle adapters for core/game/world_veh_water.c (group rx_world).
#include "oracle.h"
extern "C" { uint32_t rx_vector_int_construct16(uint32_t vec, uint32_t value_p); uint32_t rx_water_heights_init(void); }
using namespace reflex_port;

// 0042b410 (stdcall vector, &value; RET 8)
RX_PORT(0x0042b410, VectorIntConstruct16, "rx_world", 0) { rx_vector_int_construct16(m.arg(0), m.arg(1)); return m.ret(8); }
// 004242f0 (cdecl) -> EAX
RX_PORT(0x004242f0, WaterHeightsInit, "rx_world", OUT_EAX) { m.r[0] = rx_water_heights_init(); return m.ret(0); }
