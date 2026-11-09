# Construction: building the race world from the install (core/game/build_*)

Status 2026-10-18 (construction architect). The core must build, from the player's install and its own code, every object the race
frames read, with the original's exact layouts, so that the hand-ported frame code (phys_ / world_ / bike_ / anim_ / race_ / loop_)
runs on it. This note is the map of that work: what the race reads, which original functions build it, how it is grouped into
construction units, how construction is verified, the recommended agent split, and the first unit (implemented and verified).
Plan file: `core/plan/rx_build.tsv` (every construction function: address, name, unit, size, boot instructions, event-only).

## 1. Measurements (reference event: National NAT_Concord, 12 riders, AIDifficulty 95, baton boot)

Tools (all in `core/tools`, products in scratchpad `rx_build/`):
- `build_map.py <canon> <world> <race.rmap> <out.tsv>`: joins the original's canonical heap dump (REFLEX_WORLD_CANON), the world image
  of the same boot (REFLEX_WORLD_OUT) and the race's read map (REFLEX_READSET of a 1500-frame race on that world). Recovers every
  canonical node's address from the image (`resolve`: global roots, weak roots by plain-word signature, then along pointer words:
  177,885 of 178,409 nodes, 5 inconsistent), attributes each read granule to a node and, inside the node, to the object whose vtable
  is the nearest below it; follows referrers that point at a node's payload start up to a typed owner ("owner chain"). Outputs
  `<out>.tsv` (per read node: address, size, read bytes, inner classes, referrers || owner chain), `<out>.classes.tsv`, `<out>.chains.tsv`.
- `build_ctors.py <classes.tsv> <boot coverage>`: per class the functions that store its vtable and ran in the boot, and their callers.
- `build_units.py <boot coverage> <world> <out.tsv> [front-end coverage]`: construction functions = ran in the boot, not in race frames
  (native/game/plan/frame-functions.tsv); "event-only" = also not in a front-end-only boot (400 frames of the front end, scratchpad
  rx_build/fe). Assigned to units by roots + direct calls (E8/E9 decoded from .text) + class vfunc names. Static and approximate:
  virtual calls are not followed, vfunc-name patterns over-assign (e.g. render vfuncs of entity classes land in `scene`).

The baton boot of the event under the CLI (`REFLEX_BOOT_SCHED=baton REFLEX_EVENT=1 REFLEX_EVENT_SPEC=National,10,NAT_Concord,50,0,3,Laps,2,0
REFLEX_RIDERS=12 REFLEX_AI=95 REFLEX_WORLD_OUT REFLEX_WORLD_CANON= REFLEX_COVERAGE`) gives a world byte-identical to
`boot/worlds/National_NAT_Concord_ai11-world.bin`; its canonical dump: scratchpad `rx_build/orig/c-canon.bin`.

What the race reads (5.37 MB): 5.02 MB in 13,297 heap nodes, 185 KB .data, 15 KB .rdata, 20 KB CRT data, 136 KB elsewhere (stacks,
race-time allocations). By owner (bytes read; build_map chains + inner classes, regex-grouped):

| owner / unit | read | what |
|---|---|---|
| asset_anim | 1.62 MB | AnimAsset key data: packed blocks (AnimAsset+0x64 -> header +0x3c, 1.19 MB), track arrays (+0x1c), names |
| vehicle | 1.03 MB | 12 x RavenSimulationMotocross (0x8fc0-byte objects with embedded RavenPhysics::Matrix* / Vec* / constraints), 12 x 0x26010 per-rider buffers, ProxyObjectHandles, DynamicRays; and the small-object pool arena [0x00d94a38] (710 KB read, see 2) |
| fx | 853 KB | PlatformParticleManager2D's particle array (global object at 0x00c11bd0, +0x14 = 2.5 MB array of 0x270e particles: 837 KB read), emitters |
| rider | 510 KB | Character / animators / IK (InverseKinematicsAnimator+0x54 data 212 KB), RiderAnimation, ActionContext; the ClipRecorder ring [0x00a7f124] (1.87 MB, 143 KB read: replay records, ClipRecorder_Init / AllocBuffer) |
| scene | 120 KB | 1,149 InstancedAnimationEntity, 524 AnchorEntity, 283 ModelEntity, 265 CollisionEntity, anim / scenario / crowd entities |
| terrain | 105 KB | HFTerrainSurface+0x1c height grid (16 MB block, 84 KB read), DeformablePatch, TerrainTracks, XenonTriangleSurface |
| asset_model | 78 KB | ModelAsset images (+0x38 node data, +0x154 ...) |
| collision | 57 KB | CollisionShape* (Sphere 204, StaticBox 51, StaticCylinder 27, StaticComposite 16, Triangles 11, Handles 231), Static / Physics object handles |
| forest, audio, ui, data, camera, activity | 42 / 40 / 36 / 32 / 22 / 33 KB | ForestSystem; FMODEvent (not core); UI (not core); BXMLAsset / PackedData / Rdb tables; 94 Camera, MXThirdPersonCamera; MXActivityRace, 12 RaceDriver / MXPlayer, AI behaviours |
| .data | 185 KB | the table database (0x00ae0910.., ~60 KB: activity / player / rider tables), input key maps (0x00d5b000..0x00d6a000), engine / game globals |

Boot functions: 6,830 ran; 1,558 also run in race frames; **5,272 construction-only** (1.15 MB of code); of those **1,183 event-only**
(not run by the front end: 0.31 MB), the rest are loaders shared with the front end (asset IO, database, scene, entity factory).
Front end, UI, Lua, audio, render, saving and network are not core construction (the core starts from the event spec).

## 2. Memory facts every unit depends on

- **Blocks.** Every game allocation is `_aligned_malloc(size, 16)` (ReflexAlignedAlloc 0078fbe0, FUN_00401f90, StdMemoryAllocator::vf1
  004eef20): msvcr90 `malloc(size + 0x13)` = one heap block, payload at block + 0x10, the block address at payload - 4. The canonical
  dump's nodes are these blocks (node size = size + 0x13, the object at node offset 0x10). `new T[n]` of a class with a destructor:
  `n * elem + 4`, count first. `_aligned_realloc(NULL, ...)` (common in loaders: arrays of zeroed elements) is a plain allocation.
  Implemented: `game/build_alloc.[ch]`.
- **Arenas that hold many objects** (their contents are compared by offset inside one node, so the core must allocate inside them in
  the original's order): the small-object pool at `[0x00d94a38]` (3 MB block, set up by FUN_00744180: pools of 0x10..0x80-byte cells,
  counts 0x5dd / 0x2711 / 0x1f41 / 0xbb9 / 0x1f5 / 0x191 ..., marker `efcdab01`; its users still to be listed); SimpleMemoryAllocator
  bump arenas (vf1 008236d0); the 2D particle array; the ClipRecorder ring. Everything else is one block per object.
