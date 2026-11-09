using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading;
using UnityEngine;
using Reflex.Port.Content;
using Reflex.Port.RxCore;
using Reflex.Port.RxCore.Bridge;

namespace Reflex.Port.OriginalWorld
{
    [StructLayout(LayoutKind.Sequential)]
    public struct ReflexPropInfo { public int props, reported, objects, pending; }

    public static class ReflexPropsNative
    {
        public const int PropSize = 80, TextureAnimSize = 104;
        public const uint FlagVisible = 1, FlagMoved = 2, FlagNew = 4;
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_props__fn(IntPtr world, [Out] byte[] props, uint max, int all, ref ReflexPropInfo info);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_props__fn> reflex_world_props__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_props__fn>("reflex_world_props");
        public static int reflex_world_props(IntPtr world, [Out] byte[] props, uint max, int all, ref ReflexPropInfo info) => reflex_world_props__p.Get()(world, props, max, all, ref info);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_props_rescan__fn(IntPtr world);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_props_rescan__fn> reflex_world_props_rescan__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_props_rescan__fn>("reflex_world_props_rescan");
        public static int reflex_world_props_rescan(IntPtr world) => reflex_world_props_rescan__p.Get()(world);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_texture_anims__fn(IntPtr world, [Out] byte[] anims, uint max);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_texture_anims__fn> reflex_world_texture_anims__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_texture_anims__fn>("reflex_world_texture_anims");
        public static int reflex_world_texture_anims(IntPtr world, [Out] byte[] anims, uint max) => reflex_world_texture_anims__p.Get()(world, anims, max);
    }

    // The level's physics props and animated textures as the running original game moves them (native/drafts/world_runtime/props.cpp,
    // _re/names/87_props.tsv, _re/notes/props.md), plus the original draw distance / LOD rules (OriginalSceneryLod) and the night sky
    // (Template_Luma_Sky_Nighttime: Content/ReflexAnimatedTextures.cs, Shaders/ReflexOriginalSkyNight.shader). One call registers all of
    // it: OriginalProps.Attach(level) after the scenery is built (OriginalLevel.LoadNow).
    //  - Props: the scene's PhysicsRSData (bales, tyre stacks, cones, barrels, fences: about 12,000 on 42 levels), PopUpBaleRSData and
    //    PopDownBaleRSData instances are the game's PhysicsEntity objects (entity id +0x14 = the scene Id = SceneryInstance.entityId). On
    //    the simulation thread between two frames (OriginalSimulation.BetweenFrames, once per Unity frame at most) the plugin reports the
    //    props whose world matrix (+0x30) or drawn flag (ModelInstance +0xa2) changed since the last report; here the matching scenery
    //    objects take the matrix and the flag (a pop-up bale stays hidden 5 m under its place until its trigger fires, then rises).
    //  - Scrolling textures: the game's gTextureOffset of every ScrollingTextureEntity (its vf19 runs in the world) goes to the instance's
    //    renderer as _RfxScrollOverride = (u, v, 1, 0) (the scenery shader's SX_KI_LightBeam hook) and _RfxTextureOffset (the same, for
    //    the other scrolling templates: their uv0); without a running world the same formula runs on the
    //    elapsed time (ReflexLevelContent.ScrollOffset). Entities the game does not draw (+0xa2 clear: hill-climb chevrons before the
    //    challenge) are hidden.
    //  - TextureAnimEntity: the frame the game picked (texture data +0x18) is bound in the materials using that flipbook texture.
    // Read only on the guest: the bike state hashes are unchanged (props_test, _re/notes/props.md).
    [DisallowMultipleComponent]
    public sealed class OriginalProps : MonoBehaviour
    {
        public const int MaxProps = 16384, MaxTextureAnims = 1024;
        static readonly string[] PropTypes = { "PhysicsRSData", "PopUpBaleRSData", "PopDownBaleRSData" };

        public OriginalLevel level;
        public OriginalWorldRuntime runtime;
        [Tooltip("Move the knocked props (and show / hide the pop-up bales) as the running game does.")]
        public bool moveProps = true;
        [Tooltip("Scrolling textures and texture animations from the running game (the formula on the clock without a world).")]
        public bool animateTextures = true;

        [Header("Result (read only)")]
        public int sceneProps; public int gameProps; public int matched; public int moved; public int hidden; public int scrolling; public int flipbooks;
        public int updatesLastFrame; public double nativeMilliseconds;
        [TextArea(2, 6)] public string report;

