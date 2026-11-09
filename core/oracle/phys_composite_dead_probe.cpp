// phys_composite_dead_probe.cpp -- oracle diagnostic (w-composite): does a composite pair's result depend on the dead stack?
// Ports rx_DeadProbe_CompositeVsComposite (008732d0) and rx_DeadProbe_CompositeVsHFTerrain (00873010), On mode, everything else
// original. At every call the probe runs, on the same entry state (page journal, registers put back between runs):
//   R    the original with the stack as it is (the reference),
//   P1   the original with the dead stack below the entry ESP poisoned with 0x00000000 (RX_DEADPROBE_DEPTH bytes, default 0x8000),
//   P2   the same with 0x40400000 (3.0f),
//   N    the core's port (the registered rx_ adapter) with the stack as it is,
//   NP1  the core's port on poison 1, compared with P1.
// and reports every call where a run's live memory (everything but the stack below the final ESP) differs from R (NP1: from P1).
// The call then runs as the original (the ride stays the original's). P1 / P2 differing from R proves the original's result depends
// on stack words written before the call; N differing while P1 == P2 == R proves the dependence is on what the original's own
// callees leave inside the call (deterministic, reproducible by the port). Diagnostic only: nothing in the product.
// On the calls where N differs (008732d0 only), RX_DEADPROBE_MASKS=f,7,... also runs mixed_composite (below) per mask. Switches for it:
//   RX_DEADPROBE_LOG=1          one line per child dispatch (child indices, shape types, contact counts)
//   RX_DEADPROBE_GENS=1         the core's pair generators installed under the original dispatcher during those runs
//   RX_DEADPROBE_DSEL=<hex>     with mask bit 3: only the dispatches whose bit is set run as the original
//   RX_DEADPROBE_SNAP=k, RX_DEADPROBE_DIR=<dir>   the 8 KB of stack below the frame base F before dispatch k -> deadsnap_<call>_<mask>_<k>.bin
//   RX_DEADPROBE_INJECT_CALL=<call>, RX_DEADPROBE_INJECT=k:<deadsnap file>:lo-hi[,lo-hi]   before dispatch k of that call, the words
//                               F - o for o in [lo, hi) (hex, step 4) written from the snapshot (bisects the words a result needs)
//   RX_DEADPROBE_WATCH=k:<offset hex>   with RX_DEADPROBE_INJECT_CALL: the emulator's write watch on F - offset until dispatch k
//                               (<dir>/watch.txt: every write, its pc and the return addresses on the stack)
// Used to find the 0081c590 Newton-step read (core/notes/phys.md "The Newton step").
//   RX_DEADPROBE_RWATCH=<call>:<address hex>:<file>   (both ports) the emulator's write watch on one address during that call's R
//                               run (each write's pc and the return addresses on the stack): w-hfterrain found 00873010's writers
//                               with it (core/notes/phys.md "The w lanes of CompositeVsHFTerrain").
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_coll_shape.h"
#include "../game/phys_contact.h"
}
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

