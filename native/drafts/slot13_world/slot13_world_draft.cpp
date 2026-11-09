// Private slot13 world draft: original slot13 (00444b90) including static
// triangle-object contact (85ddd0 -> 8716b0 / 86fac0), interpreted from the
// pinned original instruction closure over a supplied world snapshot at its own
// addresses. Not a registered engine ABI. Instruction semantics are shared
// verbatim with the terrain-only slot13 draft (see slot13_draft.cpp).
#include "../../src/fp32.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace reflex_slot13_world_private {
using Wide=unsigned __int128;
constexpr uint32_t Stack=0x70000000,Lock=0x7f000000,Unlock=0x7f000010,SurfaceTable=0x7f001000,DeformBits=0x60000000;
constexpr uint32_t WorldStart=0x00400000,WorldEnd=0x20000000,ImageEnd=0x00e3b000;
constexpr size_t StateSize=0x9700,GridBytes=16777216,OutputSize=StateSize+108+8;
static const uint32_t FilterOffsets[9]={0x8b40,0x8b44,0x8b48,0x8e40,0x8e44,0x8e48,0x8e4c,0x8e50,0x8e54};
// Read-only image words outside the closure's pinned constants (vtable slots).
static const uint32_t VtableWords[3][2]={{0x93c8a4+128,0x44ef70},{0x93cb9c+24,0x85a8f0},{0x93cbc8+24,0x85b750}};
struct Operand { int kind=0,size=0,reg=-1,base=-1,index=-1,scale=0,displacement=0;uint64_t immediate=0; };
struct Instruction {uint32_t address;int size,opcode,count;Operand o[3];};
struct Constant {uint32_t address;size_t size;uint8_t bytes[64];}; // switch tables need up to 31 bytes
#include "program.inc"

static Wide mask(int n){return n==16?~Wide(0):(Wide(1)<<(n*8))-1;}
static float f32(uint32_t b){float v;std::memcpy(&v,&b,4);return v;}
static double f64(uint64_t b){double v;std::memcpy(&v,&b,8);return v;}
static uint32_t bits32(float v){uint32_t b;std::memcpy(&b,&v,4);return b;}
static uint64_t bits64(double v){uint64_t b;std::memcpy(&b,&v,8);return b;}
static bool nan32(uint32_t v){return (v&0x7fffffff)>0x7f800000;}
static bool nan64(uint64_t v){return (v&0x7fffffffffffffffULL)>0x7ff0000000000000ULL;}
static uint32_t arithmetic32(int operation,uint32_t a,uint32_t b){
    if(nan32(a))return a|0x400000;if(nan32(b))return b|0x400000;
    volatile float x=f32(a),y=f32(b),r;
    if(operation==0)r=x+y;else if(operation==1)r=x-y;else if(operation==2)r=x*y;else r=x/y;
    uint32_t out=bits32(r);return nan32(out)?0xffc00000u:out;
}
static uint64_t arithmetic64(int operation,uint64_t a,uint64_t b){
    if(nan64(a))return a|0x8000000000000ULL;if(nan64(b))return b|0x8000000000000ULL;
    volatile double x=f64(a),y=f64(b),r;
    if(operation==0)r=x+y;else if(operation==1)r=x-y;else if(operation==2)r=x*y;else r=x/y;
    uint64_t out=bits64(r);return nan64(out)?0xfff8000000000000ULL:out;
}
static uint32_t square_root(uint32_t a){
    if(nan32(a))return a|0x400000;if(a&0x80000000u && a!=0x80000000u)return 0xffc00000u;
    volatile float x=f32(a),r=std::sqrt(x);return bits32(r);
}
static int compare(uint32_t a,uint32_t b){
    if(nan32(a)||nan32(b))return 2;const float x=f32(a),y=f32(b);return x<y?-1:x>y?1:0;
}

