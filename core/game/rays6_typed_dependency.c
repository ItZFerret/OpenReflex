/* Typed native/exact composition of the four real outer batch sites.
 * Separate source proposal; never link alongside original or unclosed provider. */
#include "rays6_observed.h"
#include "triangles_segments_dependencies.h"
#include "fx_util.h"
#include <stdlib.h>
uint32_t rx_tri_segment_rays_dependency(rx_addr expected){
 rx_addr entry=rx_sp(),ra=U32(entry,0),B=entry+28;
 int triangles=ra==0x87039f||ra==0x870571,segments=ra==0x87115f||ra==0x87132e;
 if((!triangles&&!segments)||expected!=entry+4||fx_reg(FX_ECX)!=B+0x200||fx_reg(FX_EDX)!=(triangles?B+0x360:B+0x2a0))abort();
 rx_addr args[]={triangles?B+0x300:B+0x3c0,triangles?B+0x3c0:B+0x300,triangles?B+0x2a0:B+0x360,triangles?B+0x180:B+0xa8,B+0x420,B+0x1a0};
 for(unsigned i=0;i<6;i++)if(U32(entry,4+4*i)!=args[i])abort();
 uint32_t result=phys_rays6_triangles_observed(fx_reg(FX_ECX),fx_reg(FX_EDX),args[0],args[1],args[2],args[3],args[4],args[5]);
 /* Typed C preserves enteredESP, unlike original RET0. Model only that RET,
  * leave consumed arguments and all original private writes untouched. */
 if(rx_sp()!=entry||U32(entry,0)!=ra||fx_reg(FX_EAX)!=result)abort();rx_set_sp(expected);return result;
}