- **Fields written outside the loaders**: AnimAsset +4 (the database's load serial, `[database + 0xe8]++`), +8 (reference count),
  +0xc (package record) are written by Database::vf30 0084ad10 after AssetIO::vf3 (watch of 0x11d97a54: pc 0084af08). Expect the
  same for every asset class (unit asset_system).
- **The reference world is captured ~418 frames after the boot started the event (the intro ran)**: fields the frames write differ
  from a freshly built world. Found so far (REFLEX_WATCH on the addresses, baton boot without JIT): the packed anim header's +0x14 /
  +0x18 / +0x34 / +0x38 (AnimKeys_Locate 0082b930, a race-frame function, caches the located keys there) and the new-layout
  position-track elements' +4 (1 at load; FUN_00840440 caches a key index there while the intro plays: 0xf / 0xe in the world). Construction is compared with that in mind:
  such fields belong to the frame code (or to a later setup step) and are listed per unit.
- **Uninitialized bytes**: the original's fresh blocks hold old heap contents (names after the NUL, alignment padding, block slack, the
  block header's first 12 bytes); the core's are zero. `RX_BUILD_FILL=<byte>` makes the core fill new blocks; a second build with
  another fill gives the bytes the construction never writes, which the anchored compare skips (`--undef`).

## 3. Construction units

Function counts / code bytes from `core/plan/rx_build.tsv` (construction-only functions; event-only in parentheses). Static assignment:
treat as upper bounds and as the starting list, the agent of the unit finds the exact tree while transliterating (callees, virtual
calls). Kind: **S** = mostly format reading / mechanical construction (Sonnet), **O** = deep transliteration with ordering, math or
cross-unit state (Opus). Estimates in lines of C.

| # | unit | entry points (original) | inputs | functions (event-only) / bytes | kind | estimate |
|---|---|---|---|---|---|---|
| 0 | **alloc** (done: section 8.1) | CRT _aligned_malloc / realloc / free; ReflexAlignedAlloc 0078fbe0, 00401f90, StdMemoryAllocator vf1/vf2; small-object pool FUN_00744180 + its alloc / free; SimpleMemoryAllocator 008236d0 / 00823700 | none | 3 + pool | O | 300 (done: 100) |
| 1 | **asset_system** (done: section 8) | Database::vf30 0084ad10 (AssetIO::vf3 dispatch, serial / refcount / package fields), handle creation (AssetIO::vf1), handle binding (+0x44), which databases / packages an event loads and in which order (the level's Shared / <level>Base / subscenes, Animations, RiderPhysics, vehicle and rider part databases, AIData, EngineDependencies) | `.dx9.database` via fmt_install | Database / XMLReader classes in other_database (147 / 23 KB, loader thread) | O | 800 |
| 2a | **asset_anim** | AnimAssetIO::vf3 0082cc70, ctor 0082bb90, loaders 0082bf10 / 0082bda0; AnimAssetIO::vf1 0082ccc0 (handle); ActionAssetIO | Animations database `anim` | 7 / 4 KB | S | **done** (250) |
| 2b | asset_model | ModelAssetIO::vf3 007d53c0 / vf1 007d5410, ModelAsset ctor 007d3f30 and its stream loader; Morph | `model` / `surface` (fmt_model for checks) | 6+ / 2 KB + loader | S | 600 |
| 2c | asset_collis | CollisionAssetIO::vf3 00818ef0 / vf1 00818f30, CollisionAsset | `collis` (fmt_collis) | 14 | S | 400 |
| 2d | asset_data | BXMLAssetIO 008221f0 / 00822280, PackedDataAssetIO 007f8f30, TableDataAssetIO 004f0170, TDFAssetIO 0053a0c0, LocalizationAssetIO | `bxml`, `packdata`, `tbldata`, `tdf`, `localiz` | 61 / 14 KB | S | 700 |
| 2e | asset_terrain | TerrainAssetIO 006d8900, TerrainCollisionAssetIO 006d8bd0 | terrain assets (fmt_terrain) | few | S | 300 |
| 2f | asset_forest | ForestAssetIO 00661f10, ForestMaterial / Tree / Grass AssetIO, XenonForestAsset_Parse 0065cd90, ForestAsset_ParseTreeTypes 0065dd30, ForestTreeType_BuildLodRecords 0065eb00 | `forest` | 21 / 21 KB | S | 900 |
| 2g | asset_render (only what the race reads) | Texture / XenonMaterial / Shader / Surface AssetIO (0079xxxx, 007a10d0): handles and the few fields read (MaterialAssetHandle, XenonShaderAsset) | `texture` / `material` / `shader` | 127 / 35 KB (mostly not needed) | S | 300 |
| 3 | **world** | MXWorld construction FUN_00759f00 (from MXGame::Update state 3, 0075847d), MXWorld::vf1 00765f80 / vf3 007665f0, FUN_007669f0, the physics context (world + 0x20134), PhysicsWorld, Proxy / Static / PhysicsObjectHandle, broadphase (FUN_00882370 ..) | level + subscene list | 52 (13) / 16 KB | O | 1,200 |
| 4 | **scene** | SubSceneAssetIO 007d3e90 / 007d3f00, the entity factory **FUN_004046a0** (switch on the entity type id: alloc + ctor per class; jump table 0x405ab4; Ghidra folds it into StatusBoardEntity::vf22, see ld_entities.md), per-class property loaders (e.g. InstancedAnimationEntity::vf5 00412480, ModelEntity ctor 00401db0, CollisionEntity 00818f60, AnchorEntity 00403e90, AnimEntity* 006b9ad0 / 006ba570, ScenarioAnimEntity 0074a810, BillboardCrowdEntity 006c5f30, PopUp / PopDownBale, DropGate) | the level's scene (fmt_scene: entities, RSData fields) | 292 (107) / 169 KB (over-assigned) | S (factory, 40+ entity classes) | 3,000 |
| 5 | **terrain + collision** | HFTerrain entity / HFTerrainSurface (height grid block), DeformablePatch 007c4630 / 007c8cf0, TerrainTracks, XenonTriangleSurface; CollisionShape builders 00844360 / 008446e0 / 00848cd0 (StaticBox 00846320, StaticCylinder 00847790, Sphere 008453f0 / 00845440, StaticComposite, Triangles), CollisionShapeHandle 00776630 / 00776770 / 00777310 / 007784e0, CollisionEntity::vf28 00819650, FUN_007a2300 | fmt_terrain, fmt_collis | 14 (6) + terrain | O | 1,500 |
| 6 | **vehicle** | VehicleFactory_CreateVehicle 00542b90 -> RavenVehicleMotocross_Construct 004622b0, RavenVehicleMotocross::vf28 004625d0 -> RavenSimulationMotocross FUN_0043ac40 (+0043b750, 0043a560, 004392f0, FUN_00430b50 DynamicRays, 0042f640 / 0042f520 / 0042fae0 constraints and matrices, 0045cec0 proxies), FUN_00541690, 00542540, 004772f0 / 00476e80, VehicleLoader_Assemble 0053ddc0 (parts), WreckChecker 00618180 | vehicle databases (.phy-like BXML, collision rays), the event's vehicle choices | 231 (107) / 69 KB | O | 3,500 |
| 7 | **rider** | 0053e920 (RiderAnimation, WreckChecker), 0053eab0 / RiderCharacter 00624750, Character 00834cb0 via 00839a40, animators (Motion 0083b330, Blend 00832f50, Additive 008345c0, Switch 0083c590, IK, Procedural, Expression) created by 0082d040 / 0082d280 / 0082d480 / 0082d620 / 0082d730 / 0082d960, FUN_00619490, 006398a0, 00638ab0, 004246c0, ActionContext | RiderPhysics, Animations, AnimationSets.bxml, rider gear databases | 127 (102) / 61 KB | O | 3,000 |
| 8 | **activity** (race, players, drivers, AI) | the event spec -> tables (PlayerList / RiderList rows: EnsureLocalPlayers 0059e030, RiderList_AddRow 004dea90, 004de810, 004dd460, MXUIManager::SetupEvent 0059a970 / StartEvent 0059ade0, the AI roster MXUIManager_BuildAIRoster 0059f3e0 / AddAIRiderOfClass 0059ebc0), FUN_0075f890 (activity creation), MXActivityRace ctor 0055ef80, RaceDriver 0050ed80 (MXActivityRaceStub::vf44), MXPlayer 005490f0 / 00552ad0, PlayerSlot_FromPlayerList 0054a7b0 / PlayerSlot_SpawnStep 00767200, AI behaviours (MXActivityRaceStub::vf47 00565e00: BehaviorAvoidFlipBack 005878e0, Seperate 00588630, LapPacing 005884f0, HandleBraking 00588770, AirLeveling 0051f330 ...), RaceIntroScenario 00565830, MXRecords_InitRow / Join | MXTables (fmt_table), AIData, the level's TDF / activity entities | 611 (243) / 154 KB (incl. UI-manager code that must become plain C) | O | 4,000 |
| 9 | camera | Camera 0063dc30 / 007909f0, MXThirdPersonCamera::vf32 0064a330 (presets), MXFirstPersonCamera::vf32, replay / tower / path cameras | camera BXML (`Cameras/`) | 15 (12) / 12 KB | S | 800 |
| 10 | fx | PlatformParticleManager2D (global at 0x00c11bd0, init 008e7xxx), ParticleEmitter 00694110, PlaceableParticleEffect 006964d0, Fireworks 00692140, ParticleAsset / ParticleAttribAssetIO 007e3a10 / 006940a0 | particle assets | 29 / 5 KB + manager | S | 900 |
| 11 | replay | ClipRecorder_Init / ClipRecorder_AllocBuffer ([0x00a7f124], 0x5b68 x 0x50 records), the race-state recorders | none | few | S | 150 |
| 12 | **globals + startup tables** | initial .data from the exe file in the install (read the PE's .data: original data, not code), MXGame::Startup 00756e10 and the game init thread 00760400 (startup tables 0065c200, 0065c070, 006b64a0, 006b68e0), MXRegistry_Load 004e6c30 (the table database at 0x00ae0910, schemas: PlayerList_CreateSchema 004dcfc0, ActivityResults_CreateSchema, FUN_004decd0 ...), controller key maps (Controller_BuildKeyMaps 00547f30), the math tables the frame code uses (sin / acos tables built at startup: double precision, see OU's lesson) | MXRegistry.bxml, MXTables_Default, KeyBind_Core.xml, the exe | other_tables 193 (67), other_game 239 (37) | S (+O for the math tables) | 2,000 |

Not core (the core does not build them): UI pages / components / fonts / icons, front end, Lua, audio (FMOD events: the client does
sound), render-only state (render events, shadow / bump / water passes, number plates' textures), saving, network (Quazal).
Frame code that only touches them must be cut at the frame side (the loop_ agents decide).

## 4. Verification of construction

- `core/tools/rx_canon.[ch]`: `rx_canon_write(path, extra_roots, n, classes, info, ninfo)` writes the core arena as the RXCN v1
  canonical dump of Boot::write_canonical (same node naming from the roots, pointer / text / leaf rules, hash, layout), roots = the
  exe's .data + .tls in address order, then the caller's extra roots; also `<path>.addr` (node name -> address). `rx_canon_image`
  writes the arena as an RXWI image. Then:
  - **whole world**: `native/game/tools/world_compare.py orig-canon.bin core-canon.bin` (final acceptance once the core builds a
    whole world). Caveat: the original also has roots in the CRT DLLs' data, the threads' TLS and the live stacks; nodes named through
    those get other names in the core's dump (only-A / only-B) until the core also has those globals (the CRT's state: crt_ agent).
  - **partial world** (while units land): `core/tools/rx_canon_cmp.py --orig <canon> <world> --core <canon> <world> [--undef <core
    world 2>] [--ignore CLASS:offs] [--cstr CLASS:field:stride] anchors...` pairs objects from anchors (`G:<global>`, `A:<orig>=<core>`,
    `H:<handle vtable>:<name>=<core addr>`, `@file`) and walks both heaps in parallel along pointer words: same plain words, pointers
    to paired targets at the same offsets; an original pointer where the core has 0 is MISSING (not built yet, not walked); words that
    look like pointers on one side only but have the same raw value are data. Self-test: original vs itself from one AnimAsset walks
    172,929 nodes, IDENTICAL.
- The original's reference: a baton boot of the event with `REFLEX_WORLD_CANON=` (and REFLEX_WORLD_OUT for the image), the same
  command as section 1; `loading_accept.sh` boots the same way.
- Driver: `core/tools/build_test.c` (`build_test.sh <out dir>` builds it from src/*.c + game/build_*.c + rx_canon.c, product flags):
  `build_test <install> <prefix> anim` builds a unit and writes `<prefix>-canon.bin`, `-world.bin`, `-anchors.txt`.

## 5. Implemented and verified: alloc (CRT layer) + asset stream + asset_anim

Files: `game/build_alloc.[ch]` (msvcr90 malloc / _aligned_malloc / _aligned_realloc / _aligned_free over rx_heap, ReflexAlignedAlloc,
`new T[n]` with count; RX_BUILD_FILL), `game/build_stream.[ch]` (the loaders' InputStream over fmt_install_read bytes),
`game/build_anim.[ch]` (AnimAssetIO::vf3 0082cc70, ctor 0082bb90, loader 0082bf10 with every version branch, packed loader 0082bda0).

Run (scratchpad rx_build): `build_test <install> core/anim anim` builds all 401 `anim` assets of the Animations database (every one
consumes its stream exactly), `RX_BUILD_FILL=0xa5` a second time, then
`rx_canon_cmp.py --orig orig/c-canon.bin orig/c-world.bin --core core/anim-canon.bin core/anim-world.bin --undef core/animF-world.bin
--ignore AnimAsset:4,8,c --cstr AnimAsset:14:20 @core/anim-anchors.txt`:
**all 401 anchors found in the original world; 1,410 node pairs walked, 1,393 identical, no missing subtrees**; the 17 that differ are
all fields written after loading by playback code (section 2, confirmed by watchpoints): 13 packed headers (+0x14, +0x18 floats,
+0x34, +0x38 pointers: AnimKeys_Locate 0082b930) and 4 track blocks (position-track elements +4: 1 at load, 4 / 15 / 14 in the
world: FUN_00840440). The canonical hash formula of rx_canon.c was checked against the original's dump: recomputing every node's hash
from its stored words / pointers matches the stored value for 19,640 / 19,640 original nodes and 1,410 / 1,410 core nodes; two core
builds give byte-identical dumps; world_compare.py parses the core's dump. Ignored: AnimAsset +4 / +8 / +0xc (Database::vf30, unit asset_system).
Names compared as C strings (the bytes after each NUL are uninitialized in the original).
Notes: the only x87 operation (0082bda0's fld / fstp dword copy of the duration) is precision-independent (bits kept, a signalling
NaN quieted). Registered (RX_FUNCTION) are only functions with the game's ABI shape: ReflexAlignedAlloc 0078fbe0 and the ctor 0082bb90;
the stream loaders take the core's rx_stream and are called directly. In the oracle build (`-DRX_ORACLE`, build_cli.sh compiles
game/*.c) build_alloc routes the aligned allocations to msvcr90 through the exe's IAT slots (0x916358 / 0x91641c / 0x916350), so a
core loader run in the emulator makes the game's own blocks; all three build_*.c compile there.

## 6. Recommended order and agent split

1. **alloc (rest) + asset_system** (Opus, one agent): the small-object pool and its users, Database::vf30's fields, the handle tables,
   the database / package load order of an event. Everything after it hangs objects off handles.
2. In parallel, **asset loaders** (Sonnet, two agents): 2b model + 2c collis + 2e terrain; 2d data + 2f forest + 2g render handles.
   Each: transliterate the AssetIO vf3 / ctor / stream loader like build_anim.c, verify every asset of its type with rx_canon_cmp
   (H: anchors of the type's handle vtable).
3. **world + terrain/collision** (Opus): MXWorld, physics context, static collision from the scene's collision entities and
   fmt_collis / fmt_terrain. Needs 1 and 2c / 2e.
4. **scene** (Sonnet, after 1-2): the entity factory table and the entity classes' property loaders over fmt_scene's RSData fields.
5. **vehicle** (Opus) and **rider** (Opus), in parallel after 1-3: the largest pieces, exact float math in the constructors
   (inertia, constraint matrices: watch the x87 precision rule of core/README.md, the main thread is 53-bit).
6. **activity** (Opus): event spec -> tables -> players / drivers / AI / scenarios. The tables part (12) can go to a Sonnet agent first.
7. **camera, fx, replay, globals** (Sonnet): small, independent once 1 lands.

Rough size: ~25,000 lines of C over ~2,000 original functions actually needed (of the 5,272 construction-only, most are UI / front end /
render / audio / database internals the core replaces with fmt_* readers). Verification for every unit: rx_canon_cmp from anchors
(handles, globals, the objects of the previous units), then world_compare on the whole world at the end.

## 7. Requests to the lead

- rx_heap: a fill byte for new blocks would let the core mark unwritten bytes without build_alloc's own fill (rx_heap_set_fill), and an
  in-place resize (for _aligned_realloc's _expand path, only relevant if a unit reallocs a non-null block).
- rx_heap's allocation order is the core's own; arenas that pack many objects (section 2) are allocated by build_alloc inside one
  rx_heap block, so nothing else is needed from rx_heap for them.
- The race read map is from one event; construction of other events (FreeRide, Supercross, ATV, trucks) adds classes (see loading.md
  section 9's series-3 trucks): boot coverage per event type should be added to rx_build.tsv as the units grow.

## 8. Implemented and verified: alloc (rest) + asset_system (units 0 and 1, 2026-10-19)

Files: `game/build_pool.[ch]` (the small-object pool, SimpleMemoryAllocator), `game/build_alloc.[ch]` (+ ReflexAlignedFree 0078fc50),
`game/build_asset.[ch]` (the Database, hash tables, AssetIO dispatch, database / package loads, vf30 / vf23 / vf31), adapters
`oracle/build_pool.cpp` (group rx_build_pool), the tracer `oracle/build_trace.cpp`, tools `tools/build_test.[ch|sh]` (units `assets`, `pool`),
`tools/build_cmp.py`, `tools/build_script_concord.txt` (the test oracle's operation list, see 8.4).

### 8.1 The small-object pool [0x00d94a38] is the Lua heap

FUN_00744180 is the script system's init (struct at 0x00d94938): eight cell pools of 0x10..0x80 bytes (+0x30, 0x18 each: cell size,
count, 0x10, cells, free stack, top) carved from a first-fit heap (+0xf0: size 0x300000, end, alignment 0x10, chunk header 0x10, the
_aligned_malloc(0x300000, 16) block = [0x00d94a38], first chunk, free list; chunk {size, next free, previous free, used byte, marker ab cd
ef}). The only user is the Lua VM: lua_newstate (0077bc10) gets FUN_00743f00 as frealloc with ud = 0x00d94938 (sizes rounded to 16;
0x10..0x80 from the pool of that size while it has cells, the rest and an exhausted pool from the heap; frees go back to the pool whose
range holds the pointer, else to the heap's free list head; the heap coalesces only when no free chunk fits). So the 710 KB the race reads
there are Lua objects (strings, tables, closures, the states' stacks) of the front end's, the activity's and the scenarios' scripts:
building them exactly needs the Lua VM and the same script runs (a Lua unit; not in the plan yet: lead). Ported: 00743f00, 00823710,
008237e0, 00823900, 00823960, 008239a0, 00823a50, 00823a90, 00823ab0, 00823b20 and the memory part of 00744180 (`rx_lua_memory_init`).
- Static check (`build_test <install> core/pool pool` vs the world image): all eight pools' cell arrays and free stacks at the same offsets
  into the block as the original's (cells +0x10 / +0x7580 / +0x5f410 / +0xc4d70 / +0xddc50 / +0x11b530 / +0x127910 / +0x132ef0), the heap
  header identical; tops and the free list differ (the Lua run).
- Per call: Verify of 00743f00 (with every heap / pool function under it) in a baton boot of the reference event: see 8.6.
SimpleMemoryAllocator (0x14 bytes: block, cursor, capacity 0x19000, used; vf1 = (size + 0x13) & ~0xf bump): its only instance lives
inside FUN_007564f0 around one scene-XML read and is freed there; nothing of it persists. Ported (00823600, vf1, vf3, 00823680, vf0).

### 8.2 The Database (layouts in build_asset.h)

One Database (0x83b4c bytes, engine + 0x18), three hash tables (+4 tag -> AssetIO, +0x24 name -> database record, +0x44 tag -> type
entry {CRITICAL_SECTION, handle table}), the load serial +0xe8, the request ring indices +0xec / +0xf0 / +0xf4, the last asset's name /
tag +0x94 / +0xd4, the background cache (+0x82b88 on, five 0x31c slots at +0x82b8c: "Database\" + HR_Rider* / HR_ATV* / HR_MX* / HR_*).
Tables start with 50 / 5 / 500 buckets and grow to (int)(n * 1.5f) when count / n > 0.5 before an insert (so the final bucket counts, 75 /
163 / 2530 for anim, follow from the insert history). Database records, package records, asset records and handles: build_asset.h.
The 36 AssetIO types (tag, static object in .data + init guard, vtable, handle class and size) are a table in build_asset.c, registered in
the original's order (subscene, material (the only heap AssetIO, singleton [0x00da67fc]), texture, surface, model, morph, anim, collis,
sound, soundbnk, sounddat, bxml, shader, action, cell, water, particle, texatlas, icongeom, banner, perfdat, packdata, localiz, grass, tree,
forest, frstmtl, trncol, terrain, adv, tbldata, script, uicmpnt, tdf, bink, partattr). Every handle class is 0x50 bytes except
BXMLAssetHandle (0x58, the tag kept at +0x50).

### 8.3 API for the asset loaders (stable)

```c
rx_addr db = rx_database_create();                 /* ReflexAlignedAlloc(0x83b4c) + 0084a2d0 */
rx_database_register_all(db);                      /* the 36 AssetIO registrations (vf24) */
rx_asset_register_core_loaders();                  /* rx_asset_set_loader(<AssetIO vtable>, fn) per ported type: add yours there */
rx_database_load(db, in, "NAT_Concord", "Database\\", p5, p6);       /* Fn_0084b0a0: record, handles (+0x48), packages */
rx_database_load_package(db, in, "NAT_Concord", "Shared", progress, p98);   /* every asset through vf30 */
rx_addr h = rx_database_find_handle(db, "collis", "Fence01");        /* the handle; its asset at h + 0x44 (0 = not built) */
rx_addr p = rx_database_find_package(db, "NAT_Concord", "Shared");   /* vf8 */
rx_database_add_asset(db, name, asset, tag_lo, tag_hi);              /* vf23: assets other code creates */
rx_database_unload_package(db, "MXUI_Streaming", "ArcadePosters_AP_NAT_ConcordA");
```
A loader is the core's AssetIO::vf3: `rx_addr fn(rx_stream* s, rx_addr name, uint32_t tag_lo, uint32_t tag_hi)` returning the new asset
(the original's reads / allocations from the stream; build_anim.c's AnimAssetIO is the model). The Database writes asset +4 (serial), +8
(+1 per package load), +0xc (the package record) and handle +0x44 itself (vf30), so loaders must not. Types without a core loader still get
their handles, records and serials (the handle's +0x44 stays 0: the compare shows "missing"). The handle classes for every type exist
(rx_asset_io_new_handle = AssetIO::vf1 of all 36). Not ported: an asset's destruction (AssetIO::vf4, vf31 at refcount 0) and vf30's reload
path (pkg -1 with an asset present): both abort; the reference boot needs neither for built types.

### 8.4 Which databases and packages, in which order (the reference boot)

Traced with `oracle/build_trace.cpp` (REFLEX_BUILD_TRACE=<file> REFLEX_PORT=rx_build_trace, On mode, RUN_ORIGINAL: logs Database ops and
lets the original run; two traced boots gave the same sequence): 36 registrations, 72 database loads, 195 package loads, 4,606 asset loads
(Database::vf30, 870 of them of already loaded assets: refcount + serial only), 21 Database::vf23 (20 XenonUIMaterialAssets of the UI, 1
ModelAsset StatusBoardPoster from 0041610a), 1 package unload (the arcade poster, PackageCache_Update 00543e50), 281 requests, final serial
4,627 = [Database + 0xe8] of the world. `tools/build_script_concord.txt` is that sequence (a TEST ORACLE: build_test replays it; the
product must derive it from the event with the requesting code below, ported by the units that own it). vf23 runs on the main thread while
the loader thread loads packages: the script gives each its serial and build_test runs it between two assets of the package being loaded
(rx_asset_between_loads), which reproduces the original's interleaving exactly.

The requesters (return addresses of the Database calls; vf4/vf5 = load database (wait / async), vf15/vf16 = load package, vf17 = unload
package, vf14 = loader callback), in boot order:
| who (original) | what | unit |
|---|---|---|
| Engine::Startup 0084e2xx (+269 / +285) | EngineDependencies (database + package) | 12 globals/startup |
| UIManager::vf30 005dad05 / 005dad67, then FUN_005dbcb0 (the UI's localized package request: callers UIManager::vf30, FUN_0053c410, 00415be0, 005a4350, 005b7a80, 007273d0) | MXUI: Base, BaseLocalized.ENG, Intro.ENG, FrontEndIntro / Shared / InGame / FrontEnd .ENG, HudNationalSinglePlayer.ENG, the MXUI_DLC00x .ENG, MXUI_Streaming poster; p98 = its flag argument | ui (not core) / activity for the HUD |
| MXRegistry_Load 004e6c30 | MXTables_Default | 12 |
| FUN_0053cbd0 | Rider | 7 rider |
| FUN_0075ed60 (the game init thread 00760400) | Animations, RiderPhysics, CameraTweaks, AIData, SnakeDatabase, StuntDatabase, UI_Compound, Music; then Animations, RiderPhysics, SnakePackage, AIData, StuntData packages; UI_Compound/{Shared, UI_CompoundBase} through FUN_007560f0 | 12 |
| FUN_0067e010 (from 0067da10) | the audio databases and packages (VehicleEngineAudio x 11, UIAudio, VehicleFoleyAudio, CrowdSoundProjects, AudioDependencies, AudioLocalized*) | audio (not core; their serials still count) |
| FUN_0053c410 | MXTables_DLC003..5, MXUI_DLC001..5 (".\Database\") | 12 |
| FUN_00676ae0, MusicPlayer_TickScenario | Music/TrackList, ScenarioTrackList | audio |
| FUN_0073de80 (from 00712b10) | UI_CompoundExtras, RiderUIAnimations (Base) | ui |
| FUN_005403b0 (callers 005401b0, 00542f20) through the package cache FUN_00543750 (database) / FUN_00543b30 (package, MXGame + 0x15554) | per rider's vehicle: MX_GraphicKits, MXs, MX_Handlebars x 2, MX_Tires, MX_Wheels, MX_Exhausts x 2, MX_Forks, MX_Hguards (each database also its HR_ twin, never a package) | 6 vehicle (choices: 8 activity) |
| FUN_0053d0d0 (callers 0053cef0, 0053efa0) through the same cache | per rider's gear: Rider_Boots / CPs / Jerseys / Helmets / Goggles _Materials + _Models, Rider_Skin_Materials, Rider_NBs | 7 rider (choices: 8) |
| MXGame::Update 00758acb (game state 3) then FUN_007560f0 (from MXGame::Update 00758472 / 00758ad3) | the level database, then its packages Shared and <Level>Base (progress floats in MXGame at 0x101f1908 / 0x101f190c) | 3 world |
| FUN_0041b4b0 (LocalizedPackageReferenceEntity::vf1 00416d47, FUN_0041b410, ...) | AudioLocalizedActivity: Audio_ACTIVITY_NATIONALS, Audio_Loc_NATIONAL / OVERTHROW / ELIMINATION .ENG | 4 scene (entity) |
| FUN_00674f20 (from 00674e90) | CrowdSoundProjects/Crowd_Large_Outdoor | audio |
| PackageCache_Update 00543e50 | unloads the poster package | 6/8 |
| MXGame::Startup 00756efe (vf34) and 0054002b..00540082 (vf38) | the background cache on, its four HR_ patterns | 12 |
The serial of every asset depends on ALL loads before it (front end, UI and audio included): the product reproduces it only if it issues
the same sequence. The audio / UI parts are not core code, so either their database / package requests are ported as plain calls (no
asset building: types without loaders cost nothing but handles and serials), or the serials are declared not compared (whether any race-frame code reads an asset's +4
was not measured: the lead decides).

### 8.5 Verification (rx_canon_cmp from the Database)

`build_test <install> core/assets assets tools/build_script_concord.txt` (1.6 s), a second run with RX_BUILD_FILL=0xa5, then
`tools/build_cmp.py --leaf-pointers --orig orig/c-canon.bin orig/c-world.bin --core core/assets-canon.bin core/assets-world.bin --undef
core/assetsF-world.bin --cstr AnimAsset:14:20 @core/assets-anchors.txt` (anchor A:10867c70=<core Database>).
- The core's asset loads equal the original's trace: 4,627 / 4,627 in the same order with the same serials.
- **20,251 node pairs walked from the Database, 19,817 identical, 434 differing, all explained; no pairing conflicts; 2,868 missing
  subtrees** (handles of types without a core loader yet: texture 659, material 567, surface 316, model 315, perfdat 259, bxml 166, uicmpnt
  151, script 134, shader 114, collis 60, sound 27, soundbnk 24, + 13 small types; and anim headers' pointers written by race frames).
  915 AnimAssets are built (the Animations database and the rider / level anims of other databases), their +4 / +8 / +0xc now compared.
- The 434: 245 handles' +0x4c "loading" byte = 1 (texture 223, surface 15, material 5, uicmpnt 2: set by the render code each frame,
  FUN_007f6a60 at 007f7c41 / 007f7bb8, REFLEX_WATCH; vf30 leaves it 0); 75 packed anim headers +0x14 / +0x18 / +0x34 / +0x38 (AnimKeys_Locate,
  section 2); 43 anim float-key track blocks (AnimAsset +0x2c elements +4: the key index caches of playback, section 2); 68 AnimAssets
  +0x54 = 1 (written by AnimEntitySingleAnimLoop::vf5 at 006b9cab: the scene unit, watched); 2 big anim key blocks whose aligned-header
  word the canonical dump classes as data on one side (leaf classification of 64 KiB+ blocks, same value); the Database's 4 OS handles
  (+0xf8 / +0xfc events / thread, +0x83b38 / +0x83b48 events).
- `build_cmp.py`: rx_canon_cmp plus pointer pairing inside leaf nodes (database records and anim key blocks over 64 KiB are leaves in
  the canonical dump: their pointers are plain words; 16,862 such pointer pairs, each target paired consistently).
- Product build: core/build.sh clean (0x007fa250 is registered by the race port rx_name_hash_find, not again by build_asset.c).

### 8.6 The pool per call (oracle)

`RX_CORE_FILES="build_* crt_*" native/game/tools/build_cli.sh <dir>`, then a baton boot of the reference event with
`REFLEX_PORT=rx_build_pool REFLEX_PORT_VERIFY=1`: **rx_LuaAlloc 00743f00: 68,990 calls verified, 0 mismatches** (every heap / pool
function under it is the core's); the Verify boot's world image is byte-identical to the original's; the On-mode boot (the core's
allocator for every Lua allocation of the boot) gives a **byte-identical world image** too. The original reuses two argument slots
(the free path's loop counter in nsize's slot, the pool address in ud's slot): `rx_lua_alloc_slots`, stored by the adapter.
SimpleMemoryAllocator vf1 / vf3 are not called in this boot (its arena is used only in FUN_007564f0's scene path, not taken here).

### 8.7 Left (units 0 / 1)

- The load ORDER as game logic: the requesting functions of 8.4 belong to the units named there (world: MXGame::Update's level
  request + FUN_007560f0; vehicle / rider: FUN_005403b0 / FUN_0053d0d0 through the package cache FUN_00543750 / FUN_00543b30 /
  PackageCache_Update; activity: the rider roster that picks the parts; startup: Engine::Startup, FUN_0075ed60, MXRegistry_Load,
  FUN_0053c410, FUN_0053cbd0; UI / audio requests: to be issued as plain calls or dropped by the lead's decision on serials). The package
  cache (MXGame + 0x15554, FUN_00544220 / 00543b30 / 00543750 / 00543e50) is the next asset-system piece to port with them.
- Asset destruction (AssetIO::vf4 per type, vf31 at refcount 0, vf30's reload path), vf21 (load from a stream), vf25 / vf32: not needed
  by the reference boot for built types.
- The Lua heap's contents (a Lua unit: VM + the scripts the boot runs); the OS objects (critical sections are written as the boot's
  stand-in leaves them, events / threads are not created).
- The AssetIO objects of TextureAtlas / IconGeometry have UI members (a critical section, lists) the core does not build (UI).
- Trace tool for other units: `oracle/build_trace.cpp` (REFLEX_BUILD_TRACE=<file> REFLEX_PORT=rx_build_trace): registers only when the
  variable is set; logs and returns RUN_ORIGINAL (the boot is unchanged); add ports there to find who requests something.

### 8.8 Database virtuals of the requesting code (added by the rider unit, 2026-10-19)
`build_asset.c` registers the Database virtuals the core's requesters call (thiscall db; declared in build_asset.h):
- vf8 0084a890 `rx_database_vf_find_package(db, dbname, pkg)`: the record by the database table (007fa250), then _stricmp over its
  packages (0xa0 each).
- vf26 0084ac20 `rx_database_vf_has_handle` and vf27 0084ac70 `rx_database_vf_handle(db, name, lo, hi, flag)`: the type entry (tag table
  +0x44), then the type's handle table (+0x18); vf27 returns 0 when the name is absent (its tag copy into a local on that path has no
  effect). The type entry's CRITICAL_SECTION is not taken (one thread).
- vf4 0084b020 `rx_database_vf_load_db(db, dir, name, p3, p4)` and vf15 0084a7f0 `rx_database_vf_load_package(db, dbname, pkg, p98)`:
  the original posts a loader request (0084c190: request 3 {+0x10c dir, +0x210 name, +0x250 p3, +0x254 p4} / request 1 {+0x210 db,
  +0x250 pkg, +0x108 p98}) and Sleeps until vf7 / vf10 report it served. The product serves it at once with rx_database_load /
  rx_database_load_package on the install set by `rx_database_set_install(in)`, then advances the request ring (rx_database_request),
  as the loader thread does. The oracle build runs the original, so the loader thread stays the emulator's. This makes FactoryGearLoader
  (0053cbd0) "unverifiable" in a Verify boot: its waits switch threads.

### 8.9 Asset release through the original's virtuals (render-side assets unit, 2026-10-19)
`rx_database_release_asset` (Database::vf31 0084af30) is the original's sequence: the handle's asset (+0x44) loses a reference; at 0 the asset's
vtable slot 2 returns its AssetIO, the AssetIO's slot 4 (`rx_asset_io_destroy` = AssetIO::vf4 007d3ee0, shared by the classes, registered in
build_asset.c) deletes the asset through its vtable slot 0 (flag 1), then the handle forgets it (+0x44 = 0, AssetHandle::vf5); otherwise an asset
owned by that package is marked unowned. The virtuals are looked up with RX_FN; one that is not ported aborts naming its address. Ported: the
texture's (XenonTextureAsset::sdtor 007b2620 and vf2 007b3410, build_render_asset_texture.c: releases every D3D texture and forgets its GPU registry
record, FUN_007b0ee0). Forest / shader / water / tree / anim / ... assets reaching 0 abort (their sdtors are not ported: a package unload of
a level). Verified per call: oracle/build_render_asset.cpp, `REFLEX_PORT=rx_build_asset REFLEX_PORT_VERIFY=1` in the baton boot of the reference
event (the arcade poster package is unloaded: 1 call, 0 mismatches).

## 10. Implemented and verified: world + terrain + collision (units 3 and 5, 2026-10-19)

Files: `game/build_world.[ch]` (the physics world, broadphase, registries, the PhysicsProperties registry, the grids' finalize, MSVC
vector helpers), `game/build_mxworld.[ch]` (MXWorld constructor and vf1), `game/build_terrain.[ch]` (HFTerrainEntity::vf5 and what it
builds), `game/build_collision.[ch]` (CollisionEntity's collision part, shapes, static objects), `game/build_render.[ch]` (the engine's
GPU resource registry, vertex declaration cache, shader-asset lookups), `src/rx_d3d.[ch]` (section 11); tools `tools/build_world_test.[c|sh]`
(driver), `tools/build_world_cmp.py` (static objects / deformation checks), `tools/build_world_accept.sh` (one event end to end).
Layouts: the header comments (build_world.h: physics world, cursor, grid; build_terrain.h: HFTerrainEntity / Surface / DeformablePatch /
TerrainTracks; build_collision.h: CollisionEntity, static object, the shape classes; build_mxworld.h: MXWorld).

### 10.1 What is built, in the original's order (FUN_00759f00 and the scene's entity loads)

| step | original | core |
|---|---|---|
| MXWorld (0x22150) | 00765e20 (+ camera director 0074abe0 with the "PlayScenario" command insert 00817f70 / 00818200, world camera 0063d2c0 / 0063d930 / 0063e490 / 007909f0, 00536ca0) | `rx_mxworld_construct` |
| physics world (0xcce0) | 0075da90 -> 00768830 (vectors, reserves, 4 broadphase cursors 00773950 / 0084a0d0 / 00882370, CollisionShapeHFTerrain, world list 00773cd0 / 006b7590) | `rx_physics_world_create` |
| PhysicsProperties registry | 00850010 / 008501a0 (11 materials 008507f0, 78 contacts 00850b00 / 0084fe70, std::map name -> id 00850cd0 / 00850f30 / 00586700 + rotations, from the EngineDependencies BXML read with fmt_bxml) | `rx_phys_props_get` (first use) |
| MXWorld::vf1 | 00765f80 (contact handlers into world + 0x1928, name, camera into the scene, 00536ef0, entity queries 0x107a / 0x1019 / 0x104d) | `rx_mxworld_start` (scene services and other units' functions through rx_call_original until ported) |
| terrain | HFTerrainEntity::vf5 007c8db0: 007c4890 dims, 007cc880 height copy (LockRect of the HeightMapAsset), 007c4d10 coarse bounds, 007cacc0 deformable patch (two R32F render targets, the SYSTEMMEM deformation surface: Clear 0x80808080 + GetRenderTargetData), 007cb9a0 / 007cbbf0 the three TerrainTracks rings (30 / 10 / 5), 007c4cc0 terrain list, the scene's projection matrix (+0x2cd0), light list 007e0e90 / 007f9e70 and entity array 00544cc0 | `rx_hfterrain_entity_load` (after Entity::vf5) |
| collision | CollisionEntity::vf5 00819130's collision part (row normalisation, the asset handles, audio / sound zone / AIAvoid / PhysicsType / DriveOn fields), vf28 00819650: kind 22 per triangle group a Triangles shape (008449d0) + static object (00769550), other kinds one static object per entity (007693c0) with the shape shared through the global shape list (008445e0 / 008446e0: StaticBox 00846320, StaticComposite 00848cd0 with its children, Sphere 008453f0, StaticCylinder 00847790 / 00847b00, Unknown 00844930), the static record 0084fc20 (00401740 quaternion), vector push 00774a90 / 00777310, broadphase insert 00774850 (shape vf2: Triangles 00844d30 here, the others the race ports'), 00819510 drive-on flags | `rx_collision_entity_load` |
| finalize | 00769030 (from 00556d65): the grids' extents from the keyed + static boxes (007739c0 / 00773b50), every grid rebuilt (00769370: 00882630 / 00882cf0 / re-insert) | `rx_physics_world_finalize` |

Render-only steps of the terrain are NOT built (construction section 3: render-only state is not core), their fields stay 0: the clip-map
mesh WaterMesh_Build 007e4280 (surface +0xe8), the shared projected-texture target of 007c9160, the terrain material's effect
parameters (PlatformHFTerrainSurface::vf15 007ccb50). Shader lookups (007b12a0 / 007b1330 and their inline forms) run and find
nothing while the shader assets are not built (unit 2g); the D3D resources the terrain creates go through the registry and rx_d3d.

### 10.2 Verification (three events)

Original dumps: baton boots (section 1's command) of NAT_Concord (`National,10,NAT_Concord,50,0,3,Laps,2,0`, 12 riders, AI 95: the
reference), FR_Alpine (`FreeRide,9,FR_Alpine,45,0,-1,,0,8`) and SX_Springfield (`Supercross,8,SX_Springfield,78,0,3,Laps,2,1`, 12 riders,
AI 95), scratchpad `rx_build/orig`, `rx_build2/orig_alpine`, `rx_build2/orig_sx`. Core: `build_world_accept.sh <out> <orig dir> <level>
<orig physworld> <orig terrain entity> [subscenes] [script]` (Concord with the asset script, the others with EngineDependencies + the level's
Shared / Base packages; FR_Alpine's collision from FR_AlpineBase + FR_AlpineFreeRide). Results:
- **Static objects** (positions, quaternions = 00401210 of the normalised entity matrix, property ids, shapes compared deeply: triangles,
  composite children / rotations / offsets / names / ids, boxes, cylinders, spheres): Concord 727 / 727 paired identical (the original
  has 728: one DropGate's static object, scene unit), Alpine 262 / 262, Springfield 111 / 111 (+ a DropGate and a LeaderBoard in the
  original). Broadphase boxes identical for every object, the static grid's extents (+0x430..+0x46f) bit identical, every cell list
  identical after the index mapping.
- **Physics world** (anchor on it): every field the constructor writes identical; the differences are race-time contents: listener /
  simulation counts and arrays (+0x1918 / +0x191c / +0x1944 / +0x1948, the vehicles), the object vectors' ends (+0x1964 generic,
  +0x19a4 keyed, +0x19c4 statics with the other units' objects), the grids' contents of the dynamic / keyed objects, +0x1934.. the
  contact-handler data (vf1's handlers + other units'), +0xcc9c the ForestSystem (forest unit, FUN_00772940).
- **Terrain**: the base height grid (16 MB Concord / Alpine, 4 MB Springfield) and the coarse bounds byte identical; the deformation
  surface identical to the boot's c-deform.bin (uniform 0x3f008081) on all three; dims, spacing, gain, register flags identical; the
  TerrainTracks rings' strips identical except the strips the race filled (tyre marks: ring +0x28 count and their point data). Differences
  explained: surface +4 (0xf0 -> 0x1a100: written every frame by ShadowMapRenderEventContainer::vf2 00795654 / 007946c1, REFLEX_WATCH),
  the projected texture's matrices (+0x110.., render frames), entity +0x24 / +0x78 / +0x18 (Entity::vf5, scene unit), the render-only
  fields of 10.1, the D3D objects (the boot's device stand-ins are not heap nodes: kind 2 in the dump), the handles' assets of the types
  without core loaders, the scene's terrain array / light list counts (other entities).
- **CollisionEntity** (anchors through the static objects: 264 Concord, 10 Alpine, 64 Springfield): identical except +0xc0..+0xef (the
  ModelInstance's copy of the matrix before the row normalisation: the original's rows differ by 1-3 ulps from the scene file's because
  Entity::vf5 sets the matrix through the entity's vf16; written by vf5's rep movsd, REFLEX_WATCH shows only 007f1263's init: scene
  unit), +0x18 the name (Entity::vf5), the static records' index word (shifted by the DropGate / LeaderBoard objects the core does not
  build) and, for shapes shared with the front end's entities, the shape's reference count (+1 in the original).
- **PhysicsProperties registry** (0x00da6ad0, G: anchors on its globals): IDENTICAL (materials, contacts, both maps' trees).
- MXWorld: the constructor's words identical; everything else is vf1 / vf3 / activity / camera frames (camera director lists, camera
  matrices, +0x21dc4.. particle effects of vf3, +0x21e10, +0x22148).

### 10.3 API for the next units (scene, vehicle, activity, forest)

```c
rx_addr w = rx_mxworld_construct(ReflexAlignedAlloc(0x22150), game + 0x39c0);   /* FUN_00759f00 step 1 (game + 0x5a54 = w after) */
rx_addr pw = rx_physics_world_create(scene);                                   /* FUN_0075da90 (needs [0xa95844] / [0xa946b8]) */
rx_mxworld_start(w, scene, game + 0x39c0, pw, 0x00ae1b18);                     /* MXWorld::vf1 */
/* scene unit, per entity after the factory's constructor and Entity::vf5: */
rx_hfterrain_entity_load(entity, rs);      /* HFTerrainEntity::vf5 tail (rs = the fmt_scene entity) */
rx_collision_entity_load(entity, rs);      /* CollisionEntity::vf5 tail: adds the static objects (vf28 = rx_collision_entity_add_to_world) */
/* other entities with static collision (DropGate, LeaderBoard, PhysicsEntity statics, props): */
rx_static_add_asset(pw, out_handle, matrix, drive_on, share, collis_handle, property_id);   /* 007693c0 */
rx_static_add_shape(pw, out_handle, matrix, shape_handle, drive_on, property_id);           /* 00769550 */
rx_phys_props_find(rx_phys_props_get(), "Terrain");   /* property ids (0x00da6ad0); rx_phys_props_contact_index for the contact table */
rx_physics_world_finalize(pw, forest_entity);         /* FUN_00769030 after the scene's objects are in (activity setup, 00556d65) */
```
The RSData helpers `rx_rs_asset_handle / rx_rs_has / rx_rs_enum / rx_rs_float` (build_terrain.h) are the original's FUN_0080ec70 /
0080e180 / 0080e4d0 / 0080e850 over fmt_scene: the scene unit can reuse them. `rx_asset_handle(type, name)` = Database::vf27 (find; the
creating branch aborts). TEMP constructors for the scene unit to replace: `rx_hfterrain_entity_construct_temp` (007c8cf0),
`rx_collision_entity_construct_temp` (00818f60 + 00401db0 without the model instance); build_world_test's `entity_base` stands in for
Entity::vf5 (scene, name, matrix written raw: see the +0xc0 note).

### 10.4 Notes for the lead

- The product arena needs the executable's **.rdata** (vtables: every virtual call reads them; string literals passed by address,
  e.g. 0x0093e6d0, 0x0096cc5c) and initialized **.data**, and the CRT-initialized runtime constants (0x00d61650 (1,1,1,1), 0x00d61670
  zero, 0x00d616a0 (0,1,0,0)) before construction: build_world_test loads the PE sections itself (STAND-IN for unit 12).
- Startup state the constructors expect: the script command registry [0x00d6925c] (static initializer 008181a0, provided as
  `rx_command_registry_init`; the original has 0x1d commands from startup), XenonCore at [0x00a95844] = [0x00a95960] (+8 Database, +0x14
  the vertex declaration cache, +0x1ec the D3D device), MXGame [0x00a946b8].
- rx_d3d.c is in build_cli.sh's oracle list (lead, 2026-10-19); the phys port's surface_lock / unlock (phys_terrain.c,
  phys_coll_terrain.c) still call the vtable entry with rx_call_original_ex: with rx_d3d objects in the product they should use
  `rx_d3d_call(tex, 13 / 14, ...)` (the registered core method) -- phys agent.
- build_world_test.sh compiles every game file (the construction calls other subsystems' ports) and leaves out a file that does not
  compile (another agent's work in progress); RX_WT_ALL_LOADERS=1 registers every core asset loader (default: only collis, unit 2c).

### 10.5 Left (units 3 / 5)

- MXWorld::vf3 007665f0 (particle effects: fx unit; the activity's tables), the rest of vf1's callees (camera FUN_0063dae0 via the
  scene, 00536ef0, 00674d90 audio) are other units' and run through rx_call_original.
- Kind-0 split-triangle geometry (FUN_00771130, never in the install's files), SplitTriangleCollisionAssetHandle shapes (00844ae0),
  kind-2 composite children (00844ea0), shape / static-object destruction paths (refcount 0: FUN_00773a60 / 00773e10): abort.
- The forest's collision (FUN_00772940 -> 00667cc0, world + 0xcc9c): forest unit. Statics of non-collision entities (DropGate,
  LeaderBoard, ...): scene unit through the API above.
- Render-only terrain parts (10.1) if the lead wants them for Unity (the clip-map mesh would be the first).
- Oracle per-call Verify of the registered functions (0075da90, 00768830, 0084a0d0, 00882370, 007731f0, 007ce6a0, 00849660, 00844d30,
  00819650) was not run: verification here is the construction compare above.

## 11. The D3D resource layer (core/src/rx_d3d.[ch], 2026-10-19)

Gameplay construction creates D3D9 resources whose CPU-visible contents the race reads (the terrain's deformation surface, the height
map's level 0, the tyre-track buffers). `rx_d3d` gives them game addresses: objects are 0x40-byte arena blocks, +0 a core vtable in the
reserved range 0x00e40000..0x00e47fff (device +0, texture +0x400, surface +0x500, vertex buffer +0x600, index buffer +0x700, vertex
declaration +0x800), the entries are method ids `RX_D3D_FN(iface, slot)` registered in the function registry with one signature
`uint32_t m(const uint32_t* a)` (a[0] = this), so `rx_d3d_call(obj, slot, n, args)` is the virtual call in both builds (oracle: the
emulator's device stand-ins, rx_call_original). Semantics follow the boot's recording device (boot_devices.inc): memory-backed levels
(pitch = width * bits / 8 rounded to 4, DXT block rows), LockRect returns bits + pitch (a rect offsets them), Clear of render target 0
fills R32F with red / 255 (0x80808080 -> 0x3f008081) and A8R8G8B8 / X8R8G8B8 with the color, GetRenderTargetData copies when format
and pitch agree, Get/SetRenderTarget / DepthStencilSurface keep the device's state, Set* / Begin* / End* / Draw* are accepted, other Get*
abort (not needed yet). API: `rx_d3d_init`, `rx_d3d_device_create`, `rx_d3d_call`, `rx_d3d_create_texture / _offscreen / _vb / _ib`,
`rx_d3d_bits(obj, level, &pitch)` (tools / other layers: the bits of a level). The game's own bookkeeping above it (the GPU resource
registry [0x00a95744], the vertex declaration cache of XenonCore + 0x14, shader-asset lookups) is `game/build_render.[ch]`.
For the fx unit (terrain deformation as CPU code) and the render-handle loader (texture assets: TextureAsset +0x10 -> a holder of the
IDirect3DTexture9; build_world_test's `texture_asset_r32f` shows the shape).

## 9. Asset loaders (units 2b-2g, 2026-10-19)

Files (`core/game`): `build_load.[ch]` (registration of the loaders with the asset system, Database access, MSVC std::vector helpers, the exe's
runtime vector constants, test scaffolding), `build_collis.[ch]`, `build_xml.[ch]` (XMLReader + the query virtuals + BXMLAssetIO),
`build_model.[ch]`, `build_perf.[ch]`, `build_packdata.[ch]`, `build_tdf.[ch]`, `build_tree.[ch]`, `build_terrain_asset.[ch]`. Tools (`core/tools`):
`build_load_check.c` (every asset of a type of the whole install through its loader, which must consume its stream exactly),
`build_load_test.sh <dir>` (builds `build_test` + `build_load_check` with every game file), `build_load_cmp.py` (build_cmp.py with pointer offsets
and word indices compared from the payload of 128-aligned blocks). Hooks in files of other units (one line each): `rx_asset_register_core_loaders`
(build_asset.c) calls `rx_load_register()`; `rx_database_create` calls `rx_load_set_database(db)` (the loaders look handles up in the engine's
Database, original `[[0x00a95844] + 8]`); `world_node_extern.h` calls the anim unit's `rx_nodeset_next_unvisited` instead of the original
(Node_EvaluateWorld is needed by the model loader and the product cannot run `rx_call_original`); `tools/build_test.c` calls
`rx_load_test_scaffold()` in the assets unit.
Runtime constants: the exe's static initializers write vectors the constructors copy (identity matrix 0x00d61600, 1-splat 0x00d61690, +-FLT_MAX
splats 0x00d615e0 / f0, zero vector 0x00d61670 ...); `rx_load_runtime_constants()` writes them when unset (values read from the reference world).

### 9.1 Results
Loaders done (every asset of the install of the type consumes its stream exactly; `build_load_check`, also with RX_BUILD_FILL=0xa5):
collis 2,598, bxml 4,749, model 12,466, perfdat 259, packdata 46, tdf 53, tree 163, terrain 8, trncol 8 (plus 1 / 14 / 67 / 0 / 1 / 1 assets of `SX_FresnoB` that the fmt reader cannot read:
its package is truncated). Compare (assets unit, NAT_Concord,
`rx_build3/run.sh` = build_test twice (fill 0 / 0xa5) + `build_load_cmp.py --leaf-pointers --orig orig/c-canon.bin orig/c-world.bin --core
core/assets-canon.bin core/assets-world.bin --undef core/assetsF-world.bin --cstr AnimAsset:14:20 --ignore BXMLAsset:3c,2c4,2d4 @core/assets-anchors.txt`):
the handles of collis (60), bxml (166), model (315), perfdat (259), packdata (2), tdf (1), tree (6) no longer show a missing subtree; their nodes
compare identical (34,760 node pairs walked from the Database: collis 1,438 nodes, bxml ~10,800, ...) except as listed in 9.2. Still missing in the
compare: texture 659, material 567, surface 316, ui component 151, script (Lua) 134, shader 114, sound 27, sound bank 24, localiz 9, texture atlas 8,
icon geometry 7, cell 6, tbldata 4, particle 2, particle attribute 2, adv 1, forest 1 (see 9.4).

### 9.2 What differs from the original and why
- **BXMLAsset +0x3c, +0x2c4, +0x2d4** (ignored in the compare): the reader's cursor pool (+0x2c4 free-slot counter, slots 18 / 19 of the second pool at
  +0x1d4 hold open cursors, +0x3c the current node) is used by whoever queries the document at run time; +0x2d4 is the compressed-stream wrapper's freed
  chunk buffer pointer (a stale address the original's heap reused for another block). `+0x2f4` (size of the last read through the wrapper,
  CompressedInputStream::vf2 0080fc10) IS reproduced: all 166 documents are zlib compressed.
- **ModelAsset**: 10 models have the second node set's dirty word (+0x84) and a record flag (bit 0, world matrix valid) set by Node_EvaluateWorld during the
  intro; the **material handle of a LOD binding** (Binding +0xc) of 49 gear models (MXRider_Boots_*, Helmets, Goggles, Chstprt, Hguards, tires, MX_*_Lite)
  is overwritten after loading by the vehicle / rider setup with the chosen colour variant (`MXRider_Boots_NoFear_White` where the file says
  `MXRider_Boots`): units 6 / 7. These show up as ~400 "pairing conflict" lines of the compare (a handle reached from two models). Everything else of the 315
  models (both node sets: records, local / world / inverse world matrices, bounds; LODs; bindings; handles) is identical.
- **PackedData / Collision / BXML** blocks over 64 KiB are leaves of the canonical dump: their heap back-pointer word compares as data (known, as for the
  anim key blocks).
- **TDFAsset**'s MemoryInputStream +0xc (one instance): a pointer the TDF consumer stores at run time.
- **Vertex declarations** (tree, terrain): the original's `CreateVertexDeclaration` stand-in object is a 32-byte process-heap block, `rx_d3d`'s is 0x40
  bytes (tree buffers' own objects compare equal): 29 differing nodes of the 6 trees, build2 / rx_d3d owns it.
- 128-aligned blocks (model node sets) have an address dependent pad inside their heap node: `build_load_cmp.py` compares from the payload.

### 9.3 Layouts and conventions
The headers carry the layouts: `build_collis.h` (CollisionAsset: +0x10 the geometry; kinds 22 / 15 / 2 / 11 / 16 / 17: 22 = groups of triangles with a
radius per group, 15 = composite of boxes (5) / spheres (7) / cylinders (8) with matrices, quaternions (Matrix3_ToQuaternion) and translations in
std::vectors, 2 = box list, 11 = oriented box with sorted extents and a shape class, 16 sphere, 17 cylinder), `build_xml.h` (XMLReader object, string
table, the query virtuals), `build_model.h` (ModelAsset, NodeSet), `build_perf.h`, `build_packdata.h`, `build_tdf.h`, `build_tree.h`, `build_terrain_asset.h`
(TerrainAsset: D3D index / vertex buffers through core/src/rx_d3d.h and the vertex declaration cache; TerrainCollisionAsset binary form 0xbada55). MSVC
std::vector: from empty a resize / reserve allocates exactly n elements, push_back grows capacity + capacity / 2 (at least size + 1); `allocator<char>` is
operator new (a raw malloc block), every other vector of the game goes through ReflexAlignedAlloc. XMLReader attributes are found by string INDEX
(entry +0 is an index into the string pointer table), child nodes by name pointer. Terrain and trncol exist only in the Supercross levels
(`SX_*_LD`), so the reference event has none: they are verified by exact consumption of all 8 assets each.

### 9.4 Not done (and why)
- **texture / material / shader / surface** (1,656 of the missing handles): their loaders create D3D objects through the engine's renderer and D3DX
  (`CreateVertexShader` / `CreatePixelShader` + `D3DXGetShaderConstantTable` for shaders, `D3DXLoadSurfaceFromSurface` for textures); rx_d3d has neither
  (needs device slots 91 / 106 and a D3DX constant table object like boot_devices.inc's, and 32-byte stand-in objects to compare against the original).
  Materials (BXML, FUN_007a1420 3,679 bytes / UI form FUN_007b1af0) look shader entries up in the shader asset. The compare of D3D objects also needs their
  vtable words normalised (the emulator's COM vtables are at 0x1000xxxx, rx_d3d's at 0x00e4xxxx).
- **(superseded by 9.6: forest, water, shader and texture are done)** **forest** (ForestAsset 0x24b10 bytes: XenonForestAsset_Parse 0065cd90 + ForestAsset_ParseTreeTypes 0065dd30 + ForestTreeType_BuildLodRecords 0065eb00;
  the race reads its ForestSystem data): it looks up the shader asset's entries by name and AddRefs them (`ShadowBBVertexShader`, `BBVertexShader`), so it
  needs the shader asset first. The tree assets it references (tree) are done. grass / frstmtl: none in the reference event.
- **tbldata** (4): `Database_LoadTables` fills the global table database [0x00a89cfc] (unit 12 globals); **script** (134): LuaAssetIO feeds the Lua VM
  (the Lua unit); **adv** (1): its reader is left reset by the cursor loop (cursor pool order); UI / audio / particle types (uicmpnt, sound, soundbnk,
  localiz, texatlas, icongeom, cell, particle, partattr): not core or the fx unit.
- Model versions 1 / 2 (`FUN_007d4f00`, `ModelAsset_LoadVariant`) are not in any file of the install and abort; collision kind 0 (`FUN_0081a1e0`) likewise; BXML
  flags 2 (external data) and 4 (big endian) likewise (no file has them); the XML form of TerrainCollisionAsset (`FUN_0041d350`) aborts (no file has it).

### 9.5 Additions by the rider unit (unit 7, 2026-10-19)
`build_xml.c` also has the XMLReader virtuals themselves, registered at their addresses and declared in `build_xml.h` (thiscall reader,
stack arguments): vf1 root, vf7 child, vf8 first child + cursor, vf9 release, vf10 next, vf40 node name, vf48 node vec3, vf51 / 52 / 54 /
57 node floats / ints / string / bools, vf58 attribute, vf68 attribute C string, vf70 / 71 vec4, vf73 vec3, vf76 / 77 attribute floats /
ints. Unlike the stateless queries above, they keep the original's cursor state exactly: the current node (+0x3c) and the cursor pool
(free stack +0x184, top +0x2c4, cursors {node, index, name} at +0x1d4 + 0xc * c). Callers that must leave a reader in the original's
state (the rider tree / IK readers, unit 6's 0043ac40 factory) call them through the vtable. Return values as the original: a node form
returns 1 for type 1 or the asked type, else 0. An attribute form returns 0 unless the type matches or is 1. vf48 returns 1 for type 0xa
(also n <= 0) and for type 1.

### 9.6 Render-side assets: shader, forest, water, texture (units 2f / 2g, 2026-10-19)
Files (`core/game`): `build_render_asset.[ch]` (ShaderAssetIO::vf3 00794780 / XenonCore::vf15 00843b80 / XenonShaderAsset::vf3 007b0fb0 + FUN_007b1040),
`build_render_asset_texture.c` (TextureAssetIO::vf3 00794810, XenonTextureAsset_Create 007b2820, FUN_007b26d0, the sdtor), `build_forest.[ch]`
(ForestAssetIO::vf3 00661f10, FUN_0065cbb0, XenonForestAsset_Parse 0065cd90, ForestAsset_ParseTreeTypes 0065dd30, ForestTreeType_BuildLodRecords
0065eb00), `build_water.[ch]` (WaterAssetIO::vf3 007e3ec0, FUN_007ede50, FUN_007ee0a0, XenonWaterAsset_Load 007f0320, the clipmap mesh WaterMesh_Build
007e4280 + 007e4570 / 007e4800 / 007e4d50, FUN_007e5330 (the valid-cell mask), FUN_007ee5a0 / 600 / 690 / 700 / 540, the shared ocean / wake /
refraction objects: Ocean_Create 007e3b90, FUN_007eba00 / 007ebd10 / 007edb00, the water set FUN_004242f0). Layouts: the headers. Registered in
build_load.c (shader 0x00971bec, texture 0x00971c24, forest 0x0093a0c0, water 0x00972a88). `src/rx_d3d.[ch]` gained vertex / pixel shaders (device slots 91 /
106), ID3DXConstantTable objects (`rx_d3dx_get_shader_constant_table` = D3DXGetShaderConstantTable, the CTAB comment of the token stream; GetBufferPointer /
Size, GetDesc, GetConstant(ByName), GetConstantDesc, GetSamplerIndex, the Set* accepted) and cube textures (slot 25; GetCubeMapSurface, LockRect(face, level)).

**Results.** `tools/build_render_check.c` (one level per process: EngineDependencies + the level's database with all packages, the loaders wrapped to
require exact stream consumption): forest 23 levels x 1 asset (NAT_Concord 6 types / 2,509 trees, FR_Alpine 13 / 12,048, TRX_Verde ...), water 21
assets on 19 levels (versions 5), shader 29..118 per level, all consumed exactly; textures: `build_load_check texture <db>`: 203 FR_Alpine, 639 NAT_Concord,
662 TRX_Verde assets built (RAM ~0.3-0.5 GB per level: `rx_asset_set_loader(0x00971c24, NULL)` drops the texture loader if the client does not want them).
Two files leave bytes unread in the original's way (FUN_007b26d0 advances by the destination size, not the bytes it read): TRX_Verde RedYuccaStalk_Brown
(known bad level table), MXUI DemoSplash (a level pitch 20 for a 24 byte row). Canonical compare (`tools/build_render_cmp.py` = build_load_cmp.py plus the
D3D stand-ins: an original 16 / 32-byte heap object whose word 0 points to a vtable block of the right slot count == a core object with an rx_d3d
vtable; counted, not compared): driver `tools/build_render_test.c` (the assets unit of build_test.c with the exe's .rdata / .data, the renderer stand-in
incl. display 1280 x 720 / texture quality 2 / no refraction, H: anchors on the new types' handles; build_test.c has no .rdata so the tbldata loader
cannot run there).
* NAT_Concord reference script (4,606 asset loads): forest, shader (114), texture (659) handles no longer missing; their objects identical (forest:
  all 0x24b10 bytes and the 2,509 tree records incl. sin / cos, the type constants and LOD records; textures: the 0x24-byte asset; shaders: the 0x894-byte
  asset) except: the D3D objects (959 + 662 stand-ins counted), the instance vertex buffer pointer of the forest (+0x24adc: neither side has a node for it,
  `--ignore XenonForestAsset:24adc`) and the tree array block's allocator back pointer (>= 64 KiB blocks are leaves of the dump: known). Differing nodes 865 -> 859
  (the same set as in 9.2 / 14.4: texture / surface / material handle +0x4c loading byte, anim pose caches ...); missing subtrees: texture 659 -> 0.
* FR_Alpine (`tools/` script of the reference script with the level packages; anchors on the forest / shader / water handles + G: on [0xa9578c] [0xa95790]
  [0xa95794]): 1,569 pairs: forest (13 types, 12,048 trees) identical; water asset: surface, mesh (0xf0, incl. the level records' scales, primitive
  tables, valid-cell mask), wake, refraction object identical except RUNTIME words: surface +4 (0xf0 -> 0x1a100, the same word as the terrain's, written by the
  shadow render events), +0x488 (frame stamp), mesh +0xd8 / +0xe0 / +0xe4 (draw), the levels' centres / ring counts (+8 .. +0x20, WaterClipmap_Update),
  wake +0x30 (frame stamp) / +0x3b4 / +0x3b8, ocean +0x14014 / +0x14018 (frame stamp, time) and the ocean's spectrum arrays (not built: see below). One shader
  handle has the loading byte set (a runtime stamp).
* Scene driver (`tools/build_scene_test`, my copy in rx_build9 adds only a weak stub for the fx unit): the ForestSystem entity and its per-type lists are now
  identical (the ignores for +0x3070 .. +0x31c0 are gone); the only forest difference is the instance storage block (+0x31bc: the cull thread's lists, empty
  in Alpine); WaterEntity identical. `build_scene_test.c` now sets the renderer stand-in's refraction byte (decls + 0x50) and display (texture quality 2),
  which the water and texture loaders read.
* Per-call Verify: `oracle/build_forest.cpp` (group rx_build_forest) rx_physics_world_install_forest 00772940: NAT_Concord 2 calls, FR_Alpine 2, 0 mismatches. The
  function makes the 4-byte list block of FUN_007d25c0 (ReflexAlignedAlloc(4), grown by 1 per forest entity) and keeps it alive while 00667cc0 makes the shapes:
  without it every block allocated there sat 0x20 lower than the original's (the finalize step then differed in every heap pointer).

**Differences by design / left.**
* The ocean's Phillips spectrum (Ocean_InitSpectrum 007ea9e0, x87 over the CRT rand() stream) is not built: the ocean block's arrays stay 0 (only the renderer
  reads them; the original's depends on the run's rand state).
* The refraction object builds only the capability-0 case (as the PC build); the texture loader aborts for the quality reduction of the top mips (renderer + 0x64 < 2
  with [0x00a956fa]), versions below 4, the dynamic class 0x200, flags 8 / 0x10 (no file has them). The mip regeneration D3DXLoadSurfaceFromSurface is a no-op
  (the reference boot's stand-in does not copy).
* The x87 parts (LOD thresholds 1 / n, the first leaf transition of FUN_0065eb00, the mask's divisions) are computed in double (the game threads' 53-bit
  precision): equal on every asset checked; a 64-bit loader thread would need rx_x87.
* material (567 handles), surface (316) and the UI material form are NOT built: nothing in the race reads them (WaterSurface_LoadMaterial 007ee240, the
  terrain's effect parameters and the models only keep handles); WaterSurface_LoadMaterial needs the XenonMaterialAsset (FUN_007a1420) and is the scene
  unit's render step (surface +0x58 .. +0x88 and the constant-table handle arrays stay 0: the "missing" water fields of the compare). The water, forest and
  shader sdtors (package unload of a level) are not ported.

## 12. Implemented and verified: the table database (unit 12a, 2026-10-19)

Files: `game/build_tables.h` (layouts, API), `game/build_tables_rdb.c` (the Rdb core: the MSVC 9 tree / vector / std::string templates
of the game written once over a node descriptor, tables, columns, cells, indexes, copies, Database_RestorePlayerDefaults, the row
builder and add-row, VehicleList_AddRow / RiderList_AddRow), `game/build_tables_load.c` (Database_LoadTables 004ef060 with its column
004ef340 / row 004ef8f0 readers, TableDataAssetIO::vf3 004f0170 + FUN_004eefa0, XMLReader vf4 / vf5 / vf80), `game/build_tables_init.c`
(the exe's static initializers 00903b20 .. 00904160 of every table object in .data, _initterm order), `game/build_tables_schema.c` +
`game/build_tables_schemas.inc` (the static tables' columns and the two experience tables' rows: FUN_00756760's schema calls),
`game/build_tables_startup.c` (FUN_004e6280 of MXGame::Startup, MXRegistry_Load 004e6c30, FUN_004ef2b0, FUN_0053c410's table branch).
Driver `tools/build_tables_test.[c|sh]`; compare tools in scratchpad `rx_build12` (`tdump.py` cell values, `trees.py` every tree's
shape / colors / contents, `sdump.py` static schemas, `cells.py`) besides `build_cmp.py`. Paths the reference boot does not execute
(boot coverage `rx_build/orig/boot-cov.bin`) abort naming their address.

### 12.1 The objects
- Collections (Rdb::TableCollection 0x1f4, .data): the default database 0x00ae0910 ([0x00a89cfc] while MXTables load), the
  MXPlayerDatabases 0x00ae10f0 + 0x1f4 k, MX07Leaderboards 0x00ae0268, PlaylistDatabase 0x00adfde8, the MXMemoryCollections 0x00ae0b80
  (the 10 activity / registry tables) and 0x00ae0070 (the 16 network tables). Layout in build_tables.h.
- 26 static tables (Rdb::Table subclasses 0x78 in .data: Vehicle / Rider / Player List, Activity Lists, Activity Results, Registry /
  Network tables); 92 loaded tables per the default database, 24 Player* copies per player database.
- Columns 0x50 (type 1 int, 2 float, 4 string, 6 int64), cells in a std::vector, a searchable index (map<key, set<RowRef>>) and a
  unique index (map<key, RowRef>); set_int / set_string keep both, including the original's quirk that the unique index erases the old
  value's entry whichever row it names. Copies (004ec770) rebuild indexes first-row-wins.

### 12.2 Sequence (the reference boot)
1. `rx_tbl_static_init()` (static initializers), 2. `rx_tbl_create_schemas()` (FUN_00756760: 00756887 .. 00756936),
3. `rx_tbl_mxgame_startup_tables(in)` (FUN_004e6280), 4. the asset Database with `rx_tbl_register_loader()` (also called by
build_load's rx_load_register), 5. `rx_registry_startup(in)` (12.4) and `rx_tbl_mxregistry_load(in, db, registry)`, 6. `rx_tbl_load_dlc_tables(in, db, names, n)`
(MXTables_DLC003 .. 005). Rows of the runtime tables: `rx_tbl_add_row` / `rx_tbl_vehicle_list_add_row` / `rx_tbl_rider_list_add_row`
with the activity's values (activity unit), `rx_tbl_set_int` / `rx_tbl_set_string` for cells.

### 12.3 Verification (NAT_Concord c-canon; FR_Alpine and SX_Springfield the same)
- `build_cmp.py --leaf-pointers` from G: anchors on every pointer word of 0x00adfde8 .. 0x00ae1c60: 65,195 of 65,273 node pairs
  identical; every difference is a later write by other code: ControllerRemapping (2 rows, load count 4, and its registration into
  player database 0 by 005b9340, input unit: that insert recolors two nodes of that database's name map and rotates it: the pairing
  conflicts), the PlayerRecords row of player database 0 (race finish), the rows of Player List / Activity Lists / Vehicle / Rider List
  (activity: their cell vectors are the 83 "missing" subtrees, their index sizes the 15 differing words). The 0x11cab node is the big
  string column of VehiclePart (a leaf of the dump: its string buffers are not dump nodes; values checked by tdump.py).
- Every cell of the 92 + 4 x 24 tables, of the leaderboard / playlist tables and of the static tables identical (tdump.py / cells.py);
  1,127 trees identical in shape, colors and contents except the ones above (trees.py).
- Add-row: the reference world's 12 Vehicle List / Rider List rows replayed (RX_TBL_TEST_ROWS=1): cells and both indexes identical;
  the load count differs by the activity's two row clears.
- Globals: MXRegistry_Load's AI settings 0x00ae1b8c .. 0x00ae1ba4, the collection counts, [0x00d72a34] as the original.
- Canonical-dump caveat: the original's static-initializer blocks (0x10004xxx, before the dump's heap) are named with a 0 payload
  offset, the core's with 0x10: G: anchors on them report "offsets differ" (values compared by the scripts).

### 12.4 The engine Registry (Engine::Startup)
Files `game/build_registry.[ch]`: XMLBuilder (0x2aac editable DOM: interned-string table, cursor pools, nodes 0x24, attributes 0xc;
00874300 / 00874530 / 0087c000 and the virtuals the boot and the race frames call), the Registry (0x4c: documents, the node -> record
hash, the changed list; 00823bd0 / 00823e90 / 00823f70 / 008240c0 / 00824140 and vf1 / 2 / 3 / 6..9 / 14 / 17..20 / 30 / 35, every
one registered; the race frames call vf20 / vf17 / vf3). `rx_registry_startup(in)` = Engine::Startup's part (the "Registry" document,
then every <Registry> of Engine.bxml: MXRegistry / RendererRegistry / UIRegistry.bxml); its result is the engine's +0xc (the caller
stores it: Engine vf20). MXRegistry_Load now reads "MXGame/Tables" through it (`rx_tbl_mxregistry_load(in, db, registry)`).
Verification (all three reference worlds, anchor A:107e6120 = the original's [[0x00a95844] + 0xc], `--ignore Registry:24`): 1,593 of
1,597 node pairs identical (the four documents' DOMs: every node, value, attribute, interned string and the string tables; the
entries, records and record blocks); the four differences are later writes: two documents' current node (+0x2a10: the last query of
later code: Renderer "TargetRenderTimeMicroSeconds", MXRegistry "Tables" here), Renderer\Profile\FPS (written at run time through
vf30: 59.99 in the original) and the compressed-stream wrapper's stale chunk pointer (+0x2a88: freed, reused by the original's heap;
as BXMLAsset +0x2d4). The node -> record hash is keyed by node addresses, so its bucket layout depends on the heap's addresses: checked
by content instead (scratchpad rx_build12/reghash.py: 505 buckets, 319 entries, the same path -> record mapping for all 315 paths).
Values read from string nodes (Registry vf17 / vf19 / vf8 on string values) are parsed with the host's atof / strtol where the
original calls msvcr90's: not compared per call (an oracle check of the readers would settle it).

### 12.5 Not done / for the lead
- FUN_0053c410's DLC finder (the glob over the install, 006d3f40) and its MXUI branch (UI); the DLC names are the caller's list.
- Database load requests / serials of these loads are the asset system's (build_test's script accounting).
- Not in the reference boot (abort): column types 0 / 5, float indexes, sorted row sets, dotted criteria, Database_ClearRows of a
  loaded database, tables without <Columns>.

### 12.6 What remains of unit 12 (globals + startup), for scoping
- The exe's static initializers: 990 entries in the _initterm table 0x009166e8 .. 0x00917660, all run by the boot; 12a ported the 36
  of the table objects (00903b20 .. 00904160). The others build: runtime vector constants (0x00d615c0 .. 0x00d616c0: identity matrix,
  splats; build_load.c's `rx_load_runtime_constants` is a stand-in from the reference world), string / std::map / critical-section
  globals of every subsystem, the AssetIO statics (build_asset.c already writes those), class statics (vtables of global objects).
  A worklist: the table above, with each initializer's coverage and the .data it writes (xref of its stores).
- Engine::Startup (0084e320) besides the Registry: GetTickCount fields, the input names / key bindings (InputNames_Init 00812170,
  KeyBind_Core.bxml / KeyBind_App.bxml through FUN_00812ea0 into 0x00d92f50), the Database (asset system: done), FUN_00804070, the
  engine timing fields (+0x48 DeltaSecondsClamp from the Registry, +0x60.. QueryPerformanceCounter), the renderer / debug overlay
  objects (FUN_00822330, [0x00a95948]), the task thread and events (not core), [0x00d6daf0] 0x80000 scratch block.
- MXGame::Startup (00756e10) / FUN_00756760 / FUN_0075ed60 / the game init thread 00760400: the rest of their calls (ClipRecorder,
  Input_LoadPresets, MudSettings_Load, the activity table rows, the startup tables 0065c200 / 0065c070 / 006b64a0 / 006b68e0 named in
  section 3), each owned by the unit of the system it builds.
- Controller key maps: ControllerRemapping (005b9340: its table in 0x00ae0910 + player database 0, 2 rows) and Controller_BuildKeyMaps
  00547f30 (the KeyMaps the input update reads; input-settings.md).
- Math tables built at startup (sin / acos tables, double precision: find them by the frame code's table reads).
- FUN_0053c410's DLC finder (the glob over the install's Database folder, 006d3f40) and its MXUI branch (UI); FUN_0053cbd0 (Rider
  database: rider unit).

### 12.7 Additions by the activity unit (unit 8, 2026-10-19; lead-approved edits of build_tables_rdb.c / build_tables.h)
- New, registered: `rx_tbl_erase_row` 004ece50 (with the column erases 004f8140 int / 004f8430 string and the row-set renumbering
  004ec180), `rx_tbl_set_cell_int` Table_SetCellInt 004f6e60, `rx_tbl_query` 004eddc0 (the unique-index / searchable-index / scan /
  filter paths; the unique path assigns t1 = t2 then out = t1 as the original), `rx_tbl_delete_rows` 004ecf00, `rx_tbl_select`
  004ed2f0, `rx_tbl_find_first` 004ed410, `rx_tbl_crit_init_string` 004f6c60 and the row-set helpers (004dc570 construct, 004f3ba0
  copy, 004ebf80 intersect, 004eb9f0 cell match: int ==, !=, <, <=, >, >=, string == by _stricmp; 004eb920 parent ref).
- Dotted criteria ("Col.ParentCol", one dot) are now resolved (12.5 listed them as aborts).
- Fixes of 12a's code found by unit 8's per-call Verify (same file; 12a's table test still compares the same): an index node's
  ~RowSet zeroes its +0x24 before the destroy (rowset_destroy_member), the std::map operator[] temporaries of set_int / set_string /
  append (004fc440 / 004fbea0 / 004fb980: one allocation and free each, in that order: idx_index_at) and the exact frames (TF_PLAIN /
  TF_EBP / TF_CALL) of the functions whose stack temporaries reach the heap (the std::set / vector proxies): add_row, set_int,
  set_string, append, the list adds (Vehicle List sub 0xe0, Rider List 0xdc), crit_resolve, query, delete, select.
- `tools/build_tables_test.sh` re-run with and without these additions: identical canonical dumps (65,218 pairs, 65,124 identical,
  94 differing; the 65,273 / 65,195 / 78 of 12.3 drifted with other units' changes in the shared tree, not with these).

## 13. Implemented and verified: the riders (unit 7, 2026-10-19)

Files (`core/game`, about 4,300 lines, about 156 transliterated functions; layouts and conventions in `build_rider.h`):
`build_rider_entry.c` (the gear loader, rider entries, parts and their package requests), `build_rider_spawn.c` (0053e920 / 0053eab0
and their pieces), `build_rider_ctor.c` (RiderAnimation 00618180, AnimationEntity 0082d040, Character 0082df30, CharacterEntity
00839a40 and the element constructors), `build_rider_tree.c` (the animation tree from the BXML "<vehicle>_RavenRiderAnimationTree":
animator factories and constructors for blend / additive / motion / switch / performance / featherstone / IK / expression /
procedural, the walkers, the bone cache), `build_rider_model.c` (the model copy 007d4d60 with its node sets 007ddb40, the instance
0041c700 / 006b5de0), `build_rider_init.c` (RiderAnimation setup 00619490: tracks, mirror table, wreck checker, event slots, lean
animations, the worker objects), `build_rider_plugs.c` (the plugs and IK solvers on the vehicle 0061ad70), `build_rider_ik.c` (IK /
ragdoll data 004246c0 from "MXWreckingRagdoll", rigid bodies and pins, the 16 collision bodies in the physics world 00429700 / 00772760),
`build_rider_gear.c` (VehicleLoader_Assemble 0053ddc0, OverrideMaterials 0053d8b0, MaterialKey, the LOD-threshold helpers, the
model's LOD vector: copy / grow / destroy), `build_rider_clip.c` (ClipRecorder_Init 006aa660 / AllocBuffer 006abc40: the sample ring
[0x00a7f124], 0x5b68 x 0x50, and the four stunt event handlers), `build_rider_util.h` (internal helpers). Oracle adapters:
`core/oracle/build_rider.cpp` (group `rx_build_rider`). Additions to other units' files, agreed with the lead: XMLReader virtuals in
build_xml.c (9.5), Database vf4 / vf8 / vf15 / vf26 / vf27 in build_asset.c (8.8).

### 13.1 What is built, in the original's order
1. Game init (0075ed60, unit 12 calls these): `rx_gear_loader_construct` (0053cbd0: the "Rider" database and package, the
   MXRider_Hierarchy handle at [0x00d92ef4] + 4, the gear package cache at + 8) and `rx_clip_init` (006aa660).
2. Per Rider List row (MXPlayer set-rider 0054a3f0, the activity's): `rx_rider_entry_create(mgr, id, hr)` (0053e8c0). The entry comes
   from the row (RiderId, RiderOwner, OwnerRiderId, OwnerRiderGearSetId). The owner's tables (0053d720): a player's
   PlayerRider / PlayerRiderGear (every row), AI's AIRider / RiderGearSetGear (the gear set's rows through the non-unique index), or
   owners 5 / 6 with static tables. Each RiderGear row becomes a part (shared between entries by RiderGearId, reference counted), with
   its slot from RiderGearType.FactoryName (004e74c0 / 004dcf30) and its two packages (material, model) and databases from the
   MaterialPackageDatabaseId / ModelPackageDatabaseId references. The packages are added to the cache with their "HR_" twins
   (0053cf30) and requested (0053cef0 / 0053d0d0). This is the requesting code of the per-rider gear databases and packages of 8.4.
3. Per race rider (MXPlayer 0054a320): `rx_rider_spawn(id, mgr, world, vehicle, cb)` (0053e920). It does a RiderAnimation
   (0x5c4f0, _aligned_malloc under the allocator lock), then the setup 00619490 with the tree of the vehicle type (0053cd40). Next the
   gear on the model copy (0053dd20 -> VehicleLoader_Assemble: the pending parts swapped in, every part's materials over the model
   materials (0053d8b0), the LOD thresholds of all parts' models as LOD records of the rider's model, each part node listed in its LOD
   records with the node's bounds grown over the part's). Then +0x290 = 900 (0053c7d0), vehicle +0x1a0 = the rider, the callback's
   vf0, the scene serial + SceneManager_AddEntity, and the entry's slot in the manager's spawned list.
4. Per intro / podium rider (0074baed): `rx_rider_spawn_character(mgr, world, id)` (0053eab0): RiderCharacter 0xd30 (00624750),
   BindMud 00624990 (model copy, CharacterAnim_Init of the scene unit, the instance, three script float slots), the gear (0053dd70),
   the scene.
5. Gear changes: `rx_rider_entry_set_pending(out, entry, mgr, part)` (0053efa0); the swap happens in the next Assemble.

### 13.2 Verification (Verify boots, REFLEX_PORT_VERIFY=1, baton; calls / mismatches)
Each adapter checks one call as a whole (everything under it is the core's). Inner adapters only count when their caller runs as the
original, so they were run in separate boots by name (rx_VehicleLoaderAssemble, ...).

| function | NAT_Concord (12 riders) | SX_Springfield (12) | FR_Alpine (1) |
|---|---|---|---|
| 00618180 RiderAnimation ctor | 12 / 0 | 12 / 0 | 1 / 0 |
| 0082d040 AnimationEntity ctor | 12 / 0 | | |
| 00839a40 CharacterEntity ctor (also the scene's entities) | 254 / 0 | 400 / 0 | 25 / 0 |
| 007d4d60 ModelAsset copy | 254 / 0 | 412 / 0 | 26 / 0 |
| 0053cd40 tree handle | 12 / 0 | 12 / 0 | 1 / 0 |
| 0053ddc0 VehicleLoader_Assemble | 24 / 0 | 24 / 0 | 2 / 0 |
| 0053d8b0 OverrideMaterials | 240 / 0 | | |
| 00624990 RiderCharacter_BindMud | 12 / 0 | 12 / 0 | 1 / 0 |
| 0053e8c0 rider entry | 12 / 0 | 12 / 0 | 1 / 0 |
| 004e74c0 gear type slot | 49 / 0 | | |
| 006aa660 ClipRecorder_Init | 1 / 0 | 1 / 0 | 1 / 0 |
| 0053cbd0 FactoryGearLoader | unverifiable (waits on the loader thread) | same | same |
| 00619490 RiderAnimation setup | 12 / 12 dead stack (a) | 12 / 12 (a) | 1 / 1 (a) |
| 0053e920 race rider | 12 / 12 (a)(b) | 12 / 12 (a)(b) | 1 / 1 (a)(b) |
| 0053eab0 intro rider | 12 / 12 (b) | 12 / 12 (b) | 1 / 1 (b) |

Differences, all dead stack. The original copies uninitialised words of its frames into the heap; the product cannot have the
original's stale stack:
- (a) The physics world's dynamic-object records (0xe0, vector at physics world + 0x1954) that 00772760 builds on its frame: +0xb0..+0xcf
  (6 words) hold the frame's old contents (the same class as unit 6's vehicle bodies, build_vehicle_proxy.c). The race overwrites them:
  at scene tick 1500 the live records are identical. Only the freed old vector buffer keeps them.
- (b) VehicleLoader_Assemble stores the grown bounds of a node (centre +0, half extents +0x10) as two 16-byte vectors whose w words are
  frame locals it never writes (+0x7c / +0x4c of its frame). The core puts its frame at the original's stack addresses, which makes
  Assemble verified on its own (24 / 0). Inside 0053e920 / 0053eab0 the earlier ports leave other words there, so 2 x 2 words per rider
  model differ. Nothing reads them in 1,100 race frames (below).
- Worker / init thread stacks (0x12xxxxxx) hold the original's dead frames.

The strongest check: the race frames run on the core's riders. Memory images were taken (oracle/build_vehicle_trace.cpp
REFLEX_VTRACE_SNAP) after the 12th race rider, after the 12th intro rider and at scene tick 1500 (REFLEX_AFTER=1200: the race-frame
animation of the rx_anim group runs on the riders for 1,100 frames). The boot with `rx_anim,rx_build_rider` On was compared against the
boot with `rx_anim` On only. All three images differ only in (a), (b) and thread stacks: 229 / 229 / 133 words, every one in those classes.
The event starts and runs at the same frame counts. After the first spawn I also checked the dynamic-object vector against the
original (unit 6's grow path): 17 records, capacity 22, equal except (a).

### 13.3 What differs from the original by design
- Database vf4 / vf15 (gear loader): served synchronously in the product (8.8).
- GetTickCount calls of Assemble (results unused) are not made. The `_strnicmp` pairs of OverrideMaterials over the keys (results
  unused) are not made either.
- OS objects: critical sections are no-ops in the product (one thread). The worker thread / events of RiderAnimation (00619490) are
  product placeholders (build_rider_init.c).
- A part package with more than 6 materials, LOD vector assignment over existing records (00544ff0), the truck rider setup (0061aa10)
  and the transition-table / null-bone tree forms: not in the install's riders, abort with a message.

### 13.4 API for the activity unit (stable)
```c
rx_gear_loader_construct(U32(0, 0x00d92ef4));          /* game init: after Database + the gear loader's allocation */
rx_clip_init();                                        /* game init (0075ed60) */
rx_rider_entry_create(G + 0x15504, rider_id, hr);      /* per Rider List row (set-rider 0054a3f0); hr = high-resolution gear */
/* ... the gear packages load (unit 6's package cache, PackageCache_Update) ... */
rx_addr ra = rx_rider_spawn(rider_id, G + 0x15504, mxworld, vehicle, callback);   /* race rider on unit 6's vehicle (0054a320) */
rx_addr rc = rx_rider_spawn_character(G + 0x15504, mxworld, rider_id);            /* intro / podium rider (0074baed) */
rx_rider_entry_set_pending(&old_id, entry, G + 0x15504, part);                     /* a gear change (0053efa0) */
```
Needs: the Rider List rows (rx_tbl_rider_list_add_row), the vehicle of unit 6 (+0x194 type, +0x198, +0x18c id, +0x1328 its model
instance, the plugs' targets), the MXWorld with scene (+0x20138) and physics world (+0x20134), the Database with the Rider and gear
packages loaded. Shared helpers exported for unit 6's parts assembly 00541310: build_rider.h "the gear helpers".

### 13.5 Left
- Product run (canonical dump compare): needs unit 6's package cache (00543830 / 00543b30 / 00543c30 / 00543990 / 00543cd0 / 00543db0 /
  005436d0) and Rdb reference reader 004ec210. They are called through the registry (the original in the oracle) and are unported, so
  the product aborts there. It also needs the vehicle and the activity's driver. The core functions are product code (no oracle-only
  paths except the OS objects and Database vf4 / vf15).
- The intro rider's (b) words and 00619490's frame placement: deeper stack emulation would make 0053e920 / 0053eab0 verify clean; not
  worth it for dead words.
- Not reached by these events: 0053efa0 (no gear change), the pending-part swap of Assemble (0053ca70 is ported, unit 6's 00543cd0),
  vehicle types 2..7 (tree handles are ported, the truck setup 0061aa10 is not).

## 14. Implemented and verified: the scene (unit 4, 2026-10-19)

The entity factory FUN_004046a0 and the entity classes a level's scene creates, plus the attachment of the entities to the scene manager.
Files (`core/game`): `build_scene.h` (public API), `build_scene_int.h` (internal: the RSData readers, class creators), `build_scene.c` (the reader,
the factory and type table, the subscene loader, SceneManager_AddEntity), `build_scene_simple.c` (Anchor, GeneralStorage, DistanceFog, ShadowMap,
HDRSettings, OverlayColor, MudManager, TDF, LevelInfo, MusicTrackList, ReflectionMapGen, Sky, LensFlare), `build_scene_model.c` (ModelInstance,
ModelEntity, MassiveAd, HFBump, CollisionEntity, Model_Bounds), `build_scene_trigger.c` (MXTrigger, LevelConfig), `build_scene_anim.c` (the character
props: AnimEntitySingleAnimLoop / SplineAnimLoop / ScenarioAnim / InstancedAnimationEntity, CharacterAnim_Init / SetAnimation), `build_scene_props.c`
(PhysicsEntity and its keyed physics bodies, PopUp / PopDown bales, DropGate, HFTerrainEntity, the event-handler registration),
`build_scene_chars.c` (CrewChief, TrackSideFlagHolder, ScoreTowerFlagHolder, ThirtySecondBoard), `build_scene_forest.c` (ForestSystem and the
world's forest collision), `build_scene_camera.c` (MXCameraPath), `build_scene_start.c` (the StartEntity family, AIAvoidanceSphere),
`build_scene_activity.c` (ChallengesSpec, the collectables, EggHuntEgg, StepUpUpright), `build_scene_misc.c` (ScrollingTexture, CameraFacing, Water),
`build_scene_lights.c`, `build_scene_vf.c` (141 constant virtuals: type ids and similar, generated from the RTTI). Tools (`core/tools`):
`build_scene_test.c` / `.sh` (the driver), `build_scene_cmp.py` (build_load_cmp.py printing every differing word of a node). Edits in files of
other units (allowed by the lead): `build_model.c/h` (rx_nodeset_clone(flag), rx_model_update_subtree_bounds exported). 234 functions registered.

### 14.1 The flow (what the original does, in the core)
`rx_scene_load_subscene(scene, fmt_scene, subscene, name)` = FUN_007d1dc0 .. 007d1fb0: the subscene record of the scene manager (scene + 0x38 + 0x110 k:
name, array +0x104, count +0x108, used +0x10c), per Entity node `rx_scene_entity_from_rsdata` (FUN_0080df90: type id of the RSData namespace
FUN_0080de70 -> `rx_scene_create_entity` = FUN_004046a0 -> +0x7c isUiNode, +0x10 scene, +0x14 entityId -> vf5), then vf7 of all, then
`rx_scene_add_entity` (FUN_007d2f70: link into the sorted lists by +0x78 (sort kind +0x70 != 0; key from the counter 0x00a95838, step 0.1) or the
unsorted list, name registry, the render container's Lights_Add for an IRenderable (vf25), the tick list, the type-14 entity, vf1). The XMLReader of
the original is the fmt reader: `rx_scene_rs()` is DAT_00a9583c; the RSData helpers FUN_0080e180 .. 0080f370 are the `rs_*` functions (host
fmt readers answering what the helpers answer, including the defaults). Unknown namespace: an empty slot, as the original.
Extension point: `rx_scene_register_class(id, create)` for classes other units build (cameras, FX, audio, riders).

### 14.2 Classes
Built (type id: class; all verified in the Concord compare unless noted): 3 / 4 Model, 5 Collision, 0x10 HFTerrain, 0x11 HFBump, 0xc Sky,
0xe DistanceFog, 0x1e GeneralStorage, 0x1f ReflectionMapGen, 0x101a Anchor, 0x1028 / 0x1042 / 0x1043 / 0x107b / 0x1071 MXTrigger (one class, the
RSData differ only in allocation size), 0x1029 / 0x106f PhysicsEntity, 0x1031 / 0x1032 PopUp / PopDown bales, 0x1033 DropGate, 0x104a MassiveAd,
0x104d MudManager, 0x104f ScenarioAnim, 0x1050 / 0x1052 SingleAnimLoop / SplineAnimLoop, 0x1051 InstancedAnimation, 0x1061 LensFlare, 0x1063
OverlayColor, 0x1064 HDRSettings, 0x1065 ShadowMap, 0x1076 TDF, 0x107a LevelInfo, 0x107c MusicTrackList, 0x103b CrewChief, 0x1038 / 0x1039
flag holders, 0x103a ThirtySecondBoard, 0x104b MXCameraPath, the StartEntity family (0x1010 .. 0x1018, 0x101b .. 0x1026, 0x103c .. 0x103f), 0x1073
AIAvoidanceSphere, 0x1019 ChallengesSpec, 0x106c / 0x106d / 0x106e collectables, 0x1044 EggHuntEgg, 0x1070 StepUpUpright, 0x1078 ScrollingTexture,
0x1079 CameraFacing, **0x1041 BillboardCrowd** (build_scene_crowd.c, 2026-10-27, w-npcs: the stands crowd sections, formerly listed under "render, outside the core"); the four replay cameras 0x1046 .. 0x1049 (MXReplayTower / ZipLine / Spline / Animated; core/game/loop_cam_replay.c, QUEUE.md w-tvcam); the FX construction unit (2026-10-20, `scene_fx_entities.c` / `fx_worldparticles.c` / `build_particle.c` / `scene_viewports.c`,
notes/fx.md "The particle asset and the scene FX entities"): 0x102a PlaceableParticleEffect (006964d0 + emitter 00694110, vf5 00696670, vf7
00696810, vf9 00696870, tick 00696940), 0x102e FireworksParticleEffect (00692140; vf5 006921c0, vf7 00692240, tick 00692340), 0x102f
TriggeredPlaceableParticleEffect (006923c0; vf5 = the base's, vf7 00692450, tick 00692520), 0x1030 ParticleEntity / ParticleManager
(00695c60; Load 00695de0 with the two 00697ce0 managers, vf7 00695da0, vf9 00696040, tick 00696100), 0x105e / 0x1088 AudioVolumeMixEntity
(0067d380; vf5 0067d4d0, tick 0067d7f0 -> 0067d810) and 0x107e StatusBoardEntity (00413d80; vf5 / vf19 product-only, the UI text billboard is
UI). The particle asset itself (ParticleAssetIO::vf3 007e3a10, ParticleAsset 007e1820 / Parse 007e1a40, the 12 curve functions 007e3510 ..
007e3940, the frame rect 007e3460) is loaded through the **particle** AssetIO loader. The world's vehicle effect sets (00696480 splash /
006963b0 dust constructors, 00697870 rebind, 006977f0 reset, 006976a0 record bind, 0069b090 release, the Y-ring emitter 0069ad60) are built
in `fx_worldparticles.c`. 0x102b ParticleSphere, 0x102c ParticleYRing and 0x102d BermParticle are **absent from all 57 level scenes** (scan:
builds/fxconstruct/logs/level_fx_scan.txt): the factory keeps their empty slots.
**built but not verifiable here**: 0x100f ForestSystem (the ForestAsset loader does not exist yet: the type count reads 0)
and 0xf WaterEntity (no water asset loader: only the entity fields). Exercised by FR_Alpine (not Concord): the StartEntity family, ChallengesSpec,
HitDiscovery collectable, ScrollingTexture, Water, the triggers' LevelConfig (FUN_00409560 ported).
Outside the core (the factory leaves the slot empty and `RX_SCENE_ALLOW_UNBUILT` is not needed): render lights and visibility (6 .. 0xb, 0xd,
0x105f VolumeFog, 0x1060, 0x1062, 0x1066 .. 0x1068, 0x1034 .. 0x1037, 0x104c .. 0x104e, 0x1072, 0x1074, 0x1075, 0x1077, 0x107e,
0x100a, 0x100e Grass, 0x1c), audio (0x1d, 0x105c .. 0x105e, 0x1083 .. 0x1093), UI (0x14 .. 0x1b, 0x1008, 0x107f, 0x1080, 0x1094), FX particles
(0x102a .. 0x1030), cameras (0x1001 .. 0x100d, 0x1045, 0x105b, 0x1069, 0x106a, 0x106b; 0x1046 .. 0x1049 are built since w-tvcam).
0x1062 (`TracksEntityRSData`) is `TrackSystem` (RTTI 0x0095b6cc; ctor 0066b740, vf5 0066b950: MaterialAsset0..6 + TrackBXML,
vf19 0066bf60, vf0_o98 0066bfe0), a per-vehicle trail/render system, **not** the tyre-track decals and absent from all 58
shipped `Database/*.dx9.scene` files (tyretracks scan). The terrain's ribbon decals are the HFTerrainSurface's three
`TerrainTracks` (007c4890 / 007cb9a0 / 007cbbf0), which the core builds; see notes/unity-api-surface.md "tyretracks".
**Not built (abort in the product, empty slot with RX_SCENE_ALLOW_UNBUILT)**: the animation variants 0x1053 .. 0x1057 (spline with stop logic,
proximity, trigger, soaring, roaming proximity) and 0x1058 .. 0x105a (with particles), 0x107d MusicScoreList,
0x12 / 0x13 / 0x16 / 0x1081 / 0x1082 (character, animation, action player, rider, vehicle: other units).

### 14.3 Physics bodies, the forest and the event handlers
PhysicsEntity (and the bales) create their rigid body in the physics world's *keyed objects* (world + 0x1994, 0xe0-byte records, build_world.h):
FUN_00769670 (the record from the PhysicsType material, the pose of the entity's matrix, the box of the model, the shape of the two collis
assets via the vehicle unit's rx_shape_handle_for, the tracking entry {refs, world, index}) pushed with FUN_007738e0 (-> rx_phys_dynamic_push /
rx_phys_dynamic_insert: the same templates). The entity keeps the tracking entry at +0x158; vf1 (Enable) sets the body's flag. Shapes from two
different collis handles (Male != Female asset) abort (00844360 with two handles is not ported); Concord does not need it.
ForestSystem: ctor 00662420 without the OS events and the culling thread, ForestSystem_Init, vf7's collision-info object (the colour sampling from
the AmbientDirectionalLight's reference map is render and skipped); **`rx_physics_world_install_forest(world)` (FUN_00772940 + FUN_00667cc0)
must run first in the world's finalize** (FUN_00769030 = rx_physics_world_finalize, whose `forest` parameter aborts): the driver calls it before.
Event handlers (006c3bb0 / 006c3cd0 / 006c3860 / 006c4270 / 006c4390 / 006c44b0 / 006c4970): the proxy object, its signature type name
(crt_type_info_name) compared with the event's and appended to the event's handler list. They need the event channels (VehicleTrigger [0xa89bc8],
ActivityManagerGlobalEvents [0xa93f74], created by the boot's 00409690 / 0050c500 regions); without a channel they abort (skipped with
RX_SCENE_ALLOW_UNBUILT: the driver has no activity). The callbacks (006c14a0, 006c0b60, 006bdb50 ...) and the bale / gate ticks are the race
layer's and not ported.

### 14.4 Verification
Driver: `tools/build_scene_test.sh <dir>` then `build_scene_test <install> <out> <level> <subscene[+subscene]> <anchors> <asset script>`
(stand-ins for the unit-12 objects: XenonCore, MXGame / MXWorld, the scene manager's lists and the render container's list headers
(FUN_007df410's data part), the physics world; the asset script = core/tools/build_script_concord.txt, for FR_Alpine the same with the level
packages), compare `build_scene_cmp.py --leaf-pointers --undef <second run with RX_BUILD_FILL=0xa5>` with the original's canonical dump.
* **NAT_Concord / NAT_ConcordBase** (1,140 InstancedAnimation, 524 Anchor, 283 Model, 264 Collision, 92 / 60 / 61 SingleAnimLoop / SplineAnimLoop / Scenario, 43 PhysicsEntity, 8 + 7 bales, 13 CrewChief, ...):
  pairs walked 28,967: identical 25,740, differing 3,227, missing subtrees 3,858 at 596 fields, core-only pointers 42. Of the differing nodes only
  MXWorld (the driver's stand-in) and one LensFlare / HFTerrain word are entities; the rest are asset handles and assets (render objects, Anim / BXML /
  Model asset fields), the animation pose pools of the character props (runtime) and the sorted-list vectors whose order differs by the keys above.
* **FR_Alpine / FR_AlpineBase + FR_AlpineFreeRide**: pairs walked 12,088: identical 11,049, differing 1,039, missing subtrees 966 at 276 fields. No entity class differs but the stand-in MXWorld
  and timer words (+0x24); the differing nodes are assets (the Alpine asset script is the Concord one with the level's packages, so Anim / Model assets of
  other packages show) and the character pose pools.
* Asset loaders (the build_model edits): `build_load_check model` 12,466 assets built, the same 67 failures (SX_FresnoB's truncated package).
What still differs in the entity classes, and why (each entry is an `--ignore` of the compare, entity-relative):
the render stamps and instance flags (ModelInstance bytes +0xa0 / +0xa4 / +0xa8 / +0xb0 / +0x128: the first render frames), the matrix copy at +0xc0
(refreshed by the renderer), the tick timers +0x20 / +0x24, the sort keys +0x78 (the counter 0x00a95838 is advanced by every entity that gets a
key; the outside-core classes (particles, audio emitters ...) take keys in the original, so the keys of later entities differ by whole steps;
with the counter start 0x40dffff7 the keys of the first sorted entities are bit-identical), the sort kind +0x70 of some prop classes (disabled
by the activity at run time), Character pose state (+0x150 .. +0x198, +0x1d4, +0x244 .. +0x284, +0x340 ..: 418 frames of animation) and the pose
pools (`+0x220` / `+0x260` blocks), InstancedAnimation +0xa6c (render), the PopDown bale's lowered position (+0x64) and the pop bales' lanes
w (stack leftovers of the original), HFTerrain's projected-texture matrices (+0x180 .. +0x2bc), tracks parameters (+0x44a0 ..) and surface render
pointers (unit 3), LensFlare's matrix (the sun follows the camera), the MXTrigger state bytes (+0xa0), ForestSystem's asset-dependent fields. In the
keyed physics vector: the position lane w of 27 bale bodies (+0x4c / +0x6c: the original has run-time pointers there). Asset handles / assets
(Texture, Material, Surface, Shader handles, Anim / BXML / Model assets): the asset units' known differences.

### 14.5 Private replacements (keep unregistered; merge in the integration sweep)
`scene_character_set_root_matrix` (CharacterEntity::vf16 00618070's Node_Invalidate through rx_world_node_invalidate; `scene_vcall` overrides the
slot), `scene_rand` (a private LCG for the random start phase: the seed owner is the CRT layer), `scene_light_list_add` (007f9e70: also private in
build_terrain.c), `handler_list_push_back` (005b9ff0), the current-world lookup and camera-target constructor of MXCameraPath (copies of the
static ones of build_mxworld.c: please export `camera_target_construct` / `current_world` there), `ci_strstr` (0078fcf0), the HFBump sort lists
(render), the RSData readers `rs_*` (replace FUN_0080e180 .. 0080f370 and the XMLReader). Registered by the scene unit and used by others:
rx_character_anim_init (00834de0), rx_model_instance_construct (007f11f0), rx_model_entity_construct (00401db0), rx_collision_entity_construct
(00818f60), rx_entity_base_vf5 (0080ded0), rx_event_register_* (006c3bb0 ...). Switched to the rider unit's: ModelAsset copy 007d4d60.

### 14.6 Left
* The SceneManager constructor (007d14e0, 007df410, 007dfba0, 007d6510: plan unit other_render: threads, critical sections, the render container and its
  culling grid +0x10f0); until it exists `rx_scene_lights_add` skips the registration when the grid pointer is 0 and the animation unit's Lights_Add
  (rx_lights_add) runs otherwise (it enters critical sections through the original).
* The classes listed under "Not built" and the loaders they need (forest, water, grass assets); WaterEntity_Reload, vf2 / vf19 / vf28 / vf29 of PhysicsEntity
  (disable, tick, set-pose: run-time), the audio emitter of a prop with a real SoundAsset (+0x160).
* The event system boot objects (the channels), then the handlers register for real; the driver skips them.
* PhysicsEntity bodies with two different collision handles (00844360 with two handles).

## 15. Implemented and verified: the vehicles (unit 6, 2026-10-19)

Files (`core/game`, about 3,600 lines; layouts and the API in `build_vehicle.h`): `build_vehicle.c` (the Motocross vehicle and its
small records, RavenVehicle ctor 0045cec0, the model instance 00542e00 / 004300e0 / 00460190, vf28 004625d0 / 004602c0, the proxy
00462b90, VehicleFactory_CreateVehicle 00542b90, the tuning 00543020 / 00543130), `build_vehicle_sim.c` (RavenSimulation 00430b50,
RavenSimulationMotocross 0043ac40 with the parameter file 0043a3f0 / 0043a560 / 0048bd20, the mechanism / node constructors, the masses
0043b750), `build_vehicle_params.inc` (the 108 parameter records of 004392f0), `build_vehicle_place.c` (S vf2 0044a650, the placement
0045f600), `build_vehicle_proxy.c` (the dynamic record 0084fcc0, shapes 00844360 / 00844240, the physics world's dynamic vector push /
insert 007743a0 / 00774120 with the grow 00775400, ProxyObject 00769160), `build_vehicle_parts.c` (VehicleParts_Assemble 00541310 /
00541690 / 00542150 / 00542540, VehicleLoader_FindPackage 00543db0, std::map<uint, uint>[] 00773290), `build_vehicle_spec.c` (the
vehicle list 005404f0, VehicleList_CreateSpec 00542b20 and the spec / model / part records, the part packages' requests 005401b0 /
005403b0, the enum lookups, Rdb reference readers 004ec210), `build_vehicle_pkgcache.c` (the package cache of the parts and gear
loaders, PackageCache_Update 00543e50, the DLC path 0053c740 / XenonOnlineContent::vf3 006d3c60, the parts loader 00540610 /
00540000), `build_vehicle_tables.c` (the generic Rdb readers 00402160 / 00402220 / 00418ec0 / 00418e50 / 0041aaf0 / 0041a480 / 00419b20),
`build_vehicle_tree.[ch]` (the MSVC 9 red-black tree, generic over the node's colour offset: _Insert / insert(hint) / insert / erase
0041b840 / ++ / --; others may call it), `build_vehicle_atv.c` + `build_vehicle_atv_params.inc` (the ATV, factory 1: 103 parameters of
00489cb0), `build_vehicle_wheeled.c` + `build_vehicle_utv_params.inc` / `build_vehicle_bt_params.inc` (the UTV, Buggy and TrophyTruck,
factories 7 / 5 / 3: one layout, the class differences as descriptor tables; 98 parameters of 004c9d40 / 004af510),
`build_vehicle_util.h` / `build_vehicle_int.h` (internal). Oracle adapters: `core/oracle/build_vehicle.cpp` (group
`rx_build_vehicle`). Additions to other units' files agreed with the lead: Database vf5 / vf7 / vf10 / vf12 / vf13 / vf16 / vf17 in
build_asset.c (8.8); msvcp90 find(const char*, off) 7848540c and replace(off, n, const char*) 78486c52 / (ptr, count) 78486aff in
crt_cpp.c (crt.md, for the four-wheel proxies). Uses: the rider unit's gear helpers (build_rider.h: LOD set 00543410 / 005434c0 / 005435a0, MaterialKey 00543fc0,
00544f70, 00416f10, "HR_%s"), the XMLReader virtuals of build_xml.c (through the reader's vtable), loop_get_tick_count (loop_platform).

### 15.1 What is built, in the original's order
1. MXGame construction: `rx_vehicle_list_construct(G + 0x154b0)` (005404f0). Game init (0075ed60): `rx_vehicle_list_init_loader(list)`
   (00540610: the list's VehicleHierarchies table, the FactoryPartsLoader [0x00d92ef0] = 00540000: vtable 0x940788, the package cache
   at + 4 (005436d0), the Database's HR_ background cache patterns).
2. Per Vehicle List row (the activity's, after VehicleList_AddRow): `rx_vehicle_list_create_spec(list, vehicle_id, hr)` (00542b20). The
   spec (200 bytes) comes from the row (VehicleId, VehicleOwner, OwnerVehicleId) and the owner's tables (00541220: a player's
   PlayerVehicle / PlayerVehicleParts, AI's AIVehicle / AIVehicleParts, owners 5 / 7 static tables, 6 VehicleModel itself, 8
   PresetVehicle / PresetVehicleParts). The model record (shared by VehicleModelId, counted) carries the factory (VehicleType.FactoryName:
   0 MotocrossBike, 1 Atv, 2 MonsterTruck, 3 TrophyTruck, 4 SandRail, 5 Buggy, 6 GolfCart, 7 Rhino), the engine class / size, the tuning
   defaults and the model's parts (VehicleModelParts, Default flag). Part records (shared by VehiclePartId) carry the slot
   (VehiclePartType.FactoryName of 17), HRAvailable and their material / model packages with their databases (the
   AssetPackageDatabase references). Each package is added to the cache with its HR_ twin (00540210: the database requested through
   00543750 -> Database vf7 / vf13 / vf5); then every part's packages are requested (005401b0 / 005403b0 -> 00543b30 -> Database vf16).
   This is the requesting code of the vehicle databases and packages of 8.4: the load order is derived, not recorded.
3. Every frame: `rx_pkgcache_update(cache, dt)` (00543e50, the parts loader's and the gear loader's): requested, unreferenced
   packages idle longer than 500 ms are unloaded (Database vf17), every 0.25 s the unmarked entries are purged (0041b840).
4. Per vehicle, when its packages are in (the activity's 0054a050): `rx_vehicle_factory_create(mxworld, list, vehicle_id)` (00542b90).
   Factory 0: RavenVehicleMotocross (0x1770 bytes, 0053f750 / 004622b0) -> V+0x194 = the factory -> the model instance of the spec's
   hierarchy (00542e00 / 00460190) -> the parts (00541310: pending parts, the LOD thresholds of the parts' models, the part material
   overrides 00542150 (frame 1/1, 1/11, 1/12, 2/1, 4/5), the frame's texture map V+0x6a0 (00542540), each part's models listed in the
   instance's LOD records with the node bounds grown (00541690), the crew chief's shirt texture) -> V+0x2a0 memset -> vf28 with the
   creation block {instance, 0, model+0x10, engine class, spec id, MXWorld} (004625d0: RavenSimulationMotocross 0x8fc0 = 0043ac40 with
   the mx125 / mx250 / mx250f / mx450f parameter file, the physics world's simulation list, the ProxyObject 00462b90, S vf18, the 12
   named nodes, the fender phases (game rand), RavenVehicle::vf28 004602c0: the physics world's +0x1914 list, the root rotation, the
   placement 0045f600 at the origin, vf49; the terrain tracks slots) -> the list's made vehicles (+0x40), the scene serial and
   SceneManager_AddEntity -> the tuning (00543130).
   Factory 1, RavenVehicleATV (6000 bytes, 0053f790 / 0047c4a0; vf28 0047c7f0): RavenSimulationATV 0x9bd0 = 0048bde0 (13 rays, the 103
   parameters, six mechanism nodes: main body, fixed body (the rider), four sliding rotors; "atv_vehicle" 0048b5d0 / 0048b680 type "atv";
   the masses 0048c750; the stunt poses), the proxy 0047cdf0 ("RonR_ATVProxyCollision", material "ATV"), the shared S vf18 004da440,
   27 named nodes, the tracks slots kind 2.
   Factories 7 / 5 / 3, RavenVehicleUTV / Buggy / TrophyTruck (0x15a0 / 0x1620 / 0x1630 bytes from ReflexAlignedAlloc in the factory;
   004c03a0 / 004a2550 / 0046a3e0; vf28 004c0700 / 004a2960 / 0046a850): the simulation 0x8850 / 0x8840 / 0x8930 (004cba90 / 004b1260 /
   0044fe40: 25 rays, 98 parameters, main body + four sliding rotors, the parameter file "utv_vehicle" type "UTV" / "buggy_vehicle"
   type "buggy" / "truck2WD_vehicle" (engine class 7) or "truck2WDlite_vehicle" (8) type "truck2WD", the masses 004cc030 / 004b1800 /
   00450480), the proxy 004c0c00 / 004a2f00 / 0046aef0 (the hierarchy's name with "Hierarchy" -> "Proxy" when the databases list that
   collis, else "UTVProxyCollision" / "BuggyProxyCollision" / "TrophyTruckProxyCollision"; material "Vehicle"; S vf18 inside), 25 / 30
   / 36 named nodes (UTV: S+0x55f0 = not "UTV01_Hierarchy"; Buggy: the engine node's local rotation at V+0x1400 and the material's
   gTextureOffset / usr_fade_value texture parameters), the tracks slots kind 3 / 5 / 4.

API for the rider and activity units: the vehicle +0x14 the scene serial, +0x188 MXWorld, +0x18c the spec id, +0x190 the player
slot (the activity sets it; 0045f600's player path 0054feb0 is the replay unit's), +0x194 the factory, +0x198 model+0x10, +0x1a0 the
rider (the rider unit sets it), +0x234 the simulation (also at +0x16d4 moto, +0x1700 ATV, +0x1540 UTV, +0x15c0 Buggy, +0x15d0
TrophyTruck), +0x1328 the model instance; the spec +0xc4 the vehicle. The package
cache API (rx_pkgcache_*) is what the gear loader uses (build_rider_entry.c calls it by address).

### 15.2 Verification
Verify boots (REFLEX_PORT_VERIFY=1, baton, NAT_Concord 12 riders AI 95; inner adapters counted in a second boot with the factory
excluded, `rx_build_vehicle,-rx_VehicleFactoryCreate`); calls / mismatches:

| function | calls / mismatches | notes |
|---|---|---|
| 00542b90 VehicleFactory_CreateVehicle (everything under it native) | 12 / 0 before the parts port; now 12 / 12 (a) | |
| 00541310 VehicleParts_Assemble | 12 / 12 (a) | |
| 004622b0 / 004625d0 / 004602c0 / 00460190 / 00542e00 / 00462b90 | 12 / 0 each | sim ctor, masses, vf18, proxy inside |
| 0045f600 placement / 0044a650 S vf2 | 26 / 0, 52 / 0 | also the race's respawns |
| 00542b20 VehicleList_CreateSpec / 005404f0 / 00540610 | 12 / 0, 1 / 0, 1 / 0 | |
| package cache 00543830 / 00543b30 / 00543cd0 / 00543e50 / 00543db0 | 201 / 0, 132 / 0 (4 unverifiable: the loader thread), 417 / 0, 1624 / 0, 536 / 0 | riders' calls included |
| 00543750 / 00543c30 / 00543990 / 005436d0 / 006d3c60 / 00773290 | 16 / 0 (1 unverifiable), 2 / 0, 2 / 0, 3 / 0, 8 / 0, 84 / 0 | |
| Rdb readers 00402160 / 00402220 / 00418ec0 / 00418e50 / 0041aaf0 / 0041a480 / 00419b20 / 004ec210 | 2697 / 801 / 268 / 208 / 1072 / 145 / 190 / 288, all 0 | |
| physics 0084fcc0 / 00844240 / 007743a0 / 00774120 / 00769160 | 192 / 58 / 209 / 267 / 12, all 0 | ATV event: 00769160 12 / 0 |

SX_Springfield (`Supercross,8,SX_Springfield,78,0,3,Laps,2,1`, 12 riders) and FR_Alpine (REFLEX_EVENT=alpine, 1 rider): the same
functions, all 0 mismatches except (a) (SX: 00774120 878 / 0, 00844240 669 / 0; FR_Alpine: the factory 1 / 1 (a)).

The other classes, through the Vehicle Select garage fields of the boot (REFLEX_GARAGE = rider owner, rider, gear set, vehicle owner,
vehicle, vehicle type; 11 AI of the class): ATV `National,10,NAT_Concord,50,0,1,Laps,2,0` with `4,1,3,4,44,4`; UTV / Buggy /
TrophyTruck `Powercross,16,PX_Laguna,58,0,<class>,Laps,2,0` with class 22 / 19 / 21 and `4,1,3,4,45,11` / `4,1,3,4,46,12` /
`4,1,3,4,47,14` (12 vehicles of the class each). Inner functions counted with the vf28 (and then the simulation constructor) excluded:

| class | constructor / vf28 / vf49 | simulation ctor / parameter file / masses | proxy alone |
|---|---|---|---|
| ATV | 12 / 0, 12 / 0, 38 / 0 | 12 / 0 (inside vf28), S vf18 004da440 12 / 0 | inside vf28 |
| UTV | 12 / 0, 12 / 0, 38 / 0 | 12 / 0, 12 / 0, 12 / 0 | 12 / 12 (b) |
| Buggy | 12 / 0, 12 / 0, 38 / 0 | 12 / 0, 12 / 0, 12 / 0 | 12 / 12 (b) |
| TrophyTruck | 12 / 0, 12 / 0, 38 / 0 | 12 / 0, 12 / 0, 12 / 0 | 12 / 12 (b) |

The factory itself: 12 / 12 on every event, (a) only (UTV 196 words per vehicle, all w lanes: its model has more nodes). The two msvcp90
members of the proxies (the original 004c0c00 with rx_crt On): 7848540c 12 / 0, 78486c52 12 / 0. Race checks (`racepair.sh`, PX_Laguna
UTV / Buggy / TrophyTruck, 300 race frames, frames 450 and 700): the w lanes of (a) (2,617 .. 3,245 words, more at frame 700: the cull list
copies them every frame) plus the 17 words of (c); camera, board and race state identical. Concord with the current build: 549 + (c).

(b) The ProxyObject's record (00769160, copied from its frame) keeps eight words (+0xb0 .. +0xcf) its initializers never write. With the
original vf28 around a native proxy they hold the residue of the original proxy's deeper calls (Database vf26 / vf27, PhysicsProperties'
map lookup 00850bf0 -> 00702ad0 -> msvcp90 operator<) in the original and nothing in the core. With the whole vf28 native the call
verifies 12 / 0 (the previous vehicle's original run left the same residue). Gone right after construction: the placement's vf62 calls
DynObj_SetPose 00768460, which clears the record's contact accumulators +0xb0 / +0xc0.

(c) XenonOnlineContentPackage records (006d3d30 -> 006d3e30, at startup): the constructor copies 0x608 bytes (rep movsd) of the caller's
partly initialized local content-data struct into the record; its unwritten words are the residue of the startup code that ran at that
stack depth, here the package cache's 00543cd0 (the original builds a std::string key in its frame and calls Database vf10 / vf12; the
core leaves other residue). 15 words of the first record (0x10925c40 .. 0x10925cd8 on these boots) and the two words of a worker stack
(0x135fffd4 / d8, as the JobPool ring). known-differences.md entry 1.

(a) Dead stack (REPRODUCED 2026-10-27, w-vfactory: the core writes the frames of the calls that leave these words, known-differences.md
entry 1; the factory verifies 0 mismatches on every class): 00541690 stores the grown bound's centre and half extents with movaps from two frame vectors whose w words it never
writes (frame +0x7c / +0x3c); the original's words there are residue of msvcp90 / msvcr90 internals (e.g. 0x784859be, an msvcp90 return
address) and of the package map insert; the core's CRT leaves none. 44 words per vehicle, every one a w lane (known-differences.md
entry 1). Race check (`racerun.sh`: the group On without Verify, 300 race frames, full memory at frames 450 and 700 against the
original's, main stack excluded): 544 differing words at both frames, all at addresses = 0xc (mod 16) (those lanes and their copies in the
octree records and the cull list); camera, board and race state identical. Before the parts port the factory verified 12 / 0: the
factory, vf28s, placement and proxy keep the original's ESP, frames and /GS cookies (veh_call_sp / veh_frame16 / veh_cookie) because
the scene / octree code downstream copies uninitialized frame words (cookies, saved registers) into its records.

Product (core/tools/build_vehicle_test.c, unit `sim`): RavenSimulationMotocross built from the install compared with the original's
snapshot right after 0043ac40 (build_vehicle_cmp.py --snap): identical. Unit `lists`: the reference event's Vehicle / Rider / Player
List rows through unit 12a's add-row API (Player List: a TEST STAND-IN for the activity), identical cell by cell. Unit `vehicles`
(specs + factory in the product; the driver loads the CRT DLLs' data with rx_image_load_crt + rx_crt_startup and builds the context
with rx_engine_context_create): the lists, the MXWorld, the spec of the slot (with its package requests served at once) and the factory up
to RavenVehicle::vf28's root rotation run; there anim_extern.h's anim_matrix_to_quat still calls the original 00401740 (the anim
unit's, reported), so the canonical compare of the vehicles waits for that.

### 15.3 Test and development tools
* `core/tools/build_vehicle_test.c` / `.sh` (driver; the STAND-INs are marked), `build_vehicle_cmp.py` (rx_canon_cmp.py with
  `--snap <RXWI image>`: the original's words from a snapshot taken at the construction moment, since the vehicles run frames before
  the world capture).
* `core/oracle/build_vehicle_trace.cpp` (group `rx_build_vtrace`, registered only with REFLEX_VTRACE=<file>): logs calls of the
  functions in REFLEX_VTRACE_FNS (hex addresses) inside windows REFLEX_VTRACE_WIN=<entry>:<ret>,... (one line per call: steps, esp,
  address, return, registers, four stack words); REFLEX_VTRACE_SNAP=<ret>:<k>:<path>,... writes an RXWI image of all memory at the k-th
  return to <ret> (e.g. 0084f2a6:<frame> = the k-th Engine::Update); REFLEX_VTRACE_READS=<file> the read map of the windows (no JIT).
  The original always runs. Used for the call trees (which functions the factory reaches) and the snapshots of 15.2.
* The graft scaffold of the earlier sessions is gone (unit 12a's tables and add-row API replaced it).

### 15.4 Left
* Factories 2 / 4 / 6 (MonsterTruck, SandRail, GolfCart) have no case in 00542b90 (the original leaves the spec's vehicle 0). The
  placement reaches each class's frame functions (ATV vf60 0047e850 24 KB, vf62, vf33, S vf2 / vf16 / vf32; the four-wheel classes'
  equivalents): race-frame code, they run the original. The bodies of RavenVehicle::vf28 / the placement are the moto's (verified on
  every class).
* The pending-part swap of 00541310 (0053fa30 / 005400c0 / 00540450 / 00544c80 / 0053f9f0) and the part-record release of 005409d0
  (0053f9f0 / 00540300) abort naming the address (not reached by the reference events).
* 0045f600's player path (0054feb0) and replay mode 1 (004331d0) still call the original (replay unit).
* The product run of `vehicles` (above) and the canonical compare of all 12 vehicles; FR_Alpine and SX_Springfield boots.

## 14. Implemented and verified: globals + startup (unit 12b, 2026-10-19)

Files: `src/rx_image.[ch]` (the install's PE data), `game/build_globals.[ch]` (the entry points and the _initterm walker), `game/build_globals_gen.c`
(GENERATED: the static initializers and the constructors they call), `game/build_globals_class.c` (KeyMap, ConVar, vector, critical section, pad
names), `game/build_globals_input.c` (the key binding loader), `game/build_globals_engine.c` (Engine::Startup), `game/build_globals_crt.c` (msvcr90's
startup data), `game/build_globals_dlc.c` (the DLC finder), `game/build_globals_int.h`; tools `tools/build_globals_gen.py` (+ `build_globals_regen.sh`),
`build_globals_test.[c|sh]`, `build_globals_cmp.py`; oracle `oracle/build_globals.cpp` (startup tracer), `oracle/build_globals_ports.cpp` (GENERATED
adapters, group `rx_globals`), `oracle/build_globals_itab.inc` (the _initterm table).

### 14.1 The image (rx_image) and the startup API (call order)

```c
rx_mem_init();  fmt_install* in = fmt_install_open(root);
rx_image_load(fmt_install_vfs(in));        /* MXReflex.exe .rdata (0x916000), .data (0x9a6000, zero beyond the raw size), .tls (0xdd0000): memcpy, no code runs */
rx_image_load_crt(fmt_install_vfs(in));    /* msvcr90.dll / msvcp90.dll 9.0.30729.1 from Installers/vcredist_x86.exe: .text + .data at 0x78520000 / 0x78480000 */
rx_globals_init(in);                       /* = rx_crt_startup(); rx_globals_static_init(); rx_tbl_create_schemas(); rx_engine_startup(in, tick); rx_globals_game(in); */
```
`rx_image_load*` are no-ops in the oracle (the emulator has the sections). `rx_globals_run(in, "none" | "init" | "engine" | "game")` stops after a phase
(tests). `rx_engine_startup` creates the Database (`rx_database_create` + `register_all`, engine context +8) and the Registry (+0xc): do not create them
again. `rx_globals_game` registers the core asset loaders, runs FUN_004e6280 (the tables' startup), MXRegistry_Load and the DLC loads (12a's sequence 12.2).
The DLL files come out of the vcredist installer (CAB in IExpress, vc_red.cab in it, both LZX window 21 with E8 translation): `rx_image_extract_crt` returns the
two files (byte identical to the world runtime's cache, checked with cmp). **`fmt_lzx.c` fix** (lead's file): the E8 call translation was applied in place in the
LZ window, so later matches copied translated bytes; it now translates a copy of the frame (libmspack's e8_buf). XMem packages have no E8: unaffected.
The DLLs have no .rdata: their read-only tables (printf's `__lookuptable` 0x78521bf0, the exception action table, libm constants) sit in .text, which the core's
CRT ports read as data, so `.text` is loaded too (no code of it runs). Smoke test: `RX_CRT_SMOKE=1 build_globals_test` (crt_stricmp / strnicmp / std::string /
sprintf in the product build with the loaded data: all work; `__locale_changed` 0x785bc218 is 0 in the file and after startup).

### 14.2 msvcr90's startup data (build_globals_crt.c: DllMain -> _CRT_INIT as far as the core's CRT ports read it)
Compared with the DLL's data at the exe's entry point (oracle tracer snapshot, `dllcmp.py` in the scratchpad): the product matches except 20 words nothing
reads: the stack-cookie seed (0x785ba2a8), the stdio tables (0x785bc658), the environment / command line blocks (0x785bd660 / 664 / 6a0 / 688), the program
name (0x785bba24..2c), the encoded kernel32 entry points (0x785bbc98.. : the OS layer answers those calls), 0x785bc0dc, 0x785bc23c.., and msvcp90's whole
startup data (facet ids, the locale registry: std::string does not touch them). Set: `__crtheap` 0x785bc160 (the reference run's GetProcessHeap 0x150000),
`__active_heap` 1, `__sse2_available` 1, `__flsindex` 2 / `__tlsindex` 1 (the TlsAlloc results of the reference run), the lock table (`_mtinitlocks`: every
`_locktable` entry of kind lkPrealloc gets the next critical section of 0x785bba40, spin count 4000), `_setmbcp(1252)` (the Latin-1 letters 0xc0..0xff in
`_mbctype` / `_mbcasemap`, the 544-byte threadmbcinfo copy with refcount 2, `__ptmbcinfo`) and the 216-byte `__ptlocinfo` copy of `__initiallocinfo`. The tables
are the emulator's GetStringTypeA / LCMapStringA results (it only classifies 0xc0..0xff; real Windows also classifies 0x8a, 0x8c ...).

### 14.3 The static initializers (_initterm, 990 slots) -- `tools/build_globals_gen.py`
The initializers are tiny: stores, constant copies, SSE vector constants, global object constructors, function-local static guards, `_atexit`. The generator
runs the instructions symbolically (capstone over MXReflex.exe; registers hold constants, address expressions or loads at their place in the stream; x87
constant loads through the emulator's float format; forward branches become structured `if / else` with join variables, counted loops `for (;;)` with the
loop-carried registers as variables, stack objects live in a game-stack frame `RX_FRAME_ENTER`, calls to straight-line functions are translated the same way
(parameters = the registers read before written, the stack slots above the return address, an xmm result, the clobbered registers), `vector constructor
iterator' becomes a loop) and writes readable C over the memory model. Hand-written callees are in CALLEES / IAT / INDIRECT of the generator (KeyMap 00811230,
ConVar 00417230 / 00402b40 / 00402be0, InputNames_SetPadName, the CRT string functions through `crt.h`, sprintf "%s%u"). `_atexit` is `RX_ATEXIT(fn)`: nothing in the
product, the original's call in the oracle (so Verify compares the CRT's atexit table too).
- **751 functions are translated (749 initializers + InputNames_Init + the XenonCore constructor; of the 957 non-table initializers)** (+ the 34 table objects 00903b20..00904160 of 12a, called from the walker at their slot, + 00903b10 =
  rx_tbl_alloc_init); 56 helper functions (constructors) are generated static. They are registered (RX_FUNCTION at the initializer's address) and the walker
  `rx_globals_static_init` runs them in the table's order through the registry.
- **Verification**: (1) per call, oracle Verify (`REFLEX_PORT=rx_globals REFLEX_PORT_VERIFY=1`, baton boot of the reference event): all 749 initializer ports (the generated ones + 00903b10)
  called once and verified once, 0 mismatches (memory writes, registers, the CRT's atexit table), the Verify world byte-identical to the reference world; On mode
  (the core's initializers alone) also gives the byte-identical world image. (2) The product's state after `rx_globals_static_init` against the original's at
  ReflexWinMain (`build_globals_cmp.py`, the oracle tracer's `s1.img` + heap pages): no differing and no extra .data word; heap pointers are paired by structure.
  Words of the original that the core does not write (116,989 of the 175,557 the initializers write) are the untranslated ones, below.
- **Not translated (207), and why**: 106 initializers register UI components in a `std::map<string,factory>` (0x00d92f24; FUN_005ba330 / 005ba360 /
  00593950 / 00586700 / 00690910) -- UI, and the map insert is the event system's (build8); ~46 Quazal (network) statics through the Quazal MemoryManager
  (FUN_00883e60 ...); 11 UI loops (5e9d76); 00908500 (9 x FUN_006596f0: 113,841 words, the particle / forest parameter registrations FUN_00659ef0: a loop entered in
  its middle, 8 words read by the race), 00910630 / 009101a0 (UI / audio objects), the CRT's own slots (pre_cpp_init 0090039c, pre_c_init), a few with
  `imul` / `cvttss2si` / import calls (GetCurrentThreadId, QueryPerformanceCounter). `rx_globals_unported()` lists the slots the walker skips. Of the 7,574 words the
  race reads that differ from the exe image, the untranslated initializers account for 160 (the IAT, loader owned) + 8 (00908500) + the UI flags.
- Differences kept: x87 `fld / fst` of a NaN gives 0x7fffffff (the emulator's extended format has no payload; real hardware keeps 0x7fc00000): one constant,
  0x00d616b0 (nothing reads it). InitializeCriticalSection is the emulator's stand-in (DebugInfo 0, LockCount -1, the rest 0).

### 14.4 Engine::Startup (build_globals_engine.c, build_globals_input.c)
Built: the engine context (XenonCore, FUN_00843950 generated; [0x00a95844]; +0x10 the engine frame timing object 0x00d728d0, whose constructor is a static
initializer), the Registry (12a), `srand(tick)` (the CRT's per-thread `_holdrand`) + `g_GameRandSeed` / `Count`, InputNames (first KeyMap construction), the keyboard
scan table (Keyboard_InitScanTable, the 101 immediate stores as a table + the inverse loop), **the key bindings** (FUN_00812ea0 on "KeyBind_Core.bxml" and "KeyBind_App.bxml"
read with fmt_bxml: the line parser FUN_00813b00, the key / pad / mouse / modifier name tables, KeyMap_SetSlot 008112e0, the KeyMapCommand objects ":mod:key:" and their vector),
the Database, the vertex declaration cache (+0x14) and device (+0x1ec), the 0x80000-byte scratch block [0x00d6daf0]. The KeyMap records match the original's at the
MXGame::Startup entry (the 307 + 670 words of the two files; the race reads them every frame through the registry): in 0xd5a000..0xd6c000 the only differences are 84 words of
untranslated static initializers (UI / Quazal statics around 0xd60cf0 .. 0xd65a10) and the Camera object at 0xd66030 (vtable 0x95a2d8: Engine::Startup's renderer part sets its
view parameters, 0xd66048 / 0xd66084.. 0xd66098; the static initializer's 1 at 0xd66098 is reset there). The tick the game uses is the caller's
(`rx_globals_init` passes 0; the reference run's GetTickCount is virtual time). Engine fields set later by Engine::vf0 (1/60 at +0x14) are part of Engine::Startup's
tail and are NOT set yet (see 14.6).
**Not built (renderer / audio / platform; nothing the core reads)**: the D3D device settings in XenonCore (+0x3c..+0x1e8), the audio system (FUN_00804070: [0x00a957d0..],
g_VolumeChannelDefaults, the 0x8980 table), the debug overlay (FUN_00822330 / 00822570), FUN_007adc60's graphics registry reads, FUN_007b00c0 (renderer), the task thread
and events, the QueryPerformanceCounter stamps, the window.
**Math tables**: there are no startup-built math tables. The only table-building code the race reads is Noise1D's gradient tables 0x00a87980 / 0x00a89190 (built lazily
by Noise1D 00853180 from `rand()` seeded by Engine::Startup's `srand`): the Noise1D port must build them (and see `g_GameRandSeed` / the CRT seed).

### 14.5 The DLC finder (build_globals_dlc.c)
`rx_dlc_find(vfs, names, max)`: XenonOnlineContentPackage::vf5 006d3f40 -- the install's `Database\*DLC*.dx9.database`, the name up to the first '.', in the `std::map`'s order (strcmp);
38 names in the reference install. `rx_dlc_load(in, db)` = FUN_0053c410: the names containing "Tables" or "MXUI" go to 12a's `rx_tbl_load_dlc_tables` (database + package, serials
exact): MXTables_DLC003 .. 005 and MXUI_DLC001, 003, 004, 005 (there is no MXUI_DLC002 in the install; 12a's test list names it).

### 14.6 Left, and why
- **MXGame's constructor (FUN_00756760 around 12a's schema calls), MXGame::Startup (00756e10) and the init thread (00760400 / 0075ed60) beyond the table and DLC loads.**
  They build the MXGame object (0x16210 bytes: Game base 00882080 with three 0x2d20 objects, FUN_00755ff0 x2, 00552890 (a Quazal MemoryManager object), 005404f0 / 0053d210
  (package caches), MXGarage_Construct, MusicPlayer 00675890, MXHighlightTracking, the "LoadingThreadEvent" events), the global **EventSystem channels and their event types**
  (SceneLoading in the constructor; MXGame::Startup: 0055ed50 ActivityManagerGlobalEvents [0xa93f74], 00409690 VehicleTrigger [0xa89bc8] + the particle manager's
  jobs, 006c8ee0, 006caa80, 00549b30, 0051c620; 006c8fd0 "Stunts" [0xa94304], 006cb870 "StuntScoring" [0xa94318]), the registry-driven game fields (MXGame\Input / UISceneName,
  game +0x20..0x30, +0x34, +0x39c0 ...), the Lua state (FUN_00744180: 12's `rx_lua_memory_init` is its memory part; the VM is not ported: lead's decision), the localized
  database manager (FUN_00598640), the debug overlay. The channel machinery (registry 00593900, Channel 005930a0, 007ce320, register 00593520, map insert 00593950) is the activity
  unit's (build8: `rx_event_channel_create`, build_activity.h): the channels and their event-type registrations (FUN_00590c00 ... 006ce210) should be created by calling
  that API in this order once it covers event types. The generator already translates the straight-line constructors (it translates 00882080's callees except the
  `mul`-sized allocation FUN_007d14e0, 00755ff0 and 005404f0 / 0053d210 are generated-ready: run it with `--func` for them).
  Measured: the MXGame::Startup .. Update interval writes 625 words (115 the race reads: the channel globals, the Lua pool structure d94930.., controller selection arrays
  0x9c3e1c / 0x9c5424, the localization manager's tables), nothing else in .data.
- Engine::vf0(60.0) and the Engine fields written by Engine::Startup's tail (+0x14 = 1/60 = 0x3c888889 at 0x00d728e4 ...): trivial, to be set with the loop unit's Engine::UpdateTiming
  port (it owns the Engine object's frame fields).
- The untranslated initializers of 14.3 (UI flags, Quazal, particle parameter tables).
- The FUN_0053c410 "Leaderboards" branch (no DLC name has it).

### 14.7 Notes for the lead (sweep)
- `rx_load_runtime_constants` (build_load.c) and `rx_load_test_scaffold` are superseded: the vector constants 0x00d615c0..0x00d616c0 now come from the generated initializers
  (identical to the stand-in's values except the NaN constant above), the engine context / decl cache / device from `rx_engine_startup`. Drivers can call `rx_globals_run(in, "engine")`.
- Test drivers that load tables without `rx_globals_init` should call `rx_image_load_crt` + `rx_crt_startup()` for the CRT data (the symptom "crt_stricmp -> crt_unported" needs
  `__locale_changed` != 0, which the loaded image never has).
- Duplicate symbol in other units' files at the time of writing: `rx_game_player_mode` in build_activity_event.c and race_misc_game.c (build_globals_test.sh has `RX_SKIP="file ..."`).
- Oracle: the startup tracer is `REFLEX_GLOBALS_TRACE=<log> REFLEX_PORT=rx_globals_trace` with `REFLEX_GLOBALS_POINTS=<hex,..>` (function entries as phase boundaries) and
  `REFLEX_GLOBALS_SNAP=<hex>:<file.img>` (snapshots of the exe image range + `.heap` + `.dll`); log lines `H` (hook) / `W addr old new`. Build it alone with
  `REFLEX_PORT_FILES=none RX_CORE_FILES="build_globals.cpp" native/game/tools/build_cli.sh <dir>`.

### 14.8 The NetworkManager and its "Network" channel (2026-10-26, branch `port/network-manager-init`)
MXGame::Startup 00756e10 calls, after FUN_004e6280 and the session static (0xdcf304 / 006de1f0) at 00756f16, **006de390
NetworkManager::Init** (stdcall 0x00da6ca0, a2, a1) and then, each behind the static's guard, 007604b0 / 00760570 (MXGame's
"NetworkSessionState" / "NetworkTimerEvents" handlers), 00552d20 (the player slots' NetworkObjectAdded / Removed / Migrated handlers
through 00554eb0) and 007558e0 (the DataMiner [0xd92f10]'s "NetworkLoginStatusChanged"), before 0055ed50. The core runs the same
sequence (`rx_mxgame_network_startup`, build_mxgame_init.c) at that point of `mxgame_startup`.
- 006de390 is a composition (not registered): the Quazal allocator hooks [0x009df5b8] = 00401f90 / [0x009df5bc] = 006de180, then
  **006de4b0** (ported): the "Network" EventSystem::Channel (registry 00593900, the inlined Channel constructor, 00593520) at
  [0xda6ca0], its 40 event types (the instantiations of build_activity_events.inc and 005ed6e0, in the original's order), the
  manager's handlers 007071a0 ("NetworkEventDetailsChanged", 006df1b0) and 00707260 ("NetworkObjectAdded", 006df510).
- **Boundary** (network play): 006de390's device manager (0x1a0, 006e0d20, [0xda6cac]), PCSession (0x1d40, 006e9120, [0xda6cb0]) and
  their vf1 starts, the 0xda6b40 static (006df870), the helpers +0x44 (00702480) / +0x48 (006ee720) are not built; [0xda6cb0] = 0 is the
  offline stand-in every core reader checks (QUEUE "presencefix"). Evidence (startup compare below): the only differences on the
  channel are the 5 handlers those objects subscribe (NetworkSessionState 006e55d0 / 006e8780, NetworkLoginStatusChanged 006e8830,
  NetworkStatus 006e5640, NetworkPlayerVehicleUpdated 0070c260).
- Verify (baton boots of Concord, REFLEX_PORT_VERIFY=1): 007604b0 1/1/0, 00760570 1/1/0, 00552d20 1/1/0, 007558e0 1/1/0; with the outer
  ports excluded 007071a0 1/1/0, 00707260 1/1/0, 00554eb0 3/3/0. 006de4b0 1/1 with 195 words, all of them the type-name undecorator's
  freed scratch block (0x109eddb4 .. 0x109ee0c0) and its HeapManager word 0x785bc0f8 (the crt_undname.c difference: its scratch
  nodes are not reproduced word for word): the same residue rx_ActivityGlobalChannel 0055ed50 / rx_VehicleTriggerChannels 00409690 show.
- Startup compare (original: `REFLEX_GLOBALS_SNAP=0055ed50:...`; core: the startup up to the same point, `build_globals_cmp.py`'s pairing
  walk from 0xda6ca0 without descending into MXGame / the save object): the registry holds the same 6 channels (MXGarage, Network,
  SceneLoading, StorageNotification, StuntScoring, Stunts) with the same event counts; the "Network" channel's 40 events have the same
  names, signature strings and handler proxies (vtable, object, method) except the boundary's 5; the remaining raw-word differences are
  uninitialized bytes only (the aligned allocations' slack, unused std::string buffer bytes of long names, node padding, the map head's
  unconstructed value) and the boundary words +0xc / +0x10 / +0x44 / +0x48 / the Quazal mutex +0x50 (006de1f0's).
- With the real channel MXGame's handler is subscribed, so the offline timer event (006e4a90 / 00707320, kind 1, each Pre frame) now
  reaches **0075cf50** (MXGame's "NetworkTimerEvents" handler through the proxy 007606d0): ported (build_mxgame_init.c), with its
  callee **004df6a0** (the startup table's value; was build_activity_grid.c's static `startup_table_value`, now registered as
  `rx_startup_table_value`). Its kind-2 branch under the debug override [0xd611f8] ends in the UI's "GotoHub" (0059b7e0): UI boundary
  (oracle original, product nothing); offline only kind 1 is raised.
- Past the driver release, Activity::vf3 00557790's last unsubscription ("PageWipeTransition" on the UI manager's channel
  [[0xa93fcc] + 0x98], 00587580) read the channel through a null UI manager (the product has none) and walked address 0x98's zero
  memory forever in 00593220. The subscription in 00556ec0 already has the product boundary "no UI manager, no subscription"; the
  teardown now has the same guard (build_activity_teardown.c; the oracle always has the manager, so its Verify is unchanged).
- After vf3 the area update 007669f0 calls **Activity::vf6 00557a10** (the area's subscene unloaded through **007d19f0**'s named
  branch -> **007d1470** (each entity: 007d3040, vf9, vf0(1); the array freed), then the world's racing line **00767850**) and the
  deleting destructor **MXActivityRace::sdtor 00564af0** -> **005659e0** (MXActivityRaceStub: the audio logic 00580d30, the scenarios'
  BaseScenario vtables, Activity 005566b0; the waypoint unit's existing ports). Ported in build_activity_teardown.c /
  race_activity_teardown.c; verified with the teardown probe (`RX_TD_DTOR=1`: vf3, then the original vf6(0) and vf0(1) on the live
  Concord activity): 00557a10 1/1/0, 00564af0 1/1/0; outer ports excluded: 007d19f0 1/1/0, 005659e0 1/1/0, 00767850 1/1/0. 007d1470
  is not reached there (Concord's activity subscene has no entity record); its entity classes' vf9 / vf0 are dispatched (an unported
  class aborts naming itself). 007d19f0 without a name (the whole scene) stays unreached.
- Moto 2's rider construction erases non-empty std::set<std::string> (build_rider_init.c's 00851f10 stub aborted): it now calls the
  full 00851f10 port (rx_channel_map_erase_subtree).
- The stand-ins are gone: `rx_session.c build_session_channel` (8 types, no "NetworkActivityEvent": the moto-1 teardown's 0053a980
  found no such event and stopped in `_invalid_parameter_noinfo`) and build_event_test.c's copy.

## 16. Implemented and verified: the activity, the event and the game's state machine (unit 8, 2026-10-19)

Files (`core/game`): `build_activity.h` (the API, conventions per prototype), `build_activity_util.h` (frames at the original's
stack addresses, calls of other units' functions by address, the UI boundary), `build_activity_setup.c` (the Player List rows),
`build_activity_event.c` (SetupEvent, the AI roster, StartEvent and the level request), `build_activity_player.c` (the player
slots, MXPlayer, its stunt system / controls), `build_activity_race.c` (the event-system channels and event types, the activity
manager, MXActivityRace, the activity area, MXWorld::vf3), `build_activity_world.c` (the world load requests and FUN_00759f00),
`build_activity_driver.c` (a player joins: RaceDriver, the AI rules / behaviours), `build_activity_game.c` (MXGame::Update,
Game_EnterRealWorldAudio). Oracle adapters: `core/oracle/build_activity.cpp` (group rx_build8). Driver: `core/tools/build_event_test.c`
+ `build_event_test.sh`.

### 16.1 The flow (the original's order; the core's functions)
1. Front end -> event (the Unity front end fills an `rx_event_spec`; the original's Lua sets the UI properties):
   `rx_ensure_local_players` 0059e030 (Player List rows of the local players: 004dd460, viewports 005a1420; the PlayerMode callback
   removes them again: 0059e310 / 0059e4e0 / 0059e7a0 / Table row erase), `rx_setup_event` 0059a970 (the activity row 005a33b0 /
   004dedc0 / 004df0f0, the AI roster 0059f2d0 -> 0059f3e0 (the game's LCG 0x9cb860, random_shuffle 005c1360 on the CRT rand) ->
   0059ebc0 -> 0059e960 (Vehicle / Rider List rows)), `rx_start_event` 0059ade0 (the local players request the activity 005a3590 /
   004dde70, the other activity rows erased 005a3750, the Duration / NumLaps cells 005063c0 / 004f6e60, Game_SetupActivityArea,
   UI_RequestConfiguredLevel 0059bd60 -> MXGame_ResumeFromUI 0075c8c0 -> MXGame_RequestLevel 0075c640: state 3).
2. `rx_mxgame_update` 00757fb0 states: 3 (the Player List's Loaded / Ready flags cleared, the activity area, the load wait,
   the level database (Database vf5) and packages: `rx_level_config_init` 00756060, `rx_level_load_request` 007560f0), 4
   (`rx_level_loaded` 00756250, then the loader job "Subscene Creation" = `rx_world_construct` FUN_00759f00: run at once in the
   product), 5 (the job done), 6 (every player loaded: the grid 0075f890, the start 0075e020), 7 / 8 (the UI's loading page),
   0xb (`rx_game_enter_real_world` 0075a3f0: the vehicles into the event world, the activities started 007668e0 (vf2), the activity
   area), then 2 (running). After the state: the frame (the players' deferred work, the session tick, the world's vf5, the package /
   loader / records / effects / mud / clip recorder updates of their units).
3. `rx_world_construct` FUN_00759f00: MXWorld (unit 3) on the level config, the scene file's subscene directory 007562b0 (the activity
   subscene list 00556140, the int hash 007cfa10; the product reads the directory with fmt_scene), the physics world, the Base
   subscenes 00766110 -> `rx_mxworld_load_subscene` 00766250 -> 007564f0 (rx_scene_load_subscene, unit 4), the culling grid
   (render), MXWorld::vf1 (unit 3), `rx_mxworld_vf3` 007665f0: the particle effects (fx unit), the activity manager 0055ee60 (its
   channel "<level> Root Activity Channel"), per activity row the factory 0055ef80 -> MXActivityRace 0x13e660 (00565830 / 005562c0 /
   005825a0), vf1 00568050 / 00556d90 (Duration, ActivityMode (vf22), ActivityArea, Spectator, NumLaps; the name), vf5 00565a30 /
   00556c30 (the area's subscene 00555e60 / 00766250, the physics world finalized (units 3 / 9), the audio logic (audio), the lap pacing
   00556720 (the "<level>-lappacing" BXML through the XMLReader virtuals), the track name 0055a5d0 / racing line 007677e0,
   "Activity Basic Essentials Loaded").
4. The players: Game_SetupActivityArea 0075c430 -> `rx_players_sync` 0075ba20 (the MXPlayer slots 00552ad0 / 005490f0 from the
   Player List 0054a7b0) -> `rx_activity_area_update` 007669f0 -> the world's player pass 00766ec0 (loop unit) ->
   `rx_activity_on_player_joined` 0055b610: the driver (vf44 00566ea0 -> RaceDriver 0050ed80 / NetworkDriver 0050e1b0 / BaseDriver
   00508c10), 0054d9d0, the controls 006c9100 and stunt system 006ca750 reset, the rules and behaviours (vf47 00565e00: 22 getters
   carving them from the driver's block, each vf1; then 00508f50: each behaviour's vf3 links (the finders 0053b450 ..) and vf6
   resets), the start zone 0054cd20, the start slot and the vehicle's start pose (the slot's quaternion), the race index, RaceDriver::vf1
   0050ee40 / NetworkDriver::vf1 0050e380 (their handlers), the driver arrays, "PlayerJoined", the activity's vf50 (the first local
   human), MXRecords_Join, RaceDriver::vf3, the grid pass 0075f890.
5. The event system (also used by the MXGame unit for the global channels): `rx_event_channel_create`, `rx_event_type_register(_at)`
   (all 85 instantiations of FUN_00590c00's template), `rx_event_handler_list_push_back` 005b9ff0, the Event constructor 00592bd0.

### 16.2 Verification (per-call Verify, NAT_Concord baton boot, REFLEX_PORT=rx_build8 REFLEX_PORT_VERIFY=1; calls / mismatches)
All 0 mismatches: Rdb add_row 53, set_int 86,960, set_cell_int 12, erase 3, query 18, delete 5, find_first 6, select, the list adds
13 / 13; EnsureLocalPlayers 2, 004dd460 11, 0059e310 4, 0059e4e0 2, 0059e7a0 1; ActivityTable_AddRow 1, 004df0f0 2, 005a33b0 1,
004de3f0 1; SetupEvent 1 (whole: AI roster, LCG, shuffle); SetupAIRiders 1; StuntSystemReset 26; PlayerSlotAlloc / MXPlayer init 12,
PlayerFindByName 24, PlayerSlot_FromPlayerList 12, 0075ba20 6; the channels: registry 24, BaseChannel 24, register 26, map insert 106,
list head 718, Event ctor 669, the 85 event-type instantiations 647 calls, 005b9ff0 880; 0055ee60 1, 0055ef80 1, 00556d90 1,
00568050 1, 00556720 1, 0055a5d0 1, 007677e0 1, 00555e60 65, 00556140 2, 007cfa10 29, 00756060 / 007560f0 / 00756250 2 each,
00767480 2, 007669f0 3, StartEvent 1 (whole: request level, resume, the activity area), OnPlayerJoined 13 (whole: the 12 riders'
drivers, behaviours, start poses), MXGame::Update 290 verified of 418 calls (the others: the original switches threads in that
frame; all states 0 .. 0xb visited). Activity::vf5 / MXActivityRaceStub::vf5 match when the forest step is the original's: the
scene unit's 00772940 was 0x20 off in the heap (reported; fixed by the forest unit as section 9.6 says).
Not verifiable per call (thread switches: the loader's file I/O, the audio wait): FUN_00759f00, 00766250, 007564f0 / 007562b0 /
00766110 (the oracle runs the original for their file / audio parts; the product path is the fmt reader).
Composition note: with MXGame::Update native in the same boot, the first type-name undecoration of each event type (msvcr90's
__unDName keeps dead-stack words in its heap arena) and MXGame::Update's state 0 show dead-stack differences; each verifies clean
alone (REFLEX_PORT=rx_MXGameUpdate, or the group without it).

### 16.3 UI properties (lead's condition (a))
The core reads the spec where the original reads UI properties: EventType, EventTypeId, EventTypeActivation, EventLocation,
EventLocationId, TrackId, EventDuration / EventDurationType, EventRiders, AIDifficulty, VehicleClass, PlayerMode, the profile's
display name, EventHasLaps / EventHasTimer (from the duration type) / EventHasRounds (0); QuickEvent / IsWinningPlayer /
SpectatorMode are "not set" (0).
Kept (gameplay reads them back): EventRiders and AIDifficulty (SetupEvent's registry overrides) in the spec; every table cell the
event writes (Player List, Vehicle / Rider List, the activity table) is kept as the original.
Dropped (UI only, nothing in the core reads them): SpectatorMode, EventTypeActivated, StartEvent's EventType / EventTypeActivation
strings, ExclusiveDeviceIndex, InRealWorld, WinningPlayerCount, the UI's ActivityRulesChanged event, the HUD page (005a43e0), the
session's event description (0059cd40) and presence (MXGame_RequestLevel's session vf12), the UI's loading page (states 7 / 8: the
product goes on at once), the loading screen's progress (Fn_00760470).

### 16.4 Load order (construction.md 8.4: derived, not scripted)
The level's requests now come from the requesting code: MXGame::Update state 3 -> the level database (Database vf5 with the DLC
path 0053c740), 00756060 (Shared, <level>Base), 007560f0 (vf16 per package), state 4 polls them (vf9 / vf10), FUN_00759f00 runs as
the loader's job in the original and at once in the product; the lap pacing asset (Database vf27) and the activity subscenes follow in
the original's order.

### 16.5 API for the core session (stable)
```c
rx_event_spec e; rx_event_spec_default(&e); /* fill from the front end */ rx_event_set_spec(&e);
rx_event_set_level(fmt_level_open(in, e.location));                 /* the subscene / directory reads of the product */
rx_ensure_local_players(0, 0); rx_setup_event(ui); rx_start_event(ui);   /* MXGame state 3 */
while (U32(game, 0x18) != 2) rx_mxgame_update(game);                 /* the load: states 3 .. 0xb */
for (;;) rx_mxgame_update(game);                                     /* the race frames */
```
plus the pieces for other units: `rx_event_channel_create`, `rx_event_type_register`, `rx_event_handler_list_push_back`,
`rx_world_construct`, `rx_mxworld_vf3`, `rx_activity_on_player_joined`, `rx_race_driver_construct`, `rx_race_stub_vf47`,
`rx_game_setup_activity_area`, `rx_players_sync`, `rx_player_from_player_list`, `rx_mxplayer_init`.

### 16.6 The end-to-end driver
`tools/build_event_test.sh <out>` builds `build_event_test <install> <prefix> [frames] [level]` from src, every game file that
compiles, rx_canon: memory, image, `rx_globals_init` (12b), the spec / level, the event's setup, MXGame::Update until the race runs,
the race frames, `<prefix>-canon.bin` / `-world.bin` / `-anchors.txt` for rx_canon_cmp.py against a baton boot with
REFLEX_WORLD_CANON after the same frames. Today it stops in 12b's static initializers (unported 005a2a04, rx_fn), before unit 8.

### 16.7 Left (unit 8)
- **2026-10-19 15:10 update:** `00536ef0` (the `.tdf07` AI racing-line / track-section loader) and the FreeRide constructor
  `0055fdb0` are ported and verified (`build_activity_track.c`, `build_activity_freeride.c`; see
  `core/notes/build_activity_track.md`). The two loader call sites are switched. The remaining list below is still the
  original's where it says so.
- **2026-10-20 b11onfix update:** the `rx_build11` On port boot runs end to end now. The frame-0 stops of the 01:5x CLIs were fixed:
  `build_series_init.c` must call the UI manager's `005dc230` **this-call** (ECX = `[0xa93fcc]`); the old `RX_STDCALL` left ECX as the
  previous call's residue and the original's `lea esi, [ecx + 0x134]` then divided by a garbage bucket count in the UI property map
  (005b2370 division by zero) or read `[0x133]` with ECX = -1 (005b0620). The other new Verify entries were chain4's in-flight ABI bugs
  (005c3310's tail-jump thunk stack, 00581f50 through the same virtual tail, 004dd3d0's old `act_cell` player-list helper). Acceptance on
  the live tree: boot ref / port / Verify start (0 / 102 / 98 ports); Verify 98 ports, only the three documented dead-stack addresses
  (00564b20 / 0055ff70 / 00767200, two spellings each); Verify boot world identical; canonical world 11 differing nodes, 0 only-in-A/B
  (known-differences 6); race_compare 52/52 IDENTICAL with equal state hash (16:23 re-run identical; logs `builds/b11onfix/la2`).
  Details: QUEUE "b11onfix".
- Still the original's (oracle) / fatal in the product, in the order the race reaches them: the activity start chain
  (MXActivityRace::vf2 00564b20 -> 00566170 -> Activity::vf2 00556ec0: the activity's channel and ~45 event types, the race
  states "Pre" .. "Inactive" (005585e0), vf4 00566310, 00588130 / 005881f0 / 00588900 handler registrations, 005666d0 / 00564c90 /
  00564bc0 the mode's start, 00566260 / 00566da0), the grid pass 0075f890 and the start 0075e020, RaceDriver::vf3 0050f090 (the
  start reset), Activity::vf50 0055a4c0 / 00557fe0 / vf52 00565640, MXRecords_Join 00764170, 0075f2b0 / 0075f500 / 0075d760 /
  0075f5a0 (the load checks), 00766930 / 00757dc0 (the UI world's activities and players left; render-sync reset), 007d20b0 (the
  scenes' active flags: scene unit), 0053c350 / 004dc0a0 / 0053c3d0 (state 0 / 1: the init thread, the MXGame unit's), 0084ea00 /
  0081f290 (engine frame bookkeeping: loop unit), 006e0c50 / 00402540 / 005ee840 / 00548ad0 / 00548b40 (the pause / controller
  state).
- Not reached in the reference boot (abort): online / split-screen / series / championship paths, other activity classes than
  MXActivityRace (FreeRide 0055fdb0 runs the original), the restart states 9 / 10, a player joining an online or inactive activity.
- Product-only paths not checked by Verify: 007562b0's directory from fmt_scene, 007564f0 via rx_scene_load_subscene, 00766110 without
  the audio wait, the UI answers (accepting) of MXGame_ResumeFromUI.
- fx_world.c's session tick 006de690 models the callee-saved registers from its own adapter: the oracle build of MXGame::Update calls
  the original instead (the fx owner may want a C-callable entry).
