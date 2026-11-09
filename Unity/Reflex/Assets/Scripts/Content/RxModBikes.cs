// .rxmod bike mods (core/plan/RXMOD.md): a bike made in Blender, drawn over an original bike of the game whose physics it keeps 1:1.
//
// A .rxmod is a Reflex archive (RXAR, core/plan/RXARCHIVE.md, kind "MODS") - or, while a modder works on it, a plain folder - holding
//   mod.json         the manifest: name, author, description, thumbnail and its bikes; each bike names the original VehicleModelId it
//                    rides as (power, handling, price, ownership: that bike's), its brand / model lines, its mesh and its materials
//   *.rxmesh         a bike's mesh (RxMesh below): nodes named as the original MX_Hierarchy's (OBJ_Frame, OBJ_UPR_Forks, OBJ_LWR_Forks,
//                    OBJ_FRT_Tire, OBJ_Swingarm, OBJ_BCK_Tire, OBJ_Throttle, OBJ_ClutchLVR, OBJ_BrakeLVR) at the mod's own pivots, and
//                    triangle surfaces bound to them (rigid) or weighted over them (the rear shock between the frame and the swingarm)
//   *.png / *.jpg    the maps: a diffuse atlas, a spec map (gives the original programs' SFX map), a normal map (the tyre program's)
// Nothing of the original game is in a .rxmod. Each material borrows an original material of the bike it rides as (its pixel program, its
// constants and render states, the mud) with the mod's maps in place of the original's (ReflexBikeModel.RxMod.cs).
//
// A SKIN .rxmod ("type": "skins") holds no mesh: it paints a bike of another .rxmod (its "mod": that mod's "id" or name, its "bike": the
// bike's "id", model or VehicleModelId) by replacing map files ("maps": {"<the bike mod's entry>": "<the skin's entry>"}) and / or
// material maps ("materials"). Each skin is a MOD card of its own under the original the bike rides as (RxSkin, BikeMods.ResolveSkins).
// Install: <client root>/Mods/Bikes/ (as the classic bike mods, BikeMods.cs): a *.rxmod file, or a folder whose mod.json says
// "format": "reflex-mod", at any depth. BikeMods.Scan lists them next to the classic mods; the garage shows each bike as a MOD card under
// the original it rides as.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using UnityEngine;
using Reflex.Port.Json;

namespace Reflex.Port.Content
{
    /// A material of a .rxmod bike: the original material it borrows (template) and the mod's maps.
    public sealed class RxModMaterial
    {
        public string name;
        /// plastic, frame, engine, wheel, tire, chain (the original programs a bike has: Template_Vehicle_Plstc_Fnl, Template_MX_Frame_FNL,
        /// Template_ATV_MX_Engine_FNL, Template_MX_Wheel_Fnl, Template_tire_FNL, Template_ATV_MX_AlphaMtl_UI_FNL).
        public string template = "plastic";
        public string diffuse, spec, normal, sfx;   // entries of the mod (sfx: an authored SFX map in place of the one made from spec)
        public float mud = 1f, shine = 1f;          // how much mud the surface takes (0..1); a scale of the spec map
        public bool twoSided, alphaTest; public int alphaRef = 128;
        public bool normalFlipY;                    // the normal map's green points down (DirectX) - the programs want it up
        /// "numbers": the rider's run-time number plate on the second UV set, as the original plastics draw it (the mesh's uv1 laid out as
        /// the original's "Numbers" plate: an imported Alive bike's plastics); false: clear (the plates are in the mod's diffuse).
        public bool numbers;
        /// The mod each map is read from: the bike's own mod, or a skin's (RxSkin.Material) for the maps the skin replaces.
        public RxMod diffuseMod, specMod, normalMod, sfxMod;
        public string Key => string.Join("|", template, Src(diffuseMod, diffuse), Src(specMod, spec), Src(normalMod, normal), Src(sfxMod, sfx), mud, shine, twoSided, alphaTest, alphaRef, normalFlipY, numbers);
        static string Src(RxMod m, string e) => e == null ? "-" : (m?.hash ?? "?") + "/" + e;
        public RxModMaterial Clone() => (RxModMaterial)MemberwiseClone();
    }

    /// A skin of a .rxmod bike (a "skins" .rxmod): the maps it replaces, by the bike mod's entry name or per material.
    public sealed class RxSkin
    {
        public RxMod mod; public int index;
        public string targetMod = "", targetBike = "", name = "";
        /// An ORIGINAL bike's skin ("original": its VehicleModelId; -1: a .rxmod bike's): its maps replace the graphics kit's textures by role
        /// ("plastics", "seat", "frame", "engine", "rims", "tires") or by texture name (RXMOD.md "Skins of the original bikes").
        public int original = -1;
        public string logo;   // a path on disk or null
        /// "<the bike mod's map entry>" -> "<this skin's entry>": every material of the bike that uses that map takes the skin's.
        public readonly Dictionary<string, string> maps = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        /// Per material of the bike: the maps (and mud / shine / template) given here replace the bike's; what is not given stays the bike's.
        public readonly Dictionary<string, (string diffuse, string spec, string normal, string sfx, string template, float? mud, float? shine)> materials =
            new Dictionary<string, (string, string, string, string, string, float?, float?)>(StringComparer.OrdinalIgnoreCase);
        /// The bike it paints (BikeMods.ResolveSkins; null: that bike mod is not installed).
        public RxModBike target;
        public string Key => mod.hash + "#s" + index;
        /// The skin by content ("s:<12 hex of the mod's content>:<index>"; the showcase tests log it).
        public string Token => "s:" + mod.hash + ":" + index;
        public string Title => string.IsNullOrEmpty(name) ? mod.name : name;
        /// Whether it paints this vehicle model (an original's skin, or a .rxmod bike's riding as it).
        public bool For(int vehicleModelId) => vehicleModelId >= 0 && (original == vehicleModelId || target?.vehicleModelId == vehicleModelId);

