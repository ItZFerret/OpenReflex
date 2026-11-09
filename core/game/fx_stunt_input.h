/* fx_stunt_input.h -- the stunt input mappings (the trick gestures) of the 0x6ca2f0..0x6ca5a0 range and the gesture matcher
 * 006c9aa0: fx_stunt_input.c. See that file's header for the record layout. */
#pragma once
#include "rx_game.h"
struct fmt_vfs;
#ifdef __cplusplus
extern "C" {
#endif
/* 006ca5a0: the init thread's mapping setup (0075ed60 at 0075f103): the aerial / ground "*.bxml" tables into
 * [0xa94310] / [0xa94314]; returns 1 when both loaded. The product file reads use `vfs` (the oracle ignores it: its reader
 * opens the install's files through the original CRT, build_xml.c). */
uint32_t rx_fx_stunt_mappings_init(const struct fmt_vfs* vfs);
/* 006ca3f0 (cdecl name; EAX = the table): one "StuntMapping" bxml into a fresh (count + 1) x 0x22 table (the last record is
 * the empty terminator), or 0 when the file / the allocation failed. */
rx_addr  rx_fx_stunt_mapping_load(rx_addr name);
/* 006ca2f0 (ECX = record, stack reader, node; caller-cleaned): one <StuntMapping> node: the "stunt" attribute into +0 (0x18
 * bytes) and the "input" gesture into +0x18 (10 bytes); returns 1 when both attributes exist. */
uint32_t rx_fx_stunt_mapping_record(rx_addr rec, rx_addr reader, rx_addr node);
/* 006c9aa0 (stdcall airborne; RET 4; EDI = the gesture string): the record of the aerial (airborne != 0, [0xa94310]) or the
 * ground ([0xa94314]) table whose +0x18 input string equals the gesture, or 0. Iteration stops at a record whose +0 name is
 * empty (the terminator). */
rx_addr  rx_fx_stunt_mapping_match(uint32_t airborne, rx_addr gesture);
/* 00548810 (EAX = pad; cdecl &dir): the gesture stick's two axes from the pad's steer / lean maps (00548fc0 / 00549050, the
 * "look" stick's KeyMaps, value getter 7). */
void     rx_fx_stunt_gesture_dir(uint32_t pad, rx_addr out);
/* 006cd2c0 (ESI = the input buffer K + 0x30; stack dt, dir.x, dir.y, airborne; RET 0x10): the direction classifier and the
 * gesture buffer / count / finished flag. */
void     rx_fx_stunt_gesture_classify(rx_addr buf, float dt, float dx, float dy, uint32_t airborne);
#ifdef __cplusplus
}
#endif
