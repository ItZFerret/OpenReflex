// Animated textures and the night sky of a level, read at run time from the player's install (WP-8; native/drafts/world_runtime/props.cpp,
// _re/notes/props.md). Partial of ReflexLevelContent so it reaches the level's own loaded materials, textures and instances.
//  - ScrollingTextureEntity (ScrollingTextureRSData, 25 levels: SX light beams, FunFactory light rays, Dover smoke, waterfalls, waves,
//    hill-climb chevrons): the material parameter gTextureOffset (vertex constant c85, the scrolling templates add it to uv0:
//    Template_Animated_UV_nonlit_alpha 050c "add o3.xy, c85, v3") is the entity's +0xaa0. ScrollingTextureEntity::vf19 (00410dd0) per
//    tick: a += dt; a -= |T| once a >= |T|; offset = a / T, with T = "U / V Seconds For Wrap", negated (0x9cd9e8 = -1) when "Reverse U / V"
//    (vf5 00410c70); T = 0 leaves the offset at 0. With the world running the game's own offsets are applied (OriginalProps); without it
//    (front end) ScrollOffset below is the same formula on the elapsed time.
//  - TextureAnimEntity (TextureAnimControllerRSData: FMX_Dover / DoverB FMX_DO_Caustic, 32 frames; FR_Cheyenne's FR_CH_Ripple_AnimTex
//    is in no package, so the game animates nothing there): the texture resource holds every frame and the game picks the frame drawn
//    (texture data +0x18). Resource layout (FMX_DO_Caustic, 90152 bytes): header words 0..9 as a single texture but word 3 = frame
//    count, word 4 = the level count of all frames; frame descriptors of 16 bytes (levels, width, height, D3DFORMAT) from +0x28; then
//    (size, pitch) per level of every frame; then the level data, frame after frame.
//  - Template_Luma_Sky_Nighttime (12 levels: FunFactory, Dover, SpringfieldB, Fort Dodge, KingstonB, Manchester, Springfield, UI_Compound):
//    samplers s0 usr_sampler_star_mask, s1 usr_sampler_blend_texture, s2 usr_sampler_sky_texture (the material's texture slots in that
//    order: the daytime port drew slot 0, the star mask, as the sky); Shaders/ReflexOriginalSkyNight.shader is the port of its programs.
using System;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using UnityEngine.Rendering;

namespace Reflex.Port.Content
{
    public sealed partial class ReflexLevelContent
    {
        /// A scrolling-texture instance: its entity, renderer, and U / V periods as the game keeps them (negative = reversed).
        public sealed class ScrollingInstance { public int entityId; public Renderer renderer; public float periodU, periodV; }

        /// The ScrollingTextureRSData instances built by BuildScenery (main thread, after it).
        public List<ScrollingInstance> ScrollingInstances()
        {
            var list = new List<ScrollingInstance>();
            foreach (var inst in instances)
            {
                if (inst.entityType != "ScrollingTextureRSData" || !inst.transform) continue;
                var r = inst.transform.GetComponent<Renderer>(); if (!r) continue;
                list.Add(new ScrollingInstance { entityId = inst.entityId, renderer = r,
                    periodU = inst.scroll.z != 0 ? -inst.scroll.x : inst.scroll.x, periodV = inst.scroll.w != 0 ? -inst.scroll.y : inst.scroll.y });
            }
            return list;
        }

        /// gTextureOffset of one axis after `seconds` of ticks (ScrollingTextureEntity::vf19 with the accumulator started at 0; float
        /// accumulation per tick is not reproduced, the wrap is). period: seconds for wrap, negative when reversed.
        public static float ScrollOffset(float period, double seconds)
        {
            if (period == 0) return 0;
            double p = Math.Abs(period), a = seconds - Math.Floor(seconds / p) * p;
            return (float)(a / period);
        }

        // ------------------------------------------------------------------------------------------- flipbook textures

