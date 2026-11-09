// The MODERN HUD's mode widgets (ReflexHudView): what OriginalHud / OriginalHud.RaceHud.cs draw for the activities beyond the plain
// lap race, from the same core copies (ReflexHudModel): Eliminator, Outrun, Section Race, Freestyle, Waypoint, Free Ride (+ its
// challenges); the compass; the Freestyle final score card.
using Reflex.Port.Options;
using UnityEngine;
using UnityEngine.UI;

namespace Reflex.Port.Hud
{
    public sealed partial class ReflexHudView
    {
        static readonly Vector2 UC = new Vector2(0.5f, 1);   // upper centre

        HudRect Strip(string name, float width, float height)
            => NewPanel(root, name, TC, TC, new Vector2(0, -ReflexHudKit.MarginTop), new Vector2(width, height), 64f, 64f, 6f);

        void BuildModes()
        {
            BuildElimStrip(); BuildOutrunStrip(); BuildWaypointStrip(); BuildChallengeStrip(); BuildCompass(); BuildDiscovery();
            BuildJudges(); BuildLeaders(); BuildSection(); BuildCard();
            BuildStunts();
        }

        void UpdateModes(ReflexHudModel m, bool full, bool fmx, bool section)
        {
            var mode = m.mode;
            UpdateElimStrip(m, full && mode == HudMode.Eliminator);
            UpdateOutrunStrip(m, full && mode == HudMode.Outrun && (m.gateShown || m.outrunTimerShown));
            UpdateWaypointStrip(m, full && mode == HudMode.Waypoint);
            UpdateChallengeStrip(m, full && mode == HudMode.Challenge && m.challengeState >= 1 && m.challengeState <= 3);
            UpdateCompass(m, full && (mode == HudMode.Waypoint || mode == HudMode.FreeRide));
            UpdateDiscovery(m, full && mode == HudMode.FreeRide && m.discoveryTotal > 0);
            UpdateJudges(m, fmx);
            UpdateLeaders(m, section);
            UpdateSection(m, section);
            UpdateCard(m, full && mode == HudMode.Freestyle && m.finalShown);
            UpdateStunts(m, full && m.stuntShown);
        }

        // ------------------------------------------------------------------------------------------------ the trick names (StuntFeedbackComponent)
        // MXUIStuntFeedbackOverlay: StuntSlot1 (newest, orange, 52) / StuntSlot2 (white, smaller) / StuntSlot3 (white, fainter), the older names rising
        // up and to the left of it; each begin plays "RotateStunts" (the names move up a place, the new one drops in); 3 s after the last it is gone.
        RectTransform stuntRt; readonly Text[] stuntLine = new Text[3]; readonly string[] cStunt = { null, null, null }; HudGroup gStunts; HudFlash stuntFlash; int cStuntSerial = -1; float stuntPunchAt = -10f;
        static readonly Color StuntNew = new Color(0.97f, 0.52f, 0.22f, 1f);

        void BuildStunts()
        {
            stuntRt = ReflexHudKit.Rect(root, "Stunts", BL, BL, new Vector2(ReflexHudKit.Margin, 248), new Vector2(760, 200));
            int[] size = { ReflexHudKit.TypeNumMd, 40, 32 };
            Color[] color = { StuntNew, ReflexHudKit.White, ReflexHudKit.Dim };
            Vector2[] pos = { new Vector2(48, 0), new Vector2(24, 72), new Vector2(0, 126) };
            for (int i = 0; i < 3; i++)
                stuntLine[i] = Shade(Lbl(stuntRt, "Slot" + (i + 1), ReflexHudKit.NumHeavy, size[i], color[i], TextAnchor.LowerLeft, BL, BL, pos[i], new Vector2(700, 64)));
            stuntFlash = new HudFlash(stuntLine[0]);
            gStunts = NewGroup(stuntRt, new Vector2(-28, 0), 0.2f, 0.3f);
        }

        void UpdateStunts(ReflexHudModel m, bool show)
        {
            if (show)
            {
                for (int i = 0; i < 3; i++)
                    if (cStunt[i] != m.stuntText[i]) { cStunt[i] = m.stuntText[i]; stuntLine[i].text = cStunt[i]; }
                if (m.stuntSerial != cStuntSerial)
                {
                    cStuntSerial = m.stuntSerial;
                    stuntFlash.Trigger(ReflexHudKit.White, 0.12f);   // the new name drops in white and settles to the orange
                    stuntFlash.SetBase(StuntNew);
                }
            }
            gStunts.Set(show, dt, rm);
            stuntFlash.Tick(dt, rm);
        }

        // ------------------------------------------------------------------------------------------------ Eliminator
        Text elimLabel, elimNum; HudFlash elimFlash; HudGroup gElimStrip; int cLeft = -1, cTotal = -1;

