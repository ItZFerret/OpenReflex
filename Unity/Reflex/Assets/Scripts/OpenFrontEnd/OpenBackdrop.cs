// Location pictures as full-screen backdrops (the hub's fallback, the loading screen, credits): the install's best location art is
// small (the locations' PreviewIconId pictures are 512 x 256, a few 1024 x 512; nothing larger exists for a location: the career
// photos and the posters are the same pictures or smaller), so stretching one over a 1080p / 1440p screen shows its 4x4 DXT
// blocks. OpenBackdrop makes a picture that holds up:
//   Treat(src)     softens the picture at its own resolution (a Gaussian of a pixel or so: the blocks of the DXT compression go, the shapes stay), resamples it with
//                  a bicubic (Catmull-Rom) filter to about the screen's size class, then grades it (a little less colour, more contrast),
//                  darkens the edges (vignette) and the menu side (left), and adds a fine grain that keeps the soft picture from reading
//                  as smeared. The result has mipmaps and trilinear sampling; it is made once per picture and cached by OpenContent.
//   Apply(el, tex) sets it as the element's picture and, unless REDUCE MOTION is on, starts a very slow Ken Burns drift (scale and pan
//                  of the element, a minute a cycle). Apply(el, null) clears both.
// A picture at least 0.6 x the screen wide is not treated (it is shown sharp, with mipmaps and trilinear sampling).
using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public static class OpenBackdrop
    {
        /// A picture narrower than this share of the screen's width gets the soft treatment.
        public const float SoftBelow = 0.6f;
        /// The treatment's Gaussian, in pixels of the source picture (a DXT block is 4).
        public const float BlurSigma = 0.85f;

        /// Whether a picture of this width needs the treatment on a screen this wide.
        public static bool NeedsTreatment(int pictureWidth, int screenWidth) => pictureWidth < SoftBelow * Mathf.Max(1, screenWidth);

        /// The treated copy of a readable picture (see the class comment); outWidth 0: three times the source, at most 2048.
        public static Texture2D Treat(Texture2D src, string name, int outWidth = 0)
        {
            if (!src || !src.isReadable) return src;
            int w = src.width, h = src.height;
            int ow = outWidth > 0 ? outWidth : Mathf.Min(2048, w * 3), oh = Mathf.Max(1, Mathf.RoundToInt(ow * (float)h / w));
            var px = src.GetPixels32();
            var rgb = new float[w * h * 3];
            for (int i = 0; i < w * h; i++) { rgb[i * 3] = px[i].r; rgb[i * 3 + 1] = px[i].g; rgb[i * 3 + 2] = px[i].b; }
            Gauss(rgb, w, h, BlurSigma);
            // bicubic up: columns, then rows
            var wide = new float[ow * h * 3];
            for (int x = 0; x < ow; x++)
            {
                float sx = (x + 0.5f) * w / ow - 0.5f; int ix = Mathf.FloorToInt(sx); float fx = sx - ix;
                Cubic(fx, out float c0, out float c1, out float c2, out float c3);
                for (int y = 0; y < h; y++)
                    for (int c = 0; c < 3; c++)
                        wide[(y * ow + x) * 3 + c] = rgb[(y * w + Mathf.Clamp(ix - 1, 0, w - 1)) * 3 + c] * c0 + rgb[(y * w + Mathf.Clamp(ix, 0, w - 1)) * 3 + c] * c1
                                                    + rgb[(y * w + Mathf.Clamp(ix + 1, 0, w - 1)) * 3 + c] * c2 + rgb[(y * w + Mathf.Clamp(ix + 2, 0, w - 1)) * 3 + c] * c3;
            }
            var o = new Color32[ow * oh];
            for (int y = 0; y < oh; y++)
            {
                float sy = (y + 0.5f) * h / oh - 0.5f; int iy = Mathf.FloorToInt(sy); float fy = sy - iy;
                Cubic(fy, out float c0, out float c1, out float c2, out float c3);
                int y0 = Mathf.Clamp(iy - 1, 0, h - 1), y1 = Mathf.Clamp(iy, 0, h - 1), y2 = Mathf.Clamp(iy + 1, 0, h - 1), y3 = Mathf.Clamp(iy + 2, 0, h - 1);
                float v = y / (float)(oh - 1);   // 0 at the bottom of the picture (Unity's row 0)
                for (int x = 0; x < ow; x++)
                {
                    float r = 0, g = 0, b = 0;
                    for (int t = 0; t < 4; t++)
                    {
                        int yy = t == 0 ? y0 : t == 1 ? y1 : t == 2 ? y2 : y3; float k = t == 0 ? c0 : t == 1 ? c1 : t == 2 ? c2 : c3;
                        int i = (yy * ow + x) * 3; r += wide[i] * k; g += wide[i + 1] * k; b += wide[i + 2] * k;
                    }
                    float u = x / (float)(ow - 1);
                    // grade: less colour, a little more contrast
                    float lum = r * 0.299f + g * 0.587f + b * 0.114f;
                    r = Mathf.Lerp(lum, r, 0.9f); g = Mathf.Lerp(lum, g, 0.9f); b = Mathf.Lerp(lum, b, 0.9f);
                    r = (r - 118f) * 1.04f + 118f; g = (g - 118f) * 1.04f + 118f; b = (b - 118f) * 1.04f + 118f;
                    // vignette (an ellipse: the corners go to ~0.5) and the menu side (the left third dims to ~0.78)
                    float dx = (u - 0.5f) * 2f, dy = (v - 0.5f) * 2f;
                    float vig = 1f - 0.28f * Mathf.SmoothStep(0.4f, 1.35f, Mathf.Sqrt(dx * dx * 0.9f + dy * dy));
                    float left = Mathf.Lerp(0.88f, 1f, Mathf.SmoothStep(0f, 0.5f, u));
                    float m = vig * left;
                    // grain: +-3 levels of a hash of the pixel, the same on all channels
                    float n = (Hash(x, y) - 0.5f) * 6f;
                    o[y * ow + x] = new Color32(B(r * m + n), B(g * m + n), B(b * m + n), 255);
                }
            }
            var t2 = new Texture2D(ow, oh, TextureFormat.RGBA32, true) { name = name, wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Trilinear, anisoLevel = 1, hideFlags = HideFlags.DontSave };
            t2.SetPixels32(o); t2.Apply(true, true);
            return t2;
        }

        static byte B(float v) => (byte)Mathf.Clamp(Mathf.RoundToInt(v), 0, 255);
        static float Hash(int x, int y) { uint h = (uint)(x * 374761393 + y * 668265263); h = (h ^ (h >> 13)) * 1274126177u; return ((h ^ (h >> 16)) & 0xffff) / 65535f; }

        /// Catmull-Rom weights of the four taps around a sample at fraction f.
        static void Cubic(float f, out float a, out float b, out float c, out float d)
        {
            float f2 = f * f, f3 = f2 * f;
            a = -0.5f * f3 + f2 - 0.5f * f; b = 1.5f * f3 - 2.5f * f2 + 1f; c = -1.5f * f3 + 2f * f2 + 0.5f * f; d = 0.5f * f3 - 0.5f * f2;
        }

        /// A separable Gaussian over RGB floats (edges clamped).
        static void Gauss(float[] rgb, int w, int h, float sigma)
        {
            int r = Mathf.CeilToInt(sigma * 3f); var k = new float[r + 1]; float sum = 0;
            for (int i = 0; i <= r; i++) { k[i] = Mathf.Exp(-i * i / (2f * sigma * sigma)); sum += i == 0 ? k[i] : 2 * k[i]; }
            for (int i = 0; i <= r; i++) k[i] /= sum;
            var tmp = new float[rgb.Length];
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                    for (int c = 0; c < 3; c++)
                    {
                        float s = rgb[(y * w + x) * 3 + c] * k[0];
                        for (int i = 1; i <= r; i++) s += k[i] * (rgb[(y * w + Mathf.Max(0, x - i)) * 3 + c] + rgb[(y * w + Mathf.Min(w - 1, x + i)) * 3 + c]);
                        tmp[(y * w + x) * 3 + c] = s;
                    }
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                    for (int c = 0; c < 3; c++)
                    {
                        float s = tmp[(y * w + x) * 3 + c] * k[0];
                        for (int i = 1; i <= r; i++) s += k[i] * (tmp[(Mathf.Max(0, y - i) * w + x) * 3 + c] + tmp[(Mathf.Min(h - 1, y + i) * w + x) * 3 + c]);
                        rgb[(y * w + x) * 3 + c] = s;
                    }
        }

        // ------------------------------------------------------------------------------------------------ drift

        /// Seconds of one Ken Burns cycle (in and out).
        public const float DriftSeconds = 70f;
        static readonly Dictionary<VisualElement, IVisualElementScheduledItem> s_drift = new Dictionary<VisualElement, IVisualElementScheduledItem>();

        /// Sets the element's picture (null: none) and its slow drift (REDUCE MOTION: the picture holds still).
        public static void Apply(VisualElement el, Texture2D tex, bool drift = true)
        {
            if (el == null) return;
            el.style.backgroundImage = tex ? new StyleBackground(tex) : new StyleBackground(StyleKeyword.None);
            if (s_drift.TryGetValue(el, out var old)) { old.Pause(); s_drift.Remove(el); }
            Rest(el);
            if (!tex || !drift) return;
            float t0 = Time.realtimeSinceStartup;
            var item = el.schedule.Execute(() =>
            {
                if (OuMotion.Reduced) { Rest(el); return; }
                float p = (Time.realtimeSinceStartup - t0) / DriftSeconds * Mathf.PI * 2f;
                float e = 0.5f - 0.5f * Mathf.Cos(p);                 // 0 -> 1 -> 0, eased at both ends
                float s = 1.015f + 0.055f * e;                         // zoom in 1.5 % .. 7 %
                float maxPan = (s - 1f) * 0.5f * 0.7f * 100f;          // percent of the element, inside what the zoom covers
                el.style.scale = new Scale(new Vector2(s, s));
                el.style.translate = new Translate(Length.Percent(-maxPan * e), Length.Percent(maxPan * 0.6f * (e - 0.5f) * 2f * 0.5f));
            }).Every(33);
            s_drift[el] = item;
            el.RegisterCallback(s_onDetach);   // (the same callback once per element)
        }

        // An element taken off its panel (its document went with its scene) leaves the table: an entry held the element, its whole UI tree
        // and, through the controls' callbacks, the destroyed menu shell with its sound bank until the same element was set again - never,
        // for a menu or a loading page built anew each time (w-gcspikes: one OpenShell kept per visit of the menus). (An element put back
        // on a panel keeps its picture, without the drift until it is set again.)
        static void Forget(VisualElement el) { if (s_drift.TryGetValue(el, out var it)) { it.Pause(); s_drift.Remove(el); } }
        static readonly EventCallback<DetachFromPanelEvent> s_onDetach = e => { if (e.currentTarget is VisualElement el) Forget(el); };

        static void Rest(VisualElement el) { el.style.scale = new StyleScale(StyleKeyword.Null); el.style.translate = new StyleTranslate(StyleKeyword.Null); }
    }
}
