// The level's ambient animated models (crew chiefs, pit crews, crowds, flaggers, scenario actors, the Alpine hovercraft, billboard
// crowd sections), read at run time from the player's install like the static scenery (ReflexScenery.cs): the model + surface
// resources, the material list of the scene entity (or the model's own binding materials) and their textures, materials made by
// the same factory (ReflexSceneryMaterial, "Reflex/Original Scenery"). OriginalAmbient poses them from the running game.
//
// Skinned meshes: one Unity mesh per model with one submesh per LOD0 binding. Skinned surfaces keep their weights (indices = the
// model's own nodes, the node order of the game's per-instance node set, entity +0x210); rigid surfaces (vertices in their node's
// space) are moved to model space by the node's bind world matrix and weighted 1 to that node, so one SkinnedMeshRenderer poses
// the whole model from the game's node world matrices. bindposes[i] = inverse(node i bind world).
using System;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using UnityEngine.Rendering;

namespace Reflex.Port.Content
{
    public sealed partial class ReflexLevelContent
    {
        internal sealed class AmbientModel
        {
            public string name;
            public int nodeCount;
            public Matrix4x4[] bindWorld;                  // node bind world matrices (Unity column convention)
            public string[] nodeNames;
            public Vector3[] positions, normals; public Vector2[] uv0, uv1; public Color32[] colors; public BoneWeight[] weights;
            public Vector4[] tangents;                     // the surfaces' FLOAT3 tangents (w 1; zero where a surface has none): Reflex/Vehicle's input
            public readonly List<int[]> triangles = new List<int[]>();
            public readonly List<string> bindingMaterials = new List<string>();
            public readonly List<int> bindingNodes = new List<int>();   // each LOD0 binding's node (a hidden node's bindings are not drawn)
            public readonly List<int> bindingFirstVertex = new List<int>(), bindingVertexCount = new List<int>();   // each LOD0 binding's vertex range of positions / normals / uv0 (the crowd flashes pick a seat quad of it)
            public Vector3[] staticPositions, staticNormals;   // the bind pose in model space (billboard crowds: drawn rigid)
            public Mesh mesh, staticMesh;                     // built on the main thread
            public string error;
        }
        readonly System.Collections.Concurrent.ConcurrentDictionary<string, AmbientModel> ambientModels = new System.Collections.Concurrent.ConcurrentDictionary<string, AmbientModel>(StringComparer.OrdinalIgnoreCase);

        /// The level's scene entities (the base section and the activity's), for readers outside the content loader (w-sfx: the sound entities).
        internal IReadOnlyList<SceneEntity> SceneEntities => entities;
        /// The scene entity of an entity id (Base and activity sections), or null.
        internal SceneEntity EntityById(int id)
        {
            if (entityById == null) { var d = new Dictionary<int, SceneEntity>(); foreach (var e in entities) d[e.id] = e; entityById = d; }
            return entityById.TryGetValue(id, out var r) ? r : null;
        }
        Dictionary<int, SceneEntity> entityById;

