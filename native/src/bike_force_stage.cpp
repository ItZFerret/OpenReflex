#include "reflex_core.h"
#include "fp32.h"
#include "bike_tire_internal.h"
#include "bike_powertrain_internal.h"
#include <cfenv>

namespace force_stage_draft {
using namespace bike_powertrain_draft;
using State=std::array<uint8_t,StateSize>;
template<class T> T read(const uint8_t* p,size_t offset){T value;std::memcpy(&value,p+offset,sizeof(value));return value;}
template<class T> void write(uint8_t* p,size_t offset,const T& value){std::memcpy(p+offset,&value,sizeof(value));}
struct StatusError { uint32_t status; };
void require(uint32_t status,const char*){if(status!=REFLEX_OK)throw StatusError{status};}
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
        // The pinned closure writes five integer booleans: each wheel+C0/C4
        // and S+8DA0. All other original non-stack writes are float stores.
        // S+5C30 is interpreter pointer plumbing, excluded by Memory::write.
        for(size_t word=0;word<tire.mem.written.size();++word)if(tire.mem.written[word]){
            const size_t offset=word*4;
            const bool flag=offset==0x8850||offset==0x8854||offset==0x89a0||offset==0x89a4||offset==0x8da0;
            if(flag){if(read<uint32_t>(tire.mem.state.data(),offset)>1)throw StatusError{REFLEX_NUMERIC_RANGE};}
            else if(!reflex_detail::finite(read<float>(tire.mem.state.data(),offset)))throw StatusError{REFLEX_NUMERIC_RANGE};
        }
        std::memcpy(data,tire.mem.state.data(),StateSize);
    }
    write(data,0x5c30,original_control_pointer);write(data,0x8e60,read<Vec>(data,0x780));
    State result;std::memcpy(result.data(),data,StateSize);return result;
}
} // namespace force_stage_draft

namespace {
struct SavedEnvironment {
    std::fenv_t value{};bool active=false;
    SavedEnvironment():active(std::fegetenv(&value)==0){}
    bool restore(){if(!active)return false;if(std::fesetenv(&value)!=0)return false;active=false;return true;}
    ~SavedEnvironment(){if(active)std::fesetenv(&value);}
};
bool span_valid(const void* pointer,size_t bytes){
    const auto address=reinterpret_cast<uintptr_t>(pointer);
    return address%4==0&&address<=UINTPTR_MAX-bytes;
}
}
static_assert(sizeof(reflex_bike_force_stage_input)==38988&&sizeof(reflex_bike_force_stage_output)==38656,"force-stage ABI sizes");
static_assert(offsetof(reflex_bike_force_stage_input,control_bits)==38656&&offsetof(reflex_bike_force_stage_input,axis_x_bits)==38912,"force-stage raw snapshot offsets");
static_assert(offsetof(reflex_bike_force_stage_input,dt_bits)==38976&&offsetof(reflex_bike_force_stage_input,x87_control_word)==38984,"force-stage explicit profile offsets");
uint32_t REFLEX_CALL reflex_bike_force_stage(const reflex_bike_force_stage_input* input,uint32_t input_size,
    reflex_bike_force_stage_output* output,uint32_t output_size){
    if(!input||!output)return REFLEX_NULL_ARGUMENT;
    if(input_size!=sizeof(*input)||output_size!=sizeof(*output))return REFLEX_ABI_MISMATCH;
    if(!span_valid(input,input_size)||!span_valid(output,output_size))return REFLEX_INVALID_INPUT;
    SavedEnvironment environment;if(!environment.active||!reflex_detail::supported_fp())return REFLEX_UNSUPPORTED_FP;
    reflex_bike_force_stage_input snapshot;std::memcpy(&snapshot,input,sizeof(snapshot));
    if(snapshot.conversion_flag>1||(snapshot.x87_control_word!=0x007f&&snapshot.x87_control_word!=0x027f))return REFLEX_INVALID_INPUT;
    const float dt=f32(snapshot.dt_bits);if(!reflex_detail::finite(dt)||dt<0)return REFLEX_INVALID_INPUT;
    // The internal original record tail is dt/side/profile, whereas the public
    // ABI exposes the actual raw conversion flag and x87 control word.
    std::array<uint8_t,RecordSize> record;std::memcpy(record.data(),&snapshot,sizeof(snapshot));
    const uint32_t zero=0,profile=snapshot.conversion_flag|(snapshot.x87_control_word==0x027f?2u:0u);
    std::memcpy(record.data()+38980,&zero,4);std::memcpy(record.data()+38984,&profile,4);
    try {
        const auto result=force_stage_draft::calculate(record.data());
        if(!environment.restore())return REFLEX_UNSUPPORTED_FP;
        std::memcpy(output,result.data(),result.size());return REFLEX_OK;
    } catch(const force_stage_draft::StatusError& error) {
        return error.status;
    } catch(const std::exception&) {
        // Bounded indexed domains, memory/program closure, final stored finite
        // values and resource failures never publish a partial state.
        return REFLEX_NUMERIC_RANGE;
    } catch(...) {
        return REFLEX_NUMERIC_RANGE;
    }
}
