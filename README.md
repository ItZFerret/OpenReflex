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
- mods: `.rxmod` bikes, skins and kit packs (`tools/rxmod`), custom `.rxtrack` tracks (`tools/rxtrack`), the MX vs ATV Alive
  content importer (for players who own Alive), and the MX vs ATV Untamed track importer (the 12 stadiums of the player's own disc as
  custom tracks; written, not yet tested in the game: `core/plan/UNTAMED_TRACKS.md`)

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

## Building on macOS

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

builds `builds/windows-player/OpenReflex/` and a zip. It needs a static MinGW-w64 zlib in `builds/windows/plugin/deps`
(`include/zlib.h`, `lib/libz.a`; see "Building on Windows" below for how to make one, or set `REFLEX_WINDOWS_DEPS`) and an LGPL
ffmpeg build under `builds/thirdparty/ffmpeg-win64` (see the script's header for where to get it).

To check that the scripts compile without opening Unity (needs a Unity project that was imported once, for its package assemblies):

```bash
LIB=Unity/Reflex/Library/ScriptAssemblies tools/unity_compile.sh /tmp/reflex-compile
```

## Building on Windows

On Windows the Unity project opens in the Windows editor, and the core (`rxcore.dll`) is built with the same bash script the Mac
uses, run under **WSL** (Ubuntu) with the MinGW-w64 cross compiler. These steps haven't been run end to end on a Windows machine
yet: please report what breaks.

**1. Install the tools.**

- Git for Windows, and Unity Hub with Unity **6000.6.1f1** (the Windows editor includes Windows build support).
- WSL with Ubuntu, from an administrator PowerShell (restart when it asks):

  ```powershell
  wsl --install -d Ubuntu
  ```

- In the Ubuntu terminal, the compiler and Python:

  ```bash
  sudo apt update && sudo apt install -y mingw-w64 make python3 curl
  ```

**2. Clone the repository** to a Windows folder, so both Unity and WSL can reach it; a short path such as `C:\dev\OpenReflex`
avoids path-length problems. In WSL that folder is `/mnt/c/dev/OpenReflex`. The repository's `.gitattributes` keeps the shell
and Python scripts' line endings LF, which bash needs.

```powershell
git clone https://github.com/ItZFerret/OpenReflex.git C:\dev\OpenReflex
```

**3. Build a static zlib for MinGW-w64** (the core links it), in WSL. Any zlib 1.3.x release works; this installs it where the
build scripts look for it:

```bash
cd /mnt/c/dev/OpenReflex
mkdir -p builds/thirdparty && cd builds/thirdparty
curl -LO https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz && tar xzf zlib-1.3.1.tar.gz && cd zlib-1.3.1
D=/mnt/c/dev/OpenReflex/builds/windows/plugin/deps
make -f win32/Makefile.gcc PREFIX=x86_64-w64-mingw32- BINARY_PATH=$D/bin INCLUDE_PATH=$D/include LIBRARY_PATH=$D/lib install
```

**4. Build the core**, in WSL. `RX_WIN_CC=gcc` uses the MinGW-w64 GCC alone (the Mac build defaults to LLVM clang; set
`RX_WIN_LLVM=<llvm bin folder>` instead if you have clang installed):

```bash
cd /mnt/c/dev/OpenReflex
RX_WIN_CC=gcc RX_BUILD_JOBS=4 core/tools/build_windows.sh core/build-windows
```

This writes `core/build-windows/rxcore.dll`.

**5. Use it in the Unity editor.** Copy `core\build-windows\rxcore.dll` to `Unity\Reflex\Assets\Plugins\x86_64\` (DLLs are
git-ignored), or point the editor at it with an environment variable before starting Unity Hub:

```powershell
setx REFLEX_RXCORE_PATH "C:\dev\OpenReflex\core\build-windows\rxcore.dll"
```

Open `Unity\Reflex` in Unity Hub. On first play it asks for your MX vs ATV Reflex install and imports it, as the player does.
The in-level video screens need an ffmpeg: set `REFLEX_FFMPEG` to an `ffmpeg.exe`, or have one on `PATH`.

**6. Build a Windows player** from PowerShell (close the editor first, or build from a copy of `Unity\Reflex`):

```powershell
$env:REFLEX_RXCORE_PATH = "C:\dev\OpenReflex\core\build-windows\rxcore.dll"
$env:REFLEX_OPEN_PLAYER = "C:\dev\OpenReflex\builds\windows-player\OpenReflex\OpenReflex.exe"
& "C:\Program Files\Unity\Hub\Editor\6000.6.1f1\Editor\Unity.exe" -batchmode -nographics -quit `
  -projectPath C:\dev\OpenReflex\Unity\Reflex -executeMethod Reflex.Port.EditorTools.OpenReflexBuild.BuildWindowsBatch `
  -logFile C:\dev\OpenReflex\builds\windows-player-build.log
```

The log ends with `[open-build] Succeeded Windows x64` when it worked. For the videos in the player, put an LGPL ffmpeg build
(`ffmpeg.exe` and its DLLs, e.g. BtbN's `ffmpeg-n9.0-latest-win64-lgpl-shared` release) in an `ffmpeg` folder next to
`OpenReflex.exe`.

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
