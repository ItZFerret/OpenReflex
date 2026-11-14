// The client's Mods folder: <client root>/Mods/<kind>/ (Bikes: BikeMods.cs; EngineSounds: EngineSoundPacks.cs; Tracks: TrackMods.cs; Music: ReflexCustomMusic.cs). The client root is the
// folder the player sees the game in:
//   macOS     the folder that holds OpenReflex.app (Application.dataPath = <root>/OpenReflex.app/Contents)
//   Windows   the folder that holds OpenReflex.exe (Application.dataPath = <root>/OpenReflex_Data; Linux the same)
//   editor    the Unity project folder (Application.dataPath = <project>/Assets; Unity/*/Mods/ is git-ignored)
//   override  -reflexClientRoot <dir> on the command line or REFLEX_CLIENT_ROOT (test players point it at a scratch folder; the MXA Launcher
//             points it at its own client folder, so Mods and the imported game files outlive the per-version installs of updates)
// PlayerRoot is the same rule without the override: the files the client ships beside itself (ffmpeg) stay there.
// "Mods" and the kind folder are found in any case ("mods/bikes" works); missing ones are created with a short README.txt on the first
// start (EnsureLayout; a read-only location only logs). Paths are built with Path.Combine only, so both separators, spaces and any
// Unicode name work; the junk the desktops leave in folders (.DS_Store, Thumbs.db, desktop.ini, AppleDouble "._x" files, __MACOSX) is
// never taken for content.
using System;
using System.IO;
using UnityEngine;

namespace Reflex.Port.Content
{
    public static class ReflexMods
    {
        public const string ModsName = "Mods", BikesName = "Bikes", EngineSoundsName = "EngineSounds", TracksName = "Tracks", MusicName = "Music", GearName = "Gear";
        static string s_root, s_player;

        /// The client root for a player's data path (pure: the rule of the file header). macPlayer: a macOS player (dataPath = .app/Contents).
        /// Either separator ('/' or '\\'), so a Windows path reads the same on any machine.
        public static string ClientRootFor(string dataPath, bool macPlayer) => ReflexPaths.ClientRootFor(dataPath, macPlayer);
        /// The folder of a path (either separator; a drive root "C:\\" or "/" keeps its separator, a UNC share is a root), null at the top.
        public static string Parent(string path) => ReflexPaths.Parent(path);

        /// The client root (see the file header); null when Unity's paths are not known yet (first use is on the main thread).
        public static string ClientRoot
        {
            get
            {
                if (s_root != null) return s_root.Length > 0 ? s_root : null;
                string o = Arg("-reflexClientRoot") ?? Environment.GetEnvironmentVariable("REFLEX_CLIENT_ROOT");
                if (!string.IsNullOrEmpty(o))
                {
                    try { s_root = Path.GetFullPath(ReflexPaths.Clean(o, Path.DirectorySeparatorChar == '\\')); return s_root; }   // (any drive, a share, quotes)
                    catch (Exception e) when (e is ArgumentException || e is NotSupportedException || e is PathTooLongException) { Debug.LogWarning("[Mods] ignoring the client root '" + o + "': " + e.Message); }
                }
                try { s_root = ClientRootFor(Application.dataPath, Application.platform == RuntimePlatform.OSXPlayer) ?? ""; }
                catch (Exception) { return null; }   // (off the main thread before the first main-thread use)
                return s_root.Length > 0 ? s_root : null;
            }
        }
        /// The folder that holds the player itself (the rule of the file header, never the override); null when Unity's paths are not known yet.
        public static string PlayerRoot
        {
            get
            {
                if (s_player != null) return s_player.Length > 0 ? s_player : null;
                try { s_player = ClientRootFor(Application.dataPath, Application.platform == RuntimePlatform.OSXPlayer) ?? ""; }
                catch (Exception) { return null; }   // (off the main thread before the first main-thread use)
                return s_player.Length > 0 ? s_player : null;
            }
        }
        /// Tests: use this root (null: the rule again).
        public static void SetRootForTest(string root) { s_root = root; }

        static string Arg(string name)
        {
            try { var a = Environment.GetCommandLineArgs(); int i = Array.IndexOf(a, name); if (i >= 0 && i + 1 < a.Length) return a[i + 1]; }
            catch (Exception) { }
            return null;
        }

