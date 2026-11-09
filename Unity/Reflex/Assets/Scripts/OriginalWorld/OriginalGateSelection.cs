using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text.RegularExpressions;
using Reflex.Port.Content;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.Rendering;
using UnityEngine.SceneManagement;
using UnityEngine.UI;

namespace Reflex.Port.OriginalWorld
{
    // The Gate Selection Page of a MOTOCAREER Heat / Moto race (WP-5; GateSelection.lyr + ButtonLegend.lyr, GateSelection.lua; career.md).
    // In the game the page comes before the race's activity loads: Start shows the gates (App.ShowGates) and runs
    // MXSeriesManager::ProcessGateSelection (the riders ahead pick, the AI with the game's generator, until the player's turn), the focus goes
    // to the first available gate (App.IsSeriesGateAvailable) with the camera on it (App.SetCameraToGate( gate, 25, 7, device, 0.5 )),
    // Left / Right move along the available gates (the GateSelectionList, NavigationType LeftRight, off screen at y 736: what shows is
    // the 3D view, "GATE SELECTION" and the legend's SELECT), accept (OnAccept) AssignPlayerGate + AdvanceGateSelection.
    // A port world is the race itself, so its boot made that selection already (career::gate_selection_boot) with the page's default and
    // logged the gates the player could take ("gate selection K: available <mask> default D ..."). The page runs over that world, held in
    // Pre (pages.cpp "hold": no intro, no countdown) with the camera the game's SetCameraToGate computes in it ("gate_camera"): the default
    // gate keeps this world; another gate is the player's choice for the series' K-th gate selection (ReflexCareer.Race.gates), with which
    // the race's world is booted again (career_gates.bin) - its page then does not show.
    public sealed class OriginalGateSelection
    {
        public readonly int index, defaultGate; public readonly int[] available;
        public int Focus { get; private set; }
        public bool IsOpen => canvas && canvas.enabled;
        public static string LastLog;   // tests

        readonly OriginalWorldRuntime runtime; readonly ReflexCareer.Race race; readonly Action<bool> done;
        Canvas canvas; OriginalUiTree tree; OriginalButtonLegend legend;
        int heldBefore = -1; Matrix4x4? camera; readonly object gate = new object(); bool hooked;

        /// The last gate selection of the race's world boot log, when the player has not chosen for it yet; null otherwise.
        public static OriginalGateSelection For(OriginalWorldRuntime runtime, ReflexCareer.Race race, Action<bool> onDone)
        {
            if (race == null || !runtime) return null;
            string log;
            try { log = ReflexWorldFiles.For(race.eventKey).Log.Replace("-boot.log", "-build.log"); } catch (Exception) { return null; }   // the event's start log (plugin: reflex_world_build)
            if (!File.Exists(log)) return null;
            Match last = null;
            foreach (Match m in Regex.Matches(File.ReadAllText(log), @"gate selection (\d+): available ([0-9a-f]+) default (-?\d+) choice (-?\d+) gate (-?\d+)")) last = m;
            if (last == null) return null;
            int k = int.Parse(last.Groups[1].Value, CultureInfo.InvariantCulture);
            if (race.gates.Count > k) return null;   // chosen already (this world was booted with it)
            uint mask = uint.Parse(last.Groups[2].Value, NumberStyles.HexNumber, CultureInfo.InvariantCulture);
            var av = new List<int>(); for (int i = 0; i < 16; i++) if ((mask & (1u << i)) != 0) av.Add(i);
            if (av.Count == 0) return null;
            LastLog = last.Value;
            return new OriginalGateSelection(runtime, race, k, int.Parse(last.Groups[3].Value, CultureInfo.InvariantCulture), av.ToArray(), onDone);
        }

        OriginalGateSelection(OriginalWorldRuntime runtime, ReflexCareer.Race race, int index, int defaultGate, int[] available, Action<bool> onDone)
        {
            this.runtime = runtime; this.race = race; this.index = index; this.defaultGate = defaultGate; this.available = available; done = onDone;
            Focus = Array.IndexOf(available, defaultGate) >= 0 ? defaultGate : available[0];   // OnGateSelectionDeviceIndexChanged: the first available
        }

