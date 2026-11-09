using System;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;

namespace Reflex.Port.OriginalWorld
{
    /// The craft's spray (water) and dust (land): a small CPU particle system drawn with the port's own particle program
    /// (Reflex/Original Particles, the one that draws the game's particles) from a soft procedural puff, so it is lit, fogged and
    /// HDR-blended like them. Port extra: nothing here reads or writes the game's particle managers.
    public sealed class HoverSpray
    {
        struct P { public Vector3 pos, vel; public float age, life, size0, size1, spin, drag, rise; public uint color; public float alpha; }
        const int Max = 1400;
        readonly P[] p = new P[Max];
        int count;
        readonly ReflexParticle[] gpu = new ReflexParticle[Max];
        ComputeBuffer buffer; Material material; MaterialPropertyBlock props; Texture2D puff;
        static readonly int ParticlesId = Shader.PropertyToID("_Particles"), BaseId = Shader.PropertyToID("_Base"), MainTexId = Shader.PropertyToID("_MainTex");
        static readonly int LumId = Shader.PropertyToID("_Luminance"), DeltaId = Shader.PropertyToID("_LuminanceDelta"), FadeId = Shader.PropertyToID("_Fade"), SoftId = Shader.PropertyToID("_Soft");
        System.Random rng = new System.Random(1234);
        public int Live => count;
        public bool Ready => material != null;

        public HoverSpray(Shader shader)
        {
            if (!shader) shader = Shader.Find("Reflex/Original Particles");
            if (!shader) { Debug.LogWarning("Hovercraft spray: shader Reflex/Original Particles not found."); return; }
            material = new Material(shader) { name = "Hovercraft spray", hideFlags = HideFlags.DontSave };
            material.SetFloat("_SrcBlend", (float)BlendMode.SrcAlpha); material.SetFloat("_DstBlend", (float)BlendMode.OneMinusSrcAlpha);
            puff = MakePuff(); props = new MaterialPropertyBlock();
            buffer = new ComputeBuffer(Max, 64, ComputeBufferType.Structured);
        }

        static Texture2D MakePuff()
        {
            const int N = 64;
            var t = new Texture2D(N, N, TextureFormat.RGBA32, false, true) { name = "Hovercraft puff", wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Bilinear, hideFlags = HideFlags.DontSave };
            var px = new Color32[N * N];
            var rnd = new System.Random(7);
            var lumps = new Vector3[7];
            for (int i = 0; i < lumps.Length; i++) lumps[i] = new Vector3((float)(rnd.NextDouble() - .5) * 0.7f, (float)(rnd.NextDouble() - .5) * 0.7f, 0.22f + (float)rnd.NextDouble() * 0.2f);
            for (int y = 0; y < N; y++)
                for (int x = 0; x < N; x++)
                {
                    float u = (x + .5f) / N * 2 - 1, v = (y + .5f) / N * 2 - 1;
                    float a = 0;
                    foreach (var l in lumps) { float d = Mathf.Sqrt((u - l.x) * (u - l.x) + (v - l.y) * (v - l.y)) / l.z; a += Mathf.Clamp01(1f - d) * Mathf.Clamp01(1f - d); }
                    float edge = Mathf.Clamp01(1f - Mathf.Sqrt(u * u + v * v));
                    a = Mathf.Clamp01(a * 0.8f) * edge * edge * 1.6f;
                    px[y * N + x] = new Color32(255, 255, 255, (byte)Mathf.RoundToInt(Mathf.Clamp01(a) * 255f));
                }
            t.SetPixels32(px); t.Apply(false, true);
            return t;
        }

        float R() => (float)rng.NextDouble();
        float R2() => (float)rng.NextDouble() * 2f - 1f;
        static uint Argb(Color c) => ((uint)Mathf.RoundToInt(Mathf.Clamp01(c.a) * 255) << 24) | ((uint)Mathf.RoundToInt(Mathf.Clamp01(c.r) * 255) << 16) | ((uint)Mathf.RoundToInt(Mathf.Clamp01(c.g) * 255) << 8) | (uint)Mathf.RoundToInt(Mathf.Clamp01(c.b) * 255);

