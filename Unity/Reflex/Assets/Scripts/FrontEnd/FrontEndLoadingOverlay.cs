// The "InGame Loading Page" of TransitionPageGraph.pag (Background01.lyr + LoadingScreen.lyr: LOADING and the spinning
// IconLoadingArrow, UISpinnerComponent RotationSpeed 180 deg/s on its TargetComponent) shown while the race scene loads.
// It survives the scene change and leaves when the race scene's original world is running (OriginalWorldRuntime.Running) or
// has stopped. On the first run of an event the world is built from the install (~40 s): the race scene's build status line
// is shown under LOADING (an addition: the original loads in a few seconds and shows nothing else).
using System.Collections.Generic;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.UI;

namespace Reflex.Port.FrontEnd
{
    public sealed class FrontEndLoadingOverlay : MonoBehaviour
    {
        FeAssets assets; readonly List<FeLayer> layers = new List<FeLayer>(); Canvas canvas; FeText status;
        readonly List<(FeNode spinner, FeNode target, float speed)> spinners = new List<(FeNode, FeNode, float)>();
        float age, fade = -1; OriginalWorldRuntime runtime;
        public static FrontEndLoadingOverlay Current { get; private set; }

        public static FrontEndLoadingOverlay Show(FeAssets assets, List<string> layerNames)
        {
            var go = new GameObject("Reflex loading page");
            if (Application.isPlaying) DontDestroyOnLoad(go);
            var o = go.AddComponent<FrontEndLoadingOverlay>(); o.assets = assets;
            var c = new GameObject("canvas", typeof(Canvas), typeof(CanvasScaler)); c.transform.SetParent(go.transform, false);
            o.canvas = c.GetComponent<Canvas>(); o.canvas.renderMode = RenderMode.ScreenSpaceOverlay; o.canvas.sortingOrder = 1000;
            o.canvas.additionalShaderChannels |= AdditionalCanvasShaderChannels.TexCoord1;
            var sc = c.GetComponent<CanvasScaler>(); sc.uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize; sc.referenceResolution = new Vector2(1280, 720); sc.screenMatchMode = CanvasScaler.ScreenMatchMode.Expand;
            var bg = new GameObject("Black", typeof(RectTransform), typeof(Image)); bg.transform.SetParent(c.transform, false);
            var brt = (RectTransform)bg.transform; brt.anchorMin = Vector2.zero; brt.anchorMax = Vector2.one; brt.offsetMin = brt.offsetMax = Vector2.zero; bg.GetComponent<Image>().color = Color.black;
            var r = new GameObject("1280x720", typeof(RectTransform)); r.transform.SetParent(c.transform, false);
            var root = (RectTransform)r.transform; root.anchorMin = root.anchorMax = root.pivot = new Vector2(0.5f, 0.5f); root.sizeDelta = new Vector2(1280, 720);
            foreach (string n in layerNames)
            {
                var l = new FeLayer(assets, n, root); l.active = true; l.focused = true; l.holder.gameObject.SetActive(true);
                if (n.StartsWith("Background")) l.Play("Loop1");
                o.layers.Add(l); o.FindSpinners(l.root);
            }
            // the race scene's build status, under LOADING
            var s = new GameObject("status", typeof(RectTransform)); s.transform.SetParent(root, false);
            var srt = (RectTransform)s.transform; srt.anchorMin = srt.anchorMax = new Vector2(0, 1); srt.pivot = new Vector2(0.5f, 0); srt.anchoredPosition = new Vector2(640, -660); srt.sizeDelta = new Vector2(1100, 30);
            o.status = s.AddComponent<FeText>(); o.status.font = assets.Font("ParalucentCondLight28"); o.status.textHeight = 24; o.status.resize = false; o.status.align = new Vector2(0.5f, 0.5f);
            o.status.material = assets.baseMaterial; o.status.color = new Color(14 / 255f, 15 / 255f, 15 / 255f, 1); o.status.raycastTarget = false;
            Current = o;
            return o;
        }

        void FindSpinners(FeNode n)
        {
            if (n.type == "UISpinnerComponent" && n.P("TargetComponent") is string t) spinners.Add((n, n.Path(t), OriginalUi.F(n.props, "RotationSpeed", 180)));
            foreach (var k in n.kids) FindSpinners(k);
        }

        void Update() => Step(Time.unscaledDeltaTime);
        public void Step(float dt)
        {
            age += dt;
            foreach (var (sp, target, speed) in spinners) if (target != null) target.rotZ = (target.rotZ + speed * dt) % 360f;
            foreach (var l in layers) { l.Tick(dt); l.UpdateMasks(canvas.transform); }
            if (!runtime) runtime = FindAnyObjectByType<OriginalWorldRuntime>();
            string line = "";
            if (runtime && runtime.statusLabel) { line = runtime.statusLabel.text ?? ""; int nl = line.IndexOf('\n'); if (nl >= 0) line = line.Substring(0, nl); }
            bool building = line.StartsWith("BUILDING");
            status.Text = building ? line : "";
            bool done = runtime && (runtime.Running || line.StartsWith("STOPPED")) || !runtime && age > 15f;
            if (done && fade < 0) fade = 0;
            if (fade >= 0)
            {
                fade += dt / 0.3f;
                var g = canvas.GetComponent<CanvasGroup>(); if (!g) g = canvas.gameObject.AddComponent<CanvasGroup>();
                g.alpha = 1 - fade;
                if (fade >= 1) { if (Current == this) Current = null; Destroy(gameObject); }
            }
        }
    }
}
