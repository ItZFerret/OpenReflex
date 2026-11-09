// Static scenery of a level: the models (LOD0, node transforms applied), the entity placements (original scene matrices), the
// materials (pixel shader, textures, parameters and render states read from each material resource) and the textures.
// Port of level_export.export_scenery / alpine_scenery_export / start_support_export, but generic: every static entity of the
// level's Base scene and of the activity scene (Alpine's hovercraft under the FreeRide start) is instanced, nothing is hand picked.
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using UnityEngine.Rendering;

namespace Reflex.Port.Content
{
    public sealed partial class ReflexLevelContent
    {
        // Entity types carrying a static ModelAsset/SurfaceAsset/MaterialList triple. Skinned/animated entities (crowd, staff, flag
        // holders ...), heightfield overlays (HFBump) and the sky dome are not instanced here.
        static readonly HashSet<string> StaticTypes = new HashSet<string>
        { "ModelRSData", "CollisionRSData", "PhysicsRSData", "DropGateRSData", "PopUpBaleRSData", "PopDownBaleRSData",
          // drawn by the original renderer as static models too (recorded frames): scrolling-texture models (Kingston light beams), the
          // Massive ad slots (drawn with their MaterialList like any model: the PC exe never reads "Ad Texture", screens-ads.md), the leader board
          "ScrollingTextureRSData", "MassiveAdEntityRSData", "LeaderBoardRSData",
          "CollectibleTargetPracticeRSData" };   // Target Practice's targets (shown / hidden as the game draws them: OriginalLiveUi)

        internal sealed class ModelData
        {
            public string name;
            public Vector3[] positions, normals, tangents; public Vector2[] uv0, uv1; public Color32[] colors;
            public readonly List<int[]> triangles = new List<int[]>();    // one per LOD0 binding (submesh)
            public readonly List<string> bindingMaterials = new List<string>();
            public int lodCount, repairedNormals;
        }

        internal sealed class LevelMaterial
        {
            public string name, pixelShader, vertexShader;
            public readonly List<string> textures = new List<string>();                       // material texture slots, in order
            public readonly List<Dictionary<int, float>> samplerStates = new List<Dictionary<int, float>>();
            public readonly Dictionary<string, float[]> parameters = new Dictionary<string, float[]>(StringComparer.Ordinal);
            public readonly Dictionary<int, uint> states = new Dictionary<int, uint>();      // D3DRENDERSTATETYPE -> value
            public ShaderInfo shader;
            public string[] userSamplers = Array.Empty<string>();                             // pixel shader sampler names for the texture slots
            public int cull = 3; public bool alphaTest, alphaBlend; public int alphaRef, srcBlend = 5, dstBlend = 6, alphaFunc = 5;
            public bool zWrite = true;
        }

        /// What a pixel shader declares (CTAB): its constants and samplers.
        internal sealed class ShaderInfo
        {
            public string name;
            public ReflexSkyProgram skyProgram;
            public readonly HashSet<string> constants = new HashSet<string>(StringComparer.Ordinal);
            public readonly Dictionary<int, string> samplers = new Dictionary<int, string>();
            /// The sampler register whose texel alpha the pixel program writes to oC0.w (`texld rN, coord, sK` ... `mov oC0.w, rN.w`), -1 when
            /// it writes anything else (1, a constant, a product with a vertex term ...). See <see cref="ScanAlphaSampler"/>.
            public int alphaSampler = -1;
            public bool Has(string constant) => constants.Contains(constant);
        }

        internal readonly ConcurrentDictionary<string, ModelData> sceneModels = new ConcurrentDictionary<string, ModelData>();
        internal readonly ConcurrentDictionary<string, LevelMaterial> sceneMaterials = new ConcurrentDictionary<string, LevelMaterial>();
        internal readonly ConcurrentDictionary<string, LevelTexture> sceneTextures = new ConcurrentDictionary<string, LevelTexture>();
        readonly ConcurrentDictionary<string, ShaderInfo> shaderInfos = new ConcurrentDictionary<string, ShaderInfo>();
        readonly ConcurrentDictionary<string, string> modelErrors = new ConcurrentDictionary<string, string>();
        readonly List<SceneEntity> placed = new List<SceneEntity>();
        SceneEntity skyEntity; string skyModelName => skyEntity?.Asset("ModelAsset");
        List<SceneEntity> bumpEntities = new List<SceneEntity>();
        /// The HFBumpModelRSData instances (RT_Bump layer 1), in draw order. Not part of instances.
        public readonly List<SceneryInstance> bumpInstances = new List<SceneryInstance>();
        /// The level's sky dome (SkyRSData), or null. Not part of instances.
        public SceneryInstance sky;

        // Texture names the scenery uses that are not loaded as scenery textures (kept out of the shared dictionary)
        static string Group(string type, string model)
        {
            string low = model.ToLowerInvariant();
            if (low.Contains("_eow")) return "edge-of-world";
            if (type == "PhysicsRSData" || type == "PopUpBaleRSData" || type == "PopDownBaleRSData" || low.Contains("bale")) return "bales";
            if (type == "DropGateRSData" || low.Contains("gate")) return "start-finish-gates";
            if (low.Contains("banner") || low.Contains("sign")) return "banners-signs";
            if (low.Contains("tire")) return "tires";
            if (low.StartsWith("av_", StringComparison.Ordinal)) return "vehicles";
            if (low.Contains("tent") || low.Contains("grandstand") || low.Contains("tower") || low.Contains("barn") || low.Contains("podium")) return "structures";
            if (low.Contains("fence") || low.Contains("chain") || low.Contains("barricade") || low.Contains("rail")) return "fences-barriers";
            if (low.Contains("rock")) return "rocks";
            return "other";
        }

        // ---------------------------------------------------------------------------------------- stages

