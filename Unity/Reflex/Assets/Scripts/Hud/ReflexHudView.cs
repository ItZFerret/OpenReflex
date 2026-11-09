// The MODERN race HUD's view (Open Unleashed's MxaRaceHud look, MxaHudKit's panels and type): it draws a ReflexHudModel and nothing else,
// so the game's HUD driver (ReflexModernHud) and the gallery (synthetic models) use the same code.
//   top left      POSITION n / riders (and a +n / -n chip after a change), the field board (five rows centred on the player), and per
//                 mode the Freestyle judges (four columns of lit bars) or the Section Race leaders ("#n name")
//   top right     LAP n / N, the event timer (Freestyle, Section Race, challenges), BEST / LAST / CURRENT / the gap, the Section Race
//                 split and section best
//   top centre    the mode's counter: Eliminator riders remaining, Outrun gate counter and section timer, Waypoint gates passed /
//                 remaining and the progress, a Free Ride challenge's value and medal marks; the compass (Waypoint, Free Ride)
//   bottom right  the gauge: the gear and the speed (the game's MeasurementSystem) on a ring per REV GAUGE: RPM (the default) the engine's
//                 real rpm over its useful band, 0.3 x the limiter to the limiter (ReflexHudModel.rpmGauge, w-rpmgauge / w-revgauge; empty at
//                 idle; the red stretch is the top 15 %, where the shift light turns the fill and the rim amber; both flash red on the
//                 limiter); SPEED (and RPM without an engine reading: the legacy path) the speed (0 .. 90 mph); OFF no ring
//   centre        the start countdown (an activity's own 3-2-1, GO!; the Powercross board; the Overtake countdown; none for a start board), WRONG WAY / OFF TRACK / PENALTY,
//                 the "Player Eliminated" message, the Freestyle final score card, FINAL LAP, the camera's name
// Layout: 1920 x 1080 reference units on an 8 unit grid (ReflexHudKit.G), the type scale of ReflexHudKit (TypeLabel 22 up), everything
// anchored to the safe root's edges. Panels ease in and out, values flash when they change (a place gained / lost green / red, a new
// best lap, a penalty), all on real time and without allocating after the build; REDUCE MOTION skips the movement.
using System.Collections.Generic;
using Reflex.Port.Options;
using UnityEngine;
using UnityEngine.UI;

namespace Reflex.Port.Hud
{
    /// The user's HUD options (OPTIONS > CAMERA & HUD), read by the driver a few times a second and set directly by the gallery.
    public sealed class HudOptions
    {
        public float scale = 1f, opacity = 1f;
        public bool standings = true, gauge = true, reduceMotion, metric;
        /// REV GAUGE (ReflexSettings.RevGauge): ReflexSettings.RevGaugeRpm / RevGaugeSpeed / RevGaugeOff; the full HUD and the gauge-only supplement.
        public int revGauge = ReflexSettings.RevGaugeRpm;
        public void FromSettings()
        {
            scale = ReflexSettings.HudScale / 100f; opacity = ReflexSettings.HudOpacity / 100f;
            standings = ReflexSettings.HudStandings; gauge = ReflexSettings.HudGauge; reduceMotion = ReflexSettings.ReduceMotion; metric = ReflexSettings.Metric;
            revGauge = ReflexSettings.RevGauge;
        }
    }

    public sealed partial class ReflexHudView
    {
        public readonly HudOptions Options = new HudOptions();
        Canvas canvas; CanvasScaler scaler; RectTransform root; GameObject go;
        public Canvas Canvas => canvas;
        public GameObject GameObject => go;
        public bool Visible { get => canvas && canvas.enabled; set { if (canvas && canvas.enabled != value) canvas.enabled = value; } }
        /// What the last Apply showed (tests): the number of panel groups on screen.
        public int GroupsShown { get; private set; }
        /// Where the right column (LAP, the timer, the times and the gap, the Section Race split) ends, in screen pixels from the top; 0 while it
        /// shows nothing.
        public float RightColumnBottomPx { get; private set; }
        /// The left column's bottom in screen px from the top (0: the column is empty): PRACTICE's lap list goes under it (w-lb-practice).
        public float LeftColumnBottomPx { get; private set; }
        static readonly Vector3[] corners = new Vector3[4];

        readonly List<HudRect> panels = new List<HudRect>(48);
        readonly List<HudGroup> groups = new List<HudGroup>(32);
        float appliedOpacity = -1f;
        bool rm, snapNext; float dt, now;

        // grid + type shortcuts
        const float W = 344f, Pad = 24f, RowH = 40f, Gap = 16f;
        const int TLabel = ReflexHudKit.TypeLabel, TBody = ReflexHudKit.TypeBody;
        static readonly Vector2 TL = new Vector2(0, 1), TR = new Vector2(1, 1), TC = new Vector2(0.5f, 1), BL = Vector2.zero, BR = new Vector2(1, 0), BC = new Vector2(0.5f, 0), MC = new Vector2(0.5f, 0.5f), ML = new Vector2(0, 0.5f), MR = new Vector2(1, 0.5f);

        public static ReflexHudView Create(Transform parent, string name, int sortingOrder)
        {
            var v = new ReflexHudView();
            v.go = new GameObject(name);
            v.go.transform.SetParent(parent, false);
            v.root = ReflexHudKit.CreateCanvas(v.go, sortingOrder, out v.canvas);
            v.scaler = v.go.GetComponent<CanvasScaler>();
            v.BuildPosition(); v.BuildBoard(); v.BuildLap(); v.BuildTimes(); v.BuildTimer(); v.BuildGauge(); v.BuildCentre();
            v.BuildModes();
            v.BuildReflex();   // the wreck-recovery prompt (ReflexHudView.Reflex.cs)
            v.canvas.enabled = false;
            return v;
        }

        public void Destroy() { if (go) Object.Destroy(go); }

        /// Finish every animation on the next Apply (the gallery, a style switch).
        public void Snap() { snapNext = true; }

        // ------------------------------------------------------------------------------------------------ factories
        HudRect NewPanel(Transform parent, string name, Vector2 anchor, Vector2 pivot, Vector2 pos, Vector2 size, float fadeLeft = 0f, float fadeRight = 0f, float radius = 6f)
        {
            var p = ReflexHudKit.Panel(parent, name, anchor, pivot, pos, size, fadeLeft, fadeRight, radius);
            panels.Add(p);
            return p;
        }

        /// A coloured block that is not a panel background (accent bars, bars, fills): its alpha ignores HUD OPACITY.
        HudRect NewBlock(Transform parent, string name, Vector2 anchor, Vector2 pivot, Vector2 pos, Vector2 size, Color c, float radius = 0f)
        {
            var p = ReflexHudKit.Panel(parent, name, anchor, pivot, pos, size, 0f, 0f, radius);
            p.panel = false; p.top = p.bottom = c;
            return p;
        }

        HudGroup NewGroup(RectTransform rt, Vector2 slide, float inT = 0.30f, float outT = 0.18f)
        {
            var g = new HudGroup(rt, slide, inT, outT);
            groups.Add(g);
            return g;
        }