        void BuildElimStrip()
        {
            var rt = Strip("RidersRemaining", 360f, 104f).rectTransform;
            elimLabel = Lbl(rt, "Label", ReflexHudKit.Medium, ReflexHudKit.TypeLabel, ReflexHudKit.Dim, TextAnchor.UpperCenter, UC, UC, new Vector2(0, -10), new Vector2(340, 28));
            elimLabel.text = "RIDERS REMAINING";
            elimNum = Shade(Lbl(rt, "Num", ReflexHudKit.NumHeavy, ReflexHudKit.TypeNumLg, ReflexHudKit.White, TextAnchor.MiddleCenter, BC, BC, new Vector2(0, 6), new Vector2(340, 70)));
            elimFlash = new HudFlash(elimNum);
            gElimStrip = NewGroup(rt, new Vector2(0, 24));
        }

        void UpdateElimStrip(ReflexHudModel m, bool show)
        {
            if (show && (m.ridersLeft != cLeft || m.ridersTotal != cTotal))
            {
                if (cLeft > 0 && m.ridersLeft < cLeft) elimFlash.Trigger(ReflexHudKit.Red, 0.16f);
                cLeft = m.ridersLeft; cTotal = m.ridersTotal;
                elimNum.text = $"{m.ridersLeft}<size=40> / {m.ridersTotal}</size>";
            }
            gElimStrip.Set(show, dt, rm);
            elimFlash.Tick(dt, rm);
        }

        // ------------------------------------------------------------------------------------------------ Outrun
        Text gateText, sectionLabel; ClockText sectionClock; HudFlash gateFlash; HudGroup gOutrun; int cGate = -1, cGateTotal = -1;

        void BuildOutrunStrip()
        {
            var rt = Strip("Outrun", 480f, 96f).rectTransform;
            gateText = Lbl(rt, "Gate", ReflexHudKit.SemiBold, ReflexHudKit.TypeBody + 4, ReflexHudKit.White, TextAnchor.MiddleLeft, TL, TL, new Vector2(Pad, -14), new Vector2(220, 34));
            gateFlash = new HudFlash(gateText);
            sectionLabel = Lbl(rt, "Label", ReflexHudKit.Medium, ReflexHudKit.TypeLabel, ReflexHudKit.Dim, TextAnchor.MiddleLeft, TL, TL, new Vector2(Pad, -52), new Vector2(200, 28));
            sectionLabel.text = "SECTION TIME";
            sectionClock = new ClockText(rt, "Clock", ReflexHudKit.NumHeavy, 56, 34, ReflexHudKit.White, MR, MR, new Vector2(-Pad, -2), new Vector2(230, 80));
            Shade(sectionClock.main);
            gOutrun = NewGroup(rt, new Vector2(0, 24));
        }

        void UpdateOutrunStrip(ReflexHudModel m, bool show)
        {
            if (show)
            {
                if (m.gateShown)
                {
                    if (m.gateNumber != cGate || m.gateTotal != cGateTotal)
                    {
                        if (cGate >= 0) gateFlash.Trigger(ReflexHudKit.Green, 0.12f);
                        cGate = m.gateNumber; cGateTotal = m.gateTotal;
                        gateText.text = HudStrings.Gate + " " + m.gateNumber + "/" + m.gateTotal;   // "%s %d/%d" (00605190)
                    }
                }
                else { cGate = -1; ReflexHudKit.SetText(gateText, ""); }
                sectionLabel.gameObject.SetActive(m.outrunTimerShown); sectionClock.SetActive(m.outrunTimerShown);
                if (m.outrunTimerShown) sectionClock.SetTime(m.outrunSectionTime, '\0', false);
            }
            gOutrun.Set(show, dt, rm);
            gateFlash.Tick(dt, rm);
        }

        // ------------------------------------------------------------------------------------------------ Waypoint
        const float WaypointW = 520f, WaypointH = 112f;
        Text wpPassedLabel, wpPassed, wpRemainLabel, wpRemain; HudRect wpFill; HudGroup gWaypoint; HudFlash wpFlash; float wpFrac; int cPassed = -1, cGates = -1;

        void BuildWaypointStrip()
        {
            var rt = Strip("Waypoint", WaypointW, WaypointH).rectTransform;
            wpPassedLabel = Lbl(rt, "PassedLabel", ReflexHudKit.Medium, ReflexHudKit.TypeLabel, ReflexHudKit.Dim, TextAnchor.UpperLeft, TL, TL, new Vector2(Pad, -12), new Vector2(240, 28));
            wpPassedLabel.text = "GATES PASSED";
            wpPassed = Shade(Lbl(rt, "Passed", ReflexHudKit.NumHeavy, ReflexHudKit.TypeNumMd, ReflexHudKit.White, TextAnchor.LowerLeft, BL, BL, new Vector2(Pad, 22), new Vector2(240, 64)));
            wpFlash = new HudFlash(wpPassed);
            wpRemainLabel = Lbl(rt, "RemainLabel", ReflexHudKit.Medium, ReflexHudKit.TypeLabel, ReflexHudKit.Dim, TextAnchor.UpperRight, TR, TR, new Vector2(-Pad, -12), new Vector2(200, 28));
            wpRemainLabel.text = "REMAINING";
            wpRemain = Lbl(rt, "Remaining", ReflexHudKit.Num, ReflexHudKit.TypeNumMd - 8, ReflexHudKit.White, TextAnchor.LowerRight, BR, BR, new Vector2(-Pad, 24), new Vector2(220, 56));
            NewBlock(rt, "Track", BL, BL, new Vector2(Pad, 14), new Vector2(WaypointW - 2 * Pad, 4), new Color(1, 1, 1, 0.16f), 2f);
            wpFill = NewBlock(rt, "Progress", BL, BL, new Vector2(Pad, 14), new Vector2(0, 4), ReflexHudKit.Green, 2f);
            gWaypoint = NewGroup(rt, new Vector2(0, 24));
        }

