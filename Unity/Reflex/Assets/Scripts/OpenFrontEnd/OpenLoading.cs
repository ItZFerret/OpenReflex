// The OPEN REFLEX loading screen (after Open Unleashed's MxaMainMenu.ShowLoading): the location's picture full screen, the wordmark,
// the event and the location, a bar and a line. It survives the change to the race scene and leaves when that scene's original
// world is running (OriginalWorldRuntime.Running) or has stopped, like the original's FrontEndLoadingOverlay. The first ride of a
// setup makes its world from the install (about 15 to 40 s): the bar then shows the build's progress (ReflexWorldFiles.Progress)
// and the line its step.
using System;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed class OpenLoading : MonoBehaviour
    {
        public static OpenLoading Current { get; private set; }
        VisualElement _root, _fill; Label _sub;
        OriginalWorldRuntime _runtime;
        float _age, _fade = -1, _progress;
        bool _sawBuild;
        /// The world was being built while this was up (tests).
        public bool SawBuild => _sawBuild;
        public string Line => _sub?.text;

        /// A career event (OwnCareerLaunch): under the career's own pages (OwnCareerPanels 75, OwnCareerPageView 80: the rules page and the
        /// gate selection draw over it), over the world, until the world runs or the Gate Selection page shows it with its terrain
        /// (OriginalWorldRuntime.GateViewShown): the race scene's world is never seen before the core's startup has built it.
        public static OpenLoading ShowCareer(ReflexEvents.Event e)
        {
            if (e == null) return null;
            var ev = OpenContent.Find(e.key) ?? new OpenEvent { ev = e, key = e.key, typeName = (e.typeName ?? e.spec.type ?? "").ToUpperInvariant(), locationName = e.location };
            var o = Show(ev, null, 72);
            o._career = true;
            return o;
        }
        bool _career;

        public static OpenLoading Show(OpenEvent ev, OpenSetup s) => Show(ev, s, 100);
        static OpenLoading Show(OpenEvent ev, OpenSetup s, float sortingOrder)
        {
            if (Current) Destroy(Current.gameObject);
            var go = new GameObject("OPEN REFLEX loading");
            if (Application.isPlaying) DontDestroyOnLoad(go);
            var o = go.AddComponent<OpenLoading>();
            Current = o;
            var root = OuUi.CreateDocument(go, sortingOrder);
            OuTheme.Apply(root);
            o._root = root;
            var layer = OuUi.Div("mxa-layer mxa-screen-loading", root);
            var box = OuUi.Div("ou-loading", layer);
            var pic = OuUi.Div("ou-loading-shot", box);
            OpenBackdrop.Apply(pic, OpenContent.Backdrop(ev));
            OuUi.Div("ou-loading-shade", box).style.backgroundImage = OuUi.ScrimBottom;
            OuTheme.Logo(OuUi.Div("ou-loading-logo", box), 64, true);
            var copy = OuUi.Div("ou-loading-copy", box);
            OuUi.Text(ev.typeName + (ev.ev.spec.trackId > 0 ? "  ·  TRACK " + (ev.ev.spec.trackId + 1) : ""), "ou-eyebrow", copy);
            OuUi.Text(ev.locationName, "ou-loading-title", copy);
            var bar = OuUi.Div("ou-loading-bar", copy);
            var fill = OuTheme.Backdrop(bar, "ou-loading-bar-fill");
            o._fill = fill;
            o._sub = OuUi.Text(OpenContent.Str("STR_LOADING", "LOADING").ToUpperInvariant() + "…", "ou-loading-sub", copy);
            o._sub.name = "ou-loading-line";
            return o;
        }

        void OnDestroy() { if (Current == this) Current = null; }

        void Update()
        {
            float dt = Time.unscaledDeltaTime;
            _age += dt;
            if (!_runtime) _runtime = FindAnyObjectByType<OriginalWorldRuntime>();
            string line = "";
            if (_runtime && _runtime.statusLabel) { line = _runtime.statusLabel.text ?? ""; int nl = line.IndexOf('\n'); if (nl >= 0) line = line.Substring(0, nl); }
            bool building = line.StartsWith("BUILDING");
            if (building)
            {
                _sawBuild = true;
                string step = "";
                try { _progress = Mathf.Max(_progress, ReflexWorldFiles.Progress(out step)); } catch (Exception) { }
                _sub.text = $"MAKING THIS EVENT'S WORLD FROM YOUR INSTALL  {_progress * 100:0}%" + (string.IsNullOrEmpty(step) ? "" : "  ·  " + step.ToUpperInvariant()) + "  ·  ONCE PER SETUP";
            }
            else if (line.StartsWith("STOPPED")) _sub.text = line;
            // the bar: the build's progress, else a slow fill while the scene and the cached world load
            float target = building ? _progress : Mathf.Clamp01(_age / 6f) * 0.9f;
            if (_fill != null) _fill.style.width = Length.Percent(Mathf.Clamp01(target) * 100f);
            bool done = _runtime && (_runtime.Running || line.StartsWith("STOPPED") || _career && (_runtime.GateViewShown || _runtime.FailureMessage != null)) || !_runtime && _age > 20f;
            if (done && _fade < 0) _fade = 0;
            if (_fade >= 0)
            {
                _fade += dt / 0.3f;
                _root.style.opacity = 1 - Mathf.Clamp01(_fade);
                if (_fade >= 1) Destroy(gameObject);
            }
        }
    }
}
