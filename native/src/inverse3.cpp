#include "reflex_core.h"
#include "fp32.h"
#include <cstddef>
#include <cstring>
static_assert(sizeof(reflex_inverse3_input)==84 && sizeof(reflex_inverse3_output)==48,"inverse3 ABI");
static_assert(alignof(reflex_inverse3_input)==4 && alignof(reflex_inverse3_output)==4,"inverse3 alignment");
static_assert(offsetof(reflex_inverse3_input,numerator)==48 && offsetof(reflex_inverse3_input,mask)==64
              && offsetof(reflex_inverse3_input,transpose_rva)==80,"inverse3 offsets");
namespace {
using namespace reflex_detail;
bool cofactor(const float first[4],const float second[4],float out[4]) noexcept {
    constexpr unsigned left[3]={2,0,1},right[3]={1,2,0};
    for(unsigned i=0;i<3;++i){
        const float a=mul(first[left[i]],second[right[i]]),b=mul(first[right[i]],second[left[i]]);
        if(!finite(a)||!finite(b))return false;
        out[i]=sub(a,b);if(!finite(out[i]))return false;
    }
    // ANDPS clears all bits of the packed W cofactor, including a possible
    // NaN from discarded W overflow. Its exception effects are not reproduced.
    out[3]=0.0f;return true;
}
}
uint32_t REFLEX_CALL reflex_matrix3_inverse(const reflex_inverse3_input* input,uint32_t input_size,
                                           reflex_inverse3_output* output,uint32_t output_size) {
    using namespace reflex_detail;
    if(!input||!output)return REFLEX_NULL_ARGUMENT;
    if(input_size!=sizeof(*input)||output_size!=sizeof(*output))return REFLEX_ABI_MISMATCH;
    if(reinterpret_cast<uintptr_t>(input)%4||reinterpret_cast<uintptr_t>(output)%4)return REFLEX_INVALID_INPUT;
    if(!supported_fp())return REFLEX_UNSUPPORTED_FP;
    reflex_inverse3_input in;std::memcpy(&in,input,sizeof(in));
    if(in.transpose_rva!=0x00457c90u || in.mask[0]!=0xffffffffu || in.mask[1]!=0xffffffffu
       || in.mask[2]!=0xffffffffu || in.mask[3]!=0)return REFLEX_INVALID_INPUT;
    for(const auto& row:in.rows)for(float v:row)if(!finite(v))return REFLEX_INVALID_INPUT;
    for(float v:in.numerator)if(!finite(v))return REFLEX_INVALID_INPUT;
    float cof[3][4],scaled[3][4],products[3],scale[4];
    if(!cofactor(in.rows[2],in.rows[1],cof[0]))return REFLEX_NUMERIC_RANGE;
    for(unsigned i=0;i<3;++i){products[i]=mul(in.rows[0][i],cof[0][i]);if(!finite(products[i]))return REFLEX_NUMERIC_RANGE;}
    const float xy=add(products[0],products[1]);if(!finite(xy))return REFLEX_NUMERIC_RANGE;
    const float determinant=add(xy,products[2]);if(!finite(determinant)||determinant==0.0f)return REFLEX_NUMERIC_RANGE;
    for(unsigned i=0;i<4;++i){scale[i]=div(in.numerator[i],determinant);if(!finite(scale[i]))return REFLEX_NUMERIC_RANGE;}
    for(unsigned i=0;i<4;++i){scaled[0][i]=mul(scale[i],cof[0][i]);if(!finite(scaled[0][i]))return REFLEX_NUMERIC_RANGE;}
    for(unsigned row=1;row<3;++row){
        if(!cofactor(in.rows[row-1],in.rows[row==1?2:0],cof[row]))return REFLEX_NUMERIC_RANGE;
        for(unsigned i=0;i<4;++i){scaled[row][i]=mul(cof[row][i],scale[i]);if(!finite(scaled[row][i]))return REFLEX_NUMERIC_RANGE;}
    }
    reflex_inverse3_output complete;
    for(unsigned row=0;row<3;++row){
        for(unsigned column=0;column<3;++column)complete.rows[row][column]=scaled[column][row];
        complete.rows[row][3]=row<2?scaled[2][row]:scaled[2][3];
    }
    std::memcpy(output,&complete,sizeof(complete));return REFLEX_OK;
}
