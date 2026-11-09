// build_xml_stream_probe.cpp -- the oracle's probe for the XMLReader loader 008252c0 (core/game/build_xml_stream.c). The reference
// worlds load no document while racing (the loader runs from the tuning reload paths only when a pending stream [0x009e3100] is
// set), so a frame hook (Engine::Update 0084ed20) calls the ORIGINAL 008252c0 on prepared readers and streams: with the rx_build_xml
// ports installed in Verify mode the core runs first, then the original on the same state, per call.
//   REFLEX_PORT=rx_build_xml,rx_XmlStreamProbeHook REFLEX_PORT_VERIFY=1 RX_XML_PROBE=<file>[:<file>...] [RX_XML_FRAME=<n>]
//   [RX_XML_CUTS=1]  also every document cut short at 13 points (the failure exits of each stage)
// The stream is a MemoryInputStream (vtable 0x974d1c) built like the tuning receiver 004391a0 builds the pending one: 0x1c bytes
// {vtable, data, size, position 0, 0, not owned, last read 0}. Each document runs with the default allocator and with a reader
// allocator (a StdMemoryAllocator 0x9753b0, IMemoryAllocator vf1 / vf2), each with the allocator lock off and on ([0xda67d8]).
// All cases run on one snapshot of the guest memory and machine state, restored afterwards (the run continues on the untouched world).
#include "oracle.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
using namespace reflex_port;

namespace {
uint32_t g_frame = 0;
uint32_t rd(uint32_t a) { return rx_rd32(a); }
void wr(uint32_t a, uint32_t v) { rx_wr32(a, v); }

struct Pages {
    reflex_vm::Memory& memory;
    struct Page { uint32_t address; std::array<uint8_t, 4096> bytes; };
    std::vector<Page> saved;
    std::vector<uint8_t> was;
    explicit Pages(reflex_vm::Memory& m) : memory(m), was(1u << 20, 0) {
        for (uint32_t i = 0; i < (1u << 20); i++) if (m.mapped(i << 12)) {
            was[i] = 1; saved.push_back(Page{}); auto& p = saved.back(); p.address = i << 12;
            std::memcpy(p.bytes.data(), m.page(p.address), 4096);
        }
    }
    void restore() {
        for (auto& p : saved) std::memcpy(memory.page(p.address), p.bytes.data(), 4096);
        for (uint32_t i = 0; i < (1u << 20); i++) if (!was[i] && memory.mapped(i << 12)) memory.unmap(i << 12, 4096);
    }
};

std::vector<std::string> split(const char* s) {
    std::vector<std::string> out; std::string cur;
    for (const char* p = s; ; p++) {
        if (*p == ':' || *p == 0) { if (!cur.empty()) out.push_back(cur); cur.clear(); if (!*p) break; }
        else cur += *p;
    }
    return out;
}
}  // namespace

