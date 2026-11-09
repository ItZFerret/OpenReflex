# ld_entities (0x600000..0x700000) and ld_core (0x400000..0x4d0000): loading ports

Wave 4 (native loading), 2026-10-18. Both groups are lifted with the shared lifter (tools/lift, captured backend; all regenerated after the lead's
imul / -fwrapv fix, ~12:40). Group strings: `ld_entities`, `ld_core`.

## Files

| file | ports | range |
|---|---|---|
| ports/ld_core_40.cpp | 232 | 400000..440000 |
| ports/ld_core_44.cpp | 23 | 440000..4d0000 |
| ports/ld_entities_60.cpp | 160 | 600000..640000 |
| ports/ld_entities_64.cpp | 107 | 640000..680000 (forest: ForestSystem_CullThread, XenonForestAsset_*, ForestSystem::vf3_o90) |
| ports/ld_entities_68.cpp | 172 | 680000..6c0000 (ParticleManager2D_*, PlatformParticleManager2D::SyncRenderData) |
| ports/ld_entities_6c.cpp | 103 | 6c0000..700000 (fonts: FontPack_Sample / DrawCore / DrawOutline / Load) |
| ports/ld_core_extra.cpp | 25 | entries not in the plan (below) |
| ports/ld_entities_extra.cpp | 127 | entries not in the plan (below) |

797 ports from the plan: every `ported == 0` row of plan/loading-waves.tsv in the two ranges (255 ld_core + 544 ld_entities) except
the two mid-function entries below. Names: Ghidra names with `::` -> `__`; `FUN_x` -> `Fn_x` (purpose not identified); a name
already used by another port gets `_<addr>`.

**152 more (the `_extra` files)**: what still ran as original code in the two ranges in baton boots with `REFLEX_PORT=all`
(REFLEX_COVERAGE of a Concord and an Alpine boot, scratchpad `ldent/cov2`, `cov3`): `ldent/entries.py <cov.bin>...` takes the lowest
executed pc not covered, lifts its walk, repeats. Two kinds:
- functions the plan does not list (it was made from the exact Alpine boot; the baton Concord boot runs more front-end / event
  code): the 2D UI component stubs (0060xxxx), PhysicsEntity / InstancedAnimationEntity vfuncs, crowd / flag / bale / gate entity
  vf5, the forest draw (ForestTree_DrawGeometry 0065f6d0, TreeAsset_Set*/Draw*Lod), RiderCharacter vf2 / sdtor;
- functions Ghidra folded into the previous entry (code after the `int3` padding or a `ret`; the plan's row is the Ghidra entry,
  whose port covers only its own walk), e.g. 00693f70 in "00693870", 004046a0 in "StatusBoardEntity::vf22", 00675f70 in
  "00675d90", 006df990 in "006df870". Every such entry is preceded by `cc` / `c3` (checked), so it is a real function start.
After them nothing in 400000..4d0000 / 600000..700000 runs as original in those two boots.

