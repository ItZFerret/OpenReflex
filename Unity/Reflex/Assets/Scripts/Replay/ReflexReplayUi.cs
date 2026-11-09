// The REPLAY / PHOTO MODE screens over the world (ReflexReplay, after Open Unleashed's MxaReplayUi), UI Toolkit in the OPEN REFLEX menu
// look (OuMenu / OuModern plus Resources/OpenFrontEnd/OuReplay.uss):
//   REPLAY      the badge and the track (top left), the followed rider's card (top right: name, place, lap, speed), the transport at
//               the bottom: the timeline (click or drag to scrub; the white tick is the live moment), start / back 5 s / play /
//               forward 5 s / end, the time, SPEED, CAMERA and RIDER pickers, PHOTO MODE and EXIT; the KEYFRAMES row (ADD KEY /
//               DELETE KEY / CLEAR / AIM; the keys are diamonds on the timeline, a click jumps to one) for the KEYFRAMES camera
//   PHOTO MODE  the settings panel on the right (D-pad / arrows move, left / right change a value, A / Enter press; the mouse clicks
//               the arrows or the bars), TAKE PHOTO, the moment strip (bottom left: scrub to another moment), the aspect mask, the
//               composition grid and the logo's place over the view; a flash and a toast with the saved shot's thumbnail
// H / X (Square) hides the UI (the aspect mask and the logo stay: they are the composition). The keys are ReflexReplay's; the hint bar
// shows the keyboard's or the pad's.
using System;
using System.Collections.Generic;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.Options;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UIElements;

namespace Reflex.Port.Replay
{
    public sealed class ReflexReplayUi : MonoBehaviour
    {
        ReflexReplay r;
        VisualElement root, layer, top, transport, panel, moment, hints, flash, toast, thumb, hiddenNote, grid, logo;
        readonly VisualElement[] mask = new VisualElement[4];
        Label badge, sub, riderName, riderInfo, time, momentTime, toastTitle, toastPath, postNote;
        VisualElement fill, headDot, momentFill, momentHead, keyMarks, keyRow;
        Label keyNote, aimValue;
        float keyNoteAt = -10f;
        readonly List<VisualElement> keyDiamonds = new List<VisualElement>();
        ReflexReplayIcon playIcon;
        Label speedValue, camValue, riderValue;
        OuMenuInput input;
        readonly OuNav nav = new OuNav();
        readonly OuStepper stepper = new OuStepper();
        readonly List<(VisualElement row, Action refresh)> rows = new List<(VisualElement, Action)>();
        float flashAt = -10f, toastAt = -10f, hiddenAt;
        bool hintsPad, hintsBuilt;
        ReflexReplay.View hintsFace;
        int gridShown = -1;
        Rect frameShown; float maskAspect = -1f; int logoShown = -1;
        bool dragging, wasPlaying;
        VisualElement shoot;

        public UIDocument Document { get; private set; }
        /// The mouse is over the PHOTO MODE panel (its wheel scrolls the list, not the lens).
        public bool PointerOverPanel { get; private set; }
        /// The UI's root (tests).
        public VisualElement Root => root;

        public void Build(ReflexReplay replay)
        {
            r = replay;
            OuUi.EnsureEventSystem();
            root = OuUi.CreateDocument(gameObject, 90);
            Document = GetComponent<UIDocument>();
            OuTheme.Apply(root);
            var uss = Resources.Load<StyleSheet>(OuUi.ResourceRoot + "OuReplay");
            if (uss && !root.styleSheets.Contains(uss)) root.styleSheets.Add(uss);
            root.AddToClassList("ou-replay");
            layer = OuUi.Div("rp-layer", root);
            layer.style.position = Position.Absolute;
            layer.style.left = layer.style.top = layer.style.right = layer.style.bottom = 0;
            layer.pickingMode = PickingMode.Ignore;

            for (int i = 0; i < 4; i++) { mask[i] = OuUi.Div("rp-mask", layer); mask[i].pickingMode = PickingMode.Ignore; }
            grid = OuUi.Div("rp-grid", layer);
            grid.pickingMode = PickingMode.Ignore;
            logo = OuUi.Div("rp-logo", layer);
            logo.pickingMode = PickingMode.Ignore;
            if (ReflexReplay.LogoTexture) logo.style.backgroundImage = ReflexReplay.LogoTexture;

            BuildTop();
            BuildTransport();
            BuildPanel();
            BuildMoment();

            hints = OuUi.Div("mxa-hints", layer);
            hiddenNote = OuUi.Div("rp-hidden-note", layer);
            var hk = OuUi.Div("mxa-hint", hiddenNote);
            OuUi.Text("H", "mxa-key", hk);
            OuUi.Text("SHOW UI", "mxa-hint-label", hk);

            toast = OuUi.Div("rp-toast", layer);
            toast.pickingMode = PickingMode.Ignore;
            thumb = OuUi.Div("rp-thumb", toast);
            var tt = OuUi.Div("rp-toast-text", toast);
            toastTitle = OuUi.Text("PHOTO SAVED", "rp-toast-title", tt);
            toastPath = OuUi.Text("", "rp-toast-path", tt);

            flash = OuUi.Div("rp-flash", layer);
            flash.pickingMode = PickingMode.Ignore;
            nav.Screen = panel;
            nav.OnCancel = null;   // ReflexReplay handles Esc / B
            Hide();
        }

