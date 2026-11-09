// Custom tracks (MODS/TRACKS): the PC community's track mods, installed by dropping each into a folder of its own in
// <client root>/Mods/Tracks/ (ReflexMods.cs) - no beta slots, no copying over the game's files.
//
// What the original needed: the community's tracks are built as the files of one of the beta slots their tools know - Beta_Track_Slot_<n>
// (a Free Ride map), Beta_Nat_Track_Slot_<n> (a National track), Beta_Sx_Track_Slot_<n> (a Supercross track): <slot>.dx9.database,
// .dx9.level, .dx9.package and .dx9.scene - and a player installed one by copying them over the slot's files in the game's Database folder,
// one track per slot (the install in this repository has no Beta_* files or table rows at all: the slots are the tools' convention). The
// game loads a level by its name only (MXGame's level config: the database <level>, the packages Shared / <level>Base, the scene file's
// own subscene directory), so a slot's files are a complete level.
//
// Here: a track is a folder at any depth under Mods/Tracks that holds a <stem>.dx9.{level,database,package,scene} set (any case; several
// stems in one folder are several tracks; a set with files missing is listed as a broken track with the reason). Its kind comes from the
// stem (Beta_Nat_Track_Slot_4: National, Beta_Sx_Track_Slot_1: Supercross, Beta_Track_Slot_1: Free Ride; an original level name's prefix
// NAT_ / SX_ / FR_ ... the same), its name from the folder ("1138-farm-14-mx" -> "Farm 14 MX"); mod.ini / mod.json (name, author,
// description, thumbnail, kind) override, like the bike mods. An archive (a zip, RAR or 7z by its first bytes, whatever its name says; or
// a .zip / .rar / .7z name) is installed by the game (TrackArchives.cs: extracted into <its folder>/<its name>/ on a worker, the archive
// kept); until then, or when it cannot be, it is listed as a note that says what is happening or what is wrong.
//
// Unlimited tracks, the install never modified: a track is bound to its stem (the level name) only for the event that rides it -
//   the core   rx_session_mount_level (core/include/rx_api.h): the session's file system sees the track's four files at
//              Database/<stem>.dx9.* (OpenReflexSimulation mounts the bound track before rx_session_event_load);
//   the client Rfx.Install.TrackRouter: the original install's Db(<stem>) answers the track's own install whose database is named
//              "modtrack:<id>>" + stem, so models, textures and materials are keyed by the mod's namespace (never cached under the slot's
//              name); the level's .scene / .level files through LevelFile. Bind() happens where the event scene takes its event
//              (ReflexEventSelect.Awake), so two Free Ride tracks that both ship as Beta_Track_Slot_1 coexist.
// Packed tracks (core/plan/RXARCHIVE.md): a .rxtrack version 5 file (an RXAR archive: an old track mod, a base or Alive track converted by
// tools/rxpack or the client) is a track like a folder: its files are "<file>.rxtrack/Database/<stem>.dx9.*" paths (RxFiles reads them), its
// dress (mod.ini, mod.json, pictures) "<file>.rxtrack/Mod/*"; the core mounts the archive itself (rx_session_mount_level takes its path). The
// gzip .rxtrack packages (RXPK, the MTX terrain tracks: ExperimentalTerrain) are told apart by their first bytes and keep their own path.
// The events: one Free Ride per track (FreeRide/<stem>/@<token>), plus a race for a National (the National type) or Supercross track
// (the Supercross type), made from the original's first event of the type (its rules, classes, durations) at no location row (-1). Every
// custom track is offline only and keeps no records (the core: map_scale.h rx_ms_custom_track).
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using UnityEngine;

