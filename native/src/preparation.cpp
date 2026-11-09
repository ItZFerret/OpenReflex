// Complete 0085cb70 preparation from two source matrices/coefs/runtime globals.
#include "reflex_core.h"
#include "fp32.h"
#include <cstddef>
#include <cstring>

static_assert(sizeof(reflex_preparation_input)==116&&sizeof(reflex_preparation_node_input)==448&&sizeof(reflex_preparation_node_output)==2304,"preparation sizes");
static_assert(offsetof(reflex_preparation_node_input,coefficients)==32&&offsetof(reflex_preparation_node_input,matrix38)==64&&offsetof(reflex_preparation_node_input,matrix3c)==256,"preparation offsets");
namespace preparation_detail {
using Rows=float[12][4];
using Row=float[4];
struct Arithmetic {
    bool valid=true;
    float check(float value){valid=valid&&reflex_detail::finite(value);return value;}
    float add(float a,float b){return check(reflex_detail::add(a,b));}
    float sub(float a,float b){return check(reflex_detail::sub(a,b));}
    float mul(float a,float b){return check(reflex_detail::mul(a,b));}
    float dot(const Row& p,const Row& q,unsigned width){
        float terms[4]{};for(unsigned k=0;k<width;++k)terms[k]=mul(p[k],q[k]);
        if(width==3)return add(add(terms[0],terms[1]),terms[2]);
        const float xz=add(terms[0],terms[2]),yw=add(terms[1],terms[3]);return add(yw,xz);
    }
    void rows_product(const float (*p)[4],unsigned pn,const float (*q)[4],unsigned qn,unsigned width,int padding,float (*out)[4]){
        for(unsigned i=0;i<pn;++i){for(unsigned j=0;j<qn;++j)out[i][j]=dot(p[i],q[j],width);if(padding>=0)out[i][3]=out[i][padding];}
    }
    void add_rows(const float (*a)[4],const float (*b)[4],unsigned count,float (*out)[4]){
        for(unsigned i=0;i<count;++i)for(unsigned j=0;j<4;++j)out[i][j]=add(a[i][j],b[i][j]);
    }
    void sub_rows(const float (*a)[4],const float (*b)[4],unsigned count,float (*out)[4]){
        for(unsigned i=0;i<count;++i)for(unsigned j=0;j<4;++j)out[i][j]=sub(a[i][j],b[i][j]);
    }
    void product6(const Rows& p,const Rows& q,Rows& out){
        for(unsigned i=0;i<2;++i)for(unsigned j=0;j<2;++j){
            float left[3][4],right[3][4];rows_product(p+i*6,3,q+j*6,3,3,2,left);
            rows_product(p+i*6+3,3,q+j*6+3,3,3,2,right);add_rows(left,right,3,out+i*6+j*3);
        }
    }
    void paired6x4(const Rows& a,const Rows& old_c,Rows& b,Rows& c){
        float t[4][6];for(unsigned i=0;i<4;++i)for(unsigned j=0;j<6;++j){
            const unsigned left=j<3?j:j+3;t[i][j]=add(dot(a[left],old_c[i],3),dot(a[left+3],old_c[i+4],3));
        }
        for(unsigned j=0;j<6;++j)for(unsigned i=0;i<4;++i)b[j][i]=t[i][j];
        for(unsigned block=0;block<2;++block)for(unsigned i=0;i<4;++i){for(unsigned j=0;j<3;++j)c[block*4+i][j]=t[i][block*3+j];c[block*4+i][3]=t[i][block*3+2];}
    }
    void rotor_slot8(const Rows& q,const Rows& s,Rows& out){
        float t[6][4];rows_product(q,6,s,4,4,-1,t);
        for(unsigned i=0;i<2;++i)for(unsigned j=0;j<2;++j)rows_product(t+i*3,3,q+j*3,3,4,0,out+i*6+j*3);
    }
    void rotor_slot9(const Rows& q,const Rows& s,Rows& out){
        float a[4][4],b[4][4],left[4][4],right[4][4],x[4][4],y[4][4];
        rows_product(q,4,s,3,3,2,a);rows_product(q+4,4,s+3,3,3,2,b);add_rows(a,b,4,left);
        rows_product(q+4,4,s+9,3,3,2,a);rows_product(q,4,s+6,3,3,2,b);add_rows(a,b,4,right);
        rows_product(left,4,q,4,3,-1,x);rows_product(right,4,q+4,4,3,-1,y);add_rows(x,y,4,out);
    }
};
void transpose3(const float (*in)[4],float (*out)[4]){
    for(unsigned j=0;j<3;++j){for(unsigned i=0;i<3;++i)out[j][i]=in[i][j];out[j][3]=j<2?in[2][j]:in[2][3];}
}
void transpose(const Rows& in,unsigned source_rows,Rows& out){
    if(source_rows==12){const unsigned blocks[]={0,6,3,9};for(unsigned i=0;i<4;++i)transpose3(in+blocks[i],out+i*3);}
    else if(source_rows==8){for(unsigned block=0;block<2;++block)for(unsigned j=0;j<3;++j)for(unsigned i=0;i<4;++i)out[block*3+j][i]=in[block*4+i][j];}
    else {for(unsigned block=0;block<2;++block)for(unsigned j=0;j<4;++j){for(unsigned i=0;i<3;++i)out[block*4+j][i]=in[block*3+i][j];out[block*4+j][3]=in[block*3+2][j==2?3:j];}}
}
uint32_t invert(const Rows& in,bool four,const reflex_preparation_input& config,Rows& out){
    if(four){reflex_inverse4_input input{};reflex_inverse4_output output;
        std::memcpy(input.rows,in,64);std::memcpy(input.zero,config.zero3,16);std::memcpy(input.axis_x,config.axis_x,16);std::memcpy(input.axis_y,config.axis_y,16);std::memcpy(input.axis_z,config.axis_z,16);
        std::memcpy(input.numerator,config.numerator,16);std::memcpy(input.mask,config.mask,16);
        const auto status=reflex_matrix4_inverse(&input,sizeof(input),&output,sizeof(output));if(status==REFLEX_OK)std::memcpy(out,output.rows,64);return status;
    }
    reflex_inverse6_input input{};reflex_inverse6_output output;std::memcpy(input.blocks,in,192);std::memcpy(input.numerator,config.numerator,16);std::memcpy(input.mask,config.mask,16);
    input.source_b_transpose_rva=input.destination_c_transpose_rva=0x457c90u;input.destination_d_copy_rva=0x457bf0u;input.destination_d_negate_rva=0x4581e0u;
    const auto status=reflex_matrix6_inverse(&input,sizeof(input),&output,sizeof(output));if(status==REFLEX_OK)std::memcpy(out,output.blocks,192);return status;
}
void initialize(const reflex_preparation_node_input& node,const reflex_preparation_input& config,Rows& out,Arithmetic& ar){
    const float* axes[]={config.axis_x,config.axis_y,config.axis_z};
    for(unsigned body=0;body<2;++body)for(unsigned row=0;row<3;++row)for(unsigned lane=0;lane<4;++lane)out[body*9+row][lane]=ar.mul(node.coefficients[body][lane],axes[row][lane]);
    for(unsigned row=3;row<9;++row)std::memcpy(out[row],config.zero3,16);
}
void node_product(const reflex_preparation_node_output& node,unsigned kind,unsigned slot,Rows& out,Arithmetic& ar){
    const auto& q=node.matrices[slot==8?9:5];const auto& s=node.matrices[slot==8?6:2];
    if(kind==1){Rows temp{};ar.product6(q,s,temp);ar.product6(temp,q,out);}
    else if(slot==8)ar.rotor_slot8(q,s,out);else ar.rotor_slot9(q,s,out);
}
uint32_t paired(const Rows& a,Rows& b,Rows& c,unsigned kind,bool second,Arithmetic& ar){
    Rows saved;std::memcpy(saved,c,sizeof(saved));
    if(kind==1){ar.product6(a,saved,b);transpose(b,12,c);}
    else if(!second)ar.paired6x4(a,saved,b,c);
    else {reflex_rotor_product4_input input;reflex_rotor_product4_output output;std::memcpy(input.a_rows,a,64);std::memcpy(input.c_rows,saved,96);
        const auto status=reflex_rotor_product4(&input,sizeof(input),&output,sizeof(output));if(status!=REFLEX_OK)return status;
        std::memcpy(b,output.b_blocks,128);std::memcpy(c,output.c_rows,96);}
    return ar.valid?REFLEX_OK:REFLEX_NUMERIC_RANGE;
}
bool graph(const reflex_preparation_node_input* nodes,unsigned count){
    unsigned incoming[10]{};
    for(unsigned i=0;i<count;++i){const auto& n=nodes[i];
        if((i==0&&(n.kind!=0||n.parent!=-1))||(i&&(n.kind!=1&&n.kind!=2)))return false;
        if(i&&(n.parent<0||static_cast<unsigned>(n.parent)>=i))return false;
        if(n.child_count>5)return false;
        for(unsigned j=0;j<n.child_count;++j){auto child=n.children[j];if(!child||child>=count||nodes[child].parent!=static_cast<int32_t>(i)||++incoming[child]!=1)return false;}
    }
    for(unsigned i=1;i<count;++i)if(incoming[i]!=1)return false;return true;
}
} // namespace preparation_detail

