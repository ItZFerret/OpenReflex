using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using Unity.Collections;
using Unity.Jobs;
using UnityEngine;
using UnityEngine.Jobs;
using UnityEngine.Rendering;
using Reflex.Port.Content;

namespace Reflex.Port.OriginalWorld
{
    [StructLayout(LayoutKind.Sequential)]
    public struct ReflexAmbientInfo
    {
        public int entities, matrices, objects, reserved0;
        public float fadeStart, lodDivisor, reserved1, reserved2;
    }

    public static class ReflexAmbientNative
    {
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_ambient_mask__fn(IntPtr world, uint keep);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_ambient_mask__fn> reflex_world_ambient_mask__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_ambient_mask__fn>("reflex_world_ambient_mask");
        public static int reflex_world_ambient_mask(IntPtr world, uint keep) => reflex_world_ambient_mask__p.Get()(world, keep);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_ambient__fn(IntPtr world, [Out] byte[] entities, uint max, [Out] float[] matrices, uint maxMatrices, ref ReflexAmbientInfo info);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_ambient__fn> reflex_world_ambient__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_ambient__fn>("reflex_world_ambient");
        public static int reflex_world_ambient(IntPtr world, [Out] byte[] entities, uint max, [Out] float[] matrices, uint maxMatrices, ref ReflexAmbientInfo info) => reflex_world_ambient__p.Get()(world, entities, max, matrices, maxMatrices, ref info);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_ambient_rescan__fn(IntPtr world);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_ambient_rescan__fn> reflex_world_ambient_rescan__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_ambient_rescan__fn>("reflex_world_ambient_rescan");
        public static int reflex_world_ambient_rescan(IntPtr world) => reflex_world_ambient_rescan__p.Get()(world);
    }

    // The original game's ambient animated entities (native/drafts/world_runtime/ambient.cpp, _re/names/76_ambient.tsv): crew
    // chiefs, pit crews, score-tower people and flaggers, looping / spline / scenario animations (walking crowds, intro and podium
    // actors, the Alpine hovercraft), roaming people, the instanced grandstand crowd and the billboard crowd sections. The running
    // game animates them (their vf19 updates are kept by reflex_world_ambient_mask while the other ambient skips stay); after each
    // frame the plugin reports every entity (class, model, drawn flag, draw distance, placement) and the world matrix of every node
    // of its skeleton; this draws them with their models and materials read from the player's install at run time.
    //  - Characters: one SkinnedMeshRenderer per entity (the model's LOD0, all bindings as submeshes), its bones (flat children of
    //    one root at the origin) set from the game's node world matrices by a parallel transform job.
    //  - Grandstand crowd (InstancedAnimationEntity): the original draws each template's one pose at every instance's matrix; here the
    //    template's posed mesh is baked once per frame (10 templates in Concord) and 1140 MeshRenderers share it, each at its static
    //    instance matrix (no per-instance work per frame beyond a distance test).
    //  - Billboard crowds: the section model, rigid at its entity matrix.
    // Hidden entities (model instance +0xa2 clear: idle crew chiefs, scenario actors waiting for their scenario) and entities beyond
    // their draw distance (ModelAsset +0x120 / renderer divisor, as Model_SubmitSkinned 007f4e50 culls) are not drawn.
    [DisallowMultipleComponent]
    public sealed class OriginalAmbient : MonoBehaviour
    {
        // elapsed milliseconds since a Stopwatch.GetTimestamp() (no Stopwatch object a frame, w-gcspikes)
        static double Ms(long since) => (System.Diagnostics.Stopwatch.GetTimestamp() - since) * 1000.0 / System.Diagnostics.Stopwatch.Frequency;
        public const int EntitySize = 304, MaxEntities = 4096, MaxMatrices = 16384;
        // Bits of reflex_world_ambient_mask (reflex_world.h REFLEX_AMBIENT_*). Default: every class, as the original runs them.
        // AnimEntitySplineAnimLoopWithParticles (0x40, the spline-carried particle emitters) draws from the CRT rand() sequence the
        // race shares, so skipping it would change the FR_Alpine race against the original (the full-ambient bike state
        // 0db7b3f22f360834 at 1500 frames is the game's own; 17c6677fd2d8a7f3 is the skip's).
        public const uint AllClasses = 0x3ff, DefaultMask = AllClasses;
        public const int KindBillboardCrowd = 8;
        public const uint FlagVisible = 1, FlagSkinned = 2, FlagInstance = 4, FlagTemplate = 8, FlagAnimated = 16;
        public static readonly string[] KindNames = { "AnimEntitySingleAnimLoop", "ScenarioAnimEntity", "AnimEntitySplineAnimLoop", "CrewChief", "InstancedAnimationEntity",
            "AnimEntitySingleAnimLoopWithParticles", "AnimEntitySplineAnimLoopWithParticles", "LandAnimalProximity", "BillboardCrowdEntity", "ScoreTowerFlagHolder",
            "TrackSideFlagHolder", "AnimEntitySplineWithStopLogic", "AnimEntitySplineWithStopLogicWithParticles", "AnimEntityProximity", "AnimEntityTrigger", "AirAnimalSoaring",
            "ThirtySecondBoard" };   // 16 (open core only): the National / Supercross start board girl, drawn like any skinned ambient actor