namespace Reflex.Port.Content
{
    public sealed class TrackMod
    {
        public string id;              // the folder relative to Mods/Tracks, '/' separators ("Farm 14"); "+<stem>" added when one folder holds several
        public string name, author = "", description = "", thumbnail;   // thumbnail: a full path or null
        public string folder;
        /// The track files' stem, the level name the track is bound to while it is ridden ("Beta_Nat_Track_Slot_4").
        public string level;
        public bool experimentalTerrain, experimentalRacing;
        /// A packed track (a .rxtrack version 5): folder is the archive file, its files are paths inside it (RxFiles).
        public bool packed;
        /// Where the dress files are (mod.ini's thumbnail is relative to it): the folder, or a packed track's Mod/.
        public string DressFolder => packed ? Path.Combine(folder, "Mod") : folder;
        /// The engine activity of its race ("National", "Supercross"), or "FreeRide" for a track that is only free-ridden.
        public string kind = "FreeRide";
        /// "FREE RIDE", "NATIONAL", "SUPERCROSS" ... (what the file names, or mod.ini, say the track is).
        public string kindLabel = "FREE RIDE";
        /// Why the track cannot be ridden (null: ready).
        public string problem;
        /// A loose archive: not a track, the note to extract it.
        public bool archive;
        /// The archive's file and its install (TrackArchives): null / "installing" / "failed" (a note in the browser either way).
        public string archivePath, archiveState;
        public long bytes;
        /// The track's files by lower-case file name.
        internal readonly Dictionary<string, string> files = new Dictionary<string, string>(StringComparer.Ordinal);
        internal Rfx.Install install;
        public string Namespace => TrackMods.NamespaceMark + id + ">";
        public bool Ok => problem == null && !archive;
        public bool Races => (!experimentalTerrain || experimentalRacing) && Ok && (kind == "National" || kind == "Supercross");
        /// The event keys' unique part: "@" + the id's letters and digits + a hash of the id ("@Farm-14.5f3a09c1").
        public string Token { get { var sb = new StringBuilder("@"); foreach (char c in id) sb.Append(char.IsLetterOrDigit(c) && c < 128 ? c : '-'); return sb + "." + TrackMods.Fnv(id).ToString("x8"); } }
        public string File(string ext) => files.TryGetValue((level + ext).ToLowerInvariant(), out var f) ? f : null;
        public string Title => string.IsNullOrEmpty(author) ? name : name + " · " + author;
    }

    public static class TrackMods
    {
        public const string NamespaceMark = "modtrack:";
        static TrackMods()
        {
            Rfx.Install.TrackRouter = Route; TrackArchives.Log = Debug.Log;
        }

        static List<TrackMod> s_mods;
        static List<ReflexEvents.Event> s_events;
        static string s_signature;
        static readonly object s_lock = new object();
        /// Bumped by every rescan that found a change (OpenContent rebuilds its custom events).
        public static int Version { get; private set; }

        /// The scanned folder (<client root>/Mods/Tracks), null without a client root.
        public static string Folder => ReflexMods.Folder(ReflexMods.TracksName);

        /// Every track found, broken ones and archive notes included (cached; Refresh rescans).
        public static List<TrackMod> All
        {
            get
            {
                lock (s_lock)
                {
                    if (s_mods != null) return s_mods;
                    Rfx.Install.TrackRouter = Route;
                    string folder = Folder;
                    s_archiveGen = TrackArchives.Generation;
                    try { s_mods = Scan(folder); }
                    catch (Exception e) { Debug.LogWarning("[TrackMods] scan: " + e.Message); s_mods = new List<TrackMod>(); }
                    s_signature = Signature(folder);
                    try { Install(s_mods, folder); } catch (Exception e) { Debug.LogWarning("[TrackMods] archives: " + e.Message); }
                    s_events = null;
                    foreach (var m in s_mods)
                        Debug.Log($"[TrackMods] {m.id}: \"{m.name}\" {(m.archive ? "archive" : m.level)} {m.kindLabel}" + (m.problem != null ? " BROKEN: " + m.problem : "") + " in " + m.folder);
                    return s_mods;
                }
            }
        }
        /// Rescans when the folder changed since the last scan (a file or folder added, removed or rewritten). True when it did.
        public static bool Refresh()
        {
            lock (s_lock)
            {
                if (s_mods != null && Signature(Folder) == s_signature && TrackArchives.Generation == s_archiveGen) return false;
                s_mods = null; s_events = null; Version++;
            }
            _ = All;
            return true;
        }
        static int s_archiveGen;

        /// The archives of a scan: each one's install started (TrackArchives.Ensure; once per file), its note says how it is going.
        static void Install(List<TrackMod> mods, string root)
        {
            foreach (var m in mods)
            {
                if (!m.archive || m.archivePath == null) continue;
                var job = TrackArchives.Ensure(m.archivePath, root);
                string file = Path.GetFileName(m.archivePath), kind = TrackArchives.Name(job.kind);
                if (job.state == "installing") { m.archiveState = "installing"; m.kindLabel = "INSTALLING"; m.problem = "installing " + file + (job.kind != TrackArchives.Kind.None ? " (a " + kind + " archive)" : "") + ": the game is extracting it into a folder of its own - it is listed here in a moment"; }
                else if (job.state == "failed") { m.archiveState = "failed"; m.kindLabel = "CANNOT INSTALL"; m.problem = job.error; }
                // done: the next scan lists the track (the folder it made) and leaves the archive out
            }
        }

        public static TrackMod Find(string id) => string.IsNullOrEmpty(id) ? null : All.FirstOrDefault(m => m.id == id);

        // ------------------------------------------------------------------------------------------------ binding

