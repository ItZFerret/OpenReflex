// Public C API of the Reflex original-physics world plugin (libreflexworld).
// The plugin executes the original Reflex input update and outer physics update
// (unchanged original x86 code) over a world image; see README.md.
#pragma once
#include <stdint.h>
#if defined(_WIN32)
#define REFLEX_API __declspec(dllexport)
#else
#define REFLEX_API __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif

// XInput-compatible gamepad state (XINPUT_GAMEPAD layout and ranges).
typedef struct ReflexPad {
    uint16_t buttons;            // XINPUT_GAMEPAD_* bits
    uint8_t left_trigger, right_trigger;   // 0..255
    int16_t lx, ly, rx, ry;      // -32768..32767
} ReflexPad;

// Bodies: 0 bike frame, 1 rider, 2 front wheel, 3 rear wheel. Native world units,
// native axes (left-handed, +Y up; the same axes the Unity Alpine scene uses).
// Quaternions are (x, y, z, w).
typedef struct ReflexBikeState {
    float position[4][4];          // current physics pose (S+0xf0+0xa0*i)
    float orientation[4][4];       // (S+0x730+0xa0*i)
    float render_position[4][4];   // scheduler-interpolated pose the original renderer draws (B+0x90)
    float render_orientation[4][4];// (B+0x6d0)
    float velocity[4];             // body 0 linear velocity (S+0x100)
    float speed;                   // S+0x94
    int32_t gear;                  // S+0x8ac0 (0 = first)
    float inputs[8];               // sampled input block S+0x1c: throttle, reverse, front brake, rear brake, steer x/y, rider lean x/y
    uint32_t frames;               // frames run since create
    uint32_t reserved;
} ReflexBikeState;

// Paths: original MXReflex.exe, VC90 msvcr90.dll (the game's redistributable),
// world image, and the 2048x2048 R32F deformation grid. Returns NULL on failure.
REFLEX_API void* reflex_world_create(const char* exe_path, const char* crt_path, const char* world_path, const char* deform_path);

