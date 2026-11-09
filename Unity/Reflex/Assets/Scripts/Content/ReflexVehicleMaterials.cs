// Materials of the player's bike and rider, read at run time from the player's own Reflex install (Rfx.Install over
// Database/): a model binding's material is resolved as the game does (the graphic kit's plastics / seat, the wheel set's rims,
// the gear's skin tone ...: RfxCatalog.Resolve of the Open Unleashed port, MxaRfxBikes / MxaRfxGear), the material BXML gives its
// pixel shader, the textures with their sampler states (D3DSAMP_ADDRESSU/V: WRAP 1, CLAMP 3; MIPFILTER) and the render states
// (D3DRS_CULLMODE 22, ZWRITEENABLE 14, ALPHATESTENABLE 15, ALPHAREF 24, ALPHAFUNC 25, ALPHABLENDENABLE 27, SRC/DESTBLEND 19/20),
// the pixel shader's constant table names each sampler (usr_sampler_diffuse_texture, usr_sampler_combo_map, ..._fork_sticker...,
// usr_sampler_number, s_sampler2 = mud). Our shader is Reflex/Vehicle (Shaders/ReflexVehicle.shader with the ported programs of
// Shaders/ReflexObjectPrograms.hlsl); the binding is Content/ReflexObjectShading.cs. Nothing of the game's data is kept in the project.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEngine;

namespace Reflex.Port.Content
{
    /// One material of a package, parsed (a pass: the pixel shader, the textures in sampler order, the parameters, the render states).
    internal sealed class VehicleMaterialDef
    {
        internal sealed class Tex
        {
            public string name;                  // texture asset name
            public string sampler = "";          // the pixel shader's name of its register
            public int addrU = 1, addrV = 1;     // 1 WRAP, 2 MIRROR, 3 CLAMP
            public int mipFilter = 2;            // D3DTEXF: 1 point, 2 linear
            public int magFilter = 2, minFilter = 2, maxAniso = 1;   // D3DSAMP_MAGFILTER 5 / MINFILTER 6 (3 anisotropic) / MAXANISOTROPY 10
            public float mipBias;                // D3DSAMP_MIPMAPLODBIAS 8
            public Rfx.Asset asset;              // null: the texture is not in the install
            /// A texture made at run time in place of the install's (a .rxmod bike's own maps, Content/RxModBikes.cs): RawTexture returns it.
            public Texture2D external;
        }
        public string db, pkg, name, pixelShader = "";
        public Rfx.Package package;
        public readonly List<Tex> textures = new List<Tex>();
        public readonly Dictionary<string, float[]> parameters = new Dictionary<string, float[]>(StringComparer.Ordinal);
        public int cull = 3, alphaRef, alphaFunc = 8, srcBlend = 5, destBlend = 6;
        public bool zwrite = true, alphaTest, alphaBlend;

        public float[] Param(string n) => parameters.TryGetValue(n, out var v) ? v : null;
        public Tex Sampler(string part) => textures.FirstOrDefault(t => t.sampler.IndexOf(part, StringComparison.OrdinalIgnoreCase) >= 0);
        public string Key => db + "/" + pkg + "/" + name;
    }

    internal sealed class VehicleCatalog
    {
        public readonly Rfx.Install install;
        readonly Dictionary<string, Dictionary<int, string>> samplers = new Dictionary<string, Dictionary<int, string>>(StringComparer.OrdinalIgnoreCase);
        readonly Dictionary<string, VehicleMaterialDef> materials = new Dictionary<string, VehicleMaterialDef>(StringComparer.OrdinalIgnoreCase);
        public VehicleCatalog(Rfx.Install install) { this.install = install; }

        static VehicleCatalog shared;
        static Texture2D clear;
        /// A 1 x 1 transparent texture (a run-time plate nobody has made).
        public static Texture2D ClearTexture
        {
            get
            {
                if (!clear) { clear = new Texture2D(1, 1, TextureFormat.RGBA32, false) { name = "clear", hideFlags = HideFlags.DontSave }; clear.SetPixel(0, 0, new Color(0, 0, 0, 0)); clear.Apply(); }
                return clear;
            }
        }
        public static VehicleCatalog Shared => shared ??= new VehicleCatalog(ReflexInstall.Packages);

