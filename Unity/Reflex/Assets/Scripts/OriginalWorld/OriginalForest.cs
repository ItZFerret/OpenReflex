using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;
using Reflex.Port.Content;
using Reflex.Port.Rendering;

namespace Reflex.Port.OriginalWorld
{
    // The forest of the level (Content/ReflexForest.cs: data read from the install), drawn for the game camera as the original
    // ForestSystem does (_re/notes/forest.md):
    //   - the cluster pass (FUN_00662ec0, the forest's worker thread): the 400 clusters are frustum-culled (sphere of the box, 6
    //     planes, -0.01); a cluster whose nearest depth along the camera's forward is within the largest 3D distance of all types
    //     (ForestSystem +0x3078) has its trees tested one by one (FUN_006676d0: the tree's box in the frustum, its depth
    //     d = (position - camera) . forward, the first LOD record whose distance is beyond d);
    //   - the 3D trees (FUN_00660720 / FUN_0065f6d0): per tree gWorld (rotation about y, scale), g_TreeAlphaTests (the dissolve of
    //     branches, fronds, leaves and the next leaf LOD at depth d), then branches (gTreeLODParams3 = x), fronds (y, always two-sided:
    //     SetRenderState(CULLMODE, NONE) in FUN_00672a00) and leaves (z; on a transition record also the next leaf LOD with w);
    //     here every (type, record) is one Graphics.RenderMeshPrimitives call per geometry over a per-camera instance buffer;
    //   - the billboards (XenonForestAsset::vf4 00661000): every tree as one instanced quad (Reflex/Tree Billboard), one draw for the
    //     whole forest: the original's per-cluster draws only skip clusters whose billboards the vertex program collapses anyway.
    // Dynamic shadows: the original draws billboard shadow imposters into its VSM (TreeShader ShadowBB*); the port's dynamic shadows
    // are Unity's main-light shadows, so the 3D trees within the shadow distance cast Unity shadows instead (castShadows).
    [DisallowMultipleComponent]
    public sealed class OriginalForest : MonoBehaviour
    {
        public OriginalLevel level;
        [Tooltip("The camera the forest is drawn for (default: the runtime's chase camera, else Camera.main, else every game camera).")]
        public Camera viewer;
        public bool draw3d = true, drawBillboards = true, drawGrass = true;
        [Tooltip("Tests: the wind's four up vectors of a recorded frame (g_UpVectors c61..c64) instead of the clock.")]
        public static Vector4[] grassUpOverride;
        [Tooltip("3D trees within the URP shadow distance cast Unity main-light shadows (stand-in for the billboard shadow imposters).")]
        public bool castShadows = true;
        [Tooltip("Upload the per-frame instance data with LockBufferForWrite into a ring of buffers (else GraphicsBuffer.SetData).")]
        public bool lockedUpload = true;

        [Header("Result (read only)")]
        public int trees; public int types; public int clustersVisible; public int trees3d; public int draws3d; public int shadowTrees;
        public double milliseconds;
        [Tooltip("Last frame: billboards (constants + draw), cluster / tree selection, instance upload, 3D draw submission (ms).")]
        public double msBillboards, msSelect, msUpload, msSubmit, msLast, msGrass;
        public int grassCellsFar, grassCellsNear;
        public string grassReport;
        public int camerasLastFrame; int frameSeen = -1, camerasThisFrame;
        [TextArea(2, 8)] public string report;

        ReflexLevelContent content;
        ForestBuild build;
        Mesh quad;
        GraphicsBuffer bbBuffer;
        Bounds worldBounds;
        // per (type, block): the material of each submesh as drawn (ForestBuild.partMaterials; fronds and Alive's 3D leaves two-sided: CULLMODE NONE)
        Material[,][] drawMaterials;
        readonly Dictionary<Material, Material> noCull = new Dictionary<Material, Material>();
        readonly List<MaterialPropertyBlock> blocks = new List<MaterialPropertyBlock>();
        MaterialPropertyBlock bbBlock;
        bool subscribed;
        readonly Plane[] planes = new Plane[6];

        // per camera: a ring of instance buffers written with LockBufferForWrite (no wait on a buffer the GPU may still read)
        sealed class CamBuffers { public readonly GraphicsBuffer[] ring = new GraphicsBuffer[3]; public int capacity, next; }
        readonly Dictionary<Camera, CamBuffers> perCamera = new Dictionary<Camera, CamBuffers>();

        // scratch (no per-frame allocation)
        int[] groupCount = Array.Empty<int>(), groupStart = Array.Empty<int>(), groupFill = Array.Empty<int>();
        Vector4[] gpu = Array.Empty<Vector4>();
        int maxRecords;

        static readonly int InstancesId = Shader.PropertyToID("_RfxTreeInstances"), BaseId = Shader.PropertyToID("_RfxTreeBase"),
            Lod1Id = Shader.PropertyToID("_RfxTreeLod1"), Lod2Id = Shader.PropertyToID("_RfxTreeLod2"), SelId = Shader.PropertyToID("_RfxTreeSel"),
            LeafId = Shader.PropertyToID("_RfxTreeLeaf"), BBInstancesId = Shader.PropertyToID("_RfxBBInstances"), BBTypesId = Shader.PropertyToID("_RfxBBTreeTypes"),
            CamPosId = Shader.PropertyToID("_RfxTreeCamPos"), CamRightId = Shader.PropertyToID("_RfxTreeCamRight"), CamUpId = Shader.PropertyToID("_RfxTreeCamUp"),
            CamFwdId = Shader.PropertyToID("_RfxTreeCamFwd"), FoliageDitherId = Shader.PropertyToID("_RfxFoliageDither"), BBXId = Shader.PropertyToID("_RfxBBWorldX"), BBYId = Shader.PropertyToID("_RfxBBWorldY"),
            BBZId = Shader.PropertyToID("_RfxBBWorldZ"), BBSunId = Shader.PropertyToID("_RfxBBTangentSun"), CullId = Shader.PropertyToID("_Cull");
        static readonly int[] BBLightIds = { Shader.PropertyToID("_RfxBBTangentLight0"), Shader.PropertyToID("_RfxBBTangentLight1"), Shader.PropertyToID("_RfxBBTangentLight2"), Shader.PropertyToID("_RfxBBTangentLight3") };

        /// Tests: render a level without its forest (the "before" image of the comparison).
        public static bool disabledForTests;

