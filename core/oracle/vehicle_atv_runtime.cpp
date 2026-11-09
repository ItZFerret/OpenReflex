#include "oracle.h"
#include "../game/vehicle_atv_runtime.h"
using namespace reflex_port;
#define G "rx_AtvRuntime"
RX_PORT(0x00486560,AtvPosition,G,0){rx_atv_vehicle_position(m.r[1],m.arg(0));return m.ret(4);}
RX_PORT(0x00486580,AtvRotation,G,0){rx_atv_vehicle_rotation(m.r[1],m.arg(0));return m.ret(4);}
RX_PORT(0x004865a0,AtvVelocity,G,0){rx_atv_vehicle_velocity(m.r[1],m.arg(0));return m.ret(4);}
RX_PORT(0x004865c0,AtvAngularVelocity,G,0){rx_atv_vehicle_angular_velocity(m.r[1],m.arg(0));return m.ret(4);}
RX_PORT(0x004865e0,AtvBodyVelocity,G,0){rx_atv_vehicle_body_velocity(m.r[1],m.arg(0));return m.ret(4);}
RX_PORT(0x00486520,AtvPostStep,G,0){rx_atv_vehicle_post_step(m.r[1]);return m.ret(0);}
RX_PORT(0x00486100,AtvSafePose,G,OUT_EAX){return m.ret(16,rx_atv_vehicle_safe_pose(m.r[1],m.arg(0),m.arg(1),m.arg(2),m.arg(3)));}

RX_PORT(0x00484c50,AtvPublishRider,G,0){rx_atv_vehicle_publish_rider(m.r[1],rx_bf(m.arg(0)));return m.ret(4);}

RX_PORT(0x004847e0,AtvWorldMove,G,0){rx_atv_vehicle_world_move(m.r[1],m.arg(0));return m.ret(4);}

RX_PORT(0x0047d870,AtvUpdate,G,0){rx_atv_vehicle_update(m.r[1],rx_bf(m.arg(0)));return m.ret(4);}
RX_PORT(0x00489c80,AtvContactFilters,G,0){rx_atv_vehicle_contact_filters(m.r[0],rx_bf(m.arg(0)));return m.ret(4);}

RX_PORT(0x00486720,AtvRuntime_all_wheels_contact,G,OUT_EAX){return m.ret(0,rx_atv_vehicle_all_wheels_contact(m.r[1]));}

RX_PORT(0x0047c470,AtvRuntime_water_contact,G,OUT_EAX){return m.ret(0,rx_atv_vehicle_water_contact(m.r[1]));}

RX_PORT(0x00486650,AtvRuntime_rider_trigger,G,OUT_EAX){return m.ret(0,rx_atv_vehicle_rider_trigger(m.r[1]));}

RX_PORT(0x00486710,AtvRuntime_airborne,G,OUT_EAX){return m.ret(0,rx_atv_vehicle_airborne(m.r[1]));}

RX_PORT(0x00486780,AtvRuntime_rider_flag,G,OUT_EAX){return m.ret(0,rx_atv_vehicle_rider_flag(m.r[1]));}

RX_PORT(0x00486660,AtvRuntime_rider_action,G,OUT_EAX){return m.ret(0,rx_atv_vehicle_rider_action(m.r[1]));}
