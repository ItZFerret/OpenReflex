// Which bike and which gear a rider of the race has, from the game's own tables (the running game's registry, read at run time:
// OriginalWorld/OriginalOpponents' ReflexRoster), as the game assembles them:
//   a bike   VehicleModelId + its VehiclePart ids (AIVehicle / AIVehicleParts for an AI, PlayerVehicle / PlayerVehicleParts for
//            the local player): each VehiclePart names a model package and a material package (ModelPackageDatabaseId /
//            MaterialPackageDatabaseId -> AssetPackageDatabase -> AssetDatabase.DatabaseName / AssetPackage.PackageName).
//            VehiclePartType 11 GraphicKits: the chassis (MXs/<chassis>: MX_<chassis> on that package's MX_Hierarchy, the
//            running game's vehicle model V+0x1328) and the kit's materials; 8 Tires: OBJ_FRT_Tire_* / OBJ_BCK_Tire_*; 7
//            Handlebars, 10 Exhausts, 12 HandGuards: a part model hung on its first node's namesake; 3 Wheels: the rims'
//            materials; 24 Forks: the fork sticker textures (MxaRfxBikes / RfxCatalog of the Open Unleashed port).
//   a rider  RiderGear ids (RiderGearSetGear of the AI's set, PlayerRiderGear for the local player): each names a model
//            package (RiderGearType 3 Jersey, 1 Helmet, 2 Goggles, 4 Boots, 9 Chest Protector, 11 Neckbrace) and a material
//            package (10 Skin: materials only; the "No Chest Protector" / "No Neckbrace" pieces name neither).
// The standard (not HR_) packages, LOD 0 (the HR twins are the garage's close-ups).
using System;
using System.Collections.Generic;
using System.Linq;

namespace Reflex.Port.Content
{
    public static class ReflexVehicleSpecs
    {
        /// The bike of a VehicleModelId and its parts; null when the parts name no chassis the install has.
        public static BikeSpec Bike(ReflexGameTables t, int vehicleModelId, IEnumerable<int> partIds)
        {
            var install = VehicleCatalog.Shared.install;
            var s = new BikeSpec { vehicleModelId = vehicleModelId, materials = new List<(string db, string pkg)>(), partModels = new List<(string db, string pkg)>() };
            (string db, string pkg)? kit = null, chassis = null, tires = null;
            var mats = new List<(string db, string pkg)>();
            var ids = new List<int>();
            bool atv = ReflexAtv.IsAtvModel(vehicleModelId, t);   // VehicleType FactoryName "Atv" (ReflexAtv.cs)
            bool machine = !atv && ReflexVehicles.IsMachineModel(vehicleModelId, t);   // TrophyTruck / Buggy / Rhino (ReflexVehicles.cs)
            foreach (int id in partIds)
            {
                int r = t.Find("VehiclePart", "VehiclePartId", id);
                if (r < 0) continue;
                ids.Add(id);
                int type = t.Int("VehiclePart", "VehiclePartTypeId", r);
                var model = t.Package(t.Int("VehiclePart", "ModelPackageDatabaseId", r));
                var mat = t.Package(t.Int("VehiclePart", "MaterialPackageDatabaseId", r));
                if (model != null && install.Pkg(model.Value.db, model.Value.pkg) == null) model = null;
                if (mat != null && install.Pkg(mat.Value.db, mat.Value.pkg) == null) mat = null;
                if ((atv || machine) && type != 11) { ReflexAtv.AddPart(s, model, mat, mats); continue; }   // ATV (and a machine): every part model on its hierarchy node
                switch (type)
                {
                    case 11: chassis = model; kit = mat; break;                  // GraphicKits
                    case 8: tires = model; if (mat != null) mats.Add(mat.Value); break;
                    case 7: case 10: case 12:                                     // Handlebars, Exhausts, HandGuards
                        if (model != null) { s.partModels.Add(model.Value); s.partMaterials.Add(mat); }
                        if (mat != null) mats.Add(mat.Value);
                        break;
                    case 24: s.forkStickers = mat; if (mat != null) mats.Add(mat.Value); break;   // Forks: the sticker textures
                    default: if (mat != null) mats.Add(mat.Value); break;          // Wheels (rims) ...
                }
            }
            if (chassis == null) return null;
            if (atv) ReflexAtv.Finish(s, ReflexAtv.HierarchyOf(vehicleModelId, t));
            if (machine) { string h = ReflexAtv.HierarchyOf(vehicleModelId, t); if (h == null) return null; ReflexVehicles.Finish(s, h, kit); }
            var cp = install.Pkg(chassis.Value.db, chassis.Value.pkg);
            string chassisAsset = cp.OfType("model").Select(a => a.name).FirstOrDefault(n => n.IndexOf("Hierarchy", StringComparison.OrdinalIgnoreCase) < 0);
            if (chassisAsset == null || cp.Find(s.hierarchyModel, "model") == null) return null;
            s.chassisDb = chassis.Value.db; s.chassisPkg = chassis.Value.pkg; s.chassisModel = chassisAsset;
            if (kit != null) { s.kitDb = kit.Value.db; s.kitPkg = kit.Value.pkg; } else { s.kitDb = null; s.kitPkg = null; }
            if (tires != null)
            {
                var tp = install.Pkg(tires.Value.db, tires.Value.pkg);
                string front = tp.OfType("model").Select(a => a.name).FirstOrDefault(n => n.StartsWith("OBJ_FRT_Tire", StringComparison.OrdinalIgnoreCase));
                string rear = tp.OfType("model").Select(a => a.name).FirstOrDefault(n => n.StartsWith("OBJ_BCK_Tire", StringComparison.OrdinalIgnoreCase));
                if (front != null && rear != null) { s.tireDb = tires.Value.db; s.tirePkg = tires.Value.pkg; s.frontModel = front; s.rearModel = rear; }
            }
            if (kit != null) s.materials.Add(kit.Value);
            foreach (var m in mats) if (!s.materials.Contains(m)) s.materials.Add(m);
            s.partIds = ids.ToArray();
            s.name = chassis.Value.pkg.Replace('_', ' ');
            return s;
        }

