// The front end's sound (WP-F): App.PlayUISound, App.PlayUILocalizedSoundEvent / StopUILocalizedSoundEvent and the UI music
// (App.SetEnableUIMusic / SetUIMusicTier / MuteUIMusic), everything read from the player's install at run time.
//
// * App.PlayUISound(name): UIRegistry.xml maps the UI sound names to FMOD events (<UISound eventName="UI_02/UI/ACCEPT"
//   UIName="Accept"/>) of the UI_02 project (Database/UIAudio, package SFX). The events are played by the port's FMOD Event
//   player (OriginalWorld/OriginalAudio, the one the race uses) on a level-less library (OriginalAudio.BeginEmpty + AddProjects):
//   sound-definition play modes (sequential / random / random-no-repeat / shuffle) and their volume and pitch randomisation, the
//   event volume, max playbacks (every UI_02 event: 1, the new start takes over) and the MP3 decoder-delay trim (529 samples) are
//   that player's. One FMOD wrapper per event, as the engine's one-shot of a max-playbacks-1 event.
// * App.PlayUILocalizedSoundEvent(database, package, event, owner) (binding 007283d0 -> 0068dd40: a queue of up to 10 requests,
//   the event path through 0067e560: "[PLATFORM]" -> "PC", "[LOC]" -> the language, "[FORMAT]" -> the activity / format): the
//   package <package>.<LANG> of that database (voice ships ENG and FRE; GER / ITA / SPA text languages take the ENG voice, as the
//   race library does: ReflexAudioSet.VoiceLanguage) is loaded and the event played in its FEV project: the Lua's project segment
//   ("UI_GAME_INTRO/VO/INTRO", "NATIONAL/NATIONAL/RULE_MOTO") names the package's project without its language
//   (UI_GAME_INTRO_ENG, NATIONAL_ENG), so the path's first segment is the package's project. StopUILocalizedSoundEvent(owner) stops
//   what that owner started. An event the project does not have is silent, as in the original (UI_MOTO_COMPLETE/VO/COMPLETE_PC:
//   the data has COMPLETE_PS3 / COMPLETE_XENON only).
// * The UI music: FeUiMusic below.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.Networking;

namespace Reflex.Port.FrontEnd
{
    public sealed class FeSounds
    {
        /// Every UI sound / voice-over / music request the front end made ("ui Accept -> ui_02/ui/accept", "vo ...", "music ..."),
        /// newest last (bench / tests log it).
        public static readonly List<string> Log = new List<string>();
        public static void Note(string s) { lock (Log) { Log.Add(s); if (Log.Count > 4000) Log.RemoveRange(0, 1000); } }

        readonly Dictionary<string, string> byUiName = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);   // UIName -> event path
        Task<Dictionary<string, string>> loading;
        readonly OriginalAudio audio;
        readonly Dictionary<string, uint> wrappers = new Dictionary<string, uint>(StringComparer.OrdinalIgnoreCase);
        readonly Dictionary<string, (uint wrapper, string path)> owners = new Dictionary<string, (uint, string)>(StringComparer.Ordinal);
        uint nextWrapper = 0x00fe0001, serial;
        public readonly FeUiMusic music;
        public string Error { get; private set; }
        /// The FMOD event player (diagnostics: Stats per event path).
        public OriginalAudio Player => audio;

        /// EffectsVolume: the UI and VoiceOver channels follow it (the settings code 005b9b93: EffectsVolume -> Effects, UI, Scenario,
        /// VoiceOver, DriveOn, Skid, Crowd).
        public float volume { get => audio ? audio.masterVolume : 1f; set { if (audio) audio.masterVolume = value; } }

        public FeSounds(Component host)
        {
            _ = ReflexInstall.Packages;
            string registry = ReflexInstall.PathOf("UIRegistry.xml");
            loading = Task.Run(() => LoadRegistry(registry));
            audio = host.gameObject.GetComponent<OriginalAudio>() ?? host.gameObject.AddComponent<OriginalAudio>();
            audio.followSettings = false;
            audio.BeginEmpty("FrontEnd");
            audio.AddProjects(new[] { "UI_02" });   // UIAudio/SFX, as AudioPackagesList.xml preloads UIAudio/SFX
            music = new FeUiMusic(host);
        }

        static Dictionary<string, string> LoadRegistry(string registryPath)
        {
            var map = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            string xml = Regex.Replace(File.ReadAllText(registryPath), "<!--.*?-->", "", RegexOptions.Singleline);
            foreach (Match m in Regex.Matches(xml, "<UISound\\s+eventName=\"([^\"]+)\"\\s+UIName=\"([^\"]+)\""))
                if (!map.ContainsKey(m.Groups[2].Value)) map[m.Groups[2].Value] = m.Groups[1].Value;
            return map;
        }

