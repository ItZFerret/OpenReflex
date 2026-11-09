#include "reflex_core.h"
#include "fp32.h"
#include <cstddef>
#include <cstring>

static_assert(sizeof(reflex_matrix4_multiply_input)==80 && sizeof(reflex_matrix4_output)==16, "ABI layout changed");
static_assert(sizeof(reflex_matrix6_multiply_input)==224 && sizeof(reflex_matrix4x6_subtract_input)==176
              && sizeof(reflex_matrix6x4_subtract_input)==144, "ABI layout changed");
static_assert(offsetof(reflex_matrix4_multiply_input,source)==64
              && offsetof(reflex_matrix6_multiply_input,source)==192
              && offsetof(reflex_matrix4x6_subtract_input,source)==128
              && offsetof(reflex_matrix4x6_subtract_input,destination)==160
              && offsetof(reflex_matrix6x4_subtract_input,source)==96
              && offsetof(reflex_matrix6x4_subtract_input,destination)==112, "ABI offsets changed");
static_assert(alignof(reflex_matrix4_multiply_input)==4 && alignof(reflex_matrix4_output)==4
              && alignof(reflex_matrix6_multiply_input)==4 && alignof(reflex_matrix4x6_subtract_input)==4
              && alignof(reflex_matrix6x4_subtract_input)==4, "ABI alignment changed");
namespace {
using namespace reflex_detail;
template<typename In,typename Out>
uint32_t preflight(const In* input,uint32_t input_size,Out* output,uint32_t output_size) noexcept {
    if (!input||!output) return REFLEX_NULL_ARGUMENT;
    if(input_size!=sizeof(In)||output_size!=sizeof(Out)) return REFLEX_ABI_MISMATCH;
    if(reinterpret_cast<uintptr_t>(input)%alignof(In)||reinterpret_cast<uintptr_t>(output)%alignof(Out))
        return REFLEX_INVALID_INPUT;
    return supported_fp()?REFLEX_OK:REFLEX_UNSUPPORTED_FP;
}
bool finite_vector(const float (&values)[4]) noexcept {
    for(float value:values) if(!finite(value)) return false;
    return true;
}
template<size_t N> bool finite_vectors(const float (&values)[N][4]) noexcept {
    for(const auto& vector:values) if(!finite_vector(vector)) return false;
    return true;
}
bool dot(const float (&row)[4],const float (&source)[4],unsigned width,float& result) noexcept {
    float p[4];
    for(unsigned lane=0;lane<width;++lane) {
        p[lane]=mul(row[lane],source[lane]);
        if(!finite(p[lane])) return false;
    }
    if(width==3) {
        const float xy=add(p[0],p[1]); if(!finite(xy)) return false;
        result=add(xy,p[2]);
    } else {
        // SHUFPS40/30 selects lane2: (py+pw)+(px+pz). Other packed
        // lanes contain unused source-X additions; their FP effects are not modeled.
        const float xz=add(p[0],p[2]); if(!finite(xz)) return false;
        const float yw=add(p[1],p[3]); if(!finite(yw)) return false;
        result=add(yw,xz);
    }
    return finite(result);
}
} // namespace

uint32_t REFLEX_CALL reflex_matrix4_multiply(
    const reflex_matrix4_multiply_input* input,uint32_t input_size,
    reflex_matrix4_output* output,uint32_t output_size) {
    const auto status=preflight(input,input_size,output,output_size); if(status!=REFLEX_OK) return status;
    reflex_matrix4_multiply_input in; std::memcpy(&in,input,sizeof(in));
    if(!finite_vectors(in.rows)||!finite_vector(in.source)) return REFLEX_INVALID_INPUT;
    reflex_matrix4_output complete;
    for(unsigned row=0;row<4;++row)
        if(!dot(in.rows[row],in.source,4,complete.destination[row])) return REFLEX_NUMERIC_RANGE;
    std::memcpy(output,&complete,sizeof(complete)); return REFLEX_OK;
}