        /// Registers the props, animated textures, original LOD / draw distance and night sky for a loaded level. Main thread, after
        /// the scenery is built.
        public static OriginalProps Attach(OriginalLevel lvl)
        {
            try { return AttachNow(lvl); }
            catch (Exception e) { Debug.LogWarning("OriginalProps: " + e.Message + "\n" + e.StackTrace, lvl); return null; }   // never stops the level load
        }

        static OriginalProps AttachNow(OriginalLevel lvl)
        {
            if (!lvl || lvl.Content == null || !lvl.SceneryRoot) return null;
            var p = lvl.GetComponent<OriginalProps>() ?? lvl.gameObject.AddComponent<OriginalProps>();
            p.level = lvl; p.runtime = lvl.runtime;
            p.Begin();
            var lod = lvl.GetComponent<OriginalSceneryLod>() ?? lvl.gameObject.AddComponent<OriginalSceneryLod>();
            lod.Begin(lvl);
            p.lod = lod;
            try
            {
                var night = Shader.Find("Reflex/Original Sky Night");
                if (lvl.Content.BuildNightSky(night)) p.report += "\nnight sky: Template_Luma_Sky_Nighttime port";
            }
            catch (Exception e) { Debug.LogWarning("OriginalProps night sky: " + e.Message, lvl); }
            return p;
        }

        OriginalSceneryLod lod;
        ReflexLevelContent content;
        readonly Dictionary<int, Transform> propById = new Dictionary<int, Transform>();
        readonly Dictionary<int, List<ReflexLevelContent.ScrollingInstance>> scrollById = new Dictionary<int, List<ReflexLevelContent.ScrollingInstance>>();
        List<ReflexLevelContent.ScrollingInstance> scrollList = new List<ReflexLevelContent.ScrollingInstance>();
        float startTime;
        MaterialPropertyBlock mpb;
        static readonly int ScrollId = Shader.PropertyToID("_RfxScrollOverride"), OffsetId = Shader.PropertyToID("_RfxTextureOffset");

        // simulation thread -> main thread
        struct PropUpdate { public uint flags; public float[] matrix; }
        readonly object gate = new object();
        readonly Dictionary<int, PropUpdate> pending = new Dictionary<int, PropUpdate>();
        byte[] textureFrame = new byte[0]; int textureCount; bool textureFresh;
        int gameCount;
        int queued; IReflexSimDriver boundSim; bool wantAll = true;
        RxProp[] coreProps = new RxProp[2048];
        readonly Dictionary<int, PropUpdate> lastCoreProps = new Dictionary<int, PropUpdate>();
        readonly float[] coreMatrix = new float[16];
        readonly HashSet<int> matchedIds = new HashSet<int>();
        readonly byte[] propBuffer = new byte[2048 * ReflexPropsNative.PropSize];
        readonly byte[] animBuffer = new byte[MaxTextureAnims * ReflexPropsNative.TextureAnimSize];
        bool propsExport = true, textureExport = true;
        Action<IntPtr> collectCore, collectLegacy;
        readonly HashSet<int> movedSet = new HashSet<int>();

        void Begin()
        {
            content = level.Content; propById.Clear(); scrollById.Clear(); movedSet.Clear(); hiddenSet.Clear(); matchedIds.Clear(); matched = 0; lock (gate) { lastCoreProps.Clear(); pending.Clear(); textureCount = 0; }
            foreach (var inst in content.instances)
                if (inst.transform && Array.IndexOf(PropTypes, inst.entityType) >= 0) propById[inst.entityId] = inst.transform;
            scrollList = content.ScrollingInstances();
            foreach (var s in scrollList) { if (!scrollById.TryGetValue(s.entityId, out var l)) scrollById[s.entityId] = l = new List<ReflexLevelContent.ScrollingInstance>(); l.Add(s); }
            sceneProps = propById.Count; scrolling = scrollList.Count; startTime = Time.time;
            boundSim = null; wantAll = true;
            report = $"{sceneProps} props, {scrolling} scrolling-texture instances";
        }

