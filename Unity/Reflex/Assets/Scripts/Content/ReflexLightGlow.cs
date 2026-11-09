// Light glows, light rays / god rays, fog planes, HDR-safe transparent cards and the sun billboard of the levels (WP-R3,
// _re/notes/fx.md), read at run time from the player's install. What the PC build draws of the "fx" content, found from the exe and
// from frames of the original renderer recorded by render_capture:
//  - scenery models whose material's program is one of the glow family (Template_LightGlow, godraytest, FF_Lightrays,
//    NAT_DA_GodRay_NoTemp, NAT_BE_FogPlane, FR_CH_VistaFogPlane1, FF_DoorFog, HDRSafeTransFog, HDRSafeTransparency, FY_Projector,
//    SX_DLC_FY_Bink_ProjectionBeam): drawn by the ordinary model renderer (ModelEntity / ScrollingTextureEntity / ScenarioAnimEntity);
//    their programs are hand-ported in Shaders/ReflexLightGlow.shader. The scenery factory builds them as generic lit scenery;
//    ConvertGlowMaterials turns those materials (same Material objects, so every renderer, LOD and fading clone follows) into the
//    ported programs with the material's own textures (sampler order), parameters and render states.
//  - the sun: CameraFacingEntityRSData (13 levels: Flat_Plane_billboard with SUN_Material, "Axis To Align" CameraFacing) =
//    CameraFacingModelEntity (vtable 0x939c64, vf5 00410fc0 reads "Axis To Align" (+0x144) and "StartEnabled" (+0x70 / +0xa2));
//    its IRenderable (+0x90) vf2 00411050 submits the model through Model_SubmitSkinned (no draw distance), its render event
//    (+0x140) vf0 004110a0 builds the billboard matrix from the view matrix (rows = camera right / up / forward times the entity
//    matrix's row lengths, the entity's translation) and calls Model_RenderInstance. Recorded NAT_Stillwater frame 569: the sun is
//    the last draw into the HDR scene target (after the alpha-blended scenery), SRCALPHA / INVSRCALPHA, ZWRITE on, ZFUNC LESSEQUAL,
//    CULL NONE; gWorld = (right, up, forward) x 2 + entity position exactly.
// Not drawn by the PC build (N/A, evidence in fx.md): VolumeFogEntity (its render event XenonVolumeFogRenderEvent::vf0 007473d0 only
// resets its queue; nothing else reads the queue object 0xa946a4), LensFlareEntity / LightFlareEntity (their render events' only
// virtual is the empty 007e3be0), and OverlayLightRSData = OverlayColorEntity, whose colours (scene +0x6a0..+0x6d4) feed only the
// motion-blur pass's ScreenQuadVS (g_OverlayColorTop / Middle / Bottom), a pass the recorded race frames do not draw.
using System;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using UnityEngine.Rendering;

namespace Reflex.Port.Content
{
    public static class ReflexLightGlow
    {
        public const string ShaderName = "Reflex/Original Light Glow";
        public const int ProgramSun = 8;

        /// Pixel program (shader asset name) -> the port's program index (Shaders/ReflexLightGlow.shader header). Every copy of a
        /// name in the install's databases has the same instructions (checked: Template_LightGlow 2 builds, HDRSafeTransFog 2,
        /// HDRSafeTransparency 2, the rest 1; they differ only in comments / register allocation).
        public static readonly Dictionary<string, int> Programs = new Dictionary<string, int>(StringComparer.Ordinal)
        {
            ["Template_LightGlow"] = 0,
            ["godraytest"] = 1, ["FF_Lightrays"] = 1,
            ["NAT_DA_GodRay_NoTemp"] = 2, ["NAT_BE_FogPlane"] = 2,
            ["FR_CH_VistaFogPlane1"] = 3, ["FF_DoorFog"] = 3,
            ["HDRSafeTransFog"] = 4,
            ["HDRSafeTransparency"] = 5,
            ["FY_Projector"] = 6,
            ["SX_DLC_FY_Bink_ProjectionBeam"] = 7,
            ["SUN_Material"] = ProgramSun,
        };

