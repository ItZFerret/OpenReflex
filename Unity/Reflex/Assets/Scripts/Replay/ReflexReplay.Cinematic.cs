// CINEMATIC EFFECTS for REPLAY and PHOTO MODE (w-rmphoto; the passes: Rendering/ReflexCinematic.cs). Never while riding.
//   REPLAY       OPTIONS > OPEN REFLEX > REPLAY > CINEMATIC EFFECTS (OFF / ON, default ON): every camera gets settings of its own -
//                TRACKSIDE and HELICOPTER the long lenses (a shallow focus on the rider, the background melts), CHASE / ORBIT / FREE / GAME a
//                moderate depth of field (f/5.6, f/8) that follows the rider (auto focus, racked smoothly) with a sharp zone around him that
//                grows with his distance (the ground just before and behind him stays sharp), ONBOARD and WHEEL CAM motion blur without
//                depth of field (the bars are in front of the focus plane: nothing to focus), and on all of them a 120 degree shutter while
//                the replay moves (motion blur from the motion vectors: the camera, the bikes against the background, the spinning wheels;
//                never longer than the frame's own motion, none on a held frame), subtle
//                chromatic aberration, a vignette, a little film grain, the lens flare and dirt on the sun and the lamps.
//   PHOTO MODE   FOCUS (OFF / AUTO on the subject / MANUAL), FOCUS DISTANCE, APERTURE, MOTION BLUR (the shutter time, 0 .. 1/20 s: the moment is
//                frozen, so the shutter is a real one: sub-frames of the recording around the moment are averaged - the progressive
//                preview builds up in a fraction of a second and the saved photo takes 32 .. 64 of them: the subject blurs along its own
//                motion, wheels and all, and with ORBIT, which follows the subject, the background streaks instead; each sub-frame is a picture of
//                its own moment: the temporal AA starts over, the skinned riders are re-skinned per render, the window stays inside the
//                recording), LENS EFFECTS
//                (the flare and dirt and the subtle lens look), VIGNETTE / FILM GRAIN / CHROMATIC ABERRATION on top.
using System;
using Reflex.Port.Options;
using Reflex.Port.Rendering;
using UnityEngine;

namespace Reflex.Port.Replay
{
    public sealed partial class ReflexReplay
    {
        /// OPTIONS > OPEN REFLEX > REPLAY > CINEMATIC EFFECTS: depth of field, motion blur and lens effects in the replay (PHOTO MODE has its own).
        public static bool CinematicEffects
        {
            get => ReflexSave.GetInt("OpenReflex.ReplayCinematic", 1) != 0;
            set { ReflexSave.SetInt("OpenReflex.ReplayCinematic", value ? 1 : 0); ReflexSave.Save(); }
        }

        readonly ReflexCinematicSettings cine = new ReflexCinematicSettings();
        float focusNow = 20f; bool focusSnap = true;
        bool controlsDone;
        float cineDt;

        // PHOTO MODE's shutter: this frame's offset in time, the sub-frame of the mean it is, whether the mean is done
        float shutterOffset;
        int accIndex, accFrame = -1, accSig; bool accHold, accReset;

        /// The moment the actors are posed at: PlayTime, within PHOTO MODE's shutter the sub-frame's moment.
        public float ViewTime => PlayTime + shutterOffset;

        /// The photo's shutter time (s) for MOTION BLUR amount a (0 .. 1: up to 1/20 s).
        static float ShutterSeconds(float a) => Mathf.Clamp01(a) * (1f / 20f);
        /// Sub-frames of the progressive preview / of the saved photo.
        static int PreviewFrames(float a) => Mathf.RoundToInt(Mathf.Lerp(12f, 32f, Mathf.Clamp01(a)));
        static int PhotoFrames(float a) => Mathf.RoundToInt(Mathf.Lerp(24f, 64f, Mathf.Clamp01(a)));

        /// The i-th moment inside the shutter (a low-discrepancy order: the first frame is the middle, every prefix of the sequence covers the
        /// shutter evenly), seconds from the moment.
        static float ShutterOffset(int i, float amount)
        {
            float u = 0f, f = 0.5f;
            for (int n = i; n > 0; n >>= 1) { if ((n & 1) != 0) u += f; f *= 0.5f; }
            u = (u + 0.5f) % 1f;
            return (u - 0.5f) * ShutterSeconds(amount);
        }

