# fx.md: effects (particles, mud, terrain deformation, tyre tracks) and the stunt / entity leaves of plan rx_fx

This covers core/game/fx_*.c (header fx.h, helpers fx_util.h; oracle adapters core/oracle/fx_*.cpp, port group "rx_fx", plan
core/plan/rx_fx.tsv). Verify with the group `rx_fx` (layered: each round drops the ports that ran, so nested ports are checked under
the original callers too). Build the oracle CLI with all core files (`native/game/tools/build_cli.sh <dir>`; the earlier
`RX_CORE_FILES` subsets no longer link: other units' files reference each other).

## Files
- fx_particle.c: PlatformParticleManager2D (0x00c11bd0, additive 0x00c11d80): Particle_UpdateJob 006925f0 (one job per manager, run
  on the job pool), ParticleManager2D_Update 00698510, SpawnParticle 00698780, Particle_Copy 00692b50, the manager / emitter
  virtuals, UpdateManagers 006960c0, Tick 00696100. The product register file (g_fx_regs) lives here.
- fx_emit.c: the emitters (006941a0 matrix lerp, 006944c0 / 00694ba0 spawn builds with the game's LCG 0x9cb860, 00695b00
  in-range, 00695650 gate, 00695780 emit, 00695ab0 vf0, 0069b060 vehicle emitter vf3).
- fx_vehicle.c: vehicle effects (00697460 gate, 00693560 rate, 00696d80 effect update, 006979a0 / 00697a00 the wheel-material job,
  00691bd0 update all).
- fx_effect.c: PlaceableParticleEffect vf19 00696940, the timed / fireworks / triggered effects (00692340 / 00692520 / 006922c0 /
  006968e0).
- fx_terrain.c: tyre tracks (TerrainTracks_FlushPending 007cbe30, reset 007cc630), DeformablePatch readback 007cb930, vf14 007c4fa0,
  the terrain registry 00402d90, MudRecord_Update 00408c50.
- fx_deform.c (product only): the CPU model of ps_hft_deform (native/drafts/world_runtime/deform.cpp): the SoftnessMask (P+0x28) and
  the brush (P+0x4264) decoded from their DXT1 / DXT5 bits, the stamps of queue[!q] applied rt[idx] -> rt[!idx]. The oracle renders
  natively (the world runtime's TRACKS_REBUILD), so vf14 calls FlushPending through its address there.
- fx_entity.c: trigger volumes, MXTrigger / TimedRaceSection / ScrollingTextureEntity / overlay / gate camera, small leaves.
- fx_world.c: bales, flag holders, the land animals' vf29 / steering (006d07a0 / 006d2eb0), key maps, online content / storage /
  sessions (006d3d30, 006d3c40, 006d4a70, 006de1c0, 006de690, 006decd0).
- fx_animal.c: LandAnimalProximity::vf20 006d09f0 (heading history, inlined arcsine in double with float steps, sine / cosine
  polynomials 0x935d3c.., terrain normal, transform rebuild, nearest sound point, audio emitter).
- fx_stunt.c: the trick tracker (player +0xb58: 006c9770 and its passes) and the stunt scorer (player +0xc70: 006cab80, ratings
  006cc4b0, combo 006cae70 / 006cc040 / 006cb390, wreck 006cb720, the StuntBegins / StuntEnds / combo-lost handlers 006cbbe0 /
  006cbdb0 / 006cb6a0, the trick maps 006cd3f0 / 006cb960, the trick table size 006ccbc0 -> [0xa9431c]).
- fx_text.c: Matrix4_Mul 00401830, the text element setters 004029a0 / 00402970 / 004027e0 (UTF-8 -> UTF-16 by hand, as
  MultiByteToWideChar(CP_UTF8)), the per-thread table lookup 00402420.
- fx_rdb.c: register-exact Rdb helpers (00417da0, 00416e40, 00417df0, 00418e00, 00419ea0, 00419ae0, 004199a0, 00419a70,
  00415320, 00417590, 004021d0), ModelEntity::vf9 00401ea0, ConVar<bool>::vf2 004172e0. The tables are build_tables_rdb.c's.
- fx_tree.c: the std::map<string, int> red-black tree code of the trick maps: insert with a hint 006cd4a0 and its callees (node ctor 005bac00 with its SEH
  record, _Insert 005ba570, insert-unique 005ba210, rotations 00851f50 / 005bb590, iterator ++ / -- / == 00593dc0 / 0063bf50 / 004fd0c0, size 00608390).
- fx_vector.c: std::vector<T*> element helpers (0042bf30 alloc, 00776d20 umove, 007cfd80 fill) and the registry vector's _Insert_n 006c8260 (SEH record written).
- loop_jobpool.c (group rx_loop): the JobPool constructor 00881a70 (imports through rx_os / the oracle's emulator) and its element constructor 004dc090.
- fx_emit.c also has `rx_fx_vec_angle` 00672fe0 (the angle between two vectors: rational acos in double with float steps; the first vector's frame words are read
  as they lie when its length is 0). fx_effect.c also has the sound emitter 00678980 (calls the world unit's event layer and the race unit's rx_sound_stop).
  fx_stunt.c also has the scorer's refresh 006cc8b0 and its rating helpers 006cc830 / 006cc740 / 006cb9b0; `highlight_progress` is the product boundary of 006d7980.
- fx_types.c: static-instance getters (00402b10, 00413d20), the MSVC array iterators (004022d0 / 00402500), small scorer fills
  (006c8230, 006cb180, 006ccf60), the localized-package queue drain 0041b410.

## Conventions that matter for bit-exactness
- The callee-saved registers (fx_util.h fx_reg / fx_set_reg): frames write the caller's EBX / EBP / ESI / EDI where the original
  pushes them, and functions set the model where the original assigns a register before a call whose callee pushes it. Particle
  templates copy uninitialized frame words (the 00697a00 job's saved EDI / EBP land in template +0x68 / +0x6c), so this is what
  makes On mode identical. Oracle: the machine's registers (bound by FX_PORT); product: g_fx_regs. A direct call from another
  unit's code in the oracle (outside an FX_PORT) sees 0 for the four (dead-stack words only).
- Calls are made at the original's stack depth with their arguments and return addresses written (fx_push / fx_pop); CRT calls
  whose ports write frames (memcpy / memmove / memset, type_info::_name_internal_method's first name) go through FX_CRT /
  fx_type_name, which leave rx_sp() at the ESP before the pushes as crt_util.h expects.
- fx_jobpool_submit writes JobPool_Submit's residue after rx_jobpool_submit (loop unit).
- x87 at 53 bits (cw 0x027f on the worker); fld / fstp copies through fx_x87_bits (the emulator's NaN model); SSE arithmetic with
  x86's NaN rule (rx_adds ...). .data constants (>= 0x9a6000: 0x9cc8a8, 0x9d78a0, 0x9cd9e4, 0x9cd9e8, 0x9ddfc8, 0x9d789c, 0x9c4068,
  0x9d7b34, 0x9c3e1c..) are read from memory.
- type_info first names: the oracle runs the import for a type whose name is not cached yet (the CRT port's heap block differs:
  004172e0's Verify showed it); product: crt_type_info_name.

## Not ported (and why)
- The criterion-vector internals with SEH frames or further unported callees: 004175e0, 00417b60, 00417c20, 00418470,
  004187a0, 00418800, 00419c70, 00419cd0, 0041abd0, 0041ac70, 0041b090 (msvcp vector push_back / _Insert_n / uninitialized copy /
  destroy of the 0xb0-byte query criteria; callees 004eee60, 004f3320, 004dc0f0, the msvcp string imports) and
  MXHighlightTracking_AddProgress 006d7980 (Highlight / PlayerHighlight table queries on the Rdb query engine: a profile / Steam achievement boundary,
  core/plan/QUEUE.md "fxport"). The map insert with hint 006cd4a0 and its tree callees are ported now (fx_tree.c).
  build_tables_rdb.c re-implements the same semantics unregistered.
- 0054de80 (a local player leaves the race when the online session starts: network + UI table writes): not ported, not reachable offline.
- Callees left on the original (rx_call_original: fatal in the product, listed by the coverage report): audio commands in
  fx_effect.c, 006caaf0 / 006cc8b0 / 00588b50 (combo end / refresh / score drop), MXRecords_LongestStoppie 00764c20 and
  FailedJump 00764f80, 006c98c0 / 006c9b60 / 006c9cc0 / 006c9da0 (trick start / mode handlers), 00548810 / 006cd2c0 / 006c9aa0
  (gesture reading), 0041bd10 / 0041b4b0 (package queue), 004eedc0, 007f1aa0, the online-session callees of 006decd0 and
  006d3d30, MXSeriesManager_Construct 005b2370 and 00402be0 (static initialisers), _invalid_parameter_noinfo.
- The deformation pass is unverified in the oracle (the world runtime renders it natively there; the texture loader is not in the
  core yet).

## fxport additions (2026-10-20)
Reach, per-call counts and the not-reached list: core/plan/QUEUE.md "fxport". Verification stimulus for paths no world reaches: `REFLEX_POKE="frame:hexaddr:hexvalue,..."`
(boot_main.cpp: 32-bit memory writes at the start of a frame, applied to the reference and the port run alike). The baton boots (builds/fxport/boot_port_fx.sh:
`EV=1|alpine <cli> <filter> <outdir> 1`) reach construction-time code (the registry vector growth, the job pool constructor, the trick-map tree inserts). The
old oracle's "Fn_xxxxxxxx" lifted ports share addresses with the core ports: filter by port name (`rx_<Name>`), not by the hex address, when checking one port.

## Verification state (2026-10-19)
Layered Verify on the five worlds (Concord, Concord_ai11, FR_Alpine, SX_Springfield_ai11, TRX_Verde_ai11; 1500 frames): 0
mismatches for every port that ran. On mode (all rx_fx ports, 1800 frames): memory hash identical to the original on all five.
Never called in these runs: TerrainTracks_Reset, TrickEvent 006cd750 and a few zero-call leaves (listed by vall.sh).

## The particle asset and the scene FX entities (fxconstruct, 2026-10-20)

Until this round the scene factory left the FX classes empty (`build_scene.c`'s `outside_core`): the product's UI world scene had 38 tickers where the
oracle's has 66/68, and the first ticks of those entities (the game LCG at 0x9cb860 / count 0x9cb864) were missing, so the state-6 grid pass drew
the start zone from a different LCG state (gridfix; product slot 7 vs the oracle's 5).

Files (new): `game/build_particle.[ch]` (the **particle** AssetIO), `game/scene_fx_entities.[ch]` (the classes), `game/fx_worldparticles.c`
(the world's vehicle effect sets), `game/scene_viewports.c` (00757660's viewport list, which the FX gate and the emission divisor read),
`oracle/scene_fx_entities.cpp` (group `rx_fxconstruct`).

- **build_particle.c**: ParticleAssetIO::vf3 007e3a10, ParticleAsset ctor 007e1820 (the 12 x 11 curve LUT at 0xa817b0, built from the basis
  functions 007e3510 / 007e3580 / 007e35d0 / 007e3620 / 007e3680 / 007e3720 / 007e3740 / 007e37a0 / 007e3800 / 007e3810 / 007e3890 / 007e3940, x87 at the
  loader thread's 53 bits with the SSE polynomials in the original's order), ParticleAsset::Parse 007e1a40 (the emitter records 0x1f0 bytes, the
  frame table with the half-float rect builder 007e3460, the 16-word colour palette, the shadow / particle luminances) and the destructor 007e18f0.
  It is registered as the `particle` loader in `build_load.c`. The parse follows the original's XMLReader semantics (children by name, the
  NULL-name cursor = any sibling, the type-1 comma lists, the type-9 vec2 / 0xa vec3 / 0xb bool values).
- **scene_fx_entities.c**: 0x102a PlaceableParticleEffect (006964d0 with the emitter member 00694110; vf5 00696670, vf7 00696810 = bind to the
  scene's ParticleEntity's emitter asset, vf9 00696870), 0x102e FireworksParticleEffect (00692140; vf5 006921c0 = the base + EmitterType,
  vf7 00692240), 0x102f TriggeredPlaceableParticleEffect (006923c0; vf5 = the base's, vf7 00692450), 0x1030 ParticleEntity (00695c60; Load
  00695de0: the ParticleEmitter / Texture / ParticleAttrib asset handles, the limits, the two manager inits 00697ce0), 0x105e / 0x1088
  AudioVolumeMixEntity (0067d380; vf5 0067d4d0, vf19 0067d7f0 -> 0067d810) and 0x107e StatusBoardEntity (00413d80; vf5 00413ed0, vf19 00416530).
  The ticks are the fx unit's (00696940, 00692340 / 00692520, 00696100). **0x102b / 0x102c / 0x102d (ParticleSphere / ParticleYRing /
  BermParticle) are absent from all 57 level scenes** (`builds/fxconstruct/logs/level_fx_scan.txt`): their factory slots stay empty.
- **fx_worldparticles.c**: MXWorld::vf3 007665f0 builds the world's particle effect objects (00696480 PhysicsObjectSplashParticles 0x100,
  006963b0 PhysicsObjectDustParticles 0x110 x3, the inlined poof) once the scene has a ParticleEntity; the vehicle effect sets' rebind
  00697870 / reset 006977f0 / record bind 006976a0 / controller release 0069b090 and the Y-ring emitter vf0 0069ad60 with its rate draw.
- **scene_viewports.c**: 007d3290 SceneManager_AddViewport + 00794a10 / 00794b80 (the viewport record) as 00757660 creates the "Shared
  Viewport" (engine + 0x24) and one "ViewPort %d" per view (engine + 0x1c / + 0x20). The list is gameplay-relevant: 00695b00's distance gate
  and 00695780's `total / [scene + 0x19d0]` read it (the oracle's UI scene has one view for the human). The record's camera (+0), render
  targets (+0x60 .. +0x78) and audio listener (+0x80) are the render / audio units' (product boundaries, see below).

### Boundaries (oracle original, product nothing)
- 00697ce0 ParticleManager2D_Init's D3D arrays / vertex declarations / buffers / shaders: the product sets the simulation state (the capacity
  cap-2, the particle / dead arrays, the globals 0xa9427c / 0xa94280 / 0xa94284, the defaults +0x30 .. +0x50) and has no renderer. The oracle
  runs the original under the adapter (the Product's Load port calls it through `rx_call_original` in the oracle build).
- 00696760's sound emitter (006788f0, an FMOD event) when the effect's SoundAsset is not `Editor_Default`; 00413ed0's UI text billboard (the
  globals 0xa89bd0 / 0xa89bd8) and 00416530's UI page update: the StatusBoard vf5 / vf19 are **product-only** ports (the oracle keeps the
  originals); the product's entity fields / matrices are the original's up to the UI objects.
- 00794d10 (the renderer's camera sync per viewport) and the audio listener update (007d20b0's `audio_listener_update`): product no-ops.
- 0069a170 (the Y-ring spawn builder, 3047 bytes): not ported; 0069ad60 calls it only when a vehicle emitter is active (the product would stop
  naming 0069a170). Not reached in the UI world / the grid pass.
- The scene entities' vf5 ports (00696670 / 006921c0 / 00692440 / 00695de0 / 0067d4d0) read the scene RSData through the core's fmt reader
  (`g_rs`), which only the core's scene loader sets; they are registered **product-only** (the oracle runs the originals). The constructors,
  vf7 / vf9, the ticks and the world-particle functions are registered in both builds and adapted (`rx_fxconstruct`).

### Verification
- Product UI world, state-6 grid pass (`builds/fxconstruct/probe_build/probe`, RX_FULL_FRAME=1): **68 tickers**, the same classes as the
  oracle (4 StatusBoard 0x939de4, 19 ScrollingTexture 0x939ba4, 1 OverlayColor, 15 MXTrigger, 24 PlaceableParticleEffect 0x95e32c, 1
  ParticleEntity 0x95dfbc, 1 AudioVolumeMixEntity 0x95d3d4, 1 HFTerrain, 1 ResetMarkerFlag). The 24 placeables' bind resolves the
  ParticleAsset names (e.g. `Road_Flare` at index 15) in the loaded 33-record `UI_Compound_Particles` asset; the emitters tick and draw the
  game LCG.
- Baton-boot Verify (`builds/chain/boot_port.sh builds/fxconstruct/cli6/reflex_boot rx_fxconstruct,rx_fx`, log
  `builds/fxconstruct/logs/boot_v6.txt`): every new port that the boot reaches is called with **0 mismatches** (the constructors, vf7 / vf9,
  the world-particle functions, the Y-ring emitter, the bounds); the only mismatching port in the report is the documented `006d3d30`
  online-content dead data (two registry spellings). The first run (`boot_v5.txt`) had the two constructors' RET fixed: 00696480 / 006963b0
  return with a plain `ret`, not RET 4.
- Five reference worlds, `verify.sh builds/fxconstruct/cli6/reflex_boot rx_fx`: **114 / 108 / 110 / 112 / 110 ports called, 0 mismatches,
  On identical** (Concord / Concord_ai11 / FreeRide_FR_Alpine / SX_Springfield_ai11 / Terracross_TRX_Verde_ai11). `rx_world` (Concord,
  Concord_ai11): 170 / 168 ports, 0 mismatches, On identical. `rx_race quick` and `rx_loop quick` (Concord): only the documented pre-existing
  `0055f700` / `0084ed20` two-word mismatch and On identical (state hash 1f6611fa…, memory 1487a52f…, the cleanup2 baseline).
- The game LCG at the grid pass is **not yet the oracle's 2239/2240**: see product_chain.md "Round 6" for the measured difference (the
  number of pre-grid scene ticks / the renderer-set particle rate global [0x9c46b8]).

## ridefx (2026-10-20): the YRingParticleEmitter's spawn builder 0069a170

**Ported: `0069a170` -> `rx_yring_spawn` (fx_emit.c, group rx_fx).** The Y-ring spawn builder, 3,047 bytes, called only by
`0069ad60` YRingParticleEmitter::vf0 (fx_worldparticles.c, `rx_yring_emitter_vf0`); that call site now calls the port
(`rx_call_original(0x0069a170)` replaced). The adapter is in oracle/fx_particle.cpp (`YRingSpawn_0069a170`, no output registers:
the original leaves EAX = the holder, which its only caller ignores).

What the function does (read off the disassembly; every constant from .rdata): S = the emitter state; T = [S+0x18] * 0x1f0 + the
type list of the asset holder's data ([S+0x14] -> +0x44 -> +0x10). One LCG draw gives the lifetime (T +0xc4 / +0xc8); **below 1/60
nothing spawns and the holder's busy flag (+0x4c) is left set** (the caller clears it). Then, in this order: the colour (T +0x158,
one draw, three truncating x87 fistp channels -> `0xff<<24 | c0<<16 | c1<<8 | c2`), the atlas index (T +0x161, D +0x20, < 16), the
position (3 draws; x from the third, y from the second, z from the first; T +0x20 / +0x30), the ring start angle (one draw,
T +0xec / +0xf0): two wrapped angles (a0 + pi/2 and a0, each `pi - (|v| - trunc(|v|/2pi)*2pi)`, the Float_Abs 00401000 calls) through
the cosine polynomial at 0x935d3c.. give `(-cos a0, 0, cos(a0+pi/2))`, normalized when its squared length > 0 (SqrtF 0042c230);
the speed (one draw, T +0xfc / +0x100) and the Y velocity range (one draw, T +0xf4 / +0xf8) over the base velocity (T +0x40); the
acceleration (3 draws, x from the last, T +0x70 / +0x80 + the offset S +0xa0); through the matrix (velocity without translation,
then the source S +0x90 * T +0xe8; position with the translation; acceleration); then the player-range gate 00695b00 on the
position. In range: the sizes (T +0x98 / +0x9c), the rotation (one draw + the angle to the velocity when T +0xc0, 00672fe0), the
spin (one draw, T +0xb8 / +0xbc), the two scale draws (T +0x118 / +0x120 for x, T +0x11c / +0x124 for y, y mirrored to x when
T +0x128), the frame and frame2 draws (T +0xcc / +0xd0, T +0xdc / +0xe0), the start time (0065ca30 = timing +0x40), then the
manager's spawn vf1 (owner S +0x70, the 0x80-byte template at E +0x80) and the busy flag cleared. Frame: push ebp; and esp, -128;
sub esp, 0xf4; ebx / esi / edi -> E = A - 0x100; the template's unwritten words (E+0x8c / +0x9c / +0xac / +0xdc..+0xec / +0xf8 /
+0xfc) are the pre-existing emulated stack, identical in both Verify runs (the core's frame is game memory, not the host stack).
`spawn_colour` became `spawn_colour_at(cw_saved, cw, q, ...)` so the builder (cw at +0x54 / +0x50, fistp at +0x60) and the two
existing builders (both at +0xc / +0x10) share it.

**Reach: none of the five reference worlds and neither the full ride nor the UI world / grid pass** (measured: REFLEX_COVERAGE over
Concord / FreeRide / Terracross, 600 frames each, and the full crash drive -- Concord, 1800 frames, REFLEX_DRIVE=7, state hash
e87afe55...: 0069ad60 and 0069a170 0 executions; 0069b060 (the vehicle emitter vf3) 36,000 calls in the 1800-frame ride, but the
effect records' controller emitters are the generic ParticleEmitter vtable 0x0095df44, slot 0 = 00695ab0). The only live
Y-ring emitter is the world's `PhysicsObjectSplashParticles` (MXWorld + 0x21dc4, ctor 00696480): its bind is vtable slot 6 =
**00696470, an empty `ret 0x14`**, so its +0x14 holder / +0x18 index are never filled and 0069ad60 bails at `[S+0x14] == 0`.
`PlaceableYRingParticleEffect` (0x102c) is absent from all 57 level scenes (builds/fxconstruct/logs/level_fx_scan.txt). So the
builder is inertia in the shipped content of these events; it is still ported because the product would otherwise stop naming
0069a170 the moment a Y-ring emitter is ever bound (a water splash / a Y-ring level entity), and the task requires the ride path.

**Verification (the probe, oracle/ridefx_probe.cpp, group "thunk"):** groups `b` = 10 direct calls on a live vehicle effect
record's controller emitter state (S from `[rec+0x60]+0x10`, the real holder/index/T, the manager `[ctl+4]`, the matrix
`[ctl+0xf0]`); `a` = the caller path 0069ad60 at the same state (interpolated and not, 2 particles each). Cases: plain spawn,
colour + atlas 5, atlas index 16 (-1), align + identity matrix, align + mirrored scale + rotated matrix, NaN and +inf start
angle, out of range, no velocity source / no acceleration offset, lifetime below 1/60. Per-call Verify on Concord frame 30 and frame
1200 (racing): **direct 10 / 10 / 0 and caller path 4 / 4 / 0 mismatches at both frames** (nested 00695b00 / 00698780 / 00672fe0
run as originals in the original pass, so their effects on the spawned particle are inside the compared memory). With the caller's
own port installed (rx_fx + rx_fxconstruct + the probe) **0069ad60 is 2 / 2 / 0 at both frames** (the core caller -> the core
builder against the original), which also checks the call-site switch. **Product ride** (builds/ridefx/api, RX_FULL_FRAME=1, 1200
frames): state 2 racing, 12 riders, particle counts live 1833 / normal 431 (1647 / 412 at 120 frames, countdown): the particle
system runs on the ride, but 0069a170 is not among them (the empty Y-ring bind), so the probe is its proof. The probe restores the asset record /
emitter state / holder flag / manager slot after each call; the manager's particle slot and +0xb0 counter are put back.
`RX_RIDEFX_PROBE_FRAME` / `RX_RIDEFX_DIAG` select the frame and print the live record. Machine-instruction coverage of the
probe's own calls is not available (`rx_call_original_ex` does not feed `pc_counts`), so the branch list above is case-based.
The caller-path call-site switch in fx_worldparticles.c is ABI-checked (the original passes (S, manager, matrix, index) with
ECX = the matrix, which rx_yring_spawn does not read); the worlds never reach it.

Docs: `006cd2c0` (the gesture classifier) is player-stunt input, not vehicle FX; `0041b4b0` is the localized-package queue drain
(UI/audio); `00839890` is the land-animal scene ticker constructor (loop_entity). None is a vehicle-emitter callee; they stay
unported and outside this task.


## fxgate (2026-10-20): the UI scene's TUT placeable effects and their active gate

The grid LCG audit (loadfix) said the product's state-6 grid pass drew the game LCG 148 units more often than the oracle and blamed the emitters'
active gate with "the product has 7 more active emitters / 3 more rate draws + 3 more builds per tick". The measurement below (baton boot with
`REFLEX_GRID_HOOK` / a new `REFLEX_FX_TRACE`, builds/fxgate) shows the real numbers are different and the cause is exact:

- **The oracle's UI scene (engine context +0x1c) at the state-6 grid pass**: 68 tickers, the 24 `PlaceableParticleEffect` (0x102a) among
  them, and **every one has its controller disabled** (`S = e + 0xa0`, `S+0x1c` = e+0xbc = 0; the effect's `+0x94` manager / `+0x14` holder /
  `+0x18` type index / `+0xa4..+0xb0` squared bounds / `+0xb4..+0xb7` zone flags are all bound and valid, all distances in range: the gate is
  not the distance check). They tick (vf19 00696940 runs for all 24 every frame) and draw nothing.
- **Why they are disabled**: `TutorialManager_Construct` 0074fd10 ends with `ResetLesson(1)` and
  `TutorialManager_ShowLessonObjects(manager, -1)` (00751c40) - the only part of the tutorial manager that touches the scene. Its 0x102a pass
  names every effect from the entity's `+0x18` through `strtok(name, "_")`: a "TUT<number>" effect is shown only when the number (the token's
  last character through `atoi`) is `lesson + 1`; with lesson -1 every TUT effect is disabled through the controller's vf5 (`e+0x90` vtable
  slot 0x14 = 006968e0, which writes `e+0xbc`). The trace shows the exact order at boot: 24 `vf7` (00696810, the bind: `+0x94` manager,
  `+0xb4` type list, `+0xb8` index, `+0xbc` = 1) followed immediately by 24 `vf5(0)` from 0075206e (inside 00751c40). The tutorial manager's
  Update (007500c0) only calls it again when a lesson change is pending (`+0x1c` != 0), which never happens in the reference event.
