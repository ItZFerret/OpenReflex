/* Four-wheel V vf33. Original 004c6c40/004ac2e0/00473210: four
 * TerrainTracks_CutStrip calls followed by contact/control reset. All three
 * disassemblies have the same store order with class-specific offsets. */
#include "vehicle_fourwheel_place.h"
#include "race_extern.h"
#include "phys_terrain.h"
static void reset_ground(rx_addr v,unsigned handles,unsigned contact,unsigned extra,unsigned tail,unsigned simptr,unsigned simbase){
 RACE_FRAME_EBP(F,16,4,3);
 if(!(U8(0,0x009e30e0)&1)){SU32(0,0x009e30e0,U32(0,0x009e30e0)|1);SU32(0,0x009e30dc,0);}
 for(unsigned i=0;i<4;i++){
  uint32_t h=U32(v,handles+4*i),index;rx_addr pool;
  if(h&0x1000){pool=U32(0,0x00a95774);index=h^0x1000;}
  else if(h&0x2000){pool=U32(0,0x00a95778);index=h^0x2000;}
  else{pool=U32(0,0x00a9577c);index=h^0x4000;}
  RACE_CALL_SP(0,phys_tracks_cut_strip(pool,(int32_t)index));
 }
 rx_addr s=U32(v,simptr);
 for(unsigned o=0;o<40;o+=4)SU32(v,contact+o,0);
 for(unsigned o=0;o<32;o+=4)SU32(v,extra+o,0);
 SU32(v,0x1310,0);SU32(v,0x1314,0);
 for(unsigned o=32;o<48;o+=4)SU32(v,extra+o,0);
 SU32(v,0x1318,0);SU32(v,0x131c,0);
 rx_st4(s+simbase+0x20,rx_ld4(0x00d61670));
 rx_v4 q=rx_ld4(0x00d61670);s=U32(v,simptr);rx_st4(s+simbase+0x30,q);
 q=rx_ld4(0x00d61670);s=U32(v,simptr);rx_st4(s+simbase,q);
 q=rx_ld4(0x00d61670);s=U32(v,simptr);rx_st4(s+simbase+0x10,q);
 for(unsigned o=0;o<16;o+=4)SU32(v,tail+o,0);
 SU32(v,0x1320,1);RACE_FRAME_END(F);
}
void rx_utv_reset_ground(rx_addr v){reset_ground(v,0x13c4,0x13d4,0x1468,0x14a0,0x1540,0x8600);}
RX_FUNCTION(0x004c6c40,rx_utv_reset_ground)
void rx_buggy_reset_ground(rx_addr v){reset_ground(v,0x1410,0x1420,0x14b8,0x14f0,0x15c0,0x85f0);}
RX_FUNCTION(0x004ac2e0,rx_buggy_reset_ground)
void rx_truck_reset_ground(rx_addr v){reset_ground(v,0x144c,0x145c,0x14f8,0x1530,0x15d0,0x8600);}
RX_FUNCTION(0x00473210,rx_truck_reset_ground)