        // RiderGearType -> RiderSpec part key, in the order the outfit's packages are offered to the material override
        static readonly (int type, string key)[] GearOrder = { (3, "jersey"), (1, "helmet"), (2, "goggles"), (4, "boots"), (9, "chest_protector"), (11, "neck_brace") };
        public const int SkinGearType = 10;

        /// The outfit of a RiderGear id list; null when it names no gear model the install has.
        public static RiderSpec Rider(ReflexGameTables t, IEnumerable<int> gearIds)
        {
            var install = VehicleCatalog.Shared.install;
            var s = new RiderSpec { gearIds = gearIds.ToArray() };
            var pieces = new List<(int order, RiderSpec.Part part)>();
            foreach (int id in s.gearIds)
            {
                int r = t.Find("RiderGear", "RiderGearId", id);
                if (r < 0) continue;
                int type = t.Int("RiderGear", "RiderGearTypeId", r);
                var model = t.Package(t.Int("RiderGear", "ModelPackageDatabaseId", r));
                var mat = t.Package(t.Int("RiderGear", "MaterialPackageDatabaseId", r));
                if (model != null && install.Pkg(model.Value.db, model.Value.pkg) == null) model = null;
                if (mat != null && install.Pkg(mat.Value.db, mat.Value.pkg) == null) mat = null;
                if (type == SkinGearType) { if (mat != null) { s.skinDb = mat.Value.db; s.skinPkg = mat.Value.pkg; } continue; }
                if (model == null) { if (mat != null) s.extraMaterials.Add(mat.Value); continue; }   // "No Chest Protector" names neither
                int order = Array.FindIndex(GearOrder, g => g.type == type);
                pieces.Add((order < 0 ? GearOrder.Length : order, new RiderSpec.Part
                {
                    key = order < 0 ? "gear" + type : GearOrder[order].key, modelDb = model.Value.db, modelPkg = model.Value.pkg, asset = null,
                    matsDb = mat?.db, matsPkg = mat?.pkg
                }));
            }
            if (pieces.Count == 0) return null;
            s.parts.AddRange(pieces.OrderBy(p => p.order).Select(p => p.part));
            return s;
        }
    }
}
