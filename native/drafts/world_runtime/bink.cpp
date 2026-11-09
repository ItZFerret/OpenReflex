// Bink playback with the install's binkw32.dll on its own Machine (bink.h, _re/notes/bink.md, names _re/names/81_bink.tsv).
// The DLL imports only system DLLs (kernel32, user32, gdi32, winmm); the stand-ins below are the subset its playback path
// reaches, single-threaded:
// - CreateThread fails (returns NULL). BinkOpen then registers neither the background sound service (0x1800e667) nor the
//   threaded IO (0x1800e6d2) and the DLL services both from the calling thread: BinkWait runs the IO idle read
//   (BINK+0x168 at 0x1800fd1d) and the sound fill (0x1800cf80) runs from BinkDoFrame / BinkWait / BinkNextFrame, as it
//   does on the platforms without threads.
// - Mutexes / events / semaphores are single-threaded (waits succeed at once).
// - Time: QueryPerformanceCounter / GetTickCount / timeGetTime read the caller's clock (Player::update's ms).
// - CPU features: the interpreter's CPUID (MMX, SSE, SSE2). Decoding into frame buffers (BINKNOFRAMEBUFFERS) uses no MMX
//   (the BINK* blitter sections 0x18004000..0x18009fff are BinkCopyToBuffer's), the audio decoder takes its SSE2 path
//   (0x180302ac bit 8: 0x18014680 / 0x180146e0 / 0x18014850).
#include "bink.h"
#include "pe.h"
#include "vm.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <map>
#include <sys/stat.h>
#include <unistd.h>

namespace reflex_bink {
using namespace reflex_vm;

namespace {
constexpr uint32_t SENT = 0xec000000;          // import sentinels (16 bytes apart)
constexpr uint32_t CODE = 0xed000000;          // host-implemented guest-callable code (the BINKSND interface)
constexpr uint32_t TEB = 0x7ffd0000, PEB = 0x7ffdf000;
constexpr uint32_t STACK_LO = 0x00100000, STACK_HI = 0x00200000;
constexpr uint32_t HEAP_LO = 0x20000000, HEAP_HI = 0x70000000;
constexpr uint32_t BINK_HEAP = 0x00140000;     // HeapCreate's handle (one allocator)
// BINK (HBINK) fields (bink.h of the SDK generation; verified on this DLL's code, _re/notes/bink.md)
constexpr uint32_t B_WIDTH = 0x00, B_HEIGHT = 0x04, B_FRAMES = 0x08, B_FRAMENUM = 0x0c, B_FPS = 0x14, B_FPSDIV = 0x18;
// +0x114 / +0x118: sound tracks: count, BINKSND array (0x180 bytes each); +0x38: BinkGetRects' rects (x, y, w, h), +0xb8 their count
// BINKSND (0x180 bytes; the open function fills +0x20.. and the callbacks, the core the ring buffer +0x00..+0x1c / +0x3c..+0x54)
constexpr uint32_t S_FREQ = 0x20, S_BITS = 0x24, S_CHANS = 0x28, S_BESTSIZE = 0x2c, S_ONOFF = 0x34, S_LATENCY = 0x38;
constexpr uint32_t S_VOL = 0x58, S_PAN = 0x5c, S_PAUSED = 0x64, S_CHUNK = 0x7c, S_CB = 0x158;
// BINKFRAMEBUFFERS: TotalFrames, YABufferWidth/Height, cRcBBufferWidth/Height, FrameNum, then BINKFRAMEPLANESET[] of four
// BINKPLANE {Allocate, Buffer, BufferPitch} (Y, cR, cB, A) = 0x30 bytes each.
constexpr uint32_t FB_SETS = 0x18, FB_SET = 0x30, FB_SIZE = 0x200;
}  // namespace

struct Player::Impl {
    Machine m;
    Info info;
    std::string error;
    std::map<std::string, uint32_t> exports;
    uint32_t next_sentinel = SENT, next_code = CODE;
    double now_ms = 0;
    // heap: first fit over free ranges (address -> size), used blocks (address -> size)
    std::map<uint32_t, uint32_t> free_, used_;
    uint32_t heap_top = HEAP_LO;
    // files
    struct File { int fd = -1; uint64_t pos = 0; };
    std::map<uint32_t, File> files;
    uint32_t next_handle = 0x100;
    // playback
    uint32_t bink = 0, fb = 0, staging = 0, staging_size = 0;
    bool due = false;
    uint32_t shown = 0;
    std::vector<uint8_t> planes[4];
    std::vector<int16_t> pcm; size_t pcm_read = 0;
    uint32_t sound_bits = 16, vol = 32768;

