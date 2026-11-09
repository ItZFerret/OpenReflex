// TEST ONLY: the MODERN race HUD in the showcase. Each page builds ReflexHudView (the view ReflexModernHud drives in a race) on its own
// canvas over a procedural backdrop and feeds it a SYNTHETIC ReflexHudModel (no race is ridden, nothing is read from the game), lets the
// animations play for a moment on real time and takes a screenshot. Pages:
//   hud-race (lap race, final lap, camera toast)   hud-countdown   hud-warning (wrong way)   hud-penalty   hud-eliminator (+ the message)
//   hud-countdown-waypoint / hud-countdown-terracross (an activity's own countdown step, no start board: start_timer 0, step 2 / 3)
//   hud-countdown-overtake (an Overtake without a board: the step is hidden, as the original's 005fb830 does)
//   hud-overtake   hud-outrun   hud-section   hud-fmx   hud-fmx-final   hud-powercross   hud-waypoint   hud-freeride   hud-challenge
//   hud-challenge-time   hud-race-small (HUD SCALE 70, OPACITY 45)   hud-race-large (HUD SCALE 140, metric)
using System;
using System.Collections;
using Reflex.Port.Hud;
using UnityEngine;
using UnityEngine.UI;

namespace Reflex.Port.Showcase
{
    static class HudShowcase
    {
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register()
        {
            UiShowcase.Register("hud-race", () => Page("hud-race", Race));
            UiShowcase.Register("hud-countdown", () => Page("hud-countdown", Countdown));
            UiShowcase.Register("hud-countdown-waypoint", () => Page("hud-countdown-waypoint", m => StepCountdown(m, HudMode.Waypoint, 2)));
            UiShowcase.Register("hud-countdown-terracross", () => Page("hud-countdown-terracross", m => StepCountdown(m, HudMode.Race, 3)));
            UiShowcase.Register("hud-countdown-overtake", () => Page("hud-countdown-overtake", m => StepCountdown(m, HudMode.Overtake, 2)));
            UiShowcase.Register("hud-warning", () => Page("hud-warning", Warning));
            UiShowcase.Register("hud-penalty", () => Page("hud-penalty", Penalty));
            UiShowcase.Register("hud-eliminator", () => Page("hud-eliminator", Eliminator));
            UiShowcase.Register("hud-overtake", () => Page("hud-overtake", Overtake));
            UiShowcase.Register("hud-outrun", () => Page("hud-outrun", Outrun));
            UiShowcase.Register("hud-section", () => Page("hud-section", Section));
            UiShowcase.Register("hud-fmx", () => Page("hud-fmx", Fmx));
            UiShowcase.Register("hud-fmx-pulse", () => Page("hud-fmx-pulse", Fmx, 0.4f));   // another moment of the WarningTime pulse
            UiShowcase.Register("hud-fmx-final", () => Page("hud-fmx-final", FmxFinal, 2.2f));
            UiShowcase.Register("hud-powercross", () => Page("hud-powercross", Powercross));
            UiShowcase.Register("hud-waypoint", () => Page("hud-waypoint", Waypoint));
            UiShowcase.Register("hud-freeride", () => Page("hud-freeride", FreeRide));
            UiShowcase.Register("hud-challenge", () => Page("hud-challenge", ChallengeHeight));
            UiShowcase.Register("hud-challenge-time", () => Page("hud-challenge-time", ChallengeTime));
            UiShowcase.Register("hud-race-small", () => Page("hud-race-small", Race, 1.0f, o => { o.scale = 0.7f; o.opacity = 0.45f; }));
            UiShowcase.Register("hud-race-large", () => Page("hud-race-large", Race, 1.0f, o => { o.scale = 1.4f; o.metric = true; }));
        }

