#pragma once
#include "rx_game.h"
void rx_base_driver_vf2(rx_addr driver);                                                    /* BaseDriver::vf2 00508ee0 (ECX) */
void rx_free_ride_unsubscribe(rx_addr registry, rx_addr name, rx_addr obj, rx_addr fn);     /* 0053a410 (ECX = registry, EAX = name; stdcall obj, fn; RET 8) */
void rx_free_ride_driver_vf2(rx_addr driver);                                               /* FreeRideDriver::vf2 0050bd30 (ECX) */
rx_addr rx_rule_vf0(rx_addr self, uint32_t flags);                                      /* Rule::vf0 0051bd10 (thiscall; RET 4) -> this */
void rx_activity_entities_reset_106b(rx_addr activity);                                 /* 00558470 (stdcall; RET 4) */
void rx_activity_vf51(rx_addr activity);                                                    /* Activity::vf51 0055a510 (ECX) */
void rx_network_unsubscribe(rx_addr registry, rx_addr name, rx_addr obj, rx_addr fn);       /* 0053a7a0 (ECX = registry, EAX = name; stdcall obj, fn; RET 8) */
void rx_network_driver_dtor(rx_addr driver);                                                /* FUN_0050e2f0 (ECX) */
rx_addr rx_free_ride_driver_sdtor(rx_addr driver, uint32_t flags);                          /* FreeRideDriver::sdtor 0050bc90 (thiscall; RET 4) */
