// The open MOTOCARD's data (OpenShell.Motocard.cs shows it): what the original's motocard pages read, read the same way from the game's tables
// (FeTables.Shared: Highlight, Series, EventType, EventTypeLocations, LeaderboardEventTypeVehicleTypes, VehicleType ...) and the profile
// (ReflexProfile.Current: PlayerRider, PlayerHighlight, PlayerRecords, PlayerSeriesStatus ...), read only. Nothing here writes or draws.
//
// Where each part is from (the original's Lua is in _re/formats/out/ui/, the strings in the install's FrontEnd / Shared string tables):
//   the card      Motocard.lua PopulateLocalInformation: the profile name, PlayerRider.Number, App.CalculateCareerCompletion * 100 (floored),
//                 the account balance, the seven latest PlayerHighlight rows by UnlockedOrder (icons "Highlight##").
//   highlights    MotocardHighlights.lua: every Highlight row (HighlightId order) with the player's PlayerHighlight progress, reached when
//                 GoalProgress == Goal, sorted by HighlightSort (unlocked first, newest unlock first, then locked by progress, then id).
//   records       MotocardRecords.lua (offline: GenerateLocalRecords): the activity types of LeaderboardEventTypeVehicleTypes in
//                 MotocardRecords_ActivityTypeSort order, each one's EventTypeLocations, the PlayerRecords rows of the focused location
//                 sorted by BestLapTime / BestScore / TotalTime (ascending, as the Lua), values by App.FormatTime / FormatNumber.
//   tricks        StuntPreview.lua: the four tiers' stunts (CoreStuntCatalog = the StuntTable asset the original reads), a duplicate
//                 display name listed once, the name from MXRaven_Stunts and the gesture picture "Stunt_<key without STR_>".
// What the original's motocard does NOT have (so this port does not invent it): events played / won, podiums, distance, air time ... as
// statistics rows. The original's local motocard shows name, number, career progress, credits and recent highlights; the network upload
// (the PlayerMinedStat table, FUN_00754430) is online only. The "record totals" of OpenShell.Motocard.cs are a PORT EXTRA: sums of the
// PlayerRecords columns the core already keeps (counts only, no units the original never shows).
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEngine;

namespace Reflex.Port.OpenFrontEnd
{
    /// One of the 48 highlights as the highlights page lists it.
    internal sealed class MotocardHighlight
    {
        public int id, progress, goal, order;
        public string title, description, category;
        public bool unlocked;
        /// "9/10" for a goal above one (the page's Progress text), else "".
        public string ProgressText => goal > 1 ? progress + "/" + goal : "";
        public float Fraction => goal <= 0 ? 0f : Mathf.Clamp01(progress / (float)goal);
    }

    /// An activity type of the offline leaderboards (a row of the page's ActivityTypeList).
    internal sealed class MotocardRecordType
    {
        public int eventTypeId; public string activity, name, header;
        /// Which column the page ranks by: 0 BestLapTime, 1 BestScore, 2 TotalTime (GenerateLocalRecords).
        public int column;
    }

    /// A location of an activity type (a row of the LocationList).
    internal sealed class MotocardRecordLocation
    {
        public int eventTypeLocationId, product, posterIcon, eventLocationId, trackId;
        public string name;
    }

    /// One line of a location's offline leaderboard: the rank, the profile's name, the vehicle type and the formatted record.
    internal sealed class MotocardRecordRow
    {
        public int index, vehicleType; public string name, vehicle, value;
    }

    /// A stunt of the trick library.
    internal sealed class MotocardStunt
    {
        public int tier; public string name, key, display, material;
        /// The StuntTable's name as written (its performance: "<MX|ATV>Rider_<anim>").
        public string anim;
    }

    internal static class OpenMotocardData
    {
        static FeTables T => FeTables.Shared;
        public static ProfileData P => ReflexProfile.Current;
        static string Num(int v) => FeGarageData.Number(v);

        // ------------------------------------------------------------------------------------------------ strings

