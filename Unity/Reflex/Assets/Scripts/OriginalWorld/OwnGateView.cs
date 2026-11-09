using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.RxCore;
using UnityEngine;

namespace Reflex.Port.OriginalWorld
{
    // The career Gate Selection page's 3D view (GateSelection.lua: App.ShowGates( 25, 7 ), App.SetCameraToGate( gate, 25, 7, device, 0.5 ); core/include/rx_gate_api.h).
    // The game aims the level's UI_StartGate_Camera (an MXUIPartsViewCamera in its spring mode, 00656070, placed by the start gate entity's matrix) at
    // the focused gate: look-at point = the gate node's position, eye = that position - forward * 25 + (0, 7, 0), both in the gate model's space,
    // each reached over the call's 0.5 s by the camera's quintic ease-out (006596a0: 1 - (1 - t / d)^5) from where it was (the first call, with 0 s, is a cut).
    // The view shift by the up vector is the zero vector there (0xa95d50) and the zoom range 0 .. 0 (0070f2a0), so what the page shows is exactly that
    // look-at; the stick's small orbit of the springs is not drawn. The core exports the inputs (rx_session_start_gates); the cards (the level's
    // UI_StartCard entities) and the arrow are placed by OriginalGateCards. Nothing here moves a native object: the world holds its state.
    public sealed class OwnGateView
    {
        public const float Distance = 25f, Height = 7f, Seconds = 0.5f;

        readonly Camera camera; readonly Action<Action<IntPtr>> between; readonly Func<bool> current; readonly Func<OriginalGateCards> cardsOf; readonly Action prepareLevel;
        readonly Action<RxTerrainInfo, float[], float[]> terrainReady;
        RxStartGatesView data; bool requested, failed;
        bool active; int focus = -1; List<int> available = new List<int>();
        Vector3 lookStart, lookGoal, eyeStart, eyeGoal, lookNow, eyeNow; float elapsed, duration; bool haveNow;
        OriginalGateCards cards;

        /// terrainReady: the level's terrain as the core has it while the page is up (the race's world, and so its startup snapshot with the
        /// terrain, comes only after the gate is chosen; without it the page showed the scenery floating over the camera's clear colour).
        public OwnGateView(Camera camera, Action<Action<IntPtr>> between, Func<bool> current, Func<OriginalGateCards> cardsOf, Action prepareLevel = null, Action<RxTerrainInfo, float[], float[]> terrainReady = null)
        { this.camera = camera; this.between = between; this.current = current; this.cardsOf = cardsOf; this.prepareLevel = prepareLevel; this.terrainReady = terrainReady; }

        public bool Active => active;
        public int Focus => focus;
        public bool Ready => data != null;
        public bool Failed => failed;
        public RxStartGatesView Data => data;
        public int CardCount => cards?.CardCount ?? 0;

        /// The page is up: the gates that can be chosen (the others are taken). Requests the start gate's frames once.
        public void Begin(IReadOnlyList<int> gates)
        {
            if (!current()) return;
            active = true; available = gates.ToList(); focus = -1; haveNow = false;
            try { prepareLevel?.Invoke(); } catch (Exception e) { Debug.LogWarning("[Gate] level: " + e.Message); }
            if (data == null && !requested && !failed)
            {
                requested = true;
                between(s =>
                {
                    var r = RxStartGates.Create();
                    int rc; try { rc = RxApiNative.rx_session_start_gates(s, ref r); } catch (EntryPointNotFoundException) { rc = -1; }
                    var view = rc == 0 && r.valid != 0 ? new RxStartGatesView(r) : null;
                    // the level's terrain, read on the session's thread with the gates (rx_session_terrain: 2 = no terrain loaded yet)
                    var info = new RxTerrainInfo(); float[] heights = null, deform = null;
                    if (terrainReady != null)
                        try
                        {
                            int trc = RxApiNative.rx_session_terrain(s, ref info);
                            int n = trc == 0 ? (int)info.samples : 0;
                            if (n >= 2 && n <= 8192)
                            {
                                heights = new float[n * n]; deform = new float[n * n];
                                if (RxApiNative.rx_session_terrain_heights(s, heights, 0, 0, n, n) != 0) heights = null;
                                if (RxApiNative.rx_session_terrain_deform(s, deform, 0, 0, n, n) != 0) deform = null;
                            }
                            Debug.Log("[Gate] level terrain: rc " + trc + (heights != null ? ", " + n + " x " + n : ", none"));
                        }
                        catch (EntryPointNotFoundException) { }
                    lock (gate) { arrivedTerrain = heights; arrivedDeform = deform; arrivedInfo = info; }
                    GatesArrived(view, rc);
                });
            }
            else Refresh();
        }

