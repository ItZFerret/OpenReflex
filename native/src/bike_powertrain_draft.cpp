// Unregistered finite native draft:4390d0/43efa0,445e80/4460f0,446340.
// This standalone translation unit reuses the verified force/curve draft.
// Do not compile bike_force_draft.cpp separately into the same draft binary.
#include "bike_force_draft.cpp"

namespace bike_powertrain_draft {
using bike_force_draft::Vec;
using bike_force_draft::Arithmetic;
using bike_force_draft::finite;
using bike_force_draft::literal;
struct Curve { uint32_t count;float knots[10],values[10],first_slope,last_slope; };
struct IndexedInput { float x;uint32_t profile;float table[33]; };
struct IndexedOutput { float value; };
// Vec/scalar/table order matches the original raw oracle input records.
struct DampingInput { Vec vectors[14];float scalars[5];uint32_t flags[3];float tables[4][20];Vec control_force,g;uint32_t profile;float table_preceding[4]; };
struct DampingOutput { Vec vectors[6]; };
struct GearInput { Vec orientation,velocity;float scalars[6];uint32_t gate,gear;float ratios[5];Curve curve; };
struct GearOutput { uint32_t gear;float timer; };
struct EngineInput {
    Vec vectors[11];float scalars[13];uint32_t gear;float ratios[5];Curve curves[2];
    float throttle,brake,reaction;uint32_t mode;Vec g,numerator,axis_y;float dt;uint32_t profile;
};
struct EngineOutput { Vec vectors[5];float scalars[4]; };
static_assert(sizeof(Curve)==92&&sizeof(IndexedInput)==140&&sizeof(DampingInput)==628,"indexed/damping record sizes");
static_assert(sizeof(GearInput)==176&&sizeof(EngineInput)==508,"powertrain input record sizes");
static_assert(sizeof(DampingOutput)==96&&sizeof(GearOutput)==8&&sizeof(EngineOutput)==96,"powertrain output record sizes");
static_assert(offsetof(EngineInput,curves)==252&&offsetof(EngineInput,g)==452&&offsetof(EngineInput,dt)==500,"engine raw offsets");

float neg(Arithmetic& op,float value){return op.sub(-0.f,value);}
float lerp(Arithmetic& op,float a,float b,float t){
    if(t<=0)return a;if(t>=1)return b;return op.add(op.mul(op.sub(b,a),t),a);
}
float clamp(float value,float low,float high){if(value>=high)value=high;if(value<=low)value=low;return value;}
uint32_t parameter(float x,const Curve& table,float& value){
    bike_force_draft::CurveInput in;in.x=x;std::memcpy(&in.count,&table,sizeof(table));bike_force_draft::CurveOutput out;
    const auto status=bike_force_draft::curve(in,out);if(status==REFLEX_OK)value=out.value;return status;
}
// Exact addition rounded to 24-bit significand without a double-rounding hole.
// TwoSum retains the binary64 addition residual; it only matters when the
// rounded double lies exactly on a binary32 midpoint. Inputs here are bounded
// finite values; every accepted sum is normal and within the int24 domain.
double signed_half_add(double value,uint32_t profile){
    const double half=std::copysign(.5,value);volatile double sum=value+half;
    if(!(profile&2))return sum; // this harness maps bit2 to CW007f, otherwise027f
    volatile double other=sum-value;volatile double tail_a=value-(sum-other),tail_b=half-other;
    volatile double error=tail_a+tail_b;volatile float nearest=static_cast<float>(sum);
    const float lower=std::nextafter(nearest,-INFINITY),upper=std::nextafter(nearest,INFINITY);
    const double low_mid=(static_cast<double>(nearest)+lower)*.5,high_mid=(static_cast<double>(nearest)+upper)*.5;
    if(sum==low_mid&&error<0)return lower;if(sum==high_mid&&error>0)return upper;
    return nearest;
}
struct IndexedAccess { int low,left,right;float fraction; };
uint32_t indexed_access(Arithmetic& op,float x,uint32_t profile,IndexedAccess& access){
    if(profile>3)return REFLEX_INVALID_INPUT;
    if(!reflex_detail::finite(x))return REFLEX_INVALID_INPUT;
    const float doubled=op.mul(x,2);if(!op.valid)return REFLEX_NUMERIC_RANGE;
    // Original SUBSD is binary64; each following x87 FADD independently rounds
    // to CW007f's24-bit or CW027f's53-bit significand before truncation.
    volatile double shifted=static_cast<double>(doubled)-0.49999999999;
    const double low=std::trunc(signed_half_add(shifted,profile));
    if(!(low>-(1<<24)&&low<(1<<24)))return REFLEX_INVALID_INPUT;
    const float low_float=reflex_detail::from_int(static_cast<int32_t>(low));
    const double left=std::trunc(signed_half_add(low_float,profile)),right=std::trunc(signed_half_add(doubled,profile));
    if(!(left>-(1<<24)&&left<(1<<24)&&right>-(1<<24)&&right<(1<<24)))return REFLEX_INVALID_INPUT;
    access={static_cast<int>(low),static_cast<int>(left),static_cast<int>(right),op.sub(doubled,low_float)};return op.valid?REFLEX_OK:REFLEX_NUMERIC_RANGE;
}
uint32_t indexed_value(Arithmetic& op,float x,const float* table,int count,int base,uint32_t profile,float& result){
    IndexedAccess access;const auto status=indexed_access(op,x,profile,access);if(status!=REFLEX_OK)return status;
    const int left=access.left,right=access.right;
    if(left<base||left>=base+count||right<base||right>=base+count)return REFLEX_INVALID_INPUT;
    const float a=table[static_cast<int>(left)-base],b=table[static_cast<int>(right)-base];
    if(!reflex_detail::finite(a)||!reflex_detail::finite(b))return REFLEX_INVALID_INPUT;
    result=lerp(op,a,b,access.fraction);return op.valid?REFLEX_OK:REFLEX_NUMERIC_RANGE;
}
uint32_t indexed(const IndexedInput& in,IndexedOutput& output){
    if(in.profile>3)return REFLEX_INVALID_INPUT;Arithmetic op;IndexedOutput result;
    const auto status=indexed_value(op,in.x,in.table,33,-16,in.profile,result.value);if(status==REFLEX_OK)output=result;return status;
}
uint32_t damping(const DampingInput& in,DampingOutput& output){
    if(in.profile>3||!finite(in.g)||!finite(in.control_force))return REFLEX_INVALID_INPUT;
    for(const Vec& row:in.vectors)if(!finite(row))return REFLEX_INVALID_INPUT;
    for(float value:in.scalars)if(!reflex_detail::finite(value))return REFLEX_INVALID_INPUT;
    Arithmetic op;Vec state[14];std::memcpy(state,in.vectors,sizeof(state));Vec drag[2]={in.g,in.g};
    for(unsigned side=0;side<2;++side){
        if(!in.flags[side])continue;
        float tables[2][21];for(unsigned t=0;t<2;++t){tables[t][0]=in.table_preceding[2*side+t];std::memcpy(tables[t]+1,in.tables[2*side+t],80);}
        float first,second;auto status=indexed_value(op,in.scalars[1+side],tables[0],21,-1,in.profile,first);
        if(status!=REFLEX_OK)return status;status=indexed_value(op,in.scalars[1+side],tables[1],21,-1,in.profile,second);
        if(status!=REFLEX_OK)return status;
        const Vec v=state[1+side];const float x=op.mul(v[0],v[0]),y=op.mul(v[1],v[1]),z=op.mul(v[2],v[2]);
        const float length=op.sqrt(op.add(op.add(x,y),z));Vec normal{};
        if(length!=0)for(unsigned i=0;i<4;++i)normal[i]=op.div(v[i],length);
        const Vec combined=op.plus(op.scale(normal,op.sub(0,first)),op.scale(v,op.sub(0,second)));
        const float factor=clamp(op.div(in.scalars[3+side],literal(0x432de148u)),0,2);drag[side]=op.scale(combined,factor);
    }
    const Vec zero{};Vec root=op.scale(op.minus(zero,state[0]),in.scalars[0]);
    root=op.plus(op.plus(op.plus(root,in.control_force),drag[0]),drag[1]);
    Vec forces[3]={root,op.plus(state[3],in.g),op.plus(state[4],in.g)};
    if(in.flags[2])for(Vec& force:forces)force=op.scale(force,0);
    for(unsigned i=0;i<3;++i)op.point(state[5+i],forces[i],in.g,state[8+2*i],state[9+2*i]);
    if(!op.valid)return REFLEX_NUMERIC_RANGE;DampingOutput result;
    for(unsigned i=0;i<6;++i)result.vectors[i]=state[8+i];output=result;return REFLEX_OK;
}
uint32_t gear(const GearInput& in,GearOutput& output,bool downshift){
    if(!finite(in.orientation)||!finite(in.velocity)||in.gear>=5)return REFLEX_INVALID_INPUT;
    for(float value:in.scalars)if(!reflex_detail::finite(value))return REFLEX_INVALID_INPUT;
    for(float value:in.ratios)if(!reflex_detail::finite(value))return REFLEX_INVALID_INPUT;
    Arithmetic op;const Vec masked={in.velocity[0],in.velocity[1],in.velocity[2],0};
    const Vec local=op.product(op.conjugate(in.orientation),op.product(masked,in.orientation));
    float speed=op.div(local[2],in.scalars[0]);if(speed<.5f)speed=.5f;
    const float maximum=op.mul(in.scalars[1],literal(0x3dd67344u));GearOutput result{in.gear,in.scalars[5]};
    if(!op.valid)return REFLEX_NUMERIC_RANGE;
    if(in.gate){output=result;return REFLEX_OK;}
    const float square=op.mul(maximum,maximum);unsigned best=0;float best_value=0;
    for(unsigned i=0;i<5;++i){
        const float rotation=clamp(op.mul(in.ratios[i],speed),0,maximum);
        const float drag=neg(op,op.div(op.mul(op.mul(in.scalars[2],rotation),rotation),square));
        if(!op.valid)return REFLEX_NUMERIC_RANGE;
        float force;const auto status=parameter(rotation,in.curve,force);if(status!=REFLEX_OK)return status;
        const float score=op.add(op.sub(op.mul(force,in.ratios[i]),drag),drag);
        if(i==0||score>best_value){best=i;best_value=score;}
    }
    if(!op.valid)return REFLEX_NUMERIC_RANGE;
    if(downshift){if(best<in.gear&&result.timer>in.scalars[4])result={best,0};}
    else if(best>in.gear){if(result.timer>in.scalars[3])result={best,0};}
    else if(best<in.gear){if(result.timer>in.scalars[4])result={in.gear>0?in.gear-1:0,0};}
    output=result;return REFLEX_OK;
}
uint32_t automatic_gear(const GearInput& in,GearOutput& out){return gear(in,out,false);}
uint32_t down_gear(const GearInput& in,GearOutput& out){return gear(in,out,true);}

void coupling(Arithmetic& op,const Vec& q,const Vec& force,const Vec& position,const Vec& axis_y,float& angle,float& rate,float factor,float dt){
    const Vec conjugate=op.conjugate(q),axis=op.rotate(q,axis_y,conjugate,true);
    // The Python model intentionally retains the original unused dot's finite
    // domain; W product is executed too, although its reduction discards W.
    Vec products;for(unsigned i=0;i<4;++i)products[i]=op.mul(axis[i],position[i]);
    (void)op.add(op.add(products[0],products[1]),products[2]);
    const Vec local=op.rotate(q,force,conjugate,false);
    const float stiffness=factor<=0?1000:factor>=1?500:op.sub(1000,op.mul(factor,500));
    const float acceleration=op.add(neg(op,op.mul(angle,stiffness)),neg(op,local[1]));
    rate=op.mul(op.add(op.mul(acceleration,dt),rate),literal(0x3f7851ecu));angle=op.add(op.mul(rate,dt),angle);
}
uint32_t engine(const EngineInput& in,EngineOutput& output){
    if(in.profile>3||in.gear>=5||!finite(in.g)||!finite(in.numerator)||!finite(in.axis_y))return REFLEX_INVALID_INPUT;
    for(const Vec& row:in.vectors)if(!finite(row))return REFLEX_INVALID_INPUT;
    for(float value:in.scalars)if(!reflex_detail::finite(value))return REFLEX_INVALID_INPUT;
    for(float value:in.ratios)if(!reflex_detail::finite(value))return REFLEX_INVALID_INPUT;
    for(float value:{in.throttle,in.brake,in.reaction,in.dt})if(!reflex_detail::finite(value))return REFLEX_INVALID_INPUT;
    Arithmetic op;Vec v[11];float s[13];std::memcpy(v,in.vectors,sizeof(v));std::memcpy(s,in.scalars,sizeof(s));
    const float maximum=op.div(s[0],literal(0x4118cccdu)),minimum=op.div(s[1],literal(0x4118cccdu));
    float speed=clamp(s[2],0,maximum);const float fraction=clamp(op.div(op.sub(speed,minimum),op.sub(maximum,minimum)),0,1);
    s[10]=op.mul(op.add(op.sub(in.throttle,fraction),1),.5f);
    float blend=clamp(op.div(op.sub(s[3],s[4]),s[3]),0,1);
    if(!op.valid)return REFLEX_NUMERIC_RANGE;
    // Matches the tested VC90 _CIpow result after its binary32 store. Host libm
    // is NOT claimed equal to VC90 for every possible finite input/architecture.
    volatile double powered=std::pow(static_cast<double>(blend),.25);
    volatile float rounded=static_cast<float>(powered);blend=op.check(rounded);
    speed=clamp(lerp(op,op.div(s[5],literal(0x4118cccdu)),speed,blend),0,maximum);s[2]=speed;
    if(!op.valid)return REFLEX_NUMERIC_RANGE;
    const float ratio=in.ratios[in.gear];float drive,drag_curve;
    auto status=parameter(speed,in.curves[0],drive);if(status!=REFLEX_OK)return status;
    status=parameter(speed,in.curves[1],drag_curve);if(status!=REFLEX_OK)return status;
    float drag=neg(op,op.mul(drag_curve,op.mul(op.mul(s[6],ratio),s[7])));
    if(v[3][0]<in.g[0]&&v[3][0]<in.g[1]&&v[3][0]<in.g[2])drag=neg(op,drag);
    float root_torque=0,wheel_torque=0,couple=1;
    if(in.mode==0||in.mode==1){
        const float force=op.add(op.mul(op.sub(op.mul(drive,ratio),drag),in.throttle),drag);
        root_torque=neg(op,op.mul(in.reaction,force));wheel_torque=neg(op,force);
        couple=op.div(force>0?0:force,drag);const float matched=op.mul(ratio,v[3][0]);float inertia_x;
        if(in.mode==0){inertia_x=op.add(s[8],s[9]);s[2]=matched;}
        else{
            const float free_drag=neg(op,op.mul(drag_curve,s[7]));
            const float free_force=op.add(op.mul(op.sub(op.mul(op.mul(ratio,2),drive),free_drag),in.throttle),free_drag);
            const float free_root=neg(op,op.mul(free_force,in.reaction)),free_wheel=neg(op,free_force);
            const float free_speed=op.mul(op.add(op.div(op.mul(free_force,in.dt),s[9]),speed),s[6]);
            couple=lerp(op,1,couple,blend);wheel_torque=lerp(op,free_wheel,wheel_torque,blend);root_torque=lerp(op,free_root,root_torque,blend);
            s[2]=clamp(lerp(op,free_speed,matched,blend),0,maximum);inertia_x=s[8];
        }
        v[8]=op.plus({inertia_x,0,0,0},v[4]);for(unsigned i=0;i<4;++i)v[9][i]=op.div(in.numerator[i],v[8][i]);
    }else if(in.mode==2){
        const float free_drag=neg(op,op.mul(drag_curve,s[7]));
        const float force=op.add(op.mul(op.sub(drive,free_drag),in.throttle),free_drag);
        v[8]=op.plus({s[8],0,0,0},v[4]);for(unsigned i=0;i<4;++i)v[9][i]=op.div(in.numerator[i],v[8][i]);
        s[2]=op.add(op.div(op.mul(force,in.dt),s[9]),s[2]);
    }
    coupling(op,v[0],v[1],v[2],in.axis_y,s[11],s[12],couple,in.dt);s[2]=clamp(s[2],minimum,maximum);
    v[7]=op.plus({neg(op,wheel_torque),0,0,0},v[7]);v[6]=op.plus({root_torque,0,0,0},v[6]);
    if(in.brake>0){
        const float x=op.mul(v[10][0],v[10][0]),y=op.mul(v[10][1],v[10][1]),z=op.mul(v[10][2],v[10][2]);
        const float length=op.sqrt(op.add(op.add(x,z),y));const float factor=clamp(op.div(op.sub(length,2.5f),10),0,1);
        const Vec force={0,0,op.mul(op.sub(1,factor),-150),0};
        v[5]=op.plus(op.product(v[0],op.product(force,op.conjugate(v[0]))),v[5]);
    }
    if(!op.valid)return REFLEX_NUMERIC_RANGE;
    EngineOutput result;for(unsigned i=0;i<5;++i)result.vectors[i]=v[5+i];
    result.scalars[0]=s[2];for(unsigned i=1;i<4;++i)result.scalars[i]=s[9+i];output=result;return REFLEX_OK;
}
} // namespace bike_powertrain_draft

// Indexed/damping profile bit2 selects modeled x87 precision; bit1 selects the
// original equivalent bounded truncation branch. Engine profile is metadata.
// All host operations require nearest-even/no-flush/masked controls.
// This internal dispatcher snapshots complete inputs and publishes only success.
extern "C" uint32_t reflex_bike_powertrain_draft(uint32_t mode,const void* input,uint32_t input_size,void* output,uint32_t output_size){
    using namespace bike_powertrain_draft;using bike_force_draft::execute;
    switch(mode){
        case 1:return execute<IndexedInput,IndexedOutput>(input,input_size,output,output_size,indexed);
        case 2:return execute<DampingInput,DampingOutput>(input,input_size,output,output_size,damping);
        case 3:return execute<GearInput,GearOutput>(input,input_size,output,output_size,automatic_gear);
        case 4:return execute<GearInput,GearOutput>(input,input_size,output,output_size,down_gear);
        case 5:return execute<EngineInput,EngineOutput>(input,input_size,output,output_size,engine);
        default:return REFLEX_INVALID_INPUT;
    }
}