        /// Parses a model, its surfaces and the materials / textures named (thread-safe; no Unity objects). Cached.
        internal AmbientModel LoadAmbientModel(string name, IEnumerable<string> materialNames)
        {
            if (ambientModels.TryGetValue(name, out var cached)) return cached;
            var am = new AmbientModel { name = name };
            try
            {
                var model = Rfx.ParseModel(ReadAsset(name, "model"));
                var surfaces = Rfx.ParseSurfaces(ReadAsset(name, "surface"));
                Rfx.Check(surfaces.Length == model.surfaceCount, "model/surface count");
                int n = model.nodes.Length; am.nodeCount = n;
                am.bindWorld = model.nodes.Select(x => x.world).ToArray(); am.nodeNames = model.nodes.Select(x => x.name).ToArray();
                var pos = new List<Vector3>(); var nrm = new List<Vector3>(); var uv0 = new List<Vector2>(); var uv1 = new List<Vector2>(); var col = new List<Color32>(); var bw = new List<BoneWeight>();
                var tan = new List<Vector4>(); bool anyTan = false;
                bool anyCol = false;
                foreach (var b in model.Lod(0))
                {
                    var s = surfaces[b.surface]; int offset = pos.Count;
                    var w = model.nodes[b.node].world;
                    for (int i = 0; i < s.positions.Length; i++)
                    {
                        Vector3 p = s.positions[i], q = s.normals[i];
                        if (!s.skinned) { p = w.MultiplyPoint3x4(p); q = w.MultiplyVector(q); }
                        pos.Add(p); nrm.Add(q);
                        if (s.tangents != null) { var tv = s.skinned ? s.tangents[i] : w.MultiplyVector(s.tangents[i]); tan.Add(new Vector4(tv.x, tv.y, tv.z, 1)); anyTan = true; } else tan.Add(Vector4.zero);
                        uv0.Add(new Vector2(Clean(s.uv0[i].x), 1f - Clean(s.uv0[i].y))); uv1.Add(new Vector2(Clean(s.uv1[i].x), 1f - Clean(s.uv1[i].y)));
                        col.Add(s.colors != null ? s.colors[i] : new Color32(255, 255, 255, 255));
                        var weight = new BoneWeight();
                        if (s.skinned)
                        {
                            var k = new[] { 0, 1, 2, 3 }.OrderByDescending(j => s.weights[i * 4 + j]).ToArray();
                            for (int j = 0; j < 4; j++)
                            {
                                int bone = s.bones[i * 4 + k[j]]; float wt = s.weights[i * 4 + k[j]];
                                if (wt <= 0 || bone < 0 || bone >= n) { bone = 0; wt = 0; }
                                switch (j) { case 0: weight.boneIndex0 = bone; weight.weight0 = wt; break; case 1: weight.boneIndex1 = bone; weight.weight1 = wt; break; case 2: weight.boneIndex2 = bone; weight.weight2 = wt; break; default: weight.boneIndex3 = bone; weight.weight3 = wt; break; }
                            }
                        }
                        else { weight.boneIndex0 = b.node; weight.weight0 = 1; }
                        bw.Add(weight);
                    }
                    anyCol |= s.colors != null;
                    am.triangles.Add(s.Triangles().Select(t => t + offset).ToArray());
                    am.bindingMaterials.Add(b.material); am.bindingNodes.Add(b.node);
                    am.bindingFirstVertex.Add(offset); am.bindingVertexCount.Add(s.positions.Length);
                }
                Rfx.Check(am.triangles.Count > 0, "LOD0 has no bindings");
                am.positions = pos.ToArray(); am.normals = nrm.ToArray(); am.uv0 = uv0.ToArray(); am.uv1 = uv1.ToArray(); am.weights = bw.ToArray();
                am.colors = anyCol ? col.ToArray() : null;
                am.tangents = anyTan ? tan.ToArray() : null;
                // the bind pose (what a rigid draw of a skinned model shows): v * bind skinning = v itself
                am.staticPositions = am.positions; am.staticNormals = am.normals;
                // Model defaults may be completely replaced by scene slots (Stillwater flags and finish gates).
                // Warm available defaults only; an absent default is diagnosed when a draw actually selects it.
                // The caller supplies the union of scene names, not a slot-preserving override list.
                foreach (var m in am.bindingMaterials.Where(x => HasAsset(x, "material"))
                    .Concat(materialNames ?? Enumerable.Empty<string>()).Where(x => !string.IsNullOrEmpty(x)).Distinct()) EnsureMaterialData(m);
            }
            catch (Exception e) { am.error = e.Message; }
            return ambientModels.GetOrAdd(name, am);
        }
        static float Clean(float v) => float.IsFinite(v) ? v : 0f;

        void EnsureMaterialData(string name)
        {
            if (sceneMaterials.ContainsKey(name)) return;
            try
            {
                var lm = LoadMaterial(name);
                foreach (var t in lm.textures)
                    if (!string.IsNullOrEmpty(t) && !sceneTextures.ContainsKey(t))
                        try { var lt = ReflexTex.Parse(t, ReadAsset(t, "texture")); if (lt != null) sceneTextures[t] = lt; }
                        catch (Exception ex) { Warn($"texture {t}: {ex.Message}"); }
                sceneMaterials[name] = lm;
            }
            catch (Exception ex) { Warn($"material {name}: {ex.Message}"); }
        }

