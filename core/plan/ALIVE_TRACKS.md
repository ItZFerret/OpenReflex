# MX vs ATV Alive tracks: inventory and port status

Branch `w-alive`. Sources (read-only): the Alive disc image (XGD2, 387 files, 2.89 GB of game files; extracted by
`_re/formats/alive_xdvdfs.py` to `_re/out/alive_disc`) and the level DLC packages of the title's content folder
`5451087B/00000002` (LIVE STFS; `_re/formats/alive_stfs.py` validates every block's SHA-1 down from the volume descriptor's top
hash: `_re/out/alive_dlc/<pack>`, Sliderock in `_re/out/sliderock_canyon`). Inventory data: `_re/formats/alive_inventory.py`
(`_re/out/alive_inventory.json`); display names are the game's own (ActivityLocations -> ActivityTypeLocations -> the store
product's English string).

26 levels: 18 on the disc, 3 in the Stewart Compound, 2 each in the Quarry Nationals and SX Events packs, Sliderock Canyon.
Every level has 12 authored Free Ride starts, so every level is installed as a **Free Ride** map. Since w-aliveai (2026-11-02) the 22
levels with a race activity (every one but FR_Dunes, FR_Quarry, FR_DLC_JSComp, FR_DLC_Hills) also race with AI: `core/plan/ALIVE_RACING.md`.

| Level | Alive name | Kind | Source | Package MB | Collision | Forest / grass / water | HF decals | Animated (deferred) | Particles |
|---|---|---|---|---:|---:|---|---:|---:|---:|
| FR_Dunes | Devil's Playground | Free Ride | disc | 68.7 | 28 | yes / yes / yes | 15 | 298 | 32 |
| FR_Quarry | Bryn Carreg Quarry | Free Ride | disc | 66.0 | 63 | yes / yes / yes | 11 | 20 | 0 |
| NAT_Farm | Les Champs MX Park | National | disc | 69.2 | 87 | yes / yes / - | 12 | 329 | 10 |
| NAT_Farm_B | Sous le Pont | National | disc | 66.7 | 143 | yes / yes / - | 13 | 409 | 15 |
| NAT_Farm_C | Maison de MotoX | National | disc | 65.8 | 104 | yes / yes / - | 11 | 419 | 13 |
| NAT_Northwest | Onsenki Park | National | disc | 58.3 | 93 | yes / - / 4 | 5 | 398 | 30 |
| NAT_Northwest_B | Sanko Speedway | National | disc | 65.7 | 77 | yes / - / 4 | 5 | 375 | 62 |
| NAT_Northwest_C | Yukiyama MX | National | disc | 59.4 | 74 | yes / - / 4 | 5 | 493 | 1 |
| NAT_Southwest | Grandview | National | disc | 77.0 | 102 | yes / yes / - | 22 | 431 | 212 |
| NAT_Southwest_B | Piston Valley | National | disc | 79.9 | 142 | yes / yes / - | 21 | 421 | 251 |
| NAT_Southwest_C | Catapult Canyon | National | disc | 80.9 | 130 | yes / yes / - | 28 | 402 | 64 |
| NAT_Tropic | 45 Palms MX | National | disc | 79.0 | 112 | yes / yes / - | 27 | 373 | 9 |
| NAT_Tropic_B | Oceanview | National | disc | 77.8 | 111 | yes / yes / - | 29 | 416 | 4 |
| NAT_Tropic_C | Bamboo Hills | National | disc | 79.6 | 190 | yes / yes / - | 27 | 387 | 2 |
| ST_Farm | La Ferme du Vert | Short track (Rhythm Racing) | disc | 61.7 | 29 | yes / yes / - | 6 | 92 | 0 |
| ST_Northwest | Kinoko | Short track | disc | 62.1 | 68 | yes / - / 4 | 7 | 140 | 4 |
| ST_Southwest | MX Battlegrounds | Short track | disc | 72.5 | 51 | yes / yes / - | 6 | 180 | 56 |
| ST_Tropic | Foggy Ridge | Short track | disc | 65.1 | 37 | yes / yes / - | 4 | 140 | 6 |
| FR_DLC_JSComp | The Stewart Compound | Free Ride compound (JS7) | Stewart Compound | 68.3 | 68 | - / yes / - | 17 | 116 | 105 |
| NAT_DLC_JSComp_A | Stewart MX South Track | National (JS7) | Stewart Compound | 72.9 | 67 | - / yes / - | 19 | 115 | 105 |
| NAT_DLC_JSComp_B | JS7 Pro Loop | National (JS7) | Stewart Compound | 73.1 | 65 | - / yes / - | 19 | 118 | 105 |
| NAT_DLC_Quarry_A | Quarry Basin | National | Quarry Nationals | 86.9 | 265 | yes / yes / yes | 15 | 331 | 220 |
| NAT_DLC_Quarry_B | Edgerock MX | National | Quarry Nationals | 87.1 | 242 | yes / yes / yes | 17 | 331 | 376 |
| SX_DLC_SX01 | Lexington | Supercross (stadium, 123 billboard crowds) | SX Events | 49.9 | 49 | - / - / - | 5 | 97 | 25 |
| SX_DLC_SX02 | Williamsburg | Supercross (stadium, 123 billboard crowds) | SX Events | 48.7 | 29 | - / - / - | 5 | 106 | 25 |
| FR_DLC_Hills | Sliderock Canyon | Free Ride | Sliderock Canyon | 85.3 | 108 | yes / yes / yes | 4 | 102 | 48 |