        // ------------------------------------------------------------------------------------------------ the page driver
        static IEnumerator Page(string name, Action<ReflexHudModel> fill, float settle = 1.0f, Action<HudOptions> options = null)
        {
            var backdrop = Backdrop();
            var view = ReflexHudView.Create(null, "HUD gallery " + name, 30001);
            view.Options.scale = 1f; view.Options.opacity = 1f; view.Options.standings = true; view.Options.gauge = true; view.Options.reduceMotion = false; view.Options.metric = false;
            options?.Invoke(view.Options);
            var m = new ReflexHudModel();
            fill(m);
            view.Visible = true;
            float t0 = Time.unscaledTime;
            while (Time.unscaledTime < t0 + settle) { view.Apply(m, Time.unscaledDeltaTime, 2); yield return null; }
            view.Apply(m, 0.0001f, 2);
            yield return UiShowcase.Shot(name);
            view.Destroy();
            UnityEngine.Object.Destroy(backdrop);
            ReflexHudKit.Scale = 1f; ReflexHudKit.PanelOpacity = 1f;
            yield return UiShowcase.Frames(3);
        }

        // a bright, busy backdrop (a dirt track under a pale sky): the worst case for white text
        internal static GameObject Backdrop(int order = 30000)
        {
            var go = new GameObject("HUD gallery backdrop", typeof(Canvas), typeof(RawImage));
            var c = go.GetComponent<Canvas>(); c.renderMode = RenderMode.ScreenSpaceOverlay; c.sortingOrder = order;
            const int W = 192, H = 108;
            var tex = new Texture2D(W, H, TextureFormat.RGB24, false) { filterMode = FilterMode.Bilinear, wrapMode = TextureWrapMode.Clamp, hideFlags = HideFlags.DontSave };
            var px = new Color32[W * H];
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++)
                {
                    float v = y / (float)(H - 1), u = x / (float)(W - 1);
                    Color col;
                    float horizon = 0.46f + 0.05f * Mathf.Sin(u * 9f) + 0.03f * Mathf.Sin(u * 23f);
                    if (v > horizon)
                    {
                        float t = (v - horizon) / (1f - horizon);
                        col = Color.Lerp(new Color(0.78f, 0.88f, 0.96f), new Color(0.50f, 0.70f, 0.90f), t);
                        // a cloud or two
                        col = Color.Lerp(col, Color.white, 0.55f * Mathf.Clamp01(Mathf.Sin(u * 14f + 1f) * Mathf.Sin(v * 21f) * 2f));
                    }
                    else
                    {
                        float t = v / horizon;   // 0 at the bottom of the frame
                        Color dirt = Color.Lerp(new Color(0.62f, 0.46f, 0.30f), new Color(0.80f, 0.64f, 0.44f), t);
                        float lane = Mathf.Abs(u - (0.5f + 0.18f * (1f - t) * Mathf.Sin(t * 5f)));
                        float track = Mathf.Clamp01(1f - lane / (0.05f + 0.34f * (1f - t)));
                        col = Color.Lerp(dirt, new Color(0.34f, 0.27f, 0.20f), track * 0.8f);
                        if (t > 0.78f) col = Color.Lerp(col, new Color(0.25f, 0.42f, 0.22f), (t - 0.78f) / 0.22f);   // far hills
                    }
                    float n = Mathf.PerlinNoise(x * 0.37f, y * 0.37f) * 0.10f - 0.05f;
                    col.r += n; col.g += n; col.b += n;
                    px[y * W + x] = col;
                }
            tex.SetPixels32(px); tex.Apply(false, true);
            var img = go.GetComponent<RawImage>(); img.texture = tex; img.raycastTarget = false;
            img.rectTransform.anchorMin = Vector2.zero; img.rectTransform.anchorMax = Vector2.one; img.rectTransform.offsetMin = img.rectTransform.offsetMax = Vector2.zero;
            return go;
        }

        // ------------------------------------------------------------------------------------------------ synthetic models
        static readonly string[] Names = { "J. Cooper", "A. Bouda", "M. Saito", "C. Harris", "R. Villopoto", "T. Dungey", "B. Tickle", "K. Roczen", "E. Tomac", "C. Reed", "J. Barcia", "Z. Osborne" };

        static void Field(ReflexHudModel m, int place, int riders, int laps)
        {
            m.standings.Clear();
            for (int i = 0; i < riders; i++)
            {
                bool you = i + 1 == place;
                m.standings.Add(new HudStanding
                {
                    position = i + 1, slot = i, laps = Math.Max(0, laps - 1), you = you, ai = !you, finished = false,
                    name = you ? "Player 1" : Names[i % Names.Length], number = (10 + i * 7 % 90).ToString(), gapText = you ? "---" : "L" + Math.Max(1, laps),
                    raceTime = 100f + i, bestLap = 70f + i * 0.3f,
                });
                if (you) m.youRow = i;
            }
            m.MarkStandings();
        }