        static readonly int[] TexIds = { Shader.PropertyToID("_Tex0"), Shader.PropertyToID("_Tex1"), Shader.PropertyToID("_Tex2"), Shader.PropertyToID("_Tex3") };

        static int Blend(int d3d)
        {
            switch (d3d)
            {
                case 1: return (int)BlendMode.Zero; case 2: return (int)BlendMode.One; case 3: return (int)BlendMode.SrcColor; case 4: return (int)BlendMode.OneMinusSrcColor;
                case 5: return (int)BlendMode.SrcAlpha; case 6: return (int)BlendMode.OneMinusSrcAlpha; case 7: return (int)BlendMode.DstAlpha; case 8: return (int)BlendMode.OneMinusDstAlpha;
                case 9: return (int)BlendMode.DstColor; case 10: return (int)BlendMode.OneMinusDstColor; case 11: return (int)BlendMode.SrcAlphaSaturate;
                default: return (int)BlendMode.One;
            }
        }

        /// Makes m the ported program of its level material: shader, textures by sampler slot, usr_* values, render states.
        internal static void Configure(Material m, Shader shader, int program, ReflexLevelContent.LevelMaterial lm, Func<string, Dictionary<int, float>, Texture2D> textureFor)
        {
            m.shader = shader;
            m.shaderKeywords = Array.Empty<string>();
            m.SetFloat("_Program", program);
            for (int i = 0; i < TexIds.Length; i++)
            {
                Texture2D t = i < lm.textures.Count && !string.IsNullOrEmpty(lm.textures[i]) ? textureFor(lm.textures[i], i < lm.samplerStates.Count ? lm.samplerStates[i] : null) : null;
                m.SetTexture(TexIds[i], t ? t : Texture2D.whiteTexture);
            }
            m.SetFloat("_Overbright", lm.parameters.TryGetValue("usr_overbright", out var ob) && ob != null && ob.Length > 0 ? ob[ob.Length > 3 ? 3 : ob.Length - 1] : 5.12f);
            m.SetFloat("_RfxBillboard", program == ProgramSun ? 1 : 0);
            // render states of the material (D3DRS_*): cull 1 none / 2 CW / 3 CCW (Unity front faces are clockwise)
            m.SetFloat("_Cull", lm.cull == 1 ? (float)CullMode.Off : lm.cull == 2 ? (float)CullMode.Front : (float)CullMode.Back);
            m.SetFloat("_ZWrite", lm.zWrite ? 1 : 0);
            m.SetFloat("_SrcBlend", lm.alphaBlend ? Blend(lm.srcBlend) : (int)BlendMode.One);
            m.SetFloat("_DstBlend", lm.alphaBlend ? Blend(lm.dstBlend) : (int)BlendMode.Zero);
            m.SetFloat("_AlphaTest", lm.alphaTest ? 1 : 0);
            m.SetFloat("_Cutoff", Mathf.Clamp01(lm.alphaRef / 255f));
            m.SetFloat("_AlphaFuncGE", lm.alphaFunc == 7 ? 1 : 0);
            m.SetFloat("_RfxFadeOpaque", 0);
            // draw order: the original draws alpha-blended scenery after the opaque scenery and the sky; the sun billboard is the last
            // draw of the HDR scene (recorded frames)
            m.renderQueue = program == ProgramSun ? (int)RenderQueue.Transparent + 450
                          : lm.alphaBlend ? (int)RenderQueue.Transparent : lm.alphaTest ? (int)RenderQueue.AlphaTest : (int)RenderQueue.Geometry;
            m.SetOverrideTag("RenderType", lm.alphaBlend ? "Transparent" : lm.alphaTest ? "TransparentCutout" : "Opaque");
        }
    }

