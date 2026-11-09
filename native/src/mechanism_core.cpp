// Complete bounded original node2-through-velocity mechanism core.
#include "node2_internal.h"
namespace mechanism_core_detail {
using Globals=node2_detail::Globals;
struct Config {uint32_t node_count;float dt,gravity[4];Globals globals;float numerator[4],extra[4];};
using Node=reflex_mechanism_node_input;
using Output=reflex_mechanism_node_output;
static_assert(sizeof(reflex_mechanism_core_input)==216&&offsetof(reflex_mechanism_core_input,axis_x)==24&&offsetof(reflex_mechanism_core_input,numerator)==184,"public core config");
static_assert(sizeof(Config)==216&&sizeof(Node)==656&&sizeof(Output)==2528,"core draft layouts");
static_assert(offsetof(Config,globals)==24&&offsetof(Config,numerator)==184&&offsetof(Node,bodies)==32&&offsetof(Node,sources)==224&&offsetof(Node,locals)==608,"core draft offsets");
bool graph(const Node* nodes,unsigned count){
    unsigned incoming[10]{};
    for(unsigned i=0;i<count;++i){const auto& n=nodes[i];if(i?((n.kind!=1&&n.kind!=2)||n.parent<0||static_cast<unsigned>(n.parent)>=i):(n.kind!=0||n.parent!=-1))return false;
        if(n.child_count>5)return false;for(unsigned j=0;j<n.child_count;++j){const auto child=n.children[j];if(!child||child>=count||nodes[child].parent!=static_cast<int32_t>(i)||++incoming[child]!=1)return false;}}
    for(unsigned i=1;i<count;++i)if(incoming[i]!=1)return false;return true;
}
template<class In,class Out> uint32_t node2(const Config& config,const Node* nodes,unsigned i,Out& out){
    In input{};input.globals=config.globals;
    for(unsigned body=0;body<2;++body)for(unsigned row=0;row<2;++row){std::memcpy(input.bodies[body][row].data(),nodes[nodes[i].parent].bodies[body][row],16);std::memcpy(input.bodies[body+2][row].data(),nodes[i].bodies[body][row],16);}
    std::memcpy(input.local,nodes[i].locals,sizeof(input.local));for(unsigned m=0;m<2;++m)std::memcpy(input.matrices[m],nodes[i].sources[m],sizeof(input.matrices[m]));
    if constexpr(sizeof(In)==sizeof(node2_detail::FixedInput))return node2_detail::fixed(input,out);
    else return node2_detail::rotor(input,out);
}
uint32_t calculate(const Config& config,const Node* nodes,Output* output){
    const unsigned count=config.node_count;Output complete[10]{};reflex_preparation_node_input preparation_nodes[10]{};
    for(unsigned i=0;i<count;++i){auto& n=preparation_nodes[i];std::memcpy(&n,&nodes[i],32);for(unsigned body=0;body<2;++body)std::memcpy(n.coefficients[body],nodes[i].bodies[body][2],16);
        if(!i)continue;uint32_t status;
        if(nodes[i].kind==1){node2_detail::FixedOutput result;status=node2<node2_detail::FixedInput>(config,nodes,i,result);if(status!=REFLEX_OK)return status;
            std::memcpy(n.matrix38,result.matrices[0],192);std::memcpy(n.matrix3c,result.matrices[1],192);std::memcpy(complete[i].bias,result.bias,32);}
        else {node2_detail::RotorOutput result;status=node2<node2_detail::RotorInput>(config,nodes,i,result);if(status!=REFLEX_OK)return status;
            std::memcpy(n.matrix38,result.matrices[0],128);std::memcpy(n.matrix3c,result.matrices[1],128);std::memcpy(complete[i].bias,result.bias,16);}
    }
    reflex_preparation_input prep{};prep.node_count=count;std::memcpy(prep.axis_x,config.globals.axis_x.data(),16);std::memcpy(prep.axis_y,config.globals.axis_y.data(),16);std::memcpy(prep.axis_z,config.globals.axis_z.data(),16);
    std::memcpy(prep.numerator,config.numerator,16);std::memcpy(prep.zero4,config.extra,16);std::memcpy(prep.zero3,config.globals.g.data(),16);std::memcpy(prep.mask,config.globals.mask,16);
    reflex_preparation_node_output prepared[10];auto status=reflex_prepare_matrices(&prep,sizeof(prep),preparation_nodes,count*sizeof(*preparation_nodes),prepared,count*sizeof(*prepared));if(status!=REFLEX_OK)return status;
    reflex_force_input force{};force.dt=config.dt;force.node_count=static_cast<int32_t>(count);std::memcpy(force.gravity,config.gravity,16);std::memcpy(force.matrix_global,config.globals.g.data(),16);
    reflex_force_node_input force_nodes[10]{};reflex_force_node_output forces[10];
    for(unsigned i=0;i<count;++i){const auto& b=nodes[i].bodies;auto& f=force_nodes[i];std::memcpy(f.mass,b[0][2],16);std::memcpy(f.accumulated_linear_force,b[0][4],16);std::memcpy(f.linear_impulse,b[0][5],16);
        std::memcpy(f.angular_velocity,b[1][1],16);std::memcpy(f.angular_coefficient,b[1][2],16);std::memcpy(f.accumulated_angular_force,b[1][4],16);std::memcpy(f.angular_impulse,b[1][5],16);}
    status=reflex_force_compose(&force,sizeof(force),force_nodes,count*sizeof(*force_nodes),forces,count*sizeof(*forces));if(status!=REFLEX_OK)return status;
    reflex_constraint_solve_input solve{};solve.node_count=count;std::memcpy(solve.global_g,config.globals.g.data(),16);reflex_constraint_node_input solve_nodes[10]{};reflex_constraint_node_output solved[10];
    constexpr unsigned slots[]={0,1,3,4,5,7,8,9};
    for(unsigned i=0;i<count;++i){auto& n=solve_nodes[i];std::memcpy(&n,&nodes[i],32);for(unsigned m=0;m<8;++m)std::memcpy(n.matrices[m],prepared[i].matrices[slots[m]],192);
        for(unsigned b=0;b<2;++b)std::memcpy(n.coefficients[b],nodes[i].bodies[b][3],16);std::memcpy(n.forces[0],forces[i].linear_force,16);std::memcpy(n.forces[1],forces[i].angular_force,16);std::memcpy(n.bias,complete[i].bias,32);}
    status=reflex_constraint_solve(&solve,sizeof(solve),solve_nodes,count*sizeof(*solve_nodes),solved,count*sizeof(*solved));if(status!=REFLEX_OK)return status;
    reflex_velocity_input velocity{config.dt,static_cast<int32_t>(count)};reflex_velocity_node_input velocity_nodes[10]{};reflex_velocity_node_output velocities[10];
    for(unsigned i=0;i<count;++i){auto& n=velocity_nodes[i];std::memcpy(n.linear_velocity,nodes[i].bodies[0][1],16);std::memcpy(n.angular_velocity,nodes[i].bodies[1][1],16);
        std::memcpy(n.reciprocal_mass,nodes[i].bodies[0][3],16);std::memcpy(n.angular_coefficient,nodes[i].bodies[1][3],16);std::memcpy(n.linear_force,solved[i].forces[0],16);std::memcpy(n.angular_force,solved[i].forces[1],16);}
    status=reflex_velocity_integrate(&velocity,sizeof(velocity),velocity_nodes,count*sizeof(*velocity_nodes),velocities,count*sizeof(*velocities));if(status!=REFLEX_OK)return status;
    for(unsigned i=0;i<count;++i){auto& out=complete[i];out.preparation=prepared[i];std::memcpy(out.forces,solved[i].forces,32);std::memcpy(out.rhs,solved[i].rhs,32);std::memcpy(out.workspace,solved[i].workspace,32);std::memcpy(out.solution,solved[i].solution,32);
        std::memcpy(out.accelerations[0],velocities[i].linear_accel,16);std::memcpy(out.accelerations[1],velocities[i].angular_accel,16);std::memcpy(out.velocities[0],velocities[i].linear_velocity,16);std::memcpy(out.velocities[1],velocities[i].angular_velocity,16);}
    std::memcpy(output,complete,count*sizeof(*output));return REFLEX_OK;
}
}
uint32_t REFLEX_CALL reflex_mechanism_core(const reflex_mechanism_core_input* config,uint32_t config_size,const mechanism_core_detail::Node* nodes,uint32_t nodes_size,mechanism_core_detail::Output* output,uint32_t output_size){
    using namespace mechanism_core_detail;
    if(!config||!nodes||!output)return REFLEX_NULL_ARGUMENT;
    if(config_size!=sizeof(*config))return REFLEX_ABI_MISMATCH;
    if(reinterpret_cast<uintptr_t>(config)%4||reinterpret_cast<uintptr_t>(nodes)%4||reinterpret_cast<uintptr_t>(output)%4)return REFLEX_INVALID_INPUT;
    Config input;std::memcpy(&input,config,sizeof(input));if(input.node_count<1||input.node_count>10)return REFLEX_INVALID_INPUT;
    if(nodes_size!=input.node_count*sizeof(*nodes)||output_size!=input.node_count*sizeof(*output))return REFLEX_ABI_MISMATCH;
    if(!reflex_detail::supported_fp())return REFLEX_UNSUPPORTED_FP;
    Node snapshot[10];std::memcpy(snapshot,nodes,nodes_size);if(!graph(snapshot,input.node_count))return REFLEX_INVALID_INPUT;
    return calculate(input,snapshot,output);
}
