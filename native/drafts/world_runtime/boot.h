// Boots the unchanged MXReflex.exe from the player's own install inside the interpreter
// (no captured world image): PE loading of the executable and the VC90 runtime, a Win32
// process (threads, handles, heaps, virtual memory, files over the install, time), and
// stand-ins for the devices (Direct3D 9, DirectInput, XInput, FMOD, Steam, Bink, Winsock).
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace reflex_vm {
class Machine;

struct BootConfig {
    std::string game_dir;    // folder holding MXReflex.exe, Database/, ... (read-only)
    std::string crt_dir;     // folder holding msvcr90.dll / msvcp90.dll (VC90 SP1 9.00.30729.1)
    std::string save_dir;    // writable overlay: everything the game writes lands here
    std::string log_path;    // optional diagnostics log
    uint32_t width = 1280, height = 720;
    uint32_t cpus = 2;
    bool jit = false;        // block JIT (exact: same results and thread switches as the interpreter)
    // Baton scheduling (native/game notes/loading.md): every guest thread runs on its own host thread, one at a time; switches happen
    // only where the game synchronizes (a wait that blocks, Sleep, SwitchToThread, a zero-timeout wait that times out), never at
    // instruction counts, so natives (ports) can block inside nested calls; virtual time is logical (QueryPerformanceCounter +10 us per
    // read and per lonely yield, timeouts when every thread waits), so the schedule does not depend on how many instructions any code takes.
    // Boot::start_event under baton also waits for the loading page's logo video open before it returns (the capture moment, boot.cpp).
    // False (default): the cooperative scheduler with exact instruction-count slices (the reference worlds).
    bool baton = false;
    // With baton: also switch at the exact scheduler's instruction-count slices, with its instruction-driven time (a check of the baton
    // machinery: the same schedule, so the same world images, as baton = false). Natives must not be installed (a slice inside one breaks it).
    bool baton_slices = false;
    // MOTOCAREER (career.h): non-empty = the Steam Cloud stand-in over this folder (savegame.bxml) and an offline Steam user
    // named steam_persona, so the profile signs in with storage as the PC game does. Empty (default): no Steam, as before.
    std::string remote_storage, steam_persona = "Player";
};

class Boot {
public:
    explicit Boot(const BootConfig& config);
    ~Boot();
    // Loads the images, runs the DLL initialisation and the executable's entry until the
    // main thread first enters Engine::Frame (0084f260).
    void start();
    // Runs every thread until the main thread has entered Engine::Frame `frames` more times.
    // Virtual time advances by dt_100ns per frame (0: by executed instructions).
    void run_frames(uint32_t frames, uint64_t dt_100ns = 0);
    // Runs until pred() holds (checked at each main-thread frame) or `max_frames` frames.
    bool run_until(const std::function<bool(Boot&)>& pred, uint32_t max_frames, uint64_t dt_100ns = 0);
    Machine& machine();
    uint64_t frames() const;        // Engine::Frame entries so far
    uint64_t now() const;           // virtual time (100 ns)
    bool baton() const;             // baton scheduling (BootConfig::baton without baton_slices): logical time, switches at synchronization only
    // The front end's settle under baton (native/game notes/loading.md section 3): loading costs no virtual time there, so the front end is
    // reached after ~1 s instead of ~5 s, before its timed steps (the startup packages' asset binding); every start path runs frames until
    // 5 s virtual after it reached the front end (state 2 + its 30 frames), as start_event does. Returns the frames run (0: not baton).
    uint32_t settle_front_end(uint32_t max_frames = 600);
    // Under baton, after a level's loading: frames until the loading page's logo video (BinkOpen of a .bik after `opens_before` opens) as
    // start_event waits for it (at most max_frames). Returns the frames run (0: not baton or already opened).
    uint32_t bink_opens() const;    // BinkOpen calls of a .bik file so far
    uint32_t wait_logo_video(uint32_t opens_before, uint32_t max_frames = 600);
    std::string status() const;     // threads, handles, last activity (diagnostics)
    void set_keys(const uint8_t keys[256]);
    // A call into the game on its main thread, between frames (registers as the original ABI wants).
    struct GuestCall { uint32_t fn = 0, eax = 0, ecx = 0, edx = 0, esi = 0, edi = 0; std::vector<uint32_t> args; };
    uint32_t call(const GuestCall& c);
    // The same call made by the main thread as part of its next frames (the other threads run while it waits); returns its EAX.
    uint32_t call_in_frame(const GuestCall& c, uint32_t max_frames = 600);
    uint32_t guest_string(const std::string& s);   // NUL-terminated copy in guest memory
    uint32_t read32(uint32_t address);
    // REFLEX_BOOT_DEFORM (w-gridpair, test hook): the terrain deformation pass (wheel ruts) in the boot. The boot's null D3D device
    // only counts draws, so its deformation render targets never change (no ruts); with this the world runtime's CPU model of the GPU
    // pass (deform.cpp, the model the world tests use; `model_path` an RXDF file, e.g. a world image's <world>-deform-model.bin) renders
    // each frame's stamp queue into both render targets at HFTerrainSurface::vf14's first FlushPending, as runtime.cpp does. False: no model.
    bool enable_deformation(const std::string& model_path);
    // Starts an event through the game's own front-end functions (as the shipping Lua does:
    // sign-in, local player, event properties, SetupEvent, StartEvent, GotoLoadingPage), then
    // runs the loading and its confirmation until the world's race activity exists.
    struct EventSpec {
        std::string type = "National", location = "NAT_Concord", duration_type = "Laps";
        int type_id = 10, location_id = 50, track_id = 0, vehicle_class = 3, riders = 1, ai = 0, duration = 2, activity = 0;
        // Vehicle Select (VehicleSelect:End, _re/notes/vehicle-select.md): the race's rider and vehicle, equipped on the local
        // player's "Player List" row with the game's MXTableHelper EquipRider (004e7ae0) / EquipVehicle (004e7930) before
        // SetupEvent. Owners (MXRiderOwner / MXVehicleOwner, 0x9dcf30 / 0x9dceec): 0..3 Player1..4, 4 AI, 8 Preset (vehicle).
        // -1: not equipped (EnsureLocalPlayers' default, the profile's PlayerRider LastRiderId / LastEquippedVehicle).
        int rider_owner = -1, rider_id = 0, rider_gear_set = 0;   // the player's own rider: owner 0, 0, 0; a pro: 4, AIRiderId, gear set
        int vehicle_owner = -1, vehicle_id = 0;                    // owner 0 PlayerVehicleId, 4 AIVehicleId, 8 PresetVehicleId
        int vehicle_type = -1;                                     // UI Player1VehicleTypeId (-1: not set)
        // MOTOCAREER (career.h): a Tour series of the Series table instead of the arcade event properties above (-1: arcade);
        // series_vehicle_type: progress:SetVehicleTypeId (the VehicleTypeId of the profile's equipped vehicle, -1: not set).
        int series_id = -1, series_vehicle_type = -1;
        int series_skip_practice = -1;   // Heat / Moto: CareerPracticePopup's progress:SetSkipPractice (-1: not set)
        std::vector<std::vector<uint32_t>> series_outcomes;   // Heat / Moto: the outcomes of the series' races already run (career::race_outcome)
        std::vector<int> series_gates;   // Heat / Moto: the player's gate at each gate selection of the series so far (-1: the page's default, the first available; WP-5)
        // Free Ride challenges (modes.h, _re/notes/modes.md): MXUIManager::SetupEvent (0059a970) takes the activity from the UI variable
        // EventTypeActivation when EventType is FreeRide - what the game sets in an activation area (HillClimb, Trailblazer, HitDiscovery,
        // TargetPractice) before its AAA loading page (FUN_00598e40) starts the event; TrackId = the area's variant. Empty: not set.
        std::string activation;
        int series_challenge = -1;   // the Race & Ride challenges series (UI SeriesId; RaceAndRideChallenges:OnAcceptChallenge), -1: not set
        // The UI's exclusive device (FUN_0059a910: DAT_009c4ee0, its count DAT_00a94070, UI ExclusiveDeviceIndex), which the front end's
        // title page gives the controller that pressed start; the pages' Lua look the player's tables up with it
        // (App.LookupPlayerTable(..., App.GetExclusiveDeviceIndex())). -1: left as the boot has it (-1).
        int exclusive_device = -1;
        // The mouse cursor away from the screen (GetCursorPos -10000, -10000) from SetupEvent on: the UI's list containers give the focus
        // to the child under the mouse (UIListContainer::vf8 005d0000, MouseHover), so the stand-in's 0, 0 would take it from the pages'
        // authored focus (PreChallengePopup's ButtonText holds the accept trigger). False: 0, 0 as before.
        bool cursor_off_screen = false;
    };
    bool start_event(const EventSpec& e, uint32_t max_frames, std::string* log = nullptr);
    std::function<void(uint32_t frames, int game_state)> progress;   // called each frame of start_event
    // Writes the running process (between frames) as a paged world image (RXWI, the format of
    // _re/trace/pack_world.py, header word 15 = 'BOOT' plus the boot's import table at 0x20000)
    // for the world runtime, the terrain's deformation surface (2048x2048 R32F) and the
    // deformation model (RXDF: the level's SoftnessMask and the HFT_Deform_Default brush,
    // read from the game's own loaded textures). Device memory is left out.
    void write_world(const std::string& world, const std::string& deform, const std::string& model, std::string* info = nullptr);
    // A layout-free (canonical) dump of the running process's heap, for comparing worlds made by different boots (native/game
    // tools/world_compare.py, notes/loading.md): every heap block (and other VirtualAlloc region) reached from the modules' globals and
    // the threads' TLS (then from the live stacks), named by its first path from those roots, with its class (RTTI), size and contents,
    // pointers given as (target name, offset) instead of addresses. device: also the device blocks' contents (textures, buffers).
    void write_canonical(const std::string& path, bool device = false, std::string* info = nullptr);
    // RENDER CAPTURE (render_capture.cpp): a hook called before (after = false) and after (after = true) every method of
    // every interface of the Direct3D 9 recording device ("Device", "Texture", "VertexBuffer", ...), and the device's view
    // of one of its objects (kind: Res::Kind of boot_devices.inc; level arrays: per level, cube: 6 * levels; an ID3DXConstantTable: kind 16,
    // bytes = its CTAB).
    struct D3DResource { int kind = -1; uint32_t width = 0, height = 0, depth = 1, levels = 1, format = 0, usage = 0, pool = 0, fvf = 0, size = 0,
        data = 0, container = 0, level = 0, face = 0, version = 0; std::vector<uint32_t> level_addr, level_pitch, level_bytes, level_w, level_h;
        std::vector<uint8_t> bytes; std::string label; };
    using D3DHook = std::function<void(Machine&, const std::string& iface, uint32_t self, const std::string& method, bool after)>;
    void set_d3d_hook(D3DHook hook);
    bool d3d_resource(uint32_t object, D3DResource& out) const;
    struct Impl;
private:
    Impl* d;
};

// The Unity client's boots (libreflexworld: reflex_world_build, reflex_profile_open; native/game notes/loading.md section 9): baton
// scheduling and the native game's ports with the race's setting (reflex_world_boot_ports: the client's native-game mode and filter).
// mode 0: no ports (the original code everywhere), 1: the ports run (On), 2: Verify (every call of a port runs both ways, the original
// is kept). Default (never set): REFLEX_NATIVE_GAME (0/1/2) if set, else 1; filter "all" (REFLEX_NATIVE_PORTS overrides).
void set_client_boot_ports(const std::string& filter, int mode);
void client_boot_ports(std::string& filter, int& mode);
// c.baton = true (REFLEX_BOOT_SCHED=exact: the exact-slice scheduler and no ports, the old reference worlds).
void client_boot_config(BootConfig& c);
// Installs the client's ports into a Boot made with client_boot_config, before Boot::start (the whole boot runs them). Returns the
// number installed (0: mode 0 or the exact scheduler).
int install_client_boot_ports(Boot& b, const BootConfig& c);
// REFLEX_CLIENT_COVERAGE=<prefix> (diagnostics, native/game tools/gap_functions.py): what original code the client's boots and world
// commands run. client_coverage_attach starts counting on a machine (interpreted: coverage turns the JIT off while counts are on);
// client_coverage_write writes <prefix>-<tag>.bin (exe counts, the CRT DLLs in .bin.dll / .bin.dll.tsv as the CLI's REFLEX_COVERAGE) and
// zeroes the counts; detach stops counting (keep: the counts stay for a later attach). No-ops without the variable.
bool client_coverage_enabled();
void client_coverage_attach(Machine& m);
void client_coverage_detach(Machine& m);
void client_coverage_write(Machine& m, const std::string& tag);
void client_coverage_forget(Machine& m);
// Counting only inside a scope (a world command between frames): attach in the constructor, detach in the destructor.
struct ClientCoverageScope { Machine* m; explicit ClientCoverageScope(Machine& mm) : m(client_coverage_enabled() ? &mm : nullptr) { if (m) client_coverage_attach(*m); }
    ~ClientCoverageScope() { if (m) client_coverage_detach(*m); } };
}  // namespace reflex_vm
