// C API of libreflexworld (reflex_world.h) over reflex_vm::World.
#include <cstdlib>
#include "reflex_world.h"
#include "port.h"
#include "runtime.h"
#include "boot.h"
#include "ambient.h"
#include "props.h"
#include "atv.h"
#include "mud.h"
#include "career.h"
#include "persist.h"
#include "modes.h"
#include "input.h"
#include "garage.h"
#include <atomic>
#include <mutex>
#include "vm.h"
#include <cstring>
#include <algorithm>
#include <string>

namespace {
using namespace reflex_vm;

struct Handle {
    World world;
    uint32_t sim = 0;          // RavenSimulationMotocross (bike) object
    uint32_t frames = 0;
    double tick_error = 0;
    bool failed = false;
    std::string error;
    AmbientScene ambient;      // ambient.cpp: the ambient entities found in the world
    PropScene props;           // props.cpp: the physics props and animated textures found in the world
    input::State input;        // input.cpp: Raw Input mouse, XInputSetState rumble
};
std::string create_error;

// The vehicle simulation: the object whose vtable is RavenSimulationMotocross's with body count (S+0xec) 4 (bike, rider,
// front wheel, rear wheel), or RavenSimulationATV's with 6 (frame, rider, four wheels; atv.h), or a truck's / buggy's / UTV's with 5 (vehicles.h).
uint32_t find_sim(Machine& m) {
    for (uint32_t page = 0x400000; page < 0x20000000; page += 0x1000) {
        uint8_t* p = m.mem.page(page);
        if (!p) continue;
        for (uint32_t off = 0; off < 0x1000; off += 4) {
            uint32_t v; std::memcpy(&v, p + off, 4);
            if (!vehicle_class_of_sim(v)) continue;   // any factory class (vehicles.h)
            uint32_t s = page + off;
            if (m.mem.mapped(s + 0xec) && vehicle_sim(v, uint32_t(m.mem.read(s + 0xec, 4)))) return s;
        }
    }
    return 0;
}
}  // namespace

