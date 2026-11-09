// Mud build-up on the vehicles and riders of MX vs ATV Reflex, 1:1 with the running game (_re/notes/mud.md, _re/names/79_mud.tsv).
// The game: one mud record per vehicle in a static table (0xa96d90, 12 x 0x40: usr_mud_amount +0x00, usr_mud_wetness +0x10,
// usr_mud_color +0x20, owner V +0x30), allocated when the rider is inserted (MudAndNumberCallback::vf0 0054a2e0) and bound as the
// shader-constant overrides of the vehicle model and its rider ([V+0x1a0]); MXGame::Update runs MudRecord_Update (00408c50) every
// 0.1 s: speed >= MinimumSpeedForMudAccumulation (5 ft/s), on the ground, inside the terrain: amount += MudManager.MudRate[the
// terrain type under the vehicle] x dt (Clean%i types negative), clamped to 0..1; wetness dries at 1 / MudSecondsToDry (30 s) a
// second; colour = the level MudManager's MudR/G/B/A / 255, darkened by (1 - (1 - MudColorAttenuationInWater) x wetness) while wet.
// A restart clears it (Activity::vf18 00557de0, BaseDriver::vf3 00508f90). reflex_world_mud exports a slot's record (mud.cpp).
// The pixel shaders (every *_FNL vehicle / gear shader that declares usr_mud_amount; the term is read from the shader bytecode here,
// so each material gets its own shader's numbers):
//   mud    = saturate(mask.<channel> x scale + usr_mud_amount.w - 1)   mask: the SFX / combo map (channel y, ATV frame x, A-arms /
//                                                                      bumpers / wheels z, machine bodies w; goggles / neck brace x 1.2)
//   albedo = lerp(albedo, usr_mud_color.rgb x noise(uv x tile).rgb, mud)   noise: Mud_Noise2 (tile 15; jersey 25, tyres 2, rims 1)
//   the reflection / specular terms x (1 - mud) (the gear shaders with usr_mud_wetness: x lerp(gloss, usr_mud_wetness.w, mud))
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using UnityEngine;

namespace Reflex.Port.Content
{
    /// reflex_world_mud (reflex_world.h ReflexMudState): a slot's mud record as the game binds it to its shaders.
    [StructLayout(LayoutKind.Sequential)]
    public struct ReflexMudState
    {
        public int slot, record;          // record: index in the game's table (-1: none)
        public uint vehicle, rider;       // V = [P+0x1ac], its rider [V+0x1a0]
        public Vector4 amount;            // usr_mud_amount (four equal lanes, 0..1)
        public Vector4 wetness;           // usr_mud_wetness
        public Vector4 color;             // usr_mud_color (gamma-space RGBA, as the game's constants)
        public bool Valid => record >= 0;
    }

    public static class ReflexMudNative
    {
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_mud__fn(IntPtr world, int slot, ref ReflexMudState state);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_mud__fn> reflex_world_mud__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_mud__fn>("reflex_world_mud");
        public static int reflex_world_mud(IntPtr world, int slot, ref ReflexMudState state) => reflex_world_mud__p.Get()(world, slot, ref state);
        static bool missing;
        /// The local player's mud (into[0]) and each slot's (into[1 + i]); returns how many were written (0 without the export).
        public static int Collect(IntPtr world, int[] slots, ReflexMudState[] into)
        {
            if (missing || into == null || into.Length == 0) return 0;
            try
            {
                int n = 0;
                into[n] = default; if (reflex_world_mud(world, -1, ref into[n]) != 0) into[n].record = -1; n++;
                if (slots != null)
                    for (int i = 0; i < slots.Length && n < into.Length; i++, n++)
                    {
                        into[n] = default;
                        if (reflex_world_mud(world, slots[i], ref into[n]) != 0) { into[n].record = -1; into[n].slot = slots[i]; }
                    }
                return n;
            }
            catch (EntryPointNotFoundException) { missing = true; return 0; }
        }
    }