        // MX vs ATV Alive's foliage alpha (ReflexDrawDistance.hlsl RfxFoliageClip): coverage - screen noise > 32 / 255, the noise spanning the rest
        // (1 - 32 / 255) so a texel of alpha 1 at full fade always stays. REFLEX_ALIVE_TREE_DITHER (tests): "off" (the alpha test as on Reflex's
        // trees) or "threshold,span".
        static readonly Vector2? FoliageOverride = ParseFoliage(Environment.GetEnvironmentVariable("REFLEX_ALIVE_TREE_DITHER"));
        static Vector2? ParseFoliage(string v)
        {
            if (string.IsNullOrEmpty(v)) return null;
            if (v == "off") return Vector2.zero;
            var p = v.Split(',');
            var ci = System.Globalization.CultureInfo.InvariantCulture;
            return p.Length == 2 && float.TryParse(p[0], System.Globalization.NumberStyles.Float, ci, out float t) && float.TryParse(p[1], System.Globalization.NumberStyles.Float, ci, out float a) ? new Vector2(t, a) : (Vector2?)null;
        }
        bool aliveFoliage;
        void SetFoliageDither()
        {
            var ta = FoliageOverride ?? new Vector2(32f / 255f, 1f - 32f / 255f);
            bool on = aliveFoliage && ta.y > 0f;
            // the noise moves with the frame only while the TAA resolves it (a still pattern otherwise: no shimmer without TAA)
            var taa = ReflexTemporalAA.Active;
            float frame = on && taa && taa.active ? Time.frameCount % 64 : 0f;
            Shader.SetGlobalVector(FoliageDitherId, new Vector4(on ? 1f : 0f, ta.x, ta.y, frame));
        }

        /// MAP SCALE: the scale the level's file-built parts are drawn at (OriginalLevel.MapScale; 1 = the game's track).
        float MapScale => level ? level.MapScale : 1f;
        bool SphereVisibleScaled(Vector3 c, float r, float ms) => ms == 1f ? SphereVisible(c, r) : SphereVisible(c * ms, r * ms);

        /// Called by OriginalLevel.LoadNow once the scenery is built; never stops the level load.
        public static OriginalForest Attach(OriginalLevel lvl)
        {
            try
            {
                if (!lvl || lvl.Content == null || disabledForTests) return null;
                var f = lvl.GetComponent<OriginalForest>();
                if (lvl.Content.forest == null && lvl.Content.grass == null) { if (f) f.Clear(); return f; }
                if (!f) f = lvl.gameObject.AddComponent<OriginalForest>();
                f.level = lvl;
                f.Begin(lvl.Content);
                return f;
            }
            catch (Exception e) { Debug.LogWarning("OriginalForest: " + e.Message + "\n" + e.StackTrace, lvl); return null; }
        }

        public void Begin(ReflexLevelContent c)
        {
            Clear();
            content = c;
            aliveFoliage = AliveLevels.Is(c.level);   // Alive's content (both ALIVE LOOKs): its leaves are drawn with Alive's screen-noise alpha
            SetFoliageDither();
            var sw = System.Diagnostics.Stopwatch.StartNew();
            grassBuild = c.BuildGrass(Shader.Find("Reflex/Grass"));
            if (grassBuild != null)
            {
                var gd = grassBuild.data;
                grassReport = $"grass {gd.name}: {gd.cells} cells ({gd.gridW} x {gd.gridH}), {gd.blades} blades per cell, draw {gd.maxDrawDistance} fade {gd.fadeDistance}, alpha test {gd.alphaTestValue}";
                grassSel = new int[gd.cells]; grassKey = new int[gd.cells]; grassFade = new float[gd.cells]; grassTmp = new int[gd.cells];
                grassBlock = new MaterialPropertyBlock();
                if (!subscribed) { RenderPipelineManager.beginCameraRendering += OnBeginCamera; subscribed = true; }
            }
            build = c.BuildForest(Shader.Find("Reflex/Tree"), Shader.Find("Reflex/Tree Billboard"));
            if (build == null) { report = (c.forest == null ? "no forest" : "forest shaders Reflex/Tree, Reflex/Tree Billboard missing") + (grassReport != null ? "; " + grassReport : ""); return; }
            var f = build.data;
            trees = f.treeCount; types = f.types.Count;
            // MAP SCALE (core/plan/MAP_SCALE.md): the forest keeps the level's own (file) data; the culling and the LOD work in it with the
            // camera brought back by the scale (Eye), and what the GPU draws is the tree moved and sized by the scale
            float ms = MapScale;
            // billboard instances: the tree records (x, y, z, type + 0.5), (scale, rotation, sin, cos)
            var inst = new Vector4[Math.Max(1, f.treeCount * 2)];
            for (int i = 0; i < f.treeCount; i++)
            {
                int o = i * 8;
                inst[i * 2] = new Vector4(f.trees[o] * ms, f.trees[o + 1] * ms, f.trees[o + 2] * ms, f.trees[o + 3]);
                inst[i * 2 + 1] = new Vector4(f.trees[o + 4] * ms, f.trees[o + 5], f.trees[o + 6], f.trees[o + 7]);
            }
            bbBuffer = new GraphicsBuffer(GraphicsBuffer.Target.Structured, inst.Length, 16);
            bbBuffer.SetData(inst);
            quad = new Mesh { name = "forest billboard corners", hideFlags = HideFlags.DontSave };
            // DAT_00a941a4 (XenonForestAsset::vf3): (x, y, u, v) = (0.5, 1, 1, 0), (0.5, 0, 1, 1), (-0.5, 0, 0, 1), (-0.5, 1, 0, 0); indices 0 1 2 2 3 0
            quad.vertices = new[] { Vector3.zero, Vector3.zero, Vector3.zero, Vector3.zero };
            quad.SetUVs(0, new List<Vector4> { new Vector4(0.5f, 1, 1, 0), new Vector4(0.5f, 0, 1, 1), new Vector4(-0.5f, 0, 0, 1), new Vector4(-0.5f, 1, 0, 0) });
            quad.SetTriangles(new[] { 0, 1, 2, 2, 3, 0 }, 0, false);
            quad.bounds = new Bounds(Vector3.zero, Vector3.one * 1e5f);
            float maxH = 0; foreach (var t in f.types) if (t.loaded) maxH = Mathf.Max(maxH, (t.boundsCenter + t.boundsHalf).magnitude * 2f);
            worldBounds = new Bounds(f.totalCenter * ms, (f.totalHalf * 2f + Vector3.one * (maxH + 10f)) * ms);
            bbBlock = new MaterialPropertyBlock();
            bbBlock.SetBuffer(BBInstancesId, bbBuffer);
            var rows = build.billboardRows;
            if (ms != 1f) { rows = (Vector4[])rows.Clone(); for (int r = 2; r < rows.Length; r += 3) rows[r].x /= ms; }   // the fade's depth slope: file depth
            bbBlock.SetVectorArray(BBTypesId, rows);
            maxRecords = 1; foreach (var t in f.types) maxRecords = Math.Max(maxRecords, t.records.Count);
            int groups = f.types.Count * maxRecords * 2;
            groupCount = new int[groups]; groupStart = new int[groups]; groupFill = new int[groups];
            drawMaterials = new Material[f.types.Count, 4][];
            for (int i = 0; i < f.types.Count; i++)
                for (int k = 0; k < 4; k++)
                {
                    var pm = build.partMaterials[i, k];
                    if (pm == null) continue;
                    var dm = new Material[pm.Length];
                    // fronds (FUN_00672a00: SetRenderState(CULLMODE, NONE)); 3D leaves (MX vs ATV Alive's leaf meshes) drawn as fronds
                    for (int q = 0; q < pm.Length; q++) dm[q] = k == 1 || k == 3 ? NoCull(pm[q]) : pm[q];
                    drawMaterials[i, k] = dm;
                }
            report = $"{f.name}: {f.treeCount} trees, {f.types.Count} types, {f.clusterStart.Length} clusters, 3D within {f.max3dDistance:F0}, billboards from {f.minBillboardStart:F0}; built in {sw.ElapsedMilliseconds} ms" + (grassReport != null ? "; " + grassReport : "");
            if (!subscribed) { RenderPipelineManager.beginCameraRendering += OnBeginCamera; subscribed = true; }
        }