        [Tooltip("Classes whose original update runs (reflex_world_ambient_mask bits); set before the world is created.")]
        public uint mask = DefaultMask;
        [Tooltip("Classes the running world actually simulates (set by OriginalWorldRuntime: the mask, or all when the whole ambient animation runs).")]
        public uint simulatedMask = DefaultMask;
        public bool visible = true;
        [Tooltip("Reflex/Original Scenery (the level's materials); looked up by name when empty.")]
        public Shader sceneryShader;
        [Tooltip("Reflex/Original Crowd Billboard (the billboard crowd sections); looked up by name when empty.")]
        public Shader crowdShader;
        [Tooltip("Multiplies the original draw distances.")]
        public float distanceScale = 1f;
        public Camera viewer;

        [Header("Result (read only)")]
        public int entities; public int drawn; public int instancesDrawn; public int bones; public int posedBones; public int models; public int modelErrors;
        public double mainThreadMs;            // per Unity frame, averaged: applyMs + waitMs + bakeMs
        public double applyMs, waitMs, bakeMs; // export parse + visibility + job schedule; pose job completion wait; crowd template bakes
        [TextArea(2, 8)] public string report;

        /// The simulation thread's copy of one frame's export (OriginalSimulation.Results.ambient).
        public sealed class Frame
        {
            public readonly byte[] entities = new byte[MaxEntities * EntitySize];
            public readonly float[] matrices = new float[MaxMatrices * 16];
            public int count, matrixCount; public ReflexAmbientInfo info;
            // The scenario actors' node flags (open core: rx_session_ambient_node_flags; bit 0x10 hidden): nodeFlagCount[i] bytes from
            // nodeFlagStart[i] for entity i (0: none read: everything drawn).
            public readonly int[] nodeFlagStart = new int[MaxEntities], nodeFlagCount = new int[MaxEntities];
            public readonly byte[] nodeFlags = new byte[MaxNodeFlags];
            // The stands crowd's active camera flashes (open core: rx_session_crowd_flashes): the section (object id), the binding and seat of the quad.
            public readonly CrowdFlash[] flashes = new CrowdFlash[MaxFlashes];
            public int flashCount;
        }
        public const int MaxNodeFlags = 16384, MaxFlashes = 2048;
        public struct CrowdFlash { public uint obj; public int binding; public uint seat; public float age, length, emptyRatio; }

        /// The start gate of a converted MX vs ATV Alive race (the ScenarioAnimEntity TO_StartingGate: its model GL_TO_StartGate, JS7's
        /// FR_JSComp_StartGate; core alive_gate.c shows it, gives it its collision and drops it at the start with Alive's animation).
        static bool AliveStartGate(Entity e) => e.kind == 1 && (e.model == "GL_TO_StartGate" || e.model == "FR_JSComp_StartGate");

        /// Simulation thread, after a frame.
        public static void Collect(IntPtr world, Frame f)
        {
            int n = ReflexAmbientNative.reflex_world_ambient(world, f.entities, MaxEntities, f.matrices, MaxMatrices, ref f.info);
            f.count = Math.Max(0, n); f.matrixCount = Math.Min(f.info.matrices, MaxMatrices);
        }

        // ---------------------------------------------------------------------------------------- entities

        sealed class Entity
        {
            public uint obj; public int entityId, kind, source; public uint flags; public string model;
            public int bones, firstMatrix; public float lod; public float[] matrix = new float[16];
            public string texture, texturePackage;   // crew chief shirt (replaces CrewChief_ShirtColor_Default)
            public Material[] baseMaterials; public bool texturesChanged;
            // visuals
            public GameObject root; public Renderer renderer; public SkinnedMeshRenderer smr; public Transform[] boneTransforms;
            public Mesh baked;                 // templates: the posed mesh their instances share
            public Vector3 position;           // where the distance test is made (static entities / instances)
            public bool shown, anyShown, posed; public int boneOffset;
            public int index; public ReflexLevelContent.AmbientModel am; public Mesh ownMesh; public bool[] hiddenBindings; public bool anyHidden;   // node visibility (SetNodeVisibility)
            public bool billboard; public float[] param = new float[4];   // billboard crowd: c65 time, c66 emptyChairRatio, non-animated       // anyShown: a template with a drawn instance (baked only then)
        }
        readonly List<Entity> list = new List<Entity>();
        readonly Dictionary<uint, Entity> byObject = new Dictionary<uint, Entity>();
        ReflexLevelContent content; OriginalLevel level;
        Task<List<(string model, ReflexLevelContent.AmbientModel data)>> loading;
        bool built;
        GameObject rootObject;
        TransformAccessArray boneAccess; NativeArray<float> poseMatrices; NativeArray<int> boneSource; JobHandle poseJob; bool jobPending;
        readonly List<Entity> skinnedOrder = new List<Entity>();   // the bone ranges of boneAccess, in order
        readonly List<Entity> templates = new List<Entity>();
        int[] lastFirst = new int[0];

