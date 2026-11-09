#include "oracle.h"
extern "C" {
#include "../game/vehicle_fourwheel_leaves.h"
}
using namespace reflex_port;
namespace {void push_result(reflex_vm::Machine& m,rx_f80 r){reflex_f80::F80 v;v.kind=r.kind;v.sign=r.sign;v.mant=r.mant;v.exp=r.exp;m.fpu.push_back(v);}}
RX_PORT(0x004ac560,buggy_position,"rx_FourwheelLeaves",OUT_EAX|OUT_ECX|OUT_XMM0){uint32_t v=m.r[1],out=m.arg(0);m.r[0]=rx_buggy_position(v,out);m.r[1]=rx_rd32(v+0x15c0);m.xmm[0].lo=uint64_t(rx_rd32(out))|(uint64_t(rx_rd32(out+4))<<32);m.xmm[0].hi=uint64_t(rx_rd32(out+8))|(uint64_t(rx_rd32(out+12))<<32);return m.ret(4);}
RX_PORT(0x004ac580,buggy_rotation,"rx_FourwheelLeaves",OUT_EAX|OUT_ECX|OUT_XMM0){uint32_t v=m.r[1],out=m.arg(0);m.r[0]=rx_buggy_rotation(v,out);m.r[1]=rx_rd32(v+0x15c0);m.xmm[0].lo=uint64_t(rx_rd32(out))|(uint64_t(rx_rd32(out+4))<<32);m.xmm[0].hi=uint64_t(rx_rd32(out+8))|(uint64_t(rx_rd32(out+12))<<32);return m.ret(4);}
RX_PORT(0x004ac5a0,buggy_velocity,"rx_FourwheelLeaves",OUT_EAX|OUT_ECX|OUT_XMM0){uint32_t v=m.r[1],out=m.arg(0);m.r[0]=rx_buggy_velocity(v,out);m.r[1]=rx_rd32(v+0x15c0);m.xmm[0].lo=uint64_t(rx_rd32(out))|(uint64_t(rx_rd32(out+4))<<32);m.xmm[0].hi=uint64_t(rx_rd32(out+8))|(uint64_t(rx_rd32(out+12))<<32);return m.ret(4);}
RX_PORT(0x004ac5c0,buggy_angular_velocity,"rx_FourwheelLeaves",OUT_EAX|OUT_ECX|OUT_XMM0){uint32_t v=m.r[1],out=m.arg(0);m.r[0]=rx_buggy_angular_velocity(v,out);m.r[1]=rx_rd32(v+0x15c0);m.xmm[0].lo=uint64_t(rx_rd32(out))|(uint64_t(rx_rd32(out+4))<<32);m.xmm[0].hi=uint64_t(rx_rd32(out+8))|(uint64_t(rx_rd32(out+12))<<32);return m.ret(4);}
RX_PORT(0x004ac5e0,buggy_body_velocity,"rx_FourwheelLeaves",OUT_EAX|OUT_ECX|OUT_XMM0){uint32_t v=m.r[1],out=m.arg(0);m.r[0]=rx_buggy_body_velocity(v,out);m.r[1]=rx_rd32(v+0x15c0);m.xmm[0].lo=uint64_t(rx_rd32(out))|(uint64_t(rx_rd32(out+4))<<32);m.xmm[0].hi=uint64_t(rx_rd32(out+8))|(uint64_t(rx_rd32(out+12))<<32);return m.ret(4);}
RX_PORT(0x004ac600,buggy_body_angular_velocity,"rx_FourwheelLeaves",OUT_EAX|OUT_ECX|OUT_XMM0){uint32_t v=m.r[1],out=m.arg(0);m.r[0]=rx_buggy_body_angular_velocity(v,out);m.r[1]=rx_rd32(v+0x15c0);m.xmm[0].lo=uint64_t(rx_rd32(out))|(uint64_t(rx_rd32(out+4))<<32);m.xmm[0].hi=uint64_t(rx_rd32(out+8))|(uint64_t(rx_rd32(out+12))<<32);return m.ret(4);}
RX_PORT(0x004ac620,buggy_engine_rpm,"rx_FourwheelLeaves",OUT_EAX|OUT_FPU){uint32_t v=m.r[1];push_result(m,rx_buggy_engine_rpm(v));m.r[0]=rx_rd32(v+0x15c0);return m.ret(0);}
RX_PORT(0x004ac640,buggy_engine_limit,"rx_FourwheelLeaves",OUT_EAX|OUT_FPU){uint32_t v=m.r[1];push_result(m,rx_buggy_engine_limit(v));m.r[0]=rx_rd32(v+0x15c0);return m.ret(0);}
RX_PORT(0x004ac650,buggy_gear,"rx_FourwheelLeaves",OUT_EAX){m.r[0]=rx_buggy_gear(m.r[1]);return m.ret(0);}
RX_PORT(0x004ac660,buggy_contact_flag,"rx_FourwheelLeaves",OUT_EAX){m.r[0]=rx_buggy_contact_flag(m.r[1]);return m.ret(0);}
RX_PORT(0x004ac670,buggy_contact_state,"rx_FourwheelLeaves",OUT_EAX){m.r[0]=rx_buggy_contact_state(m.r[1]);return m.ret(0);}
RX_PORT(0x004ac680,buggy_all_wheels_contact,"rx_FourwheelLeaves",OUT_EAX){m.r[0]=rx_buggy_all_wheels_contact(m.r[1]);return m.ret(0);}
RX_PORT(0x004ac6e0,buggy_mode_two,"rx_FourwheelLeaves",OUT_EAX|OUT_ECX){m.r[0]=rx_buggy_mode_two(m.r[1]);m.r[1]=m.r[0];return m.ret(0);}
RX_PORT(0x00473490,truck_position,"rx_FourwheelLeaves",OUT_EAX|OUT_ECX|OUT_XMM0){uint32_t v=m.r[1],out=m.arg(0);m.r[0]=rx_truck_position(v,out);m.r[1]=rx_rd32(v+0x15d0);m.xmm[0].lo=uint64_t(rx_rd32(out))|(uint64_t(rx_rd32(out+4))<<32);m.xmm[0].hi=uint64_t(rx_rd32(out+8))|(uint64_t(rx_rd32(out+12))<<32);return m.ret(4);}
RX_PORT(0x004734b0,truck_rotation,"rx_FourwheelLeaves",OUT_EAX|OUT_ECX|OUT_XMM0){uint32_t v=m.r[1],out=m.arg(0);m.r[0]=rx_truck_rotation(v,out);m.r[1]=rx_rd32(v+0x15d0);m.xmm[0].lo=uint64_t(rx_rd32(out))|(uint64_t(rx_rd32(out+4))<<32);m.xmm[0].hi=uint64_t(rx_rd32(out+8))|(uint64_t(rx_rd32(out+12))<<32);return m.ret(4);}
RX_PORT(0x004734d0,truck_velocity,"rx_FourwheelLeaves",OUT_EAX|OUT_ECX|OUT_XMM0){uint32_t v=m.r[1],out=m.arg(0);m.r[0]=rx_truck_velocity(v,out);m.r[1]=rx_rd32(v+0x15d0);m.xmm[0].lo=uint64_t(rx_rd32(out))|(uint64_t(rx_rd32(out+4))<<32);m.xmm[0].hi=uint64_t(rx_rd32(out+8))|(uint64_t(rx_rd32(out+12))<<32);return m.ret(4);}
RX_PORT(0x004734f0,truck_angular_velocity,"rx_FourwheelLeaves",OUT_EAX|OUT_ECX|OUT_XMM0){uint32_t v=m.r[1],out=m.arg(0);m.r[0]=rx_truck_angular_velocity(v,out);m.r[1]=rx_rd32(v+0x15d0);m.xmm[0].lo=uint64_t(rx_rd32(out))|(uint64_t(rx_rd32(out+4))<<32);m.xmm[0].hi=uint64_t(rx_rd32(out+8))|(uint64_t(rx_rd32(out+12))<<32);return m.ret(4);}
RX_PORT(0x00473510,truck_body_velocity,"rx_FourwheelLeaves",OUT_EAX|OUT_ECX|OUT_XMM0){uint32_t v=m.r[1],out=m.arg(0);m.r[0]=rx_truck_body_velocity(v,out);m.r[1]=rx_rd32(v+0x15d0);m.xmm[0].lo=uint64_t(rx_rd32(out))|(uint64_t(rx_rd32(out+4))<<32);m.xmm[0].hi=uint64_t(rx_rd32(out+8))|(uint64_t(rx_rd32(out+12))<<32);return m.ret(4);}
RX_PORT(0x00473530,truck_body_angular_velocity,"rx_FourwheelLeaves",OUT_EAX|OUT_ECX|OUT_XMM0){uint32_t v=m.r[1],out=m.arg(0);m.r[0]=rx_truck_body_angular_velocity(v,out);m.r[1]=rx_rd32(v+0x15d0);m.xmm[0].lo=uint64_t(rx_rd32(out))|(uint64_t(rx_rd32(out+4))<<32);m.xmm[0].hi=uint64_t(rx_rd32(out+8))|(uint64_t(rx_rd32(out+12))<<32);return m.ret(4);}
RX_PORT(0x00473550,truck_engine_rpm,"rx_FourwheelLeaves",OUT_EAX|OUT_FPU){uint32_t v=m.r[1];push_result(m,rx_truck_engine_rpm(v));m.r[0]=rx_rd32(v+0x15d0);return m.ret(0);}
RX_PORT(0x00473570,truck_engine_limit,"rx_FourwheelLeaves",OUT_EAX|OUT_FPU){uint32_t v=m.r[1];push_result(m,rx_truck_engine_limit(v));m.r[0]=rx_rd32(v+0x15d0);return m.ret(0);}
RX_PORT(0x00473580,truck_gear,"rx_FourwheelLeaves",OUT_EAX){m.r[0]=rx_truck_gear(m.r[1]);return m.ret(0);}
RX_PORT(0x00473590,truck_contact_flag,"rx_FourwheelLeaves",OUT_EAX){m.r[0]=rx_truck_contact_flag(m.r[1]);return m.ret(0);}
RX_PORT(0x004735a0,truck_contact_state,"rx_FourwheelLeaves",OUT_EAX){m.r[0]=rx_truck_contact_state(m.r[1]);return m.ret(0);}
RX_PORT(0x004735b0,truck_all_wheels_contact,"rx_FourwheelLeaves",OUT_EAX){m.r[0]=rx_truck_all_wheels_contact(m.r[1]);return m.ret(0);}
RX_PORT(0x00473610,truck_mode_two,"rx_FourwheelLeaves",OUT_EAX|OUT_ECX){m.r[0]=rx_truck_mode_two(m.r[1]);m.r[1]=m.r[0];return m.ret(0);}
