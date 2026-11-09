// The level's terrain material read at run time: the HFTerrain material (nine textures in the pixel shader's slot order, their
// sampler states, usr_* parameters), the scene lighting/fog values, the static terrain shadow visibility derived from the level's
// height map and volume shadow map, the tyre-track textures and the deformation softness mask.
// Port of _re/formats/terrain_material_export.py (+ track_texture_export.py); the shader is Assets/Shaders/ReflexTerrain.shader
// (see _re/notes/terrain-shader.md). The values go into a material exactly as the former editor builder did from terrain-shader.json.
using System;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;

namespace Reflex.Port.Content
{
    public sealed class TerrainMaterialBuild
    {
        public Material material;
        public Texture2D colorMap;            // slot 0, the top-down world map
        public int spacing, gridSize;
        public Texture2D softnessMask;        // RGB softness mask for the tyre-track decal pass (v up = +z), or null
        public Texture2D trackGeneric, trackMX, trackATV, trackDefault;
        public string shaderAsset;            // the material's HFTerrain shader asset (HFTerrain_4d_cl_s_xtra, _4d_cl_s, _sx_fy, _fmx_ff ...)
        public string shaderKeyword;          // its Reflex/Original HFTerrain variant keyword (null: the xtra math)
    }

    public sealed partial class ReflexLevelContent
    {
        // Slot order of the original material (ps_hft_lod0 samplers s0..s8) -> shader property.
        static readonly string[] TerrainSlotProperties =
        {
            "_TopDownMap", "_DetailBase", "_DetailRed", "_DetailGreen", "_DetailBlue",
            "_DetailMask", "_ChromLumMap", "_SphericalLightMap", "_SphericalReflectionMap",
        };

        /// The HFTerrain shader assets (EngineDependencies / level databases; 5 families, 48 levels) -> the variant keyword of
        /// Reflex/Original HFTerrain (ReflexTerrain.shader header). HFTerrain_Basic (only the never-raced SX_Fresno / SX_FresnoB and the
        /// *_LD greyboxes) is drawn by the xtra math with its two textures re-slotted (BasicTerrain).
        public static string TerrainVariantKeyword(string shaderAsset)
        {
            switch (shaderAsset)
            {
                case "HFTerrain_4d_cl_s": return "_HFT_4D_CL_S";
                case "HFTerrain_sx_fy": return "_HFT_SX_FY";
                case "HFTerrain_fmx_ff": return "_HFT_FMX_FF";
                default: return null;   // HFTerrain_4d_cl_s_xtra
            }
        }
        const string BasicTerrain = "HFTerrain_Basic";
        static readonly string[] TerrainVariantKeywords = { "_HFT_4D_CL_S", "_HFT_SX_FY", "_HFT_FMX_FF" };

        internal sealed class TerrainInfo
        {
            public string materialName, heightName, softnessName, shadowName, shaderAsset;
            public int spacing, size;
            public LevelMaterial material;
            public readonly ConcurrentSet textures = new ConcurrentSet();
            public byte[] shadowVis;           // size*size, D3D row order: saturate(500 * (height + 0.002 - shadow)) * 255
            public float[] shadowHeights;      // the volume shadow map (R32F), D3D row order
            public float[] heights;            // the height map (R32F), kept until BuildTerrain uploads it (_HeightMap)
            public int shadowSize, shadowHeight;   // the volume shadow map's width / height (texels)
            public float? shadowConstant;      // a 1x1 non-R32F volume shadow map (FreightYard: Editor_Default_Black): its texel's R everywhere
            public LevelTexture softness;
            public string[] trackNames = new string[4];   // generic, MX, ATV, default (UTV/Pro4/ProLite/Buggy)
            public readonly Dictionary<string, LevelTexture> trackTextures = new Dictionary<string, LevelTexture>();
        }
        internal sealed class ConcurrentSet : System.Collections.Concurrent.ConcurrentDictionary<string, LevelTexture> { }

        internal TerrainInfo terrainData;