        /// The Unity mesh of an ambient model (skinned: bone weights + bindposes; rigid: the bind pose). Main thread.
        internal Mesh AmbientMesh(AmbientModel am, bool skinned)
        {
            if (am == null || am.error != null) return null;
            var have = skinned ? am.mesh : am.staticMesh;
            if (have) return have;
            var mesh = new Mesh { name = am.name + (skinned ? " (skinned)" : ""), indexFormat = am.positions.Length > 65535 ? IndexFormat.UInt32 : IndexFormat.UInt16, hideFlags = HideFlags.DontSave };
            mesh.vertices = am.positions; mesh.normals = am.normals; mesh.uv = am.uv0; mesh.uv2 = am.uv1;
            if (am.colors != null) mesh.colors32 = am.colors;
            if (am.tangents != null) mesh.tangents = am.tangents;
            mesh.subMeshCount = am.triangles.Count;
            for (int s = 0; s < am.triangles.Count; s++) mesh.SetTriangles(am.triangles[s], s, false);
            if (skinned) { mesh.boneWeights = am.weights; mesh.bindposes = am.bindWorld.Select(w => w.inverse).ToArray(); }
            mesh.RecalculateBounds();
            if (!skinned) { var b = mesh.bounds; b.Expand(8f); mesh.bounds = b; }   // billboard crowds: seat quads are expanded in the vertex program
            Track(mesh);
            if (skinned) am.mesh = mesh; else am.staticMesh = mesh;
            return mesh;
        }

        /// A texture outside the level (the crew chief's shirt: the local bike's graphic kit package, e.g. MX_GraphicKits /
        /// MX_MarsF_Lite_THQ_Red / CrewChief_ShirtColor_Red): the level first, then every graphic-kit database holding that package. Main thread.
        internal Texture2D ExternalTexture(string name, string package)
        {
            if (string.IsNullOrEmpty(name)) return null;
            string key = "external|" + package + "|" + name;
            if (textureObjects.TryGetValue(key, out var have)) return have;
            Texture2D t = null;
            try
            {
                Rfx.Asset a = FindAsset(name, "texture");
                if (a == null && !string.IsNullOrEmpty(package))
                    foreach (var f in RxFiles.GetFiles(install.dir, "*.dx9.database"))
                    {
                        string db = System.IO.Path.GetFileName(f); db = db.Substring(0, db.Length - ".dx9.database".Length);
                        if (db.IndexOf("graphickit", StringComparison.OrdinalIgnoreCase) < 0) continue;
                        a = install.Find(db, package, name, "texture");
                        if (a != null) break;
                    }
                if (a != null)
                {
                    var lt = ReflexTex.Parse(name, install.Read(a));
                    if (lt != null) { t = ReflexTex.ToTexture(lt, TextureWrapMode.Repeat, TextureWrapMode.Repeat, FilterMode.Trilinear, 1); Track(t); }
                }
                else Warn($"texture {name} ({package}) not found");
            }
            catch (Exception e) { Warn($"texture {name}: {e.Message}"); }
            textureObjects[key] = t;
            return t;
        }

        /// Billboard crowd materials (Reflex/Original Crowd Billboard): the material's two textures (the 16 x 8 crowd sheet, the empty
        /// chair) in its order, for each binding (the scene MaterialList when it matches, as for the other models). Main thread.
        internal Material[] CrowdMaterials(AmbientModel am, IList<string> sceneList, Shader crowd)
        {
            var names = BindingMaterials(am.bindingMaterials, sceneList);
            var r = new Material[names.Count];
            for (int i = 0; i < r.Length; i++)
            {
                string key = "crowd|" + names[i];
                if (materialObjects.TryGetValue(key, out var m)) { r[i] = m; continue; }
                EnsureMaterialData(names[i]);
                if (!crowd || !sceneMaterials.TryGetValue(names[i], out var lm)) continue;
                m = new Material(crowd) { name = names[i] + " (crowd)", hideFlags = HideFlags.DontSave };
                if (lm.textures.Count > 0) { var t = TextureFor(lm.textures[0], lm.samplerStates[0]); if (t) m.SetTexture("_BaseMap", t); }
                if (lm.textures.Count > 1) { var t = TextureFor(lm.textures[1], lm.samplerStates[1]); if (t) m.SetTexture("_EmptyChair", t); }
                Track(m); materialObjects[key] = m; r[i] = m;
            }
            return r;
        }

