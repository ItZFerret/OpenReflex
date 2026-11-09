// Developmental full-step composition for an explicitly constructed test world.
// No recorded frame is consumed after construction. Public engine ABI unchanged.
#include "bike_runtime.h"
#include "../../src/fp32.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
uint32_t reflex_bike_force_stage(const void*,uint32_t,void*,uint32_t);
uint32_t reflex_draft_bike_controls_terrain(const void*,uint32_t,void*,uint32_t);
int reflex_bike_slot12_draft(const void*,size_t,void*,size_t);
int reflex_slot13_draft(const void*,size_t,void*,size_t);
uint32_t reflex_draft_bike_slot15(const void*,uint32_t,void*,uint32_t);
uint32_t reflex_draft_bike_step_bridge(uint32_t,const void*,uint32_t,void*,uint32_t);
int reflex_bike_core_bridge(const void*,size_t,void*,size_t);
uint32_t reflex_bike_poststep_draft(uint32_t,const void*,uint32_t,void*,uint32_t);
}

namespace reflex_test_private {
using Bytes=std::vector<uint8_t>;
constexpr uint32_t Sim=0x10000000,Control=0x20000000,Filters=0x30000000;
constexpr size_t StateSize=0x9700;
struct Span {uint32_t address,size;};
#include "layout.inc"
static thread_local std::string CreateError;

uint32_t word(const Bytes&b,size_t p){if(p+4>b.size())throw std::runtime_error("word bounds");uint32_t v;std::memcpy(&v,b.data()+p,4);return v;}
float scalar(const Bytes&b,size_t p){uint32_t v=word(b,p);float f;std::memcpy(&f,&v,4);return f;}
void put(Bytes&b,size_t p,uint32_t v){if(p+4>b.size())throw std::runtime_error("write bounds");std::memcpy(b.data()+p,&v,4);}
uint32_t bits(float f){uint32_t b;std::memcpy(&b,&f,4);return b;}
void append(Bytes&b,uint32_t v){size_t n=b.size();b.resize(n+4);std::memcpy(b.data()+n,&v,4);}
void append(Bytes&b,const Bytes&v){b.insert(b.end(),v.begin(),v.end());}
void append(Bytes&b,const void*v,size_t n){const auto*p=static_cast<const uint8_t*>(v);b.insert(b.end(),p,p+n);}
Bytes slice(const Bytes&b,size_t p,size_t n){if(p+n>b.size())throw std::runtime_error("slice bounds");return Bytes(b.begin()+p,b.begin()+p+n);}
struct Region {uint32_t address;bool writable;Bytes data;};
struct Reader {Bytes data;size_t p=0;uint32_t u(){uint32_t v=word(data,p);p+=4;return v;}Bytes take(size_t n){Bytes b=slice(data,p,n);p+=n;return b;}};

struct Bike {
    Bytes state,control,filters,base,deform,bounds,triangles;
    std::map<uint32_t,Bytes> globals;
    std::vector<Region> regions;
    uint32_t dt=0,profile=0,nx=0,nz=0,stride=0,steps=0;
    float wx=0,wz=0,spacing=0,gain=0,normal=0;
    std::string error;