extern "C" {

void* reflex_world_create(const char* exe_path, const char* crt_path, const char* world_path, const char* deform_path) {
    auto* h = new Handle;
    try {
        h->world.load(exe_path, crt_path, world_path, deform_path);
        if (!std::getenv("REFLEX_NO_JIT")) h->world.machine().enable_jit(true);   // block JIT (exact; REFLEX_NO_JIT=1 interprets)
        h->sim = h->world.sim() ? h->world.sim() : find_sim(h->world.machine());
        input::install(h->world.machine(), h->input);   // WM_INPUT mouse, XInputSetState rumble (input.cpp)
        if (const char* ww = std::getenv("REFLEX_CLIENT_WATCH")) {   // diagnostics, as the CLI's REFLEX_WORLD_WATCH: "addr:file", every interpreted write covering addr
            auto& m = h->world.machine(); m.watch_address = uint32_t(std::strtoul(ww, nullptr, 16)); m.trace_out = std::fopen(std::strchr(ww, ':') + 1, "w");
            m.trace_from = m.trace_to = 0; m.enable_jit(false);
        }
        if (!h->sim) throw VmError("bike simulation not found in the world image");
        create_error.clear();
        return h;
    } catch (const std::exception& e) {
        create_error = e.what();
        delete h;
        return nullptr;
    }
}

static std::mutex build_lock;
static std::string build_error, build_status;
static std::atomic<float> build_progress{0};
int reflex_world_build(const char* game_dir, const char* crt_dir, const char* save_dir, const ReflexEventSpec* event, const char* out_prefix) {
    { std::lock_guard<std::mutex> g(build_lock); build_error.clear(); build_status = "starting the original game"; }
    build_progress = 0;
    try {
        BootConfig c; c.game_dir = game_dir; c.crt_dir = crt_dir; c.save_dir = save_dir; c.jit = !std::getenv("REFLEX_NO_JIT");
        client_boot_config(c);   // baton scheduling (boot.h; native/game notes/loading.md section 9)
        std::string prefix = out_prefix; c.log_path = prefix + "-boot.log";
        bool career_event = event && event->garage == REFLEX_GARAGE_SET && (event->career == REFLEX_CAREER_SET || event->career == REFLEX_CAREER_SET2);   // MOTOCAREER (career.h)
        if (career_event) { c.remote_storage = std::string(save_dir) + "/remote"; c.steam_persona = career::persona(save_dir); }
        if (event && event->garage == REFLEX_GARAGE_SET && event->test_drive == REFLEX_TESTDRIVE_SET) { c.remote_storage = std::string(save_dir) + "/remote"; c.steam_persona = career::persona(save_dir); }   // WP-G test drive
        Boot boot(c);
        struct CoverageGuard { Machine& m; ~CoverageGuard() { client_coverage_forget(m); } } coverage_guard{boot.machine()};
        install_client_boot_ports(boot, c);   // the native game for the whole boot (reflex_world_boot_ports)
        boot.start();
        Boot::EventSpec e;
        if (event) {
            e.type = event->type; e.location = event->location; e.duration_type = event->duration_type;
            e.type_id = event->type_id; e.location_id = event->location_id; e.track_id = event->track_id; e.vehicle_class = event->vehicle_class;
            e.riders = event->riders; e.ai = event->ai; e.duration = event->duration; e.activity = event->activity;
            if (event->garage == REFLEX_GARAGE_SET) {   // Vehicle Select
                e.rider_owner = event->rider_owner; e.rider_id = event->rider_id; e.rider_gear_set = event->rider_gear_set;
                e.vehicle_owner = event->vehicle_owner; e.vehicle_id = event->vehicle_id; e.vehicle_type = event->vehicle_type;
                if (career_event) { e.series_id = event->series_id; e.series_vehicle_type = event->series_vehicle_type; }
                if (event->modes == REFLEX_MODES_SET) {   // game modes (modes.h)
                    char act[33] = {}; std::memcpy(act, event->activation, 32); e.activation = act;
                    e.series_challenge = event->series_challenge; e.exclusive_device = event->exclusive_device; e.cursor_off_screen = event->cursor_off_screen != 0;
                }
                if (career_event && event->career == REFLEX_CAREER_SET2) {
                    e.series_skip_practice = event->series_skip_practice;
                    if (event->series_outcomes > 0 && !career::read_outcomes(std::string(save_dir) + "/career_outcomes.bin", event->series_outcomes, e.series_outcomes))
                        throw VmError("career_outcomes.bin does not hold " + std::to_string(event->series_outcomes) + " race outcomes");
                    career::read_gates(std::string(save_dir) + "/career_gates.bin", e.series_gates);   // WP-5: the Gate Selection page's choices
                }
            }
        }
        { std::lock_guard<std::mutex> g(build_lock); build_status = "loading " + e.location + " through the game's front end"; }
        std::string log;
        boot.progress = [&](uint32_t frames, int state) {   // front end ~250 frames, loading (states 3..7) ~250 more
            build_progress = std::min(0.95f, float(frames) / (c.baton ? 430.0f : 560.0f));   // baton: front end ~46 frames, its settle to 5 s, loading ~35, the logo video
            std::lock_guard<std::mutex> g(build_lock);
            build_status = state >= 3 && state <= 11 ? "loading " + e.location + " (game state " + std::to_string(state) + ")" : state == 2 && frames > 300 ? "starting the race" : "starting the original game";
        };
        bool test_drive = event && event->garage == REFLEX_GARAGE_SET && event->test_drive == REFLEX_TESTDRIVE_SET;   // WP-G (garage.h)
        if (test_drive) { if (!garage::start_test_drive(boot, e, 6000, &log)) throw VmError("the test drive did not start:\n" + log); }
        else if (!boot.start_event(e, 20000, &log)) throw VmError("the event did not start:\n" + log);
        { std::lock_guard<std::mutex> g(build_lock); build_status = "writing the world"; }
        build_progress = 0.96f;
        boot.write_world(prefix + "-world.bin", prefix + "-deform.bin", prefix + "-deform-model.bin", &log);
        client_coverage_write(boot.machine(), "build"); client_coverage_forget(boot.machine());   // REFLEX_CLIENT_COVERAGE (boot.h)
        if (FILE* lf = std::fopen((prefix + "-build.log").c_str(), "w")) { std::fputs(log.c_str(), lf); std::fclose(lf); }   // WP-5: the event's start log (the gate selections' lines for the Gate Selection page)
        build_progress = 1;
        { std::lock_guard<std::mutex> g(build_lock); build_status = "done"; }
        return 0;
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> g(build_lock); build_error = e.what(); build_status = "failed";
        return 1;
    }
}
float reflex_world_build_progress(char* status, int size) {
    if (status && size > 0) { std::lock_guard<std::mutex> g(build_lock); std::snprintf(status, size_t(size), "%s", build_status.c_str()); }
    return build_progress;
}
const char* reflex_world_build_error(void) { static std::string copy; std::lock_guard<std::mutex> g(build_lock); copy = build_error; return copy.c_str(); }

const char* reflex_world_error(void* world) {
    if (!world) return create_error.c_str();
    return static_cast<Handle*>(world)->error.c_str();
}

int reflex_world_frame(void* world, float dt, const uint8_t* keys, const ReflexPad* pads, uint32_t pad_mask) {
    auto* h = static_cast<Handle*>(world);
    if (!h || h->failed) return 1;
    static_assert(sizeof(ReflexPad) == sizeof(WorldPad), "pad layout");
    try {
        uint8_t none[256] = {};
        if (h->world.whole_frames()) {
            // Whole original frames: the game measures its own dt from the performance
            // counter, which advances by the real elapsed time (100 ns ticks).
            h->tick_error += double(dt) * 1e7;
            uint64_t ticks = uint64_t(h->tick_error); h->tick_error -= double(ticks);
            h->world.engine_frame(ticks, keys ? keys : none, reinterpret_cast<const WorldPad*>(pads), pads ? pad_mask : 0);
        } else {
            uint32_t bits; std::memcpy(&bits, &dt, 4);
            h->world.frame(bits, keys ? keys : none, reinterpret_cast<const WorldPad*>(pads), pads ? pad_mask : 0);
        }
        h->frames++;
        return 0;
    } catch (const std::exception& e) {
        h->error = e.what(); h->failed = true;
        return 2;
    }
}

int reflex_world_frame_at_counter(void* world, uint64_t counter, const uint8_t* keys, const ReflexPad* pads, uint32_t pad_mask) {
    auto* h = static_cast<Handle*>(world);
    if (!h || h->failed) return 1;
    try {
        uint8_t none[256] = {};
        h->world.engine_frame_at(counter, keys ? keys : none, reinterpret_cast<const WorldPad*>(pads), pads ? pad_mask : 0);
        h->frames++;
        return 0;
    } catch (const std::exception& e) { h->error = e.what(); h->failed = true; return 2; }
}

int reflex_world_bike(void* world, ReflexBikeState* out) {
    auto* h = static_cast<Handle*>(world);
    if (!h || !out) return 1;
    try {
        Memory& mem = h->world.machine().mem; uint32_t s = h->sim;
        std::memset(out, 0, sizeof *out);
        for (int i = 0; i < 4; i++) {
            uint32_t b = s + 0xf0 + 0xa0 * i;
            mem.read_bytes(b, out->position[i], 16);
            mem.read_bytes(b + 0x640, out->orientation[i], 16);
            mem.read_bytes(b + 0x90, out->render_position[i], 16);
            mem.read_bytes(b + 0x6d0, out->render_orientation[i], 16);
        }
        mem.read_bytes(s + 0x100, out->velocity, 16);
        mem.read_bytes(s + 0x94, &out->speed, 4);
        mem.read_bytes(s + sim_gear_offset(uint32_t(mem.read(s, 4))), &out->gear, 4);   // MX S+0x8ac0, ATV S+0x96f0 (atv.h)
        mem.read_bytes(s + 0x1c, out->inputs, 32);
        out->frames = h->frames;
        return 0;
    } catch (const std::exception& e) {
        h->error = e.what();
        return 2;
    }
}

// Rider bone world matrices. Physics writes the driven bones' world matrices
// (spine, arms, head, legs, entity root node 69) and leaves the others invalid
// (node record +0xc bit 0 clear); the renderer completes them with the game's lazy
// node evaluator 007db890 (thiscall node record, arg 0), run here for every node.
int reflex_world_rider_pose(void* world, float* out, uint32_t max_bones) {
    auto* h = static_cast<Handle*>(world);
    if (!h || !out || h->failed) return -1;
    try {
        Machine& m = h->world.machine(); uint32_t r = h->world.rider();
        if (!r) throw VmError("world image does not record the rider");
        uint32_t records = uint32_t(m.mem.read(r + 0x188, 4)), count = uint32_t(m.mem.read(r + 0x18c, 4));
        uint32_t world_matrices = uint32_t(m.mem.read(r + 0x19c, 4));
        if (count != 69) throw VmError("unexpected rider node count");
        uint32_t zero = 0;
        for (int i = int(count) - 1; i >= 0; i--) m.call_function(0x7db890, &zero, 1, records + 16 * uint32_t(i));
        uint32_t n = count < max_bones ? count : max_bones;
        m.mem.read_bytes(world_matrices, out, size_t(n) * 64);
        return int(n);
    } catch (const std::exception& e) {
        h->error = e.what(); h->failed = true;
        return -2;
    }
}

// The local player's vehicle model (RavenVehicleMotocross [P+0x1ac], model [V+0x1328], node
// hierarchy at model+0x10: records +8 (16 bytes), count +0xc, world matrices +0x1c; the node
// names and order are those of the MX_MarsF_Lite model: OBJ_Frame, OBJ_UPR_Forks, ...).
// Completed with the renderer's lazy node evaluator exactly like the rider.
static uint32_t local_player(Memory& m) {
    auto u32 = [&](uint32_t a) { return uint32_t(m.read(a, 4)); };
    uint32_t G = u32(0xa946b8);
    if (!G) return 0;
    uint32_t count = uint32_t(m.read(G + 0x1541c, 1));
    for (uint32_t i = 0; i < count && i < 8; i++) {
        uint32_t p = G + 0x7460 + u32(G + 0x153ec + 4 * i) * 0x12a0;
        if (u32(p + 0x10) == 0) return p;
    }
    return 0;
}

int reflex_world_bike_pose(void* world, float* out, uint32_t max_nodes) {
    auto* h = static_cast<Handle*>(world);
    if (!h || !out || h->failed) return -1;
    try {
        Machine& m = h->world.machine();
        auto u32 = [&](uint32_t a) { return uint32_t(m.mem.read(a, 4)); };
        uint32_t P = local_player(m.mem), V = P ? u32(P + 0x1ac) : 0, model = V ? u32(V + 0x1328) : 0;
        if (!model) return 0;
        uint32_t hier = model + 0x10, records = u32(hier + 8), count = u32(hier + 0xc), world_matrices = u32(hier + 0x1c);
        if (!records || !world_matrices || count == 0 || count > 64) return 0;
        uint32_t zero = 0;
        for (int i = int(count) - 1; i >= 0; i--) m.call_function(0x7db890, &zero, 1, records + 16 * uint32_t(i));
        uint32_t n = count < max_nodes ? count : max_nodes;
        m.mem.read_bytes(world_matrices, out, size_t(n) * 64);
        return int(n);
    } catch (const std::exception& e) {
        h->error = e.what(); h->failed = true;
        return -2;
    }
}

static uint32_t slot_player(Memory& m, int32_t slot) {
    uint32_t G = uint32_t(m.read(0xa946b8, 4));
    if (!G || slot < 0 || slot >= 16) return 0;
    uint32_t P = G + 0x7460 + uint32_t(slot) * 0x12a0;
    return m.read(P + 0x1ac, 4) ? P : 0;
}
static int model_pose(Machine& m, uint32_t model, float* out, uint32_t max_nodes) {
    auto u32 = [&](uint32_t a) { return uint32_t(m.mem.read(a, 4)); };
    if (!model) return 0;
    uint32_t hier = model + 0x10, records = u32(hier + 8), count = u32(hier + 0xc), world_matrices = u32(hier + 0x1c);
    if (!records || !world_matrices || count == 0 || count > 64) return 0;
    uint32_t zero = 0;
    for (int i = int(count) - 1; i >= 0; i--) m.call_function(0x7db890, &zero, 1, records + 16 * uint32_t(i));
    uint32_t n = count < max_nodes ? count : max_nodes;
    m.mem.read_bytes(world_matrices, out, size_t(n) * 64);
    return int(n);
}
int reflex_world_slots(void* world, ReflexSlot* out, uint32_t max) {
    auto* h = static_cast<Handle*>(world);
    if (!h || !out || h->failed) return -1;
    Memory& m = h->world.machine().mem; uint32_t local = local_player(m); int n = 0;
    for (int32_t k = 0; k < 16 && uint32_t(n) < max; k++) {
        uint32_t P = slot_player(m, k); if (!P) continue;
        ReflexSlot& s = out[n++];
        s.slot = k; s.type = int32_t(m.read(P + 0x10, 4)); s.local = P == local;
        s.rider_row = int32_t(m.read(P + 0x1b8, 4)); s.vehicle_row = int32_t(m.read(P + 0x1b0, 4));   // 00767200: P+0x1b0 <- RequestedVehicleId (P+0x1c4), P+0x1b8 <- RequestedRiderId (P+0x1c0)
        s.vehicle = uint32_t(m.read(P + 0x1ac, 4)); s.rider = uint32_t(m.read(P + 0x1b4, 4));
    }
    return n;
}
int reflex_world_bike_pose_slot(void* world, int32_t slot, float* out, uint32_t max_nodes) {
    auto* h = static_cast<Handle*>(world);
    if (!h || !out || h->failed) return -1;
    try {
        Machine& m = h->world.machine(); uint32_t P = slot_player(m.mem, slot);
        uint32_t V = P ? uint32_t(m.mem.read(P + 0x1ac, 4)) : 0;
        return model_pose(m, V ? uint32_t(m.mem.read(V + 0x1328, 4)) : 0, out, max_nodes);
    } catch (const std::exception& e) { h->error = e.what(); h->failed = true; return -2; }
}
// A slot's riding rider (RiderAnimation, vtable 0x958724, at P+0x1b4): 69 node world matrices, as reflex_world_rider_pose.
int reflex_world_rider_pose_slot(void* world, int32_t slot, float* out, uint32_t max_bones) {
    auto* h = static_cast<Handle*>(world);
    if (!h || !out || h->failed) return -1;
    try {
        Machine& m = h->world.machine(); uint32_t P = slot_player(m.mem, slot);
        uint32_t r = P ? uint32_t(m.mem.read(P + 0x1b4, 4)) : 0;
        if (!r || m.mem.read(r, 4) != 0x958724) return 0;
        uint32_t records = uint32_t(m.mem.read(r + 0x188, 4)), count = uint32_t(m.mem.read(r + 0x18c, 4)), world_matrices = uint32_t(m.mem.read(r + 0x19c, 4));
        if (count != 69 || !records || !world_matrices) return 0;
        uint32_t zero = 0;
        for (int i = int(count) - 1; i >= 0; i--) m.call_function(0x7db890, &zero, 1, records + 16 * uint32_t(i));
        uint32_t n = count < max_bones ? count : max_bones;
        m.mem.read_bytes(world_matrices, out, size_t(n) * 64);
        return int(n);
    } catch (const std::exception& e) { h->error = e.what(); h->failed = true; return -2; }
}
// Tables (layout from the profile bootstrap: name inline at +0x3c, column pointer range +0x10..+0x14; a column has its
// name inline at +0, type at +0x2c, storage holder at +0x40 whose +0xc..+0x10 range holds the rows).
static uint32_t table_column(Memory& m, uint32_t table, const char* column, uint32_t* type) {
    uint32_t begin = uint32_t(m.read(table + 0x10, 4)), end = uint32_t(m.read(table + 0x14, 4));
    if (end < begin || end - begin > 4096) return 0;
    for (uint32_t p = begin; p < end; p += 4) {
        uint32_t col = uint32_t(m.read(p, 4)); if (!col) continue;
        char name[33] = {}; m.read_bytes(col, name, 32);
        if (std::strcmp(name, column)) continue;
        *type = uint32_t(m.read(col + 0x2c, 4)); return col;
    }
    return 0;
}
uint32_t reflex_world_table(void* world, const char* name) {
    auto* h = static_cast<Handle*>(world);
    if (!h || !name || h->failed) return 0;
    try {
        Memory& m = h->world.machine().mem;
        // the session tables (static objects): "Player List" 00ae0e08, "Rider List" 00ae19b0 (RiderId, RiderOwner,
        // OwnerRiderId = AIRiderId, OwnerRiderGearSetId), "Vehicle List" 00ae05c8 (VehicleId, VehicleOwner, OwnerVehicleId =
        // AIVehicleId), "Activity List Real World" 00ae1b18
        for (uint32_t t : {0xae0e08u, 0xae19b0u, 0xae05c8u, 0xae1b18u}) { char found[64] = {}; m.read_bytes(t + 0x3c, found, 63); if (!std::strcmp(found, name)) return t; }
        uint32_t dbs[5] = {0xae0910, 0xae10f0, 0xae10f0 + 0x1f4, 0xae10f0 + 2 * 0x1f4, 0xae10f0 + 3 * 0x1f4};
        for (uint32_t db : dbs) {
            uint32_t count = uint32_t(m.read(db + 0x1e8, 4)); if (count > (0x1e8 - 0x38) / 4) continue;
            for (uint32_t i = 0; i < count; i++) {
                uint32_t t = uint32_t(m.read(db + 0x38 + 4 * i, 4)); if (!t) continue;
                char found[64] = {}; m.read_bytes(t + 0x3c, found, 63);
                if (!std::strcmp(found, name)) return t;
            }
        }
    } catch (const std::exception&) {}
    return 0;
}
int reflex_world_table_rows(void* world, uint32_t table) {
    auto* h = static_cast<Handle*>(world);
    if (!h || !table) return -1;
    try {
        Memory& m = h->world.machine().mem; static const uint32_t strides[] = {16, 4, 4, 0, 28, 24, 8};
        uint32_t begin = uint32_t(m.read(table + 0x10, 4)), end = uint32_t(m.read(table + 0x14, 4));
        if (end <= begin) return 0;
        uint32_t col = uint32_t(m.read(begin, 4)), type = uint32_t(m.read(col + 0x2c, 4)), holder = uint32_t(m.read(col + 0x40, 4));
        if (type >= 7 || !strides[type] || !holder) return -1;
        uint32_t first = uint32_t(m.read(holder + 0xc, 4)), last = uint32_t(m.read(holder + 0x10, 4));
        return last < first ? -1 : int((last - first) / strides[type]);
    } catch (const std::exception&) { return -1; }
}
int reflex_world_table_int(void* world, uint32_t table, const char* column, int32_t row, int32_t* out) {
    auto* h = static_cast<Handle*>(world);
    if (!h || !table || !out || row < 0) return 1;
    try {
        Memory& m = h->world.machine().mem; uint32_t type = 0, col = table_column(m, table, column, &type);
        if (!col || type != 1) return 2;
        uint32_t holder = uint32_t(m.read(col + 0x40, 4)), first = uint32_t(m.read(holder + 0xc, 4)), last = uint32_t(m.read(holder + 0x10, 4));
        if (uint32_t(row) >= (last - first) / 4) return 3;
        *out = int32_t(m.read(first + 4 * uint32_t(row), 4)); return 0;
    } catch (const std::exception&) { return 4; }
}
int reflex_world_table_string(void* world, uint32_t table, const char* column, int32_t row, char* out, int32_t size) {
    auto* h = static_cast<Handle*>(world);
    if (!h || !table || !out || size <= 0 || row < 0) return 1;
    try {
        Memory& m = h->world.machine().mem; uint32_t type = 0, col = table_column(m, table, column, &type);
        if (!col || type != 4) return 2;
        uint32_t holder = uint32_t(m.read(col + 0x40, 4)), first = uint32_t(m.read(holder + 0xc, 4)), last = uint32_t(m.read(holder + 0x10, 4));
        if (uint32_t(row) >= (last - first) / 28) return 3;
        uint32_t s = first + 28 * uint32_t(row), len = uint32_t(m.read(s + 20, 4)), cap = uint32_t(m.read(s + 24, 4));   // VC9 std::string: alloc, _Bx[16], _Mysize, _Myres
        uint32_t data = cap >= 16 ? uint32_t(m.read(s + 4, 4)) : s + 4;
        uint32_t n = std::min<uint32_t>(len, uint32_t(size - 1));
        m.read_bytes(data, out, n); out[n] = 0; return 0;
    } catch (const std::exception&) { return 4; }
}

// The camera the game renders with (_re/notes/game-camera.md): [0xa7b1d8] -> Camera (0xd6f620);
// +0x10 position, +0x20/+0x30/+0x40 right/up/forward rows, +0x50 vertical FOV (rad), +0x58 near, +0x5c far.
int reflex_world_camera(void* world, ReflexCamera* out) {
    auto* h = static_cast<Handle*>(world);
    if (!h || !out) return 1;
    std::memset(out, 0, sizeof *out);
    try {
        Memory& m = h->world.machine().mem;
        uint32_t cam = uint32_t(m.read(0xa7b1d8, 4));
        // [0xa7b1d8] is ForestSystem::vf4_o90's copy (00667680) of the viewport's camera, the static Camera 0xd6f620 (vtable 0x95a2d8);
        // levels without a forest (the FMX arenas) never set it: the camera itself then
        if (!cam && m.read(0xd6f620, 4) == 0x95a2d8) cam = 0xd6f620;
        cam = garage::camera_of(m, cam);   // WP-G: game state 2 (the test drive) never copies its viewport camera: the player's camera entity
        if (!cam) return 2;
        m.read_bytes(cam + 0x10, out->position, 12);
        m.read_bytes(cam + 0x20, out->right, 12);
        m.read_bytes(cam + 0x30, out->up, 12);
        m.read_bytes(cam + 0x40, out->forward, 12);
        m.read_bytes(cam + 0x50, &out->fov, 4);
        m.read_bytes(cam + 0x58, &out->near_plane, 4);
        m.read_bytes(cam + 0x5c, &out->far_plane, 4);
        return 0;
    } catch (const std::exception& e) { h->error = e.what(); return 3; }
}

int reflex_world_tracks(void* world, float* out, uint32_t max_triangles, uint32_t* version) {
    auto* h = static_cast<Handle*>(world);
    if (!h) return -1;
    try {
        if (version) *version = h->world.tracks_version();
        if (!out) return 0;
        std::vector<float> tris; size_t n = h->world.tracks(tris);
        if (n > max_triangles) n = max_triangles;
        std::memcpy(out, tris.data(), n * 18 * sizeof(float));
        return int(n);
    } catch (const std::exception& e) { h->error = e.what(); return -2; }
}

// First object in [lo, hi) whose dword 0 is `vtable` and whose dword at `offset` equals `value`
// (e.g. the start gate: DropGate vtable 0x95fc24, entity id at +0x14). 0 if none.
uint32_t reflex_world_find_object(void* world, uint32_t vtable, uint32_t offset, uint32_t value, uint32_t lo, uint32_t hi) {
    auto* h = static_cast<Handle*>(world);
    if (!h) return 0;
    try {
        Memory& m = h->world.machine().mem;
        for (uint64_t page = lo & ~0xfffu; page < hi; page += 0x1000) {
            if (!m.mapped(uint32_t(page))) continue;
            for (uint32_t o = 0; o < 0x1000; o += 4) {
                uint32_t a = uint32_t(page) + o;
                if (uint32_t(m.read(a, 4)) != vtable) continue;
                if (m.mapped(a + offset) && uint32_t(m.read(a + offset, 4)) == value) return a;
            }
        }
    } catch (const std::exception& e) { h->error = e.what(); }
    return 0;
}

int reflex_world_audio_enable(void* world, int enabled) {
    auto* h = static_cast<Handle*>(world);
    if (!h) return 1;
    h->world.set_audio(enabled != 0); return 0;
}
int reflex_world_audio(void* world, ReflexAudioCommand* out, uint32_t max) {
    auto* h = static_cast<Handle*>(world);
    if (!h || !out) return -1;
    static_assert(sizeof(ReflexAudioCommand) == sizeof(reflex_vm::World::AudioCommand), "audio command layout");
    return int(h->world.drain_audio(reinterpret_cast<reflex_vm::World::AudioCommand*>(out), max));
}
int reflex_world_audio_finished(void* world, uint32_t wrapper, uint32_t serial) {
    auto* h = static_cast<Handle*>(world);
    if (!h) return 1;
    h->world.audio_finished(wrapper, serial); return 0;
}

int reflex_world_terrain(void* world, ReflexTerrainInfo* out) {
    auto* h = static_cast<Handle*>(world);
    if (!h || !out) return 1;
    try {
        TerrainInfo t; if (!h->world.terrain_info(t)) return 2;
        static_assert(sizeof(ReflexTerrainInfo) == sizeof(TerrainInfo), "terrain info layout");
        std::memcpy(out, &t, sizeof t);
        h->world.clear_terrain_dirty();
        return 0;
    } catch (const std::exception& e) { h->error = e.what(); return 3; }
}

int reflex_world_terrain_heights(void* world, float* out, int x0, int z0, int w, int hgt) {
    auto* h = static_cast<Handle*>(world);
    if (!h || !out) return 1;
    try { return h->world.terrain_heights(out, x0, z0, w, hgt) ? 0 : 2; }
    catch (const std::exception& e) { h->error = e.what(); return 3; }
}

int reflex_world_terrain_deform(void* world, float* out, int x0, int z0, int w, int hgt) {
    auto* h = static_cast<Handle*>(world);
    if (!h || !out) return 1;
    try { return h->world.terrain_deform(out, x0, z0, w, hgt) ? 0 : 2; }
    catch (const std::exception& e) { h->error = e.what(); return 3; }
}

int reflex_world_port(void* world, const char* filter, int mode) {
    auto* h = static_cast<Handle*>(world);
    if (!h || !filter) return -1;
    try { return reflex_port::install(h->world.machine(), mode == 2 ? reflex_port::Mode::Verify : mode == 1 ? reflex_port::Mode::On : reflex_port::Mode::Off, filter); }
    catch (const std::exception& e) { h->error = e.what(); return -1; }
}
int reflex_world_boot_ports(const char* filter, int mode) { set_client_boot_ports(filter ? filter : "all", mode); return 0; }
int reflex_world_port_report(char* out, int size) {
    std::string r = reflex_port::report();
    if (out && size > 0) std::snprintf(out, size_t(size), "%s", r.c_str());
    return int(r.size());
}

int reflex_world_set_ambient_animation(void* world, int simulate) {
    auto* h = static_cast<Handle*>(world);
    if (!h) return 1;
    h->world.set_ambient_animation(simulate != 0);
    return 0;
}

// Particles (particles.cpp): simulate the particle managers with ambient animation off, and the frame's live particles.
int reflex_world_particles_enable(void* world, int enabled) {
    auto* h = static_cast<Handle*>(world);
    if (!h) return 1;
    h->world.set_particles(enabled != 0);
    return 0;
}
int reflex_world_particles(void* world, ReflexParticle* out, uint32_t max, ReflexParticleInfo* info) {
    auto* h = static_cast<Handle*>(world);
    if (!h || h->failed) return -1;
    static_assert(sizeof(ReflexParticle) == sizeof(World::Particle) && sizeof(ReflexParticle) == 64, "particle layout");
    static_assert(sizeof(ReflexParticleInfo) == sizeof(World::ParticleInfo), "particle info layout");
    try {
        thread_local std::vector<World::Particle> parts; World::ParticleInfo pi;
        h->world.particle_export(parts, pi);
        if (info) std::memcpy(info, &pi, sizeof pi);
        size_t n = std::min<size_t>(parts.size(), out ? max : 0);
        if (n) std::memcpy(out, parts.data(), n * sizeof(ReflexParticle));
        return int(n);
    } catch (const std::exception& e) { h->error = e.what(); return -2; }
}

// Ambient animated entities (ambient.cpp): the classes Unity draws keep their update with ambient animation off.
int reflex_world_ambient_mask(void* world, uint32_t keep) {
    auto* h = static_cast<Handle*>(world);
    if (!h) return 1;
    h->world.set_ambient_mask(keep);
    return 0;
}
int reflex_world_ambient(void* world, ReflexAmbientEntity* out, uint32_t max, float* matrices, uint32_t max_matrices, ReflexAmbientInfo* info) {
    auto* h = static_cast<Handle*>(world);
    if (!h || h->failed) return -1;
    static_assert(sizeof(ReflexAmbientEntity) == sizeof(AmbientEntity) && sizeof(ReflexAmbientEntity) == 304, "ambient entity layout");
    static_assert(sizeof(ReflexAmbientInfo) == sizeof(AmbientInfo), "ambient info layout");
    try {
        thread_local std::vector<AmbientEntity> ents; thread_local std::vector<float> mats; AmbientInfo ai;
        h->ambient.collect(h->world.machine(), ents, mats, ai);
        if (info) std::memcpy(info, &ai, sizeof ai);
        size_t n = std::min<size_t>(ents.size(), out ? max : 0);
        if (n) std::memcpy(out, ents.data(), n * sizeof(ReflexAmbientEntity));
        if (matrices) std::memcpy(matrices, mats.data(), std::min<size_t>(mats.size() / 16, max_matrices) * 64);
        return int(n);
    } catch (const std::exception& e) { h->error = e.what(); return -2; }
}
int reflex_world_ambient_rescan(void* world) {
    auto* h = static_cast<Handle*>(world);
    if (!h) return 1;
    h->ambient.rescan();
    return 0;
}

// Race progress of the local player (see _re/notes/race-state.md for the pointer chains).
int reflex_world_race(void* world, ReflexRaceState* out) {
    auto* h = static_cast<Handle*>(world);
    if (!h || !out) return 1;
    std::memset(out, 0, sizeof *out);
    try {
        Memory& m = h->world.machine().mem;
        auto u32 = [&](uint32_t a) { return uint32_t(m.read(a, 4)); };
        auto f = [&](uint32_t a) { uint32_t b = u32(a); float v; std::memcpy(&v, &b, 4); return v; };
        uint32_t G = u32(0xa946b8);
        if (!G) return 0;
        uint32_t count = uint32_t(m.read(G + 0x1541c, 1)), P = 0;
        for (uint32_t i = 0; i < count && i < 8; i++) {
            uint32_t slot = u32(G + 0x153ec + 4 * i), p = G + 0x7460 + slot * 0x12a0;
            if (u32(p + 0x10) == 0) { P = p; break; }
        }
        if (!P) return 0;
        uint32_t A = u32(P + 0x94);
        out->reset_timer = f(P + 0x116c);
        if (!A) return 0;
        uint32_t drivers = u32(A + 0x268), n = u32(A + 0x26c), D = 0;
        for (uint32_t i = 0; i < n && i < 64; i++) { uint32_t d = u32(drivers + 4 * i); if (d && u32(d + 0x74) == P) { D = d; break; } }
        out->state = int32_t(u32(A + 0x21c));   // Activity::vf24 0x558930 dispatches the state handlers on A+0x21c
        out->riders = int32_t(n);
        out->num_laps = int32_t(u32(A + 0xff070));
        uint32_t board = u32(A + 0x2cc);
        out->start_timer = board ? f(board + 0x4f4) : 0.0f;
        if (!D) return 0;
        out->valid = 1;
        out->laps_completed = int32_t(u32(D + 0xf1c));
        out->place = int32_t(u32(D + 0x78)) + 1;
        out->lap_time = f(D + 0xf20);
        out->last_lap = f(D + 0x10e0);
        out->best_lap = f(D + 0x10c4);
        out->race_time = f(D + 0x134);
        uint32_t track = u32(D + 0x14), node = track ? u32(track) : 0;
        float length = node ? f(node + 0x13d * 4) : 0.0f, to_line = f(D + 0x10cc);
        out->lap_progress = length > 0 ? std::max(0.0f, std::min(1.0f, 1.0f - to_line / length)) : 0.0f;
        return 0;
    } catch (const std::exception& e) { h->error = e.what(); return 2; }
}

// Every driver of the race activity (A+0x268 list): its player slot (P = G+0x7460+slot*0x12a0 at D+0x74), place D+0x78,
// laps completed D+0xf1c, finished D+0x70, race time D+0x134 (stops at the driver's finish), best lap D+0x10c4, last lap D+0x10e0 (as reflex_world_race reads for the
// local player). Returns the count written, 0 outside a race, <0 on failure.
int reflex_world_race_drivers(void* world, ReflexDriverResult* out, uint32_t max) {
    auto* h = static_cast<Handle*>(world);
    if (!h || !out) return -1;
    try {
        Memory& m = h->world.machine().mem;
        auto u32 = [&](uint32_t a) { return uint32_t(m.read(a, 4)); };
        auto f = [&](uint32_t a) { uint32_t b = u32(a); float v; std::memcpy(&v, &b, 4); return v; };
        uint32_t G = u32(0xa946b8), P = local_player(m); if (!G || !P) return 0;
        uint32_t A = u32(P + 0x94); if (!A) return 0;
        uint32_t drivers = u32(A + 0x268), n = u32(A + 0x26c), k = 0;
        for (uint32_t i = 0; i < n && i < 64 && k < max; i++) {
            uint32_t d = u32(drivers + 4 * i); if (!d) continue;
            uint32_t p = u32(d + 0x74);
            ReflexDriverResult& r = out[k++];
            r.slot = p >= G + 0x7460 ? int32_t((p - (G + 0x7460)) / 0x12a0) : -1;
            r.local = p == P ? 1 : 0;
            r.place = int32_t(u32(d + 0x78)) + 1;
            r.laps_completed = int32_t(u32(d + 0xf1c));
            r.finished = int32_t(u32(d + 0x70));   // set when D+0xf1c reaches A+0xff070; the race clock D+0x134 stops then
            r.race_time = f(d + 0x134); r.best_lap = f(d + 0x10c4); r.last_lap = f(d + 0x10e0);
        }
        return int(k);
    } catch (const std::exception& e) { h->error = e.what(); return -2; }
}

// The race line ahead of the local player: node centres (x, y, z) from the driver's current
// race-line node (D+0x1c) following the nodes' next links (+0x64); nodes are 0x500 bytes,
// centre at +0 (_re/notes/race-state.md). Returns the number of points (0 outside a race).
int reflex_world_race_line(void* world, float* out, uint32_t max_points) {
    auto* h = static_cast<Handle*>(world);
    if (!h || !out) return -1;
    try {
        Memory& m = h->world.machine().mem;
        auto u32 = [&](uint32_t a) { return uint32_t(m.read(a, 4)); };
        uint32_t P = local_player(m); if (!P) return 0;
        uint32_t A = u32(P + 0x94); if (!A) return 0;
        uint32_t drivers = u32(A + 0x268), n = u32(A + 0x26c), D = 0;
        for (uint32_t i = 0; i < n && i < 64; i++) { uint32_t d = u32(drivers + 4 * i); if (d && u32(d + 0x74) == P) { D = d; break; } }
        if (!D) return 0;
        uint32_t node = u32(D + 0x1c), first = node, count = 0;
        while (node && count < max_points && m.mapped(node) && m.mapped(node + 0x67)) {
            m.read_bytes(node, out + 3 * count, 12); count++;
            node = u32(node + 0x64);
            if (node == first) break;
        }
        return int(count);
    } catch (const std::exception& e) { h->error = e.what(); return -2; }
}

// Values the original in-race HUD (MXUI* components, _re/notes/ui.md, _re/formats/out/ui/hud.json) shows
// beyond reflex_world_race: speed (V+0x150 ft/s; the speedometer shows int(x * 0.68182) mph), time behind the
// leader / ahead (D+0x10B4 / D+0x10B8), the lap-time hold (shows the last lap while D+0x130 <= D+0x10DC),
// RuleOfftrackTimePenalty ([D+0xAC]: +0x10 on track, +0x20 penalty) and RuleBackwards ([D+0xA8]+0xc: riding
// the wrong way, RuleBackwards::vf5 0x519990), the race state at A+0x238 (5 hides the lap counter).
int reflex_world_hud(void* world, ReflexHudState* out) {
    auto* h = static_cast<Handle*>(world);
    if (!h || !out) return 1;
    std::memset(out, 0, sizeof *out);
    try {
        Memory& m = h->world.machine().mem;
        auto u32 = [&](uint32_t a) { return uint32_t(m.read(a, 4)); };
        auto f = [&](uint32_t a) { uint32_t b = u32(a); float v; std::memcpy(&v, &b, 4); return v; };
        uint32_t P = local_player(m); if (!P) return 0;
        uint32_t V = u32(P + 0x1ac), A = u32(P + 0x94);
        if (V) out->speed = f(V + 0x150);
        if (!A) return 0;
        out->activity_state = int32_t(u32(A + 0x238));
        uint32_t drivers = u32(A + 0x268), n = u32(A + 0x26c), D = 0;
        for (uint32_t i = 0; i < n && i < 64; i++) { uint32_t d = u32(drivers + 4 * i); if (d && u32(d + 0x74) == P) { D = d; break; } }
        if (!D) return 0;
        out->valid = 1;
        out->time_behind = f(D + 0x10b4); out->time_ahead = f(D + 0x10b8);
        out->hold_clock = f(D + 0x130); out->hold_until = f(D + 0x10dc);
        uint32_t offtrack = u32(D + 0xac), backwards = u32(D + 0xa8);
        out->on_track = offtrack ? int32_t(u32(offtrack + 0x10) != 0) : 1;
        out->penalty = offtrack ? f(offtrack + 0x20) : 0.0f;
        out->wrong_way = backwards ? int32_t(u32(backwards + 0xc) != 0) : 0;
        return 0;
    } catch (const std::exception& e) { h->error = e.what(); return 2; }
}

// A slot's vehicle class and all bodies of its simulation (atv.cpp); slot -1: the local player.
int reflex_world_vehicle(void* world, int32_t slot, ReflexVehicleInfo* out) {
    auto* h = static_cast<Handle*>(world);
    if (!h || !out || h->failed) return -1;
    static_assert(sizeof(ReflexVehicleInfo) == sizeof(VehicleInfo), "vehicle info layout");
    try {
        Memory& m = h->world.machine().mem;
        uint32_t P = slot < 0 ? local_player(m) : slot_player(m, slot), G = uint32_t(m.read(0xa946b8, 4));
        int32_t k = P && G ? int32_t((P - (G + 0x7460)) / 0x12a0) : slot;
        VehicleInfo v; bool ok = vehicle_info(m, P, k, v);
        std::memcpy(out, &v, sizeof v);
        return ok ? 0 : 1;
    } catch (const std::exception& e) { h->error = e.what(); return -2; }
}

// A slot's mud record (mud.cpp); slot -1: the local player. Reads only.
int reflex_world_mud(void* world, int32_t slot, ReflexMudState* out) {
    auto* h = static_cast<Handle*>(world);
    if (!h || !out || h->failed) return -1;
    static_assert(sizeof(ReflexMudState) == sizeof(MudState), "mud state layout");
    try {
        Memory& m = h->world.machine().mem;
        uint32_t P = slot < 0 ? local_player(m) : slot_player(m, slot), G = uint32_t(m.read(0xa946b8, 4));
        int32_t k = P && G ? int32_t((P - (G + 0x7460)) / 0x12a0) : slot;
        MudState v; bool ok = mud_state(m, P, k, v);
        std::memcpy(out, &v, sizeof v);
        return ok ? 0 : 1;
    } catch (const std::exception& e) { h->error = e.what(); return -2; }
}

uint32_t reflex_world_sim(void* world) { return world ? static_cast<Handle*>(world)->sim : 0; }

int reflex_world_read(void* world, uint32_t address, void* out, uint32_t size) {
    auto* h = static_cast<Handle*>(world);
    if (!h || !out) return 1;
    try { h->world.machine().mem.read_bytes(address, out, size); return 0; }
    catch (const std::exception& e) { h->error = e.what(); return 2; }
}

int reflex_world_memory_hash(void* world, uint64_t* hash, uint32_t* pages) {   // diagnostics: as the CLI's REFLEX_MEMHASH (resident pages, no stacks)
    auto* h = static_cast<Handle*>(world);
    if (!h || !hash) return 1;
    Machine& m = h->world.machine(); uint64_t x = 1469598103934665603ull; uint32_t n = 0;
    static int dumps = 0; FILE* df = nullptr;   // REFLEX_CLIENT_MEMDUMP=<prefix>: the same pages to <prefix>-<n>.bin (tools/memdiff.py)
    if (const char* md = std::getenv("REFLEX_CLIENT_MEMDUMP")) df = std::fopen((std::string(md) + "-" + std::to_string(++dumps) + ".bin").c_str(), "wb");
    for (uint64_t pg = 0; pg < (1ull << 20); pg++) {
        const uint8_t* p = m.mem.peek(uint32_t(pg << 12)); if (!p) continue;
        uint32_t a = uint32_t(pg << 12);
        if ((a >= 0x7fec0000u && a < 0x7ff00000u) || (a >= 0x7fbc0000u && a < 0x7fc00000u)) continue;
        n++; for (int i = 0; i < 4096; i += 8) { uint64_t v; std::memcpy(&v, p + i, 8); x = (x ^ v) * 1099511628211ull; }
        if (df) { std::fwrite(&a, 4, 1, df); std::fwrite(p, 1, 4096, df); }
    }
    if (df) std::fclose(df);
    *hash = x; if (pages) *pages = n; return 0;
}

void reflex_world_destroy(void* world) { if (world) { client_coverage_write(static_cast<Handle*>(world)->world.machine(), "world-commands"); client_coverage_forget(static_cast<Handle*>(world)->world.machine()); ui_forget(&static_cast<Handle*>(world)->world.machine()); persist::forget(static_cast<Handle*>(world)->world.machine()); } delete static_cast<Handle*>(world); }

// MOTOCAREER: the game's own completion and save of a career event (career.cpp finish_event), between frames.
int reflex_world_career_finish(void* world, const char* remote_dir, char* out, int32_t size) {
    auto* h = static_cast<Handle*>(world);
    if (!h || h->failed || !remote_dir) return -1;
    ClientCoverageScope cov_(h->world.machine());   // REFLEX_CLIENT_COVERAGE (boot.h)
    try {
        std::string err, r = career::finish_event(h->world.machine(), remote_dir, &err);
        if (!err.empty()) h->error = err;
        if (!out || size <= int32_t(r.size())) return -int(r.size()) - 1;
        std::memcpy(out, r.c_str(), r.size() + 1); return int(r.size());
    } catch (const std::exception& e) { h->error = e.what(); return -1; }
}

// PERSISTENCE (persist.cpp): the profile in a race world, between frames.
int reflex_world_persist(void* world, const char* save_dir, const char* command, char* out, int32_t size) {
    auto* h = static_cast<Handle*>(world);
    if (!h || h->failed || !save_dir || !command) return -1;
    ClientCoverageScope cov_(h->world.machine());   // REFLEX_CLIENT_COVERAGE (boot.h)
    try {
        std::string err, r = persist::world_command(h->world.machine(), save_dir, command, &err);
        if (r.empty()) { h->error = err.empty() ? std::string("persistence command failed: ") + command : err; return -1; }
        if (!err.empty()) h->error = err;
        if (!out || size <= int32_t(r.size())) return -int(r.size()) - 1;
        std::memcpy(out, r.c_str(), r.size() + 1); return int(r.size());
    } catch (const std::exception& e) { h->error = e.what(); return -1; }
}

int reflex_world_career(void* world, const char* command, char* out, int32_t size) {
    auto* h = static_cast<Handle*>(world);
    if (!h || h->failed || !command) return -1;
    ClientCoverageScope cov_(h->world.machine());   // REFLEX_CLIENT_COVERAGE (boot.h)
    try {
        std::string err, r = career::world_command(h->world.machine(), command, &err);
        if (!err.empty()) { h->error = err; return -1; }
        if (!out || size <= int32_t(r.size())) return -int(r.size()) - 1;
        std::memcpy(out, r.c_str(), r.size() + 1); return int(r.size());
    } catch (const std::exception& e) { h->error = e.what(); return -1; }
}


// Game modes (modes.cpp): the live in-game UI and the activity.
int reflex_world_ui_activate(void* world, const char* route, char* out, int32_t size) {
    auto* h = static_cast<Handle*>(world);
    if (!h || h->failed) return -1;
    ClientCoverageScope cov_(h->world.machine());   // REFLEX_CLIENT_COVERAGE (boot.h)
    try {
        std::string r = ui_activate(h->world.machine(), route ? route : "");
        if (r.empty()) { h->error = "the game's UI was not activated (no InGamePageGraph or no activity)"; return -1; }
        if (out && size > 0) { size_t n = std::min<size_t>(r.size(), size_t(size - 1)); std::memcpy(out, r.c_str(), n); out[n] = 0; }
        return int(r.size());
    } catch (const std::exception& e) { h->error = e.what(); return -1; }
}
int reflex_world_ui_event(void* world, const char* name) {
    auto* h = static_cast<Handle*>(world);
    if (!h || h->failed || !name) return -1;
    ClientCoverageScope cov_(h->world.machine());   // REFLEX_CLIENT_COVERAGE (boot.h)
    try { ui_event(h->world.machine(), name); return 0; } catch (const std::exception& e) { h->error = e.what(); return -1; }
}
int reflex_world_ui_frame(void* world, ReflexUiLayer* layers, uint32_t max_layers, ReflexUiNode* nodes, uint32_t max_nodes,
                          ReflexUiAnim* anims, uint32_t max_anims, char* text, uint32_t text_size, int32_t* counts) {
    auto* h = static_cast<Handle*>(world);
    if (!h || h->failed) return -1;
    ClientCoverageScope cov_(h->world.machine());   // REFLEX_CLIENT_COVERAGE (boot.h)
    static_assert(sizeof(ReflexUiNode) == sizeof(UiNode) && sizeof(ReflexUiLayer) == sizeof(UiLayer) && sizeof(ReflexUiAnim) == sizeof(UiAnim), "ui layouts");
    try {
        std::vector<UiLayer> L; std::vector<UiNode> N; std::vector<UiAnim> A; std::string T;
        ui_frame(h->world.machine(), L, N, A, T);
        uint32_t nl = std::min<uint32_t>(uint32_t(L.size()), layers ? max_layers : 0), nn = std::min<uint32_t>(uint32_t(N.size()), nodes ? max_nodes : 0);
        uint32_t na = std::min<uint32_t>(uint32_t(A.size()), anims ? max_anims : 0), nt = std::min<uint32_t>(uint32_t(T.size()), text ? text_size : 0);
        if (nl) std::memcpy(layers, L.data(), nl * sizeof(UiLayer));
        if (nn) std::memcpy(nodes, N.data(), nn * sizeof(UiNode));
        if (na) std::memcpy(anims, A.data(), na * sizeof(UiAnim));
        if (nt) std::memcpy(text, T.data(), nt);
        if (counts) { counts[0] = int32_t(nl); counts[1] = int32_t(nn); counts[2] = int32_t(na); counts[3] = int32_t(nt); }
        return 0;
    } catch (const std::exception& e) { h->error = e.what(); return -2; }
}
int reflex_world_ui_requests(void* world, char* out, int32_t size) {
    auto* h = static_cast<Handle*>(world);
    if (!h || h->failed || !out || size <= 0) return -1;
    ClientCoverageScope cov_(h->world.machine());   // REFLEX_CLIENT_COVERAGE (boot.h)
    std::string r = ui_requests(h->world.machine());
    size_t n = std::min<size_t>(r.size(), size_t(size - 1)); std::memcpy(out, r.c_str(), n); out[n] = 0;
    return int(n);
}
int reflex_world_mode_objects(void* world, ReflexModeObject* out, uint32_t max) {
    auto* h = static_cast<Handle*>(world);
    if (!h || h->failed) return -1;
    ClientCoverageScope cov_(h->world.machine());   // REFLEX_CLIENT_COVERAGE (boot.h)
    static_assert(sizeof(ReflexModeObject) == sizeof(ModeObject), "mode object layout");
    try { std::vector<ModeObject> v; mode_objects(h->world.machine(), v); uint32_t n = std::min<uint32_t>(uint32_t(v.size()), out ? max : 0); if (n) std::memcpy(out, v.data(), n * sizeof(ModeObject)); return int(n); }
    catch (const std::exception& e) { h->error = e.what(); return -2; }
}
int reflex_world_mode(void* world, ReflexModeState* out) {
    auto* h = static_cast<Handle*>(world);
    if (!h || !out) return -1;
    static_assert(sizeof(ReflexModeState) == sizeof(ModeState), "mode state layout");
    try { ModeState v; bool ok = mode_state(h->world.machine().mem, v); std::memcpy(out, &v, sizeof v); return ok ? 0 : 1; }
    catch (const std::exception& e) { h->error = e.what(); return -2; }
}

// Input and settings (input.cpp, _re/notes/input-settings.md).
int reflex_world_mouse(void* world, int32_t dx, int32_t dy, int32_t wheel, uint32_t button_flags) {
    auto* h = static_cast<Handle*>(world);
    if (!h || h->failed) return -1;
    ClientCoverageScope cov_(h->world.machine());   // REFLEX_CLIENT_COVERAGE (boot.h)
    try { input::post_mouse(h->world.machine(), h->input, dx, dy, wheel, button_flags); return 0; }
    catch (const std::exception& e) { h->error = e.what(); h->failed = true; return -2; }
}
int reflex_world_rumble(void* world, uint16_t* motors, uint32_t* calls) {
    auto* h = static_cast<Handle*>(world);
    if (!h) return -1;
    for (int i = 0; i < 4; i++) {
        uint32_t v = h->input.motors[i].load();
        if (motors) { motors[2 * i] = uint16_t(v >> 16); motors[2 * i + 1] = uint16_t(v); }
        if (calls) calls[i] = h->input.calls[i].load();
    }
    return 0;
}
int reflex_world_ui_setting(void* world, const char* name, int32_t value, int32_t device, int32_t set, int32_t* out) {
    auto* h = static_cast<Handle*>(world);
    if (!h || h->failed) return -1;
    ClientCoverageScope cov_(h->world.machine());   // REFLEX_CLIENT_COVERAGE (boot.h)
    try { int32_t v = input::ui_setting(h->world.machine(), name, value, device, set); if (out) *out = v; return 0; }
    catch (const std::exception& e) { h->error = e.what(); return -2; }
}

// Physics props and animated textures (props.cpp).
int reflex_world_props(void* world, ReflexProp* out, uint32_t max, int32_t all, ReflexPropInfo* info) {
    auto* h = static_cast<Handle*>(world);
    if (!h || h->failed) return -1;
    static_assert(sizeof(ReflexProp) == sizeof(PropEntity) && sizeof(ReflexProp) == 80, "prop layout");
    static_assert(sizeof(ReflexPropInfo) == sizeof(PropInfo), "prop info layout");
    try {
        PropInfo pi; h->props.collect(h->world.machine(), reinterpret_cast<PropEntity*>(out), out ? max : 0, all != 0, pi);
        if (info) std::memcpy(info, &pi, sizeof pi);
        return pi.reported;
    } catch (const std::exception& e) { h->error = e.what(); return -2; }
}
int reflex_world_props_rescan(void* world) {
    auto* h = static_cast<Handle*>(world);
    if (!h) return 1;
    h->props.rescan();
    return 0;
}
int reflex_world_texture_anims(void* world, ReflexTextureAnim* out, uint32_t max) {
    auto* h = static_cast<Handle*>(world);
    if (!h || h->failed) return -1;
    static_assert(sizeof(ReflexTextureAnim) == sizeof(TextureAnimState) && sizeof(ReflexTextureAnim) == 104, "texture anim layout");
    try {
        thread_local std::vector<TextureAnimState> v; h->props.textures(h->world.machine(), v);
        uint32_t n = std::min<uint32_t>(uint32_t(v.size()), out ? max : 0);
        if (n) std::memcpy(out, v.data(), n * sizeof(TextureAnimState));
        return int(n);
    } catch (const std::exception& e) { h->error = e.what(); return -2; }
}

}  // extern "C"

