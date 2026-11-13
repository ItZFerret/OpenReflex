# MX vs ATV Untamed tracks: disc check and import

The player's own MX vs ATV Untamed (Xbox 360) disc, turned into OPEN REFLEX custom tracks. The conversion itself was developed outside this
repository: the Python Untamed converter (the reference: about 21k lines on the build path plus 10k of verification; its design notes are
its `docs/DESIGN.md`). It converts the 10 supercross, 2 EnduroCross and 2 Minimoto stadiums and the 4 freestyle arenas natively:
textures by block copy, meshes by vertex
re-declaration, collision, materials onto stock Reflex shader templates, the ground as a 1 ft heightfield drawn with the stock supercross
dirt (since its dirt-look rounds of 2026-11-11) and the race line from the disc's `AI/<level>-<mode>.tdf07`. It writes one `Beta_Sx_Track_Slot_1.dx9.{database,level,package,scene}` set (a freestyle arena: `Beta_Fmx_Track_Slot_1`, a free-ride track) and a `mod.ini` per track, installed
by hand as `Mods/Tracks/Untamed/<short>-v<n>/` (TrackMods.cs). All 12 pass that converter's offline checks and its headless race (11 AI,
3 laps on rxcore; the two EnduroCross arenas warn only because Untamed's EnduroCross has no holeshot line). In game, Norton and Foothills
have been ridden so far.

The client now imports them itself (v1, below): players need nothing but their disc and their Reflex game files, as with the Alive
importer (`core/plan/ALIVE_TRACKS.md`). The converter is a C# port of the Python one (`core/plan/UNTAMED_CONVERT.md`: its contract, tests
and numbers). Not done yet: a Unity compile and an in-game check of the import (see "Open").

## The supported disc

`tools/formats/untamed_disc.py <disc image or extracted folder>` prints the layout, the title id, each stadium's five file sizes with the
SHA-256 of its database, scene and race line (`--package` adds the package's), and the disc's other stadiums (standard library only;
`--csharp` prints the table rows of `UntamedImporter.KnownTracks`). The disc read so far: `mxvsatvuntamed.iso`, 2,946,695,168 bytes, a bare
game-partition XDVDFS image (descriptor at 0x10000, 462 files), title id `545107DE`, media id `76BBD0C5`, disc 1 of 1 (the last two from
`default.xex`). An extracted copy of the same disc gives the same rows. Another revision or region shows "differs from the supported disc"
until its rows are added after the table's (the check takes the first row a level's files match).

