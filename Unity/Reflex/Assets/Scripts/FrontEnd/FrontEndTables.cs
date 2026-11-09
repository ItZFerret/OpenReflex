// The game tables the front-end pages query, as the game merges them (ReflexEvents.Tables: MXTables_Default plus the table data of
// the DLC databases the game finds; the same tables the Lua reads through App.Lookup): EventType, EventTypeLocations, EventLocation,
// EventTypeDurations, EventTypeVehicleClasses, VehicleClass(Type), Icon / AssetPackageDatabase / AssetPackage / AssetDatabase
// (MXUIStreamableImageIcon:SetIconId), StoreProducts / Localization / LocalizationAsset (MXTableHelper:TitleLocalizationString; the
// DLC store products' names live in the DLC UI databases, DlcStrings), DLCStoreProducts (MXTableHelper_IsStoreProductDLC),
// PlayerVehicle / VehicleModel (the default rider's vehicle type for the vehicle-class list). Which locations can be raced comes
// from ReflexEvents (the events the race scene supports; levels missing from the install are not listed there).
using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;

namespace Reflex.Port.FrontEnd
{
    public sealed class FeTables
    {
        readonly Dictionary<string, List<Dictionary<string, object>>> t;
        static FeTables _shared;
        public static FeTables Shared => _shared ??= new FeTables();

        FeTables() { t = ReflexEvents.Tables; }

        public List<Dictionary<string, object>> Rows(string table) => t.TryGetValue(table, out var r) ? r : new List<Dictionary<string, object>>();
        public static int I(Dictionary<string, object> r, string c, int def = -1) => Rfx.Int(r, c, def);
        public static string S(Dictionary<string, object> r, string c) => Rfx.Str(r, c);
        public static float F(Dictionary<string, object> r, string c) { try { return r != null && r.TryGetValue(c, out var v) && v != null ? Convert.ToSingle(v, System.Globalization.CultureInfo.InvariantCulture) : 0f; } catch (Exception) { return 0f; } }
        Dictionary<string, object> Unique(string table, string col, int v) => RowIndex.First(Rows(table), col, v);   // (indexed: w-menufps)

        /// Icon -> AssetName + the (package, database) of its PackageDatabaseId.
        public bool IconAsset(int iconId, out string asset, out string package, out string database)
        {
            asset = package = database = null;
            var ic = Unique("Icon", "IconId", iconId); if (ic == null) return false;
            asset = S(ic, "AssetName");
            var pd = Unique("AssetPackageDatabase", "PackageDatabaseId", I(ic, "PackageDatabaseId")); if (pd == null) return false;
            package = S(Unique("AssetPackage", "PackageId", I(pd, "PackageId")), "PackageName");
            database = S(Unique("AssetDatabase", "DatabaseId", I(pd, "DatabaseId")), "DatabaseName");
            return asset != null && package != null && database != null;
        }

        /// MXTableHelper:MapEventTypeId( App.GetMXActivityType( name ) ): the EventType row of an activity name ("Freeride" -> FreeRide).
        public Dictionary<string, object> EventTypeOf(string activity) => Rows("EventType").FirstOrDefault(r => string.Equals(S(r, "ActivityName"), activity, StringComparison.OrdinalIgnoreCase));
        public Dictionary<string, object> EventType(int id) => Unique("EventType", "EventTypeId", id);

        /// The string-table key of a Localization id (and the table it lives in).
        public string LocalizationKey(int id) => S(Unique("Localization", "LocalizationId", id), "LocalizationKey");
        /// MXTableHelper:TitleLocalizationString(storeProductId).
        public string TitleKey(int storeProductId) { var sp = Unique("StoreProducts", "StoreProductId", storeProductId); return sp == null ? null : LocalizationKey(I(sp, "LocalizationId")); }
        public bool Hidden(int storeProductId) => I(Unique("StoreProducts", "StoreProductId", storeProductId), "Hidden", 0) != 0;
        /// MXTableHelper_IsStoreProductDLC (004e9bf0): the product has a DLCStoreProducts row (of the merged DLC tables).
        public bool IsDlcProduct(int storeProductId) => Unique("DLCStoreProducts", "StoreProductId", storeProductId) != null;