        void StageScenery(int stage, List<Action> jobs)
        {
            if (stage == 1)
            {
                foreach (var e in entities)
                    if (StaticTypes.Contains(e.Type) && !string.IsNullOrEmpty(e.Asset("ModelAsset")) && e.Matrix != null) placed.Add(e);
                skyEntity = entities.FirstOrDefault(e => e.Type == "SkyRSData" && !string.IsNullOrEmpty(e.Asset("ModelAsset")) && e.Matrix != null);
                // in-level Bink screens (VideoEntity: "Bink Video Asset" played into "Texture To Override", _re/notes/bink.md): the target is
                // the render target the video is drawn into (cleared to 0 first); the videos are not played here, so these show black as in the
                // recorded frames before a video frame lands, not the package's placeholder image
                foreach (var e in entities) { string v = e.Asset("Texture To Override"); if (!string.IsNullOrEmpty(v)) videoTargets.Add(v); }
                // the terrain's RT_Bump layer-1 models (rt-bump.md): drawn in ascending SortBias, scene order for equal values
                bumpEntities = entities.Where(e => e.Type == "HFBumpModelRSData" && !string.IsNullOrEmpty(e.Asset("ModelAsset")) && e.Matrix != null)
                                       .Select((e, i) => (e, i)).OrderBy(t => t.e.Float("SortBias")).ThenBy(t => t.i).Select(t => t.e).ToList();
                foreach (var model in placed.Select(e => e.Asset("ModelAsset")).Concat(bumpEntities.Select(e => e.Asset("ModelAsset"))).Concat(skyEntity != null ? new[] { skyEntity.Asset("ModelAsset") } : new string[0]).Distinct())
                {
                    string m = model;
                    jobs.Add(() =>
                    {
                        try { sceneModels[m] = LoadModel(m); }
                        catch (Exception ex) { modelErrors[m] = ex.Message; }
                    });
                }
                RestoredScenery(stage, jobs);   // RESTORATION (w-fresnolook, RestoredLook.cs)
            }
            else if (stage == 2)
            {
                // instances (skipping models that could not be read) and the materials they use
                var names = new HashSet<string>();
                foreach (var e in placed)
                {
                    string model = e.Asset("ModelAsset");
                    if (!sceneModels.TryGetValue(model, out var md)) continue;
                    var over = Resolved(e.MaterialList());
                    var mats = BindingMaterials(md.bindingMaterials, over);
                    instances.Add(new SceneryInstance
                    {
                        entityId = e.id, entityType = e.Type, name = e.String("Name") ?? model, model = model, group = Group(e.Type, model),
                        section = e.section, matrix = e.Matrix, materials = mats.ToArray(),
                        adTexture = e.Type == "MassiveAdEntityRSData" ? e.Asset("Ad Texture") : null,
                        scroll = e.Type == "ScrollingTextureRSData" || e.Type == "CollectibleTargetPracticeRSData" ? new Vector4(e.Float("U Seconds For Wrap"), e.Float("V Seconds For Wrap"), e.Bool("Reverse U") ? 1 : 0, e.Bool("Reverse V") ? 1 : 0) : Vector4.zero,
                    });
                    foreach (var n in mats) names.Add(n);
                }
                foreach (var e in bumpEntities)
                {
                    string model = e.Asset("ModelAsset");
                    if (!sceneModels.TryGetValue(model, out var md)) continue;
                    var over = Resolved(e.MaterialList());
                    var mats = BindingMaterials(md.bindingMaterials, over);
                    bumpInstances.Add(new SceneryInstance { entityId = e.id, entityType = e.Type, name = e.String("Name") ?? model, model = model, group = "rt-bump",
                                                            section = e.section, matrix = e.Matrix, materials = mats.ToArray() });
                    foreach (var n in mats) names.Add(n);
                }
                if (skyEntity != null && sceneModels.TryGetValue(skyEntity.Asset("ModelAsset"), out var skyModel))
                {
                    var over = Resolved(skyEntity.MaterialList());
                    sky = new SceneryInstance { entityId = skyEntity.id, entityType = skyEntity.Type, name = skyEntity.String("Name") ?? "Sky", model = skyModel.name, group = "sky",
                                                section = skyEntity.section, matrix = skyEntity.Matrix, materials = BindingMaterials(skyModel.bindingMaterials, over).ToArray() };
                    foreach (var n in sky.materials) names.Add(n);
                }
                RestoredScenery(stage, jobs);   // RESTORATION (w-fresnolook, RestoredLook.cs): lost models drawn from their surviving surfaces
                foreach (var kv in modelErrors) Warn($"model {kv.Key} skipped: {kv.Value}");
                foreach (var name in names)
                {
                    string n = name;
                    jobs.Add(() =>
                    {
                        try { sceneMaterials[n] = LoadMaterial(n); }
                        catch (Exception ex) { Warn($"material {n}: {ex.Message}"); }
                    });
                }
            }
            else if (stage == 3)
            {
                var texNames = sceneMaterials.Values.SelectMany(m => m.textures).Concat(instances.Select(i => i.adTexture)).Where(t => !string.IsNullOrEmpty(t)).Distinct().ToList();
                foreach (var name in texNames)
                {
                    string n = name;
                    jobs.Add(() =>
                    {
                        byte[] raw = null;
                        try
                        {
                            raw = ReadAsset(n, "texture");
                            var t = ReflexTex.Parse(n, raw);
                            if (t != null) sceneTextures[n] = t; else Warn($"texture {n}: unsupported format");
                        }
                        catch (Exception ex)
                        {
                            // a header without pixels: a render target the game fills at run time (a video screen whose video the level does
                            // not carry, as the community's tracks reuse NAT_RE_Bink_MAIN): black, as a video target, not a missing texture
                            if (raw != null && raw.Length <= 64) { lock (videoTargets) videoTargets.Add(n); }
                            else Warn($"texture {n}: {ex.Message}");
                        }
                    });
                }
            }
        }

