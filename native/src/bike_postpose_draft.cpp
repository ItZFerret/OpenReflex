// Unregistered original43e200 steering/rear-anchor draft. Shared force draft
// is included unchanged once; no public ABI, CMake or Unity changes.
#include "bike_force_draft.cpp"
namespace bike_postpose_draft {
using bike_force_draft::Vec;using bike_force_draft::Arithmetic;
using bike_force_draft::finite;using bike_force_draft::literal;
struct Input { Vec vectors[6];float height,length;Vec steering,axis_x,g; };
struct Output { Vec vectors[4]; };
static_assert(sizeof(Input)==152&&sizeof(Output)==64&&offsetof(Input,steering)==104,"postpose raw record layout");
Vec normal(Arithmetic& op,const Vec& v){
    const float xx=op.mul(v[0],v[0]),yy=op.mul(v[1],v[1]),zz=op.mul(v[2],v[2]);
    const float length=op.sqrt(op.add(op.add(xx,yy),zz));Vec result{};
    if(length!=0)for(unsigned i=0;i<4;++i)result[i]=op.div(v[i],length);return result;
}
Vec cross_reverse(Arithmetic& op,const Vec& a,const Vec& b){
    Vec result{};constexpr unsigned x[3]={2,0,1},y[3]={1,2,0};
    for(unsigned i=0;i<3;++i)result[i]=op.sub(op.mul(a[x[i]],b[y[i]]),op.mul(a[y[i]],b[x[i]]));return result;
}
Vec basis_quaternion(Arithmetic& op,Vec first,const Vec& second,const Vec& third){
    const float xx0=op.mul(first[0],first[0]),yy0=op.mul(first[1],first[1]),zz0=op.mul(first[2],first[2]);
    const float total=op.add(op.add(yy0,xx0),zz0);
    if(total>0){const float reciprocal=op.div(1,op.sqrt(total));for(unsigned i=0;i<3;++i)first[i]=op.mul(reciprocal,first[i]);first[3]=0;}
    const float a=first[0],b=second[1],c=third[2];
    const float xx=op.mul(op.sub(op.sub(op.add(a,1),b),c),.25f);
    const float yy=op.mul(op.sub(op.add(op.sub(1,a),b),c),.25f);
    const float zz=op.mul(op.add(op.sub(op.sub(1,a),b),c),.25f);
    const float ww=op.mul(op.add(op.add(op.add(a,1),b),c),.25f);
    float best=xx>yy?xx:yy;if(best<=zz)best=zz;if(best<=ww)best=ww;
    if(best==ww){const float w=op.sqrt(ww),f=op.div(.25f,w);return {
        op.mul(op.sub(second[2],third[1]),f),op.mul(op.sub(third[0],first[2]),f),op.mul(op.sub(first[1],second[0]),f),w};}
    best=yy>zz?yy:zz;if(best<=xx)best=xx;
    if(best==xx){const float x=op.sqrt(xx),f=op.div(.25f,x);return {
        x,op.mul(op.add(second[0],first[1]),f),op.mul(op.add(third[0],first[2]),f),op.mul(op.sub(second[2],third[1]),f)};}
    if(yy>zz){const float y=op.sqrt(yy),f=op.div(.25f,y);return {
        op.mul(op.add(second[0],first[1]),f),y,op.mul(op.add(third[1],second[2]),f),op.mul(op.sub(third[0],first[2]),f)};}
    const float z=op.sqrt(zz),f=op.div(.25f,z);return {
        op.mul(op.add(third[0],first[2]),f),op.mul(op.add(third[1],second[2]),f),z,op.mul(op.sub(first[1],second[0]),f)};
}
uint32_t calculate(const Input& in,Output& output){
    for(const Vec& row:in.vectors)if(!finite(row))return REFLEX_INVALID_INPUT;
    if(!finite(in.steering)||!finite(in.axis_x)||!finite(in.g)||!reflex_detail::finite(in.height)||!reflex_detail::finite(in.length))return REFLEX_INVALID_INPUT;
    Arithmetic op;const Vec& q=in.vectors[2];Output result{};
    result.vectors[0]=op.product(in.vectors[5],in.steering);result.vectors[1]=op.product(q,result.vectors[0]);
    const Vec center=op.minus(in.vectors[1],in.vectors[0]);
    // Original SUBPS W is immediately AND-masked; discard it before arithmetic
    // validation so ±maximum finite positionW does not falsely reject.
    Vec delta=op.minus(in.vectors[4],in.vectors[3],3);
    const Vec local=op.product(op.conjugate(q),op.product(delta,q));delta=op.minus(local,center);delta[0]=in.g[0];
    const Vec unit=normal(op,delta);
    if(unit[2]>literal(0xbf333333u)){
        const bool positive=in.g[0]<unit[1]&&in.g[1]<unit[1]&&in.g[2]<unit[1];
        delta={0,positive?0.f:literal(0x80000000u),-1,-1};
    }
    const Vec scaled=op.scale(normal(op,delta),in.length);result.vectors[2]=op.plus(scaled,center);
    result.vectors[2][1]=op.sub(in.height,in.vectors[0][1]);
    const Vec axis=normal(op,{scaled[0],scaled[1],scaled[2],0}),zero{};
    const Vec third=op.minus(zero,axis),second=normal(op,cross_reverse(op,in.axis_x,third)),first=cross_reverse(op,third,second);
    result.vectors[3]=op.product(q,basis_quaternion(op,first,second,third));
    if(!op.valid)return REFLEX_NUMERIC_RANGE;output=result;return REFLEX_OK;
}
}
extern "C" uint32_t reflex_bike_postpose_draft(const void* input,uint32_t input_size,void* output,uint32_t output_size){
    return bike_force_draft::execute<bike_postpose_draft::Input,bike_postpose_draft::Output>(input,input_size,output,output_size,bike_postpose_draft::calculate);
}
