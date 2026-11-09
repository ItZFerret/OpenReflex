// GARAGE > RIDER > RIDER PRESETS and GARAGE > VEHICLE > BIKE PRESETS (GaragePresets.cs keeps them):
//   the list     SAVE CURRENT AS PRESET (names it on the keyboard), then every preset as a card: its name, what it holds, IN USE when it is
//                what the player wears / rides, NOT OWNED pieces; focused, the stage shows it on the rider / the bike
//   a preset     LOAD, RENAME, SAVE CURRENT OVER IT, DELETE (asked twice), and WHAT'S IN IT: every piece by type (one not owned or not
//                installed is marked), a bike preset's tuning and bike mod
using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed partial class OpenShell
    {
        int presetDeleteArmed = -1;   // the preset whose DELETE was pressed once (the next press deletes)

        static StyleSheet s_presetSheet;
        void PresetSheet(VisualElement on)
        {
            s_presetSheet ??= Resources.Load<StyleSheet>(OuUi.ResourceRoot + "OuPresets");
            if (s_presetSheet && !on.styleSheets.Contains(s_presetSheet)) on.styleSheets.Add(s_presetSheet);
        }

        /// The RIDER / VEHICLE level's card into the presets.
        void GaragePresetsCard(bool bike)
        {
            int n = bike ? GaragePresets.Bikes.Count : GaragePresets.Riders.Count;
            int inUse = bike ? GaragePresets.Bikes.Count(GaragePresets.InUse) : GaragePresets.Riders.Count(GaragePresets.InUse);
            string sub = (n == 0 ? "NONE SAVED YET" : n + (n == 1 ? " SAVED" : " SAVED")) + "  ·  " + (bike ? "SAVE AND SWAP WHOLE BIKE SETUPS" : "SAVE AND SWAP WHOLE LOOKS");
            var card = GarageCard(bike ? "garage-bike-presets" : "garage-rider-presets", bike ? "BIKE PRESETS" : "RIDER PRESETS", sub, true, null,
                () => GarageEnter(bike ? GKind.BikePresets : GKind.RiderPresets), inUse > 0 ? ("IN USE", "good") : default);
            card.RegisterCallback<FocusInEvent>(_ => GarageEquippedPreview(bike ? OpenGarageStage.Shot.Vehicle : OpenGarageStage.Shot.Rider, true));
        }

        // ------------------------------------------------------------------------------------------------ the list

        void GaragePresetList(bool bike)
        {
            presetDeleteArmed = -1;
            PresetSheet(garageRows);
            if (OpenGarageData.Pro) { GarageProNote(); return; }
            int count = bike ? GaragePresets.Bikes.Count : GaragePresets.Riders.Count;
            bool full = count >= GaragePresets.MaxPresets;
            string what = bike ? "THIS BIKE, ITS PARTS, TUNING AND BIKE MOD" : "EVERY PIECE YOU WEAR NOW";
            var save = GarageCard("garage-preset-new", "+  SAVE CURRENT AS PRESET", full ? "FULL: " + GaragePresets.MaxPresets + " PRESETS  ·  DELETE ONE FIRST" : what + "  ·  YOU NAME IT", false, null, () =>
            {
                if (full) { GarageStatus("YOU HAVE " + GaragePresets.MaxPresets + " PRESETS: DELETE ONE TO SAVE ANOTHER"); OuSound.Play("Locked"); return; }
                string stem = bike ? GaragePresets.BikeName(OpenGarageData.EquippedModel) : "RIDER";
                string start = GaragePresets.NextName(stem, bike ? GaragePresets.Bikes.Select(p => p.name) : GaragePresets.Riders.Select(p => p.name));
                OpenKeyboard("NAME THIS PRESET", "UP TO " + GaragePresets.MaxName + " LETTERS AND DIGITS", start, GaragePresets.MaxName, false, true, _nav.Focused, text =>
                {
                    string name = GaragePresets.CleanName(text); if (name.Length == 0) name = start;
                    int at = bike ? GaragePresets.SaveBike(name) : GaragePresets.SaveRider(name);
                    GarageChanged(at >= 0, "SAVED  ·  " + name, at >= 0 ? "garage-preset-" + at : null);
                });
            });
            save.AddToClassList("ou-preset-new");
            save.RegisterCallback<FocusInEvent>(_ => GarageEquippedPreview(bike ? OpenGarageStage.Shot.Vehicle : OpenGarageStage.Shot.Rider, false));

            OuTheme.Section(garageRows, "YOUR PRESETS" + (count > 0 ? "  ·  " + count : ""));
            if (count == 0)
            {
                GarageNote(bike ? "No bike presets yet. Set the bike up the way you like it (parts, tuning, bike mod), then SAVE CURRENT AS PRESET."
                                : "No rider presets yet. Put together a look in the gear lists, then SAVE CURRENT AS PRESET.");
                return;
            }
            for (int i = 0; i < count; i++)
            {
                int index = i;
                if (bike)
                {
                    var p = GaragePresets.Bikes[i];
                    int off = GaragePresets.Unavailable(p);
                    var pills = new List<(string, string)>();
                    if (GaragePresets.InUse(p)) pills.Add(("IN USE", "good"));
                    if (off < 0) pills.Add(("BIKE NOT OWNED", "bad")); else if (off > 0) pills.Add((off + " NOT OWNED", "warn"));
                    int product = FeGarageData.ProductOfAsset(FeTables.I(FeGarageData.Unique("VehicleModel", "VehicleModelId", p.model), "StoreAssetId"));
                    var card = GarageCard("garage-preset-" + i, p.name, GarageBikePresetLine(p), true, GarageBrandLogo(product), () => GarageEnter(GKind.Preset, 1, index), pills.ToArray());
                    card.AddToClassList("ou-preset-card");
                    card.RegisterCallback<FocusInEvent>(_ => GaragePresetPreview(p, true));
                }
                else
                {
                    var p = GaragePresets.Riders[i];
                    int off = GaragePresets.Unavailable(p);
                    var pills = new List<(string, string)>();
                    if (GaragePresets.InUse(p)) pills.Add(("IN USE", "good"));
                    if (off > 0) pills.Add((off + " NOT OWNED", "warn"));
                    var jersey = p.gear.Select(GaragePresets.GearItem).FirstOrDefault(it => it != null && FeTables.S(FeGarageData.Unique("RiderGearType", "RiderGearTypeId", it.type), "FactoryName") == "Jersey");
                    var card = GarageCard("garage-preset-" + i, p.name, GarageRiderPresetLine(p), true, GarageBrandLogo(jersey == null ? -1 : jersey.parent >= 0 ? jersey.parent : jersey.product),
                        () => GarageEnter(GKind.Preset, 0, index), pills.ToArray());
                    card.AddToClassList("ou-preset-card");
                    card.RegisterCallback<FocusInEvent>(_ => GaragePresetPreview(p, true));
                }
            }
            GarageNote(bike ? "LOADING a bike preset switches to that bike and puts on its parts, tuning and bike mod. Pieces you don't own are left as they are."
                            : "LOADING a rider preset puts on every piece it holds. Pieces you don't own (or a gear pack that is not installed) are left as they are.");
        }

        /// "HELMET · JERSEY" names of a rider preset (its helmet and jersey lines), for its card.
        string GarageRiderPresetLine(GaragePresets.RiderPreset p)
        {
            var names = new List<string>();
            foreach (string factory in new[] { "Helmet", "Jersey" })
            {
                var it = p.gear.Select(GaragePresets.GearItem).FirstOrDefault(x => x != null && FeTables.S(FeGarageData.Unique("RiderGearType", "RiderGearTypeId", x.type), "FactoryName") == factory);
                if (it != null) names.Add(string.IsNullOrEmpty(it.line) ? it.title.ToUpperInvariant() : it.line.ToUpperInvariant());
            }
            return (names.Count > 0 ? string.Join("  ·  ", names) + "  ·  " : "") + p.gear.Count + " PIECES";
        }
        string GarageBikePresetLine(GaragePresets.BikePreset p)
        {
            string tune = "";
            if (p.tuning != null && p.tuning.Length == 8)
            {
                int accel = p.tuning[0], top = p.tuning[1];
                tune = accel == top ? "" : accel > top ? "  ·  ACCELERATION TUNE" : "  ·  TOP SPEED TUNE";
            }
            // the graphics kit (11) and the exhaust: the pieces a setup is told apart by at a glance
            var names = new List<string>();
            var types = OpenGarageData.PartTypes(OpenGarageData.PlayerVehicleOfModel(p.model));
            foreach (var pick in p.parts.Where(x => x.type == 11 || types.Any(t => t.id == x.type && t.factory.IndexOf("Exh", StringComparison.OrdinalIgnoreCase) >= 0)).OrderBy(x => x.type == 11 ? 0 : 1))
            {
                var it = GaragePresets.PartItem(p, pick);
                if (it != null) names.Add(string.IsNullOrEmpty(it.line) ? it.title.ToUpperInvariant() : (it.line + " " + it.title).ToUpperInvariant());
            }
            return GaragePresets.BikeName(p.model) + (names.Count > 0 ? "  ·  " + string.Join("  ·  ", names) : "") + tune;
        }

        void GaragePresetPreview(GaragePresets.RiderPreset p, bool reset)
        {
            int model = OpenGarageData.EquippedModel, vehicle = OpenGarageData.Equipped;
            var gear = GaragePresets.PreviewGear(p);
            GarageShow("preset-rider:" + string.Join(",", gear), () => (model >= 0 ? OpenGarageData.BikeOfModel(model) : null, OpenGarageData.RiderWithGear(gear), true), OpenGarageStage.Shot.Rider, reset);
            GarageSubject(model, vehicle);
        }
        void GaragePresetPreview(GaragePresets.BikePreset p, bool reset)
        {
            var parts = GaragePresets.PreviewParts(p);
            var gear = OpenGarageData.EquippedGearIds();
            GarageShow("preset-bike:" + p.model + ":" + string.Join(",", parts), () => (OpenGarageData.BikeWithPart(p.model, parts, -1), OpenGarageData.RiderWithGear(gear), true), OpenGarageStage.Shot.Vehicle, reset);
            GarageSubject(p.model, OpenGarageData.PlayerVehicleOfModel(p.model));
        }

        // ------------------------------------------------------------------------------------------------ one preset

        bool GaragePresetPage(bool bike, int index)
        {
            PresetSheet(garageRows);
            if (OpenGarageData.Pro) { GarageProNote(); return true; }
            if (index < 0 || index >= (bike ? GaragePresets.Bikes.Count : GaragePresets.Riders.Count)) return false;
            var rider = bike ? null : GaragePresets.Riders[index];
            var bikeP = bike ? GaragePresets.Bikes[index] : null;
            string name = bike ? bikeP.name : rider.name;
            bool inUse = bike ? GaragePresets.InUse(bikeP) : GaragePresets.InUse(rider);
            void Preview(bool reset) { if (bike) GaragePresetPreview(bikeP, reset); else GaragePresetPreview(rider, reset); }

            var load = GarageCard("garage-preset-load", inUse ? "LOADED" : "LOAD PRESET", inUse ? "THIS IS WHAT YOU " + (bike ? "RIDE" : "WEAR") + " NOW" : bike ? "RIDE " + GaragePresets.BikeName(bikeP.model) + " SET UP THIS WAY" : "PUT ON EVERY PIECE OF IT", false, null, () =>
            {
                var r = bike ? GaragePresets.LoadBike(bikeP) : GaragePresets.LoadRider(rider);
                if (!r.ok) { GarageStatus(r.message); OuSound.Play("Locked"); return; }
                PortraitRefresh();
                GarageChanged(true, r.message, "garage-preset-load");
            }, inUse ? ("IN USE", "good") : default);
            load.AddToClassList("ou-preset-load");
            load.RegisterCallback<FocusInEvent>(_ => Preview(true));

            var rename = GarageCard("garage-preset-rename", "RENAME", name, false, null, () =>
                OpenKeyboard("RENAME PRESET", "UP TO " + GaragePresets.MaxName + " LETTERS AND DIGITS", name, GaragePresets.MaxName, false, true, _nav.Focused, text =>
                {
                    string n = GaragePresets.CleanName(text); if (n.Length == 0 || n == name) return;
                    GaragePresets.Rename(bike, index, n);
                    GarageChanged(true, "RENAMED  ·  " + n, "garage-preset-rename");
                }));
            rename.AddToClassList("ou-preset-action");
            rename.RegisterCallback<FocusInEvent>(_ => Preview(false));

            var over = GarageCard("garage-preset-overwrite", "SAVE CURRENT OVER IT", bike ? "THE BIKE YOU RIDE NOW, ITS PARTS, TUNING AND BIKE MOD" : "WHAT YOU WEAR NOW", false, null, () =>
            {
                int at = bike ? GaragePresets.SaveBike(name, index) : GaragePresets.SaveRider(name, index);
                GarageChanged(at >= 0, "SAVED OVER  ·  " + name, "garage-preset-overwrite");
            });
            over.AddToClassList("ou-preset-action");
            over.RegisterCallback<FocusInEvent>(_ => GarageEquippedPreview(bike ? OpenGarageStage.Shot.Vehicle : OpenGarageStage.Shot.Rider, false));

            presetDeleteArmed = -1;
            VisualElement delete = null;
            void Arm(bool on)
            {   // the card itself says it (no rebuild: the focus stays on it); leaving it disarms it
                presetDeleteArmed = on ? index : -1;
                var title = delete.Q<Label>(className: "ou-gcard-title"); var sub = delete.Q<Label>(className: "ou-gcard-sub");
                if (title != null) title.text = on ? "PRESS AGAIN TO DELETE" : "DELETE";
                if (sub != null) sub.text = on ? "THIS CAN'T BE UNDONE" : "REMOVE THIS PRESET";
                delete.EnableInClassList("ou-preset-delete-armed", on);
            }
            delete = GarageCard("garage-preset-delete", "DELETE", "REMOVE THIS PRESET", false, null, () =>
            {
                if (presetDeleteArmed != index) { Arm(true); GarageStatus("PRESS DELETE AGAIN TO REMOVE " + name); return; }
                presetDeleteArmed = -1;
                GaragePresets.Delete(bike, index);
                GarageStatus("DELETED  ·  " + name);
                GarageUp();
            });
            delete.AddToClassList("ou-preset-delete"); delete.AddToClassList("ou-preset-action");
            delete.RegisterCallback<FocusInEvent>(_ => Preview(false));
            delete.RegisterCallback<FocusOutEvent>(_ => { if (presetDeleteArmed == index) Arm(false); });

            // WHAT'S IN IT
            OuTheme.Section(garageRows, "WHAT'S IN IT");
            var box = OuUi.Div("ou-preset-contents", garageRows);
            int lines = 0;
            void Line(string label, string value, string state = null)
            {   // a focusable row (nothing to press): the pad walks down the list and the page scrolls with it
                var l = OuUi.Item(OuUi.Div("ou-preset-line", box), null);
                l.name = "garage-preset-line-" + lines++;
                l.RegisterCallback<FocusInEvent>(_ => { GarageRowFocused(l); Preview(false); });
                OuUi.Text(label, "ou-preset-line-label", l);
                OuUi.Text(value, "ou-preset-line-value", l).enableRichText = false;
                if (state != null) OuUi.Text(state, "ou-preset-line-state", l);
            }
            if (bike)
            {
                bool owned = OpenGarageData.PlayerVehicleOfModel(bikeP.model) >= 0;
                Line("BIKE", GaragePresets.BikeName(bikeP.model), owned ? null : "NOT OWNED");
                var types = OpenGarageData.PartTypes(OpenGarageData.PlayerVehicleOfModel(bikeP.model));
                foreach (var pick in bikeP.parts)
                {
                    var ty = types.FirstOrDefault(t => t.id == pick.type);
                    var it = GaragePresets.PartItem(bikeP, pick);
                    Line(ty.name ?? "PART", it == null ? "NOT INSTALLED" : GarageItemName(it), it == null ? "MISSING" : !it.Available && !it.equipped ? "NOT OWNED" : null);
                }
                if (bikeP.tuning != null && bikeP.tuning.Length == 8)
                {
                    var t = bikeP.tuning;
                    string Lean(int a, int b, string low, string high) => a == b ? "BALANCED" : a > b ? "MORE " + low : "MORE " + high;
                    Line("TUNING", "ENGINE " + Lean(t[0], t[1], "ACCELERATION", "TOP SPEED") + "  ·  BRAKING " + Level(t[6], "SOFT", "HARD") + "  ·  STEERING " + Level(t[7], "SLOW", "QUICK"));
                }
                string mod = bikeP.mod ?? "";
                if (mod.Length > 0)
                {
                    var m = OpenGarageData.ModsOfModel(bikeP.model).FirstOrDefault(x => x.id == mod);
                    Line("BIKE MOD", m != null ? m.name.ToUpperInvariant() : mod.ToUpperInvariant(), m == null ? "NOT INSTALLED" : null);
                }
            }
            else
            {
                var types = OpenGarageData.GearTypes();
                foreach (var pick in rider.gear)
                {
                    var ty = types.FirstOrDefault(t => t.id == pick.type);
                    var it = GaragePresets.GearItem(pick);
                    Line(ty.name ?? "GEAR", it == null ? "NOT INSTALLED" : GarageItemName(it), it == null ? "MISSING" : !it.Available && !it.equipped ? "NOT OWNED" : null);
                }
            }
            string saved = (bike ? bikeP.saved : rider.saved) > 0 ? DateTimeOffset.FromUnixTimeSeconds(bike ? bikeP.saved : rider.saved).LocalDateTime.ToString("d MMM yyyy, HH:mm").ToUpperInvariant() : "";
            if (saved.Length > 0) GarageNote("SAVED " + saved);
            return true;
        }
        static string Level(int v, string low, string high) => v <= 25 ? low : v >= 75 ? high : "STANDARD";
    }
}