        void OnDestroy() { input?.Dispose(); }

        public void Show()
        {
            root.style.display = DisplayStyle.Flex;
            if (input == null) { input = new OuMenuInput(); input.Enable(false); }   // (only what this screen injects: ReflexReplay owns the keys)
            FaceChanged();
        }

        public void Hide()
        {
            if (root != null) root.style.display = DisplayStyle.None;
            input?.Dispose();
            input = null;
            PointerOverPanel = false;
        }

        public void FaceChanged()
        {
            if (r == null) return;
            bool photo = r.Face == ReflexReplay.View.Photo, hidden = r.UiHidden;
            Show(top, !hidden);
            Show(top.Q(className: "rp-rider"), !photo);   // PHOTO MODE: the panel's SUBJECT row instead
            Show(transport, !hidden && !photo);
            Show(panel, !hidden && photo);
            Show(moment, !hidden && photo);
            Show(hints, !hidden);
            Show(hiddenNote, hidden);
            if (hidden) { hiddenAt = Time.unscaledTime; hiddenNote.RemoveFromClassList("rp-hidden-note-fade"); }
            badge.text = photo ? "PHOTO MODE" : "REPLAY";
            hintsBuilt = false;
            if (photo && !hidden) panel.schedule.Execute(() => { if (nav.Focused == null) nav.Focus(shoot); }).ExecuteLater(10);
            RefreshRows();
        }

        static void Show(VisualElement e, bool on) { if (e != null) e.style.display = on ? DisplayStyle.Flex : DisplayStyle.None; }

        // ------------------------------------------------------------------------------------------------ top

        void BuildTop()
        {
            top = OuUi.Div("rp-top", layer);
            top.pickingMode = PickingMode.Ignore;
            var left = OuUi.Div("rp-top-left", top);
            var b = OuUi.Div("rp-badge", left);
            OuUi.Div("rp-badge-dot", b);
            badge = OuUi.Text("REPLAY", "rp-badge-text", b);
            sub = OuUi.Text("", "rp-sub", left);
            var rider = OuUi.Div("rp-rider", top);
            var text = OuUi.Div("rp-rider-text", rider);
            riderName = OuUi.Text("", "rp-rider-name", text);
            riderInfo = OuUi.Text("", "rp-rider-info", text);
            OuUi.Div("rp-rider-bar", rider);
        }

        // ------------------------------------------------------------------------------------------ transport

        void BuildTransport()
        {
            transport = OuUi.Div("rp-transport", layer);
            var timeline = OuUi.Div("rp-timeline", transport);
            var track = OuUi.Div("rp-track", timeline);
            fill = OuUi.Div("rp-track-fill", track);
            OuUi.Div("rp-live", timeline);
            keyMarks = OuUi.Div("rp-keymarks", timeline);
            keyMarks.pickingMode = PickingMode.Ignore;
            headDot = OuUi.Div("rp-head", timeline);
            Scrubber(timeline);

            // KEYFRAMES: camera keys along the timeline, flown by the KEYFRAMES camera
            keyRow = OuUi.Div("rp-keyrow", transport);
            OuUi.Text("KEYFRAMES", "rp-keyrow-title", keyRow);
            TextButton(keyRow, "ADD KEY", () => r.AddKey(), false).AddToClassList("rp-key-button");
            TextButton(keyRow, "DELETE KEY", () => r.DeleteKey(), false).AddToClassList("rp-key-button");
            TextButton(keyRow, "CLEAR", () => r.ClearKeys(), false).AddToClassList("rp-key-button");
            aimValue = Chip(keyRow, "AIM", 90, _ => r.KeyAimRider = !r.KeyAimRider);
            keyNote = OuUi.Text("", "rp-keynote", keyRow);
            keyRow.PlaceInFront(timeline);

            var row = OuUi.Div("rp-controls", transport);
            IconButton(row, ReflexReplayIcon.Kind.Start, () => r.Seek(r.Buffer.StartTime));
            IconButton(row, ReflexReplayIcon.Kind.Back, () => r.Seek(r.PlayTime - 5f));
            var play = IconButton(row, ReflexReplayIcon.Kind.Play, () => r.TogglePlay());
            play.AddToClassList("rp-play");
            playIcon = play.Q<ReflexReplayIcon>();
            IconButton(row, ReflexReplayIcon.Kind.Forward, () => r.Seek(r.PlayTime + 5f));
            IconButton(row, ReflexReplayIcon.Kind.End, () => r.Seek(r.Buffer.EndTime));
            time = OuUi.Text("", "rp-time", row);
            OuUi.Div("rp-spacer", row);
            speedValue = Chip(row, "SPEED", 76, d => r.ChangeSpeed(d));
            camValue = Chip(row, "CAMERA", 300, d => r.CycleCamera(d));
            riderValue = Chip(row, "RIDER", 250, d => r.CycleTarget(d));
            TextButton(row, "PHOTO MODE", () => r.PhotoFromReplay(), true);
            TextButton(row, "EXIT", () => r.Close(), false);
        }

