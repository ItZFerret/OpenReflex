// The career's small pages in the open shell (OuP2Front.uss, the career kit of OuCareerKit.cs), each the original's:
//   CAREER WELCOME      CareerWelcome.lua: the first entry into the career (PlayerEvent "CareerWelcome" not set): STR_CAREERWELCOME_TITLE / _MESSAGE, the
//                       AI difficulty list (80 / 95 / 105 / 120 = STR_AI_LEVEL1..4, the list starts on the saved CareerDifficulty), _DIFFICULTY under it;
//                       accept stores CareerDifficulty (App.SetUISetting / SaveUISettings), sets the flag, goes on to the Race & Ride page.
//   CAREER COMPLETION   CareerCompletion.lua + RaceAndRide:Present: the career at 100 % (App.CalculateCareerCompletion >= 1) that has not shown it yet
//                       (PlayerEvent "CareerCompleted"): Videos\CareerCompletion.bik with the UI music muted, then Videos\Credits.bik (the layer's
//                       CreditsVideo), then back to Race & Ride and a save. Both play through OriginalVideo (the install's .bik, IBinkSource: the
//                       machine's ffmpeg in the open player). Back / Start skip to the next (a port courtesy; the original cannot skip).
//   VEHICLE LIST        VehiclesQuickViewPopup from Race and Ride Events (the toggle legend): the player's vehicles of the event's class types
//                       (VehicleClassType, by VehicleTypeId; columns STR_VEHICLE_TYPE / STR_VEHICLE_NAME), accept = RaceAndRideEvents:OnSelectPlayerVehicleId
//                       (the favourite and equipped vehicle); the Race & Ride page is read again with the new vehicle.
using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.Options;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed partial class OpenShell
    {
        // ------------------------------------------------------------------------------------------------ CAREER WELCOME

        /// CareerWelcome's CareerDifficulty values and their STR_AI_LEVEL names.
        public static readonly int[] CareerWelcomeDifficulties = { 80, 95, 105, 120 };

        /// MainMenu:CareerMode: the Welcome page comes first until PlayerEvent "CareerWelcome" is set.
        public static bool CareerWelcomeDue()
        {
            try { return CoreGarageProfile.Open() && CoreGarageProfile.EventStatus("CareerWelcome") == 0; }
            catch (Exception e) { Debug.LogWarning("[Career] welcome flag: " + e.Message); return false; }
        }

        /// App.PlayUILocalizedSoundEvent( "UIAudio", "UI_MOTO_INTRO", "UI_MOTO_INTRO_ENG/VO/INTRO" ): the MOTOCAREER introduction of the Career Welcome page.
        public static readonly (string database, string package, string eventPath) CareerWelcomeVoice = ("UIAudio", "UI_MOTO_INTRO", "UI_MOTO_INTRO_ENG/VO/INTRO");

        public void ShowCareerWelcome(Action proceed)
        {
            Clear(Screen.Working, "career"); SetBackground(null); MusicTier(2); CareerChrome();
            OcUi.Stage(_layer, true);
            var mid = OuUi.Div("oc-mid p2-center", _layer);
            var panel = OcUi.Panel(mid, OpenContent.Str("STR_CAREERWELCOME_TITLE", "MOTOCAREER WELCOME").ToUpperInvariant(), out var body, out _, "p2-welcome");
            panel.name = "career-welcome";
            foreach (var paragraph in OpenContent.Str("STR_CAREERWELCOME_MESSAGE", "").Split(new[] { "\n\n" }, StringSplitOptions.RemoveEmptyEntries))
                OuUi.Text(paragraph.Trim(), "p2-welcome-text", body).enableRichText = false;
            OuUi.Text(OpenContent.Str("STR_CAREERWELCOME_DIFFICULTY", "Set your preferred level of opponent difficulty. You can always change this setting in the options menu."),
                      "p2-welcome-ask", body).enableRichText = false;
            int index = Mathf.Max(0, Array.IndexOf(CareerWelcomeDifficulties, ReflexSettings.CareerDifficulty));
            VisualElement row = null;
            Action accept = () =>
            {
                ReflexSettings.CareerDifficulty = CareerWelcomeDifficulties[index]; ReflexSettings.Save();   // App.SetUISetting / SaveUISettings
                if (!CoreGarageProfile.SetEventStatus("CareerWelcome", 1)) Debug.LogWarning("[Career] the welcome flag was not saved: " + CoreGarageProfile.Error);
                OuSound.Play("Accept");
                StopNarration();   // CareerWelcome:End
                proceed?.Invoke();
            };
            row = OuUi.Spinner("DIFFICULTY", () => OpenContent.Str("STR_AI_LEVEL" + (index + 1), "").ToUpperInvariant(),
                d => { index = (index + d + CareerWelcomeDifficulties.Length) % CareerWelcomeDifficulties.Length; }, body, accept);
            row.name = "career-welcome-difficulty"; row.AddToClassList("p2-welcome-row");
            var go = OcUi.Button(OpenContent.Str("STR_CONTINUE", "CONTINUE").ToUpperInvariant(), "career-welcome-accept", accept, body);
            var column = new List<VisualElement> { row, go }; OuWidgets.LinkColumn(column);
            OcUi.Plaque(OuUi.Div("oc-bottom", _layer), OpenContent.Str("STR_CAREER", "MOTOCAREER").ToUpperInvariant());
            Action back = ShowMain;
            OcUi.Legend(_layer, (OuUi.KeySubmit, "SELECT", null), (OuUi.KeyCancel, OpenContent.Str("STR_BACK", "BACK").ToUpperInvariant(), back));
            _nav.OnCancel = () => { back(); return true; };   // (the original has no Back here; leaving without accepting asks again next time)
            _nav.DefaultItem = () => row; _nav.Focus(row);
            Narrate(CareerWelcomeVoice, "career-welcome");   // CareerWelcome:Start: the MOTOCAREER introduction, stopped at End
        }

        // ------------------------------------------------------------------------------------------------ CAREER COMPLETION

        /// RaceAndRide:Present: App.CalculateCareerCompletion >= 1 and "CareerCompleted" not set.
        public static bool CareerCompletionDue()
        {
            try
            {
                var p = CoreGarageProfile.Capture();
                return p != null && OpenMotocardData.CareerCompletion(p) >= 1f && CoreGarageProfile.EventStatus("CareerCompleted") == 0;
            }
            catch (Exception e) { Debug.LogWarning("[Career] completion check: " + e.Message); return false; }
        }

        /// The videos' names in the order they play (CareerCompletion.lua: the completion video, then the credits).
        public static readonly string[] CareerCompletionVideos = { "Videos\\CareerCompletion", "Videos\\Credits" };

        OriginalVideo _careerVideo;
        public OriginalVideo CareerVideo => _careerVideo;

        public void ShowCareerCompletion(Action proceed, string[] videos = null)
        {
            videos ??= CareerCompletionVideos;
            Clear(Screen.Working, "career"); SetBackground(null); CareerChrome();
            var page = OuUi.Div("p2-video-page", _layer); page.name = "career-completion";
            var picture = OuUi.Div("p2-video", page); picture.name = "career-completion-video"; picture.pickingMode = PickingMode.Ignore;
            var caption = OuUi.Text(OpenContent.Str("STR_MOTOCARD_TITLE_COMPLETION", "CONGRATULATIONS!").ToUpperInvariant(), "p2-video-caption", page);
            caption.enableRichText = false; caption.pickingMode = PickingMode.Ignore;
            int stage = -1; bool finished = false;
            _sounds?.music?.Mute(true);   // App.MuteUIMusic( TRUE )
            Action finish = null;
            Action next = null;
            next = () =>
            {
                if (finished) return;
                EndCareerVideo();
                stage++;
                if (stage >= videos.Length) { finish(); return; }
                var host = new GameObject("Open Reflex career completion video"); host.transform.SetParent(transform, false);
                var video = OriginalVideo.Create(host, videos[stage], false, false);
                _careerVideo = video; picture.style.backgroundImage = StyleKeyword.None;
                video.SetVolume(Mathf.Clamp01(PlayerPrefs.GetFloat("ReflexAudio.master", 1f)));
                video.Done += () => { if (_careerVideo == video) next(); };
                video.Play();
                if (stage == 1) _sounds?.music?.Mute(false);   // OnCompletionVideoDone: App.MuteUIMusic( FALSE ) before the credits video
                Debug.Log("[Career] completion: playing " + videos[stage]);
            };
            finish = () =>
            {
                if (finished) return;
                finished = true; EndCareerVideo(); _sounds?.music?.Mute(false);
                ScreenUpdate = null;
                proceed?.Invoke();   // OnCreditsVideoDone: CareerModeMenuPage, and the save (the profile saves with every change)
            };
            Action skip = () => { OuSound.Play("Back"); next(); };
            OcUi.Legend(_layer, (OuUi.KeyCancel, "SKIP", skip));
            _nav.OnCancel = () => { skip(); return true; };
            ScreenUpdate = () =>
            {
                var v = _careerVideo;
                if (v == null) return;
                if (v.Failed) { Debug.LogWarning("[Career] completion video failed: " + v.Error); next(); return; }   // no picture: on as after the last frame
                if (v.HasFrame && v.Texture)
                {
                    picture.style.backgroundImage = new StyleBackground(Background.FromRenderTexture(v.Texture));
                    caption.style.display = DisplayStyle.None;
                }
            };
            next();
        }

        void EndCareerVideo()
        {
            var v = _careerVideo; _careerVideo = null;
            if (v == null) return;
            try { v.Stop(); v.Close(); } catch (Exception e) { Debug.LogWarning("[Career] video close: " + e.Message); }
            if (v) UnityEngine.Object.Destroy(v.gameObject);
        }

        // ------------------------------------------------------------------------------------------------ VEHICLE LIST (VehiclesQuickViewPopup)

        /// The owned vehicles the event's class takes (VehicleClassType types of the series' VehicleClassId), by VehicleTypeId, the equipped one first
        /// within its type (BuildVehicleTypesFromVehicleClassId / BuildVehicles).
        internal static List<GarageItem> CareerQuickViewVehicles(CareerEventChoice choice)
        {
            var list = new List<GarageItem>();
            if (choice == null) return list;
            var types = new List<int>();
            foreach (var r in FeTables.Shared.Rows("VehicleClassType"))
                if (FeTables.I(r, "VehicleClassId") == choice.Entry.VehicleClass) types.Add(FeTables.I(r, "VehicleTypeId"));
            types.Sort();
            try
            {
                var owned = OpenGarageData.Vehicles().Where(v => v.owned).ToList();
                foreach (int t in types) list.AddRange(owned.Where(v => v.type == t));
            }
            catch (Exception e) { Debug.LogWarning("[Career] vehicle list: " + e.Message); }
            return list;
        }

        List<int> CareerQuickViewTypes(CareerEventChoice choice) => CareerQuickViewVehicles(choice).Select(v => v.type).Distinct().ToList();

        /// VehiclesQuickViewPopup over the Race &amp; Ride page. reload runs after the player picked (or kept) a vehicle: the page is read again.
        public void ShowCareerVehicleList(CareerEventChoice choice, Action reload)
        {
            var vehicles = CareerQuickViewVehicles(choice);
            var card = BeginModal("popup-vehicle-list", "p2qv", () => { }, 1500);
            OuUi.Text(OpenContent.Str("STR_QUICKVIEW", "VEHICLE LIST").ToUpperInvariant(), "ou-dialog-title ou-display", card).enableRichText = false;
            var cols = OuUi.Div("p2-qv", card);
            var left = OuUi.Div("p2-qv-left", cols);
            var head = OuUi.Div("p2-qv-head", left);
            OuUi.Text(OpenContent.Str("STR_VEHICLE_TYPE", "VEHICLE TYPE").ToUpperInvariant(), "p2-qv-col p2-qv-type", head).enableRichText = false;
            OuUi.Text(OpenContent.Str("STR_VEHICLE_NAME", "VEHICLE NAME").ToUpperInvariant(), "p2-qv-col p2-qv-name", head).enableRichText = false;
            var scroll = new ScrollView(ScrollViewMode.Vertical); scroll.AddToClassList("p2-qv-scroll"); scroll.horizontalScrollerVisibility = ScrollerVisibility.Hidden; left.Add(scroll);
            var perf = OcUi.Panel(cols, "PERFORMANCE", out var perfBody, out var perfTitle, "p2-qv-perf");
            var bars = OuUi.Div("p2-qv-bars", perfBody);
            var rows = new List<VisualElement>(); VisualElement start = null;
            foreach (var v in vehicles)
            {
                var item = v;
                var row = OuUi.Item(OuUi.Div("p2-qv-row", scroll.contentContainer), () =>
                {
                    if (!item.equipped && !OpenGarageData.SelectVehicle(item.playerVehicle))
                    { Debug.LogWarning("[Career] the vehicle could not be equipped: " + ReflexProfile.Error); OuSound.Play("Locked"); return; }
                    ClosePopup(false); reload?.Invoke();   // OnSelectPlayerVehicleId: SetPlayerFavoriteVehicle + EquipVehicle
                });
                row.name = "career-vehicle-" + item.playerVehicle;
                OuUi.Text(OpenGarageData.TypeName(item.type).ToUpperInvariant(), "p2-qv-col p2-qv-type", row).enableRichText = false;
                OuUi.Text(item.title.ToUpperInvariant(), "p2-qv-col p2-qv-name", row).enableRichText = false;
                if (item.equipped) OcUi.Tag(row, "EQUIPPED", "good");
                row.RegisterCallback<FocusInEvent>(_ =>
                {
                    scroll.ScrollTo(row);
                    perfTitle.text = item.title.ToUpperInvariant();
                    bars.Clear();
                    foreach (var s in OpenGarageData.Stats(item.id, item.playerVehicle)) QuickViewBars(bars, s);
                });
                rows.Add(row);
                if (item.equipped && start == null) start = row;
            }
            if (rows.Count == 0) OuUi.Text("No vehicle of this class is owned.", "p2-qv-empty", scroll.contentContainer);
            OuWidgets.LinkColumn(rows);
            var hint = OuUi.Div("p2-popup-hints", card);
            OuUi.Hints(hint, (OuUi.KeySubmit, "SELECT"), (OuUi.KeyCancel, "BACK"));
            _nav.Modal = card;
            var first = start ?? rows.FirstOrDefault();
            if (first != null) { _nav.Focus(first); first.schedule.Execute(() => scroll.ScrollTo(first)).ExecuteLater(60); }
        }

        static void QuickViewBars(VisualElement parent, GarageStat s)
        {
            var group = OuUi.Div("p2-qv-stat", parent);
            OuUi.Text(s.name, "p2-qv-stat-name", group).enableRichText = false;
            var tracks = OuUi.Div("p2-qv-stat-bars", group);
            for (int k = 0; k < (s.two ? 2 : 1); k++)
            {
                int b = Mathf.Clamp(k == 0 ? s.base1 : s.base2, 0, 100), bonus = k == 0 ? s.bonus1 : s.bonus2;
                var track = OuUi.Div("p2-bar", tracks);
                var fill = OuUi.Div("p2-bar-base", track); fill.style.width = Length.Percent(b);
                if (s.two) { var extra = OuUi.Div("p2-bar-bonus", track); extra.style.left = Length.Percent(b); extra.style.width = Length.Percent(Mathf.Clamp(bonus, 0, 100 - b)); }
            }
        }
    }
}