        Material NoCull(Material m)
        {
            if (!m) return null;
            if (noCull.TryGetValue(m, out var c)) return c;
            c = new Material(m) { name = m.name + " (two-sided frond)", hideFlags = HideFlags.DontSave };
            c.SetFloat(CullId, (float)CullMode.Off);
            return noCull[m] = c;
        }

        void Clear()
        {
            bbBuffer?.Release(); bbBuffer = null;
            foreach (var kv in perCamera) foreach (var rb in kv.Value.ring) rb?.Release();
            perCamera.Clear();
            foreach (var kv in noCull) if (kv.Value) DestroyImmediate(kv.Value);
            noCull.Clear();
            if (quad) DestroyImmediate(quad);
            build = null; content = null; grassBuild = null;
            aliveFoliage = false; Shader.SetGlobalVector(FoliageDitherId, Vector4.zero);
            foreach (var kv in grassPerCamera) foreach (var rb in kv.Value.ring) rb?.Release();
            grassPerCamera.Clear();
        }

        void OnDestroy()
        {
            if (subscribed) { RenderPipelineManager.beginCameraRendering -= OnBeginCamera; subscribed = false; }
            Clear();
        }

        bool Accept(Camera cam)
        {
            if (!cam || (cam.cameraType != CameraType.Game && cam.cameraType != CameraType.SceneView)) return false;
            if (viewer) return cam == viewer || cam.cameraType == CameraType.SceneView;
            if (cam.cameraType == CameraType.SceneView) return true;
            var chase = level && level.runtime ? level.runtime.chaseCamera : null;
            if (chase) return cam == chase;
            var main = Camera.main;
            if (main) return cam == main;
            return (cam.cullingMask & 1) != 0;
        }

        MaterialPropertyBlock Block(int i)
        {
            while (blocks.Count <= i) blocks.Add(new MaterialPropertyBlock());
            return blocks[i];
        }

        void OnBeginCamera(ScriptableRenderContext context, Camera cam)
        {
            if (!this || (build == null && grassBuild == null) || !isActiveAndEnabled) return;
            if (level && level.Content != content) return;       // the level was reloaded (OriginalLevel calls Attach again)
            if (!Accept(cam)) return;
            var sw = System.Diagnostics.Stopwatch.StartNew();
            var tr = cam.transform;
            Vector3 eye = tr.position, fwd = tr.forward, right = tr.right, up = tr.up;
            ReflexDrawDistance.ApplyGlobals();
            SetFoliageDither();
            Shader.SetGlobalVector(CamPosId, new Vector4(eye.x, eye.y, eye.z, 1));
            Shader.SetGlobalVector(CamRightId, right); Shader.SetGlobalVector(CamUpId, up); Shader.SetGlobalVector(CamFwdId, fwd);

            if (drawGrass && grassBuild != null) DrawGrass(cam, MapScale != 1f ? eye / MapScale : eye, fwd, tr.right);   // MAP SCALE: in the level's own feet
            msGrass = sw.Elapsed.TotalMilliseconds;
            if (build == null) { msLast = sw.Elapsed.TotalMilliseconds; return; }
            var f = build.data;
            double t0 = sw.Elapsed.TotalMilliseconds;
            if (drawBillboards && build.billboardMaterial)
            {
                // g_BBWorldZ = -normalize(fwd.x, 0, fwd.z), g_BBWorldX = Y x Z, g_BBWorldY = (0, 1, 0); g_Tangent*Light = (L.X, L.Y, L.Z) (00661000)
                var z = new Vector3(-fwd.x, 0, -fwd.z); float zl = z.sqrMagnitude; z = zl > 0 ? z / Mathf.Sqrt(zl) : Vector3.forward;
                var x = new Vector3(-z.z, 0, z.x); var y = Vector3.up;
                Shader.SetGlobalVector(BBXId, x); Shader.SetGlobalVector(BBYId, y); Shader.SetGlobalVector(BBZId, z);
                var dirs = content.LightDirections();
                for (int i = 0; i < 4; i++) Shader.SetGlobalVector(BBLightIds[i], new Vector4(Vector3.Dot(dirs[i], x), Vector3.Dot(dirs[i], y), Vector3.Dot(dirs[i], z), 0));
                var sun = content.lighting.sunDirection.normalized;
                Shader.SetGlobalVector(BBSunId, new Vector4(Vector3.Dot(sun, x), Vector3.Dot(sun, y), Vector3.Dot(sun, z), 0));
                var rp = new RenderParams(build.billboardMaterial) { camera = cam, worldBounds = worldBounds, shadowCastingMode = ShadowCastingMode.Off, receiveShadows = false, matProps = bbBlock, layer = 0 };
                Graphics.RenderMeshPrimitives(rp, quad, 0, f.treeCount);
            }
            msBillboards = sw.Elapsed.TotalMilliseconds - t0;
            if (draw3d) Draw3d(cam, MapScale != 1f ? eye / MapScale : eye, fwd, f, sw);   // MAP SCALE: the selection in the level's own feet
            msLast = sw.Elapsed.TotalMilliseconds;
            if (Time.frameCount != frameSeen) { camerasLastFrame = camerasThisFrame; camerasThisFrame = 0; frameSeen = Time.frameCount; }
            camerasThisFrame++;
            milliseconds = milliseconds == 0 ? msLast : milliseconds * 0.95 + msLast * 0.05;
        }

        void Ensure<T>(ref T[] a, int n) { if (a.Length < n) a = new T[Math.Max(n, a.Length * 2)]; }