// ------------------------------------------------------------------------------------------- WP-3: in-race UI sounds (_re/notes/hud.md)
// The game's UI sound manager g_UISoundManager ([0xd94a84], MXUISoundManager_Get 005e4bb0) plays UIRegistry.xml UISound names through
// UISound_Play 005e37e0 (restarts a playing one), UISound_PlayIfIdle 005e3840 (the warning loops) and UISound_Stop 005e38c0 (ECX the
// manager, stack the name, ret 4): MXUIPlayerRankComponent RankChangeUp (A+0x238 == 6) / EliminationWarn, the flag container ShowFlag /
// ShowRedFlag / ShowYellowFlag / ShowWhiteFlag, Flooding, ShowJudges, the loading page's LevelLoaded, the list containers' scroll sounds
// and App.PlayUISound / StopUISound of the page Lua. The events behind them (UI_02) are not loaded in the world; the hooks record the
// names and run the original.
#include <deque>
namespace {
std::mutex ui_sound_lock;
std::unordered_map<const Machine*, std::deque<std::string>> ui_sound_queue;
void ui_sound_hook(Machine& mc, uint32_t at, const char* kind) {
    const Machine* key = &mc;
    mc.natives[at] = [key, kind](Machine& m) -> uint32_t {
        uint32_t p = uint32_t(m.mem.read(m.r[4] + 4, 4)); std::string line = kind; line += ' ';
        for (uint32_t i = 0; p >= 0x10000 && i < 64 && m.mem.mapped(p + i); i++) { char c = char(m.mem.read(p + i, 1)); if (!c) break; line += c; }
        { std::lock_guard<std::mutex> g(ui_sound_lock); auto& q = ui_sound_queue[key]; if (q.size() < 256) q.push_back(line); }
        return RUN_ORIGINAL;
    };
}
}  // namespace
// The UI sounds played / stopped since the last call, one per line ("play RankChangeUp", "loop EliminationWarn", "stop Flooding").
// The first call installs the hooks (a world without them is unchanged). Between frames. Returns the length written, <0 on error.
extern "C" int reflex_world_ui_sounds(void* world, char* out, int32_t size) {
    auto* h = static_cast<Handle*>(world);
    if (!h || h->failed || !out || size <= 0) return -1;
    ClientCoverageScope cov_(h->world.machine());   // REFLEX_CLIENT_COVERAGE (boot.h)
    Machine& mc = h->world.machine(); std::string r;
    {
        std::lock_guard<std::mutex> g(ui_sound_lock); auto& q = ui_sound_queue[&mc];
        if (!mc.natives.count(0x5e37e0)) { q.clear(); ui_sound_hook(mc, 0x5e37e0, "play"); ui_sound_hook(mc, 0x5e3840, "loop"); ui_sound_hook(mc, 0x5e38c0, "stop"); }
        while (!q.empty() && r.size() + q.front().size() + 1 < size_t(size)) { r += q.front(); r += '\n'; q.pop_front(); }
    }
    std::memcpy(out, r.c_str(), r.size() + 1);
    return int(r.size());
}

