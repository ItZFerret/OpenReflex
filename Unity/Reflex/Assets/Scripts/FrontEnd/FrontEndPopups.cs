// C# ports of the front end's popup scripts (PopupPageStack.pag) and the overlay page's scripts (OverlayPageGraph.pag), WP-4:
// CheatEntryPopup / CheatPopup (OPTIONS -> CHEAT ENTRY; the code is checked by the game itself: App.ProcessCheatEntryString in the
// profile session), StorageQuestionPopup / StorageNotificationPopup / DLCBindErrorPopup (the load-game failure paths of
// TitleLoadGame), PageWipe and the UIFadeLayer (Fade.lyr). Function names follow the Lua so the layers' triggers call them.
using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using UnityEngine;

namespace Reflex.Port.FrontEnd
{
    public static class FePopupScripts
    {
        public static FeScript Create(string script)
        {
            switch (script)
            {
                case "CheatEntryPopup": return new CheatEntryPopupScript();
                case "CheatPopup": return new CheatPopupScript();
                case "StorageQuestionPopup": return new StorageQuestionPopupScript();
                case "StorageNotificationPopup": return new StorageNotificationPopupScript();
                case "DLCBindErrorPopup": return new DLCBindErrorPopupScript();
                case "PageWipe": return new PageWipeScript();
                default: return null;
            }
        }

        /// UIIconComponent:SetColorState( color, App.GetUIState( "Visible" ) ): the component's Visible state colour.
        public static void SetVisibleColor(FeNode n, int r, int g, int b, int a)
        {
            if (n == null) return;
            n.props["VisibleColor"] = new List<object> { (double)r, (double)g, (double)b, (double)a };
        }

        /// The page-stack page of a popup layer ("Hide" .. m_Layer:GetContainer( ):GetName( )): the top popup's page name.
        public static string HideEvent(FrontEndManager fe, string fallback) => fe.TopPopup != null ? "Hide" + fe.TopPopup : fallback;

        /// A question to the game in the profile session: the Lua (run as is in the game's UI Lua state, career.cpp "lua_ui") leaves its
        /// answer in the Lua global ReflexAnswer (a string); it comes back through a UI variable 30 characters at a time (UI string
        /// variables keep 31). A Lua error comes back as "ERR <message>". Null without a session (or a libreflexworld without "lua_ui").
        public static string AskGame(string lua)
        {
            if (!ReflexProfile.Open().Result) return null;
            lock (askGate)
            {
                string r = ReflexProfile.Command("lua_ui ReflexChunk ReflexAnswer = \"\"; local ok, err = pcall( function( ) " + lua + " end ); " +
                                                 "if not ok then ReflexAnswer = \"ERR \" .. tostring( err ) end; ReflexAnswer = tostring( ReflexAnswer ); App.SetUIVariableString( \"ReflexChunk\", tostring( string.len( ReflexAnswer ) ) )");
                if (r == null || !int.TryParse(r, out int length)) return null;
                var sb = new System.Text.StringBuilder(length);
                for (int at = 1; at <= length; at += 30)
                {
                    string c = ReflexProfile.Command("lua_ui ReflexChunk App.SetUIVariableString( \"ReflexChunk\", string.sub( ReflexAnswer, " + at + ", " + (at + 29) + " ) )");
                    if (c == null) return null;
                    sb.Append(c);
                }
                string answer = sb.ToString();
                if (answer.StartsWith("ERR ", StringComparison.Ordinal)) Debug.LogWarning("[FrontEnd] the game's Lua: " + answer);
                return answer;
            }
        }
        static readonly object askGate = new object();
        /// Row / field separators of the answers (a UI variable keeps neither newlines nor tabs).
        public const char RowSep = '^', FieldSep = '|';
    }