        // The trees of the frame, selected by SelectOriginal / SelectRemastered: `sel*` the visible ones (a tree in a LOD cross-fade has two entries),
        // `sh*` the shadow casters (drawn into the shadow maps only). Groups: (type, LOD record) for each of the two sets.
        int[] selTree = Array.Empty<int>(), selGroup = Array.Empty<int>(); Vector4[] selAlpha = Array.Empty<Vector4>(); Vector2[] selDith = Array.Empty<Vector2>();
        int[] shTree = Array.Empty<int>(), shGroup = Array.Empty<int>(); Vector4[] shAlpha = Array.Empty<Vector4>();
        int nSel, nShadow, clustersSeen;
        [Header("Draw distance (Rendering/ReflexDrawDistance)")]
        [Tooltip("Last frame: trees drawn in two LOD records at once (the dithered cross-fade band).")]
        public int treesBlending;
        /// Tests (REFLEX_FOREST_STATS=1): the LOD state of every tree in view (its LOD record, or past the last one) compared with the frame before:
        /// trees that changed (`lodChanges`) and trees that went back to the state they had left within 8 frames (`lodFlips`: the flip-flop).
        public static bool collectStats = Environment.GetEnvironmentVariable("REFLEX_FOREST_STATS") == "1";
        public int lodChanges, lodFlips;
        public Vector3 lastEye, lastFwd;   // tests: the camera pose of the last selection
        int statsLevel = -1;
        byte[] stNow = Array.Empty<byte>(), stOld = Array.Empty<byte>(); int[] stFrame = Array.Empty<int>();
        void Stat(int tree, int state)
        {
            if (statsLevel != ReflexDrawDistance.Level) { statsLevel = ReflexDrawDistance.Level; for (int q = 0; q < stNow.Length; q++) stNow[q] = stOld[q] = 255; }   // another draw distance: other states
            if (stNow.Length < trees) { stNow = new byte[trees]; stOld = new byte[trees]; stFrame = new int[trees]; for (int q = 0; q < trees; q++) { stNow[q] = stOld[q] = 255; } }
            if (stNow[tree] == 255) { stNow[tree] = (byte)state; return; }
            if (stNow[tree] == state) return;
            lodChanges++;
            if (stOld[tree] == state && Time.frameCount - stFrame[tree] <= 8) lodFlips++;
            stOld[tree] = stNow[tree]; stNow[tree] = (byte)state; stFrame[tree] = Time.frameCount;
        }

        void Draw3d(Camera cam, Vector3 eye, Vector3 fwd, ForestData f, System.Diagnostics.Stopwatch sw)
        {
            double t0 = sw.Elapsed.TotalMilliseconds;
            GeometryUtility.CalculateFrustumPlanes(cam, planes);
            float ms = MapScale;
            float shadowDistance = 0f;
            if (castShadows && GraphicsSettings.currentRenderPipeline is UniversalRenderPipelineAsset urp) shadowDistance = urp.shadowDistance;
            int cap = f.treeCount * 2 + 8;
            Ensure(ref selTree, cap); Ensure(ref selGroup, cap); Ensure(ref selAlpha, cap); Ensure(ref selDith, cap);
            Ensure(ref shTree, cap); Ensure(ref shGroup, cap); Ensure(ref shAlpha, cap);
            nSel = 0; nShadow = 0; clustersSeen = 0; treesBlending = 0; lodChanges = 0; lodFlips = 0;
            lastEye = eye; lastFwd = fwd;
            if (ReflexDrawDistance.Remastered) SelectRemastered(f, eye, fwd, ms, shadowDistance);
            else SelectOriginal(f, eye, fwd, ms, shadowDistance);
            int n = nSel;
            clustersVisible = clustersSeen; trees3d = n; shadowTrees = nShadow;
            // groups: [0, G) the visible trees, [G, 2G) the shadow casters
            int G = f.types.Count * maxRecords;
            Array.Clear(groupCount, 0, groupCount.Length);
            for (int j = 0; j < n; j++) groupCount[selGroup[j]]++;
            for (int j = 0; j < nShadow; j++) groupCount[G + shGroup[j]]++;
            int total = 0;
            for (int g = 0; g < 2 * G; g++) { groupStart[g] = total; groupFill[g] = total; total += groupCount[g]; }
            draws3d = 0;
            if (total == 0) return;
            Ensure(ref gpu, total * 3);
            for (int j = 0; j < n; j++)
            {
                int o = selTree[j] * 8;
                var a = new Vector4(f.trees[o] * ms, f.trees[o + 1] * ms, f.trees[o + 2] * ms, f.trees[o + 4] * ms);   // MAP SCALE: drawn in the scaled world
                var b = new Vector4(f.trees[o + 6], f.trees[o + 7], selDith[j].x, selDith[j].y);                       // zw: the LOD cross-fade (RfxDitherClip)
                int slot = groupFill[selGroup[j]]++;
                gpu[slot * 3] = a; gpu[slot * 3 + 1] = b; gpu[slot * 3 + 2] = selAlpha[j];
            }
            for (int j = 0; j < nShadow; j++)
            {
                int o = shTree[j] * 8;
                var a = new Vector4(f.trees[o] * ms, f.trees[o + 1] * ms, f.trees[o + 2] * ms, f.trees[o + 4] * ms);
                var b = new Vector4(f.trees[o + 6], f.trees[o + 7], 0, 0);
                int slot = groupFill[G + shGroup[j]]++;
                gpu[slot * 3] = a; gpu[slot * 3 + 1] = b; gpu[slot * 3 + 2] = shAlpha[j];
            }
            double t1 = sw.Elapsed.TotalMilliseconds; msSelect = t1 - t0;
            if (!perCamera.TryGetValue(cam, out var cb)) perCamera[cam] = cb = new CamBuffers();
            if (cb.capacity < total * 3)
            {
                foreach (var rb in cb.ring) rb?.Release();
                cb.capacity = Math.Max(total * 3 * 2, 3 * 1024);
                for (int r = 0; r < cb.ring.Length; r++)
                    cb.ring[r] = new GraphicsBuffer(GraphicsBuffer.Target.Structured, GraphicsBuffer.UsageFlags.LockBufferForWrite, cb.capacity, 16);
            }
            var buffer = cb.ring[cb.next]; cb.next = (cb.next + 1) % cb.ring.Length;
            if (lockedUpload)
            {
                var dst = buffer.LockBufferForWrite<Vector4>(0, total * 3);
                Unity.Collections.NativeArray<Vector4>.Copy(gpu, 0, dst, 0, total * 3);
                buffer.UnlockBufferAfterWrite<Vector4>(total * 3);
            }
            else buffer.SetData(gpu, 0, 0, total * 3);
            double t2 = sw.Elapsed.TotalMilliseconds; msUpload = t2 - t1;
            int blockIndex = 0;
            for (int g = 0; g < 2 * G; g++)
            {
                int count = groupCount[g]; if (count == 0) continue;
                bool shadowPass = g >= G;
                int gi = shadowPass ? g - G : g, ti = gi / maxRecords, k = gi % maxRecords;
                var t = f.types[ti]; var rec = t.records[k];
                var mode = shadowPass ? ShadowCastingMode.ShadowsOnly : ShadowCastingMode.Off;
                // branches (FUN_0065f6d0: c80/c81 = type + 0x20, c82 = (1, 0, 0, 0), the material's states)
                DrawGeometry(cam, buffer, ref blockIndex, ti, 0, t.geometry[0], rec.branchLod, groupStart[g], count,
                             t.branchParams, new Vector4(1, 0, 0, 0), false, mode);
                // fronds (c80/c81 = type + 0x40, c82 = (0, 1, 0, 0), CULLMODE NONE)
                DrawGeometry(cam, buffer, ref blockIndex, ti, 1, t.geometry[1], rec.frondLod, groupStart[g], count,
                             t.frondParams, new Vector4(0, 1, 0, 0), false, mode);
                // 3D leaves (MX vs ATV Alive's leaf meshes: drawn as fronds, two-sided, with the record's 3D-leaf LOD)
                if (t.geometry[3] != null)
                    DrawGeometry(cam, buffer, ref blockIndex, ti, 3, t.geometry[3], rec.leaf3dLod, groupStart[g], count,
                                 t.frondParams, new Vector4(0, 1, 0, 0), false, mode);
                // leaves (c82 = (0, 0, 1, 0); a transition record also draws the next leaf LOD with (0, 0, 0, 1))
                DrawGeometry(cam, buffer, ref blockIndex, ti, 2, t.geometry[2], rec.leafLod, groupStart[g], count,
                             null, new Vector4(0, 0, 1, 0), true, mode);
                if (rec.transition && rec.leafLod < t.leafLods - 1 && !(shadowPass && ReflexDrawDistance.Remastered))   // (a remastered shadow caster is the solid tree: one leaf LOD)
                    DrawGeometry(cam, buffer, ref blockIndex, ti, 2, t.geometry[2], rec.leafLod + 1, groupStart[g], count,
                                 null, new Vector4(0, 0, 0, 1), true, mode);
            }
            DrawGeometryEnd(sw, t2);
        }