        void StageTerrain(int stage, List<Action> jobs)
        {
            if (stage == 1)
            {
                var te = entities.FirstOrDefault(e => e.Type == "HFTerrainRSData");
                if (te == null) { Warn("level has no HFTerrain entity"); return; }
                var ti = new TerrainInfo
                {
                    materialName = te.Asset("MaterialAsset"), heightName = te.Asset("HeightMapAsset"), softnessName = te.Asset("SoftnessMask"),
                };
                var grid = te.Field("GridResolution")?.children.Find(c => c.tag == "EnumValue");
                ti.spacing = 1 << (grid?.value is int g ? g : 0);
                ti.shadowName = lighting.volumeShadowMap;
                string[] fields = { "GenericTrack", "MXTrack", "ATVTrack", "UTVTrack" };
                for (int i = 0; i < 4; i++) ti.trackNames[i] = te.Asset(fields[i]);
                terrainData = ti;
                jobs.Add(() =>
                {
                    ti.material = LoadMaterial(ti.materialName);
                    // MaterialEditorMaterial / Pass / Shaders asset="HFTerrain_..." (terrain_material_export.py parse_material)
                    ti.shaderAsset = Rfx.Bxml(ReadAsset(ti.materialName, "material")).All().FirstOrDefault(n => n.tag == "Shaders" && n.S("asset") != null)?.S("asset");
                    if (IsAliveTerrain(ti)) { }   // re-slotted in StageAliveTerrain stage 2 (Content/AliveTerrain.cs)
                    else if (ti.shaderAsset != null && ti.shaderAsset != "HFTerrain_4d_cl_s_xtra" && ti.shaderAsset != BasicTerrain && TerrainVariantKeyword(ti.shaderAsset) == null)
                        Warn($"terrain shader {ti.shaderAsset} is drawn with the HFTerrain_4d_cl_s_xtra math");
                });
                if (donorOnly) return;   // ALIVE LOOK donor: the material, its textures and the light only
                // the two R32F maps are the longest single decodes (22 MB of LZX each): start them with the first stage, in parallel
                jobs.Add(() =>
                {
                    ti.heights = ReflexTex.ReadR32F(ReadAsset(ti.heightName, "texture"), out int hw, out int hh);
                    Rfx.Check(hw == hh, "non-square height map");
                    ti.size = hw;
                    if (ExperimentalTerrain.For(level)) { ti.heights=ExperimentalTerrain.Read(TrackMods.Bound.folder); ti.size=2048; ti.spacing=2; }
                });
                if (!string.IsNullOrEmpty(ti.shadowName))
                    jobs.Add(() =>
                    {
                        var raw = ReadAsset(ti.shadowName, "texture");
                        try { ti.shadowHeights = ReflexTex.ReadR32F(raw, out ti.shadowSize, out ti.shadowHeight); }
                        catch (Exception) when (TexelOf(raw) is float c) { ti.shadowConstant = c; }   // the sampler returns the one texel everywhere
                    });
            }
            else if (stage == 2 && terrainData != null)
            {
                var ti = terrainData;
                // the nine material textures
                foreach (var name in ti.material.textures.Distinct())
                {
                    string n = name;
                    jobs.Add(() =>
                    {
                        var t = ReflexTex.Parse(n, ReadAsset(n, "texture"));
                        if (t != null) ti.textures[n] = t; else Warn($"terrain texture {n}: unsupported format");
                    });
                }
                if (ExperimentalTerrain.For(level)) jobs.Add(() =>
                {
                    const string rock="NAT_CO_Slate";
                    var t=ReflexTex.Parse(rock,ReadAsset(rock,"texture")); if(t!=null)ti.textures[rock]=t;
                });
                if (ExperimentalTerrain.For(level) && TrackMods.Bound?.id != "McKennaRock")
                {
                    // Resolve on the loading thread before texture jobs: the install's database
                    // cache is mutable, while the captured asset records are safe for workers.
                    var dirtPackage=install.Pkg("NAT_Bedford","NAT_BedfordBase");
                    foreach(string name in new[]{"NAT_BE_Dirt01","NAT_BE_Dirt02","NAT_BE_Dirt01_BumpNew","NAT_BE_Dirt02_Bump"})
                    {
                        var asset=dirtPackage?.Find(name,"texture");
                        Rfx.Check(asset!=null,"Paradise Valley dirt texture missing: "+name);
                        jobs.Add(() =>
                        {
                            var t=ReflexTex.Parse(name,install.Read(asset));
                            Rfx.Check(t!=null,"Unsupported Paradise Valley dirt texture: "+name);
                            ti.textures[name]=t;
                        });
                    }
                }
                if (ExperimentalTerrain.For(level))
                {
                    foreach(string name in new[]{"NAT_CO_B_SandBUMP","NAT_CO_B_DirtLightBUMP","NAT_CO_B_SandInsideBUMP","NAT_CO_B_GrassBUMPS","NAT_CO_B_Ruts","HFT_Default_Deform_Reveal"})
                        jobs.Add(() =>
                        {
                            var t=ReflexTex.Parse(name,ReadAsset(name,"texture"));
                            Rfx.Check(t!=null,"Unsupported MTX bump texture: "+name);
                            ti.textures[name]=t;
                        });
                }
                // AutoSphericalLightMap: the light entity loads its own SphericalLightMap (007f0c20..007f0cb5) and the renderer binds it to
                // the terrain's usr_sampler_spherical_light_map (s7) at draw time (recorded: NAT_CON_A_LightSLM-1067972 on Concord,
                // FR_AL_LightingSLM-4384382 on Alpine, instead of the material's slot-7 texture)
                if (lighting.autoSphericalLightMap && !string.IsNullOrEmpty(lighting.sphericalLightMap) && HasAsset(lighting.sphericalLightMap, "texture"))
                {
                    string n = lighting.sphericalLightMap;
                    jobs.Add(() =>
                    {
                        var t = ReflexTex.Parse(n, ReadAsset(n, "texture"));
                        if (t != null) ti.textures[n] = t; else Warn($"light spherical light map {n}: unsupported format");
                    });
                }
                // static shadow visibility: the engine's ShadowMapSampler term of ps_hft_lod0 (0ba4..0be4), per texel
                jobs.Add(() =>
                {
                    if (ti.shadowConstant is float sc)
                    {
                        ti.shadowHeights = new float[ti.size * ti.size]; ti.shadowSize = ti.shadowHeight = ti.size;
                        for (int i = 0; i < ti.shadowHeights.Length; i++) ti.shadowHeights[i] = sc;
                    }
                    if (ti.shadowHeights == null) return;
                    var heights = ti.heights; var shadow = ti.shadowHeights;
                    if (heights.Length != shadow.Length || ti.shadowSize != ti.size)
                    {   // a shadow map of another size than the height map (the community's tracks, Mods/Tracks): the shader samples it at the
                        // texel's UV (bilinear, texel centres), so it is resampled to the height map's texels
                        shadow = ResampleBilinear(shadow, ti.shadowSize, ti.shadowHeight > 0 ? ti.shadowHeight : ti.shadowSize, ti.size, ti.size);
                    }
                    Rfx.Check(heights.Length == shadow.Length, "height and shadow maps differ in size");
                    var vis = new byte[heights.Length];
                    for (int i = 0; i < vis.Length; i++)
                    {
                        double v = 500.0 * ((double)heights[i] + 0.002 - (double)shadow[i]);
                        v = v < 0 ? 0 : v > 1 ? 1 : v;
                        vis[i] = (byte)(int)(v * 255 + 0.5);
                    }
                    ti.shadowVis = vis;
                });
                if (!string.IsNullOrEmpty(ti.softnessName) && !donorOnly)
                    jobs.Add(() =>
                    {
                        var raw = ReadAsset(ti.softnessName, "texture");
                        var rgba = ReflexTex.DecodeExactRgba(raw, out int w, out int h);
                        ti.softness = new LevelTexture { name = ti.softnessName, width = w, height = h, format = TextureFormat.RGBA32 };
                        ti.softness.mips.Add(FlipRows(rgba, w, h));
                    });
            }
        }

