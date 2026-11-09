// The original's popup pages in the open shell (the same words, the same buttons), over the current screen with the focus kept inside:
//   QuitConfirmationPopup        hub QUIT: STR_QUIT / STR_QUITGAMEPOPUP_MESSAGE / YES (focused, as the Lua's SetFocused) / NO; Back = NO
//   LockedPopup                  STR_LOCKEDPOPUP_TITLE / _MESSAGE / CONTINUE   (a locked series or challenge location was chosen)
//   InvalidRaceRequirements      STR_INVALID_VEHICLEREQ_TITLE / _MESSAGE / CONTINUE   (the equipped vehicle's type is not one of the event's class)
//   StartRacePopup               a career event row was chosen (any format but Moto / Heat): STR_START_EVENT_TITLE / _MESSAGE / YES (focused, as the Lua's
//                                SetFocused) / NO; Back = NO and the focus returns to the row; the event card above the buttons is the port's addition
//   RulesPopup                   the event type's rules (OwnCareerRulesText = RulesPopup.lua's RulesType table) / CONTINUE / CONTINUE, DO NOT
//                                SHOW AGAIN (the PlayerEvent "Rule_<RulesType>" flag, RulesPopup:OnToggleKeypress); Back does nothing
// All five are one dialog (OuWidgets.Dialog: the restyle's orange title bar), a modal for OuNav (d-pad / stick / keys / mouse), Back as the Lua says.
using System;
using System.Collections.Generic;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed partial class OpenShell
    {
        VisualElement _popup;
        OuNav.Place _popupPlace;
        Func<bool> _popupSavedCancel;
        StyleEnum<DisplayStyle> _popupHints;

        /// A popup is up (tests).
        public bool PopupOpen => _popup != null && _popup.panel != null;
        public VisualElement Popup => PopupOpen ? _popup : null;

        /// A modal card over the current screen (a popup, the vehicle list): the shell's hint bar steps aside, Back runs cancel (null: Back does
        /// nothing), the focus stays inside until ClosePopup. Returns the card (the dialog's content goes into it); `wide` is the card's width.
        public VisualElement BeginModal(string name, string kind, Action cancel, float wide = 0)
        {
            if (_popup != null) ClosePopup(false);
            OuSound.Play("PopupShow");
            _popupPlace = _nav.Remember();
            _popupSavedCancel = _nav.OnCancel;
            _popupHints = _hints.style.display; _hints.style.display = DisplayStyle.None;
            var root = OuUi.Div("ou-gdialog-root p2-popup", _layer); root.name = name;
            var card = OuWidgets.Dialog(root, kind);
            if (wide > 0) card.style.width = wide;
            _popup = root;
            _nav.OnCancel = () => { if (cancel != null) { ClosePopup(); cancel(); } return true; };
            return card;
        }

        /// A popup page over the current screen. cancel: Back's action once the popup is closed (null: Back does nothing, as RulesPopup);
        /// focus: the index of the button that starts focused; stack: the buttons one under the other (long labels).
        public VisualElement OpenPopup(string name, string title, string format, string message, Action cancel, int focus, bool stack, params (string label, string id, Action action)[] buttons)
        {
            var card = BeginModal(name, "p2", cancel, stack ? 1180 : 0);
            OuUi.Text((title ?? "").ToUpperInvariant(), "ou-dialog-title ou-display", card).enableRichText = false;
            if (!string.IsNullOrEmpty(format)) OuUi.Text(format.ToUpperInvariant(), "p2-popup-format", card).enableRichText = false;
            if (!string.IsNullOrEmpty(message))
            {
                var body = OuUi.Text(message, "ou-dialog-text p2-popup-text", card); body.enableRichText = false; body.name = name + "-text";
            }
            var row = OuUi.Div("p2-popup-buttons" + (stack ? " p2-popup-stack" : ""), card);
            var made = new List<VisualElement>();
            foreach (var b in buttons)
            {
                var action = b.action;
                var button = OuUi.Button(b.label.ToUpperInvariant(), "mxa-main-button p2-popup-button", () => { ClosePopup(); action?.Invoke(); }, row);
                button.name = b.id; made.Add(button);
            }
            if (stack) OuWidgets.LinkColumn(made); else OuWidgets.LinkRow(made);
            var hint = OuUi.Div("p2-popup-hints", card);
            OuUi.Hints(hint, cancel != null ? new[] { (OuUi.KeySubmit, "SELECT"), (OuUi.KeyCancel, "BACK") } : new[] { (OuUi.KeySubmit, "SELECT") });
            _nav.Modal = card;
            if (made.Count > 0) _nav.Focus(made[Mathf.Clamp(focus, 0, made.Count - 1)]);
            return card;
        }

        /// Closes the popup; the focus goes back to where it was.
        public void ClosePopup(bool restore = true)
        {
            if (_popup == null) return;
            var root = _popup; _popup = null;
            bool attached = root.panel != null;
            _nav.Modal = null;
            _nav.OnCancel = _popupSavedCancel; _popupSavedCancel = null;
            if (attached) _hints.style.display = _popupHints;
            if (_narrationOwner == RulesNarrationOwner) StopNarration();   // RulesPopup:End: App.StopUILocalizedSoundEvent
            root.RemoveFromHierarchy();
            if (restore && attached) _nav.Restore(_popupPlace, null);
        }

        // ------------------------------------------------------------------------------------------------ QuitConfirmationPopup

        /// The hub's QUIT: asks first (QuitConfirmationPopup: STR_QUIT, STR_QUITGAMEPOPUP_MESSAGE, YES / NO, YES focused).
        /// Tests: replaces the quit itself (the showcase must not end the player).
        public static Action QuitOverride;
        internal void ConfirmQuit()
        {
            OpenPopup("popup-quit", OpenContent.Str("STR_QUIT", "QUIT"),
                null, OpenContent.Str("STR_QUITGAMEPOPUP_MESSAGE", "Are you sure you want to quit your game? Any unsaved progress will be lost."),
                () => { }, 0, false,
                (OpenContent.Str("STR_YES", "YES"), "popup-yes", () => { if (QuitOverride != null) QuitOverride(); else Quit(); }), (OpenContent.Str("STR_NO", "NO"), "popup-no", null));
        }

        // ------------------------------------------------------------------------------------------------ LockedPopup / InvalidRaceRequirements

        /// LockedPopup: a locked series, location or challenge series was chosen.
        public void ShowLockedPopup(Action closed = null)
        {
            Action done = closed ?? (() => { });
            OpenPopup("popup-locked", OpenContent.Str("STR_LOCKEDPOPUP_TITLE", "LOCKED"), null,
                OpenContent.Str("STR_LOCKEDPOPUP_MESSAGE", "Play and win each MotoCareer Series to gain Garage credits and unlock new items!"),
                done, 0, false, (OpenContent.Str("STR_CONTINUE", "CONTINUE"), "popup-continue", closed));
        }

        /// InvalidRaceRequirements: the equipped vehicle is not one of the event's class.
        public void ShowInvalidVehiclePopup(Action closed = null)
        {
            Action done = closed ?? (() => { });
            OpenPopup("popup-invalid-vehicle", OpenContent.Str("STR_INVALID_VEHICLEREQ_TITLE", "INVALID VEHICLE"), null,
                OpenContent.Str("STR_INVALID_VEHICLEREQ_MESSAGE", "You do not have a valid vehicle equipped for this event. Equip a valid vehicle by using the Vehicle List or visiting your Garage."),
                done, 0, false, (OpenContent.Str("STR_CONTINUE", "CONTINUE"), "popup-continue", closed));
        }

        // ------------------------------------------------------------------------------------------------ StartRacePopup

        /// StartRacePopup (the career's Race and Ride Events, every format but Moto / Heat): "Are you sure you want to start the event?" YES / NO.
        /// The original shows only the question; the event card (name, place, format, length, vehicle class, the player's vehicle - the same data
        /// as the events page) is the port's addition so the pad player sees what starts. YES calls proceed once the popup is closed; NO and Back
        /// close it and the focus goes back to the event row (StartRacePopup:OnBackKeypress = TravelBack).
        public void ShowStartRacePopup(CareerEventChoice choice, Action proceed)
        {
            var entry = choice.Entry;
            var card = OpenPopup("popup-start-race", OpenContent.Str("STR_START_EVENT_TITLE", "START EVENT"), null,
                OpenContent.Str("STR_START_EVENT_MESSAGE", "Are you sure you want to start the event?"),
                () => { }, 0, false,
                (OpenContent.Str("STR_PURCHASE_POPUP_BUTTONONE", "YES"), "popup-start-yes", proceed),
                (OpenContent.Str("STR_PURCHASE_POPUP_BUTTONTWO", "NO"), "popup-start-no", null));
            var facts = new VisualElement { name = "popup-start-facts" };
            facts.AddToClassList("p2-startrace-facts"); facts.pickingMode = PickingMode.Ignore;
            OcUi.Fact(facts, "EVENT", OcRaceAndRide.EventTitle(entry).ToUpperInvariant()).AddToClassList("p2-startrace-wide");
            OcUi.Fact(facts, "LOCATION", OcRaceAndRide.EventLocation(entry).ToUpperInvariant());
            OcUi.Fact(facts, OpenContent.Str("STR_EVENT_FORMAT", "Event Format").ToUpperInvariant(), OcRaceAndRide.FormatName(choice).ToUpperInvariant());
            if (entry.Laps > 0)
                OcUi.Fact(facts, OpenContent.Str("STR_EVENT_DURATION", "Event Duration").ToUpperInvariant(), entry.Laps + " " + (entry.Laps == 1 ? "LAP" : OpenContent.Str("STR_LAPS", "laps").ToUpperInvariant()));
            OcUi.Fact(facts, "VEHICLE CLASS", OpenContent.ClassName(entry.VehicleClass).ToUpperInvariant());
            try
            {
                var own = OpenGarageData.VehicleOfClasses(new[] { entry.VehicleClass });
                if (own.HasValue) OcUi.Fact(facts, "YOUR VEHICLE", own.Value.title.ToUpperInvariant());
            }
            catch (Exception e) { Debug.LogWarning("[StartRace] vehicle: " + e.Message); }
            foreach (var l in facts.Query<Label>().ToList()) l.enableRichText = false;
            var buttons = card.Q<VisualElement>(className: "p2-popup-buttons");
            card.Insert(buttons != null ? card.IndexOf(buttons) : card.childCount, facts);
        }

        // ------------------------------------------------------------------------------------------------ RulesPopup

        /// The status name a RulesType keeps its "do not show again" under (RulesPopup:OnToggleKeypress).
        public static string RuleFlag(string rulesType) => "Rule_" + rulesType;

        /// RulesPopup before an event: the type's rules (Freestyle, SectionRace, Freeride ... = OwnCareerRulesText); CONTINUE starts, CONTINUE, DO NOT SHOW
        /// AGAIN starts and keeps the flag. Back does nothing (RulesPopup:OnBackKeypress: disabled). proceed is called after the popup closes.
        /// preview: the page opened on request (the RULES chip): CONTINUE only, and Back closes it.
        public VisualElement ShowRulesPopup(string rulesType, Action proceed, bool preview = false)
        {
            var text = OwnCareerRulesText.Read(rulesType);
            string more = OpenContent.Str("STR_RULE_CONTINUE_DONOTSHOWAGAIN", "CONTINUE, DO NOT SHOW AGAIN");
            var buttons = new List<(string, string, Action)> { (OpenContent.Str("STR_CONTINUE", "CONTINUE"), "popup-rules-continue", proceed) };
            if (!preview) buttons.Add((more, "popup-rules-skip", () => { CoreGarageProfile.SetEventStatus(RuleFlag(rulesType), 1); proceed?.Invoke(); }));
            var card = OpenPopup("popup-rules", text.Title, text.Format, text.Description, preview ? (Action)(() => { }) : null, 0, true, buttons.ToArray());
            NarrateRules(rulesType);
            return card;
        }

        /// The narration's owner while a rules popup is up (the Lua's owner is the layer; one at a time here).
        const string RulesNarrationOwner = "rules-popup";
        /// The voice the rules popup plays for the RulesType (RulesPopup:Start: App.PlayUILocalizedSoundEvent( "AudioLocalizedActivity", "Audio_Loc_<ACTIVITY>",
        /// "<ACTIVITY>/<ACTIVITY>/RULE" ) -> ReflexUiVoice.RulesVoice's table), on the shell's FeSounds whose volume follows EFFECTS VOLUME (the
        /// VoiceOver channel). A RulesType without a voice, or one the install does not have, is silent.
        internal void NarrateRules(string rulesType)
        {
            var vo = ReflexUiVoice.RulesVoice(rulesType);
            if (!vo.HasValue || _sounds == null) return;
            _sounds.volume = Options.ReflexSettings.EffectsVolume;   // (the options page may have moved the slider since the shell came up)
            Narrate(vo.Value, RulesNarrationOwner);
        }

        /// The RulesType an arcade event shows before it starts (MXUIManager's rules question 005a4bd0: Freestyle and the Section Race;
        /// the Free Ride only as the Race &amp; Ride challenges Free Ride); null when the event has none.
        public static string ArcadeRulesType(ReflexEvents.Event e, bool challengesFreeRide = false)
        {
            if (e == null) return null;
            switch (e.spec.type)
            {
                case "Freestyle": return "Freestyle";
                case "SectionRace": return "SectionRace";
                case "FreeRide": return challengesFreeRide ? "Freeride" : null;
                default: return null;
            }
        }

        /// Whether the rules popup is due: the event has rules and the player has not asked for them to stay away.
        public static bool RulesDue(string rulesType) => rulesType != null && CoreGarageProfile.EventStatus(RuleFlag(rulesType)) == 0;
    }
}