        static Text Lbl(Transform parent, string name, Font font, int size, Color color, TextAnchor align, Vector2 anchor, Vector2 pivot, Vector2 pos, Vector2 box)
            => ReflexHudKit.Label(parent, name, font, size, color, align, anchor, pivot, pos, box);

        static T Shade<T>(T t) where T : Graphic => ReflexHudKit.Shadowed(t);

        // ------------------------------------------------------------------------------------------------ top left
        RectTransform posRt; Text posNum, posOf, posLabel, chipText; HudFlash posFlash; HudGroup gPos, gChip; HudVector chipTri; bool chipUp;
        int cPlace = -1, cRiders = -1, chipDelta; float chipUntil = -1f; bool placeKnown;
        static readonly string[] OfTexts = new string[64];
        static string OfText(int n) { n = Mathf.Clamp(n, 0, 63); return OfTexts[n] ??= "/ " + n; }

        void BuildPosition()
        {
            var p = NewPanel(root, "Position", TL, TL, new Vector2(ReflexHudKit.Margin, -ReflexHudKit.MarginTop), new Vector2(W, 128), 0f, 112f);
            posRt = p.rectTransform;
            posLabel = Lbl(posRt, "Label", ReflexHudKit.Medium, TLabel, ReflexHudKit.Dim, TextAnchor.UpperLeft, TL, TL, new Vector2(Pad, -12), new Vector2(120, 28));
            posLabel.text = "POSITION";
            posNum = Shade(Lbl(posRt, "Num", ReflexHudKit.NumHeavy, ReflexHudKit.TypeNumXl, ReflexHudKit.White, TextAnchor.LowerLeft, BL, BL, new Vector2(Pad - 2, 4), new Vector2(200, 100)));
            posOf = Lbl(posRt, "Of", ReflexHudKit.Num, ReflexHudKit.TypeTitle, ReflexHudKit.Dim, TextAnchor.LowerLeft, BL, BL, new Vector2(Pad + 80, 16), new Vector2(160, 56));
            posFlash = new HudFlash(posNum);
            // the +n / -n chip beside the label after a change of place
            var chip = ReflexHudKit.Rect(posRt, "Chip", TL, TL, new Vector2(Pad + 108, -10), new Vector2(76, 30));
            chipTri = ReflexHudKit.Vector(chip, "Tri", ML, new Vector2(11, 0), new Vector2(22, 22));
            chipTri.Draw = v =>
            {
                float s = 8f;
                if (chipUp) v.Tri(new Vector2(0, s), new Vector2(-s, -s * 0.7f), new Vector2(s, -s * 0.7f), ReflexHudKit.Green);
                else v.Tri(new Vector2(0, -s), new Vector2(-s, s * 0.7f), new Vector2(s, s * 0.7f), ReflexHudKit.Red);
            };
            chipText = Lbl(chip, "N", ReflexHudKit.Bold, TBody, ReflexHudKit.White, TextAnchor.MiddleLeft, ML, ML, new Vector2(28, 0), new Vector2(48, 30));
            gChip = NewGroup(chip, new Vector2(0, -8), 0.2f, 0.3f);
            gPos = NewGroup(posRt, new Vector2(-48, 0));
        }

        void UpdatePosition(ReflexHudModel m, bool show)
        {
            gPos.Set(show, dt, rm);   // first: an object measures its text once it is active
            if (show)
            {
                if (m.place != cPlace || m.riders != cRiders)
                {
                    if (placeKnown && m.place != cPlace && cPlace > 0)
                    {
                        bool better = m.place < cPlace;
                        posFlash.Trigger(better ? ReflexHudKit.Green : ReflexHudKit.RedText, 0.14f);
                        chipUp = better; chipDelta = Mathf.Abs(m.place - cPlace); chipUntil = now + 2.2f;
                        chipTri.SetVerticesDirty(); chipText.text = ReflexHudModel.IntText(chipDelta); chipText.color = better ? ReflexHudKit.Green : ReflexHudKit.RedText;
                    }
                    cPlace = m.place; cRiders = m.riders; placeKnown = true;
                    posNum.text = ReflexHudModel.IntText(m.place);
                    posOf.text = OfText(m.riders);
                    var r = posOf.rectTransform; r.anchoredPosition = new Vector2(Pad + 8 + posNum.preferredWidth, r.anchoredPosition.y);
                }
            }
            else placeKnown = false;
            gChip.Set(show && now < chipUntil, dt, rm);
            posFlash.Tick(dt, rm);
        }

        // the field board
        const int BoardRows = 5;
        struct Row { public RectTransform rt; public Text pos, num, name, gap; }
        readonly Row[] rows = new Row[BoardRows];
        RectTransform boardRt; HudRect boardHi; HudGroup gBoard; int boardVersion = -1; float hiY, hiTarget; bool hiOn;

        void BuildBoard()
        {
            var b = NewPanel(root, "Standings", TL, TL, new Vector2(ReflexHudKit.Margin, -200), new Vector2(W, RowH * BoardRows), 0f, 0f, 6f);
            b.top = new Color(0.12f, 0.13f, 0.14f, 0.80f); b.bottom = new Color(0.07f, 0.075f, 0.085f, 0.80f);
            boardRt = b.rectTransform;
            ReflexHudKit.Cell(boardRt, "PosCol", 0, 48, new Color(0, 0, 0, 0.42f));
            ReflexHudKit.Cell(boardRt, "NumCol", 48, 48, new Color(0, 0, 0, 0.22f));
            var hi = ReflexHudKit.Rect(boardRt, "You", TL, TL, Vector2.zero, new Vector2(W, RowH));
            boardHi = ReflexHudKit.Graphic<HudRect>(hi);
            boardHi.radius = 0; boardHi.raycastTarget = false; boardHi.panel = false;
            boardHi.top = new Color(0.96f, 0.96f, 0.95f, 0.96f); boardHi.bottom = new Color(0.86f, 0.86f, 0.85f, 0.96f);
            var c1 = ReflexHudKit.Cell(hi, "Pos", 0, 48, new Color(0.72f, 0.72f, 0.71f, 0.55f)); c1.panel = false;
            var c2 = ReflexHudKit.Cell(hi, "Num", 48, 48, new Color(0.80f, 0.80f, 0.79f, 0.45f)); c2.panel = false;
            for (int i = 0; i < BoardRows; i++)
            {
                var r = new Row { rt = ReflexHudKit.Rect(boardRt, "Row" + i, TL, TL, new Vector2(0, -i * RowH), new Vector2(W, RowH)) };
                r.pos = Lbl(r.rt, "Pos", ReflexHudKit.Bold, 24, ReflexHudKit.White, TextAnchor.MiddleCenter, ML, ML, Vector2.zero, new Vector2(48, RowH));
                r.num = Lbl(r.rt, "Num", ReflexHudKit.Medium, TLabel, ReflexHudKit.White, TextAnchor.MiddleCenter, ML, ML, new Vector2(48, 0), new Vector2(48, RowH));
                r.name = Lbl(r.rt, "Name", ReflexHudKit.Medium, 24, ReflexHudKit.White, TextAnchor.MiddleLeft, ML, ML, new Vector2(108, 0), new Vector2(160, RowH));
                r.name.horizontalOverflow = HorizontalWrapMode.Wrap; r.name.verticalOverflow = VerticalWrapMode.Truncate;
                r.gap = Lbl(r.rt, "Gap", ReflexHudKit.Medium, TLabel, ReflexHudKit.White, TextAnchor.MiddleRight, MR, MR, new Vector2(-14, 0), new Vector2(72, RowH));
                rows[i] = r;
            }
            gBoard = NewGroup(boardRt, new Vector2(-48, 0));
        }