    public sealed partial class ReflexLevelContent
    {
        /// One camera-facing model of the level (CameraFacingEntityRSData).
        public sealed class GlowBillboard
        {
            public int entityId; public string name, model, material; public float[] matrix; public int axisToAlign; public bool startEnabled;
            public Transform transform;
        }

        /// The level's CameraFacingEntityRSData entities ("Axis To Align": 0 CameraFacing, 1 YAlign ...).
        public List<GlowBillboard> CameraFacingEntities()
        {
            var list = new List<GlowBillboard>();
            if (entities == null) return list;
            foreach (var e in entities)
            {
                if (e.Type != "CameraFacingEntityRSData" || e.Matrix == null || string.IsNullOrEmpty(e.Asset("ModelAsset"))) continue;
                var mats = e.MaterialList();
                int axis = 0; var av = e.Value("Axis To Align");
                if (av != null) { if (av.value is string s) axis = s == "CameraFacing" ? 0 : s == "YAlign" ? 1 : -1; foreach (var c in av.children) if (c.value is int ci) axis = ci; if (av.value is int iv) axis = iv; }
                list.Add(new GlowBillboard { entityId = e.id, name = e.String("Name") ?? "CameraFacingEntity", model = e.Asset("ModelAsset"), material = mats.Count > 0 ? mats[0] : null,
                                             matrix = e.Matrix, axisToAlign = axis, startEnabled = !e.Has("StartEnabled") || e.Bool("StartEnabled") });
            }
            return list;
        }

        /// Converts the loaded materials whose program is a glow-family program into Shaders/ReflexLightGlow.shader, in place (the same
        /// Material objects, so every renderer and the LOD / fading clones made later follow). Materials built on a generated object
        /// program (Reflex/Vehicle) stay as they are. Returns the number converted; `done` keeps the names already handled. Main thread.
        public int ConvertGlowMaterials(Shader shader, HashSet<string> done)
        {
            if (!shader) return 0;
            int n = 0;
            foreach (var kv in materialObjects.ToArray())
            {
                if (!kv.Value || done.Contains(kv.Key)) continue;
                done.Add(kv.Key);
                if (!sceneMaterials.TryGetValue(kv.Key, out var lm) || lm.pixelShader == null || !ReflexLightGlow.Programs.TryGetValue(lm.pixelShader, out int program)) continue;
                if (kv.Value.shader && kv.Value.shader.name == "Reflex/Vehicle") continue;
                EnsureTextures(lm);
                ReflexLightGlow.Configure(kv.Value, shader, program, lm, TextureFor);
                n++;
            }
            return n;
        }

        /// Diagnostics (FxTests.Verify): the loaded materials on a glow-family program: (material, pixel program, port program, texture slots).
        public List<(string material, string pixelShader, int program, string[] textures)> GlowMaterialSurvey()
        {
            var list = new List<(string, string, int, string[])>();
            foreach (var kv in sceneMaterials.OrderBy(k => k.Key))
                if (kv.Value.pixelShader != null && ReflexLightGlow.Programs.TryGetValue(kv.Value.pixelShader, out int p)) list.Add((kv.Key, kv.Value.pixelShader, p, kv.Value.textures.ToArray()));
            return list;
        }
        /// Diagnostics: scene entities of a type (VolumeFogRSData, LensFlareRSData, OverlayLightRSData: present in the scenes, not drawn on PC).
        public int CountEntities(string type) => entities == null ? 0 : entities.Count(e => e.Type == type);

        void EnsureTextures(LevelMaterial lm)
        {
            foreach (var t in lm.textures)
                if (!string.IsNullOrEmpty(t) && !sceneTextures.ContainsKey(t) && !videoTargets.Contains(t))
                    try { var lt = ReflexTex.Parse(t, ReadAsset(t, "texture")); if (lt != null) sceneTextures[t] = lt; }
                    catch (Exception ex) { Warn($"glow texture {t}: {ex.Message}"); }
        }

