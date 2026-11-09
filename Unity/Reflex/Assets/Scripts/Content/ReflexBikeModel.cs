// The player's bike, built at run time from the player's own Reflex install (nothing of the game is kept in the project):
// the chassis model (MXs/<chassis>/MX_<chassis>, LOD 0) on the MX_Hierarchy nodes, the front / rear tyre models (MX_Tires/<set>)
// on the OBJ_FRT_Tire / OBJ_BCK_Tire nodes, each binding's material resolved as the game does (the graphic kit's plastics and
// seat, the wheel set's rim / spokes / tyre, the frame's fork sticker on the second UV set ...). Ported from
// _re/formats/bike_asset_export.py (the same geometry: static surfaces carry their node's world matrix, the skinned fenders
// stay in their stored coordinates; UV V flipped for Unity) and Editor/ReflexBikeBuilder + AlpineOriginalPhysicsBuilder's
// WheelGroup / BindBikePose (the part meshes are in model space; OriginalBikePose moves each by liveWorld(node) * inverse(bindWorld(node))).
// The bike the captured world uses is the MarsF Lite with the THQ red graphic kit and THQ tyres (BIKE_FORMAT.md): BikeSpec.MarsFLite.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEngine;
using Reflex.Port.OriginalWorld;

namespace Reflex.Port.Content
{
    /// Which bike: the packages the game's VehicleModelParts name for the chassis, the graphic kit and the wheel set
    /// (ReflexVehicleSpecs.Bike makes one from a VehicleModelId and its VehiclePart ids).
    public sealed class BikeSpec
    {
        public string chassisDb = "MXs", chassisPkg = "MarsF_Lite", chassisModel = "MX_MarsF_Lite";
        public string kitDb = "MX_GraphicKits", kitPkg = "MX_MarsF_Lite_THQ_Red";
        public string tireDb = "MX_Tires", tirePkg = "MX_Tire_THQ", frontModel = "OBJ_FRT_Tire_THQ", rearModel = "OBJ_BCK_Tire_THQ";
        /// The chassis package's node hierarchy: the running game's vehicle model (V+0x1328) is that package's MX_Hierarchy, whose
        /// nodes are what reflex_world_bike_pose(_slot) returns, in its order; the chassis' nodes bind to it by name.
        public string hierarchyModel = "MX_Hierarchy";
        /// An ATV (ReflexAtv: VehicleType FactoryName "Atv", hierarchy ATV_Hierarchy): no motocross tyre set; its tyres, wheels, A-arms,
        /// shocks ... are part models on their hierarchy nodes, and the two wheel groups stay empty (the pose places every part).
        public bool atv;
        /// A truck, buggy or UTV (ReflexVehicles: FactoryName TrophyTruck / Buggy / Rhino): the chassis and the kit's own wheel models on the
        /// chassis' hierarchy, materials by the game's override (ReflexVehicles.Resolve), the DMG_ damage panels hidden until the game shows them.
        public bool machine;
        /// Part models hung on the hierarchy (Handlebars, Exhausts, HandGuards): each model of the package on its first node's namesake.
        public List<(string db, string pkg)> partModels = new List<(string db, string pkg)>();
        /// Each part model's own material package (null: none), parallel to partModels.
        public List<(string db, string pkg)?> partMaterials = new List<(string db, string pkg)?>();
        /// The material packages of the parts, the kit first (the material override's pool); null: the kit's only.
        public List<(string db, string pkg)> materials;
        /// Forks: the texture package whose stickers replace the frame material's fork sticker of the same name prefix.
        public (string db, string pkg)? forkStickers;
        public int vehicleModelId = -1; public int[] partIds;
        public string name = "MarsF Lite";
        /// The package of the pose hierarchy (hierarchyModel) when it is not the chassis package (a bike mod's variant: BikeMods.Apply keeps
        /// the original chassis package's hierarchy, whose node order the core's pose follows). Null: the chassis package's.
        public string hierarchyDb, hierarchyPkg;
        /// The bike mod this spec was made from (BikeMods.Apply), null: the original.
        public string modId, modName;
        /// A .rxmod bike (Content/RxModBikes.cs, core/plan/RXMOD.md): its own model and maps drawn on this spec's original hierarchy (the
        /// original's chassis / tyre / part packages stay for the pose and the material templates); null: the packages are the model.
        public RxModBike rx;
        /// The skin (a "skins" .rxmod) painting rx, null: the bike's own maps.
        public RxSkin rxSkin;
        public static BikeSpec MarsFLite => new BikeSpec();
        /// Equal keys build equal bikes.
        public string Key => string.Join("|", chassisDb, chassisPkg, chassisModel, kitDb, kitPkg, tireDb, tirePkg, frontModel, rearModel, hierarchyModel,
            string.Join(",", partModels.Select(p => p.db + "/" + p.pkg)), materials == null ? "-" : string.Join(",", materials.Select(p => p.db + "/" + p.pkg)),
            forkStickers == null ? "-" : forkStickers.Value.db + "/" + forkStickers.Value.pkg) + (hierarchyDb == null ? "" : "|h:" + hierarchyDb + "/" + hierarchyPkg)
            + (rx == null ? "" : "|rx:" + rx.Key) + (rxSkin == null ? "" : "|skin:" + rxSkin.Key);
        /// A copy (the lists copied too).
        public BikeSpec Clone()
        {
            var c = (BikeSpec)MemberwiseClone();
            c.partModels = new List<(string db, string pkg)>(partModels); c.partMaterials = new List<(string db, string pkg)?>(partMaterials);
            c.materials = materials == null ? null : new List<(string db, string pkg)>(materials);
            c.partIds = partIds == null ? null : (int[])partIds.Clone();
            return c;
        }
    }

