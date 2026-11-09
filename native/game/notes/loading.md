# loading: converting world creation (the boot) to native code

Status 2026-10-18 (loading design agent). What runs original code once every race frame is native is world creation: the Boot
(`native/drafts/world_runtime/boot.cpp`) boots MXReflex.exe, runs the front-end calls of `start_event` and snapshots the world
(`write_world`). This note has the measurements, the scheduling design (prototyped in boot.cpp: `REFLEX_BOOT_SCHED=baton`), the
acceptance tools and the wave plan for the ~5,700 loading functions.

**Update 2026-10-18 afternoon (loading infra agent):** per-call Verify works inside baton boots (section 3a: `REFLEX_PORT_VERIFY=1`,
`VERIFY=1 tools/loading_accept.sh`); the baton scheduler's time rule changed (a lonely yield costs 10 us instead of jumping to the earliest
timeout) and `start_event` waits for the loading page's logo video under baton, so **every baton reference world changed**: they are
now captured in the event intro (race state 0, as the exact worlds), the race starts after ~720-1200 frames (race 1500 frames or more),
and Supercross races (section 6). `REFLEX_EVENT_SPEC` takes empty fields (FreeRide). Preparation of the Unity switch: section 7.

## 1. Findings: the guest threads of a boot

Tool: `REFLEX_BOOT_THREADS=<trace>` (boot.cpp, observation only; the default world stays byte-identical) and
`tools/boot_threads.py <trace>` (per thread: instructions per phase, how its runs end, where it blocks, where it is preempted, what it
signals, polls). Concord (default event) with the exact scheduler, 11.2 G instructions (exe + CRT DLLs) to "world running":

| thread (start) | kind | instructions | how it runs |
|---|---|---|---|
| main (WinMain) | Engine::Frame loop, front end, event setup, loading page | 2.09 G | blocks per frame on the engine task thread (Engine::Update+315), the render-sync workers (007e0fc0), `LoadingThreadEvent` (MXGame::Update+3b6, timed), Sleep(1) polls of the Database (0084a826 `do Sleep(1) while !vf28`), yields in XenonModelGroup::vf0_o90 / RiderAnimation_WaitJobs; 210 slice ends, mostly in the ModelGroup spin (below) |
| 0084c760 | Database async loader | 8.65 G (77 %) | requests via `DatabaseThreadEvent`; parses packages, inflates (zlib 1.2.3 / LZX), builds assets, runs their post-load code (UI objects too); preempted 2,142 times (CPU bound, mostly in 008f90b0); blocks 78 times; once on a critical section owned by main (Database::vf8) |
| 00760400 | game init thread (startup tables) | 120 M | Sleep(0) polls of the Database (Database::vf15 / vf4, 0075ed60); exits |
| 0061cdb0 | rider animation worker | 165 M | go / done events (as in the race runtime) |
| 00881900 x2 | engine job pool | 121 M | Wake0 / Wake1 (the scene ticker 1040 signals Wake0 10,573 times) |
| 007df370 x12 | render-sync workers | 87 M | one go event each |
| 007a5a70 x3 | ModelGroup workers | 1 M | `ModelGroupStartEvent<i>` / `ModelGroupEnd<i>` |
| 007d22a0 x3, 0084f320, 00806f90, 005a7f10, 00662ec0 | scene tickers, engine task thread, FMOD queue, saved-game manager, forest cull | < 10 M each | event driven |

Alpine (FreeRide) is the same picture (10.2 G; loader 8.3 G). Per-thread coverage (`REFLEX_COVERAGE_THREADS=<prefix>`, one
coverage file per thread start address) attributes the exe instructions: loader 7.9 G, main 1.26 G, rider animation 194 M, init 53 M.

**Preemption is relied on** (spin-waits that only end because another thread gets CPU):
1. **ModelGroup dispatch, FUN_007a59e0**: loops `WaitForSingleObject(end[i], 0)` over the three workers' end events until one is free:
   49,076,188 polls in the Concord reference boot (321 in Alpine). Without a time slice the main thread spins forever.
2. **UIManager::vf30 (UI startup)**: requests the `DefaultPackages` and then `do done = FUN_005dbe50(last package) while (!done)`: a pure
   memory spin (database lookups, no wait, no Sleep) until the Database loader has loaded the package.
Measured: `REFLEX_SLICE=1e15` (no preemption) hangs in (2) right after opening MXUI.dx9.package; `REFLEX_SLICE=2e9` crawls through (1).

**The game tolerates any time slice**: exact boots with 1 M, 4 M (default), 16 M, 64 M and 256 M instruction slices all start the
Concord event (world running after 1,571 / 558 / 247 / 170 / 151 frames); the worlds differ (not compared). Virtual time in the
reference boot is instruction-driven (1 GIPS: `advance_time`), so instruction counts decide both the interleaving and every timeout.

**The core problem, confirmed**: a native cannot be suspended inside a nested `Guest::call` (a slice end or a blocking stand-in makes
the nested `Machine::run` return early: the import returns 0 / `sliced`), and natives change instruction counts, hence interleaving,
timeouts and the world image.

## 2. Options