        Dictionary<int, string> Samplers(Rfx.Package p, string shader)
        {
            if (string.IsNullOrEmpty(shader)) return new Dictionary<int, string>();
            if (samplers.TryGetValue(shader, out var s)) return s;
            var a = p.Find(shader, "shader");
            if (a == null) foreach (var q in p.database.packages.Values) { a = q.Find(shader, "shader"); if (a != null) break; }
            s = a != null ? Rfx.PixelSamplers(install.Read(a)) : new Dictionary<int, string>();
            samplers[shader] = s;
            return s;
        }

        static Rfx.Asset FindTexture(Rfx.Package p, string tex)
        {
            if (string.IsNullOrEmpty(tex)) return null;
            var a = p.Find(tex, "texture");
            if (a == null) foreach (var q in p.database.packages.Values) { a = q.Find(tex, "texture"); if (a != null) break; }
            return a;
        }

        public VehicleMaterialDef Material(string db, string pkg, string material)
        {
            string key = db + "/" + pkg + "/" + material;
            if (materials.TryGetValue(key, out var def)) return def;
            var p = install.Pkg(db, pkg);
            var a = p?.Find(material, "material");
            if (a == null) { materials[key] = null; return null; }
            var root = Rfx.Bxml(install.Read(a));
            def = new VehicleMaterialDef { db = db, pkg = pkg, name = material, package = p };
            foreach (var n in root.All())
            {
                if (n.tag == "PixelShader" && def.pixelShader.Length == 0) def.pixelShader = n.S("asset") ?? "";
                else if (n.tag == "Texture")
                {
                    var t = new VehicleMaterialDef.Tex { name = n.S("textureName") ?? "" };
                    if (n.Child("XenonSamplerStates")?.value is ArraySegment<byte> ss)
                        for (int i = 0; i + 7 < ss.Count; i += 8)
                        {
                            int k = BitConverter.ToInt32(ss.Array, ss.Offset + i), v = BitConverter.ToInt32(ss.Array, ss.Offset + i + 4);
                            if (k == 1) t.addrU = v; else if (k == 2) t.addrV = v; else if (k == 7) t.mipFilter = v;
                            else if (k == 5) t.magFilter = v; else if (k == 6) t.minFilter = v; else if (k == 10) t.maxAniso = v;
                            else if (k == 8) t.mipBias = BitConverter.ToSingle(ss.Array, ss.Offset + i + 4);
                        }
                    def.textures.Add(t);
                }
                else if (n.tag == "Parameter" && n.S("name") != null) def.parameters[n.S("name")] = n.value as float[] ?? (n.value is float f ? new[] { f, f, f, f } : null);
                else if (n.tag == "XenonStates" && n.value is ArraySegment<byte> st)
                    for (int i = 0; i + 7 < st.Count; i += 8)
                    {
                        int k = BitConverter.ToInt32(st.Array, st.Offset + i), v = BitConverter.ToInt32(st.Array, st.Offset + i + 4);
                        switch (k)
                        {
                            case 22: def.cull = v; break;
                            case 14: def.zwrite = v != 0; break;
                            case 15: def.alphaTest = v != 0; break;
                            case 24: def.alphaRef = v; break;
                            case 25: def.alphaFunc = v; break;
                            case 27: def.alphaBlend = v != 0; break;
                            case 19: def.srcBlend = v; break;
                            case 20: def.destBlend = v; break;
                        }
                    }
            }
            var smp = Samplers(p, def.pixelShader);
            for (int i = 0; i < def.textures.Count; i++)
            {
                if (smp.TryGetValue(i, out var sn)) def.textures[i].sampler = sn;
                def.textures[i].asset = FindTexture(p, def.textures[i].name);
                ReplacePlaceholder(def, def.textures[i]);
            }
            materials[key] = def;
            return def;
        }

        // ------------------------------------------------------------------------------------------- the original's data faults