    public sealed partial class ReflexBikeModel
    {
        public GameObject chassis;
        public Transform frontWheel, rearWheel;
        public OriginalBikePose pose;
        /// Every part mesh with the material it was given (diagnostics / tests).
        public readonly List<(string name, Transform part, int node, string material)> parts = new List<(string, Transform, int, string)>();
        /// The pose hierarchy (BikeSpec.hierarchyModel) node names, in the order the game's pose export returns them.
        public string[] hierarchyNodes;
        /// A bike mod's hand / foot locators where they differ from the original hierarchy's (RiderLimbRetarget; null: none, the original).
        public RiderLimbRetarget.Offset[] limbOffsets;
        /// A bike of its own size: its fork posed onto the physics axle (OriginalBikePose.ForkFix; .rxmod "pose": "physics"), null: none.
        public OriginalBikePose.ForkFix forkFix;
        readonly List<Matrix4x4> binds = new List<Matrix4x4>();
        /// The pose hierarchy's bind world matrices (model space, Unity column convention), parallel to hierarchyNodes.
        public Matrix4x4[] hierarchyBind;
        /// The "Numbers" decal of the plastics' second UV set: the game binds a run-time number plate texture here
        /// (MXPlayer + 0xb00; not in the files). Null: the plate stays clear.
        public static Texture2D numberPlate;

        /// Frees the textures decoded for the bike and the rider (shared; the run-time owner is gone).
        public static void ReleaseTextures() { VehicleCatalog.ReleaseTextures(); ReleaseParsed(); ReflexRiderModel.ReleaseParsed(); RxModBikes.ReleaseTextures(); RxModBikes.ReleaseParsed(); }

        public void Destroy()
        {
            foreach (var r in new[] { chassis, frontWheel ? frontWheel.gameObject : null, rearWheel ? rearWheel.gameObject : null })
                if (r) { foreach (var f in r.GetComponentsInChildren<MeshFilter>(true)) if (f.sharedMesh) UnityEngine.Object.Destroy(f.sharedMesh);
                         foreach (var mr in r.GetComponentsInChildren<MeshRenderer>(true)) foreach (var m in mr.sharedMaterials) if (m) UnityEngine.Object.Destroy(m);
                         UnityEngine.Object.Destroy(r); }
        }

        /// The garage's high-resolution twin of a bike (WP-G, _re/notes/garage.md): MXGarage draws with HR on (MXGarage:SetHREnable, TRUE
        /// but in MotocardStunts), and the part loader (FUN_0053cf30 / FUN_0053ca70) then takes every package of a part with HRAvailable as
        /// "HR_<database>" / "HR_<package>" (FUN_0078fd60 "HR_%s" on both names), whose assets carry the prefix too ("HR_MX_MarsF_Lite",
        /// "HR_MX_Plstc"; the hierarchy keeps its name). HRAvailable is set exactly for the parts whose twin is installed (1600 vehicle-part
        /// and 628 rider-gear package references; 16 references name a twin that is not installed and keep the normal package here), so a
        /// package's twin is used when the install has it. Material keys skip the prefix (ReflexVehicles.MaterialKey: cut before the third
        /// '_' after "HR_"). Null: no twin of the chassis (the spec as it is).
        public static BikeSpec HighRes(BikeSpec s)
        {
            if (s == null) return null;
            var hr = HighResAll(s);
            return hr.chassisDb == s.chassisDb ? s : hr;
        }

