// A .rxmod bike (Content/RxModBikes.cs, core/plan/RXMOD.md) built like an original one: the original bike's MX_Hierarchy (the core poses
// it: steering, fork travel, swingarm, wheel spin, levers) moves the mod's parts, each by the hierarchy node its surface is bound to;
// the mod's mesh is fitted to that hierarchy at load (its rear axle on the original's, its front axle on the original's axle line, scaled
// to the original's wheelbase unless the bike says "fit": "keep"), and its wheels to the original's tyre radius, so the wheels sit
// where the physics rolls them. Its materials are the original bike's (program, constants, mud) with the mod's maps.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEngine;
using Reflex.Port.OriginalWorld;

namespace Reflex.Port.Content
{
    public sealed partial class ReflexBikeModel
    {
        /// What the last .rxmod build measured (diagnostics / tests): the fit's scale and pitch, the steering axis offset from the original's,
        /// the tyre radius corrections.
        public string rxFit;

        /// Reads the mod's mesh off the main thread (the garage's Prefetch).
        static void PrefetchRx(BikeSpec spec)
        {
            try { RxModBikes.Mesh(spec.rx); Load(VehicleCatalog.Shared.install, spec.chassisDb, spec.chassisPkg, spec.chassisModel); } catch (Exception) { }
        }

        /// The original tyre's radius (its tyre-material surface's half height), 0 when it cannot be read.
        static float TyreRadius(Rfx.Install install, BikeSpec spec, string model)
        {
            try
            {
                var t = Load(install, spec.tireDb, spec.tirePkg, model);
                float best = 0;
                foreach (var b in t.model.Lod(0))
                {
                    var p = t.surfaces[b.surface].positions; if (p.Length == 0) continue;
                    float lo = p.Min(v => v.y), hi = p.Max(v => v.y);
                    best = Mathf.Max(best, (hi - lo) * 0.5f);
                }
                return best;
            }
            catch (Exception e) when (e is IOException || e is InvalidDataException) { return 0; }
        }

        /// The mod's wheel radius: the largest distance from the axle (in the YZ plane) of its wheel node's vertices.
        static float ModWheelRadius(RxMesh mesh, int node)
        {
            if (node < 0) return 0;
            Vector3 a = mesh.world[node].GetColumn(3); float r = 0;
            foreach (var (s, _, n) in mesh.surfaces)
                if (n == node && !s.skinned) foreach (var v in s.positions) r = Mathf.Max(r, new Vector2(v.y - a.y, v.z - a.z).magnitude);
            return r;
        }

        /// The mod space -> the original's model space: the rear axles together, the axle lines together (a pitch about X), scaled to the
        /// original's wheelbase ("axles") or not ("keep"), centred on the original's axle x.
        static Matrix4x4 RxFit(RxMesh mesh, Rfx.Model hier, string fit, out float scale, out float pitch)
        {
            int mf = mesh.NodeIndex("OBJ_FRT_Tire"), mr = mesh.NodeIndex("OBJ_BCK_Tire"), hf = hier.NodeIndex("OBJ_FRT_Tire"), hr = hier.NodeIndex("OBJ_BCK_Tire");
            if (mf < 0 || mr < 0) throw new InvalidDataException("the mod's mesh has no OBJ_FRT_Tire / OBJ_BCK_Tire node (its axles)");
            if (hf < 0 || hr < 0) throw new InvalidDataException("the original hierarchy has no tyre nodes");
            Vector3 Fm = mesh.world[mf].GetColumn(3), Rm = mesh.world[mr].GetColumn(3), F = hier.nodes[hf].world.GetColumn(3), R = hier.nodes[hr].world.GetColumn(3);
            var dm = new Vector2(Fm.z - Rm.z, Fm.y - Rm.y); var d = new Vector2(F.z - R.z, F.y - R.y);
            if (dm.magnitude < 0.5f) throw new InvalidDataException("the mod's axles are " + dm.magnitude.ToString("F2") + " ft apart (a bike's are about 4.8 ft: is it in feet, Z forward?)");
            scale = fit == "keep" ? 1f : d.magnitude / dm.magnitude;
            pitch = Mathf.Atan2(d.y, d.x) - Mathf.Atan2(dm.y, dm.x);
            float c = Mathf.Cos(pitch) * scale, s = Mathf.Sin(pitch) * scale;
            var M = Matrix4x4.identity;   // x' = scale x; z' = c z - s y; y' = s z + c y
            M.m00 = scale; M.m11 = c; M.m12 = s; M.m21 = -s; M.m22 = c;
            var center = (F.x + R.x) * 0.5f;
            return Matrix4x4.Translate(new Vector3(center, R.y, R.z)) * M * Matrix4x4.Translate(new Vector3(-(Fm.x + Rm.x) * 0.5f, -Rm.y, -Rm.z));
        }