        bool Ready()
        {
            if (loading == null) return true;
            if (!loading.IsCompleted) return false;
            if (loading.IsFaulted) { Error = loading.Exception.GetBaseException().Message; Debug.LogWarning("Front-end UI sounds not read from the install: " + Error); }
            else foreach (var kv in loading.Result) byUiName[kv.Key] = kv.Value;
            loading = null;
            return true;
        }

        public bool Has(string uiName) { WaitRegistry(); return uiName != null && byUiName.ContainsKey(uiName); }
        public int Count { get { WaitRegistry(); return byUiName.Count; } }
        void WaitRegistry() { if (loading != null && !loading.IsCompleted) try { loading.Wait(5000); } catch (Exception) { } Ready(); }
        public string EventOf(string uiName) { WaitRegistry(); return uiName != null && byUiName.TryGetValue(uiName, out var p) ? p : null; }

        void Start(uint wrapper, string path)
        {
            if (!audio) return;
            audio.Handle(new ReflexAudioCommand { sub = 0, wrapper = wrapper, serial = ++serial, path = path, position = new float[3], param = "" });
        }
        void Stop(uint wrapper)
        {
            if (!audio) return;
            audio.Handle(new ReflexAudioCommand { sub = 1, wrapper = wrapper, serial = ++serial, path = "", position = new float[3], param = "" });
        }
        uint WrapperOf(string key) { if (!wrappers.TryGetValue(key, out var w)) wrappers[key] = w = nextWrapper++; return w; }

        /// App.PlayUISound(name).
        public void Play(string uiName)
        {
            if (string.IsNullOrEmpty(uiName)) return;
            WaitRegistry();
            if (!byUiName.TryGetValue(uiName, out var path)) return;
            Note("ui " + uiName + " -> " + path);
            Start(WrapperOf(path), path);
        }

        /// App.PlayUILocalizedSoundEvent(database, package, event, owner).
        public void PlayLocalized(string database, string package, string eventPath, string owner)
        {
            owner ??= "MXLocalizedAudio";
            string path = ResolveLocalized(database, package, eventPath, out string project);
            Note("vo " + owner + ": " + database + "/" + package + " " + eventPath + " -> " + (path ?? "(no such package)"));
            if (path == null) return;
            if (owners.TryGetValue(owner, out var old) && old.path != path) Stop(old.wrapper);
            audio?.AddProjects(new[] { project });
            uint w = WrapperOf("vo:" + owner + ":" + path);
            owners[owner] = (w, path);
            Start(w, path);
        }

        /// App.StopUILocalizedSoundEvent(owner).
        public void StopLocalized(string owner)
        {
            if (owner == null || !owners.TryGetValue(owner, out var o)) return;
            Note("vo stop " + owner);
            owners.Remove(owner);
            Stop(o.wrapper);
        }

        /// The voice language of the text language (ENG / FRE ship; the other text languages fall back to ENG).
        public static string VoiceLanguage() { try { return ReflexAudioSet.VoiceLanguage(ReflexUiData.Shared.language); } catch (Exception) { return "ENG"; } }

        /// 0067e560: the path's tokens; the first segment replaced by the localized package's FEV project. Null when the package is missing.
        public static string ResolveLocalized(string database, string package, string eventPath, out string project)
        {
            project = null;
            if (string.IsNullOrEmpty(database) || string.IsNullOrEmpty(package) || string.IsNullOrEmpty(eventPath)) return null;
            string lang = VoiceLanguage();
            string ev = Regex.Replace(eventPath, "\\[PLATFORM\\]", "PC", RegexOptions.IgnoreCase);
            ev = Regex.Replace(ev, "\\[LOC\\]", lang, RegexOptions.IgnoreCase);
            var db = ReflexInstall.Packages.Db(database);
            if (db == null) return null;
            Rfx.Package pk = null;
            foreach (var want in new[] { package + "." + lang, package + ".ENG", package })
            {
                pk = db.packages.Values.FirstOrDefault(p => string.Equals(p.name, want, StringComparison.OrdinalIgnoreCase));
                if (pk != null) break;
            }
            // the package's FEV project named by the path's first segment (+ "_<LANG>"; AudioLocalizedTutorial's Audio.<LANG> holds TUTORIAL_01..06,
            // TUTORIAL_INTRO and _OUTRO), else its first (the packages of one project)
            int seg = ev.IndexOf('/'); string first = seg > 0 ? ev.Substring(0, seg) : null;
            var sound = (first == null ? null : pk?.OfType("sound").FirstOrDefault(s => s.name.StartsWith(first + "_", StringComparison.OrdinalIgnoreCase) || string.Equals(s.name, first, StringComparison.OrdinalIgnoreCase)))
                        ?? pk?.OfType("sound").FirstOrDefault();
            if (sound == null) return null;
            project = sound.name;
            int slash = ev.IndexOf('/');
            return (project + (slash >= 0 ? ev.Substring(slash) : "/" + ev)).ToLowerInvariant();
        }