        /// The shutter's window inside the recording: at the LIVE moment (or the first one) half the window was past the end, where every
        /// sub-frame showed the last recorded pose - half the photo one sharp copy, the other half a smear beside it (w-replayterrain: the doubled
        /// rider at 1/80 s). Near either end the window closes (or opens) at the moment instead of being centred on it.
        float ShutterShift(float amount)
        {
            if ((LegacyShutter & 1) != 0) return 0f;
            float h = 0.5f * ShutterSeconds(amount);
            float lo = Buffer.StartTime + h - PlayTime, hi = Buffer.EndTime - h - PlayTime;
            return lo > hi ? 0.5f * (lo + hi) : Mathf.Clamp(0f, lo, hi);
        }

        /// Tests (tests/replayterrain: the before pictures at the same moment): 1 the shutter centred on the moment, 4 the saved photo's riders
        /// skinned once (the temporal AA's history kept between sub-frames: ReflexCinematic.KeepHistory).
        public static int LegacyShutter;

        bool ShutterOn => Face == View.Photo && Photo.motionBlur > 0.01f && cineAvailable;
        bool cineAvailable = true;

        // a hash of what the mean depends on: a change restarts it
        int ShutterSignature()
        {
            int h = 17;
            void A(float v) { unchecked { h = h * 31 + Mathf.RoundToInt(v * 1000f); } }
            A(PlayTime); A(Target); A((float)cam); A(Photo.orbit ? 1 : 0); A(freePos.x); A(freePos.y); A(freePos.z); A(freeYaw); A(freePitch);
            A(orbYaw); A(orbPitch); A(orbDist); A(userFov); A(Photo.roll); A(Photo.motionBlur);
            A(Photo.showRider ? 1 : 0); A(Photo.showBike ? 1 : 0); A(Photo.showOthers ? 1 : 0);
            if (camera) { A(camera.pixelWidth); A(camera.pixelHeight); }
            return h;
        }

        /// Start of the viewer's frame (before the actors are posed): the user's camera input first (this frame's camera is what the mean
        /// must match), then PHOTO MODE's sub-frame.
        void ShutterBegin()
        {
            cineDt = Mathf.Abs(frameReplayDt);
            if (camera)
            {
                Cam mode = Face == View.Photo ? (Photo.orbit ? Cam.Orbit : Cam.Free) : cam;
                if (mode == Cam.Keyframes && keys.Count == 0) mode = Cam.Free;
                UserControls(Time.unscaledDeltaTime, mode);
                controlsDone = true;
            }
            if (!ShutterOn) { shutterOffset = 0f; accFrame = -1; accSig = 0; accHold = false; return; }
            int sig = ShutterSignature();
            if (sig != accSig || accReset) { accSig = sig; accIndex = 0; accReset = false; }
            int n = PreviewFrames(Photo.motionBlur);
            if (accIndex < n) { accFrame = accIndex; accHold = false; shutterOffset = ShutterOffset(accIndex, Photo.motionBlur) + ShutterShift(Photo.motionBlur); accIndex++; }
            else { accFrame = n; accHold = true; shutterOffset = 0f; }
        }

        // ---------------------------------------------------------------------------------------- the settings

        struct CineLook { public bool dof; public float aperture, focalScale, shutter, chromatic, vignette, grain, flare, dirt; }

        static CineLook LookFor(Cam c)
        {
            // (w-replayterrain: "too much blur in front of the rider": the chase camera's f/2.8 and a 4 ft sharp zone blurred the ground just before
            // and behind the rider, and a 180 degree shutter streaked the ground the camera passes over. Now f/5.6 on the moving cameras, a sharp zone
            // that grows with the distance to the rider (DofZones), a 120 degree shutter whose streak is never longer than the frame's own motion)
            var l = new CineLook { dof = true, aperture = 5.6f, focalScale = 2.4f, shutter = 120f, chromatic = 0.2f, vignette = 0.2f, grain = 0.12f, flare = 0.4f, dirt = 0.35f };
            switch (c)
            {
                case Cam.Trackside: case Cam.Helicopter: l.aperture = 4f; l.focalScale = 1.4f; break;     // the long lenses: the fov already says 100+ mm
                case Cam.Chase: l.aperture = 5.6f; break;
                case Cam.Onboard: l.dof = false; l.shutter = 150f; l.vignette = 0.26f; l.chromatic = 0.28f; break;
                case Cam.Wheel: l.dof = false; l.shutter = 150f; l.vignette = 0.24f; break;
                case Cam.Game: l.aperture = 8f; break;
                default: break;                                                                                  // ORBIT, FREE, KEYFRAMES: f/5.6 on the rider
            }
            return l;
        }

        void CinematicClear() { ReflexCinematic.Current = null; shutterOffset = 0f; accFrame = -1; accSig = 0; focusSnap = true; }

