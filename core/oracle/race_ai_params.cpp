// race_ai_params.cpp -- oracle adapters for core/game/race_ai_params.c.
#include "oracle.h"
extern "C" {
#include "../game/race.h"
}
using namespace reflex_port;

// The getters (EAX = driver) -> xmm0 (upper lanes 0). Explicit pair: ECX = index, EDX = row offset, EAX kept; tier pair: ECX = the
// player, EAX = row offset, EDX = the index.
#define RX_AI_PARAM(addr, Name, block)                                                                       \
    RX_PORT(addr, Name, "rx_race", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM0) {                               \
        const uint32_t d = m.r[0]; uint32_t idx; int expl;                                                  \
        const float v = race_ai_param(d, block, &idx, &expl);                                               \
        const uint32_t row = U32(d, 0xee8) * 0xc7cu;                                                         \
        if (expl) { m.r[1] = idx; m.r[2] = row; } else { m.r[1] = U32(d, 0x74); m.r[0] = row; m.r[2] = idx; } \
        m.xmm[0].lo = rx_oracle::bits(v); m.xmm[0].hi = 0;                                                  \
        return m.ret(0);                                                                                    \
    }
RX_AI_PARAM(0x0051dd20, AiParam_dd20, 0x00a89dc4)
RX_AI_PARAM(0x0051dea0, AiParam_dea0, 0x00a8a0b4)
RX_AI_PARAM(0x0051df40, AiParam_df40, 0x00a8a2e8)
RX_AI_PARAM(0x0051e090, AiParam_e090, 0x00a8a694)
RX_AI_PARAM(0x0051e0e0, AiParam_e0e0, 0x00a8a750)
RX_AI_PARAM(0x0051e130, AiParam_e130, 0x00a8a80c)
RX_AI_PARAM(0x0051e180, AiParam_e180, 0x00a8a8c8)
RX_AI_PARAM(0x0051de50, AiParam_de50, 0x00a8a170)
RX_AI_PARAM(0x0051def0, AiParam_def0, 0x00a8a22c)
// 0051dd70 (ECX = driver, arg 0 = out; cdecl).
// ECX = the player on the difficulty-tier path.
RX_PORT(0x0051dd70, AiParamRange_dd70, "rx_race", OUT_ECX) {
    const uint32_t d = m.r[1];
    rx_ai_param_range_dd70(d, m.arg(0));
    if (I32(U32(d, 0x74), 0x8fc) == -1) m.r[1] = U32(d, 0x74);
    return m.ret(0);
}
RX_PORT(0x0050ac80, AiControls_Off, "rx_race", OUT_ECX) { rx_ai_controls_off(m.r[0]); m.r[1] = 0; return m.ret(0); }
RX_PORT(0x0050acc0, AiControls_On, "rx_race", OUT_ECX) { rx_ai_controls_on(m.r[0]); m.r[1] = 1; return m.ret(0); }