        /// HighRes's twins of every package of the spec that has one (the chassis' too when it has none: the spec's own).
        static BikeSpec HighResAll(BikeSpec s)
        {
            var install = VehicleCatalog.Shared.install;
            // (a bike mod's package keeps its namespace: "mod:<id>>MXs" -> "mod:<id>>HR_MXs", BikeMods)
            (string db, string pkg) Twin((string db, string pkg) p) => p.db != null && p.pkg != null && !BikeMods.BaseDb(p.db).StartsWith("HR_", StringComparison.Ordinal) && install.Pkg(BikeMods.NamespaceOf(p.db) + "HR_" + BikeMods.BaseDb(p.db), "HR_" + p.pkg) != null ? (BikeMods.NamespaceOf(p.db) + "HR_" + BikeMods.BaseDb(p.db), "HR_" + p.pkg) : p;
            // A twin's model: "HR_" + the model's name (or the name itself); role: else the twin's model of that role. The twins of three
            // tyres name their models apart from the normal package: MX_Tire_Maxxis_IT / _SI / _SX hold OBJ_FRT_Tire_Maxxis / OBJ_BCK_Tire_Maxxis,
            // their twins HR_OBJ_FRT_Tire_Maxxis_IT ... (and HR_OBJ_FRT_TireMaxxis_SX). Null: the twin has no such model, so that package keeps its
            // normal one (before, the twin's missing model failed the whole build and the garage fell back to the normal models of everything).
            string Asset((string db, string pkg) p, string asset, string role = null)
            {
                if (asset == null || p.db == null || !BikeMods.BaseDb(p.db).StartsWith("HR_", StringComparison.Ordinal)) return asset;
                var pk = install.Pkg(p.db, p.pkg); if (pk == null) return null;
                if (pk.Find("HR_" + asset, "model") != null) return "HR_" + asset;
                if (pk.Find(asset, "model") != null) return asset;
                return role == null ? null : pk.OfType("model").Select(a => a.name).FirstOrDefault(n => n.StartsWith("HR_" + role, StringComparison.OrdinalIgnoreCase));
            }
            var chassis = Twin((s.chassisDb, s.chassisPkg)); string chassisModel = Asset(chassis, s.chassisModel);
            if (chassisModel == null) { chassis = (s.chassisDb, s.chassisPkg); chassisModel = s.chassisModel; }
            var kit = Twin((s.kitDb, s.kitPkg)); var tire = Twin((s.tireDb, s.tirePkg));
            string front = Asset(tire, s.frontModel, "OBJ_FRT_Tire"), rear = Asset(tire, s.rearModel, "OBJ_BCK_Tire");
            if (front == null || rear == null) { tire = (s.tireDb, s.tirePkg); front = s.frontModel; rear = s.rearModel; }
            return new BikeSpec
            {
                chassisDb = chassis.db, chassisPkg = chassis.pkg, chassisModel = chassisModel,
                kitDb = kit.db, kitPkg = kit.pkg,
                tireDb = tire.db, tirePkg = tire.pkg, frontModel = front, rearModel = rear,
                hierarchyModel = s.hierarchyModel, atv = s.atv, machine = s.machine, hierarchyDb = s.hierarchyDb, hierarchyPkg = s.hierarchyPkg, modId = s.modId, modName = s.modName, rx = s.rx, rxSkin = s.rxSkin,
                partModels = s.partModels.Select(Twin).ToList(),
                partMaterials = s.partMaterials.Select(p => p == null ? ((string db, string pkg)?)null : Twin(p.Value)).ToList(),
                materials = s.materials?.Select(Twin).ToList(),
                forkStickers = s.forkStickers == null ? ((string db, string pkg)?)null : Twin(s.forkStickers.Value),
                vehicleModelId = s.vehicleModelId, partIds = s.partIds, name = s.name + " (HR)",
            };
        }

        /// The part databases whose HR twin the player's own vehicle takes while riding (Build's highResParts): the hand guards, handlebars and
        /// exhausts - the parts right under the onboard view and next to the chase camera. The race models of the hand guards are a crude
        /// stand-in (MX_Hguards SWP_Hguards_MX: 52 triangles, faceted; HR_SWP_Hguards_MX: 2492), the bars and pipes 2-5 times coarser than
        /// their twins; a few thousand triangles in all, against the chassis' 16k. The chassis and the tyres keep the race models (their twins
        /// are 45k and 2 x 30-40k).
        public static readonly string[] RideHighResParts = { "MX_Hguards", "ATV_Hguards", "MX_Handlebars", "ATV_Handlebars", "MX_Exhausts", "ATV_Exhausts" };

        /// The spec with the HR twins of its part models of RideHighResParts (and of their material packages, and the whole material pool as
        /// the garage has it, for those parts' bindings); null: no such part has a twin.
        public static BikeSpec HighResParts(BikeSpec s)
        {
            if (s == null || s.partModels.Count == 0) return null;
            var hr = HighResAll(s);
            bool any = false;
            var o = s.Clone();
            for (int k = 0; k < s.partModels.Count; k++)
            {
                string db = BikeMods.BaseDb(s.partModels[k].db);
                if (Array.IndexOf(RideHighResParts, db) < 0 || hr.partModels[k].db == s.partModels[k].db) continue;
                o.partModels[k] = hr.partModels[k];
                if (k < o.partMaterials.Count) o.partMaterials[k] = hr.partMaterials[k];
                any = true;
            }
            if (!any) return null;
            o.materials = hr.materials ?? (hr.kitPkg != null ? new List<(string db, string pkg)> { (hr.kitDb, hr.kitPkg) } : null);
            return o;
        }

