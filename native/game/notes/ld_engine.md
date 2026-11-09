# ld_engine (0x840000..0x8e0000): the Database async loader thread, engine startup

Wave 4 (native loading), 2026-10-18. Files: `ports/ld_engine_84.cpp` .. `ld_engine_8c.cpp` (lifted with `tools/lift/gen.py --list`,
captured backend), `ports/ld_engine_loader.cpp` (the loader thread procedure, lifted, then LIFT_CALL -> LD_CALL_UNBOUNDED),
`ports/ld_engine_util.h`.

## What is ported

275 of the group's 279 unported functions of plan/loading-waves.tsv (26.73 of 26.77 M boot instructions, loader thread 90 %): every
one lifted exactly (0 refused after the lifter extensions in ld_database.md, 0 hand-written; regenerated after the lead's imul fix). Top: FUN_00882cf0 (12.4 M), Database::vf5 (6.6 M),
FUN_008434f0 (4.0 M), Database::vf9, FUN_0084df50, Database::vf30 (request dispatch), XMLBuilder::vf7 / vf8 / vf10 / vf103.

Left original:
- **Engine::Frame 0084f260**: the boot's frame-stop native sits at its address (boot.cpp ENGINE_FRAME, the main thread parks there
  between frames) and does not fall through to a port; notes/loading.md says keep it unported.
- **00883ab2, 008aebc0** (`jmp [IAT]` import thunks: D3DXCompileShader, GetCurrentThreadId) and **008a0500** (`jmp 008aebc0`): owned
  by ports/thunks.cpp (group "thunk", always on). loading-waves.tsv lists them as unported; a second registration at the same address
  would shadow the thunk, so they are not in the ld_engine files.

## The loader thread procedure 0084c760 (Database_LoaderThreadProc)

`while (running) { EnterCriticalSection / LeaveCriticalSection over the request ring; switch (request kind) { 1: Database vf8 (load)
... ; 2/4: Sleep(1) polls of the engine frame counter; 3: FUN_007fa250 under a critical section; ... } }` and waits for the next
request (DatabaseThreadEvent, loading.md section 1); imports through the IAT slots 0091600c / 009160f8 (critical sections), 0091601c
(tick count), 00916020 (Sleep), 00916034 / 00916078 / 00916090 at the end. Under baton its waits block inside the stand-ins (no LIFT_CALLB needed: the boot switches threads inside the
import and returns normally). In race frames the loader thread never runs (runtime.cpp: Database requests are never serviced).

Trap: lifted plainly it stops the boot with `instruction budget`: LIFT_CALL -> Machine::call_function runs a nested call with the
default 2e9-instruction budget, and one request (a package load with its inflate) runs longer than that on the loader thread (the
Concord boot stopped at frame 307 after 5.6 G). The port therefore calls through `LD_CALL_UNBOUNDED` (ld_engine_util.h: the same
push(0) / run / return-slot rewrite, budget UINT64_MAX / 4 as the boot's own thread runs). When Guest::call / call_function get the
boot budget (loading.md section 3 item 3), the plain lifted form works and the helper can go. Any other port high in a loader call
tree (a package-load caller) needs the same.

## Trap: ports that are on a guest stack when the world is captured

Two ports never return during a boot: **ReflexWinMain 008814a0** (the main loop: `do Engine::Frame() while ...`, main thread) and
**Database_LoaderThreadProc 0084c760** (loader thread). LIFT_CALL leaves 0 in the return-address slot of the call in progress (the
nested-run sentinel of call_function; the original's address is written back only after the call returns), so the captured world
image has 0 instead of the original's return address in exactly those two live slots: main `[0022ff04]` (original 0088189f, the
call of Engine::Frame) and loader `[130fffa4]` (original 0084cc2d, its wait). The canonical compare is IDENTICAL (code addresses
are not heap names) and the race does not return through them (the world test drives Engine::Frame directly; the loader thread is
never run in race frames), so the worlds race identically; but an image is not resumable through those frames (it never is through
a native). With `-ReflexWinMain,-Database_LoaderThreadProc` the world images are byte-identical (bar the known dead data).

## Acceptance (2026-10-18, build with -fwrapv, regenerated lifter output)

- `loading_accept.sh <boot> ld_database,ld_engine 1` (Concord) and `... alpine`: canonical VERDICT IDENTICAL, race VERDICT IDENTICAL
  (52 samples), race hashes identical; world images differ in 2 live-stack words (the trap above).
- `loading_accept.sh <boot> all,-MXGame__Update` (all committed groups + these two; other wave-4 groups' files were not in the build):
  Concord: 1 differing node = the documented dead-data buffer (+5835c bfa0ae7d / 0022fc20, loading.md), race IDENTICAL, hashes
  identical; Alpine: IDENTICAL, race identical.
- race `verify.sh <boot> math,ld_database,ld_engine`: 0 mismatches, on identical, all five worlds.
- race `verify.sh <boot> all`: on identical on all five; mismatches only inside Engine__Update: [148dbd9c] (dead-stack.md, 006b0530)
  and on Terracross 121 calls of 1 vs 3 in 128-byte records at 0x142d7xxx: the same 121 with `all,-ld_database,-ld_engine` (pre-existing,
  not these groups).
