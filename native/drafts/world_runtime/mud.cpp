// Mud build-up in the original world (mud.h; _re/notes/mud.md, _re/names/79_mud.tsv): a slot's mud record, read only.
#include "mud.h"
#include "vm.h"
#include <cstring>

namespace reflex_vm {

bool mud_state(Memory& m, uint32_t P, int32_t slot, MudState& out) {
    std::memset(&out, 0, sizeof out);
    auto u32 = [&](uint32_t a) { return m.mapped(a) ? uint32_t(m.read(a, 4)) : 0u; };
    out.slot = slot; out.record = -1;
    uint32_t V = P ? u32(P + 0x1ac) : 0;
    if (!V) return false;
    out.vehicle = V; out.rider = u32(V + 0x1a0);   // the rider whose constant overrides share the record (= [P+0x1b4])
    for (uint32_t k = 0; k < MUD_RECORD_COUNT; k++) {   // as MudRecord_Free 00408fe0 / MudRecord_Clear 00409320 find it: owner +0x30
        uint32_t R = MUD_RECORDS + k * MUD_RECORD_STRIDE;
        if (u32(R + 0x30) != V) continue;
        out.record = int32_t(k);
        m.read_bytes(R + 0x00, out.amount, 16);
        m.read_bytes(R + 0x10, out.wetness, 16);
        m.read_bytes(R + 0x20, out.color, 16);
        return true;
    }
    return false;
}

}  // namespace reflex_vm