        void Emit(Vector3 pos, Vector3 vel, float life, float s0, float s1, Color color, float drag, float rise)
        {
            if (count >= Max) return;
            p[count++] = new P { pos = pos, vel = vel, age = 0, life = life, size0 = s0, size1 = s1, spin = R2() * 2f, drag = drag, rise = rise, color = Argb(color), alpha = color.a };
        }

        /// One tick: the craft's frame and what it is doing. surface: the ground or water height under it.
        public void Update(float dt, Vector3 origin, Quaternion rot, float cushion, float speed, float throttle, bool water, float surface, bool airborne, float inflating, float halfW, float halfL, Vector3 fanLeft, Vector3 fanRight)
        {
            if (material == null) return;
            // ageing / motion
            for (int i = 0; i < count; i++)
            {
                ref var q = ref p[i];
                q.age += dt;
                if (q.age >= q.life) { p[i--] = p[--count]; continue; }
                q.vel *= Mathf.Exp(-q.drag * dt);
                q.vel.y += q.rise * dt;
                q.pos += q.vel * dt;
                if (!water && q.pos.y < surface) q.pos.y = surface;
            }
            float power = Mathf.Clamp01(cushion);
            if (power > 0.15f && !airborne)
            {
                // the skirt's air: a ring of puffs round the outline, more the faster the craft goes and the harder the fans push
                float rate = (60f + speed * 2.6f + throttle * 90f) * power * (0.4f + 0.6f * power);
                int n = Mathf.FloorToInt(rate * dt + R());
                for (int i = 0; i < n; i++)
                {
                    float a = R() * Mathf.PI * 2f;
                    var local = new Vector3(Mathf.Sin(a) * (halfW + 4f), 0f, Mathf.Cos(a) * (halfL + 5f));
                    var outward = new Vector3(Mathf.Sin(a), 0, Mathf.Cos(a));
                    var wpos = origin + rot * local; wpos.y = surface + 0.6f;
                    var wvel = rot * (outward * (10f + R() * 16f)) + Vector3.up * (4f + R() * 7f);
                    if (water) Emit(wpos, wvel, 0.9f + R() * 0.9f, 3.2f + R() * 2.2f, 8f + R() * 6f, new Color(0.88f, 0.94f, 1f, 0.34f * power), 1.4f, -3f);
                    else Emit(wpos, wvel, 1.1f + R() * 1.2f, 3.5f + R() * 2.5f, 10f + R() * 7f, new Color(0.62f, 0.55f, 0.43f, 0.30f * power), 1.1f, 1.2f);
                }
                // the wash astern of the fans
                int m = Mathf.FloorToInt((24f + throttle * 220f) * power * dt + R());
                for (int i = 0; i < m; i++)
                {
                    var src = R() < 0.5f ? fanLeft : fanRight;
                    var wpos = origin + rot * (src + new Vector3(R2() * 3f, R2() * 3f, -3f));
                    var back = rot * new Vector3(R2() * 3f, 0f, -(20f + throttle * 70f + R() * 12f));
                    wpos.y = Mathf.Max(wpos.y - 8f, surface + 0.8f);
                    if (water) Emit(wpos, back + Vector3.up * (R() * 3f), 0.8f + R() * 0.9f, 2.5f + R() * 2f, 9f + R() * 7f, new Color(0.9f, 0.95f, 1f, 0.3f), 0.7f, 0f);
                    else Emit(wpos, back + Vector3.up * (R() * 4f), 1.0f + R() * 1.2f, 3f + R() * 2f, 10f + R() * 8f, new Color(0.66f, 0.58f, 0.46f, 0.3f), 0.8f, 1.5f);
                }
            }
            // the inflation's burst: the air that puffs the tubes up blows the ground round them
            if (inflating > 0f)
            {
                int n = Mathf.FloorToInt(inflating * 260f * dt + R());
                for (int i = 0; i < n; i++)
                {
                    float a = R() * Mathf.PI * 2f;
                    var local = new Vector3(Mathf.Sin(a) * (halfW + 2f), 0f, Mathf.Cos(a) * (halfL + 3f));
                    var wpos = origin + rot * local; wpos.y = surface + 0.5f;
                    var outward = rot * new Vector3(Mathf.Sin(a), 0, Mathf.Cos(a));
                    if (water) Emit(wpos, outward * (14f + R() * 18f) + Vector3.up * (3f + R() * 5f), 1.0f + R() * 0.9f, 3f + R() * 2f, 10f + R() * 8f, new Color(0.9f, 0.95f, 1f, 0.36f), 1.6f, -2f);
                    else Emit(wpos, outward * (12f + R() * 20f) + Vector3.up * (2f + R() * 4f), 1.4f + R() * 1.4f, 4f + R() * 3f, 13f + R() * 9f, new Color(0.64f, 0.56f, 0.44f, 0.34f), 1.3f, 1.0f);
                }
            }
        }