// Building a world from the player's install (boot.h): the unchanged MXReflex.exe is booted inside the
// interpreter, the event is started through the game's own front-end functions, and the running process is
// written as <out_prefix>-world.bin (paged world image), -deform.bin (deformation surface) and
// -deform-model.bin (softness mask + brush). Blocking (tens of seconds); progress can be polled from another thread.
typedef struct ReflexEventSpec {
    char type[32], location[64], duration_type[16];   // "National" / "NAT_Concord" / "Laps"; "FreeRide" / "FR_Alpine" / ""
    int32_t type_id, location_id, track_id, vehicle_class, riders, ai, duration, activity;
    // Vehicle Select (appended; read only when garage == REFLEX_GARAGE_SET, so a caller with the older, shorter struct is
    // unaffected): the race's rider and vehicle as VehicleSelect:End equips them (MXTableHelper EquipRider / EquipVehicle on the
    // local player's row before SetupEvent; _re/notes/vehicle-select.md). Owners: 0 Player1 (the player's own rider 0/0, a
    // PlayerVehicleId), 4 AI (an AIRiderId + its RiderGearSetId, an AIVehicleId), 8 Preset (vehicle); -1 leaves that one as the
    // profile has it. vehicle_type: the UI variable Player1VehicleTypeId (-1 unset).
    int32_t garage, rider_owner, rider_id, rider_gear_set, vehicle_owner, vehicle_id, vehicle_type;
    // MOTOCAREER (appended; read only when garage == REFLEX_GARAGE_SET and career == REFLEX_CAREER_SET, owners -1 = the profile's):
    // the event is the Tour series `series_id` of the Series table (career.h start_series_event), started with the profile signed in
    // with storage from <save_dir>/remote/savegame.bxml (the Steam Cloud stand-in) as the offline Steam user <save_dir>/steam_persona.txt.
    int32_t career, series_id, series_vehicle_type;
    // Heat / Moto (appended; read only when career == REFLEX_CAREER_SET2): the CareerPracticePopup answer (-1 unset, 0 practice, 1 skip)
    // and the outcomes of the series' races already run (reflex_world_career "outcome"), series_outcomes of them concatenated in
    // <save_dir>/career_outcomes.bin; the boot replays them and builds the world of the series' next race.
    int32_t series_skip_practice, series_outcomes;
    // Game modes (appended; read only when garage == REFLEX_GARAGE_SET and modes == REFLEX_MODES_SET; Boot::EventSpec, modes.h): the UI
    // variable EventTypeActivation (a Free Ride challenge: HillClimb, Trailblazer, HitDiscovery, TargetPractice; "" none), the Race & Ride
    // challenges series (UI SeriesId, -1 none), the UI's exclusive device (-1 leave), the mouse cursor away from the screen (0/1).
    int32_t modes;
    char activation[32];
    int32_t series_challenge, exclusive_device, cursor_off_screen;
    // WP-G test drive (appended; read only when garage == REFLEX_GARAGE_SET and test_drive == REFLEX_TESTDRIVE_SET; garage.h): the garage's
    // TEST DRIVE (GarageHUB:OnStartPressed, the Test Drive Loading Page: MXGarage TestDriveStart) in the front end's own world UI_Compound,
    // booted with the profile signed in with storage from <save_dir>/remote (as a career event); vehicle_owner / vehicle_id as above (-1: the
    // profile's LastEquippedVehicle). The other event fields are not used.
    int32_t test_drive;
} ReflexEventSpec;
#define REFLEX_GARAGE_SET 0x45474152   /* 'RAGE' */
#define REFLEX_CAREER_SET 0x45524143   /* 'CARE' */
#define REFLEX_CAREER_SET2 0x32524143  /* 'CAR2': REFLEX_CAREER_SET plus the Heat / Moto fields */
#define REFLEX_MODES_SET 0x45444f4d    /* 'MODE': the game-mode fields */
#define REFLEX_TESTDRIVE_SET 0x56495244 /* 'DRIV': the WP-G test drive field */
REFLEX_API int reflex_world_build(const char* game_dir, const char* crt_dir, const char* save_dir, const ReflexEventSpec* event, const char* out_prefix);
REFLEX_API float reflex_world_build_progress(char* status, int size);
REFLEX_API const char* reflex_world_build_error(void);
// The VC90 SP1 runtime from the install's Installers/vcredist_x86.exe into out_dir (msvcr90.dll, msvcp90.dll).
REFLEX_API int reflex_world_extract_crt(const char* installer, const char* out_dir);
REFLEX_API const char* reflex_world_extract_crt_error(void);
// Last error for a world, or for create when world is NULL.
REFLEX_API const char* reflex_world_error(void* world);
// The native game (native/game): installs the selected ports into this world (filter: "all", groups, names or hex addresses,
// "-x" excludes; mode 0 off, 1 the natives run, 2 verify: both run, the original is kept, differences counted). Returns the number
// installed (call before the first frame or between frames). reflex_world_port_report: one line per installed port (calls,
// verified, mismatches, first difference) into out; returns the full length.
REFLEX_API int reflex_world_port(void* world, const char* filter, int mode);
REFLEX_API int reflex_world_port_report(char* out, int size);
// The native game in the boots the plugin runs (reflex_world_build and reflex_profile_open: baton scheduling, native/game notes/loading.md
// section 9): the ports installed for the whole boot / profile session, with the same filter and mode as reflex_world_port (0: the
// original code everywhere, 1: the natives run, 2: verify). Applies to the builds and sessions started after the call (a session already
// open keeps its ports). Never called: REFLEX_NATIVE_GAME (0/1/2) if set, else 1, filter "all". Returns 0. (Added 2026-10-18; an older
// plugin has no such export: its builds run the original code under the exact-slice scheduler.)
REFLEX_API int reflex_world_boot_ports(const char* filter, int mode);
// One original frame: input manager update then outer physics update with this
// frame dt (seconds). keys: 256 DirectInput scan-code states (0x80 = down).
// pads: 4 entries (may be NULL when pad_mask is 0); bit i of pad_mask = pad i connected.
// Returns 0 on success; nonzero leaves the world in an unusable state (recreate it).
REFLEX_API int reflex_world_frame(void* world, float dt, const uint8_t* keys, const ReflexPad* pads, uint32_t pad_mask);
REFLEX_API int reflex_world_bike(void* world, ReflexBikeState* out);
// Whole frame at an absolute performance-counter value (100 ns units; replays a recorded live frame).
REFLEX_API int reflex_world_frame_at_counter(void* world, uint64_t counter, const uint8_t* keys, const ReflexPad* pads, uint32_t pad_mask);
// Rider skeleton (MXRider_Hierarchy, 69 nodes): writes min(69, max_bones) world
// matrices (row-major, row vectors, native world space) to out, 16 floats each, as
// the original renderer completes them. Returns the bone count, negative on error.
REFLEX_API int reflex_world_rider_pose(void* world, float* out, uint32_t max_bones);
// The terrain the physics collides with (the selected HFTerrainSurface).
// Grid: samples x samples heights, column c at x = min_x + c * spacing, row r at
// z = max_z - r * spacing. deform_version changes whenever the deformation surface
// changes; dirty_* is the grid rectangle [x0,x1) x [z0,z1) changed since the last call.
typedef struct ReflexTerrainInfo {
    uint32_t samples, stride;
    float spacing, min_x, min_z, max_x, max_z, gain;
    uint32_t deform_version;
    int32_t dirty_x0, dirty_z0, dirty_x1, dirty_z1;
} ReflexTerrainInfo;
REFLEX_API int reflex_world_terrain(void* world, ReflexTerrainInfo* out);
// Collision heights base*2048 + (deformation-0.5)*2*gain for columns [x0,x0+w), rows [z0,z0+h); out is h rows of w floats.
REFLEX_API int reflex_world_terrain_heights(void* world, float* out, int x0, int z0, int w, int h);
// The deformation surface (the game's DeformSampler input; 0.5 = untouched, ruts below, berms above)
// for the same rect; height = base*2048 + (deform - 0.5)*2*gain.
REFLEX_API int reflex_world_terrain_deform(void* world, float* out, int x0, int z0, int w, int h);
// Decorative ambient animation (crowds, crew chiefs, looping/spline/scenario animations): 1 runs it
// (default, original), 0 skips its per-frame update (with reflex_world_ambient_mask exceptions); gameplay never reads it.
REFLEX_API int reflex_world_set_ambient_animation(void* world, int simulate);
// Particles (particles.cpp, _re/names/74_particles.tsv): enable 1 keeps the particle managers simulated while ambient
// animation is off (bike state unchanged). After a frame, reflex_world_particles writes the live particles as the original
// renderer draws them (PyroParticles UncompressedNoLightVS): normal manager first, sorted far to near (10000 view-depth
// buckets, at most 3000), then the additive manager (same rule). Each is a camera-facing quad in the view plane:
//   corner c in (-1,1) (1,1) (1,-1) (-1,-1) with atlas uv (0,0) (1,0) (1,1) (0,1) (D3D v down) -> uv * frame.xy + frame.zw;
//   X = size.x * scale * (c.x - pivot.x), Y = size.y * scale * (c.y - pivot.y); view offset (X cos r + Y sin r, Y cos r - X sin r)
//   along the camera's right / up; colour = color * color1 (D3DCOLOR ARGB) * (luminance + shadow * luminance_delta);
//   alpha * sat(fade.x * area + fade.y) with area = (size.x scale / w) (size.y scale / w), killed below fade.z.
// Blend: flags bit 0 clear SRCALPHA / INVSRCALPHA, set SRCALPHA / ONE; Z test on, no Z write. texture: the level's particle
// atlas (asset type "texture", same name as the level's ParticleEmitterAsset). Returns the count written (<= max), <0 on error.
typedef struct ReflexParticle {
    float position[3];       // world, native axes
    float size[2];           // quad half extents (before scale)
    float pivot[2];          // quad pivot in corner units
    float scale, rotation;   // rotation: radians, in the view plane
    uint32_t color, color1;  // D3DCOLOR (A8R8G8B8), multiplied
    float uv[4];             // atlas frame: scale u, v, offset u, v (D3D: v = 0 at the top)
    uint32_t flags;          // bit 0: additive
} ReflexParticle;
typedef struct ReflexParticleInfo {
    char texture[64];
    float luminance[4], luminance_delta[4];   // c101 g_NoLightLuminance, c102 g_LightLuminanceDelta
    float fade[4];                            // c100 g_ScreenAreaToFadeAndAlphaKill
    int32_t soft, normal, additive, live;     // soft particles (NoLightPS depth fade) on; counts written; live particles
    float near_plane, far_plane, reserved[2]; // the render camera's planes (soft-particle depth is D3D z/w with these)
} ReflexParticleInfo;
REFLEX_API int reflex_world_particles_enable(void* world, int enabled);
REFLEX_API int reflex_world_particles(void* world, ReflexParticle* out, uint32_t max, ReflexParticleInfo* info);
// Ambient animated entities (ambient.cpp, _re/names/76_ambient.tsv). With ambient animation off, reflex_world_ambient_mask
// keeps the per-frame update (vf19) of the classes whose bit is set (the others stay skipped; bike state unchanged either way):
#define REFLEX_AMBIENT_SINGLE_LOOP     0x001   // AnimEntitySingleAnimLoop (pit crew, score-tower people, flags; crowd templates)
#define REFLEX_AMBIENT_SCENARIO        0x002   // ScenarioAnimEntity (intro / podium actors, the Alpine hovercraft)
#define REFLEX_AMBIENT_SPLINE_LOOP     0x004   // AnimEntitySplineAnimLoop (walking crowd)
#define REFLEX_AMBIENT_CREW_CHIEF      0x008   // CrewChief
#define REFLEX_AMBIENT_INSTANCED       0x010   // InstancedAnimationEntity (grandstand crowd; needs SINGLE_LOOP for its templates)
#define REFLEX_AMBIENT_SINGLE_LOOP_P   0x020   // AnimEntitySingleAnimLoopWithParticles
#define REFLEX_AMBIENT_SPLINE_LOOP_P   0x040   // AnimEntitySplineAnimLoopWithParticles (spline-carried emitters)
#define REFLEX_AMBIENT_LAND_ANIMAL     0x080   // LandAnimalProximity (roaming people)
#define REFLEX_AMBIENT_BILLBOARD_CROWD 0x100   // BillboardCrowdEntity
#define REFLEX_AMBIENT_FLAG_HOLDER     0x200   // ScoreTowerFlagHolder
REFLEX_API int reflex_world_ambient_mask(void* world, uint32_t keep);
// After a frame: every ambient entity (out, at most max) and the node world matrices of the skinned ones (matrices: 16 floats
// each, row vectors, native axes; at most max_matrices). kind: index into the class table (0..9 as the mask bits above, then
// 10 TrackSideFlagHolder, 11 AnimEntitySplineWithStopLogic, 12 ...WithParticles, 13 AnimEntityProximity, 14 AnimEntityTrigger,
// 15 AirAnimalSoaring: always simulated). flags: bit 0 drawn (model instance +0xa2), bit 1 skinned (bones node matrices from
// first_matrix, in the model's node order), bit 2 crowd instance (draw entity `source`'s pose with `matrix` = template pose ->
// instance, row vectors: v_instance = v_template_pose * matrix), bit 3 crowd template (pose source only, not drawn by itself),
// bit 4 animated billboard crowd, bit 5 some nodes completed host-side. matrix: skinned: the entity node (placement); billboard
// crowd: the entity world matrix. lod_distance: culled beyond (ModelAsset +0x120 / renderer divisor), alpha fading from
// info.fade_start * lod_distance^2 (squared distances). param: billboard crowd: the Crowd shader's time (c65), emptyChairRatio
// (c66), non-animated (c71 = 0 when set), active camera flashes;
// crowd instance: LOD index. texture: see the field. Returns the entity count written, <0 on error. reflex_world_ambient_rescan: find the objects again.
typedef struct ReflexAmbientEntity {
    uint32_t object, entity_id;
    int32_t kind;
    uint32_t flags;
    int32_t bones, first_matrix, source;
    float lod_distance;
    float param[4];
    float matrix[16];
    char model[64];
    char texture[64], texture_package[64];   // crew chief: the texture replacing CrewChief_ShirtColor_Default (the local rider's shirt) and its package
} ReflexAmbientEntity;
typedef struct ReflexAmbientInfo {
    int32_t entities, matrices, objects, reserved0;
    float fade_start, lod_divisor, reserved[2];
} ReflexAmbientInfo;
REFLEX_API int reflex_world_ambient(void* world, ReflexAmbientEntity* out, uint32_t max, float* matrices, uint32_t max_matrices, ReflexAmbientInfo* info);
REFLEX_API int reflex_world_ambient_rescan(void* world);
// Race progress of the local player as the game tracks it (national/race activities).
// state: 0 Pre, 1 Countdown (start board), 2 InProgress, 3 WindDown, 4 Post, 5 Inactive.
typedef struct ReflexRaceState {
    int32_t valid, state, num_laps, laps_completed, place, riders;
    float lap_time, last_lap, best_lap, race_time, lap_progress, start_timer, reset_timer;
} ReflexRaceState;
REFLEX_API int reflex_world_race(void* world, ReflexRaceState* out);
// Every rider in the race: player slot (as reflex_world_slots), local flag, place (1-based), laps, finished, times (seconds;
// race_time stops at the rider's finish).
typedef struct ReflexDriverResult {
    int32_t slot, local, place, laps_completed, finished;
    float race_time, best_lap, last_lap;
} ReflexDriverResult;
REFLEX_API int reflex_world_race_drivers(void* world, ReflexDriverResult* out, uint32_t max);
// World matrices (16 floats each, row vectors, translation in 12..14, native axes) of the local
// player's bike model nodes in model order (18 for MX_MarsF_Lite: OBJ_Frame, OBJ_UPR_Forks,
// OBJ_Throttle, OBJ_LWR_Forks, OBJ_FRT_Tire, OBJ_FRT_Fender, ..., OBJ_Swingarm, OBJ_BCK_Tire, ...; an ATV's are the 50 of its
// chassis package's ATV_Hierarchy: OBJ_Frame, OBJ_Swingarm, ..., SWP_FRT_LFT_Tire, ..., NDE_BCK_LFT_FenderVib).
// Returns the node count written, 0 when there is no vehicle, <0 on failure.
// The game's render camera for the last frame (native axes; fov vertical, radians).
typedef struct ReflexCamera { float position[3], right[3], up[3], forward[3], fov, near_plane, far_plane; } ReflexCamera;
REFLEX_API int reflex_world_camera(void* world, ReflexCamera* out);
// Tyre-track decal triangles as the game draws them into RT_Bump (_re/notes/rt-bump.md): per
// vertex 6 floats {world x, world z, side 0/1, V along the ribbon, intensity, texture type}
// (type 0 = generic/slip texture, 1 = MX, 2 = ATV ...). *version changes whenever the game
// flushed new ribbon vertices; out may be NULL to only read the version. Returns triangles.
REFLEX_API int reflex_world_tracks(void* world, float* out, uint32_t max_triangles, uint32_t* version);
// Locates a game object by vtable and one identifying dword (see plugin_api.cpp); 0 if absent.
REFLEX_API uint32_t reflex_world_find_object(void* world, uint32_t vtable, uint32_t offset, uint32_t value, uint32_t lo, uint32_t hi);
// Engine sound (opt-in, _re/notes/engine-audio.md): the game's FMOD event commands for an audio
// client. sub: 0 play (path = event, serial new, position if has_position), 1/12 stop,
// 2 volume (value), 4 set parameter (param, value), 5 3D position, 7 mute / 8 pause (value 0/1), 11 key off.
// state: the wrapper's load state (+0x138: 5 loaded, 6 playing, 7 finished) — 6 on a parameter/position
// command for an event the client never saw start means it was already playing when the world was captured.
// The client plays the event's original waves and, when a one-shot ends, reports
// reflex_world_audio_finished(wrapper, serial) so the game's engine-sound state machine advances.
typedef struct ReflexAudioCommand { int32_t sub; uint32_t wrapper, serial; float value; float position[3]; int32_t has_position, state; char path[96]; char param[32]; } ReflexAudioCommand;
REFLEX_API int reflex_world_audio_enable(void* world, int enabled);
REFLEX_API int reflex_world_audio(void* world, ReflexAudioCommand* out, uint32_t max);
REFLEX_API int reflex_world_audio_finished(void* world, uint32_t wrapper, uint32_t serial);
// The race line ahead of the local player (node centres, 3 floats each, from the current node on).
REFLEX_API int reflex_world_race_line(void* world, float* out, uint32_t max_points);
// The original HUD's other values (see plugin_api.cpp).
typedef struct ReflexHudState {
    int32_t valid, activity_state, on_track, wrong_way;
    float speed, time_behind, time_ahead, hold_clock, hold_until, penalty;
} ReflexHudState;
REFLEX_API int reflex_world_hud(void* world, ReflexHudState* out);