        /// The keys' diamonds on the timeline (a click jumps to the key; the one at the moment shown is lit), the AIM value and the
        /// last key action's note (3 s).
        void KeyFrame(ReflexReplayBuffer b, float span)
        {
            var keys = r.Keys;
            while (keyDiamonds.Count < keys.Count)
            {
                var d = OuUi.Div("rp-keymark", keyMarks);
                d.pickingMode = PickingMode.Position;
                int idx = keyDiamonds.Count;
                d.RegisterCallback<PointerDownEvent>(e =>
                {
                    if (e.button != 0 || idx >= r.Keys.Count) return;
                    r.Pause(); r.Seek(r.Keys[idx].t); OuSound.Play("Accept"); e.StopPropagation();
                });
                keyDiamonds.Add(d);
            }
            int here = r.KeyAt(r.PlayTime, 0.25f);
            for (int i = 0; i < keyDiamonds.Count; i++)
            {
                bool on = i < keys.Count;
                keyDiamonds[i].style.display = on ? DisplayStyle.Flex : DisplayStyle.None;
                if (!on) continue;
                keyDiamonds[i].style.left = Length.Percent(Mathf.Clamp01((keys[i].t - b.StartTime) / span) * 100f);
                keyDiamonds[i].EnableInClassList("rp-keymark-here", i == here);
            }
            aimValue.text = r.KeyAimRider ? "RIDER" : "VIEW";
            if (!string.IsNullOrEmpty(r.KeyNote)) { keyNote.text = r.KeyNote; r.KeyNote = null; keyNoteAt = Time.unscaledTime; }
            if (Time.unscaledTime - keyNoteAt > 3f) keyNote.text = keys.Count == 0 ? "FLY TO A SHOT, THEN ADD KEY" : "";
        }

        /// Click / drag on the timeline seeks.
        void Scrubber(VisualElement area)
        {
            area.pickingMode = PickingMode.Position;
            void SeekAt(Vector2 local)
            {
                float w = area.resolvedStyle.width;
                if (w <= 1f) return;
                var b = r.Buffer;
                r.Seek(Mathf.Lerp(b.StartTime, b.EndTime, Mathf.Clamp01(local.x / w)));
            }
            area.RegisterCallback<PointerDownEvent>(e =>
            {
                if (e.button != 0) return;
                dragging = true;
                wasPlaying = r.Playing;
                r.Pause();
                area.CapturePointer(e.pointerId);
                SeekAt(e.localPosition);
                e.StopPropagation();
            });
            area.RegisterCallback<PointerMoveEvent>(e => { if (dragging && area.HasPointerCapture(e.pointerId)) SeekAt(e.localPosition); });
            area.RegisterCallback<PointerUpEvent>(e =>
            {
                if (!dragging) return;
                dragging = false;
                area.ReleasePointer(e.pointerId);
                if (wasPlaying && r.Face == ReflexReplay.View.Replay) r.TogglePlay();
            });
        }

        VisualElement IconButton(VisualElement parent, ReflexReplayIcon.Kind kind, Action click)
        {
            var b = OuUi.Div("rp-icon-button", parent);
            b.pickingMode = PickingMode.Position;
            var icon = new ReflexReplayIcon { kind = kind };
            icon.AddToClassList("rp-icon");
            icon.pickingMode = PickingMode.Ignore;
            b.Add(icon);
            b.RegisterCallback<ClickEvent>(e => { if (e.button == 0) { OuSound.Play("Accept"); click(); } });
            return b;
        }

        Label Chip(VisualElement parent, string label, float valueWidth, Action<int> change)
        {
            var c = OuUi.Div("rp-chip", parent);
            OuUi.Text(label, "rp-chip-label", c);
            var prev = OuUi.Text("‹", "rp-chip-arrow", c);
            var val = OuUi.Text("", "rp-chip-value", c);
            val.style.minWidth = valueWidth;
            var next = OuUi.Text("›", "rp-chip-arrow", c);
            prev.pickingMode = next.pickingMode = PickingMode.Position;
            prev.RegisterCallback<ClickEvent>(e => { OuSound.Play("LeftRight"); change(-1); e.StopPropagation(); });
            next.RegisterCallback<ClickEvent>(e => { OuSound.Play("LeftRight"); change(1); e.StopPropagation(); });
            c.pickingMode = PickingMode.Position;
            c.RegisterCallback<ClickEvent>(e => { if (e.button == 0) { OuSound.Play("LeftRight"); change(1); } });
            return val;
        }

        VisualElement TextButton(VisualElement parent, string text, Action click, bool accent)
        {
            var b = OuUi.Div("rp-text-button", parent);
            if (accent) b.AddToClassList("rp-text-accent");
            OuUi.Text(text, "mxa-button-label", b);
            b.pickingMode = PickingMode.Position;
            b.RegisterCallback<ClickEvent>(e => { if (e.button == 0) { OuSound.Play("Accept"); click(); } });
            return b;
        }