        void UpdateWaypointStrip(ReflexHudModel m, bool show)
        {
            if (show)
            {
                if (m.gatesPassed != cPassed || m.gateCount != cGates || !m.hasWaypoint)
                {
                    if (cPassed >= 0 && m.gatesPassed > cPassed) wpFlash.Trigger(ReflexHudKit.Green, 0.12f);
                    cPassed = m.gatesPassed; cGates = m.gateCount;
                    wpPassed.text = m.hasWaypoint && m.gateCount > 0 ? $"{m.gatesPassed}<size=36> / {m.gateCount}</size>" : "—";
                }
                float target = m.hasWaypoint && m.gateCount > 0 ? Mathf.Clamp01(m.gatesPassed / (float)m.gateCount) : 0f;
                wpFrac = rm ? target : Mathf.Lerp(wpFrac, target, 1f - Mathf.Exp(-10f * dt));
                wpFill.rectTransform.sizeDelta = new Vector2((WaypointW - 2 * Pad) * wpFrac, 4f);
                bool dist = m.waypointDistanceValid;
                wpRemainLabel.gameObject.SetActive(dist); wpRemain.gameObject.SetActive(dist);
                if (dist)
                {
                    float shown = Mathf.Round(m.routeDistance * (Options.metric ? 0.3048f : 1f));
                    if (!Mathf.Approximately(shown, cDist) || Options.metric != cDistMetric) { cDist = shown; cDistMetric = Options.metric; wpRemain.text = WaypointHudText.Distance(m.routeDistance, Options.metric); }
                }
            }
            gWaypoint.Set(show, dt, rm);
            wpFlash.Tick(dt, rm);
        }
        float cDist = -1f; bool cDistMetric;

        // ------------------------------------------------------------------------------------------------ Free Ride challenges
        const float ChallengeW = 560f, ChallengeH = 112f;
        Text chLabel, chValue, chUnit; ClockText chClock; HudFlash chFlash; HudGroup gChallenge; HudVector chMedals; readonly Text[] chCrit = new Text[3];
        readonly bool[] chHit = new bool[3]; int chKindShown = -99; bool chMetricShown; float cChValue = float.NaN; float chCritKey = float.NaN;

        void BuildChallengeStrip()
        {
            var rt = Strip("Challenge", ChallengeW, ChallengeH).rectTransform;
            chLabel = Lbl(rt, "Label", ReflexHudKit.Medium, ReflexHudKit.TypeLabel, ReflexHudKit.Dim, TextAnchor.UpperLeft, TL, TL, new Vector2(Pad, -12), new Vector2(240, 28));
            chValue = Shade(Lbl(rt, "Value", ReflexHudKit.NumHeavy, ReflexHudKit.TypeNumLg, ReflexHudKit.White, TextAnchor.LowerLeft, BL, BL, new Vector2(Pad, 12), new Vector2(180, 74)));
            chUnit = Lbl(rt, "Unit", ReflexHudKit.Num, ReflexHudKit.TypeBody, ReflexHudKit.Dim, TextAnchor.LowerLeft, BL, BL, new Vector2(Pad + 100, 18), new Vector2(80, 34));
            chClock = new ClockText(rt, "Clock", ReflexHudKit.NumHeavy, ReflexHudKit.TypeNumLg, 40, ReflexHudKit.White, BL, BL, new Vector2(Pad, 12), new Vector2(210, 74));
            Shade(chClock.main);
            chFlash = new HudFlash(chValue);
            var medals = ReflexHudKit.Rect(rt, "Medals", MR, MR, new Vector2(-Pad + 8, 0), new Vector2(264, 96));
            chMedals = ReflexHudKit.Vector(medals, "Marks", MC, new Vector2(0, 16), new Vector2(264, 48));
            chMedals.Draw = v =>
            {
                Color[] mc = { ReflexHudKit.Bronze, ReflexHudKit.Silver, ReflexHudKit.Gold };
                for (int i = 0; i < 3; i++)
                {
                    var c = new Vector2((i - 1) * 88f, 0f);
                    v.Arc(c, 14f, 17f, -180f, 180f, new Color(1f, 1f, 1f, chHit[i] ? 0.9f : 0.30f), 40);
                    if (chHit[i]) v.Disc(c, 11f, mc[i], 32); else v.Disc(c, 11f, new Color(1f, 1f, 1f, 0.07f), 32);
                }
            };
            for (int i = 0; i < 3; i++)
                chCrit[i] = Lbl(medals, "Crit" + i, ReflexHudKit.Medium, ReflexHudKit.TypeLabel, ReflexHudKit.Dim, TextAnchor.MiddleCenter, MC, MC, new Vector2((i - 1) * 88f, -26), new Vector2(86, 28));
            gChallenge = NewGroup(rt, new Vector2(0, 24));
        }

