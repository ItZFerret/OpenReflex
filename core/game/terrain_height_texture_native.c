/* New finite height texture call sites; same unique registered deformation
 * owner, never rx_d3d_call's unregistered/original fallback. */
#include "terrain_height_exact.h"
#if !defined(RX_ORACLE) || defined(RX_ORACLE_FOURWHEEL)
#include "fx_util.h"
#include "rx_fn.h"
#include "../src/rx_d3d.h"
#include <stdlib.h>
void rx_terrain_height_texture(rx_addr tex,unsigned slot,rx_addr expected){
 rx_addr entry=rx_sp(),ra=U32(entry,0),vt=U32(tex,0),target=U32(vt,slot*4);unsigned n;
 if(slot==13){n=4;if(ra!=0x7c5f21||expected!=entry+20||U32(entry,4)!=tex||U32(entry,8)!=fx_reg(FX_EDX)||U32(entry,12)||U32(entry,16)!=0x810||fx_reg(FX_EAX)!=target||fx_reg(FX_ECX)!=vt)abort();}
 else if(slot==14){n=1;if((ra!=0x7c5fa0&&ra!=0x7c6143)||expected!=entry+8||U32(entry,4)!=tex||fx_reg(FX_EBX)!=tex||fx_reg(FX_ECX)!=vt||fx_reg(FX_EDX)!=target)abort();}
 else abort();
 if(!rx_fn(target))abort();uint32_t args[4];for(unsigned i=0;i<n;i++)args[i]=U32(entry,4+4*i);
 uint32_t hr=rx_d3d_call(tex,slot,n,args);if(rx_sp()!=entry)abort();fx_set_reg(FX_EAX,hr);rx_set_sp(expected);
}
#endif