Not tracks: `UI_World` (the front end's world), `Model_Viewer_Tool` (a developer level). The vehicle / gear packs of the content
folder and the `00009000` avatar items carry no levels.

## The soundtrack (2026-11-14)

`Content/AliveImport/AliveMusic.cs` on `Content/ReflexGameMusic.cs` (the installer shared with the Untamed soundtrack: its rules are in
`core/plan/UNTAMED_TRACKS.md` "The soundtrack"). The 23 songs of the game's jukebox go into the client's music folder as
`Mods/Music/MX vs ATV Alive/<Artist> - <Title>.ogg`, the Alive soundtrack of OPTIONS > AUDIO > SOUNDTRACK (ALL SONGS, ALIVE SONGS ONLY).

**On the disc.** `Database/Music.xenon.{database,package}` (80.6 MB): `MusicConfig/MusicAudioMap.bxml` lists the jukebox
(`Music/Jukebox/Track`: Package, BankName, SongName, ArtistName; the names in `AliveMusic.Songs` are the game's own, "when I Feel"
capitalised), the scenario scores and the three menu themes (not songs: left out). Each song is a package `TrackNN` with one asset
`MUS_INGAME_NN.soundbnk` (LZX): a 0x40-byte bank header, then an FSB4 bank of one sample in XMA (sample mode `0x01000000`), 44.1 kHz
stereo, its data whole 2048-byte packets to the bank's end. (`Track00` repeats `Track01`'s bank and is not in the jukebox.)

**The conversion.** Unity can't decode XMA. The client's ffmpeg can (the Windows package ships an LGPL build in `<client root>/ffmpeg`
for the videos; `REFLEX_FFMPEG`, the usual install folders and `PATH` too: `FfmpegBinkSource.Locate`): its FSB demuxer does not open
these banks ("format 0x1 is not implemented"), so the XMA data is put in a RIFF WAVE with an XMA2WAVEFORMATEX (one stream, stereo,
44.1 kHz, 64 KB blocks, the bank's sample count), which its `xma2` decoder reads. Each song is encoded once, at the import, to Ogg
Vorbis (libvorbis quality 6, about 190 kbps; an ffmpeg without libvorbis: MP3 with libmp3lame, VBR 2) with the title, artist, album
"MX vs ATV Alive" and year 2011 as tags. Checked: the XMA data's size, sample count and SHA-256 against the table (the supported
disc's songs); the file decodes again with no error; an Ogg file ends within 4096 samples of the song's length. The bundled ffmpeg
9.0 (2026-11-14): all 23 in 48 s, 96.5 MB, every length within the bound; a spectrogram and the levels (-10 to -13 dB RMS) are music.
Without an ffmpeg (or one with neither encoder) the row says why and nothing is written.

**Screens.** One more row of the Alive list (SOUNDTRACK · 23 SONGS), chosen unless every song is there; it is converted after the
levels and bikes (about a minute), with its own progress line; its result row says whether SOUNDTRACK plays it.

**Test.** `tests/untamed_import/import/run.sh <scratch> music` with `UX_ALIVE_ISO` and `REFLEX_FFMPEG`: no ffmpeg (refused, nothing
written), a cancel while converting (nothing left), the 23 files with their tags and lengths, the music scan's view, again (nothing
converted, 0.1 s), a deleted song converted again; the disc image unchanged.

## Racing (w-aliveai, 2026-11-02): 22 levels race with AI, overlay `map-a6-race`

`builds/alive/combined/map-a6-race` (over `builds/alive/<level>/conversion-a6` and `builds/alive/base-clean`) adds each race level's
track definition, lap pacing and start-gate subscene; Free Ride is unchanged. Headless 3-lap races with 11 AI finish on all 22
(lap times, crashes, the plan: `core/plan/ALIVE_RACING.md`). RACE > MX vs ATV Alive lists them.