        /// The "CameraFlash" texture of the stands crowd's camera flashes (BillboardCrowdEntity::vf5 006c6180 looks it up by that name;
        /// CameraFlashPS samples it as s0 with the device's states of the draw: wrap, linear, mip linear). Null when the level has none. Main thread.
        internal Texture2D CameraFlashTexture()
        {
            try
            {
                if (!sceneTextures.ContainsKey("CameraFlash"))
                {
                    if (!HasAsset("CameraFlash", "texture")) return null;
                    var lt = ReflexTex.Parse("CameraFlash", ReadAsset("CameraFlash", "texture"));
                    if (lt == null) return null;
                    sceneTextures["CameraFlash"] = lt;
                }
                return TextureFor("CameraFlash", new Dictionary<int, float> { { 1, 1f }, { 2, 1f }, { 5, 2f }, { 6, 2f }, { 7, 2f } });   // ADDRESSU / V wrap, MAG / MIN / MIP linear (006c7c71 .. 006c7ccb)
            }
            catch (Exception e) { Warn("CameraFlash texture: " + e.Message); return null; }
        }

        /// colorTable (c72[24]) of the level's "crowd" shader: the constant table defaults of StaticVertexShader (the shirt tints).
        internal Vector4[] CrowdColorTable()
        {
            try
            {
                var b = ReadAsset("crowd", "shader");
                int count = Rfx.I32(b, 4), p = 8;
                for (int e = 0; e < count; e++)
                {
                    int kind = Rfx.I32(b, p), len = Rfx.I32(b, p + 4); p += 8;
                    string name = System.Text.Encoding.ASCII.GetString(b, p, len).TrimEnd('\0'); p += len;
                    int size = Rfx.I32(b, p); p += 4; int code = p; p += size;
                    if (kind != 0 || name != "StaticVertexShader") continue;
                    uint tok = Rfx.U32(b, code + 4);
                    if ((tok & 0xffff) != 0xfffe || Rfx.U32(b, code + 8) != 0x42415443) return null;   // CTAB comment first
                    int t = code + 12, consts = Rfx.I32(b, t + 12), infoAt = Rfx.I32(b, t + 16);
                    for (int i = 0; i < consts; i++)
                    {
                        int ci = t + infoAt + i * 20;
                        int nameAt = Rfx.I32(b, ci), regCount = BitConverter.ToUInt16(b, ci + 8), def = Rfx.I32(b, ci + 16);
                        int ne = Array.IndexOf(b, (byte)0, t + nameAt);
                        if (System.Text.Encoding.ASCII.GetString(b, t + nameAt, ne - t - nameAt) != "colorTable" || def == 0) continue;
                        var r = new Vector4[regCount];
                        for (int k = 0; k < regCount; k++) r[k] = new Vector4(Rfx.F32(b, t + def + k * 16), Rfx.F32(b, t + def + k * 16 + 4), Rfx.F32(b, t + def + k * 16 + 8), Rfx.F32(b, t + def + k * 16 + 12));
                        return r;
                    }
                }
            }
            catch (Exception e) { Warn("crowd shader colorTable: " + e.Message); }
            return null;
        }

        /// Materials for an entity's bindings: its non-null scene MaterialList slots over the model defaults. Main thread.
        internal Material[] AmbientMaterials(AmbientModel am, IList<string> sceneList, SceneryShaders shaders)
        {
            var names = BindingMaterials(am.bindingMaterials, sceneList);
            var r = new Material[names.Count];
            for (int i = 0; i < r.Length; i++)
            {
                string n = names[i];
                if (string.IsNullOrEmpty(n)) continue;
                if (materialObjects.TryGetValue(n, out var m)) { r[i] = m; continue; }
                EnsureMaterialData(n);
                if (!sceneMaterials.TryGetValue(n, out var lm)) continue;
                // the characters' and crowds' own programs (Template_Crowd_*, Template_Body_* ...: Reflex/Vehicle, ReflexObjectShading), else the scenery factory
                m = ReflexObjectShading.BuildLevel(this, n, lm, TextureFor) ?? ReflexSceneryMaterial.Create(this, lm, shaders, TextureFor);
                if (m) { Track(m); materialObjects[n] = m; }
                r[i] = m;
            }
            return r;
        }
    }
}
