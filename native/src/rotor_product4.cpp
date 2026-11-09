#include "reflex_core.h"
#include "fp32.h"
#include <cstddef>
#include <cstring>
static_assert(sizeof(reflex_rotor_product4_input)==160 && sizeof(reflex_rotor_product4_output)==224,"rotor product sizes");
static_assert(alignof(reflex_rotor_product4_input)==4 && alignof(reflex_rotor_product4_output)==4,"rotor product alignment");
static_assert(offsetof(reflex_rotor_product4_input,c_rows)==64 && offsetof(reflex_rotor_product4_output,c_rows)==128,"rotor product offsets");
uint32_t REFLEX_CALL reflex_rotor_product4(const reflex_rotor_product4_input* input,uint32_t input_size,
                                          reflex_rotor_product4_output* output,uint32_t output_size) {
    using namespace reflex_detail;
    if(!input||!output)return REFLEX_NULL_ARGUMENT;
    if(input_size!=sizeof(*input)||output_size!=sizeof(*output))return REFLEX_ABI_MISMATCH;
    if(reinterpret_cast<uintptr_t>(input)%4||reinterpret_cast<uintptr_t>(output)%4)return REFLEX_INVALID_INPUT;
    if(!supported_fp())return REFLEX_UNSUPPORTED_FP;
    reflex_rotor_product4_input in;std::memcpy(&in,input,sizeof(in));
    for(const auto& row:in.a_rows)for(float word:row)if(!finite(word))return REFLEX_INVALID_INPUT;
    for(const auto& row:in.c_rows)for(float word:row)if(!finite(word))return REFLEX_INVALID_INPUT;
    reflex_rotor_product4_output complete;
    for(unsigned i=0;i<6;++i)for(unsigned j=0;j<4;++j){
        float p[4];
        for(unsigned k=0;k<4;++k){p[k]=mul(in.a_rows[j][k],in.c_rows[i][k]);if(!finite(p[k]))return REFLEX_NUMERIC_RANGE;}
        // Selected original SHUFPS40/30 lane: (pY+pW)+(pX+pZ).
        const float xz=add(p[0],p[2]),yw=add(p[1],p[3]);
        if(!finite(xz)||!finite(yw))return REFLEX_NUMERIC_RANGE;
        complete.c_rows[i][j]=add(yw,xz);if(!finite(complete.c_rows[i][j]))return REFLEX_NUMERIC_RANGE;
    }
    for(unsigned block=0;block<2;++block)for(unsigned row=0;row<4;++row){
        for(unsigned lane=0;lane<3;++lane)complete.b_blocks[block][row][lane]=complete.c_rows[block*3+lane][row];
        complete.b_blocks[block][row][3]=complete.c_rows[block*3][row];
    }
    std::memcpy(output,&complete,sizeof(complete));return REFLEX_OK;
}
