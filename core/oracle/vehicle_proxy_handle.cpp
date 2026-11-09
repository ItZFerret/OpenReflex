#include "oracle.h"
#include "../game/vehicle_proxy_handle.h"
using namespace reflex_port;
RX_PORT(0x0042af20,VehicleProxyHandleRelease,"rx_AtvLifecycle",0){rx_vehicle_proxy_handle_release(m.r[1]);return m.ret(0);}
RX_PORT(0x0042b090,VehicleProxyHandleDelete,"rx_AtvLifecycle",OUT_EAX){return m.ret(4,rx_vehicle_proxy_handle_delete(m.r[1],m.arg(0)));}