        /// Builds the bike at the origin in its model space, unparented ("Original MarsF Lite chassis · body 0" and the two wheel groups).
        /// highRes: the garage's HR twin (HighRes). highResParts (the player's own vehicle in a ride): the HR twins of the RideHighResParts
        /// part models (HighResParts), the rest the race models.
        public static ReflexBikeModel Build(BikeSpec spec = null, bool highRes = false, bool highResParts = false)
        {
            spec ??= BikeSpec.MarsFLite;
            if (spec.rx != null) return BuildRx(spec);   // a .rxmod bike: its own model on the original's hierarchy (ReflexBikeModel.RxMod.cs)
            if (highRes) spec = HighRes(spec);
            var partSpec = !highRes && highResParts ? HighResParts(spec) ?? spec : spec;
            var cat = VehicleCatalog.Shared; var install = cat.install;
            var result = new ReflexBikeModel();
            var chosen = spec.materials != null && spec.materials.Count > 0 ? spec.materials
                       : spec.kitPkg != null ? new List<(string db, string pkg)> { (spec.kitDb, spec.kitPkg) } : new List<(string db, string pkg)>();
            var stickers = spec.forkStickers != null ? install.Pkg(spec.forkStickers.Value.db, spec.forkStickers.Value.pkg) : null;

            var body = Load(install, spec.chassisDb, spec.chassisPkg, spec.chassisModel);
            if (spec.rxSkin != null) Debug.Log($"Reflex bike {spec.name}: the .rxmod skin {spec.rxSkin.Title} ({spec.rxSkin.mod.name}) over its graphics kit");
            // the pose's nodes: the chassis package's hierarchy (the chassis model's nodes bind to it by name)
            var hierAsset = install.Pkg(spec.hierarchyDb ?? spec.chassisDb, spec.hierarchyPkg ?? spec.chassisPkg)?.Find(spec.hierarchyModel, "model");
            var hier = hierAsset != null ? Rfx.ParseModel(install.Read(hierAsset)) : body.model;
            result.hierarchyNodes = hier.nodes.Select(n => n.name).ToArray();
            result.hierarchyBind = hier.nodes.Select(n => n.world).ToArray();
            if (spec.modId != null) result.limbOffsets = LimbOffsets(install, spec, hier);
            int PoseNode(string name) { int i = hier.NodeIndex(name); if (i < 0) Debug.LogWarning($"Reflex bike {spec.name}: node {name} is not in {spec.hierarchyModel}"); return Math.Max(0, i); }
            // a node's bind world: the chassis model's (its surfaces are placed by it), else the hierarchy's
            Matrix4x4 BindOf(string name) { int i = body.model.NodeIndex(name); return i >= 0 ? body.model.nodes[i].world : hier.nodes[Math.Max(0, hier.NodeIndex(name))].world; }
            var root = new GameObject($"Original {spec.name} chassis · body 0");
            result.chassis = root;
            var partChosen = partSpec == spec ? chosen : partSpec.materials ?? chosen;   // (the HR parts' bindings resolve over the HR pool, as in the garage)
            var materials = new Dictionary<string, UnityEngine.Material>();
            var defs = new Dictionary<string, VehicleMaterialDef>();
            UnityEngine.Material MaterialOf(string binding, (string db, string pkg) own, (string db, string pkg)? partMats = null, List<(string db, string pkg)> pool = null)
            {
                pool ??= chosen;
                string key = own.db + "/" + own.pkg + "|" + binding;
                if (materials.TryGetValue(key, out var m)) return m;
                var def = spec.machine ? ReflexVehicles.Resolve(cat, binding, own, partMats ?? (spec.kitPkg != null ? (spec.kitDb, spec.kitPkg) : ((string, string)?)null))   // FUN_0053d8b0
                        : GameMaterial(cat, binding, own, pool) ?? (partMats != null ? PartMaterial(cat, binding, partMats.Value) : null) ?? cat.Resolve(binding, own, pool);
                if (def == null) Debug.LogWarning("Reflex bike: no material for binding " + binding);
                if (spec.rxSkin != null && spec.rx == null) def = RxOriginalSkin(def, spec.rxSkin);   // an original bike's .rxmod skin (ReflexBikeModel.RxMod.cs)
                defs[key] = def;
                return materials[key] = cat.Build(def, binding + (def != null && def.name != binding ? " -> " + def.name : ""), null, stickers);
            }

            // the chassis (LOD 0): static surfaces carry their node's world matrix, the skinned fenders stay as stored. The original bikes'
            // skinned surfaces (the fenders over their NDE_*_FenderVIB node) move rigidly with their binding node; a bike mod's are skinned
            // over their own bone palette (OriginalBikeSkin): its cables, brake lines, rear shock and fork boots are weighted to other
            // nodes than the one the surface is bound to (VenusF's shock in a surface bound to OBJ_UPR_Forks would steer with the bars)
            bool skinMod = BikeMods.NamespaceOf(spec.chassisDb).Length > 0;
            var boneOf = new Dictionary<int, Transform>();
            Transform Bone(int j)
            {
                if (boneOf.TryGetValue(j, out var t)) return t;
                string name = body.model.nodes[j].name;
                var go = new GameObject("bone " + name); go.transform.SetParent(root.transform, false);
                result.bones.Add((go.transform, PoseNode(name), body.model.nodes[j].world));
                return boneOf[j] = go.transform;
            }
            var chassisParts = new List<(Transform part, string node, string binding, UnityEngine.Material material, VehicleMaterialDef def)>();
            foreach (var b in body.model.Lod(0))
            {
                var s = body.surfaces[b.surface];
                var m = s.skinned ? Matrix4x4.identity : body.model.nodes[b.node].world;
                string node = body.model.nodes[b.node].name;
                var mat = MaterialOf(b.material, (spec.chassisDb, spec.chassisPkg));
                var t = result.AddPart(root.transform, $"{spec.chassisModel}_{b.surface}_{node}", s, m, PoseNode(node), body.model.nodes[b.node].world, b.material, mat);
                if (skinMod && s.skinned && s.bones != null) result.Skin(t, s, b.node, Bone);
                if (skinMod) chassisParts.Add((t, node, b.material, mat, defs.TryGetValue(spec.chassisDb + "/" + spec.chassisPkg + "|" + b.material, out var d) ? d : null));
            }

            // the tyres: a tyre model is modelled in its node's space; it hangs on the body's tyre node (its own root placement is ignored)
            var wheels = new List<GameObject>();
            if (spec.atv) { result.frontWheel = new GameObject("Original front wheels · bodies 2, 3").transform; result.rearWheel = new GameObject("Original rear wheels · bodies 4, 5").transform; }
            else if (spec.machine) { result.frontWheel = new GameObject("Original front wheels · bodies 1, 2").transform; result.rearWheel = new GameObject("Original rear wheels · bodies 3, 4").transform; }
            else foreach (var (asset, nodeName) in new[] { (spec.frontModel, "OBJ_FRT_Tire"), (spec.rearModel, "OBJ_BCK_Tire") })
            {
                var tire = Load(install, spec.tireDb, spec.tirePkg, asset);
                int attach = body.model.NodeIndex(nodeName);
                Rfx.Check(attach >= 0 && tire.model.nodes.Length == 1, "the tyre's attachment node");
                var holder = new GameObject(nodeName == "OBJ_FRT_Tire" ? "Original front wheel · body 2" : "Original rear wheel · body 3");
                var attachWorld = body.model.nodes[attach].world;
                var meshes = new List<Transform>();
                foreach (var b in tire.model.Lod(0))
                {
                    var s = tire.surfaces[b.surface];
                    var m = s.skinned ? Matrix4x4.identity : attachWorld;
                    string node = tire.model.nodes[b.node].name;
                    meshes.Add(result.AddPart(holder.transform, $"{asset}_{b.surface}_{node}", s, m, PoseNode(nodeName), attachWorld, b.material, MaterialOf(b.material, (spec.tireDb, spec.tirePkg))));
                }
                // the group's pivot is the tyre mesh's bounds centre (parts keep their model-space placement)
                var tireMesh = meshes.FirstOrDefault(t => t.GetComponent<MeshRenderer>().sharedMaterial.name.StartsWith("MX_Tire", StringComparison.Ordinal) || t.GetComponent<MeshRenderer>().sharedMaterial.name.StartsWith("HR_MX_Tire", StringComparison.Ordinal)) ?? meshes[0];
                holder.transform.position = tireMesh.GetComponent<MeshFilter>().sharedMesh.bounds.center;
                foreach (var t in meshes) t.position = Vector3.zero;   // (the holder moved: the parts stay at the model origin)
                if (nodeName == "OBJ_FRT_Tire") result.frontWheel = holder.transform; else result.rearWheel = holder.transform;
                wheels.Add(holder);
            }

            // the part models (handlebars on OBJ_UPR_Forks, the exhaust on OBJ_Frame, hand guards): each model of the package is
            // modelled in its first node's space and hangs on the hierarchy's node of that name (its own root placement ignored)
            for (int k = 0; k < partSpec.partModels.Count; k++)
            {
                var (db, pkg) = partSpec.partModels[k];
                var pp = install.Pkg(db, pkg); if (pp == null) continue;
                (string db, string pkg)? partMats = PartMaterialsOf(partSpec, k);
                var pool = partSpec != spec && partSpec.partModels[k].db != spec.partModels[k].db ? partChosen : chosen;
                foreach (var ma in pp.OfType("model").Where(a => a.name.IndexOf("Hierarchy", StringComparison.OrdinalIgnoreCase) < 0).ToList())
                {
                    var part = Load(install, db, pkg, ma.name);
                    string nodeName = part.model.nodes[0].name;
                    if (hier.NodeIndex(nodeName) < 0 && body.model.NodeIndex(nodeName) < 0) { Debug.LogWarning($"Reflex bike {spec.name}: part {ma.name} hangs on {nodeName}, not a node of the bike"); continue; }
                    var attachWorld = BindOf(nodeName); var rootInv = part.model.nodes[0].world.inverse;
                    foreach (var b in part.model.Lod(0))
                    {
                        if (b.material.IndexOf("Stand", StringComparison.OrdinalIgnoreCase) >= 0) continue;   // (the HR models' garage stand)
                        var s = part.surfaces[b.surface];
                        // a surface bound to another node of the model than its first (the shock springs' models: a copy of the swingarm's chain
                        // OBJ_Frame > OBJ_Swingarm > OBJ_*_ShockSpring, the surface bound to the last) is drawn on that node of the bike, as the game
                        // binds a model's nodes to the vehicle's by name; the node's own world in the part model is not the bike's (the spring
                        // came out beside the bars, 1.3 ft off its shock): its vertices are in the bike's node space
                        string bound = part.model.nodes[b.node].name;
                        bool onNode = b.node != 0 && !string.Equals(bound, nodeName, StringComparison.OrdinalIgnoreCase) && (hier.NodeIndex(bound) >= 0 || body.model.NodeIndex(bound) >= 0);
                        var m = s.skinned ? Matrix4x4.identity : onNode ? BindOf(bound) : attachWorld * rootInv * part.model.nodes[b.node].world;
                        result.AddPart(root.transform, $"{ma.name}_{b.surface}_{bound}", s, m, PoseNode(onNode ? bound : nodeName), onNode ? BindOf(bound) : attachWorld, b.material, MaterialOf(b.material, (db, pkg), partMats, pool));
                    }
                }
            }

            // a bike mod without a "Numbers" plate of its own: the number on its own plate panels (ReflexBikeModel.ModPlates.cs; after the part
            // models: the exhaust, the seat and the bars may hide a panel)
            if (skinMod)
                try { result.ProjectModPlates(spec, partSpec, install, cat, hier, chosen, chassisParts); }
                catch (Exception e) when (e is InvalidDataException || e is IOException || e is ArgumentException) { Debug.LogWarning($"Reflex bike {spec.name}: mod number plates: {e.Message}"); }

            // a machine's damage panels (DMG_*) start hidden, as the game's node flag 0x10 has them (OriginalBikePose.ApplyFlags follows the game's)
            if (spec.machine) foreach (var p in result.parts) if (ReflexVehicles.HiddenAtStart(result.hierarchyNodes[p.node])) p.part.gameObject.SetActive(false);
            // OriginalBikePose: every part by its original node (the node's bind world inverse, in Unity's column convention)
            // (+ a mod's skin bones after the parts: posed the same way, they carry no mesh)
            var pose = root.AddComponent<OriginalBikePose>();
            pose.parts = result.parts.Select(p => p.part).Concat(result.bones.Select(x => x.bone)).ToArray();
            pose.nodes = result.parts.Select(p => p.node).Concat(result.bones.Select(x => x.node)).ToArray();
            pose.inverseBind = result.binds.Concat(result.bones.Select(x => x.bind)).Select(w => w.inverse).ToArray();
            result.pose = pose;
            return result;
        }