        /// Builds the camera-facing models (the sun) under parent: one renderer each, the billboard orientation is made per camera by the
        /// shader (_RfxBillboard); the transform holds the entity's position, _BillboardScale its matrix row lengths. Main thread.
        public List<GlowBillboard> BuildCameraFacing(Transform parent, Shader shader)
        {
            var list = CameraFacingEntities();
            var made = new List<GlowBillboard>();
            if (!shader) return made;
            foreach (var b in list)
            {
                if (b.axisToAlign != 0) { Warn($"camera-facing entity {b.entityId}: axis mode {b.axisToAlign} not ported (front-end only)"); continue; }
                try
                {
                    if (!sceneModels.TryGetValue(b.model, out var md)) sceneModels[b.model] = md = LoadModel(b.model);
                    string mn = b.material ?? md.bindingMaterials.FirstOrDefault();
                    if (mn == null) continue;
                    if (!sceneMaterials.TryGetValue(mn, out var lm)) sceneMaterials[mn] = lm = LoadMaterial(mn);
                    if (lm.pixelShader == null || !ReflexLightGlow.Programs.TryGetValue(lm.pixelShader, out int program)) { Warn($"camera-facing material {mn}: program {lm.pixelShader} not ported"); continue; }
                    EnsureTextures(lm);
                    var mesh = new Mesh { name = md.name + " (camera facing)", hideFlags = HideFlags.DontSave };
                    mesh.vertices = md.positions; mesh.normals = md.normals; mesh.uv = md.uv0;
                    if (md.colors != null) mesh.colors32 = md.colors;
                    mesh.subMeshCount = md.triangles.Count;
                    for (int s = 0; s < md.triangles.Count; s++) mesh.SetTriangles(md.triangles[s], s, false);
                    var m = b.matrix;
                    var scale = new Vector3(new Vector3(m[0], m[1], m[2]).magnitude, new Vector3(m[4], m[5], m[6]).magnitude, new Vector3(m[8], m[9], m[10]).magnitude);
                    // the billboard turns with the camera: bounds of every orientation (a sphere around the entity's position)
                    float r = 0; foreach (var p in md.positions) r = Mathf.Max(r, Vector3.Scale(p, scale).magnitude);
                    mesh.bounds = new Bounds(Vector3.zero, Vector3.one * (2 * r + 1));
                    mesh.UploadMeshData(true); Track(mesh);
                    var mat = new Material(shader) { name = mn + " (camera facing)", hideFlags = HideFlags.DontSave };
                    ReflexLightGlow.Configure(mat, shader, program, lm, TextureFor);
                    mat.SetFloat("_RfxBillboard", 1);
                    // MAP SCALE: under the scaled scenery root the billboard moves by the scale; its view-space size follows, so the sun keeps its look
                    float ms = Reflex.Port.RxCore.RxMapScale.Current;
                    mat.SetVector("_BillboardScale", new Vector4(scale.x * ms, scale.y * ms, scale.z * ms, 0));
                    Track(mat);
                    var go = new GameObject($"{b.name} [{b.model}] #{b.entityId}");
                    go.transform.SetParent(parent, false);
                    go.transform.localPosition = new Vector3(m[12], m[13], m[14]);
                    go.AddComponent<MeshFilter>().sharedMesh = mesh;
                    var mr = go.AddComponent<MeshRenderer>();
                    mr.sharedMaterials = Enumerable.Repeat(mat, md.triangles.Count).ToArray();
                    mr.shadowCastingMode = ShadowCastingMode.Off; mr.receiveShadows = false;
                    mr.enabled = b.startEnabled;
                    b.transform = go.transform;
                    made.Add(b);
                }
                catch (Exception ex) { Warn($"camera-facing entity {b.entityId} ({b.model}): {ex.Message}"); }
            }
            return made;
        }
    }
}
