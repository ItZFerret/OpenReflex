// GARAGE > RIDER > RIDER PRESETS and GARAGE > VEHICLE > BIKE PRESETS: named setups the player saves and loads again (OpenShell.GaragePresets.cs
// draws them). Kept in the client save ("Garage.Presets", JSON: never in the game's profile, so the core never sees them):
//   rider preset   the piece worn of every gear type (helmet, goggles, jersey ... skin tone): a stock RiderGearId, or a gear pack piece's token
//                  ("g:<pack hash>:<row>", KitPacks: its renumbered id changes from run to run, the token does not)
//   bike preset    the vehicle model, the part of every customizable part type (a stock VehiclePartId or a kit pack piece's token "k:..."),
//                  the eight tuning values (PlayerVehicleTuning) and the bike mod chosen for the model ("" the original bike)
// Loading puts on what the player owns of it now and says what it left as it was (a piece not owned / not installed, a bike not owned; the
// tuning when the parts now give other points). Loading goes through the garage's own equip calls (EquipGear / EquipPart / SetTuning /
// SelectVehicle), so the profile, the kit-pack choices and the race see exactly what the garage's lists would have set.
using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using UnityEngine;

namespace Reflex.Port.OpenFrontEnd
{
    internal static class GaragePresets
    {
        public const string SaveKey = "Garage.Presets";
        public const int MaxPresets = 24, MaxName = 20;

        [Serializable] public sealed class Pick { public int type; public int id = -1; public string token = ""; }
        [Serializable] public sealed class RiderPreset { public string name = ""; public long saved; public List<Pick> gear = new List<Pick>(); }
        [Serializable] public sealed class BikePreset
        {
            public string name = ""; public long saved; public int model = -1; public string mod = "";
            public List<Pick> parts = new List<Pick>(); public int[] tuning = new int[0];
        }
        [Serializable] sealed class Store { public int version = 1; public List<RiderPreset> rider = new List<RiderPreset>(); public List<BikePreset> bike = new List<BikePreset>(); }

        static Store s_store;
        static Store Data
        {
            get
            {
                if (s_store != null) return s_store;
                string json = Options.ReflexSave.GetString(SaveKey, "");
                try { s_store = string.IsNullOrEmpty(json) ? new Store() : JsonUtility.FromJson<Store>(json) ?? new Store(); }
                catch (Exception e) { Debug.LogWarning("[Presets] the saved presets could not be read (" + e.Message + "): starting empty"); s_store = new Store(); }
                s_store.rider ??= new List<RiderPreset>(); s_store.bike ??= new List<BikePreset>();
                return s_store;
            }
        }
        static void Save() { Options.ReflexSave.SetString(SaveKey, JsonUtility.ToJson(Data)); Options.ReflexSave.Save(); }

        public static IReadOnlyList<RiderPreset> Riders => Data.rider;
        public static IReadOnlyList<BikePreset> Bikes => Data.bike;
        static long Now => DateTimeOffset.UtcNow.ToUnixTimeSeconds();

        /// A name for a new preset: the given one cleaned (letters, digits, spaces, - and _; MaxName long), else "<stem> <n>" with the first free n.
        public static string CleanName(string s)
        {
            string kept = new string((s ?? "").ToUpperInvariant().Where(c => char.IsLetterOrDigit(c) || c == ' ' || c == '-' || c == '_').ToArray());
            kept = System.Text.RegularExpressions.Regex.Replace(kept, " {2,}", " ").Trim();
            return kept.Length > MaxName ? kept.Substring(0, MaxName).Trim() : kept;
        }
        public static string NextName(string stem, IEnumerable<string> taken)
        {
            stem = CleanName(stem); if (stem.Length > MaxName - 3) stem = stem.Substring(0, MaxName - 3).Trim();
            var used = new HashSet<string>(taken, StringComparer.OrdinalIgnoreCase);
            for (int n = 1; ; n++) { string s = stem + " " + n; if (!used.Contains(s)) return s; }
        }

        // ------------------------------------------------------------------------------------------------ what is worn / ridden now

        static Pick PickOf(int type, int id)
        {
            var kit = KitPacks.Item(id);
            return new Pick { type = type, id = kit != null ? -1 : id, token = kit?.Token ?? "" };
        }
        /// The id a pick stands for now (a pack piece's renumbered id; -1: its pack is not installed).
        public static int Resolve(Pick p) => string.IsNullOrEmpty(p.token) ? p.id : KitPacks.FromToken(p.token)?.id ?? -1;

        public static List<Pick> CurrentGear() => OpenGarageData.GearTypes().Select(t => PickOf(t.id, OpenGarageData.EquippedGear(t.id))).Where(p => p.id >= 0 || p.token.Length > 0).ToList();
        public static List<Pick> CurrentParts(int playerVehicle) =>
            OpenGarageData.PartTypes(playerVehicle).Select(t => PickOf(t.id, OpenGarageData.EquippedPart(playerVehicle, t.id))).Where(p => p.id >= 0 || p.token.Length > 0).ToList();
        public static int[] CurrentTuning(int playerVehicle)
        {
            var tt = OpenGarageData.Tab("PlayerVehicleTuning"); int row = tt.Find("PlayerVehicleId", playerVehicle);
            return row < 0 ? new int[0] : OuTuneRow.Columns.Select(c => tt.Int(c, row, 0)).ToArray();
        }

