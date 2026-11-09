#pragma once
#include "rx_game.h"
#include "fx_util.h"
#include "phys_math.h"
#include "world_rider.h"
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
/* These APIs retain original entered ESP; cdecl RET0 belongs to caller. */
uint32_t phys_terrain_cell_setup_exact(rx_addr work,rx_addr dims,rx_addr pos,rx_addr cells,rx_v4* live_xmm0);
void phys_terrain_cell_offsets_exact(rx_addr work,rx_v4* live_xmm0);
#ifdef __cplusplus
}
#endif
static inline void cell_disjoint(rx_addr p,uint64_t bytes,rx_addr bottom,rx_addr entry){if((uint64_t)p<entry&&(uint64_t)p+bytes>(uint64_t)bottom-0x60u)abort();}
static inline void cell_constants(int floor){
 const uint32_t c[][2]={{0x931f24,0x3f800000},{0x9322cc,0x3f000000},{0x9774a8,0x80000000},{0x9774f0,0x4b000000}};
 for(unsigned i=0;i<(floor?4u:1u);i++)if(U32(c[i][0],0)!=c[i][1])abort();
}
static inline void cell_vec_ctor(rx_addr out,rx_v4 value,rx_addr ra){
 if(ra!=0x7c5248&&ra!=0x7c5323)abort();rx_addr caller=rx_sp();uint32_t arg=out;
 (void)fx_push(1,&arg,ra);rx_addr entry=rx_sp();fx_frame f;fx_frame_enter0(&f,0,16);
 if(out&15)abort();rx_world_vec4_from_scalars(out,value.v[0],value.v[1],value.v[2],value.v[3]);
 fx_set_reg(FX_EAX,out);fx_frame_leave(&f);if(rx_sp()!=entry)abort();rx_set_sp(caller);
}
static inline rx_v4 cell_floor_frame(rx_v4 value){
 rx_addr caller=rx_sp();(void)fx_push(0,0,0x7c5257);rx_addr entry=rx_sp();
 fx_frame f;fx_frame_enter0(&f,0x20,16);rx_st4(f.E,value);
 rx_v4 result=phys_vec4_floor(value);rx_st4(f.E+0x10,result);
 fx_frame_leave(&f);if(rx_sp()!=entry)abort();rx_set_sp(caller);return result;
}
static inline void cell_cookie_leave(fx_frame*f,rx_addr ra){
 if(ra!=0x7c52c2&&ra!=0x7c52e0&&ra!=0x7c5373)abort();
 fx_set_reg(FX_ECX,U32(f->E,0x1c)^f->E);if(fx_reg(FX_ECX)!=U32(0,0x9b45e4))abort();
 SU32(f->E-4,0,ra);rx_set_sp(f->E-4);phys_security_check_cookie(fx_reg(FX_ECX));rx_set_sp(f->E);fx_frame_leave(f);
}