        /// This frame's lens and camera effects, from the camera shown (REPLAY) or the photographer's settings (PHOTO MODE).
        void CinematicFrame()
        {
            bool photo = Face == View.Photo;
            if (!photo && !CinematicEffects) { ReflexCinematic.Current = null; ReflexCinematic.ReplayFrame = Time.frameCount; return; }
            var p = Photo;
            var s = cine;
            float dt = Time.unscaledDeltaTime;
            // auto focus: the rider, racked smoothly (a cut jumps)
            float subject = Mathf.Max(1f, SubjectDistance);
            if (focusSnap) { focusNow = subject; focusSnap = false; }
            else focusNow = Mathf.Lerp(focusNow, subject, Ease(7f, dt));
            if (photo)
            {
                s.dof = p.focus != 0;
                s.focusDistance = p.focus == 2 ? Mathf.Max(1f, p.focusDistance) : focusNow;
                s.aperture = Apertures[Mathf.Clamp(p.aperture, 0, Apertures.Length - 1)];
                s.focalScale = 2.4f; s.maxBlur = 0.035f; s.highlights = 1.5f; s.focusZone = 4f; s.focusZoneNear = -1f;
                s.motionBlur = false;                       // (the photo's motion blur is the shutter accumulation below)
                s.accumulateIndex = accFrame; s.accumulateHold = accHold;
                float k = p.lens ? 1f : 0f;
                s.chromatic = Mathf.Clamp01(p.chromatic + 0.2f * k);
                s.vignette = Mathf.Clamp01(p.vignette + 0.15f * k);
                s.grain = Mathf.Clamp01(p.grain + 0.12f * k);
                s.flare = 0.5f * k; s.dirt = 0.4f * k;
            }
            else
            {
                var l = LookFor(ShownCamera);
                s.dof = l.dof;
                s.focusDistance = focusNow;
                s.aperture = l.aperture; s.focalScale = l.focalScale; s.maxBlur = 0.02f; s.highlights = 1.5f;
                // the sharp zone: the whole rider and the ground around him (half the distance behind him, 0.6 of it in front: the ground
                // between the camera and the rider), at least 4 ft
                s.focusZone = Mathf.Max(4f, 0.5f * focusNow); s.focusZoneNear = Mathf.Max(4f, 0.6f * focusNow);
                // motion blur only while the replay moves (a held / paused frame is a still: nothing moved), from the frame's own motion
                s.motionBlur = cineDt > 1e-5f; s.shutterAngle = l.shutter; s.worldDt = cineDt; s.maxMotion = 0.02f; s.maxScale = 1f;
                s.accumulateIndex = -1; s.accumulateHold = false;
                s.chromatic = l.chromatic; s.vignette = l.vignette; s.grain = l.grain; s.flare = l.flare; s.dirt = l.dirt;
            }
            ReflexCinematic.Current = s;
            ReflexCinematic.ReplayFrame = Time.frameCount;
        }

        /// PHOTO MODE's photo: the shutter's sub-frames of the moment, every one rendered into the photo's target and averaged by the cinematic
        /// pass (the history is the photo's size: the preview's mean starts over afterwards).
        void ShutterRender()
        {
            var s = ReflexCinematic.Current;
            if (!ShutterOn || s == null) { camera.Render(); return; }
            int n = PhotoFrames(Photo.motionBlur);
            float keep = shutterOffset, shift = ShutterShift(Photo.motionBlur);
            // every sub-frame is rendered within this one frame: a skinned mesh (the riders) is skinned once a frame, so without this its body
            // stayed at one moment while the bike's parts moved through the shutter (w-replayterrain)
            var skins = FindObjectsByType<SkinnedMeshRenderer>(FindObjectsSortMode.None);
            var skinWas = new bool[skins.Length];
            for (int k = 0; k < skins.Length; k++) { skinWas[k] = skins[k].forceMatrixRecalculationPerRender; skins[k].forceMatrixRecalculationPerRender = (LegacyShutter & 4) == 0; }
            try
            {
                for (int i = 0; i < n; i++)
                {
                    shutterOffset = ShutterOffset(i, Photo.motionBlur) + shift;
                    ApplyPose(ViewTime, true); ApplyParticles(ViewTime);
                    controlsDone = true; UpdateCamera(0f, 0f);
                    s.accumulateIndex = i; s.accumulateHold = false;
                    camera.Render();
                }
            }
            finally
            {
                for (int k = 0; k < skins.Length; k++) if (skins[k]) skins[k].forceMatrixRecalculationPerRender = skinWas[k];
                shutterOffset = keep;
                ApplyPose(ViewTime, true); ApplyParticles(ViewTime);
                controlsDone = true; UpdateCamera(0f, 0f);
                accReset = true;
            }
        }
    }
}
