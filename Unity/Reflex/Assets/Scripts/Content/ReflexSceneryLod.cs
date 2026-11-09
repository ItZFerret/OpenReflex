// Scenery LOD and draw distance as the original renderer decides them (WP-8, _re/notes/props.md section 3). Partial of
// ReflexLevelContent: the per-model data read from the install, and the extra LOD meshes / materials (BuildScenery makes LOD0 only).
//  - Model resource (0x88aabb00 v4): +168 the draw distance (uint, ModelAsset +0x120); 0 means "from the bounds": the static model
//    loader 007d4650 (007d4bbe..007d4c10) stores trunc(radius * 135.0) (0x92e20c), radius = the root node record's bounding radius
//    (FUN_007dd450; file node record +8), clamped to 0xffff. LOD records (count, first binding, distance) = ModelAsset +0x130
//    (0x1c-byte entries, distance first), count +0x118. Verified on Concord: 107 / 107 ModelEntity / CollisionEntity models read
//    from the running world (props_test PROPS_LOD) equal this.
//  - Model_Submit (007f1b00, every static ModelEntity: ModelEntity::vf2_o90): d2 = round(|bounds centre * world - camera|^2) (uint;
//    centre = the root node record +12; camera [[0xa95844+0x18]]+0x10); D = draw distance / the renderer divisor (1: one view);
//    AlwaysVisible ((ModelInstance +0x10 & 0x18) == 0x10, scene "AlwaysVisible"; PhysicsEntity clears it) never culls and never fades;
//    else D != 0 && D^2 < d2 culls; the alpha byte is 255 - trunc((d2 - 0.7 D^2) / (D^2 - 0.7 D^2) * 255) once d2 > 0.7 D^2 (0x9c4b4c),
//    drawn by PlatformModelAllSemiTransEvent while below 255. LOD = Model_LodForDistance (007f8420): the highest i >= 1 with
//    lod[i].distance^2 <= d2, else 0.
//  - The instance's MaterialList replaces LOD0's binding materials one to one (ModelInstance_Init 007f12c0 builds per-LOD remap tables
//    from it): every LOD uses scene binding indices, capped by the LOD0 difference count; null handles are skipped.
using System;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using UnityEngine.Rendering;

namespace Reflex.Port.Content
{
    public sealed partial class ReflexLevelContent
    {
        /// What the original's static model submit needs of a model (file values).
        public sealed class ModelLodData
        {
            public string name;
            public uint drawDistance;        // ModelAsset +0x120 (0: never culled)
            public uint[] lodDistances;      // per LOD (LOD0 first)
            public Vector3 boundsCentre;     // root node record, model space
            public float radius;
            internal readonly List<ModelData> lods = new List<ModelData>();   // LOD1.. geometry (LOD0 is BuildScenery's)
            public int ExtraLodMeshes => lods.Count(x => x != null);
        }

        /// One drawn scenery instance with its LOD state (OriginalSceneryLod).
        public sealed class LodInstance
        {
            public SceneryInstance instance; public MeshFilter filter; public MeshRenderer renderer;
            public ModelLodData model; public bool alwaysVisible, dynamic;
            public Mesh[] meshes; public Material[][] materials;          // per LOD (null entries: LOD0's)
            public int lod; public byte alpha = 255; public bool faded;   // faded: drawn through OriginalSceneryLod's blended clones
            public int lodBack = -1, lodBackFrame;   // tests (OriginalSceneryLod.collectStats): the LOD this instance left last, and when
            public bool shadowOnly;   // past its draw distance but still a shadow caster (DRAW DISTANCE HIGH / ULTRA)
        }

        /// Reads the draw distances, LOD distances and bounds of every placed model, and the extra LODs' geometry (thread-safe, no Unity
        /// objects: call from a worker thread).
        public Dictionary<string, ModelLodData> ReadLodData()
        {
            var result = new Dictionary<string, ModelLodData>(StringComparer.Ordinal);
            foreach (var model in instances.Select(i => i.model).Distinct())
            {
                try
                {
                    var b = ReadAsset(model, "model");
                    var d = ParseLodData(model, b);
                    if (d.lodDistances.Length > 1)
                        for (int l = 1; l < d.lodDistances.Length; l++)
                            try { d.lods.Add(LoadModelLod(model, b, l)); } catch (Exception e) { d.lods.Add(null); Warn($"model {model} LOD{l}: {e.Message}"); }
                    result[model] = d;
                }
                catch (Exception e) { Warn($"model {model} LOD data: {e.Message}"); }
            }
            return result;
        }