// Every rider of the race (player slots P = G+0x7460 + slot*0x12a0 with a vehicle): local human players and AI.
// rider_row / vehicle_row: the slot's "Rider List" RiderId (P+0x1b8) / "Vehicle List" VehicleId (P+0x1b0) (00767200, _re/notes/vehicle-select.md).
typedef struct ReflexSlot { int32_t slot, type, local, rider_row, vehicle_row; uint32_t vehicle, rider; } ReflexSlot;   // type: P+0x10 (0 human, 1 computer)
REFLEX_API int reflex_world_slots(void* world, ReflexSlot* out, uint32_t max);
REFLEX_API int reflex_world_bike_pose_slot(void* world, int32_t slot, float* out, uint32_t max_nodes);
REFLEX_API int reflex_world_rider_pose_slot(void* world, int32_t slot, float* out, uint32_t max_bones);
// The game's run-time tables (the global table registry 0xae0910 and the 4 controller profile databases):
// a table by name, its row count, and cells (int columns: type 1; string columns: type 4, MSVC std::string).
REFLEX_API uint32_t reflex_world_table(void* world, const char* name);
REFLEX_API int reflex_world_table_rows(void* world, uint32_t table);
REFLEX_API int reflex_world_table_int(void* world, uint32_t table, const char* column, int32_t row, int32_t* out);
REFLEX_API int reflex_world_table_string(void* world, uint32_t table, const char* column, int32_t row, char* out, int32_t size);
REFLEX_API int reflex_world_bike_pose(void* world, float* out, uint32_t max_nodes);
// A race slot's vehicle (atv.cpp, _re/notes/atv.md): its class and every body of its simulation. ReflexBikeState above holds the first
// four bodies only; for an ATV those are the frame, the rider and the two front wheels. slot -1: the local player. 0 ok, 1 no vehicle.
typedef struct ReflexVehicleInfo {
    int32_t slot, type;            // type: the factory index (vehicles.h): 0 motocross bike (RavenVehicleMotocross 0x93cd84), 1 ATV (RavenVehicleATV 0x93d0ec),
                                   // 3 truck (RavenVehicleTrophyTruck 0x93cee4), 5 buggy (RavenVehicleBuggy 0x93d2e4), 7 UTV (RavenVehicleUTV 0x93d4d4), -1 other
    int32_t bodies, model_nodes;   // simulation bodies (S+0xec: MX 4, ATV 6); the vehicle model's nodes ([V+0x1328]: MX_Hierarchy 18, ATV_Hierarchy 50)
    uint32_t vehicle, sim;         // V = [P+0x1ac], S = [V+0x234] (RavenSimulationMotocross 0x93c8a4 / RavenSimulationATV 0x93d054)
    int32_t gear;                  // as the HUD shows it (RavenVehicle*::vf42)
    float rpm, speed;              // vf47 (MX S+0x8abc, ATV S+0x96ec, * 9.55); V+0x150 (ft/s)
    float render_position[6][4], render_orientation[6][4];   // B+0x90 / B+0x6d0 of bodies 0..5 (ATV: frame, rider, front left, front right, rear left, rear right wheel;
                                                             // truck / buggy / UTV: 5 bodies, the chassis, front left, front right, rear left, rear right wheel)
} ReflexVehicleInfo;
REFLEX_API int reflex_world_vehicle(void* world, int32_t slot, ReflexVehicleInfo* out);
// A race slot's mud build-up (mud.cpp, _re/notes/mud.md): the vehicle's mud record (table 0xa96d90, 12 x 0x40), whose three float4s the
// game binds as the pixel-shader constants usr_mud_amount / usr_mud_wetness / usr_mud_color of every material of the vehicle model and of
// its rider (the shaders blend lerp(albedo, usr_mud_color * mud noise, saturate(SFX mask + usr_mud_amount.w - 1))). Updated by the game
// every 0.1 s from the surface under the vehicle (MudManager rates per terrain type, cleaning types negative); reset on a restart.
// slot -1: the local player. 0 ok, 1 no vehicle or no record.
typedef struct ReflexMudState {
    int32_t slot, record;          // record: index in the table, -1 none
    uint32_t vehicle, rider;       // V = [P+0x1ac]; its rider [V+0x1a0] (= [P+0x1b4]), bound to the same record
    float amount[4];               // usr_mud_amount (+0x00, four equal lanes, 0..1)
    float wetness[4];              // usr_mud_wetness (+0x10)
    float color[4];                // usr_mud_color (+0x20)
} ReflexMudState;
REFLEX_API int reflex_world_mud(void* world, int32_t slot, ReflexMudState* out);
// Address of the vehicle simulation object (RavenSimulationMotocross or RavenSimulationATV) in the world.
REFLEX_API uint32_t reflex_world_sim(void* world);
// Read original-world memory (addresses as in the original process).
REFLEX_API int reflex_world_read(void* world, uint32_t address, void* out, uint32_t size);
REFLEX_API void reflex_world_destroy(void* world);
// Diagnostics (native/game verification, reflex_client): FNV-1a 64 over every resident guest page in address order, the world runtime's
// thread stacks left out (as the CLI's REFLEX_MEMHASH). Between frames. Returns 0.
REFLEX_API int reflex_world_memory_hash(void* world, uint64_t* hash, uint32_t* pages);

