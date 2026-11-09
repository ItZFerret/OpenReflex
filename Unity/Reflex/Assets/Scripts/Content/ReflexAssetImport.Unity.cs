// The game files import's Unity side: where the client is, where its game files go, the import's notes in the player log.
//   ClientDirectory      the folder the player sees the game in (ReflexMods.ClientRoot: beside OpenReflex.exe / the .app, or the launcher's
//                        client folder: -reflexClientRoot)
//   DefaultDestination   <client>/game files
//   Destination          where the game files are: the folder named by "game-files-location.txt" at the client root when the player put
//                        them on another drive (the import's NOT ENOUGH SPACE page, CHOOSE ANOTHER DRIVE), else DefaultDestination. The
//                        pointer is a plain text file beside Mods/, so every later start, every launcher update (the client folder is kept)
//                        and a launcher "move client" (it moves the pointer along: the path in it is absolute) find the files again. The
//                        install lookup (ReflexInstall.Candidates) reads Destination, and the folder is also saved as the game folder
//                        (PlayerPrefs ReflexGameDir); a drive letter that changed is looked for on the other drives (ReflexInstall.Remapped).
//                        Mods/ stays at the client root: the launcher's OPEN MODS FOLDER and every mod reader look for it there, and it is
//                        small - only the ~20 GB of game files move.
using System;
using System.IO;
using UnityEngine;

namespace Reflex.Port.Content
{
    public static partial class ReflexAssetImport
    {
        public const string PointerName = "game-files-location.txt";

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void LogToPlayer()
        {
            DefaultLog = s => Debug.Log(s);
            // the packed library (Assets.rxdata, Tracks/<level>.rxtrack: RxFiles) is read by the core library: loaded on first use, from
            // any thread (the paths are taken here, on the main thread)
            RxFiles.Log = s => Debug.Log(s);
            string dataPath = Application.dataPath; bool editor = Application.isEditor;
            RxFiles.EnsureCore = () =>
            {
                if (!string.IsNullOrEmpty(Reflex.Port.RxCore.RxNativeLoader.LoadedPath)) return;
                string path = Reflex.Port.RxCore.RxNativeLoader.ResolvePath(Reflex.Port.RxCore.RxNativeLoader.PluginDirectory(dataPath, editor));
                string project = Path.GetDirectoryName(dataPath);
                Reflex.Port.RxCore.RxNativeLoader.Load(path, editor ? Path.Combine(string.IsNullOrEmpty(project) ? dataPath : project, "Temp", "OpenReflex") : null);
            };
        }

        /// The folder the player sees the game in: beside OpenReflex.exe (Windows, any drive or share) or the .app (macOS), or the client
        /// root override (ReflexMods: the launcher's client folder, which every installed version shares).
        public static string ClientDirectory
        {
            get
            {
                string root = ReflexMods.ClientRoot;
                if (root != null) return Path.GetFullPath(root);
                string data = Path.GetFullPath(Application.dataPath);
                return Path.GetFullPath(ReflexPaths.ClientRootFor(data, Application.platform == RuntimePlatform.OSXPlayer) ?? Directory.GetParent(data).FullName);
            }
        }
        public static string DefaultDestination => Path.Combine(ClientDirectory, "game files");
        public static string PointerFile => Path.Combine(ClientDirectory, PointerName);

        /// The game files folder another drive holds (the pointer's path), null when they are in the client folder.
        public static string MovedDestination
        {
            get
            {
                try
                {
                    string file = PointerFile;
                    if (!File.Exists(file)) return null;
                    string line = ReflexPaths.FirstPathLine(File.ReadAllText(file), ReflexInstall.WindowsHost);
                    if (string.IsNullOrEmpty(line) || !ReflexPaths.IsRooted(line)) return null;
                    return Path.GetFullPath(line);
                }
                catch (Exception e) when (e is IOException || e is UnauthorizedAccessException || e is ArgumentException || e is NotSupportedException) { return null; }
            }
        }
        public static string Destination => MovedDestination ?? DefaultDestination;

        /// Remembers where the game files are: the pointer at the client root for a folder outside the client, none for the default.
        public static void RememberDestination(string destination)
        {
            string full = Path.GetFullPath(destination);
            if (ReflexPaths.SamePath(full, DefaultDestination)) { try { if (File.Exists(PointerFile)) File.Delete(PointerFile); } catch (Exception e) { Debug.LogWarning("[import] pointer not removed: " + e.Message); } return; }
            File.WriteAllText(PointerFile, "# OPEN REFLEX: your imported game files are in this folder (chosen on the import's space page).\n" +
                                           "# Delete this file to import into the client folder again.\n" + full + "\n");
            Debug.Log("[import] game files location saved: " + full + " (" + PointerFile + ")");
        }

        // ------------------------------------------------------------------------------------------------ help

