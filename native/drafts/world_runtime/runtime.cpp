// Reflex original-physics runtime: loads the original executable and VC90 CRT,
// restores a world image, and runs original frames (XenonInputManager::vf43 then
// the outer physics update 00769b80). Stand-ins mirror world_update_model.py.
#include "runtime.h"
#include "clock_pin.h"
#include "port.h"
#include "deform.h"
#include "vm.h"
#include "pe.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <zlib.h>

namespace reflex_vm {

static constexpr uint32_t SENTINEL = 0xfe000000, STACK_TOP = 0x7ff00000, FS_BASE = 0x7ffd0000;
static constexpr uint32_t LOCK = 0xfd000000, UNLOCK = 0xfd000010, TABLE = 0xfd001000, DEFORM_BITS = 0x60000000;
static constexpr uint32_t CLOCK_MS = 0x00100000;
static constexpr float NEUTRAL_DEFORM = 128.0f / 255.0f;   // 0x3f008081 (Clear 0x80808080 into R32F)
static constexpr uint32_t ENCODE_IDENTITY = 0xfd000050;
static constexpr uint32_t FLS_GET = 0xfd000020, FLS_SET = 0xfd000030, DECODE_POINTER = 0xfd000040, KERNEL32_HANDLE = 0xfc000000;
// msvcr90 9.00.30729.1 globals (RVA): encoded FlsGetValue / FlsSetValue pointers.
static constexpr uint32_t CRT_FLSGETVALUE_ENC = 0x9bc9c, CRT_FLSSETVALUE_ENC = 0x9bca0;
// Animation worker threads (RiderAnimation jobs): job entity table, per-worker
// "go" / "quit" / "done" events, worker entry, kick and wait (MXWorld::vf5 runs
// World_PhysicsUpdate, then 0061d060 kick, then 0061d130 wait).
static constexpr uint32_t ANIM_JOBS = 0xa9408c, ANIM_GO = 0xaf6c08, ANIM_QUIT = 0xaf6b50, ANIM_DONE = 0xaf6b80;
static constexpr uint32_t ANIM_WORKER = 0x61cdb0, ANIM_KICK = 0x61d060, ANIM_WAIT = 0x61d130, ANIM_WORKERS = 12;
static constexpr uint32_t WORKER_STACK_TOP = 0x7fc00000;
static constexpr uint32_t SCENE_WORKER = 0x7d22a0;
// Engine job pool (2 threads): wake events, "started" events, worker proc 00881900.
static constexpr uint32_t POOL_WAKE = 0xd9d35c, POOL_STARTED = 0xd9d374, POOL_WORKER = 0x881900, POOL_TID = 0xd9d330;
static constexpr uint32_t ANIM_TID = 0xaf6c3c;
// FMOD command queue thread: "FMODQueueProcessEvent", thread id, proc 00806f90.
static constexpr uint32_t AUDIO_EVENT = 0xd67ce8, AUDIO_TID = 0xd67d1c, AUDIO_WORKER = 0x806f90;
// Engine task thread: waits engine+0x90, runs the tasks at engine+0x98 (count +0xc0), signals engine+0x94.
static constexpr uint32_t ENGINE_WORKER = 0x84f320;
static constexpr uint32_t AUDIO_EXECUTE = 0x806110;
static constexpr uint32_t TERRAIN_GPU_SYNC = 0x7c4fa0;  // HFTerrainSurface::vf14 (GPU deformation; not reproduced)
static constexpr uint32_t RENDER_WORKER = 0x7df370;
static constexpr uint32_t TRACKS_REBUILD = 0x7cbe30;    // TerrainTracks_FlushPending (eax = TerrainTracks): staged ribbon vertices -> VB/IB
static constexpr uint32_t TRACK_BUFFERS = 0xf8000000;    // memory-backed IDirect3D{Vertex,Index}Buffer9 stand-ins for the 3 TerrainTracks
static constexpr uint32_t TERRAIN_TRACKS[3] = {0xa95774, 0xa95778, 0xa9577c};
static constexpr uint32_t NULL_SURFACE = 0xf9010000;     // SyncRenderData worker threads (4 per render world = scene + 0x1a18)     // executes one FMOD queue command (replaced: no audio device)                  // animation worker thread ids (stride 8)
static constexpr uint32_t TLS_ARRAYS = 0x7ffb0000;             // per-thread static-TLS pointer arrays (TEB+0x2c)     // SceneManager ticker thread (entity vf19 with dt * timescale)
static constexpr uint32_t THREAD_TABLE = 0x1000;
static constexpr uint32_t HEAP_BASE = 0x30000000;
static constexpr uint32_t FMOD_EVENT_SYSTEM_GLOBAL = 0xa957f4;   // FMOD::EventSystem* (replaced by the stand-in)
#include "fmod_event_system_slots.inc"
#include "d3d9_device_methods.inc"
static constexpr uint32_t RENDERER_GLOBAL = 0xa95960;   // renderer; +0x1ec = IDirect3DDevice9* (replaced by the null device)   // deterministic heap (64 MiB)     // thread table page (count, then {id, start, cw, mxcsr})
// msvcr90 9.00.30729.1: __getvalueindex (TLS slot holding the decoded FlsGetValue), RVA.
static constexpr uint32_t CRT_GETVALUEINDEX = 0x971e4;

struct World::Impl {
    Machine m;
    uint32_t context = 0, header_dt = 0, manager = 0, resource = 0, ptd = 0, crt_base = 0, last_error = 0;
    uint16_t cw = 0x7f; uint32_t mxcsr = 0x1f80;
    std::set<uint32_t> keyboards;
    uint8_t keys[256] = {};
    WorldPad pads[4] = {};
    uint32_t pad_mask = 0, packet = 0;
    std::map<uint32_t, int> sections_held;
    std::vector<uint8_t> deform;
    std::vector<uint8_t> world;                                  // paged world image (RXWI)
    std::map<uint32_t, std::pair<uint32_t, uint32_t>> page_index; // page -> (offset, compressed size)
    // Queued stamp quads of queue `qi` (PlatformDeformablePatch = surface+0x230), offset applied
    // as FUN_007caf70 does (x += off.x/(extent/2), y += off.z/(extent/2)).
    std::vector<StampVertex> stamp_queue(Machine& m, uint32_t qi) {
        std::vector<StampVertex> out;
        uint32_t P = terrain + 0x230;
        uint32_t quads = uint32_t(m.mem.read(P + 0x230 + qi * 0x208, 4)), tris = uint32_t(m.mem.read(P + 0x440 + qi * 4, 4));
        quads = std::min({quads, tris / 2, 64u});
        uint32_t eb = uint32_t(m.mem.read(terrain + 0x44d0, 4)); float extent; std::memcpy(&extent, &eb, 4);
        float rcp = 1.0f / (extent * 0.5f);
        for (uint32_t q = 0; q < quads; q++) {
            float off[2]; m.mem.read_bytes(P + 0x30 + qi * 0x208 + 8 * q, off, 8);
            for (uint32_t k = 0; k < 6; k++) {
                float v[5]; m.mem.read_bytes(P + 0x448 + qi * 0x1e04 + (q * 6 + k) * 20, v, 20);
                out.push_back({off[0] * rcp + v[0], off[1] * rcp + v[1], v[2], v[3], v[4]});
            }
        }
        return out;
    }
    // native/game Verify: the render target's state before the first render inside a verified native run (restored after it)
    bool deform_journal = false, deform_saved_valid = false; std::vector<float> deform_saved;
    bool deform_locked = false;                                    // the deformation surface's LockRect / UnlockRect pairing
    std::function<std::function<void()>()> fmod_snapshot;         // the FMOD stand-ins' state (fmod_stand_ins.inc), for Verify
    void render_deform(Machine& m) {           // Render(N): draws queue[!q] into the render target
        if (deform_journal && !deform_saved_valid) { deform_saved = deform_rt; deform_saved_valid = true; }
        uint32_t q = uint32_t(m.mem.read(terrain + 0x230 + 0x18, 4));
        auto verts = stamp_queue(m, q ? 0u : 1u);
        DeformRect r = apply_stamps(deform_rt, int(deform_n), verts, deform_model);
        if (!r.empty()) {
            rt_unread.x0 = std::min(rt_unread.x0, r.x0); rt_unread.z0 = std::min(rt_unread.z0, r.z0);
            rt_unread.x1 = std::max(rt_unread.x1, r.x1); rt_unread.z1 = std::max(rt_unread.z1, r.z1);
        }
    }
    void readback_deform(Machine& m) {          // GetRenderTargetData(rt, physics surface)
        if (rt_unread.empty()) return;
        for (int z = rt_unread.z0; z < rt_unread.z1; z++)
            m.mem.write_block(DEFORM_BITS + uint32_t(z) * deform_n * 4u + uint32_t(rt_unread.x0) * 4u, &deform_rt[size_t(z) * deform_n + size_t(rt_unread.x0)], size_t(rt_unread.x1 - rt_unread.x0) * 4);
        dirty_x0 = std::min(dirty_x0, rt_unread.x0); dirty_z0 = std::min(dirty_z0, rt_unread.z0);
        dirty_x1 = std::max(dirty_x1, rt_unread.x1); dirty_z1 = std::max(dirty_z1, rt_unread.z1);
        deform_version++;
        rt_unread = DeformRect{};
    }
    uint32_t sim = 0, rider = 0, terrain = 0, deform_version = 1;
    bool ambient = true;
    bool particles = false;                   // particle managers simulated even with ambient off (set_particles)
    uint32_t ambient_keep = 0;                // ambient classes simulated even with ambient off (set_ambient_mask)
    DeformModel deform_model;                 // brush + softness mask (ruts); empty = no deformation pass
    std::vector<float> deform_rt;              // the latest rendered deformation render target
    DeformRect rt_unread;                      // texels where deform_rt differs from the physics surface
    bool deform_render_due = false;
    uint32_t tracks_version = 0;                              // bumped when a track vertex buffer is unlocked
    bool audio = false;                                       // audio client attached (World::set_audio)
    std::vector<World::AudioCommand> audio_out;              // queued for the client (capped)
    std::map<uint32_t, uint32_t> audio_serial;               // event wrapper -> serial of its current play
    std::vector<std::pair<uint32_t, uint32_t>> audio_done;   // (wrapper, serial) reported finished
    uint32_t deform_n = 2048;                 // deformation grid size (= terrain samples: 2048 nationals / free ride, 1024 supercross)
    int32_t dirty_x0 = 0, dirty_z0 = 0, dirty_x1 = 2048, dirty_z1 = 2048;   // deformation rows/cols changed since last query
    uint64_t qpc = 0; bool qpc_started = false; int qpc_calls = 0;   // virtual performance counter (10 MHz)
    // REFLEX_FIXED_CLOCK (clock_pin.h): engine_frame advances the counter by exactly
    // this many 100 ns units per frame instead of the caller's ticks.  replay: an
    // engine_frame_at call keeps its absolute counter (not pinned).
    bool fixed_clock = false; uint64_t fixed_dt = 0; bool replay = false;
    // Deterministic heap for allocations made while frames run (HeapAlloc & co.):
    // first fit with 16-byte granules in a 64 MiB gap of the world image.
    uint32_t heap_base = 0, heap_top = 0, heap_end = 0;
    std::map<uint32_t, uint32_t> heap_used;      // address -> requested size
    std::map<uint32_t, uint32_t> heap_free;      // address -> granule-rounded size
    uint32_t heap_alloc(Machine& m, uint32_t size, bool zero) {
        uint32_t need = std::max<uint32_t>(16, (size + 15) & ~15u);
        uint32_t a = 0;
        for (auto it = heap_free.begin(); it != heap_free.end(); ++it)
            if (it->second >= need) { a = it->first; uint32_t rest = it->second - need; heap_free.erase(it); if (rest) heap_free[a + need] = rest; break; }
        if (!a) { if (heap_top + need > heap_end) throw VmError("deterministic heap exhausted"); a = heap_top; heap_top += need; }
        heap_used[a] = size;
        if (zero) m.mem.write_block(a, nullptr, need);
        return a;
    }
    bool heap_owns(uint32_t a) const { return a >= heap_base && a < heap_end; }
    void heap_release(uint32_t a) {
        auto it = heap_used.find(a); if (it == heap_used.end()) throw VmError("HeapFree of an unknown block");
        uint32_t need = std::max<uint32_t>(16, (it->second + 15) & ~15u); heap_used.erase(it);
        auto next = heap_free.find(a + need); if (next != heap_free.end()) { need += next->second; heap_free.erase(next); }
        auto prev = heap_free.lower_bound(a);
        if (prev != heap_free.begin()) { --prev; if (prev->first + prev->second == a) { prev->second += need; return; } }
        heap_free[a] = need;
    }
    uint32_t engine = 0, live_device = 0, next_event = 0xf8000000;
    struct Worker { uint32_t proc = 0, param = 0, go = 0, done = 0, quit = 0, started = 0, tid = 0; };
    struct ThreadInfo { uint16_t cw = 0x27f; uint32_t mxcsr = 0x1f80, tls_array = 0; };
    std::map<uint32_t, ThreadInfo> threads;                     // thread id -> recorded state (thread table)
    uint32_t main_tls_array = 0, main_tid = 0;
    std::map<uint32_t, uint32_t> thread_ptd;                    // thread id -> _tiddata
    uint32_t current_tid() const { return active ? active->tid : main_tid; }
    std::map<uint32_t, std::vector<uint32_t>> start_tids;      // thread start address -> thread ids
    uint32_t tid_of_start(uint32_t start) const {
        auto it = start_tids.find(start);
        return (it != start_tids.end() && it->second.size() == 1) ? it->second[0] : 0u;
    }
    uint32_t ptd_of(uint32_t tid) const {
        if (tid == main_tid || !active) return ptd;
        auto it = thread_ptd.find(tid); return it == thread_ptd.end() ? 0u : it->second;
    }
    Worker* active = nullptr;                                   // worker job running (nullptr: main thread)
    int depth = 0;                                              // nested worker switches
    bool worker_go = false;
    std::set<uint32_t> signaled;                                // "done" events of finished jobs
    std::map<uint32_t, std::pair<uint16_t, uint32_t>> proc_fpu; // thread start -> recorded x87 CW / MXCSR
    std::map<std::pair<uint32_t, uint32_t>, std::pair<uint16_t, uint32_t>> thread_fpu;   // live per-worker FPU state
    bool find_worker(Machine& m, uint32_t h, Worker& w) {
        if (!h) return false;
        for (uint32_t i = 0; i < ANIM_WORKERS; i++)
            if (uint32_t(m.mem.read(ANIM_GO + 4 * i, 4)) == h) {
                w = {ANIM_WORKER, i, h, uint32_t(m.mem.read(ANIM_DONE + 4 * i, 4)), uint32_t(m.mem.read(ANIM_QUIT + 4 * i, 4)), 0, uint32_t(m.mem.read(ANIM_TID + 8 * i, 4))};
                return true;
            }
        for (uint32_t i = 0; i < 2; i++)
            if (uint32_t(m.mem.read(POOL_WAKE + 4 * i, 4)) == h) { w = {POOL_WORKER, i, h, 0, 0, uint32_t(m.mem.read(POOL_STARTED + 4 * i, 4)), uint32_t(m.mem.read(POOL_TID + 8 * i, 4))}; return true; }
        if (engine && uint32_t(m.mem.read(engine + 0x90, 4)) == h) { w = {ENGINE_WORKER, engine, h, uint32_t(m.mem.read(engine + 0x94, 4)), 0, 0, tid_of_start(ENGINE_WORKER)}; return true; }
        if (uint32_t(m.mem.read(AUDIO_EVENT, 4)) == h) { w = {AUDIO_WORKER, 0, h, 0, 0, 0, uint32_t(m.mem.read(AUDIO_TID, 4))}; return true; }
        uint32_t core = uint32_t(m.mem.read(0xa95844, 4));
        for (uint32_t off = 0x1c; off < 0x28; off += 4) {
            uint32_t scene = uint32_t(m.mem.read(core + off, 4));
            if (scene) {
                uint32_t rw = scene + 0x1a18;
                for (uint32_t i = 0; i < 4; i++)
                    if (uint32_t(m.mem.read(rw + 0x1180 + 4 * i, 4)) == h) {
                        w = {RENDER_WORKER, rw + 0x1190 + 16 * i, h, uint32_t(m.mem.read(rw + 0x1170 + 4 * i, 4)), 0, 0, uint32_t(m.mem.read(rw + 0x1154 + 8 * i, 4))};
                        return true;
                    }
            }
            if (scene && uint32_t(m.mem.read(scene + 0x2ca4, 4)) == h) { w = {SCENE_WORKER, scene, h, uint32_t(m.mem.read(scene + 0x2ca8, 4)), 0}; return true; }
        }
        return false;
    }
    std::string error;
    bool boot_image = false;   // made by the boot process (header word 15 'BOOT')
};

// The dump's thread table record as THREAD_TABLE pages: {count, version, 0, 0} + record.
// Version 2 records start with the game thread (start address 0); version 1 has 16-byte entries.
static std::vector<uint8_t> thread_table_pages(const uint8_t* rec, uint32_t n) {
    uint32_t version = 1, count = n / 16;
    if (n >= 32) {
        uint32_t first_start; std::memcpy(&first_start, rec + 4, 4);
        if (first_start == 0) {                                     // version 2
            version = 2; count = 0; uint32_t sizes = 0;
            while (32 * (count + 1) + sizes <= n) { uint32_t sz; std::memcpy(&sz, rec + 32 * count + 20, 4); count++; sizes += sz; if (32 * count + sizes == n) break; }
        }
    }
    std::vector<uint8_t> out((16 + n + 0xfff) & ~size_t(0xfff), 0);
    std::memcpy(out.data(), &count, 4); std::memcpy(out.data() + 4, &version, 4);
    std::memcpy(out.data() + 16, rec, n);
    return out;
}

World::World() : d(new Impl) {}
World::~World() { reflex_port::set_host_state(d->m, nullptr); delete d; }


void World::load(const std::string& exe_path, const std::string& crt_path, const std::string& world_path, const std::string& deform_path) {
    Machine& m = d->m;
    d->fixed_dt = fixed_clock_ticks();
    d->fixed_clock = d->fixed_dt != 0;
    // Original executable at its preferred base; read-only sections from the file.
    Pe exe(slurp(exe_path));
    if (exe.image_base != 0x400000) throw VmError("unexpected executable base");
    m.modules.push_back({0x400000, 0x400000 + exe.size, exe.image});
    // World image: either a full dump (<u32 address, u32 size, bytes>, header
    // region at address 0) or the paged form (RXWI: every dumped page, zlib per
    // page, decompressed on first touch; see _re/trace/pack_world.py).
    d->world = slurp(world_path);
    const std::vector<uint8_t>& world = d->world;
    uint32_t header[16] = {}; uint32_t crt_base = 0, cpp_base = 0;
    std::vector<std::pair<uint32_t, uint32_t>> world_regions;
    bool paged = world.size() >= 16 && rd<uint32_t>(world, 0) == 0x49575852u;
    if (paged) {
        if (rd<uint32_t>(world, 4) != 1) throw VmError("world image version");
        uint32_t count = rd<uint32_t>(world, 8);
        d->sim = rd<uint32_t>(world, 12);
        std::memcpy(header, world.data() + 16, 64);
        if (80 + size_t(count) * 12 > world.size()) throw VmError("truncated world");
        d->page_index.clear();
        for (uint32_t i = 0; i < count; i++) {
            uint32_t a = rd<uint32_t>(world, 80 + 12 * i);
            d->page_index[a >> 12] = {rd<uint32_t>(world, 84 + 12 * i), rd<uint32_t>(world, 88 + 12 * i)};
        }
    } else {
        for (size_t at = 0; at + 8 <= world.size();) {
            uint32_t a = rd<uint32_t>(world, at), n = rd<uint32_t>(world, at + 4); at += 8;
            if (at + n > world.size()) throw VmError("truncated world");
            if (a == 0) std::memcpy(header, world.data() + at, std::min<size_t>(64, n));
            else if (a == 0x100) {                              // thread table record -> THREAD_TABLE pages
                std::vector<uint8_t> table = thread_table_pages(world.data() + at, n);
                m.mem.map(THREAD_TABLE, table.data(), table.size());
            }
            else world_regions.push_back({uint32_t(at), a});
            at += n;
        }
    }
    d->context = header[2]; d->header_dt = header[3]; d->cw = uint16_t(header[5]); d->mxcsr = header[6] & ~0x3fu; crt_base = header[7]; cpp_base = header[8]; d->ptd = header[9]; d->crt_base = crt_base; d->rider = paged ? header[10] : 0;
    d->engine = header[14] == 0x454d5246u ? header[13] : 0;
    if (header[11] & 0x10000u) d->proc_fpu[ANIM_WORKER] = {uint16_t(header[11]), header[12] & ~0x3fu};
    for (auto& s : exe.sections) {
        if (s.flags & 0x80000000u) continue;                 // writable sections come from the world
        uint32_t vs = (s.vsize + 0xfff) & ~0xfffu;
        m.mem.map(0x400000 + s.va, exe.image.data() + s.va, vs, true);
    }
    if (paged) {
        Impl* s = d;
        m.mem.set_loader([s](uint32_t a, uint8_t* out) {
            auto [off, size] = s->page_index.at(a >> 12);
            if (size == 0) return;                               // zero page
            if (size == 4096) { std::memcpy(out, s->world.data() + off, 4096); return; }
            uLongf n = 4096;
            if (uncompress(out, &n, s->world.data() + off, size) != Z_OK || n != 4096) throw VmError("world page decompression");
        });
        for (auto& [page, entry] : d->page_index) m.mem.back(page << 12);
    }
    for (auto& [off, a] : world_regions) {
        uint32_t n = rd<uint32_t>(world, off - 4);
        bool in_world = (a >= 0x400000 && uint64_t(a) + n <= 0x20000000ull) || a >= 0x70000000u;
        if (in_world) m.mem.map(a, world.data() + off, n);
    }
    if (!paged) d->world.clear(), d->world.shrink_to_fit();
    if (m.mem.mapped(THREAD_TABLE)) {
        // Thread table page(s): u32 count, u32 version, 8 reserved, then the dump's
        // 0x100 record: 32-byte entries {id, start, cw, mxcsr, tls block, tls size, 0, 0}
        // (version 2) followed by the TLS blocks, or 16-byte entries (version 1).
        uint32_t count = uint32_t(m.mem.read(THREAD_TABLE, 4)), version = uint32_t(m.mem.read(THREAD_TABLE + 4, 4));
        uint32_t stride = version >= 2 ? 32 : 16, blocks = THREAD_TABLE + 16 + stride * count;
        uint32_t tls_index = uint32_t(m.mem.read(0xdcf74c, 4));
        m.mem.map(TLS_ARRAYS, nullptr, 0x1000);
        for (uint32_t i = 0; i < count && i < 64; i++) {
            uint32_t e = THREAD_TABLE + 16 + stride * i;
            uint32_t tid = uint32_t(m.mem.read(e, 4)), start = uint32_t(m.mem.read(e + 4, 4));
            Impl::ThreadInfo t; t.cw = uint16_t(m.mem.read(e + 8, 4)); t.mxcsr = uint32_t(m.mem.read(e + 12, 4)) & ~0x3fu;
            if (version >= 2) {
                uint32_t block = uint32_t(m.mem.read(e + 16, 4)), size = uint32_t(m.mem.read(e + 20, 4));
                if (block && size) {
                    std::vector<uint8_t> bytes(size); m.mem.read_bytes(blocks, bytes.data(), size); blocks += size;
                    if (!m.mem.mapped(block) || !m.mem.mapped(block + size - 1)) m.mem.map(block, bytes.data(), size);
                    t.tls_array = TLS_ARRAYS + 64 * i;
                    m.mem.write(t.tls_array + 4 * tls_index, 4, block);
                }
            }
            if (version >= 2 && m.mem.read(e + 24, 4)) d->thread_ptd[tid] = uint32_t(m.mem.read(e + 24, 4));   // packer-located _tiddata
            if (start == 0) { d->main_tls_array = t.tls_array; d->main_tid = tid; }   // the game (main) thread
            else { d->start_tids[start].push_back(tid); d->proc_fpu.emplace(start, std::make_pair(t.cw, t.mxcsr)); }
            d->threads[tid] = t;
        }
    }
    // Deterministic heap region: fixed 64 MiB at 0x30000000, above everything the live
    // process had allocated (the replay harness reserves the same range).
    d->heap_base = HEAP_BASE;
    for (uint32_t a = HEAP_BASE; a < HEAP_BASE + 0x04000000; a += 0x1000) if (m.mem.mapped(a)) throw VmError("deterministic heap range is occupied");
    d->heap_top = d->heap_base; d->heap_end = d->heap_base + 0x04000000;
    m.mem.map(d->heap_base, nullptr, 0x04000000);
    // Workers' _tiddata (raw dumps; packed images carry it in the thread table): the
    // block whose +0 is the thread id and whose locale pointers (+0x68, +0x6c) equal the
    // game thread's.
    if (!paged && d->ptd && !d->threads.empty()) {
        uint32_t mb = uint32_t(m.mem.read(d->ptd + 0x68, 4)), lc = uint32_t(m.mem.read(d->ptd + 0x6c, 4));
        std::map<uint32_t, std::vector<uint32_t>> found;
        for (uint32_t pg = 0x400000; pg < 0x20000000; pg += 0x1000) {
            uint8_t* b = m.mem.page(pg); if (!b) continue;
            for (uint32_t off = 0; off + 0x70 <= 0x1000; off += 8) {
                uint32_t tid, x, y; std::memcpy(&tid, b + off, 4);
                if (!d->threads.count(tid) || tid == d->main_tid) continue;
                std::memcpy(&x, b + off + 0x68, 4); std::memcpy(&y, b + off + 0x6c, 4);
                if (x == mb && y == lc) found[tid].push_back(pg + off);
            }
        }
        for (auto& [tid, list] : found) { if (list.size() != 1) throw VmError("ambiguous worker _tiddata"); d->thread_ptd[tid] = list[0]; }
    }
    // Imports -> named sentinels.
    uint32_t index = 0;
    for (auto& imp : exe.imports()) {
        uint32_t sentinel = SENTINEL + 16 * index++;
        m.import_names[sentinel] = imp.dll + "!" + imp.name;
        m.mem.set_readonly(imp.iat, 4, false); m.mem.write(imp.iat, 4, sentinel); m.mem.set_readonly(imp.iat, 4, true);
    }
    // VC90 CRT and C++ library relocated to their live bases; their live writable
    // data comes from the world. msvcp90.dll sits beside msvcr90.dll.
    if (!crt_base) throw VmError("world image does not record the CRT base");
    std::string cpp_path = crt_path.substr(0, crt_path.find_last_of("/\\") + 1) + "msvcp90.dll";
    std::map<std::string, std::map<std::string, uint32_t>> libraries;
    auto load_library = [&](const std::string& dll, const std::string& path, uint32_t base) {
        Pe lib(slurp(path));
        if (!base) base = lib.image_base;
        lib.relocate(base);
        m.modules.push_back({base, base + lib.size, lib.image});
        for (auto& s : lib.sections) {
            uint32_t a = base + s.va, vs = (s.vsize + 0xfff) & ~0xfffu;
            if ((s.flags & 0x80000000u) && m.mem.mapped(a)) continue;
            m.mem.map(a, lib.image.data() + s.va, vs, !(s.flags & 0x80000000u));
        }
        auto& exports = libraries[dll] = lib.exports();
        for (auto& imp : lib.imports()) {
            auto l = libraries.find(imp.dll);
            uint32_t target;
            if (l != libraries.end() && l->second.count(imp.name)) target = l->second.at(imp.name);
            else { target = SENTINEL + 16 * index++; m.import_names[target] = imp.dll + "!" + imp.name; }
            bool ro = m.mem.page(imp.iat) != nullptr;
            m.mem.set_readonly(imp.iat, 4, false); m.mem.write(imp.iat, 4, target); if (ro) m.mem.set_readonly(imp.iat, 4, true);
        }
        for (auto& imp : exe.imports()) {
            if (imp.dll != dll) continue;
            auto e = exports.find(imp.name);
            if (e == exports.end()) continue;
            m.mem.set_readonly(imp.iat, 4, false); m.mem.write(imp.iat, 4, e->second); m.mem.set_readonly(imp.iat, 4, true);
        }
    };
    load_library("msvcr90.dll", crt_path, crt_base);
    for (auto& [name, address] : libraries["msvcr90.dll"]) m.crt_exports[name] = address;
    load_library("msvcp90.dll", cpp_path, cpp_base);
    // World images made by the boot process (header word 15 'BOOT'): the boot's own import
    // sentinels (0xec000000..), stored by the CRT/game in data (pointers from GetProcAddress),
    // keep their names here; the table is at 0x20000 ('BSNT', count, {u32 address, name[124]}).
    d->boot_image = header[15] == 0x544f4f42u;
    if (header[15] == 0x544f4f42u && m.mem.mapped(0x20000) && m.mem.read(0x20000, 4) == 0x544e5342u) {
        uint32_t n = uint32_t(m.mem.read(0x20004, 4));
        for (uint32_t i = 0; i < n && i < 4096; i++) {
            uint32_t e = 0x20010 + 128 * i; char name[125] = {};
            m.mem.read_bytes(e + 4, name, 124);
            m.import_names[uint32_t(m.mem.read(e, 4))] = name;
        }
    }
    // Thread environment block and stack.
    std::vector<uint8_t> teb(4096, 0);
    uint32_t teb_words[3] = {0xffffffffu, STACK_TOP, STACK_TOP - 0x40000};
    std::memcpy(teb.data(), teb_words, 12); uint32_t self = FS_BASE; std::memcpy(teb.data() + 0x18, &self, 4);
    m.mem.map(FS_BASE, teb.data(), teb.size());
    m.mem.write(FS_BASE + 0x2c, 4, d->main_tls_array);
    m.mem.map(STACK_TOP - 0x40000, nullptr, 0x40000);
    m.mem.map(WORKER_STACK_TOP - 0x40000, nullptr, 0x40000);       // 3 nested worker stacks of 64 KiB
    m.r[4] = STACK_TOP - 0x100;
    m.fpu_cw = d->cw; m.mxcsr = d->mxcsr;
    install_stand_ins(deform_path);
    // native/game Verify: the host-side state the stand-ins change while frames run, saved before a verified native run and put back
    // after it (with the guest memory), so the original then runs from the same state.
    {
        Impl* const d = this->d;
    reflex_port::set_host_state(d->m, [d]() -> std::function<void()> {
        struct S {
            std::map<uint32_t, int> sections_held; DeformRect rt_unread; bool deform_render_due; uint32_t deform_version, tracks_version;
            int32_t dx0, dz0, dx1, dz1; size_t audio_n; std::map<uint32_t, uint32_t> audio_serial; std::vector<std::pair<uint32_t, uint32_t>> audio_done;
            uint64_t qpc; bool qpc_started; int qpc_calls; uint32_t heap_top; std::map<uint32_t, uint32_t> heap_used, heap_free;
            Impl::Worker* active; int depth; bool worker_go; std::set<uint32_t> signaled;
            std::map<std::pair<uint32_t, uint32_t>, std::pair<uint16_t, uint32_t>> thread_fpu; std::map<uint32_t, uint32_t> thread_ptd;
            uint32_t last_error, next_event, packet; bool deform_locked; std::function<void()> fmod_restore;
        };
        auto s = std::make_shared<S>(S{d->sections_held, d->rt_unread, d->deform_render_due, d->deform_version, d->tracks_version,
            d->dirty_x0, d->dirty_z0, d->dirty_x1, d->dirty_z1, d->audio_out.size(), d->audio_serial, d->audio_done,
            d->qpc, d->qpc_started, d->qpc_calls, d->heap_top, d->heap_used, d->heap_free,
            d->active, d->depth, d->worker_go, d->signaled, d->thread_fpu, d->thread_ptd, d->last_error, d->next_event, d->packet, d->deform_locked,
            d->fmod_snapshot ? d->fmod_snapshot() : std::function<void()>()});
        d->deform_journal = true; d->deform_saved_valid = false;
        return [d, s]() {
            d->sections_held = s->sections_held; d->rt_unread = s->rt_unread; d->deform_render_due = s->deform_render_due;
            d->deform_version = s->deform_version; d->tracks_version = s->tracks_version;
            d->dirty_x0 = s->dx0; d->dirty_z0 = s->dz0; d->dirty_x1 = s->dx1; d->dirty_z1 = s->dz1;
            if (d->audio_out.size() > s->audio_n) d->audio_out.resize(s->audio_n);
            d->audio_serial = s->audio_serial; d->audio_done = s->audio_done;
            d->qpc = s->qpc; d->qpc_started = s->qpc_started; d->qpc_calls = s->qpc_calls;
            d->heap_top = s->heap_top; d->heap_used = s->heap_used; d->heap_free = s->heap_free;
            d->active = s->active; d->depth = s->depth; d->worker_go = s->worker_go; d->signaled = s->signaled;
            d->thread_fpu = s->thread_fpu; d->thread_ptd = s->thread_ptd; d->last_error = s->last_error; d->next_event = s->next_event; d->packet = s->packet;
            d->deform_locked = s->deform_locked; if (s->fmod_restore) s->fmod_restore();
            if (d->deform_saved_valid) d->deform_rt.swap(d->deform_saved);
            d->deform_journal = false; d->deform_saved_valid = false; d->deform_saved.clear();
        };
    });
    }
    reflex_port::install_from_env(m);   // the native game's ports (native/game): REFLEX_PORT=<groups>, REFLEX_PORT_VERIFY=1
}

void World::install_stand_ins(const std::string& deform_path) {
    Machine& m = d->m; Impl* s = d;
    // ---- selected terrain's deformation surface ----
    uint32_t registry[3]; for (int i = 0; i < 3; i++) registry[i] = uint32_t(m.mem.read(0x9e30d4 + 4 * i, 4));
    uint32_t scene = uint32_t(m.mem.read(0xa95844, 4));
    uint32_t identity = uint32_t(m.mem.read(scene + 0x1c + uint32_t(m.mem.read(scene + 4, 4)) * 4, 4)), terrain = 0;
    for (uint32_t i = 0; i < registry[2] && i < 2; i++) if (registry[i] && uint32_t(m.mem.read(registry[i] + 0x44a8, 4)) == identity) terrain = registry[i];
    if (!terrain) throw VmError("selected terrain missing");
    s->resource = uint32_t(m.mem.read(terrain + 0x240, 4));
    s->terrain = terrain;
    s->deform = slurp(deform_path);
    s->deform_n = s->deform.size() == 16777216 ? 2048u : s->deform.size() == 4194304 ? 1024u : s->deform.size() == 1048576 ? 512u : 0u;
    if (!s->deform_n) throw VmError("deformation grid size");
    s->dirty_x1 = s->dirty_z1 = int32_t(s->deform_n);
    m.mem.map(DEFORM_BITS, s->deform.data(), s->deform.size(), false);   // written by the deformation pass
    {   // Deformation model (brush + softness mask) next to the grid: <name>-deform-model.bin for <name>-deform.bin.
        std::string model = deform_path;
        size_t dot = model.rfind(".bin");
        if (dot != std::string::npos) model = model.substr(0, dot) + "-model.bin";
        if (s->deform_model.load(model)) {
            s->deform_rt.assign(reinterpret_cast<const float*>(s->deform.data()), reinterpret_cast<const float*>(s->deform.data()) + size_t(s->deform_n) * s->deform_n);
            // The live game had already rendered queue[!q] (Render of the frame before the
            // dump) into its render target but not yet read it back: rebuild that target.
            s->render_deform(m);
        }
    }
    uint8_t table[60] = {}; uint32_t lock = LOCK, unlock = UNLOCK; std::memcpy(table + 52, &lock, 4); std::memcpy(table + 56, &unlock, 4);
    m.mem.map(TABLE, table, sizeof table, true);
    m.mem.write(s->resource, 4, TABLE);
    s->deform_locked = false;
    m.natives[LOCK] = [s](Machine& m) {
        uint32_t self = m.arg(0), rect = m.arg(1), region = m.arg(2), flags = m.arg(3);
        if (self != s->resource || region || flags != 0x810 || s->deform_locked) throw VmError("LockRect ABI");
        m.mem.write(rect, 4, s->deform_n * 4); m.mem.write(rect + 4, 4, DEFORM_BITS); s->deform_locked = true;
        return m.ret(16, 0);
    };
    m.natives[UNLOCK] = [s](Machine& m) {
        if (m.arg(0) != s->resource || !s->deform_locked) throw VmError("UnlockRect ABI");
        s->deform_locked = false; return m.ret(4, 0);
    };
    // ---- deterministic clock and critical sections ----
    // Millisecond clocks: fixed in physics-only frames; in whole frames the same
    // monotonic clock as the virtual performance counter (Wine: QPC = 100 ns units).
    m.imports["winmm.dll!timeGetTime"] = [s](Machine& m) { return m.ret(0, s->qpc_started ? uint32_t(s->qpc / 10000) : CLOCK_MS); };
    m.imports["kernel32.dll!GetTickCount"] = [s](Machine& m) { return m.ret(0, s->qpc_started ? uint32_t(s->qpc / 10000) : CLOCK_MS); };
    // CRT per-thread data (_getptd_noexit): the game thread's _tiddata from the
    // world image (header word 9); errno and the rand() seed live there.
    m.imports["kernel32.dll!GetLastError"] = [s](Machine& m) { return m.ret(0, s->last_error); };
    m.imports["kernel32.dll!SetLastError"] = [s](Machine& m) { s->last_error = m.arg(0); return m.ret(4); };
    m.imports["kernel32.dll!TlsGetValue"] = [s](Machine& m) {
        if (m.arg(0) != uint32_t(m.mem.read(s->crt_base + CRT_GETVALUEINDEX, 4))) throw VmError("TlsGetValue: unexpected slot");
        return m.ret(4, FLS_GET);
    };
    // Per-thread CRT data: each thread's _tiddata (the game thread's from header word 9,
    // workers' from the thread table, found by thread id in the world image); a
    // thread without one gets it created by the CRT itself (calloc + FlsSetValue).
    m.natives[FLS_GET] = [s](Machine& m) { return m.ret(4, s->ptd_of(s->current_tid())); };
    m.natives[FLS_SET] = [s](Machine& m) { s->thread_ptd[s->current_tid()] = m.arg(1); return m.ret(8, 1); };
    m.natives[DECODE_POINTER] = [s](Machine& m) {
        uint32_t x = m.arg(0);
        if (x == uint32_t(m.mem.read(s->crt_base + CRT_FLSSETVALUE_ENC, 4))) return m.ret(4, FLS_SET);
        if (x == uint32_t(m.mem.read(s->crt_base + CRT_FLSGETVALUE_ENC, 4))) return m.ret(4, FLS_GET);
        if (s->boot_image) return m.ret(4, x);   // boot-made images: the boot's EncodePointer is the identity
        char b[64]; std::snprintf(b, sizeof b, "DecodePointer of an unknown value %08x", x); throw VmError(b);
    };
    m.imports["kernel32.dll!DecodePointer"] = [](Machine& m) { return m.natives[DECODE_POINTER](m); };
    m.imports["kernel32.dll!GetModuleHandleW"] = [s](Machine& m) {
        uint32_t p = m.arg(0); std::string name;
        for (uint32_t i = 0; i < 64; i++) { uint16_t c = uint16_t(m.mem.read(p + 2 * i, 2)); if (!c) break; name += char(std::toupper(c)); }
        if (name == "KERNEL32.DLL") return m.ret(4, KERNEL32_HANDLE);
        throw VmError("GetModuleHandleW " + name);
    };
    m.natives[ENCODE_IDENTITY] = [](Machine& m) { return m.ret(4, m.arg(0)); };
    m.imports["kernel32.dll!GetProcAddress"] = [s](Machine& m) {
        uint32_t h = m.arg(0), p = m.arg(1); std::string name;
        for (uint32_t i = 0; i < 64; i++) { char c = char(m.mem.read(p + i, 1)); if (!c) break; name += c; }
        if (h == KERNEL32_HANDLE && name == "DecodePointer") return m.ret(8, DECODE_POINTER);
        if (name == "EncodePointer" && s->boot_image) return m.ret(8, ENCODE_IDENTITY);
        throw VmError("GetProcAddress " + name);
    };
    // Worker threads, serialized. The game's workers (RiderAnimation jobs 0061cdb0,
    // scene tickers 007d22a0) block on a "go" event, run one job and signal "done".
    // SetEvent on a worker's "go" event runs that worker's job to completion on its
    // own stack with its own FPU state (a thread switch with the signalling
    // thread's registers saved); its next wait on "go" switches back. A main-thread
    // wait on a "done" event then succeeds.
    m.imports["kernel32.dll!SetEvent"] = [s](Machine& m) {
        uint32_t h = m.arg(0);
        if (s->active) {
            if (s->active->started && h == s->active->started) return m.ret(4, 1);   // pool worker prologue
            if (s->active->done && h == s->active->done) { s->signaled.insert(h); return m.ret(4, 1); }
        }
        // Database async loader (thread 0084c760, request event Database+0xf8, Database =
        // core+8): streams sound databases ('soun'/'dbnk'). Audio is not reproduced, so
        // these loads are never serviced; the game keeps seeing them as pending.
        {
            uint32_t core = uint32_t(m.mem.read(0xa95844, 4)), database = core ? uint32_t(m.mem.read(core + 8, 4)) : 0;
            if (database && h == uint32_t(m.mem.read(database + 0xf8, 4))) {
                if (!s->active) return m.ret(4, 1);
                throw VmError("Database loader request from a worker thread");
            }
        }
        Impl::Worker w;
        if (!s->find_worker(m, h, w)) {
            char b[160]; std::snprintf(b, sizeof b, "SetEvent on an unknown event %08x (worker %08x, return %08x) stack:", h, s->active ? s->active->proc : 0u, uint32_t(m.mem.read(m.r[4], 4)));
            std::string msg = b;
            for (uint32_t a = m.r[4], n = 0; a < m.r[4] + 0x800 && n < 16; a += 4) {   // words that look like return addresses into the game
                uint32_t v = uint32_t(m.mem.read(a, 4));
                if (v > 0x401000 && v < 0x916000) { char w[16]; std::snprintf(w, sizeof w, " %08x", v); msg += w; n++; }
            }
            throw VmError(msg);
        }
        if (s->active && s->active->go == w.go) throw VmError("worker signals its own go event");
        auto fpu_state = s->thread_fpu.find({w.proc, w.param});
        auto ti = s->threads.find(w.tid);
        if (fpu_state == s->thread_fpu.end() && w.tid && ti != s->threads.end())
            fpu_state = s->thread_fpu.emplace(std::make_pair(w.proc, w.param), std::make_pair(ti->second.cw, ti->second.mxcsr)).first;
        if (fpu_state == s->thread_fpu.end()) {
            auto by_proc = s->proc_fpu.find(w.proc);
            if (by_proc == s->proc_fpu.end()) {
                if (!std::getenv("REFLEX_ASSUME_THREAD_FPU")) throw VmError("no FPU state recorded for worker thread");
                by_proc = s->proc_fpu.emplace(w.proc, std::make_pair(uint16_t(0x27f), 0x1f80u)).first;   // Windows default for new threads
            }
            fpu_state = s->thread_fpu.emplace(std::make_pair(w.proc, w.param), by_proc->second).first;
        }
        // Thread switch: save the signalling thread completely (registers, FPU, TEB, active worker).
        uint32_t saved_r[8]; std::memcpy(saved_r, m.r, sizeof saved_r);
        uint32_t flags = m.flags; Xmm xmm[8]; std::memcpy(xmm, m.xmm, sizeof xmm);
        auto fpu = m.fpu; uint16_t cw = m.fpu_cw, sw = m.fpu_sw; uint32_t mxcsr = m.mxcsr;
        uint8_t teb[4096]; m.mem.read_block(FS_BASE, teb, sizeof teb);
        Impl::Worker* outer = s->active; bool outer_go = s->worker_go;
        uint32_t stack_top = WORKER_STACK_TOP - 0x10000 * s->depth;
        if (s->depth >= 3) throw VmError("worker nesting too deep");
        s->depth++;
        s->active = &w; s->worker_go = true;
        m.mem.write(FS_BASE + 0x2c, 4, (w.tid && ti != s->threads.end()) ? ti->second.tls_array : 0u);
        m.fpu.clear(); m.fpu_cw = fpu_state->second.first; m.fpu_sw = 0; m.mxcsr = fpu_state->second.second; m.flags = 0x202;
        m.r[4] = stack_top - 0x100;
        m.push(w.param); m.push(0);
        m.run(w.proc);
        fpu_state->second = {m.fpu_cw, m.mxcsr};
        s->depth--; s->active = outer; s->worker_go = outer_go;
        std::memcpy(m.r, saved_r, sizeof saved_r); m.flags = flags; std::memcpy(m.xmm, xmm, sizeof xmm);
        m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr;
        m.mem.write_block(FS_BASE, teb, sizeof teb);
        return m.ret(4, 1);
    };
    m.imports["kernel32.dll!WaitForSingleObject"] = [s](Machine& m) {
        uint32_t h = m.arg(0), timeout = m.arg(1);
        if (!s->active) {
            if (s->signaled.erase(h)) return m.ret(8, 0);           // a serialized worker already finished
            throw VmError("WaitForSingleObject on the main thread: unexpected handle");
        }
        if (s->signaled.erase(h)) return m.ret(8, 0);               // a serialized thread already signalled it
        if (s->active->quit && h == s->active->quit && timeout == 0) return m.ret(8, 0x102);   // not quitting
        if (h == s->active->go && timeout == 0xffffffffu) {
            if (s->worker_go) { s->worker_go = false; return m.ret(8, 0); }
            return 0u;                                   // job done: the worker blocks again -> back to the signalling thread
        }
        char b[128]; std::snprintf(b, sizeof b, "WaitForSingleObject %08x on worker %08x (return %08x)", h, s->active->proc, uint32_t(m.mem.read(m.r[4], 4)));
        throw VmError(b);
    };
    m.imports["kernel32.dll!ResetEvent"] = [s](Machine& m) {
        if (s->active && m.arg(0) == s->active->go) return m.ret(4, 1);      // pool worker prologue: its own wake event
        if (!s->active) { s->signaled.erase(m.arg(0)); return m.ret(4, 1); }  // main thread resets a done event before reuse
        throw VmError("ResetEvent: unexpected handle");
    };
    // Thread ids (InverseKinematicsAnimator::vf4 calls it and discards the result).
    m.imports["kernel32.dll!GetCurrentThreadId"] = [s](Machine& m) {
        uint32_t tid = s->current_tid();
        return m.ret(0, tid ? tid : (s->active ? 0x200u + (s->active->proc & 0xfff) : 0x100u));
    };
    m.imports["kernel32.dll!Sleep"] = [](Machine&) -> uint32_t { throw VmError("Sleep: worker jobs still pending"); };
    // Virtual performance counter for Engine::UpdateTiming (Wine: 10 MHz). The first
    // read of a frame returns the frame's time; further reads in the same frame
    // (the frame-cap busy wait) see 10 us pass per read, as a real wait would.
    m.imports["kernel32.dll!QueryPerformanceFrequency"] = [](Machine& m) { m.mem.write(m.arg(0), 8, 10000000ull); return m.ret(4, 1); };
    m.imports["kernel32.dll!QueryPerformanceCounter"] = [s](Machine& m) {
        if (s->qpc_calls++ > 0) s->qpc += 100;
        m.mem.write(m.arg(0), 8, s->qpc); return m.ret(4, 1);
    };
    // Interlocked operations: threads are serialized, so plain read-modify-write.
    m.imports["kernel32.dll!InterlockedCompareExchange"] = [](Machine& m) {
        uint32_t p = m.arg(0), x = m.arg(1), c = m.arg(2), old = uint32_t(m.mem.read(p, 4));
        if (old == c) m.mem.write(p, 4, x);
        return m.ret(12, old);
    };
    m.imports["kernel32.dll!InterlockedExchange"] = [](Machine& m) { uint32_t p = m.arg(0), old = uint32_t(m.mem.read(p, 4)); m.mem.write(p, 4, m.arg(1)); return m.ret(8, old); };
    m.imports["kernel32.dll!InterlockedExchangeAdd"] = [](Machine& m) { uint32_t p = m.arg(0), old = uint32_t(m.mem.read(p, 4)); m.mem.write(p, 4, old + m.arg(1)); return m.ret(8, old); };
    m.imports["kernel32.dll!InterlockedIncrement"] = [](Machine& m) { uint32_t p = m.arg(0), v = uint32_t(m.mem.read(p, 4)) + 1; m.mem.write(p, 4, v); return m.ret(4, v); };
    m.imports["kernel32.dll!InterlockedDecrement"] = [](Machine& m) { uint32_t p = m.arg(0), v = uint32_t(m.mem.read(p, 4)) - 1; m.mem.write(p, 4, v); return m.ret(4, v); };
    // Steam: no signed-in Steam user (the live sign-in manager stays in its
    // "not signed in" state 0, re-polling SteamUser() every frame).
    // No Steam client: every interface accessor returns null (the game null-checks them),
    // callback (un)registration is a no-op.
    for (const char* f : {"SteamUser", "SteamUserStats", "SteamFriends", "SteamUtils", "SteamRemoteStorage", "SteamNetworking", "SteamMatchmaking"})
        m.imports[std::string("steam_api.dll!") + f] = [](Machine& m) { return m.ret(0, 0); };
    for (const char* f : {"SteamAPI_RegisterCallback", "SteamAPI_UnregisterCallback", "SteamAPI_RegisterCallResult", "SteamAPI_UnregisterCallResult"})
        m.imports[std::string("steam_api.dll!") + f] = [](Machine& m) { return m.ret(0, 0); };   // cdecl
    m.imports["steam_api.dll!SteamAPI_RunCallbacks"] = [](Machine& m) { return m.ret(0, 0); };   // nothing pending without Steam
    // Heap: blocks allocated while frames run come from the deterministic heap; blocks
    // allocated by the live process before the dump are released as no-ops (their
    // memory simply stays as captured).
    m.imports["kernel32.dll!HeapAlloc"] = [s](Machine& m) { uint32_t flags = m.arg(1), size = m.arg(2); return m.ret(12, s->heap_alloc(m, size, flags & 8)); };
    m.imports["kernel32.dll!HeapFree"] = [s](Machine& m) { uint32_t a = m.arg(2); if (a && s->heap_owns(a)) s->heap_release(a); return m.ret(12, 1); };
    m.imports["kernel32.dll!HeapSize"] = [s](Machine& m) {
        uint32_t a = m.arg(2); auto it = s->heap_used.find(a);
        if (it == s->heap_used.end()) throw VmError("HeapSize of a block from the live heap");
        return m.ret(12, it->second);
    };
    m.imports["kernel32.dll!HeapReAlloc"] = [s](Machine& m) {
        uint32_t flags = m.arg(1), a = m.arg(2), size = m.arg(3);
        auto it = s->heap_used.find(a);
        if (flags & 0x10) {   // HEAP_REALLOC_IN_PLACE_ONLY (the CRT's _expand): never moves; fails (NULL) when it cannot stay, as Windows
            if (it != s->heap_used.end() && size <= it->second) return m.ret(16, a);
            s->last_error = 8; return m.ret(16, 0);
        }
        if (it == s->heap_used.end()) throw VmError("HeapReAlloc of a block from the live heap");
        uint32_t old = it->second, b = s->heap_alloc(m, size, flags & 8);
        std::vector<uint8_t> tmp(std::min(old, size)); m.mem.read_block(a, tmp.data(), tmp.size()); m.mem.write_block(b, tmp.data(), tmp.size());
        s->heap_release(a);
        return m.ret(16, b);
    };
#include "fmod_stand_ins.inc"
    // ANSI <-> UTF-16 (code page 0 / 1252 / UTF-8 for ASCII; Windows-1252 otherwise).
    m.imports["kernel32.dll!MultiByteToWideChar"] = [](Machine& m) {
        static const uint16_t cp1252[32] = {0x20ac,0x81,0x201a,0x192,0x201e,0x2026,0x2020,0x2021,0x2c6,0x2030,0x160,0x2039,0x152,0x8d,0x17d,0x8f,
                                            0x90,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,0x2dc,0x2122,0x161,0x203a,0x153,0x9d,0x17e,0x178};
        uint32_t cp = m.arg(0), src = m.arg(2), dst = m.arg(4); int32_t n = int32_t(m.arg(3)), cap = int32_t(m.arg(5));
        if (cp != 0 && cp != 1252 && cp != 3 && cp != 65001) throw VmError("MultiByteToWideChar code page");
        std::vector<uint16_t> out;
        for (int32_t i = 0; n < 0 || i < n; i++) {
            uint8_t c = uint8_t(m.mem.read(src + uint32_t(i), 1));
            if (cp == 65001 && c >= 0x80) throw VmError("MultiByteToWideChar UTF-8 non-ASCII");
            out.push_back(c >= 0x80 && c < 0xa0 ? cp1252[c - 0x80] : c);
            if (n < 0 && !c) break;
        }
        if (cap == 0) return m.ret(24, uint32_t(out.size()));
        if (int32_t(out.size()) > cap) return m.ret(24, 0);
        for (size_t i = 0; i < out.size(); i++) m.mem.write(dst + 2 * uint32_t(i), 2, out[i]);
        return m.ret(24, uint32_t(out.size()));
    };
    m.imports["kernel32.dll!WideCharToMultiByte"] = [](Machine& m) {
        uint32_t cp = m.arg(0), src = m.arg(2), dst = m.arg(4); int32_t n = int32_t(m.arg(3)), cap = int32_t(m.arg(5));
        if (cp != 0 && cp != 1252 && cp != 3 && cp != 65001) throw VmError("WideCharToMultiByte code page");
        std::vector<uint8_t> out;
        for (int32_t i = 0; n < 0 || i < n; i++) {
            uint16_t c = uint16_t(m.mem.read(src + 2 * uint32_t(i), 2));
            if (c >= 0x80 && (cp == 65001 || c > 0xff || c < 0xa0)) throw VmError("WideCharToMultiByte non-Latin-1");
            out.push_back(uint8_t(c));
            if (n < 0 && !c) break;
        }
        if (cap == 0) return m.ret(32, uint32_t(out.size()));
        if (int32_t(out.size()) > cap) return m.ret(32, 0);
        m.mem.write_bytes(dst, out.data(), out.size());
        return m.ret(32, uint32_t(out.size()));
    };
    // Audio command execution (00806110, EAX = command): no audio device. Game
    // callbacks carried by the queue (type 10: fn(cmd[5], cmd[2])) run as in the
    // original; every FMOD command is consumed as handled (return 1), so the game's
    // own queue/free-list bookkeeping (00805ff0) runs unchanged.
    m.natives[AUDIO_EXECUTE] = [s](Machine& m) -> uint32_t {
        uint32_t cmd = m.r[0];
        // Event commands (type 0x13, _re/notes/engine-audio.md section 7): cmd+4 event wrapper
        // (path at +0x18), cmd+8 sub-command: 0 play, 1/12 stop, 4 set parameter (name +0x0c,
        // value +0x4c), 5 3D position, 10 occlusion. With an audio client the wrapper is marked
        // playing like FMOD would (handle +0x118, state +0x138, flags +0x13c), and its finish is
        // reported back through World::audio_finished (callback 0x80b7d0, EVENT_FINISHED).
        static FILE* audio_types = std::getenv("REFLEX_AUDIO_TYPES") ? std::fopen(std::getenv("REFLEX_AUDIO_TYPES"), "w") : nullptr;   // diagnostics
        if (audio_types) {
            uint32_t type = uint32_t(m.mem.read(cmd, 4));
            std::fprintf(audio_types, "%u", type);
            for (int i = 1; i < 22; i++) std::fprintf(audio_types, " %08x", uint32_t(m.mem.read(cmd + 4 * i, 4)));
            if (type == 0x13) { uint32_t w = uint32_t(m.mem.read(cmd + 4, 4)); std::fprintf(audio_types, " |"); for (uint32_t i = 0; i < 64; i++) { char c = char(m.mem.read(w + 0x18 + i, 1)); if (!c) break; std::fputc(c, audio_types); } }
            std::fputc('\n', audio_types);
        }
        if (s->audio && m.mem.read(cmd, 4) == 0x13) {
            uint32_t w = uint32_t(m.mem.read(cmd + 4, 4)), sub = uint32_t(m.mem.read(cmd + 8, 4));
            if (sub == 3) {   // GetEvent + setCallback (0x80aaa0): the event is ready -> load state 5, event handle set
                if (uint32_t(m.mem.read(w + 0x118, 4)) == 0) m.mem.write(w + 0x118, 4, 0xfb200000u + (w & 0xfff0u));
                m.mem.write(w + 0x138, 4, 5);
            }
            if (sub == 0 || sub == 1 || sub == 2 || sub == 4 || sub == 5 || sub == 7 || sub == 8 || sub == 10 || sub == 11 || sub == 12) {
                World::AudioCommand c{}; c.sub = int32_t(sub); c.wrapper = w; c.state = int32_t(m.mem.read(w + 0x138, 4));
                auto f32 = [&](uint32_t a) { uint32_t b = uint32_t(m.mem.read(a, 4)); float v; std::memcpy(&v, &b, 4); return v; };
                if ((sub == 0 && m.mem.read(cmd + 0x38, 1)) || sub == 5) {
                    c.position[0] = f32(cmd + 0x0c); c.position[1] = f32(cmd + 0x10); c.position[2] = f32(cmd + 0x14); c.has_position = 1;
                }
                if (sub == 2 || sub == 10) c.value = f32(cmd + 0x0c);   // 10: set3DOcclusion direct (reverb at cmd+0x10 not carried)
                if (sub == 7 || sub == 8) c.value = float(m.mem.read(cmd + 0x0c, 1) != 0);
                for (uint32_t i = 0; i + 1 < sizeof c.path; i++) { char ch = char(m.mem.read(w + 0x18 + i, 1)); if (!ch) break; c.path[i] = ch; }
                if (sub == 0) {
                    if (uint32_t(m.mem.read(w + 0x118, 4)) == 0) m.mem.write(w + 0x118, 4, 0xfb200000u + (w & 0xfff0u));
                    uint32_t st = uint32_t(m.mem.read(w + 0x138, 4)); if (st != 1 && st != 2) m.mem.write(w + 0x138, 4, 6);
                    m.mem.write(w + 0x13c, 4, 0x19);
                    c.serial = ++s->audio_serial[w];
                } else if (sub == 4) {
                    for (uint32_t i = 0; i + 1 < sizeof c.param; i++) { char ch = char(m.mem.read(cmd + 0xc + i, 1)); if (!ch) break; c.param[i] = ch; }
                    uint32_t b = uint32_t(m.mem.read(cmd + 0x4c, 4)); std::memcpy(&c.value, &b, 4);
                    c.serial = s->audio_serial[w];
                } else if (sub == 1 || sub == 12) { m.mem.write(w + 0x13c, 4, 1); c.serial = ++s->audio_serial[w]; }
                else c.serial = s->audio_serial[w];
                if (s->audio_out.size() < 4096) s->audio_out.push_back(c);
            }
        }
        if (m.mem.read(cmd, 4) == 1) {          // synchronisation: signal the waiting thread's event
            s->signaled.insert(uint32_t(m.mem.read(uint32_t(m.mem.read(cmd + 20, 4)), 4)));
        }
        if (m.mem.read(cmd, 4) == 10) {
            uint32_t fn = uint32_t(m.mem.read(cmd + 4, 4)), args[2] = {uint32_t(m.mem.read(cmd + 20, 4)), uint32_t(m.mem.read(cmd + 8, 4))};
            uint32_t esp = m.r[4], ebx = m.r[3], esi = m.r[6], edi = m.r[7], ebp = m.r[5];
            m.call_function(fn, args, 2);
            m.r[4] = esp; m.r[3] = ebx; m.r[6] = esi; m.r[7] = edi; m.r[5] = ebp;
        }
        return m.ret(0, 1);
    };
    // Null Direct3D 9 device for update-time renderer calls (rendering is not run):
    // every method succeeds (D3D_OK) without effect; object-creating/-returning
    // methods hand out null-device objects whose methods trap with their slot.
    // Effect on simulation: GPU-rendered terrain deformation is not read back, so the
    // captured deformation grid stays as captured (same as physics-only frames).
    {
        constexpr uint32_t DEVICE_OBJECT = 0xf9000000, DEVICE_VTABLE = 0xf9001000, DEVICE_CODE = 0xf9002000;
        uint32_t object[4] = {DEVICE_VTABLE, 0, 0, 0};
        m.mem.map(DEVICE_OBJECT, reinterpret_cast<uint8_t*>(object), 16, true);
        std::vector<uint32_t> vt(119);
        for (uint32_t i = 0; i < 119; i++) {
            vt[i] = DEVICE_CODE + 4 * i;
            std::string nm = D3D9_DEVICE_METHODS[i].name; uint32_t bytes = D3D9_DEVICE_METHODS[i].bytes;
            m.natives[DEVICE_CODE + 4 * i] = [nm, bytes, s](Machine& m) -> uint32_t {
                if (nm == "GetRenderTargetData" && s->deform_model.loaded() && m.arg(2) == s->resource) {
                    // Readback of the deformation render target into the physics surface.
                    s->readback_deform(m);
                    return m.ret(bytes, 0);
                }
                if (nm == "GetDepthStencilSurface" || nm == "GetRenderTarget") {   // level-reset path of vf14: a releasable null surface
                    m.mem.write(m.arg(nm == "GetRenderTarget" ? 2 : 1), 4, NULL_SURFACE);
                    return m.ret(bytes, 0);
                }
                if (nm.rfind("Create", 0) == 0 || nm == "GetBackBuffer" ||
                    nm == "GetTexture" || nm == "GetSwapChain" || nm == "GetDirect3D" || nm == "GetIndices" || nm == "GetVertexDeclaration" ||
                    nm == "GetVertexShader" || nm == "GetPixelShader" || nm == "EndStateBlock")
                    throw VmError("null D3D device: object-returning method " + nm + " during update");
                return m.ret(bytes, nm == "AddRef" || nm == "Release" ? 1 : 0);
            };
        }
        m.mem.map(DEVICE_VTABLE, reinterpret_cast<uint8_t*>(vt.data()), 119 * 4, true);
        // Null surface: QueryInterface/AddRef/Release only.
        uint32_t surface[4] = {NULL_SURFACE + 0x10, 0, 0, 0}, svt[3] = {NULL_SURFACE + 0x40, NULL_SURFACE + 0x44, NULL_SURFACE + 0x48};
        std::vector<uint8_t> page(0x100, 0); std::memcpy(page.data(), surface, 16); std::memcpy(page.data() + 0x10, svt, 12);
        m.mem.map(NULL_SURFACE, page.data(), page.size(), true);
        m.natives[NULL_SURFACE + 0x40] = [](Machine& m) { return m.ret(12, 0x80004002u); };
        m.natives[NULL_SURFACE + 0x44] = [](Machine& m) { return m.ret(4, 1); };
        m.natives[NULL_SURFACE + 0x48] = [](Machine& m) { return m.ret(4, 0); };
        uint32_t renderer = uint32_t(m.mem.read(RENDERER_GLOBAL, 4));
        if (renderer && m.mem.mapped(renderer + 0x1ec)) s->live_device = uint32_t(m.mem.read(renderer + 0x1ec, 4)), m.mem.write(renderer + 0x1ec, 4, DEVICE_OBJECT);
    }
    // Terrain GPU sync (HFTerrainSurface::vf14, fastcall, called from SyncRenderData):
    // renders the wheel ruts into the deformation render targets, reads them back
    // into the collision surface and rebuilds the terrain patch vertex buffers. There
    // is no GPU here: the deformation surface stays as captured (no new ruts).
    // With the deformation model loaded, vf14 runs as original code over the null device:
    // its readback (GetRenderTargetData into the physics surface) copies the CPU render
    // target, its queue/index flips are the game's own, and the render of this frame's
    // stamp queue (FUN_007caf70 after Sync) happens in place of the first decal-mesh
    // rebuild call (FUN_007cbe30, visual tire tracks, not reproduced). Without the model,
    // vf14 is skipped (no new ruts).
    m.natives[TERRAIN_GPU_SYNC] = [s](Machine& m) -> uint32_t {
        if (!s->deform_model.loaded()) return m.ret(0);
        uint32_t surface = m.r[1];
        if (surface == s->terrain) {
            if (m.mem.read(surface + 0x44b8, 4)) {   // level reset: both render targets cleared to 0x80808080
                std::fill(s->deform_rt.begin(), s->deform_rt.end(), NEUTRAL_DEFORM);
                s->rt_unread = DeformRect{0, 0, int(s->deform_n), int(s->deform_n)};
            }
            s->deform_render_due = true;
        }
        { const Native* p = reflex_port::installed(m, TERRAIN_GPU_SYNC); return p ? (*p)(m) : RUN_ORIGINAL; }   // a native/game port, else the original
    };
    // Tyre-track decals (TerrainTracks, _re/notes/rt-bump.md section 4): the live VB/IB are GPU
    // objects of the captured process, so each instance gets memory-backed buffer stand-ins
    // (Lock returns emulated memory) and the original FlushPending writes the exact ribbon
    // vertices/indices there; reflex_world_tracks reads them back the way TerrainTracks_Draw does.
    {
        // vtable: QI, AddRef, Release, GetDevice, SetPrivateData, GetPrivateData, FreePrivateData,
        // SetPriority, GetPriority, PreLoad, GetType, Lock, Unlock, GetDesc (stdcall, this first).
        static const uint32_t pop[14] = {12, 4, 4, 8, 20, 16, 8, 8, 4, 4, 4, 20, 4, 8};
        uint32_t vt = TRACK_BUFFERS, code = TRACK_BUFFERS + 0x100, next = TRACK_BUFFERS + 0x1000;
        std::vector<uint32_t> table(14);
        for (uint32_t i = 0; i < 14; i++) {
            table[i] = code + 4 * i;
            if (i == 11) m.natives[table[i]] = [](Machine& m) {   // Lock(off, size, void** data, flags)
                m.mem.write(m.arg(3), 4, uint32_t(m.mem.read(m.arg(0) + 4, 4)) + m.arg(1)); return m.ret(20, 0); };
            else if (i == 12) m.natives[table[i]] = [s](Machine& m) { if (m.mem.read(m.arg(0) + 12, 4)) s->tracks_version++; return m.ret(4, 0); };
            else m.natives[table[i]] = [i](Machine& m) { return m.ret(pop[i], i == 1 || i == 2 ? 1 : 0); };
        }
        m.mem.map(vt, reinterpret_cast<uint8_t*>(table.data()), 14 * 4, true);
        for (uint32_t g : TERRAIN_TRACKS) {
            uint32_t tt = uint32_t(m.mem.read(g, 4));
            if (!tt || !m.mem.mapped(tt) || !m.mem.mapped(tt + 0x37)) continue;
            uint32_t cap = uint32_t(m.mem.read(tt + 8, 4)), slots = uint32_t(m.mem.read(tt + 0xc, 4));
            if (cap == 0 || cap > 64 || slots == 0 || slots > 64) continue;
            for (int vb = 0; vb < 2; vb++) {
                uint32_t bytes = cap * slots * (vb ? 0x800u : 0x200u);
                uint32_t object = next, data = next + 0x1000; next = (data + bytes + 0xfff) & ~0xfffu;
                uint32_t obj[4] = {vt, data, bytes, uint32_t(vb)};
                m.mem.map(object, reinterpret_cast<uint8_t*>(obj), 16, true);
                m.mem.map(data, nullptr, bytes, false);   // writable, zero (ensure() gives zeroed pages)
                m.mem.write(tt + (vb ? 0x14 : 0x10), 4, object);
            }
        }
    }
    m.natives[TRACKS_REBUILD] = [s](Machine& m) -> uint32_t {
        if (s->deform_render_due) { s->deform_render_due = false; s->render_deform(m); }
        static const bool skip = std::getenv("REFLEX_NO_TRACKS") != nullptr;   // diagnostics: old behaviour (flush not run)
        if (skip) return m.ret(0);
        { const Native* p = reflex_port::installed(m, TRACKS_REBUILD); return p ? (*p)(m) : RUN_ORIGINAL; }   // a native/game port, else the original
    };
    // Events created while frames run (synchronous audio commands): fresh handles.
    m.imports["kernel32.dll!CreateEventA"] = [s](Machine& m) { return m.ret(16, s->next_event += 4); };
    m.imports["kernel32.dll!CloseHandle"] = [s](Machine& m) { s->signaled.erase(m.arg(0)); return m.ret(4, 1); };
    // Critical sections created/destroyed while frames run (single-threaded): a fresh
    // RTL_CRITICAL_SECTION is {DebugInfo 0, LockCount -1, RecursionCount 0, owner 0, semaphore 0, spin}.
    auto init_cs = [](Machine& m, uint32_t cs, uint32_t spin) {
        uint32_t v[6] = {0, 0xffffffffu, 0, 0, 0, spin}; m.mem.write_bytes(cs, v, sizeof v);
    };
    m.imports["kernel32.dll!InitializeCriticalSection"] = [init_cs](Machine& m) { init_cs(m, m.arg(0), 0); return m.ret(4); };
    m.imports["kernel32.dll!InitializeCriticalSectionAndSpinCount"] = [init_cs](Machine& m) { init_cs(m, m.arg(0), m.arg(1)); return m.ret(8, 1); };
    m.imports["kernel32.dll!DeleteCriticalSection"] = [s](Machine& m) {
        auto it = s->sections_held.find(m.arg(0));
        if (it != s->sections_held.end() && it->second) throw VmError("DeleteCriticalSection while held");
        s->sections_held.erase(m.arg(0)); return m.ret(4);
    };
    m.imports["kernel32.dll!EnterCriticalSection"] = [s](Machine& m) { s->sections_held[m.arg(0)]++; return m.ret(4); };
    m.imports["kernel32.dll!LeaveCriticalSection"] = [s](Machine& m) {
        if (--s->sections_held[m.arg(0)] < 0) throw VmError("LeaveCriticalSection without Enter");
        return m.ret(4);
    };
    // ---- input manager and keyboard devices ----
    uint32_t core = uint32_t(m.mem.read(0xa95844, 4));
    s->manager = m.call_function(uint32_t(m.mem.read(uint32_t(m.mem.read(core, 4)) + 8, 4)), nullptr, 0, core);
    for (int i = 0; i < 4; i++) {
        uint32_t block = s->manager + 0xb74 + i * 0x9b0;
        if (m.mem.read(block + 0x99c, 4)) s->keyboards.insert(uint32_t(m.mem.read(block + 0x9ac, 4)));
    }
    int n = 0;
    for (uint32_t device : s->keyboards) {
        uint32_t table_address = 0xfd002000 + 0x100 * n++;
        uint32_t get_addr = table_address + 0x80, acquire_addr = table_address + 0x90;
        uint8_t slots[0x80];
        for (int k = 0; k < 0x20; k++) { uint32_t trap = 0xfd0f0000 + 4 * k; std::memcpy(slots + 4 * k, &trap, 4); }
        std::memcpy(slots + 0x24, &get_addr, 4); std::memcpy(slots + 0x1c, &acquire_addr, 4);
        m.mem.map(table_address, slots, sizeof slots, true);
        m.mem.write(device, 4, table_address);
        m.natives[get_addr] = [s, device](Machine& m) {
            uint32_t self = m.arg(0), size = m.arg(1), data = m.arg(2);
            if (self != device || size != 0x100) throw VmError("GetDeviceState ABI");
            m.mem.write_bytes(data, s->keys, 0x100);
            return m.ret(12, 0);
        };
        m.natives[acquire_addr] = [device](Machine& m) { if (m.arg(0) != device) throw VmError("Acquire ABI"); return m.ret(4, 0); };
    }
    // ---- XInput gamepads ----
    m.imports["xinput1_3.dll!#2"] = [s](Machine& m) {          // XInputGetState
        uint32_t index = m.arg(0), out = m.arg(1);
        if (index >= 4 || !(s->pad_mask & (1u << index))) return m.ret(8, 1167);
        const WorldPad& p = s->pads[index];
        m.mem.write(out, 4, ++s->packet); m.mem.write(out + 4, 2, p.buttons); m.mem.write(out + 6, 1, p.left_trigger); m.mem.write(out + 7, 1, p.right_trigger);
        m.mem.write(out + 8, 2, uint16_t(p.lx)); m.mem.write(out + 10, 2, uint16_t(p.ly)); m.mem.write(out + 12, 2, uint16_t(p.rx)); m.mem.write(out + 14, 2, uint16_t(p.ry));
        return m.ret(8, 0);
    };
    m.imports["xinput1_3.dll!#3"] = [s](Machine& m) { uint32_t i = m.arg(0); return m.ret(8, (i < 4 && (s->pad_mask & (1u << i))) ? 0 : 1167); };
    m.imports["xinput1_3.dll!#4"] = [s](Machine& m) {          // XInputGetCapabilities
        uint32_t index = m.arg(0), out = m.arg(2);
        if (index >= 4 || !(s->pad_mask & (1u << index))) return m.ret(12, 1167);
        const uint8_t caps[] = {1, 1, 0, 0, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xc0, 0xff, 0xc0, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff};
        m.mem.write_bytes(out, caps, sizeof caps);
        return m.ret(12, 0);
    };
    m.imports["xinput1_3.dll!#8"] = [s](Machine& m) { uint32_t i = m.arg(0); return m.ret(12, (i < 4 && (s->pad_mask & (1u << i))) ? 0x10d2 : 1167); };
    // ---- mouse/window queries: fixed cursor, 1280x720 client ----
    m.imports["user32.dll!GetCursorPos"] = [](Machine& m) { uint32_t out = m.arg(0); m.mem.write(out, 4, 0); m.mem.write(out + 4, 4, 0); return m.ret(4, 1); };
    m.imports["user32.dll!GetClientRect"] = [](Machine& m) { uint32_t out = m.arg(1); uint32_t v[4] = {0, 0, 1280, 720}; m.mem.write_bytes(out, v, 16); return m.ret(8, 1); };
    m.imports["user32.dll!ScreenToClient"] = [](Machine& m) { return m.ret(8, 1); };
    m.imports["user32.dll!ClientToScreen"] = [](Machine& m) { return m.ret(8, 1); };
    m.imports["user32.dll!SetCursorPos"] = [](Machine& m) { return m.ret(8, 1); };
}

void World::frame(uint32_t dt_bits, const uint8_t keys[256], const WorldPad* pads, uint32_t pad_mask) {
    Machine& m = d->m;
    std::memcpy(d->keys, keys, 256);
    d->pad_mask = pad_mask;
    for (int i = 0; i < 4; i++) if (pads) d->pads[i] = pads[i];
    m.fpu_cw = d->cw; m.mxcsr = d->mxcsr;
    uint32_t vf43 = uint32_t(m.mem.read(uint32_t(m.mem.read(d->manager, 4)) + 0xac, 4));
    m.call_function(vf43, &dt_bits, 1, d->manager);
    m.call_function(0x769b80, &dt_bits, 1, -1, d->context);
    m.call_function(ANIM_KICK, nullptr, 0);
    m.call_function(ANIM_WAIT, nullptr, 0);
    for (auto& [cs, held] : d->sections_held) if (held) throw VmError("unbalanced critical section");
}

// One whole original frame: Engine::Update(engine) (timing, input, scene tickers,
// MXGame::Update -> world update and physics, animation jobs, ...), rendering
// excluded. ticks: elapsed time since the previous frame in 100 ns units.
void World::engine_frame(uint64_t ticks, const uint8_t keys[256], const WorldPad* pads, uint32_t pad_mask) {
    Machine& m = d->m;
    if (!d->engine) throw VmError("world image is not a frame-boundary dump");
    if (d->fixed_clock && !d->replay) ticks = d->fixed_dt;   // REFLEX_FIXED_CLOCK: one 1/60 s frame, host-independent
    std::memcpy(d->keys, keys, 256);
    d->pad_mask = pad_mask;
    for (int i = 0; i < 4; i++) if (pads) d->pads[i] = pads[i];
    if (!d->qpc_started) {          // continue the live clock: the engine's last counter value
        d->qpc = m.mem.read(d->engine + 0x68, 8); d->qpc_started = true;
    }
    d->qpc += ticks; d->qpc_calls = 0;
    m.fpu_cw = d->cw; m.mxcsr = d->mxcsr;
    // One-shot events the audio client finished playing: the game's EVENT_FINISHED callback,
    // only if the wrapper has not been restarted/stopped since (serial).
    for (auto [w, serial] : d->audio_done) {
        auto it = d->audio_serial.find(w);
        if (it == d->audio_serial.end() || it->second != serial) continue;
        uint32_t a[5] = {uint32_t(m.mem.read(w + 0x118, 4)), 4, 0, 0, w};
        m.mem.write(w + 0x13c, 4, 1);
        m.call_function(0x80b7d0, a, 5);
    }
    d->audio_done.clear();
    m.call_function(0x84ed20, &d->engine, 1);
    // REFLEX_FRAME_TAIL=1 (w-frametail, opt-in): the game-state tail of Engine::Frame 0084f260 after the update, the render excluded
    // as above: the update counter +0x58 + 1, Database vf33 0084cc80 (the end-of-frame removal of the handles queued for removal, the
    // render counter +0x54 as argument), +0x54 + 1. Without it a world test keeps both counters at the capture's value and never
    // drains the Database's pending lists (the original's frame and the product's do both).
    static const bool tail = std::getenv("REFLEX_FRAME_TAIL") && std::atoi(std::getenv("REFLEX_FRAME_TAIL")) != 0;
    if (tail) {
        m.mem.write(d->engine + 0x58, 4, uint32_t(m.mem.read(d->engine + 0x58, 4)) + 1);
        const uint32_t ctx = uint32_t(m.mem.read(0x00a95844, 4)), db = uint32_t(m.mem.read(ctx + 8, 4));
        const uint32_t vf33 = uint32_t(m.mem.read(uint32_t(m.mem.read(db, 4)) + 0x84, 4));
        const uint32_t frame = uint32_t(m.mem.read(d->engine + 0x54, 4));
        m.call_function(vf33, &frame, 1, db);
        m.mem.write(d->engine + 0x54, 4, uint32_t(m.mem.read(d->engine + 0x54, 4)) + 1);
    }
    for (auto& [cs, held] : d->sections_held) if (held) throw VmError("unbalanced critical section");
}

// Whole frame at an absolute performance-counter value (replaying a recorded live frame:
// the game's first counter read in the frame returns exactly this value).
void World::engine_frame_at(uint64_t counter, const uint8_t keys[256], const WorldPad* pads, uint32_t pad_mask) {
    if (!d->engine) throw VmError("world image is not a frame-boundary dump");
    d->qpc_started = true;
    d->qpc = counter;
    d->replay = true;
    try { engine_frame(0, keys, pads, pad_mask); } catch (...) { d->replay = false; throw; }
    d->replay = false;
}

uint32_t World::header_dt() const { return d->header_dt; }
// Ambient animation (decorative scenery animation: crowds, crew chiefs, looping and spline
// animations, scenario animations). Nothing in gameplay reads them; skipping their per-frame update
// (vf19, matched by the object's class so code-folded functions shared with other classes still run)
// removes most of a national's frame. Unity draws them from ambient.cpp's export, keeping the classes
// it draws with set_ambient_mask.
void World::set_ambient_animation(bool simulate) {
    static const uint32_t updates[] = {0x6ba250, 0x74ab50, 0x6bb340, 0x6bd6e0, 0x4137b0, 0x6ba520, 0x6bc040,
        0x6d06a0, 0x6c6650, 0x6c1a00};
    static const uint32_t classes[] = {0x95f674 /* AnimEntitySingleAnimLoop */, 0x96cee4 /* ScenarioAnimEntity */,
        0x95f83c /* AnimEntitySplineAnimLoop */, 0x95f4e4 /* CrewChief */, 0x939d24 /* InstancedAnimationEntity */,
        0x95f73c /* AnimEntitySingleAnimLoopWithParticles */, 0x95f904 /* AnimEntitySplineAnimLoopWithParticles */,
        // supercross / stadium decoration: proximity-animated characters, billboard crowds, the score-tower flag holder
        // (vf19 0x6d06a0, 0x6c6650, 0x6c1a00). Not the placed particle effects / fireworks: they draw from the CRT rand()
        // sequence gameplay shares, so skipping them changes the race (verified by the bike state hash).
        0x960dec /* LandAnimalProximity */, 0x960654 /* BillboardCrowdEntity */, 0x960094 /* ScoreTowerFlagHolder */};
    Machine& m = d->m;
    // (reflex_port::runtime_native: a native/game port at the same address stays installed underneath; the skip falls through to it.)
    for (uint32_t a : updates) {
        if (simulate) { reflex_port::runtime_native(m, a, nullptr); continue; }
        static const uint32_t mask = std::getenv("REFLEX_SKIP_MASK") ? uint32_t(std::strtoul(std::getenv("REFLEX_SKIP_MASK"), nullptr, 16)) : 0xffffffffu;   // diagnostics
        // ambient_keep (set_ambient_mask, ambient.cpp): classes whose bit is set keep running (those Unity draws).
        const uint32_t* keep = &d->ambient_keep;
        reflex_port::runtime_native(m, a, [keep, a](Machine& m) -> uint32_t {
            uint32_t vt = uint32_t(m.mem.read(m.r[1], 4));
            for (size_t i = 0; i < sizeof classes / sizeof classes[0]; i++) if (vt == classes[i] && (mask >> i & 1) && !(*keep >> i & 1)) return m.ret(4);
            const reflex_vm::Native* port = reflex_port::installed(m, a);
            return port ? (*port)(m) : RUN_ORIGINAL;
        });
    }
    // Particle systems (the particle manager update 00698510, thiscall (dt), which submits Particle_UpdateJob to the
    // engine job pool and waits for it): dust, roost, pyrotechnics. Drawn only by the original renderer; gameplay never
    // reads them (bike state verified bit-identical with and without).
    if (simulate || d->particles) reflex_port::runtime_native(m, 0x698510, nullptr); else reflex_port::runtime_native(m, 0x698510, [](Machine& m) { return m.ret(4); });
    // Render-sync worker phase of Render_SyncRenderData (007e0fc0): after the main-thread objects (terrain deformation sync,
    // tyre-track flush), the 4 render-sync workers (007df370) and 007da080 evaluate render nodes for the original renderer.
    // Without it: the state the workers leave (render world +0x11d0 = 0x7fffffff, +0x11d4 = 0) is set directly.
    if (simulate || std::getenv("REFLEX_KEEP_RENDER_SYNC")) m.natives.erase(0x7e1082);
    else m.natives[0x7e1082] = [](Machine& m) -> uint32_t {
        uint32_t rw = m.r[6];
        if (uint32_t(m.mem.read(rw + 0x11dc, 4)) + uint32_t(m.mem.read(rw + 0x11d4, 4)) != 0) { m.mem.write(rw + 0x11d0, 4, 0x7fffffffu); m.mem.write(rw + 0x11d4, 4, 0); }
        return 0x7e1101;
    };
    d->ambient = simulate;
}
bool World::ambient_animation() const { return d->ambient; }
// Per-class exception to the skips above (bit i = classes[i]; reflex_world.h REFLEX_AMBIENT_*): only the skipped
// update is restored, so the native addresses stay as installed (blocks the JIT already translated stay valid).
void World::set_ambient_mask(uint32_t keep) { d->ambient_keep = keep; }
uint32_t World::ambient_mask() const { return d->ambient_keep; }
// Particles alone (particles.cpp exports them): the manager update runs while the other ambient skips stay in place.
void World::set_particles(bool simulate) { d->particles = simulate; set_ambient_animation(d->ambient); }
bool World::particles_simulated() const { return d->ambient || d->particles; }

uint32_t World::sim() const { return d->sim; }

// Selected terrain (the object the collision code uses) and its collision heights.
bool World::terrain_info(TerrainInfo& t) {
    if (!d->terrain) return false;
    Machine& m = d->m; uint32_t tr = d->terrain;
    auto f = [&](uint32_t off) { uint32_t b = uint32_t(m.mem.read(tr + off, 4)); float v; std::memcpy(&v, &b, 4); return v; };
    t.samples = uint32_t(m.mem.read(tr + 0x44c0, 4)); t.stride = uint32_t(m.mem.read(tr + 0x44bc, 4));
    t.spacing = f(0x44e8); t.min_x = f(0x44d8); t.min_z = f(0x44dc); t.max_x = f(0x44e0); t.max_z = f(0x44e4);
    t.gain = f(0xbc); t.deform_version = d->deform_version;
    t.dirty_x0 = d->dirty_x0; t.dirty_z0 = d->dirty_z0; t.dirty_x1 = d->dirty_x1; t.dirty_z1 = d->dirty_z1;
    return true;
}
void World::clear_terrain_dirty() { d->dirty_x0 = d->dirty_z0 = 0x7fffffff; d->dirty_x1 = d->dirty_z1 = -1; }
// Collision heights for grid columns [x0, x0+w) and rows [z0, z0+h) (row 0 = max z):
// base * 2048 + (deformation - 0.5) * 2 * gain, the terrain the physics collides with.
bool World::terrain_heights(float* out, int x0, int z0, int w, int h) {
    TerrainInfo t; if (!terrain_info(t)) return false;
    Machine& m = d->m;
    uint32_t base = uint32_t(m.mem.read(d->terrain + 0x1c, 4));
    if (x0 < 0 || z0 < 0 || w <= 0 || h <= 0 || uint32_t(x0 + w) > t.samples || uint32_t(z0 + h) > t.samples) return false;
    std::vector<float> row(static_cast<size_t>(w)), def(static_cast<size_t>(w));
    for (int r = 0; r < h; r++) {
        m.mem.read_block(base + 4 * (uint32_t(z0 + r) * t.stride + uint32_t(x0)), row.data(), size_t(w) * 4);
        m.mem.read_block(DEFORM_BITS + 4 * (uint32_t(z0 + r) * d->deform_n + uint32_t(x0)), def.data(), size_t(w) * 4);
        for (int c = 0; c < w; c++) out[size_t(r) * size_t(w) + size_t(c)] = row[size_t(c)] * 2048.0f + (def[size_t(c)] - 0.5f) * 2.0f * t.gain;
    }
    return true;
}
bool World::terrain_deform(float* out, int x0, int z0, int w, int h) {
    TerrainInfo t; if (!terrain_info(t)) return false;
    if (x0 < 0 || z0 < 0 || w <= 0 || h <= 0 || uint32_t(x0 + w) > d->deform_n || uint32_t(z0 + h) > d->deform_n) return false;
    for (int r = 0; r < h; r++)
        d->m.mem.read_block(DEFORM_BITS + 4 * (uint32_t(z0 + r) * d->deform_n + uint32_t(x0)), out + size_t(r) * size_t(w), size_t(w) * 4);
    return true;
}
// Tyre-track triangles exactly as TerrainTracks_Draw (007cc4f0) submits them: for every allocated
// slot (+0x94 == 0) and live chunk (generation != 0), a 254-triangle strip from the chunk's index
// start over vertices at the chunk's VB offset. Per vertex: world x, z (SHORT4N * extent/2), side
// (0/1), V (32*|w|), intensity |s|, texture (slot type when s < 0, else 0 = generic).
void World::set_audio(bool enabled) { d->audio = enabled; if (!enabled) d->audio_out.clear(); }
size_t World::drain_audio(AudioCommand* out, size_t max) {
    size_t n = std::min(max, d->audio_out.size());
    std::copy(d->audio_out.begin(), d->audio_out.begin() + long(n), out);
    d->audio_out.erase(d->audio_out.begin(), d->audio_out.begin() + long(n));
    return n;
}
void World::audio_finished(uint32_t wrapper, uint32_t serial) { d->audio_done.push_back({wrapper, serial}); }
uint32_t World::tracks_version() const { return d->tracks_version; }
size_t World::tracks(std::vector<float>& out) {
    out.clear();
    Machine& m = d->m;
    if (!d->terrain) return 0;
    auto f = [&](uint32_t a) { uint32_t b = uint32_t(m.mem.read(a, 4)); float v; std::memcpy(&v, &b, 4); return v; };
    float hx = f(d->terrain + 0x44d0) * 0.5f, hz = f(d->terrain + 0x44d4) * 0.5f;
    for (uint32_t g : TERRAIN_TRACKS) {
        uint32_t tt = uint32_t(m.mem.read(g, 4));
        if (!tt) continue;
        uint32_t ib = uint32_t(m.mem.read(tt + 0x10, 4)), vb = uint32_t(m.mem.read(tt + 0x14, 4));
        if (ib < TRACK_BUFFERS || vb < TRACK_BUFFERS) continue;   // not ours
        uint32_t ib_data = uint32_t(m.mem.read(ib + 4, 4)), vb_data = uint32_t(m.mem.read(vb + 4, 4));
        uint32_t ring = uint32_t(m.mem.read(tt + 4, 4)), slots = uint32_t(m.mem.read(tt + 0xc, 4));
        uint32_t stride = uint32_t(m.mem.read(tt + 0x1c, 4)), prims = uint32_t(m.mem.read(tt + 0x20, 4));
        uint32_t slot_array = uint32_t(m.mem.read(tt + 0x24, 4));
        if (stride != 8 || prims > 1024) continue;
        for (uint32_t sidx = 0; sidx < slots; sidx++) {
            uint32_t slot = slot_array + 0xc0 * sidx;
            if (m.mem.read(slot + 0x94, 4)) continue;
            uint32_t chunks = uint32_t(m.mem.read(slot, 4)), gens = uint32_t(m.mem.read(slot + 0xc, 4));
            float type = float(uint32_t(m.mem.read(slot + 0xb4, 4)));
            for (uint32_t c = 0; c < ring; c++) {
                if (!m.mem.read(gens + 4 * c, 4)) continue;
                uint32_t vb_off = uint32_t(m.mem.read(chunks + 12 * c, 4)), start = uint32_t(m.mem.read(chunks + 12 * c + 4, 4));
                std::vector<uint16_t> idx(prims + 2);
                m.mem.read_bytes(ib_data + 2 * start, idx.data(), idx.size() * 2);
                auto vertex = [&](uint16_t i) {
                    int16_t v[4]; m.mem.read_bytes(vb_data + vb_off + 8u * i, v, 8);
                    auto n = [](int16_t x) { return std::max(-1.0f, float(x) / 32767.0f); };
                    out.push_back(n(v[0]) * hx); out.push_back(n(v[1]) * hz);
                    out.push_back(v[3] >= 0 ? 1.0f : 0.0f); out.push_back(32.0f * std::fabs(n(v[3])));
                    out.push_back(std::fabs(n(v[2]))); out.push_back(v[2] < 0 ? type : 0.0f);
                };
                // Strip indices can reach vertices of the chunk that were never written (all zero);
                // a triangle touching one would span to the terrain centre, so it is left out.
                auto unwritten = [&](uint16_t i) { uint64_t raw = m.mem.read(vb_data + vb_off + 8u * i, 8); return raw == 0; };
                for (uint32_t k = 0; k < prims; k++) {
                    uint16_t a = idx[k], b = idx[k + 1], cc = idx[k + 2];
                    if (a == b || b == cc || a == cc) continue;
                    if (unwritten(a) || unwritten(b) || unwritten(cc)) continue;
                    vertex(a); vertex(b); vertex(cc);
                }
            }
        }
    }
    return out.size() / 18;
}
uint32_t World::rider() const { return d->rider; }
bool World::whole_frames() const { return d->engine != 0; }
Machine& World::machine() { return d->m; }
uint32_t World::resource() const { return d->resource; }
const std::set<uint32_t>& World::keyboards() const { return d->keyboards; }

}  // namespace reflex_vm