        void StageTracks(int stage, List<Action> jobs)
        {
            if (terrainData == null || ReflexLook) return;   // ALIVE LOOK REFLEX: the donor's tyre tracks (AliveLook.ApplyDonor)
            var ti = terrainData;
            if (stage == 2)
                foreach (var name in ti.trackNames.Where(n => !string.IsNullOrEmpty(n)).Distinct())
                {
                    string n = name;
                    jobs.Add(() =>
                    {
                        string textureName = n;
                        // MX vs ATV Alive: the terrain's track fields name HFT_Track_vtex materials (diffuse tread, diffuse torn, bump tread,
                        // bump torn): the tread texture stands in for the Reflex track texture
                        if (AliveLevels.Is(level) && !HasAsset(n, "texture") && HasAsset(n, "material"))
                        {
                            var m = LoadMaterial(n);
                            if (m.textures.Count == 0) return;
                            textureName = m.textures[0];
                        }
                        var t = ReflexTex.Parse(textureName, ReadAsset(textureName, "texture"));
                        if (t != null) lock (ti.trackTextures) ti.trackTextures[n] = t;
                    });
                }
        }

        /// The R value of a 1x1 texture (Editor_Default_Black / White), or null.
        static float? TexelOf(byte[] raw)
        {
            try { var px = ReflexTex.DecodeExactRgba(raw, out int w, out int h); return w == 1 && h == 1 ? px[0] / 255f : (float?)null; }
            catch (Exception) { return null; }
        }

