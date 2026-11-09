# Reflex archives: `.rxdata` and `.rxtrack` version 5

Status: **experimental** (branch `experimental`, 2026-11-07). Readers: `core/src/fmt_rxar.c` (core), `Content/RxFiles.cs` +
`OpenReflex/RxArchiveNative.cs` (client, through the core's `rx_archive_*` exports). Writer: the same C code (`fmt_rxar_create/add/finish`),
used by `core/tools/rxpack.c` and by the client's packer (`Content/ReflexDataPack.cs`).

Nothing of the original game is ever in an archive that ships: archives are made on the player's machine from their own install (or a
modder's machine from their own track).

## Why

The client's game library (the import: `game files/`, `ReflexAssetImport.cs`) is the install with every package asset extracted and
decompressed into its own file: ~20 GB in ~150 000 files for Reflex + the Alive tracks, where the original install is 5.6 GB. A Reflex
archive holds the same files compressed (zstd) in one file with an index, so

- the library shrinks to about a third (numbers below), and a level's assets are one file to copy, share or delete;
- any single file reads without unpacking anything else (the index is loaded once; a read decodes only that file, or only the chunks of
  it that the read covers);
- the loose files keep working: an archive sits *below* the loose files of the same folder, so nothing that reads the library had to
  change its paths, a file written next to an archive replaces the packed one, and a library that was never packed is untouched.

`.rxtrack` version 5 is the same container holding one track: a level's own files exactly as the game reads them. A base Reflex track,
a converted MX vs ATV Alive track and an old Reflex track mod (a `Beta_*_Track_Slot_*` set) all convert into it without loss.

## Container (RXAR version 1)

All numbers little-endian.

### Header (64 bytes at offset 0)

| offset | size | field |
|---:|---:|---|
| 0 | 4 | magic `RXAR` |
| 4 | 4 | container version: 1, or 2 (entries may carry a SHA-256, share stored bytes, or be pool references: "Dedupe" below) |
| 8 | 4 | kind: `DATA` (0x41544144, a `.rxdata`), `TRAK` (0x4B415254, a `.rxtrack`) or `MODS` (0x53444F4D, a `.rxmod`: RXMOD.md) |
| 12 | 4 | flags: 0 (a reader refuses others) |
| 16 | 8 | index offset (= end of the data region) |
| 24 | 8 | index stored bytes (one zstd frame) |
| 32 | 8 | index raw bytes |
| 40 | 4 | file count |
| 44 | 4 | CRC-32 (zlib) of the raw index |
| 48 | 12 | reserved, zero |
| 60 | 4 | CRC-32 of header bytes 0..59 |

The writer fills the header last: a file whose first bytes are not `RXAR` is unfinished (the writer works on `<name>.partial` and renames
it only after the index and header are written and flushed).

### Data region (offset 64 .. index offset)

Each file's stored bytes, back to back, in the order they were added (not necessarily the index order). A file is

- **stored** (codec 0): its raw bytes;
- **one frame** (codec 1, chunk_log2 0): one zstd frame with content size and XXH64 checksum;
- **chunked** (codec 1, chunk_log2 = k): independent zstd frames of 2^k raw bytes each (the last one shorter), back to back. The writer
  chunks files above 4 MiB at 1 MiB (k = 20), so a read of a range decodes at most the chunks it covers.

The writer stores a file whose zstd output is not at least 3 % smaller (the original packages' LZX data, Bink video, FMOD banks); a
first 128 KiB probe at level 1 decides that without compressing the whole file.

### Index (zstd frame at the index offset)

```
u32  "RXIX"
u32  file count (= header)
u32  metadata bytes M, then M bytes of UTF-8 JSON (may be empty)
per file, sorted by path (byte order), no two paths equal ignoring ASCII case:
  u16  path bytes P, then P bytes of UTF-8 path
  u8   codec (0 stored, 1 zstd, 2 pool reference: version 2)
  u8   chunk_log2 (0, or 12..30 for codec 1)
  u16  flags (bit 0: a SHA-256 follows; version 2; other bits refused)
  u64  raw size
  u64  offset of the first stored byte (absolute; 0 for a pool reference)
  u64  stored size (sum of the chunks for a chunked file; 0 for a pool reference)
  u32  CRC-32 (zlib) of the raw bytes
  if flags bit 0: 32 bytes, the SHA-256 of the raw bytes
  if chunk_log2: u32 chunk count (= ceil(raw / 2^chunk_log2)), then u32 stored size of each chunk
```

Paths are relative to the game root, `/`-separated, with no empty, `.` or `..` component, no `\`, `:` or control byte, at most 1023
bytes. Lookups ignore ASCII case and treat `\` as `/` (the files were authored on Windows), like the core's file system.

Readers validate everything against the file: header CRC, index CRC, the index's own sizes, every file range inside the data region,
chunk counts and sums, sorted unique paths. A whole-file read checks the CRC-32; zstd checks each frame's XXH64.

### Metadata

`.rxdata`: `{"format": "reflex-data-archive", "version": 1, "created": <UTC>, "dirs": ["Assets"], "files": N}`.

`.rxtrack` version 5:

```
{"format": "reflex-track-archive", "version": 1,
 "level": "NAT_Concord",            the level name: the files are Database/<level>.dx9.*
 "name": "...",                     display name (a mod's folder name, or --name)
 "source": "base" | "alive" | "mod",
 "layout": "packed" | "extracted" | "packed+extracted",
 "created": "<UTC>",
 "files": [{"name": "NAT_Concord.dx9.level", "bytes": 296, "sha256": "..."}, ...],   every Database/*.dx9.* file of the track
 "alive": {...}}                    an Alive level's alive-levels.json entry (registry), when it has one
```

`files` is the same list `Online/Core/OnlineTrackMod.cs` hashes for the online track identity (name, bytes, SHA-256 of the four level
files), so a track has the same identity loose or packed.

## Dedupe (container version 2, 2026-11-08)

Measured on the packed library (version 1): 55 % of the stored bytes were copies - 1.9 GB the same file twice in one archive (gear and kit
textures), 2.4 GB the same file in several level archives (a level's B variant shares most of its level's assets).

- **Every entry has its SHA-256** (flags bit 0). The writer (`fmt_rxar_add`) stores bytes it already holds once: a second path to the same
  bytes points at the same range (two index entries, one copy). Readers need nothing new for that.
- **The pool.** In a library, a file more than one archive has is stored once in `Assets.rxdata` as `Pool/<sha256 hex>`; a level archive
  holds a **pool reference** for it (codec 2: its size, CRC-32 and SHA-256, no bytes). The core's file system (`fmt_vfs`: `pool_lookup`)
  and `RxFiles` map such an entry to the `Pool/` file of the root's `.rxdata`, size checked; `rxpack verify <archive> <root>` checks its
  SHA-256. A pool reference in an archive read on its own (a Mods/Tracks `.rxtrack`, an archive as a folder) has no bytes: tracks for
  sharing (`rxpack track`, `rxpack mod`) are written whole, never with references.
- **PACK GAME FILES** (`ReflexDataPack`) packs the library's *effective* files (loose first, packed second): a loose library, a library
  of version 1 archives (packed again from its archives: nothing is unpacked first), or the levels whose loose files came back. FULL (no
  archives yet, any version 1 archive, loose shared assets): the pool from every file's SHA-256 (and the old pool's files still in use),
  then `Assets.rxdata`, then each level; LEVELS (an all-version-2 library, loose files of some levels): those levels again, referencing
  what the pool has. Each archive is checked against its sources (a reference against its `Pool/` copy) before its loose copies go.
- Readers of version 1 read version 2 archives? No: a version 1 reader refuses version 2 (the experimental builds before 2026-11-08).
  Version 2 readers read both.

| library (Reflex + 26 Alive levels) | stored | loose |
|---|---:|---:|
| loose (the import before 2026-11-08) | 19.0 GB in 147 709 files | |
| version 1 archives | 7.83 GB in 83 archives | |
| version 2 (dedupe + pool) | **3.51 GB** in 83 archives (`Assets.rxdata` 2.1 GB with a 15 959-file pool, `Tracks/` 1.4 GB) | |

Packing the version 1 library again took 95 s on 6 threads; all 148 132 files read back identical; NAT_Concord, SX_Kingston and NAT_Farm
race exactly as from the loose library; NAT_Concord's event load 0.94 s (loose 1.55 s). `rxpack dupes <root>` measures what is left.

`rxpack unpack <root> [--delete]` turns a packed library back into loose files (pool references resolved).

## What a track archive holds

| path | when |
|---|---|
| `Database/<level>.dx9.database`, `.dx9.level`, `.dx9.scene` | always (the level's index, layout and scene) |
| `Database/<level>.dx9.package` | converted from an original install or a mod (layout `packed`): byte for byte |
| `Assets/<type>/<level>/<package>/<asset>.<type>` | converted from an imported library (layout `extracted`): the import's decompressed assets, its path escaping (`ReflexAssetImport.Segment`) |
| `Database/alive-<level>-*`, `Database/alive-import-<level>.json` | an Alive level's sidecars (bounce light, LUT, decals, pictures ...) |
| `Mod/<file>` | a mod's dress: `mod.ini`, `mod.json`, pictures, `.txt` (never mounted into the game's file system) |

A level's databases are its own: MXGame's level config opens the database `<level>` (packages `Shared` and `<level>Base`) and the scene's
subscene directory, all inside these files (`Content/TrackMods.cs`). Nothing else of the install is per level.

## Where archives are read

**The root's own archives** (core `fmt_vfs`, client `RxFiles`): an install or library root may hold

- `*.rxdata` at the root (`Assets.rxdata`: the shared extracted assets), and
- `Tracks/<level>.rxtrack` (one per level, base and Alive alike).

They are part of the root's file system, **below the loose files**: a loose file of the same path wins, then the track archives, then the
`.rxdata` files in name order. `REFLEX_NO_ARCHIVES=1` turns them off (comparisons). A legacy gzip `.rxtrack` (RXPK, the MTX terrain
packages of `tools/mtx/FORMAT.txt`) in `Tracks/` is not an overlay and is left alone.

**Mods/Tracks** (client `TrackMods`, core `rx_session_mount_level`): a `.rxtrack` version 5 there is a custom track like a folder of
loose files; the session mounts its `Database/<level>.dx9.*` files (`fmt_vfs_mount_archive`) exactly where the folder's would be. The
gzip RXPK `.rxtrack` packages keep their own loader (`fmt_modtrack.c`, `ExperimentalMtxLaunch.cs`): the two are told apart by the first
bytes (`RXAR` versus gzip `1F 8B`).

**Package reads in an extracted library** (`fmt_db_read`, `Rfx.Install.Read`): the extracted asset is read unless the package itself is
mounted or comes from an archive (a track converted from an original install brings its `.dx9.package`); an original install with a
track of the extracted layout reads that track's extracted assets.

## Tools

```
rxpack data   <root> <out.rxdata> [--skip-levels] [--level N] [--jobs N]   pack Assets/ (without the levels' assets: --skip-levels)
rxpack track  <root> <level> <out.rxtrack> [--name S] [--source S]          one level of an install or library
rxpack tracks <root> <out dir> [--only A,B]                                 every level of the root
rxpack mod    <mod folder> <out.rxtrack> [--stem S]                         an old Reflex track mod (Beta_*_Slot_* or any <stem> set)
rxpack list | verify <archive> [<root>] | extract <archive> <dir> | bench <root> <paths file>
```

Build: `C=$PWD/core O=<out>; source core/tools/product_link.sh; rx_product_objs && rx_link_driver <out>/rxpack core/tools/rxpack.c`.

## In the client (2026-11-08)

- **The import packs.** IMPORT YOUR GAME extracts into its staging folder as before, then packs it (`ReflexDataPack`, step 3 of 3 on
  the progress page) before the folder is published: the same layout (`Assets.rxdata` + `Tracks/<level>.rxtrack`). `Options.pack` /
  `REFLEX_IMPORT_LOOSE=1` keeps a loose library. A pack that cannot run (no core library) or stops on an error leaves the rest loose
  (a warning in import-report.txt); only a cancel stops the import. A packed library gets no `asset-index.html` / `Browse/` (they link
  loose files). The space check counts the packing's extra room (a fifth of the extracted bytes).
- **One archive at a time.** Each archive is written as `<name>.new`, every file of it is read back and compared byte for byte with its
  loose copy, it moves into place, then its loose copies are removed. The extra room needed is about one archive (the shared assets',
  ~3 GB, the largest); a stop part-way leaves a valid library (packed levels from their archives, the rest loose).
- **Libraries imported before** (loose files): packing is not optional. At startup, before the shell connects to the library (nothing
  reads it yet), PACK YOUR GAME FILES packs them (`OpenShell.Pack.cs`): PACK NOW is its only button (BACK doesn't skip it), and its
  progress page has no STOP. A pack that fails leaves the rest loose (the game plays from it) and is asked again next start; an old
  DON'T ASK AGAIN (PlayerPrefs `ReflexPackPrompt`, 2026-11-08 and before) is ignored. Not shown when `REFLEX_GAME_DIR` names the folder
  (developer and test runs) or with `REFLEX_NO_PACK_PROMPT=1`.
- **GAME LIBRARY** says what the files are and how big the folder is: `PACKED · 7.9 GB · 83 archives`, `PARTLY PACKED · ... still loose`
  or `LOOSE FILES · 20.3 GB · 149,800 files (...)` (`ReflexDataPack.Measure` on a worker), and offers PACK GAME FILES while loose files
  are left.
- **Files removed while the game runs.** PACK GAME FILES from GAME LIBRARY runs with the menus up: `RxFiles` falls back to the archive
  when a loose file vanishes between its look and its read, and the core's file system adopts a newly written archive when a loose file
  it indexed at a session's start is gone (`adopt_packed`, same size required). The menus' warm test-ride session is not made while a
  pack runs.
- `rx_archive_list` gives the client every path and size in one call; the client opens a library's archives on a worker at startup.

## Compression and speed

zstd 1.5.7 (vendored in `core/src/zstd`, BSD), one translation unit (`core/src/fmt_zstd.c`), no threads, no assembly; nothing of zstd
is exported from librxcore / rxcore.dll. Level 9 by default (about 110 MB/s per thread when packing, 0.37 of the texture data), 19 for
the smallest files (`--level 19`, 16 x slower to pack, 0.32). Decoding is about 1 GB/s per thread at any level.

Measured numbers: see "Results" below.

## Results (2026-11-07, Apple M-series, warm file cache)

Library: the imported Reflex + Alive library `REFLEX-lead/builds/alive/combined/map-a7` (82 levels: 56 Reflex incl. the test / UI scenes,
26 Alive). Packed with `rxpack data --skip-levels` + `rxpack tracks` at zstd level 9, 6 threads.

| | loose (today) | packed |
|---|---:|---:|
| files | 147 709 extracted assets + the levels' Database files | `Assets.rxdata` + 82 `Tracks/*.rxtrack` (83 files) |
| bytes (content) | 18 998 MB (6 724 MB shared + 12 275 MB in levels) | 7 847 MB (3 001 + 4 846), ratio 0.41 |
| on disk | 18.6 GB (Assets) + 0.44 GB (level files) | 7.66 GB |
| time to pack | (the import) | 18 s shared + 39 s levels (~330 MB/s) |
| core file system open (`fmt_vfs_open`) | 847 ms (stat of 150 k files) | 82 ms (83 indexes) |
| event load, create + load + 10 frames: NAT_Concord / SX_Kingston / NAT_Farm / NAT_Tropic | 1.55 / 1.45 / 1.50 / 1.55 s | 0.92 / 0.77 / 0.91 / 0.96 s |
| core reads (5 713 files, 884 MB, one thread) | 1 623 MB/s | 1 584 MB/s (decoding) from 2.6 x fewer bytes on disk |
| client reads (`Rfx.Install.Read`, 8 394 assets, 1 028 MB) | 1.14 s | 1.13 s |

zstd level 19 (`--level 19`): NAT_Concord 68.2 -> 63.0 MB, NAT_Farm 74.0 -> 67.3 MB (8-9 % smaller), ten times slower to pack.

Mods (15 community tracks of Mods/Tracks, `rxpack mod --level 19`): the size stays (their packages are LZX-compressed already and kept
byte for byte, which is what keeps their online identity); one file each instead of a folder of four.

### Verification

- **Every file of every archive** (the 83 library archives, 148 132 files; the 15 mods) decodes byte-identical to its loose file
  (`rxpack verify <archive> <loose root>`; 6.7 GB of shared assets decode in 3.1 s on 8 threads).
- **Races** (`core/tools/alive_race_test.c`, 12 riders, AI 95, 6000 frames): NAT_Concord, SX_Kingston, NAT_Farm (Alive) from the packed
  library print exactly what they print from the loose one; two mods (1138 Farm 14 MX National, 953 Nightbeach SX) mounted from their
  `.rxtrack` race exactly as from their folders (4000 frames).
- **Every riding level** (`builds/rxpack/sweep.sh`): an event of its kind loaded and 300 frames run from each library, outputs compared
  (see "Sweep" below).
- **Client readers** (`tests/rxarchive/build.sh`, .NET, real librxcore): every asset of NAT_Concord, SX_Kingston, FR_Alpine, NAT_Farm,
  FR_Dunes, SX_DLC_SX01, MXs, EngineDependencies, MXTables_Default and Rider through `Rfx.Install` (1 GB) identical; the Database listing
  the same (227 databases); every asset of the 15 packed mods identical after LZX decoding, their four files' SHA-256 (online identity)
  the same through `RxFiles.OpenRead`; the client packer (`ReflexDataPack`) on a two-level library (a Reflex and an Alive level + shared
  assets): every file read back identical, the loose copies removed, the Alive registry entry carried, a second run packs nothing.
- `tests/importflow` (the import) still passes 81 / 81.

## What is experimental, what is lossy

Nothing is lossy: every archive holds the exact bytes of the files it replaces, and the readers check CRC-32 / XXH64.

Experimental:
- **PACK GAME FILES** ran once in the rendered experimental client on the developer's library (2026-11-07 22:50, `builds/sol_garage/game
  files`): 147 435 files, 19 126 MB -> 7 814 MB in 103.7 s, read back by the client afterwards. Re-packing a level that already has an
  archive while the game holds that archive open replaces it by delete + rename (fine on macOS; on Windows it relies on delete-while-open
  sharing). The import's packing has run in the .NET harness (`tests/rxarchive` D), not in a rendered import yet.
- **A packed library needs an archive-reading client.** Release builds before this (the `release` line, v1.0.x) read only loose files: a
  library packed by experimental cannot be used by them (they share the library folder when they find the same one: the client's
  `game files`, PlayerPrefs `ReflexGameDir`, `REFLEX_GAME_DIR`). `rxpack extract <archive> <library>` for every archive turns a packed
  library back into a loose one. Symlink overlays of a library's loose files (the REFLEX-lead `builds/alive/combined/map-*` folders) lose
  the files that were packed.
- The import extracts loose first and packs at its end: its peak is the loose size plus one archive. Writing the extracted assets
  straight into the archives would lower that to the packed size; the import's resume / duplicate / optional-database rollback logic works
  on loose files, which is why it packs afterwards.
- Loose, by design: the shared databases' index files (`Database/<name>.dx9.database`), the top-level configs, `Videos/` (Bink, already
  compressed), `OnFieldCrowd/`, `Browse/`, `alive-levels.json`. A loose file always wins over a packed one, so an edit or an Alive
  re-import keeps working after packing (PACK GAME FILES packs a level's new loose files again, carrying over the rest of its archive).
- The `Tracks/` folder holds every level, the test / UI scenes (`Ctrl_*`, `UI_Compound`, `UIScene`) too: "level archives".
- The packed Mods/Tracks `.rxtrack` is listed by the custom track browser and mounted by the core; the client hashes its files for online
  play exactly like a folder's. A host serves a packed track's files from the archive; a rider who downloads it gets a loose folder.
- The container (RXAR 1) and the track metadata (version 1) are new: a later change bumps the container version (readers refuse versions
  they do not know).
- Not done: the oracle CLI build (`native/game/tools/build_cli.sh`) compiles `src/fmt_*.c` and so the archive code too; it was not rebuilt.

## Sweep

`builds/rxpack/sweep.sh` (not in git; the loop is in this section's words): for each of the 82 levels, an event of its kind (NAT_ / ST_
National 10, SX_ Supercross 8, FR_ FreeRide 9, TRX_ Terracross 17, PX_ Powercross 16, FMX_ Freestyle 7, location -1) through
`alive_race_test` for 300 frames from the loose library and from the packed one, stdout compared:

- **69 levels load and run identically** (every NAT_ / SX_ / FR_ / TRX_ / PX_ / FMX_ Reflex level and all 26 Alive levels);
- 3 identical failures: `SX_FresnoB_LD`, `SX_KingstonA_LD`, `SX_ManchesterA_LD` (layout fragments, not standalone events) abort at
  frame 3 from the loose library too;
- 10 not riding levels skipped (`Ctrl_*` test scenes, `UIScene`, `UI_Compound`); their archives are verified byte for byte like the rest;
- 0 differences.
