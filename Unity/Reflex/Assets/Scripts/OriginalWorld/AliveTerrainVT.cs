// The MX vs ATV Alive terrain's decal composite (Alive's terrain virtual texture, see Content/AliveTerrain.cs): the level's HFDecalModel
// layers drawn top-down, in SortBias order, into four camera-centred terrain-space rings (128 / 512 / 2048 feet and the whole terrain,
// 2048 x 2048 texels each, mipmapped), re-drawn when the camera has moved an eighth of a ring. The terrain shader (Reflex/Original
// HFTerrain, _AliveTerrain) samples the finest ring that holds the point (_AliveVT0..3, _AliveVTRect0..3 = x0, z0, size, valid).
using System.Collections.Generic;
using Reflex.Port.Content;
using UnityEngine;
using UnityEngine.Rendering;

namespace Reflex.Port.OriginalWorld
{
    [DisallowMultipleComponent]
    public sealed class AliveTerrainVT : MonoBehaviour
    {
        public const int Resolution = 2048;
        public static readonly float[] RingSizes = { 128f, 512f, 2048f, 0f };   // 0: the whole terrain (fixed)

        [System.NonSerialized] public List<AliveDecalDraw> decals = new List<AliveDecalDraw>();
        public Camera targetCamera;
        public OriginalTerrain terrain;          // its live Unity terrain material receives the rings
        public Material terrainMaterial;         // or this material (no OriginalTerrain)
        public Vector2 terrainCentre; public float terrainExtent = 2048f;
        /// MAP SCALE: the rings are drawn in the level's own feet (the decals' file coordinates); the terrain samples them in the scaled world
        public float mapScale = 1f;
        /// ALIVE LOOK REFLEX (Content/AliveLook.cs): only the whole-terrain composite, drawn once and bound as the terrain's _TopDownMap (no rings)
        public bool topDownOnly;
        static readonly int TopDownId = Shader.PropertyToID("_TopDownMap"), TopDownMeanId = Shader.PropertyToID("_AliveTopDownMean");

        readonly RenderTexture[] rings = new RenderTexture[4];
        readonly Vector2[] centres = new Vector2[4];
        readonly bool[] drawn = new bool[4];
        CommandBuffer cmd;
        static readonly int[] TexIds = { Shader.PropertyToID("_AliveVT0"), Shader.PropertyToID("_AliveVT1"), Shader.PropertyToID("_AliveVT2"), Shader.PropertyToID("_AliveVT3") };
        static readonly int[] RectIds = { Shader.PropertyToID("_AliveVTRect0"), Shader.PropertyToID("_AliveVTRect1"), Shader.PropertyToID("_AliveVTRect2"), Shader.PropertyToID("_AliveVTRect3") };
        public int Redraws { get; private set; }

        void OnEnable() { RenderPipelineManager.beginCameraRendering += OnBegin; }
        void OnDisable()
        {
            RenderPipelineManager.beginCameraRendering -= OnBegin;
            for (int k = 0; k < rings.Length; k++) { if (rings[k]) { rings[k].Release(); Destroy(rings[k]); } rings[k] = null; drawn[k] = false; }
            var m = terrain && terrain.UnityTerrain ? terrain.UnityTerrain.materialTemplate : terrainMaterial;
            if (m) for (int k = 0; k < 4; k++) m.SetVector(RectIds[k], Vector4.zero);
            cmd?.Release(); cmd = null;
        }

        float SizeOf(int k) => RingSizes[k] > 0f ? Mathf.Min(RingSizes[k], terrainExtent) : terrainExtent;

        void OnBegin(ScriptableRenderContext context, Camera c)
        {
            var target = targetCamera ? targetCamera : Camera.main;
            var mat = terrain && terrain.UnityTerrain && terrain.UnityTerrain.materialTemplate ? terrain.UnityTerrain.materialTemplate : terrainMaterial;
            if (c != target || decals == null || decals.Count == 0 || !mat) return;
            cmd ??= new CommandBuffer { name = "Alive terrain decals" };
            cmd.Clear();
            var p = new Vector2(c.transform.position.x, c.transform.position.z) / (mapScale > 0f ? mapScale : 1f);
            bool any = false;
            for (int k = topDownOnly ? 3 : 0; k < rings.Length; k++)
            {
                float size = SizeOf(k);
                Vector2 want = RingSizes[k] > 0f && size < terrainExtent ? Snap(p, size / 16f) : terrainCentre;
                if (drawn[k] && (RingSizes[k] <= 0f || (want - centres[k]).magnitude < size / 8f)) continue;
                if (!rings[k])
                {
                    rings[k] = new RenderTexture(Resolution, Resolution, 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Linear)
                    { name = "Alive terrain ring " + k, useMipMap = true, autoGenerateMips = true, filterMode = FilterMode.Trilinear, wrapMode = TextureWrapMode.Clamp, anisoLevel = 4, hideFlags = HideFlags.DontSave };
                    rings[k].Create();
                }
                centres[k] = want; drawn[k] = true; any = true; Redraws++;
                float h = size * 0.5f;
                cmd.SetRenderTarget(rings[k]);
                cmd.ClearRenderTarget(false, true, new Color(0.5f, 0.45f, 0.4f, 1f));
                cmd.SetViewProjectionMatrices(Matrix4x4.identity, Matrix4x4.Ortho(want.x - h, want.x + h, want.y - h, want.y + h, -1f, 1f));
                for (int i = 0; i < decals.Count; i++)
                {
                    var d = decals[i];
                    if (!d.mesh || !d.material) continue;
                    cmd.DrawMesh(d.mesh, Matrix4x4.identity, d.material, 0, 0);
                }
            }
            if (any) context.ExecuteCommandBuffer(cmd);
            // ALIVE LOOK REFLEX: the composite's mean colour, read back once after its bake (the terrain brings the composite to the Reflex
            // layers' brightness with it before picking a layer)
            if (topDownOnly && any && drawn[3])
            {
                var meanMat = mat;
                cmd.Clear();
                cmd.RequestAsyncReadback(rings[3], 0, TextureFormat.RGBA32, r =>
                {
                    if (r.hasError || !meanMat) return;
                    var px = r.GetData<byte>(); long sr = 0, sg = 0, sb = 0; int n = px.Length / 4;
                    for (int i = 0; i < n; i++) { sr += px[i * 4]; sg += px[i * 4 + 1]; sb += px[i * 4 + 2]; }
                    if (n > 0) meanMat.SetVector(TopDownMeanId, new Vector4(sr / (255f * n), sg / (255f * n), sb / (255f * n), 1f));
                });
                context.ExecuteCommandBuffer(cmd);
            }
            if (topDownOnly)
            {
                // ALIVE LOOK REFLEX: the whole-terrain ring is the terrain's top-down map (its texels on the height grid's: AliveLook.cs)
                if (drawn[3] && mat.GetTexture(TopDownId) != rings[3]) mat.SetTexture(TopDownId, rings[3]);
                return;
            }
            for (int k = 0; k < rings.Length; k++)
            {
                if (!drawn[k]) continue;
                float size = SizeOf(k);
                mat.SetTexture(TexIds[k], rings[k]);
                float s = mapScale > 0f ? mapScale : 1f;
                mat.SetVector(RectIds[k], new Vector4((centres[k].x - size * 0.5f) * s, (centres[k].y - size * 0.5f) * s, size * s, 1f));
            }
        }

        static Vector2 Snap(Vector2 p, float step) => new Vector2(Mathf.Round(p.x / step) * step, Mathf.Round(p.y / step) * step);
    }
}