    ~Impl() { for (auto& [h, f] : files) if (f.fd >= 0) ::close(f.fd); }
    void say(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
        if (!std::getenv("REFLEX_BINK_LOG")) return;
        va_list a; va_start(a, fmt); std::vfprintf(stderr, fmt, a); va_end(a); std::fputc('\n', stderr);
    }
    // ---------------------------------------------------------------- memory
    uint32_t alloc(uint32_t size, bool zero) {
        uint32_t big = size >= 0x10000, align = big ? 0x1000 : 16;
        uint32_t need = std::max<uint32_t>(16, (size + align - 1) & ~(align - 1));
        uint32_t a = 0;
        for (auto it = free_.begin(); it != free_.end(); ++it) {
            uint32_t b = (it->first + align - 1) & ~(align - 1), pad = b - it->first;
            if (it->second < pad + need) continue;
            uint32_t base = it->first, sz = it->second; free_.erase(it);
            if (pad) free_[base] = pad;
            if (sz - pad - need) free_[b + need] = sz - pad - need;
            a = b; break;
        }
        if (!a) {
            a = (heap_top + align - 1) & ~(align - 1);
            if (uint64_t(a) + need > HEAP_HI) throw VmError("bink: guest heap exhausted");
            if (a > heap_top) free_[heap_top] = a - heap_top;
            heap_top = a + need;
        }
        for (uint32_t p = a & ~0xfffu; p < a + need; p += 4096) if (!m.mem.mapped(p)) m.mem.back(p);
        if (zero) m.mem.write_block(a, nullptr, need);
        used_[a] = need;
        return a;
    }
    bool release(uint32_t a) {
        auto it = used_.find(a); if (it == used_.end()) return false;
        uint32_t sz = it->second; used_.erase(it);
        if (sz >= 0x10000) m.mem.unmap(a, sz);          // page-aligned large blocks give their host memory back
        auto nx = free_.lower_bound(a);
        if (nx != free_.end() && nx->first == a + sz) { sz += nx->second; free_.erase(nx); }
        auto pv = free_.lower_bound(a);
        if (pv != free_.begin()) { --pv; if (pv->first + pv->second == a) { pv->second += sz; return true; } }
        free_[a] = sz;
        return true;
    }
    std::string cstr(uint32_t p, size_t max = 1024) {
        std::string s; if (!p) return s;
        for (size_t i = 0; i < max; i++) { char c = char(m.mem.read(p + uint32_t(i), 1)); if (!c) break; s += c; }
        return s;
    }
    void put_cstr(uint32_t p, uint32_t cap, const std::string& s) {
        if (!cap) return;
        std::string t = s.substr(0, cap - 1); m.mem.write_block(p, t.c_str(), t.size() + 1);
    }
    uint32_t native(Native fn) { uint32_t a = next_code; next_code += 4; m.natives[a] = std::move(fn); return a; }
    uint64_t ticks() const { return uint64_t(now_ms * 10000.0); }   // 10 MHz performance counter
    // a stdcall / cdecl call into the DLL from the host (fresh stack each time; no host call nests)
    uint32_t call(const char* fn, std::vector<uint32_t> args) {
        auto e = exports.find(fn); if (e == exports.end()) throw VmError(std::string("bink: no export ") + fn);
        m.r[4] = STACK_HI - 0x100;
        return m.call_function(e->second, args.data(), int(args.size()));
    }
    void load(const std::string& dll);
    void install();
};

// ======================================================================== image and stand-ins
void Player::Impl::load(const std::string& dll) {
    Pe pe(slurp(dll));
    uint32_t base = pe.image_base;                          // 0x18000000, its preferred base (no relocation needed)
    m.modules.push_back({base, base + pe.size, pe.image});
    m.mem.map(base, pe.image.data(), std::min<uint32_t>(pe.size, 0x1000), true);
    for (auto& s : pe.sections) {
        uint32_t a = base + s.va, vs = (std::max(s.vsize, s.rawsize) + 0xfff) & ~0xfffu;
        m.mem.map(a, pe.image.data() + s.va, std::min<uint32_t>(vs, pe.size - s.va), !(s.flags & 0x80000000u));
    }
    exports = pe.exports();
    for (auto& imp : pe.imports()) {
        std::string full = imp.dll + "!" + imp.name;
        uint32_t s = next_sentinel; next_sentinel += 16; m.import_names[s] = full;
        m.mem.set_readonly(imp.iat, 4, false); m.mem.write(imp.iat, 4, s);
    }
    m.mem.set_loader([](uint32_t, uint8_t*) {});            // backed pages start zeroed
    for (uint32_t a = STACK_LO; a < STACK_HI; a += 4096) m.mem.back(a);
    uint8_t teb[4096] = {}; auto put = [&](uint32_t off, uint32_t v) { std::memcpy(teb + off, &v, 4); };
    put(0x00, 0xffffffffu); put(0x04, STACK_HI); put(0x08, STACK_LO); put(0x18, TEB); put(0x20, 0x4d0); put(0x24, 0x4d4); put(0x30, PEB);
    m.mem.map(TEB, teb, 4096); uint8_t peb[4096] = {}; m.mem.map(PEB, peb, 4096);
    install();
    uint32_t entry = base + rd<uint32_t>(pe.image, pe.nt + 24 + 16);
    m.r[4] = STACK_HI - 0x100;
    uint32_t args[3] = {base, 1, 0};                        // DllMain(hinst, DLL_PROCESS_ATTACH, 0)
    if (!m.call_function(entry, args, 3)) throw VmError("bink: DllMain failed");
}

void Player::Impl::install() {
    Impl* s = this;
    auto imp = [&](const std::string& n, Native f) { m.imports[n] = std::move(f); };
    const std::string K = "kernel32.dll!", W = "winmm.dll!";
    // heap
    imp(K + "HeapCreate", [](Machine& m) { return m.ret(12, BINK_HEAP); });
    imp(K + "HeapAlloc", [s](Machine& m) { return m.ret(12, s->alloc(m.arg(2), (m.arg(1) & 8) != 0)); });
    imp(K + "HeapFree", [s](Machine& m) { return m.ret(12, s->release(m.arg(2)) ? 1 : 0); });
    imp(K + "LocalAlloc", [s](Machine& m) { return m.ret(8, s->alloc(m.arg(1), (m.arg(0) & 0x40) != 0)); });
    // process / module
    imp(K + "DisableThreadLibraryCalls", [](Machine& m) { return m.ret(4, 1); });
    imp(K + "GetCurrentProcess", [](Machine& m) { return m.ret(0, 0xffffffffu); });
    imp(K + "GetCurrentProcessId", [](Machine& m) { return m.ret(0, 0x4d0); });
    imp(K + "TerminateProcess", [](Machine& m) -> uint32_t { throw VmError("bink: TerminateProcess " + std::to_string(m.arg(1))); });
    imp(K + "RaiseException", [](Machine& m) -> uint32_t { char b[64]; std::snprintf(b, sizeof b, "bink: RaiseException %08x", m.arg(0)); throw VmError(b); });
    imp(K + "OutputDebugStringA", [s](Machine& m) { s->say("bink debug: %s", s->cstr(m.arg(0)).c_str()); return m.ret(4); });
    imp(K + "GetLastError", [](Machine& m) { return m.ret(0, 0); });
    imp(K + "SetErrorMode", [](Machine& m) { return m.ret(4, 0); });
    imp(K + "GetEnvironmentVariableA", [](Machine& m) { return m.ret(12, 0); });   // RAD_USE_TGT unset: QueryPerformanceCounter
    imp(K + "GetWindowsDirectoryA", [s](Machine& m) { s->put_cstr(m.arg(0), m.arg(1), "C:\\WINDOWS"); return m.ret(8, 10); });
    imp(K + "GetSystemDirectoryA", [s](Machine& m) { s->put_cstr(m.arg(0), m.arg(1), "C:\\WINDOWS\\system32"); return m.ret(8, 19); });
    imp(K + "GetModuleFileNameA", [s](Machine& m) { s->put_cstr(m.arg(1), m.arg(2), "C:\\Reflex\\binkw32.dll"); return m.ret(12, 21); });
    imp(K + "GetModuleHandleA", [](Machine& m) { return m.ret(4, m.arg(0) ? 0 : 0x18000000); });
    imp(K + "LoadLibraryA", [s](Machine& m) { s->say("bink LoadLibraryA %s -> NULL", s->cstr(m.arg(0)).c_str()); return m.ret(4, 0); });
    imp(K + "FreeLibrary", [](Machine& m) { return m.ret(4, 1); });
    imp(K + "GetProcAddress", [](Machine& m) { return m.ret(8, 0); });
    imp(K + "GetSystemInfo", [](Machine& m) {
        uint32_t v[9] = {0, 4096, 0x10000, 0x7ffeffff, 1, 1, 586, 0x10000, 6u | (0x0f06u << 16)};   // one processor
        m.mem.write_block(m.arg(0), v, 36); return m.ret(4);
    });
    imp("user32.dll!GetCursorPos", [](Machine& m) { m.mem.write(m.arg(0), 8, 0); return m.ret(4, 1); });   // the security cookie's entropy (0x18001922)
    // threads and synchronisation: no threads (see the header comment)
    imp(K + "CreateThread", [s](Machine& m) { s->say("bink CreateThread %08x -> NULL", m.arg(2)); return m.ret(24, 0); });
    imp(K + "ResumeThread", [](Machine& m) { return m.ret(4, 0xffffffffu); });
    imp(K + "SetThreadPriority", [](Machine& m) { return m.ret(8, 1); });
    imp(K + "SetThreadAffinityMask", [](Machine& m) { return m.ret(8, 1); });
    imp(K + "CreateEventA", [s](Machine& m) { return m.ret(16, s->next_handle += 4); });
    imp(K + "CreateMutexA", [s](Machine& m) { return m.ret(12, s->next_handle += 4); });
    imp(K + "CreateSemaphoreA", [s](Machine& m) { return m.ret(16, s->next_handle += 4); });
    imp(K + "SetEvent", [](Machine& m) { return m.ret(4, 1); });
    imp(K + "ReleaseMutex", [](Machine& m) { return m.ret(4, 1); });
    imp(K + "ReleaseSemaphore", [](Machine& m) { if (m.arg(2)) m.mem.write(m.arg(2), 4, 0); return m.ret(12, 1); });
    imp(K + "WaitForSingleObject", [](Machine& m) { return m.ret(8, 0); });
    imp(K + "WaitForMultipleObjects", [](Machine& m) { return m.ret(16, 0); });
    imp(K + "InterlockedExchange", [](Machine& m) { uint32_t p = m.arg(0), old = uint32_t(m.mem.read(p, 4)); m.mem.write(p, 4, m.arg(1)); return m.ret(8, old); });
    imp(K + "Sleep", [](Machine& m) { return m.ret(4); });
    // time: the caller's clock
    imp(K + "QueryPerformanceFrequency", [](Machine& m) { m.mem.write(m.arg(0), 8, 10000000ull); return m.ret(4, 1); });
    imp(K + "QueryPerformanceCounter", [s](Machine& m) { m.mem.write(m.arg(0), 8, s->ticks()); return m.ret(4, 1); });
    imp(K + "GetTickCount", [s](Machine& m) { return m.ret(0, uint32_t(s->ticks() / 10000)); });
    imp(W + "timeGetTime", [s](Machine& m) { return m.ret(0, uint32_t(s->ticks() / 10000)); });
    imp(W + "timeBeginPeriod", [](Machine& m) { return m.ret(4, 0); });
    imp(W + "timeEndPeriod", [](Machine& m) { return m.ret(4, 0); });
    // files: the host path given to BinkOpen, read-only
    imp(K + "CreateFileA", [s](Machine& m) {
        std::string path = s->cstr(m.arg(0));
        int fd = ::open(path.c_str(), O_RDONLY);
        s->say("bink CreateFileA %s -> %d", path.c_str(), fd);
        if (fd < 0) return m.ret(28, 0xffffffffu);
        uint32_t h = s->next_handle += 4; s->files[h] = {fd, 0}; return m.ret(28, h);
    });
    imp(K + "ReadFile", [s](Machine& m) {
        auto it = s->files.find(m.arg(0)); uint32_t buf = m.arg(1), n = m.arg(2), out = m.arg(3);
        if (m.arg(4)) throw VmError("bink: overlapped ReadFile");
        if (it == s->files.end()) { if (out) m.mem.write(out, 4, 0); return m.ret(20, 0); }
        std::vector<uint8_t> tmp(n); ssize_t k = pread(it->second.fd, tmp.data(), n, off_t(it->second.pos)); if (k < 0) k = 0;
        m.mem.write_block(buf, tmp.data(), size_t(k)); it->second.pos += uint64_t(k);
        if (out) m.mem.write(out, 4, uint32_t(k));
        return m.ret(20, 1);
    });
    imp(K + "SetFilePointer", [s](Machine& m) {
        auto it = s->files.find(m.arg(0)); int64_t lo = int32_t(m.arg(1)); uint32_t hip = m.arg(2), method = m.arg(3);
        if (it == s->files.end()) return m.ret(16, 0xffffffffu);
        int64_t dist = hip ? (int64_t(int32_t(m.mem.read(hip, 4))) << 32) | (lo & 0xffffffff) : lo;
        struct stat st; fstat(it->second.fd, &st);
        int64_t base = method == 0 ? 0 : method == 1 ? int64_t(it->second.pos) : int64_t(st.st_size), np = base + dist;
        if (np < 0) return m.ret(16, 0xffffffffu);
        it->second.pos = uint64_t(np); if (hip) m.mem.write(hip, 4, uint32_t(it->second.pos >> 32));
        return m.ret(16, uint32_t(it->second.pos));
    });
    imp(K + "CloseHandle", [s](Machine& m) {
        auto it = s->files.find(m.arg(0)); if (it != s->files.end()) { ::close(it->second.fd); s->files.erase(it); }
        return m.ret(4, 1);
    });
}

// ======================================================================== the sound system (BINKSND)
// The DLL's own glues (waveOut 0x180118d0, DirectSound 0x180128?0) are the model: open(bs, freq, bits, chans, flags, bink)
// clears the 0x180-byte BINKSND, fills format and volume fields and eight stdcall callbacks:
//   +0x158 Ready(bs) -> a buffer can be filled     +0x15c Lock(bs, &addr, &len)      +0x160 Unlock(bs, filled)
//   +0x164 Volume(bs, vol)  +0x168 Pan(bs, pan)    +0x16c Pause(bs, on)  +0x170 SetOnOff(bs, on)  +0x174 Close(bs)
// The core's fill loop (0x1800cf80) copies decoded PCM from its ring (+0x00 write, +0x18 read, +0x0c/+0x10 bounds) while
// Ready says yes and more than BestSize (+0x2c) is buffered (all of it near the end). The chunk is the waveOut glue's
// (0x18011280): 50 ms, 32-byte aligned.
static uint32_t install_sound(Player::Impl* s) {
    uint32_t cb[8];
    cb[0] = s->native([](Machine& m) { uint32_t bs = m.arg(0);
        return m.ret(4, (!m.mem.read(bs + S_PAUSED, 4) && m.mem.read(bs + S_ONOFF, 4)) ? 1 : 0); });
    cb[1] = s->native([s](Machine& m) { uint32_t bs = m.arg(0);
        m.mem.write(m.arg(1), 4, s->staging); m.mem.write(m.arg(2), 4, uint32_t(m.mem.read(bs + S_CHUNK, 4))); return m.ret(12, 1); });
    cb[2] = s->native([s](Machine& m) { uint32_t n = m.arg(1);
        if (n > s->staging_size) throw VmError("bink: sound unlock larger than the lock");
        if (s->sound_bits == 16) { size_t k = s->pcm.size(); s->pcm.resize(k + n / 2); m.mem.read_block(s->staging, s->pcm.data() + k, n & ~1u); }
        else { std::vector<uint8_t> b(n); m.mem.read_block(s->staging, b.data(), n); for (uint8_t v : b) s->pcm.push_back(int16_t((int(v) - 128) << 8)); }
        return m.ret(8, 1); });
    cb[3] = s->native([s](Machine& m) { uint32_t v = m.arg(1); s->vol = v; m.mem.write(m.arg(0) + S_VOL, 4, v); return m.ret(8, 0); });
    cb[4] = s->native([](Machine& m) { m.mem.write(m.arg(0) + S_PAN, 4, m.arg(1)); return m.ret(8, 0); });
    cb[5] = s->native([](Machine& m) { uint32_t bs = m.arg(0); m.mem.write(bs + S_PAUSED, 4, m.arg(1) ? 1 : 0); return m.ret(8, m.arg(1) ? 1 : 0); });
    cb[6] = s->native([](Machine& m) { uint32_t bs = m.arg(0), on = m.arg(1);
        if (on <= 1) m.mem.write(bs + S_ONOFF, 4, on);
        return m.ret(8, uint32_t(m.mem.read(bs + S_ONOFF, 4))); });
    cb[7] = s->native([](Machine& m) { return m.ret(4, 0); });
    uint32_t open = s->native([s, cb](Machine& m) {
        uint32_t bs = m.arg(0), freq = m.arg(1), bits = m.arg(2), chans = m.arg(3);
        m.mem.write_block(bs, nullptr, 0x180);
        m.mem.write(bs + S_FREQ, 4, freq); m.mem.write(bs + S_BITS, 4, bits); m.mem.write(bs + S_CHANS, 4, chans);
        uint32_t bps = freq * bits * chans / 8, chunk = ((bps * 400 / 1000) / 8 + 31) & ~31u;   // 0x18011280: 50 ms
        m.mem.write(bs + S_CHUNK, 4, chunk);
        m.mem.write(bs + S_BESTSIZE, 4, bits == 16 ? chunk : 2 * chunk);
        m.mem.write(bs + S_ONOFF, 4, 1); m.mem.write(bs + S_LATENCY, 4, 0);
        m.mem.write(bs + S_VOL, 4, 0x7fff); m.mem.write(bs + S_PAN, 4, 0x7fff);
        for (int i = 0; i < 8; i++) m.mem.write(bs + S_CB + 4 * i, 4, cb[i]);
        if (!s->staging || s->staging_size < chunk) { s->staging = s->alloc(chunk + 64, true); s->staging_size = chunk; }
        s->info.audio_rate = freq; s->info.audio_channels = chans; s->info.audio_bits = bits; s->sound_bits = bits;
        s->say("bink sound open %u Hz, %u bits, %u channels, flags %08x, chunk %u", freq, bits, chans, m.arg(4), chunk);
        return m.ret(24, 1);
    });
    // BinkSetSoundSystem(open_system, param) calls open_system(param) for the BINKSNDOPEN (as BinkOpenDirectSound 0x18012940)
    return s->native([open](Machine& m) { return m.ret(4, open); });
}

// ======================================================================== Player
Player::Player(Impl* d) : d(d) {}
Player::~Player() {
    if (d && d->bink) { try { d->call("_BinkClose@4", {d->bink}); } catch (...) {} }
    delete d;
}
const Info& Player::info() const { return d->info; }
uint32_t Player::frame() const { return d->shown; }
const std::vector<uint8_t>& Player::plane(int i) const { return d->planes[i & 3]; }
uint64_t Player::steps() const { return d->m.steps; }
size_t Player::audio_queued() const { return d->pcm.size() - d->pcm_read; }
size_t Player::take_audio(int16_t* out, size_t max) {
    size_t n = std::min(max, d->pcm.size() - d->pcm_read);
    if (out && n) std::memcpy(out, d->pcm.data() + d->pcm_read, n * 2);
    d->pcm_read += n;
    if (d->pcm_read == d->pcm.size()) { d->pcm.clear(); d->pcm_read = 0; }
    else if (d->pcm_read > (1u << 20)) { d->pcm.erase(d->pcm.begin(), d->pcm.begin() + long(d->pcm_read)); d->pcm_read = 0; }
    return n;
}
uint32_t Player::volume() const { return d->vol; }

std::unique_ptr<Player> Player::open(const std::string& dll, const std::string& file, bool preload, bool jit, std::string* error) {
    auto* s = new Player::Impl;
    std::unique_ptr<Player> p(new Player(s));
    try {
        if (jit) s->m.enable_jit(true);
        s->load(dll);
        uint32_t sys = install_sound(s);
        s->call("_BinkSetSoundSystem@8", {sys, 0});          // PlatformVideoManager ctor 005957b0 (its DirectSound object: NULL)
        uint32_t path = s->alloc(uint32_t(file.size()) + 1, true); s->m.mem.write_block(path, file.c_str(), file.size() + 1);
        uint32_t flags = 0x100400u | (preload ? 0x2000u : 0);  // XenonBinkVideoManager::vf1 00595bca
        s->bink = s->call("_BinkOpen@8", {path, flags});
        if (!s->bink) {
            uint32_t e = s->call("_BinkGetError@0", {});
            throw VmError("BinkOpen failed: " + s->cstr(e));
        }
        Memory& mm = s->m.mem;
        Info& in = s->info;
        in.width = uint32_t(mm.read(s->bink + B_WIDTH, 4)); in.height = uint32_t(mm.read(s->bink + B_HEIGHT, 4));
        in.frames = uint32_t(mm.read(s->bink + B_FRAMES, 4));
        in.fps_num = uint32_t(mm.read(s->bink + B_FPS, 4)); in.fps_den = uint32_t(mm.read(s->bink + B_FPSDIV, 4));
        // frame buffers (XenonBinkVideo::vf9 005950b0 / 005944d0): the planes the DLL asks to be allocated
        s->fb = s->alloc(FB_SIZE, true);
        s->call("_BinkGetFrameBuffersInfo@8", {s->bink, s->fb});
        uint32_t sets = uint32_t(mm.read(s->fb, 4));
        in.ywidth = uint32_t(mm.read(s->fb + 4, 4)); in.yheight = uint32_t(mm.read(s->fb + 8, 4));
        in.cwidth = uint32_t(mm.read(s->fb + 0xc, 4)); in.cheight = uint32_t(mm.read(s->fb + 0x10, 4));
        if (sets < 1 || sets > 8 || !in.ywidth || !in.yheight) throw VmError("bink: unexpected frame buffer info");
        for (uint32_t k = 0; k < sets; k++)
            for (int pl = 0; pl < 4; pl++) {
                uint32_t at = s->fb + FB_SETS + k * FB_SET + uint32_t(pl) * 12;
                if (!mm.read(at, 4)) continue;
                uint32_t w = pl == 0 || pl == 3 ? in.ywidth : in.cwidth, h = pl == 0 || pl == 3 ? in.yheight : in.cheight;
                uint32_t pitch = (w + 15) & ~15u;
                mm.write(at + 4, 4, s->alloc(pitch * h, true)); mm.write(at + 8, 4, pitch);
                if (pl == 3) in.has_alpha = 1;
            }
        s->call("_BinkRegisterFrameBuffers@8", {s->bink, s->fb});
        s->planes[0].assign(size_t(in.ywidth) * in.yheight, 0); s->planes[1].assign(size_t(in.cwidth) * in.cheight, 0);
        s->planes[2].assign(size_t(in.cwidth) * in.cheight, 0); s->planes[3].assign(in.has_alpha ? size_t(in.ywidth) * in.yheight : 0, 0);
        s->say("bink open %s: %ux%u, %u frames at %u/%u, %u frame sets (Y %ux%u, c %ux%u), alpha %u, sound %u Hz x%u",
               file.c_str(), in.width, in.height, in.frames, in.fps_num, in.fps_den, sets, in.ywidth, in.yheight, in.cwidth, in.cheight,
               in.has_alpha, in.audio_rate, in.audio_channels);
        return p;
    } catch (const std::exception& e) {
        if (error) *error = e.what();
        return nullptr;
    }
}

int Player::update(double ms, bool* done) {
    Impl& s = *d; Memory& mm = s.m.mem;
    s.now_ms = ms;
    int fresh = 0;
    // 00595690: the due frame
    if (s.due) {
        s.call("_BinkRegisterFrameBuffers@8", {s.bink, s.fb});
        s.call("_BinkDoFrame@4", {s.bink});
        while (s.call("_BinkShouldSkip@4", {s.bink})) { s.call("_BinkNextFrame@4", {s.bink}); s.call("_BinkDoFrame@4", {s.bink}); }
        s.shown = uint32_t(mm.read(s.bink + B_FRAMENUM, 4));
        // 005948c0: the decoded set's planes are what the game's textures receive (its LockRect of that set without
        // D3DLOCK_NO_DIRTY_UPDATE dirties the whole level, so UpdateTexture copies all of it; the BinkGetRects rects add nothing)
        s.call("_BinkGetRects@8", {s.bink, 0});
        uint32_t set = s.fb + FB_SETS + uint32_t(mm.read(s.fb + 0x14, 4)) * FB_SET;
        for (int pl = 0; pl < 4; pl++) {
            if (s.planes[pl].empty()) continue;
            uint32_t at = set + uint32_t(pl) * 12, buf = uint32_t(mm.read(at + 4, 4)), pitch = uint32_t(mm.read(at + 8, 4));
            uint32_t w = pl == 0 || pl == 3 ? s.info.ywidth : s.info.cwidth, h = pl == 0 || pl == 3 ? s.info.yheight : s.info.cheight;
            for (uint32_t y = 0; y < h; y++) mm.read_block(buf + y * pitch, s.planes[pl].data() + size_t(y) * w, w);
        }
        s.call("_BinkNextFrame@4", {s.bink});
        s.due = false; fresh = 1;
    }
    // XenonBinkVideo::vf8 00595613: BinkWait == 0 -> the next frame is due
    if (!s.call("_BinkWait@4", {s.bink})) s.due = true;
    if (done) *done = s.shown >= s.info.frames;
    return fresh;
}

void Player::go_to(uint32_t frame) { d->call("_BinkGoto@12", {d->bink, frame, 0}); }
// XenonBinkVideo::vf1 00595210: (volume,) vf4 -> BinkGoto(1, 0), BinkSetSoundOnOff(1), the frame due at once (+0xac = 1; 00595690 runs
// within the call: the next update decodes before any BinkWait).
void Player::play() { go_to(1); set_sound(true); d->due = true; }
void Player::pause(bool on) { d->call("_BinkPause@8", {d->bink, on ? 1u : 0u}); }
void Player::set_sound(bool on) { d->call("_BinkSetSoundOnOff@8", {d->bink, on ? 1u : 0u}); }
void Player::set_volume(uint32_t v) { d->call("_BinkSetVolume@12", {d->bink, 0, v}); }

}  // namespace reflex_bink