// MOTOCAREER (career.h, _re/notes/career.md). A profile session: the unchanged game booted to its front end with the profile of
// controller 0 signed in as on PC (offline Steam user, Steam Cloud storage <save_dir>/remote/savegame.bxml loaded by the game's own
// saved-game manager). Blocking (~3.5 s); NULL on failure (reflex_profile_error(NULL)). Commands (career.cpp career::command): "json"
// (the profile as JSON: sign-in state, every Player* table of controller 0, the game's IsLocked / IsHidden / IsStoreProductNew /
// IsStoreProductDLC per store product, event statuses, career completion), "event_status <name> <v>", "set_int <table> <row> <column> <v>",
// "purchase_asset <id>", "store_not_new <id>", "ui_setting <name> <v>", "save_ui_settings", "rider_string <column> <text>",
// "validate_jersey_name <text>", "profile_name_valid <text>", "favorite_vehicle <playerVehicleId>", "lua <source>" (run in the game's
// UI Lua state), "save" (App.RequestSaveGame: the game writes savegame.bxml). The result text goes to out
// (NUL-terminated); returns its length, -length-1 when size is too small (nothing written but the length), -1 on error.
REFLEX_API void* reflex_profile_open(const char* game_dir, const char* crt_dir, const char* save_dir);
REFLEX_API int reflex_profile_command(void* profile, const char* command, char* out, int32_t size);
REFLEX_API const char* reflex_profile_error(void* profile);
REFLEX_API void reflex_profile_close(void* profile);
// After a career race (race state Post), with the world's frames stopped: the game's own completion of the series event and its save
// (career::finish_event: CompleteSeriesEvent, CompleteSeries, the saved-game worker's write into <remote_dir>/savegame.bxml). JSON to
// out as reflex_profile_command; -1 on error (reflex_world_error).
REFLEX_API int reflex_world_career_finish(void* world, const char* remote_dir, char* out, int32_t size);
// The in-race career pages' calls on a career world, between frames (career::world_command): "status", "rules_done <dontShowAgain>",
// "results", "moto_results", "outcome <flags>". Result as reflex_profile_command; -1 on error (reflex_world_error).
REFLEX_API int reflex_world_career(void* world, const char* command, char* out, int32_t size);