        /// A child folder of dir named name in any case (null: none).
        public static string ChildIgnoreCase(string dir, string name)
        {
            try
            {
                if (string.IsNullOrEmpty(dir) || !Directory.Exists(dir)) return null;
                foreach (var d in Directory.GetDirectories(dir)) if (string.Equals(Path.GetFileName(d), name, StringComparison.OrdinalIgnoreCase)) return d;
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException || e is ArgumentException) { }
            return null;
        }

        /// <client root>/Mods/<kind> (an existing folder in any case, else the path it would have); null without a client root.
        public static string Folder(string kind)
        {
            string root = ClientRoot; if (root == null) return null;
            string mods = ChildIgnoreCase(root, ModsName) ?? Path.Combine(root, ModsName);
            return ChildIgnoreCase(mods, kind) ?? Path.Combine(mods, kind);
        }

        /// A file or folder name the desktops leave behind (never content): .DS_Store, Thumbs.db, desktop.ini, AppleDouble "._x", __MACOSX,
        /// other hidden dot names.
        public static bool IsJunk(string name)
        {
            if (string.IsNullOrEmpty(name)) return true;
            if (name.StartsWith(".", StringComparison.Ordinal)) return true;   // .DS_Store, ._MXs.dx9.database, .Trashes, .git ...
            return string.Equals(name, "Thumbs.db", StringComparison.OrdinalIgnoreCase) || string.Equals(name, "desktop.ini", StringComparison.OrdinalIgnoreCase)
                || string.Equals(name, "__MACOSX", StringComparison.OrdinalIgnoreCase) || string.Equals(name, "$RECYCLE.BIN", StringComparison.OrdinalIgnoreCase)
                || string.Equals(name, "System Volume Information", StringComparison.OrdinalIgnoreCase);
        }

        /// dir relative to root with '/' separators ("Honda/CRF 450"), whichever separators the two use; null when dir is not under root.
        public static string RelativeId(string root, string dir) => ReflexPaths.RelativeId(root, dir);

        // ------------------------------------------------------------------------------------------------ first start

        const string BikesReadme =
@"OPEN REFLEX - BIKE MODS
=======================

Drop a bike mod's folder in here. A bike mod is any folder that holds the mod's .dx9.database and
.dx9.package files (for example MXs, HR_MXs, MX_GraphicKits, HR_MX_GraphicKits, MX_Exhausts ...).
Make your own folders and sub-folders to keep them organised - every folder is searched:

    Mods/Bikes/New Bike Models/MXs.dx9.database
    Mods/Bikes/Factory bikes/2010 pack/MXs.dx9.database

Mods never replace the original game: the original bikes stay, and every bike a mod re-skins shows up in
GARAGE > VEHICLE as a vehicle of its own (a card under the original, tagged MOD; CLASS > MODDED ONLY lists
just those). It is owned and priced like the bike it re-skins; picking its card equips it, picking the
original's card goes back. Graphic kits and parts the mod changes show a MOD tag in the garage strips.

Optional: a mod.ini in the mod's folder names the mod and its bikes in the menus:
    name = New Bike Models
    author = Your name
    description = One line about the mod
    thumbnail = preview.png

    ; what each bike really is: <the game's bike (its name in the garage, or its number)> = <Brand Model>
    ; the first word is the brand line, the rest the model line; a brand of several words uses a bar.
    ; <bike>.logo = a picture in the mod's folder (png / jpg) shown as the brand logo on the stage.
    [Bikes]
    Blaze 250F = Honda CRF250R
    Blaze 450F = Gas Gas | MC 450F
    Blaze 250F.logo = honda.png
    ; (a mod.json can say the same: {""bikes"":[{""bike"":""Blaze 250F"",""name"":""Honda CRF250R"",""logo"":""honda.png""}]})

.RXMOD BIKES (new bikes made in Blender)
A .rxmod file is a whole bike - its own model and textures - drawn over one of the game's bikes, whose
power, handling, price and ownership it keeps 1:1. Drop the .rxmod file in here (any folder): each bike in it
shows in GARAGE > VEHICLE as a MOD card under the original it rides as, credited to its author.
Making one (Blender + your textures -> .rxmod): see the guide next to the game's tools,
tools/rxmod/README.md (core/plan/RXMOD.md in the source), or RXMOD guide.md in this folder.

Do NOT copy mod files into the game's own Database folder.
";
        const string EngineReadme =
@"OPEN REFLEX - ENGINE SOUND MODS
===============================

Drop MX Bikes engine sound packs in here (a folder with engine.scl, sfx.cfg and the .wav files).
Make your own folders - every folder is searched. Pick a pack per bike in GARAGE > VEHICLE > ENGINE SOUND,
or for every bike in OPTIONS > AUDIO > ENGINE SOUNDS. An optional pack.ini (name = ..., author = ...) names it.
";

