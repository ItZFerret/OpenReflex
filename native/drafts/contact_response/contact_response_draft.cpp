// Private instruction-derived contact response draft. Not a registered engine ABI.
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
constexpr uint32_t Sim=0x10000000,Stack=0x70000000,Queue=0x36000000;
constexpr size_t StateSize=0x9700,RecordSize=45852;
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
// This draft explicitly rejects exponent gaps above70; integer alignment fits128.
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
    double result=std::ldexp(double(uint64_t(rounded)),common+int(shift));
    if(!std::isfinite(result)||(!result&&rounded)||(result&&std::abs(result)<std::numeric_limits<double>::min()))throw std::runtime_error("x87 exponent outside private double representation");
    return negative?-result:result;
}

// Exact product of two binary64 significands fits106 bits. Round once to the
// original PC24/PC53 precision, without a binary64 intermediate product.
static double multiply_precision(double left,double right,unsigned precision){
    if(!std::isfinite(left)||!std::isfinite(right))throw std::runtime_error("nonfinite x87 product");
    const uint64_t a=bits64(left),b=bits64(right),fraction=(1ULL<<52)-1;
    auto exponent=[](uint64_t v){unsigned e=unsigned((v>>52)&2047);return e?int(e)-1075:-1074;};
    auto significand=[](uint64_t v){return (v&fraction)|((v&0x7ff0000000000000ULL)?1ULL<<52:0);};
    const bool negative=(a^b)>>63;const Wide magnitude=Wide(significand(a))*significand(b);
    if(!magnitude)return negative?-0.0:0.0;
    const uint64_t high=uint64_t(magnitude>>64),low=uint64_t(magnitude);
    const unsigned width=high?128-unsigned(__builtin_clzll(high)):64-unsigned(__builtin_clzll(low));
    const unsigned shift=width>precision?width-precision:0;Wide rounded=magnitude>>shift;
    if(shift){const Wide rem=magnitude&((Wide(1)<<shift)-1),half=Wide(1)<<(shift-1);rounded+=rem>half||(rem==half&&(rounded&1));}
    const double result=std::ldexp(double(uint64_t(rounded)),exponent(a)+exponent(b)+int(shift));
    if(!std::isfinite(result)||(!result&&rounded)||(result&&std::abs(result)<std::numeric_limits<double>::min()))throw std::runtime_error("x87 exponent outside private double representation");
    return negative?-result:result;
}

