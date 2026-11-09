#include <array>
#include <cfenv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
extern "C" uint32_t reflex_bike_core_bridge(const void*,size_t,void*,size_t);
constexpr size_t In=38864,Out=38656;
static void require(bool x){if(!x){std::fprintf(stderr,"raw core bridge contract failed\n");std::exit(1);}}
static uint32_t word(const std::vector<uint8_t>&x,size_t offset){uint32_t v;std::memcpy(&v,x.data()+offset,4);return v;}
static void put(std::vector<uint8_t>&x,size_t offset,uint32_t v){std::memcpy(x.data()+offset,&v,4);}
int main(int argc,char**argv){
 require(argc==3);std::ifstream inputs(argv[1],std::ios::binary),expected(argv[2],std::ios::binary);require(bool(inputs)&&bool(expected));
 std::vector<uint8_t> in(In),answer(Out),result(Out),first,first_answer;unsigned cases=0;
 while(inputs.read(reinterpret_cast<char*>(in.data()),In)||inputs.gcount()){
  require(inputs.gcount()==In);expected.read(reinterpret_cast<char*>(answer.data()),Out);require(expected.gcount()==Out);
  const auto before=in;require(reflex_bike_core_bridge(in.data(),In,result.data(),Out)==0);require(result==answer&&in==before);
  if(!cases){first=in;first_answer=answer;}cases++;
 }
 require(cases>0&&inputs.eof());char byte;require(!expected.get(byte));
 for(int delta:{-int(Out),-32,-1,0,1,4,16,100,int(In-4)}){
  std::vector<uint8_t> storage(In+2*Out+32,0xcd);auto* source=storage.data()+Out;auto* dest=source+delta;std::memcpy(source,first.data(),In);const auto saved=storage;
  require(reflex_bike_core_bridge(source,In,dest,Out)==0&&std::memcmp(dest,first_answer.data(),Out)==0);
  for(size_t i=0;i<storage.size();i++)if(storage.data()+i<dest||storage.data()+i>=dest+Out)require(storage[i]==saved[i]);
 }
 result.assign(Out,0x5a);const auto untouched=result;
 require(reflex_bike_core_bridge(nullptr,In,result.data(),Out)!=0&&result==untouched);
 require(reflex_bike_core_bridge(first.data(),In-1,result.data(),Out)!=0&&result==untouched);
 require(reflex_bike_core_bridge(first.data(),In,result.data(),Out-1)!=0&&result==untouched);
 uint32_t sim=word(first,Out),root=word(first,0xd70)-sim,fixed=word(first,0xd74)-sim,rotor=word(first,0xd78)-sim;
 const std::array<std::array<uint32_t,2>,18> mutations={{{Out,1},{Out+4,0},{Out+12,1},{0,0},{0xec,3},{0xd70,0},
  {root+0xc,0},{root+0x10,6},{root+0x14,0},{fixed+0xc,1},{rotor,0},{fixed+0x30,sim+0xf0},
  {rotor+0x38,sim+rotor+0x180},{fixed+0x90+4,4},{fixed+0x90+0x10,0},
  {rotor+0x7e0+4,6},{fixed+0xbf0,0},{rotor+0x68,sim+rotor+0x7b0}}};
 for(const auto&m:mutations){auto bad=first;put(bad,m[0],m[1]);require(reflex_bike_core_bridge(bad.data(),In,result.data(),Out)!=0&&result==untouched);}
 auto bad=first;put(bad,0xf0+0x30,0x7fc00000);require(reflex_bike_core_bridge(bad.data(),In,result.data(),Out)!=0&&result==untouched);
 const int rounding=std::fegetround();require(std::fesetround(FE_UPWARD)==0);require(reflex_bike_core_bridge(first.data(),In,result.data(),Out)!=0&&result==untouched);require(std::fegetround()==FE_UPWARD);require(std::fesetround(rounding)==0);
 std::feclearexcept(FE_ALL_EXCEPT);std::feraiseexcept(FE_DIVBYZERO);int flags=std::fetestexcept(FE_ALL_EXCEPT);
 require(reflex_bike_core_bridge(first.data(),In,result.data(),Out)==0&&result==first_answer);require(std::fetestexcept(FE_ALL_EXCEPT)==flags);std::feclearexcept(FE_ALL_EXCEPT);
 std::fprintf(stderr,"raw core bridge: %u captured cases/full state; nine arbitrary overlaps; eighteen graph/layout/header rejects; nonfinite and FP atomic rejects; caller FP restored\n",cases);
}