    /// The mud term of one pixel shader, read from its bytecode (SM3 tokens; the CTAB names the constants).
    public sealed class MudShaderInfo
    {
        public string shader;
        public int amountReg = -1, colorReg = -1, wetnessReg = -1;   // c registers of usr_mud_amount / usr_mud_color / usr_mud_wetness
        public int noiseSampler = -1; public float tile = 1;          // the noise multiplied by usr_mud_color, sampled at uv x tile
        public int maskSampler = -1, maskChannel = 1; public float maskScale = 1;   // saturate(mask.channel x scale + amount.w - 1)
        public bool Usable => amountReg >= 0 && colorReg >= 0 && noiseSampler >= 0 && maskSampler >= 0;
        public override string ToString() =>
            $"{shader}\tc{amountReg}\tc{colorReg}\t{(wetnessReg >= 0 ? "c" + wetnessReg : "-")}\t{(noiseSampler >= 0 ? "s" + noiseSampler : "-")}\t{(noiseSampler >= 0 ? tile.ToString("0.0###", System.Globalization.CultureInfo.InvariantCulture) : "-")}" +
            $"\t{(maskSampler >= 0 ? "s" + maskSampler : "-")}\t{(maskSampler >= 0 ? "xyzw"[maskChannel].ToString() : "-")}\t{(maskSampler >= 0 ? maskScale.ToString("0.0###", System.Globalization.CultureInfo.InvariantCulture) : "-")}";
    }

    public static class ReflexMud
    {
        // ------------------------------------------------------------------------------------------- shader analysis

        /// The pixel shader of a shader resource (u32 2, u32 entries; entry: u32 kind (1 pixel), u32 name length, name, u32 size,
        /// SM3 tokens) analysed for its mud term; null when it declares no usr_mud_amount.
        public static MudShaderInfo Analyze(byte[] b, string name = null)
        {
            if (b == null || b.Length < 8 || BitConverter.ToInt32(b, 0) != 2) return null;
            int count = BitConverter.ToInt32(b, 4), p = 8;
            for (int e = 0; e < count && p + 8 <= b.Length; e++)
            {
                int kind = BitConverter.ToInt32(b, p), len = BitConverter.ToInt32(b, p + 4); p += 8 + len;
                if (p + 4 > b.Length) return null;
                int size = BitConverter.ToInt32(b, p); p += 4;
                if (size <= 0 || p + size > b.Length) return null;
                int code = p; p += size;
                if (kind == 1) return AnalyzeProgram(b, code, size, name);
            }
            return null;
        }

        // A component's known value: a texture fetch (sampler, channel, the coordinate's tile), a literal, or a scaled coordinate.
        struct Val { public byte kind; public int sampler, channel; public float value, tile; }   // kind 1 tex, 2 literal, 3 tiled uv, 4 input
        const byte Tex = 1, Lit = 2, Uv = 3, Input = 4;

