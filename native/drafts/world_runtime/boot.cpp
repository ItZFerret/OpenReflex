// Boot mode: the unchanged MXReflex.exe started from the player's install (see boot.h).
// Everything here is a stand-in for the operating system and devices; the game's own code
// (and the genuine VC90 runtime) does all the work. Threads are cooperative: a thread runs
// until it blocks, sleeps or uses up its time slice (an instruction boundary), so the
// schedule (and therefore the run) is deterministic for the same inputs.
#include "boot.h"
#include "deform.h"
#include "atv.h"
#include "career.h"
#include "clock_pin.h"
#include "tutorial.h"
#include "pe.h"
#include "port.h"
#include <cstdio>
namespace reflex_port { std::FILE* heap_trace_file(); }   // port.cpp: REFLEX_HEAP_TRACE (diagnostics)
#include "vm.h"
#include <algorithm>
#include <cctype>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <dirent.h>
#include <fcntl.h>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <zlib.h>

namespace reflex_vm {
using reflex_f80::F80;

#include "d3d9_device_methods.inc"
#include "fmod_event_system_slots.inc"
#include "fmod_event_slots.inc"
namespace {
constexpr uint32_t SENT = 0xec000000;          // import sentinels (16 bytes apart, named; clear of the world runtime's 0xfe000000)
constexpr uint32_t CODE = 0xed000000;          // host-implemented code addresses (COM methods, callbacks)
constexpr uint32_t THREAD_EXIT = 0xefff0000;   // return address of thread procedures
constexpr uint32_t FRAME_CALL_RETURN = 0xefff0010;   // return address of Boot::call_in_frame's call
constexpr uint32_t TEB = 0x7ffd0000;           // the interpreter's fs base (one TEB page, swapped per thread)
constexpr uint32_t PEB = 0x7ffdf000;
constexpr uint32_t MAIN_STACK_LO = 0x00030000, MAIN_STACK_HI = 0x00230000;   // 2 MiB, below the executable
constexpr uint32_t VA_LO = 0x10000000, VA_HI = 0x70000000;                    // VirtualAlloc / heaps / thread stacks
constexpr uint32_t KERNEL32 = 0x7c800000, USER32 = 0x7e410000, NTDLL = 0x7c900000;   // module handles of the stand-ins
constexpr uint32_t PSEUDO_PROCESS = 0xffffffffu, PSEUDO_THREAD = 0xfffffffeu;
constexpr uint32_t ENGINE_FRAME = 0x84f260;    // Engine::Frame (bootstrap.md)
constexpr uint64_t SLICE = 4000000;            // instructions per time slice
constexpr uint64_t WAIT_FOREVER = UINT64_MAX;
constexpr uint32_t PROCESS_HEAP = 0x00140000, DEVICE_HEAP = 0x00141000;   // heap handles (blocks share one allocator)

struct Ctx {
    uint32_t r[8] = {}, flags = 0x202, pc = 0;
    Xmm xmm[8];
    std::vector<F80> fpu;
    uint16_t cw = 0x27f, sw = 0;
    uint32_t mxcsr = 0x1f80;
    uint64_t mmx[8] = {};
    bool mmx_active = false;
};

struct Thread {
    uint32_t id = 0, handle = 0;
    Ctx ctx;
    std::vector<uint8_t> teb = std::vector<uint8_t>(4096, 0);
    enum State { Ready, Waiting, Done } state = Ready;
    bool suspended = false;
    // pending wait (the import's stack frame is still in place)
    std::vector<uint32_t> waits; bool wait_all = false; uint64_t wake_at = WAIT_FOREVER; uint32_t wait_cleanup = 0;
    bool wait_is_sleep = false; uint32_t wait_cs = 0;
    uint32_t exit_code = 0x103;   // STILL_ACTIVE
    uint32_t stack_lo = 0, stack_hi = 0;
    std::string name;
    uint32_t start = 0;
    // baton scheduling (BootConfig::baton): the host thread this guest thread runs on, and what a completed wait returns
    struct Fiber { std::thread th; std::condition_variable cv; bool go = false; };
    std::unique_ptr<Fiber> fiber;
    uint32_t wake_result = 0;
    uint32_t stop_pc = 0, stop_esp = 0; bool jit = false;   // Machine modes that belong to the running call (native/game Verify)
    uint64_t steps = 0;   // baton (not baton_slices): this thread's own instruction count, Machine::steps while it runs (so the
                          // instruction budgets of calls a native makes count only the calling thread's instructions)
};
struct FiberQuit {};               // unwinds a parked guest host thread when the boot ends (not a std::exception)
thread_local bool tl_guest = false;   // this host thread runs a guest thread (baton scheduling)

struct Obj {
    enum Kind { Event, Mutex, Thread_, File, Find, Heap, Other } kind = Other;
    bool manual = false, signaled = false;           // events
    uint32_t owner = 0, count = 0;                   // mutexes
    uint32_t thread = 0;                             // thread objects: thread id
    int fd = -1; std::string path; bool write = false; uint64_t pos = 0;   // files
    std::vector<std::string> entries; size_t next = 0; std::string dir;   // find handles
    std::string name;
    bool close_pending = false;                      // CloseHandle inside a verified call: the fd closes once that can no longer be undone
};

// Heap blocks: host-side metadata (the guest can't corrupt it), 16-byte granules.
struct HeapBlock { uint32_t size, heap; };

std::string lower(std::string s) { for (auto& c : s) c = char(std::tolower((unsigned char)c)); return s; }
bool wildcard(const char* p, const char* s) {   // case-insensitive * and ?
    if (!*p) return !*s;
    if (*p == '*') { for (; ; s++) { if (wildcard(p + 1, s)) return true; if (!*s) return false; } }
    if (!*s) return false;
    if (*p == '?' || std::tolower((unsigned char)*p) == std::tolower((unsigned char)*s)) return wildcard(p + 1, s + 1);
    return false;
}
}  // namespace

struct Boot::Impl {
    std::function<std::function<void()>()> fmod_snapshot;   // fmod_stand_ins.inc (the world runtime's Verify uses it; unused here)
    int32_t cursor_x = 0, cursor_y = 0;   // GetCursorPos (EventSpec::cursor_off_screen: modes.h)
    BootConfig cfg;
    Machine m;
    FILE* log = nullptr;
    // REFLEX_BOOT_THREADS=<file> (native/game notes/loading.md, tools/boot_threads.py): one line per thread run end, frame, signal and
    // thread creation. Observation only: the schedule is unchanged.
    FILE* tlog = nullptr;
    std::map<std::pair<uint32_t, uint32_t>, uint64_t> polls;   // (thread, return address) -> waits that timed out immediately
    std::map<uint32_t, uint64_t> spins;                         // baton: spin-site yields per site
    // REFLEX_COVERAGE_THREADS=<prefix>: coverage per thread kind (start address; 0 = the main thread), <prefix>-<start>.bin (tools/coverage.py)
    std::map<uint32_t, std::vector<uint32_t>> thread_cov;
    void cover(Thread& t) {
        static const char* prefix = std::getenv("REFLEX_COVERAGE_THREADS"); if (!prefix) return;
        auto& v = thread_cov[t.start]; if (v.empty()) v.assign(0x600000, 0);
        m.pc_counts = v.data();
    }
    void write_thread_coverage() {
        const char* prefix = std::getenv("REFLEX_COVERAGE_THREADS"); if (!prefix) return;
        for (auto& [start, v] : thread_cov) {
            char name[64]; std::snprintf(name, sizeof name, "-%08x.bin", start);
            FILE* f = std::fopen((std::string(prefix) + name).c_str(), "wb"); if (!f) continue;
            uint32_t hdr[4] = {0x56435852u, 1, 0, 0}; std::fwrite(hdr, 4, 4, f); std::fwrite(v.data(), 4, v.size(), f); std::fclose(f);
        }
    }
    void tl(const char* fmt, ...) {
        if (!tlog) return;
        va_list a; va_start(a, fmt); std::vfprintf(tlog, fmt, a); va_end(a); std::fputc('\n', tlog);
    }
    void say(const char* fmt, ...) {
        if (!log) return;
        va_list a; va_start(a, fmt); std::vfprintf(log, fmt, a); va_end(a); std::fputc('\n', log); std::fflush(log);
    }
    // ---------------------------------------------------------------- modules
    struct Mod { std::string name; uint32_t base, size, entry; std::map<std::string, uint32_t> exports; };
    std::vector<Mod> mods;
    uint32_t next_sentinel = SENT;
    uint32_t cmdline_a = 0, cmdline_w = 0;   // GetCommandLineA / W blocks (per Boot; undone with the host state in Verify)
    std::map<std::string, uint32_t> sentinel_of;   // "dll!name" -> sentinel
    uint32_t sentinel(const std::string& full) {
        auto it = sentinel_of.find(full);
        if (it != sentinel_of.end()) return it->second;
        uint32_t s = next_sentinel; next_sentinel += 16;
        m.import_names[s] = full; sentinel_of[full] = s;
        on_undo([this, s, full] { m.import_names.erase(s); sentinel_of.erase(full); next_sentinel -= 16; });
        return s;
    }
    uint32_t next_code = CODE;
    uint32_t native(Native fn) { uint32_t a = next_code; next_code += 4; m.natives[a] = std::move(fn); on_undo([this, a] { m.natives.erase(a); next_code -= 4; }); return a; }
    // ---------------------------------------------------------------- memory
    std::map<uint32_t, uint32_t> regions;          // VirtualAlloc reservations: base -> size
    // Address ranges: game memory (VirtualAlloc, heaps, stacks) below the world runtime's
    // deterministic heap (0x30000000) and device memory (resource contents, left out of world
    // images) at 0x40000000..0x60000000, clear of the runtime's deformation bits (0x60000000).
    uint32_t valloc(uint32_t size, uint32_t want = 0, uint32_t lo = VA_LO, uint32_t hi = 0x30000000) {
        size = (size + 0xffff) & ~0xffffu;
        uint32_t at = lo;
        if (want) {
            want &= ~0xffffu;
            for (auto& [b, s] : regions) if (want < b + s && b < want + size) return 0;
            at = want;
        } else {
            for (auto it = regions.lower_bound(0); it != regions.end(); ++it) {
                uint32_t b = it->first, sz = it->second;
                if (b + sz <= at) continue;
                if (b >= at + size) break;
                at = std::max(at, (b + sz + 0xffff) & ~0xffffu);
            }
            if (uint64_t(at) + size > hi) throw VmError("virtual address space exhausted");
        }
        regions[at] = size;
        on_undo([this, at] { regions.erase(at); });   // (the pages stay backed: zero until written, as before the reservation)
        for (uint32_t a = at; a < at + size; a += 4096) m.mem.back(a);
        return at;
    }
    void vfree(uint32_t base) {
        auto it = regions.find(base); if (it == regions.end()) return;
        would_switch();   // unmapped pages cannot be put back
        m.mem.unmap(base, it->second); regions.erase(it);
    }
    // heaps: per-size free lists for small blocks, size-ordered best fit for large ones
    struct Pool {
        uint32_t lo, hi, arena = 0, top = 0, end = 0;
        std::map<uint32_t, std::vector<uint32_t>> small_free;     // granules (<= 64) -> addresses
        std::multimap<uint32_t, uint32_t> large_free;             // size -> address
    };
    Pool game_pool{VA_LO, 0x30000000}, device_pool{0x40000000, 0x60000000};
    std::unordered_map<uint32_t, HeapBlock> blocks;
    uint32_t process_heap = 0, next_heap = 0x00150000;
    std::map<uint32_t, uint32_t> heap_flags;
    // large free list entries in their exact multimap order (Verify's undo puts an entry back where it was: the next best fit depends on it)
    static size_t rank_of(std::multimap<uint32_t, uint32_t>& l, std::multimap<uint32_t, uint32_t>::iterator it) { size_t k = 0; for (auto i = l.lower_bound(it->first); i != it; ++i) k++; return k; }
    static void insert_at(std::multimap<uint32_t, uint32_t>& l, uint32_t size, uint32_t a, size_t rank) {
        auto i = l.lower_bound(size); for (size_t k = 0; k < rank && i != l.end() && i->first == size; k++) ++i; l.emplace_hint(i, size, a); }
    static void erase_entry(std::multimap<uint32_t, uint32_t>& l, uint32_t size, uint32_t a) {
        auto r = l.equal_range(size); for (auto i = r.first; i != r.second; ++i) if (i->second == a) { l.erase(i); return; } }
    // REFLEX_HEAP_TRACE=<file> (diagnostics, native/game port.cpp): every block made / freed, with the exe return addresses on the stack
    void heap_trace(FILE* tf, char op, uint32_t a, uint32_t size) {
        std::fprintf(tf, "%c %08x %x @%llu", op, a, size, (unsigned long long)m.steps);
        for (uint32_t sp = m.r[4], k = 0, n = 0; k < 256 && n < 6 && m.mem.mapped(sp); sp += 4, k++) { uint32_t v = uint32_t(m.mem.read(sp, 4)); if (v > 0x401000 && v < 0x916000) { std::fprintf(tf, " %08x", v); n++; } }
        std::fputc('\n', tf);
    }
    uint32_t halloc(uint32_t heap, uint32_t size, bool zero) {
        Pool& p = heap == 0x00141000 ? device_pool : game_pool;
        uint32_t need = std::max<uint32_t>(16, (size + 15) & ~15u), g = need / 16, a = 0;
        const bool log = logging();
        if (g <= 64) {
            auto& list = p.small_free[g];
            if (!list.empty()) { a = list.back(); list.pop_back(); if (log) on_undo([&p, g, a] { p.small_free[g].push_back(a); }); }
        } else {
            auto it = p.large_free.lower_bound(need);
            if (it != p.large_free.end()) {
                a = it->second; uint32_t have = it->first; size_t rank = log ? rank_of(p.large_free, it) : 0; p.large_free.erase(it);
                bool split = have - need >= 1024;
                if (split) p.large_free.emplace(have - need, a + need); else need = have;
                if (log) on_undo([&p, a, have, need, split, rank] { if (split) erase_entry(p.large_free, have - need, a + need); insert_at(p.large_free, have, a, rank); });
            }
        }
        if (!a) {
            const uint32_t old_arena = p.arena, old_top = p.top, old_end = p.end; bool fresh = false;
            if (p.top + need > p.end) {
                uint32_t chunk = std::max<uint32_t>(need, 32u << 20);
                p.arena = valloc(chunk, 0, p.lo, p.hi); p.top = p.arena; p.end = p.arena + ((chunk + 0xffff) & ~0xffffu);
                fresh = arenas.insert(p.arena).second;
            }
            a = p.top; p.top += need;
            if (log) on_undo([this, &p, old_arena, old_top, old_end, fresh, arena = p.arena] { p.arena = old_arena; p.top = old_top; p.end = old_end; if (fresh) arenas.erase(arena); });
        }
        blocks[a] = {size, heap};
        alloc_sizes[a] = need;
        if (FILE* tf = reflex_port::heap_trace_file()) heap_trace(tf, 'A', a, size);
        if (log) on_undo([this, a] { blocks.erase(a); alloc_sizes.erase(a); });
        if (zero) m.mem.write_block(a, nullptr, need);
        return a;
    }
    std::unordered_map<uint32_t, uint32_t> alloc_sizes;      // address -> granule-rounded size
    std::set<uint32_t> arenas;                                // VirtualAlloc regions that hold heap blocks (write_canonical)
    bool hfree(uint32_t a) {
        auto it = blocks.find(a); if (it == blocks.end()) return false;
        Pool& p = it->second.heap == 0x00141000 ? device_pool : game_pool;
        const HeapBlock hb = it->second;
        uint32_t need = alloc_sizes[a]; blocks.erase(it); alloc_sizes.erase(a);
        if (FILE* tf = reflex_port::heap_trace_file()) heap_trace(tf, 'F', a, hb.size);
        if (need / 16 <= 64) p.small_free[need / 16].push_back(a); else p.large_free.emplace(need, a);
        if (logging()) on_undo([this, &p, a, hb, need] { if (need / 16 <= 64) p.small_free[need / 16].pop_back(); else erase_entry(p.large_free, need, a); blocks[a] = hb; alloc_sizes[a] = need; });
        return true;
    }
    // ---------------------------------------------------------------- handles
    std::map<uint32_t, std::shared_ptr<Obj>> handles;
    uint32_t next_handle = 0x100;
    uint32_t add_handle(std::shared_ptr<Obj> o) { uint32_t h = next_handle; next_handle += 4; handles[h] = std::move(o); on_undo([this, h] { handles.erase(h); next_handle -= 4; }); return h; }
    void obj_undo(uint32_t h) {   // a handle's object as it is before a change (events, mutexes, file positions, find handles): Verify's undo
        if (!logging()) return;
        auto it = handles.find(h); if (it == handles.end()) return;
        std::shared_ptr<Obj> keep = it->second; Obj saved = *keep; on_undo([keep, saved] { *keep = saved; });
    }
    Obj* obj(uint32_t h) { auto it = handles.find(h); return it == handles.end() ? nullptr : it->second.get(); }
    uint32_t last_error = 0;
    // ---------------------------------------------------------------- threads
    std::vector<std::unique_ptr<Thread>> threads;
    size_t cur = 0;
    uint32_t next_tid = 0x1000;
    uint64_t now_ = 0;                     // virtual time, 100 ns
    uint64_t steps_at_time = 0;            // instruction-driven time base
    uint64_t frame_dt = 0;                 // host frame mode: time per frame (0: instruction time)
    uint64_t fixed_dt = 0;                 // REFLEX_FIXED_CLOCK: pinned time per frame (0: off).  run_frames falls back to it.
    bool yield_request = false, stop_request = false, resume_bypass = false;
    uint64_t frames = 0, stop_at = 0;
    Ctx frame_call_saved; bool frame_call_done = false; uint32_t frame_call_result = 0;   // Boot::call_in_frame (MOTOCAREER)
    std::map<uint32_t, std::pair<uint32_t, uint32_t>> cs_owner;   // critical section -> (owner tid, recursion)
    uint32_t tls_next = 0; std::vector<uint8_t> tls_used = std::vector<uint8_t>(64, 0);
    uint32_t exe_tls_index_addr = 0, exe_tls_start = 0, exe_tls_end = 0, exe_tls_zero = 0, exe_tls_index = 0;
    uint8_t keys[256] = {};
    // device resources for the world writer: object -> (format, width, height, levels, level addresses, pitches)
    struct TexInfo { uint32_t format = 0, width = 0, height = 0; std::vector<uint32_t> addr, pitch, w, h; };
    std::function<bool(uint32_t, TexInfo&)> texture_info;
    Boot::D3DHook d3d_hook; std::function<bool(uint32_t, Boot::D3DResource&)> d3d_resource, d3dx_constant_table;   // RENDER CAPTURE (render_capture.cpp)
    uint32_t physics_context = 0;
    uint32_t bink_file_opens = 0;   // BinkOpen of a .bik file (the loading page's logo video: Boot::start_event's capture moment under baton)
    // ---------------------------------------------------------------- native/game Verify inside the boot (notes/loading.md)
    // While a verified call runs (Machine::no_switch > 0) no guest thread switch may happen and every change of the host state is
    // logged with its undo: Verify puts the native run's changes back (with the guest memory) before the original runs. A switch, or a
    // change that cannot be taken back (file writes, unmapping), throws WouldSwitch instead: the original then runs unverified.
    std::vector<std::function<void()>> undo_log;
    uint64_t undo_epoch = 1;   // per snapshot and per restore: device states snapshotted lazily (once per epoch: touch_*)
    bool logging() const { return m.no_switch > 0; }
    void on_undo(std::function<void()> f) { if (m.no_switch > 0) undo_log.push_back(std::move(f)); }
    void would_switch() const {
        if (m.no_switch > 0) {
            // REFLEX_WOULD_SWITCH_TRACE (w-testride, diagnostics): the guest return addresses on the stack where a verified call would switch
            if (std::getenv("REFLEX_WOULD_SWITCH_TRACE")) {
                std::fprintf(stderr, "would switch: esp %08x:", m.r[4]);
                for (uint32_t i = 0, n = 0; i < 0x400 && n < 24; i += 4) { auto& mm = const_cast<Machine&>(m); uint32_t v = mm.mem.mapped(mm.r[4] + i) ? uint32_t(mm.mem.read(mm.r[4] + i, 4)) : 0; if (v >= 0x401000 && v < 0x917000) { std::fprintf(stderr, " %x:%08x", i, v); n++; } }
                std::fprintf(stderr, "\n");
            }
            throw WouldSwitch{};
        }
    }
    std::vector<std::shared_ptr<Obj>> pending_close;   // files closed inside verified calls (Obj::close_pending)
    void commit_closes() {
        for (auto& o : pending_close) if (o->close_pending && o->fd >= 0) { close(o->fd); o->fd = -1; o->close_pending = false; }
        pending_close.clear();
    }
    void cs_undo(uint32_t cs) {   // a critical section's owner / recursion before a change
        if (!logging()) return;
        auto it = cs_owner.find(cs); bool had = it != cs_owner.end(); std::pair<uint32_t, uint32_t> v = had ? it->second : std::pair<uint32_t, uint32_t>{0, 0};
        on_undo([this, cs, had, v] { if (had) cs_owner[cs] = v; else cs_owner.erase(cs); });
    }
    struct HostSaved {
        uint64_t now = 0, steps_at_time = 0; uint32_t last_error = 0, physics_context = 0; size_t cur = 0;
        Thread::State state = Thread::Ready; std::vector<uint32_t> waits; bool wait_all = false, wait_is_sleep = false;
        uint64_t wake_at = 0; uint32_t wait_cleanup = 0, wait_cs = 0, exit_code = 0, wake_result = 0;
    } host_saved;
    std::function<void()> host_snapshot() {   // reflex_port::set_host_state: Verify's snapshot (verified calls never nest)
        commit_closes(); undo_log.clear(); undo_epoch++;
        Thread& t = T(); HostSaved& h = host_saved;
        h.now = now_; h.steps_at_time = steps_at_time; h.last_error = last_error; h.physics_context = physics_context; h.cur = cur;
        h.state = t.state; h.waits = t.waits; h.wait_all = t.wait_all; h.wait_is_sleep = t.wait_is_sleep; h.wake_at = t.wake_at;
        h.wait_cleanup = t.wait_cleanup; h.wait_cs = t.wait_cs; h.exit_code = t.exit_code; h.wake_result = t.wake_result;
        return [this]() { host_restore(); };
    }
    void host_restore() {   // callable more than once (the native's changes, then the original's after a WouldSwitch)
        std::vector<std::function<void()>> log; log.swap(undo_log);
        for (auto it = log.rbegin(); it != log.rend(); ++it) (*it)();
        undo_epoch++;
        const HostSaved& h = host_saved;
        now_ = h.now; steps_at_time = h.steps_at_time; last_error = h.last_error; physics_context = h.physics_context; cur = h.cur;
        Thread& t = T();
        t.state = h.state; t.waits = h.waits; t.wait_all = h.wait_all; t.wait_is_sleep = h.wait_is_sleep; t.wake_at = h.wake_at;
        t.wait_cleanup = h.wait_cleanup; t.wait_cs = h.wait_cs; t.exit_code = h.exit_code; t.wake_result = h.wake_result;
    }
    Thread& T() { return *threads[cur]; }
    void advance_time() {
        if (frame_dt || (cfg.baton && !cfg.baton_slices)) return;   // baton scheduling: logical time (QueryPerformanceCounter reads, timeouts)
        uint64_t s = m.steps;
        if (s > steps_at_time) { now_ += (s - steps_at_time) / 100; steps_at_time = s - (s - steps_at_time) % 100; }   // 1 GIPS virtual CPU
    }
    void save(Thread& t, uint32_t pc) {
        Ctx& c = t.ctx; std::memcpy(c.r, m.r, sizeof c.r); c.flags = m.flags; c.pc = pc;
        std::memcpy(c.xmm, m.xmm, sizeof c.xmm); c.fpu = m.fpu; c.cw = m.fpu_cw; c.sw = m.fpu_sw; c.mxcsr = m.mxcsr;
        std::memcpy(c.mmx, m.mmx, sizeof c.mmx); c.mmx_active = m.mmx_active;
        m.mem.read_block(TEB, t.teb.data(), 4096);
    }
    void load(Thread& t) {
        Ctx& c = t.ctx; std::memcpy(m.r, c.r, sizeof c.r); m.flags = c.flags;
        std::memcpy(m.xmm, c.xmm, sizeof c.xmm); m.fpu = c.fpu; m.fpu_cw = c.cw; m.fpu_sw = c.sw; m.mxcsr = c.mxcsr;
        std::memcpy(m.mmx, c.mmx, sizeof c.mmx); m.mmx_active = c.mmx_active;
        m.mem.write_block(TEB, t.teb.data(), 4096);
    }
    Thread* thread_by_id(uint32_t id) { for (auto& t : threads) if (t->id == id) return t.get(); return nullptr; }
    bool signaled(uint32_t h, uint32_t tid) {
        if (h == PSEUDO_THREAD) return false;
        Obj* o = obj(h); if (!o) return true;   // closed/invalid handles: treat as signaled (WAIT_FAILED in reality)
        switch (o->kind) {
            case Obj::Event: return o->signaled;
            case Obj::Mutex: return o->count == 0 || o->owner == tid;
            case Obj::Thread_: { Thread* t = thread_by_id(o->thread); return !t || t->state == Thread::Done; }
            default: return true;
        }
    }
    void consume(uint32_t h, uint32_t tid) {
        Obj* o = obj(h); if (!o) return;
        if (o->kind == Obj::Event || o->kind == Obj::Mutex) obj_undo(h);
        if (o->kind == Obj::Event && !o->manual) o->signaled = false;
        if (o->kind == Obj::Mutex) { o->owner = tid; o->count++; }
    }
    // Is the waiting thread ready? Returns the wait result in *result.
    bool wait_ready(Thread& t, uint32_t* result) {
        if (t.wait_cs) {
            auto& cs = cs_owner[t.wait_cs];
            if (cs.second == 0 || cs.first == t.id) { cs_undo(t.wait_cs); cs = {t.id, cs.first == t.id ? cs.second + 1 : 1}; *result = 0; return true; }
            return false;
        }
        if (t.wait_is_sleep) { if (now_ >= t.wake_at) { *result = 0; return true; } return false; }
        if (t.wait_all) {
            bool all = true; for (uint32_t h : t.waits) all = all && signaled(h, t.id);
            if (all) { for (uint32_t h : t.waits) consume(h, t.id); *result = 0; return true; }
        } else {
            for (size_t i = 0; i < t.waits.size(); i++)
                if (signaled(t.waits[i], t.id)) { consume(t.waits[i], t.id); *result = uint32_t(i); return true; }
        }
        if (now_ >= t.wake_at) { *result = 0x102; return true; }   // WAIT_TIMEOUT
        return false;
    }
    // A blocking import: `cleanup` stdcall bytes; the thread resumes when the wait completes.
    uint32_t block(uint32_t cleanup) {
        Thread& t = T(); t.state = Thread::Waiting; t.wait_cleanup = cleanup;
        uint32_t r;
        if (wait_ready(t, &r)) {
            if (tlog && r == 0x102) polls[{t.id, uint32_t(m.mem.read(m.r[4], 4))}]++;   // a wait that timed out at once (a poll: REFLEX_BOOT_THREADS)
            t.state = Thread::Ready; t.waits.clear(); t.wait_is_sleep = false; t.wait_cs = 0;
            // baton: a poll that fails (zero timeout) lets the others run first, as they would on another core; then it looks again
            if (r == 0x102 && cfg.baton && !cfg.baton_slices && tl_guest && t.wake_at == now_) {
                std::vector<uint32_t> w = t.waits; bool all = t.wait_all;
                yield(t);
                t.waits = w; t.wait_all = all; t.wake_at = now_;
                if (!wait_ready(t, &r)) r = 0x102;
                t.waits.clear();
            }
            return m.ret(cleanup, r);
        }
        would_switch();   // (a verified call: the wait does not complete at once)
        if (cfg.baton && tl_guest) { park(t, 0); return m.ret(cleanup, t.wake_result); }   // the others run until the wait completes
        yield_request = true; return 0;   // run() returns; the scheduler switches threads
    }
    // ---------------------------------------------------------------- baton scheduling (BootConfig::baton)
    // Every guest thread runs on its own host thread; exactly one (or the driver: the caller of Boot's functions) holds the baton.
    // A guest thread gives it away only where it cannot or will not continue (park): blocked, yielding, exited, or at the frame stop
    // (back to the driver). Natives can therefore block at any call depth, and the schedule depends only on the synchronization.
    std::mutex mu;
    Thread::Fiber driver;
    std::exception_ptr fiber_error;
    bool quitting = false;
    size_t index_of(const Thread& t) const { for (size_t i = 0; i < threads.size(); i++) if (threads[i].get() == &t) return i; return 0; }
    // the next thread to run, round robin from `from` (a waiting thread whose wait completes gets its result); time advances to the
    // earliest timeout when every thread waits
    Thread& pick(size_t from) {
        for (;;) {
            size_t n = threads.size();
            for (size_t k = 0; k < n; k++) {
                Thread& t = *threads[(from + k) % n]; uint32_t result = 0;
                if (t.state == Thread::Done || t.suspended) continue;
                if (t.state == Thread::Ready) return t;
                if (wait_ready(t, &result)) { t.state = Thread::Ready; t.waits.clear(); t.wait_is_sleep = false; t.wait_cs = 0; t.wake_result = result; return t; }
            }
            uint64_t wake = WAIT_FOREVER; Thread* first = nullptr;
            for (auto& t : threads) if (t->state == Thread::Waiting && !t->suspended && t->wake_at < wake) { wake = t->wake_at; first = t.get(); }
            if (wake == WAIT_FOREVER) throw VmError("deadlock: every thread waits forever\n" + describe());
            if (tlog && first && wake > now_) tl("W %x %llu %08x", first->id, (unsigned long long)(wake - now_), first->ctx.r[4] ? uint32_t(m.mem.read(first->ctx.r[4], 4)) : 0u);   // time jumps to a timeout
            now_ = std::max(now_, wake);
        }
    }
    void hand(Thread::Fiber& from, Thread::Fiber& to) {   // the caller holds the baton; returns when it has it again
        std::unique_lock<std::mutex> lk(mu);
        from.go = false; to.go = true; to.cv.notify_one();
        from.cv.wait(lk, [&] { return from.go || (quitting && &from != &driver); });
        if (!from.go) throw FiberQuit{};
    }
    void run_fiber(Thread& t) {   // host thread of a guest thread
        tl_guest = true;
        { std::unique_lock<std::mutex> lk(mu); t.fiber->cv.wait(lk, [&] { return t.fiber->go || quitting; }); if (!t.fiber->go) return; }
        try {
            load(t); resumed(t);
            // REFLEX_BOOT_WATCHDOG=<n> (diagnostics, top-level code only): report a guest thread that runs n instructions without a switch
            static const uint64_t watchdog = std::getenv("REFLEX_BOOT_WATCHDOG") ? std::strtoull(std::getenv("REFLEX_BOOT_WATCHDOG"), nullptr, 10) : 0;
            uint32_t pc = t.ctx.pc;
            for (;;) {
                if (watchdog && !cfg.baton_slices) m.slice_end = m.steps + watchdog;
                pc = m.run(pc, UINT64_MAX / 4);
                if (m.sliced && cfg.baton_slices) { preempt(t); continue; }
                if (!m.sliced) break;
                m.sliced = false; m.slice_end = UINT64_MAX;
                std::string w; char x[16];
                for (uint32_t a = m.r[4], k = 0, n = 0; k < 512 && n < 10 && m.mem.mapped(a); a += 4, k++) { uint32_t v = uint32_t(m.mem.read(a, 4)); if ((v > 0x401000 && v < 0x916000) || (v >= 0x78400000 && v < 0x78600000)) { std::snprintf(x, sizeof x, " %08x", v); w += x; n++; } }
                if (m.import_names.count(pc)) w += " import " + m.import_names[pc];
                if (const char* wt = std::getenv("REFLEX_BOOT_WATCHDOG_TRACE"); wt && !m.trace_out && m.steps - resumed_at > 3 * watchdog) {   // 1M instructions, interpreted
                    m.trace_out = std::fopen(wt, "w"); m.trace_from = m.steps; m.trace_to = m.steps + 1000000; m.enable_jit(false);
                }
                std::fprintf(stderr, "watchdog: thread %x at %08x after %llu instructions without a switch; stack%s\n", t.id, pc, (unsigned long long)(m.steps - resumed_at), w.c_str());
            }
            throw VmError("thread returned to address 0");
        } catch (const FiberQuit&) { return; }
        catch (const VmError& e) {
            char b[200]; std::snprintf(b, sizeof b, "\n  in thread %x: eax %08x ecx %08x edx %08x ebx %08x esp %08x ebp %08x esi %08x edi %08x", t.id, m.r[0], m.r[1], m.r[2], m.r[3], m.r[4], m.r[5], m.r[6], m.r[7]);
            std::string msg = e.what() + std::string(b) + "\n  stack:";
            for (uint32_t a = m.r[4], k = 0; k < 24; a += 4, k++) { char w[16]; std::snprintf(w, sizeof w, " %08x", m.mem.mapped(a) ? uint32_t(m.mem.read(a, 4)) : 0u); msg += w; }
            if (const char* dm = std::getenv("REFLEX_BOOT_ERROR_DUMP")) {   // diagnostics: "addr,len" of guest memory with the error
                uint32_t a0 = uint32_t(std::strtoul(dm, nullptr, 16)), n = uint32_t(std::strtoul(std::strchr(dm, ',') ? std::strchr(dm, ',') + 1 : "40", nullptr, 16));
                msg += "\n  dump:"; std::string txt;
                for (uint32_t a = a0; a < a0 + n; a += 4) { char w[16]; uint32_t v = m.mem.mapped(a) ? uint32_t(m.mem.read(a, 4)) : 0u; std::snprintf(w, sizeof w, " %08x", v); msg += w;
                    for (int i = 0; i < 4; i++) { char c = char(v >> (8 * i)); txt += (c >= 32 && c < 127) ? c : '.'; } }
                msg += "\n  text: " + txt;
            }
            fiber_error = std::make_exception_ptr(VmError(msg));
        } catch (...) { fiber_error = std::current_exception(); }
        std::unique_lock<std::mutex> lk(mu); t.fiber->go = false; driver.go = true; driver.cv.notify_one();   // the boot stops
    }
    void switch_to(Thread& from_t, Thread::Fiber& from, Thread& to) {
        cur = index_of(to);
        if (&to == &from_t) return;
        if (!to.fiber) { to.fiber = std::make_unique<Thread::Fiber>(); Thread* tp = &to; to.fiber->th = std::thread([this, tp] { run_fiber(*tp); }); }
        hand(from, *to.fiber);
    }
    // The running guest thread t stops here (its state says why); the others run until t is picked again. pc: kept in t.ctx.pc.
    uint64_t resumed_at = 0;   // m.steps when the running guest thread got the baton (REFLEX_BOOT_THREADS)
    void trace_park(Thread& t, char why) {
        if (!tlog) return;
        std::string w; char x[48];
        uint32_t at = m.mem.mapped(m.r[4]) ? uint32_t(m.mem.read(m.r[4], 4)) : 0u;
        for (uint32_t a = m.r[4] + 4, k = 0, n = 0; k < 256 && n < 4 && m.mem.mapped(a); a += 4, k++) {
            uint32_t v = uint32_t(m.mem.read(a, 4)); if (v > 0x401000 && v < 0x916000) { std::snprintf(x, sizeof x, " %08x", v); w += x; n++; } }
        for (; w.size() < 36;) w += " -";
        if (why == 'B') for (uint32_t h : t.waits) { Obj* o = obj(h); std::snprintf(x, sizeof x, " h%x:%s", h, o && !o->name.empty() ? o->name.c_str() : "-"); w += x; }
        if (why == 'C') { std::snprintf(x, sizeof x, " cs%08x:%x", t.wait_cs, cs_owner[t.wait_cs].first); w += x; }
        tl("R %x %llu %c %08x%s", t.id, (unsigned long long)(m.steps - resumed_at), why, at, w.c_str());
    }
    uint64_t slice_len() const { static const uint64_t sl = std::getenv("REFLEX_SLICE") ? std::strtoull(std::getenv("REFLEX_SLICE"), nullptr, 10) : SLICE; return sl; }
    uint64_t other_steps = 0;   // baton: the instructions of the threads that are not running (Machine::steps counts the running one's)
    void leaving(Thread& t) { if (cfg.baton && !cfg.baton_slices) { t.steps = m.steps; } }
    void resumed(Thread& t) {
        cover(t);
        if (cfg.baton && !cfg.baton_slices) m.steps = t.steps;
        resumed_at = m.steps; if (cfg.baton_slices) { m.slice_end = m.steps + slice_len(); m.sliced = false; }
    }
    // baton_slices: the running thread's slice ended (top-level code): the others run first
    void preempt(Thread& t) {
        would_switch();
        advance_time();
        if (tlog) tl("R %x %llu S %08x", t.id, (unsigned long long)(m.steps - resumed_at), 0u);
        t.state = Thread::Ready;
        save(t, 0); size_t me = index_of(t); leaving(t);
        switch_to(t, *t.fiber, pick((me + 1) % threads.size()));
        cur = index_of(t); load(t); resumed(t);
    }
    void park(Thread& t, uint32_t pc, bool to_driver = false) {
        would_switch();
        advance_time();
        trace_park(t, to_driver ? 'F' : t.state == Thread::Done ? 'X' : t.wait_is_sleep ? 'L' : t.wait_cs ? 'C' : 'B');
        save(t, pc); t.stop_pc = m.stop_pc; t.stop_esp = m.stop_esp; t.jit = m.jit_enabled; leaving(t);
        size_t me = index_of(t);
        if (to_driver) { cur = (me + 1) % threads.size(); hand(*t.fiber, driver); }
        else switch_to(t, *t.fiber, pick((me + 1) % threads.size()));
        cur = index_of(t);
        load(t); m.stop_pc = t.stop_pc; m.stop_esp = t.stop_esp; m.jit_enabled = t.jit; resumed(t);
    }
    // Sleep(0) / SwitchToThread / a failed poll: the others first. Alone (every other thread waits), the yield costs 10 us of virtual time
    // (as a QueryPerformanceCounter read), so a spin that waits for a timeout still gets there. (Until 2026-10-18 a lonely yield jumped to
    // the earliest timeout: the forest cull thread's failed quit poll (00662ef5) then made every race frame last ~1 s, up to the saved-game
    // manager's timeout, and the countdown of National / FreeRide events had begun when the world was captured.)
    void yield(Thread& t) {
        would_switch();   // (also alone: a verified call's yield is a switch point)
        advance_time();
        t.state = Thread::Ready;
        size_t me = index_of(t); Thread& next = pick((me + 1) % threads.size());
        if (&next == &t && cfg.baton_slices) { if (tlog) tl("R %x %llu Y %08x", t.id, (unsigned long long)(m.steps - resumed_at), 0u); resumed(t); return; }
        if (&next == &t) {
            uint64_t wake = WAIT_FOREVER; Thread* first = nullptr;
            for (auto& o : threads) if (o->state == Thread::Waiting && !o->suspended && o->wake_at < wake) { wake = o->wake_at; first = o.get(); }
            if (tlog) tl("V %x %llu %x %08x", t.id, (unsigned long long)(wake == WAIT_FOREVER ? 0 : wake - std::min(wake, now_)), first ? first->id : 0u,
                         first && first->ctx.r[4] ? uint32_t(m.mem.read(first->ctx.r[4], 4)) : 0u);   // a lonely yield (REFLEX_BOOT_THREADS: the earliest timeout ahead)
            now_ += 100;
            return;
        }
        trace_park(t, 'Y');
        save(t, 0); t.stop_pc = m.stop_pc; t.stop_esp = m.stop_esp; t.jit = m.jit_enabled; leaving(t);
        switch_to(t, *t.fiber, next);
        cur = index_of(t);
        load(t); m.stop_pc = t.stop_pc; m.stop_esp = t.stop_esp; m.jit_enabled = t.jit; resumed(t);
    }
    // driver: run the guest threads until the main thread's frame stop (or a failure)
    void schedule_baton(uint64_t frames_to_run) {
        stop_at = frames + frames_to_run; stop_request = false;
        Thread& next = pick(cur);
        cur = index_of(next);
        if (!next.fiber) { next.fiber = std::make_unique<Thread::Fiber>(); Thread* tp = &next; next.fiber->th = std::thread([this, tp] { run_fiber(*tp); }); }
        hand(driver, *next.fiber);
        m.slice_end = UINT64_MAX; m.sliced = false;   // the driver's own calls (Boot::call) are never sliced
        if (fiber_error) { auto e = fiber_error; std::rethrow_exception(e); }
    }
    void quit_fibers() {
        { std::lock_guard<std::mutex> lk(mu); quitting = true; for (auto& t : threads) if (t->fiber) t->fiber->cv.notify_all(); }
        for (auto& t : threads) if (t->fiber && t->fiber->th.joinable()) t->fiber->th.join();
    }
    // ---------------------------------------------------------------- files
    std::string cwd = "C:\\Reflex";
    std::string guest_exe = "C:\\Reflex\\MXReflex.exe";
    std::string norm(std::string g) {   // absolute, backslashes, no . / ..
        for (auto& c : g) if (c == '/') c = '\\';
        if (g.size() >= 2 && g[1] == ':') {}
        else if (!g.empty() && g[0] == '\\') g = "C:" + g;
        else g = cwd + "\\" + g;
        std::vector<std::string> parts; std::string part;
        for (size_t i = 3; i <= g.size(); i++) {
            if (i == g.size() || g[i] == '\\') {
                if (part == "..") { if (!parts.empty()) parts.pop_back(); }
                else if (!part.empty() && part != ".") parts.push_back(part);
                part.clear();
            } else part += g[i];
        }
        std::string out = std::string(1, char(std::toupper((unsigned char)g[0]))) + ":";
        for (auto& p : parts) out += "\\" + p;
        if (parts.empty()) out += "\\";
        return out;
    }
    // Host paths for a guest path: the install (read-only) and the writable overlay.
    void host_paths(const std::string& guest, std::string& game, std::string& overlay) {
        std::string g = norm(guest), l = lower(g), rel;
        if (l == "c:\\reflex" || l.rfind("c:\\reflex\\", 0) == 0) {
            rel = g.size() > 10 ? g.substr(10) : "";
            for (auto& c : rel) if (c == '\\') c = '/';
            game = cfg.game_dir + (rel.empty() ? "" : "/" + rel);
            overlay = cfg.save_dir + "/game" + (rel.empty() ? "" : "/" + rel);
        } else {
            rel = g.substr(3); for (auto& c : rel) if (c == '\\') c = '/';
            game.clear(); overlay = cfg.save_dir + "/drive_c/" + rel;
        }
    }
    static bool exists(const std::string& p, bool* dir = nullptr) {
        struct stat st; if (p.empty() || stat(p.c_str(), &st) != 0) return false;
        if (dir) *dir = S_ISDIR(st.st_mode); return true;
    }
    static void mkdirs(const std::string& path) {
        for (size_t i = 1; i < path.size(); i++) if (path[i] == '/') { std::string d = path.substr(0, i); mkdir(d.c_str(), 0755); }
    }
    std::string read_path(const std::string& guest) {   // where a read of this guest path comes from
        std::string game, overlay; host_paths(guest, game, overlay);
        if (exists(overlay)) return overlay;
        if (exists(game)) return game;
        return {};
    }
    // ---------------------------------------------------------------- guest strings
    std::string cstr(uint32_t p, size_t max = 4096) {
        std::string s; if (!p) return s;
        for (size_t i = 0; i < max; i++) { char c = char(m.mem.read(p + uint32_t(i), 1)); if (!c) break; s += c; }
        return s;
    }
    std::string wstr(uint32_t p, size_t max = 4096) {   // UTF-16 -> Latin-1
        std::string s; if (!p) return s;
        for (size_t i = 0; i < max; i++) { uint16_t c = uint16_t(m.mem.read(p + 2 * uint32_t(i), 2)); if (!c) break; s += char(c < 256 ? c : '?'); }
        return s;
    }
    void put_cstr(uint32_t p, const std::string& s) { m.mem.write_block(p, s.c_str(), s.size() + 1); }
    void put_wstr(uint32_t p, const std::string& s) { for (size_t i = 0; i <= s.size(); i++) m.mem.write(p + 2 * uint32_t(i), 2, i < s.size() ? uint8_t(s[i]) : 0); }
    uint32_t f2u(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }

