/* build_activity_freestyle.h -- construction of the Freestyle activities (build_activity_freestyle.c). */
#pragma once
#include "build_activity.h"
rx_addr rx_freestyle_audio_construct(rx_addr logic);                          /* 005623b0 (ESI = logic) -> EAX */
rx_addr rx_freestyle_stub_construct(rx_addr activity, uint32_t type, rx_addr world);   /* 00562280 (ECX = type, EAX = world; stdcall activity; RET 4) */
rx_addr rx_freestyle_construct(rx_addr activity, uint32_t type, rx_addr world);        /* 00561910 (ESI = activity, ECX = type, EAX = world) -> EAX */