        static bool Same(List<Pick> a, List<Pick> b) => a.Count == b.Count && a.All(x => b.Any(y => y.type == x.type && y.id == x.id && y.token == x.token));
        /// The preset is what the player wears / rides now.
        public static bool InUse(RiderPreset p) => !OpenGarageData.Pro && Same(p.gear, CurrentGear());
        public static bool InUse(BikePreset p)
        {
            if (OpenGarageData.Pro || OpenGarageData.EquippedModel != p.model) return false;
            int pv = OpenGarageData.Equipped;
            return Same(p.parts, CurrentParts(pv)) && CurrentTuning(pv).SequenceEqual(p.tuning ?? new int[0]) && (BikeMods.ChoiceFor(p.model) ?? "") == (p.mod ?? "");
        }

        // ------------------------------------------------------------------------------------------------ save, rename, delete

        public static int SaveRider(string name, int over = -1)
        {
            var p = over >= 0 && over < Data.rider.Count ? Data.rider[over] : new RiderPreset();
            p.name = CleanName(name).Length > 0 ? CleanName(name) : p.name; p.saved = Now; p.gear = CurrentGear();
            if (over < 0 || over >= Data.rider.Count) { if (Data.rider.Count >= MaxPresets) return -1; Data.rider.Add(p); over = Data.rider.Count - 1; }
            Save(); return over;
        }
        public static int SaveBike(string name, int over = -1)
        {
            int pv = OpenGarageData.Equipped, model = OpenGarageData.EquippedModel;
            if (OpenGarageData.Pro || model < 0) return -1;
            var p = over >= 0 && over < Data.bike.Count ? Data.bike[over] : new BikePreset();
            p.name = CleanName(name).Length > 0 ? CleanName(name) : p.name; p.saved = Now;
            p.model = model; p.mod = BikeMods.ChoiceFor(model) ?? ""; p.parts = CurrentParts(pv); p.tuning = CurrentTuning(pv);
            if (over < 0 || over >= Data.bike.Count) { if (Data.bike.Count >= MaxPresets) return -1; Data.bike.Add(p); over = Data.bike.Count - 1; }
            Save(); return over;
        }
        public static void Rename(bool bike, int index, string name)
        {
            string n = CleanName(name); if (n.Length == 0) return;
            if (bike && index >= 0 && index < Data.bike.Count) Data.bike[index].name = n;
            else if (!bike && index >= 0 && index < Data.rider.Count) Data.rider[index].name = n;
            else return;
            Save();
        }
        public static void Delete(bool bike, int index)
        {
            if (bike && index >= 0 && index < Data.bike.Count) Data.bike.RemoveAt(index);
            else if (!bike && index >= 0 && index < Data.rider.Count) Data.rider.RemoveAt(index);
            else return;
            Save();
        }

        // ------------------------------------------------------------------------------------------------ load

        /// What a load did: ok (anything put on), how many pieces it put on / left as they were, and a line for the status.
        public sealed class Result { public bool ok; public int changed, missing, notOwned; public bool tuningKept, bikeMissing; public string message = ""; }

        public static Result LoadRider(RiderPreset p)
        {
            var r = new Result();
            if (OpenGarageData.Pro) { r.message = "RIDE AS YOUR RIDER TO LOAD A RIDER PRESET"; return r; }
            foreach (var pick in p.gear)
            {
                int id = Resolve(pick);
                var item = id < 0 ? null : OpenGarageData.GearItems(pick.type).FirstOrDefault(i => i.id == id);
                if (item == null) { r.missing++; continue; }
                if (item.equipped) continue;
                if (!item.Available) { r.notOwned++; continue; }
                if (OpenGarageData.EquipGear(item)) r.changed++; else r.notOwned++;
            }
            r.ok = true;
            r.message = "LOADED  ·  " + p.name + Left(r);
            return r;
        }