        static Dictionary<string, string> s_highlights, s_stunts;
        static Dictionary<string, string> HighlightStrings => s_highlights ??= Table("Shared", "MXRaven_Highlights");
        static Dictionary<string, string> StuntStrings => s_stunts ??= Table("FrontEnd", "MXRaven_Stunts");
        static Dictionary<string, string> Table(string package, string table)
        {
            try { return OriginalUiAssets.Data.Strings(package, table); }
            catch (Exception e) { Debug.LogWarning("[Motocard] strings " + package + "/" + table + ": " + e.Message); return new Dictionary<string, string>(); }
        }
        /// A front end string (the FrontEnd table, then Shared's: STR_CHANGE_RIDER_NAME lives there).
        public static string Text(string key, string def)
        {
            var d = Shared;
            string s = OpenContent.Str(key, null);
            if ((string.IsNullOrEmpty(s) || s == key) && d.TryGetValue(key, out var v)) s = v;
            return string.IsNullOrEmpty(s) || s == key ? def : s;
        }
        static Dictionary<string, string> s_shared;
        static Dictionary<string, string> Shared => s_shared ??= Table("Shared", "MXRaven_Default_Strings");
        /// An EMB_ text (the label the original's page puts before a value: "MotoCareer Progress: ", "Total Credits: ").
        public static string Embedded(string key, string def)
        {
            try
            {
                var embeds = FeCareerScripts.Embeds(OriginalUiAssets.Data.Read(OriginalUiAssets.Data.Pkg("FrontEnd"), "localiz", "MXRaven_Default_Strings"));
                if (embeds.TryGetValue(key, out var s) && !string.IsNullOrEmpty(s)) return s;
            }
            catch (Exception e) { Debug.LogWarning("[Motocard] " + key + ": " + e.Message); }
            return def;
        }

        // ------------------------------------------------------------------------------------------------ the card

        public static string RiderName => (ReflexProfile.HasPersona ? ReflexProfile.Persona : "YOUR RIDER");
        public static string RiderNumber(ProfileData p) => p?.T("PlayerRider").Str("Number", 0) ?? "998";
        public static int Credits(ProfileData p) => p?.T("PlayerRider").Int("AccountBalance", 0, 0) ?? 0;

        /// App.CalculateCareerCompletion (00743870), the profile's PlayerSeriesStatus medals weighted by Series.CareerCompletionFactor:
        ///   for each Series row (table order):  total += factor;  the status row of its SeriesId (first match), when there is one and its
        ///   BestPlaceFinish is 0 / 1 / 2 (gold / silver / bronze; unsigned compare: 1000 "none" and negatives count nothing):
        ///   weighted += (float)(3 - place) * (1/3) * factor.   The result is weighted / total, all in single precision in the original's
        ///   operation order (SSE: cvtsi2ss, mulss 0x92e214 = 0.33333334f, mulss factor, addss). REIMPLEMENTED in C#, not a core RX_FUNCTION:
        ///   the product's menus read the profile as a managed snapshot (the core holds the player tables only inside a race session), so the
        ///   function is table maths over Series and PlayerSeriesStatus rows. A Series table with no weight (total 0) gives 0 (the
        ///   original would divide 0 by 0).
        public static float CareerCompletion(ProfileData p)
        {
            var status = p?.T("PlayerSeriesStatus");
            float weighted = 0f, total = 0f;
            foreach (var series in T.Rows("Series"))
            {
                float factor = FeTables.F(series, "CareerCompletionFactor");
                total = factor + total;
                int row = status == null ? -1 : status.Find("SeriesId", FeTables.I(series, "SeriesId"));
                if (row < 0) continue;
                int place = status.Int("BestPlaceFinish", row, -1);
                if ((uint)place > 2u) continue;
                float w = (float)(3 - place);
                w = w * 0.3333333432674408f;   // 0x92e214
                w = w * factor;
                weighted = w + weighted;
            }
            return total == 0f ? 0f : weighted / total;
        }