        /// A new world (OriginalWorldRuntime.Create): forget the previous one's entities.
        public void Begin(OriginalLevel lvl)
        {
            DestroyVisuals(); list.Clear(); byObject.Clear(); built = false; loading = null; colorTableSet = false;
            level = lvl; content = lvl ? lvl.Content : null;
        }

        static double Avg(double a, double v) => a == 0 ? v : a * 0.9 + v * 0.1;
        static string Str(byte[] b, int o, int n) { int z = Array.IndexOf(b, (byte)0, o, n); if (z < 0) z = o + n; return System.Text.Encoding.ASCII.GetString(b, o, z - o); }

        int[] words = new int[0];
        void Parse(Frame f)
        {
            if (list.Count != f.count) { if (built) Rebuild(); list.Clear(); byObject.Clear(); }
            const int W = EntitySize / 4;
            if (words.Length < f.count * W) words = new int[f.count * W];
            Buffer.BlockCopy(f.entities, 0, words, 0, f.count * EntitySize);
            for (int i = 0; i < f.count; i++)
            {
                int o = i * EntitySize, w = i * W;
                uint obj = (uint)words[w];
                Entity e;
                if (i < list.Count && list[i].obj == obj) e = list[i];
                else
                {
                    if (built) { Rebuild(); return; }   // the world's entity set changed (rescan): build again
                    e = new Entity { obj = obj, entityId = words[w + 1], kind = words[w + 2] };
                    e.model = Str(f.entities, o + 112, 64);
                    e.texture = Str(f.entities, o + 176, 64); e.texturePackage = Str(f.entities, o + 240, 64);
                    if (i < list.Count) list[i] = e; else list.Add(e);
                    byObject[obj] = e;
                }
                e.index = i;
                e.flags = (uint)words[w + 3]; e.bones = words[w + 4]; e.firstMatrix = words[w + 5]; e.source = words[w + 6];
                e.lod = BitConverter.Int32BitsToSingle(words[w + 7]);
                for (int k = 0; k < 4; k++) e.param[k] = BitConverter.Int32BitsToSingle(words[w + 8 + k]);
                if (e.kind == 3)   // crew chief: its shirt is set when it is put to use (FUN_006bd760)
                {
                    bool same = e.texture != null && f.entities[o + 176 + e.texture.Length] == 0;
                    for (int c = 0; same && c < e.texture.Length; c++) same = f.entities[o + 176 + c] == (byte)e.texture[c];
                    if (!same) { e.texture = Str(f.entities, o + 176, 64); e.texturePackage = Str(f.entities, o + 240, 64); e.texturesChanged = true; }
                }
                if (!built) Buffer.BlockCopy(f.entities, o + 48, e.matrix, 0, 64);
            }
        }

        void Rebuild()
        {
            DestroyVisuals(); built = false; loading = null;
        }

        /// The frame's export (main thread, right after OriginalSimulation.Collect).
        public void Apply(Frame f)
        {
            if (f == null || f.count == 0) return;
            long sw = System.Diagnostics.Stopwatch.GetTimestamp();
            Parse(f);
            entities = list.Count;
            if (!built)
            {
                if (loading == null) StartLoad();
                else if (loading.IsCompleted) Build(loading.Result);
            }
            if (built) { Pose(f); DrawFlashes(f); }
            applyMs = Avg(applyMs, Ms(sw));
        }

        IList<string> SceneMaterials(Entity e)
        {
            if (content == null) return null;
            int id = e.entityId;
            if ((e.flags & FlagTemplate) != 0)   // a run-time template: the materials of a scene instance that uses it
            {
                var inst = list.FirstOrDefault(x => (x.flags & FlagInstance) != 0 && x.source >= 0 && x.source < list.Count && list[x.source] == e);
                if (inst != null) id = inst.entityId;
            }
            var se = content.EntityById(id);
            return se != null && string.Equals(se.Asset("ModelAsset"), e.model, StringComparison.OrdinalIgnoreCase) ? se.MaterialList() : null;
        }

        void StartLoad()
        {
            if (content == null && Time.frameCount >= nextLevelLookup)
            {
                nextLevelLookup = Time.frameCount + 60;   // the level's content may still be loading
                if (!level) level = FindAnyObjectByType<OriginalLevel>();
                content = level ? level.Content : null;
            }
            if (content == null) return;
            var wanted = new Dictionary<string, HashSet<string>>(StringComparer.OrdinalIgnoreCase);
            foreach (var e in list)
            {
                if ((e.flags & FlagInstance) != 0 || string.IsNullOrEmpty(e.model)) continue;
                if (!wanted.TryGetValue(e.model, out var mats)) wanted[e.model] = mats = new HashSet<string>();
                var sl = SceneMaterials(e); if (sl != null) foreach (var m in sl) if (!string.IsNullOrEmpty(m)) mats.Add(m);
            }
            var c = content;
            loading = Task.Run(() =>
            {
                var r = new System.Collections.Concurrent.ConcurrentBag<(string, ReflexLevelContent.AmbientModel)>();
                Parallel.ForEach(wanted, new ParallelOptions { MaxDegreeOfParallelism = ReflexLevelContent.DecodeWorkers }, kv => r.Add((kv.Key, c.LoadAmbientModel(kv.Key, kv.Value))));
                return r.ToList();
            });
        }