        // a race in progress: place 3 of 12, lap 2 of 3
        static void Base(ReflexHudModel m, HudMode mode, int place = 3, int riders = 12, int laps = 3, int lap = 2)
        {
            m.mode = mode; m.valid = true; m.inRace = laps > 0; m.racing = true; m.state = 2; m.place = place; m.riders = riders; m.laps = laps; m.lap = lap; m.lapsCompleted = lap - 1;
            m.haveHud = true; m.onTrack = true; m.speedMph = 64f; m.gear = 3; m.heading = 52f;
            // the rev ring (w-rpmgauge): a 250F (idle 1,500, limiter 14,000, as the core reports it) at 12,400 rpm
            m.hasRpm = true; m.idleRpm = 1500f; m.maxRpm = 14000f; m.rpm = 12400f; m.rpmGauge = ReflexHudModel.RpmGauge(m.rpm, m.idleRpm, m.maxRpm);
            m.hasBest = true; m.bestLap = 72.34f; m.lastLap = 73.21f; m.lapTime = 41.27f; m.shownLapTime = 41.27f;
            m.timeBehind = 2.45f; m.timeAhead = 1.12f;
            Field(m, place, riders, lap);
        }

        static void Race(ReflexHudModel m)
        {
            Base(m, HudMode.Race, 3, 12, 3, 3);
            m.finalLap = true; m.finalLapUntil = Time.unscaledTime + 100f;
            m.cameraToast = "GAME PLAY"; m.cameraUntil = Time.unscaledTime + 100f;
        }

        static void Countdown(ReflexHudModel m)
        {
            Base(m, HudMode.Race, 7, 12, 3, 1);
            m.state = 1; m.racing = false; m.startTimer = 2.4f; m.lapTime = 0f; m.shownLapTime = 0f; m.hasBest = false; m.lastLap = 0f; m.speedMph = 0f; m.gear = 0;
            m.place = 1; m.haveHud = true;
        }

        // the countdown of an activity without a start board (rx_race_state.countdown_step; start_timer stays 0 there)
        static void StepCountdown(ReflexHudModel m, HudMode mode, int step)
        {
            Base(m, mode, 1, mode == HudMode.Waypoint ? 1 : 8, mode == HudMode.Waypoint ? 1 : 3, 1);
            m.state = 1; m.racing = false; m.startTimer = 0f; m.countdownStep = step; m.lapTime = 0f; m.shownLapTime = 0f; m.hasBest = false; m.lastLap = 0f; m.speedMph = 0f; m.gear = 0;
            m.place = 1; m.haveHud = true;
            if (mode == HudMode.Waypoint) { m.waypoint = true; m.hasWaypoint = true; m.gateCount = 9; m.gatesPassed = 0; }
        }

        static void Warning(ReflexHudModel m)
        {
            Base(m, HudMode.Race, 6, 12, 3, 2);
            m.wrongWay = true; m.onTrack = true;
        }

        static void Penalty(ReflexHudModel m)
        {
            Base(m, HudMode.Race, 5, 12, 3, 2);
            m.penaltyShown = true; m.penalty = 3f; m.holdingLastLap = true; m.shownLapTime = m.lastLap;
        }

        static void Eliminator(ReflexHudModel m)
        {
            Base(m, HudMode.Eliminator, 4, 12, 3, 1);
            m.ridersLeft = 8; m.ridersTotal = 12; m.laps = 3;
            m.eliminatedText = "A. Bouda\nEliminated\nPlayer 7!"; m.eliminatedUntil = Time.unscaledTime + 100f;
        }

        static void Overtake(ReflexHudModel m)
        {
            Base(m, HudMode.Overtake, 12, 12, 3, 1);
            m.overtakeShown = true; m.overtakeText = "7"; m.hasBest = false; m.lastLap = 0f; m.speedMph = 22f; m.gear = 1;
        }

        static void Outrun(ReflexHudModel m)
        {
            Base(m, HudMode.Outrun, 2, 8, 1, 1);
            m.gateShown = true; m.gateNumber = 3; m.gateTotal = 6; m.outrunTimerShown = true; m.outrunSectionTime = 14.62f;
        }

