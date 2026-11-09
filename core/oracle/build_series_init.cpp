// build_series_init.cpp -- oracle adapters for core/game/build_series_init.c (group rx_build11).
#include "oracle.h"
extern "C" {
#include "../game/build_series_init.h"
}
using namespace reflex_port;

// 005b14d0 (ESI = the block, EAX = the progress). Void.
RX_PORT(0x005b14d0, SeriesRewardsReset, "rx_build11", 0) { rx_series_rewards_reset(m.r[6], m.r[0]); return m.ret(0); }
// 005b0620 (EAX = the progress). Void.
RX_PORT(0x005b0620, SeriesProgressReset, "rx_build11", 0) { rx_series_progress_reset(m.r[0]); return m.ret(0); }
// 005b1440 (ESI = the block) -> EAX = ESI
RX_PORT(0x005b1440, SeriesRewardsConstruct, "rx_build11", OUT_EAX) { m.r[0] = rx_series_rewards_construct(m.r[6]); return m.ret(0); }
// 005b2370 (cdecl) -> EAX
RX_PORT(0x005b2370, SeriesManagerConstruct, "rx_build11", OUT_EAX) { m.r[0] = rx_series_manager_construct(); return m.ret(0); }