        // ------------------------------------------------------------------------------------- PHOTO MODE panel

        void BuildPanel()
        {
            panel = OuUi.Div("rp-panel", layer);
            panel.RegisterCallback<PointerEnterEvent>(_ => PointerOverPanel = true);
            panel.RegisterCallback<PointerLeaveEvent>(_ => PointerOverPanel = false);
            OuUi.Text("PHOTO MODE", "rp-title", panel);
            OuUi.Text("THE MOMENT IS FROZEN · MOVE THE CAMERA, SET THE SHOT", "rp-sub rp-panel-sub", panel);
            var sv = new ScrollView(ScrollViewMode.Vertical) { verticalScrollerVisibility = ScrollerVisibility.Auto, horizontalScrollerVisibility = ScrollerVisibility.Hidden };
            sv.AddToClassList("rp-scroll");
            panel.Add(sv);
            var c = sv.contentContainer;
            var p = ReflexReplay.Photo;

            Section(c, "CAMERA");
            Choice(c, "CAMERA", () => p.orbit ? "ORBIT" : "FREE", _ => r.SetPhotoOrbit(!p.orbit));
            Number(c, "FIELD OF VIEW", () => r.UserFov, v => r.UserFov = v, 5f, 110f, 1f, v => $"{v:0}°", false);
            Number(c, "ROLL", () => p.roll, v => p.roll = v, -45f, 45f, 1f, v => $"{v:+0;-0;0}°", true);
            Choice(c, "SUBJECT", () => r.TargetName(r.Target), d => r.CycleTarget(d));

            Section(c, "LENS");
            Choice(c, "FOCUS", () => ReflexReplay.FocusNames[p.focus], d => p.focus = (p.focus + d + 3) % 3);
            Number(c, "FOCUS DISTANCE", () => p.focusDistance, v => p.focusDistance = v, 1f, 600f, 0f, v => ReflexSettings.Metric ? $"{v * 0.3048f:0.#} M" : $"{v:0.#} FT", false,
                   () => p.focus == 2, true);
            Choice(c, "APERTURE", () => "f/" + ReflexReplay.Apertures[p.aperture].ToString("0.#"), d => p.aperture = Mathf.Clamp(p.aperture + d, 0, ReflexReplay.Apertures.Length - 1),
                   () => p.focus != 0);
            Number(c, "MOTION BLUR", () => p.motionBlur, v => p.motionBlur = v, 0f, 1f, 0.05f, MotionBlurLabel, false);

            postNote = OuUi.Text("FILTER, EXPOSURE, WHITE BALANCE AND GLOW NEED URP POST-PROCESSING ON THE GAME CAMERA (NOT AVAILABLE HERE).", "rp-note", c);
            Section(c, "COLOUR");
            Choice(c, "FILTER", () => ReflexReplay.FilterNames[p.filter], d => p.filter = (p.filter + d + ReflexReplay.FilterNames.Length) % ReflexReplay.FilterNames.Length, () => r.PostAvailable);
            Number(c, "EXPOSURE", () => p.exposure, v => p.exposure = v, -3f, 3f, 0.1f, v => $"{v:+0.0;-0.0;0.0} EV", true, () => r.PostAvailable);
            Number(c, "CONTRAST", () => p.contrast, v => p.contrast = v, -60f, 60f, 5f, v => $"{v:+0;-0;0}", true, () => r.PostAvailable);
            Number(c, "SATURATION", () => p.saturation, v => p.saturation = v, -100f, 100f, 5f, v => $"{v:+0;-0;0}", true, () => r.PostAvailable);
            Number(c, "TEMPERATURE", () => p.temperature, v => p.temperature = v, -60f, 60f, 5f, v => $"{v:+0;-0;0}", true, () => r.PostAvailable);
            Number(c, "TINT", () => p.tint, v => p.tint = v, -50f, 50f, 5f, v => $"{v:+0;-0;0}", true, () => r.PostAvailable);

            Section(c, "EFFECTS");
            Choice(c, "LENS EFFECTS", () => p.lens ? "ON" : "OFF", _ => p.lens = !p.lens);
            Number(c, "VIGNETTE", () => p.vignette, v => p.vignette = v, 0f, 1f, 0.05f, Pct, false);
            Number(c, "FILM GRAIN", () => p.grain, v => p.grain = v, 0f, 1f, 0.05f, Pct, false);
            Number(c, "CHROMATIC ABERRATION", () => p.chromatic, v => p.chromatic = v, 0f, 1f, 0.05f, Pct, false);
            Number(c, "GLOW", () => p.glow, v => p.glow = v, 0f, 1f, 0.05f, Pct, false, () => r.PostAvailable);

            Section(c, "FRAME");
            Choice(c, "ASPECT", () => ReflexReplay.AspectNames[p.aspect], d => p.aspect = (p.aspect + d + ReflexReplay.AspectNames.Length) % ReflexReplay.AspectNames.Length);
            Choice(c, "GRID", () => ReflexReplay.GridNames[p.grid], d => p.grid = (p.grid + d + ReflexReplay.GridNames.Length) % ReflexReplay.GridNames.Length);
            Choice(c, "LOGO", () => ReflexReplay.LogoNames[p.logo], d => p.logo = (p.logo + d + ReflexReplay.LogoNames.Length) % ReflexReplay.LogoNames.Length).name = "photo-logo";

            Section(c, "SCENE");
            Choice(c, "YOUR RIDER", () => p.showRider ? "SHOWN" : "HIDDEN", _ => p.showRider = !p.showRider);
            Choice(c, "YOUR MACHINE", () => p.showBike ? "SHOWN" : "HIDDEN", _ => p.showBike = !p.showBike);
            Choice(c, "OTHER RIDERS", () => p.showOthers ? "SHOWN" : "HIDDEN", _ => p.showOthers = !p.showOthers);

            Section(c, "OUTPUT");
            Choice(c, "RESOLUTION", () => ResolutionLabel(p.resolution), d => p.resolution = (p.resolution + d + ReflexReplay.ResolutionNames.Length) % ReflexReplay.ResolutionNames.Length);
            var buttons = OuUi.Div("rp-panel-buttons", c);
            OuUi.Button("RESET", "rp-text-button", () => { p.ResetLook(); r.UserFov = 55f; RefreshRows(); }, buttons);
            OuUi.Button("OPEN FOLDER", "rp-text-button", ReflexReplay.RevealPhotos, buttons);

            shoot = OuUi.Button("TAKE PHOTO", "rp-shoot", () => r.TakePhoto(), panel);
            shoot.name = "photo-take";
            var back = OuUi.Div("rp-panel-buttons", panel);
            OuUi.Button("REPLAY", "rp-text-button", () => r.SetFace(ReflexReplay.View.Replay), back);
            OuUi.Button("DONE", "rp-text-button", () => r.PhotoBack(), back);
            nav.DefaultItem = () => shoot;
        }