        static MudShaderInfo AnalyzeProgram(byte[] b, int code, int size, string name)
        {
            int end = code + size;
            if (size < 8 || BitConverter.ToUInt32(b, code) != 0xffff0300) return null;
            var info = new MudShaderInfo { shader = name };
            var defs = new Dictionary<int, float[]>();
            var temps = new Val[32 * 4];
            bool maskFound = false, noiseFound = false;
            for (int q = code + 4; q + 4 <= end;)
            {
                uint tok = BitConverter.ToUInt32(b, q);
                int op = (int)(tok & 0xffff);
                if (op == 0xffff) break;
                if (op == 0xfffe)
                {
                    int n = (int)((tok >> 16) & 0x7fff), body = q + 4;
                    if (n >= 7 && body + 4 <= end && b[body] == 'C' && b[body + 1] == 'T' && b[body + 2] == 'A' && b[body + 3] == 'B') Constants(b, body + 4, body + n * 4, info);
                    q = body + n * 4; continue;
                }
                int args = (int)((tok >> 24) & 0xf), a0 = q + 4; q = a0 + args * 4;
                if (q > end) break;
                if (info.amountReg < 0) continue;   // no CTAB mud constant: nothing to find
                if (op == 0x51) { defs[Reg(BitConverter.ToUInt32(b, a0))] = new[] { BitConverter.ToSingle(b, a0 + 4), BitConverter.ToSingle(b, a0 + 8), BitConverter.ToSingle(b, a0 + 12), BitConverter.ToSingle(b, a0 + 16) }; continue; }
                if (op == 0x1f || op == 0x30 || op == 0x2f || args == 0) continue;   // dcl, defi, defb, flow control
                uint dst = BitConverter.ToUInt32(b, a0);
                // sources (a relative source carries an address token)
                var src = new List<uint>(3);
                for (int k = a0 + 4; k < q; k += 4) { uint s = BitConverter.ToUInt32(b, k); src.Add(s); if ((s & 0x2000) != 0) k += 4; }
                if (Type(dst) == 2 || Type(dst) == 7 || Type(dst) == 14) continue;
                int dreg = Reg(dst), dmask = (int)((dst >> 16) & 0xf);
                bool isTemp = Type(dst) == 0 && dreg < 32;
                Val Get(uint s, int lane)
                {
                    int t = Type(s), r = Reg(s), c = (int)((s >> (16 + 2 * lane)) & 3);
                    if (t == 2) return defs.TryGetValue(r, out var d) ? new Val { kind = Lit, value = d[c] } : default;
                    if (t == 1) return new Val { kind = Input };
                    if (t == 0 && r < 32) return temps[r * 4 + c];
                    return default;
                }
                bool IsConst(uint s, int reg, int lane) => Type(s) == 2 && Reg(s) == reg && (lane < 0 || ((s >> 16) & 0xff) == 0xff);   // .w broadcast
                int first = 0; while (first < 4 && (dmask & (1 << first)) == 0) first++;
                var result = new Val[4]; bool any = false;
                if (op == 0x42 && src.Count >= 2)   // texld / texldb / texldp
                {
                    var c0 = Get(src[0], 0);
                    float tile = c0.kind == Input ? 1 : c0.kind == Uv ? c0.tile : 1;
                    for (int i = 0; i < 4; i++) if ((dmask & (1 << i)) != 0) { result[i] = new Val { kind = Tex, sampler = Reg(src[1]), channel = i, tile = tile }; any = true; }
                }
                else if (op == 5 && src.Count == 2)   // mul: literal x input = a tiled coordinate
                    for (int i = 0; i < 4; i++)
                    {
                        if ((dmask & (1 << i)) == 0) continue;
                        Val x = Get(src[0], i), y = Get(src[1], i);
                        if (x.kind == Lit && y.kind == Input) { result[i] = new Val { kind = Uv, tile = x.value }; any = true; }
                        else if (y.kind == Lit && x.kind == Input) { result[i] = new Val { kind = Uv, tile = y.value }; any = true; }
                    }
                else if (op == 2 && src.Count == 2 && Type(src[0]) == 1 && Type(src[1]) == 1 && Reg(src[0]) == Reg(src[1]))   // add v, v: uv x 2
                    for (int i = 0; i < 4; i++) { if ((dmask & (1 << i)) != 0) { result[i] = new Val { kind = Uv, tile = 2 }; any = true; } }
                else if (op == 1 && src.Count == 1 && ((src[0] >> 24) & 0xf) == 0)   // mov of a literal
                    for (int i = 0; i < 4; i++) { if ((dmask & (1 << i)) == 0) continue; var x = Get(src[0], i); if (x.kind == Lit) { result[i] = x; any = true; } }
                // the mask: add rD, tex.channel, c(amount).w   /   mad rD, tex.channel, literal, c(amount).w
                if (!maskFound && first < 4 && src.Count >= 2 && IsConst(src[src.Count - 1], info.amountReg, 3))
                {
                    if (op == 2) { var x = Get(src[0], first); if (x.kind == Tex) { info.maskSampler = x.sampler; info.maskChannel = x.channel; info.maskScale = 1; maskFound = true; } }
                    else if (op == 4)
                    {
                        Val x = Get(src[0], first), y = Get(src[1], first);
                        if (x.kind == Tex && y.kind == Lit) { info.maskSampler = x.sampler; info.maskChannel = x.channel; info.maskScale = y.value; maskFound = true; }
                        else if (y.kind == Tex && x.kind == Lit) { info.maskSampler = y.sampler; info.maskChannel = y.channel; info.maskScale = x.value; maskFound = true; }
                    }
                }
                // the noise: the first fetched operand of the instruction that reads usr_mud_color
                if (!noiseFound && info.colorReg >= 0 && src.Exists(s => Type(s) == 2 && Reg(s) == info.colorReg))
                    foreach (var s in src)
                    {
                        if (Type(s) != 0) continue;
                        var x = Get(s, 0);
                        if (x.kind == Tex) { info.noiseSampler = x.sampler; info.tile = x.tile; noiseFound = true; break; }
                    }
                if (isTemp) for (int i = 0; i < 4; i++) if ((dmask & (1 << i)) != 0) temps[dreg * 4 + i] = any ? result[i] : default;
            }
            return info.amountReg >= 0 ? info : null;
        }
        static int Type(uint t) => (int)(((t >> 28) & 7) | ((t >> 8) & 0x18));
        static int Reg(uint t) => (int)(t & 0x7ff);