// Exact significand addition for the bounded x87 indexed-table closure. A
// binary64 addition followed by a float cast can double-round near a tie.
// The supplied operands have a small exponent gap; integer alignment fits128.
static double add_precision(double left,double right,unsigned precision){
    if(!std::isfinite(left)||!std::isfinite(right))throw std::runtime_error("nonfinite x87 sum");
    const uint64_t a=bits64(left),b=bits64(right),fraction=(1ULL<<52)-1;
    auto exponent=[](uint64_t v){unsigned e=unsigned((v>>52)&2047);return e?int(e)-1075:-1074;};
    auto significand=[](uint64_t v){return (v&fraction)|((v&0x7ff0000000000000ULL)?1ULL<<52:0);};
    uint64_t ma=significand(a),mb=significand(b);int ea=exponent(a),eb=exponent(b);
    if(!ma)ea=eb;if(!mb)eb=ea;const int common=ea<eb?ea:eb;
    if(ea-common>70||eb-common>70)throw std::runtime_error("x87 alignment outside bounded table domain");
    __int128 ia=__int128(Wide(ma)<<(ea-common)),ib=__int128(Wide(mb)<<(eb-common));
    if(a>>63)ia=-ia;if(b>>63)ib=-ib;const __int128 sum=ia+ib;
    if(!sum)return (a&b)&0x8000000000000000ULL?-0.0:0.0;
    const bool negative=sum<0;Wide magnitude=negative?Wide(-sum):Wide(sum);
    const uint64_t high=uint64_t(magnitude>>64),low=uint64_t(magnitude);
    const unsigned width=high?128-unsigned(__builtin_clzll(high)):64-unsigned(__builtin_clzll(low));
    const unsigned shift=width>precision?width-precision:0;Wide rounded=magnitude>>shift;
    if(shift){const Wide remainder=magnitude&((Wide(1)<<shift)-1),half=Wide(1)<<(shift-1);rounded+=remainder>half||(remainder==half&&(rounded&1));}
    double result=std::ldexp(double(uint64_t(rounded)),common+int(shift));return negative?-result:result;
}


struct Region {uint32_t start;std::vector<uint8_t> data;const uint8_t* external=nullptr;size_t external_size=0;
    size_t size()const{return external?external_size:data.size();}
    uint8_t* bytes(){return data.data();}};
