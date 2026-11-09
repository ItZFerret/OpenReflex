// The MotoSkills tutorials' furniture in the compound (UI_Compound, the front end's own world): the orange chevron walls and the blue trajectory
// arrows (TUT0<n>_*: LWModelRSData / ScrollingTextureRSData entities), the road flares (GL_RoadFlare; the 24 TUT placeable effects), the position
// marker. It is lesson scenery, not the compound's look: the original's UI world hides every TUT0<n> object when it is built
// (TutorialManager_Construct -> ShowLessonObjects( manager, -1 ) 00751c40: +0xa2 = 0 on the model / scrolling-texture entities, the effects'
// controllers off; core/game/fx_tutorial.c, run by the core at rx_mxuiworld_start), and shows one lesson's set only while that lesson runs
// (OriginalTutorial.Objects). The renderer builds every instance the level file holds, so a world that is not the tutorial (the front end's
// 3D menu, the garage's TEST RIDE) takes the furniture away here:
//   * VISUALS: the instances' GameObjects go inactive (no renderer, no scrolling);
//   * COLLISION: none to remove. These entities are type 4 / 0x1078 (no physics, no CollisionEntity: the core's static collision records come
//     from the level's CollisionRSData entities and the height field only; the compound's 19 CollisionRSData are the light poles and the
//     status boards), so the rider can never touch a wall that is not drawn (core/tools/level_entities_dump.c lists the scene's entities
//     by class; QUEUE "w-ridefx"). The 15 TutorialTriggerRSData volumes are MXTrigger entities: no geometry, no drawing, and the core's
//     TutorialManager is a product boundary (+0x22180 = 0), so entering one does nothing.
// Everything else of the compound stays: the scenery, the light poles and their beams, the boards, the terrain.
using System;
using Reflex.Port.Content;
using UnityEngine;

namespace Reflex.Port.OpenFrontEnd
{
    public static class OpenTutorialProps
    {
        /// True for a level instance that is the tutorials' furniture.
        public static bool IsTutorialProp(SceneryInstance inst) =>
            inst != null && inst.name != null &&
            (inst.name.StartsWith("TUT0", StringComparison.Ordinal) || inst.name.StartsWith("PositionMarker", StringComparison.Ordinal) || inst.model == "GL_RoadFlare");

        /// Turns the tutorial furniture of a built level off; returns how many instances (idempotent: those already off are not counted).
        public static int Hide(ReflexLevelContent content)
        {
            if (content == null) return 0;
            int hidden = 0;
            foreach (var inst in content.instances)
            {
                if (!inst.transform || !IsTutorialProp(inst)) continue;
                var go = inst.transform.gameObject;
                if (!go.activeSelf) continue;
                go.SetActive(false); hidden++;
            }
            return hidden;
        }
    }
}
