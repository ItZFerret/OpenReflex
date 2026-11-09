// The shell's on-screen keyboard (OuKeyboard: a text box needs a keyboard, a pad has none; the dialog, its keys and its pad buttons are there).
// The garage's text rows open it when the pad's A uses them (OpenShell.GaragePages.cs); a keyboard and a mouse type into the box itself.
using System;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed partial class OpenShell
    {
        OuKeyboard osk;

        /// The keyboard is up (the screen's own cancel, tabs and stage keys are the keyboard's).
        public bool KeyboardOpen => osk != null;

        /// Opens the keyboard on start (OuKeyboard.Open over the shell's layer, navigation and hint bar).
        void OpenKeyboard(string title, string note, string start, int max, bool digits, bool spaces, VisualElement returnTo, Action<string> done, Action cancelled = null)
        {
            if (osk != null) CloseKeyboard(false);
            OuKeyboard k = null;
            k = OuKeyboard.Open(_layer, _nav, _hints, title, note, start, max, digits, spaces, returnTo, done, cancelled, () => { if (osk == k) osk = null; });
            if (k.IsOpen) osk = k;
        }

        /// The screen is being left or rebuilt: the keyboard goes with it (no callback; the screen's own Clear gives the navigation back).
        void ReleaseKeyboard() => osk?.Release();

        /// Closes the keyboard: keep = DONE (done gets the text), else B / CANCEL. The screen gets its buttons and the focus back.
        void CloseKeyboard(bool keep) => osk?.Close(keep);
    }
}
