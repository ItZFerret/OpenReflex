#include "reflex_core.h"
#include "fp32.h"
#include <cstddef>
#include <cstring>
static_assert(sizeof(reflex_matrix3_product_input)==100 && sizeof(reflex_inverse6_input)==240
              && sizeof(reflex_inverse6_output)==192,"factorization sizes");
static_assert(alignof(reflex_matrix3_product_input)==4 && alignof(reflex_inverse6_input)==4
              && alignof(reflex_inverse6_output)==4,"factorization alignment");
static_assert(offsetof(reflex_matrix3_product_input,q)==48 && offsetof(reflex_matrix3_product_input,p_transpose_rva)==96
              && offsetof(reflex_inverse6_input,numerator)==192 && offsetof(reflex_inverse6_input,mask)==208
              && offsetof(reflex_inverse6_input,source_b_transpose_rva)==224
              && offsetof(reflex_inverse6_input,destination_d_negate_rva)==236,"factorization offsets");
namespace {
using namespace reflex_detail;
using Rows=float[3][4];
bool product(const Rows& p,const Rows& q,Rows& output) noexcept {
    for(unsigned row=0;row<3;++row){
        for(unsigned column=0;column<3;++column){
            const float x=mul(q[row][0],p[0][column]),y=mul(q[row][1],p[1][column]),z=mul(q[row][2],p[2][column]);
            if(!finite(x)||!finite(y)||!finite(z))return false;
            const float xy=add(x,y);if(!finite(xy))return false;
            output[row][column]=add(xy,z);if(!finite(output[row][column]))return false;
        }
        output[row][3]=output[row][2];
    }
    return true;
}
bool subtract(const Rows& a,const Rows& b,Rows& output) noexcept {
    for(unsigned row=0;row<3;++row)for(unsigned lane=0;lane<4;++lane){
        output[row][lane]=sub(a[row][lane],b[row][lane]);if(!finite(output[row][lane]))return false;
    }
    return true;
}
}
uint32_t REFLEX_CALL reflex_matrix3_product(const reflex_matrix3_product_input* input,uint32_t input_size,
                                           reflex_inverse3_output* output,uint32_t output_size) {
    using namespace reflex_detail;
    if(!input||!output)return REFLEX_NULL_ARGUMENT;
    if(input_size!=sizeof(*input)||output_size!=sizeof(*output))return REFLEX_ABI_MISMATCH;
    if(reinterpret_cast<uintptr_t>(input)%4||reinterpret_cast<uintptr_t>(output)%4)return REFLEX_INVALID_INPUT;
    if(!supported_fp())return REFLEX_UNSUPPORTED_FP;
    reflex_matrix3_product_input in;std::memcpy(&in,input,sizeof(in));
    if(in.p_transpose_rva!=0x457c90u)return REFLEX_INVALID_INPUT;
    for(const auto& row:in.p)for(float word:row)if(!finite(word))return REFLEX_INVALID_INPUT;
    for(const auto& row:in.q)for(float word:row)if(!finite(word))return REFLEX_INVALID_INPUT;
    reflex_inverse3_output complete;
    if(!product(in.p,in.q,complete.rows))return REFLEX_NUMERIC_RANGE;
    std::memcpy(output,&complete,sizeof(complete));return REFLEX_OK;
}
uint32_t REFLEX_CALL reflex_matrix6_inverse(const reflex_inverse6_input* input,uint32_t input_size,
                                           reflex_inverse6_output* output,uint32_t output_size) {
    using namespace reflex_detail;
    if(!input||!output)return REFLEX_NULL_ARGUMENT;
    if(input_size!=sizeof(*input)||output_size!=sizeof(*output))return REFLEX_ABI_MISMATCH;
    if(reinterpret_cast<uintptr_t>(input)%4||reinterpret_cast<uintptr_t>(output)%4)return REFLEX_INVALID_INPUT;
    if(!supported_fp())return REFLEX_UNSUPPORTED_FP;
    reflex_inverse6_input in;std::memcpy(&in,input,sizeof(in));
    if(in.source_b_transpose_rva!=0x457c90u||in.destination_c_transpose_rva!=0x457c90u
       ||in.destination_d_copy_rva!=0x457bf0u||in.destination_d_negate_rva!=0x4581e0u
       ||in.mask[0]!=0xffffffffu||in.mask[1]!=0xffffffffu||in.mask[2]!=0xffffffffu||in.mask[3]!=0)return REFLEX_INVALID_INPUT;
    for(const auto& block:in.blocks)for(const auto& row:block)for(float word:row)if(!finite(word))return REFLEX_INVALID_INPUT;
    for(float word:in.numerator)if(!finite(word))return REFLEX_INVALID_INPUT;
    reflex_inverse3_input inverse{};std::memcpy(inverse.numerator,in.numerator,16);std::memcpy(inverse.mask,in.mask,16);inverse.transpose_rva=0x457c90u;
    reflex_inverse3_output e,h;
    std::memcpy(inverse.rows,in.blocks[0],48);
    auto status=reflex_matrix3_inverse(&inverse,sizeof(inverse),&e,sizeof(e));if(status!=REFLEX_OK)return status;
    Rows t,u,v,k,r;
    if(!product(e.rows,in.blocks[2],t)||!product(in.blocks[1],e.rows,u)||!product(u,in.blocks[2],v)
       ||!subtract(v,in.blocks[3],k))return REFLEX_NUMERIC_RANGE;
    std::memcpy(inverse.rows,k,48);
    status=reflex_matrix3_inverse(&inverse,sizeof(inverse),&h,sizeof(h));if(status!=REFLEX_OK)return status;
    reflex_inverse6_output complete;
    if(!product(h.rows,u,complete.blocks[1])||!product(t,h.rows,complete.blocks[2])
       ||!product(complete.blocks[2],u,r)||!subtract(e.rows,r,complete.blocks[0]))return REFLEX_NUMERIC_RANGE;
    for(unsigned row=0;row<3;++row)for(unsigned lane=0;lane<4;++lane){
        complete.blocks[3][row][lane]=sub(0.0f,h.rows[row][lane]);
        if(!finite(complete.blocks[3][row][lane]))return REFLEX_NUMERIC_RANGE;
    }
    std::memcpy(output,&complete,sizeof(complete));return REFLEX_OK;
}
