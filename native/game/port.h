// The native game (the Open Unleashed way): MXReflex.exe's code rewritten as native C++, one original function at a time.
//
// Each ported function is registered at its original address. Installed into a running world (libreflexworld's Machine), the
// native replaces the original there, so the game keeps running while it is converted piece by piece:
//   Mode::On      the native runs instead of the original (the port in use).
//   Mode::Verify  every call runs both on the same entry state: the native first, then the original, which is kept. Their memory
//                 writes (page journal) and output registers are compared; differences are counted and reported per function.
//                 So the reference stays the original game while every port is checked on real gameplay.
// Natives see the original ABI: arguments on the guest stack / registers (Machine::arg, r[]), results in registers, and return the
// next pc (Machine::ret). Inside a native, other original functions are called with Machine::call_function (they run ported when
// they are, original otherwise).
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "vm.h"

namespace reflex_port {

// Registers a port compares in Verify mode besides memory, ESP and the callee-saved EBX / EBP / ESI / EDI (always).
enum Outputs : uint32_t {
    OUT_EAX = 1u << 0, OUT_ECX = 1u << 1, OUT_EDX = 1u << 2,
    OUT_XMM0 = 1u << 8,              // OUT_XMM0 << i for xmm i
    OUT_XMM_ALL = 0xffu << 8,
    OUT_FPU = 1u << 16,              // the x87 stack (depth and values)
    OUT_FLAGS = 1u << 17,            // arithmetic flags (CF ZF SF OF PF)
    OUT_ESP_BELOW = 1u << 18,        // not an output: the function returns with ESP below its entry (frame-setup helpers such as
                                     // __alloca_probe, __SEH_prolog4): Verify stops the original at the return address whatever ESP
};

struct Entry {
    uint32_t address;
    const char* name;
    const char* group;               // subsystem (math, bike, rider, terrain, ...): the unit REFLEX_PORT selects
    reflex_vm::Native fn;
    uint32_t outputs;
};
std::vector<Entry>& registry();
struct Registrar { Registrar(uint32_t address, const char* name, const char* group, reflex_vm::Native fn, uint32_t outputs); };

enum class Mode { Off, On, Verify };
// Installs the selected ports into the machine (Mode::Off removes them again: the originals run). filter: "all", or a comma list of
// groups / names / hex addresses (a leading '-' excludes). Returns the number installed / removed.
int install(reflex_vm::Machine& m, Mode mode, const std::string& filter = "all");
// From the environment: REFLEX_PORT=<filter> (unset: none) and REFLEX_PORT_VERIFY=1. Returns the number installed.
int install_from_env(reflex_vm::Machine& m);

// Host-side state of the emulator that the original code changes through its stand-ins (heap bookkeeping, the virtual clock, captured
// audio commands, deformation, worker switches ...). Verify must undo it with the guest memory after the native run, so the
// original runs from the same state. The runtime registers a snapshot function per machine; it returns the matching restore, which
// Verify calls it again (the snapshot's state once more) when the original run would switch threads (the boot: WouldSwitch).
using HostSnapshot = std::function<std::function<void()>()>;
void set_host_state(reflex_vm::Machine& m, HostSnapshot snapshot);
// Development probes that replay allocating setup routines must restore the
// runtime's allocator and other host state together with their guest snapshot.
std::function<void()> snapshot_host_state(reflex_vm::Machine& m);

// A runtime native (stand-in, skip) at a port's address. runtime_native(m, a, fn) installs fn there in place of the port (fn may
// fall through to installed(m, a) when it has nothing to do); runtime_native(m, a, nullptr) removes it, putting the port (if one is
// installed) back. Ports installed after a runtime native leave it in place.
const reflex_vm::Native* installed(const reflex_vm::Machine& m, uint32_t address);   // nullptr: none
void runtime_native(reflex_vm::Machine& m, uint32_t address, reflex_vm::Native native);

// unverifiable: Verify calls whose native would have switched guest threads (a wait that blocks, a yield, a poll that fails) or changed
// host state the boot cannot take back (file writes, ...): the original ran instead, unverified (Machine::no_switch, notes/loading.md).
struct Stats { uint64_t calls = 0, verified = 0, mismatches = 0, errors = 0, unverifiable = 0; std::string first; };
const Stats& stats(uint32_t address);
// One line per installed port: calls, verified, mismatches and the first difference.
std::string report();
// Mismatches over all ports since install.
uint64_t total_mismatches();

}  // namespace reflex_port

// REFLEX_PORT_FN(0x0042c240, Vec4Sin_RangeReduced, "math", OUT_XMM_ALL) { ...; return m.ret(0); }
#define REFLEX_PORT_FN(addr, name, group, outputs) \
    static uint32_t port_##name(reflex_vm::Machine& m); \
    static reflex_port::Registrar reg_##name(addr, #name, group, port_##name, outputs); \
    static uint32_t port_##name(reflex_vm::Machine& m)