    // CheatEntryPopup.lua (CheatEntry.lyr): CHEAT CODE / the typed text (App.InitializeUIDisplayText, UseAlpha, SetInputSize 20; the
    // UIKeyboardInput events, shown upper case) / CONTINUE. Accept: App.ProcessCheatEntryString( text ) -> CheatCodeEntry = the text
    // or "Invalid", ShowCheatCodePopup, then HideCheatEntryPopup. Back: HideCheatEntryPopup.
    public sealed class CheatEntryPopupScript : FeScript
    {
        FeTextInput input; FeNode message; string entry = "";
        System.Threading.Tasks.Task<bool> processing;
        public FeTextInput Input => input;
        public override void Initialize()
        {
            C("TitleText")?.SetText(Str("STR_CHEATCODE"));
            message = C("MessageText");
            C("ButtonText")?.SetText(Str("STR_CONTINUE"));
            functions["OnAcceptKeypress"] = OnAcceptKeypress;
            functions["OnBackKeypress"] = () => { fe.PlaySound("Back"); fe.SendUIEvent("HideCheatEntryPopup"); };
        }
        public override void Start()
        {
            fe.PlaySound("PopupShow");
            entry = ""; message?.SetText(entry);
            input = new FeTextInput(20, true, false, t => { message?.SetText(t.ToUpperInvariant()); entry = t; });
            KeyGlyph.Refresh(fe, layer.root);
        }
        public override void End() { input?.End(); input = null; }
        public override void Update(float dt)
        {
            input?.Update();
            if (processing != null && processing.IsCompleted)
            {
                bool ok = !processing.IsFaulted && processing.Result; processing = null;
                fe.vars["CheatCodeEntry"] = ok ? entry : "Invalid";
                // the Lua sends ShowCheatCodePopup, then HideCheatEntryPopup (a pop of that page); FrontEndManager pops the top page, so
                // this popup goes first
                fe.SendUIEvent("HideCheatEntryPopup");
                fe.SendUIEvent("ShowCheatCodePopup");
            }
        }
        void OnAcceptKeypress()
        {
            if (processing != null) return;
            fe.PlaySound("Accept");
            string text = new string((entry ?? "").Where(c => char.IsLetter(c) || c == ' ').ToArray());
            // App.ProcessCheatEntryString (00730810 -> 005a45f0: brapbrap / couches / sidexside / compact / buggedout / fullsize unlock
            // vehicle types, gearedup / skullcap / windows / safetyfirst / kicks gear, allai, whereto, readytorace, ...)
            processing = System.Threading.Tasks.Task.Run(() =>
            {
                string r = FePopupScripts.AskGame("if( App.ProcessCheatEntryString( \"" + text + "\" ) == TRUE ) then ReflexAnswer = \"1\" else ReflexAnswer = \"0\" end");
                bool ok = r == "1";
                if (ok) ReflexProfile.Run("json").Wait();   // the game's tables after the unlock
                Debug.Log("[FrontEnd] cheat '" + text + "': " + (r ?? "no profile session") );
                return ok;
            });
        }
    }

    // CheatPopup.lua (CheatPopup.lyr, page CheatCodePopup): CHEAT CODE / STR_CHEATCODE_<CheatCodeEntry upper case> / CONTINUE; accept /
    // back: Hide<page>.
    public sealed class CheatPopupScript : FeScript
    {
        public override void Initialize()
        {
            C("TitleText")?.SetText(Str("STR_CHEATCODE"));
            C("ButtonText")?.SetText(Str("STR_CONTINUE"));
            functions["OnAcceptKeypress"] = () => { fe.PlaySound("Accept"); fe.SendUIEvent(FePopupScripts.HideEvent(fe, "HideCheatCodePopup")); fe.PlaySound("Accept"); };
            functions["OnBackKeypress"] = () => { fe.PlaySound("Back"); fe.SendUIEvent(FePopupScripts.HideEvent(fe, "HideCheatCodePopup")); fe.PlaySound("Back"); };
        }
        public override void Start()
        {
            fe.PlaySound("PopupShow");
            string code = fe.Var<string>("CheatCodeEntry", "") ?? "";
            C("MessageText")?.SetText(Str("STR_CHEATCODE_" + code.ToUpperInvariant()));
            fe.vars.Remove("CheatCodeSuccess");
            KeyGlyph.Refresh(fe, layer.root);
        }
    }

