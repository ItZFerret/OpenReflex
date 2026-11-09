#include "bike_step_bridges.h"
#include <array>
#include <cfenv>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>
#if defined(__x86_64__)
#include <xmmintrin.h>
#endif
constexpr size_t InputSize=39096,StateSize=38656;
static void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
static std::array<uint64_t,2> fp_state(){
#if defined(__x86_64__)
 uint16_t cw;__asm__ volatile("fnstcw %0":"=m"(cw));return {_mm_getcsr(),cw};
#elif defined(__aarch64__)
 uint64_t control,status;__asm__ volatile("mrs %0, fpcr":"=r"(control));__asm__ volatile("mrs %0, fpsr":"=r"(status));return {control,status};
#endif
}
int main(int argc,char**argv){
 try{
  require(argc==4,"expected input/original/output files");std::ifstream in(argv[1],std::ios::binary),original(argv[2],std::ios::binary);std::ofstream out(argv[3],std::ios::binary);
  require(bool(in)&&bool(original)&&bool(out),"open failed");std::array<uint8_t,InputSize+4> row;std::array<uint8_t,StateSize> expected{},actual{};std::array<bool,6> tested{};unsigned count=0;
  while(in.read(reinterpret_cast<char*>(row.data()),row.size())){
   require(bool(original.read(reinterpret_cast<char*>(expected.data()),expected.size())),"missing original");uint32_t stage;std::memcpy(&stage,row.data(),4);
   std::feclearexcept(FE_ALL_EXCEPT);std::feraiseexcept(FE_INVALID|FE_DIVBYZERO);const auto fp=fp_state();const int exceptions=std::fetestexcept(FE_ALL_EXCEPT);
   const uint32_t status=reflex_draft_bike_step_bridge(stage,row.data()+4,InputSize,actual.data(),StateSize);
   if(status||actual!=expected){std::fprintf(stderr,"bridge stage%u row%u status%u\n",stage,count,status);throw std::runtime_error("original full-state mismatch");}
   require(fp_state()==fp&&std::fetestexcept(FE_ALL_EXCEPT)==exceptions,"FP controls/status changed");
   if(!tested[stage]){
    tested[stage]=true;std::vector<uint8_t> overlap(InputSize+8,0x6d);std::memcpy(overlap.data()+1,row.data()+4,InputSize);
    require(reflex_draft_bike_step_bridge(stage,overlap.data()+1,InputSize,overlap.data()+3,StateSize)==0,"unaligned overlap rejected");
    require(std::memcmp(overlap.data()+3,expected.data(),StateSize)==0&&overlap[0]==0x6d&&overlap.back()==0x6d,"overlap mismatch");
    auto inplace=row;require(reflex_draft_bike_step_bridge(stage,inplace.data()+4,InputSize,inplace.data()+4,StateSize)==0,"in-place rejected");
    require(std::memcmp(inplace.data()+4,expected.data(),StateSize)==0,"in-place mismatch");
    auto bad=row;std::memset(bad.data()+4,0,4);actual.fill(0xa5);const auto sentinel=actual;
    require(reflex_draft_bike_step_bridge(stage,bad.data()+4,InputSize,actual.data(),StateSize)==3&&actual==sentinel,"invalid class not atomic");
    require(reflex_draft_bike_step_bridge(stage,row.data()+4,InputSize-1,actual.data(),StateSize)==1&&actual==sentinel,"size not atomic");
    if(stage==3){
     auto aliased=row;
     // Force the first rotor's own position to alias the parent position.
     // The first fixed-node calculation has already happened when it rejects,
     // so this also proves whole-state atomicity after a successful earlier node.
     std::memcpy(aliased.data()+4+0x6e00+0x30,aliased.data()+4+0x6e00+0x28,4);
     require(reflex_draft_bike_step_bridge(stage,aliased.data()+4,InputSize,actual.data(),StateSize)==3&&actual==sentinel,"unsupported body alias not atomic");
    }
    std::fesetround(FE_DOWNWARD);const auto unsupported=fp_state();
    require(reflex_draft_bike_step_bridge(stage,row.data()+4,InputSize,actual.data(),StateSize)==2&&actual==sentinel,"unsupported FP accepted");
    require(fp_state()==unsupported,"unsupported FP mutated");std::fesetround(FE_TONEAREST);actual=expected;
   }
   out.write(reinterpret_cast<const char*>(actual.data()),actual.size());require(bool(out),"output write");count++;
  }
  require(in.eof()&&in.gcount()==0&&count>0,"input truncated");char extra;require(!original.get(extra),"extra original");out.close();require(bool(out),"output close");
  for(unsigned i=1;i<=5;i++)require(tested[i],"missing stage coverage");
  std::fprintf(stderr,"native full-state bridges: %u exact records; five stage overlap/atomic/FP contracts passed\n",count);return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"bridge replay: %s\n",e.what());return 2;}
}
