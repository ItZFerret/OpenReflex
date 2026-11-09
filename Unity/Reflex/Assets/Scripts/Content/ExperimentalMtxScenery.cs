using System;
using System.IO;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;
namespace Reflex.Port.Content
{
    // RXSC preserves the source material layers, vertex lighting, alpha and object placements.
    public static class ExperimentalMtxScenery
    {
        static void Require(bool value,string why) { if(!value) throw new InvalidDataException("RXSC: "+why); }
        sealed class LayerDef { public uint texture; public int blend, fixedAlpha, uvSet; public bool env, ignoreVertexAlpha, clampU, clampV; public float tileU, tileV; public Color color = Color.white; }
        sealed class MaterialDef
        {
            public uint texture; public int blend, cutoff, fixedAlpha; public bool texAlpha, flat, ignoreVertexAlpha, clampU, clampV, env; public float tileU = 1, tileV = 1;
            public Color color = Color.white;
            public readonly List<LayerDef> layers = new List<LayerDef>();
        }

        /// The venue's scenery from its render cache (world.mtx) under parent; the sky domes go in skies. Returns the counts.
        internal static (int surfaces, int textures) BuildScenery(byte[] data, Transform parent, List<UnityEngine.Object> owned, List<Transform> skies)
        {
            T Own<T>(T value) where T : UnityEngine.Object { owned.Add(value); return value; }
            var shader = UnityEngine.Resources.Load<Shader>("ReflexMtxScenery"); Require(shader, "world shader");
            // MTX lighting (ExperimentalMtxLighting.cs): raw texture values like every Reflex world shader (MTX's own texture x prelit colour
            // x 2), the venue's world / sky tint and Reflex's distance fog. REFLEX_MTX_LIGHT=legacy: sRGB textures as before.
            var venue = ExperimentalMtxLighting.Current; bool raw = venue != null;
            var textures = new Dictionary<uint, Texture2D>(); int surfaces = 0;
            var built = new Dictionary<(int, bool, bool, bool), Material>();
            using var r = new BinaryReader(new MemoryStream(data, false));
            Require(r.ReadInt32() == 0x43535852, "scenery magic");
            int version=r.ReadInt32(); Require(version>=1 && version<=3, "scenery version");
            int nt = r.ReadInt32(); Require(nt >= 0 && nt < 8192, "texture count");
            for (int i = 0; i < nt; i++)
            {
                uint id = r.ReadUInt32(); int w = r.ReadInt32(), h = r.ReadInt32(); Require(w > 0 && w <= 4096 && h > 0 && h <= 4096 && (long)w*h*4 <= r.BaseStream.Length-r.BaseStream.Position, "texture size");
                var tex = Own(new Texture2D(w, h, TextureFormat.RGBA32, true, raw) { name = $"mtx {id:x8}", wrapMode = TextureWrapMode.Repeat, filterMode = FilterMode.Trilinear, anisoLevel = 8 });
                tex.SetPixelData(r.ReadBytes(w * h * 4), 0); tex.Apply(true, true); textures[id] = tex;
            }
            int nm = r.ReadInt32(); Require(nm >= 0 && nm < 100000, "material count");
            var defs = new List<MaterialDef>();
            for (int i = 0; i < nm; i++)
            {
                var d = new MaterialDef { texture = r.ReadUInt32(), blend = r.ReadByte(), cutoff = r.ReadByte(), texAlpha = r.ReadBoolean(), flat = r.ReadBoolean(), fixedAlpha = r.ReadByte(), ignoreVertexAlpha = r.ReadBoolean(), clampU = r.ReadBoolean(), clampV = r.ReadBoolean() };
                d.color = new Color(r.ReadSingle(), r.ReadSingle(), r.ReadSingle(), 1); d.env = r.ReadBoolean(); d.tileU = r.ReadSingle(); d.tileV = r.ReadSingle();
                int layers = r.ReadByte(); Require(layers <= 3, "material layers");
                for (int k = 0; k < layers; k++)
                {
                    var l = new LayerDef { texture = r.ReadUInt32(), blend = r.ReadByte(), fixedAlpha = r.ReadByte(), env = r.ReadBoolean(), ignoreVertexAlpha = r.ReadBoolean(), tileU = r.ReadSingle(), tileV = r.ReadSingle(), uvSet = r.ReadByte(), clampU = r.ReadBoolean(), clampV = r.ReadBoolean() };
                    l.color = new Color(r.ReadSingle(), r.ReadSingle(), r.ReadSingle(), 1); d.layers.Add(l);
                }
                defs.Add(d);
            }
            // A texture a material clamps is a GPU copy with that address mode (textures are shared between materials).
            var clamps = new Dictionary<(Texture2D, bool, bool), Texture2D>();
            Texture2D Clamped(Texture2D t, bool u, bool v)
            {
                if (clamps.TryGetValue((t, u, v), out var c)) return c;
                if (SystemInfo.copyTextureSupport == CopyTextureSupport.None) return clamps[(t, u, v)] = t;
                c = Own(new Texture2D(t.width, t.height, t.format, t.mipmapCount, raw) { name = t.name + " clamped", filterMode = t.filterMode, anisoLevel = t.anisoLevel,
                    wrapModeU = u ? TextureWrapMode.Clamp : TextureWrapMode.Repeat, wrapModeV = v ? TextureWrapMode.Clamp : TextureWrapMode.Repeat });
                Graphics.CopyTexture(t, c); return clamps[(t, u, v)] = c;
            }
            // the goal props: one child per named object (hidden ones inactive until a goal shows them)
            int ng = r.ReadInt32(); Require(ng >= 0 && ng < 10000, "prop count");
            var groups = new Transform[ng];
            for (int g = 0; g < ng; g++)
            {
                string name = r.ReadString(); bool hidden = r.ReadBoolean();
                var go = new GameObject(name); go.transform.SetParent(parent, false); go.SetActive(!hidden); groups[g] = go.transform;
                if (r.ReadBoolean())   // a level object, modelled about its own origin: at its node
                {
                    go.transform.localPosition = new Vector3(r.ReadSingle(), r.ReadSingle(), r.ReadSingle());
                    go.transform.localRotation = new Quaternion(r.ReadSingle(), r.ReadSingle(), r.ReadSingle(), r.ReadSingle());
                    ExperimentalMtxSpinner.Attach(go, name);
                }

            }
            int nd = r.ReadInt32(); Require(nd > 0 && nd < 100000, "draw count");
            for (int di = 0; di < nd; di++)
            {
                int mi = r.ReadInt32(); bool sky = r.ReadBoolean(), billboard = r.ReadBoolean(); int group = r.ReadInt32(), nv = r.ReadInt32(), ni = r.ReadInt32();
                Require(group >= -1 && group < ng, "prop index");
                Require(mi >= 0 && mi < nm && nv > 0 && nv <= 4000000 && ni >= 3 && ni % 3 == 0 && ni <= 12000000, "mesh size");
                Require((long)nv*(billboard?68:48)+(long)ni*4 <= r.BaseStream.Length-r.BaseStream.Position, "truncated mesh");
                var vertices = new Vector3[nv]; var uvs = new Vector2[4][]; for (int u = 0; u < 4; u++) uvs[u] = new Vector2[nv];
                var colors = new Color32[nv]; var origins = billboard ? new Vector3[nv] : null; var offsets = billboard ? new Vector2[nv] : null;
                for (int i = 0; i < nv; i++)
                {
                    vertices[i] = new Vector3(r.ReadSingle(), r.ReadSingle(), r.ReadSingle());
                    for (int u = 0; u < 4; u++) uvs[u][i] = new Vector2(r.ReadSingle(), r.ReadSingle());
                    colors[i] = new Color32(r.ReadByte(), r.ReadByte(), r.ReadByte(), r.ReadByte());
                    if (billboard) { origins[i] = new Vector3(r.ReadSingle(), r.ReadSingle(), r.ReadSingle()); offsets[i] = new Vector2(r.ReadSingle(), r.ReadSingle()); }
                }
                var indices = new int[ni]; for (int i = 0; i < ni; i++) { indices[i] = r.ReadInt32(); Require((uint)indices[i] < (uint)nv, "triangle index"); }
                if (version >= 3)
                {
                    // Root scales placement with the terrain. Cancel that scale only around
                    // each source object's common base pivot, including billboard offsets.
                    float scale = Reflex.Port.RxCore.RxMapScale.Current;
                    for (int i = 0; i < nv; i++)
                    {
                        var anchor = new Vector3(r.ReadSingle(), r.ReadSingle(), r.ReadSingle());
                        float fixedSize = r.ReadSingle();
                        Require(float.IsFinite(anchor.x) && float.IsFinite(anchor.y) && float.IsFinite(anchor.z)
                            && (fixedSize == 0 || fixedSize == 1), "prop anchor");
                        if (fixedSize == 0) continue;
                        vertices[i] = anchor + (vertices[i] - anchor) / scale;
                        if (billboard) { origins[i] = anchor + (origins[i] - anchor) / scale; offsets[i] /= scale; }
                    }
                }
                var mesh = Own(new Mesh { name = $"mtx {mi}{(sky ? " sky" : "")}{(billboard ? " billboard" : "")}", indexFormat = nv > 65535 ? IndexFormat.UInt32 : IndexFormat.UInt16 });
                mesh.vertices = vertices; for (int u = 0; u < 4; u++) mesh.SetUVs(u, uvs[u]); mesh.colors32 = colors;
                if (billboard) { mesh.SetUVs(4, origins); mesh.SetUVs(5, offsets); }
                mesh.triangles = indices; mesh.RecalculateNormals(); mesh.RecalculateBounds();
                if (billboard) { var bnd = mesh.bounds; bnd.Expand(new Vector3(bnd.size.y, 0, bnd.size.y)); mesh.bounds = bnd; }
                mesh.UploadMeshData(!Application.isEditor);   // goal props stay readable (the hoops' glass shatters)
                // A blended pass over an opaque texture whose vertex alpha is painted (clear to solid across the mesh) fades by it:
                // the wet overlays laid over a track (Bayou's puddles, at most ~35%). Uniform vertex alpha (a banner, a sign) stays opaque.
                byte amin = 255, amax = 0; foreach (var c in colors) { if (c.a < amin) amin = c.a; if (c.a > amax) amax = c.a; }
                bool painted = amin < 0x20 && amax >= amin + 16;
                if (!built.TryGetValue((mi, sky, billboard, painted), out var mat)) built[(mi, sky, billboard, painted)] = mat = Own(MakeMaterial(shader, defs[mi], textures, sky, billboard, Clamped, painted, venue));
                var go = new GameObject(mesh.name); go.transform.SetParent(group >= 0 ? groups[group] : parent, false);
                go.AddComponent<MeshFilter>().sharedMesh = mesh; var renderer = go.AddComponent<MeshRenderer>(); renderer.sharedMaterial = mat;
                renderer.shadowCastingMode = ShadowCastingMode.Off; renderer.receiveShadows = false; surfaces++;
                if (sky) skies?.Add(go.transform);
            }
            if(version>=2) ExperimentalMtxAnimation.Read(r,parent.gameObject,groups);
            Require(r.BaseStream.Position == r.BaseStream.Length, "render cache end");
            return (surfaces, textures.Count);
        }

