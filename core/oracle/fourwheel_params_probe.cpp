/* Restored genuine four-wheel parameter fixtures: guest pages, allocator/host state, full CPU. */
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
RX_PORT(0x0084ed20,FourWheelParamsProbe,"thunk",0){
 if(!std::getenv("RX_FOURWHEEL_PARAMS_PROBE")||++frame!=20)return reflex_vm::RUN_ORIGINAL;
 uint32_t regs[8];reflex_vm::Xmm xmm[8];uint64_t mmx[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);std::memcpy(mmx,m.mmx,sizeof mmx);
 auto flags=m.flags;auto fpu=m.fpu;auto cw=m.fpu_cw,sw=m.fpu_sw;auto mxcsr=m.mxcsr;bool mmx_active=m.mmx_active;
 auto host_restore=snapshot_host_state(m);
 if(!host_restore)throw std::runtime_error("fourwheel parameters require reversible host snapshot");
 Pages memory(m.mem);
 auto restore=[&](){memory.restore();host_restore();std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);std::memcpy(m.mmx,mmx,sizeof mmx);m.flags=flags;m.fpu=fpu;m.fpu_cw=cw;m.fpu_sw=sw;m.mxcsr=mxcsr;m.mmx_active=mmx_active;};
 struct RestoreOnExit{std::function<void()> fn;~RestoreOnExit(){fn();}} restoreOnExit{restore};
 auto check=[](bool ok,const char*what){if(!ok){std::fprintf(stderr,"[fourwheel-params] ERROR %s\n",what);throw std::runtime_error(what);}};
 unsigned alignment=0,cases=0,positive=0,rejected=0,empty=0,direct=0;unsigned targetCalls[3]={0,0,0},entryResidues[3]={0,0,0};
 auto invoke=[&](uint32_t fn,uint32_t eax,uint32_t ecx,uint32_t esi,uint32_t edi,std::initializer_list<uint32_t> args){rx_regs in{},out{};for(unsigned i=0;i<8;i++)in.r[i]=regs[i];in.r[4]-=alignment;in.r[0]=eax;in.r[1]=ecx;in.r[6]=esi;in.r[7]=edi;std::vector<uint32_t>a(args);const uint32_t savedEsp=rx_sp(),callerEsp=regs[4]-alignment;
  struct StackRestore{uint32_t sp;~StackRestore(){rx_set_sp(sp);}} stackRestore{savedEsp};
  rx_set_sp(callerEsp);check(rx_sp()==callerEsp,"actual caller ESP selected");
  rx_call_original_ex(fn,&in,&out,int(a.size()),a.empty()?nullptr:a.data());
  if(fn==0x4cb3f0||fn==0x4b0bc0||fn==0x44f7a0){check(args.size()==2&&out.r[4]==callerEsp,"target original RET8/caller stack balance");}
  return out;};
 auto alloc=[&](uint32_t n){uint32_t p=invoke(0x78fbe0,0,0,0,0,{n}).r[0];check(p&&!(p&15),"original aligned allocation");return p;};
 auto init=[&](unsigned lock){invoke(rd(0x916014),0,0,0,0,{0xd61704});rx_wr8(0xda67d8,uint8_t(lock));};
 auto reader=[&](){uint32_t r=alloc(0x480);auto out=invoke(0x824e70,0,0,r,0,{0});check(out.r[0]==r&&rd(r)==0x9753dc&&!rd(r+0x45c),"original XMLReader ctor");return r;};
 auto parameter=[&](unsigned which){uint32_t raw=alloc(0x2700),p=raw+16;
  for(unsigned i=0;i<16;i++){rx_wr8(raw+i,0x6a);rx_wr8(p+0x26e0+i,0x9b);}
  invoke(which==0?0x4c9d40:0x4af510,0,0,p,0,{});
  check(rd(p+0x400)==98,"original parameter registration count98");
  for(unsigned i=0;i<98;i++){uint32_t q=rd(p+4*i);check(q>=p+0x410&&q<p+0x26e0&&rd(q+0x40)<=3,"real parameter record within owned object");}
  return p;};
 auto canaries=[&](uint32_t p){for(unsigned i=0;i<16;i++)check(m.mem.read(p-16+i,1)==0x6a&&m.mem.read(p+0x26e0+i,1)==0x9b,"parameter canaries");};
 auto record=[&](uint32_t p,const char*name){for(unsigned i=0;i<rd(p+0x400);i++){uint32_t q=rd(p+4*i);unsigned k=0;while(name[k]&&m.mem.read(q+k,1)==uint8_t(name[k]))k++;if(!name[k]&&!m.mem.read(q+k,1))return q;}check(false,"expected authored registered name");return 0u;};
 const uint32_t targets[3]={0x4cb3f0,0x4b0bc0,0x44f7a0};
 auto apply=[&](unsigned which,uint32_t p,uint32_t r){const uint32_t entryEsp=regs[4]-alignment-12;entryResidues[which]|=1u<<(entryEsp&15);auto out=invoke(targets[which],0,0,0,0,{p,r});check(rd(entryEsp+4)==p&&rd(entryEsp+8)==r,"actual target stack argument slots/residue");direct++;targetCalls[which]++;canaries(p);return out.r[0];};
 const char*keys[4]={"RX_FOURWHEEL_UTV_BXML","RX_FOURWHEEL_BUGGY_BXML","RX_FOURWHEEL_TRUCK_BXML","RX_FOURWHEEL_TRUCKLITE_BXML"};
 std::vector<uint8_t> assets[4];const char*paths[4];
 for(unsigned file=0;file<4;file++){
  paths[file]=std::getenv(keys[file]);check(paths[file]&&*paths[file],"all four genuine fixture paths required");
  std::ifstream in(paths[file],std::ios::binary);check(in.is_open(),"existing tuning input available");
  assets[file]=std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
  check(!in.bad()&&assets[file].size()>36&&assets[file].size()<65536,"tuning file read/size");
  check(std::memcmp(assets[file].data(),"BXML",4)==0&&assets[file][4]==0xea&&assets[file][5]==3&&assets[file][6]==1&&assets[file][7]==0,"actual installed PC compressed BXML1002");
 }
 for(unsigned lock=0;lock<2;lock++){
  alignment=lock*4;
  for(unsigned file=0;file<4;file++){
   init(lock);unsigned which=file<2?file:2;uint32_t p=parameter(which),r=reader(),owner=alloc(0x20);
   check(invoke(0x539fa0,0,0,owner,0,{}).r[0]==owner,"original TDF owner ctor");uint32_t stream=rd(owner+0x10);
   check(stream&&rd(stream)==0x974d1c&&rd(stream+0x14)==1,"original owned MemoryInputStream");
   auto& asset=assets[file];invoke(0x810900,0,0,stream,0,{uint32_t(asset.size())});uint32_t bytes=rd(stream+4);
   check(bytes&&rd(stream+8)==asset.size()&&!rd(stream+0xc)&&rd(stream+0x14)==1,"original source buffer");m.mem.write_bytes(bytes,asset.data(),asset.size());
   check(invoke(0x8252c0,0,0,0,0,{r,stream}).r[0]&&rd(r+4)&&rd(r+8)&&rd(r+0x10)&&rd(r+0x28)==125&&!rd(r+0x33c),"genuine populated125-node reader");
   uint32_t pos=rd(stream+0xc),last=rd(stream+0x18);check(pos>0&&pos<=asset.size()&&last<=asset.size(),"genuine source consumed bounds");
   uint32_t gravity=record(p,"VehicleSim_Gravity"),speed=record(p,"VehicleSim_SpeedFilterTimeConstant"),curve=record(p,"VehicleSim_GroundDragCurve"),trip=record(p,"FrontTires_TerrainDrag");
   check(apply(which,p,r)==1,"matching root accepted");
   // Independent authored checks: exact simple float values, real curve and all
   // three distinct scalar rows. No expected bytes copied from candidate output.
   const uint32_t gY[4]={0xc2000000,0xc200cccd,0xc200cccd,0xc200cccd};
   const uint32_t curveY1[4]={0x41980000,0x428e0000,0x428e0000,0x42300000};
   const uint32_t mid[4]={0x40400000,0x40f00000,0x41700000,0x41600000},high[4]={0x42700000,0x42960000,0x43160000,0x430c0000};
   check(rd(gravity+0x54)==gY[file]&&rd(speed+0x44)==0x3f800000,"authored gravity/scalar applied");
   check(rd(curve+0x44)==5&&rd(curve+0x48)==0&&rd(curve+0x70)==0&&rd(curve+0x74)==curveY1[file],"authored ground-curve count/pairs applied");
   check(rd(trip+0x44)==0&&rd(trip+0x48)==mid[file]&&rd(trip+0x4c)==high[file],"three authored rows applied in order");
   check(apply(which,p,r)==1,"repeat apply/cursor reuse");
   check(rd(stream+4)==bytes&&rd(stream+8)==asset.size()&&rd(stream+0xc)==pos&&rd(stream+0x18)==last,"parameter walk retains external stream");
   for(size_t i=0;i<asset.size();i++)check(m.mem.read(bytes+uint32_t(i),1)==asset[i],"source bytes unchanged");
   if(lock==0&&file<2){
    std::array<uint8_t,0x26e0> before{};m.mem.read_bytes(p,before.data(),before.size());
    for(unsigned wrong=0;wrong<3;wrong++)if((file==0&&wrong!=0)||(file==1&&wrong==0)){
     check(apply(wrong,p,r)==0,"actual different vehicle root rejected");
     for(unsigned i=0;i<before.size();i++)check(m.mem.read(p+i,1)==before[i],"wrong type retains parameters");
     rejected++;cases++;
    }
   }
   std::fprintf(stderr,"[fourwheel-params] authored file%u target%08x lock%u align%u source%s cursor%u last%u records%u entryEsp%08x residue%u\n",file,targets[which],lock,alignment,paths[file],pos,last,rd(p+0x400),regs[4]-alignment-12,(regs[4]-alignment-12)&15);
   invoke(0x824fe0,0,0,r,0,{});invoke(0x539ff0,0,owner,0,0,{1});invoke(0x78fc50,0,0,0,0,{p-16});positive++;cases++;restore();
  }
 }
 // Genuine empty original reader is a valid root-failure fixture. A null
 // reader is not: the original dereferences its vtable before checking root.
 alignment=0;
 for(unsigned which=0;which<3;which++){
  init(0);uint32_t p=parameter(which),r=reader();std::array<uint8_t,0x26e0> before{};m.mem.read_bytes(p,before.data(),before.size());
  check(apply(which,p,r)==0,"genuine empty root rejected");for(unsigned i=0;i<before.size();i++)check(m.mem.read(p+i,1)==before[i],"empty root retains params");
  invoke(0x824fe0,0,0,r,0,{});invoke(0x78fc50,0,0,0,0,{p-16});empty++;cases++;restore();
 }
 check(cases==14&&positive==8&&rejected==3&&empty==3&&direct==22&&targetCalls[0]==6&&targetCalls[1]==6&&targetCalls[2]==10,"explicit direct positive/empty scope counters");
 for(unsigned which=0;which<3;which++){
  const unsigned expected=(1u<<((regs[4]-12)&15))|(1u<<((regs[4]-16)&15));
  check(entryResidues[which]==expected,"two actual target entry ESP residues exercised");
 }
 restore();std::fprintf(stderr,"[fourwheel-params] PASS cases%u authored%u wrongType%u empty%u direct%u targets%u/%u/%u; invalid/replication1or2/kind4or5 unproven\n",cases,positive,rejected,empty,direct,targetCalls[0],targetCalls[1],targetCalls[2]);
 return reflex_vm::RUN_ORIGINAL;
}