        public static ModelLodData ParseLodData(string name, byte[] b)
        {
            Rfx.Check(b.Length >= 172 && Rfx.U32(b, 0) == 0x88aabb00 && Rfx.I32(b, 4) == 4, "model header");
            int count = Rfx.I32(b, 12), namesSize = Rfx.I32(b, 16), lodCount = Rfx.I32(b, 20);
            var d = new ModelLodData { name = name, lodDistances = new uint[lodCount] };
            int rec = 172 + count * 64;                                  // node 0 record: name, parent, radius, centre xyz, 1, extent xyz, 1
            d.radius = Rfx.F32(b, rec + 8);
            d.boundsCentre = new Vector3(Rfx.F32(b, rec + 12), Rfx.F32(b, rec + 16), Rfx.F32(b, rec + 20));
            uint draw = Rfx.U32(b, 168);
            if (draw == 0) { double v = Math.Floor((double)d.radius * 135.0); draw = v >= 0xffff ? 0xffffu : v <= 0 ? 0u : (uint)v; }   // x87 extended product, truncated
            d.drawDistance = Math.Min(draw, 0xffffu);
            int p = 172 + count * 108 + namesSize;
            for (int i = 0; i < lodCount; i++) d.lodDistances[i] = Rfx.U32(b, p + 12 * i + 8);
            return d;
        }

        /// The geometry of LOD `lod` as BuildScenery makes LOD0 (node transforms applied, uv flipped, one submesh per binding).
        ModelData LoadModelLod(string name, byte[] modelBytes, int lod)
        {
            var model = Rfx.ParseModel(modelBytes);
            var surfaces = Rfx.ParseSurfaces(ReadAsset(name, "surface"));
            var md = new ModelData { name = name + " LOD" + lod, lodCount = model.lods.Length };
            var pos = new List<Vector3>(); var nrm = new List<Vector3>(); var tan = new List<Vector3>(); var uv0 = new List<Vector2>(); var uv1 = new List<Vector2>(); var col = new List<Color32>();
            bool anyTan = false, anyCol = false;
            foreach (var bnd in model.Lod(lod))
            {
                var s = surfaces[bnd.surface]; var w = model.nodes[bnd.node].world;
                Rfx.Check(!s.skinned, "skinned surface");
                int offset = pos.Count;
                for (int i = 0; i < s.positions.Length; i++)
                {
                    pos.Add(w.MultiplyPoint3x4(s.positions[i])); nrm.Add(w.MultiplyVector(s.normals[i]));
                    tan.Add(s.tangents != null ? w.MultiplyVector(s.tangents[i]) : Vector3.right);
                    uv0.Add(new Vector2(s.uv0[i].x, 1f - s.uv0[i].y)); uv1.Add(new Vector2(s.uv1[i].x, 1f - s.uv1[i].y));
                    col.Add(s.colors != null ? s.colors[i] : new Color32(255, 255, 255, 255));
                }
                anyTan |= s.tangents != null; anyCol |= s.colors != null;
                md.triangles.Add(s.Triangles().Select(t => t + offset).ToArray());
                md.bindingMaterials.Add(bnd.material);
            }
            Rfx.Check(md.triangles.Count > 0, "LOD has no bindings");
            md.positions = pos.ToArray(); md.normals = nrm.ToArray(); md.uv0 = uv0.ToArray(); md.uv1 = uv1.ToArray();
            md.tangents = anyTan ? tan.ToArray() : null; md.colors = anyCol ? col.ToArray() : null;
            return md;
        }

