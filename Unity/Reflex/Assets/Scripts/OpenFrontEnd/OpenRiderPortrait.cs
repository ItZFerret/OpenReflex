// The rider's PORTRAIT: a head-and-shoulders picture of the player's own rider in his gear, drawn live from the game's own high-resolution
// rider model into a small render texture and shown on the hub's MOTOCARD card and at the head of the garage's RIDER tab.
//
// What the original has: its MOTOCARD (the Motocard.component of MotocardLayer / MotocardPopup: _re/formats/out/ui/layers/Shared.ENG/MotocardPopup.lyr.xml,
// scripts Motocard.lua) shows only the jersey NUMBER, the name, the career progress, the credits and the seven latest highlight icons: it has
// no portrait or avatar component, and the PlayerRider row has no icon / avatar column (RiderInterface / RiderHome / RiderTrailer / RiderFont are
// empty tables). Its only picture of the rider is the 3D garage figure that the setup pages (MotocardSetupRider: gear, skin tone) show live. So the
// portrait is a port extra built on that: the live figure, framed on the head and shoulders.
//
// How: a second FeGarage in its picture mode (layer 31, 20000 ft below the world: its camera sees nothing else, the world's camera sees nothing of it),
// the same rider build as the hero's (OpenGarageData.RiderWithGear of the equipped gear), the original post chain with UI_Compound's HDR settings, the
// scene cleared to transparent. It renders by hand, once when the gear changes (never every frame). The key light is the menu's spot (Reflex/Vehicle's
// menu term) set for the render and put back, so the portrait is lit like the hero is without touching the world.
using System;
using System.Collections.Generic;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    internal sealed class OpenRiderPortrait
    {
        /// The picture's size (pixels), its vertical field of view (degrees) and how far from the head the camera stands (feet).
        public static int Width = 480, Height = 600;
        public static float Fov = 21f, Distance = 7.0f, ShoulderDrop = 1.0f, Turn = 22f, Lift = 0.30f, Pitch = 0f, Yaw = -32f;
        /// The key spot of the portrait: gain, rim gain, the light's place relative to the eye (right, up, back of the subject, feet).
        public static float KeyGain = 1.9f, RimGain = 0.9f;
        public static Vector3 KeyOffset = new Vector3(3.4f, 3.0f, 2.2f);

        readonly Transform parent;
        FeGarage g; string key; bool rendered;
        readonly List<VisualElement> bound = new List<VisualElement>();
        public string Key => key;
        public RenderTexture Texture => g?.Texture;
        public bool Ready => rendered && g != null && g.Texture;
        public float LastBuildMs { get; private set; }

        public OpenRiderPortrait(Transform parent) { this.parent = parent; }

        /// A picture holder: it shows the portrait as soon as there is one (and keeps showing it as the gear changes).
        public void Bind(VisualElement el)
        {
            if (el == null) return;
            bound.RemoveAll(e => e == null || e.panel == null && e != el);
            if (!bound.Contains(el)) bound.Add(el);
            if (Ready) Show(el);
        }
        void Show(VisualElement el)
        {
            el.style.backgroundImage = new StyleBackground(Background.FromRenderTexture(g.Texture));
            el.style.display = DisplayStyle.Flex;
        }

        /// The own rider's spec and a key for it (the equipped gear; the default rider without a profile).
        static (RiderSpec rider, string key) OwnRider()
        {
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex && !ReflexProfile.Disabled)
            {
                try
                {
                    var open = ReflexProfile.Open();
                    if (open.IsCompletedSuccessfully && open.Result && OpenGarageData.P != null)
                    {
                        var gear = OpenGarageData.EquippedGearIds();
                        var rs = OpenGarageData.RiderWithGear(gear);
                        if (rs != null) return (rs, "own:" + string.Join(",", gear) + "|" + ReflexPlateText.ForSpecs(null, rs).Key);   // (a new name / number is drawn again: the plates)
                    }
                }
                catch (Exception e) { Debug.LogWarning("[Portrait] the equipped rider: " + e.Message); }
            }
            return (RiderSpec.AnswerRed(), "default");
        }

        /// Builds and draws the portrait when the equipped gear is not the one drawn (cheap otherwise). world: the menu world when there is one
        /// (its lighting is put back after the portrait's models were built).
        public void Refresh(OpenMenuWorld world)
        {
            var (rs, k) = OwnRider();
            if (g != null && rendered && k == key) return;
            var sw = System.Diagnostics.Stopwatch.StartNew();
            try
            {
                if (g == null)
                {
                    g = new FeGarage(parent);
                    g.camera.enabled = false;                         // (drawn by hand, below)
                    g.Resize(Width, Height);
                    g.camera.fieldOfView = Fov;
                    g.camera.farClipPlane = 60f;
                    var chain = g.camera.GetComponent<Rendering.ReflexRenderPipeline>();
                    if (chain) chain.convergeAdaptation = true;       // (one render: the eye adaptation is at its end value at once)
                    world?.ReapplyGlobals();                         // (FeGarage's picture mode sets the garage's lighting; the compound's own goes back)
                }
                g.yawOverride = Yaw;
                g.ShowSpecs(null, rs, false);
                key = k;
                Frame();
                Draw(world);
                rendered = true;
            }
            catch (Exception e)
            {
                Debug.LogWarning("[Portrait] " + e.Message);
                rendered = false;
            }
            LastBuildMs = (float)sw.Elapsed.TotalMilliseconds;
            if (rendered) { foreach (var el in bound) if (el != null && el.panel != null) Show(el); Debug.Log($"[Portrait] {key} drawn in {LastBuildMs:F0} ms (main thread), {Width}x{Height}"); }
        }

        // the camera: on the head and shoulders, a little off the rider's front (three-quarter), a touch above
        Vector3 aim, eye;
        void Frame()
        {
            if (!g.RiderBounds(out var b)) { aim = new Vector3(0, 5f, 0); eye = aim + new Vector3(2f, 0.4f, Distance); g.SetCameraNow(eye, aim); return; }
            aim = new Vector3(b.center.x, b.max.y - ShoulderDrop, b.center.z);
            eye = aim + Quaternion.Euler(Pitch, Turn, 0) * new Vector3(0, 0, Distance) + new Vector3(0, Lift, 0);
            g.SetCameraNow(eye, aim);
        }

        void Draw(OpenMenuWorld world)
        {
            var o = g.root.transform.position;
            int pos = Shader.PropertyToID("_RfxMenuSpotPos"), dir = Shader.PropertyToID("_RfxMenuSpotDir"), cone = Shader.PropertyToID("_RfxMenuSpotCone"), rim = Shader.PropertyToID("_RfxMenuRimColor");
            var p0 = Shader.GetGlobalVector(pos); var d0 = Shader.GetGlobalVector(dir); var c0 = Shader.GetGlobalVector(cone); var r0 = Shader.GetGlobalVector(rim);
            try
            {
                var look = Quaternion.LookRotation(aim - eye, Vector3.up);
                var spot = o + aim + look * KeyOffset - look * Vector3.forward * 0f;
                var d = ((o + aim) - spot).normalized;
                Shader.SetGlobalVector(pos, new Vector4(spot.x, spot.y, spot.z, KeyGain));
                Shader.SetGlobalVector(dir, new Vector4(d.x, d.y, d.z, Mathf.Cos(40f * Mathf.Deg2Rad)));
                Shader.SetGlobalVector(cone, new Vector4(Mathf.Cos(18f * Mathf.Deg2Rad), 80f, 0.45f, RimGain));
                Shader.SetGlobalVector(rim, OpenMenuWorld.SpotRimColor);
                g.camera.Render();
            }
            finally
            {
                Shader.SetGlobalVector(pos, p0); Shader.SetGlobalVector(dir, d0); Shader.SetGlobalVector(cone, c0); Shader.SetGlobalVector(rim, r0);
            }
            if (Mirrored()) TurnOver();
        }

        /// How often the picture came out mirrored top to bottom (diagnostics).
        public int TurnedOver { get; private set; }

        /// The picture is a head and shoulders: the shoulders (the bottom of the picture as it should read) cover more of it than the helmet
        /// (the top). A render texture of this camera comes out mirrored vertically on some Metal / URP set-ups (seen on the Apple M5 with
        /// Unity 6000.6.1: the same camera, same code, upright in the w-menufix run and upside down later), so the picture is looked at once
        /// (a 24 x 30 copy read back) and turned over when the helmet is at the bottom.
        bool Mirrored()
        {
            if (!g.Texture) return false;
            var small = RenderTexture.GetTemporary(24, 30, 0, RenderTextureFormat.ARGB32);
            var prev = RenderTexture.active;
            try
            {
                Graphics.Blit(g.Texture, small);
                RenderTexture.active = small;
                var t = new Texture2D(24, 30, TextureFormat.RGBA32, false);
                t.ReadPixels(new Rect(0, 0, 24, 30), 0, 0);
                var px = t.GetPixels32(); UnityEngine.Object.Destroy(t);
                int top = 0, bottom = 0;
                for (int y = 0; y < 30; y++)
                    for (int x = 0; x < 24; x++)
                    {
                        var c = px[y * 24 + x];
                        bool covered = c.r + c.g + c.b > 30;   // (the clear is black, alpha 1: only the rider's pixels count)
                        if (!covered) continue;
                        if (y >= 22) top++;          // ReadPixels row 0 is the bottom of the texture as it is drawn upright
                        else if (y < 8) bottom++;
                    }
                bool mirrored = top > bottom + 6;
                if (OpenMenuWorldProbe.Wanted || mirrored) Debug.Log($"[Portrait] coverage top {top} bottom {bottom}: " + (mirrored ? "mirrored, turned over" : "upright"));
                return mirrored;
            }
            catch (Exception e) { Debug.LogWarning("[Portrait] orientation check: " + e.Message); return false; }
            finally { RenderTexture.active = prev; RenderTexture.ReleaseTemporary(small); }
        }

        /// The picture mirrored top to bottom, in place (one GPU copy through a temporary).
        void TurnOver()
        {
            var tmp = RenderTexture.GetTemporary(g.Texture.width, g.Texture.height, 0, g.Texture.format);
            try
            {
                Graphics.Blit(g.Texture, tmp, new Vector2(1f, -1f), new Vector2(0f, 1f));
                Graphics.Blit(tmp, g.Texture);
                TurnedOver++;
            }
            finally { RenderTexture.ReleaseTemporary(tmp); }
        }

        /// Tests: the same rider framed another way and drawn again now.
        public void Reframe(float yaw, float turn, float distance, float drop, float lift, float fov)
        {
            if (g == null) return;
            Yaw = yaw; Turn = turn; Distance = distance; ShoulderDrop = drop; Lift = lift; Fov = fov;
            g.yawOverride = yaw; g.camera.fieldOfView = fov;
            var (rs, _) = OwnRider();
            g.ShowSpecs(null, rs, false);   // (the yaw is applied when the rider is stood)
            g.Restand();
            Frame(); Draw(null);
        }

        public void Destroy()
        {
            bound.Clear();
            if (g != null) { g.Destroy(); g = null; }
            rendered = false; key = null;
        }
    }
}