// ------------------------------------------------------------------------------------------- WP-R2: water wake splats (_re/notes/water.md)
// The wake object DAT_00a95790 (FUN_007eba00 / 007ebd10): +0x80 the splat centres (world x, z; count +0x200), +0x3d0 the splat vertices
// (5 floats each, count of floats +0x1a50, primitives +0x390), filled by FUN_007ecd80 / 007ec280 while vehicles touch water; the
// render event FUN_007ed300 draws them and clears the three counts. Read, then cleared like the original (single view: slot 0).
extern "C" int reflex_world_water_splats(void* world, float* vertices, uint32_t max_vertices, float* centres, uint32_t max_centres, uint32_t* counts) {
    auto* h = static_cast<Handle*>(world);
    if (!h || h->failed) return -1;
    try {
        Memory& m = h->world.machine().mem;
        uint32_t W = uint32_t(m.read(0xa95790, 4));
        if (counts) { counts[0] = 0; counts[1] = 0; }
        if (!W || !m.mapped(W + 0x1a54)) return 0;
        uint32_t prims = uint32_t(m.read(W + 0x390, 4)), floats = uint32_t(m.read(W + 0x1a50, 4)), nc = uint32_t(m.read(W + 0x200, 4));
        if (floats > 0x5a0 || nc > 0x30) return -2;
        uint32_t nv = floats / 5;
        if (vertices && max_vertices) m.read_bytes(W + 0x3d0, vertices, 20u * std::min(nv, max_vertices));
        if (centres && max_centres) m.read_bytes(W + 0x80, centres, 8u * std::min(nc, max_centres));
        if (counts) { counts[0] = prims; counts[1] = nc; }
        if (floats || prims || nc) { m.write(W + 0x200, 4, 0); m.write(W + 0x1a50, 4, 0); m.write(W + 0x390, 4, 0); }
        return int(nv);
    } catch (const std::exception& e) { h->error = e.what(); return -3; }
}