        void Build(List<(string model, ReflexLevelContent.AmbientModel data)> loaded)
        {
            var shaders = new ReflexLevelContent.SceneryShaders { scenery = sceneryShader ? sceneryShader : Shader.Find("Reflex/Original Scenery") };
            var crowd = crowdShader ? crowdShader : Shader.Find("Reflex/Original Crowd Billboard");
            if (!crowd) Debug.LogWarning("OriginalAmbient: shader Reflex/Original Crowd Billboard not found; billboard crowds drawn with the scenery shader.");
            var models_ = loaded.ToDictionary(x => x.model, x => x.data, StringComparer.OrdinalIgnoreCase);
            models = models_.Count; modelErrors = models_.Values.Count(m => m.error != null);
            var sb = new System.Text.StringBuilder();
            foreach (var m in models_.Values.Where(m => m.error != null)) sb.Append("model ").Append(m.name).Append(": ").Append(m.error).Append('\n');
            rootObject = new GameObject("Original ambient entities (posed by the running game)");
            var groups = new Dictionary<int, Transform>();
            Transform Group(int kind)
            {
                if (groups.TryGetValue(kind, out var g)) return g;
                g = new GameObject(kind >= 0 && kind < KindNames.Length ? KindNames[kind] : "kind " + kind).transform; g.SetParent(rootObject.transform, false);
                return groups[kind] = g;
            }
            var bonesAll = new List<Transform>();
            skinnedOrder.Clear(); templates.Clear();
            int mismatched = 0, billboardsLogged = 0;
            foreach (var e in list)
            {
                if ((e.flags & FlagInstance) != 0) continue;
                if (!models_.TryGetValue(e.model ?? "", out var am) || am.error != null)
                {
                    if (AliveStartGate(e)) Debug.LogWarning($"[OriginalAmbient] the Alive start gate #{e.entityId} ({e.model}) is not drawn: its model {(am == null ? "was not loaded" : am.error)}", this);
                    continue;
                }
                if (AliveStartGate(e))
                    Debug.Log($"[OriginalAmbient] the Alive start gate #{e.entityId} ({e.model}): flags {e.flags}, {e.bones} nodes (the model {am.nodeCount}), draw distance {e.lod:0}, at ({e.matrix[12]:0.0}, {e.matrix[13]:0.0}, {e.matrix[14]:0.0}), materials {string.Join(",", content.AmbientMaterials(am, SceneMaterials(e), shaders).Select(m => m ? m.name : "null"))}", this);
                var mats = content.AmbientMaterials(am, SceneMaterials(e), shaders);
                e.baseMaterials = mats; e.texturesChanged = false;
                if (!string.IsNullOrEmpty(e.texture)) mats = Shirt(mats, e.texture, e.texturePackage);
                var go = new GameObject($"{e.model} #{e.entityId}"); go.transform.SetParent(Group(e.kind), false);
                e.root = go;
                if ((e.flags & FlagSkinned) != 0 && e.bones > 0)
                {
                    if (am.nodeCount != e.bones) { mismatched++; sb.Append($"{e.model}: model has {am.nodeCount} nodes, the game's node set {e.bones}\n"); Destroy(go); e.root = null; continue; }
                    e.boneTransforms = new Transform[e.bones];
                    for (int b = 0; b < e.bones; b++) { var t = new GameObject(am.nodeNames[b]).transform; t.SetParent(go.transform, false); e.boneTransforms[b] = t; }
                    // MAP SCALE: an Alive race's start gate (a ScenarioAnimEntity, w-alivegates) is part of the track: drawn at the world's scale about its
                    // own place, as its riders' GateStart slots and its bar's collision are (core alive_gate.c, build_activity_grid.c); the bones' world
                    // matrices from the core are real size about it (the characters keep their real size)
                    if (AliveStartGate(e) && Reflex.Port.RxCore.RxMapScale.Scaled)
                    {
                        float k = Reflex.Port.RxCore.RxMapScale.Current; var o = new Vector3(e.matrix[12], e.matrix[13], e.matrix[14]);
                        go.transform.localScale = Vector3.one * k; go.transform.localPosition = o * (1f - k);
                    }
                    var smr = go.AddComponent<SkinnedMeshRenderer>();
                    smr.sharedMesh = content.AmbientMesh(am, true); smr.sharedMaterials = mats; smr.bones = e.boneTransforms; smr.rootBone = e.boneTransforms[0];
                    e.am = am;
                    // bounds around the root node (the original culls by distance; Unity needs honest bounds far from the origin)
                    var rb = am.bindWorld[0].GetColumn(3); float radius = 0.5f;
                    foreach (var p in am.positions) radius = Mathf.Max(radius, ((Vector3)rb - p).magnitude);
                    smr.localBounds = new Bounds(Vector3.zero, Vector3.one * (radius * 2.4f));
                    smr.shadowCastingMode = ShadowCastingMode.On;
                    e.smr = smr; e.renderer = smr;
                    e.boneOffset = bonesAll.Count; e.posed = false;
                    bonesAll.AddRange(e.boneTransforms); skinnedOrder.Add(e);
                    if ((e.flags & FlagTemplate) != 0)
                    {
                        smr.forceRenderingOff = true;   // a pose source: its instances draw it
                        e.baked = new Mesh { name = e.model + " (crowd pose)", hideFlags = HideFlags.DontSave };
                        templates.Add(e);
                    }
                }
                else
                {
                    // billboard crowd section (and any entity without a skeleton): the model rigid at its entity matrix; the sections are
                    // seat billboards expanded by the Crowd programs (Reflex/Original Crowd Billboard)
                    ReflexLevelContent.ApplyMatrix(go.transform, e.matrix);
                    // MAP SCALE: a stands crowd section is part of the scenery: it grows with its stands (its seats' quads too: the crowd shader's
                    // RfxMapScale); the characters (crew chiefs, flaggers, the podium cast) keep their real size, as the riders
                    if (e.kind == KindBillboardCrowd && Reflex.Port.RxCore.RxMapScale.Scaled) go.transform.localScale *= Reflex.Port.RxCore.RxMapScale.Current;
                    go.AddComponent<MeshFilter>().sharedMesh = content.AmbientMesh(am, false);
                    var mr = go.AddComponent<MeshRenderer>(); mr.shadowCastingMode = ShadowCastingMode.Off;
                    if (e.kind == KindBillboardCrowd && crowd)
                    {
                        mr.sharedMaterials = content.CrowdMaterials(am, SceneMaterials(e), crowd); e.billboard = true;
                        if (!colorTableSet) { var table = content.CrowdColorTable(); if (table != null && table.Length == 24) { Shader.SetGlobalVectorArray("_RfxCrowdColorTable", table); colorTableSet = true; } }
                    }
                    else mr.sharedMaterials = mats;
                    e.renderer = mr; e.position = go.transform.position;
                    if (e.kind == KindBillboardCrowd) e.am = am;   // the seat meshes the camera flashes sit on (DrawFlashes)
                    if (billboardsLogged++ < 2)
                    {
                        var mesh = mr.GetComponent<MeshFilter>().sharedMesh;
                        int distinct = mesh ? mesh.vertices.Select(v => v).Distinct().Count() : 0;
                        sb.Append($"billboard {e.model}: {mesh?.vertexCount} vertices ({distinct} distinct), bounds {mesh?.bounds}, materials {string.Join(",", mats.Select(m => m ? m.name + "/" + (m.GetTexture("_BaseMap") ? m.GetTexture("_BaseMap").name : "-") : "null"))}\n");
                    }
                }
            }
            // grandstand crowd instances: the template's baked pose at the instance matrix
            int inst = 0;
            foreach (var e in list)
            {
                if ((e.flags & FlagInstance) == 0 || e.source < 0 || e.source >= list.Count) continue;
                var t = list[e.source];
                if (t.baked == null || t.smr == null) continue;
                var go = new GameObject($"{t.model} instance #{e.entityId}"); go.transform.SetParent(Group(e.kind), false);
                ReflexLevelContent.ApplyMatrix(go.transform, e.matrix);
                go.AddComponent<MeshFilter>().sharedMesh = t.baked;
                var mr = go.AddComponent<MeshRenderer>(); mr.sharedMaterials = t.smr.sharedMaterials; mr.shadowCastingMode = ShadowCastingMode.Off;
                e.root = go; e.renderer = mr;
                var tp = new Vector3(t.matrix[12], t.matrix[13], t.matrix[14]);   // the template's placement, carried to the instance
                e.position = go.transform.TransformPoint(tp);
                inst++;
            }
            boneAccess = new TransformAccessArray(bonesAll.ToArray());
            bones = bonesAll.Count;
            boneSource = new NativeArray<int>(Math.Max(1, bones), Allocator.Persistent);
            poseMatrices = new NativeArray<float>(MaxMatrices * 16, Allocator.Persistent);
            lastFirst = new int[0];
            foreach (var e in list) if (e.renderer) { e.renderer.enabled = false; e.shown = false; }
            built = true;
            foreach (var w in content.warnings.Take(12)) sb.Append("content warning: ").Append(w).Append('\n');
            report = $"{list.Count} entities, {skinnedOrder.Count} skinned ({templates.Count} crowd templates), {inst} crowd instances, {bones} bones, {models} models ({modelErrors} unreadable, {mismatched} node mismatches)\n" + sb;
            Debug.Log("[OriginalAmbient] " + report, this);
        }

