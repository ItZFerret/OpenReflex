/* fx_stunt_table.h -- the stunt descriptor table and its loaders (fx_stunt_table.c): the init thread's 006c8480 / 006c8b70
 * and the StuntTable row parser 006c8670. See the file header for the layout and the dead residue fields. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void     rx_stunt_table_alloc(void);                       /* 006cd6e0: the 100-record table allocation */
void     rx_stunt_descriptor_grow(uint32_t newcap);        /* 006ce1a0 (EDI = the new capacity) */
void     rx_stunt_record_copy(rx_addr dst, rx_addr src);   /* 00622050 (ECX = src, EAX = dst) */
uint32_t rx_stunt_row_read(rx_addr node, rx_addr reader, rx_addr rec);   /* 006c8670 (ECX = node): 1 = valid */
uint32_t rx_stunt_table_load(void);                        /* 006c8b70: returns the rows added */
void     rx_stunt_tables_init(void);                       /* 006c8480 */
#ifdef __cplusplus
}
#endif
