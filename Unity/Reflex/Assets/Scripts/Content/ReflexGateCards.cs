// The Gate Selection page's cards are entities of the level's scene (UIGateSelectionRSData: UI_StartCard_01 .. 16, the "X" cards
// UI_StartCard_X_01 .. 16 and UI_StartCard_Arrow: one shared model each, a material per number), hidden by the game until
// GateSelection_ShowGates places them (+0xa2 = 0 in the scene, 0073f8a0). ReflexLevelContent builds the static scenery types of
// ReflexScenery.StaticTypes only; this partial adds the card type so they are built with the rest of the level and OriginalGateCards
// (OriginalWorld) keeps them hidden until the Gate Selection page shows them. (Registered here, not in ReflexScenery.cs's set.)
using UnityEngine;

namespace Reflex.Port.Content
{
    public sealed partial class ReflexLevelContent
    {
        public const string GateCardType = "UIGateSelectionRSData";

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.SubsystemRegistration)]
        static void RegisterGateCards() { StaticTypes.Add(GateCardType); }
    }
}
