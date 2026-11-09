# build11: the .tdf07 track loader (00536ef0) and MXActivityFreeRide (0055fdb0)

Status 2026-10-19: ported and verified. Files: `core/game/build_activity_track.c` (new), `core/game/build_activity_freeride.c`
(new), `core/game/build_activity_race.c` (case 8 and the vf5 track load switched to the port), `core/game/build_activity.h`,
`core/game/build_activity_int.h`, `core/oracle/build_activity_start.cpp` (group `rx_build11`). The two call sites of the loader
were switched by the lead: `build_mxworld.c` (world construction) and `race_activity.c` (the pending-reset path).

## 00536ef0 FUN_00536ef0 — the per-activity track definition

`FUN_00536d00` builds `<Level><suffix>.tdf07` (suffix table 0x009c54cc: 0 nat, 1 sx, 2 sm, 3 opx, 4 wp, 5 mm, 6 fmx, 7 fr,
8 cb, 9 ec, 10 trx, 11 utvm, 12 pwx, 13 chl, 14 pf), then 00536ef0 parses it into the activity's `+0x890` block and the
`+0x21e10` section records used by the AI, start grid and respawn. `00536d00` also builds the asset name and checks presence
(`00536e80`).

Format (recovered from the code, confirmed by an exact-length parse of 7 shipped assets):
- Header: four **big-endian** u32: `n_nodes`, `n_sections`, `n_x`, `n_y`.
- Allocation: `n_nodes * 0x500 + n_sections * 0x130 + n_x * 16 + n_y * 16 + align16(4 * n_sections)`. The x / y counts only
  size the allocation.
- Then a big-endian stream of `tag(u32), length(u32), payload`.
  - `0x0b` new section (0x130 bytes): `+0` first node, `+4` last node, `+8 / +0x10` next-list pointer / count,
    `+0xc / +0x14` previous-list pointer / count; entries are section indices resolved after the parse.
  - `0x14` new node (0x500 bytes): `+0x60` previous, `+0x64` next, nine `8x4` dword grids at `+0x68 .. +0x4e8`.
  - Grid tags carry `rows, cols` then `rows * cols` dwords (0x10-byte rows).
  - `0x1f` track type (8 class limits; e.g. type 6 starts 90.0 / 53.66).
  - `0x2c` / `0x2d` NaN / non-finite fallbacks; `0x24` a u32 array.
- After parsing: nodes linked, every node's five points snapped to the terrain, bounding box `+0x300` (min x / 5000 / min z) and
  `+0x310` (max), and five `(section, node)` markers at `+0x98/+0x9c`, `+0x68/+0x6c`, `+0x38/+0x3c`, `+0xc8/+0xcc`,
  `+0xf8/+0xfc`.

Helpers ported with it: 00536d00, 00536dc0, 00536e80, 00538090 (block reset), 00539550 (tag / length), 005395d0 (vec3),
00539680 (u32 array), 00539720 (grid), 00539820 (0x30 record + terrain snap), 00810600 / 00810680 (BufferedFileInputStream
ctor / dtor), 008109b0 / 00810a10 / 00810960 (MemoryInputStream read / skip / close), 008104d0 (CRTFileInputStream::vf8).
`0042d960` (terrain-list lookup) is a static helper. The product reads the loose `.tdf07` through `fmt_vfs` into a
MemoryInputStream; the oracle runs the original file I/O. `007c5950` is the existing terrain height port.

## 0055fdb0 MXActivityFreeRide

Constructor ported in `build_activity_freeride.c`: `005562c0` Activity base, the FreeRide fields, the ResetMarkerFlagEntity's
ModelEntity constructor 00401db0, the ActivityAudioLogic allocation / vf32 00581030, and 005857f0 (jump tracker). The
`build_activity_race.c` case 8 in `rx_activity_create` now calls the port; the FreeRide methods 005609d0 / 005604f0 / 00560b70 /
00560290 / 005609b0 were already in `race_activity.c`.

## Verification

Baton boot `National,10,NAT_Concord,50,0,3,Laps,2,0`, `REFLEX_PORT=rx_build11 REFLEX_PORT_VERIFY=1`: 00536ef0 4 calls / 4
verified / 0 mismatches; 0055fdb0 1/1/0; 00581030 2/2/0. FreeRide boot `FreeRide,9,FR_Alpine,45,0,-1,,0,8`: 00536ef0 4/4/0,
0055fdb0 2/2/0. The two remaining boot mismatches (00556ec0 Activity::vf2, 00564b20 Race::vf2) reproduce with the baseline CLI
and are the known dead proxy data. After the call-site switch, `loading_accept.sh rx_build11 VERIFY=1` gives a canonical world
VERDICT IDENTICAL (160,456/160,456), race_compare 52/52 IDENTICAL, state hash equal.

## Left

- Only `NAT_Concord-nat` and `FR_Alpine-fr` were parsed in Verify; `-wp`, `-cb`, `-sx` use the same tag set but were not
  individually exercised. The `-cb` calls in the boots take the no-asset early return.
- The product cannot reach the loader until the session / startup gap is closed.