        static TrackMod s_bound;
        /// The track the current event rides (null: an original event).
        public static TrackMod Bound => s_bound;
        /// The event scene's event (ReflexEventSelect.Awake): its track bound to its level name, or none.
        public static void Bind(TrackMod m)
        {
            if (m != null && !m.Ok) m = null;
            Reflex.Port.RxCore.RxCustomTrack.Set(m?.level, m?.folder);   // (the core's mount: OpenReflexSimulation)
            if (m == s_bound) return;
            s_bound = m;
            Rfx.Install.TrackRouter = Route;
            Debug.Log(m != null ? $"[TrackMods] {m.level} is \"{m.name}\" ({m.folder}) for this event" : "[TrackMods] no custom track bound");
        }

        /// Rfx.Install.TrackRouter: a namespaced name to its track; the bound track's plain level name to it.
        static (Rfx.Install, string) Route(string db)
        {
            if (string.IsNullOrEmpty(db)) return (null, null);
            if (db.StartsWith(NamespaceMark, StringComparison.Ordinal))
            {
                int e = db.LastIndexOf('>');
                if (e <= NamespaceMark.Length) return (null, null);
                string id = db.Substring(NamespaceMark.Length, e - NamespaceMark.Length);
                var m = s_bound != null && s_bound.id == id ? s_bound : (s_mods ?? new List<TrackMod>()).FirstOrDefault(x => x.id == id && x.Ok);
                return m == null ? (null, null) : (InstallOf(m), db);
            }
            var b = s_bound;
            return b != null && !b.experimentalTerrain && string.Equals(db, b.level, StringComparison.OrdinalIgnoreCase) ? (InstallOf(b), b.Namespace + b.level) : (null, null);
        }
        static Rfx.Install InstallOf(TrackMod m)
        {
            lock (m.files) return m.install ??= new Rfx.Install(m.packed ? Path.Combine(m.folder, "Database") : m.folder, m.Namespace, m.files);
        }

        /// A level's file (ext ".dx9.scene", ".dx9.level"): the bound track's own when level is its level name, else the install's.
        public static string LevelFile(string level, string ext)
        {
            var b = s_bound;
            if (b != null && !b.experimentalTerrain && string.Equals(level, b.level, StringComparison.OrdinalIgnoreCase)) return b.File(ext) ?? Path.Combine(b.folder, level + ext);
            return Path.Combine(ReflexInstall.Packages.dir, level + ext);
        }

        /// The level name as the client's per-level rules read it (the prefix: "SX_" a stadium, "NAT_" the Nationals' audio ...): a bound
        /// custom track's level gets its kind's prefix ("Beta_Sx_Track_Slot_1" -> "SX_Beta_Sx_Track_Slot_1"); any other name as it is.
        public static string Style(string level)
        {
            var b = s_bound;
            if (b == null || level == null || !string.Equals(level, b.level, StringComparison.OrdinalIgnoreCase)) return level;
            switch (b.kind)
            {
                case "National": return "NAT_" + level;
                case "Supercross": return "SX_" + level;
                case "Terracross": return "TRX_" + level;
                case "Powercross": return "PX_" + level;
                case "Freestyle": return "FMX_" + level;
                default: return "FR_" + level;
            }
        }

        // ------------------------------------------------------------------------------------------------ the events

