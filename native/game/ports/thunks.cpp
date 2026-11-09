// The exe's import thunks (jmp dword ptr [IAT slot]): the native jumps to the slot's target (a stand-in or a ported CRT function),
// so not even these single instructions run as original code. Group "thunk".
#include <cstdint>
#include "../port.h"

namespace {
struct Thunk { uint32_t address, slot; };
const Thunk kThunks[] = {
#include "thunks.inc"
};
// direct jmp thunks to a thunk (target address, not a slot)
const Thunk kDirect[] = {{0x008a0500, 0x008aebc0}};   // KERNEL32.DLL::GetCurrentThreadId -> the IAT thunk
struct Register {
    Register() {
        for (const Thunk& t : kDirect) {
            const uint32_t target = t.slot;
            reflex_port::registry().push_back({t.address, "ImportThunkJmp", "thunk", [target](reflex_vm::Machine&) -> uint32_t { return target; }, 0});
        }
        for (const Thunk& t : kThunks) {
            const uint32_t slot = t.slot;
            reflex_port::registry().push_back({t.address, "ImportThunk", "thunk",
                [slot](reflex_vm::Machine& m) -> uint32_t { return uint32_t(m.mem.read(slot, 4)); }, 0});
        }
    }
} reg;
}  // namespace
