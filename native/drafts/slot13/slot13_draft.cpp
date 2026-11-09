// Private slot13 numeric draft. Not a registered engine ABI.
// Original generated cases enter once and evolve in one mutable byte image.
#include "../../src/fp32.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <set>

namespace reflex_slot13_private {
using Wide=unsigned __int128;
constexpr uint32_t Sim=0x10000000,Control=0x20000000,Stack=0x70000000;
constexpr size_t StateSize=0x9700,ControlSize=0x100,FilterSize=72,GlobalSize=152,GridSize=16384,TriangleSize=19200,RecordSize=91152,OutputSize=58360;
constexpr uint32_t Filters=0x30000000,Terrain=0x31000000,Scene=0x32000000,Resource=0x33000000,Base=0x34000000,Deform=0x35000000,Triangles=0xad4c30,Lock=0x36000000,Unlock=0x36000010;
static const uint32_t RuntimeAddresses[]={0x9b45e4,0x9cd960,0x9cd970,0x9cd9e8,0x9d3bfc,0x9d7b30,0x9ddfc8,0x9e30dc,0x9e30e0,0xa95844,0xd615c0,0xd61650,0xd61660,0xd61670,0xd61680,0xd61690,0xd616a0};
static const unsigned RuntimeSizes[]={4,4,4,4,4,4,4,4,4,4,16,16,16,16,16,16,16};
static void finite(const uint8_t* data,size_t size){for(size_t i=0;i<size;i+=4){uint32_t w;std::memcpy(&w,data+i,4);if((w&0x7f800000)==0x7f800000)throw std::runtime_error("nonfinite terrain input/output");}}
static_assert(OutputSize==StateSize+ControlSize+FilterSize+GlobalSize+TriangleSize+24);
struct Operand { int kind=0,size=0,reg=-1,base=-1,index=-1,scale=0,displacement=0;uint64_t immediate=0; };
struct Instruction {uint32_t address;int size,opcode,count;Operand o[3];};
struct Constant {uint32_t address;size_t size;uint8_t bytes[16];};
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

struct Memory {
    std::array<uint8_t,StateSize> state{};std::array<uint8_t,ControlSize> control{};std::array<uint8_t,FilterSize> filters{};
    std::array<uint8_t,0x44f0> terrain{};std::array<uint8_t,GridSize> base{},deform{};std::array<uint8_t,TriangleSize> triangles{};
    std::array<uint8_t,0x10000> stack{};std::unordered_map<uint32_t,uint8_t> globals;
    uint8_t byte(uint32_t a)const{
        if(a>=Sim&&a<Sim+StateSize)return state[a-Sim];if(a>=Control&&a<Control+ControlSize)return control[a-Control];
        if(a>=Filters&&a<Filters+FilterSize)return filters[a-Filters];if(a>=Terrain&&a<Terrain+terrain.size())return terrain[a-Terrain];
        if(a>=Base&&a<Base+GridSize)return base[a-Base];if(a>=Deform&&a<Deform+GridSize)return deform[a-Deform];
        if(a>=Triangles&&a<Triangles+TriangleSize)return triangles[a-Triangles];if(a>=Stack&&a<Stack+stack.size())return stack[a-Stack];
        auto it=globals.find(a);if(it==globals.end())throw std::runtime_error("unmapped slot13 read");return it->second;
    }
    Wide read(uint32_t a,int n)const{
        Wide out=0;for(int i=0;i<n;i++)out|=Wide(byte(a+i))<<(8*i);
        for(int i=0;i<n;i+=4)if(n-i>=4&&((a+i>=Base&&a+i<Base+GridSize)||(a+i>=Deform&&a+i<Deform+GridSize)))if((uint32_t(out>>(8*i))&0x7f800000)==0x7f800000)throw std::runtime_error("nonfinite slot13 height");
        return out;
    }
    void write(uint32_t a,int n,Wide value){
        for(int i=0;i<n;i++){
            const uint32_t at=a+i;const uint8_t v=uint8_t(value>>(8*i));
            if(at>=Sim&&at<Sim+StateSize)state[at-Sim]=v;else if(at>=Filters&&at<Filters+FilterSize)filters[at-Filters]=v;
            else if(at>=Triangles&&at<Triangles+TriangleSize)triangles[at-Triangles]=v;else if(at>=Stack&&at<Stack+stack.size())stack[at-Stack]=v;
            else if((at>=0x9e30dc&&at<0x9e30e0)||(at>=0x9e30e0&&at<0x9e30e4))globals[at]=v;
            else throw std::runtime_error("slot13 write outside owned output");
        }
    }
    void seed(uint32_t a,const uint8_t*p,size_t n){for(size_t i=0;i<n;i++)globals[a+uint32_t(i)]=p[i];}
    void seedword(uint32_t a,uint32_t v){seed(a,reinterpret_cast<const uint8_t*>(&v),4);}
};
struct Flags {bool z=false,c=false,s=false,o=false,p=false;};
struct Machine {
    Memory mem;std::array<Wide,16> r{};std::vector<double> x87;uint32_t pc=0,profile=0;Flags flags;
    uint64_t executed=0,pow_calls=0;
    unsigned locks=0,unlocks=0,lock_flags=0,null_rect=0;uint32_t returned=0,success=0;
    unsigned stride=0;bool locked=false;std::set<uint32_t> float_stores;
    Machine(const uint8_t *data){
        auto word=[&](size_t off){uint32_t v;std::memcpy(&v,data+off,4);return v;};
        uint32_t dt=word(0),nx=word(8),nz=word(12),present=word(20);profile=word(4);stride=word(16);success=word(24);
        if(profile>3||nx<4||nx>64||nz<4||nz>64||stride<nx||stride>64||present>1||success>1)throw std::runtime_error("slot13 header");
        finite(data,4);finite(data+28,20);if(f32(dt)<0||!(f32(word(28))>0&&f32(word(32))>0&&f32(word(36))>0&&f32(word(44))>0))throw std::runtime_error("slot13 metadata");
        size_t cursor=48;std::memcpy(mem.state.data(),data+cursor,StateSize);cursor+=StateSize;std::memcpy(mem.control.data(),data+cursor,ControlSize);cursor+=ControlSize;std::memcpy(mem.filters.data(),data+cursor,FilterSize);cursor+=FilterSize;
        for(const auto& c:constants)mem.seed(c.address,c.bytes,c.size);
        for(unsigned i=0;i<17;i++){mem.seed(RuntimeAddresses[i],data+cursor,RuntimeSizes[i]);cursor+=RuntimeSizes[i];}
        std::memcpy(mem.base.data(),data+cursor,GridSize);cursor+=GridSize;std::memcpy(mem.deform.data(),data+cursor,GridSize);cursor+=GridSize;std::memcpy(mem.triangles.data(),data+cursor,TriangleSize);
        mem.seedword(0x9e30d4,Terrain);mem.seedword(0x9e30d8,0);for(unsigned i=0;i<32;i++)mem.globals[Scene+i]=0;mem.seedword(Scene+0x1c,0x12345678);
        if(mem.read(0x9e30dc,4)!=1||!(mem.read(0x9e30e0,4)&1)||mem.read(0xa95844,4)!=Scene)throw std::runtime_error("slot13 registry");
        auto tw=[&](size_t off,uint32_t v){std::memcpy(mem.terrain.data()+off,&v,4);};
        tw(0x1c,present?Base:0);tw(0x240,Resource);tw(0x44bc,stride);tw(0x44a8,0x12345678);tw(0x44c8,bits32(float(nx)));tw(0x44cc,bits32(float(nz)));tw(0x44d0,word(28));tw(0x44d4,word(32));tw(0x44e8,word(36));tw(0xbc,word(40));tw(0x44ec,word(44));
        for(unsigned i=0;i<76;i++)mem.globals[Resource+i]=0;mem.seedword(Resource,Resource+16);mem.seedword(Resource+16+52,Lock);mem.seedword(Resource+16+56,Unlock);
        mem.seedword(0x93c8a4+128,0x44ef70);mem.seedword(0x93cb9c+24,0x85a8f0);mem.seedword(0x93cbc8+24,0x85b750);
        if(mem.read(Sim+0x5c30,4)!=Control||mem.read(Sim+0xd78,4)==mem.read(Sim+0xd7c,4))throw std::runtime_error("slot13 explicit control/distinct nodes");
        if(mem.read(Sim,4)!=0x93c8a4||mem.read(Sim+0xec,4)!=4||mem.read(Sim+0x1318,4)<1||mem.read(Sim+0x1318,4)>12||mem.read(Sim+0x130c,4)!=mem.read(Sim+0x1310,4))throw std::runtime_error("slot13 bike contract");
        if(mem.read(Sim+0x8790,4)>=4||mem.read(Sim+0x88e0,4)>=4)throw std::runtime_error("slot13 wheel index");
        for(unsigned off=0x8e40;off<0x8e58;off+=4){uint32_t p=uint32_t(mem.read(Sim+off,4));if((p&3)||p<Filters||p>Filters+FilterSize-12)throw std::runtime_error("slot13 filter ownership");finite(mem.filters.data()+p-Filters,12);if(!(f32(uint32_t(mem.read(p+4,4)))>0))throw std::runtime_error("slot13 filter duration");}
        for(unsigned off:{0xd78u,0xd7cu}){uint32_t p=uint32_t(mem.read(Sim+off,4));if((p&15)||p<Sim||p>Sim+StateSize-0xc0)throw std::runtime_error("slot13 node ownership");uint32_t vt=uint32_t(mem.read(p,4));if(vt!=0x93cb9c&&vt!=0x93cbc8)throw std::runtime_error("slot13 node class");for(unsigned j=0x28;j<0x38;j+=4){uint32_t b=uint32_t(mem.read(p+j,4));if((b&15)||b<Sim||b>Sim+StateSize-16)throw std::runtime_error("slot13 body ownership");}}
        r[2]=Sim;r[7]=Stack+0x8000;pc=0x444b90;mem.write(uint32_t(r[7]),4,0);mem.write(uint32_t(r[7])+4,4,dt);
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
        case OP_jle:return flags.z||flags.s!=flags.o;case OP_jns:return !flags.s;case OP_jge:return flags.s==flags.o;case OP_jmp:return true;default:throw std::runtime_error("bad branch");}
    }
    bool hook(uint32_t target){
        const uint32_t sp=uint32_t(r[7]);
        if(target==Lock){uint32_t self=uint32_t(mem.read(sp,4)),rect=uint32_t(mem.read(sp+4,4)),region=uint32_t(mem.read(sp+8,4)),f=uint32_t(mem.read(sp+12,4));if(self!=Resource||region||f!=0x810||locked)throw std::runtime_error("slot13 LockRect");locks++;lock_flags=f;null_rect=1;returned=success;locked=success;mem.write(rect,4,stride*4);mem.write(rect+4,4,success?Deform:0);r[0]=success?0:0x80004005;r[7]+=16;return true;}
        if(target==Unlock){if(mem.read(sp,4)!=Resource||!locked)throw std::runtime_error("slot13 Unlock");unlocks++;locked=false;r[7]+=4;r[0]=0;return true;}
        if(target==0x8f9efe){if(uint32_t(r[2])!=uint32_t(mem.read(0x9b45e4,4)))throw std::runtime_error("slot13 cookie");integer_flags(uint32_t(r[2]),uint32_t(r[2]),0,true);return true;}
        if(target==0x916230){uint64_t a=uint64_t(mem.read(sp,8)),b=uint64_t(mem.read(sp+8,8));x87.push_back(f64((a&0x7fffffffffffffffULL)|(b&0x8000000000000000ULL)));return true;}
        if(target==0x85ddd0)throw std::runtime_error("dynamic descriptor collision outside terrain-only contract");
        return false;
    }
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
        case OP_je:case OP_jne:case OP_jb:case OP_jbe:case OP_jae:case OP_jl:case OP_jge:case OP_jle:case OP_jns:case OP_jmp:if(taken(op))pc=uint32_t(get(a,4));return;
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
        while(pc){if(++executed>1000000)throw std::runtime_error("instruction budget");auto found=lookup.find(pc);if(found==lookup.end())throw std::runtime_error("PC outside closure");const auto& i=*found->second;pc+=i.size;try{step(i);}catch(const std::exception& error){throw std::runtime_error(std::to_string(i.address)+": "+error.what());}}
    }
};