        /// Maps of the original that hold its tools' DEFAULT placeholder (256 x 256 A8R8G8B8: an orange square reading "DEFAULT") where
        /// the material samples a real map. The game shows the fault too; the port binds the map's normal-resolution twin (the same name,
        /// package and database without "HR_": the same UV layout, as every HR jersey's maps are their twins' at twice the size), and
        /// only while the HR asset is still that placeholder. Database / texture:
        ///   O'Neal Mayhem jersey (RiderDLC01; Black, Blue, Pink "NEON", Red, Yellow): the garage's HR materials
        ///   HR_MXRider_Jersey_ONeal_Mayhem_<colour> (Jersey_FNL) sample HR_MXRider_Jersey_ONeal_Mayhem_SFX (s0: x the cloth's sheen,
        ///   y the mud mask) and HR_MXRider_Jersey_ONeal_Mayhem_Nrml (s5: the normal's xy). Both are the placeholder in all five
        ///   packages: its red (1.0) puts the full grey sheen on the whole jersey and pants, and orange read as a normal (x 1.0,
        ///   y -0.2, no z left) blotches the lighting. The twins are the authored maps, in the formats and sizes of O'Neal Hardwear's
        ///   normal-resolution maps (SFX DXT1 512 x 256, normals G16R16F 1024 x 512).
        static readonly HashSet<string> PlaceholderMaps = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
        {
            "HR_RiderDLC01_jersey/HR_MXRider_Jersey_ONeal_Mayhem_SFX",
            "HR_RiderDLC01_jersey/HR_MXRider_Jersey_ONeal_Mayhem_Nrml",
        };

        void ReplacePlaceholder(VehicleMaterialDef def, VehicleMaterialDef.Tex t)
        {
            var a = t.asset;
            if (a == null || !PlaceholderMaps.Contains(a.package.database.name + "/" + a.name)) return;
            string Lr(string s) => s.StartsWith("HR_", StringComparison.Ordinal) ? s.Substring(3) : s;
            var twin = install.Find(Lr(a.package.database.name), Lr(a.package.name), Lr(a.name), "texture");
            if (twin == null) return;
            var b = install.Read(a);
            if (b.Length < 56 || Rfx.I32(b, 44) != 256 || Rfx.I32(b, 48) != 256 || Rfx.I32(b, 52) != 21) return;   // (D3DFMT_A8R8G8B8 = 21)
            t.asset = twin; t.name = twin.name;
            Debug.Log($"Reflex material {def.Key}: {a.name} is the original's DEFAULT placeholder; its twin {twin.package.database.name}/{twin.package.name}/{twin.name} is bound");
        }

        public IEnumerable<string> MaterialNames(string db, string pkg)
        {
            var p = install.Pkg(db, pkg);
            return p == null ? Enumerable.Empty<string>() : p.OfType("material").Select(a => a.name);
        }

        /// The game's material override (the graphic kit for "MX_Plstc", the skin tone for "MXRider_Skin" ...): among the
        /// materials of the chosen packages and the model's own, the longest name equal to the binding's or starting with it +
        /// "_" (case and the HR_ prefix ignored; a chosen package wins a tie over the model's own); none: the binding's name
        /// loses its last "_" part and is tried again (down to two parts).
        public VehicleMaterialDef Resolve(string binding, (string db, string pkg) own, IEnumerable<(string db, string pkg)> chosen)
        {
            var pool = new List<((string db, string pkg) src, string name, bool chosen)>();
            foreach (var c in chosen) foreach (string n in MaterialNames(c.db, c.pkg)) pool.Add((c, n, true));
            foreach (string n in MaterialNames(own.db, own.pkg)) pool.Add((own, n, false));
            string Norm(string s) => s.StartsWith("HR_", StringComparison.OrdinalIgnoreCase) ? s.Substring(3) : s;
            string name = Norm(binding);
            while (true)
            {
                (string db, string pkg) bestSrc = default; string best = null; bool bestChosen = false;
                foreach (var x in pool)
                {
                    string n = Norm(x.name);
                    if (!(n.Equals(name, StringComparison.OrdinalIgnoreCase) || n.StartsWith(name + "_", StringComparison.OrdinalIgnoreCase))) continue;
                    if (best == null || n.Length > Norm(best).Length || n.Length == Norm(best).Length && x.chosen && !bestChosen) { best = x.name; bestSrc = x.src; bestChosen = x.chosen; }
                }
                if (best != null) return Material(bestSrc.db, bestSrc.pkg, best);
                int u = name.LastIndexOf('_');
                if (u < 0 || name.Split('_').Length <= 2) return null;
                name = name.Substring(0, u);
            }
        }