        /// ORIGINAL draw distance: the original's selection as ported (FUN_00662ec0 / FUN_006676d0): view-depth tests, the first LOD record
        /// whose distance is beyond the depth, the shadow casters the visible 3D trees within the shadow distance.
        void SelectOriginal(ForestData f, Vector3 eye, Vector3 fwd, float ms, float shadowDistance)
        {
            float shadowReach = (shadowDistance + 60f) / ms;   // MAP SCALE: the depth below is in the level's own feet
            int n = 0, visible = 0, ns = 0;
            float fx = Math.Abs(fwd.x), fy = Math.Abs(fwd.y), fz = Math.Abs(fwd.z);
            for (int c = 0; c < f.clusterStart.Length; c++)
            {
                int cnt = f.clusterCount[c]; if (cnt == 0) continue;
                Vector3 cc = f.clusterCenter[c], ch = f.clusterHalf[c];
                float radius = ch.magnitude;
                if (!SphereVisibleScaled(cc, radius, ms)) continue;
                visible++;
                float centre = Vector3.Dot(cc - eye, fwd), ext = ch.x * fx + ch.y * fy + ch.z * fz;
                if (centre - ext > f.max3dDistance) continue;            // billboards only (FUN_00662ec0: near depth > +0x3078)
                for (int i = f.clusterStart[c], e = i + cnt; i < e; i++)
                {
                    int o = i * 8;
                    int ti = (int)f.trees[o + 3];
                    var t = f.types[ti];
                    if (!t.loaded || t.records.Count == 0) continue;
                    var p = new Vector3(f.trees[o], f.trees[o + 1], f.trees[o + 2]);
                    float d = Vector3.Dot(p - eye, fwd);
                    float s = f.trees[o + 4], sn = f.trees[o + 6], cs = f.trees[o + 7];
                    var bc = t.boundsCenter;
                    var wc = new Vector3(cs * s * bc.x + sn * s * bc.z + p.x, s * bc.y + p.y, -sn * s * bc.x + cs * s * bc.z + p.z);
                    if (d >= t.MaxDistance) { if (collectStats && SphereVisibleScaled(wc, t.boundsHalf.magnitude * s, ms)) Stat(i, t.records.Count); continue; }
                    if (!SphereVisibleScaled(wc, t.boundsHalf.magnitude * s, ms)) continue;
                    int k = 0; while (k < t.records.Count - 1 && !(d < t.records[k].dist)) k++;
                    if (collectStats) Stat(i, k);
                    var alpha = ReflexLevelContent.TreeAlphaTests(t, t.records[k], d);
                    selTree[n] = i; selGroup[n] = ti * maxRecords + k; selAlpha[n] = alpha; selDith[n] = Vector2.zero; n++;
                    if (shadowDistance > 0 && d < shadowReach) { shTree[ns] = i; shGroup[ns] = ti * maxRecords + k; shAlpha[ns] = alpha; ns++; }
                }
            }
            nSel = n; nShadow = ns; clustersSeen = visible;
        }

        /// The half-width of the cross-fade band around the edge of LOD record j (scaled depth units): a share of the edge, at least a few
        /// feet, and never more than 45% of the gap to the neighbouring edges (so two bands never overlap).
        static float BandAt(ForestTreeType t, int j)
        {
            var rs = t.records; float e = rs[j].dist;
            float b = Math.Max(ReflexDrawDistance.TreeBandShare * e, ReflexDrawDistance.TreeBandMin);
            if (j > 0) b = Math.Min(b, 0.45f * (e - rs[j - 1].dist));
            if (j + 1 < rs.Count) b = Math.Min(b, 0.45f * (rs[j + 1].dist - e));
            return Math.Max(b, 0.01f);
        }

