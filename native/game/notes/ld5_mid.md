# ld5_mid (0x660000..0x7fffff): wave 5, the rest of loading and race frames on every event

2026-10-18. The `ld5_mid` rows of plan/wave5.tsv: 152 functions (146 `fn`, 6 `gap`) that still ran original code in the lead's
REFLEX_PORT=all sweep over the 31 arcade events. They were missing from wave 4 because its plan was the exact Alpine boot: event-specific
code (the grass system on NAT_Renne / RenneB, the Supercross crowd / leaderboard / land-animal entities, the Waypoint intro scenario,
the sound-effect zones of Bedford / FortDodge / Cheyenne) and code Ghidra had no function for (the lead's `Fn_xxxxxxxx` entries and
the gap rows).

## Files

| file | ports | range |
|---|---|---|
| ports/ld5_mid_66.cpp | 45 | 660000..700000 (GrassSystem_* / GrassAsset_* / GrassList_* / XenonGrassAsset_Parse, sound-effect zones, crowd, LandAnimalProximity, LeaderBoard, AnimEntitySingleAnimLoopWithParticles) |
| ports/ld5_mid_70.cpp | 66 | 700000..780000 (Waypoint / race intro scenarios, PodiumRiders_ClearMud, CrewChief, game-init thread 00760400, Fn_0070dc50, ...) |
| ports/ld5_mid_78.cpp | 41 | 780000..800000 (sort predicates 00793f70 / 00793f90, XenonModelGroup / render-queue helpers 007d5xxx, FUN_007f4180 / 007f4810, Renderer_BuildGammaRamp) |

All 152 are lifted with the shared lifter (captured backend), nothing hand-written. Lists (addr, name, group): scratchpad
`ld5_mid/ld5_mid_{66,70,78}.list`. Regenerate whole files, never mix:

    cd native/game/tools/lift; ../../../../_re/.venv/bin/python gen.py --list <list> --out ../../ports/ld5_mid_<x>.cpp --group ld5_mid

Names: Ghidra names with `::` -> `__`, `FUN_x` -> `Fn_x`; gap rows -> `Fn_<addr>`. Group string `ld5_mid`.

## Gap rows (and a trap in tools/gap_functions.py)

**gap_functions.py misattributes gap code**: a gap pc is given to the nearest candidate entry at or before it (`cs`), even when that
candidate lies in an earlier gap, so a row's address can be far from the code that actually ran. My own `ld5_mid/gap_pcs.py <bins>`
lists the executed pcs outside every functions.tsv function directly. On the lead's bins and mine:
- gap_006f6520 (307 k, all 31 boots) is really **0070dc50**, a function after a `ret` (pointer at 0070e40f; probably a Lua C function,
  it reads `[[esp+4]+8]`): ported as Fn_0070dc50 (642 calls verified in the Concord Verify boot).
