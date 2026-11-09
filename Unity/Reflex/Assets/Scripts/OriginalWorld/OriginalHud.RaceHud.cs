using System;
using System.Collections.Generic;
using System.Globalization;
using Reflex.Port.RxCore;
using UnityEngine;

namespace Reflex.Port.OriginalWorld
{
    // The HUD components that the original drives from typed events and activity fields, from the core's copies (rx_session_race_hud,
    // core/include/rx_race_hud.h): the layers' own nodes, strings (the install's localization table) and animations; nothing is computed
    // here that the core's state does not say.
    //   PlayerEliminatedComponent  (National layer, Eliminator) MXUIPlayerEliminatedComponentStub 00605de0: for 3 s (real time) after
    //                              "Player Eliminated" the text "<next>\n<STR_ELIMINATED>\n<driver>!" (the leader's name, the word, the
    //                              eliminated rider's name)
    //   Outrunner Gate Indicator   (Outrun layer) MXUIOutrunnerGateCounter 00605190: "<STR_GATE> number/total" from the first gate on
    //   EventTimerComponent        (Freestyle and Section Race layers) MXUITimerComponent 005f7140: the time left (the clock when the
    //                              activity counts up); in Freestyle the last 5 s (the activity raises "FreestyleEnd" each frame from
    //                              Duration - 5 s, 00562bd0, which arms the component) the layer's "WarningTime" animation (0.53 s: the
    //                              component grows to 120 % and darkens and back, 15 keys at 30 fps) plays again each time it ends and the
    //                              time text is red (0xffcc3333), until the time shown is 0 (vf47) or the event restarts
    //   StuntFeedbackComponent     (every race HUD, Free Ride, the challenges, Freestyle) MXUIStuntFeedbackOverlay 00602cb0: "StuntBegin" /
    //                              "PhysicsStuntBegin" of the local player put the name of his trick (and physics stunt: "<trick> + <stunt>",
    //                              MXRaven_Stunts) into the newest of three slots, the older two move up, "RotateStunts" plays and the
    //                              component shows; 3 s after the last begin (its UI clock) or at "Restart" it hides and empties the slots
    //   Timer Dashboard Container / Best Section Container / SectionLeadersContainer (Section Race layer) MXUISectionTimeComponent
    //                              00606cf0, MXUIBestSectionTimeComponent 006055b0, MXUISectionLeadersContainer 00606970: the split time
    //                              of the section the rider is in, that section's best time, and "#n <name>" per section once it has a leader
    //   FMXJudgesFeedback / Judges Panel (Freestyle layer) MXUIFMXFeedbackComponent 00607540 (four judge columns, lit bars) and
    //                              MXUIFMXFinalScoreCard 00603b10 (the five ratings at the wind down, "%2.2f", the In animation and the
    //                              ShowJudges UI sound); the layer's "ScoreCard" (MXUIStuntJudgeComponent) hides itself and is never shown.
    public sealed partial class OriginalHud
    {
        OriginalUiTree.Node playerEliminated, gateCounter, eventTimer, sectionTime, bestSectionTime, sectionLeaders, judgesFeedback, judgesPanel;
        // the event timer's WarningTime flash and the stunt feedback overlay (above)
        OriginalUiTree.Node timerFore, stuntFeedback; readonly OriginalUiTree.Node[] stuntSlot = new OriginalUiTree.Node[3];
        Color timerForeColor = Color.white; bool timerArmed, timerRed; uint timerRestarts;
        readonly string[] stuntBuffer = { "", "", "" }; float stuntClock; bool stuntShown, stuntSynced; uint stuntSeen, stuntRestarts;
        static Dictionary<string, string> stuntNames;
        const float StuntSeconds = 3f;      // 006032c0: the overlay hides once its clock passes 3 s
        const float TimerWarningSeconds = 5f;   // 00562bd0: "FreestyleEnd" from Duration - 5 s
        readonly OriginalUiTree.Node[] leaderShadow = new OriginalUiTree.Node[5], leaderLabel = new OriginalUiTree.Node[5];
        readonly OriginalUiTree.Node[] judgeScore = new OriginalUiTree.Node[5];
        readonly OriginalUiTree.Node[,] judgeBar = new OriginalUiTree.Node[4, 10];
        static readonly string[] SectionOrdinal = { "First", "Second", "Third", "Fourth", "Fifth" };
        static readonly string[] JudgeColumn = { "Overall Judge", "Difficulty Judge", "Execution Judge", "Flow Judge", "Variety Judge" };
        const float EliminatedSeconds = 3f;   // 00605ec0: GetTickCount() - the event's time > 3000
        bool eliminatedSynced, finalSynced; uint eliminatedSeen, finalSeen; float eliminatedAt = -1f; string eliminatedText = "";

