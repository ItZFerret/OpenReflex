// The credits' data, in one easy-to-edit place: Resources/OpenFrontEnd/Credits.json (add an entry to "credits" for another name:
// { "name": ..., "role": ..., "note": ... }; "legal" is the notice under them). The same list is the fallback when the file is missing.
using System;
using System.Collections.Generic;
using UnityEngine;

namespace Reflex.Port.OpenFrontEnd
{
    public static class OpenCredits
    {
        [Serializable] public sealed class Entry { public string name, role, note; }
        [Serializable] sealed class File { public string title; public List<Entry> credits; public string legal; }

        public const string Resource = "OpenFrontEnd/Credits";
        const string FallbackLegal = "This project is not affiliated with, endorsed by or sponsored by THQ, Rainbow Studios or their successors. MX vs ATV Reflex and all related marks belong to their respective owners.";

        static File s_file;
        static File Data
        {
            get
            {
                if (s_file != null) return s_file;
                try
                {
                    var text = Resources.Load<TextAsset>(Resource);
                    if (text) s_file = JsonUtility.FromJson<File>(text.text);
                }
                catch (Exception e) { Debug.LogWarning("[Credits] " + Resource + ": " + e.Message); }
                if (s_file == null || s_file.credits == null || s_file.credits.Count == 0) s_file = Fallback();
                if (string.IsNullOrWhiteSpace(s_file.legal)) s_file.legal = FallbackLegal;
                return s_file;
            }
        }

        static File Fallback() => new File
        {
            title = "CREDITS", legal = FallbackLegal,
            credits = new List<Entry>
            {
                new Entry { name = "itswillyferret", role = "PROJECT LEAD", note = "Responsible for the entire project." },
                new Entry { name = "boneboy", role = "LOGO", note = "The OPEN REFLEX logo." },
                new Entry { name = "MX vs ATV Reflex", role = "THE ORIGINAL GAME", note = "By THQ / Rainbow Studios." },
            }
        };

        public static string Title => string.IsNullOrWhiteSpace(Data.title) ? "CREDITS" : Data.title;
        public static IReadOnlyList<Entry> Entries => Data.credits;
        public static string Legal => Data.legal;
    }
}