        public void Show()
        {
            try
            {
                var cgo = new GameObject("Original Gate Selection Page", typeof(Canvas), typeof(CanvasScaler));
                cgo.transform.SetParent(runtime.transform, false);
                canvas = cgo.GetComponent<Canvas>(); canvas.renderMode = RenderMode.ScreenSpaceOverlay; canvas.sortingOrder = 58;
                var sc = cgo.GetComponent<CanvasScaler>(); sc.uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize; sc.referenceResolution = new Vector2(1280, 720); sc.screenMatchMode = CanvasScaler.ScreenMatchMode.Expand;
                var rgo = new GameObject("1280x720", typeof(RectTransform)); rgo.transform.SetParent(cgo.transform, false);
                var root = (RectTransform)rgo.transform; root.anchorMin = root.anchorMax = root.pivot = new Vector2(0.5f, 0.5f); root.sizeDelta = new Vector2(1280, 720);
                tree = new OriginalUiTree(OriginalUiAssets.Texture, OriginalUiAssets.Font);
                var doc = OriginalUiAssets.Data.BuildLayer("InGame", "GateSelection.lyr");
                foreach (var e in OriginalUi.Arr(doc, "elements")) tree.Build((Dictionary<string, object>)e, root, null);
                legend = new OriginalButtonLegend(root);
                legend.Set("accept", true, "STR_SELECT");   // OnGateSelectionDeviceIndexChanged: App.SetButtonLegend( "accept", TRUE, "STR_SELECT" )
                legend.SetAction("accept", Accept);
                canvas.enabled = true;
            }
            catch (Exception e) { Debug.LogWarning("[Pages] Gate Selection page not built: " + e.Message); }
            // the race waits (the activity's hold) and the camera goes to the focused gate
            var sim = runtime.Simulation;
            sim?.BetweenFrames(w => { string r = OriginalEventIntro.Command(w, "hold 1"); lock (gate) heldBefore = int.TryParse(r, out int v) ? v : 0; });
            RenderPipelineManager.beginCameraRendering += OnCamera; hooked = true;
            view = runtime.GetComponent<OriginalGateSelectionView>() ?? runtime.gameObject.AddComponent<OriginalGateSelectionView>(); view.page = this;
            Paint(); AskCamera();
            Debug.Log($"[Pages] Gate Selection page: selection {index}, gates {string.Join(",", available)}, default {defaultGate}");
        }

        void AskCamera()
        {
            int g = Focus; var sim = runtime ? runtime.Simulation : null;
            sim?.BetweenFrames(w =>
            {
                string r = OriginalEventIntro.Command(w, "gate_camera " + g);
                var f = (r ?? "").Split(' ');
                if (f.Length < 16) return;
                var m = new Matrix4x4();
                for (int i = 0; i < 16; i++) m[i / 4, i % 4] = float.Parse(f[i], CultureInfo.InvariantCulture);
                lock (gate) camera = m;
            });
        }

        OriginalGateSelectionView view;
        /// After every LateUpdate (OriginalGateSelectionView): the HUD page has not started (the hand-built HUD hidden), the camera at the gate.
        internal void Late()
        {
            if (!IsOpen || !runtime) return;
            if (runtime.originalHud && runtime.originalHud.Canvas) runtime.originalHud.Canvas.enabled = false;
            if (runtime.chaseCamera) OnCamera(default, runtime.chaseCamera);
        }

