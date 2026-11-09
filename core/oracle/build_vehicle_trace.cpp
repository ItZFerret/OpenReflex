// build_vehicle_trace.cpp -- development tracing for construction unit 6 (vehicles, core/notes/construction.md): logs the calls of
// a list of original functions while the boot is inside one of a set of "windows" (an entry address and the return address of its
// call site), so the dynamic call tree under VehicleFactory_CreateVehicle (and the spec / package requests) can be rebuilt (depth from
// ESP). The original always runs (RUN_ORIGINAL): the boot is unchanged.
// Registered only when REFLEX_VTRACE=<out file> is set at start; install with REFLEX_PORT=rx_build_vtrace (On mode).
//   REFLEX_VTRACE_FNS=<file>   hex addresses, one per line (the functions logged)
//   REFLEX_VTRACE_WIN=<entry>:<ret>,...   windows: logging is on between a call of <entry> and the return to <ret> (nesting counted)
// Line: steps esp addr ret ecx eax edx esi edi [esp+4] [esp+8] [esp+0xc] [esp+0x10]   (or "WIN+ / WIN-" marks). Not part of the product.
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>
#include <map>
#include <string>
#include <zlib.h>
using namespace reflex_port;

namespace {
FILE* vout;
int depth;   // windows open
uint8_t* rmap;   // REFLEX_VTRACE_READS: the read map (1 bit per 16-byte granule) of the windows (run without the JIT)
void rmap_on(reflex_vm::Machine& m) { if (rmap) m.mem.read_map = rmap; }
void rmap_off(reflex_vm::Machine& m) {
    if (!rmap) return;
    m.mem.read_map = nullptr;
    if (const char* p = std::getenv("REFLEX_VTRACE_READS")) { FILE* f = std::fopen(p, "wb"); if (f) { std::fwrite(rmap, 1, 1u << 25, f); std::fclose(f); } }
}
uint32_t w(reflex_vm::Machine& m, uint32_t a) { return uint32_t(m.mem.read(a, 4)); }

// an RXWI image of the whole memory now (every mapped page; header words zero except the magic)
void snapshot(reflex_vm::Machine& m, const char* path) {
    std::vector<uint32_t> pages;
    for (uint32_t pg = 0; pg < 0x100000u; pg++) if (m.mem.mapped(pg << 12)) pages.push_back(pg << 12);
    std::vector<uint8_t> out(80 + 12 * pages.size()), blob, c(compressBound(4096)); uint8_t buf[4096];
    uint32_t hdr[4] = {0x49575852u, 1, uint32_t(pages.size()), 0}; std::memcpy(out.data(), hdr, 16);
    size_t k = 0, base = out.size();
    for (uint32_t a : pages) {
        m.mem.read_block(a, buf, 4096);
        uint32_t e[3] = {a, 0, 0}; bool z = true; for (int i = 0; i < 4096 && z; i++) z = buf[i] == 0;
        if (!z) { uLongf n = uLongf(c.size()); compress2(c.data(), &n, buf, 4096, 1); e[1] = uint32_t(base + blob.size());
                  if (n >= 4096) { blob.insert(blob.end(), buf, buf + 4096); e[2] = 4096; } else { blob.insert(blob.end(), c.data(), c.data() + n); e[2] = uint32_t(n); } }
        std::memcpy(out.data() + 80 + 12 * k++, e, 12);
    }
    out.insert(out.end(), blob.begin(), blob.end());
    FILE* f = std::fopen(path, "wb"); if (f) { std::fwrite(out.data(), 1, out.size(), f); std::fclose(f); }
    std::fprintf(vout, "%llu SNAP %s %zu pages\n", (unsigned long long)m.steps, path, pages.size()); std::fflush(vout);
}

uint32_t on_call(reflex_vm::Machine& m, uint32_t addr) {
    if (depth > 0) {
        const uint32_t sp = m.r[4];
        std::fprintf(vout, "%llu %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x\n", (unsigned long long)m.steps, sp, addr,
                     w(m, sp), m.r[1], m.r[0], m.r[2], m.r[6], m.r[7], w(m, sp + 4), w(m, sp + 8), w(m, sp + 12), w(m, sp + 16));
    }
    return reflex_vm::RUN_ORIGINAL;
}

struct Reg {
    Reg() {
        const char* o = std::getenv("REFLEX_VTRACE");
        if (!o || !*o) return;
        vout = std::fopen(o, "w");
        if (!vout) return;
        const char* g = "rx_build_vtrace";
        if (std::getenv("REFLEX_VTRACE_READS")) rmap = static_cast<uint8_t*>(std::calloc(1u << 25, 1));
        std::set<uint32_t> fns, wins;
        if (const char* wl = std::getenv("REFLEX_VTRACE_WIN")) {
            for (const char* p = wl; *p;) {
                char* e; uint32_t a = uint32_t(std::strtoul(p, &e, 16)); if (*e != ':') break;
                uint32_t r = uint32_t(std::strtoul(e + 1, &e, 16));
                wins.insert(a);
                static char names[256][32]; static int nn;
                std::snprintf(names[nn], 32, "vtrace_ret_%08x", r);
                new Registrar(r, names[nn++], g, [r](reflex_vm::Machine& m) -> uint32_t {
                    if (depth > 0) { depth--; std::fprintf(vout, "%llu WIN- %08x %08x\n", (unsigned long long)m.steps, r, m.r[0]); std::fflush(vout); if (!depth) rmap_off(m); }
                    return reflex_vm::RUN_ORIGINAL; }, 0);
                fns.erase(a);
                static char en[256][32]; static int ne;
                std::snprintf(en[ne], 32, "vtrace_win_%08x", a);
                new Registrar(a, en[ne++], g, [a](reflex_vm::Machine& m) -> uint32_t {
                    if (!depth++) rmap_on(m); std::fprintf(vout, "%llu WIN+ %08x\n", (unsigned long long)m.steps, a);
                    return on_call(m, a); }, 0);
                p = *e == ',' ? e + 1 : e;
            }
        }
        if (const char* fl = std::getenv("REFLEX_VTRACE_FNS")) {
            FILE* f = std::fopen(fl, "r");
            unsigned a;
            while (f && std::fscanf(f, "%x", &a) == 1) if (!wins.count(a)) fns.insert(a);
            if (f) std::fclose(f);
        }
        // REFLEX_VTRACE_SNAP=<ret addr>:<k>:<path>,...  an image of memory when the boot reaches <ret addr> the k-th time
        if (const char* sl = std::getenv("REFLEX_VTRACE_SNAP")) {
            static std::map<uint32_t, std::vector<std::pair<int, std::string>>> snaps;   // ret addr -> (k, path)
            for (const char* p = sl; *p;) {
                char* e; uint32_t r = uint32_t(std::strtoul(p, &e, 16)); if (*e != ':') break;
                int k = int(std::strtol(e + 1, &e, 10)); if (*e != ':') break;
                const char* q = e + 1; const char* end = std::strchr(q, ','); std::string path = end ? std::string(q, end) : std::string(q);
                snaps[r].push_back({k, path});
                p = end ? end + 1 : q + path.size();
            }
            static char sn[64][32]; int ns = 0;
            for (auto& [r, list] : snaps) {
                std::snprintf(sn[ns], 32, "vtrace_snap_%08x", r);
                auto* lp = &list; auto* cnt = new int(0);
                new Registrar(r, sn[ns++], g, [lp, cnt](reflex_vm::Machine& m) -> uint32_t {
                    ++*cnt; for (auto& [k, path] : *lp) if (k == *cnt) snapshot(m, path.c_str()); return reflex_vm::RUN_ORIGINAL; }, 0);
            }
        }
        static std::vector<std::string> names;
        names.reserve(fns.size());
        for (uint32_t a : fns) {
            char b[32]; std::snprintf(b, sizeof b, "vtrace_%08x", a); names.push_back(b);
            new Registrar(a, names.back().c_str(), g, [a](reflex_vm::Machine& m) -> uint32_t { return on_call(m, a); }, 0);
        }
    }
} reg;
}  // namespace