        /// A MaterialList as ModelInstance_Init (007f12c0) applies it: a slot's handle that resolves to no material of the level is null and
        /// leaves the model's own binding (only the community's tracks name materials they do not carry: Mods/Tracks).
        IList<string> Resolved(IList<string> overrides)
        {
            if (overrides == null) return null;
            var r = new List<string>(overrides);
            for (int i = 0; i < r.Count; i++) if (!string.IsNullOrEmpty(r[i]) && !HasAsset(r[i], "material")) r[i] = null;
            return r;
        }

        ModelData LoadModel(string name)
        {
            var model = Rfx.ParseModel(ReadAsset(name, "model"));
            string surfaceName = name;
            var surfaces = Rfx.ParseSurfaces(ReadAsset(surfaceName, "surface"));
            Rfx.Check(surfaces.Length == model.surfaceCount, "model/surface count");
            Rfx.Check(model.lods.Length > 0, "no LODs");
            var md = new ModelData { name = name, lodCount = model.lods.Length };
            var pos = new List<Vector3>(); var nrm = new List<Vector3>(); var tan = new List<Vector3>(); var uv0 = new List<Vector2>(); var uv1 = new List<Vector2>(); var col = new List<Color32>();
            bool anyTan = false, anyCol = false;
            foreach (var b in model.Lod(0))
            {
                var s = surfaces[b.surface]; var node = model.nodes[b.node];
                Rfx.Check(!s.skinned, "skinned surface");
                int offset = pos.Count; var w = node.world;
                // the sky's vertex program replaces the world matrix translation (node + entity) by the camera position
                if (name == skyModelName) { w.m03 = 0; w.m13 = 0; w.m23 = 0; }
                for (int i = 0; i < s.positions.Length; i++)
                {
                    pos.Add(w.MultiplyPoint3x4(s.positions[i])); nrm.Add(w.MultiplyVector(s.normals[i]));
                    tan.Add(s.tangents != null ? w.MultiplyVector(s.tangents[i]) : Vector3.right);
                    uv0.Add(new Vector2(s.uv0[i].x, 1f - s.uv0[i].y)); uv1.Add(new Vector2(s.uv1[i].x, 1f - s.uv1[i].y));
                    col.Add(s.colors != null ? s.colors[i] : new Color32(255, 255, 255, 255));
                }
                anyTan |= s.tangents != null; anyCol |= s.colors != null;
                md.triangles.Add(s.Triangles().Select(t => t + offset).ToArray());
                md.bindingMaterials.Add(b.material);
            }
            Rfx.Check(md.triangles.Count > 0, "LOD0 has no bindings");
            md.positions = pos.ToArray(); md.normals = nrm.ToArray(); md.uv0 = uv0.ToArray(); md.uv1 = uv1.ToArray();
            md.tangents = anyTan ? tan.ToArray() : null; md.colors = anyCol ? col.ToArray() : null;
            return md;
        }

        LevelMaterial LoadMaterial(string name)
        {
            var root = Rfx.Bxml(ReadAsset(name, "material"));
            var m = new LevelMaterial { name = name };
            foreach (var n in (root.Child("Pass") ?? root).All())
            {
                if (n.tag == "PixelShader" && m.pixelShader == null) m.pixelShader = n.S("asset");
                else if (n.tag == "VertexShader" && m.vertexShader == null) m.vertexShader = n.S("asset");
                else if (n.tag == "Texture")
                {
                    m.textures.Add(n.S("textureName") ?? "");
                    var st = new Dictionary<int, float>();
                    var ss = n.Child("XenonSamplerStates");
                    if (ss?.value is ArraySegment<byte> seg)
                        for (int i = 0; i + 7 < seg.Count; i += 8)
                        {
                            int key = BitConverter.ToInt32(seg.Array, seg.Offset + i);
                            st[key] = key == 8 ? BitConverter.ToSingle(seg.Array, seg.Offset + i + 4) : BitConverter.ToUInt32(seg.Array, seg.Offset + i + 4);
                        }
                    m.samplerStates.Add(st);
                }
                else if (n.tag == "Parameter" && n.S("name") != null)
                    m.parameters[n.S("name")] = n.value as float[] ?? (n.value is float f ? new[] { f, f, f, f } : null);
                else if (n.tag == "XenonStates" && n.value is ArraySegment<byte> seg)
                    for (int i = 0; i + 7 < seg.Count; i += 8)
                        m.states[BitConverter.ToInt32(seg.Array, seg.Offset + i)] = BitConverter.ToUInt32(seg.Array, seg.Offset + i + 4);
            }
            uint S(int key, uint def) => m.states.TryGetValue(key, out var v) ? v : def;
            m.cull = (int)S(22, 3); m.alphaTest = S(15, 0) != 0; m.alphaRef = (int)S(24, 0); m.alphaFunc = (int)S(25, 5);
            m.alphaBlend = S(27, 0) != 0; m.srcBlend = (int)S(19, 5); m.dstBlend = (int)S(20, 6); m.zWrite = S(14, 1) != 0;
            if (m.pixelShader != null) m.shader = GetShader(m.pixelShader);
            if (m.shader != null)
                m.userSamplers = m.shader.samplers.OrderBy(kv => kv.Key).Select(kv => kv.Value)
                    .Where(s => !(s.StartsWith("sampler", StringComparison.Ordinal) && s.Length > 7 && char.IsUpper(s[7]))).ToArray();
            return m;
        }

        ShaderInfo GetShader(string asset)
        {
            if (shaderInfos.TryGetValue(asset, out var s)) return s;
            try { s = ParseShaderInfo(asset, ReadAsset(asset, "shader")); }
            catch (Exception ex) { Warn($"shader {asset}: {ex.Message}"); s = new ShaderInfo { name = asset }; }
            return shaderInfos.GetOrAdd(asset, s);
        }