        /// Draws this frame's particles (call every render frame, after Update).
        public void Draw(Camera cam, int layer)
        {
            if (material == null || count == 0) return;
            var vr = cam ? cam.transform.position : Vector3.zero;
            for (int i = 0; i < count; i++)
            {
                ref var q = ref p[i];
                float t = q.age / q.life;
                float fade = Mathf.Clamp01(t * 6f) * (1f - t) * (1f - t);        // in quickly, out slowly
                float size = Mathf.Lerp(q.size0, q.size1, 1f - (1f - t) * (1f - t));
                uint a = (uint)Mathf.RoundToInt(Mathf.Clamp01(q.alpha * fade * 1.4f) * 255f);
                gpu[i] = new ReflexParticle
                {
                    px = q.pos.x, py = q.pos.y, pz = q.pos.z, sizeX = size, sizeY = size, pivotX = 0, pivotY = 0, scale = 1f, rotation = q.spin * q.age,
                    color = (q.color & 0x00ffffffu) | (a << 24), color1 = 0xffffffffu, uScale = 1, vScale = 1, uOffset = 0, vOffset = 0, flags = 0
                };
            }
            buffer.SetData(gpu, 0, 0, count);
            props.SetBuffer(ParticlesId, buffer); props.SetInt(BaseId, 0); props.SetTexture(MainTexId, puff);
            props.SetVector(LumId, new Vector4(0.82f, 0.82f, 0.82f, 1f)); props.SetVector(DeltaId, Vector4.zero); props.SetVector(FadeId, new Vector4(0f, 1f, 0.004f, 0f)); props.SetFloat(SoftId, 1f);
            var rp = new RenderParams(material) { worldBounds = new Bounds(vr, Vector3.one * 100000f), matProps = props, shadowCastingMode = ShadowCastingMode.Off, receiveShadows = false, layer = layer };
            Graphics.RenderPrimitives(rp, MeshTopology.Triangles, count * 6, 1);
        }

        public void Clear() { count = 0; }

        public void Dispose()
        {
            buffer?.Release(); buffer = null;
            if (material) UnityEngine.Object.Destroy(material);
            if (puff) UnityEngine.Object.Destroy(puff);
            material = null; puff = null;
        }
    }

    /// The craft's sound: the two fans' whine and the cushion's roar (procedural loops, pitched by the engine's speed) and a thump for a
    /// wall. Port extra; sits on the Engine volume channel.
    public sealed class HoverSound
    {
        readonly GameObject go; readonly AudioSource whine, roar, thump;
        readonly AudioClip whineClip, roarClip, thumpClip;
        float spool;

        public HoverSound(Transform parent)
        {
            go = new GameObject("Hovercraft sound"); go.transform.SetParent(parent, false);
            whineClip = MakeWhine(); roarClip = MakeRoar(); thumpClip = MakeThump();
            whine = Make("whine", whineClip, true); roar = Make("roar", roarClip, true); thump = Make("thump", thumpClip, false);
        }

        AudioSource Make(string name, AudioClip clip, bool loop)
        {
            var s = new GameObject(name).AddComponent<AudioSource>(); s.transform.SetParent(go.transform, false);
            s.clip = clip; s.loop = loop; s.playOnAwake = false; s.spatialBlend = 0f; s.volume = 0f; s.dopplerLevel = 0f; s.bypassReverbZones = true;
            return s;
        }

        const int Rate = 32000;