        static OriginalUiTree.Node Descendant(OriginalUiTree.Node n, string name)
        {
            if (n == null) return null;
            foreach (var c in n.children) { if (c.name == name) return c; var d = Descendant(c, name); if (d != null) return d; }
            return null;
        }

        // The nodes of the layer that has them (a layer without a component leaves its node null and nothing is drawn for it).
        void BindRaceHud()
        {
            playerEliminated = tree.Find(R + "PlayerEliminatedComponent"); gateCounter = tree.Find(R + "Outrunner Gate Indicator");
            eventTimer = tree.Find(R + "EventTimerComponent"); sectionTime = tree.Find(R + "Timer Dashboard Container");
            bestSectionTime = tree.Find(R + "Best Section Container"); sectionLeaders = tree.Find(R + "SectionLeadersContainer");
            judgesFeedback = tree.Find(R + "FMXJudgesFeedback"); judgesPanel = tree.Find(R + "Judges Panel");
            if (eventTimer != null)
            {
                eventTimer.tintChildren = true;   // WarningTime animates the container's colour (the text under it)
                timerFore = tree.Child(eventTimer, "Timer Dashboard Component");
                if (timerFore != null) timerForeColor = timerFore.baseColor;
            }
            stuntFeedback = tree.Find(R + "StuntFeedbackComponent");
            for (int i = 0; i < 3; i++) { stuntSlot[i] = tree.Child(stuntFeedback, "StuntSlot" + (i + 1)); OriginalUiTree.SetText(stuntSlot[i], ""); }
            OriginalUiTree.SetVisible(stuntFeedback, false);   // vf2_o100: cleared and hidden at the start
            for (int i = 0; i < 5; i++)
            {
                var row = tree.Child(sectionLeaders, SectionOrdinal[i] + "SectionContainer");
                leaderShadow[i] = tree.Child(row, SectionOrdinal[i] + "SectionShadow"); leaderLabel[i] = tree.Child(row, SectionOrdinal[i] + "SectionLabel");
                OriginalUiTree.SetText(leaderShadow[i], ""); OriginalUiTree.SetText(leaderLabel[i], "");   // 00606b70: the labels start empty
                judgeScore[i] = tree.Child(tree.Child(judgesPanel, JudgeColumn[i]), "Score");
            }
            for (int j = 0; j < 4; j++)
            {
                var judge = tree.Child(judgesFeedback, "Judge" + (j + 1));
                for (int k = 0; k < 10; k++) { judgeBar[j, k] = Descendant(judge, "Bar" + (k + 1).ToString("00")); OriginalUiTree.SetVisible(judgeBar[j, k], false); }
                // 00607370: judge j shows its number j + 1 under the bars
                OriginalUiTree.SetText(Descendant(judge, "JudgeNumber"), (j + 1).ToString(CultureInfo.InvariantCulture));
                OriginalUiTree.SetText(Descendant(judge, "JudgeNumberShadow"), (j + 1).ToString(CultureInfo.InvariantCulture));
            }
        }

        // true once when a counter of the core's events moved up (a drop means a new activity: the new count is the baseline)
        static bool Rose(ref bool synced, ref uint seen, uint now)
        {
            if (!synced) { synced = true; seen = now; return false; }
            bool rose = now > seen; seen = now; return rose;
        }

        static string Localized(string key) => OriginalUiAssets.Data.Strings().TryGetValue(key, out var s) ? s : key;