        // ------------------------------------------------------------------------------------------- textures

        static readonly Dictionary<string, Texture2D> textureCache = new Dictionary<string, Texture2D>(StringComparer.OrdinalIgnoreCase);
        static readonly HashSet<string> textureFailed = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        /// Frees the decoded textures (the run-time owner is gone).
        public static void ReleaseTextures()
        {
            foreach (var t in textureCache.Values) if (t) UnityEngine.Object.Destroy(t);
            textureCache.Clear(); textureFailed.Clear();
            lock (rawCache) { foreach (var t in rawCache.Values) if (t) UnityEngine.Object.Destroy(t); rawCache.Clear(); rawFailed.Clear(); }
        }

        /// The texture a material samples, decoded from the install once per wrap / filter / colour-space combination
        /// (block data straight to the GPU). linear: data maps (normal, combo / SFX), not colour.
        public Texture2D Texture(VehicleMaterialDef.Tex t, bool linear)
        {
            if (t?.asset == null) return null;
            TextureWrapMode Wrap(int a) => a == 3 ? TextureWrapMode.Clamp : a == 2 ? TextureWrapMode.Mirror : TextureWrapMode.Repeat;
            string key = t.asset.package.database.name + "/" + t.asset.package.name + "/" + t.asset.name + "|" + t.addrU + t.addrV + t.mipFilter + (linear ? "L" : "S");
            if (textureCache.TryGetValue(key, out var cached) && cached) return cached;
            if (textureFailed.Contains(key)) return null;
            try
            {
                var tex = Rfx.ToTexture(Decode(install.Read(t.asset), linear), t.asset.name);
                if (!tex) throw new InvalidDataException("an unsupported texture format");
                tex.wrapModeU = Wrap(t.addrU); tex.wrapModeV = Wrap(t.addrV);
                tex.filterMode = t.mipFilter >= 2 ? FilterMode.Trilinear : FilterMode.Bilinear;   // MIPFILTER point / linear
                textureCache[key] = tex;
                return tex;
            }
            catch (Exception e) when (e is IOException || e is InvalidDataException)
            {
                textureFailed.Add(key);
                Debug.LogWarning($"Reflex texture {key}: {e.Message}");
                return null;
            }
        }

        /// Rfx.ParseTexture, plus the kits' "Numbers" placeholder (its header says 128 x 128 but mip 0 holds 128 x 64 blocks).
        static Rfx.TextureData Decode(byte[] b, bool linear)
        {
            var td = Rfx.ParseTexture(b, linear);
            if (td != null) return td;
            Rfx.Check(b.Length >= 64 && Rfx.U32(b, 0) == 0x77ddcc00, "texture header");
            int levels = Rfx.I32(b, 40), w = Rfx.I32(b, 44), h = Rfx.I32(b, 48), fmt = Rfx.I32(b, 52);
            int size = Rfx.I32(b, 56);
            if (fmt != 0x31545844 || w % 4 != 0 || size % (w / 4 * 8) != 0) return null;
            h = size / (w / 4 * 8) * 4;   // the rows mip 0 really holds
            int at = 56 + levels * 8; if (at + size > b.Length) return null;
            var mip = new byte[size]; Buffer.BlockCopy(b, at, mip, 0, size);
            // flip the block rows (Unity's row order); a DXT1 block's four pixel rows too
            int bw = w / 4, bh = h / 4;
            var flipped = new byte[size];
            for (int y = 0; y < bh; y++)
                for (int x = 0; x < bw; x++)
                {
                    int s = (y * bw + x) * 8, d = ((bh - 1 - y) * bw + x) * 8;
                    Buffer.BlockCopy(mip, s, flipped, d, 4);
                    for (int r = 0; r < 4; r++) flipped[d + 4 + r] = mip[s + 4 + 3 - r];
                }
            var t = new Rfx.TextureData { width = w, height = h, raw = true, format = TextureFormat.DXT1, linear = linear };
            t.mips.Add(flipped);
            return t;
        }

