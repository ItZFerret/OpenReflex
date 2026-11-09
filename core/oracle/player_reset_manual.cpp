#include "oracle.h"
#include "../game/player_reset_manual.h"
using namespace reflex_port;
RX_PORT(0x00550400,PlayerResetManual,"rx_PlayerResetManual",0){rx_player_reset_manual(m.r[6]);return m.ret(0);}

RX_PORT(0x00550a20,PlayerResetOutOfBounds,"rx_PlayerResetManual",0){rx_player_reset_out_of_bounds(m.r[6]);return m.ret(0);}