| Level | Short | Mode | Location (MX07Tables Free Ride name) | Package MB | Race line |
|---|---|---|---|---:|---|
| Norton_STADIUM | norton | sx | Norton Coliseum | 48.9 | AI/Norton_STADIUM-sx.tdf07 |
| Capital_City_STADIUM | capitalcity | sx | Capital City Stadium | 49.3 | AI/Capital_City_STADIUM-sx.tdf07 |
| Columbus_STADIUM | columbus | sx | Columbus Stadium | 51.7 | AI/Columbus_STADIUM-sx.tdf07 |
| Denton_STADIUM | denton | sx | Denton Stadium | 48.2 | AI/Denton_STADIUM-sx.tdf07 |
| HarrisPark_STADIUM | harrispark | sx | Harris Park Stadium | 53.6 | AI/HarrisPark_STADIUM-sx.tdf07 |
| Foothills_STADIUM | foothills | sx | Foothills Stadium | 50.5 | AI/Foothills_STADIUM-sx.tdf07 |
| WestStone_STADIUM | weststone | sx | West Stone Gardens Stadium | 40.9 | AI/WestStone_STADIUM-sx.tdf07 |
| Jefferson_STADIUM | jefferson | sx | Jefferson Stadium | 51.0 | AI/Jefferson_STADIUM-sx.tdf07 |
| Wilford_STADIUM | wilford | sx | Wilford Stadium | 55.4 | AI/Wilford_STADIUM-sx.tdf07 |
| BridgePort_STADIUM | bridgeport | sx | Bridgeport Stadium | 60.6 | AI/BridgePort_STADIUM-sx.tdf07 |
| Fairview_STADIUM | fairview | ec | Fairview Arena | 35.5 | AI/Fairview_STADIUM-ec.tdf07 |
| Lincoln_STADIUM | lincoln | ec | Lincoln Memorial Arena | 35.3 | AI/Lincoln_STADIUM-ec.tdf07 |
| Dyson_STADIUM | dyson | mm | Specter Arena | 33.4 | AI/Dyson_STADIUM-mm.tdf07 |
| Edgewater_STADIUM | edgewater | mm | Edgewater Arena | 35.3 | AI/Edgewater_STADIUM-mm.tdf07 |
| RBWCenter_STADIUM | rbwcenter | fmx | RBW Center Arena | 34.5 | AI/RBWCenter_STADIUM-fmx.tdf07 (freestyle lines) |
| Southbend_STADIUM | southbend | fmx | Southbend Arena | 30.6 | AI/Southbend_STADIUM-fmx.tdf07 (freestyle lines) |
| THQSpectrum_STADIUM | thqspectrum | fmx | THQ Spectrum Arena | 29.2 | AI/THQSpectrum_STADIUM-fmx.tdf07 (freestyle lines) |
| WestLake_STADIUM | westlake | fmx | West Lake Arena | 32.6 | AI/WestLake_STADIUM-fmx.tdf07 (freestyle lines) |