        static void Constants(byte[] b, int t, int end, MudShaderInfo info)
        {
            if (t + 20 > end) return;
            int consts = BitConverter.ToInt32(b, t + 12), at = BitConverter.ToInt32(b, t + 16);
            for (int i = 0; i < consts; i++)
            {
                int ci = t + at + i * 20; if (ci + 20 > end) return;
                int nameAt = BitConverter.ToInt32(b, ci), set = BitConverter.ToUInt16(b, ci + 4), reg = BitConverter.ToUInt16(b, ci + 6);
                if (set != 2) continue;
                int s = t + nameAt, e = s; while (e < end && b[e] != 0) e++;
                switch (System.Text.Encoding.ASCII.GetString(b, s, e - s))
                {
                    case "usr_mud_amount": info.amountReg = reg; break;
                    case "usr_mud_color": info.colorReg = reg; break;
                    case "usr_mud_wetness": info.wetnessReg = reg; break;
                }
            }
        }

        /// Every pixel shader of the install's databases whose name starts with one of the prefixes, analysed (by shader name, the
        /// first package holding it; null entries: no usr_mud_amount or not readable). Diagnostics / tests.
        public static Dictionary<string, MudShaderInfo> AnalyzeInstall(params string[] prefixes)
        {
            var install = ReflexInstall.Packages; var all = new Dictionary<string, MudShaderInfo>(StringComparer.Ordinal);
            var files = RxFiles.GetFiles(install.dir, "*.dx9.database"); Array.Sort(files, StringComparer.Ordinal);
            foreach (var file in files)
            {
                string db = System.IO.Path.GetFileName(file); db = db.Substring(0, db.Length - ".dx9.database".Length);
                if (prefixes.Length > 0 && Array.TrueForAll(prefixes, x => !db.StartsWith(x, StringComparison.Ordinal))) continue;
                var d = install.Db(db); if (d == null) continue;
                foreach (var p in d.packages.Values)
                    foreach (var a in p.OfType("shader"))
                    {
                        if (all.ContainsKey(a.name)) continue;
                        MudShaderInfo s = null;
                        try { s = Analyze(install.Read(a), a.name); } catch (Exception e) when (e is System.IO.IOException || e is System.IO.InvalidDataException || e is ArgumentException) { }
                        all[a.name] = s;
                    }
            }
            return all;
        }

        static readonly Dictionary<string, MudShaderInfo> shaders = new Dictionary<string, MudShaderInfo>(StringComparer.OrdinalIgnoreCase);
        /// A material's pixel shader's mud term (cached by shader name), found as VehicleCatalog finds the shader (its package, then the database's).
        internal static MudShaderInfo ShaderOf(VehicleCatalog cat, VehicleMaterialDef d)
        {
            if (d == null || string.IsNullOrEmpty(d.pixelShader)) return null;
            lock (shaders)
            {
                if (shaders.TryGetValue(d.pixelShader, out var s)) return s;
                var a = d.package?.Find(d.pixelShader, "shader");
                if (a == null && d.package != null) foreach (var q in d.package.database.packages.Values) { a = q.Find(d.pixelShader, "shader"); if (a != null) break; }
                try { s = a != null ? Analyze(cat.install.Read(a), d.pixelShader) : null; }
                catch (Exception e) when (e is System.IO.IOException || e is System.IO.InvalidDataException) { s = null; }
                shaders[d.pixelShader] = s;
                return s;
            }
        }

