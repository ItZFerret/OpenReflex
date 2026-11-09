// TEST ONLY: the ORIGINAL (classic) HUD's wreck-recovery arrows, drawn from the install's National layer ("Arrows": MXUIReflexComponent's four
// arrow nodes and its ReflexIn animation) and driven through the HUD's own method with SYNTHETIC rx_session_reflex_hud states (no world).
//   hud-original-reflex   the four arrows (up, right, down, left) at the start of ReflexIn and at the end of it, then a "WreckRecoveryFailed"
//                         (the first three arrows' colour (1, 0, 0, 0)); logs which layers of the install have an "Arrows" component
using System;
using System.Collections;
using System.Reflection;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using UnityEngine;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseOriginalReflex
    {
        const BindingFlags All = BindingFlags.Instance | BindingFlags.Static | BindingFlags.NonPublic | BindingFlags.Public;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register()
        {
            UiShowcase.Register("hud-original-reflex", Arrows);
            UiShowcase.Register("hud-original-reflex-layers", Layers);
            UiShowcase.Register("hud-original-reflex-dump", Dump);
        }

        // the Arrows component's nodes (layout, style, animation tracks) in the log, for reproducing it in the MODERN HUD
        static IEnumerator Dump()
        {
            UiShowcase.ShellVisible(false);
            var host = new GameObject("original HUD dump");
            var hud = host.AddComponent<OriginalHud>();
            hud.Build();
            var arrows = (OriginalUiTree.Node)typeof(OriginalHud).GetField("reflexArrows", All).GetValue(hud);
            var sb = new System.Text.StringBuilder();
            void Walk(OriginalUiTree.Node n, int depth)
            {
                sb.Append(new string(' ', depth * 2)).Append(n.name).Append(" cls=").Append(n.cls).Append(" ").Append(Json(n.json)).Append("\n");
                foreach (var kv in n.anims) sb.Append(new string(' ', depth * 2 + 1)).Append("ANIM ").Append(kv.Key).Append(" ").Append(Json(kv.Value)).Append("\n");
                foreach (var c in n.children) Walk(c, depth + 1);
            }
            Walk(arrows, 0);
            Debug.Log("[ui-showcase] arrows dump\n" + sb);
            UnityEngine.Object.Destroy(host);
            yield return UiShowcase.Frames(2);
        }

        static string Json(object o)
        {
            switch (o)
            {
                case null: return "null";
                case string s: return "\"" + s + "\"";
                case System.Collections.Generic.Dictionary<string, object> d:
                {
                    var sb = new System.Text.StringBuilder("{");
                    foreach (var kv in d) { if (kv.Key == "children") continue; if (sb.Length > 1) sb.Append(','); sb.Append(kv.Key).Append(':').Append(Json(kv.Value)); }
                    return sb.Append('}').ToString();
                }
                case System.Collections.IEnumerable l:
                {
                    var sb = new System.Text.StringBuilder("[");
                    foreach (var x in l) { if (sb.Length > 1) sb.Append(','); sb.Append(Json(x)); }
                    return sb.Append(']').ToString();
                }
                default: return System.Convert.ToString(o, System.Globalization.CultureInfo.InvariantCulture);
            }
        }

        static RxReflexHudState State(int dir, uint failAny = 0)
        {
            var r = RxReflexHudState.Create();
            r.valid = 1; r.shown = 1; r.direction = dir; r.threshold = 0.25f; r.timeMax = 0.9f; r.timeLeft = 0.6f; r.failAnyEvents = failAny;
            r.litMask = dir == RxReflexHudState.DirUp ? 1u : dir == RxReflexHudState.DirRight ? 2u : dir == RxReflexHudState.DirDown ? 4u : 8u;
            return r;
        }

        static IEnumerator Arrows()
        {
            UiShowcase.ShellVisible(false);
            var backHost = HudShowcase.Backdrop(5);   // under the classic HUD's canvas (sorting order 10)
            var host = new GameObject("original HUD gallery");
            var hud = host.AddComponent<OriginalHud>();
            hud.Build();
            var T = typeof(OriginalHud);
            var apply = T.GetMethod("ApplyReflexArrows", All);
            var arrows = (OriginalUiTree.Node)T.GetField("reflexArrows", All).GetValue(hud);
            if (arrows == null) throw new InvalidOperationException("the National layer has no Arrows component");
            var tree = hud.Tree;
            IEnumerator Run(RxReflexHudState s, int frames)
            {
                for (int i = 0; i < frames; i++) { apply.Invoke(hud, new object[] { true, s }); tree.Tick(1f / 60f); yield return null; }
            }
            // nothing: hidden
            apply.Invoke(hud, new object[] { false, default(RxReflexHudState) });
            yield return UiShowcase.Frames(3);
            yield return UiShowcase.Shot("hud-original-reflex-none");
            int[] dirs = { RxReflexHudState.DirUp, RxReflexHudState.DirRight, RxReflexHudState.DirDown, RxReflexHudState.DirLeft };
            string[] names = { "up", "right", "down", "left" };
            for (int k = 0; k < 4; k++)
            {
                var s = State(dirs[k]);
                yield return Run(s, 6);     // 0.1 s into ReflexIn
                yield return UiShowcase.Shot("hud-original-reflex-" + names[k] + "-in");
                yield return Run(s, 8);     // the end of ReflexIn (0.23 s)
                yield return UiShowcase.Shot("hud-original-reflex-" + names[k]);
            }
            // a fall of any rider: the arrows' colour (1, 0, 0, 0), the next ReflexIn brings them back
            var up = State(RxReflexHudState.DirUp, 0);
            yield return Run(up, 24);
            var failed = State(RxReflexHudState.DirUp, 1);
            apply.Invoke(hud, new object[] { true, failed });
            yield return UiShowcase.Frames(2);
            yield return UiShowcase.Shot("hud-original-reflex-failed");
            UnityEngine.Object.Destroy(host); UnityEngine.Object.Destroy(backHost);
            yield return UiShowcase.Frames(3);
        }

        // which layers of the install carry the "Arrows" component
        static IEnumerator Layers()
        {
            UiShowcase.ShellVisible(false);
            string[] routes = { "HudTerracrossSinglePlayer", "HudFreestyleSinglePlayer", "HudSectionRaceSinglePlayer", "HudOutrunSinglePlayer" };
            var results = new System.Text.StringBuilder();
            void Probe(string label, Action<OriginalHud> build)
            {
                var host = new GameObject("original HUD layer probe");
                var hud = host.AddComponent<OriginalHud>();
                try { build(hud); results.Append(label + "=" + (((OriginalUiTree.Node)typeof(OriginalHud).GetField("reflexArrows", All).GetValue(hud)) != null ? "Arrows" : "no") + " "); }
                catch (Exception e) { results.Append(label + "=ERR(" + e.Message + ") "); }
                UnityEngine.Object.Destroy(host);
            }
            Probe("National", h => h.Build());
            Probe("Supercross", h => h.RequireSupercrossHud());
            Probe("Powercross", h => h.RequirePowercrossHud());
            foreach (var r in routes) Probe(r, h => h.RequirePackageHud(r));
            Debug.Log("[ui-showcase] reflex layers: " + results);
            yield return UiShowcase.Frames(3);
        }
    }
}