        /// RESOLUTION's value with the picture's size on this screen ("4K · 3840 X 2160").
        string ResolutionLabel(int i)
        {
            var cam = r.ViewCamera;
            if (!cam) return ReflexReplay.ResolutionNames[i];
            var s = ReflexReplay.ShotSize(cam.pixelWidth, cam.pixelHeight, i);
            float a = ReflexReplay.FrameAspect(ReflexReplay.Photo.aspect);
            int w = s.x, h = s.y;
            if (a > 0f) { if (w / (float)h > a) w = Mathf.RoundToInt(h * a); else h = Mathf.RoundToInt(w / a); }
            return $"{ReflexReplay.ResolutionNames[i]} · {w} X {h}";
        }

        static string Pct(float v) => Mathf.RoundToInt(v * 100f) + "%";
        /// MOTION BLUR as the shutter time it is (the photo's shutter is open up to 1/20 s).
        static string MotionBlurLabel(float v) => v < 0.01f ? "OFF" : "1/" + Mathf.RoundToInt(20f / v) + " S";

        static void Section(VisualElement parent, string text) => OuUi.Text(text, "rp-section", parent);

        VisualElement Choice(VisualElement parent, string label, Func<string> value, Action<int> change, Func<bool> enabled = null)
        {
            var row = OuUi.Spinner(label, value, d => { change(d); RefreshRows(); }, parent);
            var val = row.Q<Label>(className: "mxa-value");
            rows.Add((row, () =>
            {
                if (val != null) val.text = value();
                row.EnableInClassList("rp-row-off", enabled != null && !enabled());
            }));
            return row;
        }

