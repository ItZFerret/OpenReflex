// Part of the MODERN HUD kit (ReflexHudKit.cs).
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;

namespace Reflex.Port.Hud
{
    /// Antialiased vector shapes in one mesh (Draw adds them while the mesh is built); local units, origin at the centre.
    [ExecuteAlways]
    public sealed class HudVector : MaskableGraphic
    {
        public System.Action<HudVector> Draw;
        readonly List<UIVertex> _v = new List<UIVertex>(1024);
        readonly List<int> _i = new List<int>(2048);
        float _f = 1f;
        protected override void OnPopulateMesh(VertexHelper vh)
        {
            vh.Clear(); _v.Clear(); _i.Clear();
            var c = canvas; _f = c && c.scaleFactor > 0.01f ? 1f / c.scaleFactor : 1f;
            Draw?.Invoke(this);
            if (_v.Count > 0) vh.AddUIVertexStream(_v, _i);
        }
        int V(Vector2 p, Color c) { var v = UIVertex.simpleVert; v.position = p; v.color = c; _v.Add(v); return _v.Count - 1; }
        static Color A(Color c, float a) { c.a *= a; return c; }
        void Q(int a, int b, int c, int d) { _i.Add(a); _i.Add(b); _i.Add(c); _i.Add(a); _i.Add(c); _i.Add(d); }

        public void Disc(Vector2 c, float r, Color col, int segs = 0)
        {
            if (segs <= 0) segs = Mathf.Clamp((int)(r * 1.2f), 10, 64);
            float f = _f * 0.5f; int ci = V(c, col), first = _v.Count;
            for (int k = 0; k < segs; k++)
            {
                float a = k * Mathf.PI * 2f / segs; var d = new Vector2(Mathf.Sin(a), Mathf.Cos(a));
                V(c + d * Mathf.Max(0f, r - f), col); V(c + d * (r + f), A(col, 0f));
            }
            for (int k = 0; k < segs; k++)
            {
                int i0 = first + k * 2, i1 = first + ((k + 1) % segs) * 2;
                _i.Add(ci); _i.Add(i0); _i.Add(i1); Q(i0, i0 + 1, i1 + 1, i1);
            }
        }

        /// A ring band r0..r1 from a0 to a1 degrees (0 = up, clockwise).
        public void Arc(Vector2 c, float r0, float r1, float a0, float a1, Color col, int segs = 0)
        {
            if (a1 < a0) (a0, a1) = (a1, a0);
            float span = a1 - a0;
            if (span <= 0.01f || r1 <= r0) return;
            if (segs <= 0) segs = Mathf.Clamp((int)(span / 3f * Mathf.Max(1f, r1 / 80f)), 2, 160);
            float f = _f * 0.5f;
            float rs0 = r0 - f, rs1 = r0 + f, rs2 = r1 - f, rs3 = r1 + f; float al1 = 1f, al2 = 1f;
            if (r1 - r0 < _f) { rs1 = rs2 = (r0 + r1) * 0.5f; al1 = al2 = (r1 - r0) / _f; }
            int first = _v.Count;
            for (int k = 0; k <= segs; k++)
            {
                float a = (a0 + span * k / segs) * Mathf.Deg2Rad; var d = new Vector2(Mathf.Sin(a), Mathf.Cos(a));
                V(c + d * Mathf.Max(0f, rs0), A(col, 0f)); V(c + d * Mathf.Max(0f, rs1), A(col, al1)); V(c + d * Mathf.Max(0f, rs2), A(col, al2)); V(c + d * rs3, A(col, 0f));
            }
            for (int k = 0; k < segs; k++) { int b = first + k * 4, n = b + 4; for (int q = 0; q < 3; q++) Q(b + q, n + q, n + q + 1, b + q + 1); }
        }

        /// A filled triangle with feathered edges (arrows, markers).
        public void Tri(Vector2 a, Vector2 b, Vector2 c, Color col)
        {
            int i0 = V(a, col), i1 = V(b, col), i2 = V(c, col);
            _i.Add(i0); _i.Add(i1); _i.Add(i2);
            float w = Mathf.Max(_f, 1f);
            Segment(a, b, w, col); Segment(b, c, w, col); Segment(c, a, w, col);
        }

        public void Tick(Vector2 c, float a, float r0, float r1, float w, Color col)
        {
            float ar = a * Mathf.Deg2Rad; var d = new Vector2(Mathf.Sin(ar), Mathf.Cos(ar));
            Segment(c + d * r0, c + d * r1, w, col);
        }

        public void Segment(Vector2 a, Vector2 b, float w, Color col)
        {
            var d = b - a; float len = d.magnitude; if (len < 1e-4f) return; d /= len;
            var n = new Vector2(-d.y, d.x); float f = _f * 0.5f, h = w * 0.5f;
            float o0 = -h - f, o1 = -h + f, o2 = h - f, o3 = h + f, al = 1f;
            if (w < _f) { o1 = o2 = 0f; al = w / _f; }
            int s = _v.Count;
            V(a + n * o0, A(col, 0)); V(a + n * o1, A(col, al)); V(a + n * o2, A(col, al)); V(a + n * o3, A(col, 0));
            V(b + n * o0, A(col, 0)); V(b + n * o1, A(col, al)); V(b + n * o2, A(col, al)); V(b + n * o3, A(col, 0));
            for (int q = 0; q < 3; q++) Q(s + q, s + 4 + q, s + 5 + q, s + 1 + q);
        }
    }
}