- **The product's gap**: its TutorialManager is a product boundary (MXUIWorld +0x22180 = 0), so the construction-time pass never ran: all 24
  effects stayed `+0xbc = 1` and every one drew a rate (`0069587b`) and spawned (`006944c0`) on every UI-scene tick.

**Fix**: `core/game/fx_tutorial.c` ports 00751c40 (`rx_tutorial_show_lesson_objects`, registered, adapter `oracle/fx_tutorial.cpp`, group rx_fx:
baton boot 1 call / 1 verified, the frame's own stack words only; the Verify boot's world image is **byte-identical** to the reference's) and
`rx_mxuiworld_start` (007651d0, build_ui_world.c) calls it with lesson -1 at the original's site (the tutorial manager's construct tail; the
manager object is not built, and lesson -1 never reads it - only the lesson-2 trajectory branch does). No emission is suppressed: the effects'
own controller state is what the original's own function sets.

**Result (product probe, RX_FULL_FRAME=1, 1/60)**: at the state-6 grid pass all 24 placeables are `active 0` (the oracle's state), the scene's
FX draws are gone and the grid LCG is **1829** (it was 2091 with the 24 effects drawing, and 4652 before loadfix). The residual 409 units to the
oracle's 2238/2239 are **not** this gate: see `product_chain.md` "Round 8 (fxgate)" - the oracle spends ~33 unpaused game frames in
MXGame state 5 (the loader's "Subscene Creation" job) while the product builds the world synchronously in one frame, and it is those frames'
UI-world vehicle-effect emissions (the empty world's idle vehicle, 0069b060 -> 00695780) that draw the LCG.

**Pause window correction**: `REFLEX_FX_TRACE` on the oracle shows the FX gate's timing word (`[engine context +0x10] + 0xc`) is **0 in
MXGame states 3..5** already (frame 306/state 3 onwards), not only at the state-6 pass. The product's stand-in set it to 1 at the UI world's
creation and cleared it at the state-5 end (loadfix). The original's clear is the loading page's script (`App.Pause(false)`, Lua binding
007424a0); with no UI manager in the product the same clear is now made at the request site in `rx_ui_request_level` (0059bd60, product branch).
It does not change the grid count yet (the product has no load-phase vehicle emissions), but the load-frame work below cannot draw without it.

## The partattr asset (partattr, 2026-10-20): the vehicle effect sets' records

The vehicle FX gap of the grid was the **ParticleAttrib asset loader**: `UI_Compound_Particles.partattr` holds the 33 records of the UI world's
vehicle effect set (and `Nat_Concord_A` the 171 of the event world), but no loader existed, so `00697870` copied nothing (`set + 0x224 = 0`)
and the product had no vehicle dust / roost (gridsubscene Round 9, blocker 1).

**Files**: new `game/build_partattr.[ch]`; `build_load.c` registers it as the `partattr` loader (AssetIO vtable 0x0093a1d8; the type entry,
handle class 0x0095df0c and the handle tag 0041bea0 were already there).

- `006940a0` ParticleAttribAssetIO::vf3: `ReflexAlignedAlloc(0x18)`, vtable 0x0095defc, +8 / +0xc / +0x10 / +0x14 = 0, the parser, the
  asset written through the out pointer (RET 0x14).
- `FUN_00693870` (ECX = the stream, stack: the asset): the temporary XMLReader (`rx_xml_reader_construct` / `_load` / destroy 00824fe0),
  `vf4 "Emitter"` count at +0x10, `ReflexAlignedAlloc(count * 0xa0)` at +0x14, the per-record constructor `00693f70` (it clears +0x34 .. +0x91,
  NOT the name / the type fields / the offset / the +0x92..+0x9f padding), then per Emitter node: Name (its own ReflexAlignedAlloc block,
  strlen + 1), VehicleType / EngineClass / EmitterShape / Tire (the `_stricmp` tables at 0x009d8b20 / 0x009d8b40 / 0x009d8b88 / 0x009d8b50 /
  0x009d8b68), ParticleZone (vf78), DistanceZonesCount + DistanceZones (the count<=0 four bytes at +0xc, the zone list writing 1 at
  `+0xb + value`), the body flag at +0x2c (only the low byte) and the location at +0x30 (the `v == 2/3/4/5` and `v == 4/5/6/7` mapping),
  Offset (vf48 vec3, 16-byte stride: +0x1c..+0x1f untouched), Triggers (five `00694010` blocks at +0x34 .. +0x54: node value float, +4
  Inverse, +5 Enabled) and TireInAir / TireInWater (+0x5c / +0x64: int value +4 Enabled), `TireInWater enabled -> +8 = -1`, EmitModifiers
  (the same five blocks at +0x6c .. +0x8c). `00694010` = the child node via vf7, `Enabled` / `Inverse` through vf58 + vf80 and the node's
  own value through vf51.
- `FUN_00693810` / `006937f0` (registered): free each name and the record array, IAsset vtable 0x0093eb78, the block itself for flag bit 0.
- Record layout (0xa0) is the one `fx_worldparticles.c` documents: +0 name, +4 EmitterShape, +8 ParticleZone, +0xc distance zones,
  +0x10 Offset, +0x20 VehicleType, +0x24 EngineClass, +0x28 Tire, +0x2c body flag, +0x30 location, +0x34 .. +0x91 triggers / modifiers.

**Verification (construction, against the original's loaded assets)**. `builds/partattr/`:

- `partattr_check <install> all`: **43 shipped partattr assets built, 0 failed**, every stream consumed exactly (the type in all databases).
- The two reference assets: `UI_Compound_Particles` **370** records (58103/58103 bytes) and `Nat_Concord_A` **171** (27124/27124).
- `records.py` compares every record field against the original's asset objects in the reference world image
  (`builds/gridsubscene/la/ref/c-world.bin`, found by vtable 0x0095defc: 0x13c2fce0 = 370, 0x1655d870 = 171): **370 / 0 and 171 / 0
  written-byte differences**, names included. The bytes the parser does not write differ (the original's heap residue; the core's fresh
  blocks are zero): +0x1c..0x1f, +0x2d..0x2f, +0x92..0x9f and the two padding bytes of each 8-byte block (16625 / 2814 byte values).

**Product** (`builds/partattr/probe_build/product_probe`, RX_FULL_FRAME=1, W from the first RX_SESSION_RACING frame):

- At the UI state-6 grid pass the UI scene's ParticleEntity's handle data is the **370**-record asset and set 0 (0x00c11e90) holds the
  local vehicle with **n = 33** (the 33 records of VehicleType MotocrossBike / EngineClass 250cc, exactly the oracle's count). After the
  world switch the event scene's ParticleEntity is the **171**-record `Nat_Concord_A` and set 0 has **n = 20** -- the reference world
  image's own set 0 (**n = 20**, the same record order: Exhaust, Hard Hit x2, Low_Ground_Dirt x2, ..., Roost_StartGate).
- **Grid LCG**: the pre-grid count moves **1829 -> 1887** (local StartZone **7 -> 1**); the oracle is 2239/2240. The 58 units are the UI
  world's vehicle effect ticks now that the set has records (`Exhaust` base 30 range 20, 1 build 17 + 1 rate per tick): the remaining
  **352** units are the 30-frame loading-page accept gate (gridsubscene's lead decision, not changed here).
- **Dust / roost**: a 1400-frame W ride (no stop, speed **73.977**, gear 3) tracks each set-0 record's emitter carry (S+0x98, the rate
  draw runs only when the gates pass): the body `Exhaust` 1392 gate-frames (~116 particles); the wheel records `Low_Ground_Dirt` (rear ~54
  + front ~51), `Hard Hit` (rear ~32 + front ~51), `Pig Pen` (~51) and `Roost_StartGate` (~2) -- the dust / roost set, ~190 wheel
  particles during the ride, with the controllers bound to manager 0x00c11bd0 and the vehicle's ParticleEmitter holder exactly as the
  reference set. The particle manager's live count rises from the stationary scene's ~1900-2200 to ~2900 during the ride.
- `00691cc0` (the slot bind, gridsubscene) is what gives set 0 the vehicle; the loader is what gives it the records.

**Not written by the parser** (do not treat as a difference): the name-pointer word at +0, +0x1c..+0x1f (Offset lane 4), +0x2d..+0x2f
(above the +0x2c byte), +0x92..+0x9f and each block's +6 / +7 padding; the original leaves its heap residue there.

**No regression**: `verify.sh builds/partattr/cli/reflex_boot` quick, Concord: rx_fx **108 ports / 0 mismatches / On identical**, rx_world
**132 / 0 / identical**, rx_race **48 / 0 / identical**; `core/build.sh` clean; product `RX_FULL_FRAME=1 rx_api_test <install> 1400` runs
1400 frames, particles rc 0 (live 2923, normal 94, texture `Nat_Concord_A`).

## stuntgesture (2026-10-21): the stunt input mappings and the gesture matcher

The trick input machine's missing pieces were the gesture matcher 006c9aa0 (a stick gesture aborted the product: it still ran through
`rx_call_original_ex`) and the two mapping loaders 006ca5a0 / 006ca3f0 (the product left `[0xa94310]` / `[0xa94314]` 0; the matcher
then read address 0, the arena's zero page, and never matched). The gesture read/classifier 00548810 / 006cd2c0, which sit between the
input machine and the matcher, were unported too and aborted the product as soon as a gesture started (a modifier press + stick
movement); they are ported here as well. Build dir `builds/stuntgesture/` (oracle CLI `builds/stuntgesture/cli`, product `api/`,
ride `builds/stuntgesture/ride/ride`, scripts `rp.sh` / `boot.sh` / `build_ride.sh`, probe `core/oracle/stunt_probe.cpp`).

**Files.** New `game/fx_stunt_input.[ch]`:
- **006ca5a0** `rx_fx_stunt_mappings_init(vfs)`: the aerial then the ground file through 006ca3f0 into `[0xa94310]` / `[0xa94314]`;
  returns 1 when both pointers are non-zero. The original's two name copies (from .rdata 0x00960914 / 0x00960934) are on its
  frame; the product file reads use the session's vfs (the oracle path ignores it and opens through the original CRT, build_xml.c).
- **006ca3f0** `rx_fx_stunt_mapping_load(name)`: the file's `<StuntMapping>` nodes into a fresh (count + 1) x 0x22 block (the last
  record zero: the terminator). Exact frame: alloca 0x11c8, reader at sp - 0x11c0, the 100 x 0x22 scratch at sp - 0xd60, the
  "StuntMapping" name at sp - 0x14, the cursor at sp - 0x11c4; 100-record cap; a record is {+0 the "stunt" attribute, 0x18 bytes;
  +0x18 the "input" gesture, 10 bytes} through 0078fd60("%s"); ReflexAlignedAlloc((count+1) x 0x22), the constructor iterator
  004022d0 with the 0x22-byte zero ctor 006c8e50, the count x 0x22 memcpy and the explicit terminator clear. The original's
  FUN_00827e00 (reader reset) + FUN_00824fe0 (destructor) are one `rx_xml_reader_destroy` here: the same frees in the same order,
  the same live-reader count, and the reader object is below the final ESP.
- **006ca2f0** `rx_fx_stunt_mapping_record(rec, reader, node)`: the two attributes in the original order; returns 1 only when both
  exist (the original's frame flag).
- **006c9aa0** `rx_fx_stunt_mapping_match(airborne, gesture)`: the aerial table when airborne != 0, else ground; walks 0x22-byte
  records while the record's stunt-name byte is non-zero and returns the first whose +0x18 input equals the gesture. The compare is
  the original's two-byte stride stopping at the first NUL.
- **00548810** `rx_fx_stunt_gesture_dir(pad, out)`: the pad's steer / lean KeyMaps (00548fc0 / 00549050, ported by the world unit)
  value of input 7, the pair the gesture classifier consumes. Its ECX at the return is the map's callee residue (no switched caller
  reads it; the adapter declares OUT_FPU only).
- **006cd2c0** `rx_fx_stunt_gesture_classify(buf, dt, x, y, airborne)`: the direction classifier ('0' up, '1' right, '2' down,
  '3' left, 'N' inside [0x9caeb8]'s dead zone), the 3-character buffer +0/+8 (count) / +0x1c (finished = count >= 3) / +4 +5 (the
  current / previous direction), the +0x10 += dt and +0x14 -= dt (airborne only) clocks with the clamp that clears the buffer when
  +0x14 < 0, and the +0xc restart flag. The NaN cases follow the original's `comiss` / `jbe` / `ja`.
`game/fx_stunt.c`'s `gesture_read` (006c9510 states 2 / 3) now calls them and the matcher directly (the call site switch);
`api/rx_session.c`'s init stand-in calls `rx_fx_stunt_mappings_init(fmt_install_vfs(s->install))` after
`rx_stunt_table_load()` (the original's 0075ed60 order 006c8480 / 006c8b70 / 006ca5a0 at 0075f103).

**The shipped content.** `AerialStuntInputMappings.bxml` has **36** mappings (all 3 characters over '0'..'3'; the ground file's rows
are commented out, so its table is the single terminator record). The product's loaded tables: aerial 36 entries, ground 0. The
shipped **right stick mapping** is the remap defaults' Reflex columns: *ReflexLeft / ReflexRight = `rs_x`, ReflexUp / ReflexDown =
`rs_y`* (the pad's right stick, XInput `rx` / `ry`, the game's own 7864 dead zone), and the keyboard's arrows drive the same two
axes. The gesture only starts while the **StuntModifier** composite is down (00548660: pad **LB** / keyboard **Space** by the remap
defaults) and the player's activity is InProgress (2 / 3 in `rx_player_update`); the sequence `323` maps to the aerial trick
`9OClockNac_L_Air` (the table's names are lower-cased by 006c8670; 006c8570 lower-cases the mapping name before the lookup).

**Verification.**
- **006c9aa0** (probe group `m`, the live game state of the Concord world, both tables): **694 calls / 694 verified / 0
  mismatches**, 36 hits (the 36 aerial inputs once each, 1..4-character walks, empty / "N" / "4" / "0x30" misses). (No-table case
  cannot be probed: address 0 is unmapped in the emulator while the product's arena zero page makes it read 0.)
- **00548810** with `REFLEX_PADSIM=1` + `rx_PadSimHook` (pad 0 sticks nonzero: the world test passes no pads otherwise): **359 /
  359 / 0**, axes 0.0759 / 0.7544 at the probe frame; **1 / 1 / 0** without a pad (0 / 0). **006cd2c0**: **11 / 11 / 0** (up / right
  / down / left, dead zone, diagonal + air, window decrease, restart, -0 clock, NaN x / NaN both; the live tracker's buffer is
  poked and restored).
- **006c9510 (call-site switch)**: probe group `i` sets the live tracker's state 2, count 3, buffer "323", window 1 and a synthetic
  ground record, then calls 006c9510: **1 call / 1 verified / 0 mismatches**, state 3, history entry "323", match flag 1.
- **006ca2f0**: baton boot with the original loader running (the two new loaders excluded from the filter): **36 / 36 / 0**.
- **006ca3f0 / 006ca5a0**: baton boot (the original init thread calls 006ca5a0): 006ca5a0 **1 call**, 006ca3f0 **2 calls** (aerial
  and ground), both **0 live-data differences**; the only differing words are the freed **zlib state / 32 KB window** blocks of
  the reader (the host-zlib boundary, known-differences 4; the same class as FMXJudgingParameters). The Verify boot's world image
  is **byte-identical** to a no-port boot's (`c-world.bin` / `c-canon.bin` cmp, all three loader boots), so the loaded tables and
  every gameplay field match.
- **No regression**: `verify.sh <cli> rx_fx` five worlds **114 / 108 / 110 / 112 / 110 ports, 0 mismatches, On identical**;
  `rx_race quick` 48 / 0 / identical; `rx_world quick` 132 / 0 / identical.

**Product ride** (`builds/stuntgesture/ride.c`, `RX_FULL_FRAME=1`, one rider / one lap Concord; it reads the tracker
K = player + 0xb58 and the scorer P = player + 0xc70 from game memory):
- `wad` (W + A/D windows, 2400 steps): no abort, 1278 racing frames, game 40.183 s, race time 21.284, place 1, no tricks (the
  scorer's air / ground clocks run: 4.902 / 14.398).
- `miss` (the same W plus pad LB held and the right stick walked up / centre / up / centre / up = "000", 2400 steps): no abort, the
  same session frame / time as `wad`; the gesture machine runs (state 2/3 for 1254 frames, 15 history entries "000", all misses),
  the matcher walks the 36-record table each time, no trick starts, the scorer / records stay at 0.
- `gesture` (the shipped "323" = 9OClockNac_L_Air): the machine records the match (history flag 1) and then the next unported
  function is reached: **006c98c0** (the trick start; its perfdata asset `MXRider_9OClockNac_L_Air` exists in the product's
  database, so it would set mode 1 and call the mode handler 006c9b60). The trick-start / trick-mode unit (006c98c0, 006cd050 /
  006cd0f0, 006c9b60 / 006c9cc0 / 006c9da0 and their helpers 006c9c20 / 006cd1c0 / 006c9e80 / 006c9eb0 / 006c9df0) is NOT ported here:
  that is the next stop for a mapped trick. The no-abort acceptance ride is `miss`; the mapped gesture's path to the matcher is
  proven per call by the probe and the ride reaches the trick start exactly as the original's flow does.

**Integration notes.** The lifted ports register the same addresses (`native/game/ports/ld_entities_6c.cpp`: 006ca3f0 / 006ca5a0;
`native/game/ports/fx_entities.cpp`: 006c9aa0): exclude/align them at integration. `anim_rider_a_ik.c`'s `input_axis_pair` still
calls 00548810 through the original (another unit's file; switching it to `rx_fx_stunt_gesture_dir` is a one-line sweep item). The
probe's `i` group uses a synthetic ground-table record because the shipped ground table is empty; group `g`/`m` use the real game
tables. `build_ride.sh` and `build_profrun.sh` are copies of `core/tools/rx_api_test.sh` with the driver swapped.

## trickstart (2026-10-21, agent "trickstart"): the trick start, the hold modes and the rider-side animation start

The unit the stuntgesture entry stopped at: 006c98c0 (the trick start) and the tracker's hold modes 1..3 with their helpers, plus
the events they post and the handlers that consume them (the scorer's and the player's). Files: `core/game/fx_stunt.c` (the trick
machine + the scorer's handlers + the stunt-channel crowd listener), `core/game/anim_rider_stunt.[ch]` (new: the player's
"StuntBegin" listener and the rider-animation start), `core/oracle/fx_stunt.cpp` / `core/oracle/anim_rider_stunt.cpp` (adapters,
group rx_fx), `core/oracle/stunt_probe.cpp` (probe groups `t` = the trick machine + the scorer handlers, `r` = the rider-side
start), `core/game/world_rider_61.c` (one call site) and `core/game/world_rider_extern.h` (one wrapper). Build dir
`builds/trickstart/` (`rp.sh`, `boot.sh`, `build_ride.sh`, `ride.c`, logs, `backup/`).

**The machine** (tracker K = player + 0xb58; the state fields are the ones in fx_stunt.c's header):
- **006c98c0** (ESI = K, EDI = the matched trick record): the perf asset gate **006cd050** (`"<MX|ATV>Rider_<trick+0x20>"` ->
  Database::vf26 "perfdat"); absent -> K + 0x60 = 0. Then the handle **006cd0f0** (vf27, create) into K + 0x20, mode K + 8 = 1,
  K + 0x1c = the trick, K + 0x60 = 0, the "StuntBegin" event (0x95ba28, type 0x9cbf38) with {player, trick}, K + 0x14 = K + 0x18 = 0.
- **Mode 1 (006c9b60)**, airborne: the input flag K + 0x18 (an active local player's K + 0x5c != 3, else K + 0x24 == 0); the perf
  asset's hold-begin time **006cd1c0** = the first looping AnimBlock's cropFront + start (0 when none); `0 >= time` -> mode 3; else
  the rider's progress (FloatListeners_Get of +0x48c / +0x46c) > time - dt and a clean flag with an active player -> "StuntHoldBegin"
  (0x960790) + mode 2 (**006c9e80**), else (progress short) nothing is written, else mode 3.
- **Mode 2 (006c9cc0)**, on the ground (dt in xmm0, no stack argument; no `ret`, it always tail-jumps to 006c9c20): K + 0x10 += dt;
  a ground trick (its +0x64 = 0) also runs K + 0x14 += dt; a used clock (K + 0x14 >= 4.0) or a bad flag -> "StuntHoldEnd" (0x960780)
  + mode 3; else airborne -> the chain; else speed >= 5 -> the chain, below 5 -> "StuntHoldEnd" then the chain.
- **Mode 3 (006c9da0)**: progress >= rider + 0xaf20 -> **006c9df0** (the complete: the input/trick state reset, K + 0xc = the commit
  flag, "StuntCompleted" 0x942784); else the chain.
- **The chain (006c9c20)**: the next match K + 0x60 for an airborne (or replay) current trick with the rider out of state 3, the
  vehicle airborne the same way, not in mode 3, and the current trick's perf end (006c9a40) passed: "ComboFromStunt" (0x9607b4,
  {player, old, new}, 006cd7b0) and the new trick's start.
- **The scorer's handlers** on the stunt channel [0xa94304] (registered by 006caf20, build_activity_player.c): "StuntBegin"
  **006cb1b0** (the combo clock, start / continue, the freestyle +0x30 add, the trick maps +0x324 / +0x344 counted twice for the
  combo, the new-trick check, the count and "StuntBegins" 0x92fd00), "StuntCompleted" **006cb3c0** (the trick count / score, the
  combo's airtime + the best trick, the combo finish, the chain and the five recent names), "ComboFromStunt" **006cb560**,
  "StuntLanded" **006cb5a0**, "StuntTakeoff" **006cb610**, "StuntKickOut" **006cb660**, and the chain note **006ccef0**.
- **The crowd listener 00581830** (ActivityAudioLogic::vf5) on the same channel: an active local player excites the crowd by 1.0
  and, below the LCG draw, plays reaction 2 (00674440 / 006745d0, both ported).
- **The player's handler 0054d120** ("StuntBegin", object = the MXPlayer): its own player, vehicle and rider; rider + 0xaf10 == 0 ->
  the ground start **006226c0** (the trick record from the tracker, player + 0xb74; the rider clip map +0xaf7c / +0xaf9c -> the clip
  slot -> **00622790**, which ends the pose override, picks state 1 / 2, scans the rider's trick tables +0xb01c / +0xba48 for the
  slot, looks the clip name up in the animation table (+0x144 + 0x248, 007fa030) and sets the listener range / thresholds); else the
  air start **00622bc0** (the perf clip name -> the animation map +0xafbc / +0xafdc for the animation id -> the listeners +0x46c keep
  the rider's clip slot, +0x48c only it; the clip's data / the state 3, the listener clamp and the perf end +0x24). Then the control
  suppression **0054d3b0** (SteeringDisabled / IgnoreBrake / IgnoreThrottle -> the KeyMap vf9 or the replay flags +0x11cc bits 0..2)
  and, online only (+0xb2c), the "UpdateStuntEvent" packet.
- **The listener broadcast** the trick animation drives: **0063b7d0** (the vector of 0x14-byte target records -> **0063c190**: each
  one steps its current toward the value, clamps it and sends (x - lo) / (hi - lo) to its listeners) and **0063b4b0** (a clamped
  float += value), both reached every frame once the trick is active (the reference worlds' drive mode never runs them).

**Left unported (the product's next stop).** With the trick started the rider animation enters the trick playback per frame
(0061d8b0 -> **00621f40** and **00620a60** when rider + 0xaf10 != 0). 00620a60 (1373 B) is the clip-time / chain-event update that
advances the listener values the tracker's modes read, and its callees 0083e0e0 / 0083e170 / 0083e1d0 / 0083e230 (the clip-range
predicates) and 0063b5f0 (ClampedFloat_Sub) are not ported; 00621f40's crash path adds 00622160 / 00622460. Until that unit lands a
mapped trick starts (mode 1, the listeners set, the scorer's StuntBegin / StuntBegins run) but cannot complete: without the
animation the progress never passes the hold-begin time, so the tracker stays in mode 1. That unit belongs to the rider-animation
share (0x62xxxx / 0x83xxxx).

**Verification** (probe, one frame, live game state; `calls / verified / mismatches`):
- Concord (drive 1, frame 300, group `tr`): 006cd050 1/1/0; 006cd0f0 2/2/0; 006cd1c0 4/4/0; 006c98c0 1/1/0; 006c9b60 3/3/0; 006c9cc0
  4/4/0; 006c9da0 2/2/0; 006c9c20 3/3/0; 006c9a40 1/1/0; 006c9e80 1/1/0; 006c9eb0 1/1/0; 006c9df0 1/1/0; 006cd7b0 1/1/0; 006ccef0
  1/1/0; 006cb1b0 1/1/0; 006cb3c0 1/1/0; 006cb560 1/1/0; 006cb5a0 1/1/0; 006cb610 1/1/0; 006cb660 1/1/0; 00581830 (the crowd
  listener) 1/1/0; 0054d120 1/1/0;
  006226c0 3/3/0; 00622bc0 1/1/0; 00622790 1/1/0; 0054d3b0 2/2/0; 006ccfc0 1/1/0; 0063b6e0 2/2/0; 00850cd0 2/2/0; 0063b2b0 1/1/0;
  0063b1e0 1/1/0; 0063b7d0 123/123/0; 0063c190 1/1/0 (a made-up vector).
- FreeRide_FR_Alpine (drive 3, frame 300): the same set 0 mismatches, and the per-frame broadcast helpers get their real counts:
  **0063b4b0 242/242/0**, **0063b7d0 121/121/0** (121 frames of an unported anim caller driving them).
- **No regression**: `verify.sh <cli> rx_fx` five worlds **114 / 108 / 110 / 112 / 110 ports, 0 mismatches, On identical**;
  `rx_race quick` 48/0/identical; `rx_world quick` 132/0/identical; `rx_anim quick` 44/0/identical; `core/build.sh` clean.

**Product** (`builds/trickstart/ride.c`, `RX_FULL_FRAME=1`, one rider / one lap Concord, `builds/trickstart/api/ride`):
- `gesture` (W + LB + the right stick "323"): the matcher records the match at step 1376 (`history flag 1`), 006c98c0 starts the
  trick and the dispatch runs 00581830 + the player's 0054d120 (the air animation start sets the rider's state 3 and the clip) +
  the scorer's 006cb1b0; the next frame stops at unported **00621f40** (the rider animation's trick playback), step 1377 /
  session frame 1386, game 23.150 s, race 4.234. No abort before that; the trick does not reach completion (see below).
- `wad` (W + A/D, 2200 steps): no stop, 1078 racing frames, game 36.850 s, race 17.950, 0 tricks, scorer 0, records 0.

**Notes / known differences.** (1) The two map lookups 0063b6e0 / 00850cd0 reuse the trick map's insert-with-hint instantiation
006cd4a0 for their missing-key path (the same 0x30 node record; only the two pad bytes +0x2a / +0x2b of a *newly inserted* node
could differ, and the animation assets contain the clip names, so the path is not taken). (2) 006f34d0 ("UpdateStuntEvent") is a
network boundary: oracle original, product nothing (the caller's online branch needs the player's +0xb2c). (3) 00622790's ground
path needs a shipped ground mapping (the ground StuntMapping file has no records), so the probe reaches it through the air variant
and the direct call; the tracker's ground start is the path a jump-less trick would take. (4) The mode-1 hold-begin path is probed
with a forced looping AnimBlock (a stimulus, restored) because the shipped perf clip has no looping block.
(w-fsfix correction: the shipped clips DO have one. 96 of the 106 MXRider_*_Air perf assets have a looping `_Hold` block, e.g.
MXRider_9OClockNac_L_Air In 0-30 / Hold 30-60 looping / Out 60-85 ticks; scratch dump of every perfdat. 00620a60 leaves that block
at once when the trick is not held (+0xaf30 0, +0xaf38 0: 00620c00 sets the time to its end), see core/plan/QUEUE.md "w-fsfix".)

## trickplayback (2026-10-21, agent "trickplayback"): the rider's trick / crash animation playback 00621f40 / 00620a60 and its clip-range / chain-event helpers ported; the product plays the "323" trick animation and the scorer/score path runs, but the ride wrecks before the scripted clip ends

The unit trickstart stopped at: 0061d8b0 (rx_world_rider_update_blends) calls **00621f40** (the crash/trick wind-up step) and **00620a60**
(the clip-time / chain-event update) every frame while the rider's +0xaf10 is set. Files: new `core/game/anim_rider_play.[ch]`
(0083e0e0 / 0083e170 / 0083e1d0 / 0083e230, 00621f40, 00620a60, 00622160, 00622460, 006cd240, 0063b5f0),
`core/oracle/anim_rider_play.cpp` (adapters, group rx_fx), `core/oracle/stunt_probe.cpp` (probe group `p`), `core/game/fx_stunt.c`
(00588b50 the combo score drop), `core/game/loop_player.c` (00764f80 MXRecords_FailedJump), `core/game/world_rider_extern.h` (the
switched wrappers: 00621f40 / 00620a60 / 0063b5f0 / 0063b360 / 0063b3f0). Build dir `builds/trickplayback/`.

**The playback.**
- **00620a60** (stdcall ra, dt; RET 8): the animator's time +0x46c is advanced by dt (clamped to [+0x464, +0x468], through the
  frame slot) and the chain events of the current clip play between the old and new time (00620fc0). If **0083e170** says a
  looping block contains the time, rider state 3 broadcasts the block's effective end (**0083e230** = end - cropBack) to the
  +0x46c listeners (`ClampedFloat_Set` on ra + 0x464, ESI is +0x464 there); otherwise the +0xaf30 wind-up path runs
  (**0083e0e0** picks the block, **0083e1d0** returns its start + cropFront). Past the current clip's asset +0x28 the next clip
  (+0xaf18) switches in: its +0x70->+0x2c goes to the +0x48c listeners, the clip events replay from 0, +0xaf14 = +0xaf18,
  +0xaf20 = +0xaf24, +0xaf18 = 0, the pose-override clip +0xc928 / +0xc92c swap (EndPoseOverride or the simulation's vf84), the
  +0x46c listeners resynchronize and the +0x48c ones are disabled. A scripted pose clip (+0xc928) then sends its blend
  (`phys_pose_blend`) and pose (+0x450 / +0x460 / +0x490, or the blended ra + 0xc950 / +0xc960 / +0xc990) to the bike
  simulation's vf28.
- **0083e0e0 / 0083e170 / 0083e1d0 / 0083e230** are PerformanceAnimator block predicates (blocks 0x70: start +0x40,
  cropFront +0x4c, cropBack +0x50, end +0x54, additive +0x64, override +0x68, looping +0x6c; count +0x8c, time +0x98, block
  record +0x70->+0x18). 0083e230 / 0083e1d0 return the block's effective end / start, -1 [0x9cd9e8] when none.
- **00621f40** (ESI = ra): +0x5b2d4 0 nothing; 1: both layer targets (+0x598 / +0x58c) at 1.0 -> 2; 2: **00622160** (state 2) when
  +0x5b2d0 is set, else **00622460** (state 1), with the stored player +0x5b204 and the trick snapshot at +0x5b210, then 0. The
  two starts name the trick's perf dib "%sRider_%s" (MXRider_9OClockNac_L_Air), take the perf end +0x24, the clip slot from
  +0xaf9c / +0xaf7c, the animation clip and its length +0x94 into +0xaf20, and the +0xaf34 wind-up flag from **006cd240** (the
  first looping block's end - cropBack) / **006cd1c0** against the driver's progress.
- **0063b5f0** ClampedFloat_Sub (listeners = clamp(get - *value)), **006cd240** the first looping block end.
- **00588b50** (caller 006cb720): the combo score-drop event to [0xa94318] (type 0x9cbeb4, name 0x92fce0, payload the player).
- **00764f80** MXRecords_FailedJump (+1 TotalFailedJumps, column 0x96d9cc), the trick abort's record write.

**Verification** (probe on the live game state, one frame, `calls / verified / mismatches`; Concord group `p`):
0083e170 14/14/0, 0083e230 14/14/0, 0083e1d0 14/14/0 (incl. NaN / +-Inf), 0083e0e0 6/6/0; 0063b5f0 4/4/0; 006cd240 1/1/0; 00621f40 4/4/0
(state 0, 1 -> 2, 2 -> 00622460, 2 -> 00622160); 00620a60 **205/205/0** (200 frames of the real trick clip: time to 2.833,
the state-3 / wind-up / state-4 / clip-switch paths); 00622160 1/1/0; 00622460 1/1/0. The probe's FreeRide run reaches the
switch body but the reference rider's animation is not linked to a vehicle there, so the probe stops in the game's own next
frame (a probe-state limit, not a mismatch). 00588b50 has no probe call (the reference world's stunt manager is not built); it
is product-driven (a wreck's score drop ran in the long ride).

**Product** (`builds/trickplayback/ride.c`, `RX_FULL_FRAME=1`, one rider / one lap Concord):
- `gesture` (the shipped "323", W + LB + right stick): the first match is at step 1376 (history flag 1) and the trick starts
  (K mode 1, rider +0xaf10 1, the scripted clip 14c18650, +0xaf20 2.833). The bike wrecks at step 1388 (12 frames later) and
  the tracker aborts; the animation update plays up to ~1.0 s of the 2.4 s scripted clip.
- `trickair` (W + LB + "323" on every jump): starts the 9OClockNac trick at steps 1443 / 1817 / 2970 / 3263 (Concord) and at
  1466 / 2557 / 2786 / 3323 / 3749 / 4159 / 4829 (NAT_Stillwater); the mode-1 hold (1.0 s) is reached on the longer hops
  (e.g. Concord 1500-run: mode 3 at step 1639, clip time 0.983) but the bike wrecks on landing before the scripted clip's
  2.4 s / 2.833 s threshold, and after the landing the rider animation state ends, so the tracker's mode 3 never sees the clip
  end. Scorer: the only completions recorded are the game's own wheelie / whip (e.g. score 6-7, 5 tricks / 33.0 after 8000
  steps; 1 trick / 7.0 after 20000 steps); the 9OClockNac never completes, so the score does not move for it.
- The long ride (20000 steps) runs to race 314.6 s, 0 laps (the rider is not driving the lap; W only), no unported address:
  00588b50 and 00764f80 are now ported and the crash/score path no longer stops.

**NEXT STOP (rider landing / trick completion, not a missing port):** the scripted trick clip (006226c0 -> 00622790, clip
14c18650) needs 2.4 s of airtime (or the trick to survive the landing with +0xaf10 = 1 long enough for 00620a60 to pass the
clip's +0x28 and switch), but every automated jump lands with a wreck (vehicle +0x280 = 23) before then, and the landing's
rider-animation end (00621400 EndStunt / 00622e30 EndCrash, world_rider_62.c) returns +0xaf10 to 0 so the clip time freezes.
The completion unit is the landing / trick-survival state (a clean jump and the exact 00621400 / 00622e30 interaction), not
another playback callee. (Answered by "trickland" below: the landing wreck is the original's own behaviour; the completion path's
two unported listeners 00581950 / 0054d220 are ported there and the trick now completes and scores.) `builds/trickplayback/ride.c` has the `gesture` / `trick` / `trickair` modes and the `GESTURE_AT`,
`COAST`, `LEAN`, `RIDE_TRACE`, `LOC`, `ETYPE`, `ETYPE_ID` knobs used for the search.

## rideparticle (2026-10-21, agent "rideparticle"): the rider's lens-swipe effect vf3 0063c040

The next stop of both 12-rider rides (groundnormal: race 9521 / 9629) was the idle gesture's attach: `0063c040`
`AttachableParticleEffect_ParticleEmitter::vf3` (198 B, vtable **0x00958908** slot 3), the object the rider constructor
allocates at rider + 0x5b6dc (`rx_rider_lens_effect_construct` 0063bff0, build_rider_ctor.c). `0061eac0`
RiderAnim_UpdateGesture's state 2 calls it once (+0x5b6e0 latch) through the rider's model node.

**Port: `rx_fx_attachable_emitter_vf3` (fx_emit.c, group rx_fx; adapter oracle/fx_particle.cpp).** Read off the
disassembly: unbound (+4 == 0) -> the **active scene's** particle entity (0xa94264 [scene + 0x2cc4], scene = [engine + 0x1c
+ 4 * [engine + 4]]) gives the holder ([ent + 0xb0]), its type list ([[holder + 0x44] + 0x10], record [self + 0xc] * 0x1f0)
and its manager ([ent + 0xc4] when the record's +0x160 byte says additive, else [ent + 0xc0]) with the holder's busy mark
([holder + 0x4c]) raised around the read; +4 = the manager. Then the attach flags (a first attach +0x2c == 0 also sets
+0x30 = 1; +0x2c = 1), the effect's node (+0x100) evaluated (007db890), and the emitter at +0x10 (its slot 0 = 00695ab0)
called with (dt through `fld [esp+0x10]` / `fstp [esp]`, the manager, the node's world matrix [node] + 0x1c + 0x40 *
[node + 0xb], 1, -1). Frame: push ebx / esi / edi; the two internal calls at the original's ESP with the return addresses
0x0063c0dd / 0x0063c102. **No other function was needed:** its only callees are `rx_world_node_evaluate_world` (007db890)
and `rx_fx_emitter_vf0` (00695ab0 -> 00695780), both ported; it is dispatched through the vtable with `RX_FN` and the
original as the fallback (exactly like 0069b060 `rx_fx_vehicle_emitter_vf3`, which has the same shape).

**Call site (world_rider_61.c `rx_world_rider_update_gesture`):** the original's 0061ebea `push ecx; fstp [esp]; mov
[ecx + 0x100], esi; call [vtable + 0xc]` became `world_vcall_f(obj, 0xc, ...)` at the original's pushed-slot depth (the
argument at [esp - 4], the return address 0x0061ebec written). The old `WORLD_CTX` call site made the product stop naming
0063c040.

**Files:** game/fx_emit.c (the port), game/fx.h (prototype), oracle/fx_particle.cpp (the adapter; it declares no output
registers: the only caller, 0061eac0, reads none - the original leaves the emitter's EAX / EDX), game/world_rider_61.c (the
call site), oracle/rideparticle_probe.cpp (the probe).

**Per-call Verify** (`builds/rideparticle/`, Concord, frame 300; the probe builds the gesture state on the live local rider
- +0x3c4 float = 1.0, +0x5b6d4 = 2, +0x5b6e0 = 0 - and calls the ORIGINAL 0061eac0 so the original's model-node block and
its vtable call reach the port):
- `rx_fx` alone: **0063c040 4 calls / 4 verified / 0 mismatches** (logs probe_final.*). Cases: unbound + non-additive record
  (manager 0xc11bd0), unbound + additive record (0xc11d80), bound + attached (no +0x30 write), bound + first attach; the
  activity scene's set is 0x17b73790 with 171 records, the grid world's effect object has type 71 / manager 0xc11bd0 bound.
- Instruction coverage of the original (REFLEX_COVERAGE, probe without rx_fx): **57 / 57 instructions** of the 198 bytes
  (no dead code).
- Composed (`rx_fx,rx_world`): the core caller 0061eac0 **4 / 4 / 0** - the call-site switch, its memory writes including
  the emitter's spawn effects (LCG, manager slot) equal the original's.

**Product (12 riders, AI 95, RX_FULL_FRAME=1, 1/60 s, 12000 steps, builds/rideparticle/ride):** both rides **pass the old
stops and finish all 12000 steps with no stop** (state 2 racing, session frame 12009, race frame 10881, game 200.179 s):
- W held: max 83.9374, gear 4, path 5434.092, hash `ffbc895e861107b1` (race 9521 is session frame 10649).
- clean: max 75.4390, gear 3, path 3777.965, hash `bb7442296fceb053` (race 9629 is session frame 10757).
No unported address is reached: the lens effect emits its particles through the already-ported emitter / spawn / gate
functions. A 24000-step W run also finishes with no stop (session 24009, race frame 22881, game 400.130 s, hash
`f3ee644800662bb4`), so the ride's next stop is not this unit's subsystem: none was reached.
`builds/rideparticle/logs/{w12000,clean12000,w24000}.{out,err,tsv}`.

**No regression** (`builds/rideparticle/cli`, quick = Concord 1200, drive 1): rx_fx **108 ports / 0 mismatches / On
identical**, rx_race **48 / 0 / identical**, rx_world **132 / 0 / identical** (state hash 1f6611fa80b80831, memory
1487a52f208b4b26). `core/build.sh` clean. The standard product driver `rx_api_test` (builds/rideparticle/api, 3000 race
frames) exits 0 (state 2 racing, game 50.194 s).

**Integration / left:** the lifted `native/game/ports/ui_camera.cpp` registers the same address (group "ui",
`AttachableParticleEffect_ParticleEmitter__vf3`): exclude / align it at integration like the other lifted duplicates. The
class's other virtuals (`00696420` vf0 the deleting destructor, `006c3a60` vf2) are not reached by the idle gesture (vf3 is
the only slot the ride calls; the object lives for the rider's lifetime) and stay unported; nothing in vf3 needs them.

## trickland (2026-10-21, agent "trickland"): the landing wreck is the game's own (the original wrecks the same way on the same jump); the trick-completion listeners 00581950 / 0054d220 ported, so a 9 O'Clock Nac started at takeoff of the long jump now completes and scores

Task: trickplayback left the "323" (9 O'Clock Nac) starting and the rider animation playing, but every landing wrecked the bike
(vehicle +0x280 = 23) before the tracker's completion threshold, so the scorer never recorded the trick. Question: is the landing
wreck a port bug (pose / state / clip / end-stunt ABI) or the game's own behaviour? Build dir `builds/trickland/` (oracle CLI
`cli`, product ride `api/ride` from `ride.c`, `run_oracle.sh` / `boot_oracle.sh` / `build_ride.sh` / `rp.sh`, probe
`core/oracle/trickland_probe.cpp`, logs `logs/`, backups `backup/`).

**The A/B device.** `trickland_probe.cpp` (oracle only, group "thunk", `RX_TRICKLAND=1`) has two ports: a scripted pad for the
reference world (the XInput imports replaced like `PadSimHook`: pad 0's LB held and the right stick walked "323" while the
tracker's `+0x88` says airborne and no trick runs, released afterwards - the product ride's `trickair` input) and an
`Engine::Update 0084ed20` frame dump of the same fields the product ride prints: tracker mode / current trick, rider
`+0xaf10` / clip / `+0xaf20` / the +0x46c +0x48c listener values, vehicle `+0x280`, the wreck checker, speed, and the root
body's quaternion / velocity / up vector. So the ORIGINAL game runs the same trick on the same track under the same input.

**Evidence (same jump, one-frame agreement).** The first real jump of the Concord ride (product step 1411, oracle frame 1272;
the world image is 139 frames ahead of the session clock, same 41 airborne frames, same takeoff speed 75.6 / 76.4):

| | product (`gesture`, W + LB + "323") | oracle (original, same input) |
|---|---|---|
| takeoff | step 1411 | f1272 |
| trick starts (mode 1) | step 1443, clip `14c18650`, af20 2.833 | f1305, clip `152b8ea0`, af20 2.833 |
| airborne ends | step 1451 (`air 1451 -> 0`) | f1312 (`AIR 0`) |
| next frame | step 1452 mode 0 af10 0, **wreck 23** | f1313 mode 0 af10 0, **wreck 23** |

The original starts the trick mid-air exactly like the product (the gesture needs ~32 frames), plays the same clip for the rest
of the flight and wrecks at the landing with the same type (0x17, the wreck checker's impact/stuck type: vehicle `+0x234 +0x14`
hit sustained -> pending 0x17 while riding). A second original jump (takeoff f1637, trick at f1670, landing f1689) wrecks the
same way. **First divergence: none** - the pose (root quaternion / up vector at the landing: e.g. product q (-0.1400, -0.2880,
0.0241, 0.9470) vs oracle q (-0.2216, -0.2318, -0.0547, 0.9456) on the two runs' own lines; both are ~20 deg roll, both
landing in the same attitude ballpark), af10, the clip time, +0x280 and the end-stunt calls all agree. The wreck is the game's
own: a 2.833 s trick cannot be finished inside a 41-frame (0.68 s) jump, and the game bails a rider who lands on a live trick.
The only Concord jump long enough for the 2.833 s clip is the 190-frame (3.17 s) one later in the ride: with the trick started
at takeoff it completes before the landing (below); started 32 frames in (the gesture's minimum) it cannot.

**The completion path had two unported functions (now ported, group rx_fx).**
- **00581950 ActivityAudioLogic::vf8** (`fx_stunt.c`, `rx_fx_stunt_audio_complete`): the stunt channel's "StuntCompleted"
  listener - the cue 005811a0 under "StuntCompleted" (0x942784) with the player, then (player +4 != 0, +0x10 == 0) the crowd
  excite [0x9c47f4] (00674440) and, below the game LCG draw [0x9c481c], reaction 2 / 1 (006745d0). The trick argument is the
  event's second payload word, never read. Adapter `oracle/fx_stunt.cpp`.
- **0054d220 FUN_0054d220** (`anim_rider_stunt.c`, `rx_rider_stunt_completed_evt`): the player's "StuntCompleted" listener -
  own player + vehicle + rider animation only; rider `+0xaf10` != 0 -> 00622e30 EndCrash (the state 4), 0054d480 (the first
  three input groups back on), a type-7 activity (freestyle) adds the trick's `+0x6c` to the player's `+0x11b8`, and online
  (+0xb2c) the "UpdateStuntEvent" packet (006f34d0 with 2, trick+0x74, 0) behind 00884950 (`rx_net_ref_valid`). Offline the
  network branch is not taken. Adapter `oracle/anim_rider_stunt.cpp`.
  Verification, probe groups on the live Concord state: **00581950 3/3/0** (active local player / +4 = 0 / +0x10 = 1) and
  **0054d220 4/4/0** (another player / state 0 / state 3 -> EndCrash / a type-7 activity adds 200 to +0x11b8)
  (`logs/probe_*`, `builds/trickland/probe_trickland_National_NAT_Concord_{t,r}`). 00581830 (vf5) still 1/1/0 in the same run.

**Product ride (1 rider / 1 lap Concord, `RX_FULL_FRAME=1`, builds/trickland/api/ride).**
- `trickair`, 14000 steps: the natural gesture timing starts the trick 24-32 frames into a jump, so the clip reaches at most
  **1.217 s** (step 10299-10372) of the 2.833 s threshold and every landing still wrecks - the game's own outcome (above).
  The longest air spans of this ride (4380-4570, 190 frames; 13500-13705, 205 frames of a slow skim) are the only ones long
  enough; the first needs the trick to start at takeoff (32 frames of gesture + 158 frames of flight < 2.833 s).
- **`AIRSTART=1 AIRSTART_AT=4380`** (the driver starts the trick with the game's own 006c98c0 and the 9 O'Clock Nac record the
  moment the rider leaves the ground at the 4380 jump - the same call the input machine makes when a player times the gesture
  at takeoff): trick at **step 4382** (speed 73.64, clip `14c18650`, af20 2.833), mode 3 from 4426, clip time **2.817 at
  4551**, **completes at step 4552** (mode 0, af10 4 via EndCrash) and the scorer moves: **tricks 0 -> 1, score 7.2300,
  recent '9oclocknac_l_air'** (`ride: END scorer ... score 7.230000(40e75c29)`). The run continues to 5200 steps with no stop
  (session 5209 frames, game 86.854 s, race 67.950, 4078 racing frames). Without the two ports the same ride stopped at 4552
  in **00581950** and then **0054d220**.
- `GESTURE_ALWAYS=1` (the gesture walked on the ground too) does not work: the gesture window +0x14 drains while grounded and
  the machine stalls in state 2, so no early match happens.

**No regression** (`builds/trickland/cli`, logs `logs/verify_*`): rx_fx five worlds **114 / 108 / 110 / 112 / 110 ports,
0 mismatches, On identical** (Concord state `a936214c3ad9ccf1`, memory `6f65ef1e44548ca1`; Concord_ai11 `fd1d4b45b48e5a5e` /
`136c116ea8ee8dab`; FreeRide `1ef28933aa2c03c7` / `ee6d922ebb4e0805`; SX `fe8060ee3ca40a7b` / `607469270a50e895`; Terracross
`0f1c6fa1301d6af3` / `0073899e3eddbd6d`); rx_anim quick **44 / 0 / identical**, rx_race quick **48 / 0 / identical**,
rx_world quick **132 / 0 / identical** (state `1f6611fa80b80831`, memory `1487a52f208b4b26`). `core/build.sh` clean (dylib
sha256 `69a6f872c8e3f2580a194f6205016743fb3315ceec3d1383dc3283477df5563b`); the standard product driver `rx_api_test`
3000 race frames exits 0 (state racing, particles 2914, ambient 1381, 12 mud records).

**What remains.** The trick completes and scores only when started at takeoff of a >= 2.9 s flight; the automated rides'
gesture timing cannot do that on Concord (the match needs ~32 frames and the jumps are 41-190 frames), so the acceptance needs
the `AIRSTART` timing (a human timing the gesture does the same). The 13500 "jump" is a slow skim that aborts the trick in
mid-air (wreck 23 while the tracker still says airborne), not a usable jump. Nothing else on the path is unported.