        // FUN_006bd760: the crew chief's material texture CrewChief_ShirtColor_Default is replaced by the local rider's crew chief shirt
        // (the driver's texture, e.g. CrewChief_ShirtColor_Red of the bike's graphic kit package MX_MarsF_Lite_THQ_Red). The default is a
        // 2 x 2 A8R8G8B8 of (248, 85, 211), pink. Any texture slot of the material: the shirt's own program (Template_Body_JerseyDyn_Cool,
        // ReflexObjectShading.BuildLevel) binds its textures as _Tex0 .. _Tex11, the scenery factory as _BaseMap ...
        readonly Dictionary<string, Material[]> shirtMaterials = new Dictionary<string, Material[]>();
        Material[] Shirt(Material[] mats, string texture, string package)
        {
            string key = string.Join("|", mats.Select(m => m ? m.name : "")) + "|" + texture;
            if (shirtMaterials.TryGetValue(key, out var r)) return r;
            var tex = content.ExternalTexture(texture, package);
            r = mats.Select(m =>
            {
                if (!m || !tex) return m;
                string slot = m.GetTexturePropertyNames().FirstOrDefault(p => m.GetTexture(p) && string.Equals(m.GetTexture(p).name, "CrewChief_ShirtColor_Default", StringComparison.OrdinalIgnoreCase));
                if (slot == null) return m;
                var c = new Material(m) { name = m.name + " (" + texture + ")", hideFlags = HideFlags.DontSave };
                c.SetTexture(slot, tex); created.Add(c);
                return c;
            }).ToArray();
            return shirtMaterials[key] = r;
        }
        readonly List<UnityEngine.Object> created = new List<UnityEngine.Object>();
        bool colorTableSet; int nextLevelLookup;
        MaterialPropertyBlock crowdProps;
        static readonly int CrowdTimeId = Shader.PropertyToID("_CrowdTime"), EmptyChairId = Shader.PropertyToID("_EmptyChairRatio"), AnimatingId = Shader.PropertyToID("_IsAnimating");