        const string TracksReadme =
@"OPEN REFLEX - CUSTOM TRACKS
===========================

Put each custom track in a folder of its own in here, with the track's files inside it:

    Mods/Tracks/Farm 14/Beta_Nat_Track_Slot_4.dx9.database
    Mods/Tracks/Farm 14/Beta_Nat_Track_Slot_4.dx9.level
    Mods/Tracks/Farm 14/Beta_Nat_Track_Slot_4.dx9.package
    Mods/Tracks/Farm 14/Beta_Nat_Track_Slot_4.dx9.scene

That is all: the track shows up in FREE RIDE > CUSTOM TRACKS (every track can be free-ridden) and, for a
National (Beta_Nat_Track_Slot_N) or Supercross (Beta_Sx_Track_Slot_N) track, in RACE > CUSTOM TRACKS too.
Make your own sub-folders to keep them organised - every folder is searched.

A track downloaded as a .zip / .rar / .7z can be put in here as it is: the game extracts it into a folder
of its own next to it (Mods/Tracks/<archive name>/) the next time it lists the custom tracks, and keeps the
archive. The game reads what the file really is (a RAR archive named .zip is fine). RAR and 7z archives are
extracted with the system's own tar (macOS; Windows 10 version 1803 or newer - the tar of older Windows 10
versions cannot read RAR 5 archives: extract those with 7-Zip or WinRAR). If an archive cannot be
installed, CUSTOM TRACKS lists it with the reason.

No beta slots to juggle: the original game needed each track copied over one of its beta slot files
(Beta_Track_Slot_1, Beta_Nat_Track_Slot_4 ...), one track per slot. Here every track keeps its own folder and
any number of tracks may use the same slot name; the game's own files are never touched. Do NOT copy track
files into the game's Database folder.

The menus name a track after its folder (""1138-farm-14-mx"" shows as FARM 14 MX). Optional: a mod.ini in
the track's folder says more:
    name = Farm 14
    author = Your name
    description = One line about the track
    thumbnail = preview.png
    ; kind = national | supercross | freeride   (normally read from the file names)
(a mod.json can say the same: {""name"": ""Farm 14"", ""author"": ""..."", ""thumbnail"": ""preview.png""})
A picture named preview / thumbnail / screenshot (.png / .jpg) in the folder is used as the thumbnail too.

Custom tracks keep no records or best laps.
";

        const string GearReadme =
@"OPEN REFLEX - RIDER GEAR AND BIKE KITS
=====================================

Drop community gear packs and bike graphics kits in here, exactly as you downloaded them: the .zip / .rar / .7z
itself (an archive holding more archives is fine) or the extracted folder. These are the original PC game's
""DLC"" kit packs - eight files named after a DLC slot, for example:

    MXTables_DLC010.dx9.database / .package            the pack's table (what it adds, its prices, its names)
    MXUI_DLC010.dx9.database / .package                its names in the menus
    RiderDLC010_graphicskit.dx9.database / .package    rider gear (jerseys, pants, gloves, helmets, boots ...)
    HR_RiderDLC010_graphicskit.dx9.database / .package the same, in detail for the garage
    (MXDLC0NN_graphicskit / HR_MXDLC0NN_graphicskit instead for a bike's graphics kit)

Keep each pack in its own folder (the game makes one for an archive); make sub-folders as you like - every
folder is searched. There is no slot limit: any number of packs work at once, and packs that use the same DLC
slot (DLC010 ...) never replace each other. The game's own files are never touched - do NOT copy pack files into
the game's Database folder (a pack already installed there the original way still works as in the original game,
one pack per DLC slot; moving it in here is better).

Rider gear shows up in GARAGE > RIDER under its gear type (JERSEY, HELMET, BOOTS ...) like the stock gear,
in the brand line the pack names (a Fox jersey colour joins the Fox line) and tagged MOD; a bike kit shows up
in GARAGE > VEHICLE > GRAPHICS KIT of the bike(s) it was made for. Pick one to wear it; pick a stock piece to go
back. If you remove a pack, whatever you wore from it goes back to your stock piece.

A pack that cannot be read is listed in the garage with the file and the reason.
";