        /// The custom tracks' events: a Free Ride per track that can be ridden, a race for a National / Supercross track (cached per scan).
        public static IReadOnlyList<ReflexEvents.Event> Events
        {
            get
            {
                var mods = All;
                lock (s_lock) if (s_events != null) return s_events;
                var list = new List<ReflexEvents.Event>();
                IReadOnlyList<ReflexEvents.Event> all;
                try { all = ReflexEvents.All; } catch (Exception e) { Debug.LogWarning("[TrackMods] events: " + e.Message); all = new List<ReflexEvents.Event>(); }
                int freeRideType = all.FirstOrDefault(e => e.spec.type == "FreeRide" && e.eventTypeLocationId >= 0)?.spec.typeId ?? 9;
                int order = 0;
                foreach (var m in mods)
                {
                    order++;
                    if (m.archive) continue;
                    list.Add(Make(m, "FreeRide", "Free Ride", freeRideType, null, order));
                    if (!m.Races) continue;
                    var template = all.FirstOrDefault(e => e.spec.type == m.kind && e.freeRideOf == null && !e.Dlc && e.vehicleClasses.Length > 0);
                    if (template != null) list.Add(Make(m, m.kind, template.typeName, template.spec.typeId, template, order));
                }
                lock (s_lock) return s_events ??= list;
            }
        }
        static ReflexEvents.Event Make(TrackMod m, string activity, string typeName, int typeId, ReflexEvents.Event template, int order)
        {
            int engine = ReflexEvents.ActivityIndex(activity);
            var e = new ReflexEvents.Event
            {
                key = activity + "/" + m.level + "/" + m.Token, typeName = typeName, location = m.level, locationLocId = -1,
                eventTypeLocationId = -1, storeProductId = -1, sort = 20000 + order, tables = "ModsTracks", trackMod = m,
                sceneSection = ReflexEvents.SceneSection(engine, m.level, 0),
                spec = new ReflexWorldFiles.EventSpec
                {
                    type = activity, location = m.level, durationType = template?.spec.durationType ?? "", typeId = typeId,
                    locationId = -1, trackId = 0, vehicleClass = template?.spec.vehicleClass ?? -1, riders = 1, ai = 0,
                    duration = template?.spec.duration ?? 0, activity = engine,
                },
            };
            if (template != null) { e.vehicleClasses = template.vehicleClasses; e.atv = template.Atv; }
            return e;
        }
        /// A custom track's event by key (ReflexEvents.Find), null when there is none (or its track is broken / gone).
        public static ReflexEvents.Event FindEvent(string key)
        {
            if (string.IsNullOrEmpty(key) || key.IndexOf("/@", StringComparison.Ordinal) < 0) return null;
            return Events.FirstOrDefault(e => string.Equals(e.key, key, StringComparison.OrdinalIgnoreCase));
        }

        // ------------------------------------------------------------------------------------------------ the scan

        const int MaxDepth = 10;
        const string DbExt = ".dx9.database", LevelExt = ".dx9.level", PkgExt = ".dx9.package", SceneExt = ".dx9.scene";
        static readonly string[] Parts = { LevelExt, DbExt, PkgExt, SceneExt };
        static readonly string[] Archives = TrackArchives.Extensions;
        static readonly string[] ThumbNames = { "thumbnail", "preview", "thumb", "screenshot", "cover", "image" };

        /// What the walk sees of a folder (names, sizes and write times of its files and folders, at any depth): a rescan only when it changes.
        static string Signature(string root)
        {
            if (string.IsNullOrEmpty(root) || !Directory.Exists(root)) return "";
            var sb = new StringBuilder();
            void Walk(string dir, int depth)
            {
                try
                {
                    foreach (var f in Directory.GetFiles(dir).OrderBy(x => x, StringComparer.Ordinal))
                    { var i = new FileInfo(f); sb.Append(f).Append('|').Append(i.Length).Append('|').Append(i.LastWriteTimeUtc.Ticks).Append('\n'); }
                    if (depth < MaxDepth) foreach (var d in Directory.GetDirectories(dir).OrderBy(x => x, StringComparer.Ordinal)) if (!ReflexMods.IsJunk(Path.GetFileName(d))) { sb.Append(d).Append('/').Append('\n'); Walk(d, depth + 1); }
                }
                catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { }
            }
            Walk(root, 0);
            return sb.ToString();
        }

        /// The tracks under root (pure file system, no Unity): every folder's <stem>.dx9.* sets (any depth, junk ignored), ordered by name;
        /// loose archives as notes.
        public static List<TrackMod> Scan(string root)
        {
            var mods = new List<TrackMod>();
            if (string.IsNullOrEmpty(root) || !Directory.Exists(root)) return mods;
            void Walk(string dir, int depth)
            {
                string[] files, dirs;
                try { files = Directory.GetFiles(dir); dirs = Directory.GetDirectories(dir); }
                catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { return; }
                mods.AddRange(Probe(root, dir, files));
                if (depth >= MaxDepth) return;
                foreach (var d in dirs.OrderBy(x => x, StringComparer.OrdinalIgnoreCase)) if (!ReflexMods.IsJunk(Path.GetFileName(d))) Walk(d, depth + 1);
            }
            Walk(root, 0);
            // two tracks of the same name: the parent folder (or the stem) tells them apart
            foreach (var g in mods.GroupBy(m => m.name, StringComparer.OrdinalIgnoreCase).Where(g => g.Count() > 1))
                foreach (var m in g) { int s = m.id.LastIndexOf('/'); m.name += " (" + (s > 0 ? m.id.Substring(0, s) : m.archive ? "archive" : m.level) + ")"; }
            mods.Sort((a, b) => a.archive != b.archive ? (a.archive ? 1 : -1) : string.Compare(a.name, b.name, StringComparison.OrdinalIgnoreCase));
            return mods;
        }