        /// A number row: label, a bar (bipolar: filled from the centre), the value; left / right step it (multiplicative when step is
        /// 0), the mouse sets it on the bar.
        void Number(VisualElement parent, string label, Func<float> get, Action<float> set, float min, float max, float step, Func<float, string> fmt,
                    bool bipolar, Func<bool> enabled = null, bool logBar = false)
        {
            var row = OuUi.Div("mxa-row", parent);
            OuUi.Text(label, "mxa-row-label", row);
            var right = OuUi.Div("mxa-row-value", row);
            var prev = OuUi.Text("‹", "mxa-arrow", right);
            var bar = OuUi.Div("rp-bar", right);
            var barFill = OuUi.Div("rp-bar-fill", bar);
            if (bipolar) OuUi.Div("rp-bar-zero", bar);
            var val = OuUi.Text("", "rp-num-value", right);
            var next = OuUi.Text("›", "mxa-arrow", right);
            float ToBar(float v) => logBar ? Mathf.InverseLerp(Mathf.Log(min), Mathf.Log(max), Mathf.Log(Mathf.Max(v, min))) : Mathf.InverseLerp(min, max, v);
            float FromBar(float f) => logBar ? Mathf.Exp(Mathf.Lerp(Mathf.Log(min), Mathf.Log(max), f)) : Mathf.Lerp(min, max, f);
            void Refresh()
            {
                float v = get();
                float f = ToBar(v);
                if (bipolar)
                {
                    float z = ToBar(0f);
                    barFill.style.left = Length.Percent(Mathf.Min(f, z) * 100f);
                    barFill.style.width = Length.Percent(Mathf.Abs(f - z) * 100f);
                }
                else
                {
                    barFill.style.left = 0;
                    barFill.style.width = Length.Percent(f * 100f);
                }
                val.text = fmt(v);
                row.EnableInClassList("rp-row-off", enabled != null && !enabled());
            }
            void Adjust(int d)
            {
                float v = get();
                v = step > 0f ? Mathf.Round((v + d * step) / step) * step : v * Mathf.Pow(1.08f, d);
                set(Mathf.Clamp(v, min, max));
                Refresh();
            }
            prev.pickingMode = next.pickingMode = PickingMode.Position;
            prev.RegisterCallback<ClickEvent>(e => { OuSound.Play("LeftRight"); Adjust(-1); e.StopPropagation(); });
            next.RegisterCallback<ClickEvent>(e => { OuSound.Play("LeftRight"); Adjust(1); e.StopPropagation(); });
            bar.pickingMode = PickingMode.Position;
            void SetAt(float x)
            {
                float w = bar.resolvedStyle.width;
                if (w <= 1f) return;
                float v = FromBar(Mathf.Clamp01(x / w));
                if (step > 0f) v = Mathf.Round(v / step) * step;
                set(Mathf.Clamp(v, min, max));
                Refresh();
            }
            bar.RegisterCallback<PointerDownEvent>(e => { if (e.button != 0) return; bar.CapturePointer(e.pointerId); SetAt(e.localPosition.x); e.StopPropagation(); });
            bar.RegisterCallback<PointerMoveEvent>(e => { if (bar.HasPointerCapture(e.pointerId)) SetAt(e.localPosition.x); });
            bar.RegisterCallback<PointerUpEvent>(e => { if (bar.HasPointerCapture(e.pointerId)) bar.ReleasePointer(e.pointerId); });
            OuUi.Item(row, null, Adjust);
            row.RegisterCallback<AttachToPanelEvent>(_ => Refresh());
            rows.Add((row, Refresh));
        }

        void RefreshRows()
        {
            foreach (var (_, refresh) in rows) refresh();
            if (postNote != null) Show(postNote, r != null && !r.PostAvailable);
        }

        // ------------------------------------------------------------------------------------------ moment strip

        void BuildMoment()
        {
            moment = OuUi.Div("rp-moment", layer);
            var row = OuUi.Div("rp-moment-row", moment);
            OuUi.Text("MOMENT", "rp-section rp-section-flush", row);
            momentTime = OuUi.Text("", "rp-time rp-time-flush", row);
            var timeline = OuUi.Div("rp-timeline rp-timeline-flush", moment);
            var track = OuUi.Div("rp-track", timeline);
            momentFill = OuUi.Div("rp-track-fill", track);
            OuUi.Div("rp-live", timeline);
            momentHead = OuUi.Div("rp-head", timeline);
            Scrubber(timeline);
        }

        // --------------------------------------------------------------------------------------------- per frame

        public void Frame()
        {
            if (r == null || root.style.display == DisplayStyle.None) return;
            bool photo = r.Face == ReflexReplay.View.Photo;
            if (input != null && photo && !r.UiHidden) NavStep();
            var b = r.Buffer;
            float span = Mathf.Max(0.001f, b.Duration);
            float frac = Mathf.Clamp01((r.PlayTime - b.StartTime) / span);
            string now = Clock(r.PlayTime - b.StartTime);
            if (!photo)
            {
                fill.style.width = Length.Percent(frac * 100f);
                headDot.style.left = Length.Percent(frac * 100f);
                time.text = $"{now}  /  {Clock(span)}";
                playIcon.SetKind(r.Playing ? ReflexReplayIcon.Kind.Pause : ReflexReplayIcon.Kind.Play);
                speedValue.text = r.SpeedLabel;
                camValue.text = r.CameraLabel;
                riderValue.text = r.TargetName(r.Target);
                KeyFrame(b, span);
            }
            else
            {
                momentFill.style.width = Length.Percent(frac * 100f);
                momentHead.style.left = Length.Percent(frac * 100f);
                float back = b.EndTime - r.PlayTime;
                momentTime.text = back < 0.005f ? $"{now}  ·  LIVE" : $"{now}  ·  {back:0.00} S BEFORE";
            }
            string track = r.Runtime ? (r.Runtime.level ?? "") : "";
            sub.text = track.Replace('_', ' ').ToUpperInvariant() + (photo ? "" : "   ·   " + r.CameraLabel);
            riderName.text = r.TargetName(r.Target);
            riderInfo.text = r.TargetInfo();
            Hints(photo);
            Frame(photo ? ReflexReplay.FrameAspect(ReflexReplay.Photo.aspect) : 0f, photo ? ReflexReplay.Photo.logo : 0);
            Grid(photo && !r.UiHidden ? ReflexReplay.Photo.grid : 0);
            // the flash and the toast
            float fa = Mathf.Clamp01(1f - (Time.unscaledTime - flashAt) / 0.35f);
            flash.style.opacity = fa * 0.85f;
            toast.EnableInClassList("rp-toast-on", Time.unscaledTime - toastAt < 3.2f);
            if (r.UiHidden && Time.unscaledTime - hiddenAt > 2.5f) hiddenNote.AddToClassList("rp-hidden-note-fade");
            if (photo && Time.frameCount % 15 == 0) RefreshRows();   // (the FOV changes with the wheel / triggers)
        }

