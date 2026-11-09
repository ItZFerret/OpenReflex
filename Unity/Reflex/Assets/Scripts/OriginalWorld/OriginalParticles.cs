using System;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using UnityEngine;
using UnityEngine.Rendering;

namespace Reflex.Port.OriginalWorld
{
    // One particle as the original renderer instances it (reflex_world.h ReflexParticle, 64 bytes, blittable).
    [StructLayout(LayoutKind.Sequential)]
    public struct ReflexParticle
    {
        public float px, py, pz;          // world position (native axes = Unity axes)
        public float sizeX, sizeY;        // quad half extents before scale
        public float pivotX, pivotY;      // quad pivot in corner units
        public float scale, rotation;     // rotation in the view plane (radians)
        public uint color, color1;        // D3DCOLOR ARGB, multiplied
        public float uScale, vScale, uOffset, vOffset;   // atlas frame (D3D v down)
        public uint flags;                // bit 0: additive manager
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct ReflexParticleInfo
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)] public string texture;
        public float lum0, lum1, lum2, lum3, delta0, delta1, delta2, delta3, fade0, fade1, fade2, fade3;
        public int soft, normal, additive, live;
        public float nearPlane, farPlane, reserved0, reserved1;
    }

    public static class ReflexParticlesNative
    {
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_particles_enable__fn(IntPtr world, int enabled);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_particles_enable__fn> reflex_world_particles_enable__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_particles_enable__fn>("reflex_world_particles_enable");
        public static int reflex_world_particles_enable(IntPtr world, int enabled) => reflex_world_particles_enable__p.Get()(world, enabled);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_particles__fn(IntPtr world, [Out] ReflexParticle[] output, uint max, ref ReflexParticleInfo info);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_particles__fn> reflex_world_particles__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_particles__fn>("reflex_world_particles");
        public static int reflex_world_particles(IntPtr world, [Out] ReflexParticle[] output, uint max, ref ReflexParticleInfo info) => reflex_world_particles__p.Get()(world, output, max, ref info);
    }

    // The original game's particles (dust, roost, dirt and mud chunks, splashes, exhaust, mist, pyro, flares, sparklers and
    // fireworks): the running game simulates its own two particle managers (PlatformParticleManager2D, kept on while the other
    // ambient animation is skipped) and exports every live particle after each frame (reflex_world_particles,
    // native/drafts/world_runtime/particles.cpp); this draws them as the original does (Reflex/Original Particles = the
    // PyroParticles programs): one camera-facing quad per particle from a structured buffer, two procedural draws (the normal
    // manager SRCALPHA / INVSRCALPHA far to near, then the additive one SRCALPHA / ONE), the level's particle atlas read from the
    // player's install at run time (the texture named like the level's ParticleEmitterAsset). No GameObject per particle.
    [DisallowMultipleComponent]
    public sealed class OriginalParticles : MonoBehaviour
    {
        public const int MaxParticles = 6000;   // 3000 drawn per manager (the original's instance cap)
        [Tooltip("Reflex/Original Particles; a serialized reference keeps it in player builds.")]
        public Shader particleShader;
        [Tooltip("The level's Database (e.g. NAT_Concord): the particle atlas is looked up there first.")]
        public string level = "";
        [Tooltip("Soft particles (NoLightPS depth fade), as the game's SoftParticles flag says; needs the URP depth texture.")]
        public bool softParticles = true;
        public bool visible = true;

        public int Normal { get; private set; }
        public int Additive { get; private set; }
        public int Live { get; private set; }
        /// The last frame's drawn particles (tests / probes; the array is reused by the next frame).
        public ReflexParticle[] LastParticles { get; private set; }
        public int LastCount { get; private set; }
        public string TextureName { get; private set; } = "";
        /// The last frame's ReflexParticleInfo (the atlas, luminance, fades).
        public ReflexParticleInfo LastInfo => info;
        /// Every Apply (the OPEN REFLEX replay records the frame's particles: Reflex.Port.Replay.ReflexReplay).
        public static event Action<OriginalParticles, ReflexParticle[], int, ReflexParticleInfo> Applied;

        ComputeBuffer buffer;
        Material normalMat, additiveMat;
        MaterialPropertyBlock normalProps, additiveProps;
        Texture2D atlas; string loading; Task<Content.LevelTexture> pending;
        ReflexParticleInfo info; bool haveInfo;
        static readonly int ParticlesId = Shader.PropertyToID("_Particles"), BaseId = Shader.PropertyToID("_Base"), MainTexId = Shader.PropertyToID("_MainTex");
        static readonly int LumId = Shader.PropertyToID("_Luminance"), DeltaId = Shader.PropertyToID("_LuminanceDelta"), FadeId = Shader.PropertyToID("_Fade"), SoftId = Shader.PropertyToID("_Soft");

        /// Called on the simulation thread after a frame (OriginalSimulation): the frame's particles into r.
        public static void Collect(IntPtr world, ref ReflexParticle[] output, ref ReflexParticleInfo info, out int count)
        {
            if (output == null) output = new ReflexParticle[MaxParticles];
            count = Math.Max(0, ReflexParticlesNative.reflex_world_particles(world, output, (uint)output.Length, ref info));
        }

        /// The frame's particles (main thread, right after OriginalSimulation.Collect).
        public void Apply(ReflexParticle[] particles, int count, ReflexParticleInfo frameInfo)
        {
            info = frameInfo; haveInfo = true; LastParticles = particles; LastCount = count;
            Normal = Math.Min(info.normal, count); Additive = Math.Max(0, Math.Min(info.additive, count - Normal)); Live = info.live;
            if (!string.IsNullOrEmpty(info.texture) && info.texture != TextureName && info.texture != loading) BeginLoad(info.texture);
            if (buffer == null) buffer = new ComputeBuffer(MaxParticles, 64, ComputeBufferType.Structured);
            if (count > 0) buffer.SetData(particles, 0, 0, Math.Min(count, MaxParticles));
            Applied?.Invoke(this, particles, count, frameInfo);
        }

        void BeginLoad(string name)
        {
            loading = name;
            string db = level ?? "";
            pending = Task.Run(() =>
            {
                var install = Content.ReflexInstall.Packages;
                Content.Rfx.Asset a = null;
                // level: a Database name or an event key ("National/NAT_Concord"): its location's database holds the atlas
                foreach (string d in db.Split('/').Reverse().Append(name))
                {
                    var database = string.IsNullOrEmpty(d) ? null : install.Db(d);
                    if (database == null) continue;
                    foreach (var p in database.packages.Values) { a = p.Find(name, "texture"); if (a != null) break; }
                    if (a != null) break;
                }
                if (a == null) throw new System.IO.FileNotFoundException("particle atlas texture '" + name + "' not found in " + db);
                return Content.ReflexTex.Parse(name, install.Read(a));
            });
        }

        void Update()
        {
            if (pending != null && pending.IsCompleted)
            {
                if (pending.IsFaulted) { Debug.LogWarning("Original particles: " + pending.Exception?.GetBaseException().Message, this); TextureName = loading; }
                else
                {
                    // sampler state of 00698890: linear min / mag, point mip, clamp
                    var t = Content.ReflexTex.ToTexture(pending.Result, TextureWrapMode.Clamp, TextureWrapMode.Clamp, FilterMode.Bilinear);
                    if (t)
                    {
                        if (atlas) Destroy(atlas); atlas = t; TextureName = loading;
                        Debug.Log($"Original particles: atlas '{TextureName}' {t.width}x{t.height} {t.format}, {t.mipmapCount} mip level(s), d3d format {pending.Result.d3dFormat}, soft {info.soft}");
                    }
                }
                pending = null; loading = null;
            }
            Draw();
        }

        void Draw()
        {
            if (!visible || !haveInfo || buffer == null || !atlas || Normal + Additive == 0) return;
            if (!normalMat)
            {
                var shader = particleShader ? particleShader : Shader.Find("Reflex/Original Particles");
                if (!shader) { visible = false; Debug.LogWarning("Original particles: shader Reflex/Original Particles missing.", this); return; }
                normalMat = new Material(shader) { name = "Original particles (normal)", hideFlags = HideFlags.DontSave };
                normalMat.SetFloat("_SrcBlend", (float)BlendMode.SrcAlpha); normalMat.SetFloat("_DstBlend", (float)BlendMode.OneMinusSrcAlpha);
                additiveMat = new Material(shader) { name = "Original particles (additive)", hideFlags = HideFlags.DontSave };
                additiveMat.SetFloat("_SrcBlend", (float)BlendMode.SrcAlpha); additiveMat.SetFloat("_DstBlend", (float)BlendMode.One);
                additiveMat.renderQueue = normalMat.renderQueue + 1;   // the additive manager draws after the normal one
                normalProps = new MaterialPropertyBlock(); additiveProps = new MaterialPropertyBlock();
            }
            var lum = new Vector4(info.lum0, info.lum1, info.lum2, info.lum3); var delta = new Vector4(info.delta0, info.delta1, info.delta2, info.delta3);
            var fade = new Vector4(info.fade0, info.fade1, info.fade2, info.fade3);
            float soft = softParticles && info.soft != 0 ? 1f : 0f;
            var bounds = new Bounds(Vector3.zero, Vector3.one * 100000f);
            void One(Material m, MaterialPropertyBlock props, int first, int n)
            {
                if (n <= 0) return;
                props.SetBuffer(ParticlesId, buffer); props.SetInt(BaseId, first); props.SetTexture(MainTexId, atlas);
                props.SetVector(LumId, lum); props.SetVector(DeltaId, delta); props.SetVector(FadeId, fade); props.SetFloat(SoftId, soft);
                var rp = new RenderParams(m) { worldBounds = bounds, matProps = props, shadowCastingMode = ShadowCastingMode.Off, receiveShadows = false, layer = gameObject.layer };
                Graphics.RenderPrimitives(rp, MeshTopology.Triangles, n * 6, 1);
            }
            One(normalMat, normalProps, 0, Normal);
            One(additiveMat, additiveProps, Normal, Additive);
        }

        void OnDestroy()
        {
            buffer?.Release(); buffer = null;
            if (normalMat) Destroy(normalMat);
            if (additiveMat) Destroy(additiveMat);
            if (atlas) Destroy(atlas);
        }
    }
}
