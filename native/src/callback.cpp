#include "reflex_core.h"
#include "fp32.h"
#include <cstddef>
#include <cstring>
static_assert(sizeof(reflex_fixed_slot3_input)==544 && sizeof(reflex_rotor_slot3_input)==400,"callback ABI");
static_assert(sizeof(reflex_fixed_slot5_input)==496 && sizeof(reflex_rotor_slot5_input)==352 && sizeof(reflex_body_force_output)==64,"callback ABI");
static_assert(offsetof(reflex_fixed_slot5_input,aliases)==480 && offsetof(reflex_rotor_slot5_input,aliases)==336,"callback aliases ABI");
namespace {
using namespace reflex_detail;
template<typename In,typename Out> uint32_t preflight(const In* in,uint32_t is,Out* out,uint32_t os) noexcept {
    if(!in||!out) return REFLEX_NULL_ARGUMENT;
    if(is!=sizeof(In)||os!=sizeof(Out)) return REFLEX_ABI_MISMATCH;
    if(reinterpret_cast<uintptr_t>(in)%4||reinterpret_cast<uintptr_t>(out)%4) return REFLEX_INVALID_INPUT;
    return supported_fp()?REFLEX_OK:REFLEX_UNSUPPORTED_FP;
}
bool finite_words(const void* input,size_t count) noexcept {
    const auto* bytes=static_cast<const unsigned char*>(input);
    for(size_t i=0;i<count;++i){float v;std::memcpy(&v,bytes+4*i,4);if(!finite(v))return false;}return true;
}
bool plus(float a,float b,float& out) noexcept {out=add(a,b);return finite(out);}
bool dot(const float* row,const float* source,unsigned width,float& out) noexcept {
    float p[4];for(unsigned i=0;i<width;++i){p[i]=mul(row[i],source[i]);if(!finite(p[i]))return false;}
    float a,b;
    if(width==3)return plus(p[0],p[1],a)&&plus(a,p[2],out);
    return plus(p[0],p[2],a)&&plus(p[1],p[3],b)&&plus(b,a,out);
}
bool multiply6(const float rows[12][4],const float source[2][4],float out[2][4]) noexcept {
    for(unsigned block=0;block<2;++block){
        for(unsigned i=0;i<3;++i){float left,right;
            if(!dot(rows[block*6+3+i],source[1],3,right)||!dot(rows[block*6+i],source[0],3,left)||!plus(left,right,out[block][i]))return false;}
        out[block][3]=out[block][2];
    }return true;
}
bool multiply4x6(const float rows[8][4],const float source[2][4],float out[1][4]) noexcept {
    for(unsigned i=0;i<4;++i){float left,right;
        if(!dot(rows[4+i],source[1],3,right)||!dot(rows[i],source[0],3,left)||!plus(left,right,out[0][i]))return false;}
    return true;
}
bool multiply6x4(const float rows[6][4],const float source[1][4],float out[2][4]) noexcept {
    for(unsigned block=0;block<2;++block){
        for(unsigned i=0;i<3;++i)if(!dot(rows[block*3+i],source[0],4,out[block][i]))return false;
        out[block][3]=out[block][0];
    }return true;
}
template<bool Fixed,typename In,typename Out>
uint32_t slot3(const In* input,uint32_t is,Out* output,uint32_t os) noexcept {
    const auto status=preflight(input,is,output,os);if(status!=REFLEX_OK)return status;
    In in;std::memcpy(&in,input,sizeof(in));if(!finite_words(&in,sizeof(in)/4))return REFLEX_INVALID_INPUT;
    float scaled[4][4]{};
    for(unsigned i=0;i<4;++i)for(unsigned lane=0;lane<3;++lane){
        scaled[i][lane]=mul(in.forces[i][lane],in.coefficients[i][lane]);if(!finite(scaled[i][lane]))return REFLEX_NUMERIC_RANGE;
    }
    constexpr unsigned count=Fixed?2:1;float products[2][count][4];
    for(unsigned i=0;i<2;++i){
        if constexpr(Fixed){if(!multiply6(in.matrices[i],scaled+i*2,products[i]))return REFLEX_NUMERIC_RANGE;}
        else {if(!multiply4x6(in.matrices[i],scaled+i*2,products[i]))return REFLEX_NUMERIC_RANGE;}
    }
    float complete[count][4];
    for(unsigned i=0;i<count;++i)for(unsigned lane=0;lane<4;++lane){float sum;
        if(!plus(products[0][i][lane],products[1][i][lane],sum)||!plus(sum,in.bias[i][lane],complete[i][lane]))return REFLEX_NUMERIC_RANGE;
    }
    static_assert(sizeof(complete)==sizeof(Out),"callback result size");std::memcpy(output,complete,sizeof(complete));return REFLEX_OK;
}
template<bool Fixed,typename In>
uint32_t slot5(const In* input,uint32_t is,reflex_body_force_output* output,uint32_t os) noexcept {
    const auto status=preflight(input,is,output,os);if(status!=REFLEX_OK)return status;
    In in;std::memcpy(&in,input,sizeof(in));if(!finite_words(&in,offsetof(In,aliases)/4))return REFLEX_INVALID_INPUT;
    for(unsigned i=0;i<4;++i){const auto a=in.aliases[i];
        if(a>i||in.aliases[a]!=a||std::memcmp(in.forces[i],in.forces[a],16)!=0)return REFLEX_INVALID_INPUT;
    }
    float products[4][4];
    for(unsigned m=0;m<2;++m){
        if constexpr(Fixed){
            float transposed[12][4]{};const unsigned blocks[]={0,6,3,9};
            for(unsigned b=0;b<4;++b)for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)
                transposed[b*3+r][c]=in.matrices[m][blocks[b]+c][r];
            if(!multiply6(transposed,in.solution,products+m*2))return REFLEX_NUMERIC_RANGE;
        }else{
            float transposed[6][4];
            for(unsigned b=0;b<2;++b)for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)
                transposed[b*3+r][c]=in.matrices[m][b*4+c][r];
            if(!multiply6x4(transposed,in.solution,products+m*2))return REFLEX_NUMERIC_RANGE;
        }
    }
    for(unsigned i=0;i<4;++i)for(unsigned lane=0;lane<4;++lane){auto& current=in.forces[in.aliases[i]][lane];
        if(!plus(!Fixed&&i%2?products[i][lane]:current,!Fixed&&i%2?current:products[i][lane],current))return REFLEX_NUMERIC_RANGE;
    }
    reflex_body_force_output complete;for(unsigned i=0;i<4;++i)std::memcpy(complete.forces[i],in.forces[in.aliases[i]],16);
    std::memcpy(output,&complete,sizeof(complete));return REFLEX_OK;
}
}
uint32_t REFLEX_CALL reflex_fixed_slot3(const reflex_fixed_slot3_input* i,uint32_t is,reflex_matrix_output* o,uint32_t os){return slot3<true>(i,is,o,os);}
uint32_t REFLEX_CALL reflex_rotor_slot3(const reflex_rotor_slot3_input* i,uint32_t is,reflex_matrix4_output* o,uint32_t os){return slot3<false>(i,is,o,os);}
uint32_t REFLEX_CALL reflex_fixed_slot5(const reflex_fixed_slot5_input* i,uint32_t is,reflex_body_force_output* o,uint32_t os){return slot5<true>(i,is,o,os);}
uint32_t REFLEX_CALL reflex_rotor_slot5(const reflex_rotor_slot5_input* i,uint32_t is,reflex_body_force_output* o,uint32_t os){return slot5<false>(i,is,o,os);}