        /// The panel's navigation: the D-pad / arrow keys only (the left stick and WASD fly the camera, Space takes the photo), A / Enter.
        void NavStep()
        {
            var kb = Keyboard.current; var pad = Gamepad.current;
            Vector2 v = Vector2.zero;
            if (kb != null) v += new Vector2((kb.rightArrowKey.isPressed ? 1 : 0) - (kb.leftArrowKey.isPressed ? 1 : 0), (kb.upArrowKey.isPressed ? 1 : 0) - (kb.downArrowKey.isPressed ? 1 : 0));
            if (pad != null) v += pad.dpad.ReadValue();
            input.injectStep = stepper.Step(v, Time.unscaledTime);
            input.injectSubmit = kb != null && (kb.enterKey.wasPressedThisFrame || kb.numpadEnterKey.wasPressedThisFrame) || pad != null && pad.buttonSouth.wasPressedThisFrame;
            nav.Update(input);
        }

        public static string Clock(float s)
        {
            s = Mathf.Max(0f, s);
            int m = (int)(s / 60f);
            return $"{m}:{s - m * 60f:00.00}";
        }

        void Hints(bool photo)
        {
            bool pad = OuGlyphs.Current != OuGlyphs.Device.Keyboard;
            if (hintsBuilt && pad == hintsPad && hintsFace == r.Face) return;
            hintsBuilt = true; hintsPad = pad; hintsFace = r.Face;
            bool ps = OuGlyphs.Current == OuGlyphs.Device.PlayStation;
            string P(string playstation, string xbox) => ps ? playstation : xbox;
            if (!photo)
            {
                if (pad) OuUi.Hints(hints, (P("Cross", "A"), "PLAY / PAUSE"), (P("L2 / R2", "LT / RT"), "SHUTTLE"), ("D-pad", "SKIP · SPEED"), (P("Triangle", "Y"), "CAMERA"),
                                    (P("L1 / R1", "LB / RB"), "RIDER"), (P("R3", "RS"), "KEY"), (P("Create", "View"), "PHOTO"), (P("Square", "X"), "HIDE UI"), (P("Circle", "B"), "EXIT"));
                else OuUi.Hints(hints, ("Space", "PLAY / PAUSE"), ("← →", "SKIP"), ("↑ ↓", "SPEED"), (", .", "FRAME"), ("C", "CAMERA"), ("[ ]", "RIDER"),
                                ("K", "KEY"), ("P", "PHOTO"), ("H", "HIDE UI"), ("Esc", "EXIT"));
            }
            else
            {
                if (pad) OuUi.Hints(hints, ("L Stick", "MOVE"), ("R Stick", "LOOK"), (P("L1 / R1", "LB / RB"), "DOWN / UP"), (P("L2 / R2", "LT / RT"), "ZOOM"),
                                    (P("Triangle", "Y"), "TAKE PHOTO"), (P("Square", "X"), "HIDE UI"), (P("Create", "View"), "REPLAY"), (P("Circle", "B"), "BACK"));
                else OuUi.Hints(hints, ("WASD", "MOVE"), ("Q / E", "DOWN / UP"), ("Right mouse", "LOOK"), ("Wheel", "ZOOM"), ("Shift", "FAST"),
                                ("Space", "TAKE PHOTO"), (", .", "FRAME"), ("H", "HIDE UI"), ("Esc", "BACK"));
            }
        }

        /// The aspect frame (inside the game camera's own part of the screen): the view outside it darkened, the grid and the logo's
        /// place inside it.
        void Frame(float aspect, int logoCorner)
        {
            float W = layer.resolvedStyle.width, H = layer.resolvedStyle.height;
            if (float.IsNaN(W) || W < 2f || H < 2f) return;
            var cam = r.ViewCamera;
            Rect vr = cam ? cam.rect : new Rect(0, 0, 1, 1);
            // the camera's viewport in panel units (its rect is from the bottom left, the panel's from the top left)
            float vx = vr.x * W, vy = (1f - vr.y - vr.height) * H, vw = vr.width * W, vh = vr.height * H;
            float cw = vw, ch = vh;
            if (aspect > 0f) { if (vw / vh > aspect) cw = vh * aspect; else ch = vw / aspect; }
            var f = new Rect(vx + (vw - cw) * 0.5f, vy + (vh - ch) * 0.5f, cw, ch);
            if (f == frameShown && Mathf.Approximately(aspect, maskAspect) && logoCorner == logoShown) return;
            frameShown = f; maskAspect = aspect; logoShown = logoCorner;
            Place(mask[0], 0, 0, W, f.y);                           // top
            Place(mask[1], 0, f.yMax, W, H - f.yMax);               // bottom
            Place(mask[2], 0, f.y, f.x, f.height);                  // left
            Place(mask[3], f.xMax, f.y, W - f.xMax, f.height);      // right
            Place(grid, f.x, f.y, f.width, f.height);
            gridShown = -1;
            var tex = ReflexReplay.LogoTexture;
            if (logoCorner > 0 && tex)
            {
                var lr = ReflexReplay.LogoRect(f.width, f.height, logoCorner, tex.width / (float)tex.height);
                Place(logo, f.x + lr.x, f.y + lr.y, lr.width, lr.height);
            }
            else logo.style.display = DisplayStyle.None;
        }

