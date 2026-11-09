/* Extra actual-world water stimuli, staged outside build inputs. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace reflex_port;
namespace {
uint32_t tick;uint32_t rd(uint32_t a){return rx_rd32(a);}void wr(uint32_t a,uint32_t b){rx_wr32(a,b);}float fp(uint32_t a){uint32_t u=rd(a);float f;std::memcpy(&f,&u,4);return f;}void wf(uint32_t a,float f){uint32_t u;std::memcpy(&u,&f,4);wr(a,u);}
struct Save{uint32_t a;std::vector<uint32_t>b;Save(uint32_t p,uint32_t n):a(p){for(uint32_t i=0;i<n;i+=4)b.push_back(rd(p+i));}void restore(){for(size_t i=0;i<b.size();i++)wr(a+uint32_t(i)*4,b[i]);}~Save(){restore();}};
}
RX_PORT(0x0084ed20,AtvWaterProbeHook,"thunk",0){
 if(!std::getenv("RX_ATV_WATER_PROBE")||++tick!=650)return reflex_vm::RUN_ORIGINAL;
 uint32_t v=0,g=rd(0xa946b8);
 if(g)for(unsigned i=0;i<16;i++){uint32_t p=g+0x7460+0x12a0*i;if(!m.mem.mapped(p+0x1ac))continue;uint32_t x=rd(p+0x1ac);if(x&&m.mem.mapped(x)&&rd(x)==0x93d0ec){v=x;break;}}
 if(!v){std::fprintf(stderr,"[atv-water] ERROR no original ATV\n");return reflex_vm::RUN_ORIGINAL;}
 uint32_t regs[8];reflex_vm::Xmm xmm[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);uint32_t flags=m.flags;auto fpu=m.fpu;
 {
  uint32_t s=rd(v+0x1700),ra=rd(v+0x1a0);Save V(v,6000),S(s,0x9bd0),R(ra+0x5b568,4);
  auto restore=[&](){V.restore();S.restore();R.restore();};
  auto call=[&](uint32_t fn,uint32_t eax,uint32_t esi,std::vector<uint32_t>a){rx_regs in{},out{};in.r[0]=eax;in.r[1]=v;in.r[6]=esi;rx_call_original_ex(fn,&in,&out,int(a.size()),a.data());return out.r[0];};
  if(!(rd(0xd72aa8)&1))std::fprintf(stderr,"[atv-water] warm singleton not initialized; leaving cold constructor untested\n");
  else call(0x424340,0,0,{});
  for(uint32_t bits:{0u,0x80000000u,0x3f800000u,0xbf800000u,0x7fc12345u}){wf(s+0x2f00,.25f);call(0x45f500,v,0,{bits});restore();}
  uint32_t found=0;
  if(rd(0xd72aa8)&1){
   uint32_t begin=rd(0xd72aac+0x10),end=rd(0xd72aac+0x14),gl=rd(0xa95844),tag=rd(gl+0x1c+4*rd(gl+4));
   if(end>=begin&&end-begin<=4096)for(uint32_t p=begin;p<end;p+=4){
    uint32_t entry=rd(p);if(!entry||rd(entry+0x47c)!=tag)continue;uint32_t patch=rd(entry+0x54);if(!patch)continue;
    wf(v+0x1660,(fp(patch+0xb0)+fp(patch+0xb8))*.5f);wf(v+0x1664,0);wf(v+0x1668,(fp(patch+0xb4)+fp(patch+0xbc))*.5f);
    if(call(0x7efef0,0,0xd72aac,{v+0x1660})){found=1;break;}
   }
  }
  if(found){
   const float x=fp(v+0x1660),y=fp(v+0x1664),z=fp(v+0x1668);
   const float scale=fp(0x9d789c); /* original submerged-depth divisor, 2.5 in this executable */
   const float anchorDelta[]={1.0f,-2.0f,-(1.0f+2.0f*scale)};
   for(unsigned branch=0;branch<3;branch++){
    restore();wr(s+0xb0,1);wr(s+0x2e80,1);wf(v+0x150,0);wr(v+0x16d0,1);
    wr(v+0x1310,0);wr(v+0x1314,0);wf(v+0x1318,4);wf(v+0x131c,0);wr(ra+0x5b568,0);wf(s+0x2f00,0);
    for(uint32_t q:{0x730u,0x7c0u}){wf(s+q,0);wf(s+q+4,0);wf(s+q+8,0);wf(s+q+12,1);}
    for(uint32_t off:{0x9410u,0x9450u,0x9560u,0x95a0u,0x9170u,0x91b0u,0x92c0u,0x9300u,0x9470u,0x95c0u,0x91d0u,0x9320u})for(unsigned k=0;k<4;k++)wf(s+off+4*k,0);
    for(uint32_t anchor:{0x400u,0x4a0u,0x2c0u,0x360u}){wf(s+anchor,x);wf(s+anchor+4,y+anchorDelta[branch]);wf(s+anchor+8,z);wf(s+anchor+12,1);}
    call(0x487b30,0,0,{v,0x3c888889});
    std::fprintf(stderr,"[atv-water] authored patch case%u xyz=(%.4f,%.4f,%.4f),anchorDelta=%.4f,depth=%.4f,wet=%u,entered=%u,rider=%x,force=%.6f\n",branch,x,y,z,anchorDelta[branch],fp(v+0x131c),rd(v+0x1310),rd(v+0x1314),rd(ra+0x5b568),fp(s+0x2f00));
    if(branch==2&&(!(fp(v+0x131c)>.5f)||!rd(v+0x1314)||rd(ra+0x5b568)!=0x16))std::fprintf(stderr,"[atv-water] ERROR deep authored stimulus did not establish entered immersion\n");
    restore();
   }
  }else std::fprintf(stderr,"[atv-water] no queryable current-tag authored patch; full wet branch NOT COVERED\n");
  std::fprintf(stderr,"[atv-water] five force cases and optional warm singleton complete, actual state restored\n");
 }
 std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu=fpu;return reflex_vm::RUN_ORIGINAL;
}
