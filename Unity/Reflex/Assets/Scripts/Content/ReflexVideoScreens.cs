// In-level video screens (jumbotrons, the FreightYard projector beam) and the in-game ad slots of a level, read from the install at
// run time (_re/notes/screens-ads.md). Playback and drawing: OriginalWorld/OriginalVideoScreens.cs.
//
// VideoEntity (RTTI 0x939b2c, scene type VideoEntityRSData, 24 levels): VideoEntity::vf5 00594d50 looks up its "Texture To Override"
// (a TextureAsset without pixels: the header of a render target, A8R8G8B8 w x h, e.g. SX_KI_B_Bink_Main 256 x 64) and its "Bink Video
// Asset" (a `bink` resource: a 4-byte length, then the .bik file), opens the video from memory through XenonBinkVideoManager::vf2
// 00595da0 (BinkOpen(data, 0x4100400) = BINKFROMMEMORY | BINKNOFRAMEBUFFERS | BINKALPHA) with the texture as its render target, sets
// Loop (+0x7c = 1) and plays it at once (XenonBinkVideo::vf1). Every material that names the texture then draws the video's latest frame
// (the game's YUV program stretched over the whole target, 00594a40). VideoEntity::vf19 00594f40 pauses / resumes the video (BinkPause,
// XenonBinkVideo::vf2) when the scene's pause flag [[0xa946b8] + 0x5a54 | 0x20c8] + 0x20014 changes and keeps that flag at +0x94.
//
// MassiveAdEntity (0x93930c, MassiveAdEntityRSData, 535 slots on 33 levels): the PC build constructs it as a plain ModelEntity (factory
// 004050ba: 0x140 bytes, ModelEntity ctor 00401db0, only the vtables differ) whose load is ModelEntity::vf5 00401e60 (ModelInstance_Init
// + Entity::vf5): no Massive SDK, and the exe has no "Ad Texture" string, so the slot draws its MaterialList like any model. The default
// ad texture the scene names is the material's own texture in all 535 slots (scratch screens/ads_all.txt), so what shows offline is the
// material as authored. DynamicSponsorPackages.dx9.database (10 packages of one GL_Test1_128 texture) is named nowhere in the exe, the
// registries or another database: never loaded. LocalizedPackageReferenceEntity (41 levels) only lists AudioLocalizedActivity /
// AudioLocalizedChallenges packages (the announcer VO, ReflexAudio): no level has language-dependent signage.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEngine;

namespace Reflex.Port.Content
{
    public static class ReflexVideoScreens
    {
        /// One VideoEntity of the level.
        public sealed class Screen
        {
            public int entityId; public string name, section;
            public string bink;                   // "Bink Video Asset" (a `bink` resource of the level database)
            public string target;                 // "Texture To Override": the render target's TextureAsset
            public int width, height, d3dFormat;  // the target's header (+44 / +48 / +52; A8R8G8B8 = 21 on every level)
            public bool enableTick;
            /// The target is the level's AmbientDirectionalLight "_ColorReferenceMap" (BuenaVista, FunFactory: an animated light colour map that
            /// every lit program samples as s13, samplerAmbientDirectionalLightColorRef, at gColorRefProjection).
            public bool colorReference;
            /// The materials of the level that sample the target: name, texture slot, the slot's sampler states, pixel program.
            public readonly List<(string material, int slot, Dictionary<int, float> sampler, string pixelShader)> uses = new List<(string, int, Dictionary<int, float>, string)>();
            public override string ToString() => $"{name} #{entityId}: {bink} -> {target} {width}x{height} fmt {d3dFormat}, {(colorReference ? "the level's colour reference map, " : "")}{uses.Count} material(s) [{string.Join(", ", uses.Select(u => u.material + "/" + u.pixelShader))}]";
        }

        /// Pixel programs that sample a video target with their own math (Shaders/ReflexVideoScreen.shader). Every other one is a mode of
        /// Reflex/Original Scenery that takes the target as an ordinary texture (Template_Prelit_Map_Ctl, Template_Prelit_Stadiums).
        public static readonly string[] OwnPrograms = { "FY_Projector", "SX_DLC_FY_Bink_ProjectionBeam" };