        static byte[] FlipRows(byte[] src, int w, int h)
        {
            var o = new byte[src.Length]; int row = w * 4;
            for (int y = 0; y < h; y++) Buffer.BlockCopy(src, y * row, o, (h - 1 - y) * row, row);
            return o;
        }

        // ---------------------------------------------------------------------------------------- Unity objects

        static Vector4 V4(float[] a) => a == null ? Vector4.zero : new Vector4(a[0], a[1], a[2], a.Length > 3 ? a[3] : 0f);

        /// A terrain texture with the sampler state the PC original recorded for every terrain draw (render-pipeline.md: MIN / MAG / MIP all
        /// linear): trilinear. The material's XenonSamplerStates say MIPFILTER point (the console's), which the port used to follow: with the
        /// detail layers' -0.67 mip bias that put a hard line on the ground wherever the mip level stepped, a line that moved with the camera.
        /// `aniso` (OPEN REFLEX, not the original): anisotropic filtering for the tiled detail layers, whose grazing-angle smear was part of the
        /// same complaint (the original's MAXANISOTROPY is 1).
        /// Anisotropic filtering of the terrain's detail layers and the RT_Bump bump textures (w-terrainseam; 1 = the original's isotropic look).
        public const int DetailAniso = 1;   // the original's isotropic filtering (MAXANISOTROPY 1)

        Texture2D TerrainTexture(string name, Dictionary<int, float> st, int aniso = 1)
        {
            if (string.IsNullOrEmpty(name) || !terrainData.textures.TryGetValue(name, out var lt)) return null;
            bool clamp = st != null && st.TryGetValue(1, out var au) && au == 3f;
            var wrap = clamp ? TextureWrapMode.Clamp : TextureWrapMode.Repeat;
            var tex = ReflexTex.ToTexture(lt, wrap, wrap, FilterMode.Trilinear, aniso);
            Track(tex);
            return tex;
        }

        static Texture2D ByteTexture(string name, byte[] d3dRows, int w, int h, TextureFormat fmt)
        {
            // d3dRows: w*h bytes, row 0 = top; Unity row order is bottom first
            var flipped = new byte[d3dRows.Length];
            for (int y = 0; y < h; y++) Buffer.BlockCopy(d3dRows, y * w, flipped, (h - 1 - y) * w, w);
            var tex = new Texture2D(w, h, fmt, true, true) { name = name, wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Bilinear, hideFlags = HideFlags.DontSave };
            tex.SetPixelData(flipped, 0); tex.Apply(true, true);
            return tex;
        }

        /// The terrain's height grid as a world without a running game needs it (the front end's menu world, OpenMenuWorld): heights in feet,
        /// raw * 2048 + the neutral deformation's offset ((128 / 255 - 0.5) * 2 * gain, the core's rx_session_terrain_heights rule; gain 2 as
        /// HFT_Deform_Default), in the height map's own row order (row 0 = the grid's +z row, the core's grid order), the sample count per side and
        /// the spacing (1 << GridResolution). Call before BuildTerrain, which uploads the map and drops it. False when the level has none. A copy.
        public bool TerrainHeights(out float[] heights, out int samples, out int spacing)
        {
            heights = null; samples = 0; spacing = 0;
            var ti = terrainData;
            if (ti?.heights == null || ti.size < 2 || ti.heights.Length != ti.size * ti.size) return false;
            const float neutral = (128f / 255f - 0.5f) * 2f * 2f;
            heights = new float[ti.heights.Length];
            for (int i = 0; i < heights.Length; i++) heights[i] = ti.heights[i] * 2048f + neutral;
            samples = ti.size; spacing = ti.spacing;
            return true;
        }

