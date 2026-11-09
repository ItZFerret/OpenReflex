#include "reflex_core.h"
#include "fp32.h"
#include <cstddef>
#include <cstring>

static_assert(sizeof(reflex_constraint_solve_input)==20,"constraint config size");
static_assert(sizeof(reflex_constraint_node_input)==1760 && sizeof(reflex_constraint_node_output)==128,"constraint node sizes");
static_assert(alignof(reflex_constraint_solve_input)==4 && alignof(reflex_constraint_node_input)==4 && alignof(reflex_constraint_node_output)==4,"constraint alignment");
static_assert(offsetof(reflex_constraint_node_input,matrices)==32 && offsetof(reflex_constraint_node_input,coefficients)==1568
              && offsetof(reflex_constraint_node_input,forces)==1600 && offsetof(reflex_constraint_node_input,bias)==1632
              && offsetof(reflex_constraint_node_input,initial_rhs)==1664 && offsetof(reflex_constraint_node_input,initial64)==1696
              && offsetof(reflex_constraint_node_input,initial68)==1728,"constraint input offsets");
namespace {
constexpr uint32_t root_kind=0,fixed_kind=1,rotor_kind=2;
using Node=reflex_constraint_node_input;
using State=reflex_constraint_node_output;

bool graph(const Node* nodes,unsigned count) noexcept {
    unsigned incoming[10]{};
    for(unsigned i=0;i<count;++i){
        const auto& n=nodes[i];
        if((i==0 && (n.kind!=root_kind || n.parent!=-1))
           ||(i!=0 && (n.kind!=fixed_kind && n.kind!=rotor_kind)))return false;
        if(i && (n.parent<0 || static_cast<unsigned>(n.parent)>=count || static_cast<unsigned>(n.parent)==i))return false;
        if(n.child_count>5)return false;
        for(unsigned k=0;k<n.child_count;++k){
            const auto child=n.children[k];
            if(child==0 || child>=count || child==i || nodes[child].parent!=static_cast<int32_t>(i) || ++incoming[child]!=1)return false;
        }
    }
    for(unsigned i=1;i<count;++i){
        if(incoming[i]!=1)return false;
        unsigned cursor=i,depth=0;
        while(cursor){if(++depth>=count)return false;cursor=static_cast<unsigned>(nodes[cursor].parent);}
    }
    return std::memcmp(nodes[0].initial_rhs[0],nodes[0].initial68[0],16)==0;
}

uint32_t subtract(const Node& matrix,unsigned slot,const float (*source)[4],float (*destination)[4]) {
    if(matrix.kind==fixed_kind){
        reflex_matrix_input in;reflex_matrix_output out;
        std::memcpy(in.rows,matrix.matrices[slot],sizeof(in.rows));std::memcpy(in.source,source,sizeof(in.source));std::memcpy(in.destination,destination,sizeof(in.destination));
        const auto status=reflex_matrix_subtract(&in,sizeof(in),&out,sizeof(out));
        if(status==REFLEX_OK)std::memcpy(destination,out.destination,sizeof(out.destination));return status;
    }
    if(slot==4||slot==6){
        reflex_matrix4x6_subtract_input in;reflex_matrix4_output out;
        std::memcpy(in.rows,matrix.matrices[slot],sizeof(in.rows));std::memcpy(in.source,source,sizeof(in.source));std::memcpy(in.destination,destination,sizeof(in.destination));
        const auto status=reflex_matrix4x6_subtract(&in,sizeof(in),&out,sizeof(out));
        if(status==REFLEX_OK)std::memcpy(destination,out.destination,sizeof(out.destination));return status;
    }
    reflex_matrix6x4_subtract_input in;reflex_matrix_output out;
    std::memcpy(in.rows,matrix.matrices[slot],sizeof(in.rows));std::memcpy(in.source,source,sizeof(in.source));std::memcpy(in.destination,destination,sizeof(in.destination));
    const auto status=reflex_matrix6x4_subtract(&in,sizeof(in),&out,sizeof(out));
    if(status==REFLEX_OK)std::memcpy(destination,out.destination,sizeof(out.destination));return status;
}

uint32_t multiply(const Node& matrix,unsigned slot,float (*values)[4]) {
    if(matrix.kind==rotor_kind && slot==5){
        reflex_matrix4_multiply_input in;reflex_matrix4_output out;
        std::memcpy(in.rows,matrix.matrices[slot],sizeof(in.rows));std::memcpy(in.source,values,sizeof(in.source));
        const auto status=reflex_matrix4_multiply(&in,sizeof(in),&out,sizeof(out));
        if(status==REFLEX_OK)std::memcpy(values,out.destination,sizeof(out.destination));return status;
    }
    reflex_matrix6_multiply_input in;reflex_matrix_output out;
    std::memcpy(in.rows,matrix.matrices[slot],sizeof(in.rows));std::memcpy(in.source,values,sizeof(in.source));
    const auto status=reflex_matrix6_multiply(&in,sizeof(in),&out,sizeof(out));
    if(status==REFLEX_OK)std::memcpy(values,out.destination,sizeof(out.destination));return status;
}

template<typename Input>
void callback_inputs(Input& in,const Node& node,const State& parent,const State& own) {
    std::memcpy(in.matrices[0],node.matrices[0],sizeof(in.matrices[0]));std::memcpy(in.matrices[1],node.matrices[1],sizeof(in.matrices[1]));
    std::memcpy(in.forces,parent.forces,32);std::memcpy(in.forces[2],own.forces,32);
}
uint32_t slot3(const Node& node,const Node& parent,const State& parent_state,State& own) {
    if(node.kind==fixed_kind){
        reflex_fixed_slot3_input in;reflex_matrix_output out;callback_inputs(in,node,parent_state,own);
        std::memcpy(in.coefficients,parent.coefficients,32);std::memcpy(in.coefficients[2],node.coefficients,32);std::memcpy(in.bias,node.bias,sizeof(in.bias));
        const auto status=reflex_fixed_slot3(&in,sizeof(in),&out,sizeof(out));
        if(status==REFLEX_OK)std::memcpy(own.rhs,out.destination,sizeof(out.destination));return status;
    }
    reflex_rotor_slot3_input in;reflex_matrix4_output out;callback_inputs(in,node,parent_state,own);
    std::memcpy(in.coefficients,parent.coefficients,32);std::memcpy(in.coefficients[2],node.coefficients,32);std::memcpy(in.bias,node.bias,sizeof(in.bias));
    const auto status=reflex_rotor_slot3(&in,sizeof(in),&out,sizeof(out));
    if(status==REFLEX_OK)std::memcpy(own.rhs,out.destination,sizeof(out.destination));return status;
}
uint32_t slot5(const Node& node,State& parent,State& own) {
    reflex_body_force_output out;uint32_t status;
    if(node.kind==fixed_kind){
        reflex_fixed_slot5_input in;callback_inputs(in,node,parent,own);std::memcpy(in.solution,own.solution,sizeof(in.solution));
        for(unsigned i=0;i<4;++i)in.aliases[i]=i;
        status=reflex_fixed_slot5(&in,sizeof(in),&out,sizeof(out));
    }else{
        reflex_rotor_slot5_input in;callback_inputs(in,node,parent,own);std::memcpy(in.solution,own.solution,sizeof(in.solution));
        for(unsigned i=0;i<4;++i)in.aliases[i]=i;
        status=reflex_rotor_slot5(&in,sizeof(in),&out,sizeof(out));
    }
    if(status==REFLEX_OK){std::memcpy(parent.forces,out.forces,32);std::memcpy(own.forces,out.forces[2],32);}return status;
}
} // namespace

