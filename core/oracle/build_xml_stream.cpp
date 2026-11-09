// build_xml_stream.cpp -- oracle adapters for core/game/build_xml_stream.c (group rx_build_xml): the XMLReader's loader from a guest
// stream object 008252c0, CompressedInputStream::vf1 0080fb80, 007105c0. The reference worlds load no document during the race; the
// probe core/oracle/build_xml_stream_probe.cpp drives the loader on prepared streams.
#include "oracle.h"
extern "C" {
#include "../game/build_xml_stream.h"
}
using namespace reflex_port;

#define G "rx_build_xml"

// The string hash table's construction / insertion (core/game/guest_string_hash.c 0082ab20 / 008434f0, called by the loader) model
// the registers they leave and pass to the originals they call (the hash 005c3380 takes the key in EDX) through these hooks, on the
// machine of the outermost loader call (they have no adapter of their own: they are verified through 008252c0). Outside a loader
// call nothing is observed (the product build's #else: no effect, saved registers read 0). They were link seams in
// oracle_unclosed.cpp until the loader reached them.
namespace {
thread_local reflex_vm::Machine* g_observed = nullptr;
struct Observe {
    reflex_vm::Machine* previous;
    explicit Observe(reflex_vm::Machine& m) : previous(g_observed) { g_observed = &m; }
    ~Observe() { g_observed = previous; }
};
}  // namespace
extern "C" void rx_guest_hash_reg(unsigned i, uint32_t v) { if (g_observed && i < 8 && i != 4) g_observed->r[i] = v; }
extern "C" uint32_t rx_guest_hash_getreg(unsigned i) { return g_observed && i < 8 ? g_observed->r[i] : 0; }
// XMM(i, v, clear): lane 0 of xmm i = v; clear = the upper lanes zeroed (a movss load), else kept (cvtsi2ss / arithmetic)
extern "C" void rx_guest_hash_xmm(unsigned i, uint32_t v, unsigned clear) {
    if (!g_observed || i >= 8) return;
    reflex_vm::Xmm& x = g_observed->xmm[i];
    x.lo = clear ? uint64_t(v) : ((x.lo & 0xffffffff00000000ull) | v);
    if (clear) x.hi = 0;
}
// 008252c0 (stdcall reader, stream; RET 8) -> EAX = 1 loaded / 0 failed
RX_PORT(0x008252c0, XMLReader_LoadStream, G, OUT_EAX) {
    Observe o(m);
    m.r[0] = rx_xml_reader_load_stream(m.arg(0), m.arg(1));
    return m.ret(8);
}
// 0080fb80 CompressedInputStream::vf1 (ECX = the wrapper; RET 0)
RX_PORT(0x0080fb80, CompressedInputStream_Close, G, 0) {
    rx_compressed_stream_close(m.r[1]);
    return m.ret(0);
}
// 0080f920 (ESI = the wrapper, EAX = the source, ECX = the decompressor; stack the chunk size; RET 4) -> EAX
RX_PORT(0x0080f920, CompressedInputStream_Init, G, OUT_EAX) {
    m.r[0] = rx_compressed_stream_init(m.r[6], m.r[0], m.r[1], m.arg(0));
    return m.ret(4);
}
// 0080fc10 CompressedInputStream::vf2 (ECX = the wrapper; stack dst, count; RET 8) -> EAX
RX_PORT(0x0080fc10, CompressedInputStream_Read, G, OUT_EAX) {
    m.r[0] = rx_compressed_stream_read(m.r[1], m.arg(0), m.arg(1));
    return m.ret(8);
}
// 007105c0 (cdecl p; RET 0)
RX_PORT(0x007105c0, AlignedFreeLocked, G, 0) {
    rx_aligned_free_locked(m.arg(0));
    return m.ret(0);
}