// ======================================================================== C API (reflex_world.h)
#include "reflex_world.h"
#include <cstddef>
namespace {
struct BinkHandle { std::unique_ptr<reflex_bink::Player> p; std::string error; bool failed = false; };
std::string bink_open_error;
}
extern "C" {
void* reflex_bink_open(const char* dll, const char* file, int32_t preload, ReflexBinkInfo* info) {
    if (!dll || !file) { bink_open_error = "no path"; return nullptr; }
    std::string err;
    auto p = reflex_bink::Player::open(dll, file, preload != 0, !std::getenv("REFLEX_NO_JIT"), &err);
    if (!p) { bink_open_error = err; return nullptr; }
    static_assert(sizeof(ReflexBinkInfo) == sizeof(reflex_bink::Info) && offsetof(ReflexBinkInfo, ywidth) == offsetof(reflex_bink::Info, ywidth)
                  && offsetof(ReflexBinkInfo, cheight) == offsetof(reflex_bink::Info, cheight), "info layout");
    if (info) std::memcpy(info, &p->info(), sizeof *info);
    auto* h = new BinkHandle; h->p = std::move(p); bink_open_error.clear();
    return h;
}
int reflex_bink_update(void* player, double ms, int32_t* frame, int32_t* done) {
    auto* h = static_cast<BinkHandle*>(player);
    if (!h || h->failed) return -1;
    try {
        bool d = false; int r = h->p->update(ms, &d);
        if (frame) *frame = int32_t(h->p->frame());
        if (done) *done = d ? 1 : 0;
        return r;
    } catch (const std::exception& e) { h->error = e.what(); h->failed = true; return -1; }
}
int reflex_bink_planes(void* player, uint8_t* y, uint8_t* cr, uint8_t* cb, uint8_t* a) {
    auto* h = static_cast<BinkHandle*>(player);
    if (!h || h->failed) return -1;
    uint8_t* out[4] = {y, cr, cb, a};
    for (int i = 0; i < 4; i++) { auto& pl = h->p->plane(i); if (out[i] && !pl.empty()) std::memcpy(out[i], pl.data(), pl.size()); }
    return 0;
}
int32_t reflex_bink_audio(void* player, int16_t* out, int32_t max) {
    auto* h = static_cast<BinkHandle*>(player);
    if (!h || h->failed) return 0;
    if (max <= 0) return int32_t(h->p->audio_queued());
    return int32_t(h->p->take_audio(out, size_t(max)));
}
int reflex_bink_control(void* player, int32_t what, int32_t value) {
    auto* h = static_cast<BinkHandle*>(player);
    if (!h || h->failed) return -1;
    try {
        switch (what) {
            case 1: h->p->go_to(1); return 0;
            case 5: h->p->play(); return 0;
            case 2: h->p->pause(value != 0); return 0;
            case 3: h->p->set_sound(value != 0); return 0;
            case 4: h->p->set_volume(uint32_t(value)); return 0;
        }
        return 1;
    } catch (const std::exception& e) { h->error = e.what(); h->failed = true; return -1; }
}
const char* reflex_bink_error(void* player) {
    auto* h = static_cast<BinkHandle*>(player);
    return h ? h->error.c_str() : bink_open_error.c_str();
}
void reflex_bink_close(void* player) { delete static_cast<BinkHandle*>(player); }
}  // extern "C"