        static readonly string[] PosTexts = new string[32];
        static string PosText(int p) { p = Mathf.Clamp(p, 0, 31); return PosTexts[p] ??= p.ToString(); }

        void UpdateBoard(ReflexHudModel m, bool show)
        {
            if (show && boardVersion != m.StandingsVersion) { boardVersion = m.StandingsVersion; Standings(m); }
            if (!show) boardVersion = -1;
            gBoard.Set(show, dt, rm);
            if (hiOn)
            {
                hiY = rm || snapNext ? hiTarget : Mathf.Lerp(hiY, hiTarget, 1f - Mathf.Exp(-16f * dt));
                boardHi.rectTransform.anchoredPosition = new Vector2(0, hiY);
            }
        }

        void Standings(ReflexHudModel m)
        {
            var f = m.standings; int n = f.Count, me = Mathf.Max(0, m.youRow);
            int shown = Mathf.Min(BoardRows, n), first = Mathf.Clamp(me - BoardRows / 2, 0, Mathf.Max(0, n - BoardRows));
            for (int i = 0; i < BoardRows; i++)
            {
                var r = rows[i]; bool on = i < shown;
                if (r.rt.gameObject.activeSelf != on) r.rt.gameObject.SetActive(on);
                if (!on) continue;
                var s = f[first + i];
                ReflexHudKit.SetText(r.pos, PosText(s.position)); ReflexHudKit.SetText(r.num, s.number); ReflexHudKit.SetText(r.name, s.name); ReflexHudKit.SetText(r.gap, s.gapText);
                Color c = s.you ? ReflexHudKit.Ink : ReflexHudKit.White;
                r.pos.color = r.num.color = r.name.color = r.gap.color = c;
                var font = s.you ? ReflexHudKit.SemiBold : ReflexHudKit.Medium;
                if (r.name.font != font) r.name.font = font;
                if (s.you) hiTarget = -i * RowH;
            }
            boardRt.sizeDelta = new Vector2(W, shown * RowH);
            bool hi = m.youRow >= first && m.youRow < first + shown;
            if (hi && !hiOn) hiY = hiTarget;
            hiOn = hi;
            if (boardHi.gameObject.activeSelf != hi) boardHi.gameObject.SetActive(hi);
        }

        // ------------------------------------------------------------------------------------------------ top right
        RectTransform lapRt; Text lapNum, lapLabel; HudFlash lapFlash; HudGroup gLap; int cLap = -1, cLaps = -1;

        void BuildLap()
        {
            var p = NewPanel(root, "Lap", TR, TR, new Vector2(-ReflexHudKit.Margin, -ReflexHudKit.MarginTop), new Vector2(W, 72), 112f, 0f);
            lapRt = p.rectTransform;
            lapLabel = Lbl(lapRt, "Label", ReflexHudKit.Medium, TLabel, ReflexHudKit.Dim, TextAnchor.LowerRight, BR, BR, new Vector2(-Pad - 130, 14), new Vector2(120, 28));
            lapLabel.text = "LAP";
            lapNum = Shade(Lbl(lapRt, "Num", ReflexHudKit.NumHeavy, ReflexHudKit.TypeNumLg, ReflexHudKit.White, TextAnchor.LowerRight, BR, BR, new Vector2(-Pad, -2), new Vector2(200, 80)));
            lapFlash = new HudFlash(lapNum, null, lapNum.rectTransform);
            gLap = NewGroup(lapRt, new Vector2(48, 0));
        }

        string cTimed;
        void UpdateLap(ReflexHudModel m, bool show)
        {
            gLap.Set(show, dt, rm);
            if (show && m.timed)
            {   // RESTORED CONTENT (championship weekend TIMED RACES): the clock, then the laps after it, then the white flag's FINAL LAP
                // (ReflexHudModel.TimedText; the label reads CLOCK while it runs: the lap total is not known then)
                string plain = m.TimedText(false);
                if (plain != cTimed)
                {
                    if (cTimed != null && m.timedExpired && m.lap > cLap) lapFlash.Trigger(ReflexHudKit.White, 0.12f);
                    cTimed = plain; cLap = m.lap; cLaps = -1;
                    int sp = plain.IndexOf(' ');
                    lapNum.text = sp > 0 ? plain.Substring(0, sp) + "<size=40>" + plain.Substring(sp) + "</size>" : plain;
                    lapLabel.text = m.timedExpired ? "" : "CLOCK";   // ("+1 LAP" / "FINAL LAP" say it themselves)
                    var r = lapLabel.rectTransform; r.anchoredPosition = new Vector2(-Pad - 12 - lapNum.preferredWidth, r.anchoredPosition.y);
                }
                lapFlash.Tick(dt, rm);
                return;
            }
            if (cTimed != null) { cTimed = null; lapLabel.text = "LAP"; cLap = -1; }
            if (show && (m.lap != cLap || m.laps != cLaps))
            {
                if (cLap > 0 && m.lap > cLap) lapFlash.Trigger(ReflexHudKit.White, 0.12f);
                cLap = m.lap; cLaps = m.laps;
                lapNum.text = m.endless ? m.lap.ToString() : $"{m.lap}<size=40> / {m.laps}</size>";   // (PRACTICE: "LAP n")
                var r = lapLabel.rectTransform; r.anchoredPosition = new Vector2(-Pad - 12 - lapNum.preferredWidth, r.anchoredPosition.y);
            }
            lapFlash.Tick(dt, rm);
        }

        // BEST / LAST / CURRENT / the gap
        const int TimeRows = 4;
        RectTransform timesRt; HudGroup gTimes;
        readonly Text[] tLabel = new Text[TimeRows]; readonly ClockText[] tClock = new ClockText[TimeRows]; readonly HudFlash[] tFlash = new HudFlash[TimeRows];
        float prevBest = -1f, newBestUntil = -1f; bool bestKnown;

