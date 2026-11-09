#include "bike_slot15_callable.h"
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
constexpr size_t Size=45900;
static void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
static std::array<uint64_t,2> fp_state(){
#if defined(__x86_64__)
    uint16_t cw;__asm__ volatile("fnstcw %0":"=m"(cw));return {_mm_getcsr(),cw};
#elif defined(__aarch64__)
    uint64_t control,status;__asm__ volatile("mrs %0, fpcr":"=r"(control));__asm__ volatile("mrs %0, fpsr":"=r"(status));return {control,status};
#else
#error Unsupported test architecture
#endif
}
int main(int argc,char** argv){
 try {
    require(argc==3,"expected input and reference files");
    std::ifstream inputs(argv[1],std::ios::binary),expected(argv[2],std::ios::binary);
    std::array<uint8_t,Size> input{},answer{},output{};unsigned count=0;
    while(inputs.read(reinterpret_cast<char*>(input.data()),Size)){
        require(bool(expected.read(reinterpret_cast<char*>(answer.data()),Size)),"missing expected row");
        std::feclearexcept(FE_ALL_EXCEPT);std::feraiseexcept(FE_INVALID|FE_DIVBYZERO);
        const auto state=fp_state();const int exceptions=std::fetestexcept(FE_ALL_EXCEPT);
        require(reflex_draft_bike_slot15(input.data(),Size,output.data(),Size)==0,"call rejected");
        require(output==answer,"call output mismatch");require(fp_state()==state,"FP controls/status changed");
        require(std::fetestexcept(FE_ALL_EXCEPT)==exceptions,"fenv status changed");
        if(count++==0){
            auto inplace=input;require(reflex_draft_bike_slot15(inplace.data(),Size,inplace.data(),Size)==0&&inplace==answer,"in-place failed");
            std::vector<uint8_t> partial(Size+8,0x6d);std::memcpy(partial.data()+1,input.data(),Size);
            require(reflex_draft_bike_slot15(partial.data()+1,Size,partial.data()+3,Size)==0,"unaligned overlap rejected");
            require(std::memcmp(partial.data()+3,answer.data(),Size)==0&&partial[0]==0x6d&&partial.back()==0x6d,"overlap result/canary");
            output.fill(0xa5);const auto sentinel=output;
            require(reflex_draft_bike_slot15(nullptr,Size,output.data(),Size)==1&&output==sentinel,"null changed output");
            require(reflex_draft_bike_slot15(input.data(),Size-1,output.data(),Size)==1&&output==sentinel,"size changed output");
            auto bad=input;std::memset(bad.data()+0xec,0,4);
            require(reflex_draft_bike_slot15(bad.data(),Size,output.data(),Size)==3&&output==sentinel,"count rejection not atomic");
            std::memcpy(bad.data(),input.data(),Size);std::memset(bad.data()+Size-4,0xff,4);
            require(reflex_draft_bike_slot15(bad.data(),Size,output.data(),Size)==3&&output==sentinel,"profile rejection not atomic");
            std::fesetround(FE_DOWNWARD);const auto unsupported=fp_state();
            require(reflex_draft_bike_slot15(input.data(),Size,output.data(),Size)==2&&output==sentinel,"unsupported environment accepted");
            require(fp_state()==unsupported&&std::fegetround()==FE_DOWNWARD,"unsupported FP mutated");
            std::fesetround(FE_TONEAREST);
        }
    }
    require(inputs.eof()&&inputs.gcount()==0&&count>0,"truncated input");char extra;require(!expected.get(extra),"extra expected data");
    std::fprintf(stderr,"private slot15 callable: %u exact records; atomic/overlap/unaligned/FP contracts passed\n",count);return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"slot15 callable test: %s\n",e.what());return 2;}
}