## Round 3 (w-alive3, 2026-10-28): client-side look fixes, overlay unchanged

The overlay is still `builds/alive/combined/map-a5` (no conversion change). The round-3 client (`builds/alive/client-a6`) adds the MapShadow sun mask (SX01 / SX02,
FR_Quarry, the Stewart Compound), Alive's fog ramp and squared fog amounts, sRGB bounce bricks and an Alive key factor of 1.5; SX01 / SX02 and the ten
compared levels pass the rendered ride again. Details, evidence and limits: `_re/formats/ALIVE_CONVERSION.md` ("Round 3 findings"), `core/plan/QUEUE.md`.

## Status (2026-10-28, round 2 / w-alive2): all 26 installed, rideable and verified as Free Ride maps

Overlay: `builds/alive/combined/map-a5` (every level; needs the w-alive2 client). Each level also has `builds/alive/<level>/map-a5`
over `builds/alive/<level>/conversion-a5`. Round 1 (`map-a4` over `conversion`) is kept untouched. Pipeline, rendering translation and limitations: `_re/formats/ALIVE_CONVERSION.md`.

- **Native:** the strict Free Ride regression (`tests/alive_tracks/regression.py`, 1,800 frames, throttle then coast / brake;
  logs in `builds/alive/combined/regression-a5`). Its "authored reset zone" resets are the national tracks' out-of-bounds
  material (zone value 7, reason 6).
- **Rendered:** real menus and real input, ride 480 frames, 14 views (`tests/alive_tracks/run.py`; runs in
  `builds/alive/runs/`; screenshots in `~/Desktop/Reflex Review/Alive Tracks/round2/<level>/`, before (round 1) / after (map-a5)
  and `before-after.jpg`; client `builds/alive/client-a5`). A "wreck respawn" is a crash into
  solid authored scenery (spectators, stadium walls); the rendered drive differs slightly from the native one, so the crashes
  differ too.