RX_PORT(0x0084ed20, XmlStreamProbeHook, "thunk", 0) {
    const char* files = std::getenv("RX_XML_PROBE");
    if (!files) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_XML_FRAME") ? uint32_t(std::atoi(std::getenv("RX_XML_FRAME"))) : 30;
    if (++g_frame != at) return reflex_vm::RUN_ORIGINAL;
    const bool cuts = std::getenv("RX_XML_CUTS") != nullptr;

    uint32_t regs[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
    std::memcpy(regs, m.r, sizeof regs); std::memcpy(xmm, m.xmm, sizeof xmm); std::memcpy(mmx, m.mmx, sizeof mmx);
    auto flags = m.flags; auto fpu = m.fpu; auto cw = m.fpu_cw, sw = m.fpu_sw; auto mxcsr = m.mxcsr; bool mmx_active = m.mmx_active;
    auto host_restore = snapshot_host_state(m);
    if (!host_restore) { std::fprintf(stderr, "[xmlprobe] ERROR no reversible host state\n"); return reflex_vm::RUN_ORIGINAL; }
    Pages pages(m.mem);
    const uint8_t lock_was = uint8_t(m.mem.read(0x00da67d8, 1));

    auto invoke = [&](uint32_t fn, uint32_t eax, uint32_t ecx, uint32_t esi, std::initializer_list<uint32_t> args) {
        rx_regs in{}, out{};
        for (unsigned i = 0; i < 8; i++) in.r[i] = regs[i];
        in.r[0] = eax; in.r[1] = ecx; in.r[6] = esi;
        std::vector<uint32_t> a(args);
        rx_call_original_ex(fn, &in, &out, int(a.size()), a.empty() ? nullptr : a.data());
        return out;
    };
    auto alloc = [&](uint32_t bytes) { return invoke(0x0078fbe0, 0, 0, 0, {bytes}).r[0]; };   /* ReflexAlignedAlloc */
    invoke(rd(0x00916014), 0, 0, 0, {0x00d61704});   /* InitializeCriticalSection of the allocator lock (the reset probe's setup) */

    unsigned cases = 0, loaded = 0, failed = 0;
    for (const std::string& path : split(files)) {
        std::ifstream input(path, std::ios::binary);
        if (!input.is_open()) { std::fprintf(stderr, "[xmlprobe] ERROR cannot read %s\n", path.c_str()); continue; }
        std::vector<uint8_t> doc((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        std::vector<uint32_t> sizes = {uint32_t(doc.size())};
        if (cuts && doc.size() > 0x24) {
            sizes.push_back(0x10);
            sizes.push_back(0x21);
            for (int k = 1; k < 10; k++) sizes.push_back(uint32_t(doc.size() * k / 10));
            sizes.push_back(uint32_t(doc.size() - 1));
            uint32_t h[8];
            std::memcpy(h, doc.data(), 0x20);
            const uint32_t version = h[1] & 0xffff, doc_flags = h[1] >> 16;
            if ((doc_flags & 2) != 0 && (doc_flags & 1) == 0) {   /* inside the external section's header and its offset table */
                const uint32_t ext = 0x20 + h[3] + h[4] + h[5] * (version < 1002 ? 8u : 12u) + h[6] * 0x20u;
                sizes.push_back(ext + 0x10);
                sizes.push_back(ext + 0x14 + 2);
            }
        }
        for (uint32_t size : sizes) {
            for (int custom = 0; custom < 2; custom++) {
                for (int lock = 0; lock < 2; lock++) {
                    rx_wr8(0x00da67d8, uint8_t(lock));
                    uint32_t allocator = 0;
                    if (custom) { allocator = alloc(4); wr(allocator, 0x009753b0); }   /* a StdMemoryAllocator */
                    const uint32_t reader = alloc(0x480);
                    for (uint32_t i = 0; i < 0x480; i += 4) wr(reader + i, 0xa5a5a5a5u);
                    invoke(0x00824e70, 0, 0, reader, {allocator});                    /* XMLReader_Construct (ESI = reader) */
                    const uint32_t bytes = alloc(size ? size : 1);
                    m.mem.write_bytes(bytes, doc.data(), size);
                    const uint32_t stream = alloc(0x1c);
                    wr(stream + 0x00, 0x00974d1c);   /* MemoryInputStream */
                    wr(stream + 0x04, bytes);
                    wr(stream + 0x08, size);
                    wr(stream + 0x0c, 0);
                    wr(stream + 0x10, 0);
                    wr(stream + 0x14, 0);
                    wr(stream + 0x18, 0);
                    const uint32_t ok = invoke(0x008252c0, 0, 0, 0, {reader, stream}).r[0];
                    cases++; if (ok) loaded++; else failed++;
                    std::fprintf(stderr, "[xmlprobe] %s size %u/%zu allocator %d lock %d -> %u (strings %d nodes %d values %d hash %08x pos %u)\n",
                                 path.c_str(), size, doc.size(), custom, lock, ok, int(rd(reader + 4)), int(rd(reader + 0x28)),
                                 int(rd(reader + 0x30)), rd(reader + 0x10), rd(stream + 0xc));
                }
            }
        }
    }
    rx_wr8(0x00da67d8, lock_was);
    pages.restore();
    host_restore();
    std::memcpy(m.r, regs, sizeof regs); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
    m.flags = flags; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
    std::fprintf(stderr, "[xmlprobe] frame %u: %u cases, %u loaded, %u failed\n", g_frame, cases, loaded, failed);
    return reflex_vm::RUN_ORIGINAL;
}
