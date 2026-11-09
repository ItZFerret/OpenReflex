#include "reflex_core.h"
#include "fp32.h"
#include <cstddef>
#include <cstring>
#include <initializer_list>
static_assert(sizeof(reflex_inverse4_input)==160 && sizeof(reflex_inverse4_output)==64,"inverse4 size");
static_assert(alignof(reflex_inverse4_input)==4 && alignof(reflex_inverse4_output)==4,"inverse4 alignment");
static_assert(offsetof(reflex_inverse4_input,zero)==64 && offsetof(reflex_inverse4_input,axis_x)==80
              && offsetof(reflex_inverse4_input,axis_y)==96 && offsetof(reflex_inverse4_input,axis_z)==112
              && offsetof(reflex_inverse4_input,numerator)==128 && offsetof(reflex_inverse4_input,mask)==144,"inverse4 offsets");
uint32_t REFLEX_CALL reflex_matrix4_inverse(const reflex_inverse4_input* input,uint32_t input_size,
                                           reflex_inverse4_output* output,uint32_t output_size) {
    using namespace reflex_detail;
    if(!input||!output)return REFLEX_NULL_ARGUMENT;
    if(input_size!=sizeof(*input)||output_size!=sizeof(*output))return REFLEX_ABI_MISMATCH;
    if(reinterpret_cast<uintptr_t>(input)%4||reinterpret_cast<uintptr_t>(output)%4)return REFLEX_INVALID_INPUT;
    if(!supported_fp())return REFLEX_UNSUPPORTED_FP;
    reflex_inverse4_input in;std::memcpy(&in,input,sizeof(in));
    for(const auto& row:in.rows)for(float word:row)if(!finite(word))return REFLEX_INVALID_INPUT;
    for(const float* row:{in.zero,in.axis_x,in.axis_y,in.axis_z,in.numerator})
        for(unsigned i=0;i<4;++i)if(!finite(row[i]))return REFLEX_INVALID_INPUT;
    reflex_inverse6_input embedded{};
    std::memcpy(embedded.blocks[0],in.rows,48);
    for(unsigned i=0;i<3;++i){
        std::memcpy(embedded.blocks[1][i],in.zero,16);
        std::memcpy(&embedded.blocks[1][i][0],&in.rows[i][3],4);
    }
    std::memcpy(embedded.blocks[2][0],in.rows[3],16);
    std::memcpy(embedded.blocks[2][1],in.zero,16);std::memcpy(embedded.blocks[2][2],in.zero,16);
    std::memcpy(embedded.blocks[3][0],in.axis_x,16);
    std::memcpy(&embedded.blocks[3][0][0],&in.rows[3][3],4);
    std::memcpy(embedded.blocks[3][1],in.axis_y,16);std::memcpy(embedded.blocks[3][2],in.axis_z,16);
    std::memcpy(embedded.numerator,in.numerator,16);std::memcpy(embedded.mask,in.mask,16);
    embedded.source_b_transpose_rva=embedded.destination_c_transpose_rva=0x457c90u;
    embedded.destination_d_copy_rva=0x457bf0u;embedded.destination_d_negate_rva=0x4581e0u;
    reflex_inverse6_output result;
    const auto status=reflex_matrix6_inverse(&embedded,sizeof(embedded),&result,sizeof(result));if(status!=REFLEX_OK)return status;
    reflex_inverse4_output complete;
    for(unsigned i=0;i<3;++i){
        std::memcpy(complete.rows[i],result.blocks[0][i],12);
        std::memcpy(&complete.rows[i][3],&result.blocks[1][i][0],4);
    }
    std::memcpy(complete.rows[3],result.blocks[2][0],12);
    std::memcpy(&complete.rows[3][3],&result.blocks[3][0][0],4);
    std::memcpy(output,&complete,sizeof(complete));return REFLEX_OK;
}