        static string ChallengeLabel(int kind) => kind == 1 ? "HEIGHT" : kind == 6 ? "TIME" : kind == 8 ? "HITS" : "SCORE";

        void UpdateChallengeStrip(ReflexHudModel m, bool show)
        {
            gChallenge.Set(show, dt, rm);
            if (show)
            {
                int kind = m.challengeKind; bool metric = Options.metric;
                bool timeKind = kind == 6 || kind == 3, heightKind = kind == 1;
                if (kind != chKindShown)
                {
                    chKindShown = kind; chLabel.text = ChallengeLabel(kind); chCritKey = float.NaN; cChValue = float.NaN;
                    chClock.SetActive(timeKind); chValue.gameObject.SetActive(!timeKind); chUnit.gameObject.SetActive(heightKind);
                }
                float val = m.challengeValueValid ? m.challengeValue : -1f;
                if (timeKind) chClock.SetTime(val > 0f ? val : 0f, '\0', true);
                else if (!Mathf.Approximately(val, cChValue) || metric != chMetricShown)
                {
                    if (cChValue >= 0f && val > cChValue && !float.IsNaN(cChValue)) chFlash.Trigger(ReflexHudKit.Green, 0.14f);
                    cChValue = val; chMetricShown = metric;
                    if (val < 0f) { chValue.text = "—"; chUnit.text = ""; }
                    else
                    {
                        int n = Mathf.RoundToInt(heightKind && metric ? val * 0.3048f : val);
                        chValue.text = ReflexHudModel.IntText(n);
                        if (heightKind) { chUnit.text = metric ? "M" : "FT"; var r = chUnit.rectTransform; r.anchoredPosition = new Vector2(Pad + 10 + chValue.preferredWidth, r.anchoredPosition.y); }
                    }
                }
                // the medal criteria (Bronze, Silver, Gold) and which are reached
                float key = m.challengeCriteria[0] + m.challengeCriteria[1] * 1000f + m.challengeCriteria[2] * 1e6f + (metric ? 0.5f : 0f);
                bool dirty = false;
                if (!Mathf.Approximately(key, chCritKey))
                {
                    chCritKey = key;
                    for (int i = 0; i < 3; i++)
                    {
                        float c = m.challengeCriteria[i];
                        chCrit[i].text = timeKind ? ReflexHudKit.Clock(c) : ReflexHudModel.IntText(Mathf.RoundToInt(heightKind && metric ? c * 0.3048f : c));
                    }
                    dirty = true;
                }
                for (int i = 0; i < 3; i++)
                {
                    float c = m.challengeCriteria[i];
                    bool hit = m.challengeValueValid && c > 0f && (timeKind ? val > 0f && val <= c : val >= c);
                    if (hit != chHit[i]) { chHit[i] = hit; dirty = true; }
                }
                if (dirty) chMedals.SetVerticesDirty();
            }
            chFlash.Tick(dt, rm);
        }

        // ------------------------------------------------------------------------------------------------ Free Ride: Hit Discovery
        // MXUIHitDiscoveredComponent (006045b0 / 006046f0): "found/total" on a find, for 3 s; here the count stays under the compass while the
        // Free Ride has a discovery tracker (the challenges Free Ride) and lights up for those 3 s after a find.
        const float DiscoveryW = 300f, DiscoveryH = 96f;
        Text discLabel, discNum; HudFlash discFlash; HudGroup gDisc; int cDiscFound = -1, cDiscTotal = -1, cDiscSerial = -1;

        void BuildDiscovery()
        {
            var rt = NewPanel(root, "HitDiscovery", TC, TC, new Vector2(0, -(ReflexHudKit.MarginTop + CompassH + ReflexHudKit.G)), new Vector2(DiscoveryW, DiscoveryH), 64f, 64f, 6f).rectTransform;
            discLabel = Lbl(rt, "Label", ReflexHudKit.Medium, ReflexHudKit.TypeLabel, ReflexHudKit.Dim, TextAnchor.UpperCenter, UC, UC, new Vector2(0, -10), new Vector2(DiscoveryW - 20, 28));
            discLabel.text = "DISCOVERIES";
            discNum = Shade(Lbl(rt, "Num", ReflexHudKit.NumHeavy, ReflexHudKit.TypeNumMd, ReflexHudKit.White, TextAnchor.MiddleCenter, BC, BC, new Vector2(0, 8), new Vector2(DiscoveryW - 20, 56)));
            discFlash = new HudFlash(discNum);
            gDisc = NewGroup(rt, new Vector2(0, 24));
        }