        /// The Reflex/Original HFTerrain material with the level's own values, the track and softness textures. Main thread.
        public TerrainMaterialBuild BuildTerrain(Shader terrainShader)
        {
            var ti = terrainData;
            if (ti == null || ti.material == null) return null;
            var result = new TerrainMaterialBuild { spacing = ti.spacing, gridSize = ti.size, shaderAsset = ti.shaderAsset, shaderKeyword = TerrainVariantKeyword(ti.shaderAsset) };
            if (terrainShader)
            {
                var mat = new Material(terrainShader) { name = $"{level} original HFTerrain ({ti.materialName})", hideFlags = HideFlags.DontSave };
                var lm = ti.material;
                mat.SetFloat("_AliveTerrain", IsAliveTerrain(ti) ? 1f : 0f);
                // RESTORED (w-fresnob): HFTerrain_Basic's ps_hft_lod0 is the xtra program without the detail layers and the specular term: colour =
                // TopDown(s0) * (SphericalLightMap(s1, by the per-pixel normal).rgb + .a * sunVis * sunColor). Its material lists only those two
                // textures, so its s1 is the light map (slot 7 here) and the detail fade is held at 0 (albedo = the top-down map); the detail mask
                // stays black (no specular).
                bool basic = ti.shaderAsset == BasicTerrain && !IsAliveTerrain(ti);
                for (int i = 0; i < TerrainSlotProperties.Length && i < lm.textures.Count && (!basic || i < 2); i++)
                {
                    string texName = lm.textures[i];
                    int slot = basic && i == 1 ? 7 : i;
                    if (slot == 7 && lighting.autoSphericalLightMap && !string.IsNullOrEmpty(lighting.sphericalLightMap) && ti.textures.ContainsKey(lighting.sphericalLightMap))
                        texName = lighting.sphericalLightMap;     // the light's own map (see StageTerrain)
                    var tex = TerrainTexture(texName, lm.samplerStates[i], !basic && i >= 1 && i <= 4 ? DetailAniso : 1);   // s1..s4: the tiled detail layers
                    if (tex) mat.SetTexture(TerrainSlotProperties[slot], tex);
                    if (i == 0) result.colorMap = tex;
                }
                if (ti.shadowVis != null)
                {
                    var vis = ByteTexture($"{level} terrain shadow visibility", ti.shadowVis, ti.size, ti.size, TextureFormat.R8);
                    Track(vis); mat.SetTexture("_ShadowVisMap", vis);
                }
                else mat.SetFloat("_UseShadowVis", 0f);
                // the pixel program's HeightMapSampler (s9): the normal is rebuilt from it per pixel with g_TerrainStrength.y = 6144 (three times
                // the geometry's 2048) and g_NormalStrength.x = 2 * spacing, as recorded on every level (ReflexTerrain.shader 0788..0924)
                if (ti.heights != null && ti.heights.Length == ti.size * ti.size)
                {
                    var hm = new Texture2D(ti.size, ti.size, TextureFormat.RFloat, false, true) { name = $"{level} terrain height map (s9)", wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Point, hideFlags = HideFlags.DontSave };
                    hm.SetPixelData(ti.heights, 0); hm.Apply(false, true);   // D3D row order (row 0 = the grid's top row): the shader Loads texels
                    Track(hm); mat.SetTexture("_HeightMap", hm);
                    mat.SetVector("_GridParams", new Vector4(ti.size, 1f, 2f * ti.spacing, 0f));
                    ti.heights = null;
                }
                else mat.SetVector("_GridParams", new Vector4(ti.size, 0f, 2f * ti.spacing, 0f));
                float[] P(string n) => lm.parameters.TryGetValue(n, out var v) ? v : null;
                mat.SetVector("_DetailTile", V4(P("usr_detail_tile")));
                var tint = V4(P("usr_dirt_tint_color"));
                mat.SetVector("_DirtTint", tint);       // raw values: SetVector on a Vector property (no colour-space conversion)
                mat.SetVector("_BumpScalar", V4(P("usr_bump_scalar")));
                float half = 1023.5f * ti.spacing;      // grid centred on the origin; OriginalTerrain overwrites this from the live world
                mat.SetVector("_GridOrigin", new Vector4(-half, half, 2047f * ti.spacing, 0f));
                var sun = lighting.sunDirection.normalized;
                mat.SetVector("_SunDirection", new Vector4(sun.x, sun.y, sun.z, 0f));
                // c56 while HFTerrain draws = gSunLightColor * 1.25 (FUN_007cd000 multiplies by the constant at 0x9c4068)
                mat.SetVector("_SunColor", new Vector4(lighting.sunColor.x * 1.25f, lighting.sunColor.y * 1.25f, lighting.sunColor.z * 1.25f, 0f));
                mat.SetVector("_FogColorStart", new Vector4(lighting.fogColorStart.r, lighting.fogColorStart.g, lighting.fogColorStart.b, 0f));
                mat.SetVector("_FogColorEnd", new Vector4(lighting.fogColorEnd.r, lighting.fogColorEnd.g, lighting.fogColorEnd.b, 0f));
                float ms = Reflex.Port.RxCore.RxMapScale.Current;   // MAP SCALE: the fog follows the scaled track (1 in the menu world)
                mat.SetVector("_FogParams", new Vector4(lighting.fogDistanceStart * ms, lighting.fogDistanceEnd * ms, FogAmount(lighting.fogAmountStart), FogAmount(lighting.fogAmountEnd)));
                mat.SetVector("_DetailFade", basic ? new Vector4(-1e9f, 1f, 0f, 0f) : new Vector4(384f, 128f, 0f, 0f));   // ps_hft_lod0 / vs_hft_lod0 constants (Basic: no detail)
                mat.SetVector("_DetailParams", new Vector4(-0.67f, 0.25f, 0f, 0f));
                // the shader variant of the material's HFTerrain shader asset, and the radiosity (colour reference) texel it samples
                foreach (var k in TerrainVariantKeywords) mat.DisableKeyword(k);
                if (result.shaderKeyword != null) mat.EnableKeyword(result.shaderKeyword);
                mat.SetVector("_ColorRef", lighting.colorReference);
                if (ReflexLook && IsAliveTerrain(ti)) BuildLookTerrain(mat, ti);   // ALIVE LOOK REFLEX (Content/AliveLook.cs)
                if (ExperimentalTerrain.For(level))
                {
                    mat.SetFloat("_UseShadowVis",0);
                    mat.SetVector("_MtxDetail",new Vector4(.22f,.32f,240f,0));
                    var surface=ExperimentalTerrain.Surface(TrackMods.Bound.folder);
                    if(surface!=null) BuildMtxSurface(mat,result,ti,surface);
                    else
                    {
                        var courseMask=ExperimentalTerrain.CourseMask(TrackMods.Bound.folder); Track(courseMask); mat.SetTexture("_DetailMask",courseMask);
                        mat.SetTexture("_TopDownMap",Texture2D.grayTexture);
                        mat.SetTexture("_ChromLumMap",Texture2D.grayTexture);
                        mat.SetVector("_DetailFade",new Vector4(100000f,128f,0,0));
                    }
                }
                if (ExperimentalTerrain.For(level)) { BindMtxLightMaps(mat, ti); mtxTerrainMaterial=mat; }
                result.material = mat; Track(mat);
            }
            // tyre-track textures: U clamp, V repeat, bilinear, linear data (track_texture_export.py + OriginalTracks import settings)
            Texture2D Track_(int i)
            {
                string n = ti.trackNames[i];
                if (string.IsNullOrEmpty(n) || !ti.trackTextures.TryGetValue(n, out var lt)) return null;
                var t = ReflexTex.ToTexture(lt, TextureWrapMode.Clamp, TextureWrapMode.Repeat, FilterMode.Bilinear, 1);
                Track(t); return t;
            }
            result.trackGeneric = Track_(0); result.trackMX = Track_(1); result.trackATV = Track_(2); result.trackDefault = Track_(3);
            if (ti.softness != null && !ExperimentalTerrain.For(level))
            {
                var s = ReflexTex.ToTexture(ti.softness, TextureWrapMode.Clamp, TextureWrapMode.Clamp, FilterMode.Bilinear, 1);
                if (s) { s.name = "Original SoftnessMask"; Track(s); result.softnessMask = s; }
            }
            if (ExperimentalTerrain.For(level) && !result.softnessMask) result.softnessMask = Texture2D.whiteTexture;
            return result;
        }