    explicit Bike(const void*data,size_t size){
        if(!data||size>200000)throw std::runtime_error("seed bounds");
        Reader r{Bytes(static_cast<const uint8_t*>(data),static_cast<const uint8_t*>(data)+size)};
        if(r.u()!=0x4b425852||r.u()!=1)throw std::runtime_error("seed version");
        dt=r.u();profile=r.u();nx=r.u();nz=r.u();stride=r.u();
        float* meta[]={&wx,&wz,&spacing,&gain,&normal};for(auto p:meta){uint32_t v=r.u();std::memcpy(p,&v,4);}
        if(profile>3||nx<4||nx>64||nz<4||nz>64||stride!=nx||!std::isfinite(wx)||!std::isfinite(wz)||!std::isfinite(spacing)||!std::isfinite(gain)||!std::isfinite(normal)||wx<=0||wz<=0||spacing<=0||normal<=0)
            throw std::runtime_error("world metadata");
        // This API currently owns one flat test pad. Keep the ray, body-probe,
        // triangle producer and Unity surface descriptions identical.
        if(nx!=32||nz!=32||stride!=32||wx!=310.f||wz!=310.f||spacing!=10.f||gain!=0.f||normal!=.1f)
            throw std::runtime_error("unsupported test-pad geometry");
        float fixed;std::memcpy(&fixed,&dt,4);if(!std::isfinite(fixed)||fixed<=0||fixed>.03f)throw std::runtime_error("fixed delta");
        state=r.take(StateSize);control=r.take(256);filters=r.take(108);
        if(word(state,0)!=0x93c8a4||word(state,0xec)!=4||word(state,0x1318)!=12||word(state,0x2e7c)||word(state,0x2ebc)||word(state,0x130c)||word(state,0x1310)||word(state,0x5c30)!=Control)
            throw std::runtime_error("test-world bike constructor");
        for(unsigned i=0;i<3;i++)if(word(state,0x8b40+i*4)!=Filters+i*12)throw std::runtime_error("rider filters");
        for(unsigned i=0;i<6;i++)if(word(state,0x8e40+i*4)!=Filters+36+i*12)throw std::runtime_error("wheel filters");
        uint32_t count=r.u();if(count>64)throw std::runtime_error("global count");
        while(count--){uint32_t a=r.u(),n=r.u();if((n!=4&&n!=16)||globals.count(a))throw std::runtime_error("global shape");globals[a]=r.take(n);}
        base=r.take(16384);deform=r.take(16384);bounds=r.take(128);triangles=r.take(19200);
        for(size_t p=0;p<base.size();p+=4)if(word(base,p)!=0||word(deform,p)!=bits(.5f))throw std::runtime_error("test-pad height profile");
        for(auto value:bounds)if(value)throw std::runtime_error("test-pad ray bounds");
        count=r.u();if(count!=sizeof(Slot15Regions)/sizeof(*Slot15Regions))throw std::runtime_error("region count");
        for(uint32_t i=0;i<count;i++){uint32_t a=r.u(),n=r.u(),w=r.u();const auto&expected=Slot15Regions[i];if(a!=expected.address||n!=expected.size||w!=expected.writable)throw std::runtime_error("region canonical binding");regions.push_back({a,bool(w),r.take(n)});}
        if(r.p!=size)throw std::runtime_error("trailing seed data");
        contacts();
    }