        void BuildTimes()
        {
            timesRt = NewPanel(root, "Times", TR, TR, new Vector2(-ReflexHudKit.Margin, -120), new Vector2(W, TimeRows * RowH + Gap), 96f, 0f).rectTransform;
            for (int i = 0; i < TimeRows; i++)
            {
                float y = -Gap * 0.5f - i * RowH;
                tLabel[i] = Lbl(timesRt, "L" + i, ReflexHudKit.Medium, 24, ReflexHudKit.Dim, TextAnchor.MiddleLeft, TL, TL, new Vector2(52, y), new Vector2(160, RowH));
                tClock[i] = new ClockText(timesRt, "V" + i, ReflexHudKit.Num, 32, 24, ReflexHudKit.Green, TR, TR, new Vector2(-Pad, y), new Vector2(170, RowH));
                tFlash[i] = new HudFlash(tClock[i].main, tClock[i].frac, tClock[i].rt);
            }
            tLabel[0].text = "BEST"; tLabel[1].text = "LAST"; tLabel[2].text = "CURRENT"; tLabel[3].text = "TO LEADER";
            tClock[2].Color = ReflexHudKit.White; tFlash[2].baseColor = ReflexHudKit.White;
            gTimes = NewGroup(timesRt, new Vector2(48, 0));
        }

        static readonly Color GapBehind = ReflexHudKit.RedText;

        void UpdateTimes(ReflexHudModel m, bool show)
        {
            bool gapOn = show && m.haveHud && m.racing && m.riders > 1 && m.mode != HudMode.Waypoint;
            if (show)
            {
                bool intro = m.state <= 1;
                // BEST: green once set; a new best lap flashes and says so for 3 s
                if (m.hasBest)
                {
                    if (bestKnown && prevBest > 0f && m.bestLap < prevBest - 0.0005f) { tFlash[0].Trigger(ReflexHudKit.Gold, 0.18f); newBestUntil = now + 3f; }
                    prevBest = m.bestLap; bestKnown = true;
                    tClock[0].SetTime(m.bestLap); tFlash[0].SetBase(ReflexHudKit.Green);
                }
                else { prevBest = -1f; bestKnown = true; tClock[0].SetDashes(); tFlash[0].SetBase(ReflexHudKit.Faint); }
                ReflexHudKit.SetText(tLabel[0], now < newBestUntil ? "NEW BEST" : "BEST");
                tClock[1].SetTime(m.lastLap); tFlash[1].SetBase(m.lastLap > 0f && m.lastLap < 3.0e38f ? ReflexHudKit.Green : ReflexHudKit.Faint);
                // CURRENT: the running lap; the last lap's time in amber for the game's 3 s hold
                float cur = intro ? 0.0001f : Mathf.Max(0.0001f, m.shownLapTime);
                tClock[2].SetTime(cur, '\0', false);
                tFlash[2].SetBase(m.holdingLastLap ? ReflexHudKit.Amber : ReflexHudKit.White);
                if (m.holdingLastLap && !holdWas) tFlash[2].Trigger(ReflexHudKit.Amber, 0.1f);
                holdWas = m.holdingLastLap;
                // the gap: behind the leader, or the lead over the next
                if (gapOn)
                {
                    bool leader = m.place <= 1;
                    float gap = leader ? m.timeAhead : m.timeBehind;
                    ReflexHudKit.SetText(tLabel[3], leader ? "AHEAD" : "TO LEADER");
                    tClock[3].SetTime(Mathf.Abs(gap), gap < 0 ? '-' : '+', false);
                    tFlash[3].SetBase(gap < 0 ? GapBehind : ReflexHudKit.Green);
                }
            }
            ReflexHudKit.Show(tLabel[3], gapOn); tClock[3].SetActive(gapOn);
            float h = ((gapOn ? 4 : 3) * RowH) + Gap;
            if (!Mathf.Approximately(timesRt.sizeDelta.y, h)) timesRt.sizeDelta = new Vector2(W, h);
            gTimes.Set(show, dt, rm);
            for (int i = 0; i < TimeRows; i++) tFlash[i].Tick(dt, rm);
        }
        bool holdWas;

        // the event timer
        RectTransform timerRt; Text timerLabel; ClockText timerClock; HudFlash timerFlash; HudGroup gTimer; bool timerWasLow, timerPulsed; float timerWarnClock;
        // the layer's "WarningTime" animation (EventTimerComponent, 30 fps keys): 0.5333 s, the clock grows to 120 % at 0.2333 s and back, its colour dipping to 18 %
        const float WarningLength = 0.5333334f, WarningPeak = 0.2333333f, WarningScale = 0.2f, WarningDip = 0.82f;

        void BuildTimer()
        {
            timerRt = NewPanel(root, "Timer", TR, TR, new Vector2(-ReflexHudKit.Margin, -ReflexHudKit.MarginTop), new Vector2(W, 72), 112f, 0f).rectTransform;
            timerLabel = Lbl(timerRt, "Label", ReflexHudKit.Medium, TLabel, ReflexHudKit.Dim, TextAnchor.LowerLeft, BL, BL, new Vector2(Pad + 24, 14), new Vector2(150, 28));
            timerLabel.text = "TIME";
            timerClock = new ClockText(timerRt, "Clock", ReflexHudKit.NumHeavy, ReflexHudKit.TypeNumMd, 36, ReflexHudKit.White, BR, BR, new Vector2(-Pad, -2), new Vector2(200, 80));
            Shade(timerClock.main);
            timerFlash = new HudFlash(timerClock.main, timerClock.frac, timerClock.rt);
            gTimer = NewGroup(timerRt, new Vector2(48, 0));
        }

        void UpdateTimer(ReflexHudModel m, bool show)
        {
            if (show)
            {
                ReflexHudKit.SetText(timerLabel, m.timerCountsUp ? "TIME" : "TIME LEFT");
                timerClock.SetTime(m.timerValue, '\0', false);
                // Freestyle's last 5 s: red (0xffcc3333) and the layer's WarningTime pulse, again each time it ends (005f7140)
                Color baseC = m.timerLow ? new Color(0.8f, 0.2f, 0.2f, 1f) : ReflexHudKit.White;
                timerFlash.SetBase(baseC);
                if (m.timerLow != timerWasLow) { timerWasLow = m.timerLow; timerWarnClock = 0f; }
                if (m.timerLow)
                {
                    timerWarnClock += dt;
                    float u = (timerWarnClock % WarningLength), tri = u < WarningPeak ? u / WarningPeak : (WarningLength - u) / (WarningLength - WarningPeak);
                    if (rm) tri = 0f;   // REDUCE MOTION: the red stays, nothing moves or dims
                    timerClock.rt.localScale = Vector3.one * (1f + WarningScale * tri); timerPulsed = true;
                    timerClock.Color = new Color(baseC.r * (1f - WarningDip * tri), baseC.g * (1f - WarningDip * tri), baseC.b * (1f - WarningDip * tri), 1f);
                }
                else if (timerPulsed) { timerPulsed = false; timerClock.rt.localScale = Vector3.one; }
            }
            gTimer.Set(show, dt, rm);
            timerFlash.Tick(dt, rm);
        }

