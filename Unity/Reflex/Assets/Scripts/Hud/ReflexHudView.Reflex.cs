// The MODERN HUD's wreck-recovery prompt (ReflexHudView): the original's "Arrows" component (MXUIReflexComponent, core/include/rx_reflex_hud.h,
// ReflexWreckHud) drawn the way the ORIGINAL HUD's layer draws it (OriginalHud.Reflex.cs), from the install's own art:
//   the arrow     one of four curved, translucent green brackets (ReflexArrowMiddle.png, colour 161 223 10 200, two mirrored tiles each) at the layer's
//                 own places in its 1280 x 720 space: the arch above the rider (up), the bowl below (down), the brackets at the left / right edges;
//                 it slides in the flick's direction and fades in over the layer's "ReflexIn" (0.2 s, repeated: the arrow pulses at 5 Hz)
//   the control   a small chip inside the arrow's bend names the control as the game binds it: the arrow key of the remap table's ReflexUp / Down /
//                 Left / Right, a named key, or the stick the preset routes the wreck input to (R STICK, L STICK, TILT) with its flick direction
//   the outcome   none: the arrow is gone the frame the save (or the fall) clears the wreck, as the original's vf11 hides it (SetVisible, no fade-out, no
//                 success / fail display); only the entrance fades (the 0.2 s "ReflexIn" pulse and the group's own 0.10 s fade-in)
// Nothing sits in the middle of the screen, where the rider is; REDUCE MOTION keeps the arrow still.
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.UI;

namespace Reflex.Port.Hud
{
    public sealed partial class ReflexHudView
    {
        // the layer's space: 1280 x 720, drawn at 1.5 (the canvas' 1920 x 1080) or smaller on a narrower screen
        const float ArrowSpaceW = 1280f, ArrowSpaceH = 720f, ArrowDraw = 1.5f;
        const float ReflexInLength = 0.1667f, ReflexInPeriod = 0.2f;   // the last key of "ReflexIn" and the component's restart (the animation is 0.2 s long)
        static readonly Color ArrowGreen = new Color(161f / 255f, 223f / 255f, 10f / 255f, 200f / 255f);   // the layer's colour of every tile
        // the four arrows (index = RxReflexHudState.Dir - 1: up, right, down, left): their group in the layer (position of the pivot, size, pivot) and the slide of
        // "ReflexIn" in layer units (x right, y down; the arrow starts this far from its place and moves the flick's way); eased = the up / down curve (t squared)
        static readonly Vector2[] ArrowPos = { new Vector2(640, 167), new Vector2(948, 87), new Vector2(640, 551), new Vector2(127, 87) };
        static readonly Vector2[] ArrowSize = { new Vector2(512, 256), new Vector2(256, 512), new Vector2(512, 256), new Vector2(256, 512) };
        static readonly Vector2[] ArrowPivot = { new Vector2(0.5f, 0.5f), Vector2.zero, new Vector2(0.5f, 0.5f), Vector2.zero };
        static readonly Vector2[] ArrowSlide = { new Vector2(0, 48), new Vector2(-64, 0), new Vector2(0, -64), new Vector2(64, 0) };
        // the two 256 x 256 tiles of each group: position in the group, turn (degrees, the layer's y-down sense), mirror x / y
        static readonly float[][] ArrowTiles =
        {
            new[] { 256f, 256f, -90f, 0f, 1f, 0f, 256f, -90f, 0f, 0f },   // up
            new[] { 0f, 256f, 0f, 0f, 1f, 0f, 0f, 0f, 0f, 0f },          // right
            new[] { 256f, 0f, 90f, 0f, 1f, 512f, 0f, 90f, 0f, 0f },      // down
            new[] { 0f, 256f, 0f, 1f, 1f, 0f, 0f, 0f, 1f, 0f },          // left
        };
        // where the control chip sits, inside the arrow's bend, in layer units from the screen's centre (x right, y up)
        static readonly Vector2[] ChipAt = { new Vector2(0, 150), new Vector2(265, 0), new Vector2(0, -150), new Vector2(-265, 0) };

        RectTransform wreckRt, wreckArrows, wreckRow; readonly RectTransform[] wreckArrow = new RectTransform[4]; readonly CanvasGroup[] wreckArrowCg = new CanvasGroup[4]; readonly RawImage[][] wreckTile = new RawImage[4][];
        HudVector wreckArrowVec; HudRect wreckBack, wreckKey; Text wreckKeyText, wreckCapText;
        HudGroup gWreck; int cWreckBeginSerial = -1, cWreckDir = -1; float wreckBeginAt = -10f; ReflexWreckHud.Cap cWreckCap = ReflexWreckHud.Cap.None; string cWreckCapName;
        bool wreckTexTried; Texture2D wreckTex; float wreckDrawScale = ArrowDraw;
        ReflexWreckHud wreckNow;   // what the Draw callbacks read

