/* Existing typed D3D semantic owner only. No original fallback is admissible
 * on the product exact path; actual deformation resource vtable must register. */
#include "terrain_triangles_exact.h"
#if !defined(RX_ORACLE) || defined(RX_ORACLE_FOURWHEEL)
#include "fx_util.h"
#include "rx_fn.h"
#include "../src/rx_d3d.h"
#include <stdlib.h>
void rx_terrain_texture_exact(rx_addr texture,unsigned slot,rx_addr expected){
 rx_addr entry=rx_sp(),ra=U32(entry,0),vt=U32(texture,0),target=U32(vt,slot*4);unsigned n;
 if(slot==13){n=4;if(ra!=0x7c6208||expected!=entry+20||U32(entry,4)!=texture||U32(entry,12)||U32(entry,16)!=0x810||fx_reg(FX_EAX)!=target||fx_reg(FX_ECX)!=vt||fx_reg(FX_EDX)!=U32(entry,8))abort();}
 else if(slot==14){n=1;if(expected!=entry+8||U32(entry,4)!=texture)abort();
  if(ra==0x7c66c2){if(fx_reg(FX_EAX)!=vt||fx_reg(FX_ECX)!=target)abort();}
  else if(ra==0x7c63e7){if(fx_reg(FX_ECX)!=vt||fx_reg(FX_EDX)!=target)abort();}
  else if(ra==0x7c66d9){if(fx_reg(FX_EDX)!=vt||fx_reg(FX_EAX)!=target)abort();}
  else abort();
 }else abort();
 if(!rx_fn(target))abort(); /* Never enter rx_d3d_call's original-only fallback. */
 uint32_t args[4];for(unsigned i=0;i<n;i++)args[i]=U32(entry,4+4*i);
 uint32_t hr=rx_d3d_call(texture,slot,(int)n,args);if(rx_sp()!=entry)abort();
 fx_set_reg(FX_EAX,hr);rx_set_sp(expected);
}
#endif