        void Pose(Frame f)
        {
            if (jobPending) { poseJob.Complete(); jobPending = false; }
            // what is drawn: the game's flag, the draw distance from the viewer
            var cam = viewer ? viewer : Camera.main;
            Vector3 eye = cam ? cam.transform.position : Vector3.zero;
            drawn = 0; instancesDrawn = 0;
            if (crowdProps == null) crowdProps = new MaterialPropertyBlock();
            foreach (var t in templates) t.anyShown = false;
            foreach (var e in list)
            {
                if (!e.renderer) continue;
                // classes whose update is skipped (mask bit clear) keep the state the world was captured in: not drawn
                bool on = visible && (e.flags & FlagTemplate) == 0 && (e.kind >= 10 || (simulatedMask >> e.kind & 1) != 0);
                if ((e.flags & FlagInstance) != 0) on &= e.source >= 0 && e.source < list.Count && (list[e.source].flags & FlagVisible) != 0;
                else on &= (e.flags & FlagVisible) != 0;
                if (on && e.lod > 0)
                {
                    Vector3 p = e.position;
                    if ((e.flags & FlagSkinned) != 0 && e.firstMatrix >= 0 && e.firstMatrix < f.matrixCount) { int o = e.firstMatrix * 16; p = new Vector3(f.matrices[o + 12], f.matrices[o + 13], f.matrices[o + 14]); }
                    float d = e.lod * distanceScale; if (e.billboard) d *= Reflex.Port.RxCore.RxMapScale.Current;   // MAP SCALE: a crowd section's reach grows with it
                    on &= (p - eye).sqrMagnitude <= d * d;
                }
                if (on != e.shown)
                {
                    e.renderer.enabled = on; e.shown = on;
                    if (AliveStartGate(e)) Debug.Log($"[OriginalAmbient] the Alive start gate is {(on ? "drawn" : "hidden")} (visible flag {(e.flags & FlagVisible) != 0}, draw distance {e.lod * distanceScale:0})", this);
                }
                if (e.texturesChanged && e.baseMaterials != null)
                {
                    e.texturesChanged = false;
                    e.renderer.sharedMaterials = string.IsNullOrEmpty(e.texture) ? e.baseMaterials : Shirt(e.baseMaterials, e.texture, e.texturePackage);
                }
                if (on) { drawn++; if ((e.flags & FlagInstance) != 0) { instancesDrawn++; list[e.source].anyShown = true; } }
                if (on && e.smr && e.am != null) ApplyNodeFlags(e, f, e.index);
                if (on && e.billboard)
                {
                    crowdProps.SetFloat(CrowdTimeId, e.param[0]); crowdProps.SetFloat(EmptyChairId, e.param[1]); crowdProps.SetFloat(AnimatingId, e.param[2] != 0 ? 0f : 1f);
                    e.renderer.SetPropertyBlock(crowdProps);
                }
            }
            // bone -> exported matrix, only for skeletons that are drawn (or crowd templates with a drawn instance); the others keep
            // their last pose until they are drawn again
            bool layout = lastFirst.Length != skinnedOrder.Count;
            if (layout) lastFirst = new int[skinnedOrder.Count];
            posedBones = 0;
            for (int i = 0; i < skinnedOrder.Count; i++)
            {
                var e = skinnedOrder[i];
                bool pose = e.shown || e.anyShown;
                if (pose) posedBones += e.boneTransforms.Length;
                if (!layout && lastFirst[i] == e.firstMatrix && pose == e.posed) continue;
                lastFirst[i] = e.firstMatrix; e.posed = pose;
                for (int b = 0; b < e.boneTransforms.Length; b++)
                    boneSource[e.boneOffset + b] = pose && (e.flags & FlagSkinned) != 0 && e.firstMatrix + b < f.matrixCount ? e.firstMatrix + b : -1;
            }
            NativeArray<float>.Copy(f.matrices, poseMatrices, Math.Min(f.matrixCount, MaxMatrices) * 16);
            poseJob = new PoseJob { m = poseMatrices, src = boneSource }.Schedule(boneAccess);
            jobPending = true;
        }

