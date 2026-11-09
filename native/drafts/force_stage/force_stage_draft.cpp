// Private single-snapshot 0043e100 composition. No registered public ABI.
// Reuse the frozen tire implementation without changing its standalone source.
#define main tire_draft_standalone_main
#include "../tire/tire_draft.cpp"
#undef main
#include "../../src/bike_powertrain_draft.cpp"

namespace force_stage_draft {
using namespace bike_powertrain_draft;
using State=std::array<uint8_t,StateSize>;
template<class T> T read(const uint8_t* p,size_t offset){T value;std::memcpy(&value,p+offset,sizeof(value));return value;}
template<class T> void write(uint8_t* p,size_t offset,const T& value){std::memcpy(p+offset,&value,sizeof(value));}
void require(uint32_t status,const char* stage){if(status!=REFLEX_OK)throw std::runtime_error(std::string(stage)+" rejected: "+std::to_string(status));}
template<class T,size_t N> void gather(T (&out)[N],const uint8_t* data,const size_t (&offsets)[N]){
    for(size_t i=0;i<N;++i)out[i]=read<T>(data,offsets[i]);
}
template<class T,size_t N> void scatter(uint8_t* data,const T (&values)[N],const size_t (&offsets)[N]){
    for(size_t i=0;i<N;++i)write(data,offsets[i],values[i]);
}

State calculate(const uint8_t* input){
    if(!reflex_detail::supported_fp())throw std::runtime_error("unsupported native FP controls");
    std::vector<uint8_t> row(input,input+RecordSize);uint8_t* data=row.data();
    const uint8_t* control=data+StateSize;const uint8_t* globals=control+ControlSize;
    const Vec axis_y=read<Vec>(globals,16),numerator=read<Vec>(globals,32),g=read<Vec>(globals,48);
    const float dt=read<float>(globals,64);const uint32_t profile=read<uint32_t>(globals,72);
    if(profile>3||!reflex_detail::finite(dt))throw std::runtime_error("invalid force profile/dt");
    const uint32_t original_control_pointer=read<uint32_t>(data,0x5c30);
    if(read<uint32_t>(data,0x2374)==0){
        Arithmetic op;write(data,0x8aec,op.add(read<float>(data,0x8aec),dt));
        if(!op.valid)throw std::runtime_error("timer range");
        if(read<uint32_t>(data,0x8d98)==0){
            GearInput in;GearOutput out;
            in.orientation=read<Vec>(data,0x730);in.velocity=read<Vec>(data,0x100);
            constexpr size_t scalar_offsets[]={0x48ec,0x5014,0x50ec,0x8ae4,0x8ae8,0x8aec};gather(in.scalars,data,scalar_offsets);
            in.gate=read<uint32_t>(data,0x2eb0);in.gear=read<uint32_t>(data,0x8ac0);
            std::memcpy(in.ratios,data+0x8ac4,sizeof(in.ratios));in.curve=read<Curve>(data,0x4f74);
            const float throttle=read<float>(control,0);if(!reflex_detail::finite(throttle))throw std::runtime_error("nonfinite throttle");
            require(gear(in,out,throttle<=0),"gear");write(data,0x8ac0,out.gear);write(data,0x8aec,out.timer);
        }
        EngineInput in;EngineOutput out;
        constexpr size_t vector_offsets[]={0x730,0x160,0xd0,0x920,0x3a10,0x140,0x780,0x960,0x930,0x940,0x100};
        constexpr size_t scalar_offsets[]={0x5014,0x505c,0x8abc,0x52ac,0x8ab8,0x50a4,0x5264,0x50ec,0x3aac,0x3a64,0x8ad8,0x8adc,0x8ae0};
        gather(in.vectors,data,vector_offsets);gather(in.scalars,data,scalar_offsets);
        in.gear=read<uint32_t>(data,0x8ac0);std::memcpy(in.ratios,data+0x8ac4,sizeof(in.ratios));
        in.curves[0]=read<Curve>(data,0x4f74);in.curves[1]=read<Curve>(data,0x5134);
        in.throttle=read<float>(control,0);in.brake=read<float>(control,4);in.reaction=read<float>(control,0x60);in.mode=read<uint32_t>(control,0x84);
        in.g=g;in.numerator=numerator;in.axis_y=axis_y;in.dt=dt;in.profile=profile;
        require(engine(in,out),"engine");
        constexpr size_t vector_outputs[]={0x140,0x780,0x960,0x930,0x940},scalar_outputs[]={0x8abc,0x8ad8,0x8adc,0x8ae0};
        scatter(data,out.vectors,vector_outputs);scatter(data,out.scalars,scalar_outputs);
        bike_force_draft::TorqueInput torque_in{read<Vec>(data,0x730),read<Vec>(data,0x870),read<Vec>(data,0x780),read<Vec>(data,0x8c0),read<Vec>(data,0x960),read<float>(control,8),read<float>(control,12)};
        bike_force_draft::TorqueOutput torque_out;require(bike_force_draft::torque(torque_in,torque_out),"torques");
        write(data,0x780,torque_out.root_torque);write(data,0x8c0,torque_out.front_torque);write(data,0x960,torque_out.rear_torque);
    }
    {
        bike_force_draft::SuspensionInput in;bike_force_draft::SuspensionOutput out;
        constexpr size_t vector_offsets[]={0xf0,0x100,0x730,0x740,0x230,0x240,0x870,0x2d0,0x2e0,0x910,0x6e70,0x6e80,0x77d0,0x77e0,0x140,0x780,0x280,0x8c0,0x320,0x960};
        constexpr size_t scalar_offsets[]={0x3e5c,0x3f7c,0x3fc4,0x8aac,0x8ab0,0x8ab4,0x400c,0x4174,0x426c,0x8aa0,0x8aa4,0x8aa8};
        gather(in.vectors,data,vector_offsets);gather(in.scalars,data,scalar_offsets);in.g=g;
        require(bike_force_draft::suspension(in,out),"suspension");
        constexpr size_t vector_outputs[]={0x140,0x780,0x280,0x8c0,0x320,0x960,0x8f20,0x8f30},flag_outputs[]={0x89e8,0x89ec,0x8898,0x889c};
        scatter(data,out.vectors,vector_outputs);scatter(data,out.flags,flag_outputs);
    }
    if(read<uint32_t>(data,0x2374)==0){
        DampingInput in;DampingOutput out;
        constexpr size_t vector_offsets[]={0x100,0x2e0,0x240,0x8a40,0x8a50,0x730,0x910,0x870,0x140,0x780,0x320,0x960,0x280,0x8c0};
        constexpr size_t scalar_offsets[]={0x2f00,0x88a4,0x89f4,0x8870,0x89c0},flag_offsets[]={0x8890,0x89e0,0x2374},table_offsets[]={0x4934,0x4984,0x42fc,0x434c};
        gather(in.vectors,data,vector_offsets);gather(in.scalars,data,scalar_offsets);gather(in.flags,data,flag_offsets);
        // These are live reads after suspension, including the table[-1] word.
        for(unsigned i=0;i<4;++i){std::memcpy(in.tables[i],data+table_offsets[i],80);in.table_preceding[i]=read<float>(data,table_offsets[i]-4);}
        in.control_force=read<Vec>(control,0x20);in.g=g;
        // Parent/tire bit2→CW027f; standalone damping bit2→CW007f.
        in.profile=profile^2;require(damping(in,out),"damping");
        constexpr size_t outputs[]={0x140,0x780,0x320,0x960,0x280,0x8c0};scatter(data,out.vectors,outputs);
    }
    for(size_t offset:{0x8f00,0x8f10,0x8ee0,0x8ef0})write(data,offset,g);
    for(uint32_t side=0;side<2;++side){
        write(data,StateSize+ControlSize+68,side);Machine tire(data);tire.run();
        std::memcpy(data,tire.mem.state.data(),StateSize);
    }
    write(data,0x5c30,original_control_pointer);write(data,0x8e60,read<Vec>(data,0x780));
    State result;std::memcpy(result.data(),data,StateSize);return result;
}
} // namespace force_stage_draft

int main(int argc,char** argv){
    if(argc!=3){std::fprintf(stderr,"usage: force-stage-draft input.bin output.bin\n");return 2;}
    try{
        std::ifstream input(argv[1],std::ios::binary);std::ofstream output(argv[2],std::ios::binary);
        if(!input||!output)throw std::runtime_error("file open");std::array<uint8_t,RecordSize> row;uint64_t count=0;
        while(input.read(reinterpret_cast<char*>(row.data()),row.size())||input.gcount()){
            if(size_t(input.gcount())!=row.size()||++count>10000)throw std::runtime_error("partial/excess row");
            const auto result=force_stage_draft::calculate(row.data());output.write(reinterpret_cast<const char*>(result.data()),result.size());
            if(!output)throw std::runtime_error("output write");
        }
        if(!input.eof())throw std::runtime_error("input failure");output.close();if(!output)throw std::runtime_error("output close");
        std::fprintf(stderr,"force stage native draft: %llu cases\n",count);return 0;
    }catch(const std::exception& error){std::fprintf(stderr,"force stage draft: %s\n",error.what());return 2;}
}