        /// Once a frame (FrontEndManager.Step): the event player's mix and the music.
        public void Tick(float dt)
        {
            if (audio && Application.isPlaying) audio.Tick(dt, Vector3.zero);
            music?.Tick(dt);
        }
    }

    // App.PlayUILocalizedSoundEvent for the in-race pages (the C# HUD / pause / post-race pages of OriginalWorld, which have no
    // FrontEndManager): the same resolution (FeSounds.ResolveLocalized) on the race's own event player, the project added to its
    // library when it is not there (the activity's announcer project usually is). RulesVoice is RulesPopup.lua's table (RulesType ->
    // the narration of the rules popup), AAANotification's is ("AudioLocalizedActivity", "Audio_ACTIVITY_FREERIDE",
    // "ACTIVITY_FREERIDE/ACTIVITY/CHALLENGE_APPEAR"). EventIntro.lua plays no narration (App.SetUIMusicTier( 2 ) only, which does nothing
    // while the jukebox plays).
    public static class ReflexUiVoice
    {
        public static (string db, string package, string eventPath)? RulesVoice(string rulesType)
        {
            const string A = "AudioLocalizedActivity";
            switch (rulesType)
            {
                case "Freestyle": return (A, "Audio_Loc_FREESTYLE", "FREESTYLE/FREESTYLE/RULE");
                case "Freeride": return (A, "Audio_Loc_FREERIDE", "FREERIDE/FREERIDE/RULE");
                case "SectionRace": return (A, "Audio_Loc_SECTION_RACE", "SECTIONRACE/SECTIONRACE/RULE");
                case "Faceoff": return (A, "Audio_Loc_FACEOFF", "FACEOFF/FACEOFF/RULE");
                case "Eliminator": return (A, "Audio_Loc_ELIMINATION", "ELIMINATOR/ELIMINATOR/RULE");
                case "Overtake": return (A, "Audio_Loc_OVERTHROW", "OVERTAKE/OVERTAKE/RULE");
                case "SpeednStyle": return (A, "Audio_Loc_SPEEDNSTYLE", "SPEEDNSTYLE/SPEEDNSTYLE/RULE");
                case "Outrun": return (A, "Audio_Loc_OUTRUNNER", "OUTRUN/OUTRUN/RULE");
                case "Rival": return (A, "Audio_Loc_RIVAL", "RIVAL/RIVAL/RULE");
                case "MotoPractice": return (A, "Audio_Loc_NATIONAL", "NATIONAL/NATIONAL/RULE_QUALIFIER");
                case "HeatPractice": return (A, "Audio_Loc_SUPERCROSS", "SUPERCROSS/SUPERCROSS/RULE_QUALIFIER");
                case "Heat": return (A, "Audio_Loc_SUPERCROSS", "SUPERCROSS/SUPERCROSS/RULE_HEAT");
                case "LCQ": return (A, "Audio_Loc_SUPERCROSS", "SUPERCROSS/SUPERCROSS/RULE_LCQ");
                case "HeatMainEvent": return (A, "Audio_Loc_SUPERCROSS", "SUPERCROSS/SUPERCROSS/RULE_MAIN");
                case "Moto": return (A, "Audio_Loc_NATIONAL", "NATIONAL/NATIONAL/RULE_MOTO");
                default: return null;
            }
        }
        public const uint RulesWrapper = 0x00fe7000, AaaWrapper = 0x00fe7001;

        /// Plays the localized event on that event player (wrapper: the owner's handle). False when the package is not installed.
        public static bool Play(OriginalAudio audio, string database, string package, string eventPath, uint wrapper)
        {
            if (!audio) return false;
            string path = FeSounds.ResolveLocalized(database, package, eventPath, out string project);
            FeSounds.Note("vo (race) " + database + "/" + package + " " + eventPath + " -> " + (path ?? "(no such package)"));
            if (path == null) return false;
            if (!audio.Projects.Contains(project)) audio.AddProjects(new[] { project });
            audio.Handle(new ReflexAudioCommand { sub = 0, wrapper = wrapper, serial = 1, path = path, position = new float[3], param = "" });
            return true;
        }
        /// App.StopUILocalizedSoundEvent.
        public static void Stop(OriginalAudio audio, uint wrapper)
        {
            if (audio) audio.Handle(new ReflexAudioCommand { sub = 1, wrapper = wrapper, serial = 1, path = "", position = new float[3], param = "" });
        }
    }

    // The UI music (MusicAudioMap.bxml <UI>: UITheme1..3 = UI_MUS/UI_MUS/PLAY_01..03 in the Music database's packages UITheme1..3, one
    // looping wave each: NAC_NAC_UI (bank00), PARADE_LAP_UI (bank01), PILOT_JET_UI (bank02); event volume 0.501 (-6 dB), fade-in
    // 2500 ms; UILevelParameter "UI_TIER_NUMBER"). The game's MusicPlayer (G + 0x15c6c; +8 its mode: 1 stopped, 2 jukebox, 3 UI,
    // 4 scenario):
    // * App.SetEnableUIMusic( TRUE ) (binding 007303f0 -> 00677840): unless the player is already in UI mode, everything stops
    //   (00677510) and a theme is drawn at random (the game's generator 009cb860: (x >> 16 & 0x7fff) % themes), its package loaded
    //   and its event started (006781a0: the music channel ramp set straight to 1, the event's own 2.5 s fade-in). FALSE (00677510):
    //   it stops (the InGame Loading Page). So every return to the front end from a race draws a theme again.
    // * App.SetUIMusicTier( 1..5 ) (00743160): in UI mode the tier (009ca5a4) ramps over UITierTransitionTime (2 s, 0067f1c0) and
    //   is set on the event's parameter "UI_TIER_NUMBER". No theme event has a parameter of that name (PLAY_01's only parameter is
    //   param00 and drives no envelope), so the tier changes nothing audible; it is kept here as the game keeps it.
    // * App.MuteUIMusic( TRUE / FALSE ) (00730500 -> 007431d0): the music channel ramps to 0 / back to 1 over 0.25 s.
    // * The garage's TEST DRIVE leaves the player in UI mode: GarageHUB:OnStartPressed and the Test Drive Loading / Exit Pages
    //   (TestDriveTransitionPage:Present: MXGarage_TestDriveStart / End only) never call SetEnableUIMusic, and the UI world's free ride starts no
    //   jukebox (FreeRideActivityAudioLogic::vf19 005822e0: +0x234 +0x20018), so the theme of the garage plays on through the ride and back.
    //   The port rides it in the race scene: Carry() keeps the theme and its place when the scene changes, and the next player enabled (the
    //   ride's, OwnTestRide; the garage's again) goes on with it, without a new draw or fade-in, where it would be had it not stopped.
    // The music channel: MusicVolume (ReflexAudio.music, the OPTIONS page) x MXRegistry VolumeChannels Music MasterVolume. The race's
    // own music switch (ReflexAudio.musicOn, OriginalOptionsMenu / M) silences it too.
    // YOUR MUSIC ONLY (OPTIONS > AUDIO, Content/ReflexCustomMusic.cs): the songs of Mods/Music play one after another in the themes' place (no theme is drawn
    // while there are songs; the same 2.5 s fade-in, music volume, music switch and mute); MIX IN leaves the themes alone, as the race's jukebox is where the
    // songs are mixed in. N skips to the next song.
    // Playback: the theme's MPEG stream (FSB sample of its bank, cut into the run-time cache) as a compressed-in-memory clip; the loop
    // is two sources scheduled back to back on the DSP clock, each starting 529 samples in (the decoder delay) and lasting the FSB
    // sample's length, as FMOD loops the sample.
    public sealed class FeUiMusic
    {
        sealed class Theme { public string package, soundAsset, bankAsset, eventPath, levelParameter; public float volume = 1f; public float fadeInMs; public Rfx.Asset bank; public int sampleIndex; public int length, rate; }
        readonly MonoBehaviour host;
        readonly AudioSource[] sources = new AudioSource[2];
        Task<List<Theme>> loadingThemes; List<Theme> themes;
        // YOUR MUSIC ONLY
        Reflex.Port.Content.ReflexSong customSong; AudioClip customClip; bool customLoading; int customGen; float customFade = 1f;
        // the songs play on a source of their own: the themes' passes are scheduled on sources[0 / 1] (PlayScheduled, SetScheduledEndTime)
        AudioSource songSource; Reflex.Port.Content.ReflexCustomMusic.SongDeck songDeck;
        Func<bool> skipFn; Action changedFn;
        /// The song of Mods/Music playing in the menus now (null: a theme or nothing).
        public Reflex.Port.Content.ReflexSong CustomSong => customClip ? customSong : null;
        AudioClip clip; Theme playingTheme; double nextStart; int nextSource; bool requested, loadingClip;
        // the theme carried over a scene change (Carry): its index, the place in its loop (s) and when it was left (realtime)
        static int s_carryTheme = -1; static double s_carryAt; static float s_carryWhen;
        double resumeAt = -1;   // the first pass starts here (s into the loop) instead of the loop's start
        float fade = 1f, fadeRate, channel = 1f, channelTarget = 1f, channelRate;
        int mutes;
        public bool Enabled { get; private set; }
        public int ThemeIndex { get; private set; } = -1;
        public string ThemeName => playingTheme?.package;
        /// App.SetUIMusicTier: the tier and its ramp (009ca5a4 target, 009ca5a0 now).
        public float Tier { get; private set; } = 1f;
        float tierTarget = 1f, tierFrom = 1f, tierTime = 2f;
        public float Volume => sources[0] ? Mathf.Max(Mathf.Max(sources[0].volume, sources[1].volume), songSource && songSource.isPlaying ? songSource.volume : 0f) : 0f;
        public bool IsPlaying => sources[0] && (sources[0].isPlaying || sources[1].isPlaying || (songSource && songSource.isPlaying));
        public string Error { get; private set; }
        /// The themes as read from the install (tests): "UITheme1 UI_MUS/UI_MUS/PLAY_01 bank UI_MUS_bank00 #0 6252048 @48000 vol 0.501 fade 2500".
        public static List<string> Describe() => LoadThemes(null).Select(t => $"{t.package} {t.eventPath} bank {t.bankAsset} #{t.sampleIndex} {t.length} @{t.rate} vol {t.volume:F3} fade {t.fadeInMs}").ToList();

        /// The scene changes while the UI music plays on (the TEST DRIVE, as the original's never stops it): the theme and its place are kept
        /// for the next player enabled within a minute. SetEnable(false) forgets them.
        public void Carry()
        {
            if (!Enabled || playingTheme == null || clip == null || ThemeIndex < 0) return;
            double len = (double)playingTheme.length / Math.Max(1, playingTheme.rate);
            s_carryTheme = ThemeIndex; s_carryWhen = Time.realtimeSinceStartup;
            s_carryAt = len > 0 ? ((AudioSettings.dspTime - nextStart) % len + len) % len : 0;   // (the passes are back to back up to nextStart)
            FeSounds.Note("music carry " + playingTheme.package + " at " + s_carryAt.ToString("0.0") + " s");
        }
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.SubsystemRegistration)]
        static void ResetCarry() => s_carryTheme = -1;   // (domain reload is off)

        public FeUiMusic(Component h)
        {
            host = h as MonoBehaviour;
            for (int i = 0; i < 2; i++)
            {
                var s = sources[i] = h.gameObject.AddComponent<AudioSource>();
                s.playOnAwake = false; s.loop = false; s.spatialBlend = 0f; s.ignoreListenerPause = true; s.priority = 0; Reflex.Port.OriginalWorld.OriginalReverb.Route(s, false);
            }
            songSource = h.gameObject.AddComponent<AudioSource>();
            songSource.playOnAwake = false; songSource.loop = false; songSource.spatialBlend = 0f; songSource.ignoreListenerPause = true; songSource.priority = 0;
            Reflex.Port.OriginalWorld.OriginalReverb.Route(songSource, false);
            songDeck = new Reflex.Port.Content.ReflexCustomMusic.SongDeck(songSource);
            _ = ReflexInstall.Packages;
            string cache = Path.Combine(ReflexInstall.CacheDir, "audio");
            loadingThemes = Task.Run(() => LoadThemes(cache));
        }

        static List<Theme> LoadThemes(string cacheDir)
        {
            var inst = ReflexInstall.Packages;
            var root = Rfx.Bxml(inst.Read("Music", "MusicConfig", "MusicAudioMap", "bxml"));
            var o = new List<Theme>();
            var set = root.Child("Settings"); var tt = set?.Child("UITierTransitionTime");
            var ui = root.Child("UI"); if (ui == null) return o;
            var db = inst.Db("Music");
            foreach (var t in ui.children)
            {
                if (t.tag != "Theme") continue;
                var th = new Theme { package = t.S("Package"), soundAsset = t.S("SoundAsset"), bankAsset = t.S("SoundBankAsset"), eventPath = t.S("EventPath"), levelParameter = t.S("UILevelParameter") };
                var pk = db?.Find(th.package);
                var fevAsset = pk?.Find(th.soundAsset, "sound");
                th.bank = pk?.Find(th.bankAsset, "soundbnk");
                if (fevAsset == null || th.bank == null) continue;
                var fevBytes = inst.Read(fevAsset);
                var fev = ReflexFmod.ParseFev(fevBytes);
                if (fev == null)
                {   // these wrappers put the FEV1 image further in than the first 0x100 bytes ParseFev looks at
                    for (int at = 0; at + 4 <= fevBytes.Length && fev == null; at++)
                        if (fevBytes[at] == 'F' && fevBytes[at + 1] == 'E' && fevBytes[at + 2] == 'V' && fevBytes[at + 3] == '1')
                        { var slice = new byte[fevBytes.Length - at]; Buffer.BlockCopy(fevBytes, at, slice, 0, slice.Length); fev = ReflexFmod.ParseFev(slice); }
                }
                if (fev == null) throw new InvalidOperationException("MusicAudioMap theme " + th.package + ": " + th.soundAsset + " is not a FEV1 v0x3a project (" + fevBytes.Length + " bytes: " + BitConverter.ToString(fevBytes, 0, Math.Min(48, fevBytes.Length)) + ")");
                string want = (th.eventPath ?? "").Substring((th.eventPath ?? "").IndexOf('/') + 1);   // "UI_MUS/PLAY_01"
                ReflexFmod.Event e = null;
                foreach (var (gpath, ev) in fev.Events()) if (ev != null && string.Equals(gpath + "/" + ev.name, want, StringComparison.OrdinalIgnoreCase)) { e = ev; break; }
                if (e == null) throw new InvalidOperationException("MusicAudioMap theme " + th.package + ": no event " + th.eventPath + " in " + fev.name);
                th.volume = e.volume; th.fadeInMs = e.fadeInMs;
                var si = (e.layers ?? new ReflexFmod.Layer[0]).Where(l => l.instances != null).SelectMany(l => l.instances).FirstOrDefault();
                var en = si != null && si.soundDef < fev.soundDefs.Length ? fev.soundDefs[si.soundDef].entries?.FirstOrDefault(x => x.kind == "wave") : null;
                if (en == null) throw new InvalidOperationException("MusicAudioMap theme " + th.package + ": " + th.eventPath + " has no wave");
                var fsb = ReflexFmod.ParseFsb(inst.Read(th.bank));
                string stem = en.wave != null ? ReflexFmod.Stem(en.wave) : null;   // (music banks carry no sample names: the entry's index)
                var smp = fsb.samples.FirstOrDefault(x => x.name != null && stem != null && string.Equals(x.Stem, stem, StringComparison.OrdinalIgnoreCase)) ?? (en.index < fsb.samples.Length ? fsb.samples[en.index] : fsb.samples.FirstOrDefault());
                if (smp == null) throw new InvalidOperationException("MusicAudioMap theme " + th.package + ": no sample in " + th.bankAsset);
                th.sampleIndex = smp.index; th.length = smp.lengthSamples; th.rate = smp.frequency;
                o.Add(th);
            }
            return o;
        }

        bool ThemesReady()
        {
            if (themes != null) return true;
            if (loadingThemes == null || !loadingThemes.IsCompleted) return false;
            if (loadingThemes.IsFaulted) { Error = loadingThemes.Exception.GetBaseException().Message; Debug.LogWarning("UI music not read from the install: " + loadingThemes.Exception.GetBaseException()); themes = new List<Theme>(); }
            else themes = loadingThemes.Result;
            if (themes.Count == 0 && Error == null) Error = "no UI theme in MusicAudioMap";
            loadingThemes = null;
            return true;
        }

        /// App.SetEnableUIMusic.
        public void SetEnable(bool on)
        {
            if (on == Enabled) return;
            Enabled = on;
            FeSounds.Note("music " + (on ? "enable" : "disable"));
            StopAll();
            if (!on) s_carryTheme = -1;
            skipFn = skipFn ?? SkipCustom; changedFn = changedFn ?? OnCustomChanged;
            if (on) { requested = true; ThemeIndex = -1; Reflex.Port.Content.ReflexCustomMusic.ResetFailures(); Reflex.Port.Content.ReflexCustomMusic.AddSkipper(skipFn); Reflex.Port.Content.ReflexCustomMusic.Changed += changedFn; }
            else { Reflex.Port.Content.ReflexCustomMusic.RemoveSkipper(skipFn); Reflex.Port.Content.ReflexCustomMusic.Changed -= changedFn; }
        }

        /// The host goes away (OpenShell / the test ride destroyed with their scene): the static SKIP SONG and YOUR MUSIC hooks let go of this
        /// player. They held it, and through its host the whole destroyed shell (its UI, its sound bank's clips), one more after every
        /// visit of the menus (w-gcspikes: the managed heap grew race after race). The carried theme (Carry) is kept.
        public void Detach()
        {
            if (skipFn != null) Reflex.Port.Content.ReflexCustomMusic.RemoveSkipper(skipFn);
            if (changedFn != null) Reflex.Port.Content.ReflexCustomMusic.Changed -= changedFn;
            Enabled = false;
        }

        // ---- YOUR MUSIC ONLY
        /// YOUR MUSIC was changed in the options: the menu music is picked again.
        void OnCustomChanged()
        {
            if (!host) { Reflex.Port.Content.ReflexCustomMusic.Changed -= changedFn; return; }
            if (!Enabled) return;
            StopAll(); requested = true; ThemeIndex = -1;
        }

        /// SKIP SONG while the menus play the songs.
        bool SkipCustom()
        {
            if (!Enabled || !host || (customClip == null && !customLoading)) return false;
            var next = Reflex.Port.Content.ReflexCustomMusic.Next();
            if (next == null) return false;
            StopCustom(); StartCustom(next, true);
            return true;
        }

        void StartCustom(Reflex.Port.Content.ReflexSong song, bool skipped)
        {
            customSong = song; customLoading = true; int gen = ++customGen;
            host.StartCoroutine(Reflex.Port.Content.ReflexCustomMusic.Load(song, c =>
            {
                if (gen != customGen) { if (c) UnityEngine.Object.Destroy(c); return; }   // stopped or skipped meanwhile
                customLoading = false;
                if (!c) { Reflex.Port.Content.ReflexCustomMusic.Failed(song); customSong = null; return; }
                customClip = c; customFade = 0f; channel = channelTarget;
                foreach (var t in sources) if (t) t.Stop();   // (no theme pass under the song)
                songSource.volume = 0f; songDeck.Play(c, song.Label); Reflex.Port.Content.ReflexCustomMusic.Played(song);
                FeSounds.Note("music custom " + song.Label + " (" + c.length.ToString("0.0") + " s)");
                Debug.Log("[Music] your music in the menus: " + song.Label + " (" + c.length.ToString("F0") + " s)");
                Reflex.Port.Hud.ReflexNowPlaying.Notify(song, skipped, () => customFade * channel);
            }));
        }

        void StopCustom()
        {
            customGen++; customLoading = false;
            songDeck.Stop();
            if (customClip) { songSource.clip = null; UnityEngine.Object.Destroy(customClip); }
            customClip = null; customSong = null;
        }

        /// The songs play instead of the themes (OPTIONS > AUDIO > YOUR MUSIC: ONLY, with songs): true while this handles the music.
        bool TickCustom(float dt)
        {
            bool want = Enabled && Reflex.Port.Content.ReflexCustomMusic.Only && !Reflex.Port.Content.ReflexCustomMusic.Failing && host && Application.isPlaying;
            if (!want)
            {
                if (customSong != null || customClip != null || customLoading) { StopCustom(); requested = Enabled; ThemeIndex = -1; }   // back to the themes
                return false;
            }
            if (customSong == null && !customLoading)
            {
                var next = Reflex.Port.Content.ReflexCustomMusic.Next();
                if (next == null) return false;
                requested = false;   // (no theme is drawn while the songs play)
                StartCustom(next, false);
            }
            channel = Mathf.MoveTowards(channel, channelTarget, channelRate * dt);
            if (customClip != null)
            {
                customFade = Mathf.Min(1f, customFade + dt / 2.5f);
                float v = customFade * channel * MusicLevel();
                songSource.volume = v;
                if (songDeck.Over()) { StopCustom(); }   // heard to its end: the next song is picked at once
            }
            return true;
        }

        /// The music channel's level: the MUSIC switch x MusicVolume x the install's Music MasterVolume.
        static float MusicLevel()
        {
            if (PlayerPrefs.GetInt("ReflexAudio.musicOn", 1) == 0) return 0f;
            float music = PlayerPrefs.GetFloat("ReflexAudio.music", 0.6f), master = 1f;
            try { master = ReflexVolumeChannels.Shared[ReflexChannel.Music].master; } catch (Exception) { }
            return music * master;
        }

        /// App.SetUIMusicTier (1..5; only while the UI music plays, 00743160).
        public void SetTier(int tier)
        {
            if (!Enabled || tier < 1 || tier > 5) return;
            tierFrom = Tier; tierTarget = tier; tierT = 0;
            FeSounds.Note("music tier " + tier);
        }
        float tierT = 1f;

        /// App.MuteUIMusic (a count: the Lua and a muting video may overlap).
        public void Mute(bool mute)
        {
            mutes = Math.Max(0, mutes + (mute ? 1 : -1));
            channelTarget = mutes > 0 ? 0f : 1f; channelRate = 1f / 0.25f;
            FeSounds.Note("music mute " + mute);
        }

        void StopAll()
        {
            StopCustom();
            foreach (var s in sources) if (s) { s.Stop(); s.clip = null; }
            if (clip) UnityEngine.Object.Destroy(clip); clip = null; playingTheme = null; requested = false;
        }

        public void Tick(float dt)
        {
            if (TickCustom(dt)) return;
            if (!ThemesReady()) return;
            if (tierT < 1f) { tierT = Mathf.Min(1f, tierT + dt / Mathf.Max(0.01f, tierTime)); Tier = Mathf.Lerp(tierFrom, tierTarget, tierT); }
            channel = Mathf.MoveTowards(channel, channelTarget, channelRate * dt);
            if (requested && !loadingClip && host && Application.isPlaying && themes.Count > 0)
            {
                requested = false;
                bool carried = s_carryTheme >= 0 && s_carryTheme < themes.Count && Time.realtimeSinceStartup - s_carryWhen < 60f;
                ThemeIndex = carried ? s_carryTheme : Math.Max(0, Reflex.Port.Content.ReflexMusicBags.Get(Reflex.Port.Content.ReflexMusicBags.Menu).Next(themes.Count));   // (the game: (x >> 16 & 0x7fff) % count of its generator; w-fb-hud: the launch's menu bag, no theme again before the others, never the last one first); a carried theme goes on
                resumeAt = carried ? s_carryAt + (Time.realtimeSinceStartup - s_carryWhen) : -1;   // (where it would be now)
                s_carryTheme = -1;
                host.StartCoroutine(Load(themes[ThemeIndex]));
            }
            if (playingTheme == null || clip == null) return;
            if (fadeRate > 0) fade = Mathf.Min(1f, fade + fadeRate * dt);
            float v = playingTheme.volume * fade * channel * MusicLevel();
            foreach (var s in sources) s.volume = v;
            // the loop: the next pass is scheduled a second ahead on the DSP clock
            double now = AudioSettings.dspTime;
            if (nextStart - now < 1.0) Schedule();
        }

        void Schedule()
        {
            var s = sources[nextSource]; nextSource ^= 1;
            double len = (double)playingTheme.length / Math.Max(1, playingTheme.rate);
            // a carried theme's first pass: from its place in the loop to the loop's end
            double from = resumeAt > 0 && len > 0 ? resumeAt % len : 0; resumeAt = -1;
            s.clip = clip; s.timeSamples = Mathf.Min(OriginalMp3Delay + (int)(from * playingTheme.rate), Math.Max(0, clip.samples - 1));
            s.PlayScheduled(nextStart); s.SetScheduledEndTime(nextStart + len - from);
            nextStart += len - from;
        }
        const int OriginalMp3Delay = 529;   // Unity's MPEG decoder delay (OriginalAudio.Mp3DecoderDelay)

        System.Collections.IEnumerator Load(Theme t)
        {
            loadingClip = true;
            string cache = Path.Combine(ReflexInstall.CacheDir, "audio");
            var file = Task.Run(() =>
            {
                Directory.CreateDirectory(cache);
                string path = Path.Combine(cache, "uimus_" + t.package + "_" + t.bankAsset + "_" + t.sampleIndex + "_" + t.bank.size + ".mp3");
                if (File.Exists(path) && new FileInfo(path).Length > 0) return path;
                var fsb = ReflexFmod.ParseFsb(ReflexInstall.Packages.Read(t.bank));
                var mp3 = ReflexFmod.CleanMpeg(fsb.Raw(fsb.samples[t.sampleIndex]), out _, out _, out _, out _);
                string tmp = path + "." + Environment.CurrentManagedThreadId + ".tmp";
                File.WriteAllBytes(tmp, mp3);
                try { File.Move(tmp, path); } catch (IOException) { File.Delete(tmp); }
                return path;
            });
            while (!file.IsCompleted) yield return null;
            if (file.IsFaulted) { Error = file.Exception.GetBaseException().Message; Debug.LogWarning("UI music " + t.package + ": " + Error); loadingClip = false; yield break; }
            using (var req = UnityWebRequestMultimedia.GetAudioClip(new Uri(file.Result).AbsoluteUri, AudioType.MPEG))
            {
                var dh = (DownloadHandlerAudioClip)req.downloadHandler; dh.compressed = true; dh.streamAudio = false;
                yield return req.SendWebRequest();
                loadingClip = false;
                if (req.result != UnityWebRequest.Result.Success) { Error = req.error; Debug.LogWarning("UI music " + t.package + ": " + req.error); yield break; }
                var c = DownloadHandlerAudioClip.GetContent(req);
                if (!Enabled || requested) { UnityEngine.Object.Destroy(c); yield break; }   // disabled (or redrawn) meanwhile
                c.name = t.package + " " + t.eventPath;
                clip = c; playingTheme = t;
                fade = t.fadeInMs > 0 && resumeAt < 0 ? 0f : 1f; fadeRate = t.fadeInMs > 0 ? 1000f / t.fadeInMs : 0f;   // (a carried theme never stopped: no fade-in)
                channel = channelTarget;   // 006781a0: the channel ramp straight to its value
                nextStart = AudioSettings.dspTime + 0.05; nextSource = 0;
                Schedule();
                FeSounds.Note("music theme " + t.package + " " + t.eventPath + " (" + (t.length / (float)Math.Max(1, t.rate)).ToString("0.0") + " s loop)");
            }
        }
    }
}
