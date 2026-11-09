#include <array>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cfenv>
extern "C" int reflex_terrain_query_draft(const void*,size_t,void*,size_t);
static void require(bool yes){if(!yes){std::fprintf(stderr,"terrain contract failed\n");std::exit(1);}}
static void word(std::vector<uint8_t>& b,size_t at,uint32_t w){std::memcpy(b.data()+at,&w,4);}
static uint32_t word(const std::vector<uint8_t>& b,size_t at){uint32_t w;std::memcpy(&w,b.data()+at,4);return w;}
static void scalar(std::vector<uint8_t>& b,size_t at,float value){std::memcpy(b.data()+at,&value,4);}
static std::vector<uint8_t> input(){
    std::vector<uint8_t> b(52112,0);
    for(unsigned i=0;i<6;i++)word(b,i*4,std::array<unsigned,6>{5,5,5,400,1,1}[i]);
    const float values[]={4,4,1,2,0,123,0,5,1.2f,99,1.2f,101,1,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1};
    std::memcpy(b.data()+24,values,sizeof(values));
    for(unsigned i=0;i<4096;i++){scalar(b,136+i*4,.125f);scalar(b,16520+i*4,.75f);}
    std::memset(b.data()+32904,0xa5,19200);word(b,52104,0xdeadbeef);scalar(b,52108,-1);return b;
}
int main(){
    auto row=input();std::vector<uint8_t> expected(19224,0x5a);
    require(reflex_terrain_query_draft(row.data(),row.size(),expected.data(),expected.size())==0);
    const uint32_t header[]={18,1,1,0x810,1,1};require(std::memcmp(expected.data(),header,sizeof(header))==0);
    const float triangles[]={-2,257,1,1,-2,257,2,2,-1,257,2,2,-2,257,1,1,-1,257,2,2,-1,257,1,1};
    require(std::memcmp(expected.data()+24,triangles,sizeof(triangles))==0);
    for(unsigned i=24+18*48;i<expected.size();i++)require(expected[i]==0xa5);
    unsigned overlaps=0;
    for(int displacement:{-19224,-64,-4,0,1,4,16,64,300,int(row.size()-4)}){
        std::vector<uint8_t> storage(row.size()+2*19224+64,0xcc);uint8_t* source=storage.data()+19224;std::memcpy(source,row.data(),row.size());
        uint8_t* destination=source+displacement;auto before=storage;
        require(reflex_terrain_query_draft(source,row.size(),destination,expected.size())==0);require(std::memcmp(destination,expected.data(),expected.size())==0);
        for(size_t j=0;j<storage.size();j++)if(storage.data()+j<destination||storage.data()+j>=destination+expected.size())require(storage[j]==before[j]);overlaps++;
    }
    std::vector<uint8_t> out(expected.size(),0x6b),before=out;
    require(reflex_terrain_query_draft(nullptr,row.size(),out.data(),out.size())==1&&out==before);
    require(reflex_terrain_query_draft(row.data(),row.size()-1,out.data(),out.size())==1&&out==before);
    require(reflex_terrain_query_draft(row.data(),row.size(),out.data(),out.size()-1)==1&&out==before);
    for(auto mutation:std::array<std::array<uint32_t,2>,8>{{{0,65},{8,4},{12,401},{16,2},{24,0x7fc00000},{56,0xbf800000},{52108,0x7f800000},{136+4*12,0x7fc00000}}}){
        auto bad=row;word(bad,mutation[0],mutation[1]);require(reflex_terrain_query_draft(bad.data(),bad.size(),out.data(),out.size())==2&&out==before);
    }
    auto late=row;scalar(late,136+4*12,3e38f);require(reflex_terrain_query_draft(late.data(),late.size(),out.data(),out.size())==2&&out==before);
    const int rounding=std::fegetround();require(std::fesetround(FE_DOWNWARD)==0);require(reflex_terrain_query_draft(row.data(),row.size(),out.data(),out.size())==2&&out==before);require(std::fesetround(rounding)==0);
    auto absent=row;word(absent,16,0);word(absent,136,0x7fc00000);require(reflex_terrain_query_draft(absent.data(),absent.size(),out.data(),out.size())==0);require(word(out,0)==0&&word(out,4)==0);require(std::memcmp(out.data()+24,row.data()+32904,19200)==0);
    auto failed=row;word(failed,20,0);require(reflex_terrain_query_draft(failed.data(),failed.size(),out.data(),out.size())==0);require(word(out,0)==0&&word(out,4)==1&&word(out,8)==0&&word(out,20)==0);require(std::memcmp(out.data()+24,row.data()+32904,19200)==0);
    auto capacity=row;word(capacity,12,1);require(reflex_terrain_query_draft(capacity.data(),capacity.size(),out.data(),out.size())==0);require(word(out,0)==0&&word(out,8)==1);require(std::memcmp(out.data()+24,row.data()+32904,19200)==0);
    std::fprintf(stderr,"terrain contracts: analytical triangle/height/W order; %u arbitrary overlaps; malformed, nonfinite, late overflow and FP atomic rejects; empty/failure/capacity preservation pass\n",overlaps);return 0;
}
