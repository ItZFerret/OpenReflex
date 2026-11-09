// Part of the MODERN HUD kit (ReflexHudKit.cs): the small animation helpers of the view. All of them run on real (unscaled) time
// so the HUD keeps animating through a slow-motion replay or a pause, none allocates after it is built, and with REDUCE MOTION
// every one of them jumps to its end state (an eased entrance becomes a plain show / hide, a flash only recolours).
using UnityEngine;
using UnityEngine.UI;

namespace Reflex.Port.Hud
{
    public static class HudEase
    {
        public static float Saturate(float t) => t < 0f ? 0f : t > 1f ? 1f : t;
        public static float OutCubic(float t) { t = 1f - Saturate(t); return 1f - t * t * t; }
        public static float InCubic(float t) { t = Saturate(t); return t * t * t; }
        public static float InOutSine(float t) => 0.5f - 0.5f * Mathf.Cos(Saturate(t) * Mathf.PI);
        /// Overshoots slightly before it settles (the score card's entrance, a punch).
        public static float OutBack(float t) { t = Saturate(t) - 1f; const float c = 1.70158f; return 1f + (c + 1f) * t * t * t + c * t * t; }
        /// 0 -> 1 -> 0 over the unit interval.
        public static float Hump(float t) { t = Saturate(t); return Mathf.Sin(t * Mathf.PI); }
    }

    /// A panel (or any group) that eases in and out: alpha, a short slide from SlideFrom and a hair of scale, driven by Set(on). The
    /// object is deactivated once it has faded out, so a hidden group costs nothing.
    public sealed class HudGroup
    {
        public readonly RectTransform rt; readonly CanvasGroup cg;
        Vector2 basePos; readonly Vector2 slide;
        float v; bool on; readonly float inTime, outTime;
        public float Alpha => cg.alpha;
        /// The smoothed resting y of a stacked column (ReflexHudView.Stack).
        public float curY;
        /// Fully shown (the entrance finished).
        public bool Shown => on && v >= 1f;
        /// On, or still fading out.
        public bool Active => rt.gameObject.activeSelf;

        public HudGroup(RectTransform rt, Vector2 slideFrom, float inTime = 0.30f, float outTime = 0.18f)
        {
            this.rt = rt; slide = slideFrom; this.inTime = inTime; this.outTime = outTime;
            cg = rt.GetComponent<CanvasGroup>(); if (!cg) cg = rt.gameObject.AddComponent<CanvasGroup>();
            cg.interactable = false; cg.blocksRaycasts = false;
            basePos = rt.anchoredPosition;
            cg.alpha = 0f; rt.gameObject.SetActive(false);
        }

        /// The layout moved the group: its resting position.
        public void SetBase(Vector2 pos) { basePos = pos; if (v >= 1f) rt.anchoredPosition = pos; }

        public void Set(bool show, float dt, bool reduceMotion)
        {
            if (show != on)
            {
                on = show;
                if (show && !rt.gameObject.activeSelf) { rt.gameObject.SetActive(true); }
            }
            float target = on ? 1f : 0f;
            if (reduceMotion || v == target) v = target;
            else v = Mathf.MoveTowards(v, target, dt / (on ? inTime : outTime));
            if (v <= 0f && !on)
            {
                if (rt.gameObject.activeSelf) rt.gameObject.SetActive(false);
                return;
            }
            float e = on ? HudEase.OutCubic(v) : HudEase.InCubic(v);
            cg.alpha = e;
            rt.anchoredPosition = basePos + slide * (1f - e);
        }

        /// Jump to the end state now (the gallery, a style switch).
        public void Snap(bool show) { on = show; v = show ? 1f : 0f; rt.gameObject.SetActive(show); cg.alpha = show ? 1f : 0f; rt.anchoredPosition = basePos; }
    }

    /// A text (and optionally a second one, e.g. a clock's fraction) that flashes a colour and punches in size when its value changes,
    /// then settles back to its base colour. The punch scales scaleTarget (default: the text's own rect) around its pivot.
    public sealed class HudFlash
    {
        public readonly Text text; readonly Text also; readonly RectTransform scaleRt;
        public Color baseColor; Color flashColor;
        float t = 1f, punch; readonly float duration;

        public HudFlash(Text text, Text also = null, RectTransform scaleTarget = null, float duration = 0.7f)
        {
            this.text = text; this.also = also; scaleRt = scaleTarget ? scaleTarget : text.rectTransform; baseColor = text.color; flashColor = baseColor; this.duration = duration;
        }

        void Paint(Color c) { text.color = c; if (also) also.color = c; }

        /// Start a flash of c (punch: the size overshoot, 0.12 = 12 %).
        public void Trigger(Color c, float punch = 0.12f) { flashColor = c; this.punch = punch; t = 0f; Paint(c); }

        /// The base colour changed (a state colour, e.g. the current lap's amber hold).
        public void SetBase(Color c) { baseColor = c; if (t >= 1f) Paint(c); }

