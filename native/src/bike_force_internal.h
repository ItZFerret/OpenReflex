// Internal draft only: no public ABI, CMake registration or Unity binding.
// Exact original 447cd0/85c770/438ee0/43c590 finite numerical building blocks.
#include "reflex_core.h"
#include "fp32.h"
#include <array>
#include <cstddef>
#include <cstring>

namespace bike_force_draft {
using Vec=std::array<float,4>;
struct TorqueInput { Vec root_orientation,front_orientation,root_torque,front_torque,rear_torque;float rear_control,front_control; };
struct TorqueOutput { Vec root_torque,front_torque,rear_torque; };
struct PointInput { Vec orientation,force,lever,linear,angular; };
struct PointOutput { Vec linear,angular; };
struct CurveInput { float x;uint32_t count;float knots[10],values[10],first_slope,last_slope; };
struct CurveOutput { float value; };
// Vector/scalar order is the pinned original record order in bike_force_model.py.
struct SuspensionInput { Vec vectors[20];float scalars[12];Vec g; };
struct SuspensionOutput { Vec vectors[8];uint32_t flags[4]; };
static_assert(sizeof(TorqueInput)==88&&sizeof(TorqueOutput)==48,"torque draft layout");
static_assert(sizeof(PointInput)==80&&sizeof(PointOutput)==32,"point draft layout");
static_assert(sizeof(CurveInput)==96&&sizeof(CurveOutput)==4,"curve draft layout");
static_assert(sizeof(SuspensionInput)==384&&sizeof(SuspensionOutput)==144,"suspension draft layout");
static_assert(offsetof(CurveInput,count)==4&&offsetof(SuspensionInput,scalars)==320&&offsetof(SuspensionOutput,flags)==128,"draft offsets");

inline float literal(uint32_t bits){float value;std::memcpy(&value,&bits,4);return value;}
bool finite(const Vec& row){for(float value:row)if(!reflex_detail::finite(value))return false;return true;}
struct Arithmetic {
    bool valid=true;
    float check(float value){valid=valid&&reflex_detail::finite(value);return value;}
    float add(float a,float b){return check(reflex_detail::add(a,b));}
    float sub(float a,float b){return check(reflex_detail::sub(a,b));}
    float mul(float a,float b){return check(reflex_detail::mul(a,b));}
    float div(float a,float b){return check(reflex_detail::div(a,b));}
    float sqrt(float a){return check(reflex_detail::sqrt(a));}
    Vec plus(const Vec& a,const Vec& b,unsigned lanes=4){Vec r{};for(unsigned j=0;j<lanes;++j)r[j]=add(a[j],b[j]);return r;}
    Vec minus(const Vec& a,const Vec& b,unsigned lanes=4){Vec r{};for(unsigned j=0;j<lanes;++j)r[j]=sub(a[j],b[j]);return r;}
    Vec scale(const Vec& a,float b){Vec r;for(unsigned j=0;j<4;++j)r[j]=mul(a[j],b);return r;}
    Vec conjugate(const Vec& q){return {mul(q[0],-1),mul(q[1],-1),mul(q[2],-1),mul(q[3],1)};}
    Vec product(const Vec& a,const Vec& b,unsigned lanes=4){
        constexpr unsigned maps[3][4]={{3,2,1,0},{2,3,0,1},{1,0,3,2}};
        constexpr float signs[3][4]={{1,-1,1,-1},{1,1,-1,-1},{-1,1,1,-1}};
        Vec result{};
        for(unsigned j=0;j<lanes;++j){
            const float tx=mul(mul(a[0],b[maps[0][j]]),signs[0][j]);
            const float ty=mul(mul(a[1],b[maps[1][j]]),signs[1][j]);
            const float tz=mul(mul(b[maps[2][j]],a[2]),signs[2][j]);
            const float tw=mul(a[3],b[j]);
            const float zy=add(tz,ty),wx=add(tw,tx);result[j]=add(zy,wx);
        }
        return result;
    }
    Vec rotate(const Vec& q,const Vec& v,const Vec& cq,bool forward,unsigned lanes=4){
        const Vec masked={v[0],v[1],v[2],0};
        return forward?product(q,product(masked,cq),lanes):product(cq,product(masked,q),lanes);
    }
    void point(const Vec& q,const Vec& force,const Vec& lever,Vec& linear,Vec& angular){
        const Vec masked={force[0],force[1],force[2],0};
        const Vec local=product(conjugate(q),product(masked,q),3);
        Vec torque{};constexpr unsigned a[3]={2,0,1},b[3]={1,2,0};
        for(unsigned j=0;j<3;++j)torque[j]=sub(mul(local[a[j]],lever[b[j]]),mul(local[b[j]],lever[a[j]]));
        linear=plus(linear,force);angular=plus(angular,torque);
    }
};

uint32_t torque(const TorqueInput& in,TorqueOutput& output){
    if(!finite(in.root_orientation)||!finite(in.front_orientation)||!finite(in.root_torque)||!finite(in.front_torque)||!finite(in.rear_torque)
       ||!reflex_detail::finite(in.rear_control)||!reflex_detail::finite(in.front_control))return REFLEX_INVALID_INPUT;
    Arithmetic op;TorqueOutput result{in.root_torque,in.front_torque,in.rear_torque};
    const Vec rear={in.rear_control,0,0,0},front={in.front_control,0,0,0},zero{};
    result.rear_torque=op.plus(result.rear_torque,rear);result.root_torque=op.plus(result.root_torque,op.minus(zero,rear));
    result.front_torque=op.plus(result.front_torque,front);Vec reaction=op.minus(zero,front);reaction[3]=0;
    const Vec world=op.product(in.front_orientation,op.product(reaction,op.conjugate(in.front_orientation)),3);
    const Vec local=op.product(op.conjugate(in.root_orientation),op.product(world,in.root_orientation));
    result.root_torque=op.plus(result.root_torque,local);
    if(!op.valid)return REFLEX_NUMERIC_RANGE;output=result;return REFLEX_OK;
}

uint32_t point(const PointInput& in,PointOutput& output){
    if(!finite(in.orientation)||!finite(in.force)||!finite(in.lever)||!finite(in.linear)||!finite(in.angular))return REFLEX_INVALID_INPUT;
    Arithmetic op;PointOutput result{in.linear,in.angular};op.point(in.orientation,in.force,in.lever,result.linear,result.angular);
    if(!op.valid)return REFLEX_NUMERIC_RANGE;output=result;return REFLEX_OK;
}

uint32_t curve(const CurveInput& in,CurveOutput& output){
    if(in.count<2||in.count>10||!reflex_detail::finite(in.x)||!reflex_detail::finite(in.first_slope)||!reflex_detail::finite(in.last_slope))return REFLEX_INVALID_INPUT;
    for(unsigned i=0;i<10;++i)if(!reflex_detail::finite(in.values[i]))return REFLEX_INVALID_INPUT;
    for(unsigned i=0;i<in.count;++i)if(!reflex_detail::finite(in.knots[i])||(i&&!(in.knots[i-1]<in.knots[i])))return REFLEX_INVALID_INPUT;
    Arithmetic op;int index=-1;float t=0;
    for(unsigned i=0;i+1<in.count;++i)if(in.knots[i]<=in.x&&in.x<=in.knots[i+1]){
        index=static_cast<int>(i);t=op.div(op.sub(in.x,in.knots[i]),op.sub(in.knots[i+1],in.knots[i]));
    }
    if(index<0){output.value=in.x<in.knots[0]?in.values[0]:in.values[in.count-1];return REFLEX_OK;}
    const float width=op.sub(in.knots[index+1],in.knots[index]),left=in.values[index],right=in.values[index+1];
    float previous,following;
    // At count2 the first-segment branch still reads value slot2.
    if(index==0){previous=op.sub(right,op.mul(op.mul(in.first_slope,width),2));following=in.values[2];}
    else{previous=in.values[index-1];following=static_cast<unsigned>(index)==in.count-2?op.add(op.mul(op.mul(in.last_slope,width),2),left):in.values[index+2];}
    const float next_half=op.mul(op.sub(following,left),.5f),current_half=op.mul(op.sub(right,previous),.5f);
    const float difference=op.sub(next_half,current_half);
    const float quadratic=op.sub(op.mul(op.sub(op.sub(right,left),current_half),3),difference);
    const float cubic=op.mul(op.sub(difference,op.mul(quadratic,2)),literal(0x3eaaaa9fu));
    const float value=op.add(op.mul(op.add(op.mul(op.add(op.mul(cubic,t),quadratic),t),current_half),t),left);
    if(!op.valid)return REFLEX_NUMERIC_RANGE;output.value=value;return REFLEX_OK;
}

uint32_t suspension(const SuspensionInput& in,SuspensionOutput& output){
    for(const Vec& row:in.vectors)if(!finite(row))return REFLEX_INVALID_INPUT;
    for(float value:in.scalars)if(!reflex_detail::finite(value))return REFLEX_INVALID_INPUT;
    if(!finite(in.g))return REFLEX_INVALID_INPUT;
    Arithmetic op;Vec state[20];std::memcpy(state,in.vectors,sizeof(state));SuspensionOutput result{};
    const Vec q=state[2],cq=op.conjugate(q),angular=op.rotate(q,state[3],cq,true,3),zero{};
    for(unsigned side=0;side<2;++side){
        const unsigned pos=side?7:4,vel=side?8:5,orientation=side?9:6,axis=side?13:11,anchor=side?12:10,linear=side?18:16,torque_index=side?19:17,s=side*6;
        const Vec direction=op.rotate(q,state[axis],cq,true);
        const Vec offset=op.minus(state[pos],state[0],3),local=op.rotate(q,offset,cq,false,3);
        Vec origin=state[anchor];if(side)origin[2]=origin[3]=literal(0xc0266666u);
        const Vec delta=op.minus(local,origin,3);
        const float xx=op.mul(delta[0],delta[0]),yy=op.mul(delta[1],delta[1]),zz=op.mul(delta[2],delta[2]);
        const float distance=op.sqrt(op.add(op.add(xx,zz),yy));
        const Vec elastic=op.scale(direction,op.mul(op.sub(distance,in.scalars[s]),in.scalars[s+3]));
        if(distance<in.scalars[s+1])result.flags[side*2]=1;
        else if(in.scalars[s+2]<distance)result.flags[side*2+1]=1;
        Vec cross{};constexpr unsigned a[3]={2,0,1},b[3]={1,2,0};
        for(unsigned j=0;j<3;++j)cross[j]=op.sub(op.mul(offset[a[j]],angular[b[j]]),op.mul(offset[b[j]],angular[a[j]]));
        const Vec relative=op.minus(state[vel],op.plus(cross,state[1],3),3);
        const float px=op.mul(relative[0],direction[0]),py=op.mul(relative[1],direction[1]),pz=op.mul(relative[2],direction[2]);
        const float speed=op.add(op.add(px,py),pz);
        const float damping=in.scalars[s+((speed<in.g[0]&&speed<in.g[1]&&speed<in.g[2])?5:4)];
        const Vec viscous=op.scale(direction,op.mul(op.sub(0,speed),damping));
        const Vec force=op.plus(viscous,elastic),reaction=op.minus(zero,force);
        op.point(state[orientation],force,in.g,state[linear],state[torque_index]);
        op.point(q,reaction,local,state[14],state[15]);
        if(!side){result.vectors[6]=op.rotate(q,viscous,cq,false);result.vectors[7]=op.rotate(q,elastic,cq,false);}
    }
    for(unsigned i=0;i<6;++i)result.vectors[i]=state[14+i];
    if(!op.valid)return REFLEX_NUMERIC_RANGE;output=result;return REFLEX_OK;
}

template<class In,class Out> uint32_t execute(const void* input,uint32_t input_size,void* output,uint32_t output_size,uint32_t (*operation)(const In&,Out&)){
    if(input_size!=sizeof(In)||output_size!=sizeof(Out))return REFLEX_ABI_MISMATCH;
    if(!input||!output)return REFLEX_NULL_ARGUMENT;
    if(reinterpret_cast<uintptr_t>(input)%4||reinterpret_cast<uintptr_t>(output)%4)return REFLEX_INVALID_INPUT;
    if(!reflex_detail::supported_fp())return REFLEX_UNSUPPORTED_FP;
    In snapshot;std::memcpy(&snapshot,input,sizeof(snapshot));Out result;
    const auto status=operation(snapshot,result);if(status==REFLEX_OK)std::memcpy(output,&result,sizeof(result));return status;
}
} // namespace bike_force_draft