| | determinism | faithfulness | effort | blocking natives |
|---|---|---|---|---|
| (a) **baton**: a host thread per guest thread, one runs at a time, switches only at synchronization; logical time | schedule independent of instruction counts: identical for original and ports that make the same calls | a legitimate schedule (zero-time CPU and I/O, switch at sync points); needs the two spin sites marked as yields and the front end given its time | prototyped (~250 lines in boot.cpp) | yes, at any depth |
| (b) single-threaded loader (async jobs run inside the requester's SetEvent, as runtime.cpp does for the race workers) | deterministic | schedule changes completely; deadlocks where a job needs a lock its requester holds (the loader waits on a critical section owned by main, Database::vf8), or another thread (Audio_PostCommandWait waits for the FMOD queue thread); per-protocol code for 13 thread kinds | large, fragile | no (a nested job that blocks deadlocks) |
| (c) exact slices, slicing off while a native runs | deterministic per port set only | reference schedule only while nothing is ported; every port shifts slices, so worlds differ like exact vs baton below (async completion order, start gate, race moment): no automatic acceptance | small | no (blocking inside a native still breaks) |
| (d) baton + preemption at import calls by a logical quantum | instruction-independent | adds fairness if some race needs it | small on top of (a) | yes |

**Chosen: (a) baton**, with (d) kept in reserve. (b) is the shape of the *final* native loader (Open Unleashed has no emulator: a
native loader can be synchronous by construction), not the vehicle of the conversion.

## 3. The baton scheduler (prototype in boot.cpp)

`BootConfig::baton` / `REFLEX_BOOT_SCHED=baton` (default off: the exact scheduler, reference worlds unchanged).
- Each guest thread runs on its own host `std::thread` (lazily started); one baton (mutex + per-fiber condition variable). The driver
  (the caller of Boot's functions) is a fiber too. `park()` / `yield()` / `switch_to()` / `pick()` / `schedule_baton()`.
- A thread gives the baton away only where the game synchronizes: a wait that does not complete at once (WaitFor*, EnterCriticalSection,
  Sleep(n)), Sleep(0) / SwitchToThread, a zero-timeout wait that times out (a poll: the others run first, then it looks again), thread
  exit, the frame stop (back to the driver). Round robin from the next thread, as the exact scheduler. The blocking stand-in simply
  returns its result after the switch (`block()`), so the switch can happen at any native call depth.
- Spin sites: a table of functions called in a memory spin-wait (now one: `0x5dbe50`); their entry yields. A port at the address runs
  after the yield (`reflex_port::installed`). `REFLEX_BOOT_WATCHDOG=<n>` reports a thread that runs n instructions without a switch (how
  0x5dbe50 was found; `REFLEX_BOOT_WATCHDOG_TRACE=<file>` adds 1 M interpreted pcs).
- Logical time: `now_` advances by 10 us per QueryPerformanceCounter read (the frame-cap busy wait makes a frame 1/60 s), by 10 us per
  lonely yield (Sleep(0) / SwitchToThread / a failed poll while every other thread waits), and to the earliest timeout when every thread
  waits. Not by instructions. (Until 2026-10-18 a lonely yield jumped to the earliest timeout: ForestSystem_CullThread's failed quit poll
  (00662ef5, once per frame while main waits for ForestSystemEnd) then made every frame after loading last ~1 s, up to the saved-game
  manager's timed wait; National and FreeRide worlds were captured 47 s of game time after "running", in the countdown.)
  `REFLEX_BOOT_THREADS` traces record the jumps (`W`) and lonely yields (`V`); `tools/boot_threads.py` sums them.
- Capture moment: after "world running" + 60 frames, `start_event` under baton also waits (at most 600 frames) for the loading page's logo
  video, BinkOpen of `Videos\Nordic_Logo_wSound.bik` (every event opens it after GotoLoadingPage; the exact boot does during state 4,
  long before its capture; under baton it comes ~frame 418, after the old capture at 400). The world runtime has no Bink stand-in
  (`unmodelled import binkw32.dll!_BinkOpen@8`, return 00595bd6, XenonBinkVideoManager::vf1 from UIVideoComponent_Play / UIPageGraph):
  that was the Supercross stop. Baton worlds are now written at frame ~419, 7.0 s virtual.
- Per-thread instruction counters (`Machine::steps` is the running thread's own), so the 2e9 budget of a call a native makes counts only
  its own thread.
- Per-thread Machine modes saved at a switch: `stop_pc`, `stop_esp`, `jit_enabled` (Verify's original run).
- `start_event`: under baton, loading costs no virtual time and the front end is reached after ~1 s instead of ~5 s; the front end must
  run its timed steps first (the startup packages' asset binding, AssetHandle::vf5 at frame 77 / 1.8 s in the exact boot): settle until
  5 s virtual. Without it the loader thread crashes at state 3 in FUN_007fd690 (font handle `RoadkillAlternates56` unbound, `[+0x44]`
  null, +0x60 read).
- Check of the machinery: `REFLEX_BOOT_SCHED=baton-sliced` (baton fibers + the exact scheduler's slices and instruction time) gives
  the reference world byte-identical (`cmp` with scratchpad/bj/ci-world.bin).
- Speed: a baton boot of Concord takes 10-16 s (exact: 14-33 s): no exact-slice JIT exits; loading states 3..7 take 3 frames instead of
  ~250.

Results (scratchpad/loading):
- Deterministic: two baton boots give byte-identical world images.
- All 31 arcade events start under baton; all but Supercross race in the CLI world test (sweep, section 6).
- **Ports in the Boot**: `REFLEX_PORT=all,-MXGame__Update` installed for the whole baton boot of Concord: 1,805 ports called 44.2 M
  times during the boot; the event starts; the world equals the original baton world except 2 words of one 640 KB buffer (dead stack
  data copied by original code: `0x0022fc20`, a main-stack address, where the original left `bfa0ae7d`) and dead stack pages; racing
  both worlds 1,500 / 1,800 frames (REFLEX_DRIVE 1) gives identical bike state and whole-memory hashes, also with REFLEX_PORT=all in
  the race. Alpine: 1,764 ports, canonical dump IDENTICAL, races identical.
- One race port is invalid on loading paths: **MXGame__Update (00757fb0, ai_gen_75.cpp)** throws `ai port: unexpected control flow at
  0x007582d4`: the old ai lifter kept only the game-state jump-table targets seen in race frames; game states 3..11 (loading) are
  missing. Re-lift it with tools/lift (full jump tables) in the first wave.
- The exact and the baton worlds are different worlds (world_compare: 61 % of the common bytes in identical nodes; async completion
  order differs in UI lists). Both are now captured in the event intro (race state 0); the baton world's countdown that this note
  reported before was the time-jump artifact above. The conversion uses baton references throughout; switching Unity's worlds to baton
  boots is a separate decision (every race hash changes): section 7.

### 3a. Verify inside the boot (done 2026-10-18, loading infra agent)

`REFLEX_PORT=<filter> REFLEX_PORT_VERIFY=1 REFLEX_BOOT_SCHED=baton reflex_boot ...` verifies every top-level call of the selected ports
during the boot, as verify.sh does in race frames (native first, then the original on the same entry state, the original's result kept).
- **No switches inside a verified call** (vm.h, port.cpp): `Machine::no_switch` is > 0 around both runs of `verify()`. Where the boot
  would switch guest threads it throws `reflex_vm::WouldSwitch` (vm.h; not a std::exception): `park` / `yield` / `preempt`, a wait
  that does not complete at once (`block`), a failed zero-timeout poll and Sleep(0) / SwitchToThread (they yield), spin-site yields,
  thread exit, the exact scheduler's switch requests, and the Engine::Frame native (a verified call that runs a whole frame).
  - The native run would switch: memory (journal), registers and host state are put back and verify returns RUN_ORIGINAL, so the
    caller's run loop executes the original with switches allowed (ports it calls are verified one by one). Counted as
    **unverifiable** (port report: `... mismatches N unverifiable U`, after the mismatch count so verify.sh's fields stay; not a mismatch).
  - The original run would switch where the native did not: put back the same way, the original runs free, counted as a **mismatch**
    ("the original would switch threads (blocks / yields), the native did not").
  - A native that throws a std::exception: as before (the original runs, counted as a mismatch "native threw").
- **Host-state undo** (boot.cpp `host_snapshot` / `host_restore`, registered with `reflex_port::set_host_state`): while `no_switch > 0`
  every change of the boot's host state is logged with its undo and replayed backwards by the restore (which Verify may call twice):
  heap (free lists in their exact multimap order, blocks, sizes, arenas, pool tops, VirtualAlloc reservations, HeapCreate / HeapReAlloc
  in place), handles (created, closed: a file's fd closes only once that can no longer be undone, `commit_closes`), event / mutex state,
  file positions, find handles, critical sections, TLS slots, created threads (not started before their first switch), ResumeThread,
  import sentinels (GetProcAddress), host code addresses, the current directory, the D3D9 device (its scalars once per undo epoch, each
  resource's refs / version / surfaces before a change, created resources, remembered render state), the D3DX constant tables and
  buffers, DirectInput devices, the FMOD objects and fmod_stand_ins.inc's state (lazily, once per epoch). Scalars (virtual time,
  last_error, the physics context, the running thread's wait fields) are saved per call. What cannot be undone throws WouldSwitch
  (unverifiable): file writes / creation (CreateFile for writing, WriteFile, SetEndOfFile, CreateDirectory, DeleteFile), VirtualFree,
  the Steam cloud stand-ins of career boots.
- **Budget**: `Machine::call_budget` (vm.h; call_function and Verify's original run) is UINT64_MAX / 4 in the boot (2e9 elsewhere).
- **Proof** (Concord and FR_Alpine, baton): Verify of the race ports (`all,-MXGame__Update` minus the ld_* groups) runs the full start:
  682 / 656 ports called, 0 mismatches except MXWorld__vf5 below; **the Verify boot's world image is byte-identical to the original
  baton world** (also with every port of the tree installed, the in-progress ld_* ones included: 909 ports, 3.6 M calls, 5,675
  unverifiable), i.e. the undo is exact for everything those boots do.
  - **MXWorld__vf5 007674b0 (engine_mt), 5 calls on a loading path: EDX** (native 009e30d4, original fffffffc). Bisected: the EDX that
    Kinematic_Integrate 0076a360's port (world, declares no outputs) leaves where the original leaves 0xfffffffc flows out of vf5,
    whose EDX counts as live only because vf5 is address-taken (live.tsv: ABI eax, edx, x87). A dead-register case like the dead-stack
    ones, as far as the vf5 callers are concerned; fix in the port (set EDX as the original) or mark vf5's EDX dead after checking its
    callers. With vf5 alone, or vf5 with engine_mt / rest: 0 mismatches.
- Inside a Verify boot no thread switches during verified calls, so `g_phase` stays global; REFLEX_BOOT_WATCHDOG (slices in nested
  runs) must not be combined with Verify.

### 3b. Remaining notes for the lead

1. `install()` does not replace a native the boot put at a port's address; the boot's natives fall through to `installed()` (done for
   0x769b80 and the spin sites). Engine::Frame 0x84f260 has no port; keep it that way or make the frame-stop native fall through.
2. The plugin (`reflex_world_build`) keeps the exact scheduler until the lead switches; `BootConfig::baton` is the switch (section 7).
3. lift_rt.h `lift::import_target` (13:34) takes IAT targets >= 0xf0000000 as sentinels: the boot's import sentinels are at
   0xec000000.. (boot.cpp SENT; the world runtime's at 0xfe000000), so in boots a lifted call through the IAT is not recognised as an
   import call (it runs as a plain LIFT_CALL with 0 in the return slot). Use `m.import_names.count(t)` for any t outside the exe.
   **Fixed 2026-10-18 (lead):** import_target now checks `m.import_names` for every target >= 0x916000.

## 4. Acceptance for loading ports

Layouts may legitimately differ, so acceptance is semantic, but under baton a faithful port (same calls in the same order: the lifter's
output) keeps even the layout: expect IDENTICAL.

- `REFLEX_WORLD_CANON=<file>` (empty: `<world>-canon.bin`) makes reflex_boot write **Boot::write_canonical**: every heap block (and
  non-heap VirtualAlloc region) reached from the modules' writable sections and the threads' TLS (then the live stacks), named by its
  first breadth-first path from those roots (64-bit hash of parent name and field offset; names travel along pointers to a block's start
  first, then interior pointers), with its RTTI class, size, plain words and pointer words as (target name, offset). Text (four printable
  bytes) is a pointer only to a block start; device memory only aligned; data blocks (no vtable, >= 64 KiB, pointer hits mostly not at
  block starts: file buffers, packed tables, textures) are leaves. Unreached blocks are kept as a multiset. ~170 k nodes, 50 MB gz.
- `tools/world_compare.py A-canon.bin B-canon.bin` -> per class identical / differing / only-A / only-B nodes, examples with the path
  from the root and the first differing words; `VERDICT: IDENTICAL` (exit 0) or DIFFERENT. `--images A-world.bin B-world.bin`: page diff
  of two world images (stacks are dead: informational).
- `REFLEX_RACE_STATE=<file>` in the world test (boot_main.cpp): every 60 frames per slot the render bodies, gear, rpm, speed and hashes
  of the simulation / rider objects with address words left out, the race state and every driver's place, laps and times.
  `tools/race_compare.py A B` -> IDENTICAL or the first differing frame and the largest body distance per slot.
- `tools/loading_accept.sh <reflex_boot> <filter> [1|alpine] [frames] [drive]`: baton boot original vs with the ports, canonical
  compare, image diff, both worlds raced (verify settings, REFLEX_DRIVE), race_compare and the state / memory hashes.
  `VERIFY=1` adds a third boot in Verify mode (section 3a): the ports with mismatches (first difference) and with unverifiable calls,
  and a byte compare of that boot's world with the original's (must be identical: the Verify boot keeps the original's results).
  Use it to localise a VERDICT DIFFERENT: the mismatching port is the place to look. Races: the baton worlds start racing after ~720
  (FreeRide) to ~1200 (Supercross) frames; the default 1500 frames covers the start.
  Rule: VERDICT IDENTICAL and race IDENTICAL; a difference is accepted only when it is dead data (uninitialized stack copied into a
  buffer, as `notes/dead-stack.md`) and the race hashes still agree; list each such case here.

Prototype runs: reference vs itself IDENTICAL (exact boots and baton boots; race identical). Reference vs REFLEX_PORT=all (race ports):
Concord 1 node / 2 dead words, race identical; Alpine IDENTICAL, race identical. Sweep of all 31 events: section 6.

## 5. Wave plan

`plan/loading-waves.tsv`: the 5,664 functions of the Alpine boot (plan/boot-alpine-functions.tsv) with instructions, the threads that run
them (per-thread coverage, %), whether a race wave already ported them (1,538 are, 1.21 G instructions), and a group. Unported: 4,126
functions, 8.2 G instructions.

| group | range | functions (unported) | unported instructions | runs on |
|---|---|---|---|---|
| ld_core | 400000..4d0000 | 520 (255) | 7 M | main 86 %, loader 9 % |
| ld_tables | 4d0000..540000 | 476 (356) | 15 M | loader 83 %, main 13 % (MXTable helpers, table rows) |
| ld_frontend | 540000..5c0000 | 497 (357) | 17 M | main 94 % (MXUIManager, events, activities) |
| ld_ui | 5c0000..600000 | 710 (573) | 46 M | loader 56 %, main 44 % (UIComponent/Container/Manager; spin site 005dbe50) |
| ld_entities | 600000..700000 | 881 (544) | 41 M | main 67 %, init 16 % (entities, forest, fonts) |
| ld_game | 700000..780000 | 445 (362) | 6 M | loader 71 %, main 28 % (MXGame::Startup, MXGame__Update re-lift) |
| ld_models | 780000..7c0000 | 411 (368) | 199 M | loader 58 %, main 41 % (models, ModelGroup 7a59e0 / 7a5a70) |
| ld_render | 7c0000..7f0000 | 249 (181) | 244 M | loader 58 %, main 27 %, init 14 % (terrain / render setup) |
| ld_assets | 7f0000..810000 | 359 (292) | 490 M | main 84 % (TextIcon, asset handles, CompressedInputStream) |
| ld_database | 810000..840000 | 361 (271) | 210 M | loader 95 % (Database, XMLReader, ZLib/LZXCompression wrappers) |
| ld_engine | 840000..8e0000 | 462 (279) | 27 M | loader 90 % (Database thread 0084c760, engine) |
| ld_inflate | 8e0000..900000 | 87 (82) | 6,908 M | loader 100 % (zlib 1.2.3 inflate 008ea630, LZX 008ef330/008f04c0, 008f90b0) |
| (thunks) | 900000..920000 | 206 | 0 | import thunks `jmp [IAT]`: not ported |

Notes for the agents:
- Lift with the shared lifter (`tools/lift/gen.py --list <list.tsv> --group <group>`), most expensive first; hand-port only what it
  refuses. The 0x8f0000.. range was the math agent's in the race waves: coordinate ld_inflate with it. zlib / LZX can later become host
  zlib 1.2.3 / libmspack lzxd (identical output bytes), but not in this wave: their internal allocations would change the layout.
- Acceptance per group: build (`tools/build_cli.sh <own dir>`), `tools/loading_accept.sh <boot> <group> 1` and `... <group> alpine`,
  plus the race `verify.sh <boot> all` (the group's ports also run in race frames where they are called there), and at the end a sweep
  over all events. Verify inside the boot needs section 3 items 1-2 first; until then acceptance is the whole-world comparison (a
  failing group is bisected with REFLEX_PORT filters).
- Ports must make the same import calls in the same order (the stand-ins and the baton schedule see them): blocking waits, Sleep,
  SwitchToThread, polls, file reads. A port of FUN_007a59e0 keeps its zero-timeout wait loop; a port at a spin site (005dbe50) stays
  behind the boot's yield (runtime native falls through to the port).
- Thread procedures (0084c760, 00760400, 007a5a70, ...) can be ported like any function under baton (their waits block normally).
- Order: ld_game (MXGame__Update re-lift, unblocks REFLEX_PORT=all), ld_inflate (73 % of the instructions), ld_database + ld_engine
  (the loader thread), ld_assets, ld_render, ld_models, then the main-thread groups (ld_ui, ld_frontend, ld_entities, ld_tables,
  ld_core). The career / tutorial / modes paths (career.cpp, tutorial.cpp) run more front-end code: cover them with their boots after.

## 6. Sweep (all events, baton, original vs the race ports)

2026-10-18 afternoon (after the section 3 changes): `scratchpad/loading/sweep2.sh <boot> <filter> [parallel]` (results in
`scratchpad/loading/sweep2.txt`): per event of events.txt a baton boot of the original and one with the ports (On mode, the whole boot),
canonical compare, then both worlds raced 1500 frames (REFLEX_DRIVE 1, the verify settings) and race_compare.py on their race states.
Filter: the race ports, `all,-MXGame__Update,-ld_ui,-ld_entities,-ld_models,-ld_frontend,-ld_tables,-ld_assets,-ld_engine,-ld_database,
-ld_core,-ld_render,-ld_game,-ld_inflate` (the ld_* groups in the tree are work in progress: `REFLEX_PORT=all` with them stopped a baton
boot after 84,778 instructions at 11:30, "execution outside mapped code bfa0ab61", while Verify of the same set keeps the original world).
- **All 31 events boot both ways and race**: race state 2 at frame 1500 in every original and ported world, race_compare IDENTICAL for
  all 31 (ported worlds race exactly like the original ones).
- Canonical: 23 IDENTICAL, 8 with one differing node (NAT_Stillwater / Concord / Bedford / ConcordB / StillwaterB / BedfordB,
  SX_Kingston / KingstonB): the dead-data buffer of section 3 (the 0x9c413-byte block, two words: original `bfa0ae7d` / `00000009`, ported
  `0022fc20` / `428de9ba`; checked on Concord, Bedford and Kingston).
- **Supercross** races now (8 of 8; race 2 at ~1200 frames): the capture moment waits for the loading page's logo video (section 3).
- **FreeRide** rows run as specified: boot_main's REFLEX_EVENT_SPEC parser splits at every comma (the empty duration type of
  `FreeRide,9,FR_Alpine,45,0,-1,,0,8` is taken as empty; a spec without 9 fields is refused with exit status 2). FR_Alpine / Gibson /
  Cheyenne are three different worlds now (140,306 / 189,807 / 150,099 nodes), not NAT_Concord.

The earlier sweep (`sweep.sh`, before these changes: FreeRide rows were Concord, Supercross stopped at BinkOpen) is in sweep.txt.

## 7. Switching the Unity client to baton worlds (prepared, not switched: the lead decides)

What changes when `reflex_world_build` (plugin_api.cpp) sets `BootConfig::baton = true`:
- **ReflexWorldFiles.Version** (Unity/Reflex/Assets/Scripts/Content/ReflexWorldFiles.cs): bump `"boot1"` to a new name (e.g. `"baton1"`):
  the cache folder is `ReflexCache/worlds/<Version>`, so every cached world is rebuilt once (old folder can be deleted). No change to
  EventSpec / the plugin ABI is needed; the world files (`-world.bin`, `-deform.bin`, `-deform-model.bin`) keep their formats.
- **World image header** (RXWI words 4.., boot.cpp write_world): same layout and magic ('FULL', 'FRME', 'BOOT'); values that change:
  word 2 the physics context (heap layout: e.g. Concord 1621e9f0 -> 16229df0), word 3 the engine dt at the capture (exact 0.0207 /
  0.0336 Kingston -> baton 0.0167 for every event), word 4 the Engine::Frame count (exact 492-584 -> baton 419), word 10 the riding
  rider (heap layout). cw / mxcsr / CRT bases / ptd / anim worker fields / engine are the same. The thread table (0x1000) has the same
  threads; the BSNT import table is the same. World sizes are the same (Concord 43,399 pages).
- **What the player sees**: both are captured in the event intro (race state 0); the intro runs in the world as before (race state 2 after
  ~720 FreeRide, ~1080 Concord, ~1200 Supercross frames with REFLEX_DRIVE 1; the exact worlds start after a similar intro).
- **Every race hash changes** (verify.sh, verify_install.sh and the plugin's reference worlds: scratchpad/boot/worlds, bj/ci-world.bin):
  the verify worlds would have to be rebuilt as baton worlds and the reference hashes renewed in one step with the switch.
- **Timings** (CLI, JIT, same machine load (~30), exact and baton run side by side; "written after" = boot + event + world write):
  Concord exact 24.7 s / baton 20.2 s, FR_Alpine 22.1 / 19.2 s, SX_Kingston 24.1 / 19.2 s; peak RSS the same (570-790 MB). The front
  end is reached after 46 frames under baton (0.5 s to the first frame, 67 M instead of 536 M instructions); loading states 3..7 take 35
  frames instead of ~250-330. A baton boot with the race ports in Verify mode takes ~1.5-2 min.
- **Other start paths** have their own front-end waits and do not yet run under baton: `tutorial::start` (tutorial.cpp) fails under baton
  ("no FrontEndPageGraph" after 46 frames: it needs the front end's 5 s virtual settle that start_event does under baton);
  `career::start_series_event` and `garage::start_test_drive` are untested under baton and need the same settle (and, if they write a
  level world after a loading page, the logo-video wait). Their owners should add `while (b.now() < 50000000) b.run_frames(1)` after the
  front end is reached when baton is on (or the lead keeps those paths on the exact scheduler: the boot config is per build).
- The CLI world test (and Unity's world runtime) still has no Bink stand-in; baton worlds no longer need one for arcade events.

## Checks

`tools/verify_install.sh noinstall` after the boot changes: the five verify worlds ok with and without ports, DEFAULT-WORLD-IDENTICAL
(the exact scheduler's default world is unchanged by every change above; the Verify machinery is inactive outside Verify mode).

## Files

boot.cpp / boot.h / boot_main.cpp (baton scheduler, thread trace, per-thread coverage, canonical dump, race state, port install:
`REFLEX_PORT` now also installs into the boot, with a warning under the exact scheduler), tools/boot_threads.py,
tools/world_compare.py, tools/race_compare.py, tools/loading_accept.sh (VERIFY=1), plan/loading-waves.tsv; Verify inside the boot:
vm.h (WouldSwitch, Machine::no_switch, call_budget), vm.cpp (call_function's budget), port.h / port.cpp (verify(), Stats::unverifiable),
boot.cpp / boot_devices.inc (host-state undo, switch guards, capture moment, lonely-yield time, W / V trace records), boot_main.cpp
(REFLEX_EVENT_SPEC parser; the world test prints the guest return addresses when a world stops); scratchpad/loading/sweep2.sh.

## Lead, 2026-10-18: return addresses on blocked threads' stacks in world images

- **What differs.** World images written after a ported boot differ from the original's in the return-address slots of frames on
  threads blocked at the capture: render-sync, ModelGroup and forest workers, the saved-game thread.
- **Why.** LIFT_CALL / LIFT_CALLB run a callee through Machine::call_function, which pushes 0, and write the real return address
  only after the call.
- **What changed.** Calls into imports (an IAT sentinel or a jmp [IAT] thunk) now push the real return address and stop on it
  (lift_rt.h import_target / call_import).
- **Why not every call.** Calls into original guest code cannot: the JIT's register-exit cache continues across a ret without
  returning to Machine::run, so a stop on the return address is not guaranteed.
- **Impact.** None on what matters: the race runtime restarts workers from their entry on fresh stacks (the words are never read),
  the canonical dumps and the races are identical, and the difference disappears with native loading (no images).
- **Acceptance.** Treat image page diffs that are only such slots as expected; world_compare's canonical verdict and race_compare
  are the acceptance.

## 8. Real return addresses in nested calls (lead, 2026-10-18)

The rule "functions on a blocked thread's stack at world capture stay original" is obsolete. `Machine::call_at(target, ra)` (vm.cpp)
pushes the original's return address and runs until the call returns there (stop_pc / stop_esp, saved and restored; the boot saves
them per thread); `Machine::tail_at(target)` does the same for tail jumps (the caller's return address stays at [ESP]). The JIT's
indirect exit (ret / jmp reg) now goes back to the dispatcher when its target is `stop_pc`, so a translated block never runs past
the return point (that was why only import calls used this before). Frame-allocating helpers that return with ESP below their slot
(__SEH_prolog4 008fa554, __EH_prolog3_catch 008fa66e, __alloca_probe 008fa230) stop at the return address whatever ESP is.
Used by lift_rt.h LIFT_CALL / LIFT_CALLB / LIFT_TAILJMP and the group helpers that know the return address (ai_util AI_CALL /
AI_TAILJMP, rest_util RST_*, crt_util CRT_CALL / CRT_TAIL, ld_engine_util LD_CALL_UNBOUNDED, collision_lift call / tail_call,
world_veh_util call, world_node_xc call). Not changed: Guest::call (hand ports), math_util, world_lit, world_rider_util,
world_wreck_* (no return address at hand; race kernels that never block).
Result, `loading_accept.sh all` (Concord, alpine, SX_Kingston, WaypointRace FR_Cheyenne): every blocked thread's LIVE stack in the
world image equals the original's; the remaining image differences (9-12 pages) are all below each thread's ESP (dead stack), plus
the known dead-data heap buffer. SavedGameManager_ThreadProc 005a7f10 is now ported (ports/ld_frontend_savedgame.cpp): only
Engine::Frame 0084f260 stays original in the exe during loading.

## 9. The Unity client on baton worlds and native boots, CPUID fixed (native switch agent, 2026-10-18 evening)

**What the plugin runs now.** Every boot libreflexworld makes runs under the baton scheduler with the native ports installed for the
whole boot, as the CLI's REFLEX_PORT=all boots did: `reflex_world_build` (arcade events, career series, test drive, tutorials, modes) and
`reflex_profile_open` (the profile session; its commands run in that boot). boot.h: `client_boot_config` (baton; `REFLEX_BOOT_SCHED=exact`
gives the old exact-slice boot without ports), `install_client_boot_ports` (before Boot::start), the setting `set_client_boot_ports` /
export **`reflex_world_boot_ports(filter, mode)`** (reflex_world.h: 0 the original code, 1 natives, 2 Verify; never called: REFLEX_NATIVE_GAME
if set, else 1, filter "all" / REFLEX_NATIVE_PORTS). Race worlds as before (`reflex_world_port`). Mode 0 is the original code path (the
baton boot of the original: the reference worlds below are built that way).

**Start paths under baton.** `Boot::settle_front_end` / `Boot::baton()` / `bink_opens()` / `wait_logo_video()` (boot.h):
- `tutorial::start`: the 5 s virtual front-end settle after state 2 + 30 frames (it failed with "no FrontEndPageGraph" without it). The
  tutorial world (UI_Compound, Freeride page, TutorialIntroductionPopup) is built at frame 382; ports 0 / 1: canonical IDENTICAL, races
  (900 frames) identical, world commands identical.
- career (`career::front_end`, used by `sign_in`: the profile session, `start_series_event`, `garage::start_test_drive`): the same settle;
  `start_series_event` also waits for the loading page's logo video under baton (it was already open in the series measured).
- Test drive (UI_Compound garage, TestDriveStart) and career series 3 (Waypoint Series 1, FR_Alpine track 1): built, ports 0 vs 1 image
  differences only in thread-stack pages, races identical with and without ports.

**CPUID.** vm.h `reflex_vm::cpuid` leaf 1: EAX 0x6fb, EBX 0x00010800, ECX 1 (SSE3), EDX 0x078bfbff (was EBX / ECX / EDX mixed up:
notes/ld5_hi.md). `[0x00dcf760]` __sse2_available = 1 in every world; FUN_008fa6b8 (the SEH-guarded movapd probe) runs and returns 1;
`_ftol2_sse` takes its SSE2 path (508,587 calls in the Concord baton boot, `_ftol2` none). msvcr90's own probe finds SSE2 too.
- **New CRT code in race frames, ported**: with SSE2 reported, the crt group's memcpy / memmove / memset ports and the `_CIpow` port reach
  msvcr90's SSE2 helpers, which nothing had run before: race frames of the new Concord world ran 14.6 M CRT instructions in 1,200 frames
  (12 k per frame; Alpine 1.0 M) as original code. `ports/crt_sse2.cpp` (group crt_sse2, lifted with crt_ld's DLL lifter, scratchpad
  switch/crtlift/gen_sse2.py): Fn_7854461e fastcopy_I (128-byte movdqa copy blocks), Fn_78545e17 fastzero_I, Fn_78545e73 _VEC_memzero,
  Fn_785a5799 the SSE2 pow kernel (645 instructions). After: 0 original instructions in race frames again (exe and CRT, Concord 1,200
  frames); `verify.sh <boot> crt_sse2` on the five new worlds: 0 mismatches, on identical.

**Re-baseline** (every reference changed; the old values are kept in verify_install.sh's comment):
- `scratchpad/boot/worlds`: the five verify worlds rebuilt by the plugin (reflex_client `build`, boot ports 0 = the original code, baton,
  CPUID fixed), same names; the old exact-slice worlds moved to `scratchpad/boot/worlds_exact_cpuid0/`. Specs: events.txt rows;
  `_ai11` = riders 12, AIDifficulty 95 (the client's full gate). National_NAT_Concord is byte-identical to the CLI's baton boot
  (`REFLEX_BOOT_SCHED=baton REFLEX_EVENT=1`). (The old SX_Springfield_ai11 was not reproducible from events.txt's spec with the exact
  scheduler either; Concord, Alpine and Verde were.)
- New reference hashes (verify settings; none = all, and the worlds the plugin builds with every port in the boot race to the same hashes):

| world | frames / drive | state hash | memory hash | old (exact, no SSE2) |
|---|---|---|---|---|
| National_NAT_Concord | 1800 / 1 | a936214c3ad9ccf1 | aff6eee02af59604 | 14fbb46b422c34d8 / f1a02da0f3546f3b |
| National_NAT_Concord_ai11 | 1500 / 2 | fd1d4b45b48e5a5e | 527c8a1386afeec3 | fc428ffca95782b6 / 62b9d1986dc7b8ba |
| FreeRide_FR_Alpine | 1500 / 3 | 1ef28933aa2c03c7 | e77600c0ba4fa124 | 9a5002d41e4ece6b / 9c6ce38c09cfa013 |
| SX_Springfield_ai11 | 1500 / 4 | fe8060ee3ca40a7b | 5f6b6c29e7e0024d | 210a0eafb60f5e69 / 9895014866da6382 |
| Terracross_TRX_Verde_ai11 | 1500 / 5 | 0f1c6fa1301d6af3 | 77f774138f2cb6f5 | 339c9eb6fa778d96 / 28623e7b63b760c9 |

- verify_install.sh: these hashes; DEFAULT-WORLD-IDENTICAL is now the plugin's own build (reflex_client, boot ports 0) of the default event
  against boot/worlds/National_NAT_Concord-world.bin (was: the CLI's exact boot against bj/ci-world.bin), plus PORTED-BUILD-RACES-IDENTICAL
  (the plugin's build with every port in the boot raced 1800 frames with every port: the reference hashes).
- verify.sh: `VERIFY_WORLDS=<dir>` overrides boot/worlds. On the new worlds `verify.sh all`: "on: identical" on all five; Verify mismatches
  only inside Engine__Update (the whole frame as one verified call): single float words ([152414cc] Concord, [1524d97c] Concord_ai11) and on
  Verde_ai11 28 words like [1524da1c] native 7ffffc60 / original float: the 006b0530 w-lane dead-stack case of dead-stack.md (the stack
  vec4's unwritten lane copied into a 16-byte store; 7ffffc60 is a stack address the natives leave there).
- **Dead data seen in the client's frame path**: a career (WaypointRace) world raced through `reflex_world_frame` (reflex_client) differs
  between ports 0 and 1 in one dword after 60 frames: 1525b7dc (StdMemoryAllocator block +0xac), original c4bbfe59, ported 7ffffc60, written
  by 006b0530's store (pc 006b067c, the same w lane). Not in the CLI world test of the same world; state hashes equal. The anim port
  (ports/anim_6b0.cpp Fn_006b0530) would need to reproduce the original's dead lane to make it byte-exact.

**Profile session and world commands (task: what original code the client's other paths still run).** Tool: `reflex_client`
(drafts/world_runtime/client_main.cpp, built by build.sh; a script of `boot_ports`, `build` / `testdrive` / `series`, `profile_open` /
`profile <command>`, `world_open` / `world_port` / `frames` / `world <api> <command>` lines through the C API) and
`tools/client_pair.sh` (none vs port in parallel, answers / save files / canonical dumps / images compared), command files
`tools/client/*.txt` (the client's sequences: ReflexProfile callers, OriginalWorldRuntime / OriginalEventIntro / OriginalInput /
OriginalLiveUi / OriginalCareerRace / OriginalTutorial / ReflexGarage). Coverage: `REFLEX_CLIENT_COVERAGE=<prefix>` (boot.h) writes
`<prefix>-build.bin` (a build), `-profile-open.bin` (boot + sign-in), `-profile-commands.bin` (the session's commands, at close),
`-world-commands.bin` (only inside the world command exports, at destroy), each with `.bin.dll` / `.dll.tsv`. Diagnostics added:
profile commands `canonical <file>` and `memhash`; export `reflex_world_memory_hash` (REFLEX_CLIENT_MEMDUMP=<prefix> also dumps the
pages for memdiff.py); REFLEX_CLIENT_WATCH=<addr>:<file> in reflex_world_create (as REFLEX_WORLD_WATCH).
Results (ports all): profile session (the 50 commands of client/profile_cmds.txt on the user's save): every answer identical, savegame.bxml and
the stats / persona files byte-identical, the session's canonical dump IDENTICAL (raw memory hashes differ: dead stack); arcade race world
commands (client/world_cmds_arcade.txt, 1530 frames), tutorial world (129 answers), career world (status / rules_done / results /
career_finish / persist): answers and saves identical, memory hashes identical except the career case above.

**What original code these paths still run** (ports all; `plan/remaining-client.tsv`: 594 functions, per path the instructions, from
gap_functions.py on the coverage runs; the next wave's list):

| path | exe functions | exe instructions | CRT DLL instructions |
|---|---|---|---|
| build Concord (arcade) | 3 | 24,673 | 873 |
| build tutorial | 174 | 91,458 | 361 |
| build test drive | 74 | 120,366 | 253 |
| build career series 3 (WaypointRace, trucks among the AI) | 278 | 7.29 M | 527 k (msvcp90 locale / num_get / time_put / collate) |
| profile open (boot + sign-in) | 65 | 86,420 | 253 |
| profile commands (client/profile_cmds.txt) | 192 | 260 M | 7,025 (wcscpy_s range) |
| world commands: arcade / tutorial / career / test drive | 131 / 120 / 48 / 9 | 54.0 M / 26.6 M / 42 k / 291 | 0 |

- By range: ld_inflate (0x8e..: the save's **deflate**, FUN_008ee4f0 / 008eea40 / 008edbb0 / 008f3b90 / 008ecdf0 ..., 223 M: every
  `save` and the persist commands that save) and ld_engine (the save writer: XMLBuilder FUN_0087be00 / 00875200 / vf127 / vf91 ...,
  114 M) are 99 %; then ld_core (87 functions: the **trophy truck / buggy physics** RavenSimulationTrophyTruck::vf32 / vf13 / vf16 / vf9 /
  vf15, RavenVehicleTrophyTruck::vf60 / vf62, VehicleDamage_SplitPart: the AI trucks of series 3's world, also in its race frames),
  ld_tables (the career pages' MXTableHelper IsLocked / IsStoreProductNew / IsStoreProductDLC / IsHidden, PurchaseStoreAsset,
  App_CalculateCareerCompletion), ld_game / ld_frontend / ld_ui / ld_entities (sign-in, storage, profile, tutorial and pages code).
  Engine::Frame 0084f260 (deliberate) in every boot.
- CRT DLL (with crt_sse2; before it the boots ran 10-50 M CRT instructions of fastcopy_I / fastzero_I / the SSE2 pow): what remains
  is msvcp90's std::locale / num_get / time_put / collate internals in the series boot and a few hundred instructions elsewhere
  (coverage .bin.dll files: scratchpad switch/cov2; switch/tools/dllfuncs.py attributes them to the nearest export).
- **Race frames of a career world are not all native**: career series 3's world (Waypoint Series 1, FR_Alpine track 1, as the plugin builds
  it from the user's save) raced 900 frames with REFLEX_PORT=all runs 71 exe functions as original code, 161 k instructions per frame
  (`plan/remaining-race-series3.tsv`): the trophy truck / buggy simulation and vehicle (RavenSimulationTrophyTruck vf9 / vf13 / vf15 /
  vf16 / vf32, RavenVehicleTrophyTruck::vf60, FUN_00457070, VehicleDamage_SplitPart ...) of AI vehicles the 31 arcade events never spawn.
  The arcade sweeps did not see them; the series events (career.md's table: Super Buggy, Trophy Truck classes) need their own race coverage.

**Acceptance after the merge (shared tree, 2026-10-18 evening):** `verify_install.sh noinstall`: the five worlds ok with ports none and all,
DEFAULT-WORLD-IDENTICAL, PORTED-BUILD-RACES-IDENTICAL. `verify.sh all`: on identical on all five, Verify mismatches only the Engine__Update
cases above. `loading_accept.sh all 1`: canonical 160,455 / 160,456 nodes identical, the one differing node the known dead-data buffer
(0x9c413 bytes, +5835c bfa0ae7d / 0022fc20), race IDENTICAL, race hashes 0194e03bfead6a7c / 6e89b2b26121122d (1500 frames, drive 1);
`loading_accept.sh all alpine`: VERDICT IDENTICAL, race IDENTICAL, 86a3f5b9cf3d6a55 / b851b4db74a0744b. `_ftol2_sse` Verify alone on the new
Concord world: 3,042,139 calls, 0 mismatches (SSE2 path); `verify.sh <boot> math` on the five new worlds: 0 mismatches, on identical.
Sweep of all 31 arcade events (CPUID fixed, shared tree, scratchpad switch/sweep/sweep2.txt, filter all): every event boots both ways
(world running at frame 418, 7.0 s virtual) and races (race state 2 at frame 1500 in all 62 worlds), race_compare IDENTICAL for all 31;
canonical 23 IDENTICAL, 8 with the one known dead-data node (NAT_Bedford / Concord / Stillwater / ConcordB / BedfordB / StillwaterB,
SX_Kingston / KingstonB): the same picture as section 6 before the fix.

**Unity client (2026-10-18 evening).** C# (applied to Unity/Reflex; it compiles against both the installed plugin and the new one, and with
the old plugin it changes nothing): `Content/ReflexNativeBoot.cs` (new: `reflex_world_boot_ports` with the race's native-game mode
REFLEX_NATIVE_GAME / ReflexNativeGame / Native and filter REFLEX_NATIVE_PORTS / "all"; `PluginBootsBaton` = the export exists);
`ReflexWorldFiles.Version` => "baton1" with such a plugin, else "boot1" (the cache folder: old exact worlds are never taken for baton ones),
`Build()` and `ReflexProfile.Open()` call `ReflexNativeBoot.Apply()` first. Bench (scratchpad switch-bench, productName ReflexSwitchBench,
the shared tree's new plugin): build_test.sh (player build, smoke race, crash test, support report) passes; batch tests FrontEndContentTests.
PlaySmoke, CareerTests.Create, PersistenceTests.Race, TutorialTests.Verify, RemapTests.Page / Race / Reset / Verify, HudTests.Play,
InputSettingsTests.Race, ModesTests.Play / Verify, AtvTests.Race, MudTests.Race, OptionsTests.Play, OpenFrontEndTests.Play, EventsTests.
Verify, ReflexPlatformChecks.Plugin pass; GarageTests.Play fails at its last step ("not back in the garage" after the test drive's Esc)
with the old plugin as well (pre-existing; the CLI driver shows the same UI flag change on old exact and new baton test-drive worlds:
ui+0x290 1 -> 0 after Esc once `pages com_stubs` is installed; without com_stubs the world stops at a boot COM sentinel ed00004c in either).
Logs: scratchpad switch/bench_logs.