        /// The skin's map for an original material's diffuse texture, null: the kit's own. By the texture's name (HR_ ignored) first, then by
        /// the role of the MATERIAL (its name: MX_Seat_* the seat - whose texture is the full-size bike's plastics atlas -, MX_Plstc_* the
        /// plastics, *Frame*, *_Eng*, *Rim*, *Tire*).
        public string OriginalMap(string texture, string material)
        {
            if (original < 0 || string.IsNullOrEmpty(texture)) return null;
            string t = texture.StartsWith("HR_", StringComparison.OrdinalIgnoreCase) ? texture.Substring(3) : texture;
            if (maps.TryGetValue(t, out var e) || maps.TryGetValue(texture, out e)) return e;
            string n = material ?? "";
            string role = n.IndexOf("Seat", StringComparison.OrdinalIgnoreCase) >= 0 ? "seat" : n.IndexOf("Plstc", StringComparison.OrdinalIgnoreCase) >= 0 ? "plastics"
                        : n.IndexOf("Frame", StringComparison.OrdinalIgnoreCase) >= 0 ? "frame" : n.IndexOf("_Eng", StringComparison.OrdinalIgnoreCase) >= 0 ? "engine"
                        : n.IndexOf("Rim", StringComparison.OrdinalIgnoreCase) >= 0 ? "rims" : n.IndexOf("Tire", StringComparison.OrdinalIgnoreCase) >= 0 ? "tires" : null;
            return role != null && maps.TryGetValue(role, out e) ? e : null;
        }

        /// The bike's material as this skin paints it.
        public RxModMaterial Material(RxModMaterial b)
        {
            var m = b.Clone();
            string Map(string entry, RxMod from, out RxMod src) { src = from; if (entry != null && maps.TryGetValue(entry, out var e) && mod.Exists(e)) { src = mod; return e; } return entry; }
            m.diffuse = Map(b.diffuse, b.diffuseMod, out m.diffuseMod); m.spec = Map(b.spec, b.specMod, out m.specMod);
            m.normal = Map(b.normal, b.normalMod, out m.normalMod); m.sfx = Map(b.sfx, b.sfxMod, out m.sfxMod);
            if (materials.TryGetValue(b.name, out var o))
            {
                if (o.diffuse != null) { m.diffuse = o.diffuse; m.diffuseMod = mod; }
                if (o.spec != null) { m.spec = o.spec; m.specMod = mod; }
                if (o.normal != null) { m.normal = o.normal; m.normalMod = mod; }
                if (o.sfx != null) { m.sfx = o.sfx; m.sfxMod = mod; }
                if (o.template != null && Array.IndexOf(RxModBikes.Templates, o.template) >= 0) m.template = o.template;
                if (o.mud != null) m.mud = o.mud.Value; if (o.shine != null) m.shine = o.shine.Value;
            }
            return m;
        }
    }

    /// One bike of a .rxmod.
    public sealed class RxModBike
    {
        public RxMod mod; public int index;
        /// The original bike it rides as: its VehicleModelId (the garage's bikes; core/plan/RXMOD.md lists them).
        public int vehicleModelId = -1;
        public string original = "", mesh, id = "";   // id: the bike's own id in its mod (skins name it), optional
        public BikeName name;
        public string logo;   // a path on disk (extracted from an archive) or null
        public readonly Dictionary<string, RxModMaterial> materials = new Dictionary<string, RxModMaterial>(StringComparer.OrdinalIgnoreCase);
        /// "keep" (the mod's own size, aligned on the rear axle and the axle line) or "axles" (scaled so both axles sit on the original's).
        public string fit = "axles";
        /// ALIVE CLASSES (core/plan/ALIVE_BIKES.md): "physics": {"file": <a parameter file of the mx*_vehicle schema in the mod>, "class": "mx50"
        /// / "mx650f"}: the bike rides on its own physics (the file over the original's class file, for the local rider only:
        /// rx_session_vehicle_physics) instead of the original's 1:1. Only an imported MX vs ATV Alive bike has one (the importer writes
        /// Alive's own file from the player's install); physicsClass the core's class id (9, 10), -1 none.
        public string physicsFile, physicsNodes; public int physicsClass = -1;
        /// "class": the garage's class header of the bike ("50CC", "650"); null: the original's class.
        public string garageClass;
        public static readonly string[] PhysicsClasses = { "mx50", "mx650f", "atv1000" };
        public const int PhysicsClassFirst = 9;
        /// "pose": "physics": the parts placed by the simulation's bodies on the mod's own hierarchy (its nodes' rest pose), not by the
        /// original's hierarchy (a bike of another size: the Alive 50cc); "fit" is then "keep".
        public bool physicsPose;
        /// "fit": "nodes": the mod's parts bound to its own node rest pose, unscaled, posed by the original's hierarchy (the mod's nodes are the
        /// original's: an imported Alive ATV over a Reflex ATV, ATV_Hierarchy node for node). Also true with physicsPose.
        public bool OwnNodes => physicsPose || fit == "nodes";
        public string Key => mod.hash + "#" + index;
        /// The bike by content ("b:<12 hex of the mod's content>:<VehicleModelId>"; the showcase tests log it).
        public string Token => "b:" + mod.hash + ":" + vehicleModelId;
    }

