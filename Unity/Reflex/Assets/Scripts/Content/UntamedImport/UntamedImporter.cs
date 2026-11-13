// MX vs ATV Untamed (Xbox 360): finds the stadium tracks of the player's own Untamed disc in what they point it at (the disc image, or the
// folder the disc was extracted into: the one holding Database and AI) and checks each against the supported disc: the four
// Database/<level>.xenon.{level,database,package,scene} files and the AI/<level>-<mode>.tdf07 race line by size, then the SHA-256 of the
// files the converter reads whole (database, scene, tdf07: about 18 MB for the 12 tracks; the .package hash is kept for the import).
// The tracks are the 10 supercross, 2 EnduroCross and 2 Minimoto stadiums and the 4 freestyle arenas (free ride) the Python Untamed
// converter (the reference) builds into custom tracks
// (Mods/Tracks/Untamed/<short>-v<n> when installed by hand, <short> when the client imported them; core/plan/UNTAMED_TRACKS.md); a track
// converted before is found there by its folder name. The disc readers are the Alive importer's (AliveContainers.cs). Reference:
// tools/formats/untamed_disc.py (prints the table rows of a disc; a level may have one row per disc revision).
// Pure C# (System only): no Python, no native code. The scan writes nothing; the import is UntamedImporter.Import.cs (the other part of
// this class: it needs the converter, the scan does not).
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text.RegularExpressions;
using System.Threading;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// A stadium of the supported disc: its files' sizes and the SHA-256 of the files the converter reads.
    public sealed class UntamedKnown
    {
        public readonly string level, shortName, mode, name;
        /// .xenon.level, .database, .package, .scene, then the .tdf07
        public readonly long[] sizes;
        public readonly string databaseSha256, sceneSha256, tdfSha256, packageSha256;
        public UntamedKnown(string level, string shortName, string mode, string name, long l, long d, long p, long s, long tdf, string databaseSha256, string sceneSha256, string tdfSha256, string packageSha256)
        {
            this.level = level; this.shortName = shortName; this.mode = mode; this.name = name; sizes = new[] { l, d, p, s, tdf };
            this.databaseSha256 = databaseSha256; this.sceneSha256 = sceneSha256; this.tdfSha256 = tdfSha256; this.packageSha256 = packageSha256;
        }
        /// Supercross, EnduroCross or Minimoto (both race as supercross: Reflex has neither race), or Freestyle (a free-ride track: Reflex
        /// has no freestyle event for custom tracks).
        public string Kind => mode == "ec" ? "EnduroCross" : mode == "mm" ? "Minimoto" : mode == "fmx" ? "Freestyle" : "Supercross";
        /// A freestyle arena: installed as a free-ride track (Beta_Fmx_Track_Slot_1), listed under FREE RIDE only.
        public bool FreeRideOnly => mode == "fmx";
        public string TdfName => level + "-" + mode + ".tdf07";
    }

    /// One stadium found in the player's sources.
    public sealed class UntamedTrackSource
    {
        public UntamedKnown known;
        public Volume volume;
        /// The volume's folder of the level files ("Database", "<folder>/Database") and of the AI folder ("" = the volume's root; null = none).
        public string directory, root;
        /// Why it can't be used (null: it is the supported disc's).
        public string problem;
        /// The custom track converted from it before, relative to Mods/Tracks ("Untamed/norton-v3"; null: none).
        public string installed;
        /// The track this importer made of it before ("Untamed/norton": the folder with its import manifest; null: none). Import replaces
        /// it only when asked (a re-import).
        public string imported;
        /// The track imported before was made by an older converter and its files are not those this one writes (the client's screen
        /// sets it: UntamedImporter.Outdated): importing it again updates it.
        public bool outdated;
        /// The copies installed by hand ("Untamed/norton-v3" ...): the importer never changes or removes them.
        public List<string> handInstalled = new List<string>();
        /// Why the import can't write this stadium's folder Mods/Tracks/Untamed/<short> (null: it can): something of the player's own is
        /// there (a folder without the import manifest, a file, a link). UntamedImporter.DestinationProblem: the import's own test.
        public string inTheWay;
        public long bytes;
        public bool Ok => problem == null;
        public string Label => known.name;
    }

    /// The tracks found in the player's sources. Keeps the sources open until disposed.
    public sealed class UntamedSources : IDisposable
    {
        public readonly List<UntamedTrackSource> tracks = new List<UntamedTrackSource>();
        public readonly List<Volume> volumes = new List<Volume>();
        /// One line per source read ("mxvsatvuntamed.iso  ·  disc image, game partition  ·  title 545107DE").
        public readonly List<string> sources = new List<string>();
        public readonly List<string> warnings = new List<string>();
        /// The disc's other stadium levels (Dyson_STADIUM ...): not converted yet.
        public readonly List<string> unsupported = new List<string>();
        public IEnumerable<UntamedTrackSource> Ready => tracks.Where(t => t.Ok);
        public void Dispose() { foreach (var v in volumes) v.Dispose(); volumes.Clear(); }
    }

    public static partial class UntamedImporter
    {
        public const uint TitleId = 0x545107DE;
        /// The Mods/Tracks subfolder the converted tracks are installed in (one folder per track, named by its short name).
        public const string TracksSubfolder = "Untamed";
        /// The import manifest in a track folder this importer made (UntamedImporter.Import): what tells its folders from hand-installed ones.
        public const string ManifestName = "untamed-import.json";

        static readonly string[] Suffixes = { "level", "database", "package", "scene" };

        // The supported disc (title 545107DE, media id 76BBD0C5, disc 1 of 1): tools/formats/untamed_disc.py --csharp. Names: MX07Tables' Free
        // Ride names of the locations. Another revision of the disc: its rows added after these (Scan takes the first row a level's files match).
        public static readonly UntamedKnown[] KnownTracks =
        {
            new UntamedKnown("Norton_STADIUM", "norton", "sx", "Norton Coliseum", 307, 66021, 48893290, 912028, 404948,
                "be835daf32926f2a2aecbf23cb3fb85f7ba51c4841607baabcfee86778f0b834", "7ccdd1c0ff03e76b4fcd81e20cd083ca38e42fd4c038df2b95b7c17c23db4cde", "a4af44e18a42b4f6b6fdbae4997c34385d42d42826d373fb7f168f4395da8f1b", "d5faccbb356a79f0bd60ed34699b11dc3d261aa5bce7c8af74c3885bbcefa5c2"),
            new UntamedKnown("Capital_City_STADIUM", "capitalcity", "sx", "Capital City Stadium", 313, 68261, 49316350, 1079600, 538236,
                "495687e297cec0af4c8d2ac94492a74363bed32a1eb6f6ad01d6538a82d3069d", "db3dab2f1b0d36fc845a8c29cd6743831b2c415bf0961a5b43ed32d44aeba824", "2f5253c344cc94d0beda198b2a45b554d657c3834c47493ebc9ddded7c622ca5", "bc32d6244a9647c957c7b10649cf1c2450ee5f2b39eba3dc39e71fdb4e3d1e51"),
            new UntamedKnown("Columbus_STADIUM", "columbus", "sx", "Columbus Stadium", 313, 63691, 51744097, 1204144, 591904,
                "774318fe460b3f2daa52e971ffdf72438081a201e4c443085b908eb62fbb3ca2", "02dbd3ca6af6ffa9075745921184822021fd8fdf2b6e5e1882e91150a0279b95", "9dc81b9763d8b29f134ea89f6d49b8118a5ed1336d9849d3b0a0356996af1c95", "a629400ff7924470634fd4d1d3f12212289893954ea7c40f609791fc1906e7e8"),
            new UntamedKnown("Denton_STADIUM", "denton", "sx", "Denton Stadium", 305, 56435, 48152892, 922050, 549568,
                "25c9e5525c04efe3977853fe81466a07b426b2b0e5fe41a324014b0354240988", "e28fb7445bec0e289eb61f4f7f0d9b0226c4395e0bd126d931fdfc033514a9a2", "8d106c2b2a7688d78158b292b60d88743512586d443f0844177e56d96938c609", "3904633977b382e81887aa01dc610247e66af47c9f4066d7acdf4f86139fba14"),
            new UntamedKnown("HarrisPark_STADIUM", "harrispark", "sx", "Harris Park Stadium", 310, 68871, 53568303, 988303, 487500,
                "9ae65d2a64e4791c8db1cec79042164e08c1ec66d6bbbcc40f079776d1781c3f", "0390dcf44c47c13607c9735cfc77399b11d7f0761db93b73dd42015832dc493d", "a9019e59aede3e10807af8cbc4f6a04b4c3eb0ea6de239611575a18fa7545bed", "16e3576793c59611647ae9482596c4dc4a853c5a46dc98083a2f7fb243059e5a"),
            new UntamedKnown("Foothills_STADIUM", "foothills", "sx", "Foothills Stadium", 308, 66609, 50497442, 1175611, 566816,
                "e7f399bb4e2d53b050df701b30c59ef7650fed94b77319eae10f7c577099470c", "521350560d82eb525cf2673313b3564f900d6612bd98474f64c2c10bcf36bc40", "3ec787db268072fbb635327953917c89c63fcb1bdec74cfc13079e7f452d2e5b", "81532dc78e5efaf234bf04b757ffb2778ef4b298c3f26edeb0f34b2c1baa72bf"),
            new UntamedKnown("WestStone_STADIUM", "weststone", "sx", "West Stone Gardens Stadium", 312, 60704, 40851598, 1196477, 665032,
                "23c383e5d293097503589ce57eeb9a9d18adf2eab83c3a91c90aca6abfafe583", "e780c56ca86a13dd199a98627b63e14b088a4a186d800f7ee839d1ca0034eced", "ce31b7e0216c97b6d45ed938b079832696be5614ab4faa3f7be7193be3da6e43", "b1b86c6067e8b31c626f791437069cc22b7ea026e6cb21237be6933dc47788e7"),
            new UntamedKnown("Jefferson_STADIUM", "jefferson", "sx", "Jefferson Stadium", 313, 59595, 50990309, 975929, 506404,
                "5d63a00c08e04bbca4acdc0105ae96b801bf9fa9410987ff4ec7ec48b394fb95", "b6f8291dc9fe2b590c7423af4382221c06b9b7ce07dd2814a078e142a03e969f", "8181b4f85eee9ab065dbee1f502214cb169e472b276c9896341ece393107f52b", "eedecf7359503c3c5029abe244e37ae3d60e14e0343bba13afa82f5eddacfebf"),
            new UntamedKnown("Wilford_STADIUM", "wilford", "sx", "Wilford Stadium", 308, 56939, 55426903, 714495, 369320,
                "99319ac7f4cb79c03c91dc9597c08d8fc8a4f7a5656720fff3af45954353c530", "52a999acd7faf5befc08d07c48e33efc02e78e7db220c99c41a4ee5f81cde4c4", "94157e140d0b7b0821fbef4a225a8c78c78829f967486a4b7dea67327f561fcd", "dd8270f7ef91e92522f6a6c90d182a6826f03d041ce7db03315d0ba2a445b1b2"),
            new UntamedKnown("BridgePort_STADIUM", "bridgeport", "sx", "Bridgeport Stadium", 315, 65983, 60636019, 1016797, 564432,
                "9bc371a90de6f03d0b50f4e4536864e69071096fa6774c46d87b6adc0bec97e6", "d46b34ead663ee16d8bf02f4f0dbe59f545149088b200625de2b4baa67b55abf", "5f7b93202620831c951ca37c780ac861a449513465e541f5d30b0d10c646853f", "a2c13eb01eb0cedf3f6fb0b426995c5c25fc8e1ab8245422da8abd364cf3d9f1"),
            new UntamedKnown("Fairview_STADIUM", "fairview", "ec", "Fairview Arena", 268, 45876, 35547290, 533481, 356848,
                "74d304227915978a9efd4a0b32560aa5b8d44845c2da97e85aef53c976062478", "3f5459b1ae8b03d003f1fff437f10065d1ab9ae5433060586a26b60da3ec1f9b", "5def8c031d4f5eeb407fd5cd24689d1705d7cffe18264740826d070fa68c3b46", "77b9201a470ef6bb145b8f56898d4d3780e9f5a5f5d4f9250840b12b92f5e5f6"),
            new UntamedKnown("Lincoln_STADIUM", "lincoln", "ec", "Lincoln Memorial Arena", 268, 47866, 35336690, 508875, 339004,
                "0e9f998c110a8bc2a1db44385df2c412710f50c7f89864c6922eeddb6288c79d", "2b3c449e948cf2c36a110df7fbc1c4b1b0d05c9fd89dac3f868816caa5673545", "e01d03edcf92511a329a436878ed5e980a46e7d28184668b619935a2fbb575bf", "583cf0b53ec09f6d1988d0488c2153ff7c1310eac4828a491327f9f8a2d62283"),
            new UntamedKnown("Dyson_STADIUM", "dyson", "mm", "Specter Arena", 285, 47650, 33413582, 556279, 178164,
                "794d9fec855ea7c35483471de5670d9fc80c4235c5ff250424fa2e95984dc91b", "0f455aa6481735adf39269e58868ca32b94cf2462a12d7b82b388ad48901e2a9", "375ca5ac230a3ad7f82eddbf36547d5f0689eee5aecfa16526692b7483c6f221", "17999ef85c3aa6d4369270c786287078ef8389aa765387ac26c8d05ca786e9a0"),
            new UntamedKnown("Edgewater_STADIUM", "edgewater", "mm", "Edgewater Arena", 286, 47974, 35343697, 647088, 329600,
                "ff74f53064d03ff55660b214d7bb8c68c57512d34e94116290f2567714974b1e", "34f18ee7df394e8bb335d4330bbbe5c4143e63a9e94376100871250d23149800", "2b15ff707881701c97737978c0bac9e1d03df9fd4034a4823e8b27d23f3799b4", "8a3a767025e231c02a73238c1c84b33eda3dc3a6eb2938eb083848c1c1880a83"),
            // the four freestyle arenas (free ride; the tdf07 holds Untamed's AI freestyle lines, which the terrain stage reads)
            new UntamedKnown("RBWCenter_STADIUM", "rbwcenter", "fmx", "RBW Center Arena", 288, 42399, 34528472, 302590, 159300,
                "47d7e0c871ba5f21e0e396cf55b555cf7a95fe93d752d7e42835e1755a88c588", "c91a5fe350a5e2a66748dea7b87d239b4e46386bb14d2d4ca8a9878b9edfd30f", "28c0c06833e2a154e57b8ad89736d019adbddbcfd0eac66d3a2504f5bd905ca1", "bcb0e7335664c44dfb067dd066ad30176f8f8c950c84042af7a6cc16d1b94af7"),
            new UntamedKnown("Southbend_STADIUM", "southbend", "fmx", "Southbend Arena", 285, 41834, 30615376, 301142, 199092,
                "3ae2dc1ade4243a46a6cd2e565c59e6835925b9dd6a6e8554935f41b9c7d662a", "214a9417baf35ee9ab605388dd59d96cedef890831fb7000d9ccdda7fe38f821", "d49f9df4b74e695704c9502c313c6989e82a492ded6516a9afc6346d457c73fa", "0f3801efaf6835c150ebde50efa4a6ea4805a8aa30776a0729a683e48456af80"),
            new UntamedKnown("THQSpectrum_STADIUM", "thqspectrum", "fmx", "THQ Spectrum Arena", 289, 41493, 29221779, 289214, 236340,
                "62327858e79a2e235f7f0c58cd89e59ff3caa8faa0f652ad01597ce2b9f53148", "fabf1e67732c45db9de54c47a5323eb8de0048ab6e1ad734bc1b1007aa299492", "65cc529feeb4a25cec0d7eff8a56d824ba23c6bb0bb8c4530fa33f41e8321370", "efe651bcd01938b99ab96a9f3cd0f276fdea7afbd3f69271ab792ac421aec29a"),
            new UntamedKnown("WestLake_STADIUM", "westlake", "fmx", "West Lake Arena", 288, 41568, 32559128, 252476, 138124,
                "d8b03eaab53ca811913110ee56160fa9696f0b29c82c481afbff18999a8f6030", "fcdf0ab6c901fc6275e0893efde264b7dd024dfb96092a1e6ea40d68965e789a", "5c969b190649c51377d06c9613b309e1f80eb252d4154f91887fb8315f0ec9ec", "d91d02f90ec388e7cd251c3f3e5382455e17f41800d263d889da726e695f2183"),
        };

        /// The number of stadiums the table supports (each level once, whatever its disc revisions).
        public static int StadiumCount => KnownTracks.Select(k => k.level).Distinct(StringComparer.OrdinalIgnoreCase).Count();

        static bool IsKnown(string level) => KnownTracks.Any(k => string.Equals(k.level, level, StringComparison.OrdinalIgnoreCase));
        static bool SameLevel(UntamedTrackSource t, string level) => string.Equals(t.known.level, level, StringComparison.OrdinalIgnoreCase);
        /// A level's place in the table (the order of its first row).
        static int Order(UntamedKnown k) => Array.FindIndex(KnownTracks, x => string.Equals(x.level, k.level, StringComparison.OrdinalIgnoreCase));

        /// The names a disc folder is read for: the level files, the race lines and default.xex (the title id).
        static bool DiscFile(string name) => name.IndexOf(".xenon.", StringComparison.OrdinalIgnoreCase) >= 0
                                             || name.EndsWith(".tdf07", StringComparison.OrdinalIgnoreCase) || string.Equals(name, "default.xex", StringComparison.OrdinalIgnoreCase);

        /// Opens the sources (each a disc image or a folder) and checks every stadium found. tracksFolder: Mods/Tracks (read on the main thread:
        /// TrackMods.Folder), for the tracks converted before; null skips that. progress: (what is being read, fraction done).
        public static UntamedSources Scan(IEnumerable<string> paths, string tracksFolder, Action<string, float> progress, CancellationToken cancel)
        {
            var r = new UntamedSources();
            // a scan that does not end (cancelled, an error) closes what it opened: nobody else gets its volumes to dispose
            try { Scan(r, paths, tracksFolder, progress, cancel); return r; }
            catch { r.Dispose(); throw; }
        }

        static void Scan(UntamedSources r, IEnumerable<string> paths, string tracksFolder, Action<string, float> progress, CancellationToken cancel)
        {
            foreach (var path in paths)
            {
                cancel.ThrowIfCancellationRequested();
                try { Open(path, r, progress); }
                catch (OperationCanceledException) { throw; }
                catch (Exception ex) { r.warnings.Add(ReflexPaths.LabelOf(path) + ": " + ex.Message); }
            }
            // every level file set: <dir>/<level>.xenon.level, the race line in <dir>/../AI (the disc's layout)
            var found = new List<(Volume v, string dir, string level)>();
            foreach (var v in r.volumes)
                foreach (var f in v.files.Where(f => f.Name.EndsWith(".xenon.level", StringComparison.OrdinalIgnoreCase)))
                    found.Add((v, f.Directory, f.Name.Substring(0, f.Name.Length - ".xenon.level".Length)));
            var titles = new Dictionary<(Volume, string), uint?>();
            int total = found.Select(x => x.level).Where(IsKnown).Distinct(StringComparer.OrdinalIgnoreCase).Count();
            foreach (var (v, dir, level) in found)
            {
                cancel.ThrowIfCancellationRequested();
                // a level's rows: one per supported disc revision
                var rows = KnownTracks.Where(k => string.Equals(k.level, level, StringComparison.OrdinalIgnoreCase)).ToList();
                if (rows.Count == 0)
                {
                    if (level.EndsWith("_STADIUM", StringComparison.OrdinalIgnoreCase) && !r.unsupported.Contains(level, StringComparer.OrdinalIgnoreCase)) r.unsupported.Add(level);
                    continue;
                }
                if (r.tracks.Any(t => SameLevel(t, level) && t.Ok)) continue;
                string root = dir.Length == 0 ? null : dir.LastIndexOf('/') < 0 ? "" : dir.Substring(0, dir.LastIndexOf('/'));
                // another game's disc (Alive also has a Database folder of .xenon files): its default.xex says so
                if (root != null)
                {
                    if (!titles.TryGetValue((v, root), out uint? title))
                    {
                        try { title = XexTitle(v, root, cancel); }
                        catch (AliveImportException) { title = null; }
                        catch (Exception ex) when (ex is IOException || ex is UnauthorizedAccessException) { title = null; r.warnings.Add(Where(v, root) + "/default.xex can't be read: " + ex.Message); }
                        titles[(v, root)] = title;
                        if (title != null && title != TitleId) r.warnings.Add(Where(v, root) + ": not MX vs ATV Untamed (title " + title.Value.ToString("X8") + ")");
                    }
                    if (title != null && title != TitleId) continue;
                }
                progress?.Invoke("Checking " + rows[0].name, total == 0 ? 0 : (float)r.tracks.Count / total);
                // the first row the files match, else the first row's difference
                UntamedTrackSource src = null;
                foreach (var known in rows)
                {
                    var s = new UntamedTrackSource { known = known, volume = v, directory = dir, root = root };
                    s.problem = Check(s, cancel);
                    if (src == null || s.problem == null) src = s;
                    if (s.problem == null) break;
                }
                src.bytes = Files(src).Where(x => x != null).Sum(x => x.size);
                r.tracks.RemoveAll(t => SameLevel(t, level));
                r.tracks.Add(src);
            }
            foreach (var t in r.tracks)
            {
                t.installed = InstalledFolder(tracksFolder, t.known);
                t.imported = ImportedFolder(tracksFolder, t.known);
                t.handInstalled = HandInstalled(tracksFolder, t.known);
                t.inTheWay = DestinationProblem(tracksFolder, t.known);
            }
            r.tracks.Sort((a, b) => Order(a.known).CompareTo(Order(b.known)));
            r.unsupported.Sort(StringComparer.OrdinalIgnoreCase);
            progress?.Invoke("Done", 1);
        }

        static void Open(string path, UntamedSources r, Action<string, float> progress)
        {
            path = Path.GetFullPath(path);
            if (r.volumes.Any(v => ReflexPaths.SamePath(v.origin, path))) return;
            if (File.Exists(path))
            {
                progress?.Invoke("Reading " + Path.GetFileName(path), 0);
                var disc = new DiscVolume(path) { displayName = Path.GetFileName(path) };
                uint? title = null;
                try { title = XexTitle(disc, "", CancellationToken.None); }
                catch (AliveImportException) { }
                catch (Exception) { disc.Dispose(); throw; }
                if (title != null && title != TitleId)
                {
                    disc.Dispose();
                    r.warnings.Add(disc.displayName + ": not MX vs ATV Untamed (title " + title.Value.ToString("X8") + ")");
                    return;
                }
                r.volumes.Add(disc);
                r.sources.Add(disc.displayName + "  ·  disc image, " + disc.layout + (title != null ? "  ·  title " + title.Value.ToString("X8") : "  ·  no default.xex"));
                return;
            }
            Bin.Require(Directory.Exists(path), "Not found: " + path);
            progress?.Invoke("Reading " + ReflexPaths.LabelOf(path), 0);
            var folder = new FolderVolume(path, 3, DiscFile) { displayName = ReflexPaths.LabelOf(path) };
            if (!folder.files.Any(f => f.Name.EndsWith(".xenon.level", StringComparison.OrdinalIgnoreCase)))
            {
                folder.Dispose();
                r.warnings.Add(ReflexPaths.LabelOf(path) + ": no MX vs ATV Untamed levels found (choose the disc image, or the folder that holds Database and AI)");
                return;
            }
            r.volumes.Add(folder);
            r.sources.Add(folder.displayName + "  ·  extracted disc folder");
        }

        static string Where(Volume v, string root) => root.Length == 0 ? v.displayName : v.displayName + "/" + root;

        /// The title id of <root>/default.xex (the XEX2 execution info, optional header 0x00040006); null when the file is not there.
        public static uint? XexTitle(Volume v, string root, CancellationToken cancel)
        {
            var f = v.Find(root.Length == 0 ? "default.xex" : root + "/default.xex");
            if (f == null) return null;
            int n = (int)Math.Min(f.size, 0x10000);
            var x = v.ReadRange(f, 0, n, cancel);
            Bin.Require(n >= 0x18 && Bin.StartsWith(x, "XEX2"), "default.xex is not an Xbox 360 executable");
            uint count = Bin.U32BE(x, 0x14);
            for (int i = 0; i < Math.Min(count, 64u) && 0x18 + i * 8 + 8 <= n; i++)
            {
                uint key = Bin.U32BE(x, 0x18 + i * 8), value = Bin.U32BE(x, 0x1C + i * 8);
                if (key == 0x00040006 && (long)value + 16 <= n) return Bin.U32BE(x, (int)value + 12);
            }
            return null;
        }

        static string FilePath(UntamedTrackSource s, string suffix) => (s.directory.Length == 0 ? "" : s.directory + "/") + s.known.level + ".xenon." + suffix;
        static string TdfPath(UntamedTrackSource s) => s.root == null ? null : (s.root.Length == 0 ? "" : s.root + "/") + "AI/" + s.known.TdfName;

        /// The level's four files, then its race line (null: missing).
        static VolumeFile[] Files(UntamedTrackSource s)
        {
            var r = Suffixes.Select(x => s.volume.Find(FilePath(s, x))).ToList();
            string tdf = TdfPath(s);
            r.Add(tdf == null ? null : s.volume.Find(tdf));
            return r.ToArray();
        }

        /// null: the supported disc's files; else what differs.
        static string Check(UntamedTrackSource s, CancellationToken cancel)
        {
            var k = s.known; var files = Files(s);
            for (int i = 0; i < 4; i++)
            {
                if (files[i] == null) return "missing Database/" + k.level + ".xenon." + Suffixes[i];
                if (files[i].size != k.sizes[i]) return k.level + ".xenon." + Suffixes[i] + " differs from the supported disc (size)";
            }
            if (files[4] == null) return "missing AI/" + k.TdfName + (s.root == null ? " (choose the folder that holds both Database and AI)" : "");
            if (files[4].size != k.sizes[4]) return k.TdfName + " differs from the supported disc (size)";
            try
            {
                using (var h = SHA256.Create())
                    foreach (var (f, sha) in new[] { (files[1], k.databaseSha256), (files[3], k.sceneSha256), (files[4], k.tdfSha256) })
                        if (Bin.Hex(h.ComputeHash(s.volume.Read(f, cancel)), 0, 32) != sha) return f.Name + " differs from the supported disc";
            }
            catch (AliveImportException ex) { return ex.Message; }
            catch (Exception ex) when (ex is IOException || ex is UnauthorizedAccessException) { return "can't be read: " + ex.Message; }
            return null;
        }

        /// The custom track converted from this stadium before, relative to tracksFolder: Mods/Tracks/Untamed/<short> or <short>-v<n> (the
        /// hand-built releases) holding a level file set; the highest version when there are several. null: none (or no tracks folder).
        public static string InstalledFolder(string tracksFolder, UntamedKnown k)
        {
            if (string.IsNullOrEmpty(tracksFolder)) return null;
            try
            {
                if (!Directory.Exists(tracksFolder)) return null;
                string dir = Directory.GetDirectories(tracksFolder).FirstOrDefault(d => string.Equals(Path.GetFileName(d), TracksSubfolder, StringComparison.OrdinalIgnoreCase));
                if (dir == null) return null;
                var name = new Regex("^" + Regex.Escape(k.shortName) + @"(?:-v(\d+))?$", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);
                string best = null; int bestVersion = -1;
                foreach (var d in Directory.GetDirectories(dir))
                {
                    var m = name.Match(Path.GetFileName(d));
                    if (!m.Success) continue;
                    bool level;
                    try { level = Directory.EnumerateFiles(d, "*.dx9.level").Any(); }
                    catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { continue; }   // (a dangling link, an unreadable folder)
                    if (!level) continue;
                    int version = m.Groups[1].Success && int.TryParse(m.Groups[1].Value, out int n) ? n : 0;
                    if (version > bestVersion) { best = d; bestVersion = version; }
                }
                return best == null ? null : Path.GetFileName(dir) + "/" + Path.GetFileName(best);
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { return null; }
        }

        /// Mods/Tracks' Untamed folder in any case (null: none, or it can't be read).
        static string UntamedFolder(string tracksFolder)
        {
            if (string.IsNullOrEmpty(tracksFolder)) return null;
            try
            {
                if (!Directory.Exists(tracksFolder)) return null;
                return Directory.GetDirectories(tracksFolder).FirstOrDefault(d => string.Equals(Path.GetFileName(d), TracksSubfolder, StringComparison.OrdinalIgnoreCase));
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { return null; }
        }

        /// The track this importer made of a stadium, relative to tracksFolder ("Untamed/norton"): the folder named by the short name that
        /// holds the import manifest. null: none.
        public static string ImportedFolder(string tracksFolder, UntamedKnown k)
        {
            string dir = UntamedFolder(tracksFolder);
            if (dir == null) return null;
            try
            {
                string d = Directory.GetDirectories(dir).FirstOrDefault(x => string.Equals(Path.GetFileName(x), k.shortName, StringComparison.OrdinalIgnoreCase));
                return d != null && File.Exists(Path.Combine(d, ManifestName)) ? Path.GetFileName(dir) + "/" + Path.GetFileName(d) : null;
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { return null; }
        }

        /// The copies of a stadium installed by hand, relative to tracksFolder: Untamed/<short>-v<n> (hand-built releases) holding a
        /// level file set, the highest version first. The importer never changes them.
        public static List<string> HandInstalled(string tracksFolder, UntamedKnown k)
        {
            var found = new List<(int version, string rel)>();
            string dir = UntamedFolder(tracksFolder);
            if (dir == null) return new List<string>();
            try
            {
                var name = new Regex("^" + Regex.Escape(k.shortName) + @"-v(\d+)$", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);
                foreach (var d in Directory.GetDirectories(dir))
                {
                    var m = name.Match(Path.GetFileName(d));
                    if (!m.Success) continue;
                    bool level;
                    try { level = Directory.EnumerateFiles(d, "*.dx9.level").Any(); }
                    catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { continue; }
                    if (level) found.Add((int.TryParse(m.Groups[1].Value, out int n) ? n : 0, Path.GetFileName(dir) + "/" + Path.GetFileName(d)));
                }
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { }
            return found.OrderByDescending(x => x.version).Select(x => x.rel).ToList();
        }

        // ------------------------------------------------------------------------------------------------ where a stadium is imported to

        static string ChildIgnoreCase(string dir, string name)
        {
            if (!Directory.Exists(dir)) return null;
            return Directory.GetDirectories(dir).FirstOrDefault(d => string.Equals(Path.GetFileName(d), name, StringComparison.OrdinalIgnoreCase));
        }

        static bool IsLink(string path)
        {
            try { return (File.Exists(path) || Directory.Exists(path)) && ReflexFs.IsLink(path); }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { return false; }
        }

        /// The folder a stadium is imported into: Mods/Tracks' Untamed folder and the stadium's folder in it, each as it is named on the
        /// disk (any case), else as the import creates it. rel: relative to tracksFolder ("Untamed/norton").
        static void Destination(string tracksFolder, UntamedKnown k, out string untamed, out string dest, out string rel)
        {
            untamed = ChildIgnoreCase(tracksFolder, TracksSubfolder) ?? Path.Combine(tracksFolder, TracksSubfolder);
            dest = ChildIgnoreCase(untamed, k.shortName) ?? Path.Combine(untamed, k.shortName);
            rel = Path.GetFileName(untamed) + "/" + Path.GetFileName(dest);
        }

        /// Why no stadium at all can be imported into this tracks folder (null: they can): Mods/Tracks/Untamed is a file, or a link to
        /// another folder. Throws what the file system throws.
        static string UntamedFolderProblem(string untamed)
        {
            string shown = "Mods/Tracks/" + Path.GetFileName(untamed);
            if (File.Exists(untamed)) return shown + " is a file, not a folder; move or delete it to import the Untamed tracks";
            if (IsLink(untamed)) return shown + " is a symbolic link to another folder; the importer writes nothing through a link";
            return null;
        }

        /// Why the import can't write a stadium's folder (null: it can): the one test of the list (UntamedTrackSource.inTheWay) and of
        /// the import, which asks it before it converts the stadium. Throws what the file system throws.
        static string DestinationProblem(string untamed, string dest, string rel, UntamedKnown k)
        {
            string problem = UntamedFolderProblem(untamed);
            if (problem != null) return problem;
            string shown = "Mods/Tracks/" + rel;
            if (IsLink(dest)) return shown + " is a symbolic link to another folder; the importer writes nothing through a link";
            if (File.Exists(dest)) return shown + " is a file; move or delete it to import " + k.name;
            // a folder of the player's own (no import manifest), whatever it holds: never replaced
            if (Directory.Exists(dest) && !File.Exists(Path.Combine(dest, ManifestName)))
                return shown + " is already there and was not made by this importer; move or delete that folder to import " + k.name;
            return null;
        }

        /// Why the import can't write a stadium's folder Mods/Tracks/Untamed/<short> (null: it can, or there is no tracks folder, or it
        /// can't be read now: the import says so).
        public static string DestinationProblem(string tracksFolder, UntamedKnown k)
        {
            if (string.IsNullOrEmpty(tracksFolder)) return null;
            try
            {
                Destination(tracksFolder, k, out string untamed, out string dest, out string rel);
                return DestinationProblem(untamed, dest, rel, k);
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { return null; }
        }

        /// The stadiums converted before (the library card), in the table's order.
        public static List<UntamedKnown> Installed(string tracksFolder) =>
            KnownTracks.GroupBy(k => k.level, StringComparer.OrdinalIgnoreCase).Select(g => g.First()).Where(k => InstalledFolder(tracksFolder, k) != null).ToList();
    }
}