        Dictionary<string, string> dlcStrings; string dlcLanguage;
        /// The string tables of the DLC tables' LocalizationAsset rows (MXTables_DLC004_StoreProducts ...): the localiz asset
        /// "<asset>.<language>" of a found DLC database (MXUI_DLC004: package MXUI_DLC004.ENG). Keys STR_<storeProductId>.
        public Dictionary<string, string> DlcStrings(string language)
        {
            if (dlcStrings != null && dlcLanguage == language) return dlcStrings;
            var d = new Dictionary<string, string>(StringComparer.Ordinal);
            var inst = ReflexInstall.Packages;
            foreach (var la in Rows("LocalizationAsset").Where(ReflexEvents.IsDlcRow))
            {
                string name = S(la, "LocalizationAsset"); if (string.IsNullOrEmpty(name)) continue;
                foreach (string db in ReflexEvents.DlcDatabases)
                {
                    IEnumerable<Rfx.Package> pkgs;
                    try { pkgs = inst.Db(db)?.packages.Values; } catch (Exception e) { UnityEngine.Debug.LogWarning($"DLC strings {db} not read: {e.Message}"); continue; }
                    if (pkgs == null) continue;
                    var a = pkgs.Select(p => p.Find(name + "." + language, "localiz") ?? p.Find(name, "localiz")).FirstOrDefault(x => x != null);
                    if (a == null) continue;
                    try { foreach (var kv in Rfx.Strings(inst.Read(a))) d[kv.Key] = kv.Value; }
                    catch (Exception e) { UnityEngine.Debug.LogWarning($"DLC strings {db}/{a.name} not read: {e.Message}"); }
                    break;
                }
            }
            dlcLanguage = language;
            return dlcStrings = d;
        }

        /// The default profile's vehicle (PlayerVehicle row 0 -> VehicleModel.VehicleTypeId): the "Player1VehicleTypeId" of LocationSelect.
        public int DefaultVehicleTypeId()
        {
            var pv = Rows("PlayerVehicle").FirstOrDefault();
            return I(Unique("VehicleModel", "VehicleModelId", I(pv, "VehicleModelId")), "VehicleTypeId", 2);
        }

        // ------------------------------------------------------------------------------------------- LocationSelect
        public sealed class Location
        {
            public int eventTypeLocationId, storeProductId, iconId, mapIconId, posterIconId, descriptionLocalizationId, eventLocationId, trackId;
            public float sortValue; public bool locked, isNew, dlc; public string filename; public ReflexEvents.Event ev;
        }

        /// LocationSelect:Start's list: the EventTypeLocations rows of the event type (the DLC rows included), hidden store products
        /// skipped, sorted by (locked, SortValue) as LocationSort with Lua's table.sort (the DLC rows, SortValue 0, come first). A
        /// location the race scene cannot run (ReflexEvents has no event for it: another vehicle type, or a mode the port does not
        /// run) is shown locked; one whose level is not installed is not listed.
        /// The DLC mark is MXTableHelper_IsStoreProductDLC as the game answers it in the profile session (ReflexProfile; from
        /// DLCStoreProducts without one; the NEW mark is not shown: the port's OnAccept does not SetStoreProductNotNew). A DLC
        /// location is also locked when the signed-in game says so (MXTableHelper_IsLocked: no PlayerProductsUnlocked row;
        /// Profile_ApplyDLCUnlocks 005a3f40 adds the found DLCs' DLCUnlocks products at sign-in). The default locations keep the
        /// port's rule (the game's career unlocks of arcade locations are not applied to them yet).
        public List<Location> Locations(int eventTypeId)
        {
            var events = ReflexEvents.All;
            var profile = ReflexProfile.Current; if (profile != null && !profile.signedIn) profile = null;
            var o = new List<Location>();
            foreach (var r in Rows("EventTypeLocations"))
            {
                if (I(r, "EventTypeId") != eventTypeId) continue;
                int sp = I(r, "StoreProductId");
                if (Hidden(sp)) continue;
                var l = new Location
                {
                    eventTypeLocationId = I(r, "EventTypeLocationId"), storeProductId = sp, iconId = I(r, "PreviewIconId"), mapIconId = I(r, "MapIconId"),
                    posterIconId = I(r, "PosterIconId"), descriptionLocalizationId = I(r, "DescriptionLocalizationId"), sortValue = F(r, "SortValue"),
                    eventLocationId = I(r, "EventLocationId"), trackId = I(r, "TrackId", 0),
                };
                l.filename = S(Unique("EventLocation", "EventLocationId", l.eventLocationId), "Filename");
                if (string.IsNullOrEmpty(l.filename) || !ReflexInstall.Packages.Has(l.filename)) continue;      // DLC / level not installed
                l.ev = events.FirstOrDefault(e => e.spec.typeId == eventTypeId && e.spec.locationId == l.eventLocationId && e.spec.trackId == l.trackId);
                l.dlc = profile != null && profile.products.ContainsKey(sp) ? profile.IsDlc(sp) : IsDlcProduct(sp);
                l.locked = l.ev == null || l.dlc && profile != null && profile.IsLocked(sp);
                o.Add(l);
            }
            // table.sort(m_EventTypeLocationData, LocationSort): a[5] == b[5] ? a[9] < b[9] : a[5] < b[5] (isLocked TRUE 1 / FALSE 0, then
            // SortValue) with Lua 5.1.4's own quicksort (not stable: the DLC rows' SortValue 0 ties keep the order that algorithm gives them)
            LuaSort(o, (a, b) => a.locked == b.locked ? a.sortValue < b.sortValue : !a.locked && b.locked);
            return o;
        }