        // Simulation thread, between two frames.
        void Collect(IntPtr w, bool open)
        {
            try
            {
                var watch = System.Diagnostics.Stopwatch.StartNew();
                if (open)
                {
                    if (propsExport && moveProps)
                    {
                        int total = RxApiNative.rx_session_props(w, coreProps, (uint)coreProps.Length);
                        if (total > coreProps.Length)
                        {
                            Array.Resize(ref coreProps, total);
                            total = RxApiNative.rx_session_props(w, coreProps, (uint)coreProps.Length);
                        }
                        lock (gate)
                        {
                            gameCount = Math.Max(0, total);
                            for (int i = 0; i < Math.Min(total, coreProps.Length); i++)
                            {
                                var p = coreProps[i];
                                p.CopyMatrix(coreMatrix);
                                bool changed = !lastCoreProps.TryGetValue(p.entity_id, out var old) || (old.flags & 1) != (p.flags & 1);
                                if (!changed) for (int j = 0; j < 16; j++) if (old.matrix[j] != coreMatrix[j]) { changed = true; break; }
                                if (!changed) continue;
                                var update = new PropUpdate { flags = p.flags, matrix = (float[])coreMatrix.Clone() };
                                lastCoreProps[p.entity_id] = update;
                                pending[p.entity_id] = update;
                            }
                        }
                    }
                    nativeMilliseconds = nativeMilliseconds == 0 ? watch.Elapsed.TotalMilliseconds : nativeMilliseconds * 0.95 + watch.Elapsed.TotalMilliseconds * 0.05;
                    return;
                }
                if (propsExport && moveProps)
                {
                    var info = new ReflexPropInfo(); int guard = 0;
                    do
                    {
                        int n = ReflexPropsNative.reflex_world_props(w, propBuffer, (uint)(propBuffer.Length / ReflexPropsNative.PropSize), wantAll ? 1 : 0, ref info);
                        wantAll = false;
                        if (n <= 0) break;
                        lock (gate)
                        {
                            for (int i = 0; i < n; i++)
                            {
                                int o = i * ReflexPropsNative.PropSize;
                                int id = BitConverter.ToInt32(propBuffer, o + 4);
                                var m = new float[16]; Buffer.BlockCopy(propBuffer, o + 16, m, 0, 64);
                                pending[id] = new PropUpdate { flags = BitConverter.ToUInt32(propBuffer, o + 12), matrix = m };
                            }
                            gameCount = info.props;
                        }
                    } while (info.pending > 0 && ++guard < 16);
                }
                if (textureExport && animateTextures)
                {
                    int n = ReflexPropsNative.reflex_world_texture_anims(w, animBuffer, MaxTextureAnims);
                    lock (gate)
                    {
                        if (textureFrame.Length < animBuffer.Length) textureFrame = new byte[animBuffer.Length];
                        textureCount = Math.Max(0, n);
                        Buffer.BlockCopy(animBuffer, 0, textureFrame, 0, textureCount * ReflexPropsNative.TextureAnimSize);
                        textureFresh = true;
                    }
                }
                nativeMilliseconds = nativeMilliseconds == 0 ? watch.Elapsed.TotalMilliseconds : nativeMilliseconds * 0.95 + watch.Elapsed.TotalMilliseconds * 0.05;
            }
            catch (EntryPointNotFoundException) { propsExport = textureExport = false; }   // an older plugin
            catch (Exception e) { Debug.LogWarning("OriginalProps: " + e.Message); }
            finally { Interlocked.Exchange(ref queued, 0); }
        }

        readonly List<KeyValuePair<int, PropUpdate>> drain = new List<KeyValuePair<int, PropUpdate>>();
        byte[] texScratch = new byte[0];

