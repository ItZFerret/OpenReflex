// The player's rider, built at run time from the player's own Reflex install: the MXRider_Hierarchy skeleton (69 nodes) and the
// gear models on it, each binding's material resolved as the game does (the outfit's material packages, the skin tone).
// Ported from _re/formats/rider_asset_export.py (parts, bone palettes -> hierarchy node indices, UBYTE4N weights) and
// Editor/OriginalWorld/RiderAssetBuilder (bones[i] == hierarchy node i, bindposes[i] = inverse(bindWorld[i]) in Unity's column
// convention; rigid helmet / goggles / neck brace hang on nodes 35 / 36 / 7). OriginalRider poses it from reflex_world_rider_pose, and
// OriginalOnboardCamera reads the head / eye bones (31, 32, 33) and the bone-weight dominant bones for the cockpit view.
// The gear the captured world uses is the default set of MXTables_Default (RIDER_FORMAT.md): the Answer red outfit, white skin.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEngine;
using Reflex.Port.OriginalWorld;

namespace Reflex.Port.Content
{
    public sealed class RiderSpec
    {
        public sealed class Part { public string key, modelDb, modelPkg, asset, matsDb, matsPkg; }
        public string hierarchyDb = "Rider", hierarchyPkg = "Rider", hierarchyAsset = "MXRider_Hierarchy";
        public string skinDb = "Rider_Skin_Materials", skinPkg = "Rider_Skin_White";
        public List<Part> parts = new List<Part>();
        /// Material packages of pieces without a model (offered to the material override with the outfit's).
        public List<(string db, string pkg)> extraMaterials = new List<(string db, string pkg)>();
        public int[] gearIds;   // the RiderGear ids it was made from (ReflexVehicleSpecs.Rider), null for a fixed outfit
        /// The AIRiderId whose Name / Number the plates carry (a pro rider on the stage); -1: the profile's own rider (not part of Key: the plates are separate textures).
        public int plateRider = -1;
        /// Equal keys build equal riders.
        public string Key => string.Join("|", hierarchyDb, hierarchyPkg, hierarchyAsset, skinDb, skinPkg,
            string.Join(",", parts.Select(p => $"{p.key}:{p.modelDb}/{p.modelPkg}/{p.asset}:{p.matsDb}/{p.matsPkg}")), string.Join(",", extraMaterials.Select(p => p.db + "/" + p.pkg)));

        /// The Answer red outfit with white skin (RIDER_FORMAT.md section 1).
        public static RiderSpec AnswerRed()
        {
            var s = new RiderSpec();
            void Add(string key, string mdb, string mpkg, string asset, string tdb, string tpkg) => s.parts.Add(new Part { key = key, modelDb = mdb, modelPkg = mpkg, asset = asset, matsDb = tdb, matsPkg = tpkg });
            Add("jersey", "Rider_Jerseys_Models", "Jerseys_BaseModel", "MXRider_Jersey", "Rider_Jerseys_Materials", "Jersey_Answer_Alpha_Red");
            Add("helmet", "Rider_Helmets_Models", "Helmet_Answer", "MXRider_Helmet_Answer", "Rider_Helmets_Materials", "Helmet_Answer_Alpha_Red");
            Add("goggles", "Rider_Goggles_Models", "Goggles_Answer", "MXRider_Goggles_Answer", "Rider_Goggles_Materials", "Goggles_Answer_Red");
            Add("boots", "Rider_Boots_Models", "Boots_Answer", "MXRider_Boots_Answer", "Rider_Boots_Materials", "Boots_Answer_White");
            Add("chest_protector", "Rider_CPs_Models", "CPs_Answer", "MXRider_Chstptr_MSR", "Rider_CPs_Materials", "ChestProtector_Answer_Red");
            return s;
        }
    }

    public sealed class ReflexRiderModel
    {
        public GameObject root;
        public OriginalRider rider;
        /// The fallback capsule rider (shown until the game's pose arrives) and the object that follows rider body 1.
        public Transform proxyRoot; public GameObject proxyArt;
        public readonly List<(string name, string material, string resolved)> parts = new List<(string, string, string)>();

        public void Destroy()
        {
            foreach (var r in new[] { root, proxyRoot ? proxyRoot.gameObject : null })
                if (r)
                {
                    foreach (var f in r.GetComponentsInChildren<MeshFilter>(true)) if (f.sharedMesh) UnityEngine.Object.Destroy(f.sharedMesh);
                    foreach (var s in r.GetComponentsInChildren<SkinnedMeshRenderer>(true)) if (s.sharedMesh) UnityEngine.Object.Destroy(s.sharedMesh);
                    foreach (var rr in r.GetComponentsInChildren<Renderer>(true)) foreach (var m in rr.sharedMaterials) if (m) UnityEngine.Object.Destroy(m);
                    UnityEngine.Object.Destroy(r);
                }
        }

