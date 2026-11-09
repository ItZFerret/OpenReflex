// The TRICK PREVIEW of the MOTOCARD's TRICK LIBRARY: the player's rider on his vehicle performs the stunt, drawn live into a small picture.
//
// What the original does: MotocardStunts.lua puts the player's rider (his gear) on his equipped vehicle in the garage in stunt mode (low-resolution
// models: SetHREnable( FALSE ), EnableStuntMode) and StuntPreview.lua's accept calls MXGarage:PreviewStunt( stuntName ), which plays the stunt on
// them. The performance of a stunt is the "<MX|ATV>Rider_<name>" perfdat of the Animations database: its rider clips (In, Hold, Out) with the
// vehicle clips of the same names (Content/ReflexStuntAnim). The tricks the physics does (Wheelie, Stoppie, Backflip, ThreeSixty, Whip) have none.
//
// How the port does it: a second FeGarage in its picture mode (the portrait's: OpenRiderPortrait; layer 31, 3000 ft beside the portrait's, its own
// camera into a small render texture, drawn by hand each frame while it is on show), the rider and the vehicle of the hero's setup (the garage's
// equipped vehicle when it is a MX Lite, MX or ATV: MotocardStunts' rule; else the first owned of those), the clips sampled with the game's own
// keyframe rules and put on the rider's bones as world matrices (the way OpenMenuWorld's push-bike walk poses its rider: FeGarage.ApplyRiderWorld),
// the vehicle moved as one piece by its OBJ_Frame track (FeGarage.SetBikeMotion). The picture is shown by the MOTOCARD's detail panel.
using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    internal sealed class OpenStuntPreview
    {
        /// The picture's size (pixels) and the camera: field of view (degrees), turn from the vehicle's left towards its front (degrees), height angle, margin (feet).
        public static int Width = 640, Height = 480;
        public static float Fov = 22f, Yaw = 34f, Pitch = 7f, Margin = 0.9f, Lift = 0.6f;
        /// The key light's offset from the subject in the camera's frame (right, up, along the view; feet) and its gain.
        public static Vector3 KeyOffset = new Vector3(9f, 9f, -10f);
        public static float KeyGain = 1.7f, RimGain = 0.9f;
        /// How the vehicle clip's frame node moves the vehicle: the clip's OBJ_Frame rotation is the node's local rotation in the vehicle's parent space
        /// (its first key is the model's own 180 degree turn about the vertical: the animation replaces it), so the vehicle turns by R(t) * R(0)^-1 about its
        /// origin and moves by the translation's change (2; 1: the same turn in the vehicle's own axes, diagnostics). The rider's clips are in the garage's
        /// space as they are (the rider's hands meet the grips; the first sweep with the frame's 180 degrees applied to him turned him about on the seat).
        public static int FrameMode = 2;
        /// Tests: nothing is posed or drawn by Tick (the fixture poses and draws by hand).
        public static bool Manual;

        readonly Transform parent;
        FeGarage g; string key; bool built;
        readonly List<VisualElement> bound = new List<VisualElement>();
        readonly List<ReflexRiderAnim.Additive> additives = new List<ReflexRiderAnim.Additive>();
        string[] names; float[] pose = new float[OriginalRider.OriginalBoneCount * 16], root = new float[16];
        Vector3 aim, eye; float distance;
        bool flipped, flipChecked;

        // the stunt on show
        string stuntName; ReflexStuntAnim anim; float t; bool playing, dirty = true;
        /// The reason nothing is shown (null: all well).
        public string Problem { get; private set; }
        public bool Ready => built && g != null && g.Texture;
        public RenderTexture Texture => g?.Texture;
        /// The picture is shown turned over (the render texture came out mirrored: Mirrored()).
        public bool Flipped => flipped;
        public bool Playing => playing;
        public float Time01 => anim == null ? 0f : Mathf.Clamp01(t / Mathf.Max(0.01f, anim.Length()));
        public string Stunt => stuntName;
        /// The stunt's performance (null: a physics trick or none chosen).
        public ReflexStuntAnim Anim => anim;
        public string Setup => key;
        public float LastBuildMs { get; private set; }
        public int Frames { get; private set; }

        public OpenStuntPreview(Transform parent) { this.parent = parent; }

        /// A picture holder: it shows the preview as soon as there is one.
        public void Bind(VisualElement el)
        {
            if (el == null) return;
            bound.RemoveAll(e => e == null || e.panel == null && e != el);
            if (!bound.Contains(el)) bound.Add(el);
            Show(el);
        }
        void Show(VisualElement el)
        {
            if (!Ready) return;
            el.style.backgroundImage = new StyleBackground(Background.FromRenderTexture(g.Texture));
            el.style.scale = flipped ? new Scale(new Vector3(1f, -1f, 1f)) : new Scale(Vector3.one);
        }

        /// The rider and the vehicle the page shows: the player's own rider in his gear, on the equipped vehicle when it is a MX Lite / MX / ATV, else the
        /// first owned of MX, ATV, MX Lite (MotocardStunts:Start's vehicleTypePreference).
        static (BikeSpec bike, RiderSpec rider, string key) OwnSetup()
        {
            var rider = RiderSpec.AnswerRed(); string rk = "default";
            try
            {
                if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex && !ReflexProfile.Disabled && OpenGarageData.P != null)
                {
                    var gear = OpenGarageData.EquippedGearIds();
                    rider = OpenGarageData.RiderWithGear(gear) ?? rider; rk = string.Join(",", gear);
                    var owned = OpenGarageData.Vehicles().Where(v => v.owned).ToList();
                    var pick = owned.FirstOrDefault(v => v.equipped && (v.type == 2 || v.type == 3 || v.type == 4));
                    foreach (int type in new[] { 3, 4, 2 }) { if (pick != null) break; pick = owned.FirstOrDefault(v => v.type == type); }
                    if (pick != null)
                    {
                        var bike = OpenGarageData.BikeOfModel(pick.id);
                        if (bike != null) return (bike, rider, "own:" + pick.id + ":" + rk);
                    }
                }
            }
            catch (Exception e) { Debug.LogWarning("[StuntPreview] the player's setup: " + e.Message); }
            return (null, rider, "none:" + rk);
        }

        /// Builds the rider and the vehicle (once per setup) and draws the first picture. world: the menu world when there is one (its lighting is put back).
        public bool Refresh(OpenMenuWorld world)
        {
            var (bike, rider, k) = OwnSetup();
            if (bike == null) { Problem = "no vehicle for the preview"; return false; }
            if (built && k == key) return true;
            var sw = System.Diagnostics.Stopwatch.StartNew();
            try
            {
                if (g == null)
                {
                    g = new FeGarage(parent, new Vector3(3000f, -20000f, 0f), null, FeGarage.Layer);
                    g.camera.enabled = false;
                    g.Resize(Width, Height);
                    g.camera.fieldOfView = Fov; g.camera.nearClipPlane = 1f; g.camera.farClipPlane = 140f;
                    var chain = g.camera.GetComponent<Rendering.ReflexRenderPipeline>();
                    if (chain) chain.convergeAdaptation = true;
                    g.highRes = false;          // MotocardStunts: SetHREnable( FALSE ), "use low res for stunts"
                    g.stageFloor = true;
                    world?.ReapplyGlobals();
                }
                g.ShowSpecs(bike, rider, true);
                g.HoldIdle = true; g.ShowStand(false);
                var rm = g.RiderModel?.rider;
                if (rm == null || rm.bones == null) throw new InvalidOperationException("the rider was not built");
                names = rm.bones.Take(OriginalRider.OriginalBoneCount).Select(b => b ? b.name : "").ToArray();
                key = k; built = true; Problem = null; dirty = true; flipChecked = false;
                PoseRest();
                Frame();
                Draw();
            }
            catch (Exception e) { Problem = e.Message; built = false; Debug.LogWarning("[StuntPreview] " + e); }
            LastBuildMs = (float)sw.Elapsed.TotalMilliseconds;
            if (built)
            {
                foreach (var el in bound) if (el != null && el.panel != null) Show(el);
                Debug.Log($"[StuntPreview] {key} built in {LastBuildMs:F0} ms (main thread), {Width}x{Height}, {(bike.atv ? "ATV" : "bike")}");
            }
            return built;
        }

        /// The vehicle kind of the setup: the ATV's performances are the ATV's own.
        public bool Atv => g?.ShownBike?.atv ?? false;

        /// Chooses the stunt shown (by the StuntTable name; null: none, the rider seated at rest). Loads its clips; false for a trick that has none.
        public bool SetStunt(string stunt)
        {
            playing = false; t = 0; dirty = true;
            stuntName = stunt; anim = null;
            if (string.IsNullOrEmpty(stunt) || !built) { PoseRest(); return false; }
            anim = ReflexStuntAnim.Load(stunt, Atv);
            if (anim == null) { PoseRest(); return false; }
            PoseAt(0f);
            return true;
        }

        /// PreviewStunt: plays the chosen stunt from its start.
        public bool Play()
        {
            if (anim == null || !built) return false;
            t = 0f; playing = true; dirty = true;
            return true;
        }

        /// The clip's time shown now (tests: poses and draws at seconds into the performance).
        public void Seek(float seconds) { if (anim == null) return; t = Mathf.Clamp(seconds, 0f, anim.Length()); playing = false; PoseAt(t); Draw(); }

        /// Called each frame while the page is on show: the performance advances and the picture is drawn again when it changed.
        public void Tick(float dt)
        {
            if (!built || Manual) return;
            if (playing && anim != null)
            {
                t += dt; dirty = true;
                float len = anim.Length();
                if (t >= len + 0.35f) { playing = false; t = 0f; PoseAt(0f); }
                else PoseAt(Mathf.Min(t, len));
            }
            if (dirty) { Draw(); dirty = false; }
        }

        // ------------------------------------------------------------------------------------------------ posing

        // The seated rider at rest: the first clip's first frame (or, with no stunt, the first key of the plain barhop's In: the seat the clips start from)
        void PoseRest()
        {
            if (!built) return;
            var a = anim ?? ReflexStuntAnim.Load("BarHop_Air", Atv);
            if (a != null) PoseWith(a, 0f); else g.SetBikeMotion(Matrix4x4.identity, 0, 0);
        }

        void PoseAt(float seconds) { if (anim != null) PoseWith(anim, seconds); }

        static void Rows(Matrix4x4 m, float[] r)
        {
            for (int i = 0; i < 3; i++) for (int k = 0; k < 4; k++) r[i * 4 + k] = k == 3 ? 0 : m[k, i];
            r[12] = m[0, 3]; r[13] = m[1, 3]; r[14] = m[2, 3]; r[15] = 1;
        }

        void PoseWith(ReflexStuntAnim a, float seconds)
        {
            var b = a.At(seconds, out float local);
            if (b == null) return;
            var origin = g.root.transform.position;
            var o = g.BikeRestOrigin;
            var first = a.First?.vehicle;
            Matrix4x4 motion = Matrix4x4.identity, place;
            if (b.vehicle != null && b.vehicle.HasFrame && first != null && first.HasFrame)
            {
                b.vehicle.Frame(local, out var p, out var r);
                first.Frame(0f, out var p0, out var r0);
                var rel = FrameMode == 1 ? Quaternion.Inverse(r0) * r : r * Quaternion.Inverse(r0);
                motion = Matrix4x4.Translate(o + (p - p0)) * Matrix4x4.Rotate(rel) * Matrix4x4.Translate(-o);
                place = Matrix4x4.Translate(origin + o);
            }
            else place = Matrix4x4.Translate(origin + o);
            g.SetBikeMotion(motion, 0, 0);
            Rows(place, root);
            a.Additives(b.start + local, additives);   // the ATV performances' ATVRider_AD_* blocks laid over the pose
            b.rider.World(local, names, g.RiderModel.rider.boneParents, root, pose, additives);
            g.ApplyRiderWorld(pose);
            dirty = true;
        }

        // ------------------------------------------------------------------------------------------------ the camera and the picture

        void Frame()
        {
            Bounds all = default; bool any = false;
            if (g.BikeBounds(out var bb)) { all = bb; any = true; }
            if (g.RiderBounds(out var rb)) { if (any) all.Encapsulate(rb); else { all = rb; any = true; } }
            if (!any) { aim = new Vector3(0, 3f, 0); distance = 20f; }
            else
            {
                aim = all.center + new Vector3(0f, Lift, 0f);
                float radius = all.extents.magnitude + Margin;
                distance = radius / Mathf.Tan(Fov * 0.5f * Mathf.Deg2Rad);
            }
            // from the vehicle's left towards its front (it faces +z, its left is -x)
            var dir = Quaternion.Euler(-Pitch, 0, 0) * new Vector3(-Mathf.Cos(Yaw * Mathf.Deg2Rad), 0f, Mathf.Sin(Yaw * Mathf.Deg2Rad));
            eye = aim + dir.normalized * distance;
            g.SetCameraNow(eye, aim);
        }

        /// The picture drawn now (the key light of the menu's spot set for the render and put back, as the portrait's).
        public void Draw()
        {
            if (!built || g == null) return;
            int pos = Shader.PropertyToID("_RfxMenuSpotPos"), dir = Shader.PropertyToID("_RfxMenuSpotDir"), cone = Shader.PropertyToID("_RfxMenuSpotCone"), rim = Shader.PropertyToID("_RfxMenuRimColor");
            var p0 = Shader.GetGlobalVector(pos); var d0 = Shader.GetGlobalVector(dir); var c0 = Shader.GetGlobalVector(cone); var r0 = Shader.GetGlobalVector(rim);
            try
            {
                var o = g.root.transform.position;
                var look = Quaternion.LookRotation(aim - eye, Vector3.up);
                var spot = o + aim + look * KeyOffset;
                var d = ((o + aim) - spot).normalized;
                Shader.SetGlobalVector(pos, new Vector4(spot.x, spot.y, spot.z, KeyGain));
                Shader.SetGlobalVector(dir, new Vector4(d.x, d.y, d.z, Mathf.Cos(48f * Mathf.Deg2Rad)));
                Shader.SetGlobalVector(cone, new Vector4(Mathf.Cos(26f * Mathf.Deg2Rad), 80f, 0.45f, RimGain));
                Shader.SetGlobalVector(rim, OpenMenuWorld.SpotRimColor);
                g.camera.Render();
            }
            finally
            {
                Shader.SetGlobalVector(pos, p0); Shader.SetGlobalVector(dir, d0); Shader.SetGlobalVector(cone, c0); Shader.SetGlobalVector(rim, r0);
            }
            Frames++;
            if (!flipChecked) { flipChecked = true; bool f = Mirrored(); if (f != flipped) { flipped = f; foreach (var el in bound) if (el != null && el.panel != null) Show(el); } }
        }

        /// The render texture of this camera comes out mirrored top to bottom on some Metal / URP set-ups (OpenRiderPortrait.Mirrored): the picture is looked
        /// at once (a small copy read back): the vehicle's wheels are wider than the rider's helmet, so the bottom of the picture covers more than the top.
        bool Mirrored()
        {
            if (!g.Texture) return false;
            var small = RenderTexture.GetTemporary(32, 24, 0, RenderTextureFormat.ARGB32);
            var prev = RenderTexture.active;
            try
            {
                Graphics.Blit(g.Texture, small);
                RenderTexture.active = small;
                var t2 = new Texture2D(32, 24, TextureFormat.RGBA32, false);
                t2.ReadPixels(new Rect(0, 0, 32, 24), 0, 0);
                var px = t2.GetPixels32(); UnityEngine.Object.Destroy(t2);
                int top = 0, bottom = 0;
                for (int y = 0; y < 24; y++)
                    for (int x = 0; x < 32; x++)
                    {
                        var c = px[y * 32 + x];
                        if (c.a < 8 || c.r + c.g + c.b < 24) continue;   // (the clear is transparent black)
                        if (y >= 17) top++; else if (y < 7) bottom++;
                    }
                bool mirrored = top > bottom + 8;
                Debug.Log($"[StuntPreview] coverage top {top} bottom {bottom}: " + (mirrored ? "mirrored, shown turned over" : "upright"));
                return mirrored;
            }
            catch (Exception e) { Debug.LogWarning("[StuntPreview] orientation check: " + e.Message); return false; }
            finally { RenderTexture.active = prev; RenderTexture.ReleaseTemporary(small); }
        }

        public void Destroy()
        {
            bound.Clear();
            if (g != null) { g.Destroy(); g = null; }
            built = false; key = null; anim = null; stuntName = null; playing = false;
        }
    }
}