Regenerate: lists `addr<TAB>name<TAB>group` per file (from plan/loading-waves.tsv, group + ported == 0, split by the ranges above,
then `_re/.venv/bin/python tools/lift/gen.py --list <list> --out ports/<file>.cpp`
Copies of the lists: scratchpad `ldent/*.list`.

## Not ported (on purpose)

- **0061e677** (Ghidra FUN_0061e677) and **0065aacc** (FUN_0065aacc): not functions. Both are jump targets inside another function's
  control flow (61e677 is reached by jcc / jmp from 0061e3d0, already ported by world_rider_gen.cpp; 65aacc lies inside the walk of
  00659ef0, ported here). A native at such an address is entered mid-function ([esp] is not a return address): On mode happens to
  work (the lifted code follows the real flow to the parent's ret), but Verify breaks ("native returned to another address" and then
  "execution outside mapped code" when its group runs without the parent's port). Found by `scratchpad/ldent/inside.py` (entries that
  lie inside the lifter walk of one of the 4 preceding Ghidra entries); only these two in both ranges.

## Lifter extensions (tools/lift/lift.py)

- Materialized flags of `inc` / `dec` (a consumer reached by several flag defs, one an inc / dec): `FL = (FL & 1) | fl_logic(R) |
  OF`; CF is kept (CF consumers' reaching-def scan passes through inc / dec, so FL's CF is the reaching def's). 00677220
  (`and eax, 0x80000001; jns; dec; or eax, -2; inc; jne`: the signed `x % 2` idiom).
- `fsqrt` (x87, through `reflex_f80::sqrt(st0, fbits, frc)` as vm.cpp M_fsqrt; flag-neutral): 006382b0, 00476ab0.
- `lock or / and / xor / add / sub / inc / dec` on memory: the plain read-modify-write (one guest thread runs at a time):
  PlatformParticleManager2D::SyncRenderData 006998e0.
- `BLOCKING` += 0x662ec0 (ForestSystem_CullThread, a thread procedure waiting on `ForestSystemStart`): every call is `LIFT_CALLB`, so
  the port also stops correctly under the exact scheduler's non-popping wait stand-in; under baton the call simply blocks.

## Acceptance (scratchpad/ldent/acc)

- Final (all 949 ports, regenerated with the -fwrapv / imul-fixed lifter): `loading_accept.sh <boot> ld_core,ld_entities 1` and
  `... alpine`: VERDICT IDENTICAL (canonical dumps), race IDENTICAL, race hashes identical. Earlier runs per group (ld_core alone,
  ld_entities alone, both events) also IDENTICAL. With the filter `all` (every port in the tree at 12:45, full build): Alpine
  IDENTICAL; Concord 1 node = the known dead-data buffer of loading.md (+5835c `bfa0ae7d` vs `0022fc20`); races identical.
- World images: ld_core 0 pages differ. ld_entities: 1 word, the return-address slot of the forest cull thread (Concord 0x19bff144,
  Alpine 0x17bff...: the thread's ESP while it waits on ForestSystemStart): original `00662ee2` (the return address of its
  WaitForSingleObject call), port `00000000`. Cause: `Machine::call_function` pushes 0 as the return address of a nested call and
  LIFT_CALL / LIFT_CALLB write the original return address only after the call returns; a thread blocked inside a ported procedure is
  captured with 0 in that slot. The same holds for every ported worker procedure (RiderAnimation_WorkerThread etc.). Harmless for the
  race runtime (it starts a worker from its entry, m.run(proc), and the forest thread is not resumed); canonical dump identical. A fix
  belongs in the shared call path (push the real return address and stop there, like Verify's stop_pc), not in the ports.
- Race frames: `verify.sh <boot> math,ld_core,ld_entities`: 0 mismatches, on: identical on all five worlds (before the extras,
  `ld_core,ld_entities` alone: no port of these groups is called in the race worlds). `verify.sh <boot> all` (full build): on:
  identical on all five; Engine::Update mismatches only the known ones ([148dbd9c] dead-stack transient of notes/dead-stack.md on
  Concord_ai11 / Alpine / Springfield; Terracross 121 x [142d776c] 1 vs 3, pre-existing per notes/rest.md).
- Coverage check after the extras: baton boots of Concord and Alpine with REFLEX_PORT=all run no original code in 400000..4d0000 /
  600000..700000 (`ldent/entries.py` on `cov4_1`, `cov4_alpine`: nothing left).

## What these functions are (rough)

- ld_entities: main-thread entity setup, the forest (ForestSystem_Init / CullThread /
  SelectTrees, XenonForestAsset_Parse / DrawBillboards, ForestAsset_SampleColorRefs: loader thread), fonts (FontPack_* : glyph
  sampling / outline rasterisation on the main thread, 14 M instructions in FontPack_Sample alone), 2D particles (pool thread jobs),
  init-thread startup tables (0065c200, 0065c070, 006b64a0, 006b68e0).
- ld_core: small entity classes (ScrollingTextureEntity, CameraFacingModelEntity), main-thread setup 004772f0 / 00476e80.
- start_event's baton settle (loading.md 3): the font handle `RoadkillAlternates56` is bound by asset code (FUN_007fd690, ld_assets);
  the font ports here make the same calls in the same order, so the settle is unchanged (boots with these ports start the event).
