/* race_freestyle_driver.h -- FreestyleDriver (vtable 0x0093fafc), race_freestyle_driver.c. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
#define STR_GHOST_SESSION_COMPLETED 0x0093ee68u   /* "Ghost Session Completed" */
rx_addr  rx_freestyle_driver_construct(rx_addr d, rx_addr player);   /* 0050c900 (ECX = driver, EAX = player) -> driver */
rx_addr  rx_freestyle_driver_sdtor(rx_addr d, uint32_t flags);       /* 0050c990 vf0 */
void     rx_freestyle_driver_vf1(rx_addr d);                         /* 0050c9c0 vf1: the start */
void     rx_freestyle_driver_vf2(rx_addr d);                         /* 0050cae0 vf2: the end */
void     rx_freestyle_driver_vf3(rx_addr d);                         /* 0050cbd0 vf3: the reset */
void     rx_freestyle_driver_vf6(rx_addr d);                         /* 0050d1c0 vf6: the countdown */
void     rx_freestyle_driver_human(rx_addr d, float dt);             /* 0050d610 (EDI = driver; stdcall dt; RET 4) */
void     rx_freestyle_driver_vf7(rx_addr d, float dt);               /* 0050cf00 vf7: the frame */
void     rx_freestyle_driver_vf9(rx_addr d, rx_addr rule);           /* 0050db40 vf9: a rule's event */
uint32_t rx_freestyle_driver_vf12(rx_addr d);                        /* 0050dba0 vf12 */
float    rx_freestyle_driver_vf13(rx_addr d);                        /* 0050dbc0 vf13: st0 = the score shown */
void     rx_freestyle_driver_vf14(rx_addr d);                        /* 0050dc70 vf14 */
void     rx_freestyle_driver_vf16(rx_addr d, uint32_t kind, rx_addr object);   /* 0050dbf0 vf16 */
void     rx_freestyle_driver_vf17(rx_addr d);                        /* 0050dda0 vf17 */
void     rx_freestyle_driver_vf18(rx_addr d);                        /* 0050dd20 vf18 */
#ifdef __cplusplus
}
#endif