        void LateUpdate()
        {
            if (!level || level.Content == null) return;
            if (level.Content != content) Begin();
            if (!runtime) runtime = level.runtime ? level.runtime : FindAnyObjectByType<OriginalWorldRuntime>();
            var sim = runtime && runtime.Running ? runtime.SimDriver : null;
            if (sim != boundSim) { boundSim = sim; wantAll = true; lock (gate) lastCoreProps.Clear(); Interlocked.Exchange(ref queued, 0); }
            collectCore ??= w => Collect(w, true);
            collectLegacy ??= w => Collect(w, false);
            if (sim != null && (propsExport || textureExport) && Interlocked.Exchange(ref queued, 1) == 0)
                sim.BetweenFrames(sim.Mode == ReflexSimMode.Open ? collectCore : collectLegacy);

            drain.Clear(); int texN = 0; bool texFresh;
            lock (gate)
            {
                foreach (var kv in pending) drain.Add(kv);
                pending.Clear();
                texFresh = textureFresh; textureFresh = false;
                if (texFresh) { if (texScratch.Length < textureFrame.Length) texScratch = new byte[textureFrame.Length]; texN = textureCount; Buffer.BlockCopy(textureFrame, 0, texScratch, 0, texN * ReflexPropsNative.TextureAnimSize); }
                gameProps = gameCount;
            }
            updatesLastFrame = drain.Count;
            foreach (var kv in drain)
            {
                if (!propById.TryGetValue(kv.Key, out var t) || !t) continue;
                var u = kv.Value;
                if (matchedIds.Add(kv.Key)) matched = matchedIds.Count;
                ReflexLevelContent.ApplyMatrix(t, u.matrix);
                if ((u.flags & ReflexPropsNative.FlagMoved) != 0) movedSet.Add(kv.Key); else movedSet.Remove(kv.Key);
                moved = movedSet.Count;
                bool hide = (u.flags & ReflexPropsNative.FlagVisible) == 0;
                SetHidden(t, hide);
            }

            if (!animateTextures) return;
            mpb ??= new MaterialPropertyBlock();
            if (sim != null && sim.Mode == ReflexSimMode.Legacy && textureExport)
            {
                if (!texFresh) return;
                for (int i = 0; i < texN; i++)
                {
                    int o = i * ReflexPropsNative.TextureAnimSize;
                    int id = BitConverter.ToInt32(texScratch, o + 4), kind = BitConverter.ToInt32(texScratch, o + 8);
                    uint flags = BitConverter.ToUInt32(texScratch, o + 12);
                    if (kind == 0)
                    {
                        if (!scrollById.TryGetValue(id, out var l)) continue;
                        var off = new Vector4(BitConverter.ToSingle(texScratch, o + 16), BitConverter.ToSingle(texScratch, o + 20), 1, 0);
                        foreach (var s in l) { SetScroll(s.renderer, off); SetHidden(s.renderer.transform, (flags & 1) == 0); }
                    }
                    else if (kind == 1)
                    {
                        int z = Array.IndexOf(texScratch, (byte)0, o + 40, 64); if (z < 0) z = o + 104;
                        string target = System.Text.Encoding.ASCII.GetString(texScratch, o + 40, z - o - 40);
                        var fb = content.FlipbookFor(target);
                        if (fb != null) { fb.Show((int)BitConverter.ToSingle(texScratch, o + 24)); flipbooks = 1; }
                    }
                }
            }
            else if (sim != null && sim.Mode == ReflexSimMode.Open && runtime && runtime.GameScrolling) { }   // the open core: the game's own offsets (OriginalSceneryModels)
            else
            {
                // no running world (front end, edit-mode tests): the game's formula on the clock since the level loaded
                double t = Time.time - startTime;
                foreach (var s in scrollList) SetScroll(s.renderer, new Vector4(ReflexLevelContent.ScrollOffset(s.periodU, t), ReflexLevelContent.ScrollOffset(s.periodV, t), 1, 0));
            }
        }

        void SetScroll(Renderer r, Vector4 v)
        {
            if (!r) return;
            r.GetPropertyBlock(mpb); mpb.SetVector(ScrollId, v); mpb.SetVector(OffsetId, v); r.SetPropertyBlock(mpb);
        }

        /// Diagnostics: the props the game moved, with how far from their scene placement (m) and the rotation (degrees).
        public string MovedReport()
        {
            var sb = new System.Text.StringBuilder();
            foreach (var id in movedSet)
            {
                var inst = content?.instances.Find(i => i.entityId == id);
                if (inst == null || !inst.transform) continue;
                var m = inst.matrix; var p0 = new Vector3(m[12], m[13], m[14]);
                var r0 = Quaternion.LookRotation(new Vector3(m[8], m[9], m[10]), new Vector3(m[4], m[5], m[6]));
                sb.Append($"{id} {inst.name}: {(inst.transform.position - p0).magnitude:F3} m, {Quaternion.Angle(r0, inst.transform.rotation):F1} deg; ");
            }
            return sb.ToString();
        }

        readonly HashSet<Transform> hiddenSet = new HashSet<Transform>();
        /// Not drawn by the running game (ModelInstance +0xa2 clear). OriginalSceneryLod asks every frame.
        public bool IsHidden(Transform t) => hiddenSet.Contains(t);
        void SetHidden(Transform t, bool hide)
        {
            if (!t) return;
            if (hide) hiddenSet.Add(t); else hiddenSet.Remove(t);
            hidden = hiddenSet.Count;
            if (lod && lod.isActiveAndEnabled) return;   // OriginalSceneryLod owns forceRenderingOff (it reads IsHidden)
            var r = t.GetComponent<Renderer>(); if (r && r.forceRenderingOff != hide) r.forceRenderingOff = hide;
        }
    }
}