        // ------------------------------------------------------------------------------------------------ the gauge
        // REV GAUGE (HudOptions.revGauge, w-revgauge): RPM the ring is the engine's useful band, ReflexHudModel.RpmFloor (0.3 x the limiter) to the
        // limiter: ticks every 1,000 rpm (2,000 past a 13,000 wide band), longer on the even thousands and at both ends, the red stretch the top
        // 15 %. The needle (two first-order stages, see UpdateGauge) and the shift light: the fill and the gauge's rim turn amber once the needle
        // is in the red stretch, and flash red on the limiter. SPEED (and RPM without an engine reading: the legacy path) the speed ring of
        // before w-rpmgauge: 0 .. GaugeMax mph, ten-mph ticks. OFF no ring: the disc, the gear and the speed stay.
        const float GaugeR = 116f, A0 = -138f, A1 = 128f, GaugeMax = 90f, RedFrom = 0.86f, Limiter = 0.995f;
        // the rev band's red stretch / the shift light (on at ShiftFrom, off below ShiftFrom - ShiftHyst), the limiter flash (held LimiterHold s
        // after the rpm leaves the limiter, so wheelspin bouncing off it reads as one flash, FlashHz)
        const float ShiftFrom = 0.85f, ShiftHyst = 0.04f, LimiterHold = 0.25f, FlashHz = 5f;
        // The needle: two first-order stages, the first rising with NeedleRise and falling with NeedleFall, the second NeedleSmooth both ways
        // (core/tools/rpm_gauge_trace.c simulates the same: keep in step). A step reads 63 % in 150 ms rising, 100 ms falling. Measured on
        // the 250F and the ATV (rpm_gauge_trace, full throttle): the frame-to-frame jitter on screen -38 % / -36 % against w-rpmgauge's ring
        // (one 50 ms stage, the idle band), while an upshift's drop shows 1.5 .. 2.3 x bigger than before (the band) and the revs in the
        // air still reach the top (0.97 .. 1.0 of the raw peak).
        // Faster falling than rising: the rpm's spikes are mostly upward (the rear wheel breaking loose; 2 : 1 over dips in the traces), and
        // the drops a rider reads (the upshift, the landing, the clutch let out) are falls.
        const float NeedleRise = 0.10f, NeedleFall = 0.05f, NeedleSmooth = 0.05f;
        RectTransform gaugeRt; HudVector gaugeBase, gaugeFill; Text gearText, speedText, unitText; HudFlash gearFlash; HudGroup gGauge;
        // fillShown: the ring's fill 0 .. 1 as drawn (the rev band, or the speed / GaugeMax); needleA the needle's first stage; light: 0 off,
        // 1 amber, 2 the limiter (flashOn its bright half); ringKind: what the ring shows (0 rev band, 1 speed, 2 none); bandFloor / bandMax
        // the rev band the base's ticks were drawn for
        float fillShown = -1f, needleA = -1f, limiterLeft, bandFloor = -1f, bandMax = -1f; int light, ringKind = -1; bool flashOn;
        int lastGear = -99, lastSpeed = -1; bool lastMetric, gaugeKnown;

        void BuildGauge()
        {
            float cx = -(ReflexHudKit.Margin + GaugeR + 8f), cy = 168f;
            gaugeRt = ReflexHudKit.Rect(root, "Gauge", BR, MC, new Vector2(cx, cy), new Vector2(GaugeR * 2 + 8, GaugeR * 2 + 8));
            gaugeBase = ReflexHudKit.Vector(gaugeRt, "Base", MC, Vector2.zero, gaugeRt.sizeDelta);
            gaugeBase.Draw = v =>
            {
                var c = Vector2.zero; float o = ReflexHudKit.PanelOpacity;
                v.Disc(c, GaugeR, new Color(0.035f, 0.04f, 0.045f, 0.68f * o), 128);
                v.Arc(c, GaugeR - 1.4f, GaugeR, -180f, 180f, new Color(1f, 1f, 1f, 0.16f), 128);
                v.Disc(c, GaugeR - 38f, new Color(0f, 0f, 0f, 0.22f * o), 96);
                if (ringKind == 2) return;
                v.Arc(c, GaugeR - 15f, GaugeR - 7f, A0, A1, new Color(1f, 1f, 1f, 0.16f));
                v.Arc(c, GaugeR - 15f, GaugeR - 7f, Mathf.Lerp(A0, A1, ringKind == 0 ? ShiftFrom : RedFrom), A1, new Color(0.86f, 0.10f, 0.12f, 0.45f));   // the redline
                if (ringKind == 0 && bandMax > bandFloor)
                {
                    // the rev band: a tick every 1,000 rpm (2,000 on a wide band), the even thousands and both ends long
                    float span = bandMax - bandFloor, step = span > 13000f ? 2000f : 1000f;
                    BandTick(v, c, A0, true); BandTick(v, c, A1, true);
                    for (float r = Mathf.Ceil(bandFloor / step) * step; r < bandMax; r += step)
                    {
                        float t = (r - bandFloor) / span; if (t * (A1 - A0) < 4f || (1f - t) * (A1 - A0) < 4f) continue;   // too close to an end tick
                        BandTick(v, c, Mathf.Lerp(A0, A1, t), Mathf.RoundToInt(r / step) % 2 == 0);
                    }
                }
                else
                    for (int k = 0; k <= 9; k++) v.Tick(c, Mathf.Lerp(A0, A1, k / 9f), GaugeR - (k % 3 == 0 ? 23f : 19f), GaugeR - 15f, k % 3 == 0 ? 2f : 1.3f, new Color(1f, 1f, 1f, k % 3 == 0 ? 0.75f : 0.38f));
            };
            gaugeFill = ReflexHudKit.Vector(gaugeRt, "Fill", MC, Vector2.zero, gaugeRt.sizeDelta);
            gaugeFill.Draw = v =>
            {
                if (ringKind == 2) return;
                // the shift light: the gauge's rim, amber in the red stretch, flashing red on the limiter
                if (light > 0)
                {
                    var rim = light == 2 ? new Color(ReflexHudKit.Red.r, ReflexHudKit.Red.g, ReflexHudKit.Red.b, flashOn ? 0.95f : 0.35f) : new Color(ReflexHudKit.Amber.r, ReflexHudKit.Amber.g, ReflexHudKit.Amber.b, 0.7f);
                    v.Arc(Vector2.zero, GaugeR - 2.6f, GaugeR + 0.6f, -180f, 180f, rim, 128);
                }
                float s = Mathf.Clamp01(fillShown); if (s <= 0.002f) return;
                float a = Mathf.Lerp(A0, A1, s);
                Color c;
                if (ringKind == 0) c = light == 2 ? (flashOn ? ReflexHudKit.Red : Color.Lerp(ReflexHudKit.Red, ReflexHudKit.White, 0.35f)) : light == 1 ? ReflexHudKit.Amber : ReflexHudKit.White;
                else c = s > RedFrom ? Color.Lerp(ReflexHudKit.White, ReflexHudKit.Amber, Mathf.InverseLerp(RedFrom, 1f, s)) : ReflexHudKit.White;
                v.Arc(Vector2.zero, GaugeR - 15f, GaugeR - 7f, A0, a, c);
                v.Tick(Vector2.zero, a, GaugeR - 19f, GaugeR - 3f, 3f, c);
            };
            gearText = Shade(Lbl(gaugeRt, "Gear", ReflexHudKit.NumHeavy, ReflexHudKit.TypeHero, ReflexHudKit.White, TextAnchor.LowerCenter, MC, new Vector2(0.5f, 0), new Vector2(0, -8), new Vector2(160, 110)));
            gearFlash = new HudFlash(gearText, null, null, 0.4f);
            var div = ReflexHudKit.Vector(gaugeRt, "Divider", MC, new Vector2(0, -6), new Vector2(130, 4));
            div.Draw = v => v.Segment(new Vector2(-58, 0), new Vector2(58, 0), 1.6f, new Color(1, 1, 1, 0.55f));
            speedText = Shade(Lbl(gaugeRt, "Speed", ReflexHudKit.Num, ReflexHudKit.TypeNumMd, ReflexHudKit.White, TextAnchor.UpperCenter, MC, new Vector2(0.5f, 1), new Vector2(0, -8), new Vector2(160, 64)));
            unitText = Lbl(gaugeRt, "Unit", ReflexHudKit.Num, TLabel, ReflexHudKit.Dim, TextAnchor.UpperCenter, MC, new Vector2(0.5f, 1), new Vector2(0, -66), new Vector2(160, 26));
            gGauge = NewGroup(gaugeRt, new Vector2(0, -24), 0.35f, 0.2f);
        }