namespace {
using reflex_vm::Machine;
struct Snap { uint32_t r[8]; uint32_t flags; reflex_vm::Xmm xmm[8]; std::vector<reflex_f80::F80> fpu; uint16_t cw, sw; uint32_t mxcsr; };
Snap save(const Machine& m) {
    Snap s; std::memcpy(s.r, m.r, sizeof s.r); s.flags = m.flags; std::memcpy(s.xmm, m.xmm, sizeof s.xmm); s.fpu = m.fpu;
    s.cw = m.fpu_cw; s.sw = m.fpu_sw; s.mxcsr = m.mxcsr; return s;
}
void load(Machine& m, const Snap& s) {
    std::memcpy(m.r, s.r, sizeof s.r); m.flags = s.flags; std::memcpy(m.xmm, s.xmm, sizeof s.xmm); m.fpu = s.fpu;
    m.fpu_cw = s.cw; m.fpu_sw = s.sw; m.mxcsr = s.mxcsr;
}
using Pages = std::map<uint32_t, std::vector<uint8_t>>;
struct Run { Pages pages; uint32_t eax = 0; };
Run capture(Machine& m) {
    Run r; r.eax = m.r[0];
    for (uint32_t pg : m.mem.journal_pages()) { const uint8_t* p = m.mem.peek(pg << 12); if (p) r.pages[pg].assign(p, p + 4096); }
    return r;
}
struct Stat { uint64_t calls = 0, p1 = 0, p2 = 0, n = 0, np1 = 0; };
std::map<uint32_t, Stat> g_stat;
bool g_in = false;
uint64_t g_call = 0;   // the probed call being run (for the snapshot / injection switches)
const char* name_of(uint32_t a) { return a == 0x008732d0u ? "CompositeVsComposite" : "CompositeVsHFTerrain"; }
struct Report { ~Report() { for (auto& [a, s] : g_stat) std::fprintf(stderr, "deadprobe %08x %s calls %llu  P1!=R %llu  P2!=R %llu  N!=R %llu  NP1!=P1 %llu\n",
    a, name_of(a), (unsigned long long)s.calls, (unsigned long long)s.p1, (unsigned long long)s.p2, (unsigned long long)s.n, (unsigned long long)s.np1); } } g_report;

// differences between two runs' live words (the entry image where a run did not write a page; entry = the restored live memory)
int diff(Machine& m, const Run& a, const Run& b, uint32_t dead_lo, uint32_t dead_hi, FILE* out, const char* tag, uint32_t addr, uint64_t call) {
    std::map<uint32_t, bool> pg; for (auto& kv : a.pages) pg[kv.first] = true; for (auto& kv : b.pages) pg[kv.first] = true;
    int n = 0;
    for (auto& kv : pg) {
        const uint8_t* live = m.mem.peek(kv.first << 12);
        auto ia = a.pages.find(kv.first), ib = b.pages.find(kv.first);
        const uint8_t* pa = ia != a.pages.end() ? ia->second.data() : live;
        const uint8_t* pb = ib != b.pages.end() ? ib->second.data() : live;
        if (!pa || !pb) continue;
        for (uint32_t i = 0; i < 4096; i += 4) {
            const uint32_t w = (kv.first << 12) + i;
            if (w >= dead_lo && w < dead_hi) continue;
            uint32_t x, y; std::memcpy(&x, pa + i, 4); std::memcpy(&y, pb + i, 4);
            if (x == y) continue;
            if (out && n < 48) std::fprintf(out, "deadprobe %08x call %llu %s [%08x] %08x ref %08x\n", addr, (unsigned long long)call, tag, w, x, y);
            n++;
        }
    }
    if (a.eax != b.eax) { if (out) std::fprintf(out, "deadprobe %08x call %llu %s eax %08x ref %08x\n", addr, (unsigned long long)call, tag, a.eax, b.eax); n++; }
    return n;
}

const reflex_port::Entry* core_port(const char* name) {
    for (const auto& e : reflex_port::registry()) if (std::strcmp(e.name, name) == 0) return &e;
    return nullptr;
}

// 008732d0 as the original executes it (disassembly 008732d0..0087353c): each callee entered at the original's ESP with its stack
// arguments, real return address and registers; `orig` bit 0 / 1 / 2 / 3 = CompositeChildRotation 00848650 / CompositeChildPosition
// 008486f0 / the child's bounds vf2 / Collision_ContactDispatch 0085ddd0 run as the original (else the core's port at the same ESP).
// Mask 0xf reproduces the original (the transliteration's check); the others show which callee's stack residue the result needs.
uint32_t mixed_composite(Machine& m, unsigned orig) {
    rx_oracle::Bind bind(m);
    const uint32_t sp0 = m.r[4], M = m.r[1];
    uint32_t a[7]; for (int i = 0; i < 7; i++) a[i] = m.arg(i);
    const uint32_t A = a[0], qA = a[1], pA = a[2], B = a[3], qB = a[4], pB = a[5], swap = a[6];
    const uint32_t ebp = sp0 - 4, F = ((sp0 - 4) & ~15u) - 0xd4 - 12;
    m.mem.write(sp0 - 4, 4, m.r[5]);
    const bool sw = (swap & 0xff) != 0;
    const uint32_t ids_a = M + (sw ? 0x628 : 0x5b0), ids_b = M + (sw ? 0x5b0 : 0x628);
    // one call at ESP F: arguments pushed below F, the return address below them; registers as the original loads them
    static const uint64_t dsel = std::getenv("RX_DEADPROBE_DSEL") ? std::strtoull(std::getenv("RX_DEADPROBE_DSEL"), nullptr, 16) : 0;
    static const bool log = std::getenv("RX_DEADPROBE_LOG") != nullptr;
    unsigned dk = 0;
    static const int snap_k = std::getenv("RX_DEADPROBE_SNAP") ? std::atoi(std::getenv("RX_DEADPROBE_SNAP")) : -1;
    static const char* snap_dir = std::getenv("RX_DEADPROBE_DIR") ? std::getenv("RX_DEADPROBE_DIR") : ".";
    static int inject_k = -1; static std::vector<uint8_t> inject_img; static std::vector<std::pair<uint32_t, uint32_t>> inject_ranges;
    static bool inject_init = false;
    if (!inject_init) {
        inject_init = true;
        if (const char* v = std::getenv("RX_DEADPROBE_INJECT")) {   // k:file:ranges (the file a deadsnap of the same call)
            std::string sv(v); size_t c1 = sv.find(':'), c2 = sv.find(':', c1 + 1);
            inject_k = std::atoi(sv.substr(0, c1).c_str());
            const std::string file = sv.substr(c1 + 1, c2 - c1 - 1), rs = sv.substr(c2 + 1);
            if (FILE* f = std::fopen(file.c_str(), "rb")) { inject_img.resize(0x2000); if (std::fread(inject_img.data(), 1, 0x2000, f) != 0x2000) inject_k = -1; std::fclose(f); }
            else inject_k = -1;
            size_t p = 0;
            while (p < rs.size()) {
                size_t q = rs.find(',', p); if (q == std::string::npos) q = rs.size();
                const std::string r = rs.substr(p, q - p); const size_t d = r.find('-');
                // offsets below F: o in [lo, hi), the address F - o
                const uint32_t lo = uint32_t(std::strtoul(r.substr(0, d).c_str(), nullptr, 16)), hi = uint32_t(std::strtoul(r.substr(d + 1).c_str(), nullptr, 16));
                inject_ranges.push_back({lo, hi}); p = q + 1;
            }
        }
    }
    static const uint64_t inject_call = std::getenv("RX_DEADPROBE_INJECT_CALL") ? std::strtoull(std::getenv("RX_DEADPROBE_INJECT_CALL"), nullptr, 10) : 0;
    auto call = [&](unsigned bit, uint32_t fn, uint32_t ra, std::initializer_list<uint32_t> args, uint32_t eax, uint32_t ecx, uint32_t edx,
                    uint32_t ebx, uint32_t esi, uint32_t edi, const std::function<void()>& native) {
        const uint32_t n = uint32_t(args.size());
        uint32_t k = 0; for (uint32_t v : args) m.mem.write(F - 4 * n + 4 * k++, 4, v);
        m.r[0] = eax; m.r[1] = ecx; m.r[2] = edx; m.r[3] = ebx; m.r[5] = ebp; m.r[6] = esi; m.r[7] = edi; m.r[4] = F - 4 * n;
        if ((orig & bit) && !(bit == 8 && dsel && !((dsel >> (dk < 63 ? dk : 63)) & 1))) { g_in = true; try { m.call_at(fn, ra); } catch (...) { g_in = false; throw; } g_in = false; }
        else { m.mem.write(F - 4 * n - 4, 4, ra); m.r[4] = F - 4 * n - 4; native(); }
        m.r[4] = F;
    };
    // RX_DEADPROBE_WATCH=k:offset (hex, below F): the emulator's write watch on F - offset from the call's start to dispatch k
    static int watch_k = -1; static uint32_t watch_off = 0;
    if (const char* v = std::getenv("RX_DEADPROBE_WATCH"); v && watch_k < 0) { watch_k = std::atoi(v); watch_off = uint32_t(std::strtoul(std::strchr(v, ':') + 1, nullptr, 16)); }
    if (watch_k >= 0 && g_call == inject_call) {
        static FILE* wf = std::fopen((std::string(snap_dir) + "/watch.txt").c_str(), "a");
        std::fprintf(wf, "watch call %llu mask %x address %08x\n", (unsigned long long)g_call, orig, F - watch_off);
        m.trace_out = wf; m.watch_address = F - watch_off;
    }
    const int32_t na = (int32_t(rx_rd32(A + 0x30)) - int32_t(rx_rd32(A + 0x2c)));
    uint32_t count_a = rx_rd32(M), count_b = count_a, eax = ids_b;
    if ((na & ~3) > 0) {
        for (uint32_t i = 0; int32_t(i) < (int32_t(rx_rd32(A + 0x30)) - int32_t(rx_rd32(A + 0x2c))) >> 2; i++) {
            const uint32_t ca = rx_rd32(rx_rd32(A + 0x2c) + 4 * i);
            call(1, 0x848650, 0x87337e, {qA, F + 0x80}, qA, i, F + 0x80, i, ca, A, [&] { phys_composite_child_rotation(A, i, qA, F + 0x80); });
            call(2, 0x8486f0, 0x873392, {qA, pA, F + 0x50}, qA, i, pA, i, ca, A, [&] { phys_composite_child_position(A, i, qA, pA, F + 0x50); });
            const uint32_t fa = rx_rd32(rx_rd32(ca) + 8);
            call(4, fa, 0x8733b8, {F + 0x80, F + 0x50, F + 0xa0, F + 0x90}, F + 0x50, ca, fa, i, ca, A,
                 [&] { phys_shape_bounds(fa, ca, F + 0x80, F + 0x50, F + 0xa0, F + 0x90); });
            const int32_t nb = int32_t(rx_rd32(B + 0x30)) - int32_t(rx_rd32(B + 0x2c));
            if ((nb & ~3) > 0) {
                for (uint32_t j = 0; int32_t(j) < (int32_t(rx_rd32(B + 0x30)) - int32_t(rx_rd32(B + 0x2c))) >> 2; j++) {
                    const uint32_t cb = rx_rd32(rx_rd32(B + 0x2c) + 4 * j);
                    call(1, 0x848650, 0x873401, {qB, F + 0x70}, rx_rd32(B + 0x2c), j, qB, cb, j, B, [&] { phys_composite_child_rotation(B, j, qB, F + 0x70); });
                    call(2, 0x8486f0, 0x873415, {qB, pB, F + 0x60}, F + 0x60, j, qB, cb, j, B, [&] { phys_composite_child_position(B, j, qB, pB, F + 0x60); });
                    const uint32_t fb = rx_rd32(rx_rd32(cb) + 8);
                    call(4, fb, 0x873438, {F + 0x70, F + 0x60, F + 0xb0, F + 0xc0}, fb, cb, F + 0x70, cb, j, B,
                         [&] { phys_shape_bounds(fb, cb, F + 0x70, F + 0x60, F + 0xb0, F + 0xc0); });
                    const rx_v4 mn_a = rx_ld4(F + 0xa0), mx_a = rx_ld4(F + 0x90), mn_b = rx_ld4(F + 0xb0), mx_b = rx_ld4(F + 0xc0);
                    bool ov = true;
                    for (int l = 0; l < 3; l++) if (!(mn_b.v[l] <= mx_a.v[l]) || !(mn_a.v[l] <= mx_b.v[l])) ov = false;
                    if (!ov) continue;
                    if (watch_k >= 0 && dk == unsigned(watch_k)) { m.watch_address = 0; if (m.trace_out) std::fflush((FILE*)m.trace_out); }
                    if (snap_k >= 0 && dk == unsigned(snap_k)) {   // the stack below the dispatcher's entry before dispatch k (RX_DEADPROBE_SNAP=k)
                        char fn[512]; std::snprintf(fn, sizeof fn, "%s/deadsnap_%llu_%x_%u.bin", snap_dir, (unsigned long long)g_call, orig, dk);
                        std::vector<uint8_t> b(0x2000); m.mem.read_bytes(F - 0x2000, b.data(), b.size());
                        if (FILE* f = std::fopen(fn, "wb")) { std::fwrite(b.data(), 1, b.size(), f); std::fclose(f); }
                    }
                    if (inject_k >= 0 && g_call == inject_call && dk == unsigned(inject_k)) {   // RX_DEADPROBE_INJECT=k:file:lo-hi,lo-hi (offsets below F, hex)
                        for (auto& [lo, hi] : inject_ranges) for (uint32_t o = lo; o < hi; o += 4) {
                            uint32_t w; std::memcpy(&w, inject_img.data() + (0x2000 - o), 4); m.mem.write(F - o, 4, w);
                        }
                    }
                    call(8, 0x85ddd0, 0x8734bb, {ca, F + 0x80, F + 0x50, cb, F + 0x70, F + 0x60, swap}, F + 0x80, M, F + 0x50, cb, j, M,
                         [&] { phys_contact_dispatch(M, ca, F + 0x80, F + 0x50, cb, F + 0x70, F + 0x60, swap); });
                    if (log) std::fprintf(stderr, "deadprobe mix%x dispatch %u: A child %u type %x, B child %u type %x, count %u -> %u\n", orig, dk, i,
                                          rx_rd32(ca + 8), j, rx_rd32(cb + 8), count_b, rx_rd32(M));
                    dk++;
                    for (int32_t k = int32_t(count_b); k < int32_t(rx_rd32(M)); k++) rx_wr32(ids_b + 4 * uint32_t(k), j);
                    count_b = rx_rd32(M);
                }
            }
            for (int32_t k = int32_t(count_a); k < int32_t(rx_rd32(M)); k++) rx_wr32(ids_a + 4 * uint32_t(k), i);
            count_a = rx_rd32(M); eax = count_a;
        }
    }
    m.r[0] = eax; m.r[4] = sp0 + 4 + 0x1c;
    return 0;
}

uint32_t probe(Machine& m, uint32_t addr, uint32_t arg_bytes, const char* core_name) {
    if (g_in) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t depth = std::getenv("RX_DEADPROBE_DEPTH") ? uint32_t(std::strtoul(std::getenv("RX_DEADPROBE_DEPTH"), nullptr, 16)) : 0x8000u;
    static FILE* out = std::getenv("RX_DEADPROBE_FILE") ? std::fopen(std::getenv("RX_DEADPROBE_FILE"), "w") : stderr;
    const reflex_port::Entry* core = core_port(core_name);
    if (!core) { std::fprintf(stderr, "deadprobe: no %s\n", core_name); std::abort(); }
    Stat& st = g_stat[addr]; const uint64_t call = ++st.calls; g_call = call;
    const Snap entry = save(m);
    const uint32_t esp0 = m.r[4], dead_lo = esp0 - 0x40000u, dead_hi = esp0 + 4 + arg_bytes;
    const bool jit = m.jit_enabled; m.jit_enabled = false;
    auto poison = [&](uint32_t v) { for (uint32_t a = esp0 - depth; a < esp0; a += 4) m.mem.write(a, 4, v); };
    auto original = [&]() { g_in = true; try { m.tail_at(addr); } catch (...) { g_in = false; throw; } g_in = false; };
    auto native = [&]() { rx_oracle::Bind b(m); (void)core->fn(m); };
    auto put_back = [&]() { m.mem.journal_restore(); load(m, entry); };
    m.mem.journal_begin();
    // RX_DEADPROBE_RWATCH=<call>:<address hex>:<file>: the emulator's write watch on that address during that call's R run
    static uint64_t rw_call = 0; static uint32_t rw_addr = 0; static FILE* rw_out = nullptr;
    if (const char* v = std::getenv("RX_DEADPROBE_RWATCH"); v && !rw_addr) {
        rw_call = std::strtoull(v, nullptr, 10); const char* c = std::strchr(v, ':');
        rw_addr = c ? uint32_t(std::strtoul(c + 1, nullptr, 16)) : 1; const char* c2 = c ? std::strchr(c + 1, ':') : nullptr;
        rw_out = std::fopen(c2 ? c2 + 1 : "rwatch.txt", "w");
    }
    if (rw_out && call == rw_call) { std::fprintf(rw_out, "rwatch call %llu esp0 %08x address %08x\n", (unsigned long long)call, esp0, rw_addr); m.trace_out = rw_out; m.watch_address = rw_addr; }
    original(); Run R = capture(m); put_back();
    if (rw_out && call == rw_call) { m.watch_address = 0; std::fflush(rw_out); }
    poison(0u); original(); Run P1 = capture(m); put_back();
    poison(0x40400000u); original(); Run P2 = capture(m); put_back();
    native(); Run N = capture(m); put_back();
    poison(0u); native(); Run NP1 = capture(m); put_back();
    std::vector<std::pair<unsigned, Run>> mixed;
    const char* masks = std::getenv("RX_DEADPROBE_MASKS");
    if (masks && addr == 0x008732d0u && diff(m, N, R, dead_lo, dead_hi, nullptr, "", addr, call)) {
        for (const char* p = masks; *p;) {
            char* e = nullptr; const unsigned mask = unsigned(std::strtoul(p, &e, 16)); if (e == p) break;
            static const bool gens = std::getenv("RX_DEADPROBE_GENS") != nullptr;
            std::map<uint32_t, reflex_vm::Native> saved;
            if (gens) for (const auto& e : reflex_port::registry())
                if (std::strncmp(e.name, "rx_CollisionPair_", 17) == 0 && std::strstr(e.name, "Composite") == nullptr) {
                    auto it = m.natives.find(e.address); if (it != m.natives.end()) saved[e.address] = it->second;
                    const reflex_port::Entry* ep = &e; m.natives[e.address] = [ep](Machine& mm) { return ep->fn(mm); };
                }
            mixed_composite(m, mask);
            if (gens) for (const auto& e : reflex_port::registry())
                if (std::strncmp(e.name, "rx_CollisionPair_", 17) == 0 && std::strstr(e.name, "Composite") == nullptr) {
                    auto it = saved.find(e.address); if (it != saved.end()) m.natives[e.address] = it->second; else m.natives.erase(e.address);
                }
            mixed.push_back({mask, capture(m)}); put_back();
            p = *e == ',' ? e + 1 : e;
        }
    }
    m.mem.journal_end();
    m.jit_enabled = jit;
    for (auto& [mask, run] : mixed) { char tag[16]; std::snprintf(tag, sizeof tag, "mix%x", mask);
        const int d = diff(m, run, R, dead_lo, dead_hi, out, tag, addr, call);
        std::fprintf(out, "deadprobe %08x call %llu mix%x: %d differences from R\n", addr, (unsigned long long)call, mask, d); }
    if (diff(m, P1, R, dead_lo, dead_hi, out, "P1", addr, call)) st.p1++;
    if (diff(m, P2, R, dead_lo, dead_hi, out, "P2", addr, call)) st.p2++;
    if (diff(m, N, R, dead_lo, dead_hi, out, "N", addr, call)) st.n++;
    if (diff(m, NP1, P1, dead_lo, dead_hi, out, "NP1", addr, call)) st.np1++;
    if (out) std::fflush(out);
    return reflex_vm::RUN_ORIGINAL;   // the call itself: the original's
}
}  // namespace

RX_PORT_RAW(0x008732d0, DeadProbe_CompositeVsComposite, "rx_probe", 0) { return probe(m, 0x008732d0u, 0x1c, "rx_CollisionPair_CompositeVsComposite"); }
RX_PORT_RAW(0x00873010, DeadProbe_CompositeVsHFTerrain, "rx_probe", 0) { return probe(m, 0x00873010u, 0x18, "rx_CollisionPair_CompositeVsHFTerrain"); }