        /// HIGH / ULTRA: the selection by distance from the camera (the depth along the view direction changes when the camera only turns),
        /// stretched by the tree scale, with the LOD records cross-faded (dithered) over a band instead of swapped; the shadow casters are
        /// every tree whose shadow can reach what the camera sees, within the shadow distance, with a LOD that depends on the distance only
        /// (not on the dissolve of the visible tree, and not on whether the tree is drawn as a billboard).
        void SelectRemastered(ForestData f, Vector3 eye, Vector3 fwd, float ms, float shadowDistance)
        {
            float invK = 1f / ReflexDrawDistance.TreeScale;
            float reach3d = f.max3dDistance / invK;                  // radial, in the level's feet
            bool shadows = shadowDistance > 0f;
            float shadowFile = shadowDistance / ms;                  // the shadow distance in the level's feet
            Vector3 sun = content.lighting.sunDirection.normalized;
            var away = new Vector3(-sun.x, 0f, -sun.z); float horiz = away.magnitude;
            away = horiz > 1e-3f ? away / horiz : Vector3.zero;
            float lengthPerHeight = horiz / Math.Max(sun.y, 0.06f);  // shadow length on level ground per unit of height
            const float LengthCap = 200f;                            // feet of shadow a tree can throw into view
            int n = 0, ns = 0, visible = 0, blend = 0;
            for (int c = 0; c < f.clusterStart.Length; c++)
            {
                int cnt = f.clusterCount[c]; if (cnt == 0) continue;
                Vector3 cc = f.clusterCenter[c], ch = f.clusterHalf[c];
                float radius = ch.magnitude;
                float cd = Vector3.Distance(cc, eye);
                bool vis3d = cd - radius <= reach3d && SphereVisibleScaled(cc, radius, ms);
                bool shad = shadows && cd - radius <= shadowFile + 60f && SphereVisibleScaled(cc + away * (LengthCap * 0.5f), radius + LengthCap * 0.5f, ms);
                if (!vis3d && !shad) continue;
                if (vis3d) visible++;
                for (int i = f.clusterStart[c], e = i + cnt; i < e; i++)
                {
                    int o = i * 8;
                    int ti = (int)f.trees[o + 3];
                    var t = f.types[ti];
                    if (!t.loaded || t.records.Count == 0) continue;
                    var p = new Vector3(f.trees[o], f.trees[o + 1], f.trees[o + 2]);
                    float dist = Vector3.Distance(p, eye), d = dist * invK;   // d: the scaled distance every LOD record, dissolve and fade is a function of
                    float s = f.trees[o + 4], sn = f.trees[o + 6], cs = f.trees[o + 7];
                    var bc = t.boundsCenter; var bh = t.boundsHalf;
                    var wc = new Vector3(cs * s * bc.x + sn * s * bc.z + p.x, s * bc.y + p.y, -sn * s * bc.x + cs * s * bc.z + p.z);
                    float rr = bh.magnitude * s;
                    var recs = t.records; int nrec = recs.Count;
                    if (collectStats && vis3d && d >= t.MaxDistance && SphereVisibleScaled(wc, rr, ms)) Stat(i, nrec);
                    if (vis3d && d < t.MaxDistance && SphereVisibleScaled(wc, rr, ms))
                    {
                        int j = 0; while (j < nrec - 1 && !(d < recs[j].dist)) j++;
                        int lo = j, hi = -1; float w = 0f;
                        if (j > 0)
                        {
                            float b = BandAt(t, j - 1), edge = recs[j - 1].dist;
                            if (d < edge + b) { lo = j - 1; hi = j; w = (d - (edge - b)) / (2f * b); }
                        }
                        if (hi < 0 && j + 1 < nrec)
                        {
                            float b = BandAt(t, j), edge = recs[j].dist;
                            if (d > edge - b) { hi = j + 1; w = (d - (edge - b)) / (2f * b); }
                        }
                        if (hi >= 0)
                        {
                            w = Mathf.Clamp01(w); w = w * w * (3f - 2f * w);
                            if (w <= 0f) hi = -1; else if (w >= 1f) { lo = hi; hi = -1; }
                        }
                        if (collectStats) Stat(i, hi >= 0 && w >= 0.5f ? hi : lo);   // (the record that carries most of the tree)
                        if (hi < 0)
                        {
                            selTree[n] = i; selGroup[n] = ti * maxRecords + lo; selAlpha[n] = ReflexLevelContent.TreeAlphaTests(t, recs[lo], d); selDith[n] = Vector2.zero; n++;
                        }
                        else
                        {
                            // the outgoing record keeps the pixels whose noise is >= w, the incoming one those below it
                            selTree[n] = i; selGroup[n] = ti * maxRecords + lo; selAlpha[n] = ReflexLevelContent.TreeAlphaTests(t, recs[lo], d); selDith[n] = new Vector2(w, -1f); n++;
                            selTree[n] = i; selGroup[n] = ti * maxRecords + hi; selAlpha[n] = ReflexLevelContent.TreeAlphaTests(t, recs[hi], d); selDith[n] = new Vector2(w, 1f); n++;
                            blend++;
                        }
                    }
                    if (shad && dist <= shadowFile + rr)
                    {
                        float height = (bc.y + bh.y) * s;
                        float len = Math.Min(height * lengthPerHeight, LengthCap);
                        if (SphereVisibleScaled(wc + away * (len * 0.5f), rr + len * 0.5f, ms))
                        {
                            // the shadow's LOD record: by the distance alone, one step coarser than the visible tree would use, never the dissolved tree
                            float dS = d * 1.5f; int j = 0; while (j < nrec - 1 && !(dS < recs[j].dist)) j++;
                            shTree[ns] = i; shGroup[ns] = ti * maxRecords + j; shAlpha[ns] = new Vector4(0, 0, 0, 0); ns++;
                        }
                    }
                }
            }
            nSel = n; nShadow = ns; clustersSeen = visible; treesBlending = blend;
        }

        void DrawGeometryEnd(System.Diagnostics.Stopwatch sw, double t2) { msSubmit = sw.Elapsed.TotalMilliseconds - t2; }

        // the grass's g_AlphaTest (see the draw); REFLEX_ALIVE_GRASS_C51 overrides it on Alive levels for comparison renders (tests only)
        static readonly float? GrassC51Override = float.TryParse(Environment.GetEnvironmentVariable("REFLEX_ALIVE_GRASS_C51"), System.Globalization.NumberStyles.Float,
                                                                 System.Globalization.CultureInfo.InvariantCulture, out float c51) ? c51 : (float?)null;
        float GrassC51(GrassData g) => GrassC51Override is float o && Shader.GetGlobalVector(AliveGlobalId).x > 0.5f ? o : g.alphaTestValue * 0.94f;

        void DrawGeometry(Camera cam, GraphicsBuffer buffer, ref int blockIndex, int ti, int k, ForestGeometry g, int lod, int start, int count,
                          float[] lodParams, Vector4 sel, bool leaf, ShadowCastingMode mode)
        {
            var mesh = build.meshes[ti, k]; var mats = drawMaterials[ti, k];
            if (!mesh || mats == null || g == null || !g.HasLod(lod)) return;
            // the LOD's submeshes: one, or one per surface of an Alive multi-material LOD (the Northwest pines' bark and needles)
            foreach (int part in g.lodParts[lod])
            {
                var mat = mats[part];
                if (!mat || g.partTriangles[part].Length == 0) continue;
                DrawPart(cam, buffer, ref blockIndex, mesh, part, mat, start, count, lodParams, sel, leaf, mode);
            }
        }

