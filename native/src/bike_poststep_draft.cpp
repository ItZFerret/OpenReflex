// Unregistered internal draft: original bike callbacks 43c440 and 43c1a0.
// Include the unchanged force draft once for its strict quaternion arithmetic.
#include "bike_force_draft.cpp"

namespace bike_poststep_draft {
using bike_force_draft::Vec;
using bike_force_draft::Arithmetic;
using bike_force_draft::finite;
using bike_force_draft::literal;
// Original record order is pinned in bike_poststep_model.py, not a public ABI.
struct Post7Input { Vec vectors[9];uint32_t flags[3];float counter,throttle,dt; };
struct Post7Output { Vec vectors[8];float counter; };
struct Post17Input { Vec vectors[7];float scalars[4];Vec axis_y;float dt; };
struct Post17Output { float scalars[4]; };
static_assert(sizeof(Post7Input)==168&&sizeof(Post7Output)==132,"post7 draft layout");
static_assert(sizeof(Post17Input)==148&&sizeof(Post17Output)==16,"post17 draft layout");
static_assert(offsetof(Post7Input,flags)==144&&offsetof(Post17Input,axis_y)==128,"post draft offsets");

uint32_t post7(const Post7Input& in,Post7Output& output){
    for(const Vec& row:in.vectors)if(!finite(row))return REFLEX_INVALID_INPUT;
    for(float value:{in.counter,in.throttle,in.dt})if(!reflex_detail::finite(value))return REFLEX_INVALID_INPUT;
    Arithmetic op;Vec state[9];std::memcpy(state,in.vectors,sizeof(state));
    if(in.flags[0]){
        const Vec delta=op.minus(state[7],state[0]);state[5]=state[8];
        state[0]=op.plus(state[0],delta);
        state[1]=op.plus(delta,state[1]); // the original reverses these operands
        state[2]=op.plus(state[2],delta);state[3]=op.plus(state[3],delta);
    }else{state[6]=state[4];state[7]=state[0];state[8]=state[5];}
    float counter=in.counter;
    if(in.flags[1]&&in.flags[2]){
        counter=op.add(op.mul(in.dt,6),counter);
        if(counter>=1.5f)counter=1.5f;else if(counter<=0)counter=0;
    }else if(!in.flags[2]){
        counter=op.sub(counter,in.dt);if(counter<=0)counter=0;
    }else if(in.throttle<=literal(0x3e4ccccdu)){
        counter=op.sub(counter,op.mul(in.dt,literal(0x3dcccccdu)));if(counter<=0)counter=0;
    }
    if(!op.valid)return REFLEX_NUMERIC_RANGE;
    Post7Output result{};constexpr unsigned indices[8]={0,1,2,3,5,6,7,8};
    for(unsigned i=0;i<8;++i)result.vectors[i]=state[indices[i]];result.counter=counter;output=result;return REFLEX_OK;
}

uint32_t post17(const Post17Input& in,Post17Output& output){
    for(const Vec& row:in.vectors)if(!finite(row))return REFLEX_INVALID_INPUT;
    for(float value:in.scalars)if(!reflex_detail::finite(value))return REFLEX_INVALID_INPUT;
    if(!finite(in.axis_y)||!reflex_detail::finite(in.dt))return REFLEX_INVALID_INPUT;
    Arithmetic op;const Vec& q=in.vectors[0];const Vec cq=op.conjugate(q);
    const Vec axis=op.rotate(q,in.axis_y,cq,true);
    // Preserve the model's bounded finite packed products, including discarded W.
    Vec products{};for(unsigned i=0;i<4;++i)products[i]=op.mul(axis[i],in.vectors[6][i]);
    const float dot=op.add(op.add(products[0],products[1]),products[2]);
    const Vec force=op.rotate(q,in.vectors[5],cq,false);
    const float acceleration=op.add(op.add(op.mul(in.scalars[0],-75),op.mul(dot,0)),op.sub(literal(0x80000000u),force[1]));
    Post17Output result{};
    result.scalars[1]=op.mul(op.add(op.mul(acceleration,in.dt),in.scalars[1]),literal(0x3f75c28fu));
    result.scalars[0]=op.add(op.mul(result.scalars[1],in.dt),in.scalars[0]);
    const Vec angular=op.rotate(q,in.vectors[1],cq,true),local=op.rotate(q,angular,cq,false);
    result.scalars[2]=op.sub(in.vectors[2][0],local[0]);
    const Vec& rear=in.vectors[3];const Vec local_rear=op.rotate(rear,angular,op.conjugate(rear),false);
    result.scalars[3]=op.sub(in.vectors[4][0],local_rear[0]);
    if(!op.valid)return REFLEX_NUMERIC_RANGE;output=result;return REFLEX_OK;
}
} // namespace bike_poststep_draft

extern "C" uint32_t reflex_bike_poststep_draft(uint32_t mode,const void* input,uint32_t input_size,void* output,uint32_t output_size){
    using namespace bike_poststep_draft;
    switch(mode){
        case 1:return bike_force_draft::execute<Post7Input,Post7Output>(input,input_size,output,output_size,post7);
        case 2:return bike_force_draft::execute<Post17Input,Post17Output>(input,input_size,output,output_size,post17);
        default:return REFLEX_INVALID_INPUT;
    }
}