        public static Result LoadBike(BikePreset p)
        {
            var r = new Result();
            int pv = OpenGarageData.PlayerVehicleOfModel(p.model);
            if (pv < 0) { r.bikeMissing = true; r.message = "YOU DON'T OWN THE " + BikeName(p.model) + ": BUY IT IN MY RIDES TO LOAD THIS PRESET"; return r; }
            if (OpenGarageData.Pro || OpenGarageData.Equipped != pv)
            {
                if (!OpenGarageData.SelectVehicle(pv)) { r.message = (ReflexProfile.Error ?? "THE BIKE COULD NOT BE SELECTED").ToUpperInvariant(); return r; }
                ReflexProfile.Open(); r.changed++;
            }
            var types = OpenGarageData.PartTypes(pv);
            foreach (var pick in p.parts)
            {
                var ty = types.FirstOrDefault(t => t.id == pick.type);
                if (ty.factory == null) { r.missing++; continue; }
                int id = Resolve(pick);
                var item = id < 0 ? null : OpenGarageData.PartItems(p.model, pv, pick.type).FirstOrDefault(i => i.id == id);
                if (item == null) { r.missing++; continue; }
                if (item.equipped) continue;
                if (!item.Available) { r.notOwned++; continue; }
                if (OpenGarageData.EquipPart(pv, item, ty.tuning)) { r.changed++; ReflexProfile.Open(); } else r.notOwned++;
            }
            string mod = p.mod ?? "";
            if ((BikeMods.ChoiceFor(p.model) ?? "") != mod && (mod.Length == 0 || OpenGarageData.ModsOfModel(p.model).Any(m => m.id == mod))) { BikeMods.SetChoice(p.model, mod); r.changed++; }
            else if (mod.Length > 0 && !OpenGarageData.ModsOfModel(p.model).Any(m => m.id == mod)) r.missing++;
            if (p.tuning != null && p.tuning.Length == 8 && !CurrentTuning(pv).SequenceEqual(p.tuning))
            {
                var t = p.tuning;
                bool tuned = ReflexProfile.SetTuning(pv, new ReflexGarage.Tuning { engine1 = t[0], engine2 = t[1], suspension1 = t[2], suspension2 = t[3], tire1 = t[4], tire2 = t[5], braking = t[6], steering = t[7] }).Result;
                if (tuned) { r.changed++; ReflexProfile.Open(); } else r.tuningKept = true;
            }
            r.ok = true;
            r.message = "LOADED  ·  " + p.name + Left(r) + (r.tuningKept ? "  ·  TUNING KEPT (THE PARTS GIVE OTHER POINTS NOW)" : "");
            return r;
        }
        static string Left(Result r) =>
            (r.notOwned > 0 ? "  ·  " + r.notOwned + (r.notOwned == 1 ? " PIECE" : " PIECES") + " NOT OWNED" : "") + (r.missing > 0 ? "  ·  " + r.missing + " NOT INSTALLED" : "") + (r.notOwned + r.missing > 0 ? ": LEFT AS THEY WERE" : "");

        // ------------------------------------------------------------------------------------------------ what a preset holds (the cards)

        public static string BikeName(int model)
        {
            var row = FeGarageData.Unique("VehicleModel", "VehicleModelId", model);
            return row == null ? "BIKE" : OpenGarageData.Title(FeGarageData.ProductOfAsset(FeTables.I(row, "StoreAssetId")), "BIKE").ToUpperInvariant();
        }
        /// A pick's item as the garage lists it now (null: not installed).
        public static GarageItem GearItem(Pick p) { int id = Resolve(p); return id < 0 ? null : OpenGarageData.GearItems(p.type).FirstOrDefault(i => i.id == id); }
        public static GarageItem PartItem(BikePreset b, Pick p)
        {
            int id = Resolve(p); if (id < 0) return null;
            int pv = OpenGarageData.PlayerVehicleOfModel(b.model);
            return OpenGarageData.PartItems(b.model, pv, p.type).FirstOrDefault(i => i.id == id);
        }
        /// The pieces of a preset the player could not put on now (not owned / not installed).
        public static int Unavailable(RiderPreset p) => p.gear.Count(g => GearItem(g) is GarageItem it ? !it.Available && !it.equipped : true);
        public static int Unavailable(BikePreset b) => OpenGarageData.PlayerVehicleOfModel(b.model) < 0 ? -1 : b.parts.Count(g => PartItem(b, g) is GarageItem it ? !it.Available && !it.equipped : true);

        /// The gear ids / part ids to preview (the current ones where the preset has nothing of a type, or its piece is not installed).
        public static List<int> PreviewGear(RiderPreset p)
        {
            var ids = OpenGarageData.EquippedGearIds();
            int TypeOf(int id) => KitPacks.Item(id) is KitItem k ? k.type : FeTables.I(FeGarageData.Unique("RiderGear", "RiderGearId", id), "RiderGearTypeId");
            foreach (var g in p.gear) { int id = Resolve(g); if (id < 0) continue; int k = ids.FindIndex(x => TypeOf(x) == g.type); if (k >= 0) ids[k] = id; else ids.Add(id); }
            return ids;
        }
        public static List<int> PreviewParts(BikePreset b)
        {
            int pv = OpenGarageData.PlayerVehicleOfModel(b.model);
            var ids = pv >= 0 ? OpenGarageData.EquippedPartIds(pv) : new List<int>();
            int TypeOf(int id) => KitPacks.Item(id) is KitItem k ? k.type : FeTables.I(FeGarageData.Unique("VehiclePart", "VehiclePartId", id), "VehiclePartTypeId");
            foreach (var g in b.parts) { int id = Resolve(g); if (id < 0) continue; int k = ids.FindIndex(x => TypeOf(x) == g.type); if (k >= 0) ids[k] = id; else ids.Add(id); }
            return ids;
        }

        /// Tests: forget the loaded store (the next read takes the client save again).
        public static void Reload() => s_store = null;
    }
}