        void TickRaceHud(OriginalWorldRuntime runtime)
        {
            bool have = runtime && runtime.HasRaceHud;
            var h = have ? runtime.RaceHud : default;
            TickStunts(runtime);
            // "Player Eliminated": the message shows for 3 s of real time from the event
            if (playerEliminated != null)
            {
                if (have && Rose(ref eliminatedSynced, ref eliminatedSeen, h.eliminatedEvents))
                {
                    eliminatedText = h.EliminatorName + "\n" + Localized("STR_ELIMINATED") + "\n" + h.EliminatedName + "!";
                    eliminatedAt = Time.unscaledTime;
                }
                bool up = eliminatedAt >= 0f && Time.unscaledTime - eliminatedAt <= EliminatedSeconds;
                OriginalUiTree.SetVisible(playerEliminated, up);
                if (up) OriginalUiTree.SetText(playerEliminated, eliminatedText);
            }
            // the Outrun gate counter: blank until the first gate that gives time
            bool gate = have && h.activityMode == 8 && h.gateEvents > 0;
            OriginalUiTree.SetVisible(gateCounter, gate);
            if (gate) OriginalUiTree.SetText(gateCounter, Localized("STR_GATE") + " " + h.gateNumber + "/" + h.gateTotal);
            // the event timer: the time left, or the clock of an activity that counts up
            bool timed = have && (h.activityType == 5 || h.activityType == 12);
            OriginalUiTree.SetVisible(eventTimer, timed);
            if (timed)
            {
                float t = h.activityType == 22 || h.clockCountsUp == 0 ? Mathf.Max(0f, h.clockLimit - h.clock) : h.clock;
                TimerText(eventTimer, "Timer Dashboard Component Shadow", "Timer Dashboard Component", OriginalUi.Time(t));
                TickTimerWarning(h, t);
            }
            // Section Race: split time, section best and the section leaders
            bool section = have && h.activityType == 12;
            OriginalUiTree.SetVisible(sectionTime, section); OriginalUiTree.SetVisible(bestSectionTime, section); OriginalUiTree.SetVisible(sectionLeaders, section);
            if (section)
            {
                TimerText(sectionTime, "Timer Dashboard Component Shadow", "Timer Dashboard Component", OriginalUi.Time(h.sectionTime));
                TimerText(bestSectionTime, "Timer Dashboard Component Shadow", "Timer Dashboard Component", OriginalUi.Time(h.sectionBest));
                string pound = Localized("STR_POUND_SIGN");
                for (int i = 0; i < 5; i++)
                {
                    string label = (h.leaderSet & (1u << i)) != 0 ? pound + (i + 1) + " " + h.LeaderName(i) : "";   // "%s%d %s" / "%s%d "
                    OriginalUiTree.SetText(leaderShadow[i], label); OriginalUiTree.SetText(leaderLabel[i], label);
                }
            }
            // Freestyle: the judges' lit bars and the final score card
            bool fmx = have && h.activityType == 5;
            OriginalUiTree.SetVisible(judgesFeedback, fmx);
            if (fmx)
                for (int j = 0; j < 4; j++)
                    for (int k = 0; k < 10; k++) OriginalUiTree.SetVisible(judgeBar[j, k], k < h.JudgeBars(j));
            OriginalUiTree.SetVisible(judgesPanel, fmx && h.finalShown != 0);
            if (fmx && Rose(ref finalSynced, ref finalSeen, h.finalEvents))
            {
                for (int i = 0; i < 5; i++) OriginalUiTree.SetText(judgeScore[i], h.FinalScore(i).ToString("0.00", CultureInfo.InvariantCulture));   // "%2.2f"
                tree.Play("In", judgesPanel);
                UiSound(this, "play", "ShowJudges");
            }
        }