        void DrawPart(Camera cam, GraphicsBuffer buffer, ref int blockIndex, Mesh mesh, int submesh, Material mat, int start, int count,
                      float[] lodParams, Vector4 sel, bool leaf, ShadowCastingMode mode)
        {
            var mpb = Block(blockIndex++);
            mpb.SetBuffer(InstancesId, buffer);
            mpb.SetFloat(BaseId, start);
            if (lodParams != null)
            {
                mpb.SetVector(Lod1Id, new Vector4(lodParams[0] / MapScale, lodParams[1], lodParams[2], lodParams[3]));   // MAP SCALE: c80.x is a slope per file foot of depth
                mpb.SetVector(Lod2Id, new Vector4(lodParams[4], lodParams[5], lodParams[6], lodParams[7]));
            }
            mpb.SetVector(SelId, sel);
            mpb.SetFloat(LeafId, leaf ? 1f : 0f);
            var rp = new RenderParams(mat) { camera = cam, worldBounds = worldBounds, shadowCastingMode = mode, receiveShadows = true, matProps = mpb, layer = 0 };
            Graphics.RenderMeshPrimitives(rp, mesh, submesh, count);
            draws3d++;
        }

        // ------------------------------------------------------------------------------------------------ grass

        GrassBuild grassBuild;
        MaterialPropertyBlock grassBlock, grassBlockNear;
        int[] grassSel = Array.Empty<int>(), grassKey = Array.Empty<int>(), grassTmp = Array.Empty<int>(); float[] grassFade = Array.Empty<float>();
        Vector4[] grassGpu = Array.Empty<Vector4>();
        readonly Dictionary<Camera, CamBuffers> grassPerCamera = new Dictionary<Camera, CamBuffers>();
        static readonly int GrassCellsId = Shader.PropertyToID("_RfxGrassCells"), GrassBaseId = Shader.PropertyToID("_RfxGrassBase"),
            GrassTuftScaleId = Shader.PropertyToID("_RfxGrassTuftScale"), GrassTuftOffsetId = Shader.PropertyToID("_RfxGrassTuftOffset"),
            AliveGlobalId = Shader.PropertyToID("_RfxAlive"), GrassGridId = Shader.PropertyToID("_RfxGrassGrid"), GrassUVId = Shader.PropertyToID("_RfxGrassWorldToUV"), GrassTexDimId = Shader.PropertyToID("_RfxGrassTexDim"),
            GrassViewRightId = Shader.PropertyToID("_RfxGrassViewRight");
        static readonly int[] GrassUpIds = { Shader.PropertyToID("_RfxGrassUp0"), Shader.PropertyToID("_RfxGrassUp1"), Shader.PropertyToID("_RfxGrassUp2"), Shader.PropertyToID("_RfxGrassUp3") };
        // GrassSystem_Init 00668d00: the wind (+0x1c0 direction (0.85355, 0, 0.28), per up vector: phase +0x180, amplitude +0x190,
        // offset +0x1a0, frequency +0x1b0); GrassSystem::vf19 00669c0f: up_i = normalize(Y + w (sin((t - phase) freq |w|) + offset) amplitude)
        static readonly float[] WindPhase = { 0.0204f, 0.8754f, -0.11f, 0.3366f }, WindAmp = { 0.15f, 0.22f, 0.35f, 0.08f },
            WindOffset = { 0.75f, 0.8f, 0.55f, 0.63f }, WindFreq = { 0.56548672914505f, 1.0053097009658813f, 0.6283185482025146f, 0.18849556148052216f };
        static readonly Vector3 Wind = new Vector3(0.85355f, 0f, 0.28f);

        int[] grassScratch = Array.Empty<int>();
        readonly Vector4[] grassUp = new Vector4[4];

        // FUN_0066e0c0: a stable merge sort of the list entries by (int) d^2, descending
        void MergeSortDescending(int[] a, int start, int n)
        {
            if (n < 2) return;
            int h = n >> 1;
            MergeSortDescending(a, start, h); MergeSortDescending(a, start + h, n - h);
            int i = start, j = start + h, e1 = start + h, e2 = start + n, o = 0;
            while (i < e1 && j < e2) grassScratch[o++] = grassKey[a[j]] > grassKey[a[i]] ? a[j++] : a[i++];
            while (i < e1) grassScratch[o++] = a[i++];
            while (j < e2) grassScratch[o++] = a[j++];
            Array.Copy(grassScratch, 0, a, start, n);
        }

        static float QuakeSqrt(float x)
        {
            // 00669698: radius = x * y (1.5 - 0.5 x y^2), y = the 0x5f3759d5 estimate of 1 / sqrt(x)
            if (x == 0f) return 0f;
            float y = BitConverter.Int32BitsToSingle(0x5f3759d5 - (BitConverter.SingleToInt32Bits(x) >> 1));
            return (1.5f - x * 0.5f * y * y) * y * x;
        }