        // the game camera at the gate: row vectors (right, up, forward, position), native axes = Unity axes
        void OnCamera(ScriptableRenderContext ctx, Camera cam)
        {
            if (!runtime || cam != runtime.chaseCamera) return;
            Matrix4x4? m; lock (gate) m = camera;
            if (!m.HasValue) return;
            var v = m.Value; var fwd = new Vector3(v[2, 0], v[2, 1], v[2, 2]); var up = new Vector3(v[1, 0], v[1, 1], v[1, 2]);
            if (fwd.sqrMagnitude < 0.5f) return;
            // the entity is the gate's frame (GateStart node, facing down the track); the page's camera stands the call's distance (25) behind
            // it along that facing and its height (7) above, looking at it (the original's UI_StartGate_Camera follows; its blend is not ported)
            var target = new Vector3(v[3, 0], v[3, 1], v[3, 2]);
            var eye = target - fwd.normalized * 25f + Vector3.up * 7f;
            cam.transform.SetPositionAndRotation(eye, Quaternion.LookRotation(target - eye, Vector3.up));
        }

        void Paint()
        {
            // GateSelectionList (off screen): the focused image FocusedColor, available ones FocusableColor, the others EnabledColor
            for (int i = 0; i < 16; i++)
            {
                var n = tree?.Find("GateSelectionList/Image" + i) ?? tree?.Find("Image" + i); if (n == null) continue;
                bool av = Array.IndexOf(available, i) >= 0;
                OriginalUiTree.SetColor(n, OriginalUiTree.StateColor(n, i == Focus, av));
            }
        }

        public void Tick(float dt)
        {
            if (!IsOpen) return;
            tree.Tick(dt); legend.Tick(dt);
            var kb = Keyboard.current; var gp = Gamepad.current;
            bool left = kb != null && kb.leftArrowKey.wasPressedThisFrame || gp != null && gp.dpad.left.wasPressedThisFrame;
            bool right = kb != null && kb.rightArrowKey.wasPressedThisFrame || gp != null && gp.dpad.right.wasPressedThisFrame;
            bool accept = kb != null && (kb.enterKey.wasPressedThisFrame || kb.numpadEnterKey.wasPressedThisFrame) || gp != null && gp.buttonSouth.wasPressedThisFrame;
            if (left) Move(-1); else if (right) Move(+1);
            if (accept) Accept();
        }

        /// The list's LeftRight navigation over the focusable (available) gates.
        public void Move(int d)
        {
            int at = Array.IndexOf(available, Focus), to = Mathf.Clamp(at + d, 0, available.Length - 1);
            if (to == at) return;   // NoScrollSoundEvent DirectionFail
            Focus = available[to]; Paint(); AskCamera();   // OnGateSelectionListFocusChanged: SetCameraToGate( focused, 25, 7, device, 0.5 )
        }

        /// OnAccept: the focused gate for this gate selection.
        public void Accept()
        {
            if (!IsOpen) return;
            canvas.enabled = false;
            if (hooked) { RenderPipelineManager.beginCameraRendering -= OnCamera; hooked = false; }
            while (race.gates.Count < index) race.gates.Add(-1);
            bool same = Focus == defaultGate;
            race.gates.Add(same ? -1 : Focus);   // the default stays -1: the world (and its name) of the default choice
            Debug.Log($"[Pages] Gate Selection: gate {Focus}" + (same ? " (the world's own)" : " -> the race's world again with that gate"));
            if (same)
            {
                int before; lock (gate) before = heldBefore;
                runtime.Simulation?.BetweenFrames(w => OriginalEventIntro.Command(w, "hold " + Math.Max(0, before)));
                done?.Invoke(false);
                return;
            }
            done?.Invoke(true);
            runtime.StopSimulation();
            ReflexEventSelect.Launch(race.eventKey, 0, false, SceneManager.GetActiveScene().name);
        }

        public void Close() { if (canvas) canvas.enabled = false; if (hooked) { RenderPipelineManager.beginCameraRendering -= OnCamera; hooked = false; } }
    }

    /// Runs the open Gate Selection page's per-frame view after every other LateUpdate (the runtime moves the camera in its own).
    [DefaultExecutionOrder(32000)]
    public sealed class OriginalGateSelectionView : MonoBehaviour
    {
        public OriginalGateSelection page;
        void LateUpdate() { if (page != null) { if (page.IsOpen) page.Late(); else page = null; } }
    }
}