    /// A .rxmod (an archive or a folder) and what its manifest says.
    public sealed class RxMod
    {
        public string path;          // the .rxmod file or the folder
        public bool archive;
        IntPtr handle;
        readonly object gate = new object();
        public string hash = "";     // 12 hex: SHA-256 of the manifest and every entry's path, size and CRC
        public string name = "", author = "", description = "", thumbnail;   // thumbnail: a path on disk or null
        public string id = "", type = "bikes";   // id: the mod's own stable id (skins name it), optional; type: bikes or skins
        public readonly List<RxModBike> bikes = new List<RxModBike>();
        public readonly List<RxSkin> skins = new List<RxSkin>();
        public readonly List<string> entries = new List<string>();
        public string error;

        public bool Exists(string entry) => entry != null && entries.Any(e => string.Equals(e, Norm(entry), StringComparison.OrdinalIgnoreCase));
        static string Norm(string e) => (e ?? "").Replace('\\', '/').TrimStart('/');

        /// An entry's bytes (thread-safe); throws FileNotFoundException / IOException.
        public byte[] Read(string entry)
        {
            entry = Norm(entry);
            if (entry.Length == 0 || entry.Split('/').Any(p => p == ".." || p == ".")) throw new FileNotFoundException("bad entry name " + entry);
            if (!archive)
            {
                string f = Path.Combine(path, entry.Replace('/', Path.DirectorySeparatorChar));
                if (!File.Exists(f)) throw new FileNotFoundException(entry + " is not in " + path);
                return File.ReadAllBytes(f);
            }
            lock (gate)
            {
                if (handle == IntPtr.Zero) handle = Reflex.Port.RxCore.RxArchiveNative.Open(path);
                if (handle == IntPtr.Zero) throw new IOException(path + ": " + Reflex.Port.RxCore.RxArchiveNative.LastError());
                int i = Reflex.Port.RxCore.RxArchiveNative.Find(handle, entry);
                if (i < 0) throw new FileNotFoundException(entry + " is not in " + Path.GetFileName(path));
                long n = Reflex.Port.RxCore.RxArchiveNative.Size(handle, i);
                if (n < 0 || n > 512L << 20) throw new IOException(entry + ": size " + n);
                var b = new byte[n];
                if (!Reflex.Port.RxCore.RxArchiveNative.Read(handle, i, 0, b, 0, (int)n)) throw new IOException(entry + ": " + Reflex.Port.RxCore.RxArchiveNative.LastError());
                return b;
            }
        }

        public void Close() { lock (gate) { if (handle != IntPtr.Zero) Reflex.Port.RxCore.RxArchiveNative.Close(handle); handle = IntPtr.Zero; } }

        /// An entry as a file on disk (the menus load logos / thumbnails from paths): the folder's own file, or a copy extracted once into
        /// the cache (RxModBikes.CacheRoot/<hash>/). Null when it is missing.
        public string FileOf(string entry)
        {
            if (string.IsNullOrEmpty(entry) || !Exists(entry)) return null;
            if (!archive) return Path.Combine(path, Norm(entry).Replace('/', Path.DirectorySeparatorChar));
            try
            {
                string dir = Path.Combine(RxModBikes.CacheRoot, hash);
                string f = Path.Combine(dir, Norm(entry).Replace('/', '_'));
                if (!File.Exists(f)) { Directory.CreateDirectory(dir); File.WriteAllBytes(f + ".partial", Read(entry)); if (File.Exists(f)) File.Delete(f); File.Move(f + ".partial", f); }
                return f;
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { Debug.LogWarning("[RxMod] " + Path.GetFileName(path) + " " + entry + ": " + e.Message); return null; }
        }
    }

    /// A .rxmod bike's mesh (the .rxmesh entry), as read.
    internal sealed class RxMesh
    {
        public string[] nodes; public int[] parents; public Matrix4x4[] world;   // mod space: feet, X right, Y up, Z forward
        public string[] materials;
        public readonly List<(Rfx.Surface surface, int material, int node)> surfaces = new List<(Rfx.Surface, int, int)>();
        public int NodeIndex(string n) { for (int i = 0; i < nodes.Length; i++) if (string.Equals(nodes[i], n, StringComparison.OrdinalIgnoreCase)) return i; return -1; }
        public int Vertices => surfaces.Sum(s => s.surface.positions.Length);
        public int Triangles => surfaces.Sum(s => s.surface.triangles.Length / 3);
    }

    public static class RxModBikes
    {
        public const string Ext = ".rxmod", MeshMagic = "RXMS", Format = "reflex-mod";
        public const uint Kind = 0x53444F4Du;   // "MODS" as a little-endian u32 (the archive header's kind)
        public static readonly string[] Templates = { "plastic", "frame", "engine", "wheel", "tire", "chain" };
        /// Where an archive's logos / thumbnails are extracted for the menus (any thread: no Unity path API).
        internal static string CacheRoot => Path.Combine(Path.GetTempPath(), "OpenReflex-rxmod");

        // ------------------------------------------------------------------------------------------------ open + manifest