        void UpdateDiscovery(ReflexHudModel m, bool show)
        {
            if (show)
            {
                if (m.discoveryFound != cDiscFound || m.discoveryTotal != cDiscTotal)
                {
                    cDiscFound = m.discoveryFound; cDiscTotal = m.discoveryTotal;
                    discNum.text = $"{m.discoveryFound}<size=32> / {m.discoveryTotal}</size>";
                }
                if (m.discoverySerial != cDiscSerial)
                {
                    if (cDiscSerial >= 0) discFlash.Trigger(ReflexHudKit.Green, 0.2f);
                    cDiscSerial = m.discoverySerial;
                }
            }
            gDisc.Set(show, dt, rm);
            discFlash.Tick(dt, rm);
        }

        // ------------------------------------------------------------------------------------------------ the compass
        const float CompassW = 560f, CompassH = 72f, Ppd = 2.8f;
        HudVector compassTicks; HudGroup gCompass; float compassHeading, drawnHeading = -999f;
        // the Waypoint race's gates (the original's compass, 005ff150): the next gate full, the one after it at 0.35; a gate past the strip's
        // edge sits on the edge as an arrow pointing the way to turn (the original pins its icon at 45 degrees)
        const float GateAfterAlpha = 0.35f, GateEdge = CompassW * 0.5f - 16f;
        readonly HudVector[] gateMark = new HudVector[2]; readonly int[] gateShape = { 9, 9 };   // 0 the pin, -1 / +1 the arrow to the left / right
        readonly Text[] compassLabels = new Text[8];
        static readonly string[] CompassNames = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };

        void BuildCompass()
        {
            var p = NewPanel(root, "Compass", TC, TC, new Vector2(0, -ReflexHudKit.MarginTop), new Vector2(CompassW, CompassH), 90f, 90f, 6f);
            var rt = p.rectTransform;
            var clip = ReflexHudKit.Rect(rt, "Clip", MC, MC, Vector2.zero, new Vector2(CompassW, CompassH));
            ReflexHudKit.Stretch(clip); clip.gameObject.AddComponent<RectMask2D>();
            compassTicks = ReflexHudKit.Vector(clip, "Ticks", MC, Vector2.zero, new Vector2(CompassW, CompassH));
            compassTicks.Draw = v =>
            {
                float h = compassHeading, half = CompassW * 0.5f;
                int start = Mathf.FloorToInt((h - half / Ppd) / 5f), end = Mathf.CeilToInt((h + half / Ppd) / 5f);
                for (int k = start; k <= end; k++)
                {
                    float ang = k * 5f, x = (ang - h) * Ppd;
                    float fade = 1f - HudEase.Saturate((Mathf.Abs(x) / half - 0.5f) / 0.5f);
                    if (fade <= 0.01f) continue;
                    bool big = k % 9 == 0, mid = k % 3 == 0;   // 45 degrees, 15 degrees
                    float len = big ? 16f : mid ? 11f : 6f;
                    v.Segment(new Vector2(x, -30f), new Vector2(x, -30f + len), big ? 2.2f : 1.4f, new Color(1f, 1f, 1f, (big ? 0.85f : mid ? 0.55f : 0.30f) * fade));
                }
            };
            for (int i = 0; i < 8; i++)
            {
                bool card = i % 2 == 0;
                compassLabels[i] = Lbl(clip, CompassNames[i], card ? ReflexHudKit.Bold : ReflexHudKit.Medium, card ? ReflexHudKit.TypeBody : ReflexHudKit.TypeLabel,
                    i == 0 ? ReflexHudKit.Amber : ReflexHudKit.White, TextAnchor.MiddleCenter, MC, MC, new Vector2(0, 10), new Vector2(60, 36));
                compassLabels[i].text = CompassNames[i];
            }
            for (int g = 0; g < 2; g++)
            {
                int slot = g;
                gateMark[g] = ReflexHudKit.Vector(clip, g == 0 ? "NextGate" : "GateAfter", MC, new Vector2(0, -8f), new Vector2(28, 28));
                gateMark[g].Draw = v =>
                {
                    var col = ReflexHudKit.Amber; var edge = new Color(0f, 0f, 0f, 0.55f);
                    int shape = gateShape[slot];
                    if (shape == 0)
                    {   // a pin pointing down at the gate's bearing
                        v.Tri(new Vector2(-9f, 8f), new Vector2(9f, 8f), new Vector2(0f, -9f), edge);
                        v.Tri(new Vector2(-7f, 7f), new Vector2(7f, 7f), new Vector2(0f, -6f), col);
                    }
                    else
                    {   // the arrow on the strip's edge, pointing the way to turn
                        float sx = shape;
                        v.Tri(new Vector2(-7f * sx, -10f), new Vector2(-7f * sx, 10f), new Vector2(10f * sx, 0f), edge);
                        v.Tri(new Vector2(-5.5f * sx, -7.5f), new Vector2(-5.5f * sx, 7.5f), new Vector2(7.5f * sx, 0f), col);
                    }
                };
                gateMark[g].gameObject.SetActive(false);
            }
            var mark = ReflexHudKit.Vector(rt, "Marker", BC, new Vector2(0, 6), new Vector2(24, 14));
            mark.Draw = v => v.Tri(new Vector2(0, 5), new Vector2(-5, -4), new Vector2(5, -4), ReflexHudKit.White);
            gCompass = NewGroup(rt, new Vector2(0, 24));
        }

