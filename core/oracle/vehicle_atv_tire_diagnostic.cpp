/* Read-only original interior hooks and native host-value observer.
 * RX_ATV_TIRE_DIAG=1 RX_ATV_TIRE_DIAG_START=6275; isolate rx_Atv_TireContact. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
extern "C" void rx_atv_tire_set_observer(void(*)(uint32_t,const float*,const float*));
using namespace reflex_port;
namespace {
uint32_t call_number;bool selected;
uint32_t read_word(reflex_vm::Machine& m,uint32_t p){return uint32_t(m.mem.read(p,4));}
void show(const char* side,uint32_t stage,const float* a,const float* b){
 if(!selected)return;
 std::fprintf(stderr,"[atv-tire-diag] %s call%u stage%u A",side,call_number,stage);
 for(unsigned i=0;i<4;i++)std::fprintf(stderr," %08x",rx_oracle::bits(a[i]));
 std::fprintf(stderr," B");for(unsigned i=0;i<4;i++)std::fprintf(stderr," %08x",rx_oracle::bits(b[i]));std::fprintf(stderr,"\n");
}
void native_values(uint32_t stage,const float* a,const float* b){show("native",stage,a,b);}
void original_values(reflex_vm::Machine& m,uint32_t stage,int xa,int xb,int stackb=-1){
 float a[4],b[4];std::memcpy(a,&m.xmm[xa],16);
 if(stackb>=0)for(unsigned i=0;i<4;i++)b[i]=rx_oracle::f32(read_word(m,m.r[4]+uint32_t(stackb)+4*i));
 else std::memcpy(b,&m.xmm[xb],16);show("original",stage,a,b);
}
uint32_t original_base(reflex_vm::Machine& m){original_values(m,1,0,0,0x220);return reflex_vm::RUN_ORIGINAL;}
uint32_t original_demand(reflex_vm::Machine& m){original_values(m,2,0,7);return reflex_vm::RUN_ORIGINAL;}
uint32_t original_slide(reflex_vm::Machine& m){
 float a[4],b[4];for(unsigned i=0;i<4;i++){a[i]=rx_oracle::f32(read_word(m,m.r[4]+0x1d0+4*i));b[i]=rx_oracle::f32(read_word(m,m.r[4]+0x1f0+4*i));}show("original",3,a,b);return reflex_vm::RUN_ORIGINAL;
}
uint32_t original_clip(reflex_vm::Machine& m){original_values(m,4,0,5);return reflex_vm::RUN_ORIGINAL;}
uint32_t original_force(reflex_vm::Machine& m){original_values(m,5,0,5);return reflex_vm::RUN_ORIGINAL;}
}
void atv_tire_diagnostic_begin(reflex_vm::Machine& m){
 if(!std::getenv("RX_ATV_TIRE_DIAG"))return;
 static bool installed=false;if(!installed){runtime_native(m,0x0049b39c,original_base);runtime_native(m,0x0049bc35,original_demand);runtime_native(m,0x0049c363,original_slide);runtime_native(m,0x0049c76f,original_clip);runtime_native(m,0x0049c781,original_force);rx_atv_tire_set_observer(native_values);installed=true;}
 ++call_number;uint32_t start=6275;if(const char* value=std::getenv("RX_ATV_TIRE_DIAG_START"))start=uint32_t(std::strtoul(value,nullptr,10));
 selected=call_number>=start&&call_number<start+12;
 if(selected)std::fprintf(stderr,"[atv-tire-diag] entry call%u S%08x W%08x body%u\n",call_number,m.r[7],m.arg(1),read_word(m,m.arg(1)));
}