        /// Every frame of a multi-frame texture resource (header word 3 > 1) as single textures, Unity row order; null when the
        /// resource has one frame or an unsupported layout.
        internal static List<LevelTexture> ParseFrames(string name, byte[] b)
        {
            if (b == null || b.Length < 56 || Rfx.U32(b, 0) != 0x77ddcc00 || Rfx.I32(b, 4) != 4) return null;
            int frames = Rfx.I32(b, 12), totalLevels = Rfx.I32(b, 16);
            if (frames < 2 || frames > 1024 || totalLevels < frames || Rfx.I32(b, 20) == 2) return null;
            int desc = 40, table = desc + 16 * frames, data = table + 8 * totalLevels;
            if (data > b.Length) return null;
            var list = new List<LevelTexture>(frames);
            int level = 0, p = data;
            for (int f = 0; f < frames; f++)
            {
                int levels = Rfx.I32(b, desc + 16 * f);
                if (levels <= 0 || level + levels > totalLevels) return null;
                int size = 0; for (int l = 0; l < levels; l++) size += Rfx.I32(b, table + 8 * (level + l));
                if (p + size > b.Length) return null;
                // the frame as a one-frame resource (40-byte header, its descriptor, its level table, its data) for ReflexTex.Parse
                var one = new byte[56 + 8 * levels + size];
                Buffer.BlockCopy(b, 0, one, 0, 40);
                BitConverter.GetBytes(1).CopyTo(one, 12); BitConverter.GetBytes(levels).CopyTo(one, 16);
                Buffer.BlockCopy(b, desc + 16 * f, one, 40, 16);
                Buffer.BlockCopy(b, table + 8 * level, one, 56, 8 * levels);
                Buffer.BlockCopy(b, p, one, 56 + 8 * levels, size);
                var t = ReflexTex.Parse(name + "#" + f, one);
                if (t == null) return null;
                list.Add(t);
                level += levels; p += size;
            }
            return list;
        }

        /// A flipbook texture animated by a TextureAnimEntity: the Unity texture of every frame, and the scenery materials (with the
        /// property) whose slot holds it. Main thread, after BuildScenery.
        public sealed class Flipbook
        {
            public string texture; public Texture2D[] frames = new Texture2D[0];
            public readonly List<(Material material, string property)> slots = new List<(Material, string)>();
            public int shown = -1;
            public void Show(int frame)
            {
                if (frames.Length == 0) return;
                frame = ((frame % frames.Length) + frames.Length) % frames.Length;
                if (frame == shown) return;
                shown = frame;
                foreach (var (m, prop) in slots) if (m) m.SetTexture(prop, frames[frame]);
            }
        }

        readonly Dictionary<string, Flipbook> flipbooks = new Dictionary<string, Flipbook>(StringComparer.Ordinal);

        /// The flipbook of `texture` (TextureAnimEntity "TextureName"), built on first use; null when the level's materials do not use it
        /// or it has a single frame.
        public Flipbook FlipbookFor(string texture)
        {
            if (string.IsNullOrEmpty(texture)) return null;
            if (flipbooks.TryGetValue(texture, out var fb)) return fb;
            flipbooks[texture] = null;
            if (!HasAsset(texture, "texture")) return null;
            List<LevelTexture> frames;
            try { frames = ParseFrames(texture, ReadAsset(texture, "texture")); }
            catch (Exception e) { Warn($"flipbook {texture}: {e.Message}"); return null; }
            if (frames == null) return null;
            fb = new Flipbook { texture = texture };
            // the sampler states of the first material slot that names the texture
            Dictionary<int, float> st = null;
            foreach (var lm in sceneMaterials.Values) { int i = lm.textures.IndexOf(texture); if (i >= 0) { st = lm.samplerStates[i]; break; } }
            fb.frames = frames.Select(t =>
            {
                TextureWrapMode Wrap(int key) => st != null && st.TryGetValue(key, out var v) ? (v == 3f || v == 4f ? TextureWrapMode.Clamp : v == 2f ? TextureWrapMode.Mirror : TextureWrapMode.Repeat) : TextureWrapMode.Repeat;
                var tex = ReflexTex.ToTexture(t, Wrap(1), Wrap(2), FilterMode.Bilinear); Track(tex); return tex;
            }).ToArray();
            // the Unity material properties that hold this texture slot (ReflexSceneryMaterial binds by sampler: find the property by the
            // frame-0 texture name the scenery bound, or the property left empty for a slot that failed to load)
            foreach (var kv in sceneMaterials)
            {
                int slot = kv.Value.textures.IndexOf(texture);
                if (slot < 0 || !materialObjects.TryGetValue(kv.Key, out var m) || !m) continue;
                string prop = null;
                foreach (var p in m.GetTexturePropertyNames()) { var t = m.GetTexture(p); if (t && t.name == texture) { prop = p; break; } }
                if (prop == null) prop = slot == 0 ? "_BaseMap" : null;   // the diffuse slot (the single-frame parse refuses the resource)
                if (prop != null) fb.slots.Add((m, prop));
            }
            flipbooks[texture] = fb;
            return fb;
        }