        /// The tracks of one folder.
        static IEnumerable<TrackMod> Probe(string root, string dir, string[] paths)
        {
            var byLower = new Dictionary<string, string>(StringComparer.Ordinal);
            foreach (var f in paths) { string n = Path.GetFileName(f); if (!ReflexMods.IsJunk(n)) byLower[n.ToLowerInvariant()] = f; }
            bool top = string.Equals(Path.GetFullPath(dir).TrimEnd('/', '\\'), Path.GetFullPath(root).TrimEnd('/', '\\'), StringComparison.Ordinal);   // (files right in Mods/Tracks)
            string rel = top ? "" : ReflexMods.RelativeId(root, dir) ?? Path.GetFileName(dir);
            foreach (var package in byLower.Where(kv => kv.Key.EndsWith(".rxtrack", StringComparison.Ordinal)))
            {
                if (RxFiles.IsArchive(package.Value)) { foreach (var p in ProbePacked(rel, top, package.Value)) yield return p; continue; }
                var m = new TrackMod
                {
                    id = (top ? "" : rel + "/") + Path.GetFileNameWithoutExtension(package.Value),
                    folder = package.Value, level = "NAT_Concord", name = Path.GetFileNameWithoutExtension(package.Value),
                    kind = "National", kindLabel = "NATIONAL · EXPERIMENTAL", experimentalTerrain = true,
                };
                try
                {
                    var info = ExperimentalTerrain.Info(m.folder);
                    ExperimentalTerrain.Read(m.folder);
                    m.kind = info.kind == "Supercross" ? "Supercross" : "National";
                    m.kindLabel = m.kind.ToUpperInvariant()+" · EXPERIMENTAL";
                    m.experimentalRacing = info.raceVersion == 1; m.name = info.name; m.bytes = new FileInfo(m.folder).Length;
                }
                catch (Exception e) { m.problem = e.Message; }
                yield return m;
            }
            // the stems of the folder's track files ("beta_track_slot_1" -> its spelling on disk)
            var stems = new SortedDictionary<string, string>(StringComparer.Ordinal);
            foreach (var kv in byLower)
                foreach (var ext in Parts)
                    if (kv.Key.EndsWith(ext, StringComparison.Ordinal) && kv.Key.Length > ext.Length)
                    {
                        string disk = Path.GetFileName(kv.Value);
                        string stem = disk.Substring(0, disk.Length - ext.Length);
                        if (!stems.ContainsKey(stem.ToLowerInvariant()) || kv.Key.EndsWith(LevelExt, StringComparison.Ordinal)) stems[stem.ToLowerInvariant()] = stem;
                    }
            foreach (var stem in stems.Values)
            {
                string folderName = top ? null : Path.GetFileName(dir.TrimEnd('/', '\\'));
                var m = new TrackMod
                {
                    id = (top ? "" : rel) + (stems.Count > 1 || top ? (top ? "" : "+") + stem : ""), folder = dir, level = stem,
                    name = folderName != null ? Pretty(folderName) + (stems.Count > 1 ? " (" + Pretty(stem) + ")" : "") : Pretty(stem),
                };
                foreach (var kv in byLower) if (kv.Key.StartsWith(stem.ToLowerInvariant() + ".", StringComparison.Ordinal)) m.files[kv.Key] = kv.Value;
                foreach (var kv in byLower) if (kv.Key == "mod.ini" || kv.Key == "mod.json" || IsImage(kv.Key)) m.files[kv.Key] = kv.Value;
                (m.kind, m.kindLabel) = KindOf(stem);
                ReadInfo(m, byLower);
                m.thumbnail ??= Thumbnail(byLower, stem);
                m.problem = Check(m);
                try { foreach (var ext in Parts) { var f = m.File(ext); if (f != null) m.bytes += new FileInfo(f).Length; } } catch (Exception) { }
                yield return m;
            }
            foreach (var kv in byLower)
            {
                // an archive by its first bytes (a RAR named .zip is a RAR) or by its name; one already installed (TrackArchives' marker in
                // the folder it made) is not listed again
                string ext = Path.GetExtension(kv.Key);
                bool part = Parts.Any(p => kv.Key.EndsWith(p, StringComparison.Ordinal)) || IsImage(kv.Key) || kv.Key == "mod.ini" || kv.Key == "mod.json" || ext == ".txt";
                var kind = part ? TrackArchives.Kind.None : TrackArchives.Sniff(kv.Value);
                if (kind == TrackArchives.Kind.None && !Archives.Contains(ext)) continue;
                if (TrackArchives.InstalledFolder(kv.Value) != null) continue;
                string file = Path.GetFileName(kv.Value), stem = TrackArchives.SafeName(Path.GetFileNameWithoutExtension(file));
                yield return new TrackMod
                {
                    id = (top ? "" : rel + "/") + file, folder = dir, level = "", archive = true, archivePath = kv.Value, name = Pretty(Path.GetFileNameWithoutExtension(file)),
                    kindLabel = "ARCHIVE", problem = $"{file} is {(kind == TrackArchives.Kind.None ? "named as an archive" : "a " + TrackArchives.Name(kind) + " archive")}: the game extracts it into Mods/Tracks/{(top ? "" : rel + "/")}{stem}/ when it lists the tracks",
                };
            }
        }