        /// Whether a file / folder is a .rxmod (a *.rxmod file, or a folder with a reflex-mod mod.json).
        public static bool IsRxMod(string path)
        {
            if (File.Exists(path)) return path.EndsWith(Ext, StringComparison.OrdinalIgnoreCase);
            string j = Path.Combine(path, "mod.json");
            if (!File.Exists(j)) return false;
            try { var m = J.Parse(File.ReadAllText(j)); return m["format"].String == Format; } catch (Exception) { return false; }
        }

        /// Opens a .rxmod and reads its manifest (never throws: a bad mod has .error set and no bikes).
        public static RxMod Open(string path)
        {
            var m = new RxMod { path = path, archive = File.Exists(path), name = Path.GetFileNameWithoutExtension(path.TrimEnd('/', '\\')) };
            try
            {
                if (m.archive)
                {
                    var h = Reflex.Port.RxCore.RxArchiveNative.Open(path);
                    if (h == IntPtr.Zero) throw new IOException(Reflex.Port.RxCore.RxArchiveNative.LastError());
                    try
                    {
                        if (Reflex.Port.RxCore.RxArchiveNative.Kind(h) != Kind) throw new InvalidDataException("not a mod archive (kind " + Reflex.Port.RxCore.RxArchiveNative.Kind(h).ToString("x8") + ")");
                        Reflex.Port.RxCore.RxArchiveNative.List(h, out var paths, out var sizes, out _);
                        m.entries.AddRange(paths);
                        m.hash = Hash(m, paths.Select((p, i) => p + ":" + sizes[i] + ":" + EntryId(h, i)));
                    }
                    finally { Reflex.Port.RxCore.RxArchiveNative.Close(h); }
                }
                else
                {
                    foreach (var f in Directory.GetFiles(path, "*", SearchOption.AllDirectories))
                    {
                        string rel = f.Substring(path.TrimEnd('/', '\\').Length + 1).Replace('\\', '/');
                        if (!ReflexMods.IsJunk(Path.GetFileName(f))) m.entries.Add(rel);
                    }
                    m.entries.Sort(StringComparer.Ordinal);
                    m.hash = Hash(m, m.entries.Select(e => e + ":" + new FileInfo(Path.Combine(path, e)).Length));
                }
                ReadManifest(m, Encoding.UTF8.GetString(m.Read("mod.json")));
            }
            catch (Exception e) when (e is IOException || e is InvalidDataException || e is FormatException || e is UnauthorizedAccessException || e is EntryPointNotFoundException || e is DllNotFoundException)
            {
                m.error = e.Message; m.bikes.Clear(); m.skins.Clear();
                Debug.LogWarning("[RxMod] " + path + ": " + e.Message);
            }
            return m;
        }

        static string EntryId(IntPtr h, int i) => Reflex.Port.RxCore.RxArchiveNative.EntrySha(h, i, out var hex) != 0 ? hex : "";
        static string Hash(RxMod m, IEnumerable<string> lines)
        {
            using var sha = SHA256.Create();
            var b = sha.ComputeHash(Encoding.UTF8.GetBytes(string.Join("\n", lines)));
            return BitConverter.ToString(b, 0, 6).Replace("-", "").ToLowerInvariant();
        }