        readonly object gate = new object(); RxStartGatesView arrived; int arrivedRc; bool hasArrived;
        float[] arrivedTerrain, arrivedDeform; RxTerrainInfo arrivedInfo;
        void GatesArrived(RxStartGatesView view, int rc) { Debug.Log("[Gate] start gates: rc " + rc + ", " + (view == null ? "none" : (view.HasCamera ? "camera entity" : "no camera entity") + ", fov " + view.Fov)); lock (gate) { arrived = view; arrivedRc = rc; hasArrived = true; } }

        /// The focus moved to a gate (OnGateSelectionListFocusChanged: SetCameraToGate( gate, 25, 7, device, 0.5 )).
        public void SetFocus(int gateIndex)
        {
            if (!active) return;
            bool first = focus < 0;
            focus = gateIndex;
            Aim(first ? 0f : Seconds);
            Refresh();
        }

        /// The page is gone: the camera is the world's again.
        public void End()
        {
            active = false; focus = -1; haveNow = false;
            cards?.Hide();
        }

        // the look-at point and the eye the call sets, in the space the camera works in (the model's with the entity, else the world's)
        bool Goals(int g, out Vector3 look, out Vector3 eye)
        {
            look = eye = default;
            if (data == null || !data.Present(g)) return false;
            if (data.HasCamera) { look = data.LocalPosition(g); eye = look - data.LocalForward(g) * Distance + new Vector3(0f, Height, 0f); }
            else { look = data.Position(g); eye = look - data.Forward(g) * Distance + new Vector3(0f, Height, 0f); }
            return true;
        }

        void Aim(float seconds)
        {
            if (!Goals(focus, out var look, out var eye)) return;
            if (!haveNow || seconds <= 0f) { lookNow = look; eyeNow = eye; haveNow = true; elapsed = duration = 0f; lookGoal = look; eyeGoal = eye; return; }
            lookStart = lookNow; eyeStart = eyeNow; lookGoal = look; eyeGoal = eye; elapsed = 0f; duration = seconds;
        }

        void Refresh()
        {
            if (data == null) return;
            if (cards == null || !cards.Alive) cards = cardsOf?.Invoke();
            cards?.Show(data, available);
            if (focus >= 0) cards?.Focus(data, focus);
        }

        /// Every frame (the world's camera pass): takes the exported frames when they came and, while the page is up, puts the camera on the gate.
        public bool ApplyCamera()
        {
            bool got; RxStartGatesView v; int rc; float[] th, td; RxTerrainInfo ti;
            lock (gate) { got = hasArrived; v = arrived; rc = arrivedRc; hasArrived = false; th = arrivedTerrain; td = arrivedDeform; ti = arrivedInfo; arrivedTerrain = arrivedDeform = null; }
            if (th != null)
                try { terrainReady?.Invoke(ti, th, td); }
                catch (Exception e) { Debug.LogWarning("[Gate] terrain: " + e.Message); }
            if (got) { data = v; if (v == null) { failed = true; Debug.LogWarning("[Gate] no start gate frames (rc " + rc + ")"); } else if (active) { Aim(0f); Refresh(); } }
            if (!active || data == null || focus < 0 || !camera || !haveNow) return false;
            if (duration > 0f && elapsed < duration)
            {
                elapsed = Mathf.Min(duration, elapsed + Time.unscaledDeltaTime);
                float u = 1f - elapsed / duration, e = 1f - u * u * u * u * u;
                lookNow = Vector3.LerpUnclamped(lookStart, lookGoal, e); eyeNow = Vector3.LerpUnclamped(eyeStart, eyeGoal, e);
            }
            else { lookNow = lookGoal; eyeNow = eyeGoal; }
            Vector3 look = lookNow, eye = eyeNow;
            if (data.HasCamera) { look = data.Place(look); eye = data.Place(eye); }
            var fwd = look - eye;
            if (fwd.sqrMagnitude < 1e-6f) return false;
            camera.transform.SetPositionAndRotation(eye, Quaternion.LookRotation(fwd, Vector3.up));
            float fov = data.Fov > 0.05f && data.Fov < 3f ? data.Fov : 0.9f;
            camera.fieldOfView = Reflex.Port.Options.ReflexClientLive.ChaseFov(fov) * Mathf.Rad2Deg;
            if (data.NearPlane > 0 && data.FarPlane > data.NearPlane) { camera.nearClipPlane = data.NearPlane; camera.farClipPlane = data.FarPlane; }
            return true;
        }
    }
}
