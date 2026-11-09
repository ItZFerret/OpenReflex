#include "oracle.h"
#include "../game/vehicle_atv_response.h"
using namespace reflex_port;
RX_PORT(0x0048d570,AtvContactResponse,"rx_AtvResponse",0){rx_atv_contact_response(m.r[1],rx_bf(m.arg(0)));return m.ret(4);}
RX_PORT(0x0048ef80,AtvWheelSlipLimiter,"rx_AtvResponse",0){rx_atv_limit_wheel_slip(m.r[7]);return m.ret(0);}
RX_PORT(0x0048ee30,AtvAllContacts,"rx_AtvResponse",OUT_EAX){return m.ret(0,rx_atv_all_contacts(m.r[1]));}
RX_PORT(0x0048ee60,AtvAllContactFrames,"rx_AtvResponse",OUT_EAX){return m.ret(0,rx_atv_all_contact_frames(m.r[1]));}
RX_PORT(0x0048eed0,AtvContactNormal,"rx_AtvResponse",OUT_EAX){return m.ret(4,rx_atv_contact_normal(m.r[1],m.arg(0)));}