        /// Lua 5.1.4 table.sort (ltablib.c auxsort, the game's Lua 0092ea30): the same element order for ties as the game.
        public static void LuaSort<T>(List<T> a, Func<T, T, bool> lt)
        {
            if (a.Count < 2) return;
            // 1-based like the C source
            T Get(int k) => a[k - 1];
            void Swap(int x, int y) { var t = a[x - 1]; a[x - 1] = a[y - 1]; a[y - 1] = t; }
            void Aux(int l, int u)
            {
                while (l < u)
                {
                    if (lt(Get(u), Get(l))) Swap(l, u);                     // a[u] < a[l]?
                    if (u - l == 1) break;
                    int i = (l + u) / 2;
                    if (lt(Get(i), Get(l))) Swap(i, l);                     // a[i] < a[l]?
                    else if (lt(Get(u), Get(i))) Swap(i, u);                // a[u] < a[i]?
                    if (u - l == 2) break;
                    T p = Get(i);                                           // pivot
                    Swap(i, u - 1);
                    i = l; int j = u - 1;
                    for (;;)
                    {
                        while (lt(Get(++i), p)) if (i > u) throw new InvalidOperationException("invalid order function for sorting");
                        while (lt(p, Get(--j))) if (j < l) throw new InvalidOperationException("invalid order function for sorting");
                        if (j < i) break;
                        Swap(i, j);
                    }
                    Swap(u - 1, i);
                    if (i - l < u - i) { j = l; i = i - 1; l = i + 2; }
                    else { j = i + 1; i = u; u = j - 2; }
                    Aux(j, i);
                }
            }
            Aux(1, a.Count);
        }

        /// LocationSelect:BuildDurationList: Min, then +Increment while below BigIncrementStart, then +BigIncrement up to Max (the Lua's
        /// trailing extra Max sits at index #list and is never shown). Returns null when the event type has no row.
        public List<int> Durations(int eventTypeId, out string singularKey, out string pluralKey, out string durationType)
        {
            singularKey = pluralKey = durationType = null;
            var r = Unique("EventTypeDurations", "EventTypeId", eventTypeId); if (r == null) return null;
            int min = I(r, "Min"), max = I(r, "Max"), sinc = I(r, "Increment"), binc = I(r, "BigIncrement"), bstart = I(r, "BigIncrementStart");
            singularKey = S(r, "SingularStringKey"); pluralKey = S(r, "PluralStringKey"); durationType = S(r, "ActivityDurationType");
            var l = new List<int>(); int v = min; l.Add(v);
            while (v < bstart && v < max && sinc > 0) { v += sinc; l.Add(v); }
            while (v < max && binc > 0) { v += binc; l.Add(v); }
            // Lua: list[0..i] with list[i+1] = max; #list = i+1 counts keys 1..i+1, so entries 0..i are shown: v's list as built.
            return l;
        }

        /// LocationSelect:BuildVehicleClassList: the event type's classes (by VehicleClassId) containing every required vehicle type,
        /// sorted by their number of vehicle types (LocationSelect_VehicleClassSort, with the game's Lua table.sort: its order of ties, e.g.
        /// an ATV at a National 1, 12, 10, 15). (id, name key, type count).
        public List<(int id, string key, int types)> VehicleClasses(int eventTypeId, params int[] requiredTypes)
        {
            var o = new List<(int, string, int)>();
            foreach (int cls in Rows("EventTypeVehicleClasses").Where(r => I(r, "EventTypeId") == eventTypeId).Select(r => I(r, "VehicleClassId")).OrderBy(x => x))
            {
                var types = Rows("VehicleClassType").Where(r => I(r, "VehicleClassId") == cls).Select(r => I(r, "VehicleTypeId")).ToList();
                if (!requiredTypes.All(types.Contains)) continue;
                o.Add((cls, S(Unique("VehicleClass", "VehicleClassId", cls), "Name"), types.Count));
            }
            LuaSort(o, (a, b) => a.Item3 < b.Item3);
            return o;
        }
    }
}
