/* Native semantic child composition. No original execution/provider. */
#include "fourwheel_snapshot.h"
#include "fx_util.h"
#include <stdlib.h>
#include <stdio.h>
static const rx_addr return_sites[2][3][9]={{{5009549, 5009614, 5010129, 5010608, 5011128, 5011598}, {4900653, 4900718, 4901233, 4901712, 4902232, 4902702}, {4666973, 4667038, 4667553, 4668032, 4668552, 4669022}}, {{5011879, 5012165, 5012466, 5012767, 5012837, 5012894, 5012912, 5012966, 5012984}, {4902983, 4903269, 4903570, 4903871, 4903941, 4903998, 4904016, 4904070, 4904088}, {4669303, 4669589, 4669890, 4670191, 4670261, 4670318, 4670336, 4670390, 4670408}}};
rx_f80 rx_fourwheel_snapshot_dependency(unsigned family,unsigned method,unsigned site,rx_addr target,unsigned kind){
 if(family>2||method>1||site>=(method?9:6)||kind>2)abort();
 rx_addr self=fx_reg(FX_ECX),entry=rx_sp();uint32_t ra=U32(entry,0);fx_saved old=fx_save();
 if(ra!=return_sites[method][family][site])abort();
 unsigned slot=method?(site<4?0x58:site==4?0x94:site==5?0xbc:site==6?0xc0:site==7?0xa8:0xc8):(site==0?0x94:site==1?0x90:0x58);
 if(target!=U32(U32(self,0),slot)||(kind==1)!=(slot==0x94||slot==0x90)||(kind==2)!=(slot==0xbc||slot==0xc0))abort();
 void* fn=rx_fn(target);if(!fn){fprintf(stderr,"snapshot: missing native child %08x at %08x\n",target,ra);abort();}
 rx_f80 result=rx_f80_from_f32(0);
 if(kind==1){rx_addr out=U32(entry,4);((void(*)(rx_addr,rx_addr))fn)(self,out);fx_set_reg(FX_EAX,out);}
 else if(kind==2)result=((rx_f80(*)(rx_addr))fn)(self);
 else fx_set_reg(FX_EAX,((uint32_t(*)(rx_addr))fn)(self));
 if(rx_sp()!=entry||U32(entry,0)!=ra||fx_reg(FX_EBX)!=old.ebx||fx_reg(FX_EBP)!=old.ebp||fx_reg(FX_ESI)!=old.esi||fx_reg(FX_EDI)!=old.edi)abort();
 rx_set_sp(entry+4+(kind==1?4:0));
 return result;
}