        /// The tracks of a .rxtrack version 5 (one per <stem>.dx9.level inside it; normally one).
        static IEnumerable<TrackMod> ProbePacked(string rel, bool top, string file)
        {
            string fileStem = Path.GetFileNameWithoutExtension(file);
            string[] paths = RxFiles.PathsOf(file);
            if (paths.Length == 0)
            {
                yield return new TrackMod { id = (top ? "" : rel + "/") + fileStem, folder = file, level = "", packed = true, name = Pretty(fileStem), kindLabel = "PACKED TRACK",
                                            problem = Path.GetFileName(file) + " does not read (" + (RxNativeOk() ? "a damaged archive" : "the core library is not available") + ")" };
                yield break;
            }
            var byLower = new Dictionary<string, string>(StringComparer.Ordinal);
            foreach (string p in paths)
            {
                string name = p.Substring(p.LastIndexOf('/') + 1);
                if (p.StartsWith("Database/", StringComparison.OrdinalIgnoreCase) && p.IndexOf('/', 9) < 0 || p.StartsWith("Mod/", StringComparison.OrdinalIgnoreCase) && p.IndexOf('/', 4) < 0)
                    byLower[name.ToLowerInvariant()] = Path.Combine(file, p.Replace('/', Path.DirectorySeparatorChar));
            }
            string meta = RxFiles.MetaOf(file);
            var stems = byLower.Keys.Where(k => k.EndsWith(LevelExt, StringComparison.Ordinal)).Select(k => Path.GetFileName(byLower[k])).Select(n => n.Substring(0, n.Length - LevelExt.Length)).OrderBy(n => n, StringComparer.OrdinalIgnoreCase).ToList();
            foreach (string stem in stems)
            {
                var m = new TrackMod
                {
                    id = (top ? "" : rel + "/") + fileStem + (stems.Count > 1 ? "+" + stem : ""), folder = file, level = stem, packed = true,
                    name = Pretty(fileStem) + (stems.Count > 1 ? " (" + Pretty(stem) + ")" : ""),
                };
                foreach (var kv in byLower) if (kv.Key.StartsWith(stem.ToLowerInvariant() + ".", StringComparison.Ordinal) || kv.Key == "mod.ini" || kv.Key == "mod.json" || IsImage(kv.Key)) m.files[kv.Key] = kv.Value;
                string metaName = JsonString(meta, "name");
                if (!string.IsNullOrWhiteSpace(metaName) && !string.Equals(metaName, stem, StringComparison.Ordinal)) m.name = metaName.Trim();
                (m.kind, m.kindLabel) = KindOf(stem);
                ReadInfo(m, byLower);
                m.thumbnail ??= Thumbnail(byLower, stem);
                m.problem = Check(m);
                foreach (var ext in Parts) { var f = m.File(ext); if (f != null) m.bytes += Math.Max(0, RxFiles.Length(f)); }
                yield return m;
            }
        }
        static bool RxNativeOk() { try { return Reflex.Port.RxCore.RxArchiveNative.Available; } catch (Exception) { return false; } }
        /// A top-level string member of a JSON document ("name"), or null (the archive metadata written by tools/rxpack and ReflexDataPack).
        internal static string JsonString(string json, string member)
        {
            if (string.IsNullOrEmpty(json)) return null;
            var mt = System.Text.RegularExpressions.Regex.Match(json, "\"" + System.Text.RegularExpressions.Regex.Escape(member) + "\"\\s*:\\s*\"((?:[^\"\\\\]|\\\\.)*)\"");
            if (!mt.Success) return null;
            try { return System.Text.RegularExpressions.Regex.Unescape(mt.Groups[1].Value); } catch (ArgumentException) { return mt.Groups[1].Value; }
        }

        static bool IsImage(string lower) => lower.EndsWith(".png", StringComparison.Ordinal) || lower.EndsWith(".jpg", StringComparison.Ordinal) || lower.EndsWith(".jpeg", StringComparison.Ordinal);

