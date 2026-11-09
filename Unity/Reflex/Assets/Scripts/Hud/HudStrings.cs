// The words of the MODERN HUD. Where the original's HUD draws a string of the install's localization table (InGame package,
// MXRaven_Default_Strings: STR_ELIMINATED, STR_GATE, STR_POUND_SIGN, STR_SECTIONS, STR_GO) the MODERN HUD draws the same
// string, looked up once; the English text below is only the fallback when the table cannot be read (a gallery without an
// install, a test). The other words (POSITION, LAP, BEST ...) are the client's own, as the first MODERN HUD already had them.
using System.Collections.Generic;
using Reflex.Port.OriginalWorld;

namespace Reflex.Port.Hud
{
    public static class HudStrings
    {
        static readonly Dictionary<string, string> cache = new Dictionary<string, string>(8, System.StringComparer.Ordinal);
        static bool tableFailed;

        /// The localized string of a table key, or the fallback.
        public static string Get(string key, string fallback)
        {
            if (cache.TryGetValue(key, out var s)) return s;
            s = fallback;
            if (!tableFailed)
            {
                try { if (OriginalUiAssets.Data.Strings().TryGetValue(key, out var t) && !string.IsNullOrEmpty(t)) s = t; }
                catch (System.Exception) { tableFailed = true; }
            }
            cache[key] = s;
            return s;
        }

        /// Forget the lookups (the language or the install changed).
        public static void Reset() { cache.Clear(); tableFailed = false; }

        public static string Eliminated => Get("STR_ELIMINATED", "Eliminated");
        public static string Gate => Get("STR_GATE", "GATE:");
        public static string Pound => Get("STR_POUND_SIGN", "#");
        public static string Sections => Get("STR_SECTIONS", "SECTIONS:");
        public static string Go => Get("STR_GO", "GO!");
    }
}
