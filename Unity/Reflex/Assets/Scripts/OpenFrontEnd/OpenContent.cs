// What the OPEN REFLEX shell shows, all read from the player's install at run time (nothing of the game is in the project):
//   OpenContent   the events (ReflexEvents.All: every single-player event the port runs, DLC included) with their event type, location
//                 and track, the game's own names and texts (FrontEnd strings: STR_MXACTIVITYTYPE_*, the locations' store-product
//                 titles and descriptions) and pictures (the LocationSelect icons: PreviewIconId / MapIconId / PosterIconId, the event
//                 types' posters EventType.IconId), as the original front end's EventSelect / LocationSelect read them (FeAssets,
//                 FeTables); an MX vs ATV Alive level (no Reflex location row) shows Alive's own pictures of it, written into the install
//                 by the importer (AlivePicture: its loading picture and its track / Free Ride map).
//   OpenSetup     one event's setup as LocationSelect + Vehicle Select make it: AI (OFF / ROOKIE 80 / AMATEUR 95 / PRO 105 / ALL TIME 120;
//                 on = a full gate of 11, the world image's field), the Event Duration (EventTypeDurations, laps), the vehicle type,
//                 its class (LocationSelect's list for the type) and the rider (the player's own on the profile's MX Lite, or a pro
//                 of Vehicle Select with his vehicle of the type), and its launch: ReflexEventSelect.Launch, exactly the call the
//                 original's LocationSelect:OnAccept makes (FrontEndManager.DoLaunch).
//   OpenHistory   the favourite locations, the recently ridden events with their setup (CONTINUE / RIDE NOW), the browser's view / sort.
using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEngine;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed class OpenEvent
    {
        public ReflexEvents.Event ev;
        public string key, activity, typeName, typeDescription, typeVehicles, locationName, description;
        public int typeId, previewIcon = -1, mapIcon = -1, posterIcon = -1, typeIcon = -1;
        public bool dlc, freeRide;
        public float order;
        public string haystack;
        /// The browser filter the event is listed under: its activity, or for a Free Ride on a race level (ReflexEvents.FreeRideAnyLevel)
        /// the race type of the level ("National", "Supercross" ...).
        public string group;
        public string Group => group ?? activity;
        /// The location the event's favourite / choices are kept under: its level, a custom track's "@<its id>" (custom tracks share
        /// their slot's level name).
        public string Location => ev.trackMod != null ? "@" + ev.trackMod.id : ev.location;
        /// RESTORATION (w-fresnob): an event on a level cut from the original game (ReflexEvents.Restored), listed under RESTORED.
        public bool Restored => mod == null && ev.restored;
        /// A converted MX vs ATV Alive level's event: its Free Ride, or its AI race (PORT ADDITION, w-aliveai).
        public bool Alive => mod == null && (freeRide || ev.aliveRace) && AliveLevels.Is(ev.location);
        /// MODS/TRACKS (Content/TrackMods.cs): the custom track of the event (Mods/Tracks), null for the original's.
        public TrackMod mod;
        public bool Custom => mod != null;
        /// A track the MX vs ATV Untamed importer made (Content/UntamedImport: Mods/Tracks/Untamed/<track>): a custom track on disk, listed
        /// in the browser's own MX vs ATV Untamed section instead of CUSTOM TRACKS.
        public bool Untamed => mod != null && OpenContent.IsUntamed(mod);
        /// A custom track that cannot be ridden (files missing, an archive to extract): listed with the reason, never started.
        public bool Broken => mod != null && !mod.Ok;
        /// What the cards and pages show after the type and the name: the level ("NAT_CONCORD"), a custom track's kind and author.
        public string Where => mod == null ? ev.location.ToUpperInvariant()
            : (Broken ? (mod.archive ? ArchiveBadge(mod) : "BROKEN") : (Untamed ? "UNTAMED  ·  " : "CUSTOM  ·  ") + mod.kindLabel) + (Untamed || string.IsNullOrEmpty(mod.author) ? "" : "  ·  BY " + mod.author.ToUpperInvariant());
        /// A downloaded archive's state (Content/TrackArchives.cs): INSTALLING while the game extracts it, CANNOT INSTALL (the reason in its
        /// description), ARCHIVE before its install starts.
        public static string ArchiveBadge(TrackMod m) => m.archiveState == "installing" ? "INSTALLING" : m.archiveState == "failed" ? "CANNOT INSTALL" : "ARCHIVE";
        /// "NATIONAL  ·  CONCORD", "WAYPOINT RACE  ·  ALPINE  ·  TRACK 2".
        public string Label => typeName + "  ·  " + locationName + (ev.spec.trackId > 0 ? "  ·  TRACK " + (ev.spec.trackId + 1) : "");
    }

    public static class OpenContent
    {
        static FeAssets s_assets;
        static List<OpenEvent> s_events;
        static readonly Dictionary<int, Texture2D> s_icons = new Dictionary<int, Texture2D>();
        public static string Error { get; private set; }

        /// The original front end's assets (strings, icons), read from the install once.
        public static FeAssets Assets
        {
            get
            {
                if (s_assets != null) return s_assets;
                s_assets = new FeAssets(Shader.Find("UI/Default")) { tables = FeTables.Shared };
                return s_assets;
            }
        }

        /// Forgets what was read (the game folder changed).
        public static void Reset()
        {
            s_assets = null; s_events = null; s_base = null; Error = null;
            foreach (var t in s_icons.Values) if (t && t.name.StartsWith("ou_icon")) UnityEngine.Object.Destroy(t);
            foreach (var t in s_backdrops.Values) if (t) UnityEngine.Object.Destroy(t);
            foreach (var t in s_alive.Values) if (t) UnityEngine.Object.Destroy(t);
            s_icons.Clear(); s_backdrops.Clear(); s_sizeLogged.Clear(); s_alive.Clear(); s_iconJobs.Clear();   // (a job still reading finishes unseen)
        }

        /// Localised text of a key, def when the tables do not have it.
        public static string Str(string key, string def = null)
        {
            if (string.IsNullOrEmpty(key)) return def ?? "";
            try { string s = Assets.Str(key); if (!string.IsNullOrEmpty(s) && s != key) return s; } catch (Exception) { }
            return def ?? key;
        }

        // the original's EventSelect order (EventSelect.lua Types), then any other runnable type
        static readonly string[] TypeOrder = { "FreeRide", "National", "Supercross", "Terracross", "Freestyle", "Powercross", "WaypointRace", "SectionRace", "UTVMoto" };
        static readonly Dictionary<string, (string desc, string vehicles)> TypeTexts = new Dictionary<string, (string, string)>
        {
            { "FreeRide", ("STR_FREERIDE_DESCRIPTION", "STR_FREERIDE_VEHICLES") }, { "National", ("STR_NATIONAL_DESCRIPTION", "STR_NATIONAL_VEHICLES") },
            { "Supercross", ("STR_SUPERCROSS_DESCRIPTION", "STR_SUPERCROSS_VEHICLES") }, { "Terracross", ("STR_TERRACROSS_DESCRIPTION", "STR_TERRACROSS_VEHICLES") },
            { "Freestyle", ("STR_FREESTYLE_DESCRIPTION", "STR_FREESTYLE_VEHICLES") }, { "Powercross", ("STR_POWERCROSS_DESCRIPTION", "STR_POWERCROSS_VEHICLES") },
            { "WaypointRace", ("STR_WAYPOINT_DESCRIPTION", "STR_WAYPOINT_VEHICLES") },
        };
        public static int TypeRank(string activity) { int i = Array.IndexOf(TypeOrder, activity); return i < 0 ? TypeOrder.Length : i; }

        /// Every event the port runs, in the original's order (event type, then LocationSelect's order).
        public static IReadOnlyList<OpenEvent> Events
        {
            get
            {
                if (s_events != null && s_customVersion == TrackMods.Version) return s_events;
                if (s_base == null) s_base = BaseEvents();
                var all = new List<OpenEvent>(s_base);
                s_customVersion = TrackMods.Version;
                try { all.AddRange(CustomEvents()); }
                catch (Exception ex) { Debug.LogWarning("[OpenReflex] custom tracks (Mods/Tracks) not listed: " + ex); }
                s_events = all.OrderBy(x => x.order).ToList();
                return s_events;
            }
        }
        static List<OpenEvent> s_base;
        static int s_customVersion = -1;

        /// MODS/TRACKS (Content/TrackMods.cs): the custom tracks of Mods/Tracks - each track's Free Ride and, for a National / Supercross
        /// track, its race - named by the track (mod.ini / its folder), its thumbnail as the picture; a broken track (or a loose archive) as
        /// one entry that says why, in FREE RIDE (and RACE for a race kind), never started. Listed under CUSTOM TRACKS (OpenShell.Browser).
        static List<OpenEvent> CustomEvents()
        {
            var list = new List<OpenEvent>();
            var events = TrackMods.Events;
            int n = 0;
            foreach (var m in TrackMods.All)
            {
                n++;
                string desc = !string.IsNullOrEmpty(m.description) ? m.description
                    : m.Ok ? "A custom track from your Mods/Tracks folder (" + m.kindLabel.ToLowerInvariant() + ")." : "This custom track cannot be ridden: " + m.problem + ".";
                var mine = events.Where(e => e.trackMod == m).ToList();
                if (!m.Ok)
                {   // one entry per mode the track belongs to (a race kind: RACE too), never launched
                    bool races = m.kind == "National" || m.kind == "Supercross";
                    foreach (bool fr in races ? new[] { true, false } : new[] { true })
                    {
                        string act = fr ? "FreeRide" : m.kind;
                        var ev = new ReflexEvents.Event
                        {
                            key = act + "/" + (m.archive ? "archive" : m.level) + "/" + m.Token, typeName = fr ? "Free Ride" : m.kind, location = m.archive ? "" : m.level,
                            locationLocId = -1, eventTypeLocationId = -1, storeProductId = -1, tables = "ModsTracks", trackMod = m,
                            spec = new ReflexWorldFiles.EventSpec { type = act, location = m.archive ? "" : m.level, typeId = -1, locationId = -1, vehicleClass = -1, riders = 1, activity = ReflexEvents.ActivityIndex(act) },
                        };
                        list.Add(CustomEvent(ev, m, desc, n));
                    }
                    continue;
                }
                foreach (var e in mine) list.Add(CustomEvent(e, m, desc, n));
            }
            return list;
        }
        static OpenEvent CustomEvent(ReflexEvents.Event e, TrackMod m, string desc, int n)
        {
            string activity = e.spec.type;
            bool untamed = IsUntamed(m);
            string name = m.name ?? m.level ?? "";
            if (untamed && name.EndsWith(UntamedSuffix, StringComparison.OrdinalIgnoreCase)) name = name.Substring(0, name.Length - UntamedSuffix.Length);   // (its section says it)
            var oe = new OpenEvent
            {
                ev = e, key = e.key, activity = activity, typeId = e.spec.typeId, freeRide = activity == "FreeRide", mod = m,
                typeName = Str("STR_MXACTIVITYTYPE_" + activity.ToUpperInvariant(), (e.typeName ?? activity)).ToUpperInvariant(),
                locationName = name.ToUpperInvariant(), description = desc,
                order = 50000 + n * 10 + (activity == "FreeRide" ? 0 : 1), group = untamed ? UntamedGroup : CustomGroup,
            };
            if (TypeTexts.TryGetValue(activity, out var tt)) { oe.typeDescription = Str(tt.desc, ""); oe.typeVehicles = Str(tt.vehicles, ""); }
            oe.haystack = string.Join(" ", oe.typeName, oe.locationName, m.level, m.id, m.author, m.kindLabel, activity, untamed ? "untamed mx vs atv untamed" : "custom mod mods track", m.Ok ? "" : "broken").ToLowerInvariant();
            return oe;
        }
        /// The browser group of the custom tracks (OpenShell.Browser's CUSTOM TRACKS filter; never an event type of Types).
        public const string CustomGroup = "*custom";
        /// The browser group of the imported MX vs ATV Untamed tracks (OpenShell.Browser's MX vs ATV Untamed filter).
        public const string UntamedGroup = "*untamed";
        const string UntamedSuffix = " (Untamed)";   // the importer's mod.ini name: "<track> (Untamed)" (UxPackage.MenuName)
        /// A custom track in the importer's folder, Mods/Tracks/Untamed/ (UntamedImporter.TracksSubfolder).
        public static bool IsUntamed(TrackMod m) => m?.id != null && m.id.StartsWith(Reflex.Port.Content.UntamedImport.UntamedImporter.TracksSubfolder + "/", StringComparison.OrdinalIgnoreCase);

        /// The install's events (cached until Reset).
        static List<OpenEvent> BaseEvents()
        {
                var list = new List<OpenEvent>();
                try
                {
                    var t = FeTables.Shared;
                    var byType = new Dictionary<int, List<FeTables.Location>>();
                    // the original's events, then a Free Ride on every race level (RESTORATION, w-freerideall): those take the race
                    // event's location row (name, description, pictures) and are listed under the race type of their level
                    foreach (var e in ReflexEvents.All.Concat(ReflexEvents.FreeRideAnyLevel).Concat(ReflexEvents.Restored))
                    {
                        var src = e.freeRideOf ?? e;
                        int typeId = e.spec.typeId, locTypeId = src.spec.typeId;
                        if (!byType.TryGetValue(locTypeId, out var locs)) byType[locTypeId] = locs = t.Locations(locTypeId);
                        int li = locs.FindIndex(l => l.ev == src);
                        var loc = li >= 0 ? locs[li] : null;
                        var type = t.EventType(typeId);
                        string activity = e.spec.type;
                        var oe = new OpenEvent
                        {
                            ev = e, key = e.key, activity = activity, typeId = typeId, freeRide = activity == "FreeRide",
                            typeName = Str("STR_MXACTIVITYTYPE_" + activity.ToUpperInvariant(), (e.typeName ?? activity)).ToUpperInvariant(),
                            typeIcon = FeTables.I(type, "IconId"),
                            dlc = loc != null ? loc.dlc : e.Dlc,
                            order = TypeRank(activity) * 1000 + (li >= 0 ? li : 999),
                        };
                        if (e.freeRideOf != null) { oe.group = src.spec.type; oe.order = (TypeRank(src.spec.type) + 1) * 1000 + (li >= 0 ? li : 999); }
                        if (e.restored)
                        {   // RESTORATION (w-fresnob): a cut level; its Free Ride is listed with the Supercross levels, after the original's
                            if (oe.freeRide) oe.group = "Supercross";
                            oe.order = (TypeRank(oe.freeRide ? "Supercross" : activity) + (oe.freeRide ? 1 : 0)) * 1000 + 900 + e.sort - 20000;
                            oe.locationName = ReflexEvents.RestoredName(e.location);
                            oe.description = e.location == "SX_FresnoB"
                                ? "RESTORED. A supercross layout cut from the original game, rebuilt from your install: its package was shipped incomplete, the missing pieces come from the other arenas. Its race line is lost: Free Ride only."
                                : "RESTORED. A supercross arena cut from the original game: complete in your install, never offered in its menus.";
                        }
                        if (TypeTexts.TryGetValue(activity, out var tt)) { oe.typeDescription = Str(tt.desc, ""); oe.typeVehicles = Str(tt.vehicles, ""); }
                        if (loc != null)
                        {
                            oe.locationName = Str(t.TitleKey(loc.storeProductId), null);
                            oe.description = loc.descriptionLocalizationId != -1 ? Str(t.LocalizationKey(loc.descriptionLocalizationId), "") : "";
                            oe.previewIcon = loc.iconId; oe.mapIcon = loc.mapIconId; oe.posterIcon = loc.posterIconId;
                        }
                        if (string.IsNullOrEmpty(oe.locationName) || oe.locationName.StartsWith("STR_")) oe.locationName = LocationFallback(e.location);
                        oe.locationName = oe.locationName.ToUpperInvariant();
                        oe.haystack = string.Join(" ", oe.typeName, oe.locationName, e.location, e.key, activity, oe.group != null ? GroupName(oe.group) + " " + oe.group : "", oe.dlc ? "dlc" : "", e.restored ? "restored cut" : "").ToLowerInvariant();
                        list.Add(oe);
                    }
                }
                catch (Exception ex) { Error = ex.Message; Debug.LogWarning("[OpenReflex] events not read from the install: " + ex); }
                return list;
        }

        public static OpenEvent Find(string key) => Events.FirstOrDefault(x => string.Equals(x.key, key, StringComparison.OrdinalIgnoreCase));

        /// "NAT_Concord" -> "CONCORD", "SX_FreightYardB" -> "FREIGHT YARD B" (a location the tables do not name).
        static string LocationFallback(string level)
        {
            string alive = AliveLevels.DisplayName(level);
            if (alive != null) return alive;
            string s = level; int us = s.IndexOf('_'); if (us >= 0) s = s.Substring(us + 1);
            var sb = new System.Text.StringBuilder();
            for (int i = 0; i < s.Length; i++) { if (i > 0 && char.IsUpper(s[i]) && char.IsLower(s[i - 1])) sb.Append(' '); sb.Append(s[i]); }
            return sb.ToString();
        }

        /// The event types (filter code, name, count) of a mode, in the original's order. Free Ride: the original's Free Ride maps (and the
        /// Alive levels) first, then the race levels by their race type (OpenEvent.Group: NATIONAL TRACKS, SUPERCROSS ...).
        public static List<(string activity, string name, int count)> Types(bool freeRide) =>
            Events.Where(e => e.freeRide == freeRide && !e.Custom).GroupBy(e => e.Group).OrderBy(g => g.Key == "FreeRide" ? -1 : TypeRank(g.Key))
                  .Select(g => (g.Key, g.First().group != null ? GroupName(g.Key) : freeRide && g.Key == "FreeRide" ? "FREE RIDE MAPS" : g.First().typeName, g.Count())).ToList();

        /// The Free Ride browser's name of a race type's levels.
        public static string GroupName(string raceType)
        {
            switch (raceType)
            {
                case "National": return "NATIONAL TRACKS";
                case "Supercross": return "SUPERCROSS";
                case "Terracross": return "TERRACROSS";
                case "Powercross": return "POWERCROSS";
                case "Freestyle": return "FREESTYLE ARENAS";
                default: return Str("STR_MXACTIVITYTYPE_" + raceType.ToUpperInvariant(), raceType).ToUpperInvariant();
            }
        }

        /// An icon of the install's tables (FeAssets.Icon) as a texture of its own (atlas sprites cut out); null when it has none.
        /// Always a copy with a mip chain and trilinear sampling (the install's textures are read without mipmaps and drawn point-hard when
        /// shrunk); readable, so OpenBackdrop can treat it. A location picture (a streamed texture asset) is made from its decoded pixels (its
        /// read and decode already done on a worker when IconLater asked for it); an atlas sprite is cut out on the GPU as before.
        public static Texture2D Icon(int iconId)
        {
            if (iconId < 0) return null;
            if (s_icons.TryGetValue(iconId, out var cached)) return cached;
            using var perf = Reflex.Port.Platform.ReflexMenuPerf.Measure("picture made");
            Texture2D result = null;
            try
            {
                if (s_iconJobs.TryGetValue(iconId, out var job))
                {   // read on a worker (IconLater): waited for when someone needs it now
                    s_iconJobs.Remove(iconId);
                    var px = job.Result;
                    result = px.rgba != null ? MakeIcon(iconId, px.rgba, px.w, px.h) : null;
                    if (px.error != null) Debug.LogWarning("[OpenReflex] icon " + iconId + ": " + px.error);
                    return s_icons[iconId] = result;
                }
                var asset = Assets.IconStreamAsset(iconId);
                if (asset != null)
                {   // a location picture: its own pixels (no GPU cut-out, no read back: w-menufps)
                    byte[] rgba = FeAssets.DecodeIconAsset(asset, out int pw, out int ph);
                    return s_icons[iconId] = MakeIcon(iconId, rgba, pw, ph);
                }
                var (tex, uv) = Assets.Icon(iconId);
                if (tex)
                {
                    int w = Mathf.Max(1, Mathf.RoundToInt(tex.width * uv.width)), h = Mathf.Max(1, Mathf.RoundToInt(tex.height * uv.height));
                    var rt = RenderTexture.GetTemporary(w, h, 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.sRGB);
                    Graphics.Blit(tex, rt, new Vector2(uv.width, uv.height), new Vector2(uv.x, uv.y));
                    var prev = RenderTexture.active; RenderTexture.active = rt;
                    result = new Texture2D(w, h, TextureFormat.RGBA32, true) { name = "ou_icon" + iconId, wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Trilinear, anisoLevel = 4, hideFlags = HideFlags.DontSave };
                    result.ReadPixels(new Rect(0, 0, w, h), 0, 0); result.Apply(true, false);
                    RenderTexture.active = prev; RenderTexture.ReleaseTemporary(rt);
                }
            }
            catch (Exception e) { Debug.LogWarning("[OpenReflex] icon " + iconId + ": " + e.Message); result = null; }
            return s_icons[iconId] = result;
        }

        // ---- the pictures without a main-thread stall (w-menufps; a tester: "FPS drops when navigating the menus")
        // A location picture was read from its package, decoded, uploaded, drawn into a render texture and read back (ReadPixels waits for the
        // GPU's queued frames) on the main thread: the RACE browser loads its cards' pictures a few a frame, and every card the cursor reaches
        // loads its track map, the first time each. IconLater reads and decodes on a worker; only the texture is made on the main thread.
        struct IconPixels { public byte[] rgba; public int w, h; public string error; }
        static readonly Dictionary<int, System.Threading.Tasks.Task<IconPixels>> s_iconJobs = new Dictionary<int, System.Threading.Tasks.Task<IconPixels>>();

        static Texture2D MakeIcon(int iconId, byte[] rgba, int w, int h)
        {
            // (w-joincrash) pixels decoded on a worker: only a size Unity can take, with exactly its bytes (else no picture, never a bad texture)
            if (rgba == null || w < 1 || h < 1 || w > 8192 || h > 8192 || rgba.Length != w * h * 4) { Debug.LogWarning($"[OpenReflex] icon {iconId}: {w} x {h}, {rgba?.Length ?? 0} bytes: not a picture"); return null; }
            var t = new Texture2D(w, h, TextureFormat.RGBA32, true) { name = "ou_icon" + iconId, wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Trilinear, anisoLevel = 4, hideFlags = HideFlags.DontSave };
            t.SetPixelData(rgba, 0); t.Apply(true, false);   // (the mip chain as Apply makes it; readable)
            return t;
        }

        /// The icon when it is made (as Icon); else null with pending true while its pixels are read on a worker (a later call makes it: a few ms
        /// on the main thread). An icon that is not a streamed texture (an atlas sprite) is made at once, as Icon does.
        public static Texture2D IconLater(int iconId, out bool pending)
        {
            pending = false;
            if (iconId < 0) return null;
            if (s_icons.TryGetValue(iconId, out var cached)) return cached;
            if (s_iconJobs.TryGetValue(iconId, out var job))
            {
                if (!job.IsCompleted) { pending = true; return null; }
                return Icon(iconId);
            }
            var asset = Assets.IconStreamAsset(iconId);
            if (asset == null) return Icon(iconId);
            s_iconJobs[iconId] = System.Threading.Tasks.Task.Run(() =>
            {
                try { var px = FeAssets.DecodeIconAsset(asset, out int w, out int h); return new IconPixels { rgba = px, w = w, h = h }; }
                catch (Exception e) { return new IconPixels { error = e.Message }; }
            });
            pending = true;
            return null;
        }

        /// The event's best picture of its location (the largest landscape picture of its icon rows: PreviewIconId, else the poster), sharp,
        /// for cards and previews. Null when the tables have none.
        public static Texture2D Picture(OpenEvent ev) => Picture(ev, false, out _);

        /// Picture without waiting (w-menufps): null with pending true while its pictures are read on a worker (ask again later).
        public static Texture2D PictureLater(OpenEvent ev, out bool pending) => Picture(ev, true, out pending);

        static Texture2D Picture(OpenEvent ev, bool later, out bool pending)
        {
            pending = false;
            if (ev == null) return null;
            if (ev.mod != null) return TrackMods.Thumbnail(ev.mod);   // a custom track: its own picture (Mods/Tracks), or none
            if (ev.ev != null && AliveLevels.Is(ev.ev.location)) return AlivePreview(ev.ev.location);   // an Alive level: Alive's own picture
            Texture2D best = null;
            if (later)
            {   // both rows read (on workers, side by side) before the choice: the choice is Picture's
                IconLater(ev.previewIcon, out bool p0); IconLater(ev.posterIcon, out bool p1);
                if (p0 || p1) { pending = true; return null; }
            }
            foreach (int id in new[] { ev.previewIcon, ev.posterIcon })
            {
                var t = Icon(id);
                if (t && t.width >= t.height && (best == null || t.width * t.height > best.width * best.height)) best = t;
            }
            return best ?? Icon(ev.previewIcon) ?? Icon(ev.posterIcon);
        }

        /// The event's track map (the browser preview's and the setup page's corner): the location's MapIconId; an Alive level's own map
        /// (AlivePicture). Null when there is none.
        public static Texture2D MapPicture(OpenEvent ev)
        {
            if (ev == null) return null;
            if (ev.mod == null && ev.ev != null && AliveLevels.Is(ev.ev.location)) return AlivePicture(ev.ev.location, true);
            return Icon(ev.mapIcon);
        }

        /// MapPicture without waiting (w-menufps): null with pending true while it is read on a worker.
        public static Texture2D MapPictureLater(OpenEvent ev, out bool pending)
        {
            pending = false;
            if (ev == null) return null;
            if (ev.mod == null && ev.ev != null && AliveLevels.Is(ev.ev.location)) return AlivePicture(ev.ev.location, true);
            return IconLater(ev.mapIcon, out pending);
        }

        // ------------------------------------------------------------------------------------------------ MX vs ATV Alive pictures

        static readonly Dictionary<string, Texture2D> s_alive = new Dictionary<string, Texture2D>(StringComparer.OrdinalIgnoreCase);

        /// An Alive level's own picture in the install (AliveLevels.Entry preview / map: the Database sidecars the importer writes from the
        /// player's disc / level packs, Content/AliveImport/AlivePictures.cs): map false, Alive's loading picture of the track (LP_*, 1024 x 256);
        /// map true, its track select map (LS_*, the layout cut out) or Free Ride map (FRMap_*, an overhead of the terrain). PC texture v4 as
        /// the install's UI textures (ReflexUiData.DecodeTexture); returned as Icon returns the tables' pictures (a readable copy with
        /// mipmaps and trilinear sampling). Null when the level has none (imported before the pictures existed: FIND LEVELS adds them).
        public static Texture2D AlivePicture(string level, bool map)
        {
            var e = AliveLevels.Get(level);
            string path = e == null ? null : AliveLevels.Sidecar(map ? e.map : e.preview);
            if (path == null) return null;
            if (s_alive.TryGetValue(path, out var cached)) return cached;
            Texture2D t = null;
            try
            {
                byte[] rgba = ReflexUiData.DecodeTexture(RxFiles.ReadAllBytes(path), out int w, out int h, out _);   // (packed with its level: RxFiles)
                var flipped = new byte[rgba.Length]; int row = w * 4;   // rows top first -> Unity's bottom first
                for (int y = 0; y < h; y++) Buffer.BlockCopy(rgba, y * row, flipped, (h - 1 - y) * row, row);
                t = new Texture2D(w, h, TextureFormat.RGBA32, true) { name = "ou_alive_" + e.level + (map ? "_map" : "_preview"), wrapMode = TextureWrapMode.Clamp,
                                                                      filterMode = FilterMode.Trilinear, anisoLevel = 4, hideFlags = HideFlags.DontSave };
                t.SetPixelData(flipped, 0); t.Apply(true, false);
            }
            catch (Exception ex) { Debug.LogWarning("[OpenReflex] Alive picture " + path + ": " + ex.Message); t = null; }
            return s_alive[path] = t;
        }

        /// An Alive level's loading picture; for a level without one, the picture of another installed Alive level of the same place
        /// (AlivePlace), else null.
        static Texture2D AlivePreview(string level)
        {
            var t = AlivePicture(level, false);
            if (t) return t;
            string place = AlivePlace(level);
            if (place == null) return null;
            foreach (var e in AliveLevels.All)
                if (!string.Equals(e.level, level, StringComparison.OrdinalIgnoreCase) && AlivePlace(e.level) == place && (t = AlivePicture(e.level, false))) return t;
            return null;
        }

        /// The place of an Alive level: NAT_Farm_B / ST_Farm -> Farm (the disc's Nationals and Short tracks share Farm, Northwest, Southwest,
        /// Tropic), NAT_DLC_JSComp_A / FR_DLC_JSComp -> JSComp, NAT_DLC_Quarry_B -> Quarry, SX_DLC_SX02 -> SX; the disc's Free Ride levels
        /// (Dunes, Bryn Carreg Quarry) are places of their own (null).
        static string AlivePlace(string level)
        {
            string s = level ?? "";
            bool freeRide = s.StartsWith("FR_", StringComparison.Ordinal);
            foreach (var p in new[] { "NAT_", "ST_", "SX_", "FR_" })
                if (s.StartsWith(p, StringComparison.Ordinal)) { s = s.Substring(p.Length); break; }
            if (s.StartsWith("DLC_", StringComparison.Ordinal)) s = s.Substring(4);
            else if (freeRide) return null;
            if (s.Length > 2 && s[s.Length - 2] == '_') s = s.Substring(0, s.Length - 2);
            if (s.StartsWith("SX", StringComparison.Ordinal)) s = "SX";
            return s.Length > 0 ? s : null;
        }

        static readonly Dictionary<(int, bool), Texture2D> s_backdrops = new Dictionary<(int, bool), Texture2D>();
        static readonly HashSet<int> s_sizeLogged = new HashSet<int>();

        /// The event's location picture for a full-screen backdrop (hub fallback, loading, credits): its best picture, and when that is
        /// narrower than 0.6 x the screen, the softened / graded / vignetted copy of OpenBackdrop.Treat (made once per picture and screen class,
        /// then cached). Null when the tables have no picture. Set it with OpenBackdrop.Apply to get the slow drift.
        public static Texture2D Backdrop(OpenEvent ev)
        {
            var src = Picture(ev);
            if (!src) return null;
            int id = src.name.StartsWith("ou_icon") && int.TryParse(src.name.Substring(7), out int n) ? n : src.name.GetHashCode();
            bool soft = OpenBackdrop.NeedsTreatment(src.width, Screen.width);
            if (s_sizeLogged.Add(id)) Debug.Log($"[OpenReflex] backdrop {ev.key}: icon {id} {src.width}x{src.height}, screen {Screen.width}x{Screen.height} -> {(soft ? "soft treatment" : "sharp")}");
            if (!soft) return src;
            if (s_backdrops.TryGetValue((id, true), out var made) && made) return made;
            var t = OpenBackdrop.Treat(src, "ou_backdrop" + id);
            return s_backdrops[(id, true)] = t;
        }

        /// The vehicle type's name (VehicleType.Name, "STR_VEHICLE_ATV" -> ATV).
        public static string VehicleTypeName(int typeId)
        {
            var r = FeTables.Shared.Rows("VehicleType").FirstOrDefault(x => FeTables.I(x, "VehicleTypeId") == typeId);
            string s = Str(FeTables.S(r, "Name"), null);
            if (string.IsNullOrEmpty(s) || s.StartsWith("STR_")) s = typeId switch { 2 => "MX LITE", 3 => "MX", 4 => "ATV", 11 => "UTV", 12 => "SUPER BUGGY", 14 => "PRO LITE", 15 => "PRO 2", _ => "TYPE " + typeId };
            return s.ToUpperInvariant();
        }

        /// A vehicle class's name (VehicleClass.Name).
        public static string ClassName(int classId)
        {
            string k = ReflexEvents.ClassName(classId);
            string s = Str(k, null);
            if (string.IsNullOrEmpty(s) || s.StartsWith("STR_")) s = (k ?? classId.ToString()).Replace("STR_VEHICLECLASS_", "").Replace('_', ' ');
            return s.ToUpperInvariant();
        }

        public static string AiName(int ai) => ai <= 0 ? Str("STR_AI_NONE", "OFF").ToUpperInvariant() : Str("STR_AI_LEVEL" + ai, ReflexWorldFiles.AiSkills[ai - 1].name).ToUpperInvariant();
    }

    /// One event's setup (LocationSelect + Vehicle Select) and its launch.
    [Serializable]
    public sealed class OpenSetup
    {
        public string eventKey;
        /// 0 off, 1..4 = ReflexWorldFiles.AiSkills (LocationSelect's default: AMATEUR).
        public int ai = 2;
        /// RIDER CAP (w-rc-client): FIELD SIZE, the riders with the AI on (you included): 12 = the original's full gate (the default); up to
        /// FieldCap(e) (RxRiderCap.Max for the lap races). A core from before the big field loads 12.
        public int riders = Reflex.Port.RxCore.RxRiderCap.Original;
        /// The Event Duration's index in EventTypeDurations (-1: LocationSelect's default, the second when there are two or more).
        public int duration = -1;
        /// The vehicle type (VehicleTypeId; -1: the event's own, the profile's MX Lite or the machine-only event's machine).
        public int vehicleType = -1;
        /// The class's index in LocationSelect's list for the type (0: its first, the default).
        public int vehicleClass;
        /// The rider: -1 the player's own (the profile's), else an AIRiderId of Vehicle Select's pros.
        public int rider = -1;
        public bool vehicleChoiceExplicit;
        /// PRACTICE (PLAY > PRACTICE): this setup launches the event as a practice session (an endless lap race,
        /// you alone). Not part of the saved setup: the PRACTICE page keeps its own (OpenHistory.PracticeSetupOf).
        [NonSerialized] public bool practice;
        /// WEATHER and WEATHER INTENSITY of this ride (Rendering/ReflexWeather.cs; the EVENT tab's rows): -1 = the setup default (the last pick on
        /// any setup page, ReflexWeather.DefaultSetting / DefaultIntensity), else 0 CLEAR, 1 RAIN, 2 SNOW, 3 DYNAMIC and 0 LIGHT, 1 MEDIUM, 2 HEAVY.
        /// PRACTICE always rides CLEAR.
        public int weather = -1, weatherIntensity = -1;
        public int Weather => practice ? 0 : weather >= 0 ? Mathf.Clamp(weather, 0, 3) : Reflex.Port.Rendering.ReflexWeather.DefaultSetting;
        public int WeatherIntensity => weatherIntensity >= 0 ? Mathf.Clamp(weatherIntensity, 0, 2) : Reflex.Port.Rendering.ReflexWeather.DefaultIntensity;

        public OpenSetup Clone() => (OpenSetup)MemberwiseClone();

        /// MAP SCALE (core/plan/MAP_SCALE.md): the event's track's scale in percent, kept per track (the location) in the client's settings,
        /// not in this setup (ReflexSettings.MapScalePercent). Every arcade event of the shell (races and Free Ride) has it.
        public static bool HasMapScale(ReflexEvents.Event e) => e != null && !string.IsNullOrEmpty(e.location);
        /// BASE SCALE (w-alivegates): the percent is relative to the track's default scale (MapScaleBase: an Alive race level's 110 / 130 %),
        /// 100 % = that default; the saved value is kept under the base it is relative to ("NAT_Farm@110"), so a later change of the default
        /// starts the track at its new default instead of carrying an old percent over (the values saved before the base, plain "NAT_Farm",
        /// were of the original's size and are not read for such a track).
        public static int MapScalePercent(ReflexEvents.Event e) => e?.trackMod?.experimentalTerrain == true ? ExperimentalMtxScale.PercentFor(e.trackMod) : HasMapScale(e) ? Reflex.Port.Options.ReflexSettings.MapScalePercent(Place(e)) : Reflex.Port.RxCore.RxMapScale.DefaultPercent;
        public static void SetMapScalePercent(ReflexEvents.Event e, int percent) { if (e?.trackMod?.experimentalTerrain == true) ExperimentalMtxScale.SetPercentFor(e.trackMod, percent); else if (HasMapScale(e)) Reflex.Port.Options.ReflexSettings.SetMapScalePercent(Place(e), percent); }
        /// The track's default scale (1 for the game's tracks and custom tracks; an Alive race level's base: AliveLevels.MapScaleBase).
        public static float MapScaleBase(ReflexEvents.Event e) => e == null || e.trackMod != null || string.IsNullOrEmpty(e.location) ? 1f : AliveLevels.MapScaleBase(e.location);
        /// The world scale of an event at its setup row's percent.
        public static float MapScaleOf(ReflexEvents.Event e) => Reflex.Port.RxCore.RxMapScale.FromPercent(MapScalePercent(e), MapScaleBase(e));
        /// The track a setting is kept for: the level, a custom track's "@<its id>" (custom tracks share their slot's level name); a track with a
        /// default scale of its own: "<level>@<the default in percent>".
        static string Place(ReflexEvents.Event e)
        {
            if (e.trackMod != null) return "@" + e.trackMod.id;
            int b = e.location != null ? AliveLevels.MapScaleBasePercent(e.location) : 100;
            return b == 100 ? e.location : e.location + "@" + b;
        }
        public string ToJson() => JsonUtility.ToJson(this);
        public static OpenSetup FromJson(string json) { try { return string.IsNullOrEmpty(json) ? null : JsonUtility.FromJson<OpenSetup>(json); } catch (Exception) { return null; } }

        public ReflexEvents.Event Event => string.IsNullOrEmpty(eventKey) ? null : ReflexEvents.Find(eventKey);
        public bool Rules(ReflexEvents.Event e) => e != null && !ReflexEvents.RulelessActivities.Contains(e.spec.type);

        // ---------------------------------------------------------------------------------------- the event's lists
        /// EventTypeDurations of the event's type (LocationSelect's list) and whether they are laps.
        public List<int> Durations(ReflexEvents.Event e, out bool laps, out string singular, out string plural)
        {
            laps = false; singular = plural = null;
            if (!Rules(e)) return new List<int>();
            var d = FeTables.Shared.Durations(e.spec.typeId, out singular, out plural, out string type) ?? new List<int>();
            laps = type == "Laps";
            return d;
        }
        public int DurationIndex(List<int> d) => d.Count == 0 ? -1 : duration >= 0 && duration < d.Count ? duration : d.Count > 1 ? 1 : 0;
        /// The race's laps (0 for a type without laps; the setup line shows them).
        public int Laps(ReflexEvents.Event e)
        {
            var d = Durations(e, out bool laps, out _, out _);
            int i = DurationIndex(d);
            return laps && i >= 0 ? d[i] : 0;
        }
        /// The chosen Event Duration for ReflexEventSelect.Launch, whatever its unit (laps, Freestyle's minutes ...): LocationSelect.lua's
        /// OnEventDurationListFocusedChanged sets App.SetUIVariableInt( "EventDuration", value ) for every type with a duration list, and
        /// SetupEvent reads it with the type's EventDurationType. 0: the event's own (a type without a list).
        public int EventDuration(ReflexEvents.Event e)
        {
            var d = Durations(e, out _, out _, out _);
            int i = DurationIndex(d);
            return i >= 0 ? d[i] : 0;
        }

        /// The vehicle types the event's raceable classes have (Vehicle Select's order), each with a rider to race it.
        public static List<int> VehicleTypes(ReflexEvents.Event e)
        {
            var o = new List<int>();
            if (e == null) return o;
            var types = e.vehicleClasses.SelectMany(ReflexAtv.TypesOfClass).Distinct().ToList();
            if (types.Count == 0 && ReflexEvents.RulelessActivities.Contains(e.spec.type)) types = ReflexEvents.DrawableVehicleTypes.ToList();   // free ride: any vehicle
            foreach (int t in ReflexEvents.DrawableVehicleTypes)
                if (types.Contains(t) && Riders(e, t).Count > 0) o.Add(t);
            return o;
        }

        static bool Core => Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex;
        static ProfileData Profile()
        {
            if (!Core) return null;
            if (ReflexProfile.Current == null) ReflexProfile.Open();
            // Core race startup reads this same garage overlay, including isolated smoke saves.
            return ReflexProfile.Current ?? CoreGarageProfile.Capture();
        }
        static int ProfileType()
        {
            var p = Profile(); if (p == null) return ReflexEvents.DefaultVehicleType;
            int vehicle = p.T("PlayerRider").Int("LastEquippedVehicle", 0), riderId = p.T("PlayerRider").Int("LastRiderId", 0, 0);
            // (the tables' rows by an index, not a scan: the setup pages ask this many times a card; w-menufps)
            if (riderId > 0) return FeTables.I(RowIndex.All(FeTables.Shared.Rows("AIRiderVehicle"), "AIRiderId", riderId).FirstOrDefault(x => FeTables.I(x, "AIVehicleId") == vehicle), "VehicleTypeId");
            var pv = p.T("PlayerVehicle"); int model = pv.Int("VehicleModelId", pv.Find("PlayerVehicleId", vehicle));
            return FeTables.I(RowIndex.First(FeTables.Shared.Rows("VehicleModel"), "VehicleModelId", model), "VehicleTypeId");
        }
        static int OwnedVehicle(int type)
        {
            var p = Profile(); if (p == null) return -1;
            var pv = p.T("PlayerVehicle"); int selected = p.T("PlayerRider").Int("LastEquippedVehicle", 0);
            var models = FeTables.Shared.Rows("VehicleModel");
            var rows = Enumerable.Range(0, pv.rows).Where(r => RowIndex.All(models, "VehicleModelId", pv.Int("VehicleModelId", r)).Any(m => FeTables.I(m, "VehicleTypeId") == type));
            return rows.Select(r => pv.Int("PlayerVehicleId", r)).OrderBy(id => id == selected ? 0 : 1).DefaultIfEmpty(-1).First();
        }
        bool ExplicitChoice => vehicleChoiceExplicit || vehicleType >= 0 || rider >= 0;

        /// The default vehicle type of the event (the profile's MX Lite; a machine-only event: its first machine).
        public static int DefaultType(ReflexEvents.Event e)
        {
            if (e == null) return ReflexEvents.DefaultVehicleType;
            if (Core)
            {
                int saved = ProfileType();
                if (!e.MachineOnly || e.ClassesFor(saved).Length > 0) return saved;
            }
            if (!e.MachineOnly) return ReflexEvents.DefaultVehicleType;
            return e.GarageFor(ReflexVehicles.TypeUtv) != null ? ReflexVehicles.TypeUtv : ReflexVehicles.TypeSuperBuggy;
        }
        public int Type(ReflexEvents.Event e)
        {
            var ts = VehicleTypes(e);
            if (vehicleType >= 0 && ts.Contains(vehicleType)) return vehicleType;
            int d = DefaultType(e);
            return ts.Contains(d) || ts.Count == 0 ? d : ts[0];
        }
        /// LocationSelect's classes for the type (the first is its default).
        public int[] Classes(ReflexEvents.Event e) => e == null || !Rules(e) ? new int[0] : MixedFirst(e, e.ClassesFor(Type(e)));

        /// OMNICROSS (Terracross) is the original's every-vehicle race (STR_TERRACROSS_VEHICLES: ATV, MX, MX Lite, UTV, Sport Truck, Sport
        /// Buggy, Sport 2 Truck), but LocationSelect's first class for a vehicle is its own Single class, so the field is all the player's
        /// type until the CLASS is changed. The OPEN front end lists the widest Multi class (17 Unlimited: every type) first for this event
        /// type, so the default field is the game's mixed one (MXUIManager_AddAIRiderOfClass draws each AI's type from the class's rows);
        /// the Single class and the others stay in the CLASS list. Every other event type keeps LocationSelect's order.
        public static int[] MixedFirst(ReflexEvents.Event e, int[] classes)
        {
            if (e == null || e.spec.type != "Terracross" || classes.Length < 2) return classes;
            int best = -1, bestTypes = 0;
            foreach (int c in classes)
            {
                if (ReflexEvents.ClassFormat(c) != "Multi") continue;
                int n = ReflexAtv.TypesOfClass(c).Length;
                if (n > bestTypes) { best = c; bestTypes = n; }
            }
            if (best < 0 || classes[0] == best) return classes;
            return new[] { best }.Concat(classes.Where(c => c != best)).ToArray();
        }
        public int Class(ReflexEvents.Event e) { var cs = Classes(e); return cs.Length == 0 ? -1 : cs[Mathf.Clamp(vehicleClass, 0, cs.Length - 1)]; }

        /// The riders of a type: the player's own (-1) on the profile's type, then Vehicle Select's pros with a vehicle of the type.
        public static List<(int id, string name)> Riders(ReflexEvents.Event e, int type)
        {
            var o = new List<(int, string)>();
            if (Core ? OwnedVehicle(type) >= 0 : type == ReflexEvents.DefaultVehicleType && (e == null || !e.MachineOnly)) o.Add((-1, "YOUR RIDER"));
            var t = FeTables.Shared;
            var veh = t.Rows("AIRiderVehicle");
            foreach (var r in t.Rows("AIRider").Where(r => FeTables.I(r, "PlayerSelectable", 0) != 0).OrderBy(r => FeTables.S(r, "DisplayName") ?? "", StringComparer.Ordinal))
            {
                int id = FeTables.I(r, "AIRiderId");
                if (RowIndex.All(veh, "AIRiderId", id).Any(v => FeTables.I(v, "VehicleTypeId") == type))
                    o.Add((id, (FeTables.S(r, "DisplayName") ?? FeTables.S(r, "Name") ?? ("RIDER " + id)).ToUpperInvariant()));
            }
            return o;
        }
        public int Rider(ReflexEvents.Event e)
        {
            var rs = Riders(e, Type(e));
            if (Core && !ExplicitChoice)
            {
                int saved = Profile()?.T("PlayerRider").Int("LastRiderId", 0, 0) ?? 0;
                return saved > 0 && rs.Any(x => x.id == saved) ? saved : -1;
            }
            if (rs.Any(r => r.id == rider)) return rider;
            return rs.Count > 0 ? rs[0].id : -1;
        }

        /// The Vehicle Select choice and the class for the race's world (ReflexWorldFiles.Garage; null: the profile's rider and bike in
        /// the event's own class), as LocationSelect:LaunchGarage hands it over.
        public ReflexWorldFiles.Garage Garage(ReflexEvents.Event e)
        {
            if (e == null) return null;
            int type = Type(e), cls = Class(e), who = Rider(e);
            if (!Rules(e)) cls = -1;
            if (Core && !ExplicitChoice)
            {
                // A class-only choice preserves the saved rider, model, parts and tuning.
                // Keep it non-null for machine events so their legacy pro-rider fallback cannot replace it.
                if (e.MachineOnly && type != ProfileType()) return e.GarageFor(type, cls);
                return cls >= 0 && (e.MachineOnly || cls != e.spec.vehicleClass) ? new ReflexWorldFiles.Garage { vehicleClass = cls, vehicleTypeId = type } : null;
            }
            if (Core && who < 0)
            {
                int own = OwnedVehicle(type);
                if (own >= 0) return new ReflexWorldFiles.Garage { riderOwner = ReflexWorldFiles.Garage.Player1, riderId = 0, vehicleOwner = ReflexWorldFiles.Garage.Player1, vehicleId = own, vehicleTypeId = type, vehicleClass = cls };
            }
            if (who < 0)
            {   // the profile's own rider on its MX Lite: only a class other than the event's own changes the world
                return cls >= 0 && cls != e.spec.vehicleClass ? new ReflexWorldFiles.Garage { vehicleClass = cls } : null;
            }
            var t = FeTables.Shared;
            var r = RowIndex.First(t.Rows("AIRider"), "AIRiderId", who);
            var v = RowIndex.All(t.Rows("AIRiderVehicle"), "AIRiderId", who).FirstOrDefault(x => FeTables.I(x, "VehicleTypeId") == type);
            if (r == null || v == null) return e.GarageFor(type, cls);
            return new ReflexWorldFiles.Garage
            {
                riderOwner = ReflexWorldFiles.Garage.AI, riderId = who, riderGearSet = FeTables.I(r, "DefaultRiderGearSetId", -1),
                vehicleOwner = ReflexWorldFiles.Garage.AI, vehicleId = FeTables.I(v, "AIVehicleId", -1), vehicleTypeId = type,
                vehicleClass = !Rules(e) ? -1 : cls >= 0 ? cls : ReflexAtv.ClassFor(e.spec.typeId, type),   // LaunchGarage(rulesAvailable ? VehicleClass : -1)
            };
        }

        public int Ai(ReflexEvents.Event e) => Rules(e) && !practice ? Mathf.Clamp(ai, 0, ReflexWorldFiles.AiSkills.Length) : 0;   // (practice: you alone)
        /// FIELD SIZE as the launch takes it (the riders with the AI on, you included): the choice within the event's cap (OpenFieldSize);
        /// 12 (the original's full gate) when unset or out of range.
        public int Riders(ReflexEvents.Event e) => OpenFieldSize.Clamp(e, riders);
        /// The riders this setup's launch asks for: FIELD SIZE. A championship weekend's stages take it too (the weekend page's FIELD SIZE row
        /// edits this setup's `riders`; every stage is launched, and restarted in game, with the same field: up to 32 for National /
        /// Supercross, the original's 12 for Waypoint / Freestyle, OpenFieldSize.Cap; the Supercross split follows the field, the core's
        /// champ_split). Before the field size a stage kept the original's 12 whatever the setup said.
        public int LaunchRiders(ReflexEvents.Event e) => Riders(e);
        /// The most riders an event offers (OpenFieldSize.Cap).
        public static int FieldCap(ReflexEvents.Event e) => OpenFieldSize.Cap(e);
        public bool CrowdedSupercross(ReflexEvents.Event e) => Ai(e) > 0 && OpenFieldSize.Crowded(e, riders);

        /// The world this setup races (built once from the install, then cached).
        public ReflexWorldFiles World(ReflexEvents.Event e)
        {
            int a = Ai(e);
            // (the legacy plugin's world: its game has 12 seats, so a bigger field's world is the full gate's; the open core needs no world file)
            return ReflexWorldFiles.For(e.key, a > 0 ? ReflexWorldFiles.OpponentsFor(Math.Min(Riders(e), Reflex.Port.RxCore.RxRiderCap.Original)) : 0, a > 0 ? ReflexWorldFiles.AiSkills[a - 1].skill : ReflexWorldFiles.AiSkillNormal, Laps(e), Garage(e));
        }

        /// Tests / bench: replaces the hand-over to the race scene (like FrontEndManager.LaunchOverride).
        public static Action<OpenSetup> LaunchOverride;

        /// LocationSelect:OnAccept's hand-over, as FrontEndManager.DoLaunch makes it: ReflexEventSelect.Launch(key, ai, remember, "Reflex",
        /// the Event Duration, garage).
        public void Launch()
        {
            var e = Event; if (e == null) return;
            if (LaunchOverride != null) { LaunchOverride(this); return; }
            var g = Garage(e);
            Debug.Log("[OpenReflex] launching " + e.key + " AI " + Ai(e) + (Ai(e) > 0 ? ", riders " + LaunchRiders(e) : "") + ", duration " + EventDuration(e) + " " + e.spec.durationType + (g != null ? ", vehicle select: " + g : ""));
            int ms = MapScalePercent(e);
            float mapBase = MapScaleBase(e);   // BASE SCALE: the track's default (an Alive race level's); the row's percent is relative to it
            if (ms != Reflex.Port.RxCore.RxMapScale.DefaultPercent) Debug.Log($"[OpenReflex] MAP SCALE {ms} % of the track's default ({e.location}{(mapBase != 1f ? ", its default " + Reflex.Port.RxCore.RxMapScale.Percent(mapBase) + " %" : "")}): the track is scaled; offline records, best laps and highlights are not kept for this event");
            else if (mapBase != 1f) Debug.Log($"[OpenReflex] MAP SCALE: {e.location} rides at its default, {Reflex.Port.RxCore.RxMapScale.Percent(mapBase)} % of the original (records kept)");
            if (practice) Debug.Log("[OpenReflex] PRACTICE: an endless lap race, you alone");
            ReflexEventSelect.Launch(e.key, Ai(e), true, "Reflex", EventDuration(e), g, Reflex.Port.RxCore.RxMapScale.FromPercent(ms, mapBase), practice, LaunchRiders(e), Weather, WeatherIntensity);
        }
    }

    /// Favourites (locations), recently ridden events with their setups, the browser's view and sort (PlayerPrefs OpenReflex.*).
    public static class OpenHistory
    {
        const string KFavourites = OpenReflex.Prefs + "Favourites", KRecent = OpenReflex.Prefs + "Recent", KView = OpenReflex.Prefs + "View",
                     KSort = OpenReflex.Prefs + "Sort", KSetups = OpenReflex.Prefs + "Setup.", KPractice = KSetups + "practice:";
        public const int MaxRecent = 12;

        static HashSet<string> s_fav;
        static HashSet<string> Favs => s_fav ??= new HashSet<string>(PlayerPrefs.GetString(KFavourites, "").Split(new[] { '\n' }, StringSplitOptions.RemoveEmptyEntries), StringComparer.OrdinalIgnoreCase);
        public static void ResetCache() => s_fav = null;

        public static bool IsFavourite(string location) => !string.IsNullOrEmpty(location) && Favs.Contains(location);
        public static int FavouriteCount => Favs.Count;
        public static bool ToggleFavourite(string location)
        {
            if (string.IsNullOrEmpty(location)) return false;
            bool on = !Favs.Remove(location);
            if (on) Favs.Add(location);
            PlayerPrefs.SetString(KFavourites, string.Join("\n", Favs)); PlayerPrefs.Save();
            return on;
        }

        /// The recently started events, newest first (each with the setup it was started with).
        public static List<OpenSetup> Recent()
        {
            var o = new List<OpenSetup>();
            foreach (var line in PlayerPrefs.GetString(KRecent, "").Split(new[] { '\n' }, StringSplitOptions.RemoveEmptyEntries))
            { var s = OpenSetup.FromJson(line); if (s != null && !string.IsNullOrEmpty(s.eventKey)) o.Add(s); }
            return o;
        }
        public static void Remember(OpenSetup s)
        {
            if (s == null || string.IsNullOrEmpty(s.eventKey)) return;
            var list = Recent();
            list.RemoveAll(x => string.Equals(x.eventKey, s.eventKey, StringComparison.OrdinalIgnoreCase));
            list.Insert(0, s.Clone());
            if (list.Count > MaxRecent) list.RemoveRange(MaxRecent, list.Count - MaxRecent);
            PlayerPrefs.SetString(KRecent, string.Join("\n", list.Select(x => x.ToJson())));
            SaveSetup(s);
        }

        /// The last setup of an event (its own choices kept per event), else a new one.
        public static OpenSetup SetupOf(string eventKey) => OpenSetup.FromJson(PlayerPrefs.GetString(KSetups + eventKey, "")) ?? new OpenSetup { eventKey = eventKey };
        public static void SaveSetup(OpenSetup s) { if (s?.eventKey == null) return; PlayerPrefs.SetString((s.practice ? KPractice : KSetups) + s.eventKey, s.ToJson()); PlayerPrefs.Save(); }
        /// PRACTICE's own setup of an event (OpenReflex.Setup.practice:<key>: its vehicle and rider choices, apart from the race's).
        public static OpenSetup PracticeSetupOf(string eventKey)
        {
            var s = OpenSetup.FromJson(PlayerPrefs.GetString(KPractice + eventKey, "")) ?? new OpenSetup { eventKey = eventKey };
            s.eventKey = eventKey; s.practice = true;
            return s;
        }

        /// 0 grid, 1 list.
        public static int View { get => PlayerPrefs.GetInt(KView, 0); set { PlayerPrefs.SetInt(KView, value); PlayerPrefs.Save(); } }
        /// 0 game order, 1 A-Z, 2 favourites first.
        public static int Sort { get => Mathf.Clamp(PlayerPrefs.GetInt(KSort, 0), 0, 2); set { PlayerPrefs.SetInt(KSort, value); PlayerPrefs.Save(); } }

        /// Every OpenReflex.* key this class writes (tests save and restore them).
        public static IEnumerable<string> Keys() => new[] { KFavourites, KRecent, KView, KSort };
    }
}
