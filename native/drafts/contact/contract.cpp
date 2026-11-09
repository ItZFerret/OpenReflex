#include <array>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cfenv>
extern "C" int reflex_contact_draft(unsigned,const void*,size_t,void*,size_t);
static void require(bool yes){if(!yes){std::fprintf(stderr,"contact contract failed\n");std::exit(1);}}
static void word(std::vector<uint8_t>& b,size_t at,uint32_t w){std::memcpy(b.data()+at,&w,4);}
static void vec(std::vector<uint8_t>& b,size_t at,float x,float y,float z,float w){float v[]={x,y,z,w};std::memcpy(b.data()+at,v,16);}
static std::vector<uint8_t> input(unsigned mode){
    std::vector<uint8_t> b(mode?464:19332,0);
    if(!mode){vec(b,4,0,0,0,1);vec(b,36,.5f,.5f,.5f,.5f);vec(b,52,3,4,5,6);vec(b,68,7,8,9,10);vec(b,84,1,0,0,0);vec(b,100,1,1,1,1);}
    else{std::memset(b.data(),0xa5,336);vec(b,16,.5f,.5f,.5f,.5f);vec(b,352,0,1,0,0);vec(b,368,0,-.25f,0,0);vec(b,384,0,0,0,1);vec(b,400,0,0,0,1);vec(b,416,1,0,0,0);vec(b,432,1,1,1,1);}
    return b;
}
int main(){
    unsigned overlaps=0;
    for(unsigned mode=0;mode<2;mode++){
        auto row=input(mode);const size_t size=mode?336:36;std::vector<uint8_t> expected(size,0x5a);
        require(reflex_contact_draft(mode,row.data(),row.size(),expected.data(),size)==0);
        if(!mode){require(std::memcmp(expected.data()+4,row.data()+52,32)==0);require(expected[0]==0);}
        else{const float radial[]={0,-.5f,0,0};require(std::memcmp(expected.data()+0x70,radial,16)==0);std::array<bool,336> allowed{};for(unsigned off:{0x30,0x40,0x50,0x60,0x70,0x90,0x120,0x130,0x140})for(unsigned j=0;j<16;j++)allowed[off+j]=true;for(unsigned j=0;j<336;j++)if(!allowed[j])require(expected[j]==row[j]);}
        for(int displacement:{-64,-4,0,1,4,16,64,300,int(row.size()-4)}){
            std::vector<uint8_t> storage(row.size()+1024,0xcc);uint8_t *source=storage.data()+512;std::memcpy(source,row.data(),row.size());
            uint8_t *destination=source+displacement;auto before=storage;
            require(reflex_contact_draft(mode,source,row.size(),destination,size)==0);require(std::memcmp(destination,expected.data(),size)==0);
            for(size_t j=0;j<storage.size();j++)if(storage.data()+j<destination||storage.data()+j>=destination+size)require(storage[j]==before[j]);overlaps++;
        }
        std::vector<uint8_t> result(size,0x6b),before=result;
        require(reflex_contact_draft(mode,nullptr,row.size(),result.data(),size)==1&&result==before);
        require(reflex_contact_draft(mode,row.data(),row.size()-1,result.data(),size)==1&&result==before);
        require(reflex_contact_draft(mode,row.data(),row.size(),result.data(),size-1)==1&&result==before);
        require(reflex_contact_draft(2,row.data(),row.size(),result.data(),size)==1&&result==before);
        auto bad=row;word(bad,mode?16:4,0x7fc00000);require(reflex_contact_draft(mode,bad.data(),bad.size(),result.data(),size)==2&&result==before);
        bad=row;if(!mode)word(bad,0,401);else std::memcpy(bad.data()+368,bad.data()+336,16);
        require(reflex_contact_draft(mode,bad.data(),bad.size(),result.data(),size)==2&&result==before);
        const int rounding=std::fegetround();require(std::fesetround(FE_DOWNWARD)==0);require(reflex_contact_draft(mode,row.data(),row.size(),result.data(),size)==2&&result==before);require(std::fesetround(rounding)==0);
    }
    std::fprintf(stderr,"contact contracts: analytical outputs, %u overlaps, malformed/nonfinite/late/FP atomic rejects pass\n",overlaps);return 0;
}
