using UnityEngine;

namespace Reflex.Port.OriginalWorld
{
    /// The audio-thread end of an engine pack (w-packpeak): on the pack's output AudioSource, multiplies the carrier (a spatialised constant 1)
    /// by the pack's mixed and limited mono signal (EnginePackMixer.Mix). Added at run time by EnginePackPlayer.
    public sealed class EnginePackFilter : MonoBehaviour
    {
        public EnginePackMixer mixer;
        void OnAudioFilterRead(float[] data, int channels)
        {
            var m = mixer;
            if (m == null) { System.Array.Clear(data, 0, data.Length); return; }
            m.Mix(data, channels);
        }
    }
}