    void imp(const std::string& name, Native fn) { m.imports[name] = std::move(fn); }
    void install_kernel32();
    void install_user32();
    void install_devices();
    void load_images();
    void new_thread(uint32_t start, uint32_t param, bool suspended, const std::string& name);
    void schedule(uint64_t frames_to_run);
    std::string describe();
};

// ======================================================================== images
void Boot::Impl::load_images() {
    Machine& mm = m;
    // Executable at its preferred base; every section from the file (writable ones too).
    auto load = [&](const std::string& name, const std::string& path) -> Mod& {
        Pe pe(slurp(path));
        uint32_t base = pe.image_base;
        mm.modules.push_back({base, base + pe.size, pe.image});
        mm.mem.map(base, pe.image.data(), std::min<uint32_t>(pe.size, 0x1000), true);   // headers
        for (auto& s : pe.sections) {
            uint32_t a = base + s.va, vs = (std::max(s.vsize, s.rawsize) + 0xfff) & ~0xfffu;
            mm.mem.map(a, pe.image.data() + s.va, std::min<uint32_t>(vs, pe.size - s.va), !(s.flags & 0x80000000u));
        }
        regions[base] = (pe.size + 0xffff) & ~0xffffu;
        Mod md{name, base, pe.size, base + rd<uint32_t>(pe.image, pe.nt + 24 + 16), pe.exports()};
        mods.push_back(md);
        // imports: resolve against loaded modules, else named sentinels
        for (auto& imp : pe.imports()) {
            uint32_t target = 0;
            for (auto& o : mods) if (o.name == imp.dll) { auto e = o.exports.find(imp.name); if (e != o.exports.end()) target = e->second; }
            if (!target) target = sentinel(imp.dll + "!" + imp.name);
            mm.mem.set_readonly(imp.iat, 4, false); mm.mem.write(imp.iat, 4, target);
        }
        // the TLS directory (executable only)
        uint32_t tls = pe.dir(9);
        if (tls) {
            exe_tls_start = rd<uint32_t>(pe.image, tls); exe_tls_end = rd<uint32_t>(pe.image, tls + 4);
            exe_tls_index_addr = rd<uint32_t>(pe.image, tls + 8); exe_tls_zero = rd<uint32_t>(pe.image, tls + 16);
            uint32_t callbacks = rd<uint32_t>(pe.image, tls + 12);
            if (callbacks && mm.mem.read(callbacks, 4)) say("warning: TLS callbacks present (not run)");
        }
        return mods.back();
    };
    for (auto& [n, a] : load("msvcr90.dll", cfg.crt_dir + "/msvcr90.dll").exports) m.crt_exports[n] = a;   // coverage .dll.tsv (native/game)
    load("msvcp90.dll", cfg.crt_dir + "/msvcp90.dll");
    load("mxreflex.exe", cfg.game_dir + "/MXReflex.exe");
    // the CRT's own imports of msvcr90 from msvcp90 were resolved in load order; re-resolve
    // msvcr90's imports that point at msvcp90 (none) and msvcp90's at msvcr90 (done).
}

// ======================================================================== threads
void Boot::Impl::new_thread(uint32_t start, uint32_t param, bool suspended, const std::string& name) {
    auto t = std::make_unique<Thread>();
    t->id = next_tid; next_tid += 4; t->name = name; t->suspended = suspended; t->start = start;
    uint32_t size = 1u << 20;
    t->stack_lo = valloc(size); t->stack_hi = t->stack_lo + size;
    // TEB: SEH chain end, stack base/limit, self, ids, TLS array, PEB.
    auto put = [&](uint32_t off, uint32_t v) { std::memcpy(t->teb.data() + off, &v, 4); };
    put(0x00, 0xffffffffu); put(0x04, t->stack_hi); put(0x08, t->stack_lo); put(0x18, TEB);
    put(0x20, 0x4d0); put(0x24, t->id); put(0x30, PEB); put(0x34, 0);
    // static TLS block of the executable
    if (exe_tls_index_addr) {
        uint32_t n = exe_tls_end - exe_tls_start, block = halloc(process_heap, n + exe_tls_zero + 16, true);
        std::vector<uint8_t> init(n); m.mem.read_block(exe_tls_start, init.data(), n); m.mem.write_block(block, init.data(), n);
        uint32_t array = halloc(process_heap, 64 * 4, true);
        m.mem.write(array + 4 * exe_tls_index, 4, block);
        put(0x2c, array);
    }
    Ctx& c = t->ctx; c.r[4] = t->stack_hi - 16;
    if (start) {
        c.r[4] -= 4; m.mem.write(c.r[4], 4, param);
        c.r[4] -= 4; m.mem.write(c.r[4], 4, THREAD_EXIT);
        c.pc = start;
    }
    auto o = std::make_shared<Obj>(); o->kind = Obj::Thread_; o->thread = t->id;
    t->handle = add_handle(o);
    say("thread %x created: start %08x param %08x %s%s", t->id, start, param, name.c_str(), suspended ? " (suspended)" : "");
    tl("T %x %08x %08x %llu %u", t->id, start, param, (unsigned long long)m.steps, threads.empty() ? 0u : threads[cur]->id);
    threads.push_back(std::move(t));
    on_undo([this] { threads.pop_back(); next_tid -= 4; });   // (not started yet: its host thread starts at its first switch)
}

void Boot::Impl::schedule(uint64_t frames_to_run) {
    if (cfg.baton) { schedule_baton(frames_to_run); return; }
    stop_at = frames + frames_to_run; stop_request = false;
    for (;;) {
        // pick the next runnable thread (round robin from cur)
        size_t n = threads.size(), pick = SIZE_MAX; uint32_t result = 0;
        for (int pass = 0; pass < 2 && pick == SIZE_MAX; pass++) {
            for (size_t k = 0; k < n; k++) {
                size_t i = (cur + k) % n; Thread& t = *threads[i];
                if (t.state == Thread::Done || t.suspended) continue;
                if (t.state == Thread::Ready) { pick = i; break; }
                if (wait_ready(t, &result)) { pick = i; break; }
            }
            if (pick == SIZE_MAX) {   // everyone waits: advance virtual time to the earliest timeout
                uint64_t wake = WAIT_FOREVER;
                for (auto& t : threads) if (t->state == Thread::Waiting && !t->suspended) wake = std::min(wake, t->wake_at);
                if (wake == WAIT_FOREVER) throw VmError("deadlock: every thread waits forever\n" + describe());
                now_ = std::max(now_, wake);
            }
        }
        cur = pick; Thread& t = *threads[cur];
        load(t); cover(t);
        uint32_t pc = t.ctx.pc;
        if (t.state == Thread::Waiting) {   // complete the blocked import
            t.state = Thread::Ready; t.waits.clear(); t.wait_is_sleep = false; t.wait_cs = 0;
            pc = m.ret(t.wait_cleanup, result);
        }
        yield_request = false; m.sliced = false; static const uint64_t slice = std::getenv("REFLEX_SLICE") ? std::strtoull(std::getenv("REFLEX_SLICE"), nullptr, 10) : SLICE;   // debugging override
        m.slice_end = m.steps + slice;
        const uint64_t run_from = m.steps;
        uint32_t next;
        try { next = m.run(pc, UINT64_MAX / 4); }
        catch (const VmError& e) {
            char b[200]; std::snprintf(b, sizeof b, "\n  in thread %x: eax %08x ecx %08x edx %08x ebx %08x esp %08x ebp %08x esi %08x edi %08x\n  stack:", t.id, m.r[0], m.r[1], m.r[2], m.r[3], m.r[4], m.r[5], m.r[6], m.r[7]);
            std::string msg = e.what(); msg += b;
            for (uint32_t a = m.r[4], k = 0; k < 24; a += 4, k++) { char w[16]; std::snprintf(w, sizeof w, " %08x", m.mem.mapped(a) ? uint32_t(m.mem.read(a, 4)) : 0u); msg += w; }
            throw VmError(msg);
        }
        m.slice_end = UINT64_MAX;
        if (static FILE* sl = std::getenv("REFLEX_SLICE_LOG") ? std::fopen(std::getenv("REFLEX_SLICE_LOG"), "w") : nullptr; sl) {   // debugging: state at every slice end
            uint64_t xh = 1469598103934665603ull; for (auto& x : m.xmm) { xh = (xh ^ x.lo) * 1099511628211ull; xh = (xh ^ x.hi) * 1099511628211ull; }
            std::fprintf(sl, "%llu t%x pc %08x %08x %08x %08x %08x %08x %08x %08x %08x f %03x x %016llx\n", (unsigned long long)m.steps, t.id, next,
                         m.r[0], m.r[1], m.r[2], m.r[3], m.r[4], m.r[5], m.r[6], m.r[7], m.flags & 0x8d5, (unsigned long long)xh);
        }
        advance_time();
        if (tlog) {   // R tid instructions reason pc|return-address ra1 ra2 ra3 [waits]
            char why = m.sliced ? 'S' : !yield_request ? '?' : t.state == Thread::Done ? 'X' : stop_request ? 'F' : t.state == Thread::Ready ? 'Y'
                     : t.wait_is_sleep ? 'L' : t.wait_cs ? 'C' : 'B';
            std::string w; char x[48];
            uint32_t at = m.sliced ? next : (m.mem.mapped(m.r[4]) ? uint32_t(m.mem.read(m.r[4], 4)) : 0u);
            for (uint32_t a = m.r[4] + (m.sliced ? 0 : 4), k = 0, n = 0; k < 256 && n < 4 && m.mem.mapped(a); a += 4, k++) {
                uint32_t v = uint32_t(m.mem.read(a, 4)); if (v > 0x401000 && v < 0x916000) { std::snprintf(x, sizeof x, " %08x", v); w += x; n++; } }
            for (; w.size() < 36;) w += " -";
            if (why == 'B') for (uint32_t h : t.waits) { Obj* o = obj(h); std::snprintf(x, sizeof x, " h%x:%s", h, o && !o->name.empty() ? o->name.c_str() : "-"); w += x; }
            if (why == 'C') { std::snprintf(x, sizeof x, " cs%08x:%x", t.wait_cs, cs_owner[t.wait_cs].first); w += x; }
            if (why == 'L') { std::snprintf(x, sizeof x, " ms%llu", (unsigned long long)((t.wake_at - now_) / 10000)); w += x; }
            tl("R %x %llu %c %08x%s", t.id, (unsigned long long)(m.steps - run_from), why, at, w.c_str());
        }
        if (m.sliced) { save(t, next); cur = (cur + 1) % threads.size(); continue; }
        if (yield_request) {           // blocked (or yielded / exited / frame stop)
            if (t.state == Thread::Done) save(t, 0);
            else if (stop_request) { save(t, ENGINE_FRAME); resume_bypass = true; cur = (cur + 1) % threads.size(); return; }   // the others run before the next frame
            else if (t.state == Thread::Ready) { uint32_t back = m.ret(t.wait_cleanup, int64_t(m.r[0])); save(t, back); }   // plain yield: the call returns
            else save(t, 0);           // waiting: the import's frame stays on the stack
            if (t.state == Thread::Done && t.id == threads[0]->id) throw VmError("the main thread exited");
            cur = (cur + 1) % threads.size();
            continue;
        }
        throw VmError("thread returned to address 0");
    }
}

std::string Boot::Impl::describe() {
    std::string s; char b[256];
    // diagnostics: REFLEX_BOOT_CLOCK=1 also prints the virtual clock (100 ns units; baton scheduling: +100 per QueryPerformanceCounter read)
    if (std::getenv("REFLEX_BOOT_CLOCK")) { std::snprintf(b, sizeof b, "  clock now %llu\n", (unsigned long long)now_); s += b; }
    for (auto& t : threads) {
        std::snprintf(b, sizeof b, "  thread %x %s state %d%s pc %08x esp %08x ret %08x waits", t->id, t->name.c_str(), int(t->state), t->suspended ? " suspended" : "",
                      t->ctx.pc, t->ctx.r[4], t->ctx.r[4] ? uint32_t(m.mem.read(t->ctx.r[4], 4)) : 0u);
        s += b;
        for (uint32_t h : t->waits) { Obj* o = obj(h); std::snprintf(b, sizeof b, " %x(%s%s%s)", h, o ? (o->kind == Obj::Event ? "event" : o->kind == Obj::Mutex ? "mutex" : o->kind == Obj::Thread_ ? "thread" : "other") : "?", o && o->signaled ? ",set" : "", o && !o->name.empty() ? (","+o->name).c_str() : ""); s += b; }
        if (t->wait_cs) { std::snprintf(b, sizeof b, " cs %08x (owner %x)", t->wait_cs, cs_owner[t->wait_cs].first); s += b; }
        // return addresses on the stack
        int k = 0;
        for (uint32_t a = t->ctx.r[4]; a < t->ctx.r[4] + 0x400 && k < 10; a += 4) {
            uint32_t v = m.mem.mapped(a) ? uint32_t(m.mem.read(a, 4)) : 0;
            if (v > 0x401000 && v < 0x916000) { std::snprintf(b, sizeof b, " <%08x", v); s += b; k++; }
        }
        s += "\n";
    }
    return s;
}

// ======================================================================== kernel32
void Boot::Impl::install_kernel32() {
    Impl* s = this;
    const std::string K = "kernel32.dll!";
    // ---- modules / process ----
    auto module_of = [s](const std::string& nm) -> uint32_t {
        std::string n = lower(nm); size_t slash = n.find_last_of("\\/"); if (slash != std::string::npos) n = n.substr(slash + 1);
        if (n.empty()) return 0x400000;
        if (n.find('.') == std::string::npos) n += ".dll";
        for (auto& md : s->mods) if (md.name == n) return md.base;
        if (n == "kernel32.dll") return KERNEL32;
        if (n == "user32.dll") return USER32;
        if (n == "ntdll.dll") return NTDLL;
        return 0;
    };
    imp(K + "GetModuleHandleA", [s, module_of](Machine& m) { uint32_t p = m.arg(0); return m.ret(4, p ? module_of(s->cstr(p)) : 0x400000); });
    imp(K + "GetModuleHandleW", [s, module_of](Machine& m) { uint32_t p = m.arg(0); return m.ret(4, p ? module_of(s->wstr(p)) : 0x400000); });
    imp(K + "LoadLibraryA", [s, module_of](Machine& m) {
        std::string n = s->cstr(m.arg(0)); uint32_t h = module_of(n);
        s->say("LoadLibraryA %s -> %08x", n.c_str(), h);
        if (!h) s->last_error = 126;
        return m.ret(4, h);
    });
    imp(K + "LoadLibraryW", [s, module_of](Machine& m) { std::string n = s->wstr(m.arg(0)); uint32_t h = module_of(n); s->say("LoadLibraryW %s -> %08x", n.c_str(), h); if (!h) s->last_error = 126; return m.ret(4, h); });
    imp(K + "FreeLibrary", [](Machine& m) { return m.ret(4, 1); });
    imp(K + "GetProcAddress", [s](Machine& m) {
        uint32_t h = m.arg(0), p = m.arg(1);
        std::string name = p < 0x10000 ? "#" + std::to_string(p) : s->cstr(p);
        for (auto& md : s->mods) if (md.base == h) { auto e = md.exports.find(name); return m.ret(8, e == md.exports.end() ? 0 : e->second); }
        std::string dll = h == KERNEL32 ? "kernel32.dll" : h == USER32 ? "user32.dll" : h == NTDLL ? "ntdll.dll" : "";
        // Fiber-local storage is reported missing: the CRT then uses TLS.
        static const std::set<std::string> absent = {"FlsAlloc", "FlsGetValue", "FlsSetValue", "FlsFree", "IsProcessorFeaturePresent"};
        if (dll.empty() || absent.count(name)) { s->say("GetProcAddress %08x %s -> 0", h, name.c_str()); return m.ret(8, 0); }
        uint32_t a = s->sentinel(dll + "!" + name);
        s->say("GetProcAddress %s!%s -> %08x", dll.c_str(), name.c_str(), a);
        return m.ret(8, a);
    });
    imp(K + "EncodePointer", [](Machine& m) { return m.ret(4, m.arg(0)); });
    imp(K + "DecodePointer", [](Machine& m) { return m.ret(4, m.arg(0)); });
    imp(K + "GetModuleFileNameA", [s](Machine& m) {
        uint32_t h = m.arg(0), buf = m.arg(1), n = m.arg(2);
        std::string p = s->guest_exe;
        for (auto& md : s->mods) if (md.base == h && md.name != "mxreflex.exe") p = "C:\\Windows\\WinSxS\\x86_microsoft.vc90.crt_1fc8b3b9a1e18e3b_9.0.30729.1_none_e163563597edeada\\" + md.name;
        std::string t = p.substr(0, n ? n - 1 : 0); s->put_cstr(buf, t); return m.ret(12, uint32_t(t.size()));
    });
    imp(K + "GetModuleFileNameW", [s](Machine& m) {
        uint32_t h = m.arg(0), buf = m.arg(1), n = m.arg(2);
        std::string p = s->guest_exe;
        for (auto& md : s->mods) if (md.base == h && md.name != "mxreflex.exe") p = "C:\\Windows\\WinSxS\\x86_microsoft.vc90.crt_1fc8b3b9a1e18e3b_9.0.30729.1_none_e163563597edeada\\" + md.name;
        std::string t = p.substr(0, n ? n - 1 : 0); s->put_wstr(buf, t); return m.ret(12, uint32_t(t.size()));
    });
    imp(K + "GetCommandLineA", [s](Machine& m) {
        uint32_t& a = s->cmdline_a;
        if (!a) { std::string c = "\"" + s->guest_exe + "\""; a = s->halloc(s->process_heap, uint32_t(c.size() + 1), true); s->put_cstr(a, c); s->on_undo([s] { s->cmdline_a = 0; }); }
        return m.ret(0, a);
    });
    imp(K + "GetCommandLineW", [s](Machine& m) {
        uint32_t& a = s->cmdline_w;
        if (!a) { std::string c = "\"" + s->guest_exe + "\""; a = s->halloc(s->process_heap, uint32_t(2 * c.size() + 2), true); s->put_wstr(a, c); s->on_undo([s] { s->cmdline_w = 0; }); }
        return m.ret(0, a);
    });
    static const std::vector<std::string> env = {"PATH=C:\\Windows\\system32;C:\\Windows", "SystemRoot=C:\\Windows", "TEMP=C:\\Temp", "TMP=C:\\Temp", "USERPROFILE=C:\\Users\\Player", "APPDATA=C:\\Users\\Player\\AppData\\Roaming", "LOCALAPPDATA=C:\\Users\\Player\\AppData\\Local", "NUMBER_OF_PROCESSORS=2", "OS=Windows_NT"};
    imp(K + "GetEnvironmentStrings", [s](Machine& m) {
        std::string b; for (auto& e : env) { b += e; b.push_back('\0'); } b.push_back('\0');
        uint32_t a = s->halloc(s->process_heap, uint32_t(b.size()), true); m.mem.write_block(a, b.data(), b.size()); return m.ret(0, a);
    });
    imp(K + "GetEnvironmentStringsW", [s](Machine& m) {
        std::string b; for (auto& e : env) { b += e; b.push_back('\0'); } b.push_back('\0');
        uint32_t a = s->halloc(s->process_heap, uint32_t(2 * b.size()), true);
        for (size_t i = 0; i < b.size(); i++) m.mem.write(a + 2 * uint32_t(i), 2, uint8_t(b[i]));
        return m.ret(0, a);
    });
    imp(K + "FreeEnvironmentStringsA", [s](Machine& m) { s->hfree(m.arg(0)); return m.ret(4, 1); });
    imp(K + "FreeEnvironmentStringsW", [s](Machine& m) { s->hfree(m.arg(0)); return m.ret(4, 1); });
    imp(K + "GetEnvironmentVariableA", [s](Machine& m) {
        std::string n = lower(s->cstr(m.arg(0))); uint32_t buf = m.arg(1), size = m.arg(2);
        for (auto& e : env) { size_t eq = e.find('='); if (lower(e.substr(0, eq)) == n) { std::string v = e.substr(eq + 1); if (v.size() + 1 > size) return m.ret(12, uint32_t(v.size() + 1)); s->put_cstr(buf, v); return m.ret(12, uint32_t(v.size())); } }
        s->last_error = 203; return m.ret(12, 0);
    });
    imp(K + "SetEnvironmentVariableA", [](Machine& m) { return m.ret(8, 1); });
    imp(K + "GetStartupInfoA", [](Machine& m) { uint32_t p = m.arg(0); m.mem.write_block(p, nullptr, 68); m.mem.write(p, 4, 68); return m.ret(4); });
    imp(K + "GetStartupInfoW", [](Machine& m) { uint32_t p = m.arg(0); m.mem.write_block(p, nullptr, 68); m.mem.write(p, 4, 68); return m.ret(4); });
    imp(K + "GetVersion", [](Machine& m) { return m.ret(0, 0x1db10106u); });   // Windows 7 SP1 (6.1.7601)
    imp(K + "GetVersionExA", [](Machine& m) {
        uint32_t p = m.arg(0); uint32_t size = uint32_t(m.mem.read(p, 4));
        uint32_t v[5] = {size, 6, 1, 7601, 2}; m.mem.write_block(p, v, 20);
        m.mem.write_block(p + 20, nullptr, 128); { const char* sp = "Service Pack 1"; m.mem.write_block(p + 20, sp, std::strlen(sp) + 1); }
        return m.ret(4, 1);
    });
    imp(K + "GetCurrentProcessId", [](Machine& m) { return m.ret(0, 0x5a4); });
    imp(K + "GetCurrentProcess", [](Machine& m) { return m.ret(0, PSEUDO_PROCESS); });
    imp(K + "GetCurrentThread", [](Machine& m) { return m.ret(0, PSEUDO_THREAD); });
    imp(K + "GetCurrentThreadId", [s](Machine& m) { return m.ret(0, s->T().id); });
    imp(K + "IsDebuggerPresent", [](Machine& m) { return m.ret(0, 0); });
    imp(K + "DebugBreak", [](Machine&) -> uint32_t { throw VmError("DebugBreak"); });
    imp(K + "OutputDebugStringA", [s](Machine& m) { s->say("[debug] %s", s->cstr(m.arg(0)).c_str()); return m.ret(4); });
    imp(K + "SetUnhandledExceptionFilter", [](Machine& m) { return m.ret(4, 0); });
    imp(K + "UnhandledExceptionFilter", [](Machine&) -> uint32_t { throw VmError("UnhandledExceptionFilter (an unhandled exception)"); });
    imp(K + "RaiseException", [s](Machine& m) -> uint32_t {
        uint32_t code = m.arg(0), nargs = m.arg(2), args = m.arg(3);
        char b[160]; std::snprintf(b, sizeof b, "RaiseException %08x (%u args, first %08x) return %08x", code, nargs, nargs && args ? uint32_t(m.mem.read(args, 4)) : 0u, uint32_t(m.mem.read(m.r[4], 4)));
        s->say("%s", b); throw VmError(b);
    });
    imp(K + "TerminateProcess", [s](Machine& m) -> uint32_t { char b[64]; std::snprintf(b, sizeof b, "TerminateProcess code %u", m.arg(1)); s->say("%s", b); throw VmError(b); });
    imp(K + "ExitProcess", [s](Machine& m) -> uint32_t { char b[64]; std::snprintf(b, sizeof b, "ExitProcess code %u", m.arg(0)); s->say("%s", b); throw VmError(b); });
    imp(K + "SetErrorMode", [](Machine& m) { return m.ret(4, 0); });
    imp(K + "GetLastError", [s](Machine& m) { return m.ret(0, s->last_error); });
    imp(K + "SetLastError", [s](Machine& m) { s->last_error = m.arg(0); return m.ret(4); });
    imp(K + "SetConsoleCtrlHandler", [](Machine& m) { return m.ret(8, 1); });
    imp(K + "GetSystemInfo", [s](Machine& m) {
        uint32_t p = m.arg(0); uint32_t v[9] = {0, 4096, 0x10000, 0x7ffeffff, (1u << s->cfg.cpus) - 1, s->cfg.cpus, 586, 0x10000, 0x0f06 << 16 | 6};
        v[8] = (6u) | (0x0f06u << 16); m.mem.write_block(p, v, 36); return m.ret(4);
    });
    imp(K + "IsProcessorFeaturePresent", [](Machine& m) { uint32_t f = m.arg(0); return m.ret(4, (f == 6 || f == 10 || f == 3 || f == 2) ? 1 : 0); });
    // ---- time ----
    imp(K + "QueryPerformanceFrequency", [](Machine& m) { m.mem.write(m.arg(0), 8, 10000000ull); return m.ret(4, 1); });
    imp(K + "QueryPerformanceCounter", [s](Machine& m) { s->advance_time(); s->now_ += 100; m.mem.write(m.arg(0), 8, s->now_); return m.ret(4, 1); });
    // REFLEX_MS_FROM_FRAME=1 (w-gridpair, test hook): the millisecond clocks read the frame's own time, the counter value
    // Engine::UpdateTiming last took (engine 0xd728d0 + 0x68), as the product's host clock does (one value per frame), instead of the
    // baton clock, which every QueryPerformanceCounter call moves on by 10 us within the frame (near a millisecond boundary the game's
    // network clock 00892170, the AI drivers' clock, then reads one ms more than the product). Default unchanged.
    static const bool ms_from_frame = std::getenv("REFLEX_MS_FROM_FRAME") != nullptr;
    auto ms_now = [s](Machine& m) -> uint32_t {
        s->advance_time();
        if (ms_from_frame && m.mem.mapped(0xd728d0 + 0x68)) {
            const uint64_t t = uint64_t(uint32_t(m.mem.read(0xd728d0 + 0x68, 4))) | (uint64_t(uint32_t(m.mem.read(0xd728d0 + 0x6c, 4))) << 32);
            if (t) return uint32_t(t / 10000);
        }
        return uint32_t(s->now_ / 10000);
    };
    imp(K + "GetTickCount", [ms_now](Machine& m) { return m.ret(0, ms_now(m)); });
    imp("winmm.dll!timeGetTime", [ms_now](Machine& m) { return m.ret(0, ms_now(m)); });
    imp("winmm.dll!timeBeginPeriod", [](Machine& m) { return m.ret(4, 0); });
    // 2009-10-20 12:00:00 UTC + virtual time (FILETIME, 100 ns since 1601)
    auto filetime = [s]() { return 128952432000000000ull + s->now_; };
    imp(K + "GetSystemTimeAsFileTime", [filetime](Machine& m) { m.mem.write(m.arg(0), 8, filetime()); return m.ret(4); });
    auto systemtime = [](uint64_t ft, uint16_t out[8]) {
        uint64_t sec = ft / 10000000ull - 11644473600ull; uint32_t ms = uint32_t(ft / 10000 % 1000);
        int64_t days = int64_t(sec / 86400); uint32_t rem = uint32_t(sec % 86400);
        // civil from days (Howard Hinnant)
        int64_t z = days + 719468, era = (z >= 0 ? z : z - 146096) / 146097; uint32_t doe = uint32_t(z - era * 146097);
        uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; int64_t y = int64_t(yoe) + era * 400;
        uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153, d = doy - (153 * mp + 2) / 5 + 1, mo = mp < 10 ? mp + 3 : mp - 9;
        out[0] = uint16_t(y + (mo <= 2)); out[1] = uint16_t(mo); out[2] = uint16_t((days + 4) % 7); out[3] = uint16_t(d);
        out[4] = uint16_t(rem / 3600); out[5] = uint16_t(rem / 60 % 60); out[6] = uint16_t(rem % 60); out[7] = uint16_t(ms);
    };
    imp(K + "GetLocalTime", [filetime, systemtime](Machine& m) { uint16_t t[8]; systemtime(filetime(), t); m.mem.write_block(m.arg(0), t, 16); return m.ret(4); });
    imp(K + "GetSystemTime", [filetime, systemtime](Machine& m) { uint16_t t[8]; systemtime(filetime(), t); m.mem.write_block(m.arg(0), t, 16); return m.ret(4); });
    imp(K + "FileTimeToSystemTime", [systemtime](Machine& m) { uint16_t t[8]; systemtime(m.mem.read(m.arg(0), 8), t); m.mem.write_block(m.arg(1), t, 16); return m.ret(8, 1); });
    imp(K + "FileTimeToLocalFileTime", [](Machine& m) { m.mem.write(m.arg(1), 8, m.mem.read(m.arg(0), 8)); return m.ret(8, 1); });
    imp(K + "GetTimeZoneInformation", [](Machine& m) { m.mem.write_block(m.arg(0), nullptr, 172); return m.ret(4, 0); });
    // ---- threads / sync ----
    imp(K + "CreateThread", [s](Machine& m) {
        uint32_t start = m.arg(2), param = m.arg(3), flags = m.arg(4), tid_out = m.arg(5);
        s->new_thread(start, param, flags & 4, "");
        Thread& t = *s->threads.back();
        if (tid_out) m.mem.write(tid_out, 4, t.id);
        return m.ret(24, t.handle);
    });
    imp(K + "ResumeThread", [s](Machine& m) {
        Obj* o = s->obj(m.arg(0)); if (!o || o->kind != Obj::Thread_) return m.ret(4, 0xffffffffu);
        Thread* t = s->thread_by_id(o->thread); uint32_t prev = t && t->suspended ? 1 : 0;
        if (t) { bool was = t->suspended; s->on_undo([t, was] { t->suspended = was; }); t->suspended = false; }
        return m.ret(4, prev);
    });
    imp(K + "SuspendThread", [](Machine&) -> uint32_t { throw VmError("SuspendThread"); });
    imp(K + "SetThreadPriority", [](Machine& m) { return m.ret(8, 1); });
    imp(K + "GetThreadPriority", [](Machine& m) { return m.ret(4, 0); });
    imp(K + "SetThreadAffinityMask", [](Machine& m) { return m.ret(8, 1); });
    imp(K + "SetThreadIdealProcessor", [](Machine& m) { return m.ret(8, 0); });
    auto thread_done = [s](Thread& t) -> uint32_t {   // baton: the host thread parks for good (FiberQuit when the boot ends)
        if (!s->cfg.baton || !tl_guest) { s->would_switch(); s->yield_request = true; return 0u; }
        if (&t == s->threads[0].get()) throw VmError("the main thread exited");
        s->park(t, 0); throw VmError("a finished thread resumed");
    };
    imp(K + "ExitThread", [s, thread_done](Machine& m) { Thread& t = s->T(); t.exit_code = m.arg(0); t.state = Thread::Done; s->say("thread %x exits %u", t.id, t.exit_code); return thread_done(t); });
    m.natives[THREAD_EXIT] = [s, thread_done](Machine& m) { Thread& t = s->T(); t.exit_code = m.r[0]; t.state = Thread::Done; s->say("thread %x returns %u", t.id, t.exit_code); return thread_done(t); };
    imp(K + "GetExitCodeThread", [s](Machine& m) {
        Obj* o = s->obj(m.arg(0)); Thread* t = o ? s->thread_by_id(o->thread) : nullptr;
        m.mem.write(m.arg(1), 4, t ? (t->state == Thread::Done ? t->exit_code : 0x103) : 0); return m.ret(8, 1);
    });
    imp(K + "Sleep", [s](Machine& m) {
        uint32_t ms = m.arg(0); Thread& t = s->T();
        t.wait_is_sleep = true; t.wake_at = s->now_ + uint64_t(ms) * 10000; t.waits.clear();
        if (ms == 0 && s->cfg.baton && tl_guest) { t.wait_is_sleep = false; s->yield(t); return m.ret(4, 0); }
        if (ms == 0) { s->would_switch(); t.wait_cleanup = 4; t.wait_is_sleep = false; t.state = Thread::Ready; s->yield_request = true; m.r[0] = 0; return 0u; }   // yield
        return s->block(4);
    });
    imp(K + "SleepEx", [s](Machine& m) {
        uint32_t ms = m.arg(0); Thread& t = s->T();
        t.wait_is_sleep = true; t.wake_at = s->now_ + uint64_t(ms) * 10000; t.waits.clear();
        if (ms == 0 && s->cfg.baton && tl_guest) { t.wait_is_sleep = false; s->yield(t); return m.ret(8, 0); }
        if (ms == 0) { s->would_switch(); t.wait_cleanup = 8; t.wait_is_sleep = false; t.state = Thread::Ready; s->yield_request = true; m.r[0] = 0; return 0u; }
        return s->block(8);
    });
    imp(K + "SwitchToThread", [s](Machine& m) {
        Thread& t = s->T();
        if (s->cfg.baton && tl_guest) { s->yield(t); return m.ret(0, 1); }
        s->would_switch(); t.wait_cleanup = 0; t.state = Thread::Ready; s->yield_request = true; m.r[0] = 1; return 0u;
    });
    imp(K + "WaitForSingleObject", [s](Machine& m) {
        Thread& t = s->T(); uint32_t ms = m.arg(1);
        t.waits = {m.arg(0)}; t.wait_all = false; t.wait_is_sleep = false; t.wake_at = ms == 0xffffffffu ? WAIT_FOREVER : s->now_ + uint64_t(ms) * 10000;
        return s->block(8);
    });
    imp(K + "WaitForSingleObjectEx", [s](Machine& m) {
        Thread& t = s->T(); uint32_t ms = m.arg(1);
        t.waits = {m.arg(0)}; t.wait_all = false; t.wait_is_sleep = false; t.wake_at = ms == 0xffffffffu ? WAIT_FOREVER : s->now_ + uint64_t(ms) * 10000;
        return s->block(12);
    });
    imp(K + "WaitForMultipleObjects", [s](Machine& m) {
        Thread& t = s->T(); uint32_t n = m.arg(0), arr = m.arg(1), ms = m.arg(3);
        t.waits.clear(); for (uint32_t i = 0; i < n; i++) t.waits.push_back(uint32_t(m.mem.read(arr + 4 * i, 4)));
        t.wait_all = m.arg(2) != 0; t.wait_is_sleep = false; t.wake_at = ms == 0xffffffffu ? WAIT_FOREVER : s->now_ + uint64_t(ms) * 10000;
        return s->block(16);
    });
    imp(K + "CreateEventA", [s](Machine& m) {
        auto o = std::make_shared<Obj>(); o->kind = Obj::Event; o->manual = m.arg(1); o->signaled = m.arg(2); o->name = s->cstr(m.arg(3));
        if (!o->name.empty()) for (auto& [h, e] : s->handles) if (e->kind == Obj::Event && e->name == o->name) { s->last_error = 183; return m.ret(16, h); }
        s->last_error = 0; return m.ret(16, s->add_handle(o));
    });
    imp(K + "CreateEventW", [s](Machine& m) {
        auto o = std::make_shared<Obj>(); o->kind = Obj::Event; o->manual = m.arg(1); o->signaled = m.arg(2); o->name = s->wstr(m.arg(3));
        s->last_error = 0; return m.ret(16, s->add_handle(o));
    });
    imp(K + "SetEvent", [s](Machine& m) {
        Obj* o = s->obj(m.arg(0)); if (o && o->kind == Obj::Event) { s->obj_undo(m.arg(0)); o->signaled = true; }
        if (s->tlog) s->tl("E %x h%x:%s %08x", s->T().id, m.arg(0), o && !o->name.empty() ? o->name.c_str() : "-", uint32_t(m.mem.read(m.r[4], 4)));
        return m.ret(4, o ? 1 : 0);
    });
    imp(K + "ResetEvent", [s](Machine& m) { Obj* o = s->obj(m.arg(0)); if (o && o->kind == Obj::Event) { s->obj_undo(m.arg(0)); o->signaled = false; } return m.ret(4, o ? 1 : 0); });
    imp(K + "PulseEvent", [s](Machine& m) { Obj* o = s->obj(m.arg(0)); if (o) { s->obj_undo(m.arg(0)); o->signaled = false; } return m.ret(4, 1); });
    imp(K + "CreateMutexA", [s](Machine& m) {
        std::string name = s->cstr(m.arg(2));
        if (!name.empty()) for (auto& [h, e] : s->handles) if (e->kind == Obj::Mutex && e->name == name) { s->last_error = 183; return m.ret(12, h); }
        auto o = std::make_shared<Obj>(); o->kind = Obj::Mutex; o->name = name;
        if (m.arg(1)) { o->owner = s->T().id; o->count = 1; }
        s->last_error = 0; return m.ret(12, s->add_handle(o));
    });
    imp(K + "ReleaseMutex", [s](Machine& m) { Obj* o = s->obj(m.arg(0)); if (o && o->kind == Obj::Mutex && o->count) { s->obj_undo(m.arg(0)); o->count--; } return m.ret(4, 1); });
    imp(K + "CloseHandle", [s](Machine& m) {
        uint32_t h = m.arg(0); auto it = s->handles.find(h);
        if (it == s->handles.end()) return m.ret(4, 1);
        if (s->logging()) {   // a verified call: the handle comes back on undo; a file's fd closes once that can no longer happen (commit_closes)
            std::shared_ptr<Obj> o = it->second;
            if (o->kind == Obj::File && o->fd >= 0) { o->close_pending = true; s->pending_close.push_back(o); s->on_undo([o] { o->close_pending = false; }); }
            s->on_undo([s, h, o] { s->handles[h] = o; });
            s->handles.erase(it); return m.ret(4, 1);
        }
        if (it->second->kind == Obj::File && it->second->fd >= 0) close(it->second->fd);
        s->handles.erase(it); return m.ret(4, 1);
    });
    imp(K + "DuplicateHandle", [s](Machine& m) {
        uint32_t src = m.arg(1), out = m.arg(3);
        if (src == PSEUDO_THREAD) { m.mem.write(out, 4, s->T().handle); return m.ret(28, 1); }
        auto it = s->handles.find(src); if (it == s->handles.end()) return m.ret(28, 0);
        m.mem.write(out, 4, s->add_handle(it->second)); return m.ret(28, 1);
    });
    // Critical sections: real ownership (threads can be preempted while holding one).
    auto init_cs = [](Machine& m, uint32_t cs, uint32_t spin) { uint32_t v[6] = {0, 0xffffffffu, 0, 0, 0, spin}; m.mem.write_block(cs, v, sizeof v); };
    imp(K + "InitializeCriticalSection", [s, init_cs](Machine& m) { init_cs(m, m.arg(0), 0); s->cs_undo(m.arg(0)); s->cs_owner[m.arg(0)] = {0, 0}; return m.ret(4); });
    imp(K + "InitializeCriticalSectionAndSpinCount", [s, init_cs](Machine& m) { init_cs(m, m.arg(0), m.arg(1)); s->cs_undo(m.arg(0)); s->cs_owner[m.arg(0)] = {0, 0}; return m.ret(8, 1); });
    imp(K + "DeleteCriticalSection", [s](Machine& m) { s->cs_undo(m.arg(0)); s->cs_owner.erase(m.arg(0)); return m.ret(4); });
    imp(K + "EnterCriticalSection", [s](Machine& m) {
        uint32_t cs = m.arg(0); s->cs_undo(cs); auto& o = s->cs_owner[cs]; Thread& t = s->T();
        if (o.second == 0 || o.first == t.id) { o = {t.id, o.first == t.id ? o.second + 1 : 1}; if (o.second == 1) o.first = t.id; return m.ret(4); }
        t.wait_cs = cs; t.waits.clear(); t.wait_is_sleep = false; t.wake_at = WAIT_FOREVER;
        return s->block(4);
    });
    imp(K + "TryEnterCriticalSection", [s](Machine& m) {
        uint32_t cs = m.arg(0); s->cs_undo(cs); auto& o = s->cs_owner[cs]; Thread& t = s->T();
        if (o.second == 0 || o.first == t.id) { o = {t.id, o.first == t.id ? o.second + 1 : 1}; return m.ret(4, 1); }
        if (s->tlog) s->polls[{t.id | 0x40000000u, uint32_t(m.mem.read(m.r[4], 4))}]++;   // a failed TryEnterCriticalSection (REFLEX_BOOT_THREADS)
        return m.ret(4, 0);
    });
    imp(K + "LeaveCriticalSection", [s](Machine& m) {
        uint32_t cs = m.arg(0); s->cs_undo(cs); auto& o = s->cs_owner[cs];
        if (o.second == 0) throw VmError("LeaveCriticalSection without Enter");
        if (--o.second == 0) o.first = 0;
        return m.ret(4);
    });
    // Interlocked: atomic by construction (one host thread, no preemption inside a native).
    imp(K + "InterlockedCompareExchange", [s](Machine& m) { uint32_t p = m.arg(0), x = m.arg(1), c = m.arg(2), old = uint32_t(m.mem.read(p, 4)); if (old == c) m.mem.write(p, 4, x);
        else if (s->tlog) s->polls[{s->T().id | 0x80000000u, uint32_t(m.mem.read(m.r[4], 4))}]++;   // a failed compare-exchange (REFLEX_BOOT_THREADS)
        return m.ret(12, old); });
    imp(K + "InterlockedExchange", [](Machine& m) { uint32_t p = m.arg(0), old = uint32_t(m.mem.read(p, 4)); m.mem.write(p, 4, m.arg(1)); return m.ret(8, old); });
    imp(K + "InterlockedExchangeAdd", [](Machine& m) { uint32_t p = m.arg(0), old = uint32_t(m.mem.read(p, 4)); m.mem.write(p, 4, old + m.arg(1)); return m.ret(8, old); });
    imp(K + "InterlockedIncrement", [](Machine& m) { uint32_t p = m.arg(0), v = uint32_t(m.mem.read(p, 4)) + 1; m.mem.write(p, 4, v); return m.ret(4, v); });
    imp(K + "InterlockedDecrement", [](Machine& m) { uint32_t p = m.arg(0), v = uint32_t(m.mem.read(p, 4)) - 1; m.mem.write(p, 4, v); return m.ret(4, v); });
    // TLS (TEB+0xe10 slots)
    imp(K + "TlsAlloc", [s](Machine& m) { for (uint32_t i = 0; i < 64; i++) if (!s->tls_used[i]) { s->tls_used[i] = 1; s->on_undo([s, i] { s->tls_used[i] = 0; }); return m.ret(0, i); } return m.ret(0, 0xffffffffu); });
    imp(K + "TlsFree", [s](Machine& m) { uint32_t i = m.arg(0); if (i < 64) { uint8_t was = s->tls_used[i]; s->on_undo([s, i, was] { s->tls_used[i] = was; }); s->tls_used[i] = 0; } return m.ret(4, 1); });
    imp(K + "TlsGetValue", [s](Machine& m) { uint32_t i = m.arg(0); s->last_error = 0; return m.ret(4, i < 64 ? uint32_t(m.mem.read(TEB + 0xe10 + 4 * i, 4)) : 0); });
    imp(K + "TlsSetValue", [](Machine& m) { uint32_t i = m.arg(0); if (i < 64) m.mem.write(TEB + 0xe10 + 4 * i, 4, m.arg(1)); return m.ret(8, i < 64); });
    // ---- memory ----
    imp(K + "GetProcessHeap", [s](Machine& m) { return m.ret(0, s->process_heap); });
    imp(K + "HeapCreate", [s](Machine& m) { uint32_t h = s->next_heap; s->next_heap += 0x10000; s->heap_flags[h] = m.arg(0); s->on_undo([s, h] { s->heap_flags.erase(h); s->next_heap -= 0x10000; }); return m.ret(12, h); });
    imp(K + "HeapDestroy", [s](Machine& m) {
        uint32_t h = m.arg(0); std::vector<uint32_t> mine;
        for (auto& [a, b] : s->blocks) if (b.heap == h) mine.push_back(a);
        std::sort(mine.begin(), mine.end());   // (address order: the free lists must not depend on the hash map's iteration order)
        for (uint32_t a : mine) s->hfree(a);
        return m.ret(4, 1);
    });
    imp(K + "HeapAlloc", [s](Machine& m) { return m.ret(12, s->halloc(m.arg(0), m.arg(2), m.arg(1) & 8)); });
    imp(K + "HeapFree", [s](Machine& m) { uint32_t a = m.arg(2); if (a && !s->hfree(a)) s->say("HeapFree of unknown block %08x", a); return m.ret(12, 1); });
    imp(K + "HeapSize", [s](Machine& m) { auto it = s->blocks.find(m.arg(2)); return m.ret(12, it == s->blocks.end() ? 0xffffffffu : it->second.size); });
    imp(K + "HeapValidate", [](Machine& m) { return m.ret(12, 1); });
    imp(K + "HeapCompact", [](Machine& m) { return m.ret(8, 0x100000); });
    imp(K + "HeapSetInformation", [](Machine& m) { return m.ret(16, 1); });
    imp(K + "HeapQueryInformation", [](Machine& m) { if (m.arg(2)) m.mem.write(m.arg(2), 4, 2); return m.ret(20, 1); });
    imp(K + "HeapReAlloc", [s](Machine& m) {
        uint32_t h = m.arg(0), flags = m.arg(1), a = m.arg(2), size = m.arg(3);
        auto it = s->blocks.find(a); if (it == s->blocks.end()) return m.ret(16, 0);
        uint32_t old = it->second.size, room = s->alloc_sizes[a];
        if (size <= room) {   // in place
            if ((flags & 8) && size > old) m.mem.write_block(a + old, nullptr, size - old);
            s->on_undo([s, a, old] { s->blocks[a].size = old; });
            it->second.size = size; return m.ret(16, a);
        }
        if (flags & 0x10) return m.ret(16, 0);   // HEAP_REALLOC_IN_PLACE_ONLY
        uint32_t b = s->halloc(h, size, flags & 8);
        std::vector<uint8_t> tmp(old); m.mem.read_block(a, tmp.data(), old); m.mem.write_block(b, tmp.data(), old);
        s->hfree(a); return m.ret(16, b);
    });
    imp(K + "HeapWalk", [](Machine& m) { return m.ret(8, 0); });
    imp(K + "GlobalAlloc", [s](Machine& m) { return m.ret(8, s->halloc(s->process_heap, m.arg(1), m.arg(0) & 0x40)); });
    imp(K + "GlobalLock", [](Machine& m) { return m.ret(4, m.arg(0)); });
    imp(K + "GlobalUnlock", [](Machine& m) { return m.ret(4, 1); });
    imp(K + "GlobalFree", [s](Machine& m) { s->hfree(m.arg(0)); return m.ret(4, 0); });
    imp(K + "LocalFree", [s](Machine& m) { s->hfree(m.arg(0)); return m.ret(4, 0); });
    imp(K + "LocalAlloc", [s](Machine& m) { return m.ret(8, s->halloc(s->process_heap, m.arg(1), m.arg(0) & 0x40)); });
    imp(K + "VirtualAlloc", [s](Machine& m) {
        uint32_t want = m.arg(0), size = m.arg(1), type = m.arg(2);
        if (want) {   // commit inside an existing reservation, or reserve at an address
            for (auto& [b, sz] : s->regions) if (want >= b && want < b + sz) return m.ret(16, want & ~0xfffu);
            uint32_t a = s->valloc(size, want); return m.ret(16, a);
        }
        (void)type; uint32_t a = s->valloc(size); return m.ret(16, a);
    });
    imp(K + "VirtualFree", [s](Machine& m) {
        uint32_t a = m.arg(0), size = m.arg(1), type = m.arg(2);
        if (type & 0x8000) s->vfree(a);
        else if (type & 0x4000) { s->would_switch(); m.mem.unmap(a, size); for (uint32_t p = a & ~0xfffu; p < a + size; p += 4096) m.mem.back(p); }
        return m.ret(12, 1);
    });
    imp(K + "VirtualProtect", [](Machine& m) { if (m.arg(3)) m.mem.write(m.arg(3), 4, 4); return m.ret(16, 1); });
    imp(K + "VirtualQuery", [s](Machine& m) {
        uint32_t a = m.arg(0), out = m.arg(1);
        uint32_t base = a & ~0xfffu, alloc = base, size = 0x1000, state = 0x10000 /*MEM_FREE*/, prot = 1;
        for (auto& [b, sz] : s->regions) if (a >= b && a < b + sz) { alloc = b; size = b + sz - base; state = 0x1000; prot = 4; }
        if (a >= MAIN_STACK_LO && a < MAIN_STACK_HI) { alloc = MAIN_STACK_LO; size = MAIN_STACK_HI - base; state = 0x1000; prot = 4; }
        uint32_t v[7] = {base, alloc, prot, size, state, prot, state == 0x10000 ? 0u : 0x20000u};
        m.mem.write_block(out, v, 28); return m.ret(12, 28);
    });
    imp(K + "SetHandleCount", [](Machine& m) { return m.ret(4, m.arg(0)); });
    // ---- strings / locale ----
    imp(K + "GetACP", [](Machine& m) { return m.ret(0, 1252); });
    imp(K + "GetOEMCP", [](Machine& m) { return m.ret(0, 437); });
    imp(K + "IsValidCodePage", [](Machine& m) { uint32_t c = m.arg(0); return m.ret(4, c == 1252 || c == 437 || c == 65001); });
    imp(K + "GetCPInfo", [](Machine& m) {
        uint32_t out = m.arg(1); std::vector<uint8_t> v(20, 0); v[0] = 1; v[4] = '?';   // MaxCharSize 1, default '?', no lead bytes
        m.mem.write_block(out, v.data(), v.size()); return m.ret(8, 1);
    });
    imp(K + "GetUserDefaultLCID", [](Machine& m) { return m.ret(0, 0x409); });
    imp(K + "GetUserDefaultLangID", [](Machine& m) { return m.ret(0, 0x409); });
    imp(K + "GetSystemDefaultLangID", [](Machine& m) { return m.ret(0, 0x409); });
    imp(K + "IsValidLocale", [](Machine& m) { return m.ret(8, 1); });
    imp(K + "EnumSystemLocalesA", [](Machine& m) { return m.ret(8, 1); });
    auto locale_info = [](uint32_t type) -> std::string {
        switch (type & 0xffff) {
            case 0x1: return "0409"; case 0x2: return "English (United States)"; case 0x3: return "ENU"; case 0x7: return "USA";
            case 0xb: return "437"; case 0x1004: return "1252"; case 0xe: return "."; case 0xf: return ","; case 0x10: return "3;0";
            case 0x14: return "$"; case 0x15: return "."; case 0x16: return ","; case 0x17: return "3;0"; case 0x18: return "2"; case 0x19: return "2";
            case 0x1a: return "1"; case 0x1b: return "0"; case 0x1c: return "0"; case 0x1d: return "/"; case 0x1e: return ":"; case 0x1f: return "M/d/yyyy";
            case 0x20: return "dddd, MMMM dd, yyyy"; case 0x1003: return "h:mm:ss tt"; case 0x28: return "AM"; case 0x29: return "PM";
            case 0x59: return "en"; case 0x5a: return "US"; case 0x1001: return "English"; case 0x1002: return "United States";
            default:
                if ((type & 0xffff) >= 0x2a && (type & 0xffff) <= 0x30) { static const char* d[] = {"Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"}; return d[(type & 0xffff) - 0x2a]; }
                if ((type & 0xffff) >= 0x31 && (type & 0xffff) <= 0x37) { static const char* d[] = {"Mon","Tue","Wed","Thu","Fri","Sat","Sun"}; return d[(type & 0xffff) - 0x31]; }
                if ((type & 0xffff) >= 0x38 && (type & 0xffff) <= 0x43) { static const char* d[] = {"January","February","March","April","May","June","July","August","September","October","November","December"}; return d[(type & 0xffff) - 0x38]; }
                if ((type & 0xffff) >= 0x44 && (type & 0xffff) <= 0x4f) { static const char* d[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"}; return d[(type & 0xffff) - 0x44]; }
                return "";
        }
    };
    imp(K + "GetLocaleInfoA", [s, locale_info](Machine& m) {
        std::string v = locale_info(m.arg(1)); uint32_t buf = m.arg(2), n = m.arg(3);
        if (!n) return m.ret(16, uint32_t(v.size() + 1));
        if (v.size() + 1 > n) { s->last_error = 122; return m.ret(16, 0); }
        s->put_cstr(buf, v); return m.ret(16, uint32_t(v.size() + 1));
    });
    imp(K + "GetLocaleInfoW", [s, locale_info](Machine& m) {
        std::string v = locale_info(m.arg(1)); uint32_t buf = m.arg(2), n = m.arg(3);
        if (!n) return m.ret(16, uint32_t(v.size() + 1));
        if (v.size() + 1 > n) { s->last_error = 122; return m.ret(16, 0); }
        s->put_wstr(buf, v); return m.ret(16, uint32_t(v.size() + 1));
    });
    static const uint16_t cp1252[32] = {0x20ac,0x81,0x201a,0x192,0x201e,0x2026,0x2020,0x2021,0x2c6,0x2030,0x160,0x2039,0x152,0x8d,0x17d,0x8f,
                                        0x90,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,0x2dc,0x2122,0x161,0x203a,0x153,0x9d,0x17e,0x178};
    imp(K + "MultiByteToWideChar", [](Machine& m) {
        uint32_t src = m.arg(2), dst = m.arg(4); int32_t n = int32_t(m.arg(3)), cap = int32_t(m.arg(5));
        std::vector<uint16_t> out;
        for (int32_t i = 0; n < 0 || i < n; i++) { uint8_t c = uint8_t(m.mem.read(src + uint32_t(i), 1)); out.push_back(c >= 0x80 && c < 0xa0 ? cp1252[c - 0x80] : c); if (n < 0 && !c) break; }
        if (cap == 0) return m.ret(24, uint32_t(out.size()));
        if (int32_t(out.size()) > cap) return m.ret(24, 0);
        for (size_t i = 0; i < out.size(); i++) m.mem.write(dst + 2 * uint32_t(i), 2, out[i]);
        return m.ret(24, uint32_t(out.size()));
    });
    imp(K + "WideCharToMultiByte", [](Machine& m) {
        uint32_t src = m.arg(2), dst = m.arg(4); int32_t n = int32_t(m.arg(3)), cap = int32_t(m.arg(5));
        std::vector<uint8_t> out;
        for (int32_t i = 0; n < 0 || i < n; i++) {
            uint16_t c = uint16_t(m.mem.read(src + 2 * uint32_t(i), 2)); uint8_t b = c < 0x100 ? uint8_t(c) : '?';
            for (int k = 0; k < 32; k++) if (cp1252[k] == c && c >= 0x100) b = uint8_t(0x80 + k);
            out.push_back(b); if (n < 0 && !c) break;
        }
        if (cap == 0) return m.ret(32, uint32_t(out.size()));
        if (int32_t(out.size()) > cap) return m.ret(32, 0);
        m.mem.write_block(dst, out.data(), out.size());
        return m.ret(32, uint32_t(out.size()));
    });
    // character typing / mapping (Latin-1 semantics, C1_* bits)
    auto ctype = [](uint16_t c) -> uint16_t {
        uint16_t t = 0;
        if (c < 0x80) {
            if (std::isupper(c)) t |= 0x1; if (std::islower(c)) t |= 0x2; if (std::isdigit(c)) t |= 0x4; if (std::isspace(c)) t |= 0x8;
            if (std::ispunct(c)) t |= 0x10; if (std::iscntrl(c)) t |= 0x20; if (c == ' ') t |= 0x40; if (std::isxdigit(c)) t |= 0x80; if (std::isalpha(c)) t |= 0x100;
        } else if (c >= 0xc0 && c <= 0xff && c != 0xd7 && c != 0xf7) t |= 0x100 | (c < 0xdf ? 0x1 : 0x2);
        else if (c >= 0xa0) t |= 0x10;
        return t;
    };
    imp(K + "GetStringTypeW", [ctype](Machine& m) {
        uint32_t type = m.arg(0), src = m.arg(1), out = m.arg(3); int32_t n = int32_t(m.arg(2));
        for (int32_t i = 0; n < 0 || i < n; i++) { uint16_t c = uint16_t(m.mem.read(src + 2 * uint32_t(i), 2)); if (n < 0 && !c) break; m.mem.write(out + 2 * uint32_t(i), 2, type == 1 ? ctype(c) : 0); }
        return m.ret(16, 1);
    });
    imp(K + "GetStringTypeA", [ctype](Machine& m) {
        uint32_t type = m.arg(1), src = m.arg(2), out = m.arg(4); int32_t n = int32_t(m.arg(3));
        for (int32_t i = 0; n < 0 || i < n; i++) { uint8_t c = uint8_t(m.mem.read(src + uint32_t(i), 1)); if (n < 0 && !c) break; m.mem.write(out + 2 * uint32_t(i), 2, type == 1 ? ctype(c) : 0); }
        return m.ret(20, 1);
    });
    auto map_char = [](uint16_t c, uint32_t flags) -> uint16_t {
        if (flags & 0x100) { if ((c >= 'A' && c <= 'Z') || (c >= 0xc0 && c <= 0xde && c != 0xd7)) return uint16_t(c + 32); }   // LCMAP_LOWERCASE
        if (flags & 0x200) { if ((c >= 'a' && c <= 'z') || (c >= 0xe0 && c <= 0xfe && c != 0xf7)) return uint16_t(c - 32); }   // LCMAP_UPPERCASE
        return c;
    };
    imp(K + "LCMapStringW", [map_char](Machine& m) {
        uint32_t flags = m.arg(1), src = m.arg(2), dst = m.arg(4); int32_t n = int32_t(m.arg(3)), cap = int32_t(m.arg(5));
        std::vector<uint16_t> out; for (int32_t i = 0; n < 0 || i < n; i++) { uint16_t c = uint16_t(m.mem.read(src + 2 * uint32_t(i), 2)); out.push_back(map_char(c, flags)); if (n < 0 && !c) break; }
        if (flags & 0x400) throw VmError("LCMapStringW LCMAP_SORTKEY");
        if (!cap) return m.ret(24, uint32_t(out.size()));
        if (int32_t(out.size()) > cap) return m.ret(24, 0);
        for (size_t i = 0; i < out.size(); i++) m.mem.write(dst + 2 * uint32_t(i), 2, out[i]);
        return m.ret(24, uint32_t(out.size()));
    });
    imp(K + "LCMapStringA", [map_char](Machine& m) {
        uint32_t flags = m.arg(1), src = m.arg(2), dst = m.arg(4); int32_t n = int32_t(m.arg(3)), cap = int32_t(m.arg(5));
        std::vector<uint8_t> out; for (int32_t i = 0; n < 0 || i < n; i++) { uint8_t c = uint8_t(m.mem.read(src + uint32_t(i), 1)); out.push_back(uint8_t(map_char(c, flags))); if (n < 0 && !c) break; }
        if (flags & 0x400) throw VmError("LCMapStringA LCMAP_SORTKEY");
        if (!cap) return m.ret(24, uint32_t(out.size()));
        if (int32_t(out.size()) > cap) return m.ret(24, 0);
        m.mem.write_block(dst, out.data(), out.size()); return m.ret(24, uint32_t(out.size()));
    });
    auto compare = [](std::vector<uint16_t> a, std::vector<uint16_t> b, uint32_t flags) -> uint32_t {
        if (flags & 1) { for (auto& c : a) if (c >= 'A' && c <= 'Z') c += 32; for (auto& c : b) if (c >= 'A' && c <= 'Z') c += 32; }
        int r = a < b ? -1 : a > b ? 1 : 0; return uint32_t(r + 2);
    };
    imp(K + "CompareStringW", [compare](Machine& m) {
        uint32_t flags = m.arg(1); auto rd16 = [&](uint32_t p, int32_t n) { std::vector<uint16_t> v; for (int32_t i = 0; n < 0 || i < n; i++) { uint16_t c = uint16_t(m.mem.read(p + 2 * uint32_t(i), 2)); if (n < 0 && !c) break; v.push_back(c); } return v; };
        return m.ret(24, compare(rd16(m.arg(2), int32_t(m.arg(3))), rd16(m.arg(4), int32_t(m.arg(5))), flags));
    });
    imp(K + "CompareStringA", [compare](Machine& m) {
        uint32_t flags = m.arg(1); auto rd8 = [&](uint32_t p, int32_t n) { std::vector<uint16_t> v; for (int32_t i = 0; n < 0 || i < n; i++) { uint8_t c = uint8_t(m.mem.read(p + uint32_t(i), 1)); if (n < 0 && !c) break; v.push_back(c); } return v; };
        return m.ret(24, compare(rd8(m.arg(2), int32_t(m.arg(3))), rd8(m.arg(4), int32_t(m.arg(5))), flags));
    });
    imp(K + "IsDBCSLeadByteEx", [](Machine& m) { return m.ret(8, 0); });
    imp(K + "FormatMessageA", [s](Machine& m) { uint32_t buf = m.arg(4), n = m.arg(5); if ((m.arg(0) & 0x100) == 0 && buf && n) s->put_cstr(buf, ""); return m.ret(28, 0); });
    // ---- console / std handles ----
    imp(K + "GetStdHandle", [](Machine& m) { uint32_t n = m.arg(0); return m.ret(4, n == 0xfffffff6u ? 0x10u : n == 0xfffffff5u ? 0x14u : n == 0xfffffff4u ? 0x18u : 0xffffffffu); });
    imp(K + "SetStdHandle", [](Machine& m) { return m.ret(8, 1); });
    imp(K + "GetConsoleCP", [](Machine& m) { return m.ret(0, 437); });
    imp(K + "GetConsoleOutputCP", [](Machine& m) { return m.ret(0, 437); });
    imp(K + "GetConsoleMode", [](Machine& m) { return m.ret(8, 0); });
    imp(K + "WriteConsoleA", [](Machine& m) { return m.ret(20, 0); });
    imp(K + "WriteConsoleW", [](Machine& m) { return m.ret(20, 0); });
    // ---- files ----
    imp(K + "GetFileType", [s](Machine& m) { uint32_t h = m.arg(0); if (h == 0x10 || h == 0x14 || h == 0x18) return m.ret(4, 2); Obj* o = s->obj(h); return m.ret(4, o && o->kind == Obj::File ? 1 : 0); });
    auto open_file = [s](Machine& m, const std::string& guest, uint32_t access, uint32_t disposition) -> uint32_t {
        bool write = access & 0x40000000u;
        std::string game, overlay; s->host_paths(guest, game, overlay);
        std::string path; int fd = -1;
        if (!write) {
            path = s->exists(overlay) ? overlay : game;
            bool dir = false;
            if (path.empty() || !s->exists(path, &dir) || dir) {
                if (disposition == 4 || disposition == 2 || disposition == 1) { write = true; }
                else { s->last_error = 2; s->say("open %s: not found", guest.c_str()); return 0xffffffffu; }
            } else fd = open(path.c_str(), O_RDONLY);
        }
        if (write) {
            s->would_switch();   // a verified call: files written or created cannot be taken back
            path = overlay; s->mkdirs(path);
            bool had = s->exists(overlay);
            if (!had && !game.empty() && s->exists(game) && disposition != 2 && disposition != 1) {   // copy-on-write from the install
                int in = open(game.c_str(), O_RDONLY), out = open(overlay.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                char buf[65536]; ssize_t k; while ((k = read(in, buf, sizeof buf)) > 0) { if (write != 0 && ::write(out, buf, size_t(k)) != k) break; }
                close(in); close(out); had = true;
            }
            int flags = O_RDWR | O_CREAT;
            if (disposition == 1 && had) { s->last_error = 80; return 0xffffffffu; }
            if (disposition == 3 && !had) { s->last_error = 2; return 0xffffffffu; }
            if (disposition == 2 || disposition == 5) flags |= O_TRUNC;
            fd = open(path.c_str(), flags, 0644);
            s->last_error = (disposition == 2 || disposition == 4) && had ? 183 : 0;
        } else s->last_error = 0;
        if (fd < 0) { s->last_error = 5; return 0xffffffffu; }
        auto o = std::make_shared<Obj>(); o->kind = Obj::File; o->fd = fd; o->path = path; o->write = write; o->name = guest;
        s->on_undo([o] { if (o->fd >= 0) { close(o->fd); o->fd = -1; } });
        s->say("open %s -> %s%s", guest.c_str(), path.c_str(), write ? " (write)" : "");
        (void)m; return s->add_handle(o);
    };
    imp(K + "CreateFileA", [s, open_file](Machine& m) { return m.ret(28, open_file(m, s->cstr(m.arg(0)), m.arg(1), m.arg(4))); });
    imp(K + "CreateFileW", [s, open_file](Machine& m) { return m.ret(28, open_file(m, s->wstr(m.arg(0)), m.arg(1), m.arg(4))); });
    imp(K + "ReadFile", [s](Machine& m) {
        Obj* o = s->obj(m.arg(0)); uint32_t buf = m.arg(1), n = m.arg(2), out = m.arg(3);
        if (m.arg(4)) throw VmError("ReadFile: overlapped I/O");
        if (!o || o->kind != Obj::File) { if (out) m.mem.write(out, 4, 0); s->last_error = 6; return m.ret(20, 0); }
        std::vector<uint8_t> tmp(n); ssize_t k = pread(o->fd, tmp.data(), n, off_t(o->pos)); if (k < 0) k = 0;
        s->obj_undo(m.arg(0));
        m.mem.write_block(buf, tmp.data(), size_t(k)); o->pos += uint64_t(k);
        if (out) m.mem.write(out, 4, uint32_t(k));
        return m.ret(20, 1);
    });
    imp(K + "WriteFile", [s](Machine& m) {
        uint32_t h = m.arg(0), buf = m.arg(1), n = m.arg(2), out = m.arg(3);
        std::vector<uint8_t> tmp(n); m.mem.read_block(buf, tmp.data(), n);
        if (h == 0x14 || h == 0x18) { s->say("[stdout] %.*s", int(n), reinterpret_cast<char*>(tmp.data())); if (out) m.mem.write(out, 4, n); return m.ret(20, 1); }
        Obj* o = s->obj(h); if (!o || o->kind != Obj::File) { s->last_error = 6; return m.ret(20, 0); }
        s->would_switch();   // (a verified call: written files cannot be taken back)
        ssize_t k = pwrite(o->fd, tmp.data(), n, off_t(o->pos)); if (k < 0) k = 0; o->pos += uint64_t(k);
        if (out) m.mem.write(out, 4, uint32_t(k));
        return m.ret(20, 1);
    });
    imp(K + "SetFilePointer", [s](Machine& m) {
        Obj* o = s->obj(m.arg(0)); int64_t lo = int32_t(m.arg(1)); uint32_t hip = m.arg(2), method = m.arg(3);
        if (!o || o->kind != Obj::File) { s->last_error = 6; return m.ret(16, 0xffffffffu); }
        int64_t dist = hip ? (int64_t(int32_t(m.mem.read(hip, 4))) << 32) | (lo & 0xffffffff) : lo;
        struct stat st; fstat(o->fd, &st);
        int64_t base = method == 0 ? 0 : method == 1 ? int64_t(o->pos) : int64_t(st.st_size), np = base + dist;
        if (np < 0) { s->last_error = 131; return m.ret(16, 0xffffffffu); }
        s->obj_undo(m.arg(0));
        o->pos = uint64_t(np); if (hip) m.mem.write(hip, 4, uint32_t(o->pos >> 32));
        s->last_error = 0; return m.ret(16, uint32_t(o->pos));
    });
    imp(K + "SetFilePointerEx", [s](Machine& m) {
        Obj* o = s->obj(m.arg(0)); int64_t dist = int64_t(uint64_t(m.arg(1)) | uint64_t(m.arg(2)) << 32); uint32_t out = m.arg(3), method = m.arg(4);
        if (!o || o->kind != Obj::File) return m.ret(20, 0);
        struct stat st; fstat(o->fd, &st);
        int64_t base = method == 0 ? 0 : method == 1 ? int64_t(o->pos) : int64_t(st.st_size); s->obj_undo(m.arg(0)); o->pos = uint64_t(base + dist);
        if (out) m.mem.write(out, 8, o->pos); return m.ret(20, 1);
    });
    imp(K + "GetFileSize", [s](Machine& m) {
        Obj* o = s->obj(m.arg(0)); if (!o || o->kind != Obj::File) return m.ret(8, 0xffffffffu);
        struct stat st; fstat(o->fd, &st); if (m.arg(1)) m.mem.write(m.arg(1), 4, uint32_t(uint64_t(st.st_size) >> 32)); return m.ret(8, uint32_t(st.st_size));
    });
    imp(K + "GetFileSizeEx", [s](Machine& m) {
        Obj* o = s->obj(m.arg(0)); if (!o || o->kind != Obj::File) return m.ret(8, 0);
        struct stat st; fstat(o->fd, &st); m.mem.write(m.arg(1), 8, uint64_t(st.st_size)); return m.ret(8, 1);
    });
    imp(K + "SetEndOfFile", [s](Machine& m) { s->would_switch(); Obj* o = s->obj(m.arg(0)); if (o && o->kind == Obj::File && ftruncate(o->fd, off_t(o->pos)) == 0) return m.ret(4, 1); return m.ret(4, 0); });
    imp(K + "FlushFileBuffers", [](Machine& m) { return m.ret(4, 1); });
    imp(K + "LockFile", [](Machine& m) { return m.ret(20, 1); });
    imp(K + "UnlockFile", [](Machine& m) { return m.ret(20, 1); });
    imp(K + "GetFileInformationByHandle", [s](Machine& m) {
        Obj* o = s->obj(m.arg(0)); if (!o || o->kind != Obj::File) return m.ret(8, 0);
        struct stat st; fstat(o->fd, &st); uint64_t ft = 128952432000000000ull;
        uint32_t v[13] = {0x20, uint32_t(ft), uint32_t(ft >> 32), uint32_t(ft), uint32_t(ft >> 32), uint32_t(ft), uint32_t(ft >> 32), 0x1234, uint32_t(uint64_t(st.st_size) >> 32), uint32_t(st.st_size), 1, 0, uint32_t(st.st_ino)};
        m.mem.write_block(m.arg(1), v, 52); return m.ret(8, 1);
    });
    imp(K + "SetFileTime", [](Machine& m) { return m.ret(16, 1); });
    auto attributes = [s](const std::string& guest) -> uint32_t {
        std::string game, overlay; s->host_paths(guest, game, overlay); bool dir = false;
        if (s->exists(overlay, &dir) || s->exists(game, &dir)) return dir ? 0x10u : 0x20u;
        s->last_error = 2; return 0xffffffffu;
    };
    imp(K + "GetFileAttributesA", [s, attributes](Machine& m) { std::string g = s->cstr(m.arg(0)); uint32_t a = attributes(g); s->say("GetFileAttributes %s -> %x", g.c_str(), a); return m.ret(4, a); });
    imp(K + "GetFileAttributesW", [s, attributes](Machine& m) { return m.ret(4, attributes(s->wstr(m.arg(0)))); });
    imp(K + "SetFileAttributesA", [](Machine& m) { return m.ret(8, 1); });
    imp(K + "SetFileAttributesW", [](Machine& m) { return m.ret(8, 1); });
    imp(K + "CreateDirectoryA", [s](Machine& m) { s->would_switch(); std::string game, overlay; s->host_paths(s->cstr(m.arg(0)), game, overlay); s->mkdirs(overlay + "/"); return m.ret(8, 1); });
    imp(K + "CreateDirectoryW", [s](Machine& m) { s->would_switch(); std::string game, overlay; s->host_paths(s->wstr(m.arg(0)), game, overlay); s->mkdirs(overlay + "/"); return m.ret(8, 1); });
    imp(K + "DeleteFileA", [s](Machine& m) { s->would_switch(); std::string game, overlay; s->host_paths(s->cstr(m.arg(0)), game, overlay); unlink(overlay.c_str()); return m.ret(4, 1); });
    imp(K + "GetCurrentDirectoryA", [s](Machine& m) { uint32_t n = m.arg(0), buf = m.arg(1); if (n < s->cwd.size() + 1) return m.ret(8, uint32_t(s->cwd.size() + 1)); s->put_cstr(buf, s->cwd); return m.ret(8, uint32_t(s->cwd.size())); });
    imp(K + "GetCurrentDirectoryW", [s](Machine& m) { uint32_t n = m.arg(0), buf = m.arg(1); if (n < s->cwd.size() + 1) return m.ret(8, uint32_t(s->cwd.size() + 1)); s->put_wstr(buf, s->cwd); return m.ret(8, uint32_t(s->cwd.size())); });
    imp(K + "SetCurrentDirectoryA", [s](Machine& m) { std::string was = s->cwd; s->on_undo([s, was] { s->cwd = was; }); s->cwd = s->norm(s->cstr(m.arg(0))); s->say("cwd %s", s->cwd.c_str()); return m.ret(4, 1); });
    imp(K + "GetFullPathNameA", [s](Machine& m) {
        std::string p = s->norm(s->cstr(m.arg(0))); uint32_t n = m.arg(1), buf = m.arg(2), part = m.arg(3);
        if (n < p.size() + 1) return m.ret(16, uint32_t(p.size() + 1));
        s->put_cstr(buf, p); if (part) { size_t sl = p.find_last_of('\\'); m.mem.write(part, 4, buf + uint32_t(sl + 1)); }
        return m.ret(16, uint32_t(p.size()));
    });
    imp(K + "GetDriveTypeA", [](Machine& m) { return m.ret(4, 3); });
    imp(K + "GetLogicalDrives", [](Machine& m) { return m.ret(0, 4); });
    imp(K + "GetSystemDirectoryW", [s](Machine& m) { std::string d = "C:\\Windows\\system32"; s->put_wstr(m.arg(0), d); return m.ret(8, uint32_t(d.size())); });
    imp(K + "GetSystemWindowsDirectoryW", [s](Machine& m) { std::string d = "C:\\Windows"; s->put_wstr(m.arg(0), d); return m.ret(8, uint32_t(d.size())); });
    imp(K + "GetWindowsDirectoryA", [s](Machine& m) { std::string d = "C:\\Windows"; s->put_cstr(m.arg(0), d); return m.ret(8, uint32_t(d.size())); });
    imp(K + "GetDiskFreeSpaceExA", [](Machine& m) { for (int i = 1; i <= 3; i++) if (m.arg(i)) m.mem.write(m.arg(i), 8, 50ull << 30); return m.ret(16, 1); });
    imp(K + "GetDiskFreeSpaceA", [](Machine& m) { if (m.arg(1)) m.mem.write(m.arg(1), 4, 8); if (m.arg(2)) m.mem.write(m.arg(2), 4, 512); if (m.arg(3)) m.mem.write(m.arg(3), 4, 0x1000000); if (m.arg(4)) m.mem.write(m.arg(4), 4, 0x2000000); return m.ret(20, 1); });
    // FindFirstFile over the install merged with the overlay
    auto find_fill = [s](Machine& m, Obj* o, uint32_t out) -> bool {
        while (o->next < o->entries.size()) {
            std::string name = o->entries[o->next++];
            std::string game, overlay; s->host_paths(o->dir + "\\" + name, game, overlay);
            struct stat st; std::string p = s->exists(overlay) ? overlay : game;
            if (stat(p.c_str(), &st) != 0) continue;
            std::vector<uint8_t> v(320, 0); uint32_t attr = S_ISDIR(st.st_mode) ? 0x10 : 0x20; uint64_t ft = 128952432000000000ull;
            std::memcpy(v.data(), &attr, 4); for (int k = 0; k < 3; k++) std::memcpy(v.data() + 4 + 8 * k, &ft, 8);
            uint32_t hi = uint32_t(uint64_t(st.st_size) >> 32), lo = uint32_t(st.st_size); std::memcpy(v.data() + 28, &hi, 4); std::memcpy(v.data() + 32, &lo, 4);
            std::memcpy(v.data() + 44, name.c_str(), std::min<size_t>(name.size(), 259));
            m.mem.write_block(out, v.data(), v.size());
            return true;
        }
        return false;
    };
    imp(K + "FindFirstFileA", [s, find_fill](Machine& m) {
        std::string pattern = s->norm(s->cstr(m.arg(0))); size_t sl = pattern.find_last_of('\\');
        auto o = std::make_shared<Obj>(); o->kind = Obj::Find; o->dir = pattern.substr(0, sl); std::string mask = pattern.substr(sl + 1);
        std::set<std::string> names;
        std::string game, overlay; s->host_paths(o->dir, game, overlay);
        for (const std::string& d : {game, overlay}) {
            if (d.empty()) continue; DIR* dd = opendir(d.c_str()); if (!dd) continue;
            while (dirent* e = readdir(dd)) { std::string n = e->d_name; if (n == "." || n == "..") continue; if (wildcard(mask.c_str(), n.c_str())) names.insert(n); }
            closedir(dd);
        }
        o->entries.assign(names.begin(), names.end());
        s->say("FindFirstFile %s: %zu", pattern.c_str(), o->entries.size());
        if (!find_fill(m, o.get(), m.arg(1))) { s->last_error = 2; return m.ret(8, 0xffffffffu); }
        return m.ret(8, s->add_handle(o));
    });
    imp(K + "FindNextFileA", [s, find_fill](Machine& m) { Obj* o = s->obj(m.arg(0)); s->obj_undo(m.arg(0)); if (o && o->kind == Obj::Find && find_fill(m, o, m.arg(1))) return m.ret(8, 1); s->last_error = 18; return m.ret(8, 0); });
    imp(K + "FindClose", [s](Machine& m) {
        uint32_t h = m.arg(0); auto it = s->handles.find(h);
        if (it != s->handles.end()) { std::shared_ptr<Obj> o = it->second; s->on_undo([s, h, o] { s->handles[h] = o; }); s->handles.erase(it); }
        return m.ret(4, 1);
    });
    imp(K + "GetLongPathNameW", [s](Machine& m) { std::string p = s->wstr(m.arg(0)); s->put_wstr(m.arg(1), p); return m.ret(12, uint32_t(p.size())); });
    // Side-by-side activation: msvcr90 must find itself in the activation context (else R6034).
    imp(K + "FindActCtxSectionStringW", [](Machine& m) {
        uint32_t out = m.arg(4); uint32_t size = out ? uint32_t(m.mem.read(out, 4)) : 0;
        if (out && size > 4) { m.mem.write_block(out + 4, nullptr, size - 4); }
        return m.ret(20, 1);
    });
    imp(K + "GetCurrentActCtx", [](Machine& m) { m.mem.write(m.arg(0), 4, 0); return m.ret(4, 1); });
    imp(K + "ActivateActCtx", [](Machine& m) { if (m.arg(1)) m.mem.write(m.arg(1), 4, 1); return m.ret(8, 1); });
    imp(K + "DeactivateActCtx", [](Machine& m) { return m.ret(8, 1); });
    imp(K + "ReleaseActCtx", [](Machine& m) { return m.ret(4); });
    imp(K + "DisableThreadLibraryCalls", [](Machine& m) { return m.ret(4, 1); });
    imp(K + "GetThreadContext", [](Machine&) -> uint32_t { throw VmError("GetThreadContext"); });
    imp(K + "RtlUnwind", [](Machine&) -> uint32_t { throw VmError("RtlUnwind (SEH unwinding is not modelled)"); });
    imp(K + "Beep", [](Machine& m) { return m.ret(8, 1); });
    imp(K + "GlobalMemoryStatus", [](Machine& m) { uint32_t p = m.arg(0); uint32_t v[8] = {32, 30, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7ffe0000, 0x7ff00000}; m.mem.write_block(p, v, 32); return m.ret(4); });
    imp(K + "GlobalMemoryStatusEx", [](Machine& m) { uint32_t p = m.arg(0); m.mem.write(p + 4, 4, 30); m.mem.write(p + 8, 8, 4ull << 30); m.mem.write(p + 16, 8, 3ull << 30); m.mem.write(p + 24, 8, 8ull << 30); m.mem.write(p + 32, 8, 6ull << 30); m.mem.write(p + 40, 8, 0x7ffe0000ull); m.mem.write(p + 48, 8, 0x70000000ull); m.mem.write(p + 56, 8, 0); return m.ret(4, 1); });
}

// ======================================================================== user32 / window
void Boot::Impl::install_user32() {
    Impl* s = this;
    const std::string U = "user32.dll!";
    static uint32_t wndproc = 0;
    imp(U + "RegisterClassExA", [](Machine& m) { wndproc = uint32_t(m.mem.read(m.arg(0) + 8, 4)); return m.ret(4, 0xc001); });
    imp(U + "RegisterClassA", [](Machine& m) { wndproc = uint32_t(m.mem.read(m.arg(0) + 4, 4)); return m.ret(4, 0xc001); });
    imp(U + "CreateWindowExA", [s](Machine& m) { s->say("CreateWindowExA '%s' %ux%u", s->cstr(m.arg(2)).c_str(), m.arg(6), m.arg(7)); return m.ret(48, 0x10010); });
    imp(U + "DestroyWindow", [](Machine& m) { return m.ret(4, 1); });
    imp(U + "ShowWindow", [](Machine& m) { return m.ret(8, 0); });
    imp(U + "UpdateWindow", [](Machine& m) { return m.ret(4, 1); });
    imp(U + "SetWindowPos", [](Machine& m) { return m.ret(28, 1); });
    imp(U + "MoveWindow", [](Machine& m) { return m.ret(24, 1); });
    imp(U + "AdjustWindowRect", [](Machine& m) { return m.ret(12, 1); });
    imp(U + "AdjustWindowRectEx", [](Machine& m) { return m.ret(16, 1); });
    imp(U + "GetSystemMetrics", [](Machine& m) { uint32_t i = m.arg(0); return m.ret(4, i == 0 ? 1920u : i == 1 ? 1080u : i == 4 ? 20u : i == 32 || i == 33 ? 4u : 0u); });
    imp(U + "GetDesktopWindow", [](Machine& m) { return m.ret(0, 0x10014); });
    imp(U + "GetWindowRect", [s](Machine& m) { uint32_t v[4] = {0, 0, s->cfg.width, s->cfg.height}; if (m.arg(0) == 0x10014) { v[2] = 1920; v[3] = 1080; } m.mem.write_block(m.arg(1), v, 16); return m.ret(8, 1); });
    imp(U + "GetClientRect", [s](Machine& m) { uint32_t v[4] = {0, 0, s->cfg.width, s->cfg.height}; m.mem.write_block(m.arg(1), v, 16); return m.ret(8, 1); });
    imp(U + "ClientToScreen", [](Machine& m) { return m.ret(8, 1); });
    imp(U + "ScreenToClient", [](Machine& m) { return m.ret(8, 1); });
    imp(U + "GetCursorPos", [s](Machine& m) { m.mem.write(m.arg(0), 4, uint32_t(s->cursor_x)); m.mem.write(m.arg(0) + 4, 4, uint32_t(s->cursor_y)); return m.ret(4, 1); });
    imp(U + "SetCursorPos", [](Machine& m) { return m.ret(8, 1); });
    imp(U + "ClipCursor", [](Machine& m) { return m.ret(4, 1); });
    imp(U + "ShowCursor", [](Machine& m) { return m.ret(4, 0); });
    imp(U + "SetCursor", [](Machine& m) { return m.ret(4, 0); });
    imp(U + "LoadIconA", [](Machine& m) { return m.ret(8, 0x20001); });
    imp(U + "LoadCursorA", [](Machine& m) { return m.ret(8, 0x20002); });
    imp(U + "DefWindowProcA", [](Machine& m) { return m.ret(16, 0); });
    imp(U + "GetWindowLongA", [](Machine& m) { return m.ret(8, 0); });
    imp(U + "SetWindowLongA", [](Machine& m) { return m.ret(12, 0); });
    imp(U + "GetParent", [](Machine& m) { return m.ret(4, 0); });
    imp(U + "SendMessageA", [](Machine& m) { return m.ret(16, 0); });
    imp(U + "PostMessageA", [](Machine& m) { return m.ret(16, 1); });
    imp(U + "PeekMessageA", [](Machine& m) { return m.ret(20, 0); });
    imp(U + "GetMessageA", [](Machine&) -> uint32_t { throw VmError("GetMessageA (blocking message loop)"); });
    imp(U + "TranslateMessage", [](Machine& m) { return m.ret(4, 0); });
    imp(U + "DispatchMessageA", [](Machine& m) { return m.ret(4, 0); });
    imp(U + "RegisterRawInputDevices", [](Machine& m) { return m.ret(12, 1); });
    imp(U + "GetRawInputData", [](Machine& m) { return m.ret(20, 0xffffffffu); });
    imp(U + "CopyRect", [](Machine& m) { uint32_t v[4]; m.mem.read_block(m.arg(1), v, 16); m.mem.write_block(m.arg(0), v, 16); return m.ret(8, 1); });
    imp(U + "OffsetRect", [](Machine& m) { uint32_t p = m.arg(0); int32_t dx = int32_t(m.arg(1)), dy = int32_t(m.arg(2)); for (int i = 0; i < 4; i++) m.mem.write(p + 4 * i, 4, uint32_t(int32_t(m.mem.read(p + 4 * i, 4)) + (i % 2 ? dy : dx))); return m.ret(12, 1); });
    imp(U + "MessageBoxA", [s](Machine& m) -> uint32_t {
        std::string text = s->cstr(m.arg(1)), cap = s->cstr(m.arg(2)); s->say("MessageBox '%s': %s", cap.c_str(), text.c_str());
        throw VmError("MessageBox '" + cap + "': " + text);
    });
    imp(U + "DialogBoxParamA", [](Machine&) -> uint32_t { throw VmError("DialogBoxParamA"); });
    imp(U + "EndDialog", [](Machine& m) { return m.ret(8, 1); });
    imp(U + "SetDlgItemTextA", [](Machine& m) { return m.ret(12, 1); });
    imp(U + "OpenClipboard", [](Machine& m) { return m.ret(4, 0); });
    imp(U + "CloseClipboard", [](Machine& m) { return m.ret(0, 1); });
    imp(U + "EmptyClipboard", [](Machine& m) { return m.ret(0, 1); });
    imp(U + "SetClipboardData", [](Machine& m) { return m.ret(8, 0); });
    imp(U + "GetProcessWindowStation", [](Machine& m) { return m.ret(0, 0x30); });
    imp(U + "GetUserObjectInformationA", [](Machine& m) { uint32_t out = m.arg(2); if (out && m.arg(3) >= 12) { m.mem.write(out, 4, 0); m.mem.write(out + 4, 4, 0); m.mem.write(out + 8, 4, 1); } if (m.arg(4)) m.mem.write(m.arg(4), 4, 12); return m.ret(20, 1); });
    imp(U + "GetLastActivePopup", [](Machine& m) { return m.ret(4, m.arg(0)); });
    imp(U + "GetForegroundWindow", [](Machine& m) { return m.ret(0, 0x10010); });
    imp(U + "GetActiveWindow", [](Machine& m) { return m.ret(0, 0x10010); });
    imp(U + "GetFocus", [](Machine& m) { return m.ret(0, 0x10010); });
    imp(U + "SetFocus", [](Machine& m) { return m.ret(4, 0x10010); });
    imp(U + "SetForegroundWindow", [](Machine& m) { return m.ret(4, 1); });
    imp(U + "IsIconic", [](Machine& m) { return m.ret(4, 0); });
    imp(U + "IsWindowVisible", [](Machine& m) { return m.ret(4, 1); });
    imp(U + "GetKeyState", [s](Machine& m) { uint32_t vk = m.arg(0) & 0xff; (void)s; return m.ret(4, 0); (void)vk; });
    imp(U + "GetAsyncKeyState", [](Machine& m) { return m.ret(4, 0); });
    imp(U + "MonitorFromWindow", [](Machine& m) { return m.ret(8, 0x10020); });
    imp(U + "EnumDisplaySettingsA", [](Machine& m) { return m.ret(12, 0); });
    imp(U + "ChangeDisplaySettingsA", [](Machine& m) { return m.ret(8, 0); });
}

#include "boot_devices.inc"

// ======================================================================== startup
Boot::Boot(const BootConfig& config) : d(new Impl) {
    d->cfg = config;
    // Opt-in pinned frame clock (clock_pin.h): every frame advances by one fixed
    // frame; instruction time is off.  Unset: frame_dt stays 0 (the old path).
    d->fixed_dt = fixed_clock_ticks();
    if (d->fixed_dt) d->frame_dt = d->fixed_dt;
}
Boot::~Boot() { reflex_port::set_host_state(d->m, nullptr); d->quit_fibers(); d->commit_closes(); d->write_thread_coverage(); if (d->log) std::fclose(d->log); if (d->tlog) { for (auto& [k, n] : d->polls) std::fprintf(d->tlog, "Z %x %08x %llu\n", k.first, k.second, (unsigned long long)n);
    for (auto& [a, n] : d->spins) std::fprintf(d->tlog, "Z 20000000 %08x %llu\n", a, (unsigned long long)n); std::fclose(d->tlog); } for (auto& [h, o] : d->handles) if (o->kind == Obj::File && o->fd >= 0) close(o->fd); delete d; }
Machine& Boot::machine() { return d->m; }
uint64_t Boot::frames() const { return d->frames; }
uint64_t Boot::now() const { return d->now_; }
std::string Boot::status() const { return d->describe(); }
void Boot::set_keys(const uint8_t keys[256]) { std::memcpy(d->keys, keys, 256); }
void Boot::set_d3d_hook(D3DHook hook) { d->d3d_hook = std::move(hook); }
bool Boot::d3d_resource(uint32_t object, D3DResource& out) const { return (d->d3d_resource && d->d3d_resource(object, out)) || (d->d3dx_constant_table && d->d3dx_constant_table(object, out)); }

void Boot::start() {
    Impl& s = *d; Machine& m = s.m;
    if (!s.cfg.log_path.empty()) s.log = std::fopen(s.cfg.log_path.c_str(), "w");
    if (const char* tf = std::getenv("REFLEX_BOOT_THREADS")) s.tlog = std::fopen(tf, "w");
    if (s.cfg.jit) { m.jit_exact_slices = !s.cfg.baton || s.cfg.baton_slices; m.enable_jit(true); }   // thread switches stay at the interpreter's exact instruction counts
    m.mem.set_loader([](uint32_t, uint8_t*) {});            // backed pages start zeroed
    s.process_heap = PROCESS_HEAP;
    // PEB and the main thread's stack
    m.mem.map(PEB, nullptr, 4096);
    m.mem.write(PEB + 0x08, 4, 0x400000); m.mem.write(PEB + 0x18, 4, PROCESS_HEAP); m.mem.write(PEB + 0x64, 4, s.cfg.cpus);
    m.mem.write(PEB + 0xa4, 4, 6); m.mem.write(PEB + 0xa8, 4, 1); m.mem.write(PEB + 0xac, 2, 7601);
    for (uint32_t a = MAIN_STACK_LO; a < MAIN_STACK_HI; a += 4096) m.mem.back(a);
    m.mem.map(TEB, nullptr, 4096);
    s.load_images();
    s.install_kernel32(); s.install_user32(); s.install_devices();
    if (!s.cfg.remote_storage.empty()) {   // MOTOCAREER (career.h)
        std::set<std::string> had_imports; for (auto& [n, f] : m.imports) had_imports.insert(n);
        std::set<uint32_t> had_natives; for (auto& [a, f] : m.natives) had_natives.insert(a);
        install_remote_storage(m, s.cfg.remote_storage); install_steam_user(m, s.cfg.steam_persona);
        // Verify inside the boot: the Steam stand-ins write the cloud folder and keep their own state, which the undo does not cover:
        // a verified call that reaches them runs unverified (WouldSwitch)
        for (auto& [n, f] : m.imports) if (!had_imports.count(n)) { Native inner = f; f = [d = d, inner](Machine& mm) { d->would_switch(); return inner(mm); }; }
        for (auto& [a, f] : m.natives) if (!had_natives.count(a)) { Native inner = f; f = [d = d, inner](Machine& mm) { d->would_switch(); return inner(mm); }; }
    }
    // native/game Verify inside the boot (notes/loading.md): nested calls of natives get the whole budget of a thread, and the host
    // state (heap, handles, files, time, critical sections, TLS, threads, device stand-ins) is undone with the guest memory
    m.call_budget = UINT64_MAX / 4;
    reflex_port::set_host_state(m, [d = d]() { return d->host_snapshot(); });
    // the executable's static TLS slot
    if (s.exe_tls_index_addr) { s.tls_used[0] = 1; s.exe_tls_index = 0; m.mem.write(s.exe_tls_index_addr, 4, 0); }
    // main thread (its stack below the executable, like Windows)
    s.new_thread(0, 0, false, "main");
    Thread& main = *s.threads[0];
    s.vfree(main.stack_lo);
    main.stack_lo = MAIN_STACK_LO; main.stack_hi = MAIN_STACK_HI;
    std::memcpy(main.teb.data() + 4, &main.stack_hi, 4); std::memcpy(main.teb.data() + 8, &main.stack_lo, 4);
    main.ctx.r[4] = MAIN_STACK_HI - 16;
    s.cur = 0; s.load(main);
    // ENGINE_FRAME: frame boundary of the main thread (host stops here)
    m.natives[ENGINE_FRAME] = [d = d](Machine& m) -> uint32_t {
        if (d->resume_bypass) { d->resume_bypass = false; return RUN_ORIGINAL; }
        d->would_switch();   // a verified call that runs a whole frame (the frame stop and the frame's waits switch threads)
        d->frames++;
        if (d->tlog) { uint32_t g = m.mem.mapped(0xd728d8) ? uint32_t(m.mem.read(0xd728d8, 4)) : 0; d->tl("F %llu %llu %llu %d", (unsigned long long)d->frames, (unsigned long long)m.steps, (unsigned long long)d->now_, g ? int(m.mem.read(g + 0x18, 4)) : -1); }
        if (d->frame_dt) d->now_ += d->frame_dt;
        if (d->cfg.baton && tl_guest && d->frames >= d->stop_at) {   // back to the driver; Boot::call_in_frame may have changed where it goes on
            Thread& t = *d->threads[0]; t.state = Thread::Ready;
            d->park(t, ENGINE_FRAME, true);
            return t.ctx.pc == ENGINE_FRAME ? RUN_ORIGINAL : t.ctx.pc;
        }
        if (d->frames >= d->stop_at) { d->stop_request = true; d->yield_request = true; return 0; }
        (void)m; return RUN_ORIGINAL;
    };
    // Boot::call_in_frame: the call returned; the main thread's registers as they were at the frame boundary, which it enters again
    m.natives[FRAME_CALL_RETURN] = [d = d](Machine& m) -> uint32_t {
        d->frame_call_result = m.r[0]; d->frame_call_done = true;
        const Ctx& c = d->frame_call_saved; std::memcpy(m.r, c.r, sizeof c.r); m.flags = c.flags;
        return ENGINE_FRAME;
    };
    // Baton scheduling: the game's memory spin-waits (loops that poll state another thread changes, with no wait, Sleep or yield in
    // them; on Windows the other thread runs on another core, under the exact scheduler in the next time slice). Each listed function
    // is called in such a loop; its entry yields (the others run until they block), so the loop sees their progress. Found with
    // REFLEX_BOOT_WATCHDOG (native/game notes/loading.md). A port at the address runs after the yield.
    if (s.cfg.baton && !s.cfg.baton_slices) {
        static const uint32_t spin_sites[] = {
            0x5dbe50,   // UIManager::vf30 (UI startup): do { done = FUN_005dbe50(last DefaultPackages package) } while (!done), the package
                        // loaded by the Database async loader (0084c760)
        };
        for (uint32_t a : spin_sites)
            m.natives[a] = [d = d, a](Machine& m) -> uint32_t {
                if (tl_guest) { d->spins[a]++; d->yield(d->T()); }
                if (auto p = reflex_port::installed(m, a)) return (*p)(m);
                return RUN_ORIGINAL;
            };
    }
    // the outer physics update's context (EAX), recorded for world images (header word 2)
    m.natives[0x769b80] = [d = d](Machine& m) -> uint32_t {
        d->physics_context = m.r[0];
        if (auto p = reflex_port::installed(m, 0x769b80)) return (*p)(m);   // World_PhysicsUpdate's port (native/game), when installed
        return RUN_ORIGINAL;
    };
    // DLL initialisation (DLL_PROCESS_ATTACH), on the main thread
    for (auto& md : s.mods) {
        if (md.name == "mxreflex.exe") continue;
        uint32_t args[3] = {md.base, 1, 0};
        s.say("DllMain %s", md.name.c_str());
        uint32_t ok = m.call_function(md.entry, args, 3);
        if (s.yield_request) throw VmError("DllMain of " + md.name + " blocked");
        if (!ok) throw VmError("DllMain of " + md.name + " failed");
    }
    // the executable's entry (mainCRTStartup -> WinMain); returning ends the main thread
    uint32_t entry = 0; for (auto& md : s.mods) if (md.name == "mxreflex.exe") entry = md.entry;
    m.push(THREAD_EXIT);
    s.save(main, entry);
    s.say("entry %08x", entry);
    s.schedule(1);
    s.say("first Engine::Frame after %llu instructions, %llu ms virtual", (unsigned long long)m.steps, (unsigned long long)(s.now_ / 10000));
}

bool Boot::baton() const { return d->cfg.baton && !d->cfg.baton_slices; }
uint32_t Boot::settle_front_end(uint32_t max_frames) {
    uint32_t n = 0;
    if (baton()) while (d->now_ < 50000000ull && n < max_frames) { run_frames(1, 0); n++; }
    return n;
}
uint32_t Boot::bink_opens() const { return d->bink_file_opens; }
uint32_t Boot::wait_logo_video(uint32_t opens_before, uint32_t max_frames) {
    uint32_t n = 0;
    if (baton()) while (d->bink_file_opens == opens_before && n < max_frames) { run_frames(1, 0); n++; }
    return n;
}
uint32_t Boot::read32(uint32_t a) { return d->m.mem.mapped(a) ? uint32_t(d->m.mem.read(a, 4)) : 0; }
uint32_t Boot::guest_string(const std::string& str) { uint32_t a = d->halloc(PROCESS_HEAP, uint32_t(str.size() + 1), false); d->put_cstr(a, str); return a; }
uint32_t Boot::call(const GuestCall& c) {
    Impl& s = *d; Machine& m = s.m; Thread& main = *s.threads[0];
    if (main.ctx.pc != ENGINE_FRAME) throw VmError("Boot::call: the main thread is not between frames");
    Ctx saved = main.ctx; size_t was = s.cur; s.cur = 0;
    s.load(main);
    m.r[0] = c.eax; m.r[1] = c.ecx; m.r[2] = c.edx; m.r[6] = c.esi; m.r[7] = c.edi;
    m.r[4] -= 0x100;   // below the frame's own stack
    for (size_t i = c.args.size(); i-- > 0;) m.push(c.args[i]);
    m.push(0);
    s.yield_request = false;
    m.run(c.fn);
    if (s.yield_request) throw VmError("Boot::call: the called function blocked");
    uint32_t result = m.r[0];
    s.save(main, ENGINE_FRAME); main.ctx = saved; s.cur = was;
    return result;
}

uint32_t Boot::call_in_frame(const GuestCall& c, uint32_t max_frames) {
    Impl& s = *d; Machine& m = s.m; Thread& main = *s.threads[0];
    if (main.ctx.pc != ENGINE_FRAME) throw VmError("Boot::call_in_frame: the main thread is not between frames");
    s.frame_call_saved = main.ctx; s.frame_call_done = false;
    Ctx& x = main.ctx;
    x.r[0] = c.eax; x.r[1] = c.ecx; x.r[2] = c.edx; x.r[6] = c.esi; x.r[7] = c.edi;
    x.r[4] -= 0x100;   // below the frame's own stack
    for (size_t i = c.args.size(); i-- > 0;) { x.r[4] -= 4; m.mem.write(x.r[4], 4, c.args[i]); }
    x.r[4] -= 4; m.mem.write(x.r[4], 4, FRAME_CALL_RETURN);
    x.pc = c.fn; s.resume_bypass = false;
    for (uint32_t f = 0; f < max_frames && !s.frame_call_done; f++) s.schedule(1);
    if (!s.frame_call_done) throw VmError("Boot::call_in_frame: the call did not return in time");
    return s.frame_call_result;
}

bool Boot::start_event(const EventSpec& e, uint32_t max_frames, std::string* out) {
    if (e.series_id >= 0) return career::start_series_event(*this, e, max_frames, out);   // MOTOCAREER (career.cpp)
    if (e.type == "Tutorial") return tutorial::start(*this, e, max_frames, out);           // MotoSkills (WP-T, tutorial.cpp)
    auto note = [&](const std::string& x) { d->say("EVENT %s", x.c_str()); d->tl("P %llu %s", (unsigned long long)d->m.steps, x.c_str()); if (out) *out += x + "\n"; };
    auto state = [&]() { uint32_t g = read32(0xd728d8); return g ? int(read32(g + 0x18)) : -1; };
    uint32_t frames = 0;
    auto frame = [&](uint32_t n = 1) { for (uint32_t i = 0; i < n; i++) { run_frames(1, 0); frames++; if (progress) progress(frames, state()); } };
    while (state() != 2) { if (frames > max_frames) { note("front end not reached"); return false; } frame(); }
    // let the front end settle (profile-login UI event). Baton scheduling: loading costs no virtual time, so the front end is reached
    // after ~1 s instead of ~5 s; its timed steps (the startup packages' asset binding, ~frame 77 / 1.8 s in the exact boot) need the
    // time a player spends on the title page: 5 s.
    frame(30);
    if (d->cfg.baton && !d->cfg.baton_slices) while (d->now_ < 50000000ull && frames <= max_frames) frame();
    uint32_t game = read32(0xd728d8), ui = read32(game + 0x745c);
    uint32_t manager = read32(0xd92ee0), profile = manager ? read32(manager + 4) : 0;
    char b[200]; std::snprintf(b, sizeof b, "front end after %u frames: game %08x ui %08x profile %08x", frames, game, ui, profile); note(b);
    std::snprintf(b, sizeof b, "focus flag %u", read32(0xa9596c)); note(b);
    if (!read32(0xa9596c)) {   // logical focus: WM_SETFOCUS through the game's window procedure (00880dc0), as Windows sends it to the foreground window
        GuestCall focus; focus.fn = 0x880dc0; focus.args = {0x10010, 7, 0, 0}; call(focus);
        std::snprintf(b, sizeof b, "WM_SETFOCUS -> focus %u", read32(0xa9596c)); note(b);
    }
    if (!ui || !profile) return false;
    GuestCall signin; signin.fn = 0x6166a0; signin.eax = profile; signin.edi = 0;          // no-storage sign-in
    std::snprintf(b, sizeof b, "sign-in -> %u (status %u)", call(signin), read32(profile + 4)); note(b); frame();
    GuestCall ensure; ensure.fn = 0x59e030; ensure.args = {0, uint32_t(e.activity)};         // EnsureLocalPlayers(0, activity)
    call(ensure); frame();
    GuestCall primary; primary.fn = 0x743030; primary.eax = 0; call(primary);                // SetPrimaryPlayerDeviceIndex(0)
    auto set_string = [&](const char* key, const std::string& value) { GuestCall c; c.fn = 0x5dbf50; c.esi = ui; c.edi = guest_string(key); c.args = {guest_string(value)}; call(c); note(std::string("UI ") + key + " = " + value); };
    auto set_int = [&](const char* key, int value) { GuestCall c; c.fn = 0x5dbfb0; c.ecx = ui; c.args = {guest_string(key), uint32_t(value)}; call(c); note(std::string("UI ") + key + " = " + std::to_string(value)); };
    set_string("PlayerMode", "SinglePlayer"); set_string("EventType", e.type); set_int("EventTypeId", e.type_id);
    if (!e.activation.empty()) set_string("EventTypeActivation", e.activation);   // a Free Ride challenge (modes.h)
    if (e.cursor_off_screen) d->cursor_x = d->cursor_y = -10000;
    if (e.series_challenge >= 0) set_int("SeriesId", e.series_challenge);
    if (e.exclusive_device >= 0) {   // FUN_0059a910 (modes.h)
        d->m.mem.write(0x9c4ee0, 4, uint32_t(e.exclusive_device)); d->m.mem.write(0xa94070, 4, read32(0xa94070) + 1);
        set_int("ExclusiveDeviceIndex", e.exclusive_device);
    }
    set_int("AIDifficulty", e.ai); set_int("EventRiders", e.riders); set_int("VehicleClass", e.vehicle_class);
    set_string("EventLocation", e.location); set_int("EventLocationId", e.location_id); set_int("TrackId", e.track_id);
    if (!e.duration_type.empty()) { set_int("EventDuration", e.duration); set_string("EventDurationType", e.duration_type); }
    frame();
    call(ensure); frame();
    if (e.rider_owner >= 0 || e.vehicle_owner >= 0 || e.vehicle_type >= 0) {
        // VehicleSelect:End (arcade accepted): playerRow = MXTableHelper:GetPlayerTableRow(0) (004e78b0: the "Player List" 00ae0e08
        // row whose Controller is 0), EquipVehicle(row, vehicleId, owner) and EquipRider(row, riderId, gearSetId, owner), then
        // OnAcceptVehicleList's Player1VehicleTypeId. Equip writes the row's RequestedVehicleId / RequestedRiderId (new "Vehicle
        // List" / "Rider List" rows, 004de810 / 004dea90); the event's player slot takes them (0054a7b0 -> P+0x1c4 / P+0x1c0,
        // 00767200 -> P+0x1b0 VehicleId / P+0x1b8 RiderId).
        int row = -1;
        uint32_t cb = read32(0xae0e08 + 0x10), ce = read32(0xae0e08 + 0x14);
        for (uint32_t p = cb; p < ce && row < 0; p += 4) {
            uint32_t col = read32(p); char name[16] = {}; d->m.mem.read_bytes(col, name, 15);
            if (std::strcmp(name, "Controller") || read32(col + 0x2c) != 1) continue;
            uint32_t holder = read32(col + 0x40), first = read32(holder + 0xc), last = read32(holder + 0x10);
            for (uint32_t r = 0; first + 4 * r < last; r++) if (read32(first + 4 * r) == 0) { row = int(r); break; }
        }
        std::snprintf(b, sizeof b, "Player List row of controller 0: %d", row); note(b);
        if (row < 0) return false;
        if (e.vehicle_owner >= 0) {
            GuestCall c; c.fn = 0x4e7930; c.eax = uint32_t(e.vehicle_owner); c.args = {uint32_t(row), uint32_t(e.vehicle_id)}; call(c);
            std::snprintf(b, sizeof b, "EquipVehicle(row %d, vehicle %d, owner %d)", row, e.vehicle_id, e.vehicle_owner); note(b);
        }
        if (e.rider_owner >= 0) {
            GuestCall c; c.fn = 0x4e7ae0; c.eax = uint32_t(e.rider_owner); c.args = {uint32_t(row), uint32_t(e.rider_id), uint32_t(e.rider_gear_set)}; call(c);
            std::snprintf(b, sizeof b, "EquipRider(row %d, rider %d, gear set %d, owner %d)", row, e.rider_id, e.rider_gear_set, e.rider_owner); note(b);
        }
        if (e.vehicle_type >= 0) set_int("Player1VehicleTypeId", e.vehicle_type);
        frame();
    }
    GuestCall setup; setup.fn = 0x59a970; setup.ecx = ui; call(setup); note("SetupEvent"); frame();
    GuestCall start; start.fn = 0x59ade0; start.ecx = ui; call(start); note("StartEvent");
    GuestCall goto_loading; goto_loading.fn = 0x740ed0; goto_loading.eax = guest_string("GotoLoadingPage"); call(goto_loading); note("GotoLoadingPage");
    const uint32_t videos_at_loading = d->bink_file_opens;
    // loading (states 3..11); state 6 waits for a confirmation key (Enter: MapKey_UI_Accept1)
    int last = -1; uint32_t at6 = 0; uint8_t keys[256] = {};
    // REFLEX_ACCEPT_AT=<n> (w-gridpair, test hook): Enter from the n-th state-6 frame instead of the 30th (1 = the player accepts the
    // loading page at once). The UI world's emitters draw the game LCG every frame of the wait, so this moves the grid pass's draw
    // (the local StartZone); the paired product ride (core/tools/gridpair.sh) uses 1. Default unchanged.
    static const int accept_at = std::getenv("REFLEX_ACCEPT_AT") ? std::atoi(std::getenv("REFLEX_ACCEPT_AT")) : 30;
    while (frames < max_frames) {
        int st = state();
        if (st != last) { std::snprintf(b, sizeof b, "state %d at frame %u (%.1f s virtual)", st, frames, d->now_ / 1e7); note(b); last = st; }
        if (st == 6) { at6++; int off = int(at6) - accept_at; keys[0x1c] = (off >= 0 && off / 120 < 3 && off % 120 < 2) ? 0x80 : 0; set_keys(keys); }
        else if (at6) { keys[0x1c] = 0; set_keys(keys); }
        if (at6 && st != 6 && st >= 7) {}
        uint32_t g = read32(0xd728d8);
        if (st == 11 || (at6 && st != 6 && read32(g + 0x20c8))) {
            frame(60);
            // Baton scheduling: loading costs no virtual time, so the loading page's logo video (BinkOpen of Videos\Nordic_Logo_wSound.bik,
            // in every event after GotoLoadingPage; the exact boot opens it during state 4, long before the capture) can still be due
            // here (Supercross: ~20 frames later). The world runtime has no Bink: wait for that open (at most 10 s), so the world image
            // is past it as the exact boot's is.
            if (d->cfg.baton && !d->cfg.baton_slices)
                for (uint32_t k = 0; d->bink_file_opens == videos_at_loading && k < 600 && frames < max_frames; k++) frame();
            std::snprintf(b, sizeof b, "world running: state %d at frame %u (%.1f s virtual)", state(), frames, d->now_ / 1e7); note(b); return true;
        }
        frame();
    }
    note("event did not start in time"); return false;
}

namespace {
void dxt1_rgb_u8r(const uint8_t* blocks, uint32_t pitch, uint32_t w, uint32_t h, std::vector<uint8_t>& out, bool dxt5 = false) {
    // 565 -> 8 bit by replication; thirds/halves (2a+b+1)/3, (a+2b+1)/3, (a+b+1)/2 (terrain_deform_reference 'u8r')
    // dxt5 (WP-V2: TRX_Lynchburg / TRX_LynchburgBW keep their SoftnessMask as DXT5): 16-byte blocks, the colour block at +8, always the
    // four-colour interpolation (DXT2..5 have no three-colour mode); the alpha block is not part of the mask's RGB.
    out.assign(size_t(w) * h * 3, 0);
    auto expand = [](uint16_t c, uint32_t* o) { uint32_t r = c >> 11, g = (c >> 5) & 63, b = c & 31; o[0] = (r << 3) | (r >> 2); o[1] = (g << 2) | (g >> 4); o[2] = (b << 3) | (b >> 2); };
    for (uint32_t by = 0; by < (h + 3) / 4; by++)
        for (uint32_t bx = 0; bx < (w + 3) / 4; bx++) {
            const uint8_t* b = blocks + by * pitch + (dxt5 ? bx * 16 + 8 : bx * 8);
            uint16_t c0, c1; uint32_t idx; std::memcpy(&c0, b, 2); std::memcpy(&c1, b + 2, 2); std::memcpy(&idx, b + 4, 4);
            uint32_t pal[4][3]; expand(c0, pal[0]); expand(c1, pal[1]);
            for (int i = 0; i < 3; i++) {
                if (c0 > c1 || dxt5) { pal[2][i] = (2 * pal[0][i] + pal[1][i] + 1) / 3; pal[3][i] = (pal[0][i] + 2 * pal[1][i] + 1) / 3; }
                else { pal[2][i] = (pal[0][i] + pal[1][i] + 1) / 2; pal[3][i] = 0; }
            }
            for (uint32_t k = 0; k < 16; k++) {
                uint32_t x = bx * 4 + (k & 3), y = by * 4 + (k >> 2); if (x >= w || y >= h) continue;
                uint32_t* c = pal[(idx >> (2 * k)) & 3]; uint8_t* o = &out[(size_t(y) * w + x) * 3];
                o[0] = uint8_t(c[0]); o[1] = uint8_t(c[1]); o[2] = uint8_t(c[2]);
            }
        }
}
}  // namespace

void Boot::write_world(const std::string& world_path, const std::string& deform_path, const std::string& model_path, std::string* info) {
    Impl& s = *d; Machine& m = s.m;
    if (s.threads[0]->ctx.pc != ENGINE_FRAME) throw VmError("write_world: the main thread is not between frames");
    char b[256];
    auto note = [&](const char* x) { s.say("WORLD %s", x); if (info) { *info += x; *info += "\n"; } };
    s.load(*s.threads[0]);   // the main thread's TEB in place (CRT slots)
    // ---- thread table (version 2): game thread first, then the workers
    uint32_t tls_n = s.exe_tls_end - s.exe_tls_start + s.exe_tls_zero;
    auto ptd_of = [&](Thread& t) -> uint32_t {
        for (uint32_t i = 0; i < 64; i++) {
            uint32_t v; std::memcpy(&v, t.teb.data() + 0xe10 + 4 * i, 4);
            if (v >= 0x10000 && m.mem.mapped(v) && read32(v) == t.id && read32(v + 0x68)) return v;
        }
        return 0;
    };
    auto tls_block = [&](Thread& t) -> uint32_t { uint32_t arr; std::memcpy(&arr, t.teb.data() + 0x2c, 4); return arr ? read32(arr + 4 * s.exe_tls_index) : 0; };
    std::vector<uint32_t> table; std::vector<uint8_t> tls_bytes;
    uint32_t main_ptd = 0, anim_cw = 0, anim_mx = 0;
    for (auto& tp : s.threads) {
        Thread& t = *tp; if (t.state == Thread::Done) continue;
        uint32_t block = tls_block(t), ptd = ptd_of(t);
        bool main = &t == s.threads[0].get();
        if (main) main_ptd = ptd;
        if (!main && t.start == 0x61cdb0 && !anim_cw) { anim_cw = 0x10000u | t.ctx.cw; anim_mx = t.ctx.mxcsr; }
        table.insert(table.end(), {t.id, main ? 0u : t.start, t.ctx.cw, t.ctx.mxcsr, block, block ? tls_n : 0u, main ? 0u : ptd, 0u});
        if (block) { size_t at = tls_bytes.size(); tls_bytes.resize(at + tls_n); m.mem.read_block(block, tls_bytes.data() + at, tls_n); }
    }
    std::vector<uint8_t> rec(table.size() * 4); std::memcpy(rec.data(), table.data(), rec.size()); rec.insert(rec.end(), tls_bytes.begin(), tls_bytes.end());
    uint32_t tcount = uint32_t(table.size() / 8), tversion = 2;
    std::vector<uint8_t> tpages(16 + rec.size()); std::memcpy(tpages.data(), &tcount, 4); std::memcpy(tpages.data() + 4, &tversion, 4); std::memcpy(tpages.data() + 16, rec.data(), rec.size());
    tpages.resize((tpages.size() + 0xfff) & ~size_t(0xfff), 0);
    // ---- the boot's imports (sentinel -> name) at 0x20000
    std::vector<uint8_t> spages(16 + 128 * s.sentinel_of.size(), 0);
    { uint32_t magic = 0x544e5342u /*BSNT*/, n = uint32_t(s.sentinel_of.size()); std::memcpy(spages.data(), &magic, 4); std::memcpy(spages.data() + 4, &n, 4);
      size_t k = 0; for (auto& [name, addr] : s.sentinel_of) { std::memcpy(spages.data() + 16 + 128 * k, &addr, 4); std::memcpy(spages.data() + 20 + 128 * k, name.c_str(), std::min<size_t>(name.size(), 123)); k++; } }
    spages.resize((spages.size() + 0xfff) & ~size_t(0xfff), 0);
    // ---- pages
    std::map<uint32_t, const uint8_t*> pages;   // nullptr: zero page
    for (uint32_t pg = 0x10; pg < 0x100000; pg++) {
        uint32_t a = pg << 12;
        if (a >= 0x40000000u && a < 0x60000000u) continue;              // device memory
        if (a == TEB || a >= 0xe0000000u) continue;
        if (a >= 0x20000 && a < 0x20000 + spages.size()) continue;
        if (!m.mem.mapped(a)) continue;
        pages[a] = m.mem.peek(a);
    }
    for (size_t i = 0; i < tpages.size(); i += 4096) pages[0x1000 + uint32_t(i)] = tpages.data() + i;
    for (size_t i = 0; i < spages.size(); i += 4096) pages[0x20000 + uint32_t(i)] = spages.data() + i;
    // ---- the bike simulation and the riding rider (pack_world.py: the referenced instance)
    auto scan = [&](uint32_t vtable, const std::function<bool(uint32_t)>& accept) {
        std::vector<uint32_t> found;
        for (auto& [a, p] : pages) { if (!p || a < 0x400000) continue; for (uint32_t off = 0; off < 4096; off += 4) { uint32_t v; std::memcpy(&v, p + off, 4); if (v == vtable && accept(a + off)) found.push_back(a + off); } }
        std::vector<uint32_t> live;
        for (uint32_t c : found) {
            bool ref = false;
            for (auto& [a, p] : pages) { if (!p || a < 0x400000) continue; for (uint32_t off = 0; off < 4096 && !ref; off += 4) { uint32_t v; std::memcpy(&v, p + off, 4); ref = v == c; } if (ref) break; }
            if (ref) live.push_back(c);
        }
        return live;
    };
    auto sims = scan(SIM_MOTOCROSS_VTABLE, [&](uint32_t o) { return read32(o + 0xec) == SIM_MOTOCROSS_BODIES; });
    { auto atv = scan(SIM_ATV_VTABLE, [&](uint32_t o) { return read32(o + 0xec) == SIM_ATV_BODIES; }); sims.insert(sims.end(), atv.begin(), atv.end()); }   // RavenSimulationATV (atv.h)
    for (auto& c : VEHICLE_CLASSES) if (c.factory > 1) { auto v = scan(c.sim_vtable, [&](uint32_t o) { return read32(o + 0xec) == c.bodies; }); sims.insert(sims.end(), v.begin(), v.end()); }   // trucks, buggies, UTVs (vehicles.h)
    auto riders = scan(0x958724, [](uint32_t) { return true; });
    std::snprintf(b, sizeof b, "bike simulations %zu, riding riders %zu", sims.size(), riders.size()); note(b);
    if (sims.size() > 1 || riders.size() > 1) {   // AI riders: keep the local player's (P = G+0x7460: vehicle +0x1ac, rider +0x1b4)
        uint32_t G = read32(0xa946b8), P = G + 0x7460, V = read32(P + 0x1ac), R = read32(P + 0x1b4);
        auto owned = [&](uint32_t c, uint32_t base, uint32_t span) { for (uint32_t a = base; a < base + span; a += 4) if (m.mem.mapped(a) && read32(a) == c) return true; return false; };
        auto pick = [&](std::vector<uint32_t>& list, const char* what) {
            std::vector<uint32_t> mine;
            for (uint32_t c : list) if ((V && owned(c, V, 0x4000)) || (R && owned(c, R, 0x2000)) || owned(c, P, 0x12a0)) mine.push_back(c);
            if (mine.size() != 1) { std::snprintf(b, sizeof b, "write_world: the local player's %s is ambiguous (%zu of %zu)", what, mine.size(), list.size()); throw VmError(b); }
            list = mine;
        };
        if (sims.size() > 1) pick(sims, "bike simulation");
        if (riders.size() > 1) pick(riders, "riding rider");
    }
    if (sims.size() != 1 || riders.size() != 1) throw VmError(std::string("write_world: ") + b);
    // ---- header
    uint32_t engine = 0xd728d0, crt = 0, cpp = 0;
    for (auto& md : s.mods) { if (md.name == "msvcr90.dll") crt = md.base; if (md.name == "msvcp90.dll") cpp = md.base; }
    uint32_t header[16] = {0x4c4c5546u, 1, s.physics_context, read32(engine + 0x18), uint32_t(s.frames), s.threads[0]->ctx.cw, s.threads[0]->ctx.mxcsr, crt, cpp, main_ptd,
                           riders[0], anim_cw, anim_mx, engine, 0x454d5246u, 0x544f4f42u /*BOOT*/};
    std::snprintf(b, sizeof b, "context %08x dt %08x ptd %08x threads %u sim %08x rider %08x pages %zu", header[2], header[3], main_ptd, tcount, sims[0], riders[0], pages.size()); note(b);
    if (!s.physics_context) note("warning: the outer physics update has not run yet (context 0; whole-frame images do not need it)");
    if (!main_ptd) {
        std::string slots; for (uint32_t i = 0; i < 16; i++) { uint32_t v; std::memcpy(&v, s.threads[0]->teb.data() + 0xe10 + 4 * i, 4); char w[40]; std::snprintf(w, sizeof w, " %u:%08x(%08x)", i, v, v >= 0x10000 && m.mem.mapped(v) ? read32(v) : 0); slots += w; }
        throw VmError("write_world: missing game-thread _tiddata or physics context; main tid " + std::to_string(s.threads[0]->id) + " slots" + slots);
    }
    // ---- RXWI
    std::vector<uint8_t> out(80 + 12 * pages.size()); std::vector<uint8_t> blob;
    uint32_t hdr[4] = {0x49575852u, 1, uint32_t(pages.size()), sims[0]}; std::memcpy(out.data(), hdr, 16); std::memcpy(out.data() + 16, header, 64);
    size_t k = 0, base = out.size(), zero = 0;
    std::vector<uint8_t> c(compressBound(4096));
    for (auto& [a, p] : pages) {
        uint32_t e[3] = {a, 0, 0};
        bool z = !p; if (p) { z = true; for (int i = 0; i < 4096 && z; i++) z = p[i] == 0; }
        if (!z) {
            uLongf n = uLongf(c.size()); compress2(c.data(), &n, p, 4096, 6);
            e[1] = uint32_t(base + blob.size());
            if (n >= 4096) { blob.insert(blob.end(), p, p + 4096); e[2] = 4096; } else { blob.insert(blob.end(), c.data(), c.data() + n); e[2] = uint32_t(n); }
        } else zero++;
        std::memcpy(out.data() + 80 + 12 * k++, e, 12);
    }
    out.insert(out.end(), blob.begin(), blob.end());
    { FILE* f = std::fopen(world_path.c_str(), "wb"); if (!f) throw VmError("cannot write " + world_path); std::fwrite(out.data(), 1, out.size(), f); std::fclose(f); }
    std::snprintf(b, sizeof b, "world image %zu bytes (%zu pages, %zu zero)", out.size(), pages.size(), zero); note(b);
    // ---- terrain: deformation surface and model
    uint32_t registry[3]; for (int i = 0; i < 3; i++) registry[i] = read32(0x9e30d4 + 4 * i);
    uint32_t scene = read32(0xa95844), identity = read32(scene + 0x1c + read32(scene + 4) * 4), terrain = 0;
    for (uint32_t i = 0; i < registry[2] && i < 2; i++) if (registry[i] && read32(registry[i] + 0x44a8) == identity) terrain = registry[i];
    if (!terrain) throw VmError("write_world: selected terrain missing");
    Impl::TexInfo surf;
    if (!s.texture_info || !s.texture_info(read32(terrain + 0x240), surf) || surf.format != 114 || (surf.width != 2048 && surf.width != 1024 && surf.width != 512) || surf.height != surf.width) {
        std::snprintf(b, sizeof b, "write_world: deformation surface missing (terrain %08x resource %08x format %x %ux%u)", terrain, read32(terrain + 0x240), surf.format, surf.width, surf.height);
        throw VmError(b);
    }
    { uint32_t n = surf.width; std::vector<uint8_t> g(size_t(n) * n * 4); for (uint32_t y = 0; y < n; y++) m.mem.read_block(surf.addr[0] + y * surf.pitch[0], g.data() + size_t(y) * n * 4, n * 4);
      FILE* f = std::fopen(deform_path.c_str(), "wb"); if (!f) throw VmError("cannot write " + deform_path); std::fwrite(g.data(), 1, g.size(), f); std::fclose(f); }
    uint32_t P = terrain + 0x230;
    Impl::TexInfo mask, brush;
    // a game texture object -> its D3D texture: the pointer itself or one/two levels of fields
    auto find_texture = [&](uint32_t p, Impl::TexInfo& t, const std::function<bool(const Impl::TexInfo&)>& ok) -> bool {
        std::function<bool(uint32_t, int)> rec = [&](uint32_t v, int depth) -> bool {
            if (v < 0x10000 || !m.mem.mapped(v)) return false;
            if (s.texture_info(v, t) && ok(t)) return true;
            if (depth == 0) return false;
            for (uint32_t off = 0; off < 0x80; off += 4) if (m.mem.mapped(v + off) && rec(read32(v + off), depth - 1)) return true;
            return false;
        };
        return rec(p, 3);   // TextureAssetHandle -> TextureAsset (+0x44) -> platform data (+0x10) -> IDirect3DTexture9 (+0)
    };
    if (!find_texture(read32(P + 0x28), mask, [](const Impl::TexInfo& t) { return (t.format == 0x31545844u || t.format == 0x35545844u) && t.width >= 256; }))   // DXT1 (DXT5: TRX_Lynchburg*)
        { std::string w; for (uint32_t off = 0; off < 0x40; off += 4) { char x[24]; std::snprintf(x, sizeof x, " +%x:%08x", off, read32(P + off)); w += x; }
          uint32_t q = read32(P + 0x28); w += "\n  [P+28]:";
          for (uint32_t off = 0; off < 0x60; off += 4) { uint32_t v = read32(q + off); Impl::TexInfo ti; char x[48]; std::snprintf(x, sizeof x, " +%x:%08x%s", off, v, s.texture_info(v, ti) ? "(TEX)" : ""); w += x; }
          uint32_t q2 = read32(q + 0x44); w += "\n  [+44]:";
          for (uint32_t off = 0; off < 0x80; off += 4) { uint32_t v = read32(q2 + off); Impl::TexInfo ti; char x[48]; std::snprintf(x, sizeof x, " +%x:%08x%s", off, v, s.texture_info(v, ti) ? "(TEX)" : ""); w += x; }
          for (uint32_t f : {0x0cu, 0x10u}) { uint32_t q3 = read32(q2 + f); char x[48]; std::snprintf(x, sizeof x, "\n  [+44][+%x]=%08x:", f, q3); w += x;
            for (uint32_t off = 0; off < 0x60; off += 4) { uint32_t v = read32(q3 + off); Impl::TexInfo ti; std::snprintf(x, sizeof x, " +%x:%08x%s", off, v, s.texture_info(v, ti) ? "(TEX)" : ""); w += x; } }
          throw VmError("write_world: SoftnessMask texture missing; P=" + std::to_string(P) + w); }
    // the material's texture handles (TextureAssetHandle, vtable 009719c8) within three levels
    bool brush_found = false;
    std::function<void(uint32_t, int)> handles = [&](uint32_t v, int depth) {
        if (brush_found || v < 0x10000 || !m.mem.mapped(v)) return;
        if (read32(v) == 0x9719c8u) {
            std::string name; for (uint32_t i = 0; i < 60; i++) { char ch = char(read32(v + 4 + i) & 0xff); if (!ch) break; name += ch; }
            Impl::TexInfo t; bool ok = find_texture(v, t, [](const Impl::TexInfo& x) { return x.format == 0x31545844u && x.addr.size() > 1; });
            std::snprintf(b, sizeof b, "material texture %s: %s %ux%u", name.c_str(), ok ? "DXT1" : "-", t.width, t.height); note(b);
            if (ok && name.find("Deform") != std::string::npos) { brush = t; brush_found = true; }
            return;
        }
        if (depth == 0) return;
        for (uint32_t off = 0; off < 0x100 && !brush_found; off += 4) if (m.mem.mapped(v + off)) handles(read32(v + off), depth - 1);
    };
    handles(read32(P + 0x4264), 3);
    if (!brush_found)
        throw VmError("write_world: deformation brush texture missing");
    std::snprintf(b, sizeof b, "terrain %08x mask %ux%u brush %ux%u (%zu mips)", terrain, mask.width, mask.height, brush.width, brush.height, brush.addr.size()); note(b);
    std::vector<uint8_t> rx = {'R', 'X', 'D', 'F'};
    auto u32 = [&](uint32_t v) { for (int i = 0; i < 4; i++) rx.push_back(uint8_t(v >> (8 * i))); };
    u32(1); u32(mask.width); u32(mask.height); u32(uint32_t(brush.addr.size()));
    for (size_t i = 0; i < brush.addr.size(); i++) { u32(brush.w[i]); u32(brush.h[i]); }
    auto add = [&](const Impl::TexInfo& t, size_t lvl) {
        uint32_t rows = std::max(1u, (t.h[lvl] + 3) / 4); std::vector<uint8_t> blocks(size_t(rows) * t.pitch[lvl]); m.mem.read_block(t.addr[lvl], blocks.data(), blocks.size());
        std::vector<uint8_t> rgb; dxt1_rgb_u8r(blocks.data(), t.pitch[lvl], t.w[lvl], t.h[lvl], rgb, t.format == 0x35545844u); rx.insert(rx.end(), rgb.begin(), rgb.end());
    };
    add(mask, 0);
    for (size_t i = 0; i < brush.addr.size(); i++) add(brush, i);
    { FILE* f = std::fopen(model_path.c_str(), "wb"); if (!f) throw VmError("cannot write " + model_path); std::fwrite(rx.data(), 1, rx.size(), f); std::fclose(f); }
    std::snprintf(b, sizeof b, "deformation model %zu bytes", rx.size()); note(b);
}

bool Boot::enable_deformation(const std::string& model_path) {
    struct St { DeformModel model; std::vector<float> grid; uint32_t n = 0, terrain = 0; bool due = false, init = false; uint64_t renders = 0, texels = 0; };
    auto st = std::make_shared<St>();
    if (!st->model.load(model_path) || !st->model.loaded()) return false;
    Impl* s = d;
    Machine& m = s->m;
    auto rd = [](Machine& mm, uint32_t a) { return mm.mem.mapped(a) ? uint32_t(mm.mem.read(a, 4)) : 0u; };
    // HFTerrainSurface::vf14 0x7c4fa0 (fastcall ECX = surface): the active scene's terrain marks a render due (a pending level
    // reset +0x44b8 clears both targets inside vf14 through the device: the grid is read again at the render)
    reflex_port::runtime_native(m, 0x7c4fa0, [st, rd](Machine& mm) -> uint32_t {
        const uint32_t surface = mm.r[1], scene = rd(mm, 0xa95844);
        if (scene && rd(mm, surface + 0x44a8) == rd(mm, scene + 0x1c + 4 * rd(mm, scene + 4))) {
            if (st->terrain != surface || rd(mm, surface + 0x44b8)) st->init = false;
            st->terrain = surface; st->due = true;
        }
        const reflex_vm::Native* p = reflex_port::installed(mm, 0x7c4fa0);
        return p ? (*p)(mm) : reflex_vm::RUN_ORIGINAL;
    });
    // TerrainTracks_FlushPending 0x7cbe30: vf14's first call renders this frame's stamp queue (queue[!q], the readback before it already
    // flipped q) into both R32F render targets (P+8 / P+0xc level 0), from the previous grid (runtime.cpp render_deform)
    reflex_port::runtime_native(m, 0x7cbe30, [st, rd, s](Machine& mm) -> uint32_t {
        if (st->due && st->terrain) {
            st->due = false;
            const uint32_t T = st->terrain, P = T + 0x230;
            Impl::TexInfo a, b;
            if (s->texture_info && s->texture_info(rd(mm, P + 8), a) && s->texture_info(rd(mm, P + 0xc), b) && a.format == 114 && b.format == 114 &&
                a.width == a.height && b.width == a.width) {
                const uint32_t n = a.width;
                if (!st->init || st->n != n) {
                    st->n = n; st->grid.assign(size_t(n) * n, 0.0f);
                    for (uint32_t y = 0; y < n; y++) mm.mem.read_block(a.addr[0] + y * a.pitch[0], &st->grid[size_t(y) * n], n * 4);
                    st->init = true;
                }
                const uint32_t q = rd(mm, P + 0x18), qi = q ? 0u : 1u;
                uint32_t quads = rd(mm, P + 0x230 + qi * 0x208), tris = rd(mm, P + 0x440 + qi * 4);
                quads = std::min({quads, tris / 2, 64u});
                const uint32_t eb = rd(mm, T + 0x44d0); float extent; std::memcpy(&extent, &eb, 4);
                const float rcp = 1.0f / (extent * 0.5f);
                std::vector<StampVertex> verts;
                for (uint32_t k = 0; k < quads; k++) {
                    float off[2]; mm.mem.read_bytes(P + 0x30 + qi * 0x208 + 8 * k, off, 8);
                    for (uint32_t j = 0; j < 6; j++) {
                        float v[5]; mm.mem.read_bytes(P + 0x448 + qi * 0x1e04 + (k * 6 + j) * 20, v, 20);
                        verts.push_back({off[0] * rcp + v[0], off[1] * rcp + v[1], v[2], v[3], v[4]});
                    }
                }
                const DeformRect r = apply_stamps(st->grid, int(n), verts, st->model);
                st->renders++;
                if (!r.empty()) {
                    for (int z = r.z0; z < r.z1; z++)
                        for (const Impl::TexInfo* t : {&a, &b})
                            mm.mem.write_block(t->addr[0] + uint32_t(z) * t->pitch[0] + uint32_t(r.x0) * 4u, &st->grid[size_t(z) * n + size_t(r.x0)], size_t(r.x1 - r.x0) * 4);
                    st->texels += uint64_t(r.z1 - r.z0) * uint64_t(r.x1 - r.x0);
                }
            }
        }
        const reflex_vm::Native* p = reflex_port::installed(mm, 0x7cbe30);
        return p ? (*p)(mm) : reflex_vm::RUN_ORIGINAL;
    });
    return true;
}

void Boot::run_frames(uint32_t frames, uint64_t dt_100ns) { d->frame_dt = dt_100ns ? dt_100ns : d->fixed_dt; d->schedule(frames); }

bool Boot::run_until(const std::function<bool(Boot&)>& pred, uint32_t max_frames, uint64_t dt_100ns) {
    d->frame_dt = dt_100ns ? dt_100ns : d->fixed_dt;
    for (uint32_t i = 0; i < max_frames; i++) { if (pred(*this)) return true; d->schedule(1); }
    return pred(*this);
}
}  // namespace reflex_vm

// ======================================================================== canonical heap dump (native/game notes/loading.md)
namespace reflex_vm {
void Boot::write_canonical(const std::string& path, bool device, std::string* info) {
    Impl& s = *d; Machine& m = s.m;
    if (s.threads[0]->ctx.pc != ENGINE_FRAME) throw VmError("write_canonical: the main thread is not between frames");
    auto u32 = [&](uint32_t a) -> uint32_t { const uint8_t* p = m.mem.peek(a & ~0xfffu); if (!p || (a & 0xfff) > 0xffc) return m.mem.mapped(a) && p ? uint32_t(m.mem.read(a, 4)) : 0u; uint32_t v; std::memcpy(&v, p + (a & 0xfff), 4); return v; };
    auto mix = [](uint64_t h, uint64_t v) { h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2); h *= 0xff51afd7ed558ccdull; return h ^ (h >> 33); };
    // ---- nodes: heap blocks (game and device heaps) and the VirtualAlloc regions that are neither heap arenas nor stacks
    struct Node { uint32_t a, size; uint8_t kind; uint32_t phase = 0, off = 0; uint64_t name = 0, parent = 0; int8_t leaf = -1; };   // kind 1 heap, 2 device, 3 region
    std::vector<Node> nodes; nodes.reserve(s.blocks.size() + s.regions.size());
    for (auto& [a, b] : s.blocks) nodes.push_back({a, b.size, uint8_t(b.heap == DEVICE_HEAP ? 2 : 1)});
    struct Range { uint32_t lo, hi; };
    std::vector<Range> stacks, modules;
    for (auto& t : s.threads) stacks.push_back({t->stack_lo, t->stack_hi});
    for (auto& md : s.mods) modules.push_back({md.base, md.base + md.size});
    auto in = [](const std::vector<Range>& rs, uint32_t a) { for (size_t i = 0; i < rs.size(); i++) if (a >= rs[i].lo && a < rs[i].hi) return int(i); return -1; };
    for (auto& [b, sz] : s.regions) if (!s.arenas.count(b) && in(stacks, b) < 0 && in(modules, b) < 0) nodes.push_back({b, sz, 3});
    std::sort(nodes.begin(), nodes.end(), [](const Node& x, const Node& y) { return x.a < y.a; });
    std::vector<Range> arena_ranges; for (uint32_t b : s.arenas) arena_ranges.push_back({b, b + s.regions[b]});
    // a word's meaning: 0 plain value, 1 a node (index, offset; one past the end counts when no node starts there), 2 dangling (inside a
    // heap arena, no live block), 3 a thread stack (thread index, offset below the stack top)
    struct Ref { uint8_t kind; uint32_t index, off; };
    auto lookup = [&](uint32_t v) -> Ref {
        if (v < 0x10000) return {0, 0, 0};
        auto it = std::upper_bound(nodes.begin(), nodes.end(), v, [](uint32_t x, const Node& n) { return x < n.a; });
        // text is not a pointer: four printable bytes count as a pointer only to a node's start; into device memory (textures,
        // buffers: 0x40000000.., the bytes '@'..'_') only aligned
        bool text = true; for (int i = 0; i < 4; i++) { uint8_t c = uint8_t(v >> (8 * i)); text = text && c >= 0x20 && c < 0x7f; }
        if (it != nodes.begin()) {
            const Node& n = *(it - 1);
            if (v < n.a + n.size || (v == n.a + n.size && n.size)) {
                if (v != n.a && (text || (n.kind == 2 && (v & 3)))) return {0, 0, 0};
                return {1, uint32_t(it - 1 - nodes.begin()), v - n.a};
            }
        }
        if (text) return {0, 0, 0};
        if (in(arena_ranges, v) >= 0) return {2, 0, 0};
        int st = in(stacks, v); if (st >= 0) return {3, uint32_t(st), stacks[size_t(st)].hi - v};
        return {0, 0, 0};
    };
    // ---- names: breadth first from the roots, a node named after its first path (parent name, field offset)
    std::vector<uint32_t> order; order.reserve(nodes.size());
    bool roots_weak = false;
    auto discover = [&](uint32_t v, uint64_t parent, uint32_t off, uint64_t name, uint32_t phase) {
        Ref r = lookup(v); if (r.kind != 1 || (parent == 0 && r.off && !roots_weak)) return;
        Node& n = nodes[r.index]; if (n.phase) return;
        n.phase = phase; n.parent = parent; n.off = off; n.name = name; order.push_back(r.index);
    };
    // Data blocks (file buffers, packed tables, textures) are leaves: their words are values, never pointers. Device blocks always;
    // a block without a vtable of 64 KiB or more when under a quarter of its words that hit a node hit a node's start (random data
    // hits nodes anywhere; real pointers mostly point at starts).
    auto leaf = [&](Node& n) -> bool {
        if (n.leaf >= 0) return n.leaf;
        bool l = n.kind == 2;
        if (!l && n.size >= 0x10000 && !(u32(n.a) >= 0x401000 && u32(n.a) < 0x916000)) {
            uint32_t hits = 0, starts = 0;
            for (uint32_t o = 0; o + 4 <= n.size; o += 4) { uint32_t v = u32(n.a + o); if (v < 0x10000) continue; Ref r = lookup(v); if (r.kind == 1) { hits++; starts += r.off == 0; } }
            l = hits >= 64 && 4 * starts < hits;
        }
        n.leaf = l; return l;
    };
    // names travel along pointers to a node's start first (strong), then along interior pointers (weak)
    auto scan = [&](size_t from, uint32_t phase, bool weak) {
        for (size_t q = from; q < order.size(); q++) {
            Node& n = nodes[order[q]];
            if (leaf(n)) continue;
            uint64_t nm = n.name;
            for (uint32_t o = 0; o + 4 <= n.size; o += 4) {
                uint32_t v = u32(n.a + o); if (v < 0x10000) continue;
                Ref r = lookup(v); if (r.kind != 1 || (r.off && !weak)) continue;
                discover(v, nm, o, mix(nm, o), phase);
            }
        }
    };
    // roots 1: the modules' writable sections (by address), then each thread's TLS (TEB+0x2c static TLS array, TlsSetValue slots)
    auto roots1 = [&]() {
    for (auto& md : s.mods) {
        uint32_t nt = md.base + u32(md.base + 0x3c), nsec = u32(nt + 4) >> 16, opt = u32(nt + 20) & 0xffff, sec = nt + 24 + opt;
        for (uint32_t i = 0; i < nsec; i++) {
            uint32_t e = sec + 40 * i, vsz = u32(e + 8), va = u32(e + 12), ch = u32(e + 36);
            if (!(ch & 0x80000000u)) continue;
            for (uint32_t a = md.base + va; a + 4 <= md.base + va + vsz; a += 4) { uint32_t v = u32(a); if (v >= 0x10000) discover(v, 0, a, mix(0x47, a), 1); }
        }
    }
    for (size_t k = 0; k < s.threads.size(); k++) {
        Thread& t = *s.threads[k]; if (t.state == Thread::Done) continue;
        auto slot = [&](uint32_t off) { uint32_t v; std::memcpy(&v, t.teb.data() + off, 4); if (v >= 0x10000) discover(v, 0, off, mix(mix(0x54, k), off), 1); };
        slot(0x2c); for (uint32_t i = 0; i < 64; i++) slot(0xe10 + 4 * i);
    }
    };
    roots1(); scan(0, 1, false);
    roots_weak = true; roots1(); scan(0, 1, true);
    size_t phase1 = order.size();
    // roots 2: the threads' live stacks (esp .. top)
    for (size_t k = 0; k < s.threads.size(); k++) {
        Thread& t = *s.threads[k]; if (t.state == Thread::Done) continue;
        uint32_t esp = t.ctx.r[4]; if (esp < t.stack_lo || esp > t.stack_hi) continue;
        for (uint32_t a = esp; a + 4 <= t.stack_hi; a += 4) { uint32_t v = u32(a); if (v >= 0x10000) discover(v, 0, t.stack_hi - a, mix(mix(0x53, k), t.stack_hi - a), 2); }   // (roots_weak: interior too)
    }
    scan(phase1, 2, false); scan(phase1, 2, true);
    size_t reached = order.size();
    for (uint32_t i = 0; i < nodes.size(); i++) if (!nodes[i].phase) { nodes[i].phase = 3; order.push_back(i); }
    // ---- class names (MSVC RTTI: vtable[-1] = CompleteObjectLocator, +0xc TypeDescriptor, +8 the decorated name)
    std::map<uint32_t, std::string> classes;
    auto class_of = [&](const Node& n, uint32_t& vt) -> std::string {
        vt = n.size >= 4 ? u32(n.a) : 0;
        if (vt < 0x401000 || vt >= 0x916000 || in(modules, vt) < 0) { vt = 0; return ""; }
        auto it = classes.find(vt); if (it != classes.end()) return it->second;
        std::string name; uint32_t col = u32(vt - 4);
        if (col >= 0x401000 && col < 0x916000 && u32(col) == 0) {
            uint32_t td = u32(col + 12);
            if (td >= 0x401000 && td < 0xe00000) for (uint32_t i = 0; i < 120; i++) { char c = char(u32(td + 8 + i) & 0xff); if (!c) break; name += c; }
        }
        if (name.rfind(".?AV", 0) == 0 || name.rfind(".?AU", 0) == 0) name = name.substr(4, name.size() >= 6 ? name.size() - 6 : 0); else name.clear();
        classes[vt] = name; return name;
    };
    // ---- write: "RXCN" v1, then per node: phase, kind, name, parent, field offset, size, vtable, class, canonical hash, the plain
    // words (pointer words as 0) and the pointer words (word index, kind, target name or thread, offset)
    gzFile f = gzopen(path.c_str(), "wb1"); if (!f) throw VmError("cannot write " + path);
    auto put = [&](const void* p, size_t n) { gzwrite(f, p, unsigned(n)); };
    auto put32 = [&](uint32_t v) { put(&v, 4); }; auto put64 = [&](uint64_t v) { put(&v, 8); };
    put32(0x4e435852u); put32(1); put32(uint32_t(order.size())); put32(uint32_t(phase1)); put32(uint32_t(reached));
    std::vector<uint32_t> words; struct P { uint32_t w; uint8_t kind; uint64_t target; uint32_t off; }; std::vector<P> ptrs;
    uint64_t bytes = 0, nptr = 0;
    for (uint32_t idx : order) {
        const Node& n = nodes[idx];
        uint32_t vt = 0; std::string cls = class_of(n, vt);
        bool content = n.kind != 2 || device;
        const bool raw = leaf(const_cast<Node&>(n));
        uint32_t nw = content ? (n.size + 3) / 4 : 0;
        words.assign(nw, 0); ptrs.clear();
        uint64_t h = mix(n.size, vt);
        for (uint32_t w = 0; w < nw; w++) {
            uint32_t v = u32(n.a + 4 * w);
            if (4 * w + 4 > n.size) v &= (1u << (8 * (n.size & 3))) - 1;   // the block's last partial word
            Ref r = 4 * w + 4 <= n.size && !raw ? lookup(v) : Ref{0, 0, 0};
            if (r.kind == 0) { words[w] = v; h = mix(h, v); continue; }
            uint64_t target = r.kind == 1 ? nodes[r.index].name : r.kind == 3 ? r.index : 0;
            ptrs.push_back({w, r.kind, target, r.off}); h = mix(mix(mix(h, 0x50000000ull + r.kind), target), r.off);
        }
        if (!content) { uint64_t c = 1469598103934665603ull; for (uint32_t o = 0; o < n.size; o += 4) c = (c ^ u32(n.a + o)) * 1099511628211ull; h = mix(h, c); }
        uint8_t hdr[2] = {uint8_t(n.phase), n.kind}; put(hdr, 2);
        put64(n.name); put64(n.parent); put32(n.off); put32(n.size); put32(vt);
        uint16_t cl = uint16_t(std::min<size_t>(cls.size(), 200)); put(&cl, 2); put(cls.data(), cl);
        put64(h); put32(nw); put(words.data(), 4 * nw); put32(uint32_t(ptrs.size()));
        for (auto& p : ptrs) { put32(p.w); put(&p.kind, 1); put64(p.target); put32(p.off); }
        bytes += n.size; nptr += ptrs.size();
    }
    gzclose(f);
    char b[200]; std::snprintf(b, sizeof b, "canonical dump: %zu nodes (%zu from globals / TLS, %zu more from stacks, %zu unreached), %llu bytes, %llu pointers",
                               order.size(), phase1, reached - phase1, order.size() - reached, (unsigned long long)bytes, (unsigned long long)nptr);
    s.say("WORLD %s", b); if (info) { *info += b; *info += "\n"; }
}
}  // namespace reflex_vm

// ---------------------------------------------------------------------------------------------------- the client's boots (boot.h)
namespace reflex_vm {
namespace {
std::mutex client_ports_lock;
bool client_ports_set = false;
std::string client_ports_filter = "all";
int client_ports_mode = 1;
}  // namespace
void set_client_boot_ports(const std::string& filter, int mode) {
    std::lock_guard<std::mutex> g(client_ports_lock);
    client_ports_set = true; client_ports_filter = filter.empty() ? "all" : filter; client_ports_mode = mode < 0 ? 0 : mode > 2 ? 2 : mode;
}
void client_boot_ports(std::string& filter, int& mode) {
    std::lock_guard<std::mutex> g(client_ports_lock);
    filter = client_ports_filter; mode = client_ports_mode;
    if (client_ports_set) return;
    if (const char* e = std::getenv("REFLEX_NATIVE_GAME"); e && *e >= '0' && *e <= '2' && !e[1]) mode = *e - '0';
    if (const char* f = std::getenv("REFLEX_NATIVE_PORTS"); f && *f) filter = f;
}
void client_boot_config(BootConfig& c) {
    const char* sc = std::getenv("REFLEX_BOOT_SCHED");
    c.baton = !(sc && std::string(sc) == "exact"); c.baton_slices = false;
}
namespace {
struct CoverageBuffers { std::vector<uint32_t> exe, dll; };
std::mutex coverage_lock;
std::map<const Machine*, CoverageBuffers> coverage_buffers;
std::map<std::string, int> coverage_tags;
}  // namespace
bool client_coverage_enabled() { const char* p = std::getenv("REFLEX_CLIENT_COVERAGE"); return p && *p; }
void client_coverage_attach(Machine& m) {
    if (!client_coverage_enabled()) return;
    std::lock_guard<std::mutex> g(coverage_lock);
    auto& c = coverage_buffers[&m];
    if (c.exe.empty()) { c.exe.assign(0x600000, 0); c.dll.assign(0x200000, 0); }
    m.pc_counts = c.exe.data(); m.dll_counts = c.dll.data();
}
void client_coverage_detach(Machine& m) { std::lock_guard<std::mutex> g(coverage_lock); if (coverage_buffers.count(&m)) { m.pc_counts = nullptr; m.dll_counts = nullptr; } }
void client_coverage_write(Machine& m, const std::string& tag) {
    if (!client_coverage_enabled()) return;
    std::lock_guard<std::mutex> g(coverage_lock);
    auto it = coverage_buffers.find(&m); if (it == coverage_buffers.end()) return;
    int n = ++coverage_tags[tag];
    std::string f = std::string(std::getenv("REFLEX_CLIENT_COVERAGE")) + "-" + tag + (n > 1 ? "-" + std::to_string(n) : "") + ".bin";
    if (FILE* cf = std::fopen(f.c_str(), "wb")) {
        uint32_t hdr[4] = {0x56435852u, 1, 0, 0}; std::fwrite(hdr, 4, 4, cf); std::fwrite(it->second.exe.data(), 4, it->second.exe.size(), cf); std::fclose(cf);
    }
    if (FILE* df = std::fopen((f + ".dll").c_str(), "wb")) { uint32_t one = 1; std::fwrite(&one, 4, 1, df); std::fwrite(it->second.dll.data(), 4, it->second.dll.size(), df); std::fclose(df); }
    if (FILE* ef = std::fopen((f + ".dll.tsv").c_str(), "w")) { for (auto& [name, a] : m.crt_exports) std::fprintf(ef, "%08x\t%s\n", a, name.c_str()); std::fclose(ef); }
    std::fill(it->second.exe.begin(), it->second.exe.end(), 0u); std::fill(it->second.dll.begin(), it->second.dll.end(), 0u);
}
void client_coverage_forget(Machine& m) {
    std::lock_guard<std::mutex> g(coverage_lock);
    if (coverage_buffers.erase(&m)) { m.pc_counts = nullptr; m.dll_counts = nullptr; }
}
int install_client_boot_ports(Boot& b, const BootConfig& c) {
    client_coverage_attach(b.machine());   // REFLEX_CLIENT_COVERAGE (the whole boot)
    if (!c.baton || c.baton_slices) return 0;   // natives need baton scheduling (a slice or a block inside one breaks the exact scheduler)
    std::string filter; int mode = 0; client_boot_ports(filter, mode);
    if (mode == 0) return 0;
    return reflex_port::install(b.machine(), mode == 2 ? reflex_port::Mode::Verify : reflex_port::Mode::On, filter);
}
}  // namespace reflex_vm
