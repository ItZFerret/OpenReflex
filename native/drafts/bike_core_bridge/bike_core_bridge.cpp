// Private raw motocross state adapter to the registered mechanism core.
// No original executable code is run here. Only constructor-consistent owned
// objects are decoded; opaque headers/padding are preserved from one snapshot.
#include "../../include/reflex_core.h"
#include "../../src/fp32.h"
#include <algorithm>
#include <array>
#include <cfenv>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace reflex_bike_core_bridge_private {
constexpr size_t StateSize=0x9700,InputSize=StateSize+16+192;
static_assert(sizeof(reflex_mechanism_core_input)==216);
static_assert(sizeof(reflex_mechanism_node_input)==656);
static_assert(sizeof(reflex_mechanism_node_output)==2528);
enum Shape {None,M66,M46,M64,M44};
constexpr unsigned Fields[]={0x38,0x3c,0x44,0x48,0x4c,0x50,0x54,0x58,0x5c,0x60};
constexpr unsigned Offsets[3][10]={
 {0x290,0x290,0x70,0x180,0x290,0x290,0x290,0x290,0x290,0x290},
 {0x90,0x1a0,0x310,0x420,0x530,0x640,0x750,0x860,0x970,0xa80},
 {0xd0,0x180,0x270,0x380,0x490,0x520,0x5d0,0x620,0x670,0x720}};
constexpr Shape Shapes[3][10]={
 {None,None,M66,M66,None,None,None,None,None,None},
 {M66,M66,M66,M66,M66,M66,M66,M66,M66,M66},
 {M46,M46,M66,M66,M64,M46,M44,M44,M46,M64}};
constexpr unsigned MatrixSizes[]={0,0x110,0xb0,0x90,0x50};
constexpr unsigned RowCounts[]={0,12,8,6,4};
constexpr unsigned Rows[5][12]={
 {},{0x20,0x30,0x40,0x60,0x70,0x80,0xa0,0xb0,0xc0,0xe0,0xf0,0x100},
 {0x20,0x30,0x40,0x50,0x70,0x80,0x90,0xa0},
 {0x20,0x30,0x40,0x60,0x70,0x80},{0x10,0x20,0x30,0x40}};