    // StorageQuestionPopup.lua: the title backing red (144, 39, 39); StorageQuestion "DeviceCorrupted" (STR_STORAGE_CORRUPTED_*) or
    // "DeviceChange"; ButtonOne / ButtonTwo -> HideStorageDeviceQuestionPopup, then End sends OnAcceptStorageQuestionButtonOne / Two
    // (TitleLoadGame's UIEventTriggers: create the motocard / continue without saving). Back does nothing.
    public sealed class StorageQuestionPopupScript : FeScript
    {
        int selected;
        public override void Initialize()
        {
            FePopupScripts.SetVisibleColor(C("TitleBacking"), 144, 39, 39, 255);
            functions["OnAcceptButtonOneKeypress"] = () => { fe.PlaySound("Accept"); selected = 1; fe.SendUIEvent("HideStorageDeviceQuestionPopup"); };
            functions["OnAcceptButtonTwoKeypress"] = () => { fe.PlaySound("Accept"); selected = 2; fe.SendUIEvent("HideStorageDeviceQuestionPopup"); };
            functions["OnBackKeypress"] = () => { };
        }
        public override void Start()
        {
            fe.PlaySound("PopupShow");
            selected = 0;
            string q = fe.Var<string>("StorageQuestion", ""), title = "STR_EMPTY", message = "STR_EMPTY", one = "STR_YES", two = "STR_NO";
            if (q == "DeviceCorrupted") { title = "STR_STORAGE_CORRUPTED_TITLE"; message = "STR_STORAGE_CORRUPTED_MESSAGE"; one = "STR_STORAGE_CORRUPTED_BUTTONONE"; two = "STR_STORAGE_CORRUPTED_BUTTONTWO"; }
            else if (q == "DeviceChange") { title = "STR_STORAGE_DEVICE_CHANGE_TITLE"; message = "STR_STORAGE_DEVICE_CHANGE_MESSAGE"; }
            fe.vars.Remove("storageQuestion");   // (sic: the Lua removes "storageQuestion")
            C("TitleText")?.SetText(Str(title)); C("MessageText")?.SetText(Str(message));
            C("ButtonOne")?.Find("ButtonText")?.SetText(Str(one)); C("ButtonTwo")?.Find("ButtonText")?.SetText(Str(two));
            var list = C("ButtonList"); if (list != null) list.focusIndex = 0;
            KeyGlyph.Refresh(fe, layer.root);
        }
        public override void End()
        {
            if (selected == 0) return;
            string fn = selected == 1 ? "OnAcceptStorageQuestionButtonOne" : "OnAcceptStorageQuestionButtonTwo";
            selected = 0;
            fe.SendUIEvent(fn);
            foreach (var name in new[] { "TitleLoadGame.lyr" }) { var l = fe.Layer(name); if (l != null && l.active) l.script?.Call(fn); }   // its UIEventTrigger
        }
    }

