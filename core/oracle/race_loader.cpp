// race_loader.cpp -- oracle adapters for core/game/race_loader.c.
#include "oracle.h"
extern "C" {
#include "../game/race.h"
}
using namespace reflex_port;

RX_PORT(0x0053cb80, Loader_PartsReady, "rx_race", OUT_EAX) { m.r[0] = uint32_t(rx_loader_parts_ready(m.arg(0))); return m.ret(4); }
// 0053cc90 (ESI = loader, EAX = cache owner, arg 0 = dt; RET 4).
RX_PORT(0x0053cc90, Loader_Update, "rx_race", 0) { rx_loader_update(m.r[6], m.r[0], rx_oracle::f32(m.arg(0))); return m.ret(4); }