        static ReflexBikeModel BuildRx(BikeSpec spec)
        {
            var cat = VehicleCatalog.Shared; var install = cat.install; var b = spec.rx;
            var mesh = RxModBikes.Mesh(b);
            var result = new ReflexBikeModel();
            // the original's hierarchy (the pose's nodes, in its order)
            var hierAsset = install.Pkg(spec.hierarchyDb ?? spec.chassisDb, spec.hierarchyPkg ?? spec.chassisPkg)?.Find(spec.hierarchyModel, "model")
                            ?? throw new InvalidDataException($"{spec.chassisDb}/{spec.chassisPkg}: no {spec.hierarchyModel}");
            var hier = Rfx.ParseModel(install.Read(hierAsset));
            result.hierarchyNodes = hier.nodes.Select(n => n.name).ToArray();
            result.hierarchyBind = hier.nodes.Select(n => n.world).ToArray();
            int frame = Math.Max(0, hier.NodeIndex("OBJ_Frame"));
            // each mod node's pose node: its namesake in the hierarchy, else its nearest ancestor's, else the frame
            var poseOf = new int[mesh.nodes.Length];
            for (int i = 0; i < mesh.nodes.Length; i++)
            {
                int j = i; while (j >= 0 && hier.NodeIndex(mesh.nodes[j]) < 0) j = mesh.parents[j];
                poseOf[i] = j < 0 ? frame : hier.NodeIndex(mesh.nodes[j]);
                if (j != i) Debug.LogWarning($"Reflex bike {b.mod.name}: node {mesh.nodes[i]} is not in {spec.hierarchyModel}: drawn on {hier.nodes[poseOf[i]].name}");
            }
            // "pose": "physics" (a bike of another size with its own hierarchy in the core: the Alive 50cc, ALIVE_BIKES.md): the mesh as it is, each
            // part bound to the mod's own node rest pose, which the core's pose of the vehicle has (rx_session_vehicle_physics_nodes)
            bool own = b.OwnNodes;
            float scale = 1f, pitch = 0f;
            var T = own ? Matrix4x4.identity : RxFit(mesh, hier, b.fit, out scale, out pitch);
            var bindW = hier.nodes.Select(n => n.world).ToArray();
            if (own)
                for (int i = 0; i < mesh.nodes.Length; i++) { int h = hier.NodeIndex(mesh.nodes[i]); if (h >= 0) bindW[h] = mesh.world[i]; }
            result.hierarchyBind = bindW;
            int hfn = hier.NodeIndex("OBJ_FRT_Tire"), hbn = hier.NodeIndex("OBJ_BCK_Tire");
            // no wheels in the mod (an MX Simulator / MX Bikes style model: its wheels are separate there): the original bike's tyres and rims
            bool ownWheels = mesh.surfaces.Any(x => !x.surface.skinned && (poseOf[x.node] == hfn || poseOf[x.node] == hbn));
            var R3 = Matrix4x4.Rotate(T.rotation);
            // the steering axes (the mod's after the fit against the original's): how far apart at the head, and their angle
            string steer = "";
            int mu = mesh.NodeIndex("OBJ_UPR_Forks"), hu = hier.NodeIndex("OBJ_UPR_Forks");
            if (mu >= 0 && hu >= 0)
            {
                Vector3 pm = T.MultiplyPoint3x4(mesh.world[mu].GetColumn(3)), am = R3.MultiplyVector(mesh.world[mu].GetColumn(1)).normalized;
                Vector3 ph = hier.nodes[hu].world.GetColumn(3), ah = ((Vector3)hier.nodes[hu].world.GetColumn(1)).normalized;
                Vector3 off = pm - ph; off -= Vector3.Dot(off, ah) * ah;
                steer = $", steering axis {off.magnitude * 12f:F2} in off the original's, {Vector3.Angle(am, ah):F1} deg apart";
            }
            // the wheels: scaled about their axles to the original tyres' radius (where the physics rolls them)
            var wheelScale = new Dictionary<int, float>();
            foreach (var (node, model) in ownWheels && !own ? new[] { ("OBJ_FRT_Tire", spec.frontModel), ("OBJ_BCK_Tire", spec.rearModel) } : new (string, string)[0])
            {
                int mi = mesh.NodeIndex(node); float want = TyreRadius(install, spec, model), have = ModWheelRadius(mesh, mi) * scale;
                float k = want > 0.3f && have > 0.3f ? want / have : 1f;
                if (k < 0.85f || k > 1.15f) { Debug.LogWarning($"Reflex bike {b.mod.name}: {node} radius {have:F3} ft against the original's {want:F3}: left as modelled"); k = 1f; }
                if (mi >= 0) wheelScale[mi] = k;
                steer += $", {node} {have * 12f:F2} in -> {want * 12f:F2} in";
            }
            result.rxFit = $"scale {scale:F4}, pitch {pitch * Mathf.Rad2Deg:F2} deg" + steer + (ownWheels ? "" : ", the original's wheels");
            Debug.Log($"Reflex bike {b.mod.name} / {b.name?.Full}{(spec.rxSkin != null ? " / skin " + spec.rxSkin.Title + " (" + spec.rxSkin.mod.name + ")" : "")}: {mesh.surfaces.Count} surfaces, {mesh.Vertices} vertices, {mesh.Triangles} triangles on {spec.chassisPkg}'s hierarchy: {result.rxFit}");

            var root = new GameObject($"RxMod {b.name?.Full ?? b.mod.name} chassis · body 0");
            result.chassis = root;
            GameObject Holder(string n, int hn) { var g = new GameObject(n); if (hn >= 0) g.transform.position = bindW[hn].GetColumn(3); return g; }
            var front = Holder("RxMod front wheel · body 2", hfn); var rear = Holder("RxMod rear wheel · body 3", hbn);
            result.frontWheel = front.transform; result.rearWheel = rear.transform;

            var materials = new Dictionary<string, UnityEngine.Material>(StringComparer.OrdinalIgnoreCase);
            var boneOf = new Dictionary<int, Transform>();
            Transform Bone(int j)
            {
                if (boneOf.TryGetValue(j, out var t)) return t;
                var go = new GameObject("bone " + mesh.nodes[j]); go.transform.SetParent(root.transform, false);
                result.bones.Add((go.transform, poseOf[j], bindW[poseOf[j]]));
                return boneOf[j] = go.transform;
            }
            for (int si = 0; si < mesh.surfaces.Count; si++)
            {
                var (src, mi, node) = mesh.surfaces[si];
                string matName = mesh.materials[mi];
                int pn = poseOf[node];
                bool wheel = !src.skinned && (pn == hfn || pn == hbn) && wheelScale.TryGetValue(node, out _);
                Vector3 axle = wheel ? (Vector3)hier.nodes[pn].world.GetColumn(3) : Vector3.zero; float k = wheel ? wheelScale[node] : 1f;
                var s = new Rfx.Surface { skinned = src.skinned, uv0 = src.uv0, uv1 = src.uv1, triangles = src.triangles, strip = src.strip, bones = src.bones, weights = src.weights,
                                          positions = new Vector3[src.positions.Length], normals = new Vector3[src.normals.Length] };
                for (int i = 0; i < s.positions.Length; i++)
                {
                    var p = T.MultiplyPoint3x4(src.positions[i]);
                    s.positions[i] = wheel ? axle + (p - axle) * k : p;
                    s.normals[i] = R3.MultiplyVector(src.normals[i]).normalized;
                }
                if (src.tangents != null) { s.tangents = new Vector3[src.tangents.Length]; for (int i = 0; i < s.tangents.Length; i++) s.tangents[i] = R3.MultiplyVector(src.tangents[i]); }
                if (!materials.TryGetValue(matName, out var mat)) materials[matName] = mat = RxMaterial(cat, install, spec, matName);   // (the skin's maps, if any)
                var parent = wheel ? (pn == hfn ? front.transform : rear.transform) : root.transform;
                var t = result.AddPart(parent, $"{Path.GetFileNameWithoutExtension(b.mesh)}_{si}_{hier.nodes[pn].name}", s, Matrix4x4.identity, pn, bindW[pn], matName, mat);
                if (wheel) t.position = Vector3.zero;   // (the holder sits on the axle: the parts stay at the model origin, as the original's)
                if (s.skinned && s.bones != null) result.Skin(t, s, node, Bone);
            }
            if (!ownWheels && !spec.atv && spec.tireDb != null) RxOriginalWheels(result, spec, install, cat, hier, front, rear);
            if (own)
            {
                result.rxFit = b.physicsPose ? "its own size and hierarchy (pose: physics)" : "its own nodes (fit: nodes)";
                Matrix4x4 Local(int i) => mesh.parents[i] < 0 ? mesh.world[i] : mesh.world[mesh.parents[i]].inverse * mesh.world[i];
                int mu2 = mesh.NodeIndex("OBJ_UPR_Forks"), ml = mesh.NodeIndex("OBJ_LWR_Forks"), mt = mesh.NodeIndex("OBJ_FRT_Tire");
                int hu2 = hier.NodeIndex("OBJ_UPR_Forks"), hl = hier.NodeIndex("OBJ_LWR_Forks"), ht = hier.NodeIndex("OBJ_FRT_Tire");
                if (b.physicsPose && mu2 >= 0 && ml >= 0 && mt >= 0 && hu2 >= 0 && hl >= 0 && ht >= 0 && mesh.parents[ml] == mu2 && mesh.parents[mt] == ml)
                {   // the fork onto the physics axle (OriginalBikePose.ForkFix): the nodes under the upper forks in the pose's hierarchy
                    bool Under(int i) { for (int p = hier.nodes[i].parent; p >= 0; p = hier.nodes[p].parent) if (p == hu2) return true; return false; }
                    result.forkFix = new OriginalBikePose.ForkFix { upr = hu2, lwr = hl, frt = ht, restUL = Local(ml), restLT = Local(mt),
                                                                     subtree = Enumerable.Range(0, hier.nodes.Length).Where(i => i != hl && i != ht && Under(i)).ToArray() };
                }
                // the rider's hands and feet on the bike's own grips and pegs (RiderLimbRetarget): its locators' change in their parents' frames
                // (the core's lean animations keep the original's locators)
                var limbs = new List<RiderLimbRetarget.Offset>();
                foreach (string n in new[] { "NDE_RHT_Hand", "NDE_LFT_Hand", "NDE_RHT_Foot", "NDE_LFT_Foot" })
                {
                    int i = hier.NodeIndex(n), j = mesh.NodeIndex(n);
                    if (i < 0 || j < 0 || hier.nodes[i].parent < 0 || mesh.parents[j] < 0) continue;
                    if (!string.Equals(hier.nodes[hier.nodes[i].parent].name, mesh.nodes[mesh.parents[j]], StringComparison.OrdinalIgnoreCase)) continue;
                    Vector3 d = (Vector3)(Local(j).GetColumn(3) - hier.nodes[i].local.GetColumn(3));
                    if (d.sqrMagnitude < 1e-8f || !RiderLimbRetarget.Chain(n, out int u, out int l, out int e)) continue;
                    limbs.Add(new RiderLimbRetarget.Offset { locator = n, node = i, parent = hier.nodes[i].parent, delta = d, upper = u, lower = l, end = e });
                }
                if (limbs.Count > 0) { result.limbOffsets = limbs.ToArray(); Debug.Log($"Reflex bike {b.mod.name}: its own grips and pegs: " + string.Join(", ", limbs.Select(o => o.locator + " " + o.delta.ToString("F3")))); }
            }
            var pose = root.AddComponent<OriginalBikePose>();
            pose.forkFix = result.forkFix;
            pose.parts = result.parts.Select(p => p.part).Concat(result.bones.Select(x => x.bone)).ToArray();
            pose.nodes = result.parts.Select(p => p.node).Concat(result.bones.Select(x => x.node)).ToArray();
            pose.inverseBind = result.binds.Concat(result.bones.Select(x => x.bind)).Select(w => w.inverse).ToArray();
            result.pose = pose;
            return result;
        }