        /// mod.json (core/plan/RXMOD.md "Manifest").
        internal static void ReadManifest(RxMod m, string text)
        {
            var j = J.Parse(text);
            if (!j.IsObject || j["format"].String != Format) throw new InvalidDataException("mod.json: \"format\" is not \"" + Format + "\"");
            if (j["version"].IsNumber && j["version"].Int > 1) throw new InvalidDataException("mod.json: version " + j["version"].Int + " is newer than this game reads");
            string type = j["type"].String ?? "bikes";
            if (type == "kits") throw new InvalidDataException("a kit pack .rxmod: it goes in Mods/Gear (bike graphics kits and rider gear)");
            if (type != "bikes" && type != "bike" && type != "skins" && type != "skin") throw new InvalidDataException("mod.json: type \"" + type + "\" (this game reads bike and skin mods)");
            if (!string.IsNullOrWhiteSpace(j["name"].String)) m.name = j["name"].String.Trim();
            m.author = j["author"].String?.Trim() ?? ""; m.description = j["description"].String?.Trim() ?? "";
            m.id = j["id"].String?.Trim() ?? "";
            if (!string.IsNullOrWhiteSpace(j["thumbnail"].String)) m.thumbnail = m.FileOf(j["thumbnail"].String.Trim());
            if (type.StartsWith("skin", StringComparison.Ordinal)) { m.type = "skins"; ReadSkins(m, j); return; }
            var bikes = j["bikes"]; if (!bikes.IsArray) throw new InvalidDataException("mod.json: no \"bikes\" list");
            for (int i = 0; i < bikes.Count; i++)
            {
                var o = bikes[i]; if (!o.IsObject) continue;
                var b = new RxModBike { mod = m, index = i, mesh = o["mesh"].String, original = o["original"].String ?? "", fit = o["fit"].String ?? "axles", id = o["id"].String?.Trim() ?? "" };
                b.vehicleModelId = o["bike"].IsInteger ? o["bike"].Int : int.TryParse(o["bike"].String, out int id) ? id : -1;
                var ph = o["physics"];
                if (ph.IsObject)
                {
                    int ci = Array.IndexOf(RxModBike.PhysicsClasses, (ph["class"].String ?? "").ToLowerInvariant());
                    if (ci >= 0 && m.Exists(ph["file"].String))
                    {
                        b.physicsFile = ph["file"].String; b.physicsClass = RxModBike.PhysicsClassFirst + ci;
                        if (m.Exists(ph["nodes"].String)) b.physicsNodes = ph["nodes"].String;
                    }
                    else Debug.LogWarning($"[RxMod] {m.name}: bike {i}: physics {ph["class"].String} / {ph["file"].String} not read (the original's physics)");
                }
                b.physicsPose = string.Equals(o["pose"].String, "physics", StringComparison.OrdinalIgnoreCase) && b.physicsNodes != null;   // (only with the core's own nodes)
                if (!string.IsNullOrWhiteSpace(o["class"].String)) b.garageClass = o["class"].String.Trim().ToUpperInvariant();
                if (b.physicsPose) b.fit = "keep";
                if (b.vehicleModelId < 0) { Debug.LogWarning($"[RxMod] {m.name}: bike {i} names no original bike (\"bike\": its VehicleModelId)"); continue; }
                if (string.IsNullOrEmpty(b.mesh) || !m.Exists(b.mesh)) { Debug.LogWarning($"[RxMod] {m.name}: bike {i}: mesh {b.mesh} is not in the mod"); continue; }
                b.name = !string.IsNullOrWhiteSpace(o["brand"].String) || !string.IsNullOrWhiteSpace(o["model"].String)
                    ? new BikeName { brand = o["brand"].String?.Trim() ?? "", model = o["model"].String?.Trim() ?? "" }
                    : BikeName.Parse(o["name"].String);
                if (!string.IsNullOrWhiteSpace(o["logo"].String)) b.logo = m.FileOf(o["logo"].String.Trim());
                var mats = o["materials"];
                if (mats.IsObject)
                    foreach (string k in mats.Keys)
                    {
                        var x = mats[k]; if (!x.IsObject) continue;
                        var mm = new RxModMaterial
                        {
                            name = k, template = (x["template"].String ?? "plastic").ToLowerInvariant(), diffuse = x["diffuse"].String, spec = x["spec"].String,
                            normal = x["normal"].String, sfx = x["sfx"].String, twoSided = x["twoSided"].Boolean,
                            alphaTest = x["alphaTest"].Boolean, normalFlipY = x["normalFlipY"].Boolean, numbers = x["numbers"].Boolean,
                            diffuseMod = m, specMod = m, normalMod = m, sfxMod = m,
                        };
                        if (x["mud"].IsNumber) mm.mud = Mathf.Clamp01((float)x["mud"].Number);
                        if (x["shine"].IsNumber) mm.shine = Mathf.Clamp((float)x["shine"].Number, 0f, 4f);
                        if (x["alphaRef"].IsInteger) mm.alphaRef = Mathf.Clamp(x["alphaRef"].Int, 1, 255);
                        if (Array.IndexOf(Templates, mm.template) < 0) { Debug.LogWarning($"[RxMod] {m.name}: material {k}: template {mm.template} unknown (plastic used)"); mm.template = "plastic"; }
                        b.materials[k] = mm;
                    }
                m.bikes.Add(b);
            }
            if (m.bikes.Count == 0) throw new InvalidDataException("mod.json: no bike could be read");
        }

        /// A skins mod.json's "skins" (core/plan/RXMOD.md "Skins"): each {"mod", "bike" (a name or a list), "name", "maps", "materials", "logo"}.
        static void ReadSkins(RxMod m, J j)
        {
            var list = j["skins"]; if (!list.IsArray) throw new InvalidDataException("mod.json: no \"skins\" list");
            for (int i = 0; i < list.Count; i++)
            {
                var o = list[i]; if (!o.IsObject) continue;
                var targets = o["bike"].IsArray ? o["bike"].Select(x => x.IsNumber ? x.Int.ToString() : x.String).ToList() : new List<string> { o["bike"].IsNumber ? o["bike"].Int.ToString() : o["bike"].String ?? "" };
                foreach (var t in targets)
                {
                    var k = new RxSkin { mod = m, index = m.skins.Count, targetMod = o["mod"].String?.Trim() ?? "", targetBike = t?.Trim() ?? "", name = o["name"].String?.Trim() ?? "" };
                    if (o["original"].IsInteger) k.original = o["original"].Int; else if (int.TryParse(o["original"].String, out int ov)) k.original = ov;
                    if (k.targetMod.Length == 0 && k.original < 0) { Debug.LogWarning($"[RxMod] {m.name}: skin {i} names neither a bike mod (\"mod\") nor an original bike (\"original\": its VehicleModelId)"); continue; }
                    if (!string.IsNullOrWhiteSpace(o["logo"].String)) k.logo = m.FileOf(o["logo"].String.Trim());
                    var maps = o["maps"];
                    if (maps.IsObject) foreach (string key in maps.Keys)
                    {
                        string e = maps[key].String;
                        if (string.IsNullOrEmpty(e) || !m.Exists(e)) { Debug.LogWarning($"[RxMod] {m.name}: skin {i}: map {e} is not in the mod"); continue; }
                        k.maps[key.Replace('\\', '/').TrimStart('/')] = e;   // (an original's skin: a role or a texture name)
                    }
                    var mats = o["materials"];
                    if (mats.IsObject) foreach (string key in mats.Keys)
                    {
                        var x = mats[key]; if (!x.IsObject) continue;
                        string E(string f) { var v = x[f].String; if (v == null) return null; if (!m.Exists(v)) { Debug.LogWarning($"[RxMod] {m.name}: skin {i}: {f} {v} is not in the mod"); return null; } return v; }
                        k.materials[key] = (E("diffuse"), E("spec"), E("normal"), E("sfx"), x["template"].String?.ToLowerInvariant(),
                                            x["mud"].IsNumber ? Mathf.Clamp01((float)x["mud"].Number) : (float?)null, x["shine"].IsNumber ? Mathf.Clamp((float)x["shine"].Number, 0f, 4f) : (float?)null);
                    }
                    if (k.maps.Count == 0 && k.materials.Count == 0) { Debug.LogWarning($"[RxMod] {m.name}: skin {i} replaces no map"); continue; }
                    m.skins.Add(k);
                }
            }
            if (m.skins.Count == 0) throw new InvalidDataException("mod.json: no skin could be read");
        }

