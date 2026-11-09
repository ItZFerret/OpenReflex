// OPTIONS > CAMERA & HUD > FIRST-PERSON VIEW: the Dynamic FPV camera (OriginalOnboardCamera). w-fpvrevert: the old camera's rows again (as at
// 7a9c99ff), every one a ReflexFpv.* key (ReflexSave; ReflexSettings.Onboard* holds the defaults and ranges), stored and then read by a running
// camera (ReflexClientLive.FpvChanged; the camera also reads again on any change of a key). Kept from w-fpvfix: the status line (which camera is on
// screen: these rows drive DYNAMIC FPV only; why DYNAMIC FPV shows the game's camera when it falls back), USE DYNAMIC FPV, and rows the HEAD MODE does
// not use greyed with the reason in their value (FpvRowUnused). New in w-fpvrevert: HEAD BOB, HEAD SMOOTHING, VIEW SMOOTHING; TILT and PITCH say
// what they do in their value. Then CAMERA SHARING's FIRST-PERSON SETUPS (ReflexOptionsMenu.CameraShare.cs).
using System;
using System.Globalization;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Options
{
    public sealed partial class ReflexOptionsMenu
    {
        static readonly CultureInfo FpvCi = CultureInfo.InvariantCulture;

        void FpvRows(VisualElement panel)
        {
            Section(panel, "FIRST-PERSON VIEW");
            ReflexSettings.MigrateFpvConfig();
            const string P = "ReflexFpv.";
            float F(string k, float d) => ReflexSave.GetFloat(P + k, d);
            int I(string k, int d) => ReflexSave.GetInt(P + k, d);
            void SetF(string k, float v) { ReflexSave.SetFloat(P + k, v); ReflexClientLive.FpvChanged(); }
            void SetI(string k, int v) { ReflexSave.SetInt(P + k, v); ReflexClientLive.FpvChanged(); }
            int Mode() => ReflexSettings.OnboardHeadMode;
            // a row the current HEAD MODE does not use: greyed (still editable: it applies again in the other modes), its value says why
            VisualElement Row(VisualElement row, string id)
            {
                Named(row, id);
                void Apply() { bool off = FpvRowUnused(id, Mode()) != null; row.style.opacity = off ? new StyleFloat(0.45f) : new StyleFloat(StyleKeyword.Null); }
                Apply(); row.schedule.Execute(Apply).Every(250);
                return row;
            }
            Func<string> Why(string id, Func<string> value) => () => { string off = FpvRowUnused(id, Mode()); return off == null ? value() : value() + "  · " + off; };
            void Spin(string label, string id, Func<string> value, Action<int> adjust) => Row(OptUi.Spinner(label, Why(id, value), adjust, panel), id);

            // which camera is on screen: these rows drive DYNAMIC FPV only
            var status = Keep(panel, FpvStatus()); status.name = "fpv-status";
            status.schedule.Execute(() => { var t = FpvStatus(); if (t != status.text) status.text = t; }).Every(250);
            var use = Named(OptUi.Button("USE DYNAMIC FPV", "mxa-link", () => { ReflexClientLive.SelectCamera(4); OptUi.Play("Select"); }, panel), "fpv-use");
            Note(panel, "Makes DYNAMIC FPV the camera now and for every event (the Camera action and F7 still cycle). The game's own 1ST PERSON is a different camera: these settings do not change it.");
            void ShowUse() { bool on = FpvSelected(); use.style.display = on ? DisplayStyle.None : DisplayStyle.Flex; }
            ShowUse(); use.schedule.Execute(ShowUse).Every(250);

            Spin("FIELD OF VIEW", "fpv-fov", () => F("fov", OriginalOnboardCamera.DefaultFov).ToString("0.#", FpvCi) + "°",
                d => SetF("fov", Mathf.Clamp(Mathf.Round(F("fov", OriginalOnboardCamera.DefaultFov) + d), 40f, 140f)));
            Note(panel, "How wide the first-person view is (measured at 4:3; wider screens keep the same height).");
            Spin("TILT", "fpv-tilt", () => { int p = Mathf.RoundToInt(F("tilt", OriginalOnboardCamera.DefaultTilt) * 100f); return p + "%" + (p == 0 ? "  (LEVEL)" : p == 100 ? "  (WITH THE BIKE)" : ""); },
                d => SetF("tilt", Mathf.Clamp01(Mathf.Round((F("tilt", OriginalOnboardCamera.DefaultTilt) + d * 0.05f) * 100f) / 100f)));
            Note(panel, "How much the horizon rolls with the bike when it leans: 0% keeps it level, 100% rolls fully with the bike.");
            Spin("PITCH", "fpv-pitch", () => { float p = F("pitch", OriginalOnboardCamera.DefaultPitch); return $"{p.ToString("0.00", FpvCi)}  ({(p * 20f + 15f).ToString("0.#", FpvCi)}° down)"; },
                d => SetF("pitch", Mathf.Clamp(Mathf.Round((F("pitch", OriginalOnboardCamera.DefaultPitch) + d * 0.01f) * 100f) / 100f, -1f, 1f)));
            Note(panel, "How far the view looks down (more shows more of the bars and the ground ahead).");
            string[] ca = { "OFF", "TRACK", "LEAN" };
            Spin("CORNER ANTICIPATION", "fpv-ca", () => ca[Mathf.Clamp(I("ca", 0), 0, 2)], d => SetI("ca", ((I("ca", 0) + d) % 3 + 3) % 3));
            Note(panel, "Turns the view into corners: TRACK looks along the race line ahead, LEAN follows the bike's lean. F11 cycles it in a race.");
            Spin("CORNER ANTICIPATION SCALE", "fpv-cascale", () => F("caScale", OriginalOnboardCamera.DefaultCaScale).ToString("0.00", FpvCi),
                d => SetF("caScale", Mathf.Clamp01(Mathf.Round((F("caScale", OriginalOnboardCamera.DefaultCaScale) + d * 0.05f) * 100f) / 100f)));
            Note(panel, "How far TRACK turns the view.");
            Spin("LEAN HEADING SCALE", "fpv-leanhead", () => F("leanHead", OriginalOnboardCamera.DefaultLeanHeadingScale).ToString("0.0", FpvCi),
                d => SetF("leanHead", Mathf.Clamp(Mathf.Round((F("leanHead", OriginalOnboardCamera.DefaultLeanHeadingScale) + d * 0.5f) * 10f) / 10f, 0f, 60f)));
            Note(panel, "How far LEAN turns the view: degrees at 50° of lean.");
            Named(OptUi.Toggle("FREE LOOK", () => I("free", 0) != 0, v => SetI("free", v ? 1 : 0), panel), "fpv-free");
            Note(panel, "Look around with the mouse (the game's mouse lean is off meanwhile). F12 in a race.");
            string[] heads = { "CHASSIS", "DYNAMIC", "TRUE" };
            Named(OptUi.Spinner("HEAD MODE", () => heads[Mode()], d => SetI("mode", (Mode() + d + 3) % 3), panel), "fpv-mode");
            Note(panel, "CHASSIS: fixed to the bike. DYNAMIC: follows the head with a steadied horizon. TRUE: follows the posed head and gaze. F8 in a race.");
            Row(OptUi.Toggle("LEAN PITCH", () => I("leanPitch", 1) != 0, v => SetI("leanPitch", v ? 1 : 0), panel), "fpv-leanpitch");
            Note(panel, "The view looks further down while the rider leans forward.");
            string Pct(float v) => Mathf.RoundToInt(v * 100f) + "%";
            string Sec(float v) => v <= 0f ? "OFF" : v.ToString("0.000", FpvCi) + " S";
            Spin("HEAD BOB", "fpv-headbob", () => Pct(F("headBob", OriginalOnboardCamera.DefaultHeadBob)),
                d => SetF("headBob", Mathf.Clamp01(Mathf.Round((F("headBob", OriginalOnboardCamera.DefaultHeadBob) + d * 0.05f) * 100f) / 100f)));
            Note(panel, "How much the view moves with the rider's head as he absorbs bumps, crouches and lands. 0% turns that movement off (the view still follows the lean).");
            Spin("HEAD SMOOTHING", "fpv-headsmooth", () => Sec(F("headSmooth", OriginalOnboardCamera.DefaultHeadSmoothing)),
                d => SetF("headSmooth", Mathf.Clamp(Mathf.Round((F("headSmooth", OriginalOnboardCamera.DefaultHeadSmoothing) + d * 0.005f) * 1000f) / 1000f, 0f, 0.25f)));
            Note(panel, "How softly the eye follows the head: more takes the edge off sharp bumps, less is more direct.");
            Spin("VIEW SMOOTHING", "fpv-viewsmooth", () => Sec(F("viewSmooth", OriginalOnboardCamera.DefaultViewSmoothing)),
                d => SetF("viewSmooth", Mathf.Clamp(Mathf.Round((F("viewSmooth", OriginalOnboardCamera.DefaultViewSmoothing) + d * 0.01f) * 100f) / 100f, 0f, 0.25f)));
            Note(panel, "Softens how quickly the view pitches and rolls with the ground (OFF: as the original Dynamic FPV). Turning stays immediate.");
            void Unit(string label, string id, string key, float fallback) => Spin(label, id, () => F(key, fallback).ToString("0.00", FpvCi), d => SetF(key, Mathf.Clamp01(Mathf.Round((F(key, fallback) + d * .05f) * 100f) / 100f)));
            Unit("TRUE SMOOTHING", "fpv-truesmooth", "trueSmooth", .35f); Unit("TRUE HEAD ROLL", "fpv-trueroll", "trueRoll", 1f);
            Unit("TRUE YAW SMOOTHING", "fpv-trueyaw", "trueYaw", 0f); Unit("TRUE PITCH SMOOTHING", "fpv-truepitch", "truePitch", 0f);
            void Offset(string label, string id, string key) => Named(OptUi.Spinner(label, () => F(key, 0).ToString("+0.00;-0.00;0.00", FpvCi) + " FT", d => SetF(key, Mathf.Clamp(Mathf.Round((F(key, 0) + d * .05f) * 100f) / 100f, -2, 2)), panel), id);
            Offset("EYE UP / DOWN", "fpv-offy", "offY"); Offset("EYE FORWARD / BACK", "fpv-offz", "offZ"); Offset("EYE RIGHT / LEFT", "fpv-offx", "offX");
            Note(panel, "F2 in first person adjusts the eye live: W/S forward/back, Q/E down/up, A/D left/right, Shift fine, R reset, Enter/Esc done.");
            Row(OptUi.Toggle("HEAD SHIFT (ACCELERATION)", () => I("shift", 0) != 0, v => SetI("shift", v ? 1 : 0), panel), "fpv-shift");
            Note(panel, "The eye sways with braking, launches and landings.");
            Spin("HEAD SHIFT STRENGTH", "fpv-shiftstrength", () => ((ReflexSettings.OnboardShiftLong + ReflexSettings.OnboardShiftLat + ReflexSettings.OnboardShiftVert) / 3f).ToString("0.00", FpvCi), d =>
            {
                float v = Mathf.Clamp(Mathf.Round(((ReflexSettings.OnboardShiftLong + ReflexSettings.OnboardShiftLat + ReflexSettings.OnboardShiftVert) / 3f + d * .1f) * 10f) / 10f, 0, 3);
                foreach (string key in new[] { "shiftStrength", "shiftLong", "shiftLat", "shiftVert" }) ReflexSave.SetFloat(P + key, v);
                ReflexClientLive.FpvChanged();
            });
            Note(panel, "How much HEAD SHIFT moves the eye. 0 turns the sway off.");
            void Gain(string label, string id, string key) => Spin(label, id, () => F(key, F("shiftStrength", 1)).ToString("0.00", FpvCi), d => SetF(key, Mathf.Clamp(Mathf.Round((F(key, F("shiftStrength", 1)) + d * .1f) * 10f) / 10f, 0, 3)));
            Gain("HEAD SHIFT LONGITUDINAL", "fpv-shiftlong", "shiftLong"); Gain("HEAD SHIFT LATERAL", "fpv-shiftlat", "shiftLat"); Gain("HEAD SHIFT VERTICAL", "fpv-shiftvert", "shiftVert");
            Named(OptUi.Button("RESET TO DEFAULTS", "mxa-link", () =>
            {
                ReflexSettings.ResetOnboard();
                var rt = ReflexClientLive.Runtime; if (rt && rt.onboard) rt.onboard.Load();
                ReflexSettings.Save(); ShowOptionsAt(_tab, "fpv-reset");
            }, panel), "fpv-reset");
            Note(panel, "Puts every first-person setting back to its default (your eye offset too).");
            FpvShareRows(panel);   // (CAMERA SHARING: FIRST-PERSON SETUPS, share codes: ReflexOptionsMenu.CameraShare.cs)
        }

        /// DYNAMIC FPV is the selected camera (the race's, or the one every event starts on).
        static bool FpvSelected() { var rt = ReflexClientLive.Runtime; return (rt ? rt.cameraMode : ReflexSettings.CameraMode) == 1; }

        /// The page's first line: what these rows drive and the camera on screen (a fallback of DYNAMIC FPV to the game's camera with its reason).
        internal static string FpvStatus()
        {
            var rt = ReflexClientLive.Runtime; bool race = rt && rt.Running;
            string name = ReflexClientLive.CameraName;
            string s = "These settings drive DYNAMIC FPV. " + (race ? "You're on: " : "Your camera: ") + name;
            if (!FpvSelected())
            {
                if (ReflexClientLive.CameraIndex == 3) s += " - the game's own first-person camera, which these settings do not change. Pick DYNAMIC FPV (below, or the Camera action / F7).";
                else s += " - pick DYNAMIC FPV (below, or the Camera action / F7) to use them.";
                return s;
            }
            if (race && rt.onboard && !rt.onboard.Active)
            {
                string why = rt.onboard.FallbackText;
                s += why.Length > 0 ? " - showing the game's camera for now: " + why + "." : " - it takes over when the ride starts.";
            }
            return s;
        }

        /// Why a FIRST-PERSON VIEW row does nothing in this HEAD MODE (null: it applies). The camera's own rules (OriginalOnboardCamera.Apply /
        /// ApplyTrue): TRUE follows the posed head's own gaze (TILT, the corner look, LEAN PITCH, HEAD BOB / HEAD SMOOTHING, VIEW SMOOTHING and HEAD SHIFT
        /// are not part of it; PITCH acts through TRUE PITCH SMOOTHING); CHASSIS keeps the eye on the riding posture (no HEAD BOB).
        internal static string FpvRowUnused(string id, int headMode)
        {
            if (headMode == OriginalOnboardCamera.HeadModeTrue)
                switch (id)
                {
                    case "fpv-tilt": case "fpv-ca": case "fpv-cascale": case "fpv-leanhead": case "fpv-leanpitch": case "fpv-headbob": case "fpv-headsmooth": case "fpv-viewsmooth":
                    case "fpv-shift": case "fpv-shiftstrength": case "fpv-shiftlong": case "fpv-shiftlat": case "fpv-shiftvert":
                        return "NOT USED IN TRUE";
                    default: return null;
                }
            if (id == "fpv-truesmooth" || id == "fpv-trueroll" || id == "fpv-trueyaw" || id == "fpv-truepitch") return "TRUE ONLY";
            if (headMode == OriginalOnboardCamera.HeadModeChassis && id == "fpv-headbob") return "OFF IN CHASSIS";
            return null;
        }
    }
}