        void UpdateCompass(ReflexHudModel m, bool show)
        {
            if (show)
            {
                // under the Waypoint strip, at the top for a Free Ride
                float y = m.mode == HudMode.Waypoint ? -(ReflexHudKit.MarginTop + WaypointH + ReflexHudKit.G) : -ReflexHudKit.MarginTop;
                gCompass.SetBase(new Vector2(0, y));
                compassHeading = m.heading;
                if (Mathf.Abs(Mathf.DeltaAngle(compassHeading, drawnHeading)) > 0.2f) { drawnHeading = compassHeading; compassTicks.SetVerticesDirty(); }
                float half = CompassW * 0.5f;
                for (int i = 0; i < 8; i++)
                {
                    float d = Mathf.DeltaAngle(compassHeading, i * 45f), x = d * Ppd;
                    var t = compassLabels[i];
                    float fade = 1f - HudEase.Saturate((Mathf.Abs(x) / half - 0.5f) / 0.5f);
                    bool on = fade > 0.02f;
                    if (t.gameObject.activeSelf != on) t.gameObject.SetActive(on);
                    if (!on) continue;
                    t.rectTransform.anchoredPosition = new Vector2(x, 10f);
                    var c = t.color; c.a = fade; t.color = c;
                }
                PlaceGate(0, m.mode == HudMode.Waypoint && m.nextGateShown, m.nextGateAngle, 1f);
                PlaceGate(1, m.mode == HudMode.Waypoint && m.afterGateShown, m.afterGateAngle, GateAfterAlpha);
            }
            gCompass.Set(show, dt, rm);
        }

        void PlaceGate(int g, bool on, float angle, float alpha)
        {
            var mk = gateMark[g];
            if (mk.gameObject.activeSelf != on) mk.gameObject.SetActive(on);
            if (!on) return;
            float x = Mathf.DeltaAngle(compassHeading, angle) * Ppd;
            int shape = x > GateEdge ? 1 : x < -GateEdge ? -1 : 0;
            x = Mathf.Clamp(x, -GateEdge, GateEdge);
            if (shape != gateShape[g]) { gateShape[g] = shape; mk.SetVerticesDirty(); }
            mk.rectTransform.anchoredPosition = new Vector2(x, shape == 0 ? -13f : -4f);
            mk.canvasRenderer.SetAlpha(alpha);   // (the vertices carry the colour: Graphic.color is not applied)
        }

        // ------------------------------------------------------------------------------------------------ Freestyle: the judges
        const int JudgeCols = 4, JudgeBars = 10;
        HudGroup gJudges; readonly HudRect[,] jBar = new HudRect[JudgeCols, JudgeBars]; readonly int[] jShown = new int[JudgeCols]; readonly float[] jFlash = new float[JudgeCols];
        static readonly Color JudgeOff = new Color(1f, 1f, 1f, 0.11f);

        void BuildJudges()
        {
            var rt = NewPanel(root, "Judges", TL, TL, new Vector2(ReflexHudKit.Margin, -ReflexHudKit.MarginTop), new Vector2(W, 268f), 0f, 56f).rectTransform;
            var title = Lbl(rt, "Title", ReflexHudKit.Medium, ReflexHudKit.TypeLabel, ReflexHudKit.Dim, TextAnchor.UpperLeft, TL, TL, new Vector2(Pad, -12), new Vector2(200, 28));
            title.text = "JUDGES";
            for (int j = 0; j < JudgeCols; j++)
            {
                float x = Pad + j * 72f;
                for (int k = 0; k < JudgeBars; k++)
                    jBar[j, k] = NewBlock(rt, "J" + (j + 1) + "B" + (k + 1), BL, BL, new Vector2(x, 56f + k * 16f), new Vector2(56f, 12f), JudgeOff, 2f);
                // judge j shows its number j + 1 under the bars (00607370)
                var n = Lbl(rt, "J" + (j + 1), ReflexHudKit.SemiBold, ReflexHudKit.TypeBody, ReflexHudKit.Dim, TextAnchor.MiddleCenter, BL, BL, new Vector2(x, 14), new Vector2(56, 34));
                n.text = ReflexHudModel.IntText(j + 1);
            }
            gJudges = NewGroup(rt, new Vector2(-48, 0));
        }

        void UpdateJudges(ReflexHudModel m, bool show)
        {
            if (show)
            {
                for (int j = 0; j < JudgeCols; j++)
                {
                    int n = m.judgeBars[j];
                    if (n > jShown[j]) jFlash[j] = 0f;
                    jShown[j] = n;
                    if (jFlash[j] < 1f) jFlash[j] = rm ? 1f : Mathf.Min(1f, jFlash[j] + dt / 0.45f);
                    for (int k = 0; k < JudgeBars; k++)
                    {
                        Color c = JudgeOff;
                        if (k < n)
                        {
                            c = Color.Lerp(ReflexHudKit.Amber, ReflexHudKit.Green, k / 9f);
                            if (k == n - 1) c = Color.Lerp(Color.white, c, HudEase.OutCubic(jFlash[j]));
                        }
                        jBar[j, k].SetColors(c, c);
                    }
                }
            }
            gJudges.Set(show, dt, rm);
        }