// WP-5 in-race pages (pages.cpp, _re/notes/in-race-pages.md): the localized UI voice the page Lua requests, and the pages' world
// commands ("state", "skip_scenario", "lua_ui", "ui", "ui_int", "gates"). Between frames.
#include "pages.h"
extern "C" {
int reflex_world_ui_voice(void* world, char* out, int32_t size) {
    auto* h = static_cast<Handle*>(world);
    if (!h || h->failed || !out || size <= 0) return -1;
    ClientCoverageScope cov_(h->world.machine());   // REFLEX_CLIENT_COVERAGE (boot.h)
    try {
        std::string r = pages::voice(h->world.machine());
        size_t n = std::min<size_t>(r.size(), size_t(size - 1)); std::memcpy(out, r.c_str(), n); out[n] = 0;
        return int(n);
    } catch (const std::exception& e) { h->error = e.what(); return -1; }
}
int reflex_world_pages(void* world, const char* command, char* out, int32_t size) {
    auto* h = static_cast<Handle*>(world);
    if (!h || h->failed || !command) return -1;
    ClientCoverageScope cov_(h->world.machine());   // REFLEX_CLIENT_COVERAGE (boot.h)
    try {
        std::string err, r = pages::world_command(h->world.machine(), command, &err);
        if (!err.empty()) { h->error = err; return -1; }
        if (!out || size <= int32_t(r.size())) return -int(r.size()) - 1;
        std::memcpy(out, r.c_str(), r.size() + 1); return int(r.size());
    } catch (const std::exception& e) { h->error = e.what(); return -1; }
}
}  // extern "C"

