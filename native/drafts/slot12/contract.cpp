#include <array>
#include <vector>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cfenv>
extern "C" int reflex_bike_slot12_draft(const void*,size_t,void*,size_t);
constexpr size_t Input=39104,Output=39096;
static void check(bool good){if(!good){std::fprintf(stderr,"slot12 contract failed\n");std::exit(1);}}
static void word(std::vector<uint8_t>& a,size_t at,uint32_t v){std::memcpy(a.data()+at,&v,4);}
int main(int argc,char**argv){
 check(argc==3);std::ifstream f(argv[1],std::ios::binary),g(argv[2],std::ios::binary);std::vector<uint8_t>a(Input),e(Output),air(Input);f.read(reinterpret_cast<char*>(a.data()),Input);g.read(reinterpret_cast<char*>(e.data()),Output);f.seekg(4*Input);f.read(reinterpret_cast<char*>(air.data()),Input);check(f&&g);
 std::vector<uint8_t> out(Output,0x6b);check(reflex_bike_slot12_draft(a.data(),Input,out.data(),Output)==0&&out==e);
 unsigned overlaps=0;for(int off:{-int(Output),-4,0,1,4,16,512,19000,int(Input-4)}){
  std::vector<uint8_t> image(Input+Output*2+1024,0x65);auto*source=image.data()+Output+512;auto*target=source+off;std::memcpy(source,a.data(),Input);auto before=image;
  check(reflex_bike_slot12_draft(source,Input,target,Output)==0&&std::memcmp(target,e.data(),Output)==0);
  for(size_t i=0;i<image.size();i++)if(image.data()+i<target||image.data()+i>=target+Output)check(image[i]==before[i]);overlaps++;
 }
 out.assign(Output,0x6b);const auto before=out;
 check(reflex_bike_slot12_draft(nullptr,Input,out.data(),Output)==1&&out==before);check(reflex_bike_slot12_draft(a.data(),Input-1,out.data(),Output)==1&&out==before);check(reflex_bike_slot12_draft(a.data(),Input,out.data(),Output-1)==1&&out==before);
 for(auto change:std::array<std::array<uint32_t,2>,4>{{{Input-4,4},{Input-8,0x7fc00000},{0x5c30,0},{0x8b40,0x30000001}}}){auto bad=a;word(bad,change[0],change[1]);check(reflex_bike_slot12_draft(bad.data(),Input,out.data(),Output)==2&&out==before);}
 word(air,0x59ec,0);check(reflex_bike_slot12_draft(air.data(),Input,out.data(),Output)==2&&out==before);
 int rounding=std::fegetround();check(std::fesetround(FE_DOWNWARD)==0);check(reflex_bike_slot12_draft(a.data(),Input,out.data(),Output)==2&&out==before);check(std::fesetround(rounding)==0);
 std::fprintf(stderr,"slot12 contracts: %u arbitrary overlaps, full original result, early+late atomic errors and FP guard passed\n",overlaps);
}
