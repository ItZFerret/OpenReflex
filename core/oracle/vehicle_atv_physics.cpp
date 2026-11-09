#include "oracle.h"
extern "C" {
#include "../game/vehicle_atv_physics.h"
}
using namespace reflex_port;
RX_PORT(0x0047c460,Atv_SteeringAngle,"rx_AtvPhysics",OUT_FPU){
 const rx_f80 r=rx_atv_steering_angle(m.r[1]);
 reflex_f80::F80 v;v.kind=r.kind;v.sign=r.sign;v.mant=r.mant;v.exp=r.exp;m.fpu.push_back(v);return m.ret(0);
}
RX_PORT(0x0049ee00,Atv_ReadController,"rx_AtvPhysics",0){rx_atv_read_controller(m.r[1],m.arg(0));return m.ret(4);}
RX_PORT(0x004a0e60,Atv_SetRelativePose,"rx_AtvPhysics",0){rx_atv_set_relative_pose(m.r[1],m.arg(0));return m.ret(4);}

RX_PORT(0x00496310,Atv_ContactFilters,"rx_AtvPhysics",0){rx_atv_update_contact_filters(m.r[7],rx_bf(m.arg(0)));return m.ret(4);}

RX_PORT(0x0048f9e0,Atv_GroundPlace,"rx_AtvPhysics",0){rx_atv_ground_place(m.r[1],m.arg(0),m.arg(1),m.arg(2),m.arg(3),m.arg(4));return m.ret(20);}

RX_PORT(0x004a17b0,Atv_RollSpring,"rx_AtvPhysics",0){rx_atv_roll_spring(m.arg(0),rx_bf(m.arg(1)));return m.ret(8);}
RX_PORT(0x004931a0,Atv_WheelSpin,"rx_AtvPhysics",0){rx_atv_publish_wheel_spin(m.r[1],rx_bf(m.arg(0)));return m.ret(4);}
RX_PORT(0x00498db0,Atv_AxleTorques,"rx_AtvPhysics",0){rx_atv_apply_axle_torques(m.arg(0));return m.ret(4);}
RX_PORT(0x0048d430,Atv_ClampSpin,"rx_AtvPhysics",0){rx_atv_clamp_body_spin(m.arg(0));return m.ret(4);}
RX_PORT(0x00493770,Atv_Resistance,"rx_AtvPhysics",0){rx_atv_apply_resistance(m.r[7]);return m.ret(0);}
