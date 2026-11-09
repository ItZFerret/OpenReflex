// Private in-process bridge to the same instruction draft as the file oracle.
#define REFLEX_BIKE_SLOT15_NO_MAIN
#include "bike_slot15_draft.cpp"
#include "bike_slot15_callable.h"

namespace {
struct SavedEnvironment {
    std::fenv_t value{};
    bool valid=std::fegetenv(&value)==0;
    ~SavedEnvironment(){if(valid)std::fesetenv(&value);}
};
}

extern "C" uint32_t reflex_draft_bike_slot15(const void *input,uint32_t input_size,
                                            void *output,uint32_t output_size) {
    using namespace bike_slot15_private;
    if(!input||!output||input_size!=RecordSize||output_size!=RecordSize)return 1;
    SavedEnvironment environment;
    if(!environment.valid||!reflex_detail::supported_fp())return 2;
    try {
        std::array<uint8_t,RecordSize> snapshot{};
        std::memcpy(snapshot.data(),input,RecordSize);
        Machine machine(snapshot.data());machine.run();
        std::array<uint8_t,RecordSize> result{};
        std::memcpy(result.data(),machine.mem.state.data(),StateSize);
        size_t cursor=StateSize;
        for(const auto& span:runtime_spans)for(size_t i=0;i<span.size;i++)
            result[cursor++]=machine.mem.byte(span.address+uint32_t(i));
        for(const auto& span:input_regions)for(size_t i=0;i<span.size;i++)
            result[cursor++]=machine.mem.byte(span.address+uint32_t(i));
        if(cursor+8!=RecordSize)return 3;
        std::memcpy(result.data()+cursor,snapshot.data()+cursor,8);
        std::memcpy(output,result.data(),RecordSize);
        return 0;
    } catch(...) {return 3;}
}