        /// The original bike's tyres and rims on the wheel holders (as Build draws them: a tyre model is modelled in its node's space and hangs
        /// on the body's tyre node), for a mod without wheels of its own.
        static void RxOriginalWheels(ReflexBikeModel result, BikeSpec spec, Rfx.Install install, VehicleCatalog cat, Rfx.Model hier, GameObject front, GameObject rear)
        {
            var pool = spec.materials != null && spec.materials.Count > 0 ? spec.materials : spec.kitPkg != null ? new List<(string db, string pkg)> { (spec.kitDb, spec.kitPkg) } : new List<(string db, string pkg)>();
            var body = Load(install, spec.chassisDb, spec.chassisPkg, spec.chassisModel);
            var mats = new Dictionary<string, UnityEngine.Material>();
            foreach (var (asset, nodeName, holder) in new[] { (spec.frontModel, "OBJ_FRT_Tire", front), (spec.rearModel, "OBJ_BCK_Tire", rear) })
            {
                var tire = Load(install, spec.tireDb, spec.tirePkg, asset);
                int attach = body.model.NodeIndex(nodeName), pn = hier.NodeIndex(nodeName);
                Rfx.Check(attach >= 0 && pn >= 0 && tire.model.nodes.Length == 1, "the tyre's attachment node");
                var attachWorld = body.model.nodes[attach].world;
                foreach (var bd in tire.model.Lod(0))
                {
                    var s = tire.surfaces[bd.surface];
                    if (!mats.TryGetValue(bd.material, out var mat))
                    {
                        var def = GameMaterial(cat, bd.material, (spec.tireDb, spec.tirePkg), pool) ?? cat.Resolve(bd.material, (spec.tireDb, spec.tirePkg), pool);
                        mats[bd.material] = mat = cat.Build(def, bd.material + (def != null && def.name != bd.material ? " -> " + def.name : ""));
                    }
                    var t = result.AddPart(holder.transform, $"{asset}_{bd.surface}_{tire.model.nodes[bd.node].name}", s, s.skinned ? Matrix4x4.identity : attachWorld, pn, attachWorld, bd.material, mat);
                    t.position = Vector3.zero;
                }
            }
        }

