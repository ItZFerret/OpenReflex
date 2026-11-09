// build_globals.cpp -- development tracing of the original's startup writes (unit 12b, core/notes/construction.md section 14).
// Not part of the product, not a verifier of core functions: it logs which .data / .rdata words change between the entries of the
// functions it hooks (the exe's 990 static initializers and a list of startup functions), and dumps memory snapshots of the exe's
// image range at chosen entries. The hooks only observe: every one returns RUN_ORIGINAL, so the boot is unchanged.
// Registered only when REFLEX_GLOBALS_TRACE=<log file> is set at start. Install with REFLEX_PORT=rx_globals_trace (On mode).
//   REFLEX_GLOBALS_POINTS=<hex,hex,...>   further functions to hook (their entry is a phase boundary; logged the first time only)
//   REFLEX_GLOBALS_SNAP=<hex>:<file>[,..] write the image range (0x00916000..0x00dd1000) at the first entry of that function
//   REFLEX_GLOBALS_STOP=<hex>             stop logging at the first entry of that function (the boot goes on)
// Log: "H <index> <address> <name> steps <n>" at each hook, then "W <address> <old> <new>" for every word that changed since the
// previous hook (so the words belong to the code that ran between the two hooks: the previous hook's function with its callees).
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
using namespace reflex_port;