        /// A picture of the folder named like a thumbnail (preview.png, <stem>.jpg ...), else the folder's only picture.
        static string Thumbnail(Dictionary<string, string> byLower, string stem)
        {
            var images = byLower.Where(kv => IsImage(kv.Key)).ToList();
            foreach (var n in ThumbNames.Prepend(stem.ToLowerInvariant()))
                foreach (var kv in images) if (Path.GetFileNameWithoutExtension(kv.Key) == n) return kv.Value;
            return images.Count == 1 ? images[0].Value : null;
        }

        /// Why a track cannot be ridden (null: it can): a file of its set missing, a level name the core cannot take, a database or a scene
        /// that does not read.
        static string Check(TrackMod m)
        {
            var missing = Parts.Where(e => m.File(e) == null).Select(e => m.level + e).ToList();
            if (missing.Count > 0) return "missing " + string.Join(", ", missing);
            if (m.level.Length > 60 || m.level.Any(c => c > 126 || c < 32 || c == '/' || c == '\\' || c == ':' || c == '%'))
                return "the file name " + m.level + " cannot be a level name (plain letters, digits and _ only, at most 60)";
            try
            {
                var db = Rfx.Bxml(RxFiles.ReadAllBytes(m.File(DbExt)));
                if (!db.All().Any(n => n.tag == "Package")) return m.level + ".dx9.database lists no package";
                var sections = ReflexScene.SectionNames(RxFiles.ReadAllBytes(m.File(SceneExt)));
                if (!sections.Any(s => s.EndsWith("Base", StringComparison.Ordinal))) return m.level + ".dx9.scene has no Base section";
                if (RxFiles.Length(m.File(PkgExt)) <= 0) return m.level + ".dx9.package is empty";
            }
            catch (Exception e) { return "the track's files do not read (" + e.Message + ")"; }
            return null;
        }

        /// The kind of a track's stem: the community's slots (Beta_Track_Slot_N, Beta_Nat_Track_Slot_N, Beta_Sx_Track_Slot_N; any other
        /// Beta_<kind>_Track_Slot_N by its kind), an original level name's prefix (NAT_, SX_, FR_ ...), else a Free Ride map.
        public static (string kind, string label) KindOf(string stem)
        {
            string token = null;
            var beta = Regex.Match(stem ?? "", @"^Beta_(?:([A-Za-z]+)_)?Track_Slot_?\d*$", RegexOptions.IgnoreCase);
            if (beta.Success) token = beta.Groups[1].Success ? beta.Groups[1].Value : "";
            else { int us = (stem ?? "").IndexOf('_'); token = us > 0 ? stem.Substring(0, us) : null; }
            switch ((token ?? "-").ToLowerInvariant())
            {
                case "": case "fr": return ("FreeRide", "FREE RIDE");
                case "nat": case "mx": return ("National", "NATIONAL");
                case "sx": return ("Supercross", "SUPERCROSS");
                // free-ridden only (the race types below are not verified on custom tracks): named for the menus
                case "trx": case "tx": return ("FreeRide", "TERRACROSS  ·  FREE RIDE");
                case "px": return ("FreeRide", "POWERCROSS  ·  FREE RIDE");
                case "fmx": case "fs": return ("FreeRide", "FREESTYLE  ·  FREE RIDE");
                default: return ("FreeRide", beta.Success ? token.ToUpperInvariant() + "  ·  FREE RIDE" : "FREE RIDE");
            }
        }

        /// "1138-farm-14-mx" -> "Farm 14 MX", "redbull_straight_rhythm" -> "Redbull Straight Rhythm", "Beta_Track_Slot_1" -> "Beta Track Slot 1".
        public static string Pretty(string raw)
        {
            string s = (raw ?? "").Trim();
            var lead = Regex.Match(s, @"^\d+\s*[-_. ]+\s*(?=\S)");
            if (lead.Success && lead.Length < s.Length) s = s.Substring(lead.Length);
            s = Regex.Replace(s, @"[_\-.]+", " ");
            s = Regex.Replace(s, @"\s+", " ").Trim();
            if (s.Length == 0) return raw ?? "";
            var words = s.Split(' ').Select(w =>
            {
                string u = w.ToUpperInvariant();
                if (Upper.Contains(u)) return u;
                if (w.Any(char.IsUpper) && w.Any(char.IsLower)) return w;   // already cased ("RedBull", "McGrath")
                return w.Length == 0 ? w : char.ToUpperInvariant(w[0]) + w.Substring(1).ToLowerInvariant();
            });
            return string.Join(" ", words);
        }
        static readonly HashSet<string> Upper = new HashSet<string>(StringComparer.Ordinal) { "MX", "SX", "FMX", "ATV", "UTV", "TRX", "PX", "AMA", "GP", "MXGP", "FR", "II", "III", "IV", "USA", "UK" };

        [Serializable] sealed class Info { public string name, author, description, thumbnail, kind; }