        /// A bike mod's locators of the rider's hands and feet against the pose hierarchy's (the original's; BikeMods.Apply keeps it): the
        /// translation change of each in its parent's frame (the rotations and parents must agree; the mod's lean animations move the same
        /// tracks by the same amount, QUEUE.md "w-bikemods"). Null: no change.
        static RiderLimbRetarget.Offset[] LimbOffsets(Rfx.Install install, BikeSpec spec, Rfx.Model hier)
        {
            var ma = install.Pkg(spec.chassisDb, spec.chassisPkg)?.Find(spec.hierarchyModel, "model");
            if (ma == null) return null;
            Rfx.Model mod;
            try { mod = Rfx.ParseModel(install.Read(ma)); } catch (InvalidDataException e) { Debug.LogWarning($"Reflex bike {spec.name}: the mod's hierarchy: {e.Message}"); return null; }
            var list = new List<RiderLimbRetarget.Offset>();
            foreach (string n in new[] { "NDE_RHT_Hand", "NDE_LFT_Hand", "NDE_RHT_Foot", "NDE_LFT_Foot" })
            {
                int i = hier.NodeIndex(n), j = mod.NodeIndex(n);
                if (i < 0 || j < 0 || hier.nodes[i].parent < 0 || mod.nodes[j].parent < 0) continue;
                if (!string.Equals(hier.nodes[hier.nodes[i].parent].name, mod.nodes[mod.nodes[j].parent].name, StringComparison.OrdinalIgnoreCase)) { Debug.LogWarning($"Reflex bike {spec.name}: {n} has another parent in the mod"); continue; }
                Vector3 d = (Vector3)(mod.nodes[j].local.GetColumn(3) - hier.nodes[i].local.GetColumn(3));
                if (d.sqrMagnitude < 1e-8f || !RiderLimbRetarget.Chain(n, out int u, out int l, out int e)) continue;
                list.Add(new RiderLimbRetarget.Offset { locator = n, node = i, parent = hier.nodes[i].parent, delta = d, upper = u, lower = l, end = e });
            }
            if (list.Count > 0) Debug.Log($"Reflex bike {spec.name}: the mod's hand / foot locators: " + string.Join(", ", list.Select(o => o.locator + " " + o.delta.ToString("F3"))));
            return list.Count > 0 ? list.ToArray() : null;
        }

