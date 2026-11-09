// The garage through the game's own code: tuning on the running physics, the test drive's world (garage.h; _re/notes/garage.md).
#include "garage.h"
#include "career.h"
#include "modes.h"
#include "vm.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace reflex_vm {
namespace garage {
namespace {
constexpr uint32_t SCRATCH = 0xee390000, SCRATCH_SIZE = 0x10000;   // pages of our own for strings passed to the game (never in an image)
uint32_t rd(Machine& m, uint32_t a) { return m.mem.mapped(a) ? uint32_t(m.mem.read(a, 4)) : 0; }
float rf(Machine& m, uint32_t a) { uint32_t v = rd(m, a); float f; std::memcpy(&f, &v, 4); return f; }
std::string fmt(const char* f, ...) __attribute__((format(printf, 1, 2)));
std::string fmt(const char* f, ...) { char b[1024]; va_list a; va_start(a, f); std::vsnprintf(b, sizeof b, f, a); va_end(a); return b; }
void note(std::string* log, const std::string& s) { if (log) *log += s + "\n"; }
struct Scratch {
    Machine& m; uint32_t at = SCRATCH;
    explicit Scratch(Machine& mm) : m(mm) { for (uint32_t p = 0; p < SCRATCH_SIZE; p += 0x1000) if (!m.mem.mapped(SCRATCH + p)) m.mem.map(SCRATCH + p, nullptr, 0x1000); }
    uint32_t str(const std::string& s) {
        uint32_t n = uint32_t(s.size()) + 1; if (at + n > SCRATCH + SCRATCH_SIZE) throw VmError("garage: string too long for the scratch pages");
        uint32_t a = at; m.mem.write_block(a, s.c_str(), n); at += (n + 15) & ~15u; return a;
    }
};
// A Lua chunk in the game's UI Lua state as the engine runs a script asset (career.cpp "lua"): FUN_00744920 (ECX chunk name, stack(wrapper
// 00d94938, buffer, size), ret 0xc: luaL_loadbuffer 0077a4b0, the function kept as the global "<name>_Chunk"), FUN_00744e70 (cdecl(wrapper,
// "<name>_Chunk", "")) calls it; the global is cleared after.
void lua(Machine& m, const std::string& src) {
    static int serial = 0;
    std::string name = "ReflexGarage" + std::to_string(++serial);
    Scratch sc(m);
    uint32_t a1[3] = {0xd94938, sc.str(src), uint32_t(src.size())};
    m.call_function(0x744920, a1, 3, sc.str(name));
    uint32_t a2[3] = {0xd94938, sc.str(name + "_Chunk"), sc.str("")};
    m.call_function(0x744e70, a2, 3);
    std::string rm = name + "_Chunk = nil";
    Scratch sc2(m);
    uint32_t a3[3] = {0xd94938, sc2.str(rm), uint32_t(rm.size())};
    m.call_function(0x744920, a3, 3, sc2.str(name + "R"));
    uint32_t a4[3] = {0xd94938, sc2.str(name + "R_Chunk"), sc2.str("")};
    m.call_function(0x744e70, a4, 3);
}
// The vehicle spec of a slot's vehicle (App_ApplyVehicleTuning 00743550's search: G+0x154d8 / count +0x154dc, spec+4 == V+0x18c).
uint32_t spec_of(Machine& m, uint32_t V) {
    uint32_t G = rd(m, 0xa946b8), list = rd(m, G + 0x154d8), n = rd(m, G + 0x154dc);
    for (uint32_t i = 0; V && i < n && i < 64; i++) { uint32_t s = rd(m, list + 4 * i); if (s && rd(m, s + 4) == rd(m, V + 0x18c)) return s; }
    return 0;
}
}  // namespace

bool start_test_drive(Boot& b, const Boot::EventSpec& e, uint32_t max_frames, std::string* log) {
    Machine& m = b.machine();
    auto call = [&](uint32_t fn, std::vector<uint32_t> args, uint32_t eax = 0, uint32_t ecx = 0, uint32_t esi = 0, uint32_t edi = 0) {
        Boot::GuestCall c; c.fn = fn; c.args = std::move(args); c.eax = eax; c.ecx = ecx; c.esi = esi; c.edi = edi; return b.call(c); };
    auto ui = [&]() { uint32_t g = b.read32(0xd728d8); return g ? b.read32(g + 0x745c) : 0u; };
    auto set_string = [&](const char* k, const std::string& v) { call(0x5dbf50, {b.guest_string(v)}, 0, 0, ui(), b.guest_string(k)); note(log, std::string("UI ") + k + " = " + v); };
    auto set_int = [&](const char* k, int v) { call(0x5dbfb0, {b.guest_string(k), uint32_t(v)}, 0, ui()); note(log, std::string("UI ") + k + " = " + std::to_string(v)); };
    // the profile signed in with storage (the player's save: its vehicles, parts and PlayerVehicleTuning), at the front end
    if (!career::sign_in(b, log)) { note(log, "test drive: the profile did not sign in with storage"); return false; }
    if (!b.read32(0xa9596c)) call(0x880dc0, {0x10010, 7, 0, 0});   // WM_SETFOCUS (00880dc0), as Boot::start_event
    // the front end's state when the garage is up: Title (the exclusive device, FUN_0059a910), MainMenu:CareerMode (PlayerMode SinglePlayer,
    // EnableTourMode, InCareerMode), the local player's Player List row (EnsureLocalPlayers 0059e030, SetPrimaryPlayerDeviceIndex 00743030)
    m.mem.write(0x9c4ee0, 4, 0); m.mem.write(0xa94070, 4, b.read32(0xa94070) + 1); set_int("ExclusiveDeviceIndex", 0);
    set_string("PlayerMode", "SinglePlayer"); set_int("InCareerMode", 1);
    m.mem.write(career::SERIES_MANAGER + 0x190, 4, 1); set_string("SeriesOwner", "Tour");
    call(0x59e030, {0, 0}); b.run_frames(1);
    call(0x743030, {}, 0);
    if (e.vehicle_owner >= 0) {   // GarageMyRides:EquipVehicle's MXTableHelper:EquipVehicle( row, id, owner ) (004e7930; Boot::start_event)
        int row = -1;
        uint32_t cb = b.read32(0xae0e08 + 0x10), ce = b.read32(0xae0e08 + 0x14);
        for (uint32_t p = cb; p < ce && row < 0; p += 4) {
            uint32_t col = b.read32(p); char name[16] = {}; m.mem.read_bytes(col, name, 15);
            if (std::strcmp(name, "Controller") || b.read32(col + 0x2c) != 1) continue;
            uint32_t holder = b.read32(col + 0x40), first = b.read32(holder + 0xc), last = b.read32(holder + 0x10);
            for (uint32_t r = 0; first + 4 * r < last; r++) if (b.read32(first + 4 * r) == 0) { row = int(r); break; }
        }
        if (row < 0) { note(log, "test drive: no Player List row of controller 0"); return false; }
        call(0x4e7930, {uint32_t(row), uint32_t(e.vehicle_id)}, uint32_t(e.vehicle_owner));
        note(log, fmt("EquipVehicle(row %d, vehicle %d, owner %d)", row, e.vehicle_id, e.vehicle_owner));
    }
    b.run_frames(1);
    // w-testride: the engine's pause word (timing [0xa95844]+0x10 +0xc), the UI world's own (+0x20014), the game state, at each step
    auto pause = [&](const char* at) {
        uint32_t t = b.read32(b.read32(0xa95844) + 0x10), w = b.read32(b.read32(0xa946b8) + 0x20c8);
        note(log, fmt("pause at %s: timing+0xc %u timing+0x10 %u ui world+0x20014 %u game state %u world ready %u ui+0x284 %u ui+0x290 %u", at, b.read32(t + 0xc), b.read32(t + 0x10), w ? b.read32(w + 0x20014) : 99u, b.read32(b.read32(0xa946b8) + 0x18), b.read32(b.read32(0xa946b8) + 0x15b54), b.read32(ui() + 0x284), b.read32(ui() + 0x290)));
    };
    pause("the garage");
    // GarageHUB:OnStartPressed: mxGarage:Unload( ) is a no-op here (the garage was never loaded), App.EnableTestDrive( TRUE ) (00599550: ECX ui,
    // EAX 1), App.SyncDatabaseWithManagers( ) (Game_SetupActivityArea 0075c430, ESI game), "GotoLoadingPage"
    call(0x599550, {}, 1, ui());
    uint32_t G = b.read32(0xa946b8);
    // REFLEX_TEST_DRIVE_DIRECT (w-testride, Verify): the steps as direct calls between frames (Boot::call) instead of inside the main
    // thread's frame (call_in_frame returns through the frame's own trampoline, which a verified call cannot compare against)
    const bool direct = std::getenv("REFLEX_TEST_DRIVE_DIRECT") != nullptr;
    { Boot::GuestCall c; c.fn = 0x75c430; c.esi = G; b.call_in_frame(c, 60); }
    note(log, fmt("EnableTestDrive: ui+0x290 %u ui+0x284 %u", b.read32(ui() + 0x290), b.read32(ui() + 0x284)));
    // the Test Drive Loading Page: TestDriveTransitionPage:Present -> mxGarage:TestDriveStart( ) (00762a20, EAX garage), then the activity
    // area's setup each frame until the local slot has its vehicle and rider (PlayerSlot_SpawnStep waits for the loader)
    pause("EnableTestDrive + Game_SetupActivityArea");
    { Boot::GuestCall c; c.fn = 0x762a20; c.eax = G + GARAGE; if (direct) b.call(c); else b.call_in_frame(c, 600); }
    pause("TestDriveStart");
    uint32_t P = G + 0x7460, f = 0;
    for (; f < max_frames && !(b.read32(P + 0x1ac) && b.read32(P + 0x1b4)); f++) {
        Boot::GuestCall c; c.fn = 0x75c430; c.esi = G;
        b.call_in_frame(c, 60);
    }
    if (!b.read32(P + 0x1ac) || !b.read32(P + 0x1b4)) { note(log, fmt("test drive: no vehicle / rider after %u frames", f)); return false; }
    uint32_t A = b.read32(P + 0x94);
    note(log, fmt("test drive: spawned after %u frames: activity %08x (vtable %08x), vehicle %08x (vtable %08x), rider %08x", f, A, A ? b.read32(A) : 0, b.read32(P + 0x1ac), b.read32(b.read32(P + 0x1ac)), b.read32(P + 0x1b4)));
    pause("spawned");
    b.run_frames(60);
    pause("60 frames later");
    // GotoInGameHUB: InGamePageGraph "Fullscreen HUB" and the activity's route (modes.h)
    note(log, "UI: " + ui_activate(m));
    return true;
}

void end_test_drive(Boot& b, std::string* log) {
    // the pause request in a test drive (MXUIManager_OnPauseRequest 00598e40, ui+0x290 set): ui+0x294 = 1, EnableTestDrive( FALSE )
    // (00599550: ECX ui, EAX 0; the hub route "GotoGarageVehiclePage" and GotoHub, which the live UI follows to the Test Drive Exit Page,
    // whose TestDriveTransitionPage:Present calls MXGarage_TestDriveEnd 00762cc0)
    uint32_t g = b.read32(0xd728d8), ui = g ? b.read32(g + 0x745c) : 0;
    if (!ui) { note(log, "test drive end: no UI manager"); return; }
    b.machine().mem.write(ui + 0x294, 4, 1);
    Boot::GuestCall c; c.fn = 0x599550; c.eax = 0; c.ecx = ui;
    b.call_in_frame(c, 600);   // in the frame: the players' unload waits for other threads (a direct call would block)
    note(log, fmt("test drive end: EnableTestDrive( FALSE ): ui+0x290 %u ui+0x284 %u", b.read32(ui + 0x290), b.read32(ui + 0x284)));
    if (std::getenv("REFLEX_TEST_DRIVE_END_CALL")) {   // MXGarage_TestDriveEnd called here (EAX = the garage), not by the exit page
        uint32_t G = b.read32(0xa946b8);
        Boot::GuestCall e; e.fn = 0x762cc0; e.eax = G + GARAGE; b.call_in_frame(e, 600);
        note(log, "test drive end: MXGarage_TestDriveEnd");
    }
}

uint32_t camera_of(Memory& mem, uint32_t cam) {
    auto u = [&](uint32_t a) { return mem.mapped(a) ? uint32_t(mem.read(a, 4)) : 0u; };
    if (cam != 0xd6f620) return cam;
    uint32_t game = u(0xd728d8); if (!game || u(game + 0x18) != 2) return cam;
    for (uint32_t o : {0x10u, 0x14u, 0x18u, 0x40u, 0x44u}) if (u(cam + o) != 0) return cam;   // the static camera was set: as given
    uint32_t G = u(0xa946b8), e = G ? u(G + 0x7460 + 0x9a8) : 0;
    return e && u(e) ? e + 0x90 : cam;
}

std::string world_command(Machine& m, const std::string& cmd, std::string* error) {
    struct Regs { Machine& m; uint32_t sp, bp; explicit Regs(Machine& mm) : m(mm), sp(mm.r[4]), bp(mm.r[5]) {} ~Regs() { m.r[4] = sp; m.r[5] = bp; } } keep(m);   // cdecl calls leave their arguments
    size_t sp = cmd.find(' ');
    std::string verb = cmd.substr(0, sp), rest = sp == std::string::npos ? "" : cmd.substr(sp + 1);
    if (verb == "lua") { if (rest.empty()) { if (error) *error = "lua <source>"; return ""; } lua(m, rest); return "{}"; }
    if (verb == "apply_tuning") { m.call_function(0x743550, nullptr, 0, -1, uint32_t(std::atoi(rest.c_str()))); return "{}"; }
    if (verb == "tuning") {
        int slot = std::atoi(rest.c_str());
        uint32_t G = rd(m, 0xa946b8), P = G + 0x7460 + 0x12a0 * uint32_t(slot), V = G ? rd(m, P + 0x1ac) : 0, S = V ? rd(m, V + 0x234) : 0, s = spec_of(m, V);
        std::string j = fmt("{\"vehicle\":%u,\"spec\":%u,\"playerVehicleId\":%d,\"values\":[", V, s, s ? int(rd(m, s + 0x9c)) : -1);
        for (int i = 0; i < 8; i++) j += fmt("%s%d", i ? "," : "", s ? int(rd(m, s + 0xa0 + 4 * uint32_t(i))) : 0);
        j += "],\"defaults\":[";   // the vehicle model's values the tuning adds to (spec+8 -> +0x1c..+0x38: Default*1/2, Braking, Steering)
        uint32_t md = s ? rd(m, s + 8) : 0;
        for (int i = 0; i < 8; i++) j += fmt("%s%d", i ? "," : "", md ? int(rd(m, md + 0x1c + 4 * uint32_t(i))) : 0);
        j += "],\"vehicleTuning\":[";
        for (int i = 0; i < 5; i++) j += fmt("%s%.9g", i ? "," : "", V ? double(rf(m, V + 0x220 + 4 * uint32_t(i))) : 0.0);
        j += "],\"simTuning\":[";
        for (int i = 0; i < 4; i++) j += fmt("%s%.9g", i ? "," : "", S ? double(rf(m, S + 0x2ef0 + 4 * uint32_t(i))) : 0.0);
        return j + "]}";
    }
    if (error) *error = "unknown garage command: " + cmd;
    return "";
}
}  // namespace garage
}  // namespace reflex_vm