        /// Whether a skin's "mod" / "bike" name this bike: the mod's id or name; the bike's id, model ("YZ250F"), brand + model or VehicleModelId
        /// (an empty "bike": the mod's only bike). Case and spaces ignored.
        public static bool Paints(RxSkin k, RxModBike b)
        {
            string N(string x) => new string((x ?? "").Where(char.IsLetterOrDigit).ToArray()).ToLowerInvariant();
            if (b?.mod == null || b.mod.type != "bikes" || k.original >= 0) return false;
            string t = N(k.targetMod);
            if (t.Length == 0 || (t != N(b.mod.id) && t != N(b.mod.name))) return false;
            string w = N(k.targetBike);
            if (w.Length == 0) return b.mod.bikes.Count == 1;
            return w == N(b.id) || w == N(b.name?.model) || w == N(b.name?.Full) || w == b.vehicleModelId.ToString();
        }

        /// The material a bike's mesh names, as drawn (its skin's when it has one); null: not in the bike's mod.json.
        public static RxModMaterial MaterialOf(RxModBike b, RxSkin skin, string name)
        {
            if (b == null || !b.materials.TryGetValue(name, out var mm)) return null;
            return skin != null && skin.target == b ? skin.Material(mm) : mm;
        }

        // ------------------------------------------------------------------------------------------------ meshes

        static readonly Dictionary<string, RxMesh> meshes = new Dictionary<string, RxMesh>();
        public static void ReleaseParsed() { lock (meshes) meshes.Clear(); }

        /// A bike's mesh, read once (any thread).
        internal static RxMesh Mesh(RxModBike b)
        {
            string key = b.mod.hash + "/" + b.mesh;
            lock (meshes) if (meshes.TryGetValue(key, out var c)) return c;
            using var perf = Reflex.Port.Platform.ReflexMenuPerf.Measure("model read and parse");
            var m = ParseMesh(b.mod.Read(b.mesh));
            lock (meshes) meshes[key] = m;
            return m;
        }

