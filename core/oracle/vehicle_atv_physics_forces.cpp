#include "oracle.h"
extern "C" {
#include "../game/vehicle_atv_physics_forces.h"
}
using namespace reflex_port;
void atv_suspension_diagnostic(reflex_vm::Machine&);
void atv_tire_diagnostic_begin(reflex_vm::Machine&);
RX_PORT(0x00493c70,Atv_Suspension,"rx_AtvPhysics",0){atv_suspension_diagnostic(m);rx_atv_apply_suspension(m.r[7]);return m.ret(0);}
RX_PORT(0x00498110,Atv_PowerGear,"rx_AtvPhysics",0){rx_atv_choose_power_gear(m.r[7]);return m.ret(0);}
RX_PORT(0x00498380,Atv_CoastGear,"rx_AtvPhysics",0){rx_atv_choose_coast_gear(m.r[7]);return m.ret(0);}

RX_PORT(0x004985e0,Atv_Engine,"rx_AtvPhysics",0){rx_atv_update_engine(m.r[7],rx_bf(m.arg(0)));return m.ret(4);}

RX_PORT(0x00499220,Atv_SteerQuaternions,"rx_AtvPhysics",0){rx_atv_steering_quaternions(m.r[6]);return m.ret(0);}

RX_PORT(0x0049a340,Atv_TireContact,"rx_AtvPhysics",0){atv_tire_diagnostic_begin(m);rx_atv_tire_contact_force(m.r[7],rx_bf(m.arg(0)),m.arg(1));return m.ret(8);}
RX_PORT(0x00490b80,Atv_ForceStep,"rx_AtvPhysics",0){rx_atv_step_forces(m.r[1],rx_bf(m.arg(0)));return m.ret(4);}

RX_PORT(0x0049ee30,Atv_ControlStep,"rx_AtvPhysics",0){rx_atv_control_step(m.r[1],rx_bf(m.arg(0)));return m.ret(4);}
RX_PORT(0x004a0ab0,Atv_RiderOffControl,"rx_AtvPhysics",0){rx_atv_rider_off_control(m.r[7],rx_bf(m.arg(0)));return m.ret(4);}

RX_PORT(0x00493630,Atv_PinAndWater,"rx_AtvPhysics",0){rx_atv_pin_and_water(m.r[1],rx_bf(m.arg(0)));return m.ret(4);}
RX_PORT(0x0048e8a0,Atv_BeginRelative,"rx_AtvPhysics",0){rx_atv_begin_relative_pose(m.r[1]);return m.ret(0);}
RX_PORT(0x004a1250,Atv_ApplyRelative,"rx_AtvPhysics",0){rx_atv_apply_relative_pose(m.r[1]);return m.ret(0);}
