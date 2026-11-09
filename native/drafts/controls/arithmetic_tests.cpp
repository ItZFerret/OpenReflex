// Independent input/output adapter for exact arithmetic regression vectors.
#define main controls_draft_file_main
#include "controls_draft.cpp"
#undef main
int main(int argc,char** argv){
    if(argc!=3||!reflex_detail::supported_fp())return 2;
    try{
        std::ifstream input(argv[1],std::ios::binary);std::ofstream output(argv[2],std::ios::binary);
        std::array<uint8_t,24> row{};
        while(input.read(reinterpret_cast<char*>(row.data()),row.size())||input.gcount()){
            if(size_t(input.gcount())!=row.size())return 2;
            uint32_t op,precision;double a,b;std::memcpy(&op,row.data(),4);std::memcpy(&precision,row.data()+4,4);
            std::memcpy(&a,row.data()+8,8);std::memcpy(&b,row.data()+16,8);
            if(op>2||(precision!=24&&precision!=53))return 2;
            const double result=op==0?add_precision(a,b,precision):op==1?multiply_precision(a,b,precision):float_remainder(a,b);
            output.write(reinterpret_cast<const char*>(&result),8);
        }
        return input.eof()&&output?0:2;
    }catch(const std::exception& e){std::fprintf(stderr,"controls arithmetic: %s\n",e.what());return 2;}
}