        void BuildReflex()
        {
            wreckRt = ReflexHudKit.Rect(root, "Reflex", MC, MC, Vector2.zero, new Vector2(ArrowSpaceW * ArrowDraw, ArrowSpaceH * ArrowDraw));
            // the arrows, in the layer's own space
            wreckArrows = ReflexHudKit.Rect(wreckRt, "Arrows", MC, MC, Vector2.zero, new Vector2(ArrowSpaceW, ArrowSpaceH));
            for (int i = 0; i < 4; i++)
            {
                var g = ReflexHudKit.Rect(wreckArrows, "Arrow" + i, TL, new Vector2(ArrowPivot[i].x, 1f - ArrowPivot[i].y), new Vector2(ArrowPos[i].x, -ArrowPos[i].y), ArrowSize[i]);
                wreckArrow[i] = g;
                wreckArrowCg[i] = g.gameObject.AddComponent<CanvasGroup>(); wreckArrowCg[i].interactable = false; wreckArrowCg[i].blocksRaycasts = false;
                wreckTile[i] = new RawImage[2];
                for (int k = 0; k < 2; k++)
                {
                    var t = ArrowTiles[i]; int o = k * 5;
                    var rt = ReflexHudKit.Rect(g, "Tile" + k, TL, new Vector2(0, 1), new Vector2(t[o], -t[o + 1]), new Vector2(256, 256));
                    rt.localRotation = Quaternion.Euler(0, 0, -t[o + 2]);
                    var img = ReflexHudKit.Graphic<RawImage>(rt); img.raycastTarget = false; img.color = ArrowGreen;
                    bool mx = t[o + 3] > 0.5f, my = t[o + 4] > 0.5f;
                    img.uvRect = new Rect(mx ? 1 : 0, my ? 1 : 0, mx ? -1 : 1, my ? -1 : 1);
                    wreckTile[i][k] = img;
                }
                g.gameObject.SetActive(false);
            }
            // the control chip: [key / stick cap] [direction arrow] [R STICK]
            wreckRow = ReflexHudKit.Rect(wreckRt, "Control", MC, MC, Vector2.zero, new Vector2(320, 52));
            wreckBack = NewPanel(wreckRow, "Back", MC, MC, Vector2.zero, new Vector2(120, 56), 0f, 0f, 14f);   // the chip is read on any sky or ground
            wreckKey = ReflexHudKit.Panel(wreckRow, "Key", MC, MC, Vector2.zero, new Vector2(48, 40), 0f, 0f, 8f);
            wreckKey.top = new Color(0.97f, 0.97f, 0.96f, 0.92f); wreckKey.bottom = new Color(0.80f, 0.81f, 0.82f, 0.92f); wreckKey.panel = false;
            wreckKeyText = Lbl(wreckKey.rectTransform, "Text", ReflexHudKit.Bold, TLabel, ReflexHudKit.Ink, TextAnchor.MiddleCenter, MC, MC, Vector2.zero, new Vector2(48, 40));
            wreckArrowVec = ReflexHudKit.Vector(wreckRow, "Dir", MC, Vector2.zero, new Vector2(40, 40));
            wreckArrowVec.Draw = DrawWreckSmallArrow;
            wreckCapText = Shade(Lbl(wreckRow, "Cap", ReflexHudKit.Bold, TLabel, ReflexHudKit.White, TextAnchor.MiddleLeft, MC, ML, Vector2.zero, new Vector2(140, 32)));
            gWreck = NewGroup(wreckRt, Vector2.zero, 0.10f, 0.22f);
        }

        // the art, from the install (the HUD package's ReflexArrowMiddle texture), once
        bool WreckTexture()
        {
            if (wreckTexTried) return wreckTex != null;
            wreckTexTried = true;
            try { var d = ReflexUiData.Shared; wreckTex = OriginalUiAssets.Texture(d.TextureKeyOf("HudNationalSinglePlayer", "ReflexArrowMiddle")); } catch (System.Exception) { wreckTex = null; }
            if (wreckTex) foreach (var tiles in wreckTile) foreach (var img in tiles) img.texture = wreckTex;
            return wreckTex != null;
        }

        static Vector2 DirVec(int dir) => dir == RxReflexHudState.DirUp ? Vector2.up : dir == RxReflexHudState.DirDown ? Vector2.down : dir == RxReflexHudState.DirLeft ? Vector2.left : Vector2.right;

        void DrawWreckSmallArrow(HudVector v)
        {
            var w = wreckNow; if (w == null) return;
            Vector2 d = DirVec(w.direction), n = new Vector2(-d.y, d.x);
            Color c = w.cap == ReflexWreckHud.Cap.ArrowKey ? ReflexHudKit.Ink : ReflexHudKit.White;
            v.Segment(-d * 11f, d * 4f, 6f, c);
            v.Tri(d * 15f, d * 2f + n * 10f, d * 2f - n * 10f, c);
        }

        // one arrow at a moment of "ReflexIn": the slide (layer units, from its place) and the alpha
        static void ReflexInFrame(int i, float t, out Vector2 slide, out float alpha)
        {
            float u = Mathf.Clamp01(t / ReflexInLength);
            bool eased = i == 0 || i == 2;   // up / down: the curve 1 - (1 - u)^2 (0, .36, .64, .84, .96, 1); left / right: linear
            float k = eased ? (1f - u) * (1f - u) : 1f - u;
            slide = new Vector2(ArrowSlide[i].x * k, -ArrowSlide[i].y * k);   // layer y is down, the canvas' up
            alpha = eased ? 1f - k : u;
        }

