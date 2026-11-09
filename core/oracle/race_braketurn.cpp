#include "oracle.h"
extern "C" {
#include "../game/race_braketurn.h"
}
using namespace reflex_port;
RX_PORT(0x00522eb0,BrakeTurn_Enable,"rx_BrakeTurn",0){rx_brake_turn_enable(m.r[1],m.arg(0));return m.ret(4);}
RX_PORT(0x00522fc0,BrakeTurn_Steer,"rx_BrakeTurn",OUT_EAX){m.r[0]=rx_brake_turn_steer(m.r[6]);return m.ret(0);}
RX_PORT(0x00523230,BrakeTurn_Clutch,"rx_BrakeTurn",OUT_EAX){m.r[0]=rx_brake_turn_clutch(m.r[1]);return m.ret(0);}
RX_PORT(0x005232a0,BrakeTurn_WaitPullback,"rx_BrakeTurn",OUT_EAX){m.r[0]=rx_brake_turn_wait_pullback(m.r[2]);return m.ret(0);}
RX_PORT(0x00523300,BrakeTurn_Pullback,"rx_BrakeTurn",OUT_EAX){m.r[0]=rx_brake_turn_pullback(m.r[1]);return m.ret(0);}