        /// GrassSystem::vf2_o90 00669310 (cell selection: the grid window around the camera, d^2 = horizontal distance^2 - cell radius^2,
        /// within MaxDrawDistance, the sphere in the frustum; fade = 1 - (d^2 - fade^2) / (max^2 - fade^2); fade 1: the near list) and the
        /// draws (FUN_0066e300 far, FUN_006700d0 near: each list sorted by (int) d^2 descending, one cell per draw with the blade order of the
        /// camera heading). Here each list is one instanced draw in that order.
        void DrawGrass(Camera cam, Vector3 eye, Vector3 fwd, Vector3 right)
        {
            var g = grassBuild.data;
            GeometryUtility.CalculateFrustumPlanes(cam, planes);
            float ms = MapScale;   // MAP SCALE: `eye` is in the level's own feet here; the cells are drawn moved and sized by the scale
            float gs = ReflexDrawDistance.GrassScale;   // HIGH / ULTRA: the cell window and its fade reach further
            float maxD = g.maxDrawDistance * gs, fadeD = g.fadeDistance * gs, max2 = maxD * maxD, fade2 = fadeD * fadeD;
            float ox = g.center.x - g.half.x, oz = g.center.z - g.half.z;
            int Col(float x) { int i = (int)(x * g.gridW / (g.half.x * 2f)); return Math.Max(0, Math.Min(g.gridW - 1, i)); }
            int Row(float z) { int i = (int)(z * g.gridH / (g.half.z * 2f)); return Math.Max(0, Math.Min(g.gridH - 1, i)); }
            float rx = eye.x - ox, rz = eye.z - oz;
            int c0 = Col(rx - maxD), c1 = Col(rx + maxD), r0 = Row(rz - maxD), r1 = Row(rz + maxD);
            int nNear = 0, nFar = 0, nAll = 0;
            for (int row = r0; row <= r1; row++)
                for (int col = c0; col <= c1; col++)
                {
                    int ci = row * g.gridW + col;
                    var c = g.cellCenter[ci]; var h = g.cellHalf[ci];
                    float r2 = h.x * h.x + h.y * h.y;
                    float dx = c.x - eye.x, dz = c.z - eye.z;
                    float d2 = dx * dx + dz * dz - r2;
                    if (!(d2 < max2)) continue;
                    if (!SphereVisibleScaled(c, QuakeSqrt(r2), ms)) continue;
                    float fade = 1f - (d2 - fade2) / (max2 - fade2);
                    if (fade >= 1f) fade = 1f; else if (fade <= 0f) fade = 0f;
                    grassSel[nAll] = ci; grassKey[nAll] = (int)d2; grassFade[nAll] = fade; nAll++;
                    if (fade == 1f) nNear++; else nFar++;
                }
            grassCellsFar = nFar; grassCellsNear = nNear;
            if (nAll == 0) return;
            // far then near, each by (int) d^2 descending, ties in scan order (the merge sorts FUN_0066e0c0 are stable)
            int[] order = grassTmp; int k0 = 0, k1 = nFar;
            for (int j = 0; j < nAll; j++) if (grassFade[j] == 1f) order[k1++] = j; else order[k0++] = j;
            if (grassScratch.Length < nAll) grassScratch = new int[grassSel.Length];
            MergeSortDescending(order, 0, nFar); MergeSortDescending(order, nFar, nNear);
            if (grassGpu.Length < nAll * 2) grassGpu = new Vector4[Math.Max(nAll * 2, 256)];
            for (int j = 0; j < nAll; j++)
            {
                int e = order[j], ci = grassSel[e];
                var c = g.cellCenter[ci]; var h = g.cellHalf[ci];
                grassGpu[j * 2] = new Vector4((c.x - h.x) * ms, (c.z - h.y) * ms, h.x * 2f * ms, h.y * 2f * ms);
                grassGpu[j * 2 + 1] = new Vector4(grassFade[e], g.cellSeed[ci * 3], g.cellSeed[ci * 3 + 1], g.cellSeed[ci * 3 + 2]);
            }
            if (!grassPerCamera.TryGetValue(cam, out var cb)) grassPerCamera[cam] = cb = new CamBuffers();
            if (cb.capacity < nAll * 2)
            {
                foreach (var rb in cb.ring) rb?.Release();
                cb.capacity = Math.Max(nAll * 4, 1024);
                for (int r = 0; r < cb.ring.Length; r++) cb.ring[r] = new GraphicsBuffer(GraphicsBuffer.Target.Structured, GraphicsBuffer.UsageFlags.LockBufferForWrite, cb.capacity, 16);
            }
            var buffer = cb.ring[cb.next]; cb.next = (cb.next + 1) % cb.ring.Length;
            var dst = buffer.LockBufferForWrite<Vector4>(0, nAll * 2);
            Unity.Collections.NativeArray<Vector4>.Copy(grassGpu, 0, dst, 0, nAll * 2);
            buffer.UnlockBufferAfterWrite<Vector4>(nAll * 2);
            // the blade order of the camera heading (FUN_0066e300 0066f9b2..0066fa3a): a = atan2(-f.z, -f.x) + pi / 16, wrapped to [0, 2 pi),
            // k = trunc(16 a / 2 pi)
            float ang = (float)(Math.Atan2(-fwd.z, -fwd.x) + 0.19634954631328583);
            if (ang < 0f) ang += 6.2831855f;
            int heading = Math.Max(0, Math.Min(15, (int)(ang / 6.2831855f * 16f)));
            // the wind
            float t = Application.isPlaying ? Time.timeSinceLevelLoad : (float)(System.Diagnostics.Stopwatch.GetTimestamp() / (double)System.Diagnostics.Stopwatch.Frequency);
            float wl = Wind.magnitude;
            var up = grassUp;
            for (int i = 0; i < 4; i++)
            {
                if (grassUpOverride != null && grassUpOverride.Length == 4) { up[i] = grassUpOverride[i]; continue; }
                float a = ((float)Math.Sin((t - WindPhase[i]) * WindFreq[i] * wl) + WindOffset[i]) * WindAmp[i];
                var u = (Vector3.up + Wind * a).normalized; up[i] = u;
            }
            float uvA = 1f / (g.half.x * 2f), uvB = -(g.center.x - g.half.x) * uvA, uvC = -1f / (g.half.z * 2f), uvD = (g.center.z + g.half.z) / (g.half.z * 2f);
            uvA /= ms; uvC /= ms;   // MAP SCALE: the world texture over the scaled grid
            var tuftScale = g.tuftScale; var tuftOffset = g.tuftOffset;
            tuftScale.x *= ms; tuftScale.y *= ms; tuftOffset.x *= ms; tuftOffset.y *= ms;   // the tufts grow with the map (zw: atlas uv)
            void Setup(MaterialPropertyBlock mpb, int start)
            {
                mpb.SetBuffer(GrassCellsId, buffer); mpb.SetFloat(GrassBaseId, start);
                mpb.SetVector(GrassTuftScaleId, tuftScale); mpb.SetVector(GrassTuftOffsetId, tuftOffset);
                // g_AlphaTest (c51, added to the near cells' alpha before the ALPHAREF test): Reflex's executable sets AlphaTestValue x 0.94
                // with ALPHAREF trunc(256 AlphaTestValue), i.e. a near tuft keeps every texel above about 0.064 AlphaTestValue, as the far
                // cells (blended, ALPHAREF 2) do. MX vs ATV Alive keeps the same program shape (PixelShaderNear: min(tex.a + c51, 1) x fade)
                // and the same entity values, so the same rule applies (ALIVE_CONVERSION.md, grass)
                mpb.SetVector(GrassGridId, new Vector4((g.center.y - g.half.y) * ms, g.half.y * 2f * ms, GrassC51(g), g.mipBias));
                mpb.SetVector(GrassUVId, new Vector4(uvA, uvB, uvC, uvD));
                mpb.SetVector(GrassTexDimId, new Vector4(g.textureWidth, g.textureHeight, 1f / g.textureWidth, 1f / g.textureHeight));
                for (int i = 0; i < 4; i++) mpb.SetVector(GrassUpIds[i], up[i]);
                mpb.SetVector(GrassViewRightId, right);
            }
            var bounds = new Bounds(g.center * ms, (g.half * 2f + Vector3.one * 50f) * ms);
            if (nFar > 0)
            {
                Setup(grassBlock, 0);
                Graphics.RenderMeshPrimitives(new RenderParams(grassBuild.far) { camera = cam, worldBounds = bounds, shadowCastingMode = ShadowCastingMode.Off, receiveShadows = false, matProps = grassBlock, layer = 0 },
                                              grassBuild.mesh, heading, nFar);
            }
            if (nNear > 0)
            {
                grassBlockNear ??= new MaterialPropertyBlock();
                Setup(grassBlockNear, nFar);
                Graphics.RenderMeshPrimitives(new RenderParams(grassBuild.near) { camera = cam, worldBounds = bounds, shadowCastingMode = ShadowCastingMode.Off, receiveShadows = true, matProps = grassBlockNear, layer = 0 },
                                              grassBuild.mesh, heading, nNear);
            }
        }

        bool SphereVisible(Vector3 c, float r)
        {
            for (int i = 0; i < 6; i++) if (planes[i].GetDistanceToPoint(c) < -r - 0.01f) return false;
            return true;
        }
    }
}