    const Bytes& global(uint32_t a)const {auto i=globals.find(a);if(i==globals.end())throw std::runtime_error("missing explicit global");return i->second;}
    template<size_t N>void append_globals(Bytes&row,const Span(&spans)[N],bool contact=false){
        for(auto s:spans){const auto&v=global(s.address);if(v.size()!=s.size)throw std::runtime_error("global size");
            if(s.address==0xa95844)append(row,contact?0x31000000u:0x32000000u);
            else if(s.address==0xa946b8)append(row,0x33000000u);else append(row,v);}
    }
    template<size_t N>size_t accept_globals(const Bytes&row,size_t cursor,const Span(&spans)[N]){
        for(auto s:spans){Bytes value=slice(row,cursor,s.size);if(s.address!=0xa95844&&s.address!=0xa946b8)globals[s.address]=std::move(value);cursor+=s.size;}return cursor;
    }
    template<typename Fn>Bytes call(const char*name,Fn f,const Bytes&row,size_t size){Bytes output(size);auto status=f(row.data(),row.size(),output.data(),output.size());if(status)throw std::runtime_error(std::string(name)+" rejected ("+std::to_string(status)+")");return output;}
    template<size_t N>size_t common(const Bytes&row,unsigned first,unsigned count,const Span(&spans)[N]){
        state=slice(row,0,StateSize);control=slice(row,StateSize,256);size_t cursor=StateSize+256;
        for(unsigned i=0;i<count;i++){auto v=slice(row,cursor,12);std::memcpy(filters.data()+12*(first+i),v.data(),12);cursor+=12;}return accept_globals(row,cursor,spans);
    }
    void contacts(){
        Bytes local=state;for(unsigned i=0;i<6;i++)put(local,0x8e40+i*4,Filters+12*i);
        Bytes row;for(auto v:{dt,profile,nx,nz,stride,1u,1u,bits(wx),bits(wz),bits(spacing),bits(gain),bits(normal)})append(row,v);
        append(row,local);append(row,control);append(row,filters.data()+36,72);append_globals(row,Slot13Globals);append(row,base);append(row,deform);append(row,triangles);
        auto out=call("terrain contacts",reflex_slot13_draft,row,58360);size_t cursor=common(out,3,6,Slot13Globals);triangles=slice(out,cursor,19200);
        for(unsigned i=0;i<6;i++)put(state,0x8e40+i*4,Filters+36+12*i);
    }
    void controls(){
        Bytes ray;for(auto v:{nx,nz,(nx+15)/16,1u,1u,1u,profile,bits(wx),bits(wz),bits(spacing),bits(16.f),bits(-1.f)})append(ray,v);
        ray.resize(ray.size()+64);append(ray,base);append(ray,bounds);
        Bytes row=state;append(row,control);append(row,filters.data(),36);append_globals(row,ControlsGlobals);append(row,dt);append(row,profile);append(row,ray);
        common(call("controls",reflex_draft_bike_controls_terrain,row,39188),0,3,ControlsGlobals);
    }
    Bytes node_globals()const{
        Bytes out;unsigned index=0;for(auto a:{0xd615c0u,0xd616a0u,0xd61680u,0xd61670u,0x93a1f0u,0x93a200u,0x93a210u,0x93a220u,0x92db80u,0x931f24u,0x92e330u,0x9d7b30u,0x9774a8u}){const auto&v=global(a);if(v.size()!=(index++<9?16u:4u))throw std::runtime_error("node global size");append(out,v);}return out;
    }
    void forces(){
        for(unsigned i=0;i<4;i++)for(auto offset:{0x80u,0x70u,0x6c0u,0x6b0u,0x60u,0x6a0u})std::memcpy(state.data()+0xd0+i*0xa0+offset,global(0xd61670).data(),16);
        // Null moving-object binding makes4355d0 return. The test has seven
        // deliberately unpressed debug-force keys, so435320 adds no forces.
        Bytes row=state;append(row,control);for(auto a:{0xd615c0u,0xd616a0u,0xd61650u,0xd61670u})append(row,global(a));
        append(row,dt);append(row,profile&1);append(row,profile&2?0x027fu:0x007fu);
        state=call("forces",reflex_bike_force_stage,row,StateSize);
        row=state;for(auto v:{Sim,0x400000u,dt,0u})append(row,v);append(row,node_globals());append(row,global(0xd61650));append(row,global(0xd61660));
        state=call("mechanism",reflex_bike_core_bridge,row,StateSize);
    }
    void rider(){
        Bytes row=state;append(row,control);append(row,filters.data(),36);append_globals(row,Slot12Globals);append(row,dt);append(row,profile);
        common(call("rider",reflex_bike_slot12_draft,row,39096),0,3,Slot12Globals);
    }
    void response(){
        Bytes row=state;append_globals(row,Slot15Globals,true);for(const auto&r:regions)append(row,r.data);append(row,dt);append(row,profile);
        auto out=call("contact response",reflex_draft_bike_slot15,row,45900);state=slice(out,0,StateSize);size_t cursor=accept_globals(out,StateSize,Slot15Globals);
        for(auto&r:regions){r.data=slice(out,cursor,r.data.size());cursor+=r.data.size();}
    }
    void bridge(uint32_t stage){
        Bytes row=state;append(row,control);append(row,Sim);append(row,dt);append(row,node_globals());append(row,global(0xd61660));
        Bytes out(StateSize);auto status=reflex_draft_bike_step_bridge(stage,row.data(),row.size(),out.data(),out.size());if(status)throw std::runtime_error("pose bridge "+std::to_string(stage)+" rejected ("+std::to_string(status)+")");state=std::move(out);
    }
    void integrate(){controls();forces();if(word(state,0x2de0)==0||word(state,0x2de4)==1)rider();response();bridge(1);bridge(2);bridge(3);contacts();bridge(4);bridge(5);steps++;}
    void input(const reflex_test_bike_input& in){
        const float values[]={in.throttle,in.reverse,in.front_brake,in.rear_brake,in.steer_x,in.steer_y,in.rider_x,in.rider_y};
        for(unsigned i=0;i<8;i++)if(!std::isfinite(values[i])||values[i]>(1.f)||values[i]<(i<4?0.f:-1.f))throw std::runtime_error("input range");
        if(in.clutch>1||in.substeps<1||in.substeps>8||!std::isfinite(in.world_dt)||in.world_dt<=0||in.world_dt>.1f)throw std::runtime_error("world step range");
        for(unsigned i=0;i<8;i++)put(state,0x1c+i*4,bits(values[i]));put(state,0x48,in.clutch);
        for(auto offset:{0x3cu,0x4cu,0x50u})put(state,offset,0); // unrepresented sampled actions
        if(word(state,0x54)){put(state,0x24,bits(1));put(state,0x28,bits(1));if(in.throttle>0||in.reverse>0)put(state,0x54,0);}
    }
    void post(float world_dt){
        Bytes row;for(auto offset:{0xf0u,0x190u,0x230u,0x2d0u,0x730u,0x100u,0x8c30u,0x8c40u,0x8c50u})append(row,state.data()+offset,16);
        for(auto offset:{0x8c28u,0x60u,0x5cu,0x68u})append(row,word(state,offset));append(row,word(control,0));append(row,bits(world_dt));
        Bytes output(132);if(reflex_bike_poststep_draft(1,row.data(),row.size(),output.data(),output.size()))throw std::runtime_error("world poststep rejected");
        size_t cursor=0;for(auto offset:{0xf0u,0x190u,0x230u,0x2d0u,0x100u,0x8c30u,0x8c40u,0x8c50u}){std::memcpy(state.data()+offset,output.data()+cursor,16);cursor+=16;}put(state,0x68,word(output,128));
    }
};
struct SavedEnvironment {std::fenv_t value{};bool valid=std::fegetenv(&value)==0;~SavedEnvironment(){if(valid)std::fesetenv(&value);}};
static_assert(sizeof(reflex_test_bike_input)==44);
static_assert(sizeof(reflex_test_bike_frame)==156);
} // namespace

