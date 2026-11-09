# OPEN REFLEX

OPEN REFLEX is a fan-made, open source re-implementation of **MX vs ATV Reflex** (THQ / Rainbow Studios, 2009) for macOS and
Windows. The game's logic is hand-written C (`core/`, the "OPEN REFLEX core") that reads the original game's own files; Unity 6
(`Unity/Reflex`) draws it, plays its audio and runs its menus.

> **Not affiliated with, endorsed by or sponsored by THQ Nordic, Rainbow Studios or any rights holder.** MX vs ATV and Reflex are
> trademarks of their owners. This repository contains **no game files**: you need your own copy of MX vs ATV Reflex (the PC
> version, e.g. from Steam). The game imports it on first start; nothing is downloaded.

## What's in it

This is the **single-player** build. It has:

- **Career**, **Race** (National, Supercross, Waypoint, Freestyle and the other event types), **Free Ride** with its challenges,
  **Practice** (unlimited laps on your own with lap times) and **MotoSkills**
- the **Garage** (vehicles, gear, tuning, graphics kits) and the championship weekend
- **Physics Studio** (edit and share vehicle physics configs) and the OPEN REFLEX riding options (air helpers, Free Ride borders)
- enhanced lighting, weather, replays and photo mode, ultrawide support
- mods: `.rxmod` bikes, skins and kit packs (`tools/rxmod`), custom `.rxtrack` tracks (`tools/rxtrack`), and the MX vs ATV Alive
  content importer (for players who own Alive)

Online play, accounts and online leaderboards are not part of this repository.

## Layout

| Path | What it is |
|---|---|
| `core/` | the game core in C11 (`librxcore`): file readers (`src/fmt_*`), the game's functions (`game/`), the session API Unity calls (`api/`, `include/rx_api.h`). Rules and conventions: `core/README.md`; design notes: `core/notes`, `core/plan` |
| `Unity/Reflex/` | the Unity 6000.6.1f1 project: rendering, audio, input, menus, the importer (`Assets/Scripts`) |
| `tools/` | modding tools (`rxmod`, `rxtrack`), the Python readers of the game's formats they use (`formats`), the offline compile check (`unity_compile.sh`), helpers |
| `tests/` | test harnesses and showcase fixtures (most need a local copy of the game files) |
| `scripts/` | build helpers (Windows cross build, packaging) |
| `native/` | the earlier native drafts; with the `core/oracle` adapters a test oracle only, never shipped |

## Building

Requirements: macOS on Apple Silicon, Xcode command line tools (clang), Python 3, and Unity **6000.6.1f1** installed through Unity
Hub at its default path. The Windows player is cross-built on the Mac with Homebrew `mingw-w64` and LLVM.

```bash
core/build.sh
```

builds `core/build/librxcore.dylib`.

```bash
core/tools/build_mac_player.sh
```

builds the core, stages a copy of the Unity project and produces `builds/player/OpenReflex.app`.

```bash
core/tools/build_windows_player.sh
```

builds `builds/windows-player/OpenReflex/` and a zip. It needs the Windows zlib/capstone libraries
(`scripts/windows/build_deps_windows.sh`, or set `REFLEX_WINDOWS_DEPS`) and an LGPL ffmpeg build under
`builds/thirdparty/ffmpeg-win64` (see the script's header for where to get it).

To check that the scripts compile without opening Unity (needs a Unity project that was imported once, for its package assemblies):

```bash
LIB=Unity/Reflex/Library/ScriptAssemblies tools/unity_compile.sh /tmp/reflex-compile
```

## Playing

Start the player, point it at your MX vs ATV Reflex install and let it import the game files (about 15 GB free space needed). The
help for import problems ships next to the `Mods` folder as `HELP - IMPORTING YOUR GAME.txt`; the same text is in
`Unity/Reflex/Assets/Scripts/Content/ReflexAssetImport.Unity.cs`.

## Credits

- **itswillyferret**: project lead, responsible for the entire project
- **boneboy**: the OPEN REFLEX logo
- **THQ / Rainbow Studios**: the original MX vs ATV Reflex

Third-party code and assets and their licenses are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## License

OPEN REFLEX is free software under the **GNU General Public License version 3** ([LICENSE](LICENSE)): you may use, study, share
and change it; versions you distribute must stay under the GPL with their source available. The license covers this project's own
code and documentation. It does not cover the original game, its files or trademarks, or the third-party material listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md), which keeps its own licenses.
