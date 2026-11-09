// OPTIONS > INTERFACE: how the client's menus look and read (ours), and the language of the game's own texts.
//   MENUS      UI SCALE (every menu page, 80% .. 130%: the panels' reference resolution), CONTROLLER GLYPHS (the button hints: AUTO follows
//              the device used last; KEYBOARD, XBOX, PLAYSTATION), REDUCE MOTION (the menus' transitions and the HUD's flourishes off)
//   LANGUAGE   the game's texts (its own string tables: event names, rules, the career's pages): the install's ENG and whatever other
//              Latin-script languages it carries; applies at the next start (the shell's type does not draw Japanese, Korean or Chinese)
using System;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Options
{
    public sealed partial class ReflexOptionsMenu
    {
        static readonly string[] LanguageNames = { "ENG:ENGLISH", "GER:DEUTSCH", "FRE:FRANÇAIS", "SPA:ESPAÑOL", "ITA:ITALIANO" };
        static string LanguageName(string code) { foreach (var n in LanguageNames) if (n.StartsWith(code + ":", StringComparison.Ordinal)) return n.Substring(4); return code; }

        VisualElement BuildInterface(VisualElement panel)
        {
            Section(panel, "MENUS");
            var first = Named(OptUi.Spinner("UI SCALE", () => ReflexSettings.UiScale + "%", d => { ReflexSettings.UiScale = ReflexSettings.UiScale + d * 10; ReflexSettings.Save(); }, panel), "ui-scale");
            Note(panel, "The size of every menu page (80% .. 130%). The layout keeps the screen's safe area, and a very wide screen keeps the pages in the middle. Ours.");
            Named(OptUi.Spinner("CONTROLLER GLYPHS", () => ReflexSettings.GlyphStyleNames[ReflexSettings.GlyphStyle], d =>
            {
                ReflexSettings.GlyphStyle = ((ReflexSettings.GlyphStyle + d) % 4 + 4) % 4; ReflexSettings.Save(); ShowOptions(_tab);
            }, panel), "glyph-style");
            Note(panel, "The buttons the hints and the buttons show: AUTO follows the keyboard or the pad you used last; or always the KEYBOARD, XBOX or PLAYSTATION names. Ours.");
            Named(OptUi.Toggle("REDUCE MOTION", () => ReflexSettings.ReduceMotion, v => { ReflexSettings.ReduceMotion = v; ReflexSettings.Save(); ShowOptions(_tab); }, panel), "reduce-motion");
            Note(panel, "Switches off the menus' transitions and the HUD's animated flourishes (the values and the colours stay). Ours.");

            Section(panel, "LANGUAGE");
            string[] codes; try { codes = ReflexUiData.AvailableLanguages(); } catch (Exception) { codes = new[] { "ENG" }; }
            Named(OptUi.Spinner("LANGUAGE", () =>
            {
                string chosen = ReflexSettings.Language;
                string show = string.IsNullOrEmpty(chosen) ? ReflexUiData.Shared.language : chosen;
                return LanguageName(show) + (string.IsNullOrEmpty(chosen) ? "  (GAME)" : "");
            }, d =>
            {
                var order = new[] { "" }.Concat(codes).ToArray();
                int i = Array.IndexOf(order, ReflexSettings.Language); if (i < 0) i = 0;
                ReflexSettings.Language = order[((i + d) % order.Length + order.Length) % order.Length]; ReflexSettings.Save();
            }, panel), "language");
            Note(panel, codes.Length > 1
                ? "The language of the game's texts: event names, rules, the career's pages and the HUD's words. GAME is the install's own. Applies at the next start."
                : "This install carries one language (English). The game's own options have no language row: the language comes from the installer's language.txt.");
            return first;
        }
    }
}
