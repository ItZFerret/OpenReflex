using Reflex.Port.Options;
using UnityEngine;
using UnityEngine.InputSystem;

namespace Reflex.Port.OriginalWorld
{
    // The client's options in a race (F4): the full options pages (Reflex.Port.Options.ReflexOptions, Open Unleashed's options in
    // its look: CONTROLS, AUDIO, VIDEO, GAMEPLAY, CAMERA & HUD with the Dynamic FPV camera's settings, SYSTEM). This component stays
    // the race's handle on them (OriginalWorldRuntime.optionsMenu: IsOpen keeps the keys from the game, ClosedFrame keeps the Esc
    // that closed them from the pause page) and applies the saved volumes when the race starts. Opening pauses the race; F4 or Esc
    // close (the pages' own Back).
    [DisallowMultipleComponent]
    public sealed class OriginalOptionsMenu : MonoBehaviour
    {
        public OriginalWorldRuntime runtime;
        public bool IsOpen => ReflexOptions.IsOpen;
        public int ClosedFrame => ReflexOptions.ClosedFrame;
        int tab = ReflexOptions.CameraHud;

        void Start() { if (!runtime) runtime = GetComponent<OriginalWorldRuntime>(); ApplySavedAudio(); }

        void Update()
        {
            var kb = Keyboard.current;
            if (kb == null || !kb.f4Key.wasPressedThisFrame || runtime && runtime.OwnMenuActive) return;
            if (IsOpen) Close(); else if (ReflexOptions.ClosedFrame != Time.frameCount) Open();
        }

        public void Open() { if (!IsOpen) ReflexOptions.Open(tab, () => tab = ReflexOptions.Tab); }
        public void Close() { if (IsOpen) { tab = ReflexOptions.Tab; ReflexOptions.Close(); } }

        void ApplySavedAudio()
        {
            if (runtime && runtime.gameAudio && ReflexSave.Has("ReflexAudio.master")) runtime.gameAudio.masterVolume = ReflexSettings.EffectsVolume;
            var music = FindAnyObjectByType<OriginalMusic>();
            if (music) { if (ReflexSave.Has("ReflexAudio.music")) music.volume = ReflexSettings.MusicVolume; music.playing = ReflexSettings.MusicOn; }
        }
    }
}
