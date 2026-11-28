#include <cassert>
#include "../src/core/CameraViewToggle.h"

// The pedestrian camera toggle only applies to an on-foot player whose camera
// is really following him: vehicles cycle their own views and must be left
// alone, and menus, cutscenes, replays and script cameras all own the input.
static void TestToggleOnlyOnFoot()
{
    assert(CanTogglePedestrianCameraView(true, false, true, false, false, false, false));
    // In a vehicle the camera has its own cycling, so the key must do nothing.
    assert(!CanTogglePedestrianCameraView(true, true, true, false, false, false, false));
    // No player ped at all.
    assert(!CanTogglePedestrianCameraView(false, false, true, false, false, false, false));
    // The camera is not looking at the player, for example during an obbe cam.
    assert(!CanTogglePedestrianCameraView(true, false, false, false, false, false, false));
    // Any input owner that is not the player blocks the toggle.
    assert(!CanTogglePedestrianCameraView(true, false, true, true, false, false, false));
    assert(!CanTogglePedestrianCameraView(true, false, true, false, true, false, false));
    assert(!CanTogglePedestrianCameraView(true, false, true, false, false, true, false));
    assert(!CanTogglePedestrianCameraView(true, false, true, false, false, false, true));
}

// Whether the stored preference may be applied this frame. Walking is
// deliberately not part of the rules, otherwise first person walking would be
// impossible, which is exactly what the vanilla peek suffered from.
static void TestApplicationSurvivesWalking()
{
    assert(CanApplyPedestrianFirstPersonView(true, false, true, false, true));
    // A script may keep the first person cam alive without looking at the player.
    assert(CanApplyPedestrianFirstPersonView(false, true, true, false, true));
    assert(!CanApplyPedestrianFirstPersonView(false, false, true, false, true));
    // Leaving the pedestrian target blocks the view.
    assert(!CanApplyPedestrianFirstPersonView(true, false, false, false, true));
    assert(!CanApplyPedestrianFirstPersonView(true, false, true, true, true));
    // A dead or otherwise out of control ped must not stay in first person.
    assert(!CanApplyPedestrianFirstPersonView(true, false, true, false, false));
    // The script escape hatch overrides the widescreen refusal.
    assert(CanApplyPedestrianFirstPersonView(true, true, true, true, true));
}

// The preference is what the player asked for, the effective view is that
// preference ANDed with the per-frame guards. A single blocked frame must not
// erase the choice, otherwise the toggle needs a key press per frame.
static void TestPreferenceOutlivesBlockedFrames()
{
    bool preference = true;
    // Blocked frame, for example the ped briefly airborne in noclip.
    bool effective = preference && CanApplyPedestrianFirstPersonView(true, false, true, false, false);
    assert(!effective);
    // Once the ped is controllable again the preference applies on its own,
    // with no second key press.
    assert(preference);
    effective = preference && CanApplyPedestrianFirstPersonView(true, false, true, false, true);
    assert(effective);
}

int main()
{
    TestToggleOnlyOnFoot();
    TestApplicationSurvivesWalking();
    TestPreferenceOutlivesBlockedFrames();
    return 0;
}