    // StorageNotificationPopup.lua: the title backing red; StorageNotification DeviceChange / DeviceDeclined / DeviceOutOfSpace
    // (STR_STORAGE_NO_FREE_SPACE_TITLE + EMB_STORAGE_NO_FREE_SPACE_MESSAGE) / DeviceRemoved; CONTINUE; accept / back: Hide<page>.
    public sealed class StorageNotificationPopupScript : FeScript
    {
        public override void Initialize()
        {
            FePopupScripts.SetVisibleColor(C("TitleBacking"), 144, 39, 39, 255);
            functions["OnAcceptKeypress"] = () => { fe.PlaySound("Accept"); fe.SendUIEvent(FePopupScripts.HideEvent(fe, "HideStorageNotificationPopup")); };
            functions["OnBackKeypress"] = () => { fe.PlaySound("Back"); fe.SendUIEvent(FePopupScripts.HideEvent(fe, "HideStorageNotificationPopup")); };
        }
        public override void Start()
        {
            fe.PlaySound("PopupShow");
            string n = fe.Var<string>("StorageNotification", ""), title = "STR_EMPTY", message = "STR_EMPTY";
            switch (n)
            {
                case "DeviceChange": title = "STR_STORAGE_DEVICE_CHANGE_TITLE"; message = "STR_STORAGE_DEVICE_CHANGE_MESSAGE"; break;
                case "DeviceDeclined": title = "STR_STORAGE_DEVICE_DECLINED_TITLE"; message = "STR_STORAGE_DEVICE_DECLINED_MESSAGE"; break;
                case "DeviceOutOfSpace": title = "STR_STORAGE_NO_FREE_SPACE_TITLE"; message = "EMB_STORAGE_NO_FREE_SPACE_MESSAGE"; break;
                case "DeviceRemoved": title = "STR_STORAGE_DEVICE_REMOVED_TITLE"; message = "STR_STORAGE_DEVICE_REMOVED_MESSAGE"; break;
            }
            fe.vars.Remove("StorageNotification");
            C("TitleText")?.SetText(Str(title));
            string emb = message.StartsWith("EMB_", StringComparison.Ordinal) ? FeCareerScripts.Embed(fe, message) : "";
            C("MessageText")?.SetText(emb.Length > 0 ? emb : Str(message));
            C("ButtonText")?.SetText(Str("STR_CONTINUE"));
            KeyGlyph.Refresh(fe, layer.root);
        }
    }

    // DLCBindErrorPopup.lua (TitleLoadGame:OnGameLoaded when MXProfile:ValidateDLC is FALSE: the save is bound to DLC that is not
    // installed): STR_DLCBINDERROR_*, the title backing red, ButtonTwo focused. ButtonOne (continue without saving): End restores the
    // default tables, signs in without storage, SetGameLoaded, "Create Motocard". ButtonTwo (cancel): ClearExclusiveMode,
    // SetPrimaryPlayerDeviceIndex( -1 ), "Profile Status Update" (back to the title). Back does nothing.
    public sealed class DLCBindErrorPopupScript : FeScript
    {
        bool createNew, cancel;
        public override void Initialize()
        {
            C("TitleText")?.SetText(Str("STR_DLCBINDERROR_TITLE")); C("MessageText")?.SetText(Str("STR_DLCBINDERROR_MESSAGE"));
            C("ButtonOne")?.Find("ButtonText")?.SetText(Str("STR_DLCBINDERROR_BUTTONONE")); C("ButtonTwo")?.Find("ButtonText")?.SetText(Str("STR_DLCBINDERROR_BUTTONTWO"));
            FePopupScripts.SetVisibleColor(C("TitleBacking"), 144, 39, 39, 255);
            functions["OnAcceptButtonOneKeypress"] = () => { fe.PlaySound("Accept"); createNew = true; fe.SendUIEvent(FePopupScripts.HideEvent(fe, "HideDLCBindErrorPopup")); };
            functions["OnAcceptButtonTwoKeypress"] = () => { fe.PlaySound("Accept"); cancel = true; fe.SendUIEvent(FePopupScripts.HideEvent(fe, "HideDLCBindErrorPopup")); };
            functions["OnBackKeypress"] = () => { };
        }
        public override void Start()
        {
            fe.PlaySound("PopupShow");
            createNew = cancel = false;
            var list = C("ButtonList"); if (list != null) list.focusIndex = 1;   // m_ButtonTwo:SetFocused( TRUE )
            KeyGlyph.Refresh(fe, layer.root);
        }
        public override void End()
        {
            if (createNew)
            {
                createNew = false;
                ReflexProfile.Run("lua local p = App.GetMXProfile( 0 ); p:RestoreDefaultTables( TRUE ); p:SignIn( nil ); p:SetGameLoaded( )");
                fe.SendUIEvent("Create Motocard");
            }
            else if (cancel)
            {
                cancel = false;
                // ClearExclusiveMode, SetPrimaryPlayerDeviceIndex( -1 ), "Profile Status Update" (-> Profile Login Status Updated Page ->
                // the title): the player is signed out; the next Start loads again (the port: a new profile session)
                ReflexProfile.Close();
                fe.JumpTo("Title Page");
            }
        }
    }