        /// The capsule stand-in of the rider (body 1 of the game's physics), shown until the original rider pose arrives.
        public static ReflexRiderModel BuildProxy()
        {
            var r = new ReflexRiderModel();
            var proxyRoot = new GameObject("Rider body 1"); r.proxyRoot = proxyRoot.transform;
            var mat = ReflexObjectShading.Fallback(new Color(0.72f, 0.76f, 0.81f), "Rider proxy");
            var art = new GameObject("Rider PROXY capsule (fallback)"); art.transform.SetParent(proxyRoot.transform, false);
            void Prim(string name, PrimitiveType type, Vector3 p, Vector3 s)
            {
                var item = GameObject.CreatePrimitive(type); item.name = name; item.transform.SetParent(art.transform, false);
                item.transform.localPosition = p; item.transform.localScale = s;
                UnityEngine.Object.Destroy(item.GetComponent<Collider>()); item.GetComponent<Renderer>().sharedMaterial = mat;
            }
            Prim("Torso proxy", PrimitiveType.Capsule, new Vector3(0, 0.45f, 0), new Vector3(0.85f, 1.15f, 0.65f));
            Prim("Helmet proxy", PrimitiveType.Sphere, new Vector3(0, 1.8f, 0.2f), Vector3.one * 0.82f);
            r.proxyArt = art;
            return r;
        }

        /// The skeleton and the gear, unparented at the origin in the bind pose. proxy: shown by OriginalRider until a pose arrives.
        public static ReflexRiderModel Build(RiderSpec spec = null, GameObject proxy = null)
        {
            spec ??= RiderSpec.AnswerRed();
            var cat = VehicleCatalog.Shared; var install = cat.install;
            var result = new ReflexRiderModel();

            // ---- skeleton (MXRider_Hierarchy: 69 nodes) ----
            var hp = install.Pkg(spec.hierarchyDb, spec.hierarchyPkg); Rfx.Check(hp != null, "the rider hierarchy package");
            using var perfAll = Reflex.Port.Platform.ReflexMenuPerf.Measure("rider model built");
            var hier = Rfx.ParseModel(install.Read(hp.Find(spec.hierarchyAsset, "model") ?? throw new InvalidDataException("MXRider_Hierarchy missing")));
            int count = hier.nodes.Length;
            Rfx.Check(count == OriginalRider.OriginalBoneCount, "the rider hierarchy has 69 nodes");
            var root = new GameObject("Original rider (MXRider_Hierarchy, original animation)");
            result.root = root;
            var skeletonRoot = new GameObject("Skeleton"); skeletonRoot.transform.SetParent(root.transform, false);
            var bones = new Transform[count]; var parents = new int[count]; var bindWorld = new float[count * 16]; var bindposes = new Matrix4x4[count];
            for (int i = 0; i < count; i++)
            {
                var n = hier.nodes[i];
                var go = new GameObject(n.name);
                parents[i] = n.parent;
                go.transform.SetParent(n.parent < 0 ? skeletonRoot.transform : bones[n.parent], false);
                var w = n.world;   // Unity column matrix; the game's row-major row-vector layout is its transpose
                for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) bindWorld[i * 16 + r * 4 + c] = w[c, r];
                Vector3 right = w.GetColumn(0), up = w.GetColumn(1), fwd = w.GetColumn(2), pos = w.GetColumn(3);
                // mesh nodes (Jersey / Gloves / Boots) carry a uniform scale: identity rotation when the basis is not unit length
                bool rigid = Mathf.Abs(right.magnitude - 1) < 1e-3f && Mathf.Abs(up.magnitude - 1) < 1e-3f && Mathf.Abs(fwd.magnitude - 1) < 1e-3f;
                go.transform.SetPositionAndRotation(pos, rigid ? Quaternion.LookRotation(fwd, up) : Quaternion.identity);
                bones[i] = go.transform;
                bindposes[i] = w.inverse;
            }