        // ---- the stands crowd's camera flashes (open core: rx_session_crowd_flashes; the legacy plugin exports only their count)
        // XenonBillboardCrowdRenderEvent 006c7880 draws each ACTIVE flash record of every drawn crowd section as ONE quad of the section's seat
        // mesh (the LOD-0 binding the record belongs to; seat = the record's number % (vertices / 4)) with CameraFlashVS / CameraFlashPS
        // (Shaders/ReflexCrowdFlash.shader) and the level's "CameraFlash" texture. The quads of this frame are written into one dynamic mesh.
        [Header("Camera flashes (read only)")]
        public int flashesActive, flashesDrawn;
        public Shader flashShader;
        Mesh flashMesh; Material flashMaterial; MeshRenderer flashRenderer; bool flashTried;
        Vector3[] flashPos, flashNormal; Vector2[] flashUv0, flashUv1; int[] flashIndices;

        bool PrepareFlashes()
        {
            if (flashRenderer) return true;
            if (flashTried || !rootObject || content == null) return false;
            flashTried = true;
            var shader = flashShader ? flashShader : Shader.Find("Reflex/Original Crowd Flash");
            var tex = content.CameraFlashTexture();
            if (!shader || !tex) { if (!shader) Debug.LogWarning("OriginalAmbient: shader Reflex/Original Crowd Flash not found; no camera flashes."); return false; }
            flashMaterial = new Material(shader) { name = "CameraFlash (crowd)", hideFlags = HideFlags.DontSave };
            flashMaterial.SetTexture("_BaseMap", tex); created.Add(flashMaterial);
            flashMesh = new Mesh { name = "Crowd camera flashes", hideFlags = HideFlags.DontSave, indexFormat = IndexFormat.UInt16 }; flashMesh.MarkDynamic(); created.Add(flashMesh);
            flashPos = new Vector3[MaxFlashes * 4]; flashNormal = new Vector3[MaxFlashes * 4]; flashUv0 = new Vector2[MaxFlashes * 4]; flashUv1 = new Vector2[MaxFlashes * 4];
            flashIndices = new int[MaxFlashes * 6];
            for (int q = 0; q < MaxFlashes; q++)
            {   // the strip of 4 vertices (v0 v1 v2, v2 v1 v3)
                int v = q * 4, i = q * 6;
                flashIndices[i] = v; flashIndices[i + 1] = v + 1; flashIndices[i + 2] = v + 2; flashIndices[i + 3] = v + 2; flashIndices[i + 4] = v + 1; flashIndices[i + 5] = v + 3;
            }
            var go = new GameObject("Crowd camera flashes"); go.transform.SetParent(rootObject.transform, false);
            go.AddComponent<MeshFilter>().sharedMesh = flashMesh;
            flashRenderer = go.AddComponent<MeshRenderer>();
            flashRenderer.sharedMaterial = flashMaterial; flashRenderer.shadowCastingMode = ShadowCastingMode.Off; flashRenderer.receiveShadows = false;
            flashRenderer.lightProbeUsage = LightProbeUsage.Off; flashRenderer.reflectionProbeUsage = ReflectionProbeUsage.Off; flashRenderer.allowOcclusionWhenDynamic = false;
            flashRenderer.enabled = false;
            return true;
        }

        void DrawFlashes(Frame f)
        {
            flashesActive = f.flashCount; flashesDrawn = 0;
            if (f.flashCount == 0 && !flashRenderer) return;
            if (!PrepareFlashes()) return;
            int n = 0;
            if (visible)
                for (int i = 0; i < f.flashCount && n < MaxFlashes; i++)
                {
                    var fl = f.flashes[i];
                    if (!byObject.TryGetValue(fl.obj, out var e) || !e.shown || !e.billboard || e.am == null || !e.root) continue;   // a section the viewer does not draw
                    var am = e.am;
                    if (fl.binding < 0 || fl.binding >= am.bindingVertexCount.Count) continue;
                    int seats = am.bindingVertexCount[fl.binding] / 4;
                    if (seats <= 0) continue;
                    int v0 = am.bindingFirstVertex[fl.binding] + 4 * (int)(fl.seat % (uint)seats);   // DrawPrimitive(TRIANGLESTRIP, 4 * (record % (vertices / 4)), 2)
                    var m = e.root.transform.localToWorldMatrix;                                    // gWorld c0
                    for (int k = 0; k < 4; k++)
                    {
                        int o = n * 4 + k; var p = am.positions[v0 + k];
                        flashPos[o] = m.MultiplyPoint3x4(p);
                        flashNormal[o] = am.normals[v0 + k];                                         // raw: CameraFlashVS does not transform it
                        flashUv0[o] = am.uv0[v0 + k];
                        flashUv1[o] = new Vector2(fl.emptyRatio, p.magnitude);                       // c66, |position| (dp3 v0, v0)
                    }
                    n++;
                }
            flashesDrawn = n;
            flashMesh.Clear(false);
            if (n == 0) { flashRenderer.enabled = false; return; }
            flashMesh.SetVertices(flashPos, 0, n * 4); flashMesh.SetNormals(flashNormal, 0, n * 4);
            flashMesh.SetUVs(0, flashUv0, 0, n * 4); flashMesh.SetUVs(1, flashUv1, 0, n * 4);
            flashMesh.SetIndices(flashIndices, 0, n * 6, MeshTopology.Triangles, 0, false);
            flashMesh.bounds = new Bounds(Vector3.zero, Vector3.one * 2e6f);   // the quads are expanded in the vertex program
            flashRenderer.enabled = true;
        }