struct Memory {
    std::array<uint8_t,StateSize> state{};std::array<uint8_t,0x10000> stack{};
    std::unordered_map<uint32_t,uint8_t> globals;
    uint8_t byte(uint32_t at)const{
        if(at>=Sim&&at<Sim+StateSize)return state[at-Sim];
        if(at>=Stack&&at<Stack+stack.size())return stack[at-Stack];
        auto found=globals.find(at);if(found==globals.end())throw std::runtime_error("unmapped contact response read");return found->second;
    }
    Wide read(uint32_t at,int n)const{Wide value=0;for(int i=0;i<n;i++)value|=Wide(byte(at+i))<<(8*i);return value;}
    void write(uint32_t at,int n,Wide value){
        for(int i=0;i<n;i++){
            const uint32_t a=at+i;const uint8_t v=uint8_t(value>>(8*i));
            if(a>=Sim&&a<Sim+StateSize)state[a-Sim]=v;
            else if(a>=Stack&&a<Stack+stack.size())stack[a-Stack]=v;
            else if((a>=Queue&&a<Queue+0x1904)||(a>=0x9e30dc&&a<0x9e30e4))globals[a]=v;
            else throw std::runtime_error("unowned contact response write");
        }
    }
};
struct Flags {bool z=false,c=false,s=false,o=false,p=false;};
struct Machine {
    Memory mem;std::array<Wide,16> r{};std::vector<double> x87;uint32_t pc=0x4360b0,profile=0;Flags flags;
    uint64_t executed=0;
    Machine(const uint8_t *data){
        std::memcpy(mem.state.data(),data,StateSize);size_t cursor=StateSize;
        for(const auto& c:constants)for(size_t i=0;i<c.size;i++)mem.globals[c.address+uint32_t(i)]=c.bytes[i];
        for(const auto& span:runtime_spans)for(size_t i=0;i<span.size;i++)mem.globals[span.address+uint32_t(i)]=data[cursor++];
        for(const auto& span:input_regions)for(size_t i=0;i<span.size;i++)mem.globals[span.address+uint32_t(i)]=data[cursor++];
        if(cursor!=RecordSize||mem.read(Sim+0xec,4)<1||mem.read(Sim+0xec,4)>10||mem.read(Sim+0x20f8,4)>12||mem.read(Sim+0x1318,4)>25)throw std::runtime_error("contact response layout/count");
        r[2]=Sim;r[7]=Stack+0x8000;mem.write(uint32_t(r[7]),4,0);
    }
    uint32_t address(const Operand& o)const{return uint32_t((o.base<0?0:uint32_t(r[o.base]))+(o.index<0?0:uint32_t(r[o.index]))*o.scale+o.displacement);}
    Wide get(const Operand& o,int size=0)const{
        const int n=size?size:o.size;
        if(o.kind==0&&o.reg>=16)return (r[o.reg==16?0:3]>>0)&255;
        return (o.kind==0?r[o.reg]:o.kind==1?Wide(o.immediate):mem.read(address(o),n))&mask(n);
    }
    void put(const Operand& o,Wide v,int size=0){
        const int n=size?size:o.size;v&=mask(n);
        if(o.kind==0&&o.reg>=16){const unsigned shift=0,index=o.reg==16?0:3;r[index]=(r[index]&~(Wide(255)<<shift))|((v&255)<<shift);}
        else if(o.kind==0)r[o.reg]=o.reg>=8?(r[o.reg]&~mask(n))|v:v;
        else if(o.kind==2)mem.write(address(o),n,v);else throw std::runtime_error("writing immediate");
    }
    void integer_flags(uint32_t a,uint32_t b,uint64_t result,bool subtraction=false,unsigned bits=32){
        const uint32_t limit=uint32_t((uint64_t(1)<<bits)-1),sign=1u<<(bits-1),v=uint32_t(result)&limit;a&=limit;b&=limit;
        flags.z=v==0;flags.c=subtraction?a<b:result>limit;flags.s=bool(v&sign);
        flags.o=bool((subtraction?(a^b)&(a^v):~(a^b)&(a^v))&sign);flags.p=__builtin_parity(unsigned(v&255))==0;
    }
    bool taken(int opcode)const {
        switch(opcode){case OP_je:return flags.z;case OP_jne:return !flags.z;case OP_jb:return flags.c;
        case OP_jbe:return flags.c||flags.z;case OP_ja:return !flags.c&&!flags.z;case OP_jae:return !flags.c;case OP_jl:return flags.s!=flags.o;
        case OP_jle:return flags.z||flags.s!=flags.o;case OP_jge:return flags.s==flags.o;case OP_jnp:return !flags.p;case OP_jmp:return true;default:throw std::runtime_error("bad branch");}
    }
    bool hook(uint32_t target){
        if(target!=0x402dc0&&target!=0x75c580&&target!=0x42bd90)throw std::runtime_error("contact response target outside closure");return false;
    }
    void step(const Instruction& i){
        const auto& a=i.o[0];const auto& b=i.o[1];const auto& c=i.o[2];const int op=i.opcode;
        switch(op){
        case OP_imul:{
            const int64_t left=int32_t(uint32_t(i.count==1?r[0]:get(b))),right=int32_t(uint32_t(i.count==1?get(a):get(c)));
            const int64_t value=left*right;
            if(i.count==1){r[0]=uint32_t(value);r[3]=uint32_t(uint64_t(value)>>32);}else if(i.count==3)put(a,uint32_t(value));else throw std::runtime_error("IMUL form");
            flags.c=flags.o=value<INT32_MIN||value>INT32_MAX;return;
        }
        case OP_sar:case OP_shr:{
            const uint32_t x=uint32_t(get(a)),count=uint32_t(get(b))&31;if(!count)return;
            const uint32_t value=op==OP_sar?uint32_t(int32_t(x)>>count):x>>count;put(a,value);integer_flags(x,0,value);
            flags.c=(x>>(count-1))&1;if(count==1)flags.o=op==OP_shr&&(x&0x80000000);return;
        }
        case OP_mov:case OP_movaps:case OP_movapd:case OP_movups:put(a,get(b));return;
        case OP_movss:case OP_movsd:{int n=op==OP_movss?4:8;Wide v=get(b,n);if(a.kind==0&&b.kind==2)r[a.reg]=v;else put(a,v,n);return;}
        case OP_lea:put(a,address(b));return;
        case OP_push:{Wide v=get(a,4);r[7]-=4;mem.write(uint32_t(r[7]),4,v);return;}
        case OP_pop:{Wide v=mem.read(uint32_t(r[7]),4);r[7]+=4;put(a,v,4);return;}
        case OP_call:{uint32_t target=a.kind==2?address(a):uint32_t(get(a,4));if(hook(target))return;r[7]-=4;mem.write(uint32_t(r[7]),4,pc);pc=target;return;}
        case OP_ret:pc=uint32_t(mem.read(uint32_t(r[7]),4));r[7]+=4+(i.count?uint32_t(get(a)):0);return;
        case OP_je:case OP_jne:case OP_jb:case OP_jbe:case OP_jae:case OP_ja:case OP_jl:case OP_jge:case OP_jle:case OP_jnp:case OP_jmp:if(taken(op))pc=uint32_t(get(a,4));return;
        case OP_sete:case OP_setle:put(a,op==OP_sete?flags.z:flags.z||flags.s!=flags.o);return;
        case OP_lahf:{const unsigned value=2|(unsigned(flags.s)<<7)|(unsigned(flags.z)<<6)|(unsigned(flags.p)<<2)|unsigned(flags.c);r[0]=(r[0]&~Wide(0xff00))|(value<<8);return;}
        case OP_unpcklps:{Wide x=get(a),y=get(b);put(a,Wide(uint32_t(x))|(Wide(uint32_t(y))<<32)|(Wide(uint32_t(x>>32))<<64)|(Wide(uint32_t(y>>32))<<96));return;}
        case OP_add:case OP_sub:case OP_and:case OP_or:case OP_xor:case OP_cmp:case OP_test:case OP_inc:case OP_dec:case OP_shl:{
            uint32_t x=uint32_t(get(a)),y=op==OP_inc||op==OP_dec?1:uint32_t(get(b));uint64_t value=0;const bool old_carry=flags.c;
            if(op==OP_add||op==OP_inc)value=uint64_t(x)+y;else if(op==OP_sub||op==OP_cmp||op==OP_dec)value=uint64_t(x)-y;
            else if(op==OP_and||op==OP_test)value=x&y;else if(op==OP_or)value=x|y;else if(op==OP_xor)value=x^y;else value=uint64_t(x)<<(y&31);
            integer_flags(x,y,value,op==OP_sub||op==OP_cmp||op==OP_dec,unsigned(a.size*8));
            if(op==OP_and||op==OP_or||op==OP_xor||op==OP_test)flags.c=flags.o=false;
            if(op==OP_inc||op==OP_dec)flags.c=old_carry;
            if(op!=OP_cmp&&op!=OP_test)put(a,value);return;}
        case OP_comiss:case OP_ucomiss:{int c=compare(uint32_t(get(a,4)),uint32_t(get(b,4)));flags={c==0||c==2,c==-1||c==2,false,false,c==2};return;}
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
        case OP_fldz:x87.push_back(0);return;
        case OP_fst:{if(x87.empty())throw std::runtime_error("x87 empty");double value=x87.back();if(a.size==4){volatile float v=float(value);put(a,bits32(v));}else put(a,bits64(value));return;}
        case OP_fstp:{if(x87.empty())throw std::runtime_error("x87 empty");double value=x87.back();x87.pop_back();if(a.size==4){volatile float v=float(value);put(a,bits32(v));}else put(a,bits64(value));return;}
        case OP_fadd:{if(x87.empty())throw std::runtime_error("x87 empty");double operand=a.size==4?double(f32(uint32_t(get(a)))):f64(uint64_t(get(a)));x87.back()=add_precision(x87.back(),operand,profile&2?53:24);return;}
        case OP_fmul:{if(x87.empty())throw std::runtime_error("x87 empty");double operand=a.size==4?double(f32(uint32_t(get(a)))):f64(uint64_t(get(a)));x87.back()=multiply_precision(x87.back(),operand,profile&2?53:24);return;}
        default:throw std::runtime_error("unsupported instruction");
        }
    }
    void run(){
        static const auto lookup=[](){std::unordered_map<uint32_t,const Instruction*> map;for(const auto& i:program)map[i.address]=&i;return map;}();
        while(pc){if(++executed>100000)throw std::runtime_error("instruction budget");auto found=lookup.find(pc);if(found==lookup.end())throw std::runtime_error("PC outside closure");const auto& i=*found->second;pc+=i.size;try{step(i);}catch(const std::exception& error){throw std::runtime_error(std::to_string(i.address)+": "+error.what());}}
        if(!x87.empty())throw std::runtime_error("x87 stack not balanced");
    }
};

