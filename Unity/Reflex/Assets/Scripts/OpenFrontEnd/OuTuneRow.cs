// One tuning row of GARAGE > VEHICLE > VEHICLE TUNING and of the race pause's TUNING: what the setting does, at a glance.
//
//   ENGINE                                              MORE TOP SPEED
//   ‹  ACCELERATION  ━━━━━━━━━━━|━━━━━●━━━━  TOP SPEED  ›
//      40                                       50
//
// The ends are the game's own words for the two sides (STR_TUNE_ENGINE1 / 2 "Acceleration" / "Top Speed", SUSPENSION1 / 2 "Soft" / "Hard",
// TIRES1 / 2 "Slippery" / "Sticky", BRAKING_LOW / HIGH "Soft" / "Hard", STEERING_LOW / HIGH "Slow" / "Quick"); the thumb sits where the
// setting is between them (the tick: where both sides are equal), the side it leans to is lit, and the verdict on the right says it in words
// (BALANCED, MORE TOP SPEED, MAX TOP SPEED; STANDARD / HARD for braking and steering). RIGHT always moves toward the right-hand end.
//   engine / suspension / tires: the equipped performance part's bonus points move between the two values (the total stays: MoveGarageTuning,
//     5 points a step); the ends show each value (the model's default + its points, of 100). Stock parts bring no points: the row says so.
//   braking / steering: 0 / 50 / 100, the original's three steps.
using System;
using System.Collections.Generic;
using Reflex.Port.FrontEnd;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed class OuTuneRow
    {
        public static readonly string[] Columns = { "Engine1", "Engine2", "Suspension1", "Suspension2", "Tire1", "Tire2", "Braking", "Steering" };
        static readonly string[] Keys = { "ENGINE", "SUSPENSION", "TIRES", "BRAKING", "STEERING" };
        static readonly (string low, string high)[] Fallback = { ("ACCELERATION", "TOP SPEED"), ("SOFT", "HARD"), ("SLIPPERY", "STICKY"), ("SOFT", "HARD"), ("SLOW", "QUICK") };

        public readonly VisualElement row;
        public readonly int index;
        readonly int[] values;
        readonly Dictionary<string, object> modelRow;
        readonly Label verdict, lowName, highName, lowValue, highValue, leftArrow, rightArrow;
        readonly VisualElement track, mid, fill, thumb;
        /// After a step that changed the values (save them); refused: a step that could not be made (why, for the status line).
        public Action changed;
        public Action<string> refused;

        public bool Split => index < 3;
        public string Name => OpenGarageData.StatLabel(index);
        public string LowEnd => End(index < 3 ? "STR_TUNE_" + Keys[index] + "1" : "STR_TUNE_" + Keys[index] + "_LOW", Fallback[index].low);
        public string HighEnd => End(index < 3 ? "STR_TUNE_" + Keys[index] + "2" : "STR_TUNE_" + Keys[index] + "_HIGH", Fallback[index].high);
        public string Description => OpenContent.Str("STR_TUNE_" + OpenGarageData.StatNames[index].ToUpperInvariant() + "_DESCRIPTION", "");
        static string End(string key, string fallback) { string s = OpenContent.Str(key, null); return string.IsNullOrEmpty(s) || s.StartsWith("STR_") ? fallback : s.ToUpperInvariant(); }

        int Base(int k) => FeTables.I(modelRow, "Default" + Columns[k], 0);

        /// values: the eight PlayerVehicleTuning numbers (changed in place); modelRow: the VehicleModel row (its Default* values).
        public OuTuneRow(VisualElement parent, string name, string extraClass, int index, int[] values, Dictionary<string, object> modelRow)
        {
            this.index = index; this.values = values; this.modelRow = modelRow;
            var sheet = Resources.Load<StyleSheet>(OuUi.ResourceRoot + "OuTune");
            row = OuUi.Item(OuUi.Div("mxa-row ou-tune2-row ou-tune2" + (string.IsNullOrEmpty(extraClass) ? "" : " " + extraClass), parent), null);
            if (sheet) row.styleSheets.Add(sheet);
            row.name = name;
            var head = OuUi.Div("ou-tune2-head", row);
            OuUi.Text(Name, "mxa-row-label ou-tune2-name", head);
            verdict = OuUi.Text("", "ou-tune2-verdict", head);
            var line = OuUi.Div("ou-tune2-line", row);
            leftArrow = OuUi.Text("‹", "mxa-arrow ou-tune2-arrow", line);
            var low = OuUi.Div("ou-tune2-end ou-tune2-end-low", line);
            lowName = OuUi.Text(LowEnd, "ou-tune2-end-name", low); lowValue = OuUi.Text("", "ou-tune2-end-value", low);
            track = OuUi.Div("ou-tune2-track", line);
            mid = OuUi.Div("ou-tune2-mid", track);
            fill = OuUi.Div("ou-tune2-fill", track);
            thumb = OuUi.Div("ou-tune2-thumb", track);
            var high = OuUi.Div("ou-tune2-end ou-tune2-end-high", line);
            highName = OuUi.Text(HighEnd, "ou-tune2-end-name", high); highValue = OuUi.Text("", "ou-tune2-end-value", high);
            rightArrow = OuUi.Text("›", "mxa-arrow ou-tune2-arrow", line);
            foreach (var e in new VisualElement[] { head, line, low, high, track, mid, fill, thumb }) e.pickingMode = PickingMode.Ignore;
            leftArrow.pickingMode = rightArrow.pickingMode = PickingMode.Position;
            leftArrow.RegisterCallback<ClickEvent>(e => { Step(-1); e.StopPropagation(); });
            rightArrow.RegisterCallback<ClickEvent>(e => { Step(+1); e.StopPropagation(); });
            var item = (OuNavItem)row.userData;
            item.adjust = d => Step(d);
            item.submit = () => Step(Position() >= 0.999f ? -2 : +1);   // A / Enter: one step right (at the right end: back to the left)
            Draw();
        }

        /// Where the setting sits between its two ends (0 the left end, 1 the right end).
        public float Position()
        {
            if (!Split) return Mathf.Clamp01(values[index + 3] / 100f);
            var (lo, hi) = Range();
            if (hi <= lo) return 0.5f;
            return Mathf.Clamp01((float)(hi - values[index * 2]) / (hi - lo));   // the left value's points: all of them at the left end
        }
        /// A split row's range of the left value's points (the total of both stays; neither value past 100, no points below 0).
        (int lo, int hi) Range()
        {
            int a = index * 2, total = values[a] + values[a + 1];
            return (Math.Max(0, total - (100 - Base(a + 1))), Math.Min(total, 100 - Base(a)));
        }
        public bool Locked => Split && values[index * 2] + values[index * 2 + 1] == 0;
        /// Where both sides would be equal (the centre mark): a split row's even point inside its range (an end when it is out of reach).
        public float Even()
        {
            if (!Split) return 0.5f;
            var (lo, hi) = Range(); if (hi <= lo) return 0.5f;
            int a = index * 2; float one = (values[a] + values[a + 1] + Base(a + 1) - Base(a)) / 2f;   // base1 + one == base2 + two
            return Mathf.Clamp01((hi - one) / (hi - lo));
        }
        /// The side the setting leans to: -1 the left end, +1 the right end, 0 even (split rows: by their two values; the others: by STANDARD).
        public int Lean()
        {
            if (!Split) return Math.Sign(values[index + 3] - 50);
            int a = index * 2;
            return Math.Sign(Base(a + 1) + values[a + 1] - (Base(a) + values[a]));
        }
        static readonly (string moreLow, string moreHigh, string mostLow, string mostHigh)[] Words =
        {
            ("MORE ACCELERATION", "MORE TOP SPEED", "MAX ACCELERATION", "MAX TOP SPEED"),
            ("SOFTER", "HARDER", "SOFTEST", "HARDEST"),
            ("MORE SLIPPERY", "STICKIER", "MOST SLIPPERY", "STICKIEST"),
        };

        /// d > 0: toward the right-hand end (d = -2: all the way back to the left).
        public void Step(int d)
        {
            if (d == -2)
            {
                bool any = false;
                while (Split ? Move(-1) : values[index + 3] > 0 && SetLevel(values[index + 3] - 50)) any = true;
                if (any) { Draw(); changed?.Invoke(); }
                return;
            }
            bool ok = Split ? Move(Math.Sign(d)) : SetLevel(values[index + 3] + Math.Sign(d) * 50);
            if (!ok)
            {
                refused?.Invoke(Locked ? ("NO " + Name + " POINTS: FIT A PERFORMANCE PART (CUSTOMIZE VEHICLE) TO TUNE IT")
                                       : (Name + " IS ALREADY AT " + (d > 0 ? HighEnd : LowEnd)));
                OuSound.Play("Locked"); return;
            }
            Draw(); changed?.Invoke();
        }
        bool Move(int toRight)
        {
            int a = index * 2;   // toward the right end: points from the left value to the right one
            if (!OpenShell.MoveGarageTuning(Base(a), Base(a + 1), values[a], values[a + 1], -toRight, out int one, out int two)) return false;
            values[a] = one; values[a + 1] = two; return true;
        }
        bool SetLevel(int v)
        {
            v = Mathf.Clamp(v, 0, 100);
            if (v == values[index + 3]) return false;
            values[index + 3] = v; return true;
        }

        /// The verdict in words.
        public string Verdict()
        {
            if (Locked) return "STOCK PART · NO POINTS";
            float p = Position();
            if (!Split) return p < 0.25f ? LowEnd : p > 0.75f ? HighEnd : End("STR_TUNE_" + Keys[index] + "_MID", "STANDARD");
            var w = Words[index];
            if (p <= 0.001f) return w.mostLow;   // as far as it goes
            if (p >= 0.999f) return w.mostHigh;
            int lean = Lean();
            return lean < 0 ? w.moreLow : lean > 0 ? w.moreHigh : "BALANCED";
        }

        public void Draw()
        {
            float p = Position();
            thumb.style.left = Length.Percent(p * 100f);
            float even = Even(); mid.style.left = Length.Percent(even * 100f);
            float from = Mathf.Min(p, even), to = Mathf.Max(p, even);   // the lit part: from the even mark to the thumb
            fill.style.left = Length.Percent(from * 100f); fill.style.width = Length.Percent((to - from) * 100f);
            verdict.text = Verdict();
            if (Split)
            {
                int a = index * 2;
                lowValue.text = (Base(a) + values[a]).ToString(); highValue.text = (Base(a + 1) + values[a + 1]).ToString();
            }
            else { lowValue.text = ""; highValue.text = ""; }
            lowValue.style.display = highValue.style.display = Split ? DisplayStyle.Flex : DisplayStyle.None;
            row.EnableInClassList("ou-tune2-locked", Locked);
            int side = Locked ? 0 : Lean();
            lowName.parent.EnableInClassList("ou-tune2-end-on", side < 0);
            highName.parent.EnableInClassList("ou-tune2-end-on", side > 0);
            leftArrow.EnableInClassList("ou-tune2-arrow-off", Locked || p <= 0.001f);
            rightArrow.EnableInClassList("ou-tune2-arrow-off", Locked || p >= 0.999f);
        }
    }
}
