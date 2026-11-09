// The garage's TEST RIDE action (w-testride; OpenTestRide.cs has the ride itself): pad X / Square, keyboard T or the TEST RIDE button under the
// garage's panel. What is ridden: on the MY RIDES vehicle list the bike card in view when you own it (a bike mod's card with that mod, the
// original's card of a covered bike with the original), else the equipped setup (a pro rider's too); a vehicle you do not own is bought first.
// The way back: the shell starts again in the garage on the same level (every level above it keeps its place) with the focus on the same card.
using System;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using UnityEngine;
using UnityEngine.InputSystem;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed partial class OpenShell
    {
        GarageItem garageRideItem;   // the MY RIDES list's card in view (GarageVehicleShow)

        /// T / the pad's X / Square on the garage (no dialog open, not typing): the test ride. True when it started.
        bool GarageTestRideKey()
        {
            if (garageDialog != null || osk != null || OpenTestRide.Current != null) return false;   // (the on-screen keyboard has X: delete)
            var focused = _layer.focusController?.focusedElement;
            if (focused is UnityEngine.UIElements.TextElement || focused is UnityEngine.UIElements.TextField) return false;
            bool pressed = Keyboard.current?.tKey.wasPressedThisFrame == true;
            foreach (var pad in Gamepad.all) if (pad.buttonWest.wasPressedThisFrame) pressed = true;
            if (!pressed) return false;
            return GarageTestRide();
        }

        /// TEST RIDE: the vehicle in view (or the equipped one) and the MOTOCARD rider in the compound.
        public bool GarageTestRide()
        {
            if (OpenTestRide.Current != null || garageRows == null) return false;
            bool pro = OpenGarageData.Pro;
            var v = GTop?.kind == GKind.Models ? garageRideItem : null;
            if (v != null && !v.owned)
            {
                GarageStatus("BUY THIS VEHICLE TO TAKE IT FOR A TEST RIDE");
                OuSound.Play("Locked");
                return false;
            }
            int model, pv; string mod = null;
            if (v != null && !pro)
            {
                model = v.id; pv = v.playerVehicle;
                if (OpenGarageData.ModsOfModel(v.id).Count > 0) mod = v.modId ?? "";   // (the card's own model: a mod or the original)
            }
            else { model = OpenGarageData.EquippedModel; pv = pro ? -1 : OpenGarageData.Equipped; }
            if (model < 0) { GarageStatus("NO VEHICLE TO RIDE"); OuSound.Play("Locked"); return false; }
            var row = FeGarageData.Unique("VehicleModel", "VehicleModelId", model);
            int product = FeGarageData.ProductOfAsset(FeTables.I(row, "StoreAssetId"));
            int type = FeTables.I(row, "VehicleTypeId");
            string title = OpenGarageData.Title(product, "VEHICLE"), make = OpenGarageData.BrandName(product) ?? OpenGarageData.TypeName(type);
            var m = mod != null && mod.Length > 0 ? BikeMods.Find(mod) : mod == null ? BikeMods.ChosenFor(model) : null;
            var named = m == null ? null : BikeMods.NameFor(m, model, title);
            if (named != null && named.model.Length > 0) title = named.model;
            if (named != null && named.brand.Length > 0) make = named.brand;
            var ride = new OpenTestRide.Ride
            {
                title = title, make = make, model = model, playerVehicle = pv, vehicleType = type, modId = mod,
                tab = GarageCurrentTab, focus = _nav.Focused?.name, garageClass = GTop?.kind == GKind.Models ? GTop.a : -1,
            };
            if (GTop != null && _nav.Focused != null && garageRows != null && garageRows.Contains(_nav.Focused)) GTop.focus = _nav.Focused.name;
            ride.path = GaragePath;
            // the profile's equipped setup rides as it is; another vehicle of yours is this session's selection (the profile is not changed)
            ReflexWorldFiles.Garage selection = !pro && pv >= 0 && pv != OpenGarageData.Equipped
                ? new ReflexWorldFiles.Garage { vehicleOwner = ReflexWorldFiles.Garage.Player1, vehicleId = pv, vehicleTypeId = type } : null;
            OuSound.Play("Accept");
            _nav.Suspended = () => true;   // (nothing moves under the curtain)
            _sounds?.music?.Carry();   // the original never stops the UI music for a test drive (FeUiMusic): the ride's player goes on with this theme (OwnTestRide)
            OpenTestRide.Begin(ride, selection);
            return true;
        }

        /// The shell starts after a test ride: the garage again, where the ride left it (once the menu world is there, so the garage's stage is
        /// the 3D one, as before the ride). True when it took the start.
        bool TestRideReturn()
        {
            var r = OpenTestRide.PendingReturn;
            if (r == null) return false;
            OpenTestRide.PendingReturn = null;
            _heroWalked = true;   // (the hub does not walk the bike in again)
            Clear(Screen.Loading);
            _hints.Clear();
            float since = Time.unscaledTime;
            ScreenUpdate = () =>
            {
                if (MenuWorld == null && _world && _world.Status == OpenMenuWorld.State.Loading && Time.unscaledTime - since < 15f) return;
                ScreenUpdate = null;
                if (!string.IsNullOrEmpty(r.path)) ShowGaragePath(r.path); else ShowGarage(r.tab);
                OuLayout.Apply(_layer, OuLayout.Kind.Layer);   // (the layer's insets: the loading screen's zero padding stays until its geometry changes)
                string why = OpenTestRide.FailedReason; OpenTestRide.FailedReason = null;
                if (!string.IsNullOrEmpty(why)) GarageStatus("THE TEST RIDE STOPPED: " + why.ToUpperInvariant());
                OpenTestRide.Returned();
                Debug.Log($"[TestRide] back in the garage: tab {r.tab}, path {r.path}, focus {_nav.Focused?.name}, {OpenTestRide.LastReturnSeconds:F2} s after RETURN TO GARAGE");
            };
            return true;
        }
    }
}
