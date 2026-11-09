# ld5_lo (0x400000..0x65ffff): wave 5, the rest of loading and race frames on every event

Status 2026-10-18: 309 of the 312 ld5_lo rows of plan/wave5.tsv are ported (gap_0055e0e0 was already ported by an earlier wave;
00566c50 and 005a7f10 are left on purpose, below), plus 4 functions the coverage sweep found that the plan misattributed (below):
313 ports. All lifted with tools/lift (gen.py --list, captured backend); nothing refused, no hand-written ports, no lifter change
needed. After them, baton boots of all 31 events with REFLEX_PORT=all run no original code in 0x400000..0x65ffff except
SavedGameManager_ThreadProc, and their 1500-frame races run no original exe code at all (gap_functions.py: 0 functions). Group string `ld5_lo`; outputs LIFT_OUTS (compared only where live).

## Files

| file | ports | range |
|---|---|---|
| ports/ld5_lo_4.cpp | 23 | 400000..4fffff (sound-zone / grass vf22, WaypointRaceGate, TimedRaceSection, std helpers) |
| ports/ld5_lo_50.cpp | 61 | 500000..54ffff (RaceDriver / WaypointRaceDriver, Rule*, Behavior* AI vfuncs, AI spline search 005385f0 / 00538bb0) |
| ports/ld5_lo_55.cpp | 62 | 550000..57ffff (MXActivityRace / RaceStub / WaypointRace / FreeRide vfuncs) |
| ports/ld5_lo_58.cpp | 62 | 580000..5bffff (activity audio logic, UI page factories, Bink asset IO / video manager) |
| ports/ld5_lo_5c.cpp | 80 | 5c0000..5fffff (UI component / trigger factories, MXUI HUD components) |
| ports/ld5_lo_6.cpp | 21 | 600000..65ffff (MXUIWaypointCountComponent, MXZipLineCamera / MXSplineCamera, init-thread tables 0065caxx) |
| ports/ld5_lo_extra.cpp | 4 | functions found by the coverage sweep (below): 005834f0, 00597640, 0059d500, 005d7da0 |

Lists (addr, name, group): scratchpad `ld5_lo/A_*.list`; regenerate with
`_re/.venv/bin/python tools/lift/gen.py --list <list> --out ports/ld5_lo_<x>.cpp --group ld5_lo` (run from tools/lift).

Names: Ghidra names with `::` -> `__`; FUN_x / Fn_x / gap_x -> `Fn_<addr>`, except 28 factories identified here
(`<Class>_Create`: ReflexAlignedAlloc(size), the class constructor, the vtable at +0, return the object or 0; listed in
_re/names/9e_ld5_lo.tsv, e.g. 005ce9a0 UIContainer_Create, 005d98f0 UIFocusContainer_Create, 005d7d20 UIKeypressTrigger_Create,
005a7f60 MXUIAAALoadingPage_Create). A name already used by another port would get `_<addr>` (none needed).

## The gap rows

gap_00417c50, gap_004db6d0, gap_00571c20, gap_00583520, gap_005df5b0: each starts right after `int3` padding (or a `ret`) at a
16-aligned address, so each is a real function Ghidra had not created: ported as `Fn_<addr>` at that address (lifted whole from
there; 00583520 is a 9-instruction thunk that tail-jumps to 005811a0).

## What the gap rows really were (and a trap in gap_functions.py)