        /// Motocard.lua: math.floor( App.CalculateCareerCompletion * 100.0 ), the product in Lua's doubles.
        public static int CareerPercent(ProfileData p) => (int)Math.Floor((double)CareerCompletion(p) * 100.0);

        /// The seven latest unlocked highlights (HighlightIds, newest first): PlayerHighlight by UnlockedOrder descending, those with an order above 0.
        public static List<int> RecentHighlights(ProfileData p, int max = 7)
        {
            var t = p?.T("PlayerHighlight"); var result = new List<int>();
            if (t == null) return result;
            var rows = Enumerable.Range(0, t.rows).Select(r => (id: t.Int("HighlightId", r), order: t.Int("UnlockedOrder", r, 0), r))
                                 .Where(x => x.order > 0).OrderByDescending(x => x.order).ThenBy(x => x.r);
            foreach (var x in rows) { if (result.Count >= max) break; result.Add(x.id); }
            return result;
        }

        // ------------------------------------------------------------------------------------------------ highlights

        /// The Highlight material's name of an id: "Highlight05", "Highlight17" (the two-digit pad is the Lua's: < 10 gets a 0).
        public static string HighlightMaterial(int id) => id < 10 ? "Highlight0" + id : "Highlight" + id;

        /// The 48 highlights (every Highlight row) in the page's order, and how many are reached.
        public static List<MotocardHighlight> Highlights(ProfileData p, out int unlocked)
        {
            var progress = p?.T("PlayerHighlight");
            var list = new List<MotocardHighlight>();
            unlocked = 0;
            foreach (var r in T.Rows("Highlight").OrderBy(x => FeTables.I(x, "HighlightId")))
            {
                int id = FeTables.I(r, "HighlightId"), goal = FeTables.I(r, "Goal", 0);
                int row = progress == null ? -1 : progress.Find("HighlightId", id);
                int gp = row >= 0 ? progress.Int("GoalProgress", row, 0) : 0, order = row >= 0 ? progress.Int("UnlockedOrder", row, -1) : -1;
                bool reached = gp == goal;
                if (reached) unlocked++;
                list.Add(new MotocardHighlight
                {
                    id = id, goal = goal, progress = gp, order = order, unlocked = reached, category = Category(FeTables.S(r, "Enum")),
                    title = HighlightStrings.TryGetValue("STR_HIGHLIGHT_" + id + "_TITLE", out var t) ? t : "HIGHLIGHT " + id,
                    description = HighlightStrings.TryGetValue("STR_HIGHLIGHT_" + id + "_DESCRIPTION", out var d) ? d : "",
                });
            }
            var sorted = list.Select((h, i) => (h, i)).ToList();
            sorted.Sort((x, y) => Before(x.h, y.h) ? -1 : Before(y.h, x.h) ? 1 : x.i.CompareTo(y.i));   // table.sort( HighlightSort )
            return sorted.Select(x => x.h).ToList();
        }

        /// HighlightSort( a, b ): a is before b.
        static bool Before(MotocardHighlight a, MotocardHighlight b)
        {
            if (a.unlocked) return !b.unlocked || a.order > b.order;
            if (b.unlocked) return false;
            float pa = a.goal != 0 ? (float)a.progress / a.goal : 0f, pb = b.goal != 0 ? (float)b.progress / b.goal : 0f;
            return pa == pb ? a.id < b.id : pa > pb;
        }

        /// The Highlight row's Enum ("Career_07", "FMX_01", "Online_03") as a short word.
        static string Category(string e)
        {
            string head = (e ?? "").Split('_')[0];
            switch (head) { case "Career": return "CAREER"; case "Online": return "ONLINE"; case "Tutorial": return "TUTORIAL"; case "Tricks": return "TRICKS"; case "FMX": return "FREESTYLE"; default: return "MISC"; }
        }

