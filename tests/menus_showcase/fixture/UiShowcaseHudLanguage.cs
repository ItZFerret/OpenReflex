// TEST ONLY: the race HUD in the install's other languages (the bug report "race timer and lap freezing", 2026-11-05: a German player's
// timer and lap counter stopped at the first trick). The language packages other than ENG name their string tables "<table>.<LANG>"
// (InGame.GER: MXRaven_Stunts.GER); ReflexUiData looked for the bare name, so Strings("InGame", "MXRaven_Stunts") threw from
// ReflexHudModel.Fill every HUD frame and ReflexModernHud never reached view.Apply. Page (muted):
//   hudlang   every selectable language the install carries: the HUD / pause / front end tables are found (Has) and read (Strings);
//             German's trick names are German; the HUD model's trick line (ReflexHudModel.StuntLine) in German, without an exception;
//             the modern HUD drawn with that German line (hudlang-ger.png).
// Log lines "[hudlang-test] ..." (PASS / FAIL).
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.Hud;
using Reflex.Port.Options;
using Reflex.Port.OriginalWorld;
using UnityEngine;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseHudLanguage
    {
        const string Tag = "[hudlang-test] ";
        static int fails;
        static void Check(bool ok, string what) { Debug.Log(Tag + (ok ? "PASS " : "FAIL ") + what); if (!ok) fails++; }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() { UiShowcase.Register("hudlang", Page); }

        sealed class Mute : MonoBehaviour { void LateUpdate() { if (AudioListener.volume != 0f) AudioListener.volume = 0f; } }   // test players are muted

        // the tables the HUD, the pause menu, the post race and the front end read (each language package carries them, bare or ".<LANG>")
        static readonly (string package, string table)[] Tables =
        {
            ("InGame", "MXRaven_Default_Strings"), ("InGame", "MXRaven_Stunts"),
            ("Shared", "MXRaven_Default_Strings"), ("Shared", "MXRaven_Highlights"), ("Shared", "MXTables_Default_StoreProducts"),
            ("FrontEnd", "MXRaven_Default_Strings"), ("FrontEnd", "MXRaven_Stunts"),
            ("FrontEndIntro", "MXRaven_Intro"), ("HudNationalSinglePlayer", "MXRaven_Default_Strings"),
        };

        static FieldInfo StuntNamesField => typeof(ReflexHudModel).GetField("s_stuntNames", BindingFlags.Static | BindingFlags.NonPublic);
        static string StuntLine(string trick, string stunt)
            => (string)typeof(ReflexHudModel).GetMethod("StuntLine", BindingFlags.Static | BindingFlags.NonPublic).Invoke(null, new object[] { trick, stunt });

        static void UseLanguage(string lang)
        {
            ReflexSettings.Language = lang;
            OriginalUiAssets.Reset();
            StuntNamesField.SetValue(null, null);
        }

        static IEnumerator Page()
        {
            fails = 0;
            new GameObject("hudlang mute").AddComponent<Mute>();
            UiShowcase.ShellVisible(false);

            var langs = ReflexUiData.AvailableLanguages();
            Debug.Log(Tag + "languages: " + string.Join(", ", langs));
            Check(langs.Contains("GER"), "the install carries German (InGame.GER)");
            foreach (string lang in langs)
            {
                var d = new ReflexUiData(lang);
                foreach (var (package, table) in Tables)
                {
                    string at = $"{lang} {d.Pkg(package)}/{table}";
                    bool has = d.Has(d.Pkg(package), "localiz", table);
                    int count = -1; string err = null;
                    try { count = d.Strings(package, table).Count; } catch (Exception e) { err = e.Message; }
                    Check(has && count > 0, at + $": found {has}, {count} strings" + (err != null ? " (" + err + ")" : ""));
                }
            }

            // the German HUD strings are German (the ".GER" table, not the English one again); the original keeps the trick names English in
            // every language (MXRaven_Stunts.GER = the ENG names)
            var engHud = new ReflexUiData("ENG").Strings("InGame", "MXRaven_Default_Strings");
            var gerHud = new ReflexUiData("GER").Strings("InGame", "MXRaven_Default_Strings");
            var differ = gerHud.Where(kv => !string.IsNullOrEmpty(kv.Value) && engHud.TryGetValue(kv.Key, out var e) && e != kv.Value).Select(kv => kv.Key).ToList();
            Check(differ.Count > 100, $"the German HUD strings differ from the English ones ({differ.Count} of {gerHud.Count})");
            string sk = engHud.ContainsKey("STR_LAP") ? "STR_LAP" : differ.FirstOrDefault() ?? "";
            Debug.Log(Tag + $"sample {sk}: ENG '{(engHud.TryGetValue(sk, out var ev) ? ev : "")}' GER '{(gerHud.TryGetValue(sk, out var gv) ? gv : "")}'");
            var ger = new ReflexUiData("GER").Strings("InGame", "MXRaven_Stunts");
            var stuntKeys = ger.Where(kv => !string.IsNullOrEmpty(kv.Value)).Select(kv => kv.Key).ToList();
            string key = stuntKeys.FirstOrDefault() ?? "", key2 = stuntKeys.Skip(7).FirstOrDefault() ?? key;

            // the HUD model's trick line in German: the path that threw every frame in the report
            string prevLang = ReflexSettings.Language;
            UseLanguage("GER");
            Check(OriginalUiAssets.Data.language == "GER", "OPTIONS LANGUAGE German: the HUD's data is German (" + OriginalUiAssets.Data.language + ")");
            string line = null, lineErr = null;
            try { line = StuntLine(key, key2); } catch (TargetInvocationException e) { lineErr = e.InnerException?.Message ?? e.Message; }
            string want = (ger.TryGetValue(key, out var a) && a.Length > 0 ? a : key) + " + " + (ger.TryGetValue(key2, out var b) && b.Length > 0 ? b : key2);
            Check(lineErr == null && line == want, $"ReflexHudModel.StuntLine in German: '{line}' (want '{want}')" + (lineErr != null ? " threw " + lineErr : ""));

            // the modern HUD with the German trick line, the lap counter and the clocks (the view ReflexModernHud drives in a race)
            var backdrop = HudShowcase.Backdrop();
            var view = ReflexHudView.Create(null, "HUD gallery hudlang", 30001);
            view.Options.scale = 1f; view.Options.opacity = 1f; view.Options.standings = true; view.Options.gauge = true; view.Options.reduceMotion = false; view.Options.metric = true;
            var m = new ReflexHudModel();
            m.mode = HudMode.Race; m.valid = true; m.inRace = true; m.racing = true; m.state = 2; m.haveHud = true; m.onTrack = true;
            m.laps = 3; m.lap = 2; m.lapsCompleted = 1; m.place = 3; m.riders = 12; m.speedMph = 38f; m.gear = 3;
            m.lapTime = 41.37f; m.shownLapTime = 41.37f; m.lastLap = 63.02f; m.bestLap = 63.02f; m.hasBest = true; m.raceTime = 104.39f;
            m.DebugStunt(line ?? want);
            view.Visible = true;
            for (float t0 = Time.unscaledTime; Time.unscaledTime < t0 + 1f; ) { view.Apply(m, Time.unscaledDeltaTime, 2); yield return null; }
            view.Apply(m, 0.0001f, 2);
            yield return UiShowcase.Shot("hudlang-ger");
            view.Destroy();
            UnityEngine.Object.Destroy(backdrop);

            UseLanguage(prevLang);
            Debug.Log(Tag + (fails == 0 ? "ALL PASS" : fails + " FAILED"));
        }
    }
}