EnduroCross races as supercross: Reflex has no EnduroCross race, and a `Beta_Ec` stem would make the track Free Ride only. The Minimoto
stadiums (added 2026-11-12) race as supercross too, on their `-mm` race lines, with full-size bikes (the jumps are built for minibikes;
not rescaled). The four freestyle arenas (added 2026-11-12, the reference's final arena round v3) are free-ride tracks: OPEN REFLEX has
no freestyle event for custom tracks, so they are installed in a `Beta_Fmx_Track_Slot_1` slot, which `TrackMods.KindOf` lists under
FREE RIDE only ("FREESTYLE · FREE RIDE"). They are built on SX_FortDodge reduced to its free-ride kit (no race line, gate, 30 s board or
cut-scenes; the 12 free-ride starts on Untamed's start pad). Untamed's steel ramps keep Untamed's own riding faces: collision-only
terrain triangles that leave the heightfield and go into the collision of the ramp they lie on (DriveOn 1, the stock FMX ramp-top
values); on THQ Spectrum four ramps whose steel Untamed placed up to 0.7 ft off their riding faces are moved onto them. The ramps'
perforated grating, their navy / red side panels, RBW Center's dark funbox frame and the Untamed logo on the jumbotron are drawn as in
the reference. The `-fmx.tdf07` (Untamed's AI freestyle lines) is read by the terrain stage only, never shipped. Not supported yet:
every `*_WORLD` outdoor level (the four outdoor freestyle venues among them).

## M1 (done): choose the disc, recognize the stadiums

GAME LIBRARY > MX vs ATV UNTAMED > CHOOSE UNTAMED DISC (`OpenFrontEnd/OpenShell.UntamedImport.cs`), the same gamepad folder / file browser
as the Alive import: the disc image (.iso) or the extracted disc folder (the one holding `Database` and `AI`). FIND TRACKS runs
`Content/UntamedImport/UntamedImporter.Scan` on a worker thread:

- a file opens as a disc image (`DiscVolume`, AliveContainers.cs); one whose `default.xex` names another title is refused with its title id;
- a folder opens as a `FolderVolume` that also lists `*.tdf07` and `default.xex` (its optional file filter);
- each stadium's four `.xenon` files and its race line must exist with the table's sizes, then the SHA-256 of the files the converter reads
  whole (database, scene, tdf07: about 18 MB for the 12) must match; the package hash is in the table for the import step. A file that
  can't be read marks its stadium, not the whole scan;
- the custom track converted before is found in `Mods/Tracks/Untamed/<short>` or `<short>-v<n>` (the highest n); `imported` is the
  importer's own (`<short>` with `untamed-import.json`), `handInstalled` the `<short>-v<n>` copies;
- `inTheWay` says what of the player's own is where the track goes (`UntamedImporter.DestinationProblem`, the import's own test:
  `Untamed/<short>` without the import manifest whatever it holds, a file of that name, a link, `Mods/Tracks/Untamed` being a file).
  The list blocks such a stadium with that sentence.

The scan writes nothing, and one that is cancelled or fails closes the sources it opened. The browser marks a folder `✓ UNTAMED` when it holds `Database` with stadium levels and `AI` (the `Database`
folder alone lacks the race lines).
Headless test: `sh tests/untamed_import/run.sh <scratch> <disc image> [<extracted folder>] [<another game's disc image>]` (image, folder,
both, another title, a non-disc file, an installed track, and the table against `untamed_disc.py`). It builds with Unity's bundled SDK on
macOS or with Visual Studio Build Tools' Roslyn and the .NET 8 runtime on Windows (Git Bash). The UI itself is not covered by it.

## The import (v1)

`Content/UntamedImport/UntamedImporter.Import.cs` (`UntamedImporter.Import`, Unity-free) and the screens in
`OpenFrontEnd/OpenShell.UntamedImport.cs`.

**Screens.** After FIND TRACKS the 18 tracks (14 stadiums, 4 freestyle arenas) are a list to choose from. Ready ones are chosen. One that is installed already is marked
and not chosen: IMPORTED (choosing it imports it again and replaces it) or INSTALLED BY HAND (choosing it imports a new copy, the older
one stays). A stadium that can't be imported says why (its disc files, the Reflex game files, a folder in the way). IMPORT n TRACKS
shows the track in progress with its step, a bar for the track and one for the whole import, and CANCEL. The results list every
stadium (installed, failed with the reason, cancelled), the folder, the hand-installed copies still there, and where to find the
tracks, listed in a section of their own: FREE RIDE and RACE > MX vs ATV Untamed (every track under `Mods/Tracks/Untamed/`; `OpenContent.UntamedGroup`), not under CUSTOM TRACKS. `TrackMods.Refresh()` lists them without a restart.

**What Import does**, per chosen stadium, on a worker thread:

1. Checks the stadium's five disc files against the supported disc again, the package included (the scan leaves it out), and the
   stadium the jumbotron picture is made from (WestStone for the supercross, Minimoto and freestyle arenas, Fairview for the EnduroCross
   arenas: the whole disc is needed).
2. Builds the track with `UxBuildTrack.Build` (each track's options: `UxTrackOptions.ForLevel`) into a staging folder inside
   `Mods/Tracks`: `.untamed-<8 hex>/w` (the work store, 90 - 155 MB), `/o` (the track), `/previous` (a replaced track until the new one
   is in), `/lock` (held open). A dot name, so `TrackMods` never lists it.
3. Checks the result: the rules `TrackMods` lists a track by (`UxPackage.Rules`: one complete set, a level name the core takes, a
   database that covers the package, a scene with a Base section), `mod.ini` as the client reads it (name, kind supercross or, for a
   freestyle arena, freeride, the picture), and every file re-hashed against `sha256.txt` and the builder's result.
4. Writes `untamed-import.json` (importer and converter versions, the disc files' SHA-256, the disc image's file name, the kind of
   library, the options, the seven files with size and SHA-256, whether the four `.dx9` files are the tested build's, the time) and
   moves the folder to `Mods/Tracks/Untamed/<short>/`. The manifest holds no path: players share a track folder as it is.

Rules it keeps:

- The disc and the Reflex game files are only read. The library is `UxDirectoryLibrary` over `ReflexInstall.Dir`: an original install, or
  the client's game files (loose or packed), read without Unity.
- Before anything is written: the library holds what the converter reads (`UntamedImporter.LibraryProblem`: SX_FortDodge and 15 more
  stock levels, 7 game databases, `AnimationSets.bxml`; the EnduroCross arenas also FR_Alpine and SX_DLC01_FreightYard, the freestyle
  arenas NAT_Concord: RBW Center's tuff blocks wear its Acerbis wrap), free disk
  space (the tracks' estimate + 320 MB while importing), on Windows the path lengths, and `Mods/Tracks/Untamed` not being a file or a
  link. A failure says which and stops the import.
- A track is there complete or not at all: one folder move. A failed or cancelled stadium leaves nothing; the others go on. Cancel
  works between stadiums and inside one. The staging folder is removed however the import ends; one left by a crash or by closing
  the game is removed when the shell first starts, and by the next FIND TRACKS or import; a replaced track still in it goes back
  first. The shell destroyed while a scan or an import runs (a ride starts, the game quits) cancels it.
- `Untamed/<short>` is replaced only when it carries the importer's manifest, nothing in it changed since (every file re-hashed; no
  other file but `.DS_Store`, `._*`, `Thumbs.db` and `desktop.ini`, and no folder: any other name, dot names included, is the
  player's) and the player chose it. Any other `Untamed/<short>` stops that stadium. `Untamed/<short>-v<n>` is never changed.
- What stops a stadium is found before it is converted: something in the way (`DestinationProblem`), a changed track, and on Windows
  a track to replace that has a file open (the game keeps the package of a track ridden since it started open; the shell closes those
  first with `TrackMods.Release`).
- The importer removes only its own folders (staging, a replaced track), walking them by hand: a link in one is removed as a link and
  never entered, so nothing outside is deleted or changed.
- The four `.dx9` files are compared with the tested build's hashes (the track rows of `tests/untamed_convert/check/expected.txt`,
  a table in the importer); a track that differs is still installed and the result says so.

**Test** (headless, .NET 8, Windows): `sh tests/untamed_import/import/run.sh <scratch> [quick|all|both]` with `UX_DISC_ISO` and
`UX_REFLEX` or `UX_CLIENT_LIB` (and `UX_DISC_FOLDER` for the differing-disc checks). 2026-11-11, from the disc image:

| Run | Result |
|---|---|
| `quick`, an original (Steam) install | 102 checks pass: Norton and Lincoln imported (22 s; the tested build), the manifest (no path in it), re-import, an injected failure, cancel inside the first and inside the second stadium (ends 0.1 s later), a cancelled scan (the disc image closed at once), `Mods/Tracks/Untamed` being a file, folders and a file in the way (flagged by the scan, refused by the import before converting), a track in use (refused before converting), a changed track, a file / dot-named file / dot-named folder of the player's in a track, the desktops' files (pass, go with the replaced folder), a folder not made by the importer, a hand-installed `norton-v3` untouched, a crashed import's staging folder with links to a folder outside (that folder as it was, its read-only file still read-only), the library / space / path errors, a changed package byte, the sources unchanged |
| `quick`, the client's game files (packed) | 98 checks pass (run without the extracted disc: no differing-disc checks) |
| `all`, an original (Steam) install | 12 of 12 imported in 134 s (9.8 - 14.5 s each, 16 threads; another run, beside the reference's verifier, 151 s), 619 MB installed, all 12 the tested build; at most 211 MB staged; peak working set 1.7 - 1.9 GB, managed heap 119 - 150 MB between tracks; every stock file read is one `LibraryProblem` checks |

Outside Unity the whole game assembly (428 files, `OpenShell.UntamedImport.cs` and `TrackMods.cs` included) compiles with 0 errors
against the installed client's Unity 6000.6.1f1 assemblies, with its Mono class library and with .NET Standard 2.1; and the importer run
under that client's Mono (`mono-2.0-bdwgc.dll` in a small embedding host) gives the tested build (all 12, en-US and tr-TR, before
the review's fixes; Norton, Denton, Wilford and Lincoln again after them; with the dirt-look converter Norton, Foothills, Fairview
and Lincoln, tr-TR: 103 s for the four).

## Why the converter can't simply be copied in

| Input | What it gives each track | Can it ship? | Replacement |
|---|---|---|---|
| A community Digital Earthworks supercross slot ("a Sketch of a sx Tack", 2015) | the clone base: 951 of its 962 assets (37.8% of Norton's package), 174 entities (gate kit, anchors, scenario actors, crew chiefs, environment, HFTerrain), its sky | No: 820 of its assets are stock Reflex bytes, the rest third-party work with no licence | clone a stock SX level from the player's imported library (SX_FortDodge: same start gate, drop gate and gate collision as the DE kit, 41 of its 45 anchors, all 59 scenario actors) |
| A community free-ride slot (HuCkiT, 2013) | one value, the dirt tint (0.329, 0.290, 0.239, 1) | No | the base terrain material's own value (the default since the dirt-look round), or the constant (`--dirt-tint const`, the older builds) |
| A library built from the Reflex install (8 shaders, 8 exemplar materials, 9 entity prototypes, GL_Hay) | every converted material and entity is a filled-in copy | No (stock bytes) | read at import from the client's game files (`Rfx.Install.Read`), as `AliveInstall` already copies crowd.shader and Water_Wake.shader |
| Microsoft XMemCompress (`mszip.dll`) | LZX for every new package resource | No | an own LZX encoder (`UxLzxEncoder`: packages 6.4 % larger; the core and `RfxData.Lzx` decode them) |
| numpy, scipy, Pillow | LANCZOS resampling and BC1/BC3 encoding of the ground textures, convex hulls and seeded sampling for 27 box-fit collisions, float16 rounding | not in the client | portable C# versions, first adopted in the Python reference so the outputs can be compared |

So an imported track's `.package` differs in bytes from the Python reference's (and with it the offsets in `.database`); every resource
decodes to the same bytes, and `.level` and `.scene` are identical. The C# output is deterministic; its own hashes are the tested build.

## Reuse from the Alive importer

Untamed's resource layouts are Reflex PC's, byte-swapped (texture v6 vs v4, surface / model v4 with the same group record, collis v3, the
0xC0FFFEEE scene container, BXML 0x103EA, `.tdf07` = the Reflex tdf without its size word). What it shares with Alive is the Xbox 360 layer.

- As is: `DiscVolume` / `FolderVolume`; `LzxDecoder` with the `RfxData.Lzx` framing (md5-identical to the converter on all 15,143 LZX
  resources of the 12 levels); `AliveTextures.TiledOffset` / `PackedMipOffset` / the PC texture assembler; `AliveRace.WalkTdf`; the shader
  readers `AliveConverters.Shaders` / `AliveRecipe.Disassemble` (the converter's shader module is a port of them).
- Not as is: `XMem.Decode` (rejects every Untamed resource: their chunks have no terminator); `AliveTextures.ConvertBc` for LIN_DXT1 / LIN_DXT5
  (187 textures; Untamed's linear pitch differs); Alive's DXN path (Untamed needs G16R16F, and its BC4 rounding differs);
  `AliveConverters.Material` (different cull, blend-op and sampler value maps).

## Milestones

1. **M2 (done): read the disc's content.** `UxDisc` and the readers of `UNTAMED_CONVERT.md` ("The foundation").
2. **M3 (done, by the converter's owner): the Python reference on the stock base.** SX_FortDodge from the player's library; the releases
   norton-v7, capitalcity-v5, columbus-v6, denton-v5, harrispark-v6, foothills-v6, weststone-v5, jefferson-v5, wilford-v5,
   bridgeport-v5, fairview-v7, lincoln-v7 pass its checks and the headless race (the arenas' v7 = its water-glare round of
   2026-11-12: the water reflects a copy of the level cube with the stadium lights dimmed, no white bloom blobs). They carry its two rounds of 2026-11-11 after the
   owner's first ride ("the dirt is very bright and uses the same patterns everywhere"): the stock supercross ground (SX_FortDodge's
   own dirt layers; a lighter racing surface, a darker infield and an edge line generated around each track) and, in the two
   arenas, a shore colour taken from that ground.
3. **M4 (done on Windows): the stages and the builder in C#**, compared with those releases on all 12 levels (`UNTAMED_CONVERT.md`);
   the ground and shore of the releases before them (norton-v6 ... lincoln-v5) remain as builder options, still compared.
4. **M5 (v1 written, not yet run in the game): the import**, above.
5. **M6: polish.** An MX vs ATV Untamed filter in the browser (like MX vs ATV Alive), an ENDUROCROSS label, menu pictures from the disc's UI.

## Open

- **In-game check.** No Unity editor on the machine this was written on (the compile and the Mono run above are outside it). Not run:
  Unity's own compile and asset import, the screens, a gamepad on them, an import inside the client (its time and memory there are
  unknown), riding an imported track.
- **Memory after an import.** The conversion needs over 1 GB for a while (peak working set 1.6 - 1.9 GB on .NET 8). Under the client's
  Mono in the embedding host the process stayed at 1.0 - 1.7 GB private bytes after an import (1,361 MB after 4 stadiums, the same after
  18 more collections with 50 MB live): Boehm keeps the heap for reuse and did not give it back. Whether the player does the same is
  not measured; the shell logs the managed heap (used / reserved) when an import ends. If it holds there: lower the converter's
  transient peak (not found by capping threads: 2 threads left 1,047 MB), or tell the player to restart after a large import.
- **Two clients on one Mods/Tracks, macOS / Linux.** A staging folder whose `lock` is held open belongs to a running import and is left
  by the clean-up (first shell start, FIND TRACKS, import). Windows enforces that lock between processes; Unity's Mono elsewhere may
  only track it inside one process, in which case a second client started on the same folder could remove a running import's staging
  folder (that stadium then fails; nothing half-written is published). Not run on macOS.
- **macOS.** The converter and the import test have run on Windows only. `tests/untamed_convert/check/expected.txt` must hold there too (the ground maps
  take `exp`, `log` and `pow` from the converter's own arithmetic, not from the platform's C library, for that reason).
- **The look of the new ground and shore in the game.** The reference's owner rode norton-v7 ("dirt is much better and water is
  fine"); the other 11 tracks' ground and the arenas' new shore colour have only passed the offline checks, in the reference and here.
- **A stadium imported next to a hand-installed copy** is listed twice, the import under MX vs ATV Untamed and the hand-installed copy under CUSTOM TRACKS, with nearly the same name (both `mod.ini` say
  "<name> (Untamed)"); the results tell the player the older folder is still there.
- **A track ridden since the start and then re-imported**: `TrackMods.Release` closes its package file first; models and textures
  the client already loaded from it stay until the next start (the same bytes unless the converter changed).
- Menu pictures from the disc's UI.
- The outdoor levels (the four outdoor freestyle venues among them).
- **The freestyle arenas in game through the import.** The reference's owner rode its first arena builds (v1: "they look good"; the
  ramps' missing grating was fixed after that); the final builds (v3) and this importer's identical files have passed the offline
  checks and the reference's headless free-ride load check only. To look at in game: navy / red ramp side panels, RBW Center's dark
  funbox frame, the Untamed logo on the jumbotrons, the wheels on the steel of all five THQ Spectrum ramps, and THQ ramp 5216299,
  whose Untamed riding faces are about 0.7 ft wider than its drawn plate on one side (Untamed's own geometry, kept: a wheel can sit
  on an invisible strip just past the plate edge).
