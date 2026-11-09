// The hub's MOTOSKILLS (w-tutorials; the original's main menu MOTOSKILLS key, STR_TUTORIALS: MainMenu:OnTutorialKeypress): the six lessons with
// their state from the profile (PlayerRider.TutorialsCompleted: the lessons unlocked are that + 1, TutorialPauseMenu:Start), the focused lesson's
// card (its title, the pause menu's brief description and the PreTutorialPopup's instructions: STR_TUTORIAL_<n>_TITLE / _BRIEF_DESC /
// _DESCRIPTION) and START LESSON, which rides it in the compound (OpenTutorial.cs). The lessons' run ends on the MOTOSKILLS page again.
using System;
using Reflex.Port.Content;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed partial class OpenShell
    {
        int motoSkillsFocus;

        /// The hub's entry: the caption with the profile's progress.
        static string MotoSkillsCaption()
        {
            int done = OpenTutorial.Completed;
            return "MX TRAINING · SIX LESSONS · " + (done >= OpenTutorial.Lessons ? "ALL COMPLETED" : done + " / 6 COMPLETED");
        }

        /// MOTOSKILLS: the lesson list and the focused lesson's card.
        public void ShowMotoSkills() => ShowMotoSkills(-1);
        public void ShowMotoSkills(int focus)
        {
            Clear(Screen.MotoSkills, "motoskills");
            SetBackground(HubPicture()); MusicTier(2);
            _layer.AddToClassList("mxa-dim");
            int completed = OpenTutorial.Completed, available = Mathf.Min(completed + 1, OpenTutorial.Lessons);
            if (focus < 0) focus = Mathf.Min(completed, OpenTutorial.Lessons - 1);
            motoSkillsFocus = Mathf.Clamp(focus, 0, available - 1);
            OuTheme.Header(_layer, "MOTOSKILLS", OpenTutorial.S("STR_TUTORIAL_INTRO_TITLE", "MX TRAINING").ToUpperInvariant());
            var page = OuUi.Div("ou-page", _layer);
            var side = OuUi.Div("ou-page-side", page);
            var intro = OuUi.Text(OpenTutorial.S("STR_TUTORIAL_INTRO_DESCRIPTION", "Six lessons in the compound, from the Rider Reflex to freestyle tricks."), "ou-dialog-lead", side);
            intro.enableRichText = false; intro.style.whiteSpace = WhiteSpace.Normal; intro.style.maxWidth = 560; intro.style.marginBottom = 14; intro.style.fontSize = 20;
            var list = OuUi.Div("", side); list.name = "motoskills-list"; list.style.width = 600;
            var rows = new VisualElement[OpenTutorial.Lessons];
            var main = OuUi.Div("ou-page-main", page);
            var card = OuUi.Div("ou-card-panel ou-motoskills-card", main); card.name = "motoskills-card";
            var cs = card.style; cs.paddingLeft = 32; cs.paddingRight = 32; cs.paddingTop = 26; cs.paddingBottom = 28; cs.maxWidth = 860;
            var eyebrow = OuUi.Text("", "ou-eyebrow", card);
            var title = OuUi.Text("", "ou-continue-title ou-display", card); title.name = "motoskills-title"; title.enableRichText = false;
            var brief = OuUi.Text("", "ou-hero-sub ou-label", card); brief.enableRichText = false; brief.style.whiteSpace = WhiteSpace.Normal; brief.style.marginTop = 6;
            var actions = OuUi.Div("ou-button-row", card); actions.style.marginTop = 14; actions.style.marginBottom = 10;
            var scroll = new ScrollView(ScrollViewMode.Vertical) { name = "motoskills-instructions-scroll" };
            scroll.horizontalScrollerVisibility = ScrollerVisibility.Hidden; scroll.style.maxHeight = 420; card.Add(scroll);
            var how = OuUi.Text("", "ou-dialog-lead", scroll); how.name = "motoskills-instructions"; how.enableRichText = false;
            how.style.whiteSpace = WhiteSpace.Normal; how.style.fontSize = 20;
            VisualElement start = null;
            void Card(int n)
            {
                motoSkillsFocus = n;
                bool locked = n >= available, done = n < completed;
                eyebrow.text = "LESSON " + (n + 1) + (done ? "  ·  COMPLETED" : locked ? "  ·  LOCKED" : "  ·  NEW");
                title.text = OpenTutorial.Title(n).ToUpperInvariant();
                brief.text = OpenTutorial.Brief(n);
                how.text = locked ? "Complete lesson " + n + " (" + OpenTutorial.Title(n - 1) + ") to unlock this lesson." : OpenTutorial.Description(n);
                if (start != null) start.SetEnabled(!locked);
            }
            for (int i = 0; i < OpenTutorial.Lessons; i++)
            {
                int n = i; bool locked = i >= available, done = i < completed;
                var b = OuUi.Item(OuUi.Div("mxa-button mxa-row", list), () => { if (n < available) StartMotoSkills(n); else OuSound.Play("Locked"); });
                b.name = "motoskills-lesson-" + (i + 1);
                OuUi.Text((i + 1) + "   " + OpenTutorial.Title(i).ToUpperInvariant(), "mxa-row-label", b).enableRichText = false;
                OuUi.Text(done ? "COMPLETED" : locked ? "LOCKED" : "NEW", "mxa-value", OuUi.Div("mxa-row-value", b));
                if (locked) b.AddToClassList("ou-locked");
                b.RegisterCallback<FocusInEvent>(_ => Card(n));
                rows[i] = b;
            }
            start = OuUi.Button("START LESSON", "mxa-start ou-cta", () => { if (motoSkillsFocus < available) StartMotoSkills(motoSkillsFocus); }, actions);
            start.name = "motoskills-start";
            OuTheme.Backdrop(start);
            var back = OuUi.Button("BACK", "mxa-main-button", ShowHome, OuUi.Div("ou-actions", side)); back.name = "motoskills-back";
            back.parent.style.marginTop = 14;
            OuWidgets.LinkColumn(rows);
            foreach (var r in rows) ((OuNavItem)r.userData).right = start;
            ((OuNavItem)start.userData).left = rows[motoSkillsFocus];
            ((OuNavItem)rows[OpenTutorial.Lessons - 1].userData).down = back;
            ((OuNavItem)back.userData).up = rows[OpenTutorial.Lessons - 1];
            Card(motoSkillsFocus);
            OuMotion.Enter(list, 0.06f, 0.05f);
            _nav.OnCancel = () => { ShowHome(); return true; };
            _nav.DefaultItem = () => rows[motoSkillsFocus];
            _nav.Focus(rows[motoSkillsFocus]);
            _statusText.text = completed >= OpenTutorial.Lessons ? "MOTOSKILLS COMPLETED" : completed + " OF 6 LESSONS COMPLETED";
            Hints(true);
        }

        /// START LESSON: the lesson in the compound (the shell waits under the curtain).
        void StartMotoSkills(int lesson)
        {
            if (OpenTutorial.Current != null || OpenTestRide.Current != null) return;
            OuSound.Play("Accept");
            _nav.Suspended = () => true;
            _sounds?.music?.SetEnable(false);
            OpenTutorial.Begin(lesson);
        }

        /// The shell starts after the lessons: the MOTOSKILLS page again (once the menu world is there, as after a test ride). True when it took the start.
        bool TutorialReturn()
        {
            int focus = OpenTutorial.PendingReturn;
            if (focus < 0) return false;
            OpenTutorial.PendingReturn = -1;
            _heroWalked = true;
            Clear(Screen.Loading);
            _hints.Clear();
            float since = Time.unscaledTime;
            ScreenUpdate = () =>
            {
                if (MenuWorld == null && _world && _world.Status == OpenMenuWorld.State.Loading && Time.unscaledTime - since < 15f) return;
                ScreenUpdate = null;
                ReflexProfile.Open();   // the lessons' session settled the profile (PlayerRider.TutorialsCompleted): read it again
                ShowMotoSkills(focus);
                OuLayout.Apply(_layer, OuLayout.Kind.Layer);
                string why = OpenTutorial.FailedReason; OpenTutorial.FailedReason = null;
                if (!string.IsNullOrEmpty(why)) _statusText.text = "THE LESSON STOPPED: " + why.ToUpperInvariant();
                OpenTutorial.Returned();
                Debug.Log($"[MotoSkills] back on the MOTOSKILLS page: {OpenTutorial.Completed} completed, focus lesson {focus + 1}");
            };
            return true;
        }
    }
}