        /// The material a bike's (or an ATV's) part binding ends up with in the game, read back from the running game's vehicles (each
        /// vehicle's own model V+0x1328, ObjShadeTests.BindingRule: 343 of 343 bindings of a 12-rider race): the first material, over the
        /// vehicle's material packages in part order (the graphic kit first: BikeSpec.materials), whose MaterialKey (FUN_00543fc0: digits
        /// dropped, cut before the second '_') prefixes the binding's key over its own length (VehicleLoader_OverrideMaterials
        /// FUN_0053d8b0's _strnicmp); none: the model package's own material of the binding's name. (An exhaust model's generic
        /// MX_Exh_Silencer / MX_Exh_Pipe both take the package's first MX_Exh material, its _Pipe, as the game draws them.) Null: neither.
        static VehicleMaterialDef GameMaterial(VehicleCatalog cat, string binding, (string db, string pkg) own, List<(string db, string pkg)> packages)
        {
            string kb = ReflexVehicles.MaterialKey(binding);
            foreach (var (db, pkg) in packages)
                foreach (string m in cat.MaterialNames(db, pkg))
                {
                    string km = ReflexVehicles.MaterialKey(m);
                    if (km.Length > 0 && kb.Length >= km.Length && string.Compare(kb, 0, km, 0, km.Length, StringComparison.OrdinalIgnoreCase) == 0) return cat.Material(db, pkg, m);
                }
            return cat.Material(own.db, own.pkg, binding);
        }

        /// The material package of part model k of a spec: the one of spec.materials whose package shares the model package's
        /// database (MX_Handlebars/MX_Hbar_Stock -> MX_Handlebars/MX_Hbar_Stock_2), else none.
        static (string db, string pkg)? PartMaterialsOf(BikeSpec spec, int k)
        {
            if (k < spec.partMaterials.Count) return spec.partMaterials[k];
            if (spec.materials == null) return null;
            var (db, pkg) = spec.partModels[k];
            foreach (var m in spec.materials) if (string.Equals(m.db, db, StringComparison.OrdinalIgnoreCase) && m.pkg.StartsWith(pkg, StringComparison.OrdinalIgnoreCase)) return m;
            foreach (var m in spec.materials) if (string.Equals(m.db, db, StringComparison.OrdinalIgnoreCase)) return m;
            return null;
        }

