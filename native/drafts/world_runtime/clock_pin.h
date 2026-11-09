// Opt-in pinned frame clock for the oracle (native/game notes/clock-pin.md).
//
// REFLEX_FIXED_CLOCK=1       pin the frame clock at 60 Hz (166667 x 100 ns per frame)
// REFLEX_FIXED_CLOCK=<hz>    pin it at <hz> (e.g. 60, 120)
// REFLEX_CLOCK=<hz>          the same, as an alias
//
// Off (unset / empty / 0) keeps the runtime's original behaviour byte for byte: the
// boot advances virtual time by executed instructions, the world runtime uses the
// ticks its caller passes.  On, the frame boundary advances the virtual time by
// exactly one frame of this length, so Engine::UpdateTiming measures a steady frame
// and a run is independent of the host's load.
#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace reflex_vm {

// 100 ns units per frame, or 0 when the option is off.  60 Hz -> 166667
// (10,000,000 / 60 rounded), the counter's 10 MHz frequency.
inline uint64_t fixed_clock_ticks() {
    const char* v = std::getenv("REFLEX_FIXED_CLOCK");
    if (!v || !*v) v = std::getenv("REFLEX_CLOCK");
    if (!v || !*v || !std::strcmp(v, "0")) return 0;
    unsigned long hz = 60;                       // "1" = enable at the game's own 60 Hz cap
    char* end = nullptr;
    unsigned long x = std::strtoul(v, &end, 10);
    if (end && *end == 0 && x > 1) hz = x;
    if (hz < 1) hz = 60;
    if (hz > 100000) hz = 100000;
    return (10000000ull + hz / 2) / hz;          // 10,000,000 * 100 ns / hz
}

}  // namespace reflex_vm