        // ------------------------------------------------------------------------------------------- Unity materials

        /// A Forks part's sticker (a texture package with no material: "MX_Fork_RG3" for the frame material's "MX_Fork"): the
        /// package's texture equal to the frame's sticker or starting with it + "_" (HR_ and case ignored), else the frame's own.
        internal static VehicleMaterialDef.Tex Sticker(VehicleMaterialDef.Tex t, Rfx.Package stickers)
        {
            if (stickers == null || string.IsNullOrEmpty(t.name)) return t;
            string Norm(string s) => s.StartsWith("HR_", StringComparison.OrdinalIgnoreCase) ? s.Substring(3) : s;
            string want = Norm(t.name);
            var a = stickers.OfType("texture").FirstOrDefault(x => Norm(x.name).Equals(want, StringComparison.OrdinalIgnoreCase))
                 ?? stickers.OfType("texture").FirstOrDefault(x => Norm(x.name).StartsWith(want + "_", StringComparison.OrdinalIgnoreCase));
            return a == null ? t : new VehicleMaterialDef.Tex { name = a.name, sampler = t.sampler, addrU = t.addrU, addrV = t.addrV, mipFilter = t.mipFilter,
                                                                 magFilter = t.magFilter, minFilter = t.minFilter, maxAniso = t.maxAniso, mipBias = t.mipBias, asset = a };
        }

        /// The Reflex/Vehicle material for a game material, bound as the game binds it (ReflexObjectShading.Build; null def: a plain grey).
        /// stickers: the bike's Forks texture package. tint: unused (the original's colours are the material's constants).
        public UnityEngine.Material Build(VehicleMaterialDef d, string label, Color? tint = null, Rfx.Package stickers = null)
        {
            using var perf = Reflex.Port.Platform.ReflexMenuPerf.Measure("material made");   // (its textures read and decoded with it)
            return ReflexObjectShading.Build(this, d, label, stickers);
        }

        // ------------------------------------------------------------------------------------------- raw textures (Reflex/Vehicle)

        static readonly Dictionary<string, Texture> rawCache = new Dictionary<string, Texture>(StringComparer.OrdinalIgnoreCase);
        static readonly HashSet<string> rawFailed = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        static TextureWrapMode Wrap(int a) => a == 3 || a == 4 ? TextureWrapMode.Clamp : a == 2 ? TextureWrapMode.Mirror : a == 5 ? TextureWrapMode.MirrorOnce : TextureWrapMode.Repeat;

        /// A material texture as the original device samples it: the file's own data without sRGB conversion (ReflexTex: DXT blocks,
        /// A8R8G8B8, G16R16F normal maps as RGHalf, FP16), with the material's sampler states (address U / V, min / mag / mip filter,
        /// anisotropy, LOD bias). Cached per asset and states.
        public Texture2D RawTexture(VehicleMaterialDef.Tex t)
        {
            if (t != null && t.external) return t.external;
            if (t?.asset == null) return null;
            string key = t.asset.package.database.name + "/" + t.asset.package.name + "/" + t.asset.name + "|" + t.addrU + "," + t.addrV + "," + t.magFilter + "," + t.minFilter + "," + t.mipFilter + "," + t.maxAniso + "," + t.mipBias;
            lock (rawCache) { if (rawCache.TryGetValue(key, out var c) && c is Texture2D ct && ct) return ct; if (rawFailed.Contains(key)) return null; }
            try
            {
                var bytes = install.Read(t.asset);
                LevelTexture lt;
                try { lt = ReflexTex.Parse(t.asset.name, bytes); }
                catch (ArgumentException) { lt = ReflexTex.Parse(t.asset.name, PadShortLevels(bytes)); }
                if (lt == null || lt.cube) throw new InvalidDataException("an unsupported texture format");
                var filter = t.mipFilter >= 2 ? FilterMode.Trilinear : (t.minFilter <= 1 && t.magFilter <= 1) ? FilterMode.Point : FilterMode.Bilinear;
                var tex = ReflexTex.ToTexture(lt, Wrap(t.addrU), Wrap(t.addrV), filter, t.minFilter == 3 ? Math.Max(1, t.maxAniso) : 1, t.asset.package.database.name);
                if (!tex) throw new InvalidDataException("no texture");
                tex.mipMapBias = t.mipBias;
                lock (rawCache) rawCache[key] = tex;
                return tex;
            }
            catch (Exception e) when (e is IOException || e is InvalidDataException || e is ArgumentException)   // (a malformed resource: the kits' "Numbers" placeholder)
            {
                lock (rawCache) rawFailed.Add(key);
                Debug.LogWarning($"Reflex texture {key}: {e.Message}");
                return null;
            }
        }