// Private atomic snapshot boundary, supports arbitrary input/output overlap.
static thread_local std::string LastError;
struct EnvironmentGuard {std::fenv_t saved{};EnvironmentGuard(){if(std::fegetenv(&saved))throw std::runtime_error("save FP environment");}~EnvironmentGuard(){std::fesetenv(&saved);}};
extern "C" int reflex_slot13_draft(const void *input,size_t input_bytes,void *output,size_t output_bytes){
    if(!input||!output||input_bytes!=RecordSize||output_bytes!=OutputSize)return 1;
    if(!reflex_detail::supported_fp())return 2;
    try{
        EnvironmentGuard environment;Machine m(static_cast<const uint8_t*>(input));m.run();if(m.locked||!m.x87.empty())throw std::runtime_error("slot13 unbalanced resource/x87");
        for(uint32_t a:m.float_stores){uint32_t w=uint32_t(m.mem.read(a,4));finite(reinterpret_cast<uint8_t*>(&w),4);}
        uint32_t count=uint32_t(m.mem.read(Sim+0x8788,4));if(count>400)throw std::runtime_error("slot13 triangle count");
        std::array<uint8_t,OutputSize> result{};size_t cursor=0;
        auto append=[&](const uint8_t*p,size_t n){std::memcpy(result.data()+cursor,p,n);cursor+=n;};
        append(m.mem.state.data(),StateSize);append(m.mem.control.data(),ControlSize);append(m.mem.filters.data(),FilterSize);
        for(unsigned i=0;i<17;i++)for(unsigned j=0;j<RuntimeSizes[i];j++)result[cursor++]=m.mem.byte(RuntimeAddresses[i]+j);
        append(m.mem.triangles.data(),TriangleSize);const uint32_t telemetry[]={m.locks,m.unlocks,m.lock_flags,m.null_rect,m.returned,count};append(reinterpret_cast<const uint8_t*>(telemetry),24);
        std::memcpy(output,result.data(),result.size());return 0;
    }catch(const std::exception&e){LastError=e.what();return 2;}
}
} // namespace reflex_slot13_private
#ifndef REFLEX_SLOT13_NO_MAIN
int main(int argc,char **argv){
    using namespace reflex_slot13_private;
    if(argc!=3)return 2;
    try{
        std::ifstream input(argv[1],std::ios::binary);std::ofstream output(argv[2],std::ios::binary);if(!input||!output)throw std::runtime_error("file open");
        std::vector<uint8_t> row(RecordSize),result(OutputSize);uint64_t count=0;
        while(input.read(reinterpret_cast<char*>(row.data()),row.size())||input.gcount()){
            if(size_t(input.gcount())!=row.size()||++count>10000)throw std::runtime_error("partial/excess row");if(reflex_slot13_draft(row.data(),row.size(),result.data(),result.size()))throw std::runtime_error(LastError);output.write(reinterpret_cast<const char*>(result.data()),result.size());if(!output)throw std::runtime_error("output write");
        }
        if(!input.eof())throw std::runtime_error("input failure");output.close();if(!output)throw std::runtime_error("output close");std::fprintf(stderr,"slot13 native draft: %llu calls\n",count);return 0;
    }catch(const std::exception&e){std::fprintf(stderr,"slot13 draft: %s\n",e.what());return 2;}
}
#endif
