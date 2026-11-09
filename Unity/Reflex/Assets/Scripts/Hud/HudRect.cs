// Part of the MODERN HUD kit (ReflexHudKit.cs).
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;

namespace Reflex.Port.Hud
{
    /// A panel: rounded corners, a vertical tint (bottom -> top) and a horizontal fade over fadeLeft / fadeRight units.
    [ExecuteAlways]
    public sealed class HudRect : MaskableGraphic
    {
        public Color top = Color.white, bottom = Color.white;
        public float radius = 4f, fadeLeft, fadeRight;
        /// A panel's background: its alpha follows HUD OPACITY (ReflexHudKit.PanelOpacity). Accent bars and fills set this false.
        public bool panel = true;
        static Texture2D s_round;
        public override Texture mainTexture => radius > 0.01f ? Round : s_WhiteTexture;
        static Texture2D Round
        {
            get
            {
                if (s_round) return s_round;
                const int N = 64;
                s_round = new Texture2D(N, N, TextureFormat.RGBA32, false) { name = "HudRound", wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Bilinear, hideFlags = HideFlags.DontSave };
                var px = new Color32[N * N];
                for (int y = 0; y < N; y++)
                    for (int x = 0; x < N; x++)
                    {
                        float dx = x + 0.5f - N * 0.5f, dy = y + 0.5f - N * 0.5f;
                        px[y * N + x] = new Color32(255, 255, 255, (byte)(Mathf.Clamp01(N * 0.5f - Mathf.Sqrt(dx * dx + dy * dy) + 0.5f) * 255f));
                    }
                s_round.SetPixels32(px); s_round.Apply(false, true);
                return s_round;
            }
        }
        public void SetColors(Color t, Color b) { if (top == t && bottom == b) return; top = t; bottom = b; SetVerticesDirty(); }
        static readonly List<float> s_xs = new List<float>(32), s_ys = new List<float>(8), s_ux = new List<float>(32), s_uy = new List<float>(8);
        protected override void OnPopulateMesh(VertexHelper vh)
        {
            vh.Clear();
            Rect r = GetPixelAdjustedRect();
            float w = r.width, h = r.height;
            if (w <= 0 || h <= 0) return;
            float rad = Mathf.Min(radius, Mathf.Min(w, h) * 0.5f);
            float fl = Mathf.Min(fadeLeft, w), fr = Mathf.Min(fadeRight, w - fl);
            s_xs.Clear(); s_ys.Clear(); s_ux.Clear(); s_uy.Clear();
            s_xs.Add(0); s_xs.Add(w);
            if (rad > 0) { s_xs.Add(rad); s_xs.Add(w - rad); }
            const int Steps = 8;
            for (int i = 1; i < Steps; i++) { if (fl > 0) s_xs.Add(fl * i / Steps); if (fr > 0) s_xs.Add(w - fr * i / Steps); }
            if (fl > 0) s_xs.Add(fl);
            if (fr > 0) s_xs.Add(w - fr);
            s_xs.Sort();
            s_ys.Add(0); s_ys.Add(h);
            if (rad > 0) { s_ys.Add(rad); s_ys.Add(h - rad); }
            s_ys.Sort();
            float last = float.NaN; foreach (float x in s_xs) if (!(Mathf.Abs(x - last) < 1e-3f)) { s_ux.Add(x); last = x; }
            last = float.NaN; foreach (float y in s_ys) if (!(Mathf.Abs(y - last) < 1e-3f)) { s_uy.Add(y); last = y; }
            int cols = s_ux.Count;
            var vert = UIVertex.simpleVert;
            Color mul = color;
            if (panel) mul.a *= ReflexHudKit.PanelOpacity;
            for (int j = 0; j < s_uy.Count; j++)
            {
                float y = s_uy[j];
                Color c = Color.Lerp(bottom, top, y / h) * mul;
                float v = rad > 0 ? (y < rad ? 0.5f * y / rad : y > h - rad ? 1f - 0.5f * (h - y) / rad : 0.5f) : 0.5f;
                for (int i = 0; i < cols; i++)
                {
                    float x = s_ux[i], a = 1f;
                    if (fl > 0 && x < fl) a = Smooth(x / fl);
                    if (fr > 0 && x > w - fr) a = Mathf.Min(a, Smooth((w - x) / fr));
                    float u = rad > 0 ? (x < rad ? 0.5f * x / rad : x > w - rad ? 1f - 0.5f * (w - x) / rad : 0.5f) : 0.5f;
                    if (fl > 0 && x < rad) u = 0.5f;
                    if (fr > 0 && x > w - rad) u = 0.5f;
                    vert.position = new Vector3(r.xMin + x, r.yMin + y);
                    var cc = c; cc.a *= a; vert.color = cc;
                    vert.uv0 = new Vector4(u, v, 0, 0);
                    vh.AddVert(vert);
                }
            }
            for (int j = 0; j + 1 < s_uy.Count; j++)
                for (int i = 0; i + 1 < cols; i++)
                {
                    int a = j * cols + i;
                    vh.AddTriangle(a, a + cols, a + cols + 1);
                    vh.AddTriangle(a, a + cols + 1, a + 1);
                }
        }
        static float Smooth(float t) { t = Mathf.Clamp01(t); return t * t * (3f - 2f * t); }
    }
}