struct Memory {
    std::vector<Region> regions;std::vector<std::pair<uint32_t,uint32_t>> writable;
    const Region* find(uint32_t a,size_t n)const{
        auto it=std::upper_bound(regions.begin(),regions.end(),a,[](uint32_t x,const Region& r){return x<r.start;});
        if(it==regions.begin())return nullptr;--it;return uint64_t(a)+n<=uint64_t(it->start)+it->size()?&*it:nullptr;
    }
    void add(uint32_t start,const uint8_t* p,size_t n){
        if(!n)return;
        if(auto* r=const_cast<Region*>(find(start,n))){
            if(r->external)throw std::runtime_error("overlay onto external grid");
            std::memcpy(r->bytes()+(start-r->start),p,n);return;
        }
        uint64_t lo=start,hi=uint64_t(start)+n;std::vector<Region> keep,merge;
        for(auto& r:regions){if(r.start<hi&&lo<uint64_t(r.start)+r.size()){if(r.external)throw std::runtime_error("overlap with external grid");merge.push_back(std::move(r));}else keep.push_back(std::move(r));}
        for(auto& r:merge){lo=std::min<uint64_t>(lo,r.start);hi=std::max<uint64_t>(hi,uint64_t(r.start)+r.size());}
        Region out{uint32_t(lo),std::vector<uint8_t>(size_t(hi-lo))};
        for(auto& r:merge)std::memcpy(out.data.data()+(r.start-lo),r.data.data(),r.data.size());
        std::memcpy(out.data.data()+(start-lo),p,n);
        keep.push_back(std::move(out));regions=std::move(keep);
        std::sort(regions.begin(),regions.end(),[](const Region& x,const Region& y){return x.start<y.start;});
    }
    // The supplied grid replaces any captured bytes in its range (pointer-chase
    // windows can include grid samples); bytes outside the grid are kept.
    void external(uint32_t start,const uint8_t* p,size_t n){
        const uint64_t end=uint64_t(start)+n;std::vector<Region> keep;
        for(auto& r:regions){
            const uint64_t rs=r.start,re=rs+r.size();
            if(re<=start||end<=rs){keep.push_back(std::move(r));continue;}
            if(r.external)throw std::runtime_error("external grids overlap");
            if(rs<start)keep.push_back(Region{r.start,std::vector<uint8_t>(r.data.begin(),r.data.begin()+(start-rs))});
            if(end<re)keep.push_back(Region{uint32_t(end),std::vector<uint8_t>(r.data.begin()+(end-rs),r.data.end())});
        }
        regions=std::move(keep);
        Region r{start,{}};r.external=p;r.external_size=n;regions.push_back(std::move(r));
        std::sort(regions.begin(),regions.end(),[](const Region& x,const Region& y){return x.start<y.start;});
    }
    void allow(uint32_t start,uint32_t n){writable.push_back({start,start+n});}
    uint8_t byte(uint32_t a)const{const Region* r=find(a,1);if(!r)throw std::runtime_error("unmapped world read "+std::to_string(a));return r->external?r->external[a-r->start]:r->data[a-r->start];}
    Wide read(uint32_t a,int n)const{
        const Region* r=find(a,size_t(n));if(!r)throw std::runtime_error("unmapped world read "+std::to_string(a));
        const uint8_t* p=(r->external?r->external:r->data.data())+(a-r->start);Wide out=0;for(int i=0;i<n;i++)out|=Wide(p[i])<<(8*i);return out;
    }
    void write(uint32_t a,int n,Wide v){
        bool ok=false;for(auto& w:writable)if(a>=w.first&&uint64_t(a)+n<=w.second){ok=true;break;}
        Region* r=const_cast<Region*>(find(a,size_t(n)));if(!ok||!r||r->external)throw std::runtime_error("unowned world write "+std::to_string(a));
        for(int i=0;i<n;i++)r->data[a-r->start+i]=uint8_t(v>>(8*i));
    }
};
struct Flags {bool z=false,c=false,s=false,o=false,p=false;};
struct Machine {
    Memory mem;std::array<Wide,16> r{};std::vector<double> x87;uint32_t pc=0,profile=0;Flags flags;
    uint64_t executed=0;unsigned locks=0,unlocks=0;bool locked=false;uint32_t stride=0,resource=0,sim=0;std::set<uint32_t> float_stores;
    Machine(const uint8_t* world,size_t world_size,const uint8_t* base,const uint8_t* deform,uint32_t dt,uint32_t x87_cw){
        std::vector<std::pair<uint32_t,uint32_t>> image_data;
        for(size_t at=0;at<world_size;){
            if(at+8>world_size)throw std::runtime_error("truncated world header");
            uint32_t a,n;std::memcpy(&a,world+at,4);std::memcpy(&n,world+at+4,4);at+=8;
            if(n>world_size-at)throw std::runtime_error("truncated world region");
            if(!sim){if(n!=StateSize)throw std::runtime_error("first world region must be the simulation");sim=a;}
            if(a>=WorldStart&&uint64_t(a)+n<=WorldEnd){mem.add(a,world+at,n);if(uint64_t(a)+n<=ImageEnd)image_data.push_back({a,n});}
            at+=n;
        }
        for(const auto& c:constants){if(!mem.find(c.address,c.size))mem.add(c.address,c.bytes,c.size);}
        for(auto& v:VtableWords)mem.add(v[0],reinterpret_cast<const uint8_t*>(&v[1]),4);
        mem.allow(sim,StateSize);
        for(uint32_t off:FilterOffsets)mem.allow(uint32_t(mem.read(sim+off,4)),12);
        for(auto& d:image_data)mem.allow(d.first,d.second);
        std::vector<uint8_t> stack(0x10000);mem.add(Stack,stack.data(),stack.size());mem.allow(Stack,0x10000);
        uint32_t registry[3];for(int i=0;i<3;i++)registry[i]=uint32_t(mem.read(0x9e30d4+4*i,4));
        uint32_t scene=uint32_t(mem.read(0xa95844,4)),identity=uint32_t(mem.read(scene+0x1c+uint32_t(mem.read(scene+4,4))*4,4)),terrain=0;
        for(uint32_t i=0;i<registry[2]&&i<2;i++)if(registry[i]&&uint32_t(mem.read(registry[i]+0x44a8,4))==identity)terrain=registry[i];
        if(!terrain)throw std::runtime_error("selected terrain missing");
        resource=uint32_t(mem.read(terrain+0x240,4));stride=uint32_t(mem.read(terrain+0x44bc,4));
        if(stride!=2048||uint32_t(mem.read(terrain+0x44c0,4))!=2048)throw std::runtime_error("world draft expects the 2048x2048 grid");
        mem.external(uint32_t(mem.read(terrain+0x1c,4)),base,GridBytes);mem.external(DeformBits,deform,GridBytes);
        std::array<uint8_t,60> table{};std::memcpy(table.data()+52,&Lock,4);std::memcpy(table.data()+56,&Unlock,4);mem.add(SurfaceTable,table.data(),table.size());
        mem.add(resource,reinterpret_cast<const uint8_t*>(&SurfaceTable),4);
        profile=(x87_cw==0x027f?2u:0u)|(uint32_t(mem.read(0xdcf760,4))&1u);
        if(x87_cw!=0x027f&&x87_cw!=0x007f)throw std::runtime_error("unsupported x87 control");
        r[2]=sim;r[7]=Stack+0x8000;mem.write(uint32_t(r[7]),4,0);mem.write(uint32_t(r[7])+4,4,dt);pc=0x444b90;
    }
    uint32_t address(const Operand& o)const{return uint32_t((o.base<0?0:uint32_t(r[o.base]))+(o.index<0?0:uint32_t(r[o.index]))*o.scale+o.displacement);}
    Wide get(const Operand& o,int size=0)const{
        const int n=size?size:o.size;return (o.kind==0?r[o.reg]:o.kind==1?Wide(o.immediate):mem.read(address(o),n))&mask(n);
    }
    void put(const Operand& o,Wide v,int size=0){
        const int n=size?size:o.size;v&=mask(n);
        if(o.kind==0)r[o.reg]=(o.reg>=8||n<4)?(r[o.reg]&~mask(n))|v:v;
        else if(o.kind==2)mem.write(address(o),n,v);else throw std::runtime_error("writing immediate");
    }
    void integer_flags(uint32_t a,uint32_t b,uint64_t result,bool subtraction=false){
        const uint32_t v=uint32_t(result);flags.z=v==0;flags.c=subtraction?a<b:result>0xffffffffULL;flags.s=bool(v&0x80000000);
        flags.o=bool((subtraction?(a^b)&(a^v):~(a^b)&(a^v))&0x80000000);flags.p=false;
    }
    bool taken(int opcode)const {
        switch(opcode){case OP_je:return flags.z;case OP_jne:return !flags.z;case OP_jb:return flags.c;
        case OP_jbe:return flags.c||flags.z;case OP_jae:return !flags.c;case OP_jl:return flags.s!=flags.o;
        case OP_jle:return flags.z||flags.s!=flags.o;case OP_ja:return !flags.c&&!flags.z;case OP_jg:return !flags.z&&flags.s==flags.o;case OP_jns:return !flags.s;case OP_jge:return flags.s==flags.o;case OP_jmp:return true;default:throw std::runtime_error("bad branch");}
    }
    bool hook(uint32_t target);
    void step(const Instruction& i){
        const auto& a=i.o[0];const auto& b=i.o[1];const auto& c=i.o[2];const int op=i.opcode;
        if((op==OP_movaps||op==OP_movups||op==OP_movss)&&a.kind==2){uint32_t at=address(a);if(!(at>=Stack&&at<Stack+0x10000))for(int j=0;j<(op==OP_movss?4:16);j+=4)float_stores.insert(at+j);}
        switch(op){
        case OP_cmpnless:{int order=compare(uint32_t(get(a,4)),uint32_t(get(b,4)));put(a,order==1||order==2?0xffffffff:0,4);return;}
        case OP_unpcklps:{Wide x=get(a),y=get(b);put(a,Wide(uint32_t(x))|(Wide(uint32_t(y))<<32)|(Wide(uint32_t(x>>32))<<64)|(Wide(uint32_t(y>>32))<<96));return;}
        case OP_movq:{Wide value=get(b,8);if(a.kind==0)r[a.reg]=value;else put(a,value,8);return;}
        case OP_imul:{int64_t value;if(i.count==1){value=int64_t(int32_t(uint32_t(r[0])))*int32_t(uint32_t(get(a)));r[0]=uint32_t(value);r[3]=uint32_t(uint64_t(value)>>32);}else{value=int64_t(int32_t(uint32_t(get(a))))*int32_t(uint32_t(get(b)));put(a,uint32_t(value));}flags.c=flags.o=value<INT32_MIN||value>INT32_MAX;return;}
        case OP_sar:case OP_shr:{uint32_t v=uint32_t(get(a)),count=uint32_t(get(b))&31;if(count){flags.c=v&(1u<<(count-1));flags.o=op==OP_shr&&bool(v&0x80000000);v=op==OP_sar?uint32_t(int32_t(v)>>count):v>>count;put(a,v);flags.z=v==0;flags.s=v>>31;flags.p=__builtin_parity(v&255)==0;}return;}
        case OP_sete:put(a,flags.z?1:0);return;
        case OP_mov:case OP_movaps:case OP_movapd:case OP_movups:put(a,get(b));return;
        case OP_movss:case OP_movsd:{int n=op==OP_movss?4:8;Wide v=get(b,n);if(a.kind==0&&b.kind==2)r[a.reg]=v;else put(a,v,n);return;}
        case OP_lea:put(a,address(b));return;
        case OP_push:{Wide v=get(a,4);r[7]-=4;mem.write(uint32_t(r[7]),4,v);return;}
        case OP_pop:{Wide v=mem.read(uint32_t(r[7]),4);r[7]+=4;put(a,v,4);return;}
        case OP_call:{uint32_t target=a.kind==2?address(a):uint32_t(get(a,4));if(hook(target))return;r[7]-=4;mem.write(uint32_t(r[7]),4,pc);pc=target;return;}
        case OP_ret:pc=uint32_t(mem.read(uint32_t(r[7]),4));r[7]+=4+(i.count?uint32_t(get(a)):0);return;
        case OP_je:case OP_jne:case OP_jb:case OP_jbe:case OP_jae:case OP_jl:case OP_jge:case OP_jle:case OP_jns:case OP_ja:case OP_jg:case OP_jmp:if(taken(op))pc=uint32_t(get(a,4));return;
        case OP_movzx:put(a,get(b),4);return;
        case OP_dec:{const uint32_t x=uint32_t(get(a));const bool carry=flags.c;integer_flags(x,1,uint64_t(x)-1,true);flags.c=carry;put(a,x-1);return;}
        case OP_add:case OP_sub:case OP_and:case OP_or:case OP_xor:case OP_cmp:case OP_test:case OP_inc:case OP_shl:{
            uint32_t x=uint32_t(get(a)),y=op==OP_inc?1:uint32_t(get(b));uint64_t value=0;
            if(op==OP_add||op==OP_inc)value=uint64_t(x)+y;else if(op==OP_sub||op==OP_cmp)value=uint64_t(x)-y;
            else if(op==OP_and||op==OP_test)value=x&y;else if(op==OP_or)value=x|y;else if(op==OP_xor)value=x^y;else value=uint64_t(x)<<(y&31);
            const bool carry=flags.c;integer_flags(x,y,value,op==OP_sub||op==OP_cmp);if(op==OP_inc)flags.c=carry;
            if(op==OP_and||op==OP_or||op==OP_xor||op==OP_test)flags.c=flags.o=false;
            if(op!=OP_cmp&&op!=OP_test)put(a,value);return;}
        case OP_comiss:{int c=compare(uint32_t(get(a,4)),uint32_t(get(b,4)));flags={c==0||c==2,c==-1||c==2,false,false,c==2};return;}
        case OP_andps:case OP_andnps:case OP_orps:case OP_xorps:{Wide x=get(a),y=get(b);put(a,op==OP_andps?x&y:op==OP_andnps?~x&y:op==OP_orps?x|y:x^y);return;}
        case OP_shufps:{Wide x=get(a),y=get(b),result=0;unsigned imm=unsigned(get(c));for(int j=0;j<4;j++)result|=Wide(uint32_t((j<2?x:y)>>(32*((imm>>(2*j))&3))))<<(32*j);put(a,result);return;}
        case OP_cmpltps:case OP_cmpleps:case OP_cmpneqps:case OP_minps:case OP_maxps:{
            Wide x=get(a),y=get(b),result=0;for(int j=0;j<4;j++){uint32_t left=uint32_t(x>>(j*32)),right=uint32_t(y>>(j*32)),out;int c=compare(left,right);
                if(op==OP_minps||op==OP_maxps)out=c==(op==OP_minps?-1:1)?left:right;
                else out=(op==OP_cmpltps?c==-1:op==OP_cmpleps?c==-1||c==0:c!=0)?0xffffffff:0;
                result|=Wide(out)<<(j*32);}put(a,result);return;}
        case OP_addps:case OP_subps:case OP_mulps:case OP_divps:case OP_addss:case OP_subss:case OP_mulss:case OP_divss:case OP_addsd:case OP_subsd:case OP_mulsd:case OP_divsd:{
            bool wide=op==OP_addsd||op==OP_subsd||op==OP_mulsd||op==OP_divsd;
            int lanes=op==OP_addps||op==OP_subps||op==OP_mulps||op==OP_divps?4:1,bits=wide?64:32,size=bits*lanes/8;
            int operation=op==OP_addps||op==OP_addss||op==OP_addsd?0:op==OP_subps||op==OP_subss||op==OP_subsd?1:op==OP_mulps||op==OP_mulss||op==OP_mulsd?2:3;
            Wide x=get(a,size),y=get(b,size),result=0;
            for(int j=0;j<lanes;j++)result|=(wide?Wide(arithmetic64(operation,uint64_t(x>>(j*bits)),uint64_t(y>>(j*bits)))):Wide(arithmetic32(operation,uint32_t(x>>(j*bits)),uint32_t(y>>(j*bits)))))<<(j*bits);
            put(a,result,size);return;}
        case OP_sqrtps:case OP_sqrtss:{int lanes=op==OP_sqrtps?4:1;Wide value=get(b,lanes*4),result=0;for(int j=0;j<lanes;j++)result|=Wide(square_root(uint32_t(value>>(j*32))))<<(j*32);put(a,result,lanes*4);return;}
        case OP_cvtss2sd:{volatile double value=double(f32(uint32_t(get(b,4))));put(a,bits64(value),8);return;}
        case OP_cvtsd2ss:{volatile float value=float(f64(uint64_t(get(b,8))));put(a,bits32(value),4);return;}
        case OP_cvtpd2ps:{Wide value=get(b,16),result=0;for(int j=0;j<2;j++){volatile float v=float(f64(uint64_t(value>>(j*64))));result|=Wide(bits32(v))<<(j*32);}put(a,result,16);return;}
        case OP_cvtps2pd:{Wide value=get(b,8),result=0;for(int j=0;j<2;j++){volatile double v=double(f32(uint32_t(value>>(j*32))));result|=Wide(bits64(v))<<(j*64);}put(a,result,16);return;}
        case OP_cvtsi2ss:{volatile float v=float(int32_t(uint32_t(get(b,4))));put(a,bits32(v),4);return;}
        case OP_cvttss2si:{float v=f32(uint32_t(get(b,4)));if(!std::isfinite(v)||v<-2147483648.0f||v>=2147483648.0f)throw std::runtime_error("int32 range");put(a,uint32_t(int32_t(v)),4);return;}
        case OP_fld:x87.push_back(a.size==4?double(f32(uint32_t(get(a)))):f64(uint64_t(get(a))));return;
        case OP_fld1:x87.push_back(1);return;
        case OP_fstp:{if(x87.empty())throw std::runtime_error("x87 empty");double value=x87.back();x87.pop_back();if(a.size==4){volatile float v=float(value);put(a,bits32(v));}else put(a,bits64(value));return;}
        case OP_fadd:{if(x87.empty())throw std::runtime_error("x87 empty");double operand=a.size==4?double(f32(uint32_t(get(a)))):f64(uint64_t(get(a)));x87.back()=add_precision(x87.back(),operand,profile&2?53:24);return;}
        default:throw std::runtime_error("unsupported instruction");
        }
    }
    void run(){
        static const auto lookup=[](){std::unordered_map<uint32_t,const Instruction*> map;for(const auto& i:program)map[i.address]=&i;return map;}();
        while(pc){if(++executed>3000000)throw std::runtime_error("instruction budget");auto found=lookup.find(pc);if(found==lookup.end())throw std::runtime_error("PC outside closure "+std::to_string(pc));const auto& i=*found->second;pc+=i.size;try{step(i);}catch(const std::exception& error){throw std::runtime_error(std::to_string(i.address)+": "+error.what());}}
    }
};
static const std::set<uint32_t> ExternalPairs={0x861a60,0x861ba0,0x862da0,0x863f70,0x864460,0x864dd0,0x864ec0,0x8650c0,0x8656c0,0x8657d0,0x867ca0,0x869d40,0x86a1e0,0x86b360,0x86b4d0,0x86b7f0,0x86bd80,0x86e090,0x86e750,0x86f150,0x86f2a0,0x86f3d0,0x872860,0x872970,0x872c90,0x873010,0x8732d0};
bool Machine::hook(uint32_t target){
    const uint32_t sp=uint32_t(r[7]);
    if(ExternalPairs.count(target))throw std::runtime_error("shape pair outside recovered Triangles contacts");
    if(target==Lock){uint32_t self=uint32_t(mem.read(sp,4)),rect=uint32_t(mem.read(sp+4,4)),region=uint32_t(mem.read(sp+8,4)),f=uint32_t(mem.read(sp+12,4));
        if(self!=resource||region||f!=0x810||locked)throw std::runtime_error("LockRect ABI");locks++;locked=true;mem.allow(rect,8);mem.write(rect,4,stride*4);mem.write(rect+4,4,DeformBits);r[0]=0;r[7]+=16;return true;}
    if(target==Unlock){if(uint32_t(mem.read(sp,4))!=resource||!locked)throw std::runtime_error("Unlock ABI");unlocks++;locked=false;r[7]+=4;r[0]=0;return true;}
    if(target==0x8f9efe){if(uint32_t(r[2])!=uint32_t(mem.read(0x9b45e4,4)))throw std::runtime_error("cookie");integer_flags(uint32_t(r[2]),uint32_t(r[2]),0,true);return true;}
    if(target==0x916230){uint64_t a=uint64_t(mem.read(sp,8)),b=uint64_t(mem.read(sp+8,8));x87.push_back(f64((a&0x7fffffffffffffffULL)|(b&0x8000000000000000ULL)));return true;}
    return false;
}