        static void BandTick(HudVector v, Vector2 c, float a, bool major) =>
            v.Tick(c, a, GaugeR - (major ? 23f : 19f), GaugeR - 15f, major ? 2f : 1.3f, new Color(1f, 1f, 1f, major ? 0.75f : 0.38f));

        void UpdateGauge(ReflexHudModel m, bool show)
        {
            if (show)
            {
                int kind = Options.revGauge == ReflexSettings.RevGaugeOff ? 2 : Options.revGauge == ReflexSettings.RevGaugeRpm && m.hasRpm ? 0 : 1;
                float lo = kind == 0 ? ReflexHudModel.RpmFloor(m.idleRpm, m.maxRpm) : -1f, hi = kind == 0 ? m.maxRpm : -1f;
                if (kind != ringKind || !Mathf.Approximately(lo, bandFloor) || !Mathf.Approximately(hi, bandMax))
                {
                    if (kind != ringKind) { fillShown = needleA = -1f; light = 0; limiterLeft = 0f; }   // a new ring starts where the value is
                    ringKind = kind; bandFloor = lo; bandMax = hi;
                    gaugeBase.SetVerticesDirty(); gaugeFill.SetVerticesDirty();
                }
                float prev = fillShown; int prevLight = light; bool prevFlash = flashOn;
                if (kind == 0)
                {
                    // the needle follows the engine through two stages (NeedleRise / NeedleFall, then NeedleSmooth): the rear wheel's spin swings
                    // the rpm by a thousand in a frame or two; REDUCE MOTION keeps the damping (it is a filter, not a flourish)
                    float target = m.rpmGauge;
                    if (prev < 0f || needleA < 0f || snapNext) needleA = fillShown = target;
                    else
                    {
                        needleA += (target - needleA) * (1f - Mathf.Exp(-dt / (target > needleA ? NeedleRise : NeedleFall)));
                        fillShown += (needleA - fillShown) * (1f - Mathf.Exp(-dt / NeedleSmooth));
                    }
                    // the shift light: amber from the red stretch (by the needle, with hysteresis), red on the limiter (the raw rpm, held)
                    limiterLeft = m.rpmGauge >= Limiter ? LimiterHold : Mathf.Max(0f, limiterLeft - dt);
                    bool amber = fillShown >= ShiftFrom || (prevLight >= 1 && fillShown >= ShiftFrom - ShiftHyst);
                    light = limiterLeft > 0f ? 2 : amber ? 1 : 0;
                    flashOn = light == 2 && (rm || Mathf.Repeat(now * FlashHz, 1f) < 0.5f);   // REDUCE MOTION: a steady red
                }
                else if (kind == 1)
                {
                    // the speed ring (as before w-rpmgauge: one ~70 ms stage)
                    float target = Mathf.Clamp01(m.speedMph / GaugeMax);
                    fillShown = prev < 0f || rm || snapNext ? target : Mathf.Lerp(prev, target, 1f - Mathf.Exp(-14f * dt));
                    light = 0; flashOn = false;
                }
                else { fillShown = 0f; light = 0; flashOn = false; }
                if (Mathf.Abs(fillShown - prev) > 0.0005f || light != prevLight || flashOn != prevFlash) gaugeFill.SetVerticesDirty();
                float spd = Options.metric ? m.speedMph * 1.609344f : m.speedMph;
                int si = Mathf.RoundToInt(spd); bool metric = Options.metric;
                if (si != lastSpeed || metric != lastMetric || !gaugeKnown) { lastSpeed = si; lastMetric = metric; speedText.text = ReflexHudModel.IntText(si); unitText.text = metric ? "KM/H" : "MPH"; }
                if (m.gear != lastGear || !gaugeKnown)
                {
                    if (gaugeKnown) gearFlash.Trigger(ReflexHudKit.White, 0.08f);
                    lastGear = m.gear; gearText.text = m.gear < 0 ? "R" : ReflexHudModel.IntText(m.gear + 1);
                }
                gaugeKnown = true;
            }
            gGauge.Set(show, dt, rm);
            gearFlash.Tick(dt, rm);
        }

        // ------------------------------------------------------------------------------------------------ the centre
        HudRect countBand; Text count, countSub; HudGroup gCount; float countPunchAt = -10f; string cCountText, cSubKey;
        RectTransform warnRt; HudRect warnBar; Text warnTitle, warnSub; HudGroup gWarn; string cWarnTitle; float cPenalty = -1f;
        HudRect noticeBox; Text noticeText; HudGroup gNotice;
        RectTransform finalRt; Text finalText; HudGroup gFinal;
        RectTransform elimRt; Text elimNext, elimWord, elimName; HudGroup gElim; string cElim;

