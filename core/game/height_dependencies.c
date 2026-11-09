/* Complete finite typed cell/window composition. Replace parent unclosed/cell
 * dependency TUs with this one; never duplicate the canonical terrain owner. */
#include "terrain_height_exact.h"
#include "terrain_window_frames.h"
#if !defined(RX_ORACLE) || defined(RX_ORACLE_FOURWHEEL) || defined(RX_HEIGHT_WINDOW_TYPED_TEST)
#include <stdlib.h>
rx_v4 rx_terrain_height_dependency(enum rx_height_role role,rx_addr expected){
 rx_addr entry=rx_sp();uint32_t ra=U32(entry,0);rx_v4 live;if(expected!=entry+4)abort();
 if(role==RX_HEIGHT_CELL_SETUP){if(ra!=0x7c5f8b)abort();(void)phys_terrain_cell_setup_exact(fx_reg(FX_ESI),fx_reg(FX_ECX),fx_reg(FX_EDX),U32(entry,4),&live);}
 else if(role==RX_HEIGHT_CELL_OFFSETS){if(ra!=0x7c5fc2)abort();phys_terrain_cell_offsets_exact(fx_reg(FX_ESI),&live);}
 else if(role==RX_HEIGHT_SAMPLE_BASE||role==RX_HEIGHT_SAMPLE_DEFORM){
  if(ra!=(role==RX_HEIGHT_SAMPLE_BASE?0x7c5fd6u:0x7c601au))abort();
  phys_terrain_sample_window_exact(fx_reg(FX_ESI),fx_reg(FX_EDI),U32(entry,4),U32(entry,8),U32(entry,12),&live);
 }else abort();
 if(rx_sp()!=entry)abort();rx_set_sp(expected);return live;
}
#endif
