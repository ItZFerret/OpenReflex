#include "oracle.h"
#include "../game/vehicle_atv_leaves.h"
using namespace reflex_port;
static void atv_leaf_fpu(reflex_vm::Machine& m,rx_f80 r){reflex_f80::F80 v;v.kind=r.kind;v.sign=r.sign;v.mant=r.mant;v.exp=r.exp;m.fpu.push_back(v);}
RX_PORT(0x0047cbb0,AtvControlMode,"rx_AtvLeaves",0){rx_atv_vehicle_set_control_mode(m.r[1],m.arg(0));return m.ret(4);}
RX_PORT(0x0047c440,AtvClearContacts,"rx_AtvLeaves",0){rx_atv_clear_contact_history(m.r[1]);return m.ret(0);}
RX_PORT(0x00486600,AtvBodyAngularVelocity,"rx_AtvLeaves",OUT_EAX){return m.ret(4,rx_atv_vehicle_body_angular_velocity(m.r[1],m.arg(0)));}
RX_PORT(0x00486640,AtvMaxRPM,"rx_AtvLeaves",OUT_FPU){atv_leaf_fpu(m,rx_atv_vehicle_max_rpm(m.r[1]));return m.ret(0);}
RX_PORT(0x00486790,AtvClutchOverride,"rx_AtvLeaves",OUT_EAX){return m.ret(0,rx_atv_vehicle_clutch_override(m.r[1]));}
RX_PORT(0x004867b0,AtvWheelLoad,"rx_AtvLeaves",OUT_FPU){atv_leaf_fpu(m,rx_atv_vehicle_wheel_load(m.r[1],m.arg(0)));return m.ret(4);}
RX_PORT(0x00487af0,AtvWheelTag,"rx_AtvLeaves",OUT_EAX){return m.ret(4,rx_atv_vehicle_has_wheel_tag(m.r[1],m.arg(0)));}
RX_PORT(0x004744c0,AtvCollisionType,"rx_AtvLeaves",OUT_EAX){return m.ret(4,rx_atv_vehicle_collision_type(m.r[1],m.arg(0)));}
RX_PORT(0x0048d2a0,AtvBodyContactBits,"rx_AtvLeaves",OUT_EAX){return m.ret(0,rx_atv_body_contact_bits(m.r[1]));}
RX_PORT(0x0048d160,AtvNeedsContacts,"rx_AtvLeaves",OUT_EAX){return m.ret(0,rx_atv_needs_contact_resolution(m.r[1]));}
RX_PORT(0x0046a2c0,AtvOutsideTerrain,"rx_AtvLeaves",OUT_EAX){return m.ret(0,rx_atv_vehicle_outside_terrain(m.r[1]));}

RX_PORT(0x0048d320,AtvPendingTuning,"rx_AtvLeaves",0){rx_atv_apply_pending_tuning(m.r[1]);return m.ret(0);}