// WP-G garage (garage.cpp): tuning on the running vehicle, between frames.
extern "C" int reflex_world_garage(void* world, const char* command, char* out, int32_t size) {
    auto* h = static_cast<Handle*>(world);
    if (!h || h->failed || !command) return -1;
    ClientCoverageScope cov_(h->world.machine());   // REFLEX_CLIENT_COVERAGE (boot.h)
    try {
        std::string err, r = garage::world_command(h->world.machine(), command, &err);
        if (!err.empty()) { h->error = err; return -1; }
        if (!out || size <= int32_t(r.size())) return -int(r.size()) - 1;
        std::memcpy(out, r.c_str(), r.size() + 1); return int(r.size());
    } catch (const std::exception& e) { h->error = e.what(); return -1; }
}

// WP-T MotoSkills tutorials (tutorial.cpp, _re/notes/tutorials.md): a tutorial world's commands ("attach", "state", "progress", "frame",
// "objects", "test_pass"). Between frames.
#include "tutorial.h"
extern "C" int reflex_world_tutorial(void* world, const char* command, char* out, int32_t size) {
    auto* h = static_cast<Handle*>(world);
    if (!h || h->failed || !command) return -1;
    ClientCoverageScope cov_(h->world.machine());   // REFLEX_CLIENT_COVERAGE (boot.h)
    try {
        std::string err, r = tutorial::world_command(h->world.machine(), command, &err);
        if (!err.empty()) { h->error = err; return -1; }
        if (!out || size <= int32_t(r.size())) return -int(r.size()) - 1;
        std::memcpy(out, r.c_str(), r.size() + 1); return int(r.size());
    } catch (const std::exception& e) { h->error = e.what(); return -1; }
}

