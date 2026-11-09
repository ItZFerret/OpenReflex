#pragma once
#include <stdint.h>
/* Version1 little-endian dense schema-bound controller0 snapshot. No guest pointers.
 * Arrays follow header, tables, columns, cells, then text bytes. Offsets for text
 * are relative to text pool; names are NUL terminated, text is length-delimited. */
#define RX_PLAYER_SNAPSHOT_MAGIC 0x31535052u
#define RX_PLAYER_SNAPSHOT_VERSION 1u
#define RX_PLAYER_SNAPSHOT_MAX_BYTES (64u*1024u*1024u)
typedef struct rx_player_snapshot_header {
 uint32_t magic,version,total_bytes,tables,columns,cells,text_bytes,reserved;
} rx_player_snapshot_header;
typedef struct rx_player_snapshot_table {
 char name[48]; uint32_t rows,column_first,column_count,cell_first;
} rx_player_snapshot_table;
typedef struct rx_player_snapshot_column { char name[32];uint32_t type; } rx_player_snapshot_column;
typedef struct rx_player_snapshot_cell { uint32_t bits,text_offset,text_length; } rx_player_snapshot_cell;