        /// mod.ini (key = value; ';' / '#' comments) or mod.json ({"name": ..}) of the track's folder.
        static void ReadInfo(TrackMod m, Dictionary<string, string> files)
        {
            string name = null, author = null, description = null, thumb = null, kind = null;
            try
            {
                if (files.TryGetValue("mod.ini", out var ini))
                {
                    var kv = BikeMods.ParseIni(RxFiles.ReadAllText(ini));
                    kv.TryGetValue("name", out name); kv.TryGetValue("author", out author); kv.TryGetValue("description", out description);
                    kv.TryGetValue("thumbnail", out thumb); kv.TryGetValue("kind", out kind);
                }
                else if (files.TryGetValue("mod.json", out var json))
                {
                    var info = JsonUtility.FromJson<Info>(RxFiles.ReadAllText(json));
                    if (info != null) { name = info.name; author = info.author; description = info.description; thumb = info.thumbnail; kind = info.kind; }
                }
            }
            catch (Exception e) { Debug.LogWarning("[TrackMods] " + m.folder + " info: " + e.Message); }
            if (!string.IsNullOrWhiteSpace(name)) m.name = name.Trim();
            if (!string.IsNullOrWhiteSpace(author)) m.author = author.Trim();
            if (!string.IsNullOrWhiteSpace(description)) m.description = description.Trim();
            if (!string.IsNullOrWhiteSpace(thumb))
            {
                string p = Path.Combine(m.DressFolder, thumb.Trim().Replace('\\', Path.DirectorySeparatorChar).Replace('/', Path.DirectorySeparatorChar));
                if (RxFiles.Exists(p)) m.thumbnail = p;
            }
            if (!string.IsNullOrWhiteSpace(kind))
                switch (kind.Trim().ToLowerInvariant().Replace(" ", ""))
                {
                    case "national": case "nat": case "mx": case "motocross": m.kind = "National"; m.kindLabel = "NATIONAL"; break;
                    case "supercross": case "sx": m.kind = "Supercross"; m.kindLabel = "SUPERCROSS"; break;
                    case "freeride": case "fr": m.kind = "FreeRide"; m.kindLabel = "FREE RIDE"; break;
                }
        }

        internal static uint Fnv(string s) { uint h = 2166136261; foreach (char c in s ?? "") { h ^= c; h *= 16777619; } return h; }

        // ------------------------------------------------------------------------------------------------ the thumbnails

        static readonly Dictionary<string, Texture2D> s_thumbs = new Dictionary<string, Texture2D>(StringComparer.Ordinal);
        /// The track's thumbnail picture (loaded once; null when it has none or it does not read). Main thread.
        public static Texture2D Thumbnail(TrackMod m)
        {
            if (m?.thumbnail == null) return null;
            if (s_thumbs.TryGetValue(m.thumbnail, out var t)) return t;
            t = null;
            try
            {
                // (w-joincrash) a mod's picture is anyone's file: at most 32 MB read, and a picture of at most 8192 x 8192 kept
                long size = RxFiles.Length(m.thumbnail);
                if (size <= 0 || size > 32L << 20) { Debug.LogWarning("[TrackMods] thumbnail " + m.thumbnail + ": " + size + " bytes, not read"); return s_thumbs[m.thumbnail] = null; }
                var bytes = RxFiles.ReadAllBytes(m.thumbnail);
                t = new Texture2D(2, 2, TextureFormat.RGBA32, true) { name = "ou_trackmod " + m.id, wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Trilinear, anisoLevel = 4, hideFlags = HideFlags.DontSave };
                if (!t.LoadImage(bytes, false)) { UnityEngine.Object.Destroy(t); t = null; }
                else if (t.width > 8192 || t.height > 8192) { Debug.LogWarning($"[TrackMods] thumbnail {m.thumbnail}: {t.width} x {t.height}, too large"); UnityEngine.Object.Destroy(t); t = null; }
            }
            catch (Exception e) { Debug.LogWarning("[TrackMods] thumbnail " + m.thumbnail + ": " + e.Message); t = null; }
            return s_thumbs[m.thumbnail] = t;
        }

        /// Opens Mods/Tracks in the desktop's file browser (made first when missing).
        public static void OpenFolder()
        {
            string f = Folder; if (f == null) return;
            try { ReflexMods.EnsureLayout(); Directory.CreateDirectory(f); } catch (Exception e) { Debug.LogWarning("[TrackMods] " + f + ": " + e.Message); }
            Application.OpenURL(new Uri(f.EndsWith(Path.DirectorySeparatorChar.ToString()) ? f : f + Path.DirectorySeparatorChar).AbsoluteUri);
        }
    }
}