- gap_007a1330 (5 k) is the tail 007d07bc.. of FUN_007d05f0, gap_0073ce60 (96) the tails 0074e40b.. / 0074e87b.. of the Waypoint
  functions, gap_006a78a0 (24) the tail 006bf6fe of LeaderBoard::vf5: all covered by the lifted ports of their functions (the lifter
  follows the real control flow past Ghidra's size).
- gap_007f1aa0 (3 k) is 00814740.. / 00823a90.. (ld5_hi's range). 007f1aa0 itself already has a port (ports/rest_7.cpp, wave 3a): not
  ported again here.
- gap_006c14a0 (race, Stillwater / Kingston) is real: a function (pointer stored by 006c0ed2 / 006c0f63), verified in a Stillwater race.
The row addresses 006f6520, 007a1330, 0073ce60, 006a78a0 are nevertheless real function starts (after `ret` / `int3`, with callers:
006f237d / 006f23f4, 00661f9c, pointer 009b902c, 006a2bc7 with ESI input); they are lifted and kept, but **none of them ran in any of
the 31 boots or races** (unverified, exact by construction). A check (`ld5_mid/inside.py`) confirms no port lies inside the lifter
walk of any of the 8 preceding entries (no labels).

## Renderer_BuildGammaRamp 007aff40

Left unported in wave 4 (ld_models.md) because of `fldln2` / `fyl2x`. lift_rt.h now has the x87 transcendental helpers
(`x87_ldconst`, `x87_fyl2x`, ...: vm.cpp execute_fp's `reflex_f80::transcendental` with the control word's rounding; vm.cpp's memo is
only a cache of that pure function). The lifter emits them now (lift.py change below), so it is a plain lifted port.

## Lifter change (tools/lift/lift.py, additive)

- `X87_CONST` (fldpi / fldl2e / fldln2 / fldl2t / fldlg2 -> `x87_ldconst(m, "<op>")`) and `X87_TRANS` (fpatan / fyl2x / fyl2xp1 /
  f2xm1 / fsin / fcos / fptan -> `x87_<op>(m)`), both flag-neutral. Captured backend only (lift_fl.py unchanged).

## Grass (NAT_Renne / RenneB)

GrassSystem_Init / SelectCells / SortBuckets / DrawFar / Draw, GrassAsset_SortDescending / BuildHeadingOrders, GrassList_MergeSort*,
XenonGrassAsset_Parse, GrassAssetIO vfuncs run in the boot; GrassSystem::vf19 (the per-frame update incl. the sway, which calls the CRT
`sin` through the exe's import, i.e. __libm_sse2_sin: the ld5_hi agent's) runs in race frames. The lifted vf19 makes the same CRT call.

## Acceptance (scratchpad ld5_mid/acc, final binaries: every port in the tree at 15:xx)

- `loading_accept.sh <boot> ld5_mid` on Concord (`1`), `alpine`, NAT_Renne (grass), SX_FortDodge, TRX_Lynchburg, WaypointRace
  FR_Cheyenne, FreeRide FR_Cheyenne, NAT_Stillwater: VERDICT IDENTICAL, world images 0 pages differ, race IDENTICAL, race hashes
  identical (all eight). 65..112 of the ports are called per boot.
- Verify inside the boot (scratchpad `ld5_mid/verify_boot.sh`: loading_accept.sh's VERIFY=1 boot alone, to stay within 2 processes)
  on Concord, Renne, FortDodge, Lynchburg, WP Cheyenne, FR Cheyenne: 0 mismatches (38..57 ports verified; 2..5 with unverifiable
  calls: the thread procedure 00760400 and callers of waits), Verify boot world byte-identical to the original's.
- Race Verify on the ported baton worlds (scratchpad `ld5_mid/race_verify.sh`: verify.sh's checks on one world, 1500 frames):
  Renne GrassSystem::vf19 1500/1500 + AnimEntitySingleAnimLoopWithParticles::vf22 60; FortDodge SoundEffectZoneEntitySphere::vf19
  3000; WP Cheyenne SoundEffectLineZoneEntitySphere::vf19 1500, WaypointIntroScenario vf1 / vf3, WaypointRaceActivityAudioManagerEntity
  vf22; FR Cheyenne 006cb390 / 006ccef0; Stillwater 006c0b60 / 006c14a0: 0 mismatches, On identical everywhere.
- Filter `all`: Concord 1 node = the known dead-data buffer of loading.md (+5835c bfa0ae7d / 0022fc20, +5836c), every other event
  IDENTICAL (alpine, Renne, FortDodge, Lynchburg, WP / FR Cheyenne); races identical. Image page diffs = the blocked threads'
  return-address slots (loading.md, lead's section).
- `verify.sh <boot> all`: on identical on all five with the reference hashes; mismatches only Engine__Update's known transients
  ([148dbd9c] dead stack on Concord_ai11 / Alpine / Springfield; Terracross 121 x [142d776c] 1 vs 3).
- Coverage (own copies of bootcov3 / racecov run.sh, -P 2, REFLEX_PORT=all): race frames of all 31 events run no original exe
  instruction at all; the boots of all 31 leave nothing in 660000..800000 (gap_functions.py lists gap_007f1aa0 = ld5_hi's 00814740 /
  00823a90, see above).
