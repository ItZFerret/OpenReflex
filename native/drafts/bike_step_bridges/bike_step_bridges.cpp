#include "bike_step_bridges.h"
#include "../../include/reflex_core.h"
#include "../../src/fp32.h"
#include <array>
#include <cstddef>
#include <cstring>
#include <stdexcept>

// Keep every reused implementation byte unchanged while giving this isolated
// link unit unique private symbols. The public plugin is not modified.
#define reflex_pose_integrate bridge_pose_math
#include "../../src/pose.cpp"
#undef reflex_pose_integrate
#define node2_draft bridge_node2_math
#define node6_draft bridge_node6_math
#define reflex_node2_draft bridge_node2_export
#define reflex_node6_draft bridge_node6_export
#include "../../src/node6_draft.cpp"
#undef reflex_node6_draft
#undef reflex_node2_draft
#undef node6_draft
#undef node2_draft
#define bike_force_draft bridge_postpose_force_math
#define bike_postpose_draft bridge_postpose_math
#define reflex_bike_force_draft bridge_postpose_force_export
#define reflex_bike_postpose_draft bridge_postpose_export
#include "../../src/bike_postpose_draft.cpp"
#undef reflex_bike_postpose_draft
#undef reflex_bike_force_draft
#undef bike_postpose_draft
#undef bike_force_draft
#define bike_force_draft bridge_poststep_force_math
#define bike_poststep_draft bridge_poststep_math
#define reflex_bike_force_draft bridge_poststep_force_export
#define reflex_bike_poststep_draft bridge_poststep_export
#include "../../src/bike_poststep_draft.cpp"
#undef reflex_bike_poststep_draft
#undef reflex_bike_force_draft
#undef bike_poststep_draft
#undef bike_force_draft

static_assert(sizeof(reflex_draft_bike_bridge_input)==39096,"private bridge input layout");
static_assert(sizeof(reflex_draft_bike_bridge_globals)==176,"private bridge global layout");
static_assert(offsetof(reflex_draft_bike_bridge_input,globals)==38920,"private bridge globals offset");
static_assert(offsetof(reflex_draft_bike_bridge_globals,extra)==160,"private bridge extra offset");

