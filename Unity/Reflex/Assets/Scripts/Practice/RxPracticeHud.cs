// RxPracticeHud: PRACTICE's lap list, a UI Toolkit document over the world (HudPanelUi: the glass panel with the amber bar, the HUD's
// condensed type). RxPractice adds it to a practice world.
//   LAP LIST   left, under the MODERN HUD's left column (top left with the CLASSIC HUD): "PRACTICE · LAP 12 · 0:41.2" (the lap from the lap log's
//              serial: laps_completed stops at 97), the last 8 laps newest first ("#11  1:23.456  +0.312", the gap to the session best) with a
//              badge: SESSION BEST, or the reason a lap is not counted in amber (FROM THE GATE, RESET, OFF TRACK, CUT, WRONG WAY, MAP SCALE, TOO
//              SHORT, MARKER), then SESSION BEST.
//   BANNER     top centre for 4 s (Banner): LAP NOT COUNTED · <REASON> when a flying lap that is not counted was faster than the session best.
// Hidden while a page is up (pause, results, the replay viewer), with the HUD off (H) and while the game is paused. Refreshed 10 times a second.
using System;
using System.Collections.Generic;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Practice
{
    [DisallowMultipleComponent]
    public sealed class RxPracticeHud : MonoBehaviour
    {
        public const int ListedLaps = 8;

        static RxPracticeHud s_current;
        OriginalWorldRuntime rt;
        GameObject host; VisualElement root, panel; Label title, best, banner; HudRows rows;
        float refreshAt, bannerUntil; float placedTop = -1f;
        Reflex.Port.Hud.ReflexModernHud modern;

        /// A banner at the top centre for `seconds` (the last one wins).
        public static void Banner(string text, Color color, float seconds = 4f)
        {
            var h = s_current; if (!h || h.banner == null) return;
            h.banner.text = text; h.banner.style.color = color; h.bannerUntil = Time.unscaledTime + seconds;
        }

        void Awake() { s_current = this; }

        void Start()
        {
            rt = GetComponent<OriginalWorldRuntime>();
            host = new GameObject("Practice HUD"); host.transform.SetParent(transform, false);
            root = OuUi.CreateDocument(host, 34); OuTheme.Apply(root); root.pickingMode = PickingMode.Ignore;
            panel = HudPanelUi.Panel(root, 300, 360);
            panel.name = "rx-practice-laps";
            panel.style.position = Position.Absolute; panel.style.left = 18; panel.style.top = 70;
            title = HudPanelUi.Text(panel, "PRACTICE", 15, HudPanelUi.Amber, 2f); title.style.marginBottom = 4;
            rows = new HudRows(panel, 16);
            best = HudPanelUi.Text(panel, "", 14, HudPanelUi.Green, 1f); best.style.marginTop = 6;
            banner = HudPanelUi.Text(root, "", 26, HudPanelUi.Amber, 2f);
            banner.name = "rx-practice-banner";
            banner.style.position = Position.Absolute; banner.style.top = 110; banner.style.left = 0; banner.style.right = 0;
            banner.style.unityTextAlign = TextAnchor.MiddleCenter; banner.style.display = DisplayStyle.None;
            RxPractice.LapCompleted += OnLap;
            Refresh();
        }

        void OnDestroy()
        {
            RxPractice.LapCompleted -= OnLap;
            if (s_current == this) s_current = null;
            if (rt) RxPractice.WorldGone(rt);   // (the practice world is left: its lap list goes with it)
            if (host) Destroy(host);
        }

        void OnLap(RxLap lap)
        {
            if (lap.invalid != null && lap.invalid != RxPractice.Marker && lap.flying && lap.ms > 0)   // (a MARKER lap was told at the ride back: w-lb-markers)
            {   // only when it would have been the session best
                var sb = RxPractice.SessionBest;
                if (sb == null || lap.ms < sb.ms) Banner("LAP NOT COUNTED  ·  " + lap.invalid, HudPanelUi.Amber);
            }
            refreshAt = 0f;
        }

        void Update()
        {
            if (root == null) return;
            bool hide = Time.timeScale == 0f || rt && (rt.OwnMenuActive || rt.paused || !rt.OriginalHudVisible);
            root.style.display = hide ? DisplayStyle.None : DisplayStyle.Flex;
            if (hide) return;
            Place();
            banner.style.display = Time.unscaledTime < bannerUntil ? DisplayStyle.Flex : DisplayStyle.None;
            if (Time.unscaledTime < refreshAt) return;
            refreshAt = Time.unscaledTime + 0.1f;
            Refresh();
        }

        /// Under the MODERN HUD's left column while it shows one, else at the top left.
        void Place()
        {
            if (!modern && rt) modern = rt.GetComponent<Reflex.Port.Hud.ReflexModernHud>();
            float below = modern ? modern.LeftColumnBottomPx : 0f, top = 70f;
            if (below > 0f && panel.panel != null) top = Mathf.Max(top, RuntimePanelUtils.ScreenToPanel(panel.panel, new Vector2(0f, below)).y + 8f);
            if (Mathf.Abs(top - placedTop) > .5f) { placedTop = top; panel.style.top = top; }
        }

        void Refresh()
        {
            var laps = RxPractice.Session;
            var t = RxPractice.Summary();
            float clock = rt ? rt.Race.lap_time : 0f;
            title.text = "PRACTICE  ·  LAP " + t.lap + "  ·  " + (RxPractice.OutLap ? "OUT LAP" : Clock(clock));   // (w-lb-markers: after a ride back to the marker)
            var list = new List<HudRow>();
            for (int i = laps.Length - 1; i >= 0 && list.Count < ListedLaps; i--) list.Add(Row(laps[i], t.best));
            if (list.Count == 0) list.Add(new HudRow { name = "THE FIRST LAP RUNS FROM THE GATE", dim = true });
            rows.Set(list);
            best.text = t.best != null ? "SESSION BEST  " + RxPractice.LapText(t.best.ms) + "  ·  " + t.clean + " CLEAN OF " + t.timed : t.timed > 0 ? "NO CLEAN LAP YET  ·  " + t.timed + " TIMED" : "";
        }

        static HudRow Row(RxLap lap, RxLap sessionBest)
        {
            var r = new HudRow { rank = "#" + lap.serial, name = RxPractice.LapText(lap.ms) };
            if (sessionBest != null && lap != sessionBest && lap.invalid == null) r.mid = RxPractice.GapText(lap.ms - sessionBest.ms);
            if (lap.invalid != null) { r.tag = lap.invalid; r.tagColor = HudPanelUi.Amber; r.dim = true; }
            else if (lap == sessionBest) { r.tag = "SESSION BEST"; r.tagColor = HudPanelUi.Green; }
            return r;
        }

        static string Clock(float s) { if (!(s > 0f)) s = 0f; int m = (int)(s / 60f); return m + ":" + (s - m * 60f).ToString("00.0", System.Globalization.CultureInfo.InvariantCulture); }
    }
}