            // ---- gear ----
            var chosen = spec.parts.Where(p => p.matsPkg != null).Select(p => (p.matsDb, p.matsPkg)).Concat(spec.extraMaterials).Append((spec.skinDb, spec.skinPkg)).ToList();
            var materials = new Dictionary<string, UnityEngine.Material>();
            var skinned = new List<SkinnedMeshRenderer>(); var rigid2 = new List<Renderer>();
            foreach (var part in spec.parts)
            {
                var pk = install.Pkg(part.modelDb, part.modelPkg); Rfx.Check(pk != null, part.modelDb + "/" + part.modelPkg);
                // a gear package holds one model (the tables name the package): the spec's asset, else that model
                string asset = part.asset ?? pk.OfType("model").Select(a => a.name).FirstOrDefault(n => n.IndexOf("Hierarchy", StringComparison.OrdinalIgnoreCase) < 0)
                               ?? throw new InvalidDataException(part.modelDb + "/" + part.modelPkg + ": no gear model");
                var (model, surfaces) = LoadGear(install, pk, asset);
                // the model's own nodes -> the rider hierarchy's, by name (skinned weights index the model's nodes)
                var map = model.nodes.Select(n => hier.NodeIndex(n.name)).ToArray();
                bool identity = map.Length <= count && map.Select((h, i) => h == i).All(x => x);
                foreach (var b in model.Lod(0))
                {
                    var s = surfaces[b.surface];
                    string name = $"{asset}_lod0_s{b.surface}_{b.material}";
                    string key = $"{part.key}:{b.material}";
                    if (!materials.TryGetValue(key, out var mat))
                    {
                        var def = cat.Resolve(b.material, (part.modelDb, part.modelPkg), chosen);
                        if (def == null) Debug.LogWarning("Reflex rider: no material for binding " + b.material);
                        materials[key] = mat = cat.Build(def, key + (def != null ? " -> " + def.name : ""));
                        result.parts.Add((name, b.material, def?.Key));
                    }
                    var go = new GameObject(name);
                    if (s.skinned)
                    {
                        go.transform.SetParent(root.transform, false);
                        var smr = go.AddComponent<SkinnedMeshRenderer>();
                        smr.sharedMesh = ReflexMeshes.Build(name, s, Matrix4x4.identity, true, bindposes, identity ? null : map);
                        smr.sharedMaterial = mat; smr.bones = bones; smr.rootBone = bones[0];
                        smr.updateWhenOffscreen = true;   // native world coordinates are far from the origin; keep bounds honest
                        skinned.Add(smr);
                    }
                    else
                    {
                        // rigid: v_model * world[node]; the model's single node is named like its rider hierarchy node (Helmet 35, Goggles 36)
                        int node = hier.NodeIndex(model.nodes[b.node].name);
                        Rfx.Check(node >= 0, "rigid gear node " + model.nodes[b.node].name);
                        go.transform.SetParent(bones[node], false);
                        // in the posed node's frame only, as the game draws it (v * world[node]); the gear model's own node matrix is not used.
                        // The neck braces' (Leatt, Alpinestar, HR_) is the chest bone's bind frame, their vertices 0.26..1.07 up its x axis:
                        // where every clip of the install puts NeckBrace (local identity under MXRider_Chest; the core's pose has world[7] ==
                        // world[6]), not MXRider_Hierarchy's bind NeckBrace (turned 90 degrees), so placing the brace by the difference of the
                        // two turned it a second time, beside the shoulder. The other differences are leftovers the game ignores
                        // (Goggles_Thor_Ally's node is mirrored, Goggles_Spy's scaled 0.886, Helmet_One_Trooper's 0.963, Helmet_Fox_V2's tilted 2 deg)
                        go.AddComponent<MeshFilter>().sharedMesh = ReflexMeshes.Build(name, s, Matrix4x4.identity, false);
                        var mr = go.AddComponent<MeshRenderer>(); mr.sharedMaterial = mat; rigid2.Add(mr);
                    }
                }
            }
            var rider = root.AddComponent<OriginalRider>();
            rider.skinnedMeshes = skinned.ToArray(); rider.skinnedMesh = skinned.FirstOrDefault();
            rider.rigidMeshes = rigid2.ToArray(); rider.bones = bones; rider.boneParents = parents; rider.bindWorldMatrices = bindWorld;
            rider.proxy = proxy;
            rider.ClearOriginalPose();   // proxy visible, the gear hidden until the game's pose arrives
            result.rider = rider;
            return result;
        }

        static readonly Dictionary<string, (Rfx.Model, Rfx.Surface[])> parsed = new Dictionary<string, (Rfx.Model, Rfx.Surface[])>(StringComparer.OrdinalIgnoreCase);
        /// Frees the parsed gear models kept for further riders (a field of AI riders shares most of them).
        public static void ReleaseParsed() { lock (parsed) parsed.Clear(); }
        static (Rfx.Model, Rfx.Surface[]) LoadGear(Rfx.Install install, Rfx.Package pk, string asset)
        {
            string key = pk.database.name + "/" + pk.name + "/" + asset;
            lock (parsed) if (parsed.TryGetValue(key, out var r)) return r;
            using var perf = Reflex.Port.Platform.ReflexMenuPerf.Measure("model read and parse");
            var model = Rfx.ParseModel(install.Read(pk.Find(asset, "model") ?? throw new InvalidDataException(asset + ".model missing")));
            var surfaces = Rfx.ParseSurfaces(install.Read(pk.Find(asset, "surface") ?? pk.OfType("surface").FirstOrDefault() ?? throw new InvalidDataException(asset + ".surface missing")));
            Rfx.Check(surfaces.Length == model.surfaceCount, "model / surface counts: " + asset);
            lock (parsed) parsed[key] = (model, surfaces);
            return (model, surfaces);
        }
    }
}
