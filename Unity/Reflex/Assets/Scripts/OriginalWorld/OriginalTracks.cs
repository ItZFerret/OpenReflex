using System;
using System.IO;
using UnityEngine;
using UnityEngine.Rendering;

namespace Reflex.Port.OriginalWorld
{
    // The tyre-track decal layer of the original RT_Bump target (_re/notes/rt-bump.md section 2.2):
    // the game's own ribbon triangles (TerrainTracks vertex/index buffers, reflex_world_tracks) are
    // drawn with ps_hft_tracks (Reflex/Original HFT Tracks, multiply blend) into a world-space
    // texture over the terrain, which the terrain shader multiplies into RT_Bump.xyz (_TrackMap).
    // Redrawn completely whenever the game flushed new ribbon vertices (the original redraws every
    // frame; the ribbons only change on a flush). Old marks disappear when the game's ring reuses
    // their chunk, exactly as in the original.
    [DisallowMultipleComponent]
    public sealed class OriginalTracks : MonoBehaviour
    {
        public Shader trackShader;
        public Texture2D trackGeneric, trackMX, trackATV, trackDefault;
        [Tooltip("The terrain's SoftnessMask texture (RGB, v up = +z), read from the install by OriginalLevel; when empty the legacy <deform>-model.bin file is used.")]
        public Texture2D softnessMask;
        [Tooltip("Track texture resolution (square, over the whole terrain).")]
        public int resolution = 4096;

        static readonly int TrackMapId = Shader.PropertyToID("_TrackMap"), UseTrackMapId = Shader.PropertyToID("_UseTrackMap");
        static readonly int TrackMapRectId = Shader.PropertyToID("_TrackMapRect"), TrackMapTexelId = Shader.PropertyToID("_TrackMapTexel");

        RenderTexture map;
        Material material;
        Mesh mesh;
        Texture2D softness; bool ownsSoftness;
        Material terrainMaterial;
        Vector2 centre, half;
        CommandBuffer commands;
        // The ribbon mesh is the core's vertex block as it comes: 6 floats per vertex (world x, world z | side, V | intensity, type) in one
        // vertex stream, uploaded with one SetVertexBufferData (no per-vertex loop, no Vector3 / Vector2 arrays). The decal shader turns the
        // world position into the terrain-normalized one (centre / half size, _TrackWorld) that the CPU used to compute per vertex. The index
        // buffer is the identity 0..capacity-1, written once per growth.
        static readonly VertexAttributeDescriptor[] Layout =
        {
            new VertexAttributeDescriptor(VertexAttribute.Position, VertexAttributeFormat.Float32, 2),
            new VertexAttributeDescriptor(VertexAttribute.TexCoord0, VertexAttributeFormat.Float32, 2),
            new VertexAttributeDescriptor(VertexAttribute.TexCoord1, VertexAttributeFormat.Float32, 2),
        };
        const MeshUpdateFlags NoWork = MeshUpdateFlags.DontRecalculateBounds | MeshUpdateFlags.DontValidateIndices | MeshUpdateFlags.DontNotifyMeshUsers | MeshUpdateFlags.DontResetBoneBounds;
        static readonly int TrackWorldId = Shader.PropertyToID("_TrackWorld");
        int capacity;

        public int Triangles { get; private set; }

        /// The ribbon mesh (world x / z in position.xy; the material's _TrackWorld makes the shader normalize them to the terrain's [-1, 1])
        /// and its material, for drawing the track pass into the RT_Bump slices as the original does (Rendering/ReflexRTBump.cs).
        /// False while there are no ribbons.
        public bool TryGetDraw(out Mesh ribbons, out Material decal, out Vector2 terrainCentre, out Vector2 terrainHalf)
        {
            ribbons = mesh; decal = material; terrainCentre = centre; terrainHalf = half;
            return mesh && material && Triangles > 0;
        }