// BINK (bink.h, _re/notes/bink.md): the install's Videos/*.bik decoded by the install's own binkw32.dll, run unchanged in the
// interpreter / JIT on a Machine of its own per player, driven as the game's XenonBinkVideo does (BinkOpen with
// BINKNOFRAMEBUFFERS | BINKALPHA [| BINKPRELOADALL], frame buffers of our own, BinkDoFrame / BinkShouldSkip / BinkNextFrame /
// BinkWait paced by the caller's clock). The picture is the four A8 planes the game's textures receive (Y and A ywidth x yheight,
// cR / cB cwidth x cheight; the game's ps_2_0 YUV shader turns them into RGB(A)); the sound is the PCM the decoder hands to the
// sound system (interleaved 16-bit). A player is used from one thread at a time.
typedef struct ReflexBinkInfo {
    uint32_t width, height, frames, fps_num, fps_den;   // frame rate fps_num / fps_den
    uint32_t has_alpha;                                // the A plane is decoded
    uint32_t audio_rate, audio_channels, audio_bits;    // 0 rate: no sound track
    uint32_t ywidth, yheight, cwidth, cheight;          // plane sizes (BINKFRAMEBUFFERS)
} ReflexBinkInfo;
// dll: <install>/binkw32.dll; file: the .bik; preload: BINKPRELOADALL (UIVideoComponent LoadToMemory). NULL on failure
// (reflex_bink_error(NULL)).
REFLEX_API void* reflex_bink_open(const char* dll, const char* file, int32_t preload, ReflexBinkInfo* info);
// The game's per-frame step (XenonBinkVideo::vf8) at `ms` (the caller's clock since the start of play): returns 1 when a new frame
// was decoded (its number, 1-based, to *frame), 0 when none, -1 on error; *done = 1 once the last frame is shown.
REFLEX_API int reflex_bink_update(void* player, double ms, int32_t* frame, int32_t* done);
// Copies the current planes (tightly packed; a NULL pointer skips that plane). 0 on success.
REFLEX_API int reflex_bink_planes(void* player, uint8_t* y, uint8_t* cr, uint8_t* cb, uint8_t* a);
// Takes up to max interleaved samples of the PCM decoded so far; returns how many (max 0: how many are queued).
REFLEX_API int32_t reflex_bink_audio(void* player, int16_t* out, int32_t max);
// 1 BinkGoto(1) (the game's loop restart, XenonBinkVideo::vf4), 2 BinkPause(value), 3 BinkSetSoundOnOff(value),
// 4 BinkSetVolume(track 0, value: round(volume * 32768)), 5 play (XenonBinkVideo::vf1: BinkGoto(1), sound on, the next update
// decodes at once). 0 on success.
REFLEX_API int reflex_bink_control(void* player, int32_t what, int32_t value);
REFLEX_API const char* reflex_bink_error(void* player);
REFLEX_API void reflex_bink_close(void* player);