        /// A DXT resource whose levels hold fewer block rows than its size says (the kits' "Numbers" placeholder, ATV_Exh_FMF_Factory4_Combo:
        /// 128 x 128 with 128 x 64 of blocks): the device's texture is the declared size with the file's rows at its top and the rest
        /// zero, as the resource is copied into it.
        static byte[] PadShortLevels(byte[] b)
        {
            int levels = Rfx.I32(b, 40), w = Rfx.I32(b, 44), h = Rfx.I32(b, 48), fmt = Rfx.I32(b, 52), entries = Math.Max(levels, Rfx.I32(b, 16));
            int bs = fmt == 0x31545844 ? 8 : fmt == 0x35545844 || fmt == 0x33545844 ? 16 : 0;
            if (bs == 0 || Rfx.I32(b, 20) == 2) return b;
            var o = new List<byte>(); var head = new byte[56 + levels * 8]; Buffer.BlockCopy(b, 0, head, 0, Math.Min(head.Length, b.Length));
            BitConverter.GetBytes(levels).CopyTo(head, 16);
            int p = 56 + entries * 8; var data = new List<byte>();
            for (int l = 0; l < levels; l++)
            {
                int lw = Math.Max(1, w >> l), lh = Math.Max(1, h >> l), want = ((lw + 3) / 4) * ((lh + 3) / 4) * bs, have = Rfx.I32(b, 56 + l * 8);
                for (int i = 0; i < want; i++) data.Add(i < have && p + i < b.Length ? b[p + i] : (byte)0);
                BitConverter.GetBytes(want).CopyTo(head, 56 + l * 8); p += have;
            }
            o.AddRange(head); o.AddRange(data);
            return o.ToArray();
        }

        /// A material's cube texture (a dcl_cube sampler of the program), raw.
        public Cubemap RawCube(VehicleMaterialDef.Tex t)
        {
            if (t?.asset == null) return null;
            string key = t.asset.package.database.name + "/" + t.asset.package.name + "/" + t.asset.name + "|cube";
            lock (rawCache) { if (rawCache.TryGetValue(key, out var c) && c is Cubemap cc && cc) return cc; if (rawFailed.Contains(key)) return null; }
            try
            {
                var lt = ReflexTex.Parse(t.asset.name, install.Read(t.asset));
                var cube = lt != null && lt.cube ? ReflexTex.ToCubemap(lt) : null;
                if (!cube) throw new InvalidDataException("not a supported cube texture");
                cube.mipMapBias = t.mipBias;
                lock (rawCache) rawCache[key] = cube;
                return cube;
            }
            catch (Exception e) when (e is IOException || e is InvalidDataException || e is ArgumentException)   // (a malformed resource: the kits' "Numbers" placeholder)
            {
                lock (rawCache) rawFailed.Add(key);
                Debug.LogWarning($"Reflex texture {key}: {e.Message}");
                return null;
            }
        }
    }

    /// Reflex/Vehicle, the ported object programs (Shaders/ReflexVehicle.shader, ReflexObjectShading).
    public static class ReflexVehicleShader
    {
        public static Shader Override;   // assigned by the scene (a serialized reference keeps it in player builds)
        static Shader found;
        public static Shader Get()
        {
            if (Override) return Override;
            if (!found) found = Shader.Find("Reflex/Vehicle");
            if (!found) throw new InvalidOperationException("Shader 'Reflex/Vehicle' is missing (Assets/Shaders/ReflexVehicle.shader).");
            return found;
        }
    }
}