uint32_t REFLEX_CALL reflex_prepare_matrices(const reflex_preparation_input* input,uint32_t input_size,const reflex_preparation_node_input* nodes,uint32_t nodes_size,reflex_preparation_node_output* output,uint32_t output_size){
    using namespace preparation_detail;
    if(!input||!nodes||!output)return REFLEX_NULL_ARGUMENT;
    if(input_size!=sizeof(*input))return REFLEX_ABI_MISMATCH;
    if(reinterpret_cast<uintptr_t>(input)%4||reinterpret_cast<uintptr_t>(nodes)%4||reinterpret_cast<uintptr_t>(output)%4)return REFLEX_INVALID_INPUT;
    reflex_preparation_input config;std::memcpy(&config,input,sizeof(config));const unsigned count=config.node_count;
    if(count<1||count>10)return REFLEX_INVALID_INPUT;
    if(nodes_size!=count*sizeof(*nodes)||output_size!=count*sizeof(*output))return REFLEX_ABI_MISMATCH;
    if(!reflex_detail::supported_fp())return REFLEX_UNSUPPORTED_FP;
    reflex_preparation_node_input in[10];std::memcpy(in,nodes,nodes_size);if(!graph(in,count))return REFLEX_INVALID_INPUT;
    const float* constants[]={config.axis_x,config.axis_y,config.axis_z,config.numerator,config.zero4,config.zero3};
    for(const auto constant:constants)for(unsigned i=0;i<4;++i)if(!reflex_detail::finite(constant[i]))return REFLEX_INVALID_INPUT;
    if(config.mask[0]!=0xffffffffu||config.mask[1]!=0xffffffffu||config.mask[2]!=0xffffffffu||config.mask[3]!=0)return REFLEX_INVALID_INPUT;
    reflex_preparation_node_output complete[10]{};Arithmetic ar;
    for(unsigned i=0;i<count;++i){
        for(const auto& row:in[i].coefficients)for(float w:row)if(!reflex_detail::finite(w))return REFLEX_INVALID_INPUT;
        if(i){const unsigned rows=in[i].kind==1?12:8;
            for(unsigned r=0;r<rows;++r)for(unsigned k=0;k<4;++k)if(!reflex_detail::finite(in[i].matrix38[r][k])||!reflex_detail::finite(in[i].matrix3c[r][k]))return REFLEX_INVALID_INPUT;
            std::memcpy(complete[i].matrices[0],in[i].matrix38,rows*16);std::memcpy(complete[i].matrices[1],in[i].matrix3c,rows*16);}
    }
    for(unsigned i=count;i-->1;){auto& m=complete[i].matrices;const bool fixed=in[i].kind==1;initialize(in[i],config,m[2],ar);transpose(m[1],fixed?12:8,m[4]);
        for(unsigned r=0;r<(fixed?12u:4u);++r)std::memcpy(m[6][r],fixed?config.zero3:config.zero4,16);
        std::memcpy(m[8],m[0],fixed?192:128);transpose(m[8],fixed?12:8,m[9]);}
    initialize(in[0],config,complete[0].matrices[2],ar);if(!ar.valid)return REFLEX_NUMERIC_RANGE;
    for(unsigned i=count;i-->1;){auto& m=complete[i].matrices;const bool fixed=in[i].kind==1;
        for(unsigned k=0;k<in[i].child_count;++k){const auto child=in[i].children[k];Rows product{};node_product(complete[child],in[child].kind,8,product,ar);std::memcpy(complete[child].node_products[0],product,192);ar.sub_rows(m[2],product,12,m[2]);}
        if(!ar.valid)return REFLEX_NUMERIC_RANGE;auto status=invert(m[2],false,config,m[3]);if(status!=REFLEX_OK)return status;
        transpose(m[4],fixed?12:6,m[5]);status=paired(m[3],m[4],m[5],in[i].kind,false,ar);if(status!=REFLEX_OK)return status;
        Rows product{};node_product(complete[i],in[i].kind,9,product,ar);std::memcpy(complete[i].node_products[1],product,fixed?192:64);ar.sub_rows(m[6],product,fixed?12:4,m[6]);if(!ar.valid)return REFLEX_NUMERIC_RANGE;
        status=invert(m[6],!fixed,config,m[7]);if(status!=REFLEX_OK)return status;status=paired(m[7],m[8],m[9],in[i].kind,true,ar);if(status!=REFLEX_OK)return status;
    }
    auto& root=complete[0].matrices;
    for(unsigned k=0;k<in[0].child_count;++k){const auto child=in[0].children[k];Rows product{};node_product(complete[child],in[child].kind,8,product,ar);std::memcpy(complete[child].node_products[0],product,192);ar.sub_rows(root[2],product,12,root[2]);}
    if(!ar.valid)return REFLEX_NUMERIC_RANGE;const auto status=invert(root[2],false,config,root[3]);if(status!=REFLEX_OK)return status;
    std::memcpy(output,complete,output_size);return REFLEX_OK;
}
