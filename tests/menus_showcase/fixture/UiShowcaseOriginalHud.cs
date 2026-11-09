// TEST ONLY: the ORIGINAL (classic) HUD's event timer warning and stunt feedback overlay, drawn from the install's Freestyle layer
// (HudFreestyleSinglePlayer / SinglePlayerFMX.lyr) and driven through the HUD's own methods with synthetic values (no world, no core).
using System;
using System.Collections;
using System.Reflection;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseOriginalHud
    {
        const BindingFlags All = BindingFlags.Instance | BindingFlags.Static | BindingFlags.NonPublic | BindingFlags.Public;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() { UiShowcase.Register("hud-original-fmx", Fmx); }

        static IEnumerator Fmx()
        {
            UiShowcase.ShellVisible(false);
            var backHost = HudShowcase.Backdrop(5);   // under the classic HUD's canvas (sorting order 10)
            var host = new GameObject("original HUD gallery");
            var hud = host.AddComponent<OriginalHud>();
            hud.RequirePackageHud("HudFreestyleSinglePlayer");
            var T = typeof(OriginalHud);
            var tree = hud.Tree;
            var timer = (OriginalUiTree.Node)T.GetField("eventTimer", All).GetValue(hud);
            if (timer == null) throw new InvalidOperationException("the Freestyle layer has no EventTimerComponent");
            OriginalUiTree.SetVisible(timer, true);
            var timerText = T.GetMethod("TimerText", All);
            var warn = T.GetMethod("TickTimerWarning", All); var begin = T.GetMethod("StuntBegin", All); var clear = T.GetMethod("ClearStunts", All);
            var h = RxRaceHudState.Create(); h.valid = 1; h.activityType = 5; h.activityMode = 1; h.clockLimit = 120f;
            IEnumerator Run(float clock, int frames)
            {
                for (int i = 0; i < frames; i++)
                {
                    h.clock = clock + i / 60f;
                    float left = Mathf.Max(0f, h.clockLimit - h.clock);
                    timerText.Invoke(null, new object[] { timer, "Timer Dashboard Component Shadow", "Timer Dashboard Component", OriginalUi.Time(left), null });
                    warn.Invoke(hud, new object[] { h, left });
                    tree.Tick(1f / 60f);
                    yield return null;
                }
            }
            yield return Run(60f, 20);                       // far from the end: no warning
            yield return UiShowcase.Shot("hud-original-timer-quiet");
            foreach (var n in new[] { "NAC NAC", "TSUNAMI", "SUPERMAN SEAT GRAB + HEELCLICKER" }) { begin.Invoke(hud, new object[] { n }); yield return Run(60f, 8); }
            yield return Run(115.4f, 12);                    // inside the last 5 s: red and pulsing
            yield return UiShowcase.Shot("hud-original-warning-a");
            yield return Run(115.7f, 9);
            yield return UiShowcase.Shot("hud-original-warning-b");
            yield return Run(119.7f, 30);                    // the time reaches 0: disarmed, the text stays red
            yield return UiShowcase.Shot("hud-original-warning-end");
            clear.Invoke(hud, null);
            UnityEngine.Object.Destroy(host); UnityEngine.Object.Destroy(backHost);
            yield return UiShowcase.Frames(3);
        }
    }
}
