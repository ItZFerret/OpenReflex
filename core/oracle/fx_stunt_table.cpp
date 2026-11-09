// fx_stunt_table.cpp -- oracle adapters for the stunt descriptor table loaders (fx_stunt_table.c; agent "profilestats").
// The reference race worlds never call them (the game init thread loads the table; the product runs the init stand-in), so these are
// for the group's completeness and for a baton-boot call check.
#include "oracle.h"
extern "C" {
#include "../game/fx_stunt_table.h"
}
RX_PORT(0x006cd6e0, StuntTableAlloc, "rx_fx", 0) { rx_stunt_table_alloc(); return m.ret(0); }
RX_PORT(0x006ce1a0, StuntDescriptorGrow, "rx_fx", 0) { rx_stunt_descriptor_grow(m.r[7]); return m.ret(0); }
RX_PORT(0x00622050, StuntRecordCopy, "rx_fx", 0) { rx_stunt_record_copy(m.r[0], m.r[1]); return m.ret(0); }
RX_PORT(0x006c8670, StuntRowRead, "rx_fx", 0) { return m.ret(0, rx_stunt_row_read(m.r[1], m.arg(0), m.arg(1))); }
RX_PORT(0x006c8b70, StuntTableLoad, "rx_fx", 0) { return m.ret(0, rx_stunt_table_load()); }
RX_PORT(0x006c8480, StuntTablesInit, "rx_fx", 0) { rx_stunt_tables_init(); return m.ret(0); }