        /// The LOD state of every placed instance (main thread, after BuildScenery and ReadLodData): the extra LOD meshes, their
        /// materials (loaded now when LOD0 did not use them), the AlwaysVisible flags.
        public List<LodInstance> BuildLods(Dictionary<string, ModelLodData> data)
        {
            var list = new List<LodInstance>();
            if (data == null) return list;
            var byId = new Dictionary<int, SceneEntity>();
            foreach (var e in placed) byId[e.id] = e;
            Shader sceneryShader = materialObjects.Values.Select(m => m ? m.shader : null).FirstOrDefault(s => s && s.name == "Reflex/Original Scenery");
            var shaders = new SceneryShaders { scenery = sceneryShader };
            var meshCache = new Dictionary<(string, int), Mesh>();
            Material MaterialFor(string n)
            {
                if (materialObjects.TryGetValue(n, out var m)) return m;
                materialObjects[n] = null;
                try
                {
                    if (!sceneMaterials.TryGetValue(n, out var lm)) { lm = LoadMaterial(n); sceneMaterials[n] = lm; }
                    foreach (var t in lm.textures)
                        if (!string.IsNullOrEmpty(t) && !sceneTextures.ContainsKey(t))
                            try { var lt = ReflexTex.Parse(t, ReadAsset(t, "texture")); if (lt != null) sceneTextures[t] = lt; } catch (Exception e) { Warn($"texture {t}: {e.Message}"); }
                    m = ReflexSceneryMaterial.Create(this, lm, shaders, TextureFor, CubeFor);
                    if (m) { Track(m); materialObjects[n] = m; }
                }
                catch (Exception e) { Warn($"LOD material {n}: {e.Message}"); }
                return materialObjects[n];
            }
            foreach (var inst in instances)
            {
                if (!inst.transform || !data.TryGetValue(inst.model, out var d)) continue;
                var mf = inst.transform.GetComponent<MeshFilter>(); var mr = inst.transform.GetComponent<MeshRenderer>();
                if (!mf || !mr) continue;
                bool physics = inst.entityType == "PhysicsRSData" || inst.entityType == "PopUpBaleRSData" || inst.entityType == "PopDownBaleRSData";
                // the gate selection's cards and arrow (UIGateSelectionRSData) are moved to the start gate by the page (OriginalGateCards): their draw
                // distance is measured where they are now, as the original's Model_SubmitSkinned does with the instance's matrix every frame. Their
                // place in the level file is 370 .. 1200 ft from the gate on most tracks, so a centre taken there (when the LOD data was read before
                // the page first moved them) culled the arrow and the cards for the rest of the level (w-uibatch: "the arrow can be missing")
                bool moved = physics || inst.entityType == GateCardType;
                var li = new LodInstance { instance = inst, filter = mf, renderer = mr, model = d, dynamic = moved,
                    alwaysVisible = !physics && byId.TryGetValue(inst.entityId, out var se) && se.Bool("AlwaysVisible") };
                int n = d.lodDistances.Length;
                li.meshes = new Mesh[Math.Max(1, n)]; li.materials = new Material[Math.Max(1, n)][];
                li.meshes[0] = mf.sharedMesh; li.materials[0] = mr.sharedMaterials;
                if (n > 1 && sceneModels.TryGetValue(inst.model, out var md0))
                {
                    // Preserve the authored scene slots, including nulls; merged LOD0 names lose that information.
                    var overrides = byId.TryGetValue(inst.entityId, out var source) ? source.MaterialList() : null;
                    for (int l = 1; l < n; l++)
                    {
                        var md = l - 1 < d.lods.Count ? d.lods[l - 1] : null;
                        if (md == null) { li.meshes[l] = li.meshes[l - 1]; li.materials[l] = li.materials[l - 1]; continue; }
                        if (!meshCache.TryGetValue((inst.model, l), out var mesh))
                        {
                            mesh = new Mesh { name = md.name, indexFormat = md.positions.Length > 65535 ? IndexFormat.UInt32 : IndexFormat.UInt16, hideFlags = HideFlags.DontSave };
                            mesh.vertices = md.positions; mesh.normals = md.normals; mesh.uv = md.uv0; mesh.uv2 = md.uv1;
                            if (md.colors != null) mesh.colors32 = md.colors;
                            if (md.tangents != null) mesh.tangents = md.tangents.Select(t => new Vector4(t.x, t.y, t.z, 1)).ToArray();
                            mesh.subMeshCount = md.triangles.Count;
                            for (int s = 0; s < md.triangles.Count; s++) mesh.SetTriangles(md.triangles[s], s, false);
                            mesh.RecalculateBounds(); mesh.UploadMeshData(true); Track(mesh);
                            meshCache[(inst.model, l)] = mesh;
                        }
                        li.meshes[l] = mesh;
                        li.materials[l] = LodBindingMaterials(md0.bindingMaterials, md.bindingMaterials, overrides).Select(MaterialFor).ToArray();
                    }
                }
                list.Add(li);
            }
            return list;
        }
    }
}