        /// The .rxmesh layout (little-endian; core/plan/RXMOD.md "Mesh"):
        ///   "RXMS" u32 version (1) u32 flags (0)
        ///   u32 nodes; each: str name, i32 parent (-1 root, else an earlier node), f32[16] world matrix (row-major, column vectors: the
        ///     translation is elements 3, 7, 11) in mod space (feet; X right, Y up, Z forward)
        ///   u32 materials; each: str name
        ///   u32 surfaces; each: u32 material, u32 node, u32 flags (1 skinned, 2 uv1, 4 tangents), u32 vertices V, u32 indices I,
        ///     f32[3V] positions, f32[3V] normals, f32[2V] uv0 (OpenGL / Blender: v up from the image's bottom), [f32[2V] uv1],
        ///     [f32[4V] tangents], [u8[4V] bone nodes (255 none), f32[4V] weights], u32[I] triangles (clockwise front faces)
        ///   str = u16 byte length + UTF-8
        internal static RxMesh ParseMesh(byte[] data)
        {
            using var r = new BinaryReader(new MemoryStream(data, false), Encoding.UTF8);
            void Need(bool ok, string what) { if (!ok) throw new InvalidDataException("rxmesh: " + what); }
            string Str() { int n = r.ReadUInt16(); return Encoding.UTF8.GetString(r.ReadBytes(n)); }
            Need(data.Length >= 16 && Encoding.ASCII.GetString(r.ReadBytes(4)) == MeshMagic, "magic");
            int version = r.ReadInt32(); Need(version == 1, "version " + version + " is newer than this game reads"); r.ReadInt32();
            var m = new RxMesh();
            int nodes = r.ReadInt32(); Need(nodes > 0 && nodes <= 256, "node count");
            m.nodes = new string[nodes]; m.parents = new int[nodes]; m.world = new Matrix4x4[nodes];
            for (int i = 0; i < nodes; i++)
            {
                m.nodes[i] = Str(); m.parents[i] = r.ReadInt32(); Need(m.parents[i] >= -1 && m.parents[i] < i, "node order");
                var w = new Matrix4x4(); for (int k = 0; k < 16; k++) w[k / 4, k % 4] = r.ReadSingle();
                m.world[i] = w;
            }
            int mats = r.ReadInt32(); Need(mats >= 0 && mats <= 256, "material count");
            m.materials = new string[mats]; for (int i = 0; i < mats; i++) m.materials[i] = Str();
            int surfs = r.ReadInt32(); Need(surfs >= 0 && surfs <= 4096, "surface count");
            float[] F(int n) { var a = new float[n]; var b = r.ReadBytes(n * 4); Need(b.Length == n * 4, "truncated"); Buffer.BlockCopy(b, 0, a, 0, b.Length); return a; }
            for (int s = 0; s < surfs; s++)
            {
                int mat = r.ReadInt32(), node = r.ReadInt32(), flags = r.ReadInt32(), v = r.ReadInt32(), ix = r.ReadInt32();
                Need(mat >= 0 && mat < mats && node >= 0 && node < nodes && v > 0 && v <= 4_000_000 && ix % 3 == 0 && ix > 0 && ix <= 12_000_000 && (flags & ~7) == 0, "surface " + s);
                var p = F(v * 3); var n = F(v * 3); var t0 = F(v * 2); var t1 = (flags & 2) != 0 ? F(v * 2) : null; var tg = (flags & 4) != 0 ? F(v * 4) : null;
                var sf = new Rfx.Surface { positions = new Vector3[v], normals = new Vector3[v], uv0 = new Vector2[v], uv1 = new Vector2[v], strip = new int[0] };
                for (int i = 0; i < v; i++)
                {
                    sf.positions[i] = new Vector3(p[i * 3], p[i * 3 + 1], p[i * 3 + 2]);
                    sf.normals[i] = new Vector3(n[i * 3], n[i * 3 + 1], n[i * 3 + 2]);
                    sf.uv0[i] = new Vector2(t0[i * 2], 1f - t0[i * 2 + 1]);   // (stored as the game's surfaces: D3D rows; ReflexMeshes.Build flips back)
                    if (t1 != null) sf.uv1[i] = new Vector2(t1[i * 2], 1f - t1[i * 2 + 1]);
                }
                if (tg != null) { sf.tangents = new Vector3[v]; for (int i = 0; i < v; i++) sf.tangents[i] = new Vector3(tg[i * 4], tg[i * 4 + 1], tg[i * 4 + 2]); }
                if ((flags & 1) != 0)
                {
                    sf.skinned = true; sf.bones = new int[v * 4]; var bb = r.ReadBytes(v * 4); Need(bb.Length == v * 4, "truncated");
                    for (int i = 0; i < v * 4; i++) { sf.bones[i] = bb[i] == 255 ? -1 : bb[i]; Need(sf.bones[i] < nodes, "bone node"); }
                    sf.weights = F(v * 4);
                }
                var tri = new int[ix]; var ib = r.ReadBytes(ix * 4); Need(ib.Length == ix * 4, "truncated"); Buffer.BlockCopy(ib, 0, tri, 0, ib.Length);
                foreach (int k in tri) Need(k >= 0 && k < v, "index");
                sf.triangles = tri;
                m.surfaces.Add((sf, mat, node));
            }
            return m;
        }

        // ------------------------------------------------------------------------------------------------ maps

        static readonly Dictionary<string, Texture2D> textures = new Dictionary<string, Texture2D>();
        public static void ReleaseTextures() { foreach (var t in textures.Values) if (t) UnityEngine.Object.Destroy(t); textures.Clear(); pixels.Clear(); }

        /// A map of the mod as the programs sample it: raw values (no sRGB), mipmapped, block-compressed (main thread).
        internal static Texture2D Map(RxMod m, string entry)
        {
            if (string.IsNullOrEmpty(entry) || m == null) return null;
            string key = m.hash + "/" + entry;
            if (textures.TryGetValue(key, out var c) && c) return c;
            try
            {
                using var perf = Reflex.Port.Platform.ReflexMenuPerf.Measure("rxmod texture");
                var t = new Texture2D(2, 2, TextureFormat.RGBA32, true, true) { name = Path.GetFileName(entry), wrapMode = TextureWrapMode.Repeat, filterMode = FilterMode.Trilinear, anisoLevel = 4, hideFlags = HideFlags.DontSave };
                if (!t.LoadImage(m.Read(entry), false)) throw new InvalidDataException("not a PNG / JPG");
                t.Compress(false); t.Apply(false, true);
                return textures[key] = t;
            }
            catch (Exception e) when (e is IOException || e is InvalidDataException) { Debug.LogWarning($"[RxMod] {m.name} {entry}: {e.Message}"); return null; }
        }

