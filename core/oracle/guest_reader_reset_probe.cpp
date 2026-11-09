/* Restored guest reader reset fixtures: guest pages, allocator/host state, full CPU. */
#include "oracle.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <stdexcept>
#include <functional>
#include <fstream>
#include <iterator>
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
RX_PORT(0x0084ed20,GuestReaderResetProbe,"thunk",0){
 const char*scope=std::getenv("RX_GUEST_READER_RESET_PROBE");if(!scope||++frame!=20)return reflex_vm::RUN_ORIGINAL;
 uint32_t regs[8];reflex_vm::Xmm xmm[8];uint64_t mmx[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);std::memcpy(mmx,m.mmx,sizeof mmx);
 auto flags=m.flags;auto fpu=m.fpu;auto cw=m.fpu_cw,sw=m.fpu_sw;auto mxcsr=m.mxcsr;bool mmx_active=m.mmx_active;
 auto host_restore=snapshot_host_state(m);
 if(!host_restore){std::fprintf(stderr,"[guest-reader-reset] ERROR no reversible host snapshot\n");return reflex_vm::RUN_ORIGINAL;}
 Pages memory(m.mem);
 auto restore=[&](){memory.restore();host_restore();std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);std::memcpy(m.mmx,mmx,sizeof mmx);m.flags=flags;m.fpu=fpu;m.fpu_cw=cw;m.fpu_sw=sw;m.mxcsr=mxcsr;m.mmx_active=mmx_active;};
 struct RestoreOnExit{std::function<void()> fn;~RestoreOnExit(){fn();}} restoreOnExit{restore};
 unsigned directResets=0,cases=0,loadedCases=0,customCases=0;
 auto check=[](bool ok,const char*what){if(!ok){std::fprintf(stderr,"[guest-reader-reset] ERROR %s\n",what);throw std::runtime_error(what);}};
 auto invoke=[&](uint32_t fn,uint32_t eax,uint32_t ecx,uint32_t esi,uint32_t edi,std::initializer_list<uint32_t> args){rx_regs in{},out{};for(unsigned i=0;i<8;i++)in.r[i]=regs[i];in.r[0]=eax;in.r[1]=ecx;in.r[6]=esi;in.r[7]=edi;std::vector<uint32_t>a(args);rx_call_original_ex(fn,&in,&out,int(a.size()),a.empty()?nullptr:a.data());return out;};
 auto alloc=[&](uint32_t bytes){uint32_t p=invoke(0x78fbe0,0,0,0,0,{bytes}).r[0];check(p!=0,"original allocation");return p;};
 auto init=[&](unsigned lock){invoke(rd(0x916014),0,0,0,0,{0xd61704});rx_wr8(0xda67d8,uint8_t(lock));};
 auto reader=[&](uint32_t allocator){uint32_t p=alloc(0x480);for(unsigned i=0;i<0x480;i++)rx_wr8(p+i,0xa5);auto out=invoke(0x824e70,0,0,p,0,{allocator});check(out.r[0]==p&&rd(p)==0x9753dc&&rd(p+0x45c)==allocator,"original reader constructor");return p;};
 const uint32_t cleared[]={0x10,4,8,0xc,0x14,0x18,0x1c,0x20,0x24,0x30,0x34,0x28,0x2c,0x38,0x3c,0x33c};
 auto reset=[&](uint32_t r){
  uint32_t live=rd(0xa95950),allocator=rd(r+0x45c);std::array<uint8_t,0x480>before{};m.mem.read_bytes(r,before.data(),before.size());
  invoke(0x827e00,0,0,r,0,{});directResets++;
  for(uint32_t off:cleared)check(rd(r+off)==0,"reset ownership field cleared");
  check(rd(r+0x45c)==allocator&&rd(0xa95950)==live,"reset retains allocator/live-reader count");
  for(unsigned i=0;i<before.size();i++){bool mutableByte=false;for(uint32_t off:cleared)if(i>=off&&i<off+4)mutableByte=true;check(mutableByte||uint8_t(m.mem.read(r+i,1))==before[i],"reader nonowned bytes/canary unchanged");}
 };
 check(std::strcmp(scope,"empty")==0||std::strcmp(scope,"full")==0,"scope must be empty or full");
 bool full=std::strcmp(std::getenv("RX_GUEST_READER_RESET_PROBE"),"full")==0;
 const char*paths[2]={std::getenv("RX_GUEST_RESET_BXML0"),std::getenv("RX_GUEST_RESET_BXML1")};
 if(full)check(paths[0]&&*paths[0]&&paths[1]&&*paths[1],"full scope requires existing host uncompressed/compressed asset paths");
 for(unsigned lock=0;lock<2;lock++){
  init(lock);uint32_t r=reader(0);reset(r);reset(r);cases++;restore();
  init(lock);uint32_t map=alloc(0x20);invoke(0x82ab20,0,0,map,4,{0x3f400000,1});uint32_t al=rd(map+0x18);check(al&&rd(al)==0x9753b0&&rd(rd(al)+0xc)==0x7e3be0,"genuine StdMemoryAllocator vf3");r=reader(al);reset(r);reset(r);customCases++;cases++;restore();
  if(full)for(unsigned compressed=0;compressed<2;compressed++){
   init(lock);r=reader(0);
   std::ifstream input(paths[compressed],std::ios::binary);check(input.is_open(),"existing asset input available");
   std::vector<uint8_t> asset((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
   check(!input.bad()&&asset.size()>=32&&asset.size()<=0x7fffffffu,"asset size/read");
   check(std::memcmp(asset.data(),"BXML",4)==0,"asset BXML magic");
   uint32_t version=uint32_t(asset[4])|(uint32_t(asset[5])<<8),flags=uint32_t(asset[6])|(uint32_t(asset[7])<<8);
   check(version==1002&&(flags&1)==compressed&&!(flags&6),"source expected compression/PC flags");
   // Genuine original factory owns the real MemoryInputStream; its original
   // initialization allocates/owns the backing bytes. No manual vtable/object.
   uint32_t owner=alloc(0x20);check(invoke(0x539fa0,0,0,owner,0,{}).r[0]==owner,"original TDF owner constructor");
   uint32_t stream=rd(owner+0x10);check(stream&&rd(stream)==0x974d1c&&rd(stream+0x14)==1,"original owned MemoryInputStream");
   invoke(0x810900,0,0,stream,0,{uint32_t(asset.size())});
   uint32_t bytes=rd(stream+4);check(bytes&&rd(stream+8)==asset.size()&&rd(stream+0xc)==0&&rd(stream+0x14)==1&&rd(stream+0x18)==0,"original memory allocation/ownership");
   m.mem.write_bytes(bytes,asset.data(),asset.size());
   check(invoke(0x8252c0,0,0,0,0,{r,stream}).r[0]!=0,"genuine original guest-stream BXML load");
   check(rd(r+4)>0&&rd(r+8)&&rd(r+0x10)&&rd(r+0x28)>0,"genuine populated reader strings/hash/nodes");
   check(rd(r+0x33c)==0,"actual external memory-stream ownership");
   uint32_t position=rd(stream+0xc),lastRead=rd(stream+0x18);check(position>0&&position<=asset.size()&&lastRead<=asset.size(),"source consumed with bounded cursor/last read");
   std::fprintf(stderr,"[guest-reader-reset] loaded lock%u compressed%u strings%u nodes%u sourceBytes%zu cursor%u lastRead%u source%s allocatorVT%08x\n",lock,compressed,rd(r+4),rd(r+0x28),asset.size(),position,lastRead,paths[compressed],rd(rd(rd(r+0x10)+0x18)));
   reset(r);reset(r);
   check(rd(stream+4)==bytes&&rd(stream+8)==asset.size()&&rd(stream+0xc)==position&&rd(stream+0x18)==lastRead&&rd(stream+0x14)==1,"reset does not close external MemoryInputStream");
   for(size_t i=0;i<asset.size();i++)check(uint8_t(m.mem.read(bytes+uint32_t(i),1))==asset[i],"byte-identical source retained");
   // Actual typed owner destructor releases owned source/stream/owner. Reader
   // destructor owns its independent buffers and live-reader counter.
   invoke(0x824fe0,0,0,r,0,{});invoke(0x539ff0,0,owner,0,0,{1});
   loadedCases++;cases++;restore();

  }
 }
 check(directResets==(full?16u:8u)&&cases==(full?8u:4u)&&customCases==2&&loadedCases==(full?4u:0u),"explicit fixture scope/counts");
 restore();std::fprintf(stderr,"[guest-reader-reset] PASS scope%s cases%u directResets%u loaded%u custom%u; embedded-owned-FILE branch unproven\n",full?"full":"empty-only",cases,directResets,loadedCases,customCases);
 return reflex_vm::RUN_ORIGINAL;
}