        /// The pixel program's constant table: names of constants and (register -> name) of samplers (the program of kind 1 in the
        /// shader resource; vertex entries are skipped).
        internal static ShaderInfo ParseShaderInfo(string name, byte[] b)
        {
            var info = new ShaderInfo { name = name, skyProgram = name == "SkyEntityComp" ? ReflexSkyProgram.Read(b) : null };
            Rfx.Check(b.Length >= 8 && Rfx.I32(b, 0) == 2, "shader resource");
            int count = Rfx.I32(b, 4), p = 8;
            Rfx.Check(count > 0 && count <= 64, "shader entries");
            for (int e = 0; e < count; e++)
            {
                int kind = Rfx.I32(b, p), len = Rfx.I32(b, p + 4); p += 8; Rfx.Check(len > 0 && len <= 256 && p + len <= b.Length, "shader entry name"); p += len;
                int size = Rfx.I32(b, p); p += 4; Rfx.Check(size > 0 && p + size <= b.Length, "shader entry size");
                int code = p; p += size;
                if (kind != 1) continue;
                for (int q = code + 4; q + 4 <= code + size;)
                {
                    uint tok = Rfx.U32(b, q);
                    if ((tok & 0xffff) != 0xfffe) break;
                    int n = (int)((tok >> 16) & 0x7fff), body = q + 4;
                    if (n >= 7 && body + 4 <= b.Length && b[body] == 'C' && b[body + 1] == 'T' && b[body + 2] == 'A' && b[body + 3] == 'B')
                    {
                        int t = body + 4, end = body + n * 4;
                        int consts = Rfx.I32(b, t + 12), infoAt = Rfx.I32(b, t + 16);
                        for (int i = 0; i < consts; i++)
                        {
                            int ci = t + infoAt + i * 20; Rfx.Check(ci + 20 <= end, "shader constant table");
                            int nameAt = Rfx.I32(b, ci), set = BitConverter.ToUInt16(b, ci + 4), reg = BitConverter.ToUInt16(b, ci + 6);
                            int ne = Array.IndexOf(b, (byte)0, t + nameAt, end - t - nameAt); if (ne < 0) ne = end;
                            string cname = System.Text.Encoding.ASCII.GetString(b, t + nameAt, ne - t - nameAt);
                            info.constants.Add(cname);
                            if (set == 3) info.samplers[reg] = cname;
                        }
                        info.alphaSampler = ScanAlphaSampler(b, body + n * 4, code + size);
                        return info;
                    }
                    q = body + n * 4;
                }
            }
            return info;
        }

        static int ShaderRegisterType(uint token) => (int)(((token >> 28) & 7) | ((token >> 8) & 24));

        /// Which sampler's alpha a pixel program outputs. Walks the instruction stream (after the constant table) of straight-line programs
        /// (any flow control: -1): a texld that writes a whole temp register remembers its sampler, any other write of a temp's w forgets
        /// it, and the last write of oC0.w decides: `mov(_sat)(_pp) oC0.w, rN.w` with rN from `texld rN, coord, sK` gives K, anything else
        /// -1. (The per-level tree programs of Bedford, `NAT_BE_LeaningTree_Needles / _Branches`, end with `texld_pp r1, v2, s0` ...
        /// `mov_sat_pp oC0.w, r1.w`.)
        internal static int ScanAlphaSampler(byte[] b, int at, int end)
        {
            var texOf = new int[32]; for (int i = 0; i < texOf.Length; i++) texOf[i] = -1;
            int result = -1;
            while (at + 4 <= end)
            {
                uint ins = Rfx.U32(b, at); int op = (int)(ins & 0xffff);
                if (op == 0xffff) break;
                if (op == 0xfffe) { at += 4 + (int)((ins >> 16) & 0x7fff) * 4; continue; }
                int len = (int)((ins >> 24) & 15);
                if (at + 4 + len * 4 > end) return -1;
                if ((op >= 25 && op <= 30) || (op >= 38 && op <= 45) || op == 96) return -1;          // call / loop / rep / if / break / label / ret
                if (op != 0 && op != 31 && op != 65 && op != 81 && op != 82 && op != 83 && len >= 1)   // not nop / dcl / texkill / def*
                {
                    uint d = Rfx.U32(b, at + 4); int type = ShaderRegisterType(d), reg = (int)(d & 2047), mask = (int)((d >> 16) & 15);
                    if (type == 0 && reg < texOf.Length)
                    {
                        if (op == 66 && len == 3 && mask == 15 && ShaderRegisterType(Rfx.U32(b, at + 12)) == 10) texOf[reg] = (int)(Rfx.U32(b, at + 12) & 2047);
                        else if ((mask & 8) != 0) texOf[reg] = -1;
                    }
                    else if (type == 8 && reg == 0 && (mask & 8) != 0)
                    {
                        result = -1;
                        if (op == 1 && len == 2)
                        {
                            uint s = Rfx.U32(b, at + 8);
                            if (ShaderRegisterType(s) == 0 && (s & 0x0f002000u) == 0 && ((s >> 22) & 3) == 3 && (s & 2047) < texOf.Length) result = texOf[s & 2047];
                        }
                    }
                }
                at += 4 + len * 4;
            }
            return result;
        }

        // ---------------------------------------------------------------------------------------- Unity objects

        /// Settings for the scenery material factory (set by OriginalLevel).
        public sealed class SceneryShaders
        {
            public Shader scenery;           // Reflex/Original Scenery
            public Shader sky;               // Reflex/Original Sky
        }

        readonly Dictionary<string, Texture2D> textureObjects = new Dictionary<string, Texture2D>();
        readonly Dictionary<string, Material> materialObjects = new Dictionary<string, Material>();
        public IReadOnlyDictionary<string, Material> SceneryMaterials => materialObjects;
        /// Diagnostics: every scenery material with its program, textures (sampler) and parameters.
        public string DescribeMaterials()
        {
            var sb = new System.Text.StringBuilder();
            foreach (var kv in sceneMaterials.OrderBy(k => k.Key))
            {
                var lm = kv.Value;
                sb.Append(kv.Key).Append(" ps=").Append(lm.pixelShader).Append(" vs=").Append(lm.vertexShader);
                for (int i = 0; i < lm.textures.Count; i++) sb.Append(" | t").Append(i).Append('=').Append(lm.textures[i]).Append(" (").Append(i < lm.userSamplers.Length ? lm.userSamplers[i] : "-").Append(')');
                foreach (var p in lm.parameters) sb.Append(" | ").Append(p.Key).Append('=').Append(p.Value == null ? "null" : string.Join(",", p.Value));
                sb.Append('\n');
            }
            return sb.ToString();
        }

