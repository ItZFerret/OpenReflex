#include <vector>
#include <array>
#include <fstream>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cfenv>
extern "C" int reflex_slot13_draft(const void*,size_t,void*,size_t);
constexpr size_t InputSize=91152,OutputSize=58360;
static void require(bool b){if(!b){std::fprintf(stderr,"slot13 contract failed\n");std::exit(1);}}
static uint32_t word(const std::vector<uint8_t>&v,size_t a){uint32_t w;std::memcpy(&w,v.data()+a,4);return w;}
static void word(std::vector<uint8_t>&v,size_t a,uint32_t w){std::memcpy(v.data()+a,&w,4);}
int main(int argc,char**argv){
    require(argc==2);std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t>input(InputSize),expected(OutputSize);file.read(reinterpret_cast<char*>(input.data()),InputSize);require(file.gcount()==InputSize);
    require(reflex_slot13_draft(input.data(),input.size(),expected.data(),expected.size())==0);
    require(word(expected,0x1318)==12&&word(expected,0x8890)==1&&word(expected,0x89e0)==1);
    const uint32_t telemetry[]={14,14,0x810,1,1,32};require(std::memcmp(expected.data()+OutputSize-24,telemetry,24)==0);
    unsigned overlaps=0;
    for(int delta:{-int(OutputSize),-64,-1,0,1,4,16,72,200,int(InputSize-4)}){
        std::vector<uint8_t>storage(InputSize+OutputSize*2+64,0xcc);auto*source=storage.data()+OutputSize;std::memcpy(source,input.data(),InputSize);auto*destination=source+delta;const auto before=storage;
        require(reflex_slot13_draft(source,InputSize,destination,OutputSize)==0);require(std::memcmp(destination,expected.data(),OutputSize)==0);
        for(size_t i=0;i<storage.size();i++)if(storage.data()+i<destination||storage.data()+i>=destination+OutputSize)require(storage[i]==before[i]);overlaps++;
    }
    std::vector<uint8_t>out(OutputSize,0x5a);const auto untouched=out;
    require(reflex_slot13_draft(nullptr,InputSize,out.data(),OutputSize)==1&&out==untouched);
    require(reflex_slot13_draft(input.data(),InputSize-1,out.data(),OutputSize)==1&&out==untouched);
    require(reflex_slot13_draft(input.data(),InputSize,out.data(),OutputSize-1)==1&&out==untouched);
    for(auto mutation:std::array<std::array<uint32_t,2>,10>{{{8,65},{16,1},{24,2},{44,0},{48+0x1310,80},{48+0x1318,0},{48+0xd78,0},{48+0x8e40,0},{48+0x8790,4},{48+0x9700+0x100+4,0}}}){
        auto bad=input;word(bad,mutation[0],mutation[1]);require(reflex_slot13_draft(bad.data(),InputSize,out.data(),OutputSize)==2&&out==untouched);
    }
    auto bad=input;for(size_t i=48+0x9700+0x100+72+152;i<48+0x9700+0x100+72+152+16384;i+=4)word(bad,i,0x7fc00000);
    require(reflex_slot13_draft(bad.data(),InputSize,out.data(),OutputSize)==2&&out==untouched);
    const int rounding=std::fegetround();require(std::fesetround(FE_DOWNWARD)==0);require(reflex_slot13_draft(input.data(),InputSize,out.data(),OutputSize)==2&&out==untouched);require(std::fegetround()==FE_DOWNWARD);require(std::fesetround(rounding)==0);
    std::feclearexcept(FE_ALL_EXCEPT);std::feraiseexcept(FE_DIVBYZERO);int flags=std::fetestexcept(FE_ALL_EXCEPT);
    require(reflex_slot13_draft(input.data(),InputSize,out.data(),OutputSize)==0&&out==expected);require(std::fetestexcept(FE_ALL_EXCEPT)==flags);std::feclearexcept(FE_ALL_EXCEPT);
    std::fprintf(stderr,"slot13 contracts: twelve probes/two wheel contacts/14 resource calls; %u arbitrary overlaps; malformed/nonfinite/FP atomic rejects; caller exception state restored\n",overlaps);return 0;
}