        // A scenario script hides parts of an actor (SetNodeVisibility 0055dff0: the node record's +0xc bit 0x10; the renderer skips the
        // bindings of a hidden node): the actor's own copy of the mesh gets those bindings' submeshes emptied (the shared mesh is untouched).
        static readonly int[] NoTriangles = new int[0];
        void ApplyNodeFlags(Entity e, Frame f, int index)
        {
            int n = index >= 0 && index < f.count ? f.nodeFlagCount[index] : 0, start = n > 0 ? f.nodeFlagStart[index] : 0;
            var nodes = e.am.bindingNodes; int bindings = nodes.Count;
            if (e.hiddenBindings == null || e.hiddenBindings.Length != bindings) e.hiddenBindings = new bool[bindings];
            bool changed = false, any = false;
            for (int k = 0; k < bindings; k++)
            {
                int node = nodes[k];
                bool hide = n > 0 && node >= 0 && node < n && (f.nodeFlags[start + node] & 0x10) != 0;
                any |= hide;
                if (hide != e.hiddenBindings[k]) { e.hiddenBindings[k] = hide; changed = true; }
            }
            if (!changed) return;
            e.anyHidden = any;
            var shared = content.AmbientMesh(e.am, true);
            if (!any) { e.smr.sharedMesh = shared; return; }
            if (!e.ownMesh) { e.ownMesh = Instantiate(shared); e.ownMesh.name = shared.name + " (" + e.entityId + ")"; e.ownMesh.hideFlags = HideFlags.DontSave; created.Add(e.ownMesh); }
            for (int k = 0; k < bindings && k < e.ownMesh.subMeshCount; k++) e.ownMesh.SetTriangles(e.hiddenBindings[k] ? NoTriangles : e.am.triangles[k], k, false);
            e.smr.sharedMesh = e.ownMesh;
        }

        void LateUpdate()
        {
            if (!jobPending) return;
            long sw = System.Diagnostics.Stopwatch.GetTimestamp();
            poseJob.Complete(); jobPending = false;
            double wait = Ms(sw);
            foreach (var t in templates) if (t.smr && t.anyShown) t.smr.BakeMesh(t.baked, true);
            waitMs = Avg(waitMs, wait); bakeMs = Avg(bakeMs, Ms(sw) - wait);
            mainThreadMs = applyMs + waitMs + bakeMs;
        }

        struct PoseJob : IJobParallelForTransform
        {
            [ReadOnly] public NativeArray<float> m;
            [ReadOnly] public NativeArray<int> src;
            public void Execute(int i, TransformAccess t)
            {
                int s = src[i]; if (s < 0) return;
                int o = s * 16;
                var x = new Vector3(m[o], m[o + 1], m[o + 2]); var y = new Vector3(m[o + 4], m[o + 5], m[o + 6]); var z = new Vector3(m[o + 8], m[o + 9], m[o + 10]);
                var p = new Vector3(m[o + 12], m[o + 13], m[o + 14]);
                float sx = x.magnitude, sy = y.magnitude, sz = z.magnitude;
                if (!(sx > 1e-8f && sy > 1e-8f && sz > 1e-8f) || float.IsNaN(p.x) || float.IsInfinity(p.x)) return;
                bool mirrored = Vector3.Dot(Vector3.Cross(x, y), z) < 0;
                t.localPosition = p; t.localRotation = Quaternion.LookRotation(z / sz, y / sy);
                t.localScale = new Vector3(mirrored ? -sx : sx, sy, sz);
            }
        }

        void DestroyVisuals()
        {
            if (jobPending) { poseJob.Complete(); jobPending = false; }
            if (boneAccess.isCreated) boneAccess.Dispose();
            if (boneSource.IsCreated) boneSource.Dispose();
            if (poseMatrices.IsCreated) poseMatrices.Dispose();
            foreach (var e in list) { if (e.baked) Destroy(e.baked); e.baked = null; e.root = null; e.renderer = null; e.smr = null; e.boneTransforms = null; e.ownMesh = null; e.hiddenBindings = null; e.am = null; }
            if (rootObject) Destroy(rootObject);
            foreach (var o in created) if (o) Destroy(o); created.Clear(); shirtMaterials.Clear();
            rootObject = null; skinnedOrder.Clear(); templates.Clear();
            flashRenderer = null; flashMesh = null; flashMaterial = null; flashTried = false;   // destroyed with the root / created list
        }

        void OnDestroy() => DestroyVisuals();
        void OnDisable() { if (jobPending) { poseJob.Complete(); jobPending = false; } }
    }
}
