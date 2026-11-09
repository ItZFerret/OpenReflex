// build_pool.cpp -- oracle adapters for core/game/build_pool.c (group rx_build_pool): the Lua frealloc over the small-object pool
// and SimpleMemoryAllocator, verified per call inside the boot (REFLEX_PORT=rx_build_pool REFLEX_PORT_VERIFY=1 REFLEX_BOOT_SCHED=baton).
#include "oracle.h"
extern "C" {
#include "../game/build_pool.h"
}
using namespace reflex_port;

// 00743f00 (cdecl: ud, ptr, osize, nsize) -> EAX; the heap / pool functions it calls are the core's (checked with it)
RX_PORT(0x00743f00, LuaAlloc, "rx_build_pool", OUT_EAX) {
    m.r[0] = rx_lua_alloc(m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    if (rx_lua_alloc_slots.ud_written) m.mem.write(m.r[4] + 4, 4, rx_lua_alloc_slots.ud);        // [esp + 4]: ud's slot
    if (rx_lua_alloc_slots.nsize_written) m.mem.write(m.r[4] + 0x10, 4, rx_lua_alloc_slots.nsize); // [esp + 0x10]: nsize's slot
    return m.ret(0);
}
// SimpleMemoryAllocator::vf1 008236d0 (thiscall, ret 4) -> EAX
RX_PORT(0x008236d0, SimpleAllocatorAlloc, "rx_build_pool", OUT_EAX) { m.r[0] = rx_simple_allocator_alloc(m.r[1], m.arg(0)); return m.ret(4); }
// SimpleMemoryAllocator::vf3 00823700 (ECX)
RX_PORT(0x00823700, SimpleAllocatorReset, "rx_build_pool", 0) { rx_simple_allocator_reset(m.r[1]); return m.ret(0); }