        /// A part model's generic binding that its product's material package names in full (the exhausts' "MX_Exh_Pipe" ->
        /// "MX_Exh_FMF_PowerCore_250_Pipe"; the Tag bars' "MX_Hbar_Tag_T2_Metal" -> "MX_Hbars_Tag_T2_Gold"), which the override
        /// rule (equal, or the binding + "_") misses: the package's one material with the binding's words in order, else its only
        /// material (MxaRfxBikes.PartMaterial). Null: the rule's answer stands.
        static VehicleMaterialDef PartMaterial(VehicleCatalog cat, string binding, (string db, string pkg) mats)
        {
            string Norm(string s) => (s.StartsWith("HR_", StringComparison.OrdinalIgnoreCase) ? s.Substring(3) : s).ToLowerInvariant();
            string b = Norm(binding); string[] words = b.Split('_');
            var names = cat.MaterialNames(mats.db, mats.pkg).ToList();
            if (names.Count == 0 || names.Any(n => Norm(n) == b || Norm(n).StartsWith(b + "_", StringComparison.Ordinal))) return null;
            bool InOrder(string n) { int i = 0; foreach (string t in Norm(n).Split('_')) if (i < words.Length && t == words[i]) i++; return i == words.Length; }
            var hits = names.Where(InOrder).ToList();
            string pick = hits.Count == 1 ? hits[0] : hits.Count == 0 && names.Count == 1 ? names[0] : null;
            return pick != null ? cat.Material(mats.db, mats.pkg, pick) : null;
        }

        /// Reads and parses a bike's models (chassis, hierarchy, tyres, part models) on the calling thread - a worker thread: the front end's
        /// garage while the player browses (OpenFrontEnd/OpenMenuWorld) - so the Build that follows on the main thread only makes the Unity
        /// objects. Thread-safe (the parse cache is locked); failures are left for that Build to report.
        public static void Prefetch(BikeSpec spec, bool highRes = false)
        {
            if (spec == null) return;
            if (spec.rx != null) { PrefetchRx(spec); return; }
            try
            {
                if (highRes) spec = HighRes(spec);
                var install = VehicleCatalog.Shared.install;
                Load(install, spec.chassisDb, spec.chassisPkg, spec.chassisModel);
                if (!spec.atv && !spec.machine)
                {
                    try { Load(install, spec.tireDb, spec.tirePkg, spec.frontModel); Load(install, spec.tireDb, spec.tirePkg, spec.rearModel); } catch (Exception) { }
                }
                foreach (var (db, pkg) in spec.partModels)
                {
                    var pp = install.Pkg(db, pkg); if (pp == null) continue;
                    foreach (var ma in pp.OfType("model").Where(a => a.name.IndexOf("Hierarchy", StringComparison.OrdinalIgnoreCase) < 0).ToList())
                        try { Load(install, db, pkg, ma.name); } catch (Exception) { }
                }
            }
            catch (Exception) { }
        }

        /// Part meshes: model-space geometry with the original node (OriginalBikePose moves the part by that node: live world(node)
        /// * inverse(bind)).
        Transform AddPart(Transform parent, string name, Rfx.Surface s, Matrix4x4 m, int node, Matrix4x4 bind, string binding, UnityEngine.Material material)
        {
            var go = new GameObject(name); go.transform.SetParent(parent, false);
            go.AddComponent<MeshFilter>().sharedMesh = ReflexMeshes.Build(name, s, m, false);
            var mr = go.AddComponent<MeshRenderer>(); mr.sharedMaterial = material;
            parts.Add((name, go.transform, node, binding)); binds.Add(bind);
            return go.transform;
        }

        /// A bike mod's skin bones (OriginalBikeSkin): an empty transform per model node a skinned surface is weighted to, with its pose node
        /// and the model's bind world of that node.
        public readonly List<(Transform bone, int node, Matrix4x4 bind)> bones = new List<(Transform, int, Matrix4x4)>();
        /// Each skinned part of a bike mod: its binding node and every model node its vertices are weighted to (diagnostics / tests).
        public readonly List<(Transform part, int bindingNode, int[] weightedNodes)> skinned = new List<(Transform, int, int[])>();

        /// Skins a part built from a skinned surface (its mesh in model space) over the bone of each model node its vertices are weighted to.
        void Skin(Transform part, Rfx.Surface s, int bindingNode, Func<int, Transform> boneOf)
        {
            if (SkinPart(part, s, boneOf, out var nodes) != null) skinned.Add((part, bindingNode, nodes));
        }

        /// Adds OriginalBikeSkin to a part whose mesh (MeshFilter) is the skinned surface s in model space: one bone (boneOf(model node))
        /// per node its vertices are weighted to. nodes: those model nodes, ascending. Null: no weighted vertex.
        internal static OriginalBikeSkin SkinPart(Transform part, Rfx.Surface s, Func<int, Transform> boneOf, out int[] nodes)
        {
            var mesh = part.GetComponent<MeshFilter>().sharedMesh;
            int n = s.positions.Length;
            var slotOf = new Dictionary<int, int>(); var bt = new List<Transform>();
            var slots = new int[n * 4]; var weights = new float[n * 4];
            for (int i = 0; i < n * 4; i++)
            {
                int node = s.bones[i]; float w = s.weights[i];
                if (node < 0 || w <= 0) { slots[i] = -1; continue; }
                if (!slotOf.TryGetValue(node, out int k)) { slotOf[node] = k = bt.Count; bt.Add(boneOf(node)); }
                slots[i] = k; weights[i] = w;
            }
            nodes = slotOf.Keys.OrderBy(x => x).ToArray();
            if (bt.Count == 0) return null;
            var skin = part.gameObject.AddComponent<OriginalBikeSkin>();
            skin.Bind(mesh, bt.ToArray(), slots, weights);
            return skin;
        }

