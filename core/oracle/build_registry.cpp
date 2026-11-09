// build_registry.cpp -- oracle adapters for unit 12a's Registry (core/game/build_registry.c; added by unit 10, group rx_build10):
// the virtuals the startup and race code call and the pointer-key hash, verified per call in a baton boot (REFLEX_PORT=rx_build10
// REFLEX_PORT_VERIFY=1). ABI glue only (thiscall registry; stdcall arguments).
#include "oracle.h"
extern "C" {
#include "../game/build_registry.h"
}
using namespace reflex_port;
typedef uint32_t (*reg3_t)(rx_addr, uint32_t, uint32_t, uint32_t);
static uint32_t call3(uint32_t fn, reflex_vm::Machine& m) { return RX_FN(fn, reg3_t)(m.r[1], m.arg(0), m.arg(1), m.arg(2)); }
// the readers return LeaveCriticalSection's EAX (no caller reads it): no register output
#define G "rx_build10"
RX_PORT(0x008241f0, RegistryFind, G, OUT_EAX) { m.r[0] = rx_registry_vf1_find(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x00824580, RegistryBools, G, 0) { call3(0x00824580, m); return m.ret(0xc); }
RX_PORT(0x00824640, RegistryFloats, G, 0) { rx_registry_vf17_floats(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x008245c0, RegistryInts, G, 0) { call3(0x008245c0, m); return m.ret(0xc); }
RX_PORT(0x00824540, RegistryString, G, 0) { rx_registry_vf20_string(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x00824700, RegistryVec3, G, 0) { call3(0x00824700, m); return m.ret(0xc); }
RX_PORT(0x0082ac10, RegistryHashFind, G, OUT_EAX) { m.r[0] = rx_registry_hash_find(m.r[6], m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0082ac80, RegistryHashInsert, G, 0) { rx_registry_hash_insert(m.r[7], m.arg(0), m.arg(1)); return m.ret(8); }
