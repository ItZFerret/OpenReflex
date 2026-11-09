// OPTIONS > AUDIO > MUSIC: YOUR MUSIC (OFF / MIX IN / ONLY), the music folder and the NOW PLAYING pop-up, after Open Unleashed's MxaOptionsMenu rows of the same names
// (Content/ReflexCustomMusic.cs, Hud/ReflexNowPlaying.cs).
using Reflex.Port.Content;
using UnityEngine.UIElements;

namespace Reflex.Port.Options
{
    public sealed partial class ReflexOptionsMenu
    {
        static string SongCount()
        {
            if (!ReflexCustomMusic.Scanned) return "SCANNING";
            int n = ReflexCustomMusic.Count;
            return n + (n == 1 ? " SONG" : " SONGS");
        }

        static void MusicRows(VisualElement panel)
        {
            ReflexCustomMusic.RescanIfOld(2f);   // (songs dropped in since the last look)
            Named(OptUi.Spinner("YOUR MUSIC", () => ReflexCustomMusic.ModeNames[ReflexSettings.CustomMusic] + "  (" + SongCount() + ")", d =>
            {
                ReflexSettings.CustomMusic = (ReflexSettings.CustomMusic + d + 3) % 3;
                ReflexSettings.Save();
                ReflexCustomMusic.SettingsChanged();
            }, panel), "custom-music");
            Note(panel, "Your own songs (.mp3, .ogg, .wav) from the Mods/Music folder beside the game, subfolders too. MIX IN plays them among the game's tracks in the races (the menus keep the game's themes); " +
                        "ONLY plays them instead of the game's soundtrack, in the menus and the races. The game's timing stays: the fade-ins, the pause duck, the end of the race. Ours.");
            Named(OptUi.Button("OPEN MUSIC FOLDER", "mxa-link", ReflexCustomMusic.OpenFolder, panel), "music-folder");
            Note(panel, "Opens Mods/Music (made when missing) in the file browser. The songs' title and artist come from their tags, or the file name: \"Artist - Title.mp3\". Ours.");
            Named(OptUi.Toggle("NOW PLAYING POP-UP", () => ReflexSettings.NowPlayingPopup, v => { ReflexSettings.NowPlayingPopup = v; ReflexSettings.Save(); }, panel), "now-playing");
            Note(panel, "The song's title and artist slide in when it starts, the game's tracks too (in a race once the fade-in makes it audible). N plays the next song, in the menus when they play your songs. Ours.");
        }
    }
}