        // ------------------------------------------------------------------------------------------- materials

        // (Reflex/Vehicle runs each material's own pixel program, mud term included (ReflexObjectShading): the record's constants are all
        // a material needs - Apply below; the analysis above stays as the reference of what each program does with them.)

        /// The vehicle's and its rider's materials get the slot's record (every frame; only changes are written).
        /// Every Apply with a valid record (the OPEN REFLEX replay records each rider's mud: Reflex.Port.Replay.ReflexReplay).
        public static event Action<ReflexBikeModel, ReflexRiderModel, ReflexMudState> Applied;
        public static void Apply(ReflexBikeModel bike, ReflexRiderModel rider, in ReflexMudState s)
        {
            if (!s.Valid) return;
            ReflexDirt.Apply(bike, rider, s);   // MUD BUILDUP (OPTIONS > VIDEO): the dirt of this vehicle follows its record
            Applied?.Invoke(bike, rider, s);
            if (bike != null)
            {
                Apply(bike.chassis, s);
                if (bike.frontWheel) Apply(bike.frontWheel.gameObject, s);
                if (bike.rearWheel) Apply(bike.rearWheel.gameObject, s);
            }
            if (rider != null) Apply(rider.root, s);
        }

        /// The record of a slot among the collected ones (index 0 is the local player's).
        public static bool Find(ReflexMudState[] states, int count, int slot, out ReflexMudState s)
        {
            for (int i = 1; i < count && states != null && i < states.Length; i++) if (states[i].slot == slot && states[i].Valid) { s = states[i]; return true; }
            s = default; return false;
        }

        static void Apply(GameObject root, in ReflexMudState s)
        {
            if (!root) return;
            if (!root.TryGetComponent<ReflexMudMaterials>(out var held)) { held = root.AddComponent<ReflexMudMaterials>(); held.hideFlags = HideFlags.DontSave; }
            held.Set(s);
        }
    }

    /// The mud materials under a model root (collected once) and the record last written to them.
    [DisallowMultipleComponent]
    public sealed class ReflexMudMaterials : MonoBehaviour
    {
        readonly List<Material> materials = new List<Material>();
        bool collected; Vector4 amount = new Vector4(-1, -1, -1, -1), wetness, color; int version = -1;
        // the record's three constants, by their CTAB names (Reflex/Vehicle runs each material's own program: ReflexObjectShading)
        static readonly int MudAmount = Shader.PropertyToID("usr_mud_amount"), MudColor = Shader.PropertyToID("usr_mud_color"), MudWetness = Shader.PropertyToID("usr_mud_wetness");
        public int Count => materials.Count;
        public Vector4 Amount => amount;

        public void Set(in ReflexMudState s)
        {
            if (!collected)
            {
                collected = true;
                foreach (var r in GetComponentsInChildren<Renderer>(true))
                    foreach (var m in r.sharedMaterials)
                        if (m && m.HasProperty(MudAmount) && !materials.Contains(m)) materials.Add(m);
            }
            if (s.amount == amount && s.wetness == wetness && s.color == color && version == ReflexDirt.Version) return;
            amount = s.amount; wetness = s.wetness; color = s.color; version = ReflexDirt.Version;
            Write();
        }

        /// MUD BUILDUP (or the look) switched: the constants again (the original's mud term is replaced by the new dirt, or off with MUD BUILDUP OFF: ReflexDirt).
        public void Refresh() { if (collected && version != -1) { version = ReflexDirt.Version; Write(); } }

        void Write()
        {
            // the game's own constants, raw (the original's programs take them as they are); a zero amount while the new dirt replaces them or
            // MUD BUILDUP is OFF
            var a = ReflexDirt.HidesOriginalMud ? Vector4.zero : amount;
            foreach (var m in materials) { if (!m) continue; m.SetVector(MudAmount, a); m.SetVector(MudWetness, wetness); m.SetVector(MudColor, color); }
        }
    }
}