// Game modes (modes.cpp, _re/notes/modes.md): the live in-game UI and the activity.
// reflex_world_ui_activate puts the game's UI where the shipping flow has it once a level runs (InGamePageGraph "Fullscreen HUB" with the
// focus) and posts the hub's route for the running activity ("Goto<Activity>SinglePlayer", or `route`); from then on the activity's own
// pages run (HUD, RulesPopup, EventIntro, Pre / PostChallengePopup, the post-event pages) and take the keyboard (Enter: accept). Writes the
// event posted to out. Returns its length, -1 on failure. Between frames (on the thread that runs the frames).
REFLEX_API int reflex_world_ui_activate(void* world, const char* route, char* out, int32_t size);
REFLEX_API int reflex_world_ui_event(void* world, const char* name);   // App.SendUIEvent (00740ed0). 0 ok
// The UI events the pages leave the level with (GotoFrontEndMainMenuPage, GotoQuittingPage, GotoCareerRaceAndRidePage, ...), held back
// since the last call (their pages would unload the world through the database loader): one per line in out. Returns the length.
REFLEX_API int reflex_world_ui_requests(void* world, char* out, int32_t size);
// What the game's UI shows: every visible page graph's current page (container order = draw order; a UIPageStack's visible pages bottom
// first), the page's layers in order, and each layer's component tree depth first (the layer itself first, parent -1). Nodes as the
// MXUI uicmpnt layer of that name builds them; texts UTF-8 (NUL-terminated at node.text in text); the composite animations started
// (state 0) or stopped (3, after running to the end) since the previous call, in order. counts: layers, nodes, anims, text bytes
// (each capped by its buffer). Returns 0, <0 on error.
typedef struct ReflexUiNode {
    int32_t parent, layer;
    uint32_t object, vtable, state;   // state: UIComponent +0xb4 bits 0 Focused, 1 Highlighted, 2 Focusable, 3 Enabled, 4 Visible
    float position[2], scale[2], size[2];   // +0x40 Position (pivot point in the parent), +0x60 scale, +0x88 Size
    uint32_t color;                   // +0xd8 the colour state's current colour (A8R8G8B8)
    int32_t text;                     // byte offset into text, -1: not a text component
    char name[48];
} ReflexUiNode;
typedef struct ReflexUiLayer { int32_t first_node, nodes; uint32_t object, state; char graph[32], page[48], name[48]; } ReflexUiLayer;
typedef struct ReflexUiAnim { int32_t node, state; float time, length; char name[48]; } ReflexUiAnim;
REFLEX_API int reflex_world_ui_frame(void* world, ReflexUiLayer* layers, uint32_t max_layers, ReflexUiNode* nodes, uint32_t max_nodes,
                                     ReflexUiAnim* anims, uint32_t max_anims, char* text, uint32_t text_size, int32_t* counts);
// The local player's activity: kind = index in modes.cpp's class table (name: the class without "MXActivity": "Freestyle",
// "HillClimbChallenge", "FreeRide" ...), state A+0x21c (0 Pre .. 4 Post), sub_state A+0x238, clock A+0x294, countdown A+0x29c,
// rules_hold A+0xff068 (the rules page is pending), the activation area the player is in. 0 ok, 1 no activity.
typedef struct ReflexModeState {
    int32_t valid, kind, state, sub_state;
    uint32_t activity, vtable;
    float clock, countdown;
    int32_t rules_hold, area_active;   // area_active: the player is in an activation area (MXPlayer +0xa0)
    int32_t area_track, reserved;      // its TrackId (+0x1a8)
    char area[32];                     // its activity (+0xa4: "HillClimb" ...; "FreeRide" outside one)
    char name[32];
} ReflexModeState;
REFLEX_API int reflex_world_mode(void* world, ReflexModeState* out);
// The challenges' collectables (modes.h ModeObject): kind 0 MXChallengeCollectable, 1 a Target Practice target, 2 a Hit Discovery spot;
// the scene entity id, drawn (model instance +0xa2), hit (targets +0x140), "Axis To Align", the scrolling texture offset, world matrix
// (16 floats, row vectors). Returns the count written (<= max), <0 on error.
typedef struct ReflexModeObject {
    uint32_t object, vtable;
    int32_t kind, entity_id, drawn, hit, axis, reserved;
    float uv_offset[2];
    float matrix[16];
} ReflexModeObject;
REFLEX_API int reflex_world_mode_objects(void* world, ReflexModeObject* out, uint32_t max);

// INPUT AND SETTINGS (input.cpp, _re/notes/input-settings.md).
// The mouse as the PC game reads it: one Raw Input WM_INPUT through the game's window procedure (00880dc0 -> GetRawInputData ->
// 00817cb0, accumulated into the input manager's mouse; XenonInputManager::vf43 turns it into the RiderReflexMouseX / Y axes:
// clamp(dx / 1280 * (1/30) / dt * sensitivity, -1, 1), Y negated, sensitivity = UI setting MouseSensitivity * 0.05 in [0.1, 5]).
// dx / dy: relative mouse counts (Raw Input lLastX / lLastY, +y down); wheel: WHEEL_DELTA units (0: none); button_flags:
// RI_MOUSE_* usButtonFlags (1 / 2 left down / up, 4 / 8 right, 0x10 / 0x20 middle, 0x40.. buttons 4 / 5). Call between frames,
// on the thread that runs them (OriginalSimulation.BetweenFrames). 0 ok.
REFLEX_API int reflex_world_mouse(void* world, int32_t dx, int32_t dy, int32_t wheel, uint32_t button_flags);
// The rumble the game asked for: the last XInputSetState per pad (motors[2i] wLeftMotorSpeed, motors[2i+1] wRightMotorSpeed,
// 0..65535) and the number of calls per pad (may be NULL). Any thread. 0 ok.
REFLEX_API int reflex_world_rumble(void* world, uint16_t* motors, uint32_t* calls);
// The game's UI setting `name` (DrivingCamera, Brightness, Gamma, OnlinePlayerName, MeasurementSystem, ControllerPreset,
// EffectsVolume, MusicVolume, EngineVolume, CrowdVolume, ControllerMotion, ControllerHandedness, CareerDifficulty, HUDVisibility,
// MouseSensitivity, Vibration) of `device` (-1: the active one): set 1 runs App.SetUISetting (UISettings_Set 005b9fa0, which
// applies it: DrivingCamera switches the player's camera preset in a race (activity state 1..3; on the gate from the next race, as
// the game), MouseSensitivity, Vibration, volumes, gamma ...); set 2 as the settings the world was made with (a race on the gate also
// gets the DrivingCamera's camera, as its player's creation 0054a7b0 assigns it); set 0 only reads. *out = the value before
// (UISettings_Get 005b9f50). Between frames, on the frames' thread. 0 ok.
REFLEX_API int reflex_world_ui_setting(void* world, const char* name, int32_t value, int32_t device, int32_t set, int32_t* out);