constexpr unsigned VectorOffsets[3][3]={{0x2d0,0x2f0,0x2d0},{0x2e0,0xbc0,0xb90},{0x250,0x7b0,0x7e0}};
constexpr unsigned VectorDims[3][3]={{3,6,3},{6,6,6},{4,6,4}};
constexpr unsigned VectorVtables[3][3]={{0x53cb14,0x53cb00,0x53cb14},{0x53cb00,0x53cb00,0x53cb00},{0x53cb2c,0x53cb00,0x53cb2c}};
constexpr unsigned Vtables[]={0x53cb70,0x53cbc8,0x53cb9c};
constexpr unsigned GlobalRvas[]={0x9615c0,0x9616a0,0x961680,0x961670,0x53a1f0,0x53a200,0x53a210,0x53a220,0x52db80,0x531f24,0x52e330,0x5d7b30,0x5774a8,0x961650,0x961660};
static uint32_t word(const uint8_t* p){uint32_t v;std::memcpy(&v,p,4);return v;}
static void require(bool value){if(!value)throw std::runtime_error("invalid raw bike core object");}
struct Region {uint64_t begin,end;};
struct EnvironmentGuard {
 std::fenv_t saved{};
 EnvironmentGuard(){require(std::fegetenv(&saved)==0);}
 ~EnvironmentGuard(){std::fesetenv(&saved);}
};
struct Adapter {
 std::array<uint8_t,InputSize> input{};
 std::array<uint8_t,StateSize> result{};
 reflex_mechanism_core_input config{};
 reflex_mechanism_node_input nodes[4]{};
 reflex_mechanism_node_output output[4]{};
 uint32_t simulation=0,image=0,pointers[4]{};
 std::vector<Region> regions;
 explicit Adapter(const void* source){
  std::memcpy(input.data(),source,input.size());std::memcpy(result.data(),input.data(),result.size());
  simulation=word(input.data()+StateSize);image=word(input.data()+StateSize+4);
  require(simulation&&!(simulation&15)&&uint64_t(simulation)+StateSize<=0x100000000ULL);
  require(image&&uint64_t(image)+0x9616b0<=0x100000000ULL&&word(input.data()+StateSize+12)==0);
  require(word(input.data())==image+0x53c8a4&&word(input.data()+0xec)==4);
  config.node_count=4;std::memcpy(&config.dt,input.data()+StateSize+8,4);
  std::memcpy(config.gravity,input.data()+0xd0,16);std::memcpy(config.axis_x,input.data()+StateSize+16,192);
  region(simulation+0xd0,32);region(simulation+0xd70,16);
  for(unsigned k=0;k<15;k++){uint32_t address=image+GlobalRvas[k];unsigned size=k>=9&&k<=12?4:16;
   require(address%(size==4?4:16)==0);regions.push_back({address,uint64_t(address)+size});}
  for(unsigned i=0;i<4;i++)decode(i);
  unsigned incoming[4]{};
  for(unsigned i=0;i<4;i++)for(unsigned j=0;j<nodes[i].child_count;j++){
   unsigned child=nodes[i].children[j];require(child>0&&child<4&&nodes[child].parent==int(i)&&++incoming[child]==1);
  }
  for(unsigned i=1;i<4;i++)require(incoming[i]==1);
  std::sort(regions.begin(),regions.end(),[](const Region&a,const Region&b){return a.begin<b.begin;});
  for(size_t i=1;i<regions.size();i++)require(regions[i-1].end<=regions[i].begin);
 }
 const uint8_t* bytes(uint32_t address,unsigned size)const{
  require(address>=simulation&&uint64_t(address)+size<=uint64_t(simulation)+StateSize);
  return input.data()+address-simulation;
 }
 uint32_t get(uint32_t address)const{return word(bytes(address,4));}
 void region(uint32_t address,unsigned size){
  require(address&&!(address&15));bytes(address,size);regions.push_back({address,uint64_t(address)+size});
 }
 void matrix(uint32_t address,Shape shape){
  region(address,MatrixSizes[shape]);const auto* p=bytes(address,MatrixSizes[shape]);
  const unsigned vt[]={0,0x53ca94,0x53cab8,0x53cadc,0x53ca70};
  const unsigned heights[]={0,6,4,6,4},widths[]={0,6,6,4,4};
  require(word(p)==image+vt[shape]&&word(p+4)==heights[shape]&&word(p+8)==widths[shape]);
  auto block=[&](unsigned offset,unsigned vtable,unsigned rows,unsigned columns){
   require(word(p+offset)==image+vtable&&word(p+offset+4)==rows&&word(p+offset+8)==columns);
  };
  if(shape==M66)for(unsigned offset:{0x10u,0x50u,0x90u,0xd0u})block(offset,0x53ca04,3,3);
  if(shape==M46)for(unsigned offset:{0x10u,0x60u})block(offset,0x53ca4c,4,3);
  if(shape==M64)for(unsigned offset:{0x10u,0x50u})block(offset,0x53ca28,3,4);
 }
 void decode(unsigned i){
  auto& n=nodes[i];n.kind=i==0?0:i==1?1:2;
  const uint32_t p=get(simulation+0xd70+4*i);pointers[i]=p;region(p,0x70);
  require(get(p)==image+Vtables[n.kind]);n.parent=int32_t(get(p+0xc));n.child_count=get(p+0x10);
  require(i?(n.parent>=0&&unsigned(n.parent)<i):n.parent==-1);require(n.child_count<=3);
  for(unsigned j=0;j<n.child_count;j++)n.children[j]=get(p+0x14+4*j);
  for(unsigned j=0;j<4;j++){
   uint32_t expected=0;
   if(j>=2)expected=simulation+(j==2?0xf0:0x730)+0xa0*i;
   else if(i)expected=simulation+(j==0?0xf0:0x730)+0xa0*unsigned(n.parent);
   require(get(p+0x28+4*j)==expected);
   if(j>=2){region(expected,0xa0);constexpr unsigned BodyRows[]={0,0x10,0x20,0x30,0x50,0x60};
    for(unsigned row=0;row<6;row++)std::memcpy(n.bodies[j-2][row],bytes(expected+BodyRows[row],16),16);}
  }
  for(unsigned m=0;m<10;m++){
   const uint64_t expected=uint64_t(p)+Offsets[n.kind][m];require(expected<=0xffffffff&&get(p+Fields[m])==expected);
   Shape shape=Shapes[n.kind][m];if(shape==None)continue;matrix(uint32_t(expected),shape);
   if(m<2)for(unsigned r=0;r<RowCounts[shape];r++)std::memcpy(n.sources[m][r],bytes(uint32_t(expected)+Rows[shape][r],16),16);
  }
  constexpr unsigned VectorFields[]={0x40,0x64,0x68};
  for(unsigned v=0;v<3;v++){
   uint64_t address=uint64_t(p)+VectorOffsets[n.kind][v];unsigned dimension=VectorDims[n.kind][v],size=dimension==6?0x30:0x20;
   require(address<=0xffffffff&&get(p+VectorFields[v])==address);
   if(i||v!=2)region(uint32_t(address),size); // The one permitted alias: root .40 == .68.
   require(get(uint32_t(address))==image+VectorVtables[n.kind][v]&&get(uint32_t(address)+4)==dimension);
  }
  if(!i)return;
  region(p+(n.kind==1?0x2c0:0x240),n.kind==1?32:16);
  const unsigned locals=n.kind==1?1:3,local=n.kind==1?0x70:0x90;
  for(unsigned k=0;k<locals;k++){region(p+local+k*16,16);std::memcpy(n.locals[k],bytes(p+local+k*16,16),16);}
  matrix(p+(n.kind==1?0xbf0:0x800),M66);matrix(p+(n.kind==1?0xd00:0x910),n.kind==1?M66:M44);
 }
 void put(uint32_t address,const void* value,unsigned size){bytes(address,size);std::memcpy(result.data()+address-simulation,value,size);}
 void write_matrix(uint32_t address,Shape shape,const float rows[12][4]){
  for(unsigned row=0;row<RowCounts[shape];row++)put(address+Rows[shape][row],rows[row],16);
 }
 uint32_t run(){
  uint32_t status=reflex_mechanism_core(&config,sizeof(config),nodes,sizeof(nodes),output,sizeof(output));if(status)return status;
  for(unsigned i=0;i<4;i++){
   const auto& n=nodes[i];const auto& o=output[i];const uint32_t p=pointers[i];
   for(unsigned m=0;m<10;m++)if(Shapes[n.kind][m]!=None)write_matrix(p+Offsets[n.kind][m],Shapes[n.kind][m],o.preparation.matrices[m]);
   if(i){
    write_matrix(p+(n.kind==1?0xbf0:0x800),M66,o.preparation.node_products[0]);
    write_matrix(p+(n.kind==1?0xd00:0x910),n.kind==1?M66:M44,o.preparation.node_products[1]);
    put(p+(n.kind==1?0x2c0:0x240),o.bias,n.kind==1?32:16);
   }
   const float (*vectors[3])[4]={o.rhs,o.workspace,o.solution};
   for(unsigned v=0;v<3;v++)if(i||v==1)put(p+VectorOffsets[n.kind][v]+16,vectors[v],VectorDims[n.kind][v]==6?32:16);
   for(unsigned body=0;body<2;body++){
    uint32_t b=simulation+(body==0?0xf0:0x730)+0xa0*i;
    put(b+0x10,o.velocities[body],16);put(b+0x40,o.forces[body],16);put(b+0x70,o.accelerations[body],16);
   }
  }
  return REFLEX_OK;
 }
};
} // namespace reflex_bike_core_bridge_private

extern "C" uint32_t reflex_bike_core_bridge(const void* input,size_t input_size,void* output,size_t output_size){
 using namespace reflex_bike_core_bridge_private;
 if(!input||!output)return REFLEX_NULL_ARGUMENT;
 if(input_size!=InputSize||output_size!=StateSize)return REFLEX_ABI_MISMATCH;
 if(!reflex_detail::supported_fp())return REFLEX_UNSUPPORTED_FP;
 try{
  EnvironmentGuard guard;Adapter adapter(input);const uint32_t status=adapter.run();
  if(status==REFLEX_OK)std::memcpy(output,adapter.result.data(),StateSize);return status;
 }catch(const std::exception&){return REFLEX_INVALID_INPUT;}
}
