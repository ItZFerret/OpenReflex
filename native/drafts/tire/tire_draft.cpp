// Private instruction-derived tire draft. Not a registered engine ABI.
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

using Wide=unsigned __int128;
constexpr uint32_t Sim=0x10000000,Control=0x20000000,Stack=0x70000000;
constexpr size_t StateSize=0x9700,ControlSize=0x100,RecordSize=StateSize+ControlSize+64+12;
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
    std::array<uint8_t,StateSize> state;std::array<uint8_t,ControlSize> control;
    std::array<uint8_t,0x10000> stack{};std::unordered_map<uint32_t,uint8_t> globals;
    uint8_t byte(uint32_t a)const{
        if(a>=Sim&&a<Sim+StateSize)return state[a-Sim];
        if(a>=Control&&a<Control+ControlSize)return control[a-Control];
        if(a>=Stack&&a<Stack+stack.size())return stack[a-Stack];
        auto it=globals.find(a);if(it==globals.end())throw std::runtime_error("unmapped reference read");return it->second;
    }
    Wide read(uint32_t a,int n)const {Wide out=0;for(int i=0;i<n;i++)out|=Wide(byte(a+i))<<(8*i);return out;}
    void write(uint32_t a,int n,Wide value){
        for(int i=0;i<n;i++){
            if(a+i>=Sim&&a+i<Sim+StateSize)state[a+i-Sim]=uint8_t(value>>(8*i));
            else if(a+i>=Stack&&a+i<Stack+stack.size())stack[a+i-Stack]=uint8_t(value>>(8*i));
            else throw std::runtime_error("write outside tire/stack");
        }
    }
};
struct Flags {bool z=false,c=false,s=false,o=false,p=false;};
struct Machine {
    Memory mem;std::array<Wide,16> r{};std::vector<double> x87;uint32_t pc=0x447f10,profile;Flags flags;
    uint64_t executed=0,pow_calls=0;
    Machine(const uint8_t *data){
        std::memcpy(mem.state.data(),data,StateSize);std::memcpy(mem.control.data(),data+StateSize,ControlSize);
        const uint8_t *tail=data+StateSize+ControlSize;
        for(const auto& c:constants)for(size_t i=0;i<c.size;i++)mem.globals[c.address+uint32_t(i)]=c.bytes[i];
        constexpr uint32_t addresses[4]={0xd615c0,0xd616a0,0xd61650,0xd61670};
        for(unsigned i=0;i<4;i++)for(unsigned j=0;j<16;j++)mem.globals[addresses[i]+j]=tail[i*16+j];
        uint32_t dt,side;std::memcpy(&dt,tail+64,4);std::memcpy(&side,tail+68,4);std::memcpy(&profile,tail+72,4);
        if(side>1||profile>3)throw std::runtime_error("invalid tire selector");
        const unsigned wheel=side?0x88e0:0x8790,index=uint32_t(mem.read(Sim+wheel,4));
        if(index!=2&&index!=3)throw std::runtime_error("tire index outside bike scope");
        if(!(f32(uint32_t(mem.read(Sim+wheel+0xe4,4)))>0)||!(f32(dt)>=0))throw std::runtime_error("invalid filter time/dt");
        if(mem.read(Sim+wheel+0x100,4)&&f32(dt)==0)throw std::runtime_error("active zero dt");
        mem.write(Sim+0x5c30,4,Control);r[5]=Sim;r[7]=Stack+0x8000;
        mem.write(uint32_t(r[7]),4,0);mem.write(uint32_t(r[7])+4,4,dt);mem.write(uint32_t(r[7])+8,4,Sim+wheel);
    }
    uint32_t address(const Operand& o)const{return uint32_t((o.base<0?0:uint32_t(r[o.base]))+(o.index<0?0:uint32_t(r[o.index]))*o.scale+o.displacement);}
    Wide get(const Operand& o,int size=0)const{
        const int n=size?size:o.size;return (o.kind==0?r[o.reg]:o.kind==1?Wide(o.immediate):mem.read(address(o),n))&mask(n);
    }
    void put(const Operand& o,Wide v,int size=0){
        const int n=size?size:o.size;v&=mask(n);
        if(o.kind==0)r[o.reg]=o.reg>=8?(r[o.reg]&~mask(n))|v:v;
        else if(o.kind==2)mem.write(address(o),n,v);else throw std::runtime_error("writing immediate");
    }
    void integer_flags(uint32_t a,uint32_t b,uint64_t result,bool subtraction=false){
        const uint32_t v=uint32_t(result);flags.z=v==0;flags.c=subtraction?a<b:result>0xffffffffULL;flags.s=bool(v&0x80000000);
        flags.o=bool((subtraction?(a^b)&(a^v):~(a^b)&(a^v))&0x80000000);flags.p=false;
    }
    bool taken(int opcode)const {
        switch(opcode){case OP_je:return flags.z;case OP_jne:return !flags.z;case OP_jb:return flags.c;
        case OP_jbe:return flags.c||flags.z;case OP_jae:return !flags.c;case OP_jl:return flags.s!=flags.o;
        case OP_jge:return flags.s==flags.o;case OP_jmp:return true;default:throw std::runtime_error("bad branch");}
    }
    bool hook(uint32_t target){
        if(target==0x4390d0){const float x=f32(uint32_t(r[8]));if(!(x>=0&&x<=9.5f))throw std::runtime_error("indexed table bound");return false;}
        if(target==0x438ee0){
            const uint32_t table=uint32_t(r[4]),count=uint32_t(mem.read(table,4));if(count<2||count>10)throw std::runtime_error("curve count");
            for(unsigned i=1;i<count;i++)if(!(f32(uint32_t(mem.read(table+4*i,4)))<f32(uint32_t(mem.read(table+4+4*i,4)))))throw std::runtime_error("curve knots");
            return false;
        }
        if(target==0x8f9efe){if(uint32_t(r[2])!=uint32_t(mem.read(0x9b45e4,4)))throw std::runtime_error("stack cookie");integer_flags(uint32_t(r[2]),uint32_t(r[2]),0,true);return true;}
        if(target==0x8fa0d0){
            if(x87.empty()||!std::isfinite(x87.back())||x87.back()<=-2147483649.0||x87.back()>=2147483648.0)throw std::runtime_error("conversion range");
            const int32_t value=int32_t(x87.back());x87.pop_back();r[0]=uint32_t(value);r[3]=value<0?0xffffffffu:0;return true;
        }
        if(target==0x916230){
            uint64_t a=uint64_t(mem.read(uint32_t(r[7]),8)),b=uint64_t(mem.read(uint32_t(r[7])+8,8));
            x87.push_back(f64((a&0x7fffffffffffffffULL)|(b&0x8000000000000000ULL)));return true;
        }
        if(target==0x9009b4){if(f64(uint64_t(r[9]))!=2)throw std::runtime_error("pow exponent");r[8]=arithmetic64(2,uint64_t(r[8]),uint64_t(r[8]));pow_calls++;return true;}
        return false;
    }
    void step(const Instruction& i){
        const auto& a=i.o[0];const auto& b=i.o[1];const auto& c=i.o[2];const int op=i.opcode;
        switch(op){
        case OP_mov:case OP_movaps:case OP_movapd:case OP_movups:put(a,get(b));return;
        case OP_movss:case OP_movsd:{int n=op==OP_movss?4:8;Wide v=get(b,n);if(a.kind==0&&b.kind==2)r[a.reg]=v;else put(a,v,n);return;}
        case OP_lea:put(a,address(b));return;
        case OP_push:{Wide v=get(a,4);r[7]-=4;mem.write(uint32_t(r[7]),4,v);return;}
        case OP_pop:{Wide v=mem.read(uint32_t(r[7]),4);r[7]+=4;put(a,v,4);return;}
        case OP_call:{uint32_t target=a.kind==2?address(a):uint32_t(get(a,4));if(hook(target))return;r[7]-=4;mem.write(uint32_t(r[7]),4,pc);pc=target;return;}
        case OP_ret:pc=uint32_t(mem.read(uint32_t(r[7]),4));r[7]+=4+(i.count?uint32_t(get(a)):0);return;
        case OP_je:case OP_jne:case OP_jb:case OP_jbe:case OP_jae:case OP_jl:case OP_jge:case OP_jmp:if(taken(op))pc=uint32_t(get(a,4));return;
        case OP_add:case OP_sub:case OP_and:case OP_or:case OP_xor:case OP_cmp:case OP_test:case OP_inc:case OP_shl:{
            uint32_t x=uint32_t(get(a)),y=op==OP_inc?1:uint32_t(get(b));uint64_t value=0;
            if(op==OP_add||op==OP_inc)value=uint64_t(x)+y;else if(op==OP_sub||op==OP_cmp)value=uint64_t(x)-y;
            else if(op==OP_and||op==OP_test)value=x&y;else if(op==OP_or)value=x|y;else if(op==OP_xor)value=x^y;else value=uint64_t(x)<<(y&31);
            integer_flags(x,y,value,op==OP_sub||op==OP_cmp);
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
        while(pc){if(++executed>20000)throw std::runtime_error("instruction budget");auto found=lookup.find(pc);if(found==lookup.end())throw std::runtime_error("PC outside closure");const auto& i=*found->second;pc+=i.size;try{step(i);}catch(const std::exception& error){throw std::runtime_error(std::to_string(i.address)+": "+error.what());}}
    }
};

int main(int argc,char **argv){
    if(argc==2&&std::string(argv[1])=="--self-test"){
        const double near=-.5+std::ldexp(1.0,-40);
        if(add_precision(near,-.5,24)!=-1||add_precision(near,-.5,53)!=-1+std::ldexp(1.0,-40))return 1;
        if(bits64(add_precision(-0.0,-0.0,24))!=0x8000000000000000ULL||bits64(add_precision(-0.0,0.0,53))!=0)return 1;
        if(arithmetic32(3,0,0)!=0xffc00000||square_root(0x40000000)!=0x3fb504f3)return 1;
        std::fprintf(stderr,"tire native arithmetic:6 directed checks passed\n");return 0;
    }
    if(argc!=3){std::fprintf(stderr,"usage: tire-draft input.bin output.bin\n");return 2;}
    if(!reflex_detail::supported_fp()){std::fprintf(stderr,"Unsupported native FP controls\n");return 2;}
    try{
        std::ifstream input(argv[1],std::ios::binary);std::ofstream output(argv[2],std::ios::binary);
        if(!input||!output)throw std::runtime_error("file open");std::vector<uint8_t> row(RecordSize);uint64_t count=0,instructions=0,pow_calls=0;
        while(input.read(reinterpret_cast<char*>(row.data()),row.size())||input.gcount()){
            if(size_t(input.gcount())!=row.size()||++count>10000)throw std::runtime_error("partial/excess row");
            Machine machine(row.data());machine.run();std::memcpy(machine.mem.state.data()+0x5c30,row.data()+0x5c30,4);
            output.write(reinterpret_cast<const char*>(machine.mem.state.data()),StateSize);if(!output)throw std::runtime_error("output write");instructions+=machine.executed;pow_calls+=machine.pow_calls;
        }
        if(!input.eof())throw std::runtime_error("input failure");output.close();if(!output)throw std::runtime_error("output close");
        std::fprintf(stderr,"tire native draft: %llu cases, %llu instructions, %llu pow adapters\n",count,instructions,pow_calls);return 0;
    }catch(const std::exception& error){std::fprintf(stderr,"tire draft: %s\n",error.what());return 2;}
}