        static readonly Dictionary<string, Texture2D> s_icons = new Dictionary<string, Texture2D>();
        /// A highlight's picture (the material "Highlight##" of the install's atlas, cut out): in colour, or desaturated to grey (the
        /// original's SetDesaturationLevel( 255 ) on a locked one). Cached; null when the install has no such material.
        public static Texture2D HighlightIcon(int id, bool locked)
        {
            string key = HighlightMaterial(id) + (locked ? "#grey" : "");
            if (s_icons.TryGetValue(key, out var cached)) return cached;
            Texture2D result = null;
            try
            {
                var (tex, uv) = OpenContent.Assets.Material(HighlightMaterial(id));
                if (tex)
                {
                    int w = Mathf.Max(1, Mathf.RoundToInt(tex.width * uv.width)), h = Mathf.Max(1, Mathf.RoundToInt(tex.height * uv.height));
                    var rt = RenderTexture.GetTemporary(w, h, 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.sRGB);
                    Graphics.Blit(tex, rt, new Vector2(uv.width, uv.height), new Vector2(uv.x, uv.y));
                    var prev = RenderTexture.active; RenderTexture.active = rt;
                    result = new Texture2D(w, h, TextureFormat.RGBA32, true) { name = "mc_" + key, wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Trilinear, anisoLevel = 4, hideFlags = HideFlags.DontSave };
                    result.ReadPixels(new Rect(0, 0, w, h), 0, 0);
                    RenderTexture.active = prev; RenderTexture.ReleaseTemporary(rt);
                    if (locked)
                    {
                        var px = result.GetPixels32();
                        for (int i = 0; i < px.Length; i++)
                        {
                            byte g = (byte)Mathf.Clamp(Mathf.RoundToInt(px[i].r * 0.299f + px[i].g * 0.587f + px[i].b * 0.114f), 0, 255);
                            px[i] = new Color32(g, g, g, px[i].a);
                        }
                        result.SetPixels32(px);
                    }
                    result.Apply(true, false);
                }
            }
            catch (Exception e) { Debug.LogWarning("[Motocard] highlight icon " + id + ": " + e.Message); }
            return s_icons[key] = result;
        }

        // ------------------------------------------------------------------------------------------------ records

        /// MotocardRecords_ActivityTypeSort's list: the EventTypeIds of the offline leaderboards in this order, any other after (stable).
        static readonly int[] TypeOrder = { 9, 14, 10, 17, 7, 8, 16 };

        /// App.FormatTime( seconds ) = 00790100: minutes and seconds with the hundredths; 0 and the unset float (FLT_MAX) show dashes.
        public static string TimeText(float t)
        {
            if (t == 0f || t == float.MaxValue) return "--:--.--";
            if (t > 5999.99f) return "99:99.99";
            if (t < -5999.99f) return "-99:99.99";
            int cs = (int)(Math.Abs(t * 100f) + 0.5f);
            return (t < 0 ? "-" : "") + (cs / 6000).ToString("00") + ":" + (cs / 100 % 60).ToString("00") + "." + (cs % 100).ToString("00");
        }

        /// MXUIManager_SetEventFlags (005a3820) for the event type the leaderboards set: EventHasLaps / EventHasTimer by the activity enum
        /// (jump table 005a3aac, bytes 005a3ac0): the lap races (National .. Endurocross, Terracross, UTVMoto, Powercross) have laps, Freestyle /
        /// FMXJam and the unlisted kinds a timer, Free Ride a timer only for the split-screen / LAN modes (single player here: none),
        /// Section Race / mini games rounds.
        public static void EventFlags(string activity, out bool laps, out bool timer)
        {
            int index = ReflexEvents.ActivityIndex(activity);
            int kind;   // the jump table's case: 0 laps, 1 timer, 2 free ride, 3 rounds
            if (index < 0 || index > 0x14) kind = 1;
            else kind = new[] { 0, 0, 0, 0, 0, 1, 0, 0, 2, 3, 3, 1, 1, 1, 3, 3, 3, 0, 0, 0, 1 }[index];   // (the table's 4 = the default case = timer)
            laps = kind == 0; timer = kind == 1;
        }