        // terrainMat: the live terrain material (Reflex/Original HFTerrain); softnessFile: <level>-deform-model.bin (RXDF).
        public bool Init(Material terrainMat, ReflexTerrainInfo info, string softnessFile)
        {
            Release();
            if (!trackShader || !terrainMat || !terrainMat.HasProperty(TrackMapId)) return false;
            terrainMaterial = terrainMat;
            centre = new Vector2((info.min_x + info.max_x) * 0.5f, (info.min_z + info.max_z) * 0.5f);
            half = new Vector2((info.max_x - info.min_x) * 0.5f, (info.max_z - info.min_z) * 0.5f);
            softness = softnessMask ? softnessMask : LoadSoftness(softnessFile);
            ownsSoftness = !softnessMask;
            material = new Material(trackShader) { name = "Original HFT tracks (live)" };
            material.SetTexture("_TrackGeneric", trackGeneric); material.SetTexture("_TrackMX", trackMX ? trackMX : trackGeneric);
            material.SetTexture("_TrackATV", trackATV ? trackATV : trackGeneric); material.SetTexture("_TrackDefault", trackDefault ? trackDefault : trackGeneric);
            if (softness) material.SetTexture("_SoftnessMask", softness);
            material.SetVector(TrackWorldId, new Vector4(centre.x, centre.y, half.x, half.y));   // the vertices are world positions
            map = new RenderTexture(resolution, resolution, 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Linear)
                { name = "Original tyre tracks (RT_Bump track pass)", wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Bilinear, useMipMap = false };
            map.Create();
            mesh = new Mesh { name = "Original tyre-track ribbons", indexFormat = IndexFormat.UInt32 };
            capacity = 0; Triangles = 0;
            mesh.MarkDynamic();
            commands = new CommandBuffer { name = "Original tyre tracks" };
            Redraw(0);
            terrainMaterial.SetTexture(TrackMapId, map);
            terrainMaterial.SetVector(TrackMapRectId, new Vector4(centre.x, centre.y, half.x, half.y));
            terrainMaterial.SetVector(TrackMapTexelId, new Vector4(1f / resolution, 1f / resolution, 0, 0));
            terrainMaterial.SetFloat(UseTrackMapId, 1f);
            return true;
        }

        // data: triangles * 3 vertices * {world x, world z, side, V, intensity, texture type}.
        public void Apply(float[] data, int triangles)
        {
            if (!map || data == null) return;
            int n = Math.Max(0, Math.Min(triangles, data.Length / 18)) * 3;
            if (capacity < n || capacity == 0)
            {
                // grown (power of two): fresh buffers
                int cap = Mathf.NextPowerOfTwo(Math.Max(n, 3 * 1024));
                mesh.Clear();
                mesh.indexFormat = IndexFormat.UInt32;
                mesh.SetVertexBufferParams(cap, Layout);
                mesh.SetIndexBufferParams(cap, IndexFormat.UInt32);
                var identity = new int[cap];
                for (int i = 0; i < cap; i++) identity[i] = i;
                mesh.SetIndexBufferData(identity, 0, 0, cap, NoWork);
                mesh.subMeshCount = 1;
                mesh.bounds = new Bounds(Vector3.zero, new Vector3(4, 4, 4));
                capacity = cap;
            }
            if (n > 0) mesh.SetVertexBufferData(data, 0, 0, n * 6, 0, NoWork);   // counts are floats, not vertices
            mesh.SetSubMesh(0, new SubMeshDescriptor(0, n, MeshTopology.Triangles), NoWork);
            Redraw(n);
            Triangles = n / 3;
        }

        void Redraw(int vertices)
        {
            commands.Clear();
            commands.SetRenderTarget(map);
            commands.ClearRenderTarget(false, true, Color.white);   // track pass multiplies into 1 (the RT_Bump clear is applied in the terrain shader)
            if (vertices > 0)
            {
                commands.SetViewProjectionMatrices(Matrix4x4.identity, Matrix4x4.identity);   // the command buffer applies the render-texture flip itself
                commands.DrawMesh(mesh, Matrix4x4.identity, material, 0, 0);
            }
            Graphics.ExecuteCommandBuffer(commands);
        }

        // RXDF (terrain_deform_pack.py): "RXDF", version 1, mask w, h, mips, mip sizes, then the RGB u8
        // softness mask (row 0 = +Z edge). Uploaded with v up = +z.
        static Texture2D LoadSoftness(string path)
        {
            try
            {
                if (!File.Exists(path)) return null;
                using var f = File.OpenRead(path);
                using var r = new BinaryReader(f);
                if (new string(r.ReadChars(4)) != "RXDF" || r.ReadUInt32() != 1) return null;
                int w = (int)r.ReadUInt32(), h = (int)r.ReadUInt32(), mips = (int)r.ReadUInt32();
                f.Seek(8L * mips, SeekOrigin.Current);
                byte[] rgb = r.ReadBytes(w * h * 3);
                if (rgb.Length != w * h * 3) return null;
                var rows = new byte[rgb.Length];
                for (int y = 0; y < h; y++) Buffer.BlockCopy(rgb, y * w * 3, rows, (h - 1 - y) * w * 3, w * 3);
                var tex = new Texture2D(w, h, TextureFormat.RGB24, false, true) { name = "Original SoftnessMask", wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Bilinear };
                tex.SetPixelData(rows, 0); tex.Apply(false, true);
                return tex;
            }
            catch (Exception e) { Debug.LogWarning("Softness mask not loaded: " + e.Message); return null; }
        }

        public void Release()
        {
            if (terrainMaterial) terrainMaterial.SetFloat(UseTrackMapId, 0f);
            if (map) { map.Release(); Destroy(map); }
            if (material) Destroy(material);
            if (mesh) Destroy(mesh);
            if (softness && ownsSoftness) Destroy(softness);
            commands?.Release();
            map = null; material = null; mesh = null; softness = null; commands = null; terrainMaterial = null;
        }

        void OnDestroy() => Release();
    }
}
