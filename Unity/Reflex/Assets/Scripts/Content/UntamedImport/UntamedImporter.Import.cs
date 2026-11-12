// MX vs ATV Untamed (Xbox 360): the import. Each chosen stadium of the player's own disc (UntamedImporter.Scan) is converted by the track
// builder (UxBuildTrack: the disc's level + the stock level SX_FortDodge and a few more stock files of the player's Reflex library) into
// a custom track of the client: <Mods/Tracks>/Untamed/<short>/ = Beta_Sx_Track_Slot_1.dx9.{database,level,package,scene}, mod.ini,
// preview.png, sha256.txt and the import manifest untamed-import.json (TrackMods lists it; core/plan/UNTAMED_TRACKS.md).
//   sources    only read: the disc (the scan's open volumes) and the Reflex library (an IUxLibrary; UxDirectoryLibrary reads an original
//              install or the client's game files, loose or packed, without Unity). Each stadium's five disc files are checked against
//              the supported disc again, the package included (the scan leaves that one out)
//   staging    <Mods/Tracks>/.untamed-<id>/ : 'w' the builder's work store (90 - 155 MB a track), 'o' the track being built, 'previous'
//              a replaced track until the new one is in, 'lock' held open while the import runs. A dot name: TrackMods never lists it.
//              Removed when the import ends, however it ends; one a crashed import left is removed by the next import (a replaced track
//              still in it goes back first)
//   publish    one folder move on the same volume, after the result was checked (the rules TrackMods lists a track by, the files
//              re-hashed against sha256.txt): a track is there complete or not at all
//   replacing  Untamed/<short> is replaced only when it carries this importer's manifest, nothing in it changed since (every file it
//              lists re-hashed; no other file but the desktops' own, no folder) and the caller asks (options.replace). Any other
//              Untamed/<short> stops that stadium, before it is converted (UntamedImporter.DestinationProblem: the list's test too).
//              Untamed/<short>-v<n> (installed by hand) is never changed or removed: the result names it
//   removing   the importer removes only its own folders (staging, a replaced track) and never through a link: a link in one is
//              removed as a link, what it points at stays as it is
//   failures   a stadium that fails or is cancelled leaves nothing behind and the others go on; what holds for every stadium (the
//              library, the folder, disk space, path lengths) is checked before anything is written and throws UntamedImportException
//   manifest   names the disc image / folder and the kind of library, never their paths (a track folder is shared as it is)
// Pure C# (System only): no Python, no native code.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// A precondition of the whole import failed (nothing was written): the message is for the player.
    public sealed class UntamedImportException : Exception
    {
        public UntamedImportException(string message) : base(message) { }
        public UntamedImportException(string message, Exception inner) : base(message, inner) { }
    }

    public enum UntamedImportState
    {
        /// Not reached (only inside a running import).
        Pending,
        /// Converted and installed.
        Imported,
        /// Not installed: error says why. Nothing of it was left behind.
        Failed,
        /// The import was cancelled before or while this stadium was converted. Nothing of it was left behind.
        Cancelled,
        /// The importer's track of this stadium is installed and a re-import was not asked for: left as it is.
        AlreadyImported,
    }

    public sealed class UntamedImportOptions
    {
        /// Replace the track this importer installed before (a re-import). False: such a stadium is reported AlreadyImported.
        public bool replace;
        /// Worker threads of the converter (0: one per processor). The track's files do not depend on it.
        public int threads;
        /// The free bytes of the volume a folder is on, null when unknown (default: System.IO.DriveInfo of the path's root; the client
        /// passes its own, which knows mount points and network shares). Unknown skips the disk space check.
        public Func<string, long?> freeBytes;
        /// The importer's own notes (a staging folder removed, a replaced track put back, a stadium's failure with its stack).
        public Action<string> log;
        /// The converter's log lines (thousands a stadium; null: dropped).
        public Action<string> converterLog;
        /// Tests: called when a stadium is built and checked, before it is published (the stadium, its work store, its staged folder).
        /// An exception fails that stadium.
        public Action<UntamedKnown, string, string> staged;
    }

    /// Where a running import is.
    public sealed class UntamedProgress
    {
        /// The stadium in progress (its disc level and menu name), its place among the chosen (0-based) and their number.
        public string level, name; public int index, count;
        /// What is being done ("Converting: terrain", "Installing").
        public string stage;
        /// 0..1 of this stadium, and of the whole import.
        public float track, overall;
    }

    public sealed class UntamedTrackResult
    {
        public UntamedKnown known;
        public string level, shortName, displayName;
        public UntamedImportState state;
        /// Why it failed (state Failed).
        public string error;
        /// Where it is, relative to Mods/Tracks ("Untamed/norton"; Imported and AlreadyImported).
        public string folder;
        /// It replaced the track imported before.
        public bool replaced;
        /// The four .dx9 files are those of the tested build (TestedBuild: byte for byte the files the converter's tests and the headless
        /// races ran on).
        public bool testedBuild;
        /// The copies installed by hand that are still there ("Untamed/norton-v3" ...): never changed; the player may delete them.
        public List<string> olderCopies = new List<string>();
        /// The installed files (name, size, sha256: the seven the manifest lists); their total size; the package's resources.
        public List<(string name, long bytes, string sha256)> files = new List<(string, long, string)>();
        public long bytes; public int assets;
        public double seconds;
        /// Seconds per step: 'disc check', the converter's steps (UxBuildResult.steps), 'track check', 'publish'.
        public List<KeyValuePair<string, double>> steps = new List<KeyValuePair<string, double>>();
        public List<string> warnings = new List<string>();
        public bool Ok => state == UntamedImportState.Imported;
        public override string ToString() => level + ": " + state + (error != null ? " (" + error + ")" : "") + (folder != null ? " -> " + folder : "");
    }

    public sealed class UntamedImportResult
    {
        /// One per chosen stadium, in the order given.
        public readonly List<UntamedTrackResult> tracks = new List<UntamedTrackResult>();
        /// Mods/Tracks (the full path) and the folder the tracks are in below it.
        public string tracksFolder, folder = UntamedImporter.TracksSubfolder;
        public bool cancelled;
        public double seconds;
        public int Imported => tracks.Count(t => t.Ok);
    }

    public static partial class UntamedImporter
    {
        /// The importer's version in the manifest (the converter's is UxBuildTrack.PORT).
        public const string ImporterVersion = "untamed-import/1";
        /// The staging folders in Mods/Tracks: ".untamed-" + 8 hex digits (a dot name: never taken for a track).
        public const string StagingPrefix = ".untamed-";
        /// The longest path the converter writes below its work store, with the suffix of a file being written (measured on the 12
        /// stadiums: 62 characters + ".tmp" and a counter); the test checks every staged path against it.
        public const int WorkPathBudget = 96;
        /// What a running import needs free besides the tracks themselves: one stadium's work store and its staged folder.
        public const long StagingBytes = 320L << 20;
        const string SlotStem = "Beta_Sx_Track_Slot_1";

        // The four .dx9 files of the tested build (the track rows of tests/untamed_convert/check/expected.txt: what the converter writes from the
        // supported disc and a stock Reflex library, on any machine): short name -> database, package, scene; the level file is the same
        // for every stadium. The test compares this table with that file.
        const string TestedLevelSha256 = "62066933a86159f17112cb2cd8c68be56945974670f4709eec112c80f5d22b94";
        static readonly Dictionary<string, string[]> TestedBuild = new Dictionary<string, string[]>(StringComparer.Ordinal)
        {
            { "norton", new[] { "46e5e572c4831aa939a7758103c46b5af62f2ea6b6f202f769c3b7ace3f7a8ef", "f780311aeaf3c82bd170d3fab6fc4e10cb3699d4c939cf6449ee87c8ffac3488", "599f929bd23d254a89b94abf82e8b3ecdf23c0db81334cabc3dbb204003d4d4c" } },
            { "capitalcity", new[] { "ed002570095a17602d605d6de9a0bec3686f138c4765fc4a19c4866fa41556fe", "e29fe62bdc49cb456185053ec299b12effac169922c6b5cba25c3cb346672b0d", "98fe76e37a1933bcc05dd77134e40ab8f077a73ffb1ae3be8d6db66c734b448c" } },
            { "columbus", new[] { "3c1bd49ef15f2db62d5765bd7c210e6f658f029a792dc67785ad867db608b93a", "a649b54addc9df859129cca745d430722f183602288e3c01a4af799e67aabb43", "9fa25873977c87f759d956bf18f6782464811e54b7b54c66381ad706560e74d8" } },
            { "denton", new[] { "4d342ddfb75d3efce4901e5c8976d620f482a4cfa956cd16174a6b7e9fbc7872", "fc8d4afa8b0088c7c78736489382e78307bd062f88b3bd94e96d4d266165e3f4", "5f9df68ce93a9a3a09a6b8b662ae76c438108b1196e8c642cd721c8784e32567" } },
            { "harrispark", new[] { "547949b20fa030d29ece01243acc1f0eb338bcae3ca1bf7f2efbc986335face4", "03b28291daa70ea79d9629d0e1f77b6e25739208a1e023eae7c6167b074b61a1", "ca4b7528ffeaa4b1e4688720edbe6f696274a6e7caba547d261359a90542c020" } },
            { "foothills", new[] { "1de2756630afd2aedb9180aa251b467ce820a8211d5987a0c2a42a7c01022070", "cd88d8ccccdee24222f89af5ee72f8c17e9ea002ac4a94259ed9e246db67c7b8", "90725ea705c61355e3f89601ffcda39d42a4bc8f0d8f9dd144e57589048828bc" } },
            { "weststone", new[] { "812c17a3f8930519b0c1a20b754beec2647528d145abe24884561973dd5ad366", "0623f80a183be33c55594fa24030ce1788969bb07cd92bbcff5e3ea66bd3674a", "b0a84e58fd05600e4b5ce505a427bc0387e2bc258301980642c6b14fe4d7919e" } },
            { "jefferson", new[] { "6d21f1306852aa106d0a84a41c4f0ef62f0c0808d9cb9d22f698bf39eeb87e52", "ae63dc59cddb7a61ac8bd9623ed228e4b1a90b40d870f467757ea9d0ef90da64", "2de56a8fd890dff3852c9ef5466d16bf4c8dbc680d82c4667e9557e7d1f2456b" } },
            { "wilford", new[] { "41058915dae58da909ab6a7162316ef5ff13c39f76edffa46cb5bb975f2e2fde", "8f1218c2856aeed39180ba3ad6eb19e159fe8116df461ea7eb1d4eb9793544a5", "e53019a6924b050fb12056ffe1e4bdf4454ee6fe1a70cfa92e987ce8f69de642" } },
            { "bridgeport", new[] { "98b74e73408776ec414dab6dca657486721c21e24d9d5ca74497a3f806d72494", "829dc57f24a52abb0fd9dddbbfc9a08aae9d52a097d3bb040b59c58c8d43ad52", "f4bd9c759550864ce262e7f7ebb9292725537c9218138bec4f92cfb65000af72" } },
            { "fairview", new[] { "4590d8756e03b14194bfc2a1fa1f4ec5c2a0e8528a98f17d0cd32789f367c10c", "4f5802d4ac4ada83a0bc7f0a0a1b4d63c81b3557d707a61a7fb58cadc9799801", "ab8dd8c900e41fc895fd1c6e74e6fbffee74bc51cbbd9eb29f95ac162c07606c" } },
            { "lincoln", new[] { "bce986cfe4054da20710812d6c80f57438149cb137535f6d076716257469e551", "c03df2e8036544ad94f40cba0d4a62e98042748db889025008d491a425a96da7", "6779df7a5776493f56cbaa574a9d36d516100bf08ddbdd5ada63dbe1bbbdf176" } },
        };

        /// The sha256 of a file of the tested build ("database", "level", "package", "scene"), null for a stadium outside the table.
        public static string TestedSha256(string shortName, string kind)
        {
            if (!TestedBuild.TryGetValue(shortName, out var row)) return null;
            switch (kind) { case "database": return row[0]; case "package": return row[1]; case "scene": return row[2]; case "level": return TestedLevelSha256; }
            return null;
        }

        /// A track imported before that this converter would write differently: its manifest names another converter version
        /// (UxBuildTrack.PORT) and one of its four .dx9 files is not the tested build's. False for a track of this version (whatever
        /// its files: a library that differs from the tested one is not an update), for no import and for a manifest that does not read.
        public static bool Outdated(string tracksFolder, UntamedKnown k)
        {
            string rel = ImportedFolder(tracksFolder, k);
            if (rel == null) return false;
            var m = ReadManifest(Path.Combine(tracksFolder, rel.Replace('/', Path.DirectorySeparatorChar)));
            if (m == null || m.Get("converter") as string == UxBuildTrack.PORT) return false;
            foreach (object fo in (List<object>)m.Get("files"))
            {
                if (!(fo is JObj f) || !(f.Get("name") is string name)) continue;
                int dot = name.LastIndexOf('.');
                string want = dot < 0 || name.IndexOf(".dx9.", StringComparison.OrdinalIgnoreCase) < 0 ? null : TestedSha256(k.shortName, name.Substring(dot + 1).ToLowerInvariant());
                if (want != null && !string.Equals(f.Get("sha256") as string, want, StringComparison.OrdinalIgnoreCase)) return true;
            }
            return false;
        }

        // ------------------------------------------------------------------------------------------------ the Reflex library

        // What the converter reads of the player's Reflex library for the 12 stadiums (the test records every read of an import of all
        // 12 and asks LibraryProblem about a library without each): the base, the levels its stock parts come from (UxStock's tables:
        // material templates, entity prototypes, the Tuff Blox wraps and bale parts, GL_Hay), and the water's two for the EnduroCross
        // arenas. (level, what it is to the import, EnduroCross arenas only)
        const string StockParts = "a stock level the Untamed tracks take parts from";
        static readonly (string level, string what, bool ecOnly)[] StockLevels =
        {
            ("SX_FortDodge", "the stock supercross stadium every Untamed track is built on", false),
            ("SX_FortDodgeB", StockParts, false), ("SX_Kingston", StockParts, false), ("SX_KingstonB", StockParts, false), ("SX_Manchester", StockParts, false),
            ("SX_ManchesterB", StockParts, false), ("SX_Springfield", StockParts, false), ("SX_SpringfieldB", StockParts, false), ("FR_Gibson", StockParts, false),
            ("Ctrl_AmbientAnimation", StockParts, false), ("FMX_BuenaVista", StockParts, false), ("FMX_BuenaVistaB", StockParts, false), ("FMX_Dover", StockParts, false),
            ("FMX_Springfield", StockParts, false), ("NAT_Bedford", StockParts, false), ("NAT_RenneB", StockParts, false),
            ("FR_Alpine", "the stock water", true),
            ("SX_DLC01_FreightYard", "the stock water system; it comes with the game's downloadable Freight Yard supercross track", true),
        };
        /// The always-loaded game databases the converter prunes the stock base against, and the game's animation set table.
        static readonly string[] StockTables = { "AudioDependencies", "EngineDependencies", "Animations", "RiderPhysics", "AIData", "SnakeDatabase", "StuntDatabase" };
        const string AnimationSets = "AnimationSets.bxml";

        /// Why the Reflex library cannot give the converter what these stadiums need, or null: a stock level or table that is not
        /// there. (That a stock file is the original's is checked when it is read: UxStock's md5 pins; Import reports the base's.)
        public static string LibraryProblem(IUxLibrary library, IEnumerable<UntamedKnown> tracks)
        {
            if (library == null) return "Your MX vs ATV Reflex game files were not found. Import (or connect) the Reflex game first: GAME LIBRARY.";
            var list = (tracks ?? new UntamedKnown[0]).ToList();
            bool ec = list.Any(k => k.mode == "ec");
            string lead = "Your MX vs ATV Reflex game files (" + library.Describe + ") ";
            try
            {
                foreach (var (level, what, ecOnly) in StockLevels)
                {
                    if (ecOnly && !ec || library.HasLevel(level)) continue;
                    if (level == UxStock.DEFAULT_STOCK_LEVEL)
                        return lead + "have no " + level + " (Database/" + level + ".dx9.database): " + what + ". Import the complete MX vs ATV Reflex game files (GAME LIBRARY) and try again.";
                    return lead + "have no " + level + " (" + what + ")" +
                           (ecOnly ? ", which the EnduroCross arenas (" + string.Join(", ", list.Where(k => k.mode == "ec").Select(k => k.name)) + ") need. The supercross stadiums can be imported without it."
                                   : ", which the Untamed tracks need. Import the complete MX vs ATV Reflex game files (GAME LIBRARY) and try again.");
                }
                foreach (string table in StockTables)
                    if (!library.HasLevel(table)) return lead + "have no " + table + ".dx9.database, which the Untamed tracks need. Import the complete MX vs ATV Reflex game files (GAME LIBRARY) and try again.";
                if (library.ReadRootFile(AnimationSets) == null) return lead + "have no " + AnimationSets + ", which the Untamed tracks need. Import the complete MX vs ATV Reflex game files (GAME LIBRARY) and try again.";
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException || e is UxException || e is AliveImportException)
            {
                return lead + "can't be read: " + e.Message;
            }
            return null;
        }

        /// The same over a library folder (an original install, or the client's game files: ReflexInstall.Dir), opened only for the check.
        public static string LibraryProblem(string libraryFolder, IEnumerable<UntamedKnown> tracks)
        {
            if (string.IsNullOrEmpty(libraryFolder)) return LibraryProblem((IUxLibrary)null, tracks);
            try { using (var library = new UxDirectoryLibrary(libraryFolder)) return LibraryProblem(library, tracks); }
            catch (Exception e) when (e is UxException || e is AliveImportException || e is IOException || e is UnauthorizedAccessException || e is ArgumentException || e is NotSupportedException)
            {
                return "Your MX vs ATV Reflex game files in " + libraryFolder + " can't be read: " + e.Message;
            }
        }

        // ------------------------------------------------------------------------------------------------ the import

        /// Converts and installs the chosen stadiums one after another into tracksFolder/Untamed/<short>/ (see the file header).
        ///   found         the scan the stadiums come from (its volumes stay open; the jumbotron picture of a stadium is made from
        ///                 another stadium of the same disc)
        ///   tracks        the chosen stadiums (each Ok), in the order to import
        ///   tracksFolder  Mods/Tracks (TrackMods.Folder: read on the main thread); created when missing
        ///   library       the player's Reflex library (only read)
        ///   progress      called from this thread and the converter's
        /// Returns every chosen stadium's result. Throws UntamedImportException when nothing can be imported at all (nothing was written)
        /// and OperationCanceledException only when cancelled before the first stadium started; later a cancel ends in the result.
        public static UntamedImportResult Import(UntamedSources found, IEnumerable<UntamedTrackSource> tracks, string tracksFolder, IUxLibrary library,
                                                 UntamedImportOptions options, Action<UntamedProgress> progress, CancellationToken cancel)
        {
            options = options ?? new UntamedImportOptions();
            var log = options.log ?? (s => { });
            var chosen = (tracks ?? new UntamedTrackSource[0]).Where(t => t != null).ToList();
            var clock = System.Diagnostics.Stopwatch.StartNew();
            if (string.IsNullOrEmpty(tracksFolder)) throw new UntamedImportException("The custom tracks folder (Mods/Tracks) was not found.");
            try { tracksFolder = Path.GetFullPath(tracksFolder); }
            catch (Exception e) when (e is ArgumentException || e is NotSupportedException || e is PathTooLongException) { throw new UntamedImportException("The custom tracks folder can't be used: " + e.Message, e); }
            var result = new UntamedImportResult { tracksFolder = tracksFolder };
            foreach (var t in chosen)
                result.tracks.Add(new UntamedTrackResult { known = t.known, level = t.known.level, shortName = t.known.shortName, displayName = t.known.name, state = UntamedImportState.Pending });
            if (chosen.Count == 0) return result;
            foreach (var t in chosen)
                if (!t.Ok) throw new UntamedImportException(t.known.name + " can't be imported: " + t.problem);
            if (chosen.Select(t => t.known.level).Distinct(StringComparer.OrdinalIgnoreCase).Count() != chosen.Count) throw new UntamedImportException("A stadium was chosen twice.");
            cancel.ThrowIfCancellationRequested();

            // ---- what holds for every stadium, before anything is written
            string problem = LibraryProblem(library, chosen.Select(t => t.known));
            if (problem != null) throw new UntamedImportException(problem);
            string stage;
            try
            {
                // (Mods/Tracks/Untamed is a file, or a link: no stadium could be published)
                string inTheWay = UntamedFolderProblem(ChildIgnoreCase(tracksFolder, TracksSubfolder) ?? Path.Combine(tracksFolder, TracksSubfolder));
                if (inTheWay != null) throw new UntamedImportException(inTheWay);
                Directory.CreateDirectory(tracksFolder);
                CleanStaging(tracksFolder, log);
                stage = Path.Combine(tracksFolder, StagingPrefix + Guid.NewGuid().ToString("N").Substring(0, 8));
                CheckPathLengths(tracksFolder, stage);
                CheckFreeSpace(tracksFolder, chosen, options);
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException)
            {
                throw new UntamedImportException("The custom tracks folder " + tracksFolder + " can't be written: " + e.Message, e);
            }
            UxStock stock; UxBuildShared shared;
            try { stock = new UxStock(library); shared = new UxBuildShared(stock); }
            catch (OperationCanceledException) { throw; }
            catch (Exception e) when (e is UxException || e is AliveImportException || e is IOException || e is UnauthorizedAccessException)
            {
                throw new UntamedImportException("Your MX vs ATV Reflex game files (" + library.Describe + ") can't be used for the Untamed tracks: " + e.Message, e);
            }

            FileStream lockFile = null;
            try
            {
                try
                {
                    Directory.CreateDirectory(stage);
                    lockFile = new FileStream(Path.Combine(stage, LockName), FileMode.Create, FileAccess.ReadWrite, FileShare.None);
                }
                catch (Exception e) when (e is IOException || e is UnauthorizedAccessException)
                {
                    throw new UntamedImportException("The custom tracks folder " + tracksFolder + " can't be written: " + e.Message, e);
                }
                var verified = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);   // (disc file -> its sha256, each read once)
                for (int i = 0; i < chosen.Count; i++)
                {
                    var res = result.tracks[i];
                    if (cancel.IsCancellationRequested) { res.state = UntamedImportState.Cancelled; continue; }
                    var one = System.Diagnostics.Stopwatch.StartNew();
                    try
                    {
                        ImportTrack(found, chosen[i], res, i, chosen.Count, tracksFolder, stage, library, stock, shared, options, verified, progress, cancel);
                    }
                    catch (Exception e)
                    {
                        // a cancel surfaces as OperationCanceledException, alone or inside the converter's parallel loops
                        if (res.state == UntamedImportState.Imported) log("[untamed] " + res.level + " is installed; after that: " + e);   // (a progress callback that threw)
                        else if (cancel.IsCancellationRequested) { res.state = UntamedImportState.Cancelled; res.error = null; res.folder = null; }
                        else { res.state = UntamedImportState.Failed; res.error = Message(e); res.folder = null; log("[untamed] " + res.level + " failed: " + e); }
                    }
                    finally
                    {
                        // nothing of a stadium that did not get in stays: its work store and its staged folder
                        DeleteFolder(Path.Combine(stage, WorkName)); DeleteFolder(Path.Combine(stage, OutName));
                    }
                    res.seconds = one.Elapsed.TotalSeconds;
                }
                result.cancelled = result.tracks.Any(t => t.state == UntamedImportState.Cancelled);
            }
            finally
            {
                lockFile?.Dispose();
                RemoveStaging(tracksFolder, stage, log);
            }
            result.seconds = clock.Elapsed.TotalSeconds;
            return result;
        }

        /// The same over a Reflex library folder: an original install, or the client's game files (ReflexInstall.Dir).
        public static UntamedImportResult Import(UntamedSources found, IEnumerable<UntamedTrackSource> tracks, string tracksFolder, string libraryFolder,
                                                 UntamedImportOptions options, Action<UntamedProgress> progress, CancellationToken cancel)
        {
            if (string.IsNullOrEmpty(libraryFolder)) throw new UntamedImportException(LibraryProblem((IUxLibrary)null, null));
            UxDirectoryLibrary library;
            try { library = new UxDirectoryLibrary(libraryFolder); }
            catch (Exception e) when (e is UxException || e is AliveImportException || e is IOException || e is UnauthorizedAccessException || e is ArgumentException || e is NotSupportedException)
            {
                throw new UntamedImportException("Your MX vs ATV Reflex game files in " + libraryFolder + " can't be read: " + e.Message, e);
            }
            using (library) return Import(found, tracks, tracksFolder, library, options, progress, cancel);
        }

        const string LockName = "lock", WorkName = "w", OutName = "o", PreviousName = "previous";

        /// An exception as one sentence for the player (the first cause of a parallel loop's).
        static string Message(Exception e)
        {
            if (e is AggregateException a) e = a.Flatten().InnerExceptions.FirstOrDefault() ?? e;
            string m = (e.Message ?? "").Trim();
            if (m.Length == 0) m = e.GetType().Name;
            if (e is UxException || e is AliveImportException || e is UntamedImportException || e is IOException || e is UnauthorizedAccessException) return m;
            return e.GetType().Name + ": " + m;
        }

        static string Rel(string shortName) => TracksSubfolder + "/" + shortName;

        static void ImportTrack(UntamedSources found, UntamedTrackSource src, UntamedTrackResult res, int index, int count, string tracksFolder, string stage,
                                IUxLibrary library, UxStock stock, UxBuildShared shared, UntamedImportOptions options, Dictionary<string, string> verified,
                                Action<UntamedProgress> progress, CancellationToken cancel)
        {
            var k = src.known;
            var lv = UxCommon.Level(k.level);
            void Report(string what, float f)
            {
                f = Math.Min(1f, Math.Max(0f, f));
                progress?.Invoke(new UntamedProgress { level = k.level, name = k.name, index = index, count = count, stage = what, track = f, overall = (index + f) / count });
            }
            var clock = System.Diagnostics.Stopwatch.StartNew();
            void Took(string step) { res.steps.Add(new KeyValuePair<string, double>(step, clock.Elapsed.TotalSeconds)); clock.Restart(); }
            Report("Checking the disc files", 0f);
            res.olderCopies = HandInstalled(tracksFolder, k);

            // ---- what is installed already
            Destination(tracksFolder, k, out string untamed, out string dest, out string rel);
            string shown = "Mods/Tracks/" + rel;
            string inTheWay = DestinationProblem(untamed, dest, rel, k);
            if (inTheWay != null) throw new UxException(inTheWay);
            bool replacing = false;
            if (Directory.Exists(dest))
            {
                if (!options.replace) { res.state = UntamedImportState.AlreadyImported; res.folder = rel; return; }
                RequireUnchanged(dest, shown, cancel);
                RequireFree(dest, shown);
                replacing = true;
            }

            // ---- the disc files, against the supported disc (the scan checked all but the package)
            var hashes = VerifySource(src, verified, cancel);
            Report("Checking the disc files", 0.02f);
            // the jumbotron still is made from a texture of another stadium of the same disc (UxScreens)
            string stillLevel = k.mode == "ec" ? "Fairview_STADIUM" : "WestStone_STADIUM";
            if (!string.Equals(stillLevel, k.level, StringComparison.OrdinalIgnoreCase))
            {
                var other = found?.tracks.FirstOrDefault(t => SameLevel(t, stillLevel) && ReferenceEquals(t.volume, src.volume) && t.directory == src.directory);
                if (other == null) throw new UxException(k.name + " needs " + stillLevel + " of the same disc (its screen picture is made from it), which was not found; choose the whole disc");
                if (!other.Ok) throw new UxException(k.name + " needs " + stillLevel + " of the same disc (its screen picture is made from it): " + other.problem);
                VerifySource(other, verified, cancel);
            }
            Took("disc check");
            Report("Reading the disc", 0.04f);

            // ---- build into the staging folder
            string work = Path.Combine(stage, WorkName), outDir = Path.Combine(stage, OutName);
            DeleteFolder(work); DeleteFolder(outDir);
            var disc = new UxDisc(src.volume, src.directory, src.root);          // (not disposed: the volume is the scan's)
            var ctx = new UxContext { lv = lv, disc = disc.Level(lv, cancel), library = library, stock = stock, work = new UxWork(work), cancel = cancel, log = options.converterLog ?? (s => { }) };
            var o = UxTrackOptions.ForLevel(lv);
            o.threads = options.threads;
            UxBuildResult built = UxBuildTrack.Build(ctx, disc, o, outDir, (f, text) => Report(StageText(text), 0.04f + 0.90f * f), cancel, shared);
            ctx.disc.ClearCache();
            res.steps.AddRange(built.steps); clock.Restart();

            // ---- the result: what TrackMods lists a track by, and the files as they are on the disk
            Report("Checking the track", 0.95f);
            var problems = CheckTrack(outDir, built);
            if (problems.Count > 0) throw new UxException("the converted track does not pass its checks: " + string.Join("; ", problems));
            bool tested = true;
            foreach (string kind in UxCommon.SlotFileKinds)
            {
                string name = UxCommon.SlotFileName(built.slot, kind), want = TestedSha256(k.shortName, kind);
                string got = built.files.Where(f => f.name == name).Select(f => f.sha256).FirstOrDefault();
                if (want == null || got != want) tested = false;
            }
            if (!tested) res.warnings.Add("the track's files differ from the tested build (another version of a stock Reflex file, or of the converter)");
            res.warnings.AddRange(built.warnings);
            // the builder's list leaves out sha256.txt itself
            var files = built.files.ToList();
            string listFile = Path.Combine(outDir, UxPackage.SHA256);
            files.Add((UxPackage.SHA256, new FileInfo(listFile).Length, UxBin.Sha256File(listFile)));
            var manifest = Manifest(src, lv, built, files, hashes, library, tested, o);
            File.WriteAllText(Path.Combine(outDir, ManifestName), UxJson.Dumps(manifest, 1, false, false) + "\n", new UTF8Encoding(false));
            Took("track check");
            options.staged?.Invoke(k, work, outDir);
            cancel.ThrowIfCancellationRequested();
            clock.Restart();

            // ---- publish: the track imported before set aside, the new folder moved in (the old one back if that fails)
            Report("Installing", 0.97f);
            Publish(stage, outDir, untamed, dest, k.shortName, replacing, shown);
            res.state = UntamedImportState.Imported; res.folder = rel; res.replaced = replacing; res.testedBuild = tested;
            res.files = files; res.bytes = files.Sum(f => f.bytes); res.assets = built.assets;
            res.olderCopies = HandInstalled(tracksFolder, k);
            Took("publish");
            Report("Done", 1f);
        }

        /// The builder's step texts for the progress screen ("converting: terrain" -> "Converting: terrain").
        static string StageText(string text)
        {
            if (string.IsNullOrEmpty(text)) return "Converting";
            if (text == "done") return "Checking the track";
            return char.ToUpperInvariant(text[0]) + text.Substring(1);
        }

        // ------------------------------------------------------------------------------------------------ the sources

        /// The stadium's five disc files against the supported disc: sizes, and the SHA-256 of the database, the scene, the race line
        /// and the package (read in pieces). Returns kind -> sha256 ("level" too: the table has no hash for it). Throws when one differs.
        static JObj VerifySource(UntamedTrackSource s, Dictionary<string, string> verified, CancellationToken cancel)
        {
            var k = s.known; var files = Files(s);
            string[] kinds = { "level", "database", "package", "scene", "tdf07" };
            string[] want = { null, k.databaseSha256, k.packageSha256, k.sceneSha256, k.tdfSha256 };
            var o = new JObj();
            for (int i = 0; i < 5; i++)
            {
                string name = i < 4 ? k.level + ".xenon." + Suffixes[i] : k.TdfName;
                if (files[i] == null) throw new UxException(name + " is missing on the disc");
                if (files[i].size != k.sizes[i]) throw new UxException(name + " differs from the supported disc (size)");
                string key = s.volume.origin + "|" + files[i].path;
                if (!verified.TryGetValue(key, out string sha))
                {
                    sha = Sha256(s.volume, files[i], cancel);
                    if (want[i] != null && sha != want[i]) throw new UxException(name + " differs from the supported disc");
                    verified[key] = sha;
                }
                o.Set(kinds[i], sha);
            }
            return o;
        }

        /// The SHA-256 of a volume's file, read in pieces of 4 MB.
        static string Sha256(Volume v, VolumeFile f, CancellationToken cancel)
        {
            const int Piece = 4 << 20;
            using (var h = SHA256.Create())
            {
                for (long at = 0; at < f.size; at += Piece)
                {
                    cancel.ThrowIfCancellationRequested();
                    int n = (int)Math.Min(Piece, f.size - at);
                    byte[] b = v.ReadRange(f, at, n, cancel);
                    if (b == null || b.Length != n) throw new UxException(f.Name + " can't be read whole");
                    h.TransformBlock(b, 0, n, null, 0);
                }
                h.TransformFinalBlock(new byte[0], 0, 0);
                return UxBin.Hex(h.Hash);
            }
        }

        // ------------------------------------------------------------------------------------------------ the result's checks

        static readonly Regex KindRx = new Regex(@"^Beta_(?:([A-Za-z]+)_)?Track_Slot_?\d*$", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);

        /// key = value lines as the client reads a mod.ini (BikeMods.ParseIni).
        internal static Dictionary<string, string> ParseIni(string text)
        {
            var d = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            foreach (var raw in (text ?? "").Replace("\r", "").Split('\n'))
            {
                string l = raw.Trim().TrimStart('﻿');
                if (l.Length == 0 || l[0] == ';' || l[0] == '#' || l[0] == '[') continue;
                int e = l.IndexOf('='); if (e <= 0) continue;
                string v = l.Substring(e + 1).Trim();
                if (v.Length >= 2 && v[0] == '"' && v[v.Length - 1] == '"') v = v.Substring(1, v.Length - 2);
                d[l.Substring(0, e).Trim().ToLowerInvariant()] = v;
            }
            return d;
        }

        /// What is wrong with a converted track's folder (empty: nothing): the rules the client lists a custom track by (TrackMods.Check
        /// and KindOf: one complete <stem>.dx9.* set, a level name the core takes, a database that lists a package and covers the package
        /// file, a scene with a Base section; UxPackage.Rules re-reads the files), a mod.ini the client reads as a supercross track with
        /// its picture, and every file as sha256.txt and the builder's result say.
        public static List<string> CheckTrack(string folder, UxBuildResult built = null)
        {
            var probs = new List<string>();
            if (!Directory.Exists(folder)) { probs.Add("no folder was written"); return probs; }
            try
            {
                probs.AddRange(UxPackage.Rules(folder));
                string stem = SlotStem;
                foreach (string kind in UxCommon.SlotFileKinds)
                {
                    string p = Path.Combine(folder, UxCommon.SlotFileName(stem, kind));
                    if (!File.Exists(p)) probs.Add(Path.GetFileName(p) + " is missing");
                    else if (new FileInfo(p).Length <= 0) probs.Add(Path.GetFileName(p) + " is empty");
                }
                var kind2 = KindRx.Match(stem);
                if (!kind2.Success || !string.Equals(kind2.Groups[1].Value, "sx", StringComparison.OrdinalIgnoreCase)) probs.Add(stem + " is not a supercross slot name");
                if (built != null && built.slot != stem) probs.Add("the track was built as " + built.slot + ", not " + stem);
                // mod.ini
                string ini = Path.Combine(folder, UxPackage.MOD_INI);
                if (!File.Exists(ini)) probs.Add("mod.ini is missing");
                else
                {
                    var kv = ParseIni(File.ReadAllText(ini, Encoding.UTF8));
                    if (!kv.TryGetValue("name", out string name) || string.IsNullOrWhiteSpace(name)) probs.Add("mod.ini has no name");
                    if (!kv.TryGetValue("kind", out string kind) || kind.Trim().ToLowerInvariant() != "supercross") probs.Add("mod.ini does not say kind = supercross");
                    if (!kv.TryGetValue("thumbnail", out string thumb) || string.IsNullOrWhiteSpace(thumb) || thumb.IndexOfAny(new[] { '/', '\\' }) >= 0 || !File.Exists(Path.Combine(folder, thumb.Trim())))
                        probs.Add("mod.ini's thumbnail is missing");
                }
                // preview.png: a PNG
                string png = Path.Combine(folder, UxPackage.PREVIEW);
                if (!File.Exists(png)) probs.Add("preview.png is missing");
                else
                {
                    var head = new byte[8]; int n;
                    using (var f = File.OpenRead(png)) n = f.Read(head, 0, 8);
                    if (n != 8 || head[0] != 0x89 || head[1] != 'P' || head[2] != 'N' || head[3] != 'G' || head[4] != 0x0D || head[5] != 0x0A || head[6] != 0x1A || head[7] != 0x0A) probs.Add("preview.png is not a PNG picture");
                }
                // sha256.txt and the builder's list: every file is on the disk as it was written
                string list = Path.Combine(folder, UxPackage.SHA256);
                var listed = new Dictionary<string, string>(StringComparer.Ordinal);
                if (!File.Exists(list)) probs.Add("sha256.txt is missing");
                else
                    foreach (string line in File.ReadAllText(list, Encoding.UTF8).Split('\n'))
                    {
                        if (line.Trim().Length == 0) continue;
                        int star = line.IndexOf(" *", StringComparison.Ordinal);
                        if (star != 64) { probs.Add("sha256.txt does not read"); break; }
                        listed[line.Substring(star + 2).Trim()] = line.Substring(0, 64);
                    }
                var onDisk = new Dictionary<string, string>(StringComparer.Ordinal);
                foreach (string f in Directory.GetFiles(folder))
                {
                    string n = Path.GetFileName(f);
                    if (n == UxPackage.SHA256 || n == ManifestName) continue;
                    onDisk[n] = UxBin.Sha256File(f);
                }
                if (Directory.GetDirectories(folder).Length > 0) probs.Add("the folder holds another folder");
                foreach (var kv in onDisk) if (!listed.TryGetValue(kv.Key, out string sha) || sha != kv.Value) probs.Add(kv.Key + " is not as sha256.txt lists it");
                foreach (var kv in listed) if (!onDisk.ContainsKey(kv.Key)) probs.Add(kv.Key + " is listed in sha256.txt and missing");
                if (built != null)
                {
                    foreach (var f in built.files) if (!onDisk.TryGetValue(f.name, out string sha) || sha != f.sha256) probs.Add(f.name + " is not as the converter wrote it");
                    if (built.files.Count != onDisk.Count) probs.Add("the folder holds " + onDisk.Count + " files, the converter wrote " + built.files.Count);
                }
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException || e is UxException || e is AliveImportException) { probs.Add("the track's files do not read (" + e.Message + ")"); }
            return probs;
        }

        // ------------------------------------------------------------------------------------------------ the manifest

        // The manifest stays in the track's folder, which players zip and share: it names the disc image (or the disc folder) and the kind
        // of Reflex library, and never where they are (a path under the user's folder carries the user's name).
        static string LastName(string path)
        {
            try { return Path.GetFileName((path ?? "").TrimEnd('/', '\\')) ?? ""; }
            catch (ArgumentException) { return ""; }
        }
        static string LibraryKind(IUxLibrary library) => library is UxDirectoryLibrary d ? (d.IsExtracted ? "game library" : "original install") : "library";

        static JObj Manifest(UntamedTrackSource src, UxLevelRow lv, UxBuildResult built, List<(string name, long bytes, string sha256)> installed, JObj hashes,
                             IUxLibrary library, bool tested, UxTrackOptions o)
        {
            var k = src.known;
            var files = new List<object>();
            foreach (var f in installed) files.Add(new JObj().Set("name", f.name).Set("bytes", f.bytes).Set("sha256", f.sha256));
            var source = new JObj().Set("game", "MX vs ATV Untamed (Xbox 360)").Set("title_id", TitleId.ToString("X8", CultureInfo.InvariantCulture))
                .Set("from", LastName(src.volume.origin)).Set("form", src.volume is DiscVolume ? "disc image" : "extracted disc folder")
                .Set("level", k.level).Set("race_line", k.TdfName).Set("sha256", hashes);
            return new JObj().Set("version", 1L).Set("importer", ImporterVersion).Set("converter", UxBuildTrack.PORT).Set("converter_reference", UxBuildTrack.REFERENCE_TOOL_MD5)
                .Set("level", k.level).Set("short", k.shortName).Set("name", k.name).Set("menu_name", UxPackage.MenuName(lv)).Set("kind", k.Kind).Set("slot", built.slot)
                .Set("source", source)
                .Set("library", new JObj().Set("base", UxStock.DEFAULT_STOCK_LEVEL).Set("from", LibraryKind(library)))
                .Set("options", o.Record(lv))
                .Set("tested_build", tested)
                .Set("assets", (long)built.assets).Set("package_bytes", built.packageBytes)
                .Set("imported_utc", DateTime.UtcNow.ToString("yyyy-MM-ddTHH:mm:ssZ", CultureInfo.InvariantCulture))
                .Set("files", files);
        }

        /// The manifest of an imported track's folder (null: none, or it does not read).
        public static JObj ReadManifest(string trackFolder)
        {
            try
            {
                string p = Path.Combine(trackFolder, ManifestName);
                if (!File.Exists(p)) return null;
                var m = UxJson.ParseUtf8(File.ReadAllBytes(p)) as JObj;
                return m != null && m.Get("importer") is string s && s.StartsWith("untamed-import/", StringComparison.Ordinal) && m.Get("files") is List<object> ? m : null;
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException || e is UxException || e is FormatException || e is ArgumentException) { return null; }
        }

        /// A file the desktops write into a folder by themselves: .DS_Store and AppleDouble "._x" (macOS), Thumbs.db and desktop.ini
        /// (Windows). Only these go with a replaced track unasked; any other name, dot names included, is the player's.
        static bool IsDesktopFile(string name) => string.Equals(name, ".DS_Store", StringComparison.OrdinalIgnoreCase) || name.StartsWith("._", StringComparison.Ordinal)
                                                  || string.Equals(name, "Thumbs.db", StringComparison.OrdinalIgnoreCase) || string.Equals(name, "desktop.ini", StringComparison.OrdinalIgnoreCase);

        /// A track imported before may be replaced: its manifest reads, every file it lists is there unchanged and the folder holds
        /// nothing else: no folder (or link) at all, no file but the desktops' own (IsDesktopFile). Throws with the reason otherwise:
        /// what the player put there is theirs, and the replaced folder is deleted whole.
        static void RequireUnchanged(string dest, string shown, CancellationToken cancel)
        {
            var m = ReadManifest(dest);
            string hint = "; move or delete that folder to import the track again";
            if (m == null) throw new UxException(shown + " has an import manifest that does not read" + hint);
            var listed = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            foreach (object x in (List<object>)m.Get("files"))
            {
                var f = x as JObj; string name = f?.Get("name") as string, sha = f?.Get("sha256") as string;
                if (name == null || sha == null || name.IndexOfAny(new[] { '/', '\\', ':' }) >= 0 || name == "." || name == "..") throw new UxException(shown + " has an import manifest that does not read" + hint);
                listed[name] = sha;
            }
            string folder = Directory.GetDirectories(dest).Select(Path.GetFileName).OrderBy(n => n, StringComparer.Ordinal).FirstOrDefault();
            if (folder != null) throw new UxException(shown + " holds a folder the importer did not write (" + folder + ")" + hint);
            foreach (string f in Directory.GetFiles(dest))
            {
                cancel.ThrowIfCancellationRequested();
                string n = Path.GetFileName(f);
                if (n == ManifestName || IsDesktopFile(n)) continue;
                if (!listed.TryGetValue(n, out string sha)) throw new UxException(shown + " holds a file the importer did not write (" + n + ")" + hint);
                if (UxBin.Sha256File(f) != sha) throw new UxException(shown + " was changed since it was imported (" + n + ")" + hint);
            }
        }

        static string InUse(string shown) => shown + " is in use (was the track ridden since the game started?): restart the game and import it again";

        /// Windows: a folder with an open file can't be moved, and the game keeps the package of a track ridden since it started open.
        /// The track to replace is asked for that now, before the stadium is converted (Publish would find it only when the new track
        /// is moved in). A file held for a moment (a virus scanner) is waited for, as MoveFolder does.
        static void RequireFree(string dest, string shown)
        {
            if (Path.DirectorySeparatorChar != '\\') return;
            foreach (string f in Directory.GetFiles(dest))
                for (int attempt = 0; ; attempt++)
                {
                    try { using (new FileStream(f, FileMode.Open, FileAccess.Read, FileShare.None)) { } break; }
                    catch (FileNotFoundException) { break; }
                    catch (Exception e) when (e is IOException || e is UnauthorizedAccessException)
                    {
                        if (attempt < 5) { Thread.Sleep(120 * (attempt + 1)); continue; }
                        throw new UxException(InUse(shown) + " (" + e.Message + ")", e);
                    }
                }
        }

        // ------------------------------------------------------------------------------------------------ folders

        // (ChildIgnoreCase, IsLink, Destination and DestinationProblem: UntamedImporter.cs, the scan asks them too)

        /// Moves a folder (the same volume). A file another program holds for a moment (a virus scanner on files just written) is waited
        /// for; inUse: the message when it stays held.
        static void MoveFolder(string from, string to, string inUse)
        {
            for (int attempt = 0; ; attempt++)
            {
                try { Directory.Move(from, to); return; }
                catch (Exception e) when (e is IOException || e is UnauthorizedAccessException)
                {
                    if (attempt < 5) { Thread.Sleep(120 * (attempt + 1)); continue; }
                    if (inUse != null) throw new UxException(inUse + " (" + e.Message + ")", e);
                    throw;
                }
            }
        }

        /// Removes a folder and what it holds; false when something of it stays (a file in use). Never through a link (DeleteTree).
        static bool DeleteFolder(string dir)
        {
            for (int attempt = 0; attempt < 4; attempt++)
            {
                try
                {
                    if (!Directory.Exists(dir)) return true;
                    DeleteTree(dir);
                    if (!Directory.Exists(dir)) return true;
                }
                catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { }
                Thread.Sleep(100 * (attempt + 1));
            }
            return !Directory.Exists(dir);
        }

        /// A folder or file the file system marks as a reparse point: a symbolic link, a junction, a mounted volume.
        static bool IsReparse(string path) => (File.GetAttributes(path) & FileAttributes.ReparsePoint) != 0;

        /// Removes a folder file by file, walking it by hand: a link in it (a junction, a symbolic link to a folder or a file) is removed
        /// itself and never entered, so nothing outside the folder is deleted or changed (the read-only attribute is cleared only on
        /// the folder's own files). Throws what the file system throws (a file in use).
        static void DeleteTree(string dir)
        {
            if (IsReparse(dir))
            {
                try { Directory.Delete(dir); }                                      // (not recursive: the link alone)
                catch (IOException) when (Path.DirectorySeparatorChar != '\\') { File.Delete(dir); }   // (a link is a file to unlink there)
                return;
            }
            foreach (string d in Directory.GetDirectories(dir)) DeleteTree(d);
            foreach (string f in Directory.GetFiles(dir))
            {
                var a = File.GetAttributes(f);
                // (a link to a file: deleted as the link; its attributes may be the target's)
                if ((a & FileAttributes.ReadOnly) != 0 && (a & FileAttributes.ReparsePoint) == 0) File.SetAttributes(f, a & ~FileAttributes.ReadOnly);
                File.Delete(f);
            }
            var own = File.GetAttributes(dir);
            if ((own & FileAttributes.ReadOnly) != 0) File.SetAttributes(dir, own & ~FileAttributes.ReadOnly);
            Directory.Delete(dir);
        }

        static void Publish(string stage, string outDir, string untamed, string dest, string shortName, bool replacing, string shown)
        {
            Directory.CreateDirectory(untamed);
            string aside = null;
            if (replacing)
            {
                aside = Path.Combine(stage, PreviousName, shortName);
                Directory.CreateDirectory(Path.Combine(stage, PreviousName));
                DeleteFolder(aside);
                MoveFolder(dest, aside, InUse(shown));
            }
            try { MoveFolder(outDir, dest, null); }
            catch
            {
                if (aside != null) { try { MoveFolder(aside, dest, null); } catch (Exception e) when (e is IOException || e is UnauthorizedAccessException || e is UxException) { } }   // (RemoveStaging tries again)
                throw;
            }
            if (aside != null) DeleteFolder(aside);
        }

        /// A staging folder's replaced tracks go back where they were (when nothing is there now); true when none is left in it.
        static bool Recover(string tracksFolder, string stage, Action<string> log)
        {
            string previous = Path.Combine(stage, PreviousName);
            if (!Directory.Exists(previous)) return true;
            bool clean = true;
            foreach (string old in Directory.GetDirectories(previous))
            {
                string untamed = ChildIgnoreCase(tracksFolder, TracksSubfolder) ?? Path.Combine(tracksFolder, TracksSubfolder);
                string dest = ChildIgnoreCase(untamed, Path.GetFileName(old)) ?? Path.Combine(untamed, Path.GetFileName(old));
                if (Directory.Exists(dest) || File.Exists(dest)) continue;            // (the new track got in: the old one goes with the staging folder)
                try
                {
                    Directory.CreateDirectory(untamed);
                    MoveFolder(old, dest, null);
                    log("[untamed] put back " + Rel(Path.GetFileName(old)));
                }
                catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { clean = false; log("[untamed] " + old + " could not be put back: " + e.Message); }
            }
            return clean;
        }

        /// The import's own staging folder goes, however the import ended (a replaced track that could not be put back keeps it).
        static void RemoveStaging(string tracksFolder, string stage, Action<string> log)
        {
            try
            {
                if (!Directory.Exists(stage)) return;
                if (!Recover(tracksFolder, stage, log)) return;
                if (!DeleteFolder(stage)) log("[untamed] the staging folder " + stage + " could not be removed; the next import removes it");
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { log("[untamed] the staging folder " + stage + ": " + e.Message); }
        }

        static readonly Regex StagingRx = new Regex("^" + Regex.Escape(StagingPrefix) + "[0-9a-f]{8}$", RegexOptions.CultureInvariant);

        /// The staging folders of imports that did not end (a crash, the power): removed, a replaced track in them put back first. One
        /// whose lock is held belongs to an import that is running (another client on the same folder) and stays.
        public static int CleanStaging(string tracksFolder, Action<string> log = null)
        {
            log = log ?? (s => { });
            int removed = 0;
            if (string.IsNullOrEmpty(tracksFolder) || !Directory.Exists(tracksFolder)) return 0;
            foreach (string dir in Directory.GetDirectories(tracksFolder))
            {
                if (!StagingRx.IsMatch(Path.GetFileName(dir)) || IsLink(dir)) continue;
                string lockPath = Path.Combine(dir, LockName);
                bool free;
                try { using (new FileStream(lockPath, FileMode.Open, FileAccess.ReadWrite, FileShare.None)) { } free = true; }
                catch (FileNotFoundException) { free = true; }
                catch (DirectoryNotFoundException) { continue; }
                catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { free = false; }
                if (!free) { log("[untamed] " + dir + " belongs to an import that is running: left"); continue; }
                try
                {
                    if (!Recover(tracksFolder, dir, log)) continue;
                    if (DeleteFolder(dir)) { removed++; log("[untamed] removed the staging folder an earlier import left: " + dir); }
                }
                catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { log("[untamed] " + dir + ": " + e.Message); }
            }
            return removed;
        }

        // ------------------------------------------------------------------------------------------------ space and path lengths

        /// What an imported stadium takes on the disk, estimated from its disc package (the 12 measured: 0.98 to 1.24 times its size).
        public static long EstimatedBytes(UntamedKnown k) => (long)(k.sizes[2] * 1.25) + (4L << 20);

        /// The bytes an import of these stadiums needs free in the tracks folder's volume: the tracks, one work store and one staged track.
        public static long NeededBytes(IEnumerable<UntamedKnown> tracks) => StagingBytes + (tracks ?? new UntamedKnown[0]).Sum(EstimatedBytes);

        static long? DefaultFreeBytes(string folder)
        {
            try
            {
                string root = Path.GetPathRoot(Path.GetFullPath(folder));
                if (string.IsNullOrEmpty(root)) return null;
                var d = new DriveInfo(root);
                return d.IsReady ? d.AvailableFreeSpace : (long?)null;
            }
            catch (Exception) { return null; }
        }

        static string Mb(long bytes) => ((bytes + (1 << 20) - 1) >> 20).ToString(CultureInfo.InvariantCulture) + " MB";

        static void CheckFreeSpace(string tracksFolder, List<UntamedTrackSource> chosen, UntamedImportOptions options)
        {
            long? free = (options.freeBytes ?? DefaultFreeBytes)(tracksFolder);
            if (free == null) return;
            long need = NeededBytes(chosen.Select(t => t.known));
            if (free.Value < need)
                throw new UntamedImportException("Not enough free disk space for " + chosen.Count + (chosen.Count == 1 ? " track" : " tracks") + " in " + tracksFolder + ": about " + Mb(need) +
                                                 " are needed (" + Mb(StagingBytes) + " of it only while importing), " + Mb(free.Value) + " are free. Free some space or choose fewer tracks.");
        }

        /// Windows: every path the import writes (the work store below the staging folder, the published files) fits in 260 characters.
        static void CheckPathLengths(string tracksFolder, string stage)
        {
            if (Path.DirectorySeparatorChar != '\\') return;
            int longestShort = KnownTracks.Max(k => k.shortName.Length);
            int staged = Path.Combine(stage, WorkName).Length + 1 + WorkPathBudget;
            int published = Path.Combine(tracksFolder, TracksSubfolder).Length + 1 + longestShort + 1 + (SlotStem + ".dx9.database.tmp").Length;
            int aside = Path.Combine(stage, PreviousName).Length + 1 + longestShort + 1 + (SlotStem + ".dx9.database").Length;
            if (Math.Max(staged, Math.Max(published, aside)) >= 260)
                throw new UntamedImportException("The client folder is too deeply nested for the tracks' files (" + tracksFolder + "); move the client to a shorter path, such as " + ReflexPaths.ShortFolderExample(tracksFolder));
        }
    }
}
