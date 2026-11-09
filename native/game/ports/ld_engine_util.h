// ld_engine_util.h: helpers of the ld_engine loading ports (native/game/notes/ld_engine.md).
#pragma once
#include <cstdint>
#include "lift_rt.h"

// LIFT_CALL without the nested 2e9-instruction budget of Machine::call_function (same pushes, same run, same return-address
// slot handling): for calls of a thread procedure whose callees run longer than 2e9 instructions on one thread (the Database
// loader's package loads inflate up to ~4.9 G instructions). The boot runs guest threads with UINT64_MAX / 4 itself
// (boot.cpp); this matches it. (notes/loading.md section 3 item 3: when Guest::call gets that budget in the boot, this goes.)
#define LD_CALL_UNBOUNDED(target, ra)                                                                                      \
    do { uint32_t t_ = (target); LIFT_SYNC_OUT(); const uint64_t b_ = m.call_budget; m.call_budget = UINT64_MAX / 4;       \
         try { m.call_at(t_, (ra)); } catch (...) { m.call_budget = b_; throw; } m.call_budget = b_; LIFT_SYNC_IN(); } while (0)
