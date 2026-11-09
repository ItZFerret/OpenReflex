# ld_game (0x700000..0x780000): MXGame::Startup and the game's init, plus the MXGame__Update re-lift

Wave 4 (native loading), 2026-10-18.

## MXGame__Update 00757fb0 (ports/ai_gen_75.cpp, the ai group's)

The old ai lifter built its jump tables from the targets seen in race frames: the game-state switch at 007582d4
(`jmp [eax*4 + 0x759ccc]`, states 0..11) kept 6 of 12 targets, so loading states threw `ai port: unexpected control flow at
0x007582d4` and REFLEX_PORT=all could not run in the boot. Re-lifted in place with the shared lifter (captured backend, full
table: 007584f1 7585d1 758d08 7595ab 759606 759625 added); the other ports of the file stay as they were (the file includes
lift_rt.h next to ai_util.h; tool: scratchpad ldg/tools/relift_one.py). A scan of every committed port for `*_BAD` jump-table
defaults against the full tables the shared lifter finds (ldg/tools/jtscan_all.py) finds no other incomplete table (36 sites).
Result: REFLEX_PORT=all boots Concord under baton; loading_accept.sh all = only the known dead-data buffer (+5835c / +5836c of
the 640 KB buffer, notes/loading.md), races identical.

## The group

361 functions lifted with the captured backend into ports/ld_game_70.cpp, ld_game_71_73.cpp, ld_game_74.cpp, ld_game_75.cpp,
ld_game_76.cpp, ld_game_77.cpp (by address), one with the fl backend (ld_game_fl.cpp: 0075f890, flags of an `inc` read on several
paths). Lists: scratchpad ldg/game_*.tsv; regenerate with gen.py --list rather than edit. Names are the Ghidra names (FUN_ -> Fn_).
345 of the 362 run in a Concord baton boot.

## Traps

- Runtime natives at addresses of this range: 0x769b80 World_PhysicsUpdate (falls through to the port), 0x740ed0 App_SendUIEvent /
  0x72e780 (modes.cpp), 0x7130a0 App_DebugPrint_Lua / 0x70f720 UIImage_SetMaterialByName / 0x73f874 (pages.cpp, temporary,
  restored afterwards). A runtime native at a port address wins (port.cpp install), so these ports run only where the runtime
  leaves the address alone.
- **0x73f874 is inside GateSelection_SetCameraToGate 0073ed70** (pages.cpp "gate_camera" hooks the entity vf 0x40 call there).
  0073ed70 is not a boot function and is not ported; a future port of it must add 0x73f874 to lift.py HOOKS (or stay original).
- Thread procedure 00760400 (game init thread) runs under baton like any function; its Sleep(0) polls are ordinary import calls.

## Hand edits of generated code (re-apply after a regeneration)

- Fn_00774120 declares its outputs without EDX: EDX at its return is whatever BroadphaseGrid_Insert 008826d0 leaves (the
  collision port declares no outputs, so its scratch EDX differs); no caller reads it (live.tsv keeps EDX only through the ABI
  rule of the callers' callers). With LIFT_OUTS, the per-call Verify in the boot reported every call under REFLEX_PORT=all.

## Results (final build, -fwrapv, current lifter incl. the unsigned imul fix)

- loading_accept.sh ld_inflate,ld_game with VERIFY=1, Concord and Alpine: 184 ports verified in the boot, 0 mismatches (18 with
  unverifiable calls), Verify boot world identical to the original's; canonical dump IDENTICAL, world images 0 pages differ, race
  IDENTICAL, race hashes identical.
- loading_accept.sh all (VERIFY=1), Concord: 799 ports verified, 0 mismatches; canonical dump: only the known dead-data buffer
  (+5835c / +5836c, notes/loading.md); race identical. Alpine: 778 verified, 0 mismatches; IDENTICAL, race identical.
- verify.sh all: none of these ports runs in race frames (verify.sh ld_game,ld_inflate: 0 ports called on all five worlds, on
  identical). verify.sh all: "on: identical" on all five; mismatches only in Engine__Update: the known [148dbd9c] transient
  (notes/dead-stack.md) on Concord_ai11 / Alpine / SX, and on Terracross 121 calls with words 1 vs 3 at a 128-byte stride
  (0x142d766c..0x142da96c); both also with REFLEX_PORT=all,-MXGame__Update, i.e. not from these groups.
