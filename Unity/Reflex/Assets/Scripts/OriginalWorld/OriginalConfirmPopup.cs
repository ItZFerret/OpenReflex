using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UI;

namespace Reflex.Port.OriginalWorld
{
    // The game's two-button confirmation popups (Shared RestartEventConfirmationPopup.lyr / QuitEventConfirmationPopup.lyr): a
    // darkener, the box with a blue title bar, the message wrapped to 541 px and YES / NO (NO focused first, as the game's
    // two-button popups default to the safe answer). Left/Right move, Enter accepts, Esc closes; the mouse clicks a button.
    // Shared by the pause menu and the post-race menu.
    public sealed class OriginalConfirmPopup
    {
        public static readonly Color Focused = new Color(217 / 255f, 164 / 255f, 65 / 255f, 1f);
        static readonly Color TextFocused = new Color(14 / 255f, 15 / 255f, 15 / 255f, 1f), ButtonText = new Color(170 / 255f, 169 / 255f, 151 / 255f, 1f);

        readonly OriginalUiTree tree; readonly Dictionary<string, string> loc;
        readonly OriginalUiTree.Node restartPopup, quitPopup;
        OriginalUiTree.Node popup, buttonOne, buttonTwo;
        int focus; Action yes;

        public bool IsOpen => popup != null;

        /// restart / quit: the Describe() trees of the two popup layers (ReflexUiData.DescribeLayer("Shared", ...)).
        public OriginalConfirmPopup(OriginalUiTree tree, RectTransform root, Dictionary<string, string> loc, Dictionary<string, object> restart, Dictionary<string, object> quit)
        {
            this.tree = tree; this.loc = loc;
            if (restart != null) { restartPopup = tree.Build(restart, root, null); OriginalUiTree.SetVisible(restartPopup, false); }
            if (quit != null) { quitPopup = tree.Build(quit, root, null); OriginalUiTree.SetVisible(quitPopup, false); }
        }

        string Str(string key, string fallback) => loc != null && loc.TryGetValue(key, out var v) && !string.IsNullOrEmpty(v) ? v : fallback;

        /// challenge = an activated event (EventTypeActivated, the Free Ride challenges): RestartEventConfirmationPopup:Start / QuitEvent...
        /// then set STR_RESTARTCHALLENGEPOPUP_* / STR_QUITCHALLENGEPOPUP_* and play PopupShow; the accept plays Accept.
        public void ShowRestart(Action onYes, bool challenge = false) => Show(restartPopup,
            challenge ? Str("STR_RESTARTCHALLENGEPOPUP_TITLE", "RESTART CHALLENGE") : Str("STR_RESTARTEVENTPOPUP_TITLE", "RESTART EVENT"),
            challenge ? Str("STR_RESTARTCHALLENGEPOPUP_MESSAGE", "Are you sure you want to restart the challenge?  All progress will be lost.") : Str("STR_RESTARTEVENTPOPUP_MESSAGE", "Are you sure you want to restart the event?"), onYes, challenge);
        public void ShowQuit(Action onYes, bool challenge = false) => Show(quitPopup,
            challenge ? Str("STR_QUITCHALLENGEPOPUP_TITLE", "QUIT CHALLENGE") : Str("STR_QUITEVENTPOPUP_TITLE", "QUIT EVENT"),
            challenge ? Str("STR_QUITCHALLENGEPOPUP_MESSAGE", "Are you sure you want to quit the challenge?  All progress will be lost.") : Str("STR_QUITEVENTPOPUP_MESSAGE", "Are you sure you want to quit the event?"), onYes, challenge);
        bool sounds;

        /// Keyboard and gamepad while a popup is up (the caller returns after this): left / right (d-pad, stick) move, Enter / A accept, Esc / B close.
        public void HandleKeys(Keyboard kb) => HandleKeys(kb, OriginalPadInput.Read());
        public void HandleKeys(Keyboard kb, OriginalPad pad)
        {
            if (kb != null && (kb.leftArrowKey.wasPressedThisFrame || kb.rightArrowKey.wasPressedThisFrame) || pad.left || pad.right) Focus(1 - focus);
            if (kb != null && kb.enterKey.wasPressedThisFrame || pad.accept) Accept();
            else if (kb != null && kb.escapeKey.wasPressedThisFrame || pad.back) Close();
        }

        static OriginalUiTree.Node Deep(OriginalUiTree.Node n, string name)
        {
            if (n == null) return null;
            foreach (var c in n.children) { if (c.name == name) return c; var d = Deep(c, name); if (d != null) return d; }
            return null;
        }

        static void Clickable(OriginalUiTree.Node n, Action onClick)
        {
            if (n == null || !n.graphic) return;
            n.graphic.raycastTarget = true;
            var b = n.rect.gameObject.AddComponent<Button>(); b.transition = Selectable.Transition.None; b.onClick.AddListener(() => onClick());
        }

        void Show(OriginalUiTree.Node p, string title, string message, Action onYes, bool challenge)
        {
            if (p == null) { onYes(); return; }
            popup = p; yes = onYes; sounds = challenge;
            if (sounds) OriginalHud.UiSound(null, "play", "PopupShow");
            OriginalUiTree.SetVisible(p, true);
            var popupTitle = Deep(p, "TitleText"); var popupMessage = Deep(p, "MessageText");
            buttonOne = Deep(p, "ButtonOne"); buttonTwo = Deep(p, "ButtonTwo");
            OriginalUiTree.SetText(popupTitle, title);
            if (popupMessage != null && popupMessage.text)
            {
                var t = popupMessage.text;
                t.Text = t.font != null ? t.font.Wrap(message, t.textHeight, 541f) : message;
            }
            OriginalUiTree.SetText(Deep(buttonOne, "ButtonText"), Str("STR_YES", "YES"));
            OriginalUiTree.SetText(Deep(buttonTwo, "ButtonText"), Str("STR_NO", "NO"));
            if (Deep(buttonOne, "SelectionBacking") is OriginalUiTree.Node s1 && s1.rect.GetComponent<Button>() == null) Clickable(s1, () => { Focus(0); Accept(); });
            if (Deep(buttonTwo, "SelectionBacking") is OriginalUiTree.Node s2 && s2.rect.GetComponent<Button>() == null) Clickable(s2, () => { Focus(1); Accept(); });
            Focus(1);
        }

        void Focus(int i)
        {
            focus = i;
            foreach (var (b, k) in new[] { (buttonOne, 0), (buttonTwo, 1) })
            {
                bool f = k == focus;
                OriginalUiTree.SetColor(Deep(b, "SelectionBacking"), f ? Focused : new Color(1, 1, 1, 0));
                OriginalUiTree.SetColor(Deep(b, "ButtonText"), f ? TextFocused : ButtonText);
            }
        }

        void Accept() { var y = yes; bool ok = focus == 0; if (sounds) OriginalHud.UiSound(null, "play", "Accept"); Close(); if (ok) y?.Invoke(); }
        public void Close() { if (popup != null) OriginalUiTree.SetVisible(popup, false); popup = null; yes = null; }
    }
}
