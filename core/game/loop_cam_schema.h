/* loop_cam_schema.h -- the camera tuning schema (00908500 / 006596f0 / 00659ef0) of loop_cam_schema.c.
 *
 * The nine 0x1da80-byte class records at 0x00af6d10 (MX, atv, PROTruck, PRO2Truck, Buggy, UTV and three unused)
 * are built by the CRT static initializer 00908500 before anything loads a tuning file: 006596f0 constructs the
 * records' tables (14 preset-type entries of 0x404 bytes at +0, then 94 blocks of 14 name records), 00659ef0 fills
 * the 14 entries' record lists with the camera setting names / kinds / defaults. 0065c200 (loop_cam_tuning.c)
 * then parses Cameras\<class>_Camera.bxml into those entries; the cameras' vf32 reads the settings by offset. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* 00908500 (_initterm slot 0x009166e0 + 0x5f0, the initializer walker runs it in place): 9 x 006596f0. */
void rx_camera_schema_init(void);
/* 006596f0 (ECX = one class record) -> EAX = the record: the constructors ('vector constructor iterator' 004022d0
 * on each block). */
rx_addr rx_camera_schema_construct(rx_addr rec);
/* 00659ef0 (stack = one class record; RET 4): the record names / kinds / defaults and the 14 entries' lists. */
void rx_camera_schema_fill(rx_addr rec);
#ifdef __cplusplus
}
#endif
