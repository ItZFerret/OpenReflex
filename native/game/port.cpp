// Port registry, installation and the Verify mode (port.h).
#include "port.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <sstream>

#include <cstdio>
namespace reflex_port { std::FILE* heap_trace_file(); }   // port.cpp: REFLEX_HEAP_TRACE (diagnostics)
namespace reflex_port {
using reflex_vm::Machine;
using reflex_vm::RUN_ORIGINAL;

std::vector<Entry>& registry() { static std::vector<Entry> r; return r; }
Registrar::Registrar(uint32_t address, const char* name, const char* group, reflex_vm::Native fn, uint32_t outputs) {
    registry().push_back({address, name, group, std::move(fn), outputs});
}

namespace {

std::map<uint32_t, Stats> g_stats;
std::map<const Machine*, HostSnapshot> g_host;
// Installed ports per machine, and the addresses where the machine's native is the port itself (elsewhere a runtime native that was
// there first, e.g. the ambient-animation skips, keeps its place and falls through to the port: installed()).
std::map<const Machine*, std::map<uint32_t, reflex_vm::Native>> g_installed;
std::map<const Machine*, std::set<uint32_t>> g_owned;
uint64_t g_mismatches = 0;
// Where a call happens relative to a verification: outside one, inside its native run (nested ports run as natives: they are
// part of the native being checked), or inside its original run (everything original: the reference).
enum class Phase { None, Native, Original };
Phase g_phase = Phase::None;

struct Regs {
    uint32_t r[8]; uint32_t flags; reflex_vm::Xmm xmm[8]; std::vector<reflex_f80::F80> fpu;
    uint16_t cw, sw; uint32_t mxcsr; uint64_t mmx[8]; bool mmx_active;
};
Regs save(const Machine& m) {
    Regs s; std::memcpy(s.r, m.r, sizeof s.r); s.flags = m.flags; std::memcpy(s.xmm, m.xmm, sizeof s.xmm); s.fpu = m.fpu;
    s.cw = m.fpu_cw; s.sw = m.fpu_sw; s.mxcsr = m.mxcsr; std::memcpy(s.mmx, m.mmx, sizeof s.mmx); s.mmx_active = m.mmx_active; return s;
}
void load(Machine& m, const Regs& s) {
    std::memcpy(m.r, s.r, sizeof s.r); m.flags = s.flags; std::memcpy(m.xmm, s.xmm, sizeof s.xmm); m.fpu = s.fpu;
    m.fpu_cw = s.cw; m.fpu_sw = s.sw; m.mxcsr = s.mxcsr; std::memcpy(m.mmx, s.mmx, sizeof s.mmx); m.mmx_active = s.mmx_active;
}
bool same_f80(const reflex_f80::F80& a, const reflex_f80::F80& b) { return a.kind == b.kind && a.sign == b.sign && a.mant == b.mant && a.exp == b.exp; }

const char* kRegName[8] = {"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"};

// Liveness of each function's results (live.inc, from tools/liveness.py): registers some caller can read after it returns.
struct Live { uint32_t address, mask; };
const Live kLive[] = {
#include "live.inc"
};
uint32_t live_mask(uint32_t address) {
    static const bool off = std::getenv("REFLEX_PORT_ALL_OUTPUTS") != nullptr;   // diagnostics: compare every declared output
    if (off) return ~0u;
    size_t lo = 0, hi = sizeof kLive / sizeof kLive[0];
    while (lo < hi) { size_t mid = (lo + hi) / 2; if (kLive[mid].address < address) lo = mid + 1; else hi = mid; }
    return lo < sizeof kLive / sizeof kLive[0] && kLive[lo].address == address ? kLive[lo].mask : ~0u;
}

// Differences between the native's and the original's results (registers by `outputs`, then memory), as text; empty if none.
std::string compare(const Entry& e, const Regs& n, const Regs& o, const std::map<uint32_t, std::vector<uint8_t>>& native_pages,
                    Machine& m, uint32_t esp_entry) {
    std::ostringstream d; int shown = 0;
    const uint32_t outputs = e.outputs & live_mask(e.address);
    // REFLEX_PORT_SHOW=n: differences printed per mismatching call (default 12). REFLEX_PORT_DIFF_FILE=path: every difference of every
    // mismatching call appended as "<port> <address> <call #> <difference>" lines (for classifying them by script).
    static const int lim = std::getenv("REFLEX_PORT_SHOW") ? std::atoi(std::getenv("REFLEX_PORT_SHOW")) : 12;
    static FILE* diff_file = std::getenv("REFLEX_PORT_DIFF_FILE") ? std::fopen(std::getenv("REFLEX_PORT_DIFF_FILE"), "a") : nullptr;
    static std::map<uint32_t, unsigned> compares;
    const unsigned call_no = ++compares[e.address];
    auto diff = [&](const std::string& what) {
        if (shown++ < lim) d << (shown > 1 ? "; " : "") << what;
        if (diff_file) std::fprintf(diff_file, "%s %08x %u %s\n", e.name, e.address, call_no, what.c_str());
    };
    char b[160];
    for (int i : {3, 4, 5, 6, 7}) if (n.r[i] != o.r[i]) { std::snprintf(b, sizeof b, "%s %08x orig %08x", kRegName[i], n.r[i], o.r[i]); diff(b); }
    if ((outputs & OUT_EAX) && n.r[0] != o.r[0]) { std::snprintf(b, sizeof b, "eax %08x orig %08x", n.r[0], o.r[0]); diff(b); }
    if ((outputs & OUT_ECX) && n.r[1] != o.r[1]) { std::snprintf(b, sizeof b, "ecx %08x orig %08x", n.r[1], o.r[1]); diff(b); }
    if ((outputs & OUT_EDX) && n.r[2] != o.r[2]) { std::snprintf(b, sizeof b, "edx %08x orig %08x", n.r[2], o.r[2]); diff(b); }
    for (int i = 0; i < 8; i++)
        if ((outputs & (OUT_XMM0 << i)) && (n.xmm[i].lo != o.xmm[i].lo || n.xmm[i].hi != o.xmm[i].hi)) {
            std::snprintf(b, sizeof b, "xmm%d %016llx%016llx orig %016llx%016llx", i, (unsigned long long)n.xmm[i].hi, (unsigned long long)n.xmm[i].lo,
                          (unsigned long long)o.xmm[i].hi, (unsigned long long)o.xmm[i].lo); diff(b);
        }
    if (n.fpu.size() != o.fpu.size()) { std::snprintf(b, sizeof b, "x87 depth %zu orig %zu", n.fpu.size(), o.fpu.size()); diff(b); }
    else if (outputs & OUT_FPU) for (size_t i = 0; i < n.fpu.size(); i++) if (!same_f80(n.fpu[i], o.fpu[i])) { std::snprintf(b, sizeof b, "st(%zu) differs", n.fpu.size() - 1 - i); diff(b); }
    if (n.cw != o.cw) { std::snprintf(b, sizeof b, "x87 cw %04x orig %04x", n.cw, o.cw); diff(b); }
    if (n.mxcsr != o.mxcsr && ((n.mxcsr ^ o.mxcsr) & ~0x3fu)) { std::snprintf(b, sizeof b, "mxcsr %08x orig %08x", n.mxcsr, o.mxcsr); diff(b); }
    if ((outputs & OUT_FLAGS) && ((n.flags ^ o.flags) & 0x8d5)) { std::snprintf(b, sizeof b, "flags %03x orig %03x", n.flags & 0x8d5, o.flags & 0x8d5); diff(b); }
    // memory: every page either run wrote (the other run's view of a page it did not write is the entry image), without the
    // stack below the final ESP (dead locals, which differ freely)
    // the stacks (runtime.cpp: main 0x7fec0000..0x7ff00000, workers 0x7fbc0000..0x7fc00000): the one the call runs on is dead below
    // its final ESP; the others hold only what emulated worker jobs started inside the call left behind (dead once they block / end)
    const uint32_t kStacks[2][2] = {{0x7fec0000u, 0x7ff00000u}, {0x7fbc0000u, 0x7fc00000u}};
    uint32_t esp = o.r[4], dead_lo = esp_entry > 0x40000 ? esp_entry - 0x40000 : 0;
    // A call on a worker stack (the worker region holds every worker's stack): only its caller's frames just above its entry ESP
    // (arguments, out-parameters: 16 KB) are live; the rest of the region is other workers' dead locals (the OPEN REFLEX core's
    // hand-written thread bodies never leave the original's dead locals there).
    // OPEN REFLEX core groups (rx_*): the JobPool ring 0x00d94b10..0x00d9cb10 holds job records copied from the submitters' stacks
    // (partly uninitialized locals = dead data the core's hand-written callers never reproduce); slot functions / indices stay in.
    const bool core = std::strncmp(e.group, "rx_", 3) == 0;
    static const bool keep_dead = std::getenv("REFLEX_PORT_DEAD") != nullptr;   // diagnostics: compare the dead stack below the final ESP too (the residue a hand-written port must leave)
    auto dead = [&](uint32_t a) {
        if (keep_dead && a >= dead_lo && a < esp) return false;
        if (a >= dead_lo && a < esp) return true;
        if (core && a >= 0x00d94b10u && a < 0x00d9cb10u) return true;
        for (auto& r : kStacks) {
            if (a < r[0] || a >= r[1]) continue;
            if (!(esp_entry >= r[0] && esp_entry < r[1])) return true;
            if (r[0] == 0x7fbc0000u && !(a >= esp_entry && a < esp_entry + 0x4000u)) return true;
        }
        return false;
    };
    std::map<uint32_t, bool> pages;
    for (auto& [pg, img] : native_pages) pages[pg] = true;
    for (uint32_t pg : m.mem.journal_pages()) pages[pg] = true;
    for (auto& [pg, unused] : pages) {
        (void)unused;
        const uint8_t* orig = m.mem.peek(pg << 12);
        auto nit = native_pages.find(pg);
        const uint8_t* nat = nit != native_pages.end() ? nit->second.data() : m.mem.journal_pre(pg);
        if (!nat) nat = orig;                                  // native wrote it, original did not: native's image vs the live one
        if (!orig || !nat) continue;
        for (uint32_t i = 0; i < 4096; i += 4) {
            uint32_t a = (pg << 12) + i;
            if (dead(a)) continue;
            uint32_t x, y; std::memcpy(&x, nat + i, 4); std::memcpy(&y, orig + i, 4);
            if (x != y) { std::snprintf(b, sizeof b, "[%08x] %08x orig %08x", a, x, y); diff(b); }
        }
    }
    if (shown > lim) d << "; ... " << shown << " differences";
    if (diff_file && shown) std::fflush(diff_file);
    return d.str();
}

// Verify: native first, then the original on the same entry state (its result is kept).
// Inside the boot (baton scheduling) a verified call must not switch guest threads: Machine::no_switch is set around both runs and
// the boot throws reflex_vm::WouldSwitch where it would switch (a wait that blocks, a yield, a failed poll, thread exit, the frame
// stop) or change host state it cannot take back. Native run blocked: memory, registers and host state are put back and the original
// runs instead, unverified (RUN_ORIGINAL: the caller's run loop executes it with switches allowed; ports it calls are verified one by
// one); counted as unverifiable. Original run blocked (the native did not): put back the same way, the original runs free, counted as a
// mismatch.
uint32_t verify(Machine& m, const Entry& e) {
    Stats& st = g_stats[e.address];
    st.calls++;
    FILE* tf = heap_trace_file();
    if (tf) std::fprintf(tf, "V+ native %s %llu steps %llu\n", e.name, (unsigned long long)st.calls, (unsigned long long)m.steps);
    const Regs entry = save(m);
    const uint32_t esp_entry = m.r[4], ret_addr = uint32_t(m.mem.read(esp_entry, 4));
    struct NoSwitch { Machine& m; bool on = true; NoSwitch(Machine& mm) : m(mm) { m.no_switch++; } void end() { if (on) { m.no_switch--; on = false; } } ~NoSwitch() { end(); } } guard(m);
    auto host = g_host.find(&m);
    std::function<void()> host_restore = host != g_host.end() ? host->second() : nullptr;
    m.mem.journal_begin();
    auto put_back = [&]() { m.mem.journal_restore(); load(m, entry); if (host_restore) host_restore(); };
    // the native
    Regs nat; std::map<uint32_t, std::vector<uint8_t>> native_pages; std::string error; uint32_t native_next = 0; bool blocked = false;
    g_phase = Phase::Native;
    try { native_next = e.fn(m); } catch (const reflex_vm::WouldSwitch&) { blocked = true; } catch (const std::exception& ex) { error = ex.what(); }
    g_phase = Phase::None;
    if (blocked) {   // the native would switch threads: the original runs in its place, unverified
        put_back(); m.mem.journal_end(); guard.end();
        st.unverifiable++;
        return RUN_ORIGINAL;
    }
    nat = save(m);
    for (uint32_t pg : m.mem.journal_pages()) { const uint8_t* p = m.mem.peek(pg << 12); if (p) native_pages[pg].assign(p, p + 4096); }
    m.mem.journal_restore();
    load(m, entry);
    if (host_restore) host_restore();
    // the original, to its real return address (Machine::run stops there once the call has popped it; interpreted, since chained
    // JIT blocks would run past the stop)
    const bool jit = m.jit_enabled; const uint32_t sp = m.stop_pc, se = m.stop_esp;
    m.jit_enabled = false; m.stop_pc = ret_addr; m.stop_esp = (e.outputs & OUT_ESP_BELOW) ? 0 : esp_entry;
    if (tf) std::fprintf(tf, "V+ original %s %llu steps %llu\n", e.name, (unsigned long long)st.calls, (unsigned long long)m.steps);
    g_phase = Phase::Original;
    auto end_original = [&]() { g_phase = Phase::None; m.jit_enabled = jit; m.stop_pc = sp; m.stop_esp = se; };
    bool original_blocked = false;
    try { m.run(e.address, m.call_budget); }
    catch (const reflex_vm::WouldSwitch&) { original_blocked = true; }
    catch (...) { end_original(); m.mem.journal_end(); throw; }
    end_original();
    if (original_blocked) {   // the original would switch threads where the native did not: a difference; the original runs free
        put_back(); m.mem.journal_end(); guard.end();
        st.mismatches++; g_mismatches++;
        const std::string diff = "the original would switch threads (blocks / yields), the native did not";
        if (st.first.empty()) st.first = diff;
        static const bool loud = std::getenv("REFLEX_PORT_LOG") != nullptr;
        if (loud || st.mismatches <= 3) std::fprintf(stderr, "[port] %s %08x call %llu: %s\n", e.name, e.address, (unsigned long long)st.calls, diff.c_str());
        return RUN_ORIGINAL;
    }
    guard.end();
    if (tf) std::fprintf(tf, "V- %s %llu steps %llu\n", e.name, (unsigned long long)st.calls, (unsigned long long)m.steps);
    Regs orig = save(m);
    std::string diff = error.empty() ? compare(e, nat, orig, native_pages, m, esp_entry) : "native threw: " + error;
    if (error.empty() && native_next != ret_addr) diff += (diff.empty() ? "" : "; ") + std::string("native returned to another address");
    m.mem.journal_end();
    st.verified++;
    if (!diff.empty()) {
        if (!error.empty()) st.errors++;
        st.mismatches++; g_mismatches++;
        if (st.first.empty()) st.first = diff;
        static const bool loud = std::getenv("REFLEX_PORT_LOG") != nullptr;
        if (loud || st.mismatches <= 3) std::fprintf(stderr, "[port] %s %08x call %llu: %s\n", e.name, e.address, (unsigned long long)st.calls, diff.c_str());
    }
    return ret_addr;
}

}  // namespace

FILE* heap_trace_file() {   // REFLEX_HEAP_TRACE=<file>: the boot's heap blocks made / freed, Verify's native / original runs marked
    static FILE* f = std::getenv("REFLEX_HEAP_TRACE") ? std::fopen(std::getenv("REFLEX_HEAP_TRACE"), "w") : nullptr;
    return f;
}
const reflex_vm::Native* installed(const Machine& m, uint32_t address) {
    auto it = g_installed.find(&m); if (it == g_installed.end()) return nullptr;
    auto f = it->second.find(address); return f == it->second.end() ? nullptr : &f->second;
}
void runtime_native(Machine& m, uint32_t address, reflex_vm::Native native) {
    auto& owned = g_owned[&m];
    if (native) { m.natives[address] = std::move(native); owned.erase(address); return; }
    if (auto p = installed(m, address)) { m.natives[address] = *p; owned.insert(address); }
    else { m.natives.erase(address); owned.erase(address); }
}
void set_host_state(Machine& m, HostSnapshot snapshot) { if (snapshot) g_host[&m] = std::move(snapshot); else g_host.erase(&m); }
std::function<void()> snapshot_host_state(Machine& m) {
    auto host = g_host.find(&m);
    return host != g_host.end() ? host->second() : std::function<void()>{};
}
const Stats& stats(uint32_t address) { return g_stats[address]; }
uint64_t total_mismatches() { return g_mismatches; }

static bool selected(const Entry& e, const std::string& filter) {
    if (filter.empty() || filter == "none") return false;
    bool in = false;
    std::stringstream ss(filter); std::string item;
    while (std::getline(ss, item, ',')) {
        bool neg = !item.empty() && item[0] == '-'; if (neg) item = item.substr(1);
        char* end = nullptr; unsigned long a = std::strtoul(item.c_str(), &end, 16);
        bool hex = !item.empty() && end && *end == 0 && item.size() >= 6;
        bool match = item == "all" || item == e.group || item == e.name || (hex && a == e.address);
        if (match) in = !neg;
    }
    return in;
}

int install(Machine& m, Mode mode, const std::string& filter) {
    int n = 0;
    auto& inst = g_installed[&m]; auto& owned = g_owned[&m];
    for (const Entry& e : registry()) {
        if (!selected(e, filter)) continue;
        if (mode == Mode::Off) {   // back to the original
            if (inst.erase(e.address)) n++;
            if (owned.erase(e.address)) m.natives.erase(e.address);
            continue;
        }
        const Entry* ep = &e;
        reflex_vm::Native fn;
        // import thunks (ports/thunks.cpp) continue at their target instead of returning: never verified, always on
        if (mode == Mode::On || std::strcmp(e.group, "thunk") == 0) fn = [ep](Machine& mm) -> uint32_t { if (g_phase == Phase::Original) return RUN_ORIGINAL; g_stats[ep->address].calls++; return ep->fn(mm); };
        else fn = [ep](Machine& mm) -> uint32_t {
            if (g_phase == Phase::Original) return RUN_ORIGINAL;
            if (g_phase == Phase::Native) return ep->fn(mm);
            return verify(mm, *ep);
        };
        inst[e.address] = fn;
        if (!m.natives.count(e.address) || owned.count(e.address)) { m.natives[e.address] = fn; owned.insert(e.address); }
        g_stats[e.address];
        n++;
    }
    return n;
}

int install_from_env(Machine& m) {
    const char* f = std::getenv("REFLEX_PORT");
    if (!f || !*f) return 0;
    const char* v = std::getenv("REFLEX_PORT_VERIFY");
    return install(m, v && *v == '1' ? Mode::Verify : Mode::On, f);
}

std::string report() {
    std::ostringstream o;
    for (const Entry& e : registry()) {
        auto it = g_stats.find(e.address); if (it == g_stats.end()) continue;
        const Stats& s = it->second;
        char b[256]; std::snprintf(b, sizeof b, "port %-34s %08x %-8s calls %10llu verified %10llu mismatches %llu", e.name, e.address, e.group,
                                   (unsigned long long)s.calls, (unsigned long long)s.verified, (unsigned long long)s.mismatches);
        o << b;
        if (s.unverifiable) o << " unverifiable " << s.unverifiable;   // (after the mismatch count: tools/verify.sh reads fields 6 and 10)
        if (!s.first.empty()) o << "  first: " << s.first; o << "\n";
    }
    return o.str();
}

}  // namespace reflex_port
