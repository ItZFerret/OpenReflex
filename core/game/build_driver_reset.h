/* build_driver_reset.h -- the drivers' reset (vf3): BaseDriver, NetworkDriver, FreeRideDriver (build_driver_reset.c). */
#pragma once
#include "rx_game.h"

uint32_t rx_activity_kind_flag(uint32_t kind);                 /* 0051f150 (EAX = the activity kind) -> EAX: 0 for the kinds 0, 2, 6, 0x11, 0x13, else 1 */
void     rx_driver_reset_timers(rx_addr player);               /* 00508ad0 (EAX = the player) */
void     rx_mud_record_clear(rx_addr vehicle);                 /* MudRecord_Clear 00409320 (EDX = the vehicle) */
void     rx_base_driver_vf3(rx_addr d);                        /* BaseDriver::vf3 00508f90 (ECX) */
void     rx_network_driver_vf3(rx_addr d);                     /* NetworkDriver::vf3 0050e400 (ECX) */
void     rx_free_ride_driver_vf3(rx_addr d);                   /* FreeRideDriver::vf3 0050bd80 (ECX) */
void     rx_driver_behaviors_vf6(rx_addr d);                   /* 005092b0 (stdcall driver; RET 4) */
void     rx_driver_rules_vf6(rx_addr d);                       /* 005092f0 (stdcall driver; RET 4) */
rx_addr  rx_driver_behavior_f(rx_addr d);                      /* 0053a4c0 (stdcall driver; RET 4) -> EAX */
