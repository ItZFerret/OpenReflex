using Reflex.Port.RxCore;
using UnityEngine;

namespace Reflex.Port.OriginalWorld
{
    // The wreck-recovery arrows of the layers (the "Arrows" component, MXUIReflexComponent: vf2 005f79c0, vf11 005f7b80, the "WreckRecoveryFailed"
    // handler 005f7b00), from the core's copy of the component (rx_session_reflex_hud, core/include/rx_reflex_hud.h): the layer's own nodes and
    // animation, nothing computed here that the core does not say.
    //   vf2     the four arrow nodes are found by name (ArrowTop / ArrowRight / ArrowBottom / ArrowLeft) and the component starts hidden
    //   vf11    each frame, for a bike / ATV: the wreck kind of the local player picks the arrow (up, down, left, right: 2-3 / 5-6 / 8-9 / 11-12); the
    //           component and that arrow become visible and the layer's "ReflexIn" plays (the arrows slide in over 0.2 s, alpha 0 -> 1) - started
    //           again whenever it is not playing, so the arrow pulses; kind 0 hides everything. The frame the component turns visible the rider's
    //           sounds 0 (the WRECK_AVOIDANCE_WARN warning) and 5 start (the core's sound ring), the frame it hides slot 5 stops
    //   handler "WreckRecoveryFailed" (any rider's fall): ArrowTop / ArrowRight / ArrowBottom get the colour (1, 0, 0, 0), invisible until ReflexIn
    //           plays again (ArrowLeft is not in the handler's loop of three)
    public sealed partial class OriginalHud
    {
        OriginalUiTree.Node reflexArrows; readonly OriginalUiTree.Node[] reflexArrow = new OriginalUiTree.Node[4];   // Top, Right, Bottom, Left
        static readonly string[] ReflexArrowNames = { "ArrowTop", "ArrowRight", "ArrowBottom", "ArrowLeft" };
        bool reflexFailSynced; uint reflexFailSeen;

        void BindReflexArrows()
        {
            reflexArrows = tree.Find(R + "Arrows");
            if (reflexArrows == null) return;
            for (int i = 0; i < 4; i++)
            {
                reflexArrow[i] = tree.Child(reflexArrows, ReflexArrowNames[i]);
                if (reflexArrow[i] != null) reflexArrow[i].tintChildren = true;   // UIColorContainer: its animated colour tints its two images
                OriginalUiTree.SetVisible(reflexArrow[i], false);
            }
            OriginalUiTree.SetVisible(reflexArrows, false);
        }

        /// The component's frame (vf11) on the core's copy.
        void TickReflexArrows(OriginalWorldRuntime runtime) => ApplyReflexArrows(runtime && runtime.HasReflexHud, runtime && runtime.HasReflexHud ? runtime.ReflexHud : default);

        void ApplyReflexArrows(bool have, RxReflexHudState r)
        {
            if (reflexArrows == null) return;
            bool shown = have && r.shown != 0;
            for (int i = 0; i < 4; i++) OriginalUiTree.SetVisible(reflexArrow[i], shown && (r.litMask & (1u << i)) != 0);
            OriginalUiTree.SetVisible(reflexArrows, shown);
            if (shown && !tree.IsPlaying("ReflexIn", reflexArrows)) tree.Play("ReflexIn", reflexArrows);   // 005c9af0: started when not already playing
            // "WreckRecoveryFailed": the first three arrows' colour (1, 0, 0, 0)
            if (have && Rose(ref reflexFailSynced, ref reflexFailSeen, r.failAnyEvents))
                for (int i = 0; i < 3; i++) OriginalUiTree.SetTint(reflexArrow[i], new Color(1f, 0f, 0f, 0f));
        }
    }
}