// PERSISTENCE (persist.cpp, persist.h, _re/notes/persistence.md). The profile in a race world, through the game's own code, between
// frames or with them stopped. save_dir = the profile session's folder (<save>/remote/savegame.bxml, <save>/stats). Commands:
// "attach" (an arcade world, booted without the save: the Steam stand-ins (remote storage, ISteamUserStats over
// <save>/stats/UserGameStats_55140.txt, the user's stats received), the profile signed in with storage, savegame.bxml loaded into the
// game (ReadSaveGame + ApplyLoaded) and the race's PlayerRecords working row joined again; before the first frame),
// "attach_steam" (a career world: the Steam stand-ins only), "leave" (the race left: RequestSaveGame + write, the activity teardown's
// records commit, the next save + write, StoreStats), "restart" (Activity::vf18's records reset, the commit, a save), "records"
// (PlayerRecords / PlayerHighlight as JSON), "achievements" (the 48 highlights and Steam achievements). JSON as
// reflex_profile_command; -1 on error (reflex_world_error). The profile session (reflex_profile_command) answers "achievements" too.
REFLEX_API int reflex_world_persist(void* world, const char* save_dir, const char* command, char* out, int32_t size);

// PHYSICS PROPS and ANIMATED TEXTURES (props.cpp, props.h, _re/names/87_props.tsv, _re/notes/props.md). Read only.
// The PhysicsEntity family (scene PhysicsRSData: bales, tyre stacks, cones, barrels, fences ... that riders knock; PopUpBaleRSData,
// PopDownBaleRSData; step-up bars; parts split off vehicles) found once per world by vtable scan. reflex_world_props writes the
// props whose entity world matrix (+0x30, row vectors) or drawn flag (ModelInstance +0xa2) changed since the last call (every prop
// with all != 0), at most max; the rest stay pending for the next call. Returns the count written, <0 on error. After a frame, on
// the frames' thread. reflex_world_props_rescan: find the objects again (parts created later), every prop reported once more.
#define REFLEX_PROP_PHYSICS        0   // PhysicsEntity (vtable 0x93743c)
#define REFLEX_PROP_POP_UP_BALE    1   // PopUpBale (0x95ffcc): hidden 5 m under its place until its trigger fires, then rises
#define REFLEX_PROP_POP_DOWN_BALE  2   // PopDownBale (0x95ff04)
#define REFLEX_PROP_STEP_UP_BAR    3   // MXStepUpBarEntity (0x939034)
#define REFLEX_PROP_VEHICLE_PART   4   // VehicleDamagePhysicsEntity (0x93ccac): a part split off a vehicle model at run time
#define REFLEX_PROP_FLAG_VISIBLE   1   // drawn (ModelInstance +0xa2)
#define REFLEX_PROP_FLAG_MOVED     2   // the matrix differs from the first one reported (the scene placement)
#define REFLEX_PROP_FLAG_NEW       4   // first report of this object
typedef struct ReflexProp {
    uint32_t object, entity_id;        // guest object, scene entity id (+0x14: the scene's Id, ReflexLevelContent instances' entityId)
    int32_t kind;                      // REFLEX_PROP_*
    uint32_t flags;                    // REFLEX_PROP_FLAG_*
    float matrix[16];                  // entity world matrix (+0x30), row vectors, native axes = Unity axes
} ReflexProp;
typedef struct ReflexPropInfo {
    int32_t props, reported, objects, pending;   // live props, entries written, objects found by the scan, changes left for the next call
} ReflexPropInfo;
REFLEX_API int reflex_world_props(void* world, ReflexProp* out, uint32_t max, int32_t all, ReflexPropInfo* info);
REFLEX_API int reflex_world_props_rescan(void* world);
// The animated textures: ScrollingTextureEntity (kind 0, ScrollingTextureRSData: offset = the material parameter gTextureOffset
// (vertex c85, added to uv0 by the scrolling templates) the game computed this frame; param = signed U / V periods (seconds for
// wrap, negative = reversed), U / V accumulators) and TextureAnimEntity (kind 1, TextureAnimControllerRSData: target = TextureName;
// param = the frame drawn, the texture's frame count, frames per second, mode 1 cycles / 2 ping-pong). Returns the count written.
typedef struct ReflexTextureAnim {
    uint32_t object, entity_id;
    int32_t kind;
    uint32_t flags;                    // 1 drawn (scrolling: ModelInstance +0xa2)
    float offset[2];
    float param[4];
    char target[64];
} ReflexTextureAnim;
REFLEX_API int reflex_world_texture_anims(void* world, ReflexTextureAnim* out, uint32_t max);

// WP-3 (plugin_api.cpp, _re/notes/hud.md): the UI sounds the game's UI played / stopped since the last call (g_UISoundManager: UISound_Play
// 005e37e0, UISound_PlayIfIdle 005e3840, UISound_Stop 005e38c0; UIRegistry.xml names), one per line: "play <name>", "loop <name>",
// "stop <name>". The first call installs the hooks. Between frames. Returns the length written, <0 on error.
REFLEX_API int reflex_world_ui_sounds(void* world, char* out, int32_t size);

// WP-R2 (plugin_api.cpp, _re/notes/water.md): the wake splats the game queued since the last call. Vehicles in water add quads to the
// wake object (DAT_00a95790) through FUN_007ecd80 / 007ec280; the original's WakeSimRenderEvent (FUN_007ed300) draws them into the
// 512 x 512 wake target and clears them once per rendered frame, which the runtime does not run, so this call takes them and clears
// the counts exactly as 007ed300 does (+0x200 centres, +0x1a50 floats, +0x390 primitives; render-only state, the bike state hashes
// are unchanged). vertices: max_vertices x 5 floats (x, y offsets in clip units of the target around the splat's centre, strength, u,
// v; triangle list); centres: max_centres x 2 floats (world x, z, one per 6 vertices). counts[0] = primitives, counts[1] = centres.
// Between frames. Returns the vertex count, <0 on error.
REFLEX_API int reflex_world_water_splats(void* world, float* vertices, uint32_t max_vertices, float* centres, uint32_t max_centres, uint32_t* counts);