        void BuildCentre()
        {
            countBand = NewPanel(root, "CountBand", MC, MC, new Vector2(0, 250), new Vector2(860, 240), 300f, 300f, 0f);
            countBand.top = new Color(0.07f, 0.075f, 0.085f, 0.46f); countBand.bottom = new Color(0.04f, 0.045f, 0.05f, 0.46f);
            count = Shade(ReflexHudKit.Fill(countBand.rectTransform, "Count", ReflexHudKit.NumHeavy, ReflexHudKit.TypeDisplay, ReflexHudKit.White, TextAnchor.MiddleCenter));
            count.rectTransform.offsetMin = new Vector2(0, 56);
            countSub = Lbl(countBand.rectTransform, "Sub", ReflexHudKit.Medium, TBody, ReflexHudKit.Dim, TextAnchor.MiddleCenter, BC, BC, new Vector2(0, 20), new Vector2(640, 32));
            gCount = NewGroup(countBand.rectTransform, new Vector2(0, -16), 0.25f, 0.2f);

            var w = NewPanel(root, "Warning", MC, MC, new Vector2(0, 40), new Vector2(640, 112), 192f, 192f, 0f);
            warnRt = w.rectTransform;
            warnBar = NewBlock(warnRt, "Accent", TC, TC, Vector2.zero, new Vector2(400, 4), ReflexHudKit.Red);
            warnBar.fadeLeft = warnBar.fadeRight = 140f;
            warnTitle = Shade(Lbl(warnRt, "Title", ReflexHudKit.Bold, ReflexHudKit.TypeTitle, ReflexHudKit.White, TextAnchor.MiddleCenter, TC, TC, new Vector2(0, -12), new Vector2(600, 56)));
            warnSub = Lbl(warnRt, "Sub", ReflexHudKit.Medium, TBody, ReflexHudKit.Dim, TextAnchor.MiddleCenter, TC, TC, new Vector2(0, -68), new Vector2(600, 32));
            gWarn = NewGroup(warnRt, new Vector2(0, 16), 0.18f, 0.22f);

            noticeBox = NewPanel(root, "Notice", BC, BC, new Vector2(0, 120), new Vector2(520, 48), 160f, 160f, 0f);
            noticeText = ReflexHudKit.Fill(noticeBox.rectTransform, "Text", ReflexHudKit.Medium, TBody, ReflexHudKit.White, TextAnchor.MiddleCenter);
            gNotice = NewGroup(noticeBox.rectTransform, new Vector2(0, -12), 0.22f, 0.25f);

            var f = NewPanel(root, "FinalLap", TC, TC, new Vector2(0, -(ReflexHudKit.MarginTop + 232f)), new Vector2(560, 80), 200f, 200f, 0f);
            finalRt = f.rectTransform;
            finalText = Shade(ReflexHudKit.Fill(finalRt, "Text", ReflexHudKit.Bold, ReflexHudKit.TypeTitle, ReflexHudKit.White, TextAnchor.MiddleCenter));
            finalText.text = "FINAL LAP";
            gFinal = NewGroup(finalRt, new Vector2(0, 24), 0.3f, 0.3f);

            // the "Player Eliminated" message: <next> \n <STR_ELIMINATED> \n <driver>!
            var e = NewPanel(root, "Eliminated", MC, MC, new Vector2(0, 250), new Vector2(640, 184), 220f, 220f, 0f);
            elimRt = e.rectTransform;
            var bar = NewBlock(elimRt, "Accent", TC, TC, Vector2.zero, new Vector2(400, 4), ReflexHudKit.Red); bar.fadeLeft = bar.fadeRight = 140f;
            elimNext = Lbl(elimRt, "Next", ReflexHudKit.Medium, ReflexHudKit.TypeSub, ReflexHudKit.Dim, TextAnchor.MiddleCenter, TC, TC, new Vector2(0, -16), new Vector2(600, 40));
            elimWord = Shade(Lbl(elimRt, "Word", ReflexHudKit.NumHeavy, ReflexHudKit.TypeNumLg, ReflexHudKit.Red, TextAnchor.MiddleCenter, TC, TC, new Vector2(0, -56), new Vector2(600, 72)));
            elimName = Shade(Lbl(elimRt, "Name", ReflexHudKit.Bold, ReflexHudKit.TypeSub + 8, ReflexHudKit.White, TextAnchor.MiddleCenter, TC, TC, new Vector2(0, -128), new Vector2(600, 48)));
            gElim = NewGroup(elimRt, new Vector2(0, -20), 0.28f, 0.3f);
        }

        void UpdateCentre(ReflexHudModel m, bool full)
        {
            // ---- the countdown band: the Powercross board, the Overtake countdown, the start timer, GO!
            string text = null, sub = null; Color col = ReflexHudKit.White; int size = ReflexHudKit.TypeDisplay; bool numeric = true;
            if (full && m.powercross && m.pcVisible)
            {
                if (m.pcStep > 0) text = ReflexHudModel.IntText(m.pcStep); else { text = HudStrings.Go; col = ReflexHudKit.Green; }
                sub = "POWERCROSS";
            }
            else if (full && m.overtakeShown)
            {
                text = m.overtakeText; sub = m.ModeTitle;
                if (text == HudStrings.Go) col = ReflexHudKit.Green;
            }
            else if (full && m.state == 1 && m.countdownStep >= 1 && !m.powercross && m.mode != HudMode.FreeRide)
            {
                // an activity without a start board counts its own steps (Waypoint, Terracross, Freestyle, Section Race ...): the step
                // the original's BeginningCountdownComponent shows; an Overtake hides it (005fb830): its own countdown follows at InProgress
                if (m.mode != HudMode.Overtake) { text = ReflexHudModel.IntText(Mathf.Min(m.countdownStep, 9)); sub = SubLine(m); }
            }
            // the start board (National, Supercross): no countdown. The original shows none: the board at the gate turns and the gate drops
            // at the board's random time, so the start is the rider's reaction (the HUD never shows race.start_timer)
            else if (full && now < m.goUntil && !m.powercross) { text = HudStrings.Go; col = ReflexHudKit.Green; sub = ""; }
            if (text != null)
            {
                if (text != cCountText)
                {
                    cCountText = text; countPunchAt = now;
                    count.text = text; count.color = col; count.fontSize = size; count.font = numeric ? ReflexHudKit.NumHeavy : ReflexHudKit.SemiBold;
                }
                if (sub != null) SetSub(sub, m);
                // each new number lands: a quick scale-down with a hint of overshoot
                float t = (now - countPunchAt) / 0.4f;
                float s = rm ? 1f : t >= 1f ? 1f : Mathf.Lerp(1.35f, 1f, HudEase.OutBack(t));
                count.rectTransform.localScale = new Vector3(s, s, 1f);
            }
            else cCountText = null;
            gCount.Set(text != null, dt, rm);

            // ---- WRONG WAY / OFF TRACK / PENALTY (while racing)
            string title = null, wsub = null; Color accent = ReflexHudKit.Red; bool pulse = false;
            if (full && m.racing)
            {
                if (m.wrongWay) { title = "WRONG WAY"; wsub = !m.onTrack ? "OFF TRACK" : "TURN AROUND"; pulse = true; }
                else if (!m.onTrack) { title = "OFF TRACK"; accent = ReflexHudKit.Amber; wsub = m.resetTimer > 0 ? "RESETTING TO THE TRACK" : "RETURN TO THE TRACK"; }
                else if (m.penaltyShown) { title = "PENALTY"; accent = ReflexHudKit.Amber; }
            }
            if (title != null)
            {
                if (title != cWarnTitle) { cWarnTitle = title; warnTitle.text = title; warnTitle.color = title == "PENALTY" ? ReflexHudKit.Amber : ReflexHudKit.White; warnBar.top = warnBar.bottom = accent; warnBar.SetVerticesDirty(); cPenalty = -1f; }
                if (title == "PENALTY")
                {
                    if (!Mathf.Approximately(cPenalty, m.penalty)) { cPenalty = m.penalty; warnSub.text = "+" + ClockString(m.penalty); }
                }
                else ReflexHudKit.SetText(warnSub, wsub ?? "");
                warnSub.color = title == "PENALTY" ? ReflexHudKit.Amber : ReflexHudKit.Dim;
                // an urgent warning breathes
                warnBar.color = pulse && !rm ? new Color(1f, 1f, 1f, 0.55f + 0.45f * Mathf.Sin(now * 9f)) : Color.white;
            }
            else cWarnTitle = null;
            gWarn.Set(title != null, dt, rm);

            // ---- the camera's name; FINAL LAP; "Player Eliminated"
            bool toast = full && now < m.cameraUntil && !string.IsNullOrEmpty(m.cameraToast);
            if (toast) ReflexHudKit.SetText(noticeText, m.cameraToast);
            gNotice.Set(toast, dt, rm);
            gFinal.Set(full && now < m.finalLapUntil, dt, rm);
            bool elim = full && now <= m.eliminatedUntil && !string.IsNullOrEmpty(m.eliminatedText);
            if (elim && m.eliminatedText != cElim)
            {
                cElim = m.eliminatedText;
                int a = cElim.IndexOf('\n'), b = cElim.LastIndexOf('\n');
                if (a >= 0 && b > a) { elimNext.text = cElim.Substring(0, a); elimWord.text = cElim.Substring(a + 1, b - a - 1).ToUpperInvariant(); elimName.text = cElim.Substring(b + 1); }
                else { elimNext.text = ""; elimWord.text = cElim; elimName.text = ""; }
            }
            gElim.Set(elim, dt, rm);
        }