// WP-C controller remapping (remap.cpp, _re/notes/input-settings.md "Controller remapping"): the ControllerRemapping table of a world
// ("table") and the local players' KeyMaps built again from it ("apply", after a save was attached). Between frames.
#include "remap.h"
extern "C" int reflex_world_remap(void* world, const char* command, char* out, int32_t size) {
    auto* h = static_cast<Handle*>(world);
    if (!h || h->failed || !command) return -1;
    ClientCoverageScope cov_(h->world.machine());   // REFLEX_CLIENT_COVERAGE (boot.h)
    try {
        std::string err, r = remap::world_command(h->world.machine(), command, &err);
        if (r.empty()) { h->error = err.empty() ? std::string("remap command failed: ") + command : err; return -1; }
        if (!out || size <= int32_t(r.size())) return -int(r.size()) - 1;
        std::memcpy(out, r.c_str(), r.size() + 1); return int(r.size());
    } catch (const std::exception& e) { h->error = e.what(); return -1; }
}

// ---------------------------------------------------------------------------------------------------------------------------
// Platform (additive exports; reflex_world.h "API VERSION, STRUCT SIZES, CRASH REPORTS, PERF"): the API version, the sizes of
// the structs the Unity side marshals, the crash handler (crash.inc) and the perf line.
#include "crash.inc"
#if defined(__APPLE__)
#include <malloc/malloc.h>
#include <mach/mach.h>
#endif

