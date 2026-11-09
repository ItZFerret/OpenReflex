#include "oracle.h"
extern "C" {
#include "../game/vehicle_atv_methods.h"
#include "../game/vehicle_atv_place.h"
}
using namespace reflex_port;
RX_PORT(0x0048cde0,Atv_ApplyTuning,"rx_AtvMethods",0){rx_sim_atv_apply_tuning(m.r[1]);return m.ret(0);}
RX_PORT(0x0049c8d0,Atv_Reset,"rx_AtvMethods",0){rx_sim_atv_reset(m.r[1],m.arg(0),m.arg(1));return m.ret(8);}

RX_PORT(0x004997f0,Atv_UpdateFrames,"rx_AtvMethods",0){rx_sim_atv_update_frames(m.r[1]);return m.ret(0);}

RX_PORT(0x004a1a10,Atv_UpdateProbes,"rx_AtvMethods",0){rx_sim_atv_update_probes(m.r[1]);return m.ret(0);}

RX_PORT(0x004862e0,Atv_ResetGround,"rx_AtvMethods",0){rx_atv_reset_ground(m.r[1]);return m.ret(0);}