        /// Textures that in-level videos draw into (VideoEntity "Texture To Override"): black (no video playback in the world yet).
        public readonly HashSet<string> videoTargets = new HashSet<string>(StringComparer.Ordinal);

        /// `trilinear` / `aniso` (the RT_Bump bump textures, BuildBumpModels): the PC original's terrain-pass sampler state (MIP linear) and
        /// anisotropic filtering instead of the material's console states (MIP point: hard mip steps in the RT_Bump slices, which the terrain
        /// shows as lines that move with the camera).
        Texture2D TextureFor(string name, Dictionary<int, float> st) => TextureFor(name, st, false, 1);

        Texture2D TextureFor(string name, Dictionary<int, float> st, bool trilinear, int aniso)
        {
            if (name != null && videoTargets.Contains(name)) return Texture2D.blackTexture;
            if (!sceneTextures.TryGetValue(name, out var lt)) return null;
            TextureWrapMode Wrap(int key) { return st != null && st.TryGetValue(key, out var v) ? (v == 3f || v == 4f ? TextureWrapMode.Clamp : v == 2f ? TextureWrapMode.Mirror : TextureWrapMode.Repeat) : TextureWrapMode.Repeat; }
            var wu = Wrap(1); var wv = Wrap(2);
            bool tri = trilinear || (st != null && st.TryGetValue(7, out var mip) && mip == 2f);
            var filter = tri ? FilterMode.Trilinear : FilterMode.Bilinear;
            float bias = st != null && st.TryGetValue(8, out var b8) ? b8 : 0f;      // MIPMAPLODBIAS (e.g. -1 on the Concord facades)
            string key = $"{name}|{wu}|{wv}|{filter}|{bias}|{aniso}";
            if (textureObjects.TryGetValue(key, out var t)) return t;
            t = ReflexTex.ToTexture(lt, wu, wv, filter, aniso);
            if (t && bias != 0) t.mipMapBias = bias;
            Track(t); textureObjects[key] = t;
            return t;
        }

        readonly Dictionary<string, Cubemap> cubeObjects = new Dictionary<string, Cubemap>();
        Cubemap CubeFor(string name)
        {
            if (!sceneTextures.TryGetValue(name, out var lt) || !lt.cube) return null;
            if (cubeObjects.TryGetValue(name, out var c)) return c;
            c = ReflexTex.ToCubemap(lt); if (c) Track(c);
            return cubeObjects[name] = c;
        }

        static int ToUnityBlend(int d3d)
        {
            switch (d3d)
            {
                case 1: return (int)BlendMode.Zero; case 2: return (int)BlendMode.One; case 3: return (int)BlendMode.SrcColor; case 4: return (int)BlendMode.OneMinusSrcColor;
                case 5: return (int)BlendMode.SrcAlpha; case 6: return (int)BlendMode.OneMinusSrcAlpha; case 7: return (int)BlendMode.DstAlpha; case 8: return (int)BlendMode.OneMinusDstAlpha;
                case 9: return (int)BlendMode.DstColor; case 10: return (int)BlendMode.OneMinusDstColor; case 11: return (int)BlendMode.SrcAlphaSaturate;
                default: return (int)BlendMode.One;
            }
        }

        /// The RT_Bump layer-1 draws (Rendering/ReflexRTBump.cs): the level's HFBumpModelRSData meshes with materials of
        /// Hidden/Reflex/RTBump (pass by the material's pixel program: HFB_4tex, HFB_1tex_vertalpha, HFB_1tex_deformed, HFB_ch_road). Main thread.
        public List<Rendering.ReflexRTBump.Draw> BuildBumpModels(Shader rtBumpShader)
        {
            var list = new List<Rendering.ReflexRTBump.Draw>();
            if (!rtBumpShader) return list;
            if (ExperimentalTerrain.For(level)) return BuildMtxBumpModels(rtBumpShader);
            var mats = new Dictionary<string, (Material, int)>();
            foreach (var inst in bumpInstances)
            {
                if (!sceneModels.TryGetValue(inst.model, out var md)) continue;
                var mesh = new Mesh { name = md.name + " (RT_Bump)", indexFormat = md.positions.Length > 65535 ? IndexFormat.UInt32 : IndexFormat.UInt16, hideFlags = HideFlags.DontSave };
                mesh.vertices = md.positions; mesh.uv = md.uv0; mesh.uv2 = md.uv1;
                if (md.colors != null) mesh.colors32 = md.colors;
                mesh.subMeshCount = md.triangles.Count;
                for (int s = 0; s < md.triangles.Count; s++) mesh.SetTriangles(md.triangles[s], s, false);
                mesh.RecalculateBounds(); mesh.UploadMeshData(true); Track(mesh);
                var go = new GameObject(); ApplyMatrix(go.transform, inst.matrix); var matrix = go.transform.localToWorldMatrix; UnityEngine.Object.DestroyImmediate(go);
                if (Reflex.Port.RxCore.RxMapScale.Scaled) matrix = Matrix4x4.Scale(Vector3.one * Reflex.Port.RxCore.RxMapScale.Current) * matrix;   // MAP SCALE: in the scaled world
                for (int s = 0; s < md.triangles.Count && s < inst.materials.Length; s++)
                {
                    string mn = inst.materials[s];
                    if (!mats.TryGetValue(mn, out var mp))
                    {
                        mp = (null, -1);
                        if (sceneMaterials.TryGetValue(mn, out var lm))
                        {
                            var m = new Material(rtBumpShader) { name = mn + " (RT_Bump)", hideFlags = HideFlags.DontSave };
                            int pass = m.FindPass(lm.pixelShader ?? "");
                            if (pass < 0) Warn($"RT_Bump material {mn}: pixel program {lm.pixelShader} not ported");
                            for (int i = 0; i < lm.textures.Count; i++)
                            {
                                string smp = i < lm.userSamplers.Length ? lm.userSamplers[i] : "";
                                bool tiled = !smp.EndsWith("mask", StringComparison.Ordinal);   // the bump layers (tile rate) / rut decals; the mask is per terrain
                                var t = TextureFor(lm.textures[i], lm.samplerStates[i], true, tiled ? DetailAniso : 1); if (!t) continue;
                                if (smp.EndsWith("mask", StringComparison.Ordinal)) m.SetTexture("_Mask", t);
                                else if (smp.EndsWith("bump_map_r", StringComparison.Ordinal)) m.SetTexture("_Bump1", t);
                                else if (smp.EndsWith("bump_map_g", StringComparison.Ordinal)) m.SetTexture("_Bump2", t);
                                else if (smp.EndsWith("bump_map_b", StringComparison.Ordinal)) m.SetTexture("_Bump3", t);
                                else m.SetTexture("_Bump0", t);
                            }
                            if (lm.parameters.TryGetValue("usr_bump_map_tile_rate", out var tr) && tr != null) m.SetVector("_TileRate", new Vector4(tr[0], tr[1], tr[2], tr[3]));
                            if (lm.parameters.TryGetValue("usr_dirt_grass_puffy_tuner", out var tu) && tu != null) m.SetFloat("_Tuner", tu[3]);
                            if (lm.parameters.TryGetValue("usr_terrain_size", out var ts) && ts != null) m.SetFloat("_TerrainSize", ts[3] * Reflex.Port.RxCore.RxMapScale.Current);   // MAP SCALE: the scaled world's extent
                            m.SetFloat("_Cull", (float)CullMode.Off);   // flat top-down decals: the slice projection mirrors the winding
                            Track(m); mp = (m, pass);
                        }
                        mats[mn] = mp;
                    }
                    if (mp.Item1 && mp.Item2 >= 0) list.Add(new Rendering.ReflexRTBump.Draw { mesh = mesh, submesh = s, material = mp.Item1, pass = mp.Item2, matrix = matrix, name = inst.name });
                }
            }
            return list;
        }

