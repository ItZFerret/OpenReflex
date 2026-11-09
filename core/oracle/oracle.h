// oracle.h -- the core's game code checked against the original inside the emulator (development only; never in the product).
// The core's C sources are compiled a second time with RX_ORACLE into the reflex_boot CLI (native/game/tools/build_cli.sh): their
// memory accessors (src/rx_mem.h) go to the emulated machine's journaled memory. Each ported function gets a small adapter here
// that takes the original's arguments from the guest registers / stack, calls the core function and sets the result registers;
// registered as a native/game port in group "rx_<subsystem>", it runs under Verify mode exactly like the earlier ports:
//   native/game/tools/verify.sh <reflex_boot> rx_mech      -> per call: core result vs original, memory and registers.
// Adapters are ABI glue only: no game logic here.
#pragma once
#include "port.h"
#ifndef RX_ORACLE
#define RX_ORACLE 1
#endif
extern "C" {
#include "../game/rx_game.h"   /* + each adapter file includes its subsystem header */
}

namespace rx_oracle {
// Binds the core's memory accessors to this machine for the duration of a call (nested calls keep the binding).
struct Bind { reflex_vm::Machine* prev; explicit Bind(reflex_vm::Machine& m); ~Bind(); };
inline float f32(uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; }
inline uint32_t bits(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
}  // namespace rx_oracle

// RX_PORT(0x0085d270, Mechanism_IntegrateVelocities, "rx_mech", OUT_EAX | OUT_ECX) { ... return m.ret(4); }  (m = the machine)
#define RX_PORT(addr, name, group, outputs)                                                   \
    static uint32_t rxport_body_##name(reflex_vm::Machine& m);                                \
    static uint32_t rxport_##name(reflex_vm::Machine& m) {                                    \
        rx_oracle::Bind b_(m); const uint32_t s3 = m.r[3], s5 = m.r[5], s6 = m.r[6], s7 = m.r[7];  \
        const uint32_t pc_ = rxport_body_##name(m);  /* callee-saved EBX EBP ESI EDI as the original leaves them */ \
        m.r[3] = s3; m.r[5] = s5; m.r[6] = s6; m.r[7] = s7; return pc_; }                      \
    static reflex_port::Registrar rxreg_##name(addr, "rx_" #name, group, rxport_##name, outputs); \
    static uint32_t rxport_body_##name(reflex_vm::Machine& m)

// RX_PORT_RAW: no automatic EBX / EBP / ESI / EDI restore, for the rare LTCG function whose callers read a callee-saved register
// as a result (the adapter sets every register itself).
#define RX_PORT_RAW(addr, name, group, outputs)                                               \
    static uint32_t rxport_body_##name(reflex_vm::Machine& m);                                \
    static uint32_t rxport_##name(reflex_vm::Machine& m) { rx_oracle::Bind b_(m); return rxport_body_##name(m); } \
    static reflex_port::Registrar rxreg_##name(addr, "rx_" #name, group, rxport_##name, outputs); \
    static uint32_t rxport_body_##name(reflex_vm::Machine& m)