        /// The level's volume shadow map (R32F, shadow height in units of 2048 m, terrain UV space) for the scenery shaders, or null.
        public Texture2D BuildVolumeShadowMap()
        {
            var ti = terrainData;
            if (ti?.shadowHeights == null) return null;
            int n = ti.shadowSize, m = ti.shadowHeight > 0 ? ti.shadowHeight : n;
            if (ti.shadowHeights.Length != n * m) return null;
            var flipped = new float[n * m];
            for (int y = 0; y < m; y++) Array.Copy(ti.shadowHeights, y * n, flipped, (m - 1 - y) * n, n);
            var tex = new Texture2D(n, m, TextureFormat.RFloat, false, true) { name = level + " volume shadow map", wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Bilinear, hideFlags = HideFlags.DontSave };
            tex.SetPixelData(flipped, 0); tex.Apply(false, true);
            Track(tex);
            return tex;
        }

        /// A w x h float map sampled bilinearly (clamped, texel centres aligned as a GPU samples it at the same UV) at W x H texels.
        internal static float[] ResampleBilinear(float[] src, int w, int h, int W, int H)
        {
            var dst = new float[W * H];
            if (src == null || w <= 0 || h <= 0 || src.Length < w * h) return dst;
            for (int y = 0; y < H; y++)
            {
                double fy = (y + 0.5) * h / H - 0.5; int y0 = (int)Math.Floor(fy); double ty = fy - y0;
                int ya = Math.Max(0, Math.Min(h - 1, y0)), yb = Math.Max(0, Math.Min(h - 1, y0 + 1));
                for (int x = 0; x < W; x++)
                {
                    double fx = (x + 0.5) * w / W - 0.5; int x0 = (int)Math.Floor(fx); double tx = fx - x0;
                    int xa = Math.Max(0, Math.Min(w - 1, x0)), xb = Math.Max(0, Math.Min(w - 1, x0 + 1));
                    double top = src[ya * w + xa] * (1 - tx) + src[ya * w + xb] * tx, bottom = src[yb * w + xa] * (1 - tx) + src[yb * w + xb] * tx;
                    dst[y * W + x] = (float)(top * (1 - ty) + bottom * ty);
                }
            }
            return dst;
        }