        /// Builds the Unity meshes, materials and one game object per placed entity under a new root. Main thread.
        public GameObject BuildScenery(Transform parent, SceneryShaders shaders)
        {
            var root = new GameObject($"Original {level} scenery (read from the install at run time)");
            if (parent) root.transform.SetParent(parent, false);
            if (ExperimentalTerrain.For(level))
            {
                var data = ExperimentalTerrain.Scenery(TrackMods.Bound.folder);
                if (data != null)
                {
                    root.name = "MTX scenery: " + TrackMods.Bound.name;
                    var view = root.AddComponent<ExperimentalMtxView>();
                    var owned = new List<UnityEngine.Object>();
                    try
                    {
                        var counts = ExperimentalMtxScenery.BuildScenery(data, root.transform, owned, view.skies);
                        root.transform.localScale = Vector3.one * Reflex.Port.RxCore.RxMapScale.Current;
                        foreach (var dome in view.skies) dome.localScale /= Reflex.Port.RxCore.RxMapScale.Current;
                        Debug.Log($"[MTX] scenery loaded: {counts.surfaces} meshes, {counts.textures} textures, {view.skies.Count} sky meshes");
                    }
                    finally { foreach (var item in owned) Track(item); }
                    return root;
                }
            }
            var meshes = new Dictionary<string, Mesh>();
            foreach (var kv in sceneModels)
            {
                var md = kv.Value;
                var mesh = new Mesh { name = md.name, indexFormat = md.positions.Length > 65535 ? IndexFormat.UInt32 : IndexFormat.UInt16, hideFlags = HideFlags.DontSave };
                mesh.vertices = md.positions; mesh.normals = md.normals; mesh.uv = md.uv0; mesh.uv2 = md.uv1;
                if (md.colors != null) mesh.colors32 = md.colors;
                if (md.tangents != null) mesh.tangents = md.tangents.Select(t => new Vector4(t.x, t.y, t.z, 1)).ToArray();
                mesh.subMeshCount = md.triangles.Count;
                for (int s = 0; s < md.triangles.Count; s++) mesh.SetTriangles(md.triangles[s], s, false);
                mesh.RecalculateBounds();
                mesh.UploadMeshData(true);
                Track(mesh); meshes[kv.Key] = mesh;
            }
            foreach (var kv in sceneMaterials)
            {
                var m = ReflexSceneryMaterial.Create(this, kv.Value, shaders, TextureFor, CubeFor);
                if (m) { Track(m); materialObjects[kv.Key] = m; }
            }
            var groups = new Dictionary<string, Transform>();
            foreach (var inst in instances)
            {
                if (!groups.TryGetValue(inst.group, out var gp))
                {
                    gp = new GameObject(inst.group).transform; gp.SetParent(root.transform, false); groups[inst.group] = gp;
                }
                // "<name> [<model>] #<entity id>" — the same naming as the former editor-built scene (start gate lookup)
                var go = new GameObject($"{inst.name} [{inst.model}] #{inst.entityId}");
                go.transform.SetParent(gp, false);
                ApplyMatrix(go.transform, inst.matrix);
                go.AddComponent<MeshFilter>().sharedMesh = meshes[inst.model];
                var mr = go.AddComponent<MeshRenderer>();
                mr.sharedMaterials = inst.materials.Select(n => materialObjects.TryGetValue(n, out var mm) ? mm : null).ToArray();
                mr.shadowCastingMode = ShadowCastingMode.Off;      // static shadows come from the level's volume shadow map (as in the original)
                // (no "Ad Texture" override: the PC build's MassiveAdEntity is a plain ModelEntity that draws its MaterialList, screens-ads.md)
                if (inst.scroll != Vector4.zero)
                {
                    var mpb = new MaterialPropertyBlock();
                    if (inst.scroll != Vector4.zero) mpb.SetVector("_ScrollParams", inst.scroll);
                    mr.SetPropertyBlock(mpb);
                }
                inst.transform = go.transform;
            }
            if (sky != null && shaders.sky && meshes.TryGetValue(sky.model, out var skyMesh))
            {
                var lm = sceneMaterials.TryGetValue(sky.materials[0], out var m0) ? m0 : null;
                var tex = lm != null && lm.textures.Count > 0 ? TextureFor(lm.textures[0], lm.samplerStates[0]) : null;
                var mat = new Material(shaders.sky) { name = sky.materials[0], hideFlags = HideFlags.DontSave };
                if (tex) mat.SetTexture("_BaseMap", tex);
                if (lm != null && lm.pixelShader == "SkyEntityComp" && lm.textures.Count > 1)   // the DLC levels' HDR sky (lum, saturation)
                {
                    var sat = TextureFor(lm.textures[1], lm.samplerStates[1]);
                    if (sat) mat.SetTexture("_SatMap", sat);
                    var program = lm.shader?.skyProgram;
                    if (program == null && AliveLevels.SkyHdrPath(level) is string skyHdr)
                        program = ReflexSkyProgram.ReadAliveComposite(skyHdr);
                    if (program == null) throw new InvalidOperationException("SkyEntityComp has an unsupported package-local pixel program: " + lm.name);
                    mat.SetVector("_SkySaturationScale", program.saturationScale);
                    mat.SetVector("_SkySaturationBias", program.saturationBias);
                    mat.SetVector("_SkyLuminance", program.luminance);
                    mat.EnableKeyword("_SKY_ENTITY_COMP");
                }
                if (lm != null && lm.parameters.TryGetValue("usr_self_bracketing_value", out var b) && b != null) mat.SetFloat("_Bracket", b[b.Length > 3 ? 3 : b.Length - 1]);
                mat.SetFloat("_SkyZTest", SystemInfo.usesReversedZBuffer ? (float)CompareFunction.GreaterEqual : (float)CompareFunction.LessEqual);
                mat.SetFloat("_Cull", lm != null && lm.cull == 1 ? (float)CullMode.Off : lm != null && lm.cull == 2 ? (float)CullMode.Front : (float)CullMode.Back);
                Track(mat);
                skyMesh.bounds = new Bounds(Vector3.zero, Vector3.one * 2e5f);     // follows the camera in the vertex shader: never frustum-culled
                var go = new GameObject($"{sky.name} [{sky.model}] #{sky.entityId}");
                go.transform.SetParent(root.transform, false);
                ApplyMatrix(go.transform, sky.matrix);
                go.AddComponent<MeshFilter>().sharedMesh = skyMesh;
                var mr = go.AddComponent<MeshRenderer>(); mr.sharedMaterial = mat; mr.shadowCastingMode = ShadowCastingMode.Off; mr.receiveShadows = false;
                sky.transform = go.transform;
            }
            return root;
        }

