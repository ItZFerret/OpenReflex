#include "oracle.h"
#include "../game/player_reset_gate.h"
using namespace reflex_port;
RX_PORT(0x0054e470,PlayerCanReset,"rx_PlayerResetGate",OUT_EAX){return m.ret(0,rx_player_reset_input_allowed(m.r[2]));}
