/* Restored tuning registry fixtures: guest pages, allocator/host state, full CPU. */
#include "oracle.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <stdexcept>
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
RX_PORT(0x0084ed20,TuningRegistryProbe,"thunk",0){
 if(!std::getenv("RX_TUNING_REGISTRY_PROBE")||++frame!=20)return reflex_vm::RUN_ORIGINAL;
 uint32_t regs[8];reflex_vm::Xmm xmm[8];uint64_t mmx[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);std::memcpy(mmx,m.mmx,sizeof mmx);
 auto flags=m.flags;auto fpu=m.fpu;auto cw=m.fpu_cw,sw=m.fpu_sw;auto mxcsr=m.mxcsr;bool mmx_active=m.mmx_active;
 auto host_restore=snapshot_host_state(m);
 if(!host_restore){std::fprintf(stderr,"[tuning-registry] ERROR no reversible host snapshot\n");return reflex_vm::RUN_ORIGINAL;}
 // Captured boot CRT may cache this import instead of requesting the runtime's
 // ENCODE_IDENTITY address. Match boot.cpp828/runtime.cpp523 for boot images.
 auto oldEncode=m.imports.find("kernel32.dll!EncodePointer");
 struct RestoreEncode{reflex_vm::Machine& m;bool had;reflex_vm::Native previous;
  ~RestoreEncode(){if(had)m.imports["kernel32.dll!EncodePointer"]=previous;else m.imports.erase("kernel32.dll!EncodePointer");}
 } encode{m,oldEncode!=m.imports.end(),oldEncode!=m.imports.end()?oldEncode->second:reflex_vm::Native{}};
 if(!encode.had)m.imports["kernel32.dll!EncodePointer"]=[](reflex_vm::Machine& mm){return mm.ret(4,mm.arg(0));};
 Pages memory(m.mem);
 auto restore=[&](){memory.restore();host_restore();std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);std::memcpy(m.mmx,mmx,sizeof mmx);m.flags=flags;m.fpu=fpu;m.fpu_cw=cw;m.fpu_sw=sw;m.mxcsr=mxcsr;m.mmx_active=mmx_active;};
 std::fprintf(stderr,"[tuning-registry] begin restored probe\n");
 unsigned cases=0,getters=0,drains=0,cleanups=0,exits=0;
 auto check=[](bool ok,const char* what){if(!ok){std::fprintf(stderr,"[tuning-registry] ERROR %s\n",what);throw std::runtime_error(what);}};
 auto invoke=[&](uint32_t fn,uint32_t esi,std::initializer_list<uint32_t> args){
  if(fn==0x42fed0)getters++;if(fn==0x4db6d0)drains++;if(fn==0x42ff00)cleanups++;if(fn==0x911ed0)exits++;
  rx_regs in{},out{};for(unsigned i=0;i<8;i++)in.r[i]=regs[i];in.r[6]=esi;std::vector<uint32_t>a(args);rx_call_original_ex(fn,&in,&out,int(a.size()),a.empty()?nullptr:a.data());return out;};
 auto alloc=[&](uint32_t n){auto p=invoke(0x0078fbe0,0,{n}).r[0];check(p!=0,"allocation failed");for(uint32_t i=0;i<n;i+=4)wr(p+i,0x584d4c51);return p;};
 auto initcs=[&](uint32_t cs){invoke(rd(0x00916014),0,{cs});};
 // Captured CRT onexit storage belongs to the live boot heap, whose size the
 // restored frame runtime cannot query. Rehome the exact used callback range
 // into a genuine original malloc block, retaining original pointer encoding.
 auto prepare=[&](unsigned lock){
  initcs(0xd61704);rx_wr8(0xda67d8,uint8_t(lock));
  uint32_t beginSlot=0xdcf768,endSlot=0xdcf764;
  uint32_t begin=invoke(rd(0x9163a0),0,{rd(beginSlot)}).r[0];
  if(begin==0xffffffffu){
   // EXE8f9f38 delegates to DLL_onexit; preserve that sentinel/branch.
   // DLL7858c96e decodes begin785bd664 and end785bd660.
   beginSlot=0x785bd664;endSlot=0x785bd660;
   begin=invoke(rd(0x9163a0),0,{rd(beginSlot)}).r[0];
  }
  uint32_t end=invoke(rd(0x9163a0),0,{rd(endSlot)}).r[0];
  check(begin!=0xffffffffu && end!=0xffffffffu,"onexit invalid selected table sentinel");
  check(end>=begin && end-begin<=0x100000u && ((end-begin)&3u)==0,"onexit used range");
  uint32_t bytes=end-begin;check(bytes==0 || begin!=0,"onexit null used range");
  uint32_t live=invoke(rd(0x916290),0,{bytes+16}).r[0];check(live!=0,"onexit malloc");
  for(uint32_t i=0;i<bytes;i+=4){check(m.mem.mapped(begin+i),"onexit source range mapped");wr(live+i,rd(begin+i));}
  wr(beginSlot,invoke(rd(0x916394),0,{live}).r[0]);
  wr(endSlot,invoke(rd(0x916394),0,{live+bytes}).r[0]);
 };
 auto fresh=[&](){wr(0xd72ad4,rd(0xd72ad4)&~1u);check(invoke(0x0042fed0,0,{}).r[0]==0xd72ad8,"getter address");check(rd(0xd92d2c)!=0 && rd(rd(0xd92d2c))==0xd92d2c,"getter proxy");};
 // Original initialization/allocation/OS calls construct all ownership objects.
 auto queue=[&](uint32_t q,unsigned n){uint32_t owner=alloc(0x20200);for(uint32_t i=0;i<0x20200;i+=4)wr(owner+i,0);initcs(owner+0x201b0);
  for(unsigned i=0;i<n;i++)invoke(0x007905d0,owner,{alloc(16)});
  for(unsigned i=0;i<12;i+=4)wr(q+i,rd(owner+0x201c8+i));
 };
 for(unsigned lock=0;lock<2;lock++){
  prepare(lock);
  for(uint32_t guard:{0u,2u,0x80000000u}){std::fprintf(stderr,"[tuning-registry] getter lock%u guard%08x\n",lock,guard);wr(0xd72ad4,guard);check(invoke(0x0042fed0,0,{}).r[0]==0xd72ad8,"first getter address");uint32_t proxy=rd(0xd92d2c);check(proxy&&rd(proxy)==0xd92d2c,"first getter proxy");check(rd(0xd72ad4)==(guard|1),"guard preserves high bits");check(invoke(0x0042fed0,0,{}).r[0]==0xd72ad8&&rd(0xd92d2c)==proxy,"repeat getter stable proxy");cases+=2;restore();prepare(lock);}
  for(unsigned n:{0u,1u,3u}){uint32_t owner=alloc(0x20200);for(uint32_t i=0;i<0x20200;i+=4)wr(owner+i,0);initcs(owner+0x201b0);for(unsigned i=0;i<n;i++)invoke(0x007905d0,owner,{alloc(16)});uint32_t tail=rd(owner+0x201d0);std::vector<uint32_t>payloads;for(uint32_t node=rd(owner+0x201cc);node;node=rd(node+8))payloads.push_back(rd(node));invoke(0x004db6d0,owner+0x201c8,{});check(rd(owner+0x201c8)==0,"drained count");check(rd(owner+0x201d0)==tail&&rd(owner+0x201cc)==tail,"original stale head/tail");for(auto p:payloads)check(rd(p)==0x584d4c51,"queue payload remains live");cases++;restore();prepare(lock);}
  for(uint32_t target:{0x0042ff00u,0x00911ed0u})for(unsigned populated:{0u,1u}){
   fresh();
   if(populated){
    // Actual kernel event handles, not fabricated values. Cleanup never waits.
    for(uint32_t h:{0xd72adcu,0xd92c28u,0xd92c38u}){uint32_t handle=invoke(rd(0x00916038),0,{0,1,0,0}).r[0];check(handle&&handle!=0xffffffffu,"CreateEvent failed");wr(h,handle);}
    // Real initialized, unheld sections. Conditional Delete branches need
    // an original producer proving their nonzero+12 flags; not forged here.
    for(uint32_t cs:{0xd92c10u,0xd92c40u,0xd92c64u,0xd92c88u})initcs(cs);
    queue(0xd92c2c,1);queue(0xd92c58,2);queue(0xd92c7c,3);queue(0xd92ca0,1);
    uint32_t buffer=alloc(32);wr(0xd92d38,buffer);wr(0xd92d3c,buffer+16);wr(0xd92d40,buffer+32);
   }
   uint32_t proxy=rd(0xd92d2c);invoke(target,0,{});
   for(uint32_t q:{0xd92c2cu,0xd92c58u,0xd92c7cu,0xd92ca0u})check(rd(q)==0,"cleanup queue count");
   for(uint32_t h:{0xd72adcu,0xd92c28u,0xd92c38u,0xd92d38u,0xd92d3cu,0xd92d40u})check(rd(h)==0,"cleanup clears handles/endpoints");
   check(rd(0xd92d2c)==proxy,"original proxy slot remains dangling");cases++;restore();prepare(lock);
  }
 }
 check(cases==26&&getters==20&&drains==6&&cleanups==4&&exits==4,"explicit direct invocation counts");
 restore();std::fprintf(stderr,"[tuning-registry] PASS cases%u getters%u drains%u cleanup%u exit%u\n",cases,getters,drains,cleanups,exits);
 return reflex_vm::RUN_ORIGINAL;
}