namespace {
struct StructSize { const char* name; int size; };
#define RW_SIZE(t) { #t, int(sizeof(t)) }
const StructSize kStructSizes[] = {
    RW_SIZE(ReflexPad), RW_SIZE(ReflexBikeState), RW_SIZE(ReflexEventSpec), RW_SIZE(ReflexTerrainInfo), RW_SIZE(ReflexParticle),
    RW_SIZE(ReflexParticleInfo), RW_SIZE(ReflexAmbientEntity), RW_SIZE(ReflexAmbientInfo), RW_SIZE(ReflexRaceState),
    RW_SIZE(ReflexDriverResult), RW_SIZE(ReflexCamera), RW_SIZE(ReflexAudioCommand), RW_SIZE(ReflexHudState), RW_SIZE(ReflexSlot),
    RW_SIZE(ReflexVehicleInfo), RW_SIZE(ReflexMudState), RW_SIZE(ReflexBinkInfo), RW_SIZE(ReflexUiNode), RW_SIZE(ReflexUiLayer),
    RW_SIZE(ReflexUiAnim), RW_SIZE(ReflexModeState), RW_SIZE(ReflexModeObject), RW_SIZE(ReflexProp), RW_SIZE(ReflexPropInfo),
    RW_SIZE(ReflexTextureAnim),
};
#undef RW_SIZE
}  // namespace

extern "C" {

int reflex_world_version(void) { return REFLEX_WORLD_API_VERSION; }

int reflex_world_sizeof(const char* name) {
    if (!name) return -1;
    for (const auto& s : kStructSizes) if (std::strcmp(s.name, name) == 0) return s.size;
    return -1;
}

int reflex_world_struct_names(char* out, int32_t size) {
    std::string t;
    for (const auto& s : kStructSizes) { if (!t.empty()) t += ' '; t += s.name; }
    return rwcrash::rwc_copy_out(out, size, t.c_str(), t.size());
}

int reflex_world_perf_stats(char* out, int32_t size) {
    double heap = -1, rss = -1, footprint = -1; int threads = -1;
#if defined(__APPLE__)
    malloc_statistics_t ms{};
    malloc_zone_statistics(nullptr, &ms);
    heap = ms.size_in_use / 1048576.0;
    mach_task_basic_info_data_t bi{}; mach_msg_type_number_t n = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, task_info_t(&bi), &n) == KERN_SUCCESS) rss = bi.resident_size / 1048576.0;
    task_vm_info_data_t vi{}; n = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, task_info_t(&vi), &n) == KERN_SUCCESS) footprint = vi.phys_footprint / 1048576.0;
    thread_act_array_t list = nullptr; mach_msg_type_number_t count = 0;
    if (task_threads(mach_task_self(), &list, &count) == KERN_SUCCESS) {
        threads = int(count);
        for (mach_msg_type_number_t i = 0; i < count; i++) mach_port_deallocate(mach_task_self(), list[i]);
        vm_deallocate(mach_task_self(), vm_address_t(list), count * sizeof(thread_act_t));
    }
#endif
    char t[256];
    snprintf(t, sizeof t, "heap=%.1fMB rss=%.1fMB footprint=%.1fMB threads=%d", heap, rss, footprint, threads);
    return rwcrash::rwc_copy_out(out, size, t, std::strlen(t));
}

}  // extern "C"
