// The CREDITS screen (the hub's CREDITS, OPTIONS > SYSTEM > ABOUT): who made what, the original game, and the notice that this project is
// not affiliated with its owners. The names are OpenCredits' (Resources/OpenFrontEnd/Credits.json). Every card takes the focus, so up / down
// (the d-pad, the left stick, the mouse wheel) scroll the list; BACK / Esc / B returns to where it was opened from.
using System;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed partial class OpenShell
    {
        /// CREDITS. back = where Back / Esc goes (null: the hub).
        public void ShowCredits(Action back = null)
        {
            Clear(Screen.Credits, "credits");
            SetBackground(HubPicture()); MusicTier(2);
            _layer.AddToClassList("mxa-dim");
            OuTheme.Header(_layer, OpenCredits.Title, OpenReflex.Name);
            var page = OuUi.Div("ou-page", _layer);
            var side = OuUi.Div("ou-page-side", page);
            OuTheme.Logo(OuUi.Div("ou-credits-logo", side), 120);
            OuUi.Text("THE PEOPLE AND THE GAME BEHIND IT", "ou-hero-sub ou-label", side).style.marginTop = 24;
            Action exit = back ?? ShowMain;
            var done = OuUi.Button("BACK", "mxa-main-button", exit, OuUi.Div("ou-actions", side));
            done.name = "credits-back";
            var main = OuUi.Div("ou-page-main", page);
            var scroll = new ScrollView(ScrollViewMode.Vertical) { name = "credits-list" };
            scroll.AddToClassList("ou-credits"); scroll.horizontalScrollerVisibility = ScrollerVisibility.Hidden; main.Add(scroll);
            var cards = new System.Collections.Generic.List<VisualElement>();
            foreach (var entry in OpenCredits.Entries)
            {
                var card = OuUi.Item(OuUi.Div("ou-credit", scroll), null);
                OuUi.Text(entry.role ?? "", "ou-credit-role ou-label", card).enableRichText = false;
                OuUi.Text(entry.name ?? "", "ou-credit-name ou-display", card).enableRichText = false;
                if (!string.IsNullOrWhiteSpace(entry.note)) OuUi.Text(entry.note, "ou-credit-note", card).enableRichText = false;
                card.RegisterCallback<FocusInEvent>(_ => scroll.ScrollTo(card));
                cards.Add(card);
            }
            var legal = OuUi.Item(OuUi.Div("ou-legal", scroll), null);
            legal.name = "credits-legal";
            OuUi.Text(OpenCredits.Legal, "ou-legal-text", legal).enableRichText = false;
            legal.RegisterCallback<FocusInEvent>(_ => scroll.ScrollTo(legal));
            cards.Add(legal);
            OuWidgets.LinkColumn(cards);
            foreach (var c in cards) ((OuNavItem)c.userData).left = done;
            ((OuNavItem)done.userData).right = cards[0];
            OuMotion.Enter(scroll.contentContainer, 0.06f, 0.05f);
            _nav.OnCancel = () => { exit(); return true; };
            _nav.DefaultItem = () => done; _nav.Focus(done);
            Hints(true);
        }
    }
}
