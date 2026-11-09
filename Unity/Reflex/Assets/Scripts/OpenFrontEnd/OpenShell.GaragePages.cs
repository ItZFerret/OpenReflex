// The levels of the open garage (OpenShell.Garage.cs is the frame and the way up and down):
//   GARAGE > VEHICLE > MY RIDES > class > vehicle             (GarageMyRides.lua: the types, then their models)
//   GARAGE > VEHICLE > part type > product line > item         (GarageVehicleCustomize.lua: part types, primary list, secondary list)
//   GARAGE > VEHICLE > ENGINE SOUND (a row) / VEHICLE TUNING
//   GARAGE > RIDER > NAME & NUMBER
//   GARAGE > RIDER > gear type > product line > item           (GarageRiderCustomize.lua: gear types, primary list, secondary list)
//   GARAGE > RIDER > RIDER PRESETS / GARAGE > VEHICLE > BIKE PRESETS > a preset   (OpenShell.GaragePresets.cs)
//   GARAGE > PRO RIDERS
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
        VisualElement garageSoundSlot; int garageSoundModel = -1;

        const int ModdedOnly = -2;   // the MY RIDES class entry of the modded bikes only (-1: every class)

        /// The rows of a level; false when it cannot be shown now (the garage then goes one up).
        bool BuildGarageLevel(GLevel l)
        {
            switch (l.kind)
            {
                case GKind.Home: GarageHomeLevel(); return true;
                case GKind.Rider: GarageRiderLevel(); return true;
                case GKind.Identity: GarageIdentityRows(); return true;
                case GKind.GearSection: return GarageGearSectionLevel(l.a);
                case GKind.GearLine: return GarageGearLineLevel(l.a, l.b);
                case GKind.ProRiders: GarageProRidersPage(); return true;
                case GKind.Vehicle: GarageVehicleLevel(); return true;
                case GKind.Classes: GarageClassesLevel(); return true;
                case GKind.Models: return GarageModelsLevel(l.a);
                case GKind.PartSection: return GaragePartSectionLevel(l.a);
                case GKind.PartLine: return GaragePartLineLevel(l.a, l.b);
                case GKind.Tuning: GarageTuningPage(); return true;
                case GKind.RiderPresets: GaragePresetList(false); return true;
                case GKind.BikePresets: GaragePresetList(true); return true;
                case GKind.Preset: return GaragePresetPage(l.a == 1, l.b);
            }
            return false;
        }

        /// A level's short name: its title, and its step in the breadcrumb.
        string GarageLevelName(GLevel l)
        {
            if (l == null) return "";
            switch (l.kind)
            {
                case GKind.Home: return OpenContent.Str("STR_GARAGE", "GARAGE").ToUpperInvariant();
                case GKind.Rider: return OpenContent.Str("STR_GARAGE_RIDER", "RIDER").ToUpperInvariant();
                case GKind.Identity: return "NAME & NUMBER";
                case GKind.GearSection: return OpenGarageData.GearTypes().FirstOrDefault(t => t.id == l.a).name ?? "GEAR";
                case GKind.PartSection: return OpenGarageData.PartTypes(OpenGarageData.Equipped).FirstOrDefault(t => t.id == l.a).name ?? "PART";
                case GKind.GearLine: case GKind.PartLine: return OpenGarageData.Title(l.b, "LINE").ToUpperInvariant();
                case GKind.ProRiders: return OpenContent.Str("STR_PRO_RIDERS", "PRO RIDERS").ToUpperInvariant();
                case GKind.Vehicle: return "VEHICLE";
                case GKind.Classes: return OpenContent.Str("STR_GARAGE_VEHICLE_MYRIDES", "MY RIDES").ToUpperInvariant();
                case GKind.Models: return l.a == ModdedOnly ? "MODDED BIKES" : l.a < 0 ? "ALL VEHICLES" : OpenGarageData.TypeName(l.a).ToUpperInvariant();
                case GKind.Tuning: return OpenContent.Str("STR_GARAGE_VEHICLE_TUNE", "VEHICLE TUNING").ToUpperInvariant();
                case GKind.RiderPresets: return "RIDER PRESETS";
                case GKind.BikePresets: return "BIKE PRESETS";
                case GKind.Preset:
                    var list = l.a == 1 ? GaragePresets.Bikes.Select(p => p.name).ToList() : GaragePresets.Riders.Select(p => p.name).ToList();
                    return l.b >= 0 && l.b < list.Count ? list[l.b] : "PRESET";
            }
            return "";
        }

        /// What a level asks for (the line under its title).
        string GarageLevelHelp(GLevel l)
        {
            switch (l?.kind)
            {
                case GKind.Home: return "CHOOSE WHAT TO CHANGE";
                case GKind.Rider: return OpenGarageData.Pro ? "RIDING AS A PRO RIDER: HIS GEAR IS HIS OWN" : "CHOOSE WHAT TO CHANGE";
                case GKind.Identity: return "ON THE MOTOCARD, THE JERSEY AND THE PLATES";
                case GKind.GearSection: case GKind.PartSection: return "CHOOSE A BRAND / MODEL";
                case GKind.GearLine: case GKind.PartLine: return "CHOOSE A COLOR / STYLE  ·  " + OuGlyphs.Retarget(OuUi.KeySubmit).ToUpperInvariant() + " TO EQUIP";
                case GKind.ProRiders: return "WHO RIDES: YOUR RIDER OR A PRO";
                case GKind.Vehicle: return OpenGarageData.Pro ? "RIDING AS A PRO RIDER: HIS VEHICLE IS HIS OWN" : "RIDING  " + GarageBikeTitle(OpenGarageData.EquippedModel, OpenGarageData.Equipped);
                case GKind.Classes: return "CHOOSE A CLASS";
                case GKind.Models: return "CHOOSE A VEHICLE  ·  " + OuGlyphs.Retarget(OuUi.KeySubmit).ToUpperInvariant() + " TO RIDE IT";
                case GKind.Tuning: return "LEFT / RIGHT SLIDES TOWARD THE WORD ON THAT SIDE  ·  SAVED AT ONCE";
                case GKind.RiderPresets: return "SAVE THE LOOK YOU WEAR NOW, OR LOAD ONE YOU SAVED";
                case GKind.BikePresets: return "SAVE THE BIKE SETUP YOU RIDE NOW, OR LOAD ONE YOU SAVED";
                case GKind.Preset: return l.a == 1 ? "BIKE PRESET  ·  LOAD, RENAME, SAVE OVER OR DELETE IT" : "RIDER PRESET  ·  LOAD, RENAME, SAVE OVER OR DELETE IT";
            }
            return "";
        }

        /// The stage of a level with nothing focused on it: the equipped setup.
        void GarageLevelIdle() => GarageEquippedPreview(GarageOnVehicle ? OpenGarageStage.Shot.Vehicle : OpenGarageStage.Shot.Overview, false);

        /// The vehicle as the player rides it: the bike mod's name when one is chosen for it, else the model's.
        static string GarageBikeTitle(int model, int playerVehicle)
        {
            var row = FeGarageData.Unique("VehicleModel", "VehicleModelId", model);
            if (row == null) return "—";
            int product = FeGarageData.ProductOfAsset(FeTables.I(row, "StoreAssetId"));
            string title = OpenGarageData.Title(product, "VEHICLE");
            var mod = playerVehicle >= 0 ? BikeMods.ChosenFor(model) : null;
            if (mod != null && OpenGarageData.ModsOfModel(model).Contains(mod)) { var n = BikeMods.NameFor(mod, model, title); if (n != null && n.model.Length > 0) title = n.model; }
            return title.ToUpperInvariant();
        }

        static string GarageItemName(GarageItem item) => item == null ? "—" : (string.IsNullOrEmpty(item.line) ? item.title : item.line + "  ·  " + item.title).ToUpperInvariant();
        static Texture2D GarageBrandLogo(int product) { int icon = product < 0 ? -1 : OpenGarageData.BrandIcon(product); return icon >= 0 ? OpenContent.Icon(icon) : null; }

        // ================================================================================================ GARAGE (home)

        void GarageHomeLevel()
        {
            bool pro = OpenGarageData.Pro;
            PortraitRefresh();
            int model = OpenGarageData.EquippedModel;
            var mrow = FeGarageData.Unique("VehicleModel", "VehicleModelId", model);
            string vehicleLine = mrow == null ? "NO VEHICLE" : GarageBikeTitle(model, pro ? -1 : OpenGarageData.Equipped) + "  ·  " + OpenGarageData.TypeName(FeTables.I(mrow, "VehicleTypeId")).ToUpperInvariant();
            var veh = GarageCard("garage-h-vehicle", "VEHICLE", vehicleLine + "  ·  PARTS, ENGINE SOUND, TUNING", true, null, () => GarageEnter(GKind.Vehicle));
            veh.RegisterCallback<FocusInEvent>(_ => GarageEquippedPreview(OpenGarageStage.Shot.Vehicle, true));
            string number = OpenGarageData.Tab("PlayerRider").Str("Number", 0);
            string riderLine = pro ? "PRO RIDER  ·  " + GarageRidingAs()
                : (ReflexProfile.HasPersona ? ReflexProfile.Persona : "YOUR RIDER").ToUpperInvariant() + (string.IsNullOrEmpty(number) ? "" : "  #" + number) + "  ·  " + GarageWearing();
            var rider = GarageCard("garage-h-rider", OpenContent.Str("STR_GARAGE_RIDER", "RIDER").ToUpperInvariant(), riderLine, true, null, () => GarageEnter(GKind.Rider));
            rider.RegisterCallback<FocusInEvent>(_ => GarageEquippedPreview(OpenGarageStage.Shot.Rider, true));
            var pros = GarageCard("garage-h-pro", OpenContent.Str("STR_PRO_RIDERS", "PRO RIDERS").ToUpperInvariant(), "RIDING AS  " + GarageRidingAs(), true, null, () => GarageEnter(GKind.ProRiders),
                pro ? ("PRO", "gold") : default);
            pros.RegisterCallback<FocusInEvent>(_ => GarageEquippedPreview(OpenGarageStage.Shot.Overview, true));

            OuTheme.Section(garageRows, "PROFILE");
            GarageMotocardCard();
            var unlock = OuUi.Toggle("UNLOCK EVERYTHING", () => CoreGarageProfile.UnlockEverythingEnabled, SetGarageUnlocks, garageRows);
            unlock.name = "garage-unlock-everything";
            unlock.RegisterCallback<FocusInEvent>(_ => { GarageRowFocused(unlock); GarageEquippedPreview(OpenGarageStage.Shot.Overview, false); });
            GarageNote(CoreGarageProfile.UnlockEverythingEnabled
                ? "UNLOCKED PROFILE · Turn off to return to your saved career."
                : "CAREER PROFILE · Unlock mode keeps your earned progress separate.");
        }

        /// Who rides: the player's own rider or the pro rider (and his vehicle).
        string GarageRidingAs()
        {
            if (!OpenGarageData.Pro) return "YOUR RIDER";
            var cur = OpenGarageData.ProRides().FirstOrDefault(r => r.rider == OpenGarageData.ProId && r.vehicle == OpenGarageData.Equipped);
            return cur?.name ?? "PRO RIDER";
        }

        /// The MOTOCARD card: the player's portrait with his name, jersey number and credits; A opens the MOTOCARD, its B comes back here.
        void GarageMotocardCard()
        {
            var card = OuUi.Item(OuUi.Div("ou-gmoto", garageRows), () => ShowMotocard(MotocardStats, ResumeGarage));
            card.name = "garage-motocard";
            card.RegisterCallback<FocusInEvent>(_ => { GarageRowFocused(card); GarageEquippedPreview(OpenGarageStage.Shot.Overview, false); });
            PortraitFrame(card, "garage-motocard-portrait");
            var copy = OuUi.Div("ou-gmoto-copy", card);
            OuUi.Text((ReflexProfile.HasPersona ? ReflexProfile.Persona : "YOUR RIDER").ToUpperInvariant(), "ou-gmoto-name", copy);
            var line = OuUi.Div("ou-gmoto-line", copy);
            OuUi.Text("#" + (OpenGarageData.Tab("PlayerRider").Str("Number", 0) ?? "998"), "ou-gmoto-number", line);
            OuUi.Text(OpenGarageData.Unlocked ? "ALL UNLOCKED" : OpenGarageData.Balance + " CREDITS", "ou-gmoto-credits", line);
            OuUi.Text(OpenGarageData.Pro ? "RIDING AS A PRO — YOUR OWN RIDER SHOWN" : "STATS · ACHIEVEMENTS · RECORDS · TRICKS", "ou-gmoto-sub", copy);
            OuUi.Text("OPEN MOTOCARD  ›", "ou-gmoto-open", copy);
        }

        void SetGarageUnlocks(bool enabled)
        {
            if (!CoreGarageProfile.SetUnlockEverything(enabled)) { GarageStatus((CoreGarageProfile.Error ?? "The profile could not be switched.").ToUpperInvariant()); return; }
            ReflexProfile.Open();
            if (garageStack.Count > 0) garageStack[garageStack.Count - 1].focus = "garage-unlock-everything";
            ResumeGarage();
        }

        // ================================================================================================ the stage's subjects

        /// The stage shows the equipped setup (a pro's when one rides), framed by shot.
        void GarageEquippedPreview(OpenGarageStage.Shot shot, bool reset) => GarageRiderPreviewCore(null, false, shot, reset);
        /// The stage for a pro rider's card (null: your own rider).
        void GarageRiderPreview(OpenGarageData.ProRide pro) => GarageRiderPreviewCore(pro, pro == null, OpenGarageStage.Shot.Overview, false);
        void GarageRiderPreview() => GarageRiderPreviewCore(null, false, OpenGarageStage.Shot.Overview, false);
        void GarageRiderPreviewCore(OpenGarageData.ProRide ride, bool own, OpenGarageStage.Shot shot, bool reset)
        {
            if (ride != null)
            {
                GarageShow("pro:" + ride.rider + ":" + ride.vehicle, () => { var (b, r) = OpenGarageData.ProSpecs(ride); return (b, r, true); }, shot, reset);
                GarageSubject(ride.model, -1);
                return;
            }
            int vehicle = own && OpenGarageData.Pro ? OpenGarageData.FirstOwnVehicle() : OpenGarageData.Equipped;
            bool proNow = OpenGarageData.Pro && !own;
            if (proNow)
            {
                var cur = OpenGarageData.ProRides().FirstOrDefault(r => r.rider == OpenGarageData.ProId && r.vehicle == OpenGarageData.Equipped);
                if (cur != null) { GarageRiderPreviewCore(cur, false, shot, reset); return; }
            }
            var pv = OpenGarageData.Tab("PlayerVehicle"); int row = pv.Find("PlayerVehicleId", vehicle);
            int model = row < 0 ? -1 : pv.Int("VehicleModelId", row);
            var gear = OpenGarageData.EquippedGearIds();
            GarageShow("own:" + model + ":" + string.Join(",", gear) + ":" + string.Join(",", OpenGarageData.EquippedPartIds(vehicle)),
                () => (model >= 0 ? OpenGarageData.BikeOfModel(model) : null, OpenGarageData.RiderWithGear(gear), true), shot, reset);
            GarageSubject(model, vehicle);
        }

        // ================================================================================================ RIDER

        void GarageRiderLevel()
        {
            bool pro = OpenGarageData.Pro;
            string number = OpenGarageData.Tab("PlayerRider").Str("Number", 0) ?? "";
            string jersey = OpenGarageData.Tab("PlayerRider").Str("Name", 0) ?? "";
            var id = GarageCard("garage-identity", "NAME & NUMBER",
                (ReflexProfile.HasPersona ? ReflexProfile.Persona : "YOUR RIDER").ToUpperInvariant() + "  ·  #" + number + (jersey.Length > 0 ? "  ·  JERSEY " + jersey.ToUpperInvariant() : ""),
                true, null, () => GarageEnter(GKind.Identity));
            id.RegisterCallback<FocusInEvent>(_ => GarageEquippedPreview(OpenGarageStage.Shot.Rider, true));
            if (!pro) GaragePresetsCard(false);
            OuTheme.Section(garageRows, OpenContent.Str("STR_CUSTOMIZE", "CUSTOMIZE").ToUpperInvariant() + " " + OpenContent.Str("STR_GARAGE_RIDER", "RIDER").ToUpperInvariant());
            if (pro) { GarageProNote(); return; }
            var types = OpenGarageData.GearTypes();
            if (types.Count == 0) { GarageNote("This rider has no gear to change."); return; }
            GarageKitNotes(false);
            foreach (var ty in types)
            {
                var type = ty;
                int worn = OpenGarageData.EquippedGear(type.id);
                var items = OpenGarageData.GearItems(type.id);
                var item = items.FirstOrDefault(i => i.id == worn);
                string kit = type.factory == "Jersey" ? "JERSEY, PANTS & GLOVES  ·  " : "";
                var card = GarageCard("garage-gear-" + type.id, type.name, kit + GarageItemName(item), true, GarageBrandLogo(item?.parent >= 0 ? item.parent : item?.product ?? -1),
                    () => GarageEnter(GKind.GearSection, type.id), items.Any(i => i.isNew && !i.locked) ? ("NEW", "warn") : default);
                card.RegisterCallback<FocusInEvent>(_ =>
                {
                    GarageGearEquippedPreview(type.id, type.factory, true);
                    if (item != null) GarageItemSubject(item, type.name, false);
                });
            }
        }

        void GarageProNote()
        {
            GarageNote("A PRO RIDER KEEPS HIS OWN GEAR, PARTS AND TUNING. Ride as yourself to change yours.");
            var own = OuUi.Button("RIDE AS YOUR RIDER", "mxa-main-button", () =>
            {
                int v = OpenGarageData.FirstOwnVehicle();
                if (v >= 0) GarageChanged(OpenGarageData.SelectVehicle(v), "RIDING AS YOUR RIDER", "garage-own-rider");
            }, garageRows);
            own.name = "garage-own-rider";
            own.RegisterCallback<FocusInEvent>(_ => { GarageRowFocused(own); GarageRiderPreview(); });
        }

        void GarageGearEquippedPreview(int type, string factory, bool reset)
        {
            var ids = OpenGarageData.EquippedGearIds();
            GarageShow("gear:" + string.Join(",", ids), () => (null, OpenGarageData.RiderWithGear(ids), false), GearShot(factory), reset);
        }

        void GarageGearItemPreview(int type, string factory, GarageItem item)
        {
            var ids = OpenGarageData.EquippedGearIds();
            GarageShow("gear:" + string.Join(",", ids) + ":" + type + ":" + item.id, () => (null, OpenGarageData.RiderWithGear(ids, item.id), false), GearShot(factory), false);
        }

        /// A gear type's brands / models (the original's primary store list): product lines lead down to their items, a product without a line is
        /// an item here.
        bool GarageGearSectionLevel(int type)
        {
            if (OpenGarageData.Pro) return false;
            var ty = OpenGarageData.GearTypes().FirstOrDefault(t => t.id == type);
            if (ty.name == null) return false;
            var groups = OpenGarageData.Groups(OpenGarageData.GearItems(type));
            if (groups.Count == 0) { GarageNote("Nothing to wear here."); return true; }
            foreach (var g in groups)
            {
                if (!g.IsLine)
                {
                    GarageItemCard(g.item, ty.name, false, OpenGarageData.EquipGear, it => GarageGearItemPreview(type, ty.factory, it));
                    continue;
                }
                var grp = g;
                var worn = grp.items.FirstOrDefault(i => i.equipped);
                var card = GarageLineCard(grp, worn, () => GarageEnter(GKind.GearLine, type, grp.line));
                card.RegisterCallback<FocusInEvent>(_ =>
                {
                    GarageGearEquippedPreview(type, ty.factory, false);
                    GarageLineSubject(grp, ty.name, worn, false);
                });
            }
            return true;
        }

        /// The items of a gear product line (the original's secondary list): the colours / styles; focused = previewed, A = equip or buy.
        bool GarageGearLineLevel(int type, int line)
        {
            if (OpenGarageData.Pro) return false;
            var ty = OpenGarageData.GearTypes().FirstOrDefault(t => t.id == type);
            if (ty.name == null) return false;
            var items = OpenGarageData.GearItems(type).Where(i => i.parent == line).ToList();
            if (items.Count == 0) return false;
            foreach (var it in items) GarageItemCard(it, ty.name, false, OpenGarageData.EquipGear, x => GarageGearItemPreview(type, ty.factory, x), true);
            return true;
        }

        // ================================================================================================ lines and items (gear and parts)

        /// A product line's card: its name and brand logo, how many styles it has and how many are yours, EQUIPPED when the equipped one is in it.
        VisualElement GarageLineCard(GarageGroup g, GarageItem worn, Action enter, bool mod = false)
        {
            int owned = g.Owned;
            string sub = g.items.Count + (g.items.Count == 1 ? " STYLE" : " STYLES") + (owned == g.items.Count ? "  ·  ALL OWNED" : owned > 0 ? "  ·  " + owned + " OWNED" : "")
                         + (worn != null ? "  ·  " + worn.title.ToUpperInvariant() : "");
            var pills = new List<(string, string)>();
            if (worn != null) pills.Add(("EQUIPPED", "good"));
            if (g.AnyNew) pills.Add(("NEW", "warn"));
            if (g.AnyDlc) pills.Add(("DLC", null));
            if (mod || g.items.Any(i => i.kit != null && !i.kit.pack.builtIn)) pills.Add(("MOD", "mod"));   // (a gear / kit pack's pieces in the line; the built-in GEICO kits are DLC)
            var card = GarageCard("garage-line-" + g.line, g.title.ToUpperInvariant(), sub, true, GarageBrandLogo(g.line), enter, pills.ToArray());
            card.EnableInClassList("ou-gcard-off", owned == 0);
            return card;
        }

        /// The stage's card for a product line in view: the equipped piece when it is one of the line's, else the line (its styles, its price).
        void GarageLineSubject(GarageGroup g, string section, GarageItem worn, bool part)
        {
            if (worn != null) { GarageItemSubject(worn, section, part); return; }
            if (garageMake == null) return;
            if (part) GarageSubject(OpenGarageData.EquippedModel, OpenGarageData.Equipped); else { garageStats.Clear(); garageSubjectModel = -1; }
            garageBadges.Clear();
            garageMake.text = section;
            garageModelName.text = g.title.ToUpperInvariant();
            OuUi.Text(g.items.Count + (g.items.Count == 1 ? " STYLE" : " STYLES"), "ou-badge", garageBadges);
            int from = g.items.Where(i => !i.Available && !i.locked).Select(i => i.price).DefaultIfEmpty(-1).Min();
            if (g.Owned > 0) OuUi.Text(g.Owned + " OWNED", "ou-badge", garageBadges);
            else if (from > 0) OuUi.Text("FROM " + from.ToString("N0") + " CREDITS", "ou-badge", garageBadges);
            var logo = GarageBrandLogo(g.line);
            if (logo) { var l = OuUi.Div("ou-stage-brand", garageBadges); l.style.backgroundImage = logo; }
        }

        /// An item's card (a piece of gear or a part): EQUIPPED / OWNED / its price / LOCKED; focused = preview(item) on the stage, A = equip
        /// (or the purchase, then equip). inLine: the line's own name is in the title above, the card says the item only.
        VisualElement GarageItemCard(GarageItem item, string section, bool part, Func<GarageItem, bool> equip, Action<GarageItem> preview, bool inLine = false, bool mod = false)
        {
            var pills = new List<(string, string)>();
            if (item.equipped) pills.Add(("EQUIPPED", "good"));
            else if (item.Available) pills.Add(("OWNED", null));
            else pills.Add(item.locked ? ("LOCKED", "bad") : (item.price.ToString("N0"), "gold"));
            if (item.isNew && !item.locked) pills.Add(("NEW", "warn"));
            if (item.dlc) pills.Add(("DLC", null));
            mod |= item.kit != null && !item.kit.pack.builtIn;   // a gear / kit pack's piece (KitPacks): tagged MOD, its pack named under it (the built-in GEICO kits: DLC only)
            if (mod) pills.Add(("MOD", "mod"));
            string sub = inLine ? null : (!string.IsNullOrEmpty(item.line) ? item.line : OpenGarageData.BrandName(item.product))?.ToUpperInvariant();
            if (item.kit != null && !item.kit.pack.builtIn)
            {   // (a long folder name is cut: "ALPINESTARSHUNTERLAWRENCELEPURPLEGEAR" ran under the pills)
                string pack = item.kit.pack.name.ToUpperInvariant(); if (pack.Length > 28) pack = pack.Substring(0, 27).TrimEnd() + "…";
                sub = (sub == null ? "" : sub + "  ·  ") + pack;
            }
            var card = GarageCard("garage-item-" + item.id, item.title.ToUpperInvariant(), sub, false, inLine ? null : GarageBrandLogo(item.product), () => GarageItemAccept(item, equip), pills.ToArray());
            card.EnableInClassList("ou-gcard-off", !item.Available);
            if (mod) card.AddToClassList("ou-gcard-mod");
            card.RegisterCallback<FocusInEvent>(_ =>
            {
                preview(item);
                GarageItemSubject(item, section, part);
                if (item.isNew && !item.locked) { item.isNew = false; ReflexProfile.Run("store_not_new " + item.product); }
            });
            return card;
        }

        void GarageItemAccept(GarageItem item, Func<GarageItem, bool> equip)
        {
            string card = "garage-item-" + item.id;
            if (item.Available)
            {
                if (item.equipped) { GarageStatus("ALREADY EQUIPPED  ·  " + item.title.ToUpperInvariant()); return; }
                bool ok = equip(item);
                if (ok && !GarageOnVehicle) PortraitRefresh();
                GarageChanged(ok, "EQUIPPED  ·  " + GarageItemName(item), card);
                return;
            }
            // (the item was read before the purchase: it is owned now, so the equip, which takes only an owned item, takes it)
            GaragePurchase(item, bought => { bought.owned = true; equip(bought); if (!GarageOnVehicle) PortraitRefresh(); if (GTop != null) GTop.focus = card; });
        }

        // ================================================================================================ NAME & NUMBER

        /// A text row: the label, a field; Enter / A on the row types into it. Nothing is applied while typing (the field is delayed: no value event
        /// per key, so a keystroke costs only the text box itself); Enter, leaving the box (another row, a click elsewhere, the pad) keeps the text and
        /// applies it once through set (cleaned: letters / digits, upper case; an empty box keeps the old text), Esc / B drops it. This is what a
        /// save write + profile reload + plates / portrait rebuild per keystroke used to cost.
        void GarageTextRow(string label, string name, Func<string> get, bool spaces, Action<string> set, int max)
        {
            var row = OuUi.Div("mxa-row", garageRows);
            row.name = name;
            OuUi.Text(label, "mxa-row-label", row);
            var field = new TextField { value = get() ?? "", maxLength = max, isDelayed = true };
            field.AddToClassList("mxa-path"); field.AddToClassList("mxa-rider-name");
            row.Add(field);
            bool typing = false;

            void End(bool keep, bool refocus)
            {
                if (!typing) return;
                typing = false; garageTyping = null; garageTypeCancel = null; garageTypeEnded = Time.frameCount;
                string now = get() ?? "", v = keep ? CleanLetters(field.text, spaces) : "";
                if (v.Length > 0 && v != now) set(v);
                field.SetValueWithoutNotify(get() ?? now);   // (cleaned / emptied / dropped: the box shows the text that holds)
                GarageHints();
                if (refocus) row.Focus();
            }
            field.RegisterCallback<FocusInEvent>(_ =>
            {
                if (typing) return;
                typing = true; garageTyping = row; garageTypeCancel = () => End(false, true);
                Hints((OuUi.KeySubmit, "KEEP"), (OuUi.KeyCancel, "CANCEL"));
            });
            field.RegisterCallback<FocusOutEvent>(_ => End(true, false));
            field.RegisterCallback<KeyDownEvent>(e =>
            {
                if (e.keyCode == KeyCode.Return || e.keyCode == KeyCode.KeypadEnter) { End(true, true); e.StopPropagation(); }
                else if (e.keyCode == KeyCode.Escape) { End(false, true); e.StopPropagation(); }
            }, TrickleDown.TrickleDown);
            // a click in the box belongs to the box: the row's own click (focus the row, then press it) would take the focus from the text input
            // the pointer-down just gave it and put it back, losing the caret the click placed
            field.RegisterCallback<ClickEvent>(e => e.StopPropagation());
            // the pad has no keys to type with: its A opens the on-screen keyboard on the text (the same cleaning and apply as Enter in the box)
            void PadType()
            {
                End(true, false);   // (the box was being typed in: what it holds is applied first)
                OpenKeyboard(label, GarageKeyboardNote(spaces, max), get() ?? "", max, false, spaces, row, text =>
                {
                    string now = get() ?? "", v = CleanLetters(text, spaces);
                    if (v.Length > 0 && v != now) set(v);
                    field.SetValueWithoutNotify(get() ?? now);
                    GarageHints();
                }, () => GarageHints());
            }
            row.RegisterCallback<ClickEvent>(_ => garageClickFrame = Time.frameCount, TrickleDown.TrickleDown);
            OuUi.Item(row, () =>
            {
                if (Time.frameCount - garageTypeEnded <= 1) return;
                if (OuMenuInput.GamepadLast && garageClickFrame != Time.frameCount) PadType(); else field.Q("unity-text-input")?.Focus();
            });
            row.RegisterCallback<FocusInEvent>(_ => { GarageRowFocused(row); GarageEquippedPreview(OpenGarageStage.Shot.Rider, false); });
        }

        /// What the on-screen keyboard of a name says it takes.
        static string GarageKeyboardNote(bool spaces, int max) => "UP TO " + max + (spaces ? " LETTERS, DIGITS, SPACES, - AND _" : " LETTERS OR DIGITS");

        /// The jersey name / number changed: the plates on the stage's models (the garage's, or the hub's hero in the menu world) are made again
        /// at once, and the MOTOCARD portrait is redrawn.
        void GaragePlatesChanged()
        {
            garagePreview?.RefreshPlates();
            PortraitRefresh();
        }

        static string CleanLetters(string s, bool spaces) => new string((s ?? "").ToUpperInvariant().Where(c => char.IsLetterOrDigit(c) || spaces && (c == ' ' || c == '-' || c == '_')).ToArray()).Trim();

        /// NAME (the motocard's name the hub and the results show), JERSEY NAME (8 letters / digits on the jersey) and NUMBER (the jersey's
        /// and the plates', 1..999): the PlayerRider row's Name / Number as the motocard's pages write them.
        void GarageIdentityRows()
        {
            GarageTextRow("NAME", "garage-name", () => ReflexProfile.HasPersona ? ReflexProfile.Persona : "", true, v =>
            {
                string was = ReflexProfile.HasPersona ? ReflexProfile.Persona : null;
                ReflexProfile.Persona = v; GarageStageHeader();
            }, 16);
            GarageTextRow("JERSEY NAME", "garage-jersey-name", () => OpenGarageData.Tab("PlayerRider").Str("Name", 0) ?? "", false, v =>
            {
                if (!CoreGarageProfile.SetRiderText("Name", v)) { GarageStatus((CoreGarageProfile.Error ?? "The name could not be saved.").ToUpperInvariant()); return; }
                ReflexProfile.Open(); GarageStatus("JERSEY NAME SAVED"); GarageStageHeader(); GaragePlatesChanged();
            }, 8);
            GarageNumberRow();
            GarageHideNumberRow("HIDE NUMBER · BIKE", "garage-hide-bike-number", () => ReflexPlateText.HideBikeSetting, v => ReflexPlateText.HideBikeSetting = v, OpenGarageStage.Shot.Front, "BIKE NUMBER");
            GarageHideNumberRow("HIDE NUMBER · GEAR", "garage-hide-gear-number", () => ReflexPlateText.HideRiderSetting, v => ReflexPlateText.HideRiderSetting = v, OpenGarageStage.Shot.Torso, "GEAR NUMBER");
            GarageNote("NAME is the motocard's (the hub, the results, the leaderboards). JERSEY NAME and NUMBER are printed on your jersey and plates. HIDE NUMBER leaves the number off your bike's plates or your jersey / chest protector (the jersey name stays), in races too.");
        }

        /// HIDE NUMBER (bike / gear): the client save's choice (ReflexPlateText.HideBikeSetting / HideRiderSetting), drawn at once on the stage
        /// (focused: the bike's front plate / the rider's chest); the race reads it from the plates' text.
        void GarageHideNumberRow(string label, string name, Func<bool> get, Action<bool> set, OpenGarageStage.Shot shot, string what)
        {
            var row = OuUi.Toggle(label, get, v => { set(v); GaragePlatesChanged(); GarageStatus(what + (v ? " HIDDEN" : " SHOWN")); }, garageRows, "HIDDEN", "SHOWN");
            row.name = name;
            row.RegisterCallback<FocusInEvent>(_ => { GarageRowFocused(row); GarageEquippedPreview(shot, false); });
        }

        public const int RiderNumberMin = 1, RiderNumberMax = 999;
        /// The jersey number as the player's row stores it (a number outside 1..999 is what an older save held; the box shows it as it is).
        static int GarageNumberNow() => int.TryParse(OpenGarageData.Tab("PlayerRider").Str("Number", 0), out int n) ? n : 998;
        static int ClampRiderNumber(int n) => Mathf.Clamp(n, RiderNumberMin, RiderNumberMax);
        /// What a typed box holds as a number: digits only, 1..999 (null when there is no digit at all).
        internal static int? ParseRiderNumber(string text)
        {
            string digits = new string((text ?? "").Where(char.IsDigit).ToArray());
            if (digits.Length == 0) return null;
            return digits.Length > 4 ? RiderNumberMax : ClampRiderNumber(int.Parse(digits));
        }

        /// NUMBER: a box that takes digits, 1..999. A / Enter on the row types into it (Enter or leaving it keeps the number, Esc / B drops
        /// what was typed; an empty box goes back to the number it had); left / right step the number (held, faster) so a pad needs no keyboard.
        /// It reaches the game exactly as before: CoreGarageProfile.SetRiderText("Number", n) on the PlayerRider row.
        void GarageNumberRow()
        {
            var row = OuUi.Div("mxa-row", garageRows);
            row.name = "garage-number";
            OuUi.Text("NUMBER", "mxa-row-label", row);
            var right = OuUi.Div("mxa-row-value", row);
            var prev = OuUi.Text("‹", "mxa-arrow", right);
            var field = new TextField { value = GarageNumberNow().ToString(), maxLength = 3 };
            field.AddToClassList("mxa-path"); field.AddToClassList("mxa-rider-name"); field.AddToClassList("ou-gnumber");
            right.Add(field);
            field.RegisterCallback<ClickEvent>(e => e.StopPropagation());   // (a click in the box is the box's: the row's click would take the focus from it and give it back)
            var next = OuUi.Text("›", "mxa-arrow", right);
            bool typing = false; float last = -10f; int streak = 0;

            bool Save(int n)
            {
                if (!CoreGarageProfile.SetRiderText("Number", n.ToString())) { GarageStatus((CoreGarageProfile.Error ?? "The number could not be saved.").ToUpperInvariant()); return false; }
                ReflexProfile.Open(); GarageStatus("NUMBER SAVED"); GarageStageHeader(); GaragePlatesChanged();
                return true;
            }
            void Step(int d)
            {
                // holding left / right speeds up: 1, then 5 after a dozen steps, then 25
                float now = Time.unscaledTime; streak = now - last < 0.45f ? streak + 1 : 0; last = now;
                int step = streak > 30 ? 25 : streak > 12 ? 5 : 1;
                int span = RiderNumberMax - RiderNumberMin + 1;
                int n = ((ClampRiderNumber(GarageNumberNow()) - RiderNumberMin + d * step) % span + span) % span + RiderNumberMin;
                if (Save(n)) field.SetValueWithoutNotify(n.ToString());
            }
            void End(bool keep, bool refocus)
            {
                if (!typing) return;
                typing = false; garageTyping = null; garageTypeCancel = null; garageTypeEnded = Time.frameCount;
                int? n = keep ? ParseRiderNumber(field.value) : null;
                if (n.HasValue && n.Value != GarageNumberNow()) Save(n.Value);
                field.SetValueWithoutNotify(GarageNumberNow().ToString());   // (clamped / emptied / dropped: the box shows the number that holds)
                GarageHints();
                if (refocus) row.Focus();
            }
            // digits only while typing
            field.RegisterValueChangedCallback(e =>
            {
                string digits = new string((e.newValue ?? "").Where(char.IsDigit).ToArray());
                if (digits != e.newValue) field.SetValueWithoutNotify(digits);
            });
            field.RegisterCallback<FocusInEvent>(_ =>
            {
                if (typing) return;
                typing = true; garageTyping = row; garageTypeCancel = () => End(false, true);
                Hints((OuUi.KeySubmit, "KEEP NUMBER"), (OuUi.KeyCancel, "CANCEL"));
                field.schedule.Execute(() => { if (typing) field.SelectAll(); }).ExecuteLater(1);
            });
            field.RegisterCallback<FocusOutEvent>(_ => End(true, false));
            field.RegisterCallback<KeyDownEvent>(e =>
            {
                if (e.keyCode == KeyCode.Return || e.keyCode == KeyCode.KeypadEnter) { End(true, true); e.StopPropagation(); }
                else if (e.keyCode == KeyCode.Escape) { End(false, true); e.StopPropagation(); }
            }, TrickleDown.TrickleDown);
            prev.pickingMode = next.pickingMode = PickingMode.Position;
            prev.RegisterCallback<ClickEvent>(e => { OuSound.Play("LeftRight"); Step(-1); e.StopPropagation(); });
            next.RegisterCallback<ClickEvent>(e => { OuSound.Play("LeftRight"); Step(+1); e.StopPropagation(); });
            // the pad's A: the on-screen keyboard's number pad (left / right still step the number)
            void PadType()
            {
                End(true, false);
                OpenKeyboard("JERSEY NUMBER", RiderNumberMin + " TO " + RiderNumberMax, GarageNumberNow().ToString(), 3, true, false, row, text =>
                {
                    int? n = ParseRiderNumber(text);
                    if (n.HasValue && n.Value != GarageNumberNow()) Save(n.Value);
                    field.SetValueWithoutNotify(GarageNumberNow().ToString());
                    GarageHints();
                }, () => GarageHints());
            }
            row.RegisterCallback<ClickEvent>(_ => garageClickFrame = Time.frameCount, TrickleDown.TrickleDown);
            OuUi.Item(row, () =>
            {
                if (Time.frameCount - garageTypeEnded <= 1) return;
                if (OuMenuInput.GamepadLast && garageClickFrame != Time.frameCount) PadType(); else field.Q("unity-text-input")?.Focus();
            }, Step);
            row.RegisterCallback<FocusInEvent>(_ => { GarageRowFocused(row); GarageEquippedPreview(OpenGarageStage.Shot.Rider, false); });
        }

        // ================================================================================================ PRO RIDERS

        /// Who rides - the player's own rider, then every pro rider with his vehicles.
        void GarageProRidersPage()
        {
            bool pro = OpenGarageData.Pro;
            PortraitRefresh();
            OuTheme.Section(garageRows, "RIDING AS");
            int ownVehicle = OpenGarageData.FirstOwnVehicle();
            if (ownVehicle >= 0)
            {
                var own = GarageCard("garage-own-rider", "YOUR RIDER", "YOUR GEAR  ·  YOUR VEHICLES  ·  YOUR TUNING",
                    () => GarageChanged(OpenGarageData.SelectVehicle(ownVehicle), "RIDING AS YOUR RIDER", "garage-own-rider"), pro ? default : ("EQUIPPED", "good"));
                own.RegisterCallback<FocusInEvent>(_ => GarageRiderPreview(null));
            }
            var rides = OpenGarageData.ProRides();
            if (rides.Count > 0) OuTheme.Section(garageRows, OpenContent.Str("STR_PRO_RIDERS", "PRO RIDERS").ToUpperInvariant());
            foreach (var ride in rides)
            {
                var r = ride;
                bool current = pro && OpenGarageData.ProId == r.rider && OpenGarageData.Equipped == r.vehicle;
                var card = GarageCard("garage-pro-" + r.rider + "-" + r.vehicle, r.name, OpenGarageData.TypeName(r.vehicleType), () => GarageChanged(OpenGarageData.SelectPro(r), "RIDING AS " + r.name, "garage-pro-" + r.rider + "-" + r.vehicle),
                    current ? ("EQUIPPED", "good") : default);
                card.RegisterCallback<FocusInEvent>(_ => GarageRiderPreview(r));
            }
        }

        // ================================================================================================ VEHICLE

        void GarageVehicleLevel()
        {
            bool pro = OpenGarageData.Pro;
            int vehicle = OpenGarageData.Equipped, model = OpenGarageData.EquippedModel;
            var mrow = FeGarageData.Unique("VehicleModel", "VehicleModelId", model);
            int product = mrow == null ? -1 : FeGarageData.ProductOfAsset(FeTables.I(mrow, "StoreAssetId"));
            string riding = mrow == null ? "NO VEHICLE" : GarageBikeTitle(model, pro ? -1 : vehicle) + "  ·  " + OpenGarageData.TypeName(FeTables.I(mrow, "VehicleTypeId")).ToUpperInvariant();
            var rides = GarageCard("garage-my-rides", OpenContent.Str("STR_GARAGE_VEHICLE_MYRIDES", "MY RIDES").ToUpperInvariant(), (pro ? "PRO'S VEHICLE  ·  " : "RIDING  ") + riding + "  ·  CHANGE, BUY, BIKE MODS",
                true, GarageBrandLogo(product), () => GarageEnter(GKind.Classes));
            rides.RegisterCallback<FocusInEvent>(_ => GarageEquippedPreview(OpenGarageStage.Shot.Vehicle, true));
            if (pro) { GarageProNote(); return; }
            GaragePresetsCard(true);

            // ---- ENGINE SOUND (the audio work's per-bike picker: this bike's own choice)
            if (OpenGarageHooks.EngineSound != null && model >= 0)
            {
                garageSoundSlot = OuUi.Div("ou-gsound", garageRows);
                garageSoundSlot.RegisterCallback<FocusInEvent>(e => { if (e.target is VisualElement t && t.userData is OuNavItem) GarageRowFocused(t); GarageEquippedPreview(OpenGarageStage.Shot.Rear, false); });
                GarageHostSound(vehicle, model);
            }
            var tune = GarageCard("garage-tuning", OpenContent.Str("STR_GARAGE_VEHICLE_TUNE", "VEHICLE TUNING").ToUpperInvariant(), "ENGINE  ·  SUSPENSION  ·  TIRES  ·  BRAKING  ·  STEERING", true, null, () => GarageEnter(GKind.Tuning));
            tune.RegisterCallback<FocusInEvent>(_ => GarageEquippedPreview(OpenGarageStage.Shot.Vehicle, false));

            OuTheme.Section(garageRows, OpenContent.Str("STR_GARAGE_VEHICLE_CUSTOMIZE", "CUSTOMIZE VEHICLE").ToUpperInvariant());
            var types = OpenGarageData.PartTypes(vehicle);
            if (types.Count == 0) { GarageNote("This vehicle has no parts to change."); return; }
            var mod = BikeMods.ChosenFor(model);
            var tables = mod != null ? OpenGarageData.PartTables() : null;
            foreach (var ty in types)
            {
                var type = ty;
                int worn = OpenGarageData.EquippedPart(vehicle, type.id);
                var items = OpenGarageData.PartItems(model, vehicle, type.id);
                var item = items.FirstOrDefault(i => i.id == worn);
                var pills = new List<(string, string)>();
                if (items.Any(i => i.isNew && !i.locked)) pills.Add(("NEW", "warn"));
                if (mod != null && item != null && OpenGarageData.PartIsMod(mod, tables, item.id)) pills.Add(("MOD", "mod"));
                var card = GarageCard("garage-part-" + type.id, type.name, GarageItemName(item), true, GarageBrandLogo(item?.parent >= 0 ? item.parent : item?.product ?? -1),
                    () => GarageEnter(GKind.PartSection, type.id), pills.ToArray());
                card.RegisterCallback<FocusInEvent>(_ =>
                {
                    GaragePartEquippedPreview(type.factory, true);
                    if (item != null) GarageItemSubject(item, type.name, true);
                });
            }
        }

        /// The model and engine sound rows of the bike in view (the slots are refilled only when the bike changes).
        void GarageHostSound(int playerVehicle, int model)
        {
            if (garageSoundSlot == null || model == garageSoundModel && garageSoundSlot.childCount > 0) return;
            var slot = garageSoundSlot;
            slot.Clear(); garageSoundModel = model;
            if (OpenGarageHooks.EngineSound != null)
            {
                try { OpenGarageHooks.EngineSound(slot, playerVehicle, model); }
                catch (Exception e) { Debug.LogWarning("[Garage] engine sound slot: " + e.Message); }
            }
        }

        void GaragePartEquippedPreview(string factory, bool reset)
        {
            int vehicle = OpenGarageData.Equipped, model = OpenGarageData.EquippedModel;
            var parts = OpenGarageData.EquippedPartIds(vehicle);
            GarageShow("part:" + model + ":" + string.Join(",", parts), () => (OpenGarageData.BikeWithPart(model, parts, -1), null, false), PartShot(factory), reset);
        }

        void GaragePartItemPreview(string factory, GarageItem item)
        {
            int vehicle = OpenGarageData.Equipped, model = OpenGarageData.EquippedModel;
            var parts = OpenGarageData.EquippedPartIds(vehicle);
            GarageShow("part:" + model + ":" + string.Join(",", parts) + ":" + item.type + ":" + item.id, () => (OpenGarageData.BikeWithPart(model, parts, item.id), null, false), PartShot(factory), false);
        }

        bool GaragePartSectionLevel(int type)
        {
            if (OpenGarageData.Pro) return false;
            int vehicle = OpenGarageData.Equipped, model = OpenGarageData.EquippedModel;
            var ty = OpenGarageData.PartTypes(vehicle).FirstOrDefault(t => t.id == type);
            if (ty.name == null) return false;
            var mod = BikeMods.ChosenFor(model);
            var tables = mod != null ? OpenGarageData.PartTables() : null;
            bool IsMod(GarageItem it) => mod != null && OpenGarageData.PartIsMod(mod, tables, it.id);
            Func<GarageItem, bool> equip = it => OpenGarageData.EquipPart(vehicle, it, ty.tuning);
            var groups = OpenGarageData.Groups(OpenGarageData.PartItems(model, vehicle, type));
            if (groups.Count == 0) { GarageNote("Nothing to fit here."); return true; }
            foreach (var g in groups)
            {
                if (!g.IsLine) { GarageItemCard(g.item, ty.name, true, equip, it => GaragePartItemPreview(ty.factory, it), false, IsMod(g.item)); continue; }
                var grp = g;
                var worn = grp.items.FirstOrDefault(i => i.equipped);
                var card = GarageLineCard(grp, worn, () => GarageEnter(GKind.PartLine, type, grp.line), grp.items.Any(IsMod));
                card.RegisterCallback<FocusInEvent>(_ =>
                {
                    GaragePartEquippedPreview(ty.factory, false);
                    GarageLineSubject(grp, ty.name, worn, true);
                });
            }
            if (ty.tuning != null && ty.tuning.Length > 0) GarageNote("A " + ty.name + " brings " + ty.tuning.ToUpperInvariant() + " points to tune in VEHICLE TUNING.");
            if (groups.Any(g => g.items.Any(i => i.kit != null)) && BikeMods.ChosenFor(model) != null)
                GarageNote("KIT PACKS are made for the original bike: on " + BikeMods.Title(BikeMods.ChoiceFor(model)) + " the mod's own kit shows (pick the original bike's card in MY RIDES to see them).");
            GarageKitNotes(true);
            return true;
        }

        bool GaragePartLineLevel(int type, int line)
        {
            if (OpenGarageData.Pro) return false;
            int vehicle = OpenGarageData.Equipped, model = OpenGarageData.EquippedModel;
            var ty = OpenGarageData.PartTypes(vehicle).FirstOrDefault(t => t.id == type);
            if (ty.name == null) return false;
            var items = OpenGarageData.PartItems(model, vehicle, type).Where(i => i.parent == line).ToList();
            if (items.Count == 0) return false;
            var mod = BikeMods.ChosenFor(model);
            var tables = mod != null ? OpenGarageData.PartTables() : null;
            foreach (var it in items)
                GarageItemCard(it, ty.name, true, x => OpenGarageData.EquipPart(vehicle, x, ty.tuning), x => GaragePartItemPreview(ty.factory, x), true, mod != null && OpenGarageData.PartIsMod(mod, tables, it.id));
            return true;
        }

        static OpenGarageStage.Shot GearShot(string factory)
        {
            switch (factory)
            {
                case "Helmet": case "Goggle": case "Goggles": case "SkinTone": return OpenGarageStage.Shot.Head;
                case "Jersey": case "ChestProtector": case "Neckbrace": return OpenGarageStage.Shot.Torso;
                case "Pants": return OpenGarageStage.Shot.Legs;
                case "Boots": return OpenGarageStage.Shot.Feet;
                case "Gloves": return OpenGarageStage.Shot.Hands;
                default: return OpenGarageStage.Shot.Rider;
            }
        }

        static OpenGarageStage.Shot PartShot(string factory)
        {
            string f = (factory ?? "").ToLowerInvariant();
            if (f.Contains("exhaust")) return OpenGarageStage.Shot.Rear;
            if (f.Contains("handle") || f.Contains("grip") || f.Contains("guard") || f.Contains("fork") || f.Contains("bumper") || f.Contains("light")) return OpenGarageStage.Shot.Front;
            if (f.Contains("wheel") || f.Contains("tire") || f.Contains("rim")) return OpenGarageStage.Shot.Wheels;
            if (f.Contains("nerf") || f.Contains("arm") || f.Contains("shock") || f.Contains("susp")) return OpenGarageStage.Shot.Low;
            return OpenGarageStage.Shot.Vehicle;
        }

        // ================================================================================================ MY RIDES

        /// The vehicle classes (GarageMyRides' types, in its order), then the modded bikes and every vehicle at once.
        void GarageClassesLevel()
        {
            bool pro = OpenGarageData.Pro;
            var all = OpenGarageData.Vehicles();
            int equippedType = pro ? -1 : all.FirstOrDefault(v => v.equipped)?.type ?? -1;
            foreach (var type in all.Select(v => v.type).Distinct().ToList())
            {
                int t = type;
                var of = all.Where(v => v.type == t).ToList();
                var eq = of.FirstOrDefault(v => v.equipped);
                int owned = of.Count(v => v.owned);
                string sub = (eq != null ? "RIDING  " + GarageBikeTitle(eq.id, eq.playerVehicle) + "  ·  " : "") + owned + " OWNED  ·  " + of.Count + " IN ALL";
                var pills = new List<(string, string)>();
                if (t == equippedType) pills.Add(("EQUIPPED", "good"));
                if (of.Any(v => v.isNew && !v.locked)) pills.Add(("NEW", "warn"));
                var card = GarageCard("garage-class-" + t, OpenGarageData.TypeName(t).ToUpperInvariant(), sub, true, null, () => GarageEnter(GKind.Models, t), pills.ToArray());
                card.EnableInClassList("ou-gcard-off", owned == 0);
                card.RegisterCallback<FocusInEvent>(_ => GarageEquippedPreview(OpenGarageStage.Shot.Overview, false));
            }
            OuTheme.Section(garageRows, "MORE");
            if (BikeMods.All.Count > 0)
            {
                OpenGarageData.LogModCoverage(all);
                int n = all.Sum(v => OpenGarageData.ModsOfModel(v.id).Count);
                var mods = GarageCard("garage-class-mods", "MODDED BIKES", n + " BIKE MOD MODELS  ·  MODS/BIKES", true, null, () => GarageEnter(GKind.Models, ModdedOnly), ("MOD", "mod"));
                mods.RegisterCallback<FocusInEvent>(_ => GarageEquippedPreview(OpenGarageStage.Shot.Overview, false));
            }
            var every = GarageCard("garage-class-all", "ALL VEHICLES", all.Count(v => v.owned) + " OWNED  ·  " + all.Count + " IN ALL  ·  EVERY CLASS", true, null, () => GarageEnter(GKind.Models, -1));
            every.RegisterCallback<FocusInEvent>(_ => GarageEquippedPreview(OpenGarageStage.Shot.Overview, false));
        }

        /// The vehicles of a class (-1 every class, ModdedOnly the modded bikes); under each covered bike's own card one card per installed bike
        /// mod that has its chassis: a vehicle of its own, named as the mod names it (mod.ini [Bikes]), owned and priced as the base bike.
        bool GarageModelsLevel(int cls)
        {
            var all = OpenGarageData.Vehicles();
            if (cls >= 0 && !all.Any(v => v.type == cls)) return false;
            if (BikeMods.All.Count > 0) OpenGarageData.LogModCoverage(all);
            var entries = new List<GarageItem>();
            foreach (var v in all.Where(v => cls < 0 || v.type == cls))
            {
                if (cls != ModdedOnly) entries.Add(v);
                foreach (var m in OpenGarageData.ModsOfModel(v.id)) entries.Add(ModCard(v, m));
            }
            if (cls == ModdedOnly && entries.Count == 0) { GarageNote("No installed bike mod has a chassis for a bike in this garage (Mods/Bikes)."); return true; }
            // a bike of a class of its own (an imported Alive 50cc / 650: its "class") is a section of its own, after the game's classes
            foreach (var group in entries.GroupBy(v => OwnClassOf(v) ?? OpenGarageData.TypeName(v.type)).OrderBy(g => g.Any(x => OwnClassOf(x) != null) ? 1 : 0))
            {
                if (cls < 0 || group.Any(x => OwnClassOf(x) != null)) OuTheme.Section(garageRows, group.Key);
                foreach (var v in group)
                {
                    var item = v;
                    string choice = BikeMods.ChoiceFor(item.id);
                    string brand = ShownBrand(item);
                    bool here = item.equipped && choice == item.modId;   // the card that matches the choice made for the bike
                    var pills = new List<(string, string)>();
                    if (here) pills.Add(("EQUIPPED", "good"));
                    else if (item.owned) pills.Add(("OWNED", null));
                    else pills.Add(item.locked ? ("LOCKED", "bad") : (item.price > 0 ? item.price.ToString("N0") : "FREE", "gold"));
                    if (item.mod != null) pills.Add(("MOD", "mod"));
                    if (item.isNew && !item.locked) pills.Add(("NEW", "warn"));
                    if (item.dlc) pills.Add(("DLC", null));
                    string sub = (brand ?? OpenGarageData.TypeName(item.type)) + (item.mod != null && !string.IsNullOrEmpty(item.mod.author) ? "  ·  BY " + item.mod.author : "");
                    var card = GarageCard(GarageVehicleCardName(item), ShownModel(item).ToUpperInvariant(), sub.ToUpperInvariant(), false, item.mod == null ? GarageBrandLogo(item.product) : null, () => GarageVehicleAccept(item), pills.ToArray());
                    card.EnableInClassList("ou-gcard-off", !item.owned);
                    card.RegisterCallback<FocusInEvent>(_ => GarageVehicleShow(item, false));
                }
            }
            return true;
        }

        /// The garage class of a mod card's bike when it has its own (RxModBike.garageClass: "50CC", "650"), else null.
        static string OwnClassOf(GarageItem v) => v.mod?.rx?.bikes.FirstOrDefault(b => b.vehicleModelId == v.id)?.garageClass;

        /// The base bike's item as a mod shows it: a card of its own (same ownership and price), titled as the mod names the bike.
        static GarageItem ModCard(GarageItem v, BikeMod m)
        {
            var c = v.Clone(); c.modId = m.id; c.mod = m;
            c.modName = BikeMods.NameFor(m, v.id, v.title);
            if (c.modName != null) c.title = c.modName.Full;   // (the purchase dialog and the status line say what the bike is)
            return c;
        }
        static string ShownModel(GarageItem v) => v.mod != null && v.modName != null && v.modName.model.Length > 0 ? v.modName.model : v.title;
        static string ShownBrand(GarageItem v) => v.mod != null && v.modName != null && v.modName.brand.Length > 0 ? v.modName.brand : OpenGarageData.BrandName(v.product);

        static string GarageVehicleCardName(GarageItem v)
        {
            if (v.mod != null) return "garage-mod-" + v.id + "-" + Math.Max(0, OpenGarageData.ModsOfModel(v.id).IndexOf(v.mod));
            return v.owned ? "garage-vehicle-" + v.playerVehicle : "garage-model-" + v.id;
        }

        /// A: equips the bike (a modded card: the base bike AND that mod as its model; the original's card: the base bike with the original
        /// model); a bike not owned goes through the purchase of the base bike, then the same.
        void GarageVehicleAccept(GarageItem v)
        {
            bool covered = OpenGarageData.ModsOfModel(v.id).Count > 0;
            string want = v.modId ?? "";
            string label = v.title.ToUpperInvariant();
            if (v.owned)
            {
                bool sameModel = !covered || BikeMods.ChoiceFor(v.id) == want;
                if (v.equipped && sameModel) { GarageStatus("ALREADY EQUIPPED  ·  " + label); return; }
                bool ok = v.equipped || OpenGarageData.SelectVehicle(v.playerVehicle);
                if (ok && covered) BikeMods.SetChoice(v.id, want);
                if (ok) WorldPicked();   // the menu world: the rider walks the new vehicle in (asked first: the level's rebuild joins it, no pop)
                GarageChanged(ok, "EQUIPPED  ·  " + label, GarageVehicleCardName(v));
                return;
            }
            GaragePurchase(v, bought =>
            {
                int pv = OpenGarageData.PlayerVehicleOfModel(bought.id);
                if (pv >= 0)
                {
                    OpenGarageData.SelectVehicle(pv); if (covered) BikeMods.SetChoice(bought.id, want);
                    // the focus stays on the bike just bought (its card is the owned one's now)
                    if (GTop != null) GTop.focus = GarageVehicleCardName(new GarageItem { id = bought.id, owned = true, playerVehicle = pv, mod = bought.mod, modId = bought.modId });
                    WorldPicked();
                }
            });
        }

        void GarageVehicleShow(GarageItem v, bool header)
        {
            if (v == null) return;
            garageRideItem = v;   // (the card TEST RIDE rides: OpenShell.TestRide.cs)
            int model = v.id, pv = v.playerVehicle;
            string mod = v.modId ?? "";
            var gear = OpenGarageData.EquippedGearIds();
            bool equippedCard = v.owned && v.equipped && (v.modId ?? "") == BikeMods.ChoiceFor(model);
            if (garagePreview != null && garagePreview.world && MenuWorld is OpenMenuWorld world)
            {   // the menu world: the vehicle browsed to stands in the hero spot itself, close and large, tagged PREVIEW (the rider steps out of the
                // picture; read on a worker thread; nothing walks until it is picked: GarageVehicleAccept); the equipped card brings them back
                if (equippedCard) { world.EndPreview(!world.PickPending); var (hb, hr, hk) = EquippedHero(); world.ShowHeroAsync(hb, hr, hk, false); }
                else world.ShowPreview(OpenGarageData.BikeOfModelAs(model, mod), "preview:" + model + ":" + pv + ":" + (pv >= 0 ? string.Join(",", OpenGarageData.EquippedPartIds(pv)) : "") + ":mod=" + mod);
                garageShownKey = null; garagePendingMake = null;
                var shot = equippedCard ? OpenGarageStage.Shot.Overview : OpenGarageStage.Shot.Vehicle;   // (the pair; a preview: the vehicle close)
                garageStage.Frame(shot, header || garageStage.Current != shot);
            }
            else
                GarageShow("veh:" + model + ":" + pv + ":" + (pv >= 0 ? string.Join(",", OpenGarageData.EquippedPartIds(pv)) : "") + ":" + string.Join(",", gear) + ":mod=" + mod,
                    () => (OpenGarageData.BikeOfModelAs(model, mod), OpenGarageData.RiderWithGear(gear), true), OpenGarageStage.Shot.Overview, false);
            GarageSubject(model, pv, mod);
            if (!equippedCard)
            {
                var tag = OuUi.Text("PREVIEW", "ou-badge ou-badge-preview", garageBadges);
                tag.name = "garage-preview-tag";
                tag.SendToBack();
            }
            if (v.isNew && !v.locked) { v.isNew = false; ReflexProfile.Run("store_not_new " + v.product); }
        }

        // ================================================================================================ VEHICLE TUNING

        void GarageTuningPage()
        {
            if (OpenGarageData.Pro) { GarageProNote(); GarageRiderPreview(); return; }
            int vehicle = OpenGarageData.Equipped, model = OpenGarageData.EquippedModel;
            var tt = OpenGarageData.Tab("PlayerVehicleTuning"); int row = tt.Find("PlayerVehicleId", vehicle);
            if (row < 0 || model < 0) { GarageNote("No tuning is available for this vehicle."); return; }
            int[] values = OuTuneRow.Columns.Select(c => tt.Int(c, row, 0)).ToArray();
            var modelRow = FeGarageData.Unique("VehicleModel", "VehicleModelId", model);
            OuTheme.Section(garageRows, "VEHICLE BALANCE");
            Label description = null;
            void Save()
            {
                bool ok = ReflexProfile.SetTuning(vehicle, new ReflexGarage.Tuning { engine1 = values[0], engine2 = values[1], suspension1 = values[2], suspension2 = values[3], tire1 = values[4], tire2 = values[5], braking = values[6], steering = values[7] }).Result;
                if (!ok) { GarageStatus((ReflexProfile.Error ?? "The tuning could not be saved.").ToUpperInvariant()); OuSound.Play("Locked"); return; }
                GarageSubject(model, vehicle);
            }
            for (int i = 0; i < 5; i++)
            {
                // OuTuneRow: the setting's two ends in the game's words, where it sits between them and a verdict (BALANCED, MORE TOP SPEED ...)
                var tune = new OuTuneRow(garageRows, "garage-tuning-" + i, null, i, values, modelRow);
                var r = tune.row;
                tune.changed = () => { Save(); GarageStatus(tune.Name + ": " + tune.Verdict() + "  ·  SAVED"); };
                tune.refused = why => GarageStatus(why);
                r.RegisterCallback<FocusInEvent>(_ =>
                {
                    GarageRowFocused(r);
                    if (description != null) description.text = tune.Description;
                    GarageShow("tune:" + model + ":" + string.Join(",", OpenGarageData.EquippedPartIds(vehicle)), () => (OpenGarageData.BikeOfModel(model), null, false), OpenGarageStage.Shot.Vehicle, true);
                    GarageSubject(model, vehicle);
                });
            }
            description = OuUi.Text("", "mxa-note ou-garage-note", garageRows);
            GarageNote("LEFT / RIGHT slides each setting toward the word on that side; the small mark on the slider is where both sides are equal. Engine, suspension and tires move the equipped part's bonus points from one side to the other (the total stays the same): a stock part has none, fit a performance part (exhaust, suspension, tires) to tune it. Braking and steering always tune.");
        }

        internal static bool MoveGarageTuning(int base1, int base2, int one, int two, int direction, out int nextOne, out int nextTwo)
        {
            nextOne = one; nextTwo = two;
            int step = Math.Sign(direction) * 5;
            if (step == 0 || one + step < 0 || two - step < 0 || base1 + one + step > 100 || base2 + two - step > 100) return false;
            nextOne += step; nextTwo -= step; return true;
        }
    }
}