        sealed class Loaded { public Rfx.Model model; public Rfx.Surface[] surfaces; }
        static readonly Dictionary<string, Loaded> loaded = new Dictionary<string, Loaded>(StringComparer.OrdinalIgnoreCase);
        /// Frees the parsed models kept for further bikes (OriginalOpponents builds a field of them).
        public static void ReleaseParsed() { lock (loaded) loaded.Clear(); }
        static Loaded Load(Rfx.Install install, string db, string pkg, string asset)
        {
            string key = db + "/" + pkg + "/" + asset;
            lock (loaded) if (loaded.TryGetValue(key, out var l)) return l;
            using var perf = Reflex.Port.Platform.ReflexMenuPerf.Measure("model read and parse");
            var p = install.Pkg(db, pkg); Rfx.Check(p != null, $"{db}/{pkg}");
            var model = Rfx.ParseModel(install.Read(p.Find(asset, "model") ?? throw new InvalidDataException($"{db}/{pkg}/{asset}.model missing")));
            var sa = p.Find(asset, "surface") ?? p.OfType("surface").FirstOrDefault(x => x.name.IndexOf("Hierarchy", StringComparison.OrdinalIgnoreCase) < 0)
                     ?? throw new InvalidDataException($"{db}/{pkg}/{asset}.surface missing");
            var surfaces = Rfx.ParseSurfaces(install.Read(sa));
            Rfx.Check(surfaces.Length == model.surfaceCount, "model / surface counts: " + asset);
            var r = new Loaded { model = model, surfaces = surfaces };
            lock (loaded) loaded[key] = r;
            return r;
        }
    }

    /// Unity meshes from the game's surfaces.
    internal static class ReflexMeshes
    {
        static float Clean(float v) => float.IsFinite(v) ? v : 0f;

        /// A surface placed by m (row-vector node matrices are already Unity column matrices in Rfx.Model): DX9 UV rows (top origin) -> (u, 1 - v);
        /// the second UV set (decals: the fork sticker, the plates) is uv2. skinned: bone weights over the model's nodes (bindposes given).
        /// boneMap: the model's node index -> the skeleton's (null: the same).
        public static Mesh Build(string name, Rfx.Surface s, Matrix4x4 m, bool skinned, Matrix4x4[] bindposes = null, int[] boneMap = null)
        {
            using var perf = Reflex.Port.Platform.ReflexMenuPerf.Measure("mesh made");
            int n = s.positions.Length;
            var pos = new Vector3[n]; var nrm = new Vector3[n]; var uv = new Vector2[n]; var uv2 = new Vector2[n];
            for (int i = 0; i < n; i++)
            {
                pos[i] = m.MultiplyPoint3x4(s.positions[i]);
                nrm[i] = m.MultiplyVector(s.normals[i]);   // node transforms are rigid
                uv[i] = new Vector2(Clean(s.uv0[i].x), 1 - Clean(s.uv0[i].y));
                uv2[i] = new Vector2(Clean(s.uv1[i].x), 1 - Clean(s.uv1[i].y));   // (some LOD fork legs hold a FLOAT16 NaN here)
            }
            var mesh = new Mesh { name = name, indexFormat = UnityEngine.Rendering.IndexFormat.UInt32 };
            mesh.vertices = pos; mesh.normals = nrm; mesh.uv = uv; mesh.uv2 = uv2;
            mesh.SetUVs(2, pos);   // the vertex as built (the rest pose: uv3 is not skinned): MATERIALS: ENHANCED's dirt mask and grain are anchored to it
            if (s.tangents != null)   // FLOAT3 tangents (usage 6): the vertex programs' tangent input (Reflex/Vehicle)
            {
                var tan = new Vector4[n];
                for (int i = 0; i < n; i++) { var t = m.MultiplyVector(s.tangents[i]); tan[i] = new Vector4(t.x, t.y, t.z, 1); }
                mesh.tangents = tan;
            }
            mesh.triangles = s.Triangles().ToArray();
            if (skinned && s.skinned)
            {
                var weights = new BoneWeight[n];
                for (int v = 0; v < n; v++)
                {
                    // strongest influence first (consumers read boneIndex0 as the dominant bone)
                    var k = new[] { 0, 1, 2, 3 }.OrderByDescending(j => s.weights[v * 4 + j]).ToArray();
                    var w = new BoneWeight();
                    for (int j = 0; j < 4; j++)
                    {
                        int bone = s.bones[v * 4 + k[j]]; float wt = s.weights[v * 4 + k[j]];
                        if (boneMap != null) bone = bone >= 0 && bone < boneMap.Length ? boneMap[bone] : -1;
                        if (wt <= 0 || bone < 0) { bone = 0; wt = 0; }
                        switch (j) { case 0: w.boneIndex0 = bone; w.weight0 = wt; break; case 1: w.boneIndex1 = bone; w.weight1 = wt; break; case 2: w.boneIndex2 = bone; w.weight2 = wt; break; default: w.boneIndex3 = bone; w.weight3 = wt; break; }
                    }
                    weights[v] = w;
                }
                mesh.boneWeights = weights; mesh.bindposes = bindposes;
            }
            mesh.RecalculateBounds();
            return mesh;
        }
    }
}