static thread_local std::string LastError;
struct EnvironmentGuard {std::fenv_t saved{};EnvironmentGuard(){if(std::fegetenv(&saved))throw std::runtime_error("save FP environment");}~EnvironmentGuard(){std::fesetenv(&saved);}};
// Output: final simulation (0x9700), nine 12-byte filters in FilterOffsets order, locks, unlocks.
extern "C" int reflex_slot13_world_draft(const void* world,size_t world_bytes,const void* base,size_t base_bytes,const void* deform,size_t deform_bytes,
                                         uint32_t dt_bits,uint32_t x87_cw,void* output,size_t output_bytes){
    if(!world||!base||!deform||!output||base_bytes!=GridBytes||deform_bytes!=GridBytes||output_bytes!=OutputSize)return 1;
    if(!reflex_detail::supported_fp())return 2;
    try{
        EnvironmentGuard environment;
        Machine m(static_cast<const uint8_t*>(world),world_bytes,static_cast<const uint8_t*>(base),static_cast<const uint8_t*>(deform),dt_bits,x87_cw);
        m.run();if(m.locked||!m.x87.empty())throw std::runtime_error("unbalanced resource/x87");
        std::vector<uint8_t> out(OutputSize);size_t cursor=0;
        for(size_t i=0;i<StateSize;i++)out[cursor++]=m.mem.byte(m.sim+uint32_t(i));
        for(uint32_t off:FilterOffsets){uint32_t p=uint32_t(m.mem.read(m.sim+off,4));for(unsigned i=0;i<12;i++)out[cursor++]=m.mem.byte(p+i);}
        std::memcpy(out.data()+cursor,&m.locks,4);std::memcpy(out.data()+cursor+4,&m.unlocks,4);
        std::memcpy(output,out.data(),OutputSize);return 0;
    }catch(const std::exception& e){LastError=e.what();return 2;}
}
extern "C" const char* reflex_slot13_world_draft_error(){return LastError.c_str();}
} // namespace reflex_slot13_world_private