        /// The original programs' SFX / combo map for a material, made from its spec map (core/plan/RXMOD.md "Materials"): R the shine the
        /// frame / engine / wheel programs blend towards (0 on plastics: there R lights the diffuse itself), G where mud may build up (mud x MudMask), B the
        /// reflection; the chain program's B is its alpha (the diffuse's). An authored "sfx" map is used as it is.
        /// G of the SFX map at "mud": 1 (the share of the surface a fully muddy bike has caked: mud = saturate(G + usr_mud_amount - 1)). The
        /// original plastics' mask is 0.03 at its median and 0.73 at its 90th percentile (their lower edges): half everywhere is the even middle.
        public const float MudMask = 0.5f;
        internal static Texture2D Sfx(RxModMaterial mat)
        {
            if (!string.IsNullOrEmpty(mat.sfx)) return Map(mat.sfxMod, mat.sfx);
            string key = "sfx/" + mat.template + "/" + mat.specMod?.hash + "/" + mat.spec + "/" + mat.diffuseMod?.hash + "/" + mat.diffuse + "/" + mat.mud + "/" + mat.shine;
            if (textures.TryGetValue(key, out var c) && c) return c;
            {
                using var perf = Reflex.Port.Platform.ReflexMenuPerf.Measure("rxmod sfx map");
                bool chain = mat.template == "chain";
                var (sw, sh) = !string.IsNullOrEmpty(mat.spec) ? SizeOf(mat.specMod, mat.spec) : (0, 0);
                var (dw, dh) = chain && !string.IsNullOrEmpty(mat.diffuse) ? SizeOf(mat.diffuseMod, mat.diffuse) : (0, 0);
                int w = sw > 0 ? sw : dw > 0 ? dw : 4, h = sw > 0 ? sh : dw > 0 ? dh : 4;
                if (w > 2048) { h = Math.Max(4, h * 2048 / w); w = 2048; }
                var sp = sw > 0 ? Pixels(mat.specMod, mat.spec, w, h) : null; var dp = dw > 0 ? Pixels(mat.diffuseMod, mat.diffuse, w, h) : null;
                var o = new Color32[w * h]; byte mud = (byte)Mathf.RoundToInt(mat.mud * MudMask * 255);
                bool plastic = mat.template == "plastic";
                for (int i = 0; i < o.Length; i++)
                {
                    int s = sp != null ? Mathf.Clamp(Mathf.RoundToInt((sp[i].r * 0.299f + sp[i].g * 0.587f + sp[i].b * 0.114f) * mat.shine), 0, 255) : 64;
                    o[i] = new Color32(plastic ? (byte)0 : (byte)s, mud, chain ? (dp != null ? dp[i].a : (byte)255) : (byte)s, 255);
                }
                var t = new Texture2D(w, h, TextureFormat.RGBA32, true, true) { name = mat.name + "_SFX", wrapMode = TextureWrapMode.Repeat, filterMode = FilterMode.Trilinear, hideFlags = HideFlags.DontSave };
                t.SetPixels32(o); t.Apply(true, false); t.Compress(false); t.Apply(false, true);
                return textures[key] = t;
            }
        }

        /// The tyre program's normal map (the original's is G16R16F: signed X / Y, read raw as RGHalf) from an 8-bit normal map.
        internal static Texture2D Normal(RxModMaterial mat)
        {
            if (string.IsNullOrEmpty(mat.normal)) return null;
            var m = mat.normalMod;
            string key = m.hash + "/nrm/" + mat.normal + "/" + mat.normalFlipY;
            if (textures.TryGetValue(key, out var c) && c) return c;
            var src = Load(m, mat.normal); if (!src) return null;
            try
            {
                int w = Math.Min(src.width, 2048), h = Math.Max(4, src.height * w / src.width);
                var px = Resample(src, w, h);
                var o = new Color[w * h];
                for (int i = 0; i < o.Length; i++) o[i] = new Color(px[i].r / 127.5f - 1f, (mat.normalFlipY ? -1f : 1f) * (px[i].g / 127.5f - 1f), 0, 1);
                var t = new Texture2D(w, h, TextureFormat.RGHalf, true, true) { name = mat.name + "_NRML", wrapMode = TextureWrapMode.Repeat, filterMode = FilterMode.Trilinear, hideFlags = HideFlags.DontSave };
                t.SetPixels(o); t.Apply(true, true);
                return textures[key] = t;
            }
            finally { UnityEngine.Object.Destroy(src); }
        }

        static readonly Dictionary<string, (int w, int h, Color32[] px)> pixels = new Dictionary<string, (int, int, Color32[])>();
        /// A map's pixels at w x h, decoded once per mod and size (the three templates' SFX maps come from one spec map).
        static Color32[] Pixels(RxMod m, string entry, int w, int h)
        {
            string key = m.hash + "/" + entry + "/" + w + "x" + h;
            if (pixels.TryGetValue(key, out var c)) return c.px;
            var t = Load(m, entry); if (!t) return null;
            try { var px = Resample(t, w, h); if (pixels.Count > 8) pixels.Clear(); pixels[key] = (w, h, px); return px; }
            finally { UnityEngine.Object.Destroy(t); }
        }
        static (int w, int h) SizeOf(RxMod m, string entry)
        {
            foreach (var kv in pixels) if (kv.Key.StartsWith(m.hash + "/" + entry + "/", StringComparison.Ordinal)) return (kv.Value.w, kv.Value.h);
            var t = Load(m, entry); if (!t) return (0, 0);
            try { return (t.width, t.height); } finally { UnityEngine.Object.Destroy(t); }
        }

        static Texture2D Load(RxMod m, string entry)
        {
            if (string.IsNullOrEmpty(entry)) return null;
            try
            {
                var t = new Texture2D(2, 2, TextureFormat.RGBA32, false, true) { hideFlags = HideFlags.DontSave };
                if (!t.LoadImage(m.Read(entry), false)) { UnityEngine.Object.Destroy(t); return null; }
                return t;
            }
            catch (Exception e) when (e is IOException || e is InvalidDataException) { Debug.LogWarning($"[RxMod] {m.name} {entry}: {e.Message}"); return null; }
        }

        /// A texture's pixels at w x h (nearest; the maps are powers of two).
        static Color32[] Resample(Texture2D t, int w, int h)
        {
            var src = t.GetPixels32(); if (t.width == w && t.height == h) return src;
            var o = new Color32[w * h];
            for (int y = 0; y < h; y++) { int sy = y * t.height / h; for (int x = 0; x < w; x++) o[y * w + x] = src[sy * t.width + x * t.width / w]; }
            return o;
        }
    }
}