        // The fill-light rig of AmbientDirectionalLightRSData: four lights at elevation "_Light Direction Offset" degrees, at azimuths
        // +135 (back), +45 (side A), -135 (side B), -45 (front) degrees around the sun's azimuth (atan2(z, x)). Derived by fitting the
        // lights' own baked spherical light maps (NAT_CON_A_LightSLM-1067972, FR_AL_LightingSLM-4384382) with the entity's colours:
        // rmse at the DXT5 noise floor (0.006) with unit-length directions and scales 0.99..1.03 (the lights are in no scene field:
        // the engine builds them in code). The elevation is exactly the entity's "_Light Direction Offset" (30 Concord, 45 Alpine).
        // Recorded frames of the original renderer (Concord, Kingston, Alpine) give gBackLight +135, gSideLightA -135, gSideLightB +45,
        // gFrontLight -45 degrees (side A / side B were swapped here before).
        public static readonly float[] LightAzimuthOffsets = { 135f, -135f, 45f, -45f };
        public Vector3[] LightDirections()
        {
            var sun = lighting.sunDirection;
            float phi = Mathf.Atan2(sun.z, sun.x), e = lighting.lightDirectionOffset * Mathf.Deg2Rad;
            var d = new Vector3[4];
            for (int i = 0; i < 4; i++)
            {
                float a = phi + LightAzimuthOffsets[i] * Mathf.Deg2Rad;
                d[i] = new Vector3(Mathf.Cos(e) * Mathf.Cos(a), Mathf.Sin(e), Mathf.Cos(e) * Mathf.Sin(a));
            }
            return d;
        }