        static void Place(VisualElement e, float x, float y, float w, float h)
        {
            e.style.left = x; e.style.top = y; e.style.width = Mathf.Max(0f, w); e.style.height = Mathf.Max(0f, h);
            e.style.display = w > 0.5f && h > 0.5f ? DisplayStyle.Flex : DisplayStyle.None;
        }

        void Grid(int kind)
        {
            if (kind == gridShown) return;
            gridShown = kind;
            grid.Clear();
            void V(float f) { var l = OuUi.Div("rp-grid-line", grid); l.style.left = Length.Percent(f * 100f); l.style.top = 0; l.style.bottom = 0; l.style.width = 1; }
            void Hz(float f) { var l = OuUi.Div("rp-grid-line", grid); l.style.top = Length.Percent(f * 100f); l.style.left = 0; l.style.right = 0; l.style.height = 1; }
            switch (kind)
            {
                case 1: V(1f / 3f); V(2f / 3f); Hz(1f / 3f); Hz(2f / 3f); break;
                case 2: V(0.5f); Hz(0.5f); break;
                case 3: { const float g = 0.382f; V(g); V(1f - g); Hz(g); Hz(1f - g); break; }
            }
        }

        // ---------------------------------------------------------------------------------------------- capture

        public void CaptureBegin() { }

        public void CaptureEnd(string path, string error)
        {
            toastAt = Time.unscaledTime;
            if (path == null)
            {
                toastTitle.text = "PHOTO NOT SAVED";
                toastPath.text = error ?? "";
                thumb.style.display = DisplayStyle.None;
                return;
            }
            flashAt = Time.unscaledTime;
            toastTitle.text = "PHOTO SAVED";
            toastPath.text = path;
            thumb.style.display = DisplayStyle.Flex;
            if (r.LastPhoto)
            {
                thumb.style.backgroundImage = new StyleBackground(r.LastPhoto);
                float a = r.LastPhoto.width / (float)Mathf.Max(1, r.LastPhoto.height);
                thumb.style.width = Mathf.Clamp(99f * a, 56f, 200f);
            }
        }
    }

    /// The transport's vector icons (the fonts have no media glyphs).
    public sealed class ReflexReplayIcon : VisualElement
    {
        public enum Kind { Play, Pause, Back, Forward, Start, End }
        public Kind kind;

        public ReflexReplayIcon() { generateVisualContent += Draw; }

        public void SetKind(Kind k) { if (k == kind) return; kind = k; MarkDirtyRepaint(); }

        void Draw(MeshGenerationContext mgc)
        {
            var rc = contentRect;
            float w = rc.width, h = rc.height;
            if (w < 1f || h < 1f) return;
            var p = mgc.painter2D;
            p.fillColor = Color.white;
            void Tri(float x0, float x1, bool right)
            {
                p.BeginPath();
                if (right) { p.MoveTo(new Vector2(x0, 0f)); p.LineTo(new Vector2(x1, h * 0.5f)); p.LineTo(new Vector2(x0, h)); }
                else { p.MoveTo(new Vector2(x1, 0f)); p.LineTo(new Vector2(x0, h * 0.5f)); p.LineTo(new Vector2(x1, h)); }
                p.ClosePath();
                p.Fill();
            }
            void Bar(float x0, float x1)
            {
                p.BeginPath();
                p.MoveTo(new Vector2(x0, 0f)); p.LineTo(new Vector2(x1, 0f)); p.LineTo(new Vector2(x1, h)); p.LineTo(new Vector2(x0, h));
                p.ClosePath();
                p.Fill();
            }
            switch (kind)
            {
                case Kind.Play: Tri(w * 0.18f, w * 0.92f, true); break;
                case Kind.Pause: Bar(w * 0.16f, w * 0.40f); Bar(w * 0.60f, w * 0.84f); break;
                case Kind.Back: Tri(0f, w * 0.5f, false); Tri(w * 0.5f, w, false); break;
                case Kind.Forward: Tri(0f, w * 0.5f, true); Tri(w * 0.5f, w, true); break;
                case Kind.Start: Bar(0f, w * 0.16f); Tri(w * 0.16f, w * 0.92f, false); break;
                case Kind.End: Tri(w * 0.08f, w * 0.84f, true); Bar(w * 0.84f, w); break;
            }
        }
    }
}