        void UpdateReflex(ReflexHudModel m, bool full)
        {
            var w = m.wreck;
            bool show = full && w.Visible(now) && WreckTexture();
            if (show)
            {
                wreckNow = w;
                if (w.beginSerial != cWreckBeginSerial) { cWreckBeginSerial = w.beginSerial; wreckBeginAt = now; }
                // as large as the layer on a 16 : 9 screen, smaller where the safe area is narrower
                float width = root.rect.width, height = root.rect.height;
                float s = Mathf.Clamp(Mathf.Min(width / ArrowSpaceW, height / ArrowSpaceH), 0.2f, ArrowDraw);
                if (s != wreckDrawScale || wreckArrows.localScale.x != s) { wreckDrawScale = s; wreckArrows.localScale = new Vector3(s, s, 1f); }
                int lit = Mathf.Clamp(w.direction - 1, 0, 3);
                for (int i = 0; i < 4; i++)
                {
                    bool on = i == lit;
                    ReflexHudKit.Show(wreckArrow[i], on);
                    if (!on) continue;
                    Vector2 slide; float alpha;
                    if (!rm) ReflexInFrame(i, (now - wreckBeginAt) % ReflexInPeriod, out slide, out alpha);
                    else { slide = Vector2.zero; alpha = 1f; }
                    wreckArrow[i].anchoredPosition = new Vector2(ArrowPos[i].x, -ArrowPos[i].y) + slide;
                    wreckArrowCg[i].alpha = alpha;
                    foreach (var img in wreckTile[i]) img.color = ArrowGreen;
                }
                // no control chip: the original's arrows only (the user's call; LayoutWreckRow / ChipAt stay for a chip if one is wanted again)
                ReflexHudKit.Show(wreckRow, false);
                wreckArrowVec.SetVerticesDirty();
            }
            else wreckNow = null;
            // hidden at once (vf11's SetVisible(false)): no fade-out, so a save reads as saved the frame it happens
            if (show || !gWreck.Active) gWreck.Set(show, dt, rm); else gWreck.Snap(false);
        }

        // [arrow key] | [L / R cap] [direction arrow] R STICK | [named key] | TILT [direction arrow]
        void LayoutWreckRow(ReflexWreckHud w)
        {
            if (w.cap == cWreckCap && w.capName == cWreckCapName && w.direction == cWreckDir) return;
            cWreckCap = w.cap; cWreckCapName = w.capName; cWreckDir = w.direction;
            bool arrowKey = w.cap == ReflexWreckHud.Cap.ArrowKey, stick = w.cap == ReflexWreckHud.Cap.LeftStick || w.cap == ReflexWreckHud.Cap.RightStick, tilt = w.cap == ReflexWreckHud.Cap.Tilt;
            float keyW = w.cap == ReflexWreckHud.Cap.NamedKey ? Mathf.Max(48f, 20f + 13f * w.capName.Length) : 48f;
            wreckKey.rectTransform.sizeDelta = new Vector2(keyW, 40f); wreckKeyText.rectTransform.sizeDelta = new Vector2(keyW, 40f);
            wreckKey.radius = stick ? 20f : 8f;
            wreckKeyText.text = stick ? (w.cap == ReflexWreckHud.Cap.LeftStick ? "L" : "R") : arrowKey ? "" : tilt ? "" : w.capName;
            ReflexHudKit.Show(wreckKey, !tilt); ReflexHudKit.Show(wreckArrowVec, stick || tilt || arrowKey);
            wreckCapText.text = stick || tilt ? (tilt ? "TILT" : w.capName) : "";
            ReflexHudKit.Show(wreckCapText, stick || tilt);
            float capW = stick || tilt ? 10f + 11f * wreckCapText.text.Length : 0f;
            float gap = 10f, dirW = stick || tilt ? 44f : 0f;
            float total = (tilt ? 0f : keyW) + (dirW > 0 ? (tilt ? 0f : gap) + dirW : 0f) + (capW > 0 ? gap + capW : 0f);
            float x = -total * 0.5f;
            if (!tilt) { wreckKey.rectTransform.anchoredPosition = new Vector2(x + keyW * 0.5f, 0f); x += keyW; }
            // an arrow key shows its direction on the cap itself
            if (arrowKey) wreckArrowVec.rectTransform.anchoredPosition = wreckKey.rectTransform.anchoredPosition;
            else if (dirW > 0) { if (!tilt) x += gap; wreckArrowVec.rectTransform.anchoredPosition = new Vector2(x + dirW * 0.5f, 0f); x += dirW; }
            if (capW > 0) { x += gap; wreckCapText.rectTransform.anchoredPosition = new Vector2(x, 0f); x += capW; }
            wreckBack.rectTransform.sizeDelta = new Vector2(total + 28f, 56f);
        }
    }
}