        void SetSub(string sub, ReflexHudModel m)
        {
            if (sub != cSubKey) { cSubKey = sub; countSub.text = sub; }
        }

        string subCache; int subLaps = -1, subRiders = -1;
        string SubLine(ReflexHudModel m)
        {
            if (m.inRace && m.mode != HudMode.Freestyle)
            {
                string timedKey = m.timed ? m.TimedFormat : null;
                if (subLaps != m.laps || subRiders != m.riders || m.mode != subMode || !string.Equals(subTimed, timedKey) || !ReferenceEquals(subStage, m.champStage))
                {
                    subLaps = m.laps; subRiders = m.riders; subMode = m.mode; subTimed = timedKey; subStage = m.champStage;
                    string lead = (m.champStage != null ? m.champStage + "  ·  " : "") + (m.mode == HudMode.Race ? "" : m.ModeTitle + "  ·  ");
                    // (a timed championship stage: its format, never the placeholder lap total)
                    string format = m.timed ? timedKey : m.laps + (m.laps == 1 ? " LAP" : " LAPS");
                    subCache = m.endless ? "PRACTICE  ·  UNLIMITED LAPS" : lead + format + "  ·  " + m.riders + (m.riders == 1 ? " RIDER" : " RIDERS");
                }
                return subCache;
            }
            return m.ModeTitle;
        }
        HudMode subMode; string subTimed, subStage;

        /// m:ss.cc as text (rare, cached by the caller).
        public static string ClockString(float t) => ReflexHudKit.Clock(t);

        // ------------------------------------------------------------------------------------------------ one frame
        /// Draw the model. presentation: 0 hidden, 1 the gauge alone (the classic-speedometer supplement), 2 the full HUD.
        public void Apply(ReflexHudModel m, float deltaTime, int presentation)
        {
            dt = Mathf.Min(deltaTime, 0.1f); now = Time.unscaledTime;
            rm = Options.reduceMotion || snapNext;
            ApplyOptions();
            ShowAll(m, presentation);
            snapNext = false;
        }

        void ApplyOptions()
        {
            ReflexHudKit.Scale = Options.scale;
            ReflexHudKit.ApplyScale(scaler);
            float o = Mathf.Clamp(Options.opacity, 0.4f, 1f);
            if (!Mathf.Approximately(appliedOpacity, o))
            {
                appliedOpacity = o; ReflexHudKit.PanelOpacity = o;
                for (int i = 0; i < panels.Count; i++) panels[i].SetVerticesDirty();
                gaugeBase.SetVerticesDirty();
            }
        }

        /// Stack a column's group under the previous ones (smoothly), then advance the cursor by its height when it is on.
        void Stack(HudGroup g, bool visible, ref float cursor, float x, float h)
        {
            float target = -cursor;
            if (!g.Active || rm) g.curY = target; else g.curY += (target - g.curY) * (1f - Mathf.Exp(-14f * dt));
            g.SetBase(new Vector2(x, g.curY));
            if (visible) cursor += h + Gap;
        }

        float Height(HudGroup g) => g.rt.sizeDelta.y;

        void ShowAll(ReflexHudModel m, int presentation)
        {
            bool full = presentation == 2;
            var mode = m.mode;
            bool racer = full && (mode == HudMode.Race || mode == HudMode.Eliminator || mode == HudMode.Overtake || mode == HudMode.Outrun || mode == HudMode.SectionRace || mode == HudMode.Waypoint);
            bool pos = racer && Options.standings && (m.inRace || m.riders > 1);
            bool board = pos && m.standings.Count > 1;
            bool fmx = full && mode == HudMode.Freestyle;
            bool lap = racer && m.inRace;
            bool timer = full && m.timerShown;
            bool times = racer && m.inRace;
            bool section = full && m.sectionShown;
            bool leaders = section;
            bool gaugeOn = presentation == 1 || (full && Options.gauge && m.Covered);

            // the columns (smoothly restacked as panels come and go)
            float y = ReflexHudKit.MarginTop, x = ReflexHudKit.Margin;
            Stack(gPos, pos, ref y, x, 128f);
            Stack(gBoard, board, ref y, x, Height(gBoard));
            Stack(gJudges, fmx, ref y, x, Height(gJudges));
            Stack(gLeaders, leaders, ref y, x, Height(gLeaders));
            if (y > ReflexHudKit.MarginTop && canvas) { root.GetWorldCorners(corners); LeftColumnBottomPx = Screen.height - corners[1].y + y * canvas.scaleFactor; }
            else LeftColumnBottomPx = 0f;
            y = ReflexHudKit.MarginTop; x = -ReflexHudKit.Margin;
            Stack(gLap, lap, ref y, x, 72f);
            Stack(gTimer, timer, ref y, x, 72f);
            Stack(gTimes, times, ref y, x, Height(gTimes));
            Stack(gSection, section, ref y, x, Height(gSection));
            if (y > ReflexHudKit.MarginTop && canvas) { root.GetWorldCorners(corners); RightColumnBottomPx = Screen.height - corners[1].y + y * canvas.scaleFactor; }
            else RightColumnBottomPx = 0f;

            UpdatePosition(m, pos);
            UpdateBoard(m, board);
            UpdateLap(m, lap);
            UpdateTimes(m, times);
            UpdateTimer(m, timer);
            UpdateGauge(m, gaugeOn);
            UpdateCentre(m, full);
            UpdateModes(m, full, fmx, section);
            UpdateReflex(m, full);

            int shown = 0; for (int i = 0; i < groups.Count; i++) if (groups[i].Active) shown++;
            GroupsShown = shown;
        }
    }
}