        public void Tick(float dt, bool reduceMotion)
        {
            if (t >= 1f) return;
            t += dt / duration;
            if (t >= 1f) { t = 1f; Paint(baseColor); scaleRt.localScale = Vector3.one; return; }
            Paint(Color.Lerp(flashColor, baseColor, HudEase.OutCubic(t)));
            float s = reduceMotion ? 1f : 1f + punch * (1f - HudEase.OutCubic(Mathf.Min(1f, t * 2.2f)));
            scaleRt.localScale = new Vector3(s, s, 1f);
        }
    }

    /// A clock: "m:ss" in a main text and ".cc" in a smaller one. The main text changes once per second and the fraction text takes
    /// one of 100 cached strings, so a running timer allocates nothing and re-meshes only three glyphs per frame. SetText shows a plain
    /// string (dashes, a distance) in the main text instead.
    public sealed class ClockText
    {
        public readonly RectTransform rt; public readonly Text main, frac;
        readonly float fracWidth; int shownSecond = int.MinValue, shownCenti = -1; string plain; bool isPlain, dashed; char sign;
        static readonly string[] Centi = BuildCenti();
        static string[] BuildCenti() { var a = new string[100]; for (int i = 0; i < 100; i++) a[i] = "." + (i < 10 ? "0" : "") + i; return a; }
        static readonly string[] Secs = new string[3600];
        const string Dashes = "-:--", DashFrac = ".--";

        public Color Color { get => main.color; set { main.color = value; frac.color = value; } }

        /// A clock whose box is anchored by (anchor, pivot, pos) with the given size; the digits end at the box's right edge.
        public ClockText(Transform parent, string name, Font font, int size, int fracSize, Color color, Vector2 anchor, Vector2 pivot, Vector2 pos, Vector2 box)
        {
            rt = ReflexHudKit.Rect(parent, name, anchor, pivot, pos, box);
            frac = ReflexHudKit.Label(rt, "Frac", font, fracSize, color, TextAnchor.LowerRight, new Vector2(1, 0), new Vector2(1, 0), new Vector2(0, size * 0.06f), new Vector2(fracSize * 2.4f, box.y));
            frac.text = ".88";
            fracWidth = Mathf.Ceil(frac.preferredWidth) + 2f;
            frac.rectTransform.sizeDelta = new Vector2(fracWidth, box.y);
            main = ReflexHudKit.Label(rt, "Main", font, size, color, TextAnchor.LowerRight, new Vector2(1, 0), new Vector2(1, 0), new Vector2(-fracWidth, 0), new Vector2(box.x - fracWidth, box.y));
            frac.text = DashFrac; main.text = Dashes;
        }

        public void SetActive(bool on) { if (rt.gameObject.activeSelf != on) rt.gameObject.SetActive(on); }

        /// Seconds shown as m:ss.cc, with an optional sign ('+', '-' or 0) in front of the minutes; <= 0 / none / NaN shows dashes.
        public void SetTime(float t, char sign = '\0', bool dashesForZero = true)
        {
            if (float.IsNaN(t) || t >= 3.0e38f || (dashesForZero && t <= 0f)) { SetDashes(); return; }
            if (t < 0f) t = 0f;
            int cs = (int)(t * 100f + 0.0001f);
            int sec = cs / 100, c = cs - sec * 100;
            dashed = false;
            if (isPlain) { isPlain = false; frac.gameObject.SetActive(true); main.rectTransform.anchoredPosition = new Vector2(-fracWidth, 0f); shownSecond = int.MinValue; }
            if (sec != shownSecond || sign != this.sign)
            {
                shownSecond = sec; this.sign = sign;
                string s = sec < Secs.Length ? (Secs[sec] ??= Format(sec)) : Format(sec);
                main.text = sign == '\0' ? s : sign + s;   // a signed text (a gap) is built once per second
            }
            if (c != shownCenti) { shownCenti = c; frac.text = Centi[c]; }
        }

        static string Format(int sec) => (sec / 60) + ":" + (sec % 60 < 10 ? "0" : "") + (sec % 60);

        public void SetDashes()
        {
            if (dashed) return;
            if (isPlain) main.rectTransform.anchoredPosition = new Vector2(-fracWidth, 0f);
            isPlain = false; dashed = true; shownSecond = int.MinValue; shownCenti = -1; sign = '\0';
            if (!frac.gameObject.activeSelf) frac.gameObject.SetActive(true);
            main.text = Dashes; frac.text = DashFrac;
        }

        /// A plain string in place of the clock (the fraction text hidden).
        public void SetText(string s)
        {
            if (isPlain && plain == s) return;
            dashed = false; isPlain = true; plain = s; shownSecond = int.MinValue; shownCenti = -1; sign = '\0';
            if (frac.gameObject.activeSelf) frac.gameObject.SetActive(false);
            main.text = s;
            main.rectTransform.anchoredPosition = Vector2.zero;   // the plain text ends at the box edge
        }

        public bool IsPlain => isPlain;
    }
}