namespace {
constexpr uint32_t LO = 0x00916000u, HI = 0x00dd1000u;   // .rdata .. .tls
struct Tr {
    std::vector<uint8_t> shadow;
    bool init = false, stopped = false;
    FILE* log = nullptr;
    int index = 0;
    uint64_t wcount = 0;
} T;
struct Itab { uint32_t slot, fn; };
const Itab ITAB[] = {
#include "build_globals_itab.inc"
};
std::vector<uint32_t> g_points;
std::vector<std::pair<uint32_t, std::string>> g_snaps;
uint32_t g_stop = 0;

uint32_t hits(uint32_t a) {
    static std::vector<std::pair<uint32_t, uint32_t>> v;
    for (auto& p : v) if (p.first == a) return p.second++;
    v.push_back({a, 1}); return 0;
}

const uint8_t* page_of(reflex_vm::Machine& m, uint32_t a) {
    if (!m.mem.mapped(a)) return nullptr;
    return m.mem.page(a);
}

void hook(reflex_vm::Machine& m, uint32_t addr, const char* name, bool once) {
    if (T.stopped) return;
    if (once && hits(addr) != 0) return;
    if (addr == g_stop) { T.stopped = true; }
    const size_t n = HI - LO;
    if (T.shadow.empty()) T.shadow.assign(n, 0);
    std::fprintf(T.log, "H %d %08x %s steps %llu\n", T.index++, addr, name, (unsigned long long)m.steps);
    for (uint32_t pg = LO; pg < HI; pg += 0x1000) {
        const uint8_t* p = page_of(m, pg);
        uint8_t* s = &T.shadow[pg - LO];
        if (!p) { continue; }
        if (!T.init) { std::memcpy(s, p, 0x1000); continue; }
        if (std::memcmp(s, p, 0x1000) == 0) continue;
        for (uint32_t o = 0; o < 0x1000; o += 4) {
            uint32_t a, b; std::memcpy(&a, s + o, 4); std::memcpy(&b, p + o, 4);
            if (a != b) { std::fprintf(T.log, "W %08x %08x %08x\n", pg + o, a, b); T.wcount++; }
        }
        std::memcpy(s, p, 0x1000);
    }
    T.init = true;
    for (auto& sn : g_snaps) if (sn.first == addr) {
        FILE* f = std::fopen(sn.second.c_str(), "wb");
        if (f) {
            for (uint32_t pg = LO; pg < HI; pg += 0x1000) {
                const uint8_t* p = page_of(m, pg);
                uint8_t z[0x1000] = {};
                std::fwrite(p ? p : z, 1, 0x1000, f);
            }
            std::fclose(f);
        }
        // the heap (and everything else outside the exe image and the stacks): {u32 address, 4096 bytes} per resident page
        std::string hp = sn.second; if (hp.size() > 4 && hp.compare(hp.size() - 4, 4, ".img") == 0) hp.resize(hp.size() - 4);
        if (FILE* d = std::fopen((hp + ".dll").c_str(), "wb")) {   // the CRT DLLs' data: msvcp90 .data 0x78505000.., msvcr90 .data 0x785b7000..
            for (uint32_t r : {0x78505000u, 0x785b7000u}) for (uint32_t a = r; a < r + 0x7000u; a += 0x1000) {
                const uint8_t* p = page_of(m, a); if (!p) continue;
                std::fwrite(&a, 4, 1, d); std::fwrite(p, 1, 0x1000, d);
            }
            std::fclose(d);
        }
        FILE* h = std::fopen((hp + ".heap").c_str(), "wb");
        if (h) {
            for (uint64_t pg = 0x01000000ull; pg < 0x7f000000ull; pg += 0x1000) {
                const uint8_t* p = m.mem.peek(uint32_t(pg));
                if (!p) continue;
                uint32_t a = uint32_t(pg);
                std::fwrite(&a, 4, 1, h); std::fwrite(p, 1, 0x1000, h);
            }
            std::fclose(h);
        }
    }
    std::fflush(T.log);
}

template <int K> uint32_t init_hook(reflex_vm::Machine& m) {
    char nm[32]; std::snprintf(nm, sizeof nm, "init%03d", K);
    hook(m, ITAB[K].fn, nm, false);
    return reflex_vm::RUN_ORIGINAL;
}
template <int... Ks> struct Seq {};
template <int N, int... Ks> struct Gen : Gen<N - 1, N - 1, Ks...> {};
template <int... Ks> struct Gen<0, Ks...> { using type = Seq<Ks...>; };
template <int... Ks> void reg_inits(Seq<Ks...>) {
    ((void)new Registrar(ITAB[Ks].fn, "globals_trace_init", "rx_globals_trace", init_hook<Ks>, 0), ...);
}

// point hooks: one registrar per listed address (static table of trampolines)
constexpr int MAXP = 64;
template <int K> uint32_t point_hook(reflex_vm::Machine& m) {
    char nm[32]; std::snprintf(nm, sizeof nm, "point%02d", K);
    hook(m, g_points[K], nm, true);
    return reflex_vm::RUN_ORIGINAL;
}
template <int... Ks> void reg_points(Seq<Ks...>) {
    ((void)(Ks < (int)g_points.size() ? (void)new Registrar(g_points[Ks], "globals_trace_point", "rx_globals_trace", point_hook<Ks>, 0) : (void)0), ...);
}

struct Reg {
    Reg() {
        const char* lf = std::getenv("REFLEX_GLOBALS_TRACE");
        if (!lf || !*lf) return;
        T.log = std::fopen(lf, "w");
        g_points.push_back(0x009006a7u);   // the PE entry: the image as loaded
        g_points.push_back(0x008814a0u);   // ReflexWinMain: after the CRT's _initterm
        if (const char* p = std::getenv("REFLEX_GLOBALS_POINTS")) {
            std::string s = p; size_t i = 0;
            while (i < s.size()) { size_t j = s.find(',', i); if (j == std::string::npos) j = s.size();
                if (j > i) g_points.push_back(uint32_t(std::strtoul(s.substr(i, j - i).c_str(), nullptr, 16))); i = j + 1; }
        }
        if (const char* p = std::getenv("REFLEX_GLOBALS_SNAP")) {
            std::string s = p; size_t i = 0;
            while (i < s.size()) { size_t j = s.find(',', i); if (j == std::string::npos) j = s.size();
                std::string e = s.substr(i, j - i); size_t c = e.find(':');
                if (c != std::string::npos) g_snaps.push_back({uint32_t(std::strtoul(e.substr(0, c).c_str(), nullptr, 16)), e.substr(c + 1)});
                i = j + 1; }
            for (auto& sn : g_snaps) { bool have = false; for (uint32_t a : g_points) have |= a == sn.first; if (!have) g_points.push_back(sn.first); }
        }
        if (const char* p = std::getenv("REFLEX_GLOBALS_STOP")) { g_stop = uint32_t(std::strtoul(p, nullptr, 16)); bool have = false; for (uint32_t a : g_points) have |= a == g_stop; if (!have) g_points.push_back(g_stop); }
        if (g_points.size() > MAXP) g_points.resize(MAXP);
        reg_inits(typename Gen<(int)(sizeof ITAB / sizeof ITAB[0])>::type());
        reg_points(typename Gen<MAXP>::type());
    }
} reg_;
}  // namespace