        /// The offline leaderboards' activity types, as MotocardRecords:PopulateActivityTypeList makes them. header = the column's title
        /// (OnActivityTypeListFocusedChanged), column = what GenerateLocalRecords ranks by.
        public static List<MotocardRecordType> RecordTypes()
        {
            var ids = new List<int>(); int last = -1;
            foreach (var r in T.Rows("LeaderboardEventTypeVehicleTypes").OrderBy(r => FeTables.I(r, "EventTypeId")))
            {
                int id = FeTables.I(r, "EventTypeId"); if (id == last) continue; last = id; ids.Add(id);
            }
            int Rank(int id) { int i = Array.IndexOf(TypeOrder, id); return i < 0 ? 1000 : i; }
            var list = new List<MotocardRecordType>();
            foreach (int id in ids.Select((x, i) => (x, i)).OrderBy(x => Rank(x.x)).ThenBy(x => x.i).Select(x => x.x))
            {
                string activity = FeTables.S(T.EventType(id), "ActivityName") ?? "";
                EventFlags(activity, out bool laps, out bool timer);
                string header = laps && activity != "WaypointRace" ? Text("STR_LEADERBOARD_BESTLAPTIME", "BEST LAP TIME")
                              : timer ? Text("STR_LEADERBOARD_HIGHESTSCORE", "HIGHEST SCORE") : Text("STR_LEADERBOARD_BESTTOTALTIME", "BEST TOTAL TIME");
                // GenerateLocalRecords: neither flag -> laps; the Waypoint race has no laps
                bool useLaps = laps, useTimer = timer;
                if (!useLaps && !useTimer) useLaps = true;
                if (activity == "WaypointRace") useLaps = false;
                list.Add(new MotocardRecordType
                {
                    eventTypeId = id, activity = activity, header = header.ToUpperInvariant(), column = useLaps ? 0 : useTimer ? 1 : 2,
                    name = Text("STR_MXACTIVITYTYPE_" + activity.ToUpperInvariant(), FeTables.S(T.EventType(id), "Name") ?? activity).ToUpperInvariant(),
                });
            }
            return list;
        }

        /// The type's locations (EventTypeLocations by EventTypeId, table order).
        public static List<MotocardRecordLocation> RecordLocations(MotocardRecordType type)
        {
            var list = new List<MotocardRecordLocation>();
            if (type == null) return list;
            foreach (var r in T.Rows("EventTypeLocations").Where(r => FeTables.I(r, "EventTypeId") == type.eventTypeId))
            {
                int product = FeTables.I(r, "StoreProductId");
                string title = T.TitleKey(product);
                list.Add(new MotocardRecordLocation
                {
                    eventTypeLocationId = FeTables.I(r, "EventTypeLocationId"), product = product, posterIcon = FeTables.I(r, "PosterIconId"),
                    eventLocationId = FeTables.I(r, "EventLocationId"), trackId = FeTables.I(r, "TrackId", 0),
                    name = (title == null ? "LOCATION " + FeTables.I(r, "EventLocationId") : OpenContent.Str(title, title)).ToUpperInvariant(),
                });
            }
            return list;
        }

