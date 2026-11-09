// MX vs ATV Alive terrain: the decal layers its virtual texture is composited from (HFDecalModel entities, listed by the overlay installer
// in Database/alive-<level>-decals.json, ordered by SortBias) and the HFTerrain_vtex* material's layout. Alive draws every decal mesh
// top-down into the terrain's virtual texture pages (the HFB_* programs: StaticVertexShader projects (x, z) only) and its terrain program
// (ps_hft_lod0 of HFTerrain_vtex_xtraRelief) samples that texture, adjusts it by the chrominance / luminance map
// ((vt + 0.25 cl.rgb) sat(2 cl.a) + sat(cl.a - 0.5)), squares it and multiplies it by the light buffer. The client renders the same
// decals into camera-centred terrain-space rings (OriginalWorld/AliveTerrainVT.cs, Shaders/ReflexAliveDecal.shader).
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEngine;

namespace Reflex.Port.Content
{
    /// One decal layer ready to draw: its mesh ((x, z) in position.xy, uv0 in Unity v, alpha in colour.a), the Reflex/Alive Terrain Decal
    /// material of its program, the SortBias.
    public sealed class AliveDecalDraw { public string name, program; public Mesh mesh; public Material material; public float sortBias; public int order; }

    public sealed partial class ReflexLevelContent
    {
        [Serializable] sealed class DecalEntry { public string name, model, surface, diffuse, bump, relief; public float sort_bias; public int group, order; public float[] matrix; }
        [Serializable] sealed class DecalFile { public string level; public DecalEntry[] decals; }

        sealed class DecalData
        {
            public DecalEntry entry; public LevelMaterial material; public string program;
            public Vector3[] positions; public Vector2[] uv; public Color32[] colors; public int[] triangles;
        }
        readonly List<DecalData> aliveDecals = new List<DecalData>();
        public int AliveDecalCount => aliveDecals.Count;

        /// An MX vs ATV Alive terrain material (the virtual-texture HFTerrain_vtex* programs).
        bool IsAliveTerrain(TerrainInfo ti) => ti?.shaderAsset != null && ti.shaderAsset.StartsWith("HFTerrain_vtex", StringComparison.Ordinal) && AliveLevels.Is(level);

        /// The HFB program families the decal shader implements (Reflex/Alive Terrain Decal _Program).
        static int AliveDecalProgram(string program)
        {
            switch (program)
            {
                case "HFB_4tex": return 0;
                case "HFB_4tex_texvertalpha": return 1;
                case "HFB_1tex_vertalpha": return 2;
                case "HFB_1tex": case "HFB_1tex_deformed": case "HFB_2tex": case "HFB_2tex_1c": return 3;
                case "HFB_4tex_vertalpha": return 4;
                default: return -1;
            }
        }

        void StageAliveTerrain(int stage, List<Action> jobs)
        {
            var alive = AliveLevels.Get(level);
            if (alive == null) return;
            if (stage == 1)
            {
                string path = AliveLevels.Sidecar(alive.terrain_decals);
                if (path == null) return;
                DecalFile file;
                try { file = JsonUtility.FromJson<DecalFile>(RxFiles.ReadAllText(path)); }   // (packed with its level: RxFiles)
                catch (Exception e) { Warn("Alive terrain decals: " + e.Message); return; }
                foreach (var e in file?.decals ?? Array.Empty<DecalEntry>())
                {
                    var d = new DecalData { entry = e };
                    lock (aliveDecals) aliveDecals.Add(d);
                    jobs.Add(() =>
                    {
                        try
                        {
                            d.material = LoadMaterial(e.diffuse);
                            d.program = d.material.pixelShader ?? Rfx.Bxml(ReadAsset(e.diffuse, "material")).All().FirstOrDefault(n => n.S("asset") != null)?.S("asset");
                            var model = Rfx.ParseModel(ReadAsset(e.model, "model"));
                            var surfaces = Rfx.ParseSurfaces(ReadAsset(e.surface ?? e.model, "surface"));
                            var pos = new List<Vector3>(); var uv = new List<Vector2>(); var col = new List<Color32>(); var tri = new List<int>();
                            var entity = Matrix4x4.identity;
                            if (e.matrix != null && e.matrix.Length == 16)
                                for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) entity[c, r] = e.matrix[r * 4 + c];
                            foreach (var b in model.Lod(0))
                            {
                                var s = surfaces[b.surface]; var w = entity * model.nodes[b.node].world;
                                int at = pos.Count;
                                for (int i = 0; i < s.positions.Length; i++)
                                {
                                    var p = w.MultiplyPoint3x4(s.positions[i]);
                                    pos.Add(new Vector3(p.x, p.z, 0f));
                                    uv.Add(s.uv0 != null ? new Vector2(s.uv0[i].x, 1f - s.uv0[i].y) : Vector2.zero);
                                    col.Add(s.colors != null ? s.colors[i] : new Color32(255, 255, 255, 255));
                                }
                                foreach (int t in s.Triangles()) tri.Add(at + t);
                            }
                            d.positions = pos.ToArray(); d.uv = uv.ToArray(); d.colors = col.ToArray(); d.triangles = tri.ToArray();
                        }
                        catch (Exception ex) { Warn($"Alive terrain decal {e.name}: {ex.Message}"); d.material = null; }
                    });
                }
            }
            else if (stage == 2 && terrainData != null)
            {
                var ti = terrainData;
                // The terrain material of an Alive level (HFTerrain_vtex_xtraRelief: chrominance / luminance map, specular mask, gray) gets
                // the base decal layer's textures in the Reflex terrain slots (0 placeholder, 1..4 the layers, 5 the mask; 6 the
                // chrominance / luminance map), the terrain shader's own splat before the decal composite exists.
                // (ALIVE LOOK REFLEX: not re-slotted; the donor's detail set goes in, AliveLook.cs BuildLookTerrain)
                var baseLayer = IsAliveTerrain(ti) && !ReflexLook ? AliveBaseLayer() : null;
                if (baseLayer != null && baseLayer.textures.Count >= 5 && ti.material.textures.Count >= 1)
                {
                    string chromLum = ti.material.textures[0]; var chromState = ti.material.samplerStates[0];
                    ti.material.textures.Clear(); ti.material.samplerStates.Clear();
                    foreach (int slot in new[] { 0, 0, 1, 2, 3, 4 })
                    {
                        ti.material.textures.Add(baseLayer.textures[slot]); ti.material.samplerStates.Add(baseLayer.samplerStates[slot]);
                    }
                    ti.material.textures.Add(chromLum); ti.material.samplerStates.Add(chromState);
                    if (baseLayer.parameters.TryGetValue("usr_bump_map_tile_rate", out var tile)) ti.material.parameters["usr_detail_tile"] = tile;
                }
                foreach (var name in aliveDecals.Where(d => d.material != null).SelectMany(d => d.material.textures).Distinct())
                {
                    string n = name;
                    if (ti.textures.ContainsKey(n)) continue;
                    jobs.Add(() =>
                    {
                        try { var t = ReflexTex.Parse(n, ReadAsset(n, "texture")); if (t != null) ti.textures[n] = t; }
                        catch (Exception ex) { Warn($"Alive terrain decal texture {n}: {ex.Message}"); }
                    });
                }
            }
        }