        const string MusicReadme =
@"OPEN REFLEX - YOUR MUSIC
========================

Put your own songs in here (.mp3, .ogg or .wav; make your own sub-folders, every folder is searched):

    Mods/Music/Rock/Artist - Title.mp3
    Mods/Music/Chill/Some Album/01 - Song.ogg

OPTIONS > AUDIO > YOUR MUSIC picks how they play:
    OFF      the soundtrack only (OPTIONS > AUDIO > SOUNDTRACK)
    MIX IN   your songs are played among the soundtrack's songs in the races (the menus keep the game's themes)
    ONLY     your songs replace the game's soundtrack, in the menus and the races (the game's soundtrack
             comes back while this folder has no songs)
The races keep the game's own timing: the music starts when the game starts it, fades in and out as the
game does, and ducks under the pause menu. MUSIC and MUSIC VOLUME in the same page apply to your songs.

The NOW PLAYING pop-up (OPTIONS > AUDIO) shows the title and artist from the file's tags (ID3 for .mp3, the
comments of an .ogg) or, without tags, from the file name: ""Artist - Title.mp3"". Press N to skip to the next
song. The songs are shuffled so what you heard lately comes last and no artist plays twice in a row.

GAME LIBRARY > MX vs ATV ALIVE and MX vs ATV UNTAMED put the soundtracks of your own discs in ""MX vs ATV Alive""
and ""MX vs ATV Untamed"" here. Those two folders are the games' soundtracks, not your music: OPTIONS > AUDIO >
SOUNDTRACK plays them (ALL SONGS, or ALIVE / UNTAMED SONGS ONLY) whatever YOUR MUSIC is.

Nothing is copied or uploaded: the files are played from here. Songs that Unity cannot decode are skipped
(copy-protected files, .ogg files with Opus audio, a file that is not what its name says).
";

        static bool s_layoutDone;
        /// Creates <client root>/Mods/Bikes, Mods/EngineSounds, Mods/Tracks and Mods/Music with their README.txt when missing (once per start; a location that
        /// cannot be written only logs). Not in batch-mode runs without an explicit client root (builds, tests).
        public static void EnsureLayout()
        {
            if (s_layoutDone) return; s_layoutDone = true;
            try { if (Application.isBatchMode && Arg("-reflexClientRoot") == null && Environment.GetEnvironmentVariable("REFLEX_CLIENT_ROOT") == null) return; }
            catch (Exception) { return; }
            foreach (var (kind, text) in new[] { (BikesName, BikesReadme), (EngineSoundsName, EngineReadme), (TracksName, TracksReadme), (MusicName, MusicReadme), (GearName, GearReadme) })
            {
                string dir = Folder(kind); if (dir == null) return;
                try
                {
                    Directory.CreateDirectory(dir);
                    string readme = Path.Combine(dir, "README.txt");
                    if (!File.Exists(readme)) File.WriteAllText(readme, text.Replace("\r\n", "\n").Replace("\n", Environment.NewLine));
                }
                catch (Exception e) when (e is IOException || e is UnauthorizedAccessException || e is NotSupportedException) { Debug.Log("[Mods] could not create " + dir + ": " + e.Message); }
            }
        }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Startup()
        {
            _ = ClientRoot; _ = PlayerRoot;   // (Unity's paths: main thread)
            EnsureLayout();
            Debug.Log("[Mods] client root " + (ClientRoot ?? "(none)") + "; bikes " + Folder(BikesName) + "; tracks " + Folder(TracksName) + "; gear " + Folder(GearName) + "; music " + Folder(MusicName));
        }
    }
}
