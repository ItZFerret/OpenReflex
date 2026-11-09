#include "oracle.h"
extern "C" {
#include "../game/vehicle_atv_controls.h"
}
using namespace reflex_port;
RX_PORT(0x00497a70,Atv_ControlUpdate,"rx_AtvControls",0){rx_atv_control_update(m.r[1],rx_oracle::f32(m.arg(0)));return m.ret(4);}
RX_PORT(0x00494910,Atv_AirControl,"rx_AtvControls",0){rx_atv_air_control(m.arg(0),rx_oracle::f32(m.arg(1)));return m.ret(8);}
RX_PORT(0x00496880,Atv_GroundControl,"rx_AtvControls",0){rx_atv_ground_control(m.r[6],rx_oracle::f32(m.arg(0)));return m.ret(4);}