        // MXUITimerComponent::vf11's warning (005f7140): armed by "FreestyleEnd" (the Freestyle activity raises it each frame from Duration - 5 s while
        // its mode is not 0 / 2 / 3, 00562bd0), disarmed when the time shown is 0 or less (vf47); while armed the "WarningTime" animation plays again
        // whenever it is not playing, the time text turning red. A restart of the event puts it back (the component's layer starts again).
        void TickTimerWarning(RxRaceHudState h, float shown)
        {
            if (h.restartEvents != timerRestarts)
            {
                timerRestarts = h.restartEvents;
                timerArmed = false;
                if (timerRed) { timerRed = false; OriginalUiTree.SetColor(timerFore, timerForeColor); tree.Finish("WarningTime", eventTimer); }
            }
            bool raised = h.activityType == 5 && h.activityMode != 0 && h.activityMode != 2 && h.activityMode != 3 && h.clock >= h.clockLimit - TimerWarningSeconds;
            if (raised) timerArmed = true;
            if (shown <= 0f) timerArmed = false;
            if (!timerArmed || eventTimer == null) return;
            if (!tree.IsPlaying("WarningTime", eventTimer))
            {
                tree.Play("WarningTime", eventTimer);
                OriginalUiTree.SetColor(timerFore, new Color(0xcc / 255f, 0x33 / 255f, 0x33 / 255f, 1f));   // 0xffcc3333
                timerRed = true;
            }
        }

        // MXUIStuntFeedbackOverlay (the stunt names): see the header
        void TickStunts(OriginalWorldRuntime runtime)
        {
            if (stuntFeedback == null) return;
            if (runtime && runtime.HasStuntHud)
            {
                var s = runtime.StuntHud;
                if (!stuntSynced || s.events < stuntSeen) { stuntSynced = true; stuntSeen = s.events; stuntRestarts = s.restartEvents; ClearStunts(); }   // a new activity: the baseline
                else
                {
                    if (s.restartEvents != stuntRestarts) { stuntRestarts = s.restartEvents; ClearStunts(); }   // "Restart": hidden and empty
                    int fresh = (int)Mathf.Min(s.events - stuntSeen, s.count);
                    for (int k = fresh - 1; k >= 0; k--) StuntBegin(StuntText(s.Trick(k), s.Stunt(k)));       // oldest first, as the events came
                    stuntSeen = s.events;
                }
            }
            if (stuntShown)
            {   // 006032c0 vf11: past 3 s the overlay hides and empties; the clock runs while it is shown
                if (stuntClock > StuntSeconds) ClearStunts();
                stuntClock += Time.unscaledDeltaTime;
            }
        }

        // 006033d0: "<trick>", "<stunt>" or "<trick> + <stunt>", each name the LocString key looked up in MXRaven_Stunts
        static string StuntText(string trick, string stunt)
        {
            string Name(string key)
            {
                if (key.Length == 0) return null;
                stuntNames ??= OriginalUiAssets.Data.Strings("InGame", "MXRaven_Stunts");
                return stuntNames.TryGetValue(key, out var v) && !string.IsNullOrEmpty(v) ? v : key;
            }
            string a = Name(trick), b = Name(stunt);
            return a != null && b != null ? a + " + " + b : a ?? b ?? "";
        }

        // 00603130: the slots move up (3 <- 2 <- 1), the new name in the first, "RotateStunts", visible, the clock 0
        void StuntBegin(string text)
        {
            if (text.Length == 0) return;
            stuntBuffer[2] = stuntBuffer[1]; stuntBuffer[1] = stuntBuffer[0]; stuntBuffer[0] = text;
            for (int i = 0; i < 3; i++) OriginalUiTree.SetText(stuntSlot[i], stuntBuffer[i]);
            tree.Play("RotateStunts", stuntFeedback);
            OriginalUiTree.SetVisible(stuntFeedback, true);
            stuntShown = true; stuntClock = 0f;
        }

        // 00603340: hidden, the clock 0, the three slots empty
        void ClearStunts()
        {
            OriginalUiTree.SetVisible(stuntFeedback, false);
            stuntShown = false; stuntClock = 0f;
            for (int i = 0; i < 3; i++) { stuntBuffer[i] = ""; OriginalUiTree.SetText(stuntSlot[i], ""); }
        }
    }
}
