// build_event_proxies.cpp -- oracle adapter for core/game/build_event_proxies.c (group rx_build11).
#include "oracle.h"
extern "C" { void rx_event_proxy_call_fn_activity(uint32_t proxy, uint32_t payload); void rx_event_proxy_call_member_int_float(uint32_t proxy, uint32_t payload); }
using namespace reflex_port;

// 0041a3b0 EventHandlerProxy<void(*)(Activity*)>::vf2 (thiscall proxy, payload pointer; RET 4)
RX_PORT(0x0041a3b0, EventProxyCallFnActivity, "rx_build11", 0) { rx_event_proxy_call_fn_activity(m.r[1], m.arg(0)); return m.ret(4); }

// 007606d0 (thiscall proxy, stack payload pointer; RET 4)
RX_PORT(0x007606d0, EventProxy_CallMemberIntFloat, "rx_build11", 0) { rx_event_proxy_call_member_int_float(m.r[1], m.arg(0)); return m.ret(4); }
