// loop_jobpool.cpp -- oracle adapters for core/game/loop_jobpool.c (group "rx_loop").
#include "fx_oracle.h"
extern "C" {
#include "../game/fx.h"
}
using namespace reflex_port;

FX_PORT(0x004dc090, JobPool_ElementCtor_004dc090, OUT_EAX) { m.r[0] = rx_jobpool_element_ctor(m.r[1]); return m.ret(0); }
FX_PORT(0x00881a70, JobPool_Construct_00881a70, OUT_EAX) { m.r[0] = rx_jobpool_construct(m.r[1]); return m.ret(0); }