The coverage sweep after the first 309 ports (scratchpad `ld5_lo/left.py lo hi <bins>`: every original pc executed in the range,
grouped by containing function or gap) showed original code at 005834f0, 00597640, 0059d500 and 005d7da0, not at 00571c20 / 00583520
(whose ports are installed but never called in these boots). These four start right after a `ret` of the previous function (no int3
padding) and are reached only through pointers stored in .text (callbacks pushed by WaypointRaceActivityAudioLogic vf3 / vf4, and
factory tables of the static initializers at 0x905060 / 0x906316), so gap_functions.py has no candidate entry for them, and it gives a gap
pc to the nearest preceding candidate **anywhere** (`bisect` over all candidates, not within the pc's own gap): the 18 instructions /
6 runs of 005834f0 became "gap_00571c20", the 10,354 of 00597640 + 0059d500 + 005d7da0 became "gap_00583520" (exact sums). The five
gap rows' own entries are real functions too (after int3 padding) and stay ported; the four real ones are ported in ld5_lo_extra.cpp.

## Not ported (on purpose)

- **00566c50 Fn_00566c50**: not a function. It is the `call __security_check_cookie; mov esp, ebp; pop ebp; ret 4` epilogue of
  005666d0 (Ghidra cut 005666d0 at 00566c11), reached only by the fall-through of 005666d0, whose port covers it (found with
  scratchpad `ld5_lo/inside.py`: entries inside the lifter walk of one of the 8 preceding entries; the only one in the group).
- **005a7f10 SavedGameManager_ThreadProc**: kept original as documented in notes/ld_frontend.md (the thread is blocked inside the
  proc at world capture). See "SavedGameManager_ThreadProc" below for a re-check.

## Acceptance (scratchpad ld5_lo/acc.sh, res_*.txt)

`ld5_lo/acc.sh` is loading_accept.sh limited to 2 processes (reference boot / race cached per event), with `VERIFY=1` (a Verify boot
after the two boots) and `RV=1` (the reference world raced 1500 frames with the ports in Verify mode, and in On mode against the
reference race hashes: the race-frame check for events verify.sh does not cover, e.g. waypoint races).

Filter `ld5_lo` (build of 2026-10-18 afternoon):

| event | ports called (boot) | Verify boot | canonical | image pages | race | race Verify (ports called) |
|---|---|---|---|---|---|---|
| Concord (1) | 199 | - | IDENTICAL | 0 | IDENTICAL, hashes identical | - |
| FR_Alpine (FreeRide) | 68 | 63 ports called, 0 mismatches, world identical | IDENTICAL | 0 | IDENTICAL | 1, 0 mismatches; On identical |
| WaypointRace FR_Cheyenne | 146 | 93, 0 mismatches (2 with unverifiable calls), world identical | IDENTICAL | 0 | IDENTICAL | 3 (WaypointRaceGate::vf19 12,000 calls, MXActivityWaypointRace vf23 / vf8), 0 |
| WaypointRace FR_Gibson/1 | - | 92, 0 mismatches, world identical | IDENTICAL | 0 | IDENTICAL | 3, 0 |
| Supercross SX_FortDodge | 207 | 111, 0, world identical | IDENTICAL | 0 | IDENTICAL | 1 (SoundEffectZoneEntitySphere::vf22), 0 |
| Supercross SX_Kingston | - | 119, 0, world identical | IDENTICAL | 0 | IDENTICAL | 5 (MXZipLineCamera / MXSplineCamera vf19 / vf22, 00419a10), 0 |
| Terracross TRX_Lynchburg | 205 | 106, 0, world identical | IDENTICAL | 0 | IDENTICAL | 1, 0 |
| National NAT_Renne | 212 | 117, 0, world identical | IDENTICAL | 0 | IDENTICAL | 1 (GrassSystem::vf22), 0 |
| National NAT_Bedford | - | 106, 0, world identical | IDENTICAL | 0 | IDENTICAL | 2, 0 |

Filter `all` (every port in the tree, incl. the other wave-5 groups' files at that time; 6,700-7,030 ports called per boot):
Concord: 1 node differs = the known dead-data buffer of loading.md (+5835c `bfa0ae7d` vs `0022fc20`, +5836c), race IDENTICAL;
FR_Alpine, WaypointRace FR_Cheyenne, SX_FortDodge, TRX_Lynchburg, NAT_Renne: IDENTICAL, races IDENTICAL, race hashes identical.
World images differ only in stack pages of blocked threads (13-16 pages: the return-address slots of notes/loading.md "Lead,
2026-10-18"; ld5_lo alone: 0 pages).

Race frames: `tools/verify.sh <boot> all` (same build): on: identical with the reference hashes on all five worlds (Concord
f1a02da0f3546f3b, Concord_ai11 62b9d1986dc7b8ba, Alpine 9c6ce38c09cfa013, SX_Springfield_ai11 9895014866da6382, TRX_Verde_ai11
28623e7b63b760c9); Verify mismatches only in Engine__Update (engine_mt): the [148dbd9c] dead-stack transient of notes/dead-stack.md
(Concord_ai11, Alpine, Springfield) and the pre-existing Terracross [142d776c] 1 vs 3 (notes/rest.md). No ld5_lo port is called in
those five worlds' race frames (the waypoint / sound-zone / camera functions run on other events: checked with RV above).

Second round (with ld5_lo_extra.cpp, fresh reference boots): ld5_lo on Concord, WaypointRace FR_Alpine and FR_Alpine (FreeRide) with
VERIFY / RV: IDENTICAL everywhere, 0 image pages, 0 Verify mismatches (Verify boot worlds identical), races identical; `all` on Concord
(the dead-data buffer only) and WaypointRace FR_Alpine (IDENTICAL), races identical.

## Coverage (scratchpad ld5_lo/bootcov, ld5_lo/racecov: copies of the lead's run.sh, -P 2, REFLEX_PORT=all)

All 31 events boot and race (race state 2 at frame 1499). `tools/gap_functions.py` (the lead's fixed version: gap rows named after the gap's
first executed pc) over the boot bins: Engine::Frame (deliberate, notes/ld_engine.md), gap_00823a90 and gap_00814740 (0x8xxxxx, not
this range), SavedGameManager_ThreadProc (deliberate, here); race bins: 0 functions. The lead's corrected gap rows in this range
(005d7da0, 0059d500, 00597640, 005834f0: ported in ld5_lo_extra.cpp; 0051709d, 00565b18, 00479668, 005387b3, 0055b047, 00556be7,
00566c44, 0055ab54, 0057902c, 00578b27: tails of 00516e10, 00565a30, 00478d90, 005385f0, 0055ab60, 00556720, 005666d0, 0055a7e0,
00578f40, 005787a0, all lifted whole here) run no original code any more.

## SavedGameManager_ThreadProc, re-check for the lead

Its blocking call is `call edi` with edi = [IAT WaitForSingleObject] (005a7f2f). Since the lead's change to LIFT_CALL (lift_rt.h
import_target / call_import: calls into imports push the real return address), a lifted port would leave 005a7f31 in the slot, so
the reason of notes/ld_frontend.md is probably gone. Not tested (kept original as the wave-5 rules say); a candidate for whoever
owns it: lift it, then check a Concord image diff (0 pages) with loading_accept.sh.