int main(int argc,char **argv){
    if(argc!=3||!reflex_detail::supported_fp())return 2;
    try{
        std::ifstream input(argv[1],std::ios::binary);std::ofstream output(argv[2],std::ios::binary);
        if(!input||!output)throw std::runtime_error("file open");std::vector<uint8_t> row(RecordSize);uint64_t count=0,instructions=0;
        while(input.read(reinterpret_cast<char*>(row.data()),row.size())||input.gcount()){
            if(size_t(input.gcount())!=row.size()||++count>10000)throw std::runtime_error("partial/excess row");Machine m(row.data());m.run();
            output.write(reinterpret_cast<const char*>(m.mem.state.data()),StateSize);
            for(const auto& span:runtime_spans)for(size_t i=0;i<span.size;i++)output.put(char(m.mem.byte(span.address+uint32_t(i))));
            for(const auto& span:input_regions)for(size_t i=0;i<span.size;i++)output.put(char(m.mem.byte(span.address+uint32_t(i))));
            if(!output)throw std::runtime_error("output write");instructions+=m.executed;
        }
        if(!input.eof())throw std::runtime_error("input failure");output.close();if(!output)throw std::runtime_error("output close");
        std::fprintf(stderr,"contact response native: %llu cases, %llu instructions\n",count,instructions);return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"contact response native: %s\n",e.what());return 2;}
}