        /// Sets the level-wide shader globals the scenery shader reads (the original's global constants) and returns the volume
        /// shadow map texture that was bound (null when the level has none). Main thread.
        public Texture2D ApplyShaderGlobals(bool volumeShadow = true)
        {
            var l = lighting;
            var dirs = LightDirections();
            var sun = l.sunDirection.normalized;
            Shader.SetGlobalVector("_RfxSunDir", new Vector4(sun.x, sun.y, sun.z, 0));
            Shader.SetGlobalVector("_RfxSunColor", new Vector4(l.sunColor.x, l.sunColor.y, l.sunColor.z, 0));
            for (int i = 0; i < 4; i++)
            {
                Shader.SetGlobalVector("_RfxLightDir" + i, new Vector4(dirs[i].x, dirs[i].y, dirs[i].z, 0));
                Shader.SetGlobalVector("_RfxLightColor" + i, new Vector4(l.lightColors[i].x, l.lightColors[i].y, l.lightColors[i].z, 0));
            }
            Shader.SetGlobalVector("_RfxBounceColor", new Vector4(l.bounceColor.x, l.bounceColor.y, l.bounceColor.z, 0));
            // MX vs ATV Alive's light buffer, colour grade and squared albedo: only when the level is drawn with the Alive look (AliveLook.cs);
            // _RfxAliveData.x marks Alive content whatever the look (its grass / tree vertex encodings), _RfxSkyGamma brings the Alive sky's
            // HDR composite (Alive's linear colour, gamma-encoded by its tonemap) to the display values of Reflex's chain
            bool alive = AliveShading;
            Shader.SetGlobalVector("_RfxAliveData", new Vector4(AliveLevels.Is(level) ? 1f : 0f, ReflexLook ? 1f : 0f, 0f, 0f));
            Shader.SetGlobalFloat("_RfxSkyGamma", ReflexLook ? AliveLookSkyGamma : 1f);
            if (alive && l.aliveBounce != null && l.aliveBounceData != null)
            {
                l.aliveBounce.Upload(l.aliveBounceData); l.aliveBounceData = null;
                Track(l.aliveBounce.o); Track(l.aliveBounce.s); Track(l.aliveBounce.x); Track(l.aliveBounce.y); Track(l.aliveBounce.z);
            }
            AliveBounce.ApplyGlobals(alive, l.sideLightsEnabled, l.bounceLightEnabled, l.aliveBounce);
            if (alive && l.aliveLutData != null) { l.aliveLut = AliveBounce.UploadLut(l.aliveLutData); l.aliveLutData = null; Track(l.aliveLut); }
            AliveBounce.ApplyLut(alive ? l.aliveLut : null);
            // gColorRefSample c84 is (0, 0, 0, 0) for every static draw of the recorded frames: the bounce term of the vertex
            // programs (sat(-N.y) * lerp(sample.rgb, bounce, sample.w)) is zero for scenery.
            Shader.SetGlobalVector("_RfxColorRefSample", Vector4.zero);
            Shader.SetGlobalVector("_RfxColorRef", l.colorReference);
            Shader.SetGlobalVector("_RfxSpecularColor", new Vector4(l.specularColor.x, l.specularColor.y, l.specularColor.z, 0));
            // c56 while HFTerrain draws: gSunLightColor * 1.25 (FUN_007cd000, constant 0x9c4068)
            Shader.SetGlobalVector("_RfxTerrainSunColor", new Vector4(l.sunColor.x * 1.25f, l.sunColor.y * 1.25f, l.sunColor.z * 1.25f, 0));
            Shader.SetGlobalVector("_RfxFogColorStart", new Vector4(l.fogColorStart.r, l.fogColorStart.g, l.fogColorStart.b, FogAmount(l.fogAmountStart)));
            Shader.SetGlobalVector("_RfxFogColorEnd", new Vector4(l.fogColorEnd.r, l.fogColorEnd.g, l.fogColorEnd.b, FogAmount(l.fogAmountEnd)));
            // MAP SCALE (core/plan/MAP_SCALE.md): the fog's distances follow the scaled track (1 at 100 %, and always in the menu world)
            float ms = Reflex.Port.RxCore.RxMapScale.Current;
            float fogStart = l.fogDistanceStart * ms, fogEnd = l.fogDistanceEnd * ms;
            Shader.SetGlobalVector("_RfxFogParams", new Vector4(fogStart, fogEnd, FogAmount(l.fogAmountStart), FogAmount(l.fogAmountEnd)));
            // c14 gDistanceFog = (start, end, 1 / (end - start), -start / (end - start)) as recorded (Concord (0, 250, 0.004, 0), Alpine (-500, 8000, ...))
            float fogRange = fogEnd - fogStart;
            Shader.SetGlobalVector("_RfxFogDistance", fogRange != 0 ? new Vector4(fogStart, fogEnd, 1f / fogRange, -fogStart / fogRange) : new Vector4(0, 0, 0, 0));
            Texture2D vol = volumeShadow ? BuildVolumeShadowMap() : null;
            // gWorldProjectionXZY c39/c40 = (1 / (grid size * spacing)): 1/2048 Concord, 1/4096 Alpine, 1/1024 Kingston (recorded)
            float extent = terrainData != null && terrainData.size > 1 ? terrainData.size * terrainData.spacing * ms : 0f;   // MAP SCALE: the world extent
            Shader.SetGlobalVector("_RfxVolumeShadowParams", new Vector4(extent > 0 ? 1f / extent : 0f, vol ? 1f : 0f, ms, 0));   // z: RfxMapScale (ReflexCommon.hlsl)
            if (vol) Shader.SetGlobalTexture("_RfxVolumeShadowMap", vol);
            // the level's baked sun mask (Alive MapShadow): bound with the same world projection as the volume shadow map
            if (l.mapShadowData != null && extent > 0)
            {
                var mst = new Texture2D(l.mapShadowSize, l.mapShadowSize, TextureFormat.R8, true, true) { name = level + " map shadow", wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Bilinear, hideFlags = HideFlags.DontSave };
                mst.SetPixelData(l.mapShadowData, 0); mst.Apply(true, true); Track(mst);
                l.mapShadow = mst; l.mapShadowData = null;
            }
            Shader.SetGlobalVector("_RfxMapShadowParams", new Vector4(l.mapShadow != null ? 1f : 0f, extent > 0 ? 1f / extent : 0f, 0, 0));
            if (l.mapShadow != null) Shader.SetGlobalTexture("_RfxMapShadowMap", l.mapShadow);
            return vol;
        }

        /// A D3D row-vector matrix (rows 0..2 = the local axes, row 3 = translation) as a Unity transform; a mirrored matrix
        /// (negative determinant) becomes a negative X scale.
        public static void ApplyMatrix(Transform t, float[] m)
        {
            var x = new Vector3(m[0], m[1], m[2]); var y = new Vector3(m[4], m[5], m[6]); var z = new Vector3(m[8], m[9], m[10]);
            t.position = new Vector3(m[12], m[13], m[14]);
            float sx = x.magnitude, sy = y.magnitude, sz = z.magnitude;
            if (sx < 1e-8f || sy < 1e-8f || sz < 1e-8f) return;
            bool mirrored = Vector3.Dot(Vector3.Cross(x, y), z) < 0;
            t.rotation = Quaternion.LookRotation(z / sz, y / sy);
            t.localScale = new Vector3(mirrored ? -sx : sx, sy, sz);
        }
    }
}