        static Material MakeMaterial(Shader shader, MaterialDef d, Dictionary<uint, Texture2D> textures, bool sky, bool billboard, Func<Texture2D, bool, bool, Texture2D> clamped, bool painted, ExperimentalMtxLighting.Venue venue)
        {
            textures.TryGetValue(d.texture, out var t0);
            if (t0 && (d.clampU || d.clampV)) t0 = clamped(t0, d.clampU, d.clampV);   // the pass's address modes (sky panels clamp)
            var m = new Material(shader) { name = $"MTX {d.texture:x8}", enableInstancing = true };
            if (t0) m.SetTexture("_MainTex", t0);
            // Layers (Neversoft blend modes): 1/2 add, 3/4 subtract, 5/6 blend, 7/8 modulate, 9/10 brighten, 12/13 blend by the
            // previous pass's alpha (inverse); environment passes are sphere-mapped from the view normal.
            for (int k = 0; k < 3; k++)
            {
                var l = k < d.layers.Count ? d.layers[k] : null; int n = k + 1;
                bool ok = l != null && textures.TryGetValue(l.texture, out var lt) && lt && l.blend != 0 && l.blend != 11 && l.blend <= 13;
                m.SetFloat($"_L{n}Mode", ok ? l.blend : 0);
                if (!ok) continue;
                var ltex = textures[l.texture];
                if (l.clampU || l.clampV) ltex = clamped(ltex, l.clampU, l.clampV);   // a decal layer: once where its UVs land
                m.SetTexture($"_L{n}Tex", ltex);
                m.SetVector($"_L{n}Params", new Vector4(l.fixedAlpha / 128f, l.env ? 1 : 0, l.ignoreVertexAlpha ? 1 : 0, l.uvSet));
                m.SetVector($"_L{n}Tile", new Vector4(l.tileU > 0 ? l.tileU : 1, l.tileV > 0 ? l.tileV : 1, 0, 0));
                m.SetColor($"_L{n}Color", l.color);
            }
            bool blended = d.blend == 5 || d.blend == 6, additive = d.blend == 1 || d.blend == 2;
            // A real alpha cutoff cuts the texture out whatever the blend (foliage, tree cards, fences: never sorted).
            bool cutout = d.texAlpha && d.cutoff >= 16;
            // Blended passes without one fade by texture alpha x vertex alpha (blend 6: fixed alpha): decals with alpha, the
            // panorama, and the flat shadow / shade decals. An opaque printed texture (banners, signs) stays opaque: its
            // vertex alpha is not a fade.
            bool fade = blended && !cutout && (d.texAlpha || d.flat || painted && !d.ignoreVertexAlpha);
            // ... and still blends what passes the test (the hardware did both): a rotor's pale blur disc stays a faint blur, a
            // leaf's soft edge stays soft; it writes depth like any cut-out, so it needs no sorting.
            bool cutBlend = cutout && blended;
            m.SetFloat("_BaseBlend", fade || additive || cutBlend ? 1 : 0); m.SetFloat("_Cutoff", cutout ? Mathf.Max(.08f, d.cutoff / 255f) : 0);
            m.SetFloat("_FixedAlpha", d.blend == 6 || d.blend == 2 ? d.fixedAlpha / 128f : 0);
            m.SetFloat("_IgnoreVertexAlpha", d.ignoreVertexAlpha ? 1 : 0);
            // Shadow decals (a flat grey texture faded by vertex alpha) darken the ground: their colour is not doubled.
            m.SetFloat("_ColorScale", fade && d.flat && !d.texAlpha ? 1 : 2);
            m.SetFloat("_SrcBlend", fade || additive || cutBlend ? (float)BlendMode.SrcAlpha : (float)BlendMode.One);
            m.SetFloat("_DstBlend", fade || cutBlend ? (float)BlendMode.OneMinusSrcAlpha : additive ? (float)BlendMode.One : (float)BlendMode.Zero);
            m.SetFloat("_Sky", sky ? 1 : 0); m.SetFloat("_ZWrite", sky || fade || additive ? 0 : 1);
            m.SetFloat("_Billboard", billboard ? 1 : 0);
            m.SetColor("_BaseColor", d.color);
            m.SetVector("_BaseEnv", new Vector4(d.env ? 1 : 0, d.tileU > 0 ? d.tileU : 1, d.tileV > 0 ? d.tileV : 1, 0));
            m.renderQueue = sky ? 1000 : fade || additive ? 3000 : cutout ? 2450 : 2000;
            m.SetFloat("_MtxRaw", venue != null ? 1 : 0);
            if (venue != null)
            {
                // MTX's `lighting` script: world colours toward vc_target by vc_percent, then x lev / 128; the sky x sky / 128
                var world = ExperimentalMtxLighting.WorldTint(venue); var skyTint = ExperimentalMtxLighting.SkyTint(venue);
                m.SetVector("_MtxTint", sky ? new Vector4(skyTint.x, skyTint.y, skyTint.z, 0) : new Vector4(world.x, world.y, world.z, world.w));
                m.SetFloat("_MtxVcPercent", sky ? 0 : skyTint.w);
                // Reflex's fog: none on the sky; additive passes are only dimmed by it (they add no fog colour)
                m.SetFloat("_MtxFog", sky ? 0 : additive ? 2 : 1);
                m.SetFloat("_MtxSkyGamma", sky ? ExperimentalMtxLighting.SkyGamma : 1);
            }
            return m;
        }

    }
}