        static void Section(ReflexHudModel m)
        {
            Base(m, HudMode.SectionRace, 2, 10, 1, 1);
            m.sectionShown = true; m.sectionCount = 5; m.sectionCurrent = 2; m.sectionTime = 18.47f; m.sectionBest = 17.92f;
            m.timerShown = true; m.timerValue = 38.4f; m.timerLow = false;
            m.leaderSet = 0b00011; m.leaderNames[0] = "Melissa Saito"; m.leaderNames[1] = "Cece Harris";
        }

        static void Fmx(ReflexHudModel m)
        {
            m.mode = HudMode.Freestyle; m.valid = true; m.racing = true; m.state = 2; m.haveHud = true; m.onTrack = true; m.speedMph = 41f; m.gear = 2; m.heading = 200f;
            m.timerShown = true; m.timerValue = 4.2f; m.timerLow = true;   // the WarningTime flash: Freestyle's last 5 s
            m.judgeBars[0] = 8; m.judgeBars[1] = 5; m.judgeBars[2] = 6; m.judgeBars[3] = 3;
            m.stuntShown = true; m.stuntSerial = 3; m.stuntText[0] = "SUPERMAN SEAT GRAB + HEELCLICKER"; m.stuntText[1] = "TSUNAMI"; m.stuntText[2] = "NAC NAC";   // StuntFeedbackComponent
        }

        static void FmxFinal(ReflexHudModel m)
        {
            Fmx(m);
            m.timerShown = true; m.timerValue = 0f; m.timerLow = false; m.state = 3; m.racing = false; m.stuntShown = false;
            m.judgeBars[0] = 8; m.judgeBars[1] = 9; m.judgeBars[2] = 7; m.judgeBars[3] = 6;
            m.finalShown = true; m.finalSerial = 1;
            m.finalScores[0] = 8.14f; m.finalScores[1] = 9.00f; m.finalScores[2] = 6.76f; m.finalScores[3] = 7.27f; m.finalScores[4] = 4.50f;
        }

        static void Powercross(ReflexHudModel m)
        {
            Base(m, HudMode.Race, 4, 8, 3, 1);
            m.powercross = true; m.pcVisible = true; m.pcStep = 2; m.state = 1; m.racing = false; m.startTimer = 2f; m.hasBest = false; m.lastLap = 0f; m.lapTime = 0f; m.shownLapTime = 0f; m.speedMph = 0f; m.gear = 0;
        }

        static void Waypoint(ReflexHudModel m)
        {
            Base(m, HudMode.Waypoint, 2, 6, 1, 1);
            m.waypoint = true; m.hasWaypoint = true; m.waypointDistanceValid = true; m.gatesPassed = 4; m.gateCount = 9; m.routeDistance = 1380f; m.heading = 318f;
            m.hasBest = false; m.lastLap = 0f;
        }

        static void FreeRide(ReflexHudModel m)
        {
            m.mode = HudMode.FreeRide; m.valid = true; m.racing = true; m.state = 2; m.haveHud = true; m.onTrack = true; m.speedMph = 47f; m.gear = 2; m.heading = 137f;
            m.stuntShown = true; m.stuntSerial = 2; m.stuntText[0] = "WHIP"; m.stuntText[1] = "NO HANDER"; m.stuntText[2] = "";
        }

        static void ChallengeHeight(ReflexHudModel m)
        {
            FreeRide(m);
            m.mode = HudMode.Challenge; m.challengeValid = true; m.challengeKind = 1; m.challengeState = 2; m.challengeValueValid = true; m.challengeValue = 213f;
            m.challengeCriteria[0] = 150f; m.challengeCriteria[1] = 250f; m.challengeCriteria[2] = 400f;
            m.timerShown = true; m.timerValue = 72.0f; m.speedMph = 33f; m.gear = 1;
        }

        static void ChallengeTime(ReflexHudModel m)
        {
            FreeRide(m);
            m.mode = HudMode.Challenge; m.challengeValid = true; m.challengeKind = 6; m.challengeState = 2; m.challengeValueValid = true; m.challengeValue = 47.83f;
            m.challengeCriteria[0] = 70f; m.challengeCriteria[1] = 55f; m.challengeCriteria[2] = 45f;
        }
    }
}
