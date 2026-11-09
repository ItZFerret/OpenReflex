// Internal draft only: original fixed85b8b0/rotor859f90 node-slot2 rows/bias.
// No public ABI, build registration or Unity binding. Raw graph/headers live
// in the mechanism-core validator; this normalized leaf snapshots its input.
#include "reflex_core.h"
#include "fp32.h"
#include <array>
#include <cstddef>
#include <cstring>

namespace node2_draft {
using Vec=std::array<float,4>;
using Mat3=std::array<Vec,3>;
struct Globals {
    Vec axis_x,axis_y,axis_z,g,sign_x,sign_y,sign_z,conjugate;
    uint32_t mask[4];float one,two,negative_two,negative_zero;
};
template<unsigned Locals,unsigned Rows> struct Input { Vec bodies[4][2],local[Locals],matrices[2][Rows];Globals globals; };
template<unsigned Rows,unsigned Bias> struct Output { Vec matrices[2][Rows],bias[Bias]; };
using FixedInput=Input<1,12>;using FixedOutput=Output<12,2>;
using RotorInput=Input<3,8>;using RotorOutput=Output<8,1>;
static_assert(sizeof(Globals)==160&&sizeof(FixedInput)==688&&sizeof(FixedOutput)==416,"fixed node2 draft layout");
static_assert(sizeof(RotorInput)==592&&sizeof(RotorOutput)==272,"rotor node2 draft layout");
static_assert(offsetof(FixedInput,globals)==528&&offsetof(RotorInput,globals)==432,"node2 globals offsets");

bool finite(const Vec& v){for(float x:v)if(!reflex_detail::finite(x))return false;return true;}
bool valid_globals(const Globals& g){
    if(!finite(g.axis_x)||!finite(g.axis_y)||!finite(g.axis_z)||!finite(g.g)||!finite(g.sign_x)||!finite(g.sign_y)||!finite(g.sign_z)||!finite(g.conjugate))return false;
    if(!reflex_detail::finite(g.one)||!reflex_detail::finite(g.two)||!reflex_detail::finite(g.negative_two)||!reflex_detail::finite(g.negative_zero))return false;
    return g.mask[0]==0xffffffffu&&g.mask[1]==0xffffffffu&&g.mask[2]==0xffffffffu&&g.mask[3]==0;
}
struct Arithmetic {
    const Globals& g;bool valid=true;
    float check(float value){valid=valid&&reflex_detail::finite(value);return value;}
    float add(float a,float b){return check(reflex_detail::add(a,b));}
    float sub(float a,float b){return check(reflex_detail::sub(a,b));}
    float mul(float a,float b){return check(reflex_detail::mul(a,b));}
    Vec neg(const Vec& row){Vec result;for(unsigned i=0;i<4;++i)result[i]=sub(0,row[i]);return result;}
    float dot(const Vec& p,const Vec& q){
        const float x=mul(p[0],q[0]),y=mul(p[1],q[1]),z=mul(p[2],q[2]);return add(add(x,y),z);
    }
    Vec transform(const Mat3& m,const Vec& v){
        const float x=dot(m[0],v),y=dot(m[1],v),z=dot(m[2],v);return {x,y,z,z};
    }
    Vec row_product(const Vec& p,const Mat3& q){
        const float x=dot(p,q[0]),y=dot(p,q[1]),z=dot(p,q[2]);return {x,y,z,z};
    }
    Mat3 quaternion_rows(const Vec& q,bool reverse=false){
        const float x=q[0],y=q[1],z=q[2],w=q[3];
        float xx,yy,zz,xy,xz,yz,wx,wy,wz;
        if(reverse){
            const float x2=mul(x,g.two),y2=mul(y,g.two),z2=mul(z,g.two);
            xx=mul(x,x2);yy=mul(y,y2);zz=mul(z,z2);
            xy=mul(y,x2);xz=mul(z,x2);yz=mul(z,y2);
            wx=mul(w,x2);wy=mul(w,y2);wz=mul(w,z2);
        }else{
            const float nx=mul(x,g.negative_two),ny=mul(y,g.negative_two),nz=mul(z,g.negative_two);
            xx=sub(g.negative_zero,mul(x,nx));yy=sub(g.negative_zero,mul(y,ny));zz=sub(g.negative_zero,mul(z,nz));
            xy=sub(g.negative_zero,mul(y,nx));xz=sub(g.negative_zero,mul(z,nx));yz=sub(g.negative_zero,mul(z,ny));
            wx=mul(w,nx);wy=mul(w,ny);wz=mul(w,nz);
        }
        Mat3 out{{{sub(g.one,add(zz,yy)),add(xy,wz),sub(xz,wy),0},
                  {sub(xy,wz),sub(g.one,add(zz,xx)),add(yz,wx),0},
                  {add(xz,wy),sub(yz,wx),sub(g.one,add(yy,xx)),0}}};
        for(Vec& row:out)row[3]=row[2];return out;
    }
    Mat3 skew(const Vec& v){
        const float nx=sub(0,v[0]),ny=sub(0,v[1]),nz=sub(0,v[2]);
        return {{{g.g[0],v[2],ny,ny},{nz,g.g[1],v[0],v[0]},{v[1],nx,g.g[2],g.g[3]}}};
    }
    Vec cross_reverse(const Vec& a,const Vec& b){
        // Original shuffled b cross a; the mask clears W after subtraction.
        return {sub(mul(a[2],b[1]),mul(a[1],b[2])),sub(mul(a[0],b[2]),mul(a[2],b[0])),sub(mul(a[1],b[0]),mul(a[0],b[1])),0};
    }
    Vec product(const Vec& a,const Vec& b,unsigned lanes=4){
        constexpr unsigned bx[4]={3,2,1,0},by[4]={2,3,0,1},bz[4]={1,0,3,2};Vec result{};
        for(unsigned i=0;i<lanes;++i){
            const float tz=mul(mul(a[2],b[bz[i]]),g.sign_z[i]),ty=mul(mul(a[1],b[by[i]]),g.sign_y[i]);
            const float tw=mul(a[3],b[i]),tx=mul(mul(a[0],b[bx[i]]),g.sign_x[i]);
            result[i]=add(add(tz,ty),add(tw,tx));
        }
        return result;
    }
    Vec rotate_xyz(const Vec& q,const Vec& v){
        Vec conjugate;for(unsigned i=0;i<4;++i)conjugate[i]=mul(q[i],g.conjugate[i]);
        const Vec masked={v[0],v[1],v[2],0};return product(q,product(masked,conjugate),3);
    }
};
Mat3 transpose3(const Mat3& m){
    // Original857c90 duplicates Z in rows0/1, retains original row2 W.
    return {{{m[0][0],m[1][0],m[2][0],m[2][0]},
             {m[0][1],m[1][1],m[2][1],m[2][1]},
             {m[0][2],m[1][2],m[2][2],m[2][3]}}};
}
template<unsigned L,unsigned R> bool valid_common(const Input<L,R>& in){
    if(!valid_globals(in.globals)||!finite(in.bodies[1][0])||!finite(in.bodies[1][1])||!finite(in.bodies[3][0]))return false;
    for(const Vec& local:in.local)if(!finite(local))return false;return true;
}
uint32_t fixed(const FixedInput& in,FixedOutput& output){
    // Original fixed body never reads the remaining five supplied body vec4s.
    if(!valid_common(in))return REFLEX_INVALID_INPUT;
    const Globals& g=in.globals;Arithmetic op{g};FixedOutput result{};
    const Mat3 rp=op.quaternion_rows(in.bodies[1][0]),own=op.quaternion_rows(in.bodies[3][0]),skew=op.skew(in.local[0]);
    const Vec axes[3]={g.axis_x,g.axis_y,g.axis_z};
    for(unsigned i=0;i<3;++i){
        result.matrices[0][i]=axes[i];result.matrices[0][i+3]=op.neg(op.row_product(rp[i],skew));
        result.matrices[0][i+6]=g.g;result.matrices[0][i+9]=op.neg(rp[i]);
        result.matrices[1][i]=op.neg(axes[i]);result.matrices[1][i+3]=g.g;result.matrices[1][i+6]=g.g;result.matrices[1][i+9]=own[i];
    }
    result.bias[0]=op.transform(rp,op.cross_reverse(op.cross_reverse(in.local[0],in.bodies[1][1]),in.bodies[1][1]));result.bias[1]=g.g;
    if(!op.valid)return REFLEX_NUMERIC_RANGE;output=result;return REFLEX_OK;
}
uint32_t rotor(const RotorInput& in,RotorOutput& output){
    if(!valid_common(in)||!finite(in.bodies[0][0])||!finite(in.bodies[0][1])||!finite(in.bodies[2][0])||!finite(in.bodies[2][1])||!finite(in.bodies[3][1]))return REFLEX_INVALID_INPUT;
    Arithmetic op{in.globals};RotorOutput result{};std::memcpy(result.matrices,in.matrices,sizeof(result.matrices));
    const Mat3 rp=op.quaternion_rows(in.bodies[1][0]),rpt=transpose3(rp),own_reverse=op.quaternion_rows(in.bodies[3][0],true);
    const Vec axes[2]={op.transform(rp,in.local[0]),op.transform(rp,in.local[1])};
    const Vec joint=op.product(in.bodies[1][0],in.local[2]);
    const Vec zy[2]={op.rotate_xyz(joint,in.globals.axis_z),op.rotate_xyz(joint,in.globals.axis_y)};
    Vec delta{};for(unsigned i=0;i<3;++i)delta[i]=op.sub(in.bodies[2][0][i],in.bodies[0][0][i]);
    const Mat3 skew=op.skew(op.transform(rpt,delta));
    for(unsigned i=0;i<2;++i){
        result.matrices[0][i]=axes[i];result.matrices[1][i]=op.neg(axes[i]);
        result.matrices[0][i+4]=op.neg(op.row_product(op.row_product(axes[i],rpt),skew));
        result.matrices[0][i+6]=op.neg(op.row_product(zy[i],rpt));
        // Original EDX at85a441 still addresses zy, not the prior product.
        result.matrices[1][i+6]=op.neg(op.row_product(zy[i],own_reverse));
    }
    const Vec omega=op.rotate_xyz(in.bodies[1][0],in.bodies[1][1]),own_omega=op.rotate_xyz(in.bodies[3][0],in.bodies[3][1]);
    Vec relative{},acceleration=op.cross_reverse(delta,omega);
    for(unsigned i=0;i<3;++i){
        relative[i]=op.sub(own_omega[i],omega[i]);
        acceleration[i]=op.sub(acceleration[i],op.mul(op.sub(in.bodies[2][1][i],in.bodies[0][1][i]),in.globals.two));
    }
    acceleration=op.cross_reverse(acceleration,omega);
    result.bias[0]={op.dot(acceleration,axes[0]),op.dot(acceleration,axes[1]),op.dot(op.cross_reverse(zy[0],omega),relative),op.dot(op.cross_reverse(zy[1],omega),relative)};
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
} // namespace node2_draft

// Full snapshot before computation permits aligned arbitrary input/output
// overlap; every failure leaves output unchanged. Finite contributing math
// only: dead temporary rows, discarded W and floating status are out of scope.
extern "C" uint32_t reflex_node2_draft(uint32_t mode,const void* input,uint32_t input_size,void* output,uint32_t output_size){
    using namespace node2_draft;
    switch(mode){
        case 1:return execute<FixedInput,FixedOutput>(input,input_size,output,output_size,fixed);
        case 2:return execute<RotorInput,RotorOutput>(input,input_size,output,output_size,rotor);
        default:return REFLEX_INVALID_INPUT;
    }
}