        /// The pixel programs whose texture slots RxMaterial fills (each sampled as: SFX / combo map, diffuse, mud noise, the number plate or a
        /// fork sticker on the second UV set, the tyre's normal map).
        static readonly string[] RxPrograms = { "Template_Vehicle_Plstc_Fnl", "Template_MX_Frame_FNL", "Template_ATV_MX_Engine_FNL", "Template_MX_Wheel_Fnl", "Template_tire_FNL", "Template_ATV_MX_AlphaMtl_UI_FNL" };

        /// The original bike's binding a template borrows: the first binding of its chassis (or tyre) model whose name fits the template.
        static readonly (string template, Func<string, bool> fits, bool tyre)[] RxTemplates =
        {
            ("plastic", n => n.StartsWith("MX_Plstc", StringComparison.OrdinalIgnoreCase) || n.StartsWith("HR_MX_Plstc", StringComparison.OrdinalIgnoreCase), false),   // (the garage's HR twin: HR_MX_Plstc)
            ("frame", n => n.IndexOf("Frame", StringComparison.OrdinalIgnoreCase) >= 0, false),
            ("engine", n => n.IndexOf("_Eng", StringComparison.OrdinalIgnoreCase) >= 0, false),
            ("chain", n => n.IndexOf("Chain", StringComparison.OrdinalIgnoreCase) >= 0, false),
            ("wheel", n => n.IndexOf("Rim", StringComparison.OrdinalIgnoreCase) >= 0, true),
            ("tire", n => n.IndexOf("Tire", StringComparison.OrdinalIgnoreCase) >= 0 && n.IndexOf("Rim", StringComparison.OrdinalIgnoreCase) < 0, true),
        };