        /// GenerateLocalRecords for the focused location (all vehicle types): the PlayerRecords rows of its EventTypeLocationId, sorted by the
        /// type's column ascending (the Lua's order; a stable sort here, the Lua's table sort is not), each with the profile's name, the vehicle
        /// type's name and the value formatted by the column.
        public static List<MotocardRecordRow> Records(ProfileData p, MotocardRecordType type, MotocardRecordLocation location)
        {
            var result = new List<MotocardRecordRow>();
            var t = p?.T("PlayerRecords");
            if (t == null || type == null || location == null) return result;
            string column = type.column == 0 ? "BestLapTime" : type.column == 1 ? "BestScore" : "TotalTime";
            var rows = Enumerable.Range(0, t.rows).Where(r => t.Int("EventTypeLocationId", r) == location.eventTypeLocationId)
                                 .Select(r => (r, key: type.column == 1 ? (double)t.Int("BestScore", r, 0) : t.Float(column, r)))
                                 .OrderBy(x => x.key).ThenBy(x => x.r).ToList();
            int index = 1;
            foreach (var (r, _) in rows)
            {
                int vt = t.Int("VehicleTypeId", r);
                var vrow = T.Rows("VehicleType").FirstOrDefault(v => FeTables.I(v, "VehicleTypeId") == vt);
                string vname = vrow == null ? "TYPE " + vt : Text(FeTables.S(vrow, "Name"), FeTables.S(vrow, "ShortName") ?? "TYPE " + vt);
                string value = type.column == 0 ? TimeText(t.Float("BestLapTime", r)) : type.column == 1 ? Num(t.Int("BestScore", r, 0)) : TimeText(t.Float("TotalTime", r));
                result.Add(new MotocardRecordRow { index = index++, vehicleType = vt, name = RiderName, vehicle = vname.ToUpperInvariant(), value = value });
            }
            return result;
        }

        // ------------------------------------------------------------------------------------------------ tricks

        /// The trick library: the four tiers' stunts (tier, then the StuntTable's order), a display name already listed skipped.
        public static List<MotocardStunt> Stunts()
        {
            var list = new List<MotocardStunt>(); var seen = new HashSet<string>();
            try
            {
                var raw = CoreStuntCatalog.RawNames();
                foreach (var s in CoreStuntCatalog.LoadAll())
                {
                    if (string.IsNullOrEmpty(s.key) || !seen.Add(s.key)) continue;
                    list.Add(new MotocardStunt
                    {
                        tier = s.tier, name = s.name, key = s.key, anim = raw.TryGetValue(s.name, out var asWritten) ? asWritten : s.name,
                        display = (StuntStrings.TryGetValue(s.key, out var label) ? label : s.name.Replace('_', ' ')).ToUpperInvariant(),
                        material = "Stunt_" + (s.key.StartsWith("STR_", StringComparison.Ordinal) ? s.key.Substring(4) : s.key),
                    });
                }
            }
            catch (Exception e) { Debug.LogWarning("[Motocard] trick library: " + e.Message); }
            return list;
        }

        // ------------------------------------------------------------------------------------------------ record totals (a port extra)

        /// Sums of the PlayerRecords columns the core keeps (counts, laps, ride time): (events, laps, seconds, jumps attempted, jumps missed,
        /// combos completed, holeshots won). Rows of the race's working copy (EventTypeLocationId -1) do not count.
        public static (int events, int laps, float seconds, int jumps, int failed, int combos, int holeshots, int earnings) Totals(ProfileData p)
        {
            var t = p?.T("PlayerRecords");
            int events = 0, laps = 0, jumps = 0, failed = 0, combos = 0, holeshots = 0; float seconds = 0f;
            if (t != null)
                for (int r = 0; r < t.rows; r++)
                {
                    if (t.Int("EventTypeLocationId", r) < 0) continue;
                    events += t.Int("TotalEvents", r, 0); laps += t.Int("TotalLaps", r, 0); seconds += t.Float("TotalTime", r);
                    jumps += t.Int("TotalAttemptedJumps", r, 0); failed += t.Int("TotalFailedJumps", r, 0);
                    combos += t.Int("TotalCombosCompleted", r, 0); holeshots += t.Int("TotalHoleshotsWon", r, 0);
                }
            return (events, laps, seconds, jumps, failed, combos, holeshots, p?.T("PlayerRider").Int("TotalEarnings", 0, 0) ?? 0);
        }

        /// "3:07:42" for a ride time in seconds.
        public static string Duration(float seconds)
        {
            int s = Mathf.Max(0, Mathf.RoundToInt(seconds));
            return (s / 3600) + ":" + (s / 60 % 60).ToString("00") + ":" + (s % 60).ToString("00");
        }
    }
}
