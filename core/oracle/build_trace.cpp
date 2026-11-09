// build_trace.cpp -- development tracing of the original's asset system (unit asset_system, core/notes/construction.md): logs every
// Database operation of a boot (database / package loads and unloads, each asset load through Database::vf30 with its serial, asset
// releases, AssetIO registrations, the async requests with their callers) and lets the original run (RUN_ORIGINAL: nothing changes).
// Registered only when REFLEX_BUILD_TRACE=<file> is set at start (so REFLEX_PORT=all / Verify runs of other agents never see it);
// install with REFLEX_PORT=rx_build_trace (On mode). Not part of the product.
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <string>
using namespace reflex_port;

namespace {
FILE* out() {
    static FILE* f = [] { const char* p = std::getenv("REFLEX_BUILD_TRACE"); return p && *p ? std::fopen(p, "w") : stderr; }();
    return f;
}
std::string str(reflex_vm::Machine& m, uint32_t a, int max = 0x104) {
    std::string s;
    if (a < 0x10000) return "<" + std::to_string(a) + ">";
    for (int i = 0; i < max; i++) { uint8_t c = uint8_t(m.mem.read(a + uint32_t(i), 1)); if (!c) break; s += (c >= 0x20 && c < 0x7f) ? char(c) : '?'; }
    return s;
}
std::string tag(reflex_vm::Machine& m, uint32_t a) {   // an 8-byte inline type tag
    std::string s;
    for (int i = 0; i < 8; i++) { uint8_t c = uint8_t(m.mem.read(a + uint32_t(i), 1)); if (!c) break; s += (c >= 0x20 && c < 0x7f) ? char(c) : '?'; }
    return s;
}
uint32_t w(reflex_vm::Machine& m, uint32_t a) { return uint32_t(m.mem.read(a, 4)); }
uint32_t sp(reflex_vm::Machine& m, int k) { return w(m, m.r[4] + 4u * uint32_t(k)); }   // [esp + 4k]
void line(reflex_vm::Machine& m, const std::string& s) { std::fprintf(out(), "%llu %s\n", (unsigned long long)m.steps, s.c_str()); std::fflush(out()); }
char buf[2048];

// the package record (0xa0): +0 name, +0x9c its database record (+0 name)
std::string pkg(reflex_vm::Machine& m, uint32_t p) {
    if (p == 0xffffffffu) return "-";
    if (p < 0x10000) return "?";
    return str(m, w(m, p + 0x9c), 0x40) + "/" + str(m, p, 0x40);
}

uint32_t db_load(reflex_vm::Machine& m) {   // Fn_0084b0a0: EDX name, [esp+8] dir, [esp+0xc], [esp+0x10]
    std::snprintf(buf, sizeof buf, "DBLOAD %s dir=%s p5=%x p6=%x", str(m, m.r[2]).c_str(), str(m, sp(m, 2)).c_str(), sp(m, 3), sp(m, 4));
    line(m, buf); return reflex_vm::RUN_ORIGINAL;
}
uint32_t db_unload(reflex_vm::Machine& m) {   // FUN_0084baf0: [esp+8] name
    std::snprintf(buf, sizeof buf, "DBUNLOAD %s", str(m, sp(m, 2)).c_str()); line(m, buf); return reflex_vm::RUN_ORIGINAL;
}
uint32_t pkg_load(reflex_vm::Machine& m) {   // Fn_0084bc70: [esp+8] database name, [esp+0xc] package name, [esp+0x10]
    std::snprintf(buf, sizeof buf, "PKGLOAD %s/%s p=%x", str(m, sp(m, 2)).c_str(), str(m, sp(m, 3)).c_str(), sp(m, 4));
    line(m, buf); return reflex_vm::RUN_ORIGINAL;
}
uint32_t pkg_unload(reflex_vm::Machine& m) {   // FUN_0084c550: EDX database name, ECX package name
    std::snprintf(buf, sizeof buf, "PKGUNLOAD %s/%s", str(m, m.r[2]).c_str(), str(m, m.r[1]).c_str()); line(m, buf); return reflex_vm::RUN_ORIGINAL;
}
uint32_t asset_load(reflex_vm::Machine& m) {   // Database::vf30 (ECX db; handle, stream, p3, package record or -1)
    const uint32_t db = m.r[1], h = sp(m, 1);
    std::snprintf(buf, sizeof buf, "ASSET %s %s h=%08x vt=%08x has=%08x pkg=%s p3=%x serial=%u", str(m, h + 4, 0x40).c_str(),
                  tag(m, db + 0xd4).c_str(), h, w(m, h), w(m, h + 0x44), pkg(m, sp(m, 4)).c_str(), sp(m, 3), w(m, db + 0xe8));
    line(m, buf); return reflex_vm::RUN_ORIGINAL;
}
uint32_t asset_new(reflex_vm::Machine& m) {   // Database::vf23 (name, asset)
    const uint32_t a = sp(m, 2);
    std::snprintf(buf, sizeof buf, "NEWASSET %s asset=%08x vt=%08x ret=%08x", str(m, sp(m, 1), 0x40).c_str(), a, a ? w(m, a) : 0, sp(m, 0));
    line(m, buf); return reflex_vm::RUN_ORIGINAL;
}
uint32_t asset_stream(reflex_vm::Machine& m) {   // Database::vf21 (p1, p2)
    std::snprintf(buf, sizeof buf, "STREAMASSET p1=%08x tag=%s p2=%08x ret=%08x", sp(m, 1), tag(m, sp(m, 1) + 0x40).c_str(), sp(m, 2), sp(m, 0));
    line(m, buf); return reflex_vm::RUN_ORIGINAL;
}
uint32_t asset_release(reflex_vm::Machine& m) {   // Database::vf31 (handle, package record)
    const uint32_t h = sp(m, 1);
    std::snprintf(buf, sizeof buf, "RELEASE %s h=%08x vt=%08x pkg=%s", str(m, h + 4, 0x40).c_str(), h, w(m, h), pkg(m, sp(m, 2)).c_str());
    line(m, buf); return reflex_vm::RUN_ORIGINAL;
}
uint32_t handle_release(reflex_vm::Machine& m) {   // Database::vf32 (handle)
    const uint32_t h = sp(m, 1);
    std::snprintf(buf, sizeof buf, "HRELEASE %s h=%08x vt=%08x refs=%d", str(m, h + 4, 0x40).c_str(), h, w(m, h), int32_t(w(m, h + 0x48)));
    line(m, buf); return reflex_vm::RUN_ORIGINAL;
}
uint32_t io_register(reflex_vm::Machine& m) {   // Database::vf24 (io, tag lo, tag hi)
    std::snprintf(buf, sizeof buf, "REGISTER %s io=%08x vt=%08x", tag(m, m.r[4] + 8).c_str(), sp(m, 1), w(m, sp(m, 1))); line(m, buf); return reflex_vm::RUN_ORIGINAL;
}
uint32_t io_unregister(reflex_vm::Machine& m) {   // Database::vf25
    std::snprintf(buf, sizeof buf, "UNREGISTER %s", tag(m, m.r[4] + 4).c_str()); line(m, buf); return reflex_vm::RUN_ORIGINAL;
}
template <int K> uint32_t request(reflex_vm::Machine& m) {   // the async requests: vf4 / vf5 / vf15 / vf16 (load), vf17 / vf6 (unload), vf14
    static const char* names[] = {"vf4-loadpkg-wait", "vf5-loadpkg", "vf15-loaddb-wait", "vf16-loaddb", "vf17-unloadpkg", "vf6-unloaddb", "vf14-callback"};
    std::snprintf(buf, sizeof buf, "REQ %s a=%s b=%s c=%x ret=%08x", names[K], str(m, sp(m, 1)).c_str(), K == 5 || K == 6 ? "" : str(m, sp(m, 2)).c_str(),
                  sp(m, 3), sp(m, 0));
    line(m, buf); return reflex_vm::RUN_ORIGINAL;
}

uint32_t cache_enable(reflex_vm::Machine& m) {   // Database::vf34 (ECX db): the background cache on
    std::snprintf(buf, sizeof buf, "CACHEON ret=%08x", sp(m, 0)); line(m, buf); return reflex_vm::RUN_ORIGINAL;
}
uint32_t cache_add(reflex_vm::Machine& m) {   // Database::vf38 (ECX db; a, b): one background cache entry
    std::snprintf(buf, sizeof buf, "CACHEADD a=%s b=%s on=%u ret=%08x", str(m, sp(m, 1)).c_str(), str(m, sp(m, 2)).c_str(), w(m, m.r[1] + 0x82b88), sp(m, 0));
    line(m, buf); return reflex_vm::RUN_ORIGINAL;
}

uint32_t pc_package(reflex_vm::Machine& m) {   // FUN_00543b30 (EAX cache; package name, flag): the package cache's request
    std::snprintf(buf, sizeof buf, "PCPKG %s flag=%x ret=%08x", str(m, sp(m, 1)).c_str(), sp(m, 2), sp(m, 0)); line(m, buf); return reflex_vm::RUN_ORIGINAL;
}
uint32_t pc_database(reflex_vm::Machine& m) {   // FUN_00543750 (ESI database name): the package cache's database request
    std::snprintf(buf, sizeof buf, "PCDB %s ret=%08x", str(m, m.r[6]).c_str(), sp(m, 0)); line(m, buf); return reflex_vm::RUN_ORIGINAL;
}

struct Reg {
    Reg() {
        if (!std::getenv("REFLEX_BUILD_TRACE")) return;
        const char* g = "rx_build_trace";
        new Registrar(0x0084b0a0, "trace_db_load", g, db_load, 0);
        new Registrar(0x0084baf0, "trace_db_unload", g, db_unload, 0);
        new Registrar(0x0084bc70, "trace_pkg_load", g, pkg_load, 0);
        new Registrar(0x0084c550, "trace_pkg_unload", g, pkg_unload, 0);
        new Registrar(0x0084ad10, "trace_asset_load", g, asset_load, 0);
        new Registrar(0x0084aa10, "trace_asset_new", g, asset_new, 0);
        new Registrar(0x0084a930, "trace_asset_stream", g, asset_stream, 0);
        new Registrar(0x0084af30, "trace_asset_release", g, asset_release, 0);
        new Registrar(0x0084af90, "trace_handle_release", g, handle_release, 0);
        new Registrar(0x0084ab30, "trace_io_register", g, io_register, 0);
        new Registrar(0x0084abb0, "trace_io_unregister", g, io_unregister, 0);
        new Registrar(0x0084b020, "trace_req_vf4", g, request<0>, 0);
        new Registrar(0x0084b070, "trace_req_vf5", g, request<1>, 0);
        new Registrar(0x0084a7f0, "trace_req_vf15", g, request<2>, 0);
        new Registrar(0x0084a7c0, "trace_req_vf16", g, request<3>, 0);
        new Registrar(0x0084cde0, "trace_req_vf17", g, request<4>, 0);
        new Registrar(0x0084cd30, "trace_req_vf6", g, request<5>, 0);
        new Registrar(0x0084c0f0, "trace_req_vf14", g, request<6>, 0);
        new Registrar(0x0084cf10, "trace_cache_enable", g, cache_enable, 0);
        new Registrar(0x0084cf50, "trace_cache_add", g, cache_add, 0);
        new Registrar(0x00543b30, "trace_pc_package", g, pc_package, 0);
        new Registrar(0x00543750, "trace_pc_database", g, pc_database, 0);
    }
} reg_;
}  // namespace