#ifndef REFLEX_SLOT13_WORLD_NO_MAIN
static std::vector<uint8_t> slurp(const char* path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error(std::string("open ")+path);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){
    using namespace reflex_slot13_world_private;
    if(argc!=7){std::fprintf(stderr,"usage: world.bin base.bin deform.bin dt_hex x87cw_hex output.bin\n");return 2;}
    try{
        auto world=slurp(argv[1]),base=slurp(argv[2]),deform=slurp(argv[3]);std::vector<uint8_t> out(OutputSize);
        if(!std::strncmp(argv[4],"cases=",6)){
            // Generated cases <u32 dt, 0x9700 simulation>: replace the world's first
            // (simulation) region per case; outputs are concatenated.
            auto cases=slurp(argv[4]+6);if(cases.size()%(4+StateSize))throw std::runtime_error("case file size");
            std::ofstream o(argv[6],std::ios::binary);
            for(size_t c=0;c<cases.size();c+=4+StateSize){
                std::vector<uint8_t> w=world;uint32_t dt;std::memcpy(&dt,cases.data()+c,4);
                const uint8_t* state=cases.data()+c+4;uint32_t sim;std::memcpy(&sim,w.data(),4);
                // Apply the case state to the simulation and to every later captured
                // window overlapping it (those windows are restored after it).
                for(size_t at=0;at+8<=w.size();){uint32_t a,n;std::memcpy(&a,w.data()+at,4);std::memcpy(&n,w.data()+at+4,4);at+=8;
                    uint64_t lo=std::max<uint64_t>(a,sim),hi=std::min<uint64_t>(uint64_t(a)+n,uint64_t(sim)+StateSize);
                    if(lo<hi)std::memcpy(w.data()+at+(lo-a),state+(lo-sim),size_t(hi-lo));at+=n;}
                int status=reflex_slot13_world_draft(w.data(),w.size(),base.data(),base.size(),deform.data(),deform.size(),dt,uint32_t(std::strtoul(argv[5],nullptr,16)),out.data(),out.size());
                if(status)std::fill(out.begin(),out.end(),0xee),std::fprintf(stderr,"case %zu: %s\n",c/(4+StateSize),LastError.c_str());
                o.write(reinterpret_cast<const char*>(out.data()),out.size());
            }
            return o?0:2;
        }
        int status=reflex_slot13_world_draft(world.data(),world.size(),base.data(),base.size(),deform.data(),deform.size(),
            uint32_t(std::strtoul(argv[4],nullptr,16)),uint32_t(std::strtoul(argv[5],nullptr,16)),out.data(),out.size());
        if(status){std::fprintf(stderr,"slot13 world draft: status %d: %s\n",status,LastError.c_str());return 2;}
        std::ofstream o(argv[6],std::ios::binary);o.write(reinterpret_cast<const char*>(out.data()),out.size());return o?0:2;
    }catch(const std::exception& e){std::fprintf(stderr,"slot13 world draft: %s\n",e.what());return 2;}
}
#endif
