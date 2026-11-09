#include "oracle.h"
extern "C" {
#include "../game/vehicle_utv_runtime.h"
}
using namespace reflex_port;
namespace {void utv_push_f80(reflex_vm::Machine& m,rx_f80 r){reflex_f80::F80 v;v.kind=r.kind;v.sign=r.sign;v.mant=r.mant;v.exp=r.exp;m.fpu.push_back(v);}}
#define G "rx_UtvRuntime"
RX_PORT(0x004c6780,UtvPublishRider,G,0){rx_utv_publish_rider(m.r[1],rx_bf(m.arg(0)));return m.ret(4);}
RX_PORT(0x004c6ec0,UtvPosition,G,OUT_EAX|OUT_ECX|OUT_XMM0){uint32_t v=m.r[1],out=m.arg(0);rx_utv_position(v,out);m.r[0]=out;m.r[1]=rx_rd32(v+0x1540);m.xmm[0].lo=uint64_t(rx_rd32(out))|(uint64_t(rx_rd32(out+4))<<32);m.xmm[0].hi=uint64_t(rx_rd32(out+8))|(uint64_t(rx_rd32(out+12))<<32);return m.ret(4);}
RX_PORT(0x004c6ee0,UtvRotation,G,OUT_EAX|OUT_ECX|OUT_XMM0){uint32_t v=m.r[1],out=m.arg(0);rx_utv_rotation(v,out);m.r[0]=out;m.r[1]=rx_rd32(v+0x1540);m.xmm[0].lo=uint64_t(rx_rd32(out))|(uint64_t(rx_rd32(out+4))<<32);m.xmm[0].hi=uint64_t(rx_rd32(out+8))|(uint64_t(rx_rd32(out+12))<<32);return m.ret(4);}
RX_PORT(0x004c6f00,UtvVelocity,G,OUT_EAX|OUT_ECX|OUT_XMM0){uint32_t v=m.r[1],out=m.arg(0);rx_utv_velocity(v,out);m.r[0]=out;m.r[1]=rx_rd32(v+0x1540);m.xmm[0].lo=uint64_t(rx_rd32(out))|(uint64_t(rx_rd32(out+4))<<32);m.xmm[0].hi=uint64_t(rx_rd32(out+8))|(uint64_t(rx_rd32(out+12))<<32);return m.ret(4);}
RX_PORT(0x004c6f20,UtvAngularVelocity,G,OUT_EAX|OUT_ECX|OUT_XMM0){uint32_t v=m.r[1],out=m.arg(0);rx_utv_angular_velocity(v,out);m.r[0]=out;m.r[1]=rx_rd32(v+0x1540);m.xmm[0].lo=uint64_t(rx_rd32(out))|(uint64_t(rx_rd32(out+4))<<32);m.xmm[0].hi=uint64_t(rx_rd32(out+8))|(uint64_t(rx_rd32(out+12))<<32);return m.ret(4);}
RX_PORT(0x004c6f40,UtvBodyVelocity,G,OUT_EAX|OUT_ECX|OUT_XMM0){uint32_t v=m.r[1],out=m.arg(0);rx_utv_body_velocity(v,out);m.r[0]=out;m.r[1]=rx_rd32(v+0x1540);m.xmm[0].lo=uint64_t(rx_rd32(out))|(uint64_t(rx_rd32(out+4))<<32);m.xmm[0].hi=uint64_t(rx_rd32(out+8))|(uint64_t(rx_rd32(out+12))<<32);return m.ret(4);}
RX_PORT(0x004c6f60,UtvBodyAngularVelocity,G,OUT_EAX|OUT_ECX|OUT_XMM0){uint32_t v=m.r[1],out=m.arg(0);rx_utv_body_angular_velocity(v,out);m.r[0]=out;m.r[1]=rx_rd32(v+0x1540);m.xmm[0].lo=uint64_t(rx_rd32(out))|(uint64_t(rx_rd32(out+4))<<32);m.xmm[0].hi=uint64_t(rx_rd32(out+8))|(uint64_t(rx_rd32(out+12))<<32);return m.ret(4);}
RX_PORT(0x004c6f80,UtvEngineRpm,G,OUT_EAX|OUT_FPU){uint32_t v=m.r[1];utv_push_f80(m,rx_utv_engine_rpm(v));m.r[0]=rx_rd32(v+0x1540);return m.ret(0);}
RX_PORT(0x004c6fa0,UtvMaxRpm,G,OUT_EAX|OUT_FPU){uint32_t v=m.r[1];utv_push_f80(m,rx_utv_max_rpm(v));m.r[0]=rx_rd32(v+0x1540);return m.ret(0);}
RX_PORT(0x004c6fb0,UtvGear,G,OUT_EAX){return m.ret(0,rx_utv_gear(m.r[1]));}
RX_PORT(0x004c6fc0,UtvRiderTrigger,G,OUT_EAX){return m.ret(0,rx_utv_rider_trigger(m.r[1]));}
RX_PORT(0x004c6fd0,UtvAirborne,G,OUT_EAX){return m.ret(0,rx_utv_airborne(m.r[1]));}
RX_PORT(0x004c6fe0,UtvAllWheelsContact,G,OUT_EAX){return m.ret(0,rx_utv_all_wheels_contact(m.r[1]));}
RX_PORT(0x004c7040,UtvRiderFlag,G,OUT_EAX|OUT_ECX){m.r[0]=rx_utv_rider_flag(m.r[1]);m.r[1]=m.r[0];return m.ret(0);}
RX_PORT(0x004cdb70,UtvProjectBodyPositions,G,0){rx_utv_project_body_positions(m.r[1]);return m.ret(0);}
RX_PORT(0x004cdbe0,UtvClampAngularVelocity,G,0){rx_utv_clamp_angular_velocity(m.arg(0));return m.ret(4);}