// WP-5 in-race pages (pages.cpp, _re/notes/in-race-pages.md). Between frames.
// The localized UI voice the page Lua requested since the last call (App.PlayUILocalizedSoundEvent -> UILocSound_Play 0068dd40,
// StopUILocalizedSoundEvent -> UILocSound_Stop 0068de20; the world never services them): "play <database>|<package>|<event path>|<owner>"
// and "stop <owner>" lines. The first call installs the hooks. Returns the length written, <0 on error.
REFLEX_API int reflex_world_ui_voice(void* world, char* out, int32_t size);
// "state" {race, rulesHold, allowPause, post, skip, vtable}; "skip_scenario" (App.SkipToNextScenario); "lua_ui <key> <source>" (the source
// in the game's UI Lua state, the UI variable <key> as the answer); "ui <key>" / "ui_int <key>"; "gates" (the series' gate selection:
// ProcessGateSelection and the 16 gates' availability). Result as reflex_world_career; -1 on error (reflex_world_error).
REFLEX_API int reflex_world_pages(void* world, const char* command, char* out, int32_t size);

// WP-G garage (garage.h, _re/notes/garage.md), between frames: "lua <source>" (the game's UI Lua state; GarageVehicleTuning:End's
// PlayerVehicleTuning writes), "apply_tuning <device>" (App.ApplyVehicleTuning: the tuning onto the running vehicle and simulation),
// "tuning <slot>" (JSON: the slot's vehicle spec values, the model's, the vehicle / simulation tuning terms). Result as reflex_profile_command;
// -1 on error (reflex_world_error).
REFLEX_API int reflex_world_garage(void* world, const char* command, char* out, int32_t size);

// WP-T MotoSkills tutorials (tutorial.h, _re/notes/tutorials.md). A tutorial world is built with ReflexEventSpec.type "Tutorial" (location
// "UI_Compound", activity 8; no other field read): the front end's Main Menu Page and MainMenu:OnTutorialKeypress in the game's Lua, the
// image written on the Freeride page of the UI world. Commands between frames: "attach" (once, before the client shows anything: the live
// UI hooks without navigation, the localized voice incl. StopTutorialAudio, the COM stubs, Bink as the boot has it, the vehicle part of
// OnTutorialKeypress on the loaded tables), "frame" (each frame: the saved-game worker's pass for a requested save; "Saved"), "state"
// (JSON: TutorialMode, the TutorialManager's lesson / state / step, the PreTutorialPopup video's path and state), "progress"
// ("<TutorialsCompleted> <TutorialsEntered>"), "objects" (the lessons' TUT0n scene objects: "<entity id> <shown> <name>" lines),
// "test_pass" (tests: the running lesson's pass). Result as reflex_world_pages.
REFLEX_API int reflex_world_tutorial(void* world, const char* command, char* out, int32_t size);

// CONTROLLER REMAPPING (remap.cpp, remap.h, _re/notes/input-settings.md "Controller remapping"). The ControllerRemapping table (row 0
// the keyboard, row 1 the pad; the game's input ids) of the running game, between frames: "table" -> JSON {columns, kb, js};
// "apply" -> the local human players' KeyMaps built again from the table as their creation does (00547f30), for a world whose save
// was attached after its boot (reflex_world_persist "attach"). JSON as reflex_profile_command; -1 on error (reflex_world_error).
// The profile session (reflex_profile_command) answers "remap" (the table with the page's names, the DIK -> key id table, the key
// and pad names), "remap mapped <id>" and "remap glyph <MapKey>".
REFLEX_API int reflex_world_remap(void* world, const char* command, char* out, int32_t size);

// API VERSION, STRUCT SIZES, CRASH REPORTS, PERF (platform; additive exports: plugin_api.cpp, crash.inc). The Unity side
// (ReflexWorldNative.Check) compares reflex_world_version() with its own expected version and reflex_world_sizeof() with the
// managed layout of every struct it marshals, once at load, and refuses a plugin whose layouts differ.
//
// REFLEX_WORLD_API_VERSION changelog (bump on any change of an existing export's signature or of a struct layout; adding an
// export or a sizeof name needs no bump: callers look new exports up and treat a missing one as "older plugin"):
//   1  2026-10-18  first numbered version: every export above, plus reflex_world_version / reflex_world_sizeof /
//                  reflex_world_crash_install / _set_context / _note / _status / _test and reflex_world_perf_stats.
#define REFLEX_WORLD_API_VERSION 1
REFLEX_API int reflex_world_version(void);
// sizeof of the named struct of this header ("ReflexPad", "ReflexBikeState", ... every typedef above), -1 for an unknown name.
REFLEX_API int reflex_world_sizeof(const char* name);
// Every struct name reflex_world_sizeof knows, space separated (as reflex_world_pages: returns the length, copies when it fits).
REFLEX_API int reflex_world_struct_names(char* out, int32_t size);

// Crash capture (crash.inc, after Open Unleashed's crash.c). install: process-wide signal handlers (POSIX: SIGSEGV SIGBUS SIGILL
// SIGFPE SIGABRT SIGTRAP; Windows: vectored + unhandled-exception filter + SIGABRT) that write <dir>/crash-<session>.txt for a
// fault whose stack passes through the plugin (and for any SIGABRT), then chain to the previous handler (Mono / Unity). 0 ok, -1
// (see reflex_world_crash_status). set_context: a key / value shown in the report (an empty value removes the key; 16 keys).
// note: a breadcrumb in the report's ring of the last 48 notes. status: "installed=1 handlers=6 id=<LC_UUID> base=0x... session=...".
// test (debug only, the player's -reflexCrashTest switch): 1 a bad write, 2 abort(), 3 SIGFPE; never returns for those.
REFLEX_API int reflex_world_crash_install(const char* dir, const char* session, const char* build);
REFLEX_API void reflex_world_crash_set_context(const char* key, const char* value);
REFLEX_API void reflex_world_crash_note(const char* text);
REFLEX_API int reflex_world_crash_status(char* out, int32_t size);
REFLEX_API int reflex_world_crash_test(int32_t kind);
// One line of "key=value" process diagnostics for the session log's perf samples (ReflexPerfLog): heap=<MB> (malloc zones in
// use) rss=<MB> footprint=<MB> (phys_footprint, what Activity Monitor shows) threads=<n>. Cheap; returns the length.
REFLEX_API int reflex_world_perf_stats(char* out, int32_t size);

#ifdef __cplusplus
}
#endif