        static AudioClip MakeWhine()
        {
            // two ducted fans: a harmonic stack on a 160 Hz blade-pass fundamental (whole cycles per second: it loops), a beat between the fans
            int n = Rate; var d = new float[n]; var rnd = new System.Random(3);
            double[] amp = { 1.0, 0.62, 0.45, 0.30, 0.22, 0.16, 0.10 };
            for (int i = 0; i < n; i++)
            {
                double t = (double)i / Rate, s = 0;
                for (int h = 0; h < amp.Length; h++) { int hz = 160 * (h + 1); s += amp[h] * Math.Sin(2 * Math.PI * hz * t + h * 0.7) + 0.55 * amp[h] * Math.Sin(2 * Math.PI * (hz + (h + 1) * 3) * t + h); }
                s *= 0.5 + 0.5 * Math.Cos(2 * Math.PI * 6 * t) * 0.35 + 0.35;
                d[i] = (float)(s * 0.18 + (rnd.NextDouble() - 0.5) * 0.04);
            }
            return Clip("Hovercraft fan whine", d);
        }

        static AudioClip MakeRoar()
        {
            // the air under the skirt: low-passed noise, crossfaded so it loops
            int n = Rate * 3; var raw = new float[n + Rate / 2]; var rnd = new System.Random(5);
            float a = 0, b = 0, c = 0;
            for (int i = 0; i < raw.Length; i++)
            {
                float w = (float)(rnd.NextDouble() * 2 - 1);
                a += (w - a) * 0.16f; b += (a - b) * 0.12f; c += (b - c) * 0.10f;     // three poles: a brown, rumbling spectrum
                raw[i] = (c * 5.5f + a * 0.35f);
            }
            var d = new float[n]; int x = Rate / 2;
            for (int i = 0; i < n; i++)
            {
                float v = raw[i];
                if (i < x) { float f = (float)i / x; v = raw[i] * f + raw[n + i] * (1f - f); }
                d[i] = v * 0.9f;
            }
            return Clip("Hovercraft air roar", d);
        }

        static AudioClip MakeThump()
        {
            int n = Rate / 2; var d = new float[n]; var rnd = new System.Random(9);
            for (int i = 0; i < n; i++) { double t = (double)i / Rate; double f = 70 - 40 * t * 2; d[i] = (float)((Math.Sin(2 * Math.PI * f * t) * 0.9 + (rnd.NextDouble() - .5) * 0.5 * Math.Exp(-t * 30)) * Math.Exp(-t * 7)); }
            return Clip("Hovercraft thump", d);
        }

        static AudioClip Clip(string name, float[] data)
        {
            var c = AudioClip.Create(name, data.Length, 1, Rate, false); c.SetData(data, 0); c.hideFlags = HideFlags.DontSave;
            return c;
        }

        /// spool 0..1 (the fans' speed), throttle 0..1, speed 0..1 of the craft's top speed, gain = the Engine channel x master.
        public void Tick(float dt, bool running, float spoolTarget, float throttle, float speed01, float cushion, float gain, bool paused)
        {
            spool += (spoolTarget - spool) * (1f - Mathf.Exp(-(spoolTarget > spool ? 0.9f : 1.6f) * dt));
            if (!running) spool = Mathf.MoveTowards(spool, 0, dt * 0.6f);
            bool on = spool > 0.01f && !paused;
            if (on && !whine.isPlaying) { whine.Play(); roar.Play(); }
            if (!on && whine.isPlaying) { whine.Pause(); roar.Pause(); }
            if (paused) return;
            float k = Mathf.Clamp01(cushion);
            whine.pitch = 0.45f + spool * (0.55f + throttle * 0.75f + speed01 * 0.25f);
            whine.volume = gain * Mathf.Clamp01(spool * 1.2f) * (0.16f + 0.2f * throttle);
            roar.pitch = 0.6f + 0.25f * spool + speed01 * 0.5f + throttle * 0.2f;
            roar.volume = gain * Mathf.Clamp01(spool) * (0.18f + 0.45f * k * (0.35f + speed01 * 0.65f + throttle * 0.3f));
        }

        public float Spool => spool;
        public void Thump(float strength, float gain) { thump.volume = gain * Mathf.Clamp01(strength / 60f) * 0.9f; thump.pitch = 0.9f + 0.2f * UnityEngine.Random.value; thump.Play(); }

        public void Dispose()
        {
            if (go) UnityEngine.Object.Destroy(go);
            if (whineClip) UnityEngine.Object.Destroy(whineClip); if (roarClip) UnityEngine.Object.Destroy(roarClip);
            if (thumpClip) UnityEngine.Object.Destroy(thumpClip);
        }
    }
}
