// SPEED BLUR (OPTIONS > VIDEO, ENHANCED only; not part of the original renderer): motion blur while riding, stronger the faster the camera
// moves. It drives the replay's motion blur pass (ReflexCinematic: URP motion vectors, the camera and every moving object, the spinning
// wheels) with a shutter that opens with speed: nothing below ~15 mph, the full shutter from ~65 mph. The chase camera moves with the bike,
// so the bike and rider stay sharp while the ground, the scenery and the wheels streak. A replay or photo mode that draws its own effects
// this frame wins (ReflexCinematic.ReplayFrame); a jump of the camera (a reset, a cut) closes the shutter for a few frames.
using UnityEngine;

namespace Reflex.Port.Rendering
{
    [DisallowMultipleComponent]
    public sealed class ReflexSpeedBlur : MonoBehaviour
    {
        /// 0 OFF, 1 SUBTLE, 2 STRONG (save key OpenReflex.SpeedBlur).
        public static int Setting
        {
            get => Mathf.Clamp(Options.ReflexSave.GetInt("OpenReflex.SpeedBlur", 1), 0, 2);
            set { Options.ReflexSave.SetInt("OpenReflex.SpeedBlur", Mathf.Clamp(value, 0, 2)); Options.ReflexSave.Save(); }
        }
        public static readonly string[] Names = { "OFF", "SUBTLE", "STRONG" };

        const float MphToFtPerS = 1.4666667f;
        const float StartMph = 15f, FullMph = 65f;
        static readonly float[] MaxShutter = { 0f, 140f, 240f };   // degrees of a 1/60 s frame at full speed

        readonly ReflexCinematicSettings live = new ReflexCinematicSettings { dof = false, motionBlur = true };
        Camera cam;
        Vector3 lastPos; bool hasLast; float speed; int quiet;
        bool wrote;

        void OnDisable() { Release(); }

        void LateUpdate()
        {
            if (!cam) cam = GetComponent<Camera>();
            var tr = cam ? cam.transform : transform;
            float dt = Time.unscaledDeltaTime;
            Vector3 p = tr.position;
            float v = hasLast && dt > 1e-4f ? (p - lastPos).magnitude / dt : 0f;
            lastPos = p; hasLast = true;
            if (v > 300f) { quiet = 3; v = 0f; }                     // a reset / a camera cut: no streak across the jump
            if (quiet > 0) { quiet--; v = 0f; }
            speed = Mathf.Lerp(speed, v, 1f - Mathf.Exp(-dt * 8f));  // a steady shutter (no flutter from bumps)

            int setting = Setting;
            bool on = setting > 0 && ReflexEnhancedLighting.Requested && !ReplayOwns() && Time.timeScale > 0f;
            if (!on) { Release(); return; }
            float mph = speed / MphToFtPerS;
            float t = Mathf.Clamp01((mph - StartMph) / (FullMph - StartMph));
            t = t * t * (3f - 2f * t);
            if (t <= 0.001f) { Release(); return; }
            live.shutterAngle = MaxShutter[setting] * t;
            live.worldDt = 0f;
            live.maxMotion = setting == 2 ? 0.06f : 0.04f;
            ReflexCinematic.Current = live;
            wrote = true;
        }

        static bool ReplayOwns() => ReflexCinematic.ReplayFrame >= Time.frameCount - 1;

        void Release()
        {
            if (wrote && ReflexCinematic.Current == live) ReflexCinematic.Current = null;
            wrote = false;
        }
    }
}