        // ------------------------------------------------------------------------------------------------ Section Race: the leaders, the split times
        HudGroup gLeaders; RectTransform leadersRt; Text leadersTitle; readonly Text[] leaderNum = new Text[5], leaderName = new Text[5]; int leaderRowsShown = -1; readonly bool[] leaderWas = new bool[5];
        const float LeaderRow = 36f;

        void BuildLeaders()
        {
            leadersRt = NewPanel(root, "SectionLeaders", TL, TL, new Vector2(ReflexHudKit.Margin, -ReflexHudKit.MarginTop), new Vector2(W, 56f + 5 * LeaderRow + 8f), 0f, 64f).rectTransform;
            leadersTitle = Lbl(leadersRt, "Title", ReflexHudKit.Medium, ReflexHudKit.TypeLabel, ReflexHudKit.Dim, TextAnchor.UpperLeft, TL, TL, new Vector2(Pad, -12), new Vector2(240, 28));
            leadersTitle.text = HudStrings.Sections;
            for (int i = 0; i < 5; i++)
            {
                float y = -44f - i * LeaderRow;
                leaderNum[i] = Lbl(leadersRt, "N" + i, ReflexHudKit.Bold, 24, ReflexHudKit.Dim, TextAnchor.MiddleLeft, TL, TL, new Vector2(Pad, y), new Vector2(48, LeaderRow));
                leaderNum[i].text = HudStrings.Pound + (i + 1);
                leaderName[i] = Lbl(leadersRt, "Name" + i, ReflexHudKit.Medium, 24, ReflexHudKit.White, TextAnchor.MiddleLeft, TL, TL, new Vector2(Pad + 52, y), new Vector2(240, LeaderRow));
            }
            gLeaders = NewGroup(leadersRt, new Vector2(-48, 0));
        }

        void UpdateLeaders(ReflexHudModel m, bool show)
        {
            if (show)
            {
                int rows = m.sectionCount > 0 ? Mathf.Min(5, m.sectionCount) : 5;
                for (int i = 0; i < 5; i++)
                {
                    bool on = i < rows;
                    if (leaderNum[i].gameObject.activeSelf != on) { leaderNum[i].gameObject.SetActive(on); leaderName[i].gameObject.SetActive(on); }
                    if (!on) continue;
                    bool set = (m.leaderSet & (1u << i)) != 0;
                    ReflexHudKit.SetText(leaderName[i], set ? m.leaderNames[i] : "");
                    if (set != leaderWas[i]) { leaderWas[i] = set; leaderNum[i].color = set ? ReflexHudKit.White : ReflexHudKit.Faint; }
                }
                if (rows != leaderRowsShown) { leaderRowsShown = rows; leadersRt.sizeDelta = new Vector2(W, 56f + rows * LeaderRow + 8f); }
            }
            gLeaders.Set(show, dt, rm);
        }

        HudGroup gSection; Text sectionHeader; ClockText splitClock, bestClock; Text splitLabel, bestLabel; int cSecCur = -99, cSecCount = -1;

        void BuildSection()
        {
            var rt = NewPanel(root, "Section", TR, TR, new Vector2(-ReflexHudKit.Margin, -300), new Vector2(W, 3 * RowH + Gap), 96f, 0f).rectTransform;
            sectionHeader = Lbl(rt, "Header", ReflexHudKit.Medium, 24, ReflexHudKit.Dim, TextAnchor.MiddleLeft, TL, TL, new Vector2(52, -Gap * 0.5f), new Vector2(240, RowH));
            splitLabel = Lbl(rt, "SplitLabel", ReflexHudKit.Medium, 24, ReflexHudKit.Dim, TextAnchor.MiddleLeft, TL, TL, new Vector2(52, -Gap * 0.5f - RowH), new Vector2(160, RowH)); splitLabel.text = "SPLIT";
            bestLabel = Lbl(rt, "BestLabel", ReflexHudKit.Medium, 24, ReflexHudKit.Dim, TextAnchor.MiddleLeft, TL, TL, new Vector2(52, -Gap * 0.5f - 2 * RowH), new Vector2(160, RowH)); bestLabel.text = "SECTION BEST";
            splitClock = new ClockText(rt, "Split", ReflexHudKit.Num, 32, 24, ReflexHudKit.White, TR, TR, new Vector2(-Pad, -Gap * 0.5f - RowH), new Vector2(170, RowH));
            bestClock = new ClockText(rt, "Best", ReflexHudKit.Num, 32, 24, ReflexHudKit.Green, TR, TR, new Vector2(-Pad, -Gap * 0.5f - 2 * RowH), new Vector2(170, RowH));
            gSection = NewGroup(rt, new Vector2(48, 0));
        }

        void UpdateSection(ReflexHudModel m, bool show)
        {
            if (show)
            {
                if (m.sectionCurrent != cSecCur || m.sectionCount != cSecCount)
                {
                    cSecCur = m.sectionCurrent; cSecCount = m.sectionCount;
                    sectionHeader.text = m.sectionCurrent >= 0 ? "SECTION " + (m.sectionCurrent + 1) + (m.sectionCount > 0 ? " / " + m.sectionCount : "") : "SECTION —";
                }
                splitClock.SetTime(m.sectionTime, '\0', true);
                bestClock.SetTime(m.sectionBest, '\0', true);
            }
            gSection.Set(show, dt, rm);
        }

