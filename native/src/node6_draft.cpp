// Unregistered fixed85b750/rotor85a8f0 post-pose joint correction draft.
// Reuse literal quaternion/Matrix3 math; include node2_draft.cpp only once in a
// draft translation unit. No public ABI or build/Unity registration.
#include "node2_draft.cpp"

namespace node6_draft {
using Vec=node2_draft::Vec;using Globals=node2_draft::Globals;
template<unsigned N> struct Input { Vec bodies[4],locals[N];Globals globals;Vec extra; };
using FixedInput=Input<2>;using RotorInput=Input<3>;
struct Output { Vec position,orientation; };
static_assert(sizeof(FixedInput)==272&&sizeof(RotorInput)==288&&sizeof(Output)==32,"node6 original oracle layouts");
static_assert(offsetof(FixedInput,globals)==96&&offsetof(RotorInput,globals)==112,"node6 global offsets");
struct Arithmetic:node2_draft::Arithmetic {
    explicit Arithmetic(const Globals& g):node2_draft::Arithmetic{g}{}
    float div(float a,float b){return check(reflex_detail::div(a,b));}
    float sqrt(float a){return check(reflex_detail::sqrt(a));}
    Vec plus(const Vec& a,const Vec& b){Vec out;for(unsigned i=0;i<4;++i)out[i]=add(a[i],b[i]);return out;}
    Vec conjugate(const Vec& q){Vec out;for(unsigned i=0;i<4;++i)out[i]=mul(q[i],g.conjugate[i]);return out;}
};
template<unsigned N> bool valid(const Input<N>& in){
    if(!node2_draft::valid_globals(in.globals)||!node2_draft::finite(in.extra))return false;
    for(const Vec& row:in.bodies)if(!node2_draft::finite(row))return false;
    for(const Vec& row:in.locals)if(!node2_draft::finite(row))return false;return true;
}
uint32_t fixed(const FixedInput& in,Output& output){
    if(!valid(in))return REFLEX_INVALID_INPUT;Arithmetic op(in.globals);Output result;
    const Vec& pq=in.bodies[1];const Vec masked={in.locals[0][0],in.locals[0][1],in.locals[0][2],0};
    result.orientation=op.product(pq,in.locals[1]);
    result.position=op.plus(op.product(pq,op.product(masked,op.conjugate(pq))),in.bodies[0]);
    if(!op.valid)return REFLEX_NUMERIC_RANGE;output=result;return REFLEX_OK;
}
uint32_t rotor(const RotorInput& in,Output& output){
    if(!valid(in))return REFLEX_INVALID_INPUT;Arithmetic op(in.globals);Output result;
    const Vec& pq=in.bodies[1];const Vec& oq=in.bodies[3];const auto rows=op.quaternion_rows(pq);
    // Preserve the model's bounded finite domain for the originally executed
    // reverse-own matrix, even though this helper's output is not consumed.
    (void)op.quaternion_rows(oq,true);
    const Vec anchor=op.transform(rows,in.locals[0]),axis=op.transform(rows,in.locals[1]);
    const Vec base=op.plus(in.bodies[0],anchor);Vec delta;
    for(unsigned i=0;i<4;++i)delta[i]=op.sub(in.bodies[2][i],base[i]);
    const float distance=op.dot(delta,axis);Vec scaled;for(unsigned i=0;i<4;++i)scaled[i]=op.mul(distance,axis[i]);
    result.position=op.plus(scaled,base);
    const Vec relative=op.product(op.conjugate(pq),oq);Vec twist=op.product(op.conjugate(in.locals[2]),relative);
    // Literal partial lane replacement by runtime00d61660, not zero constants.
    twist[1]=in.extra[1];twist[2]=in.extra[2];Vec sq;
    for(unsigned i=0;i<4;++i)sq[i]=op.mul(twist[i],twist[i]);
    const float norm=op.sqrt(op.add(op.add(sq[3],sq[1]),op.add(sq[2],sq[0])));Vec normal{};
    // Original packed divide runs then masks zero magnitude to +0. Status is
    // outside this value-equivalence draft, so the invalid divide is omitted.
    if(norm!=0)for(unsigned i=0;i<4;++i)normal[i]=op.div(twist[i],norm);
    result.orientation=op.product(op.product(pq,in.locals[2]),normal);
    if(!op.valid)return REFLEX_NUMERIC_RANGE;output=result;return REFLEX_OK;
}
} // namespace node6_draft

// Inputs are snapshotted before either original output is computed. Normalized
// aligned overlaps are supported; original raw pointer aliases are not modeled.
extern "C" uint32_t reflex_node6_draft(uint32_t mode,const void* input,uint32_t input_size,void* output,uint32_t output_size){
    using namespace node6_draft;using node2_draft::execute;
    switch(mode){
        case 1:return execute<FixedInput,Output>(input,input_size,output,output_size,fixed);
        case 2:return execute<RotorInput,Output>(input,input_size,output,output_size,rotor);
        default:return REFLEX_INVALID_INPUT;
    }
}