        public const string HelpName = "HELP - IMPORTING YOUR GAME.txt";
        /// The player's help for a failed import (also README.md "If the import fails"): written next to Mods/ by the import pages, opened by
        /// their OPEN HELP button. Plain words, the real error texts, what to do.
        public const string HelpText =
@"OPEN REFLEX - IF THE IMPORT FAILS
=================================

OPEN REFLEX plays with your own copy of MX vs ATV Reflex. On the first start it copies the game's files out of
your install into a ""game files"" folder (once; nothing is downloaded and your game is not changed). The import
checks everything first and tells you what is wrong. If it still fails, find your message below.

NOT ENOUGH SPACE  (""Win32 IO returned 112"", ""disk full"", ""There is not enough space on the disk"")
  The Steam version needs about 15 GB free (14.8 GB of files plus 1 GB to spare). The import shows exactly how much
  it needs and how much your drive has. If the drive is too full, pick CHOOSE ANOTHER DRIVE: the game files go into
  an OpenReflex folder on that drive, and OPEN REFLEX always looks there from then on, after updates too (the place is
  saved in game-files-location.txt next to the Mods folder). Your mods and settings stay where they are.
  To free space instead: empty the Recycle Bin, uninstall games you don't play, run Windows' Disk Cleanup.
  A failed or cancelled import removes everything it wrote. A folder named "".rxi-"" plus 8 letters left next to
  ""game files"" (from an older version, or OPEN REFLEX was closed mid-import) is an unfinished import: the import
  offers RESUME or CLEAN UP for it, or delete it yourself.
  Steam Deck: the import can put the game files on the microSD card.

MODS IN THE GAME FOLDER  (""While reading MXTables_DLC008.dx9.database: Specified cast is not valid"")
  Community gear, bike kits and tracks are installed by copying files into the game's Database folder. Steam never
  removes them: uninstalling the game and ""Verify integrity of game files"" leave added files where they are.
  The import lists them (REVIEW MODS) and can:
    MOVE TO MODS     gear and kit packs go to Mods\Gear, beta-slot tracks to Mods\Tracks. OPEN REFLEX uses them
                     from there, any number at once. (The original game no longer sees them.)
    LEAVE THEM OUT   import the game without them. They stay in the game folder.
  Files this version can't read are always left out, and everything else is imported.
  To delete them yourself: in Steam, right-click MX vs ATV Reflex > Manage > Browse local files, open Database,
  and delete the files the import listed, for example for a pack in slot 010:
    MXTables_DLC010, MXUI_DLC010, RiderDLC010_graphicskit, HR_RiderDLC010_graphicskit (each .dx9.database and .dx9.package)
  Keep the game's own DLC files (MXTables_DLC003-005, MXUI_DLC001-005, RiderDLC01-02, MXDLC01, ATVDLC01 and their HR_ twins).

A GAME FILE CAN'T BE READ
  Your install may be damaged. In Steam: right-click MX vs ATV Reflex > Properties > Installed Files > Verify
  integrity of game files, then import again. From a disc: reinstall the game.

ACCESS DENIED / THE IMPORT STOPS AT RANDOM FILES
  An antivirus may be scanning or blocking the new files (the import already waits and retries a few times). Allow
  the MXA Launcher's folder (or OPEN REFLEX's folder) in your antivirus and try again, or pick another place.

THE FOLDER'S PATH IS TOO LONG
  Windows allows 260 characters per path. Pick a short folder such as D:\OpenReflex.

ONEDRIVE
  Don't put the game files in a OneDrive folder (Desktop and Documents often are): OneDrive would upload them and may
  move them off your PC. If your game itself is in OneDrive, right-click its folder > Always keep on this device.

FINDING YOUR GAME
  Pick the folder that contains the Database folder (it also holds MXReflex.exe), usually
  ...\steamapps\common\MX vs ATV Reflex. In Steam: right-click the game > Manage > Browse local files.

UNINSTALLING
  If you moved the game files to another drive, delete that OpenReflex folder yourself after uninstalling.

STILL STUCK?
  On the error page, press COPY ERROR DETAILS (it copies the error, your version and system; nothing is sent) and
  include it in a bug report on the project's issue tracker.
";

        /// Writes the help next to Mods/ (when missing or out of date); its path, or null when it cannot be written.
        public static string EnsureHelp()
        {
            try
            {
                string file = Path.Combine(ClientDirectory, HelpName), text = HelpText.Replace("\r\n", "\n").Replace("\n", Environment.NewLine);
                if (!File.Exists(file) || File.ReadAllText(file) != text) File.WriteAllText(file, text);
                return file;
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException || e is NotSupportedException) { Debug.Log("[import] help not written: " + e.Message); return null; }
        }

        /// Why the import cannot be written beside the client (a read-only disc, Program Files, a share without write access), null when it can.
        public static string DestinationProblem() => DestinationProblem(ClientDirectory);
        /// Why nothing can be written in folder (created when missing: an "OpenReflex" folder on another drive), null when it can.
        public static string DestinationProblem(string folder)
        {
            try
            {
                Directory.CreateDirectory(folder);
                string probe = Path.Combine(folder, StagePrefix + "write-" + Guid.NewGuid().ToString("N").Substring(0, 8));
                File.WriteAllBytes(probe, new byte[1]); File.Delete(probe);
                return null;
            }
            catch (Exception e) when (e is UnauthorizedAccessException || e is IOException || e is ArgumentException || e is NotSupportedException)
            {
                return "OPEN REFLEX can't write to " + folder + " (" + e.Message + ").";
            }
        }
    }
}
