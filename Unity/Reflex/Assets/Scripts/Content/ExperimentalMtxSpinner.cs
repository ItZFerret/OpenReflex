// ExperimentalMtxSpinner: MTX's turning level objects (their scripts spin them about their pivots): wind turbine blades, the news
// helicopter's rotors, the compound's roof fans, the water wheel. Each turns about its own axis from where its node set it.
using UnityEngine;

namespace Reflex.Port.Content
{
    internal sealed class ExperimentalMtxSpinner : MonoBehaviour
    {
        Vector3 _axis; float _speed; Quaternion _rest;

        /// Adds a spinner when the object's name is one MTX turns (degrees per second about its local axis).
        internal static void Attach(GameObject go, string name)
        {
            string n = name.ToLowerInvariant(); Vector3 axis; float speed;
            if (n.StartsWith("blades_")) { axis = Vector3.right; speed = 60; }
            else if (n.EndsWith("_topblade")) { axis = Vector3.up; speed = 1200; }
            else if (n.EndsWith("_tailblade")) { axis = Vector3.right; speed = 1800; }
            else if (n.Contains("fan") && n.EndsWith("spin")) { axis = Vector3.up; speed = 240; }
            else if (n.StartsWith("waterwheel")) { axis = Vector3.right; speed = 20; }
            else return;
            var s = go.AddComponent<ExperimentalMtxSpinner>(); s._axis = axis; s._speed = speed; s._rest = go.transform.localRotation;
        }

        void Update() => transform.localRotation = _rest * Quaternion.AngleAxis(Time.time * _speed % 360f, _axis);
    }
}