        public int TerrainSpacing => terrainData?.spacing ?? 0;
        public int TerrainGridSize => terrainData?.size ?? 0;
        void BuildMtxSurface(Material mat, TerrainMaterialBuild result, TerrainInfo ti, byte[] surface)
        {
            const int n=2048;
            var authored=ExperimentalTerrain.Colour(TrackMods.Bound.folder);
            mat.SetFloat("_MtxSourceColour",authored!=null?1:0);
            mat.SetVector("_MtxDetail",new Vector4(.22f,.32f,240f,4f)); // normalize grain around dark soil luminance
            // MTX lighting (ExperimentalMtxLighting.cs): the authored colour is MTX's display colour (texture x prelit vertex colour). Like
            // every Reflex texture it is used raw, as the albedo that, under the venue's light on open level ground, shows that colour: divided
            // by the flat ground's light, times MTX's world tint (lev_*). Legacy: decoded as sRGB.
            var mtxVenue=ExperimentalMtxLighting.Current; bool rawColour=authored!=null && mtxVenue!=null;
            var groundGain=Vector3.one;
            if(rawColour)
            {
                var tint=ExperimentalMtxLighting.WorldTint(mtxVenue);
                float flat=Mathf.Max(MtxFlatGroundLight(),.5f);
                groundGain=new Vector3(Mathf.Min(1f,tint.x/flat),Mathf.Min(1f,tint.y/flat),Mathf.Min(1f,tint.z/flat));
                Warn($"[MTX] ground colour raw, gain {groundGain:F3} (flat ground light {flat:F2})");
            }
            var mask=new Color32[n*n];var soft=new Color32[n*n];var far=new Color32[n*n];var chroma=new Color32[n*n];
            for(int z=0;z<n;z++)for(int x=0;x<n;x++)
            {
                int src=(z*n+x)*4,dst=(n-1-z)*n+x;
                float grass=surface[src]/255f,rough=surface[src+1]/255f,rock=surface[src+2]/255f;
                float dirt=Mathf.Max(0,1-grass-rock);
                // Broad drying patches and smaller loose-soil pockets, stable in track coordinates.
                // Only the visual mix changes: softness still comes from the authored surfaces.
                float broad=Mathf.PerlinNoise(x/73f+17.3f,z/73f+41.7f);
                float patches=Mathf.PerlinNoise(x/13f+9.2f,z/13f+63.1f);
                float grain=Mathf.PerlinNoise(x/3.7f+32.8f,z/3.7f+11.6f);
                float authoredRough=rough/Mathf.Max(dirt,.0001f);
                rough=dirt*Mathf.Lerp(.10f+.28f*patches,.52f+.33f*patches,authoredRough);
                float tone=.76f+.18f*broad+.06f*grain;
                // Bedford samples are still too bright under the Concord donor light rig.
                // Calibrate soil reflectance, not global exposure: grass and rock keep their response.
                // MTX lighting (ExperimentalMtxLighting.cs; bright scenery, the venue's own sun, Concord's exposure): .75 keeps the soil dark
                // brown without the near-black it showed at .42 (.6: still nearly black in the shade) under the new rig
                tone*=Mathf.Lerp(1f,ExperimentalMtxLighting.Current!=null?.75f:.42f,dirt);
                if(authored!=null)tone=1f;
                chroma[dst]=new Color32(0,0,0,(byte)(127.5f*tone));
                // Sequential lerps: rough soil, then rock, then grass (which has final priority).
                float rockMask=rock/Mathf.Max(1-grass,.0001f);
                float roughMask=rough/Mathf.Max(1-grass-rock,.0001f);
                mask[dst]=new Color32((byte)(255*Mathf.Clamp01(roughMask)),(byte)(255*Mathf.Clamp01(rockMask)),surface[src],(byte)Mathf.Lerp(10,3,dirt));
                soft[dst]=new Color(.85f*dirt+.30f*grass+.05f*rock,.65f*dirt+.25f*grass+.05f*rock,1,1);
                var color=new Color(.305f,.242f,.200f)*Mathf.Max(0,1-grass-rough-rock)
                    +new Color(.320f,.265f,.217f)*rough+new Color(.25f,.24f,.22f)*rock+new Color(.180f,.181f,.094f)*grass;
                color*=tone*(.91f+.18f*patches);color.a=1;far[dst]=color;
                if(authored!=null)
                {
                    if(rawColour)
                    {
                        // MTX's colour with a little more saturation (luminance kept): brown desert dirt otherwise reads grey under the near-white rig
                        float r=authored[src],g=authored[src+1],b=authored[src+2],y=.3f*r+.59f*g+.11f*b,k=ExperimentalMtxLighting.GroundSaturation;
                        r=Mathf.Max(0,y+(r-y)*k);g=Mathf.Max(0,y+(g-y)*k);b=Mathf.Max(0,y+(b-y)*k);
                        far[dst]=new Color32((byte)Mathf.Min(255,r*groundGain.x+.5f),(byte)Mathf.Min(255,g*groundGain.y+.5f),(byte)Mathf.Min(255,b*groundGain.z+.5f),255);
                    }
                    else far[dst]=new Color32(authored[src],authored[src+1],authored[src+2],255);
                }
            }
            Texture2D Own(string name,Color32[] pixels,int size,bool linear=true)
            {
                var t=new Texture2D(size,size,TextureFormat.RGBA32,true,linear){name=name,wrapMode=TextureWrapMode.Clamp,filterMode=FilterMode.Trilinear};
                t.SetPixels32(pixels);t.Apply(true,true);Track(t);return t;
            }
            mat.SetTexture("_DetailMask",Own("MTX authored ground materials",mask,n));
            // Source RGB is display-encoded, like the imported scenery textures: raw (above, the Reflex pipeline's convention), or with
            // REFLEX_MTX_LIGHT=legacy decoded on sampling as before.
            result.colorMap=Own("MTX ground macro colour",far,n,authored==null || rawColour);
            mat.SetTexture("_TopDownMap",result.colorMap);
            mat.SetTexture("_ChromLumMap",Own("MTX soil colour variation",chroma,n));
            result.softnessMask=Own("MTX dirt grass rock softness",soft,n);
            string[] names={"NAT_BE_Dirt01","NAT_BE_Dirt02","NAT_CO_Slate","NAT_CO_B_Grass"};
            for(int i=0;i<4;i++)
            {
                var tex=TerrainTexture(names[i],ti.material.samplerStates[1],DetailAniso);
                if(tex)mat.SetTexture(TerrainSlotProperties[i+1],tex);
            }
            // Donor terrain has one-foot cells. Retain its physical texture size when this two-foot grid is enlarged.
            mat.SetVector("_DetailTile",new Vector4(128,128,128,50)*(2*Reflex.Port.RxCore.RxMapScale.Current));
            mat.SetVector("_DetailFade",new Vector4(768,256,0,0));
            Debug.Log(authored!=null ? "[MTX] Authored venue colours and surface placement, Reflex soil/grass/rock detail, native relief and tire marks" : "[MTX] Paradise dark soil, native relief and tire marks");
        }
    }
}
