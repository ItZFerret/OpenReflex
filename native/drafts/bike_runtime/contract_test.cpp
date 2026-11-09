// Reusable private-runtime contracts. Build separately for each host architecture
// and run: contract_test <runtime.dylib> <bike-test-seed.bin> <final-state.bin>
// Expected results are invariants and repeated calculations, never a saved frame.
#include "bike_runtime.h"
#include <array>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(__x86_64__)
#include <xmmintrin.h>
#endif

using Bytes=std::vector<uint8_t>;
constexpr size_t StateSize=0x9700,SeedHeader=48;
static unsigned Checks=0,Updates=0,RejectedSeeds=0;
static void require(bool ok,const std::string& message){++Checks;if(!ok)throw std::runtime_error(message);}
static uint32_t word(const Bytes& b,size_t at){require(at+4<=b.size(),"test fixture read bounds");uint32_t v;std::memcpy(&v,b.data()+at,4);return v;}
static void put(Bytes& b,size_t at,uint32_t value){require(at+4<=b.size(),"test fixture write bounds");std::memcpy(b.data()+at,&value,4);}
struct FP {std::array<uint64_t,3> words;int exceptions;bool operator==(const FP& b)const{return words==b.words&&exceptions==b.exceptions;}};
static FP floating_state(){
#if defined(__x86_64__)
 uint16_t cw,sw;__asm__ volatile("fnstcw %0":"=m"(cw));__asm__ volatile("fnstsw %0":"=m"(sw));
 return {{{_mm_getcsr(),cw,sw}},std::fetestexcept(FE_ALL_EXCEPT)};
#elif defined(__aarch64__)
 uint64_t control,status;__asm__ volatile("mrs %0, fpcr":"=r"(control));__asm__ volatile("mrs %0, fpsr":"=r"(status));
 return {{{control,status,0}},std::fetestexcept(FE_ALL_EXCEPT)};
#else
#error unsupported test architecture
#endif
}
struct SavedFP {std::fenv_t value;SavedFP(){require(std::fegetenv(&value)==0,"save FP environment");}~SavedFP(){std::fesetenv(&value);}};
static void sticky_flags(){std::feclearexcept(FE_ALL_EXCEPT);std::feraiseexcept(FE_INVALID|FE_DIVBYZERO);}
struct API {
 void* lib;
 decltype(&reflex_test_bike_create) create;
 decltype(&reflex_test_bike_destroy) destroy;
 decltype(&reflex_test_bike_update) update;
 decltype(&reflex_test_bike_frame_read) frame;
 decltype(&reflex_test_bike_error) error;
 decltype(&reflex_test_bike_state) state;
 template<class T>T symbol(const char* name){auto p=dlsym(lib,name);require(p!=nullptr,std::string("missing API ")+name);return reinterpret_cast<T>(p);}
 explicit API(const char* path):lib(dlopen(path,RTLD_NOW|RTLD_LOCAL)){
  require(lib!=nullptr,std::string("dlopen: ")+(lib?"":dlerror()));
  create=symbol<decltype(create)>("reflex_test_bike_create");destroy=symbol<decltype(destroy)>("reflex_test_bike_destroy");
  update=symbol<decltype(update)>("reflex_test_bike_update");frame=symbol<decltype(frame)>("reflex_test_bike_frame_read");
  error=symbol<decltype(error)>("reflex_test_bike_error");state=symbol<decltype(state)>("reflex_test_bike_state");
 }
 ~API(){dlclose(lib);}
};
struct Context {
 API& api;void* pointer;
 Context(API& a,const Bytes& seed):api(a),pointer(nullptr){
  sticky_flags();const auto fp=floating_state();pointer=api.create(seed.data(),uint32_t(seed.size()));
  require(floating_state()==fp,"create mutated FP environment");require(pointer!=nullptr,std::string("valid constructor rejected: ")+api.error(nullptr));
 }
 ~Context(){api.destroy(pointer);}
};
struct Snapshot {Bytes state;reflex_test_bike_frame frame;};
static Snapshot snapshot(API& api,void* ctx){Snapshot s{Bytes(StateSize),{}};require(api.state(ctx,s.state.data(),StateSize)==0,"state export");require(api.frame(ctx,&s.frame)==0,"frame export");return s;}
static bool identical(const Snapshot& a,const Snapshot& b){return a.state==b.state&&std::memcmp(&a.frame,&b.frame,sizeof(a.frame))==0;}
static void within_world(const Snapshot& s){
 for(unsigned body=0;body<4;++body)for(unsigned lane=0;lane<3;++lane){
  const float x=s.frame.positions[body*4+lane];require(std::isfinite(x)&&std::fabs(x)<140.f,"bounded scenario left +/-140 test area");
 }
 require(std::isfinite(s.frame.speed),"nonfinite exported speed");
}
static void update(API& api,void* ctx,const reflex_test_bike_input& input){
 sticky_flags();const auto fp=floating_state();auto status=api.update(ctx,&input);
 require(floating_state()==fp,"successful update mutated FP environment");
 require(status==0,std::string("bounded update rejected: ")+api.error(ctx));++Updates;within_world(snapshot(api,ctx));
}
static void rejected_update(API& api,void* ctx,const reflex_test_bike_input* input){
 const auto old=snapshot(api,ctx);sticky_flags();const auto fp=floating_state();auto status=api.update(ctx,input);
 require(floating_state()==fp,"rejected update mutated FP environment");require(status!=0,"invalid input accepted");
 require(identical(old,snapshot(api,ctx)),"rejected update changed state/frame");
}
static void rejected_seed(API& api,const Bytes& seed){
 sticky_flags();const auto fp=floating_state();void* ctx=api.create(seed.data(),uint32_t(seed.size()));
 require(floating_state()==fp,"rejected seed mutated FP environment");if(ctx)api.destroy(ctx);
 require(ctx==nullptr,"malformed seed accepted");require(api.error(nullptr)&&*api.error(nullptr),"missing constructor diagnostic");++RejectedSeeds;
}
static size_t global_entry(const Bytes& seed,uint32_t address){
 size_t p=SeedHeader+StateSize+256+108;const auto n=word(seed,p);p+=4;
 for(uint32_t i=0;i<n;++i){const auto a=word(seed,p),size=word(seed,p+4);if(a==address)return p;p+=8+size;}
 throw std::runtime_error("missing test global");
}
static size_t region_table(const Bytes& seed){
 size_t p=SeedHeader+StateSize+256+108;const auto n=word(seed,p);p+=4;
 for(uint32_t i=0;i<n;++i)p+=8+word(seed,p+4);
 return p+16384+16384+128+19200;
}
static void resize_global(Bytes& seed,uint32_t address,uint32_t size){
 const size_t at=global_entry(seed,address);const uint32_t old=word(seed,at+4);
 Bytes data(size,0);std::memcpy(data.data(),seed.data()+at+8,std::min(old,size));
 seed.erase(seed.begin()+at+8,seed.begin()+at+8+old);seed.insert(seed.begin()+at+8,data.begin(),data.end());put(seed,at+4,size);
}

