// TEST ONLY (tests/menus_showcase, run.py --batch): screenshots of a -batchmode player, which has no window to capture and never takes the
// focus. The frame is composed offscreen at REFLEX_SHOT_SIZE (WxH): the enabled cameras (depth order) into one target, every UI Toolkit panel
// into a target of its own (its PanelSettings' targetTexture, set once at start: the panels lay out at the shot's size), the overlay canvases
// (the MODERN HUD kit's uGUI: NOW PLAYING, toasts) through a camera into another, then blended on the CPU in that order. OuLayout and the HUD's
// safe root are laid out for the shot's size (OuLayout.ScreenOverride). REFLEX_SHOT_FLIP_CAM / REFLEX_SHOT_FLIP_UI = 0 / 1 override the row order.
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    public static class UiShowcaseOffscreen
    {
        const string Tag = "[ui-showcase] ";
        public static Vector2Int Size = new Vector2Int(1920, 1080);
        static readonly Dictionary<PanelSettings, RenderTexture> targets = new Dictionary<PanelSettings, RenderTexture>();

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Init()
        {
            if (!Application.isBatchMode || !UiShowcase.Wanted) return;
            var sz = Environment.GetEnvironmentVariable("REFLEX_SHOT_SIZE");
            if (!string.IsNullOrEmpty(sz) && sz.Split('x') is var p && p.Length == 2 && int.TryParse(p[0], out int w) && int.TryParse(p[1], out int h) && w > 0 && h > 0)
                Size = new Vector2Int(w, h);
            OuLayout.ScreenOverride = Size;
            try { Screen.SetResolution(Size.x, Size.y, FullScreenMode.Windowed); } catch (Exception) { }   // (the uGUI canvases' scalers read the screen)
            Debug.Log(Tag + "OFFSCREEN " + Size.x + "x" + Size.y + " (window " + Screen.width + "x" + Screen.height + " at start)");
            var go = new GameObject("UI showcase offscreen TEST ONLY"); UnityEngine.Object.DontDestroyOnLoad(go);
            go.AddComponent<Keeper>();
        }

        /// Every panel renders into a target of the shot's size (new panels as they appear).
        sealed class Keeper : MonoBehaviour
        {
            void Update() => Attach();
        }

        static void Attach()
        {
            bool any = false;
            foreach (var p in Resources.FindObjectsOfTypeAll<PanelSettings>())
            {
                if (!p || targets.ContainsKey(p)) continue;
                if (p.targetTexture != null) { targets[p] = null; continue; }   // the product's own offscreen panels (status boards) keep theirs
                var rt = new RenderTexture(Size.x, Size.y, 24, RenderTextureFormat.ARGB32) { name = "showcase ui " + p.name };
                p.targetTexture = rt; p.clearColor = true; p.colorClearValue = new Color(0, 0, 0, 0);
                targets[p] = rt; any = true;
            }
            if (any) OuLayout.Refresh();
        }

        static int Flip(string env, bool fallback)
        {
            var v = Environment.GetEnvironmentVariable(env);
            return v == "0" ? 0 : v == "1" ? 1 : fallback ? 1 : 0;
        }

        static Texture2D Read(RenderTexture r)
        {
            var a = RenderTexture.active; RenderTexture.active = r;
            var t = new Texture2D(r.width, r.height, TextureFormat.RGBA32, false); t.ReadPixels(new Rect(0, 0, r.width, r.height), 0, 0); t.Apply(false);
            RenderTexture.active = a; return t;
        }

        static void FlipRows(Color32[] px, int W, int H)
        {
            var row = new Color32[W];
            for (int y = 0; y < H / 2; y++)
            {
                Array.Copy(px, y * W, row, 0, W); Array.Copy(px, (H - 1 - y) * W, px, y * W, W); Array.Copy(row, 0, px, (H - 1 - y) * W, W);
            }
        }

        /// premultiplied "over": the UI targets are drawn onto a transparent clear
        static void Over(Color32[] dst, Color32[] src)
        {
            for (int i = 0; i < dst.Length; i++)
            {
                int a = src[i].a; if (a == 0) continue;
                int k = 255 - a;
                dst[i] = new Color32((byte)Mathf.Min(255, src[i].r + dst[i].r * k / 255), (byte)Mathf.Min(255, src[i].g + dst[i].g * k / 255),
                                     (byte)Mathf.Min(255, src[i].b + dst[i].b * k / 255), 255);
            }
        }

        public static IEnumerator Shot(string path)
        {
            Attach();
            for (int i = 0; i < 4; i++) yield return null;   // (a panel attached now lays out and repaints)
            int W = Size.x, H = Size.y;
            RenderTexture camRt = null, canvasRt = null; GameObject uiCamGo = null;
            var restored = new List<(Canvas c, RenderMode m, Camera cam, float d)>();
            var scalers = new List<UnityEngine.UI.CanvasScaler>();
            try
            {
                camRt = new RenderTexture(W, H, 24, RenderTextureFormat.ARGB32);
                var a0 = RenderTexture.active; RenderTexture.active = camRt; GL.Clear(true, true, Color.black); RenderTexture.active = a0;
                var cams = Camera.allCameras.Where(c => c && c.enabled && c.targetTexture == null).OrderBy(c => c.depth).ToList();
                Debug.Log(Tag + "OFFSCREEN cameras: " + string.Join(", ", cams.Select(c => c.name + " (depth " + c.depth + ", clear " + c.clearFlags + ")")));
                foreach (var cam in cams)
                {
                    var prev = cam.targetTexture; var rect = cam.rect;
                    cam.targetTexture = camRt; for (int k = 0; k < 4; k++) cam.Render(); cam.targetTexture = prev; cam.rect = rect;   // (a few times: the temporal AA's history settles, as tests/lighting_showcase)
                }
                var bg = Read(camRt); var px = bg.GetPixels32();
                if (Flip("REFLEX_SHOT_FLIP_CAM", false) == 1) FlipRows(px, W, H);   // (the cameras' target comes back upright: the display gamma ramp no longer turns offscreen renders, ReflexGammaOutput)

                bool layers = Environment.GetEnvironmentVariable("REFLEX_SHOT_LAYERS") == "1";
                void Layer(string what, Color32[] p) { if (!layers) return; var lt = new Texture2D(W, H, TextureFormat.RGBA32, false); lt.SetPixels32(p); lt.Apply(false); File.WriteAllBytes(path.Replace(".png", "." + what + ".png"), lt.EncodeToPNG()); UnityEngine.Object.Destroy(lt); }
                Layer("camera", px);
                foreach (var kv in targets.Where(k => k.Key && k.Value).OrderBy(k => k.Key.sortingOrder))
                {
                    var t = Read(kv.Value); var f = t.GetPixels32();
                    Layer("panel-" + kv.Key.name.Replace(" ", "_") + "-" + kv.Key.sortingOrder, f);
                    if (Flip("REFLEX_SHOT_FLIP_UI", false) == 1) FlipRows(f, W, H);
                    Over(px, f); UnityEngine.Object.Destroy(t);
                }

                // the overlay canvases (uGUI), drawn by a camera of their own into a transparent target
                var canvases = Resources.FindObjectsOfTypeAll<Canvas>()   // (FindObjectsByType skips HideAndDontSave objects: NOW PLAYING's canvas is one)
                    .Where(c => c && c.isActiveAndEnabled && c.isRootCanvas && c.renderMode == RenderMode.ScreenSpaceOverlay).OrderBy(c => c.sortingOrder).ToList();
                if (canvases.Count > 0)
                {
                    canvasRt = new RenderTexture(W, H, 24, RenderTextureFormat.ARGB32);
                    uiCamGo = new GameObject("showcase canvas camera");
                    var uiCam = uiCamGo.AddComponent<Camera>();
                    uiCam.clearFlags = CameraClearFlags.SolidColor; uiCam.backgroundColor = new Color(0, 0, 0, 0); uiCam.cullingMask = ~0;
                    uiCam.orthographic = true; uiCam.nearClipPlane = 0.01f; uiCam.farClipPlane = 1000f; uiCam.targetTexture = canvasRt; uiCam.enabled = false;
                    uiCamGo.transform.position = new Vector3(0, -10000, 0);
                    foreach (var c in canvases)
                    {
                        restored.Add((c, c.renderMode, c.worldCamera, c.planeDistance)); c.renderMode = RenderMode.ScreenSpaceCamera; c.worldCamera = uiCam; c.planeDistance = 10f;
                        // the scaler reads the window (640 x 480 in batch mode): the shot's own scale instead (ScaleWithScreenSize, match = height)
                        var sc = c.GetComponent<UnityEngine.UI.CanvasScaler>();
                        if (sc && sc.enabled && sc.uiScaleMode == UnityEngine.UI.CanvasScaler.ScaleMode.ScaleWithScreenSize && Screen.height != H) { scalers.Add(sc); sc.enabled = false; c.scaleFactor = H / Mathf.Max(1f, sc.referenceResolution.y); }
                    }
                }
                if (canvasRt != null)
                {
                    Canvas.ForceUpdateCanvases();
                    var uiCam = uiCamGo.GetComponent<Camera>();
                    // the pipeline's camera does not keep the target's alpha: the canvases are drawn over black and over white and the
                    // coverage is what differs (difference matting); over black is the premultiplied colour
                    uiCam.backgroundColor = Color.black; uiCam.Render();
                    var tb = Read(canvasRt); var fb = tb.GetPixels32();
                    uiCam.backgroundColor = Color.white; uiCam.Render();
                    var tw = Read(canvasRt); var fw = tw.GetPixels32();
                    var key = fb[0];
                    bool keyed = Mathf.Abs(fw[0].r - fb[0].r) + Mathf.Abs(fw[0].g - fb[0].g) + Mathf.Abs(fw[0].b - fb[0].b) < 6;   // the clear colour did not take (the pipeline's own background): key it out
                    for (int i = 0; i < fb.Length; i++)
                    {
                        if (keyed) { int k = Mathf.Max(Mathf.Abs(fb[i].r - key.r), Mathf.Max(Mathf.Abs(fb[i].g - key.g), Mathf.Abs(fb[i].b - key.b))); fb[i].a = (byte)(k < 6 ? 0 : 255); if (k < 6) fb[i].r = fb[i].g = fb[i].b = 0; continue; }
                        int d = ((fw[i].r - fb[i].r) + (fw[i].g - fb[i].g) + (fw[i].b - fb[i].b)) / 3;
                        fb[i].a = (byte)Mathf.Clamp(255 - d, 0, 255);
                    }
                    if (Flip("REFLEX_SHOT_FLIP_CAM", false) == 1) FlipRows(fb, W, H);   // (a camera's target: the cameras' row order)
                    Layer("canvases", fb);
                    Over(px, fb); UnityEngine.Object.Destroy(tb); UnityEngine.Object.Destroy(tw);
                }
                bg.SetPixels32(px); bg.Apply(false);
                File.WriteAllBytes(path, bg.EncodeToPNG());
                UnityEngine.Object.Destroy(bg);
            }
            catch (Exception e) { Debug.LogError(Tag + "OFFSCREEN shot failed: " + e); }
            finally
            {
                foreach (var r in restored) if (r.c) { r.c.renderMode = r.m; r.c.worldCamera = r.cam; r.c.planeDistance = r.d; }
                foreach (var sc in scalers) if (sc) sc.enabled = true;
                if (uiCamGo) UnityEngine.Object.Destroy(uiCamGo);
                if (camRt) { camRt.Release(); UnityEngine.Object.Destroy(camRt); }
                if (canvasRt) { canvasRt.Release(); UnityEngine.Object.Destroy(canvasRt); }
            }
            yield return null;
        }
    }
}