        // ------------------------------------------------------------------------------------------------ Freestyle: the final score card
        const float CardW = 1000f, CardH = 296f;
        RectTransform cardRt; HudGroup gCard; readonly Text[] cardLabel = new Text[5], cardValue = new Text[5]; readonly HudRect[] cardBar = new HudRect[5];
        int cardSerial = -1; float cardAt = -10f; bool cardWasShown;
        static readonly string[] CardNames = { "OVERALL", "DIFFICULTY", "EXECUTION", "FLOW", "VARIETY" };
        static readonly string[] ScoreTexts = new string[1101];
        static string ScoreText(float v)
        {
            int h = Mathf.Clamp(Mathf.RoundToInt(v * 100f), 0, 1100);
            return ScoreTexts[h] ??= (h / 100f).ToString("0.00", System.Globalization.CultureInfo.InvariantCulture);   // "%2.2f"
        }

        static Color ScoreColor(float v) => v >= 7.5f ? ReflexHudKit.Green : v >= 5f ? ReflexHudKit.White : ReflexHudKit.Amber;

        void BuildCard()
        {
            var p = NewPanel(root, "ScoreCard", MC, MC, new Vector2(0, 30), new Vector2(CardW, CardH), 0f, 0f, 8f);
            cardRt = p.rectTransform;
            p.top = new Color(0.14f, 0.15f, 0.17f, 0.90f); p.bottom = new Color(0.06f, 0.065f, 0.075f, 0.92f);
            var title = Lbl(cardRt, "Title", ReflexHudKit.SemiBold, ReflexHudKit.TypeSub, ReflexHudKit.Dim, TextAnchor.UpperCenter, TC, TC, new Vector2(0, -20), new Vector2(600, 44));
            title.text = "FINAL SCORE";
            float x = (CardW - (240f + 4 * 160f + 4 * 16f)) * 0.5f;
            for (int i = 0; i < 5; i++)
            {
                float w = i == 0 ? 240f : 160f; bool main = i == 0;
                var cell = ReflexHudKit.Rect(cardRt, CardNames[i], BL, BL, new Vector2(x, 28), new Vector2(w, 188));
                var cellBg = ReflexHudKit.Graphic<HudRect>(cell); cellBg.raycastTarget = false; cellBg.panel = false; cellBg.radius = 6;
                cellBg.top = new Color(1, 1, 1, main ? 0.10f : 0.06f); cellBg.bottom = new Color(1, 1, 1, main ? 0.05f : 0.03f);
                cardValue[i] = Shade(Lbl(cell, "Value", ReflexHudKit.NumHeavy, main ? ReflexHudKit.TypeNumXl : ReflexHudKit.TypeNumLg, ReflexHudKit.White, TextAnchor.MiddleCenter, MC, MC, new Vector2(0, 14), new Vector2(w, 110)));
                cardLabel[i] = Lbl(cell, "Label", ReflexHudKit.Medium, main ? ReflexHudKit.TypeBody : ReflexHudKit.TypeLabel, ReflexHudKit.Dim, TextAnchor.LowerCenter, BC, BC, new Vector2(0, 16), new Vector2(w, 32));
                cardLabel[i].text = CardNames[i];
                cardBar[i] = NewBlock(cell, "Bar", BL, BL, new Vector2(12f, 0f), new Vector2(w - 24f, 4f), ReflexHudKit.White, 2f);
                x += w + 16f;
            }
            gCard = NewGroup(cardRt, new Vector2(0, -12), 0.25f, 0.3f);
        }

        void UpdateCard(ReflexHudModel m, bool show)
        {
            if (show)
            {
                if (!cardWasShown || m.finalSerial != cardSerial) { cardSerial = m.finalSerial; cardAt = now; }
                // the In animation: the card lands with a little overshoot and the ratings count up
                float t = rm ? 1f : (now - cardAt);
                float s = rm ? 1f : Mathf.LerpUnclamped(0.88f, 1f, HudEase.OutBack(t / 0.5f));
                cardRt.localScale = new Vector3(s, s, 1f);
                float count = rm ? 1f : HudEase.OutCubic((t - 0.25f) / 1.0f);
                for (int i = 0; i < 5; i++)
                {
                    float v = m.finalScores[i];
                    ReflexHudKit.SetText(cardValue[i], ScoreText(v * count));
                    var c = ScoreColor(v * count);
                    if (cardValue[i].color != c) { cardValue[i].color = c; cardBar[i].top = cardBar[i].bottom = c; cardBar[i].SetVerticesDirty(); }
                    float frac = Mathf.Clamp01(v * count / 10f);
                    cardBar[i].rectTransform.sizeDelta = new Vector2((i == 0 ? 216f : 136f) * frac, 4f);
                }
            }
            cardWasShown = show;
            gCard.Set(show, dt, rm);
        }
    }
}