| Level | Alive name | Converted / pending | Tree types | Albedo programs (reduced + whole) | Native Free Ride regression | Rendered ride | Run |
|---|---|---|---:|---:|---|---|---|
| FR_DLC_Hills | Sliderock Canyon | 641 / 15 | 6 | 26 + 2 whole | PASS (0 resets, 113 ft) | PASS (161 ft, 60 ft/s) | 20261028T155638Z-FR_DLC_Hills |
| FR_DLC_JSComp | The Stewart Compound | 953 / 13 | 0 | 30 + 0 whole | PASS (0 resets, 376 ft) | PASS (176 ft, 61 ft/s) | 20261028T155952Z-FR_DLC_JSComp |
| FR_Dunes | Devil's Playground | 391 / 12 | 5 | 16 + 10 whole | PASS (0 resets, 392 ft) | PASS (160 ft, 56 ft/s) | 20261028T160305Z-FR_Dunes |
| FR_Quarry | Bryn Carreg Quarry | 423 / 15 | 4 | 9 + 0 whole | PASS (0 resets, 552 ft) | PASS (150 ft, 60 ft/s) | 20261028T160625Z-FR_Quarry |
| NAT_DLC_JSComp_A | Stewart MX South Track | 1127 / 15 | 0 | 39 + 0 whole | PASS (0 resets, 346 ft) | PASS (162 ft, 60 ft/s) | 20261028T160927Z-NAT_DLC_JSComp_A |
| NAT_DLC_JSComp_B | JS7 Pro Loop | 1132 / 15 | 0 | 39 + 0 whole | PASS (0 resets, 346 ft) | PASS (122 ft, 51 ft/s) | 20261028T161231Z-NAT_DLC_JSComp_B |
| NAT_DLC_Quarry_A | Quarry Basin | 1046 / 16 | 9 | 32 + 0 whole | PASS (1 resets, 1 authored reset zone, 327 ft) | PASS (113 ft, 47 ft/s) | 20261028T161421Z-NAT_DLC_Quarry_A |
| NAT_DLC_Quarry_B | Edgerock MX | 1040 / 15 | 9 | 32 + 1 whole | PASS (1 resets, 1 authored reset zone, 327 ft) | PASS (196 ft, 60 ft/s) | 20261028T161658Z-NAT_DLC_Quarry_B |
| NAT_Farm | Les Champs MX Park | 1034 / 15 | 9 | 24 + 1 whole | PASS (0 resets, 284 ft) | PASS (149 ft, 53 ft/s) | 20261028T162047Z-NAT_Farm |
| NAT_Farm_B | Sous le Pont | 999 / 15 | 9 | 25 + 1 whole | PASS (0 resets, 333 ft) | PASS (148 ft, 52 ft/s) | 20261028T162247Z-NAT_Farm_B |
| NAT_Farm_C | Maison de MotoX | 938 / 15 | 9 | 25 + 1 whole | PASS (0 resets, 284 ft) | PASS (198 ft, 61 ft/s) | 20261028T162447Z-NAT_Farm_C |
| NAT_Northwest | Onsenki Park | 782 / 15 | 5 | 26 + 1 whole | PASS (0 resets, 118 ft) | PASS (110 ft, 57 ft/s) | 20261028T162754Z-NAT_Northwest |
| NAT_Northwest_B | Sanko Speedway | 837 / 15 | 5 | 26 + 2 whole | PASS (0 resets, 118 ft) | PASS (110 ft, 57 ft/s) | 20261028T162953Z-NAT_Northwest_B |
| NAT_Northwest_C | Yukiyama MX | 823 / 15 | 5 | 27 + 1 whole | PASS (0 resets, 94 ft) | PASS (123 ft, 56 ft/s, 1 wreck respawn) | 20261028T163152Z-NAT_Northwest_C |
| NAT_Southwest | Grandview | 1006 / 15 | 7 | 27 + 2 whole | PASS (0 resets, 224 ft) | PASS (103 ft, 50 ft/s, 1 wreck respawn) | 20261028T163352Z-NAT_Southwest |
| NAT_Southwest_B | Piston Valley | 1034 / 15 | 7 | 28 + 2 whole | PASS (0 resets, 172 ft) | PASS (117 ft, 50 ft/s) | 20261028T163551Z-NAT_Southwest_B |
| NAT_Southwest_C | Catapult Canyon | 1118 / 15 | 7 | 29 + 2 whole | PASS (0 resets, 221 ft) | PASS (109 ft, 50 ft/s, 1 wreck respawn) | 20261028T163752Z-NAT_Southwest_C |
| NAT_Tropic | 45 Palms MX | 1128 / 16 | 9 | 31 + 3 whole | PASS (1 resets, 1 authored reset zone, 186 ft) | PASS (164 ft, 57 ft/s) | 20261028T163952Z-NAT_Tropic |
| NAT_Tropic_B | Oceanview | 1095 / 16 | 9 | 33 + 3 whole | PASS (1 resets, 1 authored reset zone, 185 ft) | PASS (164 ft, 57 ft/s) | 20261028T164152Z-NAT_Tropic_B |
| NAT_Tropic_C | Bamboo Hills | 1124 / 16 | 9 | 36 + 3 whole | PASS (1 resets, 1 authored reset zone, 186 ft) | PASS (164 ft, 57 ft/s) | 20261028T164353Z-NAT_Tropic_C |
| ST_Farm | La Ferme du Vert | 785 / 15 | 9 | 26 + 1 whole | PASS (1 resets, 1 authored reset zone, 150 ft) | PASS (91 ft, 42 ft/s) | 20261028T164554Z-ST_Farm |
| ST_Northwest | Kinoko | 730 / 15 | 5 | 26 + 1 whole | PASS (1 resets, 1 authored reset zone, 110 ft) | PASS (107 ft, 58 ft/s) | 20261028T164841Z-ST_Northwest |
| ST_Southwest | MX Battlegrounds | 803 / 15 | 7 | 28 + 1 whole | PASS (0 resets, 233 ft) | PASS (166 ft, 61 ft/s) | 20261028T165230Z-ST_Southwest |
| ST_Tropic | Foggy Ridge | 876 / 15 | 9 | 30 + 0 whole | PASS (4 resets, 4 authored reset zone, 146 ft) | PASS (110 ft, 49 ft/s) | 20261028T165449Z-ST_Tropic |
| SX_DLC_SX01 | Lexington | 1288 / 14 | 0 | 50 + 10 whole | PASS (0 resets, 308 ft) | PASS (110 ft, 47 ft/s) | 20261028T165649Z-SX_DLC_SX01 |
| SX_DLC_SX02 | Williamsburg | 1276 / 14 | 0 | 51 + 10 whole | PASS (0 resets, 268 ft) | PASS (79 ft, 48 ft/s, 1 wreck respawn) | 20261028T165838Z-SX_DLC_SX02 |

Importer (player path): GAME LIBRARY → IMPORT ALIVE CONTENT. It was tested end to end through the UI
(`tests/alive_tracks/run_import.py`; run `builds/alive/runs/20261028T091340Z-import-FR_Dunes`), and its output is byte-identical
to this pipeline on FR_DLC_Hills, FR_DLC_JSComp, FR_Dunes, NAT_Farm, NAT_Tropic, NAT_Northwest, SX_DLC_SX01 and SX_DLC_SX02 (the
registry's `conversion` field aside).
