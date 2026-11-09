#include <vector>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cfenv>
extern "C" int reflex_terrain_ray_draft(const void*,size_t,void*,size_t);
static void check(bool value){if(!value){std::fprintf(stderr,"ray contract failed\n");std::exit(1);}}
static void put(std::vector<uint8_t>& a,unsigned off,uint32_t x){std::memcpy(a.data()+off,&x,4);}
static uint32_t get(const std::vector<uint8_t>& a,unsigned off){uint32_t x;std::memcpy(&x,a.data()+off,4);return x;}
static std::vector<uint8_t> seed(){
 std::vector<uint8_t> a(16624,0);const uint32_t header[]={33,33,3,1,1,1,0};std::memcpy(a.data(),header,sizeof(header));
 const float scalar[]={32,32,1,16,-1,-2,512,2,6,2,0,-2,7,3,4,5,6,7,8,9,10};std::memcpy(a.data()+28,scalar,sizeof(scalar));
 for(unsigned i=0;i<4096;i++)put(a,112+i*4,0x3e000000);
 const float bounds[]={255,257};for(unsigned i=0;i<16;i++)std::memcpy(a.data()+16496+i*8,bounds,8);return a;
}
int main(){
 auto a=seed();std::vector<uint8_t> expected(36,0x6b);check(reflex_terrain_ray_draft(a.data(),a.size(),expected.data(),36)==0);
 check(get(expected,0)==1&&get(expected,8)==0x43800000);check(get(expected,20)==0&&get(expected,24)==0x3f800000&&get(expected,28)==0x80000000);
 check(get(expected,16)==get(a,92)&&get(expected,32)==get(a,108));
 for(unsigned profile=0;profile<4;profile++){auto p=a;put(p,24,profile);std::vector<uint8_t> out(36);check(reflex_terrain_ray_draft(p.data(),p.size(),out.data(),36)==0&&out==expected);}
 unsigned overlaps=0;
 for(int offset:{-36,-4,0,1,4,16,64,1000,int(a.size()-4)}){
  std::vector<uint8_t> buffer(a.size()+2048,0xcc);uint8_t*src=buffer.data()+512;std::memcpy(src,a.data(),a.size());uint8_t*dst=src+offset;auto before=buffer;
  check(reflex_terrain_ray_draft(src,a.size(),dst,36)==0&&std::memcmp(dst,expected.data(),36)==0);
  for(size_t i=0;i<buffer.size();i++)if(buffer.data()+i<dst||buffer.data()+i>=dst+36)check(buffer[i]==before[i]);overlaps++;
 }
 std::vector<uint8_t> output(36,0x6b),before=output;
 check(reflex_terrain_ray_draft(nullptr,a.size(),output.data(),36)==1&&output==before);check(reflex_terrain_ray_draft(a.data(),a.size()-1,output.data(),36)==1&&output==before);check(reflex_terrain_ray_draft(a.data(),a.size(),output.data(),35)==1&&output==before);
 for(auto change:std::array<std::array<uint32_t,2>,5>{{{0,65},{8,1},{24,4},{28,0x7fc00000},{48,0x7f800000}}}){auto bad=a;put(bad,change[0],change[1]);check(reflex_terrain_ray_draft(bad.data(),bad.size(),output.data(),36)==2&&output==before);}
 auto bad=a;for(unsigned i=0;i<4096;i++)put(bad,112+i*4,0x7fc00000);check(reflex_terrain_ray_draft(bad.data(),bad.size(),output.data(),36)==2&&output==before);
 int rounding=std::fegetround();check(std::fesetround(FE_DOWNWARD)==0);check(reflex_terrain_ray_draft(a.data(),a.size(),output.data(),36)==2&&output==before);check(std::fesetround(rounding)==0);
 for(unsigned off:{12u,16u,20u}){auto missing=a;put(missing,off,0);check(reflex_terrain_ray_draft(missing.data(),missing.size(),output.data(),36)==0);check(get(output,0)==0&&std::memcmp(output.data()+4,missing.data()+80,32)==0);}
 auto opaque=a;put(opaque,60,0x7fc00000);put(opaque,76,0x7fc00000);put(opaque,92,0xffc00000);put(opaque,108,0x7fa12345);check(reflex_terrain_ray_draft(opaque.data(),opaque.size(),output.data(),36)==0);check(get(output,16)==get(opaque,92)&&get(output,32)==get(opaque,108));
 std::fprintf(stderr,"ray contracts: flat height/normal,4 profiles,%u arbitrary overlaps, atomic malformed/nonfinite/FP rejects,3 missing sources,opaque W pass\n",overlaps);return 0;
}