int main(int argc,char** argv){
 try{
  require(argc==4,"usage: contract_test runtime.dylib seed.bin final-state.bin");SavedFP original_environment;
  require(std::fesetround(FE_TONEAREST)==0,"nearest rounding");API api(argv[1]);
  std::ifstream input(argv[2],std::ios::binary);Bytes seed((std::istreambuf_iterator<char>(input)),{});
  require(input.good()||input.eof(),"read seed");require(seed.size()>90000,"seed completeness");
  float dt;const uint32_t dt_bits=word(seed,8);std::memcpy(&dt,&dt_bits,4);
  reflex_test_bike_input valid{};valid.substeps=4;valid.world_dt=dt*4;
  Context subject(api,seed),control(api,seed);const auto start=snapshot(api,subject.pointer);
  require(identical(start,snapshot(api,control.pointer)),"fresh contexts differ");
  require(api.update(nullptr,&valid)!=0,"null context update accepted");rejected_update(api,subject.pointer,nullptr);
  require(api.frame(subject.pointer,nullptr)!=0&&api.frame(nullptr,nullptr)!=0,"null frame accepted");
  Bytes sentinel(StateSize,0xa5);const auto saved=sentinel;
  require(api.state(subject.pointer,sentinel.data(),StateSize-1)!=0&&sentinel==saved,"bad state size wrote output");
  require(api.state(nullptr,sentinel.data(),StateSize)!=0&&sentinel==saved,"null state context wrote output");
  require(api.state(subject.pointer,nullptr,StateSize)!=0,"null state output accepted");api.destroy(nullptr);
  for(unsigned lane=0;lane<8;++lane){
   for(float value:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),1.01f,lane<4?-0.01f:-1.01f}){
    auto bad=valid;std::memcpy(reinterpret_cast<uint8_t*>(&bad)+lane*4,&value,4);rejected_update(api,subject.pointer,&bad);
   }
  }
  for(uint32_t count:{0u,9u,0xffffffffu}){auto bad=valid;bad.substeps=count;rejected_update(api,subject.pointer,&bad);}
  {auto bad=valid;bad.clutch=2;rejected_update(api,subject.pointer,&bad);}
  for(float value:{0.f,-.01f,.101f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){auto bad=valid;bad.world_dt=value;rejected_update(api,subject.pointer,&bad);}
  // Recovery must match an untouched context, including hidden filter/global
  // history as it affects future exported state.
  for(unsigned i=0;i<8;++i){update(api,subject.pointer,valid);update(api,control.pointer,valid);require(identical(snapshot(api,subject.pointer),snapshot(api,control.pointer)),"rejections changed later recurrence");}
  for(int rounding:{FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}){
   require(std::fesetround(rounding)==0,"set unsupported rounding");rejected_update(api,subject.pointer,&valid);rejected_seed(api,seed);
   require(std::fesetround(FE_TONEAREST)==0,"restore supported rounding");
  }
#if defined(__x86_64__)
  {const auto csr=_mm_getcsr();_mm_setcsr(csr|0x8000u);rejected_update(api,subject.pointer,&valid);_mm_setcsr(csr);}
#elif defined(__aarch64__)
  {uint64_t control_word;__asm__ volatile("mrs %0, fpcr":"=r"(control_word));const uint64_t unsupported=control_word|(1ull<<24);__asm__ volatile("msr fpcr, %0"::"r"(unsupported));rejected_update(api,subject.pointer,&valid);__asm__ volatile("msr fpcr, %0"::"r"(control_word));}
#endif
  for(auto change:std::vector<std::pair<size_t,uint32_t>>{{0,0},{4,99},{8,0},{8,0x7fc00000},{12,4},{16,3},{16,65},{20,3},{20,65},{24,1},{28,0},{32,0x7f800000},{36,0},{40,0x7fc00000},{44,0},{SeedHeader,0},{SeedHeader+0xec,3},{SeedHeader+0x1318,13},{SeedHeader+0x2e7c,1},{SeedHeader+0x2ebc,1},{SeedHeader+0x5c30,0},{SeedHeader+0x8b40,0},{SeedHeader+0x8e40,0}}){auto bad=seed;put(bad,change.first,change.second);rejected_seed(api,bad);}
  {auto bad=seed;bad.pop_back();rejected_seed(api,bad);bad=seed;bad.push_back(0);rejected_seed(api,bad);}
  const auto region=region_table(seed);
  for(auto change:std::vector<std::pair<size_t,uint32_t>>{{region,0},{region+4,0},{region+8,8},{region+12,1}}){auto bad=seed;put(bad,change.first,change.second);rejected_seed(api,bad);}
  const auto geometry=region-(16384+16384+128+19200);
  // Structurally valid but unsupported geometry must not silently fall back to
  // the flat test pad; check actual samples as well as dimensional metadata.
  for(auto change:std::vector<std::pair<size_t,uint32_t>>{{16,31},{28,0x439a8000},{36,0x41300000},{40,0x3f800000},{44,0x3e4ccccd},{geometry,0x3f800000},{geometry+16384,0},{geometry+32768,1}}){auto bad=seed;put(bad,change.first,change.second);rejected_seed(api,bad);}
  {auto bad=seed;resize_global(bad,0xd61670,4);rejected_seed(api,bad);}
  {auto bad=seed;put(bad,global_entry(bad,0xd61670),0xdeadbeef);rejected_seed(api,bad);}
  // These two wrong sizes preserve total record length. If validation is lazy,
  // rejection must occur atomically when the node globals are first consumed.
  {auto bad=seed;resize_global(bad,0x93a1f0,4);resize_global(bad,0x931f24,16);
   sticky_flags();const auto fp=floating_state();void* ctx=api.create(bad.data(),uint32_t(bad.size()));require(floating_state()==fp,"malformed global create FP");
   if(ctx){rejected_update(api,ctx,&valid);rejected_update(api,ctx,&valid);api.destroy(ctx);}else ++RejectedSeeds;
  }
  for(uint32_t profile=0;profile<4;++profile){auto changed=seed;put(changed,12,profile);Context a(api,changed),b(api,changed);for(unsigned i=0;i<3;++i){update(api,a.pointer,valid);update(api,b.pointer,valid);require(identical(snapshot(api,a.pointer),snapshot(api,b.pointer)),"profile repeated result differs");}}
  {auto sampled=seed;for(size_t at:{0x3cu,0x4cu,0x50u})put(sampled,SeedHeader+at,1);
   Context actions(api,sampled),neutral(api,seed);update(api,actions.pointer,valid);update(api,neutral.pointer,valid);
   require(identical(snapshot(api,actions.pointer),snapshot(api,neutral.pointer)),"unrepresented sampled action persisted from seed");
  }
  std::fprintf(stderr,"API rejection, seed tampering, FP and four profiles passed; running bounded deterministic drive\n");
  Bytes expected_final;
  for(unsigned repetition=0;repetition<2;++repetition){
   Context a(api,seed),b(api,seed);require(identical(start,snapshot(api,a.pointer)),"new context did not reset exactly");
   for(unsigned tick=0;tick<120;++tick){
    auto in=valid;in.throttle=tick>=16&&tick<90?.25f:0.f;
    if(tick>=90){in.front_brake=.2f;in.rear_brake=.2f;}
    if(tick>=48&&tick<72){in.steer_x=.03f;in.rider_x=-.02f;}
    if(tick%20==0){auto bad=in;bad.throttle=2;rejected_update(api,a.pointer,&bad);}
    update(api,a.pointer,in);update(api,b.pointer,in);
    require(identical(snapshot(api,a.pointer),snapshot(api,b.pointer)),"same inputs diverged between contexts");
   }
   const auto last=snapshot(api,a.pointer);require(last.frame.steps==480,"integration count differs");
   if(repetition==0)expected_final=last.state;else require(expected_final==last.state,"destroy/recreate recurrence differs");
  }
  std::ofstream output(argv[3],std::ios::binary);output.write(reinterpret_cast<const char*>(expected_final.data()),expected_final.size());output.close();require(bool(output),"write final state");
  std::printf("{\"success\":true,\"checks\":%u,\"successful_updates\":%u,\"rejected_seeds\":%u,\"drive_world_updates_per_context\":120,\"drive_integrations_per_context\":480,\"final_state_bytes\":%zu}\n",Checks,Updates,RejectedSeeds,expected_final.size());return 0;
 }catch(const std::exception& error){std::fprintf(stderr,"runtime contract failure: %s\n",error.what());return 1;}
}