        /// Diagnostics: a hash of each frame's top level (tests), null when the texture is not a flipbook.
        public ulong[] FlipbookFrameHashes(string texture)
        {
            var frames = ParseFrames(texture, ReadAsset(texture, "texture"));
            return frames?.Select(f => { ulong h = 1469598103934665603ul; foreach (var x in f.mips[0]) h = (h ^ x) * 1099511628211ul; return h; }).ToArray();
        }

        // ------------------------------------------------------------------------------------------- night sky

        /// Replaces the sky dome's material by the Template_Luma_Sky_Nighttime port when the level's sky uses it. Main thread, after
        /// BuildScenery. Returns the material (null: the sky is not a night sky).
        public Material BuildNightSky(Shader nightShader)
        {
            if (sky == null || !sky.transform || !nightShader || sky.materials.Length == 0) return null;
            if (!sceneMaterials.TryGetValue(sky.materials[0], out var lm) || lm.pixelShader != "Template_Luma_Sky_Nighttime") return null;
            var mat = new Material(nightShader) { name = lm.name + " (night)", hideFlags = HideFlags.DontSave };
            for (int i = 0; i < lm.textures.Count; i++)
            {
                var t = TextureFor(lm.textures[i], lm.samplerStates[i]); if (!t) continue;
                string s = i < lm.userSamplers.Length ? lm.userSamplers[i] : "";
                // CTAB order s0 star mask, s1 blend, s2 sky (the material lists its textures in the same order)
                string prop = s == "usr_sampler_star_mask" ? "_StarMask" : s == "usr_sampler_blend_texture" ? "_BlendMap" : s == "usr_sampler_sky_texture" ? "_BaseMap"
                            : i == 0 ? "_StarMask" : i == 1 ? "_BlendMap" : "_BaseMap";
                mat.SetTexture(prop, t);
            }
            Vector4 P(string n, Vector4 def) => lm.parameters.TryGetValue(n, out var v) && v != null && v.Length >= 4 ? new Vector4(v[0], v[1], v[2], v[3]) : def;
            // defaults are the program's CTAB defaults
            mat.SetFloat("_Bracket", P("usr_self_bracketing_value", new Vector4(0, 0, 0, 2.1f)).w);
            mat.SetVector("_BlendColor", P("usr_blend_color", new Vector4(0.043137f, 0.05098f, 0.082353f, 1)));
            mat.SetVector("_StarColor", P("usr_star_color", Vector4.one));
            mat.SetVector("_StarR1", P("usr_star_mask_transform_r1", new Vector4(4, 0, 0, 0)));
            mat.SetVector("_StarR2", P("usr_star_mask_transform_r2", new Vector4(0, 3, 0, 0)));
            mat.SetFloat("_SkyZTest", SystemInfo.usesReversedZBuffer ? (float)CompareFunction.GreaterEqual : (float)CompareFunction.LessEqual);
            mat.SetFloat("_Cull", lm.cull == 1 ? (float)CullMode.Off : lm.cull == 2 ? (float)CullMode.Front : (float)CullMode.Back);
            Track(mat);
            var mr = sky.transform.GetComponent<MeshRenderer>();
            if (mr) mr.sharedMaterial = mat;
            return mat;
        }
    }
}