namespace {
using State=std::array<uint8_t,REFLEX_DRAFT_BIKE_STATE_BYTES>;
using Vec=std::array<float,4>;
struct SavedEnvironment {std::fenv_t env{};bool valid=std::fegetenv(&env)==0;~SavedEnvironment(){if(valid)std::fesetenv(&env);}};
template<class T> T read(const uint8_t* bytes,size_t offset){T value;std::memcpy(&value,bytes+offset,sizeof(value));return value;}
template<class T> void write(uint8_t* bytes,size_t offset,const T& value){std::memcpy(bytes+offset,&value,sizeof(value));}
void require(bool condition){if(!condition)throw std::runtime_error("invalid bike bridge snapshot");}
void status(uint32_t value){require(value==REFLEX_OK);}

void pose(const reflex_draft_bike_bridge_input& in,State& state){
    reflex_pose_input config{read<float>(reinterpret_cast<const uint8_t*>(&in.dt_bits),0),4,in.globals.extra[3]};
    reflex_pose_node_input nodes[4]{};reflex_pose_node_output output[4]{};
    for(unsigned i=0;i<4;i++){
        const size_t linear=0xf0+i*0xa0,angular=0x730+i*0xa0;
        std::memcpy(nodes[i].position,state.data()+linear,16);std::memcpy(nodes[i].linear_velocity,state.data()+linear+16,16);
        std::memcpy(nodes[i].orientation,state.data()+angular,16);std::memcpy(nodes[i].angular_velocity,state.data()+angular+16,16);
    }
    status(bridge_pose_math(&config,sizeof(config),nodes,sizeof(nodes),output,sizeof(output)));
    for(unsigned i=0;i<4;i++){std::memcpy(state.data()+0xf0+i*0xa0,output[i].position,16);std::memcpy(state.data()+0x730+i*0xa0,output[i].orientation,16);}
}
void steering(const reflex_draft_bike_bridge_input& in,State& state){
    bridge_postpose_math::Input input{};bridge_postpose_math::Output output{};
    constexpr size_t offsets[]={0x3770,0x41d0,0x730,0xf0,0x2d0,0x8a70},outputs[]={0x6eb0,0x8a80,0x77d0,0x8a90};
    for(unsigned i=0;i<6;i++)input.vectors[i]=read<Vec>(state.data(),offsets[i]);
    input.height=read<float>(state.data(),0x4054);input.length=read<float>(state.data(),0x4224);
    input.steering=read<Vec>(in.control,0x50);std::memcpy(input.axis_x.data(),in.globals.axis_x,16);std::memcpy(input.g.data(),in.globals.g,16);
    status(bridge_postpose_math::calculate(input,output));for(unsigned i=0;i<4;i++)write(state.data(),outputs[i],output.vectors[i]);
}
void node6(const reflex_draft_bike_bridge_input& in,State& state){
    require(in.simulation&&!(in.simulation&15)&&uint64_t(in.simulation)+state.size()<=0x100000000ull);
    auto local=[&](uint32_t address,size_t size=16){
        require(address>=in.simulation);const size_t offset=address-in.simulation;
        require(!(offset&15)&&size<=state.size()&&offset<=state.size()-size);return offset;
    };
    bridge_node2_math::Globals globals;std::memcpy(&globals,&in.globals,sizeof(globals));
    Vec extra;std::memcpy(extra.data(),in.globals.extra,16);
    constexpr size_t node_offsets[]={0x5cd0,0x5ff0,0x6e00,0x7760};
    for(unsigned index=1;index<4;index++){
        const size_t node=local(read<uint32_t>(state.data(),0xd70+index*4),0xc0);
        require(node==node_offsets[index]);
        require(read<uint32_t>(state.data(),node)==(index==1?0x93cbc8u:0x93cb9cu));
        size_t offsets[4];for(unsigned i=0;i<4;i++)offsets[i]=local(read<uint32_t>(state.data(),node+0x28+i*4));
        // This private bridge handles the constructor-pinned motocross graph.
        // The reused normalized node6 leaf does not establish arbitrary raw
        // within-node alias ordering; reject such bindings before arithmetic.
        require(offsets[0]==0xf0&&offsets[1]==0x730&&
                offsets[2]==0xf0+index*0xa0&&offsets[3]==0x730+index*0xa0);
        bridge_node6_math::Output output{};
        if(index==1){
            bridge_node6_math::FixedInput input{};input.globals=globals;input.extra=extra;
            for(unsigned i=0;i<4;i++)input.bodies[i]=read<Vec>(state.data(),offsets[i]);
            input.locals[0]=read<Vec>(state.data(),node+0x70);input.locals[1]=read<Vec>(state.data(),node+0x80);
            status(bridge_node6_math::fixed(input,output));
        }else{
            bridge_node6_math::RotorInput input{};input.globals=globals;input.extra=extra;
            for(unsigned i=0;i<4;i++)input.bodies[i]=read<Vec>(state.data(),offsets[i]);
            input.locals[0]=read<Vec>(state.data(),node+0x70);input.locals[1]=read<Vec>(state.data(),node+0x80);input.locals[2]=read<Vec>(state.data(),node+0xb0);
            status(bridge_node6_math::rotor(input,output));
        }
        write(state.data(),offsets[2],output.position);write(state.data(),offsets[3],output.orientation);
    }
}
void post_contact(const reflex_draft_bike_bridge_input& in,State& state){
    bridge_poststep_math::Post17Input input{};bridge_poststep_math::Post17Output output{};
    constexpr size_t vectors[]={0x730,0x740,0x880,0x910,0x920,0x160,0xd0},scalars[]={0x8d90,0x8d94,0x8a30,0x8a34};
    for(unsigned i=0;i<7;i++)input.vectors[i]=read<Vec>(state.data(),vectors[i]);
    for(unsigned i=0;i<4;i++)input.scalars[i]=read<float>(state.data(),scalars[i]);
    std::memcpy(input.axis_y.data(),in.globals.axis_y,16);std::memcpy(&input.dt,&in.dt_bits,4);
    status(bridge_poststep_math::post17(input,output));for(unsigned i=0;i<4;i++)write(state.data(),scalars[i],output.scalars[i]);
}
void filtered_speed(const reflex_draft_bike_bridge_input& in,State& state){
    bridge_poststep_force_math::Arithmetic op;const Vec v=read<Vec>(state.data(),0x100);Vec squares;
    for(unsigned i=0;i<4;i++)squares[i]=op.mul(v[i],v[i]);
    const float speed=op.sqrt(op.add(op.add(squares[0],squares[2]),squares[1]));
    const float period=read<float>(state.data(),0x98),fixed_dt=read<float>(state.data(),0xbc),old=read<float>(state.data(),0x94);
    require(reflex_detail::finite(period)&&period>0&&reflex_detail::finite(fixed_dt)&&reflex_detail::finite(old)&&reflex_detail::finite(in.globals.one));
    const float duration=period>fixed_dt?fixed_dt:period;
    const float blend=op.mul(duration,op.div(in.globals.one,period));
    const float value=op.add(op.mul(op.sub(speed,old),blend),old);require(op.valid);
    write(state.data(),0x9c,blend);write(state.data(),0x94,value);
}
}

extern "C" uint32_t reflex_draft_bike_step_bridge(uint32_t stage,const void* input,uint32_t input_size,void* output,uint32_t output_size){
    if(!input||!output||input_size!=sizeof(reflex_draft_bike_bridge_input)||output_size!=REFLEX_DRAFT_BIKE_STATE_BYTES||stage<1||stage>5)return 1;
    SavedEnvironment environment;if(!environment.valid||!reflex_detail::supported_fp())return 2;
    try{
        reflex_draft_bike_bridge_input in;std::memcpy(&in,input,sizeof(in));State state;std::memcpy(state.data(),in.state,state.size());
        require(read<uint32_t>(state.data(),0)==0x93c8a4&&read<uint32_t>(state.data(),0xec)==4);
        switch(stage){case 1:pose(in,state);break;case 2:steering(in,state);break;case 3:node6(in,state);break;case 4:post_contact(in,state);break;case 5:filtered_speed(in,state);break;}
        std::memcpy(output,state.data(),state.size());return 0;
    }catch(...){return 3;}
}