        /// The base layer of an Alive terrain (the first full-terrain HFB_4tex decal): its material, for the terrain shader's own splat
        /// (used when the decal composite is not available, e.g. the first frame).
        LevelMaterial AliveBaseLayer() =>
            aliveDecals.Where(d => d.material != null && AliveDecalProgram(d.program) == 0).OrderBy(d => d.entry.sort_bias).ThenBy(d => d.entry.order)
                       .Select(d => d.material).FirstOrDefault();

        /// The decal layers as Unity meshes and Reflex/Alive Terrain Decal materials, in drawing order. Main thread.
        public List<AliveDecalDraw> BuildAliveTerrainDecals(Shader decalShader)
        {
            var list = new List<AliveDecalDraw>();
            if (!decalShader || terrainData == null) return list;
            foreach (var d in aliveDecals.OrderBy(d => d.entry.sort_bias).ThenBy(d => d.entry.order))
            {
                int program = AliveDecalProgram(d.program);
                if (d.material == null || d.positions == null || d.positions.Length == 0 || program < 0)
                {
                    if (d.material != null && program < 0) Warn($"Alive terrain decal {d.entry.name}: program {d.program} not drawn");
                    continue;
                }
                var mesh = new Mesh { name = "Alive decal " + d.entry.name, indexFormat = d.positions.Length > 65000 ? UnityEngine.Rendering.IndexFormat.UInt32 : UnityEngine.Rendering.IndexFormat.UInt16, hideFlags = HideFlags.DontSave };
                mesh.vertices = d.positions; mesh.uv = d.uv; mesh.colors32 = d.colors; mesh.triangles = d.triangles;
                mesh.bounds = new Bounds(Vector3.zero, new Vector3(1e6f, 1e6f, 1e6f));
                Track(mesh);
                var m = new Material(decalShader) { name = "Alive decal " + d.material.name, hideFlags = HideFlags.DontSave };
                m.SetFloat("_Program", program);
                string[] slots = { "_Layer0", "_Layer1", "_Layer2", "_Layer3", "_Mask" };
                var tex = d.material.textures;
                if (program == 2 || program == 3)
                {
                    if (tex.Count > 0) m.SetTexture("_Layer0", TerrainTexture(tex[0], d.material.samplerStates[0]));
                }
                else
                    for (int i = 0; i < slots.Length && i < tex.Count; i++)
                        m.SetTexture(slots[i], TerrainTexture(tex[i], d.material.samplerStates[i]));
                if (d.material.parameters.TryGetValue("usr_bump_map_tile_rate", out var tile) && tile != null && tile.Length >= 4)
                    m.SetVector("_TileRate", new Vector4(tile[0], tile[1], tile[2], tile[3]));
                m.SetFloat("_Opaque", list.Count == 0 ? 1f : 0f);   // the first layer covers the ring
                Track(m);
                list.Add(new AliveDecalDraw { name = d.entry.name, program = d.program, mesh = mesh, material = m, sortBias = d.entry.sort_bias, order = d.entry.order });
            }
            return list;
        }
    }
}