uint32_t REFLEX_CALL reflex_matrix6_multiply(
    const reflex_matrix6_multiply_input* input,uint32_t input_size,
    reflex_matrix_output* output,uint32_t output_size) {
    const auto status=preflight(input,input_size,output,output_size); if(status!=REFLEX_OK) return status;
    reflex_matrix6_multiply_input in; std::memcpy(&in,input,sizeof(in));
    if(!finite_vectors(in.rows)||!finite_vectors(in.source)) return REFLEX_INVALID_INPUT;
    float products[12];
    for(unsigned block=0;block<2;++block) {
        for(unsigned row=0;row<3;++row) {
            const unsigned left=block*6+row,right=left+3;
            if(!dot(in.rows[right],in.source[1],3,products[right])
               ||!dot(in.rows[left],in.source[0],3,products[left])) return REFLEX_NUMERIC_RANGE;
        }
    }
    reflex_matrix_output complete;
    for(unsigned block=0;block<2;++block) {
        for(unsigned row=0;row<3;++row) {
            const unsigned left=block*6+row;
            complete.destination[block][row]=reflex_detail::add(products[left],products[left+3]);
            if(!reflex_detail::finite(complete.destination[block][row])) return REFLEX_NUMERIC_RANGE;
        }
        complete.destination[block][3]=complete.destination[block][2];
    }
    std::memcpy(output,&complete,sizeof(complete)); return REFLEX_OK;
}

uint32_t REFLEX_CALL reflex_matrix4x6_subtract(
    const reflex_matrix4x6_subtract_input* input,uint32_t input_size,
    reflex_matrix4_output* output,uint32_t output_size) {
    const auto status=preflight(input,input_size,output,output_size); if(status!=REFLEX_OK) return status;
    reflex_matrix4x6_subtract_input in; std::memcpy(&in,input,sizeof(in));
    if(!finite_vectors(in.rows)||!finite_vectors(in.source)||!finite_vector(in.destination)) return REFLEX_INVALID_INPUT;
    float products[8];
    for(unsigned row=0;row<4;++row)
        if(!dot(in.rows[row+4],in.source[1],3,products[row+4])
           ||!dot(in.rows[row],in.source[0],3,products[row])) return REFLEX_NUMERIC_RANGE;
    reflex_matrix4_output complete;
    for(unsigned row=0;row<4;++row) {
        const float correction=reflex_detail::add(products[row],products[row+4]);
        if(!reflex_detail::finite(correction)) return REFLEX_NUMERIC_RANGE;
        complete.destination[row]=reflex_detail::sub(in.destination[row],correction);
        if(!reflex_detail::finite(complete.destination[row])) return REFLEX_NUMERIC_RANGE;
    }
    std::memcpy(output,&complete,sizeof(complete)); return REFLEX_OK;
}

uint32_t REFLEX_CALL reflex_matrix6x4_subtract(
    const reflex_matrix6x4_subtract_input* input,uint32_t input_size,
    reflex_matrix_output* output,uint32_t output_size) {
    const auto status=preflight(input,input_size,output,output_size); if(status!=REFLEX_OK) return status;
    reflex_matrix6x4_subtract_input in; std::memcpy(&in,input,sizeof(in));
    if(!finite_vectors(in.rows)||!finite_vector(in.source)||!finite_vectors(in.destination)) return REFLEX_INVALID_INPUT;
    float products[6];
    for(unsigned row=0;row<6;++row) if(!dot(in.rows[row],in.source,4,products[row])) return REFLEX_NUMERIC_RANGE;
    reflex_matrix_output complete;
    for(unsigned block=0;block<2;++block) {
        for(unsigned lane=0;lane<4;++lane) {
            // Native stack packing leaves W equal to each triple's FIRST dot.
            const float correction=products[block*3+(lane<3?lane:0)];
            complete.destination[block][lane]=reflex_detail::sub(in.destination[block][lane],correction);
            if(!reflex_detail::finite(complete.destination[block][lane])) return REFLEX_NUMERIC_RANGE;
        }
    }
    std::memcpy(output,&complete,sizeof(complete)); return REFLEX_OK;
}