        /// A .rxmod material: the original bike's material of its template (as the game resolves that binding: the graphic kit first) with
        /// the mod's diffuse, its SFX map (from its spec map) and its normal map in place of the original's maps; the mud noise and the
        /// render states stay. Falls back to the plastics template, then to a plain grey.
        static UnityEngine.Material RxMaterial(VehicleCatalog cat, Rfx.Install install, BikeSpec spec, string name)
        {
            var b = spec.rx;
            var mm = RxModBikes.MaterialOf(b, spec.rxSkin, name);   // (painted by the chosen skin, RxSkin)
            if (mm == null) { Debug.LogWarning($"Reflex bike {b.mod.name}: material {name} is not in mod.json (plastic, no maps)"); mm = new RxModMaterial { name = name, diffuseMod = b.mod, specMod = b.mod, normalMod = b.mod, sfxMod = b.mod }; }
            var pool = spec.materials != null && spec.materials.Count > 0 ? spec.materials : spec.kitPkg != null ? new List<(string db, string pkg)> { (spec.kitDb, spec.kitPkg) } : new List<(string db, string pkg)>();
            // the programs whose texture slots a .rxmod's maps are known to fit (core/plan/RXMOD.md "Materials"); another program (the KTM 450's
            // Template_ATV_MX_Plstc: a dirt mask and a wet-mud normal map) is not borrowed: the chassis' own material, else the reference bike's
            bool Fits(VehicleMaterialDef d) => d != null && Array.IndexOf(RxPrograms, d.pixelShader) >= 0;
            VehicleMaterialDef Template(string template)
            {
                var e = RxTemplates.FirstOrDefault(x => x.template == template); if (e.fits == null) return null;
                try
                {
                    if (spec.atv || (e.tyre && spec.tireDb == null)) goto reference;   // (an ATV: the motocross programs of the reference bike)
                    var own = e.tyre ? (spec.tireDb, spec.tirePkg) : (spec.chassisDb, spec.chassisPkg);
                    var model = e.tyre ? Load(install, spec.tireDb, spec.tirePkg, spec.frontModel) : Load(install, spec.chassisDb, spec.chassisPkg, spec.chassisModel);
                    string binding = model.model.Lod(0).Select(x => x.material).FirstOrDefault(e.fits);
                    if (binding != null)
                    {
                        var kit = GameMaterial(cat, binding, own, pool); if (Fits(kit)) return kit;
                        var mine = cat.Material(own.Item1, own.Item2, binding); if (Fits(mine)) return mine;
                    }
                    // the reference bike every install has (the MarsF Lite with THQ tyres: BikeSpec.MarsFLite)
                    reference:
                    var r = BikeSpec.MarsFLite; var rown = e.tyre ? (r.tireDb, r.tirePkg) : (r.chassisDb, r.chassisPkg);
                    var rmodel = e.tyre ? Load(install, r.tireDb, r.tirePkg, r.frontModel) : Load(install, r.chassisDb, r.chassisPkg, r.chassisModel);
                    string rb = rmodel.model.Lod(0).Select(x => x.material).FirstOrDefault(e.fits);
                    var rpool = new List<(string db, string pkg)> { (r.kitDb, r.kitPkg) };
                    var rd = rb == null ? null : GameMaterial(cat, rb, rown, rpool) ?? cat.Material(rown.Item1, rown.Item2, rb);
                    return Fits(rd) ? rd : null;
                }
                catch (Exception ex) when (ex is IOException || ex is InvalidDataException) { Debug.LogWarning($"Reflex bike {b.mod.name}: template {template}: {ex.Message}"); return null; }
            }
            var t = Template(mm.template) ?? Template("plastic");
            if (t == null) { Debug.LogWarning($"Reflex bike {b.mod.name}: material {name}: {spec.chassisPkg} has no {mm.template} / plastic binding to borrow (a plain grey)"); return cat.Build(null, "rxmod " + name); }
            var d = new VehicleMaterialDef { db = t.db, pkg = t.pkg, name = t.name, pixelShader = t.pixelShader, package = t.package, cull = mm.twoSided ? 1 : t.cull, alphaRef = mm.alphaTest ? mm.alphaRef : t.alphaRef,
                                             alphaFunc = mm.alphaTest ? 7 : t.alphaFunc, srcBlend = t.srcBlend, destBlend = t.destBlend, zwrite = t.zwrite, alphaTest = mm.alphaTest || t.alphaTest, alphaBlend = t.alphaBlend };
            foreach (var kv in t.parameters) d.parameters[kv.Key] = kv.Value;
            int diffuse = -1;
            foreach (var x in t.textures)
            {
                var c = new VehicleMaterialDef.Tex { name = x.name, sampler = x.sampler, addrU = x.addrU, addrV = x.addrV, mipFilter = x.mipFilter, magFilter = x.magFilter, minFilter = x.minFilter, maxAniso = x.maxAniso, mipBias = x.mipBias, asset = x.asset };
                string n = x.name ?? "";
                if (n.IndexOf("Mud_Noise", StringComparison.OrdinalIgnoreCase) >= 0) { }
                else if (n.Equals("Numbers", StringComparison.OrdinalIgnoreCase) || n.IndexOf("namnum", StringComparison.OrdinalIgnoreCase) >= 0)
                { if (!mm.numbers) { c.name = "rxmod no plate"; c.external = VehicleCatalog.ClearTexture; } }   // ("numbers": the original's binding: the rider's plate)   // (the plates are in the mod's own diffuse: no run-time number on its second UV set)
                else if (n.EndsWith("_SFX", StringComparison.OrdinalIgnoreCase) || n.IndexOf("Combo", StringComparison.OrdinalIgnoreCase) >= 0) c.external = RxModBikes.Sfx(mm) ?? VehicleCatalog.ClearTexture;
                else if (n.IndexOf("NRML", StringComparison.OrdinalIgnoreCase) >= 0 || n.IndexOf("_Norm", StringComparison.OrdinalIgnoreCase) >= 0) c.external = RxModBikes.Normal(mm) ?? FlatNormal;
                else { c.external = VehicleCatalog.ClearTexture; diffuse = d.textures.Count; }   // (a fork sticker on the frame's second UV set: clear)
                c.addrU = 1; c.addrV = 1;   // (the mod's maps repeat; its UVs are in 0..1)
                d.textures.Add(c);
            }
            if (diffuse >= 0) d.textures[diffuse].external = RxModBikes.Map(mm.diffuseMod, mm.diffuse) ?? Texture2D.grayTexture;
            var built = cat.Build(d, "rxmod " + name + " (" + mm.template + ": " + t.name + ")");
            // clean until the race's mud record says otherwise (OriginalWorldRuntime / OriginalOpponents set usr_mud_amount every frame; the
            // garage and the menus never do, and the program's default would draw the mod's whole mud mask)
            if (built && built.HasProperty("usr_mud_amount")) built.SetVector("usr_mud_amount", Vector4.zero);
            return built;
        }

