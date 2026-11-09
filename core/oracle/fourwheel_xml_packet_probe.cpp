/* Restored packet publication fixtures: guest pages, allocator/host state, full CPU. */
#include "oracle.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace reflex_port;
namespace {
uint32_t frame;uint32_t rd(uint32_t a){return rx_rd32(a);}void wr(uint32_t a,uint32_t v){rx_wr32(a,v);}
struct Pages {
 reflex_vm::Memory& memory;
 struct Page{uint32_t address;std::array<uint8_t,4096> bytes;};
 std::vector<Page> saved;std::vector<uint8_t> mapped;
 explicit Pages(reflex_vm::Memory& m):memory(m),mapped(1u<<20,0){
  /* Fault in all backed pages before snapshot: later lazy touches then cannot
   * conceal a modified page. A captured world has bounded mapped memory. */
  for(uint32_t i=0;i<(1u<<20);i++)if(m.mapped(i<<12)){
   mapped[i]=1;saved.push_back(Page{});auto& p=saved.back();p.address=i<<12;
   std::memcpy(p.bytes.data(),m.page(p.address),4096);
  }
 }
 void restore(){
  for(auto& p:saved)std::memcpy(memory.page(p.address),p.bytes.data(),4096);
  for(uint32_t i=0;i<(1u<<20);i++)if(!mapped[i]&&memory.mapped(i<<12))memory.unmap(i<<12,4096);
 }
 ~Pages(){restore();}
};
}
RX_PORT(0x0084ed20,FourWheelXmlPacketProbe,"thunk",0){
 if(!std::getenv("RX_FOURWHEEL_XML_PACKET_PROBE"))return reflex_vm::RUN_ORIGINAL;
 if(++frame!=20)return reflex_vm::RUN_ORIGINAL;
 uint32_t regs[8];reflex_vm::Xmm xmm[8];uint64_t mmx[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);std::memcpy(mmx,m.mmx,sizeof mmx);
 auto flags=m.flags;auto fpu=m.fpu;auto cw=m.fpu_cw,sw=m.fpu_sw;auto mxcsr=m.mxcsr;bool mmx_active=m.mmx_active;
 auto host_restore=snapshot_host_state(m);
 if(!host_restore){std::fprintf(stderr,"[xml-packet] ERROR no reversible host snapshot\n");return reflex_vm::RUN_ORIGINAL;}
 // Model the actual system import, not a game thunk port: original-phase
 // comparisons deliberately bypass port hooks. Preserve map ownership too.
 auto previous_import=m.imports.find("ws2_32.dll!#8");
 bool had_import=previous_import!=m.imports.end();
 reflex_vm::Native old_import;if(had_import)old_import=previous_import->second;
 m.imports["ws2_32.dll!#8"]=[](reflex_vm::Machine& mm){uint32_t x=mm.arg(0);return mm.ret(4,(x>>24)|((x>>8)&0xff00u)|((x<<8)&0xff0000u)|(x<<24));};
 Pages memory(m.mem);
 auto restore=[&](){memory.restore();host_restore();std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);std::memcpy(m.mmx,mmx,sizeof mmx);m.flags=flags;m.fpu=fpu;m.fpu_cw=cw;m.fpu_sw=sw;m.mxcsr=mxcsr;m.mmx_active=mmx_active;};
 unsigned bad=0,alloc_calls=0,append_calls=0,publish_calls=0,enabled=0,disabled=0;
 auto allocate=[&](uint32_t n){uint32_t p=rx_call_original(0x0078fbe0,RX_CDECL,0,1,&n);if(!p)++bad;else for(uint32_t i=0;i<n;i++)rx_wr8(p+i,0);return p;};
 auto initcs=[&](uint32_t cs){rx_call_original(rd(0x00916014),RX_STDCALL,0,1,&cs);};
 auto invoke=[&](uint32_t fn,uint32_t esi,std::initializer_list<uint32_t> args){rx_regs in{},out{};for(unsigned i=0;i<8;i++)in.r[i]=regs[i];in.r[0]=0x12345678;in.r[1]=0x89abcdef;in.r[2]=0x76543210;in.r[6]=esi;std::vector<uint32_t>a(args);rx_call_original_ex(fn,&in,&out,int(a.size()),a.empty()?nullptr:a.data());return out;};
 auto eq=[&](uint32_t actual,uint32_t want,const char*what){if(actual!=want){++bad;std::fprintf(stderr,"[xml-packet] ERROR %s actual=%08x expected=%08x\n",what,actual,want);}};
 auto queue=[&](uint32_t owner,unsigned n){uint32_t previous=0;for(unsigned i=0;i<n;i++){uint32_t node=allocate(12),packet=allocate(16);wr(node,packet);wr(node+4,previous);wr(node+8,0);if(previous)wr(previous+8,node);else wr(owner+0x201cc,node);previous=node;}wr(owner+0x201c8,n);wr(owner+0x201d0,previous);};
 for(unsigned lock=0;lock<2;lock++)for(uint32_t n:{0u,1u,15u,16u,17u,0x20000u}){
  initcs(0x00d61704);rx_wr8(0x00da67d8,uint8_t(lock));
  auto out=invoke(0x00401f90,regs[6],{n});++alloc_calls;
  if(!out.r[0]||(out.r[0]&15))++bad;
  restore();
 }
 for(unsigned count:{0u,1u,3u})for(unsigned repeat=0;repeat<2;repeat++){
  uint32_t owner=allocate(0x20200);initcs(owner+0x201b0);queue(owner,count);
  for(unsigned j=0;j<=repeat;j++){
   uint32_t packet=allocate(16),oldtail=rd(owner+0x201d0),oldhead=rd(owner+0x201cc),oldn=rd(owner+0x201c8);
   auto out=invoke(0x007905d0,owner,{packet});++append_calls;
   uint32_t tail=rd(owner+0x201d0);
   eq(rd(owner+0x201c8),oldn+1,"append count");eq(rd(tail),packet,"append packet");eq(rd(tail+4),oldtail,"append prev");eq(rd(tail+8),0,"append next");eq(out.r[1],oldtail,"append liveECX");
   eq(rd(owner+0x201cc),oldn?oldhead:tail,"append head");if(oldtail)eq(rd(oldtail+8),tail,"oldtail next");
  }
  restore();
 }
 for(unsigned active=0;active<2;active++)for(unsigned count:{0u,1u,3u})for(uint32_t size:{0u,1u,4u,7u,31u,256u})for(unsigned lock=0;lock<2;lock++){
  initcs(0x00d61704);rx_wr8(0x00da67d8,uint8_t(lock));
  uint32_t owner=allocate(0x20200);initcs(owner+0xc);initcs(owner+0x201b0);queue(owner,count);wr(owner+0x28,active);
  uint32_t payload=size?allocate(size):0;for(uint32_t i=0;i<size;i++)rx_wr8(payload+i,uint8_t(i*37+size));
  uint32_t head=rd(owner+0x201cc),tail=rd(owner+0x201d0),n=rd(owner+0x201c8);uint32_t type=size?0x89abcdefu:10u;
  auto out=invoke(0x00790540,regs[6],{owner,type,size,payload});++publish_calls;
  if(!active){++disabled;eq(rd(owner+0x201c8),n,"disabled count");eq(rd(owner+0x201d0),tail,"disabled tail");eq(out.r[0],0x12345678,"disabled EAX");eq(out.r[2],0x76543210,"disabled EDX");}
  else{
   ++enabled;uint32_t node=rd(owner+0x201d0),packet=rd(node),buffer=rd(packet+8);eq(rd(owner+0x201c8),n+1,"publish count");eq(rd(owner+0x201cc),n?head:node,"publish head");eq(rd(node+4),tail,"publish prev");eq(rd(node+8),0,"publish next");if(tail)eq(rd(tail+8),node,"publish oldnext");eq(rd(packet),0,"packet cursor");eq(rd(packet+4),size+8,"packet size");
   auto be=[&](uint32_t p){return(uint32_t(rx_rd8(p))<<24)|(uint32_t(rx_rd8(p+1))<<16)|(uint32_t(rx_rd8(p+2))<<8)|rx_rd8(p+3);};eq(be(buffer),size+8,"network size");eq(be(buffer+4),type,"network type");for(uint32_t i=0;i<size;i++)if(rx_rd8(buffer+8+i)!=uint8_t(i*37+size))++bad;
  }
  for(uint32_t i=0;i<size;i++)if(rx_rd8(payload+i)!=uint8_t(i*37+size))++bad;
  restore();
 }
 restore();
 if(had_import)m.imports["ws2_32.dll!#8"]=old_import;else m.imports.erase("ws2_32.dll!#8");
 std::fprintf(stderr,"[xml-packet] %s allocator=%u append=%u publish=%u enabled=%u disabled=%u errors=%u; full guest/host/CPU and allocations restored; no forced heap failure\n",bad?"ERROR":"complete",alloc_calls,append_calls,publish_calls,enabled,disabled,bad);
 return reflex_vm::RUN_ORIGINAL;
}