    // PageWipe.lua (OverlayPageGraph's PageWipe.lyr): hidden; the UI event ShowPageWipe(name) plays "In" and shows it; at the end of
    // "In" (UIAnimationTrigger OnInAnimationComplete) "Out" plays and PageWipeTransition(name) is sent, at the end of "Out"
    // (OnOutAnimationComplete) it hides and PageWipeComplete(name) is sent. PageWipeInDuration = the "In" animation's length.
    // Senders: the in-race PostEventMenu / PostEventMotoResults / AAALoading (WP-5); the front end's Lua sends none.
    public sealed class PageWipeScript : FeScript
    {
        string wipeName; Action<string> onTransition, onComplete;
        public override void Initialize()
        {
            layer.root.Visible = false;
            fe.vars["PageWipeInDuration"] = layer.Length("In");
            functions["OnInAnimationComplete"] = () =>
            {
                layer.Play("Out");
                var t = onTransition; onTransition = null; t?.Invoke(wipeName);   // PageWipeTransition
            };
            functions["OnOutAnimationComplete"] = () =>
            {
                layer.root.Visible = false;
                var c = onComplete; onComplete = null; c?.Invoke(wipeName);         // PageWipeComplete
            };
        }
        /// The UI event ShowPageWipe( name ).
        public void Show(string name, Action<string> transition, Action<string> complete)
        {
            wipeName = name; onTransition = transition; onComplete = complete;
            layer.Play("In"); layer.root.Visible = true;
            Debug.Log("[FrontEnd] PageWipeStart: " + name);
        }
        public bool Wiping => layer.root.visible;
    }

    // UIFadeLayer (Fade.lyr, no script; vtable 00951b64): the UI events FadeIn / FadeOut (time, colour; e.g. the gate selection's
    // FadeOut 0.5 s black, 005bdaa0) and FadeSetAlpha; vf11 each frame: the FadeImage's alpha = colour alpha x elapsed / time (fade out)
    // or 1 - that (fade in); FadeComplete at the end.
    public sealed class FeFade
    {
        readonly FeLayer layer; readonly FeNode image; float t, time; bool fadingIn, active; Color color = Color.black; Action done;
        public FeFade(FeLayer layer) { this.layer = layer; image = layer.Find("FadeImage"); SetAlpha(0f); }
        public void FadeOut(float seconds, Color c, Action complete = null) { Begin(false, seconds, c, complete); }
        public void FadeIn(float seconds, Action complete = null) { Begin(true, seconds, color, complete); }
        public void SetAlpha(float a) { if (image != null) image.overrideColor = new Color(color.r, color.g, color.b, a); }
        void Begin(bool fadeIn, float seconds, Color c, Action complete) { fadingIn = fadeIn; time = Mathf.Max(0.0001f, seconds); t = 0; color = c; active = true; done = complete; }
        public bool Active => active;
        public void Tick(float dt)
        {
            if (!active) return;
            t = Mathf.Min(time, t + dt);
            float a = color.a * (t / time);
            SetAlpha(fadingIn ? 1f - a : a);
            if (t >= time) { active = false; var d = done; done = null; d?.Invoke(); }   // FadeComplete
        }
    }
}