        /// An original material with a .rxmod skin's map in place of its diffuse texture (RxSkin.OriginalMap: by the texture's name or role);
        /// the material as it is when the skin does not paint it.
        static VehicleMaterialDef RxOriginalSkin(VehicleMaterialDef def, RxSkin skin)
        {
            if (def == null) return null;
            var tex = def.Sampler("diffuse") ?? def.textures.FirstOrDefault(t => t.name != null && !new[] { "SFX", "Numbers", "Mud_Noise", "NRML", "Norm", "Combo", "Fork", "namnum", "dirtmask", "Mud_WET" }.Any(x => t.name.IndexOf(x, StringComparison.OrdinalIgnoreCase) >= 0));
            string entry = skin.OriginalMap(tex?.name, def.name);
            if (entry == null) return def;
            var map = RxModBikes.Map(skin.mod, entry); if (!map) return def;
            var d = new VehicleMaterialDef { db = def.db, pkg = def.pkg, name = def.name, pixelShader = def.pixelShader, package = def.package, cull = def.cull, alphaRef = def.alphaRef,
                                             alphaFunc = def.alphaFunc, srcBlend = def.srcBlend, destBlend = def.destBlend, zwrite = def.zwrite, alphaTest = def.alphaTest, alphaBlend = def.alphaBlend };
            foreach (var kv in def.parameters) d.parameters[kv.Key] = kv.Value;
            foreach (var x in def.textures)
                d.textures.Add(new VehicleMaterialDef.Tex { name = x.name, sampler = x.sampler, addrU = x.addrU, addrV = x.addrV, mipFilter = x.mipFilter, magFilter = x.magFilter, minFilter = x.minFilter,
                                                            maxAniso = x.maxAniso, mipBias = x.mipBias, asset = x.asset, external = ReferenceEquals(x, tex) ? map : x.external });
            return d;
        }

        static Texture2D flatNormal;
        /// A flat normal (the tyre program's map when the mod has none): signed X / Y = 0.
        static Texture2D FlatNormal
        {
            get
            {
                if (!flatNormal) { flatNormal = new Texture2D(4, 4, TextureFormat.RGHalf, false, true) { name = "rxmod flat normal", hideFlags = HideFlags.DontSave }; flatNormal.SetPixels(Enumerable.Repeat(new Color(0, 0, 0, 1), 16).ToArray()); flatNormal.Apply(); }
                return flatNormal;
            }
        }
    }
}