        /// The level's VideoEntity screens with their targets and the materials that use them (after ReflexLevelContent.Load).
        public static List<Screen> Read(ReflexLevelContent content)
        {
            var list = new List<Screen>();
            if (content?.entities == null) return list;
            foreach (var e in content.entities)
            {
                if (e.Type != "VideoEntityRSData") continue;
                var s = new Screen { entityId = e.id, name = e.String("Name") ?? "VideoEntity", section = e.section, bink = e.Asset("Bink Video Asset"),
                                     target = e.Asset("Texture To Override"), enableTick = e.Bool("EnableTick", true) };
                if (string.IsNullOrEmpty(s.bink) || string.IsNullOrEmpty(s.target)) { content.Warn($"VideoEntity {s.name} #{s.entityId}: no video or target"); continue; }
                try
                {
                    var h = content.ReadAsset(s.target, "texture");
                    s.d3dFormat = ReflexTex.D3dFormat(h); s.width = Rfx.I32(h, 44); s.height = Rfx.I32(h, 48);
                }
                catch (Exception ex) { content.Warn($"VideoEntity {s.name}: target {s.target}: {ex.Message}"); continue; }
                s.colorReference = s.target == content.ColorReferenceName;
                foreach (var kv in content.sceneMaterials)
                    for (int i = 0; i < kv.Value.textures.Count; i++)
                        if (kv.Value.textures[i] == s.target) s.uses.Add((kv.Key, i, kv.Value.samplerStates[i], kv.Value.pixelShader));
                list.Add(s);
            }
            return list;
        }

        /// Tests: the MassiveAdEntity slots of the level against their materials: slots, slots whose "Ad Texture" is a texture of their MaterialList
        /// material, of which in its diffuse / banner sampler, and how many of those the material samples with other sampler states than
        /// the scenery's per-instance ad override (BuildScenery: default states) does.
        public static (int slots, int inMaterial, int diffuseSlot, int samplerDiffers) CheckAds(ReflexLevelContent c)
        {
            int ads = 0, inMat = 0, diffuse = 0, differs = 0;
            foreach (var inst in c.instances)
            {
                if (inst.entityType != "MassiveAdEntityRSData") continue;
                ads++;
                foreach (var mn in inst.materials)
                {
                    if (!c.sceneMaterials.TryGetValue(mn, out var lm)) continue;
                    int i = lm.textures.IndexOf(inst.adTexture);
                    if (i < 0) continue;
                    inMat++;
                    string smp = i < lm.userSamplers.Length ? lm.userSamplers[i].ToLowerInvariant() : "";
                    if (smp.Contains("diffuse") || smp.Contains("banner") || smp.Contains("base")) diffuse++;
                    var st = lm.samplerStates[i];
                    bool same = st == null || st.All(kv => kv.Key == 1 || kv.Key == 2 || kv.Key == 3 ? kv.Value == 1f : kv.Key == 7 ? kv.Value != 2f : kv.Key == 8 ? kv.Value == 0f : true);
                    if (!same) differs++;
                    break;
                }
            }
            return (ads, inMat, diffuse, differs);
        }

        /// The .bik bytes of a screen's video (the `bink` resource without its 4-byte length).
        public static byte[] Video(ReflexLevelContent content, Screen s)
        {
            var b = content.ReadAsset(s.bink, "bink");
            Rfx.Check(b.Length > 12 && Rfx.U32(b, 0) == b.Length - 4 && b[4] == 'B' && b[5] == 'I' && b[6] == 'K', $"bink {s.bink}: header");
            var v = new byte[b.Length - 4]; Buffer.BlockCopy(b, 4, v, 0, v.Length);
            return v;
        }

        static int tempCounter;
        /// Writes the video to a file of the per-user cache that the decoder opens (the plugin's player and ffmpeg read files); the caller
        /// deletes it (the plugin's BinkOpen with BINKPRELOADALL has the whole file in memory at once, as the game's in-memory asset;
        /// ffmpeg opens it again for the loop: deleted when the screens go).
        public static string TempFile(string cacheDir, ReflexLevelContent content, Screen s)
        {
            byte[] bytes = Video(content, s);
            // unique per call: a level reloaded in the frame its old screens are destroyed must not lose its file to their cleanup
            string name = $"{content.level}_{s.entityId}_{System.Diagnostics.Process.GetCurrentProcess().Id}_{System.Threading.Interlocked.Increment(ref tempCounter)}.bik";
            Exception first = null;
            // the per-user cache first; a cache that cannot be written (a locked-down profile, a sandboxed test) falls back to the system's temp folder
            foreach (string root in new[] { cacheDir, Path.Combine(Path.GetTempPath(), "ReflexCache") })
            {
                try
                {
                    string dir = Path.Combine(root, "videos"); Directory.CreateDirectory(dir);
                    try { foreach (string old in Directory.GetFiles(dir, "*.bik")) if (File.GetLastWriteTimeUtc(old) < DateTime.UtcNow.AddDays(-1)) File.Delete(old); } catch (Exception) { }   // left by a crashed run
                    string path = Path.Combine(dir, name);
                    File.WriteAllBytes(path, bytes);
                    return path;
                }
                catch (Exception e) { first ??= e; }
            }
            throw first;
        }
    }

    public sealed partial class ReflexLevelContent
    {
        /// The scenery's texture for a material slot (the same object and sampler states BuildScenery gave it). Main thread.
        internal Texture2D SceneryTexture(string name, Dictionary<int, float> samplerStates) => string.IsNullOrEmpty(name) ? null : TextureFor(name, samplerStates);
    }
}