uint32_t REFLEX_CALL reflex_constraint_solve(const reflex_constraint_solve_input* input,uint32_t input_size,
                                            const reflex_constraint_node_input* nodes,uint32_t nodes_size,
                                            reflex_constraint_node_output* output,uint32_t output_size) {
    if(!input||!nodes||!output)return REFLEX_NULL_ARGUMENT;
    if(input_size!=sizeof(*input))return REFLEX_ABI_MISMATCH;
    if(reinterpret_cast<uintptr_t>(input)%4||reinterpret_cast<uintptr_t>(nodes)%4||reinterpret_cast<uintptr_t>(output)%4)return REFLEX_INVALID_INPUT;
    reflex_constraint_solve_input config;std::memcpy(&config,input,sizeof(config));
    if(config.node_count<1||config.node_count>10)return REFLEX_INVALID_INPUT;
    const unsigned count=config.node_count;
    if(nodes_size!=count*sizeof(*nodes)||output_size!=count*sizeof(*output))return REFLEX_ABI_MISMATCH;
    if(!reflex_detail::supported_fp())return REFLEX_UNSUPPORTED_FP;
    Node in[10];std::memcpy(in,nodes,nodes_size);State complete[10];
    if(!graph(in,count))return REFLEX_INVALID_INPUT;
    for(float word:config.global_g)if(!reflex_detail::finite(word))return REFLEX_INVALID_INPUT;
    for(unsigned i=0;i<count;++i){
        std::memcpy(complete[i].forces,in[i].forces,32);std::memcpy(complete[i].rhs,in[i].initial_rhs,32);
        std::memcpy(complete[i].workspace,in[i].initial64,32);std::memcpy(complete[i].solution,in[i].initial68,32);
    }
    for(unsigned i=1;i<count;++i){
        const auto parent=static_cast<unsigned>(in[i].parent);const auto status=slot3(in[i],in[parent],complete[parent],complete[i]);
        if(status!=REFLEX_OK)return status;
    }
    for(unsigned i=count;i-->0;){
        std::memcpy(complete[i].workspace[0],config.global_g,16);std::memcpy(complete[i].workspace[1],config.global_g,16);
        for(unsigned k=0;k<in[i].child_count;++k){
            const auto child=in[i].children[k];const auto status=subtract(in[child],7,complete[child].solution,complete[i].workspace);
            if(status!=REFLEX_OK)return status;
        }
        if(i){
            std::memcpy(complete[i].solution,complete[i].rhs,in[i].kind==fixed_kind?32:16);
            const auto status=subtract(in[i],4,complete[i].workspace,complete[i].solution);if(status!=REFLEX_OK)return status;
        }else{
            const auto status=multiply(in[i],2,complete[i].workspace);if(status!=REFLEX_OK)return status;
        }
    }
    for(unsigned i=1;i<count;++i){
        const auto parent=static_cast<unsigned>(in[i].parent);
        auto status=multiply(in[i],5,complete[i].solution);if(status!=REFLEX_OK)return status;
        status=subtract(in[i],6,complete[parent].workspace,complete[i].solution);if(status!=REFLEX_OK)return status;
        status=multiply(in[i],2,complete[i].workspace);if(status!=REFLEX_OK)return status;
        status=subtract(in[i],3,complete[i].solution,complete[i].workspace);if(status!=REFLEX_OK)return status;
        status=slot5(in[i],complete[parent],complete[i]);if(status!=REFLEX_OK)return status;
    }
    std::memcpy(output,complete,output_size);return REFLEX_OK;
}
