// Unregistered closed443af0/43f270/447810 corrections. No complete callback claim.
#include "bike_force_draft.cpp"
namespace bike_velocity_correction_draft {
using namespace bike_force_draft;
struct CapInput { uint32_t count,flag;Vec angular[4]; };
struct CapOutput { Vec angular[4];uint32_t flag; };
struct AlternateInput { Vec angular; };
struct AlternateOutput { Vec angular; };
struct WheelInput { Vec root_q,root_w,front_q,front_w,rear_q,rear_w;float selector[2];uint32_t count;float knots[10],values[10],first_slope,last_slope; };
struct WheelOutput { Vec front_w,rear_w;uint32_t flag; };
static_assert(sizeof(CapInput)==72&&sizeof(CapOutput)==68&&sizeof(AlternateInput)==16&&sizeof(AlternateOutput)==16,"cap layouts");
static_assert(sizeof(WheelInput)==196&&sizeof(WheelOutput)==36&&offsetof(WheelInput,count)==104,"wheel correction layout");
float length(Arithmetic& op,const Vec& v){
    const float xx=op.mul(v[0],v[0]),yy=op.mul(v[1],v[1]),zz=op.mul(v[2],v[2]);
    return op.sqrt(op.add(op.add(xx,zz),yy));
}
bool cap(Arithmetic& op,Vec& v,float maximum){
    const float size=length(op,v);if(!(maximum<size))return false;
    for(unsigned j=0;j<4;++j)v[j]=op.div(op.mul(v[j],maximum),size);return true;
}
uint32_t calculate_cap(const CapInput& in,CapOutput& output){
    if(in.count<1||in.count>4)return REFLEX_INVALID_INPUT;
    for(unsigned i=0;i<in.count;++i)if(!finite(in.angular[i]))return REFLEX_INVALID_INPUT;
    CapOutput result;std::memcpy(result.angular,in.angular,sizeof(result.angular));result.flag=in.flag;
    Arithmetic op;for(unsigned i=0;i<in.count;++i)if(cap(op,result.angular[i],i==0?20.f:120.f))result.flag=1;
    if(!op.valid)return REFLEX_NUMERIC_RANGE;output=result;return REFLEX_OK;
}
uint32_t calculate_alternate(const AlternateInput& in,AlternateOutput& output){
    if(!finite(in.angular))return REFLEX_INVALID_INPUT;
    Arithmetic op;AlternateOutput result{in.angular};cap(op,result.angular,10);
    if(!op.valid)return REFLEX_NUMERIC_RANGE;output=result;return REFLEX_OK;
}
uint32_t calculate_wheel(const WheelInput& in,WheelOutput& output){
    if(!finite(in.root_q)||!finite(in.root_w)||!finite(in.front_q)||!finite(in.front_w)||!finite(in.rear_q)||!finite(in.rear_w)
       ||!reflex_detail::finite(in.selector[0])||!reflex_detail::finite(in.selector[1])||in.count<2||in.count>10)return REFLEX_INVALID_INPUT;
    Arithmetic op;Vec masked=in.root_w;masked[3]=0;
    Vec world=op.product(in.root_q,op.product(masked,op.conjugate(in.root_q)));world[3]=0;
    WheelOutput result{};unsigned locked=0;
    for(unsigned i=0;i<2;++i){
        const Vec& q=i?in.rear_q:in.front_q;const Vec& w=i?in.rear_w:in.front_w;
        const Vec relative=op.product(op.conjugate(q),op.product(world,q));
        const Vec delta=op.minus(w,relative);const float size=length(op,delta);
        if(!op.valid)return REFLEX_NUMERIC_RANGE;
        float factor=0;
        if(in.selector[i]>0&&size<10)++locked;
        else{
            CurveInput c{};c.x=size;c.count=in.count;std::memcpy(c.knots,in.knots,sizeof(c.knots));std::memcpy(c.values,in.values,sizeof(c.values));
            c.first_slope=in.first_slope;c.last_slope=in.last_slope;CurveOutput value{};
            const auto status=curve(c,value);if(status!=REFLEX_OK)return status;factor=value.value;
        }
        (i?result.rear_w:result.front_w)=op.plus(op.scale(delta,factor),relative);
    }
    result.flag=locked==2?1:0;if(!op.valid)return REFLEX_NUMERIC_RANGE;output=result;return REFLEX_OK;
}
}
extern "C" uint32_t reflex_bike_velocity_correction_draft(uint32_t mode,const void* input,uint32_t input_size,void* output,uint32_t output_size){
    using namespace bike_velocity_correction_draft;
    switch(mode){
        case 1:return execute<CapInput,CapOutput>(input,input_size,output,output_size,calculate_cap);
        case 2:return execute<AlternateInput,AlternateOutput>(input,input_size,output,output_size,calculate_alternate);
        case 3:return execute<WheelInput,WheelOutput>(input,input_size,output,output_size,calculate_wheel);
        default:return REFLEX_INVALID_INPUT;
    }
}