using reflex_test_private::Bike;
extern "C" void* reflex_test_bike_create(const void*data,uint32_t size){reflex_test_private::SavedEnvironment env;try{if(!env.valid||!reflex_detail::supported_fp())throw std::runtime_error("Unsupported floating point controls");return new Bike(data,size);}catch(const std::exception&e){reflex_test_private::CreateError=e.what();return nullptr;}}
extern "C" void reflex_test_bike_destroy(void*ctx){delete static_cast<Bike*>(ctx);}
extern "C" uint32_t reflex_test_bike_update(void*ctx,const reflex_test_bike_input*in){
    if(!ctx||!in)return 1;auto&b=*static_cast<Bike*>(ctx);
    reflex_test_private::SavedEnvironment env;
    try{if(!env.valid||!reflex_detail::supported_fp())throw std::runtime_error("Unsupported floating point controls");Bike next=b;next.input(*in);for(uint32_t i=0;i<in->substeps;i++)next.integrate();next.post(in->world_dt);next.error.clear();b=std::move(next);return 0;}catch(const std::exception&e){b.error=e.what();return 2;}
}
extern "C" uint32_t reflex_test_bike_frame_read(void*ctx,reflex_test_bike_frame*out){
    if(!ctx||!out)return 1;auto&b=*static_cast<Bike*>(ctx);reflex_test_bike_frame f{};
    for(unsigned i=0;i<4;i++){std::memcpy(f.positions+4*i,b.state.data()+0xf0+0xa0*i,16);std::memcpy(f.orientations+4*i,b.state.data()+0x730+0xa0*i,16);}
    std::memcpy(f.velocity,b.state.data()+0x100,16);std::memcpy(&f.speed,b.state.data()+0x94,4);std::memcpy(&f.gear,b.state.data()+0x8ac0,4);f.steps=b.steps;*out=f;return 0;
}
extern "C" const char* reflex_test_bike_error(void*ctx){return ctx?static_cast<Bike*>(ctx)->error.c_str():reflex_test_private::CreateError.c_str();}
extern "C" uint32_t reflex_test_bike_state(void*ctx,void*out,uint32_t n){if(!ctx||!out||n!=reflex_test_private::StateSize)return 1;std::memcpy(out,static_cast<Bike*>(ctx)->state.data(),n);return 0;}
